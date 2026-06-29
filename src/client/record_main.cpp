// SEADS trajectory recorder (seads_record) — Step 5 renderer feed.
//
// Drives the REAL sealed kernel and captures the resulting flight as the exact byte stream a
// 20 Hz server would transmit: GEO-001/KIN-001 wire snapshots (netsnap, protocol 2), written as
// a .seadsrec container (see seadsrec.h) for the C++ client AND mirrored to a trajectory.js for
// the web globe. Both come from the SAME decoded wire frames, so every viewer sees identical,
// quantization-faithful data — proving the layer 1/2/4a path end to end.
//
// DOWNSTREAM-ONLY: reads kernel state through the public getters, never writes the sim, never
// hashes, touches no rail/golden. Rides seal v1.4r0 (no seal).
//
// Usage:
//   seads_record --demo                       [--out flight.seadsrec] [--js trajectory.js] [--snap-every 5]
//   seads_record --id GOLDEN-SK-TurnClimb-001  [--out ...] [--js ...] [--snap-every 5]
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "kernel.h"
#include "golden_params.h"
#include "scenario_params.h"
#include "snapshot.h"
#include "seadsrec.h"

using namespace seads;

namespace {

constexpr double PI_      = 0x1.921fb54442d18p+1;  // matches det_math / netsnap
constexpr double DEG2RAD  = PI_ / 180.0;

// A non-sealed maneuver schedule. Degrees in, converted to kernel radians at add time. B2: the
// vertical input is now a commanded load factor g (target_g); a level turn at bank b uses
// g~1/cos(b), g above that climbs, below descends. throttle [0,1] sustains energy.
struct Phase { unsigned start_tick; double target_phi_deg; double target_g; double throttle; };
struct Ac    { int64_t id; double lat_deg, lon_deg, psi_deg, phi_deg, alt_m, tas_mps;
               const Envelope* env; std::vector<Phase> sched; };
struct Flight { std::string id; unsigned ticks; std::vector<Ac> ac; };

// Built-in 3-ship demo (NOT a golden — recorder is downstream of the seal). A KI-61, a Spitfire
// and a Bf 109 break in different directions over the tiny R=15 km globe so remote-interpolation
// is actually visible: banked turns at different rates, two of them pulling g to climb.
Flight make_demo() {
    Flight f;
    f.id = "DEMO-3SHIP";
    f.ticks = 6000;  // 60 s at 100 Hz
    f.ac.push_back(Ac{0,  0.0,   0.0,  45.0, 0.0, 2000.0, 170.0, &envtab::KI61, {
        {0,   0.0,  1.0,  0.85},
        {150, 55.0, 1.85, 0.85},   // hard left bank, pull to turn + climb
        {3000, -40.0, 1.0, 0.85}}});
    f.ac.push_back(Ac{1, 8.0,   6.0,  90.0, 0.0, 3500.0, 200.0, &envtab::SPITFIRE_MK5, {
        {0,   0.0,  1.05, 0.85},
        {300, -50.0, 1.45, 0.85},  // right bank, sustained-ish turn
        {2500, 50.0, 1.65, 0.85}}});
    f.ac.push_back(Ac{2, -6.0, -4.0, 200.0, 0.0, 1500.0, 150.0, &envtab::BF109F4, {
        {0,   0.0,  1.15, 0.9},     // gentle climb
        {800, 45.0, 1.55, 0.9},     // climbing turn
        {4000, 0.0, 1.0,  0.9}}});
    return f;
}

// Adapt a sealed scenario (radians already baked in) into the recorder's Flight shape.
Flight from_scenario(const scen::Scenario& S) {
    Flight f;
    f.id = S.id;
    f.ticks = S.ticks;
    for (unsigned a = 0; a < S.n_ac; ++a) {
        const scen::AcSpec& s = S.ac[a];
        Ac ac;
        ac.id = static_cast<int64_t>(a);
        // Mark as pre-converted: store radians directly (sentinel via NaN-free path below).
        ac.lat_deg = s.lat; ac.lon_deg = s.lon; ac.psi_deg = s.psi;
        ac.phi_deg = s.phi; ac.alt_m = s.alt; ac.tas_mps = s.tas;
        ac.env = s.env;
        for (unsigned j = 0; j < s.n_phase; ++j)
            ac.sched.push_back(Phase{s.sched[j].start_tick, s.sched[j].target_phi,
                                     s.sched[j].target_g, s.sched[j].throttle});  // phi already radians
        f.ac.push_back(ac);
    }
    return f;
}

// Build the kernel from a Flight. `deg` selects whether AcSpec angles need deg->rad (demo) or
// are already radians (sealed scenario replay). Returns the per-aircraft envelope list.
void seed(Kernel& k, const Flight& f, bool deg, std::vector<const Envelope*>& env) {
    for (const auto& a : f.ac) {
        if (deg)
            k.add(a.lat_deg * DEG2RAD, a.lon_deg * DEG2RAD, a.psi_deg * DEG2RAD,
                  a.phi_deg * DEG2RAD, a.alt_m, a.tas_mps);
        else
            k.add(a.lat_deg, a.lon_deg, a.psi_deg, a.phi_deg, a.alt_m, a.tas_mps);
        env.push_back(a.env);
    }
}

void command_at(const Flight& f, unsigned t, bool deg, std::vector<Command>& cmd) {
    for (std::size_t a = 0; a < f.ac.size(); ++a) {
        const auto& sched = f.ac[a].sched;
        std::size_t idx = 0;
        for (std::size_t j = 0; j < sched.size(); ++j) {
            if (sched[j].start_tick <= t) idx = j; else break;
        }
        double phi = sched[idx].target_phi_deg;
        cmd[a].target_phi = deg ? phi * DEG2RAD : phi;     // sealed scheds store radians
        cmd[a].target_g = sched[idx].target_g;
        cmd[a].throttle = sched[idx].throttle;
    }
}

// One captured frame: build wire EntityStates from kernel, encode (-> .seadsrec), decode back
// (-> exactly what a client receives) for the JSON mirror.
netsnap::Snapshot capture(const Kernel& k, const Flight& f, uint32_t tick,
                          std::vector<uint8_t>& wire) {
    netsnap::Snapshot snap;
    snap.protocol = netsnap::SNAPSHOT_PROTOCOL;
    snap.server_tick = static_cast<int64_t>(tick);
    for (std::size_t a = 0; a < f.ac.size(); ++a) {
        snap.entities.push_back(netsnap::from_kernel(
            f.ac[a].id, k.lat(a), k.lon(a), k.psi(a), k.alt(a), k.phi(a), k.tas(a), k.gamma(a)));
    }
    wire.clear();
    netsnap::encode_snapshot(snap, wire);
    netsnap::Snapshot decoded;
    std::size_t pos = 0;
    netsnap::decode_snapshot(wire.data(), wire.size(), pos, decoded);  // round-trip = client view
    return decoded;
}

void write_js(const std::string& path, const client::RecordingMeta& meta, const std::string& id,
              const std::vector<netsnap::Snapshot>& frames) {
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return; }
    std::fprintf(fp, "// AUTO-GENERATED by seads_record — decoded GEO-001/KIN-001 wire frames.\n");
    std::fprintf(fp, "window.SEADS_TRAJECTORY = {\n");
    std::fprintf(fp, "  \"meta\": {\"scenario\": \"%s\", \"radius_m\": %.1f, \"tick_hz\": %u, "
                     "\"snap_hz\": %u, \"protocol\": %u, \"n_frames\": %u},\n",
                 id.c_str(), meta.radius_m, meta.tick_hz, meta.snap_hz, meta.protocol,
                 static_cast<unsigned>(frames.size()));
    std::fprintf(fp, "  \"frames\": [\n");
    for (std::size_t fi = 0; fi < frames.size(); ++fi) {
        const auto& fr = frames[fi];
        std::fprintf(fp, "    {\"tick\": %lld, \"e\": [",
                     static_cast<long long>(fr.server_tick));
        for (std::size_t e = 0; e < fr.entities.size(); ++e) {
            const auto& en = fr.entities[e];
            std::fprintf(fp,
                "{\"id\":%lld,\"lat\":%.7f,\"lon\":%.7f,\"brg\":%.6f,\"alt\":%.3f,"
                "\"phi\":%.6f,\"tas\":%.3f,\"gamma\":%.6f}%s",
                static_cast<long long>(en.id), en.lat_deg, en.lon_deg, en.bearing_deg,
                en.alt_m, en.phi_deg, en.tas_mps, en.gamma_deg,
                (e + 1 < fr.entities.size()) ? "," : "");
        }
        std::fprintf(fp, "]}%s\n", (fi + 1 < frames.size()) ? "," : "");
    }
    std::fprintf(fp, "  ]\n};\n");
    std::fclose(fp);
}

}  // namespace

