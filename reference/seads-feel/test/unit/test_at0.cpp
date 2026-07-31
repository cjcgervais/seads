// AT-0 — sign conventions vs glm reality (SPEC §7 table, §9.3, §9.7;
// HARNESS §5). Runs BEFORE any gain exists or any tuning happens: these
// tests pin every geometry -> sign mapping the 4b cascade will consume —
// pointing demand per body axis, bank-to-turn roll, coordination yaw from
// beta, FINE roll-hold from (phi_held - phi) — plus the shared extraction
// (control/extract.h) driven end-to-end by synthetic SimStates, so the
// GLUE is tested, not injected past (SPEC §9.7: a sign flip in extraction
// is the positive-feedback failure class).
//
// Fixture discipline (CLAUDE.md S1/S3): crafted states come from
// harness::flight_state, which builds geometry from raw rotations and raw
// velocity components — never from the formulas under test — and every
// through-extraction case first asserts a PHYSICAL predicate on the fixture
// itself (wingtip altitude, velocity-along-body-right) so a sign bug cannot
// live in both instrument and mechanism and cancel.
//
// Canonical cases run in TWO world frames (the +X-pole golden frame and a
// fully skew one — no state may alias body axes to world axes) and at TWO
// banks (0 deg and 60 deg — in level flight body_up == local_up and a
// wrong-basis demand law is invisible; the banked frame separates them).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "config/load_aircraft.h"
#include "control/controller.h"
#include "control/extract.h"
#include "test/harness/injector.h"

#ifdef NDEBUG
#error \
    "SEADS gate requires an assert-live build (SPEC 6.1); configure with CMAKE_BUILD_TYPE=Debug"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1e-7;  // exactness pin on double-precision geometry
inline double rad(double deg) { return deg * kPi / 180.0; }

struct WorldFrame {
    const char* name;
    glm::dvec3 up;
    glm::dvec3 heading;
};

// Frame A: the +X pole, nose -Z — non-identity q, but axis-aligned.
// Frame B: skew point, skew heading — NO body axis lies on a world axis,
// so any world-for-body axis confusion in the code under test must show.
const WorldFrame kFrames[] = {
    {"pole+X", {1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}},
    {"skew", {3.0, -2.0, 5.0}, {1.0, 1.0, -0.3}},
};

const double kBanks[] = {0.0, rad(60.0)};

glm::dvec3 body_right_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{1.0, 0.0, 0.0};
}
glm::dvec3 body_up_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 1.0, 0.0};
}
glm::dvec3 nose_of(const sim::SimState& s) {
    return s.orientation * glm::dvec3{0.0, 0.0, -1.0};
}

}  // namespace

