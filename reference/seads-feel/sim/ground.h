#pragma once

#include <cmath>
#include <glm/glm.hpp>

#include "sim/environment.h"
#include "sim/params.h"
#include "sim/state.h"
#include "sim/world.h"
#include "world/buildings.h"
#include "world/heightfield.h"

// R4 — solid ground: terrain-crash + touch-and-stick GROUNDED landing
// (MASTER_PLAN §3.A; Chad's 2026-07-15 rulings: the contact regime lives in
// the KERNEL reading env.ground, and landing v1 is touch-and-stick — gentle
// contact within limits -> GROUNDED -> rolling friction to a stop; takeoff =
// the normal airborne integration winning again, no scripted rotate speed).
// Ground-roll STEERING is R4b (cuttable tail) — no steering here.
//
// SINGLE-SOURCE: the ONE elevation query is world::HeightField::radius_at —
// the same post-blur field the render mesh displaces and the props drape onto
// (the H1 anti-fork: a second height source forks the crash surface). The
// landing normal is the field's own finite-difference normal_at, so slope
// acceptance and elevation can never disagree.
//
// Called by sim::step AFTER the airborne translation integration, gated on
// env && env->ground — the env==null path never reaches this file (R3's
// all-null bit-identity proof, preserved by construction). Rotation stays the
// normal kernel in every regime (v1: attitude is free on the ground; the
// low-speed BALLISTIC attitude-hold already owns the stopped airframe).
//
// Known v1 simplifications (Chad flies these, then rules):
//  - The surface constraint is RADIAL (snap to radius_at along local_up), not
//    along the terrain normal — second-order error inside the slope limit
//    (cos 15 deg ~ 0.97), and rolling ignores slope-along deceleration.
//  - Liftoff releases on ANY net outward acceleration; a marginal-lift roll
//    can micro-cycle grounded<->airborne. The re-attach GRACE below (sink
//    within a few ticks of gravity sag re-attaches without the attitude
//    acceptance) is what keeps that from chattering into a crash — a marginal
//    release happens precisely at high alpha, where re-grading attitude would
//    kill a benign rollout (red-team P0-1). The strict acceptance still gates
//    every real descent (a true approach sinks >> a few g*dt).
//  - The sink acceptance is RADIAL, not terrain-normal: a fast level pass
//    into a landable slope reads sink ~ 0 and sticks. Chad flies, then rules.

