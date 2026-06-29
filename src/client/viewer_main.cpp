// SEADS raylib globe viewer (seads_viewer) — Step 5, OPTIONAL (built only with -DSEADS_CLIENT=ON).
//
// A thin, DOWNSTREAM-ONLY shell over the seads_client lib: it loads a .seadsrec recording, feeds
// the decoded 20 Hz wire frames through the layer-4a interpolation buffer, and draws the result
// on a 3D globe. It is read-only — it NEVER advances a kernel, hashes, or writes the sim, so it
// lives entirely outside the determinism gate and cannot affect a world_hash (no seal). The only
// thing it does that the kernel may not: read the wall clock (raylib GetTime()) to choose a render
// time ~100 ms in the past, which is exactly the layer-4a presentation delay.
//
// Headless self-check (no window/GPU, runnable anywhere):
//   seads_viewer flight.seadsrec --selfcheck 8
// GUI (replay):
//   seads_viewer flight.seadsrec [--speed 1.0]
//
// --- FLY mode (Track A: live local-input loop, netcode layer 4b) ---------------------------------
// With --fly the OWN aircraft is no longer replayed: it is FLOWN. Keyboard input (A/D bank,
// W/S climb) maps to a seads::Command that drives a predict::Predictor — the same client-side
// prediction harness used in netcode layer 4b, running the REAL sealed kernel at a fixed 100 Hz
// from the wall clock. The remotes keep coming from the recording on the layer-4a interpolation
// path (Playback), so prediction (own, crisp, zero-latency) and interpolation (remote, ~100 ms in
// the past) are visible on the same globe at once — the full 4a+4b loop.
//   seads_viewer flight.seadsrec --fly [--speed 1.0]
//   seads_viewer --fly --selfcheck 6        (headless; no recording or GPU needed)
// Still DOWNSTREAM-ONLY: input feeds Commands into the kernel-driving Predictor, never the wire;
// no rail/golden/world_hash is touched (no seal). NOTE: there is no authoritative server in this
// single-process viewer, so only Predictor::predict() runs here — reconcile() (snap+replay against
// a server snapshot) is exercised by the layer-4b parity tests and engages against a real server.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "playback.h"
#include "seadsrec.h"
#include "predict.h"          // seads::predict::Predictor (netcode layer 4b)
#include "envelope_tables.h"  // seads::envtab::* tuning envelopes
#include "golden_params.h"    // sealed rail constants (R, dt, g0, ceiling)
#include "raylib.h"

using namespace seads;
using namespace seads::client;

