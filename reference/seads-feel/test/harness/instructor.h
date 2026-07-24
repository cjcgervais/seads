#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "control/controller.h"
#include "control/transport.h"
#include "input/aim_state.h"
#include "render/camera.h"
#include "sim/aero.h"  // sim::rho_at (MB-atm altitude-aware trim)
#include "sim/params.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/injector.h"

// Closed-loop instructor driver (HARNESS §4): the aim carry + control::step()
// + sim::step() wired at fixed sim_dt over the REAL spherical plant — never a
// flat stub (HARNESS §3: a flat plant misgrades on-sphere corrective
// rotation). Shared by the controller goldens, the cascade tests, and the
// harness recorder so a re-record can never drift from what the tests replay.
//
// The aim is a persistent world-frame unit vector, parallel-transported every
// tick (SPEC §9.1) by the quaternion taking local_up(t-dt) to local_up(t) —
// exactly what keeps a "level" aim level on R = 15 km. Scripted maneuvers set
// the aim to a new world direction; between commands it is only transported.

namespace harness {

struct ClosedLoop {
    sim::SimState state{};
    control::Internal internal{};
    sim::Inputs last_inputs{};       // the Inputs control::step just emitted
    glm::dvec3 aim{0.0, 0.0, -1.0};  // world-frame targetDir, persistent
    glm::dvec3 prev_up{0.0, 0.0, 1.0};
    // Held keyboard override (SPEC §9.5), indexed [pitch, yaw, roll] to match
    // control::Input. Persist across ticks like a held key; a scenario sets and
    // clears them between ticks. Off by default -> pure instructor flight.
    bool ovr_mask[3] = {false, false, false};
    double ovr_sign[3] = {0.0, 0.0, 0.0};

    // Freelook (SPEC §9.5, Section 4d): the caller-side aim state machine, run
    // through the SHARED input::Freelook so the harness exercises the same code
    // the app will. `held` persists across ticks like the Space key; `fl` owns
    // the latches; `mouse_aim_live` mirrors the last tick's CQ2 gate for AT-8.
    // Off by default -> the aim path is byte-identical (strict superset).
    // SEAM (S7-hrz, diff red-team P2-2): ClosedLoop carries a bare vec3 aim,
    // so the app-only horizon-recovery roll is UNREPRESENTABLE here — the
    // mirror-equivalence pin therefore CANNOT cover it (golden/AT scenarios
    // must stay recovery-free, which they are: no misaligned release exists in
    // any of them). Its trajectory-neutrality is pinned by the dedicated
    // rate-on-vs-off leg in test_instructor_tick.cpp instead.
    // SEAM (S7-nest): the app's freelook ACTIONS have also diverged from this
    // harness's — the shipped tick nests aim:=nose per tick under
    // freelook+override and snaps the RELEASE to the guarded VELOCITY (SPEC
    // §9.5 as amended, §0 S7-nest); ClosedLoop keeps the 4d-era one-shot
    // nose snaps, which is fine for what it grades (AT-8's release-catch
    // transient — latch semantics, shared via input::Freelook, unchanged).
    // App-side freelook behavior is pinned in test_instructor_tick.cpp only.
    bool freelook_held = false;
    input::Freelook fl{};
    bool mouse_aim_live = true;
    // Aim-motion gate (SPEC §9.3 as amended): scenarios set this like a held
    // key to model "the hand is moving the aim this tick". Default false =>
    // every existing scenario/golden flies the legacy latch bit-identically.
    bool aim_moved = false;
    // S-aimff (v4 rung 1): the scripted mouse-induced aim angular velocity
    // [rad/s, WORLD] a scenario reports alongside its aim motion (the track
    // instrument sets it while sweeping; scripted SET-jumps supply none).
    // Forwarded into control::Input under the SAME CQ2 gate as aim_moved —
    // the moved-consumer discipline: one new field, one forwarding leg, one
    // test leg. Default 0 => every existing scenario/golden feeds the
    // feedforward nothing (structurally inert behind the gain gate).
    glm::dvec3 aim_rate_world{0.0};

