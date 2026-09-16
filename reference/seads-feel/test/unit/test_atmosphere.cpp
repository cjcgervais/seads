// R6 — the AtmosphereField spatial-density migration (MASTER_PLAN §3.C;
// sim/fields.h + sim/aero.h::atm_frac_at). Fable-BEFORE-folded suite: the
// spatial fraction MULTIPLIES the existing vertical taper via a
// complement-product union, the null path is bit-for-bit the frozen kernel,
// and — the point of the round — the plant and the controller inversion sample
// the SAME density at a bubble edge (the AT-18b fork detector, extended to the
// spatial axis; a stale altitude-ρ inversion mis-deflects by 1/u).
//
// Every fixture that probes the spatial term does so BELOW taper_alt (4000 m),
// where atm_frac(alt) == 1.0 exactly, so the bubble factor u is isolated — and
// asserts u is genuinely < 1 there (the fixture-no-op trap: a core point or a
// bubble that equals baseline at the probe proves nothing under mutation).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "control/controller.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "world/heightfield.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// GENERIC sphere directions (lessons.md: no world axis may coincide with the
// truth — an axis-aligned fixture hides a fixed-axis bug). A town bubble on a
// slanted direction; a probe ray offset from it by a chosen great-circle arc.
const glm::dvec3 kCenter = glm::normalize(glm::dvec3{0.3, 1.0, 0.2});

// A world position at altitude h along a unit direction.
glm::dvec3 at(const glm::dvec3& dir, double h) {
    return glm::normalize(dir) * (kP.R + h);
}

// A direction offset from kCenter by great-circle arc s [m] (surface metres),
// in a generic tangent direction (not any axis).
glm::dvec3 offset_dir(double arc_m) {
    const double theta = arc_m / kP.R;
    const glm::dvec3 tangent =
        glm::normalize(glm::cross(kCenter, glm::dvec3{0.11, 0.07, 1.0}));
    return glm::normalize(std::cos(theta) * kCenter +
                          std::sin(theta) * tangent);
}

// The R6 static test bubble: 6 km ground radius, 4 km ceiling, km-scale edges.
sim::AtmosphereField one_bubble() {
    sim::AtmosphereField af;
    af.bubbles.push_back(
        sim::AtmosphereField::Bubble{kCenter, 6000.0, 4000.0, 1200.0, 600.0});
    return af;
}

// The plant's authority torque at a position (step.cpp's c*q_eff*delta*Input,
// no damping — zero at the omega the round-trip assumes). Written from the
// SAME aero.h pieces the plant uses, sampling SPATIAL density, so
// "reproduces tau_cmd" is a real single-count check, not a tautology against
// plant_invert's own math (the AT-18b discipline, spatialized).
double authority_torque_at(double input, double c, double V,
                           const glm::dvec3& pos, const sim::Environment* env,
                           const sim::AircraftParams& p) {
    return c * sim::q_eff(sim::q_dyn(sim::rho_at(pos, env, p), V), p) *
           sim::delta_max_eff(V, p) * input;
}

}  // namespace

TEST_CASE("R6 atm_frac_at: null env/atm is bit-identical to the scalar taper") {
    // Probe ABOVE taper_alt so atm_frac(alt) < 1 — the anti-no-op (a fixture
    // at f == 1 hides everything). The null path must be the SAME arithmetic.
    const double h = 6000.0;
    const glm::dvec3 pos = at(kCenter, h);
    const double base = sim::atm_frac(sim::altitude(pos, kP), kP);
    REQUIRE(base < 1.0);  // the taper is genuinely acting here

    sim::Environment empty;  // every field null
    sim::AtmosphereField af = one_bubble();
    sim::Environment atm_null;  // env present, atm null
    atm_null.grav = nullptr;

    CHECK(sim::atm_frac_at(pos, nullptr, kP) == base);  // null env
    CHECK(sim::atm_frac_at(pos, &empty, kP) == base);   // env, atm null
    CHECK(sim::atm_frac_at(pos, &atm_null, kP) == base);
    // rho_at(position,env,p) == p.rho * atm_frac_at
    CHECK(sim::rho_at(pos, nullptr, kP) == kP.rho * base);
}