namespace {

constexpr float DISPLAY_R = 10.0f;  // globe radius in raylib units (world metres are scaled down)

// FLY-mode tuning (presentation-only; the kernel re-clamps every command to the envelope anyway).
constexpr double RAD2DEG_V = 180.0 / PI_C;            // PI_C from globe.h
constexpr double DEG2RAD_V = PI_C / 180.0;
constexpr double MAX_BANK_RAD = 60.0 * DEG2RAD_V;     // commanded bank at full A/D deflection
constexpr double MAX_CLIMB_MPS = 10.0;                // commanded climb/descent at full W/S

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { std::fclose(f); return false; }
    out.resize(static_cast<size_t>(n));
    size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

// Map a world position (metres, globe frame) to raylib display units (globe radius -> DISPLAY_R).
Vector3 to_display(const Vec3& p, double radius_m) {
    float s = DISPLAY_R / static_cast<float>(radius_m);
    return Vector3{static_cast<float>(p.x) * s, static_cast<float>(p.y) * s,
                   static_cast<float>(p.z) * s};
}

// Headless data-path proof: advance render_tick across the recording, print sampled positions.
int run_selfcheck(const Playback& pb, int n) {
    double t0 = static_cast<double>(pb.first_tick());
    double t1 = static_cast<double>(pb.last_tick());
    std::printf("selfcheck: span ticks [%.0f, %.0f], delay=%.1f ticks, %d Hz snaps\n", t0, t1,
                pb.delay_ticks(), pb.snap_hz());
    for (int i = 0; i < n; ++i) {
        double a = (n > 1) ? static_cast<double>(i) / (n - 1) : 0.0;
        double rt = t0 + a * (t1 - t0);
        std::vector<RenderEntity> ents = pb.sample(rt);
        std::printf("  t=%.1f:", rt);
        for (const auto& e : ents) {
            std::printf(" [#%lld lat=%.3f lon=%.3f alt=%.0f brg=%.1f]", static_cast<long long>(e.id),
                        e.lat_deg, e.lon_deg, e.alt_m, e.bearing_deg);
        }
        std::printf("\n");
    }
    return 0;
}

void draw_hud(const Playback& pb, double render_tick, const std::vector<RenderEntity>& ents) {
    DrawText("SEADS — ATM-Sphere globe viewer (read-only / layer-4a interp)", 12, 12, 20, RAYWHITE);
    char line[160];
    std::snprintf(line, sizeof(line), "render_tick %.1f  (%.2fs)  delay %.0fms  %dHz snaps",
                  render_tick, render_tick / pb.tick_hz(), pb.delay_ticks() / pb.tick_hz() * 1000.0,
                  pb.snap_hz());
    DrawText(line, 12, 38, 16, LIGHTGRAY);
    int y = 70;
    for (const auto& e : ents) {
        std::snprintf(line, sizeof(line),
                      "#%lld  lat %+7.3f  lon %+8.3f  alt %5.0fm  brg %5.1f  bank %+5.1f  tas %5.1f",
                      static_cast<long long>(e.id), e.lat_deg, e.lon_deg, e.alt_m, e.bearing_deg,
                      e.phi_deg, e.tas_mps);
        DrawText(line, 12, y, 16, SKYBLUE);
        y += 22;
    }
    DrawText("drag: orbit   wheel: zoom   space: pause   R: restart", 12,
             GetScreenHeight() - 28, 16, GRAY);
}

int run_gui(Playback& pb, double speed) {
    const int W = 1280, H = 720;
    InitWindow(W, H, "SEADS globe viewer");
    SetTargetFPS(60);

    Camera3D cam{};
    cam.position = Vector3{0, DISPLAY_R * 1.4f, DISPLAY_R * 2.6f};
    cam.target = Vector3{0, 0, 0};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;

    const double t0 = static_cast<double>(pb.first_tick());
    const double t1 = static_cast<double>(pb.last_tick());
    const double span = (t1 - t0);
    // Per-aircraft trails (display-space points), keyed by slot order.
    std::vector<std::vector<Vector3>> trails;
    bool paused = false;
    double sim_clock = 0.0;     // seconds of playback elapsed
    double last_wall = GetTime();

    while (!WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last_wall;
        last_wall = now;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_R)) { sim_clock = 0.0; trails.clear(); }
        if (!paused) sim_clock += dt * speed;

        UpdateCamera(&cam, CAMERA_THIRD_PERSON);

        // Render time = playback position MINUS the layer-4a delay, clamped + looped over the span.
        double played_ticks = sim_clock * pb.tick_hz();
        double loop = (span > 0) ? std::fmod(played_ticks, span) : 0.0;
        double render_tick = t0 + loop - pb.delay_ticks();
        if (render_tick < t0) render_tick = t0;

        std::vector<RenderEntity> ents = pb.sample(render_tick);
        if (trails.size() < ents.size()) trails.resize(ents.size());
        for (size_t i = 0; i < ents.size(); ++i) {
            Vector3 d = to_display(ents[i].pos, pb.radius_m());
            if (loop < pb.delay_ticks() + 1.0) trails[i].clear();  // reset trails on loop
            trails[i].push_back(d);
            if (trails[i].size() > 400) trails[i].erase(trails[i].begin());
        }

