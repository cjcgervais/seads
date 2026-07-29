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
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "app/instructor_tick.h"
#include "app/loop.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "config/load_scenario.h"
#include "config/load_world.h"
#include "drone/drone.h"
#include "input/aim_curve.h"
#include "input/live_input.h"
#include "input/raw_input.h"
#include "raylib.h"
#include "render/camera.h"
#include "render/draw.h"
#include "render/interp.h"
#include "render/readout.h"
#include "render/vortex.h"
#include "render/wind_audio.h"
#include "sim/aero.h"
#include "sim/state.h"
#include "sim/world.h"

namespace {

// One hitch clamp for the whole frame path: the accumulator caps sim time
// with it, and the device throttle sweep uses the SAME cap so device
// wall-time can never outrun sim time across a debugger pause.
constexpr double kMaxFrameDt = 0.25;

}  // namespace

int main(int argc, char** argv) {
    int smoke_frames = 0;
    const char* shot_path = nullptr;
    // v5 HUD RESTYLE VERIFY (Chad 2026-07-23, DEBUG-ONLY smoke args): two
    // optional trailing args drive a ONE-SHOT direct nudge of loop.aim at
    // frame 30 (see the application site below) so a screenshot can show the
    // off-screen mouse glyph / the split-S fear read without needing a live
    // hand on the mouse. Neither arg exists outside --smoke.
    double smoke_offset_aim_deg = 0.0;  // yaw the aim LEFT this many degrees
    double smoke_aim_down_deg = 0.0;    // pitch the aim DOWN this many degrees
    if (argc >= 3 && std::strcmp(argv[1], "--smoke") == 0) {
        smoke_frames = std::atoi(argv[2]);
        if (smoke_frames <= 0) {
            std::fprintf(stderr, "bad --smoke frame count\n");
            return 2;
        }
        if (argc >= 4) shot_path = argv[3];
        if (argc >= 5) smoke_offset_aim_deg = std::atof(argv[4]);
        if (argc >= 6) smoke_aim_down_deg = std::atof(argv[5]);
    }

    sim::AircraftParams params;
    control::ControllerParams cparams;
    cfg::ScenarioParams scen;
    cfg::WorldParams world;
    try {
        params = cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
        cparams = cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml",
                                            params);
        scen =
            cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", params);
        world = cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "config: %s\n", e.what());
        return 1;
    }

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1920, 1080, "SEADS — mouse-aim instructor");
    render::init_draw();
    // MB-7c (i): the wind-audio channel — a procedural noise stream whose
    // volume/pitch track airspeed (render::wind_level, pure). Cosmetic,
    // read-only; skipped in smoke (headless CI has no audio device) and
    // whenever the device fails to open.
    AudioStream wind_stream{};
    bool wind_ok = false;
    if (smoke_frames == 0) {
        InitAudioDevice();
        if (IsAudioDeviceReady()) {
            wind_stream = LoadAudioStream(22050, 16, 1);
            PlayAudioStream(wind_stream);
            SetAudioStreamVolume(wind_stream, 0.0f);
            wind_ok = true;
        }
    }
    uint64_t wind_rng = 0x9E3779B97F4A7C15ull;  // xorshift seed (cosmetic)
    double wind_lp = 0.0;                       // leaky integrator state
    // Feed the world config into the render layer BEFORE the first frame builds
    // the planet (no bare geometry numbers in render/ — world_build_plan §1).
    render::set_planet_build_params({world.ground.relief_scale_m,
                                     world.planet.subdiv, world.planet.u_offset,
                                     world.planet.dem_blur_radius});
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
    loop.curr = app::spawn_state(params);
    loop.prev = loop.curr;
    loop.prev_up = sim::local_up(loop.curr.position);
    loop.aim.reseed(loop.curr.orientation, loop.prev_up);
    loop.grounded = true;  // the first tick is a spawn tick (SPEC §9.5)

    // The target-drone fleet (SPEC §0 S8-drone): `count` plant instances flown
    // by the dedicated bank-hold autopilot, scattered airborne over the world
    // (drone 0 ahead of the player). They advance inside step_frame in lockstep
    // with the player tick; nullptr would leave the player path bit-identical
    // (the firewall).
    app::DroneWorld dw;
    dw.dparams = scen.drone;
    dw.gparams = scen.gunsight;
    for (int i = 0; i < dw.dparams.count; ++i)
        dw.drones.push_back(
            drone::spawn_drone(params, dw.dparams, i, dw.dparams.count));

    bool raw_mode = false;     // start in the instructor
    double pending_dx = 0.0;   // mouse delta accrued across frames until a tick
    double pending_dy = 0.0;   //   consumes it (no loss when render fps > sim)
    bool refocus_drop = true;  // drop the next live mouse delta (first frame /
                               //   first focused frame — cursor-warp spike)
    // MB-7c display state (cosmetic, render-only — off every control path):
    // the wingtip vortex trails and the smoothed speed-trend cue.
    render::VortexTrails vortices;
    double speed_trend = 0.0;
    double prev_speed_for_trend = glm::length(loop.curr.velocity);
    // render/rig-D port (plane-model-only): the last-committed commanded
    // Inputs, held across 0-tick frames (mirroring how the HUD holds its
    // SimState read) so the Fleet Rig's cosmetic control-surface deflection
    // never chases a stale zero on a fast-render/slow-sim frame. Render-only,
    // read from FrameResult::last_inputs, never written back into sim/control
    // (RA9).
    sim::Inputs player_inputs_disp{};

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
    while (!WindowShouldClose()) {
        const double frame_dt =
            smoke_frames > 0 ? params.sim_dt : GetFrameTime();
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
            raw_in = input::poll_raw(raw_dev, clamped_dt);
        } else {
            live = input::poll_live(live_dev, clamped_dt);
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
        raw_in.gear_cmd = raw_dev.gear_down ? 1.0f : 0.0f;

        app::FrameInput fin;
        fin.raw_mode = raw_mode;
        fin.raw_in = raw_in;
        fin.throttle = live.throttle;
        fin.flap_cmd = flap_frac(live_dev.flap_pos);
        fin.gear_cmd = live_dev.gear_down ? 1.0 : 0.0;
        fin.freelook_held = live.freelook_held;
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
        const app::FrameResult fr =
            app::step_frame(loop, accum, frame_dt, fin, pending_dx, pending_dy,
                            params, cparams, &dw);
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

        if (fr.respawned) {
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
            orbit_inertia.reset();  // S-globelook: nor its coast velocity —
                                    // a dead life's spin can't haunt the
                                    // reborn camera
            orient_tap.reset();     // a dead life's tap can't orient the reborn
            prev_freelook = false;
            pending_orient =
                false;     // nor an undelivered fire from the dead life
            zoom_t = 0.0;  // reborn unzoomed (re-eases up if RMB still held)
            // S-reticle: reborn on the fresh spawn aim (hygiene — the cap
            // would self-heal in one frame anyway).
            reticle_dir = loop.aim.forward();
            // MB-7c: a dead life's vortices must not hang over the fresh
            // spawn, and the trend cue restarts at the spawn speed (else the
            // respawn speed step reads as a phantom multi-g accel).
            render::vortex_reset(vortices);
            speed_trend = 0.0;
            prev_speed_for_trend = glm::length(loop.curr.velocity);
            // Same clean rebirth for the lagged camera-forward: snap it to the
            // reborn nose so the fresh spawn isn't drawn easing in from the
            // dead life's camera direction (the P3c/orbit sibling; cosmetic,
            // §9.2).
            cam_fwd = loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            cam_up = loop.aim.up();  // reborn: the aim frame reseeded to level
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
        // Ease the re-center gate across the freelook edge (P1-2): mouse-aim ->
        // 1, freelook -> 0. Same time-constant as the fov ease. render-only.
        recenter_gate =
            render::blend_toward(recenter_gate, live.freelook_held ? 0.0 : 1.0,
                                 render::kZoomEaseTime, clamped_dt);
        const double fovy =
            render::kChaseFovyDeg +
            (render::kZoomFovyDeg - render::kChaseFovyDeg) * zoom_t;

        const sim::SimState draw_state =
            render::interpolate(loop.prev, loop.curr, accum.alpha());
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
            // S-keychase: while the pilot flies on the override KEYS (and is
            // NOT in freelook, which owns the camera itself), the parked aim is
            // not where he's going — anchor the rest target on the flight path
            // instead. keys_flying false => the caller's own dials verbatim, so
            // mouse-aim flying is bit-identical.
            const bool keys_flying =
                !live.freelook_held &&
                (live.override_mask[0] || live.override_mask[1] ||
                 live.override_mask[2]);
            const render::ChaseAnchor ca = render::chase_anchor(
                keys_flying, cparams.cam_lead, cparams.cam_lag_base,
                cparams.cam_lag_gain, cparams.cam_key_anchor_rate);
            cam_fwd = render::ease_chase_forward(
                cam_fwd, vel_dir, loop.aim.forward(), ca.lead, ca.lag_base,
                ca.lag_gain, clamped_dt);
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
        const render::CameraPose pose =
            raw_mode ? render::chase_camera(draw_state, params, chase)
                     : app::instructor_camera(draw_state, render_fwd, cam_up,
                                              params, chase, orbit);
        render::FrameInfo info;
        info.fps = GetFPS();
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
        info.drones_draw.reserve(dw.drones.size());
        for (const drone::DroneState& d : dw.drones)
            info.drones_draw.push_back(
                render::interpolate(d.prev, d.curr, accum.alpha()));
        // Lead-angle gunsight (S8-drone): read the meter's snapshot (computed
        // once per sim tick in app::tick, single source) into the HUD. The
        // pipper + TOT% are pure reads — no render-time solve. Instructor mode
        // only (raw mode has no aim HUD).
        info.gunsight_active = !raw_mode;
        info.gunsight_has_target = dw.meter.has_target;
        info.gunsight_lead = dw.meter.lead_now;
        info.gunsight_on_target = dw.meter.on_target_now;
        // Engaged fleet slot (S-caret skip-by-index, P1-2): the meter's
        // engaged_index IS an index into dw.drones, populated 1:1 into
        // info.drones_draw above, so it locates the engaged bandit's draw
        // state. -1 when nothing is engaged.
        info.gunsight_target_index =
            dw.meter.has_target ? dw.meter.engaged_index : -1;
        info.tot_frac = dw.meter.fraction();
        info.tot_range = dw.meter.range_now;
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
        {
            const render::FlightReadout rd =
                render::flight_readout(draw_state, params);
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
            // Wind audio ∝ airspeed, hushed by thin air (MB-7c i): volume +
            // pitch mapped by the pure render::wind_level; the noise stream
            // below is fed per frame. Cosmetic, read-only.
            if (wind_ok) {
                const render::WindLevel wl = render::wind_level(
                    rd.speed, sim::atm_frac(rd.altitude, params));
                SetAudioStreamVolume(wind_stream,
                                     static_cast<float>(wl.volume));
                SetAudioStreamPitch(wind_stream, static_cast<float>(wl.pitch));
                while (IsAudioStreamProcessed(wind_stream)) {
                    short buf[1024];
                    for (int i = 0; i < 1024; ++i) {
                        // Leaky-integrated white noise (brown-ish) reads as
                        // wind rush; xorshift PRNG (cosmetic, no <random>).
                        wind_rng ^= wind_rng << 13;
                        wind_rng ^= wind_rng >> 7;
                        wind_rng ^= wind_rng << 17;
                        const double white =
                            static_cast<double>(
                                static_cast<int32_t>(wind_rng)) /
                            2147483648.0;
                        wind_lp += 0.06 * (white - wind_lp);
                        buf[i] = static_cast<short>(wind_lp * 22000.0);
                    }
                    UpdateAudioStream(wind_stream, buf, 1024);
                }
            }
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
                           chase.height, chase.distance,
                           fovy * (3.14159265358979323846 / 180.0));
        render::draw_frame(draw_state, params, pose, info);

        if (smoke_frames > 0 && ++frames >= smoke_frames) {
            if (shot_path != nullptr) TakeScreenshot(shot_path);
            break;
        }
    }
    if (wind_ok) {
        UnloadAudioStream(wind_stream);
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