// ---------------------------------------------------------------------------
// Canonical targets -> omega_des signs per the SPEC §7 table:
//   pitch-up = +x, yaw-right = -y, roll-right = -z.
// Targets are built by rotating the nose about BODY axes (+about body_right
// = up, -about body_up = right — signs derived from omega x r and pinned
// here against glm's angleAxis reality).
// ---------------------------------------------------------------------------
TEST_CASE("AT-0 canonical: pointing demand signs, two frames x two banks") {
    for (const auto& f : kFrames) {
        for (double bank : kBanks) {
            CAPTURE(f.name, bank);
            const sim::SimState s = harness::flight_state(
                kP, 140.0, 2000.0, f.up, f.heading, bank, 0.0, 0.0);
            const glm::dvec3 nose = nose_of(s);
            const glm::dvec3 right = body_right_of(s);
            const glm::dvec3 up_b = body_up_of(s);
            const auto demand = [&](const glm::dvec3& target) {
                return control::rotation_demand_body(s.orientation, target);
            };

            // Aligned: zero demand.
            CHECK(glm::length(demand(nose)) < kTol);

            // 15 deg RIGHT: -rotation about body_up carries the nose right.
            // Each constructed target carries its own physical predicate —
            // a mirrored misreading of angleAxis in fixture AND law must
            // not co-pass (the instrument-shares-the-bug trap).
            const glm::dvec3 t_right = glm::angleAxis(-rad(15.0), up_b) * nose;
            REQUIRE(glm::dot(t_right, right) > 0.1);
            glm::dvec3 d = demand(t_right);
            CHECK(d.y < -rad(14.0));  // yaw-right = -omega_y
            CHECK(std::abs(d.x) < kTol);
            // (d.z == 0 is structural for a pure-yaw offset: cross((0,0,-1),
            // t) has no z component — a NaN canary, not roll coverage.)
            CHECK(std::abs(d.z) < kTol);
            CHECK(glm::length(d) == Catch::Approx(rad(15.0)).margin(kTol));

            // 15 deg LEFT.
            const glm::dvec3 t_left = glm::angleAxis(rad(15.0), up_b) * nose;
            REQUIRE(glm::dot(t_left, right) < -0.1);
            d = demand(t_left);
            CHECK(d.y > rad(14.0));

            // 15 deg UP: +rotation about body_right carries the nose up.
            const glm::dvec3 t_up = glm::angleAxis(rad(15.0), right) * nose;
            REQUIRE(glm::dot(t_up, up_b) > 0.1);
            d = demand(t_up);
            CHECK(d.x > rad(14.0));  // pitch-up = +omega_x
            CHECK(std::abs(d.y) < kTol);
            CHECK(glm::length(d) == Catch::Approx(rad(15.0)).margin(kTol));

            // 15 deg DOWN and 60 deg DOWN: both pitch-down at the geometric
            // level (whether 4b PUSHES or ROLLS THROUGH is the §9.3 G-gate's
            // call, tested by AT-15 — the demand sign itself is not modal).
            const glm::dvec3 t_down15 =
                glm::angleAxis(-rad(15.0), right) * nose;
            REQUIRE(glm::dot(t_down15, up_b) < -0.1);
            d = demand(t_down15);
            CHECK(d.x < -rad(14.0));
            d = demand(glm::angleAxis(-rad(60.0), right) * nose);
            CHECK(d.x < -rad(59.0));
            CHECK(glm::length(d) == Catch::Approx(rad(60.0)).margin(kTol));

            // ASTERN: axis geometrically ambiguous — the documented
            // tie-break is +X, the pitch-up pull-through.
            d = demand(-nose);
            CHECK(d.x == Catch::Approx(kPi).margin(kTol));
            CHECK(std::abs(d.y) < kTol);
            CHECK(std::abs(d.z) < kTol);

            // NEAR-astern, 1 deg above: shortest arc is a pitch-UP
            // pull-through, magnitude 179 deg.
            d = demand(glm::angleAxis(rad(179.0), right) * nose);
            CHECK(d.x > rad(170.0));
            CHECK(glm::length(d) == Catch::Approx(rad(179.0)).margin(kTol));

            // NEAR-astern, 1 deg BELOW: the shortest arc flips wholesale to
            // pitch-DOWN — inherent to shortest-arc, ON PURPOSE here. The
            // measure-zero +X tie-break stabilizes nothing in flight; 4b's
            // near-astern elev-sign latch (SPEC §9.3, e > 170 deg) is the
            // real anti-dither mechanism and must be applied DOWNSTREAM of
            // this primitive, overriding this sign.
            d = demand(glm::angleAxis(-rad(179.0), right) * nose);
            CHECK(d.x < -rad(170.0));
        }
    }
}

// ---------------------------------------------------------------------------
// Bank-to-turn (MANEUVER): roll to put the target above the nose.
// bank_error takes the target in BODY coordinates, so cases are stated
// directly in body components (x = right, y = up, z = back).
// ---------------------------------------------------------------------------
TEST_CASE("AT-0 canonical: bank-to-turn roll demand signs") {
    // Target level-right of the nose: bank error +90 deg, roll RIGHT.
    glm::dvec3 t = glm::normalize(glm::dvec3{0.26, 0.0, -0.97});
    CHECK(control::bank_error(t) == Catch::Approx(kPi / 2.0).margin(kTol));
    CHECK(control::maneuver_roll_demand(t) < -1.0);  // -omega_z = roll right

    // Target level-left: mirror.
    t = glm::normalize(glm::dvec3{-0.26, 0.0, -0.97});
    CHECK(control::maneuver_roll_demand(t) > 1.0);

    // Target up-right 45 deg off the up axis: half-way roll right.
    t = glm::normalize(glm::dvec3{0.3, 0.3, -0.9});
    CHECK(control::bank_error(t) == Catch::Approx(kPi / 4.0).margin(kTol));
    CHECK(control::maneuver_roll_demand(t) ==
          Catch::Approx(-kPi / 4.0).margin(kTol));

    // Target already above the nose: no roll.
    t = glm::normalize(glm::dvec3{0.0, 0.3, -0.95});
    CHECK(std::abs(control::maneuver_roll_demand(t)) < kTol);

    // BELOW the horizon (target_body.y < 0) — MANEUVER's bread-and-butter
    // regime, and where the |bankErr| > 135 deg direction latch lives: the
    // error must keep growing PAST 90 deg (a magnitude-capping bug here
    // silently disables roll-through-inverted in 4b).
    t = glm::normalize(glm::dvec3{0.2, -0.5, -0.84});
    CHECK(control::bank_error(t) ==
          Catch::Approx(std::atan2(0.2, -0.5)).margin(kTol));  // ~ +158 deg
    CHECK(control::bank_error(t) > rad(135.0));
    CHECK(control::maneuver_roll_demand(t) < -rad(135.0));

    t = glm::normalize(glm::dvec3{-0.2, -0.5, -0.84});
    CHECK(control::bank_error(t) < -rad(135.0));
    CHECK(control::maneuver_roll_demand(t) > rad(135.0));

    // Straight below: atan2(+0, -y) = +pi exactly — the deterministic sign
    // the 4b roll-direction latch consumes at the inverted boundary.
    t = glm::normalize(glm::dvec3{0.0, -0.3, -0.95});
    CHECK(control::bank_error(t) == Catch::Approx(kPi).margin(kTol));
    CHECK(control::maneuver_roll_demand(t) == Catch::Approx(-kPi).margin(kTol));
}