namespace sim {

inline void ground_contact(const SimState& state, SimState& next,
                           const world::HeightField& hf, const GroundParams& gp,
                           const AircraftParams& p, double wheel_brake,
                           double dt) {
    next.crashed = false;  // transient: re-derived every tick
    next.wing_strike = 0;  // transient consequence events (R4-FLY-5)
    next.prop_strike = false;
    const glm::dvec3 up = local_up(next.position);
    const double r = glm::length(next.position);
    // The CONTACT surface is the terrain plus the gear/belly height: SimState
    // position is the CG, and a CG constrained to the terrain buries half the
    // fuselage (Chad's first fly: "sunk into the road, only a part sticking
    // up"). The wheels meet the ground; the CG rides contact_height_m above.
    const double r_s = hf.radius_at(up) + gp.contact_height_m;

    if (state.on_ground) {
        // GROUNDED regime. Release iff the airborne integration produced net
        // OUTWARD acceleration this tick (lift + thrust beating gravity — the
        // honest takeoff). dv IS the integrator's applied impulse; the
        // tangential-chord growth of r on a sphere is O((v dt)^2 / R) and
        // never enters this dot.
        const double radial_acc =
            glm::dot(next.velocity - state.velocity, up) / dt;
        if (radial_acc > 0.0) {
            next.on_ground = false;  // liftoff: the airborne integration stands
            return;
        }
        // The ground fell away (rolled off an edge steeper than the landable
        // limit): release to the airborne integration — never teleport down a
        // cliff. The follow tolerance is what a limit-slope descent can drop in
        // one tick (x2 for bilinear kinks) plus one tick of gravity fall —
        // structural, derived from the config the mechanism reads (never a
        // calibrated constant).
        const double c = std::max(gp.slope_limit_cos, 1e-9);
        const double tan_lim = std::sqrt(std::max(0.0, 1.0 - c * c)) / c;
        const double follow_m =
            2.0 * tan_lim * glm::length(state.velocity) * dt + p.g * dt * dt;
        if (r > r_s + follow_m) {
            next.on_ground = false;
            return;
        }
        // Over-limit terrain under the roll: DIRECTION decides (dot(n, up) is
        // directionless — red-team follow-up). INTO a rising wall = crash (the
        // radial snap would otherwise hoist the airframe up any cliff for
        // free); onto a FALLING edge = release to the airborne integration
        // (the fell-away check above usually fires first; this covers the
        // crest tick where the probe already reads the slope ahead). On an
        // incline the outward normal tilts against the uphill direction, so
        // uphill motion has dot(n, v_t) < 0.
        const glm::dvec3 n = hf.normal_at(up, gp.normal_probe_m);
        if (glm::dot(n, up) < gp.slope_limit_cos) {
            const glm::dvec3 v_t =
                next.velocity - glm::dot(next.velocity, up) * up;
            const double sp_t = glm::length(v_t);
            const bool uphill = sp_t > 1e-6 && glm::dot(n, v_t / sp_t) < 0.0;
            next.on_ground = false;
            if (uphill || sp_t <= 1e-6) {
                next.crashed = true;
            }
            return;
        }
        // R4-FLY-5 GROUND LOOP (Chad: "if I turn too fast the plane should
        // rotate and crash"): the integrator's applied LATERAL acceleration
        // (the path-turning force — aero side force at taxi yaw) beyond what
        // tires hold = a ground loop. Measured from the actual impulse, not a
        // yaw-rate proxy: radial and along-track components excluded, so the
        // landing capture and plain braking can never trigger it.
        if (gp.ground_loop_lat_g > 0.0) {
            const glm::dvec3 dv = next.velocity - state.velocity;
            glm::dvec3 dv_t = dv - glm::dot(dv, up) * up;
            const glm::dvec3 v_t0 =
                state.velocity - glm::dot(state.velocity, up) * up;
            const double sp0 = glm::length(v_t0);
            if (sp0 > 1e-6) {
                const glm::dvec3 vhat = v_t0 / sp0;
                dv_t -= glm::dot(dv_t, vhat) * vhat;
                if (glm::length(dv_t) / dt > gp.ground_loop_lat_g * p.g) {
                    next.on_ground = false;
                    next.crashed = true;  // ground loop: the tires let go
                    return;
                }
            }
        }
        // Constrain: snap radially to the local surface, keep the tangential
        // velocity (ground speed), pay rolling friction toward a full stop.
        // The stiction clamp IS the stop — touch-and-stick never creeps.
        // R4g wheel brakes: the held brake ADDS decel (fraction of g) on top
        // of rolling friction — same clamp, so a brake-hold at full throttle
        // pins the plane at exactly 0 (the runup) and never creeps backward.
        // Brakes are tangential only: they cannot fake or suppress the radial
        // liftoff-release above (Fable design review (a)).
        next.position = up * r_s;
        glm::dvec3 v_t = next.velocity - glm::dot(next.velocity, up) * up;
        const double sp = glm::length(v_t);
        const double brake = glm::clamp(wheel_brake, 0.0, 1.0);
        const double dec = (gp.friction + brake * gp.brake_friction) * p.g * dt;
        v_t = sp > dec ? v_t * (1.0 - dec / sp) : glm::dvec3{0.0};
        next.velocity = v_t;
        next.on_ground = true;
        return;
    }

    // Airborne: the terrain contact test. radius_at >= R everywhere (DEM 0 ==
    // sea level == R), so this strictly subsumes the bare-sphere altitude <= 0
    // rule — and a sea-level lake touchdown is a CONTACT, not a crash.
    if (r > r_s) return;

    // T3 DEEP-PENETRATION WALL STRIKE (docs/tunnel_staging.md; runs BEFORE any
    // landing acceptance). Landing acceptance is a SURFACE law: it snaps the CG
    // radially UP to r_s. A state reaches ground_contact deep below the surface
    // by EMERGING from a tunnel volume into solid rock (the sim gates
    // ground_contact on !in_tunnel, so a wall graze — crossing sd=0 sideways
    // ~2000 m down with a level attitude and near-zero radial sink — arrives
    // here on the tick it leaves the net); a fast flight into a steep DEM face
    // can also exceed the threshold airborne (it crashes either way — the clause
    // just grades it via this branch rather than the acceptance). A penetration
    // deeper than any legitimate per-tick contact approach (one tick of sink,
    // sub-metre, plus the sub-metre bilinear kink — well under contact_height_m
    // + a few metres) is a wall strike, NOT a touchdown: grade it a crash and
    // return, before the acceptance can teleport the airframe up the full depth.
    // gp value 0 = off (the clause never fires; T1 behavior bit-identical). r_s
    // > r here (the r > r_s early return above), so r_s - r > 0.
    if (gp.deep_penetration_m > 0.0 && (r_s - r) > gp.deep_penetration_m) {
        next.crashed = true;
        return;
    }

    // Contact. Landing iff ALL of: gentle sink, upright against the terrain
    // normal, terrain locally within the slope limit. Else crashed — the
    // caller owns the respawn.
    //
    // RE-ATTACH GRACE (red-team P0-1, the hysteresis that makes the regime
    // boundary safe): a sink within a few ticks of pure gravity sag can only
    // be a marginal liftoff settling back — and a marginal liftoff happens
    // precisely at high alpha, so re-grading the ATTITUDE here would kill a
    // benign rollout ("the perfect landing dies two seconds later"). Such a
    // micro-sag re-attaches with only the terrain-slope check; every real
    // descent arrives far above the grace (structural: derived from g*dt,
    // never a feel dial) and takes the full acceptance.
    const glm::dvec3 n = hf.normal_at(up, gp.normal_probe_m);
    const double sink = -glm::dot(next.velocity, up);
    const glm::dvec3 body_up = next.orientation * glm::dvec3{0.0, 1.0, 0.0};
    const bool gentle = sink <= gp.max_sink_ms;
    const bool upright = glm::dot(body_up, n) >= gp.slope_limit_cos;
    const bool flat = glm::dot(n, up) >= gp.slope_limit_cos;
    const bool grace = sink <= 3.0 * p.g * dt;
    if ((gentle && upright && flat) || (grace && flat)) {
        next.on_ground = true;
        next.position = up * r_s;
        next.velocity -= glm::dot(next.velocity, up) * up;  // keep ground speed
    } else {
        next.crashed = true;
    }
}

// R4f — building collision (Chad's fly ask, 2026-07-15: "I go right through
// houses"). A position inside any building prism is a crash, airborne OR
// rolling (consistent with the rising-wall crash above: you flew/taxied into
// a structure). The prism base is hf.radius_at(center_dir) — the SAME field
// as terrain contact, one crash surface. Gated by the caller on env->ground
// AND env->obstacles both live; the null path never reaches here. The
// equiv-area radius under-covers long buildings (clipping a warehouse corner
// may not register) — inflate_m is the dial, documented, Chad's on-sight.
inline void obstacle_contact(SimState& next,
                             const world::BuildingColliders& obs,
                             const world::HeightField& hf,
                             const GroundParams& gp) {
    if (next.crashed) return;  // already dead this tick
    if (obs.hit(next.position, hf, gp.obstacle_inflate_m,
                gp.obstacle_base_margin_m)) {
        next.crashed = true;
    }
}

// R4-FLY-5 — grounded POST-ROTATION dynamics (Chad's consequence ruling,
// 2026-07-15: wings bank freely on the ground and DAMAGE on strike; hard
// braking noses over and breaks the prop; residual rotation dies through the
// tires). MUST run AFTER the rotation integration in sim::step —
// ground_contact runs before it, and any orientation/omega edit there is
// overwritten the same tick (Fable design review P0: the pre-rotation slot
// is dead code). Contains:
//  1. ROLL ALIGNMENT — REVERTED by Chad (shipped rate 0 = off, bit-identical
//     free-roll ground); the code stays dormant for a future surface.
//  2. TIRE ROTATIONAL FRICTION — with the q_att_floor removed on the ground
//     (controls need AIRSPEED: rudder is dead at a stop), aero damping
//     vanishes at q→0 and residual rotation would spin a parked plane
//     forever; the tires/tailwheel resist it.
//  3. BRAKE NOSE-OVER — braking pitches the nose down proportionally to the
//     brake decel (the wheels-ahead-of-CG moment); past the prop clearance
//     angle the PROP STRIKES (event; the caller owns what a broken prop
//     means — the kernel only detects).
//  4. WINGTIP STRIKE — a tip swept below the terrain surface sets
//     wing_strike ±1 (the caller maps it to wing damage).
inline void ground_dynamics(SimState& next, const world::HeightField& hf,
                            const GroundParams& gp, double wheel_brake,
                            double dt) {
    const glm::dvec3 up = local_up(next.position);
    const glm::dvec3 fwd = next.orientation * glm::dvec3{0.0, 0.0, -1.0};

    // 1. Roll alignment (dormant at rate 0 — Chad reverted it).
    if (gp.roll_align_rate > 0.0) {
        const glm::dvec3 n = hf.normal_at(up, gp.normal_probe_m);
        glm::dvec3 des = n - glm::dot(n, fwd) * fwd;
        const double dlen = glm::length(des);
        if (dlen >= 1e-9) {
            des /= dlen;
            const glm::dvec3 bup = next.orientation * glm::dvec3{0.0, 1.0, 0.0};
            const double psi = std::atan2(glm::dot(glm::cross(bup, des), fwd),
                                          glm::dot(bup, des));
            const double dmax = gp.roll_align_rate * dt;
            const double dpsi = glm::clamp(psi, -dmax, dmax);
            next.orientation =
                glm::normalize(glm::angleAxis(dpsi, fwd) * next.orientation);
            next.angular_vel.z = 0.0;
        }
    }

    // 2. Tire rotational friction (all axes — the gear resists any rotation
    // of a grounded airframe; at speed the aero torques dwarf it).
    if (gp.ground_ang_damp > 0.0) {
        next.angular_vel /= 1.0 + gp.ground_ang_damp * dt;
    }

    // 3. Brake nose-over + prop strike. The pitch-down is the visible
    // consequence Chad asked for ("send you to pitch down attitude"); the
    // strike fires when the nose crosses the prop clearance angle against
    // the terrain plane — evaluated UNCONDITIONALLY while grounded (review
    // P2: a brakeless elevator nose-slam at taxi speed must strike too, not
    // just a braking dip). Pitch-down = rotation about the world RIGHT axis
    // by a NEGATIVE angle (pitch-up = +omega_x, SPEC §7). Released brakes
    // let the tail SETTLE back (restore toward the terrain plane at the same
    // rate, never past it) — a nose-dip is a moment, not a pose.
    if (gp.brake_pitch_rate > 0.0) {
        const glm::dvec3 n = hf.normal_at(up, gp.normal_probe_m);
        const glm::dvec3 right = next.orientation * glm::dvec3{1.0, 0.0, 0.0};
        const double sp = glm::length(next.velocity);
        const double brake = glm::clamp(wheel_brake, 0.0, 1.0);
        if (brake > 0.0 && sp > 1.0) {  // braking at speed: the nose dips
            // R4-FLY-6: the dip fades with ground speed (an inertial moment —
            // a crawl-stop has no momentum to pitch the nose; braking from
            // speed still threatens the prop). 0 = unscaled, bit-identical.
            const double f = gp.noseover_full_speed_ms > 0.0
                                 ? std::min(1.0, sp / gp.noseover_full_speed_ms)
                                 : 1.0;
            const double dth =
                gp.brake_pitch_rate * brake * gp.brake_friction * f * dt;
            next.orientation =
                glm::normalize(glm::angleAxis(-dth, right) * next.orientation);
        } else {
            // Settle: if the nose sits BELOW the terrain plane, pitch back
            // up toward it (clamped at the plane — no bobbing).
            const glm::dvec3 fwd2 =
                next.orientation * glm::dvec3{0.0, 0.0, -1.0};
            const double below = -glm::dot(fwd2, n);  // + = nose below plane
            if (below > 0.0) {
                const double dth =
                    std::min(gp.brake_pitch_rate * gp.brake_friction * dt,
                             std::asin(glm::clamp(below, -1.0, 1.0)));
                next.orientation = glm::normalize(glm::angleAxis(dth, right) *
                                                  next.orientation);
            }
        }
        // The clearance test itself: however the nose got below the angle
        // (braking dip OR a brakeless slam), the prop is in the dirt.
        const glm::dvec3 fwd3 = next.orientation * glm::dvec3{0.0, 0.0, -1.0};
        if (glm::dot(fwd3, n) < -std::sin(gp.prop_strike_pitch_rad)) {
            next.prop_strike = true;
        }
    }

    // 4. Wingtip strike: a tip below the TERRAIN surface (the dirt, not the
    // contact surface — the wing hits the ground itself). Reported, never
    // resolved here: the caller owns the damage model.
    if (gp.wing_halfspan_m > 0.0) {
        const glm::dvec3 right = next.orientation * glm::dvec3{1.0, 0.0, 0.0};
        for (double sgn : {-1.0, 1.0}) {
            const glm::dvec3 tip =
                next.position + sgn * gp.wing_halfspan_m * right;
            const glm::dvec3 td = glm::normalize(tip);
            if (glm::length(tip) < hf.radius_at(td)) {
                next.wing_strike = sgn > 0.0 ? 1 : -1;
            }
        }
    }
}

}  // namespace sim