    ClosedLoop(const sim::SimState& s0, const glm::dvec3& aim0)
        : state(s0),
          aim(glm::normalize(aim0)),
          prev_up(sim::local_up(s0.position)) {}

    // Aim the nose: set the world-frame target to the current nose direction
    // (SPEC §9.5: aim := nose at spawn/reset).
    void aim_nose() { aim = state.orientation * glm::dvec3{0.0, 0.0, -1.0}; }

    // Hold / release a single override axis (test convenience).
    void hold_override(int axis, double sign) {
        ovr_mask[axis] = true;
        ovr_sign[axis] = sign;
    }
    void release_override() {
        for (int i = 0; i < 3; ++i) {
            ovr_mask[i] = false;
            ovr_sign[i] = 0.0;
        }
    }

    // Hold / release freelook (the Space key). While held the aim is only
    // transported and the mouse would go to the camera (SPEC §9.5).
    void hold_freelook() { freelook_held = true; }
    void release_freelook() { freelook_held = false; }

    control::Telemetry tick(double throttle, const sim::AircraftParams& ap,
                            const control::ControllerParams& cp,
                            bool grounded = false) {
        const glm::dvec3 up = sim::local_up(state.position);
        aim = control::transport_aim(aim, prev_up, up);  // parallel transport
        prev_up = up;

        // GROUNDED (spawn/reset) is where the caller pairs the in-core
        // control::reset() with the caller-side aim reset (SPEC §9.5,
        // aim_state.h::reset): drop every freelook latch and re-aim to the
        // nose, or a respawn mid-freelook inherits the dead life's hold/
        // override_used and the aim carries a stale target. The app loop must
        // do the IDENTICAL pairing when it wires the instructor — modelling it
        // here (the caller-of-record) is what keeps the two callers from
        // drifting (the input::Freelook shared-owner discipline, CLAUDE.md 4d).
        if (grounded) {
            fl.reset();
            aim_nose();
        }

        // Freelook aim state machine (SPEC §9.5, shared input::Freelook). Runs
        // every tick, in every mode; a no-op on the aim unless freelook is or
        // was engaged. snap_to_nose overwrites the (harmlessly transported) aim
        // with the current nose; mouse_aim_live is the CQ2 gate AT-8 reads.
        const bool any_ovr = ovr_mask[0] || ovr_mask[1] || ovr_mask[2];
        const input::Freelook::Step fs = fl.step(
            freelook_held, any_ovr, ap.sim_dt, cp.freelook_easeback_time);
        if (fs.snap_to_nose) {
            aim = state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        }
        mouse_aim_live = fs.mouse_aim_live;

        // Keyboard override does NOT touch the aim (SPEC §9.5, S7-ovr2): mirror
        // of app::tick — the keyboard is a pure supplementary in-envelope
        // nudge; the aim stays mouse-owned so a keypress never swings the
        // camera.

        control::Input in;
        in.target_dir_world = aim;
        in.throttle = throttle;
        in.grounded = grounded;
        // Motion counts only when the mouse could actually feed the aim —
        // mirror of app::tick's CQ2-gated site.
        in.aim_moved = aim_moved && mouse_aim_live && !grounded;
        // S-aimff: the scripted aim rate rides the SAME gate (freelook /
        // grounded ticks can never report a moving hand to the feedforward).
        in.aim_rate_world =
            (mouse_aim_live && !grounded) ? aim_rate_world : glm::dvec3{0.0};
        for (int i = 0; i < 3; ++i) {
            in.override_mask[i] = ovr_mask[i];
            in.override_sign[i] = ovr_sign[i];
        }

        const control::Output o =
            control::step(state, in, internal, ap, cp, ap.sim_dt);
        internal = o.internal;
        last_inputs = o.inputs;
        state = sim::step(state, o.inputs, ap, ap.sim_dt);
        return o.telem;
    }
};

// Mini-camera: the SHIPPED camera-basis advance main.cpp runs, so the
// mouse-driven loop/split-S legs carry the shipped (vestigial-to-the-raw-mouse)
// cam_fwd/cam_up into app::tick. cam_fwd eases toward the aim via the
// SINGLE-SOURCE render::ease_chase_forward (not forked); cam_up is the CARRIED
// aim-frame up (S7-cam3 — main.cpp's `cam_up = loop.aim.up()`), passed in. Seed
// on the nose/local_up, then each tick feed the PREVIOUS values into app::tick
// (the one-tick lag main.cpp has), then advance from the new state + aim. Zero-
// mouse is a no-op so this never perturbs the trajectory (RA9 holds).
struct MiniCamera {
    glm::dvec3 cam_fwd{0.0, 0.0, -1.0};
    glm::dvec3 cam_up{0.0, 0.0, 1.0};

