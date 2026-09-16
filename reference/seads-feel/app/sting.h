#pragma once
// ★★★ STING RPAS — the Sudburian's hand-launched FPV interceptor drone
// (Chad's ask 2026-09-03; plan Game_loop_idea/PLAN_STING_RPAS.md).
//
// WHAT THIS IS. On foot (J dismount), P shoulders a handheld launcher stock;
// the player LEADS an enemy aircraft, fires, and a Sting-class quad (Wild
// Hornets "Sting", the Shahed interceptor) launches off the stock. The camera
// goes to a chase cam behind the drone and the player flies it to a ram-
// detonate intercept. Two large turns, a battery clock, or the P key end it in
// a self-destruct. Three launches per match.
//
// WHAT THIS IS DELIBERATELY NOT.
//  * NOT new physics. Chad's words: "flies to intercept with flying kernel but
//    different cascade for the sting (simplified)". The plant is the frozen
//    kernel's own `sim::step`, untouched, flying the SAME AircraftParams the
//    aeroplane flies — only the thrust/speed command differs. No multirotor
//    model, no second plant.
//  * ★★★ IT FLIES THE REAL INSTRUCTOR CASCADE (Chad's fly ruling 2026-09-03:
//    the v1 bank/gamma PD was "uncontrollable — I should be able to use mouse
//    aim circle and it have a cascade"). So the Sting now carries its OWN
//    input::AimFrame (transported + mouse-rotated exactly like the plane's),
//    its own control::Internal, and each tick runs control::step -> sim::step
//    — the identical mouse-aim experience, reticle circle and all, on the
//    identical airframe. The v1 PD (drone::autopilot steered at the aim) is
//    GONE, not kept as an arm.
//  * NOT a life. Losing the drone costs a launch, never planes_left, and it
//    neither reads nor writes the respawn lock.
//
// PURITY. Everything here is raylib-free and clock-free: state in, state out,
// dt passed. The key table, camera, HUD, and FX live in app/main.cpp /
// instructor_tick.h. Tested by test/unit/test_sting.cpp without a window.

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "control/controller.h"
#include "app/rest_horizon.h"  // v13 rest-edge horizon law (shared w/ the plane)
#include "input/aim_frame.h"
#include "sim/aero.h"
#include "sim/state.h"
#include "sim/step.h"
#include "weapon/ballistics.h"

