#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "app/loop.h"
#include "control/controller.h"
#include "drone/drone.h"
#include "input/aim_frame.h"
#include "input/aim_state.h"
#include "render/camera.h"
#include "render/gunsight.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"

// The app's fixed-dt per-tick core, extracted so the SHIPPED tick IS the tested
// tick (CLAUDE.md: "route the live path THROUGH the tested function"; the P1b
// audit finding, pre_spec_audits/fable5_section6_fixreport.md). Before this the
// crash predicate (altitude <= 0 -> respawn), the GROUNDED pairing, and the
// frame-carried camera call site executed under NO test — a golden pinned only
// the code that recorded it, everywhere except the app loop.
//
// The tick is MODE-COMMON, not "the instructor branch": transport, the GROUNDED
// pairing, and the crash-reset fire in RAW mode too (they sit outside
// main.cpp's `if (raw_mode)`). Extracting them once, driven by a `raw_mode`
// flag, is what keeps the crash predicate from forking into an untested
// raw-side copy (the "grep every branch that does the gated thing" /
// "two-copies seam" lessons).
//
// PURE / raylib-free (it may include control/, input/, render/ but never a
// device or a clock): the caller (app/main.cpp) owns the accumulator, device
// polling, the per-FRAME mouse accrual + freelook-orbit camera, and focus-loss;
// this owns the per-TICK plant/instructor advance. The harness ClosedLoop::tick
// (test/harness/instructor.h) is the OTHER caller of the same shared pieces
// (extract/transport/Freelook/control::step); a mirror-equivalence test pins
// the two against drift (test_instructor_tick.cpp).

