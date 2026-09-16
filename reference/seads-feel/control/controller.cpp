#include "control/controller.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

// R6: the inversion/braking/load_factor now take (position, env) and sample
// the plant's thinned density via sim::rho_at(position,env,ap) — altitude is
// derived inside sim/aero.h, so this TU no longer needs sim/world.h.

// The cascade (SPEC §9.3/§9.4/§9.6), transliterated from SOLUTION §5.5's
// five-times-red-teamed reference core into the SEADS sign primitives that
// AT-0 froze (rotation_demand_body, bank_error/maneuver_roll_demand,
// roll_hold_demand, coordination_yaw_demand — controller.h). Damping is the
// measured-omega feedback, by construction: no derivative-on-error term
// exists anywhere in this file (SPEC §9.4). Every gate/regime/latch leg is
// hysteretic; the command blend is continuous in e (smoothstep) so the latch
// cannot limit-cycle the commands.

namespace control {

namespace {

// Braking-aware pointing law (SPEC §9.3): sign(e)*min(K|e|, sqrt(2*aB*|e|),
// wMax). The three branches meet in C0 kinks; the linear branch owns small e
// so there is no infinite-gain hunting near zero. Sign-preserving: it never
// changes the sign of its input demand.
double sqrt_law(double e, double K, double a_brake, double w_max) {
    const double m = std::min(
        {K * std::abs(e), std::sqrt(2.0 * a_brake * std::abs(e)), w_max});
    return e >= 0.0 ? m : -m;
}

double smoothstep(double lo, double hi, double x) {
    const double t = std::clamp((x - lo) / (hi - lo), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Seek law (§7 Chad 2026-07-06): the NOSE pursuit rate vs pointing error. Same
// shape as sqrt_law (sign-preserving, braking-capped) but the near-zero LINEAR
// branch — which vanishes as e->0 and makes the nose CREEP lazily into the
// centre — gets a STEP FLOOR w_step (the nose SNAPS toward the aim) plus a
// PARABOLIC rise K|e|(1+expo|e|) (a bigger deflection pulls progressively
// harder). The braking sqrt still caps it so a big flick STOPS cleanly, and the
// deadzone (the reticle CIRCLE) zeros the whole pointing inside — so the snap
// ends parked in the circle, not in a limit cycle (Chad's "step up in gain then
// a smooth parabolic rise; no direction change inside the mouse circle").
// w_step == 0 and expo == 0 recovers the plain sqrt_law.
// COUPLED RE-DERIVED COPY (instrument-scope H1, red-team P2-3): the harness
// ATTRIB branch classifier (test/harness/harness_main.cpp run_step) recomputes
// these seek/brake/ceiling candidates to attribute which branch binds per
// tick. It is an INSTRUMENT, not a truth source — but a shape change here
// (branch added/removed, composition reordered) silently misattributes there.
// Change both together.
double seek_law(double e, double w_step, double K, double expo, double a_brake,
                double w_max) {
    const double a = std::abs(e);
    const double seek = std::max(w_step, K * a * (1.0 + expo * a));
    const double m = std::min({seek, std::sqrt(2.0 * a_brake * a), w_max});
    return e >= 0.0 ? m : -m;
}

// S-rimshot v2: TRUE iff the seek/taper branch is the BINDING branch of the
// pointing law at error magnitude a — i.e. the min picks it over both the
// braking sqrt and the rate ceiling. The universal ENGAGE keys on this (the
// ledger-2.0 attribution: the felt slow-in IS the taper; the braking phase
// and the w_max plateau are the physics/G walls, not a slow-in — engaging
// out there would carry the plateau rate into a multi-degree fly-by past
// the aim). Kept ADJACENT to seek_law/sqrt_law: the SAME branch
// expressions — change them together (the harness ATTRIB coupling note
// above applies here too). Boundary <=: at the exact kink the branches are
// equal and "binding" holds — engage is legal from the first taper tick.
// Yaw's law is sqrt_law (no step floor, no expo): call with w_step = 0,
// expo = 0 and the expression reduces to its linear branch exactly.
bool seek_binding(double a, double w_step, double K, double expo,
                  double a_brake, double w_max) {
    const double seek = std::max(w_step, K * a * (1.0 + expo * a));
    return seek <= std::sqrt(2.0 * a_brake * a) && seek <= w_max;
}

// S-rimshot v2: an approach axis participates in the taper-binding AND only
// when it carries a real share of the approach direction (|u_i| above
// this). A relevance guard on the unit components (the
// rotation_demand_body kDegenerateEps class), not a feel dial: a near-zero
// component's branch state is meaningless for the arrival and must not
// veto (or bless) the engage.
constexpr double kCapAxisMix = 0.3;

}  // namespace

Output step(const sim::SimState& s, const Input& in, const Internal& internal,
            const sim::AircraftParams& ap, const ControllerParams& cp,
            const sim::Environment* env, double dt) {
    Output out;

    // Extract through the ONE shared path (SPEC §9.7). The controller guards
    // its OWN held v-hat (the seam forbids reading SimState.last_vhat, §9.6).
    const Extracted e = extract(s, internal.last_vhat, ap.v_dir_eps);
    out.telem.extracted = e;

    // Fold-safe bank (F2, §7 Chad 2026-07-06): the SIGNED roll valid through
    // full inversion. Every wings-hold / auto-level / push bank-hold roll now
    // measures bank with THIS instead of e.phi (which folds past 90 deg,
    // inverting its own feedback sign — the reason the old cascade GATED all of
    // them on cos_phi_theta > 0 and quit rolling when inverted). So the
    // ailerons keep the plane upright through an Immelmann / split-S instead of
    // leaving it belly-up. It is EXACTLY e.phi in the upright band (a pure
    // passthrough when cos_phi_theta >= 0), so the upright golden — pitched
    // pull included — does NOT move; only the inverted region changes.
    // BALLISTIC alone keeps bare e.phi + the upright gate (out of F2 scope —
    // the tail-slide alpha lies down there anyway).
    const double phi_full = unfold_bank(e.phi, e.cos_phi_theta);

    // GROUNDED (SPEC §9.5): zero Inputs, fresh internal state — EXCEPT
    // held_bank carries the CURRENT bank (a banked spawn must not fire an
    // uncommanded leveling roll on the first live tick) and the held v-hat
    // stays current (NaN-safety at the v->0 guard).
    if (in.grounded) {
        Internal fresh = reset();
        fresh.held_bank = phi_full;  // fold-safe capture (F2)
        fresh.last_vhat = e.vhat;
        out.internal = fresh;
        out.inputs.throttle =
            static_cast<float>(std::clamp(in.throttle, 0.0, 1.0));
        // MB-flaps: device passthrough survives GROUNDED like throttle (the
        // "zero Inputs" contract is about DEFLECTIONS; a grounded tick must
        // not command a one-tick retract twitch under a held device setting).
        out.inputs.flap_cmd =
            static_cast<float>(std::clamp(in.flap_cmd, 0.0, 1.0));
        out.inputs.gear_cmd =
            static_cast<float>(std::clamp(in.gear_cmd, 0.0, 1.0));
        // R4g: the brake is a GROUNDED input above all — dropping it on this
        // early-return would ship a brakeless instructor path (Fable (c)).
        out.inputs.wheel_brake =
            static_cast<float>(std::clamp(in.wheel_brake, 0.0, 1.0));
        out.telem.regime = fresh.regime;
        out.telem.held_bank = fresh.held_bank;  // the carried-bank mirror
        // telem.blend stays 0.0 BY CONVENTION on a GROUNDED tick (err is
        // never computed here; the respawn contract is aim := nose, so
        // blend 0 == FINE-at-spawn is the true reading). Tape consumers
        // doing blend-dwell reads must treat a respawn tick's 0 as this
        // convention, not a measured mid-hunt value (red-team P2-2).
        return out;
    }

    Internal ns = internal;  // the returned copy; caller's `internal` untouched
    ns.last_vhat = e.vhat;

    // BALLISTIC hysteresis (SPEC §9.6): enter below v_ballistic, exit above the
    // ~1.3x floor. Determined FIRST — the AoA filter (below) and the cascade
    // both gate on it.
    if (ns.ballistic && e.speed > cp.v_ballistic_exit) ns.ballistic = false;
    if (!ns.ballistic && e.speed < cp.v_ballistic) ns.ballistic = true;

    // AoA low-pass — the SOLE smoothing exception (SPEC §9.3b), first-order at
    // fixed sim_dt. It bounds authority (the protection clamp), never the
    // tracking error, so it is not on the mouse->aim->omega_des->Input path.
    // FROZEN while ballistic: below v_ballistic the tail-slide's alpha ~ +/-pi
    // is a lie (§9.6.4 gates the limiter off down there), and a low-passed lie
    // is exactly the "blended alpha" the ruling bans — carried into the
    // protection clamp on the way OUT it commands an uncommanded hardover for
    // ~3 tau. Re-seed from the now-valid raw alpha on the ballistic->cascade
    // EXIT edge so the clamp resumes from truth, not the stale ~pi it
    // accumulated below the floor.
    if (!ns.ballistic) {
        if (internal.ballistic) {
            ns.aoa_filtered =
                e.alpha;  // exit edge: reseed from the valid alpha
        } else {
            const double lp = std::clamp(dt / cp.aoa_filter_tau, 0.0, 1.0);
            ns.aoa_filtered =
                internal.aoa_filtered + lp * (e.alpha - internal.aoa_filtered);
        }
    }  // else ballistic: freeze (ns already carries internal.aoa_filtered)

    // S-aimff (v4 rung 1): low-pass the DERIVED aim-rate signal at fixed
    // sim_dt — loop-shaping DOWNSTREAM of the raw mouse->aim path, which
    // stays untouched (§9.1; the app-side frame smear already de-spikes
    // multi-tick frames, this is a tuning refinement). aim_ff_tau = 0 is the
    // exact pass-through arm (bit-identical signal). The state tracks every
    // airborne tick — ballistic included — so the feedforward (gated into the
    // pointing branch below, never the ballistic attitude-hold) resumes from
    // truth; GROUNDED zeroes it with the rest of Internal (reset()).
    if (cp.aim_ff_tau > 0.0) {
        const double lp = std::clamp(dt / cp.aim_ff_tau, 0.0, 1.0);
        ns.aim_rate_filt = internal.aim_rate_filt +
                           lp * (in.aim_rate_world - internal.aim_rate_filt);
    } else {
        ns.aim_rate_filt = in.aim_rate_world;
    }

    // Pointing error and its body-frame demand (AT-0 primitives). e == the
    // rotation angle; demand.x/.y ARE the pitch/yaw omega_des signs.
    const glm::dvec3 demand =
        rotation_demand_body(s.orientation, in.target_dir_world);
    const double err = glm::length(demand);
    const glm::dvec3 target_body =
        sim::body_dir_of(s.orientation, glm::normalize(in.target_dir_world));
    // Regime mix weight, hoisted (pure function of err + config — bit-identical
    // to the former in-branch local) so the telemetry mirror below reads the
    // ONE value the cascade blends with (§6.5 pin #2).
    const double blend = smoothstep(cp.blend_lo, cp.blend_hi, err);
    // De-rolled lateral azimuth (MB-lean's frame-true lateral axis — see the
    // lean block's comment for the full derivation), hoisted so the lean
    // update and the blend-band roll target continuity share ONE expression
    // (an in-band recompute would be the H1 fork). Same expression tree, same
    // inputs — the lean block's consumption is bit-identical.
    const double az_lat = std::atan2(
        target_body.x * e.cos_phi_theta + target_body.y * std::sin(e.phi),
        -target_body.z);

    // S-righthand: the hand-at-rest clock. Runs unconditionally (every
    // regime, every tick) so the MB-right ramp below can never be fooled by a
    // mode change. in.aim_moved is the CQ2-gated signal -- freelook and
    // GROUNDED ticks read "not moved", which is correct: the hand is off the
    // aim. Saturates at the dial so it cannot grow without bound.
    // THE HAND IS LIVE if the mouse fed the aim this tick OR the smeared
    // aim rate is still carrying this frame's motion. in.aim_moved ALONE is
    // WRONG and frame-rate dependent (red-team P1, the AT-9 class): it is a
    // per-TICK bit, true only on the ONE mouse-consuming tick of each frame,
    // so a hand that never rests still accumulates hand_rest on the other
    // N-1 ticks -- measured max 0 / 0.0083 / 0.025 / 0.058 / 0.092 s at
    // 240 / 60 / 30 / 15 / 10 fps. At 10 fps that is already gate 0.55, and a
    // 0.4 s frame hitch at an apex reaches 0.9967 -- the full 180 deg/s
    // righting handed back to a pilot who never stopped flying.
    // aim_rate_world is ZOH-smeared across ticks 2..N of the frame and
    // carries the SAME CQ2 gate (freelook / grounded report zero), so it is
    // the frame-rate-invariant reading of "the hand is on the aim".
    const bool hand_live =
        in.aim_moved ||
        glm::dot(in.aim_rate_world, in.aim_rate_world) > 0.0;
    ns.hand_rest = hand_live
                       ? 0.0
                       : std::min(internal.hand_rest + dt,
                                  std::max(cp.right_hand_rest, 0.0));

    // Regime hysteresis + heldBank capture on FINE entry (SPEC §9.3). The
    // latch gates ONLY the capture edge + telemetry; commands blend
    // continuously in `err` below.
    if (ns.regime == Regime::FINE && err > cp.blend_hi) {
        ns.regime = Regime::MANEUVER;
    } else if (ns.regime == Regime::MANEUVER && err < cp.blend_lo) {
        ns.regime = Regime::FINE;
        ns.held_bank = phi_full;  // fold-safe capture (F2)
    }

    // Keyboard override -> pursuit suspension (SPEC §9.5, S7-ovr), derived
    // IN-CORE from the mask edges so a caller can never forget it (the
    // fail-safe, SOLUTION §5.5). While ANY axis is held, pursuit of the
    // (mouse-owned) aim is suspended — the keyboard drives the airframe
    // directly below; coordination and rate damping stay live because they sit
    // OUTSIDE this gate. The caller does NOT move the aim during override
    // (S7-ovr2: the mouse owns it, so a keypress never swings the camera). On
    // full release, pursuit resumes and phi_held is recaptured at the current
    // bank; the braking law then flies the nose back to the held mouse aim from
    // wherever the yank left it. The recapture wins over the FINE-entry one
    // above if both fire this tick.
    const bool any_ovr =
        in.override_mask[0] || in.override_mask[1] || in.override_mask[2];
    if (any_ovr && !ns.any_override) {
        ns.pursuit = false;
    } else if (!any_ovr && ns.any_override) {
        ns.pursuit = true;
        ns.held_bank = phi_full;  // fold-safe capture (F2)
    }
    ns.any_override = any_ovr;

    // S-rimshot v2 (v4 rung 2, UNIVERSAL — the flick-misconception fix):
    // only the mode-safety legs live up here; ENGAGE and the whole event
    // (CARRY/RETURN, hand-back, yank exits, demand ownership) live in the
    // pointing branch below — they need the branch's pointed demand.
    // carry = 0 is the STRUCTURAL OFF: every capture block is skipped, the
    // state stays IDLE, and every gated expression downstream reduces to the
    // bit-identical legacy tree (the S-hrz rate = 0 pattern). There is NO
    // arm threshold, NO park gate, and in.aim_moved is never read by the
    // machine (Chad's ruling: the rebound is the universal arrival behavior
    // — any deflection, always, even mid-track).
    //  - Refractory clear FIRST (before any ENGAGE evaluation this tick):
    //    one bounce per ARRIVAL — the latch is set ONLY by the COMPLETION
    //    exits (the arrest / crossed-anyway / inside-deadzone legs), which
    //    also store the exit error as its clear scale, BOUNDED by the
    //    event's own apex allowance (P0-1). The clear needs
    //    err > max(circle, handback_frac * exit err). Measured honest
    //    releases land 0.06-0.15 deg INSIDE the 0.55 deg circle (the
    //    achieved rate at the tau surface is far below the return_w cap),
    //    so in practice the clear IS the circle edge — Chad's "aim leaves
    //    the reticle = new arrival" — and the stored-err term is the fence
    //    for the low-authority pathologies where a release could land
    //    outside and the closing coast (ratio ~ K_theta*tau ~ 0.11 at real
    //    rate) would re-engage every ~0.15 s: the ~7 Hz rim-amplitude hunt
    //    AT-4 forbids. Hysteretic by geometry (set at the bounded exit err,
    //    cleared a handback_frac margin above it, always >= the circle).
    //  - The RE-FLICK exits (hand-back / abandon / yank / apex-death) do
    //    NOT set the latch: the pilot re-aimed, the next arrival is
    //    genuinely new and must take a full fresh rebound with zero dead
    //    window (Chad's ruling: "abandon the bounce, chase instantly").
    //  - Override (pursuit false) / ballistic reset a live event to IDLE
    //    (not a completion — no latch). GROUNDED resets via
    //    control::reset() above. Freelook is invisible to the pure core by
    //    design — a held aim is just a parked target.
    if (cp.capture_carry > 0.0) {
        if (ns.cap_refractory &&
            err > std::max(cp.capture_circle,
                           cp.capture_handback_frac * ns.cap_err0)) {
            ns.cap_refractory = false;
        }
        if (!ns.pursuit || ns.ballistic) {
            // Mode reset: a live event dies AND any held refractory clears
            // (re-review round 2 P2-1: the approach-scale regime latches
            // store up to ~5.5 deg — un-cleared, a latch survives a whole
            // override episode / a freelook aim:=nose and eats the first
            // post-override arrival below ~6 deg; the AT-4 coast fence is
            // not in play here — that scenario has the aim parked and
            // pursuit live). Both triggers are already hysteretic.
            ns.capture = CaptureState::IDLE;
            ns.cap_refractory = false;
        }
    }

    // Deadzone hysteresis (SPEC §9.3, aim-motion gate as amended 2026-07-10):
    // pointing term -> 0 below deadzone_lo. With rest_dwell > 0 the latch
    // ALSO requires the hand at rest for the dwell, and an aim-moved tick
    // unlatches instantly — slow tracking is continuous (no relaxation-cycle
    // stairs) while the hand moves; the at-rest trim hold is untouched. The
    // gate is hysteretic in TIME (instant unlatch, dwell to relatch — a
    // strobing aim_moved cannot cycle it faster than the dwell), and the
    // unlatched pointing at sub-lo error is bounded by K_theta*deadzone_lo —
    // no lurch is representable. rest_dwell == 0 = LEGACY: aim_moved ignored,
    // this block bit-identical to the pre-gate arithmetic.
    const bool motion_gate = cp.deadzone_rest_dwell > 0.0;
    if (motion_gate) {
        ns.rest_time =
            in.aim_moved ? 0.0
                         : std::min(ns.rest_time + dt, cp.deadzone_rest_dwell);
    }
    if (ns.deadzoned &&
        (err > cp.deadzone_hi || (motion_gate && in.aim_moved))) {
        ns.deadzoned = false;
    }
    // S-rimshot: the deadzone latch is DEFERRED while the event owns the
    // pointing demand (CARRY/RETURN) — the crossing sweeps err through the
    // circle and a latch there would truncate the traverse (rung-2 spec:
    // "the deadzone zeroing MUST NOT truncate the event"). The event's
    // center-exit hands back to the normal law first; the latch then engages
    // on the NEXT evaluation exactly as legacy (pinned in test_capture). At
    // carry = 0 the state is IDLE forever, so both added conjuncts are
    // constant-true and this is the bit-identical legacy latch.
    if (!ns.deadzoned && err < cp.deadzone_lo &&
        (!motion_gate || ns.rest_time >= cp.deadzone_rest_dwell) &&
        ns.capture != CaptureState::CARRY &&
        ns.capture != CaptureState::RETURN) {
        ns.deadzoned = true;
    }

    // Curvature feedforward (SPEC §9.3): omega_ff = (local_up x v)/r, world ->
    // body, magnitude V/r. Added AFTER the deadzone (bypasses it) and
    // bypassing the G/AoA clamps. Mandatory from the first tick — level flight
    // limit-cycles without it. At v->0 it vanishes cleanly.
    //
    // The divisor is the ACTUAL orbital radius r = |position| = R + altitude,
    // NOT the baked planet radius ap.R. The great circle the aim is
    // parallel-transported along, and the level-flight path the ff must match,
    // both curve at V/|position|, not V/R. Dividing by ap.R over-commands
    // pitch-down by V*h/(R*(R+h)); the pointing loop cancels it with a steady
    // nose-UP demand, parking the aim ABOVE the nose by V*h/(R*(R+h)*K_theta)
    // — the V-scaled, altitude-scaled ff-deficit the PARK instrument exposed
    // (~18% of V/R at h=3000). Recomputed each tick, so it is self-correcting
    // at every altitude and at h=0 is bit-identical to the old ap.R divisor.
    // (Correctness fix: wrong radius constant, autoresearch attribution.)
    const glm::dvec3 omega_ff_world =
        glm::cross(e.local_up, s.velocity) / glm::length(s.position);
    const glm::dvec3 omega_ff_body =
        sim::body_dir_of(s.orientation, omega_ff_world);

    glm::dvec3 omega_des{0.0};
    // Per-axis IN-ENVELOPE rate bounds for the keyboard override (SPEC §9.5
    // S7-ovr): the SAME ceilings the cascade clamps to, set per regime below,
    // so a held axis is driven to its max in-envelope rate — never past it (the
    // old "leave the envelope" bypass is gone).
    double ovr_ceil[3] = {0.0, 0.0, 0.0};   // max +rate [pitch, yaw, roll]
    double ovr_floor[3] = {0.0, 0.0, 0.0};  // max -rate

    if (ns.ballistic) {
        // Reduced attitude-hold toward targetDir on q_att_floor authority
        // (SPEC §9.6): point the nose, rate-clamped to omega_bal; AoA limiter
        // and coordination gated OFF (the tail-slide lies, alpha ~ 180). Roll
        // levels only when upright — the phi fold (roll_hold_demand caveat).
        //
        // Gated on pursuit like the cascade below (SPEC §9.5): the ballistic
        // attitude-hold IS a form of pointing the parked cursor, so a keyboard
        // override suspends it too — else the non-held axes would keep
        // auto-pointing while the pilot forces recovery on the held one (the
        // "pursuit suspended while held" contract, silent otherwise). Suspended
        // -> omega_des stays 0, the inner loop just damps rates; the held axis
        // is taken over at the inner loop regardless.
        if (ns.pursuit) {
            omega_des.x =
                std::clamp(cp.K_theta * demand.x, -cp.omega_bal, cp.omega_bal);
            omega_des.y =
                std::clamp(cp.K_theta * demand.y, -cp.omega_bal, cp.omega_bal);
            if (e.cos_phi_theta > 0.0) {
                omega_des.z =
                    std::clamp(cp.K_phi * roll_hold_demand(ns.held_bank, e.phi),
                               -cp.omega_bal, cp.omega_bal);
            }
        }
        // Clean hysteretic re-entry: latches idle in ballistic (MB-right's
        // righting latch included — at v -> 0 the attitude-hold owns roll).
        ns.push_mode = false;
        ns.roll_latch = 0.0;
        ns.elev_latch = 0.0;
        ns.righting = false;
        ns.inv_rest = 0.0;
        // Override bound (S7-ovr): every axis clamps to the ballistic
        // attitude-hold rate down here (the cascade's G/AoA ceilings are
        // meaningless at v -> 0).
        for (int i = 0; i < 3; ++i) {
            ovr_ceil[i] = cp.omega_bal;
            ovr_floor[i] = -cp.omega_bal;
        }
    } else {
        // ===== Outer loop (cascade) =====
        const double g = ap.g;
        const double V_clamp = std::max(e.speed, cp.v_min);  // G-clamp V-floor
        const double w_max_pitch = (cp.n_max - e.cos_phi_theta) * g / V_clamp;
        const double w_min_pitch = (cp.n_min - e.cos_phi_theta) * g / V_clamp;

        // The pitch-rate envelope for the keyboard override (S7-ovr): the AoA
        // pushback, then the G floor AND ceiling applied LAST — EXACTLY the
        // pursuit clamp's 4-step sequence at the end of the pointing block
        // (§9.3b: "G floor and ceiling applied last so protection can never
        // command beyond the load limits"). clamp(A, w_min, w_max) reproduces
        // that sequence's saturated output (AoA-ceiling A_c >= AoA-floor A_f
        // always, n_max > n_min so w_min <= w_max). NOTE: it is NOT enough to
        // one-side-bound each — a pushback ceiling below w_min would let a held
        // pitch-up key command past the -G floor (the S7-ovr red-team P0). Both
        // bounds must sit inside [w_min_pitch, w_max_pitch].
        const double pitch_ceil =
            std::clamp(cp.K_aoa * (cp.aoa_max - ns.aoa_filtered), w_min_pitch,
                       w_max_pitch);
        const double pitch_floor =
            std::clamp(-cp.K_aoa * (cp.aoa_max_neg + ns.aoa_filtered),
                       w_min_pitch, w_max_pitch);
        ovr_ceil[0] = pitch_ceil;
        ovr_floor[0] = pitch_floor;
        ovr_ceil[1] = cp.yaw_max;
        ovr_floor[1] = -cp.yaw_max;
        ovr_ceil[2] = cp.p_max;
        ovr_floor[2] = -cp.p_max;

        // Per-axis braking margins from the airframe's TRUE authority (the
        // measured==derived alpha_max of AT-18a), read from the shared params
        // at the CURRENT position (MB-atm: authority thins with the
        // atmosphere above the taper; the braking law must brake against the
        // authority the plant actually has up there). R6: SPATIAL density
        // (position+env) so the estimate matches the plant's own thinned
        // authority inside a bubble edge (null env => identical, bit-for-bit).
        const double aB_pitch =
            cp.k_b * sim::ang_accel_max_derived(ap.c_pitch, ap.I_pitch, e.speed,
                                                s.position, env, ap);
        const double aB_yaw =
            cp.k_b * sim::ang_accel_max_derived(ap.c_yaw, ap.I_yaw, e.speed,
                                                s.position, env, ap);
        const double aB_roll =
            cp.k_b * sim::ang_accel_max_derived(ap.c_roll, ap.I_roll, e.speed,
                                                s.position, env, ap);

        // Coordination: always on, OUTSIDE the pointing gate (SPEC §9.3), so
        // deadzone/override suspension can't kill it.
        // S-yaw-magnet (SPEC §0 2026-07-10, Chad: "the magnet for yaw isn't
        // strong enough to pull it into the middle"): the anti-crab pull
        // fades near center so the rudder's own pointing finishes into the
        // circle instead of parking at the pointing-vs-coordination
        // equilibrium e_ss ~ K_coord*beta/(K_theta*yaw_scale). CONTINUOUS in
        // err (smoothstep — the blend precedent; no latch, nothing to
        // chatter): center_frac at err = 0 rising to 1 by center_band, so a
        // sustained banked turn (err >> band) keeps FULL coordination and
        // AT-16 holds by construction. center_frac = 1 short-circuits to the
        // legacy expression bit-identically (band unread; loader enforces
        // band > 0 whenever the relief is armed, so smoothstep's divisor is
        // never zero here).
        const double coord_scale =
            (cp.coord_center_frac < 1.0)
                ? cp.coord_center_frac +
                      (1.0 - cp.coord_center_frac) *
                          smoothstep(0.0, cp.coord_center_band, err)
                : 1.0;
        // S-aimff (red-team P1): the coordination seed is CAPTURED so the FF
        // gate below can bound POINTED+FF alone — coordination keeps its
        // documented above-the-ceiling exemption ([rate_clamps] yaw_max).
        const double yaw_coord =
            coord_scale * cp.K_coord * coordination_yaw_demand(e.beta);
        double yaw = yaw_coord;
        double pitch = 0.0, roll = 0.0;

        // Smooth wings auto-level at REST (SPEC §9.3, §7 Item-2 Part B): while
        // the pilot is NOT maneuvering (err < blend_lo — the mouse resting on/
        // near the nose) and upright, decay the wings-hold SETPOINT held_bank
        // toward 0, so the wings ease to LEVEL and hands-off flight settles
        // straight-and-level instead of holding a residual bank. The band is
        // err < blend_lo, NOT just the deadzone: a residual bank keeps turning,
        // which sweeps the aim a few tenths of a degree off the nose (out of
        // the tiny deadzone) — a deadzone-only decay STALLS there (observed ~7
        // deg). held_bank starts at the captured bank, so the roll STARTS from
        // it (roll ~0 on the first rest tick — a banked spawn fires no lurch,
        // AT-13) and eases to level with decelerating roll (exponential, no
        // overshoot, no latch). Gated on ns.pursuit so a keyboard override
        // SUSPENDS it ("unless a key is pressed"). NO cos_phi_theta > 0 gate
        // anymore (F2): held_bank is measured fold-safe (phi_full), so decaying
        // it toward 0 rolls the plane to UPRIGHT even from inverted (an
        // Immelmann/loop that ends belly-up now STAYS inverted at rest
        // (S7-loop-invert, Chad's loop ruling: "if I stop the aim inverted it
        // stays inverted; it only rights near level"). Re-gated on
        // cos_phi_theta > 0 (REVERSES F2's un-gating): a banked-but-UPRIGHT
        // rest still levels the wings; an INVERTED rest is left as-is — the
        // pilot rolls out by aiming (bank-to-turn), never an uncommanded
        // auto-right
        // ("the only auto-rotation is banking"). unfold_bank measurement is
        // KEPT (bit-exact upright, so the golden does not move).
        // Wings-leveling fade (S7-loop-invert red-team P2-1): a CONTINUOUS gate
        // for the CASCADE wings-leveling roll instead of a bare cos_phi_theta >
        // 0 toggle, so the knife-edge (wings ~ vertical) can't chatter
        // (CLAUDE.md "every gate hysteretic" — the previously-deferred
        // cosPhiTheta dither, now BROADENED to auto-level + deadzone). (The
        // separate BALLISTIC attitude-hold at v->0 keeps its own bare cos>0
        // gate on e.phi — out of F2 scope, low-authority/low-speed, untouched
        // here.) smoothstep(-band, +band, cos): EXACTLY 1 upright (cos >= band,
        // so the upright golden stays bit-identical), EXACTLY 0 inverted (stays
        // inverted), smooth only within the tiny +/-band around 90 deg bank.
        // roll_maneuver (bank-to-turn) and the push bank-hold are UNTOUCHED.
        const double wings_level_gate = smoothstep(
            -cp.wings_level_band, cp.wings_level_band, e.cos_phi_theta);
        // MB-lean (Chad 2026-07-08: "attracted like a magnet... hold a
        // shallower angle of bank with use of rudder at moments of moderate
        // deflection"): in FINE the held_bank setpoint decays toward a
        // proportional LEAN at the aim, not toward level — the lift vector
        // closes the last fraction of a degree the rudder/coordination
        // standoff used to park at (~0.5 deg), and a moderate lateral aim
        // holds a shallow bank instead of the blend band slamming to the
        // ~90 deg bank_error and auto-leveling back. az is the DE-ROLLED
        // lateral azimuth (right-positive): the bare body-frame x leaks the
        // VERTICAL error when banked (a banked climb aim would wag the wings
        // through level — the bank-contamination class). The de-roll axis is
        // (cosPhiTheta, sin(e.phi), 0) == cross(nose_b, local_up_b) — the
        // TRUE world-horizontal right direction in body coords, from the two
        // already-extracted fields, EXACT AT EVERY ATTITUDE (a phi_full
        // cos/sin pair is exact only at zero pitch: e.phi folds pitch into
        // the roll gauge and a pitched+banked vertical aim leaked ~7 deg of
        // phantom lean — the MB-lean diff red-team P1-1). The un-normalized
        // numerator self-fades toward vertical flight (|cross| -> 0), so a
        // near-vertical lean degrades gracefully to decay-toward-level
        // instead of a noise-gauged direction; pole-free here (err <
        // blend_lo keeps -target_body.z > 0.996). lean_gain == 0
        // reproduces the old decay-to-0 arithmetic BIT-IDENTICALLY (the
        // increment's product tree is load-bearing for that proof — do not
        // re-associate). wings_level_gate still multiplies the whole
        // increment: a belly-up rest stays frozen (S7-loop-invert); the
        // MB-right righting block below deliberately keeps its own UN-gated
        // decay toward 0 (righting targets level; its arm needs the aim near
        // the nose so the lean is ~0 there — the brief knife-edge overlap
        // where both run is a benign double decay).
        if (ns.pursuit && err < cp.blend_lo) {
            const double lean_target =
                std::clamp(cp.lean_gain * az_lat, -cp.lean_max, cp.lean_max);
            ns.held_bank += wings_level_gate * cp.auto_level_rate * dt *
                            (lean_target - ns.held_bank);
        }

        // Pointing gated on pursuit AND not-deadzoned (SPEC §9.5/§9.3): while
        // an override suspends pursuit, the pointing terms (pitch/bank-to-turn)
        // fall to zero exactly as in the deadzone, but coordination (above) and
        // the inner-loop rate damping stay live. Same idle-latches exit branch.
        if (ns.pursuit && !ns.deadzoned) {
            // (blend hoisted to function scope — the telemetry mirror shares
            // it.)

            // Elev sign latch near astern (hysteretic): the shortest-arc pitch
            // sign flips wholesale across straight-behind (AT-0) — hold it.
            if (err > cp.astern_on && ns.elev_latch == 0.0) {
                ns.elev_latch = (demand.x >= 0.0 ? 1.0 : -1.0);
            } else if (err < cp.astern_off) {
                ns.elev_latch = 0.0;
            }
            const double elev = (ns.elev_latch != 0.0)
                                    ? ns.elev_latch * std::abs(demand.x)
                                    : demand.x;

            const double w_push =
                seek_law(elev, cp.pursuit_step, cp.K_theta, cp.pursuit_expo,
                         aB_pitch, w_max_pitch);

            // bankErr + roll latch ONLY when the maneuver term is live
            // (blend > 0) AND the target has a real lateral component. Both
            // the NOSE (blend == 0, err ~ 0) and the ASTERN (blend == 1,
            // target_body ~ (0,0,+1)) degeneracies collapse x^2 + y^2 -> 0,
            // where bank_error ASSERTS (the aim := nose trap, controller.h;
            // exactly-astern is a representable held aim). In both, the roll
            // is left to the FINE wings-hold / the near-astern elev-sign
            // pull-through — bank_error is never evaluated on (0,0,+/-1).
            // This is a geometric DEGENERACY guard (like rotation_demand_body's
            // kDegenerateEps or the plant's lift_axis_len guard), NOT a control
            // regime gate — so a bare threshold is correct: bank direction is
            // genuinely undefined at the astern pole and a transported world
            // aim over a curving plant cannot pin at the measure-zero boundary
            // (the hysteresis-on-every-gate rule targets regime switches that
            // chatter between sustained commands, not singularity guards).
            const double lat_sq =
                target_body.x * target_body.x + target_body.y * target_body.y;
            double bank_eff = 0.0;
            bool have_bank = false;
            if (blend > 0.0 && lat_sq > 1e-12) {
                const double be = bank_error(target_body);
                if (std::abs(be) > cp.roll_latch_on) {
                    ns.roll_latch = (be >= 0.0 ? 1.0 : -1.0);
                } else if (std::abs(be) < cp.roll_latch_off) {
                    ns.roll_latch = 0.0;
                }
                bank_eff =
                    (ns.roll_latch != 0.0) ? ns.roll_latch * std::abs(be) : be;
                have_bank = true;
            } else {
                ns.roll_latch = 0.0;
            }

            // Push-vs-roll gate (SPEC §9.3, S7-push), every leg hysteretic.
            // Below the nose, decide on GEOMETRY, not the -G budget: PUSH (nose
            // down, at up to -n_min G) while the target is ahead of / not far
            // past the wing-line, and ROLL through (the loop / split-S) only
            // once it goes behind. target_body.z is the fore/aft component (< 0
            // ahead, > 0 behind), so the pitch-down angle from the nose is
            // acos(-z): z crosses 0 at exactly straight-down (90 deg) and the
            // enter/exit thresholds sit a little past vertical (config
            // down_enter/down_exit, stored as z = -cos(angle); z_enter < z_exit
            // = hysteresis). The down-dominant guard (|bank_eff| >
            // push_gate_bank) still keeps a down-AND-to-the-side aim on
            // bank-to-turn — only near-vertical pushes. (This replaces the old
            // ratio = w_push/w_min_pitch test, which rolled as soon as the pull
            // exceeded the -3 G budget — i.e. ~7 deg below the nose, the "can't
            // nose down without banking".)
            // World-horizon gate (§7 Item 2, THE lateral-reversal fix): the
            // aim's WORLD elevation (bank-independent), so a hard lateral flick
            // — which projects body-BELOW only because the plane is banked — is
            // NOT read as "nose down". Push (elevator-down chase) may engage
            // ONLY when the aim is genuinely below the horizon; otherwise the
            // bank-to-turn rolls the wings to the aim and pulls (Chad: "never
            // chase by elevator-down unless the mouse calls for nose-down").
            const glm::dvec3 aim_w = glm::normalize(in.target_dir_world);
            const double aim_elev = glm::dot(aim_w, e.local_up);
            // Sideways cone (§7 Item 2 refinement, Chad 2026-07-06): |aim out
            // of the plane's VERTICAL plane| = |dot(aim, normal)| where normal
            // = cross(nose, local_up). Confines push (pure pitch-down, no roll)
            // to a cone around LOCAL-down: a pure dive sits IN the plane
            // (sideways ~ 0, PITCHES), while a down-AND-to-the-side flick sits
            // OUT of it and ROLLS over to track. Bank-INDEPENDENT (references
            // local_up, not the body). Degenerate at vertical flight (nose ∥
            // local_up) — the plane is undefined there, so treat as in-plane (a
            // vertical dive pitches).
            const glm::dvec3 vplane_n = glm::cross(e.nose, e.local_up);
            const double vplane_len = glm::length(vplane_n);
            const double aim_side =
                (vplane_len > 1e-6)
                    ? std::abs(glm::dot(aim_w, vplane_n / vplane_len))
                    : 0.0;
            // Rung F "THE SACRED MIDDLE" (Chad 2026-07-24 fly): a
            // below-horizon, tightly IN-PLANE aim (within side_pure_enter of
            // the vertical plane) pure-pitches at ANY depth below the horizon,
            // not just past horizon_enter -- "when I nose straight down I need
            // a wider knife edge... maintain my horizon [as] I slowly pitch up
            // from the dive in that same direction." The standing rung-E ruling
            // (a lateral/ bank-over nose-down needs 45+ deg down) is UNTOUCHED:
            // this is an OR onto the horizon leg only, and the wider
            // side_cone_enter/exit gate still applies on every entry regardless
            // of which arm fires
            // -- a shallow LATERAL aim (past side_pure, under the outer cone)
            // still does not push (F2).
            const bool sacred_middle_enter =
                aim_side < cp.push_side_pure_enter && aim_elev < 0.0;
            const bool sacred_middle_exit =
                aim_side < cp.push_side_pure_exit && aim_elev < 0.0;
            if (ns.push_mode) {
                if (elev >= 0.0 || !have_bank ||
                    std::abs(bank_eff) <= cp.push_gate_bank_lo ||
                    target_body.z > cp.push_down_z_exit ||
                    (aim_elev > cp.push_horizon_exit && !sacred_middle_exit) ||
                    aim_side > cp.push_side_exit) {
                    ns.push_mode = false;
                }
            } else if (have_bank && std::abs(bank_eff) > cp.push_gate_bank &&
                       elev < 0.0 && target_body.z <= cp.push_down_z_enter &&
                       (aim_elev < cp.push_horizon_enter ||
                        sacred_middle_enter) &&
                       aim_side < cp.push_side_enter) {
                ns.push_mode = true;
                ns.held_bank = phi_full;  // fold-safe capture (F2)
            }

            if (ns.push_mode) {
                pitch = std::max(w_push, w_min_pitch);
                // yaw_scale INSIDE the gain slot (MB-rud): yaw_max is a
                // true ceiling on pointed yaw, SHARED with the keyboard
                // override's ovr_ceil[1] -- "highest deflection ALWAYS
                // keyboard" holds by construction. The braking branch stays
                // unscaled (braking is physics, not gain). The old post-scale
                // let this branch command yaw_scale*yaw_max, ABOVE the
                // override's ceiling.
                yaw += sqrt_law(demand.y, cp.K_theta * cp.yaw_scale, aB_yaw,
                                cp.yaw_max);
                // Fold-safe wings-hold during the push (F2): no cos_phi_theta >
                // 0 gate — a push that carries the plane past 90 deg still
                // holds its bank fold-safely instead of the roll cutting out
                // mid-maneuver.
                roll = std::clamp(
                    cp.K_phi * roll_hold_demand(ns.held_bank, phi_full),
                    -cp.p_max, cp.p_max);
            } else {
                // Hold the up-elevator until the wings ROLL to alignment, so a
                // large lateral aim FLIPS the bank toward the aim FIRST and
                // only then pulls (S7-turn2): cos(bankErr)^power. power=1
                // (plain cos) let the pull engage while only partly banked —
                // the nose chased the aim by pitch and the trajectory climbed
                // ("pushed up" in a turn). A higher power keeps the pull ~0
                // until well-aligned, so the roll leads and the pull turns
                // (horizontal) instead of climbing. Also kills the corkscrew
                // harder on big deflections.
                const double align =
                    have_bank ? std::pow(std::max(0.0, std::cos(bank_eff)),
                                         cp.bank_align_power)
                              : 1.0;
                pitch = std::max(w_push * ((1.0 - blend) + blend * align),
                                 w_min_pitch);
                // Rung C2c (S-holdline, docs/v5_kernel_handoff.md "RUNG C",
                // Chad fly-2: "it should hold the line of my mouse inputs and
                // try to get to my mouse until full stall ... even as it
                // dives and gains energy"). SUPERSEDES rung C2b's unscoped
                // servo (director ruling, the near-astern trace): the raw
                // sqrt_law(sag,...) servo fired on `sag > 0` ALONE, so it
                // fired in FINE too (stomping the capture machinery's yaw-band
                // glance and jinking legs) and, worse, at the near-astern
                // elev-sign latch's deliberate NEGATIVE pull-through (aim
                // ~175 deg behind), where `max(pitch, floor_w>=0)` silently
                // flipped a legitimately negative demand positive. Two more
                // gates, composed MULTIPLICATIVELY (no latch, no chatter --
                // the S7-loop-invert wings_level_gate precedent):
                //   - blend: this is a MANEUVER limb, exactly like
                //     roll_maneuver/yaw_gate above -- at blend = 0 (the FINE
                //     endgame) it is structurally OFF, so the capture
                //     machinery and every near-center behavior (and the
                //     goldens' FINE segments) are bit-untouched.
                //   - fwd_gate = 1 - smoothstep(0.85, 0.95, target_body.z)
                //     (rung D4, Chad's 2026-07-23 dive report: a 148 m/s
                //     full-left dive sat at err 90-130 deg, INSIDE the old
                //     0.3/0.7 fade -- z = -cos(err) there is ~0.0 to ~0.64,
                //     so the fade was already biting the pilot's ordinary
                //     committed dive, not just the true-astern edge it was
                //     meant to cede). FULL servo through the whole forward
                //     SPHERE now -- pure-lateral (z ~ 0), steep dives, even
                //     well past the wing-line -- until ~148 deg off the nose
                //     (z = 0.85), gone by ~162 deg (z = 0.95). Only the
                //     TRUE-astern latch geometry is ceded -- that narrow
                //     window still belongs to the elev-sign latch's
                //     deliberate (possibly negative) pull-through, and the
                //     servo may never stomp its sign there.
                // Only RAISES pitch (max with the legacy align/blend-faded
                // demand, never fights a legacy pull already fighting the
                // sag harder), through the SAME braking-law family as every
                // pointing demand (H1: sqrt_law, the plain form the yaw
                // calls use -- a servo, not the flick/pursuit law, so no
                // pursuit_step/expo). Zero AT zero sag (continuous -- no
                // momentum to slingshot, the AT-12 slingshot rung C2's floor
                // caused), proportional under a deep dive (the measured 12 s
                // dead-elevator sag-dive this rung exists to kill). Scaled by
                // pull_floor in [0,1] (0 = structural OFF, the ?: skips the
                // servo call entirely -- bit-identical legacy tree, the fly
                // fallback). Still bounded by the AoA/G clamp sequence
                // downstream -- protection never bypassed.
                //
                // Rung C2d bound (director ruling, the jink trace): the
                // servo bypassed the flown pursuit_step/expo shaping
                // (seek_law, not sqrt_law), so for an ORDINARY above-nose
                // ALIGNED approach (align = 1) it could EXCEED the legacy
                // demand and become a SECOND pointing law -- outside its
                // mandate, which is to recover only what the align fade
                // surrendered, never to re-shape an already-aligned pull or
                // re-seed a knife-edge bank_error degeneracy (jump 3's wrong-
                // way convergence traced to jump 2's assist re-seeding which
                // branch the roll-180-vs-pitch-down ambiguity picked -- the
                // servo was still CONVERGING, not wedged, but had no business
                // touching that geometry at all). The servo may never exceed
                // what the FULLY-ALIGNED law would command this tick: w_push
                // is the same-tick budget/brake-composed pull (seek_law,
                // already computed above THIS branch feeds), so
                // min(floor_w, w_push) caps it there. For an aligned approach
                // (blend = 1, align = 1) legacy pitch == w_push exactly, so
                // max(pitch, min(floor_w, w_push)) is BIT-INERT -- the servo
                // cannot rise above what pitch already is, by construction.
                if (cp.pull_floor > 0.0) {
                    const double sag = aim_elev - glm::dot(e.nose, e.local_up);
                    if (sag > 0.0) {
                        const double fwd_gate =
                            1.0 - smoothstep(0.85, 0.95, target_body.z);
                        const double floor_w =
                            std::min(blend * fwd_gate * cp.pull_floor *
                                         sqrt_law(sag, cp.K_theta, aB_pitch,
                                                  w_max_pitch),
                                     w_push);
                        pitch = std::max(pitch, floor_w);
                    }
                }
                // Bank-aligned rudder gate (MB-rud; supersedes S7-yaw's
                // blend fade): full rudder pointing while the bank is within
                // ~90 deg of aligned -- the nose CRABS onto the aim
                // immediately and through the whole roll-IN (Chad's Q1b
                // ruling: "nose should crab immediately and bank
                // immediately") -- fading to yaw_min_frac only past
                // knife-edge, where the split-S corkscrew lives (AT-15's
                // budget_frac; the old global blend fade capped yaw_scale at
                // 1.2 to protect exactly this window). cos is EVEN in
                // bank_eff: immune to roll_latch sign flips and the +/-180
                // wrap -- no new latch legs. Composed through blend EXACTLY
                // like the pitch align gate above ((1-blend) + blend*gate):
                // a bare have_bank?gate:1 would step ~(1-yaw_min_frac) of
                // the pointed yaw across err == blend_lo, because a lateral
                // aim's bank_eff saturates near 90 deg the instant have_bank
                // flips (plan red-team P0-1 -- the McRuer transition trap).
                const double yaw_gate =
                    have_bank ? cp.yaw_min_frac +
                                    (1.0 - cp.yaw_min_frac) *
                                        smoothstep(-cp.yaw_align_band, 0.0,
                                                   std::cos(bank_eff))
                              : 1.0;
                yaw += ((1.0 - blend) + blend * yaw_gate) *
                       sqrt_law(demand.y, cp.K_theta * cp.yaw_scale, aB_yaw,
                                cp.yaw_max);
                // S-straightline (2026-07-30, docs/straightline_thread.md —
                // Chad's spec: "the elevator increase should smoothly
                // coorelate to banking increase... The line that my tracers
                // draw should be straight"). AXIS-CORRECTION pitch
                // FEEDFORWARD: the ATTRIBUTED dip mechanism (lathold DIP
                // instrument, commit 1) is the CRAB's vertical component —
                // the rudder sweeps the nose toward a lateral aim about the
                // BANKED body-up axis, so sin(phi) of that sweep points at
                // the ground (~20 deg/s of nose-drop at 50 deg bank; gravity
                // sag is ~2-4% of the measured dip and stays the sag servo's
                // job). The exact nose-elevation kinematics use the SAME
                // frame-true pair as MB-lean's lateral axis (exact at every
                // attitude, no unfold):
                //     d(elev)/dt = pitch*cosPhiTheta + yaw*sin(e.phi)
                // so the pitch that makes the pointing sweep happen about
                // LOCAL UP (a horizontal, straight-tracer sweep) instead of
                // the banked body axis is w_axis = -yaw*sin(phi)/cosPhiTheta.
                // SIGNED: it also kills the upward kink when a reversal yaw
                // RAISES the nose. It cancels ONLY the yaw channel's
                // PARASITIC vertical component — the part moving the nose
                // AWAY from the aim's elevation line: with yv = yaw*sin(phi)
                // and sag = aim_elev - nose_elev, digging (yv < 0) is
                // parasitic when the aim is at/above the line (sag >= 0),
                // climbing (yv > 0) is parasitic when the aim is at/below it
                // (sag <= 0); each side fades over a +/-kLineParaBand of sag
                // (continuous — at the line BOTH cancel fully, the flick and
                // the reversal kink; past the band the yaw's motion TOWARD
                // the aim is the pointing arc itself and is never fought —
                // the additivity premise sweep caught the v2 all-component
                // cancel driving an elevated-aim-while-banked geometry into
                // the -G floor, an uncommanded push). A commanded climb/dive
                // flows through the pitch channel untouched. SCOPE: an
                // in-plane split-S carries
                // only a small transient demand.y mid-roll (yaw_gate faded
                // to yaw_min_frac there — integrated FF < ~1-2 deg, AT-15
                // re-stated as a bound); the PUSH branch has no complement
                // at all (a push-wedge down-and-lateral crab keeps its kink
                // — push is a commanded deep dive, the straight-tracer
                // contract does not bind there; re-audit P2-3).
                // KEYED ON THE EMITTED YAW (director condition 1): read
                // AFTER the yaw_gate/blend-composed pointing add above, so a
                // yaw the kernel is NOT commanding (faded past knife-edge,
                // FINE-blended) is never phantom-cancelled. The (parked,
                // carry=0) S-rimshot CARRY/RETURN overwrite below replaces
                // pitch wholesale, FF included — the event owns the demand,
                // same as S-aimff. Curvature ff (added outside the clamps)
                // is deliberately NOT cancelled — it is the sphere's frame
                // rotation, never a crab. S-aimff's LATER yaw add is ALSO
                // deliberately not cancelled (re-audit P2-1): aim_ff feeds a
                // matched pitch+yaw PAIR whose elevation effect is the
                // COMMANDED aim motion, not a crab — cancelling its yaw half
                // would double-count against its own pitch half. (Known
                // pre-existing residual: at the yaw_max clamp corner the
                // pair truncates asymmetrically — an aim_ff artifact this FF
                // neither causes nor fixes.)
                // BOUND STORY (director condition 2 — its own, not the dead
                // gravity rationale's): the term is demand-COUPLED but
                // structurally self-bounded — it can never exceed
                // |emitted yaw| * sin(phi)/cosPhiTheta, knife_fade caps the
                // 1/cosPhiTheta growth (product -> 0 continuously at the
                // knife-edge; past it the elevator's elevation authority has
                // collapsed anyway — the documented-known-limit top-rudder
                // window), and the AoA pushback downstream is the hard wall
                // (w_max_pitch never binds at n_max 32): full cancellation
                // on a hard flick IS a high-G level pull — the honest
                // real-airplane entry, a HEADLINE fly-card row. NOT capped
                // by w_push: on a pure lateral flick demand.x ~ 0 so
                // w_push ~ 0 — exactly where the crab digs hardest.
                // Multiplicative continuous gates only (the C2c scar; no
                // latch, no floor): blend (MANEUVER limb — FINE/goldens'
                // FINE segments/capture legs bit-untouched, honest additive
                // fade to 0 at the boundary), fwd_gate (shared D4 edges —
                // cedes the true-astern window to the elev-sign latch's
                // deliberate pull-through), knife_fade. The sag servo above
                // stays the residual-error backstop: feedback-plus-
                // feedforward, no double-pay (the servo is zero at zero sag;
                // with the sweep held level, sag never develops).
                // line_hold_ff = 0.0 is the STRUCTURAL OFF arm
                // (bit-identical v11 tree — fly kill-switch and golden
                // baseline arm).
                if (cp.line_hold_ff > 0.0 && e.cos_phi_theta > 0.0) {
                    // Instrument mirror: harness_main.cpp kDipKnifeBand.
                    constexpr double kLineKnifeBand = 0.2;  // [cos units]
                    // Parasitic-blend band, dot units (~5 deg of elevation;
                    // the audit P1-2 derivation: wide enough to clear
                    // plant-lag chop at the line, narrow enough that a
                    // 10-deg-past-the-line commanded arc is never fought).
                    constexpr double kLineParaBand = 0.087;  // [dot units]
                    // S-yawbudget: the vertical dig always allowed (rad/s) = 1 deg/s.
                    constexpr double kYawBudgetFloor = 0.017453292519943295;
                    const double knife_fade =
                        smoothstep(0.0, kLineKnifeBand, e.cos_phi_theta);
                    const double fwd_gate_ff =
                        1.0 - smoothstep(0.85, 0.95, target_body.z);
                    // S-yawbudget (2026-09-13, TARGET 2 -- the lateral
                    // nose-down). ORDER IS LOAD-BEARING: the budget scales
                    // `yaw` BEFORE the axis-correction below reads it, so the
                    // pitch feedforward cancels the rudder contribution that
                    // is ACTUALLY EMITTED. Applied AFTER, pitch cancels a dig
                    // the budget then removes -- over-correcting, and
                    // breaking S-straightline's stated contract that the FF
                    // writes pitch ONLY while yaw stays equal across arms.
                    //
                    // `avail` uses the pitch BEFORE the cancellation is
                    // spent: the elevator's spare vertical authority
                    // available TO spend. That is the causally correct
                    // quantity and it breaks the circularity
                    // (budget -> yaw -> w_axis -> pitch -> budget).
                    //
                    // See control/params.h for the measured decomposition on
                    // Chad's tape 4, the gate's V250 lat-90 numbers, and the
                    // honest scope (a mitigation, not a fix).
                    if (cp.yaw_vert_budget > 0.0 && e.cos_phi_theta > 0.0) {
                        const double yv_dig = yaw * std::sin(e.phi);
                        if (yv_dig < 0.0) {  // digging only, never climbing
                            const double sag_pre =
                                aim_elev - glm::dot(e.nose, e.local_up);
                            const double avail =
                                std::max(0.0, pitch_ceil - pitch)
                                * e.cos_phi_theta;
                            // BUDGET FLOOR (red-team P1-1, 2026-09-15):
                            // 1 deg/s of vertical dig is always allowed.
                            // With the elevator clipped `avail` is exactly
                            // 0, so without a floor the scale is 1 - gate
                            // for ANY dig -- including a dig that is
                            // negligible because sin(phi) is: measured on
                            // his tape 6, 475 ticks at |sin phi| < 0.2 had
                            // 27-30 deg/s of RUDDER removed for a 0.2 deg/s
                            // dig, and 9 phi sign-crossings landed on a
                            // killed tick (a yaw step of gate*yaw). The
                            // floor is CONTINUOUS at dig == floor (scale ->
                            // 1) and also covers the pure-pitch roundoff
                            // case (yv ~ 1e-17 << floor). Cost, measured:
                            // the dive recoveries give back 3-18 m.
                            const double budget = std::max(
                                cp.yaw_vert_budget * avail, kYawBudgetFloor);
                            if (-yv_dig > budget) {
                                const double gate =
                                    smoothstep(cp.yaw_vert_gap_lo,
                                               cp.yaw_vert_gap_hi, sag_pre);
                                const double yb_scale =
                                    1.0 - gate
                                              * (1.0 - budget / (-yv_dig));
                                yaw *= yb_scale;
                                out.telem.yaw_budget_scale = yb_scale;
                            }
                        }
                    }
                    const double yv = yaw * std::sin(e.phi);
                    const double sag_now =
                        aim_elev - glm::dot(e.nose, e.local_up);
                    const double w_dn =
                        smoothstep(-kLineParaBand, 0.0, sag_now);
                    const double w_up =
                        1.0 - smoothstep(0.0, kLineParaBand, sag_now);
                    const double parasitic = w_dn * std::min(yv, 0.0) +
                                             w_up * std::max(yv, 0.0);
                    const double w_axis =
                        -parasitic / std::max(e.cos_phi_theta, 1e-6);
                    pitch += blend * fwd_gate_ff * knife_fade *
                             cp.line_hold_ff * w_axis;
                }
                // Skid within the bank (S7-yaw2): the turn keeps its full
                // high-G bank-to-turn roll (Chad ruled OUT capping the bank —
                // flat turns were too low-G). The added skid comes purely from
                // the strong yaw POINTING above (yaw_scale/yaw_gate/c_yaw):
                // the nose crabs onto the aim WITHIN the hard bank, growing
                // sideslip, without flattening the turn or bleeding the G.
                // Blend-band roll TARGET continuity (the 5-10 deg roll slam,
                // 2026-07-30 — coverage-completion of MB-lean; the pitch
                // align / yaw_gate McRuer-transition treatment applied to the
                // roll limb's TARGET instead of its weight, because the
                // weights are ALREADY continuous — the slam is the in-band
                // tug-of-war between two near-saturated demands at targets
                // tens of degrees apart). Inside the band the maneuver limb
                // chases the mixed bank target
                //     blend*phi_commit + (1-blend)*lean_target
                // via bank_eff_used = bank_eff + w_eff*(1-blend)*(e_lean -
                // bank_eff), where e_lean = lean_target - phi_full is the
                // hold-style error in bank_eff's roll-right-positive
                // convention and lean_target is the LIVE
                // clamp(lean_gain*az_lat, +/-lean_max) — the value the frozen
                // held_bank WOULD be chasing (the ruled err < blend_lo gate
                // on the held_bank UPDATE does not move), so on a slow-add
                // both roll limbs AGREE in target and the boundary
                // tug-of-war collapses. Guards, in order:
                //   * roll_target_mix > 0: the structural knob-off arm —
                //     bank_eff_used IS bank_eff, bit-identical legacy.
                //   * blend < 1.0: smoothstep returns EXACTLY 1.0 at/above
                //     blend_hi, so a committed flick takes the identical
                //     expression tree — bit-untouched. This is an EVALUATION
                //     guard whose two limbs are continuous at the boundary
                //     (the added term -> 0 as blend -> 1): the lat_sq class,
                //     NOT a regime gate — no hysteresis needed.
                //   * w_eff rides wings_level_gate: inverted the continuity
                //     term fades to raw bank_eff exactly — split-S /
                //     knife-edge roll-through untouched (AT-15's legs are
                //     also all at blend == 1, doubly safe).
                // Pole-free in-band: blend < 1 => err < blend_hi =>
                // -target_body.z >= cos(blend_hi) ~ 0.988. The roll_latch and
                // the push gate keep reading the RAW bank_eff above — the
                // reshaping never feeds back into a latch.
                double bank_eff_used = bank_eff;
                if (cp.roll_target_mix > 0.0 && have_bank && blend < 1.0) {
                    const double lean_t = std::clamp(cp.lean_gain * az_lat,
                                                     -cp.lean_max, cp.lean_max);
                    const double e_lean = lean_t - phi_full;
                    const double w_eff = cp.roll_target_mix * wings_level_gate;
                    bank_eff_used =
                        bank_eff + w_eff * (1.0 - blend) * (e_lean - bank_eff);
                }
                const double roll_maneuver =
                    have_bank
                        ? -sqrt_law(bank_eff_used, cp.K_phi, aB_roll, cp.p_max)
                        : 0.0;
                // FINE wings-hold — faded by wings_level_gate (S7-loop-invert,
                // REVERSES F2's un-gating): ~1 upright, ->0 as it inverts.
                // Inverted, the wings-leveling term is 0, so a well-tracked
                // vertical LOOP (in-plane aim -> roll_maneuver ~ 0) rolls ZERO
                // and STAYS inverted while pitch carries it around the top; the
                // pilot rolls out by aiming. unfold_bank measurement KEPT
                // (bit-exact upright, golden unmoved); the bank-to-turn
                // roll_maneuver below is untouched, so a lateral turn keeps
                // blend*roll_maneuver and the DOWN split-S still rolls through
                // inverted. Clamped to +/-p_max.
                // S-leanlead (feel/yaw-bank-balance, 2026-09-10; params.h):
                // the wings-hold limb chases held_bank + lean_lead*d, where
                // d is the live lean's SAME-SIGN EXCESS over held_bank:
                //     d = lean_t - clamp(held_bank, min(0,lean_t), max(0,lean_t))
                // i.e. only the part of the lean that is MORE bank in its
                // own direction than the hold already carries. d == 0 (the
                // SAME double as hold_target := held_bank, so the tree below
                // is bit-identical to legacy) at: rest (lean_t == 0 -- the
                // AT-13 banked-spawn capture, the MB-right hand-off), the
                // lean RELEASE (held_bank at/beyond lean_t, same sign), an
                // aim shrinking toward the nose. An OPPOSITE-sign held_bank
                // (a MANEUVER->FINE capture at -40 deg with a small right
                // aim) leads by lean_t ALONE, never by the old bank: the
                // red-team P0 of the first cut (d = lean_t - held_bank fired
                // whenever the signs differed, so the target STEPPED by
                // lean_lead*|held_bank| -- ~30 deg/s of aileron at a 20 deg
                // carry -- across az_lat = 0, a bare-threshold chatter edge
                // under a dithering hand). This form is CONTINUOUS in lean_t
                // and in held_bank everywhere (a clamp of continuous
                // arguments), so no latch/hysteresis is needed: there is no
                // threshold. Pinned by the continuity leg in
                // test_yawbank_balance.cpp. lean_lead == 0 never enters the
                // branch: the structural OFF arm. Gated on blend < 1 (the
                // roll_target_mix precedent -- (1-blend) already zeroes this
                // limb at/above blend_hi, and in the band the maneuver limb
                // targets the live lean too, so the two limbs AGREE instead
                // of tugging; a bare err < blend_lo gate here would be a new
                // step at the FINE edge -- the McRuer transition trap).
                // Pole-free: az_lat is the frame-true de-rolled azimuth
                // hoisted above.
                //
                // S-leanlead-lateral (2026-09-12 walk-back fix; params.h
                // carries the derivation): the lead is scaled by the aim
                // offset's HORIZON-LATERAL SHARE |az_lat|/err. A pure-PITCH
                // aim reads share == |sin(phi)| (the de-roll numerator
                // collapses to sin(eps)*sin(e.phi) when target_body.x == 0),
                // so the lead is gated OFF through the shallow banks where
                // the loop/dive runaway seeds; a genuinely sideways aim
                // reads share == 1 at ANY bank, so the turn entry the dial
                // was built for keeps the full lead. Gating on |az_lat|
                // ALONE would not discriminate -- az_lat is nonzero for pure
                // pitch too, which is the whole bug. Continuous (a
                // smoothstep of a continuous ratio), so no latch and no
                // hysteresis. lean_lead_lateral == false takes the other
                // limb -- the SAME double cp.lean_lead -- so the landed v14
                // tree is bit-identical there, and lean_lead == 0 never
                // enters the branch at all. The divide is guarded by the
                // deadzone ENTER radius (config-relative, no magic
                // constant): inside it the pointing term is zeroed anyway
                // and the ratio is pure noise, so the lead is off.
                double hold_target = ns.held_bank;
                if (cp.lean_lead > 0.0 && blend < 1.0) {
                    const double lean_t = std::clamp(cp.lean_gain * az_lat,
                                                     -cp.lean_max, cp.lean_max);
                    const double d =
                        lean_t - std::clamp(ns.held_bank, std::min(0.0, lean_t),
                                            std::max(0.0, lean_t));
                    if (d != 0.0) {
                        const double lead =
                            cp.lean_lead_lateral
                                ? cp.lean_lead *
                                      ((err > cp.deadzone_lo)
                                           ? smoothstep(cp.lean_lead_lat_lo,
                                                        cp.lean_lead_lat_hi,
                                                        std::abs(az_lat) / err)
                                           : 0.0)
                                : cp.lean_lead;
                        if (lead != 0.0) {
                            hold_target = ns.held_bank + lead * d;
                        }
                    }
                }
                const double roll_hold =
                    wings_level_gate *
                    std::clamp(
                        cp.K_phi * roll_hold_demand(hold_target, phi_full),
                        -cp.p_max, cp.p_max);
                roll = blend * roll_maneuver +
                       (1.0 - blend) * roll_hold;
                // S-unload (1b5d98e83..40325f297) BUILT-AND-REMOVED 2026-09-15 at Chad's ruling
                // ("no deck save unload, keep the yaw budget"). It scaled the
                // PULL past 90 deg of bank_full while the flight path fell
                // below the aim; it saved the t6 deck event (AGL 0 -> 80) but
                // ARMED ON THE BACK HALF OF A PURE LOOP (25.9% of ticks), and
                // kernel v15 was signed on loops. The full actuator table is in
                // control/params.h; the code is recoverable at the scrap tag
                // scrapped/s-unload-20260915. Nothing of it remains here.

                // instrument-only report (Telemetry): the two limbs as
                // emitted, WEIGHTED, so the tape shows what each contributed.
                out.telem.roll_maneuver = blend * roll_maneuver;
                out.telem.roll_hold = (1.0 - blend) * roll_hold;
            }

            // S-aimff (v4 rung 1): aim-rate feedforward — the aim vector's
            // own mouse-induced angular velocity fed as an additive rate
            // demand, so the nose LEADS a moving aim instead of chasing its
            // error (a steady aim contributes exactly 0; feedforward adds no
            // error-loop gain). Gated ADD (the S-dampff/S-wvane ±0.0
            // discipline: at gain 0 the expression tree is the bit-identical
            // legacy one). Pitch is added BEFORE the AoA/G clamp sequence
            // below — protection bounds the FF, never bypassed. Yaw
            // (red-team P1 shape): the SHARED yaw_max ceiling bounds the
            // POINTED+FF sum ONLY — subtract the captured yaw_coord, clamp
            // pointed+FF, add yaw_coord back — so coordination rides ABOVE
            // the ceiling exactly as legacy (the documented [rate_clamps]
            // exemption, ~4-5 deg/s in a hard skid; a total-yaw clamp here
            // would silently revoke it). "Highest deflection ALWAYS
            // keyboard" holds on the pointed path: the pointed term is
            // already inside yaw_max from sqrt_law, so at ZERO aim rate the
            // clamp is an identity and a still mouse is untouched (the
            // steady-state contract, params.h). The whole shape lives INSIDE
            // the gain gate: gain 0 = the bit-identical legacy tree. NO
            // roll FF (roll demand is bank geometry, not aim pursuit); the
            // BALLISTIC attitude-hold never reaches here; a held override
            // axis is overwritten AFTER the pointing block, discarding the
            // FF by construction.
            // aim_rate_body serves BOTH gated mechanisms below (the S-aimff
            // lead and the S-rimshot frame-carry); hoisted so neither
            // re-derives it. Computing the local unconditionally changes no
            // output — each consumer stays inside its own knob gate.
            const glm::dvec3 aim_rate_body =
                sim::body_dir_of(s.orientation, ns.aim_rate_filt);
            if (cp.aim_ff_gain > 0.0) {
                pitch += cp.aim_ff_gain * aim_rate_body.x;
                yaw = yaw_coord +
                      std::clamp(
                          yaw - yaw_coord + cp.aim_ff_gain * aim_rate_body.y,
                          -cp.yaw_max, cp.yaw_max);
            }

            // S-rimshot v2 (v4 rung 2, UNIVERSAL): ENGAGE + the whole event
            // on the POINTING axes (roll = bank geometry, untouched — the
            // branch roll above stands). Placed AFTER the S-aimff block (the
            // engage comparison reads the post-ff pointed demand, so a
            // well-led moving track — where the ff already supplies the
            // lead — sits at ratio ~1 and never engages) and BEFORE the
            // AoA/G clamp sequence: every event demand rides through the
            // SAME protection legacy pointing does (never bypassed), and
            // the yaw ownership keeps the S-aimff red-team shape — the
            // shared yaw_max ceiling bounds the POINTED demand only,
            // coordination (yaw_coord) rides above it exactly as legacy.
            //
            // ONE net closing rate feeds every event site (ENGAGE ratio,
            // w_hold capture, crossing/apex detect, the arrest):
            // w_rel = (omega - omega_ff - aim_rate)·u - yaw_coord*u_y — net
            // of the curvature/transport rotation (raw omega carries the
            // V/r trim rate ~1 deg/s at V250; un-netted, level trim
            // noise-engages), the coordination demand (a crabbed arrival's
            // rudder rate is not closing rate), and the mouse-frame aim
            // rate (closing is measured RELATIVE to the moving crosshair).
            // The accounting invariant: measured net of X == commanded net
            // of X, with X re-added exactly once downstream (curvature ff
            // after this branch, yaw_coord at the yaw emission, the aim
            // rate as the emission's frame-carry) — so at carry = 1 the
            // engage-tick emission reproduces the measured omega:
            // rate-continuous entry by construction. NOTE the engage
            // comparison reads the PRE-clamp pointed demand (the AoA/G
            // sequence runs below): near protection the ratio is measured
            // in demand units, not the plant's clamped truth — a
            // clamp-limited arrival engages a touch early and its event
            // demand then clamps honestly downstream (the envelope leg
            // pins that path).
            if (cp.capture_carry > 0.0) {
                const double wnx =
                    s.angular_vel.x - omega_ff_body.x - aim_rate_body.x;
                const double wny = s.angular_vel.y - omega_ff_body.y -
                                   aim_rate_body.y - yaw_coord;
                // YAW EXACTNESS (v3 red-team P1-1): the EVENT plans and
                // drives in ERROR-space rate — net of the curvature ff and
                // the aim rate but NOT of yaw_coord. The error geometry
                // (s_u, the crossing, the rim) moves at the PHYSICAL rate,
                // which carries the delivered coordination: netting
                // yaw_coord out of the plan while the emission re-adds it
                // made every kinematic surface blind to the coordination
                // bleed — the building sideslip's K_coord*(-beta) opposed
                // the outbound carry and the nose died at ~1/3 of the wall
                // before the brake surface fired (measured yaw rim
                // 0.29-0.44 vs pitch 0.95-1.00). wny stays for the ENGAGE
                // ratio (a crabbed arrival's rudder rate is not closing
                // rate — the trigger contract is unchanged); everything
                // the EVENT owns (glance depth, w_hold, crossing/apex/
                // brake kinematics, the RETURN laws, the coast) reads
                // wny_ev, and the event emission COMPENSATES yaw_coord
                // inside the yaw_max clamp so the demanded error-space
                // rate is what the error actually flies. The accounting
                // invariant holds one level up: measured-in-error-space ==
                // commanded-in-error-space, with the clamp still bounding
                // the TOTAL pointed+compensation demand at yaw_max
                // (protection never bypassed).
                const double wny_ev = wny + yaw_coord;
                // ENGAGE (IDLE -> CARRY): the self-calibrating slow-in
                // onset — the seek taper is the BINDING branch (per-axis,
                // ANDed over the relevant axes) and the pointed demand
                // dipped below engage_frac x the net closing rate ("the law
                // stopped asking for the rate we have"). Size-free and
                // park-free: fires for ANY deflection, hand moving or not
                // (Chad's universal ruling). The closed-loop decay ratio is
                // MONOTONE-falling into its asymptote (K_theta/lambda_slow
                // ~ 0.86 pitch @V220), so the band is crossed at most once
                // per approach — no chatter leg needed; the refractory (one
                // bounce per arrival, top of step) is the only re-entry
                // gate. u is captured HERE, where the error direction is
                // fresh — event state for a sub-second maneuver (the
                // elev_latch pattern), never a world basis.
                // FINE-endgame gate (diff red-team P0-2, measured): ENGAGE
                // only below blend_lo, where the seek/sqrt pointing laws OWN
                // the arrival. In the MANEUVER blend band a lateral/banked
                // arrival is flown by the ROLL channel while the yaw/pitch
                // pointed demands are deliberately faded (yaw_gate,
                // blend*align) — the ratio then dips on a demand that LIES
                // about the arrival, ENGAGE captures w_hold at 0-8% of the
                // real closing rate, and CARRY replaces the demand with a
                // crawl (measured on the shipped build: yaw-20 @V140
                // engages=3 reversals=5, 1.4 s stalls). NOT a deflection-
                // size gate: every arrival PASSES THROUGH the FINE endgame
                // on its way to center and bounces there (pitch already
                // engaged at 5.4-7.7 deg — this moves it to <= blend_lo,
                // ticks later), so Chad's universal ruling holds — any
                // deflection, always; the carry begins where the pointing
                // law honestly reads the approach.
                if (ns.capture == CaptureState::IDLE && !ns.cap_refractory &&
                    err < cp.blend_lo) {
                    const double dlen =
                        std::sqrt(demand.x * demand.x + demand.y * demand.y);
                    if (dlen > 1e-9) {
                        const double ux = demand.x / dlen;
                        const double uy = demand.y / dlen;
                        const double w_rel = wnx * ux + wny * uy;
                        // Frame ruling (diff red-team P2-3): `pointed` reads
                        // the EMITTED demand — error law + the gain-scaled
                        // aim_ff lead — while w_rel nets the FULL aim rate.
                        // Intended semantics: the ratio asks "is the LAW
                        // (everything it would emit) still asking for the
                        // closing rate we carry RELATIVE to the crosshair?"
                        // — a (1-gain)*aim_rate bias vs a fully frame-
                        // relative compare, which only strengthens the
                        // lockstep quiet (pointed keeps the lead term while
                        // w_rel ~ 0) and delays engage marginally on a
                        // fast-moving catch-up (leg 5's 12 deg/s arm pins
                        // the behavior: still exactly one bounce).
                        const double pointed =
                            pitch * ux + (yaw - yaw_coord) * uy;
                        // Per-axis taper-binding, ANDed over the axes
                        // carrying the approach. The AND-form is
                        // load-bearing: an OR on a mostly-yaw diagonal
                        // fires while yaw still rides its plateau and the
                        // carried rate blows the apex out. In the MANEUVER
                        // blend band the emitted pitch is the pull
                        // machinery, not seek_law — there this is a shape
                        // CLASSIFIER ("the error is inside the taper
                        // region"); the ratio test on the real emitted
                        // demand is the actual protection.
                        bool taper = true;
                        if (std::abs(ux) > kCapAxisMix) {
                            taper = seek_binding(
                                std::abs(demand.x), cp.pursuit_step, cp.K_theta,
                                cp.pursuit_expo, aB_pitch, w_max_pitch);
                        }
                        if (taper && std::abs(uy) > kCapAxisMix) {
                            taper = seek_binding(std::abs(demand.y), 0.0,
                                                 cp.K_theta * cp.yaw_scale, 0.0,
                                                 aB_yaw, cp.yaw_max);
                        }
                        if (taper && w_rel > cp.capture_w_eps &&
                            pointed < cp.capture_engage_frac * w_rel) {
                            // S-rimshot v3 (POOL BALL, Chad's Q3): the glance
                            // classifier at ENGAGE — the projected glance
                            // depth past center w_rel^2/(2*alpha_u) (alpha_u
                            // the trackable full-authority decel along u,
                            // the same min-over-axes composition as the
                            // RETURN ceiling — H1, same derived call). Under
                            // glance_frac x circle there is no glance-worthy
                            // momentum: DIRECT-SEEK — enter RETURN at once
                            // (u = -demand-hat, exactly the axis RETURN's
                            // per-tick refresh maintains) so the capture is
                            // the ACTIVE full-authority sqrt drive + the
                            // dead-blow, never the legacy taper creep. This
                            // is the universal terminal capture: the banked
                            // LATERAL arrival (closing 1-3 deg/s through the
                            // bank/lean standoff) takes this path instead of
                            // the old one-attempt-then-regroup. d_allow is
                            // seeded from the MEASURED entry error (the yank
                            // exit compares against break_frac x it — an
                            // honest direct approach only shrinks the error
                            // and can never trip it).
                            const double a_p = sim::ang_accel_max_derived(
                                ap.c_pitch, ap.I_pitch, e.speed, s.position, env, ap);
                            const double a_y = sim::ang_accel_max_derived(
                                ap.c_yaw, ap.I_yaw, e.speed, s.position, env, ap);
                            const double alpha_u =
                                std::min(a_p / std::max(std::abs(ux), 1e-9),
                                         a_y / std::max(std::abs(uy), 1e-9));
                            // Glance depth from the ERROR-space physical
                            // rate (the ball's actual closing speed — the
                            // netted w_rel stays the TRIGGER's measure, but
                            // the glance/carry take the speed the error
                            // really has; on a crabbed arrival they differ
                            // by yaw_coord*u_y, ~8% at the measured yaw
                            // grid engages). An away-moving physical rate
                            // has no glance-worthy momentum (floor 0 ->
                            // DIRECT-SEEK). This is also what keeps the
                            // engage-tick emission rate-continuous in
                            // omega space (leg 8d/9): at carry = 1 the
                            // compensated emission reproduces the measured
                            // omega exactly.
                            const double w_ev =
                                std::max(wnx * ux + wny_ev * uy, 0.0);
                            const double glance = w_ev * w_ev / (2.0 * alpha_u);
                            // Kernel v5 rung A (S-truedepth,
                            // docs/v5_kernel_handoff.md): the glance depth
                            // this event EARNS is its own stopping distance
                            // under full trackable braking, never deeper
                            // than the wall — min'd against rim_frac*circle,
                            // never re-derived from the live rate once
                            // captured (cap_rim_t is the event's ENGAGE-time
                            // reference, like cap_err0). depth_frac <= 0 is
                            // the v4 fixed-rim OFF arm (always the wall).
                            // Rung A2 (Chad's fly-1 verdict "a little more"):
                            // the sub-wall CURVE — rim_live = rim * s^pow on
                            // the saturation s = min(1, depth*glance/rim).
                            // s = 1 (wall-earning) is a FIXED POINT at every
                            // pow, so committed arrivals are untouched;
                            // below the wall pow > 1 presses small events
                            // quadratically shallower (the felt ask: soften
                            // the small end ONLY). pow == 1.0 takes the
                            // literal-s branch — bit-identical rung A (the
                            // knob-off arm; std::pow(x,1.0) exactness is
                            // toolchain luck we do not lean on).
                            const double rim_full =
                                cp.capture_rim_frac * cp.capture_circle;
                            double rim_live = rim_full;
                            if (cp.capture_depth_frac > 0.0) {
                                const double sat =
                                    std::min(1.0, cp.capture_depth_frac *
                                                      glance / rim_full);
                                rim_live =
                                    cp.capture_depth_pow != 1.0
                                        ? rim_full *
                                              std::pow(sat,
                                                       cp.capture_depth_pow)
                                        : rim_full * sat;
                            }
                            if (glance <
                                cp.capture_glance_frac * cp.capture_circle) {
                                ns.capture = CaptureState::RETURN;
                                ns.cap_ux = -ux;
                                ns.cap_uy = -uy;
                                ns.cap_w_hold = 0.0;
                                ns.cap_crossed = true;
                                ns.cap_err0 = dlen;
                                ns.cap_rim_t = rim_live;
                                ns.cap_d_allow =
                                    std::max(rim_live + glance, dlen);
                                ns.cap_stall_ticks = 0;
                                ns.cap_inbound = true;  // no glance leg
                            } else {
                                // WALL-REACHABILITY clamp (v3 red-team
                                // P1-2, Chad's ruling: never-past-the-rim
                                // WINS over full-speed-through-center): a
                                // carried rate whose stopping distance
                                // exceeds the ring makes the wall
                                // unreachable (measured yaw 10@140:
                                // w_cap 34.6 dps, stop ~0.75 deg > the
                                // 0.55 deg ring -> rim 1.26). Cap the hold
                                // at the rate the wall can absorb:
                                // w_wall = sqrt(2*alpha_u*rim) at the
                                // TRACKABLE authority alpha_u (same
                                // derived call as the brake surface — H1).
                                // Deliberately NO damping-assist credit: a
                                // full-w damp credit treats the -damp*q*w
                                // decel as constant over the stop and
                                // over-promises (measured: crediting it
                                // passed 33.3 dps at V140 and the apex
                                // still overran to rim 1.21; bare alpha_u
                                // caps ~29.6 and the rim-targeted outbound
                                // law lands the glance ON the wall from
                                // any rate at or under it). Re-checked
                                // live through CARRY (V/alt drift). The
                                // crossing stays brisk (~18-30 dps — fast
                                // across a 0.55 deg circle); pitch's grid
                                // rates already fit under it (measured
                                // w_cap 14.9-26.4 vs wall ~29-38) so the
                                // clamp binds only where physics made the
                                // spec unmeetable.
                                // rim_t is the momentum-earned rim_live
                                // computed above (S-truedepth): at
                                // depth_frac >= 1 the depth term binds only
                                // when glance < rim_full/depth_frac, and
                                // there w_wall = sqrt(2*alpha_u*depth_frac*
                                // glance) = w_ev*sqrt(depth_frac) >= w_ev —
                                // the clamp below binds only at the wall cap
                                // (never below the arriving rate), exactly
                                // as legacy at depth_frac <= 0 (rim_t ==
                                // rim_full there).
                                const double rim_t = rim_live;
                                const double w_wall =
                                    std::sqrt(2.0 * alpha_u * rim_t);
                                ns.capture = CaptureState::CARRY;
                                ns.cap_ux = ux;
                                ns.cap_uy = uy;
                                ns.cap_w_hold =
                                    std::min(cp.capture_carry * w_ev, w_wall);
                                ns.cap_crossed = false;
                                ns.cap_err0 = dlen;
                                ns.cap_rim_t = rim_t;
                                ns.cap_d_allow = 0.0;
                                ns.cap_stall_ticks = 0;
                                ns.cap_inbound = false;
                            }
                        }
                    }
                }
                if (ns.capture == CaptureState::CARRY) {
                    // AXIS CO-ROTATION (yaw-exactness, measured): the
                    // captured u is a direction in the body's transverse
                    // pointing plane, but the body ROLLS through the
                    // arrival (the bank-to-turn unroll on a lateral flick)
                    // and a body-frozen axis does not co-rotate with the
                    // world-fixed line of travel — the perpendicular error
                    // silently accrued dlen*w_z*dt per tick (measured yaw
                    // 30@140: perp 0.14 -> 0.77 deg over one carry; the
                    // ball passed BESIDE the circle and the crossing was
                    // the documented PHANTOM). Transport u by the measured
                    // roll: a world-fixed ray's transverse components
                    // rotate at -w_z about the nose (d(bx)/dt = w_z*by,
                    // d(by)/dt = -w_z*bx — exact for the transverse
                    // plane), so u co-rotates by theta = w_z*dt each tick.
                    // Pure-axis arrivals fly w_z ~ 0: theta ~ 0 and the
                    // legacy frozen-axis behavior is reproduced to fp.
                    {
                        const double th = s.angular_vel.z * dt;
                        const double c = std::cos(th), sn = std::sin(th);
                        const double rux = ns.cap_ux * c + ns.cap_uy * sn;
                        const double ruy = -ns.cap_ux * sn + ns.cap_uy * c;
                        ns.cap_ux = rux;
                        ns.cap_uy = ruy;
                    }
                    const double s_u =
                        demand.x * ns.cap_ux + demand.y * ns.cap_uy;
                    // ERROR-space closing rate along u (yaw-exactness: the
                    // rate the error geometry actually flies — see wny_ev).
                    const double w_u = wnx * ns.cap_ux + wny_ev * ns.cap_uy;
                    const double dlen =
                        std::sqrt(demand.x * demand.x + demand.y * demand.y);
                    // WALL-REACHABILITY re-check through CARRY (P1-2):
                    // V/altitude drift moves the arrestable rate; the clamp
                    // is monotone-down (never re-raises a clamped hold).
                    {
                        const double a_p = sim::ang_accel_max_derived(
                            ap.c_pitch, ap.I_pitch, e.speed, s.position, env, ap);
                        const double a_y = sim::ang_accel_max_derived(
                            ap.c_yaw, ap.I_yaw, e.speed, s.position, env, ap);
                        const double alpha_u =
                            std::min(a_p / std::max(std::abs(ns.cap_ux), 1e-9),
                                     a_y / std::max(std::abs(ns.cap_uy), 1e-9));
                        // S-truedepth: the event's OWN captured apex target,
                        // never recomputed from the live rate here.
                        const double w_wall =
                            std::sqrt(2.0 * alpha_u * ns.cap_rim_t);
                        ns.cap_w_hold = std::min(ns.cap_w_hold, w_wall);
                    }
                    // Crossing latch FIRST: the same-tick legs below key on
                    // it (the sign flip separates "approaching" from
                    // "past"), and the hand-back would false-fire
                    // post-crossing where the event deliberately drives
                    // AWAY from the aim.
                    if (!ns.cap_crossed && s_u <= 0.0) {
                        ns.cap_crossed = true;
                        // Apex allowance at the crossing: how far past the
                        // aim the carried rate can honestly run — rim +
                        // w^2/(2*alpha_u), alpha_u the trackable
                        // full-authority ceiling along u (min over axes of
                        // alpha_axis/|u_axis|, the RETURN-ceiling
                        // composition; H1: the same derived call, never a
                        // re-derivation). The yank exits compare |demand|
                        // against break_frac x this — a parked aim
                        // physically cannot trip them, a yanked one does.
                        const double a_pitch = sim::ang_accel_max_derived(
                            ap.c_pitch, ap.I_pitch, e.speed, s.position, env, ap);
                        const double a_yaw = sim::ang_accel_max_derived(
                            ap.c_yaw, ap.I_yaw, e.speed, s.position, env, ap);
                        const double alpha_u = std::min(
                            a_pitch / std::max(std::abs(ns.cap_ux), 1e-9),
                            a_yaw / std::max(std::abs(ns.cap_uy), 1e-9));
                        // S-truedepth: the event's own captured apex target
                        // (never the wall unconditionally).
                        ns.cap_d_allow =
                            ns.cap_rim_t + w_u * w_u / (2.0 * alpha_u);
                    }
                    if (!ns.cap_crossed) {
                        // Pre-crossing exits — both mean "this is no longer
                        // the arrival we engaged on"; both hand back THIS
                        // tick (the emission below is gated on the
                        // post-transition state — the MB-right exit-leg
                        // lesson). Re-flick family: NO refractory (the next
                        // arrival is new and takes its own fresh rebound).
                        if (dlen > cp.capture_handback_frac * ns.cap_err0) {
                            // HAND-BACK (re-flick): the error GREW past its
                            // engage value — the pilot re-aimed; chase now,
                            // re-engage at the new knee, fresh full rebound
                            // (Chad's ruling: "abandon the bounce, chase
                            // instantly"). ERROR-domain on purpose: a
                            // rate-domain compare saturates against w_max
                            // at every knee-engage (the live law is capped
                            // AT w_max while the stored engage demand sits
                            // at ~engage_frac*w_max) and the re-flick
                            // contract silently dies (red-team R2-F9).
                            ns.capture = CaptureState::IDLE;
                        } else if (w_u <= 0.0) {
                            // APEX-DEATH (v3 POOL BALL rewrite): the net
                            // closing rate died before the crossing
                            // (protection-capped carry at low V, the aim
                            // accelerating away, or the banked LATERAL
                            // arrival's bank/lean standoff creep). Two
                            // consecutive ticks so one noise sample cannot
                            // kill a live carry. Chad's universality ruling
                            // REJECTED the old exit-to-legacy regroup ("one
                            // attempt then regroup" is dead): the attempt
                            // now transitions to RETURN — the ACTIVE
                            // full-authority direct capture of whatever
                            // error remains (the same terminal law every
                            // arrival gets), never a hand-back to the taper
                            // creep. d_allow seeded from the measured error
                            // so the yank exit cannot kill the capture it
                            // just started.
                            if (++ns.cap_stall_ticks >= 2) {
                                if (dlen > 1e-9) {
                                    ns.capture = CaptureState::RETURN;
                                    ns.cap_ux = -demand.x / dlen;
                                    ns.cap_uy = -demand.y / dlen;
                                    ns.cap_crossed = true;
                                    ns.cap_stall_ticks = 0;
                                    ns.cap_inbound = true;  // no glance leg
                                    // S-truedepth: the event's own captured
                                    // apex target (set at ENGAGE).
                                    ns.cap_d_allow =
                                        std::max(ns.cap_rim_t, dlen);
                                } else {
                                    // Degenerate: already at center —
                                    // ordinary completion.
                                    ns.capture = CaptureState::IDLE;
                                    ns.cap_refractory = true;
                                    ns.cap_err0 = std::min(err, ns.cap_err0);
                                }
                            }
                        } else {
                            ns.cap_stall_ticks = 0;
                        }
                    } else {
                        if (dlen > cp.capture_break_frac * ns.cap_d_allow) {
                            // ABANDON (post-crossing): the error left the
                            // event's allowance. WHO moved is decided by
                            // the error-GROWTH discriminator (the
                            // hand-back's own signal): grown past
                            // handback_frac x the engage err = the PILOT
                            // re-flicked — no latch, chase now, the new
                            // arrival bounces fresh (Chad's ruling). NOT
                            // grown = the arrival's geometry CURVED out of
                            // the event's frame (a banked approach's
                            // rotating demand direction fires PHANTOM
                            // crossings at near-zero carried rate, so
                            // d_allow ~ rim while the error is still
                            // multi-degree — the measured lateral-arrival
                            // path). v3 POOL BALL: the old regroup-to-
                            // legacy latch here was the REJECTED "one
                            // attempt then regroup" — the arrival now
                            // DIRECT-SEEKS instead: RETURN captures the
                            // remaining error with the active full-
                            // authority sqrt drive + dead-blow (the same
                            // universal terminal law; RETURN's per-tick u
                            // refresh absorbs the rotating frame that
                            // broke CARRY's fixed axis).
                            const bool regrew =
                                dlen > cp.capture_handback_frac * ns.cap_err0;
                            if (regrew) {
                                ns.capture = CaptureState::IDLE;
                            } else if (dlen > 1e-9) {
                                ns.capture = CaptureState::RETURN;
                                ns.cap_ux = -demand.x / dlen;
                                ns.cap_uy = -demand.y / dlen;
                                ns.cap_stall_ticks = 0;
                                ns.cap_inbound = true;  // no glance leg
                                ns.cap_d_allow = std::max(ns.cap_d_allow, dlen);
                            } else {
                                ns.capture = CaptureState::IDLE;
                                ns.cap_refractory = true;
                                ns.cap_err0 = std::min(err, ns.cap_err0);
                            }
                        } else {
                            // PREDICTIVE BRAKE SURFACE (v3 POOL BALL, the
                            // core fix — Chad's Q2 "dead ON the inside
                            // wall" + "predictably the same every time").
                            // The old rim DETECT (-s_u >= rim) fired the
                            // brake AT the rim, so the apex = rim +
                            // stopping distance (variable with V/angle/
                            // deflection = the bug). The surface fires
                            // EARLY by exactly the plan: remaining distance
                            // to the rim <= the stopping distance
                            // w^2/(2*alpha_u) at the trackable
                            // full-authority decel (min over axes — the
                            // RETURN-ceiling composition, same derived
                            // call, H1) + the servo's reaction distance
                            // w*tau_u (the S-dampff arrest lesson: the
                            // known plant response is part of the plan;
                            // tau composed along u by projection like the
                            // arrest surface). RETURN's opposing demand
                            // saturates the inner loop and does the actual
                            // braking; the stop lands ON the rim because
                            // the transition led it by the stopping
                            // distance. Recomputed live every tick —
                            // self-correcting across V/deflection/angle.
                            // w_u <= 0 fallback: the rate died short of
                            // the rim — RETURN snaps home from wherever
                            // (never a legacy handback, Chad's ruling).
                            // Lead-term composition (MEASURED, not the
                            // naive servo pole): the brake is
                            // authority-SATURATED (RETURN's opposing
                            // demand + the damping ff pin the inner loop
                            // at the clamp), so the reaction distance is
                            // NOT w*tau_servo (I/(K_w+damp*q) ~ 43 ms
                            // over-led by 2x the whole rim — measured rim
                            // 0.46-0.92 short) — the stop IS the
                            // kinematic w^2/(2*alpha_u) plus the HALF-TICK
                            // discretization lead 0.5*w*dt (the surface is
                            // sampled once per tick; the fire lands 0..1
                            // tick late, half a tick in expectation).
                            const double a_p = sim::ang_accel_max_derived(
                                ap.c_pitch, ap.I_pitch, e.speed, s.position, env, ap);
                            const double a_y = sim::ang_accel_max_derived(
                                ap.c_yaw, ap.I_yaw, e.speed, s.position, env, ap);
                            const double alpha_u = std::min(
                                a_p / std::max(std::abs(ns.cap_ux), 1e-9),
                                a_y / std::max(std::abs(ns.cap_uy), 1e-9));
                            // Stopping distance at BARE alpha_u — the
                            // damping-assist credit is deliberately GONE
                            // (yaw-exactness): crediting -damp*q*w at the
                            // full carried rate shrinks the surface by ~15%
                            // and the fire lands one tick late at 25-30 dps
                            // (per-tick travel 0.23 deg vs the credit's
                            // 0.11 deg margin) — the rim-targeted alpha_req
                            // law below can stretch an EARLY fire to land
                            // ON the wall (it under-brakes by design) but
                            // can never un-overrun a late one (measured
                            // yaw 30@140: late fire -> apex 1.27 rim).
                            // Conservative-early + closed-loop shaping =
                            // dead on from either side; the plant's damping
                            // still physically assists the brake.
                            // S-truedepth: the event's own captured apex
                            // target, not the wall unconditionally.
                            const double rim_target = ns.cap_rim_t;
                            if (w_u <= 0.0 || rim_target - (-s_u) <=
                                                  w_u * w_u / (2.0 * alpha_u) +
                                                      0.5 * w_u * dt) {
                                ns.capture = CaptureState::RETURN;
                                ns.cap_stall_ticks = 0;
                                // Floor the allowance on the MEASURED
                                // distance (the formula's alpha_max is
                                // optimistic wherever the achieved decel
                                // was demand-capped) — un-floored, the
                                // yank exit would kill its own event at
                                // its own apex (red-team R2-F6).
                                ns.cap_d_allow = std::max(ns.cap_d_allow, dlen);
                            }
                        }
                    }
                }
                if (ns.capture == CaptureState::RETURN) {
                    // Live-center chase: the crossed-anyway sign test runs
                    // against LAST tick's axis (pre-refresh u — with a
                    // refreshed axis the sign can never flip), then u
                    // refreshes to -demand-hat so the demand and the arrest
                    // reference the aim WHERE IT IS. On a parked planar aim
                    // the refresh is an identity — the old CARRY->RETURN
                    // edge re-capture (red-team P2-1, the off-plane fix) is
                    // simply this refresh's first iteration.
                    const double s_prev =
                        demand.x * ns.cap_ux + demand.y * ns.cap_uy;
                    const double rlen =
                        std::sqrt(demand.x * demand.x + demand.y * demand.y);
                    // YANK is tested FIRST (diff red-team P0-1, mirroring
                    // CARRY's order): a CROSS-NOSE re-flick flips the demand
                    // sign, so the s_prev >= 0 completion leg would classify
                    // it as "crossed anyway" and latch the refractory with
                    // cap_err0 = the flick size — the clear threshold then
                    // sits at 1.1x the yank and silently EATS the yanked
                    // aim's own arrival bounce (both of Chad's rulings
                    // violated at once). Yank-first makes the opposite-side
                    // re-flick take the no-latch exit like every other
                    // re-flick.
                    if (rlen > cp.capture_break_frac * ns.cap_d_allow) {
                        // YANK (mid-drop): the error left the event's apex
                        // allowance. Same error-growth discriminator as the
                        // CARRY abandon: grown past handback_frac x the
                        // engage err = a pilot re-flick — no latch, fresh
                        // bounce at the new arrival; not grown = the
                        // arrival curved out of the event's frame (phantom
                        // crossing on a banked approach) — completion-
                        // family regroup, latch at the exit err. A parked
                        // PLANAR aim cannot reach here at all (d_rem <=
                        // d_allow by physics + the measured-apex floor).
                        const bool regrew =
                            rlen > cp.capture_handback_frac * ns.cap_err0;
                        ns.capture = CaptureState::IDLE;
                        if (!regrew) {
                            // Regime latch at the APPROACH scale (see the
                            // CARRY abandon leg).
                            ns.cap_refractory = true;
                            ns.cap_err0 = std::max(err, ns.cap_err0);
                        }
                    } else {
                        const bool crossed = s_prev >= 0.0;
                        if (!ns.cap_inbound) {
                            // OUTBOUND (the glance leg): KEEP the carried
                            // direction-of-travel axis, co-rotated with the
                            // measured roll exactly as CARRY transports it
                            // (yaw-exactness) — the -demand-hat refresh
                            // re-aims the rim geometry at the RESIDUAL
                            // error, which on a curved banked arrival sits
                            // ~40 deg off the line of travel at the
                            // crossing (measured 30@140), and the glance
                            // then brakes toward the wrong wall. Chad's
                            // spec point: the glance lands "at the point
                            // opposite the DIRECTION OF TRAVEL". On a
                            // straight planar arrival the residual error
                            // IS along the travel line, so this is the
                            // same axis to fp there.
                            const double th = s.angular_vel.z * dt;
                            const double c = std::cos(th), sn = std::sin(th);
                            const double rux = ns.cap_ux * c + ns.cap_uy * sn;
                            const double ruy = -ns.cap_ux * sn + ns.cap_uy * c;
                            ns.cap_ux = rux;
                            ns.cap_uy = ruy;
                        } else if (rlen > 1e-9) {
                            ns.cap_ux = -demand.x / rlen;
                            ns.cap_uy = -demand.y / rlen;
                        }
                        const double w_u = wnx * ns.cap_ux + wny_ev * ns.cap_uy;
                        // v3 POOL BALL terminal (SUPERSEDES the v2
                        // free-coast tau-arrest): the v2 release at
                        // rlen <= |w|*tau assumed a clean exponential
                        // coast to center, but the coast is contaminated
                        // both ways — pitch carried PAST center (the
                        // post-release taper demand + residual integrator
                        // kept driving: measured w_exit 3.8 deg/s, over2
                        // 0.16 deg), yaw fell SHORT and creeped (the
                        // coordination demand bleeds the coast: measured
                        // release at ~1 deg, stall at 0.3 deg, capture
                        // 6+ s). The inbound is now braked by the SAME
                        // self-correcting servo-fed alpha_req law that
                        // lands the outbound glance ON the wall (emission
                        // block below), so RETURN keeps ownership INTO
                        // center and completes: (a) at the crossing
                        // (s_prev flip above, rate ~dead by construction),
                        // (b) inside the deadzone circle with a coast that
                        // cannot leave it (-w_u*tau_u <= deadzone_lo — the
                        // dead-blow surface reborn at the circle's own
                        // scale; tau composed along u by projection, q_eff
                        // the SAME aero.h composition — H1), or (c) the
                        // STALL guard: the closing rate died at the
                        // standoff park (|w_u| < w_eps, two consecutive
                        // ticks — one noise sample never completes a live
                        // return; w_u > 0 excluded: the wall-apex hover is
                        // the outbound leg's own regime, not a stall).
                        // Without (c) a yaw-magnet standoff park above
                        // deadzone_lo would wedge the machine in RETURN
                        // forever.
                        const double q_arr = sim::q_eff(
                            sim::q_dyn(sim::rho_at(s.position, env, ap), e.speed), ap);
                        // The generalized DEAD-BLOW surface, in the FULL
                        // pointing-plane VECTORS (the u-projection
                        // under-reads near center: the demand direction
                        // rotates toward lateral while the pitch rate
                        // still carries the nose — a projected release
                        // freed a hidden 3.6 deg/s, measured +0.12 deg
                        // blow-past on the 90-deg row). The servo's free
                        // coast displaces the error by exactly
                        // (wnx*tau_p, wny*tau_y) (per-axis poles — the v2
                        // arrest model, vectorized), so the PREDICTED
                        // LANDING is demand - coast. Complete when that
                        // landing sits inside the PARK BAND (deadzone_hi,
                        // the re-arm circle — the deadzone/trim regime
                        // owns the inside from there), gated to releases
                        // INSIDE the reticle circle, with the coast
                        // ITSELF bounded to the band scale (2x the park
                        // radius): the free-decay model carries
                        // residual-integrator/taper/coordination
                        // contamination proportional to the coast length,
                        // so a release is only trusted when even a ~50%
                        // model error still lands in band. A rate-window
                        // completion (err AND |w| small) never fires
                        // instead: the brake's one-tick saturated jerk
                        // ~ alpha*dt = 6.7 deg/s swings the rate through
                        // any sub-deg/s window (measured 5-tick limit
                        // cycle at 0.01-0.05 deg, 20 reversal flips).
                        const double tau_p =
                            ap.I_pitch / (cp.K_w_pitch + ap.damp_pitch * q_arr);
                        const double tau_y =
                            ap.I_yaw / (cp.K_w_yaw + ap.damp_yaw * q_arr);
                        // Coast from the ERROR-space rates (the physical
                        // displacement the free coast carries — the netted
                        // wny under-read it by the coordination bleed).
                        const double coast_x = wnx * tau_p;
                        const double coast_y = wny_ev * tau_y;
                        const double coast_len =
                            std::sqrt(coast_x * coast_x + coast_y * coast_y);
                        const double pred_x = demand.x - coast_x;
                        const double pred_y = demand.y - coast_y;
                        const double pred_len =
                            std::sqrt(pred_x * pred_x + pred_y * pred_y);
                        const bool coast_ok = coast_len <= 2.0 * cp.deadzone_hi;
                        const bool dead_inside = rlen <= cp.capture_circle &&
                                                 pred_len <= cp.deadzone_hi &&
                                                 coast_ok;
                        bool stalled = false;
                        if (ns.cap_inbound
                                ? std::sqrt(wnx * wnx + wny_ev * wny_ev) <
                                      cp.capture_w_eps
                                : (w_u <= 0.0 && -w_u < cp.capture_w_eps)) {
                            // Symmetric FULL-rate stall once inbound (a
                            // brake-overshoot reversal near center is a
                            // dying rate, not a new travel); one-sided
                            // u-projected while the glance leg may still
                            // run — the wall-apex hover carries w_u -> 0+
                            // and must not complete AT the rim.
                            stalled = ++ns.cap_stall_ticks >= 2;
                        } else {
                            ns.cap_stall_ticks = 0;
                        }
                        // A CROSSING completes only if GENTLE (same coast
                        // bound): near center in 2D the demand VECTOR
                        // rotates and flips s_prev while the rate is
                        // still hot (measured: 90-deg row exits at
                        // 3.6 deg/s, +0.12 blow-past; yaw at 3.8, +0.16)
                        // — a hot crossing keeps RETURN ownership and the
                        // refreshed-u laws pull it back for a gentle
                        // release ticks later.
                        const bool crossed_dead = crossed && coast_ok;
                        if (dead_inside || stalled || crossed_dead) {
                            // COMPLETION (dead at/near center): refractory
                            // ON, exit err stored as the clear scale,
                            // bounded by the event's own allowance (P0-1
                            // belt-and-braces).
                            ns.capture = CaptureState::IDLE;
                            ns.cap_refractory = true;
                            ns.cap_err0 = std::min(
                                err, cp.capture_break_frac * ns.cap_d_allow);
                        }
                    }
                }
                // Demand ownership (v3 POOL BALL). CARRY: the carry-scaled
                // incoming NET rate, held — no taper, no braking cap —
                // full speed through the crossing (Chad's Q1) until the
                // predictive brake surface above fires. RETURN, three
                // laws: OUTBOUND (pre-wall) — the rim-targeted servo-fed
                // constant-decel brake that lands the apex ON the wall;
                // INBOUND ceiling — the return rides return_w (400 deg/s =
                // effectively uncapped, Q4) bounded by the plant-physics
                // ceiling sqrt(2*alpha*d_remaining) (alpha = the FULL
                // derived per-axis authority, sim::ang_accel_max_derived
                // at the live V + altitude, NOT k_b-scaled — braking is
                // physics; same derived call as the braking margins, never
                // a re-derivation — H1); INBOUND brake — the mirrored
                // servo-fed alpha_req law that kills rate and distance
                // together at center (the dead-blow's delivery; the
                // predicted-landing surface in the exit block is the
                // release). NO linear floor under the sqrt (rate ~ d IS
                // the taper creep the event kills). Along the captured
                // axis u each body axis must supply alpha*|u_axis|, so the
                // trackable ceiling is min over axes of
                // alpha_axis/|u_axis| (a near-zero component constrains
                // nothing — the 1e-9 floor sends its quotient to ~inf and
                // the min ignores it). All demands add the FULL aim-rate
                // frame-carry (the moving-frame transport term, structural
                // like the curvature ff — NOT the aim_ff lead dial):
                // w_hold was captured net of the aim rate, so the rebound
                // plays out relative to the MOVING crosshair; on a parked
                // aim aim_rate ~ 0 and this is the plain replace tree. All
                // pass through the AoA/G clamp sequence below and the
                // shared yaw_max ceiling here (physics wall stands).
                if (ns.capture == CaptureState::CARRY) {
                    // Yaw-exactness compensation (P1-1): the event demands
                    // the ERROR-space rate, so the emission subtracts
                    // yaw_coord INSIDE the clamp (the outer add + inner
                    // subtract cancel unclamped — the error flies the
                    // event's w; clamped, the TOTAL still respects yaw_max
                    // exactly as legacy). Without it the delivered
                    // coordination bled the outbound carry (yaw rim
                    // 0.29-0.44 measured).
                    // PERP CHANNEL (through-the-middle): the event owns
                    // only the ALONG-TRACK rate; the legacy pointed
                    // demand's perpendicular projection keeps riding, so
                    // the transverse error stays nulled and the ball
                    // crosses THROUGH the middle (Chad's spec). Un-nulled,
                    // a banked arrival's compensation-lag drift accrued
                    // ~0.4 deg of perp by the crossing — the ball passed
                    // beside the center and the apex |d| read the perp in
                    // quadrature (rim_e 1.2-1.4). Straight arrivals carry
                    // Lperp ~ 0 (the law is radial): pure-axis rows are
                    // untouched to measurement precision.
                    // ... and ARREST the transverse rate (the ball rolls
                    // STRAIGHT): a banked arrival hands the event a ~6 dps
                    // perpendicular coast (the carve's turning rate at the
                    // engage handoff), which the servo's own pole lets run
                    // ~0.45 deg before the position pull turns it — emit
                    // the measured perp rate NEGATED so the inner loop
                    // kills the coast at ~2x the pole (rate-cancel through
                    // the rate servo — measured feedback, not D-on-error;
                    // straight arrivals carry wperp ~ 0 and are untouched).
                    const double w_u_c = wnx * ns.cap_ux + wny_ev * ns.cap_uy;
                    const double wpx = wnx - w_u_c * ns.cap_ux;
                    const double wpy = wny_ev - w_u_c * ns.cap_uy;
                    const double lu =
                        pitch * ns.cap_ux + (yaw - yaw_coord) * ns.cap_uy;
                    const double lpx = pitch - lu * ns.cap_ux;
                    const double lpy = (yaw - yaw_coord) - lu * ns.cap_uy;
                    pitch =
                        ns.cap_w_hold * ns.cap_ux + lpx - wpx + aim_rate_body.x;
                    yaw = yaw_coord +
                          std::clamp(ns.cap_w_hold * ns.cap_uy + lpy - wpy +
                                         aim_rate_body.y - yaw_coord,
                                     -cp.yaw_max, cp.yaw_max);
                } else if (ns.capture == CaptureState::RETURN) {
                    const double s_u =
                        demand.x * ns.cap_ux + demand.y * ns.cap_uy;
                    const double d_rem = std::max(0.0, -s_u);
                    const double a_pitch = sim::ang_accel_max_derived(
                        ap.c_pitch, ap.I_pitch, e.speed, s.position, env, ap);
                    const double a_yaw = sim::ang_accel_max_derived(
                        ap.c_yaw, ap.I_yaw, e.speed, s.position, env, ap);
                    const double alpha_u =
                        std::min(a_pitch / std::max(std::abs(ns.cap_ux), 1e-9),
                                 a_yaw / std::max(std::abs(ns.cap_uy), 1e-9));
                    const double w_u_em = wnx * ns.cap_ux + wny_ev * ns.cap_uy;
                    // S-truedepth: the event's own captured apex target.
                    const double rim_t = ns.cap_rim_t;
                    const double q_arr = sim::q_eff(
                        sim::q_dyn(sim::rho_at(s.position, env, ap), e.speed), ap);
                    const double tau_u =
                        ns.cap_ux * ns.cap_ux * ap.I_pitch /
                            (cp.K_w_pitch + ap.damp_pitch * q_arr) +
                        ns.cap_uy * ns.cap_uy * ap.I_yaw /
                            (cp.K_w_yaw + ap.damp_yaw * q_arr);
                    // The glance is once per event: latch the inbound turn
                    // the first tick the net outbound rate is DEAD (below
                    // the same w_eps floor the engage trigger trusts — the
                    // rim-targeted alpha_req brake approaches zero rate
                    // asymptotically, so a strict sign-flip wait let the
                    // nose HOVER at the wall ~15 ticks while the perp
                    // drifted, measured rim_e 1.79; the apex is the rate's
                    // death, and the snap home is immediate — Chad's "like
                    // a dead blow hammer"). A later brake-overshoot
                    // reversal near center is pulled back by the inbound
                    // law, never re-launched at the wall.
                    if (w_u_em < cp.capture_w_eps) ns.cap_inbound = true;
                    double w_dem;  // signed demand along u
                    if (!ns.cap_inbound && w_u_em > 0.0 && d_rem < rim_t) {
                        // OUTBOUND glance leg (v3 POOL BALL, Chad's Q2 "dead
                        // ON the inside wall" + "same every time"): a
                        // closed-loop constant-deceleration brake TARGETED
                        // AT THE RIM — alpha_req = w^2/(2*(rim - d)) is the
                        // exact decel that stops ON the wall, recomputed
                        // live every tick (position feedback: over-braking
                        // drops alpha_req and eases off, under-braking
                        // raises it — self-correcting across V/deflection/
                        // integrator state, sub-tick precise where the
                        // fire-tick alone quantizes the apex by w*dt ~ 0.33
                        // rim). Fed through the KNOWN servo response (the
                        // S-dampff/arrest lesson): with the damping ff the
                        // closed inner loop tracks w_des at the pole
                        // (K_w + damp*q_eff)/I, so demanding
                        // w - alpha_req*tau_u achieves decel alpha_req.
                        // Floored at 0 (the wall demand is a dead stop,
                        // never a reversal — the inbound law below owns the
                        // way home once the rate flips). If alpha_req
                        // exceeds the plant's authority the demand
                        // saturates downstream and the apex honestly
                        // overruns (the physics wall).
                        // ZOH centering: the demand is computed from
                        // tick-START state but applies across the tick —
                        // the nose travels ~0.5*w*dt before the "current"
                        // plan is half-executed, a measured uniform +0.18
                        // rim bias. Lead the target by that half-tick
                        // travel (state-derived, scales with the live
                        // rate).
                        const double alpha_req =
                            w_u_em * w_u_em /
                            (2.0 *
                             std::max(rim_t - d_rem - 0.5 * w_u_em * dt, 1e-6));
                        // The demand may go NEGATIVE (a real opposing
                        // brake — the gap w - w_des is what makes the
                        // servo decelerate at alpha_req), FLOORED at the
                        // inbound law's own ceiling at this position
                        // (min(return_w, sqrt(2*alpha*d)) — the outbound
                        // brake may never demand more inbound rate than
                        // the return itself would: an unfloored alpha_req
                        // spike at the wall wound the integrator and blew
                        // the recross hot — measured drift 0.13 deg), and
                        // bounded by the AoA/G clamps downstream
                        // (protection never bypassed).
                        w_dem = std::max(
                            w_u_em - alpha_req * tau_u,
                            -std::min(cp.capture_return_w,
                                      std::sqrt(2.0 * alpha_u * d_rem)));
                    } else {
                        // INBOUND: two phases, both time-optimal. CEILING —
                        // rate rides the return_w cap (400 deg/s =
                        // effectively uncapped, Chad's Q4: the sqrt IS the
                        // law everywhere) bounded by the plant-physics
                        // ceiling sqrt(2*alpha*d_remaining). BRAKE — once
                        // the remaining distance is inside the live
                        // stopping surface, the SAME self-correcting
                        // servo-fed constant-decel law that lands the
                        // glance ON the wall now lands the return ON
                        // center (alpha_req = w^2/(2*d), fed through the
                        // servo pole as w + alpha_req*tau_u): the rate and
                        // the distance die TOGETHER — the dead-blow with
                        // no free-coast contamination (the v2 release
                        // coast measured hot on pitch, short on yaw).
                        // Never demands outbound sign (min 0) and never
                        // more inbound than the ceiling.
                        const double ceil_w =
                            std::min(cp.capture_return_w,
                                     std::sqrt(2.0 * alpha_u * d_rem));
                        const double aw = -w_u_em;  // inbound closing speed
                        if (aw > 0.0 && d_rem <= aw * aw / (2.0 * alpha_u) +
                                                     0.5 * aw * dt) {
                            // Stopping surface reached. Brake ONLY if the
                            // servo coast would MISS the park band
                            // (aw*tau > 2*deadzone_hi — the SAME coast
                            // bound the dead-blow release trusts, so the
                            // regimes tile exactly: gentle coasts
                            // complete, escaping coasts brake). A coast
                            // that already dies in band needs no opposing
                            // torque — braking a sub-coast-scale residual
                            // sends a_req through its floor into a
                            // +400 deg/s opposing spike (AoA-clamped to
                            // ~38) whose one-tick jerk REVERSES the rate
                            // (the measured extra reversal pair on the
                            // 45-deg rows). The brake demand may cross to
                            // the OPPOSING side (positive along u) — that
                            // gap is what makes the servo decelerate at
                            // alpha_req (the continuum solution follows
                            // w^2 = 2*a_req*d exactly into center);
                            // clamped to the event's own cap both ways,
                            // AoA/G bound it downstream.
                            if (aw * tau_u > 2.0 * cp.deadzone_hi) {
                                // a_req CAPPED at the derived trackable
                                // authority alpha_u: past it the servo
                                // model is a lie (the ZOH endgame covers
                                // ~2 mrad/tick, so a late fire computes
                                // a_req up to 2.4x alpha — the saturated
                                // over-brake then REVERSES the rate:
                                // measured +3 deg/s wiggle = the extra
                                // reversal pair on the 45-deg rows;
                                // capped, the brake rides saturation to
                                // ~zero rate and the dead-blow releases
                                // clean).
                                const double a_req = std::min(
                                    aw * aw /
                                        (2.0 *
                                         std::max(d_rem - 0.5 * aw * dt, 1e-6)),
                                    alpha_u);
                                w_dem =
                                    std::clamp(w_u_em + a_req * tau_u, -ceil_w,
                                               cp.capture_return_w);
                            } else {
                                w_dem = 0.0;
                            }
                        } else if (aw <= 0.0 && d_rem <= cp.deadzone_hi) {
                            // Reversed rate INSIDE the park band: a
                            // brake-overshoot dying at center — demand 0
                            // and let the servo kill it where it sits. The
                            // steep near-center sqrt would RE-ACCELERATE
                            // the residual into a micro-hunt (measured 20
                            // reversal flips, 81 return ticks); the
                            // dead-inside/stall completions then end the
                            // event.
                            w_dem = 0.0;
                        } else {
                            w_dem = -ceil_w;
                        }
                    }
                    // Same yaw_coord compensation as the CARRY emission
                    // (P1-1) — the RETURN laws are error-space too. The
                    // legacy-perp channel rides ONLY on the outbound
                    // glance leg (carried axis): inbound refreshes u to
                    // the live error direction each tick, so the perp is
                    // owned by the refresh and an extra term would fight
                    // it.
                    double rpx = 0.0, rpy = 0.0;
                    if (!ns.cap_inbound) {
                        const double lu =
                            pitch * ns.cap_ux + (yaw - yaw_coord) * ns.cap_uy;
                        rpx =
                            pitch - lu * ns.cap_ux - (wnx - w_u_em * ns.cap_ux);
                        rpy = (yaw - yaw_coord) - lu * ns.cap_uy -
                              (wny_ev - w_u_em * ns.cap_uy);
                    }
                    pitch = w_dem * ns.cap_ux + rpx + aim_rate_body.x;
                    yaw =
                        yaw_coord + std::clamp(w_dem * ns.cap_uy + rpy +
                                                   aim_rate_body.y - yaw_coord,
                                               -cp.yaw_max, cp.yaw_max);
                }
            }

            // AoA protection (SPEC §9.3b): symmetric pushback, then the G
            // floor AND ceiling applied LAST so protection itself can never
            // command beyond the load limits.
            pitch = std::min(pitch, cp.K_aoa * (cp.aoa_max - ns.aoa_filtered));
            pitch =
                std::max(pitch, -cp.K_aoa * (cp.aoa_max_neg + ns.aoa_filtered));
            pitch = std::max(pitch, w_min_pitch);
            pitch = std::min(pitch, w_max_pitch);
        } else {
            // Deadzone OR override pursuit-suspension: pointing term -> 0;
            // coordination stays (computed above the gate); feedforward (added
            // below) bypasses. Latches idle so they re-arm cleanly on resume.
            ns.push_mode = false;
            ns.roll_latch = 0.0;
            ns.elev_latch = 0.0;

            // Wings auto-level at rest (§7 Item-2 Part B): emit the wings-hold
            // roll toward the (above-decayed) held_bank — the deadzone's roll
            // is otherwise 0, so the wings would freeze. Gated on pursuit (an
            // override suspends it) and FADED by wings_level_gate
            // (S7-loop-invert, reverses F2): UPRIGHT the wings level as before;
            // past ~90 deg bank the fade -> 0 so an INVERTED rest STAYS
            // inverted (a paused loop / held Immelmann) instead of
            // auto-righting — the pilot rolls out by aiming, OR (MB-right,
            // below) the slow righting arms after inverted_delay of rest.
            // Pitch trim (deadzone-holds-trim) and coordination are untouched.
            if (ns.pursuit) {
                roll = wings_level_gate *
                       std::clamp(
                           cp.K_phi * roll_hold_demand(ns.held_bank, phi_full),
                           -cp.p_max, cp.p_max);
            }
        }

        // Inverted auto-righting (MB-right, Chad 2026-07-07: "roll over on
        // bank after about 2 s no gross inputs if belly up... slow roll off
        // ailerons"). Scoped amendment of S7-loop-invert: an inverted REST
        // still stays inverted — but only for inverted_delay. ARM: accumulate
        // rest time while pursuing (no override), the mouse near the nose
        // (err < blend_lo — the auto-level band; small adjustments do not
        // reset it), genuinely belly-up (cosPhiTheta < -band, the region
        // where wings_level_gate == 0 by construction so the two mechanisms
        // never overlap), and not pushing. FIRE: latch `righting` after
        // inverted_delay. While latched, the held_bank setpoint decays to 0
        // UN-GATED and the roll follows it CLAMPED to +/-inverted_rate — a
        // uniform slow aileron roll that eases into level (the setpoint
        // outruns the clamped wings, so the demand saturates until nearly
        // upright — same felt shape as the S7-hrz recovery). Pitch/yaw stay
        // on the aim, so it reads as a lazy coordinated roll. DISARM
        // (hysteretic pairs, no chatter): a real mouse deflection (err >
        // blend_hi — pairs with the blend_lo arm), any override (pursuit
        // false), push engagement, or DONE (cos > +band — rolled up through
        // knife-edge; recapture held_bank at the current bank so the normal
        // auto-level takes over with zero demand — no lurch, the AT-13
        // capture discipline). inverted_rate == 0 never arms (knob-off
        // strict-superset arm).
        const bool at_rest = ns.pursuit && err < cp.blend_lo;
        // (!ns.push_mode is defense-in-depth, not a live leg: at_rest forces
        // the push exit the SAME tick in both branches — blend = 0 kills
        // have_bank in the pointing branch, and the deadzone branch forces
        // push_mode = false. Kept so a future push-gate reshape cannot
        // silently arm the timer mid-push. Same for the !ns.righting guard
        // on the else-reset below: inv_rest is never consumed while latched
        // and every exit resets it — kept for state hygiene. MB-right
        // red-team P3-3.)
        if (cp.inverted_rate > 0.0 && at_rest && !ns.push_mode &&
            e.cos_phi_theta < -cp.wings_level_band) {
            ns.inv_rest += dt;
            if (!ns.righting && ns.inv_rest >= cp.inverted_delay) {
                ns.righting = true;
            }
        } else if (!ns.righting) {
            ns.inv_rest = 0.0;
        }
        // CANCEL legs (pilot re-engagement): a real mouse deflection (err >
        // blend_hi), any override (pursuit false), or push engagement. On
        // these the branch-computed roll IS the correct output (the pilot or
        // the pointing owns the wings again), so they disarm BEFORE the
        // apply block. The DONE leg lives INSIDE the apply block below — a
        // disarm-here would emit the branch roll computed against the
        // already-decayed held_bank for one tick (~p_max spike at knife-edge,
        // 4.5 rad/s probed — the MB-right red-team P1-1).
        if (ns.righting && (!ns.pursuit || err > cp.blend_hi || ns.push_mode)) {
            ns.righting = false;
            ns.inv_rest = 0.0;
            ns.held_bank = phi_full;  // fold-safe capture (F2)
        }
        if (ns.righting) {
            if (e.cos_phi_theta > cp.wings_level_band) {
                // DONE: rolled up through knife-edge. Recapture AND emit the
                // recaptured demand (exactly 0) THIS tick — the normal
                // auto-level then eases from zero (the AT-13 no-lurch
                // discipline, made true on the handoff tick itself: P1-1).
                ns.righting = false;
                ns.inv_rest = 0.0;
                ns.held_bank = phi_full;
                roll = 0.0;
            } else {
                ns.held_bank -= cp.auto_level_rate * dt * ns.held_bank;
                // S-righthand: scale the righting AUTHORITY by how long the
                // hand has been off the aim (params.h carries the derivation
                // and Chad's ruling). 0 while he is still flying it, full
                // after right_hand_rest of rest. Continuous -- a smoothstep
                // of a time integral -- so there is no new threshold and a
                // mid-loop pause gets a proportional amount of righting.
                // right_hand_rest == 0 skips the ramp entirely and this is
                // the bit-identical legacy expression.
                const double hand_gate =
                    (cp.right_hand_rest > 0.0)
                        ? smoothstep(0.0, cp.right_hand_rest, ns.hand_rest)
                        : 1.0;
                roll = hand_gate *
                       std::clamp(
                           cp.K_phi * roll_hold_demand(ns.held_bank, phi_full),
                           -cp.inverted_rate, cp.inverted_rate);
                out.telem.roll_right = roll;
                out.telem.hand_gate = hand_gate;
            }
        }

        omega_des = {pitch, yaw, roll};
    }

    // Keyboard override (SPEC §9.5, S7-ovr): overwrite the held axis's
    // omega_des with its clamped max IN-ENVELOPE rate (ovr_ceil/ovr_floor
    // above), eased in over ovr_ramp_time. The inner loop then drives the
    // airframe to that rate through the SAME rate-PI + plant inversion
    // mouse-aim uses — snappy and airframe-leading, but bounded by the wing's
    // AoA/G envelope (no over-stall skid). The aim rides the nose caller-side,
    // so pursuit (suspended above) has ~nothing to point at and release is
    // seamless. Non-held axes reset the ramp so the engage re-fires from 0.
    for (int i = 0; i < 3; ++i) {
        if (!in.override_mask[i]) {
            ns.ovr_ramp[i] = 0.0;
            continue;
        }
        // A held axis carries a direction: the caller maps the key to +/-1
        // (§7). Assert it rather than clamp — a 0 or out-of-range sign is a
        // caller-contract violation (fail loudly, never emit a plausible Input
        // the plant would silently clamp and the HUD would misreport). ramp in
        // [0,1] then bounds the commanded rate to the envelope by construction.
        assert(std::abs(in.override_sign[i]) == 1.0);
        ns.ovr_ramp[i] =
            std::min(internal.ovr_ramp[i] + dt / cp.ovr_ramp_time, 1.0);
        const double rate =
            (in.override_sign[i] > 0.0) ? ovr_ceil[i] : ovr_floor[i];
        omega_des[i] = rate * ns.ovr_ramp[i];
    }

    // Feedforward bypasses the deadzone and the clamps (SPEC §9.3), added to
    // every mode.
    const glm::dvec3 omega_des_total = omega_des + omega_ff_body;

    // ===== Inner loop — rate-PI + plant inversion (SPEC §9.4) =====
    // The division goes through plant_invert — the SAME function AT-18b pins
    // by round-trip — so the live inner-loop inversion is never a fork of the
    // tested one (q applied exactly once, the sim's exact params). Saturation
    // is |Input| == 1 <=> |raw| >= 1, so the emitted (clamped) Input's
    // magnitude is a faithful unsaturated test for the anti-windup rule.
    const glm::dvec3 e_omega = omega_des_total - s.angular_vel;

    const double K_w[3] = {cp.K_w_pitch, cp.K_w_yaw, cp.K_w_roll};
    const double K_wi[3] = {cp.K_wi_pitch, cp.K_wi_yaw, cp.K_wi_roll};
    const double c_ax[3] = {ap.c_pitch, ap.c_yaw, ap.c_roll};
    // S-dampff (SPEC §0/§9.4): damping feedforward — the OTHER half of the
    // plant inversion. plant_invert inverts only the authority denominator;
    // the plant ALSO fights every rotation with -damp*Q*omega, and without
    // this term the integrator must wind up to source that whole torque
    // during a sustained rotation (~0.6-1.4 rad at V=250), then carries the
    // nose ~1 deg PAST the aim at capture, draining at tau = K_w/K_wi = 2.5 s
    // (Chad's "sitting high after a deflection"). Feeding the KNOWN viscous
    // torque forward AT THE DEMAND (omega_des_total, never measured omega —
    // measured would cancel the plant's physical damping) zeroes the
    // integrator's sustained job. Stability: the eo coefficient is
    // damp_ff-independent, so the discrete rate-loop pole and the ZOH
    // ceiling are untouched; q_eff below is the SAME aero.h composition
    // plant_invert uses (an inline re-derivation would be the H1 fork).
    const double damp_ax[3] = {ap.damp_pitch, ap.damp_yaw, ap.damp_roll};
    const double damp_ff[3] = {cp.damp_ff_pitch, cp.damp_ff_yaw,
                               cp.damp_ff_roll};
    // R6: SPATIAL density (position+env) — the damp_ff feedforward q and the
    // plant inversion below MUST see the SAME density the plant applies, or a
    // bubble edge re-arms the S-dampff preload pathology / mis-deflects by
    // 1/u (null env => the identical altitude q, bit-for-bit).
    const double q_eff_val =
        sim::q_eff(sim::q_dyn(sim::rho_at(s.position, env, ap), e.speed), ap);
    double inputs[3];
    for (int i = 0; i < 3; ++i) {
        // Uniform rate-PI + plant inversion (S7-ovr): held and unheld axes run
        // the SAME inversion — a held axis just has omega_des set to its
        // in-envelope override rate above, so keyboard obeys the envelope and
        // gets compression for free (the division goes through plant_invert,
        // the function AT-18b round-trip-pins, so the live inversion is never a
        // fork).
        const double eo = e_omega[i];
        double tau_cmd = K_w[i] * eo + K_wi[i] * internal.integ[i];
        if (damp_ff[i] > 0.0) {
            // Gated ADD (the S-wvane ±0.0 discipline): at damp_ff = 0 the
            // tau_cmd expression tree is the bit-identical legacy one.
            tau_cmd += damp_ff[i] * damp_ax[i] * q_eff_val * omega_des_total[i];
        }
        inputs[i] = plant_invert(tau_cmd, c_ax[i], e.speed, s.position, env, ap);
        if (in.override_mask[i] || s.on_ground) {
            // Held axis: the integrator is FROZEN, not zeroed (ns.integ[i]
            // already == internal.integ[i], untouched here) — trim survives the
            // maneuver and is there when the axis re-enters the cascade on
            // release (zeroing manufactures post-maneuver sag).
            // R4-FLY-5 GROUNDED freeze (review P2): on the ground the plant
            // flies TRUE q (no q_att_floor — dead stick at rest) while this
            // inversion still divides by the floored q_eff, so the commanded
            // torque under-delivers below the ~24 m/s crossover and the
            // integral would wind on any taxi aim motion, releasing as a
            // pitch/yaw twitch mid-takeoff-roll. Freeze (the override
            // pattern), never zero. on_ground is false on every env-null
            // path — goldens bit-identical.
            // v4 merge: the grounded-freeze branch (fields-forge) and the
            // capture unwind-only branch (v4) both survive — a grounded plane
            // is never in a capture event, so the two conditions are disjoint.
        } else if (i != 2 && ns.capture != CaptureState::IDLE) {
            // S-rimshot v3: pointing axes UNWIND-ONLY while the capture
            // event owns the demand. Two failure modes bracket this rule
            // (both measured): free integration lets the event's saturated
            // brake/slam gaps WIND contamination the dead-blow coast then
            // dumps as a hot recross (w_exit 3.8 deg/s, over2 0.16 deg);
            // a full FREEZE preserves the APPROACH's wound-to-cap integral
            // through the whole event (the documented "freeze-only pins
            // the integral through the return swing" blow-through —
            // measured +12 rad/s^2 of phantom pitch-up torquing the brake
            // into a reversal spike). Unwind-only: integrate exactly when
            // the error would drive the stored integral TOWARD zero — the
            // approach wind drains during the brake, the event can never
            // wind NEW contamination past zero, and the trim-scale value
            // is back in the integrator's hands the tick the event
            // completes. Roll (i == 2) is never event-owned and
            // integrates as legacy.
            if (eo * internal.integ[i] < 0.0) {
                ns.integ[i] = std::clamp(internal.integ[i] + eo * dt,
                                         -cp.integ_cap, cp.integ_cap);
            }
        } else if (std::abs(inputs[i]) < 1.0 || (eo * tau_cmd < 0.0)) {
            // Unwinding anti-windup (SPEC §9.4): integrate when unsaturated, OR
            // when the error opposes the command (integration is pulling the
            // output OUT of saturation). Freeze-only would pin the integral at
            // the cap through the return swing — the blow-through it prevents.
            // S-dampff: the sign test DELIBERATELY reads the ff-inclusive
            // tau_cmd (the true emitted torque) — at a saturated ff'd demand a
            // small opposing eo now integrates where legacy froze; that is the
            // de-saturating direction, self-limiting, cap-clamped below.
            // (Diff red-team P3-1: argued benign, more correct than pre-ff.)
            ns.integ[i] = std::clamp(internal.integ[i] + eo * dt, -cp.integ_cap,
                                     cp.integ_cap);
        }
    }

    out.inputs.pitch = static_cast<float>(inputs[0]);
    out.inputs.yaw = static_cast<float>(inputs[1]);
    out.inputs.roll = static_cast<float>(inputs[2]);
    out.inputs.throttle = static_cast<float>(std::clamp(in.throttle, 0.0, 1.0));
    // MB-flaps passthrough (the throttle pattern — the plant owns the slew).
    out.inputs.flap_cmd = static_cast<float>(std::clamp(in.flap_cmd, 0.0, 1.0));
    out.inputs.gear_cmd = static_cast<float>(std::clamp(in.gear_cmd, 0.0, 1.0));
    // R4g wheel brakes: this tail serves cascade AND ballistic AND override —
    // the one other emission site (Fable (c): two sites, both must forward).
    out.inputs.wheel_brake =
        static_cast<float>(std::clamp(in.wheel_brake, 0.0, 1.0));

    out.internal = ns;
    out.telem.e = err;
    out.telem.omega_des = omega_des_total;
    out.telem.regime = ns.regime;
    out.telem.push_mode = ns.push_mode;
    out.telem.ballistic = ns.ballistic;
    out.telem.deadzoned = ns.deadzoned;
    out.telem.pursuit = ns.pursuit;
    out.telem.aoa_filtered = ns.aoa_filtered;
    out.telem.righting = ns.righting;
    out.telem.capture = ns.capture;      // S-rimshot state mirror (v4 rung 2)
    out.telem.blend = blend;             // the regime mix weight (§6.5 pin #2)
    out.telem.held_bank = ns.held_bank;  // post-capture/lean roll-hold setpoint
    // True load factor from lift, logged ALONGSIDE the cosPhiTheta G-proxy
    // (SPEC §16 CQ1) — telemetry, never a gate. The formula is sim/aero.h's
    // single source (H1), shared bit-identically with the HUD.
    out.telem.load_factor = sim::load_factor(
        e.alpha, e.speed, s.position, env, s.flap, ap);
    return out;
}

}  // namespace control
