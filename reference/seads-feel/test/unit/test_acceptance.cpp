// Section 6 — the acceptance gate (SPEC §14 objectives 1–5; HARNESS §5).
//
// The acceptance-tier sweeps Sections 4/5 CONSCIOUSLY DEFERRED here (CLAUDE.md
// ## Deferred): AT-11 full AoA protection sweep and AT-16 banked-tracking |β|
// bound; plus the objective-level checks the build order (§15.6) assigns to
// this gate — circumnavigation, poles, the closed-loop holonomy pair, and
// clean rebirth (AT-13). All closed-loop through the REAL spherical sim::step()
// (HARNESS §3: a flat plant misgrades on-sphere corrective rotation) or, for
// the camera, the shipped render poses.
//
// The core mechanisms are already unit-pinned (test_cascade, test_aim_frame,
// test_override, test_freelook). These verify the ASSEMBLED behavior over long
// flights: envelope protection holds, a lap closes with no seam, the poles are
// non-singular, the carried aim frame is holonomy-correct on the real
// trajectory, and a respawn is clean. Untuned starting gains (Section 7 tunes
// for feel) — so bounds are envelope/sanity bounds, not feel targets.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/gtc/quaternion.hpp>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "control/transport.h"
#include "input/aim_frame.h"
#include "render/camera.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

bool finite3(const glm::dvec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool finite_state(const sim::SimState& s) {
    return finite3(s.position) && finite3(s.velocity) &&
           finite3(s.angular_vel) && std::isfinite(s.orientation.w) &&
           std::isfinite(s.orientation.x) && std::isfinite(s.orientation.y) &&
           std::isfinite(s.orientation.z);
}
bool finite_pose(const render::CameraPose& p) {
    return finite3(p.eye) && finite3(p.target) && finite3(p.up);
}

double stall_alpha() { return kAp.Cl_max / kAp.Cl_alpha; }  // plant stall AoA

}  // namespace

// ===========================================================================
// AT-11 — AoA protection sweep (HARNESS §5). full-up at low speed + injected
// inverted-stall: no departure; the G floor AND ceiling bound the pull; the
// limiter is inactive at cruise AoA; an override CAN exceed it (by design).
// ===========================================================================

// (a) No departure: a sustained hard pull holds the FILTERED AoA at/below
// aoa_max and the RAW AoA below the plant stall cap — the aircraft turns hard
// but never stalls or departs. (The clamp BINDING decisively is proven by the
// override contrast in (e); the low-speed energy-exhaustion apex is AT-17's
// domain, not a protection failure.)
// PREMISE RE-SCOPE (MB, 2026-07-08 — the AT-15 "calibrated to the untuned
// table" class): the old CHECK_FALSE(saw_ballistic) assumed "energy to
// sustain it," which was only true because the WEAK integrator (K_wi_pitch
// 20k) under-attained the commanded pull and thereby conserved speed. With a
// step-1-tuned integrator the pull is genuinely HELD at the AoA limiter, and
// 15 s of Cl~Cl_max induced drag (Cd_i ~ 6.5x Cd0) exhausts any starting
// energy — the combat egg working, not a departure. Departure detection is
// OWNED by the AoA bounds + finite-state + airborne below; the energy apex
// is AT-17's. In exchange the leg gains a stronger premise the old one
// lacked: the limiter must actually BIND (filtered AoA reaches near aoa_max)
// — a protection mutant that blocks the pull entirely used to pass the old
// no-ballistic check FOR FREE.
TEST_CASE("AT-11: sustained full pull holds AoA below stall (no departure)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 130.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();

    // Command a hard 60 deg pull and HOLD it (a fixed world aim above the nose;
    // the controller drives into the protection).
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(rad(60.0), right) * cl.aim);

    double aoa_f_max = 0.0, aoa_raw_max = 0.0;
    for (int i = 0; i < 1800; ++i) {  // 15 s
        const control::Telemetry t = cl.tick(1.0, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        // The AoA contract is graded only while the limiter is LIVE — in
        // BALLISTIC the tail-slide alpha lies and the limiter is gated off
        // by design (SPEC §9.6.4); the energy apex is AT-17's domain.
        if (!t.ballistic) {
            aoa_f_max = std::max(aoa_f_max, t.aoa_filtered);
            aoa_raw_max = std::max(aoa_raw_max, std::abs(t.extracted.alpha));
        }
    }
    std::printf(
        "[AT-11a] aoa_f_max=%.3f  aoa_raw_max=%.3f  aoa_max=%.3f  stall=%.3f "
        "(deg)\n",
        deg(aoa_f_max), deg(aoa_raw_max), deg(kCp.aoa_max), deg(stall_alpha()));
    CHECK(aoa_f_max <=
          kCp.aoa_max + rad(1.5));  // limiter held the filtered AoA
    CHECK(aoa_raw_max <=
          stall_alpha() + rad(2.0));  // never past the plant stall
    // The pull actually DROVE INTO the protection (the re-scope's new
    // premise): a mutant that blocks the pull outright passed the old
    // no-ballistic clause for free.
    // 0.8 -> 0.7 (MB 2026-07-08, T_max 9000): the stronger thrust holds more
    // speed through the pull, so the G-clamp shares the limiting with the AoA
    // ceiling and the peak filtered AoA sits at ~0.76*aoa_max — still deep in
    // pushback territory (the clamp ceiling K_aoa*(aoa_max - aoa_f) is well
    // below the unclamped demand there). The premise stays: a pull-blocking
    // mutant reads cruise AoA ~0.1*aoa_max and FAILS.
    CHECK(aoa_f_max > 0.7 * kCp.aoa_max);
    CHECK(sim::altitude(cl.state.position, kAp) > 1000.0);  // still airborne
}

// (b/c) G floor AND ceiling bound the protection itself (SPEC §9.3b: "the G
// floor AND ceiling applied LAST so protection itself can never command beyond
// the load limits"). The push-gate keeps NORMAL flight well inside the budget,
// so the clamps only bind where protection commands a hard recovery: inject an
// over-stall FILTERED AoA and the pushback saturates — then the G-limits, not
// the pushback gain, set the emitted pitch rate. Structural (one tick, from a
// small non-deadzone aim so the pointing term itself is negligible) so the
// clamp value is read directly, not inferred from overdamped closed-loop G.
namespace {
// Pitch curvature-feedforward component in the body frame (the ff added to
// every mode, SPEC §9.3) — computed from raw state so the test subtracts it
// off the emitted omega_des.x to isolate the clamped pointing pitch. Divisor
// is the ACTUAL orbital radius |position| (= R + altitude), matching the
// controller's corrected ff (radius-error fix, auto/feel-research): at the
// 4000 m spawn this is R/(R+4000) = 79% of the old /R crumb — the honest
// isolation of the pointing term against the shipped ff.
double ff_pitch(const sim::SimState& st) {
    const glm::dvec3 up = sim::local_up(st.position);
    const glm::dvec3 ffw =
        glm::cross(up, st.velocity) / glm::length(st.position);
    return sim::body_dir_of(st.orientation, ffw).x;
}
// The PRE-fix ff oracle: divides by the BAKED planet radius kAp.R instead of
// the actual orbital radius |position|. Used ONLY as a self-guard cross-check
// so a mis-derived (baked-R) oracle in the AT-11 floor/ceiling legs cannot
// silently pass — the two must differ by exactly R/(R+h) (mirrors the
// test_cascade ff-radius ratio pin).
double ff_pitch_bakedR(const sim::SimState& st) {
    const glm::dvec3 up = sim::local_up(st.position);
    const glm::dvec3 ffw = glm::cross(up, st.velocity) / kAp.R;
    return sim::body_dir_of(st.orientation, ffw).x;
}
}  // namespace

TEST_CASE("AT-11: positive over-stall pushback is clamped by the n_min floor") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const double V = 200.0;
    const sim::SimState s0 = harness::level_state(kAp, V, 4000.0, up, heading);
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    control::Input in;
    in.target_dir_world = glm::angleAxis(rad(2.0), right) * nose;  // small pull
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    internal.aoa_filtered =
        rad(40.0);  // >> aoa_max: pushback commands hard DOWN

    const control::Output o =
        control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
    const double cpt = o.telem.extracted.cos_phi_theta;
    const double w_min = (kCp.n_min - cpt) * kAp.g / std::max(V, kCp.v_min);
    const double pointing = o.telem.omega_des.x - ff_pitch(s0);
    // Oracle self-guard: the ff we subtract off must be (i) non-vacuous and
    // (ii) genuinely the |position| divisor, not the baked kAp.R. If the oracle
    // silently reverted to /R (a mis-derivation), the isolated `pointing` would
    // be wrong yet the floor CHECK could still pass by coincidence — so pin the
    // ff itself against the R/(R+h) geometry ratio (the test_cascade ff-radius
    // pin, here at the h=4000 spawn). ff_pitch = ff_bakedR * R/(R+h).
    REQUIRE(std::abs(ff_pitch(s0)) > 0.0);  // non-vacuity premise
    CHECK(ff_pitch(s0) / ff_pitch_bakedR(s0) ==
          Catch::Approx(kAp.R / (kAp.R + 4000.0)).epsilon(1e-9));
    // The clamp reads the FILTERED AoA this tick (the low-pass moves 40 -> ~33
    // deg on the first tick); the pushback it commands is still a hard
    // nose-DOWN well past the floor.
    const double pushback = kCp.K_aoa * (kCp.aoa_max - o.telem.aoa_filtered);
    std::printf(
        "[AT-11 floor] pointing pitch=%.4f  w_min_pitch=%.4f  pushback=%.4f\n",
        pointing, w_min, pushback);
    CHECK(pushback < w_min);  // premise: wants < floor
    CHECK(pointing ==
          Catch::Approx(w_min).margin(1e-6));  // floored, not pushback
}

TEST_CASE(
    "AT-11: inverted over-stall pushback is clamped by the n_max ceiling") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const double V = 200.0;
    const sim::SimState s0 = harness::level_state(kAp, V, 4000.0, up, heading);
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    control::Input in;
    in.target_dir_world =
        glm::angleAxis(-rad(2.0), right) * nose;  // small push
    in.throttle = 0.7;
    control::Internal internal = control::reset();
    internal.aoa_filtered = -rad(40.0);  // inverted stall: pushback commands UP

    const control::Output o =
        control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
    const double cpt = o.telem.extracted.cos_phi_theta;
    const double w_max = (kCp.n_max - cpt) * kAp.g / std::max(V, kCp.v_min);
    const double pointing = o.telem.omega_des.x - ff_pitch(s0);
    // Oracle self-guard (see the floor leg): the subtracted ff must be
    // non-vacuous AND the |position| divisor, differing from the baked-R
    // version by exactly R/(R+h) — a mis-derived (baked-R) oracle cannot pass.
    REQUIRE(std::abs(ff_pitch(s0)) > 0.0);  // non-vacuity premise
    CHECK(ff_pitch(s0) / ff_pitch_bakedR(s0) ==
          Catch::Approx(kAp.R / (kAp.R + 4000.0)).epsilon(1e-9));
    // Inverted-stall pushback commands a hard nose-UP (past the ceiling), read
    // off the FILTERED AoA the clamp actually consumes this tick.
    const double pushback =
        -kCp.K_aoa * (kCp.aoa_max_neg + o.telem.aoa_filtered);
    std::printf(
        "[AT-11 ceil] pointing pitch=%.4f  w_max_pitch=%.4f  "
        "pushback=%.4f\n",
        pointing, w_max, pushback);
    CHECK(pushback > w_max);  // premise: wants > ceiling
    CHECK(pointing ==
          Catch::Approx(w_max).margin(1e-6));  // ceilinged, not pushback
}