TEST_CASE("R6 atm_frac_at: in-bubble-core is EXACTLY the baseline taper") {
    // A point at the bubble center, below the ceiling and below taper_alt:
    // u == 1.0 exactly (the complement-product's exact-1 plateau), so the
    // spatial factor is a true no-op — flight in your home bubble core is
    // bit-identical to the frozen kernel.
    const glm::dvec3 pos = at(kCenter, 2000.0);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const double base = sim::atm_frac(sim::altitude(pos, kP), kP);
    REQUIRE(base == 1.0);                            // below taper_alt
    CHECK(sim::atm_frac_at(pos, &env, kP) == base);  // EXACT, not approx
}

TEST_CASE("R6 atm_frac_at: bubble EDGE thins below baseline (u isolated)") {
    // Mid-edge: arc = ground_radius + edge_soft/2 (old u_h = 1-smoothstep(.5)
    // = 0.5); altitude 2000 (< ceiling 4000, < taper_alt) => OLD u_v = 1
    // exactly (independent-axis product, alt term never engaged).
    //
    // S-domeround (docs/airdome_round_spec.md §1) SUPERSEDES the "== 0.5"
    // reading: alt=2000 is NOT on the arc=0 axis, so it is no longer
    // vertically inert — the meridional norm folds alt_p/H=2000/4000=0.5
    // into the SAME distance the horizontal offset uses (H falls back to
    // the literal ceiling_m here since one_bubble()'s Bubble doesn't set
    // dome_h_m, and af.dome_exponent defaults to 3.0). This point is
    // genuinely off both pure axes, so the corner rounds it thinner than
    // the old per-axis reading — offline-derived (Python, s/d/w worked by
    // hand from the spec formula) and cross-checked against the live field
    // before pinning: u ~= 0.207489, not 0.5. This is the SAME class of
    // move the spec's own §5 leg 3 proves is intended, not a regression.
    const double arc = 6000.0 + 1200.0 / 2.0;  // 6600 m
    const glm::dvec3 pos = at(offset_dir(arc), 2000.0);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const double base = sim::atm_frac(sim::altitude(pos, kP), kP);
    REQUIRE(base == 1.0);  // spatial isolated
    const double f = sim::atm_frac_at(pos, &env, kP);
    CHECK(f < 0.9);  // genuinely thinned
    CHECK(f == Catch::Approx(0.20748909094246104).epsilon(1e-9));
}

TEST_CASE("R6 atm_frac_at: outside all zones above the deck is EXACT vacuum") {
    // Far from the bubble (arc >> radius + soft) and above the deck: u == 0
    // exactly (the smoothstep's exact-0 plateau — no subnormal tail). This is
    // the near-vacuum the v1 hard-stall emerges from.
    const glm::dvec3 pos = at(offset_dir(60000.0), 2000.0);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;
    CHECK(sim::atm_frac_at(pos, &env, kP) == 0.0);  // EXACT zero
}

TEST_CASE("R6 atm_frac_at: the global deck keeps low air breathable outside") {
    // Below deck_agl_m, far from every bubble: full baseline air (the
    // go-anywhere floor — you can land outside your bubble).
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 low = at(offset_dir(60000.0), 50.0);  // 50 m AGL
    const double base = sim::atm_frac(sim::altitude(low, kP), kP);
    CHECK(sim::atm_frac_at(low, &env, kP) == base);  // deck u == 1
}

