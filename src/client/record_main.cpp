// SEADS trajectory recorder (seads_record) — Step 5 renderer feed.
//
// Drives the REAL sealed kernel and captures the resulting flight as the exact byte stream a
// 20 Hz server would transmit: GEO-001/KIN-002/WEAPON-001 wire snapshots (netsnap, protocol 4),
// written as a .seadsrec container (see seadsrec.h) for the C++ client AND mirrored to a
// trajectory.js for the web globe. Both come from the SAME decoded wire frames, so every viewer
// sees identical, quantization-faithful data — INCLUDING the gunnery state (hp + live rounds),
// which now rides the WEAPON-001 section (seal v1.12r0) instead of being read out-of-band from
// the local kernel. Proves the layer 1/2/4a path AND the weapon wire end to end.
//
// DOWNSTREAM-ONLY: reads kernel state through the public getters, never writes the sim, never
// hashes, touches no rail/golden. Rides seal v1.12r0 (no seal).
//
// Usage:
//   seads_record --demo                       [--out flight.seadsrec] [--js trajectory.js] [--snap-every 5]
//   seads_record --id GOLDEN-SK-TurnClimb-001  [--out ...] [--js ...] [--snap-every 5]
#include <cmath>
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
// G3 (v1.11r0): `fire` (0/1) drives the gun trigger so the gun demo can shoot. Missing initializers
// value-init to 0, so the non-firing demos below need no change.
struct Phase { unsigned start_tick; double target_phi_deg; double target_g; double throttle; double fire; };
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

// Built-in gun demo (NOT a golden) — a P-47D guns down an A6M2 in a co-altitude tail chase while a
// Spitfire does aerobatics nearby. Shows G1→G3 live: tracer rounds, hitpoints, and a kill. The
// chase geometry mirrors GOLDEN-SK-Hit-001 (reliable kill); the attacker fires from t=120.
Flight make_gundemo() {
    Flight f;
    f.id = "DEMO-GUNKILL";
    f.ticks = 900;  // 9 s at 100 Hz
    // AC0 P-47D attacker — closes from astern, fires a continuous stream; gentle shared left turn
    // keeps it on the target. After the kill its tracers stream on past the frozen corpse.
    f.ac.push_back(Ac{0, 0.0, 0.0, 90.0, 0.0, 4000.0, 230.0, &envtab::P47D, {
        {0,    8.0, 1.02, 0.95, 0.0},
        {120,  8.0, 1.02, 0.95, 1.0}}});  // FIRING from t=120 to the end
    // AC1 A6M2 target — same heading/altitude ~520 m ahead, mild turn; it takes the burst and dies.
    f.ac.push_back(Ac{1, 0.0, 2.0, 90.0, 0.0, 4000.0, 170.0, &envtab::A6M2, {
        {0,   8.0, 1.02, 0.7, 0.0}}});
    // AC2 Spitfire — uninvolved, loops and banks elsewhere for scenery.
    f.ac.push_back(Ac{2, 6.0, -5.0, 200.0, 0.0, 3000.0, 200.0, &envtab::SPITFIRE_MK5, {
        {0,    0.0, 1.2, 0.9, 0.0},
        {300, -55.0, 1.8, 0.9, 0.0},
        {700,  50.0, 1.7, 0.9, 0.0}}});
    return f;
}