// (b') G floor, CLOSED-LOOP VALUE (P2a; fixreport §P2a). The one-tick (b) test
// above pins clamp PRECEDENCE: the emitted omega_des.x is CLAMPED to w_min (the
// G-limit wins over the AoA pushback, "applied LAST"). What (b) CANNOT pin — it
// recomputes w_min with the SAME (n_min − cosPhiTheta)·g/V the controller uses,
// so instrument and mechanism share the expression — is that the clamp LAW maps
// to the right ACHIEVED load factor: a spec-level sign/credit error (e.g.
// n_min + cosPhiTheta) passes (b). This leg closes that gap by flying the
// sustained floor and reading the INDEPENDENT true-n instrument
// (sim::load_factor = q·S·Cl(alpha)/mg, aero.h — real AoA→lift, NOT the
// controller's rate formula).
//
// The achieved floor is NOT bare n_min: the curvature feedforward omega_ff =
// (local_up × v)/R is added AFTER the clamps (SPEC §9.3, bypasses them), so the
// sustained G is n_min + V·ff_x/g — exactly CQ1's flat-cosPhiTheta-credit bias
// (the ≤0.35 g the ruling left OUT of the clamp), read back by the very true-n
// instrument CQ1 built. The oracle is that DERIVED setpoint, anchored to the
// config n_min plus the raw-state ff (no copy of (n_min − cpt) in it), so a
// clamp-formula sign/credit mutant — which shifts the ACHIEVED rate — fails it
// (n_min+cpt → −3.9 vs oracle −2.75, mutation-verified; the (b) test stays
// green under it). Division of labor: (b) uniquely owns the
// cosPhiTheta→|cosPhiTheta| mutant class (identical to correct code at cpt<0,
// invisible HERE by construction; (b) reads −2g/V vs its recomputed −4g/V).
//
// Realizability (extends the S6 "overdamped closed-loop barely reaches n_max"
// lesson to the floor): the floor binds ONLY on the AoA-pushback path (a deep
// pointing demand rolls THROUGH inverted via the push-gate, never a sustained
// floor push), and plant_invert omits the plant damping, so holding w_min needs
// the integrator to source damp·q_eff·w_min of torque. At cpt=−1 the budget
// halves to |n_min+1| and FITS under K_wi·integ_cap (upright it does not —
// integ caps ~30% short forever, achieved |n| plateaus ~2.4). So: enter
// inverted (cpt=−1: budget fits, cpt flat at the pole, V quasi-stationary),
// preload the integrator to steady state (skip the ~12 s integral pole), hold
// an over-stall aoa_filtered each tick (keep the pushback saturated, as (b)
// injects it once).
TEST_CASE("AT-11: sustained G floor settles true-n at the derived setpoint") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // V0 tracks the §7 performance tune (n_min -4->-10, integ_cap 0.5->2.0).
    // The integ preload that sources the floor's damping torque scales ~
    // (n_min+1)*V, so the DEEPER floor needs the BIGGER integ_cap to fit — and
    // with headroom to spare V0 goes UP (200-ish), which also keeps the deep
    // -10 G lift-achievable (Cl = 10*m*g/(q*S) ~ 1.3 < 1.8 at 170 m/s) and
    // keeps the speed from bleeding below where the pushback out-negatives the
    // floor. The preload check below is the tripwire: a deeper n_min or a
    // smaller integ_cap trips it LOUD (raise integ_cap or drop V0) instead of
    // silently re-drooping.
    const double V0 = 170.0;
    // Altitude 6000 -> 3500 (MB-atm 2026-07-08): the quasi-static preload and
    // achievable-lift arithmetic below assume CONSTANT density; 6 km now sits
    // mid-taper (atm_frac ~ 0.69, and drifting as the maneuver descends), so
    // the preloaded hold drooped ~2.7 g. At 3500 m the whole maneuver (the
    // -8 g push curls with radius V^2/8g ~ 370 m, dipping < ~750 m) stays
    // BELOW taper_alt: atm_frac == 1 exactly and every formula here is
    // unchanged. The altitude-thinning physics itself is pinned by AT-18a's
    // 7 km leg + the MB-atm plant legs, not this floor diagnostic.
    sim::SimState s0 = harness::flight_state(kAp, V0, 3500.0, up, heading,
                                             rad(180.0), 0.0, 0.0);  // cpt = -1
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    // Steady-state integrator preload: sources the plant damping torque for the
    // floor rate (plant_invert inverts only the authority term). w_min0 at
    // cpt = -1 is (n_min + 1)*g/V; the +ff term leaves only tracking residual.
    // S-dampff (SPEC §0): the damping FEEDFORWARD now sources damp_ff_pitch of
    // that torque directly, so the integrator's steady share scales by
    // (1 - damp_ff_pitch) — at 1.0 the preload is ZERO (preloading the old
    // full value would DOUBLE-source the torque, over-rotate the hold off the
    // cpt = -1 pole, and break the floor-binding premise below — observed at
    // the flip). Config-relative: tracks any damp_ff_pitch, including the
    // 0.0 legacy A/B arm.
    const double w_min0 = (kCp.n_min + 1.0) * kAp.g / std::max(V0, kCp.v_min);
    const double qeff0 = sim::q_eff(sim::q_dyn(kAp.rho, V0), kAp);
    cl.internal.integ[0] = (1.0 - kCp.damp_ff_pitch) * kAp.damp_pitch * qeff0 *
                           (w_min0 + ff_pitch(s0)) / kCp.K_wi_pitch;
    // Premise: the torque budget FITS under the cap. A n_min deepening or an
    // integ_cap cut trips this LOUD (drop V0) instead of silently re-drooping.
    // NOTE (S-dampff): at damp_ff_pitch = 1 the preload is 0 and this REQUIRE
    // is VACUOUS — the tripwire retires WITH the constraint it guarded (the
    // n_min <-> integ_cap coupling is DEAD on a fully ff'd pitch axis; the
    // n_min ladder re-derives against lift/Cl_max and the pilot's spine).
    REQUIRE(std::abs(cl.internal.integ[0]) < 0.95 * kCp.integ_cap);

    double max_dev = 0.0, n_last = 0.0;
    int samples = 0;
    for (int i = 0; i < 481; ++i) {  // ~4.0 s
        const glm::dvec3 right = cl.state.orientation * glm::dvec3{1, 0, 0};
        const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0, 0, -1};
        // F2: pure-PITCH aim (about body-right, zero lateral) at 6 deg — just
        // above blend_lo (5 deg). Why 6 not 2: F2 un-gated the wings-hold,
        // which would now ROLL this artificially-held inverted floor back to
        // UPRIGHT (the whole point of F2), destroying the
        // sustained-inverted-floor premise this PITCH/G-floor diagnostic needs.
        // Keeping err >= blend_lo stops the auto-level decay of held_bank; a
        // pure-pitch aim gives bank_error == 0 (no maneuver roll); and pinning
        // held_bank to the current fold-safe bank makes roll_hold_demand
        // exactly 0 — so roll stays ~0 and the attitude holds, while the floor
        // (pushback-clamped, so aim-magnitude-independent) is unchanged. A
        // fixture pin like the integ preload / aoa_filtered hold; the
        // roll-to-upright itself is pinned by the F2 leg in
        // test_instructor_tick.
        cl.aim = glm::normalize(glm::angleAxis(rad(6.0), right) * nose);
        cl.internal.aoa_filtered = rad(40.0);  // hold the pushback saturated
        const control::Extracted exq =
            control::extract(cl.state, cl.internal.last_vhat, kAp.v_dir_eps);
        cl.internal.held_bank =
            control::unfold_bank(exq.phi, exq.cos_phi_theta);
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        if (i >= 300) {  // settled window t in [2.5, 4.0] s
            const double V = t.extracted.speed;
            const double cpt = t.extracted.cos_phi_theta;
            const double w_min_now =
                (kCp.n_min - cpt) * kAp.g / std::max(V, kCp.v_min);
            const double pushback = kCp.K_aoa * (kCp.aoa_max - t.aoa_filtered);
            REQUIRE(pushback <
                    w_min_now);  // the FLOOR is the binding constraint
            const double n_ref = kCp.n_min + V * ff_pitch(cl.state) / kAp.g;
            max_dev = std::max(max_dev, std::abs(t.load_factor - n_ref));
            n_last = t.load_factor;
            ++samples;
        }
    }
    std::printf("[AT-11 floor CL] samples=%d max|n-n_ref|=%.3f n_last=%.3f\n",
                samples, max_dev, n_last);
    REQUIRE(samples > 100);
    // The achieved sustained G tracks the DERIVED floor setpoint (n_min + the
    // curvature credit) — the value the one-tick precedence test cannot see.
    // BOUND RELAXED 0.05->0.10*|n_min| for the §7 deep floor (n_min -4->-8): a
    // DEEP floor is no longer quasi-static — at ~-8 G the airframe maneuvers
    // out of the preloaded hold (cpt drifts off the pole, V bleeds), so the
    // achieved floor droops ~0.6 g below the derived setpoint instead of
    // tracking it to 0.15 g like the shallow -4 floor did. That droop is
    // imperfect tracking of a non-quasi-static floor, NOT a sign/credit error:
    // the setpoint-formula mutations this leg guards (n_min+cpt, drop-cpt)
    // shift the achieved floor by ~1.2-2 g (2*cpt), still far outside the
    // relaxed 0.8 g band. RED-TEAM this relaxation (HARNESS §6) alongside the
    // §7 performance tune.
    CHECK(max_dev < 0.10 * std::abs(kCp.n_min));  // ~0.8 g (deep-floor droop)
    // ...and it is a genuinely DEEP negative-G floor (past the shallow 6-deg
    // push ~ -1.3 g, the AT-15 leg) — "near the floor", not merely "negative".
    CHECK(n_last < kCp.n_min + 1.0);  // more negative than -7 g
}

// (d) Inactive at cruise AoA: at trim (AoA ~1.5 deg) the pushback term is far
// from binding, and a gentle pull is NOT suppressed — the limiter is dormant.
TEST_CASE("AT-11: AoA limiter is inactive at cruise AoA") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    control::Telemetry t;
    for (int i = 0; i < 600; ++i) t = cl.tick(thr, kAp, kCp);  // settle 5 s
    std::printf("[AT-11d] cruise aoa_filtered=%.3f deg  aoa_max=%.3f deg\n",
                deg(t.aoa_filtered), deg(kCp.aoa_max));
    CHECK(t.aoa_filtered < 0.5 * kCp.aoa_max);  // far from the limit
    // A gentle 5 deg pull commands pitch-UP — the limiter is not clamping it.
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(rad(5.0), right) * cl.aim);
    const control::Telemetry pull = cl.tick(thr, kAp, kCp);
    CHECK(pull.omega_des.x > 0.0);  // pitch-up allowed (limiter dormant)
}

// (e) Override STAYS IN the envelope (S7-ovr): a held pitch override at low
// speed is driven to its clamped in-envelope rate through the SAME AoA/G clamp
// the instructor uses, so its RAW AoA stays bounded by aoa_max — just like the
// SAME low-speed pull under the instructor. This INVERTS the frozen "override
// can exceed the AoA limit": the consent-to-leave-the-envelope bypass is gone,
// so a held override now carves at the limit instead of blowing through it.
TEST_CASE("AT-11 (S7-ovr): keyboard override STAYS within the AoA limit") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 90.0, 3000.0, up, heading, &thr);

    // Instructor: hard pull aim, no override — AoA stays bounded by the
    // limiter.
    harness::ClosedLoop guard(s0, glm::dvec3{0.0, 0.0, -1.0});
    guard.aim_nose();
    const glm::dvec3 gr = guard.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    guard.aim = glm::normalize(glm::angleAxis(rad(60.0), gr) * guard.aim);
    double guard_aoa_max = 0.0;
    for (int i = 0; i < 480; ++i) {
        const control::Telemetry t = guard.tick(1.0, kAp, kCp);
        REQUIRE(finite_state(guard.state));
        guard_aoa_max = std::max(guard_aoa_max, t.extracted.alpha);
    }

    // Override: hold full pitch-up (+1) — now ALSO respects protection (the aim
    // rides the nose, the held axis is driven to its in-envelope rate).
    harness::ClosedLoop ovr(s0, glm::dvec3{0.0, 0.0, -1.0});
    ovr.aim_nose();
    ovr.hold_override(0, +1.0);  // pitch axis, nose-up
    double ovr_aoa_max = 0.0;
    for (int i = 0; i < 480; ++i) {
        const control::Telemetry t = ovr.tick(1.0, kAp, kCp);
        REQUIRE(finite_state(ovr.state));
        ovr_aoa_max = std::max(ovr_aoa_max, t.extracted.alpha);
    }
    std::printf("[AT-11e] guard_aoa_max=%.3f  ovr_aoa_max=%.3f  aoa_max=%.3f\n",
                deg(guard_aoa_max), deg(ovr_aoa_max), deg(kCp.aoa_max));
    CHECK(guard_aoa_max <= kCp.aoa_max + rad(2.0));  // instructor: contained
    CHECK(ovr_aoa_max <= kCp.aoa_max + rad(3.0));    // override: ALSO contained
}

// ===========================================================================
// AT-6 — heavier at speed (HARNESS §5; SPEC §16 CQ1 revisit trigger; HARNESS §8
// tuning-step-3 instrument). The felt "weight" of a pull is the speed-scaling
// of the achievable pitch rate: the G-clamp w_max = (n_max - cosPhiTheta)*g/V
// shrinks as V grows, so the SAME stick offset yields a LOWER commanded pitch
// rate at higher speed. Without a mechanized check that achieved omega tracks
// the ω_max(V) prediction, a wrong speed-scaling in the felt heaviness is
// indistinguishable from a feel problem once Section-7 gains make the ceiling
// bind in ordinary pulls (the definitional phantom-chase, CLAUDE.md).
//
// Structural: one tick from a V-trimmed level state with a pure 20 deg pull, so
// the emitted pointing pitch is exactly the braking-law min (linear / sqrt-
// brake / G-clamp) with the pointing/align terms unity and the AoA pushback
// dormant. The binding value is DERIVED from config + the tick's own telemetry
// (cosPhiTheta) and its three branches min'd exactly as sqrt_law does — never
// read back from the cascade. A V_clamp -> v_min fork (P1a) makes the G-clamp
// speed-independent (~20x too large, same at both speeds): the derived
// prediction still uses the true V, so both the value CHECKs and the ratio
// CHECK fail.
namespace {
// The pointing pitch the cascade emits for a pure `offset`-rad pull from a
// V-trimmed level state, paired with the value first principles predicts.
//
// v5 rung D (Chad 2026-07-23 arcade energy ruling): this oracle used to
// rebuild the braking law as a plain LINEAR term (K*elev) — a shape that
// predates seek_law (the cascade's real pursuit law is the step-floor +
// PARABOLIC rise K*a*(1+expo*a), sqrt-brake, w_max min). At the old n_max=16
// the small G-budget always made w_max the true binding branch regardless of
// that gap, so the plain-linear oracle never showed. n_max=32 doubled the
// budget and exposed it: the real cascade still saturates at w_max at BOTH
// speeds (measured V140 pointing 2.17225 == w_max 2.17225; V224 1.35764 ==
// w_max 1.35764 -- the "heavier at speed" behavior holds perfectly, ratio ==
// V-ratio), but the old oracle's linear branch now binds BELOW w_max and
// disagrees. Re-derived: predicted IS the config-derived w_max(V) -- the
// fixture is chosen (offset = 20 deg) specifically so the pull SATURATES,
// which is now an explicit premise (seek/brake both required strictly above
// w_max) rather than an assumption baked silently into "predicted".
struct At6Probe {
    double pointing;  // measured omega_des.x minus the curvature feedforward
    double w_max;      // the G-clamp branch (the speed-scaled term) -- pred
    double seek;       // seek_law's own seek branch (step-floor + parabolic)
    double brake;      // the sqrt-brake branch
};
At6Probe at6_pull(double V, double offset) {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, V, 4000.0, up, heading, &thr);
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    control::Input in;
    in.target_dir_world = glm::angleAxis(offset, right) * nose;  // pure pull up
    in.throttle = thr;
    control::Internal internal =
        control::reset();  // trim AoA; pushback dormant