    void seed(const sim::SimState& s) {
        cam_fwd = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        cam_up = sim::local_up(s.position);
    }
    void advance(const sim::SimState& s, const glm::dvec3& aim_fwd,
                 const glm::dvec3& aim_up, const control::ControllerParams& cp,
                 double dt) {
        const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const double sp = glm::length(s.velocity);
        const glm::dvec3 vel_dir = sp > 1.0 ? s.velocity / sp : nose;
        cam_fwd =
            render::ease_chase_forward(cam_fwd, vel_dir, aim_fwd, cp.cam_lead,
                                       cp.cam_lag_base, cp.cam_lag_gain, dt);
        cam_up = aim_up;  // carried aim-up (S7-cam3), mirrors main.cpp
    }

    // S-orient (docs/comfort program Q3): the ORIENT verb's camera cut —
    // HARD-SEAT cam_fwd to the aim (research REC-1: angular snap, not a slew).
    // Mirrors main.cpp's `if (fr.orient_fired) cam_fwd = loop.aim.forward();`
    // EXACTLY, so the comfort instrument measures the law the app ships. Called
    // by the comfort driver AFTER advance() on the tick app::tick reported
    // orient_fired.
    void orient_cut(const glm::dvec3& aim_fwd) { cam_fwd = aim_fwd; }
};

// Approximately trimmed level flight (HARNESS): velocity horizontal along
// `heading`, the orientation pitched up by the AoA that trims lift for level
// flight on the sphere — Cl = m(g - V^2/R)/(qS), alpha = Cl/Cl_alpha. Built
// from level_state (raw rotations, never the extract formulas) so the fixture
// can't share a sign bug with the code under test.
inline sim::SimState level_trim_state(const sim::AircraftParams& p, double V,
                                      double altitude, const glm::dvec3& up_dir,
                                      const glm::dvec3& heading,
                                      double* trim_throttle = nullptr) {
    sim::SimState s = level_state(p, V, altitude, up_dir, heading);
    // MB-atm: trim at the FIXTURE's altitude density (a sea-level q mistrims
    // any fixture above the taper — the "flat instrument" class, altitude
    // edition). Bit-identical below taper_alt (rho_at == rho), so goldens
    // and every sub-taper fixture are untouched.
    const double q = 0.5 * sim::rho_at(altitude, p) * V * V;
    const double Cl_trim = p.mass * (p.g - V * V / p.R) / (q * p.S);
    const double alpha = Cl_trim / p.Cl_alpha;
    const glm::dvec3 right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
    // Pitch the ORIENTATION up by alpha about body_right (velocity stays
    // horizontal) -> AoA = alpha, nose alpha above the horizontal velocity.
    s.orientation =
        glm::normalize(glm::angleAxis(alpha, right) * s.orientation);
    if (trim_throttle != nullptr) {
        const double Cd = p.Cd0 + p.k_induced * Cl_trim * Cl_trim;
        const double drag = q * p.S * Cd;
        *trim_throttle = drag / p.T_max;
    }
    return s;
}

// ---- The controller golden (HARNESS §3 T2) --------------------------------
// ONE fixed start + ONE fixed scripted aim sequence, closed-loop through the
// REAL sim::step(): trim, a 25 deg bank-to-turn, a 20 deg pull, back to level.
// Shared by the golden test (compare) and the harness recorder (record) so a
// re-record can never drift from what the test replays. Changing ANYTHING here
// moves the golden — a HALT until the move is confirmed intentional. Recorded
// only AFTER AT-0 + the 4b cascade land (HARNESS §5: a golden of wrong code
// freezes the bug in).

constexpr int kCtrlGoldenTicks = 1200;  // 10 s at 1/120
constexpr int kCtrlGoldenCheckpointEvery = 300;

inline sim::SimState ctrl_golden_start(const sim::AircraftParams& p,
                                       double* throttle) {
    return level_trim_state(p, 140.0, 3000.0, glm::dvec3{1.0, 0.0, 0.0},
                            glm::dvec3{0.0, 0.0, -1.0}, throttle);
}

// Advance one golden tick. At scripted instants re-aim RELATIVE to the current
// nose (deterministic: same start + same script -> same flight -> same aim).
inline control::Telemetry ctrl_golden_step(
    ClosedLoop& cl, int tick, double throttle, const sim::AircraftParams& ap,
    const control::ControllerParams& cp) {
    constexpr double kPi = 3.14159265358979323846;
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 up_b = cl.state.orientation * glm::dvec3{0.0, 1.0, 0.0};
    if (tick == 240) {  // 25 deg RIGHT: bank-to-turn (MANEUVER)
        cl.aim =
            glm::normalize(glm::angleAxis(-25.0 * kPi / 180.0, up_b) * nose);
    } else if (tick == 600) {  // 20 deg UP: a pull (G/AoA path)
        cl.aim =
            glm::normalize(glm::angleAxis(20.0 * kPi / 180.0, right) * nose);
    } else if (tick == 960) {  // back to level
        cl.aim_nose();
    }
    return cl.tick(throttle, ap, cp);
}

// ---- AT-12: sustained max-rate turn (HARNESS §5; SPEC §16 "predictable
// energy") -----------------------------------------------------------------
// A level break turn: each tick the aim is a horizontal-plane direction
// `kAt12TurnOffset` to the side of the current horizontal heading, so the
// instructor banks and pulls at the G/AoA limit while trying to hold altitude
// (throttle is passthrough — the instructor never manages energy, SPEC §9.8).
// The pointing error stays saturated, so the maneuver is a SUSTAINED max-rate
// turn, not a capture-then-coast. Shared by the AT-12 acceptance leg (energy
// gate + retention printout) and the `ctrl_fly` CSV recorder so the instrument
// and the gate describe the SAME flight — the "one driver, no drift" rule.

constexpr int kAt12Ticks = 1200;             // 10 s at 1/120
constexpr double kAt12TurnOffsetDeg = 70.0;  // lateral aim offset -> max-rate
constexpr double kAt12Throttle = 1.0;        // full throttle (energy retention)
constexpr double kAt12StartSpeed = 150.0;    // [m/s]
constexpr double kAt12StartAlt = 6000.0;     // [m] — headroom for a hard turn

inline sim::SimState at12_start(const sim::AircraftParams& p) {
    return level_trim_state(p, kAt12StartSpeed, kAt12StartAlt,
                            glm::dvec3{1.0, 0.0, 0.0},
                            glm::dvec3{0.0, 0.0, -1.0});
}

// Re-aim to a fixed horizontal-plane offset from the current heading — call
// BEFORE each in-turn tick. The target is recomputed from local_up every tick
// (SPEC §6.1: never a cached axis), so it stays level as the plane turns.
inline void at12_reaim(ClosedLoop& cl) {
    constexpr double kPi = 3.14159265358979323846;
    const glm::dvec3 up = sim::local_up(cl.state.position);
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    glm::dvec3 h = nose - glm::dot(nose, up) * up;  // heading in the horizon
    if (glm::length(h) < 1e-6)                      // nose ~ straight up/down
        h = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    h = glm::normalize(h);
    const double offset = kAt12TurnOffsetDeg * kPi / 180.0;
    cl.aim = glm::normalize(glm::angleAxis(-offset, up) * h);
}

}  // namespace harness
