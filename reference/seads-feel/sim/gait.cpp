#include "sim/gait.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

namespace sim {

namespace {

// ★★★ RED-TEAM FIX (G1b, P1): see `sim/gait.h`'s banner for the WHY. Two
// reads that differ in DIRECTION by more than a hair are two different
// points on the ground -- an ordinary grade, never a glitch -- so this
// accepts them outright and starts fresh history there. Two reads of the
// (near-)identical direction are read together as ONE point sampled twice,
// and THAT is where a temporal glitch could show up; there, the accepted
// radius moves toward the new sample by at most `kMaxRatePerEventM` instead
// of latching on a rejection, so a real discontinuity converges out within a
// few events rather than freezing the foot forever.
constexpr double kSamePointM = 0.05;      // chord distance = "the same point"
constexpr double kMaxRatePerEventM = 0.15;  // the law's own number, now a RATE

double guarded_radius(glm::dvec3& last_dir, double& last_r,
                      const glm::dvec3& dir, double sample_r) {
    if (last_r < 0.0) {
        last_dir = dir;
        last_r = sample_r;
        return sample_r;
    }
    // Chord distance at the last-known radius, small-angle: r*|dir - last|.
    // `dir` and `last_dir` are both unit, so this is accurate to O(theta^3)
    // at any radius this game ever samples at.
    const double chord = last_r * glm::length(dir - last_dir);
    if (chord >= kSamePointM) {
        // A different point on the ground -- e.g. the next plant, a stride
        // away. No temporal-glitch guard applies; a grade is not a glitch.
        last_dir = dir;
        last_r = sample_r;
        return sample_r;
    }
    // The same point, re-read: rate-limit toward it rather than
    // reject-and-hold, so a genuine one-off glitch cannot latch forever.
    const double delta = sample_r - last_r;
    const double step = std::clamp(delta, -kMaxRatePerEventM, kMaxRatePerEventM);
    last_r += step;
    last_dir = dir;
    return last_r;
}

// A stable local yaw, published on `GaitFoot` for a later rung (G5) and not
// consumed by G1. `east`/`north` are an arbitrary but CONSISTENT tangent
// basis at `up` -- consistent is all a stored angle needs to be, since
// nothing here compares it against a world compass.
double yaw_of(const glm::dvec3& fwd, const glm::dvec3& up) {
    glm::dvec3 east = glm::cross(glm::dvec3(0.0, 1.0, 0.0), up);
    if (glm::length(east) < 1.0e-6) east = glm::cross(glm::dvec3(1.0, 0.0, 0.0), up);
    east = glm::normalize(east);
    const glm::dvec3 north = glm::cross(up, east);
    return std::atan2(glm::dot(fwd, east), glm::dot(fwd, north));
}

// A literal, not `M_PI`: `sim/walker.cpp`'s own Hermite swing uses the same
// literal for the identical portability reason (MSVC does not define M_PI
// without `_USE_MATH_DEFINES`) -- one idiom, not two.
constexpr double kPi = 3.14159265358979323846;

// ★★★ GAIT LADDER G2. Where, in PHASE, the low point (bob) / the
// stance-ward extreme (sway, roll) sits, relative to a foot's OWN contact
// instant (phase 0 in that foot's own reference -- `gait_vertical_bob_m` and
// its two neighbours are all called with the SAME raw `w.gait_phase`, which
// is foot index 1 / "right"'s own reference per the header's crossover
// banner; the period-0.5 and period-1.0 shapes below make both feet's
// contacts land correctly off that one phase without a second call).
//
// Walk: the measured mid-stance, `ds/2` into the foot's own stance window
// (Nilsson & Thorstensson's own row is what "mid-stance" means here -- the
// same one `walker_stance_frac` reads). Run: a fixed 8% of the cycle lag
// past contact (plan §4 G2: "low point lagging ground contact ~8% of the
// cycle"). Blended continuously by `gait_run_blend_t`, never a threshold.
double contact_offset(double ds, double run_t) {
    constexpr double kRunLagFrac = 0.08;
    return (1.0 - run_t) * (0.5 * ds) + run_t * kRunLagFrac;
}

}  // namespace

double gait_spring_step(double x, double& v, double target, double omega_rps,
                        double dt_s) {
    if (!(dt_s > 0.0) || !(omega_rps > 0.0)) return x;
    // Critically damped: acceleration = omega^2*(target - x) - 2*omega*v.
    // Semi-implicit Euler (velocity first, then position with the NEW
    // velocity), the same order every integrator in this repo uses
    // (sim/walker.cpp's fall, sim/sled.cpp's own steps). Critical damping is
    // the exact boundary where a spring reaches its target in minimum time
    // with no ring -- a foot planting is a STEP in demand, and this is the
    // law that keeps that step from snapping the pelvis or bouncing it.
    const double acc =
        omega_rps * omega_rps * (target - x) - 2.0 * omega_rps * v;
    v += acc * dt_s;
    x += v * dt_s;
    return x;
}

double gait_run_blend_t(double stance_frac) {
    // The measured ends `walker_stance_frac` itself reads out of Nilsson &
    // Thorstensson's row: 0.68 at the walk floor (0.6 m/s), 0.38 at the jog
    // ceiling (3.5 m/s, and by that curve's own saturation, everything
    // faster too -- see walker.h's speed-cap banner). Reading the SAME blend
    // rather than inventing a second one is the whole of L-DEPTH here.
    constexpr double kWalkDs = 0.68, kRunDs = 0.38;
    const double t = (kWalkDs - stance_frac) / (kWalkDs - kRunDs);
    return std::clamp(t, 0.0, 1.0);
}

double gait_vertical_bob_m(double phase01, double stance_frac,
                          double bob_walk_m, double bob_run_m) {
    const double ds = std::clamp(stance_frac, 0.05, 0.95);
    const double t = gait_run_blend_t(stance_frac);
    const double amp = bob_walk_m + (bob_run_m - bob_walk_m) * t;
    const double off = contact_offset(ds, t);
    const double phase = phase01 - std::floor(phase01);
    // Period is HALF a stride cycle (a dip at EACH foot's own contact, and
    // both dips look the same) -- `4*pi*phase` has period 0.5 in phase,
    // unlike the sway/roll below which must alternate sign between the two
    // feet and so run at half this frequency.
    return -amp * std::cos(4.0 * kPi * (phase - off));
}

double gait_lateral_sway_m(double phase01, double stance_frac,
                          double sway_walk_m, double sway_run_m) {
    const double ds = std::clamp(stance_frac, 0.05, 0.95);
    const double t = gait_run_blend_t(stance_frac);
    const double amp = sway_walk_m + (sway_run_m - sway_walk_m) * t;
    const double off = contact_offset(ds, t);
    const double phase = phase01 - std::floor(phase01);
    // Period is a FULL stride cycle -- toward left once, toward right once
    // -- `2*pi*phase`. At `phase = off` (foot index 1 / "right"'s own
    // contact-relative instant) this is `-amp*cos(0) = -amp`: negative,
    // i.e. toward `-left`, which is index 1's own `side_sign` -- correct by
    // construction, not by trial.
    return -amp * std::cos(2.0 * kPi * (phase - off));
}

double gait_trendelenburg_roll_rad(double phase01, double stance_frac,
                                   double roll_amp_rad) {
    const double ds = std::clamp(stance_frac, 0.05, 0.95);
    const double t = gait_run_blend_t(stance_frac);
    const double off = contact_offset(ds, t);
    const double phase = phase01 - std::floor(phase01);
    // Same period and offset as the sway, OPPOSITE sign: at the same
    // instant the pelvis sways toward the STANCE foot it tips toward the
    // SWING side. Getting either sign wrong independently is the drunk test.
    return roll_amp_rad * std::cos(2.0 * kPi * (phase - off));
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2i -- THE SHIFT KEY (jump / sprint / dash). See sim/gait.h
// for Chad's words and for why this does not touch `WalkerMode`.
HopState step_hop(const HopState& prev, const HopInputs& in, double dt_s,
                  const HopParams& p) {
    HopState out = prev;
    // The edges are published for exactly one step, so they are cleared FIRST
    // and set only by the branch that causes them (a consumer that runs at a
    // different rate than the sim would otherwise re-fire a landing thump).
    out.launched = false;
    out.landed = false;
    if (!(dt_s > 0.0)) return out;

    // ---- THE CHARGE. Held time accumulates; a release starts a grace clock
    // rather than clearing the arm outright (see `dash_arm_grace_s`: the only
    // way to PRESS the key that arms the dash is to let go of it first, so a
    // strict clear-on-release would make the dash unreachable by construction).
    if (in.shift_down) {
        out.shift_held_s = prev.shift_held_s + dt_s;
        out.shift_off_s = 0.0;
    } else {
        out.shift_off_s = prev.shift_off_s + dt_s;
        if (out.shift_off_s > p.dash_arm_grace_s) {
            out.shift_held_s = 0.0;
            out.dash_armed = false;
        }
    }
    if (out.shift_held_s >= p.dash_charge_s) out.dash_armed = true;

    // ---- THE JUMP. A press launches him only from the ground: a second press
    // in mid-air is not a double jump, it is a key held down by a player who
    // wants the sprint, and it must not re-launch. `jump_carries_dash` is read
    // at the LANDING, which is where Chad put the dash.
    if (in.shift_press && !prev.airborne) {
        out.airborne = true;
        out.height_m = 0.0;
        out.vert_vel_mps = p.jump_speed_mps;
        out.jump_carries_dash = out.dash_armed;
        out.launched = true;
        // ★★★ G2i-b: WHICH KNEE LEADS, decided ONCE, HERE. The lead is the
        // leg that was already SWINGING when he left the ground -- a man
        // jumps off the leg he had his weight on and carries the other one
        // through. `step_gait`'s own crossover is: foot index 1 (right) runs
        // at phase+0.0 and index 0 (left) at phase+0.5, stance while the
        // shifted phase is below the duty factor. Split at the HALF cycle
        // rather than at the live duty factor: this only decides which of two
        // knees comes forward, the duty is not in this struct, and a tenth of
        // a cycle either way is not a wrong answer -- but it IS deterministic,
        // which is what a latched lead has to be.
        const double ph = in.gait_phase01 - std::floor(in.gait_phase01);
        out.lead_side = (ph < 0.5) ? 0 : 1;
    }

    // ---- THE BALLISTIC. Semi-implicit Euler (velocity first), the same
    // integrator shape the kernel uses everywhere else, so the apex height is
    // v0^2/2g to within a step and the flight is symmetric.
    if (out.airborne) {
        out.vert_vel_mps -= p.gravity_mps2 * dt_s;
        out.height_m += out.vert_vel_mps * dt_s;
        if (out.height_m <= 0.0 && !out.launched) {
            out.height_m = 0.0;
            out.vert_vel_mps = 0.0;
            out.airborne = false;
            out.landed = true;
            // ★ "a dash will start ... when landing from it": the burst is
            // spent HERE, and the arm is consumed with it so one charge buys
            // one dash.
            if (out.jump_carries_dash) {
                out.dash_s = p.dash_decay_s;
                out.dash_armed = false;
                out.shift_held_s = 0.0;
            }
            out.jump_carries_dash = false;
        }
    }

    // ---- ★★★ G2i-b: THE TUCK, AND IT IS JUST THE HEIGHT. See the header:
    // one law, no clock, no branch that can pop -- 0 at the push-off, full
    // over the top, back to 0 by the time the boots meet the snow, so the
    // landing re-enters stance out of an already-extending leg.
    if (out.airborne) {
        const double apex = p.gravity_mps2 > 0.0
                                ? (p.jump_speed_mps * p.jump_speed_mps) /
                                      (2.0 * p.gravity_mps2)
                                : 0.0;
        const double full = std::clamp(p.tuck_full_frac, 1.0e-3, 1.0) * apex;
        const double h01 =
            full > 1.0e-9 ? std::clamp(out.height_m / full, 0.0, 1.0) : 0.0;
        // Smootherstep-free on purpose: this is already the tail of a
        // parabola, and a plain smoothstep on top of it is the ease the eye
        // reads as a fold rather than a snap.
        out.tuck01 = p.tuck_gain * (h01 * h01 * (3.0 - 2.0 * h01));
    } else {
        out.tuck01 = 0.0;
    }

    // ---- THE BURST, counting down. Never negative, so `hop_speed_mul`'s
    // ratio is a clean [0,1] ramp.
    out.dash_s = std::max(0.0, out.dash_s - dt_s);
    return out;
}

double hop_speed_mul(const HopState& s, const HopInputs& in,
                     const HopParams& p) {
    // "sprint only if shift is held" -- the LEVEL, not the charge, and not a
    // latch: let go and he is walking again on the very next step.
    const double base = in.shift_down ? p.sprint_cap_mul : 1.0;
    if (s.dash_s <= 0.0) return base;
    // The burst decays linearly back to whatever is live underneath it (the
    // sprint if he is still holding, the walk if he let go) -- `max` rather
    // than a lerp to `base` so a dash can never be SLOWER than the sprint it
    // is riding on top of, whatever the two dials are set to.
    const double frac = p.dash_decay_s > 0.0
                            ? std::clamp(s.dash_s / p.dash_decay_s, 0.0, 1.0)
                            : 0.0;
    return std::max(base, 1.0 + (p.dash_cap_mul - 1.0) * frac);
}

// ---------------------------------------------------------------------------
// ★★★ GAIT LADDER G2j -- THE PUMP-REPAIR WORK ANIMATION (see sim/gait.h).
double repair_wrench_stroke(double phase01, double torque_frac) {
    double u = phase01 - std::floor(phase01);
    const double tf = std::clamp(torque_frac, 0.05, 0.95);
    // Both legs are smootherstep-shaped rather than linear so the reversals
    // at the two ends are velocity-continuous -- a ratchet SNAPS back, but it
    // does not teleport, and a linear ramp reads as a jitter at this rate.
    if (u < tf) {
        // The loaded pull: +1 -> -1 over the LONGER part of the cycle.
        const double a = u / tf;
        return 1.0 - 2.0 * (a * a * (3.0 - 2.0 * a));
    }
    // The reset: -1 -> +1 over the shorter remainder. Faster by exactly the
    // ratio the two fractions differ by, which is what makes it a ratchet.
    const double a = (u - tf) / (1.0 - tf);
    return -1.0 + 2.0 * (a * a * (3.0 - 2.0 * a));
}

double repair_blow_swing(double blow_left_s, double blow_total_s) {
    if (!(blow_left_s > 0.0) || !(blow_total_s > 0.0)) return 0.0;
    // `u` runs 0 -> 1 through the blow (the state counts DOWN, the shape reads
    // forwards -- one inversion, here, rather than at every call site).
    const double u = std::clamp(1.0 - blow_left_s / blow_total_s, 0.0, 1.0);
    constexpr double kRaise = 0.42;  // wind up
    constexpr double kHit = 0.60;    // ... and the strike lands here
    if (u < kRaise) {
        const double a = u / kRaise;
        return a * a * (3.0 - 2.0 * a);  // 0 -> +1, the fist comes up
    }
    if (u < kHit) {
        const double a = (u - kRaise) / (kHit - kRaise);
        return 1.0 - 2.0 * a;  // +1 -> -1 FAST and LINEAR: a blow does not
                               // ease into the machine, it arrives
    }
    const double a = (u - kHit) / (1.0 - kHit);
    return -1.0 * (1.0 - a * a * (3.0 - 2.0 * a));  // -1 -> 0, the recovery
}

RepairWork step_repair_work(const RepairWork& prev, bool working,
                            bool finish_edge, double dt_s,
                            const RepairWorkParams& p) {
    RepairWork out = prev;
    out.on = working;
    if (dt_s > 0.0 && working)
        out.phase = std::fmod(prev.phase + p.cycle_hz * dt_s, 1.0);
    if (!working && prev.blow_s <= 0.0) {
        // Idle: the stroke rests at the top of the pull so the NEXT repair
        // starts from a hand that is already where a hand goes, not from
        // wherever the last one happened to stop.
        out.phase = 0.0;
    }
    // ★ THE ONE-SHOT. Fired by the edge, and it OUTLIVES `working` -- see the
    // header: `FinishRepair` is the transition OUT of Repairing, so the blow
    // is always swung by a man the mode machine has already sent back to Afoot.
    if (finish_edge) out.blow_s = p.blow_s;
    if (dt_s > 0.0) out.blow_s = std::max(0.0, out.blow_s - dt_s);
    return out;
}

GaitState step_gait(const GaitState& prev, const sim::WalkerState& w,
                    const sim::WalkerParams& p, const world::SnowpackField& f,
                    double dt_s, const GaitBodyGeom& body,
                    const GaitAir& air_in) {
    // ★★★ G2i: WHILE HE IS IN THE AIR, NEITHER FOOT IS ON THE GROUND. Both
    // feet leave stance (so no pin is set, no footstep is stamped, and no
    // stance foot can demand a pelvis drop -- the drop law's own `!in_stance`
    // guard does the last one for free), and both foot targets ride UP with
    // the body by exactly the hop height, so the legs travel with him instead
    // of reaching back down to snow he is no longer standing on. The landing
    // is an ordinary stance entry through the ordinary edge, which is why it
    // pins and prints like any other footfall.
    const bool air = air_in.height_m > 0.0;
    // ★ G2: dt_s is now consumed, by the pelvis-drop spring below. G1's own
    // foot-plant math stays phase- and event-driven, exactly as it was.
    GaitState out = prev;

    const double r = glm::length(w.pos);
    if (!(r > 0.0)) return out;  // degenerate world: nothing to plant against
    // ★★★ G2i-d: THE FLIGHT EDGES. `GaitAir` carries a LEVEL (`height_m`),
    // not an event, so both transitions -- the push-off and the touchdown --
    // are remembered on this side. Recorded AFTER the degenerate-world bail
    // so a frame that never ran the foot loop cannot eat one.
    out.was_air = air;
    const glm::dvec3 up = w.pos / r;

    // Re-orthogonalise his heading against THIS point's up, exactly the
    // fallback `step_walker` itself uses when there is no usable heading
    // (a dead stop on the very first step) -- one tangent-seed idiom, not a
    // second one invented here.
    glm::dvec3 fwd = w.heading - glm::dot(w.heading, up) * up;
    if (glm::length(fwd) < 1.0e-9) {
        const glm::dvec3 seed =
            std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
        fwd = glm::normalize(seed - glm::dot(seed, up) * up);
    } else {
        fwd = glm::normalize(fwd);
    }
    const glm::dvec3 left = glm::normalize(glm::cross(up, fwd));

    const double speed = glm::length(w.vel);
    const double stance_raw = sim::walker_stance_frac(p, speed);
    const double ds = std::clamp(stance_raw, 0.05, 0.95);
    const double stride = w.stride_m > 0.0 ? w.stride_m
                                            : sim::walker_stride(p, w.depth_m);
    const double lift = sim::walker_lift(p, w.depth_m);
    // The swing's ground excursion. Hoisted out of the swing branch (where it
    // was, unchanged, a per-foot local) because the touchdown re-anchor below
    // needs the same number to read the blend parameter it is freezing.
    const double excur = ds * stride;

    // ★ G2: the worse of the two feet decides the pelvis drop (see below,
    // after the loop) -- filled per-side as each foot's `target_w` settles.
    double needed_drop_m[2] = {0.0, 0.0};

    for (int s = 0; s < 2; ++s) {
        GaitFoot& o = out.foot[s];
        const GaitFoot& pf = prev.foot[s];
        // ★ THE CROSSOVER (see the header banner): this struct's own index
        // convention is 0 = left, 1 = right, but the render consumer's
        // existing leg-swing law (sled_model.cpp, pre-G1) puts the RIGHT leg
        // at phase+0.0 and the LEFT at phase+0.5. So here: index 0 (left)
        // gets +0.5, index 1 (right) gets +0.0 -- the consumer crosses the
        // wire back onto `model_side`, and says so where it does.
        double phase = w.gait_phase + (s == 0 ? 0.5 : 0.0);
        phase -= std::floor(phase);
        const bool in_stance_now = air ? false : (phase < ds);
        // Left = index 0 = +1 along `left`; right = index 1 = -1. (Purely a
        // self-consistent internal sign -- nothing outside this function
        // reads which absolute direction "left" pointed.)
        const double side_sign = (s == 0) ? 1.0 : -1.0;

        bool prev_in_stance;
        if (!pf.valid) {
            // ★ NO POP: seed both anchors at the under-hip rest position --
            // where a standing man's foot actually is -- and then let the
            // transition logic below run exactly once, unconditionally, so
            // the first real value is computed by the SAME law a live
            // transition uses rather than by a special case.
            const glm::dvec3 seed_dir =
                glm::normalize(w.pos + side_sign * 0.095 * left);
            const world::SnowpackField::GroundSample g = f.sample_at(seed_dir);
            const glm::dvec3 seed_w = seed_dir * g.drive_r;
            o.pin_w = seed_w;
            o.plant_w = seed_w;
            o.target_w = seed_w;
            o.last_ground_r = g.drive_r;
            o.last_ground_dir = seed_dir;
            o.yaw = yaw_of(fwd, up);
            prev_in_stance = !in_stance_now;  // force the edge below to fire
        } else {
            prev_in_stance = pf.in_stance;
        }

        // ★★★ G2i-d -- LEAVING THE GROUND AND MEETING IT AGAIN ARE THE SAME
        // EVENT, AND BOTH RE-ANCHOR THE FOOT WHERE THE BOOT IS.
        //
        // `air` overrides the stance/swing decision wholesale, so the flight
        // edges throw feet across the branch boundary with no regard for the
        // phase, and both directions snapped before this rung:
        //   TAKEOFF -- a foot in mid-STANCE is forced into the swing branch,
        //     where the blend parameter its phase happens to sit at can be
        //     anything. Measured on this lane's own walk fixture: 1.10 m in
        //     one 1/120 s frame, the foot teleporting from its pin to nearly
        //     the far plant. (Pre-existing since G2i; the tuck never hid it,
        //     because `tuck01` is ~0 at the push-off by construction.)
        //   TOUCHDOWN -- neither edge fired for the WHOLE flight, so both
        //     feet still carry a `pin_w`/`plant_w` pair sampled before the
        //     push-off: world points now several metres behind him. The
        //     stance foot would pin where he jumped from and the swing foot
        //     would blend toward a plant he has already run past.
        //
        // One law for both: on a flight edge the pin goes under where the
        // boot ACTUALLY IS (`pf.target_w` -- the same source, and the same
        // reasoning, as the ordinary stance entry's red-team fix below), the
        // boot's height above that pin is remembered, and `prev_in_stance` is
        // flipped so whichever of the two edges below is the right one FIRES
        // and re-derives the plant through the ordinary law. Nothing new is
        // invented about WHERE a foot goes; the edge just stops lying to the
        // laws that already know.
        double boot_up0_m = 0.0;
        const bool flight_edge = (air != prev.was_air) && pf.valid;
        if (flight_edge) {
            const glm::dvec3 src = glm::length(pf.target_w) > 1.0e-9
                                       ? pf.target_w
                                       : (w.pos + side_sign * 0.095 * left);
            const glm::dvec3 dir = glm::normalize(src);
            const world::SnowpackField::GroundSample g = f.sample_at(dir);
            const double rr = guarded_radius(o.last_ground_dir,
                                             o.last_ground_r, dir, g.drive_r);
            o.pin_w = dir * rr;
            o.yaw = yaw_of(fwd, up);
            boot_up0_m = std::max(0.0, glm::length(src) - rr);
            prev_in_stance = !in_stance_now;
        }

        if (in_stance_now && !prev_in_stance) {
            // ★★★ RED-TEAM FIX (G1b, P2): pin under where the foot ACTUALLY
            // WAS last frame (`pf.target_w`, the swing blend's last published
            // position) -- NOT at the far predicted `plant_w`.
            //
            // `ds` (the stance/swing split) is recomputed EVERY FRAME from
            // the CURRENT speed (`walker_stance_frac`), so a hard
            // deceleration can raise `ds` enough to land a still-mid-air foot
            // in stance on the very frame the phase has not actually reached
            // the plant. The first cut pinned at `plant_w` regardless -- a
            // point up to a full stride ahead of where the foot visibly
            // was -- and the foot teleported there. Pinning at the swing
            // blend's own last output instead means a mid-swing cutoff plants
            // exactly where the foot already looked like it was; in the
            // ORDINARY case (the swing completes on its own schedule) the
            // blend has already carried `target_w` to within a hair of
            // `plant_w` by the time phase crosses `ds`, so this is
            // bit-for-bit the old behaviour there.
            const glm::dvec3 src =
                (pf.valid && glm::length(pf.target_w) > 1.0e-9) ? pf.target_w
                                                                : o.plant_w;
            const glm::dvec3 dir = glm::length(src) > 1.0e-9
                                        ? glm::normalize(src)
                                        : glm::normalize(w.pos + side_sign *
                                                                     0.095 *
                                                                     left);
            const world::SnowpackField::GroundSample g = f.sample_at(dir);
            const double rr =
                guarded_radius(o.last_ground_dir, o.last_ground_r, dir, g.drive_r);
            o.pin_w = dir * rr;
            o.yaw = yaw_of(fwd, up);
            o.swing_t0 = 0.0;  // G2i-d: a planted foot has no swing to resume
            o.swing_up0 = 0.0;
        }
        if (!in_stance_now && prev_in_stance) {
            // ★ SWING START: the predictive plant, sampled once and held for
            // the whole swing (never resampled mid-flight -- L-PIN's
            // "targets never snap" reads on the swing side too: a foot
            // heading somewhere should not change its mind mid-air).
            // ★★★ G2e: the plant must land where the STANCE will be
            // CENTRED, and that depends on the duty factor. The hip travels
            // (1-ds)*stride during the swing and ds*stride during the
            // stance, so a pin that brackets the hip symmetrically sits at
            // (1 - 0.5*ds)*stride ahead of the swing-start position. The
            // first cut used a flat 0.5*stride -- close enough at walk duty
            // (0.62 -> 0.69 wanted), but at the 5.5 m/s run (ds 0.38 ->
            // 0.81 wanted vs 0.5 given) every pin landed ~0.9 m SHORT, the
            // whole stance trailed hopelessly behind the hip, and the drop
            // law read that horizontal despair as a demand to kneel.
            const glm::dvec3 raw =
                w.pos + fwd * ((1.0 - 0.5 * ds) * stride) +
                side_sign * 0.095 * left;
            const glm::dvec3 dir = glm::length(raw) > 1.0e-9
                                        ? glm::normalize(raw)
                                        : up;
            const world::SnowpackField::GroundSample g = f.sample_at(dir);
            const double rr =
                guarded_radius(o.last_ground_dir, o.last_ground_r, dir, g.drive_r);
            const glm::dvec3 plant_w = dir * rr;
            // Heel/toe pitch: two samples 0.22 m apart along heading, +
            // toe-up. Not guarded against a jump -- these two points are read
            // together, once, to describe a SLOPE, not tracked across frames.
            const glm::dvec3 heel_dir = glm::normalize(plant_w - fwd * 0.11);
            const glm::dvec3 toe_dir = glm::normalize(plant_w + fwd * 0.11);
            const double heel_r = f.sample_at(heel_dir).drive_r;
            const double toe_r = f.sample_at(toe_dir).drive_r;
            o.pitch_rad = std::atan2(toe_r - heel_r, 0.22);
            o.plant_w = plant_w;
            // ★ G2i-d: an ordinary swing start runs the un-remapped blend --
            // this pair is 0 on every frame that is not the tail of a hop,
            // which is what keeps the walk bit-identical.
            o.swing_t0 = 0.0;
            o.swing_up0 = 0.0;
        }

        o.in_stance = in_stance_now;
        o.valid = true;

        if (in_stance_now) {
            // ★ L-PIN, LITERALLY: the output target IS the pin, bit for bit
            // -- no recomputation, so it cannot drift even if the field under
            // it is discontinuous.
            o.target_w = o.pin_w;
        } else {
            // ★ SWING BLEND: reuse `walker_foot_offset`'s own Hermite SHAPE
            // (never a second curve, per the plan) to interpolate from the
            // released pin toward the predicted plant -- both of which are
            // ground points -- and then spend the SAME curve's `up_off`
            // upward from THAT interpolated ground point, not from the hip.
            double fwd_off = 0.0, up_off = 0.0;
            sim::walker_foot_offset(phase, stride, lift, ds, &fwd_off,
                                    &up_off);
            double t_blend = excur > 1.0e-9 ? (fwd_off + 0.5 * excur) / excur
                                            : 1.0;
            // ★★★ G2i-d: A FLIGHT EDGE RE-ZEROES THE BLEND, NOT THE PHASE
            // (L-PHASE: a rung may READ `gait_phase`, never reset it -- and
            // the render consumer's arm swing reads the same phase, so
            // shifting it would walk his arms out of step with his legs).
            // Freeze the blend parameter the still-advancing phase is sitting
            // at; from then until this foot plants, the swing runs
            // [t0,1] -> [0,1] off the re-anchored pin, and the lift runs from
            // the height the boot actually had to the height the phase wants.
            // So the step RESUMES from where the boot is -- same plant, same
            // phase, no teleport -- and the pair clears itself at the next
            // ordinary swing start.
            if (flight_edge) {
                o.swing_t0 = std::clamp(t_blend, 0.0, 0.99);
                o.swing_up0 = boot_up0_m;
            }
            if (o.swing_t0 > 0.0) {
                const double den = 1.0 - o.swing_t0;
                t_blend = den > 1.0e-9 ? (t_blend - o.swing_t0) / den : 1.0;
                const double tt = std::clamp(t_blend, 0.0, 1.0);
                up_off = o.swing_up0 * (1.0 - tt) + up_off * tt;
            }
            t_blend = std::clamp(t_blend, -0.1, 1.1);
            const glm::dvec3 ground_pos =
                o.pin_w + (o.plant_w - o.pin_w) * t_blend;
            const double gl = glm::length(ground_pos);
            const glm::dvec3 up_dir = gl > 1.0e-9 ? ground_pos / gl : up;
            o.target_w = ground_pos + up_dir * up_off;
        }
        // ★★★ G2i: and then the whole foot goes up with the man. Applied
        // AFTER the stance/swing branch on purpose -- it lifts whichever of
        // the two published this frame's target, and it is exactly zero on
        // every frame he is not in the air, so the walk is bit-identical.
        if (air) {
            // G2i's rigid ride: the stride pose he left the ground in,
            // translated straight up by the hop height.
            glm::dvec3 t = o.target_w + up * air_in.height_m;
            // ★★★ G2i-b -- AND THEN HE TUCKS. Chad, flying G2i: "jump should
            // have a knee forward or both knees bent a bit while in the air
            // because they kind of hang back there right now". The rigid ride
            // IS that hang: a foot half a stride behind him at takeoff is
            // still half a stride behind him at the apex, on a leg at nearly
            // full extension. So the target BLENDS off that ride toward a
            // posture hung from the hip. `tuck01` is 0 at both ends of the
            // arc (sim/gait.h's one law), so the fold has no branch to pop
            // through -- but see G2i-d below: on the DESCENT it no longer
            // returns to the ride, because the ride was the trailing stride
            // Chad saw the second time.
            //
            // The hip is estimated the SAME way the pelvis-drop law below
            // estimates it -- `w.pos` corrected by `lie_clearance_m` (it is
            // his drawn origin, NOT ground level -- the defect the drop law's
            // own banner records) plus the measured pelvis height and half
            // span -- and then raised by the hop, because render carries the
            // whole body up by that number and the hip goes with it. The
            // pelvis drop is 0 in the air for free (no stance foot, no
            // demand), so this hip is the rest hip.
            const glm::dvec3 hip_air =
                w.pos +
                up * (body.pelvis_rest_h_m - p.lie_clearance_m +
                      air_in.height_m) +
                side_sign * body.hip_half_span_m * left;
            const bool lead = (s == air_in.lead_side);
            // ★★★ G2i-d -- ON THE WAY DOWN THE STRIDE IS GONE. Chad, flying
            // G2i-c: "when he is going to land his feet are swinging and
            // trailing back -- I didn't want that at all. I want it to look
            // like a normal jump is all."
            //
            // The tuck below blends the pose AWAY from `t`, and until this
            // rung `t` was the rigid ride for the whole flight -- the live
            // pin/plant swing blend, still advancing on `gait_phase` and
            // still aimed at ground points sampled before he jumped, simply
            // translated up. On the ascent that is correct (he leaves the
            // ground off the stride he had, and the tuck folds it away). On
            // the DESCENT it is the whole defect: `tuck01` is height-driven
            // and therefore falls back to 0 by the touchdown, handing the
            // legs back to a mid-air stride exactly when he is most visible
            // against the snow.
            //
            // So the descent moves the ENDPOINT: `t` is walked off the ride
            // and onto a LANDING POSE -- both boots under their own hips,
            // near-extended, reaching -- by `land01`, which is 0 at the apex
            // and full a tenth of a second later (see
            // `AirLandParams::full_at_fall_mps` for why it is driven by fall
            // SPEED and not by height). It is continuous at the changeover
            // for any `tuck_gain`, and by the time the tuck starts to unfold
            // the gait target's weight in the product is already ~0.
            const double land01 =
                (air_in.vert_vel_mps < 0.0 &&
                 air_in.land.full_at_fall_mps > 1.0e-9)
                    ? std::clamp(-air_in.vert_vel_mps /
                                     air_in.land.full_at_fall_mps,
                                 0.0, 1.0)
                    : 0.0;
            if (land01 > 0.0) {
                const double down_m = lead ? air_in.land.lead_drop_m
                                           : air_in.land.trail_drop_m;
                const double fore_m = lead ? air_in.land.lead_fwd_m
                                           : -air_in.land.trail_back_m;
                const glm::dvec3 landing =
                    hip_air - up * down_m + fwd * fore_m;
                t += (landing - t) * land01;
            }
            if (air_in.tuck01 > 0.0) {
                const double down_m = lead ? air_in.tuck.lead_drop_m
                                           : air_in.tuck.trail_drop_m;
                const double fore_m = lead ? air_in.tuck.lead_fwd_m
                                           : -air_in.tuck.trail_back_m;
                const glm::dvec3 tucked =
                    hip_air - up * down_m + fwd * fore_m;
                const double k = std::clamp(air_in.tuck01, 0.0, 1.0);
                t += (tucked - t) * k;
            }
            o.target_w = t;
        }

        // ★★★ GAIT LADDER G2 -- "over-reach lowers the pelvis, never
        // straightens the knee." The hip this foot answers to, estimated the
        // way `render/sled_model.cpp`'s own leg block estimates it (rest
        // height + half-span off the SAME `up`/`left` this function already
        // built) -- BEFORE this frame's drop is applied, because the drop
        // IS the unknown being solved for here. `diff` decomposes into the
        // vertical separation (hip above foot) and the horizontal reach; if
        // dropping the hip by `drop` alone can bring the total inside
        // `reach_frac * leg_reach`, `drop` is exactly that amount -- solved
        // from the right triangle, not iterated.
        //
        // ★★★ THE DEFECT THIS COMMENT RECORDS SO IT IS NEVER RE-DISCOVERED:
        // `w.pos` is NOT ground level. `step_walker`'s own tail pins it at
        // `drive_r + lie_clearance_m` -- his DRAWN ORIGIN, a fixed clearance
        // (~0.564 m) above the surface, not his feet. `body.pelvis_rest_h_m`
        // (0.98 m) is measured the OTHER way, as hip height above the
        // GROUND (it mirrors `render/sled_model.cpp`'s own pelvis-node
        // fallback, which is hip height above the ROOT bone, and the ROOT
        // sits at `w.pos`'s own mapped position -- so this reasoning update
        // does NOT change which reference point pelvis height is measured
        // from; it corrects what `w.pos` itself is offset from). The first
        // cut added the full 0.98 m on top of `w.pos` UNCONDITIONALLY --
        // double-counting the 0.564 m clearance already baked into `w.pos`
        // and inflating every hip estimate by that much (measured: v_comp
        // read ~1.5-1.6 m on this rung's own smoke run, where 0.98 m was
        // expected -- a one-off stderr probe added and removed during this
        // rung's own build round is what caught it). Subtracting
        // `p.lie_clearance_m` here recovers the approximate GROUND point
        // under him before adding the real pelvis height back on top.
        {
            // ★★★ G2e: ONLY A STANCE FOOT MAY ASK FOR PELVIS DROP. Nobody
            // crouches to reach the foot they are SWINGING -- the swing leg
            // simply folds at the knee, free of charge. The first cut let a
            // late-swing target (still well ahead of the hip, lift nearly
            // spent) demand up to ~0.55 m of drop every cycle, and the
            // spring averaged the two alternating feet into a permanent
            // deep bounce (measured 0.23 m at the 5.5 run WITH the duty
            // fade -- three times the stance-only figure).
            if (!in_stance_now) {
                needed_drop_m[s] = 0.0;
                continue;
            }
            const glm::dvec3 hip_pos =
                w.pos + up * (body.pelvis_rest_h_m - p.lie_clearance_m) +
                side_sign * body.hip_half_span_m * left;
            const glm::dvec3 diff = hip_pos - o.target_w;
            const double v_comp = glm::dot(diff, up);
            const double h_comp = glm::length(diff - v_comp * up);
            const double max_reach = body.reach_frac * body.leg_reach_m;
            double drop = 0.0;
            if (h_comp < max_reach) {
                const double allowed_v =
                    std::sqrt(std::max(0.0, max_reach * max_reach -
                                                h_comp * h_comp));
                drop = v_comp - allowed_v;
            } else {
                // ★★★ G2e (Chad: "walking underneath the road up to my
                // waist"): the horizontal reach alone already exceeds the
                // envelope -- NO amount of vertical drop closes a HORIZONTAL
                // component, so this foot demands NOTHING. The first cut
                // demanded `v_comp` here -- pelvis down to FOOT LEVEL, and
                // at the 5.5 m/s run the mis-centred plant put a foot
                // horizontally out of reach on most frames, so the man ran
                // waist-deep in the road. Render's 0.98*reach clamp owns
                // the horizontal residual; this law lowers, it does not
                // chase the unfixable.
                drop = 0.0;
            }
            // ★★★ G2e: THE CROUCH IS A WALK'S ANSWER, NOT A RUN'S. A walker
            // vaults a straight-ish leg and lowers the pelvis when reach
            // runs out; a RUNNER resolves the same geometry with a flight
            // phase and toe-off plantarflexion (~0.25 m of effective leg
            // G4 will add) -- never by squatting. Until G4 exists, a run's
            // stance-edge over-reach belongs to the render clamp (which
            // reads as a runner's full leg extension), so the drop demand
            // fades continuously as the duty factor leaves walking:
            // full at ds >= 0.62 (free walk), ~quarter at the 5.5 m/s
            // run's 0.38. Measured before this factor: the run pegged the
            // 0.35 m posture cap and Chad ran waist-deep.
            const double run_upright_w =
                std::clamp((ds - 0.36) / (0.62 - 0.36), 0.0, 1.0);
            drop *= run_upright_w;
            // Capped by THREE physical bounds: the leg cannot fold past
            // most of its own length (0.85*reach); the pelvis keeps a
            // 0.10 m crouch clearance above the sole; and ★G2e: a WALKING
            // crouch is bounded by posture, not by anatomy alone -- past
            // ~0.35 m this stops being a gait and becomes a squat, and the
            // clamp is the honest answer beyond it.
            const double drop_cap =
                std::min({0.85 * body.leg_reach_m,
                          body.pelvis_rest_h_m - 0.10, 0.35});
            needed_drop_m[s] = std::clamp(drop, 0.0, std::max(0.0, drop_cap));
        }
    }

    // ★ THE WORSE FOOT WINS, THEN THE SPRING SETTLES IT. A critically damped
    // response so a foot planting -- a genuine STEP in demand -- lowers the
    // pelvis smoothly rather than snapping it, and settles without ringing.
    // ★ RED-TEAM FIX (G2, own build round): the plan's own 12 rad/s was
    // measured (a one-off stderr probe, this rung's own smoke run) to lag a
    // RAMPING demand -- the hip-to-pin distance grows continuously through a
    // whole stance, not just at footfall -- by ~0.1-0.17 m, which is exactly
    // the kind of steady-state tracking error a 2nd-order system carries
    // against a ramp. 16 rad/s cuts that lag roughly in proportion while
    // still reading as a smooth settle, not a snap; `reach_frac`'s widened
    // margin (see sim/gait.h) is the OTHER half of closing this gap, and
    // between the two the render clamp stops being the thing doing the work.
    {
        constexpr double kDropOmegaRps = 16.0;
        const double drop_target = std::max(needed_drop_m[0], needed_drop_m[1]);
        out.pelvis_drop_vel_mps = prev.pelvis_drop_vel_mps;
        out.pelvis_drop_m = gait_spring_step(prev.pelvis_drop_m,
                                             out.pelvis_drop_vel_mps,
                                             drop_target, kDropOmegaRps, dt_s);
        if (out.pelvis_drop_m < 0.0) {
            // ★ NEVER RISES ABOVE REST: the law lowers the pelvis, full
            // stop. A spring that undershot toward a negative target (never
            // actually asked for here, since `drop_target >= 0` always) is
            // clamped and its velocity killed so it cannot ring through
            // zero instead of settling on it.
            out.pelvis_drop_m = 0.0;
            out.pelvis_drop_vel_mps = std::max(0.0, out.pelvis_drop_vel_mps);
        }
    }

    // ★ BOB, SWAY, TRENDELENBURG: pure functions of the phase and the SAME
    // `ds` this step already computed -- see the header for the sign
    // conventions. `w.gait_phase` is the raw, un-shifted phase (foot index
    // 1 / "right"'s own reference), the SAME argument the per-foot loop
    // above used before adding its own +0.0/+0.5 crossover offset.
    {
        constexpr double kBobWalkM = 0.035, kBobRunM = 0.09;
        constexpr double kSwayWalkM = 0.045, kSwayRunM = 0.02;
        constexpr double kRollAmpRad = 0.026179939;  // ~1.5 degrees
        out.bob_m = gait_vertical_bob_m(w.gait_phase, ds, kBobWalkM, kBobRunM);
        out.sway_m =
            gait_lateral_sway_m(w.gait_phase, ds, kSwayWalkM, kSwayRunM);
        out.roll_rad =
            gait_trendelenburg_roll_rad(w.gait_phase, ds, kRollAmpRad);
    }

    return out;
}

}  // namespace sim