    const control::Output o =
        control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);

    // elev == demand.x for a pure pitch offset (the AT-0 primitive the cascade
    // itself uses); rebuild seek_law's own branches from config -- deriving,
    // never copying, the binder.
    const glm::dvec3 demand =
        control::rotation_demand_body(s0.orientation, in.target_dir_world);
    const double elev = std::abs(demand.x);
    const double cpt = o.telem.extracted.cos_phi_theta;
    const double aB = kCp.k_b * sim::ang_accel_max_derived(
                                    kAp.c_pitch, kAp.I_pitch, V, 4000.0, kAp);
    const double w_max = (kCp.n_max - cpt) * kAp.g / std::max(V, kCp.v_min);
    const double seek = std::max(
        kCp.pursuit_step, kCp.K_theta * elev * (1.0 + kCp.pursuit_expo * elev));
    const double brake = std::sqrt(2.0 * aB * elev);

    return {o.telem.omega_des.x - ff_pitch(s0), w_max, seek, brake};
}
}  // namespace

TEST_CASE("AT-6: the same pull is heavier (lower pitch rate) at higher speed") {
    const double offset = rad(20.0);  // a firm pull, no lateral component
    const At6Probe lo = at6_pull(140.0, offset);
    const At6Probe hi = at6_pull(224.0, offset);

    std::printf(
        "[AT-6] V=140 pointing=%.5f w_max=%.5f seek=%.5f brake=%.5f | "
        "V=224 pointing=%.5f w_max=%.5f seek=%.5f brake=%.5f | ratio=%.4f "
        "(1/V=%.4f)\n",
        lo.pointing, lo.w_max, lo.seek, lo.brake, hi.pointing, hi.w_max,
        hi.seek, hi.brake, hi.pointing / lo.pointing, 140.0 / 224.0);

    // 0. Premise: the fixture's pull actually SATURATES the G-clamp at both
    //    speeds -- seek_law's own seek/brake branches sit strictly ABOVE
    //    w_max, so w_max is the true binding min (rung D exposed that this
    //    was previously an unstated assumption, not a checked one -- a
    //    future edit that gentles the fixture below saturation now trips
    //    this loud instead of silently mismatching #1 below).
    REQUIRE(lo.seek > lo.w_max);
    REQUIRE(lo.brake > lo.w_max);
    REQUIRE(hi.seek > hi.w_max);
    REQUIRE(hi.brake > hi.w_max);

    // 1. The emitted pitch rate IS the config-derived G-clamp w_max(V) at
    //    both speeds (never read back from the cascade).
    CHECK(lo.pointing == Catch::Approx(lo.w_max).margin(1e-9));
    CHECK(hi.pointing == Catch::Approx(hi.w_max).margin(1e-9));

    // 2. Heavier at speed: the higher-speed pull yields a STRICTLY lower pitch
    //    rate, and the ratio tracks the clamp's ~1/V scaling (cosPhiTheta
    //    differs slightly between the two trims, so allow a small band).
    CHECK(hi.pointing < lo.pointing);
    CHECK(hi.pointing / lo.pointing ==
          Catch::Approx(140.0 / 224.0).margin(0.03));
}

// ===========================================================================
// AT-12 — predictable energy (HARNESS §5; SPEC §14 obj.4; SPEC §16 the "tune
// drag with a number" plant requirement). A sustained max-rate turn for 10 s
// at full throttle: the closed-loop AT-12 maneuver (harness/instructor.h,
// shared with the `ctrl_fly` CSV recorder so the instrument and this gate
// describe the SAME flight).
//
// Two jobs, deliberately split (fix report P2c, handoff item 3):
//
//  (1) The INSTRUMENT / PRINTOUT — speed retention V_final/V_initial. This is
//      the Section-7 drag knob's readout, NOT gated: the retention TARGET is a
//      feel decision on a still-untuned airframe (the honest ledger — CLAUDE.md
//      Deferred). Gating it now would freeze a tuning target and re-create the
//      phantom-chase AT-6 guards against. Printed for the flight log.
//
//  (2) The GATE — "predictable" energy, mechanized tune-INDEPENDENTLY: over the
//      whole flight the plant's KINETIC-energy change reconciles, tick for
//      tick, with the work done by every ENERGY-BEARING config force — gravity
//      + thrust + drag. (Lift does exactly zero work: it is built ⟂ v̂ in
//      step.cpp, and here v ∥ v̂ since speed ≫ v_dir_eps, so lift·v ≡ 0 — SCOPE,
//      not a blind spot: a lift-magnitude/axis defect changes the TRAJECTORY,
//      never the energy the airframe keeps, so it is not an
//      energy-predictability failure. Lift wiring is pinned one-tick in
//      test_aero.cpp; turn-rate/AoA consequences are AT-11/AT-16's. This gate
//      is the ENERGY quantity v·F, and an energy defect is BY DEFINITION along
//      v.) The reconciliation is EXACT, not O(dt)-loose, because the plant's
//      per-tick linear work is recovered algebraically from its OWN output —
//      m·v·(v'−v) = m·v·a·dt — with the semi-implicit KE surplus ½m|v'−v|² thus
//      never entering; the config forces are evaluated at the identical
//      pre-tick state through sim/aero.h's single-source expressions. Plant ==
//      config ⇒ residual is machine-epsilon. A plant drag DEFECT — a forked
//      coefficient, a dropped induced term, a sign slip in step.cpp — makes the
//      applied force diverge from the documented Cd = Cd0 + k·Cl² model and the
//      residual jumps to a large fraction of the energy throughput. This
//      certifies the plant obeys its own drag law (so the retention printout
//      the drag knob is tuned against is HONEST — the S3 "a flat instrument
//      certifies a flat controller" discipline, applied to energy) and holds at
//      ANY drag TUNE (a config edit moves plant and prediction together — only
//      a code fork trips it).
//
//  LOAD-BEARING (Fable consult, do not "simplify" away): the force COMPOSITION
//  below is duplicated from step.cpp on purpose — sharing the primitives
//  (q_dyn/current_vhat/lift_coeff/gravity_dir, sim/aero.h) is right (H1), but
//  factoring the assembled grav+thrust+drag expression into a helper CALLED BY
//  BOTH step.cpp and here would make the residual identically zero under every
//  plant mutation, including the flagship k_induced fork. The duplication IS
//  the fork detector. The throttle FLOAT round-trip (sim::Inputs.throttle is
//  float) is mirrored below so a throttle retune to a non-float-exact value
//  does not false-trip the "tune-independent" claim.
// ===========================================================================
namespace {
struct AppliedPower {
    double net = 0.0;   // (grav+thrust+drag)·v  — the signed energy rate [W]
    double comp = 0.0;  // |grav·v| + |thrust·v| + |drag·v| — the component
                        // throughput [W] the residual is normalized against
                        // (the net nearly cancels: thrust+grav vs drag, so
                        // normalizing by |net| would inflate the rel floor and
                        // shrink under an energy-neutral T_max retune).
};

// Power done by the energy-bearing forces at a pre-tick state, reconstructed
// from the SAME config expressions step.cpp applies. `throttle_cmd` is the
// instructor passthrough; the plant slews the engine AND routes it through the
// float sim::Inputs, so thrust uses next.throttle computed on the float-cast
// command exactly as step.cpp does. Power = F·v with the pre-tick velocity —
// the velocity the semi-implicit step applies force to.
AppliedPower applied_power_config(const sim::SimState& s, double throttle_cmd,
                                  const sim::AircraftParams& p) {
    // Mirror the float seam (sim/state.h Inputs.throttle is float; the plant
    // clamps static_cast<double>(inputs.throttle)) — else a non-float-exact
    // throttle tune drifts the two sides by ~1e-8 and false-trips the gate.
    const double cmd = std::clamp(
        static_cast<double>(static_cast<float>(throttle_cmd)), 0.0, 1.0);
    const double slew_cap = p.throttle_slew_rate * p.sim_dt;
    const double next_throttle =
        s.throttle + std::clamp(cmd - s.throttle, -slew_cap, slew_cap);

    const double speed = glm::length(s.velocity);
    // MB-atm: the reconstruction reads the SAME thinned density and lapsed
    // thrust the plant applies (shared sim/aero.h primitives — the AT-12
    // fork-detector now covers the atmosphere: the AT-12 turn flies at
    // 6 km, atm_frac ~ 0.69, so a plant-applies-f-but-config-doesn't fork
    // blows the residual immediately).
    const double alt = sim::altitude(s.position, p);
    const double q = sim::q_dyn(sim::rho_at(alt, p), speed);
    const glm::dvec3 vhat = sim::current_vhat(s, p);
    const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};

    const glm::dvec3 gravity = (p.mass * p.g) * sim::gravity_dir(s.position);
    const glm::dvec3 thrust =
        (p.T_max * sim::atm_frac(alt, p) * next_throttle) * nose;
    const double alpha = sim::alpha_of(sim::body_dir_of(s.orientation, vhat));
    const double Cl = sim::lift_coeff(alpha, p);
    // S-wvane (SPEC §0, 2026-07-11): the sideslip drag price Cd_beta*sin^2(b)
    // is an ENERGY-bearing term and must be reconstructed here (the AT-12
    // turn carries real beta — landing the plant term without this line
    // tripped the gate at rel 0.055, the fork detector doing its job). The
    // SIDE FORCE itself is deliberately ABSENT: it is perpendicular to the
    // pre-tick GUARDED vhat by construction (workless — the lift scope
    // argument: a side-force defect moves the trajectory, never the energy;
    // its semi-implicit KE surplus is the same class as lift's, outside
    // this gate by design). Sub-v_dir_eps the guard holds a STALE vhat and
    // the residual work is real but <= ~10 W (q ~ v^2) — outside every
    // gated flight (diff red-team P3-1 scoping).
    const double beta_w = sim::beta_of(sim::body_dir_of(s.orientation, vhat));
    const double sb_w = std::sin(beta_w);
    const glm::dvec3 drag =
        -(q * p.S * (p.Cd0 + p.Cd_beta * sb_w * sb_w + p.k_induced * Cl * Cl)) *
        vhat;

    const double gp = glm::dot(gravity, s.velocity);
    const double tp = glm::dot(thrust, s.velocity);
    const double dp = glm::dot(drag, s.velocity);
    return {gp + tp + dp, std::abs(gp) + std::abs(tp) + std::abs(dp)};
}
}  // namespace