// ---------------------------------------------------------------------------
// RUNG E16 — THE DECK'S FRAME (docs/ENEMY_AI_E1_E2_SPEC.md SS E16.D).
// The dial above says "full air below deck_agl_m ABOVE THE SURFACE,
// EVERYWHERE (the go-anywhere floor: you can still land outside a bubble)".
// On a bare sphere the sphere frame and the terrain frame are the same
// sentence, which is why R6 could ship the sphere one. Under the real DEM
// (relief_scale 350 m, 69% of the surface above deck_agl_m) they are not, and
// the go-anywhere floor ends up underground.
//
// THE LEGS ARE WRITTEN AGAINST THE RULE, NEVER AGAINST A SHIPPED NUMBER (the
// ladder's own recurring trap): leg 1 asks whether a pilot 50 m over HIGH
// GROUND can breathe, which is the ruling, and leg 2 asks the flag to be an
// exact no-op, which is the firewall. Neither reads config.
// ---------------------------------------------------------------------------
TEST_CASE("E16 deck frame: the go-anywhere floor follows the TERRAIN") {
    // Terrain high enough that the whole sphere-framed deck (120 + 200 m) is
    // buried under it -- the shipped DEM's top decile, not an invented case.
    constexpr double kElev = 330.0;
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kP.R;
    hf.relief_scale = 1000.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(kElev / hf.relief_scale * 65535.0 +
                                            0.5));

    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;
    env.ground = &hf;

    // 50 m above the ground, far outside every bubble: the pilot who has just
    // flown out of his dome and wants to land.
    const glm::dvec3 dir = offset_dir(60000.0);
    const glm::dvec3 low = at(dir, kElev + 50.0);
    const double base = sim::atm_frac(sim::altitude(low, kP), kP);

    // (1) THE SPHERE FRAME SUFFOCATES HIM. Not "less air" -- none: 380 m of
    // altitude is past the deck's fade (320 m), so u == 0 exactly.
    af.deck_terrain_relative = false;
    CHECK(sim::atm_frac_at(low, &env, kP) == 0.0);

    // (2) THE TERRAIN FRAME IS THE RULING: 50 m AGL is inside deck_agl_m, so
    // the deck term saturates and the air is the plain baseline taper.
    af.deck_terrain_relative = true;
    CHECK(sim::atm_frac_at(low, &env, kP) == base);
    CHECK(base > 0.9);  // fixture guard: the probe is in the fight band

    // (3) AND IT IS STILL A DECK, NOT A GIFT OF AIR -- well above the terrain
    // the vacuum is exactly as it was. The dial moves the floor, it does not
    // raise the roof.
    const glm::dvec3 high = at(dir, kElev + 2000.0);
    CHECK(sim::atm_frac_at(high, &env, kP) == 0.0);
}

TEST_CASE("E16 deck frame: OFF is the R6 sphere deck, bit-for-bit") {
    // The superset firewall, both ways: the flag off must reproduce R6
    // exactly, and the flag ON with no ground field must too (the frozen-
    // kernel path never grew a terrain reference).
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kP.R;
    hf.relief_scale = 1000.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(0.25 * 65535.0));

    sim::AtmosphereField af = one_bubble();
    sim::AtmosphereField ref = one_bubble();
    sim::Environment env;
    env.atm = &af;
    env.ground = &hf;
    sim::Environment env_ref;
    env_ref.atm = &ref;  // no ground: the R6 world

    bool any_differ = false;
    for (double arc : {0.0, 3000.0, 7000.0, 20000.0, 60000.0}) {
        const glm::dvec3 dir = offset_dir(arc);
        for (double h : {10.0, 100.0, 250.0, 319.0, 500.0, 3000.0}) {
            const glm::dvec3 pos = at(dir, h);
            const double r6 = sim::atm_frac_at(pos, &env_ref, kP);
            af.deck_terrain_relative = false;
            CHECK(sim::atm_frac_at(pos, &env, kP) == r6);
            af.deck_terrain_relative = true;
            if (sim::atm_frac_at(pos, &env, kP) != r6) any_differ = true;
        }
    }
    // THE ANTI-VACUOUS LEG: if the ON arm never differed anywhere, the two
    // CHECKs above would pass with the mechanism deleted.
    CHECK(any_differ);

    // ON with no ground field is the R6 deck too.
    ref.deck_terrain_relative = true;
    for (double h : {10.0, 250.0, 500.0}) {
        const glm::dvec3 pos = at(offset_dir(60000.0), h);
        sim::AtmosphereField plain = one_bubble();
        sim::Environment env_plain;
        env_plain.atm = &plain;
        CHECK(sim::atm_frac_at(pos, &env_ref, kP) ==
              sim::atm_frac_at(pos, &env_plain, kP));
    }
}

