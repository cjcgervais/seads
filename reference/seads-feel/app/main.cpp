// SEADS app shell (SPEC §5): the fixed-timestep accumulator loop wiring
// input -> instructor -> sim -> render. This is the Section-5 landing of the
// deferred app-wiring: the pure instructor (control/, Section 4) now flies the
// LIVE loop, mouse->aim on the §9.1 raw aim frame behind the §9.2 frame-carried
// camera. The per-tick sequence mirrors the tested harness ClosedLoop::tick()
// (test/harness/instructor.h) EXACTLY — transport, GROUNDED pairing, the shared
// input::Freelook, control::step, sim::step — so the live path runs the same
// pinned code the goldens replay (CLAUDE.md: route the live path THROUGH the
// tested code). Render reads state and never writes; the sim never sees a
// keycode or frame time.
//
// F1 toggles RAW mode (SPEC §5 debug): device -> Inputs directly, instructor
// off, the Section-3 local_up camera. Instructor mode is the default.
//
// Usage:
//   seads.exe                     fly (mouse-aim instructor)
//   seads.exe --smoke N [shot]    run N fixed frames hands-off (level
//   instructor
//                                 flight), optionally TakeScreenshot(shot).

#include <algorithm>
#include <cctype>   // std::isspace -- env dial validation (sled first build)
#include <chrono>  // smoke-only per-frame timing readout (perf measurement)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <random>  // [seasons] W1: app-local RNG for the per-spawn season draw
#include <string>  // T25 SEADS_PROF spike-line assembly

#include "app/build_info.gen.h"  // GENERATED per build (cmake/build_info.cmake)
#include "app/conquest_tape.h"  // CONQUEST TAPE: seads_tape::ConquestTape (AI primary-data recorder)
#include "app/feel_tape.h"
#include "app/instructor_tick.h"
#include "app/interact.h"  // ★ L1: the diegetic reach law (sites + prompts)
#include "app/loop.h"
#include "app/player_mode.h"   // ★ L1: the OUTER mode machine (Pilot/Sled/Afoot/...)
#include "app/player_mount.h"  // ★ L1: THE mount seam -- grip + man, one place
#include "app/spawn_menu.h"    // ★ L3: the spawn overlay (the one screen)
#include "app/spawn_policy.h"  // ★ L3: player_spawn -- the ONE place a player is born
#include "app/flash_cam.h"  // ★ ROAD-REPAIR E2: the FLASH INSTRUMENT camera
#include "app/flak_walkup.h"  // ★ L5: the gun's approach mark, one copy
#include "app/walker_place.h"  // ★ L1: putting the man down (stub until R4e)
#include "combat/pump_repair.h"  // ★ L2: the wrench -- the SECOND writer of pump hp
#include "combat/engine_repair.h"  // ★ L10: the same wrench, on the engine
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "config/load_world.h"
#include "drone/drone.h"
#include "input/aim_curve.h"
#include "input/live_input.h"
#include "input/raw_input.h"
#include "raylib.h"
#include "render/bagpipe_throttle.h"  // throttle voice: bagpipe drone, pitch ∝ throttle
#include "render/buildings.h"  // R4f: building_colliders (env.obstacles)
#include "render/camera.h"
#include "render/celestial.h"
#include "render/draw.h"
#include "app/flak_ai.h"        // FLAK STAGE D: the AI gunners
#include "render/flak_gunner.h"  // F-POSE: gunner enable
#include "render/sled_model.h"   // F-POSE: sled_rider_hide_set while manned
#include "render/flak_model.h"  // FLAK: FlakDraw + flak_site placement
#include "render/gun_synth.h"
#include "render/interp.h"
#include "render/intro_sequence.h"  // splash cue timeline (pure)
#include "render/bubble_map.h"  // ★ L4: MapView + the map zoom/follow law
#include "render/map_style.h"  // M-KEY chart look/declutter dials (S-mapread)
#include "render/music_director.h"  // MUSIC bus: bed select + blast duck (pure)
#include "render/planet.h"  // R3 sweep seed: r3_full_depth_env_override + SnowParams ship value
#include "render/post.h"
#include "render/probe.h"
#include "render/pump_ambience.h"  // the wolf / wildcat cries at the surface pumps (pure)
#include "render/bell_ambience.h"  // the church bell, 1-12 chimes, two minutes behind the train (pure)
#include "render/pump_frame.h"  // S-pumpcube: the neon team wire cube dials
#include "render/readout.h"
#include "render/road_census.h"  // ★ ROAD-REPAIR: the road-gap census instrument
#include "render/ribbon_clip.h"  // ★ ROAD-REPAIR: the T24 cut clip the drape takes
#include "render/ribbon_subdiv.h"  // ★ ROAD-REPAIR: which baked rungs are really drawn
#include "render/rider_pose.h"  // R5 row 9: ski_steer_max_rad + sled_sag0_m (single sources)
#include "render/rig.h"  // R4 wheel-spin: the rig's own wheel radius (single source)
#include "render/sled_model.h"  // R5 row 6: sled_model_rider_back (breath anchor)
#include "render/sled_plumes.h"  // R5 rows 4/5/6: roost/exhaust/breath trails
#include "render/sample_voice.h"  // the stope blast's reverb send (one-shot)
#include "render/sled_synth.h"  // the snowmachine engine: idle bed + brap + tone
#include "render/sting_deploy.h"  // ★ ST-5: the seat-deploy blend's pure laws
#include "render/sting_audio.h"  // ★ ST-5 polish: the Sting's prop-spin + motor-buzz laws
#include "render/stope_reverb.h"  // the stope echo: "make all sounds ... echo"
#include "render/sudbury_gis.gen.h"  // R4k touchdown FX: kSudburyLakes (the ONE lake list)
#include "render/team_color.h"  // S-mapteam: the ONE faction palette
#include "render/team_kit.h"    // L11: the chosen side's four colours
#include "render/town_ambience.h"  // the distant Chelmsford train, gated on town density (pure)
#include "render/tunnel_lamp_hits.h"  // T5c: TunnelLampWorld (destructible)
#include "render/tunnel_mesh.h"  // T2: render::kMouthCutFactor (portal cut radius)
#include "render/vortex.h"
#include "render/snowfall.h"  // AS-2: the snowfall field (beside the haze)
#include "render/weather.h"
#include "render/wind_audio.h"
#include "render/wind_synth.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "sim/sled.h"  // S3: the sled kernel (INV-7, its own seal)
#include "sim/walker.h"  // ★ R4c §7.3 stages 4-7: the man, once he is off
#include "sim/gait.h"    // ★★★ GAIT LADDER G1: his feet, off the same walker
#include "sim/state.h"
#include "sim/world.h"
#include "test/harness/recorder.h"  // F9 RECORD: seads_replay::Recorder (app-includable)
#include "test/harness/sled_tape.h"  // ★ SLED TAPE R1: seads_sledtape::Writer (auto-on with drive mode)
#include "weapon/ballistics.h"
#include "world/faction_bubbles.h"  // ROAD-REPAIR census: the two pump surface anchors
#include "world/cold.h"        // SC1: COLD IS POWER (WINTER_LAW §3.7)
#include "world/snowpack.h"    // S2: the analytic snowpack + surface classes
#include "world/tracks.h"      // SF3-B: the driven-surface deformation
#include "world/tunnel_geo.h"  // T2: the mouth dirs for the portal cut disks
#include "world/tunnel_net.h"  // T1/T2: TunnelParams + build_tunnel_net

// Discrete-GPU export hints (perf): on a hybrid-GPU Windows laptop, the OS
// driver picks the GPU for a process by inspecting these two symbols in the
// exe's export table BEFORE any GL context exists — without them a hybrid
// system may silently run the whole render on the weaker integrated GPU.
// MinGW GCC honors __declspec(dllexport) identically to MSVC here, so this
// is toolchain-portable (CLAUDE.md: MinGW GCC + Ninja is the pinned
// toolchain). Must be extern "C" (no name mangling) and at file scope so the
// linker actually exports them by their exact NVIDIA/AMD-documented names.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

// One hitch clamp for the whole frame path: the accumulator caps sim time
// with it, and the device throttle sweep uses the SAME cap so device
// wall-time can never outrun sim time across a debugger pause.
constexpr double kMaxFrameDt = 0.25;

// Target-visibility probe readback (docs/world_build_plan.md §4). Caller glue:
// projects the probe bandit to a pixel with the SAME project_dir + lens-shift
// the reticle uses (so we sample exactly where it drew), reads back the
// framebuffer, and folds a small plane patch + a background ring through the
// PURE render::probe_contrast oracle. Prints one CSV row to stdout and, if
// given, appends it (with a header on first write) to `csv`. No ctest exercises
// this (no ctest runs seads.exe) — the oracle it calls IS gate-pinned.
void run_probe_measurement(const render::CameraPose& pose, double fovy_deg,
                           double lens_shift_ndc,
                           const render::ProbePlacement& place,
                           render::ProbeGeometry geom,
                           const glm::dvec3& level_forward, double alt,
                           double range, double season, double day,
                           const char* csv) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const double fovy_rad = fovy_deg * 3.14159265358979323846 / 180.0;
    const double aspect = static_cast<double>(sw) / static_cast<double>(sh);
    const glm::dvec3 cam_forward = glm::normalize(pose.target - pose.eye);
    const glm::dvec3 tdir = glm::normalize(place.target - pose.eye);
    const render::ScreenPoint sp =
        render::project_dir(tdir, cam_forward, pose.up, fovy_rad, aspect);
    const bool is_above = geom == render::ProbeGeometry::Above;
    // Would a LEVEL-flying player see the bandit? The probe camera is pointed
    // AT the target (always on-screen), so this preserves the real gameplay
    // fact the re-point otherwise erases: a high-alt below-horizon bandit is
    // off the bottom of a level player's frame (P2). On-screen = in front AND
    // within the lens-shifted NDC box the scene actually drew.
    const render::ScreenPoint lsp =
        render::project_dir(tdir, level_forward, pose.up, fovy_rad, aspect);
    const bool level_onscreen = lsp.in_front && std::fabs(lsp.x) < 1.0 &&
                                std::fabs(lsp.y - lens_shift_ndc) < 1.0;

    // Size the plane box to the bandit's PROJECTED extent so the far range
    // cells aren't all background (P1-1); the peak stat below then catches even
    // a sub-pixel bright speck. The astern airframe is ~2.5 m across (fuselage
    // + fin); ppr = pixels per radian (vertical).
    const double ppr = static_cast<double>(sh) / fovy_rad;
    constexpr double kProbeModelHalfM = 1.25;
    const int box_half = std::clamp(
        static_cast<int>(std::lround(kProbeModelHalfM / range * ppr)), 1, 8);
    const int ring_in = box_half + 8;  // annulus clears the plane
    const int ring_out = ring_in + 12;

    render::PatchStats plane, bg;
    render::PixelCoord ctr{-1, -1};
    if (sp.in_front) {
        ctr = render::ndc_to_pixel(sp.x, sp.y, lens_shift_ndc, sw, sh);
        Image img = LoadImageFromScreen();
        for (int dy = -ring_out; dy <= ring_out; ++dy) {
            for (int dx = -ring_out; dx <= ring_out; ++dx) {
                const int x = ctr.x + dx, y = ctr.y + dy;
                if (x < 0 || x >= sw || y < 0 || y >= sh) continue;
                const Color c = GetImageColor(img, x, y);
                const double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
                if (dx >= -box_half && dx <= box_half && dy >= -box_half &&
                    dy <= box_half) {
                    plane.add(r, g, b);
                } else {
                    const double rr = std::sqrt(static_cast<double>(dx) * dx +
                                                static_cast<double>(dy) * dy);
                    if (rr >= ring_in && rr <= ring_out) bg.add(r, g, b);
                }
            }
        }
        UnloadImage(img);
    }
    const render::ProbeContrast c = render::probe_contrast(plane, bg);

    if (csv != nullptr) {
        const bool exists = [csv]() {
            FILE* f = std::fopen(csv, "r");
            if (f) {
                std::fclose(f);
                return true;
            }
            return false;
        }();
        FILE* f = std::fopen(csv, "a");
        if (f) {
            if (!exists)
                std::fprintf(
                    f,
                    "alt_m,range_m,season,day,geom,onscreen,"
                    "level_onscreen,below_horizon,above_terrain,"
                    "clear_of_terrain,px,py,box_half,n_plane,n_bg,"
                    "lum_plane,lum_bg,lum_michelson,lum_peak_michelson,"
                    "chroma_plane,chroma_bg,chroma_delta,chroma_peak\n");
            std::fprintf(f,
                         "%.0f,%.0f,%.3f,%.3f,%s,%d,%d,%d,%d,%d,%d,%d,%d,%lld,"
                         "%lld,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n",
                         alt, range, season, day, is_above ? "above" : "below",
                         sp.in_front ? 1 : 0, level_onscreen ? 1 : 0,
                         place.below_horizon ? 1 : 0,
                         place.above_terrain ? 1 : 0,
                         place.clear_of_terrain ? 1 : 0, ctr.x, ctr.y, box_half,
                         plane.n, bg.n, c.lum_plane, c.lum_bg, c.lum_michelson,
                         c.lum_peak_michelson, c.chroma_plane, c.chroma_bg,
                         c.chroma_delta, c.chroma_peak);
            std::fclose(f);
        }
    }
    std::printf(
        "[probe] alt=%.0fm range=%.0fm geom=%s season=%.2f day=%.2f "
        "onscreen=%d "
        "level_onscreen=%d below_horizon=%d above_terrain=%d "
        "clear_of_terrain=%d "
        "px=(%d,%d) box=%d n_plane=%lld n_bg=%lld\n"
        "        LUM plane=%.3f bg=%.3f michelson=%.3f peak=%.3f\n"
        "        CHROMA plane=%.3f bg=%.3f delta=%.3f peak=%.3f\n",
        alt, range, is_above ? "above" : "below", season, day,
        sp.in_front ? 1 : 0, level_onscreen ? 1 : 0,
        place.below_horizon ? 1 : 0, place.above_terrain ? 1 : 0,
        place.clear_of_terrain ? 1 : 0, ctr.x, ctr.y, box_half, plane.n, bg.n,
        c.lum_plane, c.lum_bg, c.lum_michelson, c.lum_peak_michelson,
        c.chroma_plane, c.chroma_bg, c.chroma_delta, c.chroma_peak);
    // Validity is geometry-specific — never let an invalid row read as data:
    //  Below = ray reaches GUARANTEED ground AND target above the relief top;
    //  Above = eye above the relief top (upward ray clears near terrain) AND
    //          target above the relief top (it is up in the sky).
    const bool valid = is_above
                           ? (place.clear_of_terrain && place.above_terrain)
                           : (place.below_horizon && place.above_terrain);
    if (!valid)
        std::printf(
            "        WARNING: %s-geom target invalid at alt %.0f m / range "
            "%.0f m (below_horizon=%d above_terrain=%d clear_of_terrain=%d) — "
            "pick a %s.\n",
            is_above ? "above" : "below", alt, range,
            place.below_horizon ? 1 : 0, place.above_terrain ? 1 : 0,
            place.clear_of_terrain ? 1 : 0,
            is_above ? "higher alt (eye above relief)"
                     : "higher alt or nearer "
                       "range");
    if (!sp.in_front || plane.n == 0 || bg.n == 0)
        std::printf(
            "        WARNING: sample patch empty/clipped (onscreen=%d "
            "n_plane=%lld n_bg=%lld) — target off-screen; contrast is NOT "
            "measured (not zero-contrast).\n",
            sp.in_front ? 1 : 0, plane.n, bg.n);
}

}  // namespace

namespace {
// T25 — THE STUTTER ATTRIBUTION INSTRUMENT (fly round-16 tail, Chad: "I had
// stuttering a couple of times in the tunnel and also while shooting in the
// sky"; the T16 lesson says MEASURE FIRST — the round-12 "choppiness" was an
// SDF scan, not render). Opt-in via SEADS_PROF=1: the app (which owns the
// clock — render/ reads none) records a lap per named draw pass through
// FrameInfo::prof_mark, plus an "app_tick" lap for everything before the
// draw (input + sim ticks + combat + audio). When a frame's total exceeds
// max(2x the rolling 120-frame median, 20 ms), ONE line prints with the
// worst laps — fly, stutter, then read which bucket owned the spike. Off
// (the default): zero overhead, prof_mark stays nullptr.
struct FrameProf {
    bool on = false;
    std::chrono::steady_clock::time_point last{};
    std::vector<std::pair<const char*, double>> lap;  // bucket -> ms
    std::vector<double> ring;                         // recent frame totals
    long frame = 0;
    // Sub-lap attribution (this fix): the caller sets this to the frame's
    // fixed-tick count (app::FrameResult::ticks) before end() prints, so a
    // spike line shows catch-up multiplication alongside the lap split.
    int ticks = 0;
    void begin() {
        lap.clear();
        last = std::chrono::steady_clock::now();
    }
    void mark(const char* n) {
        const auto now = std::chrono::steady_clock::now();
        const double ms =
            std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        for (auto& b : lap)
            if (b.first == n || std::strcmp(b.first, n) == 0) {
                b.second += ms;
                return;
            }
        lap.emplace_back(n, ms);
    }
    // Per-bucket RUNNING MEAN over the whole run. The spike line above answers
    // "what owned that hitch"; it cannot answer "what does this pass COST every
    // frame", which is the question a perf budget is written against. A whole-
    // frame A/B (run with and against SEADS_NO_PRECIP and subtract) was tried
    // first and is USELESS here: run-to-run frame-time variance on this box is
    // +/- 0.5 ms, larger than the pass being measured. So the laps are summed.
    std::vector<std::pair<const char*, double>> sum;
    long sum_frames = 0;
    void accumulate() {
        // Skip the first 60 frames: the lazy renderers (ribbons, buildings,
        // trees, BOTH precip lattices) build their meshes on the frame they are
        // first drawn, and a build cost folded into a steady-state mean is a
        // number that lies. The smoke timing readout warms up the same way.
        if (frame <= 60) return;
        ++sum_frames;
        for (const auto& b : lap) {
            bool hit = false;
            for (auto& t : sum)
                if (std::strcmp(t.first, b.first) == 0) {
                    t.second += b.second;
                    hit = true;
                    break;
                }
            if (!hit) sum.emplace_back(b.first, b.second);
        }
    }
    void report() const {
        if (sum_frames <= 0) return;
        for (const auto& t : sum)
            std::printf("[PROF_MEAN] %-14s %.3f ms/frame over %ld frames\n",
                        t.first, t.second / sum_frames, sum_frames);
    }
    void end() {
        ++frame;
        accumulate();
        double total = 0.0;
        for (const auto& b : lap) total += b.second;
        std::vector<double> sorted = ring;  // median of RECENT frames
        double median = 8.0;
        if (sorted.size() >= 30) {
            std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2,
                             sorted.end());
            median = sorted[sorted.size() / 2];
        }
        if (ring.size() >= 120) ring.erase(ring.begin());
        ring.push_back(total);
        if (total > std::max(2.0 * median, 20.0)) {
            std::sort(lap.begin(), lap.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            std::string tops;
            for (std::size_t i = 0; i < lap.size() && i < 4; ++i) {
                char buf[64];
                std::snprintf(buf, sizeof buf, " %s %.1f", lap[i].first,
                              lap[i].second);
                tops += buf;
            }
            std::printf(
                "[PROF] frame %ld SPIKE %.1f ms (median %.1f) ticks=%d |%s\n",
                frame, total, median, ticks, tops.c_str());
        }
    }
};
FrameProf g_prof;
void prof_mark_thunk(const char* n) { g_prof.mark(n); }

// F9 RECORD: felt_flight_<n>.seadsrec, incrementing n until the name is free
// (never overwrite a prior capture). fopen-existence check, no <filesystem>
// dependency added to this tree for one helper.
std::string next_felt_flight_name() {
    for (int n = 1;; ++n) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "felt_flight_%d.seadsrec", n);
        std::FILE* f = std::fopen(buf, "rb");
        if (f == nullptr) return std::string(buf);
        std::fclose(f);
    }
}

// The step_frame per-tick hook (app/instructor_tick.h TickHook): a plain free
// function wrapping seads_replay::Recorder::on_tick, since TickHook is a raw
// function pointer + void* ctx (not a std::function) — kept dependency-free
// for the header's app-includable contract.
void felt_recorder_hook(const app::TickInput& in, const app::LoopState& st,
                        const control::Telemetry& telem, void* ctx) {
    static_cast<seads_replay::Recorder*>(ctx)->on_tick(in, st, telem);
}

// ===========================================================================
// S-lapguard FEEL TAPE (2026-09-12). Chad flew the scripted-clean build and
// still got the rollover, so the scripted sweeps stop being the only grader:
// this records HIS OWN HAND, per sim tick, so the instrument can replay the
// real input instead of a synthetic one.
//
//   PowerShell, one line:
//     $env:SEADS_FEEL_TAPE="D:\flight_sim2\seads-feel\build-play\feel_tape_loop.csv"; D:\flight_sim2\seads-feel\build-play\seads.exe
//
// Pull the loop, quit. Replay with the lane instrument (test_loop_rollover's
// "S-lapguard: replay a recorded feel tape" case reads SEADS_FEEL_TAPE and
// prints the roll_out trace + the roll limbs at right_hand_rest 0 and 0.25).
//
// APP-SIDE ONLY: it is a TickHook (the existing step_frame per-tick seam), it
// only READS LoopState/Telemetry, and with the env var unset the hook pointer
// stays null so the cost is exactly zero -- the kernel is untouched.
// The ONE source for "which dials did I just fly?". Built once at load and
// then emitted THREE ways: stderr (terminal launches), <exe_dir>/
// seads_launch.log (Explorer launches -- Chad: "it just opens the game no
// config lines"), and the feel tape's own header. One string, so a flight can
// never be attributed to dials it did not fly.
std::string g_config_banner;
std::string g_exe_dir;

// <exe_dir>/seads_launch.log. APPENDED, with a separator and a wall clock, not
// overwritten: across a fly session Chad launches several times, and the
// question is always "which of those launches was the one that rolled" -- an
// overwriting log answers it only for the last.
void write_launch_log() {
    if (g_exe_dir.empty()) return;
    const std::string path = g_exe_dir + "/seads_launch.log";
    std::FILE* f = std::fopen(path.c_str(), "ab");
    if (f == nullptr) return;
    std::time_t now = std::time(nullptr);
    char when[64] = "";
    std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S",
                  std::localtime(&now));
    std::fprintf(f, "\n===== SEADS launch %s =====\n", when);
    std::fprintf(f, "[exe] %s\n", g_exe_dir.c_str());
    std::fprintf(f, "[build] %s %s\n", __DATE__, __TIME__);
    std::fputs(g_config_banner.c_str(), f);
    std::fclose(f);
}


// One process-wide tape (the app has one aeroplane). Null file => the hook is
// never installed => zero cost.
app::FeelTapeCtx g_feel_tape;

// CONQUEST TAPE (docs/conquest_tape_spec.md §4): conquest_tape_<n>.jsonl,
// incrementing n until the name is free — the SAME next-free-index scan
// pattern as next_felt_flight_name above (never overwrite a prior capture).
std::string next_conquest_tape_name() {
    for (int n = 1;; ++n) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "conquest_tape_%d.jsonl", n);
        std::FILE* f = std::fopen(buf, "rb");
        if (f == nullptr) return std::string(buf);
        std::fclose(f);
    }
}

// ★ SLED TAPE R1: sled_tape_<n>.sledtape — the SAME next-free-index scan as
// the two recorders above (never overwrite a prior capture).
std::string next_sled_tape_name() {
    for (int n = 1;; ++n) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "sled_tape_%d.sledtape", n);
        std::FILE* f = std::fopen(buf, "rb");
        if (f == nullptr) return std::string(buf);
        std::fclose(f);
    }
}

// The app::ConquestTapeHook wrapper (spec §4): mirrors felt_recorder_hook's
// shape exactly — a free function wrapping seads_tape::ConquestTape::on_tick
// via ctx, since ConquestTapeHook is also a raw function pointer + void*.
void conquest_tape_hook(const app::TickInput& in, const app::LoopState& st,
                        const app::DroneWorld* dw,
                        const combat::CombatWorld* cw,
                        const app::ConquestWorld* cq,
                        const sim::Environment* env, void* ctx) {
    static_cast<seads_tape::ConquestTape*>(ctx)->on_tick(in, st, dw, cw, cq,
                                                         env);
}
// ★ ROAD-REPAIR ONAPING RUNG 1 -- ONE PASTE, NOT SIX.
//
// Chad's priority defect is at the Valley pump on Onaping ("the roads are bad
// there and some drop offs coming off the road you dont see"), and getting
// there was a five-variable block copied out of docs/road_repair/
// CENSUS_FINAL.md §1 that is easy to get half-right. `SEADS_ONAPING_SMOKE=1`
// is that block, named: it FILLS IN each of the five environment variables the
// existing readers already look for, and fills in NONE that the caller set
// itself -- so every individual override still wins and nothing downstream
// learns a new name. No new spawn logic exists anywhere; this is an env
// expansion and that is all it is.
//
// ⚠ The dir is the CENSUS_FINAL §1 rung-1 station: a road_major beside the
// Valley pump pond, full bank, no junction within 2.5 km.
bool onaping_smoke_armed() {
    const char* v = std::getenv("SEADS_ONAPING_SMOKE");
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

void apply_onaping_smoke_env() {
    if (!onaping_smoke_armed()) return;
    static const char* const kPairs[][2] = {
        {"SEADS_SMOKE_SPAWN_DIR",
         "-0.964529115,0.218194969,0.148575039"},
        {"SEADS_SPAWN_ALT", "25"},
        {"SEADS_SLEDCAM", "2.2,35,12"},
        {"SEADS_SLED_RIG_SMOKE", "0,0,0,0,0,0,0,0,0,0"},
        {"SEADS_SLED_DEBUG_MODE", "1"},
    };
    for (const auto& kv : kPairs) {
        if (std::getenv(kv[0]) != nullptr) continue;  // his override wins
#ifdef _WIN32
        ::_putenv_s(kv[0], kv[1]);
#else
        ::setenv(kv[0], kv[1], 0);
#endif
    }
    std::printf(
        "ONAPING SMOKE: spawn dir/alt/sledcam/rig/sled-debug filled in "
        "(CENSUS_FINAL.md §1 rung 1). Any of the five set by hand wins.\n");
}

// ★ ROAD-REPAIR E2 -- THE FLASH INSTRUMENT (docs/road_repair/onaping_flash_E2.md).
//
// `SEADS_FLASH_SMOKE=<site>` pins a GRAZING camera on a census junction node
// and renders SEADS_FLASH_FRAMES frames of it with the eye walked
// SEADS_FLASH_JITTER_M metres along the view each frame.  Nothing in the world
// moves (the frame dt is forced to 0 in the loop below), so every pixel that
// CHANGES between those frames changed because a depth tie was broken the
// other way -- the flicker docs/road_repair/onaping_eyesores.md §3.4 could
// not capture and the F3/F2 rungs must be graded on.
//
// It BUILDS NOTHING and MOVES NO GEOMETRY. The one dial it can touch is the
// deliberate POSITIVE CONTROL, `SEADS_FLASH_POSCTL` (render/ribbons.cpp), which
// scales the road deck's polygon offset and defaults to 1.0 = the shipped
// number = OFF.
bool flash_smoke_armed() {
    const char* v = std::getenv("SEADS_FLASH_SMOKE");
    return v != nullptr && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

double flash_env_d(const char* name, double dflt) {
    const char* v = std::getenv(name);
    if (v == nullptr || v[0] == '\0') return dflt;
    return std::atof(v);
}

// The player body must never be IN the frame: a plane crossing the shot would
// be counted as flash. So the flash rig spawns it at the ANTIPODE of the site
// -- half a planet (47 km) away, far over a R = 15 km horizon -- through the
// EXISTING SEADS_SMOKE_SPAWN_DIR reader. No new spawn logic; a caller-set dir
// still wins.
void apply_flash_smoke_env(const app::FlashSite& site) {
    char dir[128];
    std::snprintf(dir, sizeof dir, "%.9f,%.9f,%.9f", -site.dir.x, -site.dir.y,
                  -site.dir.z);
    const char* names[2] = {"SEADS_SMOKE_SPAWN_DIR", "SEADS_SPAWN_ALT"};
    const char* vals[2] = {dir, "2000"};
    for (int i = 0; i < 2; ++i) {
        if (std::getenv(names[i]) != nullptr) continue;  // his override wins
#ifdef _WIN32
        ::_putenv_s(names[i], vals[i]);
#else
        ::setenv(names[i], vals[i], 0);
#endif
    }
}

}  // namespace

int main(int argc, char** argv) {
    // The exe's own directory -- where seads_launch.log and the default feel
    // tape live, so an Explorer launch (no terminal, no stderr) still leaves
    // a record of the dials it flew.
    if (argc > 0 && argv[0] != nullptr) {
        const std::string a0(argv[0]);
        const size_t cut = a0.find_last_of("/\\");
        g_exe_dir = (cut == std::string::npos) ? std::string(".")
                                               : a0.substr(0, cut);
    }
    int smoke_frames = 0;
    // ★ ROAD-REPAIR ONAPING: expand SEADS_ONAPING_SMOKE=1 into the six envs
    // BEFORE anything reads one of them. It writes the environment and
    // nothing else, so every reader below is untouched.
    apply_onaping_smoke_env();
    // Declared here (not with the other --smoke args below) because the FLASH
    // rig's SEADS_FLASH_CEL_S seeds it during arming; the `--smoke` 4th
    // argument still overwrites it, so the argument wins.
    double smoke_cel_offset = 0.0;
    // ★ ROAD-REPAIR E2 -- THE FLASH INSTRUMENT, armed here for the same
    // reason: it WRITES the environment (the antipode spawn) and every reader
    // below must already see it. Unarmed, not one branch of this file moves.
    app::FlashSite flash_scratch{};
    const app::FlashSite* flash_site = nullptr;
    app::FlashCamParams flash_p;
    const char* flash_out = "flash";
    if (flash_smoke_armed()) {
        flash_site =
            app::flash_site_by_name(std::getenv("SEADS_FLASH_SMOKE"),
                                    &flash_scratch);
        if (flash_site == nullptr) {
            int nsite = 0;
            const app::FlashSite* all = app::flash_sites(&nsite);
            std::fprintf(stderr, "SEADS_FLASH_SMOKE: unknown site. Known:");
            for (int i = 0; i < nsite; ++i)
                std::fprintf(stderr, " %s", all[i].name);
            std::fprintf(stderr, " (or \"x,y,z\")\n");
            return 2;
        }
        flash_p.dist_m = flash_env_d("SEADS_FLASH_DIST_M", flash_p.dist_m);
        flash_p.eye_h_m = flash_env_d("SEADS_FLASH_EYE_H_M", flash_p.eye_h_m);
        flash_p.az_deg = flash_env_d("SEADS_FLASH_AZ_DEG", flash_p.az_deg);
        flash_p.target_h_m =
            flash_env_d("SEADS_FLASH_TARGET_H_M", flash_p.target_h_m);
        flash_p.jitter_m = flash_env_d("SEADS_FLASH_JITTER_M", flash_p.jitter_m);
        flash_p.frames = static_cast<int>(
            flash_env_d("SEADS_FLASH_FRAMES", flash_p.frames));
        if (flash_p.frames < 1) flash_p.frames = 1;
        if (const char* o = std::getenv("SEADS_FLASH_OUT")) flash_out = o;
        // THE LIGHT IS A DIAL TOO. [celestial] day_period_s is 300 s, so the
        // sun angle at a site is a function of when in that 300 s the frame
        // lands -- and a site rendered at local midnight grades its own
        // shadows, not the depth buffer. SEADS_FLASH_CEL_S seeds the SAME
        // render-only cel_time_offset the `--smoke` 4th argument seeds; the
        // per-site value in docs/road_repair/onaping_flash_E2.md is the one
        // that measured the brightest road, swept, not guessed.
        smoke_cel_offset = flash_env_d("SEADS_FLASH_CEL_S", smoke_cel_offset);
        apply_flash_smoke_env(*flash_site);
        std::printf(
            "[FLASH] site=%s dir=%.9f,%.9f,%.9f pairs=%d deck_overlap_max=%.1f m "
            "valley=%.3f km\n[FLASH] %s\n[FLASH] dist=%.1f m eye_h=%.1f m "
            "az=%.1f deg jitter=%.4f m frames=%d out=%s_f##.png\n",
            flash_site->name, flash_site->dir.x, flash_site->dir.y,
            flash_site->dir.z, flash_site->pairs,
            flash_site->deck_overlap_max_m, flash_site->valley_km,
            flash_site->note, flash_p.dist_m, flash_p.eye_h_m, flash_p.az_deg,
            flash_p.jitter_m, flash_p.frames, flash_out);
    }
    // ★ R3 SATURATION SWEEP -- the live value, owned HERE so there is ONE
    // authority and the on-screen readout can never disagree with what the
    // shader got. Seeded from SEADS_R3_FULLDEPTH (or the SnowParams ship value
    // when unset), stepped on PgUp/PgDn, fed to DrawInfo::r3_full_depth.
    float r3_full_depth = [] {
        const float ov = render::r3_full_depth_env_override();
        return ov > 0.0f ? ov : render::SnowParams{}.full_depth;
    }();
    const char* shot_path = nullptr;
    // Optional 4th --smoke arg: a deterministic celestial-time offset [s] to
    // seed the render-only cel_time_offset (same field the interactive ]/[
    // scrub drives). Lets a smoke shot land a specific day/dusk/night cell
    // without a clock (Fable-after P1-4: the green gate is blind to seads.exe,
    // so the nightscape needs pinned visual cells). Seam-safe: render-time
    // offset only.
    // v5 HUD RESTYLE VERIFY (Chad 2026-07-23, DEBUG-ONLY smoke args): two
    // further optional trailing args drive a ONE-SHOT direct nudge of loop.aim
    // at frame 30 (see the application site below) so a screenshot can show
    // the off-screen mouse glyph / the split-S fear read without needing a
    // live hand on the mouse. Neither arg exists outside --smoke.
    double smoke_offset_aim_deg = 0.0;  // yaw the aim LEFT this many degrees
    double smoke_aim_down_deg = 0.0;    // pitch the aim DOWN this many degrees
    if (argc >= 3 && std::strcmp(argv[1], "--smoke") == 0) {
        smoke_frames = std::atoi(argv[2]);
        if (smoke_frames <= 0) {
            std::fprintf(stderr, "bad --smoke frame count\n");
            return 2;
        }
        if (argc >= 4) shot_path = argv[3];
        if (argc >= 5) smoke_cel_offset = std::atof(argv[4]);
        if (argc >= 6) smoke_offset_aim_deg = std::atof(argv[5]);
        if (argc >= 7) smoke_aim_down_deg = std::atof(argv[6]);
    }
    // The flash rig needs every determinism property the --smoke path already
    // has, so it IS one: an explicit `--smoke N` still wins the frame count.
    if (flash_site != nullptr && smoke_frames == 0)
        smoke_frames = flash_p.frames;

    // Target-visibility probe (docs/world_build_plan.md §4; little_planet Stage
    // 0): a deterministic screenshot rig that spawns high, places ONE bandit at
    // a fixed range against the procedural ground, and measures its
    // luminance/chroma contrast vs the local background patch — every
    // Legibility sortie gets a NUMBER. A smoke variant (hands-off, fixed-dt)
    // plus a target injection + framebuffer readback below.
    // season_frac/day_frac are the API surface for the celestial stages (Stage
    // 1+); inert now, logged honestly.
    //   seads.exe --probe ALT_M RANGE_M [out.csv] [season_frac] [day_frac]
    bool probe_mode = false;
    double probe_alt = 3000.0, probe_range = 1000.0;
    double probe_season = 0.0, probe_day = 0.5;
    const char* probe_csv = nullptr;
    render::ProbeGeometry probe_geom = render::ProbeGeometry::Below;
    if (argc >= 2 && std::strcmp(argv[1], "--probe") == 0 && argc < 4) {
        std::fprintf(stderr,
                     "usage: --probe ALT_M RANGE_M [out.csv] [season] [day] "
                     "[below|above]\n");
        return 2;
    }
    if (argc >= 4 && std::strcmp(argv[1], "--probe") == 0) {
        probe_mode = true;
        probe_alt = std::atof(argv[2]);
        probe_range = std::atof(argv[3]);
        if (probe_alt <= 0.0 || probe_range <= 0.0) {
            std::fprintf(stderr, "bad --probe alt/range\n");
            return 2;
        }
        if (argc >= 5) probe_csv = argv[4];
        if (argc >= 6) probe_season = std::atof(argv[5]);
        if (argc >= 7) probe_day = std::atof(argv[6]);
        if (argc >= 8)
            probe_geom = std::strcmp(argv[7], "above") == 0
                             ? render::ProbeGeometry::Above
                             : render::ProbeGeometry::Below;
        smoke_frames =
            8;  // fixed hands-off settle, then measure the last frame
    }

    sim::AircraftParams params;
    control::ControllerParams cparams;
    cfg::ScenarioParams scen;
    cfg::WorldParams world;
    cfg::GameParams
        game;  // SCARCE SKIES mechanics (R4 [ground], config/game.toml)
    // Frozen-epoch celestial sun (Stage 2) + the [atmosphere] tuning, mapped to
    // render types once so render/ stays free of config/. sun_world is the
    // world LIGHT-TRAVEL direction (sun -> scene) = -(direction to sun); frozen
    // at t_epoch = 0.0 (the epoch IS the config phase offsets — no motion yet).
    glm::dvec3 sun_world{-0.45, -0.75, -0.48};
    render::AtmosphereParams atm;
    // S-airdome (docs/bubble_atmosphere_spec.md §1.9): the render-side mirror
    // of the LIVE sim::AtmosphereField. Render-tuning fields (haze/weather/
    // aerial/march) are set ONCE below from [atmosphere]; the geometry fields
    // (enabled/deck/bubbles) are rebuilt EVERY FRAME from env.atm (copy, never
    // re-derive) so the visual dome can never disagree with the flyable one.
    render::AirField air_field_render;
    // Stage 3a: the celestial params persist past the loader so the frame loop
    // can wheel the sun each tick (sun_dir(cel, t_cel)); `sunp` carries the
    // disc visual params, mapped once from cel below.
    render::CelestialParams cel;
    render::SunParams sunp;
    // Stage 3 weather variable: the pure t_cel -> haze[0,1] params. S-airdome
    // (spec §1.7) SUPERSEDES the prior "haze_density = weather_haze(t_cel) *
    // haze_overcast_density" line: the frame loop now feeds the RAW weather
    // scalar straight into atm.haze_density (uWeatherHaze); haze_overcast_
    // density is loaded/validated (config/load_world.cpp) but no longer read
    // here — air_weather_gain (render::AirField) replaces its role.
    render::WeatherParams wparams;
    // W2a localized weather field: the [weather] scalar above is the storm
    // BUDGET; wcparams places the microsystem cells on the sphere. The frame
    // loop feeds weather_cell(eyeGroundTrack, t_cel) into atm.haze_density.
    render::WeatherCellParams wcparams;
    // AS-2 (atmosphere rung, docs/SESSION_HANDOFF_20260912_atmosphere_snow.md):
    // the SNOWFALL field's own params, mapped from [precip]. The snow gets a
    // field BESIDE the haze so "more snow" never moves the Chad-signed haze
    // distribution. flurry_level == 0 && snow_floor == 0 => the old wfield.
    render::SnowfallParams sfparams;
    // Star field (Stage 4): the [stars] render knobs, mapped once from config.
    // The sky wheel (mat3) is set per frame in the loop; `cel` (built below)
    // supplies the star mesh's inertial basis via the FrameInfo pointer.
    render::StarParams starp;
    // Moon look knobs (Stage 5), mapped once from [moon] + [celestial]. The
    // moon dir + illuminated fraction are set per frame in the loop.
    render::MoonParams moonp;
    // Aurora (Stage 8), mapped once from [aurora] + cel (the inertial basis).
    // The periodic phase vec2(sin,cos) is set per frame; anim_rate drives it.
    render::AuroraParams aurp;
    double aurora_anim_rate = 0.0;
    try {
        params = cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
        cparams = cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml",
                                            params);
        // Load world BEFORE scenario so cannon_drag_k is available to
        // single-source the gunsight drag constant (FIX 5, Fable: no fork).
        world = cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
        game = cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", params);
        scen = cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml",
                                       params, world.guns.cannon_drag_k);

        // SELF-EVIDENCING FLY (2026-09-12). Two fly trees on this box bake
        // DIFFERENT absolute config dirs (SEADS_CONFIG_DIR is compile-time;
        // there is no env override), so "which kernel did I just fly?" was
        // unanswerable from the running game -- a walk-back was reported as
        // not working when the exe may simply have been reading another
        // tree's toml. Print the resolved path and the feel dials that are
        // live in THIS process, once, at load. Read-only, app-side: it
        // cannot touch the kernel.
        {
            char b[1024];
            std::snprintf(
                b, sizeof b,
                "[config] %s\n"
                "[config] auto_level: lean_lead %.3f lateral %d lat_lo %.2f "
                "lat_hi %.2f | lean_gain %.2f lean_max %.2f deg rate %.2f\n"
                "[config] roll: K_phi %.2f p_max %.1f deg/s | blend "
                "%.1f..%.1f deg | horizon_recovery rate %.1f deg/s "
                "straight_max %.1f deg/s\n"
                "[config] regime: wings_level_band %.3f\n"
                "[config] auto_level: right_hand_rest %.3f s (0 = off) | "
                "hand_net_window %.3f s (0 = off)\n"
                "[config] coordination: yaw_vert_budget %.2f (0 = off) gap %.1f..%.1f deg\n",
                SEADS_CONFIG_DIR "/controller.toml", cparams.lean_lead,
                int(cparams.lean_lead_lateral), cparams.lean_lead_lat_lo,
                cparams.lean_lead_lat_hi, cparams.lean_gain,
                cparams.lean_max * 180.0 / 3.14159265358979323846,
                cparams.auto_level_rate, cparams.K_phi,
                cparams.p_max * 180.0 / 3.14159265358979323846,
                cparams.blend_lo * 180.0 / 3.14159265358979323846,
                cparams.blend_hi * 180.0 / 3.14159265358979323846,
                cparams.horizon_recovery_rate * 180.0 /
                    3.14159265358979323846,
                cparams.horizon_recovery_straight_max * 180.0 /
                    3.14159265358979323846,
                cparams.wings_level_band, cparams.right_hand_rest,
                cparams.hand_net_window, cparams.yaw_vert_budget,
                std::asin(std::clamp(cparams.yaw_vert_gap_lo, -1.0,
                                     1.0)) * 180.0 /
                    3.14159265358979323846,
                std::asin(std::clamp(cparams.yaw_vert_gap_hi, -1.0,
                                     1.0)) * 180.0 /
                    3.14159265358979323846);
            g_config_banner = b;
        }
        std::fputs(g_config_banner.c_str(), stderr);
        write_launch_log();
        // (The S-lapguard FEEL TAPE used to be opened here. T2b red-team P1-5
        // moved it PAST the ground/facet resolve -- see the [config] ground
        // line further down -- because the crash surface is a dial a tape must
        // be replayed against, and it is not known yet at this point.)
        // (the four [config] fprintf blocks that stood here are now the
        // ONE g_config_banner above -- stderr, <exe_dir>/seads_launch.log
        // and the feel-tape header must never be able to disagree.)

        render::CelestialConfig ccfg;
        ccfg.orbit_normal = world.celestial.orbit_normal;
        ccfg.tilt_deg = world.celestial.tilt_deg;
        ccfg.tilt_lean = world.celestial.tilt_lean;
        ccfg.day_period_s = world.celestial.day_period_s;
        ccfg.year_period_s = world.celestial.year_period_s;
        ccfg.epoch_day_frac = world.celestial.epoch_day_frac;
        ccfg.epoch_year_frac = world.celestial.epoch_year_frac;
        ccfg.epoch_moon_frac = world.celestial.epoch_moon_frac;
        ccfg.sun_angular_diameter_deg =
            world.celestial.sun_angular_diameter_deg;
        ccfg.sun_distance_m = world.celestial.sun_distance_m;
        ccfg.sun_intensity = world.celestial.sun_intensity;
        ccfg.sun_glare_deg = world.celestial.sun_glare_deg;
        ccfg.moon_angular_diameter_deg =
            world.celestial.moon_angular_diameter_deg;
        ccfg.moon_period_s = world.celestial.moon_period_s;
        ccfg.moon_inclination_deg = world.celestial.moon_inclination_deg;
        ccfg.moon_intensity = world.celestial.moon_intensity;
        ccfg.star_brightness = world.celestial.star_brightness;
        cel = render::make_celestial(ccfg, params.R);
        sun_world =
            -render::sun_dir(cel, 0.0);  // initial (t=0); loop wheels it
        // Sun disc visual params (Stage 3a), mapped once from the derived cel.
        sunp.ang_radius_rad = static_cast<float>(cel.sun_angular_radius_rad);
        sunp.intensity = static_cast<float>(cel.sun_intensity);
        sunp.distance_m = static_cast<float>(cel.sun_distance_m);

        const auto& a = world.atmosphere;
        atm.sky_space = static_cast<float>(a.sky_space);
        atm.sky_band_top_rad = static_cast<float>(a.sky_band_top_rad);
        atm.sky_day = static_cast<float>(a.sky_day);
        atm.sky_dusk = static_cast<float>(a.sky_dusk);
        atm.sky_night = static_cast<float>(a.sky_night);
        atm.dusk_lo_rad = static_cast<float>(a.dusk_lo_rad);
        atm.dusk_hi_rad = static_cast<float>(a.dusk_hi_rad);
        atm.night_fill_min = static_cast<float>(a.night_fill_min);
        atm.ground_day_gain = static_cast<float>(a.ground_day_gain);
        atm.dither = static_cast<float>(a.dither);
        atm.haze_scale_m = static_cast<float>(a.haze_scale_m);
        atm.haze_density = 0.0f;  // set per-frame by the weather gate (below)
        // Atmospheric scatter (Stage 3): the haze-gated color. Gate reuses the
        // per-frame uWeatherAmt (no new per-frame path).
        atm.scatter_strength = static_cast<float>(a.scatter_strength);
        atm.mie_g = static_cast<float>(a.mie_g);
        // S-sunglare: the Mie forward-halo coefficients (config, not baked).
        atm.mie_halo_gain = static_cast<float>(a.mie_halo_gain);
        atm.mie_dusk_boost = static_cast<float>(a.mie_dusk_boost);
        atm.mie_rim_lift = static_cast<float>(a.mie_rim_lift);
        for (int i = 0; i < 3; ++i) {
            atm.rayleigh_tint[i] = static_cast<float>(a.rayleigh_tint[i]);
            atm.mie_tint[i] = static_cast<float>(a.mie_tint[i]);
        }
        // S-airdome (spec §1.1/§1.9): the render-tuning half of AirField
        // (geometry — enabled/deck/bubbles — is rebuilt live every frame
        // below, from the SAME sim::AtmosphereField the plant flies).
        air_field_render.haze_density = a.air_haze_density;
        air_field_render.weather_gain = a.air_weather_gain;
        air_field_render.aerial_gain = a.aerial_gain;
        air_field_render.tau_scale_m = a.tau_scale_m;
        air_field_render.march_max_m = a.march_max_m;
        air_field_render.march_steps_sky = a.march_steps_sky;
        air_field_render.march_steps_ground = a.march_steps_ground;
        // Quality fix pass item 7: haze_overcast_density now wired back as
        // the CEILING on the weather thickening term (uAirHazeCeiling), no
        // longer an orphaned key.
        air_field_render.haze_overcast_density = a.haze_overcast_density;
        for (int i = 0; i < 3; ++i)
            air_field_render.mie_tint_day[i] =
                static_cast<float>(a.mie_tint_day[i]);

        // Weather variable params (Stage 3), mapped once from [weather].
        const auto& wcfg = world.weather;
        wparams.period1_s = wcfg.period1_s;
        wparams.period2_s = wcfg.period2_s;
        wparams.period3_s = wcfg.period3_s;
        wparams.weight1 = wcfg.weight1;
        wparams.weight2 = wcfg.weight2;
        wparams.weight3 = wcfg.weight3;
        wparams.phase1 = wcfg.phase1;
        wparams.phase2 = wcfg.phase2;
        wparams.phase3 = wcfg.phase3;
        wparams.gate_lo = wcfg.gate_lo;
        wparams.gate_hi = wcfg.gate_hi;

        // Localized weather field params (W2a), mapped once from
        // [weather_cell].
        const auto& wccfg = world.weather_cell;
        wcparams.cell_count = wccfg.cell_count;
        wcparams.inner_deg = wccfg.inner_deg;
        wcparams.outer_deg = wccfg.outer_deg;
        wcparams.thresh_lo = wccfg.thresh_lo;
        wcparams.thresh_hi = wccfg.thresh_hi;
        wcparams.env_width = wccfg.env_width;
        wcparams.bubble_cell_count = wccfg.bubble_cell_count;
        wcparams.bubble_inner_deg = wccfg.bubble_inner_deg;
        wcparams.bubble_outer_deg = wccfg.bubble_outer_deg;
        wcparams.bubble_fill_frac = wccfg.bubble_fill_frac;

        // AS-2 snowfall field params, mapped once from [precip].
        sfparams.flurry_level = world.precip.flurry_level;
        sfparams.flurry_thresh_lo = world.precip.flurry_thresh_lo;
        sfparams.flurry_thresh_hi = world.precip.flurry_thresh_hi;
        sfparams.flurry_gate_lo = world.precip.flurry_gate_lo;
        sfparams.flurry_period_scale = world.precip.flurry_period_scale;
        sfparams.flurry_inner_deg = world.precip.flurry_inner_deg;
        sfparams.flurry_outer_deg = world.precip.flurry_outer_deg;
        sfparams.flurry_cell_count =
            static_cast<int>(world.precip.flurry_cell_count);
        sfparams.flurry_phase_off = world.precip.flurry_phase_off;
        sfparams.snow_floor = world.precip.snow_floor;
        // AS-5: the snow squall's own dials. The HAZE still reads wparams/
        // wcparams through weather_cell (wfield) -- only the snow sees these.
        sfparams.squall_own_dials = world.precip.squall_own_dials;
        sfparams.squall_inner_deg = world.precip.squall_inner_deg;
        sfparams.squall_outer_deg = world.precip.squall_outer_deg;
        sfparams.squall_cell_count =
            static_cast<int>(world.precip.squall_cell_count);
        sfparams.squall_gate_lo = world.precip.squall_gate_lo;
        sfparams.squall_thresh_lo = world.precip.squall_thresh_lo;
        sfparams.squall_thresh_hi = world.precip.squall_thresh_hi;
        sfparams.squall_period_scale = world.precip.squall_period_scale;
        sfparams.squall_phase_off = world.precip.squall_phase_off;

        // Star field render knobs (Stage 4), mapped once from [stars].
        // Brightness reuses the [celestial] star_brightness (the overall
        // night-sky gain); glare_suppress converts deg -> a cos threshold at
        // the config boundary.
        const auto& scfg = world.stars;
        starp.brightness = static_cast<float>(world.celestial.star_brightness);
        starp.mag_ref = static_cast<float>(scfg.mag_ref);
        starp.mag_limit = static_cast<float>(scfg.mag_limit);
        starp.size_px = static_cast<float>(scfg.size_px);
        starp.glare_suppress_cos = static_cast<float>(
            std::cos(scfg.glare_suppress_deg * 3.14159265358979323846 / 180.0));
        starp.wash_lum = static_cast<float>(scfg.wash_lum);
        // S-starnight: clear dome air veils stars only if this is dialed up.
        starp.air_extinction = static_cast<float>(scfg.air_extinction);
        starp.r_star_m = static_cast<float>(scfg.r_star_m);

        // Moon look knobs (Stage 5): the disc angular size from [celestial],
        // the rest from [moon]. Phase-scaled ground fill is derived per frame.
        moonp.ang_radius_rad =
            static_cast<float>(0.5 * world.celestial.moon_angular_diameter_deg *
                               3.14159265358979323846 / 180.0);
        moonp.disc_intensity = static_cast<float>(world.moon.disc_intensity);
        moonp.ground_gain = static_cast<float>(world.moon.ground_gain);
        moonp.fill_lo = static_cast<float>(world.moon.fill_lo);
        moonp.sparkle_sharpness =
            static_cast<float>(world.moon.sparkle_sharpness);

        // Aurora (Stage 8): the [aurora] knobs + the INERTIAL basis from cel (â
        // + its equatorial perpendiculars, so the oval is anchored to the fixed
        // ground pole and does NOT wheel with the stars). shell_r = R + height
        // (the eye stays inside; height is set above the flyable ceiling).
        const auto& acfg = world.aurora;
        aurp.intensity =
            acfg.enabled ? static_cast<float>(acfg.intensity) : 0.0f;
        aurp.oval_center_rad = static_cast<float>(acfg.oval_center_rad);
        aurp.oval_width_rad = static_cast<float>(acfg.oval_width_rad);
        aurp.curtain_scale = static_cast<float>(acfg.curtain_scale);
        aurp.shell_r_m = static_cast<float>(params.R + acfg.height_m);
        aurp.ground_glow = static_cast<float>(acfg.ground_glow);
        aurp.haze_suppress = static_cast<float>(acfg.haze_suppress);
        for (int i = 0; i < 3; ++i) {
            aurp.tint_low[i] = static_cast<float>(acfg.tint_low[i]);
            aurp.tint_high[i] = static_cast<float>(acfg.tint_high[i]);
        }
        aurp.spin_axis = glm::vec3(cel.spin_axis);
        aurp.ref_a = glm::vec3(cel.equator_x);
        aurp.ref_b = glm::vec3(cel.equator_y);
        aurora_anim_rate = acfg.anim_rate;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "config: %s\n", e.what());
        return 1;
    }

    // [seasons] W1 (docs/weather_seasons_plan.md): the active WEATHER SEASON,
    // chosen APP-SIDE — never in render/, never a function of t_cel, never a
    // clock (SPEC §6/§9). Precedence (Fable-before P1-6): env SEADS_SEASON >
    // [seasons] static_season > smoke/probe default > a weighted RANDOM draw.
    // Chad's ruling: a FRESH random draw each spawn/respawn ("random on every
    // spawn"), but SETTABLE to a static season for building the winter loop.
    // The RNG is app-local (cosmetic, like the screenshot rig), seeded once
    // from random_device; the strict loader already guaranteed the weights sum
    // > 0.
    std::mt19937 season_rng(std::random_device{}());
    std::discrete_distribution<int> season_dist{
        world.seasons.weight_winter, world.seasons.weight_spring,
        world.seasons.weight_summer, world.seasons.weight_autumn};
    render::Season env_season = render::Season::Summer;
    const bool have_env_season = [&] {
        const char* e = std::getenv("SEADS_SEASON");
        return e != nullptr && render::season_from_string(e, env_season);
    }();
    // The precedence (env > static > smoke > random) is the pure, unit-tested
    // render::resolve_season; the random branch's draw() is invoked ONLY when
    // reached, so the deterministic smoke/probe path never samples the RNG.
    const auto pick_season = [&]() -> render::Season {
        return render::resolve_season(
            have_env_season, env_season, world.seasons.static_season,
            smoke_frames > 0, [&] { return season_dist(season_rng); });
    };
    render::Season current_season = pick_season();
    // Feature A: hero-plane visibility mode, cycled live with 'N'. NeonRim is
    // Chad's default pick (hot neon rim + brighter body); Mirror is the classic
    // finish. App-owned + render-only (read into FrameInfo below; never the
    // kernel).
    render::PlaneViz plane_viz = render::PlaneViz::NeonRim;

    // R4-FLY-7 thumb-throttle button codes ([input] game.toml — Windows
    // drivers disagree which raylib code a physical thumb fires; Chad edits
    // the table, never the code). input/ stays config-free: built HERE.
    const input::ThumbBinds thumb_binds{
        game.input.thumb_up_a, game.input.thumb_up_b, game.input.thumb_down_a,
        game.input.thumb_down_b};

    // VSYNC is dropped in a --smoke run so the per-frame timing readout below
    // measures true GPU throughput (a vsync-capped swap would floor every
    // scenario at the refresh interval and hide relative draw costs). Gameplay
    // keeps vsync.
    SetConfigFlags(
        (smoke_frames > 0 ? 0u : static_cast<unsigned>(FLAG_VSYNC_HINT)) |
        FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "SEADS — mouse-aim instructor");
    render::init_draw();
    // MB-7c (i): the wind-audio channel — a procedural noise stream whose
    // volume/pitch track airspeed (render::wind_level, pure). Cosmetic,
    // read-only; skipped in smoke (headless CI has no audio device) and
    // whenever the device fails to open.
    AudioStream wind_stream{};
    bool wind_ok = false;
    render::WindSynth wind_synth;  // real-time calm wind
    // Soundbank channels (recorded assets; see the load block below for why
    // these use Music/Sound rather than the hand-fed AudioStream pattern).
    // Two music beds cross-faded on the tunnel-net predicate: Chad, 2026-08-17
    // — "Volatus_Aeturnus is perfect music for when the airplane enters the
    // tunnels and while in the black stope ... As you exit that music fades."
    Music music_surface{};
    Music music_deep{};
    Sound stope_boom{};
    bool music_surface_ok = false;
    bool music_deep_ok = false;
    bool stope_boom_ok = false;
    render::MusicDirector music_dir;
    render::StopeRumble stope_rumble;
    // ⭐ THE TWO ANIMALS AT THE SURFACE PUMPS (Chad, 2026-08-20: "I want a wolf
    // cry to sound when you get to the enemy surface pump in sudbury ... and a
    // wildcat [cry] for near levak and onaping falls allied pump ... when you
    // fly over the area or snomobile close to it"). One raylib Sound each,
    // triggered by the pure scheduler in render/pump_ambience.h — which owns
    // the ranges, the cadence and the balance, and knows nothing about raylib.
    //
    // Indexed by render::PumpCry: [0] wolf @ Sudbury surface, [1] wildcat @
    // Valley (Onaping/Levack) surface. Keyed to the PLACES, not to who owns
    // them this match — see the header for why that distinction matters.
    Sound wolf_cry{};
    Sound wildcat_cry{};
    bool wolf_cry_ok = false;
    bool wildcat_cry_ok = false;
    render::CryScheduler cry_sched[render::kPumpCryCount];

    // ⭐ THE DISTANT TRAIN. Chad, 2026-08-17: "The train sounds every once in a
    // couple of minutes when on the snowmachine near a town or in a town and
    // can be heard also when flying over a town sometimes" -- and 2026-08-24,
    // "where is that train sound when flying over chelmsford?", which is this
    // block existing at last. render/town_ambience.h had been finished,
    // unit-tested and shipped since 2026-08-17 with NO CALLER: the asset was
    // built and installed, the manifest row said wired=yes, and nothing in the
    // app had ever included the header. Same failure the sled synth had; the
    // manifest's own advice applies -- if a row looks placed, check for a
    // caller, not just for a decision.
    //
    // NOT render/train.cpp. That one is the Copper Cliff slag trolley, an
    // object you can see on a closed spur; this is a mainline train out past
    // the treeline that has no model and never will until Chad builds one.
    Sound train_pass{};
    bool train_pass_ok = false;
    render::TrainAmbience train_amb;
    // Building density is what "near a town" means here (see the header). Held
    // as the same cached singleton the collision path uses, fetched below and
    // INDEPENDENT of [buildings] collide -- turning collision off should not
    // empty the towns of sound.
    const world::BuildingColliders* town_colliders = nullptr;

    // ⭐ THE CHURCH BELL. Chad, 2026-08-24: "make it play different amounts of
    // plays at random, sometimes it rings 3 times, sometimes 7 times, up to
    // twelve to simulate the number of chimes it plays based on the hour ... It
    // should play about two minutes after the train might, in and over
    // chelmsford" -- and, on the floor, "from one chime up to 12, 3 was an
    // example, it should be random though".
    //
    // ⚠ THE ONE POLYPHONIC SAMPLE CHANNEL IN THIS GAME, and it has to be. Every
    // other one-shot here (the train, the blast, both animals) is a single
    // raylib Sound, where PlaySound on a playing voice RESTARTS it -- which is
    // fine when the gaps clear the asset, and is exactly what must not happen
    // here: the bell rings for 4.44 s and is struck every 2.4 s, so a strike
    // would truncate the one still ringing, up to eleven times a peal, and a
    // truncated bell is a click.
    //
    // So the strikes go round a ring of ALIASES (LoadSoundAlias, raylib 5.5):
    // separate voices sharing one copy of the sample data, so the ring costs
    // four handles and no memory. render::kBellVoiceCount sizes it and the
    // header static_asserts that size against the asset's own decay, so a
    // re-cut cannot silently bring the truncation back.
    Sound church_bell{};
    Sound church_bell_voice[render::kBellVoiceCount]{};
    bool church_bell_ok = false;
    int church_bell_next = 0;
    render::BellTower bell_tower;
    // Fixed on the first strike of a peal and held for the rest of it -- see
    // the per-frame block for why it is not re-evaluated per chime.
    float bell_level = 0.0f;
    // Intro-only channels, unloaded the moment the splash sequence ends.
    //
    // intro_music is the SAME asset as music_surface, opened as its own handle
    // with a very deep sub-buffer. That is not redundancy — it is the whole
    // reason the loading page can have music at all. Nothing calls
    // UpdateMusicStream while planet_heightfield() or build_tunnel_net() is
    // running, so a gameplay-sized sub-buffer (93 ms) empties in the first
    // blink of the load and the "louder in the loading page" bed is silence for
    // all of it. This handle buffers SECONDS ahead instead; the gameplay bus
    // keeps its small buffer and its small hitches, untouched.
    Music intro_music{};
    Music intro_wind{};
    Sound intro_machine{};
    Sound intro_ring{};
    Sound intro_operator{};
    bool intro_music_ok = false;
    bool intro_wind_ok = false;
    bool intro_machine_ok = false;
    bool intro_ring_ok = false;
    bool intro_operator_ok = false;
    // Throttle voice: a highland-bagpipe DRONE whose PITCH rides the throttle
    // (render::BagpipeThrottle — replaces the old synthesized hum; Chad,
    // 2026-07-24). Full throttle = the drone sped up (higher); it drops in
    // pitch as throttle comes back. Same guards as the wind channel — skipped
    // in smoke, only when the device opened AND the .wav decoded. Cosmetic,
    // read-only off state.throttle.
    AudioStream engine_stream{};
    bool engine_ok = false;
    render::BagpipeThrottle engine_synth;
    // Combat voices: the gun-fire stutter (GunSynth) and the explosion/hit
    // one-shot pool (CombatSfxSynth). Event-driven, not continuous — but fed
    // through the same per-frame AudioStream plumbing as wind/engine. Cosmetic,
    // read-only: GunSynth is driven by the trigger-held boolean, CombatSfxSynth
    // by edge-detecting cw.kills/cw.hits below. Same smoke/device guards.
    AudioStream gun_stream{};
    bool gun_ok = false;
    render::GunSynth gun_synth;
    AudioStream sfx_stream{};
    bool sfx_ok = false;
    render::CombatSfxSynth sfx_synth;
    // ⭐ THE SNOWMACHINE ENGINE (Chad, 2026-08-17: "Idle engine indy 650 for
    // idle and indy_650 engine sound for riding ... Tapping keys gives the brap
    // brap ramp up"). render/sled_synth.h has existed, unit-tested and
    // red-teamed, since that session and was never connected to the app -- the
    // manifest's two loops shipped `wired=no`. This is the connection.
    //
    // Sample-based AND rpm-coupled, which is what let it past the "a static
    // loop kills the sense of speed" doctrine: both of Chad's recordings are
    // replayed at a REV-DRIVEN resample ratio (the bagpipe drone's pattern),
    // with the riding bed re-attacked on each throttle onset for the brap and a
    // derived two-stroke tone under the sustain. Fed through the same
    // 22050/16/1 hand-fed plumbing as wind/engine/gun/sfx.
    AudioStream sled_stream{};
    // ★★★ ST-5 POLISH (c) -- THE STING'S MOTOR BUZZ (Chad 2026-09-05: "need
    // some sound for the model increasing pitch and volume with throttle").
    // The snowmachine channel's shape exactly: a pure synth (render/
    // sting_audio.h) + a hand-fed 22050/16/1 stream + a slewed gate, so the
    // drone fades in rather than stepping a full-scale channel on at a buffer
    // boundary.
    bool sting_audio_ok = false;
    render::StingSynth sting_synth;
    AudioStream sting_stream{};
    double sting_audio_gate = 0.0;
    bool sled_ok = false;
    render::SledSynth sled_synth;
    // How far the machine is "mounted", 0..1, slewed. NOT a boolean: the gate
    // has to open and close over a few hundred ms or mounting and dismounting
    // would step a full-scale engine channel on and off in one buffer, which is
    // a click at exactly the moment the player is looking for feedback.
    double sled_audio_gate = 0.0;
    // THE STOPE ECHO (Chad's written spec: "Make all sounds like guns echo when
    // in the stope as well"). One reverb network per hand-fed stream — filter
    // state cannot be shared between two different signals — keyed on the same
    // app::inside_tunnel predicate as the camera and the music bed. Outside the
    // net every one of them bypasses to a bit-identical dry passthrough.
    // The wind bed is deliberately NOT reverbed; see render/stope_reverb.h.
    render::StopeReverb gun_verb;
    render::StopeReverb sfx_verb;
    render::StopeReverb engine_verb;
    render::StopeReverb sled_verb;
    render::StopeReverb sting_verb;  // ST-5 (c): the drone rides the room too
    // ⭐ THE BLAST'S OWN ECHO (Chad, 2026-08-17: "yes echos the stope explosion
    // itself"). The recorded blast keeps playing DRY through raylib's Sound
    // path -- full stereo, its own 44.1 kHz, untouched -- and a mono copy is
    // fed through this send, whose reverb runs wet_only. A send rather than an
    // insert, precisely so the dry blast keeps the stereo quality Chad asked
    // for while still coming back off the walls.
    AudioStream boom_send_stream{};
    bool boom_send_ok = false;
    render::SampleVoice boom_send;
    render::StopeReverb boom_verb;
    if (smoke_frames == 0) {
        InitAudioDevice();
        if (IsAudioDeviceReady()) {
            // Match the stream sub-buffer to our 1024-frame feed. Without this,
            // raylib's default sub-buffer is larger, so each swap gets
            // partially zero-filled -> periodic clicks (the "crackling"). Pin
            // it = clean.
            SetAudioStreamBufferSizeDefault(1024);

            wind_stream = LoadAudioStream(22050, 16, 1);
            PlayAudioStream(wind_stream);
            // The wind's master trim now comes from render/mix_levels.h,
            // which holds the WHOLE gameplay balance on one screen. Two of
            // Chad's rulings are folded into it: fly 1 ("the wind and the
            // engine are too loud to hear the music") moved the RATIOS, and
            // "trim wind / engine further" then dropped the whole gameplay bus
            // so the loading page is audibly louder than gameplay, as his
            // readme asks. This is a MIX trim only -- the wind synth's internal
            // law (speed/density/G) is untouched, so the energy cue it exists
            // for is unchanged, it is only seated lower.
            SetAudioStreamVolume(wind_stream,
                                 static_cast<float>(render::kWindStreamTrim));
            wind_synth.init(22050.0);
            wind_ok = true;

            // Throttle voice = the bagpipe drone, decoded HERE (raylib owns
            // the disk/codec) and handed to the pure synth as float samples;
            // the synth resamples + seam-crossfades it once, then pitch-shifts
            // it live off throttle. If the .wav is missing/undecodable, the
            // channel simply stays silent (guarded).
            //
            // The path moved from a LOOSE assets/audio/bagpipe_drone.wav into
            // the soundbank's loops/ folder on 2026-08-18, and that move is the
            // whole point rather than tidying. `.gitignore`'s
            // `assets/audio/*.wav` is a SINGLE-LEVEL glob -- a `*` does not
            // cross a `/` -- so the old flat path was silently un-addable: `git
            // add` exited 0 and staged nothing. Combined with the guard just
            // below, that made this channel silent in every checkout but this
            // one, from 2026-07-24 until now, with nothing ever reported. One
            // folder deeper commits normally. The asset is now built and
            // levelled by tools/audio/build_soundbank.sh like every other one.
            //
            // The guard also PRINTS, matching the soundbank loaders below:
            // silence you were never told about is the trap, not the silence.
            {
                const char* bag_path =
                    SEADS_ASSET_DIR "/audio/loops/bagpipe_drone.wav";
                if (!FileExists(bag_path)) {
                    std::printf("[AUDIO] missing, throttle voice silent: %s\n",
                                bag_path);
                } else {
                    Wave w = LoadWave(bag_path);
                    if (w.frameCount > 0 && w.data != nullptr) {
                        float* s = LoadWaveSamples(w);  // interleaved float
                        if (s != nullptr) {
                            engine_synth.init(
                                22050.0, s, w.frameCount,
                                static_cast<int>(w.channels),
                                static_cast<double>(w.sampleRate));
                            UnloadWaveSamples(s);
                        }
                    }
                    UnloadWave(w);
                    if (!engine_synth.ok()) {
                        std::printf(
                            "[AUDIO] failed to decode, throttle voice silent: "
                            "%s\n",
                            bag_path);
                    }
                }
            }
            if (engine_synth.ok()) {
                engine_stream = LoadAudioStream(22050, 16, 1);
                PlayAudioStream(engine_stream);
                // Same source and the same two rulings as the wind trim
                // above (render/mix_levels.h). The throttle-swells-volume cue
                // is preserved -- it is a ratio inside the synth, not an
                // absolute -- just seated further under the music.
                SetAudioStreamVolume(
                    engine_stream,
                    static_cast<float>(render::kEngineStreamTrim));
                engine_ok = true;
            }

            // Gun-fire stutter. Composite cyclic rates = per-gun rof × the
            // battery count of each class (3 cannon: hub + 2 wing gondolas;
            // 2 cowl MG — matches the gw.battery build below). The synth
            // carries its own audio-time shot schedulers, so the brrrt stays
            // smooth regardless of frame rate.
            gun_stream = LoadAudioStream(22050, 16, 1);
            PlayAudioStream(gun_stream);
            SetAudioStreamVolume(gun_stream,
                                 static_cast<float>(render::kGunStreamLevel));
            gun_synth.init(22050.0, 3.0 * world.guns.cannon_rof_hz,
                           2.0 * world.guns.mg_rof_hz);
            gun_ok = true;

            // Explosion + hit one-shots (polyphonic voice pool).
            sfx_stream = LoadAudioStream(22050, 16, 1);
            PlayAudioStream(sfx_stream);
            SetAudioStreamVolume(sfx_stream,
                                 static_cast<float>(render::kSfxStreamLevel));
            sfx_synth.init(22050.0);
            sfx_ok = true;

            // ⭐ THE SNOWMACHINE ENGINE. Both of Chad's recordings are decoded
            // HERE (raylib owns the disk and the codec) and handed to the pure
            // synth as float samples; the synth downmixes, resamples and then
            // replays them at a rev-driven ratio. Exactly the split the bagpipe
            // drone already uses -- and the reason this channel is allowed to
            // be sample-based at all, per the manifest's DOCTRINE HOLD note: it
            // is rpm-coupled playback, not a flat loop.
            //
            // ⚠ CREATED HERE, ON PURPOSE, while the stream sub-buffer default
            // is still the 1024 pinned at the top of this block. The music
            // loads below raise it to 4096 for the 44.1 kHz beds, and raylib
            // gives LoadAudioStream whatever the default is at the moment of
            // the call while ZERO-FILLING the unwritten remainder of each
            // sub-buffer -- so a 22050 Hz stream created after that point and
            // fed 1024 frames plays 25% signal and 75% silence. That is the
            // exact defect the blast send below documents at length. Do not
            // move this block past the music loads.
            //
            // Both loads are guarded and both PRINT on failure, like every
            // other channel: a missing asset leaves the machine silent with a
            // stated reason rather than silent for nobody knows how long. If
            // EITHER wav is missing the synth stays uninitialised and the whole
            // channel is skipped -- the idle bed alone, with no riding bed to
            // hand over to, is worse than nothing.
            {
                const char* idle_path =
                    SEADS_ASSET_DIR "/audio/loops/sled_idle_loop.wav";
                const char* run_path =
                    SEADS_ASSET_DIR "/audio/loops/sled_run.wav";
                if (!FileExists(idle_path) || !FileExists(run_path)) {
                    std::printf(
                        "[AUDIO] missing, snowmachine engine silent: %s / %s\n",
                        idle_path, run_path);
                } else {
                    Wave iw = LoadWave(idle_path);
                    Wave rw = LoadWave(run_path);
                    float* is = (iw.frameCount > 0 && iw.data != nullptr)
                                    ? LoadWaveSamples(iw)
                                    : nullptr;
                    float* rs = (rw.frameCount > 0 && rw.data != nullptr)
                                    ? LoadWaveSamples(rw)
                                    : nullptr;
                    if (is != nullptr && rs != nullptr) {
                        sled_synth.init(22050.0, is, iw.frameCount,
                                        static_cast<int>(iw.channels),
                                        static_cast<double>(iw.sampleRate), rs,
                                        rw.frameCount,
                                        static_cast<int>(rw.channels),
                                        static_cast<double>(rw.sampleRate));
                    }
                    if (is != nullptr) UnloadWaveSamples(is);
                    if (rs != nullptr) UnloadWaveSamples(rs);
                    UnloadWave(iw);
                    UnloadWave(rw);
                    if (!sled_synth.ok()) {
                        std::printf(
                            "[AUDIO] failed to decode, snowmachine engine "
                            "silent: %s / %s\n",
                            idle_path, run_path);
                    }
                }
            }
            if (sled_synth.ok()) {
                sled_stream = LoadAudioStream(22050, 16, 1);
                PlayAudioStream(sled_stream);
                // Opens at ZERO. The player starts in the air, and the mount
                // gate below is what brings the machine in -- starting at the
                // mix level would put a full-scale idle bed under the loading
                // page's first flight frames.
                SetAudioStreamVolume(sled_stream, 0.0f);
                sled_ok = true;
            }
            // ★★★ ST-5 POLISH (c): the Sting's motor buzz. Wholly
            // synthetic -- there is no drone recording in the soundbank -- so
            // unlike the snowmachine there is no wav to miss and no "silent
            // with a stated reason" branch: it either has an audio device or
            // it does not. Kill switch SEADS_STING_AUDIO=0, the
            // SEADS_STING_MODEL spelling, read once.
            {
                const char* sa = std::getenv("SEADS_STING_AUDIO");
                if (sa == nullptr || std::strcmp(sa, "0") != 0) {
                    sting_synth.init(22050.0);
                    sting_stream = LoadAudioStream(22050, 16, 1);
                    PlayAudioStream(sting_stream);
                    // Opens at ZERO, the sled_stream rule: the gate below is
                    // what brings it in, and nothing should be flying yet.
                    SetAudioStreamVolume(sting_stream, 0.0f);
                    sting_audio_ok = true;
                }
            }

            // The stope rooms, at the same rate every hand-fed stream runs.
            gun_verb.init(22050.0);
            sfx_verb.init(22050.0);
            engine_verb.init(22050.0);
            sled_verb.init(22050.0);
            sting_verb.init(22050.0);
            boom_verb.init(22050.0);
            boom_verb.wet_only = true;  // a SEND: the room only, never the dry

            // ---------------------------------------------------------------
            // The SOUNDBANK channels (Chad's audio rung, 2026-08-17).
            //
            // These are RECORDED assets, unlike the four procedural channels
            // above, and that split is deliberate. The synths own every layer
            // whose job is to convey SPEED -- wind rides airspeed, the drone
            // rides throttle -- because a static loop there "kills the sense
            // of speed instantly" (audio_ideas/wind_audio.md). Nothing below
            // is speed-coupled: music, a scripted intro, an ambient blast.
            // No procedural channel is replaced or touched.
            //
            // raylib's Music (streamed) and Sound (decoded whole) APIs are
            // used here rather than the hand-fed AudioStream pattern above,
            // because these need no per-sample DSP -- only a volume, which
            // render::MusicDirector computes. That also lets the beds stay
            // 44.1 kHz STEREO (Chad: "I prefer stereo sound quality") instead
            // of being forced down the synths' 22050/16/1 mono pipe.
            //
            // Every load is guarded by FileExists: a missing asset leaves the
            // channel silent rather than taking the game down. That policy is
            // why bagpipe_drone.wav was quietly absent from every fresh
            // checkout for three weeks, so each guard below ALSO prints, once,
            // on failure -- silence you were never told about is the trap, not
            // the silence. (The drone itself now lives in the bank at
            // audio/loops/ and prints the same way; see its loader above.)
            // ---------------------------------------------------------------
            auto load_music = [](const char* path, Music& out, bool& ok) {
                if (!FileExists(path)) {
                    std::printf("[AUDIO] missing, channel silent: %s\n", path);
                    return;
                }
                out = LoadMusicStream(path);
                if (out.stream.buffer == nullptr) {
                    std::printf("[AUDIO] failed to decode: %s\n", path);
                    return;
                }
                out.looping = true;
                PlayMusicStream(out);
                SetMusicVolume(out, 0.0f);  // director owns the level
                ok = true;
            };
            auto load_sound = [](const char* path, Sound& out, bool& ok) {
                if (!FileExists(path)) {
                    std::printf("[AUDIO] missing, channel silent: %s\n", path);
                    return;
                }
                out = LoadSound(path);
                ok = (out.frameCount > 0);
                if (!ok) std::printf("[AUDIO] failed to decode: %s\n", path);
            };

            // Raise the stream sub-buffer BEFORE any music loads. The 1024
            // pinned above is sized for the 22050 Hz hand-fed streams, where
            // it is 46 ms. ⚠ ANY hand-fed stream created after this point must
            // set it back to 1024 first (the blast send below does) -- raylib
            // zero-fills the unwritten remainder of a sub-buffer, so a
            // 4096-frame stream fed 1024 frames stutters at 5.4 Hz. raylib
            // applies AUDIO.Buffer.defaultSize to every subsequent
            // LoadAudioStream, and LoadMusicStream opens at the ASSET's rate —
            // so a 44.1 kHz bed would inherit 23 ms sub-buffers and underrun on
            // any frame spike. This repo prints a profiler line every time a
            // frame exceeds 20 ms, and the post-processing FBO is built lazily
            // on the first flight frame (a guaranteed hitch with the surface
            // bed already playing). 4096 = 93 ms at 44.1 kHz. Safe to set here:
            // the four synth streams already exist. ⚠ It is NO LONGER true that
            // nothing after this point creates a 22050 Hz stream -- the blast
            // send does, and it sets the default back to 1024 around its own
            // LoadAudioStream for exactly the reason spelled out there. Any
            // future stream must do the same. ⭐ THE LOADING-PAGE BED, opened
            // FIRST and with a far deeper sub-buffer than anything else in the
            // game.
            //
            // Chad's readme: the music "will be louder in the LOADING PAGE and
            // quieter during the gameplay". A loading page has, by definition,
            // no frame loop — the world build below runs for seconds at a time
            // inside single calls (planet_heightfield, build_tunnel_net), and
            // raylib only refills a music stream when UpdateMusicStream is
            // called. At 4096 frames the bed holds 186 ms and then plays
            // silence through the whole load.
            //
            // SIZED AGAINST A MEASUREMENT, not by feel. SEADS_LOADPROF=1 on
            // this build prints the gap between consecutive pumps; the longest
            // single stage measured 21.9 s (render::planet_heightfield ->
            // load_planet, one uninterruptible call) of a ~22 s total. raylib
            // keeps TWO sub-buffers filled, so the survivable gap is 2x the
            // size below: 1411200 frames = 32 s each, 64 s of runway, ~3x the
            // worst measured stage. That is the margin for the LONGEST STAGE,
            // not for the total load. ~11 MB, freed the moment the intro ends.
            //
            // If a stage ever outruns it the bed simply goes quiet mid-page —
            // re-measure with SEADS_LOADPROF before changing this number.
            //
            // A looping Music refills a FULL sub-buffer across the loop point
            // (raudio.c: `|| music.looping`), so a 32 s sub-buffer on a 104 s
            // bed wraps seamlessly rather than zero-padding.
            //
            // It is a SEPARATE handle from music_surface on purpose: the
            // gameplay bus keeps its 4096-frame buffer, so nothing about the
            // crossfade, the blast duck, or the per-frame refill cost changes.
            SetAudioStreamBufferSizeDefault(1411200);
            load_music(SEADS_ASSET_DIR "/audio/music/music_ambient_main.ogg",
                       intro_music, intro_music_ok);

            SetAudioStreamBufferSizeDefault(4096);

            load_music(SEADS_ASSET_DIR "/audio/music/music_ambient_main.ogg",
                       music_surface, music_surface_ok);
            load_music(SEADS_ASSET_DIR "/audio/music/music_deep_tunnel.ogg",
                       music_deep, music_deep_ok);
            load_sound(SEADS_ASSET_DIR "/audio/sfx/black_stope_explosion.wav",
                       stope_boom, stope_boom_ok);
            // The dry blast rides the gameplay bus like everything else. It
            // had no SetSoundVolume at all, so raylib played it at unity --
            // which made it the single loudest thing in the game the moment
            // the music bed came down.
            if (stope_boom_ok)
                SetSoundVolume(stope_boom,
                               static_cast<float>(render::kBoomDryLevel));

            // ⭐ THE BLAST'S REVERB SEND. Decode the SAME asset a second time,
            // as raw samples this time, and hand them to the pure voice; the
            // Sound above still plays the dry blast. raylib owns the disk and
            // the codec, the voice owns the downmix/resample -- the same split
            // the bagpipe drone already uses.
            //
            // Costs ~1.1 MB (13 s of mono 22050 float) for the whole session.
            // Worth it: this is the ONLY way the blast Chad names in the same
            // sentence as the echo can actually echo, because a raylib Sound is
            // mixed inside raylib and exposes no buffer to process.
            if (stope_boom_ok) {
                const char* boom_path =
                    SEADS_ASSET_DIR "/audio/sfx/black_stope_explosion.wav";
                Wave bw = LoadWave(boom_path);
                if (bw.frameCount > 0 && bw.data != nullptr) {
                    float* bs = LoadWaveSamples(bw);  // interleaved float
                    if (bs != nullptr) {
                        boom_send.init(22050.0, bs,
                                       static_cast<std::size_t>(bw.frameCount),
                                       static_cast<int>(bw.channels),
                                       static_cast<double>(bw.sampleRate));
                        UnloadWaveSamples(bs);
                    }
                }
                UnloadWave(bw);
                if (boom_send.ok()) {
                    // ⚠ BACK TO 1024 FIRST. The music loads above raised the
                    // default sub-buffer to 4096, and LoadAudioStream takes
                    // whatever the default is at the moment it is called. The
                    // refill below feeds 1024 frames, and raylib ZERO-FILLS the
                    // rest of the sub-buffer (raudio.c:2693) -- so a 4096-frame
                    // stream fed 1024 plays 25% signal, 75% silence: the echo
                    // becomes a 5.4 Hz chop with ~0.4 s of queue latency, not a
                    // room. This is the exact defect the 1024 pin at the top of
                    // this block exists to prevent, and the comment there
                    // ("nothing after this point creates a 22050 Hz stream")
                    // stopped being true the moment this send was added.
                    SetAudioStreamBufferSizeDefault(1024);
                    boom_send_stream = LoadAudioStream(22050, 16, 1);
                    PlayAudioStream(boom_send_stream);
                    // The send level. The dry blast is already at full through
                    // the Sound path, so this is how much ROOM sits behind it.
                    SetAudioStreamVolume(
                        boom_send_stream,
                        static_cast<float>(render::kBoomSendLevel));
                    boom_send_ok = true;
                    // Restore the deep default: intro_wind below is a 44.1 kHz
                    // Music and would underrun on a frame spike at 1024.
                    SetAudioStreamBufferSizeDefault(4096);
                } else {
                    std::printf(
                        "[AUDIO] stope blast will not echo (send decode "
                        "failed); the dry blast still plays\n");
                }
            }

            // The two pump animals. Same guarded loader as everything else, so
            // a tree that has not run install_soundbank.sh is simply an
            // animal-less map with a printed reason, not a crash.
            //
            // No SetSoundVolume here, unlike the blast above: these are the one
            // channel whose level is a FUNCTION of where you are when it fires
            // (render::cry_gain), so the volume is set at each trigger instead
            // of once at load. Setting a level here as well would be dead code
            // that reads like the mix.
            load_sound(SEADS_ASSET_DIR "/audio/sfx/wolf_cry.wav", wolf_cry,
                       wolf_cry_ok);
            load_sound(SEADS_ASSET_DIR "/audio/sfx/wildcat_cry.wav",
                       wildcat_cry, wildcat_cry_ok);
            // Same story for the train: its level is a function of WHERE you
            // are listening from (ground or air), so it is set at the trigger
            // rather than once here.
            load_sound(SEADS_ASSET_DIR "/audio/sfx/train_chelmsford_pass.wav",
                       train_pass, train_pass_ok);

            // The church bell, and its ring of alias voices. The aliases are
            // built ONLY if the source decoded -- LoadSoundAlias on a zeroed
            // Sound hands back a voice pointing at no sample data, which plays
            // silently and unloads into whatever the uninitialised buffer
            // pointer happens to be.
            load_sound(SEADS_ASSET_DIR "/audio/sfx/church_bell.wav",
                       church_bell, church_bell_ok);
            if (church_bell_ok) {
                for (int v = 0; v < render::kBellVoiceCount; ++v) {
                    church_bell_voice[v] = LoadSoundAlias(church_bell);
                }
            }

            // Intro one-shots. Loaded before the splash loop runs, unloaded
            // straight after it -- they are never referenced again in flight.
            load_music(SEADS_ASSET_DIR "/audio/loops/intro_wind_bed.ogg",
                       intro_wind, intro_wind_ok);
            load_sound(SEADS_ASSET_DIR "/audio/sfx/intro_machine.wav",
                       intro_machine, intro_machine_ok);
            load_sound(SEADS_ASSET_DIR "/audio/sfx/intro_ring_cycle.wav",
                       intro_ring, intro_ring_ok);
            load_sound(SEADS_ASSET_DIR "/audio/sfx/intro_operator.wav",
                       intro_operator, intro_operator_ok);
        }
    }

    // -----------------------------------------------------------------------
    // ⭐ THE LOADING PAGE — the title screen the world is built behind.
    //
    // Chad's readme, and the shape he ruled: TITLE FIRST, music loud, world
    // loading behind it; then black, the music fades out and the wind rises;
    // then the phone call; then gameplay with the music back, quieter.
    //
    // Everything between here and the intro loop is the world build. It used to
    // run against a blank window with nothing playing, and the title card came
    // at the END of the intro, which is the opposite of what he wrote.
    //
    // pump_loading() is what makes a straight-line load into a page: it
    // advances the page clock, refills the deep-buffered bed, and presents the
    // title. Call it at every stage boundary below. It is a no-op in a --smoke
    // run (no audio device, and no screenshot gate may inherit these frames).
    // -----------------------------------------------------------------------
    auto to_rgba = [](const glm::dvec3& c, double alpha) {
        auto ch = [](double v) {
            return static_cast<unsigned char>(std::clamp(v, 0.0, 1.0) * 255.0 +
                                              0.5);
        };
        return Color{ch(c.r), ch(c.g), ch(c.b), ch(alpha)};
    };
    // The title card, drawn by BOTH the loading page and the handoff at the top
    // of the intro (where it dissolves into the black). One lambda so the two
    // cannot drift into two different title screens.
    //
    // Chad's thematic pair, 2026-08-17: "the splash screen needs to have our
    // thematic orange / blue colors that are opposite on the color wheel and
    // complimentary.. Aleady chosen and predefined in this game." They are
    // render::kSlagOrange and render::kComplementBlue, the faction palette in
    // render/team_color.h — READ FROM THERE, never retyped, because that header
    // exists precisely because the slag orange used to live only inside a GLSL
    // string and any second surface painting "the slag orange" was a fork
    // waiting to drift (H1).
    //
    // ⚠ The blue is DERIVED, not picked: the exact 180 deg HSV complement at
    // equal S and V. The eye-plausible channel reversal (0.12, 0.55, 1.00) is
    // NOT it — that lands 181.36 deg away, an error invisible on screen that
    // would quietly make "precisely opposite" a lie. test_team_color.cpp pins
    // the 180 deg to 1e-9.
    auto draw_title_card = [&](double alpha) {
        if (!(alpha > 0.0)) return;
        const int sw = GetScreenWidth();
        const int sh = GetScreenHeight();
        const int fs = 64;
        const int w = MeasureText(render::kGameTitle, fs);
        const int x = (sw - w) / 2;
        const int y = sh / 2 - fs;

        DrawText(render::kGameTitle, x, y, fs,
                 to_rgba(render::kComplementBlue, alpha));

        // The slag-orange rule: the complement, directly under the title, so
        // the pair reads as a pair. Grows in with the fade.
        const int rule_w = static_cast<int>(w * std::clamp(alpha, 0.0, 1.0));
        DrawRectangle(x + (w - rule_w) / 2, y + fs + 18, rule_w, 3,
                      to_rgba(render::kSlagOrange, alpha));

        const int ss = 20;
        const int sub_w = MeasureText(render::kGameSubtitle, ss);
        DrawText(render::kGameSubtitle, (sw - sub_w) / 2, y + fs + 40, ss,
                 to_rgba(render::kSlagOrange, alpha * 0.85));
    };
    // Not pure black: a deep slate carrying a trace of the ally blue, so the
    // orange rule reads as warm against something rather than floating in a
    // void. Shared by the loading page and the intro so the handoff to black is
    // a fade of the TITLE, not a change of background.
    const Color kIntroBackdrop{6, 9, 14, 255};

    double load_t = 0.0;
    double load_t0 = 0.0;         // GetTime() when the page first drew
    double load_prev = 0.0;       // GetTime() at the end of the previous pump
    const char* load_stage = "";  // name of the stage currently running
    bool load_first_pump = true;
    // Opt-in opening-sequence trace. The sub-buffer above is sized against the
    // LONGEST gap between two pumps, so that gap is a number worth being able
    // to read: SEADS_LOADPROF=1 prints it per stage, and prints every intro cue
    // as it fires. Measure before resizing anything — and this is the ONLY way
    // to see the opening at all, since no ctest ever runs this binary and the
    // last intro bug (the first-frame dt eating the whole sequence) shipped
    // with a fully green gate.
    const bool load_prof = std::getenv("SEADS_LOADPROF") != nullptr;
    if (load_prof) {
        // The mix, as shipped. No ctest can see these -- nothing in the gate
        // links seads.exe -- so this readout is the only instrument that says
        // what Chad is actually about to hear, and the only way a fly report
        // ("still too loud") can be attributed to a number rather than a mood.
        std::printf(
            "[MIX] loading %.3f | music %.3f (deep %.3f) | wind %.3f | "
            "drone %.3f | guns %.3f | sfx %.3f | blast %.3f (echo %.3f)\n",
            render::kLoadMusicGain, render::kMusicSurfaceGain,
            render::kMusicDeepGain, render::kWindStreamTrim,
            render::kEngineStreamTrim, render::kGunStreamLevel,
            render::kSfxStreamLevel, render::kBoomDryLevel,
            render::kBoomSendLevel);
        std::fflush(stdout);
    }
    auto pump_loading = [&](const char* stage) {
        if (smoke_frames > 0) return;
        // ⚠ GetTime(), NOT GetFrameTime(). raylib sets CORE.Time.frame in
        // EndDrawing (rcore.c:944) as update+draw, where `update` was measured
        // back in BeginDrawing — so a GetFrameTime() read taken BEFORE
        // BeginDrawing reports the frame BEFORE last, i.e. it attributes each
        // stage's duration to the pump after the one that followed it. That
        // first cost me the whole feature: the page clock lagged a full stage,
        // so load_title_alpha() was still ~0 when the 21 s stage began and the
        // title was invisible black for the entire load, then snapped up in the
        // last instants. The absolute clock has no such phase.
        const double now = GetTime();
        const bool first = load_first_pump;
        if (load_first_pump) {
            load_first_pump = false;
            load_t0 = now;    // the page clock starts when the page does
            load_prev = now;  // ...and the first stage has not happened yet
        }
        load_t = now - load_t0;
        // Print the name of the stage that just RAN, not the one about to.
        // ⚠ The first version printed the gap against the UPCOMING label, and I
        // read the 21.9 s off the wrong line and blamed building_colliders for
        // planet_heightfield's cost in the handoff. An instrument that needs a
        // mental off-by-one to read is an instrument that will be misread.
        // stage == nullptr = a frame of the fade-in hold, which has no stage
        // behind it and would otherwise print ~100 identical zero lines.
        // `first` has no stage behind it -- it would always print 0.000 s
        // against a label, which reads as a measurement and is not one.
        if (stage != nullptr) {
            if (load_prof && !first) {
                std::printf("[LOADPAGE] %7.3f s  %s\n", now - load_prev,
                            load_stage);
                // Flushed, not buffered. Redirected to a file, a 4 kB stdio
                // buffer holds the whole trace hostage until the process
                // exits -- which, for a window you close by hand, is never.
                // An instrument you cannot read while it runs is not one.
                std::fflush(stdout);
            }
            // Advanced OUTSIDE the print guard: the first pump prints
            // nothing but must still name the stage it is starting, or
            // every later line would be labelled one stage early.
            load_stage = stage;
        }
        // Present FIRST, refill second. The first refill decodes a full 64 s of
        // Vorbis (see the sub-buffer note above) and takes a moment; doing it
        // after the draw means the title is already on screen while it happens,
        // instead of the window staying blank until the music is ready.
        BeginDrawing();
        ClearBackground(kIntroBackdrop);
        draw_title_card(render::load_title_alpha(load_t));
        EndDrawing();
        if (intro_music_ok) {
            UpdateMusicStream(intro_music);
            SetMusicVolume(intro_music,
                           static_cast<float>(render::kLoadMusicGain));
        }
        load_prev = GetTime();
    };

    // ⭐ Bring the title UP BEFORE the world build, not during it. The build is
    // one 21 s uninterruptible call; if the fade-in were left to run against
    // the pump points, the page would spend that whole stage at whatever alpha
    // it had reached — which is zero, because the first pump is the first frame
    // of the fade. Hold here until the card is fully lit, then load behind it.
    // ~1.6 s, and it is the only part of the page a fast machine could lose.
    if (smoke_frames == 0) {
        pump_loading("render build params");
        while (!WindowShouldClose() && render::load_title_alpha(load_t) < 1.0)
            pump_loading(nullptr);
    }

    // T2 portal surgery: the mouth cut disks carved into the terrain mesh at
    // load — one per mouth, radius = kMouthCutFactor*tube_radius. The CUT
    // radius controls which triangles are dropped; the COLLAR outer radius is
    // larger (derived by collar_reach() in tunnel_mesh.h from the terrain cell
    // diagonal) so the funnel spans the full jagged terrain edge. Only under
    // the [tunnel] gate; empty otherwise => bit-identical terrain.
    std::vector<render::CutDisk> planet_cuts;
    if (game.tunnel.enabled) {
        // T4a: per-mouth cut radii. Errington keeps the tube-scaled portal
        // hole; Murray is an OPEN PIT so its cut spans the bowl opening. Routed
        // through the world:: single-source helpers (m18 pin: the Murray cut
        // must == bowl_radius, never the tube radius).
        // T6c: the Errington cut spans the ENTRY PIT rim (the recess crater),
        // not a tube-scaled portal hole. The pit rim ==
        // kPitRimFactor*tube_width (see build_tunnel_net::net.pit.bowl_r);
        // route it through the single-source helper so the terrain hole and the
        // pit wall can never disagree (P2-1).
        const double err_pit_rim =
            world::errington_pit_rim(game.tunnel.tube_width_m);
        const double err_cut =
            world::errington_cut_radius(err_pit_rim, render::kMouthCutFactor);
        const double mur_cut =
            world::murray_cut_radius(game.tunnel.bowl_radius_m);
        planet_cuts.push_back({world::kTunnelMouthErrington, err_cut});
        planet_cuts.push_back({world::kTunnelMouthMurray, mur_cut});
        // T13 — THE ENTRY APPROACH TRENCH (S3): open-cut disks marching
        // up-tangent from the Errington pit. Routed through the SAME
        // world::errington_trench_steps the SDF Bowls use, so the removed
        // terrain == the survivable open-cut volume (the T6-P0 discipline). The
        // tree scatter reuses this cut list below, so the trench footprint is
        // tree-cleared identically (single source). trench_len_m <= 0 => an
        // empty list => no extra cuts (bit-identical terrain).
        const std::vector<world::TrenchStep> trench_steps =
            world::errington_trench_steps(
                world::kTunnelMouthErrington, world::kTunnelMouthMurray,
                game.tunnel.trench_len_m, game.tunnel.trench_rim_m,
                game.tunnel.mouth_sink_m, params.R);
        for (const world::TrenchStep& s : trench_steps)
            planet_cuts.push_back({s.dir, s.rim_m});
    }
    // Feed the world config into the render layer BEFORE the first frame builds
    // the planet (no bare geometry numbers in render/ — world_build_plan §1).
    // ★ One law, both consumers (INV-9): the shader gets the SAME shed curve
    // the snowpack applies to depth, from the same [snowpack] config -- now
    // including the SLOPE GATE (Chad's fly ruling 2026-08-11). The degree->x
    // conversion happens once, in world::SnowParams; nowhere else.
    render::set_barren_shed_law(
        world.snowpack.k_barren, world.snowpack.barren_shed_lo,
        world.snowpack.barren_shed_hi, world.snowpack.barren_slope_x_lo(),
        world.snowpack.barren_slope_x_hi(),
        world.snowpack.barren_flat_shed_frac);
    render::set_planet_build_params(
        {.relief_scale = world.ground.relief_scale_m,
         .subdiv = world.planet.subdiv,
         .tiles = world.planet.tiles,
         .u_offset = world.planet.u_offset,
         .dem_blur_radius = world.planet.dem_blur_radius,
         .cubemap_size = world.planet.cubemap_size,
         .procedural = world.ground.procedural,
         .water_reflectivity = world.water.reflectivity,
         .water_sparkle = world.water.sparkle_sharpness,
         .water_surface_lift_m = world.water.surface_lift_m,
         .water_star_reflect = world.water.star_reflect,
         .water_star_density = world.water.star_density,
         .cuts = planet_cuts});
    // S2 boreal tree scatter (stereoscope-sudbury). [trees] fly-dials -> the
    // render POD; draw.cpp translates to world::TreeParams + render::PropLook.
    render::set_tree_build_params(
        {.enabled = world.trees.enabled,
         .density_gain = world.trees.density_gain,
         .min_scale = world.trees.min_scale,
         .max_scale = world.trees.max_scale,
         .tree_height_m = world.trees.height_m,
         .base_width_m = world.trees.base_width_m,
         .render_range_m = world.trees.render_range_m,
         .ambient = world.trees.ambient,
         .diffuse = world.trees.diffuse,
         .fade_frac = world.trees.fade_frac,
         .cells_per_face = world.trees.cells_per_face,
         .chunk_cells = world.trees.chunk_cells,
         // T6 scatter exclusion: reuse the same cut
         // disk list so the tree hole matches the
         // terrain hole exactly (single source).
         // S2b: the trail corridor clears the canopy
         // (WINTER_LAW §2.4b, Chad's S2 fly).
         .corridor_margin_m = world.trees.corridor_margin_m,
         .cuts = planet_cuts,
         // SF1: the snowhill mask sources bind LATE
         // (set_tree_snowhill, after planet build)
         .snowhill = {},
         .snowhill_hf = nullptr});
    // ★ ROAD-REPAIR ONAPING RUNG 2 -- THE APRON KILL, read ONCE. Both
    // consumers of [bank_mesh] apron_m in this process (the drawn mesh, via
    // the render POD just below, and the SEADS_ROAD_CENSUS ruler further down)
    // take this value, so `SEADS_NO_APRON=1` is one switch that turns the
    // apron off in the seat AND in the instrument -- which is exactly how the
    // before/after table is taken with one exe and one build.
    const bool no_apron = std::getenv("SEADS_NO_APRON") != nullptr;
    const double apron_m_live = no_apron ? 0.0 : world.bank_mesh.apron_m;
    // ★ ROAD-REPAIR F1 -- THE DECK-YIELD KILL, the apron kill's exact shape
    // and for the same reason: ONE env, read ONCE, so the A/B Chad flies and
    // the numbers the build logs are the same switch. 0.0 is the identity, so
    // SEADS_NO_DECK_YIELD=1 is bit-for-bit the pre-F1 bank mesh.
    const bool no_deck_yield = std::getenv("SEADS_NO_DECK_YIELD") != nullptr;
    const double deck_yield_m_live =
        no_deck_yield ? 0.0 : world.bank_mesh.deck_yield_m;
    // ★ ROAD-REPAIR ONAPING SINK -- THE SUBDIVISION DIAL, read ONCE, the
    // apron kill's exact shape. Both consumers in this process take this
    // value: the drawn drape (via the render POD just below) and the
    // SEADS_RIBBON_SAG ruler further down. So SEADS_RIBBON_MAXSEG=0 turns the
    // subdivision off in the seat AND in the instrument, and the before/after
    // table is two readings of ONE ruler on ONE exe.
    double ribbon_max_seg_live = world.ribbons.max_seg_m;
    if (const char* e = std::getenv("SEADS_RIBBON_MAXSEG"))
        ribbon_max_seg_live = std::max(0.0, std::atof(e));
    double ribbon_max_tr_live = world.ribbons.max_tr_m;
    if (const char* e = std::getenv("SEADS_RIBBON_MAXTR"))
        ribbon_max_tr_live = std::max(0.0, std::atof(e));
    // ★ ROAD-REPAIR F2 -- THE JUNCTION-CUT KILL, the apron kill's exact shape
    // and for the same reason: ONE env, read ONCE, so the A/B Chad flies and
    // the numbers the build logs are the same switch. 0.0 is the identity by
    // an explicit branch in render/ribbon_junction.h, so
    // SEADS_NO_JUNCTION_CUT=1 is bit-for-bit the pre-F2 drape.
    const bool no_junction_cut =
        std::getenv("SEADS_NO_JUNCTION_CUT") != nullptr;
    double ribbon_junction_cut_live =
        no_junction_cut ? 0.0 : world.ribbons.junction_cut_m;
    if (!no_junction_cut)
        if (const char* e = std::getenv("SEADS_JUNCTION_CUT"))
            ribbon_junction_cut_live = std::max(0.0, std::atof(e));
    // ★ ROAD-REPAIR F3 -- THE OVER-BANK BIAS, the SEADS_RIBBON_MAXSEG
    // pattern: read ONCE here so the seat A/B and any instrument in this
    // process take the same value. SEADS_OVER_BANK_BIAS=0 is the KILL
    // (bit-for-bit the pre-F3 deck pass, by the branch in
    // render/ribbons.cpp); any other value sweeps it.
    double ribbon_over_bank_bias_live = world.ribbons.over_bank_bias;
    if (const char* e = std::getenv("SEADS_OVER_BANK_BIAS"))
        ribbon_over_bank_bias_live = std::max(0.0, std::atof(e));
    // SF2-BANKS [bank_mesh] -> the render POD (§3.6c).
    render::set_bank_build_params(
        {.enabled = world.bank_mesh.enabled,
         .station_m = world.bank_mesh.station_m,
         .skirt_m = world.bank_mesh.skirt_m,
         .skirt_bury_m = world.bank_mesh.skirt_bury_m,
         .speckle_density = world.bank_mesh.speckle_density,
         .speckle_dark = world.bank_mesh.speckle_dark,
         .speckle_aa = world.bank_mesh.speckle_aa,
         .crest_smudge = world.bank_mesh.crest_smudge,
         .min_amp_m = world.bank_mesh.min_amp_m,
         .skirt_rings = world.bank_mesh.skirt_rings,
         .chord_tol_m = world.bank_mesh.chord_tol_m,
         .junction_station_m = world.bank_mesh.junction_station_m,
         .apron_m = apron_m_live,
         .apron_tol_m = world.bank_mesh.apron_tol_m,
         .apron_min_drop_m = world.bank_mesh.apron_min_drop_m,
         .deck_yield_m = deck_yield_m_live});
    // S3 draped linework ribbons (stereoscope-sudbury). [ribbons] look dials ->
    // the render POD; the geometry is baked (render/sudbury_gis.gen.h). Roads
    // mono, the snowmobile trail the one sanctioned world-chroma.
    render::set_ribbon_build_params(
        {.enabled = world.ribbons.enabled,
         .lift_m = world.ribbons.lift_m,
         .max_seg_m = ribbon_max_seg_live,
         .max_tr_m = ribbon_max_tr_live,
         .junction_cut_m = ribbon_junction_cut_live,
         .over_bank_bias = ribbon_over_bank_bias_live,
         .line_aa = world.ribbons.line_aa,
         .road_bed = world.ribbons.road_bed,
         .road_line = world.ribbons.road_line,
         .road_center_frac = world.ribbons.road_center_frac,
         .road_dash_m = world.ribbons.road_dash_m,
         .road_gap_m = world.ribbons.road_gap_m,
         .road_mottle = world.ribbons.road_mottle,
         .road_mottle_frac = world.ribbons.road_mottle_frac,
         .road_fade = world.ribbons.road_fade,
         .trail_color = world.ribbons.trail_color,
         .trail_mottle = world.ribbons.trail_mottle,
         .trail_winter_color = world.ribbons.trail_winter_color,
         .trail_corduroy = world.ribbons.trail_corduroy,
         .trail_corduroy_m = world.ribbons.trail_corduroy_m,
         // T24: the ribbons yield to the SAME excavation cuts as the
         // terrain + trees (single source) — no draped line hovers across
         // the Errington pit/trench void.
         .cuts = planet_cuts});
    // S4 building massing (stereoscope-sudbury). [buildings] look dials -> the
    // render POD; the geometry (footprints/heights/roofs) is baked
    // (render/sudbury_gis.gen.h). MONO (silvered by the S1 post); sun-lit flat
    // facets read as blocks from the air.
    render::set_building_build_params(
        {.enabled = world.buildings.enabled,
         .wall_val = world.buildings.wall_val,
         .roof_val = world.buildings.roof_val,
         .ambient = world.buildings.ambient,
         .diffuse = world.buildings.diffuse,
         .height_scale = world.buildings.height_scale,
         .window_color = world.buildings.window_color,
         .window_bright = world.buildings.window_bright,
         .window_lit_frac = world.buildings.window_lit_frac});
    // Street-lamp night point lights (stereoscope-sudbury). [streetlamps] look
    // dials
    // -> the render POD; positions are baked (render/sudbury_gis.gen.h).
    render::set_lamp_build_params({.enabled = world.streetlamps.enabled,
                                   .color = world.streetlamps.color,
                                   .brightness = world.streetlamps.brightness,
                                   .core_bright = world.streetlamps.core_bright,
                                   .core_sharp = world.streetlamps.core_sharp,
                                   .size_m = world.streetlamps.size_m,
                                   .min_px = world.streetlamps.min_px});
    // Precipitation (W3). [precip] look dials -> the render POD; the particle
    // lattice is procedural (render/precip.cpp). Season-gated + weather-gated;
    // the per-frame wrapped fall phase + weather intensity are set below (main
    // loop).
    render::set_precip_build_params(
        {.enabled = world.precip.enabled,
         .cell_size_m = world.precip.cell_size_m,
         .box_half_m = world.precip.box_half_m,
         .wrap_fade = world.precip.wrap_fade,
         .color = world.precip.color,
         .snow_size_m = world.precip.snow_size_m,
         .snow_speed_mps = world.precip.snow_speed_mps,
         .snow_opacity = world.precip.snow_opacity,
         .rain_size_m = world.precip.rain_size_m,
         .rain_streak_m = world.precip.rain_streak_m,
         .rain_rate_hz = world.precip.rain_rate_hz,
         .rain_opacity = world.precip.rain_opacity,
         // AS-1/AS-3 (atmosphere rung): per-flake variety, the density law, the
         // zero-mean sway, the contrast rim, the underground band and the FAR
         // veil lattice. Every one is OFF at 0 (rim_dark at 1).
         .size_var = world.precip.size_var,
         .alpha_var = world.precip.alpha_var,
         .density_exp = world.precip.density_exp,
         .density_soft = world.precip.density_soft,
         .sway_m = world.precip.sway_m,
         .rim_dark = world.precip.rim_dark,
         .rock_band_m = world.precip.rock_band_m,
         .veil_enabled = world.precip.veil_enabled,
         .veil_cell_size_m = world.precip.veil_cell_size_m,
         .veil_box_half_m = world.precip.veil_box_half_m,
         .veil_size_m = world.precip.veil_size_m,
         .veil_opacity = world.precip.veil_opacity,
         .veil_sway_m = world.precip.veil_sway_m,
         .veil_inner_fade_m = world.precip.veil_inner_fade_m});
    // CC1 Superstack smoke (Living Copper Cliff). [smoke] look dials -> the
    // render POD; the plume anchor (the Superstack axis dir + 381 m) is baked
    // into draw.cpp from the LOCKED projection. The per-frame wrapped phase is
    // set below (main loop).
    render::set_smoke_build_params(
        {.enabled = world.smoke.enabled,
         .puffs = static_cast<int>(world.smoke.puffs),
         .color = world.smoke.color,
         .rise_m = world.smoke.rise_m,
         .drift_m = world.smoke.drift_m,
         .r0_m = world.smoke.r0_m,
         .r1_m = world.smoke.r1_m,
         .opacity = world.smoke.opacity,
         .jitter_m = world.smoke.jitter_m,
         .wind = world.smoke.wind});
    const double smoke_rate =
        world.smoke.rate_hz;  // plume cycle rate (per s of t_cel)
    // CC2 slag-pot trains (Living Copper Cliff). [train] look dials -> the
    // render POD; the spur + loco/pot meshes are procedural in
    // render/train.cpp. The per-frame wrapped loco-head phase is set below in
    // the main loop.
    render::set_train_build_params({.enabled = world.train.enabled,
                                    .pots = static_cast<int>(world.train.pots),
                                    .car_gap_m = world.train.car_gap_m,
                                    .tip_span_m = world.train.tip_span_m,
                                    .lift_m = world.train.lift_m,
                                    .color = world.train.color,
                                    .ambient = world.train.ambient,
                                    .diffuse = world.train.diffuse});
    const double train_rate =
        world.train.rate_hz;  // loop cycles per s of t_cel
    // CC3 slag pour (Living Copper Cliff) — the marquee. [slag] look dials ->
    // the render POD; the mound/pot/lava are procedural in render/slag.cpp. The
    // per-frame wrapped pour phase is set below in the main loop.
    render::set_slag_build_params(
        {.enabled = world.slag.enabled,
         .mound_radius_m = world.slag.mound_radius_m,
         .mound_top_r_m = world.slag.mound_top_r_m,
         .mound_height_m = world.slag.mound_height_m,
         .ridge_length_m = world.slag.ridge_length_m,
         .crest_width_m = world.slag.crest_width_m,
         .face_angle_deg = world.slag.face_angle_deg,
         .benches = static_cast<int>(world.slag.benches),
         .mound_color = world.slag.mound_color,
         .ambient = world.slag.ambient,
         .diffuse = world.slag.diffuse,
         .rivers = static_cast<int>(world.slag.rivers),
         .river_halfwidth_m = world.slag.river_halfwidth_m,
         .glow = world.slag.glow,
         .night_boost = world.slag.night_boost,
         .pour_rate_hz = world.slag.pour_rate_hz,
         .lift_m = world.slag.lift_m});
    const double pour_rate =
        world.slag.pour_rate_hz;  // pour cycles per s of t_cel
    // S-mapteam (Chad 2026-08-09): the ONE faction palette -> render, ONCE and
    // BEFORE any draw. Every faction-coloured pixel in the game reads this:
    // the map tactical layer AND the world draw (aircraft livery, ALLY/BANDIT
    // tags, Chad's callsign tag, the 3D pump beacons) AND the slag lava shader
    // (teams.enemy IS the heatColor() hot stop, injected at shader build). It
    // must be pushed before ensure_slag()/the first frame, which it is - this
    // block runs during startup config wiring.
    render::set_team_colors({.ally = world.teams.ally,
                             .enemy = world.teams.enemy,
                             .objective = world.teams.objective});
    // M-KEY tactical chart (S-mapread, Chad 2026-08-09): [map] look/declutter
    // dials -> render::MapStyle, ONCE. Both map TUs (tourist_map.cpp's
    // greyscale basemap, draw.cpp's coloured tactical layer) read this same
    // struct, so a colour can never fork between the two layers.
    render::set_map_style(
        {.paper = world.map.paper,
         .ink = world.map.ink,
         .water = world.map.water,
         .basemap_ink = world.map.basemap_ink,
         .lake_label_span_m = world.map.lake_label_span_m,
         .trails_visible = world.map.trails_visible,
         .minor_roads_visible = world.map.minor_roads_visible,
         .rivers_visible = world.map.rivers_visible,
         .zone_labels_visible = world.map.zone_labels_visible,
         .place_labels_visible = world.map.place_labels_visible,
         .neutral_color = world.map.neutral_color,
         .outline_color = world.map.outline_color,
         .marker_px = world.map.marker_px,
         .objective_px = world.map.objective_px,
         .player_px = world.map.player_px,
         .outline_px = world.map.outline_px,
         .territory_fill = world.map.territory_fill,
         .territory_outline_px = world.map.territory_outline_px,
         .arrow_hold_sin = world.map.arrow_hold_sin,
         .trails_zoom = world.map.trails_zoom,
         .minor_roads_zoom = world.map.minor_roads_zoom,
         .place_labels_zoom = world.map.place_labels_zoom,
         .zoom_step = world.map.zoom_step,
         .zoom_max = world.map.zoom_max,
         .follow_zoom = world.map.follow_zoom});
    // S-pumpcube (Chad 2026-08-09): the neon team wire cube that frames a pump
    // so it reads as CLAIMED in the unlit stope. Geometry/strength only — the
    // hues come from set_team_colors() above, one palette, never two.
    render::set_pump_frame_style(
        {.enabled = world.pump_frame.enabled,
         .deep_only = world.pump_frame.deep_only,
         .half_extent_m = world.pump_frame.half_extent_m,
         .layers = world.pump_frame.layers,
         .layer_step_m = world.pump_frame.layer_step_m,
         .brightness = world.pump_frame.brightness,
         .neutral_color = world.pump_frame.neutral_color});
    // S1 stereoscope post pass: map the [tone] look dials (config edge,
    // doubles) into the render PostParams once. The post pass is the ONE tone
    // owner; the scene is authored mono, the planes are the only chroma
    // (render/post.*). S6 far-field-only DoF rides the same PostParams: [dof]
    // enabled=0 or SEADS_NO_DOF=1 forces radius 0 (the shader's off-switch —
    // A/B + degraded).
    const bool no_dof_env = [] {
        const char* e = std::getenv("SEADS_NO_DOF");
        return e != nullptr && e[0] != '\0' && e[0] != '0';
    }();
    const float dof_radius = (world.dof.enabled && !no_dof_env)
                                 ? static_cast<float>(world.dof.radius_px)
                                 : 0.0f;
    // S6 "printed card" halftone/dither MODE: config [halftone] enabled+style,
    // with a SEADS_HALFTONE=0|1|2 launch override for A/B (0 forces off; 1
    // dots; 2 dither). mode 0 -> the post shader skips the block (bit-exact
    // identity).
    const int halftone_mode = [&world]() -> int {
        const char* e = std::getenv("SEADS_HALFTONE");
        if (e != nullptr && e[0] != '\0') {
            if (e[0] == '1') return 1;
            if (e[0] == '2') return 2;
            return 0;  // "0" (or anything else) -> forced off
        }
        return world.halftone.enabled ? world.halftone.style : 0;
    }();
    render::set_post_params(
        {.contrast = static_cast<float>(world.tone.contrast),
         .lift = static_cast<float>(world.tone.lift),
         .grain = static_cast<float>(world.tone.grain),
         .split_shadow = glm::vec3(world.tone.split_shadow),
         .split_hi = glm::vec3(world.tone.split_hi),
         .split_hi_night = glm::vec3(world.tone.split_hi_night),
         .sat_c0 = static_cast<float>(world.tone.sat_c0),
         .sat_c1 = static_cast<float>(world.tone.sat_c1),
         .sat_dark = static_cast<float>(world.tone.sat_dark),
         .halation = static_cast<float>(world.tone.halation),
         .halation_threshold =
             static_cast<float>(world.tone.halation_threshold),
         .halation_tint = glm::vec3(world.tone.halation_tint),
         .vignette = static_cast<float>(world.tone.vignette),
         .fxaa = static_cast<float>(world.tone.fxaa),
         .focus_start = static_cast<float>(world.dof.focus_start_m),
         .focus_end = static_cast<float>(world.dof.focus_end_m),
         .dof_radius = dof_radius,
         .sky_coc = static_cast<float>(world.dof.sky_coc),
         .halftone_mode = halftone_mode,
         .halftone_scale = static_cast<float>(world.halftone.scale_px),
         .halftone_angle = static_cast<float>(world.halftone.angle_rad),
         .halftone_soft = static_cast<float>(world.halftone.soft),
         .halftone_ink = static_cast<float>(world.halftone.ink),
         .halftone_grain_mul = static_cast<float>(world.halftone.grain_mul)});
    if (smoke_frames == 0) DisableCursor();  // relative mouse for aim

    // Device + camera state the CALLER owns (persistent, like held keys). The
    // per-tick sim/instructor state lives in `loop` (app::instructor_tick.h).
    input::RawDeviceState raw_dev;
    input::LiveDeviceState live_dev;
    render::CameraOrbit orbit;
    // S-globelook (v4 rung 3): the freelook globe-inertia state — the coast
    // velocity + hand-rate EMA that lets the held orbit glide like a spun
    // globe. Caller-owned persistent beside `orbit` (the pure law lives in
    // render::OrbitInertia; this loop owns only the glue). Reset EVERYWHERE
    // the orbit is zeroed/eased (orient verb, respawn, and every
    // freelook-released frame — which also covers focus loss, raw mode, and
    // smoke, since those all read a default `live`), so a dead life's spin
    // can never haunt the reborn camera. Cosmetic (§9.2): never feeds
    // mouse->aim.
    render::OrbitInertia orbit_inertia;
    // S-orient (docs/comfort program Q3): the freelook double-tap detector +
    // the press-edge tracker feeding it. Both caller-owned persistent state
    // (like `orbit`); the detector is PURE (input::OrientTap). `prev_freelook`
    // builds the rising edge from the SAME device sample freelook reads
    // (live.freelook_held), so the edge and the hold can never disagree.
    input::OrientTap orient_tap;
    bool prev_freelook = false;
    // S-orient P0-1: the orient fire is caller-latched across 0-tick frames,
    // the SAME pending pattern as pending_dx/dy. orient_tap.step() consumes the
    // double-tap ON DETECTION, but the fire is only DELIVERED to the tick if
    // the frame actually runs >= 1 tick (step_frame offers it on the first
    // tick). If the detecting frame ran 0 ticks (render fps > sim), the fire
    // would be lost. So we OR the per-frame detection into `pending_orient`,
    // offer THAT to step_frame each frame, and clear it only after a frame that
    // ran ticks (fr.ticks > 0). Cleared on focus-loss / F1 / respawn alongside
    // orient_tap.reset().
    bool pending_orient = false;
    // RMB-hold gunsight zoom (2026-07-07): eased amount, 0 = resting (60 fov),
    // 1 = fully zoomed (kZoomFovyDeg). Persistent like `orbit`; cosmetic and
    // DOWNSTREAM of the aim (RA9). Snapped to 0 on respawn (a clean rebirth,
    // like the orbit/cam_fwd resets).
    double zoom_t = 0.0;
    // Freelook mousewheel dolly-out (2026-07-10, Chad): the zoom-out level, 0 =
    // default chase (identical to the mouse-aim view), 1 = full wheel-out.
    // RESET to 0 on every freelook ENTRY (prev_freelook rising edge) so each
    // press starts at the default and you scroll out fresh; adjusted by the
    // wheel only while freelook is held; applied to the chase framing ONLY in
    // freelook (combat is always the default). Cosmetic + DOWNSTREAM of the aim
    // (RA9 — freelook drives the orbit, never mouse->aim). Snapped to 0 on
    // respawn.
    double dolly_t = 0.0;
    // (prev_freelook — the freelook rising-edge detect — is declared once with
    // the OrientTap block above; it serves both the freelook-entry dolly reset
    // and the v4 orient double-tap edge.)
    // S-reticle (Chad 2026-07-08): the DISPLAY-EASED reticle direction —
    // render::reticle_smooth hides the integer-mouse staircase on slow
    // sweeps. Persistent like `orbit`; read ONLY into info.reticle_dir (the
    // reticle draw), NEVER into loop.aim/cam_fwd/control (RA9). The hard lag
    // cap makes any snap (first frame, respawn, freelook release) self-heal
    // within one frame; the respawn reseed below is hygiene, not correctness.
    glm::dvec3 reticle_dir{0.0, 0.0, -1.0};
    // The re-center gate, EASED (not stepped) across the freelook edge: 1 in
    // mouse-aim (re-center on), 0 in freelook (narrow in place). A hard step
    // would whip the camera when Space is tapped mid-zoom (render_fwd snapping
    // between the pipper and the tens-of-degrees-lagged cam_fwd) — the one
    // discontinuity in an otherwise all-eased camera (Fable red-team P1-2).
    // recenter_t = recenter_gate * zoom_t, so an unzoomed zoom_t=0 already
    // zeroes it (no separate respawn reset needed). Cosmetic (RA9): render
    // only.
    double recenter_gate = 1.0;

    app::Accumulator accum(params.sim_dt, kMaxFrameDt);
    const render::ChaseParams chase;
    app::LoopState loop;
    // Optional spawn-altitude override (SEADS_SPAWN_ALT, metres) for the
    // screenshot/probe rig and ground-framing shots; defaults to the 2 km
    // cruise spawn. Read caller-side ONLY (never inside spawn_state — the
    // tick's crash branch calls that on the tested path, which must stay
    // env-free). Applies to the INITIAL spawn; a mid-run crash respawns at the
    // shipped 2 km default.
    double spawn_alt = 2000.0;
    bool spawn_alt_overridden = false;
    if (const char* sa = std::getenv("SEADS_SPAWN_ALT")) {
        const double v = std::atof(sa);
        if (v > 0.0) {
            spawn_alt = v;
            spawn_alt_overridden = true;
        }
    }
    if (probe_mode) spawn_alt = probe_alt;  // the probe arg is authoritative
    // Tunnel world home spawn (Chad's ruling, 2026-07-18): born OVER ERRINGTON
    // MINE, nose toward Murray — the raid is dead ahead — and crash/death
    // respawns come home too (LoopState spawn spec; the tick stays env-free).
    // Tunnel disabled => the legacy +X-pole spawn bit-identically.
    if (game.tunnel.enabled) {
        // ★★★ L11b -- THE HOME IS THE SIDE'S HOME (app/spawn_policy.h). This
        // block used to type Errington and Murray in directly, with no faction
        // term at all, so a Central City player was still born over the
        // Valley. The config side is the one known here (the menu has not run
        // -- no window yet); the menu's side answer re-anchors and re-spawns
        // on the first frame, exactly as its Snowmachine answer re-places both
        // bodies. faction 0 returns the two vectors this block used to type.
        const int cfg_fac = (game.conquest.player_faction == "valley")
                                ? combat::CQ_VALLEY
                                : combat::CQ_SUDBURY;
        loop.spawn_up = app::faction_home_dir(cfg_fac);
        loop.spawn_fwd = app::faction_home_fwd(cfg_fac);
        if (!spawn_alt_overridden && !probe_mode) spawn_alt = 2500.0;
        loop.spawn_alt = spawn_alt;
    }
    // S-airdome (docs/bubble_atmosphere_spec.md §4, a SANCTIONED smoke-only
    // hook following the SEADS_SMOKE_GEAR / SEADS_OBLIQUE pattern):
    // SEADS_SMOKE_SPAWN_DIR="x,y,z" overrides the spawn DIRECTION (any
    // non-zero vector, normalized) so the screenshot rig can place the
    // aircraft over an arbitrary point (a bubble core, the vacuum gap, an
    // antipode) without touching the shipped tunnel-home spawn. Ignored
    // unless smoke_frames > 0. The facing is a deterministic tangent (project
    // world -Z off the new up, falling back to +X if degenerate) — direction
    // doesn't matter for a sky/atmosphere establishing shot, only position +
    // altitude (SEADS_SPAWN_ALT) do.
    // ★ ROAD-REPAIR ONAPING: the spawn override was SMOKE-ONLY, and that is
    // why the CENSUS_FINAL §1 paste blocks never actually moved an
    // INTERACTIVE ride to Onaping -- without `--smoke N shot.png` this whole
    // branch was skipped and he spawned at the tunnel home. SEADS_ONAPING_SMOKE
    // arms it for a live ride too; nothing else changes, and with the flag
    // unset the gate is exactly what it was.
    if (smoke_frames > 0 || onaping_smoke_armed()) {
        if (const char* sd = std::getenv("SEADS_SMOKE_SPAWN_DIR")) {
            double sx = 0.0, sy = 1.0, sz = 0.0;
            if (std::sscanf(sd, "%lf,%lf,%lf", &sx, &sy, &sz) == 3) {
                const glm::dvec3 dir(sx, sy, sz);
                if (glm::length(dir) > 1e-9) {
                    loop.spawn_up = glm::normalize(dir);
                    glm::dvec3 fwd =
                        glm::dvec3(0.0, 0.0, -1.0) -
                        loop.spawn_up *
                            glm::dot(glm::dvec3(0.0, 0.0, -1.0), loop.spawn_up);
                    if (glm::length(fwd) < 1e-6)
                        fwd =
                            glm::dvec3(1.0, 0.0, 0.0) -
                            loop.spawn_up * glm::dot(glm::dvec3(1.0, 0.0, 0.0),
                                                     loop.spawn_up);
                    fwd = glm::normalize(fwd);
                    // Companion knob: SEADS_SMOKE_SPAWN_PITCH_DEG tilts the
                    // facing DOWN from level by this many degrees (about the
                    // tangent x up axis) — a high establishing shot (the
                    // spec's "12 km up, both domes in frame") needs to look
                    // well below level on a R=15 km planet (the horizon dip
                    // acos(R/(R+alt)) is tens of degrees at altitude).
                    if (const char* pd =
                            std::getenv("SEADS_SMOKE_SPAWN_PITCH_DEG")) {
                        const double deg = std::atof(pd);
                        const double rad =
                            deg * (3.14159265358979323846 / 180.0);
                        // fwd/up are already orthonormal, so this is a plain
                        // 2D rotation of fwd toward -up in their shared plane.
                        fwd = glm::normalize(fwd * std::cos(rad) -
                                             loop.spawn_up * std::sin(rad));
                    }
                    loop.spawn_fwd = fwd;
                    loop.spawn_alt = spawn_alt;
                }
            }
        }
    }
    // ★★★ L3 -- THROUGH THE ONE ROUTING FUNCTION (app/spawn_policy.h). The
    // FIRST birth is the aircraft either way: the menu cannot run here (the
    // window, the world, the pumps and the snowpack do not exist yet at this
    // point in main()), so the menu opens on the first FRAME instead and a
    // Snowmachine answer re-places both bodies then. Aircraft through
    // player_spawn IS spawn_state, unchanged (test_spawn_policy.cpp pins it).
    loop.curr = app::player_spawn(app::PlayerSpawnChoice::Aircraft, params,
                                  spawn_alt, loop.spawn_up, loop.spawn_fwd);
    loop.prev = loop.curr;
    loop.prev_up = sim::local_up(loop.curr.position);
    loop.aim.reseed(loop.curr.orientation, loop.prev_up);
    loop.grounded = true;  // the first tick is a spawn tick (SPEC §9.5)

    // R4 SOLID GROUND — the live world the kernel flies (MASTER_PLAN §3.C,
    // Chad's 2026-07-15 ruling: contact in the kernel via env.ground).
    // ground points at the RENDER-BUILT post-blur heightfield — the ONE
    // elevation source (mesh, props, contact; the H1 anti-fork — never a file
    // re-read). Built here, AFTER InitWindow + set_planet_build_params, exactly
    // like the first draw would. Null (disabled in game.toml, or the planet
    // failed to load) => the v3 bare-sphere world bit-identically. The SAME env
    // goes to player, drones, and controller — the R3-review P1: one crash
    // surface, never a fork.
    // ★ WINTER S2 (WINTER_LAW §2.2): the analytic snowpack field, assembled
    // from the ONE height field, the FRACTIONAL landmask (INV-8) and the
    // promoted surface linework (§2.3). It is a pure function of those three
    // plus [snowpack], so it holds only non-owning pointers and has no state
    // of its own to go stale. Sources that are absent stay null and switch
    // their term off BY DATA -- the black-rock `barren` raster is not in this
    // tree yet, and nothing here needs an #ifdef to cope with that (§6c.1).
    // ★ ROAD-REPAIR C0 CORNER BLEND: the A/B arm, in ONE process against ONE
    // baked linework. SEADS_CORNER_BLEND=<metres> overrides [snowpack]
    // corner_blend_m; SEADS_CORNER_BLEND=0 is the pre-repair query exactly
    // (the blend is a branch, not 0-width blend arithmetic), which is what
    // lets the census report BEFORE and AFTER without a rebuild between them.
    //
    // ⚠ IT IS SET ON THE CONFIG, NOT ON snow_field.p, and that is the whole
    // point: the drive takes its dials from world.snowpack here, and the DRAWN
    // fold mask takes its own copy later (render/draw.cpp corridor,
    // render/planet.cpp fold). An override applied to snow_field.p alone would
    // leave the fold blending while the drive did not -- a fork between drawn
    // and driven, invented by the instrument. The first run of this A/B did
    // exactly that, and the blend-overflow counter reading non-zero in an arm
    // whose dial was 0 is what caught it.
    if (const char* cb = std::getenv("SEADS_CORNER_BLEND")) {
        world.snowpack.corner_blend_m = std::atof(cb);
        std::printf("ROAD-REPAIR: corner_blend_m overridden to %.3f m\n",
                    world.snowpack.corner_blend_m);
    }
    world::SnowpackField snow_field;
    snow_field.p = world.snowpack;
    // ★ SF1 (§3.6b): the St. Charles snow mountain -- an authored, localized
    // depth override with its baked anchor frame. Same value-copy pattern as
    // the dials above; the field stays a pure function of its inputs.
    snow_field.hill = world.snowhill;

    // ★★ WINTER S3 THE SLED (WINTER_LAW §3). Its own sealed kernel (INV-7):
    // sim/sled.* has its own state and step, shares world::SnowpackField, and
    // touches the aircraft kernel NOT AT ALL. That is why this integration
    // adds nothing to the flight tick -- the sled runs ALONGSIDE the aircraft
    // and app::step_frame is not modified by one character.
    //
    // ★ THE MODE KEY IS SCAFFOLDING, AND IT IS LABELLED ONE. §1 rules FULL
    // EMBODIMENT -- land, walk, find the wrench, remount -- and §2.4c.1
    // explicitly forbids faking a dismount before S8 exists. A fly/drive
    // toggle is therefore NOT the shipped mechanism; it exists so the first
    // drive can happen at S3 instead of waiting for S8. Do not grow a menu on
    // it: that is the thing §1 rejects.
    sim::SledParams sled_params;
    // RC roll-comfort dials from scenario.toml [sled_comfort] (§0b: tune
    // without a recompile). Committed values == kernel defaults, bit-neutral.
    sled_params.comfort = scen.sled_comfort;
    // ★★★ R4a §7.9 -- THE ONE DIAL CHAD OWNS, REACHABLE WITHOUT A REBUILD.
    // "R4a's tuning gate is the important one and it is a felt call, not a
    // number: the ratio of bounced-and-held to thrown-off must be strongly
    // biased to held. Chad rules `grip_capacity_n`. Nobody self-passes it."
    // 78.34 is the p90 of the load over his own 45 drives, so it should read as
    // "maybe 10 % of jumps" -- LOWER throws him off more often, HIGHER less,
    // and a huge value (SEADS_GRIP_CAPACITY=1e30) restores stage 1 exactly,
    // which is the kill switch and the A/B. The value set here is the value the
    // tape records (`grip.capacity` is in SLEDTAPE_PARAMS_D), so a drive can
    // never disagree with its own tape about what it was flown at.
    if (const char* e = std::getenv("SEADS_GRIP_CAPACITY"))
        sled_params.grip.capacity = std::atof(e);
    // ★★★ THE SLED FIRST BUILD (docs/sled_audit/ladder_v2.md §4, the
    // 2026-09-18 addendum §D). FIVE dials Chad arms one at a time from the
    // launch line, in the SEADS_GRIP_CAPACITY shape above and for the same
    // reason: TOML IS THE LANDING PATH, NOT THE DRIVE PATH. A require()'d TOML
    // key is strict -- it would mean editing config/scenario.toml, the loader
    // twice and test_load_scenario, and it would make each dial part of the
    // shipped game on the FIRST build, before he has felt any of them.
    //
    // ★★★ SLED KERNEL v2, 2026-09-18 -- THE LANDING PATH IS NOW TAKEN AND THE
    // PARAGRAPH ABOVE IS HISTORY, KEPT BECAUSE IT IS THE RECORD OF WHY THE
    // DRIVE WENT THROUGH ENV. He drove all five and ruled: "yes tyo all 7 and
    // all 3 of these reccomendations I concurr I want this all in a v2". The
    // DRIVEN VALUES ARE THE SHIPPED DEFAULTS now --
    //   traction_mu 3.0 and track_lat_slip_shed 1.4 in sim/sled.h (SledParams
    //     has no TOML bridge; the struct default IS the shipped value),
    //   rolled_throttle_frac 0.15 in sim/sled.h (a SledComfort field with no
    //     TOML key, so the struct default survives `comfort = scen.sled_
    //     comfort` above and is likewise the shipped value),
    //   right_assist_max_ms 4.0 and right_stand_shift_frac 0.5 in
    //     config/scenario.toml [sled_comfort] (those two ARE loaded keys, so
    //     the TOML line wins and the struct defaults stay at the identity for
    //     the tape-absent rule).
    //
    // ★ SO "NO ENV SET" NO LONGER MEANS "IDENTITY". It means SHIPPED v2. Every
    // one of these five assignments is now an OVERRIDE of a live value, and
    // that is exactly what they are kept for (the SEADS_GRIP_CAPACITY shape):
    // a kill switch back to the pre-v2 machine
    //   SEADS_SLED_TRACTION_MU=0 SEADS_SLED_TAILSHED=0
    //   SEADS_SLED_ROLLED_THROTTLE=0 SEADS_SLED_RIGHT_MAXSPD=1.3888888888888888
    //   SEADS_SLED_STAND_SHIFT=1
    // and the A/B for the next lane that wants to move one.
    //
    // ★ THE OLD TAPES ARE STILL SAFE, and not by luck: the tape-absent rule in
    // test/harness/sled_tape.h reconstructs an ABSENT dial at its IDENTITY, not
    // at these defaults, and the proof is in docs/SLED_KERNEL_V2_LANDING.md §2
    // (all three goldens and all six of his 09-17 tapes replay byte-identically
    // before and after this change).
    //
    // ★ PLACED BEFORE `sim::SledState sled;` ON PURPOSE -- the tape writer
    // records `sled_params`, so the value the tape names is the value that was
    // flown. A drive can never disagree with its own tape about what it was
    // driven at. Both NEW fields are in the tape roster
    // (test/harness/sled_tape.h) in this same commit, which is the law that
    // makes this env route safe at all.
    //
    // ⚠ A NON-NUMERIC VALUE IS A WARNING AND A KEPT DEFAULT, never a silent
    // zero: std::atof("abc") is 0.0, and 0.0 is the IDENTITY for three of
    // these five -- so a typo would look exactly like "the dial did nothing"
    // on the one run that was supposed to answer a question.
    //
    // ★★ FOLDED RED-TEAM P2-7 / P2-3 / P3-10 (2026-09-18). Three defects lived
    // in the first draft of this block and all three end in the same place --
    // a banner that says a dial is armed when it is not:
    //   ORDER. The trailing-space skip ran BEFORE the no-conversion test, so
    //     `SEADS_SLED_STAND_SHIFT=" "` (a stray space from `set VAR= `, a
    //     pasted launch line or a .bat) advanced `end` past `e`, passed
    //     validation and wrote 0.0 -- and 0.0 is NOT the identity for
    //     right_stand_shift_frac (1.0) or right_assist_max_ms (1.3889). That is
    //     the exact silent zero the note above says this code prevents. The
    //     no-conversion test runs FIRST now.
    //   NON-FINITE. `strtod` accepts "nan" and "inf" and both passed the
    //     full-string check. `SEADS_SLED_TRACTION_MU=nan` poisons every contact
    //     force in the kernel. Rejected.
    //   ACCEPTANCE, NOT PRESENCE. The banner built its armed list from
    //     `getenv` alone, so a REJECTED value ("0,15") and an EMPTY one both
    //     printed as armed on the one line whose whole job is "a run's own log
    //     says what it was flown at". `env_dial` returns a bool now and the
    //     banner is built from the RETURN VALUE.
    // ★ AND A BAND WARNING (FOLDED P1-4): an out-of-band value WARNS AND STILL
    // APPLIES. The SEADS_GRIP_CAPACITY precedent is explicit that a huge value
    // is the kill switch and the A/B, so a refusal would be wrong -- the
    // warning is the fix, not a veto.
    {
        auto env_dial = [](const char* name, double* dst, double lo,
                           double hi) -> bool {
            const char* e = std::getenv(name);
            if (!e) return false;
            auto keep = [&](const char* why) {
                std::fprintf(stderr,
                             "[config] %s='%s' %s -- KEEPING the default %.10g "
                             "(the dial is OFF, not zero-by-accident)\n",
                             name, e, why, *dst);
                return false;
            };
            if (!*e) return keep("is empty");
            char* end = nullptr;
            const double v = std::strtod(e, &end);
            if (end == e) return keep("is not a number");  // FIRST, always
            while (*end != '\0' &&
                   std::isspace(static_cast<unsigned char>(*end)))
                ++end;
            if (*end != '\0') return keep("is not a number");
            if (!std::isfinite(v)) return keep("is not finite");
            if (v < lo || v > hi)
                std::fprintf(stderr,
                             "[config] %s=%.10g is OUTSIDE the sane band "
                             "[%.10g, %.10g] -- APPLYING IT ANYWAY (a huge "
                             "value is the kill switch and the A/B), but read "
                             "that run as an out-of-band run\n",
                             name, v, lo, hi);
            *dst = v;
            return true;
        };
        bool armed_ok[7] = {false, false, false, false, false, false, false};
        // B0 -- the contact ceiling (rung 7, promoted: it is B1's
        // precondition). A track cannot push harder than the snow it stands
        // on. Shipped 0.0 = OFF through the drive; SHIPS 3.0 SINCE v2, so this
        // env var is now the way back to the pre-v2 machine.
        armed_ok[0] =
            env_dial("SEADS_SLED_TRACTION_MU", &sled_params.traction_mu, 0.0,
                     100.0);
        // B1 -- "if I key press throttle should ramp up" while rolled.
        // SHIPS 0.15 SINCE v2 (sim/sled.h; no TOML key).
        armed_ok[1] =
            env_dial("SEADS_SLED_ROLLED_THROTTLE",
                     &sled_params.comfort.rolled_throttle_frac, 0.0, 1.0);
        // B2a -- the measured blocker. The righting pump only armed below
        // 1.3889 m/s and was open 13-20 % of the time he was over, so the gate
        // was driven BEFORE the rung it blocks. SHIPS 4.0 SINCE v2
        // (config/scenario.toml); this env var overrides that line.
        armed_ok[2] =
            env_dial("SEADS_SLED_RIGHT_MAXSPD",
                     &sled_params.comfort.right_assist_max_ms, 0.0, 30.0);
        // B2b -- the pendulum. 1.0 WAS shipped: the automatic brace commanded
        // the whole shift, so his own rocking was drowned. Below 1.0 his body
        // is live and the TIMING starts to matter -- he drove 0.5 and SHIPS
        // 0.5 since v2 (config/scenario.toml).
        armed_ok[3] =
            env_dial("SEADS_SLED_STAND_SHIFT",
                     &sled_params.comfort.right_stand_shift_frac, 0.0, 1.0);
        // B3 -- the tail swing. Driven ALONE up its own ladder; SHIPS 1.4
        // SINCE v2 (sim/sled.h).
        // BAND [0, 5], not [0, 1]: after the red-team fold this dial multiplies
        // the ROOST (|trk_slip| * avail), and `avail` runs ~0.23 at WOT on
        // 0.30 m snow -- so the useful ladder is 0.4 / 0.8 / 1.4, not
        // 0.15 / 0.3 / 0.5. It stopped being a fraction when it started
        // reading a fraction. Handoff SS5 carries the arithmetic.
        armed_ok[4] = env_dial("SEADS_SLED_TAILSHED",
                               &sled_params.track_lat_slip_shed, 0.0, 5.0);
        // ★ N2 -- the lake-ice low-speed ski bite (his run-7 words: "the ski
        // runners are not digging in to the ice ... less grip at lower speeds
        // than I would like"). Additive ski mu on LakeIce only, fading to
        // exactly 0 by 8 m/s. SHIPS 0.25 from config/scenario.toml
        // [sled_comfort] ice_bite_mu -- picked by MEASUREMENT, NOT YET FLOWN;
        // this env var overrides that line and 0 is the kill switch.
        // BAND [0, 0.45]: 0.22 + 0.45 = 0.67 keeps a ski on ice under a ski on
        // a groomed trail (TrailMain mu_lat 0.70).
        armed_ok[5] = env_dial("SEADS_SLED_ICE_BITE",
                               &sled_params.comfort.ice_bite_mu, 0.0, 0.45);
        // ★ N1 -- the leg work (his runs 5 and 7: "rider attached can use
        // legs to roll it over backward and on its side"; "extend legs with
        // shift would put the sled up first then falling over on its side").
        // The rider's leg torque budget [N m] for a machine stuck on its END
        // (CTRL kicks it over BACKWARD, nose up, onto its back or a side) or
        // on its BACK (SHIFT lifts the higher end, the pendulum on the same
        // press rights it); every stage press-gated, one-way, and gated on
        // contact, speed, hands AND the kernel's own rolled latch (the fact
        // that makes R legal -- red-team fold 2026-09-19). SHIPS 2400
        // from config/scenario.toml [sled_comfort] leg_work_nm -- picked by
        // MEASUREMENT, NOT YET FLOWN; this env var overrides that line and 0
        // is the kill switch. BAND [0, 4000]. The R autoright key below is
        // untouched: the ladder makes it unnecessary, not absent.
        armed_ok[6] = env_dial("SEADS_SLED_LEGWORK",
                               &sled_params.comfort.leg_work_nm, 0.0, 4000.0);
        // ★ THE BANNER. One line, always printed, naming every value in force
        // and which of them an env var actually moved -- so a run's own log
        // says what it was flown at and "did you have it armed?" is never a
        // question anybody has to answer from memory.
        const char* const kNames[7] = {
            "SEADS_SLED_TRACTION_MU", "SEADS_SLED_ROLLED_THROTTLE",
            "SEADS_SLED_RIGHT_MAXSPD", "SEADS_SLED_STAND_SHIFT",
            "SEADS_SLED_TAILSHED",     "SEADS_SLED_ICE_BITE",
            "SEADS_SLED_LEGWORK"};
        // ★ BUILT FROM ACCEPTANCE, NEVER FROM PRESENCE (FOLDED P2-3/P3-10):
        // a value this block REJECTED is not armed, and the one line he will
        // skim must not say it is.
        std::string armed;
        for (int k = 0; k < 7; ++k) {
            if (!armed_ok[k]) continue;
            const char* e = std::getenv(kNames[k]);
            if (!armed.empty()) armed += " ";
            armed += kNames[k];
            armed += "=";
            armed += (e ? e : "");
        }
        std::printf(
            "[config] sled first-build: traction_mu %.10g "
            "rolled_throttle_frac %.10g right_assist_max_ms %.10g "
            "right_stand_shift_frac %.10g track_lat_slip_shed %.10g "
            "ice_bite_mu %.10g leg_work_nm %.10g (env: %s)\n",
            sled_params.traction_mu, sled_params.comfort.rolled_throttle_frac,
            sled_params.comfort.right_assist_max_ms,
            sled_params.comfort.right_stand_shift_frac,
            sled_params.track_lat_slip_shed, sled_params.comfort.ice_bite_mu,
            sled_params.comfort.leg_work_nm,
            armed.empty() ? "none -- SLED KERNEL v2 + N2 + N1 shipped defaults"
                          : armed.c_str());
    }
    sim::SledState sled, sled_prev;
    // ★★★ R4c §7.3 STAGES 4-7 -- THE MAN, ONCE HE IS OFF THE MACHINE. Kernel
    // state, stepped beside the sled and off the SAME snowpack (§R5+: "a foot
    // is just another contact patch"). `Riding` is the whole of it until the
    // grip breaks, and every branch below is inert in that mode.
    sim::WalkerState walker, walker_prev;
    sim::WalkerParams walker_params;
    // ★★★ GAIT LADDER G1 (docs/PLAN_20260904_gait_ladder.md): his feet, a
    // SEPARATE value type stepped beside `walker` off the SAME published
    // state -- `sim::WalkerState` itself does not grow (L-FROZEN). Reset
    // alongside `walker` at every place he is placed, thrown or remounted
    // (`app::walker_place_afoot` / `app::player_mount_request` /
    // `app::player_dismount_request` / the fall's own `sim::walker_throw`).
    sim::GaitState gait;
    // ★★★ GAIT LADDER G2i -- THE SHIFT KEY (jump / sprint / dash). Chad:
    // "make the shift key jump press and also speed up his run a tad while
    // pressing and holding; for about 5 seconds a dash will start on a jump
    // press when landing from it". Another value type stepped beside the
    // walker, for the same L-FROZEN / L-NPC reason `gait` is (sim/gait.h's
    // own banner says why this is NOT a `WalkerMode`). Its dials are
    // live-tunable by env, the SEADS_WALK_SPEEDS precedent -- Chad signs the
    // FEEL of a jump with his stick, never on paper.
    sim::HopState hop;
    sim::HopParams hop_params;
    if (const char* e = std::getenv("SEADS_HOP"))
        std::sscanf(e, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                    &hop_params.jump_speed_mps, &hop_params.sprint_cap_mul,
                    &hop_params.dash_charge_s, &hop_params.dash_cap_mul,
                    &hop_params.dash_decay_s, &hop_params.dash_arm_grace_s,
                    &hop_params.tuck_full_frac, &hop_params.tuck_gain);
    // ★★★ GAIT LADDER G2i-b -- THE AIR POSE'S OWN GEOMETRY (Chad, flying
    // G2i: "jump should have a knee forward or both knees bent a bit while in
    // the air because they kind of hang back there right now"). Four
    // offsets from the hip, in the same currency the foot targets are in --
    // separate from SEADS_HOP because they are a POSTURE, not the hop's
    // physics, and Chad tunes one thing at a time (L-ONE-DIAL).
    // SEADS_HOP_TUCK="lead_drop,lead_fwd,trail_drop,trail_back".
    sim::AirTuckParams hop_tuck;
    if (const char* e = std::getenv("SEADS_HOP_TUCK"))
        std::sscanf(e, "%lf,%lf,%lf,%lf", &hop_tuck.lead_drop_m,
                    &hop_tuck.lead_fwd_m, &hop_tuck.trail_drop_m,
                    &hop_tuck.trail_back_m);
    // ★★★ GAIT LADDER G2i-d -- AND THE LANDING POSE'S OWN GEOMETRY (Chad,
    // flying G2i-c: "when he is going to land his feet are swinging and
    // trailing back -- I didn't want that at all. I want it to look like a
    // normal jump is all"). Same four offsets, plus the fall speed at which
    // the pose fully owns the legs -- its own env because the descent is its
    // own half of the arc and Chad tunes one thing at a time (L-ONE-DIAL).
    // SEADS_HOP_LAND="lead_drop,lead_fwd,trail_drop,trail_back,full_at_fall".
    sim::AirLandParams hop_land;
    if (const char* e = std::getenv("SEADS_HOP_LAND"))
        std::sscanf(e, "%lf,%lf,%lf,%lf,%lf", &hop_land.lead_drop_m,
                    &hop_land.lead_fwd_m, &hop_land.trail_drop_m,
                    &hop_land.trail_back_m, &hop_land.full_at_fall_mps);
    // ★ ... and the SMOKE rig's scripted press (see where it fires, below).
    long long smoke_hop_tick = 0, smoke_hop_hold = 1, smoke_hop_t0 = 0;
    bool smoke_hop_fired = false;
    if (smoke_frames > 0)
        if (const char* e = std::getenv("SEADS_SMOKE_HOP"))
            std::sscanf(e, "%lld,%lld", &smoke_hop_tick, &smoke_hop_hold);
    // ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK POSE. Same shape again:
    // a value type, a pure step, an env dial for the cadence Chad's eye signs.
    sim::RepairWork work;
    sim::RepairWorkParams work_params;
    if (const char* e = std::getenv("SEADS_WRENCH"))
        std::sscanf(e, "%lf,%lf,%lf", &work_params.cycle_hz,
                    &work_params.torque_frac, &work_params.blow_s);
    // ★ G2g FOOTSTEPS: the ring render draws from (render/draw.h Footprint).
    // Stamped on each stance-ENTRY edge while he is on his feet in SNOW;
    // 256 prints ~= the last 60-odd strides, oldest overwritten.
    static render::FrameInfo::Footprint footprints[256];
    int footprint_n = 0;
    int footprint_head = 0;
    // ★★★ R4c §7.5 THE POOF, and the ONE hazard that makes it silently do
    // nothing: `sim::WalkerState::poof` is a per-TICK one-shot, and a frame can
    // consume several ticks. By the time the plumes block runs, the flag has
    // been cleared by a later tick. So the edge is APPENDED here inside the
    // tick loop, consumed by the plumes block, and cleared at the top of the
    // next frame -- never read per-frame off the walker.
    render::PoofRequest pending_poof[2]{};
    int pending_poof_n = 0;
    // His dials, reachable without a rebuild -- the SEADS_SLED_CAM precedent.
    // The three speeds are the only numbers in the walker that are HUMAN
    // figures rather than repo measurements, so they are the ones his eye is
    // most likely to want (see sim/walker.h).
    if (const char* e = std::getenv("SEADS_WALK_SPEEDS"))
        std::sscanf(e, "%lf,%lf,%lf", &walker_params.speed_hardpack_mps,
                    &walker_params.speed_mid_mps, &walker_params.speed_deep_mps);
    if (const char* e = std::getenv("SEADS_WALK_RISE"))
        std::sscanf(e, "%lf,%lf", &walker_params.rise_s_hardpack,
                    &walker_params.rise_s_deep);
    if (const char* e = std::getenv("SEADS_GRIP_LATGAIN"))
        walker_params.lat_gain_per_s = std::atof(e);
    walker_params.lie_clearance_m = sled_params.cg_height_m;
    // ★★★ L1 -- THE OUTER PLAYER MODE MACHINE (app/player_mode.h). This ONE
    // struct replaces the loose `drive_mode` / `sled_seeded` pair: `drive_mode`
    // is now `player.off_aircraft()` and `sled_seeded` is `player.sled_seeded`,
    // both pure reads, so no site below can hold a stale copy of which body the
    // player is in. Pilot and Sled behave exactly as they did.
    app::PlayerModeState player;
    // ★ STING RPAS (the sting lane, 2026-09-03; plan
    // Game_loop_idea/PLAN_STING_RPAS.md). `sting` is the whole mechanic's
    // state, threaded into step_frame like the flak world. `sting_shouldered`
    // is app-side UI only (the launcher is up, the man is aiming) — the OUTER
    // mode stays Afoot until the launch click, so a cancelled shoulder never
    // touches the mode table. The aim angles are the shoulder view's own
    // az/el about the man's heading; Chad's 3-per-match ruling lives in
    // sting.st.left.
    app::StingWorld sting;
    // The Sting's own derived airframe (fly-2's band, bottom widened to
    // 150 m/s by the ST-5 prop/audio ruling), built ONCE
    // from the loaded plane params — never re-derived per frame.
    sting.ap = app::sting_aircraft_params(params);
    sting.st.left = sting.sp.per_match;  // fly-4: the loadout is 5 — one dial
    bool sting_shouldered = false;
    double sting_aim_az = 0.0, sting_aim_el = 0.25;
    // ★ GIVE UP (Chad 2026-09-04): "a way to let the Sudburian die so I can
    // respawn in a snowmachine / plane after I run out of stings to deploy
    // or am too far from the plane." X HELD for kGiveUpHoldS; the frame it
    // completes is flagged so the ordinary respawn block below runs.
    constexpr double kGiveUpHoldS = 1.5;
    double giveup_hold_s = 0.0;
    bool gave_up_frame = false;
    char giveup_note[96] = {};  // sled_note is a bare pointer: own the text
    // ★ PUMP WALK-IN PROBE (smoke-only, 2026-09-06; Chad: "doesn't let me
    // get to within 6 m of the pump"): SEADS_PUMPWALK_SMOKE="<start_m>".
    bool pumpwalk_done = false;
    int pumpwalk_pump = -1;
    // ★ L10 ENGINE WALK-IN PROBE (smoke-only, the SEADS_PUMPWALK_SMOKE class).
    // The pump rung's own LAW, learned on 2026-09-06: run the rig before
    // believing any "I cannot reach it" report. This one exists so the engine
    // fix never has to be argued about from memory either.
    bool enginewalk_done = false;
    bool enginewalk_armed = false;  // one-shot U press when the site lights
    bool enginewalk_press = false;  // armed at the site table, spent at KEY_U
    // ★★ THE PARKED AEROPLANE THE PROBE HOLDS STILL. Measured, not assumed:
    // parking it once was not enough -- a --smoke run keeps stepping the
    // airframe, and the aeroplane rolled away from the walking man at about
    // 11 m/s while he closed at 0.73, so the site could never light. A BENCH
    // rig may clamp its fixture; what it must never clamp is the thing under
    // test, and the thing under test here is the reach law, the prompt, the
    // key and the wrench -- not the aeroplane's ground handling.
    sim::SimState enginewalk_park{};
    // SPACE freelook orbit around the flying drone (fly-2: "it needs the
    // free look cam space press thing") — the sled_cam_az/el idiom: mouse
    // lent to the camera while held, the AIM HOLDS (the plane's rule 1),
    // ease back behind the aim on release.
    double sting_cam_az = 0.0, sting_cam_el = 0.0;
    // fly-3: the wheel zooms the chase/orbit distance (map keeps the wheel
    // while it is open — same arbitration the sled world already uses).
    double sting_cam_dist = 7.0;
    // The reach law, all of it, from config ([interact] / [repair]). app/ holds
    // no reach number of its own.
    app::InteractDials interact_dials;
    interact_dials.sled_reach_m = game.interact.sled_reach_m;
    interact_dials.aircraft_reach_m = game.interact.aircraft_reach_m;
    interact_dials.gun_reach_m = game.interact.gun_reach_m;
    interact_dials.dismount_side_m = game.interact.dismount_side_m;
    interact_dials.dismount_stop_ms = game.interact.dismount_stop_ms;
    interact_dials.repair_reach_m = game.repair.reach_m;
    interact_dials.engine_reach_m = game.repair.engine_reach_m;
    // ★ L3 THE SPAWN DIALS (config/game.toml [spawn]). Same discipline as the
    // reaches above: app/ invents no distance of its own.
    app::SpawnDials spawn_dials;
    spawn_dials.sled_dist_m = game.spawn.sled_dist_m;
    spawn_dials.aircraft_beside_m = game.spawn.aircraft_beside_m;
    // ★★ L2 THE REPAIR DIALS (config/game.toml [repair]). combat/ holds no
    // dial of its own and app/ invents no number: the reach the site is
    // registered at above and the reach the wrench is gated on below are THE
    // SAME dial read twice, never two constants that could drift.
    combat::RepairParams repair_params;
    repair_params.full_s = game.repair.full_s;
    repair_params.reach_m = game.repair.reach_m;
    repair_params.repair_points = game.repair.repair_points;
    // ★★ L10 THE ENGINE'S OWN PAIR, read from the same section and under
    // the same discipline: the reach the site is registered at and the reach
    // the wrench is gated on are ONE dial read twice.
    combat::EngineRepairParams engine_repair_params;
    engine_repair_params.full_s = game.repair.engine_full_s;
    engine_repair_params.reach_m = game.repair.engine_reach_m;
    // The last progress the SIM TICK published, for the HUD bar. Negative =
    // nobody is turning a wrench. Written tick-side, read frame-side -- never
    // recomputed by the draw.
    double repair_frac_hud = -1.0;
    // The site the man is standing at THIS frame, and the line the HUD draws
    // for it. Rebuilt every frame from live positions -- never cached, because
    // a cached site is a cached position and the machine moves.
    app::InteractSite near_site;
    const char* interact_line = nullptr;
    bool track_demo_done = false;  // SF3-B visual rig, one-shot
    // ★ R4a: bumped on every write to `sled` OUTSIDE step_sled -- the mount
    // seed and the KEY_R autoright, the SAME two sites that emit a tape O
    // record ("any write to `sled` outside step_sled emits a state-override
    // record"). render finite-differences velocity/angular_vel for the scarf's
    // frame field, and differencing ACROSS one of these reads a teleport as
    // hundreds of g -- the model-space scarf anchor does not move on a
    // respawn, so the solver's own teleport test is blind to it. Naming the
    // discontinuity at its source beats guessing a magnitude threshold.
    long sled_epoch = 0;
    // ★ SLED TAPE R1 (GI_DRIVE1_HANDOFF §3, consult F6): AUTO-ON with drive
    // mode — the F9/zero-tape lesson mechanized: recording is not optional,
    // one file per drive-mode episode, streamed binary with a 1 s drain so a
    // crash tape survives (valid without its footer). Firewalled: the writer
    // only ever sees const refs; the on/off bit-identity leg in
    // test/unit/test_sled_tape.cpp is the proof.
    seads_sledtape::Writer sled_tape;
    std::ofstream sled_tape_file;
    bool sled_tape_on = false;
    long sled_tick_no = 0;  // the tape's only time source
    long sled_tape_last_flush = 0;
    // ★ S3 PHASE A -- the sled's OWN control accumulators (§9d). The mouse is
    // the rider's WEIGHT on a sled, not an aim vector: curved deltas accrue
    // into a held lateral/fore-aft command ([-1,1] of the kernel's reach box),
    // no return spring (side-hilling must be HOLDABLE, §9d.3; C recentres).
    // The thumb throttle is SPRING-RETURN (a real thumb lever springs shut --
    // §14's whole free-roll band is unreachable off a latched ramp).
    double sled_lean_lat = 0.0, sled_lean_fwd = 0.0;  // [-1,1] commands
    // *** SK-1c (Chad's ruling 2026-08-25): MOUSE X IS THE STEERING AXIS, and
    // the lean AUTO-FOLLOWS it -- "link the steer to the lean ... so it auto
    // leans tied to the left right of the mouse lean dot". Measured reason it
    // has to be an analog axis at all: with A/D the command is BINARY +/-1 and
    // steer_rate_per_s 2.0 reaches full lock in 0.5 s, and on Road the machine
    // ROLLS at every command >= 0.12 -- the only non-rolling turn lives at
    // ~0.11 (R 82-99 m), which a keyboard can never hold.
    double sled_steer_cmd = 0.0;  // [-1,1], the analog handlebar
    bool sled_hud_xray = true;    // SK-1c: H toggles xray grid vs bottom bar
    double sled_thumb = 0.0;      // [0,1] spring-return
    // ★ R2c-5: the brake command the kernel was actually handed, held at frame
    // scope so the RIDER'S ARMS read the same number step_sled did rather than
    // recomputing the key state at draw time (the one-number rule, §2.4c.1).
    double sled_brake_cmd = 0.0;  // [0,1]
    // ★ DRIVE-2 FIX: sled freelook orbit (SPACE + mouse) -- app-owned camera
    // angles around the machine; ease back behind on release. The HEAD rig
    // channel reads these same numbers (cam owns look, head follows, §9d).
    double sled_cam_az = 0.0, sled_cam_el = 0.0;  // [rad]
    // ★ CAM-SMOOTH (2026-09-08): the sled camera's LAGGED anchor and forward.
    // The eye used to be welded to the machine's CG and heading, so every
    // suspension tick, ski contact and yaw wobble off the facet terrain went
    // 1:1 into the view. Both are first-order lags on the FRAME dt (cosmetic,
    // render-side, never fed back). The anchor lag carries a velocity
    // feed-forward (target = pos + vel*tau) so at steady speed the lag is
    // ZERO -- the framing numbers Chad swept in the seat do not move; only
    // the accelerations (the bumps) are filtered. SEADS_SLED_CAM_TAU="pos,fwd"
    // retunes without a rebuild; "0,0" is the welded camera exactly.
    glm::dvec3 sled_cam_anchor{0.0};
    glm::dvec3 sled_cam_fwd{0.0};
    bool sled_cam_lag_seeded = false;
    // ★★★ THE BORED HEAD (Chad 2026-09-07): "after two seconds of that if you
    // are still in free look they get bored and look straight ahead, this
    // allows the player to look at their character."  While freelook is held
    // the CAMERA keeps obeying the mouse exactly as before -- it is only the
    // rig's HEAD that gives up: it follows for kHeadBoredHoldS, then turns
    // front over kHeadBoredTurnS.  Without this the head tracks the orbit
    // forever and the player can never see his own man's face, only the back
    // of a head that keeps turning away.  Seconds held, reset on release.
    double sled_look_held_s = 0.0;
    // ★ R5 ROW 1 VIEW DIAL: keys 1-4 select a chase framing preset; the three
    // scalars EASE toward the selected preset (the az/el ease-back idiom) so a
    // 4.6 m -> 12.0 m jump glides instead of cutting. Session start = preset 1
    // (RIDE) with the scalars ALREADY AT its exact values, so a session that
    // never presses a number key is bit-identical to the shipped framing.
    int sled_cam_preset = 0;  // 0..3 = RIDE / TRACK / CHASE / WIDE
    double sled_cam_dist = 4.6, sled_cam_high = 2.0, sled_cam_ahead = 1.0;
    // ★★★ ST-5 POLISH (a) -- THE SLED CAMERA'S ZOOM (Chad 2026-09-05: "the
    // free look default zoom level is too close that I am seeing into the
    // hollow model, zoom level should be variable with mouse scroll wheel").
    //
    // A MULTIPLIER ON THE PRESET DISTANCE, not a retune of the presets. The
    // preset table below carries an explicit standing warning -- "the four
    // presets Chad swept live in the seat (do not retune these numbers on
    // paper)" -- and editing 4.6 would spend a number he set with his hands.
    // A multiplier keeps every preset's own SHAPE (the down-angle, the ahead
    // aim, the ratios between the four) and gives the ruling one honest dial:
    // at zoom 1.0 the framing is bit-identical to what he signed, and the
    // wheel walks it out from there.
    //
    // ⚠ WHY THE DEFAULT IS NOT 1.0. The 3D pass runs a 2 m NEAR PLANE
    // (render/draw.cpp's rlSetClipPlanes(2.0, 60000.0)) and the sled + rider
    // shell is roughly 1.8 m in bounding radius about the anchor, so preset 1
    // RIDE's 4.6 m leaves only ~2.8 m between the eye and the nearest surface
    // -- and freelook spends that margin: the orbit pulls the eye down toward
    // the snow and the anchor swaps to the MAN (a smaller body drawn at the
    // same distance) the moment he steps off. That is the "seeing into the
    // hollow model" he reported: the near plane slicing the shell open from
    // inside. 1.45 puts RIDE at 6.67 m and the nearest surface ~4.9 m out,
    // more than double the plane, and every other preset scales with it.
    // ⭐ THIS IS THE ONE NUMBER TO MOVE if the default still reads wrong.
    constexpr double kSledCamZoomDefault = 1.45;
    // The band, in EFFECTIVE metres of chase distance -- clamped on the
    // product, never on the multiplier, so the same wheel travel means the
    // same framing on every preset (a fixed multiplier band would let CAM 4
    // WIDE's 12 m sail past any sane limit while RIDE could not reach it).
    constexpr double kSledCamDistMin = 3.0;
    constexpr double kSledCamDistMax = 30.0;
    // Geometric per notch, the sting flight cam's own shape (main.cpp's
    // sting_cam_dist wheel step) -- a linear step is coarse when you are close
    // and useless when you are far.
    constexpr double kSledCamZoomStep = 1.18;
    double sled_cam_zoom = kSledCamZoomDefault;
    double saved_throttle_target = 0.0;  // aircraft latch frozen across drive
    // A refused mount has to SAY so. A silent no-op reads as a broken key, and
    // the player's next move is to press it harder.
    const char* sled_note = nullptr;
    double sled_note_until_s = 0.0;

    sim::Environment env;
    if (game.ground.enabled) {
        // ★ R1 THE DRAWN FOLD (BLOCK-VP1). MUST precede the planet load: the
        // cubesphere is LAID ON this fold, so arming it afterwards would do
        // nothing. It hands the planet the SAME [snowpack] dials the sled
        // drives on -- one set of numbers, so the drawn surface can only ever
        // move TOWARD the driven one, never fork from it.
        //
        // ⚠ SEASON: the folded mesh is season-STATIC. Under the standing
        // winter-default ruling that is correct, but a future season toggle
        // must rebuild the planet mesh, not merely change uSeasonSnow -- a
        // summer world would otherwise keep winter geometry.
        render::set_planet_snow_fold(world.snowpack, true);
        pump_loading("planet heightfield");
        env.ground = render::planet_heightfield(params);
        env.ground_params.slope_limit_cos =
            std::cos(game.ground.slope_limit_rad);
        env.ground_params.friction = game.ground.friction;
        env.ground_params.max_sink_ms = game.ground.max_sink_ms;
        env.ground_params.normal_probe_m = game.ground.normal_probe_m;
        env.ground_params.contact_height_m = game.ground.contact_height_m;
        // R4g (Chad's fly asks): wheel brakes + grounded roll alignment.
        env.ground_params.brake_friction = game.ground.brake_friction;
        env.ground_params.roll_align_rate = game.ground.roll_align_rate_rad;
        // R4-FLY-5 consequence dials (damage, not prevention — Chad).
        env.ground_params.ground_ang_damp = game.ground.ground_ang_damp;
        env.ground_params.wing_halfspan_m = game.ground.wing_halfspan_m;
        env.ground_params.ground_loop_lat_g = game.ground.ground_loop_lat_g;
        env.ground_params.brake_pitch_rate = game.ground.brake_pitch_rate_rad;
        env.ground_params.prop_strike_pitch_rad =
            game.ground.prop_strike_pitch_rad;
        env.ground_params.noseover_full_speed_ms =
            game.ground.noseover_full_speed_ms;
        // T3: the deep-penetration wall-strike floor (tunnel-wall collision).
        env.ground_params.deep_penetration_m = game.ground.deep_penetration_m;
        // ★ terrain-clip T2 — the ONE dial, and its kill. SEADS_FACET_CONTACT
        // REPLACES the config value (the SEADS_OVER_BANK_BIAS pattern), so
        // `SEADS_FACET_CONTACT=0` returns the shipped exe to the pre-T2 DEM-
        // field crash surface with the facet fn never called, without editing
        // config/game.toml — the honest OFF arm of the A/B and the kill.
        // Clamped to the loader's own [0, 1] so a typo in the environment can
        // never hand the kernel a surface nothing is drawn on.
        //
        // ★ T2b (red-team P1-4): A NON-NUMERIC VALUE IS IGNORED, LOUDLY.
        // std::atof("off") is 0.0 -- it would SILENTLY DISARM the fix and
        // nothing would say so. The pattern chosen is WARN-AND-KEEP (not
        // error-and-exit): a fly session must never die because of a typo in
        // the environment, and the resolved value is printed in the [config]
        // banner below, so "which surface did I just fly?" stays answerable
        // from the launch log whatever the environment said. Stricter than the
        // SEADS_RIBBON_MAXSEG / SEADS_CORNER_BLEND atof neighbours on purpose:
        // this one decides the CRASH SURFACE.
        env.ground_params.facet_contact = game.ground.facet_contact;
        if (const char* fc_env = std::getenv("SEADS_FACET_CONTACT")) {
            char* fc_end = nullptr;
            const double fc_val = std::strtod(fc_env, &fc_end);
            const bool fc_ok = fc_end != nullptr && fc_end != fc_env &&
                               *fc_end == '\0' && std::isfinite(fc_val);
            if (fc_ok) {
                env.ground_params.facet_contact = fc_val;
            } else {
                std::fprintf(stderr,
                             "[config] WARNING: SEADS_FACET_CONTACT=\"%s\" is "
                             "not a number -- IGNORED, keeping game.toml "
                             "[ground] facet_contact %.2f\n",
                             fc_env, game.ground.facet_contact);
            }
        }
        env.ground_params.facet_contact =
            std::clamp(env.ground_params.facet_contact, 0.0, 1.0);
        // R4f building collision: prisms from the SAME bake as the rendered
        // massing, based on the SAME height field (one crash surface). Gated
        // on ground being live (the base radius needs the field) + the
        // [buildings] collide toggle. Null = ghosts (pre-R4f, A/B).
        if (env.ground != nullptr && game.buildings.collide) {
            pump_loading("building colliders");
            env.obstacles =
                render::building_colliders(game.buildings.inflate_m, params.R);
            env.ground_params.obstacle_inflate_m = game.buildings.inflate_m;
            env.ground_params.obstacle_base_margin_m =
                game.buildings.base_margin_m;
        }
    }
    // The town-density index for the train ambience. The SAME function-local
    // static the collision path builds (one parse, one index, one truth), so
    // this is a pointer fetch after the first call and costs nothing here. It
    // is fetched OUTSIDE the [buildings] collide gate on purpose: whether you
    // can crash into a house and whether you can hear a train near it are
    // unrelated questions, and only one of them is a gameplay toggle.
    town_colliders =
        render::building_colliders(game.buildings.inflate_m, params.R);

    pump_loading("fields and weapons");
    // R5 GravityField (MASTER_PLAN §3.B — the Escape Ceiling). Null unless
    // [gravity] enabled: ACTIVATION IS CHAD'S HALT (it deliberately changes
    // flight above h_g0; below the band and with enabled=false the plant is
    // bit-identical). The SAME env reaches player and drones — a bandit
    // baited past the edge floats away on the same field (§3.B).
    // The field PARAMS load unconditionally (Chad tunes [gravity] dials and
    // may activate by the T-key toggle below, not just the config flag);
    // only the POINTER — the activation — follows enabled.
    sim::GravityField grav_field;
    grav_field.h_g0_m = game.gravity.h_g0_m;
    grav_field.sigma_g_m = game.gravity.sigma_g_m;
    // S-airdome (docs/bubble_atmosphere_spec.md §1.8, Chad's 2026-08-09
    // ruling): "turn off the escape sky for the game loop" — CONQUEST no
    // longer force-enables the escape-sky taper at startup (SUPERSEDES the
    // prior "CONQUEST force-enables the escape-sky taper at startup, spec
    // §1" rule). [gravity] enabled stays the sole config gate; the T-key
    // toggle keeps working afterwards either way.
    if (game.gravity.enabled) {
        env.grav = &grav_field;
    }
    // Gun battery (rig-D guns): the Bf 109 F-4/R1 5-gun fit. APP-OWNED; render
    // gets a const view via info.projectiles. Only created here (instructor
    // path with a window); the pool is advanced in the fixed tick via &gw
    // below. MOVED EARLIER (2026-07-25 fly-2 ruling D) so the conquest pump
    // balance below can single-source the battery's DPS (combat::battery_dps)
    // instead of a re-typed gun table.
    weapon::GunWorld gw;
    gw.battery.convergence_range = world.guns.convergence_range_m;
    {
        const auto& g = world.guns;
        // hub 20mm Motorkanone
        gw.battery.guns.push_back({g.muzzle_hub, weapon::Round::Cannon20mm,
                                   g.cannon_speed_mps, g.cannon_rof_hz,
                                   g.cannon_drag_k, g.cannon_damage});
        // left cowl 7.92mm MG
        gw.battery.guns.push_back({g.muzzle_cowl_l, weapon::Round::MG792,
                                   g.mg_speed_mps, g.mg_rof_hz, g.mg_drag_k,
                                   g.mg_damage});
        // right cowl 7.92mm MG
        gw.battery.guns.push_back({g.muzzle_cowl_r, weapon::Round::MG792,
                                   g.mg_speed_mps, g.mg_rof_hz, g.mg_drag_k,
                                   g.mg_damage});
        // left wing 20mm gondola
        gw.battery.guns.push_back({g.muzzle_wing_l, weapon::Round::Cannon20mm,
                                   g.cannon_speed_mps, g.cannon_rof_hz,
                                   g.cannon_drag_k, g.cannon_damage});
        // right wing 20mm gondola
        gw.battery.guns.push_back({g.muzzle_wing_r, weapon::Round::Cannon20mm,
                                   g.cannon_speed_mps, g.cannon_rof_hz,
                                   g.cannon_drag_k, g.cannon_damage});
    }

    // T1 TunnelNet (MASTER_PLAN §2.5; world/tunnel_net.h): the Errington<->
    // Murray underground. Built from the [tunnel] dials + the aircraft R, over
    // the SAME ground field the terrain reads (env.ground may be null here — a
    // ground-disabled world tunnels through the bare sphere at R). Same
    // lifetime pattern as grav_field/atm_field; only the POINTER follows
    // enabled. Null => env.tunnels stays null => bit-identical (R5/R6). MOVED
    // EARLIER (2026-07-25 fly-2 ruling B) so the conquest deep-pump placement
    // below can read the LIVE tunnel_net.arena instead of duplicated [tunnel]
    // constants.
    world::TunnelParams tparams;
    tparams.sphere_R = params.R;
    tparams.tube_width_m = game.tunnel.tube_width_m;
    tparams.tube_height_m = game.tunnel.tube_height_m;
    tparams.depth_m = game.tunnel.depth_m;
    tparams.soft_m = game.tunnel.soft_m;
    tparams.ramp_frac = game.tunnel.ramp_frac;
    tparams.spacing_m = game.tunnel.spacing_m;
    tparams.floor_height_m = game.tunnel.floor_height_m;
    tparams.arena_a_m = game.tunnel.arena_a_m;
    tparams.arena_c_m = game.tunnel.arena_c_m;
    tparams.arena_depth_m = game.tunnel.arena_depth_m;
    tparams.cavern_core_m = game.tunnel.cavern_core_m;
    tparams.breach_margin_m = game.tunnel.breach_margin_m;
    tparams.chamber_long_m = game.tunnel.chamber_long_m;
    tparams.chamber_lat_m = game.tunnel.chamber_lat_m;
    tparams.chamber_vert_m = game.tunnel.chamber_vert_m;
    tparams.chamber_breach_offset_m = game.tunnel.chamber_breach_offset_m;
    tparams.connector_radius_m = game.tunnel.connector_radius_m;
    tparams.chambers_on = game.tunnel.chambers_on;
    tparams.bowl_radius_m = game.tunnel.bowl_radius_m;
    tparams.bowl_depth_m = game.tunnel.bowl_depth_m;
    tparams.mouth_sink_m = game.tunnel.mouth_sink_m;
    tparams.min_cover_m = game.tunnel.min_cover_m;
    tparams.trench_len_m = game.tunnel.trench_len_m;
    tparams.trench_rim_m = game.tunnel.trench_rim_m;
    tparams.headframe_h_m = game.tunnel.headframe_h_m;  // T13 (B1)
    tparams.headframe_on = game.tunnel.headframe_on;
    // ★ WINTER S2: bind the snowpack sources now that the planet exists (the
    // height field build is what loads the landmask and the baked ribbons).
    // env.ground is THE height field -- never a second one (INV-1).
    pump_loading("snowpack sources");
    snow_field.hf = env.ground;
    snow_field.landmask = render::planet_landmask(params);
    snow_field.barren = render::planet_barren(params);
    snow_field.lines = render::planet_linework(params);
    // ★ PHASE W3 (§PHASE W3, RULED-GI-1, P2-7): the drawn-facet radius
    // injection. world/ is render-free by law, so SnowpackField cannot call
    // render::facet_radius_at itself; the app layer is the one place with
    // both env.ground (== snow_field.hf, the H1 anti-fork -- never a second
    // height source) and the shipped subdiv/tiles the mesh was actually
    // built at (world.planet.subdiv/world.planet.tiles -- the SAME [planet]
    // values render::set_planet_build_params fed the mesh build a few lines
    // below). Always injected (cheap: a captured pointer + two ints);
    // [snowpack] hf_faceted_ground (default false) is what gates whether
    // SnowpackField ever calls it -- see ground_radius_base()'s comment.
    if (snow_field.hf != nullptr) {
        const world::HeightField* facet_hf = snow_field.hf;
        const int facet_subdiv = world.planet.subdiv;
        const int facet_tiles = world.planet.tiles;
        snow_field.facet_radius_fn = [facet_hf, facet_subdiv,
                                      facet_tiles](glm::dvec3 dir) {
            return render::facet_radius_at(*facet_hf, dir, facet_subdiv,
                                           facet_tiles);
        };
        // ★ terrain-clip T2: THE SAME SEAM, THE SAME FUNCTION, for the
        // AIRCRAFT. sim/ is render-free by law (the same law world/ obeys
        // three lines up), so sim::Environment cannot call
        // render::facet_radius_at either — the app injects it, from the SAME
        // env.ground (== snow_field.hf, the H1 anti-fork: never a second
        // height source) and the SAME shipped subdiv/tiles the mesh was built
        // at. Not the DRAWN facet (drawn_radius_at, below): the aircraft's
        // base is the TERRAIN facet, exactly as the sled's is, so the ambient
        // snow fold is never counted into the crash surface.
        //
        // Always injected (a captured pointer + two ints). [ground]
        // facet_contact is the only thing that decides whether the kernel ever
        // calls it — 0 leaves the pre-T2 field arithmetic untouched.
        env.ground_facet_fn = [facet_hf, facet_subdiv,
                               facet_tiles](glm::dvec3 dir) {
            return render::facet_radius_at(*facet_hf, dir, facet_subdiv,
                                           facet_tiles);
        };
        // ★ ROAD-REPAIR (drawn == driven on road decks): the DRAWN facet, the
        // same injection, from the same three values. render::drawn_radius_at
        // reads the fold provider render/draw.cpp bound at load_planet -- the
        // same one the ribbons and the planet mesh were built with, never a
        // second one -- so the floor is against THE deck on screen.
        //
        // ⚠ NOT the drive's BASE. ground_radius_base still reads
        // facet_radius_fn (the terrain facet); this reaches the drive only
        // through SnowpackField::apply_deck_floor, one-sided, inside the
        // corridor envelope. See snowpack.h's drawn_radius_fn comment.
        //
        // SEADS_DECK_FLOOR=0 leaves it null, which is the honest OFF arm of
        // the A/B (bit-identical to pre-repair) and the kill switch.
        const char* deck_floor_env = std::getenv("SEADS_DECK_FLOOR");
        if (deck_floor_env == nullptr || std::atof(deck_floor_env) != 0.0) {
            snow_field.drawn_radius_fn = [facet_hf, facet_subdiv,
                                          facet_tiles](glm::dvec3 dir) {
                return render::drawn_radius_at(*facet_hf, dir, facet_subdiv,
                                               facet_tiles);
            };
        }
    }
    // ★ SF1 (§3.6b): the snow-mountain footprint clears the tree scatter the
    // way the corridors do -- bound HERE because the mask needs the same
    // HeightField the snowpack drives (one function, two consumers).
    render::set_tree_snowhill(snow_field.hill, snow_field.hf);
    // ★★★ terrain-clip T2b (red-team P1-4) -- THE CRASH SURFACE, ON THE
    // RECORD. The ONE dial that decides whether the aeroplane collides with the
    // DEM field or with the mesh facet the eye is shown resolves from three
    // places (config/game.toml, SEADS_FACET_CONTACT, and whether a render layer
    // existed to inject render::facet_radius_at at all), and until this line
    // none of them appeared in the launch log. "injected: no" with a non-zero
    // dial is the HALF-ARMED state the identity-by-branch fallback produces --
    // it reads the pre-T2 field, and now it says so out loud.
    //
    // It is appended to g_config_banner (not fprintf'd on its own) so stderr,
    // <exe_dir>/seads_launch.log and the feel-tape header can never disagree --
    // the same law the four old [config] blocks were folded into one for.
    {
        char gb[256];
        std::snprintf(gb, sizeof gb,
                      "[config] ground: facet_contact %.2f (injected: %s)\n",
                      env.ground_params.facet_contact,
                      env.ground_facet_fn ? "yes" : "no");
        g_config_banner += gb;
        std::fputs(gb, stderr);
        write_launch_log();  // rewrite the log with the complete banner
    }
    // S-lapguard FEEL TAPE: opened iff SEADS_FEEL_TAPE names a path. The banner
    // goes in as CSV comment lines, so a tape can never be replayed against the
    // wrong dials -- which is why it is opened HERE, after the ground line
    // above joined the banner (T2b P1-5).
    if (const char* tp = std::getenv("SEADS_FEEL_TAPE")) {
        if (app::feel_tape_open(g_feel_tape, tp, g_config_banner, params))
            std::fprintf(stderr, "[feel-tape] recording to %s\n", tp);
    }
    // ★ SF2-BANKS (§3.6c): the bank strips sample THIS field at build -- bind
    // the pointer here, the same late site, so the drawn bank can never fork
    // from the driven one.
    render::set_bank_snow_sources(&snow_field);
    // ★ SF3-A THE RIDER SNOW PATCH: the drawn near-field snow surface reads
    // THIS field and the shipped [planet] build params, bound at the same
    // late site as the banks so the drawn snow can never fork from the
    // driven snow.
    render::set_snow_patch_sources(&snow_field, world.planet.subdiv,
                                   world.planet.tiles);
    // ★ SF3-B THE TRACK FIELD. Owned here because laying track is a thing
    // the SLED does over TIME, and world/ owns no clock -- but the field
    // itself is pure, and it is bound INTO the snowpack so the deformation
    // lands on the driven surface rather than on a drawn copy.
    world::TrackField sled_tracks;
    if (snow_field.hf != nullptr) {
        world::TrackParams tp{};
        // ★ R4 A/B: seed the readability FLOOR Chad ruled (0.55 m). This is
        // the dial the measured ladder was swept on -- 0.12 invisible, 0.30
        // faint, 0.60 a legible trail, 1.50 a trench -- so it stays reachable
        // without a rebuild (the SEADS_R3_FULLDEPTH precedent). `0` disarms
        // the floor entirely, which is the honest depth-proportional arm of
        // the A/B (0.45 x depth, no floor) and the kill-switch for the rung.
        if (const char* e = std::getenv("SEADS_TRACK_FLOOR"))
            tp.min_depress_m = std::atof(e);
        sled_tracks.reset(snow_field.hf->R, tp);
        snow_field.tracks = &sled_tracks;
    }
    // ★ ICE CLEARANCE PROBE (SEADS_ICE_PROBE=1). Chad, driving R1: "windy lake
    // was flashing with the new cover ... looked like a mesh problem, went away
    // when I flew close and the ice appeared to come back."
    //
    // The ice you see is a MIRROR MESH lifted [water] surface_lift_m above the
    // flattened lakebed; radius_at over water IS the lakebed. So the planet
    // mesh must stay BELOW the mirror by that lift. This reports the surviving
    // clearance: positive = mirror above ground (correct), <= 0 = the ground
    // has punched through the ice, which is what flashes.
    if (std::getenv("SEADS_ICE_PROBE") != nullptr) {
        if (snow_field.hf == nullptr || snow_field.landmask == nullptr) {
            std::fprintf(stderr, "ICE PROBE: no height field / no landmask\n");
            return 3;
        }
        const double R = snow_field.hf->R;
        const double lift = snow_field.p.ice_lift_m;
        const glm::dvec3 c = glm::normalize(loop.spawn_up);
        glm::dvec3 e = glm::cross(glm::dvec3(0.0, 1.0, 0.0), c);
        if (glm::length(e) < 1e-9) e = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c);
        e = glm::normalize(e);
        const glm::dvec3 n = glm::normalize(glm::cross(c, e));
        std::vector<double> clear_open, clear_shore;
        std::size_t n_punch = 0;
        for (double y = -35000.0; y <= 35000.0; y += 60.0) {
            for (double x = -35000.0; x <= 35000.0; x += 60.0) {
                const glm::dvec3 d =
                    glm::normalize(c + e * (x / R) + n * (y / R));
                const double w = snow_field.landmask->at(d);
                if (w < 0.5) continue;  // water only
                // The ice plane, and the ground actually drawn beneath it.
                const double bed = snow_field.hf->radius_at(d);
                const double mirror = bed + lift;
                const double drawn = render::drawn_radius_at(
                    *snow_field.hf, d, world.planet.subdiv, world.planet.tiles);
                const double clearance = mirror - drawn;
                if (clearance <= 0.0) ++n_punch;
                (w > 0.99 ? clear_open : clear_shore).push_back(clearance);
            }
        }
        const auto rep = [](const char* nm, std::vector<double>& v) {
            if (v.empty()) {
                std::printf("  %-22s (no samples)\n", nm);
                return;
            }
            std::sort(v.begin(), v.end());
            const auto q = [&](double f) {
                return v[static_cast<std::size_t>(f * (v.size() - 1))];
            };
            std::printf("  %-22s n=%-7zu  min %+.3f  p10 %+.3f  p50 %+.3f\n",
                        nm, v.size(), v.front(), q(.10), q(.50));
        };
        std::printf(
            "\n=== ICE CLEARANCE (mirror - drawn ground; must stay > 0) ===\n"
            "  [water] surface_lift_m = %.3f m\n",
            lift);
        rep("open water", clear_open);
        rep("shore feather", clear_shore);
        std::printf("  GROUND PUNCHED THROUGH ICE: %zu samples\n\n", n_punch);
        return 0;
    }
    // ★ R1 SCORECARD (SEADS_VP1_GAP=1) -- THE number this rung exists to move.
    //
    // BLOCK-VP1, stated in docs/snow_info_packet_winter_to_barrens.md:24, is
    // that "the renderer drapes every mesh on the bare DEM while the sled
    // drives on DEM + snow depth -- a ~0.77 m gap in bush, everywhere". So the
    // rung is judged by exactly one distribution: drive_radius_at (where the
    // machine is) minus drawn_radius_at (what the screen shows under it).
    // Before R1 that is the snow depth. After R1 it should be ~0, and it should
    // stay ~0 on the plowed corridors too -- which is what the mask is for.
    if (std::getenv("SEADS_VP1_GAP") != nullptr) {
        if (snow_field.hf == nullptr) {
            std::fprintf(stderr, "VP1 GAP: no height field\n");
            return 3;
        }
        const double R = snow_field.hf->R;
        const glm::dvec3 c = glm::normalize(loop.spawn_up);
        glm::dvec3 e = glm::cross(glm::dvec3(0.0, 1.0, 0.0), c);
        if (glm::length(e) < 1e-9) e = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c);
        e = glm::normalize(e);
        const glm::dvec3 n = glm::normalize(glm::cross(c, e));
        std::vector<double> gap_open, gap_road;
        std::vector<double> band_deck, band_a, band_b, band_c, band_d, band_far;
        std::size_t n_under = 0;
        double worst_under = 0.0, worst_under_out = 1e9;

        for (double y = -4000.0; y <= 4000.0; y += 12.0) {
            for (double x = -4000.0; x <= 4000.0; x += 12.0) {
                const glm::dvec3 d =
                    glm::normalize(c + e * (x / R) + n * (y / R));
                // What the machine rides, and what the screen draws under it.
                const double drive = snow_field.drive_radius_at(d);
                const double drawn = render::drawn_radius_at(
                    *snow_field.hf, d, world.planet.subdiv, world.planet.tiles);
                const double g = drive - drawn;
                // Split by whether this is plowed ground: the corridors are the
                // place a naive fold buries the road, so they get their own row
                // rather than being averaged away in the bush.
                bool on_road = false;
                if (snow_field.lines != nullptr) {
                    const world::LineHit h = snow_field.lines->nearest(d, 60.0);
                    on_road = h.found && h.dist_m <= h.half_w_m;
                }
                // ★ R2: bucket by DISTANCE OUT FROM THE CORRIDOR EDGE. Chad,
                // after driving R1: "I noticed the skis do go under sometimes."
                // A NEGATIVE gap is exactly that -- the drawn surface standing
                // above where the machine rides. The suspect is the mask ramp:
                // the mesh interpolates the fold between corners ~59 m apart,
                // so a cell straddling a plowed road carries full fold at its
                // far corner and none at the deck, and the interpolated surface
                // in between sits above the deck the sled is actually on.
                double out = 1e9;
                if (snow_field.lines != nullptr) {
                    const world::LineHit h =
                        snow_field.lines->nearest(d, 400.0);
                    if (h.found) out = h.dist_m - h.half_w_m;
                }
                if (out <= 0.0)
                    gap_road.push_back(g);
                else
                    gap_open.push_back(g);
                if (g < 0.0) {
                    n_under += 1;
                    if (out < worst_under_out) worst_under_out = out;
                    if (g < worst_under) worst_under = g;
                }
                if (out <= 0.0)
                    band_deck.push_back(g);
                else if (out < 10.0)
                    band_a.push_back(g);
                else if (out < 20.0)
                    band_b.push_back(g);
                else if (out < 40.0)
                    band_c.push_back(g);
                else if (out < 80.0)
                    band_d.push_back(g);
                else
                    band_far.push_back(g);
            }
        }
        const auto report = [](const char* name, std::vector<double>& v) {
            if (v.empty()) {
                std::printf("  %-24s (no samples)\n", name);
                return;
            }
            std::sort(v.begin(), v.end());
            const auto q = [&](double f) {
                return v[static_cast<std::size_t>(f * (v.size() - 1))];
            };
            double sum = 0.0;
            std::size_t under = 0;
            for (double x : v) {
                sum += std::fabs(x);
                if (x < 0.0) ++under;
            }
            std::printf(
                "  %-24s n=%-7zu  p10 %+.3f  p50 %+.3f  p90 %+.3f  "
                "mean|gap| %.3f  under %.1f%%\n",
                name, v.size(), q(.10), q(.50), q(.90), sum / v.size(),
                100.0 * under / v.size());
        };
        std::printf(
            "\n=== R1 SCORECARD: SEE-vs-DRIVE gap (drive_radius - "
            "drawn_radius) ===\n");
        report("open ground", gap_open);
        report("on a plowed corridor", gap_road);
        std::printf("\n  --- by distance OUT from the corridor edge ---\n");
        report("on the deck", band_deck);
        report("0-10 m out", band_a);
        report("10-20 m out", band_b);
        report("20-40 m out", band_c);
        report("40-80 m out", band_d);
        report("beyond 80 m", band_far);
        std::printf(
            "\n  SKIS-UNDER (drawn ABOVE driven): %zu samples"
            " (%.2f%%), worst %.3f m, nearest such sample %.0f m"
            " out from a corridor edge\n",
            n_under,
            100.0 * n_under /
                std::max<std::size_t>(1, gap_open.size() + gap_road.size()),
            worst_under, worst_under_out > 1e8 ? -1.0 : worst_under_out);
        std::printf(
            "  (BLOCK-VP1 baseline was ~0.77 m in bush; 0 means the machine\n"
            "   sits ON the world it is drawn in.)\n\n");
        return 0;
    }
    // ★ R1 GUARD PROBE (SEADS_BARREN_PROBE=1) -- Chad, 2026-08-26, ruling
    // the mesh fold IN but with a fence: "make sure you dont cover over my
    // barrens black rocks and shatter cones with snow though, these are
    // places with black rock that is on a slope."
    //
    // The fold puts ambient_depth_at into the DRAWN surface. The black-rock
    // shed AND its slope gate already live INSIDE that function
    // (world/snowpack.cpp:142-145), so sloped rock should shed by
    // construction -- but "should" is not a measurement, and burying his
    // barrens is the one outcome he fenced off. So: sample the real barren
    // raster on the real DEM and report the depth the fold would add, split
    // by the MESH-scale slope the gate keys on (world/snowpack.h:103).
    if (std::getenv("SEADS_BARREN_PROBE") != nullptr) {
        if (snow_field.hf == nullptr || snow_field.barren == nullptr) {
            std::fprintf(stderr, "BARREN PROBE: no height field / no barren\n");
            return 3;
        }
        const double R = snow_field.hf->R;
        const glm::dvec3 c = glm::normalize(loop.spawn_up);
        glm::dvec3 e = glm::cross(glm::dvec3(0.0, 1.0, 0.0), c);
        if (glm::length(e) < 1e-9) e = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c);
        e = glm::normalize(e);
        const glm::dvec3 n = glm::normalize(glm::cross(c, e));
        // Slope buckets keyed to the shed law: below lo it is "flat" and
        // holds snow by Chad's own fly ruling; above hi it sheds fully.
        const double lo = snow_field.p.barren_slope_lo_deg;
        const double hi = snow_field.p.barren_slope_hi_deg;
        struct Bucket {
            int n = 0;
            double sum = 0.0;
            double worst = 0.0;
        };
        Bucket flat, band, steep, verysteep;
        int n_barren = 0, n_tot = 0;
        std::vector<double> steep_depths;
        std::vector<double> all_barren;
        // Two passes: the first learns the distribution, the second buckets
        // the top decile -- the ground that actually reads as his black rock.
        double barren_cut = 0.0;
        for (int pass = 0; pass < 2; ++pass) {
            // +/- 10 km around the spawn at 40 m, i.e. the mapped ground he
            // actually rides, not the whole sphere.
            for (double y = -35000.0; y <= 35000.0; y += 100.0) {
                for (double x = -35000.0; x <= 35000.0; x += 100.0) {
                    const glm::dvec3 d =
                        glm::normalize(c + e * (x / R) + n * (y / R));
                    ++n_tot;
                    const double b = snow_field.barren->at(d);
                    all_barren.push_back(b);
                    // world/snowpack.h:103: barren is a COVERAGE product,
                    // median 0.212 on land -- a 0.5 cut selects nothing. The
                    // cut is taken from the measured distribution below, not
                    // from a round number.
                    if (b <= barren_cut) continue;
                    ++n_barren;
                    const double dep = snow_field.ambient_depth_at(d);
                    const double slope_deg =
                        snow_field.slope_at(d) * 180.0 / 3.14159265358979;
                    Bucket* bk = &flat;
                    if (slope_deg >= 20.0)
                        bk = &verysteep;
                    else if (slope_deg >= hi)
                        bk = &steep;
                    else if (slope_deg >= lo)
                        bk = &band;
                    bk->n += 1;
                    bk->sum += dep;
                    if (dep > bk->worst) bk->worst = dep;
                    if (slope_deg >= hi) steep_depths.push_back(dep);
                }
            }
            if (pass == 0) {
                std::sort(all_barren.begin(), all_barren.end());
                const auto bq = [&](double f) {
                    return all_barren[static_cast<std::size_t>(
                        f * (all_barren.size() - 1))];
                };
                std::printf(
                    "  barren coverage over the sample: p50 %.3f  p90 %.3f  "
                    "p99 %.3f  max %.3f\n",
                    bq(.50), bq(.90), bq(.99), all_barren.back());
                // Genuine mapped rock, not merely the top decile of a
                // mostly-zero raster: 0.30 is where barren_shed starts to
                // bite (world/snowpack.h barren_shed_hi).
                barren_cut = 0.30;
                n_barren = 0;
                n_tot = 0;
                flat = band = steep = verysteep = Bucket{};
                steep_depths.clear();
            }
        }
        const auto row = [](const char* name, const Bucket& b, int tot) {
            std::printf("    %-22s %7d  %5.1f%%   mean %.3f m   worst %.3f m\n",
                        name, b.n, tot > 0 ? 100.0 * b.n / tot : 0.0,
                        b.n > 0 ? b.sum / b.n : 0.0, b.worst);
        };
        std::printf(
            "\n=== R1 GUARD: snow the FOLD would add over MAPPED BLACK ROCK "
            "===\n"
            "  samples %d, of which barren>0.5: %d (%.1f%%)\n"
            "  shed gate: flat below %.0f deg, full shed above %.0f deg\n"
            "  cut: barren > %.3f (the blackest tenth)\n",
            n_tot, n_barren, n_tot > 0 ? 100.0 * n_barren / n_tot : 0.0, lo, hi,
            barren_cut);
        row("flat (<8 deg)", flat, n_barren);
        row("transition (8-12)", band, n_barren);
        row("SLOPED (>=12 deg)", steep, n_barren);
        row("  of which >=20 deg", verysteep, n_barren);
        if (!steep_depths.empty()) {
            std::sort(steep_depths.begin(), steep_depths.end());
            const auto q = [&](double f) {
                return steep_depths[static_cast<std::size_t>(
                    f * (steep_depths.size() - 1))];
            };
            std::printf(
                "  SLOPED rock depth percentiles [m]: p50 %.3f  p90 %.3f  "
                "p99 %.3f  max %.3f\n",
                q(.50), q(.90), q(.99), steep_depths.back());
        }
        std::printf("\n");
        return 0;
    }
    // ★ R3 EXPOSURE PROBE (SEADS_R3_PROBE=1) -- does reading COVERAGE off the
    // real depth field keep Chad's fence? R1 folded the field into the
    // GEOMETRY; R3 makes the same field decide where snow SHOWS, retiring the
    // planet FS's "PROVISIONAL SNOW STUB" (a private slope mask plus a private
    // barren shed, both older than the field and disagreeing with it).
    //
    // The fence is the same one he set on the fold: "dont cover over my
    // barrens black rocks ... on a slope". Arming the depth channel should
    // keep sloped rock BARE by construction -- the black-rock shed already
    // lives inside ambient_depth_at, which is exactly why the shader's own
    // barren multiply is faded out as the dial arms (double-shed otherwise).
    // "By construction" is not a measurement, so this measures it: legacy
    // coverage vs depth coverage, side by side, bucketed by the slope the
    // gate keys on.
    //
    // ⚠ HONEST LIMIT: the legacy column is recomputed HERE from the field's
    // macro normal, while the shipped shader keys `snowFlat` on the M2 normal
    // MAP and `bgate` on a mip-smoothed normalCube fetch. Those are the same
    // ~40 m scale but not the same samples, so read the legacy column as a
    // close reconstruction, not as a pixel-exact replica. The DEPTH column has
    // no such caveat -- it is the shipped expression on the shipped field.
    if (std::getenv("SEADS_R3_PROBE") != nullptr) {
        if (snow_field.hf == nullptr) {
            std::fprintf(stderr, "R3 PROBE: no height field\n");
            return 3;
        }
        const double R = snow_field.hf->R;
        const glm::dvec3 c = glm::normalize(loop.spawn_up);
        glm::dvec3 e = glm::cross(glm::dvec3(0.0, 1.0, 0.0), c);
        if (glm::length(e) < 1e-9) e = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c);
        e = glm::normalize(e);
        const glm::dvec3 n = glm::normalize(glm::cross(c, e));
        // ✅ ONE HOME NOW. This used to carry a duplicated default with a note
        // that app must not include the shader's dial; main.cpp includes
        // render/planet.h since the 2026-08-27 sweep, so the probe reads the
        // SHIP values directly and there is no second constant to drift.
        // SWEPT, not picked. The first run at the 0.35 default showed depth
        // mode putting MORE cover on sloped black rock than the stub it
        // replaces (0.307 vs 0.230), which cuts against Chad's "the really
        // steep slopes need to be even blacker" ruling -- so the honest output
        // is the CURVE he rules on, not a constant tuned until a row went
        // green (CLAUDE.md: a gate turned green by tweaking constants is still
        // a guess). Override a single value with SEADS_R3_FULLDEPTH.
        // ★ The fence dial the DEPTH column models. Seeded from the SHIP value
        // (main.cpp already includes render/planet.h for the sweep seed, so
        // there is no second constant to drift), swept with SEADS_R3_SHEDKEEP.
        const double shed_keep = [] {
            const char* e = std::getenv("SEADS_R3_SHEDKEEP");
            if (e == nullptr)
                return static_cast<double>(
                    render::SnowParams{}.barren_shed_keep);
            const double v = std::atof(e);
            return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
        }();
        std::printf("  barren_shed_keep %.2f (SEADS_R3_SHEDKEEP sweeps it)\n",
                    shed_keep);
        const char* fd_env = std::getenv("SEADS_R3_FULLDEPTH");
        std::vector<double> sweep;
        if (fd_env != nullptr)
            sweep.push_back(std::atof(fd_env));
        else
            sweep = {
                render::SnowParams{}.full_depth, 0.20, 0.35, 0.50, 0.70, 0.90};
        for (const double full_depth : sweep) {
            struct B {
                int n = 0;
                double leg = 0.0;
                double dep = 0.0;
            };
            B open, rock_flat, rock_slope, rock_steep;
            int n_tot = 0;
            for (double y = -35000.0; y <= 35000.0; y += 100.0) {
                for (double x = -35000.0; x <= 35000.0; x += 100.0) {
                    const glm::dvec3 d =
                        glm::normalize(c + e * (x / R) + n * (y / R));
                    // Skip water: the FS masks exposure by (1-water) in BOTH
                    // modes, so lake pixels cannot discriminate the two.
                    if (snow_field.landmask != nullptr &&
                        !snow_field.landmask->empty() &&
                        snow_field.landmask->at(d) > 0.5)
                        continue;
                    ++n_tot;
                    const double slope = snow_field.slope_at(d);
                    const double slope_deg = slope * 180.0 / 3.14159265358979;
                    const double b = snow_field.barren != nullptr &&
                                             !snow_field.barren->empty()
                                         ? snow_field.barren->at(d)
                                         : 0.0;
                    // LEGACY: the stub the shader ships -- smoothstep(slope_lo,
                    // 1, cos(slope)) times the barren shed x slope gate.
                    const double cos_slope = std::cos(slope);
                    const double t = (cos_slope - 0.55) / (1.0 - 0.55);
                    const double tc = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
                    const double snow_flat = tc * tc * (3.0 - 2.0 * tc);
                    const double legacy =
                        snow_flat *
                        (1.0 - snow_field.p.k_barren *
                                   snow_field.barren_shed(b) *
                                   snow_field.barren_slope_gate(slope));
                    // DEPTH: the shipped R3 expression on the shipped field.
                    const double depth = snow_field.ambient_depth_at(d);
                    const double u = depth / full_depth;
                    const double uc = u < 0.0 ? 0.0 : (u > 1.0 ? 1.0 : u);
                    double cover = uc * uc * (3.0 - 2.0 * uc);
                    // ★ THE SURVIVING BARREN SHED
                    // (SnowParams::barren_shed_keep, Chad 2026-08-27). The
                    // shipped FS multiplies coverage by (1 - max(1 - mix, keep)
                    // * k_barren * bshed * bgate); this probe reports the ARMED
                    // case (mix = 1), so the max() reduces to `keep`. Without
                    // this the DEPTH column would report a fence the shader
                    // does not draw -- and the fence is the whole reason the
                    // column exists.
                    if (shed_keep > 0.0) {
                        cover *= 1.0 - shed_keep * snow_field.p.k_barren *
                                           snow_field.barren_shed(b) *
                                           snow_field.barren_slope_gate(slope);
                    }
                    B* bk = &open;
                    if (b > 0.30) {
                        bk = &rock_flat;
                        if (slope_deg >= 20.0)
                            bk = &rock_steep;
                        else if (slope_deg >= snow_field.p.barren_slope_hi_deg)
                            bk = &rock_slope;
                    }
                    bk->n += 1;
                    bk->leg += legacy;
                    bk->dep += cover;
                }
            }
            const auto row = [](const char* name, const B& b) {
                std::printf(
                    "    %-26s %7d   legacy %.3f   DEPTH %.3f   %+.3f\n", name,
                    b.n, b.n ? b.leg / b.n : 0.0, b.n ? b.dep / b.n : 0.0,
                    b.n ? (b.dep - b.leg) / b.n : 0.0);
            };
            std::printf(
                "\n=== R3 EXPOSURE: legacy stub vs the DEPTH field (mean "
                "cover) ===\n"
                "  land samples %d   full-cover depth %.2f m\n"
                "  the FENCE is the two ROCK-sloped rows: they must stay "
                "LOW.\n",
                n_tot, full_depth);
            row("open ground", open);
            row("mapped rock, flat", rock_flat);
            row("mapped rock, SLOPED >=12", rock_slope);
            row("mapped rock, STEEP >=20", rock_steep);
        }
        std::printf("\n");
        return 0;
    }
    // ★ SF3-0 TERRAIN CENSUS (SEADS_SNOWDUMP=<out.tsv>). The depth field as it
    // is actually DRIVEN, sampled on the real DEM and the real linework --
    // never sled_probe's flat analytic fixture. Bound HERE because this is the
    // first line where snow_field carries every pointer the sled reads (hf,
    // the post-blur H1 height store; lines; barren; landmask; the facet
    // injection), so the census cannot fork from the driven surface the way a
    // standalone tool re-decoding the DEM png would -- render/planet.cpp:685
    // retains the height store POST-BLUR, and that blur is not in the asset.
    //
    // The question it answers is the one the flatscape complaint actually
    // poses: how much relief does f() put under the rider inside the band he
    // demonstrably drives? The band's bounds are not chosen -- inner is the
    // plow-bank envelope (bank_rise 3.00 + bank_fall 6.00 = edge + 9.0 m,
    // config/world.toml:473-475), outer is his own tapes (nearest-corridor
    // p50 19-24 m, max 85 m).
    //
    // Read-only, env-gated, and it EXITS before the window opens: nothing here
    // can move a frame, a golden or a seam.
    // ★ SF3-A0 THROUGHPUT BENCH (SEADS_SNOWBENCH=<n_side>). The rider patch of
    // rung A displaces every vertex by depth_at, exactly as the shipped bank
    // strips already do (render/bank_mesh.h:17) -- so the patch's resolution
    // and rebuild rate are bounded by how fast depth_at actually runs on the
    // REAL field, corridor search and all. Measured, never guessed: the
    // consult flagged two live O(N) defects on this same path
    // (snowhill_add evaluated twice per ground sample, world/snowpack.cpp:359
    // and :424), so an arithmetic estimate here would be an estimate of the
    // wrong function.
    //
    // Samples a square patch centred on the sled's own spawn ground point at
    // the cell size the patch would use, then reports ns/call and what that
    // costs per frame at 60 Hz. Read-only, exits.
    if (const char* bench_s = std::getenv("SEADS_SNOWBENCH")) {
        if (snow_field.hf == nullptr) {
            std::fprintf(stderr, "SNOWBENCH: no height field\n");
            return 3;
        }
        const int n_side = std::max(8, std::atoi(bench_s));
        const double R = snow_field.hf->R;
        // Centre on the shipped spawn direction so the corridor search sees a
        // representative neighbourhood (a patch out in empty bush would miss
        // the LineNetwork work that dominates near a road).
        const glm::dvec3 c = glm::normalize(loop.spawn_up);
        glm::dvec3 e = glm::cross(glm::dvec3(0.0, 1.0, 0.0), c);
        if (glm::length(e) < 1e-9) e = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c);
        e = glm::normalize(e);
        const glm::dvec3 n = glm::normalize(glm::cross(c, e));

        // ★ SF3-B: the track query rides the SAME hot path depth_at does, so
        // measure it LOADED, not empty. An empty field early-outs and would
        // report a cost the game never actually pays once you have driven.
        world::TrackField bench_tracks;
        if (std::getenv("SEADS_SNOWBENCH_TRACKS") != nullptr) {
            bench_tracks.reset(R, world::TrackParams{});
            // Lay a serpentine through the patch so a good share of the
            // sampled nodes land near track, as they would after a drive.
            for (int i = 0; i < 4000; ++i) {
                const double t = 0.25 * i;
                const double dx = (std::fmod(t, 160.0) - 80.0) / R;
                const double dy = (20.0 * std::sin(t * 0.05)) / R;
                bench_tracks.add(glm::normalize(c + e * dx + n * dy));
            }
            snow_field.tracks = &bench_tracks;
            std::printf("  [track field loaded: %zu stamps]\n",
                        bench_tracks.size());
        }
        const bool bench_ambient =
            std::getenv("SEADS_SNOWBENCH_AMBIENT") != nullptr;
        for (double cell_m : {1.0, 2.0, 4.0}) {
            const double half = 0.5 * cell_m * (n_side - 1);
            volatile double sink = 0.0;  // keep the calls from being elided
            const auto t0 = std::chrono::steady_clock::now();
            for (int iy = 0; iy < n_side; ++iy) {
                for (int ix = 0; ix < n_side; ++ix) {
                    const double dx = (-half + cell_m * ix) / R;
                    const double dy = (-half + cell_m * iy) / R;
                    const glm::dvec3 d = glm::normalize(c + e * dx + n * dy);
                    sink =
                        sink + (bench_ambient ? snow_field.ambient_depth_at(d)
                                              : snow_field.depth_at(d));
                }
            }
            const auto t1 = std::chrono::steady_clock::now();
            (void)sink;
            const double us =
                std::chrono::duration<double, std::micro>(t1 - t0).count();
            const int calls = n_side * n_side;
            const double span = cell_m * (n_side - 1);
            std::printf(
                "  cell %4.1f m  patch %5.0f m across  %6d verts  "
                "%9.1f us  %7.1f ns/call  %6.2f ms/frame-if-full-rebuild\n",
                cell_m, span, calls, us, 1000.0 * us / calls, us / 1000.0);
        }
        std::printf("\n  (16.7 ms is the whole 60 Hz budget.)\n\n");
        // FIELD-vs-FACET: how much of the DEM relief survives the DRAWN mesh?
        // sphere_param.h:200 measured p99 ~6 m at subdiv 200 UNTILED; the
        // shipped planet is tiles=2 (~59 m cells), so re-measure rather than
        // inherit the old number. This decides whether the rider patch can sit
        // on the facet (safe, but then it shows only snow) or must carry the
        // true field (shows the terrain too, but can sink UNDER the planet
        // mesh and intersect it -- the flashing class).
        {
            std::vector<double> dev;
            const double cell_dev = 2.0;
            const double half_dev = 0.5 * cell_dev * (n_side - 1);
            for (int iy = 0; iy < n_side; ++iy)
                for (int ix = 0; ix < n_side; ++ix) {
                    const double dx = (-half_dev + cell_dev * ix) / R;
                    const double dy = (-half_dev + cell_dev * iy) / R;
                    const glm::dvec3 d = glm::normalize(c + e * dx + n * dy);
                    dev.push_back(snow_field.hf->radius_at(d) -
                                  render::facet_radius_at(*snow_field.hf, d,
                                                          world.planet.subdiv,
                                                          world.planet.tiles));
                }
            std::sort(dev.begin(), dev.end());
            const auto q = [&](double f) {
                return dev[static_cast<std::size_t>(f * (dev.size() - 1))];
            };
            std::printf(
                "  FIELD minus FACET over the patch [m]:\n"
                "    p01 %+.3f  p10 %+.3f  p50 %+.3f  p90 %+.3f  "
                "p99 %+.3f\n    min %+.3f  max %+.3f\n\n",
                q(.01), q(.10), q(.50), q(.90), q(.99), dev.front(),
                dev.back());
        }

        // FIELD-vs-FACET: how much of the DEM relief survives the DRAWN mesh?
        // sphere_param.h:200 measured p99 ~6 m at subdiv 200 UNTILED; the
        // shipped planet is tiles=2 (~59 m cells), so re-measure rather than
        // inherit the old number. This decides whether the rider patch can sit
        // on the facet (safe, but then it shows only snow) or must carry the
        // true field (shows the terrain too, but can sink UNDER the planet
        // mesh and intersect it -- the flashing class).
        {
            std::vector<double> dev;
            const double cell_dev = 2.0;
            const double half_dev = 0.5 * cell_dev * (n_side - 1);
            for (int iy = 0; iy < n_side; ++iy)
                for (int ix = 0; ix < n_side; ++ix) {
                    const double dx = (-half_dev + cell_dev * ix) / R;
                    const double dy = (-half_dev + cell_dev * iy) / R;
                    const glm::dvec3 d = glm::normalize(c + e * dx + n * dy);
                    dev.push_back(snow_field.hf->radius_at(d) -
                                  render::facet_radius_at(*snow_field.hf, d,
                                                          world.planet.subdiv,
                                                          world.planet.tiles));
                }
            std::sort(dev.begin(), dev.end());
            const auto q = [&](double f) {
                return dev[static_cast<std::size_t>(f * (dev.size() - 1))];
            };
            std::printf(
                "  FIELD minus FACET over the patch [m]:\n"
                "    p01 %+.3f  p10 %+.3f  p50 %+.3f  p90 %+.3f  "
                "p99 %+.3f\n    min %+.3f  max %+.3f\n\n",
                q(.01), q(.10), q(.50), q(.90), q(.99), dev.front(),
                dev.back());
        }

        return 0;
    }

    // ============================ SEADS_DECK_COST ============================
    // ★ ROAD-REPAIR P1 (red-team 2026-09-09) -- THE ON-CORRIDOR PER-SAMPLE
    // COST OF THE SINK FLOOR, measured where the sled actually pays it.
    //
    // The sink-fix commit quoted +5.8 us/sample averaged over a 444,889-sample
    // GRID, most of whose samples are off-corridor and take one of
    // apply_deck_floor's three early-outs. That number says nothing about the
    // sample the machine pays for while it is ON a road -- which is the only
    // sample the floor costs anything for. This block measures THAT one, on
    // the real baked linework, in the sled's own access pattern: kPatches (3)
    // laterals x SledParams::substeps (12) substeps per frame, 60 frames/s, at
    // 15 m/s along the centreline of the longest plowed run on the map.
    //
    // Three arms, one process, one dir list, so nothing but the code under
    // test differs:
    //   A  FLOOR OFF        drawn_radius_fn nulled == SEADS_DECK_FLOOR=0
    //   B  FLOOR ON, MEMO OFF   the sink-fix commit exactly as it shipped
    //   C  FLOOR ON, MEMO ON    with the fold-corner memo (the P1 fix)
    // and world::line_nearest_calls() counts the corridor lookups per sample in
    // each arm, so the W1.1 "one lookup per ground sample" rule is a NUMBER.
    // Exits when done -- a measurement run, not a flight.
    if (std::getenv("SEADS_DECK_COST") != nullptr) {
        if (snow_field.hf == nullptr || snow_field.lines == nullptr ||
            snow_field.lines->empty()) {
            std::fprintf(stderr, "DECK_COST: no height field / no linework\n");
            return 3;
        }
        const world::HeightField& hf = *snow_field.hf;
        const double R = hf.R;
        render::BankBuildParams bp;
        bp.station_m = 16.0;
        bp.lift_m = static_cast<float>(world.ribbons.lift_m);
        // Plowed roads only (kinds 0/1) -- the corridor class the sink was
        // reported on -- and the SAME resampler the banks and the census use.
        const std::vector<render::BankRun> runs =
            render::recover_bank_runs(hf, bp, {0, 1});
        const render::BankRun* best = nullptr;
        for (const render::BankRun& r : runs)
            if (r.ctr.size() >= 2 &&
                (best == nullptr || r.ctr.size() > best->ctr.size()))
                best = &r;
        if (best == nullptr || best->s.size() != best->ctr.size()) {
            std::fprintf(stderr, "DECK_COST: no usable plowed run\n");
            return 3;
        }

        // Walk the run by ARC LENGTH at the substep cadence. `i` only ever
        // advances (L is monotone), so this is a linear walk, not a search.
        const double speed_mps = 15.0;
        const double dt = 1.0 / 60.0;
        const int substeps = sled_params.substeps;
        const double step_m = speed_mps * dt / substeps;
        // The three patches, spread the way the machine's are: the two skis
        // either side, the track down the middle.
        const double lat_m[sim::kPatches] = {-0.55, +0.55, 0.0};
        const std::size_t max_samples = 240000;
        std::vector<glm::dvec3> dirs;
        dirs.reserve(max_samples);
        std::size_t i = 0;
        for (double L = best->s.front();
             L <= best->s.back() && dirs.size() + sim::kPatches <= max_samples;
             L += step_m) {
            while (i + 2 < best->s.size() &&
                   static_cast<double>(best->s[i + 1]) < L)
                ++i;
            const double s0 = best->s[i], s1 = best->s[i + 1];
            const double t = (L - s0) / std::max(1e-6, s1 - s0);
            const glm::dvec3 c = glm::normalize(best->ctr[i] * (1.0 - t) +
                                                best->ctr[i + 1] * t);
            glm::dvec3 tg = best->ctr[i + 1] - best->ctr[i];
            tg -= c * glm::dot(tg, c);
            if (glm::length(tg) < 1e-12) continue;
            const glm::dvec3 perp = glm::normalize(glm::cross(glm::normalize(tg), c));
            for (int k = 0; k < sim::kPatches; ++k)
                dirs.push_back(glm::normalize(c + perp * (lat_m[k] / R)));
        }
        if (dirs.empty()) {
            std::fprintf(stderr, "DECK_COST: no samples\n");
            return 3;
        }
        std::size_t on_corridor = 0;
        for (const glm::dvec3& d : dirs) {
            const world::LineHit h = snow_field.lines->nearest(d, 60.0);
            if (h.found && h.dist_m <= h.half_w_m) ++on_corridor;
        }
        std::printf(
            "\n=== DECK FLOOR COST (on-corridor, the sled's own cadence) ===\n"
            "  run path_id %d kind %d, %zu stations, %.0f m of road\n"
            "  %zu samples (%d patches x %d substeps @ %.0f m/s, %.4f m "
            "apart), %zu ON the deck (%.1f %%)\n",
            best->path_id, best->kind, best->ctr.size(),
            static_cast<double>(best->s.back() - best->s.front()), dirs.size(),
            sim::kPatches, substeps, speed_mps, step_m, on_corridor,
            100.0 * static_cast<double>(on_corridor) / dirs.size());

        auto arm = [&](const char* name, bool report) {
            world::reset_line_nearest_calls();
            double checksum = 0.0;
            const auto t0 = std::chrono::steady_clock::now();
            for (const glm::dvec3& d : dirs)
                checksum += snow_field.sample_at(d, false).drive_r;
            const auto t1 = std::chrono::steady_clock::now();
            const double us =
                std::chrono::duration<double, std::micro>(t1 - t0).count() /
                static_cast<double>(dirs.size());
            const double lookups =
                static_cast<double>(world::line_nearest_calls()) /
                static_cast<double>(dirs.size());
            if (report)
                std::printf(
                    "  %-24s %8.3f us/sample   %5.2f lines->nearest()/sample"
                    "   (checksum %.6f)\n",
                    name, us, lookups, checksum);
            return us;
        };

        // Arm A: the floor OFF -- the honest pre-repair cost.
        std::function<double(glm::dvec3)> saved = snow_field.drawn_radius_fn;
        snow_field.drawn_radius_fn = nullptr;
        arm("(warm)", false);
        const double a1 = arm("A FLOOR OFF", true);
        const double a2 = arm("A FLOOR OFF (again)", true);
        // Arm B: the floor ON with the memo OFF -- the sink-fix commit as it
        // shipped, three fold lookups per corridor sample.
        snow_field.drawn_radius_fn = saved;
        render::set_facet_fold_memo(false);
        arm("(warm)", false);
        const double b1 = arm("B FLOOR ON, MEMO OFF", true);
        const double b2 = arm("B FLOOR ON, MEMO OFF (again)", true);
        // Arm C: the floor ON with the memo ON -- what this commit ships.
        render::set_facet_fold_memo(true);
        arm("(warm)", false);
        const double c1 = arm("C FLOOR ON, MEMO ON", true);
        const double c2 = arm("C FLOOR ON, MEMO ON (again)", true);

        // And the floor is LIVE on this run, said as a number rather than
        // assumed: how far arm C lifted the drive against arm A.
        double lifted_max = 0.0, lifted_sum = 0.0;
        std::size_t lifted_n = 0;
        for (const glm::dvec3& d : dirs) {
            const double on = snow_field.sample_at(d, false).drive_r;
            snow_field.drawn_radius_fn = nullptr;
            const double off = snow_field.sample_at(d, false).drive_r;
            snow_field.drawn_radius_fn = saved;
            const double dd = on - off;
            if (dd > 1e-9) {
                ++lifted_n;
                lifted_sum += dd;
                lifted_max = std::max(lifted_max, dd);
            }
        }
        std::printf(
            "  FLOOR COST  B-A %+.3f / %+.3f us/sample   C-A %+.3f / %+.3f "
            "us/sample   (memo saves %.3f us/sample)\n"
            "  FLOOR LIVE  %zu of %zu samples raised, mean %+.3f m, max "
            "%+.3f m\n\n",
            b1 - a1, b2 - a2, c1 - a1, c2 - a2, 0.5 * ((b1 - c1) + (b2 - c2)),
            lifted_n, dirs.size(),
            lifted_n ? lifted_sum / static_cast<double>(lifted_n) : 0.0,
            lifted_max);
        return 0;
    }

    // ============================ SEADS_ROAD_CENSUS ==========================
    // ★ ROAD-REPAIR (Chad, 2026-09-08): "the Sudburian and the snowmachine
    // sink into the road; snowbank road sections are jagged; sparkle drops at
    // road edges." Three complaints, one missing instrument: nothing in this
    // tree could say, as a NUMBER, how far the DRAWN road deck sits from the
    // DRIVEN surface underneath it.
    //
    // This is that instrument, and it lives HERE for the same reason
    // SEADS_SNOWDUMP does: the census needs the REAL baked DEM, the REAL
    // landmask and the REAL linework, and those reach the world only through
    // render::planet_heightfield/landmask/linework -- the app layer is the one
    // place that holds them together with the shipped [planet] subdiv/tiles.
    // A standalone tool would have to re-load the planet, which is exactly the
    // second elevation source INV-1 forbids.
    //
    // The measuring itself is NOT here: render/road_census.* is pure, and the
    // two facet functions reach it INJECTED (the facet_radius_fn pattern a few
    // hundred lines above), so no layering is bent to take a measurement.
    //
    //   SEADS_ROAD_CENSUS=<path.tsv>   write the per-station rows there
    //   SEADS_ROAD_CENSUS_STRIDE=<n>   emit every n-th STATION (all of its
    //                                  laterals) to the TSV. The SUMMARY
    //                                  always walks EVERY station; the full
    //                                  TSV is 2.12M rows / 267 MB, which is
    //                                  not a thing to commit. Striding by
    //                                  STATION, never by row: 8 laterals per
    //                                  station means a row stride that shares
    //                                  a factor with 8 would ship one lateral
    //                                  and drop the other seven.
    // Exits when done -- it is a measurement run, not a flight.
    if (const char* census_path = std::getenv("SEADS_ROAD_CENSUS")) {
        if (snow_field.hf == nullptr || snow_field.lines == nullptr ||
            snow_field.lines->empty()) {
            std::fprintf(stderr,
                         "ROAD_CENSUS: no height field / no linework\n");
            return 3;
        }
        const world::HeightField& hf = *snow_field.hf;
        const double R = hf.R;
        const int subdiv = world.planet.subdiv, tiles = world.planet.tiles;

        // The SAME stations the banks are built on -- recover_bank_runs is the
        // one resampler, shared (bank_mesh.h). Every drawn ribbon kind, not
        // just the plowed ones: the trail sinks too, and kind 3 (river) is
        // carried so the table can say it was looked at.
        render::BankBuildParams bp;
        bp.station_m = 16.0;
        bp.lift_m = static_cast<float>(world.ribbons.lift_m);
        const std::vector<render::BankRun> runs =
            render::recover_bank_runs(hf, bp, {0, 1, 2, 3});
        long n_st = 0;
        for (const render::BankRun& r : runs)
            n_st += static_cast<long>(r.ctr.size());
        std::printf(
            "\n=== ROAD-GAP CENSUS === %zu runs, %ld stations @ %.0f m\n",
            runs.size(), n_st, bp.station_m);

        // ★ ROAD-REPAIR ONAPING RUNG 2 -- A THING THAT WAS CHECKED AND IS
        // NOT A PROBLEM, recorded so nobody spends the afternoon on it twice.
        // render::drawn_radius_at(hf, d, N, tiles) is the DRAPE-FACING form: it
        // reads a GLOBAL fold provider that only render/draw.cpp's
        // ensure_planet binds. This census runs headless, so it LOOKS as though
        // `drawn` here might be the bare terrain facet rather than the planet
        // mesh -- which would have made every `drawn`/`float_*x_m` column of
        // rung 1 a measurement of the wrong surface. It was tested by binding
        // an identical provider here before the walk: EVERY percentile in
        // §1.4 and §2 came back to the digit (|gap| p50 0.548 / p90 1.073 /
        // max 3.759 planet-wide, both ways). The planet is already loaded, and
        // therefore the fold already bound, by the time this block runs.
        render::RoadCensusHooks hooks;
        hooks.facet_r = [&hf, subdiv, tiles](glm::dvec3 d) {
            return render::facet_radius_at(hf, d, subdiv, tiles);
        };
        hooks.drawn_r = [&hf, subdiv, tiles](glm::dvec3 d) {
            return render::drawn_radius_at(hf, d, subdiv, tiles);
        };
        // F8: ONE lift dial. The ribbon drape and the bank strip are both
        // [ribbons] lift_m; re-typing 0.45 here would be the second dial.
        hooks.ribbon_lift_m = world.ribbons.lift_m;
        hooks.bank_lift_m = world.ribbons.lift_m;
        // ★ ROAD-REPAIR ONAPING RUNG 2 -- THE APRON RULER's dials.
        // `apron_m` is the LIVE one (zeroed by SEADS_NO_APRON, so the same exe
        // writes the before and the after), while the PROBE and the SAMPLE
        // distances are the shipped number either way: a before/after table
        // has to be two readings of ONE ruler, not two rulers.
        hooks.apron_m = apron_m_live;
        // ★ THE RULER'S OWN DISTANCE, A CONSTANT -- 12.0 m, two burial
        // skirts. It is deliberately NOT [bank_mesh] apron_m: a ruler whose
        // graduations move when the dial moves cannot compare a before to an
        // after, and the committed Valley slices beside this doc would stop
        // being readable the day the dial changed. apron_m is the COVERAGE
        // (what the mesh actually draws); this is where the ruler LOOKS.
        const double apron_probe_m = 12.0;
        hooks.apron_probe_m = apron_probe_m;
        hooks.apron_sample_m = apron_probe_m;
        hooks.apron_tol_m = world.bank_mesh.apron_tol_m;
        hooks.apron_min_drop_m = world.bank_mesh.apron_min_drop_m;
        hooks.skirt_bury_m = world.bank_mesh.skirt_bury_m;
        {
            // The apron's ring count is DERIVED in render/bank_mesh, never
            // re-typed here -- the ruler asks the mesh how many rings it has.
            render::BankBuildParams ap = bp;
            ap.apron_m = apron_probe_m;
            ap.skirt_m = world.bank_mesh.skirt_m;
            ap.skirt_rings = world.bank_mesh.skirt_rings;
            hooks.apron_rings = render::bank_apron_ring_count(ap);
        }
        world::reset_line_blend_overflows();
        const std::vector<render::RoadCensusRow> rows =
            render::road_census(runs, snow_field, hooks);
        std::printf("  %zu rows (%zu laterals/station)\n", rows.size(),
                    render::road_census_lateral_fracs().size() + 1);

        // --- the TSV ---
        long stride = 1;
        if (const char* e = std::getenv("SEADS_ROAD_CENSUS_STRIDE"))
            stride = std::max(1L, std::atol(e));
        // "wb", not "w": on Windows a TEXT-mode stream translates every
        // newline into CRLF, so the census TSV landed in the working tree
        // with 35,409 CRs the index does not have (.gitattributes pins the repo
        // to LF). Fixed at the WRITE, which is the only place it can be
        // fixed: a re-run of the probe would otherwise re-dirty the file.
        std::FILE* fp = std::fopen(census_path, "wb");
        if (fp == nullptr) {
            std::fprintf(stderr, "ROAD_CENSUS: cannot open %s\n", census_path);
            return 3;
        }
        std::fprintf(fp,
                     "path_id\tkind\tstation_s\tlateral_m\tdrive_minus_facet\t"
                     "ribbon_minus_drive\tbank_minus_drive\t"
                     "drawn_minus_drive\tsurface\thalf_w_m\tjunction_m\t"
                     "seg_id\td_half_w_m\td_drive_r_m\tis_crest\t"
                     "q_half_w_m\td_q_half_w_m\td_drive_r_lat_m\t"
                     // ★ ROAD-REPAIR P2 (red-team 2026-09-09): the headline
                     // C0 instrument belongs IN the committed artefact.
                     // crest_grade is the local |d(drive_r)/ds| central
                     // difference ALONG the run over census_grade_fine_m,
                     // and it is nonzero on CREST rows only
                     // (is_crest == 1) -- every other row carries 0.0000
                     // by construction, not by omission.
                     "crest_grade\t"
                     "dir_x\tdir_y\tdir_z\t"
                     // ★ ROAD-REPAIR ONAPING RUNG 1: the two edge
                     // measurements, APPENDED so every reader of the older
                     // TSVs keeps its column indices. Per-STATION values,
                     // stamped on all eight rows of the station (the
                     // d_half_w_m idiom). skirt_drop_m > 0 == the terrain
                     // falls away outward across the burial skirt;
                     // float_*x_m > 0 == the drawn planet stands above the
                     // driven surface at that multiple of half_w.
                     "skirt_drop_m\tskirt_side\t"
                     "float_2x_m\tfloat_3x_m\tfloat_4x_m\tfloat_side\t"
                     // ★ ROAD-REPAIR ONAPING RUNG 2, appended again so
                     // every rung-1 reader keeps its column indices.
                     // Per-STATION, measured on the SKIRT SIDE.
                     // apron_gap_*_m = what the EYE IS SHOWN minus
                     // what the MACHINE STANDS ON, at the outer skirt
                     // ring (A -- the control: the apron must not move
                     // it), at skirt + apron/2 (B) and at skirt +
                     // apron (C). NEGATIVE == the drawn ground is
                     // BELOW the surface you ride on.
                     "apron_gap_a_m\tapron_gap_b_m\tapron_gap_c_m\t"
                     "apron_reach_m\n");
        const std::size_t per_st =
            render::road_census_lateral_fracs().size() + 1;
        long emitted = 0;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if ((i / per_st) % static_cast<std::size_t>(stride) != 0) continue;
            ++emitted;
            const render::RoadCensusRow& r = rows[i];
            std::fprintf(
                fp,
                "%d\t%d\t%.1f\t%+.2f\t%+.4f\t%+.4f\t%+.4f\t%+.4f\t%s\t%.2f\t"
                "%.1f\t%lld\t%+.3f\t%+.4f\t%d\t%.3f\t%+.3f\t%+.4f\t%.4f\t"
                "%.9f\t%.9f\t%.9f\t"
                "%+.4f\t%d\t%+.4f\t%+.4f\t%+.4f\t%d\t"
                "%+.4f\t%+.4f\t%+.4f\t%.2f\n",
                r.path_id, r.kind, r.station_s, r.lateral_m,
                r.drive_minus_facet, r.ribbon_minus_drive, r.bank_minus_drive,
                r.drawn_minus_drive,
                world::surface_name(static_cast<world::Surface>(r.surf)),
                r.half_w_m, std::min(r.junction_m, 99999.0),
                static_cast<long long>(r.seg_id), r.d_half_w_m, r.d_drive_r_m,
                r.is_crest ? 1 : 0, r.q_half_w_m, r.d_q_half_w_m,
                r.d_drive_r_lat_m, r.crest_grade, r.dir.x, r.dir.y, r.dir.z,
                r.skirt_drop_m, r.skirt_side, r.float_2x_m, r.float_3x_m,
                r.float_4x_m, r.float_side, r.apron_gap_a_m,
                r.apron_gap_b_m, r.apron_gap_c_m, r.apron_reach_m);
        }
        std::fclose(fp);
        std::printf("  TSV -> %s (every %ld-th station, %ld rows)\n",
                    census_path, stride, emitted);

        // --- the summary ---
        const double grade_bound = snow_field.p.bank_max_grade;
        auto show = [](const render::RoadCensusStats& st) {
            std::printf(
                "  %-26s rows %8ld  stations %7ld | ribbon |d| p50 %.3f p90 "
                "%.3f p99 %.3f max %.3f | bank |d| p50 %.3f p90 %.3f p99 %.3f "
                "max %.3f | SINK %ld  STEPS %ld | CORNER %ld/%ld  |dqw| "
                "p50 %.3f p99 %.3f max %.3f | CREST GRADE p50 %.3f p90 %.3f "
                "p99 %.3f max %.3f  over-bound %ld/%ld\n",
                st.label.c_str(), st.rows, st.stations, st.ribbon_p50,
                st.ribbon_p90, st.ribbon_p99, st.ribbon_max, st.bank_p50,
                st.bank_p90, st.bank_p99, st.bank_max, st.sink_stations,
                st.width_steps, st.corner_steps, st.corner_rows,
                st.d_q_half_w_p50, st.d_q_half_w_p99, st.d_q_half_w_max,
                st.grade_p50, st.grade_p90, st.grade_p99, st.grade_max,
                st.grade_over_bound, st.grade_rows);
        };
        std::printf("\n-- PLANET-WIDE --\n");
        show(render::road_census_stats(rows, "all kinds", nullptr, grade_bound));
        // The kinds that are actually DRAPED as ribbons. kind 3 (river) is
        // drawn as a mirror water disc by render/river_surfaces, not by
        // build_path_mesh, so its `ribbon` column is a hypothetical -- it is
        // carried for completeness and excluded from the headline.
        show(render::road_census_stats(
            rows, "drawn ribbons (0/1/2)",
            [](const render::RoadCensusRow& r) { return r.kind != 3; },
            grade_bound));
        const char* kind_name[4] = {"road_major", "road_minor", "trail",
                                    "river"};
        for (int k = 0; k < 4; ++k)
            show(render::road_census_stats(
                rows, kind_name[k],
                [k](const render::RoadCensusRow& r) { return r.kind == k; },
                grade_bound));

        // --- within 2500 m of each pump ---
        const glm::dvec3 pumps[2] = {world::kPumpValleySurface,
                                     world::kPumpSudburySurface};
        const char* pump_name[2] = {"PUMP valley", "PUMP sudbury"};
        for (int pi = 0; pi < 2; ++pi) {
            const glm::dvec3 P = pumps[pi];
            auto near_pump = [P, R](const render::RoadCensusRow& r) {
                return R * std::acos(std::clamp(glm::dot(r.dir, P), -1.0,
                                                1.0)) <= 2500.0;
            };
            std::printf("\n-- %s, within 2500 m --\n", pump_name[pi]);
            show(render::road_census_stats(rows, "all kinds", near_pump,
                                           grade_bound));
            for (int k = 0; k < 4; ++k)
                show(render::road_census_stats(
                    rows, kind_name[k],
                    [k, near_pump](const render::RoadCensusRow& r) {
                        return r.kind == k && near_pump(r);
                    },
                    grade_bound));
        }

        // --- the worst 10 stations by |ribbon - drive| ---
        std::vector<std::size_t> idx(rows.size());
        for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::partial_sort(
            idx.begin(),
            idx.begin() + static_cast<long long>(std::min<std::size_t>(
                              10, idx.size())),
            idx.end(), [&rows](std::size_t a, std::size_t b) {
                return std::fabs(rows[a].ribbon_minus_drive) >
                       std::fabs(rows[b].ribbon_minus_drive);
            });
        std::printf(
            "\n-- WORST 10 by |ribbon - drive| --\n"
            "  #  kind        s_m     lat_m  rib-drv  bank-drv  drv-facet  "
            "half_w  junc_m   surface        dir(x,y,z)            "
            "valley_km sudbury_km\n");
        for (std::size_t k = 0; k < std::min<std::size_t>(10, idx.size());
             ++k) {
            const render::RoadCensusRow& r = rows[idx[k]];
            const double dv =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpValleySurface), -1.0, 1.0));
            const double ds =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpSudburySurface), -1.0,
                        1.0));
            std::printf(
                "  %2zu %-10s %8.1f %+7.2f %+8.3f %+9.3f %+10.3f %6.2f %7.1f  "
                "%-14s (%+.6f,%+.6f,%+.6f) %8.2f %9.2f\n",
                k + 1, kind_name[r.kind & 3], r.station_s, r.lateral_m,
                r.ribbon_minus_drive, r.bank_minus_drive, r.drive_minus_facet,
                r.half_w_m, std::min(r.junction_m, 99999.0),
                world::surface_name(static_cast<world::Surface>(r.surf)),
                r.dir.x, r.dir.y, r.dir.z, dv * 0.001, ds * 0.001);
        }

        // --- the worst 10 SINKS: centreline stations of a DRAWN ribbon where
        // the deck stands highest above the surface the body is placed on.
        // This is Chad's first complaint stated as a number.
        std::vector<std::size_t> sidx;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].is_centre && rows[i].kind != 3) sidx.push_back(i);
        std::partial_sort(
            sidx.begin(),
            sidx.begin() + static_cast<long long>(
                               std::min<std::size_t>(10, sidx.size())),
            sidx.end(), [&rows](std::size_t a, std::size_t b) {
                return rows[a].ribbon_minus_drive >
                       rows[b].ribbon_minus_drive;
            });
        std::printf(
            "\n-- WORST 10 SINKS (centreline, drawn ribbons) --\n"
            "  #  kind        s_m    rib-drv  drv-facet  half_w  junc_m   "
            "surface        dir(x,y,z)            valley_km sudbury_km\n");
        for (std::size_t k = 0; k < std::min<std::size_t>(10, sidx.size());
             ++k) {
            const render::RoadCensusRow& r = rows[sidx[k]];
            const double dv =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpValleySurface), -1.0, 1.0));
            const double ds =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpSudburySurface), -1.0,
                        1.0));
            std::printf(
                "  %2zu %-10s %8.1f %+8.3f %+10.3f %6.2f %7.1f  %-14s "
                "(%+.6f,%+.6f,%+.6f) %8.2f %9.2f\n",
                k + 1, kind_name[r.kind & 3], r.station_s,
                r.ribbon_minus_drive, r.drive_minus_facet, r.half_w_m,
                std::min(r.junction_m, 99999.0),
                world::surface_name(static_cast<world::Surface>(r.surf)),
                r.dir.x, r.dir.y, r.dir.z, dv * 0.001, ds * 0.001);
        }

        // --- the worst 10 CORNER STEPS (ROAD-REPAIR C0 corner blend) -----
        // A corner step is a row where the corridor query handed this lateral
        // a DIFFERENT road's half-width than it handed the same lateral one
        // station earlier, and the driven radius moved with it. That is the
        // jag: the bank does not step because the bake changed width, it steps
        // because the argmin flipped to the road across the intersection.
        std::vector<std::size_t> cidx;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].has_prev && rows[i].kind != 3 &&
                std::fabs(rows[i].d_drive_r_lat_m) > render::kCornerDriveStepM)
                cidx.push_back(i);
        std::partial_sort(
            cidx.begin(),
            cidx.begin() + static_cast<long long>(
                               std::min<std::size_t>(10, cidx.size())),
            cidx.end(), [&rows](std::size_t a, std::size_t b) {
                return std::fabs(rows[a].d_q_half_w_m) >
                       std::fabs(rows[b].d_q_half_w_m);
            });
        std::printf(
            "\n-- WORST 10 CORNER STEPS (drawn ribbons, |d_drive_r| > "
            "%.2f m) --\n"
            "  #  kind        s_m     lat_m   d_qhw   qhw   d_drv  junc_m  "
            "half_w  surface        dir(x,y,z)            valley_km "
            "sudbury_km\n",
            render::kCornerDriveStepM);
        for (std::size_t k = 0; k < std::min<std::size_t>(10, cidx.size());
             ++k) {
            const render::RoadCensusRow& r = rows[cidx[k]];
            const double dv =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpValleySurface), -1.0, 1.0));
            const double ds =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpSudburySurface), -1.0,
                        1.0));
            std::printf(
                "  %2zu %-10s %8.1f %+7.2f %+7.3f %5.2f %+7.3f %7.1f %6.2f  "
                "%-14s (%+.6f,%+.6f,%+.6f) %8.2f %9.2f\n",
                k + 1, kind_name[r.kind & 3], r.station_s, r.lateral_m,
                r.d_q_half_w_m, r.q_half_w_m, r.d_drive_r_lat_m,
                std::min(r.junction_m, 99999.0), r.half_w_m,
                world::surface_name(static_cast<world::Surface>(r.surf)),
                r.dir.x, r.dir.y, r.dir.z, dv * 0.001, ds * 0.001);
        }
        // --- the worst 10 CREST GRADES: the C0 table ---------------------
        // These are the ten crest samples where the surface under a ski is
        // steepest ALONG the road. Unlike the 16 m station deltas above, this
        // is a local measurement, so it is the one that can tell a step from a
        // ramp -- and it is the list the next round should be aimed at.
        std::vector<std::size_t> gidx;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].is_crest && rows[i].kind < 2) gidx.push_back(i);
        std::partial_sort(
            gidx.begin(),
            gidx.begin() + static_cast<long long>(
                               std::min<std::size_t>(10, gidx.size())),
            gidx.end(), [&rows](std::size_t a, std::size_t b) {
                return rows[a].crest_grade > rows[b].crest_grade;
            });
        std::printf(
            "\n-- WORST 10 CREST GRADES (plowed roads, |d drive_r/ds| over "
            "%.2f m along the run) --\n"
            "  #  kind        s_m     lat_m   grade   qhw  junc_m  half_w  "
            "surface        dir(x,y,z)            valley_km sudbury_km\n",
            hooks.fine_m);
        for (std::size_t k = 0; k < std::min<std::size_t>(10, gidx.size());
             ++k) {
            const render::RoadCensusRow& r = rows[gidx[k]];
            const double dv =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpValleySurface), -1.0, 1.0));
            const double ds =
                R * std::acos(std::clamp(
                        glm::dot(r.dir, world::kPumpSudburySurface), -1.0,
                        1.0));
            std::printf(
                "  %2zu %-10s %8.1f %+7.2f %7.3f %5.2f %7.1f %6.2f  %-14s "
                "(%+.6f,%+.6f,%+.6f) %8.2f %9.2f\n",
                k + 1, kind_name[r.kind & 3], r.station_s, r.lateral_m,
                r.crest_grade, r.q_half_w_m, std::min(r.junction_m, 99999.0),
                r.half_w_m,
                world::surface_name(static_cast<world::Surface>(r.surf)),
                r.dir.x, r.dir.y, r.dir.z, dv * 0.001, ds * 0.001);
        }

        // The blend's ONE soundness assumption, as a number: a query whose
        // whole 16-slot candidate buffer was still inside the band could have
        // dropped a competitor that still carried weight. It must be 0.
        std::printf(
            "\n  corner_blend_m = %.3f m   blend buffer overflows: %lld  "
            "(must be 0)\n",
            snow_field.p.corner_blend_m, world::line_blend_overflows());

        // --- THE DRAWN-BANK CONTINUITY METRIC (ROAD-REPAIR, Chad's second
        // complaint: "snowbank road sections are jagged") ------------------
        //
        // Everything above measures the drawn surfaces against the DRIVEN one
        // at a station. None of it could see a JAG, because a jag is not a
        // registration error: it is a discontinuity BETWEEN CONSECUTIVE
        // STATIONS of the drawn strip. So this section builds the bank strips
        // twice, in one process, against one field --
        //
        //   arm A  the pre-repair geometry, exactly: taper_caps off (a culled
        //          station breaks the strip BEFORE it, leaving the last
        //          full-height station as an open wall), one skirt ring (which
        //          smoothstep(1) == 1 puts on facet - skirt_bury_m, where it
        //          always was), and no ring refinement (the shipped 7 knots).
        //   arm B  the shipped repair.
        //
        // -- and reports render::bank_strip_continuity for each: the max
        // radial step between consecutive stations of one strip at the same
        // ring, and the height of the wall each strip END still stands on.
        // The driven step is the census's own d_drive_r_m at the crest, over
        // the same runs, so "the drawn step vs the driven step" is a
        // comparison of two measured numbers and not of a number against a
        // hope.
        {
            const std::vector<render::CutDisk>& cuts = planet_cuts;
            render::BankBuildParams pa;  // arm A: the pre-repair geometry
            pa.station_m = bp.station_m;
            pa.lift_m = bp.lift_m;
            pa.taper_caps = false;
            pa.skirt_rings = 1;
            pa.chord_tol_m = 0.0;
            render::BankBuildParams pb;  // arm B: uniform 16 m stations
            pb.station_m = bp.station_m;
            pb.lift_m = bp.lift_m;
            // ★ ROAD-REPAIR arm C: census_after_banks.md §2's NAMED FOLLOW-UP
            // -- the same taper, resampled SHORT where the junction gap is
            // fading, so the cap's fall is spent over three or four stations
            // instead of one 16 m one. The dial comes from the shipped config,
            // so arm C is what the world actually builds; B is C's identity
            // value (junction_station_m = 0) and therefore the honest before.
            render::BankBuildParams pc;  // arm C: + short junction stations
            pc.station_m = bp.station_m;
            pc.lift_m = bp.lift_m;
            pc.junction_station_m = world.bank_mesh.junction_station_m;
            auto arm = [&](const char* name,
                           const render::BankBuildParams& p) {
                long verts = 0;
                const std::vector<render::BankStripCPU> strips =
                    render::build_bank_strips(hf, subdiv, tiles, snow_field, p,
                                              cuts, &verts);
                const int nr =
                    static_cast<int>(render::bank_ring_offsets(
                                         snow_field.p, p.chord_tol_m,
                                         p.max_rings)
                                         .size()) +
                    std::max(1, p.skirt_rings);
                // The crest ring, found the way build_bank_surfaces finds it:
                // the ring of THIS p's own knot list whose composed profile is
                // tallest, normalized. Asked of the same list the geometry
                // used, so the two arms are each measured at their own crest
                // and not at a shared guess.
                const std::vector<double> knots = render::bank_ring_offsets(
                    snow_field.p, p.chord_tol_m, p.max_rings);
                const double span =
                    snow_field.p.bank_rise_m + snow_field.p.bank_fall_m;
                double best_e = snow_field.p.bank_rise_m, best_h = -1.0;
                for (double e : knots) {
                    const double feather =
                        e < snow_field.p.corridor_edge_m
                            ? e / std::max(1e-6, snow_field.p.corridor_edge_m)
                            : 1.0;
                    const double hh = feather + snow_field.bank_profile(e);
                    if (hh > best_h) {
                        best_h = hh;
                        best_e = e;
                    }
                }
                const render::BankStripContinuity c =
                    render::bank_strip_continuity(strips, nr,
                                                  best_e / std::max(1e-6, span));
                std::printf(
                    "  %-22s strips %6zu verts %9ld rings %2d | CREST step p50 "
                    "%.3f p90 %.3f p99 %.3f max %.3f | any-ring step p50 %.3f "
                    "p90 %.3f max %.3f (>0.5 m: %ld / %ld) | strip ends %6ld  "
                    "wall p50 %.3f p99 %.3f max %.3f\n",
                    name, strips.size(), verts, nr, c.crest_p50, c.crest_p90,
                    c.crest_p99, c.crest_max, c.step_p50, c.step_p90,
                    c.step_max, c.steps_over_50cm, c.pairs, c.strip_ends,
                    c.end_amp_p50, c.end_amp_p99, c.end_amp_max);
            };
            std::printf(
                "\n-- DRAWN-BANK CONTINUITY (plowed roads, both arms, one "
                "field) --\n");
            arm("A pre-repair", pa);
            arm("B repaired", pb);
            if (pc.junction_station_m > 0.0) {
                std::printf("     (arm C resamples at %.1f m within "
                            "bank_gap_m = %.1f m of a junction)\n",
                            pc.junction_station_m, snow_field.p.bank_gap_m);
                arm("C + short stations", pc);
            }
            // ★ ROAD-REPAIR ONAPING RUNG 2, arm D: arm C plus the shipped
            // APRON. Two numbers come out of it that nothing else can give:
            // the apron's own vertex bill against the bank mesh's 8.02 M
            // (debt #10 -- the +72 % this lane already owes and Chad has not
            // timed), and the proof that the apron moved NO bank vertex --
            // bank_strip_continuity skips apron strips, so every continuity
            // figure on arm D must be arm C's, to the digit.
            if (apron_m_live > 0.0) {
                render::BankBuildParams pd = pc;
                pd.apron_m = apron_m_live;
                pd.apron_tol_m = world.bank_mesh.apron_tol_m;
                pd.apron_min_drop_m = world.bank_mesh.apron_min_drop_m;
                pd.skirt_m = world.bank_mesh.skirt_m;
                pd.skirt_rings = world.bank_mesh.skirt_rings;
                std::printf("     (arm D adds the apron: apron_m %.1f, tol "
                            "%.2f m, min_drop %.2f m, %d apron rings)\n",
                            pd.apron_m, pd.apron_tol_m, pd.apron_min_drop_m,
                            render::bank_apron_ring_count(pd));
                arm("D + apron", pd);
                long apron_verts = 0, apron_strips = 0, bank_verts = 0;
                long cverts = 0, dverts = 0;
                {
                    const std::vector<render::BankStripCPU> sc =
                        render::build_bank_strips(hf, subdiv, tiles,
                                                  snow_field, pc, cuts,
                                                  &cverts);
                    const std::vector<render::BankStripCPU> sd =
                        render::build_bank_strips(hf, subdiv, tiles,
                                                  snow_field, pd, cuts,
                                                  &dverts);
                    (void)sc;
                    for (const render::BankStripCPU& s : sd) {
                        const long v = static_cast<long>(s.pos.size() / 3);
                        if (s.is_apron) {
                            apron_verts += v;
                            ++apron_strips;
                        } else {
                            bank_verts += v;
                        }
                    }
                }
                std::printf(
                    "  THE APRON BILL: bank verts %ld (arm C %ld -- must be "
                    "equal), apron strips %ld verts %ld = %+.2f %% of the "
                    "bank mesh\n",
                    bank_verts, cverts, apron_strips, apron_verts,
                    cverts > 0 ? 100.0 * static_cast<double>(apron_verts) /
                                     static_cast<double>(cverts)
                               : 0.0);
            }

            // The DRIVEN step at the same stations, for scale: |d_drive_r_m|
            // at the crest rows of plowed runs. A drawn step of the same order
            // is the surface doing what the ground does; anything above it is
            // the mesh's own jag.
            std::vector<double> dd;
            for (const render::RoadCensusRow& r : rows)
                if (r.is_crest && r.kind < 2 && r.d_drive_r_m != 0.0)
                    dd.push_back(std::fabs(r.d_drive_r_m));
            std::printf(
                "  %-22s samples %6zu | driven step p50 %.3f p90 %.3f p99 "
                "%.3f max %.3f\n",
                "DRIVEN (crest)", dd.size(), render::census_pct(dd, 0.50),
                render::census_pct(dd, 0.90), render::census_pct(dd, 0.99),
                dd.empty() ? 0.0
                           : *std::max_element(dd.begin(), dd.end()));
        }
        std::printf("\n");
        return 0;
    }

    // ============================ SEADS_RIBBON_SAG ===========================
    // ★ ROAD-REPAIR / ONAPING SINK -- THE RULER FOR THE CHORD.
    //
    // Chad, 2026-09-10 fly: "a few spots near Onaping (Valley) pump I went into
    // the road on snowmachine ... on a hill to the north of the pump in the
    // middle of the road going up slightly inclined pavement."  The road census
    // on the same tip reports SINK 0 on plowed roads within 2.5 km of Valley.
    // Both are true, because they measure different surfaces:
    //
    //   the census walks a FUNCTION -- drawn_radius_at(dir)+lift AT a station,
    //     which is exact at every station by construction.
    //   the eye sees a MESH -- render/ribbons.cpp build_path_mesh evaluates
    //     that same function at the BAKED rungs and then fills FLAT TRIANGLES
    //     between them.  Between two rungs the drawn deck is a straight CHORD.
    //
    // Over a concave grade break the chord bridges ABOVE the ground; the
    // machine's drive_r follows the terrain (floored to the ribbon function at
    // its OWN point, which is down in the dip), so the body ends up under the
    // drawn deck.  That is "went into the road", and a station-sampling ruler
    // cannot see it.  This block is the ruler that can.
    //
    //   SEADS_RIBBON_SAG=<path.tsv>   one row per DRAWN ribbon segment
    //   SEADS_RIBBON_SAG_SUB=<m>      subsample spacing along the chord (1.0)
    //
    // It walks every path the drape draws (kind != 3), takes the SAME T24 cut
    // clip the mesh takes, asks render::ribbon_drawn_segments which rungs are
    // actually joined by a kept triangle, and subsamples each chord.  Three
    // chords per segment: the LEFT and RIGHT edges (exact mesh edges) and the
    // CENTRELINE (the line the machine rides -- exact at both ends and within
    // the quad's diagonal twist in between).  Exits when done.
    if (const char* sag_path = std::getenv("SEADS_RIBBON_SAG")) {
        if (snow_field.hf == nullptr) {
            std::fprintf(stderr, "RIBBON_SAG: no height field\n");
            return 3;
        }
        const world::HeightField& hf = *snow_field.hf;
        const double R = hf.R;
        const int subdiv = world.planet.subdiv, tiles = world.planet.tiles;
        // F8 again: ONE lift dial, the [ribbons] one the drape itself uses.
        const double lift = world.ribbons.lift_m;
        double sub_m = 1.0;
        if (const char* e = std::getenv("SEADS_RIBBON_SAG_SUB"))
            sub_m = std::max(0.05, std::atof(e));
        const int kMaxSub = 128;  // a cost bound, never a resolution choice:
                                  // at the measured p99 spacing it is unreached

        std::FILE* fp = std::fopen(sag_path, "wb");  // "wb": LF, the census rule
        if (fp == nullptr) {
            std::fprintf(stderr, "RIBBON_SAG: cannot open %s\n", sag_path);
            return 3;
        }
        std::fprintf(fp,
                     "path_id\tkind\tseg\tseg_len_m\tn_splits\tsub_len_m\tn_sub\t"
                     "sag_max_pos\tsag_max_neg\t"
                     "sagdrv_max_pos\tsagdrv_max_neg\t"
                     "edge_sag_max_pos\tcross_sag_max_pos\t"
                     "lat_sag_max_pos\tlat_sagdrv_max_pos\t"
                     "worst_dir_x\tworst_dir_y\tworst_dir_z\t"
                     "valley_km\tsudbury_km\tr0_m\tr1_m\n");

        std::vector<double> spacing_all, spacing_valley;
        long n_seg = 0, n_gt10_all = 0, n_gt10_valley = 0, n_valley = 0;
        long n_gt10_all_drv = 0, n_gt10_valley_drv = 0;
        long n_x10_all = 0, n_x10_valley = 0;
        const double t_sag0 = GetTime();
        for (std::size_t pi = 0; pi < render::kSudburyRibbonPathCount; ++pi) {
            const render::GisRibbonPath& P = render::kSudburyRibbonPaths[pi];
            if (P.vtx_count < 3 || P.idx_count < 3) continue;
            if (P.kind == 3) continue;  // rivers are MIRROR water, not a drape
            const std::vector<unsigned short> kept =
                render::ribbon_indices_outside_cuts(pi, planet_cuts, R);
            if (kept.empty()) continue;
            const std::vector<int> segs = render::ribbon_drawn_segments(kept);
            auto vdir = [&P](int i) {
                const render::GisRibbonVertex& V =
                    render::kSudburyRibbonVerts[P.vtx_off + i];
                return glm::dvec3(V.dir[0], V.dir[1], V.dir[2]);
            };
            // A DRAWN rung EDGE direction at the parameter t along this baked
            // segment -- the EXACT expression render::build_ribbon_batches
            // places an inserted vertex at, so this ruler reads the mesh that
            // actually ships and not a model of it.
            auto edge_dir = [&](const glm::dvec3& da, const glm::dvec3& db,
                                double t) {
                return t <= 0.0 ? da
                                : (t >= 1.0 ? db
                                            : glm::normalize(da + (db - da) * t));
            };
            const std::function<double(const glm::dvec3&)> rfn =
                [&hf, subdiv, tiles](const glm::dvec3& d) {
                    return render::drawn_radius_at(hf, d, subdiv, tiles);
                };
            for (int m : segs) {
                if (2 * m + 3 >= P.vtx_count) continue;
                const glm::dvec3 dL0 = vdir(2 * m), dR0 = vdir(2 * m + 1);
                const glm::dvec3 dL1 = vdir(2 * m + 2), dR1 = vdir(2 * m + 3);
                const glm::dvec3 dc0 = glm::normalize(dL0 + dR0);
                const glm::dvec3 dc1 = glm::normalize(dL1 + dR1);
                const double seg_len =
                    R * std::acos(std::clamp(glm::dot(dc0, dc1), -1.0, 1.0));
                if (!(seg_len > 1.0e-6)) continue;  // a duplicated rung
                // THE SAME split count the mesh builder uses, from the SAME
                // pure function and the SAME live dial.
                const int nsp = render::ribbon_seg_splits(
                    dc0, dc1, R, ribbon_max_seg_live);
                // The COLUMN count the mesh gives this quad. ⚠ The mesh drops
                // to a single span on a quad the T24 clip halved (a grid cell
                // straddling the lost diagonal would resurrect it); this ruler
                // assumes the WHOLE-quad count everywhere, which is exact off
                // the excavation cuts and optimistic on the handful of quads
                // inside them.
                const int ntq = std::max(
                    render::ribbon_tr_splits(dL0, dR0, R, ribbon_max_tr_live),
                    render::ribbon_tr_splits(dL1, dR1, R, ribbon_max_tr_live));
                const glm::dvec3 dmid = glm::normalize(dc0 + dc1);
                const double valley_m =
                    R * std::acos(std::clamp(
                            glm::dot(dmid, world::kPumpValleySurface), -1.0,
                            1.0));
                const double sudbury_m =
                    R * std::acos(std::clamp(
                            glm::dot(dmid, world::kPumpSudburySurface), -1.0,
                            1.0));
                const bool near_valley = valley_m <= 2500.0;
                const bool plowed = (P.kind == 0 || P.kind == 1);
                if (plowed) {
                    spacing_all.push_back(seg_len);
                    if (near_valley) spacing_valley.push_back(seg_len);
                }
                const double sub_len = seg_len / nsp;
                int n = static_cast<int>(std::ceil(sub_len / sub_m));
                if (n < 2) n = 2;
                if (n > kMaxSub) n = kMaxSub;
                double cmaxp = 0.0, cmaxn = 0.0, dmaxp = 0.0, dmaxn = 0.0;
                double emaxp = 0.0, worst = -1.0e30, xmaxp = 0.0;
                double lmaxp = 0.0, lmaxd = 0.0;
                glm::dvec3 worst_dir = dmid;
                // FIVE lateral stations across the deck, and every one of them
                // is a point on the SUBDIVIDED rung polyline the mesh actually
                // has (render::ribbon_rung_point -- the same function, the same
                // dial): the two EDGES (exact mesh edges), the CENTRELINE the
                // machine rides (a REAL vertex row whenever the transverse dial
                // splits this rung, since the column count is forced even), and
                // +-half_w/2 so the residual OFF centre is visible too.
                const double us[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
                // One row per BAKED segment either way -- the population is
                // identical before and after, so the two tables are the same
                // rows read twice. What changes is that the row now maxes over
                // the nsp x ntq CELLS the mesh is actually made of.
                for (int q = 0; q < nsp; ++q) {
                    const double ta = static_cast<double>(q) / nsp;
                    const double tb = static_cast<double>(q + 1) / nsp;
                    const glm::dvec3 dLa = edge_dir(dL0, dL1, ta);
                    const glm::dvec3 dRa = edge_dir(dR0, dR1, ta);
                    const glm::dvec3 dLb = edge_dir(dL0, dL1, tb);
                    const glm::dvec3 dRb = edge_dir(dR0, dR1, tb);
                    for (int li = 0; li < 5; ++li) {
                        const double u = us[li];
                        const bool is_centre = (li == 2);
                        const bool is_edge = (li == 0 || li == 4);
                        const glm::dvec3 Pa = render::ribbon_rung_point(
                            dLa, dRa, ntq, u, rfn, lift);
                        const glm::dvec3 Pb = render::ribbon_rung_point(
                            dLb, dRb, ntq, u, rfn, lift);
                        // \xe2\x98\x85 THE TRANSVERSE TERM ALONE, measured AT the rungs,
                        // where the longitudinal chord error is zero by
                        // construction -- so the two terms cannot contaminate
                        // each other. With the transverse dial on and the
                        // centreline a real vertex row this collapses to ~0,
                        // which is the whole point of the second cut.
                        if (is_centre)
                            for (int z = 0; z < (q + 1 == nsp ? 2 : 1); ++z) {
                                const glm::dvec3 Pz = z == 0 ? Pa : Pb;
                                const double zr = glm::length(Pz);
                                xmaxp = std::max(xmaxp,
                                                 zr - (rfn(Pz / zr) + lift));
                            }
                        for (int k = 1; k < n; ++k) {
                            const double t = static_cast<double>(k) / n;
                            const glm::dvec3 Pm = Pa + t * (Pb - Pa);
                            const double chord_r = glm::length(Pm);
                            const glm::dvec3 sd = Pm / chord_r;
                            const double sag = chord_r - (rfn(sd) + lift);
                            if (is_edge) {
                                emaxp = std::max(emaxp, sag);
                                continue;  // the edges carry no drive column
                            }
                            const double drv = snow_field.sample_at(sd).drive_r;
                            const double sagd = chord_r - drv;
                            if (is_centre) {
                                cmaxp = std::max(cmaxp, sag);
                                cmaxn = std::min(cmaxn, sag);
                                dmaxp = std::max(dmaxp, sagd);
                                dmaxn = std::min(dmaxn, sagd);
                                if (sagd > worst) {
                                    worst = sagd;
                                    worst_dir = sd;
                                }
                            } else {
                                lmaxp = std::max(lmaxp, sag);
                                lmaxd = std::max(lmaxd, sagd);
                            }
                        }
                    }
                }
                ++n_seg;
                if (near_valley) ++n_valley;
                if (plowed) {
                    if (cmaxp > 0.10) {
                        ++n_gt10_all;
                        if (near_valley) ++n_gt10_valley;
                    }
                    if (dmaxp > 0.10) {
                        ++n_gt10_all_drv;
                        if (near_valley) ++n_gt10_valley_drv;
                    }
                    if (xmaxp > 0.10) {
                        ++n_x10_all;
                        if (near_valley) ++n_x10_valley;
                    }
                }
                std::fprintf(
                    fp,
                    "%d\t%d\t%d\t%.2f\t%d\t%.2f\t%d\t%+.4f\t%+.4f\t%+.4f\t"
                    "%+.4f\t%+.4f\t%+.4f\t%+.4f\t%+.4f\t"
                    "%.9f\t%.9f\t%.9f\t%.4f\t%.4f\t%.3f\t%.3f\n",
                    static_cast<int>(pi), P.kind, m, seg_len, nsp, sub_len, n,
                    cmaxp, cmaxn, dmaxp, dmaxn, emaxp, xmaxp, lmaxp, lmaxd,
                    worst_dir.x, worst_dir.y,
                    worst_dir.z, valley_m / 1000.0, sudbury_m / 1000.0,
                    glm::length(render::ribbon_rung_point(dL0, dR0, ntq, 0.5,
                                                          rfn, lift)),
                    glm::length(render::ribbon_rung_point(dL1, dR1, ntq, 0.5,
                                                          rfn, lift)));
            }
        }
        std::fclose(fp);
        auto pct = [](std::vector<double>& v, double q) {
            if (v.empty()) return 0.0;
            std::sort(v.begin(), v.end());
            const std::size_t k = static_cast<std::size_t>(
                q * static_cast<double>(v.size() - 1) + 0.5);
            return v[std::min(k, v.size() - 1)];
        };
        std::printf(
            "\n=== RIBBON CHORD SAG === %ld drawn segments (%ld within 2500 m "
            "of PUMP valley), [ribbons] max_seg_m %.2f / max_tr_m %.2f LIVE, "
            "sub %.2f m, %.0f ms\n",
            n_seg, n_valley, ribbon_max_seg_live, ribbon_max_tr_live, sub_m,
            (GetTime() - t_sag0) * 1000.0);
        std::printf(
            "  PLOWED baked rung spacing [m]  planet-wide n %zu  p50 %.2f  "
            "p90 %.2f  p99 %.2f  max %.2f\n",
            spacing_all.size(), pct(spacing_all, 0.50), pct(spacing_all, 0.90),
            pct(spacing_all, 0.99), pct(spacing_all, 1.00));
        std::printf(
            "  PLOWED baked rung spacing [m]  <=2.5 km Valley n %zu  p50 %.2f  "
            "p90 %.2f  p99 %.2f  max %.2f\n",
            spacing_valley.size(), pct(spacing_valley, 0.50),
            pct(spacing_valley, 0.90), pct(spacing_valley, 0.99),
            pct(spacing_valley, 1.00));
        std::printf(
            "  PLOWED segments with max positive sag > 0.10 m:  vs the ribbon "
            "FUNCTION %ld planet-wide / %ld near Valley | vs DRIVE_R %ld "
            "planet-wide / %ld near Valley\n",
            n_gt10_all, n_gt10_valley, n_gt10_all_drv, n_gt10_valley_drv);
        std::printf(
            "  PLOWED TRANSVERSE floor (the rung's own span across the road, "
            "which no longitudinal split can touch): max positive > 0.10 m on "
            "%ld planet-wide / %ld near Valley\n",
            n_x10_all, n_x10_valley);
        std::printf("  TSV -> %s\n\n", sag_path);
        return 0;
    }

    if (const char* dump_path = std::getenv("SEADS_SNOWDUMP")) {
        const world::LineNetwork* net = snow_field.lines;
        if (net == nullptr || net->empty() || snow_field.hf == nullptr) {
            std::fprintf(stderr, "SNOWDUMP: no linework / no height field\n");
            return 3;
        }
        const double R = snow_field.hf->R;
        const std::size_t n_st = net->st.size();
        // ~2000 transects however dense the bake is, so the run cost is a
        // property of this instrument and not of the ribbon densification.
        const std::size_t stride = std::max<std::size_t>(1, n_st / 2000);
        const double kBandInnerM = 9.0;   // plow-bank envelope: edge + 9.0 m
        const double kBandOuterM = 85.0;  // his tapes' max off-corridor range
        const double kStepM = 2.0;

        std::FILE* fp = std::fopen(dump_path, "w");
        if (fp == nullptr) {
            std::fprintf(stderr, "SNOWDUMP: cannot open %s\n", dump_path);
            return 3;
        }
        std::fprintf(fp,
                     "station\tkind\thalf_w_m\toffset_m\tterrain_m\tdepth_m\t"
                     "drive_m\tsurface\n");

        // Per-transect peak-to-peak, collected only over the BAND (the metres
        // inside the bank envelope are the bank, not terrain, and folding them
        // in would report the plow windrow as relief).
        std::vector<double> pp_depth, pp_drive, max_grade;
        std::size_t n_transect = 0, n_samp = 0;
        for (std::size_t i = 0; i < n_st; i += stride) {
            const world::LineStation& S = net->st[i];
            // The corridor tangent, from the neighbouring station IN THE SAME
            // RUN -- a run boundary is a different road, and differencing
            // across it yields a perpendicular belonging to neither.
            std::size_t j = i + 1;
            if (j >= n_st || net->st[j].run != S.run) {
                if (i == 0) continue;
                j = i - 1;
                if (net->st[j].run != S.run) continue;
            }
            glm::dvec3 tang = net->st[j].dir - S.dir;
            tang -= S.dir * glm::dot(tang, S.dir);
            if (glm::length(tang) < 1e-12) continue;
            tang = glm::normalize(tang);
            const glm::dvec3 perp = glm::normalize(glm::cross(S.dir, tang));

            double d_lo = 1e30, d_hi = -1e30, v_lo = 1e30, v_hi = -1e30;
            double prev_drive = 0.0, grade = 0.0;
            bool have_prev = false;
            bool any_band = false;
            for (double off = -kBandOuterM; off <= kBandOuterM + 1e-9;
                 off += kStepM) {
                const double ang = off / R;
                const glm::dvec3 d = glm::normalize(S.dir * std::cos(ang) +
                                                    perp * std::sin(ang));
                const double terr = snow_field.hf->radius_at(d) - R;
                const double dep = snow_field.depth_at(d);
                const double drv = snow_field.drive_radius_at(d) - R;
                std::fprintf(fp, "%zu\t%d\t%.2f\t%.1f\t%.3f\t%.4f\t%.3f\t%s\n",
                             i, static_cast<int>(S.kind),
                             static_cast<double>(S.half_w_m), off, terr, dep,
                             drv,
                             world::surface_name(snow_field.surface_at(d)));
                ++n_samp;
                if (std::fabs(off) >= kBandInnerM) {
                    any_band = true;
                    d_lo = std::min(d_lo, dep);
                    d_hi = std::max(d_hi, dep);
                    v_lo = std::min(v_lo, drv);
                    v_hi = std::max(v_hi, drv);
                    if (have_prev)
                        grade = std::max(grade,
                                         std::fabs(drv - prev_drive) / kStepM);
                    prev_drive = drv;
                    have_prev = true;
                } else {
                    have_prev = false;  // do not step ACROSS the corridor
                }
            }
            if (!any_band) continue;
            pp_depth.push_back(d_hi - d_lo);
            pp_drive.push_back(v_hi - v_lo);
            max_grade.push_back(grade);
            ++n_transect;
        }
        std::fclose(fp);

        auto pct = [](std::vector<double>& v, double q) {
            if (v.empty()) return 0.0;
            std::sort(v.begin(), v.end());
            std::size_t k = static_cast<std::size_t>(q * (v.size() - 1) + 0.5);
            return v[k];
        };
        std::printf(
            "\n=== SF3-0 TERRAIN CENSUS -- real DEM, real linework ===\n"
            "  stations %zu, stride %zu -> %zu transects, %zu samples\n"
            "  band [%.0f, %.0f] m off centreline, %.0f m step\n",
            n_st, stride, n_transect, n_samp, kBandInnerM, kBandOuterM, kStepM);
        std::printf(
            "  DEPTH peak-to-peak in band [m]  p10 %.3f  p50 %.3f  "
            "p90 %.3f  max %.3f\n",
            pct(pp_depth, 0.10), pct(pp_depth, 0.50), pct(pp_depth, 0.90),
            pct(pp_depth, 1.00));
        std::printf(
            "  DRIVEN surface p-to-p in band [m]  p10 %.3f  p50 %.3f  "
            "p90 %.3f  max %.3f\n",
            pct(pp_drive, 0.10), pct(pp_drive, 0.50), pct(pp_drive, 0.90),
            pct(pp_drive, 1.00));
        std::printf(
            "  max |d(driven)/d(offset)| [m/m]  p50 %.4f  p90 %.4f  "
            "max %.4f\n",
            pct(max_grade, 0.50), pct(max_grade, 0.90), pct(max_grade, 1.00));
        std::printf("  wrote %s\n\n", dump_path);
        return 0;
    }

    pump_loading("tunnel net");
    world::TunnelNet tunnel_net = world::build_tunnel_net(tparams, env.ground);
    if (game.tunnel.enabled) {
        env.tunnels = &tunnel_net;
    }
    // T5c DESTRUCTIBLE GASLAMPS: the app-owned lamp world (positions + per-lamp
    // alive state), built from the pure T1 placement so the hit pass has it
    // from frame 1 (the renderer builds its glow buffers from THIS same world;
    // a shot flips `dirty` and the renderer rebuilds from the survivors). Only
    // under the [tunnel] gate; deaths persist across respawn within a run
    // (world state).
    render::TunnelLampWorld tunnel_lamps;
    if (game.tunnel.enabled)
        tunnel_lamps.init(render::place_tunnel_lamps(tunnel_net));

    // R6 AtmosphereField (MASTER_PLAN §3.C): the single TEST bubble is centered
    // on the SPAWN direction so Chad starts inside and flies out to the soft
    // edge. Params load unconditionally (dials config-owned; the B key
    // activates live below); only the POINTER follows enabled. enabled => the
    // bubble always exists (Fable-BEFORE §3: never a live EMPTY field/vacuum).
    sim::AtmosphereField atm_field;
    atm_field.deck_agl_m = game.atmosphere.deck_agl_m;
    atm_field.deck_soft_m = game.atmosphere.deck_soft_m;
    // RUNG E16 — which surface the deck is measured from (false = R6's bare
    // sphere, bit-for-bit; the rationale lives on the field's own member).
    atm_field.deck_terrain_relative = game.atmosphere.deck_terrain_relative;
    // S-domeround (docs/airdome_round_spec.md §1.1): the ONE roundness dial,
    // shared by every bubble in this field (Chad's fly-dial).
    atm_field.dome_exponent = game.atmosphere.bubble_dome_exponent;
    // CONQUEST (spec §1/§2): the ConquestWorld — the two faction ovals REPLACE
    // the single R6 test bubble, the pumps + lives + squadron bookkeeping, and
    // the live-rebuild handle. Built here so the atm_field carries the 6
    // faction bubbles from frame 1 when [conquest] enabled; conquest also
    // force-enables the atmosphere field (below). Off-arm (disabled) => the
    // EXACT old single-bubble path (asserted in the test: 1 bubble vs 6).
    const bool conquest_on = game.conquest.enabled;
    app::ConquestWorld cq;
    // FLAK F-PLACE (docs/FLAK_GUN_SPEC.md §1, Chad ruled 2026-08-27): one
    // gun per SURFACE pump, kFlankOffsetM toward the ENEMY surface pump.
    // Filled inside the conquest init (needs env.ground); empty otherwise.
    std::vector<render::FlakDraw> flak_guns;
    // FLAK F-FIRE (spec §7.4): the manned gun's world -- pose, drum, its OWN
    // projectile pool. Threaded into app::tick; absent guns => it just idles.
    app::FlakWorld flak_fk;
    // ★★★ L5 -- THE STATION TABLE FOR THE APPROACH MARK, FETCHED ONCE.
    // `render::flak_stations` recomposes the GLB's rest hierarchy on every
    // call and lazy-loads the model on the first one, so the interact site --
    // which is rebuilt every frame by law (app/interact.h) -- caches it. The
    // retry is deliberate: the GLB is not loaded until the first draw, so the
    // first few frames legitimately have no table and no site.
    render::flak::Stations flak_site_st{};
    bool flak_site_st_ok = false;
    // ── FLAK STAGE D (immersion ladder 4/4): every gun the PLAYER is not
    // standing at is manned by an AI gunner who fights. One FlakAiGun per
    // placed Oerlikon; the double-drive guard is the `manned` flag set each
    // frame below. flak_ai_shots_seen is the DISTANT-REPORT audio reader --
    // its own difference of the monotone spawned_total (never the accumulator:
    // the Stage C brass-storm lesson).
    std::vector<app::FlakAiGun> flak_ai;
    std::vector<long long> flak_ai_shots_seen;
    double flak_recoil = 0.0;  // [m] cosmetic aft kick of the gun body
    // FLAK STAGE B COSMETICS (render/flak_gun.h "STAGE B"): three envelopes
    // stepped from the SAME undrained spawned_accum the recoil reads. All
    // cosmetic -- nothing below is read by the fire control, the aim, the
    // pose or the pipper.
    double flak_flash = 0.0;   // muzzle-flash envelope [0, kFlashCeil]
    double flak_shake = 0.0;   // sight-shake envelope  [0, kShakeCeil]
    double flak_blast = 0.0;   // snow-blast intensity  [0, kBlastCeil]
    long long flak_shot_seq = 0;  // shots fired: the deterministic FX phase
    // ── FLAK STAGE C (immersion ladder 3/4). Both COSMETIC; app/flak_tick.h's
    // 4 s reload FACT is untouched. The brass ring is app-owned (render/ only
    // draws it); the clank edge detector is one double.
    render::flak::BrassPool flak_brass;      // 150-case ring, spent 20 mm
    long long flak_brass_seen = 0;           // last FlakWorld::spawned_total
                                             //   the brass spawner consumed
    double flak_prev_reload_left_s = 0.0;    // for the reload_left_s EDGES
    // Hit/kill confirmation for the manned gun (Chad 2026-08-29: "add a flak
    // hit indication somewhere"): decaying envelopes bumped on cw.hits /
    // cw.kills deltas WHILE MANNED (both counters are player-only -- AI-gun
    // sweeps book into the sink, and hits are now sink-gated too).
    double flak_hitmark = 0.0;   // white X at the pipper, tau ~0.25 s
    double flak_killmark = 0.0;  // red X, larger, tau ~0.6 s
    long long flak_prev_hits = 0, flak_prev_kills = 0;
    // ROUND 5 (the burst boom): last-seen flak_fk.burst_total, differenced in
    // the sfx audio block -- one distant crump per curtain self-destruct.
    long long flak_prev_burst_total = 0;
    // FLAK FREE LOOK (Chad 2026-08-28: "there should be free look for flak").
    // SPACE + mouse turns the gunner's HEAD off the sight axis (the gun holds
    // its pose -- the deltas are lent to the camera, the sled DRIVE-2
    // precedent); release eases back onto the sight. az + = look right,
    // el + = look up, both [rad].
    double flak_cam_az = 0.0, flak_cam_el = 0.0;
    // F-POSE PULLOUT (Chad's 2026-08-31 ruling): free-look does not just
    // turn his head, it swings the camera OUT behind his shoulder so the
    // Sudburian on the gun is actually SEEN -- the sight picture itself
    // stays clean. 0 = down the sight, 1 = fully outside; eased both ways
    // at flak::kExtEaseHz so there is no cut.
    double flak_ext_t = 0.0;
    // ★★★ ST-5 PHASE D -- THE SEAT DEPLOY (Chad 2026-09-04: "deployment from
    // the seat to the Sudburian's hands to take aim"). The blend the launcher
    // pose runs on, 0 = stowed beside the tunnel, 1 = stock on his shoulder,
    // three seconds between. Stepped on clamped_dt beside flak_ext_t below;
    // the laws (the cut, the launch snap, the rates) are pure and executed by
    // ctest in render/sting_deploy.h.
    double sting_deploy_t = 0.0;
    // THE CUT FLAG. A fall, a mount, a respawn or the X give-up takes the
    // stance away, and on the next frame he is not holding anything -- so the
    // pose must GO, not ease down through frames in which the hands are
    // already elsewhere (the game-loop packet §3.5: "your pose must be able
    // to cut, not only blend"). Set at the two sites that lose the stance,
    // consumed once by the step.
    bool sting_deploy_cut = false;
    // Rising-edge memory for the launch snap: the drone going active is the
    // moment the launcher reaches full extension, whether or not the blend
    // had got there yet.
    bool sting_active_prev = false;
    // Is he shouldering it FROM THE SEAT. Set from the SAME `seated` branch
    // of the `sting_stance` lambda that decides the launch origin, so the
    // drawn launcher and the missile's birthplace cannot disagree.
    bool sting_seated_now = false;
    // ★★★ ST-5 POLISH (b)+(c) -- THE ONE THROTTLE SCALAR AND THE ONE PHASE.
    //
    // `sting_thr01` is how hard the machine is working, [0,1], and BOTH the
    // blade blur and the motor buzz are functions of it. Two separate throttle
    // reads -- one for the picture, one for the sound -- would be a fork that
    // nobody notices until the props are screaming while sitting still.
    //
    // ⚠ WHICH THROTTLE. The cascade's own internal throttle is computed
    // inside app::sting_tick (ci.throttle = base + p*(v_cmd - v), app/sting.h)
    // and is NOT stored on StingState, so it is not reachable here without
    // widening that struct -- another lane's file. And it is the wrong signal
    // anyway: that law drives to ZERO whenever the drone is above its setpoint
    // (a dive, the end of every attack run), which would cut the buzz out at
    // exactly the moment the thing is loudest. So this is the SPEED FRACTION
    // over the band's top (fly-2's 450 m/s), which is monotone in how fast the
    // machine is actually moving and never gaps.
    double sting_thr01 = 0.0;
    // The props' accumulated spin angle [rad], wrapped to [0, 2pi) every frame
    // so a 60 s battery cannot grind float precision away by the time it is
    // narrowed to a float for FrameInfo. Stepped below on clamped_dt.
    double sting_prop_phase = 0.0;
    // The TRUE rate [rad/s] the same frame, kept beside the phase because the
    // blur disc ramps on it and the phase no longer carries it (the phase is
    // accumulated at the soft-knee APPARENT rate -- render/draw.h's two-field
    // banner has the why).
    double sting_prop_rate = 0.0;
    // ★★★ POLISH FLY-2 (Chad, verbatim): "free look press should take us out
    // of fpv to 3rd person from about 5 feet away". The shouldered launcher's
    // own orbit -- the sled_cam_az/el idiom, third instance (sled, drone,
    // launcher), same signs, same 6/s ease-back, same wheel step. The AIM
    // HOLDS while it is engaged: freelook drives the orbit, never mouse->aim.
    double sting_look_az = 0.0, sting_look_el = 0.0;  // [rad]
    // "about 5 feet" = 1.5 m. Wheel-zoomable and PERSISTENT across presses,
    // like sled_cam_zoom -- he asked for the zoom precisely because one fixed
    // distance was not enough.
    double sting_look_dist = 1.5;
    if (conquest_on) {
        cq.params.growth_radius_frac = game.conquest.growth_radius_frac;
        cq.params.growth_ceiling_frac = game.conquest.growth_ceiling_frac;
        // 2026-07-25 fly-2 ruling C: victim-shrink fracs (single-sourced from
        // [conquest], the loader's [0,1]-checked pair).
        cq.params.match_countdown_s = game.conquest.match_countdown_s;
        cq.params.shrink_radius_frac = game.conquest.shrink_radius_frac;
        cq.params.shrink_ceiling_frac = game.conquest.shrink_ceiling_frac;
        // COMPETITIVE rung: enemy pump-raid DPS fraction ([conquest], [0,1]).
        cq.params.raid_dps_frac = game.conquest.raid_dps_frac;
        // FIX-F3 (docs/ai_phase2_fix_spec.md): the raid-pause latch's own
        // dial pair (single-sourced from [conquest], loader-checked
        // disengage > engage > 0) -- instructor_tick.h reads cq->params for
        // this, never dw->dparams.engage_range/disengage_range.
        cq.params.raid_pause_engage_m = game.conquest.raid_pause_engage_m;
        cq.params.raid_pause_disengage_m = game.conquest.raid_pause_disengage_m;
        // ALL-VS-PLAYER FURBALL (Chad's ask, 2026-07-26): single-sourced from
        // [conquest] all_vs_player; app::tick (instructor_tick.h) reads this
        // to override the 5v5 split / max_engaged / range gate.
        cq.params.all_vs_player = game.conquest.all_vs_player;
        // RUNG E5 REINFORCEMENT WAVES (Chad 2026-08-20 — the ruling that
        // consciously supersedes respawn_drones=false; see [conquest]
        // reinforce_delay_s and combat/reinforce.h). 0 = OFF = permanent
        // death, bit-identical. cq.reinforce_policy stays NULL here: null
        // means the shipped airborne policy. The millwright tier substitutes
        // its land-at-a-marker / class-choice policy by assigning that ONE
        // pointer right here — nothing else on the path changes.
        cq.params.reinforce_delay_s = game.conquest.reinforce_delay_s;
        cq.params.reinforce_restores_roster =
            game.conquest.reinforce_restores_roster;
        // RUNG E6 (Chad 2026-08-20, tape 2). Every one of these ships an
        // off-value that reproduces the pre-E6 tick bit-for-bit; see
        // combat/conquest.h for the per-dial rationale.
        cq.params.reinforce_pool_n = game.conquest.reinforce_pool_n;
        cq.params.leash_min_radius_scale = game.conquest.leash_min_radius_scale;
        cq.params.raid_no_pause = game.conquest.raid_no_pause;
        // RUNG E12.1 -- raid duty follows the LIVE roster.
        cq.params.raid_backfill = game.conquest.raid_backfill;
        cq.state.player_faction = (game.conquest.player_faction == "valley")
                                      ? combat::CQ_VALLEY
                                      : combat::CQ_SUDBURY;
        // Surface pump anchors: the baked unit dir * (local terrain radius +
        // ~10 m) via the app's height sampling (env.ground; bare sphere R if
        // ground is off).
        const auto surf = [&](const glm::dvec3& dir) {
            const glm::dvec3 u = glm::normalize(dir);
            const double r = (env.ground != nullptr)
                                 ? env.ground->radius_at(u) +
                                       app::kSurfacePumpMastM
                                 : params.R + app::kSurfacePumpMastM;
            return u * r;
        };
        // Deep pump positions (2026-07-25 fly-2 ruling B, Chad's F6 report —
        // "no pumps in the black stope"): computed from the LIVE
        // world::TunnelNet arena (built above, single-sourced — no
        // config-duplicated arena_a_m/arena_c_m/kPlanetR_m constants), one
        // per faction, toward its own faction's tunnel mouth.
        // FLY-3 FIX ("colored lights, not destructible"): 30 m above the
        // ELLIPSOID surface at 80% depth sits BELOW the arena's SEALED floor
        // slab (tunnel_net T12: "the floor is SEALED... filled over the
        // core") — the pump hit-sphere was buried in that fill, so every
        // round died on the floor before reaching it while the emissive
        // marker still glowed through. Lift the pumps to CHAMBER-CENTER
        // HEIGHT (v = 0): height_above_floor = a_pos*sqrt(1-frac^2) cancels
        // the ellipsoid floor drop exactly, and v=0 at h=0.6b is open flown
        // air (the mavericks' own transit volume) at every legal arena tune.
        const double lift_to_center =
            tunnel_net.arena.a_pos *
            std::sqrt(1.0 - combat::kDeepPumpFrac * combat::kDeepPumpFrac);
        const glm::dvec3 valley_deep = combat::place_deep_pump(
            tunnel_net.arena.center, tunnel_net.arena.u_long,
            world::kTunnelMouthErrington, tunnel_net.arena.a_pos,
            tunnel_net.arena.b, combat::kDeepPumpFrac, lift_to_center);
        const glm::dvec3 sudbury_deep = combat::place_deep_pump(
            tunnel_net.arena.center, tunnel_net.arena.u_long,
            world::kTunnelMouthMurray, tunnel_net.arena.a_pos,
            tunnel_net.arena.b, combat::kDeepPumpFrac, lift_to_center);
        // Pump HP (ruling D): pump_kill_seconds of the player's OWN gun
        // battery's DPS (already built above, gw.battery) — config-derived,
        // never welded.
        const double pump_max_hp = combat::derive_pump_max_hp(
            gw.battery, game.conquest.pump_kill_seconds);
        combat::make_pumps(surf(world::kPumpValleySurface),
                           surf(world::kPumpSudburySurface), valley_deep,
                           sudbury_deep, pump_max_hp, cq.state.pumps);
        // FLAK F-PLACE: the two Oerlikons, each on its pump's THREAT FLANK
        // (toward the enemy surface pump -- strafers cross the gun's front),
        // grounded at the local terrain radius; the pedestal's snow pad is
        // 0.15 m deep, so base-at-surface buries the crib, top flush. A
        // degenerate pump pair places NO gun (never an arbitrary one).
        {
            const glm::dvec3 us[2] = {
                glm::normalize(world::kPumpValleySurface),
                glm::normalize(world::kPumpSudburySurface)};
            for (int gi = 0; gi < 2; ++gi) {
                glm::dvec3 u_site, fwd;
                if (!render::flak::flak_site(us[gi], us[1 - gi], params.R,
                                             render::flak::kFlankOffsetM,
                                             u_site, fwd))
                    continue;
                // ── HILLTOP SITE SEARCH (Chad 2026-08-29: "move the flak gun
                // a little further north to the top of the hill or an
                // adjacent hill as the pump itself occludes the view of enemy
                // planes"). The flank point stays the FALLBACK (no ground =>
                // exactly the old site). Four wrong cuts paid for this shape:
                // a raw height-max parked the gun against a rock face; an
                // open-scored north bonus lost to a 276 m mountain SOUTH; a
                // mean horizon penalty let ONE walled azimuth slide; and any
                // site at all stood inside the boreal scatter (the tree
                // clearing below). So: NORTHWARD FAN ONLY (his word is the
                // ruling, not a tie-break), flat pad, open horizon scored in
                // 8 azimuths, and the THREAT fan (+-30 deg toward the enemy
                // pump) is a HARD gate -- >8 deg of rock there and the site
                // is out, however high. Deterministic, config-free (v0 code
                // constants, the FlakAiParams precedent). fwd0 stays the
                // threat bearing.
                if (env.ground != nullptr) {
                    constexpr double kSiteMinM = 90.0;   // clear of the pump
                    constexpr double kSiteMaxM = 300.0;  // "a little" further
                    constexpr int kSiteBearings = 24;
                    constexpr int kSiteRadii = 8;
                    constexpr double kNorthBonusM = 6.0;
                    const glm::dvec3 up = us[gi];
                    // bubble_map's fixed reference: local north = the ref
                    // axis projected off the vertical.
                    const glm::dvec3 refn{0.0, 0.0, 1.0};
                    const glm::dvec3 north = glm::normalize(
                        refn - up * glm::dot(refn, up));
                    const glm::dvec3 east = glm::normalize(
                        glm::cross(north, up));
                    const auto ring_h = [&](const glm::dvec3& u_c, double rm,
                                            double& mx, double& avg) {
                        mx = -1e18;
                        avg = 0.0;
                        for (int k = 0; k < 8; ++k) {
                            const double a = 2.0 * PI * (k + 0.5) / 8.0;
                            const glm::dvec3 u_r = glm::normalize(
                                u_c + (std::cos(a) * north +
                                       std::sin(a) * east) *
                                          (rm / params.R));
                            const double h = env.ground->radius_at(u_r);
                            mx = std::max(mx, h);
                            avg += h / 8.0;
                        }
                    };
                    double best = -1e18;
                    glm::dvec3 best_u = u_site;
                    for (int b = 0; b < kSiteBearings; ++b) {
                        const double th =
                            2.0 * PI * (b + 0.5) / kSiteBearings;
                        const glm::dvec3 dir =
                            std::cos(th) * north + std::sin(th) * east;
                        // northward fan only (within ~72 deg of north)
                        if (glm::dot(dir, north) < 0.3) continue;
                        for (int ri = 0; ri < kSiteRadii; ++ri) {
                            const double d =
                                kSiteMinM +
                                (kSiteMaxM - kSiteMinM) *
                                    (ri + 0.5) / kSiteRadii;
                            const glm::dvec3 u_c = glm::normalize(
                                us[gi] + dir * (d / params.R));
                            const double h = env.ground->radius_at(u_c);
                            double pad_mx, pad_avg, far_mx, far_avg;
                            ring_h(u_c, 8.0, pad_mx, pad_avg);
                            ring_h(u_c, 45.0, far_mx, far_avg);
                            if (std::abs(pad_mx - h) > 2.5) continue;
                            if (std::abs(h - pad_avg) > 2.5) continue;
                            (void)far_mx;
                            // the firing line stays clear of the own pump
                            // (the original flank rule's whole point)
                            const auto tangentize =
                                [&](const glm::dvec3& v) {
                                    const glm::dvec3 t =
                                        v - u_c * glm::dot(v, u_c);
                                    return glm::dot(t, t) > 1e-18
                                               ? glm::normalize(t)
                                               : glm::dvec3{0.0};
                                };
                            const glm::dvec3 t_own =
                                tangentize(us[gi] - u_c);
                            const glm::dvec3 t_enemy =
                                tangentize(glm::normalize(us[1 - gi]) - u_c);
                            if (glm::dot(t_own, t_enemy) >
                                std::cos(25.0 * PI / 180.0))
                                continue;
                            const auto horizon_deg =
                                [&](const glm::dvec3& ad) {
                                    double worst = 0.0;
                                    for (double dd = 60.0; dd <= 1020.0;
                                         dd += 96.0) {
                                        const glm::dvec3 u_s =
                                            glm::normalize(
                                                u_c + ad * (dd / params.R));
                                        const double dh =
                                            env.ground->radius_at(u_s) - h;
                                        worst = std::max(
                                            worst, std::atan2(dh, dd) *
                                                       (180.0 / PI));
                                    }
                                    return worst;
                                };
                            double horizon_pen = 0.0;
                            for (int az = 0; az < 8; ++az) {
                                const double aa = 2.0 * PI * (az + 0.5) / 8.0;
                                horizon_pen +=
                                    std::max(0.0,
                                             horizon_deg(
                                                 std::cos(aa) * north +
                                                 std::sin(aa) * east) -
                                                 6.0) /
                                    8.0;
                            }
                            bool threat_walled = false;
                            for (int ta = -1; ta <= 1 && !threat_walled;
                                 ++ta) {
                                const double rot_a =
                                    ta * (30.0 * PI / 180.0);
                                const glm::dvec3 td = glm::normalize(
                                    t_enemy * std::cos(rot_a) +
                                    glm::cross(u_c, t_enemy) *
                                        std::sin(rot_a));
                                if (horizon_deg(td) > 8.0)
                                    threat_walled = true;
                            }
                            if (threat_walled) continue;
                            const double prom =
                                std::clamp(h - far_avg, 0.0, 10.0);
                            const double score =
                                h + 2.0 * prom - 25.0 * horizon_pen +
                                kNorthBonusM * glm::dot(dir, north);
                            if (score > best) {
                                best = score;
                                best_u = u_c;
                            }
                        }
                    }
                    u_site = best_u;
                    // re-derive the threat bearing FROM the new site
                    glm::dvec3 to_enemy =
                        glm::normalize(us[1 - gi]) - u_site;
                    to_enemy -= u_site * glm::dot(to_enemy, u_site);
                    if (glm::dot(to_enemy, to_enemy) > 1e-18)
                        fwd = glm::normalize(to_enemy);
                }
                render::FlakDraw fg;
                fg.faction = gi;  // us[0] = Valley = CQ_VALLEY(0), us[1] =
                                  // Sudbury = CQ_SUDBURY(1) -- same order as
                                  // make_pumps' surface pair above
                fg.pos = u_site * (env.ground != nullptr
                                       ? env.ground->radius_at(u_site)
                                       : params.R);
                fg.up = u_site;
                fg.fwd0 = fwd;
                flak_guns.push_back(fg);
            }
            // GUN-PAD TREE CLEARINGS (Chad 2026-08-29): the hilltop sites
            // stood inside the boreal scatter and the sight view was a wall
            // of conifers. 60 m of cleared canopy puts the nearest 16 m tree
            // below ~15 deg of the gunner's eye -- under the raider band.
            // TREE-ONLY cuts (add_tree_cuts): joining planet_cuts would
            // punch a terrain hole under each gun (the portal-surgery list).
            {
                std::vector<render::CutDisk> pad_cuts;
                for (const render::FlakDraw& g : flak_guns)
                    pad_cuts.push_back({g.up, 60.0});
                if (!pad_cuts.empty()) render::add_tree_cuts(pad_cuts);
            }
            // ★★★ L7 -- TRAMPLED SNOW AT THE GUN (Chad, 2026-09-03: "a gun
            // should not be placed on top of snow, just trample the snow down
            // around the gun", and "it should be trampled a little bigger than
            // the gun pad itself; it's fun to drive right up to the flak gun in
            // deep snow, I don't want a big 60 m cleared section").
            //
            // THE GUN DOES NOT MOVE. fg.pos above is still u_site *
            // radius_at(u_site) -- BARE TERRAIN, exactly where it was. What
            // changes is the SNOW: the crew tramples it flat where they stand.
            // Measured at the shipped sites, the Valley gun sat under 0.727 m
            // of ambient snow (drawn +0.768 m over the base) while the gunner's
            // own sight camera falls from 1.78 m over the base at 0 deg
            // elevation to 0.10 m at the 87 deg stop -- i.e. the sight went
            // UNDER the drawn snow as the barrel came up.
            //
            // THE RADIUS IS DERIVED, NOT DIALLED. The working circle is 3.048
            // m across [OP 909's 10 ft; assets/flak/flak_src/flak_gun_geom.py
            // WORKING_CIRCLE_D, docs/FLAK_GUN_SPEC.md §the measured table] --
            // pad radius 1.524 m -- and the gunner's own walk-up mark
            // st_approach stands 1.70 m behind the train axis, so 3.5 m is the
            // pad plus the man plus a hand's margin. The 1.5 m feather is the ramp that keeps the lip
            // rideable rather than a wall. keep 0.05 m is PACKED, not bare:
            // trampled snow is still snow, and a hard zero would make the pad
            // read as scraped pavement.
            //
            // ★ THIS IS ONE 5 m DISK, NOT THE 60 m TREE PAD ABOVE. The tree cut
            // answers a SIGHT-LINE question (conifers in the raider band); this
            // answers a STANDING question. Chad ruled explicitly against
            // widening the second to the first.
            //
            // ★ INIT-TIME, ONCE, BEFORE THE TICK LOOP: the disks are constants
            // derived from gun placement, which the sled tape's header already
            // pins through the world params, so no tape leg and no tap window
            // is touched (no sample_at call is added here -- registration only
            // pushes into a vector).
            {
                constexpr double kGunTrampleRadiusM = 3.5;
                constexpr double kGunTrampleFeatherM = 1.5;
                constexpr double kGunTrampleKeepM = 0.05;
                for (const render::FlakDraw& g : flak_guns)
                    snow_field.tramples.push_back({g.up, kGunTrampleRadiusM,
                                                   kGunTrampleFeatherM,
                                                   kGunTrampleKeepM});
            }
        }
        // FLAK battery: ONE 20 mm Oerlikon. Muzzle speed / cyclic are the
        // MEASURED facts from render/flak_gun.h (single-sourced with the GLB
        // extras + test_flak_gun); damage / reload / self-destruct are the
        // config dials; drag single-sourced from the cannon (world.toml rule).
        // muzzle_body is measured off the loaded GLB at MAN time (KEY_O).
        flak_fk.gw.battery.convergence_range = world.guns.convergence_range_m;
        flak_fk.gw.battery.guns.push_back(
            {glm::dvec3(0.0), weapon::Round::Cannon20mm,
             render::flak::kMuzzleSpeedMps, render::flak::kRofHz,
             world.guns.cannon_drag_k, world.guns.flak_damage});
        flak_fk.drum_rounds = render::flak::kDrumRounds;
        flak_fk.rounds_left = render::flak::kDrumRounds;
        flak_fk.reload_s = world.guns.flak_reload_s;
        flak_fk.selfdestruct_s = world.guns.flak_selfdestruct_s;
        flak_fk.prox_radius_m = world.guns.flak_prox_radius_m;
        // ── STAGE D: one AI gunner per placed gun. The world he fights with
        // is a COPY of the player's in every dial -- same battery, same drum,
        // same reload, same self-destruct, same proximity fuze -- because the
        // whole read is that the guns are the same gun. His FlakWorld carries
        // `manned = true` (an AI gunner IS a man on the gun); the frame loop
        // turns that off for whichever gun the player takes.
        for (int gi = 0; gi < static_cast<int>(flak_guns.size()); ++gi) {
            app::FlakAiGun ag;
            ag.gun = gi;
            ag.faction = flak_guns[gi].faction;
            // Per-gun seed: the ONLY difference between two gunners, so the
            // two guns' wander and burst rhythm never march in step.
            ag.ai.seed = static_cast<unsigned>(gi) * 2654435761u + 12345u;
            ag.ai.rng = ag.ai.seed ^ 0x9e3779b9u;
            ag.fk.mount = render::flak::make_mount_frame(
                flak_guns[gi].pos, flak_guns[gi].up, flak_guns[gi].fwd0);
            ag.fk.gw.battery = flak_fk.gw.battery;
            ag.fk.drum_rounds = flak_fk.drum_rounds;
            ag.fk.rounds_left = flak_fk.rounds_left;
            ag.fk.reload_s = flak_fk.reload_s;
            ag.fk.selfdestruct_s = flak_fk.selfdestruct_s;
            ag.fk.prox_radius_m = flak_fk.prox_radius_m;
            ag.fk.manned = true;
            flak_ai.push_back(ag);
        }
        flak_ai_shots_seen.assign(flak_ai.size(), 0LL);
        // Live bubble rebuild handle + dome look (single-sourced from config).
        cq.atm_field = &atm_field;
        cq.bubble_ceiling_m = game.atmosphere.bubble_ceiling_m;
        cq.bubble_edge_soft_m = game.atmosphere.bubble_edge_soft_m;
        cq.bubble_ceil_soft_m = game.atmosphere.bubble_ceil_soft_m;
        // S-domeround: the volume-preserving H the loader derived from
        // bubble_ceiling_m + bubble_dome_exponent (+ bubble_ceiling_
        // volume_preserve) — growth scales it exactly like ceiling_m.
        cq.bubble_dome_h_m = game.atmosphere.bubble_dome_h_m;
        app::rebuild_conquest_bubbles(cq);  // the 2 faction ellipse bubbles
    } else {
        // The R6 single TEST bubble (unchanged): centered on the SPAWN
        // direction so Chad starts inside and flies out to the soft edge.
        sim::AtmosphereField::Bubble test_bubble;
        test_bubble.center_dir = glm::normalize(loop.curr.position);
        test_bubble.ground_radius_m = game.atmosphere.bubble_radius_m;
        test_bubble.ceiling_m = game.atmosphere.bubble_ceiling_m;
        test_bubble.edge_soft_m = game.atmosphere.bubble_edge_soft_m;
        test_bubble.ceil_soft_m = game.atmosphere.bubble_ceil_soft_m;
        // S-domeround: the loader-derived volume-preserving H (or the
        // literal ceiling_m if bubble_ceiling_volume_preserve=false).
        test_bubble.dome_h_m = game.atmosphere.bubble_dome_h_m;
        atm_field.bubbles.push_back(test_bubble);
    }
    // CONQUEST force-enables the atmosphere field at startup (spec §1); the B
    // key toggle keeps working afterwards.
    if (game.atmosphere.enabled || conquest_on) {
        env.atm = &atm_field;
    }
    // R5e: the ESCAPE CLAIM latch lives in app::tick / LoopState now (it
    // severs the plant inputs, so it must be per-tick, not per-frame); the
    // HUD reads loop.escape_claimed below.
    // Gate on ANY live field (Fable-BEFORE P2-1): the old ground-only gate
    // would silently no-op a gravity-only activation, and R6/R11 fields
    // would re-arm the same trap.
    const sim::Environment* env_ptr = env.any_live() ? &env : nullptr;

    // The target-drone fleet (SPEC §0 S8-drone): `count` plant instances flown
    // by the dedicated bank-hold autopilot, scattered airborne over the world
    // (drone 0 ahead of the player). They advance inside step_frame in lockstep
    // with the player tick; nullptr would leave the player path bit-identical
    // (the firewall).
    app::DroneWorld dw;
    dw.dparams = scen.drone;
    dw.gparams = scen.gunsight;
    // Honest-pipper cant correction (Task A, iter-8): single-source convergence
    // range, hub muzzle position, and g from world.toml [guns] so
    // lead_solution can call weapon::harmonization_rise with the SAME
    // parameters the battery uses. hit_radius_m from scenario.toml [drone] for
    // the red cue.
    dw.gparams.convergence_range = world.guns.convergence_range_m;
    dw.gparams.hub_muzzle_body = world.guns.muzzle_hub;
    dw.gparams.hit_radius_m = scen.drone.hit_radius_m;
    // CONQUEST squadron presence (spec §6): the mode IS the 10 named tunnel-run
    // mavericks (combat::kNumMavericks). Force the maverick program on and pin
    // the fleet to EXACTLY 10 — the conquest roster (mav_alive[10], the 5v5
    // teams) is 1:1 with spawn_index 0..9, so a larger count would alias two
    // drones onto one maverick slot. Does not edit scenario.toml defaults
    // (which already ship 10 + maverick enabled); this just guarantees it.
    if (conquest_on) {
        // game-AI-R2 DECOUPLE (2026-08-05): [conquest] all_vs_player keeps its
        // ENGAGEMENT meaning (cq.params.all_vs_player above — everyone fights
        // the player) but NO LONGER zeroes maverick.enabled. The 2026-07-26
        // furball line here silently overrode scenario.toml's [maverick]
        // enabled = true, so with the shipped game.toml no tunnel run ever
        // started (the R1 probe: 10 drones, 100% PATROL for 20 min — a game
        // switch killing a scenario switch). The tunnel-run program is now
        // governed by scenario.toml [maverick] alone; turn runs off THERE if
        // a pure furball is ever wanted again.
        dw.dparams.count = combat::kNumMavericks;
    }
    for (int i = 0; i < dw.dparams.count; ++i)
        dw.drones.push_back(
            drone::spawn_drone(params, dw.dparams, i, dw.dparams.count));
    // CONQUEST spawn placement (fly-1 fix): the stock scatter clusters the
    // whole fleet around the PLAYER spawn sub-point — which sits near the
    // vacuum gap, so the squadron spawned OUTSIDE both bubbles in near-vacuum
    // air (mushing into terrain; the "bandits clustering east, one crashed"
    // report). In conquest each maverick is RE-PLACED around ITS OWN
    // faction's ellipse center: same deterministic golden-angle disc scatter
    // (no rng), 5 km arc radius (inside both cores; min semi-minor is
    // 13.05 km), spawn_alt above the bare sphere. Non-conquest path
    // untouched (bit-identical firewall). Respawn can't undo this: conquest
    // kills mark drones inert, respawn_in_place never runs.
    //
    // ★★★ L12 -- AND IT IS RE-RUN WHEN THE SIDE MENU ANSWERS. Which faction a
    // spawn_index flies for now follows the PLAYER'S side (combat/conquest.h
    // roster_sizes: his wing is always the small one), and this placement runs
    // three thousand lines before a window exists. So it is a LAMBDA, called
    // once here with the CONFIG side and again from the menu's team pick --
    // the same move L11b makes for the player's own spawn, and safe for the
    // same reason: the game is paused under the menu, not one tick has run.
    // Picking the config side re-writes identical values.
    const auto place_conquest_fleet = [&](int player_faction) {
        constexpr double kConquestSpreadM = 5000.0;
        const double golden = 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < static_cast<int>(dw.drones.size()); ++i) {
            const bool valley = combat::maverick_faction(i, player_faction) ==
                                combat::CQ_VALLEY;
            const glm::dvec3 c = glm::normalize(
                valley ? world::kValleyCenterDir : world::kSudburyCenterDir);
            // Per-faction slot index (0..4 within each team of 5) so each
            // team fills its own disc evenly rather than sharing one fill.
            const int slot = combat::team_slot(i);
            const double frac =
                (static_cast<double>(slot) + 0.5) /
                combat::team_size(combat::maverick_faction(i, player_faction),
                                  player_faction);
            const double ang = (kConquestSpreadM / params.R) * std::sqrt(frac);
            const double brg = golden * static_cast<double>(i);
            glm::dvec3 ref{0.0, 1.0, 0.0};
            if (std::abs(glm::dot(ref, c)) > 0.9) ref = glm::dvec3{1, 0, 0};
            const glm::dvec3 east = glm::normalize(glm::cross(ref, c));
            const glm::dvec3 north = glm::cross(c, east);
            const glm::dvec3 dir = glm::normalize(
                std::cos(ang) * c +
                std::sin(ang) * (std::cos(brg) * east + std::sin(brg) * north));
            const glm::dvec3 pos = dir * (params.R + dw.dparams.spawn_alt);
            const glm::dvec3 heading =
                std::cos(brg) * east + std::sin(brg) * north;
            drone::DroneState& d = dw.drones[static_cast<size_t>(i)];
            d.curr = drone::level_state_at(dw.dparams, pos, heading);
            d.prev = d.curr;
            d.grounded = true;  // first tick pairs control::reset, as stock
        }
    };
    if (conquest_on) place_conquest_fleet(cq.state.player_faction);

    // Combat kill-loop (kill-loop design): HP + hit detection + FX.
    // Params from scenario config (hp, hit_radius_m).
    combat::CombatWorld cw;
    cw.params.hit_radius_m = scen.drone.hit_radius_m;
    // Bandit return fire + player stakes (bandit_combat_plan.md): the static
    // combat setup (difficulty, bandit gun, player HP) from scenario.toml
    // [combat]. player_hp starts full. Bandit rof/damage are derived from
    // difficulty at fire time, so only the level + gun ballistics come from
    // here.
    cw.setup = scen.combat;
    cw.player_hp = scen.combat.player_hp;
    // CONQUEST no-respawn (spec §4a): a killed maverick STAYS dead (frozen
    // wreck). false => combat_tick marks the drone inert instead of respawning.
    if (conquest_on) cw.respawn_drones = false;
    // Component damage feel dials (damage_model_plan.md) from scenario.toml
    // [damage]. Identity at zero damage, so this is bit-identical until a hit
    // lands — it just lets Chad tune the hurt-plane feel without a recompile.
    cw.damage_params = scen.damage;

    // Combat-audio edge detectors: cw.kills / cw.hits are monotone counters
    // (advanced only inside combat_tick — read-only here). Their per-frame
    // delta triggers the explosion / hit one-shot voices. Seeded to the current
    // value so the first frame never fires a phantom boom.
    long long prev_kills = cw.kills;
    long long prev_hits = cw.hits;

    bool raw_mode = false;     // start in the instructor
    double pending_dx = 0.0;   // mouse delta accrued across frames until a tick
    double pending_dy = 0.0;   //   consumes it (no loss when render fps > sim)
    bool refocus_drop = true;  // drop the next live mouse delta (first frame /
                               //   first focused frame — cursor-warp spike)
    // MB-7c display state (cosmetic, render-only — off every control path):
    // the wingtip vortex trails and the smoothed speed-trend cue.
    render::VortexTrails vortices;
    // Feature B: dual wingtip airshow smoke — the SMOKE stop of the N display
    // cycle (after Halo; off at every other stop). Launch state = NeonRim,
    // smoke off. V is reserved for a future cockpit view (Chad 2026-08-06).
    render::WingtipSmokeTrails wingtip_smoke;
    // ★ R5 rows 4/5/6: the sled ROOST / EXHAUST / RIDER-BREATH puff trails —
    // app-owned pure bookkeeping (render/sled_plumes.h, the wingtip_smoke
    // precedent), advanced per render frame off SledState, drawn read-only.
    render::SledPlumes sled_plumes;
    bool smoke_on = false;
    double speed_trend = 0.0;
    double prev_speed_for_trend = glm::length(loop.curr.velocity);
    // render/rig-D port (plane-model-only): the last-committed commanded
    // Inputs, held across 0-tick frames (mirroring how the HUD holds its
    // SimState read) so the Fleet Rig's cosmetic control-surface deflection
    // never chases a stale zero on a fast-render/slow-sim frame. Render-only,
    // read from FrameResult::last_inputs, never written back into sim/control
    // (RA9).
    sim::Inputs player_inputs_disp{};

    // rig-B (render-only, RA9): the last commanded surfaces, carried for the
    // Fleet Rig's control-surface deflection. Persistent so a render frame with
    // no sim tick keeps the previous command. Housed HERE (render display
    // state) NOT beside the FrameInput fill, so it can never be routed back
    // into control (Fable before-consult P1). Updated only on a ticking frame;
    // snaps to neutral on a respawn.
    sim::Inputs rig_player_inputs{};
    // R4 wheel-spin display state (cosmetic; accumulated below, tick-derived).
    double wheel_roll_rad = 0.0;

    // Decoupled lagging camera-forward (S7-cam Phase 1): persistent world dir,
    // eased toward a velocity-anchored target each frame. Seed to the nose so
    // the first frame doesn't streak from the identity.
    glm::dvec3 cam_fwd = loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
    // Camera-up = the CARRIED aim-frame up (S7-cam3, reverses S7-cam2). Set
    // from loop.aim.up() each instructor frame below, so the camera shows the
    // world from the aim frame's orientation (mouse-up == screen-up at any
    // attitude). This seed is the spawn's level up (the aim was reseeded to
    // nose/local_up above). Raw mode leaves it stale — chase_camera builds its
    // own up.
    glm::dvec3 cam_up = loop.aim.up();

    int frames = 0;
    int render_frame =
        0;  // monotonic render-frame ordinal (post-pass grain seed)
    // DEBUG celestial time-scrub (Stage 3b observability): hold ] / [ to wheel
    // the sun forward/back so the day/night cycle (a 1-hour day by design, so
    // ~static within one dogfight) is watchable in seconds. Feeds ONLY render
    // (t_cel -> sun_dir); never sim/control/drone, never the probe (interactive
    // only, gated on smoke_frames==0), so the deterministic dump is untouched.
    double cel_time_offset = smoke_cel_offset;
    // DEBUG force-haze (Stage-3 scatter observability): the scatter is gated by
    // the weather amount, and weather is CLEAR ~70% of the time; the ]/[ scrub
    // wheels the sun AND weather together (same t_cel), so a hazy front rarely
    // coincides with a low sun — the halo is nearly untestable in the wild.
    // Toggle '\' to FORCE full overcast so the scatter geometry (rim blue high
    // / orange sun-halo low) can be flown independent of the weather roll.
    // Feeds ONLY atm.haze_density (render); never sim/control/drone/probe,
    // interactive only — the deterministic dump is untouched. NOT the shipped
    // look.
    bool force_haze = false;
    // S-airdome (spec §4, a sanctioned smoke-only hook, the SEADS_SMOKE_GEAR
    // pattern): SEADS_SMOKE_FORCE_HAZE forces the same '\' overcast the
    // interactive key drives, since debug keys are never polled on the
    // hands-off --smoke path (smoke_frames==0 gate below).
    if (smoke_frames > 0 && std::getenv("SEADS_SMOKE_FORCE_HAZE"))
        force_haze = true;
    // F9 RECORD: persistent recorder + toggle state. The version tag now TRACKS
    // THE BUILD (2026-07-29 — the build-info seam, cmake/build_info.cmake):
    // `<branch>@<git describe --tags --always --dirty>` of the tree this binary
    // was compiled from, regenerated on every build. It was previously a
    // hand-maintained constant ("v5-reconcile@46051ca23") that had already gone
    // stale and stamped Golden Felt Flight #2 with a kernel it was not flown on
    // — a provenance field you must correct in metadata afterwards is worse
    // than none, because it reads as authoritative while being wrong. A
    // `-dirty` suffix is load-bearing: it means the sources were edited, so the
    // recording is not reproducible from any commit.
    seads_replay::Recorder felt_recorder;
    bool f9_recording = false;
    double f9_saved_at_s = -1e18;
    char f9_saved_name[64] = {0};
    // `<branch>@<describe> kernel=<seal>`. The kernel seal is STATED (from
    // ./KERNEL_SEAL), not inferred: the flight kernel is cherry-picked into
    // this repo, so its tag is unreachable from HEAD and `git describe` falls
    // back to the nearest reachable GAME seal — a header that named only
    // describe would read "game-kernel-v5" on a build flying v7 kernel content.
    const std::string felt_version_tag = std::string(app::kBuildBranch) + "@" +
                                         app::kBuildDescribe +
                                         " kernel=" + app::kKernelSeal;
    const char* kFeltFlightVersionTag = felt_version_tag.c_str();
    // CONQUEST TAPE (docs/conquest_tape_spec.md §4): AUTO-ON exactly when
    // conquest_on — no keybind, no config knob (unlike F9's manual toggle).
    // Constructed BEFORE the frame loop; tag = the game.toml-derived scenario
    // name (falls back to "conquest" if scenario naming is unavailable here).
    // The ofstream is opened ONCE and the ctor's drained hdr line is written
    // immediately, so a crash tape still carries its header.
    std::unique_ptr<seads_tape::ConquestTape> conquest_tape;
    std::ofstream conquest_tape_file;
    long long conquest_tape_last_flush = 0;
    if (conquest_on) {
        conquest_tape = std::make_unique<seads_tape::ConquestTape>("conquest");
        // BINARY mode: the fnv1a footer hashes the exact bytes appended
        // ("\n" line ends); a text-mode Windows ofstream writes "\r\n" and
        // the analyzer's re-hash of the on-disk bytes would MISMATCH on
        // every clean tape (caught on the first real smoke tape).
        conquest_tape_file.open(next_conquest_tape_name(), std::ios::binary);
        std::string hdr_buf;
        if (conquest_tape->drain(hdr_buf)) {
            conquest_tape_file << hdr_buf;
            conquest_tape_file.flush();
        }
    }
    // Smoke-only debug hook (like SEADS_OBLIQUE): force the gear DOWN in a
    // hands-off smoke shot so the rig-D landing-gear stance can be captured
    // (smoke has no device latch to lower it). Never affects interactive play.
    const bool smoke_gear = smoke_frames > 0 && std::getenv("SEADS_SMOKE_GEAR");
    // Smoke-fire debug hook (mirror of smoke_gear): hold the trigger in a
    // hands-off --smoke shot so tracers appear in the screenshot.
    // SEADS_SMOKE_FIRE=1; only active when smoke_frames > 0.
    const bool smoke_fire = smoke_frames > 0 && std::getenv("SEADS_SMOKE_FIRE");
    // ATMOSPHERE AS-1..AS-3 evidence rig (docs/atmosphere_snow/):
    // SEADS_SNOW_FORCE=<0..1> forces FrameInfo::precip_intensity POST air-gate
    // so a screenshot can show a NAMED intensity -- a flurry at 0.3, a squall
    // at 1.0 -- instead of waiting for the storm budget and the eye's ground
    // track to line up. Read ONCE here, smoke-only, exactly like the two above:
    // in live play smoke_frames == 0 and this is a null pointer forever.
    const char* const snow_force_smoke =
        smoke_frames > 0 ? std::getenv("SEADS_SNOW_FORCE") : nullptr;
    render::ProbePlacement probe_place;  // probe bandit geometry (probe mode)
    glm::dvec3 probe_level_fwd{0.0, 0.0,
                               -1.0};  // pre-repoint level cam forward
    // Smoke-only per-frame draw timing accumulators (perf measurement).
    double smoke_ms_sum = 0.0, smoke_ms_min = 1e30, smoke_ms_max = 0.0;
    int smoke_ms_n = 0;
    std::vector<double> smoke_ms_all;  // S0/D2: per-frame, for percentiles
    // M-KEY FULL-SCREEN BUBBLE MAP (Chad's ask, 2026-07-25): a pure
    // render-layer toggle (never sim/control state) — SEADS_MAP=1 starts the
    // map already open (the SEADS_TUNCAM precedent) so a --smoke screenshot
    // can certify the map visually; interactively, KEY_M flips it live.
    bool map_open = std::getenv("SEADS_MAP") != nullptr;
    // ★★★ L4 — THE MAP'S VIEW, OWNED HERE AND PERSISTENT
    // (docs/PLAN_20260901_game_loop_millwright.md §4.4; render/bubble_map.h's
    // MapView banner has the why). It lives beside `map_open` and NOT inside
    // the renderer because the zoom must survive closing and reopening the
    // chart -- a view that reset every time you pressed M would make the wheel
    // useless for exactly the job Chad asked it to do, which is reading one
    // pump's surroundings across several visits.
    render::MapView map_view;
    // ★ SEADS_MAP_ZOOM: the SEADS_MAP precedent, one step further. The wheel
    // is a live input and a --smoke run has no hands, so without this the ONE
    // thing a screenshot could never certify is the zoomed chart -- which is
    // the half of L4 that changes what is drawn (declutter, follow, the
    // re-baked basemap). Clamped through the same law the wheel uses, so the
    // env can never put the view somewhere the wheel could not.
    if (const char* mz = std::getenv("SEADS_MAP_ZOOM")) {
        map_view.zoom = std::atof(mz);  // the frame loop clamps it, below
    }
    // ★ LIGHT TUNE (Chad, 2026-08-17: "which button do I press to turn off
    // frost? And lighting tuning?"). The frost half of that question is now
    // answered by DELETION -- there is no helmet fog and so no key for it (his
    // 2026-08-17 ruling, "no more frost just turn it off"); what is left here
    // is the lighting half. The three measured causes of the light flood lived
    // in config/world.toml behind a relaunch. Chad judges visuals BY EYE, so he
    // needs to move them live, read the number off the screen, and tell us what
    // to write into the TOML. Same debug-key class as K/T/B/M: pure app-owned
    // render state, never sim/control state, never anything a tape or the
    // kernel can see.
    //
    // Bases are the SHIPPED config values; nothing here changes a default. Zero
    // key presses => every dial sits exactly on its base and tune_armed stays
    // false, so the frame is bit-identical to the shipped look.
    const float tune_day_base =
        static_cast<float>(world.atmosphere.ground_day_gain);
    const glm::vec3 tune_glow_base = glm::vec3(world.ground.night_glow);
    const float tune_moon_base = static_cast<float>(world.moon.ground_gain);
    // false until a tune key is pressed => no plate. SEADS_TUNE=1 arms it at
    // launch (the SEADS_MAP precedent) so a --smoke screenshot can certify the
    // plate visually -- the interactive keys are gated off under --smoke.
    bool tune_armed = std::getenv("SEADS_TUNE") != nullptr;
    int tune_dial = 0;  // 0 day_gain · 1 night_glow · 2 moon ground_gain
    float tune_day_gain = tune_day_base;
    // night_glow is dialled as ONE scalar on the shipped triple so its COLOUR
    // RATIO is preserved -- the loader enforces a COOL night (b > r) and the
    // hue is not what is being tuned; the flood is.
    float tune_glow_mul = 1.0f;
    float tune_moon_gain = tune_moon_base;
    // T25: the stutter attribution instrument (opt-in; see FrameProf above).
    g_prof.on = std::getenv("SEADS_PROF") != nullptr;
    if (g_prof.on)
        std::printf(
            "[PROF] SEADS_PROF live — spike lines print when a frame "
            "exceeds max(2x median, 20 ms)\n");
    // -----------------------------------------------------------------------
    // ⭐ THE INTRO SEQUENCE — the black, the phone call, and the handoff into
    // gameplay. The TITLE already happened: it was the loading page above.
    //
    //     TITLE (loading page)  ->  BLACK: music fades out, wind rises
    //     0:00 wind alone -> 0:08 answering machine -> 0:37 ringing
    //     -> 0:50 operator -> gameplay, music back and quieter
    //
    // That is Chad's readme shape ("Resigned to fate for AFTER title screen and
    // black screen to entry"; "louder in the loading page and quieter during
    // the gameplay"). The previous build had the title card at the END and the
    // music off throughout — the opposite — because it was assembled from chat
    // while his readmes were unreadable on disk.
    //
    // The voice ORDER is his explicit ruling, chosen over the conventional
    // ring-then-machine reading: wind alone -> answering machine -> ringing ->
    // operator. All cue times live in render/intro_sequence.h and are pinned by
    // the gate.
    //
    // It runs HERE, after the world build, so the sequence ends into a game
    // that is already standing up — and now the load has its own page in front
    // of it rather than a blank window.
    //
    // Skippable on any key or click — a 64 s intro that cannot be escaped is a
    // 64 s intro you come to resent. Gated on smoke_frames == 0: a --smoke run
    // must never sit through this, or every screenshot gate and timing
    // baseline in the repo breaks.
    // -----------------------------------------------------------------------
    // Where the gameplay music swell STARTS. Normally 0 — the intro ran to the
    // end, the bed is out, and it comes back from silence. On a SKIP it is
    // seeded so the gameplay bed picks up at the level the loading-page bed was
    // just playing at: skipping at t = 1 s cuts a nearly-full bed, and starting
    // the swell from zero there would drop the player into 4 s of near-silence.
    double music_return_seed = 0.0;
    if (smoke_frames == 0) {
        double intro_t = 0.0;
        int intro_cue = 0;
        // The gameplay beds stay silent through the sequence and swell in
        // afterwards (intro_music_return below). The bed you can still HEAR at
        // t = 0 is intro_music, the loading-page handle, fading out under the
        // rising wind.
        if (music_surface_ok) SetMusicVolume(music_surface, 0.0f);
        if (music_deep_ok) SetMusicVolume(music_deep, 0.0f);

        // Drain input accumulated during the world load. The loading page polls
        // events every pump, but a user who clicked the window to focus it, or
        // tapped a key wondering whether the title had hung, would otherwise
        // skip the entire intro on its first frame. Keys are a FIFO and must be
        // read out; a mouse-button PRESS is an edge recomputed each poll, so a
        // second PollInputEvents is what retires it (calling
        // IsMouseButtonPressed has no side effect and would not).
        PollInputEvents();
        while (GetKeyPressed() != 0) {
        }
        PollInputEvents();
        while (GetKeyPressed() != 0) {
        }

        // Hold the page until it has had its minimum, so a warm-cache load does
        // not flash the title and cut straight to black.
        while (!WindowShouldClose() && !render::load_page_done(load_t))
            pump_loading(nullptr);
        if (load_prof)
            std::printf(
                "[LOADPAGE] title up for %.2f s total; handing off to black\n",
                load_t);
        if (load_prof) std::fflush(stdout);

        // ⚠ The sequence clock is the ABSOLUTE clock, not accumulated
        // GetFrameTime(). Two reasons, both learned here:
        //
        // (1) GetFrameTime() is set in EndDrawing as update+draw, where
        //     `update` was measured at the PREVIOUS BeginDrawing — so read at
        //     the top of a loop it reports the frame before last. The old
        //     `intro_first_frame` guard zeroed frame 1, which put the work
        //     between the last loading pump and this loop's first draw into
        //     frame 2 instead of dropping it. Anything heavy landing in that
        //     range would silently push intro_t past a cue and the stale-cue
        //     guard would swallow the answering machine — the exact bug shape
        //     Chad's first fly caught, one frame further along.
        // (2) It is still deliberately UNCLAMPED wall time: the cues are
        //     scheduled off intro_t but the sounds play at wall-clock rate once
        //     triggered, so clamping would permanently desync them from their
        //     own audio the first time a frame ran long. Minimizing the window
        //     parks this loop in glfwWaitEvents while the audio device keeps
        //     running; with real time the timeline stays honest and the
        //     sequence simply ends.
        const double intro_t0 = GetTime();
        while (!WindowShouldClose() && !render::intro_finished(intro_t)) {
            intro_t = GetTime() - intro_t0;

            // Skip on any key or click — but ALWAYS drain the key queue, even
            // while the skip is disarmed. raylib buffers presses in a FIFO, so
            // a key tapped at t = 0.1 that is merely not READ is still sitting
            // there at t = 0.51 and skips the intro the instant the guard
            // opens. Ignoring input means consuming it, not deferring it.
            bool key_hit = false;
            while (GetKeyPressed() != 0) key_hit = true;
            const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            if (intro_t > 0.50 && (key_hit || click)) {
                // Hand the level over rather than dropping it: the swell starts
                // wherever the loading-page bed had got to.
                music_return_seed =
                    render::kIntroMusicReturn *
                    std::clamp(render::intro_music_gain(intro_t) /
                                   render::kLoadMusicGain,
                               0.0, 1.0);
                break;  // skip
            }

            // Fire every cue whose time has passed. A while-loop rather than a
            // single test so a hitched frame can never swallow a cue.
            while (intro_cue < render::kIntroCueCount &&
                   intro_t >= render::kIntroCues[intro_cue].t) {
                // A cue the frame jumped clean over is CONSUMED, not fired.
                // In normal running the overshoot is one frame (~17 ms); after
                // a stall it can be tens of seconds, and firing three stale
                // one-shots at once would be worse than the silence.
                if (intro_t - render::kIntroCues[intro_cue].t >
                    render::kIntroCueStale) {
                    ++intro_cue;
                    continue;
                }
                if (load_prof) {
                    std::printf(
                        "[INTRO] t=%6.2f  cue %d fires\n", intro_t,
                        static_cast<int>(render::kIntroCues[intro_cue].cue));
                    std::fflush(stdout);
                }
                switch (render::kIntroCues[intro_cue].cue) {
                    case render::IntroCue::kMachine:
                        if (intro_machine_ok) PlaySound(intro_machine);
                        break;
                    case render::IntroCue::kRing:
                        // Re-triggered once per cycle, so the ring count is a
                        // dial rather than a property of the recording.
                        if (intro_ring_ok) PlaySound(intro_ring);
                        break;
                    case render::IntroCue::kOperator:
                        if (intro_operator_ok) PlaySound(intro_operator);
                        break;
                }
                ++intro_cue;
            }

            if (intro_wind_ok) {
                UpdateMusicStream(intro_wind);
                SetMusicVolume(
                    intro_wind,
                    static_cast<float>(render::intro_wind_volume(intro_t)));
            }
            // "BLACK, music fades out, wind rises." The loading-page bed is
            // still the one playing; it leaves under the wind and is gone
            // before the answering machine speaks (static_assert'd in
            // intro_sequence.h).
            if (intro_music_ok) {
                UpdateMusicStream(intro_music);
                SetMusicVolume(
                    intro_music,
                    static_cast<float>(render::intro_music_gain(intro_t)));
            }

            BeginDrawing();
            ClearBackground(kIntroBackdrop);

            // The handoff to black: the title arrives still lit from the
            // loading page and dissolves out over the first beat, so the cut
            // from "title screen" to "black screen" is a dissolve rather than a
            // hard cut into a phone call.
            draw_title_card(render::intro_title_alpha(intro_t));

            const char* hint = "any key to skip";
            DrawText(hint, GetScreenWidth() - MeasureText(hint, 18) - 28,
                     GetScreenHeight() - 40, 18,
                     to_rgba(render::kComplementBlue, 0.35));
            EndDrawing();
        }

        // The intro channels are never referenced again — free them here
        // rather than carrying five idle decoded buffers through the flight.
        // intro_music is the big one: its deep sub-buffer is ~11 MB and exists
        // only so the loading page could have a bed.
        if (intro_music_ok) {
            StopMusicStream(intro_music);
            UnloadMusicStream(intro_music);
            intro_music_ok = false;
        }
        if (intro_wind_ok) {
            StopMusicStream(intro_wind);
            UnloadMusicStream(intro_wind);
            intro_wind_ok = false;
        }
        if (intro_machine_ok) {
            StopSound(intro_machine);
            UnloadSound(intro_machine);
            intro_machine_ok = false;
        }
        if (intro_ring_ok) {
            StopSound(intro_ring);
            UnloadSound(intro_ring);
            intro_ring_ok = false;
        }
        if (intro_operator_ok) {
            StopSound(intro_operator);
            UnloadSound(intro_operator);
            intro_operator_ok = false;
        }
    }

    // "-> gameplay, music returns QUIETER." Time since the intro handed over,
    // driving the swell that brings the gameplay bed back up. Without it the
    // bed snaps from silence to full on the first flight frame, which reads as
    // a bug rather than as a return. Multiplies whatever the MusicDirector
    // asks for, so the crossfade and the blast duck are untouched; it reaches 1
    // after render::kIntroMusicReturn seconds and never moves again. Seeded on
    // a skip so the bed hands over at its current level (see above).
    double music_return_t = music_return_seed;

    // ★★★ L3 -- THE SPAWN MENU (app/spawn_menu.h) AND THE BIRTH IT ORDERS.
    //
    // It is armed HERE, not at the spawn call three thousand lines up, because
    // the answer needs a world: the pumps, the snowpack and the window all come
    // into existence between that call and this line. So the FIRST frame of the
    // session opens the menu over an already-spawned aeroplane, the game is
    // PAUSED under it (step_frame is fed a zero dt below), and a Snowmachine
    // answer re-places BOTH bodies before a single tick has run.
    app::SpawnMenuState spawn_menu;
    spawn_menu.first_spawn = true;
    // ★★★ L11 -- THE SIDE, ASKED FIRST AND ASKED ONCE (Chad 2026-09-10:
    // "allow player to choose 'central city' or 'valley' spawn for the
    // teams"). The first birth of the session opens on the SIDE stage and
    // falls through to the ride stage when it is answered; every respawn below
    // opens on the ride stage, because you do not change armies by dying.
    spawn_menu.stage = app::kSpawnStageTeam;
    spawn_menu.team = app::kSpawnMenuTeamValley;  // the shipped default
    // ⚠ NEVER UNDER --smoke / --probe. A menu that waits for a keypress in a
    // headless run is a hang, and a hang in CI reads as a timeout.
    spawn_menu.open = smoke_frames == 0;
    // ★ 2026-09-03 (Chad): "the choice for snowmachine should only be
    // available after a pump has been damaged" -- at the first spawn too.
    // The row is drawn dead with its reason until an own surface pump is
    // dead or damaged (the same gate the respawn menu uses).
    spawn_menu.machine_offered =
        conquest_on && app::own_surface_pump_to_fix(cq.state) >= 0 &&
        snow_field.hf != nullptr;
    // The cursor: mouse-aim runs with it captured (DisableCursor), and a menu
    // you cannot point at is half a menu. Released while it is open, restored
    // to whatever the input mode wants when it closes.
    if (spawn_menu.open && smoke_frames == 0) EnableCursor();

    // ★★★ THE SLED TAPE'S THREE VERBS, IN ONE PLACE
    // (red-team finding, 2026-09-01). Before this there were FOUR hand-copied
    // open blocks, two hand-copied close blocks and three bare
    // `sled_tape.on_override` calls scattered through `main()`. Two of the open
    // blocks were the SAME open, duplicated inside one pair of braces by an
    // edit that lost a `}` -- the second `ofstream::open` on an already-open
    // stream sets `failbit`, `is_open()` still reports true, and every
    // subsequent `<<` is a silent no-op, so the mount-seeded tape file was
    // created and then left EMPTY. That defect predates this lane (it is on
    // origin/main); it is fixed here because these three verbs are what the
    // override law below is enforced through, and an instrument that writes
    // nothing cannot enforce anything. `test_sled_tape.cpp` now owns a leg that
    // reproduces the double-open directly.
    const auto sled_tape_close = [&]() {
        if (!sled_tape_on) return;
        std::string buf;
        if (sled_tape.drain(buf)) sled_tape_file << buf;
        sled_tape_file << sled_tape.footer();
        sled_tape_file.flush();
        sled_tape_file.close();
        sled_tape_on = false;
    };
    const auto sled_tape_open = [&]() {
        sled_tape_close();  // one tape per episode -- never two writers, ever
        sled_tape = seads_sledtape::Writer{};
        sled_tape_file.open(next_sled_tape_name(),
                            std::ios::out | std::ios::binary);
        char note[128];
        std::snprintf(note, sizeof note, "hf_faceted=%d depth_base=%.3f",
                      snow_field.p.hf_faceted_ground ? 1 : 0,
                      snow_field.p.base_m);
        sled_tape.begin(felt_version_tag, params.sim_dt, sled_params, note,
                        sled_tick_no, sled);
        sled_tape_on = sled_tape_file.is_open();
    };
    // ★★★ EVERY OUT-OF-KERNEL WRITE TO `sled` GOES THROUGH HERE, AND THE LAW
    // IS ENFORCED RATHER THAN REMEMBERED. `app::sled_override_replayable`
    // (app/player_mount.h) knows which of `SledState`'s fields the pin roster
    // carries; a state it refuses -- today, exactly a DETACHED GRIP -- would be
    // recorded as one machine and replayed as another, so the episode ENDS
    // there instead. That is the same thing `BoardAircraft` already does, for
    // the same reason, and it is now what a J dismount does too.
    const auto sled_tape_mark_override = [&](const sim::SledState& s_) {
        if (!sled_tape_on) return;
        if (app::sled_override_replayable(s_)) {
            sled_tape.on_override(sled_tick_no, s_);
            return;
        }
        sled_tape_close();
    };

    // ★★★ BORN ON THE MACHINE. Everything a Snowmachine spawn has to do, in
    // ONE place, so the first spawn and every later respawn cannot drift:
    //
    //   the machine   seeded at [spawn] sled_dist_m from the pump that wants a
    //                 wrench, on the bearing from that pump toward our own
    //                 bubble centre, grounded to the DRIVE surface, facing the
    //                 pump (app::solve_sled_spawn owns all of that geometry);
    //   the man       seated on it, through THE mount seam;
    //   the aeroplane parked beside it, grounded and stopped, so a snowmachine
    //                 spawn never costs you the aircraft.
    //
    // ⚠ THE GROUND QUERIES ARE HERE, IN THE FRAME BLOCK, AND NOT INSIDE THE
    // TICK LOOP -- the same law the L1 dismount block obeys: `sample_tap` is
    // armed around the tick loop for the sled's own queries, and a
    // `drive_radius_at` fired inside that window makes every tape of the drive
    // unreplayable (the R4c finding). It is null out here.
    //
    // Returns false when the world cannot place a machine -- the caller then
    // leaves the aeroplane exactly as it was born.
    const auto spawn_on_machine = [&]() -> bool {
        if (!conquest_on || snow_field.hf == nullptr) return false;
        const int pi = app::spawn_target_pump(cq.state);
        if (pi < 0) return false;
        glm::dvec3 centre, major;
        double ea = 0.0, eb = 0.0;
        world::FactionGrowth grow[2];
        app::faction_growth_from_state(cq.state, grow);
        world::faction_ellipse(cq.state.player_faction, grow, centre, major, ea,
                               eb);
        app::SledSpawnPlacement place = app::solve_sled_spawn(
            cq.state.pumps[pi].pos, centre, params.R, spawn_dials);
        if (!place.valid) return false;
        place.sled_pos =
            place.sled_dir * (snow_field.drive_radius_at(place.sled_dir) +
                              sled_params.cg_height_m + 0.05);
        const double air_r =
            (env.ground != nullptr)
                ? env.ground->radius_at(place.aircraft_dir)
                : params.R;
        place.aircraft_pos =
            place.aircraft_dir * (air_r + env.ground_params.contact_height_m);
        // The machine, then the rider, then the aeroplane -- and the two sled
        // writes go through the ONE seeding function and the ONE mount seam.
        app::seed_sled_at(sled, place.sled_pos, place.sled_dir, place.sled_fwd);
        sled_prev = sled;
        render::sled_plumes_reset(sled_plumes);
        app::player_mount_request(sled, walker);
        gait = sim::GaitState{};  // ★★★ GAIT LADDER G1: reset beside the walker
        ++sled_epoch;  // an external write to `sled` -- see the decl
        // ★ THROUGH THE NAMED FORCE, NEVER A RAW ASSIGNMENT. A respawn can
        // interrupt a repair -- you were killed mid-fix -- and a bare
        // `player.mode = Sled` here left `repairing`/`repair_pump` pointing at
        // a pump 300 m behind the new machine. `player_mode_force` is the one
        // write that settles the hook (app/player_mode.h).
        app::player_mode_force(player, app::PlayerMode::Sled);
        player.sled_seeded = true;
        // The app-side input accumulators are not kernel state and survive a
        // birth (SK-1c audit defect 5, the same argument as the mount seed).
        sled_steer_cmd = 0.0;
        sled_lean_lat = 0.0;
        sled_lean_fwd = 0.0;
        sled_thumb = 0.0;
        sled_brake_cmd = 0.0;
        // The aircraft's latched throttle ramp is frozen across the drive
        // exactly as the KEY_J seed freezes it (§8 P1-3).
        saved_throttle_target = live_dev.throttle_target;
        live_dev.throttle_target = 0.0;
        live_dev.gear_down = true;  // it is PARKED, not bellied in
        loop.curr = app::player_spawn(app::PlayerSpawnChoice::Snowmachine,
                                      params, spawn_alt, loop.spawn_up,
                                      loop.spawn_fwd, &place);
        loop.prev = loop.curr;
        loop.prev_up = sim::local_up(loop.curr.position);
        loop.aim.reseed(loop.curr.orientation, loop.prev_up);
        loop.grounded = true;
        // ★ SLED TAPE R1: one tape per episode, and this IS an episode -- it
        // starts with an out-of-kernel state write, exactly like the mount
        // seed, so the tape's first body record is the post-seed pin.
        sled_tape_open();
        sled_note = "FIX THE PUMP";
        sled_note_until_s = GetTime() + 3.0;
        return true;
    };

    while (!WindowShouldClose()) {
        if (g_prof.on) g_prof.begin();
        // ★ ROAD-REPAIR E2: the flash rig FREEZES the world. Zero dt means no
        // tick, no drift, no animation -- so the ONLY thing that differs
        // between two flash frames is the centimetre the eye moved, and any
        // pixel that changes changed because a depth tie flipped.
        const double frame_dt =
            flash_site != nullptr
                ? 0.0
                : (smoke_frames > 0 ? params.sim_dt : GetFrameTime());
        const double clamped_dt = std::clamp(frame_dt, 0.0, kMaxFrameDt);

        // v5 HUD RESTYLE VERIFY: at frame 30 (frames==29, 0-indexed, before
        // this iteration's increment), apply the requested one-shot aim
        // nudge DIRECTLY to loop.aim via the SAME call family apply_mouse
        // uses — input::AimFrame::apply_mouse(dx, dy, sens), the
        // frame-axis (carried aim-frame own up/right) overload, sens=1 so
        // dx/dy ARE the radians to rotate. Per the documented sign
        // convention (dx>0 = mouse right = aim yaws right; dy>0 = mouse
        // down = aim pitches down), "LEFT by offset_aim_deg" is
        // dx = -offset_rad, and "DOWN by aim_down_deg" is dy = +down_rad.
        // This bypasses the whole per-frame mouse-delta accrual/consume
        // path (pending_dx/dy, aim_curve, CQ2) entirely — a debug-only
        // direct write to the aim state, never reachable outside --smoke.
        if (smoke_frames > 0 && frames == 29 &&
            (smoke_offset_aim_deg != 0.0 || smoke_aim_down_deg != 0.0)) {
            constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
            loop.aim.apply_mouse(-smoke_offset_aim_deg * kDeg2Rad,
                                 smoke_aim_down_deg * kDeg2Rad, 1.0);
        }

        // Mode toggle (F1): a clean reseed on the next tick either way.
        if (smoke_frames == 0 && IsKeyPressed(KEY_F1)) {
            raw_mode = !raw_mode;
            loop.grounded = true;
            // Reseat the lagged camera-forward on the nose so switching into
            // instructor doesn't ease in from a stale direction.
            cam_fwd = loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            cam_up = loop.aim.up();  // reseat from the (reseeding) aim frame
            orient_tap.reset();      // no cross-mode-toggle double-tap
            prev_freelook = false;
            pending_orient =
                false;  // drop an undelivered fire across the toggle
            // The Enable/DisableCursor below warps the cursor — the next
            // poll_live delta is the warp, not the hand, and the MB-aim curve
            // would amplify it gain_max x (diff red-team P1: the same spike
            // refocus_drop kills on focus regain; raw-mode frames reset the
            // flag false, so entering the instructor via F1 needs the re-arm).
            refocus_drop = true;
            if (raw_mode)
                EnableCursor();  // raw uses the absolute virtual stick
            else
                DisableCursor();  // instructor uses relative mouse-aim
        }

        // F9 RECORD (v5 kernel-v5-reconcile): toggles the felt-flight
        // recorder. Start: reset (drop any un-flushed prior capture) and set
        // the flag; step_frame below wires the per-tick hook only while
        // f9_recording, so a stopped recorder costs nothing (nullptr hook,
        // strict superset, AT-9 untouched). Stop: flush to the next free
        // felt_flight_<n>.seadsrec and arm the HUD "SAVED ..." fade.
        if (smoke_frames == 0 && IsKeyPressed(KEY_F9)) {
            if (!f9_recording) {
                felt_recorder.reset();
                f9_recording = true;
            } else {
                f9_recording = false;
                const std::string name = next_felt_flight_name();
                felt_recorder.flush(name, kFeltFlightVersionTag);
                f9_saved_at_s = GetTime();
                std::snprintf(f9_saved_name, sizeof f9_saved_name, "%s",
                              name.c_str());
            }
        }

        // ★★★ L3 -- THE SPAWN MENU, ANSWERED BEFORE ANYTHING ELSE IN THE
        // FRAME. While it is open the game is PAUSED (step_frame is fed a zero
        // dt below, so no tick runs and nothing ages) and the diegetic keys are
        // skipped, so the only thing a press can mean is the answer.
        if (spawn_menu.open) {
            const int pick = app::spawn_menu_poll(spawn_menu);
            if (pick >= 0 && spawn_menu.stage == app::kSpawnStageTeam) {
                // ★★★ L11 -- THE SIDE. Two writes, both before a single tick
                // has run (the game is paused under the menu): the COLOUR
                // table every draw site reads, and the FACTION the conquest
                // state already had a field for. The row int IS the faction
                // int by construction (render/team_kit.h PlayerTeam ==
                // combat::CqFaction), so there is no third mapping to keep in
                // step. Valley re-writes the shipped values and changes
                // nothing.
                spawn_menu.team = pick;
                render::set_player_team(pick == app::kSpawnMenuTeamCentralCity
                                            ? render::kTeamCentralCity
                                            : render::kTeamValley);
                cq.state.player_faction =
                    (pick == app::kSpawnMenuTeamCentralCity)
                        ? combat::CQ_SUDBURY
                        : combat::CQ_VALLEY;
                // The ride stage's dead row is gated on an own surface pump
                // being down -- and WHICH pumps are "his" just changed, so the
                // offer is recomputed rather than carried over from the arming
                // above.
                spawn_menu.machine_offered =
                    conquest_on && app::own_surface_pump_to_fix(cq.state) >= 0 &&
                    snow_field.hf != nullptr;
                // ★★★ L11b -- AND HE IS BORN AT HIS OWN SIDE'S HOME (Chad
                // 2026-09-10: "I picked central city but my spawn was over the
                // valley"). The aeroplane was placed three thousand lines up,
                // before a window existed, at the CONFIG side's home; the side
                // he just picked may not be that side. So the spawn spec is
                // re-anchored and the aircraft re-spawned right here -- the
                // same move the Snowmachine answer below makes, and safe for
                // the same reason: the game is PAUSED under this menu and not
                // one tick has run, so nothing is being teleported mid-flight.
                // Picking the config side re-writes identical values.
                if (game.tunnel.enabled) {
                    loop.spawn_up = app::faction_home_dir(
                        cq.state.player_faction);
                    loop.spawn_fwd = app::faction_home_fwd(
                        cq.state.player_faction);
                    loop.curr = app::player_spawn(
                        app::PlayerSpawnChoice::Aircraft, params,
                        loop.spawn_alt, loop.spawn_up, loop.spawn_fwd);
                    loop.prev = loop.curr;
                    loop.prev_up = sim::local_up(loop.curr.position);
                    loop.aim.reseed(loop.curr.orientation, loop.prev_up);
                    loop.grounded = true;  // still a spawn tick (SPEC 9.5)
                }
                // ★★★ L12 -- AND THE ROSTER IS RE-DEALT SO HE IS ALWAYS
                // OUTNUMBERED (Chad 2026-09-11: "whichever faction the player
                // chooses ... he needs to have the numbers disadvantage
                // against ai"). combat::maverick_faction keys the 7/3 split to
                // the PLAYER'S side, so the ten drones -- placed around their
                // faction ellipses three thousand lines up, before a window
                // existed -- must be re-placed now that the side is known.
                // Same paused-under-the-menu safety as the aeroplane above.
                if (conquest_on) place_conquest_fleet(cq.state.player_faction);
                spawn_menu.stage = app::kSpawnStageRide;
                spawn_menu.hover = -1;
            } else if (pick >= 0) {
                if (pick == app::kSpawnMenuSnowmachine) spawn_on_machine();
                spawn_menu.open = false;
                spawn_menu.first_spawn = false;
                // Give the cursor back to whatever the input mode wants: the
                // instructor captures it for relative mouse-aim, raw mode
                // shows it for the absolute virtual stick.
                if (smoke_frames == 0) {
                    if (raw_mode)
                        EnableCursor();
                    else
                        DisableCursor();
                    // Enable/DisableCursor WARPS the cursor, and the next
                    // poll_live delta is the warp rather than the hand -- the
                    // same spike the F1 mode toggle arms this flag for.
                    refocus_drop = true;
                }
            }
        }

        // Sample devices ONCE per frame (SPEC §10). Focus loss / smoke fall
        // back to neutral input (SPEC §9.5 robustness: an alt-tab mid-pull must
        // not hold deflection forever).
        const bool live_focus = smoke_frames == 0 && IsWindowFocused();
        sim::Inputs raw_in;
        input::LiveInput live;
        if (smoke_frames > 0) {
            live.throttle = 1.0f;
            raw_in.throttle = 1.0f;
        } else if (!live_focus) {
            live.throttle = static_cast<float>(live_dev.throttle_target);
            raw_in.throttle = static_cast<float>(live_dev.throttle_target);
            app::instructor_focus_loss(loop);  // drop freelook, aim := nose
            pending_dx = pending_dy = 0.0;
            orient_tap.reset();  // an alt-tab can't complete a stale double-tap
            prev_freelook = false;
            pending_orient =
                false;  // and can't carry an undelivered fire across
        } else if (raw_mode) {
            raw_in = input::poll_raw(raw_dev, clamped_dt, thumb_binds);
        } else {
            live = input::poll_live(live_dev, clamped_dt, thumb_binds);
            // First focused frame after a focus loss: GetMouseDelta can carry
            // a large cursor-warp delta (DisableCursor recenter) — a bounded
            // nuisance under the linear gain, but the MB-aim curve would
            // amplify it by up to gain_max. Drop it (plan red-team P2-4b; the
            // focus-loss branch above already zeroes pending).
            if (refocus_drop) live.mouse_dx = live.mouse_dy = 0.0;
            // MB-aim rate-keyed acceleration curve (input/aim_curve.h), at
            // the ONE site where the per-frame delta and its TRUE window
            // coexist. RAW frame_dt, NEVER clamped_dt (plan red-team P1-1:
            // the sim-stall clamp under-reports wall time and would turn a
            // hitch of slow tracking into a spurious flick).
            const glm::dvec2 curved = input::aim_curve(
                live.mouse_dx, live.mouse_dy, frame_dt, cparams);
            pending_dx += curved.x;
            pending_dy += curved.y;
        }
        refocus_drop = !live_focus;

        // Advance the fixed-dt accumulator and run its whole ticks through the
        // shared app::step_frame (test-pinned by AT-9 — the plant never sees
        // frame_dt). The per-frame mouse delta is accrued above and consumed on
        // the one tick-bearing offer inside; the crash-reset neutralization of
        // the RESOLVED input lives inside the seam (a mid-frame respawn flies
        // the rest of the frame on a dead stick, F2/P0-2).
        // MB-flaps: map the 3-position device LATCH -> the commanded fraction
        // (0 / combat_frac / 1, config-sourced), for BOTH modes. Read from
        // the persistent device state (a latch survives focus loss like
        // throttle_target — a default per-frame sample would silently retract
        // deployed flaps on alt-tab).
        const auto flap_frac = [&](int pos) {
            return pos == 0 ? 0.0 : (pos == 1 ? params.flap_combat : 1.0);
        };
        raw_in.flap_cmd = static_cast<float>(flap_frac(raw_dev.flap_pos));
        raw_in.gear_cmd = (raw_dev.gear_down || smoke_gear) ? 1.0f : 0.0f;

        app::FrameInput fin;
        fin.raw_mode = raw_mode;
        fin.raw_in = raw_in;
        fin.throttle = live.throttle;
        fin.flap_cmd = flap_frac(live_dev.flap_pos);
        fin.gear_cmd = (live_dev.gear_down || smoke_gear) ? 1.0 : 0.0;
        fin.wheel_brake = live.wheel_brake;  // R4g held brake (B)
        fin.freelook_held = live.freelook_held;
        fin.fire_held = live.fire_held || smoke_fire;  // LMB + smoke hook
        // S-orient (docs/comfort program Q3): build the freelook PRESS-EDGE
        // from the same device sample freelook reads, feed the pure OrientTap
        // detector on WALL time (frame_dt — the gap between two physical taps
        // is real seconds, not sim ticks; the window is a human-tapping
        // cadence), and offer the fire to the tick. Raw mode / focus loss never
        // taps: the detector is reset on focus loss below, and raw frames poll
        // no live freelook (prev_freelook holds). Off-switch:
        // orient_double_tap_s = 0 => step() short-circuits and never fires
        // (bit-identical).
        const bool freelook_edge =
            !raw_mode && live.freelook_held && !prev_freelook;
        // P0-1: OR the detection into the caller-side latch (the fire may be
        // detected on a 0-tick frame; step_frame only DELIVERS it on a tick).
        // Cleared below iff the frame ran ticks — the pending_dx/dy pattern.
        pending_orient =
            pending_orient || orient_tap.step(freelook_edge, frame_dt,
                                              cparams.orient_double_tap_s);
        fin.orient_cmd = pending_orient;
        prev_freelook = !raw_mode && live.freelook_held;
        for (int i = 0; i < 3; ++i) {
            fin.override_mask[i] = live.override_mask[i];
            fin.override_sign[i] = live.override_sign[i];
        }
        // Vestigial (S7-raw): the mouse is the RAW carried aim frame (§9.1) and
        // app::tick ignores cam_fwd/cam_up. Forwarded only so the
        // FrameInput->TickInput plumbing stays uniform; the camera-render path
        // below uses the live cam_fwd/cam_up directly, not these.
        fin.cam_fwd = cam_fwd;
        fin.cam_up = cam_up;
        // RMB-zoom mouse-gain scale: a zoomed (magnified) view aims
        // proportionally slower for precision. BINARY on the button state, NOT
        // the eased zoom_t — a pure input function stays frame-rate independent
        // (an fov-eased gain would be the AT-9 frame-quantized aim divergence).
        // live.zoom_held is false in raw/focus-loss/smoke (default LiveInput).
        fin.aim_gain_scale = (!raw_mode && live.zoom_held)
                                 ? render::kZoomFovyDeg / render::kChaseFovyDeg
                                 : 1.0;
        // ★ WINTER S3: while DRIVING, the rider's hands are on the handlebars,
        // so the aircraft gets a dead stick and idle throttle. It is still
        // ticked -- a landed machine simply sits there -- which is what keeps
        // app::step_frame unmodified and the flight goldens untouched.
        if (player.off_aircraft()) {
            fin.raw_in = sim::Inputs{};
            fin.throttle = 0.0;
            fin.wheel_brake = 1.0f;  // parked, not rolling away behind you
            fin.fire_held = false;
            // ★ §8 P1-4: clear the MASK, not just the sign -- the controller
            // asserts |sign|==1 under an armed mask, and Q/E writing the
            // aircraft's yaw override while ALSO leaning the rider was the
            // red team's one blocker in the control map.
            for (int i = 0; i < 3; ++i) fin.override_mask[i] = false;
            // ★ §8 P1-5: the curved mouse deltas belong to the RIDER'S WEIGHT
            // now. Consume them into the sled lean accumulators (px scale per
            // §9d.4: the aircraft's own full-scale in its own unit), then zero
            // them so leaning does not also fly the parked aircraft's stick.
            // EXCEPT under freelook: SPACE lends the mouse to the camera --
            // the weight HOLDS, the deltas pass through untouched.
            // ★★ DRIVE-2 FIX (Chad: "mouse is not moving with my mouse. I
            // cannot shift my weight"). The old path consumed pending_dx/dy
            // -- the AIRCRAFT AIM deltas, downstream of input::aim_curve's
            // rate-keyed acceleration. Weight shift is a POSITION, not an
            // aim vector: a slow deliberate lean got crushed by the curve's
            // low-rate gain and a full-scale of 120/aim_sensitivity ~ 860 px,
            // so the dot barely moved. Read the RAW device deltas instead,
            // with the weight's own full-scale. Mouse is PRIMARY; Q/E keys
            // stay as backup (his ruling).
            // ★ STING: while the launcher is shouldered or the drone is
            // flying, the mouse belongs to the STING -- the shoulder aim or
            // the flight aim -- and never to the rider's weight. The flight
            // deltas ACCRUE into cmd (the pending_dx pattern: consumed and
            // zeroed by sting_step on the frame's first tick); the shoulder
            // aim is app-side view state, applied directly. W/S resolve the
            // flight speed command here too, beside the mouse they steer with.
            if (player.mode == app::PlayerMode::Drone) {
                // SPACE lends the mouse to the orbit camera; the aim HOLDS
                // (the plane's freelook rule 1 — the drone keeps chasing the
                // parked ring). Otherwise the mouse is the aim's.
                // The v13 rest-edge horizon law inside sting_step cancels
                // while the orbit camera owns the mouse (the plane's arm).
                sting.cmd.freelook = live.freelook_held;
                if (live.freelook_held) {
                    sting_cam_az += live.mouse_dx * 0.006;
                    sting_cam_el = std::clamp(
                        sting_cam_el + live.mouse_dy * 0.004, -1.2, 1.2);
                } else {
                    sting.cmd.dx += live.mouse_dx;
                    sting.cmd.dy += live.mouse_dy;
                }
                // fly-3: the WHEEL zooms the camera out/in (any time on the
                // drone, freelook or not) — geometric steps, clamped so the
                // little machine never leaves its own frame. The M-key map
                // keeps the wheel while open (its existing claim).
                if (!map_open && live.wheel != 0.0) {
                    sting_cam_dist = std::clamp(
                        sting_cam_dist * std::pow(1.18, -live.wheel), 3.0,
                        60.0);
                }
                sting.cmd.speed_cmd = IsKeyDown(KEY_W)   ? sting.sp.speed_max
                                      : IsKeyDown(KEY_S) ? sting.sp.speed_min
                                                         : sting.sp.speed_cruise;
            } else if (sting_shouldered) {
                // ★★★ POLISH FLY-2 -- FREE LOOK PULLS OUT TO THIRD PERSON.
                // Chad, verbatim: "that doesn't work when the rpas is
                // deployed ... I am looking right into the body of the
                // sudburian. It should be automatically a wider 3rd person
                // view just a few feet back when I press free look".
                //
                // SPACE-HELD, mirroring the sled and the flak gun exactly
                // (input/live_input.cpp:41 -- freelook is IsKeyDown(SPACE),
                // a hold and never a toggle, everywhere in this game). Held =
                // the mouse is lent to the ORBIT and the launcher's aim holds
                // where he left it (the flak gunner's own free-look law, and
                // RA9: freelook drives the orbit, never mouse->aim).
                // Released = the orbit eases back behind him at 6/s, the same
                // ease the sled and drone cameras use, and the mouse is the
                // aim again.
                if (live.freelook_held) {
                    constexpr double kStingLookElMax = 1.20;
                    constexpr double kStingLookElMin = -0.55;
                    sting_look_az += live.mouse_dx * 0.006;
                    sting_look_el =
                        std::clamp(sting_look_el + live.mouse_dy * 0.004,
                                   kStingLookElMin, kStingLookElMax);
                } else {
                    sting_aim_az += live.mouse_dx * 0.004;
                    sting_aim_el = std::clamp(
                        sting_aim_el - live.mouse_dy * 0.004, -0.35, 1.40);
                    const double ease = std::min(1.0, 6.0 * clamped_dt);
                    sting_look_az -= sting_look_az * ease;
                    sting_look_el -= sting_look_el * ease;
                }
                // THE WHEEL, and it zooms whether or not SPACE is down --
                // his sentence is "I needed to zoom then too", not "while
                // holding space". 1.2 m (the stock at arm's length) to 8 m.
                //
                // ⚠ CONSUMED HERE, and that is what keeps the sled chase
                // camera's own wheel handler (further down the frame) from
                // ALSO eating this notch: while he is shouldering the
                // launcher he is still standing on/beside a seeded machine,
                // so both blocks are live in the same frame. The map's claim
                // still outranks both -- and unlike the sled handler this one
                // runs BEFORE the map block, so the `!map_open` test is doing
                // real work here rather than restating an upstream guarantee
                // (the sting FLIGHT cam's wheel, a few lines up, is early for
                // the same reason and carries the same test).
                if (!map_open && live.wheel != 0.0) {
                    sting_look_dist = std::clamp(
                        sting_look_dist * std::pow(1.18, -live.wheel), 1.2,
                        8.0);
                    live.wheel = 0.0;
                }
            } else if (!live.freelook_held) {
                const double px_full = 300.0;  // full lean ~ a palm swipe
                // mouse RIGHT (+dx) leans RIGHT = -lean_lat (+1 = LEFT);
                // mouse FORWARD (-dy) leans FORWARD = +lean_fwd
                // *** SK-1d, Chad 2026-08-25: "I dont like the steering via
                // mouse coupled with lean they have to be two independent
                // things, part of what is so fun on the road is driving by
                // lean."  Mouse X is LEAN again, at its original 300 px --
                // SK-1c had taken it for steer. Steering keeps SK-1c's analog
                // accumulator but lives on A/D alone. NOTHING couples them.
                sled_lean_lat = std::clamp(
                    sled_lean_lat - live.mouse_dx / px_full, -1.0, 1.0);
                sled_lean_fwd = std::clamp(
                    sled_lean_fwd - live.mouse_dy / px_full, -1.0, 1.0);
            }
            // The aircraft stick stays dead either way.
            pending_dx = 0.0;
            pending_dy = 0.0;
            pending_orient = false;  // no aim vector to snap to on a sled
        }
        // ★ L3: PAUSED UNDER THE MENU. A zero dt accumulates no ticks, so
        // fr.ticks is 0 and every tick-gated consumer below is already correct
        // by its own existing guard -- no second "is it paused" test anywhere.
        // The pending mouse is dropped so the aim does not jump when the menu
        // closes on a click.
        if (spawn_menu.open) {
            pending_dx = 0.0;
            pending_dy = 0.0;
        }
        // ── FLAK STAGE D VISUAL CERTIFICATION (SMOKE ONLY, env-gated -- the
        // SEADS_SMOKE_GEAR / SEADS_FLAK_MANNED pattern). The green gate is
        // blind to seads.exe, and an AI gun only fires when a raider of the
        // OPPOSING side happens to be inside 1500 m of it, which no fixed
        // frame count can promise. SEADS_FLAK_AI_TARGET="<gun_index>" parks
        // the first opposing-faction drone on a slow circular orbit 600 m out
        // and 320 m above that gun, so the flash, the tracer stream and the
        // curtain can be photographed. NOT A SHIPPED PATH: gated on
        // smoke_frames > 0, it writes only drone kinematics, and it is dead
        // in every interactive run.
        if (smoke_frames > 0 && !flak_ai.empty()) {
            if (const char* fa = std::getenv("SEADS_FLAK_AI_TARGET")) {
                int gi = std::atoi(fa);
                if (gi < 0) gi = 0;
                if (gi >= static_cast<int>(flak_ai.size()))
                    gi = static_cast<int>(flak_ai.size()) - 1;
                const app::FlakAiGun& ag = flak_ai[gi];
                for (drone::DroneState& d : dw.drones) {
                    if (d.inert) continue;
                    if (combat::maverick_faction(
                            d.spawn_index, cq.state.player_faction) ==
                        ag.faction)
                        continue;
                    const double w = 0.20;  // rad/s -> ~120 m/s at 600 m
                    const double th = w * static_cast<double>(frames) *
                                      params.sim_dt;
                    const glm::dvec3 r =
                        ag.fk.mount.fwd0 * std::cos(th) +
                        ag.fk.mount.right0 * std::sin(th);
                    const glm::dvec3 tang =
                        -ag.fk.mount.fwd0 * std::sin(th) +
                        ag.fk.mount.right0 * std::cos(th);
                    d.prev = d.curr;
                    d.curr.position =
                        ag.fk.mount.pos + r * 600.0 + ag.fk.mount.up * 320.0;
                    d.curr.velocity = tang * (600.0 * w);
                    break;  // one aeroplane is the shot
                }
            }
        }
        g_feel_tape.rec = f9_recording ? &felt_recorder : nullptr;
        const app::FrameResult fr = app::step_frame(
            loop, accum, spawn_menu.open ? 0.0 : frame_dt, fin, pending_dx,
            pending_dy, params, cparams,
            env_ptr, &dw, &gw, &cw,
            game.tunnel.enabled ? &tunnel_lamps : nullptr,
            // S-lapguard FEEL TAPE: when the tape is open it takes the
            // single per-tick hook slot and CHAINS the felt recorder, so
            // enabling the tape can never silently disable F9.
            (g_feel_tape.f != nullptr)
                ? app::feel_tape_hook
                : (f9_recording ? felt_recorder_hook : nullptr),
            (g_feel_tape.f != nullptr)
                ? static_cast<void*>(&g_feel_tape)
                : (f9_recording ? static_cast<void*>(&felt_recorder)
                                : nullptr),
            conquest_on ? &cq : nullptr,  // CONQUEST wiring (null off-arm)
            conquest_on ? conquest_tape_hook : nullptr,
            conquest_on ? static_cast<void*>(conquest_tape.get()) : nullptr,
            &flak_fk,  // FLAK: always threaded -- unmanned + empty = idle
            // STAGE D AI gunners -- RULED OFF 2026-08-30 (player-only guns,
            // app::kFlakAiOperate); the vector stays built for the smoke rig.
            (app::kFlakAiOperate && !flak_ai.empty()) ? &flak_ai : nullptr,
            &sting);  // STING RPAS: always threaded -- inactive = idle
        // CONQUEST TAPE: periodic flush every 120 ticks (1 s sim, spec §9
        // P1-3 — was 1200/10 s) — a hard kill (window close mid-fight) now
        // loses at most 1 s of tape. SAMPLING is tick-driven (inside
        // ConquestTape::on_tick); this is only the app's own file-write
        // cadence, which may ride frame boundaries.
        // ★★ SC1 COLD IS POWER (WINTER_LAW §3.7, SC1_COLD_SPEC.md §2/§3). ONE
        // clock: the SAME TICK-DERIVED t_cel formula Stage 3a uses below
        // (never wall time), wheeled through the pure celestial core, feeds
        // world::air_temp_c. Computed HERE -- before the sled ticks a few
        // lines down -- so sled_params carries THIS tick's temperature into
        // step_sled (spec §3: "App writes them once per tick before
        // step_sled"); the render-side sun_world/night-art computation
        // further down (Stage 3a) recomputes the same pure formula against
        // that tick's (possibly debug-scrubbed) t_cel, which is why this is a
        // second EVALUATION of one clock, never a second clock. cel_time_offset
        // has not been touched by this tick's debug scrub yet (that lives
        // below, gated `smoke_frames == 0`, interactive-only) -- a harmless,
        // declared one-frame skew against the render-side value while a
        // scrub key is actively held (spec §2: "the ]/[ scrub therefore
        // scrubs temperature too... a DECLARED debug affordance").
        {
            const double t_cel_now =
                static_cast<double>(loop.tick_count) * params.sim_dt +
                cel_time_offset;
            // "Up at the PLAYER" (spec §2), not the camera eye the old
            // draw.cpp local computation used: loop.curr.position is the
            // aircraft's own state, fresh after step_frame. (When driving,
            // the sled sits a few metres away — negligible at planetary
            // sun-elevation scale.)
            const glm::dvec3 cold_up = glm::normalize(loop.curr.position);
            const glm::dvec3 cold_sun_world = -render::sun_dir(cel, t_cel_now);
            const double sun_sin_elev = glm::dot(cold_sun_world, cold_up);
            // ★ night_phase: a PHASE OF THE SUN computed from celestial time
            // ALONE (no player position, spec §2), derived analytically --
            // no iteration, no accumulating timer state. render/celestial.cpp:
            // sun_dir's ecliptic angle is theta = phi_year + omega_year*t; the
            // daily wheel angle is Omega = phi_day + omega_day*t; and by the
            // sidereal-wheel construction (omega_day = omega_year - 2pi/day)
            // their difference psi = theta - Omega evolves at EXACTLY
            // 2*pi/day_period_s, independent of year length. At the fixed
            // reference ground point equator_x (the ground is inertially
            // fixed, so this is a valid, fixed "ground point" on the
            // equator), the sun's elevation there works out to
            // cos(theta)*cos(Omega) + cos(tilt)*sin(theta)*sin(Omega) --
            // which is cos(psi) exactly at tilt = 0 and cos(psi) to a small,
            // named cos(tilt) correction otherwise (dropped here, like the
            // breathing term: real but small — tilt ~20-25 deg puts the
            // dropped term's peak error at 1-cos(tilt) ~ 0.04-0.08 of
            // amplitude). cos(psi) = 0 at psi = +/- pi/2: psi=pi/2 is
            // "sunset" (elevation falling through zero), psi=3pi/2 (or
            // -pi/2) is "sunrise", psi=pi is "solar midnight" -- exactly the
            // spec's 0/0.5/1 anchors.
            double night_phase = 0.0;
            {
                constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
                constexpr double kPi = 3.14159265358979323846;
                const double psi = (cel.phi_year - cel.phi_day) +
                                   (kTwoPi / cel.day_period_s) * t_cel_now;
                double raw = (psi - kPi / 2.0) / kPi;  // period-2 in raw
                raw -= 2.0 * std::floor(raw / 2.0);    // wrap to [0,2)
                // [0,1) is the dark span (0=sunset..1=sunrise); [1,2) folds
                // the daylight span back onto the same numeric range -- inert
                // because the day/night blend in world::air_temp_c weights
                // the night term to ~0 whenever sun_sin_elev is high (spec
                // §3's day component dominates there).
                night_phase = raw < 1.0 ? raw : (raw - 1.0);
            }
            const double air_t =
                world::air_temp_c(sun_sin_elev, night_phase, world.cold);
            sled_params.air_temp_c = air_t;
            sled_params.snow_hardness = world::hardness(air_t, world.cold);
            sled_params.cold_t_ref_c = world.cold.t_ref_c;
        }
        // ★★ WINTER S3 THE SLED TICK. Same fixed-tick budget the flight kernel
        // just consumed (fr.ticks -- never frame_dt; the plant never sees wall
        // time, SPEC §10), stepped through sim::step_sled, which reads the ONE
        // ground through world::SnowpackField. G toggles; on the first entry
        // the machine is placed on the drive surface under the aircraft, on
        // the aircraft's heading.
        // SEADS_SLED_DEBUG_MODE=1 arms drive mode from frame 1 without a
        // keypress, so the smoke harness can EXERCISE the drive path
        // headlessly. Worth the four lines: without it the first thing that
        // ever runs this code is Chad pressing G, and a crash there costs a
        // whole fly cycle.
        const bool sled_env_arm =
            !player.off_aircraft() && smoke_frames > 0 &&
            loop.tick_count > 30 &&
            std::getenv("SEADS_SLED_DEBUG_MODE") != nullptr;
        // ── FLAK F-FIRE/F-SIGHT (docs/FLAK_GUN_SPEC.md §7.4/§7.5). While
        // manned, the DEMAND pose takes the RAW per-frame mouse (§9.1:
        // nothing smoothed on the hand -- the gun's slew cap is the MASS) and
        // LMB is the trigger. The flight kernel is untouched: the
        // aircraft/sled sit parked the whole time.
        //
        // ★★★ L5 -- THE O KEY IS NO LONGER DECIDED HERE. Manning the gun is a
        // MODE TRANSITION now (app/player_mode.h) and it is gated on the man's
        // position, so it has to be decided where the interact-site table
        // exists -- which is after this block. What was here was labelled
        // SCAFFOLDING of the KEY_J class ("the sled's own stopped-and-near
        // rule, not embodiment, awaiting an on-foot state"), and the on-foot
        // state now exists, so it is DELETED rather than kept as a fallback:
        // the 30 m from-the-saddle rule was the thing L5 replaces.
        //
        // `flak_man` is hoisted out of the block so the one place that mans a
        // gun can be called from where the decision is made. It is still the
        // ONLY writer of `flak_fk.manned = true`.
        const auto flak_man = [&](int idx) {
            const render::FlakDraw& fgm = flak_guns[idx];
            flak_fk.manned = true;
            flak_fk.gun = idx;
            flak_fk.mount =
                render::flak::make_mount_frame(fgm.pos, fgm.up, fgm.fwd0);
            flak_fk.demand = flak_fk.pose;  // no snap on mount
            // The station table off the LOADED GLB -- never retyped.
            // flak_rebuild_shooter carries st_muzzle through the CRADLE
            // kinematics every tick, so the round leaves the bell at every
            // elevation (2026-08-31: a zero-elevation offset spat the tracers
            // out of the breech).
            render::flak::Stations fst;
            if (render::flak_stations(fst)) flak_fk.stations = fst;
        };
        // ★★★ L5 -- AND THE ONE PLACE THAT TAKES HIM OFF IT, for the same
        // reason: leaving the gun is two writes plus putting the man down at
        // the approach mark, and a second copy of that is how the r4a lane's
        // stranded-man bug was born (app/player_mount.h's banner). The caller
        // hands in where he lands, because only it owns a ground query.
        const auto flak_unman = [&](const glm::dvec3& stand_pos,
                                    const glm::dvec3& face_dir) {
            flak_fk.manned = false;
            flak_fk.fire_held = false;
            app::walker_place_afoot(walker, stand_pos, face_dir);
            gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
        };
        {
            // Smoke-only (the SEADS_FLAK_MANNED class): SEADS_STING_POSE_SMOKE
            // seeds the machine on the ground below the spawn, sits the man on
            // it (the same seed/mount/force seam spawn_on_machine uses) and
            // holds the launcher SHOULDERED from tick 40 -- so the seat-deploy
            // pose can be certified headlessly. Pair with SEADS_STING_DEPLOY
            // ="t" to pin the blend at any frame of the draw and
            // `--smoke N shot.png` for the picture. Optional value syntax
            // "yaw_deg,cam_az_deg,cam_el_deg,aim_az_deg": rotates the machine
            // so the shot can look at his right side where the sweep happens,
            // pins the orbit, and (ST-5's sweep) pins the aim azimuth the
            // head and torso twist onto.
            static bool sting_pose_smoke_done = false;
            const char* sting_pose_env = std::getenv("SEADS_STING_POSE_SMOKE");
            if (sting_pose_env != nullptr && smoke_frames > 0 &&
                loop.tick_count > 40 && snow_field.hf != nullptr &&
                !sting_pose_smoke_done) {
                sting_pose_smoke_done = true;
                const glm::dvec3 dir = glm::normalize(loop.curr.position);
                const glm::dvec3 spos =
                    dir * (snow_field.drive_radius_at(dir) +
                           sled_params.cg_height_m + 0.05);
                glm::dvec3 east = glm::cross(glm::dvec3(0.0, 1.0, 0.0), dir);
                if (glm::length(east) < 1e-6) east = glm::dvec3(1.0, 0.0, 0.0);
                east = glm::normalize(east - glm::dot(east, dir) * dir);
                const double yaw = std::atof(sting_pose_env) * (PI / 180.0);
                const glm::dvec3 fwd =
                    glm::normalize(east * std::cos(yaw) +
                                   glm::cross(dir, east) * std::sin(yaw));
                app::seed_sled_at(sled, spos, dir, fwd);
                sled_prev = sled;
                render::sled_plumes_reset(sled_plumes);
                app::player_mount_request(sled, walker);
                ++sled_epoch;
                app::player_mode_force(player, app::PlayerMode::Sled);
                player.sled_seeded = true;
            }
            if (sting_pose_smoke_done) {
                sting_shouldered = true;
                // Optional 2nd/3rd values "yaw,cam_az,cam_el" (degrees): pin
                // the freelook orbit so the shot can look at his RIGHT side,
                // where the whole sweep happens (the chase cam follows the
                // machine, so yawing the machine alone never changes the
                // view). Re-pinned every frame; the ease-back downstream
                // shaves <10% once, which a screenshot does not care about.
                double y_ = 0.0, caz = 0.0, cel = 0.0, aaz = 0.0;
                const int nf = std::sscanf(sting_pose_env, "%lf,%lf,%lf,%lf",
                                           &y_, &caz, &cel, &aaz);
                if (caz != 0.0 || cel != 0.0) {
                    sled_cam_az = caz * (PI / 180.0);
                    sled_cam_el = cel * (PI / 180.0);
                }
                // ★★★ ST-5 THE SWEEP: optional 4TH value "aim_az_deg" pins
                // the aim azimuth so the head/torso twist can be certified at
                // 0 / 60 / 90 / -90 headlessly. Pinned RAW and re-pinned every
                // frame -- the seated 180 clamp runs downstream in the same
                // frame, which is what lets a value like 170 PROVE the clamp
                // bites (the shot must be pose-identical to 90). Smoke-only,
                // the SEADS_FLAK_POSE class: it moves the drawn aim, never a
                // launched missile (`smoke_frames > 0` gates the whole block
                // and the launch click is off in smoke).
                if (nf >= 4) sting_aim_az = aaz * (PI / 180.0);
            }
        }
        {
            // ★ PUMP WALK-IN PROBE (smoke-only, the SEADS_STING_POSE_SMOKE
            // class). SEADS_PUMPWALK_SMOKE="<start_m>": at tick 40 damages the
            // player's own surface pump, seeds the machine start_m east of the
            // pump's FOOT on the drive surface, sits the man on it, then puts
            // him OFF it on his feet facing the pump -- the real R5 sequence.
            // The tick loop then walks him straight at the pump and the frame
            // logs the ground distance, his height over DEM/drive, the
            // terrain rise toward the pump and the live prompt. It answers
            // "where does the 6 m floor come from" at the REAL placement.
            const char* pumpwalk_env = std::getenv("SEADS_PUMPWALK_SMOKE");
            if (pumpwalk_env != nullptr && smoke_frames > 0 && conquest_on &&
                loop.tick_count > 40 && snow_field.hf != nullptr &&
                env.ground != nullptr && !pumpwalk_done) {
                pumpwalk_done = true;
                const int pi = app::own_surface_pump(cq.state);
                if (pi >= 0) {
                    pumpwalk_pump = pi;
                    const combat::Pump& pu0 = cq.state.pumps[pi];
                    combat::damage_pump(cq.state, pi, pu0.max_hp * 0.5,
                                        pu0.faction == combat::CQ_VALLEY
                                            ? combat::CQ_SUDBURY
                                            : combat::CQ_VALLEY,
                                        cq.params);
                    const glm::dvec3 pdir = glm::normalize(pu0.pos);
                    glm::dvec3 east =
                        glm::cross(glm::dvec3(0.0, 1.0, 0.0), pdir);
                    if (glm::length(east) < 1e-6) east = glm::dvec3(1, 0, 0);
                    east = glm::normalize(east - glm::dot(east, pdir) * pdir);
                    double start = 0.0, bearing_deg = 0.0;
                    std::sscanf(pumpwalk_env, "%lf,%lf", &start, &bearing_deg);
                    if (start <= 0.0) start = 40.0;
                    {
                        // optional 2nd value: bearing about the pump's up
                        const double br = bearing_deg * (PI / 180.0);
                        const glm::dvec3 north = glm::cross(pdir, east);
                        east = glm::normalize(east * std::cos(br) +
                                              north * std::sin(br));
                    }
                    const glm::dvec3 sdir = glm::normalize(
                        pdir * glm::length(pu0.pos) + east * start);
                    const glm::dvec3 spos =
                        sdir * (snow_field.drive_radius_at(sdir) +
                                sled_params.cg_height_m + 0.05);
                    app::seed_sled_at(sled, spos, sdir, -east);
                    sled_prev = sled;
                    render::sled_plumes_reset(sled_plumes);
                    app::player_mount_request(sled, walker);
                    ++sled_epoch;
                    player.sled_seeded = true;
                    // ... and off it, on his feet, 2 m toward the pump.
                    const glm::dvec3 fdir = glm::normalize(
                        sdir * glm::length(pu0.pos) - east * 2.0);
                    const glm::dvec3 fpos =
                        fdir * (snow_field.drive_radius_at(fdir) +
                                walker_params.lie_clearance_m);
                    app::walker_place_afoot(walker, fpos, -east);
                    app::player_mode_force(player, app::PlayerMode::Afoot);
                    std::fprintf(stderr,
                                 "PUMPWALK seeded pump=%d start=%.1f m\n", pi,
                                 start);
                } else {
                    std::fprintf(stderr, "PUMPWALK no own surface pump\n");
                }
            }
            // ★ L10 ENGINE WALK-IN PROBE. SEADS_ENGINEWALK_SMOKE="<start_m>[,
            // bearing_deg]": at tick 40 it BREAKS THE ENGINE the way a prop
            // strike does (a flat 0.0 -- app/instructor_tick.h), puts the man
            // off on his feet `start_m` from the parked aeroplane, and lets
            // the walk-in below carry him to it. It presses U itself when the
            // site lights, so the whole sequence Chad described -- walk up,
            // one button, the wrench turns -- runs with no window.
            //
            // ⚠ NO CONQUEST NEEDED, unlike PUMPWALK. That asymmetry is the
            // point: an engine breaks in free flight too, and a rig that could
            // only run inside a match would not exercise the mode the fix most
            // often matters in.
            const char* enginewalk_env = std::getenv("SEADS_ENGINEWALK_SMOKE");
            if (enginewalk_env != nullptr && smoke_frames > 0 &&
                loop.tick_count > 40 && snow_field.hf != nullptr &&
                env.ground != nullptr && !enginewalk_done) {
                enginewalk_done = true;
                cw.damage.engine = 0.0;  // what a prop strike writes
                cw.player_hp = combat::summary_hp(cw.damage);
                // ★★ THE AEROPLANE IS PARKED FIRST, and this is the half the
                // first run of the rig got wrong: a --smoke run flies, so a
                // man walking at `loop.curr.position` chases a glider away
                // over the horizon and the site never lights. The fix that
                // matters is not "walk faster", it is that a man on foot only
                // ever meets a PARKED aeroplane -- he had to land and get out
                // of it. Parked by the SAME recipe app::player_spawn parks it
                // with (surface orientation, zero velocity, throttle shut,
                // gear down, on_ground true), so the rig certifies the state
                // the game actually produces.
                const glm::dvec3 adir = glm::normalize(loop.curr.position);
                {
                    const double air_r = (env.ground != nullptr)
                                             ? env.ground->radius_at(adir)
                                             : params.R;
                    glm::dvec3 afwd =
                        glm::cross(adir, glm::dvec3(0.0, 1.0, 0.0));
                    if (glm::length(afwd) < 1e-6) afwd = glm::dvec3(0, 0, -1);
                    afwd = glm::normalize(afwd);
                    loop.curr.position =
                        adir * (air_r + env.ground_params.contact_height_m);
                    loop.curr.orientation =
                        app::surface_orientation(adir, afwd);
                    loop.curr.velocity = glm::dvec3{0.0};
                    loop.curr.throttle = 0.0;
                    loop.curr.last_vhat =
                        loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
                    loop.curr.gear = 1.0;
                    loop.curr.on_ground = true;
                    loop.prev = loop.curr;
                    enginewalk_park = loop.curr;
                }
                glm::dvec3 east = glm::cross(glm::dvec3(0.0, 1.0, 0.0), adir);
                if (glm::length(east) < 1e-6) east = glm::dvec3(1, 0, 0);
                east = glm::normalize(east - glm::dot(east, adir) * adir);
                double start = 0.0, bearing_deg = 0.0;
                std::sscanf(enginewalk_env, "%lf,%lf", &start, &bearing_deg);
                if (start <= 0.0) start = 30.0;
                {
                    const double br = bearing_deg * (PI / 180.0);
                    const glm::dvec3 north = glm::cross(adir, east);
                    east = glm::normalize(east * std::cos(br) +
                                          north * std::sin(br));
                }
                // ★★ AND A MACHINE IS SEEDED, because the man cannot take a
                // step without one. MEASURED, not assumed: the whole sled +
                // walker + gait tick is gated on
                // `player.off_aircraft() && player.sled_seeded`, so the first
                // build of this rig stood him in the snow for four thousand
                // ticks without moving. That gate is CORRECT for the shipped
                // game -- `Afoot` is only reachable by stepping off a
                // snowmachine, so a man on his feet always has one -- and the
                // rig has to reproduce the real sequence rather than route
                // around it. Same seed/mount/step-off as SEADS_PUMPWALK_SMOKE.
                const glm::dvec3 sdir = glm::normalize(
                    adir * glm::length(loop.curr.position) + east * start);
                const glm::dvec3 spos =
                    sdir * (snow_field.drive_radius_at(sdir) +
                            sled_params.cg_height_m + 0.05);
                app::seed_sled_at(sled, spos, sdir, -east);
                sled_prev = sled;
                render::sled_plumes_reset(sled_plumes);
                app::player_mount_request(sled, walker);
                gait = sim::GaitState{};
                ++sled_epoch;
                player.sled_seeded = true;
                // ... and off it, on his feet, two metres toward the aeroplane.
                const glm::dvec3 fdir = glm::normalize(
                    sdir * glm::length(loop.curr.position) - east * 2.0);
                const glm::dvec3 fpos =
                    fdir * (snow_field.drive_radius_at(fdir) +
                            walker_params.lie_clearance_m);
                app::walker_place_afoot(walker, fpos, -east);
                app::player_mode_force(player, app::PlayerMode::Afoot);
                std::fprintf(stderr,
                             "ENGINEWALK seeded start=%.1f m engine=%.2f\n",
                             start, cw.damage.engine);
            }
        }
        {
            // Smoke-only (the SEADS_RIGCAM class): SEADS_FLAK_MANNED=1 forces
            // gun 0 manned + firing at a raised demand from tick 30, so the
            // sight view and the tracer river can be certified headlessly.
            const bool flak_smoke_manned =
                smoke_frames > 0 && !flak_guns.empty() &&
                loop.tick_count > 30 &&
                std::getenv("SEADS_FLAK_MANNED") != nullptr;
            if (flak_smoke_manned && !flak_fk.manned) {
                flak_man(0);
                // The value is the DEMANDED elevation in degrees (default
                // 18): the tracer river has to be certifiable at the high
                // angles too, because that is where the muzzle swings
                // furthest about the trunnion and where a spawn point
                // measured at zero elevation ends up beside the gunner's
                // ear instead of at the bell (2026-08-31). Unlike
                // SEADS_FLAK_POSE -- which moves only the DRAWN gun -- this
                // drives the real demand, so gun, flash and round agree.
                const double ed = std::atof(std::getenv("SEADS_FLAK_MANNED"));
                flak_fk.demand.elev_rad =
                    render::flak::clamp_elev((ed > 0.0 ? ed : 18.0) *
                                             (PI / 180.0));
            }
            if (flak_fk.manned) {
                // Raw per-frame mouse -> the demand pose. The rad-per-px is a
                // FEEL DIAL (code constant, the render/camera.h precedent).
                constexpr double kFlakAimRadPerPx = 0.0025;
                const Vector2 fmd = GetMouseDelta();
                if (live.freelook_held) {
                    // FREE LOOK: SPACE lends the mouse to the head -- the gun
                    // demand holds where it was. Mouse right = look right,
                    // mouse forward = look up (a head, not the ruled muzzle
                    // sign -- the sled orbit precedent).
                    flak_cam_az += static_cast<double>(fmd.x) * 0.006;
                    flak_cam_el = std::clamp(
                        flak_cam_el - static_cast<double>(fmd.y) * 0.004,
                        -0.9, 1.2);
                } else {
                    // ★ SIGN RULED BY CHAD 2026-08-28 ("right left is
                    // reversed"): mouse-right = muzzle-right on HIS hand,
                    // which is MINUS here. His word beats the derivation.
                    flak_fk.demand.train_rad = render::flak::wrap_pi(
                        flak_fk.demand.train_rad -
                        static_cast<double>(fmd.x) * kFlakAimRadPerPx);
                    flak_fk.demand.elev_rad = render::flak::clamp_elev(
                        flak_fk.demand.elev_rad -
                        static_cast<double>(fmd.y) * kFlakAimRadPerPx);
                    // Release eases the head back onto the sight (6/s, the
                    // sled ease).
                    const double ease = std::min(1.0, 6.0 * clamped_dt);
                    flak_cam_az -= flak_cam_az * ease;
                    flak_cam_el -= flak_cam_el * ease;
                }
                flak_fk.fire_held =
                    (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !raw_mode) ||
                    flak_smoke_manned;
                // Smoke rig (smoke-only): SEADS_FLAK_LOOK="az_deg,el_deg"
                // pins the free-look head angles for screenshot
                // certification.
                if (smoke_frames > 0) {
                    if (const char* e = std::getenv("SEADS_FLAK_LOOK")) {
                        double azd = 0.0, eld = 0.0;
                        std::sscanf(e, "%lf,%lf", &azd, &eld);
                        flak_cam_az = azd * (PI / 180.0);
                        flak_cam_el = eld * (PI / 180.0);
                    }
                }
                // F-POSE PULLOUT: free-look held eases the camera out to
                // the third-person view of the man; release eases it back
                // onto the sight. Cosmetic only -- the gun demand is
                // untouched by free-look either way (above).
                {
                    // Gated on the gunner kill switch as well as the key:
                    // the pullout exists ONLY to show the man, so with
                    // SEADS_FLAK_GUNNER=0 free-look stays the plain head
                    // turn it was before this rung and the kill switch's
                    // "bit-identical to the pre-rung build" still holds.
                    const double ext_tgt =
                        (live.freelook_held && render::flak_gunner_enabled())
                            ? 1.0
                            : 0.0;
                    flak_ext_t = render::blend_toward(
                        flak_ext_t, ext_tgt,
                        1.0 / render::flak::kExtEaseHz, clamped_dt);
                    // The exponential only ASYMPTOTES to 0, so without this
                    // the sight camera would sit a vanishing but non-zero
                    // offset off the axis forever after the first free-look.
                    // Snap the tail shut: below this the pullout is a
                    // sub-millimetre nudge no eye can see.
                    if (flak_ext_t < 1e-4) flak_ext_t = 0.0;
                    // Smoke rig (smoke-only, the SEADS_FLAK_LOOK class):
                    // SEADS_FLAK_EXT="t" pins the pullout blend so the
                    // third-person view is screenshot-certifiable
                    // headlessly (live.freelook_held is hard-false in a
                    // smoke run, so the path is otherwise unreachable).
                    if (smoke_frames > 0) {
                        if (const char* xe = std::getenv("SEADS_FLAK_EXT"))
                            flak_ext_t = std::clamp(std::atof(xe), 0.0, 1.0);
                    }
                }
            } else {
                flak_fk.fire_held = false;
                flak_cam_az = 0.0;
                flak_cam_el = 0.0;
                flak_ext_t = 0.0;
            }
            // RECOIL (Chad 2026-08-28: "add some recoil animation") --
            // COSMETIC, render-only: each round kicks the gun body (barrel +
            // receiver + drum + grips, the parts that recoil on the real
            // Oerlikon) ~6 cm aft in cradle space; a spring returns it with
            // tau 55 ms, so at the 7.5 Hz cyclic the gun visibly hammers.
            // Reads spawned_accum WITHOUT draining it (the audio drain owns
            // the zero). Kick size / tau are FEEL DIALS.
            flak_recoil *= std::exp(-clamped_dt / 0.055);
            flak_recoil = std::min(
                0.14, flak_recoil + 0.06 * flak_fk.spawned_accum);
            // ── HIT / KILL CONFIRMATION (Chad 2026-08-29). Deltas of the
            // player-only counters (AI-gun sweeps book into the sink and
            // never move these); bumped only WHILE MANNED, but tracked every
            // frame so mounting mid-fight cannot fire a stale delta.
            // tau 0.25 -> 0.35 s, round 3: the 0.25 s white flash was too
            // brief to register in Chad's drives (paired with the yellow
            // recolour in draw.cpp).
            flak_hitmark *= std::exp(-clamped_dt / 0.35);
            flak_killmark *= std::exp(-clamped_dt / 0.60);
            if (flak_fk.manned) {
                if (cw.hits > flak_prev_hits) flak_hitmark = 1.0;
                if (cw.kills > flak_prev_kills) flak_killmark = 1.0;
            }
            // Bug-B observation hook (env-gated, works in a REAL DRIVE too):
            // SEADS_FLAK_HIT_DBG=1 prints every player hit/kill counter move
            // so one instrumented drive can settle whether cw.hits EVER
            // rises while manned (the X chain is proven sound headlessly;
            // round 5 confirmed the dead-band suspect — one X all round —
            // and Chad RULED "raise the self destruct time": 4.8 s puts
            // shell reach at ~1800 m = engage_m, dead band closed).
            if (std::getenv("SEADS_FLAK_HIT_DBG") &&
                (cw.hits != flak_prev_hits || cw.kills != flak_prev_kills))
                std::fprintf(stderr,
                             "[FHIT] tick=%lld hits=%lld kills=%lld "
                             "manned=%d\n",
                             static_cast<long long>(loop.tick_count),
                             static_cast<long long>(cw.hits),
                             static_cast<long long>(cw.kills),
                             flak_fk.manned ? 1 : 0);
            flak_prev_hits = cw.hits;
            flak_prev_kills = cw.kills;
            // ── STAGE B COSMETICS (immersion ladder 2/4). THE SAME UNDRAINED
            // READ as the recoil above, and it must stay in THIS window:
            // step_frame() (above) ADDS to spawned_accum, this block reads it
            // WITHOUT draining, and the audio block far below owns the zero.
            // Move any of these after the drain and they silently never fire.
            // Shapes/dials all live in render/flak_gun.h; the app owns only
            // the accumulators.
            flak_flash = render::flak::flash_step(flak_flash, clamped_dt,
                                                  flak_fk.spawned_accum);
            flak_shake = render::flak::shake_step(flak_shake, clamped_dt,
                                                  flak_fk.spawned_accum);
            flak_blast = render::flak::blast_step(flak_blast, clamped_dt,
                                                  flak_fk.spawned_accum);
            // ── STAGE C: THE SPENT BRASS. THE FOURTH READER, and it is in
            // THIS window on purpose (render/flak_gun.h "STAGE C"): the same
            // undrained spawned_accum the recoil and the three Stage B
            // envelopes read, before the audio block far below owns the zero.
            // ★ SPAWNED BEFORE flak_shot_seq ADVANCES, so the case indices are
            // the shots' OWN indices -- case i of this frame gets
            // flak_shot_seq + i, unique and monotone, and the jitter that
            // falls out is reproducible in a smoke run.
            // ★ IT DIFFERENCES spawned_total, NOT spawned_accum. A DISCRETE
            // reader cannot use the accumulator: its zero is owned by the
            // audio block, which is skipped whole when audio fails to open
            // (every --smoke run), and the first build then re-spawned the
            // running total EVERY FRAME -- 3.1 million cases in 12 k frames,
            // the ring churning so fast not one case ever landed. The three
            // Stage B envelopes survive a stuck accumulator because a
            // saturating exponential cannot tell 7 from 7000; a spawner can.
            {
                const long long shots_now = flak_fk.spawned_total;
                long long d = shots_now - flak_brass_seen;
                if (d > 0 && flak_fk.manned) {
                    // Cap one frame's worth: a long stall must not dump the
                    // whole backlog into the ring at one pose.
                    if (d > 32) d = 32;
                    render::flak::Stations bst;
                    if (render::flak_stations(bst))
                        render::flak::brass_spawn_burst(
                            flak_brass, flak_fk.mount, bst, flak_fk.pose,
                            static_cast<int>(d), flak_brass_seen);
                }
                flak_brass_seen = shots_now;
            }
            // The pile is world-anchored and outlives the fight, so it steps
            // whether or not he is still on the gun; before the first round it
            // is an empty vector and this is one branch.
            if (!flak_brass.cases.empty()) {
                // ★ THE PAD IS THE SNOW, NOT THE ROCK. F-PLACE grounds a gun
                // at the BARE TERRAIN radius on purpose (it buries the crib so
                // the pad sits flush), so the surface the player sees is the
                // snowpack ON TOP of that -- up to 0.75 m higher. Resting the
                // cases at mount.pos simulated and drew them perfectly and
                // left every one of them invisible under the snow. ONE query
                // per frame at the mount (mount.up IS the site's unit
                // direction), never per case.
                const double pad_h =
                    flak_fk.mount.up != glm::dvec3(0.0)
                        ? snow_field.depth_at(flak_fk.mount.up)
                        : 0.0;
                render::flak::brass_step(flak_brass, flak_fk.mount, params.g,
                                         clamped_dt, pad_h);
            }
            if (flak_fk.spawned_accum > 0)
                flak_shot_seq += flak_fk.spawned_accum;
            // ── STAGE D: the AI gunners' per-frame bookkeeping. Three jobs,
            // and all three are in THIS window on purpose (the frame-order
            // contract in render/flak_gun.h): the envelopes read spawned_accum
            // UNDRAINED, the audio block below owns the zero.
            //
            //   (1) THE DOUBLE-DRIVE GUARD. The gun the player mans is taken
            //       off its AI gunner outright: manned = false makes flak_tick
            //       skip both the slew and the fire, and flak_ai_tick returns
            //       on its first line without touching demand -- so a release
            //       re-arms from wherever the barrel actually is, no snap.
            //   (2) The station table, read off the loaded GLB the same way
            //       the player's is at MAN time -- the tick carries st_muzzle
            //       through the cradle kinematics from there.
            //   (3) The flash / blast envelopes and the FX phase, per gun.
            {
                render::flak::Stations ast;
                const bool have_ast = render::flak_stations(ast);
                for (std::size_t ai = 0; ai < flak_ai.size(); ++ai) {
                    app::FlakAiGun& ag = flak_ai[ai];
                    const bool player_has_it =
                        flak_fk.manned && flak_fk.gun == ag.gun;
                    ag.fk.manned = !player_has_it;
                    if (player_has_it) ag.fk.fire_held = false;
                    if (have_ast) ag.fk.stations = ast;
                    const int sp = ag.fk.spawned_accum;
                    ag.flash = render::flak::flash_step(ag.flash, clamped_dt,
                                                        sp);
                    ag.blast = render::flak::blast_step(ag.blast, clamped_dt,
                                                        sp);
                    if (sp > 0) ag.shot_seq += sp;
                }
            }
        }
        // ★ J, NOT G. G IS THE LANDING GEAR -- in BOTH input paths
        // (input/live_input.cpp:74, input/raw_input.cpp:75). Putting the mode
        // switch there meant one press dropped the gear AND jumped to the sled,
        // which is what Chad hit on the first drive. J has no flight-sim
        // muscle memory and nothing else claims it. V stays RESERVED for his
        // cockpit view -- do not take it.
        const bool sled_key = IsKeyPressed(KEY_J) && !spawn_menu.open;
        // ★★ AND YOU CANNOT LEAVE A FLYING AIRCRAFT. Chad's word for the first
        // version was "teleported", which is exactly the failure §1 names:
        // "teleporting the rider is the menu, not embodiment". Requiring the
        // aircraft to be STOPPED ON THE GROUND does not make this embodiment --
        // that is S8 and nothing here pretends otherwise -- but it removes the
        // part the law actually rejects, and it stops the aircraft being
        // abandoned mid-air with a dead stick while you drive away from it.
        const bool can_mount =
            loop.curr.on_ground && glm::length(loop.curr.velocity) < 4.0;
        // ★★★ L1 -- THE INTERACT SITE TABLE, REBUILT EVERY FRAME FROM LIVE
        // POSITIONS. Never cached: a cached site is a cached position and the
        // machine moves. `app/interact.h` owns the reach law; this block only
        // says what exists and where.
        // ★ L5 grew this from 4: the aeroplane, the machine, up to two own
        // surface pumps and the own flak gun.
        // ★ L10 grew it by one more: the aeroplane's ENGINE, a second verb on
        // the same object.
        app::InteractSite sites[7];
        int n_sites = 0;
        // ★★★ L10 -- THE ENGINE SITE IS REGISTERED FIRST, ON PURPOSE.
        // `app::nearest_site` breaks a tie by REGISTRATION ORDER (its compare
        // is strictly `<`), and these two sites are at the same aeroplane and
        // therefore always exactly tied. The prompt line shows ONE sentence,
        // and with a broken engine the useful sentence is the wrench, not the
        // ladder: boarding a dead stick puts him in a glider on the ground.
        // Both KEYS stay armed regardless -- `mode_context_from` resolves per
        // kind, which is that function's whole banner -- so J still boards.
        if (combat::engine_needs_repair(cw.damage)) {
            app::InteractSite& e = sites[n_sites++];
            e.kind = app::SiteKind::EngineRepair;
            // ★ AT THE MAN'S OWN RADIUS, exactly like the pump site below and
            // for the same reason: `app::nearest_site` compares straight-line
            // distances, so a site projected onto the walker's own sphere
            // makes that compare the GROUND distance without the site table
            // needing to know the law. The law itself (for the sim tick, which
            // holds the real aeroplane position) is
            // combat::engine_in_repair_reach.
            e.pos_w = glm::normalize(loop.curr.position) *
                      (player.on_foot() ? glm::length(walker.pos)
                                        : glm::length(loop.curr.position));
            e.reach_m = interact_dials.engine_reach_m;
        }
        {
            app::InteractSite& a = sites[n_sites++];
            a.kind = app::SiteKind::AircraftBoard;
            a.pos_w = loop.curr.position;
            a.reach_m = interact_dials.aircraft_reach_m;
        }
        if (player.sled_seeded) {
            app::InteractSite& s = sites[n_sites++];
            s.kind = app::SiteKind::SledMount;
            s.pos_w = sled.position;
            s.reach_m = interact_dials.sled_reach_m;
        }
        // ★ THE OWN SURFACE PUMP, AND ONLY WHILE IT NEEDS FIXING. Chad's ask
        // is "revive a LOST pump", so the site is live when the pump is dead
        // OR merely damaged -- and absent when it is whole, which is what makes
        // "U near a healthy pump shows nothing" structural rather than a
        // separate check the prompt could disagree with.
        //
        // ⚠ INDEX == FACTION FOR SURFACE PUMPS is the audit's fact
        // (combat/raid.h:266, app/instructor_tick.h:2126) and it is READ here
        // rather than assumed: the loop asks each pump whether it is a surface
        // pump of the player's own faction.
        if (conquest_on) {
            for (int i = 0; i < combat::kNumPumps && n_sites < 6; ++i) {
                const combat::Pump& pm = cq.state.pumps[i];
                if (!pm.surface) continue;
                if (pm.faction != cq.state.player_faction) continue;
                if (pm.alive && pm.hp >= pm.max_hp) continue;
                app::InteractSite& s = sites[n_sites++];
                s.kind = app::SiteKind::PumpRepair;
                // ★ 2026-09-03: the pump sits 10 m up its mast (surf() lambda:
                // radius_at + 10.0); the site is its FOOT, on the man's own
                // radius, so the 6 m reach is walked on the ground. The same
                // law lives in combat::pump_in_repair_reach for the tick.
                s.pos_w = glm::normalize(pm.pos) *
                          (player.on_foot() ? glm::length(walker.pos)
                                            : glm::length(loop.curr.position));
                s.reach_m = interact_dials.repair_reach_m;
                s.faction = pm.faction;
                s.index = i;
            }
        }
        // ★★★ L5 -- THE FLAK GUN, AT ITS OWN APPROACH MARK.
        //
        // The gun is not a point: `st_approach` is a station BAKED INTO THE
        // SHIPPED GLB (render/flak_gun.h, pinned by test_flak_gun.cpp at
        // y == 0, behind the shoulder pads) and until this rung it was read by
        // NOTHING. It is a TRAIN station, so it rides the platform's own train
        // angle -- the mark is behind the gunner wherever the gun is pointing,
        // which is what makes "walk up to it" mean the same thing at every
        // bearing. Transformed by the gun's mount frame, never retyped.
        //
        // ⚠ OWN FACTION ONLY, exactly like the pump sites: you do not man the
        // enemy's air defence by walking onto its pad.
        //
        // ⚠ AND THE GROUND QUERY IS HERE, IN THE FRAME BLOCK, OUTSIDE THE TICK
        // LOOP -- `snow_field.sample_tap` is armed around the tick loop for the
        // sled's own queries and an extra ground record inside that window
        // makes every tape of the drive unreplayable (the R4c finding, stated
        // at the dismount block). It is null out here.
        if (!flak_guns.empty()) {
            if (!flak_site_st_ok) flak_site_st_ok = render::flak_stations(flak_site_st);
            for (int gi = 0;
                 gi < static_cast<int>(flak_guns.size()) && n_sites < 7; ++gi) {
                const render::FlakDraw& fg = flak_guns[gi];
                if (conquest_on && fg.faction != cq.state.player_faction)
                    continue;
                if (!flak_site_st_ok) continue;  // no GLB -> no mark, no key
                const render::flak::MountFrame mf =
                    render::flak::make_mount_frame(fg.pos, fg.up, fg.fwd0);
                const glm::dvec3 ap = app::flak_approach_world(
                    mf, flak_site_st, fg.train_rad);
                const glm::dvec3 adir = glm::normalize(ap);
                app::InteractSite& s = sites[n_sites++];
                s.kind = app::SiteKind::FlakGun;
                s.pos_w = app::flak_stand_pos(
                    ap, snow_field.drive_radius_at(adir),
                    walker_params.lie_clearance_m);
                s.reach_m = interact_dials.gun_reach_m;
                s.faction = fg.faction;
                s.index = gi;
            }
        }
        //
        // Where the PLAYER'S BODY is for the purposes of reach: his own feet
        // once he is off, the aeroplane's CG while he is in it.
        const glm::dvec3 reach_from =
            player.on_foot() ? walker.pos : loop.curr.position;
        near_site = app::nearest_site(sites, n_sites, reach_from, player.mode);
        interact_line = app::interact_prompt(near_site);
        // ★ 2026-09-06 (Chad: "doesn't let me get to within 6 m ... that is
        // as close as I can get"): the out-of-reach hint counted DOWN to the
        // reach and then vanished into a static "U  FIX PUMP", so the last
        // number a man ever saw was the reach itself. Inside reach the line
        // keeps the ground distance to the foot, so it reads as arriving.
        // ★ L10: the engine's prompt counts the same way, off the same law,
        // because "it reads as arriving" is a property of walking up to a
        // thing and not a property of pumps.
        if (near_site.kind == app::SiteKind::PumpRepair ||
            near_site.kind == app::SiteKind::EngineRepair) {
            const double gd = std::sqrt(
                combat::ground_offset2(near_site.pos_w, reach_from));
            static char fix_line[48];  // the prompt seam holds a C string
            std::snprintf(fix_line, sizeof fix_line, "U  FIX %s  %.0f m",
                          near_site.kind == app::SiteKind::PumpRepair
                              ? "PUMP"
                              : "ENGINE",
                          gd);
            interact_line = fix_line;
        }
        // ★ 2026-09-03 (Chad: "no UI to instruct"): on foot with a job in
        // sight but out of reach, SAY SO -- the nearest PumpRepair site
        // within 120 m along the ground names itself and its distance.
        if ((interact_line == nullptr || interact_line[0] == ' ') &&
            player.on_foot()) {
            // ★ L10: the PUMP still wins the line when both are out there.
            // Same ordering as the mode table's InteractKey arm and for the
            // same reason -- the pump is the one with a match clock running
            // against it -- so the hint never advertises a job the U key
            // would not have chosen.
            auto nearest_of = [&](app::SiteKind k) {
                double best = 1e30;
                for (int i = 0; i < n_sites; ++i) {
                    if (sites[i].kind != k) continue;
                    best = std::min(
                        best, std::sqrt(combat::ground_offset2(sites[i].pos_w,
                                                               reach_from)));
                }
                return best;
            };
            const double pump_d = nearest_of(app::SiteKind::PumpRepair);
            const double eng_d = nearest_of(app::SiteKind::EngineRepair);
            static char hint[64];  // the prompt seam holds a C string
            if (pump_d < 120.0) {
                std::snprintf(hint, sizeof hint,
                              "WALK TO THE PUMP TO FIX  %.0f m", pump_d);
                interact_line = hint;
            } else if (eng_d < 120.0) {
                std::snprintf(hint, sizeof hint,
                              "WALK TO THE PLANE TO FIX THE ENGINE  %.0f m",
                              eng_d);
                interact_line = hint;
            }
        }
        // ★ L10 ENGINE WALK-IN PROBE: hold the fixture, log the instrument,
        // and press U once the site lights.
        if (enginewalk_done) {
            loop.curr = enginewalk_park;  // the aeroplane stays parked
            loop.prev = enginewalk_park;
        }
        if (enginewalk_done && env.ground != nullptr) {
            if (loop.tick_count % 30 == 0) {
                const double gd = std::sqrt(
                    combat::ground_offset2(loop.curr.position, walker.pos));
                std::fprintf(
                    stderr,
                    "ENGINEWALK t=%lld gd=%.2f engine=%.3f wmode=%d spd=%.2f "
                    "pmode=%d target=%d site=%d frac=%.3f prompt=[%s]\n",
                    static_cast<long long>(loop.tick_count), gd,
                    cw.damage.engine, static_cast<int>(walker.mode),
                    glm::length(walker.vel), static_cast<int>(player.mode),
                    static_cast<int>(player.repair_target),
                    static_cast<int>(near_site.kind), repair_frac_hud,
                    interact_line ? interact_line : "");
            }
            // ★ ONE PRESS, exactly like a man's -- ARMED here and SPENT at
            // the real U key below, so the rig goes through the SAME
            // transition a player's press goes through. A probe that wrote
            // the mode directly would certify a path nobody can take.
            if (!enginewalk_armed &&
                near_site.kind == app::SiteKind::EngineRepair &&
                player.mode == app::PlayerMode::Afoot &&
                walker.mode == sim::WalkerMode::Afoot) {
                enginewalk_armed = true;
                enginewalk_press = true;
            }
        }
        // ★ PUMP WALK-IN PROBE: the instrument line, every 30 ticks.
        if (pumpwalk_done && pumpwalk_pump >= 0 && loop.tick_count % 30 == 0 &&
            env.ground != nullptr) {
            const glm::dvec3 pp = cq.state.pumps[pumpwalk_pump].pos;
            const glm::dvec3 pdir = glm::normalize(pp);
            const glm::dvec3 wdir = glm::normalize(walker.pos);
            const double wr = glm::length(walker.pos);
            glm::dvec3 d = pdir * wr - walker.pos;
            d -= wdir * glm::dot(d, wdir);
            std::fprintf(
                stderr,
                "PUMPWALK t=%lld gd=%.2f h_dem=%.2f h_drv=%.2f "
                "dem_rise_to_pump=%.2f drv_rise_to_pump=%.2f wmode=%d "
                "spd=%.2f pmode=%d site=%d prompt=[%s]\n",
                static_cast<long long>(loop.tick_count), glm::length(d),
                wr - env.ground->radius_at(wdir),
                wr - snow_field.drive_radius_at(wdir),
                env.ground->radius_at(pdir) - env.ground->radius_at(wdir),
                snow_field.drive_radius_at(pdir) -
                    snow_field.drive_radius_at(wdir),
                static_cast<int>(walker.mode), glm::length(walker.vel),
                static_cast<int>(player.mode), static_cast<int>(near_site.kind),
                interact_line ? interact_line : "");
        }
        // ★ L5 -- AND THE WAY OUT. Sites are only live on foot, so while he is
        // ON the gun the table is empty and the prompt would go blank on the
        // one control the player has just learned. The same seam says how to
        // let go (the "J GET ON" / "OFF THE MACHINE" pairing).
        if (player.mode == app::PlayerMode::OnGun)
            interact_line = "O  LEAVE THE GUN";
        // ★ L5 -- WHICH gun, and WHERE he stands when he steps off it. The
        // mode table is glm-free and answers only "a gun is in reach"; the
        // index and the mark are geometry, so they are resolved here off the
        // same site table the gate reads. Nearest wins, exactly like
        // `nearest_site`.
        int gun_site_gi = -1;
        glm::dvec3 gun_site_stand{0.0};
        {
            double best_d2 = 0.0;
            for (int i = 0; i < n_sites; ++i) {
                if (sites[i].kind != app::SiteKind::FlakGun) continue;
                const glm::dvec3 d = sites[i].pos_w - reach_from;
                const double d2 = glm::dot(d, d);
                if (d2 > sites[i].reach_m * sites[i].reach_m) continue;
                if (gun_site_gi < 0 || d2 < best_d2) {
                    gun_site_gi = sites[i].index;
                    gun_site_stand = sites[i].pos_w;
                    best_d2 = d2;
                }
            }
        }
        // ★ AND THE MARK FOR THE GUN HE IS ALREADY ON, which is NOT in the
        // site table: sites are only live on foot (app/interact.h), so the
        // frame he presses O to leave there is no FlakGun site to read. It is
        // the same station through the same transform, off the manned gun's
        // own mount frame and its CURRENT train -- he steps off the platform
        // where the platform is now, not where it was when he got on.
        glm::dvec3 gun_leave_stand = walker.pos;
        glm::dvec3 gun_leave_face{0.0};
        if (flak_fk.manned && flak_fk.gun >= 0 &&
            flak_fk.gun < static_cast<int>(flak_guns.size())) {
            const glm::dvec3 ap = app::flak_approach_world(
                flak_fk.mount, flak_fk.stations, flak_fk.pose.train_rad);
            gun_leave_stand = app::flak_stand_pos(
                ap, snow_field.drive_radius_at(glm::normalize(ap)),
                walker_params.lie_clearance_m);
            // Facing AWAY from the gun: he has just let go of it and turned
            // round. `walker_place_afoot` tangentialises this at his feet.
            gun_leave_face =
                app::flak_face_away(gun_leave_stand, flak_fk.mount.pos);
        }
        // ★★★ IS HE ON HIS FEET? THE WALKER ANSWERS, BY ENUMERATOR NAME.
        // `player.mode == Afoot` only means the keys are the man's -- a fall
        // sets it while `walker.mode` is still Falling / Buried / Down /
        // CrawlProne. Without this the F key admitted a repair from a man
        // face-down in the snow (red-team finding, 2026-09-01).
        const bool man_upright = walker.mode == sim::WalkerMode::Afoot;
        // ★★★ A FINISHED JOB IS NOT AN ABANDONED ONE. The site table drops a
        // pump the instant it is whole (`pm.alive && pm.hp >= pm.max_hp`), so
        // the frame after a successful fix has no pump in reach and the
        // walked-away arm fired -- printing "FIX ABANDONED" over the
        // "PUMP BACK ON LINE" the revive had just written, on the central
        // action of the whole rung. The pump's own HP is what tells the two
        // apart, so it is handed to the table (red-team finding, 2026-09-01).
        // ★ L10: asked OF THE JOB HE IS ON. The pump's HP answers for a pump
        // and the engine's health answers for an engine; asking the pump
        // while he is at the aeroplane would report "abandoned" on a finished
        // engine, which is the exact sentence-swap the pump rung's red team
        // found and named FinishRepair to prevent.
        bool repair_complete = false;
        if (player.repair_target == app::RepairTarget::Pump && conquest_on &&
            player.repair_pump >= 0 &&
            player.repair_pump < combat::kNumPumps) {
            const combat::Pump& rpm = cq.state.pumps[player.repair_pump];
            repair_complete = rpm.alive && rpm.hp >= rpm.max_hp;
        } else if (player.repair_target == app::RepairTarget::Engine) {
            repair_complete = !combat::engine_needs_repair(cw.damage);
        }
        app::ModeContext mctx = app::mode_context_from(
            sites, n_sites, reach_from, player.mode, can_mount,
            player.sled_seeded,
            glm::length(sled.velocity) < interact_dials.dismount_stop_ms,
            man_upright, repair_complete);
        // ★ STING: the table's launch gate. Shouldered + a launch remaining;
        // uprightness rides in mctx.man_upright like every other on-foot verb.
        mctx.drone_ready =
            sting_shouldered && sting.st.left > 0 && !sting.st.active;
        mctx.sled_upright = !sled.rolled;  // the seat is a stance too
        // Walking away from the pump ends the job, through the same two writes
        // the F key uses -- and finishing it ends the job through the same two
        // writes and a different sentence.
        {
            // ★ L10: the TARGET IS READ BEFORE THE UPDATE RUNS, because the
            // update is what clears it. Reading it after would name every
            // finished job "PUMP".
            const app::RepairTarget was_target = player.repair_target;
            const app::ModeAction upd = app::player_mode_update(player, mctx);
            if (upd == app::ModeAction::EndRepair) {
                sled_note = "FIX ABANDONED";
                sled_note_until_s = GetTime() + 2.0;
            } else if (upd == app::ModeAction::FinishRepair &&
                       was_target == app::RepairTarget::Engine) {
                // ★ L10: the engine has no revive event of its own to have
                // spoken first (a pump's "PUMP BACK ON LINE" comes from the
                // sim tick), so this is the only sentence the finish gets.
                sled_note = "ENGINE REBUILT";
                sled_note_until_s = GetTime() + 3.0;
            } else if (upd == app::ModeAction::FinishRepair) {
                // A REVIVE already wrote "PUMP BACK ON LINE" from the sim tick
                // and that sentence stands. A merely DAMAGED pump brought back
                // to full never revived and so has no sentence of its own --
                // it gets one here, and only when nothing is already showing.
                if (GetTime() >= sled_note_until_s) {
                    sled_note = "PUMP AT FULL";
                    sled_note_until_s = GetTime() + 2.0;
                }
            }
            // ★★★ GAIT LADDER G2j -- THE WORK POSE, STEPPED WHERE THE MODE
            // IS DECIDED. `Repairing` is the level and `FinishRepair` is the
            // edge, and both are known exactly here -- reading them anywhere
            // else would be a second opinion about a state machine that has
            // one owner (app/player_mode.h's whole banner).
            //
            // ⚠ IT ADVANCES ON THE SIM CLOCK, NOT THE WALL CLOCK: `fr.ticks`
            // fixed sub-steps at `params.sim_dt` each, so a --smoke run's
            // wrench turns at exactly the rate a drive's does and a frame the
            // accumulator did not fire advances nothing (the scarf channel's
            // own rule, render/sled_model.h).
            bool work_now = player.mode == app::PlayerMode::Repairing;
            bool work_finish = upd == app::ModeAction::FinishRepair;
            // ★ THE WORK-POSE SMOKE RIG (SEADS_SMOKE_REPAIR=1 wrench only,
            // =2 also fires the settling blow on a 1.5 s beat, SMOKE ONLY --
            // the SEADS_SMOKE_WALK pattern). A repair needs a DAMAGED pump
            // inside reach, and no smoke rig can drive a machine to one; the
            // pose it produces is the same pose the U key produces, because
            // this sets the same two inputs the mode machine sets and
            // nothing else. It cannot exist outside --smoke.
            if (smoke_frames > 0) {
                static const int smoke_repair = [] {
                    const char* e = std::getenv("SEADS_SMOKE_REPAIR");
                    return e != nullptr ? std::atoi(e) : 0;
                }();
                if (smoke_repair > 0 &&
                    walker.mode == sim::WalkerMode::Afoot) {
                    work_now = true;
                    if (smoke_repair >= 2 && (loop.tick_count % 90) == 0)
                        work_finish = true;
                }
            }
            work = sim::step_repair_work(
                work, work_now, work_finish,
                params.sim_dt * static_cast<double>(fr.ticks), work_params);
        }
        // ★ J, NOT G. G IS THE LANDING GEAR -- in BOTH input paths
        // (input/live_input.cpp:74, input/raw_input.cpp:75). Putting the mode
        // switch there meant one press dropped the gear AND jumped to the sled,
        // which is what Chad hit on the first drive. J has no flight-sim
        // muscle memory and nothing else claims it. V stays RESERVED for his
        // cockpit view -- do not take it.
        app::ModeAction mact = app::ModeAction::None;
        if ((sled_key || sled_env_arm) && snow_field.hf != nullptr) {
            // The smoke arm bypasses the parked-aircraft gate by construction:
            // it exists to exercise the drive path headlessly from frame 1.
            if (sled_env_arm) mctx.aircraft_ready = true;
            mact = app::player_mode_transition(
                player, app::ModeEvent::MountKey, mctx);
            if (mact == app::ModeAction::RefuseMountMoving) {
                sled_note = "LAND AND STOP THE AIRCRAFT FIRST";
                sled_note_until_s = GetTime() + 2.5;
            } else if (mact == app::ModeAction::RefuseDismountMoving) {
                sled_note = "STOP THE MACHINE FIRST";
                sled_note_until_s = GetTime() + 2.5;
            } else if (mact != app::ModeAction::None) {
                // ★ §8 P1-3: freeze the aircraft's LATCHED throttle ramp across
                // the drive. poll_live keeps ramping the latch off LEFT SHIFT /
                // LEFT CTRL, which the sled repurposes as stand/tuck -- without
                // this, two seconds of standing hands back an aircraft at full
                // commanded throttle on dismount.
                if (mact == app::ModeAction::SeedAndMount) {
                    saved_throttle_target = live_dev.throttle_target;
                    sled_thumb = 0.0;
                    sled_lean_lat = 0.0;
                    sled_lean_fwd = 0.0;
                } else if (mact == app::ModeAction::BoardAircraft) {
                    live_dev.throttle_target = saved_throttle_target;
                    // ★ SLED TAPE R1: dismount ends the episode — drain,
                    // footer, close. The file is already crash-valid without
                    // this.
                    //
                    // ★ L1: AND "DISMOUNT" NOW MEANS BOARDING THE AEROPLANE,
                    // not stepping off the machine. Getting off on foot leaves
                    // the machine in the world and the tape running -- the man
                    // walking beside it is part of the same drive episode, and
                    // the tape is a MACHINE tape (sim/walker.h) that does not
                    // care which body the keys are going to.
                    sled_tape_close();
                }
            }
            if (mact == app::ModeAction::SeedAndMount) {
                const glm::dvec3 up = glm::normalize(loop.curr.position);
                const glm::dvec3 nose =
                    loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
                glm::dvec3 fwd = nose - glm::dot(nose, up) * up;
                if (glm::length(fwd) < 1e-6)
                    fwd = glm::normalize(glm::cross(up, glm::dvec3(0, 0, 1)));
                fwd = glm::normalize(fwd);
                glm::dmat3 basis;
                basis[0] = glm::cross(fwd, up);
                basis[1] = up;
                basis[2] = -fwd;
                // Parked BESIDE the aircraft, not inside it: the plane is now
                // stopped on the same ground, and seeding at its CG puts the
                // machine through the fuselage.
                const glm::dvec3 beside =
                    glm::normalize(loop.curr.position + basis[0] * 5.0);
                // ★★★ L3: THROUGH THE ONE SEEDING FUNCTION
                // (app::seed_sled_at). The whole-struct reset and the four
                // R4a `right_*` re-arms moved there UNCHANGED when the spawn
                // menu added a second site that puts a machine down; the
                // reason they must travel together is written at that
                // function. The orientation is rebuilt from the SAME
                // (up, fwd) pair this block already computed -- fwd is
                // already tangent, so the re-projection inside is a no-op to
                // within an ulp, and nothing downstream of a mount seed is a
                // golden.
                app::seed_sled_at(
                    sled,
                    beside * (snow_field.drive_radius_at(beside) +
                              sled_params.cg_height_m + 0.05),
                    up, fwd);
                sled_prev = sled;
                // ★ R5 rows 4/5/6: a fresh seed starts with clean air —
                // a previous run's roost/exhaust/breath must not hang
                // over the new spawn (the wingtip_smoke_reset precedent).
                render::sled_plumes_reset(sled_plumes);
                // SF3-B: the untracked reference, taken BEFORE the visual
                // rig below lays anything.
                const double drv_pre_demo = snow_field.drive_radius_at(
                    glm::normalize(sled.position));
                // ★★★ R4c: AND THE MAN IS SEEDED WITH THE MACHINE. This block
                // resets `sled` -- which re-attaches the grip -- and there is
                // NO `!sled_seeded` guard on it, so every press of the mount
                // key runs it. Without this line the man was left behind in
                // whatever state he was in: `walker.mode` still `Afoot`, so the
                // CAMERA stayed anchored at the old fall site while the machine
                // drove away, W/S/A/D drove BOTH bodies, and the next grip
                // break was silent forever -- `walker_throw` returns early on a
                // man who is already off, so no departure and no helmet dent.
                //
                // The two states are one fact and must be written together.
                // Found by red-team, 2026-09-01.
                // ★ L1: THROUGH THE ONE SEAM (app/player_mount.h). The
                // seed has already reset the whole SledState (so the grip is
                // attached again by construction) -- routing through the seam
                // anyway is what keeps the number of places a rider is put on a
                // machine at exactly one.
                app::player_mount_request(sled, walker);
                gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
                ++sled_epoch;  // external write -- see the decl
                // ★ SF3-B VISUAL RIG (SEADS_TRACK_DEMO=1): lay a
                // deterministic star of runs THROUGH THE MACHINE so a
                // screenshot can certify the whole chain -- TrackField ->
                // drive_radius_at -> the rider patch -- with no hand on
                // the throttle. Fired HERE, at the seed, because the sled
                // is placed beside the PLANE and is nowhere near
                // loop.spawn_up: track laid at load time landed off the
                // map as far as the camera was ever concerned.
                if (snow_field.tracks != nullptr && !track_demo_done &&
                    std::getenv("SEADS_TRACK_DEMO") != nullptr) {
                    track_demo_done = true;
                    const glm::dvec3 c0 = glm::normalize(sled.position);
                    glm::dvec3 e0 =
                        glm::cross(glm::dvec3(0.0, 1.0, 0.0), c0);
                    if (glm::length(e0) < 1e-9)
                        e0 = glm::cross(glm::dvec3(1.0, 0.0, 0.0), c0);
                    e0 = glm::normalize(e0);
                    const glm::dvec3 n0 =
                        glm::normalize(glm::cross(c0, e0));
                    const double R0 = snow_field.hf->R;
                    for (int ray = 0; ray < 8; ++ray) {
                        const double th = ray * 3.14159265358979 / 4.0;
                        const glm::dvec3 fwd =
                            n0 * std::cos(th) + e0 * std::sin(th);
                        const glm::dvec3 side =
                            n0 * -std::sin(th) + e0 * std::cos(th);
                        for (int i = 0; i < 400; ++i) {
                            const double t = 0.15 * i;
                            const double lat = 3.0 * std::sin(t * 0.05);
                            // R4: through the SHIPPED depth-carrying path,
                            // so the demo certifies the law the sled
                            // actually lays, not a constant-depth stand-in.
                            const glm::dvec3 d0 = glm::normalize(
                                c0 + fwd * (t / R0) + side * (lat / R0));
                            sled_tracks.add(
                                d0, snow_field.track_lay_depth_at(d0));
                        }
                    }
                    std::printf(
                        "SF3-B: demo track at the sled, %zu stamps; "
                        "drive_radius delta on-run %+.4f m\n",
                        sled_tracks.size(),
                        snow_field.drive_radius_at(c0) - drv_pre_demo);
                }
                // SK-1c audit defect 5: the seed reseeds the KERNEL but the
                // input accumulators are app-side and survived the remount
                // -- dismount at half-lock and you remounted already
                // steering. Harmless while steer was binary; steer now has
                // yaw authority.
                sled_steer_cmd = 0.0;
                sled_lean_lat = 0.0;
                sled_lean_fwd = 0.0;
                // ★ SLED TAPE R1: one tape per episode, auto-on at the
                // mount seed (an out-of-kernel state write — the tape's
                // first body record is the post-seed pin as an O override,
                // so replay starts from exactly this machine).
                sled_tape_open();
                // Announced, so the smoke run PROVES the drive path
                // executed rather than merely not crashing -- an
                // armed-but-never-entered harness is the S2 lesson (a green
                // suite over an unbound input) in a different costume.
                TraceLog(
                    LOG_INFO,
                    "WINTER S3: DRIVE mode -- sled on the drive surface, "
                    "depth %.2f m under it",
                    snow_field.depth_at(up));
            }
            // ★★★ L1 -- STEPPING OFF, AND GETTING BACK ON, THROUGH THE ONE
            // SEAM. Both are OUT-OF-KERNEL WRITES to `sled` (the grip latch),
            // so both do what every other such write in this file does: bump
            // the epoch (render finite-differences velocity across it) and emit
            // a tape override record.
            if (mact == app::ModeAction::Dismount) {
                // 0.9 m off the LEFT running board, on the drive surface,
                // facing the way the machine faces. `basis[0]` of the machine
                // is its own +X = RIGHT (sim/sled.h body frame), so left is the
                // negative of it -- and the side step is re-grounded rather
                // than merely offset, because a tangential step on a 15 km ball
                // leaves the sphere.
                const glm::dmat3 sR = glm::mat3_cast(sled.orientation);
                const glm::dvec3 ddir = app::dismount_dir(
                    sled.position, sR[0], interact_dials.dismount_side_m);
                // ⚠ THE GROUND QUERY IS HERE, IN THE FRAME BLOCK, AND NOT
                // INSIDE THE TICK LOOP. `snow_field.sample_tap` is armed around
                // the tick loop for exactly the sled's own queries; a
                // `drive_radius_at` fired inside that window would emit an
                // extra ground record and make every tape of this drive
                // unreplayable (the R4c finding, one block down). It is null
                // out here.
                const glm::dvec3 dpos = app::dismount_pos(
                    ddir, snow_field.drive_radius_at(ddir),
                    walker_params.lie_clearance_m);
                app::player_dismount_request(
                    sled, walker, dpos, sR * glm::dvec3(0.0, 0.0, -1.0));
                gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
                sled_note = "OFF THE MACHINE";
                sled_note_until_s = GetTime() + 1.5;
                // ★★★ AND THE TAPE EPISODE ENDS HERE, BY LAW RATHER THAN BY
                // HAND. A dismount is the first override in this program whose
                // `grip` is not the pin roster's default, and `grip` is not in
                // the roster at all -- so a recorded dismount replayed as a
                // machine that still had a rider on it, from that tick to the
                // end of the drive (app/player_mount.h states the measurement).
                // `sled_tape_mark_override` refuses the state and closes the
                // file; the next mount opens a fresh episode.
                sled_tape_mark_override(sled);
                ++sled_epoch;  // external write -- see the decl
            } else if (mact == app::ModeAction::Mount) {
                // ★ THE MACHINE IS NOT RE-SEEDED. It stays exactly where he
                // left it -- that is what "the sled persists in the world"
                // means, and it is the whole difference between walking back to
                // your machine and being handed a new one.
                app::player_mount_request(sled, walker);
                gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
                // SK-1c audit defect 5, same argument: the input accumulators
                // are app-side and survive a remount.
                sled_steer_cmd = 0.0;
                sled_lean_lat = 0.0;
                sled_lean_fwd = 0.0;
                sled_thumb = 0.0;
                sled_note = "ON THE MACHINE";
                sled_note_until_s = GetTime() + 1.5;
                // ★ A MOUNT IS A REPLAYABLE OVERRIDE (the seam restores the
                // whole default `GripState`), so a walk-back after a FALL stays
                // inside one tape -- the fall itself emits no override, the
                // kernel reproduces it. But a walk-back after a DISMOUNT has no
                // tape left to override: that episode was closed on the way
                // off, and this is where the next one starts.
                if (sled_tape_on)
                    sled_tape_mark_override(sled);
                else
                    sled_tape_open();
                ++sled_epoch;  // external write -- see the decl
            }
        }
        // ★★★ L1 -- F FIXES THE PUMP, O MANS THE GUN. Both are position-gated
        // by the same site table the J key reads, so a key near nothing does
        // nothing and shows nothing.
        //
        // ⚠ F IS ALSO THE FLAP CYCLE (input/live_input.cpp:73). The plan's
        // audit listed F as free and it is not; the collision is harmless here
        // (the aeroplane is parked and its flaps are cosmetic while nobody is
        // in it) but it is a real overlap and it is named rather than claimed
        // away. Reported as an open issue.
        // ★ L10: ... or the engine walk-in probe's one shot (smoke only by
        // construction -- nothing else can set the flag).
        if ((IsKeyPressed(KEY_U) || enginewalk_press) && !spawn_menu.open) {
            enginewalk_press = false;
            const app::ModeAction fa = app::player_mode_transition(
                player, app::ModeEvent::InteractKey, mctx);
            if (enginewalk_done)
                std::fprintf(stderr, "ENGINEWALK pressed U -> action=%d\n",
                             static_cast<int>(fa));
            if (fa == app::ModeAction::BeginRepair) {
                sled_note = player.repair_target == app::RepairTarget::Engine
                                ? "FIXING ENGINE"
                                : "FIXING PUMP";
                sled_note_until_s = GetTime() + 1.5;
            } else if (fa == app::ModeAction::EndRepair) {
                sled_note = "FIX PAUSED";
                sled_note_until_s = GetTime() + 1.5;
            }
        }
        // ★★★ L5 -- THE WALK-UP (plan §4.5, docs/FLAK_GUN_SPEC.md §7 F-WALK).
        // Manning the gun now requires all three of: the keys are the MAN'S
        // (PlayerMode::Afoot), the man is ON HIS FEET (the walker answers, by
        // enumerator name -- see `man_upright` above), and he is standing
        // inside `[interact] gun_reach_m` of the gun's own baked approach
        // mark. The old rule -- 30 m from a stopped sled or a landed aeroplane
        // -- is GONE, not kept as a fallback: it was labelled scaffolding
        // awaiting exactly this state.
        if (IsKeyPressed(KEY_O) && !spawn_menu.open) {
            const app::ModeAction ga = app::player_mode_transition(
                player, app::ModeEvent::GunKey, mctx);
            if (ga == app::ModeAction::ManGun && gun_site_gi >= 0) {
                flak_man(gun_site_gi);
                sled_note = "ON THE GUN";
                sled_note_until_s = GetTime() + 1.5;
            } else if (ga == app::ModeAction::ManGun) {
                // The table said yes and the geometry could not name a gun.
                // That cannot happen -- the same site table answered both --
                // but a mode left at OnGun with nothing manned would be a
                // silent dead end, so it is unwound loudly instead.
                app::player_mode_force(player, app::PlayerMode::Afoot);
                sled_note = "NO GUN IN REACH";
                sled_note_until_s = GetTime() + 2.0;
            } else if (ga == app::ModeAction::LeaveGun) {
                flak_unman(gun_leave_stand, gun_leave_face);
                sled_note = "OFF THE GUN";
                sled_note_until_s = GetTime() + 1.5;
            } else if (!flak_guns.empty()) {
                // Say WHY, and say the one thing that is actually wrong.
                if (player.mode == app::PlayerMode::Sled)
                    sled_note = "GET OFF THE MACHINE FIRST";
                else if (player.mode == app::PlayerMode::Pilot)
                    sled_note = "GET OUT OF THE AIRCRAFT FIRST";
                else if (!man_upright)
                    sled_note = "GET UP FIRST";
                else
                    sled_note = "WALK UP TO THE GUN";
                sled_note_until_s = GetTime() + 2.0;
            }
        }
        // ★★★ GIVE UP -- X HELD (Chad 2026-09-04): "I need a way I can
        // respawn in a snowmachine / plane." A man on foot four kilometres
        // from a parked aeroplane with no stings left has no move in the game;
        // this is his move. It is the AIRCRAFT crash path replayed by hand
        // (app/instructor_tick.h): the death is booked with its own cause, the
        // conquest pool pays a life, the airframe is reborn clean at the spawn
        // marker, and the same spawn menu opens -- Snowmachine only while a
        // pump of yours is damaged, exactly as after any other death.
        //
        // ⚠ LEGAL EVERYWHERE BUT THE COCKPIT AND THE GUN. In the aircraft the
        // crash IS the give-up, and the aeroplane's own path owns it; on the
        // gun the unman seam needs a stand point, so he gets off it first.
        // ⚠ THE LOCK (ruling R9) WINS: once the match clock is live the hold
        // completes into the refusal note and nothing else -- a keypress must
        // never end the match.
        // ⚠ THE MACHINE STAYS IN THE WORLD, seeded where he left it, so the
        // map's SLED glyph keeps its answer.
        gave_up_frame = false;
        if (smoke_frames == 0 && !spawn_menu.open && IsKeyDown(KEY_X) &&
            player.mode != app::PlayerMode::Pilot &&
            player.mode != app::PlayerMode::OnGun) {
            giveup_hold_s += GetFrameTime();
            if (giveup_hold_s < kGiveUpHoldS) {
                std::snprintf(giveup_note, sizeof giveup_note,
                              "HOLD X -- GIVING UP  %.1f",
                              kGiveUpHoldS - giveup_hold_s);
                sled_note = giveup_note;
                sled_note_until_s = GetTime() + 0.3;
            } else {
                giveup_hold_s = -1e9;  // one completion per hold
                if (combat::respawns_locked(cq.state)) {
                    sled_note = "NO RESPAWNS -- THE MATCH CLOCK IS LIVE";
                    sled_note_until_s = GetTime() + 6.0;
                } else {
                    app::book_player_death(
                        cw, combat::CombatWorld::DeathCause::kGiveUp,
                        loop.curr, params, env_ptr);
                    if (conquest_on) combat::on_player_death(cq.state);
                    // the man leaves whatever he was on: launcher, tape,
                    // machine keys -- the sting itself dies with the mode
                    // (the silent-kill below) if it was flying.
                    sting_shouldered = false;
                    sled_tape_close();
                    live_dev.throttle_target = saved_throttle_target;
                    app::player_mode_force(player, app::PlayerMode::Pilot);
                    // the clean rebirth, mirrored from the crash path
                    combat::reset_damage(cw.damage);
                    cw.player_hp = combat::summary_hp(cw.damage);
                    cw.player_invuln_ticks = static_cast<int>(
                        cw.setup.respawn_invuln_time / params.sim_dt + 0.5);
                    for (weapon::Projectile& p : cw.enemy_pool)
                        p.active = false;
                    loop.curr = app::player_spawn(
                        app::PlayerSpawnChoice::Aircraft, params, spawn_alt,
                        loop.spawn_up, loop.spawn_fwd);
                    loop.prev = loop.curr;
                    loop.prev_up = sim::local_up(loop.curr.position);
                    loop.aim.reseed(loop.curr.orientation, loop.prev_up);
                    loop.grounded = true;
                    gave_up_frame = true;
                    sled_note = "THE SUDBURIAN GAVE UP";
                    sled_note_until_s = GetTime() + 3.0;
                }
            }
        } else if (giveup_hold_s != 0.0) {
            // released, or the hold is illegal here: say why once, reset.
            if (giveup_hold_s > 0.0 && giveup_hold_s < kGiveUpHoldS &&
                IsKeyDown(KEY_X)) {
                sled_note = player.mode == app::PlayerMode::Pilot
                                ? "YOU ARE IN THE AIRCRAFT -- FLY IT"
                                : "GET OFF THE GUN FIRST";
                sled_note_until_s = GetTime() + 2.0;
            }
            giveup_hold_s = 0.0;
        } else if (IsKeyPressed(KEY_X) && smoke_frames == 0 &&
                   !spawn_menu.open) {
            sled_note = player.mode == app::PlayerMode::Pilot
                            ? "YOU ARE IN THE AIRCRAFT -- FLY IT"
                            : "GET OFF THE GUN FIRST";
            sled_note_until_s = GetTime() + 2.0;
        }
        // ★ STING FROM THE SEAT (Chad 2026-09-04): "press P on the snowmachine
        // ... deployment from the seat to the Sudburian's hands to take aim".
        // ONE answer to "where is the man and which way does he face" for
        // the shoulder, the launch and the aim camera: on the machine (or
        // flying a sting launched from it) the SEAT is the stance; otherwise
        // the walker. The seat is legal while the machine is on its skis.
        const auto sting_stance_ok = [&]() {
            return (player.mode == app::PlayerMode::Afoot && man_upright) ||
                   (player.mode == app::PlayerMode::Sled && !sled.rolled);
        };
        const auto sting_stance = [&](glm::dvec3& pos, glm::dvec3& heading,
                                      double& head_h) {
            const bool seated =
                player.mode == app::PlayerMode::Sled ||
                (player.mode == app::PlayerMode::Drone &&
                 player.drone_home == app::PlayerMode::Sled);
            if (seated) {
                pos = sled.position;
                heading = sled.orientation * glm::dvec3(0.0, 0.0, -1.0);
                // ★ MEASURED, replacing the 1.35 placeholder (the game-loop
                // packet asked for the posed number): the rider's neck sits
                // 0.657 m over the kernel CG in the riding hunch, and the
                // posed launcher's st_muzzle sweeps 0.76-0.86 m across the
                // shouldering elevations (test_sting_pose executes the
                // band). 0.80 splits it, so the drone is born out of the
                // drawn muzzle, not a torso-length above it.
                head_h = 0.80;
            } else {
                pos = walker.pos;
                heading = walker.heading;
                // Afoot the standing calibration was right; the posed muzzle
                // band is 1.80-1.90 over his boots.
                head_h = 1.85;
            }
        };
        // ★★★ ST-5 THE SEATED 180 (Chad 2026-09-05, VERBATIM: "They should
        // only be able to manoeuvre it in a 180 degree sweep while seated on
        // the snowmachine, if they want to target an enemy behind them they
        // need to turn the snowmachine around.")
        //
        // ⚠ ON THE ACCUMULATOR, EVERY FRAME -- not on the drawn pose and not
        // only on a frame the mouse moved. Two failures that buys:
        //   * a pose-only clamp would let the mouse wind `sting_aim_az` to
        //     300 degrees behind an aim that stopped at 90, and then charge
        //     him 210 degrees of mouse to come back off the stop: a dead
        //     stick, and the classic version of this bug.
        //   * a clamp inside the mouse handler would let a REMOUNT (afoot at
        //     150 degrees, then P onto the seat) keep an out-of-window aim
        //     until he happened to move the mouse. There is no mouse event on
        //     the frame the stance changes, so the pull-in has to be
        //     unconditional.
        //
        // ⚠ THE WINDOW TURNS WITH THE MACHINE FOR FREE, and that is the
        // second half of his sentence. `sting_aim_az` is measured over the
        // STANCE FRAME -- every consumer below builds `h` from
        // `sled.orientation * (0,0,-1)` -- so steering the sled carries the
        // whole 180 degrees around with it. "Turn the snowmachine around" is
        // therefore not a rule that had to be written; it is what a
        // heading-relative accumulator already means.
        //
        // ⚠ SEATED ONLY. His sentence names the seat; afoot the walker turns
        // his whole body and the sweep is the world.
        //
        // PLACED HERE, at the head of the sting block: after the mouse has
        // accumulated (the input block, ~4 000 lines up) and BEFORE every
        // consumer -- the P shoulder, the launch click, the over-shoulder and
        // freelook cameras, and the FrameInfo aim the pose is built from --
        // so no reader in the frame can see an unclamped number.
        {
            const bool seated_now =
                player.mode == app::PlayerMode::Sled ||
                (player.mode == app::PlayerMode::Drone &&
                 player.drone_home == app::PlayerMode::Sled);
            if (seated_now)
                sting_aim_az = render::sting::clamp_seated_az(sting_aim_az);
        }
        // ★★★ STING RPAS -- P SHOULDERS THE LAUNCHER (Chad 2026-09-03; the
        // 2026-08-21 P-for-PULL side-hang ruling is RETIRED, his word, same
        // day). The key is three verbs by state: shoulder it (Afoot, upright,
        // a launch left), stow it (shouldered), detonate it (flying). Only
        // the LAUNCH CLICK below and the flight's END move the outer mode,
        // both through the transition table -- P itself never does.
        if (IsKeyPressed(KEY_P) && !spawn_menu.open && smoke_frames == 0) {
            if (player.mode == app::PlayerMode::Drone) {
                sting.cmd.detonate = true;  // ends through sting_step
            } else if (sting_shouldered) {
                sting_shouldered = false;
                sled_note = "LAUNCHER STOWED";
                sled_note_until_s = GetTime() + 1.5;
            } else if (sting_stance_ok() && sting.st.left > 0 &&
                       !sting.st.active) {
                sting_shouldered = true;
                sting_aim_az = 0.0;   // along his heading...
                sting_aim_el = 0.25;  // ...a touch above the horizon
                sled_note = "LEAD THE TARGET -- CLICK TO LAUNCH, P TO STOW";
                sled_note_until_s = GetTime() + 3.0;
            } else {
                if (sting.st.left <= 0)
                    sled_note = "NO STINGS LEFT";
                else if (player.mode == app::PlayerMode::Sled)
                    sled_note = "RIGHT THE MACHINE FIRST";
                else if (player.mode == app::PlayerMode::Pilot)
                    sled_note = "GET OUT OF THE AIRCRAFT FIRST";
                else
                    sled_note = "GET UP FIRST";
                sled_note_until_s = GetTime() + 2.0;
            }
        }
        // THE LAUNCH CLICK. Afoot -> Drone through the table (never a raw
        // mode write -- the loop lane's stranded-hook class); the sting
        // leaves the stock on the shoulder view's aim.
        if (sting_shouldered && smoke_frames == 0 && !spawn_menu.open &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            const app::ModeAction da = app::player_mode_transition(
                player, app::ModeEvent::DroneKey, mctx);
            if (da == app::ModeAction::DeployDrone) {
                glm::dvec3 spos, shead;
                double shh;
                sting_stance(spos, shead, shh);
                const glm::dvec3 lup = glm::normalize(spos);
                glm::dvec3 h = shead - glm::dot(shead, lup) * lup;
                const double hl = glm::length(h);
                h = hl > 1e-6 ? h / hl
                              : glm::normalize(glm::cross(
                                    lup, glm::dvec3{1.0, 0.0, 0.0}));
                const glm::dvec3 sleft = glm::normalize(glm::cross(lup, h));
                const double ce = std::cos(sting_aim_el),
                             se = std::sin(sting_aim_el);
                const glm::dvec3 aim =
                    ce * (std::cos(sting_aim_az) * h -
                          std::sin(sting_aim_az) * sleft) +
                    se * lup;
                app::sting_launch_counted(sting.st, spos + lup * shh, aim,
                                          sting.sp);
                sting.cmd = app::StingCmd{};
                sting.cmd.speed_cmd = sting.sp.speed_cruise;
                sting_cam_az = sting_cam_el = 0.0;  // fresh orbit each flight
                sting_shouldered = false;
                sled_note = "STING AWAY";
                sled_note_until_s = GetTime() + 1.5;
            }
        }
        // The shoulder cannot outlive its stance: a fall, a mount, a respawn
        // while aiming all drop the launcher (the man needs his hands).
        if (sting_shouldered && !sting_stance_ok()) {
            sting_shouldered = false;
            sting_deploy_cut = true;  // ST-5: the pose goes with the stance
        }
        // A flying sting whose operator's MODE was forced elsewhere (a
        // respawn, a spawn-menu rebirth) dies with the link -- silently, no
        // boom, no sentence: the world moved on.
        if (sting.st.active && player.mode != app::PlayerMode::Drone) {
            sting.st.active = false;
            sting.st.end = app::StingEnd::None;
            sting_deploy_cut = true;  // ST-5: forced out of the mode
        }
        // ★★★ ST-5 PHASE D: one step of the deploy blend, on clamped_dt, the
        // flak_ext_t pattern. The rates and the three laws (cut beats launch
        // beats ramp) are pure -- render/sting_deploy.h, and
        // test/unit/test_sting_pose.cpp executes them.
        {
            sting_seated_now =
                player.mode == app::PlayerMode::Sled ||
                (player.mode == app::PlayerMode::Drone &&
                 player.drone_home == app::PlayerMode::Sled);
            render::sting::DeployIn din;
            din.shouldered = sting_shouldered;
            din.cut = sting_deploy_cut;
            din.launched = sting.st.active && !sting_active_prev;
            din.dt = clamped_dt;
            sting_deploy_t = render::sting::deploy_step(sting_deploy_t, din);
            sting_deploy_cut = false;
            sting_active_prev = sting.st.active;
            // Smoke rig (smoke-only, the SEADS_FLAK_EXT class):
            // SEADS_STING_DEPLOY="t" pins the blend so any frame of the
            // three-second draw is screenshot-certifiable headlessly -- the
            // P key is unreachable in a smoke run, so the pose is otherwise
            // undrawable without driving there.
            if (smoke_frames > 0) {
                if (const char* de = std::getenv("SEADS_STING_DEPLOY"))
                    sting_deploy_t = std::clamp(std::atof(de), 0.0, 1.0);
            }
        }
        // ★★★ ST-5 POLISH (b): THE PROPS SPIN (Chad 2026-09-05: "the sting
        // propellors need to spin when flying"). One throttle scalar, one
        // phase, stepped on clamped_dt right where the deploy blend was --
        // the flak_ext_t discipline, and the reason render/draw.h can hold a
        // spin ANGLE rather than reaching for a clock it is not allowed to
        // read.
        //
        // Two rates, because there are two things drawn: the FLYING drone
        // spins from idle to pinned with the throttle, and the one still on
        // the launcher rail spins UP slowly over the last stretch of the
        // three-second deploy, so what leaves his shoulder is visibly already
        // running. Both laws are pure and shared with the audio
        // (render/sting_audio.h) -- picture and sound off one number.
        {
            const double sv = glm::length(sting.st.curr.velocity);
            // ★ ST-5 prop/audio ruling (Chad, verbatim): "by pressing s, I
            // should hear it go down to a lower frequency and volume and see
            // the blades seemingly spinning slower and it shall go down to
            // 150 m/s as a bottom end." That is a statement about the BAND's
            // bottom, not about zero -- so this scalar is the fraction ACROSS
            // THE BAND (speed_min..speed_max) rather than the old fraction of
            // the top. With the floor at 250 the old law pinned the S end at
            // 0.56, which put the props and the buzz better than half way up
            // their range and is exactly why he could not hear or see the
            // machine back off. Band-normalised, S held = 0.0 = the lazy
            // bottom of every voice, W held = 1.0 = pinned.
            //
            // SINGLE-SOURCED off sting.sp: the band constants live in
            // app/sting.h and nothing here restates them, so a later ruling
            // on the band moves the picture and the sound with it.
            //
            // Still monotone in the speed and it never gaps: below the floor
            // (the first moments off the stock at launch_speed 30 m/s, or a
            // hard overspeed bleed) it clamps to the idle voice rather than
            // going silent -- a spun-up drone that is not yet fast, which is
            // what is actually happening.
            const double vlo = sting.sp.speed_min;
            const double vspan = std::max(1.0, sting.sp.speed_max - vlo);
            sting_thr01 = sting.st.active
                              ? std::clamp((sv - vlo) / vspan, 0.0, 1.0)
                              : 0.0;
            sting_prop_rate =
                sting.st.active
                    ? render::sting_spin_rate(sting_thr01)
                    : render::sting_rail_spin_rate(sting_deploy_t);
            // ★ POLISH FLY-2: the phase advances at the APPARENT rate, not the
            // true one. Doing it HERE, at the accumulator, is what keeps the
            // drawn angle continuous -- the alternative (ship the true phase
            // and scale it at the draw) multiplies a growing number by a
            // throttle-dependent factor, so every throttle movement snaps the
            // blades to a new angle. The true rate still ships, for the disc.
            // Smoke rig (smoke-only, the SEADS_FLAK_POSE class):
            // SEADS_STING_SPIN="rad_per_s" pins the TRUE rate so the blur
            // disc's ramp can be screenshot-certified at any speed. Headlessly
            // the drone never flies (there is no P key), so on the rail the
            // rate is 22 rad/s -- below the disc's 40 rad/s threshold -- and
            // the one thing this rung adds would otherwise be unphotographable.
            if (smoke_frames > 0) {
                if (const char* sp_env = std::getenv("SEADS_STING_SPIN"))
                    sting_prop_rate = std::atof(sp_env);
            }
            const double rate =
                render::sting_apparent_spin_rate(sting_prop_rate);
            sting_prop_phase += rate * clamped_dt;
            constexpr double kTwoPi = 2.0 * PI;
            sting_prop_phase -=
                std::floor(sting_prop_phase / kTwoPi) * kTwoPi;
        }
        // Terrain: the DEM surface lives app-side, so the ground check is the
        // app's. One frame of latency is 1-2 m at chase speed -- inside the
        // fuze radius's own slack.
        if (sting.st.active) {
            const glm::dvec3 spos = sting.st.curr.position;
            const glm::dvec3 sup2 = glm::normalize(spos);
            if (glm::length(spos) <
                snow_field.drive_radius_at(sup2) + 0.5)
                sting.cmd.ground_kill = true;
        }
        // THE FLIGHT'S END -- consume the event exactly once: back to Afoot
        // through the table, and one sentence naming why.
        if (sting.st.end != app::StingEnd::None) {
            if (player.mode == app::PlayerMode::Drone)
                app::player_mode_transition(player, app::ModeEvent::DroneKey,
                                            mctx);
            switch (sting.st.end) {
                case app::StingEnd::Target:
                    sled_note = "SPLASH -- TARGET DESTROYED";
                    break;
                case app::StingEnd::SelfDestruct:
                    sled_note = "STING SPENT -- TWO TURNS";
                    break;
                case app::StingEnd::Battery:
                    sled_note = "STING BATTERY DEAD";
                    break;
                case app::StingEnd::Ground:
                    sled_note = "STING DOWN";
                    break;
                case app::StingEnd::Manual:
                    sled_note = "STING DETONATED";
                    break;
                default:
                    break;
            }
            sled_note_until_s = GetTime() + 2.5;
            sting.st.end = app::StingEnd::None;
        }
        if (player.off_aircraft() && player.sled_seeded && fr.ticks > 0) {
            // ★ S3 PHASE A CONTROL MAP (§9d.5, red-team folded):
            //   W = thumb throttle (SPRING-RETURN: off the key it snaps shut
            //       fast -- that closure is what makes the CVT free-roll band
            //       reachable at all), S = brake, A/D = steer,
            //   Q/E = keyboard lean alias, LEFT SHIFT/CTRL = stand/tuck,
            //   C = recentre the weight, mouse = analog weight (above).
            // ★★★ L1 -- W/S/A/D GO TO EXACTLY ONE BODY, AND THIS IS THE
            // TEST FOR WHICH. `bars` is "the player is driving AND the man is
            // still on the machine": a deliberate dismount fails the first half
            // and a fall fails the second, so the same four keys are the MAN'S
            // in both cases and the machine's in neither. The complement is
            // read one block down (`walker.mode != Riding`) so the two cannot
            // disagree about who is holding the bars.
            //
            // ⚠ `walker.mode` IS READ BY ENUMERATOR NAME, never by value --
            // sim/walker.h's frozen surface says so and the enum is ordered by
            // the fall sequence, so a stage inserted tomorrow would silently
            // re-point a literal.
            const bool bars =
                player.driving() && walker.mode == sim::WalkerMode::Riding;
            if (bars && IsKeyDown(KEY_W))
                sled_thumb = std::min(1.0, sled_thumb + 2.5 * frame_dt);
            else
                sled_thumb = std::max(0.0, sled_thumb - 6.0 * frame_dt);
            // ★ R4 DIAGNOSTIC (SEADS_SLED_DRIVE=<0..1>, SMOKE ONLY, the
            // SEADS_SLED_DEBUG_MODE precedent). The track rig we had
            // (SEADS_TRACK_DEMO) lays its whole star in ONE instant at a
            // PARKED machine -- so every screenshot that ever certified the
            // track chain certified the STATIONARY case. Chad drove it and saw
            // nothing. A defect that only exists while the machine and the
            // CAMERA are moving is therefore invisible to every instrument in
            // this repo, which is the same shape as the bank-strip gap: the
            // one place he reports a defect is the one place we cannot look.
            // This pins the thumb open so a smoke run really drives.
            if (smoke_frames > 0) {
                static const char* drv = std::getenv("SEADS_SLED_DRIVE");
                if (drv != nullptr) sled_thumb = std::atof(drv);
            }
            // *** SK-1d: A/D are now the ONLY steer input (the mouse went
            // back to lean), still driving SK-1c's analog accumulator rather
            // than the old binary +/-1 instant full lock.
            // ★ AND THEY SELF-CENTRE ON RELEASE. Binary A/D returned to
            // straight for free the moment you let go; an accumulator does
            // not, and without this the bars stay wherever you last left them
            // and every corner has to be counter-tapped out. Release is
            // FASTER than the push (3.0 vs 2.0 /s) so straightening is never
            // the slow half. Q/E and the mouse do not touch this value.
            // Chad, 2026-08-25: "steering is too slow". 0.8/s took 1.25 s to
            // reach full lock -- SLOWER than the kernel's own handlebar slew
            // (steer_rate_per_s 2.0), so the COMMAND was the bottleneck, not
            // the machine. Matched to the kernel: the bars now move as fast as
            // the sled can act on them, and no faster (a command that outruns
            // the slew just queues up and lies to the HUD).
            const double steer_key_rate = 2.0 * frame_dt;
            const bool steer_l = bars && IsKeyDown(KEY_A),
                       steer_r = bars && IsKeyDown(KEY_D);
            if (steer_l)
                sled_steer_cmd = std::min(1.0, sled_steer_cmd + steer_key_rate);
            if (steer_r)
                sled_steer_cmd =
                    std::max(-1.0, sled_steer_cmd - steer_key_rate);
            if (steer_l == steer_r) {  // neither held, or both fighting
                const double back = 3.0 * frame_dt;
                sled_steer_cmd = sled_steer_cmd > 0.0
                                     ? std::max(0.0, sled_steer_cmd - back)
                                     : std::min(0.0, sled_steer_cmd + back);
            }
            const double lean_key_rate = 2.0 * frame_dt;
            if (IsKeyDown(KEY_Q))
                sled_lean_lat = std::min(1.0, sled_lean_lat + lean_key_rate);
            if (IsKeyDown(KEY_E))
                sled_lean_lat = std::max(-1.0, sled_lean_lat - lean_key_rate);
            if (IsKeyPressed(KEY_C)) {
                sled_lean_lat = 0.0;
                sled_lean_fwd = 0.0;
                sled_steer_cmd = 0.0;  // SK-1c: C recentres the bars too
            }
            // *** SK-1c: H swaps the back HUD between the X-RAY grid and the
            // bottom bar, so both of Chad's presentations can be A/B'd on ONE
            // drive rather than argued about.
            if (IsKeyPressed(KEY_H)) sled_hud_xray = !sled_hud_xray;
            // ★ DRIVE-2: R = AUTORIGHT (Chad: "I need a key for now that
            // lets me autoright until we get the guy running back to the
            // snowmachine"). Scaffolding in the mount-seeding class, not
            // physics: keep the heading, set the machine upright ON the
            // drive surface at its own position, kill all motion. Replaced
            // by the S8 walk-back embodiment later.
            // ★★★ AND R IS NOT A TELEPORT BACK ONTO A MACHINE YOU CHOSE TO
            // LEAVE (red-team finding, 2026-09-01). The guard on this block
            // widened from `drive_mode` to `off_aircraft()` when L1 landed, and
            // `off_aircraft()` is true on foot, repairing and on the gun -- so
            // one unreach-gated press put the player back on the bars from
            // three hundred metres away, which is `[interact] sled_reach_m` and
            // the whole diegetic mount made decorative. The law is in
            // `app::autoright_legal`; the crash case Chad asked for (thrown
            // off, machine down, any distance) still passes it.
            if (IsKeyPressed(KEY_R) &&
                app::autoright_legal(player, sled.rolled)) {
                const glm::dvec3 up = glm::normalize(sled.position);
                glm::dvec3 fwd = sled.orientation * glm::dvec3(0.0, 0.0, -1.0);
                fwd = fwd - glm::dot(fwd, up) * up;
                if (glm::length(fwd) < 1e-6)
                    fwd = glm::normalize(glm::cross(up, glm::dvec3(0, 0, 1)));
                fwd = glm::normalize(fwd);
                glm::dmat3 basis;
                basis[0] = glm::cross(fwd, up);
                basis[1] = up;
                basis[2] = -fwd;
                sled.orientation = glm::normalize(glm::quat_cast(basis));
                sled.position = up * (snow_field.drive_radius_at(up) +
                                      sled_params.cg_height_m + 0.05);
                sled.velocity = glm::dvec3{0.0};
                sled.angular_vel = glm::dvec3{0.0};
                sled.rolled = false;
                // RC readout state rides along (red-team P2-11): a saturated
                // rolled_hold_s would re-latch ROLLED for a beat after the
                // reset; air_s is ground-truth again next step but clear it
                // with the rest for a clean slate.
                sled.rolled_hold_s = 0.0;
                sled.air_s = 0.0;
                // ★★★ K-WS1 / K2. THE EXCHANGE-MOMENTUM HISTORY GOES WITH
                // THEM. `ws_exch_l` is the rider's last-substep exchange
                // momentum, and a machine that has just been TELEPORTED has
                // no momentum history -- carrying one across the reset would
                // spend a delta against a state that no longer exists. It is
                // also not in the tape's pin roster (adding it would change
                // the pin41 column count and break every existing golden), so
                // zeroing it here is ALSO what keeps a taped override
                // replayable bit-exactly with the dial ON -- the replay
                // rebuilds state from the pin, where this field is zero.
                // Measured: without this line
                // sled_tape_round_trip_replays_bit_identical diverges at the
                // override tick (orientation.w) whenever k_air_shift > 0.
                sled.ws_exch_l = glm::dvec3{0.0};
                // ★★★ R4a §7.7, AND THE SAME ARGUMENT ONE FIELD OVER (red-team
                // finding, 2026-08-30). `sled.grip` is not in the pin roster
                // either, so the replay rebuilds it from a pin that does not
                // carry it -- i.e. DEFAULT. `ws_exch_l` re-converges from the
                // inputs and still needed this line; `grip.attached` is a
                // ONE-WAY LATCH and structurally CANNOT re-converge, so it
                // needs it more. Inert today (nothing reads the grip, and the
                // capacity cannot be met), which is exactly why it goes in now:
                // the day stage 2 wires the release in, a live drive containing
                // an autoright would otherwise stop replaying.
                // ★★★ L1: THE CO-ATTACH IS THE SEAM NOW, NOT TWO ADJACENT
                // LINES. R4c added `walker_remount` here because the autoright
                // was already the one place `sled.grip` is re-attached and the
                // man's state has to end in the same place -- but two adjacent
                // lines in one function is the same bug waiting for a third
                // call site, and L1 added two more (the mount seed and the
                // walk-up mount). `app::player_mount_request` writes BOTH, and
                // is now the only thing in this program that does.
                app::player_mount_request(sled, walker);
                gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
                // ★ L1: the outer machine goes back with him. R is still the
                // interim remount (§7.9, R4e retires it), so it is still a
                // mount -- and a mount that left the outer mode at `Afoot`
                // would keep the keys on a man who is back on the bars.
                // ★ ... and it goes through the named force, so a righting can
                // never strand the repair hook (app/player_mode.h). R is
                // already refused mid-job by `autoright_legal`, so this is the
                // belt to that braces -- and the belt is what survives the day
                // somebody widens the predicate.
                app::player_mode_force(player, app::PlayerMode::Sled);
                sled_prev = sled;
                sled_lean_lat = 0.0;
                sled_lean_fwd = 0.0;
                sled_note = "AUTORIGHT";
                sled_note_until_s = GetTime() + 1.5;
                // ★ SLED TAPE R1 (consult Q4 standing rule): any write to
                // `sled` outside step_sled emits a state-override record --
                // through the law, which passes it (the autoright re-attaches
                // the whole default `GripState` through the seam).
                sled_tape_mark_override(sled);
                ++sled_epoch;  // external write -- see the decl
            }
            sim::SledInputs sin_;
            sin_.throttle = static_cast<float>(sled_thumb);
            sin_.brake =
                std::max(live.wheel_brake, IsKeyDown(KEY_S) ? 1.0f : 0.0f);
            sled_brake_cmd = sin_.brake;  // ★ R2c-5, for the rider's arms
            // Handlebars ride the existing ROLL keys (A/D) -- the same hands
            // that bank a plane turn a sled, so nothing has to be relearned
            // across the mode switch. The sign already agrees: A = roll-left =
            // +1 in the house table, and steer +1 = LEFT.
            // *** SK-1d: THE COUPLING IS GONE. SK-1c auto-followed lean off
            // steer (kSteerLeanFollow 0.85) on the theory that the machine
            // turns BY leaning, so steering without leaning asks it to do the
            // one thing it cannot. Chad's verdict overrules that theory on
            // feel -- lean is the thing he DRIVES with on the road, and it
            // cannot be a passenger of the bars. The two axes are now fully
            // independent: mouse = lean, A/D = steer, and neither writes the
            // other. (docs/snowform_measurements.md M12 also removed the
            // measurement the coupling was argued from -- the "carve band"
            // it was tuned around was a 4-second observation artifact.)
            // ★ SCARF-DRAPE RIG (SEADS_SMOKE_SLED_LEAN="lat,fwd", SMOKE ONLY,
            // the SEADS_SLED_DRIVE precedent): pins the lean commands so a
            // headless run can hold the exact articulation the scarf is
            // reported sinking at. Does not exist outside --smoke.
            double smoke_stand = 0.0;
            bool smoke_stand_on = false;
            // ★ AND THE RAMP: "f0,f1,frames" sweeps lean_fwd linearly then
            // holds -- Chad's report is about the END of the pull-back (the
            // roll-up), a DYNAMIC the constant pin cannot reproduce.
            if (smoke_frames > 0) {
                static const char* sr =
                    std::getenv("SEADS_SMOKE_SLED_LEAN_RAMP");
                if (sr != nullptr) {
                    double f0 = 0.0, f1 = 0.0, nf = 1.0;
                    if (std::sscanf(sr, "%lf,%lf,%lf", &f0, &f1, &nf) == 3 &&
                        nf >= 1.0) {
                        const double u = std::min(
                            1.0, static_cast<double>(loop.tick_count) / nf);
                        sled_lean_fwd =
                            std::clamp(f0 + (f1 - f0) * u, -1.0, 1.0);
                        sled_lean_lat = 0.0;
                    }
                }
            }
            if (smoke_frames > 0) {
                static const char* sl = std::getenv("SEADS_SMOKE_SLED_LEAN");
                if (sl != nullptr) {
                    double llat = 0.0, lfwd = 0.0, lst = 0.0;
                    const int got =
                        std::sscanf(sl, "%lf,%lf,%lf", &llat, &lfwd, &lst);
                    if (got >= 2) {
                        sled_lean_lat = std::clamp(llat, -1.0, 1.0);
                        sled_lean_fwd = std::clamp(lfwd, -1.0, 1.0);
                    }
                    if (got >= 3) {
                        smoke_stand = std::clamp(lst, -1.0, 1.0);
                        smoke_stand_on = true;
                    }
                }
            }
            sin_.steer = static_cast<float>(sled_steer_cmd);
            sin_.lean_lat =
                static_cast<float>(std::clamp(sled_lean_lat, -1.0, 1.0));
            sin_.lean_fwd = static_cast<float>(sled_lean_fwd);
            sin_.stand = smoke_stand_on
                             ? static_cast<float>(smoke_stand)
                             : (IsKeyDown(KEY_LEFT_SHIFT)
                                    ? 1.0f
                                    : (IsKeyDown(KEY_LEFT_CONTROL) ? -1.0f
                                                                   : 0.0f));
            // ★★★ L1 -- AND THE WHOLE INPUT STRUCT GOES WITH HIM. The kernel
            // already reads every one of these through `hands_on`
            // (sim/sled.cpp:291) so the MACHINE could never be driven from the
            // snow -- but `sled_brake_cmd` is APP state that render draws the
            // rider's arms from, and it was still answering S while the man was
            // fifty metres away. One statement, at the seam, rather than a
            // guard on each key.
            if (!bars) {
                sin_ = sim::SledInputs{};
                sled_brake_cmd = 0.0;
            }
            // (sled_prev / walker_prev are captured INSIDE the tick loop
            // below, one tick behind -- CAM-SMOOTH. A per-frame capture here
            // was N ticks behind and unusable with accum.alpha().)
            // ★ SLED TAPE R1: arm the ground tap for exactly the sled's own
            // queries (armed around the tick loop only — nothing else calls
            // sample_at in between), record the pin AFTER each tick. The
            // input struct is per-FRAME (consult Q4's finding) but the tape
            // records per TICK — what the kernel actually consumed.
            if (sled_tape_on)
                snow_field.sample_tap =
                    [&](const glm::dvec3& d,
                        const world::SnowpackField::GroundSample& g) {
                        sled_tape.on_ground(sled_tick_no, d, g);
                    };
            for (int t = 0; t < fr.ticks; ++t) {
                // ★ CAM-SMOOTH: the last-two-states pair for BOTH kernel
                // bodies, captured at the top of EVERY tick (unconditionally,
                // so a tick that leaves the walker untouched still yields
                // prev == curr and a still draw, never a stale pair).
                sled_prev = sled;
                walker_prev = walker;
                sled = sim::step_sled(sled, sin_, sled_params, snow_field,
                                      params.sim_dt);
                // ★ N1 THE LEG WORK, APP-ONLY HUD: the stage the kernel has
                // ARMED (sim::LegStage, derived state, not taped), so he can
                // tell an armed stage from a dead key and judge the press.
                // Nothing here feeds the kernel; the tape never sees it.
                if (sled_params.comfort.leg_work_nm > 0.0 && bars) {
                    const char* stage_note = nullptr;
                    if (sled.leg_stage == sim::kLegPitched)
                        stage_note = "STUCK ON END -- CTRL: legs kick it over";
                    else if (sled.leg_stage == sim::kLegInverted)
                        stage_note = "UPSIDE DOWN -- SHIFT: legs lift it";
                    else if (sled.leg_stage == sim::kLegOnSide)
                        stage_note = "ON ITS SIDE -- SHIFT rights it";
                    if (stage_note != nullptr &&
                        (sled_note == nullptr || GetTime() >= sled_note_until_s ||
                         sled_note == stage_note)) {
                        sled_note = stage_note;
                        sled_note_until_s = GetTime() + 0.25;
                    }
                }
                // ★★★ R4c §7.3 STAGES 4-7 -- THE MAN, ON THE MACHINE'S OWN TICK.
                // Stepped INSIDE this loop and not once per frame: he is a
                // kernel body, and a body advanced on frames is frame-rate
                // dependent. He samples the same `snow_field` the machine does.
                {
                    // ★★★ THE TAP IS DISARMED ACROSS THE WALKER'S OWN GROUND
                    // QUERY, AND THIS IS NOT A NICETY -- IT IS WHAT KEEPS A
                    // TAPE OF A FALL REPLAYABLE.
                    //
                    // `snow_field.sample_tap` is armed around this loop and its
                    // comment says exactly why: "armed for exactly the sled's
                    // own queries -- nothing else calls sample_at in between".
                    // R4c put `step_walker` inside that window, and the walker
                    // calls `sample_at` once per tick. Every tick after the
                    // grip breaks would then emit an EXTRA ground record, while
                    // replay -- which re-steps only the sled -- consumes them in
                    // order against an exact direction key. The first walker
                    // record is a key mismatch and the tape is dead, for
                    // precisely the drives this rung exists to record.
                    //
                    // Found by red-team, 2026-09-01. No test covers it: the
                    // tape round-trip leg drives the kernel directly with no
                    // walker in the loop.
                    auto tap_held = snow_field.sample_tap;
                    snow_field.sample_tap = nullptr;
                    if (walker.mode == sim::WalkerMode::Riding &&
                        !sled.grip.attached) {
                        // The grip broke THIS tick. Seed him off the machine's
                        // own state -- the SAME reference point render spends
                        // (CG + his lean), so the drawn offset is exactly zero
                        // on the frame he lets go.
                        const glm::dmat3 sR = glm::mat3_cast(sled.orientation);
                        const glm::dvec3 lean_b(-sled.rider_lat_m,
                                                sled.rider_up_m,
                                                -sled.rider_fwd_m);
                        sim::walker_throw(walker, walker_params,
                                          sled.position + sR * lean_b,
                                          sled.velocity, sR[0],
                                          -sled.rider_lat_m);
                        gait = sim::GaitState{};  // ★★★ GAIT LADDER G1
                        // ★★★ CHAD, 2026-08-31: "I have a damage modeled helmut by
                        // pressing U. Each fall goes up the stages of damage
                        // for the helmut. A subtle accumulation of visible
                        // damage from falls, though it wont be health
                        // affective."
                        //
                        // ★ THE HOOK WAS ALREADY THERE AND ALREADY SAID SO:
                        // render/sled_model.h calls this "cosmetic crash-damage
                        // helmet ... crash logic may set it directly", and the
                        // setter ALREADY clamps to [0,4]. So "each fall goes up
                        // a stage" needs no new clamp and no new state -- the
                        // helmet stops at its worst dent instead of wrapping
                        // round to pristine, which is what U does and is not
                        // what a helmet does. COSMETIC ONLY: nothing reads it
                        // but the draw, and no kernel value depends on it.
                        render::sled_helmet_dent_set(
                            render::sled_helmet_dent_get() + 1);
                        // ★★★ L1: AND THE OUTER MACHINE FOLLOWS THE MAN. A fall
                        // is a dismount the player did not ask for: the keys
                        // are his body's now, the site table has to resolve
                        // around HIS feet, and J has to mean "get back on"
                        // rather than "step off the machine you are not on".
                        // Leaving the outer mode at `Sled` here would have let
                        // one J press in the snow TELEPORT him to the machine's
                        // side -- the fall undone by the mode machine's own
                        // ignorance of it.
                        app::player_mode_force(player, app::PlayerMode::Afoot);
                    }
                    // The SAME keys the machine uses, so his hands do not
                    // change when his body does: W/S walk, A/D turn. The
                    // kernel already ignores these for the machine while he is
                    // off it (sim/sled.cpp §7.4), so they cannot drive both.
                    sim::WalkerInputs win;
                    // ★★★ L1: ... and the same four keys reach the MAN only
                    // while he is the body they belong to. Before L1 he was
                    // only ever off the machine after a fall, so "driving" and
                    // "on foot" could not both want them; a deliberate dismount
                    // makes that a real fork.
                    // ★★★ L5 -- AND NOT WHILE HE IS ON THE GUN. `OnGun` is
                    // not `Riding`, so without this the same W/S/A/D that
                    // trains the gun ALSO walked the man off the pad, and the
                    // camera-anchored body drifted away underneath the sight.
                    // The gun is a third consumer of the four keys and it is
                    // named here, where the other two are decided.
                    // ★ STING -- the FOURTH consumer of the four keys, named
                    // here beside the other three: while the drone flies,
                    // W/S are its speed command (resolved above), and while
                    // the launcher is shouldered the man is planted in his
                    // aiming stance -- neither may also walk the body.
                    // ★ GAIT SMOKE RIG (SEADS_SMOKE_WALK=1, SMOKE ONLY, the
                    // SEADS_SMOKE_GEAR pattern): at tick 40 the grip is cut
                    // exactly where the kernel cuts it, so the REAL throw /
                    // land / get-up / walk chain runs -- no synthetic
                    // placement, because a rig that skips the real path
                    // certifies a different game. Once he is on his feet he
                    // holds forward with a gentle turn so he circles inside
                    // the SEADS_SLEDCAM frame. Does not exist outside
                    // --smoke.
                    if (smoke_frames > 0 &&
                        std::getenv("SEADS_SMOKE_WALK") != nullptr) {
                        if (walker.mode == sim::WalkerMode::Riding &&
                            loop.tick_count > 40) {
                            sled.grip.attached = false;
                        }
                        if (walker.mode == sim::WalkerMode::Afoot ||
                            walker.mode == sim::WalkerMode::CrawlKnees) {
                            win.forward = 1.0f;
                            win.turn = 0.3f;
                        }
                        if (loop.tick_count % 60 == 0) {
                            std::fprintf(
                                stderr,
                                "SMOKE_WALK t=%lld mode=%d speed=%.2f "
                                "g0v=%d g1v=%d rise=%.2f sub=%.2f\n",
                                static_cast<long long>(loop.tick_count),
                                static_cast<int>(walker.mode),
                                glm::length(walker.vel),
                                gait.foot[0].valid ? 1 : 0,
                                gait.foot[1].valid ? 1 : 0, walker.rise_s,
                                walker.submerge);
                        }
                    }
                    if (smoke_frames == 0 &&
                        walker.mode != sim::WalkerMode::Riding &&
                        player.mode != app::PlayerMode::OnGun &&
                        player.mode != app::PlayerMode::Drone &&
                        !sting_shouldered) {
                        win.forward = IsKeyDown(KEY_W)   ? 1.0f
                                      : IsKeyDown(KEY_S) ? -1.0f
                                                         : 0.0f;
                        win.turn = (IsKeyDown(KEY_A) ? 1.0f : 0.0f) -
                                   (IsKeyDown(KEY_D) ? 1.0f : 0.0f);
                    }
                    // ★★★ GAIT LADDER G2i -- THE SHIFT KEY, AND WHICH BODY
                    // IT BELONGS TO. Shift is ALREADY the STAND/tuck input
                    // while he is RIDING (the `sin_.stand` line in the sled
                    // control map above) -- so this reads it only on the
                    // frames the four walk keys are the MAN'S, which is the
                    // same test the block just above uses, restated once
                    // rather than duplicated per key. `sting_shouldered` and
                    // OnGun/Drone are excluded for the reason they are
                    // excluded from W/S/A/D: a fifth consumer of a key that
                    // already has four owners is how a control map rots.
                    sim::HopInputs hin;
                    if (smoke_frames == 0 &&
                        walker.mode == sim::WalkerMode::Afoot &&
                        player.mode != app::PlayerMode::OnGun &&
                        player.mode != app::PlayerMode::Drone &&
                        !sting_shouldered) {
                        hin.shift_down = IsKeyDown(KEY_LEFT_SHIFT) ||
                                         IsKeyDown(KEY_RIGHT_SHIFT);
                        hin.shift_press = IsKeyPressed(KEY_LEFT_SHIFT) ||
                                          IsKeyPressed(KEY_RIGHT_SHIFT);
                    }
                    // ★★★ G2i-b -- THE SMOKE RIG PRESSES THE JUMP
                    // (SEADS_SMOKE_HOP="tick[,hold_ticks]", SMOKE ONLY, the
                    // SEADS_SMOKE_WALK pattern above). The whole acceptance
                    // for an AIR POSE is a screenshot of a man in the air,
                    // and the block above reads the keyboard only when
                    // `smoke_frames == 0` -- so with no scripted press there
                    // is no frame to read. Fires on the first tick at or
                    // after `tick` on which he is actually ON HIS FEET (the
                    // grip-cut / throw / get-up chain owns when that is), and
                    // latches, so the shot is deterministic frame for frame.
                    if (smoke_frames > 0 && smoke_hop_tick > 0 &&
                        walker.mode == sim::WalkerMode::Afoot) {
                        if (!smoke_hop_fired &&
                            loop.tick_count >= smoke_hop_tick) {
                            hin.shift_press = true;
                            smoke_hop_fired = true;
                            smoke_hop_t0 = loop.tick_count;
                        }
                        if (smoke_hop_fired &&
                            loop.tick_count < smoke_hop_t0 + smoke_hop_hold)
                            hin.shift_down = true;
                    }
                    // ★ G2i-b: and WHICH KNEE LEADS comes from the phase he
                    // is walking at, read on the launch frame only.
                    hin.gait_phase01 = walker.gait_phase;
                    hop = sim::step_hop(hop, hin, params.sim_dt, hop_params);
                    // ★ G2i-b: the air pose's own instrument. One line per
                    // airborne tick names the frame to screenshot (and, when a
                    // shot reads wrong, says whether the ARC or the TUCK is the
                    // guilty half). Smoke only.
                    if (smoke_frames > 0 && hop.airborne)
                        std::fprintf(stderr,
                                     "SMOKE_HOP t=%lld h=%.3f vz=%.2f "
                                     "tuck=%.2f lead=%d\n",
                                     static_cast<long long>(loop.tick_count),
                                     hop.height_m, hop.vert_vel_mps,
                                     hop.tuck01, hop.lead_side);
                    // ★★★ AND THE SPRINT IS A MULTIPLIER ON THE RULED DEPTH
                    // CURVE, NOT A FOURTH ROW (L-DEPTH, L-ONE-DIAL). Scaling
                    // the three anchors of a LOCAL COPY leaves
                    // `walker_speed_cap`'s shape in depth exactly as ruled,
                    // and `walker_stride` -- which derives its stride from
                    // that same cap -- picks the longer stride up for free,
                    // so cadence follows through the shipped path with no
                    // second stride law anywhere. The copy is deliberate:
                    // `walker_params` itself must stay the ruled numbers,
                    // because half a dozen other sites read it for
                    // `lie_clearance_m` and the depth fractions.
                    const double hop_mul =
                        sim::hop_speed_mul(hop, hin, hop_params);
                    sim::WalkerParams walk_p = walker_params;
                    walk_p.speed_hardpack_mps *= hop_mul;
                    walk_p.speed_mid_mps *= hop_mul;
                    walk_p.speed_deep_mps *= hop_mul;
                    // ★ L10 ENGINE WALK-IN PROBE: walk him straight at the
                    // parked aeroplane, by the same arithmetic the pump probe
                    // uses -- the target projected onto his own sphere, the
                    // vertical removed, stop half a metre out.
                    if (enginewalk_done &&
                        walker.mode == sim::WalkerMode::Afoot &&
                        player.mode != app::PlayerMode::Repairing) {
                        const glm::dvec3 wup = glm::normalize(walker.pos);
                        glm::dvec3 to =
                            glm::normalize(loop.curr.position) *
                                glm::length(walker.pos) -
                            walker.pos;
                        to -= wup * glm::dot(to, wup);
                        if (glm::length(to) > 0.5) {
                            walker.heading = glm::normalize(to);
                            win.forward = 1.0f;
                            win.turn = 0.0f;
                        } else {
                            win.forward = 0.0f;
                        }
                    } else if (enginewalk_done) {
                        win.forward = 0.0f;  // standing at the work
                    }
                    // ★ PUMP WALK-IN PROBE: walk him straight at the pump.
                    if (pumpwalk_done && pumpwalk_pump >= 0 &&
                        walker.mode == sim::WalkerMode::Afoot) {
                        const glm::dvec3 wup = glm::normalize(walker.pos);
                        glm::dvec3 to = glm::normalize(
                                            cq.state.pumps[pumpwalk_pump].pos) *
                                            glm::length(walker.pos) -
                                        walker.pos;
                        to -= wup * glm::dot(to, wup);
                        if (glm::length(to) > 0.5) {
                            walker.heading = glm::normalize(to);
                            win.forward = 1.0f;
                            win.turn = 0.0f;
                        } else {
                            win.forward = 0.0f;
                        }
                    }
                    // MERGE (gait x loop L9): ONE call, and it takes the gait
                    // lane's sprint-scaled COPY -- hop_mul is exactly 1.0 with
                    // no Shift held (every smoke/probe frame), so the walk-in
                    // probe's arithmetic is untouched.
                    walker = sim::step_walker(walker, win, walk_p,
                                              snow_field, params.sim_dt);
                    // ★★★ GAIT LADDER G1: his feet, stepped off the SAME
                    // published walker state, in the SAME disarmed-tap
                    // window -- `step_gait` samples `snow_field` per foot,
                    // exactly the ground-query discipline `step_walker` just
                    // used above, and for the identical reason (see the
                    // banner on `tap_held`: a tape of a fall must not see any
                    // ground record but the sled's own).
                    {
                        const sim::GaitState gait_before = gait;
                        // ★ G2i-b: the height AND the tuck AND the lead
                        // knee, in one value -- render applies, it does not
                        // author (L-PURE).
                        sim::GaitAir air;
                        air.height_m = hop.height_m;
                        air.tuck01 = hop.tuck01;
                        air.lead_side = hop.lead_side;
                        air.tuck = hop_tuck;
                        // ★ G2i-d: the SIGN of the climb is what tells the
                        // pose which half of the arc he is on.
                        air.vert_vel_mps = hop.vert_vel_mps;
                        air.land = hop_land;
                        gait = sim::step_gait(gait, walker, walk_p,
                                              snow_field, params.sim_dt,
                                              sim::GaitBodyGeom{}, air);
                        // ★ G2g FOOTSTEPS (Chad: "introduce footsteps where
                        // he walks in the snow"): a print IS a stance entry
                        // -- the frame a foot pins is the frame it marks the
                        // snow. Only on his feet, only in real snow (a
                        // plowed road takes no print), and only off a REAL
                        // edge (both states valid -- the seed frame is not
                        // a step).
                        if ((walker.mode == sim::WalkerMode::Afoot ||
                             walker.mode == sim::WalkerMode::CrawlKnees) &&
                            walker.depth_m > 0.03) {
                            for (int s = 0; s < 2; ++s) {
                                if (!gait.foot[s].in_stance ||
                                    !gait_before.foot[s].valid ||
                                    gait_before.foot[s].in_stance)
                                    continue;
                                render::FrameInfo::Footprint& fp =
                                    footprints[footprint_head];
                                fp.pos = gait.foot[s].pin_w;
                                fp.fwd = glm::vec3(walker.heading);
                                footprint_head =
                                    (footprint_head + 1) % 256;
                                footprint_n = std::min(footprint_n + 1, 256);
                            }
                        }
                    }
                    snow_field.sample_tap = tap_held;  // the sled's again
                    if (walker.poof && pending_poof_n < 2) {
                        // §7.5's snow burst, on the roost/spray path (§0.3) --
                        // a fourth TRAIL in render/sled_plumes.h, not a second
                        // particle system.
                        render::PoofRequest& q = pending_poof[pending_poof_n++];
                        const glm::dvec3 wup = glm::normalize(walker.pos);
                        q.up = wup;
                        // ★ THE SNOW SURFACE, NOT HIS CHEST. He is drawn a
                        // lie-clearance above the ground; a burst anchored at
                        // his origin would erupt from the middle of him.
                        q.pos = walker.pos - wup * walker_params.lie_clearance_m;
                        q.fwd = glm::length(walker.heading) > 0.5
                                    ? walker.heading
                                    : wup;
                        // ★ THE KERNEL'S PUBLISHED IMPACT SPEED, not
                        // `length(walker.vel)`: the landing step strips the
                        // inward component, so a velocity read AFTER the step
                        // describes the aftermath and would size the burst at
                        // nothing.
                        q.speed_mps = walker.landed ? walker.poof_speed_mps : 0.0;
                        q.depth_frac =
                            sim::walker_depth_frac(walker_params, walker.depth_m);
                        q.emergence = !walker.landed;
                        sled_note = walker.landed ? "INTO THE SNOW" : "UP";
                        sled_note_until_s = GetTime() + 1.5;
                    }
                }
                // ★ SF3-B: lay track. TrackField::add self-rejects anything
                // closer than stamp_spacing_m, so a parked machine cannot
                // drill a hole and a fast one cannot leave gaps.
                // ★ R4: lay with the LOCAL SNOW DEPTH. The cut is a property
                // of the pass, not of the query -- by the time you look at the
                // rut the snow you displaced is gone -- so the depth rides
                // into the ring here and TrackParams::depress_for resolves it
                // once. ★ R4b: track_lay_depth_at, NOT depth_at -- depth_at
                // subtracts the pass's OWN compaction, so the machine read its
                // own rut, reported no snow, and cut shallower every stamp.
                // See world/snowpack.h for why the guard that patched that is
                // gone and the fix lives here instead.
                if (snow_field.tracks != nullptr) {
                    const glm::dvec3 sdir = glm::normalize(sled.position);
                    const double lay_depth =
                        snow_field.track_lay_depth_at(sdir);
                    const bool laid = sled_tracks.add(sdir, lay_depth);
                    // ★ R4 PROBE (SEADS_TRACK_PROBE=1). Chad drove the shipped
                    // build and saw no track; the only rig that ever certified
                    // the chain lays its stamps at a PARKED machine. This
                    // reports what the MOVING machine actually writes and what
                    // the ground behind it actually reads -- the one thing no
                    // instrument here could see.
                    static const bool probe =
                        std::getenv("SEADS_TRACK_PROBE") != nullptr;
                    if (probe && (sled_tick_no % 120) == 0) {
                        // The comparison NOBODY had ever made: the rider
                        // patch's drawn height against the PLANET MESH's own
                        // drawn height at the same point. It is what found
                        // R4b's root cause -- the patch sat on facet + lift_m
                        // while the planet mesh has carried the ambient fold
                        // since R1, so the whole patch drew 0.56 m below the
                        // world around it.
                        //
                        // ★ POST-FIX READING: the patch base now carries the
                        // fold too, so patch_vs_mesh is lift_m off-track (a
                        // few cm of clearance, as intended) and lift_m + the
                        // cut inside a rut -- NEGATIVE there on purpose,
                        // because a rut is supposed to sit below the snow
                        // around it. The number to watch is the off-track one:
                        // if it ever drifts far from +lift_m, the patch has
                        // forked from the planet mesh again.
                        const double facet =
                            snow_field.facet_radius_fn
                                ? snow_field.facet_radius_fn(sdir)
                                : 0.0;
                        const double fold = snow_field.draw_fold_at(sdir);
                        const double dfm = sled_tracks.deform_at(sdir);
                        std::printf(
                            "TRACKPROBE t=%lld v=%.1f km/h stamps=%zu "
                            "laid=%d lay_depth=%.3f deform_here=%+.4f | "
                            "fold=%.3f lift=0.150 patch_vs_mesh=%+.3f "
                            "drive_vs_mesh=%+.3f\n",
                            static_cast<long long>(sled_tick_no),
                            glm::length(sled.velocity) * 3.6,
                            sled_tracks.size(), laid ? 1 : 0, lay_depth, dfm,
                            fold, 0.150 + dfm,
                            snow_field.drive_radius_at(sdir) - facet - fold);
                        std::fflush(stdout);
                    }
                }
                if (sled_tape_on)
                    sled_tape.on_tick(sled_tick_no, sin_, sled_params, sled);
                ++sled_tick_no;
            }
            snow_field.sample_tap = nullptr;
        }
        // ★★★ L2 — THE WRENCH, ON THE SIM TICK
        // (docs/PLAN_20260901_game_loop_millwright.md §4.2). Chad, 2026-09-01:
        // "fix the pumps by driving up, getting off, then making the fix, the
        // fix should take about a minute to full restore."
        //
        // ⚠ fr.ticks, NEVER frame_dt. "About a minute" is a minute of SIMULATED
        // time — the same AT-9 discipline conquest_countdown_tick and
        // rebuild_conquest_bubbles already follow, and the reason the bubble
        // rebuild below is triggered from a tick result and not from the draw.
        //
        // ⚠ IT IS OUTSIDE THE SLED TAPE'S TAP WINDOW BY CONSTRUCTION — the
        // block above disarms `snow_field.sample_tap` on the line before this
        // one — and it calls no ground query of its own anyway. (R4c's finding:
        // any new `sample_at` inside that window makes every sled tape
        // unreplayable.)
        //
        // ⚠ THE REACH IS RE-ASKED AT THE BODY. The frame-side site table
        // (app/interact.h) ends the job when he walks away, but that is a
        // FRAME's answer and the work happens on TICKS; asking again here means
        // the progress and the prompt are gated on one dial and one position,
        // never on a stale copy of either.
        //
        // ⚠ AND THE WRENCH ONLY TURNS IN A MAN WHO IS STANDING. `combat/
        // pump_repair.h` says outright that it "has no opinion about ...
        // whether the man is standing -- the caller owns the policy", and until
        // the red team pressed F while Buried no caller did. The uprightness is
        // re-asked HERE, beside the reach, for the same reason the reach is
        // re-asked here: the frame's answer is a frame old and the work happens
        // on ticks. BY ENUMERATOR NAME (sim/walker.h is frozen surface).
        repair_frac_hud = -1.0;
        if (conquest_on && player.mode == app::PlayerMode::Repairing &&
            player.repair_target == app::RepairTarget::Pump &&
            walker.mode == sim::WalkerMode::Afoot &&
            combat::pump_in_repair_reach(cq.state, player.repair_pump,
                                         walker.pos, repair_params)) {
            const int rp_i = player.repair_pump;
            bool revived = false;
            for (int t = 0; t < fr.ticks; ++t) {
                if (combat::repair_pump_tick(cq.state, rp_i, params.sim_dt,
                                             repair_params)
                        .revived)
                    revived = true;
            }
            // The bar reads the PUMP'S OWN HP, not a counter beside it: the
            // progress IS the hp, so there is no second counter to keep in
            // sync with it.
            //
            // ⚠ AND THE TRUTH ABOUT BEING SHOT AT WHILE YOU WORK, because the
            // comment that stood here said the opposite. `combat::damage_pump`
            // returns immediately on `!pu.alive` (combat/conquest.h:802), so a
            // DEAD pump is invulnerable: from the moment the fix starts until
            // the tick it revives, no raid, strike or stray round can knock
            // the bar back down. It is not an oversight and it is not being
            // fixed here (RULED, red-team 2026-09-01): the counter-play to a
            // millwright is the MECHANIC, not the machine -- kill the man and
            // the whole job is lost, which is exactly the stake plan §1 R2
            // names. What a raid CAN do is take the pump back the instant it
            // comes alive again, and from that tick the bar tells the truth
            // about that too.
            const combat::Pump& rpm = cq.state.pumps[rp_i];
            repair_frac_hud =
                rpm.max_hp > 0.0
                    ? std::min(1.0, std::max(0.0, rpm.hp) / rpm.max_hp)
                    : 0.0;
            if (revived) {
                // ★ FROM THE SIM TICK, AFTER A REVIVE — never a frame clock.
                // A revive is the ONE event that can move a dome scale UPWARD,
                // and the live bubbles are built from those scales.
                app::rebuild_conquest_bubbles(cq);
                sled_note = "PUMP BACK ON LINE";
                sled_note_until_s = GetTime() + 3.0;
            }
        }
        // ★★★ L10 — THE SAME WRENCH, ON THE ENGINE (Chad 2026-09-07: "fix
        // the airplane engine when it says engine out, the same way the
        // sudburian can fix the pump").
        //
        // Every ⚠ on the pump block above applies here WORD FOR WORD and is
        // not restated: fr.ticks and never a frame clock, outside the sled
        // tape's tap window by construction, the reach re-asked AT THE BODY on
        // the tick rather than trusted from a frame-old site table, and the
        // wrench only turning in a man the WALKER says is standing (by
        // enumerator name -- sim/walker.h is frozen surface).
        //
        // ⚠ NOT GATED ON `conquest_on`. The pump block is, because a pump only
        // exists inside a conquest match. An aeroplane's engine is broken in
        // every mode this game has, including a plain free flight where a
        // botched landing snaps the prop -- gating this the same way would
        // have made the fix silently absent from the modes he flies most.
        if (player.mode == app::PlayerMode::Repairing &&
            player.repair_target == app::RepairTarget::Engine &&
            walker.mode == sim::WalkerMode::Afoot &&
            combat::engine_in_repair_reach(loop.curr.position, walker.pos,
                                           engine_repair_params)) {
            bool restarted = false;
            for (int t = 0; t < fr.ticks; ++t) {
                if (combat::repair_engine_tick(cw.damage, params.sim_dt,
                                               engine_repair_params)
                        .restarted)
                    restarted = true;
            }
            // ★ THE SUMMARY HP IS DERIVED, AND IT IS RE-DERIVED HERE. The
            // combat tick recomputes `cw.player_hp` from the damage every
            // tick it runs (combat/kill.h), and the ground-scrape branch in
            // app/instructor_tick.h re-derives it at its own write for the
            // ticks combat does not run. This is the third writer of the
            // damage and it follows the same rule, so the HP readout can
            // never lag the component bar it summarises.
            cw.player_hp = combat::summary_hp(cw.damage);
            // The bar reads the ENGINE'S OWN HEALTH, not a counter beside it.
            repair_frac_hud = std::min(1.0, std::max(0.0, cw.damage.engine));
            if (restarted) {
                // ★ THE PLATE HAS JUST GONE OUT. `ENGINE OUT` is gated on
                // `dmg_engine <= 0` (render/draw.cpp), so the first tick of
                // work clears it -- and a plate vanishing with no sentence is
                // a change the player cannot attribute to himself.
                sled_note = "ENGINE TURNING OVER";
                sled_note_until_s = GetTime() + 3.0;
            }
        }
        // ★ SLED TAPE R1: the same 1 s drain cadence as the conquest tape —
        // a crash costs at most a second of tape, never the episode.
        if (sled_tape_on && loop.tick_count - sled_tape_last_flush >= 120) {
            std::string buf;
            if (sled_tape.drain(buf)) {
                sled_tape_file << buf;
                sled_tape_file.flush();
            }
            sled_tape_last_flush = loop.tick_count;
        }
        if (conquest_on && loop.tick_count - conquest_tape_last_flush >= 120) {
            std::string buf;
            if (conquest_tape->drain(buf)) {
                conquest_tape_file << buf;
                conquest_tape_file.flush();
            }
            conquest_tape_last_flush = loop.tick_count;
        }
        // CONQUEST FX: drain this frame's pump-death events into the shared
        // explosion FX pool (render-only; frame-drained is fine — FX never
        // touch the plant). The bubble growth already took effect tick-side in
        // app::tick (AT-9); this is only the boom.
        if (conquest_on && !cq.events.empty()) {
            for (const combat::PumpDeathEvent& e : cq.events)
                combat::fx_spawn(cw.fx, e.pos, glm::dvec3{0.0},
                                 combat::FxKind::Explosion);
            cq.events.clear();
        }
        // T25 sub-mark (this fix): input + all catch-up sim/combat ticks this
        // frame — splits the composite app_tick lap so a [PROF] spike can
        // attribute the tick multiplication, not just "everything before
        // draw."
        if (g_prof.on) {
            g_prof.mark("sim_combat");
            g_prof.ticks = fr.ticks;
        }

        // R4-FLY-6/7 TOUCHDOWN + ROLLING FX (Chad: "something to indicate
        // touchdown — dust / splash in water / ice crystals on snow", then
        // fly-7: the splash "should last for the duration of the landing
        // until stopped"): app::tick reports the airborne->GROUNDED capture
        // edge; ADDITIONALLY the burst re-spawns while ROLLING above
        // [fx] rolling_min_speed_ms — spray/dust that dies out as the roll
        // slows and stops. The material decision lives HERE, where season +
        // GIS already do (render only reads); re-spawns re-test the CURRENT
        // position, so rolling off the ice onto the shore switches splash ->
        // dust. Burst spawns at the WHEELS, stationary — the plane rolls
        // away through its own plume. Render-only: the pool never feeds back.
        // Rate limit (review P2-2, now also the re-spawn CADENCE): one burst
        // per half-life (~0.6 s) — bounds the grace micro-cycle AND paces the
        // rolling plume with the same clock-free scan.
        const double roll_speed = glm::length(loop.curr.velocity);
        const bool rolling = loop.curr.on_ground &&
                             game.fx.rolling_min_speed_ms > 0.0 &&
                             roll_speed > game.fx.rolling_min_speed_ms;
        bool touchdown_recent = false;
        if (fr.touchdown || rolling) {
            for (const combat::Fx& g : cw.fx.pool) {
                if (g.active && g.kind == combat::FxKind::Touchdown &&
                    g.age < 0.5 * combat::kTouchdownLifetime) {
                    touchdown_recent = true;
                    break;
                }
            }
        }
        if ((fr.touchdown || rolling) && !touchdown_recent &&
            game.fx.touchdown_intensity > 0.0) {
            const glm::dvec3 fx_pos =
                fr.touchdown ? fr.touchdown_pos : loop.curr.position;
            const double fx_speed =
                fr.touchdown ? fr.touchdown_speed : roll_speed;
            int variant = 0;  // dust
            if (current_season == render::Season::Winter) {
                variant = 2;  // ice crystals (the world is snowed in)
            } else {
                const glm::dvec3 td = glm::normalize(fx_pos);
                for (const render::GisLake& lk : render::kSudburyLakes) {
                    const glm::dvec3 c{lk.dir[0], lk.dir[1], lk.dir[2]};
                    // v1 circle test (handoff): within the lake's equivalent
                    // DISK RADIUS => water. span_m is sqrt(area) (the bake:
                    // sudbury_fetch.py), so radius = span/sqrt(pi) — using
                    // span raw over-covers every round lake ~1.77x and
                    // splashes on dry shoreland (review P1-1).
                    const double r_lake = 0.5642 * lk.span_m;  // 1/sqrt(pi)
                    if (glm::dot(td, c) >= std::cos(r_lake / params.R)) {
                        variant = 1;  // splash
                        break;
                    }
                }
            }
            const glm::dvec3 up_td = glm::normalize(fx_pos);
            const glm::dvec3 wheels =
                fx_pos - up_td * game.ground.contact_height_m;
            const double e01 = std::clamp(
                fx_speed / game.fx.touchdown_ref_speed_ms, 0.15, 1.0);
            combat::Fx& f =
                combat::fx_spawn(cw.fx, wheels, glm::dvec3{0.0},
                                 combat::FxKind::Touchdown, up_td, e01);
            f.variant = variant;
        }

        // rig-B (render-only): carry the commanded surfaces for the Fleet Rig.
        // Only a ticking frame produces a fresh command; a 0-tick frame keeps
        // the previous. RA9-safe: read ONLY into info.player_inputs (the draw),
        // never back into control.
        if (fr.ticks > 0) rig_player_inputs = fr.last_inputs;

        // R4 ground-roll wheel spin (cosmetic, RA9: display state only). The
        // tyre angle integrates ground speed / wheel radius over TICK time
        // (fr.ticks * sim_dt, never frame wall-time — the t_cel discipline;
        // frame-rate independent by construction). Radius is the rig's own
        // wheel spec (single source — no second wheel size anywhere). Wrapped
        // each frame so the float cast in the draw never loses precision; a
        // respawn snaps the tyres to rest with the rest of the display state.
        if (loop.curr.on_ground && fr.ticks > 0) {
            const double wheel_r =
                0.5 *
                render::aircraft_node_specs()[render::kLeftWheel].box_dims.y;
            wheel_roll_rad += glm::length(loop.curr.velocity) / wheel_r *
                              (fr.ticks * params.sim_dt);
            wheel_roll_rad =
                std::fmod(wheel_roll_rad, 2.0 * 3.14159265358979323846);
        }
        if (fr.respawned || gave_up_frame) wheel_roll_rad = 0.0;
        // P0-1: the offered fire was DELIVERED (step_frame gates it onto the
        // first tick) iff the frame ran >= 1 tick; only then is it consumed. A
        // 0-tick frame carries pending_orient to the next frame (like the mouse
        // delta). A mid-frame respawn inside step_frame already cancels the
        // fire; here we simply retire the caller latch once a tick has seen it.
        if (fr.ticks > 0) pending_orient = false;
        // render/rig-D port: capture the frame's last-committed commanded
        // Inputs for the Fleet Rig deflection; a 0-tick frame holds the
        // previous value (same discipline as the HUD's SimState read).
        if (fr.ticks > 0) player_inputs_disp = fr.last_inputs;

        // S-orient (docs/comfort program Q3): the ORIENT verb fired this frame
        // — HARD-CUT the lagged camera-forward to the (freshly
        // velocity-snapped) aim (research REC-1: angular snap beats a slew;
        // position/FOV keep their smoothing). This is the ONE place the
        // discrete orient event re-seats cam_fwd, matching
        // harness::MiniCamera::orient_cut so the comfort instrument measures
        // the shipped law. Cosmetic/downstream of the aim (§9.2 / RA9): a hard
        // cut of a DOWNSTREAM lag var, never fed back into the aim. The
        // freelook orbit is also zeroed so the view recenters behind the flight
        // path (the orient's "go behind me").
        if (fr.orient_fired) {
            cam_fwd = loop.aim.forward();
            orbit = render::CameraOrbit{};
            orbit_inertia.reset();  // S-globelook: the orient's "go behind
                                    // me" also stops the spun globe
        }

        // The freelook-orbit camera is the ONE mouse consumer the caller owns
        // (cosmetic, §9.2, OFF the mouse->aim loop): route the consumed delta
        // to it when freelook holds. clamped_dt-scaled decay below eases it
        // back.
        if (!raw_mode && live.freelook_held) {
            // Both freelook axes are INVERTED (user pref): mouse right ->
            // camera orbits left; mouse up -> camera orbits down. Negate both
            // deltas.
            // S-globelook (v4 rung 3): classify the held frame for the
            // globe-inertia helper — the whole classification is GATED on
            // inertia_tau > 0, so tau = 0 leaves the helper inert and every
            // expression below on the literal legacy tree (structural OFF,
            // the S7-hrz rate=0 pattern). Three frame kinds:
            //   GRAB  — the hand's delta was APPLIED this frame: the position
            //           path below is the unchanged legacy expression; the
            //           helper only estimates the hand's rate (RAW frame_dt,
            //           the aim_curve precedent — the rate window is wall
            //           time) and seeds the coast velocity from it.
            //   PEND  — the hand moved but the delta is still pending (a
            //           0-tick frame): no position change today either, so
            //           add NO motion (the held-with-motion path stays
            //           bit-identical) — just accrue the rate window.
            //   COAST — held and still: the globe glides at the decaying
            //           seeded velocity (clamped_dt, the cosmetic-animation
            //           dt every other camera ease here uses). Coast ENTRY
            //           is dwell-gated INSIDE the helper (red-team P1): a
            //           still frame within inertia_dwell of the last hand
            //           motion is helper-reclassified PEND (zero delta, rate
            //           window accrues), so an integer-mouse slow drag's
            //           0-count gap frames never reseed-and-coast (the
            //           staircase amplification); the expressions below see
            //           only the zero delta, unchanged either way.
            // Cosmetic (§9.2): the orbit never feeds mouse->aim; CQ2 and the
            // input::Freelook latches are untouched.
            render::OrbitInertia::Delta coast_d;
            bool coasting = false;
            if (cparams.freelook_inertia_tau > 0.0) {
                const bool hand_applied =
                    fr.consumed_dx != 0.0 || fr.consumed_dy != 0.0;
                const bool hand_active = hand_applied || live.mouse_dx != 0.0 ||
                                         live.mouse_dy != 0.0;
                if (hand_applied) {
                    // The applied delta in the SAME orbit radians the legacy
                    // expressions apply (inverted axes and all).
                    orbit_inertia.grab(
                        -fr.consumed_dx * cparams.freelook_orbit_sensitivity,
                        -fr.consumed_dy * cparams.freelook_orbit_sensitivity,
                        frame_dt, cparams.freelook_inertia_tau,
                        cparams.freelook_inertia_cap);
                } else if (hand_active) {
                    orbit_inertia.pend(frame_dt);
                } else {
                    coast_d = orbit_inertia.coast(
                        clamped_dt, cparams.freelook_inertia_tau,
                        cparams.freelook_inertia_cap,
                        cparams.freelook_inertia_dwell);
                    coasting = true;
                }
            }
            // S-freelook360 (Chad 2026-07-17 "keep going around indefinitely
            // full freedom"): yaw_max = 0 selects the UNLIMITED orbit — no
            // swing stop; the accumulated yaw wraps to (-pi, pi] each frame
            // (render::wrap_pi) so the stored angle never creeps and the
            // release decay below eases home the SHORT way. yaw_max > 0
            // keeps the legacy clamped swing bit-identically.
            const double yaw_next =
                coasting ? orbit.yaw + coast_d.yaw
                         : orbit.yaw - fr.consumed_dx *
                                           cparams.freelook_orbit_sensitivity;
            orbit.yaw =
                cparams.freelook_orbit_yaw_max > 0.0
                    ? std::clamp(yaw_next, -cparams.freelook_orbit_yaw_max,
                                 cparams.freelook_orbit_yaw_max)
                    : render::wrap_pi(yaw_next);
            // S-globelook: coasting into the yaw swing stop (clamped arm
            // only — the wrap arm has no wall, the globe keeps spinning
            // around) zeroes the yaw coast velocity: the wall absorbs the
            // spin, nothing winds behind it.
            if (coasting && cparams.freelook_orbit_yaw_max > 0.0 &&
                orbit.yaw != yaw_next)
                orbit_inertia.hit_yaw_clamp();
            // Pitch is clamped ASYMMETRICALLY (§6 red-team P1): the OVERHEAD
            // (negative, view-toward-straight-down) side hits the camera-up
            // degeneracy pole at pi/2 - atan(height/distance) ~ 75 deg (the
            // resting view is already tilted down), well inside a naive 90 deg
            // cap — so cap it just short of that pole, DERIVED from the same
            // chase geometry the lens shift tracks. The eye-below (positive)
            // side pole is unreachable, so the config knob alone bounds it.
            const double overhead_cap = render::freelook_overhead_pitch_cap(
                chase.height, chase.distance, chase.degenerate_dot,
                render::kFreelookPoleMargin);
            const double pitch_floor =
                -std::min(cparams.freelook_orbit_pitch_max, overhead_cap);
            const double pitch_next =
                coasting ? orbit.pitch + coast_d.pitch
                         : orbit.pitch - fr.consumed_dy *
                                             cparams.freelook_orbit_sensitivity;
            orbit.pitch = std::clamp(pitch_next, pitch_floor,
                                     cparams.freelook_orbit_pitch_max);
            // S-globelook: same wall-absorb on the pitch floor/cap.
            if (coasting && orbit.pitch != pitch_next)
                orbit_inertia.hit_pitch_clamp();
        }

        // ★★★ L6 -- THE LAST LIFE, SAID OUT LOUD (Chad's ruling R9,
        // 2026-09-03). No respawn happened and none ever will, so the one
        // thing the game owes him is the REASON -- said on the death frame,
        // through the same note seam every other diegetic verdict uses.
        if (fr.respawn_refused) {
            sled_note = "NO RESPAWNS -- THE MATCH CLOCK IS LIVE";
            sled_note_until_s = GetTime() + 6.0;
        }
        // ... or the X hold above re-placed the aeroplane by hand: same menu,
        // same device reset, one block (Chad 2026-09-04).
        if (fr.respawned || gave_up_frame) {
            // ★★★ L3 -- AND THIS IS WHERE THE MILLWRIGHT GETS HIS CHOICE.
            // "Respawn after death: menu offered ONLY while an own surface pump
            // is dead or damaged; otherwise Aircraft, as today" (plan §4.3) --
            // the whole gate is app::spawn_menu_should_show, tested with no
            // window.
            //
            // ⚠ ONLY WHEN THE PLAYER WAS FLYING IT. `fr.respawned` is the
            // AIRCRAFT's rebirth, and the aeroplane can be shot to pieces while
            // it sits parked and its owner is out on the snow with a wrench.
            // That is not the player's death and it must not stop his game to
            // ask him a question.
            if (player.mode == app::PlayerMode::Pilot &&
                app::spawn_menu_should_show(smoke_frames == 0, false,
                                            conquest_on, cq.state)) {
                spawn_menu.open = true;
                spawn_menu.hover = -1;
                spawn_menu.machine_offered =
                    conquest_on &&
                    app::own_surface_pump_to_fix(cq.state) >= 0 &&
                    snow_field.hf != nullptr;
                if (smoke_frames == 0) EnableCursor();
            }
            // Crash reset fired in-frame (the AT-13 clean rebirth). step_frame
            // already neutralized the resolved input + pending mouse; reset the
            // caller's PERSISTENT device state so next frame's poll starts
            // neutral (the virtual stick can't steer the fresh airframe), and
            // the frame-scope `live` so the post-loop cosmetics (orbit decay,
            // HUD freelook flag) read the reborn, hands-off state.
            raw_dev = input::RawDeviceState{};
            // MB-flaps: the device latches reset to clean/up with the rest of
            // the persistent device state (the spawn airframe is clean; a
            // dead life's landing flaps must not deploy on the fresh spawn).
            live_dev.flap_pos = 0;
            live_dev.gear_down = false;
            live = input::LiveInput{};
            live.throttle = static_cast<float>(loop.curr.throttle);
            // Clean rebirth for the cosmetic freelook orbit too (P3c sibling,
            // Fable §6 red-team finding 3): without this, dying while
            // freelook-held draws the fresh spawn through the DEAD life's orbit
            // offset (up to the configured yaw/pitch max), easing back over
            // ~120 ms — a phantom camera swing belonging to no input. Snap it
            // to zero so the reborn life starts looking forward, matching the
            // in-core aim reseed. Off the mouse->aim loop (cosmetic, SPEC
            // §9.2); no trajectory/test touched. Caller glue (no ctest runs
            // seads.exe — honest ledger).
            orbit = render::CameraOrbit{};
            zoom_t = 0.0;   // reborn unzoomed (re-eases up if RMB still held)
            dolly_t = 0.0;  // reborn at the resting chase (freelook re-dollies)
            orbit_inertia.reset();  // S-globelook: nor its coast velocity —
                                    // a dead life's spin can't haunt the
                                    // reborn camera
            orient_tap.reset();     // a dead life's tap can't orient the reborn
            prev_freelook = false;
            pending_orient =
                false;  // nor an undelivered fire from the dead life
            // S-reticle: reborn on the fresh spawn aim (hygiene — the cap
            // would self-heal in one frame anyway).
            reticle_dir = loop.aim.forward();
            // MB-7c: a dead life's vortices must not hang over the fresh
            // spawn, and the trend cue restarts at the spawn speed (else the
            // respawn speed step reads as a phantom multi-g accel).
            render::vortex_reset(vortices);
            // Feature B: dead life's smoke must not hang over the fresh spawn.
            render::wingtip_smoke_reset(wingtip_smoke);
            speed_trend = 0.0;
            prev_speed_for_trend = glm::length(loop.curr.velocity);
            // Same clean rebirth for the lagged camera-forward: snap it to the
            // reborn nose so the fresh spawn isn't drawn easing in from the
            // dead life's camera direction (the P3c/orbit sibling; cosmetic,
            // §9.2).
            cam_fwd = loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            cam_up = loop.aim.up();  // reborn: the aim frame reseeded to level
            // rig-B: reborn surfaces neutral (a dead life's hard deflection
            // must not pose the fresh spawn's control surfaces). Cosmetic, RA9.
            rig_player_inputs = sim::Inputs{};
            // [seasons] W1: a fresh life = a fresh RANDOM season (Chad: "random
            // on every spawn"). No-op under env/static/smoke (pick_season is
            // deterministic there); only the random branch re-rolls. Cosmetic,
            // off the sim/aim loop.
            current_season = pick_season();
        }

        // Freelook orbit eases back to zero when released (SPEC §9.2: cosmetic,
        // OUTSIDE the loop — it never touches the aim). Exponential decay ~120
        // ms; the CQ2 mouse->aim suspension that guards the aim is separate
        // (input::Freelook, above).
        if (!live.freelook_held) {
            const double decay = std::exp(-clamped_dt / 0.12);
            orbit.yaw *= decay;
            orbit.pitch *= decay;
            // S-globelook: release WIPES the coast — the globe never coasts
            // through the ease-home; it eases home exactly as today. This
            // path also covers focus loss, raw mode, and smoke (all read a
            // default `live`), so no stale spin survives any of them.
            orbit_inertia.reset();
        }

        // RMB-zoom amount eases toward held/released each frame (cosmetic, §9.2
        // / RA9 — never fed to mouse->aim). Raw mode never zooms (RMB is the
        // virtual stick there); live.zoom_held is false in raw/focus-loss/smoke
        // so those all ease back to the resting fov. The single current fovy
        // drives the Camera3D, the off-center lens frustum, the reticle
        // projection, AND the matching lens_shift below.
        const double zoom_target = (!raw_mode && live.zoom_held) ? 1.0 : 0.0;
        zoom_t = render::blend_toward(zoom_t, zoom_target,
                                      render::kZoomEaseTime, clamped_dt);
        // Smoke rig (smoke-only, the SEADS_FLAK_LOOK class):
        // SEADS_FLAK_ZOOM="t" pins the RMB-zoom amount [0..1] so the
        // zoom-on-pipper camera path can be screenshot-certified —
        // live.zoom_held is hard-false in every smoke run (a default
        // `live`), so without this the path is unreachable headlessly.
        // NOT A SHIPPED PATH: gated on smoke_frames > 0.
        if (smoke_frames > 0) {
            if (const char* zt = std::getenv("SEADS_FLAK_ZOOM"))
                zoom_t = std::clamp(std::atof(zt), 0.0, 1.0);
        }
        // Ease the re-center gate across the freelook edge (P1-2): mouse-aim ->
        // 1, freelook -> 0. Same time-constant as the fov ease. render-only.
        recenter_gate =
            render::blend_toward(recenter_gate, live.freelook_held ? 0.0 : 1.0,
                                 render::kZoomEaseTime, clamped_dt);
        const double fovy =
            render::kChaseFovyDeg +
            (render::kZoomFovyDeg - render::kChaseFovyDeg) * zoom_t;

        // Freelook mousewheel dolly-out (2026-07-10, Chad): each freelook ENTRY
        // resets dolly_t to 0 (the default, identical to the mouse-aim chase);
        // while Space is held the wheel scrolls out from there. Scroll-down =
        // wider (toward 1), scroll-up = back in (toward 0); a one-line sign
        // flip if it reads backwards. Cosmetic + DOWNSTREAM of the aim
        // (freelook drives the orbit, not mouse->aim, so RA9 holds). Raw mode
        // never dollies; combat always renders the default (see eff_chase
        // below).
        // ★★★ L4 — THE MAP EATS THE WHEEL FIRST (plan §3: "mouse wheel zooms
        // while the map is open ... the map consumes it first when open").
        // The wheel already had an owner -- the freelook dolly below -- and
        // two owners for one axis is a coin flip, so this is a CONSUMPTION,
        // not a second reader: `live.wheel` is zeroed on the way past, and the
        // dolly's own `if (live.wheel != 0.0)` therefore does nothing while
        // the chart is up. Close the map and the dolly has its wheel back,
        // unchanged. Map shut => this block is inert and the dolly is
        // bit-identical to before L4.
        if (map_open && live.wheel != 0.0) {
            const render::MapStyle& ms = render::map_style();
            const int notches = static_cast<int>(
                live.wheel > 0.0 ? std::floor(live.wheel + 0.5)
                                 : std::ceil(live.wheel - 0.5));
            map_view.zoom = render::map_zoom_step(map_view.zoom, notches,
                                                  ms.zoom_step, 1.0,
                                                  ms.zoom_max);
            live.wheel = 0.0;
        }
        {
            // ★ THE CLAMP AND THE FOLLOW ARE RE-EVALUATED EVERY FRAME, not
            // only on a wheel notch. A value that only gets clamped when the
            // wheel moves is a value that can be OUT of the band for as long
            // as nobody scrolls -- which is exactly what SEADS_MAP_ZOOM (and
            // any future config reload) would do. Follow is a pure read of the
            // zoom for the same reason: it must never be found disagreeing
            // with the number printed beside it.
            const render::MapStyle& ms = render::map_style();
            map_view.zoom = render::map_zoom_step(map_view.zoom, 0,
                                                  ms.zoom_step, 1.0,
                                                  ms.zoom_max);
            map_view.follow_player =
                render::map_view_follows(map_view.zoom, ms.follow_zoom);
            // No PAN exists yet, so the stored centre is the fixed map centre
            // and the renderer supplies the follow centre (it is the only side
            // that knows the fit scale the quantum is measured in). The field
            // is the seam a later click-to-pan writes.
            map_view.centre_m = glm::dvec2(0.0, 0.0);
        }
        // ★ ST-5 (a): ...and the aeroplane's freelook dolly only claims the
        // wheel while he is ON the aeroplane. Without this gate a wheel notch
        // taken on the sled would ALSO walk `dolly_t`, and two owners would
        // silently share one axis. It is inert by construction (dolly_t only
        // reaches the chase framing through the aircraft camera, which the
        // sled block overrides outright, and freelook ENTRY zeroes it) --
        // which is exactly why it was cheap to close before it was not.
        if (!raw_mode && live.freelook_held && !player.off_aircraft()) {
            if (!prev_freelook) dolly_t = 0.0;  // reset to default on entry
            if (live.wheel != 0.0)
                dolly_t = std::clamp(
                    dolly_t - live.wheel * render::kDollyWheelStep, 0.0, 1.0);
        }
        prev_freelook = live.freelook_held;
        // Effective chase framing this frame. The dolly is applied ONLY while
        // freelook is held: releasing Space returns to the normal mouse-aim
        // chase, so combat framing — and the plane — is always the default (no
        // getting stuck zoomed out). Fed to the instructor camera AND the
        // matching lens_shift below (single-source, reticle stays centered);
        // the surface clamp (camera.cpp) keeps the eye above ground at any
        // distance.
        render::ChaseParams eff_chase = chase;
        if (!raw_mode && live.freelook_held) {
            // GEOMETRIC mapping (not linear): distance =
            // base*(max/base)^dolly_t. Each wheel notch changes distance by a
            // fixed PERCENTAGE, so the steps are small/fine near the plane and
            // grow as you pull out — "shorter interval closer to the plane"
            // (Chad, 2026-07-10).
            eff_chase.distance =
                chase.distance *
                std::pow(render::kDollyMaxDistance / chase.distance, dolly_t);
        }
        // Env-gated dolly telemetry (SEADS_DOLLY_DEBUG=1): live wheel/level in
        // the title bar so the pilot can confirm the wheel registers. Seam-safe
        // (env only, title bar only) — never affects the shipped view.
        if (smoke_frames == 0 && std::getenv("SEADS_DOLLY_DEBUG")) {
            SetWindowTitle(TextFormat(
                "SEADS  dolly_t=%.2f  dist=%.0fm  wheel=%.1f  freelook=%d",
                dolly_t, eff_chase.distance, live.wheel,
                static_cast<int>(live.freelook_held)));
        }

        const sim::SimState draw_state =
            render::interpolate(loop.prev, loop.curr, accum.alpha());
        // ★ CAM-SMOOTH: the sled, the man and the sting drawn from the SAME
        // sub-tick blend the aeroplane has always had (render/interp.h). Every
        // draw-side reader below takes these; the raw `sled` / `walker` /
        // `sting.st.curr` stay the kernel's, read only by gameplay logic.
        const sim::SledState sled_draw =
            render::interpolate_sled(sled_prev, sled, accum.alpha());
        const sim::WalkerState walker_draw =
            render::interpolate_walker(walker_prev, walker, accum.alpha());
        const sim::SimState sting_draw_state =
            render::interpolate(sting.st.prev, sting.st.curr, accum.alpha());
        // The forward actually fed to the camera pose this frame. Defaults to
        // the persistent lagged cam_fwd; the RMB-zoom re-center rotates a COPY
        // toward the pipper below (never the persistent cam_fwd, so releasing
        // zoom eases back to the lag cleanly). Raw mode ignores it.
        glm::dvec3 render_fwd = cam_fwd;
        // Ease the decoupled camera-forward toward its velocity-anchored target
        // (S7-cam Phase 1); instructor mode only (raw uses the nose-locked
        // chase). cam_fwd is a DOWNSTREAM consumer of the aim — never fed back
        // into mouse->aim (RA9). Below the v-floor, anchor on the nose.
        if (!raw_mode) {
            const glm::dvec3 nose =
                draw_state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const double sp = glm::length(draw_state.velocity);
            const glm::dvec3 vel_dir =
                sp > 1.0 ? draw_state.velocity / sp : nose;
            // S-keyprec (Chad 2026-07-29, SEALED KERNEL v8): the camera is
            // bound to the AIM, always. The override keys reach the trajectory
            // and NOTHING else — no key state is read here, by ruling. (This is
            // the sealed-v6 call; S-keychase's key-flown anchor swap lived here
            // and is RETIRED — see the record in render/camera.h.)
            //
            // ⚠ REVIEW BAR, because no test can enforce it (v8 red-team P2,
            // confirmed LIVE, not hypothetical): re-adding key→camera coupling
            // as a DEFAULTED parameter on MiniCamera::advance and wiring it
            // only HERE reproduces the retired flight-path anchor AND passes
            // the whole suite — the test_relorient.cpp "S-keyprec" leg calls
            // advance without the flag, and no ctest runs seads.exe. So the
            // standing rule is: THIS CALL STAYS BRANCH-FREE ON KEY STATE. A
            // conditional appearing here, or a new argument threaded from
            // `live.override_*`, is a ruling violation on sight — no
            // measurement needed.
            cam_fwd = render::ease_chase_forward(
                cam_fwd, vel_dir, loop.aim.forward(), cparams.cam_lead,
                cparams.cam_lag_base, cparams.cam_lag_gain, clamped_dt);
            // Camera-up = the CARRIED aim-frame up (S7-cam3, 2026-07-07 —
            // reverses S7-cam2's horizon-lock). The raw mouse rotates the aim
            // about THIS frame's own up/right (§9.1); showing the world from
            // the aim frame's orientation makes mouse-up == screen-up at EVERY
            // attitude, so the post-maneuver "mouse inverts" — which was the
            // horizon-locked camera DIVERGING from the carried mouse frame, not
            // the mouse — is gone. Pole-free: the aim frame never degenerates
            // at the zenith, so no ease / cone-carry is needed;
            // aim_chase_camera re-orthogonalizes it against the lagged cam_fwd.
            // Downstream of the aim (never fed back into mouse->aim), so RA9
            // holds. The horizon now ROLLS with the aim through a loop (Chad's
            // ruling "rolls with me").
            cam_up = loop.aim.up();
            // RMB-zoom re-center (mouse-aim): rotate the eye-positioning
            // forward toward the aim/pipper by recenter_t = recenter_gate *
            // zoom_t, so the magnified detail sits under the reticle. Freelook
            // eases the gate to 0 — it zooms the center-screen orbit view IN
            // PLACE ("zooms my center screen") — and the edge is SMOOTH (no
            // whip, P1-2). render_fwd is a per-frame copy (cam_fwd untouched),
            // so a zoom release eases back to the pure lag. DOWNSTREAM of the
            // aim (render-only), never fed to mouse->aim (RA9).
            const double recenter_t = recenter_gate * zoom_t;
            render_fwd =
                render::blend_forward(cam_fwd, loop.aim.forward(), recenter_t);
        }
        // T9a CAVECAM: inside the tunnel net the eye is legally below R+2, so
        // the chase surface clamp would hoist it to bare radius (a zoomed-out
        // exterior egg view, plane invisible on a nose-up pose). Compute the
        // flag ONCE from the draw-state position via the shared predicate and
        // pass it to BOTH pose paths; the camera then keeps the eye on the raw
        // offset inside the cavern. Named trade (docs/tunnel_staging.md T9): a
        // sub-50 m wall graze can drop out of contains() for a few frames
        // before the crash fires => a brief camera pop on an already-dying
        // trajectory; every air-crossable contains() boundary sits >= ~75 m
        // above the R+2 clamp threshold (DEM-probed), so the gate boundary and
        // the clamp-binding region are disjoint — statelessness is safe.
        const bool cave = app::inside_tunnel(env_ptr, draw_state.position);
        render::CameraPose pose =
            raw_mode ? render::chase_camera(draw_state, params, chase, cave)
                     : app::instructor_camera(draw_state, render_fwd, cam_up,
                                              params, eff_chase, orbit, cave);
        // ★ WINTER S3: driving takes the camera. Low and close behind the
        // machine -- a sled is read off the SNOW, and an aircraft chase
        // distance puts the one surface you are judging out of reach.
        if (player.off_aircraft() && player.sled_seeded) {
            const sim::SledState& sdraw = sled_draw;  // CAM-SMOOTH: blended
            // ★★★ R4c: THE CAMERA FOLLOWS THE MAN, NOT THE MACHINE, ONCE HE IS
            // OFF IT. Everything downstream -- the orbit, the presets, the
            // freelook fence -- is written against an anchor POSITION and a
            // tangent FORWARD, so swapping which body supplies those two is
            // the whole change and no framing number moves.
            //
            // ⚠ AND WHILE HE IS BURIED THE ANCHOR IS STILL HIS, deliberately:
            // he is under the snow, the camera stays where he went in, and the
            // frame he climbs back out of is the frame he disappeared into.
            // Cutting to the machine there would throw away the beat.
            const bool cam_on_man =
                walker.mode != sim::WalkerMode::Riding;
            glm::dvec3 anchor_pos =
                cam_on_man ? walker_draw.pos : sdraw.position;
            glm::dvec3 sup = glm::normalize(anchor_pos);
            const glm::dmat3 sR = glm::mat3_cast(sdraw.orientation);
            glm::dvec3 sfwd =
                cam_on_man && glm::length(walker_draw.heading) > 0.5
                    ? walker_draw.heading
                    : sR * glm::dvec3(0.0, 0.0, -1.0);
            sfwd = glm::normalize(sfwd - glm::dot(sfwd, sup) * sup);
            // ★ CAM-SMOOTH: LAG the anchor and the forward (state declared
            // beside sled_cam_az). Feed-forward on the anchor: at constant
            // velocity the filtered point IS the body, so no framing number
            // moves; a bump is an acceleration and gets filtered. The forward
            // rotates toward the body's heading exponentially. Re-seeded on a
            // teleport (respawn, remount across > 20 m) and every smoke frame
            // (the screenshot rigs stay bit-identical). Tangent-projected
            // again after the lag so the orbit basis below stays clean.
            {
                static double tau_pos = 0.06, tau_fwd = 0.12;
                static bool tau_read = false;
                if (!tau_read) {
                    tau_read = true;
                    if (const char* e = std::getenv("SEADS_SLED_CAM_TAU"))
                        std::sscanf(e, "%lf,%lf", &tau_pos, &tau_fwd);
                    // Clamp to [0, 2] s: a stray "inf" would freeze the
                    // anchor at its seed (kp -> 0) and teleport it every 20 m.
                    tau_pos = std::isfinite(tau_pos) ? std::clamp(tau_pos, 0.0, 2.0) : 0.06;
                    tau_fwd = std::isfinite(tau_fwd) ? std::clamp(tau_fwd, 0.0, 2.0) : 0.12;
                }
                const bool reseed =
                    !sled_cam_lag_seeded || smoke_frames > 0 ||
                    glm::length(anchor_pos - sled_cam_anchor) > 20.0;
                if (reseed) {
                    sled_cam_anchor = anchor_pos;
                    sled_cam_fwd = sfwd;
                    sled_cam_lag_seeded = true;
                } else {
                    const glm::dvec3 anchor_vel =
                        cam_on_man ? walker_draw.vel : sdraw.velocity;
                    // The step and its zero-lag feed-forward live in
                    // render/interp.h (cam_lag_step) so test_interp.cpp pins
                    // the fixed point: anchor == body at constant velocity for
                    // every frame dt, and tau 0 == welded exactly.
                    sled_cam_anchor = render::cam_lag_step(
                        sled_cam_anchor, anchor_pos, anchor_vel, clamped_dt,
                        tau_pos);
                    const double kf =
                        tau_fwd > 0.0 ? 1.0 - std::exp(-clamped_dt / tau_fwd)
                                      : 1.0;
                    const glm::dvec3 c = glm::normalize(sled_cam_fwd);
                    const double gap = std::acos(
                        std::clamp(glm::dot(c, sfwd), -1.0, 1.0));
                    if (gap > 1e-9 && gap < PI - 1e-6) {
                        const glm::dvec3 axis =
                            glm::normalize(glm::cross(c, sfwd));
                        sled_cam_fwd = glm::normalize(
                            glm::angleAxis(gap * kf, axis) * c);
                    } else if (gap >= PI - 1e-6) {
                        sled_cam_fwd = sfwd;  // directionless at pi: snap
                    }
                }
                anchor_pos = sled_cam_anchor;
                sup = glm::normalize(anchor_pos);
                sfwd = sled_cam_fwd - glm::dot(sled_cam_fwd, sup) * sup;
                const double sl = glm::length(sfwd);
                sfwd = sl > 1e-9 ? sfwd / sl : sled_cam_fwd;
                sled_cam_fwd = sfwd;
            }
            // ★ DRIVE-2 FIX (Chad: "freelook doesn't work, can't see the
            // sled but from behind"). SPACE + mouse ORBITS the camera
            // around the machine (the weight input holds -- the deltas are
            // lent to the camera upstream); release eases back behind. The
            // rig's head follows these same angles (§9d: camera owns look).
            // ★ R5 ROW 2 FREELOOK ELEVATION FENCE. He hunts aircraft from the
            // seat AND gets attacked from above -- the old +1.1 rad (63 deg)
            // ceiling lost any plane that climbed over him. Ceiling is
            // NEAR-vertical, NOT pi/2: pose.up = sup below, so at exactly 90
            // deg the view direction becomes parallel to the up-vector and the
            // view matrix degenerates (spins / NaNs). The ~7 deg margin is
            // load-bearing -- do NOT "clean it up" to pi/2. Floor: -0.55 rad,
            // not the -0.6 first proposed -- eye height off the CG is
            // cam_high + cam_dist*sin(el), and at -0.6 the RIDE and CHASE
            // presets put the eye ~4 cm BELOW the running surface (CG sits
            // 0.564 m up); -0.55 keeps >= ~0.16 m of snow clearance on every
            // preset while still showing the track beside the machine.
            constexpr double kSledLookElMax = 1.45;   // rad, ~83 deg up
            constexpr double kSledLookElMin = -0.55;  // rad, ~31.5 deg down
            if (smoke_frames == 0 && live.freelook_held) {
                sled_cam_az += live.mouse_dx * 0.006;
                sled_cam_el = std::clamp(sled_cam_el + live.mouse_dy * 0.004,
                                         kSledLookElMin, kSledLookElMax);
                // ★★★ THE BORED HEAD's clock (see sled_look_held_s). It runs
                // on the FRAME dt the ease-back below already uses, and only
                // while the look is actually held: releasing SPACE re-arms the
                // full two seconds of obedience for the next look.
                sled_look_held_s += clamped_dt;
            } else if (smoke_frames == 0) {
                const double ease = std::min(1.0, 6.0 * clamped_dt);
                sled_cam_az -= sled_cam_az * ease;
                sled_cam_el -= sled_cam_el * ease;
                sled_look_held_s = 0.0;
            }
            // ★★★ ST-5 POLISH (a): THE WHEEL ZOOMS THE SLED CAMERA, in
            // freelook AND in the ordinary chase (his sentence says "zoom
            // level should be variable with mouse scroll wheel", not "while
            // holding space"). Persistent, like sled_cam_az/el and for the
            // same reason: a zoom that reset every time he let go of a key
            // would be useless for the job he asked it to do.
            //
            // ⚠ THE MAP OWNS THE WHEEL WHILE IT IS OPEN -- the v4 lesson, and
            // it is already ENFORCED upstream: the map block CONSUMES the
            // wheel (zeroes live.wheel) before this point in the frame, so
            // `!map_open` here is a second lock on a door that is already
            // bolted. It stays, in the open, because the arbitration is the
            // part of this that a future reader must not have to rediscover.
            //
            // The other two claimants: the sting flight cam (gated on
            // PlayerMode::Drone, mutually exclusive with the sled block this
            // sits inside) and the aeroplane's freelook dolly (gated below on
            // being ON the aircraft). No weapon select or HUD page reads the
            // wheel -- input/live_input.cpp holds the ONLY GetMouseWheelMove
            // in the tree, and these four sites are all of its consumers.
            if (smoke_frames == 0 && !map_open && live.wheel != 0.0) {
                sled_cam_zoom *= std::pow(kSledCamZoomStep, -live.wheel);
                live.wheel = 0.0;  // consumed, like the map's
            }
            const glm::dvec3 sleft = glm::normalize(glm::cross(sup, sfwd));
            const glm::dvec3 odir =
                std::cos(sled_cam_az) * sfwd - std::sin(sled_cam_az) * sleft;
            const double ce = std::cos(sled_cam_el), se = std::sin(sled_cam_el);
            // ★ Chad, gi4-ride: "can you actually make my camera track closer
            // to the rider". Was 7.0 back / 2.6 up / 4.0 ahead -- that framed
            // the whole machine but pushed the rider small and put the SNOW he
            // reads the sled off at arm's length. In tight, and the aim point
            // lifted to the rider's shoulders so he is not sitting on the
            // bottom edge of the frame. The orbit radius is the same number,
            // so freelook keeps its shape.
            // SEADS_SLED_CAM="dist,high,ahead" retunes it without a rebuild
            // (the SEADS_SLED_SAG0 / SEADS_SLED_BEAM precedent).
            // ★ R5 ROW 1 VIEW DIAL: keys 1-4 = the four presets Chad swept
            // live in the seat (do not retune these numbers on paper). The
            // geometry that motivates them: vfov 60 deg and the shipped
            // 4.6/2.0/1.0 pitch the eye only ~10.6 deg down -- the frame's
            // bottom edge meets the snow just 2.3 m behind the machine, so he
            // could not see his own tracks. cam_ahead is the dial that buys
            // ground: NEGATIVE aims BEHIND the machine and tips the frame down
            // onto the snow (cam_dist alone just shrinks the rider).
            struct SledCamPreset {
                const char* name;
                double dist, high, ahead;
            };
            static constexpr SledCamPreset kSledCamPresets[4] = {
                // shipped default -- close, forward, horizon
                {"CAM 1 RIDE", 4.6, 2.0, 1.0},
                // same closeness, tilted down onto the snow
                {"CAM 2 TRACK", 4.6, 3.2, -1.5},
                // backed off and up, ski lines separate
                {"CAM 3 CHASE", 9.0, 4.5, -1.0},
                // overhead chase
                {"CAM 4 WIDE", 12.0, 6.5, -2.0},
            };
            double cam_dist, cam_high, cam_ahead;
            if (const char* e = std::getenv("SEADS_SLED_CAM")) {
                // The env var is the LIVE-TUNING dial and it PINS the camera:
                // while set, the frame takes the env values verbatim and the
                // number keys do not fight it. The eased state is pinned too,
                // so a later preset press never whips off a stale value.
                // ★ ST-5 (a): the env dial stays THE AUTHORITY on defaults,
                // so its own fallback triple is the RAISED one -- the preset
                // distance times the new default zoom -- rather than the bare
                // 4.6 the multiplier has since superseded. A shell with
                // SEADS_SLED_CAM set to fewer than three fields therefore gets
                // the same framing as a shell with none, which is the whole
                // point of a default.
                cam_dist = 4.6 * kSledCamZoomDefault;
                cam_high = 2.0;
                cam_ahead = 1.0;
                std::sscanf(e, "%lf,%lf,%lf", &cam_dist, &cam_high, &cam_ahead);
                sled_cam_dist = cam_dist;
                sled_cam_high = cam_high;
                sled_cam_ahead = cam_ahead;
                // A refused key has to SAY so (the 1979 mount rule: a silent
                // no-op reads as a broken key). Env vars persist for the life
                // of the shell, so a stale SEADS_SLED_CAM from an earlier
                // tuning sweep would otherwise kill the dial invisibly. Name
                // the culprit; the key still does NOT move the framing.
                if (smoke_frames == 0) {
                    for (int i = 0; i < 4; ++i) {
                        if (IsKeyPressed(static_cast<int>(KEY_ONE) + i)) {
                            sled_note = "CAM PINNED BY SEADS_SLED_CAM";
                            sled_note_until_s = GetTime() + 1.5;
                        }
                    }
                }
            } else {
                // Gated like the freelook above so the headless smoke /
                // screenshot rig never sees a key.
                if (smoke_frames == 0) {
                    for (int i = 0; i < 4; ++i) {
                        if (IsKeyPressed(static_cast<int>(KEY_ONE) + i) &&
                            sled_cam_preset != i) {
                            sled_cam_preset = i;
                            // Reuse the sled_note transient (the AUTORIGHT /
                            // refused-mount channel) to say which seat he got.
                            sled_note = kSledCamPresets[i].name;
                            sled_note_until_s = GetTime() + 1.5;
                        }
                    }
                }
                // EASE between presets (the sled_cam_az/el ease-back idiom) --
                // a hard cut between 4.6 and 12.0 m is jarring. At rest the
                // scalars SIT on the preset exactly, so an untouched session
                // stays bit-identical.
                const SledCamPreset& p = kSledCamPresets[sled_cam_preset];
                const double ease = std::min(1.0, 6.0 * clamped_dt);
                sled_cam_dist += (p.dist - sled_cam_dist) * ease;
                sled_cam_high += (p.high - sled_cam_high) * ease;
                sled_cam_ahead += (p.ahead - sled_cam_ahead) * ease;
                // ★ ST-5 (a): the wheel's multiplier, applied to the eased
                // preset distance and CLAMPED ON THE PRODUCT. Clamping here
                // rather than at the wheel keeps the stored zoom honest across
                // a preset change: walk RIDE out to the 30 m stop, press 4 for
                // WIDE, and the effective distance stays at the stop instead
                // of jumping to 12 x 1.45.
                cam_dist = std::clamp(sled_cam_dist * sled_cam_zoom,
                                      kSledCamDistMin, kSledCamDistMax);
                sled_cam_zoom = cam_dist / std::max(0.1, sled_cam_dist);
                cam_high = sled_cam_high;
                cam_ahead = sled_cam_ahead;
            }
            // ★ R4c: the SAME framing, around whichever body the frame is
            // about. `anchor_pos` is the machine until he comes off it, so
            // every drive before this rung frames identically.
            //
            // ★★★ G2d (Chad: "view while walking is too zoomed forward --
            // sudburian vanished into the bottom of screen"): the framing
            // numbers were tuned around the SLED's anchor (the CG, 0.564 m
            // up, target at +0.95 = the rider's shoulders). The MAN's anchor
            // is his feet, so the same +0.95 aimed at his WAIST and 4.6 m
            // could not hold a 1.85 m standing body -- he fell off the
            // bottom edge. On foot: pull back and lift the look target to
            // his chest, ON TOP of whichever preset is active, so keys 1-4
            // keep their relative meaning afoot. A pinned SEADS_SLED_CAM
            // stays verbatim (it is the live-tuning dial; pinning means
            // pinning).
            double target_up = 0.95;
            if (cam_on_man && std::getenv("SEADS_SLED_CAM") == nullptr) {
                cam_dist += 3.4;
                cam_high += 0.6;
                target_up = 1.05;
            }
            pose.eye = anchor_pos - odir * cam_dist * ce +
                       sup * (cam_high + cam_dist * se);
            pose.target = anchor_pos + odir * cam_ahead * ce + sup * target_up;
            pose.up = sup;
            // GLTF wiring smoke rig (SEADS_RIGCAM pattern, smoke-only):
            // SEADS_SLEDCAM="dist,az_deg,el_deg" orbits the MACHINE so the
            // channel screenshots (steer/susp/lean signs) can be certified
            // from any angle. az 0 = dead ahead of the sled, + = around its
            // left; el + = above the horizon.
            if (smoke_frames > 0 && std::getenv("SEADS_SLEDCAM") != nullptr) {
                double dist = 6.0, az_deg = 30.0, el_deg = 12.0;
                std::sscanf(std::getenv("SEADS_SLEDCAM"), "%lf,%lf,%lf", &dist,
                            &az_deg, &el_deg);
                const double az = az_deg * (PI / 180.0);
                const double el = el_deg * (PI / 180.0);
                const glm::dvec3 sleft = glm::normalize(glm::cross(sup, sfwd));
                const glm::dvec3 dir = (std::cos(el) * (std::cos(az) * sfwd +
                                                        std::sin(az) * sleft) +
                                        std::sin(el) * sup);
                pose.eye = sdraw.position + dir * dist + sup * 0.4;
                pose.target = sdraw.position + sup * 0.4;
            }
            // ★★★ G2h: THE MAN CAM (SEADS_MANCAM="dist,az_deg,el_deg,up_m",
            // SMOKE ONLY, the SEADS_SLEDCAM pattern one block up). The orbit
            // above frames the MACHINE, and the whole point of the gait rungs
            // is a man who WALKS AWAY FROM IT -- by the time a posture is
            // worth judging he is out of that frame, and the chase camera
            // (which is what SEADS_SLED_CAM pins) can only ever look at his
            // BACK. A back view cannot answer "is the torso straight" or "is
            // the head plumb", which is exactly the question G2h was given.
            // So: the same orbit construction, around whichever body the
            // frame is about (the MAN whenever he is off the machine), with
            // az measured off HIS heading -- az 90 is a clean side profile,
            // az 0 is head-on from in front. Does not exist outside --smoke,
            // and it is the LAST word on the pose so it beats the pins above.
            if (smoke_frames > 0 && std::getenv("SEADS_MANCAM") != nullptr) {
                double dist = 4.0, az_deg = 90.0, el_deg = 5.0, up_m = 1.0;
                std::sscanf(std::getenv("SEADS_MANCAM"), "%lf,%lf,%lf,%lf",
                            &dist, &az_deg, &el_deg, &up_m);
                const double az = az_deg * (PI / 180.0);
                const double el = el_deg * (PI / 180.0);
                const glm::dvec3 dir =
                    (std::cos(el) *
                         (std::cos(az) * sfwd + std::sin(az) * sleft) +
                     std::sin(el) * sup);
                pose.eye = anchor_pos + dir * dist + sup * up_m;
                pose.target = anchor_pos + sup * up_m;
                pose.up = sup;
                if (loop.tick_count % 120 == 0)
                    std::fprintf(stderr,
                                 "MANCAM d=%.2f az=%.0f el=%.0f up=%.2f eye_h="
                                 "%.2f tgt_h=%.2f |eye-tgt|=%.2f mode=%d\n",
                                 dist, az_deg, el_deg, up_m,
                                 glm::length(pose.eye) - glm::length(anchor_pos),
                                 up_m,
                                 glm::length(pose.eye - pose.target),
                                 static_cast<int>(walker.mode));
            }
        }
        // ── STING RPAS CAMERAS (the sting lane). Two views, both overriding
        // whatever the sled/man block above framed, both losing to the flak
        // sight below by state (OnGun and Drone are mutually exclusive modes,
        // and the shoulder needs Afoot).
        //
        // SHOULDERED: an over-the-shoulder aim view down the launcher -- the
        // mouse steers sting_aim_az/el (routed above), the crosshair is the
        // aim, and the launch click fires along exactly this ray.
        // ★ SMOKE HOOK for the third-person orbit (the SEADS_FLAK_LOOK class):
        // SEADS_STING_LOOK="az_deg,el_deg,dist_m" forces the pull-out view and
        // pins its angles, because SPACE is unreachable in a headless run and
        // this camera is otherwise the one thing in the rung a screenshot
        // could not certify. Read once.
        static const char* sting_look_env = std::getenv("SEADS_STING_LOOK");
        const bool sting_look_smoke = smoke_frames > 0 && sting_look_env;
        if (sting_shouldered && (smoke_frames == 0 || sting_look_smoke)) {
            glm::dvec3 spos, shead;
            double shh;
            sting_stance(spos, shead, shh);
            const glm::dvec3 lup = glm::normalize(spos);
            glm::dvec3 h = shead - glm::dot(shead, lup) * lup;
            const double hl = glm::length(h);
            h = hl > 1e-6
                    ? h / hl
                    : glm::normalize(glm::cross(lup, glm::dvec3{1.0, 0.0, 0.0}));
            const glm::dvec3 sleft = glm::normalize(glm::cross(lup, h));
            if (sting_look_smoke) {
                double azd = 0.0, eld = 12.0, dm = 1.5;
                std::sscanf(sting_look_env, "%lf,%lf,%lf", &azd, &eld, &dm);
                sting_look_az = azd * (PI / 180.0);
                sting_look_el = eld * (PI / 180.0);
                sting_look_dist = dm;
            }
            if (live.freelook_held || sting_look_smoke) {
                // ★★★ THIRD PERSON, ANCHORED ON THE MAN. The whole point is
                // that the deploy is WATCHABLE (Chad's "it should be
                // automatically a wider 3rd person view"), so the anchor is
                // his chest -- 1 m up the stance -- and not the launcher, the
                // aim ray or the head: those all swing, and a camera hung off
                // a swinging thing cannot show the swing.
                //
                // The orbit is the sled's arithmetic verbatim (odir composed
                // from az about the local up, elevation lifting the eye), so
                // the three free-look cameras in this game answer the mouse
                // identically. Target is the anchor itself rather than the
                // sled's look-ahead point: at 1.5 m an ahead-aim would push
                // the man to the edge of frame.
                const glm::dvec3 anchor = spos + lup * 1.0;
                const glm::dvec3 odir = std::cos(sting_look_az) * h -
                                        std::sin(sting_look_az) * sleft;
                const double oce = std::cos(sting_look_el),
                             ose = std::sin(sting_look_el);
                // A SHOULDER OFFSET, and it is not decoration. Straight
                // behind at 1.5 m ("about 5 feet", his number) the helmet
                // sits dead centre and hides the very swing this view exists
                // to show. Sliding the eye 0.45 m off the orbit's own right
                // -- the same 0.45 the over-the-shoulder aim view already
                // uses, and off the ORBIT's right rather than his heading's so
                // it holds all the way round -- puts the man on the left of
                // frame and the launcher clear. Target lifted a quarter metre
                // for the same reason: aiming at the chest from chest height
                // fills the bottom half with helmet.
                const glm::dvec3 oleft = glm::normalize(glm::cross(lup, odir));
                pose.eye = anchor - odir * sting_look_dist * oce +
                           lup * (sting_look_dist * ose) - oleft * 0.45;
                pose.target = anchor + lup * 0.25;
                pose.up = lup;
            } else {
                // The over-the-shoulder AIM view, unchanged -- this is the
                // sight picture the launch click fires along.
                const double ce = std::cos(sting_aim_el),
                             se = std::sin(sting_aim_el);
                const glm::dvec3 aim = ce * (std::cos(sting_aim_az) * h -
                                             std::sin(sting_aim_az) * sleft) +
                                       se * lup;
                const glm::dvec3 head = spos + lup * shh;
                pose.eye = head - aim * 1.6 + lup * 0.35 - sleft * 0.45;
                pose.target = head + aim * 60.0;
                pose.up = lup;
            }
        }
        // FLYING: the chase cam behind the drone (Chad: "you are in a chase
        // cam to a blender modelled sting like model"), anchored on the
        // CARRIED AIM — the plane's own camera law ("camera rotation = raw
        // aim, 1:1, instantly"): sweep the mouse and the view sweeps with the
        // reticle, the drone banking into frame as the cascade chases it.
        // Camera-up = the aim frame's carried up (never rebuilt from
        // local_up — the zenith pole), exactly as the aircraft camera reads
        // it. SEADS_STING_CAM="dist,high,ahead" stays the live-tuning dial.
        if (sting.st.active && player.mode == app::PlayerMode::Drone) {
            const glm::dvec3 spos = sting_draw_state.position;  // CAM-SMOOTH
            const glm::dvec3 adir = sting.st.aim.forward();
            const glm::dvec3 aup = sting.st.aim.up();
            // SPACE freelook orbit (fly-2): held = the deltas already
            // accrued upstream; released = ease back behind the aim, the
            // sled camera's own ease shape.
            if (smoke_frames == 0 && !live.freelook_held) {
                const double ease = std::min(1.0, 6.0 * clamped_dt);
                sting_cam_az -= sting_cam_az * ease;
                sting_cam_el -= sting_cam_el * ease;
            }
            const glm::dvec3 aleft = glm::normalize(glm::cross(aup, adir));
            const glm::dvec3 odir_h = std::cos(sting_cam_az) * adir -
                                      std::sin(sting_cam_az) * aleft;
            const double oce = std::cos(sting_cam_el),
                         ose = std::sin(sting_cam_el);
            double cd = sting_cam_dist, ch = 2.2, ca = 14.0;
            if (const char* e = std::getenv("SEADS_STING_CAM")) {
                // The env dial PINS all three (the SEADS_SLED_CAM rule) —
                // sync the wheel state so clearing it never whips the view.
                std::sscanf(e, "%lf,%lf,%lf", &cd, &ch, &ca);
                sting_cam_dist = cd;
            }
            pose.eye =
                spos - odir_h * cd * oce + aup * (ch + cd * ose);
            pose.target = spos + odir_h * ca * oce + aup * 0.3;
            pose.up = aup;
        }
        // ── FLAK F-SIGHT (spec §7.5): while manned, the camera rides the
        // sight axis kSightEyeBackM behind st_eye -- close enough to be the
        // gunner's head, which is what Chad asked for after his 2026-08-30
        // fly (it was 2.9 m and his mitts floated mid-air). Nothing of the
        // man is drawn from here (his 2026-08-31 ruling: a clean sight
        // picture); free-look instead eases the camera OUT to the pullout
        // below, where he IS drawn. The global 2 m near plane would clip
        // the sight assembly from this close, so the 3D pass swaps in a
        // close near plane for THIS view only (render/draw.cpp). Eye moved
        // ALONG the axis only => the pipper stays parallax-true. Up = the
        // cradle's carried +Y (no roll, ever). Overrides the sled cam (you
        // man FROM the sled); the smoke-only debug cams below still win in
        // a smoke run.
        if (flak_fk.manned) {
            render::flak::Stations fst;
            if (render::flak_stations(fst)) {
                const render::flak::SightCamera sc = render::flak::sight_camera(
                    flak_fk.mount, fst, flak_fk.pose);
                // FREE LOOK: yaw/pitch the LOOK DIRECTION about the gunner's
                // eye point (a head turn, not an orbit) -- the eye stays
                // anchored on the pulled-back sight axis so releasing SPACE
                // eases back onto an unchanged sight picture. The head yaws
                // about the LOCAL VERTICAL (mount up), never the cradle's
                // carried +Y -- yawing about the tilted cradle up rolls the
                // horizon (~15 deg at 18 deg elevation, near-vertigo at 87),
                // caught by the az=-120 certification screenshot. The camera
                // up cross-fades sight-up -> vertical over the first ~17 deg
                // of head travel, so the AT-REST sight picture (signed
                // no-roll) is bit-identical and the ease-back lands on it.
                const glm::dvec3 f = sc.forward;
                const glm::dvec3 uv = flak_fk.mount.up;  // local vertical
                // Rodrigues about a unit axis (a true rotation -- a linear
                // blend of f and a fixed right axis degenerates at az=90).
                const auto rot = [](const glm::dvec3& v, const glm::dvec3& k,
                                    double c, double s) {
                    return v * c + glm::cross(k, v) * s +
                           k * glm::dot(k, v) * (1.0 - c);
                };
                // az + = look right = NEGATIVE rotation about up (RH rule).
                const glm::dvec3 fy = rot(f, uv, std::cos(flak_cam_az),
                                          -std::sin(flak_cam_az));
                const glm::dvec3 ry = glm::normalize(glm::cross(fy, uv));
                // el + = look up = positive rotation about the yawed right.
                const glm::dvec3 look = rot(fy, ry, std::cos(flak_cam_el),
                                            std::sin(flak_cam_el));
                const double head =
                    std::min(1.0, (std::abs(flak_cam_az) +
                                   std::abs(flak_cam_el)) / 0.3);
                pose.eye = sc.eye - f * render::flak::kSightEyeBackM;
                // ── RMB ZOOM = PURE MAGNIFICATION DOWN THE AIM (Chad's
                // 2026-08-30 RULING: "zoom should not anchor to bandit,
                // only pipper, no auto aim"). "My pipper" = the SIGHT
                // RETICLE. The round-2 lens_shift fade (below, at the
                // FrameInfo fill) already puts the reticle dead centre at
                // full zoom; the extra ease-onto-the-solved-lead blend that
                // shipped with it STEERED THE VIEW onto whatever bandit the
                // sight had latched -- Chad's "zoom went to a dot in the
                // sky". REMOVED: the zoom never moves the look axis; it
                // narrows the FOV about the parallax-true sight axis and
                // nothing else. (lookz survives as the name the shake block
                // rides; it IS the look axis now.)
                const glm::dvec3 lookz = look;
                pose.target = pose.eye + lookz * 100.0;
                pose.up = glm::normalize(sc.up * (1.0 - head) + uv * head);
                // ── STAGE B CAMERA SHAKE: each round jolts the SIGHT PICTURE
                // and nothing else. A pure VIEW offset applied LAST, after
                // the sight axis and the free-look head rotation are both
                // solved -- so it rides both paths and neither can read it
                // back. The gun's demand/pose, the bore and the pipper
                // solution are computed elsewhere and never see this.
                //
                // ★ At rest (no shots) flak_shake is EXACTLY 0.0 and this
                // block does not execute -- the signed at-rest sight picture
                // is bit-identical. Sub-milliradian by design (the gunner is
                // strapped into two shoulder pads); dials in flak_gun.h.
                //
                // ★★ VERDICT-ROUND-2 FIX (Chad 2026-08-29: "zoom still
                // magnifies the bore"): the jolt must ride the ZOOM-BLENDED
                // axis `lookz`, never the raw `look`. This block used to
                // rebuild the view from `look` and re-write pose.target,
                // silently DISCARDING the zoom-on-pipper blend above --
                // and it runs essentially always in combat, because
                // fk_env_step's pure exponential decay keeps flak_shake
                // strictly > 0.0 for ~60 s of real time after any shot
                // (denormal underflow is its only zero). Unzoomed / lead-
                // invalid, lookz == look, so the shipped shake picture is
                // bit-identical by construction.
                // ★ Degenerate-basis guard (red-team, verdict round 2): a
                // near-overhead lead can pull the zoom-blended lookz almost
                // parallel to pose.up, and normalize(cross(...)) would go
                // NaN and poison pose.eye/target for the whole shake tail.
                // The jolt is pure cosmetics — skip it on that edge; the
                // zoom blend above (already written) stands untouched.
                const glm::dvec3 shake_rv = glm::cross(lookz, pose.up);
                if (flak_shake > 0.0 &&
                    glm::dot(shake_rv, shake_rv) > 1e-8) {
                    const render::flak::ShakeSample sh =
                        render::flak::shake_sample(
                            static_cast<int>(flak_shot_seq & 0x3fffffffLL),
                            flak_shake);
                    const glm::dvec3 rgt = glm::normalize(shake_rv);
                    const glm::dvec3 upn = glm::cross(rgt, lookz);
                    // First-order rotation: at < 1 mrad the small-angle form
                    // and a true Rodrigues differ below a millionth of a
                    // pixel, and this one cannot degenerate.
                    const glm::dvec3 jl = glm::normalize(
                        lookz + rgt * sh.yaw_rad + upn * sh.pitch_rad);
                    pose.eye += rgt * sh.right_m + upn * sh.up_m -
                                lookz * sh.back_m;
                    pose.target = pose.eye + jl * 100.0;
                }
                // ── F-POSE PULLOUT (Chad's 2026-08-31 ruling): free-look
                // eases the camera OUT behind his shoulder, where the whole
                // crouched Sudburian is drawn; release eases it back onto
                // the sight picture, which stays clean. A pure camera
                // LERP applied LAST, after the sight axis, the free-look
                // head turn and the shake are all solved -- so at
                // flak_ext_t == 0 every one of those is bit-identical and
                // this block cannot touch the aim (free-look already holds
                // the gun demand where it was).
                if (flak_ext_t > 0.0) {
                    const render::flak::ExternalCamera xc =
                        render::flak::external_camera(
                            flak_fk.mount, flak_fk.pose, flak_cam_az,
                            flak_cam_el, render::flak::kExtDistM);
                    const double t = flak_ext_t;
                    pose.eye = pose.eye * (1.0 - t) + xc.eye * t;
                    pose.target = pose.target * (1.0 - t) + xc.target * t;
                    const glm::dvec3 ub = pose.up * (1.0 - t) + xc.up * t;
                    if (glm::dot(ub, ub) > 1e-12) pose.up = glm::normalize(ub);
                }
            }
        }
        render::FrameInfo info;
        info.fps = GetFPS();
        info.season = current_season;  // [seasons] W1: read-only into render/
        info.plane_viz = plane_viz;    // Feature A: hero-plane viz mode ('N')
        // F9 RECORD: pure HUD read of the app-owned toggle/toast state.
        info.recording = f9_recording;
        info.rec_saved_at_s = f9_saved_at_s;
        std::snprintf(info.rec_saved_name, sizeof info.rec_saved_name, "%s",
                      f9_saved_name);
        // DEBUG scrub: ] forward / [ back, ~4 real-seconds per full day (uses
        // the frame clock, but interactive-only — never on the probe path).
        if (smoke_frames == 0) {
            const double rate = cel.day_period_s / 4.0;
            if (IsKeyDown(KEY_RIGHT_BRACKET))
                cel_time_offset += rate * clamped_dt;
            if (IsKeyDown(KEY_LEFT_BRACKET))
                cel_time_offset -= rate * clamped_dt;
            if (IsKeyPressed(KEY_BACKSLASH)) force_haze = !force_haze;
            // DEBUG 'K': cycle the WEATHER SEASON live
            // (Winter->Spring->Summer-> Autumn) to fly-verify W1 HUD tag / W3
            // precip / W4 snow without a relaunch. Cosmetic + app-owned
            // (read-only into render/, never the kernel); a respawn re-rolls
            // per the [seasons] precedence. Reflect it THIS frame so the
            // HUD/precip/snow (all read below) update at once.
            if (IsKeyPressed(KEY_K)) {
                current_season = static_cast<render::Season>(
                    (static_cast<int>(current_season) + 1) %
                    render::kSeasonCount);
                info.season = current_season;
            }
            // DEBUG 'N': cycle the hero-plane DISPLAY options live — the five
            // visibility modes, then the wingtip SMOKE stop
            // (Mirror->NeonRim->Searchlight->Glow->Halo->Smoke->...). On the
            // Smoke stop the plane wears the default NeonRim look and the
            // airshow trails run; every other stop is smoke-off (Chad
            // 2026-08-06: "make smoke one of the toggleable options when I
            // press N — put it with the others"; V is RESERVED for a future
            // cockpit view). Cosmetic + app-owned (read-only into render/,
            // never the kernel). Reflect it THIS frame so the plane shader /
            // tag / trails update at once.
            if (IsKeyPressed(KEY_N)) {
                const int kViz = static_cast<int>(render::PlaneViz::Count);
                int slot = smoke_on ? kViz : static_cast<int>(plane_viz);
                slot = (slot + 1) % (kViz + 1);
                smoke_on = slot == kViz;
                plane_viz = smoke_on ? render::PlaneViz::NeonRim
                                     : static_cast<render::PlaneViz>(slot);
                info.plane_viz = plane_viz;
            }
            // R5b 'T': toggle the GravityField (escape ceiling) LIVE — the
            // same debug-key class as K/season and \/haze (Chad's ask after
            // the first R5 fly: activation by keypress, with the state
            // readable on sight). Kernel-legal: env is DATA sampled per
            // tick, so the next tick simply flies the other field — no
            // kernel state, no mid-flight seam. Startup state = [gravity]
            // enabled; the dials stay config-owned either way.
            if (IsKeyPressed(KEY_T)) {
                env.grav = env.grav == nullptr ? &grav_field : nullptr;
                env_ptr = env.any_live() ? &env : nullptr;
            }
            // R6: B toggles the AtmosphereField bubble LIVE (same debug-key
            // class — env is DATA sampled per tick, no kernel state, no
            // mid-flight seam). Startup state = [atmosphere] enabled.
            // ★ gated off in drive mode (§9d.5): B is the sled BRAKE hand --
            // a pre-existing double-bind, gated rather than moved.
            if (IsKeyPressed(KEY_B) && !player.off_aircraft()) {
                env.atm = env.atm == nullptr ? &atm_field : nullptr;
                env_ptr = env.any_live() ? &env : nullptr;
            }
            // M-KEY FULL-SCREEN BUBBLE MAP (Chad's ask, 2026-07-25): toggle
            // the top-down faction map overlay LIVE. Same debug-key class as
            // K/T/B — a pure render-layer bool, never sim/control state; the
            // 3D world keeps rendering underneath and flight input stays
            // live while the map is open (Chad can fly blind briefly).
            if (IsKeyPressed(KEY_M)) {
                map_open = !map_open;
            }
            // ★ 'H' IS FREE AGAIN. It used to toggle the helmet frost; the
            // frost is deleted (Chad, 2026-08-17: "no more frost just turn it
            // off"), so the key went with it rather than being left bound to a
            // no-op. Do not quietly reuse H for something else -- Chad has
            // muscle memory on it and a silent rebind is worse than a dead key.
            // ★ 'L' LIGHT dial select, ',' down / '.' up. Steps are ~3-5% of
            // each dial's SHIPPED value, rounded to a clean decimal so the
            // readout is a number Chad can copy straight into world.toml.
            // Clamps are the loader's own admissible ranges (ground_day_gain
            // and moon ground_gain are both checked [0,2]; the night_glow
            // multiplier is floored above 0 because the loader rejects a zero
            // channel -- "0 IS the bug").
            if (IsKeyPressed(KEY_L)) {
                tune_dial = (tune_dial + 1) % 3;
                tune_armed = true;
            }
            // ★ R3 SATURATION SWEEP, LIVE ON PgUp / PgDn (Chad 2026-08-27).
            // The dial the red team named as the prime suspect in "there is no
            // intermediate": coverage is smoothstep(0, full_depth, depth) and
            // the ship 0.70 m saturates BELOW the map's median depth (census
            // p50 0.77 m), so over half the land is clipped and 0.77 m shades
            // identically to 1.75 m. Stepping it from the SEAT -- rather than
            // relaunching per value -- is what makes this rulable in one ride.
            // Clamped to a sane band: 0.10 floor because <= 0 whites out the
            // planet (see render::r3_full_depth_env_override), 3.0 ceiling
            // because the census p99 is 1.75 m and past that every rung looks
            // the same. Steps 0.10 m; SHIFT steps 0.02 m for a fine read near
            // whatever he settles on.
            {
                const int r3_step = IsKeyPressed(KEY_PAGE_UP)     ? 1
                                    : IsKeyPressed(KEY_PAGE_DOWN) ? -1
                                                                  : 0;
                if (r3_step != 0) {
                    const float d = (IsKeyDown(KEY_LEFT_SHIFT) ||
                                     IsKeyDown(KEY_RIGHT_SHIFT))
                                        ? 0.02f
                                        : 0.10f;
                    r3_full_depth = std::clamp(
                        r3_full_depth + d * static_cast<float>(r3_step), 0.10f,
                        3.0f);
                    TraceLog(LOG_INFO, "PLANET: R3 full_depth -> %.2f m",
                             static_cast<double>(r3_full_depth));
                }
            }
            const int tune_step = IsKeyPressed(KEY_PERIOD)  ? 1
                                  : IsKeyPressed(KEY_COMMA) ? -1
                                                            : 0;
            if (tune_step != 0) {
                tune_armed = true;
                if (tune_dial == 0) {
                    tune_day_gain = std::clamp(
                        tune_day_gain + 0.05f * tune_step, 0.0f, 2.0f);
                } else if (tune_dial == 1) {
                    tune_glow_mul = std::clamp(
                        tune_glow_mul + 0.05f * tune_step, 0.05f, 1.50f);
                } else {
                    tune_moon_gain = std::clamp(
                        tune_moon_gain + 0.025f * tune_step, 0.0f, 2.0f);
                }
            }
        }
        // Bezel tag + HUD plate: visible == the field is LIVE this frame
        // (whether from config or the toggle) — "was it on?" must never be
        // a mystery again. The NO RETURN read is the tick's OWN claim latch
        // (R5e, Chad fly-4: crossing E_spec = 0 IS the loss — app::tick
        // severed the controls the tick it latched), so the plate can never
        // disagree with the airframe.
        info.escape_sky = env.grav != nullptr;
        info.escape_claimed = loop.escape_claimed;
        // M-KEY FULL-SCREEN BUBBLE MAP: pure read of the render-layer toggle.
        info.map_open = map_open;
        // ★★★ L4 — THE VIEW AND THE ACTIVE BODY (plan §4.4). The renderer is
        // not allowed to know what a PlayerMode is (render never includes
        // app/), so the mode is RESOLVED here into a position + a forward and
        // the chart simply draws the body it is handed. That is the same split
        // L1 used for the mode table itself: the decision lives where the
        // state is, the effect lives where the drawing is.
        info.map_view = &map_view;
        {
            const bool on_sled = player.driving();
            const bool afoot = player.on_foot();
            info.map_body_valid = on_sled || afoot;
            if (on_sled) {
                info.map_body_pos = sled.position;
                info.map_body_fwd =
                    sled.orientation * glm::dvec3(0.0, 0.0, -1.0);
                info.map_body_tag = "SNOWMACHINE";
            } else if (afoot) {
                // ★ HIS HEADING IS ALREADY A WORLD TANGENT (sim/walker.h), so
                // it needs no re-derivation and the arrow's vertical-hold
                // guard can never fire on him.
                info.map_body_pos = walker.pos;
                info.map_body_fwd = walker.heading;
                info.map_body_tag = "ON FOOT";
            } else {
                info.map_body_tag = "AIRCRAFT";
            }
            // The bodies he is NOT in get a glyph. The machine only once it
            // has actually been seeded -- before that there is no machine in
            // the world to draw (L1's PlayerModeState::sled_seeded is the ONE
            // place that fact lives).
            info.map_sled_glyph = player.sled_seeded && !on_sled;
            info.map_sled_pos = sled.position;
            info.map_aircraft_glyph = info.map_body_valid;
            info.map_aircraft_pos = loop.curr.position;
            // The Sting in flight, whatever body he is on (Chad 2026-09-04).
            info.map_sting_glyph = sting.st.active;
            info.map_sting_pos = sting.st.curr.position;
            info.map_sting_fwd = sting.st.curr.velocity;
        }
        info.r3_full_depth = r3_full_depth;
        // R6: the bubble state + the LOCAL air fraction (the spatial
        // atm_frac_at the plant flies) — the read-on-sight "am I leaving the
        // breathable air?" cue. Sampled at the interpolated draw position so
        // the plate tracks the felt sag. Null atm => 1 (full air), the plate
        // is hidden (bubble_live false).
        info.bubble_live = env.atm != nullptr;
        info.air_frac = sim::atm_frac_at(draw_state.position, env_ptr, params);
        // S-airdome (spec §1.9): rebuild the GEOMETRY half of AirField from
        // the live sim::AtmosphereField every frame — a copy, never a
        // re-derivation — so the visual dome (sky/ground/star march) can
        // never disagree with the flyable one; growth/shrink/extinction show
        // up with no extra plumbing. Null env.atm (e.g. the B-key toggle) =>
        // enabled=false, 0 bubbles (air_at reads 1.0 everywhere, matching the
        // plant's null-env spatial factor).
        air_field_render.enabled = env.atm != nullptr;
        air_field_render.planet_R = params.R;
        if (env.atm != nullptr) {
            air_field_render.deck_agl_m = env.atm->deck_agl_m;
            air_field_render.deck_soft_m = env.atm->deck_soft_m;
            // S-domeround: the shared roundness dial rides with the rest of
            // the live geometry copy.
            air_field_render.dome_exponent = env.atm->dome_exponent;
            const int n =
                std::min<int>(static_cast<int>(env.atm->bubbles.size()),
                              render::kMaxAirBubbles);
            // Quality fix pass item 7: kMaxAirBubbles silently truncated the
            // sky/ground/star march if the sim ever grew more bubbles than
            // the shader's fixed-size uniform arrays hold. Warn once per
            // truncation onset (edge-triggered on the sim's bubble count
            // itself, not a frame counter, so a later shrink-then-regrow
            // past the cap warns again).
            static int s_last_warned_count = -1;
            if (static_cast<int>(env.atm->bubbles.size()) >
                    render::kMaxAirBubbles &&
                static_cast<int>(env.atm->bubbles.size()) !=
                    s_last_warned_count) {
                TraceLog(LOG_WARNING,
                         "AIRDOME: sim::AtmosphereField has %zu bubbles > "
                         "render::kMaxAirBubbles (%d) -- the sky/ground/star "
                         "march is TRUNCATED to the first %d",
                         env.atm->bubbles.size(), render::kMaxAirBubbles, n);
                s_last_warned_count = static_cast<int>(env.atm->bubbles.size());
            }
            air_field_render.bubble_count = n;
            for (int i = 0; i < n; ++i) {
                const sim::AtmosphereField::Bubble& b = env.atm->bubbles[i];
                render::AirField::Bubble& rb = air_field_render.bubbles[i];
                rb.center_dir = b.center_dir;
                rb.major_axis = b.major_axis;
                rb.a = b.ground_radius_m;
                rb.b = b.minor_radius_m;
                // S-domeround: this uniform slot now carries H (the
                // volume-preserving dome centre height), NOT the literal
                // config ceiling_m -- render/air_field.h's Bubble::ceiling_m
                // comment explains the fold. Same fallback sim/aero.h's
                // atm_frac_at applies: dome_h_m<=0 -> H = ceiling_m.
                rb.ceiling_m = (b.dome_h_m > 0.0) ? b.dome_h_m : b.ceiling_m;
                rb.edge_soft_m = b.edge_soft_m;
                rb.ceil_soft_m = b.ceil_soft_m;
            }
        } else {
            air_field_render.bubble_count = 0;
        }
        info.air_field = air_field_render;
        // T2: the greybox interior is drawn from the SAME net the collision
        // uses — under the SAME gate as env.tunnels (null => no tunnel draw,
        // the shipped default frame unchanged).
        info.tunnel_net = env.tunnels;  // == &tunnel_net iff [tunnel] enabled
        // T5c: the destructible lamp world (dark spots persist). Same gate.
        info.tunnel_lamps = game.tunnel.enabled ? &tunnel_lamps : nullptr;
        // Stage 3a — the MOVING sun: TICK-DERIVED celestial time (never
        // wall-clock; frame-rate independent — tick_count is AT-9-pinned),
        // wheeled by the pure celestial core. render/ never sees a clock. The
        // debug scrub offset is added on top (0 unless a key is held).
        const double t_cel =
            static_cast<double>(loop.tick_count) * params.sim_dt +
            cel_time_offset;
        sun_world = -render::sun_dir(cel, t_cel);  // light-travel = -(to sun)
        info.sun_dir = sun_world;
        info.sun = sunp;
        // ★ SC1 (WINTER_LAW §3.7, spec §2): the night-art blend, MOVED UP from
        // render/draw.cpp's old per-frame local recompute (render/celestial.h
        // carries the authorised-exception ruling for this seam). Same
        // mapping, bit-for-bit (the -0.10/0.05 thresholds + smoothstep) --
        // only the up vector changed, from the camera EYE to the PLAYER
        // (draw_state.position, spec's literal ask), and the input is now the
        // double sun_world already computed above rather than a locally
        // re-derived float. draw.cpp's old block is gone (source-grep leg
        // sled_cold_night_art_recompute_is_gone pins it).
        {
            const glm::dvec3 up_player = glm::normalize(draw_state.position);
            const float sun_el =
                static_cast<float>(glm::dot(sun_world, up_player));
            const float tt = glm::clamp(
                (sun_el - (-0.10f)) / (0.05f - (-0.10f)), 0.0f, 1.0f);
            const float smooth = tt * tt * (3.0f - 2.0f * tt);
            render::set_post_night(1.0f - smooth);
        }
        // ★ THE SC1 HELMET FOG CALL SITE IS GONE (Chad, 2026-08-17: "no more
        // frost just turn it off ... Just how it affects my view"). It used to
        // sit here beside set_post_night, deliberately outside every mode
        // branch. Nothing replaces it: there is no fog uniform left to feed.
        // The COLD MECHANIC above is untouched -- air temp still drives pack
        // hardness and top speed, which is the half of §3.7 he kept.
        // Star field (Stage 4): the sky WHEEL as a FINISHED mat3 (never a raw
        // large t in render/) + the once-mapped [stars] knobs + the celestial
        // basis for the one-time star-mesh build. Same TICK-DERIVED t_cel as
        // the sun, so the ]/[ scrub wheels the whole starfield about Polaris
        // too.
        info.sky_wheel =
            glm::mat3(glm::mat3_cast(render::sky_wheel(cel, t_cel)));
        info.stars = starp;
        info.celestial = &cel;
        // Moon (Stage 5): light-travel dir (moon -> scene, mirroring the sun) +
        // the illuminated fraction (0 new .. 1 full), both pure functions of
        // the same TICK-DERIVED t_cel (no clock; the ]/[ scrub wheels the moon
        // too).
        info.moon_dir = -render::moon_dir(cel, t_cel);
        info.moon_phase_frac = render::moon_illum_fraction(cel, t_cel);
        info.moon = moonp;
        // Aurora (Stage 8): the periodic phase as vec2(sin,cos) of a BOUNDED
        // wrapped angle (fmod keeps it small so float sin is exact; the shader
        // is exactly 2*pi-periodic in it — no strobe). Same tick-derived t_cel.
        const double aur_phi =
            std::fmod(t_cel * aurora_anim_rate, 2.0 * 3.14159265358979323846);
        info.aurora_phase_sc = glm::vec2(static_cast<float>(std::sin(aur_phi)),
                                         static_cast<float>(std::cos(aur_phi)));
        info.aurora = aurp;
        // CC1 Superstack smoke (Living Copper Cliff): the FINISHED wrapped puff
        // phase, frac(t_cel*rate) computed in DOUBLE here so render/ never sees
        // raw t_cel (house law; Fable P1: a float32 t_cel stutters at ~10 h).
        // Same TICK-DERIVED t_cel as the sun — the ]/[ scrub drifts the plume
        // too.
        info.smoke_phase = t_cel * smoke_rate - std::floor(t_cel * smoke_rate);
        // CC2 slag-pot train: the FINISHED wrapped loco-head phase (loco at s =
        // phase*L). frac(t_cel*rate) in DOUBLE so render/ never sees raw t_cel.
        info.train_phase = t_cel * train_rate - std::floor(t_cel * train_rate);
        // CC3 slag pour: the FINISHED wrapped pour phase (pot tip + lava front
        // derive from it). frac(t_cel*rate) in DOUBLE so render/ never sees raw
        // t_cel.
        info.pour_phase = t_cel * pour_rate - std::floor(t_cel * pour_rate);
        // W2a LOCALIZED weather gate (docs/weather_seasons_plan.md W2a): the
        // pure t_cel weather_haze becomes the storm BUDGET, and
        // render::weather_cell localizes it to a patchwork of fixed microsystem
        // cells on the sphere. We sample the field at the EYE's ground-track
        // (normalize(pose.eye)) and feed the ONE local scalar into the SAME
        // uWeatherAmt uniform every consumer already reads (zero fork) — "fly
        // into a squall, it clears as you leave." Same TICK-DERIVED t_cel as
        // the sun (no clock, frame-rate independent; ]/[ scrubs weather).
        //
        // S-airdome (spec §1.7, SUPERSEDES the prior "weather GATES the whole
        // sky" reading): atm.haze_density now carries the RAW weather scalar
        // [0,1] (uWeatherAmt in the shaders), no longer pre-scaled by
        // haze_overcast_density — weather is a local THICKENER on top of the
        // always-on AirField baseline (uAirHazeDensity + uWeatherAmt *
        // uAirWeatherGain, both read from render::AirField), not the on/off
        // gate for atmosphere. Quality fix pass item 7: haze_overcast_density
        // is now wired back as the CEILING on that thickening term
        // (uAirHazeCeiling, set above from air_field_render.haze_overcast_
        // density) — no longer the orphaned key the initial landing left.
        const glm::dvec3 eye_track = glm::normalize(pose.eye);
        // S-bubbleweather (Chad's fly, 2026-08-09: "add more weather as micro
        // events that happen more frequently within the bubble"): the cells
        // are ANCHORED INSIDE the live domes rather than spread over the whole
        // sphere. Built from air_field_render — the SAME mirror of the live
        // sim::AtmosphereField the visual march reads — so the squalls track
        // domes as conquest grows/shrinks/kills them, and weather can never be
        // generated somewhere the air field says there is no air. The
        // gate_weather_by_air call below still stands as the hard guarantee;
        // this makes the storm budget actually BUY weather inside the domes
        // instead of spending it on cells in vacuum that the gate then zeroes.
        render::WeatherAnchor wanchors[render::kMaxAirBubbles];
        int wanchor_count = 0;
        if (air_field_render.enabled && air_field_render.planet_R > 0.0) {
            for (int i = 0; i < air_field_render.bubble_count; ++i) {
                const auto& b = air_field_render.bubbles[i];
                if (b.a <= 0.0) continue;  // an extinct dome anchors nothing
                render::WeatherAnchor& an = wanchors[wanchor_count++];
                an.center_dir = b.center_dir;
                an.radius_rad = b.a / air_field_render.planet_R;
                an.tangent_ref = b.major_axis;  // lock the pattern to the
                                                // bubble's own frame
            }
        }
        double wfield = render::weather_cell(eye_track, t_cel, wparams,
                                             wcparams, wanchors, wanchor_count);
        // S-domeround (docs/airdome_round_spec.md §2, Chad's 2026-08-09
        // ruling: "if there is thin air somewhere then no weather there.
        // Weather only in the bubbles."): gate the SCALAR at the source by
        // the local (spatial-only) air fraction at the eye, so every
        // consumer of wfield/uWeatherAmt agrees -- precip, the HUD/audio,
        // and the `\` force-haze debug key all go quiet in the vacuum gap.
        // render::air_at is already the spatial-only factor, already
        // pinned against sim::atm_frac_at (test_air_field.cpp) -- reuse it
        // rather than re-deriving the ratio inline.
        wfield =
            render::gate_weather_by_air(wfield, pose.eye, air_field_render);
        atm.haze_density = static_cast<float>(wfield);
        // DEBUG '\' toggle: force uWeatherHaze to 1 -- itself air-gated
        // (spec §2: forcing overcast in vacuum must produce nothing).
        if (force_haze)
            atm.haze_density = static_cast<float>(
                render::gate_weather_by_air(1.0, pose.eye, air_field_render));
        info.atmosphere = atm;
        // W3 precipitation: the SAME W2 field the haze uses gates precip
        // (snow/rain only under an active microsystem, fading as you leave).
        // The fall phase is a pure wrapped fn of t_cel at the season-selected
        // rate (frac in DOUBLE so render/ never sees raw t_cel; no clock).
        // S-wetseason (Chad's fly 2026-08-09, "I didnt see any weather in the
        // bubbles"): Winter = snow, EVERY other season = rain. The old
        // "Summer/Autumn dry" rule made half of all spawns structurally unable
        // to show precipitation regardless of the weather field — see the
        // matching season gate in render/precip_draw.cpp, which owns the same
        // ruling and must not fork from this rate.
        //
        // AS-3 (atmosphere rung 2026-09-12): Winter's dial is now a fall SPEED
        // in m/s, not cycles/s, because the NEAR and FAR lattices have
        // different cell sizes and a flake falls exactly one cell per cycle —
        // only a shared SPEED makes the two layers agree. rate = speed / cell.
        const double precip_rate =
            current_season == render::Season::Winter
                ? (world.precip.cell_size_m > 0.0
                       ? world.precip.snow_speed_mps / world.precip.cell_size_m
                       : 0.0)
                : world.precip.rain_rate_hz;
        const double precip_rate_far =
            current_season == render::Season::Winter &&
                    world.precip.veil_cell_size_m > 0.0
                ? world.precip.snow_speed_mps / world.precip.veil_cell_size_m
                : 0.0;
        info.precip_phase =
            t_cel * precip_rate - std::floor(t_cel * precip_rate);
        info.precip_phase_far =
            t_cel * precip_rate_far - std::floor(t_cel * precip_rate_far);
        // AS-2 — MORE SNOW. The precip intensity is no longer the HAZE scalar.
        // It is the SNOWFALL FIELD (render::snowfall_intensity), built beside
        // the haze out of the same machinery: max(squall, flurry, floor), where
        // the squall term IS `wfield`'s weather_cell so the heavy band is
        // unchanged and the Chad-signed [weather]/[weather_cell] distribution
        // is untouched. `atm.haze_density` above still carries `wfield` alone.
        //
        // The SAME air gate as the haze (S-domeround, Chad 2026-08-09: "if
        // there is thin air somewhere then no weather there") — snow in vacuum
        // is nonsense, and the gate is the hard guarantee, not the generator.
        const double snowfield = render::gate_weather_by_air(
            render::snowfall_intensity(eye_track, t_cel, wparams, wcparams,
                                       wanchors, wanchor_count, sfparams),
            pose.eye, air_field_render);
        info.precip_intensity = snowfield;
        // SMOKE-ONLY evidence rig (docs/atmosphere_snow/): force the precip
        // intensity POST air-gate so a screenshot can show a named intensity
        // (a flurry at 0.3, a squall at 1.0) without waiting for the storm
        // budget to cooperate. Never reachable in live play — smoke_frames is
        // 0 there, exactly like SEADS_SMOKE_GEAR / SEADS_SMOKE_FIRE.
        if (smoke_frames > 0 && snow_force_smoke != nullptr) {
            info.precip_intensity =
                std::clamp(std::atof(snow_force_smoke), 0.0, 1.0);
        }
        // W4 seasonal ground (winter snow-cover): active ONLY in Winter (0 =
        // the terrain unchanged otherwise); a shader tint on the planet LAND
        // albedo.
        info.winter_snow =
            current_season == render::Season::Winter
                ? static_cast<float>(world.ground.winter_snow_cover)
                : 0.0f;
        info.snow_albedo = static_cast<float>(world.ground.snow_albedo);
        info.snow_slope_lo = static_cast<float>(world.ground.snow_slope_lo);
        info.ice_albedo = static_cast<float>(world.ground.ice_albedo);
        info.ice_reflect_frac =
            static_cast<float>(world.ground.ice_reflect_frac);
        info.ice_glint_frac = static_cast<float>(world.ground.ice_glint_frac);
        info.night_glow = glm::vec3(world.ground.night_glow);
        // ★ LIGHT TUNE application (see the declarations above). Applied HERE,
        // into the FrameInfo copies only -- `atm`, `moonp` and `world` keep the
        // shipped config values, so nothing outside this frame's render read
        // can see a dialled number. With no key pressed each expression is the
        // base value exactly, so this is bit-identity by construction.
        info.atmosphere.ground_day_gain = tune_day_gain;
        info.moon.ground_gain = tune_moon_gain;
        info.night_glow = tune_glow_base * tune_glow_mul;
        info.tune_armed = tune_armed;
        info.tune_dial = tune_dial;
        info.tune_day_gain = tune_day_gain;
        info.tune_glow_mul = tune_glow_mul;
        info.tune_glow = info.night_glow;
        info.tune_moon_gain = tune_moon_gain;
        info.ice_night_reflect_frac =
            static_cast<float>(world.ground.ice_night_reflect_frac);
        info.snow_sparkle = static_cast<float>(world.ground.snow_sparkle);
        info.snow_sparkle_sharp =
            static_cast<float>(world.ground.snow_sparkle_sharp);
        info.barren_face_dark =
            static_cast<float>(world.ground.barren_face_dark);
        info.barren_face_dark_hi_deg =
            static_cast<float>(world.ground.barren_face_dark_hi_deg);
        info.barren_face_mottle =
            static_cast<float>(world.ground.barren_face_mottle);
        // ★ WINTER S2 THE SURFACE READOUT (WINTER_LAW §2.3): sample the
        // analytic snowpack under the aircraft's GROUND TRACK (the radial
        // sub-point, not the aircraft position) and hand the class + depth to
        // the HUD plate. The app owns the field, render/ only prints it -- so
        // the number on screen is the one the S3 contact patches will read.
        if (snow_field.hf != nullptr) {
            const glm::dvec3 gdir = glm::normalize(draw_state.position);
            info.surface_label =
                world::surface_name(snow_field.surface_at(gdir));
            info.snow_depth_m = snow_field.depth_at(gdir);
            info.surface_corridor_m = -1.0;
            info.surface_near_label = nullptr;
            if (snow_field.lines != nullptr) {
                // A WIDER radius than the physics one, deliberately. The
                // corridor query that feeds depth_at stays at
                // [snowpack] corridor_search_m (~90 m) because the feather and
                // the bank are both done by ~20 m out and S3's contact patches
                // will call it several times per substep -- inflating a physics
                // dial to make a HUD line nicer is how a hot path gets slow for
                // a cosmetic reason. The plate gets its own probe instead.
                constexpr double kHudCorridorProbeM = 400.0;
                const world::LineHit lh =
                    snow_field.lines->nearest(gdir, kHudCorridorProbeM);
                if (lh.found) {
                    info.surface_corridor_m = lh.dist_m;
                    // ★ Chad's fly-1 F3: report the depth ON the corridor,
                    // sampled at the closest point of its CENTERLINE, so a 5 m
                    // trail can be read without being flown down at 150 m/s.
                    // Exact -- depth_at(foot) is the same function the contact
                    // patches will call, not an approximation of it.
                    info.surface_corridor_depth_m =
                        snow_field.depth_at(lh.foot);
                    // Name the nearest corridor by the SAME class table the
                    // classifier uses, so the plate cannot disagree with
                    // surface_at about what a corridor is.
                    info.surface_near_label = world::surface_name(
                        world::is_plowed(lh.kind)
                            ? world::Surface::Road
                            : (lh.kind == world::LineKind::TrailMain
                                   ? world::Surface::TrailMain
                                   : world::Surface::TrailTributary));
                }
            }
        }
        // ★★ WINTER S3 THE SLED DASH. A pure read of sim::SledState -- the
        // numbers on the plate are the numbers the contact patches produced,
        // never a second evaluation of the world.
        info.sled_note = (sled_note != nullptr && GetTime() < sled_note_until_s)
                             ? sled_note
                             : nullptr;
        // ★ L1: the diegetic prompt. While he is actually turning the wrench
        // the offer changes to the way OUT of it -- the site is the same site,
        // but the sentence a player needs is not.
        info.interact_prompt =
            player.mode == app::PlayerMode::Repairing
                ? "U  STOP FIXING"
                : interact_line;
        // ★ L2: and how far along the fix is. Published by the SIM TICK
        // (repair_frac_hud), read here -- the draw never recomputes it.
        info.repair_frac = repair_frac_hud;
        // ★ L10: and WHICH job the bar is counting. Read from the mode state,
        // never from which tick block happened to write the fraction -- the
        // caption and the progress must name the same job.
        info.repair_what = player.repair_target == app::RepairTarget::Engine
                               ? "ENGINE"
                               : "PUMP";
        // ★★★ L10b -- WHO MAY SEE THE ENGINE OUT PLATE (Chad 2026-09-08: "yes
        // make the plate visible afoot too"). The rule lives HERE because the
        // player mode lives here; render/ asks no questions about who is
        // driving what (render/draw.h's `show_engine_out` banner).
        //
        // Everything BUT the drone, and the drone is not a fussy exclusion: a
        // sting flight fills the screen with ANOTHER aircraft's camera, and a
        // plate reading ENGINE OUT over it would be read as that aircraft's
        // engine. It is the one mode where the sentence would say something
        // false. In the cockpit, on the machine, on his feet, at the gun and
        // mid-wrench, it says what it has always said.
        info.show_engine_out = player.mode != app::PlayerMode::Drone;
        info.sled_active = player.off_aircraft() && player.sled_seeded;
        if (info.sled_active) {
            info.sled_speed_ms = sled.ground_speed_ms;
            info.sled_plane_frac = sled.plane_frac;
            info.sled_roost_flux = sled.roost_flux;
            info.sled_depth_m = sled.depth_under_m;
            info.sled_surface = world::surface_name(sled.surface);
            info.sled_rolled = sled.rolled;
            for (int i = 0; i < sim::kPatches; ++i) {
                info.sled_susp_x[i] = sled_draw.susp_x[i];
                info.sled_susp_v[i] = sled_draw.susp_v[i];  // R1a defect 8
                info.sled_sink_m[i] = sled_draw.sink_m[i];
            }
            // CAM-SMOOTH: the drawn machine is the sub-tick blend.
            info.sled_pos = sled_draw.position;
            info.sled_basis = glm::mat3_cast(sled_draw.orientation);
            // ★ THE SCARF CHANNEL (docs/SCARF_SPEC.md §4). The kernel's own
            // velocity plus the sim's own clock: fr.ticks is the fixed number
            // of ticks step_frame just consumed (the same count the wheel roll
            // and t_cel integrate on), so the scarf advances on TICKS and never
            // on wall time. render/ rotates the velocity into the body frame.
            info.sled_vel_ms = sled.velocity;
            // ★ R4a: angular_vel is ALREADY the body frame (sim/sled.h) --
            // render must NOT rotate it again.
            info.sled_omega_body_rps = sled.angular_vel;
            info.sled_epoch = sled_epoch;
            info.sled_ticks = fr.ticks;
            info.sled_tick_no = sled_tick_no;  // SK-1c audit item 1
            info.sled_dt_s = params.sim_dt;
            info.sled_cg_h = sled_params.cg_height_m;
            // ★★★ R4a THE ARMING RUNG: the mass budget the rider-load rod
            // model (render/rider_load.h) is baked from. Pure reads of the
            // same SledParams the kernel is stepping with this tick.
            info.sled_rider_mass_kg = sled_params.rider_mass_kg;
            info.sled_mass_kg = sled_params.mass_kg;
            info.sled_cg_height_m = sled_params.cg_height_m;
            // ★ R4a instrument: the contact story the rider-load selector
            // cannot see. Pure reads of shipped state, no `dbg` sink.
            info.sled_air_s = sled.air_s;
            info.sled_air_grace_s = sled_params.comfort.rolled_grace_s;
            info.sled_hull_engage = sled.hull_engage_lp;
            info.sled_susp_sum_m =
                sled.susp_x[0] + sled.susp_x[1] + sled.susp_x[2];
            // ★★★ R4a §7.3 STAGE 4: the latch and the snow. A straight read of
            // the kernel's own state -- render does not decide whether he let
            // go and holds no second copy of the answer.
            info.sled_grip_attached = sled.grip.attached;
            info.sled_walker = &walker_draw;  // CAM-SMOOTH: blended
            info.sled_gait = &gait;  // ★★★ GAIT LADDER G1
            // ★★★ G2i / G2j: the hop height and the work pose. Plain values
            // off the pure states stepped above -- render authors none of it.
            info.sled_hop_height_m = hop.height_m;
            info.sled_work_on = work.on;
            info.sled_work_phase = work.phase;
            info.sled_work_blow_on = work.blow_s > 0.0;
            info.sled_work_blow =
                sim::repair_blow_swing(work.blow_s, work_params.blow_s);
            info.footprints = footprints;  // ★ G2g footsteps ring
            info.footprint_n = footprint_n;
            info.footprint_head = footprint_head;
            info.sled_rest = sled_params.susp_rest_m;
            info.sled_stance = sled_params.stance_m;
            info.sled_ski_fwd = sled_params.ski_fwd_m;
            info.sled_track_aft = sled_params.track_aft_m;
            // ★ GLTF WIRING RUNG: the rig channels, straight off SledState
            // (steer_actual is the rate-limited bar the kernel steered
            // with; rider_* are the §9d.7 real displacements). No second
            // animation state -- these ARE the physics numbers.
            // ★ CAM-SMOOTH: the rider and the bars are DRAWN on the blended
            // machine, so they read the blend too (red-team F2: a steer or
            // weight-shift slew across a 1-/3-tick frame hopped the man half a
            // tick against the sled he sits on). HUD readers below stay raw.
            info.sled_steer = sled_draw.steer_actual;
            info.sled_hud_xray = sled_hud_xray;  // SK-1c
            // ★ R2c-5: the arms answer the controls. Same numbers as sin_.
            info.sled_throttle = sled_thumb;
            // R4a: the self-right's brace/push side, for the leg animation.
            info.sled_right_push = sled.right_shift_cmd;
            info.sled_brake = sled_brake_cmd;
            info.sled_rider_lat_m = sled_draw.rider_lat_m;
            info.sled_rider_fwd_m = sled_draw.rider_fwd_m;
            info.sled_rider_up_m = sled_draw.rider_up_m;
            // D3.ii weight dot: normalize to the LIVE reach box -- the box
            // opens as the rider stands (lean_lat seated->stand), forward
            // and aft reaches differ, so the normalization lives HERE with
            // the params, and render/ draws a pure [-1,1] dot.
            {
                const double stand_frac = std::clamp(
                    sled.rider_up_m / sled_params.stand_rise_m, 0.0, 1.0);
                const double lat_max =
                    sled_params.lean_lat_seated_m +
                    stand_frac * (sled_params.lean_lat_stand_m -
                                  sled_params.lean_lat_seated_m);
                const double fwd_max = sled.rider_fwd_m >= 0.0
                                           ? sled_params.lean_fwd_max_m
                                           : sled_params.lean_aft_max_m;
                info.sled_weight_x =
                    std::clamp(sled.rider_lat_m / lat_max, -1.0, 1.0);
                info.sled_weight_y =
                    std::clamp(sled.rider_fwd_m / fwd_max, -1.0, 1.0);
                info.sled_stand_frac = stand_frac;
            }
            // Head follows the freelook camera (§9d: camera owns look).
            // Model yaw is +LEFT; orbiting the view right (az+) turns the
            // head right = negative model yaw.
            //
            // ★★★ THE BORED HEAD (Chad 2026-09-07, see sled_look_held_s).
            // A man holds a look over his shoulder for a couple of seconds and
            // then faces front again -- and while he does, the player finally
            // gets to look AT him instead of at the back of a head that keeps
            // turning away. Two seconds obeying the law above, then the follow
            // weight eases 1 -> 0 over two more, smoothstep so the turn starts
            // and ends soft (a linear ramp reads as a servo, not a neck).
            // The CAMERA is untouched: it keeps answering the mouse for as
            // long as he holds it. Weight is exactly 1.0 for the whole hold
            // window, so a look shorter than two seconds is bit-identical to
            // what shipped before, and it is 1.0 whenever the look is not
            // held (the clock is 0 then, and the angles are easing home).
            constexpr double kHeadBoredHoldS = 2.0;  // obey, then get bored
            constexpr double kHeadBoredTurnS = 2.0;  // the turn to front
            double head_follow = 1.0;
            if (sled_look_held_s > kHeadBoredHoldS) {
                const double u = std::clamp(
                    (sled_look_held_s - kHeadBoredHoldS) / kHeadBoredTurnS,
                    0.0, 1.0);
                head_follow = 1.0 - u * u * (3.0 - 2.0 * u);
            }
            info.sled_cam_yaw = -sled_cam_az * head_follow;
            info.sled_cam_pitch = -0.3 * sled_cam_el * head_follow;
            // ★ SC1 (WINTER_LAW §3.7, spec §4). Pure reads off the SAME
            // sled_params.air_temp_c the kernel just consumed this tick --
            // never a second evaluation of world::air_temp_c against a
            // possibly-different sun_sin_elev/night_phase pair.
            info.sled_cold_valid = world.cold.enabled;
            info.sled_air_temp_c = sled_params.air_temp_c;
            // SOFT SNOW note: air_temp_c > t_ref_c + 2 (spec §4's literal
            // threshold).
            info.sled_soft_note =
                world.cold.enabled &&
                sled_params.air_temp_c > world.cold.t_ref_c + 2.0;
            // frost = world::frost (spec §4): NO negation, single-sourced so
            // the HUD and the leg 8 unit test read the SAME formula.
            // world.cold.enabled=false pins frost at 0 (air_temp_c ==
            // t_ref_c, above t_night_c, so the smoothstep floors).
            info.sled_frost = world::frost(sled_params.air_temp_c, world.cold);
            // ★ The helmet-fog accumulator lived here and is DELETED with the
            // effect it fed. Worth recording why it was so hard to catch: it
            // integrated on `frame_dt`, WALL CLOCK -- so the smoke harness, at
            // ~370 fps, only ever accumulated ~1.5 s of fog no matter how many
            // frames it was given, and could not see the blinder Chad was
            // driving into. If you ever measure a time-integrated LOOK again,
            // check what clock it integrates on before trusting a green run.
        }
        // ★ R5 rows 4/5/6 — ROOST / EXHAUST / RIDER BREATH: one mechanism
        // (render/sled_plumes.h), advanced here off the SAME SledState fields
        // the dash just published (roost_flux is §3.5a's "one number, two
        // consumers" — reading it IS the anti-fork rule; its available_snow
        // term already carries the track's own GroundSample depth). Runs even
        // when the sled goes inactive so leftover puffs age out instead of
        // freezing mid-air (gains 0 there = age-only, no emission).
        //
        // Dials (the SEADS_TRACK_PACK / SEADS_SLED_CAM precedent — read once,
        // validated, warn-and-default, TraceLog'd): SEADS_ROOST /
        // SEADS_EXHAUST / SEADS_BREATH, each a [0,4] rate multiplier, default
        // 1. 0 kills its effect exactly (the A/B baseline arm); the ring caps
        // in sled_plumes.h bound the cost at any setting.
        {
            static const auto plume_dial = [](const char* name) {
                const char* e = std::getenv(name);
                if (e == nullptr) return 1.0;
                const double v = std::atof(e);
                if (v < 0.0 || v > 4.0) {
                    TraceLog(LOG_WARNING,
                             "PLUMES: %s=\"%s\" outside [0,4] -- IGNORED, "
                             "using 1.0",
                             name, e);
                    return 1.0;
                }
                TraceLog(LOG_INFO, "PLUMES: %s=%.2f", name, v);
                return v;
            };
            static const double roost_gain = plume_dial("SEADS_ROOST");
            static const double exhaust_gain = plume_dial("SEADS_EXHAUST");
            static const double breath_gain = plume_dial("SEADS_BREATH");
            static const double poof_gain = plume_dial("SEADS_POOF");
            render::SledPlumeInputs pin;  // defaults = inert (gains 1, all
                                          // keys 0 -> only the idle wisp)
            if (info.sled_active) {
                pin.pos = sled_draw.position;
                pin.basis = info.sled_basis;
                pin.vel = sled_draw.velocity;
                pin.up = glm::normalize(sled_draw.position);
                pin.ground_speed = sled.ground_speed_ms;
                pin.roost_flux = sled.roost_flux;
                pin.throttle = sled_thumb;
                pin.roost_gain = roost_gain;
                pin.exhaust_gain = exhaust_gain;
                pin.breath_gain = breath_gain;
                // Row 6 anchor: the posed Sudburian's neck from the LAST
                // sled_model_draw (render::sled_model_rider_back — the same
                // published frame the SK-1c back HUD rides). One render frame
                // stale by construction; the puff inherits the sled velocity
                // and decays it, so the lag reads as breath sweeping aft, not
                // as an offset head. Face direction follows the freelook
                // orbit (§9d: camera owns look, head follows).
                const glm::dvec3 sup = pin.up;
                glm::dvec3 sfwd = pin.basis * glm::dvec3(0.0, 0.0, -1.0);
                sfwd = glm::normalize(sfwd - glm::dot(sfwd, sup) * sup);
                const glm::dvec3 sleft = glm::normalize(glm::cross(sup, sfwd));
                const glm::dvec3 face = std::cos(sled_cam_az) * sfwd -
                                        std::sin(sled_cam_az) * sleft;
                const render::RiderBack& rb = render::sled_model_rider_back();
                if (rb.valid) {
                    // Mouth vent ≈ 0.18 m up the spine from the neck joint +
                    // 0.10 m out the face.
                    pin.head_pos = rb.neck + sup * 0.18 + face * 0.10;
                    pin.head_valid = true;
                } else {
                    // DOCUMENTED APPROXIMATION (GLB missing / placeholder
                    // machine): seated helmet ≈ 1.25 m above the running
                    // surface (render/rider_pose.h's measured cowl top 0.892
                    // with the head group above it), minus cg_height_m 0.564
                    // => body y ≈ +0.69, slightly ahead of the CG.
                    pin.head_pos =
                        sled_draw.position +
                        pin.basis * glm::dvec3(0.0, 0.69, -0.10);
                    pin.head_valid = false;
                }
                pin.head_fwd = face;
            } else {
                pin.up = glm::normalize(draw_state.position);
                pin.roost_gain = 0.0;
                pin.exhaust_gain = 0.0;
                pin.breath_gain = 0.0;
            }
            // ★★★ AND THE POOF IS ASSIGNED **OUTSIDE** THAT BRANCH, WHICH IS
            // LOAD-BEARING. The else-branch above zeroes every gain because a
            // parked/absent machine should show nothing -- but the poof fires
            // when the man is OFF the machine, which is exactly when
            // `sled_active` is least trustworthy. Assigning these three lines
            // inside the `if` is the single most likely way to build this and
            // see nothing at all.
            pin.poof_gain = poof_gain;
            pin.poof_n = pending_poof_n;
            for (int i = 0; i < pending_poof_n && i < 2; ++i)
                pin.poof[i] = pending_poof[i];
            pending_poof_n = 0;  // consumed; the next frame's ticks refill it
            render::sled_plumes_update(sled_plumes, pin, clamped_dt);
            info.sled_plumes = &sled_plumes;
        }
        // ★ R5 row 9 — SHADOWS ON SNOW: build the CASTER PROXY LIST
        // (option (c), docs/snow_R5_immersion_ledger.md ROW 9 — the shadow is
        // evaluated per-fragment in the RECEIVING ground shader against these
        // capsules; nothing here computes where a shadow lands, so the
        // fence-3 under-the-world class cannot exist). READ-ONLY off sim:
        // every input is a state/params READ, nothing flows back.
        //
        // The dial, the SEADS_TRACK_PACK / SEADS_SPARKLE precedent to the
        // letter: read once, validated [0,4], warn-and-default, TraceLog'd.
        // 0 KILLS THE ROW AT THE SOURCE — the caster list stays empty, the
        // shader early-outs on count 0, and every pixel is bit-identical to
        // pre-row-9 (the A/B baseline). 1 = ship. Values > 1 push the umbra
        // darker than ship (the tint divides down), the deliberately-too-far
        // ladder arms.
        {
            static const double shadow_dial = [] {
                const char* e = std::getenv("SEADS_SHADOWS");
                if (e == nullptr) return 1.0;
                const double v = std::atof(e);
                if (v < 0.0 || v > 4.0) {
                    TraceLog(LOG_WARNING,
                             "SHADOWS: SEADS_SHADOWS=\"%s\" outside [0,4] -- "
                             "IGNORED, using 1.0",
                             e);
                    return 1.0;
                }
                TraceLog(LOG_INFO, "SHADOWS: SEADS_SHADOWS=%.2f", v);
                return v;
            }();
            if (world.shadows.enabled && shadow_dial > 0.0) {
                info.shadow_strength = static_cast<float>(
                    std::min(world.shadows.strength * shadow_dial, 1.0));
                // Dial arms above 1 DARKEN the umbra past ship (the tint
                // divides toward black as the dial grows) — ship strength is
                // already 1.0, so without this the 2/4 arms would clamp into
                // a saturated no-op and the ladder would have
                // indistinguishable rungs, which is not a measurement.
                // Ladder (umbra vs sunlit at a high sun): 0 = OFF baseline,
                // 0.5 ~ 69%, 1 ~ 38% (ship), 2 ~ 21%, 4 ~ 12% (deliberately
                // too far: a hard near-black shadow no snowfield shows).
                glm::dvec3 tint = world.shadows.tint;
                if (shadow_dial > 1.0) tint /= shadow_dial;
                info.shadow_tint = glm::vec3(tint);
                // ★ FENCE 5 — THE ALTITUDE GATE. A caster below the drive
                // surface (the sled or the aircraft inside the tunnel
                // network) must not shadow the ground over its head; the
                // proxy test cannot know terrain, so the gate lives at the
                // list. Surface radius from the ONE height source
                // (world::HeightField::radius_at — the same field the sim
                // ground contact reads). No heightfield (bare-sphere
                // fallback) => gate against R itself.
                const auto above = [&](const glm::dvec3& p) {
                    const double surf_r =
                        snow_field.hf != nullptr
                            ? snow_field.hf->radius_at(glm::normalize(p))
                            : params.R;
                    return render::shadow_caster_above_surface(
                        p, surf_r, world.shadows.underground_margin_m);
                };
                // CASTER 1 — HIS AIRCRAFT (the landing instrument). The
                // player plane is drawn every frame, drive mode included, at
                // draw_state — so it casts whenever it is above ground.
                // Dimensions: MEASURED from aircraft_node_specs() box_dims
                // inside render/shadow_casters.h (the probe's method — span
                // 10.10 / length 8.90 / chord 2.40).
                if (above(draw_state.position)) {
                    render::aircraft_shadow_proxies(info.shadow_casters,
                                                    draw_state.position,
                                                    draw_state.orientation);
                }
                // CASTER 2 — THE SLED (machine + rider, six capsules — Chad's
                // 2026-08-28 ruling: the shadow "ACTUALLY FOLLOWS AND
                // PROJECTS THE MEASURED OUTLINE"). Chart dims from the
                // kernel's own MEASURED SledParams (sim/sled.h:388); the
                // model-frame silhouette constants are MEASURED off the
                // shipped GLB inside render/shadow_casters.h. Articulation is
                // straight kernel-state reads — the SAME channels the drawn
                // machine poses with (sled_model.cpp pose_pass): steer_actual
                // through ski_steer_max_rad() onto the ski kingpins,
                // per-corner susp_x onto the unsprung proxies, the rider lean
                // slew onto the rider capsule, sled_sag0_m() as the one
                // mount-drop source.
                // helmet_top 1.574660 m is the MEASURED seated helmet CROWN (R4a's
                // measure_helmet_crown.py, re-run here; was 1.25, which was
                // COMPOSED not measured and left the head 0.32 m short). The
                // row-6 breath fallback anchor documents above.
                // rider_radius 0.25 m is a DOCUMENTED APPROXIMATION — no
                // measured clothed-torso width exists in the tree; a seated
                // adult in winter gear is ~0.5 m across the shoulders.
                if (info.sled_active && above(sled_draw.position)) {
                    render::SledShadowDims sd;
                    sd.cg_h = sled_params.cg_height_m;
                    sd.half_stance = 0.5 * sled_params.stance_m;
                    sd.ski_half_w = 0.5 * sled_params.ski_width_m;
                    sd.track_half_w = 0.5 * sled_params.track_width_m;
                    sd.sag0 = render::sled_sag0_m();
                    sd.steer_rad =
                        sled_draw.steer_actual * render::ski_steer_max_rad();
                    for (int ci = 0; ci < 3; ++ci)
                        sd.susp_m[ci] = sled_draw.susp_x[ci];
                    sd.rider_lat_m = sled_draw.rider_lat_m;
                    sd.rider_fwd_m = sled_draw.rider_fwd_m;
                    sd.rider_up_m = sled_draw.rider_up_m;
                    sd.helmet_top = 1.574660;
                    sd.rider_radius = 0.25;
                    render::sled_shadow_proxies(info.shadow_casters,
                                                sled_draw.position,
                                                info.sled_basis, sd);
                }
                // CASTERS 3 (enemy aircraft) and 4 (Sudburians) are DATA
                // appends here when their rows open — 3 is
                // aircraft_shadow_proxies per nearby drone (CPU-culled to the
                // uniform budget); 4 is unbuildable today (there are no
                // townspeople NPCs in the tree — the only Sudburian is the
                // rider, who caster 2 already carries).
            }
        }
        // Fleet Rig mirror knobs (rig-A.2): the FELT config, read-only.
        info.rig_reflectivity = world.fleet_rig.reflectivity;
        info.rig_fresnel_power = world.fleet_rig.fresnel_power;
        // S-mapteam: the plane livery IS the faction palette (one table).
        // ★★★ L11 (Chad 2026-09-10): through the TEAM KIT, so the livery, the
        // scarf, the wingtip smoke and the helmet band all answer the one
        // question "which side is he on". Valley = team_colors().ally, exactly
        // what this line read before; Central City = the slag orange.
        info.rig_player_color = glm::vec3(render::player_kit().plane);
        // RUNG E4 VISIBILITY: the [plane_legibility] dials, carried whole (the
        // loader owns the struct, the draw layer only reads it). Every dial at
        // its off value => today's frame.
        info.legibility = world.legibility;
        // RUNG E4.2: the BANDIT LIVERY wears the DERIVED legibility tint — the
        // ruled slag orange pushed toward a deeper saturated red so an enemy
        // cannot read as warm haze in the white scatter light. ONE authority
        // (render::enemy_legibility_tint, render/team_color.h): the in-game
        // tag and the E4.3 glint bead call the SAME function, and the slag pour
        // + the tactical map deliberately keep the unshifted hue. shift = 0
        // returns team_colors().enemy bit-identically.
        info.rig_bandit_color = glm::vec3(render::enemy_legibility_tint(
            render::team_colors().enemy, world.legibility.enemy_tint_shift));
        // rig-B: the commanded surfaces (player carried render-side; drones
        // filled in the drones_draw loop below) + the deflection/prop
        // feel-knobs from [fleet_rig]. drone_inputs is set with drones_draw.
        info.player_inputs = rig_player_inputs;
        info.player_wheel_roll_rad = wheel_roll_rad;
        info.rig_aileron_deg = world.fleet_rig.aileron_deg;
        info.rig_elevator_deg = world.fleet_rig.elevator_deg;
        info.rig_rudder_deg = world.fleet_rig.rudder_deg;
        info.rig_gear_deploy_deg = world.fleet_rig.gear_deploy_deg;
        info.rig_prop_disc_alpha = world.fleet_rig.prop_disc_alpha;
        info.rig_prop_idle_alpha = world.fleet_rig.prop_idle_alpha;
        info.raw_mode = raw_mode;
        info.freelook = !raw_mode && live.freelook_held;
        // S-reticle: the reticle draws the DISPLAY-EASED direction; the cap
        // is the largest uncurved per-frame aim step (quant_px * sensitivity
        // — derived LIVE so a retune of either knob tracks, the AT-15
        // calibrated-constant class), scaled by the BINARY zoom gain so the
        // screen-px trail is zoom-consistent (the same 0.25 the aim deltas
        // already carry while zoomed). Display-only: the raw aim feeds the
        // instructor/camera above, untouched.
        {
            const double cap =
                cparams.aim_curve_quant_px * cparams.aim_sensitivity *
                ((!raw_mode && live.zoom_held)
                     ? render::kZoomFovyDeg / render::kChaseFovyDeg
                     : 1.0);
            reticle_dir = render::reticle_smooth(
                reticle_dir, loop.aim.forward(), clamped_dt, cap);
        }
        info.reticle_dir = reticle_dir;
        // The bandits, interpolated the SAME way as the player (SPEC §10) and
        // drawn in the player's view. Shown in both modes — they fly their own
        // instructor regardless of the player's raw/instructor toggle.
        // render/rig-D port: the commanded Inputs pose the Fleet Rig's
        // ailerons/elevator/rudder (render-only, RA9 — a downstream read of
        // the last-committed Inputs, never fed back). Drones have no exposed
        // per-tick commanded Inputs (their bank-hold autopilot's Inputs are
        // internal to drone::step and discarded) — info.drone_inputs stays
        // empty, so every drone draws its rig at rest pose (the documented
        // rig-B fallback for a missing/short drone_inputs).
        info.player_inputs = player_inputs_disp;
        info.drone_scale = dw.dparams.size;
        info.drones_draw.clear();
        info.drone_inputs.clear();
        info.drones_alive.clear();
        info.drones_faction.clear();
        info.drones_friendly.clear();
        info.drones_draw.reserve(dw.drones.size());
        info.drone_inputs.reserve(dw.drones.size());
        info.drones_alive.reserve(dw.drones.size());
        info.drones_faction.reserve(dw.drones.size());
        for (const drone::DroneState& d : dw.drones) {
            info.drones_draw.push_back(
                render::interpolate(d.prev, d.curr, accum.alpha()));
            // rig-B: the bandit's commanded surfaces (same source as the
            // player — the autopilot Inputs). Parallel to drones_draw.
            info.drone_inputs.push_back(d.last_inputs);
            // CONQUEST: 0 = a killed wreck (inert), so draw skips it while the
            // index mapping to the gunsight stays intact. All 1 off-conquest.
            info.drones_alive.push_back(d.inert ? 0 : 1);
            // M-KEY MAP: which side this maverick flies for (single-source
            // with the conquest score bookkeeping's combat::maverick_faction
            // / combat::is_enemy), parallel to drones_draw.
            info.drones_faction.push_back(combat::maverick_faction(
                d.spawn_index, cq.state.player_faction));
            info.drones_friendly.push_back(d.friendly_side ? 1 : 0);
        }
        // M-KEY MAP: which side the PLAYER flies for this session (read
        // regardless of conquest_on — the roster/faction split is a fixed
        // fact of the fleet, not gated by the conquest scoring toggle).
        info.conquest_player_faction = cq.state.player_faction;
        // LIVE bubble growth -> the M-KEY map outline (2026-07-26 fix: the map
        // used to draw the BAKED base ellipses, so a killed pump never moved
        // the drawn bubble).
        for (int f = 0; f < 2; ++f)
            info.conquest_radius_scale[f] = cq.state.radius_scale[f];
        // SUDDEN-DEATH MATCH CLOCK -> HUD (disarmed => not drawn).
        info.conquest_countdown_faction = cq.state.countdown_faction;
        info.conquest_countdown_s = cq.state.countdown_s;
        info.conquest_countdown_paused = cq.state.countdown_paused;
        // ★ L6: the pool, single-sourced from the predicate the clock itself
        // runs on, so the HUD and the buzzer can never disagree about whether
        // a clock exists.
        info.conquest_countdown_armed = combat::countdown_active(cq.state);
        info.conquest_respawn_locked = combat::respawns_locked(cq.state);
        info.conquest_deathmatch = cq.state.deathmatch;
        // Lead-angle gunsight (S8-drone): read the meter's snapshot (computed
        // once per sim tick in app::tick, single source) into the HUD. The
        // pipper + TOT% are pure reads — no render-time solve. Instructor mode
        // only (raw mode has no aim HUD).
        info.gunsight_active = !raw_mode;
        info.gunsight_has_target = dw.meter.has_target;
        info.gunsight_lead = dw.meter.lead_now;
        info.gunsight_on_target = dw.meter.on_target_now;
        info.gunsight_out_of_envelope =
            dw.meter.out_of_envelope_now;  // red pipper (Task B.3)
        info.tot_frac = dw.meter.fraction();
        info.tot_range = dw.meter.range_now;
        info.tot_closure = dw.meter.closure_rate_now;
        // Engaged-target slot for the below-target range readout (Chad's ask):
        // the meter's engaged fleet slot IS an index into dw.drones, which is
        // populated 1:1 into info.drones_draw above — so the same index locates
        // the engaged bandit's interpolated draw state. -1 when nothing
        // engaged.
        info.gunsight_target_index =
            dw.meter.has_target ? dw.meter.engaged_index : -1;
        // Lead PIPPER world point B = engaged bandit's INTERPOLATED draw state
        // (so the marker and the drawn bandit share ONE position — no
        // inter-source jitter) advanced by the solved time-to-intercept. draw
        // projects THIS from the eye; parallax-correct (Fable 2026-07-13).
        // Falls back to the target's current position when unsolved (ttl=0).
        if (info.gunsight_target_index >= 0 &&
            info.gunsight_target_index <
                static_cast<int>(info.drones_draw.size())) {
            const sim::SimState& tgt =
                info.drones_draw[info.gunsight_target_index];
            info.gunsight_lead_point =
                tgt.position + tgt.velocity * dw.meter.ttl_now;
        }
        // Fixed boresight reticle (Task B, iter-7): the sightline the canted
        // rounds cross at convergence range. SINGLE-SOURCE with
        // harmonization_rise: the body-frame sightline is (0,0,-conv) — the
        // same point that muzzle_world_dir / harmonization_rise use as the
        // convergence target. Transforming just the nose direction is exact for
        // the sightline: the rounds converge ON the line shoot_pos +
        // t*(orient*(0,0,-1)).
        info.gunsight_boresight =
            glm::normalize(draw_state.orientation * glm::dvec3{0.0, 0.0, -1.0});
        // Probe mode: replace the patrolling fleet with ONE deterministic
        // bandit at a fixed range ahead, DEPRESSED below the local horizon so
        // it silhouettes against the procedural GROUND (the ground-busyness-vs-
        // plane-pop collision §4 exists to catch), pinned relative to the
        // CURRENT player pose so the range stays exact as the hands-off plane
        // drifts. No HUD pipper (the probe measures the plane, not the reticle
        // overlay).
        if (probe_mode) {
            // Place ONE bandit off the local horizontal: Below = DEPRESS below
            // the sphere horizon (dip + margin) so it silhouettes against
            // GROUND; Above = ELEVATE above horizontal so it silhouettes
            // against SKY (Fable P0-1: the horizon dips 15-34 deg on the 15 km
            // planet). Clear the ground TERRAIN shell (R + relief top) so the
            // target is never buried; feasibility flags are logged at
            // measurement.
            constexpr double kProbeMarginDeg = 5.0;
            const glm::dvec3 nose =
                draw_state.orientation * glm::dvec3{0.0, 0.0, -1.0};
            probe_place = render::probe_placement(
                draw_state.position, nose, params.R,
                params.R + world.ground.relief_scale_m,
                kProbeMarginDeg * 3.14159265358979323846 / 180.0, probe_range,
                probe_geom);
            sim::SimState tgt =
                draw_state;  // level cruise attitude, enemy livery
            tgt.position = probe_place.target;
            tgt.velocity = glm::dvec3{0.0};
            info.drones_draw.clear();
            info.drones_draw.push_back(tgt);
            // rig-B (Fable after-red-team P1): clear the live patrol fleet's
            // commanded Inputs too — else the frozen probe target inherits
            // drone-0's tick-varying deflection and wobbles the measured
            // silhouette. Short drone_inputs => the target draws at rest.
            info.drone_inputs.clear();
            info.drone_scale = 1.0;
            info.gunsight_active = false;
            info.gunsight_has_target = false;
            // Point the SAME camera pose straight AT the bandit so it stays
            // on-screen at any altitude (a level chase camera loses a high-alt
            // below-horizon target off the bottom of the frame). This one pose
            // drives BOTH draw_frame AND run_probe_measurement, so the rendered
            // and measured camera can never diverge (Fable P0-1). Capture the
            // pre-repoint LEVEL forward first: run_probe_measurement logs
            // whether a level-flying player would actually see the bandit — the
            // real gameplay fact the re-point would otherwise erase (P2).
            probe_level_fwd = glm::normalize(pose.target - pose.eye);
            pose.target = render::probe_camera_forward(probe_place, pose.eye) *
                              probe_range +
                          pose.eye;
        }
        // MB-7c energy legibility (read-only, the S8 gunsight firewall). The
        // dial's limits come from config ONCE (the protection clamp's aoa_max
        // and the plant stall alpha — the dial, the clamp, and the vortices
        // agree on where the edge is). All display quantities read the SHARED
        // flight_readout / interpolated draw state; nothing flows back.
        info.aoa_max = cparams.aoa_max;
        info.aoa_max_neg = cparams.aoa_max_neg;
        info.stall_alpha = params.Cl_max / params.Cl_alpha;
        // S-cues (comfort program): peripheral orientation cues, pure HUD,
        // DEFAULT OFF (alpha 0 => draw_frame skips them, strict superset).
        // Config-sourced (config/controller.toml [comfort]); the draw reads
        // local_up/bank fresh each frame. Cosmetic — off every control path.
        info.cue_horizon_alpha = cparams.cue_horizon_alpha;
        info.cue_horizon_gap_frac = cparams.cue_horizon_gap_frac;
        info.cue_bank_arc_alpha = cparams.cue_bank_arc_alpha;
        // S-carets (REC-2): screen-edge threat indicators for off-screen
        // drones.
        info.cue_caret_alpha = cparams.cue_caret_alpha;
        // REC-6: the stylized cockpit frame (the steady-state rest frame).
        info.cue_cockpit_alpha = cparams.cue_cockpit_alpha;
        // MB HUD status stack: the commanded device latches for the labeled
        // flaps/gear block — the SAME latch the sim command reads above
        // (raw_dev in raw mode, live_dev in instructor mode), so the label
        // can never disagree with what the plant was told. Display-only.
        info.flap_mode = raw_mode ? raw_dev.flap_pos : live_dev.flap_pos;
        info.gear_down_cmd = raw_mode ? raw_dev.gear_down : live_dev.gear_down;
        // R4-FLY-7 thumb-binding diagnosis: which raylib mouse buttons are
        // held right now — the HUD prints the codes so a driver mismatch is
        // read off the screen and fixed in [input] game.toml. Display-only.
        info.mouse_buttons_held = 0;
        for (int b = 0; b <= 7; ++b) {
            if (IsMouseButtonDown(b)) info.mouse_buttons_held |= 1 << b;
        }
        {
            const render::FlightReadout rd =
                render::flight_readout(draw_state, env_ptr, params);
            // Speed trend: display-smoothed dV/dt over ~0.5 s (cosmetic; a
            // raw per-frame derivative flickers). Reset across respawn below.
            if (clamped_dt > 0.0) {
                const double raw_trend =
                    (rd.speed - prev_speed_for_trend) / clamped_dt;
                const double k = 1.0 - std::exp(-clamped_dt / 0.5);
                speed_trend += (raw_trend - speed_trend) * k;
            }
            prev_speed_for_trend = rd.speed;
            info.speed_trend = speed_trend;
            // Wingtip vortices: intensity from the SHARED readout (AoA vs the
            // protection limit, true n), trails advanced per render frame on
            // the interpolated state.
            const double vs = render::vortex_strength(rd.aoa, cparams.aoa_max,
                                                      rd.load_factor, rd.speed);
            render::vortex_update(vortices, draw_state, vs, clamped_dt);
            info.vortices = &vortices;
            // Feature B: advance the wingtip smoke trails per render frame.
            render::wingtip_smoke_update(wingtip_smoke, draw_state, smoke_on,
                                         clamped_dt);
            info.wingtip_smoke = &wingtip_smoke;
            // Tracer streaks (rig-D guns): const view of the app-owned pool.
            // tracer_lifetime_s / tracer_len_m / colors from config (no bare
            // numbers in render/ — house law). Raw mode has no trigger so
            // tracers won't spawn there, but the pool is still valid to read.
            info.projectiles = &gw.pool;
            // Enemy tracers + player stakes HUD (bandit combat AI): const views
            // of the app-owned combat state (read-only; the firewall).
            info.enemy_projectiles = &cw.enemy_pool;
            // RUNG S3-GUNS: the cosmetic pool. This line and
            // combat::cosmetic_fire_tick are its only two readers in the whole
            // program — no damage sweep is ever handed it.
            info.cosmetic_projectiles = &cw.cosmetic_pool;
            info.combat_fx = &cw.fx.pool;
            info.touchdown_fx_intensity = game.fx.touchdown_intensity;
            info.kill_count = cw.kills;
            info.ticks_since_kill = cw.ticks_since_kill;
            info.player_hp = cw.player_hp;
            // summary_hp is a 0..100 worst-component readout, so the bar max is
            // a fixed 100 (NOT setup.player_hp — that would desync the bar if
            // the config value != 100; Fable P1-1). >0 still gates the panel on
            // combat.
            info.player_hp_max = 100.0;
            info.death_count = cw.deaths;
            info.ticks_since_death = cw.ticks_since_death;
            // CONQUEST overlay (spec §5a): the status stack + VICTORY/DEFEAT
            // banner + the pump world markers. conquest_active gates the whole
            // overlay off when disabled (bit-identical). All read-only.
            info.conquest_active = conquest_on;
            if (conquest_on) {
                info.conquest_planes_left = cq.state.planes_left;
                info.conquest_score_valley = cq.state.score[combat::CQ_VALLEY];
                info.conquest_score_sudbury =
                    cq.state.score[combat::CQ_SUDBURY];
                info.conquest_pumps_total = combat::kNumPumps;
                int alive = 0;
                info.conquest_pumps.clear();
                info.conquest_pumps.reserve(combat::kNumPumps);
                for (const combat::Pump& pu : cq.state.pumps) {
                    if (pu.alive) ++alive;
                    info.conquest_pumps.push_back(
                        render::FrameInfo::ConquestPump{
                            pu.pos, pu.faction, pu.alive, pu.surface,
                            pu.max_hp > 0.0 ? std::max(0.0, pu.hp / pu.max_hp)
                                            : 0.0});
                    // ★ THE FOOT (Chad 2026-09-06: the body comes DOWN): the
                    // terrain point under a surface pump, mast law from
                    // app/spawn_policy.h; a deep pump's foot is itself.
                    info.conquest_pumps.back().foot =
                        pu.surface ? pu.pos - glm::normalize(pu.pos) *
                                                  app::kSurfacePumpMastM
                                   : pu.pos;
                }
                info.conquest_pumps_alive = alive;
                // Nearest alive pump within 2 km of the player -> its hp
                // fraction for the "PUMP nn%" HUD line (fly-3 feedback).
                info.conquest_near_pump_frac = -1.0;
                double best_d2 = 2000.0 * 2000.0;
                for (const combat::Pump& pu : cq.state.pumps) {
                    if (!pu.alive) continue;
                    const glm::dvec3 dp = pu.pos - loop.curr.position;
                    const double d2 = glm::dot(dp, dp);
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        info.conquest_near_pump_frac =
                            pu.max_hp > 0.0 ? std::max(0.0, pu.hp / pu.max_hp)
                                            : 0.0;
                    }
                }
                info.conquest_outcome =
                    cq.state.outcome == combat::Outcome::VICTORY  ? 1
                    : cq.state.outcome == combat::Outcome::DEFEAT ? 2
                                                                  : 0;
                // COMPETITIVE rung: the raid warning + blinking pump marker.
                info.conquest_pump_under_attack = cq.pump_under_attack;
                info.conquest_raided_pump = cq.raided_pump;
                // FLAK: the placed guns; the manned one draws at the LIVE
                // slewed pose (the same numbers the shooter frame fires from).
                info.flak_guns = flak_guns;
                info.flak_manned = flak_fk.manned;
                info.flak_manned_gun =
                    flak_fk.manned ? flak_fk.gun : -1;
                // F-POSE: the pullout blend decides both how solidly the
                // man draws (0 down the sight -- the clean sight picture)
                // and when the camera is far enough out to stop paying for
                // the close near plane. That second gate is LATE (0.9, not
                // the halfway point) on purpose: at t = 0.5 the eye is only
                // ~2 m off the mount, so handing the 2 m near plane back
                // there would slice the gun open mid-swing. By 0.9 it is
                // ~4 m out and the default plane is clear of the muzzle.
                info.flak_gunner_alpha = static_cast<float>(
                    render::flak::gunner_fade(flak_ext_t));
                if (flak_ext_t >= 0.9) info.flak_cam_external = true;
                // F-POSE: while manned the man is AT THE GUN, so the parked
                // sled hides its rider and the gunner drawer takes over
                // (render/flak_gunner.cpp). Keyed on READY, not enabled: if
                // the GLB fails to load the rider stays visible on his sled
                // rather than vanishing. SEADS_FLAK_GUNNER=0 kills both
                // arms -- bit-identical to the pre-rung frame.
                // flak_cam_external defaults false (the gunner's own head
                // view, mitts only); the FLAKCAM camera block below sets it
                // the frame it actually owns the camera.
                render::sled_rider_hide_set(flak_fk.manned &&
                                            render::flak_gunner_ready());
                info.flak_rounds_left = flak_fk.rounds_left;
                info.flak_reload_left_s = flak_fk.reload_left_s;
                // ★ STING RPAS: read-only copies for the world draw + HUD.
                info.sting_flying = sting.st.active;
                info.sting_draw = sting_draw_state;  // CAM-SMOOTH
                // ⚠ TWO AIMS, AND WHICH ONE SHIPS IS THE STATE'S CALL.
                // `st.aim` is the FLYING drone's carried frame -- while the
                // man is only SHOULDERING, it is stale (the last flight's, or
                // its default), and shipping it pitched the drawn launcher
                // ~57 deg down and welded his hands after it: Chad's
                // "collapsed over ... sting behind the rider" defect, root
                // cause (the deploy-debug smoke measured st_muzzle 0.27 m
                // over the origin against the 0.76 the shoulder math gives).
                // The shoulder's own aim is sting_aim_az/el over the stance
                // frame -- the SAME construction the launch click fires on,
                // so what he sees is what the missile is born onto.
                if (sting.st.active) {
                    info.sting_aim_dir = sting.st.aim.forward();
                } else {
                    glm::dvec3 spos, shead;
                    double shh;
                    sting_stance(spos, shead, shh);
                    const glm::dvec3 lup = glm::normalize(spos);
                    glm::dvec3 h = shead - glm::dot(shead, lup) * lup;
                    const double hl = glm::length(h);
                    h = hl > 1e-6 ? h / hl
                                  : glm::normalize(glm::cross(
                                        lup, glm::dvec3{1.0, 0.0, 0.0}));
                    const glm::dvec3 sleft =
                        glm::normalize(glm::cross(lup, h));
                    const double ce = std::cos(sting_aim_el),
                                 se = std::sin(sting_aim_el);
                    info.sting_aim_dir =
                        ce * (std::cos(sting_aim_az) * h -
                              std::sin(sting_aim_az) * sleft) +
                        se * lup;
                }
                info.sting_speed_mps = glm::length(sting.st.curr.velocity);
                info.sting_alt_m =
                    glm::length(sting.st.curr.position) - params.R;
                // fly-5: the DRONE's own air — the same spatial atm_frac_at
                // the plant flies, at the DRONE's position (the plane's
                // plate reads the parked aeroplane and lies for the drone).
                info.sting_air_frac = sim::atm_frac_at(
                    sting.st.curr.position, env_ptr, sting.ap);
                info.sting_shouldered = sting_shouldered;
                // ★★★ ST-5 PHASE D: the deploy blend and which stance it is
                // being drawn in. Both app-owned; render evaluates neither.
                info.sting_deploy = static_cast<float>(sting_deploy_t);
                // ST-5 polish (b): the app-owned prop angle, already wrapped.
                info.sting_prop_phase =
                    static_cast<float>(sting_prop_phase);
                info.sting_prop_rate = static_cast<float>(sting_prop_rate);
                // POLISH FLY-2: a shouldered-launcher camera owns the frame,
                // so render swaps in the close near plane (draw.h's banner:
                // the 2 m plane was cutting the man open at 1.5 m).
                // Only for the CLOSE ones: the over-shoulder view is always
                // 1.6 m, but the pull-out orbit can be wheeled to 8 m, and a
                // 0.15 m near plane against a 60 km far plane is depth
                // precision spent for nothing once the subject is metres away.
                info.sting_close_cam =
                    sting_shouldered &&
                    (!live.freelook_held || sting_look_dist < 3.0);
                info.sting_seated = sting_seated_now;
                info.sting_left = sting.st.left;
                info.sting_battery01 = std::clamp(
                    sting.st.battery_s / sting.sp.battery_s, 0.0, 1.0);
                info.sting_turns_left =
                    std::max(0, sting.sp.turns_max - sting.st.turns_used);
                info.sting_warn_s = sting.st.warn_s;
                info.flak_projectiles = &flak_fk.gw.pool;
                // STAGE B: the two cosmetic envelopes + the FX phase. Stepped
                // in the flak input block from the UNDRAINED spawned_accum
                // (frame-order contract in render/flak_gun.h); read-only here.
                info.flak_flash = flak_flash;
                info.flak_blast = flak_blast;
                info.flak_shot_seq =
                    static_cast<int>(flak_shot_seq & 0x3fffffffLL);
                info.flak_brass = &flak_brass;  // STAGE C, read-only there
                // ── STAGE D: the AI-manned guns. Their POSE (so the barrels
                // visibly track the raiders), their drum swap, their own
                // flash/blast envelopes, and their tracer pools -- all
                // read-only copies into the frame info, exactly like the
                // player's. Their AIR-SIGNAL BEAMS are deliberately left ON
                // (the suppression at the beam draw is keyed on
                // flak_manned_gun alone): the beam is how the player FINDS a
                // gun, and only the one he is standing at should go dark.
                info.flak_ai_projectiles.clear();
                for (const app::FlakAiGun& ag : flak_ai) {
                    if (ag.gun < 0 ||
                        ag.gun >= static_cast<int>(info.flak_guns.size()))
                        continue;
                    if (flak_fk.manned && flak_fk.gun == ag.gun) {
                        // The player has it: his own block below owns the
                        // pose, and the AI's stale pose must not fight it.
                        // The pool still draws -- rounds already in the air
                        // when he took the gun keep flying.
                        info.flak_ai_projectiles.push_back(&ag.fk.gw.pool);
                        continue;
                    }
                    render::FlakDraw& fd = info.flak_guns[ag.gun];
                    fd.train_rad = ag.fk.pose.train_rad;
                    fd.elev_rad = ag.fk.pose.elev_rad;
                    fd.reload_frac =
                        ag.fk.reload_s > 0.0
                            ? ag.fk.reload_left_s / ag.fk.reload_s
                            : 0.0;
                    fd.flash = ag.flash;
                    fd.blast = ag.blast;
                    fd.shot_seq =
                        static_cast<int>(ag.shot_seq & 0x3fffffff);
                    info.flak_ai_projectiles.push_back(&ag.fk.gw.pool);
                }
                info.flak_lead_valid = false;
                info.flak_hitmark = flak_hitmark;
                info.flak_killmark = flak_killmark;
                if (flak_fk.manned && flak_fk.gun >= 0 &&
                    flak_fk.gun < static_cast<int>(info.flak_guns.size())) {
                    info.flak_guns[flak_fk.gun].train_rad =
                        flak_fk.pose.train_rad;
                    info.flak_guns[flak_fk.gun].elev_rad =
                        flak_fk.pose.elev_rad;
                    info.flak_guns[flak_fk.gun].recoil_m = flak_recoil;
                    // STAGE C: the drum swap, closed-form off the reload
                    // timer. EXACTLY 0 when no reload runs, and only the
                    // MANNED gun can fire, so only it can ever be reloading.
                    info.flak_guns[flak_fk.gun].reload_frac =
                        flak_fk.reload_s > 0.0
                            ? flak_fk.reload_left_s / flak_fk.reload_s
                            : 0.0;
                    // The sight pipper: STICKY engaged target (acquire inside
                    // engage, keep to 1.15x -- the gunsight-selector
                    // anti-chatter lesson), solved by the SAME lead_solution
                    // the aircraft pipper uses, from the gun's own synthetic
                    // shooter state (zero velocity, local gravity).
                    static int flak_tgt = -1;
                    const double engage_m = 1800.0;
                    const glm::dvec3 gpos = flak_fk.mount.pos;
                    const auto fl_d2 = [&](int i) {
                        const glm::dvec3 dd =
                            dw.drones[i].curr.position - gpos;
                        return glm::dot(dd, dd);
                    };
                    // ★ ROUND 4 (Chad 2026-08-30: "zoom went to a dot in
                    // the sky for about 5 tries"): the old selector took
                    // the NEAREST drone by DISTANCE, regardless of where
                    // the gunner points -- so the pipper (and with it the
                    // RMB zoom and the hit X) could anchor to a bandit he
                    // was not engaging, a dot elsewhere in the sky, and
                    // hold it until it died or left. The gun now targets
                    // where the GUNNER AIMS: acquire the smallest-angle
                    // drone off the BORE inside the acquire cone, keep it
                    // until it dies, leaves 1.15x range, or drifts past
                    // the wider drop cone (hysteresis = the anti-chatter
                    // lesson, both cones are FEEL DIALS). Correct lead
                    // tracking parks the drone ~lead-angle off the bore
                    // (<= ~12 deg here), well inside both cones. No drone
                    // in the cone => no pipper, zoom holds the bore --
                    // point near a bandit and the sight picks HIM.
                    const double kAcquireConeCos =
                        std::cos(25.0 * PI / 180.0);
                    const double kDropConeCos =
                        std::cos(40.0 * PI / 180.0);
                    const glm::dvec3 fbore = render::flak::bore_dir_world(
                        flak_fk.mount, flak_fk.pose);
                    const auto fl_cos = [&](int i) {
                        const glm::dvec3 dd =
                            dw.drones[i].curr.position - gpos;
                        const double l2 = glm::dot(dd, dd);
                        if (l2 < 1e-12) return -1.0;
                        return glm::dot(dd, fbore) / std::sqrt(l2);
                    };
                    if (flak_tgt >= 0 &&
                        (flak_tgt >= static_cast<int>(dw.drones.size()) ||
                         dw.drones[flak_tgt].inert ||
                         fl_d2(flak_tgt) >
                             (1.15 * engage_m) * (1.15 * engage_m) ||
                         fl_cos(flak_tgt) < kDropConeCos))
                        flak_tgt = -1;
                    if (flak_tgt < 0) {
                        double bc = kAcquireConeCos;
                        for (int i = 0;
                             i < static_cast<int>(dw.drones.size()); ++i) {
                            if (dw.drones[i].inert) continue;
                            if (fl_d2(i) >= engage_m * engage_m) continue;
                            const double c = fl_cos(i);
                            if (c > bc) {
                                bc = c;
                                flak_tgt = i;
                            }
                        }
                    }
                    if (flak_tgt >= 0) {
                        const render::LeadSolution fsol =
                            render::lead_solution(
                                flak_fk.shooter, dw.drones[flak_tgt].curr,
                                render::flak::kMuzzleSpeedMps, engage_m,
                                -params.g * flak_fk.mount.up,
                                world.guns.cannon_drag_k, params.sim_dt);
                        // Smoke-only Bug-B confirmation arm
                        // (SEADS_FLAK_AIM_LEAD, the SEADS_FLAK_MANNED
                        // class): slew the manned gun's DEMAND onto the
                        // solved firing direction each frame, so the smoke
                        // rounds actually intercept the orbiting rig target
                        // and the hit/kill X chain can be certified
                        // headlessly (no fixed SEADS_FLAK_POSE can track
                        // the orbit). NOT A SHIPPED PATH.
                        if (smoke_frames > 0 && fsol.valid &&
                            std::getenv("SEADS_FLAK_AIM_LEAD"))
                            render::flak::aim_to_pose(flak_fk.mount,
                                                      fsol.lead_dir,
                                                      flak_fk.demand);
                        if (fsol.valid) {
                            // the drawn point: the target advanced by the
                            // solved TOF (the aircraft-pipper convention)
                            const sim::SimState& ft =
                                dw.drones[flak_tgt].curr;
                            info.flak_lead_point =
                                ft.position +
                                ft.velocity * fsol.time_to_intercept;
                            info.flak_lead_valid = fsol.in_range;
                            // Smoke-only zoom-blend probe (SEADS_FLAK_ZOOM_DBG).
                            if (smoke_frames > 0 &&
                                std::getenv("SEADS_FLAK_ZOOM_DBG"))
                                std::fprintf(
                                    stderr,
                                    "[FZDBG2] in_range=%d ang(camfwd,lead)="
                                    "%.3f deg\n",
                                    static_cast<int>(fsol.in_range),
                                    std::acos(std::clamp(
                                        glm::dot(
                                            glm::normalize(pose.target -
                                                           pose.eye),
                                            glm::normalize(
                                                info.flak_lead_point -
                                                pose.eye)),
                                        -1.0, 1.0)) * 180.0 / PI);
                        }
                    }
                }
                // Smoke-only pose override (the SEADS_RIGCAM class): certify
                // the DRIVEN nodes visually — train about local up, elevation
                // about the trunnion. SEADS_FLAK_POSE="train_deg,elev_deg".
                if (smoke_frames > 0) {
                    if (const char* fp = std::getenv("SEADS_FLAK_POSE")) {
                        double td = 0.0, ed = 0.0;
                        if (std::sscanf(fp, "%lf,%lf", &td, &ed) == 2)
                            for (render::FlakDraw& fg : info.flak_guns) {
                                fg.train_rad = td * PI / 180.0;
                                fg.elev_rad = render::flak::clamp_elev(
                                    ed * PI / 180.0);
                            }
                    }
                }
            }
            // Component damage panel (damage_model_plan.md): per-component
            // health.
            info.dmg_engine = cw.damage.engine;
            info.dmg_pilot = cw.damage.pilot;
            info.dmg_wing_left = cw.damage.wing_left;
            info.dmg_wing_right = cw.damage.wing_right;
            info.dmg_structure = cw.damage.structure;
            info.tracer_lifetime_s = world.guns.tracer_lifetime_s;
            info.tracer_len_m = world.guns.tracer_len_m;
            info.tracer_cannon_rgb = glm::vec3(world.guns.tracer_cannon_rgb);
            info.tracer_mg_rgb = glm::vec3(world.guns.tracer_mg_rgb);
            // FLAK STAGE B: the gun's own tracer look, kept OFF the signed
            // aircraft keys (config [guns] flak_tracer_*).
            info.flak_tracer_rgb = glm::vec3(world.guns.flak_tracer_rgb);
            info.flak_tracer_base =
                static_cast<float>(world.guns.flak_tracer_base);
            info.flak_tracer_len_mult =
                static_cast<float>(world.guns.flak_tracer_len_mult);
            info.flak_tracer_lum_mult =
                static_cast<float>(world.guns.flak_tracer_lum_mult);
            // Tracer comet look dials (iter-7 Task E: no bare numbers in
            // render/).
            info.tracer_cannon_base =
                static_cast<float>(world.guns.tracer_cannon_base);
            info.tracer_mg_base = static_cast<float>(world.guns.tracer_mg_base);
            info.tracer_r_core_frac =
                static_cast<float>(world.guns.tracer_r_core_frac);
            info.tracer_r_core_min =
                static_cast<float>(world.guns.tracer_r_core_min);
            info.tracer_r_glow_mult =
                static_cast<float>(world.guns.tracer_r_glow_mult);
            info.tracer_glow_lum =
                static_cast<float>(world.guns.tracer_glow_lum);
            info.tracer_glow_alpha =
                static_cast<float>(world.guns.tracer_glow_alpha);
            info.tracer_core_lum =
                static_cast<float>(world.guns.tracer_core_lum);
            info.tracer_core_alpha =
                static_cast<float>(world.guns.tracer_core_alpha);
            info.tracer_head_r_mult =
                static_cast<float>(world.guns.tracer_head_r_mult);
            info.tracer_head_lum =
                static_cast<float>(world.guns.tracer_head_lum);
            info.tracer_head_alpha =
                static_cast<float>(world.guns.tracer_head_alpha);
            info.tracer_slag_dark =
                static_cast<float>(world.guns.tracer_slag_dark);
            info.tracer_slag_period_m =
                static_cast<float>(world.guns.tracer_slag_period_m);
            info.tracer_glow_len_mult =
                static_cast<float>(world.guns.tracer_glow_len_mult);
            // T25 sub-mark (this fix): vortex_update + the FlightReadout/HUD
            // dial assembly above, split out of the composite app_tick lap.
            if (g_prof.on) g_prof.mark("hud_vortex");
            // Wind audio (MB-7c i, now the multi-layer render::WindSynth):
            // pink-noise bed + gusts + pull-up slip + birch-flute phrases +
            // muted tag flitter, all driven by airspeed / air density / G.
            // Cosmetic, read-only off the render snapshot. Drivers set per
            // buffer; the synth streams the samples.
            if (wind_ok) {
                // R6: SPATIAL air density — thins at the bubble edge so the
                // ear hears the thinning air (Fable-BEFORE §5: the #1 felt
                // cue; a stale altitude-density wind track makes the honest
                // ρ-gradient wall read as a script). Null env => bit-identical
                // to sim::atm_frac(rd.altitude, params).
                wind_synth.set_drivers(
                    rd.speed,
                    sim::atm_frac_at(draw_state.position, env_ptr, params),
                    rd.load_factor);
                while (IsAudioStreamProcessed(wind_stream)) {
                    short buf[1024];
                    wind_synth.render(buf, 1024);
                    UpdateAudioStream(wind_stream, buf, 1024);
                }
            }
            // Throttle voice: the bagpipe drone, pitch rises with throttle.
            // Read-only off the sim throttle passthrough (state.throttle,
            // [0,1]).
            //
            // THE STOPE ECHO rides this channel and the two below it. One
            // set_inside per frame (the slew is per-frame), then the room is
            // applied to every buffer the synth renders. `cave` is the SAME
            // app::inside_tunnel result the camera and the music bed already
            // read this frame — never a second predicate.
            engine_verb.set_inside(cave, clamped_dt);
            gun_verb.set_inside(cave, clamped_dt);
            sfx_verb.set_inside(cave, clamped_dt);
            sled_verb.set_inside(cave, clamped_dt);
            sting_verb.set_inside(cave, clamped_dt);
            boom_verb.set_inside(cave, clamped_dt);
            if (engine_ok) {
                engine_synth.set_throttle(draw_state.throttle);
                while (IsAudioStreamProcessed(engine_stream)) {
                    short buf[1024];
                    engine_synth.render(buf, 1024);
                    engine_verb.process(buf, 1024);
                    UpdateAudioStream(engine_stream, buf, 1024);
                }
            }
            // ⭐ THE SNOWMACHINE ENGINE. Chad, 2026-08-17: "Idle engine indy
            // 650 for idle and indy_650 engine sound for riding ... Tapping
            // keys gives the brap brap ramp up."
            //
            // THE DRIVER IS `sled_thumb`, THE THUMB THROTTLE, AND IT IS PUSHED
            // AT FRAME RATE. Both halves of that matter:
            //
            //   - The thumb throttle is the KEY PRESS. Chad's brap is a
            //     property of how the throttle is USED, not of how fast the
            //     machine is going, and render/sled_drive.h detects onsets off
            //     exactly this signal (hysteretic gate + refractory). Driving
            //     it from a speed or an rpm instead would give a machine that
            //     swells but never barks.
            //   - Per FRAME, never per buffer. A buffer is 46 ms at 1024
            //     frames / 22050 Hz, and a red-team on this synth showed that
            //     feeding it there DROPS real taps outright: a 40 ms press that
            //     opens and closes between two buffers is never seen, and two
            //     quick stabs inside one buffer collapse into one. The synth
            //     keeps an onset LATCH that render() drains, which is why the
            //     call below sits here with the input and not in the drain.
            //
            // OFF THE MACHINE THE THROTTLE IS FORCED TO ZERO, and that is not
            // belt-and-braces. `sled_thumb` is only ADVANCED inside the
            // drive-mode block, but nothing zeroes it on dismount (the mount
            // branch resets it, the dismount branch does not) -- so stepping
            // off at full throttle FREEZES it at 1.0 for the whole flight.
            // Passing it unconditionally would hold the rev follower pinned
            // the entire time you were airborne, and the mount gate below,
            // opening over 250 ms, would then fade in a screaming engine at the
            // exact moment you got back on a machine that is sitting still.
            // Forcing zero here lets the follower decay to idle while you fly,
            // so a remount always starts from idle. It also keeps the
            // hysteretic onset gate closed, so no brap can be latched in the
            // air and fired at you on the way back down.
            if (sled_ok) {
                const bool mounted =
                    player.off_aircraft() && player.sled_seeded;
                // The mount gate: a slewed 0..1, not the boolean. ~0.25 s each
                // way, so mounting fades the machine in rather than stepping a
                // full-scale engine channel on at a buffer boundary.
                sled_audio_gate += ((mounted ? 1.0 : 0.0) - sled_audio_gate) *
                                   render::audio_slew_coef(clamped_dt, 0.25);
                sled_synth.set_drivers(mounted ? sled_thumb : 0.0, clamped_dt);
                SetAudioStreamVolume(
                    sled_stream, static_cast<float>(render::kSledStreamLevel *
                                                    sled_audio_gate));
                // Below audibility the SYNTH is not run and the buffer is
                // zeroed -- cheaper, and it guarantees a parked machine cannot
                // idle away under the whole flight game.
                //
                // The stream is still FED every buffer: raylib replays whatever
                // is left in a sub-buffer nobody refilled, so skipping the
                // refill would loop the last 46 ms of engine forever instead of
                // going quiet.
                //
                // And the silence goes THROUGH the room, not around it. Feeding
                // the reverb only while audible would freeze its delay lines
                // holding the last engine samples from the previous ride, and
                // the next mount inside the tunnel net would open with a burst
                // of audio from minutes ago. Outside the net the room is a
                // bit-identical dry bypass, so this costs nothing in the common
                // case and flushes the tail properly in the rare one.
                const bool audible = sled_audio_gate > 1e-4;
                while (IsAudioStreamProcessed(sled_stream)) {
                    short buf[1024];
                    if (audible) {
                        sled_synth.render(buf, 1024);
                    } else {
                        for (int i = 0; i < 1024; ++i) buf[i] = 0;
                    }
                    sled_verb.process(buf, 1024);
                    UpdateAudioStream(sled_stream, buf, 1024);
                }
            }
            // ⭐⭐⭐ THE STING'S MOTOR BUZZ (ST-5 polish (c), Chad 2026-09-05:
            // "need some sound for the model increasing pitch and volume with
            // throttle"). Four detuned harmonic stacks through a brightness
            // lowpass -- render/sting_audio.h holds every law; this block is
            // only the plumbing, exactly as the snowmachine's is.
            //
            // THREE DRIVERS, and each is here rather than in the synth for a
            // reason:
            //
            //   THROTTLE  `sting_thr01`, the SAME scalar the prop spin is
            //             stepped from (see its declaration for why the
            //             cascade's internal throttle is neither reachable nor
            //             wanted). One number drives the blur and the buzz, so
            //             they can never disagree.
            //   DISTANCE  the drone is the one sound source in this game that
            //             routinely leaves the listener behind: freelook on a
            //             parked man puts the eye on the snow while the Sting
            //             is kilometres out at 450 m/s. Measured from
            //             `pose.eye` -- the frame's ACTUAL eye, whichever
            //             camera won it -- and never from the player body,
            //             which is a different point in exactly the case that
            //             matters.
            //   GATE      full while flying; a quiet fraction while it is
            //             still on the rail spinning up, so the deploy has a
            //             sound of its own; zero otherwise. Slewed ~0.2 s, the
            //             sled_audio_gate rule -- a boolean would step a
            //             full-scale channel on at a buffer boundary, and the
            //             Sting's launch is exactly the moment nobody wants a
            //             click.
            //
            // ⚠ THE GATE IS SLEWED OUTSIDE THE `sting_audio_ok` GUARD, the
            // flak_prev_reload_left_s rule two blocks down: a cross-frame
            // follower that only advances when the audio device happens to be
            // up will be found holding a stale value the first frame it comes
            // back, and here that would fade in a full-level drone that is no
            // longer flying. Cheap, and it cannot be wrong.
            {
                const double sting_rail =
                    render::sting_rail_spin_rate(sting_deploy_t) /
                    render::kStingRailRadS;
                const double sting_gate_target =
                    sting.st.active ? 1.0
                                    : 0.35 * std::clamp(sting_rail, 0.0, 1.0);
                sting_audio_gate +=
                    (sting_gate_target - sting_audio_gate) *
                    render::audio_slew_coef(clamped_dt, 0.20);
            }
            if (sting_audio_ok) {
                // Distance from the eye to whichever Sting is making the
                // noise: the flying one if it is up, else the one on the rail
                // at the man's shoulder (the launcher draw's own anchor is a
                // handful of centimetres off `sting_draw`, far inside one
                // attenuation step, so the stance position is close enough and
                // costs no second solve).
                const double sting_dist =
                    glm::length(sting.st.curr.position - pose.eye);
                const double sting_gain =
                    sting_audio_gate * render::sting_distance_gain(sting_dist);
                sting_synth.set_drivers(sting_thr01, sting_gain);
                SetAudioStreamVolume(
                    sting_stream,
                    static_cast<float>(render::kStingStreamLevel));
                // Smoke/debug observability: a sound cannot be screenshot, so
                // SEADS_STING_AUDIO_DBG prints the sounded fundamental and the
                // applied linear gain per frame. Off by default, one getenv,
                // read once (the SEADS_STING_DEPLOY_DEBUG class).
                static const bool sting_audio_dbg =
                    std::getenv("SEADS_STING_AUDIO_DBG") != nullptr;
                if (sting_audio_dbg) {
                    TraceLog(LOG_INFO,
                             "STINGAUD thr=%.3f d=%.1fm gate=%.3f f0=%.1fHz "
                             "gain=%.4f",
                             sting_thr01, sting_dist, sting_audio_gate,
                             sting_synth.voice_hz(), sting_synth.voice_gain());
                }
                // Fed EVERY buffer even when inaudible, and the silence goes
                // THROUGH the room -- both for the reasons written out at the
                // snowmachine block above (raylib replays an unrefilled
                // sub-buffer forever; a bypassed reverb freezes its delay
                // lines holding the last flight's motors).
                const bool sting_audible = sting_audio_gate > 1e-4;
                while (IsAudioStreamProcessed(sting_stream)) {
                    short buf[1024];
                    if (sting_audible) {
                        sting_synth.render(buf, 1024);
                    } else {
                        for (int i = 0; i < 1024; ++i) buf[i] = 0;
                    }
                    sting_verb.process(buf, 1024);
                    UpdateAudioStream(sting_stream, buf, 1024);
                }
            }
            // Gun-fire stutter: firing = trigger held in the instructor (raw
            // mode keeps the LMB free, so no gun roar there). Matches the
            // fire_tick gate closely enough for audio; the synth tails out the
            // last cracks on release. Read-only off the input snapshot.
            if (gun_ok) {
                // While MANNED the held LMB is the FLAK trigger, not the
                // aircraft's -- without this gate the plane's composite gun
                // roar played over the flak (found fixing Chad's 2026-08-28
                // sound ask).
                gun_synth.set_firing(fin.fire_held && !raw_mode &&
                                     !flak_fk.manned);
                // FLAK BOOM (Chad: "a boom sounds"): TWO cannot-miss crack
                // voices (crack + sub-thump each) per round that actually
                // left the barrel this frame -- cadence is the true 7.5 Hz
                // cyclic, twice-triggered for weight. Voice-pool capped.
                // ★ Round 4 (Chad 2026-08-30 "sounds are too quiet for the
                // gun"): each voice now fires at kFlakBoomGain -- the
                // trigger_cannon gain arm scales the voice envelopes and the
                // synth's soft-clip still bounds the sum, so this is a pure
                // loudness dial. He is standing AT a 20 mm's breech.
                const double kFlakBoomGain = 3.2;  // FEEL DIAL (round 5c: 2.2 -> 3.2,
                                                   //   "turn up the flak gun too";
                                                   //   was 1.0 pre-round-4)
                const int fshots = std::min(flak_fk.spawned_accum, 5);
                for (int i = 0; i < fshots; ++i) {
                    gun_synth.trigger_cannon(/*free=*/true, kFlakBoomGain);
                    gun_synth.trigger_cannon(/*free=*/true, kFlakBoomGain);
                }
                flak_fk.spawned_accum = 0;
                // ── STAGE D: THE DISTANT REPORT. Every AI gun's shots reach
                // him attenuated by range -- that is the whole point of the
                // rung: the valley SOUNDS defended from the air.
                //
                // ★ THE SHOT COUNT COMES OFF spawned_total, NOT the
                // accumulator. This is a DISCRETE reader (one trigger per
                // round) and the accumulator's zero is owned by this very
                // block, which is skipped whole when audio fails to open --
                // the Stage C brass-storm failure, restated. Differencing the
                // monotone total cannot be broken by who drains what.
                //
                // ★ The LISTENER is the gun he is standing at when he is
                // standing at one (the camera is at the sight, kilometres from
                // the aircraft), else the aeroplane/sled. This is the ONLY
                // place in the rung that reads the player's position, and it
                // reads it for a GAIN -- never for a target.
                {
                    const glm::dvec3 listener = flak_fk.manned
                                                    ? flak_fk.mount.pos
                                                    : draw_state.position;
                    constexpr double kRefM = 120.0;   // unity-gain distance
                    constexpr double kMaxM = 3000.0;  // beyond: silent
                    int budget = 6;  // triggers/frame, the voice-pool guard
                    for (std::size_t ai = 0;
                         ai < flak_ai.size() &&
                         ai < flak_ai_shots_seen.size();
                         ++ai) {
                        const long long now = flak_ai[ai].fk.spawned_total;
                        long long d = now - flak_ai_shots_seen[ai];
                        flak_ai_shots_seen[ai] = now;
                        if (d <= 0 || budget <= 0) continue;
                        const double dist = glm::length(
                            flak_ai[ai].fk.mount.pos - listener);
                        if (!(dist < kMaxM)) continue;
                        const double gain =
                            std::min(1.0, kRefM / std::max(1.0, dist));
                        if (d > budget) d = budget;
                        for (long long k = 0; k < d; ++k)
                            gun_synth.trigger_cannon(/*free=*/true, gain);
                        budget -= static_cast<int>(d);
                    }
                }
                while (IsAudioStreamProcessed(gun_stream)) {
                    short buf[1024];
                    gun_synth.render(buf, 1024);
                    gun_verb.process(buf, 1024);  // "make ... guns echo"
                    UpdateAudioStream(gun_stream, buf, 1024);
                }
            }
            // ★ THE DRAIN MUST NOT LIVE ONLY BEHIND gun_ok (Stage C's brass
            // storm, generalized): with no audio device (every smoke run, or
            // a failed device) the zero above never runs, spawned_accum grows
            // for the whole flight, and every per-frame reader upstream --
            // recoil, flash, shake, blast -- re-adds the entire history each
            // frame and rides its ceiling forever. Same-frame readers have
            // already consumed this frame's value by this point, so the
            // unconditional zero is a no-op when gun_ok already drained it.
            flak_fk.spawned_accum = 0;
            // ── STAGE D: THE SAME UNCONDITIONAL ZERO for every AI gun. The
            // FlakWorld banner's reader-window contract applies to each one:
            // an accumulator with no drainer grows for the whole flight and
            // every envelope upstream rides its ceiling forever. Same-frame
            // readers (the flash/blast step above) have already consumed it.
            for (app::FlakAiGun& ag : flak_ai) ag.fk.spawned_accum = 0;
            // Explosion + hit one-shots: fire one voice per NEW kill / damaging
            // hit since last frame (the counters advance inside combat_tick).
            // Capped at the voice-pool size so a dense burst can't spin
            // forever.
            if (sfx_ok) {
                // ── FLAK STAGE C: THE RELOAD FOLEY. Two one-shots on the
                // EDGES of reload_left_s -- a bright CLANK as the empty drum
                // comes off, a heavy THUNK-LATCH 4 s later as the fresh one
                // seats. ★ IN CombatSfxSynth, NOT GunSynth: GunSynth's voices
                // ride firing_gain_, which set_firing(false) closes while the
                // flak is manned (the 2026-08-28 boom lesson -- the boom only
                // survives there by setting ShotVoice::free). This synth has
                // no such gate, so foley cannot be silenced by it.
                // ★ NOT in the spawned_accum window: these are timer edges,
                // not shot counts, so the drain below cannot eat them.
                const double frl = flak_fk.reload_left_s;
                if (frl > 0.0 && flak_prev_reload_left_s <= 0.0)
                    sfx_synth.trigger_clank(/*heavy=*/false);  // drum OFF
                else if (frl <= 0.0 && flak_prev_reload_left_s > 0.0)
                    sfx_synth.trigger_clank(/*heavy=*/true);   // drum SEATED
                long long dk = cw.kills - prev_kills;
                long long dh = cw.hits - prev_hits;
                for (long long k = 0; k < dk && k < 8; ++k)
                    sfx_synth.trigger_explosion();
                for (long long h = 0; h < dh && h < 8; ++h)
                    sfx_synth.trigger_hit();
                // ── FLAK ROUND 5 (Chad 2026-08-30: "the secondary boom of
                // the proximity fuse has no sound. There is supposed to be a
                // loud boom and an echo about the mountains."). Two reads of
                // one ask:
                //   * a DAMAGING prox burst (dh while MANNED -- the same
                //     player-only gate as the yellow X) is THE boom, fired
                //     with the two mountain-echo repeats;
                //   * every curtain self-destruct is a distant crump (no
                //     echo of its own -- at 7.5 Hz the crumps ARE the
                //     rumble), differenced off the monotone burst_total
                //     exactly like spawned_total (the Stage C rule: a
                //     discrete reader must own an undrainable count).
                // Caps are voice-pool guards, per frame.
                if (flak_fk.manned)
                    for (long long h = 0; h < dh && h < 3; ++h)
                        sfx_synth.trigger_flak_boom(render::kFlakBoomHitGain,
                                                    /*echo=*/true,
                                                    /*hollow=*/false);
                {
                    long long db =
                        flak_fk.burst_total - flak_prev_burst_total;
                    for (long long b = 0; b < db && b < 3; ++b)
                        sfx_synth.trigger_flak_boom(
                            render::kFlakBoomCurtainGain, /*echo=*/false,
                            /*hollow=*/true);
                }
                while (IsAudioStreamProcessed(sfx_stream)) {
                    short buf[1024];
                    sfx_synth.render(buf, 1024);
                    sfx_verb.process(buf, 1024);
                    UpdateAudioStream(sfx_stream, buf, 1024);
                }
            }
            prev_kills = cw.kills;
            prev_hits = cw.hits;
            // STAGE C: outside the sfx_ok gate on purpose -- if audio is
            // unavailable the edge detector must still track, or the first
            // frame after audio comes back would fire a phantom clank.
            flak_prev_reload_left_s = flak_fk.reload_left_s;
            // ROUND 5: same rule as the reload edge above -- the burst
            // tracker advances even with no audio device, or the first frame
            // after audio comes back would fire a phantom crump volley.
            flak_prev_burst_total = flak_fk.burst_total;
            // T25 sub-mark (this fix): the audio-stream refill loops
            // (wind/engine/gun/sfx synth), split out of the composite
            // app_tick lap.
            // MUSIC bus (Chad's audio rung, 2026-08-17). Two beds cross-faded
            // on app::inside_tunnel — the SAME single-source predicate CAVECAM
            // uses above, so the music and the camera can never disagree about
            // where the tunnel is. The black stope is inside that net (it is
            // the sealed chamber holding the DEEP pumps), so one predicate
            // covers both places Chad named.
            //
            // The director is pure: it computes two volumes and we apply them.
            // Cost per frame is two UpdateMusicStream refills and two
            // SetMusicVolume calls, inside the existing "audio" prof lap.
            {
                // Reuse `cave` — the SAME app::inside_tunnel result the camera
                // already computed this frame. Recomputing it would run
                // TunnelNet::contains a second time (a full ~80-primitive SDF
                // scan whenever the broad phase misses, i.e. exactly while you
                // are inside the net) and book the cost into the "audio" lap.
                const bool deep = cave;
                music_dir.update(deep, clamped_dt);

                // The rumble of the deep workings, and the duck it drives.
                // The duck is INSIDE the asset guard: ducking the bed 14 dB
                // every 16-37 s with nothing audible causing it would read as a
                // bug in the music player, not as a missing file.
                if (stope_rumble.update(deep, clamped_dt) && stope_boom_ok) {
                    PlaySound(stope_boom);  // the DRY blast, stereo, native
                    boom_send.trigger();    // ...and its echo off the walls
                    music_dir.blast();      // occlude the bed under the blast
                }

                // The return swell (see music_return_t): 0 -> 1 over the first
                // few seconds of flight so the bed comes BACK after the intro
                // rather than switching on. Pinned to 1 for the rest of the
                // session, so it is a one-time entrance, not a live dial.
                music_return_t += clamped_dt;
                const double ret = render::intro_music_return(music_return_t);

                if (music_surface_ok) {
                    UpdateMusicStream(music_surface);
                    SetMusicVolume(
                        music_surface,
                        static_cast<float>(ret * music_dir.surface_volume()));
                }
                if (music_deep_ok) {
                    UpdateMusicStream(music_deep);
                    SetMusicVolume(
                        music_deep,
                        static_cast<float>(ret * music_dir.deep_volume()));
                }

                // ⭐ THE TWO ANIMALS AT THE SURFACE PUMPS. Chad, 2026-08-20:
                // the wolf when you reach the Sudbury surface pump, the
                // wildcat near the Onaping/Levack one, "when you fly over the
                // area or snomobile close to it".
                //
                // DISTANCE IS MEASURED ALONG THE GROUND, from the unit
                // directions world/faction_bubbles.h already bakes — not from
                // the lifted pump positions combat::make_pumps builds. Three
                // reasons, and they are the whole design of this block:
                //
                //   1. It works with [conquest] OFF. The lifted positions only
                //      exist inside the conquest branch at startup; the map
                //      still has an Onaping and a Coniston either way, and an
                //      animal that only lives in one game mode is not country.
                //   2. It is altitude-blind, which is what "fly OVER the area"
                //      means. A straight-line 3D distance would hold the wolf
                //      silent directly above the pump at any real altitude and
                //      then fire it on the way back down.
                //   3. It needs no terrain sampling, so it cannot disagree with
                //      the ground under it and costs two dot products a frame.
                //
                // Cosmetic and read-only, like every channel in this block:
                // nothing here is written back, and the scheduler is pure.
                {
                    // On the machine vs in the air. The SAME pair the HUD's
                    // sled_active reads, so the two can never disagree about
                    // which vehicle you are in.
                    const bool on_sled =
                        player.off_aircraft() && player.sled_seeded;
                    const glm::dvec3 listener =
                        on_sled ? sled.position : draw_state.position;
                    const render::CryContext cctx =
                        on_sled ? render::CryContext::kGround
                                : render::CryContext::kFlying;
                    const double lr = glm::length(listener);
                    // `cave` again — the ONE app::inside_tunnel result this
                    // frame. Down in the workings you are under rock and these
                    // are surface animals; hearing the wolf from inside the
                    // black stope would say the sound is stuck to the camera.
                    // Held rather than merely muted: update() is skipped, so
                    // the visit does not silently serve out its gap
                    // underground and cry the instant you surface.
                    if (!cave && lr > 1.0) {
                        const glm::dvec3 up = listener / lr;
                        const auto ground_dist = [&](const glm::dvec3& d) {
                            const double c = std::clamp(
                                glm::dot(up, glm::normalize(d)), -1.0, 1.0);
                            return std::acos(c) * params.R;
                        };
                        const glm::dvec3* const cry_dir[render::kPumpCryCount] =
                            {&world::kPumpSudburySurface,
                             &world::kPumpValleySurface};
                        Sound* const cry_snd[render::kPumpCryCount] = {
                            &wolf_cry, &wildcat_cry};
                        const bool cry_ok[render::kPumpCryCount] = {
                            wolf_cry_ok, wildcat_cry_ok};
                        for (int i = 0; i < render::kPumpCryCount; ++i) {
                            const auto which = static_cast<render::PumpCry>(i);
                            const double dist = ground_dist(*cry_dir[i]);
                            const bool near_pump =
                                render::in_cry_range(dist, cctx);
                            if (!cry_sched[i].update(near_pump, cctx,
                                                     clamped_dt))
                                continue;
                            if (!cry_ok[i]) continue;
                            // The level is evaluated at the DISTANCE YOU ARE AT
                            // when it fires and then fixed for the whole cry —
                            // a raylib Sound has one volume. That is the honest
                            // model anyway: an animal that faded as you flew
                            // away would be an animal attached to your camera.
                            // On the gameplay bus like every other channel
                            // (render/mix_levels.h), so it comes down with the
                            // mix instead of floating above it.
                            SetSoundVolume(
                                *cry_snd[i],
                                static_cast<float>(
                                    render::kGameplayBusGain *
                                    render::cry_gain(which, dist, cctx)));
                            PlaySound(*cry_snd[i]);
                        }

                        // ⭐ THE DISTANT TRAIN, on the same tick and under the
                        // same roof-gate as the animals: down in the workings
                        // you are under rock, and a mainline train heard from
                        // inside the black stope would say the sound is stuck
                        // to the camera. Held rather than muted, so a visit
                        // underground does not serve out its wait in silence
                        // and then sound a train the instant you surface.
                        //
                        // "Near a town" is BUILDING DENSITY, not a curated
                        // list -- there is no town list in this game, and a
                        // town is exactly where the buildings are. The radius
                        // is per-context and it is load-bearing: one ~92 m
                        // index cell is a sub-second event at flying speed,
                        // which would accumulate a few seconds of clock per
                        // overflight against gaps measured in minutes. See
                        // render/town_ambience.h -- that mismatch, not the
                        // missing caller alone, is why the air heard nothing.
                        const render::TownContext tctx =
                            on_sled ? render::TownContext::kGround
                                    : render::TownContext::kFlying;
                        bool near_town = false;
                        if (town_colliders != nullptr) {
                            near_town = render::is_town(
                                town_colliders->prisms_near_m(
                                    listener, render::town_radius_m(tctx)),
                                tctx);
                        }
                        const bool train_now =
                            train_amb.update(near_town, tctx, clamped_dt);
                        if (train_now && train_pass_ok) {
                            // One level for the whole pass, like the cries: a
                            // raylib Sound has one volume, and a train that
                            // faded as you flew on would be a train attached
                            // to your camera. On the gameplay bus with every
                            // other channel.
                            SetSoundVolume(
                                train_pass,
                                static_cast<float>(render::kGameplayBusGain *
                                                   render::train_gain(tctx)));
                            PlaySound(train_pass);
                        }

                        // ⭐ THE CHURCH BELL, two minutes behind that train.
                        //
                        // Armed off `train_now` -- the SCHEDULER's tick, not
                        // the PlaySound above. If the train asset failed to
                        // load, the town still has a church; making one
                        // missing file silence two channels is the kind of
                        // coupling the blast's duck already got wrong once (it
                        // ducked the music for an explosion that had not
                        // loaded).
                        if (train_now) bell_tower.on_train();

                        // Same near_town, same tctx, same clamped_dt as the
                        // train: one answer to "am I over Chelmsford", read by
                        // both channels. A second density lookup here would be
                        // a second ~O(cells) scan AND a second opinion.
                        const render::BellTick bell =
                            bell_tower.update(near_town, clamped_dt);
                        if (bell.strike && church_bell_ok) {
                            // The level is fixed at the START of the peal and
                            // held for every strike in it. Re-evaluating per
                            // chime would step the level mid-peal if you took
                            // off halfway through, and a bell that changes
                            // loudness between chime four and chime five is a
                            // mixer, not a tower.
                            if (bell.peal_start) {
                                bell_level = static_cast<float>(
                                    render::kGameplayBusGain *
                                    render::bell_gain(tctx));
                            }
                            // Round-robin the ring so a strike never lands on
                            // the voice still ringing from the last one. This
                            // is the whole reason the aliases exist.
                            Sound& voice = church_bell_voice[church_bell_next];
                            church_bell_next = (church_bell_next + 1) %
                                               render::kBellVoiceCount;
                            SetSoundVolume(voice, bell_level);
                            PlaySound(voice);
                        }
                    } else {
                        // Under rock, or at the planet's centre. The cries and
                        // the train HOLD here (their schedulers are simply not
                        // ticked, so a visit underground cannot serve out its
                        // wait in silence and then fire the instant you
                        // surface) and the bell's countdown holds with them for
                        // the same reason.
                        //
                        // But a peal IN PROGRESS is abandoned, which is the one
                        // thing the hold must not do to it: freezing at chime
                        // four and resuming when you came up would be a bell
                        // that waited underground for you. Descending into the
                        // stope mid-peal ends the peal, exactly as flying out
                        // of town does.
                        bell_tower.abandon_peal();
                    }
                }

                // The blast's echo, refilled HERE -- after the rumble
                // scheduler above, not before it. Triggering the send and then
                // waiting a frame to feed it would add a whole buffer of queue
                // latency on top of the room's own 25 ms predelay, which reads
                // as a second boom rather than as a reflection.
                //
                // Fed EVERY frame, playing or not: the voice renders silence
                // when idle and the room needs that silence to flush its tail
                // out. wet_only means what reaches the mix is the room alone --
                // the dry blast came from PlaySound.
                if (boom_send_ok) {
                    while (IsAudioStreamProcessed(boom_send_stream)) {
                        short buf[1024];
                        boom_send.render(buf, 1024);
                        boom_verb.process(buf, 1024);
                        UpdateAudioStream(boom_send_stream, buf, 1024);
                    }
                }
            }
            if (g_prof.on) g_prof.mark("audio");
        }
        // Vertical lens shift (SPEC §9.2 framing): centers the resting reticle
        // for the current chase framing (auto-tracks distance/height).
        // Instructor mode only — raw mode draws no reticle and keeps the plane
        // centered.
        // Current FOV (RMB-zoom eases it) — ONE source for the Camera3D, the
        // lens frustum, the reticle projection, and this lens shift, so a zoom
        // can never desync the reticle from the scene. The lens shift is
        // recomputed at the zoomed fov (narrower fov => larger NDC shift for
        // the same atan(height/distance) tilt), so the resting reticle stays
        // centered at any zoom.
        info.fovy_deg = fovy;
        info.lens_shift_ndc =
            raw_mode ? 0.0
                     : render::lens_shift_ndc(
                           eff_chase.height, eff_chase.distance,
                           fovy * (3.14159265358979323846 / 180.0));
        // ★★ VERDICT-ROUND-2 FIX, second half (Chad 2026-08-29: "zoom still
        // magnifies the bore, not the pipper"): the flak sight INHERITS this
        // chase-cam tilt shift, and because the shift is recomputed at the
        // current fov it GROWS as the zoom narrows — measured 1.057 NDC at
        // full zoom (21.43 deg), which renders the camera-forward point (the
        // zoom-blended pipper) at pixel y ≈ 1110 on a 1080-px screen: off the
        // bottom. So even a perfectly-centred look axis could never show the
        // pipper at centre. While manned, fade the shift out by zoom_t: at
        // zoom_t = 0 the SIGNED at-rest sight picture is bit-identical (the
        // shift it shipped with is untouched); at full zoom the frustum is
        // symmetric and the blended pipper sits dead centre — Chad's spec
        // ("fully at max zoom; released it eases back"). The plane's own
        // chase zoom is untouched (gated on flak_fk.manned).
        if (flak_fk.manned) info.lens_shift_ndc *= (1.0 - zoom_t);
        // DEBUG terrain-review camera (world thread): an oblique aerial over
        // the basin center (+Z) so a --smoke shot frames the ground the level
        // chase cam can't. Seam-safe: SMOKE-only + env-gated + not in probe
        // mode, so it never touches live play or the deterministic probe. Tune
        // with SEADS_OBL_UP / SEADS_OBL_BACK (metres). Not a shipped view.
        if (smoke_frames > 0 && !probe_mode && std::getenv("SEADS_OBLIQUE")) {
            const char* up_s = std::getenv("SEADS_OBL_UP");
            const char* bk_s = std::getenv("SEADS_OBL_BACK");
            const double up_m = up_s ? std::atof(up_s) : 7000.0;
            const double bk_m = bk_s ? std::atof(bk_s) : 9000.0;
            // SEADS_OBL_PAN / SEADS_OBL_TILT (degrees) rotate the look-center
            // off the midpoint so a smoke shot can frame a specific feature (a
            // named far lake, the antipode) — terrain-bake diagnosis. 0 = the
            // midpoint.
            const char* pan_s = std::getenv("SEADS_OBL_PAN");
            const char* tilt_s = std::getenv("SEADS_OBL_TILT");
            const double pan = (pan_s ? std::atof(pan_s) : 0.0) * PI / 180.0;
            const double tilt = (tilt_s ? std::atof(tilt_s) : 0.0) * PI / 180.0;
            // Rodrigues rotation (no new includes): axis assumed unit.
            const auto rot = [](glm::dvec3 v, glm::dvec3 ax, double a) {
                ax = glm::normalize(ax);
                const double cc = std::cos(a), ss = std::sin(a);
                return v * cc + glm::cross(ax, v) * ss +
                       ax * glm::dot(ax, v) * (1.0 - cc);
            };
            const glm::dvec3 north{0.0, 1.0, 0.0};
            glm::dvec3 c = rot({0.0, 0.0, 1.0}, north, pan);  // azimuth about N
            const glm::dvec3 eaxis = glm::normalize(glm::cross(north, c));
            c = glm::normalize(rot(c, eaxis, tilt));  // toward the pole
            const glm::dvec3 east = glm::normalize(glm::cross(north, c));
            pose.eye = c * (params.R + up_m) - east * bk_m;
            pose.target = c * params.R;
            pose.up = c;
        }
        // Rig-inspection debug cam (smoke-only, env-gated): orbit the PLANE
        // (not the planet like SEADS_OBLIQUE) so rig-D asset work can frame the
        // gear / canopy / underside from any azimuth. SEADS_RIGCAM=1;
        // SEADS_RIG_AZ deg (0=behind, 180=nose-on), SEADS_RIG_EL deg (up),
        // SEADS_RIG_DIST m. Body frame: nose -Z, up +Y. Not a shipped view.
        if (smoke_frames > 0 && !probe_mode && std::getenv("SEADS_RIGCAM")) {
            const auto envf = [](const char* k, double d) {
                const char* s = std::getenv(k);
                return s ? std::atof(s) : d;
            };
            const double az = envf("SEADS_RIG_AZ", 150.0) * PI / 180.0;
            const double el = envf("SEADS_RIG_EL", 12.0) * PI / 180.0;
            const double dist = envf("SEADS_RIG_DIST", 13.0);
            const glm::dvec3 off_body{dist * std::cos(el) * std::sin(az),
                                      dist * std::sin(el),
                                      dist * std::cos(el) * std::cos(az)};
            const glm::dquat q = glm::dquat(draw_state.orientation);
            pose.eye = draw_state.position + q * off_body;
            pose.target = draw_state.position;
            pose.up = glm::normalize(draw_state.position);  // local-up
        }
        // FLAK CAM (smoke-only, env-gated — the SEADS_RIGCAM pattern): frame
        // flak gun [i] from dist/az/el so F-LOAD/DRAW can be certified
        // VISUALLY (the green gate is blind to the app binary; CLAUDE.md
        // Learned). SEADS_FLAKCAM="dist_m,az_deg,el_deg[,gun_index]" — az 0 =
        // in front of the gun (down its threat bearing, looking back), about
        // local-up; el above the horizon. Not a shipped view.
        if (smoke_frames > 0 && !probe_mode && !flak_guns.empty()) {
            if (const char* fc = std::getenv("SEADS_FLAKCAM")) {
                double fdist = 8.0, faz = 30.0, fel = 10.0;
                int fgi = 0;
                std::sscanf(fc, "%lf,%lf,%lf,%d", &fdist, &faz, &fel, &fgi);
                if (fgi < 0) fgi = 0;
                if (fgi >= static_cast<int>(flak_guns.size()))
                    fgi = static_cast<int>(flak_guns.size()) - 1;
                const render::FlakDraw& fg = flak_guns[fgi];
                const render::flak::MountFrame mf =
                    render::flak::make_mount_frame(fg.pos, fg.up, fg.fwd0);
                const double fa = faz * PI / 180.0, fe = fel * PI / 180.0;
                const glm::dvec3 fdir =
                    mf.fwd0 * (std::cos(fe) * std::cos(fa)) +
                    mf.right0 * (std::cos(fe) * std::sin(fa)) +
                    mf.up * std::sin(fe);
                const glm::dvec3 ftgt = fg.pos + mf.up * 1.2;
                pose.eye = ftgt + fdir * fdist;
                pose.target = ftgt;
                pose.up = mf.up;
                // F-POSE: THIS frame the camera is external, so the manned
                // gun's gunner draws full-body and solid. Set where the
                // camera is actually taken, never inferred from the env
                // elsewhere -- live play can never reach here, so the
                // sight view can never leak a figure or vice versa.
                info.flak_cam_external = true;
                info.flak_gunner_alpha = 1.0f;
            }
        }
        // T6a — TUNNEL MOUTH CAM: external view from outside the Errington
        // mouth. SEADS_TUNCAM_MOUTH="dist_m,az_deg,el_deg"
        //   dist_m  — distance from the mouth surface point [m]
        //   az_deg  — azimuth about local-up at the mouth (0 = toward Murray)
        //   el_deg  — elevation above the local horizon (+ = up)
        // Eye is placed at (dist_m) from the mouth surface point along the
        // direction (azimuth az about local-up, elevation el above horizon);
        // target is the mouth surface point; up = local_up at the mouth.
        // All computed in planet frame — no fixed world axes. Smoke-only.
        if (smoke_frames > 0 && !probe_mode &&
            std::getenv("SEADS_TUNCAM_MOUTH")) {
            double dist_m = 500.0, az_deg = 0.0, el_deg = 20.0;
            char which[16] = "errington";  // T14b: optional 4th token "murray"
            std::sscanf(std::getenv("SEADS_TUNCAM_MOUTH"), "%lf,%lf,%lf,%15s",
                        &dist_m, &az_deg, &el_deg, which);
            const bool murray = which[0] == 'm';
            const double az = az_deg * (PI / 180.0);
            const double el = el_deg * (PI / 180.0);
            // Surface point: mouth unit dir * sphere radius at the surface.
            // Use params.R (the nominal sphere radius). The mouth direction IS
            // local_up at that point by definition of the planet frame.
            const glm::dvec3 mouth_up =
                glm::normalize(murray ? world::kTunnelMouthMurray
                                      : world::kTunnelMouthErrington);
            const glm::dvec3 mouth_pt = mouth_up * params.R;
            // The OTHER mouth projected off mouth_up gives the azimuth
            // reference (az=0 points toward it, along the underground route).
            const glm::dvec3 other = murray ? world::kTunnelMouthErrington
                                            : world::kTunnelMouthMurray;
            const glm::dvec3 toward_murray =
                glm::normalize(other - glm::dot(other, mouth_up) * mouth_up);
            // A tangent perpendicular to both mouth_up and toward_murray
            // (rightward).
            const glm::dvec3 right_tang =
                glm::normalize(glm::cross(toward_murray, mouth_up));
            // Rodrigues: rotate toward_murray by az about mouth_up, then tilt
            // by el above the horizon.
            const auto rodrigues = [](glm::dvec3 v, glm::dvec3 ax,
                                      double a) -> glm::dvec3 {
                const double cc = std::cos(a), ss = std::sin(a);
                return v * cc + glm::cross(ax, v) * ss +
                       ax * glm::dot(ax, v) * (1.0 - cc);
            };
            glm::dvec3 horiz = rodrigues(toward_murray, mouth_up, az);
            // Horizon-elevation tilt: rotate horiz up by el about the local
            // perpendicular (right in the az plane).
            const glm::dvec3 az_right =
                glm::normalize(glm::cross(horiz, mouth_up));
            const glm::dvec3 cam_dir = rodrigues(horiz, az_right, el);
            pose.target = mouth_pt;
            pose.eye = mouth_pt + cam_dir * dist_m;
            pose.up = mouth_up;
            (void)right_tang;  // computed for reference; az_right is the
                               // rotation axis in the tilted plane
            std::printf(
                "[TUNCAM_MOUTH] dist=%.1f az=%.1f el=%.1f deg | "
                "eye=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f)\n",
                dist_m, az_deg, el_deg, pose.eye.x, pose.eye.y, pose.eye.z,
                pose.target.x, pose.target.y, pose.target.z);
        }
        // T6a — TUNNEL BORE CAM: eye on the spine, looking along the tangent.
        // SEADS_TUNCAM_BORE="s_m[,back_off]"
        //   s_m      — arc length from Errington mouth along the spine [m]
        //   back_off — metres to pull the eye backward along the tangent (opt,
        //              default 0); useful for a slight pull-back view.
        // The eye sits at the interpolated spine point; forward = spine tangent
        // toward Murray; up = local radial up projected off the tangent.
        // Smoke-only. tunnel_net must be built (game.tunnel.enabled) for this
        // to show the tunnel interior; it works regardless (just won't see
        // walls if tunnel is disabled in config).
        if (smoke_frames > 0 && !probe_mode &&
            std::getenv("SEADS_TUNCAM_BORE")) {
            double s_target = 500.0, back_off = 0.0;
            // ATMOSPHERE AS-1: an optional 3rd token "out" flips the look
            // direction to face BACK toward the Errington mouth -- the "stand
            // in the mouth and look out" view, which is the ONE view that can
            // prove the underground snow gate is per-FLAKE and not per-eye
            // (snow outside the mouth still falls; nothing under the rock
            // does). Smoke-only, same as the rest of this block.
            char bore_look[16] = "in";
            std::sscanf(std::getenv("SEADS_TUNCAM_BORE"), "%lf,%lf,%15s",
                        &s_target, &back_off, bore_look);
            const bool bore_out = std::strncmp(bore_look, "out", 3) == 0;
            // Walk the spine accumulating arc length to find the interpolation
            // point. The spine nodes are Errington-first (T1 convention).
            const auto& sp = tunnel_net.spine;
            glm::dvec3 eye_pt{0.0};
            glm::dvec3 tangent{0.0, 1.0, 0.0};  // fallback
            if (sp.size() >= 2) {
                double arc = 0.0;
                bool found = false;
                for (std::size_t i = 0; i + 1 < sp.size(); ++i) {
                    const glm::dvec3 seg = sp[i + 1].pos - sp[i].pos;
                    const double seg_len = glm::length(seg);
                    if (seg_len < 1e-9) continue;
                    if (arc + seg_len >= s_target) {
                        const double t = (s_target - arc) / seg_len;
                        eye_pt = sp[i].pos + seg * t;
                        tangent = glm::normalize(seg);  // toward Murray
                        found = true;
                        break;
                    }
                    arc += seg_len;
                }
                if (!found) {
                    // Past the end: use the last segment.
                    const std::size_t n = sp.size();
                    eye_pt = sp[n - 1].pos;
                    const glm::dvec3 seg = sp[n - 1].pos - sp[n - 2].pos;
                    if (glm::length(seg) > 1e-9) tangent = glm::normalize(seg);
                }
            } else if (!sp.empty()) {
                eye_pt = sp[0].pos;
            }
            // Pull backward along the tangent if requested.
            if (back_off > 0.0) eye_pt -= tangent * back_off;
            // Local radial up at this spine point, projected off the tangent.
            const glm::dvec3 radial_up = glm::normalize(eye_pt);
            const glm::dvec3 cam_up = glm::normalize(
                radial_up - glm::dot(radial_up, tangent) * tangent);
            pose.eye = eye_pt;
            pose.target = eye_pt + tangent * (bore_out ? -500.0 : 500.0);
            pose.up = (glm::length(cam_up) > 0.1) ? cam_up : radial_up;
            std::printf(
                "[TUNCAM_BORE] s=%.1f back_off=%.1f | "
                "eye=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f)\n",
                s_target, back_off, pose.eye.x, pose.eye.y, pose.eye.z,
                pose.target.x, pose.target.y, pose.target.z);
        }
        // T11 — THE ARENA CAM: an interior view across the shallow dogfight
        // arena. SEADS_TUNCAM_CAVERN="r_m,az_deg[,look]"
        //   r_m     — eye radius (arena_floor < r_m < arena_apex) on a great
        //             circle through the two mouths (bores/breaches in view)
        //   az_deg  — azimuth of the eye about that great circle (0 = toward
        //             the Errington mouth's arena crossing)
        //   look    — target: "core" (the glowing center below, DEFAULT),
        //             "breach" (the Errington bore's arena breach), or
        //             "ceiling" (the arena ceiling straight up from the eye)
        // All in planet frame. Smoke-only (tunnel must be enabled to see
        // walls).
        if (smoke_frames > 0 && !probe_mode &&
            std::getenv("SEADS_TUNCAM_CAVERN")) {
            double r_m = tunnel_net.arena_r, az_deg = 0.0;
            char look[32] = "core";
            std::sscanf(std::getenv("SEADS_TUNCAM_CAVERN"), "%lf,%lf,%31s",
                        &r_m, &az_deg, look);
            const double az = az_deg * (PI / 180.0);
            // A great circle through the two mouths: basis (E, b) with normal
            // n.
            const glm::dvec3 E = glm::normalize(world::kTunnelMouthErrington);
            const glm::dvec3 M = glm::normalize(world::kTunnelMouthMurray);
            const glm::dvec3 n = glm::normalize(glm::cross(E, M));
            const glm::dvec3 b = glm::normalize(glm::cross(n, E));
            const glm::dvec3 dir =
                glm::normalize(std::cos(az) * E + std::sin(az) * b);
            const glm::dvec3 eye_pt = dir * r_m;
            pose.eye = eye_pt;
            // The breach targets (T14): the TRUE crossings of the bore
            // centreline with the arena wall — the same locator the mesh
            // cut/beacons use, so the smoke shot looks at the real opening.
            // "breach" = Errington side; "breach2" = Murray side (T14b).
            glm::dvec3 breach = tunnel_net.spine.front().pos;
            const std::vector<render::Breach> tun_breaches =
                render::breach_points(tunnel_net);
            if (!tun_breaches.empty())
                breach = (std::strncmp(look, "breach2", 7) == 0)
                             ? tun_breaches.back().pos
                             : tun_breaches.front().pos;
            if (std::strncmp(look, "breach", 6) == 0)
                pose.target = breach;
            else if (std::strncmp(look, "ceiling", 7) == 0)
                pose.target = eye_pt + dir * 3000.0;  // straight up (radial)
            else
                pose.target = glm::dvec3{0.0, 0.0, 0.0};  // the glowing core
            const glm::dvec3 view = glm::normalize(pose.target - pose.eye);
            glm::dvec3 up = dir;  // local radial up
            up = up - glm::dot(up, view) * view;
            pose.up = glm::length(up) > 0.1
                          ? glm::normalize(up)
                          : glm::normalize(glm::cross(view, n));
            std::printf(
                "[TUNCAM_CAVERN] r=%.1f az=%.1f look=%s | "
                "eye=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f)\n",
                r_m, az_deg, look, pose.eye.x, pose.eye.y, pose.eye.z,
                pose.target.x, pose.target.y, pose.target.z);
        }
        // S-pumpcube — THE PUMP CAM: an interior view of ONE conquest pump.
        // SEADS_TUNCAM_PUMP="index[,dist_m][,el_deg]"
        //   index   — conquest pump index: 0 Valley SURFACE, 1 Sudbury SURFACE,
        //             2 Valley DEEP (stope), 3 Sudbury DEEP (stope)
        //   dist_m  — eye distance from the pump (default 140)
        //   el_deg  — elevation above the pump's local horizon (default 12)
        // Exists for the same reason SEADS_TUNCAM_CAVERN does: the two DEEP
        // pumps sit in a sealed unlit chamber, the green gate is structurally
        // blind to anything drawn (no ctest runs seads.exe), and "is the neon
        // ownership frame legible in the dark" can ONLY be answered by a
        // screenshot taken down there. Planet frame throughout, smoke-only.
        if (smoke_frames > 0 && !probe_mode &&
            std::getenv("SEADS_TUNCAM_PUMP")) {
            int idx = 2;
            double dist_m = 140.0, el_deg = 12.0;
            std::sscanf(std::getenv("SEADS_TUNCAM_PUMP"), "%d,%lf,%lf", &idx,
                        &dist_m, &el_deg);
            if (idx >= 0 && idx < combat::kNumPumps) {
                const glm::dvec3 pp = cq.state.pumps[idx].pos;
                const glm::dvec3 pu_up = glm::normalize(pp);
                // Look along the underground RUN so the frame is seen from a
                // pilot's transit direction, not from an arbitrary world axis.
                const glm::dvec3 run =
                    glm::normalize(world::kTunnelMouthMurray) -
                    glm::normalize(world::kTunnelMouthErrington);
                glm::dvec3 tang = run - glm::dot(run, pu_up) * pu_up;
                tang = (glm::length(tang) > 1e-6) ? glm::normalize(tang)
                                                  : glm::dvec3{0.0, 0.0, 1.0};
                const double el = el_deg * (PI / 180.0);
                const glm::dvec3 dir =
                    glm::normalize(tang * std::cos(el) + pu_up * std::sin(el));
                pose.eye = pp + dir * dist_m;
                pose.target = pp;
                glm::dvec3 up = pu_up;
                const glm::dvec3 view = glm::normalize(pose.target - pose.eye);
                up = up - glm::dot(up, view) * view;
                pose.up = (glm::length(up) > 0.1) ? glm::normalize(up) : tang;
                std::printf(
                    "[TUNCAM_PUMP] idx=%d %s faction=%d dist=%.1f el=%.1f | "
                    "eye=(%.1f,%.1f,%.1f) pump=(%.1f,%.1f,%.1f)\n",
                    idx, cq.state.pumps[idx].surface ? "SURFACE" : "DEEP",
                    cq.state.pumps[idx].faction, dist_m, el_deg, pose.eye.x,
                    pose.eye.y, pose.eye.z, pp.x, pp.y, pp.z);
            } else {
                std::printf("[TUNCAM_PUMP] index %d out of range (%zu pumps)\n",
                            idx, static_cast<std::size_t>(combat::kNumPumps));
            }
        }
        // T12 — THE CORE-WINDOW CAM (SEADS_TUNCAM_WINDOW) is GONE: the core
        // window is sealed (Chad's round-9 fly: "cover up the core"). The arena
        // is viewed via SEADS_TUNCAM_CAVERN.
        info.frame_count = render_frame++;  // app-owned grain seed (post pass)
        // T25 (this fix): sim_combat/hud_vortex/audio are now their own laps
        // (marked above); this residual "app_tick" lap is only whatever
        // FrameInfo/camera/debug-key work sits between the audio mark and
        // here — near-zero. The draw passes lap themselves through
        // info.prof_mark.
        if (g_prof.on) {
            g_prof.mark("app_tick");
            info.prof_mark = &prof_mark_thunk;
        }
        // ★ L3: the app's own last overlay -- the spawn menu, drawn INSIDE the
        // frame draw_frame owns (render::FrameInfo::overlay), so the world is
        // presented once, with the panel on it. Null on every other frame.
        if (spawn_menu.open) {
            info.overlay = +[](void* ctx) {
                app::spawn_menu_draw(
                    *static_cast<const app::SpawnMenuState*>(ctx));
            };
            info.overlay_ctx = &spawn_menu;
        }
        // ★ ROAD-REPAIR E2 -- THE GRAZING CAMERA, last word on the pose.
        // Every earlier pose path (chase, instructor, sled, walker, freelook)
        // has already run and is simply overwritten: the flash camera is a
        // FIXED station on a census junction node, not a body's camera, and it
        // must not inherit one metre of any body's motion. The ground radius
        // is the SAME drawn surface the census measured (snow_field's own
        // binding), never a second one.
        if (flash_site != nullptr) {
            const world::SnowpackField& sf = snow_field;
            const std::function<double(glm::dvec3)> gr =
                [&sf](glm::dvec3 d) -> double {
                if (sf.drawn_radius_fn) return sf.drawn_radius_fn(d);
                if (sf.facet_radius_fn) return sf.facet_radius_fn(d);
                return sf.hf != nullptr ? sf.hf->radius_at(d) : 15000.0;
            };
            pose = app::flash_camera(flash_site->dir, flash_p, frames, gr);
            if (frames == 0)
                std::printf("[FLASH] graze=%.3f deg eye_alt=%.2f m range=%.2f m\n",
                            app::flash_graze_deg(pose),
                            glm::length(pose.eye) - gr(glm::normalize(pose.eye)),
                            glm::length(pose.target - pose.eye));
        }
        // AS-1 EVIDENCE LINE (atmosphere rung, smoke-only). A screenshot of
        // a dark cavern cannot by itself prove "zero flakes" -- a --smoke
        // run is not bit-reproducible run to run -- so the rig also PRINTS
        // the number the underground gate turns on: the tunnel-net signed
        // distance at the FINAL eye, i.e. after every TUNCAM override
        // above. < 0 == inside the net, and the renderer's whole-box
        // decision then skips the pass entirely. Same single-source SDF
        // app::inside_tunnel reads.
        if (smoke_frames > 0 && snow_force_smoke != nullptr &&
            env.tunnels != nullptr)
            std::printf(
                "[SNOW_ROCK] full_sd=%.2f roofed_sd=%.2f m (<0 = inside; the "
                "GATE reads the ROOFED one) forced_intensity=%.3f\n",
                env.tunnels->signed_distance(pose.eye),
                env.tunnels->roofed_signed_distance(pose.eye),
                info.precip_intensity);
        const auto t_draw0 = std::chrono::steady_clock::now();
        render::draw_frame(draw_state, params, pose, info, env_ptr);
        // ★ ROAD-REPAIR E2: one PNG per frame. TakeScreenshot reads the
        // presented framebuffer, and draw_frame ends with EndDrawing, so this
        // is the frame that was just shown -- the same call the --smoke shot
        // makes, made N times.
        if (flash_site != nullptr) {
            char fp[512];
            std::snprintf(fp, sizeof fp, "%s_f%02d.png", flash_out, frames);
            // raylib's TakeScreenshot drops any directory and writes the BASE
            // name into the working directory, so a prefix with a path in it
            // silently lands in the repo root. Shoot the base name, then move
            // it -- one rename, no new file writer.
            const char* base = fp;
            for (const char* q = fp; *q != '\0'; ++q)
                if (*q == '/' || *q == '\\') base = q + 1;
            TakeScreenshot(base);
            if (base != fp && std::rename(base, fp) != 0)
                std::fprintf(stderr, "[FLASH] could not move %s -> %s\n", base,
                             fp);
        }
        if (g_prof.on) g_prof.end();
        // Smoke-only per-frame timing (perf measurement, no gameplay change): a
        // steady-state average over the run (past a warm-up that excludes the
        // lazy first-frame mesh/shader builds) reflects GPU-bound frame cost
        // even without vsync, because the driver's frame queue backs up onto
        // the GPU after a few frames. min/max bound the jitter (the
        // "choppiness").
        if (smoke_frames > 0) {
            const auto t_draw1 = std::chrono::steady_clock::now();
            const double ms =
                std::chrono::duration<double, std::milli>(t_draw1 - t_draw0)
                    .count();
            constexpr int kSmokeTimeWarmup = 20;  // skip lazy-build frames
            if (frames >= kSmokeTimeWarmup) {
                smoke_ms_sum += ms;
                smoke_ms_min = std::min(smoke_ms_min, ms);
                smoke_ms_max = std::max(smoke_ms_max, ms);
                ++smoke_ms_n;
                // Winter S0 / D2: retain every frame so the readout can report
                // PERCENTILES. avg/min/max cannot answer S6's question -- a
                // single 200 ms hitch moves max and barely moves avg, and it is
                // exactly the shape dense vegetation is expected to add.
                // p95/p99 are where that shows up.
                smoke_ms_all.push_back(ms);
            }
        }

        if (smoke_frames > 0 && ++frames >= smoke_frames) {
            if (smoke_ms_n > 0) {
                std::printf(
                    "[SMOKE_TIMING] frames=%d avg=%.3f ms min=%.3f ms "
                    "max=%.3f ms (%.1f fps avg)\n",
                    smoke_ms_n, smoke_ms_sum / smoke_ms_n, smoke_ms_min,
                    smoke_ms_max, 1000.0 * smoke_ms_n / smoke_ms_sum);
                // S0/D2 — the provenance-stamped percentile line. Without the
                // GPU string and the bake_id this number cannot be compared at
                // S6 and is worthless (RT-6); the caller adds commit + bake_id.
                std::vector<double> q = smoke_ms_all;
                std::sort(q.begin(), q.end());
                const auto pct = [&q](double p) {
                    if (q.empty()) return 0.0;
                    const std::size_t i = static_cast<std::size_t>(
                        p / 100.0 * static_cast<double>(q.size() - 1) + 0.5);
                    return q[std::min(i, q.size() - 1)];
                };
                // Renderer settings are stated here because they are set here
                // (InitWindow 1920x1080, MSAA 4x, vsync dropped in smoke). The
                // GPU string is captured by the caller from the OS -- raylib
                // exposes no portable renderer-name query, and guessing at one
                // is how a baseline acquires a field nobody can trust.
                std::printf(
                    "[SMOKE_PCTL] n=%zu p50=%.3f p95=%.3f p99=%.3f ms "
                    "msaa=4x size=1920x1080 vsync=off\n",
                    q.size(), pct(50.0), pct(95.0), pct(99.0));
            }
            g_prof.report();  // SEADS_PROF=1: the per-pass mean cost table
            if (probe_mode)
                run_probe_measurement(pose, fovy, info.lens_shift_ndc,
                                      probe_place, probe_geom, probe_level_fwd,
                                      probe_alt, probe_range, probe_season,
                                      probe_day, probe_csv);
            if (smoke_gear)
                std::fprintf(stderr, "[smoke_gear] state.gear=%.3f\n",
                             draw_state.gear);
            if (shot_path != nullptr) TakeScreenshot(shot_path);
            break;
        }
    }
    if (engine_ok) UnloadAudioStream(engine_stream);
    if (gun_ok) UnloadAudioStream(gun_stream);
    if (sfx_ok) UnloadAudioStream(sfx_stream);
    if (sled_ok) UnloadAudioStream(sled_stream);
    if (sting_audio_ok) UnloadAudioStream(sting_stream);
    if (boom_send_ok) UnloadAudioStream(boom_send_stream);
    // Soundbank channels. These MUST be freed before CloseAudioDevice() below,
    // which is nested inside the wind_ok guard — anything unloaded after that
    // call is unloaded against a dead device.
    if (music_surface_ok) UnloadMusicStream(music_surface);
    if (music_deep_ok) UnloadMusicStream(music_deep);
    if (stope_boom_ok) UnloadSound(stope_boom);
    if (wolf_cry_ok) UnloadSound(wolf_cry);
    if (wildcat_cry_ok) UnloadSound(wildcat_cry);
    if (train_pass_ok) UnloadSound(train_pass);
    // The bell's alias ring comes down BEFORE the sound it aliases. The
    // aliases do not own the sample data (that is the point of them), so
    // unloading the source first would leave four voices pointing at a freed
    // buffer for the length of this loop.
    if (church_bell_ok) {
        for (int v = 0; v < render::kBellVoiceCount; ++v) {
            UnloadSoundAlias(church_bell_voice[v]);
        }
        UnloadSound(church_bell);
    }
    if (wind_ok) {
        UnloadAudioStream(wind_stream);
        CloseAudioDevice();
    }
    // FLUSH-ON-EXIT (Chad 2026-07-24, the lost best-flight lesson): closing
    // the window mid-recording used to drop the whole capture — F9 was a
    // pure toggle and the buffer died with the process ("I hit record after
    // I decided this is the best" -> nothing on disk). A live recording is
    // ALWAYS flushed on the way out; F9-stop remains the deliberate save.
    if (f9_recording && !felt_recorder.records().empty()) {
        felt_recorder.flush(next_felt_flight_name(), kFeltFlightVersionTag);
    }
    // ★ SLED TAPE R1 clean exit: window closed mid-drive — same contract as
    // dismount (drain + footer + close); a crash before this line still left
    // a valid headerless-footer tape on disk via the 1 s drain.
    sled_tape_close();
    // CONQUEST TAPE clean exit (spec §4): drain whatever the periodic flush
    // above hasn't yet, write the fnv1a footer (clean-exit only — a tape
    // without it is still a valid crash tape), flush, close.
    if (conquest_on) {
        std::string buf;
        if (conquest_tape->drain(buf)) conquest_tape_file << buf;
        conquest_tape_file << conquest_tape->footer();
        conquest_tape_file.flush();
        conquest_tape_file.close();
    }
    render::unload_post();  // free the S1 post FBO + shader before the GL
                            // context
    CloseWindow();
    return 0;
}