TEST_CASE(
    "AT-12: sustained max-rate turn keeps energy predictable (drag law)") {
    harness::ClosedLoop cl(harness::at12_start(kAp),
                           glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    // GROUNDED spawn tick captures held_bank at the (level) bank so no spurious
    // leveling roll fires (SPEC §9.5); energy accounting starts AFTER it.
    cl.tick(harness::kAt12Throttle, kAp, kCp, /*grounded=*/true);

    // Spool window: throttle slews from 0 toward full over ~0.5 s (SPEC §9.8),
    // and the turn takes a beat to establish — the sustained-n floor is checked
    // only AFTER it, so the ramp-in does not false-fail the premise.
    constexpr int kSpoolTicks = 240;  // 2 s
    const double V0 = glm::length(cl.state.velocity);
    double residual = 0.0;        // Σ |plant linear work − config work| (abs
                                  // per tick — a signed sum could cancel an
                                  // oscillating defect over a near-closed path)
    double abs_throughput = 0.0;  // Σ component throughput dt — the budget
    double n_sum = 0.0;           // mean |true load factor|
    double n_peak = 0.0;
    double n_min_sustained = 1e9;  // min |n| AFTER spool — genuinely sustained
    int n_soft_ticks = 0;          // sustained ticks with |n| < 1.5 (v5 rung C:
                                   // the anti-glide guard is DURATION-shaped)
    double phi_absmax = 0.0;
    for (int i = 0; i < harness::kAt12Ticks; ++i) {
        harness::at12_reaim(cl);
        const glm::dvec3 v = cl.state.velocity;  // pre-tick
        const AppliedPower P =
            applied_power_config(cl.state, harness::kAt12Throttle, kAp);
        const control::Telemetry t = cl.tick(harness::kAt12Throttle, kAp, kCp);
        REQUIRE(finite_state(cl.state));  // (gate) no NaN anywhere, 10 s
        const glm::dvec3 vp = cl.state.velocity;  // post-tick
        // Plant's exact per-tick linear work m·v·(v'−v), with the semi-implicit
        // KE surplus ½m|v'−v|² thus never entering — algebra on the plant's OWN
        // output. Equals m·v·a·dt = (grav+thrust+drag)_plant·v·dt (lift·v ≡ 0).
        const double plant_work = kAp.mass * glm::dot(v, vp - v);
        residual += std::abs(plant_work - P.net * kAp.sim_dt);
        abs_throughput += P.comp * kAp.sim_dt;

        const double n = std::abs(t.load_factor);
        n_sum += n;
        n_peak = std::max(n_peak, n);
        if (i >= kSpoolTicks) {
            n_min_sustained = std::min(n_min_sustained, n);
            if (n < 1.5) ++n_soft_ticks;
        }
        phi_absmax = std::max(phi_absmax, std::abs(t.extracted.phi));
    }
    const double Vf = glm::length(cl.state.velocity);
    const double rel = residual / abs_throughput;
    const double n_mean = n_sum / harness::kAt12Ticks;

    std::printf(
        "[AT-12] retention=%.1f%% (V %.1f->%.1f) | residual=%.3e MJ "
        "throughput=%.3f MJ rel=%.2e | n_mean=%.2f n_min_sus=%.2f n_peak=%.2f "
        "phi_max=%.0f deg alt=%.0f m\n",
        100.0 * Vf / V0, V0, Vf, residual * 1e-6, abs_throughput * 1e-6, rel,
        n_mean, n_min_sustained, n_peak, deg(phi_absmax),
        sim::altitude(cl.state.position, kAp));

    // Premise (fail LOUD if a retune stops the turn — the AT-15 discipline):
    // the maneuver must actually be a SUSTAINED hard turn, or the energy gate
    // reconciles a benign glide and pins nothing about drag under load.
    // RE-PARAMETERIZED 2026-07-12 (Rung Y3, damp_ff_yaw 1.0): the max-rate
    // hold has a wind-up->relax breathing cycle in BOTH ff arms (ff=0: peak
    // 8.47g, trough 2.38 at t=9.7 s; ff=1: peak 8.80g, trough 1.70 at t=8.0 s
    // — same phenomenon, phase-shifted and ~0.7g deeper; 21 ticks below 2.0
    // in a 5.79g-mean turn). The old `> 2.0` per-tick floor was CALIBRATED to
    // the sagged yaw's trough phase (passed by 0.38g of margin), the exact
    // AT-15 "bound welded to today's table" class. Honest-to-intent form: the
    // floor rejects any GLIDE segment (a glide is n ~= 1.0-1.2, far below
    // 1.5), and the NEW mean guard rejects spike-then-glide averaging that a
    // floor alone tolerated.
    // RE-PARAMETERIZED AGAIN 2026-07-23 (v5 rung C, hold-the-line): the sag
    // servo + K_aoa 10 deepen the breathing trough to a SINGLE 26-tick
    // (0.22 s) beat at n 1.00 in a 6.5g-mean turn — 1.2% of the sustained
    // window. The bare `n_min > 1.5` floor was calibrated to the pre-rung
    // trough amplitude (the same welded-bound class it was re-parameterized
    // FOR in Y3). Honest-to-intent, duration-shaped: a GLIDE settles at
    // n ~ 1 for the whole window (n_soft ~ 100%), a breathing beat does not
    // (measured 1.2%) — cap the soft fraction at 5%. The hard floor drops to
    // 0.5: a real UNLOAD/pushover mid-turn (the C2b slingshot measured
    // n = -2.7 at its apex — |n| through 0) still fails LOUD.
    REQUIRE(n_soft_ticks <
            (harness::kAt12Ticks - kSpoolTicks) / 20);  // no glide segment
    REQUIRE(n_min_sustained > 0.5);   // no unload/pushover apex mid-turn
    REQUIRE(n_mean > 4.0);            // genuinely a sustained HARD turn
    REQUIRE(phi_absmax > rad(60.0));  // rolled into a genuine bank
    // MB-flaps premise: this reconstruction carries NO flap/gear terms — it
    // is correct only while the AT-12 flight is CLEAN (ClosedLoop cannot
    // command a flap today). If a future harness flight deploys, extend
    // applied_power_config (test_flaps' deployed_power_config is the model)
    // instead of chasing a phantom "plant drag fork" (diff red-team P3-1).
    REQUIRE(cl.state.flap == 0.0);
    REQUIRE(cl.state.gear == 0.0);

    // (gate) Predictable energy: the plant's KE bookkeeping matches the config
    // force model to machine precision (the surplus-removed linear work is
    // EXACT, so this is not an O(dt) closure — observed rel ~ 1e-13). A plant
    // drag-code fork makes the applied drag diverge from the documented model
    // and the residual jumps to a large fraction of the throughput — mutation:
    // k_induced -> 0 in step.cpp sends rel from ~1e-13 to ~1.8. Pinned FAR
    // above the fp floor, FAR below the mutation signal; tune-independent (a
    // config edit moves both sides together).
    CHECK(rel < 1e-9);
}

// ===========================================================================
// AT-16 — banked tracking (HARNESS §5). A sustained coordinated banked turn:
// |β| bounded throughout (coordination verified closed-loop, C7), a real bank
// held, no NaN. UNDER S7-autolevel (§7 Item-2 Part B): a banked turn is now
// sustained by HOLDING THE AIM OFF TO THE SIDE (err > blend_lo -> MANEUVER,
// bank-to-turn), because resting the aim ON the nose now eases the wings LEVEL
// (the WT-style release-to-level model). This SUPERSEDES the pre-autolevel
// "hold a banked turn by resting the aim on the nose" (the grounded-capture +
// wings-hold sustain) — that path now auto-levels. The FINE↔MANEUVER
// anti-chatter is pinned by the boundary-straddle leg below (unaffected).
// ===========================================================================
TEST_CASE("AT-16: sustained coordinated banked turn (aim held off-nose)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::level_state(kAp, 160.0, 3000.0, up, heading);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});

    double beta_max = 0.0, phi_max = 0.0;
    int maneuver_ticks = 0, fine_ticks = 0;
    for (int i = 0; i < 1800; ++i) {  // 15 s
        // Hold the aim ~15 deg to the RIGHT of the current heading, in the
        // horizon (recomputed from local_up each tick — never a cached axis,
        // SPEC §6.1), so err stays > blend_lo and the coordinated turn
        // sustains.
        const glm::dvec3 lu = sim::local_up(cl.state.position);
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        glm::dvec3 h =
            nose - glm::dot(nose, lu) * lu;  // heading in the horizon
        if (glm::length(h) < 1e-6)
            h = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        h = glm::normalize(h);
        cl.aim = glm::normalize(glm::angleAxis(-rad(15.0), lu) * h);
        const control::Telemetry t = cl.tick(0.8, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        if (i > 600) {  // after 5 s settle into the turn
            beta_max = std::max(beta_max, std::abs(t.extracted.beta));
            phi_max = std::max(phi_max, std::abs(t.extracted.phi));
            if (t.regime == control::Regime::MANEUVER)
                ++maneuver_ticks;
            else
                ++fine_ticks;
        }
    }
    std::printf(
        "[AT-16] phi_max=%.1f deg  beta_max=%.2f deg  maneuver=%d fine=%d\n",
        deg(phi_max), deg(beta_max), maneuver_ticks, fine_ticks);
    CHECK(phi_max > rad(20.0));          // sustains a real bank (turning)
    CHECK(beta_max <= rad(5.0));         // coordinated (|β| bounded, C7)
    CHECK(maneuver_ticks > fine_ticks);  // the sustained turn lives in MANEUVER
}

// AT-16 (anti-chatter leg): the FINE↔MANEUVER latch is hysteretic (blend_lo=5,
// blend_hi=12 deg, controller.cpp:101-104) so a pointing error that DWELLS at
// the boundary with ripple must NOT limit-cycle the regime. A closed-loop chase
// nulls err to 0 and only sweeps through 12 deg monotonically (never dwells),
// so a single-threshold latch would look identical — useless as a hysteresis
// test. Instead drive the latch OPEN-LOOP on a fixed state: err == the
// commanded offset, oscillated in [10.5, 13.5] deg (ripple straddling blend_hi)
// while carrying the Internal across ticks. The hysteretic latch trips to
// MANEUVER ONCE (err never falls below blend_lo=5) and stays; a non-hysteretic
// latch (blend_lo→blend_hi, or a bare `err>12` per tick) switches TWICE per
// ripple period — tens of times over the run.
TEST_CASE("AT-16: FINE<->MANEUVER latch does not chatter at the boundary") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 160.0, 3000.0, up, heading);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};

    control::Internal internal = control::reset();
    int switches = 0, maneuver_ticks = 0;
    control::Regime prev = control::Regime::FINE;
    for (int i = 0; i < 1800; ++i) {  // 15 s
        // err oscillates in [7.0, 10.0] deg — dwells on both sides of blend_hi
        // (9 deg, §7 2026-07-06 12->9); starts BELOW it (a clean FINE tick 0,
        // no fp-boundary ambiguity). Crosses the boundary each period, never
        // below blend_lo (5), so a hysteretic latch switches ONCE.
        const double off =
            rad(8.5) + rad(1.5) * std::sin(2.0 * kPi * i / 120.0);
        control::Input in;
        in.target_dir_world = glm::angleAxis(off, right) * nose;  // pure pitch
        in.throttle = 0.7;
        const control::Output o =
            control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
        internal = o.internal;  // carry the regime latch across ticks
        REQUIRE(o.telem.e == Catch::Approx(off).margin(1e-9));  // err == offset
        if (o.telem.regime != prev) ++switches;
        prev = o.telem.regime;
        if (o.telem.regime == control::Regime::MANEUVER) ++maneuver_ticks;
    }
    std::printf("[AT-16 boundary] regime switches=%d  maneuver_ticks=%d\n",
                switches, maneuver_ticks);
    REQUIRE(maneuver_ticks > 0);  // premise: it DID reach MANEUVER (crossed 9)
    REQUIRE(maneuver_ticks < 1800);  // ...and it dwelt on the FINE side too
    // Hysteretic: exactly one FINE→MANEUVER edge, no return (err never < 5
    // deg). A single-threshold latch chatters ~2x per 1 s ripple → ~30 over 15
    // s.
    CHECK(switches <= 2);
}

// ===========================================================================
// Circumnavigation (HARNESS §5, SPEC §14.1): a trimmed lap traverses a full
// circumference and APPROXIMATELY CLOSES with no edge, wall, wrap, or seam —
// the sphere has no coordinate singularity to hit. Altitude stays bounded the
// whole way (the curvature feedforward holds the great circle). Untuned gains
// (Section 7 tunes for feel) leave a slow orbit-plane precession, so closure is
// "approximately" — the EXACT great-circle-holonomy null is unit-pinned in
// test_aim_frame; here the objective is "laps the globe, nothing breaks."
// ===========================================================================
TEST_CASE(
    "circumnav: a trimmed lap circles the globe and approximately closes") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.hold_freelook();  // aim only transported (SPEC §9.1 the whole way)

    const glm::dvec3 p_start = cl.state.position;
    const double V0 = glm::length(cl.state.velocity);
    const int period =
        static_cast<int>(std::lround(2.0 * kPi * kAp.R / V0 / kAp.sim_dt));

    double alt_min = 1e9, alt_max = -1e9, min_dist = 1e18, path = 0.0;
    glm::dvec3 prev = cl.state.position;
    const int total = period + period / 3;  // 1.33 laps: bracket the closure
    for (int i = 0; i < total; ++i) {
        cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        const double alt = sim::altitude(cl.state.position, kAp);
        alt_min = std::min(alt_min, alt);
        alt_max = std::max(alt_max, alt);
        path += glm::length(cl.state.position - prev);
        prev = cl.state.position;
        if (i > period / 2)  // look for the return once past the far side
            min_dist =
                std::min(min_dist, glm::length(cl.state.position - p_start));
    }
    const double circumference = 2.0 * kPi * glm::length(p_start);
    std::printf(
        "[circumnav] period=%d  closest_return=%.0f m  circumference=%.0f m  "
        "path=%.0f m  alt=[%.1f,%.1f]\n",
        period, min_dist, circumference, path, alt_min, alt_max);
    CHECK(alt_min > 1000.0);            // no dive, no wrap, no escape
    CHECK(alt_max < 4000.0);            // bounded — holds altitude
    CHECK(path > 0.9 * circumference);  // it really lapped the globe
    CHECK(min_dist <
          0.05 * glm::length(p_start));  // approximately closes (<0.05R)
}