TEST_CASE("R6 atm_frac_at: overlapping bubbles never exceed baseline") {
    // Two bubbles whose cores overlap at the probe: the complement-product
    // union saturates at u == 1 (a p-norm/LSE max would fly denser-than-
    // baseline air here). The point sits in BOTH cores.
    sim::AtmosphereField af;
    const glm::dvec3 c2 = glm::normalize(kCenter + glm::dvec3{0.02, 0.0, 0.01});
    af.bubbles.push_back(
        sim::AtmosphereField::Bubble{kCenter, 6000.0, 4000.0, 1200.0, 600.0});
    af.bubbles.push_back(
        sim::AtmosphereField::Bubble{c2, 6000.0, 4000.0, 1200.0, 600.0});
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 pos = at(kCenter, 1000.0);  // both cores
    const double base = sim::atm_frac(sim::altitude(pos, kP), kP);
    CHECK(sim::atm_frac_at(pos, &env, kP) == base);  // exactly 1*base
    CHECK(sim::atm_frac_at(pos, &env, kP) <= base);  // never denser
}

TEST_CASE("R6 AT-18b spatial: the inversion samples the plant bubble rho") {
    // The fork the round is ABOUT: at a thinning edge, plant_invert must use
    // the SAME density the plant applies, or the deflection is off by 1/u.
    const double arc = 6000.0 + 1200.0 / 2.0;  // mid-edge, u_h=0.5
    const glm::dvec3 pos = at(offset_dir(arc), 2000.0);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const double V = 140.0;
    const double c = kP.c_pitch;
    const double u = sim::atm_frac_at(pos, &env, kP) /
                     sim::atm_frac(sim::altitude(pos, kP), kP);
    REQUIRE(u < 0.9);  // edge genuinely thin

    // A sub-saturation command at the SPATIAL authority.
    const double tau =
        0.4 * c * sim::q_eff(sim::q_dyn(sim::rho_at(pos, &env, kP), V), kP) *
        sim::delta_max_eff(V, kP);

    // Correct (spatial) inversion round-trips exactly.
    const double in_ok = control::plant_invert(tau, c, V, pos, &env, kP);
    CHECK(std::abs(in_ok) < 1.0);  // below saturation
    CHECK(authority_torque_at(in_ok, c, V, pos, &env, kP) ==
          Catch::Approx(tau).epsilon(1e-12));

    // MUTATION: the STALE altitude inversion (no bubble) under-deflects — fed
    // through the plant's real (spatial) torque it produces only tau*u, so the
    // round-trip is off by the bubble factor. The fork is detected.
    const double alt = sim::altitude(pos, kP);
    const double in_stale = control::plant_invert(tau, c, V, alt, kP);
    const double reproduced =
        authority_torque_at(in_stale, c, V, pos, &env, kP);
    CHECK(reproduced == Catch::Approx(tau * u).epsilon(1e-9));
    CHECK(std::abs(reproduced - tau) > 0.05 * std::abs(tau));  // MUST fork
}

