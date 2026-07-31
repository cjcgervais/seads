// Stage 2 of S8-drone: the lead-angle gunsight (render/gunsight.h). Pure
// deflection-shooting math, pinned against a HAND-COMPUTED intercept and
// mutation-verified — each case names the defect it catches.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>

#include "render/gunsight.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

sim::SimState at(const glm::dvec3& pos, const glm::dvec3& vel) {
    sim::SimState s;
    s.position = pos;
    s.velocity = vel;
    return s;
}

}  // namespace

// ===========================================================================
// Known crossing geometry, hand-computed: shooter at the origin, target 100 m
// out along +X crossing at 60 m/s along +Y, bullet 100 m/s.
//   a = u^2 - s^2 = -6400,  b = 0,  c = 10000  ->  t = 100/sqrt(s^2-u^2)
//     = 100/80 = 1.25 s;  intercept = (100, 75, 0);  lead_dir = (0.8, 0.6, 0).
// MUTATION: drop the v*t term (aim at the target's CURRENT position) ->
// lead_dir collapses to (1,0,0), the deflection vanishes, and the direction
// check fails.
// ===========================================================================
TEST_CASE("lead_solution: hand-computed crossing intercept") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const sim::SimState target = at({100, 0, 0}, {0, 60, 0});
    const render::LeadSolution sol = render::lead_solution(
        shooter, target, 100.0, 1000.0, glm::dvec3{0, 0, 0});

    REQUIRE(sol.valid);
    REQUIRE(sol.in_range);  // range 100 < 1000
    REQUIRE(sol.range == Catch::Approx(100.0).margin(1e-9));
    REQUIRE(sol.time_to_intercept == Catch::Approx(1.25).margin(1e-9));
    REQUIRE(sol.lead_dir.x == Catch::Approx(0.8).margin(1e-9));
    REQUIRE(sol.lead_dir.y == Catch::Approx(0.6).margin(1e-9));  // the LEAD
    REQUIRE(sol.lead_dir.z == Catch::Approx(0.0).margin(1e-9));
}

// A target RECEDING faster than the bullet can never be hit: both roots are
// negative -> valid=false (and the fallback lead_dir stays straight-at-target).
// MUTATION: accept a non-positive root -> valid flips true and the counter
// would score an impossible shot.
TEST_CASE(
    "lead_solution: a target receding faster than the bullet is unsolved") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const sim::SimState target =
        at({100, 0, 0}, {200, 0, 0});  // +X, away, fast
    const render::LeadSolution sol = render::lead_solution(
        shooter, target, 100.0, 1000.0, glm::dvec3{0, 0, 0});
    REQUIRE_FALSE(sol.valid);  // f(t) never crosses 0 within t_max
    REQUIRE_FALSE(sol.in_range);
    // Fallback: pipper still points at the target so it stays visible.
    REQUIRE(sol.lead_dir.x == Catch::Approx(1.0).margin(1e-9));
}

// GRAVITY DROP: a STATIONARY target dead ahead (+X), gravity pulling down (-Y).
// The bullet falls, so the aim must rise: lead_dir gains a +Y (up) component.
// MUTATION: drop the -0.5*grav*t^2 term -> lead_dir = (1,0,0), y = 0 -> fails.
TEST_CASE("lead_solution: gravity drop raises the aim") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const sim::SimState target =
        at({500, 0, 0}, {0, 0, 0});          // stationary ahead
    const glm::dvec3 grav{0.0, -9.81, 0.0};  // down = -Y
    // Slow round (100 m/s) so the ~5 s flight makes the drop large + clear.
    const render::LeadSolution sol =
        render::lead_solution(shooter, target, 100.0, 1000.0, grav);
    REQUIRE(sol.valid);
    REQUIRE(sol.lead_dir.y > 0.1);  // aim HIGH to lead the drop
    REQUIRE(sol.lead_dir.x > 0.0);  // still mostly toward the target
    // With no gravity the same shot is dead ahead (y == 0) — the contrast.
    const render::LeadSolution flat = render::lead_solution(
        shooter, target, 100.0, 1000.0, glm::dvec3{0, 0, 0});
    REQUIRE(flat.lead_dir.y == Catch::Approx(0.0).margin(1e-9));
}

// VELOCITY INHERITANCE: the SHOOTER moves +Y at 50; a STATIONARY target dead
// ahead (+X). The bullet inherits +Y, so the aim must lean DOWN (-Y) to cancel
// that drift. MUTATION: w = target.velocity (drop the -shooter.velocity) ->
// lead_dir.y = 0 -> fails.
TEST_CASE("lead_solution: the bullet inherits the shooter velocity") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 50, 0});  // moving +Y
    const sim::SimState target =
        at({100, 0, 0}, {0, 0, 0});  // stationary ahead
    const render::LeadSolution sol = render::lead_solution(
        shooter, target, 100.0, 1000.0, glm::dvec3{0, 0, 0});
    REQUIRE(sol.valid);
    REQUIRE(sol.lead_dir.y < -0.1);  // lean DOWN to cancel the inherited +Y
}