// ===========================================================================
// AT-14c (closed loop): the freelook-held aim frame, parallel-transported over
// a sustained real banked turn, composes correctly with the plant's local_up
// sequence — the quaternion frame reproduces the vector aim path and stays
// live. The exact holonomy null+area PAIR is unit-pinned in test_aim_frame
// (RA8).
// ===========================================================================
TEST_CASE(
    "AT-14c: the aim frame transports correctly over real banked flight") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // The exact holonomy PAIR — great-circle lap ≈ identity AND banked small
    // circle = enclosed-area/R² — is a CLOSED-loop measurement, decisively
    // unit-pinned in test_aim_frame (RA8; SPEC cites it as de-risking AT-14 at
    // unit level). Closed-loop on the real plant it is confounded: the
    // transport is BY the local_up rotation, so on an OPEN leg any
    // local_up-referenced metric is null by construction, and a full lap drifts
    // (untuned gains) far more than the holonomy. What the ASSEMBLED path adds
    // is that the full AimFrame's QUATERNION transport (which also carries
    // camera-up) agrees with the VECTOR transport the ClosedLoop/app aim path
    // uses (control::transport_aim), over the real local_up sequence, and stays
    // live. (Orthonormality is a TYPE invariant of AimFrame — q is normalized
    // each step — so it is not asserted here; test_aim_frame owns the frame
    // algebra.) A transport that composes the rotation onto the quaternion
    // wrong (side/order) diverges from the vector path and is caught.
    sim::SimState s0 = harness::flight_state(kAp, 160.0, 3000.0, up, heading,
                                             rad(55.0), 0.0, rad(2.0));
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(0.8, kAp, kCp, /*grounded=*/true);  // capture held_bank (AT-16)
    cl.aim_nose();
    cl.hold_freelook();  // aim only transported while the bank turns us
    input::AimFrame af;
    af.reseed(cl.state.orientation, sim::local_up(cl.state.position));
    const glm::dvec3 fwd_start = af.forward();
    // An independent aim VECTOR carried by control::transport_aim through the
    // SAME up-sequence — the reference the quaternion frame must reproduce.
    glm::dvec3 vec_aim = fwd_start;
    glm::dvec3 prev_up = sim::local_up(cl.state.position);

    double bank_max = 0.0, frame_vs_vec_max = 0.0, fwd_swing_max = 0.0;
    for (int i = 0; i < 3600; ++i) {  // 30 s of banked turning
        const control::Telemetry t = cl.tick(0.8, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        const glm::dvec3 u = sim::local_up(cl.state.position);
        af.transport(prev_up, u);  // quaternion frame
        vec_aim = control::transport_aim(vec_aim, prev_up, u);  // vector path
        prev_up = u;
        REQUIRE((std::isfinite(af.q.w) && std::isfinite(af.q.x)));
        bank_max = std::max(bank_max, std::abs(t.extracted.phi));
        // The quaternion frame's forward tracks the vector transport to fp.
        frame_vs_vec_max =
            std::max(frame_vs_vec_max, glm::length(af.forward() - vec_aim));
        fwd_swing_max = std::max(
            fwd_swing_max, deg(std::acos(std::clamp(
                               glm::dot(af.forward(), fwd_start), -1.0, 1.0))));
    }
    std::printf(
        "[AT-14c] bank_max=%.1f deg  aim swing=%.1f deg  frame_vs_vec=%.2e\n",
        deg(bank_max), fwd_swing_max, frame_vs_vec_max);
    CHECK(bank_max > rad(40.0));  // the leg really banked (turns the plane)
    CHECK(fwd_swing_max > 10.0);  // transport is LIVE (the aim really moves)
    CHECK(frame_vs_vec_max < 1e-12);  // frame transport == vector transport
}

// ===========================================================================
// AT-13 — clean rebirth (HARNESS §5, SPEC §6.3/§9.5). Die mid-override cursor
// astern → the GROUNDED respawn resets internal + aim:=nose (no dive at the old
// cursor); a banked spawn fires no leveling roll; the crash predicate is
// altitude ≤ 0.
// ===========================================================================
TEST_CASE("AT-13: respawn mid-override astern is clean (reset + aim:=nose)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const sim::SimState s0 =
        harness::level_state(kAp, 140.0, 3000.0, up, heading);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    // Aim astern and hold a pitch override for a while: wind up internal state.
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    cl.aim = -nose;  // cursor directly behind
    cl.hold_override(0, +1.0);
    for (int i = 0; i < 300; ++i) cl.tick(0.7, kAp, kCp);  // 2.5 s of override
    REQUIRE(cl.internal.any_override);
    REQUIRE(cl.internal.ovr_ramp.x > 0.0);

    // Respawn: release the keys and take ONE grounded tick (the app pairs
    // control::reset + aim:=nose + fl.reset — ClosedLoop mirrors it).
    cl.release_override();
    const control::Telemetry g = cl.tick(0.7, kAp, kCp, /*grounded=*/true);
    // Internal reset ran: no wound integrator, no override latch, pursuit back.
    CHECK(cl.internal.integ == glm::dvec3{0.0});
    CHECK(cl.internal.ovr_ramp == glm::dvec3{0.0});
    CHECK_FALSE(cl.internal.any_override);
    CHECK(cl.internal.pursuit);
    // Inputs zeroed this tick (GROUNDED), and aim re-seated to the nose — NOT
    // left at the astern cursor that would fling the nose around on live
    // resume.
    CHECK(cl.last_inputs.pitch == 0.0f);
    const glm::dvec3 nose_now =
        cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    CHECK(glm::dot(glm::normalize(cl.aim), nose_now) > 0.999);
    (void)g;

    // Live resume: no violent dive — bounded rates, stays airborne 3 s.
    double omega_max = 0.0;
    for (int i = 0; i < 360; ++i) {
        cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        omega_max = std::max(omega_max, glm::length(cl.state.angular_vel));
    }
    std::printf("[AT-13] post-respawn omega_max=%.3f rad/s\n", omega_max);
    CHECK(omega_max < rad(240.0));  // bounded recovery, no violent fling/tumble
    CHECK(sim::altitude(cl.state.position, kAp) > 1000.0);  // clean rebirth
}

TEST_CASE("AT-13: banked spawn fires no uncommanded leveling roll") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Spawn banked 35 deg.
    sim::SimState s0 = harness::flight_state(kAp, 140.0, 3000.0, up, heading,
                                             rad(35.0), 0.0, rad(2.0));
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    // One grounded (spawn) tick: heldBank must carry the current bank so the
    // first live tick does not command a roll toward level.
    const control::Telemetry g = cl.tick(0.7, kAp, kCp, /*grounded=*/true);
    CHECK(g.extracted.phi == Catch::Approx(rad(35.0)).margin(rad(1.0)));
    CHECK(cl.internal.held_bank == Catch::Approx(rad(35.0)).margin(rad(1.0)));
    CHECK(cl.last_inputs.roll == 0.0f);  // grounded: zero Inputs

    // First LIVE tick (aim on nose, deadzone): roll command ~0 (no leveling).
    cl.aim_nose();
    const control::Telemetry live = cl.tick(0.7, kAp, kCp);
    std::printf("[AT-13 banked] live omega_des.z=%.4f rad/s\n",
                live.omega_des.z);
    // No LURCH: because held_bank carries the spawn bank, the first tick's
    // demand is only one decay step, K_phi*rate*dt*phi0 — derived from the
    // config the mechanism reads (1.5x margin), NOT a constant calibrated to
    // today's table (the config-relative-bounds lesson: a fixed 5 deg/s
    // false-failed the rate 3.0->4.5 retune). The slam it guards against is
    // K_phi*phi0 (~175 deg/s) — orders of magnitude above.
    const double one_step =
        kCp.K_phi * kCp.auto_level_rate * kAp.sim_dt * rad(35.0);
    CHECK(std::abs(live.omega_des.z) < 1.5 * one_step);
}

TEST_CASE("AT-13: the crash predicate is altitude <= 0") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Just above the surface, diving straight in: within a few ticks alt <= 0.
    sim::SimState s = harness::level_state(kAp, 120.0, 5.0, up, heading);
    s.velocity = 120.0 * (-up);  // straight down
    s.last_vhat = -up;
    bool crossed = false;
    for (int i = 0; i < 20 && !crossed; ++i) {
        s = sim::step(s, sim::Inputs{}, kAp, nullptr, kAp.sim_dt);
        REQUIRE(finite_state(s));
        if (sim::altitude(s.position, kAp) <= 0.0) crossed = true;
    }
    CHECK(crossed);  // the app resets on exactly this predicate (main.cpp)
}

// ===========================================================================
// Poles (SPEC §14.2): gravity locally correct everywhere (no fixed down axis),
// no flip/singularity through the zenith, and both cameras stay well-formed —
// the frame-carried aim camera CONTINUOUS across a vertical pass (its whole
// point), the raw chase camera at least finite/unit-up (its zenith fallback is
// a disclosed hard switch, camera.h).
// ===========================================================================
TEST_CASE("poles: gravity and altitude are correct at and around the poles") {
    for (const glm::dvec3 axis : {glm::dvec3{1, 0, 0}, glm::dvec3{0, 1, 0},
                                  glm::dvec3{0, 0, 1}, glm::dvec3{-1, 0, 0}}) {
        const glm::dvec3 pos = (kAp.R + 2000.0) * glm::normalize(axis);
        const glm::dvec3 g_dir = sim::gravity_dir(pos);
        CHECK(glm::length(g_dir + glm::normalize(pos)) <
              1e-12);  // gravity == -local_up, no fixed down axis
        CHECK(sim::altitude(pos, kAp) == Catch::Approx(2000.0).margin(1e-6));
    }
}

TEST_CASE("poles: frame-carried camera is continuous across a vertical pass") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    // Zoom straight up, over the top, and down the far side (nose passes
    // through the local zenith): a great-circle vertical loop through the pole.
    sim::SimState s0 = harness::level_state(kAp, 200.0, 3000.0, up, heading);
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    s0.orientation =
        glm::normalize(glm::angleAxis(rad(88.0), right) * s0.orientation);
    s0.velocity = 200.0 * (s0.orientation * glm::dvec3{0.0, 0.0, -1.0});
    s0.last_vhat = glm::normalize(s0.velocity);

    // Fly the loop through the real closed loop with the aim HELD (freelook),
    // and carry an AimFrame in parallel — transported by the SAME local_up
    // sequence — exactly as the app does (main.cpp: AimFrame +
    // aim_chase_camera).
    harness::ClosedLoop cl(s0, s0.orientation * glm::dvec3{0.0, 0.0, -1.0});
    cl.hold_freelook();  // aim only transported; the loop flies the zoom loop
    input::AimFrame af;
    af.reseed(s0.orientation, sim::local_up(s0.position));
    render::ChaseParams chase;

    // A vertical loop is a GREAT circle -> NULL holonomy, so on this path the
    // carried up and a local_up rebuild COINCIDE in direction: no continuity
    // test can tell them apart. The frame-carried camera's benefit here is
    // SINGULARITY AVOIDANCE — at the near-zenith the local_up basis degenerates
    // (its perpendicular-to-forward component -> 0), while the carried frame
    // stays a clean orthonormal basis. Pin THAT: track the zenith approach and
    // the local_up basis conditioning there.
    double max_zenith = 0.0, min_localup_perp = 1.0, min_carried_dot = 1.0;
    glm::dvec3 prev_up = sim::local_up(cl.state.position);
    for (int i = 0; i < 3000; ++i) {  // 25 s: up, over, down
        cl.tick(1.0, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        const glm::dvec3 u = sim::local_up(cl.state.position);
        af.transport(prev_up, u);  // the SAME rotation ClosedLoop applied
        prev_up = u;

        const render::CameraPose pa = render::aim_chase_camera(
            cl.state, af.forward(), af.up(), kAp, chase);
        const render::CameraPose pr =
            render::chase_camera(cl.state, kAp, chase);
        REQUIRE(finite_pose(pa));
        REQUIRE(finite_pose(pr));
        CHECK(glm::length(pa.up) == Catch::Approx(1.0).margin(1e-9));
        CHECK(glm::length(pr.up) == Catch::Approx(1.0).margin(1e-9));
        // Structural: the shipped camera-up IS the carried aim-up (§9.2); a
        // mutation that ignores the passed up (body_up, a fixed axis) trips
        // this.
        min_carried_dot = std::min(min_carried_dot, glm::dot(pa.up, af.up()));

        const glm::dvec3 fwd = af.forward();
        const double z = glm::dot(fwd, u);  // 1 == aim at the zenith
        max_zenith = std::max(max_zenith, z);
        // |local_up perpendicular to forward| = how well-conditioned a local_up
        // camera basis is; -> 0 at the zenith (the pole this frame removes).
        min_localup_perp =
            std::min(min_localup_perp, glm::length(u - glm::dot(u, fwd) * fwd));
    }
    std::printf(
        "[poles] max_zenith=%.4f  min|local_up⟂fwd|=%.4f  "
        "min(pose.up·carried)=%.7f\n",
        max_zenith, min_localup_perp, min_carried_dot);
    CHECK(max_zenith > 0.99);  // the aim passed within ~8 deg of the zenith
    CHECK(min_localup_perp < 0.15);  // a local_up basis DID degenerate there...
    CHECK(min_carried_dot >
          1.0 - 1e-9);  // ...yet the carried camera stayed well-formed (§9.2)
    // Raw camera: finite + unit-up asserted every tick above (its zenith
    // fallback is a disclosed hard switch, so no continuity bound is claimed).
}