// ---------------------------------------------------------------------------
// Injected-state signs (AT-0's middle clause): the numbers the cascade
// feeds these primitives in 4b, injected directly.
// ---------------------------------------------------------------------------
TEST_CASE("AT-0 injected: coordination yaw sign from beta") {
    // beta > 0 = velocity right of the nose -> yaw right = -omega_y.
    CHECK(control::coordination_yaw_demand(rad(10.0)) < -rad(9.0));
    CHECK(control::coordination_yaw_demand(-rad(10.0)) > rad(9.0));
    CHECK(control::coordination_yaw_demand(0.0) == 0.0);
}

TEST_CASE("AT-0 injected: FINE roll-hold sign from (phi_held - phi)") {
    // Banked right of the held bank (phi > phi_held): roll LEFT (+omega_z)
    // back toward it.
    CHECK(control::roll_hold_demand(0.0, rad(20.0)) > rad(19.0));
    // Held bank to the right of current: roll RIGHT (-omega_z) to recapture.
    CHECK(control::roll_hold_demand(rad(10.0), 0.0) < -rad(9.0));
    CHECK(control::roll_hold_demand(rad(30.0), rad(30.0)) == 0.0);
}

// ---------------------------------------------------------------------------
// Through-extraction (AT-0's third clause): synthetic SimStates at known
// bank/sideslip/AoA driven through control::extract — the same signs must
// come out the far end. Each fixture is first validated by a physical
// predicate that does NOT use the extraction formulas.
// ---------------------------------------------------------------------------
TEST_CASE("AT-0 through-extraction: bank and the signed gravity credit") {
    for (const auto& f : kFrames) {
        CAPTURE(f.name);

        // +20 deg = right wing down: the right wingtip must sit CLOSER to
        // the planet center than the CG — pure lengths, no dot-with-up.
        sim::SimState s = harness::flight_state(kP, 140.0, 2000.0, f.up,
                                                f.heading, rad(20.0), 0.0, 0.0);
        REQUIRE(glm::length(s.position + 10.0 * body_right_of(s)) <
                glm::length(s.position));
        control::Extracted e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.phi == Catch::Approx(rad(20.0)).margin(kTol));
        CHECK(std::abs(e.beta) < kTol);
        CHECK(std::abs(e.alpha) < kTol);
        // F2 fold-safe bank: UPRIGHT (cos_phi_theta > 0) unfold_bank is a
        // BIT-EXACT passthrough of phi — nothing in normal flight changes (the
        // golden-freeze guarantee; == not Approx).
        CHECK(control::unfold_bank(e.phi, e.cos_phi_theta) == e.phi);
        // M23: the fold gate is `cos_phi_theta >= 0` — at EXACTLY the zero
        // crossing (knife-edge, body_up ⟂ local_up) unfold_bank must still take
        // the UPRIGHT passthrough (returns phi, not the pi-fold). A `>=`->`>`
        // mutant returns pi-phi here and no closed-loop leg exercises the exact
        // boundary. Pin the predicate itself with an exactly-representable phi
        // (0.25 is a dyadic rational, bit-exact) so equality is meaningful (the
        // S4a "pin the predicate with an exact magnitude" discipline).
        CHECK(control::unfold_bank(0.25, 0.0) == 0.25);
        CHECK(control::unfold_bank(-0.25, 0.0) == -0.25);
        // ...and the FINE roll-hold demand built on the extracted phi rolls
        // LEFT (+omega_z) to level it — the full glue path.
        CHECK(control::roll_hold_demand(0.0, e.phi) > rad(19.0));
        // The exported frame fields are §9.7 published surface — pin them
        // directly (4b/HUD consumers must not inherit a tail-for-nose).
        CHECK(glm::dot(e.nose, nose_of(s)) == Catch::Approx(1.0).margin(kTol));
        CHECK(glm::dot(e.body_right, body_right_of(s)) ==
              Catch::Approx(1.0).margin(kTol));
        CHECK(glm::dot(e.body_up, body_up_of(s)) ==
              Catch::Approx(1.0).margin(kTol));
        CHECK(glm::dot(e.local_up, glm::normalize(s.position)) ==
              Catch::Approx(1.0).margin(kTol));

        // -35 deg = left wing down: right wingtip ABOVE the CG.
        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                  -rad(35.0), 0.0, 0.0);
        REQUIRE(glm::length(s.position + 10.0 * body_right_of(s)) >
                glm::length(s.position));
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.phi == Catch::Approx(-rad(35.0)).margin(kTol));

        // 60 deg bank: the signed gravity credit reads cos(60) = 0.5.
        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading, rad(60.0),
                                  0.0, 0.0);
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.cos_phi_theta == Catch::Approx(0.5).margin(kTol));

        // INVERTED: phi (an asin of the right-wing dip) degenerates to ~0 —
        // cos_phi_theta = -1 is what tells the controller it is upside
        // down, which is exactly why SPEC §9.3 forbids clamping it >= 0.
        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                  rad(180.0), 0.0, 0.0);
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.cos_phi_theta == Catch::Approx(-1.0).margin(kTol));
        CHECK(std::abs(e.phi) < kTol);
        // F2: fully inverted, unfold_bank reads +/-pi (180 deg bank) — NOT the
        // ~0 the folded asin reads. This is what lets the wings-hold roll the
        // plane OUT of an inverted attitude (the gate quit here).
        CHECK(std::abs(control::unfold_bank(e.phi, e.cos_phi_theta)) ==
              Catch::Approx(rad(180.0)).margin(kTol));

        // THE FOLD, pinned executably (roll_hold_demand's validity-domain
        // caveat): at actual bank 120 deg the asin reads phi = +60 deg —
        // LESS than at 90 deg though the bank grew. Inside the fold a
        // wings-hold on phi has inverted feedback; cos_phi_theta < 0 is the
        // gate 4b's FINE regime must use to stay out of it.
        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                  rad(120.0), 0.0, 0.0);
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.phi == Catch::Approx(rad(60.0)).margin(kTol));
        CHECK(e.cos_phi_theta == Catch::Approx(-0.5).margin(kTol));
        // F2: unfold_bank UN-folds the 120 deg bank the asin read as +60: it
        // returns the TRUE, monotone 120 deg (a right roll grew PAST 90 deg, so
        // the wings-hold demand keeps its correct sign instead of reversing).
        CHECK(control::unfold_bank(e.phi, e.cos_phi_theta) ==
              Catch::Approx(rad(120.0)).margin(kTol));
        // ...and a LEFT roll past 90 deg unfolds NEGATIVE (bank -120: phi folds
        // to -60, unfolds to -120) — the sign is preserved through the fold.
        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                  -rad(120.0), 0.0, 0.0);
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(control::unfold_bank(e.phi, e.cos_phi_theta) ==
              Catch::Approx(-rad(120.0)).margin(kTol));
    }
}

