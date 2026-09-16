#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "control/extract.h"
#include "control/params.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/params.h"
#include "sim/state.h"

// The instructor's guidance SIGN primitives (SPEC §9.3 / §7 sign table),
// landed in Section 4a so AT-0 can pin every geometry->sign mapping against
// glm reality BEFORE any gain exists. Each returns a gain-free demand in
// radians; Section 4b's cascade shapes magnitudes (braking law, clamps,
// blend) strictly sign-preservingly through sign(e)*min(...) — the SIGNS
// live here and only here. control::step() itself arrives in 4b; per SPEC
// §5 these are its internal stage helpers, individually tested ONLY by
// AT-0 (the constitution's sanctioned exception to the one-golden-surface
// rule).
//
// Body frame (SPEC §7): +X right, +Y up, -Z forward. pitch-up = +omega_x,
// yaw-right = -omega_y, roll-right = -omega_z.

namespace control {

// Pointing demand: the angle*axis rotation taking the nose (-Z) onto the
// target, expressed in the BODY frame — component signs ARE the omega_des
// signs (target right => y < 0 = yaw-right; target up => x > 0 = pitch-up).
// atan2(|cross|, dot) is robust at all separations. Exactly astern the axis
// is geometrically ambiguous: tie-break = +X, the pitch-up pull-through
// (4b's near-astern elev-sign latch, SPEC §9.3, keeps the choice stable
// once made; the eps is a degeneracy guard, not a gain).
inline glm::dvec3 rotation_demand_body(const glm::dquat& orientation,
                                       const glm::dvec3& target_dir_world) {
    const glm::dvec3 t =
        sim::body_dir_of(orientation, glm::normalize(target_dir_world));
    const glm::dvec3 nose{0.0, 0.0, -1.0};
    const glm::dvec3 c = glm::cross(nose, t);
    const double s = glm::length(c);
    const double d = glm::dot(nose, t);
    const double angle = std::atan2(s, d);
    constexpr double kDegenerateEps = 1e-9;
    if (s < kDegenerateEps) {
        return d > 0.0 ? glm::dvec3{0.0} : angle * glm::dvec3{1.0, 0.0, 0.0};
    }
    return (angle / s) * c;
}

// Bank-to-turn (MANEUVER, SPEC §9.3): the angle about the nose axis from
// body-up to the target's lateral projection, + = target to the right of
// the body-up half-plane. Degenerate only for a target straight ahead or
// astern (x = y = 0) — those never reach the roll law (FINE regime / the
// astern pitch tie-break own them), asserted rather than comment-trusted.
// NOTE for 4b: aim := nose (every spawn/reset/freelook release) puts the
// target at exactly (0,0,-1) — gate the EVALUATION of this stage on the
// regime, don't compute-both-and-blend, or the first tick after respawn
// asserts.
inline double bank_error(const glm::dvec3& target_body) {
    assert(target_body.x * target_body.x + target_body.y * target_body.y >
           1e-18);
    return std::atan2(target_body.x, target_body.y);
}

// "Roll to put the target above the nose": target right => roll right =
// -omega_z, so the demand is the negated bank error. Rolling right carries
// body-up toward the target's side — bank_error converges to 0.
inline double maneuver_roll_demand(const glm::dvec3& target_body) {
    return -bank_error(target_body);
}

// FINE-mode wings hold (SPEC §9.3): drive phi toward the captured phi_held.
// phi_held > phi means MORE right wing down is wanted => roll right =
// -omega_z, so the demand is phi - phi_held (negative in that case).
//
// VALIDITY DOMAIN of the RAW SPEC phi: |actual bank| < 90 deg. phi is an asin
// and FOLDS past 90 deg (actual bank 100 deg reads phi = 80 deg and DECREASING
// as bank grows) — inside the fold this demand's feedback sign inverts and it
// rolls the plane further over. F2 (§7): the cascade wings-hold now passes the
// FOLD-SAFE `unfold_bank(phi, cos_phi_theta)` for BOTH args, so it stays valid
// (and correctly-signed) through full inversion — the old `cos_phi_theta > 0`
// gate on the wings-hold is gone. BALLISTIC alone still passes raw phi under
// the upright gate (out of F2 scope). AT-0 pins the fold + the unfold
// executably.
inline double roll_hold_demand(double phi_held, double phi) {
    return phi - phi_held;
}

// FOLD-SAFE bank (F2, §7 Chad 2026-07-06): SPEC §7's bank `phi` sign-EXTENDED
// past its fold so it stays monotone and correctly-signed through FULL
// inversion (range (-pi, pi]) — the drop-in that lets the ailerons keep the
// plane upright past 90 deg bank. `phi = -asin(dot(body_right, local_up))`
// FOLDS at 90 deg (bank 100 deg reads phi = 80 deg and DECREASING), so past
// vertical its own feedback sign inverts and the wings-hold rolls the WRONG way
// — which is why the old cascade GATED every wings-hold on `cos_phi_theta > 0`
// and simply quit rolling when inverted (belly-up forever after an Immelmann).
//
// The gate `cos_phi_theta = dot(body_up, local_up) >= 0` is EXACTLY "upright":
//   upright  -> return phi UNCHANGED (bit-identical to e.phi, so an upright
//               golden — even one that PITCHES — does not move: this touches
//               nothing in normal flight, only past 90 deg bank).
//   inverted -> reflect the folded asin about +/-pi: at bank 100 deg (phi read
//               back as +80) return 180 - 80 = 100; at 180 deg (phi 0) return
//               180. Monotone across 90 deg (both sides give +/-90 there, so it
//               is continuous — no branch discontinuity, no dither) and the
//               sign of `phi` picks the matching pole so a left roll unfolds to
//               -pi.
//
// This is preferred over the true roll-about-nose angle (atan2 of body_up from
// the horizon): that is a DIFFERENT quantity from SPEC's asin `phi` whenever
// the nose is pitched (they diverge by the cos(pitch) foreshortening), so it
// would move the upright golden — violating "F2 is roll-only, don't touch
// normal flight". Sign-extending the existing `phi` changes ONLY the inverted
// region.
inline double unfold_bank(double phi, double cos_phi_theta) {
    if (cos_phi_theta >= 0.0) return phi;  // upright: SPEC phi, bit-exact
    constexpr double kPi = 3.14159265358979323846;
    return (phi >= 0.0 ? kPi : -kPi) - phi;  // inverted: past the asin fold
}

// Yaw coordination: null sideslip. beta > 0 = velocity right of the nose
// (SPEC §7) => yaw the nose right onto it = -omega_y, so the demand is
// -beta. (Gated OFF below v_ballistic — the tail-slide lies, SPEC §9.6.4 —
// that gate is 4b cascade state, not this primitive.)
inline double coordination_yaw_demand(double beta) { return -beta; }

// ===========================================================================
// Section 4b — the cascade (SPEC §9.3/§9.4/§9.6). ONE step function (SPEC §5),
// the primitives above as its sign-preserving building blocks.
// ===========================================================================

enum class Regime { FINE, MANEUVER };

// S-rimshot v2 (v4 rung 2, UNIVERSAL — the flick-misconception fix,
// docs/v4_rimshot_MISCONCEPTION_handoff.md): the rebound capture's states.
// Chad's ruling 2026-07-17: the carry-through-rebound-to-dead-center is how
// the nose settles onto the aim for ANY deflection, ALWAYS, even mid-track —
// there is NO arm threshold, NO park gate, and aim_moved is never read here.
//   IDLE  — no event; every expression is the legacy tree. ENGAGE fires
//           directly from IDLE at the slow-in onset: the seek taper is the
//           binding branch AND the normal pointed demand dips below
//           engage_frac x the net closing rate (the self-calibrating "the
//           law stopped asking for the rate we have" — size-free by
//           construction; a well-led lockstep track sits at ratio ~1 and
//           never engages: no catching-up = nothing to carry).
//   CARRY — phase 1: the demand is HELD at carry x the incoming net rate —
//           through the crossing, to the far rim of the aim circle.
//   RETURN— phase 2: the TIME-OPTIMAL ARREST back to dead center — rate
//           rides the return_w cap, then the full-authority brake
//           sqrt(2*alpha*d_remaining) terminates AT center with ~zero rate
//           (the dead blow); exits to the normal law ON the center crossing.
// One bounce per ARRIVAL: the COMPLETION exits set a refractory latch that
// clears only once err grows past max(circle, handback_frac x the exit err
// bounded by the event's apex allowance) — without it the post-arrest coast
// (whose ratio ~ K_theta*tau ~ 0.11 sits under any engage_frac at real
// rate) re-engages every ~0.15 s: a ~7 Hz rim-amplitude hunt (AT-4's
// forbidden hunt). Measured honest releases land INSIDE the circle, so the
// clear is the circle edge in practice; the stored term fences the
// low-authority pathologies. The RE-FLICK exits (hand-back / yanks) never
// set the latch — an abandoned bounce chases instantly and re-bounces fresh
// (yank tested BEFORE the completion legs in RETURN: a cross-nose re-flick
// flips the crossed-anyway sign and would otherwise latch with the flick
// size). APEX-DEATH is completion-family and LATCHES at its exit err: the
// arrival attempt ended (a creeping banked endgame has nothing to carry —
// measured 1-3 deg/s on lateral flicks); unlatch would re-engage the same
// creep every apex (measured 4x churn).
// ENGAGE is additionally gated to the FINE endgame (err < blend_lo): in the
// blend band a banked arrival is flown by ROLL while the faded yaw/pitch
// demands LIE about the arrival — the ratio would fire on the lie and carry
// a near-zero rate (measured multi-engage stalls). Every arrival passes
// through the endgame, so the universal contract holds.
// carry = 0 keeps the state IDLE forever (structural off).
enum class CaptureState { IDLE, CARRY, RETURN };

// Controller internal state (SPEC §9.7): passed in, NEVER mutated; step()
// returns a NEW value (aliasing breaks golden determinism). Carries the
// controller's OWN held v-hat (§9.6 channel 1 — the seam forbids reading
// SimState.last_vhat) and the filtered AoA (the sole smoothing exception).
struct Internal {
    glm::dvec3 integ{0.0};                 // rate-error integral, body axes
    glm::dvec3 last_vhat{0.0, 0.0, -1.0};  // controller's held v-hat (§9.6)
    double aoa_filtered = 0.0;             // low-passed AoA (§9.3b)
    double held_bank = 0.0;                // phi_held, captured state (§9.3)
    Regime regime = Regime::FINE;
    bool deadzoned = false;
    bool push_mode = false;
    bool ballistic = false;
    // Direction latches (§9.3): 0 = unlatched, else the held sign (+1/-1).
    double roll_latch = 0.0;
    double elev_latch = 0.0;
    // S-righthand: seconds the pilot's hand has been OFF the aim. Feeds the
    // MB-right authority ramp; reset by any aim-moved tick.
    double hand_rest = 0.0;
    // Keyboard override (§9.5). ovr_ramp per body axis [pitch,yaw,roll], in
    // [0,1]: climbs by dt/ovr_ramp_time while the axis is held, reset to 0 the
    // tick it releases (the release transient is caught by the cascade
    // re-engaging under the braking law, not a ramp-down). pursuit gates the
    // outer pointing loop; it is suspended while ANY axis is held and resumes
    // on full release. any_override is the previous-tick mask-OR (the edge the
    // suspend/recapture fires on).
    glm::dvec3 ovr_ramp{0.0};
    bool pursuit = true;
    bool any_override = false;
    // Inverted auto-righting (MB-right, [auto_level] inverted_delay/_rate):
    // inv_rest accumulates rest-while-belly-up time; righting latches the
    // slow aileron roll to upright once the delay elapses. Hysteretic: arm
    // at rest (err < blend_lo) AND belly-up (cosPhiTheta < -band); disarm at
    // err > blend_hi, override, push engage, or upright (cos > +band).
    double inv_rest = 0.0;  // [s] rest-while-inverted accumulator
    bool righting = false;  // the slow-righting roll latch
    // Aim-motion gate (SPEC §9.3 as amended 2026-07-10): hand-at-rest time.
    // Born LARGE ("at rest since forever") so a fresh spawn latches the
    // deadzone immediately, exactly like the legacy path (AT-13's banked-
    // spawn shape is untouched); capped to rest_dwell in-step.
    double rest_time = 1.0e9;  // [s] time since the last aim-moved tick
    // S-aimff (v4 rung 1): first-order-filtered aim rate [rad/s, WORLD]
    // feeding the aim-rate feedforward — loop-shaping on the DERIVED rate
    // demand, never on the aim itself (§9.1 stays raw). aim_ff_tau = 0 =>
    // exact pass-through of in.aim_rate_world. Zeroed with the rest of
    // Internal by reset() (the GROUNDED reseed).
    glm::dvec3 aim_rate_filt{0.0};
    // S-rimshot v2 (v4 rung 2, universal): the rebound capture. cap_u is the
    // approach direction in the POINTING plane (pitch, yaw components of the
    // rotation demand), captured at ENGAGE and refreshed per tick in RETURN
    // (the live-center chase) — event state for a sub-second maneuver, like
    // elev_latch's held sign, never a world basis. cap_w_hold is the
    // carry-scaled incoming net rate CARRY holds; cap_crossed latches once
    // the error component along cap_u flips sign (the nose went past the
    // aim). cap_err0 is the event's REFERENCE ERROR, two lives: at ENGAGE
    // it is |demand| — the error-domain hand-back base (a rate-domain base
    // saturates against w_max at every knee-engage and kills the re-flick
    // contract); at a COMPLETION exit it is re-stored as the exit err — the
    // refractory latch's clear scale. cap_d_allow is the event's apex
    // allowance rim + w^2/(2*alpha), captured at the crossing and floored on
    // the MEASURED apex at RETURN entry — the yank exits compare |demand|
    // against break_frac x it (a parked aim physically cannot trip them).
    // cap_stall_ticks counts consecutive pre-crossing ticks with the net
    // closing rate <= 0 (the stalled/receding-aim apex-death exit).
    // cap_refractory = one-bounce-per-arrival: set ONLY by the COMPLETION
    // exits (arrest / crossed-anyway / inside-deadzone), cleared when
    // err > max(circle, handback_frac * cap_err0) — the arrest releases at
    // d = w*tau, OUTSIDE the circle for a fast event, and its coast closes
    // from there: a bare circle clear would re-open at the release tick and
    // the coast would re-engage (the hunt). The re-flick exits never set it
    // (a fresh rebound with zero dead window). All zeroed by reset().
    CaptureState capture = CaptureState::IDLE;
    double cap_ux = 0.0, cap_uy = 0.0;  // approach dir (pointing plane, unit)
    double cap_w_hold = 0.0;            // [rad/s] held carry demand (net)
    bool cap_crossed = false;           // nose crossed the aim this event
    double cap_err0 = 0.0;     // [rad] reference err (hand-back / refractory)
    double cap_d_allow = 0.0;  // [rad] apex allowance (yank exits)
    int cap_stall_ticks = 0;   // w_rel-dead tick count (CARRY apex-death /
                               // RETURN stall-completion guard)
    bool cap_refractory = false;  // one bounce per arrival latch
    // v3 POOL BALL: the glance is ONCE per event. false = RETURN's outbound
    // wall-brake leg may still run (entered from the glance surface);
    // latched true the first tick the net rate turns inbound (and at every
    // DIRECT-SEEK-family entry) — a brake overshoot that momentarily
    // reverses the rate near center must be pulled back by the inbound law,
    // never re-launched at the wall (unlatched, a wall<->center ping-pong
    // measured 700+ return_ticks with 16 reversals).
    bool cap_inbound = false;
    // Kernel v5 rung A (S-truedepth): the event's apex TARGET [rad] —
    // min(wall, depth_frac x the momentum's own stopping depth), captured
    // at ENGAGE like cap_err0 — the event's reference, never recomputed
    // from the decaying live rate.
    double cap_rim_t = 0.0;
};

// Controller input (the aim + mode). targetDir is the world-frame aim,
// already parallel-transported by the caller this tick (control/transport.h).
// Freelook (§9.5) arrives in Section 4d — it is purely the caller holding
// target_dir_world and routing the mouse to the camera; the pure core needs no
// freelook flag (SOLUTION §5.5).
struct Input {
    glm::dvec3 target_dir_world{0.0, 0.0, -1.0};
    double throttle = 0.0;  // device passthrough (SPEC §9.8)
    // MB-flaps: pilot-managed devices, passthrough EXACTLY like throttle (the
    // instructor never manages lift/energy for the pilot). Defaulted 0 =>
    // every pre-flap caller commands a clean airframe (strict superset; the
    // 4b golden reproduces bit-identically).
    double flap_cmd = 0.0;  // [0,1] commanded flap deflection fraction
    double gear_cmd = 0.0;  // [0,1] commanded gear extension
    // R4g wheel brakes: passthrough EXACTLY like throttle (the instructor
    // never manages energy); the GROUNDED plant regime is the only consumer.
    // Defaulted 0 => every pre-brake caller/golden rolls brake-free.
    double wheel_brake = 0.0;  // [0,1] held brake fraction
    bool grounded = false;  // GROUNDED enforcement in-core (§9.5)
    // Aim-motion gate (SPEC §9.3 as amended 2026-07-10): TRUE iff raw device
    // deltas were applied to the AIM this tick (the caller's apply_mouse site
    // — freelook camera orbiting and keyboard overrides do NOT count; they
    // never touch the aim). A gate bit, not a filter: nothing smoothed enters
    // the mouse->aim path (§9.1). Default false => every existing caller and
    // the whole golden/AT suite fly the legacy latch bit-identically.
    bool aim_moved = false;
    // S-aimff (v4 rung 1): the mouse-induced angular velocity of the AIM this
    // tick [rad/s, WORLD frame] — the aim vector's own rotation rate from the
    // pilot's hand (parallel transport EXCLUDED by construction at the
    // caller's apply_mouse site; frame-smeared by app::step_frame so it is
    // frame-rate independent in the integral sense — the AT-9 ban on
    // frame-quantized control signals). Feeds the aim-rate feedforward
    // (cp.aim_ff_gain). Default zero-vector => every existing caller/test/
    // golden feeds the FF nothing (structurally inert, defense in depth
    // behind the gain gate).
    glm::dvec3 aim_rate_world{0.0};
    // Per-axis keyboard override (§9.5), indexed [pitch, yaw, roll] to match
    // the inner loop. mask[i] = the axis key is held; sign[i] = +/-1 = the
    // commanded direction (the caller maps W/S, Q/E, A/D -> signs per §7). Off
    // by default: a caller that sets neither flies the pure instructor, so the
    // 4b golden reproduces bit-identically (override is a strict superset).
    bool override_mask[3] = {false, false, false};
    double override_sign[3] = {0.0, 0.0, 0.0};
};

// Per-tick telemetry (HARNESS §4): read alongside the flight log, never a
// gate. Includes true load factor beside the G-proxy (SPEC §16 CQ1).
struct Telemetry {
    double e = 0.0;             // total pointing error [rad]
    glm::dvec3 omega_des{0.0};  // commanded body rate (incl. feedforward)
    Extracted extracted{};      // phi/beta/cosPhiTheta/alpha/speed/...
    Regime regime = Regime::FINE;
    bool push_mode = false;
    bool ballistic = false;
    bool deadzoned = false;
    bool pursuit = true;       // false while a keyboard override is held (§9.5)
    double load_factor = 1.0;  // true n = |perp accel|/g (CQ1 telemetry)
    double aoa_filtered = 0.0;  // the low-passed AoA feeding the clamp (§9.3b);
                                // frozen below v_ballistic (the tail-slide lie)
    bool righting = false;      // MB-right inverted auto-righting latch
    // Blend-band instrument (jitter_attribution §6.5 pin #2): the regime mix
    // weight and the roll-hold setpoint, mirrored per tick so the boundary
    // state is READ from the tape, never re-inferred from bank/aileron. Pure
    // mirrors — no consumer of behavior reads them (the moved-consumer rule).
    double blend = 0.0;      // smoothstep(blend_lo, blend_hi, e)
    double held_bank = 0.0;  // ns.held_bank after this tick's captures/lean
    // S-righthand instrument (2026-09-12): the ROLL CHANNEL, limb by limb, so
    // an attribution never has to be inferred from a clamp value again (the
    // MB-right -180.0 signature in Chad's tape was read off the clamp, not
    // off the limb). Pure REPORT fields -- nothing in the kernel reads them.
    //
    // READ THEM AS "WHAT THIS TICK'S BRANCH EMITTED", NOT "the current value
    // of that limb" (red-team P2): each is written INSIDE the branch that
    // computes it, so on a tick that took another branch the field keeps its
    // DEFAULT, not a stale one -- safe only because Output (and therefore
    // Telemetry) is constructed fresh per control::step call. In particular
    // hand_gate reads 1.0 on every tick where the MB-right branch did not
    // run, which means "not applicable", not "full authority".
    double roll_hold = 0.0;      // the FINE wings-hold limb's emission
    double roll_maneuver = 0.0;  // the MANEUVER bank-to-turn limb's emission
    double roll_right = 0.0;     // the MB-right inverted-righting emission
    double hand_gate = 1.0;      // S-righthand's hand-rest authority ramp
    // S-rimshot (v4 rung 2): the event machine's state, mirrored for the
    // instrument and the tests. A state mirror alone is blind (the
    // moved-consumer trap) — every test pairs this probe with BEHAVIOR
    // assertions on the emitted demand.
    CaptureState capture = CaptureState::IDLE;
};

struct Output {
    sim::Inputs inputs{};
    Internal internal{};
    Telemetry telem{};
};

// Fresh internal state — every (re)spawn and while GROUNDED (SPEC §9.7).
inline Internal reset() { return Internal{}; }

// The pure controller (SPEC §9.7). Extraction happens inside (through the ONE
// shared control::extract) so a caller can never forget it or pass a stale
// frame; the filtered AoA and held v-hat live in `internal`.
// `env` all-null => bit-identical to v3. No default argument (Fable red-team):
// the compiler enumerates every call site so none silently passes null when the
// gravity/atmosphere fields go live in Phase 2.
Output step(const sim::SimState& s, const Input& in, const Internal& internal,
            const sim::AircraftParams& ap, const ControllerParams& cp,
            const sim::Environment* env, double dt);

// Inner-loop plant inversion (SPEC §9.4 / §7): the sim's EXACT torque model
// with the sim's EXACT params (config/aircraft.toml — single source), clamped
// ONLY to +/-1. Feeding this Input back through the plant reproduces tau_cmd
// below saturation — AT-18b, the controller-side single-count round-trip that
// AT-18a (which reads omega only) is structurally blind to. The denominator
// is > 0 always — c > 0 and q_att_floor > 0 are enforced at load
// (config/load_aircraft.cpp), q_eff >= q_att_floor > 0, delta_max_eff > 0 —
// so there is no divide-by-zero at v -> 0.
// Density-agnostic core: the inversion DENOMINATOR lives here ONCE — the
// altitude body and the R6 position+env overload both feed it their density,
// so the spatial migration can never fork the inversion from the altitude
// path (the exact fork AT-18b's spatial leg detects).
inline double plant_invert_at_rho(double tau_cmd, double c_axis, double speed,
                                  double rho, const sim::AircraftParams& ap) {
    // MB-atm: the inversion sees the SAME thinned density the plant applies
    // (H1: inverting the sea-level q at altitude would under-deflect by
    // 1/atm_frac). q_eff's floor keeps the denominator > 0 at any density.
    const double denom = c_axis *
                         sim::q_eff(sim::q_dyn(rho, speed), ap) *
                         sim::delta_max_eff(speed, ap);
    return std::clamp(tau_cmd / denom, -1.0, 1.0);
}
// Altitude body — the null-atm / test path (no bubbles present).
inline double plant_invert(double tau_cmd, double c_axis, double speed,
                           double altitude, const sim::AircraftParams& ap) {
    return plant_invert_at_rho(tau_cmd, c_axis, speed,
                               sim::rho_at(altitude, ap), ap);
}
// R6 SPATIAL overload — the LIVE inversion. The plant applies spatial density
// (sim::step -> atm_frac_at); the inversion MUST sample the SAME density at
// the SAME position, or a bubble edge under/over-deflects by 1/u (AT-18b).
inline double plant_invert(double tau_cmd, double c_axis, double speed,
                           const glm::dvec3& position,
                           const sim::Environment* env,
                           const sim::AircraftParams& ap) {
    return plant_invert_at_rho(tau_cmd, c_axis, speed,
                               sim::rho_at(position, env, ap), ap);
}

}  // namespace control