// Built-in 4-ship dogfight (NOT a golden) — a longer, rolling engagement: two hunter/prey pairs in
// parallel lanes. AC0 P-47D kills AC1 A6M2, then AC2 P-51 (slightly staggered) kills AC3 Ki-61; both
// victors then BREAK into hard banking turns over the tiny globe. The firing runs use the proven
// co-altitude tail-chase geometry from GOLDEN-SK-Hit-001 (gentle shared bank, attacker faster + fast
// rounds overtake), so the kills land reliably before the hard maneuvering begins. ~26 s of action.
Flight make_dogfight() {
    Flight f;
    f.id = "DEMO-DOGFIGHT";
    f.ticks = 2600;  // 26 s at 100 Hz
    // --- Pair A (equatorial lane): P-47D hunts an A6M2 ~520 m ahead, co-altitude ---
    f.ac.push_back(Ac{0, 0.0, 0.0, 90.0, 0.0, 4000.0, 235.0, &envtab::P47D, {
        {0,     6.0, 1.01, 0.96, 0.0},
        {150,   6.0, 1.01, 0.96, 1.0},   // FIRE — downs the A6M2 (~t280)
        {430,   6.0, 1.01, 0.96, 0.0},   // cease fire after the kill
        {650,  55.0, 1.65, 0.97, 0.0},   // BREAK hard right + climb
        {1450,-48.0, 1.55, 0.97, 0.0},   // reverse the turn
        {2200, 20.0, 1.12, 0.95, 0.0}}});
    f.ac.push_back(Ac{1, 0.0, 2.0, 90.0, 0.0, 4000.0, 172.0, &envtab::A6M2, {
        {0,     6.0, 1.01, 0.72, 0.0}}}); // flies the shared gentle turn, takes the burst, dies
    // --- Pair B (northern lane, lat +3): P-51 hunts a Ki-61 ~650 m ahead, staggered later ---
    f.ac.push_back(Ac{2, 3.0, 0.0, 90.0, 0.0, 3600.0, 240.0, &envtab::P51, {
        {0,     6.0, 1.01, 0.96, 0.0},
        {300,   6.0, 1.01, 0.96, 1.0},   // FIRE — downs the Ki-61 (~t500), staggered after Pair A
        {600,   6.0, 1.01, 0.96, 0.0},
        {820,  60.0, 1.70, 0.97, 0.0},   // BREAK
        {1650,-42.0, 1.45, 0.97, 0.0},
        {2300,  0.0, 1.0,  0.95, 0.0}}});
    f.ac.push_back(Ac{3, 3.0, 2.5, 90.0, 0.0, 3600.0, 176.0, &envtab::KI61, {
        {0,     6.0, 1.01, 0.74, 0.0}}});
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
                                     s.sched[j].target_g, s.sched[j].throttle,
                                     s.sched[j].fire ? 1.0 : 0.0});  // phi already radians; fire 0/1
        f.ac.push_back(ac);
    }
    return f;
}