// ===========================================================================
// AT-15 legs - the push-vs-roll gate, closed-loop (HARNESS section 5; SPEC
// section 9.3). The primitives are unit-pinned (test_cascade: bank_error beyond
// +/-90 deg, the roll/elev latches, the push entry BOTH branches; the single-
// tick 6 deg-below push engage). These legs verify the ASSEMBLED gate behavior
// the Section-7 `push_gate` tuning step will perturb - added BEFORE that table
// is touched (fix report P2b): a threshold retune that dead-zones the roll-
// through path, forgets the ratio-exit handoff, or collapses either hysteresis
// band must FAIL here, not pass silently. Every leg is mutation-verified
// against the exact defect it claims to catch (numbers cited inline). Bank past
// 90 deg is read from the SIGNED cos_phi_theta, NEVER phi: phi = -asin(.) FOLDS
// at 90 deg (reads back DOWN past knife-edge, CLAUDE.md S4a) - it caps at ~89
// deg even fully inverted, so it pins "did it ROLL" (a wings-level pitch loop
// keeps phi ~ 0) while cosPhiTheta < 0 pins "did it go INVERTED" (past 90 deg
// bank). The pair separates roll-through from both a push (no roll, no
// inversion) and a pitch-loop (inversion, no roll).
//
// CALIBRATION MANIFEST (S7-push geometry gate). The following constants are
// calibrated to the CURRENT `[push_gate] down_enter/down_exit` = 88/96 deg
// boundary (turnover ~92 deg, just past straight-down); when Section 7 retunes
// down_enter/down_exit recalibrate them AS A SET (moving the band trips several
// at once - by design, loud):
//   * 60 deg (engage/push) / 135 deg (roll-through, past the cutover) / the
//     60->135 deg handoff (Leg 3) here
//   * the [84,94] deg geometry-dither window (Leg 4) and the [8,13] deg lateral
//     -> bankErr [115,127] bank-dither window (bank-band leg)
//   * `test_cascade.cpp`'s 6/60/85 deg-pushes / 135 deg-rolls single-tick pin
// HIDDEN cross-table dependency: engagement needs `err > blend_lo` for
// have_bank
// (`[regime] blend_lo=5`), so a blend_lo tweak alone can kill the family even
// with `[push_gate]` untouched. Config-DERIVED constants (not on this list,
// they co-move with a retune): the bank-exit swap angle (from
// push_gate_bank_lo).
// MB-rud dependency (2026-07-07): Leg 2's budget_frac headroom now leans on
// the bank-aligned rudder gate FLOORING yaw pointing through the roll-through
// (`[coordination] yaw_align_band`/`yaw_min_frac` — the split-S enters at
// bankErr ~135 deg, cos < -band, gate = yaw_min_frac). A `yaw_min_frac` raise
// or `yaw_align_band` widening re-arms rudder in exactly this window, and the
// MB-4 `yaw_scale` retune multiplies whatever the gate passes — re-run this
// leg after ANY of the three moves (the loader caps band <= 1 so the floor
// stays reachable).
// ===========================================================================

// Leg 2 - a PAST-VERTICAL aim (135 deg below, behind the wing-line) ROLLS
// THROUGH INVERTED (the loop / split-S), not a nose-down push: past the
// geometry cutover (down_exit) the gate stays out and the maneuver rolls the
// target overhead. (S7-push: 60 deg below now PUSHES nose-down — the whole
// point of the change — so the roll-through test moves past the cutover.)
// Closed-loop on the real spherical plant: bank sweeps past 90 deg (cosPhiTheta
// < 0), G never breaches the -3 floor, <=1 push<->roll mode edge, recovers
// airborne. Mutation: push ENTRY is gated by down_enter (target_body.z <=
// z_enter), NOT down_exit - so the mutation forcing this 135 deg aim to bunt
// instead of roll is down_enter -> 179 (z_enter -> ~1, so z(135 deg)=0.707 now
// ARMS push): it bunts nose-low, cosPhiTheta stays > 0 AND phi stays small ->
// both the inversion and roll asserts fail. (down_exit is guarded separately by
// Leg 3's open-loop exit.)
TEST_CASE(
    "AT-15: a past-vertical aim rolls through inverted (G within budget)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 140.0, 3000.0, up, heading, &thr);
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    // GROUNDED spawn tick captures held_bank at the current (level) bank so the
    // wings-hold does not fire a spurious leveling roll (SPEC section 9.5),
    // matching AT-16's banked-spawn discipline.
    cl.tick(thr, kAp, kCp, /*grounded=*/true);
    const glm::dvec3 nose = cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
    cl.aim = glm::normalize(glm::angleAxis(-rad(135.0), right) *
                            nose);  // 135 below (past vertical -> rolls)

    double cpt_min = 1e9, n_min = 1e9, phi_absmax = 0.0;
    double phi_at_first_invert = -1.0;  // |phi| at the FIRST cosPhiTheta<0 tick
    double capture_max = -1.0;  // best dot(nose, aim) reached (=1 -> on)
    double budget_frac_preinvert =
        0.0;  // max |emitted pitch| / |w_min| pre-flip
    bool inverted_yet = false;
    int edges = 0;
    bool prev_push = false;
    for (int i = 0; i < 900;
         ++i) {  // 7.5 s: enough to roll through and recover
        const control::Telemetry t = cl.tick(thr, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        cpt_min = std::min(cpt_min, t.extracted.cos_phi_theta);
        n_min = std::min(n_min, t.load_factor);
        phi_absmax = std::max(phi_absmax, std::abs(t.extracted.phi));
        if (t.extracted.cos_phi_theta < 0.0 && phi_at_first_invert < 0.0)
            phi_at_first_invert = std::abs(t.extracted.phi);
        if (t.extracted.cos_phi_theta < 0.0) inverted_yet = true;
        // Before the wings roll through knife-edge, the align guard holds the
        // pull near zero; the corkscrew pulls to the budget. Measure the
        // emitted pitch as a FRACTION of the (per-tick, budget-relative) w_min
        // floor.
        if (!inverted_yet) {
            const double w_min = (kCp.n_min - t.extracted.cos_phi_theta) *
                                 kAp.g / std::max(t.extracted.speed, kCp.v_min);
            budget_frac_preinvert =
                std::max(budget_frac_preinvert, t.omega_des.x / w_min);
        }
        capture_max = std::max(capture_max, glm::dot(t.extracted.nose, cl.aim));
        if (t.push_mode != prev_push) ++edges;
        prev_push = t.push_mode;
    }
    std::printf(
        "[AT-15 roll-through] cosPhiTheta_min=%.3f phi_max=%.1f "
        "phi@invert=%.1f "
        "deg n_min=%.3f edges=%d capture_max=%.3f budget_frac=%.3f alt=%.0f\n",
        cpt_min, deg(phi_absmax), deg(phi_at_first_invert), n_min, edges,
        capture_max, budget_frac_preinvert,
        sim::altitude(cl.state.position, kAp));
    CHECK(cpt_min < -0.2);  // went INVERTED (bank passed 90 deg)
    // ROLLED through (not a wings-level pitch loop): welded to the FIRST
    // inverted tick — the phi fold makes phi read ~90 deg exactly WHEN a roll
    // crosses knife-edge, while a pitch loop crosses inverted with phi~0.
    // Decoupled extrema (phi_max anywhere, cpt_min anywhere) would pass a
    // bank-then-loop. (Residual, not chased: a >60 deg-banked pitch-over also
    // crosses inverted with phi>60 — but it flies POSITIVE G, not a
    // single-fault output of this gate.)
    CHECK(phi_at_first_invert > rad(60.0));
    // Corkscrew guard, DISCRIMINATING and RETUNE-ROBUST (2nd Fable consult):
    // the align guard holds the pull to ~0 until the wings roll to alignment,
    // so the emitted pitch stays a tiny FRACTION of the w_min budget while
    // still upright (clean 0.033); the corkscrew (align:=1, pitch pulls to the
    // floor WHILE rolling) uses the FULL budget (1.033). Reading the emitted
    // COMMAND as a fraction of the budget (not the plant's G) makes the bound
    // scale with any [g_limits] n_min retune — a hardcoded g-bound (was
    // `n_min>-1.0`) both false-fails a legit tune AND silently disarms if n_min
    // softens. Subject is the controller's own omega_des, so the w_min
    // reconstruction is legitimate.
    CHECK(budget_frac_preinvert < 0.4);
    CHECK(n_min > kCp.n_min);  // ...and the plain -3G budget floor, separately
    CHECK(edges <= 1);         // <=1 push<->roll edge (here it never pushes)
    // The maneuver COMPLETES: the nose captures the target. This is a
    // COMPLETION premise, not corkscrew coverage — the align:=1 corkscrew
    // CONVERGES and captures too (that mutant is caught by budget_frac above);
    // capture's only prey is a hypothetical DIVERGENT barrel roll. Untuned
    // gains -> loose bound.
    CHECK(capture_max > 0.9);
    CHECK(sim::altitude(cl.state.position, kAp) >
          1000.0);  // recovered airborne
}

// Leg 3 - mid-push GEOMETRY-EXIT handoff (SPEC section 9.3, S7-push hysteretic
// gate). A below-nose command engages the push (negative-G, wings held); SWING
// the aim past vertical mid-push so the target crosses behind the wing-line
// (target_body.z > down_exit) and the gate hands off to roll-through - cleanly,
// once. Open-loop on a fixed trim state carrying Internal across ticks (a
// closed-loop chase exits push by ALIGNMENT as the nose reaches the target -
// the probe showed the push dropping at ~tick 14 for exactly that reason - so
// it can never exercise a GEOMETRY exit; same reason AT-16's anti-chatter leg
// is open-loop). At 135 deg below the ONLY satisfied exit clause is
// `target_body.z > cp.push_down_z_exit` (elev < 0, have_bank, |bank_eff|=180
// deg > bank_lo) - so this pins that clause specifically. Mutation (down_exit
// -> 179, controller.cpp): the push never lets go once behind the wing-line ->
// final push_mode stays true, edges=1 -> fails (clean: off->on at the 60 deg
// command, on->off at the 135 deg, edges=2).
TEST_CASE("AT-15: mid-push geometry exit hands off to roll") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    auto aim_below = [&](double d) {
        return glm::normalize(glm::angleAxis(-rad(d), right) * nose);
    };
    control::Internal internal = control::reset();
    int edges = 0;
    bool prev = false, seen_on = false, final_push = false;
    for (int i = 0; i < 60; ++i) {
        // Phase 1 (0-9): aim the nose -> deadzoned, push OFF. Phase 2 (10-29):
        // 60 deg below -> front of the wing-line, push ENGAGES (nose down).
        // Phase 3 (30-59): 135 deg below -> PAST the geometry cutover (behind
        // the wing-line, target_body.z > down_exit), push EXITS to roll (the
        // loop).
        const double below = (i < 10) ? 0.0 : (i < 30 ? 60.0 : 135.0);
        control::Input in;
        in.throttle = thr;
        in.target_dir_world = (below == 0.0) ? nose : aim_below(below);
        const control::Output o =
            control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
        internal = o.internal;
        if (o.telem.push_mode != prev) ++edges;
        prev = o.telem.push_mode;
        seen_on = seen_on || o.telem.push_mode;
        final_push = o.telem.push_mode;
    }
    std::printf("[AT-15 handoff] seen_on=%d final_push=%d edges=%d\n", seen_on,
                final_push, edges);
    // (This leg drives a FIXED state open-loop, so load_factor stays at the
    // trim AoA and can't measure the push's G — the
    // -3G-budget-during-roll-through is Leg 2's job, closed-loop. Here the
    // claim is the clean GEOMETRY-exit handoff: nose-down push while ahead of
    // the wing-line, roll once the aim goes behind it.)
    REQUIRE(seen_on);         // premise: the push DID engage at 60 deg below
    CHECK_FALSE(final_push);  // it handed off to roll once the aim went behind
    CHECK(edges == 2);        // exactly one engage + one clean exit - no dither
}