TEST_CASE("R6 every-consumer sweep: derived helpers scale by the SAME u") {
    // load_factor and ang_accel_max_derived at the edge must both thin by u
    // vs their altitude-baseline values — no consumer left on stale density.
    const double arc = 6000.0 + 1200.0 / 2.0;
    const glm::dvec3 pos = at(offset_dir(arc), 2000.0);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const double alt = sim::altitude(pos, kP);
    const double u = sim::atm_frac_at(pos, &env, kP) / sim::atm_frac(alt, kP);
    REQUIRE(u < 0.9);

    const double V = 140.0, alpha = 0.05, flap = 0.0;
    // n scales linearly with rho (q ∝ rho), so n_spatial / n_alt == u.
    const double n_alt = sim::load_factor(alpha, V, alt, flap, kP);
    const double n_pos = sim::load_factor(alpha, V, pos, &env, flap, kP);
    REQUIRE(std::abs(n_alt) > 1e-6);  // baseline term > eps
    CHECK(n_pos / n_alt == Catch::Approx(u).epsilon(1e-9));

    // ang_accel above the q_att_floor also scales by u (q dominates the floor
    // at V=140: q ~ 9800 Pa >> floor).
    const double a_alt =
        sim::ang_accel_max_derived(kP.c_pitch, kP.I_pitch, V, alt, kP);
    const double a_pos =
        sim::ang_accel_max_derived(kP.c_pitch, kP.I_pitch, V, pos, &env, kP);
    CHECK(a_pos / a_alt == Catch::Approx(u).epsilon(1e-9));
}

TEST_CASE("R6 atm_falloff: exact 0/1 plateaus (no subnormal tail)") {
    CHECK(sim::atm_falloff(-1.0, 1000.0) == 1.0);    // inside the core
    CHECK(sim::atm_falloff(0.0, 1000.0) == 1.0);     // exactly at the edge
    CHECK(sim::atm_falloff(1000.0, 1000.0) == 0.0);  // exactly at soft
    CHECK(sim::atm_falloff(1e6, 1000.0) == 0.0);     // far past
    CHECK(sim::atm_falloff(500.0, 0.0) == 0.0);      // soft <= 0 guard
    CHECK(sim::atm_falloff(500.0, 1000.0) ==
          Catch::Approx(0.5));  // 1-smoothstep(.5)
}

TEST_CASE("R6 control::step wiring: the LIVE cascade samples the bubble rho") {
    // P1-1 (adversarial after-review): the ONLY tripwire on the round's
    // central claim at the CALL SITE. The AT-18b spatial leg pins plant_invert
    // the FUNCTION; this pins that control::step actually CALLS the
    // position+env overload — revert controller.cpp's call sites to the
    // altitude overload and the edge leg below goes silent (the moved-consumer
    // trap, verbatim).
    const control::ControllerParams kCp =
        cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kP);
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    const double V = 140.0, h = 2000.0;  // below taper_alt: bubble u isolated
    const glm::dvec3 up_edge = offset_dir(6000.0 + 1200.0 / 2.0);  // u ~ 0.5
    const glm::dvec3 up_core = kCenter;                            // u == 1

    auto emit = [&](const glm::dvec3& up, const sim::Environment* e) {
        const glm::dvec3 head =
            glm::normalize(glm::cross(up, glm::dvec3{0.11, 0.07, 1.0}));
        const sim::SimState s =
            harness::flight_state(kP, V, h, up, head, 0.0, 0.0, 0.0);
        const glm::dvec3 nose = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        const glm::dvec3 rb = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
        control::Input in;
        in.target_dir_world =  // a small PITCH-up aim: direct proportional
            glm::normalize(glm::angleAxis(0.02, rb) * nose);  // pursuit, gentle
        in.throttle = 0.7;
        return control::step(s, in, control::reset(), kP, kCp, e, kP.sim_dt)
            .inputs;
    };

    // EDGE (u ~ 0.5): the null-env inversion divides by baseline density, the
    // live-env inversion by the thinner bubble density — so the emitted
    // PITCH deflection DIFFERS. Require the null command below saturation so
    // the difference is a real 1/u effect, not two clamps hiding it.
    const sim::Inputs en = emit(up_edge, nullptr);
    const sim::Inputs el = emit(up_edge, &env);
    REQUIRE(std::abs(en.pitch) > 1e-4f);  // baseline term > eps (no-op guard)
    REQUIRE(std::abs(en.pitch) < 0.9f);   // below saturation
    CHECK(std::abs(el.pitch - en.pitch) > 1e-3);  // wiring carries bubble rho

    // CORE (u == 1): the bubble density EQUALS baseline, so the emitted inputs
    // are BIT-IDENTICAL — proving the edge difference is the THINNING bubble,
    // not env-presence alone (the mutation anchor).
    const sim::Inputs cn = emit(up_core, nullptr);
    const sim::Inputs cl = emit(up_core, &env);
    CHECK(cl.roll == cn.roll);
    CHECK(cl.pitch == cn.pitch);
    CHECK(cl.yaw == cn.yaw);
}