int main(int argc, char** argv) {
    const char* id = nullptr;
    const char* out_path = nullptr;
    const char* js_path = nullptr;
    bool demo = false;
    unsigned snap_every = 5;  // 20 Hz at 100 Hz physics
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--id") && i + 1 < argc) id = argv[++i];
        else if (!std::strcmp(argv[i], "--out") && i + 1 < argc) out_path = argv[++i];
        else if (!std::strcmp(argv[i], "--js") && i + 1 < argc) js_path = argv[++i];
        else if (!std::strcmp(argv[i], "--snap-every") && i + 1 < argc)
            snap_every = static_cast<unsigned>(std::atoi(argv[++i]));
    }
    if (!demo && !id) {
        std::fprintf(stderr,
            "usage: seads_record (--demo | --id <GOLDEN-SK-...>) [--out f.seadsrec] "
            "[--js trajectory.js] [--snap-every 5]\n");
        return 2;
    }
    if (snap_every == 0) snap_every = 1;

    Flight flight;
    bool deg = false;
    if (demo) { flight = make_demo(); deg = true; }
    else {
        const scen::Scenario* S = nullptr;
        for (unsigned k = 0; k < scen::N_ALL; ++k)
            if (!std::strcmp(scen::ALL[k]->id, id)) { S = scen::ALL[k]; break; }
        if (!S) { std::fprintf(stderr, "unknown scenario id: %s\n", id); return 2; }
        flight = from_scenario(*S);
    }

    Rails rails;
    rails.R = golden::R_M; rails.dt = golden::DT_S; rails.g0 = golden::G0;
    rails.atm_top = golden::ATM_TOP_M; rails.soft = golden::SOFT_M;

    Kernel k(rails);
    std::vector<const Envelope*> env;
    seed(k, flight, deg, env);

    std::vector<std::vector<uint8_t>> wire_frames;
    std::vector<netsnap::Snapshot> decoded_frames;
    std::vector<Command> cmd(flight.ac.size());

    auto grab = [&](uint32_t tick) {
        std::vector<uint8_t> wire;
        decoded_frames.push_back(capture(k, flight, tick, wire));
        wire_frames.push_back(std::move(wire));
    };

    grab(0);  // initial state (server_tick 0)
    for (unsigned t = 0; t < flight.ticks; ++t) {
        command_at(flight, t, deg, cmd);
        k.step(cmd, env);
        unsigned elapsed = t + 1;
        if (elapsed % snap_every == 0) grab(elapsed);
    }
    if (flight.ticks % snap_every != 0) grab(flight.ticks);  // ensure the tail is captured

    client::RecordingMeta meta;
    meta.protocol = netsnap::SNAPSHOT_PROTOCOL;
    meta.tick_hz  = 100;
    meta.snap_hz  = 100u / snap_every;
    meta.radius_m = rails.R;
    meta.n_frames = static_cast<uint32_t>(wire_frames.size());

    std::printf("scenario=%s ticks=%u aircraft=%zu frames=%zu snap_every=%u (%u Hz)\n",
                flight.id.c_str(), flight.ticks, flight.ac.size(), wire_frames.size(),
                snap_every, meta.snap_hz);

    if (out_path) {
        std::vector<uint8_t> blob;
        client::write_recording(meta, wire_frames, blob);
        std::FILE* fp = std::fopen(out_path, "wb");
        if (!fp) { std::fprintf(stderr, "cannot open %s\n", out_path); return 2; }
        std::fwrite(blob.data(), 1, blob.size(), fp);
        std::fclose(fp);
        std::printf("wrote %zu bytes -> %s\n", blob.size(), out_path);
    }
    if (js_path) {
        write_js(js_path, meta, flight.id, decoded_frames);
        std::printf("wrote trajectory.js -> %s\n", js_path);
    }
    return 0;
}