// Leg 4 - NO DITHER at the gate boundary (the push_gate hysteresis, every leg
// hysteretic per SPEC section 9.3). A nose-low angle that DWELLS at the
// geometry ENTER threshold (down_enter) with small ripple must not chatter
// push<->roll (S7-push: the gate is now on target_body.z, not the -G ratio).
// Open-loop on a fixed state, Internal carried: oscillate the angle in [84 deg,
// 94 deg], which straddles down_enter (~88 deg) but never reaches down_exit
// (~96 deg) - so a HYSTERETIC gate latches ON at the first trough and stays (1
// switch), while a single-threshold gate toggles every ripple. A full sweep
// across BOTH thresholds would switch ~2x/period regardless of hysteresis -
// useless as a hysteresis test, the exact AT-16-boundary lesson (CLAUDE.md S6
// red-team). Mutation (down_exit := down_enter, collapse the band): ~8 switches
// over the run vs the hysteretic 1. The band is calibrated to the CURRENT table
// - the premise REQUIREs below fail LOUD if a Section-7 retune moves the
// boundary out of [84 deg,94 deg] (that is the point of landing this before the
// retune).
TEST_CASE("AT-15: push gate does not dither at the boundary (hysteresis)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    const sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 right = s0.orientation * glm::dvec3{1.0, 0.0, 0.0};
    control::Internal internal = control::reset();
    int switches = 0, push_ticks = 0;
    bool prev = false;
    for (int i = 0; i < 240; ++i) {  // 2 s, ripple period 0.5 s
        // Ripple the nose-low angle across the geometry ENTER threshold
        // (down_enter ~88 deg) but staying below the EXIT (down_exit ~96 deg):
        // 84..94 deg. A hysteretic gate latches ON at the first trough and
        // stays (1 switch); a single-threshold gate toggles every ripple.
        const double below =
            89.0 + 5.0 * std::sin(2.0 * kPi * i / 60.0);  // 84..94
        control::Input in;
        in.throttle = thr;
        in.target_dir_world =
            glm::normalize(glm::angleAxis(-rad(below), right) * nose);
        const control::Output o =
            control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
        internal = o.internal;
        if (o.telem.push_mode != prev) ++switches;
        prev = o.telem.push_mode;
        if (o.telem.push_mode) ++push_ticks;
    }
    std::printf("[AT-15 no-dither] switches=%d push_ticks=%d/240\n", switches,
                push_ticks);
    // Premise: the ripple genuinely straddles the boundary - it engages, and a
    // single-threshold gate WOULD drop out at the peaks (both break if a retune
    // moved the push window off [84 deg,94 deg]).
    REQUIRE(push_ticks > 0);
    REQUIRE(push_ticks < 240);
    CHECK(switches <=
          1);  // hysteretic: latches once, no chatter (collapse -> ~8)
}

// Leg 5 - the push DECISION is now V-INDEPENDENT (S7-push: geometry, not the -G
// budget), but the AT-6 "heavier at speed" phenomenon survives in the RATE. The
// OLD gate flipped push->roll at 1.6V because the -G budget tightened ~1/V; the
// new geometry gate does NOT flip (a nose-down aim noses down at any speed -
// the whole point). What DOES still scale ~1/V is the nose-down RATE: the pitch
// saturates on the -G floor w_min_pitch = (n_min - cosPhiTheta)*g /
// max(V,v_min), so the same 60 deg push is SLOWER at 1.6V. Single-tick from
// trim at V=100 and 1.6V=160, identical 60 deg-below geometry. The
// V-independence of push_mode is the controller pin (a regression to the
// -G-budget gate flips hi.push); the rate ordering makes it behavioral.
// Mutation (restore the ratio gate): hi.push flips true -> false, the
// V-independence CHECK fails.
TEST_CASE(
    "AT-15 (S7-push): push gate is V-INDEPENDENT; the rate is heavier at "
    "1.6V") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    const glm::dvec3 tb = glm::normalize(glm::dvec3{
        0.0, -std::sin(rad(60.0)), -std::cos(rad(60.0))});  // 60 below
    struct Gate {
        bool push;
        double omega_x;
    };
    auto gate_at = [&](double V) {
        double thr = 0.7;
        const sim::SimState s0 =
            harness::level_trim_state(kAp, V, 3000.0, up, heading, &thr);
        control::Input in;
        in.throttle = thr;
        in.target_dir_world = s0.orientation * tb;
        const control::Output o =
            control::step(s0, in, control::reset(), kAp, kCp, nullptr, kAp.sim_dt);
        const double cpt = o.telem.extracted.cos_phi_theta;
        const double w_min = (kCp.n_min - cpt) * kAp.g /
                             std::max(o.telem.extracted.speed, kCp.v_min);
        std::printf(
            "[AT-15 1.6V] V=%.0f push=%d omega_x=%.4f w_min_budget=%.4f "
            "cosPhiTheta=%.3f\n",
            V, o.telem.push_mode, o.telem.omega_des.x, w_min, cpt);
        return Gate{o.telem.push_mode, o.telem.omega_des.x};
    };
    const Gate lo = gate_at(100.0);
    const Gate hi = gate_at(160.0);  // 1.6 x 100
    // The push DECISION is now GEOMETRY-based -> V-INDEPENDENT: 60 deg below
    // noses down at BOTH speeds (the old ratio gate ROLLED at 1.6V). A
    // regression to the -G-budget gate flips hi.push back to false -> this
    // fails.
    CHECK(lo.push);
    CHECK(hi.push);
    // ...but the AT-6 "heavier at speed" survives in the RATE: the pitch
    // saturates on the -G floor w_min_pitch = (n_min - cpt)*g/V, which tightens
    // ~1/V, so the SAME 60 deg push commands a SLOWER nose-down rate at 1.6V.
    // Behavioral, not flag-only (a mis-consumed push_mode would not slow here).
    CHECK(std::abs(hi.omega_x) < std::abs(lo.omega_x));
}

// Leg 1 (closed-loop companion) - a below-nose target PUSHES with the wings
// HELD. The single-tick push pin (test_cascade.cpp) and Legs 3-5 all run push
// on a FROZEN state, so the push branch's DYNAMICS - the sustained negative-G,
// the wings-hold that suppresses the roll a below-nose target would otherwise
// command, the held_bank capture (controller.cpp push-engage) - were pinned
// NOWHERE closed-loop (Fable P1). This leg flies a real sustained push on the
// spherical plant from a BANKED entry (so wings-hold is a live signal, not a
// level-flight no-op), continuously re-aiming 55 deg below the moving nose so
// the push STAYS engaged (a bare fixed aim exits by alignment quickly). It
// is the behavioral CONTRAST to Leg 2: the SAME gate sends 55 deg-below into a
// wings-held push (phi held, cosPhiTheta > 0) and a PAST-VERTICAL 135 deg-below
// into a roll-through (phi -> 90, cosPhiTheta < 0).
// v5 RUNG E (Chad 2026-07-23, the knife-edge ruling): the push/roll boundary
// used to be ONLY the geometry cutover down_enter (~88 deg below the nose),
// so a shallow 6 deg-below was deep in the push zone -- that folklore is
// GONE. At the rung-E table, `[push_gate] horizon_enter` (1 -> 45 deg) gates
// FIRST, on the aim's WORLD elevation: at 1 deg, a big lateral deflection
// whose aim dipped a hair below the horizon (the near-astern side-cone
// degeneracy) became push-eligible at exactly the wrong moment (Chad's
// repeated dive, "not go nose down with the bank over unless it's past like
// 45 degrees downward"). A SHALLOW below-nose aim (< 45 deg below horizon)
// is now flown by the normal pointing law / bank-to-turn -- NEVER the push
// commitment; the split-S/push family begins only past 45 deg down
// (deflection AND real down input). This fixture moved to 55 deg below (5
// deg past horizon_enter for margin) to stay genuinely push-eligible; probed
// via telemetry (push_ticks) before asserting, per the same discipline as
// before. If a retune drops horizon_enter/down_enter below the fixture's
// depth the REQUIRE(push_ticks) fails LOUD (the point of landing this before
// any further retune). Mutation-verified: forcing the gate to never engage
// (down_enter -> a tiny angle) -> push_ticks 0 -> REQUIRE fires; a full roll
// in the push branch would blow the phi_dev bound. The wings-hold SIGN is
// only weakly separable closed-loop (roll_hold sits near its fixed point: a
// sign flip drifts phi_dev to ~1.2 deg over 1 s, well inside the HARNESS
// 10 deg) - so phi_dev<10 is a GROSS wings-hold guard here; the sign itself
// is unit-pinned in test_cascade / AT-0's roll_hold_demand primitive.
TEST_CASE(
    "AT-15: a shallow below-nose target pushes with wings held (closed-loop)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    sim::SimState s0 = harness::flight_state(kAp, 100.0, 3000.0, up, heading,
                                             rad(30.0), 0.0, rad(2.0));
    harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
    cl.aim_nose();
    cl.tick(0.7, kAp, kCp,
            /*grounded=*/true);  // banked spawn captures held_bank
    cl.aim_nose();
    for (int i = 0; i < 120; ++i)
        cl.tick(0.7, kAp, kCp);  // settle the 30 deg turn

    double phi0 = 1e9, phi_dev = 0.0, n_min = 1e9;
    int push_ticks = 0, edges = 0;
    bool prev = false;
    for (int i = 0; i < 120;
         ++i) {  // 1 s continuously aiming 55 deg below the nose (v5 rung E,
                 // Chad 2026-07-23 knife-edge ruling: horizon_enter 1->45 --
                 // a shallow 6 deg-below no longer reaches push at all; the
                 // re-aim tracks 55 deg below the CURRENT nose about the
                 // body-right axis, which is IN-PLANE by construction
                 // (aim_side ~ 0) and stays 5 deg past horizon_enter for
                 // margin, same as before just deeper)
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(-rad(55.0), right) * nose);
        const control::Telemetry t = cl.tick(0.7, kAp, kCp);
        REQUIRE(finite_state(cl.state));
        if (t.push_mode != prev) ++edges;
        prev = t.push_mode;
        if (t.push_mode) {
            if (phi0 > 1e8) phi0 = t.extracted.phi;
            phi_dev = std::max(phi_dev, std::abs(t.extracted.phi - phi0));
            n_min = std::min(n_min, t.load_factor);
            ++push_ticks;
        }
    }
    std::printf(
        "[AT-15 push] push_ticks=%d edges=%d phi0=%.1f phi_dev=%.2f "
        "n_min=%.3f\n",
        push_ticks, edges, deg(phi0), deg(phi_dev), n_min);
    REQUIRE(push_ticks >
            100);          // the gate PUSHED (and sustained) a 6 deg below-nose
    CHECK(edges <= 2);     // engaged cleanly, no dither
    REQUIRE(n_min < 0.0);  // it is genuinely a NEGATIVE-G push (not a pull)
    CHECK(n_min > kCp.n_min);  // ...within the -3G budget (the floor: AT-11b/c)
    CHECK(phi_dev <
          rad(10.0));  // wings HELD through the push (bank change <10 deg)
    CHECK(sim::altitude(cl.state.position, kAp) > 1000.0);  // stayed airborne
}