// -- TRUE-ELLIPSE bubbles (conquest, Chad's 2026-07-25 ruling) --------------
// sim/fields.h::AtmosphereField::Bubble grew `major_axis` + `minor_radius_m`
// (both defaulted off — 0-vector / 0.0 = legacy circular); sim/aero.h's
// atm_frac_at gates the whole ellipse branch on `minor_radius_m > 0.0`. These
// legs pin: (1) the legacy circular path is BIT-IDENTICAL (same operations,
// not just numerically close) with the new fields at their defaults; (2) the
// ellipse anisotropy itself (on-axis vs 90-degree-bearing at the same arc);
// (3) both degenerate guards (arc~0 at the center; a degenerate |major_axis|).

TEST_CASE(
    "R6 ellipse: minor_radius_m==0 is bit-identical to the legacy circle") {
    // Two bubbles at the SAME center/radius/softness: one built via the bare
    // (pre-ellipse) 5-field aggregate, one via the full 7-field aggregate with
    // major_axis{0} + minor_radius_m 0.0 explicitly. Scattered probe points
    // (in-core, mid-edge, far-outside, near-ceiling) must read EXACTLY equal —
    // not Approx — proving the ellipse branch is skipped, not merely small.
    sim::AtmosphereField::Bubble legacy{kCenter, 6000.0, 4000.0, 1200.0, 600.0};
    sim::AtmosphereField::Bubble explicit_default{
        kCenter, 6000.0, 4000.0, 1200.0, 600.0, glm::dvec3{0.0}, 0.0};

    sim::AtmosphereField af_legacy;
    af_legacy.bubbles.push_back(legacy);
    sim::AtmosphereField af_explicit;
    af_explicit.bubbles.push_back(explicit_default);
    sim::Environment env_legacy;
    env_legacy.atm = &af_legacy;
    sim::Environment env_explicit;
    env_explicit.atm = &af_explicit;

    // 200+ scattered points: a spread of bearings (via offset_dir on assorted
    // arcs) x a spread of altitudes, exercising in-core / mid-soft / far /
    // near-ceiling regimes.
    int checked = 0;
    for (double arc : {0.0, 1500.0, 4000.0, 6000.0, 6600.0, 7200.0, 9000.0,
                       20000.0, 60000.0}) {
        for (double h :
             {50.0, 500.0, 2000.0, 3800.0, 4000.0, 4300.0, 4600.0, 6000.0}) {
            for (int k = 0; k < 3; ++k) {
                const double theta =
                    arc / kP.R + 0.001 * k;  // slight bearing jitter
                const glm::dvec3 tangent = glm::normalize(glm::cross(
                    kCenter, glm::dvec3{0.11 + 0.01 * k, 0.07, 1.0}));
                const glm::dvec3 dir = glm::normalize(
                    std::cos(theta) * kCenter + std::sin(theta) * tangent);
                const glm::dvec3 pos = at(dir, h);
                const double f_legacy = sim::atm_frac_at(pos, &env_legacy, kP);
                const double f_explicit =
                    sim::atm_frac_at(pos, &env_explicit, kP);
                CHECK(f_legacy == f_explicit);  // EXACT, not Approx
                // ORACLE arm (direct-check P2 fold): the two arms above share
                // the SAME shipped code path, so their equality alone is a
                // tautology. Recompute the PRE-ELLIPSE circular expression by
                // hand (deliberately duplicated — the fork detector, AT-12
                // discipline) and require EXACT equality against the shipped
                // result. Mutation this kills: any reassociation/extra-op on
                // the minor==0 path (e.g. routing legacy bubbles through the
                // ellipse r_eff with cos=1).
                //
                // S-domeround (docs/airdome_round_spec.md §1): the union term
                // is now the superellipse dome law, not the old u_h*u_v
                // product — updated here in lockstep (both bubbles have
                // dome_h_m unset, so H falls back to the literal ceiling_m
                // 4000, and af.dome_exponent defaults to 3.0; both arms
                // share that same default, so the oracle mirrors it).
                {
                    const double alt = sim::altitude(pos, kP);
                    double one_minus =
                        1.0 - sim::atm_falloff(alt - af_legacy.deck_agl_m,
                                               af_legacy.deck_soft_m);
                    const double cc = glm::clamp(
                        glm::dot(glm::normalize(pos), glm::normalize(kCenter)),
                        -1.0, 1.0);
                    const double arc_o = kP.R * std::acos(cc);
                    const double alt_p = std::max(alt, 0.0);
                    constexpr double kH =
                        4000.0;  // sentinel fallback == ceiling_m
                    constexpr double kN =
                        3.0;  // AtmosphereField::dome_exponent default
                    double u;
                    const double rho = std::sqrt(arc_o * arc_o + alt_p * alt_p);
                    if (rho <= 0.0) {
                        u = 1.0;
                    } else {
                        const double s = std::pow(std::pow(arc_o / 6000.0, kN) +
                                                      std::pow(alt_p / kH, kN),
                                                  1.0 / kN);
                        const double d = rho * (s - 1.0) / s;
                        const double w =
                            std::pow(alt_p / kH, kN) / std::pow(s, kN);
                        const double soft = 1200.0 * (1.0 - w) + 600.0 * w;
                        u = sim::atm_falloff(d, soft);
                    }
                    one_minus *= (1.0 - u);
                    const double oracle =
                        sim::atm_frac(alt, kP) * (1.0 - one_minus);
                    CHECK(f_explicit == oracle);  // EXACT
                }
                ++checked;
            }
        }
    }
    REQUIRE(checked >= 200);
}

