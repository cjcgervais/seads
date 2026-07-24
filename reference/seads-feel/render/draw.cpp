#include "render/draw.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "raylib.h"
#include "raymath.h"  // MatrixFrustum (off-center lens-shift projection)
#include "render/orient_cues.h"  // S-cues (comfort): ghost horizon + bank arc
#include "render/planet.h"
#include "render/readout.h"
#include "rlgl.h"
#include "sim/world.h"

namespace render {

namespace {

// The seam: re-base to the camera eye in double, THEN cast (see draw.h).
Vector3 rel(const glm::dvec3& world, const glm::dvec3& eye) {
    const glm::dvec3 r = world - eye;
    return Vector3{static_cast<float>(r.x), static_cast<float>(r.y),
                   static_cast<float>(r.z)};
}

// S-cues palette (comfort program): a warm SLAG-ORANGE family so the
// orientation cues read as the smelter-lit HUD they are, single-sourced HERE so
// no color literal scatters through the glue. kCueSlag = the BRIGHT primary
// (main ghost-horizon line + bank pointer); kCueEmber = the DIMMER member (bank
// arc frame + ticks + the pitch-ladder rungs). Alpha is filled per-draw from
// the config dials (kept from the existing cue_* alphas). RGB only here.
constexpr unsigned char kCueSlagR = 255, kCueSlagG = 140, kCueSlagB = 50;
constexpr unsigned char kCueEmberR = 220, kCueEmberG = 110, kCueEmberB = 45;

// Cubesphere build params, sourced from config/world.toml via
// set_planet_build_params() (called once at startup from app/main.cpp) so no
// bare geometry numbers live here (docs/world_build_plan.md §1). Defaults (in
// draw.h) reproduce the pre-config planet.
PlanetBuildParams g_planet_cfg{};

// Textured, DEM-displaced cubesphere (render/planet.*). Built once on the
// first frame (needs a live GL context — draw_planet only runs inside
// BeginMode3D, well after InitWindow). Relief is render-only; the physics
// crash surface is still the perfect sphere at R (SPEC §6).
//   relief_scale = metres at full-white DEM (Everest-class peaks read on a
//   15 km toy planet); subdiv = cubesphere verts/edge/face.
void draw_planet(const sim::AircraftParams& params, const glm::dvec3& eye) {
    // Geometry knobs come from config/world.toml (g_planet_cfg, set once at
    // startup); u_offset still puts the steep equatorial relief under the +X
    // spawn sub-point for a legible opening view (until Sudbury replaces it,
    // P1).
    static Planet planet =
        load_planet(SEADS_ASSET_DIR, params.R, g_planet_cfg.relief_scale,
                    g_planet_cfg.subdiv, g_planet_cfg.u_offset,
                    g_planet_cfg.dem_blur_radius);
    // Fixed world-space sun (directional: no eye rebasing). RAKING the spawn
    // sub-point (+X) at a low angle so terrain casts shadow and relief reads;
    // sun is the light-TRAVEL direction, a point is lit when its normal faces
    // -sun (-sun ~ (0.45,0.75,0.48): ~27° elevation over the +X sub-point).
    static const glm::vec3 sun =
        glm::normalize(glm::vec3{-0.45f, -0.75f, -0.48f});

    if (planet.ok) {
        draw_planet_mesh(planet, eye, sun);
        return;
    }
    // Fallback: assets or GPU features unavailable — keep the sim flyable with
    // the original untextured sphere (no floating wire shell).
    const Vector3 center = rel(glm::dvec3{0.0}, eye);
    DrawSphereEx(center, static_cast<float>(params.R), 64, 96,
                 Color{72, 108, 58, 255});
}

// Body-frame primitives (SPEC §7: +X right, +Y up, -Z forward), drawn under
// the aircraft's model matrix. `enemy` swaps to the saturated bandit livery
// (S8-drone): the world art direction wants the planes as the only chroma —
// the player stays neutral gunmetal + red nose, the bandit is a saturated warm
// scheme so it reads as hostile at a glance (full complementary liveries are
// the world thread). Defaulted false => the player draw is bit-unchanged.
void draw_aircraft(const sim::SimState& state, const glm::dvec3& eye,
                   bool enemy = false, double scale = 1.0) {
    const Vector3 p = rel(state.position, eye);
    const glm::mat4 body_to_world =
        glm::mat4_cast(glm::quat(state.orientation));

    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlMultMatrixf(glm::value_ptr(body_to_world));  // glm + rlgl: column-major
    const float s = static_cast<float>(scale);     // cosmetic size (S8-drone)
    rlScalef(s, s, s);

    const Color fuse =
        enemy ? Color{188, 74, 42, 255} : Color{90, 96, 104, 255};
    const Color wing = enemy ? Color{158, 58, 32, 255} : Color{70, 76, 84, 255};
    const Color tail = enemy ? Color{158, 58, 32, 255} : Color{70, 76, 84, 255};
    const Color fin =
        enemy ? Color{206, 96, 54, 255} : Color{104, 110, 118, 255};
    const Color nose =
        enemy ? Color{240, 208, 72, 255} : Color{190, 60, 50, 255};

    DrawCube(Vector3{0.0f, 0.0f, -0.8f}, 1.2f, 1.3f, 8.5f, fuse);    // fuselage
    DrawCube(Vector3{0.0f, -0.1f, 0.2f}, 11.0f, 0.25f, 2.3f, wing);  // wing
    DrawCube(Vector3{0.0f, 0.2f, 3.6f}, 4.2f, 0.2f, 1.3f, tail);   // tailplane
    DrawCube(Vector3{0.0f, 1.0f, 3.7f}, 0.2f, 1.8f, 1.4f, fin);    // fin
    DrawCube(Vector3{0.0f, 0.0f, -5.2f}, 0.7f, 0.7f, 1.0f, nose);  // nose cap
    rlPopMatrix();
}

}  // namespace

void set_planet_build_params(const PlanetBuildParams& p) { g_planet_cfg = p; }

void init_draw() {
    // 15 km planet: raylib's default far plane (1000 m) would cull the
    // world. Horizon slant range tops out ~sqrt(h(2R+h)); 60 km covers any
    // survivable altitude. Near at 2 m — depth precision scales linearly
    // with it, and nothing renders closer than the chase offset anyway; at
    // 0.5 m the far half of the sphere z-fought its own wire overlay.
    rlSetClipPlanes(2.0, 60000.0);
}

// Cosmetic viewport frame (the "custom frame for the graphics"): a thin HUD
// bezel with corner brackets and a title strip. Pure 2D overlay — reads state
// only through the info/readout already computed. Kept faint so it never
// competes with the reticle/nose-marker pair.
void draw_bezel(int sw, int sh) {
    const Color edge = {120, 200, 255, 70};      // faint cyan frame
    const Color bracket = {150, 220, 255, 190};  // brighter corner accents
    constexpr int m = 16;                        // inset margin
    constexpr int L = 30;                        // corner bracket length
    constexpr float t = 2.0f;                    // bracket thickness

    DrawRectangleLines(m, m, sw - 2 * m, sh - 2 * m, edge);

    const float x0 = static_cast<float>(m), y0 = static_cast<float>(m);
    const float x1 = static_cast<float>(sw - m),
                y1 = static_cast<float>(sh - m);
    // Four L-shaped corner brackets.
    DrawLineEx({x0, y0}, {x0 + L, y0}, t, bracket);
    DrawLineEx({x0, y0}, {x0, y0 + L}, t, bracket);
    DrawLineEx({x1, y0}, {x1 - L, y0}, t, bracket);
    DrawLineEx({x1, y0}, {x1, y0 + L}, t, bracket);
    DrawLineEx({x0, y1}, {x0 + L, y1}, t, bracket);
    DrawLineEx({x0, y1}, {x0, y1 - L}, t, bracket);
    DrawLineEx({x1, y1}, {x1 - L, y1}, t, bracket);
    DrawLineEx({x1, y1}, {x1, y1 - L}, t, bracket);

    // Title along the bottom edge (clear of the top-left flight HUD).
    DrawText("SEADS  ·  SPHERICAL EARTH  ·  R=15 km", m + 10, sh - m - 18, 14,
             Color{170, 210, 245, 190});
}

// MB HUD attitude dial (evolves the MB-7c AoA dial, Chad 2026-07-08): the
// needle now reads PITCH ATTITUDE — the nose's angle to the local horizon, a
// read that HOLDS its value (the AoA delta zeroed out in steady flight) —
// and an outer ROLL ARC carries a full-range bank pointer (the ADI-style
// bank indicator). Stall awareness moves to a slim AoA strip on the gauge's
// LEFT (inside the roll ring it would collide with the bank pointer exactly
// at knife-edge reads). Reads ONLY the shared flight_readout + config limits
// on FrameInfo (the flat-instrument rule); hidden until aoa_max is set.
void draw_attitude_dial(const FlightReadout& r, const FrameInfo& info, int sw,
                        int sh) {
    if (info.aoa_max <= 0.0) return;
    const float cx = static_cast<float>(sw) - 150.0f;
    const float cy = 0.5f * static_cast<float>(sh);
    const float R = 108.0f;      // big, per the MB-7c ruling
    const float Rb = R + 22.0f;  // the roll-arc ring

    // Pitch needle geometry: fixed +/-90 deg span mapped onto the +/-70 deg
    // screen sweep; phi = needle angle from horizontal-left, + = up.
    constexpr double kSweep = 70.0 * PI / 180.0;
    constexpr double kHalfPi = 0.5 * PI;
    const auto phi_of = [&](double pitch) {
        const double t = std::clamp((pitch + kHalfPi) / PI, 0.0, 1.0);
        return -kSweep + 2.0 * kSweep * t;
    };
    const auto pt = [&](double phi, float rad) {
        return Vector2{cx - rad * static_cast<float>(std::cos(phi)),
                       cy - rad * static_cast<float>(std::sin(phi))};
    };

    // Reference ticks every 30 deg of pitch; the horizon (0) drawn heavier —
    // it is THE reference the read holds against.
    for (const double d : {-90.0, -60.0, -30.0, 0.0, 30.0, 60.0, 90.0}) {
        const double p = phi_of(d * PI / 180.0);
        const bool horizon = d == 0.0;
        DrawLineEx(
            pt(p, R - (horizon ? 16.0f : 10.0f)), pt(p, R + 6.0f),
            horizon ? 3.0f : 2.0f,
            horizon ? Color{225, 240, 255, 220} : Color{200, 225, 245, 150});
    }

    // The pitch needle — the one big moving element (white; the stall
    // coloring lives on the AoA strip now).
    const double np = phi_of(r.pitch_attitude);
    DrawLineEx(pt(np, 18.0f), pt(np, R - 14.0f), 4.0f, RAYWHITE);
    DrawCircleV(Vector2{cx, cy}, 5.0f, Color{200, 225, 245, 200});

    // Roll arc: bank angle psi measured from 12 o'clock, + = right wing down
    // = clockwise on screen (turn-coordinator convention — the pointer moves
    // WITH the bank; flight-log question if Chad reads it backwards).
    // pointer = r.bank_full, the FULL +/-180 read — NEVER the folded r.bank
    // (bank 100 would read 80). Past +/-90 the pointer swings below the
    // horizontal and inverted sits at 6 o'clock: real attitudes, by design.
    const auto ptb = [&](double psi, float rad) {
        return Vector2{cx + rad * static_cast<float>(std::sin(psi)),
                       cy - rad * static_cast<float>(std::cos(psi))};
    };
    // Faint ring context across the upright half, ticks at 0/30/60/90.
    {
        const double p0 = -kHalfPi, p1 = kHalfPi;
        const int n = 48;
        for (int i = 0; i < n; ++i) {
            const double u0 = p0 + (p1 - p0) * (static_cast<double>(i) / n);
            const double u1 = p0 + (p1 - p0) * (static_cast<double>(i + 1) / n);
            DrawLineEx(ptb(u0, Rb), ptb(u1, Rb), 2.0f,
                       Color{120, 200, 255, 60});
        }
    }
    for (const double d : {-90.0, -60.0, -30.0, 0.0, 30.0, 60.0, 90.0}) {
        const double psi = d * PI / 180.0;
        const bool top = d == 0.0;
        DrawLineEx(ptb(psi, Rb - 6.0f), ptb(psi, Rb + 6.0f), top ? 3.0f : 2.0f,
                   top ? Color{225, 240, 255, 220} : Color{200, 225, 245, 150});
    }
    // The bank pointer: a bold tick riding the ring at bank_full.
    DrawLineEx(ptb(r.bank_full, Rb - 12.0f), ptb(r.bank_full, Rb + 6.0f), 4.0f,
               Color{140, 235, 160, 255});

    // Labels + live numerics under the pivot: pitch big, bank beneath it.
    constexpr double kRadToDeg = 57.2957795130823;
    char t[24];
    std::snprintf(t, sizeof t, "%+3.0f", r.pitch_attitude * kRadToDeg);
    DrawText("PITCH", static_cast<int>(cx) - 26, static_cast<int>(cy) + 16, 18,
             Color{170, 210, 245, 200});
    DrawText(t, static_cast<int>(cx) - 20, static_cast<int>(cy) + 36, 20,
             RAYWHITE);
    std::snprintf(t, sizeof t, "BANK %+4.0f", r.bank_full * kRadToDeg);
    DrawText(t, static_cast<int>(cx) - 44, static_cast<int>(cy) + 60, 16,
             Color{140, 235, 160, 220});

    // Slim AoA stall strip on the LEFT of the gauge: the old dial's exact
    // display span and zone colors, vertical (up = more AoA). The dial, the
    // vortices, and the protection clamp all stay keyed to the same aoa_max.
    const double disp_max = std::max(info.stall_alpha, info.aoa_max) * 1.15;
    const double disp_min = -info.aoa_max_neg * 1.30;
    const float xs = cx - Rb - 24.0f;  // clear of the roll ring
    const float half_h = 100.0f;
    const auto y_of = [&](double a) {
        const double u =
            std::clamp((a - disp_min) / (disp_max - disp_min), 0.0, 1.0);
        return cy + half_h - 2.0f * half_h * static_cast<float>(u);
    };
    const auto seg = [&](double a0, double a1, Color col) {
        const float y1 = y_of(a0), y0 = y_of(a1);  // y grows down
        DrawRectangleRec(Rectangle{xs, y0, 6.0f, y1 - y0}, col);
    };
    const double amber_from = kVortexAoAFrac * info.aoa_max;
    seg(disp_min, amber_from, Color{110, 220, 140, 130});
    seg(amber_from, info.aoa_max, Color{240, 190, 70, 180});
    seg(info.aoa_max, disp_max, Color{235, 80, 60, 200});
    // Current-AoA caret (the old needle's warm-up color logic).
    Color ccol = RAYWHITE;
    if (std::abs(r.aoa) >= info.aoa_max)
        ccol = Color{245, 90, 70, 255};
    else if (r.aoa >= amber_from)
        ccol = Color{245, 200, 90, 255};
    const float yc = y_of(r.aoa);
    DrawTriangle(Vector2{xs - 9.0f, yc - 5.0f}, Vector2{xs - 9.0f, yc + 5.0f},
                 Vector2{xs - 1.0f, yc}, ccol);
    DrawText("AoA", static_cast<int>(xs) - 12,
             static_cast<int>(cy + half_h) + 8, 14, Color{170, 210, 245, 180});
}

// MB-7c (iv): the energy cluster — big alt/speed/G with a speed-TREND cue
// (chevrons: green climbing the energy hill, red bleeding). Bottom-left, top
// of the status stack (flaps/gear + telemetry sit below it, bezel title last).
void draw_energy_cluster(const FlightReadout& r, const FrameInfo& info,
                         int sh) {
    const int x = 34;
    const int y = sh - 232;
    char t[48];
    std::snprintf(t, sizeof t, "%3.0f", r.speed);
    DrawText("SPD", x, y, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y - 10, 38, RAYWHITE);
    DrawText("m/s", x + 130, y + 8, 16, Color{170, 210, 245, 170});
    // Trend chevrons: 1..3 by |dV/dt|, up = gaining. Deadband 0.4 m/s^2 so
    // level cruise shows calm.
    const double tr = info.speed_trend;
    const int n = tr > 0.4    ? std::min(3, 1 + static_cast<int>(tr / 2.0))
                  : tr < -0.4 ? -std::min(3, 1 + static_cast<int>(-tr / 2.0))
                              : 0;
    for (int i = 0; i < std::abs(n); ++i) {
        const float bx = static_cast<float>(x + 180);
        const float by = static_cast<float>(y + 12 - 8 * i);
        // Up-chevron (gaining): base corners LOW (+5, screen y down), apex
        // HIGH (-5). s = +1 gaining / -1 bleeding flips it.
        const float s = n > 0 ? 1.0f : -1.0f;
        const Color c =
            n > 0 ? Color{110, 230, 140, 220} : Color{240, 110, 90, 220};
        DrawLineEx({bx, by + 5 * s}, {bx + 7, by - 5 * s}, 3.0f, c);
        DrawLineEx({bx + 7, by - 5 * s}, {bx + 14, by + 5 * s}, 3.0f, c);
    }
    std::snprintf(t, sizeof t, "%5.0f", r.altitude);
    DrawText("ALT", x, y + 46, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y + 38, 30, Color{225, 235, 250, 235});
    DrawText("m", x + 148, y + 50, 16, Color{170, 210, 245, 170});
    std::snprintf(t, sizeof t, "%+4.1f", r.load_factor);
    DrawText("G", x, y + 86, 18, Color{170, 210, 245, 200});
    DrawText(t, x + 52, y + 78, 30,
             std::abs(r.load_factor) > 7.0 ? Color{245, 200, 90, 240}
                                           : Color{225, 235, 250, 235});
}

// MB HUD status stack (Chad 2026-07-08: "I cannot even read the HUD at the
// top of the screen"): the flight telemetry line + a labeled FLAPS/GEAR
// block, bottom-left above the bezel title, under the energy cluster. Labels
// read the COMMANDED detents (FrameInfo, the same latch the sim command
// reads); deploy bars read the slewed plant positions and show only while
// the device is in motion toward its target.
void draw_status_stack(const FlightReadout& r, const sim::SimState& state,
                       const sim::AircraftParams& params, const FrameInfo& info,
                       int sh) {
    constexpr double kRadToDeg = 57.2957795130823;
    const int x = 34;
    char line[160];

    // Deploy bar: outline + fill fraction, drawn only mid-slew.
    const auto bar = [&](int y, double pos, double target) {
        if (std::abs(pos - target) <= 0.01) return;
        const float bx = static_cast<float>(x + 150);
        const float by = static_cast<float>(y + 5);
        DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), 80, 8,
                           Color{170, 210, 245, 200});
        DrawRectangleRec(
            Rectangle{bx + 1.0f, by + 1.0f,
                      78.0f * static_cast<float>(std::clamp(pos, 0.0, 1.0)),
                      6.0f},
            Color{240, 190, 70, 220});
    };

    // FLAPS + GEAR lines (always shown — the "better indication" ask).
    const double flap_target = info.flap_mode == 0 ? 0.0
                               : info.flap_mode == 1
                                   ? static_cast<double>(params.flap_combat)
                                   : 1.0;
    std::snprintf(line, sizeof line, "FLAPS %s",
                  flap_mode_label(info.flap_mode));
    DrawText(line, x, sh - 112, 18, Color{225, 235, 250, 235});
    bar(sh - 112, state.flap, flap_target);
    std::snprintf(line, sizeof line, "GEAR %s",
                  info.gear_down_cmd ? "DOWN" : "UP");
    DrawText(line, x, sh - 88, 18, Color{225, 235, 250, 235});
    bar(sh - 88, state.gear, info.gear_down_cmd ? 1.0 : 0.0);

    // The telemetry line (moved from the unreadable top strip). BANK prints
    // the FULL-RANGE read — the folded phi is the exact lie the roll arc
    // exists to fix.
    std::snprintf(line, sizeof line,
                  "%s  V %5.1f m/s  ALT %6.0f m  THR %3.0f%%  G %+4.1f  "
                  "AoA %+5.1f  BANK %+5.0f  %d fps",
                  info.raw_mode ? "RAW  " : "INSTR", r.speed, r.altitude,
                  state.throttle * 100.0, r.load_factor, r.aoa * kRadToDeg,
                  r.bank_full * kRadToDeg, info.fps);
    DrawText(line, x, sh - 60, 18, RAYWHITE);
}