// AT-15 (bank-side exit) - push RELEASES when the aim swings off the nose-low
// pole so |bankErr| falls back inside the band, even with the demand still
// below the nose (SPEC section 9.3, the fourth push-exit clause `|bank_eff| <=
// push_gate_bank_lo`). Every other AT-15 leg sits at bankErr = 180 deg exactly,
// so that clause is dead code to the rest of the suite - deleting it leaves
// them all green (Fable P3), yet its loss is a real "retune dead-zones a path"
// bug: push would stay latched as the pilot swings the cursor laterally back
// toward level. Open-loop, Internal carried: engage straight-below (bankErr
// 180 deg), then swap to a MOSTLY-LATERAL, slightly-below target whose bankErr
// sits 2 deg INSIDE bank_lo (derived from config), with the demand still below
// the nose (target_body.y < 0, so elev < 0 - NOT the elev>=0 exit) and small
// (ratio <= hi - NOT the ratio exit), leaving `|bank_eff| <= bank_lo` as the
// ONLY firing clause. Mutation (delete that clause): push stays engaged -> the
// release assert fails (mutation-verified).
//
// v5 RUNG E (Chad 2026-07-23, the knife-edge ruling) FIXTURE RE-DERIVATION:
// `[push_gate] horizon_enter/exit` (1/-1.5 -> 45/40 deg) now requires the aim
// deeply below the WORLD horizon at BOTH engage and swap, but bank_lo=100
// deg sits close to the lateral pole (90 deg). An UNBANKED aircraft makes
// world elevation == target_body.y exactly (body frame == world frame in
// level flight), and target_body.y = r*cos(bank_eff) (r = sin(off-nose
// angle) <= 1) is then HARD-CAPPED to |cos(bank_eff)| <= 0.174 near
// bank_eff=98-100 -- the original fixture's technique (unbanked aircraft,
// deepen the angles) cannot reach the new 0.643-0.707 depth requirement
// there at all: GEOMETRICALLY UNREACHABLE, not merely a tight number.
// Fix: BANK THE AIRCRAFT ITSELF (s0 rolled by phi_air about its own nose,
// pitch untouched). Since target_dir_world = s0.orientation * tb,
// target_body recomputes to EXACTLY normalize(tb) regardless of s0's
// attitude (the orientation cancels in body_dir_of) -- bank_eff =
// atan2(tb.x,tb.y) stays under direct control. World elevation and the
// WORLD-referenced side_cone (both keyed on the nose/local_up plane, which
// a roll about the nose does NOT move) become the SAME rotation of a
// single (elevation,side) pair by phi_air: elevation = r*cos(bank_eff +
// phi_air), side = r*sin(bank_eff + phi_air) -- their RATIO is tan(bank_eff
// + phi_air), fixed by geometry alone. A first attempt at bank_eff_engage=
// 180 (matching the rest of the suite's convention) + bank_eff_swap=98
// proved a HARD IMPOSSIBILITY, not just tight: engage needs its own
// (bank_eff+phi_air) near 180 (deep + narrow side_cone), swap needs ITS
// (bank_eff+phi_air) ALSO near 180, but these two angles are rigidly 82 deg
// apart (bank_eff_engage - bank_eff_swap = 180-98 = 82) for the SAME
// phi_air -- an 82 deg gap the ~27.5-32.5 deg side_cone bands cannot
// bridge. Re-derived: bank_eff_engage moved off the suite's "180 exactly"
// convention to 140 deg (still comfortably > push_gate_bank=120, entry
// fires the same way) -- now only 42 deg from bank_eff_swap=98, closeable
// by ONE shared phi_air=58 deg (measured: engage elevation -0.947, side
// 0.308 vs entry thresh 0.4617; swap elevation -0.914, side 0.407 vs exit
// thresh 0.537 -- comfortable margins on every leg, verified via telemetry
// before asserting, per the same discipline as Leg 1).
TEST_CASE("AT-15: push releases on the bank-side exit clause") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    const double phi_air = rad(58.0);
    const glm::dvec3 nose0 = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    s0.orientation =
        glm::normalize(glm::angleAxis(phi_air, nose0) * s0.orientation);

    auto step_to = [&](control::Internal& io, const glm::dvec3& tb) {
        control::Input in;
        in.throttle = thr;
        in.target_dir_world = s0.orientation * glm::normalize(tb);
        const control::Output o =
            control::step(s0, in, io, kAp, kCp, nullptr, kAp.sim_dt);
        io = o.internal;
        return o.telem;
    };
    control::Internal internal = control::reset();
    control::Telemetry t{};
    // engage: bank_eff = 140 deg (> push_gate_bank=120, off the suite's
    // usual 180 exactly -- see banner), off-nose radius c_engage = 85 deg
    // (r=0.996, kept < down_enter=88 so target_body.z stays inside the
    // entry z-bound too).
    const double a_engage = rad(140.0);
    const double c_engage = rad(85.0);
    for (int i = 0; i < 6; ++i)
        t = step_to(internal,
                    {std::sin(c_engage) * std::sin(a_engage),
                     std::sin(c_engage) * std::cos(a_engage), -std::cos(c_engage)});
    REQUIRE(t.push_mode);  // premise: the push is engaged (bank_eff 140 deg)
    // Swap to a lateral target whose bankErr is 2 deg INSIDE bank_lo, DERIVED
    // from the config so the geometry tracks any Section-7 bank_lo retune (a
    // hardcoded 45 deg sat 1.6 deg from bank_lo=100 and would false-fail a
    // legit tighten; 2nd Fable consult). bank_error(tb)=atan2(tb.x,tb.y)=a for
    // a cone target tb={sin(c)sin(a), sin(c)cos(a), -cos(c)}; a=bank_lo-2 (>90
    // -> tb.y<0 keeps elev<0, the non-exit); c_swap = 90 deg (r=1, right at
    // the wing-line -- still inside the down_exit z-bound) maximizes the
    // elevation/side margin at this bank_eff.
    // ISOLATION verified (measured, printed then removed): at this swap
    // target elev_world=-0.9114 (exit thresh -0.6428, NOT the horizon exit),
    // demand.x=-0.2186<0 (NOT the elev>=0 exit), tb.z=~0 (exit thresh
    // 0.1045, NOT the z/down_exit exit), aim_side=0.4067 (exit thresh
    // 0.5373, NOT the side_cone exit) -- only |bank_eff|=98<=bank_lo=100
    // fires, so the release below is genuinely isolated to the bank clause.
    const double a = kCp.push_gate_bank_lo - rad(2.0);
    const double c_swap = rad(90.0);
    const control::Telemetry after =
        step_to(internal, {std::sin(c_swap) * std::sin(a),
                           std::sin(c_swap) * std::cos(a), -std::cos(c_swap)});
    std::printf(
        "[AT-15 bank-exit] engaged=%d after_swap_push=%d bankErr=%.1f\n",
        t.push_mode, after.push_mode, deg(a));
    CHECK_FALSE(after.push_mode);  // released via |bank_eff| <= bank_lo
}

// AT-15 (bank-band no dither) - the push gate's BANK hysteresis (enter above
// push_gate_bank=120, exit below push_gate_bank_lo=100) must not chatter when
// |bankErr| DWELLS at the enter threshold with ripple. Leg 4 pins the RATIO
// band's anti-chatter and the bank-exit leg pins that the exit CLAUSE fires,
// but the bank BAND's collapse (bank_lo := bank_hi) was invisible to the whole
// suite (2nd Fable consult) - a real hole going INTO the section that retunes
// that table. Structural mirror of Leg 4: open-loop, Internal carried, a
// variable-bankErr target ripples in [115, 127] deg - straddling bank_hi=120
// but never reaching bank_lo=100 (nor roll_on=135, so roll_latch stays off
// and bank_eff==be). Hysteretic: engages once at the first bankErr>120
// trough and stays (exit unreachable) -> 1 switch. Collapsed band
// (bank_lo:=bank_hi): exits every time bankErr dips below 120 -> chatters
// (mutation-verified). The premise REQUIREs fail LOUD if a bank retune moves
// the window off [115,127] deg.
//
// v5 RUNG E (Chad 2026-07-23, the knife-edge ruling) FIXTURE RE-DERIVATION:
// the original fixture held a FIXED 6-deg-below term and rippled only the
// lateral component (lat 8..13 deg) -- an UNBANKED aircraft, so world
// elevation == target_body.y == -sin(6 deg) == -0.1045 throughout, nowhere
// near the new 0.643-0.707 depth requirement (the same geometric ceiling
// as Legs 2/3's trap: |target_body.y| <= |cos(bank_eff)| <= ~0.17 near
// bank_hi/lo). BANKED THE AIRCRAFT (s0 rolled by phi_air about its own
// nose, same technique as the bank-side-exit leg immediately above -- see
// its banner for the elevation/side = r*cos/sin(bank_eff+phi_air) identity)
// with phi_air = 58 deg (reused unchanged: the whole ripple band [115,127]
// deg lands (bank_eff+phi_air) in [173,185], a narrow 12 deg span hugging
// 180 deg throughout, so elevation and side_cone both carry large margin
// at EVERY point of the ripple, not just at the mean). Off-nose radius
// c = 85 deg (r=0.996, matching Leg 2's engage radius) fixed across the
// ripple (only bank_eff varies, exactly mirroring the original's "only lat
// varies" shape). Measured (printed then removed): elevation ranges
// -0.981 to -0.988 across the whole ripple (comfortably past both
// horizon_enter -0.7071 and horizon_exit -0.6428, so the horizon gate
// cannot be the cause of any switch), aim_side stays <= 0.121 (well
// under both side_cone thresholds) -- isolating the bank band as the ONLY
// gate the ripple can trip, per the leg's intent.
TEST_CASE("AT-15: push gate bank band does not dither (hysteresis)") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    double thr = 0.7;
    sim::SimState s0 =
        harness::level_trim_state(kAp, 100.0, 3000.0, up, heading, &thr);
    const double phi_air = rad(58.0);
    const glm::dvec3 nose0 = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    s0.orientation =
        glm::normalize(glm::angleAxis(phi_air, nose0) * s0.orientation);
    const double c = rad(85.0);
    control::Internal internal = control::reset();
    int switches = 0, push_ticks = 0;
    bool prev = false;
    // The ripple's start phase is shifted (-cos instead of sin) so a(0)=115
    // deg (BELOW bank_hi=120 -- not yet engaged) and rises through the
    // enter threshold partway through, giving a genuine pre-entry segment
    // (measured push_ticks=226/240, i.e. push_mode is false for the first
    // ~14 ticks) instead of latching from tick 0.
    for (int i = 0; i < 240; ++i) {  // 2 s, ripple period 0.5 s
        const double a =
            rad(121.0 - 6.0 * std::cos(2.0 * kPi * i / 60.0));  // 115..127
        const glm::dvec3 tb{std::sin(c) * std::sin(a), std::sin(c) * std::cos(a),
                            -std::cos(c)};
        control::Input in;
        in.throttle = thr;
        in.target_dir_world = s0.orientation * glm::normalize(tb);
        const control::Output o =
            control::step(s0, in, internal, kAp, kCp, nullptr, kAp.sim_dt);
        internal = o.internal;
        if (o.telem.push_mode != prev) ++switches;
        prev = o.telem.push_mode;
        if (o.telem.push_mode) ++push_ticks;
    }
    std::printf("[AT-15 bank-dither] switches=%d push_ticks=%d/240\n", switches,
                push_ticks);
    REQUIRE(push_ticks > 0);    // premise: the ripple DID engage (bankErr>120)
    REQUIRE(push_ticks < 240);  // ...and DID dip below the enter threshold
    CHECK(switches <= 1);  // hysteretic: latches once (collapse band -> ~8)
}

// ---------------------------------------------------------------------------
// S-dampff differential carry-past (SPEC §0 — Chad's "sitting high a degree+
// after a deflection", the pitch-thread attribution): without the damping
// feedforward, a large pitch travel winds the rate integrator to source the
// plant's damping torque (plant_invert inverts only authority); at capture
// the stale integral carries the nose PAST the aim (~0.9 deg at V=250) and
// drains only at tau = K_w/K_wi = 2.5 s — the felt multi-second high hang.
// The SAME 20-deg pitch-up step at V=250 flies twice, differing ONLY in
// damp_ff_pitch (self-armed both ways — the shipped dial is Chad's A/B):
//   arm B (ff = 0, the defect-exists PREMISE): post-capture rebound > 0.6 deg
//   arm A (ff = 1): rebound < 0.35 deg, settle |e| < 0.5 deg within 2.0 s of
//     the step (Chad's spec made executable), and |integ_pitch| stays < 0.1
//     rad s (the preload class is DEAD on-model — no other standing torque
//     exists in this plant; kills a wrong-axis/wrong-q ff that a rebound
//     bound alone might survive).
// PREMISE KNOB SET (the AT-15 discipline — these bounds are calibrated to
// today's table and a retune of any of {n_max, K_wi_pitch, K_w_pitch,
// damp_pitch, integ_cap, v_redline} moves them; trip LOUD, re-derive, never
// blind-relax): the arm-B rebound scales with the wound integral
// ~ damp_pitch*q(V)*w_max/K_wi_pitch and its drain with K_w/K_wi. n_min is
// deliberately NOT in the set — a pitch-UP step winds on the w_max side
// (do not false-suspect the n_min ladder if this leg ever trips).
// Throttle 1.0; V bleeds ~250 -> ~210 over the window (drag > T_max at 250)
// exactly as the step-harness probes flew it — the bounds carry that drift.
// ---------------------------------------------------------------------------
TEST_CASE("S-dampff: the wound-integrator carry-past dies at the capture") {
    const glm::dvec3 up{1.0, 0.0, 0.0}, heading{0.0, 0.0, -1.0};
    struct Arm {
        double rebound = 0.0;   // max e AFTER first capture [rad]
        double late_max = 0.0;  // max e past 2.0 s from the step [rad]
        bool captured = false;
        double integ_end = 0.0;
    };
    auto fly = [&](double ff) {
        control::ControllerParams cp = kCp;
        cp.damp_ff_pitch = ff;
        // S-rimshot v2 isolation: the universal capture deliberately CARRIES
        // the nose ~1 deg past the aim (rim + stopping distance) on exactly
        // this step — which is the mechanism, not the S-dampff carry-past
        // this leg exists to pin. carry = 0 is the sanctioned bit-identical
        // legacy arm; the differential below then measures S-dampff alone.
        cp.capture_carry = 0.0;
        const sim::SimState s0 =
            harness::level_state(kAp, 250.0, 3000.0, up, heading);
        harness::ClosedLoop cl(s0, glm::dvec3{0.0, 0.0, -1.0});
        cl.aim_nose();
        for (int i = 0; i < 240; ++i) cl.tick(1.0, kAp, cp);  // settle 2 s
        const glm::dvec3 nose =
            cl.state.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 right =
            cl.state.orientation * glm::dvec3{1.0, 0.0, 0.0};
        cl.aim = glm::normalize(glm::angleAxis(rad(20.0), right) * nose);
        Arm a;
        for (int i = 0; i < 1200; ++i) {  // 10 s
            const control::Telemetry t = cl.tick(1.0, kAp, cp);
            REQUIRE(finite_state(cl.state));
            if (!a.captured && t.e < rad(0.2)) a.captured = true;
            if (a.captured) a.rebound = std::max(a.rebound, t.e);
            if (i >= 240) a.late_max = std::max(a.late_max, t.e);  // > 2 s
        }
        a.integ_end = cl.internal.integ[0];
        return a;
    };

    const Arm b = fly(0.0);  // legacy dynamics
    const Arm a = fly(1.0);  // full pitch inversion
    std::printf(
        "[S-dampff carry-past] B: cap=%d rebound=%.3f deg late=%.3f | "
        "A: cap=%d rebound=%.3f late=%.3f integ_end=%.4f\n",
        b.captured, deg(b.rebound), deg(b.late_max), a.captured, deg(a.rebound),
        deg(a.late_max), a.integ_end);
    // Premise: both arms reach the aim at all...
    REQUIRE(b.captured);
    REQUIRE(a.captured);
    // ...and the DEFECT exists in the legacy arm (else this differential is
    // theater — the MB-atm non-vacuousness discipline).
    REQUIRE(b.rebound > rad(0.6));
    // The fix: carry-past dead, Chad's 2-second spec holds, integrator idle.
    REQUIRE(a.rebound < rad(0.35));
    REQUIRE(a.late_max < rad(0.5));
    REQUIRE(std::abs(a.integ_end) < 0.1);
}