TEST_CASE("AT-0 through-extraction: sideslip into the coordination sign") {
    for (const auto& f : kFrames) {
        CAPTURE(f.name);

        // +10 deg = velocity right of the nose: positive component along
        // body_right, by construction and asserted physically.
        sim::SimState s = harness::flight_state(kP, 140.0, 2000.0, f.up,
                                                f.heading, 0.0, rad(10.0), 0.0);
        REQUIRE(glm::dot(s.velocity, body_right_of(s)) > 1.0);
        control::Extracted e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.beta == Catch::Approx(rad(10.0)).margin(kTol));
        // Glue path: the coordination demand yaws RIGHT (-omega_y) onto it.
        CHECK(control::coordination_yaw_demand(e.beta) < -rad(9.0));

        s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading, 0.0,
                                  -rad(10.0), 0.0);
        REQUIRE(glm::dot(s.velocity, body_right_of(s)) < -1.0);
        e = control::extract(s, s.last_vhat, kP.v_dir_eps);
        CHECK(e.beta == Catch::Approx(-rad(10.0)).margin(kTol));
        CHECK(control::coordination_yaw_demand(e.beta) > rad(9.0));
    }
}

TEST_CASE("AT-0 through-extraction: AoA and a combined attitude") {
    const WorldFrame& f = kFrames[1];  // the skew frame — the harder one

    // +8 deg AoA = nose above velocity = velocity dips below the body-up
    // plane (negative component along body_up — the physical predicate).
    sim::SimState s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                            0.0, 0.0, rad(8.0));
    REQUIRE(glm::dot(s.velocity, body_up_of(s)) < -1.0);
    control::Extracted e = control::extract(s, s.last_vhat, kP.v_dir_eps);
    CHECK(e.alpha == Catch::Approx(rad(8.0)).margin(kTol));

    // Combined bank/slip/AoA: all three channels recovered independently.
    s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading, rad(30.0),
                              -rad(5.0), rad(4.0));
    e = control::extract(s, s.last_vhat, kP.v_dir_eps);
    CHECK(e.phi == Catch::Approx(rad(30.0)).margin(kTol));
    CHECK(e.beta == Catch::Approx(-rad(5.0)).margin(kTol));
    CHECK(e.alpha == Catch::Approx(rad(4.0)).margin(kTol));
}