// Build the kernel from a Flight. `deg` selects whether AcSpec angles need deg->rad (demo) or
// are already radians (sealed scenario replay). Returns the per-aircraft envelope list.
void seed(Kernel& k, const Flight& f, bool deg, std::vector<const Envelope*>& env) {
    for (const auto& a : f.ac) {
        double hp = a.env->hp_start;          // G3 (v1.11r0): per-airframe starting hitpoints
        if (deg)
            k.add(a.lat_deg * DEG2RAD, a.lon_deg * DEG2RAD, a.psi_deg * DEG2RAD,
                  a.phi_deg * DEG2RAD, a.alt_m, a.tas_mps, 0.0, hp);
        else
            k.add(a.lat_deg, a.lon_deg, a.psi_deg, a.phi_deg, a.alt_m, a.tas_mps, 0.0, hp);
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
        cmd[a].fire = sched[idx].fire != 0.0;              // G3: gun trigger
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
            f.ac[a].id, k.lat(a), k.lon(a), k.psi(a), k.alt(a), k.phi(a), k.tas(a), k.gamma(a),
            k.hp(a), k.fire_cd(a),
            k.ammo(a),         // WEAPON-001: hp + fire-rate cooldown (v1.12r0) + magazine ammo (v1.14r0)
            k.last_hit_by(a),  // + attacker attribution (v1.17r0)
            k.engine_hp(a), k.wing_hp(a), k.tail_hp(a),  // + region pools + kill tally (v1.19r0)
            k.kills(a)));
    }
    for (std::size_t i = 0; i < k.proj_count(); ++i) {  // WEAPON-001: live ballistic rounds
        snap.projectiles.push_back(netsnap::proj_from_kernel(
            static_cast<int64_t>(i), k.proj_lat(i), k.proj_lon(i), k.proj_psi(i), k.proj_alt(i),
            k.proj_damage(i), static_cast<int64_t>(k.proj_ttl(i)),
            static_cast<int64_t>(k.proj_owner(i))));
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
    std::fprintf(fp, "// AUTO-GENERATED by seads_record — decoded GEO-001/KIN-002/WEAPON-001 wire frames.\n");
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
        // G1→G4 extras now sourced from the DECODED WEAPON-001 wire (hp + fire-rate v1.12r0; the
        // magazine `ammo` v1.14r0): per-aircraft hp + rounds-remaining + live rounds
        // [lat,lon,alt,owner] exactly as a client receives them (quantization-faithful), NOT read
        // out-of-band from the kernel.
        std::fprintf(fp, "],\"hp\":[");
        for (std::size_t e = 0; e < fr.entities.size(); ++e)
            std::fprintf(fp, "%.1f%s", fr.entities[e].hp, (e + 1 < fr.entities.size()) ? "," : "");
        std::fprintf(fp, "],\"ammo\":[");
        for (std::size_t e = 0; e < fr.entities.size(); ++e)
            std::fprintf(fp, "%.0f%s", fr.entities[e].ammo, (e + 1 < fr.entities.size()) ? "," : "");
        std::fprintf(fp, "],\"kills\":[");  // v1.19r0: the wire-sourced scoreboard
        for (std::size_t e = 0; e < fr.entities.size(); ++e)
            std::fprintf(fp, "%.0f%s", fr.entities[e].kills, (e + 1 < fr.entities.size()) ? "," : "");
        std::fprintf(fp, "],\"p\":[");
        for (std::size_t pi = 0; pi < fr.projectiles.size(); ++pi) {
            const auto& pr = fr.projectiles[pi];
            std::fprintf(fp, "[%.7f,%.7f,%.1f,%lld]%s", pr.lat_deg, pr.lon_deg, pr.alt_m,
                         static_cast<long long>(pr.owner),
                         (pi + 1 < fr.projectiles.size()) ? "," : "");
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
    bool demo = false, gundemo = false, dogfight = false;
    unsigned snap_every = 5;  // 20 Hz at 100 Hz physics
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--demo")) demo = true;
        else if (!std::strcmp(argv[i], "--gundemo")) { gundemo = true; demo = true; }
        else if (!std::strcmp(argv[i], "--dogfight")) { dogfight = true; demo = true; }
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
    if (dogfight) { flight = make_dogfight(); deg = true; }
    else if (gundemo) { flight = make_gundemo(); deg = true; }
    else if (demo) { flight = make_demo(); deg = true; }
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
    std::vector<client::RecEvent> events;  // layer-6 journal, captured at the full 100 Hz rate
    std::vector<Command> cmd(flight.ac.size());

    auto grab = [&](uint32_t tick) {
        std::vector<uint8_t> wire;
        decoded_frames.push_back(capture(k, flight, tick, wire));  // geo+kin+weapon, decoded
        wire_frames.push_back(std::move(wire));
    };

    grab(0);  // initial state (server_tick 0)
    for (unsigned t = 0; t < flight.ticks; ++t) {
        command_at(flight, t, deg, cmd);
        k.step(cmd, env);
        unsigned elapsed = t + 1;
        // Layer-6 combat events: one record per connecting round from the kernel's per-round hit
        // queue (this step's hits, projectile order). Quantized to milli-hp exactly as the reliable
        // event channel does (event.cpp): damage = post-clamp effective loss, hp = post-clamp hp.
        for (const HitEvent& h : k.hit_events())
            events.push_back(client::RecEvent{
                static_cast<int64_t>(elapsed), h.target, h.attacker,
                std::llround(h.hp_before * 1000.0) - std::llround(h.hp_after * 1000.0),
                std::llround(h.hp_after * 1000.0), h.killed, h.region});
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
    // Combat summary (final kernel state): hp per aircraft + any kills.
    for (std::size_t a = 0; a < flight.ac.size(); ++a)
        std::printf("  aircraft %zu: hp %.0f%s\n", a, k.hp(a), k.hp(a) <= 0.0 ? "  *** KILLED ***" : "");
    std::printf("  rounds airborne at end: %zu\n", k.proj_count());
    std::printf("  combat events (per-round journal): %zu\n", events.size());

    if (out_path) {
        std::vector<uint8_t> blob;
        client::write_recording(meta, wire_frames, events, blob);
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