        BeginDrawing();
        ClearBackground(Color{8, 10, 18, 255});
        BeginMode3D(cam);
        DrawSphereWires(Vector3{0, 0, 0}, DISPLAY_R, 18, 24, Color{40, 70, 110, 255});
        DrawGrid(0, 0);  // no-op floor; keeps depth nice
        const Color palette[4] = {GOLD, LIME, ORANGE, VIOLET};
        for (size_t i = 0; i < ents.size(); ++i) {
            Color c = palette[i % 4];
            // Trail.
            for (size_t k = 1; k < trails[i].size(); ++k)
                DrawLine3D(trails[i][k - 1], trails[i][k], Fade(c, 0.6f));
            Vector3 d = to_display(ents[i].pos, pb.radius_m());
            DrawSphere(d, 0.12f, c);
            // Up (radial) stick so altitude/orientation reads in 3D.
            Vector3 up = to_display(geo_deg_to_cartesian(ents[i].lat_deg, ents[i].lon_deg,
                                                         ents[i].alt_m + pb.radius_m() * 0.06,
                                                         pb.radius_m()),
                                    pb.radius_m());
            DrawLine3D(d, up, Fade(c, 0.8f));
        }
        EndMode3D();
        draw_hud(pb, render_tick, ents);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

// ---- FLY mode (Track A): own ship predicted from local input, remotes interpolated -------------

// Sealed rail constants -> a Rails for the local prediction kernel (same numbers the golden uses).
Rails fly_rails() {
    Rails r;
    r.R = golden::R_M; r.dt = golden::DT_S; r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M; r.soft = golden::SOFT_M;
    return r;
}

// Own aircraft spawn (radians): mid-band altitude, heading east, cruising. KI-61 envelope.
const Envelope& kFlyEnv = envtab::KI61;
predict::OwnState fly_start() {
    return predict::OwnState{0.0, 0.0, 90.0 * DEG2RAD_V, 0.0, 2500.0, 180.0};
}

// Current keyboard state -> a per-tick Command (sampled once per frame, applied to every substep).
Command fly_input_command() {
    double tphi = 0.0, tclimb = 0.0;
    if (IsKeyDown(KEY_A)) tphi += MAX_BANK_RAD;     // bank left
    if (IsKeyDown(KEY_D)) tphi -= MAX_BANK_RAD;     // bank right
    if (IsKeyDown(KEY_W)) tclimb += MAX_CLIMB_MPS;  // climb
    if (IsKeyDown(KEY_S)) tclimb -= MAX_CLIMB_MPS;  // descend
    return Command{tphi, tclimb};
}

void draw_fly_hud(const predict::Predictor& pred, uint32_t tick, size_t n_remote, bool paused) {
    DrawText("SEADS — FLY  (own = predicted / layer-4b   remotes = interpolated / layer-4a)", 12,
             12, 20, RAYWHITE);
    const Kernel& k = pred.kernel();
    double brg = k.psi(0) * RAD2DEG_V;
    if (brg < 0) brg += 360.0;
    char line[200];
    std::snprintf(line, sizeof(line),
                  "OWN  t=%u (%.2fs)  lat %+7.3f  lon %+8.3f  alt %5.0fm  hdg %5.1f  bank %+5.1f  "
                  "tas %5.1f%s",
                  tick, tick * 0.01, k.lat(0) * RAD2DEG_V, k.lon(0) * RAD2DEG_V, k.alt(0), brg,
                  k.phi(0) * RAD2DEG_V, k.tas(0), paused ? "   [PAUSED]" : "");
    DrawText(line, 12, 40, 16, Color{255, 120, 120, 255});
    std::snprintf(line, sizeof(line), "remotes (interp): %zu", n_remote);
    DrawText(line, 12, 62, 16, SKYBLUE);
    DrawText("A/D bank   W/S climb   mouse-drag: orbit   wheel: zoom   space: pause   R: reset", 12,
             GetScreenHeight() - 28, 16, GRAY);
}

// Headless proof that the live-input -> Predictor path drives the real kernel (no window/GPU).
// Flies a fixed climbing-left-turn input for 600 ticks and prints the own state at n samples.
int run_fly_selfcheck(int n) {
    if (n < 1) n = 1;
    Rails rails = fly_rails();
    predict::Predictor pred(rails, &kFlyEnv, fly_start());
    const Command cmd{MAX_BANK_RAD * 0.6, 4.0};  // banked left + climbing
    const uint32_t TICKS = 600;
    std::printf("fly selfcheck: %u ticks @ %d Hz, input target_phi=%.4frad climb=%.1fm/s\n", TICKS,
                static_cast<int>(1.0 / rails.dt), cmd.target_phi, cmd.target_climb);
    const uint32_t every = (TICKS / static_cast<uint32_t>(n)) ? (TICKS / static_cast<uint32_t>(n)) : 1;
    for (uint32_t t = 1; t <= TICKS; ++t) {
        pred.predict(t, cmd);
        if (t % every == 0 || t == TICKS) {
            const Kernel& k = pred.kernel();
            double brg = k.psi(0) * RAD2DEG_V; if (brg < 0) brg += 360.0;
            std::printf("  t=%4u  lat=%+8.4f lon=%+8.4f alt=%7.1f hdg=%6.1f bank=%+6.2f tas=%6.1f\n",
                        t, k.lat(0) * RAD2DEG_V, k.lon(0) * RAD2DEG_V, k.alt(0), brg,
                        k.phi(0) * RAD2DEG_V, k.tas(0));
        }
    }
    return 0;
}

int run_fly(Playback& pb, double speed) {
    const int W = 1280, H = 720;
    InitWindow(W, H, "SEADS fly — predict own / interp remotes");
    SetTargetFPS(60);

    Rails rails = fly_rails();
    const predict::OwnState start = fly_start();
    predict::Predictor pred(rails, &kFlyEnv, start);
    const double DT = rails.dt;              // exact kernel tick (0.01 s)
    double accumulator = 0.0;                // wall-time carried toward the next whole tick
    uint32_t own_tick = 0;

    // Remotes ride the recording's own (looping) timeline, sampled the layer-4a delay in the past.
    const double t0 = static_cast<double>(pb.first_tick());
    const double t1 = static_cast<double>(pb.last_tick());
    const double span = t1 - t0;
    double sim_clock = 0.0;
    bool paused = false;
    double last_wall = GetTime();

    // Manual orbit camera (mouse) — WASD is reserved for flight, so we can't use raylib's built-in.
    double az = -PI_C * 0.5, el = 0.55, dist = DISPLAY_R * 2.8;
    Camera3D cam{};
    cam.up = Vector3{0, 1, 0};
    cam.fovy = 45.0f;
    cam.projection = CAMERA_PERSPECTIVE;
    cam.target = Vector3{0, 0, 0};

    std::vector<Vector3> own_trail;
    std::vector<std::vector<Vector3>> rem_trails;

    while (!WindowShouldClose()) {
        double now = GetTime();
        double dt = now - last_wall;
        last_wall = now;
        if (dt > 0.25) dt = 0.25;  // clamp huge stalls (tab background) — no spiral of death
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_R)) {
            pred = predict::Predictor(rails, &kFlyEnv, start);
            own_tick = 0; accumulator = 0.0; sim_clock = 0.0;
            own_trail.clear(); rem_trails.clear();
        }

