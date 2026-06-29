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
// GUI:
//   seads_viewer flight.seadsrec [--speed 1.0]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "playback.h"
#include "seadsrec.h"
#include "raylib.h"

using namespace seads;
using namespace seads::client;

namespace {

constexpr float DISPLAY_R = 10.0f;  // globe radius in raylib units (world metres are scaled down)

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

}  // namespace

int main(int argc, char** argv) {
    std::string path;
    int selfcheck = 0;
    double speed = 1.0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selfcheck" && i + 1 < argc) selfcheck = std::atoi(argv[++i]);
        else if (a == "--speed" && i + 1 < argc) speed = std::atof(argv[++i]);
        else if (!a.empty() && a[0] != '-') path = a;
    }
    if (path.empty()) {
        std::fprintf(stderr, "usage: seads_viewer <flight.seadsrec> [--selfcheck N] [--speed S]\n");
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
    return run_gui(pb, speed);
}