TEST_CASE("AT-0 through-extraction: the v->0 guard holds the caller's vhat") {
    const WorldFrame& f = kFrames[0];
    sim::SimState s = harness::flight_state(kP, 140.0, 2000.0, f.up, f.heading,
                                            0.0, 0.0, 0.0);
    const glm::dvec3 nose = nose_of(s);

    // Speed far below v_dir_eps, direction along body_right: unguarded this
    // would read beta = +90 deg; guarded, extraction must use the held
    // vhat (the nose) and report beta = 0. No NaN anywhere. The SIM's own
    // held copy is POISONED to body_right first: if extraction ever reads
    // SimState.last_vhat instead of the caller's copy (the §9.6 channel-1
    // seam violation), beta reads +90 deg and this fails.
    s.velocity = 0.1 * body_right_of(s);
    s.last_vhat = body_right_of(s);
    control::Extracted e = control::extract(s, nose, kP.v_dir_eps);
    CHECK(glm::length(e.vhat - nose) < kTol);
    CHECK(std::abs(e.beta) < kTol);
    CHECK(std::abs(e.alpha) < kTol);
    CHECK(e.speed == Catch::Approx(0.1).margin(kTol));
    CHECK(std::isfinite(e.phi));
    CHECK(std::isfinite(e.cos_phi_theta));

    // At EXACTLY eps the guard must NOT hold (>= is live velocity) — the
    // boundary sim/aero.h's one-predicate rule exists for. First on the
    // predicate itself with an exactly representable magnitude (sqrt of a
    // square is exact for axis-aligned eps — immune to quat-rotation ulp):
    const glm::dvec3 held{0.0, 0.0, -1.0};
    const glm::dvec3 at_eps = sim::guarded_dir(
        glm::dvec3{kP.v_dir_eps, 0.0, 0.0}, held, kP.v_dir_eps);
    CHECK(glm::length(at_eps - glm::dvec3{1.0, 0.0, 0.0}) < kTol);

    // ...then through extraction: the same lateral velocity reads as real
    // +90 deg sideslip (|eps * body_right| lands exactly on 1.0 with this
    // toolchain; the direct case above keeps the boundary pinned even if a
    // future flag change perturbs that).
    s.velocity = kP.v_dir_eps * body_right_of(s);
    s.last_vhat = nose;
    e = control::extract(s, nose, kP.v_dir_eps);
    CHECK(e.beta == Catch::Approx(kPi / 2.0).margin(kTol));

    // ...and comfortably above, same reading.
    s.velocity = 2.0 * body_right_of(s);
    e = control::extract(s, nose, kP.v_dir_eps);
    CHECK(e.beta == Catch::Approx(kPi / 2.0).margin(kTol));
}