TEST_CASE(
    "R6 ellipse: anisotropy -- on-axis reaches further than 90-degree "
    "bearing") {
    // a = 12000 (semi-major), b = 5000 (semi-minor), major_axis = a generic
    // tangent at kCenter (not aligned with any world axis — S8-drone lesson).
    const glm::dvec3 major =
        glm::normalize(glm::cross(kCenter, glm::dvec3{0.11, 0.07, 1.0}));
    sim::AtmosphereField::Bubble ell{kCenter, 12000.0, 4000.0, 1200.0,
                                     600.0,   major,   5000.0};
    sim::AtmosphereField af;
    af.bubbles.push_back(ell);
    sim::Environment env;
    env.atm = &af;

    // On-axis (bearing == major_axis direction), 0.9*a = 10800 m: inside
    // (r_eff(0) == a == 12000 > 10800).
    const glm::dvec3 on_axis_dir = glm::normalize(
        std::cos(10800.0 / kP.R) * kCenter + std::sin(10800.0 / kP.R) * major);
    const glm::dvec3 pos_on_axis = at(on_axis_dir, 500.0);
    const double f_on_axis = sim::atm_frac_at(pos_on_axis, &env, kP);
    CHECK(f_on_axis ==
          Catch::Approx(1.0).epsilon(1e-6));  // inside the hard core

    // 90-degree bearing (perpendicular to major_axis), SAME arc distance
    // 10800 m: r_eff(90deg) == b == 5000 < 10800, so this point is OUTSIDE
    // (near-vacuum) even though it's the identical distance from center that
    // was safely inside on-axis — the anisotropy itself.
    const glm::dvec3 perp = glm::normalize(glm::cross(kCenter, major));
    const glm::dvec3 off_axis_dir = glm::normalize(
        std::cos(10800.0 / kP.R) * kCenter + std::sin(10800.0 / kP.R) * perp);
    const glm::dvec3 pos_off_axis = at(off_axis_dir, 500.0);
    const double f_off_axis = sim::atm_frac_at(pos_off_axis, &env, kP);
    CHECK(f_off_axis < 0.1);  // genuinely outside — the anisotropy proof
}