TEST_CASE("lead_solution: bad muzzle speed and coincident target are guarded") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const glm::dvec3 g0{0, 0, 0};
    // muzzle <= 0
    REQUIRE_FALSE(render::lead_solution(shooter, at({100, 0, 0}, {0, 0, 0}),
                                        0.0, 1000.0, g0)
                      .valid);
    // target on the shooter (range ~ 0)
    const render::LeadSolution coincident = render::lead_solution(
        shooter, at({0, 0, 0}, {10, 0, 0}), 100.0, 1000.0, g0);
    REQUIRE_FALSE(coincident.valid);
    REQUIRE(std::isfinite(coincident.lead_dir.x));  // no NaN
}

// ===========================================================================
// on_target — nose inside the hit cone AND in range. cone = cos(2 deg).
// MUTATION: drop the `in_range` conjunct -> the out-of-range case (nose exactly
// on the lead, but range > max_range) flips from false to true.
// ===========================================================================
TEST_CASE("on_target: nose in the cone and in range") {
    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const sim::SimState target =
        at({100, 0, 0}, {0, 60, 0});  // lead (0.8,0.6,0)
    const double cone = std::cos(2.0 * 3.14159265358979323846 / 180.0);

    const render::LeadSolution in = render::lead_solution(
        shooter, target, 100.0, 1000.0, glm::dvec3{0, 0, 0});
    const glm::dvec3 on_lead{0.8, 0.6, 0.0};

    // Nose exactly on the lead, in range -> hit.
    REQUIRE(render::on_target(on_lead, in, cone));
    // Nose 5 deg off the lead -> outside the 2 deg cone -> miss.
    const double a = 5.0 * 3.14159265358979323846 / 180.0;
    // Rotate on_lead by 5 deg in the x-y plane about +Z.
    const glm::dvec3 off{on_lead.x * std::cos(a) - on_lead.y * std::sin(a),
                         on_lead.x * std::sin(a) + on_lead.y * std::cos(a),
                         0.0};
    REQUIRE_FALSE(render::on_target(off, in, cone));

    // In the cone but OUT of range -> miss (the in_range conjunct).
    const render::LeadSolution far = render::lead_solution(
        shooter, target, 100.0, 50.0, glm::dvec3{0, 0, 0});  // max_range < 100
    REQUIRE(far.valid);
    REQUIRE_FALSE(far.in_range);
    REQUIRE_FALSE(render::on_target(on_lead, far, cone));
}

// ===========================================================================
// meter_tick — the time-on-target counter. total counts engaged ticks; hit
// counts on-solution ticks; a null target advances neither. MUTATION: increment
// total but not gate hit on on_target (always hit) -> fraction hits 1.0 when
// the nose is off. MUTATION: advance total on a null target -> engaged-time
// inflates.
// ===========================================================================
TEST_CASE("meter_tick: counts engaged ticks and on-target ticks") {
    render::GunsightParams gp;
    gp.muzzle_speed = 100.0;
    gp.max_range = 1000.0;
    gp.hit_cone_cos = std::cos(2.0 * 3.14159265358979323846 / 180.0);

    const sim::SimState shooter = at({0, 0, 0}, {0, 0, 0});
    const sim::SimState target =
        at({100, 0, 0}, {0, 60, 0});  // lead (0.8,0.6,0)
    const glm::dvec3 on_lead{0.8, 0.6, 0.0};
    const glm::dvec3 off{0.0, 0.0, -1.0};

    const glm::dvec3 g0{0, 0, 0};
    render::OnTargetMeter m;
    // 5 ticks on target, 3 off — all engaged.
    for (int i = 0; i < 5; ++i)
        render::meter_tick(m, shooter, on_lead, &target, gp, g0);
    for (int i = 0; i < 3; ++i)
        render::meter_tick(m, shooter, off, &target, gp, g0);
    REQUIRE(m.total_ticks == 8);
    REQUIRE(m.hit_ticks == 5);
    REQUIRE(m.fraction() == Catch::Approx(5.0 / 8.0));
    REQUIRE(m.has_target);
    REQUIRE_FALSE(m.on_target_now);  // last tick was off
    REQUIRE(m.range_now == Catch::Approx(100.0).margin(1e-9));

    // A null target (no bandit ahead): neither counter advances, has_target
    // clears — idle time is not "engaged time".
    render::meter_tick(m, shooter, on_lead, nullptr, gp, g0);
    REQUIRE(m.total_ticks == 8);
    REQUIRE(m.hit_ticks == 5);
    REQUIRE_FALSE(m.has_target);
    REQUIRE_FALSE(m.on_target_now);
}