namespace app {

// S-aimff (v4 rung 1): the axis*angle rotation vector of a unit quaternion
// [rad, same frame as the quaternion]. Shortest arc (w >= 0 branch), total —
// an identity/near-identity delta returns exactly 0 (no normalize(0) NaN).
// Used to turn the apply_mouse quaternion delta into the aim's angular
// velocity for the aim-rate feedforward. Red-team P2-1: the shortest-arc
// fold means a per-frame rotation > pi (~1287 px/frame at 0.14 deg/px)
// reads as its < pi complement — ONE frame of anti-lead on an absurd
// flick, bounded by the ceilings/clamps downstream — accepted.
inline glm::dvec3 quat_rotation_vec(const glm::dquat& dq) {
    const glm::dquat q = dq.w < 0.0 ? -dq : dq;  // shortest arc
    const glm::dvec3 v{q.x, q.y, q.z};
    const double s = glm::length(v);
    if (s < 1e-12) return glm::dvec3{0.0};
    return (2.0 * std::atan2(s, q.w) / s) * v;
}

// Spawn / crash-respawn state (SPEC §6.3: respawn airborne AT speed, born at
// cruise power — not a 0.5 s spool from idle). Shared by main.cpp and the
// tick's crash branch so the shipped respawn data IS what the test asserts
// against. Level at 2 km over the +X pole, 140 m/s along the nose.
inline sim::SimState spawn_state(const sim::AircraftParams& p) {
    sim::SimState s;
    s.position = {p.R + 2000.0, 0.0, 0.0};
    const glm::dmat3 m{glm::dvec3{0.0, -1.0, 0.0},  // body X -> world -Y
                       glm::dvec3{1.0, 0.0, 0.0},   // body Y (up) -> world +X
                       glm::dvec3{0.0, 0.0, 1.0}};  // body Z -> world +Z
    s.orientation = glm::normalize(glm::quat_cast(m));
    s.velocity = 140.0 * glm::dvec3{0.0, 0.0, -1.0};
    s.last_vhat = {0.0, 0.0, -1.0};
    s.throttle = 1.0;
    return s;
}

// The per-tick mutable set the accumulator loop carries across ticks (main.cpp
// held these as loose locals). main.cpp legitimately writes `grounded` (the F1
// mode toggle) and `aim`/`fl` (focus loss) from OUTSIDE a tick — this is loop
// state, not a sealed invariant.
struct LoopState {
    sim::SimState prev{};
    sim::SimState curr{};
    input::AimFrame aim{};
    input::Freelook fl{};
    input::HorizonRecovery recov{};  // S7-hrz open-loop roll latch
    control::Internal internal{};
    glm::dvec3 prev_up{0.0, 0.0, 1.0};
    bool grounded = true;  // the first tick is a spawn tick (SPEC §9.5)
};

// Per-tick inputs the caller resolves for THIS tick (rebuilt every tick from
// the frame-scope device sample, so a respawn mid-frame cannot leak the
// pre-crash stick/freelook into the fresh airframe). `aim_dx/aim_dy` are the
// per-FRAME mouse delta the caller OFFERS on the one consuming tick (0
// otherwise); the tick applies it to the aim iff the CQ2 gate allows
// (mouse_aim_live && !grounded), matching main.cpp — so the offer is harmless
// when freelook routes the same delta to the orbit (Freelook::step returns
// mouse_aim_live=false while held). Aim sensitivity is read from `cp` (single
// source), never carried here.
struct TickInput {
    bool raw_mode = false;
    sim::Inputs raw_in{};  // used iff raw_mode
    double throttle = 0.0;
    // MB-flaps: device passthrough alongside throttle (instructor mode; raw
    // mode carries them inside raw_in). Defaulted 0 = clean airframe, so
    // every pre-flap caller (goldens/mirror/tests) is bit-identical.
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    bool freelook_held = false;
    // S-orient (docs/comfort program Q3): the caller sets this TRUE on the tick
    // its input::OrientTap detector fired (a freelook double-tap within the
    // window). Defaulted false => bit-identical (the mirror-equivalence /
    // firewall path never sets it), a strict superset. The tick composes the
    // ORIENT verb: aim := guarded velocity, S7-hrz up-debt capture, and a
    // reported camera-forward cut (res.orient_fired) the caller hard-seats.
    bool orient_cmd = false;
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    double aim_dx =
        0.0;  // per-frame mouse delta, offered on the consuming tick
    double aim_dy = 0.0;
    // RMB-zoom mouse-gain scale (2026-07-07): multiplies aim_sensitivity for
    // THIS tick's apply_mouse so a zoomed (magnified) view aims proportionally
    // slower — precision on the target. Defaulted 1.0 => bit-identical to the
    // pre-zoom tick (the harness/goldens/mirror never set it), a strict
    // superset. The caller sets it BINARY (zoom held ? kZoom/kChase : 1.0), a
    // pure function of the button state — NOT the eased fov — so it stays
    // frame-rate independent (an fov-eased gain would be the frame-quantized
    // signal driving the control-relevant aim that diverged S7-mouselevel /
    // AT-9). A scalar magnitude on the raw delta, not a basis smoothing (RA9).
    double aim_gain_scale = 1.0;
    // S-aimff (v4 rung 1): the frame's mouse-induced aim angular velocity
    // [rad/s, world], FORWARDED by step_frame to ticks 2..N of a multi-tick
    // frame (the smear/ZOH — mouse deltas arrive per FRAME and land on the
    // consuming tick; a naive rotation/sim_dt there is a frame-rate-dependent
    // spike train, the AT-9 / S7-mouselevel ban). On the CONSUMING tick
    // app::tick IGNORES this field and computes the rate itself from the
    // rotation apply_mouse actually applied, spread over frame_ticks:
    // rate = rotation/(frame_ticks*sim_dt), so the FF demand INTEGRAL equals
    // K_ff*(frame rotation) at any frame rate. Defaults (0, 1) => every
    // direct caller is per-tick exact and bit-identical while the mouse is
    // still.
    glm::dvec3 aim_rate_ff{0.0};
    int frame_ticks = 1;  // N of the frame this tick belongs to (step_frame)
    // Vestigial (S7-raw removed the F1 screen-relative mouse): the mouse now
    // rotates the RAW carried aim frame (§9.1), so app::tick no longer reads
    // cam_fwd/cam_up. Retained only so the caller's FrameInput->TickInput
    // forwarding stays uniform; the camera-render path uses main.cpp's OWN
    // cam_fwd/cam_up, not these. Safe to remove with the 5-arg apply_mouse.
    glm::dvec3 cam_fwd{0.0, 0.0, 0.0};
    glm::dvec3 cam_up{0.0, 0.0, 0.0};
};

struct TickResult {
    control::Telemetry telem{};  // instructor mode only (default in raw)
    sim::Inputs inputs{};        // the emitted Inputs (mirror of last_inputs)
    bool respawned = false;      // the crash predicate fired THIS tick
    // S-orient (docs/comfort program Q3): the ORIENT verb fired THIS tick — the
    // CALLER re-seats its lagged cam_fwd := st.aim.forward() instantly (a hard
    // angular cut; position/FOV keep their smoothing — research REC-1). Wired
    // in app/main.cpp AND harness::MiniCamera so the instrument measures the
    // law the app ships. Default false => no cut (bit-identical firewall path).
    bool orient_fired = false;
    // S-aimff: the aim rate control::step SAW this tick (== the value fed to
    // control::Input::aim_rate_world). The consuming tick reports its
    // computed rate here so step_frame can forward THE SAME value into ticks
    // 2..N of the frame (the smear); also the moved-consumer test's probe.
    glm::dvec3 aim_rate_ff{0.0};
};

// The optional per-tick drone bundle threaded through app::tick / step_frame by
// pointer, DEFAULTED to nullptr (SPEC §0 S8-drone). nullptr => the drone path
// is skipped and the tick is BIT-IDENTICAL to the pre-drone tick — the
// firewall that keeps the player's flight-model / controller golden unmoved,
// pinned by the existing mirror-equivalence test (test_instructor_tick.cpp,
// which calls tick/step_frame with no dw). The drone advances once per PLAYER
// tick (frame-rate independent, in lockstep — the single-accumulator interleave
// that keeps the future TOT meter frame-rate independent, avoiding the
// two-copies-seam trap).
struct DroneWorld {
    std::vector<drone::DroneState> drones{};
    drone::DroneParams dparams{};
    render::GunsightParams gparams{};
    render::OnTargetMeter
        meter{};  // the player's lead/time-on-target instrument
};

// The engaged-target selection (extracted so the HYSTERESIS is pinnable
// open-loop — the S6 "drive the latch on an oscillating fixed state"
// discipline; Fable P1-2). Returns the fleet slot of the engaged bandit, or -1.
// A bandit qualifies if within the forward cone AND track_range; the
// currently-latched bandit `prev` qualifies in a WIDER cone (+0.08 cos)
// + 1.1*range and KEEPS engagement unless a challenger is materially closer (<
// 0.85*its range) — so the pipper does not strobe between crisscrossing
// bandits. PURE.
inline int select_engaged_target(int prev,
                                 const std::vector<drone::DroneState>& drones,
                                 const glm::dvec3& shooter_pos,
                                 const glm::dvec3& nose,
                                 const render::GunsightParams& gp) {
    int best = -1;
    double best_r = 1e300;
    bool prev_ok = false;
    double prev_r = 1e300;
    for (std::size_t i = 0; i < drones.size(); ++i) {
        const glm::dvec3 to = drones[i].curr.position - shooter_pos;
        const double r = glm::length(to);
        if (r < 1e-6) continue;
        const bool latched = static_cast<int>(i) == prev;
        const double cone =
            latched ? gp.track_cone_cos - 0.08 : gp.track_cone_cos;
        const double range_lim =
            latched ? gp.track_range * 1.1 : gp.track_range;
        if (glm::dot(to / r, nose) <= cone || r > range_lim) continue;
        if (latched) {
            prev_ok = true;
            prev_r = r;
        }
        if (r < best_r) {
            best_r = r;
            best = static_cast<int>(i);
        }
    }
    if (prev_ok && (best < 0 || best_r >= 0.85 * prev_r)) return prev;
    return best;
}

// One fixed-dt tick (SPEC §9.5 sequence), both modes. Mutates `st`. Mirrors
// harness::ClosedLoop::tick tick-for-tick (transport -> GROUNDED pairing ->
// [raw: sim::step] | [instructor: freelook -> mouse-aim -> control::step ->
// sim::step] -> crash predicate -> respawn), the SAME shared pieces, so the two
// callers can't drift (pinned by the mirror-equivalence test).
inline TickResult tick(LoopState& st, const TickInput& in,
                       const sim::AircraftParams& ap,
                       const control::ControllerParams& cp,
                       DroneWorld* dw = nullptr) {
    TickResult res;

    st.prev = st.curr;
    const glm::dvec3 up = sim::local_up(st.curr.position);
    st.aim.transport(st.prev_up, up);  // every tick, every mode (SPEC §9.1)
    st.prev_up = up;
    // The mouse aim is PURE RAW (§9.1, S7-raw): transport + apply_mouse on the
    // frame's OWN carried up/right, nothing camera-referenced, nothing eased.
    // (An S7-mouselevel "gentle roll-to-local_up" was tried to fix the mouse
    // feeling inverted after a half-loop, but ANY easing of the mouse basis
    // curls an active sweep and is a §9.1-banned smoothing on the mouse->aim
    // path — Chad's standing rule "no auto-leveling, chase the raw aim".
    // REMOVED. The post-maneuver "up-is-down" WAS the accepted trade of the
    // raw frame; S7-hrz below now rights it — but only as an OPEN-LOOP
    // fixed-angle roll on a freelook release (a player action, never
    // autonomous), which is why it is not the S7-mouselevel dead-end:
    // docs/horizon_recovery_plan.md.)

    // GROUNDED pairing (SPEC §9.5): the in-core reset paired with the
    // caller-side aim/freelook reset, exactly as ClosedLoop does.
    if (st.grounded) {
        st.fl.reset();
        st.recov.reset();  // a respawn never inherits a dead life's roll
        st.aim.reseed(st.curr.orientation, up);
        st.internal = control::reset();
    }

    if (in.raw_mode) {
        res.inputs = in.raw_in;
        st.curr = sim::step(st.curr, in.raw_in, ap, ap.sim_dt);
    } else {
        const bool any_ovr =
            in.override_mask[0] || in.override_mask[1] || in.override_mask[2];
        const input::Freelook::Step fs = st.fl.step(
            in.freelook_held, any_ovr, ap.sim_dt, cp.freelook_easeback_time);
        // §5b aim NESTING + the D8 release target (S7-nest, SPEC §9.5
        // amendment — docs/horizon_recovery_plan.md D7/D8):
        //  - While freelook AND any override key are held, the aim RIDES THE
        //    NOSE per tick ("nested with the nose dot") — the per-tick
        //    strengthening of rule 2's one-shot snap, which this branch
        //    subsumes (rule 2 fires only when freelook && any_override).
        //    The mouse never feeds the aim during freelook, so nothing here
        //    touches the raw mouse->aim path.
        //  - The rule-3 RELEASE snap (override used during the hold) now
        //    lands on the GUARDED VELOCITY, not the nose: the aim sits on
        //    the flight path, the S7-hrz recovery rolls about that same
        //    axis, and the chase camera's rest is behind velocity — one
        //    composed settle. The guard is the CALLER'S OWN (red-team F5 —
        //    this feeds ci.target_dir_world, a CONTROL input, so the sim's
        //    held last_vhat must not be read): below v_ballistic or
        //    tail-slide (vhat . nose <= 0) fall back to the nose.
        //  - Freelook WITHOUT keys: with release_orient OFF, untouched — the
        //    held-turn aim stays carried, release moves nothing (the 4d
        //    conditional reset). With release_orient ON (S-relorient, Chad
        //    2026-07-28), EVERY airborne no-override release drives THIS same
        //    guarded-velocity snap and reports the orient camera cut — the
        //    flown double-tap verb, automatic. It fires HERE (not in the
        //    S-orient block below) so the S7-hrz capture on fs.released
        //    measures the up-debt about the POST-snap forward on the SAME
        //    tick (plan-audit P1: the later slot captures the stale pre-snap
        //    axis and retires the wrong debt). The !grounded term is
        //    REDUNDANT DEFENSE, not load-bearing (diff red-team P2-1): the
        //    GROUNDED pairing's fl.reset() above already eats the release
        //    edge on every grounded tick, so fs.released && grounded is
        //    unreachable — the term is mutation-unkillable by construction.
        //    An override still held at release keeps legacy exactly (the D9
        //    precedent — the pilot is actively maneuvering).
        const bool release_orient = cp.freelook_release_orient &&
                                    fs.released && !st.grounded && !any_ovr;
        if (in.freelook_held && any_ovr) {
            st.aim.snap_forward_to_nose(st.curr.orientation);
        } else if (fs.snap_to_nose || release_orient) {
            const glm::dvec3 nose =
                st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            glm::dvec3 dir = nose;
            const double spd = glm::length(st.curr.velocity);
            if (spd > cp.v_ballistic) {
                const glm::dvec3 vhat = st.curr.velocity / spd;
                if (glm::dot(vhat, nose) > 0.0) dir = vhat;
            }
            st.aim.snap_forward_to_dir(dir, st.curr.orientation);
            if (release_orient) res.orient_fired = true;
        }
        // Consume the offered mouse delta into the aim iff MOUSE mode is live
        // and not on a spawn/reset tick (else drop it — never queue it into a
        // smoothed basis, RA9). transport already carried the aim this tick.
        // S-aimff (v4 rung 1): bracket apply_mouse to read back the rotation
        // it ACTUALLY applied (transport ran above, so the delta excludes it
        // by construction) — the aim's own mouse-induced angular velocity,
        // smeared over the frame's N ticks (rotation/(frame_ticks*sim_dt)):
        // the FF demand integral is K_ff*(frame rotation) at any frame rate.
        // A non-consuming tick (zero offered delta) uses the value step_frame
        // forwarded; freelook (mouse_aim_live false) and grounded ticks
        // report ZERO — the mouse fed the orbit or nothing, never the aim.
        glm::dvec3 aim_rate{0.0};
        if (fs.mouse_aim_live && !st.grounded) {
            aim_rate = in.aim_rate_ff;  // ticks 2..N: the forwarded smear
            const glm::dquat q_pre = st.aim.q;
            st.aim.apply_mouse(in.aim_dx, in.aim_dy,
                               cp.aim_sensitivity * in.aim_gain_scale);
            if (in.aim_dx != 0.0 || in.aim_dy != 0.0) {
                const int n = in.frame_ticks > 1 ? in.frame_ticks : 1;
                aim_rate = quat_rotation_vec(st.aim.q * glm::inverse(q_pre)) /
                           (n * ap.sim_dt);
            }
        }
        // Horizon recovery (S7-hrz, docs/horizon_recovery_plan.md): on the
        // freelook RELEASE edge, capture the frame's up-misalignment ONCE
        // (AFTER the rule-3 snap above, so the angle is about the released
        // forward) and roll it away OPEN-LOOP with the D3 profile — a gauge
        // move about the aim direction, invisible to control::step below.
        // Canceled (level, not edge) by a freelook re-press or any override
        // key (D9); a release WHILE a key is still held therefore never arms
        // (the 4d overlap: the pilot is still maneuvering — the next clean
        // tap recovers). rate = 0 skips ALL of this structurally, including
        // the capture — the knob-off strict-superset proof (plan F8).
        if (cp.horizon_recovery_rate > 0.0 && !st.grounded) {
            if (in.freelook_held || any_ovr) {
                st.recov.reset();
            } else {
                if (fs.released) st.recov.capture(st.aim.up_misalignment(up));
                const double d =
                    st.recov.step(ap.sim_dt, cp.horizon_recovery_rate,
                                  cp.horizon_recovery_settle);
                if (d != 0.0) st.aim.roll_about_forward(d);
            }
        }
        // S-orient (docs/comfort program Q3): the ORIENT verb — a DISCRETE,
        // player-commanded composed event that puts the pilot "back together"
        // behind the flight path with the horizon righted. Fires ONLY in
        // instructor mode, airborne, on the tick the caller's OrientTap
        // detected a freelook double-tap. The verb does TWO things here:
        //   1. aim := GUARDED VELOCITY — the S7-nest rule-3 guard verbatim
        //      (spd > v_ballistic AND vhat . nose > 0, else nose): a discrete
        //      player-commanded snap, the sanctioned shape (rules 2/3).
        //   2. res.orient_fired => the caller hard-cuts cam_fwd :=
        //      aim.forward() (the camera cut behind the flight path).
        //
        // The up-debt RETIREMENT is NOT captured here (P1 red-team fix): on
        // every reachable input the orient fires on the SECOND-tap PRESS tick,
        // where freelook_held is TRUE, so the S7-hrz block just above RESET
        // recov this tick — but the NEXT held tick's reset arm would wipe any
        // capture we set here anyway. The debt is retired by the second tap's
        // RELEASE edge through the normal S7-hrz path (fs.released ->
        // recov.capture in the block above), which fires on the release AFTER
        // this press. So the orient verb = the aim snap + the camera cut; the
        // up-debt roll rides the existing S7-hrz release capture the double-tap
        // already produces. In the D9 overlap (an override still held at that
        // release), S7-hrz's own D9 clause skips the capture — the roll is
        // skipped that once, existing semantics (noted on the fly card).
        // SUPPRESSED while any override key is held: the §5b nested aim := nose
        // above would fight this velocity snap the same tick (the D9 precedent
        // — the pilot is still actively maneuvering; the next clean double-tap
        // orients). orient_double_tap_s = 0 means the caller's OrientTap never
        // fires, so orient_cmd stays false — the structural off-switch.
        if (in.orient_cmd && !st.grounded && !any_ovr) {
            const glm::dvec3 nose =
                st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            glm::dvec3 dir = nose;
            const double spd = glm::length(st.curr.velocity);
            if (spd > cp.v_ballistic) {
                const glm::dvec3 vhat = st.curr.velocity / spd;
                if (glm::dot(vhat, nose) > 0.0) dir = vhat;
            }
            st.aim.snap_forward_to_dir(dir, st.curr.orientation);
            res.orient_fired = true;
        }
        // RAW mouse basis (§9.1, S7-raw 2026-07-06 — REVERSES the F1 screen-
        // relative coupling). apply_mouse rotates the aim about the aim frame's
        // OWN carried up/right (transport + apply_mouse) — nothing camera-
        // referenced. The F1 experiment oriented the mouse delta by the eased/
        // lagged RENDER camera basis to keep mouse-up == screen-up; but that is
        // a SMOOTHED basis feeding mouse->aim (the RA9 ban), and near the
        // zenith the camera's screen-right rotates/degenerates under the input,
        // so a SLOW vertical mouse STALLED at ~vertical and could not loop over
        // (the mouse stopped being raw). Chad's ruling: make it RAW — the
        // carried §9.1 frame references nothing external, so it never poles and
        // a steady vertical mouse loops over the top continuously. TRADE-OFF (a
        // SEPARATE dial): the carried frame's UP drifts off local_up after a
        // big maneuver (holonomy + the maneuver's own rotation), so mouse
        // left/right can feel rotated — a gentle roll-to-local_up correction
        // (roll-only about forward, never poles) addresses THAT without
        // re-coupling the mouse to the camera. in.cam_fwd/cam_up are no longer
        // read here (retained on TickInput for the camera-render path the
        // caller drives; vestigial to the mouse). With zero mouse apply_mouse
        // is a strict no-op, so RA9 holds and mirror-equivalence stays
        // bit-identical. Keyboard override does NOT touch the aim (SPEC §9.5,
        // S7-ovr2): the MOUSE owns the reticle/camera, the keyboard is a pure
        // supplementary in-envelope nudge (the in-core override drives the
        // airframe rate, pursuit suspended). Earlier "aim rides the nose"
        // collapsed the aim onto the nose on the keypress, which swung the
        // (aim-leaning) camera back behind the plane — the exact "keypress
        // snaps the view" the pilot hit. On release, pursuit resumes toward the
        // held mouse aim (the reticle never moved), so the instructor simply
        // flies back to where you were already pointing.

        control::Input ci;
        ci.target_dir_world = st.aim.forward();
        ci.throttle = in.throttle;
        ci.flap_cmd = in.flap_cmd;  // MB-flaps passthrough (throttle pattern)
        ci.gear_cmd = in.gear_cmd;
        ci.grounded = st.grounded;
        // Aim-motion gate (SPEC §9.3 as amended): TRUE iff apply_mouse above
        // actually rotated the aim this tick — the same CQ2 gate, so freelook
        // (mouse -> camera) and grounded ticks can never count as motion.
        ci.aim_moved = fs.mouse_aim_live && !st.grounded &&
                       (in.aim_dx != 0.0 || in.aim_dy != 0.0);
        // S-aimff: the smeared aim rate (0 unless the mouse actually fed the
        // aim this frame — the same CQ2 gate as aim_moved, one seam).
        ci.aim_rate_world = aim_rate;
        res.aim_rate_ff = aim_rate;
        for (int i = 0; i < 3; ++i) {
            ci.override_mask[i] = in.override_mask[i];
            ci.override_sign[i] = in.override_sign[i];
        }
        const control::Output o =
            control::step(st.curr, ci, st.internal, ap, cp, ap.sim_dt);
        st.internal = o.internal;
        res.telem = o.telem;
        res.inputs = o.inputs;
        st.curr = sim::step(st.curr, o.inputs, ap, ap.sim_dt);
    }

    st.grounded = false;

    // Crash -> reset (SPEC §6.3): respawn airborne; the NEXT tick is GROUNDED,
    // which pairs control::reset() with aim := nose + fl.reset() (AT-13 clean
    // rebirth). prev := curr so render::interpolate never streaks the dead
    // life's position to the spawn across one frame; prev_up reseeded so the
    // next transport can't hit transport_rotation's antiparallel assert.
    if (sim::altitude(st.curr.position, ap) <= 0.0) {
        st.curr = spawn_state(ap);
        st.prev = st.curr;
        st.prev_up = sim::local_up(st.curr.position);
        // Reseed the aim frame HERE, not only on the next GROUNDED tick (P3c,
        // pre_spec_audits/fable5_section6_fixreport.md): if the crash lands on
        // the frame's LAST tick, the render frame between this return and the
        // next tick would draw the fresh spawn position through the DEAD life's
        // aim frame (an astern camera pop for one frame). Idempotent with the
        // GROUNDED pairing's reseed above — same (orientation, local_up) seed,
        // since the next tick's transport(prev_up==up) is identity — so it
        // changes nothing on the loop-correctness path, only kills the stale
        // render. Uses the same reseed the GROUNDED pairing uses (aim := nose,
        // up := local_up), never the carried up of the dead life.
        st.aim.reseed(st.curr.orientation, st.prev_up);
        st.grounded = true;
        res.respawned = true;
    }

    // The target-drone fleet advances once per player tick (SPEC §0 S8-drone),
    // AFTER the player's crash/respawn so it is independent of the player's
    // life (a player crash does not touch the bandits). Additive + nullptr-
    // gated => the no-drone path above is bit-identical (the firewall).
    if (dw != nullptr) {
        for (drone::DroneState& d : dw->drones) drone::tick(d, ap, dw->dparams);
        // Advance the lead/time-on-target instrument against the ENGAGED bandit
        // (nearest ahead of the player nose AND within track_range), ONCE per
        // sim tick so the count is frame-rate independent (AT-9). Read-only —
        // only READS st.curr (const) and writes dw->meter, never the player
        // (the firewall). Skipped in RAW mode (the HUD hides the gunsight
        // there, so counting invisibly would pollute the session %).
        if (!in.raw_mode) {
            const glm::dvec3 pnose =
                st.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const int eng =
                select_engaged_target(dw->meter.engaged_index, dw->drones,
                                      st.curr.position, pnose, dw->gparams);
            dw->meter.engaged_index = eng;
            const sim::SimState* engaged =
                eng >= 0 ? &dw->drones[eng].curr : nullptr;
            const glm::dvec3 grav =
                ap.g * sim::gravity_dir(st.curr.position);  // world down accel
            render::meter_tick(dw->meter, st.curr, pnose, engaged, dw->gparams,
                               grav);
        }
    }
    return res;
}

// The per-FRAME input the caller resolves once from the frame-scope device
// sample (SPEC §10: sample devices once per frame). All RESOLVED values — no
// raylib device objects — so step_frame stays PURE/clock-free and the test can
// drive it headless. The per-frame mouse delta is NOT here: it is accrued by
// the caller across frames (no loss when render fps > sim) and passed by
// reference so a 0-tick frame carries it.
struct FrameInput {
    bool raw_mode = false;
    sim::Inputs raw_in{};  // used iff raw_mode
    double throttle = 0.0;
    // MB-flaps device passthrough (see TickInput; forwarded per tick).
    double flap_cmd = 0.0;
    double gear_cmd = 0.0;
    bool freelook_held = false;
    // S-orient (docs/comfort program Q3): the caller resolves the OrientTap
    // ONCE per frame (from the same freelook press-edge freelook reads) and
    // offers the fire on the FIRST tick of the frame only (step_frame gates
    // it), so a discrete double-tap fires exactly one orient event regardless
    // of how many ticks the frame carries. Default false => bit-identical.
    bool orient_cmd = false;
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
    // RMB-zoom mouse-gain scale (forwarded to every tick this frame). Defaulted
    // 1.0 => the pre-zoom frame is bit-identical (strict superset). See
    // TickInput::aim_gain_scale for why it is a binary button function, not the
    // eased fov (frame-rate independence).
    double aim_gain_scale = 1.0;
    // Vestigial (S7-raw removed the F1 screen-relative mouse — the mouse now
    // rotates the RAW carried aim frame §9.1). Retained only so the
    // FrameInput->TickInput forwarding stays uniform; app::tick no longer reads
    // them. The camera-render path uses main.cpp's OWN cam_fwd/cam_up, not
    // these.
    glm::dvec3 cam_fwd{0.0, 0.0, 0.0};
    glm::dvec3 cam_up{0.0, 0.0, 0.0};
};

struct FrameResult {
    int ticks = 0;           // whole fixed ticks stepped this frame
    bool respawned = false;  // a crash-reset fired during THIS frame
    // S-orient (docs/comfort program Q3): the ORIENT verb fired on some tick of
    // THIS frame — the caller hard-cuts its lagged cam_fwd :=
    // loop.aim.forward() (research REC-1: angular snap, position/FOV keep
    // smoothing). Default false.
    bool orient_fired = false;
    double consumed_dx = 0.0;  // mouse delta consumed into the aim this frame
    double consumed_dy = 0.0;  //   (0 on a 0-tick frame) — for the caller's
                               //   freelook-orbit camera (cosmetic, §9.2)
    // S-aimff instrument (the AT-9 companion): SUM over this frame's ticks of
    // the controller-seen aim rate * sim_dt — the smear INVARIANT (equal for
    // the same total mouse delta at any frame/tick partition). Consumed by
    // the frame-rate-integral test leg (test_aim_ff), never by the app/HUD —
    // an asserted instrument field, not an M1-class dead report.
    glm::dvec3 aim_rate_dt_sum{0.0};
    // No telem here: the HUD reads SimState (control::extract +
    // sim::load_factor on last_vhat, S5), never the frame's last-tick telem — a
    // FrameResult.telem would be a dead report field (the M1-class hole waiting
    // to happen).
    // render/rig-D port: the LAST tick's emitted commanded Inputs this frame
    // (mirror of TickResult::inputs on the final consumed tick), read-only,
    // for the render-only Fleet Rig control-surface deflection. Zero Inputs
    // (rest pose) on a 0-tick frame — the caller should hold its previous
    // value in that case, same discipline as the HUD's SimState read.
    sim::Inputs last_inputs{};
};

// One render frame (SPEC §10, the accumulator loop): advance the fixed-dt
// accumulator by frame_dt and run that many whole ticks through app::tick. The
// plant NEVER sees frame_dt — this is the frame-rate independence AT-9 verifies
// end-to-end (only the accumulator MECHANISM was unit-pinned before, in
// test_accumulator).
//
// The per-frame mouse delta (`pending_*`, by reference) is OFFERED on the one
// consuming tick and zeroed there; a 0-tick frame leaves it to carry to the
// next (no loss when render fps > sim). app::tick's CQ2 gate drops the aim's
// copy when freelook holds it; the caller routes `consumed_*` to the
// freelook-orbit camera.
//
// Crash-reset neutralization stays INSIDE the seam (Fable AT-9 consult P0-2):
// if a crash fires on tick 1 of a 4-tick frame, the remaining ticks of THIS
// frame must fly the fresh airframe on a DEAD stick (F2) — reborn at cruise
// power, not steered by the pre-crash input. Deferring that to the caller would
// leave those residual ticks flying the dead life's stick, and HOW MANY
// residual ticks there are is frame-rate dependent (3 at 30 fps, 0 at 240) —
// the exact frame-rate dependence AT-9 exists to forbid. `respawned` is still
// returned so the caller resets its PERSISTENT device state (the virtual
// stick), which step_frame, being device-free, cannot touch.
inline FrameResult step_frame(LoopState& st, Accumulator& accum,
                              double frame_dt, const FrameInput& fin,
                              double& pending_dx, double& pending_dy,
                              const sim::AircraftParams& ap,
                              const control::ControllerParams& cp,
                              DroneWorld* dw = nullptr) {
    FrameResult res;
    res.ticks = accum.advance(frame_dt);

    // A mutable copy of the frame input: a mid-frame respawn neutralizes it in
    // place so the remaining ticks fly neutralized (F2 / P0-2, above).
    FrameInput cur = fin;
    bool mouse_consumed = false;
    // S-aimff: the consuming tick's computed aim rate, forwarded (ZOH) into
    // every remaining tick of THIS frame — the smear that keeps the FF demand
    // integral frame-rate independent. Zeroed on a mid-frame respawn with the
    // rest of the resolved input (a dead life's hand must not steer the fresh
    // airframe).
    glm::dvec3 frame_aim_rate{0.0};
    for (int t = 0; t < res.ticks; ++t) {
        TickInput in;
        in.raw_mode = cur.raw_mode;
        in.raw_in = cur.raw_in;
        in.throttle = cur.throttle;
        in.flap_cmd = cur.flap_cmd;  // MB-flaps forward (mirror leg in AT-9)
        in.gear_cmd = cur.gear_cmd;
        in.freelook_held = cur.freelook_held;
        for (int i = 0; i < 3; ++i) {
            in.override_mask[i] = cur.override_mask[i];
            in.override_sign[i] = cur.override_sign[i];
        }
        in.cam_fwd =
            cur.cam_fwd;  // vestigial (S7-raw); forwarded for uniformity
        in.cam_up = cur.cam_up;
        in.aim_gain_scale = cur.aim_gain_scale;  // RMB-zoom mouse-gain scale
        // S-aimff: ticks after the consuming one carry the frame's smeared
        // rate (0 until the consuming tick computes it below).
        in.aim_rate_ff = frame_aim_rate;
        // Consume the accrued mouse on the FIRST tick of the frame (per-frame
        // delta, per-tick transport — the S5 mouse/tick split). Report it for
        // the caller's orbit; app::tick's CQ2 gate owns whether it reaches aim.
        const bool consuming = !mouse_consumed;
        if (consuming) {
            in.aim_dx = pending_dx;
            in.aim_dy = pending_dy;
            res.consumed_dx = pending_dx;
            res.consumed_dy = pending_dy;
            pending_dx = pending_dy = 0.0;
            // S-aimff: the consuming tick spreads its applied rotation over
            // the WHOLE frame (rate = rotation/(N*sim_dt), app::tick).
            in.frame_ticks = res.ticks;
            // S-orient: offer the double-tap fire on the FIRST tick of the
            // frame only (a discrete event fires exactly once, not once per
            // interleaved tick). `cur.orient_cmd` is cleared on a mid-frame
            // respawn below with the rest of the resolved input.
            in.orient_cmd = cur.orient_cmd;
            mouse_consumed = true;
        }

        const TickResult r = tick(st, in, ap, cp, dw);
        res.orient_fired = res.orient_fired || r.orient_fired;
        // render/rig-D port (2026-07-23, plane-model-only): the last tick's
        // emitted commanded Inputs, for the Fleet Rig's cosmetic control-
        // surface deflection (render-only, RA9 — read, never written back).
        // Overwritten every tick so the FRAME's value is the LAST tick's,
        // matching how the HUD reads SimState post-frame. Additive report
        // field, defaulted zero Inputs (rest pose) => no existing consumer
        // or test is affected.
        res.last_inputs = r.inputs;
        // S-aimff: forward the consuming tick's computed rate to the rest of
        // the frame; every tick's controller-seen rate integrates into the
        // smear-invariant instrument. Red-team P2-3: the forwarded WORLD
        // vector is ZOH'd, not re-transported across ticks 2..N — the frame
        // rotates under it by <= ~4e-4 rad of misalignment over 3 ticks at
        // V = 220 (curvature + maneuver rate), far under the FF's own
        // resolution — accepted.
        if (consuming) frame_aim_rate = r.aim_rate_ff;
        res.aim_rate_dt_sum += r.aim_rate_ff * ap.sim_dt;
        if (r.respawned) {
            res.respawned = true;
            // Neutralize the resolved input for the rest of THIS frame: fresh
            // airframe, dead stick, reborn at cruise power (st.curr is now the
            // spawn state — read its throttle, single source, F2).
            cur.raw_in = sim::Inputs{};
            cur.raw_in.throttle = static_cast<float>(st.curr.throttle);
            cur.throttle = st.curr.throttle;
            cur.flap_cmd = 0.0;  // spawn state is clean (flap/gear up)
            cur.gear_cmd = 0.0;
            cur.freelook_held = false;
            cur.orient_cmd = false;  // a respawn cancels a pending orient event
            for (int i = 0; i < 3; ++i) {
                cur.override_mask[i] = false;
                cur.override_sign[i] = 0.0;
            }
            pending_dx = pending_dy = 0.0;
            frame_aim_rate = glm::dvec3{0.0};  // dead hand off the fresh life
        }
    }
    return res;
}

// Focus loss (SPEC §9.5): drop the freelook hold and aim := nose — via
// snap_forward_to_nose, which KEEPS the carried-up holonomy (NOT reseed, which
// would re-seed up from local_up and level the camera roll; the S5 lesson). The
// caller additionally clears held device keys and the pending mouse delta.
inline void instructor_focus_loss(LoopState& st) {
    st.fl.reset();
    st.recov.reset();  // an alt-tab mid-roll must not resume the roll (S7-hrz)
    st.aim.snap_forward_to_nose(st.curr.orientation);
}

// The §9.2 chase-camera call site. `cam_forward` is the caller's lagged,
// velocity-anchored camera-forward (S7-cam Phase 1) — decoupled from the aim,
// so a keyboard yank no longer snaps the view. `cam_up` is the caller's CARRIED
// aim-frame up (S7-cam3, 2026-07-07 — reverses the S7-cam2 horizon-lock):
// `cam_up = loop.aim.up()`, so the camera shows the world from the aim/mouse
// frame's orientation and mouse-up == screen-up at any attitude (the
// post-maneuver mouse-inversion was the horizon-locked camera diverging from
// the carried mouse frame). It rolls with the aim through a loop; pole-free
// (never degenerates at the zenith). aim_chase_camera re-orthogonalizes it
// against cam_forward. Both are DOWNSTREAM of mouse->aim (the aim frame stays
// the raw §9.1 mouse basis), so RA9 holds. Extracting the call keeps the
// camera-up source pinnable.
inline render::CameraPose instructor_camera(const sim::SimState& draw_state,
                                            const glm::dvec3& cam_forward,
                                            const glm::dvec3& cam_up,
                                            const sim::AircraftParams& ap,
                                            const render::ChaseParams& chase,
                                            const render::CameraOrbit& orbit) {
    return render::aim_chase_camera(draw_state, cam_forward, cam_up, ap, chase,
                                    orbit);
}

}  // namespace app