namespace app {

// Every number here is either Chad's verbatim ruling (turns_max 2, per_match
// 3), a plan-approved default from the 2026-09-03 Sting research (battery
// game-compressed from the real 6-min-at-max endurance; speed_max 140 m/s
// where the real ~87 m/s cannot catch a 110-175 m/s pursuit bandit — the
// sourced interceptor law is a ~25% speed margin over the target), or a
// derived starting point his eye retunes in flight (the PD gains are the
// bandit PURSUIT gains, already proven stable on this exact airframe).
struct StingParams {
    // Launch.
    double launch_speed = 30.0;  // [m/s] off the stock, along the aim
    double launch_up_m = 1.7;    // [m] muzzle height above the man's feet
    // Flight envelope (the speed COMMAND band; the plant does the flying).
    // ★ Chad's fly-2 ruling, verbatim: "they should go 250m/s-450m/s not
    // accurate to real life but more fun" + "zoom exxageratingly fast" — so
    // the band IS 250-450 and the Sting flies its OWN derived airframe
    // (sting_aircraft_params below) whose thrust makes the acceleration
    // violent on purpose.
    // ★ ST-5 prop/audio round RULING (Chad, verbatim): "I would like to have
    // a lower bottom end, right now it only goes as slow as 250 m/s, but by
    // pressing s, I should hear it go down to a lower frequency and volume
    // and see the blades seemingly spinning slower and it shall go down to
    // 150 m/s as a bottom end." So the S floor ALONE moves, 250 -> 150; the
    // cruise and the chase speeds are untouched (he signed those, and this
    // ruling names only the bottom). The band widening is what gives the
    // prop blur and the motor buzz somewhere low to go — main.cpp's
    // sting_thr01 is the band fraction over exactly this pair.
    double speed_max = 450.0;    // [m/s] W held — the chase speed
    double speed_cruise = 330.0; // [m/s] no key — the transit speed
    double speed_min = 150.0;    // [m/s] S held — the line-up/loiter speed
    // Autothrottle — the bandit kCruiseTrim law verbatim (0.40 + 0.02/mps).
    // The CASCADE never manages energy (throttle is passthrough by SPEC
    // §9.8), so this app-side speed hold is legal and identical in kind to
    // the bandits'.
    double throttle_base = 0.40;
    double throttle_p = 0.02;
    // Life limits (Chad: "two large turns before self destruct, or on a
    // timer" / "3 reloads per match").
    // ★ fly-4 RULING (Chad, after his first kill): "I realized that I need
    // more than 3 turns before it blows up, maybe it just needs a finite
    // battery and that will be enough of a challenge." So the turn-count
    // destruct is OFF — turns_max 0 disables the arming structurally (the
    // latch machinery stays, counted and testable, behind this one dial).
    int turns_max = 0;
    double turn_g = 6.0;          // |n-1| beyond this counts toward a turn...
    double turn_dwell_s = 0.7;    // ...once held this long (one turn per latch)
    double turn_release_g = 3.0;  // hysteresis: latch re-arms below this
    double battery_s = 60.0;  // [s] flight clock (fly-5: "only a 1 min
                              // battery for each of the 5 rpas. it blows up")
    double warn_s = 3.0;        // [s] armed-warble window before the boom
    int per_match = 5;          // fly-4: "we should get 5 drones as a loadout"
    // The ram. damage/v_ref feed combat::ke_damage through the ordinary
    // combat_tick sweep (the drone IS the projectile): at v_ref 20 m/s any
    // real closing speed saturates the overspeed cap, so 2000 reference HP
    // one-shots a 100 HP bandit through the obliquity floor with an order of
    // magnitude to spare. prox_add_m is the contact-charge fuze — the sourced
    // kill is a ram, so the radius is small (it must PASS THROUGH the target's
    // 15 m hit sphere, not burst near it like the flak curtain).
    double ram_damage = 2000.0;
    double ram_v_ref = 20.0;
    double prox_add_m = 4.0;
};

// Per-frame command, written by main.cpp from the devices, consumed (and
// zeroed where noted) by sting_step on the first tick of the frame.
struct StingCmd {
    double dx = 0.0;         // mouse delta, zeroed on consumption
    double dy = 0.0;
    double speed_cmd = 0.0;  // [m/s] resolved from W/S each frame (held state)
    bool detonate = false;   // P mid-flight — consumed on first tick
    bool ground_kill = false;  // main's terrain check (the DEM surface lives
                               // app-side); latched until consumed
    // SPACE freelook (main.cpp lends the mouse to the orbit camera while it is
    // held, and the aim HOLDS). Held state, written every frame; the v13
    // rest-edge horizon law cancels on it exactly as the aeroplane's does —
    // rolling the carried frame under a camera the pilot is orbiting by hand
    // would turn the whole picture under him. Defaults false, so a caller that
    // never writes it (the unit tests) is simply never in freelook.
    bool freelook = false;
};

// Why the flight ended. `Target` is the one the whole mechanic exists for.
enum class StingEnd : int {
    None = 0,
    Target,        // rammed an enemy airframe (the sweep retired the "round")
    SelfDestruct,  // the two-large-turns law
    Battery,       // the clock
    Ground,        // flew it into the world
    Manual,        // P pressed in flight
};

struct StingState {
    bool active = false;
    sim::SimState prev{};
    sim::SimState curr{};
    // ★ THE AIM IS THE PLANE'S OWN CARRIER: an input::AimFrame, parallel-
    // transported every tick and rotated by the raw mouse at the plane's own
    // cp.aim_sensitivity — so the reticle circle, the chase, the muscle
    // memory are all the aircraft's. prev_up is the transport's memory.
    input::AimFrame aim{};
    glm::dvec3 prev_up{0.0, 1.0, 0.0};
    // The cascade's internal state — control::reset() at every launch (a
    // fresh drone must not inherit a previous flight's integrators).
    control::Internal ctl{};
    double battery_s = 0.0;   // counts down; <= 0 ends the flight
    int turns_used = 0;
    double turn_dwell = 0.0;  // seconds the g-latch has been loaded
    bool turn_latched = false;
    // ★ THE PLANE'S OWN REST-EDGE HORIZON LAW (Chad 2026-09-10: "I need the
    // sting's horizon to auto-rotate just like for the airplane"). Same code,
    // same cp.horizon_recovery_* dials, no new knobs — see app/rest_horizon.h.
    // Zeroed at every launch by sting_launch's `st = StingState{}`.
    RestHorizonState rest{};
    double warn_s = -1.0;     // >= 0 = the destruct warble is running
    int left = 5;             // launches remaining this match (main seeds it
                              // from sp.per_match at startup — ONE dial)
    StingEnd end = StingEnd::None;  // set on the ending tick; caller consumes
};

// ★ THE STING'S OWN AIRFRAME (fly-2): the aeroplane's params with the thrust
// and the authority-compression ramp scaled so the ruled 150-450 m/s band is
// reachable ("not accurate to real life but more fun") and stays
// controllable across it. A derived COPY built once at startup — config, not
// a kernel edit; control::step's plant inversion and sim::step fly the SAME
// object, so the H1 no-forked-params discipline holds by construction.
//  * T_max x6: parasitic drag grows ~V^2, so 450 m/s needs ~3.4x the thrust
//    that tops the plane at ~245 — 6x buys that plus the violent
//    acceleration off the stock ("zoom exxageratingly fast", his words).
//  * v_full x2.5 / v_redline x2: full control deflection through the cruise
//    band, still ramping (never below min_frac) at the 450 ceiling.
inline sim::AircraftParams sting_aircraft_params(
    const sim::AircraftParams& base) {
    sim::AircraftParams ap = base;
    ap.T_max = base.T_max * 6.0;
    ap.v_full = base.v_full * 2.5;
    ap.v_redline = base.v_redline * 2.0;
    return ap;
}

// The small rotation vector of a delta quaternion (angle*axis) — the aim's
// own mouse-induced rotation, feeding the cascade's aim-rate feedforward the
// same way app::tick's bracket does. Sign-canonicalized (q and -q are the
// same rotation).
inline glm::dvec3 sting_rot_vec(const glm::dquat& dq) {
    const bool neg = dq.w < 0.0;
    const glm::dvec3 v = neg ? -glm::dvec3(dq.x, dq.y, dq.z)
                             : glm::dvec3(dq.x, dq.y, dq.z);
    const double w = neg ? -dq.w : dq.w;
    const double s = glm::length(v);
    if (s < 1e-12) return glm::dvec3(0.0);
    return v * (2.0 * std::atan2(s, w) / s);
}

// Seed the drone off the stock: at the man's shoulder, nose on the aim,
// wings level to the local horizon, launch_speed along the aim. last_vhat
// seeded so the plant's velocity-direction guard has a valid ray from tick 1.
inline void sting_launch(StingState& st, const glm::dvec3& man_pos,
                         const glm::dvec3& aim, const StingParams& sp) {
    const glm::dvec3 lup = glm::normalize(man_pos);
    st = StingState{};  // fresh flight...
    st.battery_s = sp.battery_s;
    sim::SimState s{};
    s.position = man_pos + lup * sp.launch_up_m + aim * 1.0;
    s.velocity = aim * sp.launch_speed;
    s.last_vhat = aim;
    // Basis: nose = -Z on the aim, up as close to local-up as the aim allows.
    glm::dvec3 upv = lup - glm::dot(lup, aim) * aim;
    const double ul = glm::length(upv);
    upv = ul > 1e-6 ? upv / ul
                    : glm::normalize(glm::cross(aim, glm::dvec3{1, 0, 0}));
    glm::dmat3 basis;
    basis[0] = glm::cross(aim, upv);  // +X right
    basis[1] = upv;                   // +Y up
    basis[2] = -aim;                  // -Z forward => +Z = -aim
    s.orientation = glm::normalize(glm::quat_cast(basis));
    s.throttle = 1.0;
    st.prev = s;
    st.curr = s;
    // The plane's own seeds: aim frame reseeded on the launch attitude (the
    // legitimate spawn seed, never the banned continuous rebuild), transport
    // memory primed, cascade internal fresh — no inherited integrators.
    st.aim.reseed(s.orientation, lup);
    st.prev_up = lup;
    st.ctl = control::reset();
    st.active = true;
}

// ...except the launch counter, which belongs to the MATCH, not the flight.
// (sting_launch resets the struct; the caller re-seats `left` — this helper
// makes the pairing impossible to forget.)
inline void sting_launch_counted(StingState& st, const glm::dvec3& man_pos,
                                 const glm::dvec3& aim, const StingParams& sp) {
    const int left = st.left;
    sting_launch(st, man_pos, aim, sp);
    st.left = left - 1;
}

// ★ THE TWO-LARGE-TURNS LAW (Chad's ruling; the g/dwell dials are plan
// defaults his eye retunes). A "large turn" is a HYSTERETIC EVENT, not a
// g-tick: |n-1| above turn_g loads a dwell; holding it turn_dwell_s latches
// ONE turn; the latch re-arms only after the load falls below turn_release_g.
// Factored out of sting_step so a test can drive it with a synthetic n
// sequence instead of having to fly a real 2.5 g turn through the plant.
// WRONG IMPLEMENTATION this leg is red under: counting ticks above the
// threshold (every hard turn then costs MANY "turns") or comparing without
// the release hysteresis (one long turn re-latches at the dwell boundary).
inline void sting_turn_tick(StingState& st, const StingParams& sp, double n,
                            double dt) {
    const double load = std::abs(n - 1.0);
    if (!st.turn_latched) {
        if (load > sp.turn_g) {
            st.turn_dwell += dt;
            if (st.turn_dwell >= sp.turn_dwell_s) {
                st.turn_latched = true;
                ++st.turns_used;
            }
        } else {
            st.turn_dwell = 0.0;
        }
    } else if (load < sp.turn_release_g) {
        st.turn_latched = false;
        st.turn_dwell = 0.0;
    }
    // turns_max <= 0 = the destruct is OFF (fly-4's ruling): turns are still
    // COUNTED (the HUD/instrument keeps its number) but nothing ever arms.
    if (sp.turns_max > 0 && st.turns_used >= sp.turns_max && st.warn_s < 0.0)
        st.warn_s = sp.warn_s;
}

// One fixed tick of the flight. Consumes cmd's one-shot fields on the tick
// they apply. Returns the end reason on the ENDING tick (st.active goes
// false); StingEnd::None on every other tick. FX and mode glue are the
// caller's.
inline StingEnd sting_step(StingState& st, StingCmd& cmd, const StingParams& sp,
                           const sim::AircraftParams& ap,
                           const control::ControllerParams& cp,
                           const sim::Environment* env, double dt) {
    if (!st.active) return StingEnd::None;

    const auto finish = [&](StingEnd why) {
        st.active = false;
        st.end = why;
        return why;
    };

    // The one-shot commands first: a detonate press must not fly one more
    // tick, and main's terrain verdict is already a tick old.
    if (cmd.detonate) {
        cmd.detonate = false;
        return finish(StingEnd::Manual);
    }
    if (cmd.ground_kill) {
        cmd.ground_kill = false;
        return finish(StingEnd::Ground);
    }

    // ★★★ THE PLANE'S OWN AIM PATH (Chad's fly ruling: "mouse aim circle and
    // it have a cascade"). Transport the carried frame across this tick, then
    // consume the frame's mouse delta at the plane's own sensitivity —
    // nothing smoothed, nothing camera-referenced, the §9.1 discipline
    // verbatim. The bracket reads back the rotation actually applied for the
    // cascade's aim-rate feedforward (the S-aimff responsiveness rung).
    const glm::dvec3 up = sim::local_up(st.curr.position);
    st.aim.transport(st.prev_up, up);
    st.prev_up = up;
    bool aim_moved = false;
    glm::dvec3 aim_rate{0.0};
    if (cmd.dx != 0.0 || cmd.dy != 0.0) {
        const glm::dquat q_pre = st.aim.q;
        st.aim.apply_mouse(cmd.dx, cmd.dy, cp.aim_sensitivity);
        aim_rate = sting_rot_vec(st.aim.q * glm::inverse(q_pre)) / dt;
        aim_moved = true;
        cmd.dx = cmd.dy = 0.0;
    }

    // ★ THE REST-EDGE HORIZON RECOVERY, the aeroplane's own law on the
    // drone's carried frame (Chad 2026-09-10). The drone is ALWAYS airborne
    // (a grounded Sting is a finished flight) so the plane's !grounded arm is
    // structurally true here; the freelook arm is cmd.freelook, main's SPACE
    // orbit. The legal reads are the same: the mouse (aim_moved) and the
    // flight path (st.curr.velocity, this tick's pre-step velocity, exactly as
    // the plane reads it). A roll about the aim's own forward never moves the
    // aim direction, so the cascade below is untouched by it.
    rest_horizon_tick(st.aim, st.rest, cp.horizon_recovery_rate > 0.0 &&
                                           !cmd.freelook,
                      aim_moved, st.curr.velocity, up, cp, dt);

    // The REAL cascade -> the frozen plant. Throttle is passthrough by SPEC
    // §9.8, so the app-side speed hold below is legal — the same law the
    // bandits' autothrottle uses, at the Sting's own band.
    //
    // ★ fly-4 ("can the plane slow fairly fast?"): the error is SIGNED now —
    // above the setpoint the throttle drives to ZERO instead of idling at
    // base (the old max(0,·) kept 40% power on while "slowing") — and past
    // a 15 m/s overspeed the FLAPS deploy as the plant's own documented
    // speed brake (over-speed flaps keep their drag while the lift washes
    // out — MB-flaps' design, used exactly as designed). Both retract the
    // moment the speed is back inside the band.
    const double v = glm::length(st.curr.velocity);
    const double v_cmd = std::clamp(cmd.speed_cmd > 0.0 ? cmd.speed_cmd
                                                        : sp.speed_cruise,
                                    sp.speed_min, sp.speed_max);
    control::Input ci;
    ci.target_dir_world = st.aim.forward();
    ci.throttle = std::clamp(
        sp.throttle_base + sp.throttle_p * (v_cmd - v), 0.0, 1.0);
    ci.flap_cmd = v > v_cmd + 15.0 ? 1.0 : 0.0;
    ci.aim_moved = aim_moved;
    ci.aim_rate_world = aim_rate;
    const control::Output o =
        control::step(st.curr, ci, st.ctl, ap, cp, env, dt);
    st.ctl = o.internal;

    st.prev = st.curr;
    st.curr = sim::step(st.curr, o.inputs, ap, env, dt);

    // The battery clock.
    st.battery_s -= dt;
    if (st.battery_s <= 0.0) return finish(StingEnd::Battery);

    // The two-large-turns law (sting_turn_tick above). The instrument is the
    // cascade's OWN telemetry true-n — the same number the plane's G-meter
    // reads, computed once by the code that already owns it.
    sting_turn_tick(st, sp, o.telem.load_factor, dt);
    if (st.warn_s >= 0.0) {
        st.warn_s -= dt;
        if (st.warn_s <= 0.0) return finish(StingEnd::SelfDestruct);
    }
    return StingEnd::None;
}

// Build the one-"round" pool for the ram sweep: the drone's own tick segment
// AS a projectile, so combat_tick's swept hit, ke_damage, FX, kill credit,
// respawn/inert pass — the whole kill loop — is reused verbatim with zero new
// collision code. pool[0].active == false afterwards IS the detonation.
inline void sting_ram_round(const StingState& st, const StingParams& sp,
                            std::vector<weapon::Projectile>& pool) {
    pool.clear();
    weapon::Projectile p;
    p.prev_pos = st.prev.position;
    p.pos = st.curr.position;
    p.vel = st.curr.velocity;
    p.damage = sp.ram_damage;
    p.v_ref = sp.ram_v_ref;
    p.active = true;
    pool.push_back(p);
}

// The app-side aggregate threaded through app::tick (the FlakWorld pattern:
// defaulted null everywhere => a strict superset).
struct StingWorld {
    StingParams sp{};
    StingState st{};
    StingCmd cmd{};
    std::vector<weapon::Projectile> pool{};  // the ram sweep's one-round pool
    // The Sting's own derived airframe (sting_aircraft_params) — set ONCE by
    // main at startup from the loaded plane params. Zero-defaulted like
    // AircraftParams itself: an unset copy fails loudly, never flies
    // plausibly. ⚠ The ram sweep's combat_tick still takes the PLANE's ap —
    // that arg only feeds the DRONE respawn path, which must stay the
    // bandits' own airframe.
    sim::AircraftParams ap{};
};

}  // namespace app