        // Camera: drag to orbit, wheel to zoom.
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 md = GetMouseDelta();
            az -= md.x * 0.005;
            el += md.y * 0.005;
        }
        if (el > 1.5) el = 1.5;
        if (el < -1.5) el = -1.5;
        dist *= (1.0 - GetMouseWheelMove() * 0.1);
        if (dist < DISPLAY_R * 1.2) dist = DISPLAY_R * 1.2;
        if (dist > DISPLAY_R * 8.0) dist = DISPLAY_R * 8.0;
        Vec3 eye = orbit_eye(az, el, dist, Vec3{0, 0, 0});
        cam.position = Vector3{static_cast<float>(eye.x), static_cast<float>(eye.y),
                               static_cast<float>(eye.z)};

        // Fly the own ship: fixed-timestep prediction from the current input.
        Command cmd = fly_input_command();
        if (!paused) {
            accumulator += dt * speed;
            sim_clock += dt * speed;
            while (accumulator >= DT) {
                pred.predict(++own_tick, cmd);
                accumulator -= DT;
            }
        }

        // Remotes: interpolate at (looped playback time - layer-4a delay).
        double played = sim_clock * pb.tick_hz();
        double loop = (span > 0) ? std::fmod(played, span) : 0.0;
        double render_tick = t0 + loop - pb.delay_ticks();
        if (render_tick < t0) render_tick = t0;
        std::vector<RenderEntity> rem = pb.sample(render_tick);

        Vec3 owp = geo_to_cartesian(pred.kernel().lat(0), pred.kernel().lon(0), pred.kernel().alt(0),
                                    pb.radius_m());
        Vector3 od = to_display(owp, pb.radius_m());
        if (!paused) {
            own_trail.push_back(od);
            if (own_trail.size() > 600) own_trail.erase(own_trail.begin());
        }
        if (rem_trails.size() < rem.size()) rem_trails.resize(rem.size());
        for (size_t i = 0; i < rem.size(); ++i) {
            Vector3 d = to_display(rem[i].pos, pb.radius_m());
            if (loop < pb.delay_ticks() + 1.0) rem_trails[i].clear();
            rem_trails[i].push_back(d);
            if (rem_trails[i].size() > 400) rem_trails[i].erase(rem_trails[i].begin());
        }

        BeginDrawing();
        ClearBackground(Color{8, 10, 18, 255});
        BeginMode3D(cam);
        DrawSphereWires(Vector3{0, 0, 0}, DISPLAY_R, 18, 24, Color{40, 70, 110, 255});
        const Color pal[4] = {GOLD, LIME, ORANGE, VIOLET};
        for (size_t i = 0; i < rem.size(); ++i) {
            Color c = pal[i % 4];
            for (size_t k = 1; k < rem_trails[i].size(); ++k)
                DrawLine3D(rem_trails[i][k - 1], rem_trails[i][k], Fade(c, 0.5f));
            Vector3 d = to_display(rem[i].pos, pb.radius_m());
            DrawSphere(d, 0.10f, c);
            Vector3 up = to_display(geo_deg_to_cartesian(rem[i].lat_deg, rem[i].lon_deg,
                                                         rem[i].alt_m + pb.radius_m() * 0.05,
                                                         pb.radius_m()),
                                    pb.radius_m());
            DrawLine3D(d, up, Fade(c, 0.7f));
        }
        // Own ship: hot, larger, with a longer trail.
        for (size_t k = 1; k < own_trail.size(); ++k)
            DrawLine3D(own_trail[k - 1], own_trail[k], Fade(RED, 0.8f));
        DrawSphere(od, 0.16f, RED);
        Vec3 oup_geo = geo_to_cartesian(pred.kernel().lat(0), pred.kernel().lon(0),
                                        pred.kernel().alt(0) + pb.radius_m() * 0.07, pb.radius_m());
        DrawLine3D(od, to_display(oup_geo, pb.radius_m()), Fade(RED, 0.9f));
        EndMode3D();
        draw_fly_hud(pred, own_tick, rem.size(), paused);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string path;
    int selfcheck = 0;
    double speed = 1.0;
    bool fly = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selfcheck" && i + 1 < argc) selfcheck = std::atoi(argv[++i]);
        else if (a == "--speed" && i + 1 < argc) speed = std::atof(argv[++i]);
        else if (a == "--fly") fly = true;
        else if (!a.empty() && a[0] != '-') path = a;
    }
    // Fly + headless needs neither a recording nor a GPU — run it before requiring a file.
    if (fly && selfcheck > 0) return run_fly_selfcheck(selfcheck);
    if (path.empty()) {
        std::fprintf(stderr,
                     "usage: seads_viewer <flight.seadsrec> [--fly] [--selfcheck N] [--speed S]\n"
                     "       seads_viewer --fly --selfcheck N            (headless, no recording)\n");
        return 2;
    }
    std::vector<uint8_t> blob;
    if (!read_file(path, blob)) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return 2; }
    Recording rec;
    if (!read_recording(blob.data(), blob.size(), rec)) {
        std::fprintf(stderr, "bad recording: %s\n", path.c_str());
        return 2;
    }
    Playback pb;
    if (!pb.load(rec)) { std::fprintf(stderr, "empty recording\n"); return 2; }
    std::printf("loaded %s: %u frames, radius %.0fm, %dHz physics / %dHz snaps\n", path.c_str(),
                rec.meta.n_frames, pb.radius_m(), pb.tick_hz(), pb.snap_hz());

    if (selfcheck > 0) return run_selfcheck(pb, selfcheck);
    if (fly) return run_fly(pb, speed);
    return run_gui(pb, speed);
}