void draw_frame(const sim::SimState& state, const sim::AircraftParams& params,
                const CameraPose& pose, const FrameInfo& info) {
    Camera3D cam{};
    cam.position = Vector3{0.0f, 0.0f, 0.0f};  // eye-relative world
    cam.target = rel(pose.target, pose.eye);
    cam.up =
        Vector3{static_cast<float>(pose.up.x), static_cast<float>(pose.up.y),
                static_cast<float>(pose.up.z)};
    // Current FOV (RMB-zoom eases it). The off-center frustum (line ~157) and
    // the reticle projection (line ~196) both read cam.fovy below, so this one
    // assignment carries the zoom to all three consistently.
    cam.fovy = static_cast<float>(info.fovy_deg);
    cam.projection = CAMERA_PERSPECTIVE;

    BeginDrawing();
    ClearBackground(Color{18, 24, 44, 255});

    BeginMode3D(cam);
    // Vertical lens shift (SPEC §9.2 framing): override raylib's symmetric
    // projection with an off-center frustum so the whole 3D scene slides down
    // (the reticle overlay below subtracts the SAME shift). The camera's look
    // direction is untouched — only the projection's vertical center moves.
    if (info.lens_shift_ndc != 0.0) {
        const double aspect3d = static_cast<double>(GetScreenWidth()) /
                                static_cast<double>(GetScreenHeight());
        const double nearZ = rlGetCullDistanceNear();
        const FrustumBounds fb =
            off_center_frustum(static_cast<double>(cam.fovy) * PI / 180.0,
                               aspect3d, nearZ, info.lens_shift_ndc);
        // rlSetMatrixProjection is a no-op under a GL 1.1 build (rlgl gates it
        // on GRAPHICS_API_OPENGL_33/ES2); desktop raylib defaults to GL33 so
        // this is live. Under GL11 the 3D scene would stay symmetric while the
        // reticle still shifts — decoupled. Not reachable in the shipped
        // desktop build.
        rlSetMatrixProjection(MatrixFrustum(fb.l, fb.r, fb.b, fb.t, nearZ,
                                            rlGetCullDistanceFar()));
    }
    draw_planet(params, pose.eye);
    draw_aircraft(state, pose.eye);
    for (const sim::SimState& d : info.drones_draw)
        draw_aircraft(d, pose.eye, /*enemy=*/true, info.drone_scale);
    // MB-7c (iii): wingtip vortex trails — fading world-space streamers off
    // the tips near stall AoA / high G. Read-only cosmetic overlay; points
    // are re-based to the eye at draw time (the double->float seam).
    if (info.vortices != nullptr) {
        const auto draw_trail = [&](const std::vector<VortexPoint>& tr) {
            for (std::size_t i = 1; i < tr.size(); ++i) {
                const VortexPoint& a = tr[i - 1];
                const VortexPoint& b = tr[i];
                // Break across respawn/teleport gaps.
                if (glm::length(b.pos - a.pos) > 30.0) continue;
                const double fade = b.strength * (1.0 - b.age / kVortexLife);
                if (fade <= 0.0) continue;
                const unsigned char al =
                    static_cast<unsigned char>(200.0 * fade);
                DrawLine3D(rel(a.pos, pose.eye), rel(b.pos, pose.eye),
                           Color{225, 240, 255, al});
            }
        };
        draw_trail(info.vortices->left);
        draw_trail(info.vortices->right);
    }
    EndMode3D();

    // Correct-frame HUD (SPEC §12): the readouts come through the ONE shared
    // render::flight_readout — velocity-relative AoA, local_up-aware bank/G.
    // The telemetry line itself lives in the bottom-left status stack now
    // (Chad: the top strip was unreadable); only the help line stays up top.
    const FlightReadout r = flight_readout(state, params);
    DrawText(info.raw_mode
                 ? "RAW: S/W pitch  A/D roll  Q/E yaw  Shift/Ctrl throttle  "
                   "RMB stick   [F1] instructor"
                 : "MOUSE aim   S/W A/D Q/E override   SPACE freelook   "
                   "Shift/Ctrl throttle   [F1] raw",
             12, 12, 16, Color{200, 200, 200, 180});

    // Reticle/nose-marker pair (SPEC §9.2): the on-screen gap IS the
    // controller's error — to within S-reticle's hard-capped display ease
    // (info.reticle_dir; the raw aim is untouched upstream). Instructor mode
    // only — raw mode has no aim state.
    if (!info.raw_mode) {
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const double fovy_rad = static_cast<double>(cam.fovy) * PI / 180.0;
        const double aspect = static_cast<double>(sw) / sh;
        const glm::dvec3 cam_forward = glm::normalize(pose.target - pose.eye);
        // FLOAT pixel coords (S-reticle): the smoothed sub-pixel motion must
        // not be re-quantized by an int cast; reticle, freelook ring, and
        // nose marker all take the same float path so the §9.2 pair degrades
        // symmetrically. Same lens-shift subtraction as before (NDC +y = up).
        const auto to_pxf = [&](const ScreenPoint& p) {
            return Vector2{static_cast<float>((p.x * 0.5 + 0.5) * sw),
                           static_cast<float>(
                               (0.5 - (p.y - info.lens_shift_ndc) * 0.5) * sh)};
        };
        // REC-6 — STYLIZED COCKPIT FRAME (the steady-state REST FRAME, drawn
        // UNDER all the HUD cues below): a subtle, SCREEN-ANCHORED peripheral
        // canopy interior. It is STATIC (does NOT ride the scene / lens shift —
        // that is the whole point: a stationary peripheral reference suppresses
        // vection). NDC -> pixels WITHOUT the lens-shift subtraction (unlike
        // to_pxf). alpha 0 => SKIPPED (strict superset). Dark steel-blue base
        // with an ember-orange accent on the INNER line of each doubled strut.
        if (info.cue_cockpit_alpha > 0.0) {
            const auto to_px_static = [&](const glm::dvec2& n) {
                return Vector2{static_cast<float>((n.x * 0.5 + 0.5) * sw),
                               static_cast<float>((0.5 - n.y * 0.5) * sh)};
            };
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_cockpit_alpha, 0.0, 1.0) * 255.0);
            const Color steel{60, 75, 95, a};                          // base
            const Color ember{kCueEmberR, kCueEmberG, kCueEmberB, a};  // accent
            const CockpitFrame fr = cockpit_frame(aspect);
            for (int i = 0; i < fr.count; ++i) {
                DrawLineEx(to_px_static(fr.segs[i].a),
                           to_px_static(fr.segs[i].b), 1.5f,
                           fr.segs[i].inner ? ember : steel);
            }
        }
        // S-cues (comfort program): peripheral, world-stable orientation cues,
        // drawn UNDER the reticle/nose/pipper layer (they are the backdrop the
        // pilot glances at, never at the reticle). alpha 0 => SKIPPED entirely,
        // so the shipped default frame is bit-identical (strict superset).
        // Cue A — GHOST HORIZON: the local LEVEL line (⊥ local_up), recomputed
        // fresh from normalize(position) every frame (SPEC §6.1, never cached),
        // with a center exclusion gap (nothing where the pilot aims).
        if (info.cue_horizon_alpha > 0.0) {
            // local_up taken at the AIRCRAFT (state.position), matching the
            // control::extract phi frame — NOT the eye at chase distance. The
            // two differ by ~0.1° at chase range on R=15 km; using the airframe
            // point keeps the ghost line's "up" the same up the bank arc reads.
            const glm::dvec3 local_up = glm::normalize(state.position);
            const GhostHorizonResult gh = ghost_horizon_segments(
                cam_forward, pose.up, local_up, fovy_rad, aspect,
                info.lens_shift_ndc, info.cue_horizon_gap_frac);
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_horizon_alpha, 0.0, 1.0) * 255.0);
            // SLAG-ORANGE recolor: the main horizon line + its ticks are the
            // BRIGHT slag; the pitch-ladder rungs below are the DIMMER ember.
            const Color hc{kCueSlagR, kCueSlagG, kCueSlagB, a};
            // Draw a set of NDC segments (with sky-side ticks) at a stroke.
            const auto stroke_segs = [&](const GhostHorizonResult& g,
                                         const Color& col) {
                for (int i = 0; i < g.count; ++i) {
                    const ScreenPoint pa{g.segs[i].a.x, g.segs[i].a.y, true};
                    const ScreenPoint pb{g.segs[i].b.x, g.segs[i].b.y, true};
                    DrawLineEx(to_pxf(pa), to_pxf(pb), 1.5f, col);
                    // Sky-side tick: points toward projected +local_up (UP
                    // upright, DOWN inverted) — resolves the line's up/down
                    // ambiguity. Same alpha as the line.
                    const ScreenPoint tr{g.segs[i].tick_root.x,
                                         g.segs[i].tick_root.y, true};
                    const ScreenPoint tk{g.segs[i].tick.x, g.segs[i].tick.y,
                                         true};
                    DrawLineEx(to_pxf(tr), to_pxf(tk), 1.5f, col);
                }
            };
            stroke_segs(gh, hc);

            // GHOST PITCH LADDER (the angle indicator): SHORT rungs at ±15/±30/
            // ±45° elevation, in the DIMMER ember at 0.7x the horizon alpha, so
            // they read as ladder rungs backing the main line. Gated by the
            // SAME cue_horizon_alpha dial (the ladder is part of the horizon
            // cue — no new dial). Each rung reuses the ghost-horizon math via
            // ghost_ladder (elev=0 special case IS the horizon); out-of-view
            // rungs emit 0 segments.
            const unsigned char la = static_cast<unsigned char>(
                std::clamp(info.cue_horizon_alpha, 0.0, 1.0) * 0.7 * 255.0);
            const Color lc{kCueEmberR, kCueEmberG, kCueEmberB, la};
            constexpr double kDeg = 3.14159265358979323846 / 180.0;
            for (double elev_deg : {15.0, -15.0, 30.0, -30.0, 45.0, -45.0}) {
                const GhostHorizonResult rung =
                    ghost_ladder(cam_forward, pose.up, local_up, fovy_rad,
                                 aspect, info.lens_shift_ndc,
                                 info.cue_horizon_gap_frac, elev_deg * kDeg);
                stroke_segs(rung, lc);
            }
        }
        // Cue B — BANK ARC (Falcon 4.0 convention): a small arc at the TOP
        // center with ticks at 0/±10/±20/±30/±45/±60° and a moving pointer
        // showing aircraft bank φ (from the SHARED extraction via
        // flight_readout, single source, SPEC §7). Screen-anchored periphery.
        if (info.cue_bank_arc_alpha > 0.0) {
            // FULL-RANGE bank (±π), reusing the readout `r` above: the folded
            // ±90° phi reads WINGS LEVEL while inverted (readout.h: never feed
            // a roll gauge phi). bank_full unfolds through 180° so the pointer
            // pegs hard-over past the scale instead of snapping to center.
            const double phi = r.bank_full;
            const BankArcGeometry ba = bank_arc_geometry(
                phi, /*arc_span_deg=*/60.0,
                /*arc_sweep_rad=*/1.2217);  // 1.2217 rad = 70° total sweep
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_bank_arc_alpha, 0.0, 1.0) * 255.0);
            // SLAG-ORANGE recolor: the arc frame + ticks are the DIMMER ember;
            // the moving bank pointer is the BRIGHT slag (matches the horizon
            // line — the two primary attitude reads share the bright tone).
            const Color ac{kCueEmberR, kCueEmberG, kCueEmberB, a};
            const Color pc{kCueSlagR, kCueSlagG, kCueSlagB, a};
            // Layout: pivot below the top edge, arc radius in pixels. The pure
            // dir vectors are in +y-DOWN screen coords, so pivot + r*dir lands
            // the ticks along the top arc.
            const float cx = static_cast<float>(sw) * 0.5f;
            const float radius = static_cast<float>(sh) * 0.14f;
            const float pivot_y = static_cast<float>(sh) * 0.03f + radius;
            const Vector2 pivot{cx, pivot_y};
            for (int i = 0; i < ba.tick_count; ++i) {
                const BankTick& tk = ba.ticks[i];
                const float rin = tk.major ? radius - 12.0f : radius - 7.0f;
                const Vector2 p0{pivot.x + static_cast<float>(tk.dir.x) * rin,
                                 pivot.y + static_cast<float>(tk.dir.y) * rin};
                const Vector2 p1{
                    pivot.x + static_cast<float>(tk.dir.x) * radius,
                    pivot.y + static_cast<float>(tk.dir.y) * radius};
                DrawLineEx(p0, p1, tk.major ? 2.0f : 1.5f, ac);
            }
            // The moving pointer (a caret from the arc inward toward the
            // pivot).
            const Vector2 tip{pivot.x + static_cast<float>(ba.pointer_dir.x) *
                                            (radius + 2.0f),
                              pivot.y + static_cast<float>(ba.pointer_dir.y) *
                                            (radius + 2.0f)};
            const Vector2 base{pivot.x + static_cast<float>(ba.pointer_dir.x) *
                                             (radius - 16.0f),
                               pivot.y + static_cast<float>(ba.pointer_dir.y) *
                                             (radius - 16.0f)};
            DrawLineEx(base, tip, 3.0f, pc);
        }

        // S-carets (comfort program, REC-2): screen-edge threat indicators for
        // OFF-SCREEN drones/bandits — a small peripheral caret pointing where
        // to TURN to face the threat, so multi-bogey awareness needs no
        // disorienting freelook excursion. Pure HUD read of the SAME drone data
        // the gunsight uses (info.drones_draw + pose.eye); no new state. alpha
        // 0
        // => the whole pass is SKIPPED (strict superset). Drawn UNDER the
        // reticle layer (peripheral backdrop, never at the reticle — S-retclamp
        // lesson: a cue where the pilot AIMS is in the loop).
        if (info.cue_caret_alpha > 0.0 && !info.drones_draw.empty()) {
            const unsigned char a = static_cast<unsigned char>(
                std::clamp(info.cue_caret_alpha, 0.0, 1.0) * 255.0);
            const Color cc{240, 120, 90,
                           a};  // threat tint (low-sat red-orange)
            constexpr double kEdgeMarginNdc = 0.06;  // inset from the very edge
            constexpr double kCaretHystNdc = 0.05;   // P1-3 anti-strobe band
            // P1-3: per-fleet-slot caret shown-state, so the on_screen boundary
            // is HYSTERETIC (the gunsight target-pick precedent — every
            // instrument-selection gate is hysteretic). This is display-only
            // cosmetic state (like the planet/sun statics above); it feeds no
            // control/sim/aim path. Keyed by drones_draw index; sized
            // generously and reset if the fleet count ever exceeds it.
            constexpr int kMaxCaretSlots = 64;
            static std::array<bool, kMaxCaretSlots> s_caret_shown{};
            // P1-2: skip the engaged target ONLY when its pipper actually drew
            // — engaged AND the lead projects in front (the pipper only draws
            // in_front). An engaged bandit BEHIND you draws NO pipper, so it
            // must still get a caret (the worst SA hole). Identity skip by
            // index, not a lead-cone angle match.
            const bool pipper_drew =
                info.gunsight_active && info.gunsight_has_target &&
                project_dir(info.gunsight_lead, cam_forward, pose.up, fovy_rad,
                            aspect)
                    .in_front;
            const int engaged = pipper_drew ? info.gunsight_target_index : -1;
            const int nd = static_cast<int>(info.drones_draw.size());
            for (int di = 0; di < nd; ++di) {
                if (di == engaged) continue;  // its pipper diamond covers it
                const sim::SimState& dstate = info.drones_draw[di];
                const glm::dvec3 to = dstate.position - pose.eye;
                const double rng = glm::length(to);
                if (!(rng > 1e-6)) continue;
                const glm::dvec3 dir = to / rng;
                const bool prev_shown =
                    di < kMaxCaretSlots ? s_caret_shown[di] : false;
                const EdgeCaret ec =
                    edge_caret(dir, cam_forward, pose.up, fovy_rad, aspect,
                               info.lens_shift_ndc, kEdgeMarginNdc, prev_shown,
                               kCaretHystNdc);
                if (di < kMaxCaretSlots) s_caret_shown[di] = !ec.on_screen;
                if (ec.on_screen) continue;  // visible in view: no caret needed
                // Nearer = bigger (faint 1/r scale, clamped): a 1500 m
                // reference reads full size, clamped to [0.6, 1.6] so far/near
                // stay legible.
                const double sc = std::clamp(1500.0 / rng, 0.6, 1.6);
                const float len = static_cast<float>(16.0 * sc);  // px, tip len
                const float wid =
                    static_cast<float>(9.0 * sc);  // px, base half
                const ScreenPoint ep{ec.edge.x, ec.edge.y, true};
                const Vector2 base_px = to_pxf(ep);
                // Pointing direction: ec.angle is DISPLAY-NDC (+y up); pixel y
                // is DOWN, so negate the y-component for the on-screen vector.
                const double ca = std::cos(ec.angle);
                const double sa = std::sin(ec.angle);
                const Vector2 pdir{static_cast<float>(ca),
                                   static_cast<float>(-sa)};
                const Vector2 perp{-pdir.y, pdir.x};
                // A filled triangle: tip along pdir, base straddling perp.
                const Vector2 tip{base_px.x + pdir.x * len,
                                  base_px.y + pdir.y * len};
                Vector2 bl{base_px.x - perp.x * wid, base_px.y - perp.y * wid};
                Vector2 br{base_px.x + perp.x * wid, base_px.y + perp.y * wid};
                // raylib backface-culls: keep the SAME screen winding as the
                // working AoA triangle above (signed area < 0 in +y-down px
                // coords) regardless of which way the caret points.
                const float area = (bl.x - tip.x) * (br.y - tip.y) -
                                   (bl.y - tip.y) * (br.x - tip.x);
                if (area > 0.0f) std::swap(bl, br);
                DrawTriangle(tip, bl, br, cc);
            }
        }

        const glm::dvec3 nose = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const ScreenPoint ret = project_dir(info.reticle_dir, cam_forward,
                                            pose.up, fovy_rad, aspect);
        const ScreenPoint nos =
            project_dir(nose, cam_forward, pose.up, fovy_rad, aspect);

        if (nos.in_front) {
            DrawCircleV(to_pxf(nos), 4.0f,
                        Color{120, 230, 120, 220});  // nose marker
        }
        if (ret.in_front) {
            const Vector2 rc = to_pxf(ret);
            DrawCircleLinesV(rc, 9.0f, RAYWHITE);  // aim reticle
            // Freelook: the reticle nests in a cursor ring — the "aim locked"
            // cue (SPEC §9.2). The held aim sits inside the mouse cursor.
            if (info.freelook) {
                DrawCircleLinesV(rc, 20.0f, Color{255, 220, 120, 200});
            }
        }
        // v5 OFF-SCREEN AIM ARROW (Chad 2026-07-23: "give me a little red
        // arrow to show where the mouse aim is when it's temporarily off
        // screen"): when the TRUE aim is off screen (behind the camera, or
        // in front but outside the visible box), a red edge arrow points
        // toward it. PURE DISPLAY ADDITION — the reticle is never moved,
        // clamped, or substituted (the S-retclamp lesson stands: a pinned
        // edge MARKER replacing the reticle under-reports the error; an
        // arrow ADDS the missing read while the true reticle stays raw).
        // Direction from the SHARED camera screen basis (never a re-derived
        // projection — the projection-basis-fork lesson).
        {
            const bool off_screen =
                !ret.in_front || std::abs(ret.x) > 1.0 ||
                std::abs(ret.y - info.lens_shift_ndc) > 1.0;
            const ScreenBasis sb = camera_screen_basis(cam_forward, pose.up);
            if (off_screen && sb.ok) {
                const glm::dvec3 aim_n = glm::normalize(info.reticle_dir);
                const double dx = glm::dot(aim_n, sb.r);
                const double dy = glm::dot(aim_n, sb.u);
                const double dlen = std::sqrt(dx * dx + dy * dy);
                if (dlen > 1e-6) {
                    // Screen-space direction (y down) from center toward the
                    // aim; anchor the arrow on the screen rect inset by a
                    // margin, tip outward.
                    const double ux = dx / dlen, uy = -dy / dlen;
                    const double cx = sw * 0.5, cy = sh * 0.5;
                    const double margin = 26.0;
                    double t = 1e18;
                    if (std::abs(ux) > 1e-9)
                        t = std::min(t, (sw * 0.5 - margin) / std::abs(ux));
                    if (std::abs(uy) > 1e-9)
                        t = std::min(t, (sh * 0.5 - margin) / std::abs(uy));
                    const float ax = static_cast<float>(cx + ux * t);
                    const float ay = static_cast<float>(cy + uy * t);
                    const float px = static_cast<float>(-uy);  // perp
                    const float py = static_cast<float>(ux);
                    const Color red{230, 55, 45, 235};
                    const Vector2 tip{ax + static_cast<float>(ux) * 14.0f,
                                      ay + static_cast<float>(uy) * 14.0f};
                    Vector2 bl{ax + px * 8.0f, ay + py * 8.0f};
                    Vector2 br{ax - px * 8.0f, ay - py * 8.0f};
                    // raylib front-face winding guard (same pattern as the
                    // bandit edge markers above).
                    const float area = (br.x - tip.x) * (bl.y - tip.y) -
                                       (bl.x - tip.x) * (br.y - tip.y);
                    Vector2 b1 = bl, b2 = br;
                    if (area > 0.0f) std::swap(b1, b2);
                    DrawTriangle(tip, b1, b2, red);
                }
            }
        }

        // Lead-angle pipper (S8-drone): the meter's snapshot (computed once per
        // sim tick in app::tick) says WHERE to aim for a hit on the engaged
        // bandit; here we only PROJECT that world direction and draw it. Green
        // when the nose is on the solution and in range, amber otherwise.
        if (info.gunsight_active && info.gunsight_has_target) {
            const ScreenPoint lp = project_dir(info.gunsight_lead, cam_forward,
                                               pose.up, fovy_rad, aspect);
            if (lp.in_front) {
                const Vector2 lc = to_pxf(lp);  // float path (was int-cast;
                                                // it drew float lines anyway)
                const Color col = info.gunsight_on_target
                                      ? Color{80, 240, 120, 255}
                                      : Color{240, 180, 60, 220};
                const float fx = lc.x;
                const float fy = lc.y;
                constexpr float d = 8.0f;  // diamond half-extent
                DrawLineEx({fx - d, fy}, {fx, fy - d}, 2.0f, col);
                DrawLineEx({fx, fy - d}, {fx + d, fy}, 2.0f, col);
                DrawLineEx({fx + d, fy}, {fx, fy + d}, 2.0f, col);
                DrawLineEx({fx, fy + d}, {fx - d, fy}, 2.0f, col);
            }
        }
    }

    // Time-on-target readout (S8-drone Stage 3): the tracking number. Only when
    // the gunsight is active; shows the lifetime on-target %, the engaged
    // range, and a live ON TARGET cue when the nose is on the solution.
    if (info.gunsight_active && info.gunsight_has_target) {
        char tline[96];
        std::snprintf(tline, sizeof tline, "TOT %3.0f%%   RNG %5.0f m   %s",
                      info.tot_frac * 100.0, info.tot_range,
                      info.gunsight_on_target ? "* ON TARGET *" : "");
        DrawText(tline, 12, 36, 18,
                 info.gunsight_on_target ? Color{90, 240, 130, 255}
                                         : Color{230, 220, 160, 220});
    }

    // MB HUD: the attitude dial (pitch needle + roll arc + AoA strip), the
    // energy cluster, and the status stack (telemetry + flaps/gear) — all
    // read-only, through the ONE shared readout `r` computed above (never a
    // re-derived AoA/G/bank — the flat-instrument rule).
    draw_attitude_dial(r, info, GetScreenWidth(), GetScreenHeight());
    draw_energy_cluster(r, info, GetScreenHeight());
    draw_status_stack(r, state, params, info, GetScreenHeight());

    draw_bezel(GetScreenWidth(), GetScreenHeight());
    EndDrawing();
}

}  // namespace render