TEST_CASE(
    "R6 ellipse: degenerate guards -- arc~0 and |major_axis|~0 fall back") {
    // (a) arc ~ 0 (dead center): the bearing is undefined; the sampler must
    // use minor_radius_m without dividing by ~0 (no NaN) and read full core.
    {
        const glm::dvec3 major =
            glm::normalize(glm::cross(kCenter, glm::dvec3{0.11, 0.07, 1.0}));
        sim::AtmosphereField::Bubble ell{kCenter, 12000.0, 4000.0, 1200.0,
                                         600.0,   major,   5000.0};
        sim::AtmosphereField af;
        af.bubbles.push_back(ell);
        sim::Environment env;
        env.atm = &af;
        const glm::dvec3 pos = at(kCenter, 500.0);  // arc == 0 exactly
        const double f = sim::atm_frac_at(pos, &env, kP);
        CHECK(std::isfinite(f));
        CHECK(f == Catch::Approx(1.0).epsilon(1e-6));
    }
    // (b) |major_axis| ~ 0 (degenerate axis): falls back to CIRCULAR using
    // ground_radius_m (== a) — a point at 0.9*a on any bearing reads inside,
    // including the "off-axis" direction that a live ellipse would exclude.
    {
        const glm::dvec3 major_zero{1e-15, 0.0, 0.0};  // effectively zero after
                                                       // re-orthogonalization
        sim::AtmosphereField::Bubble ell{kCenter, 12000.0,    4000.0, 1200.0,
                                         600.0,   major_zero, 5000.0};
        sim::AtmosphereField af;
        af.bubbles.push_back(ell);
        sim::Environment env;
        env.atm = &af;
        const glm::dvec3 tangent =
            glm::normalize(glm::cross(kCenter, glm::dvec3{0.3, 0.9, 0.1}));
        const double arc =
            10800.0;  // 0.9 * a; would be OUTSIDE a b=5000 ellipse
                      // off-axis, but inside a a=12000 circle
        const glm::dvec3 dir = glm::normalize(std::cos(arc / kP.R) * kCenter +
                                              std::sin(arc / kP.R) * tangent);
        const glm::dvec3 pos = at(dir, 500.0);
        const double f = sim::atm_frac_at(pos, &env, kP);
        CHECK(std::isfinite(f));
        CHECK(f ==
              Catch::Approx(1.0).epsilon(1e-6));  // circular fallback, not thin
    }
}

TEST_CASE(
    "R6 in-core step: live atm env is bit-identical to null at the core") {
    // A full sim::step at a bubble-core point below taper (atm-only env, grav
    // null) must match the null-env step EXACTLY — the all-null bit-identity
    // proof extended: an ACTIVE field that happens to be transparent here
    // cannot move the state.
    sim::AtmosphereField af = one_bubble();
    sim::Environment env;
    env.atm = &af;

    sim::SimState s{};
    s.position = at(kCenter, 2000.0);  // core, below ceiling
    s.velocity =
        glm::normalize(glm::cross(kCenter, glm::dvec3{0, 0, 1})) * 120.0;
    s.orientation = glm::dquat{1.0, 0.0, 0.0, 0.0};
    s.throttle = 0.7;
    s.last_vhat = glm::normalize(s.velocity);

    sim::Inputs in{};
    const sim::SimState a = sim::step(s, in, kP, nullptr, kP.sim_dt);
    const sim::SimState b = sim::step(s, in, kP, &env, kP.sim_dt);
    CHECK(a.position == b.position);
    CHECK(a.velocity == b.velocity);
    CHECK(a.orientation == b.orientation);
    CHECK(a.throttle == b.throttle);
}
