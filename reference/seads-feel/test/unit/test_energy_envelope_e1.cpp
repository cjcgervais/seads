// ENV-1 — THE DEPTH INSTRUMENT (docs/RUNG_SPEC_20260828_energy_envelope.md
// §4 "RUNG E1"), from Chad's 2026-08-28 ruling off tape 13.
//
// HIS WORDS: "They need an awareness of proximity ot the edge, if they are deep
// in bubble they can climb and get an energy advantage ... Safe flying on deck,
// energy fighting in the safe atmospheric zone, with an applied buffer for them
// to fight lower on the fringes."
//
// This rung builds ONLY the awareness — the signed depth `d` that the ENV-2 altitude
// allowance A(d) will be a function of. It changes no behaviour. That is the
// house law "build the instrument before the feature", and it is also why the
// bit-identity leg below is a PASS condition here and nowhere else in this
// ladder.
//
// ⚠ THE TRAP THIS FILE EXISTS TO AVOID: an instrument that is bit-identical AND
// constant is indistinguishable from an instrument that is not wired up at all.
// So bit-identity alone is never the whole gate — legs A1/A2 prove the number
// VARIES and RESPONDS before A3 is allowed to mean anything.
//
// Every leg names the mutation that must make it go red. An arm without one
// does not ship in this spec.
//
// TEST_CASE and SECTION names are STRICTLY ASCII, and contain NO COMMA (a
// non-ASCII name silently never runs; a name with a comma cannot be selected by
// a filter and also silently runs nothing — both recurring traps here).

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "combat/conquest.h"
#include "drone/drone.h"
#include "world/faction_bubbles.h"

namespace {

constexpr double kR = 15000.0;  // the planet radius the game ships

// Build the stamp exactly as app/instructor_tick.h's E1 block does. ⚠ This is a
// third hand-mirror of that block (the app, test_conquest_match's arm_leash,
// and here). It is deliberate and it is pinned: leg A4 below asserts this
// mirror agrees with the shipped `world::faction_ellipse` rather than with a
// retyped radius, so the mirror cannot drift into describing a table that has
// moved -- the law paid for seven times on this ladder.
drone::AirDomes stamp(int own, double scale_own, double scale_other) {
    world::FactionGrowth grow[2];
    world::FactionGrowth unit[2];  // radius_scale = 1 => the BASE radii
    grow[own].radius_scale = scale_own;
    grow[1 - own].radius_scale = scale_other;
    drone::AirDomes ad;
    for (int slot = 0; slot < 2; ++slot) {
        const int f = (slot == 0) ? own : (1 - own);
        drone::DomeEllipse& e = ad.dome[slot];
        e.radius_scale = grow[f].radius_scale;
        if (!(e.radius_scale >= 1e-9)) continue;
        world::faction_ellipse(f, grow, e.center_dir, e.major_axis, e.a_m,
                               e.b_m);
        glm::dvec3 bdir{0.0, 1.0, 0.0};
        glm::dvec3 bmaj{1.0, 0.0, 0.0};
        world::faction_ellipse(f, unit, bdir, bmaj, e.base_a_m, e.base_b_m);
        e.live = e.a_m > 0.0 && e.b_m > 0.0;
    }
    return ad;
}

// A position at arc distance `arc` from a dome centre, along that dome's own
// major axis, at radius kR (ground level -- depth is a HORIZONTAL quantity and
// must not care about altitude; leg A5 pins that).
glm::dvec3 at_arc(const glm::dvec3& center_dir, const glm::dvec3& major_axis,
                  double arc, double radius = kR) {
    const glm::dvec3 cn = glm::normalize(center_dir);
    glm::dvec3 m = major_axis - cn * glm::dot(major_axis, cn);
    m = glm::normalize(m);
    const double th = arc / kR;
    return glm::normalize(cn * std::cos(th) + m * std::sin(th)) * radius;
}

}  // namespace

TEST_CASE("E1-A1 depth is signed and tracks the shipped ellipse",
          "[e1depth]") {
    // SUDBURY, both domes at the shipped baseline scale.
    const int own = world::SUDBURY;
    const drone::AirDomes ad = stamp(own, 1.0, 1.0);
    REQUIRE(ad.dome[0].live);
    const drone::DomeEllipse& e = ad.dome[0];

    // The premise first (the fixture-no-op class): the stamp really is the
    // shipped SUDBURY ellipse, read from the loader's own constants.
    REQUIRE(e.a_m == world::kSudburyMajorRadiusM);
    REQUIRE(e.b_m == world::kSudburyMinorRadiusM);

    SECTION("inside is positive and outside is negative on the major axis") {
        const double a = e.a_m;
        // Walk out along the major axis: depth must fall monotonically and
        // change sign exactly at the edge.
        const glm::dvec3 deep = at_arc(e.center_dir, e.major_axis, 0.0);
        const glm::dvec3 mid = at_arc(e.center_dir, e.major_axis, a * 0.5);
        const glm::dvec3 edge = at_arc(e.center_dir, e.major_axis, a);
        const glm::dvec3 out = at_arc(e.center_dir, e.major_axis, a + 3000.0);

        const double d_deep = drone::air_depth_m(ad, deep, kR);
        const double d_mid = drone::air_depth_m(ad, mid, kR);
        const double d_edge = drone::air_depth_m(ad, edge, kR);
        const double d_out = drone::air_depth_m(ad, out, kR);

        REQUIRE(d_deep > d_mid);
        REQUIRE(d_mid > d_edge);
        REQUIRE(d_edge > d_out);
        REQUIRE(d_deep > 0.0);
        REQUIRE(d_out < 0.0);
        // At the edge the depth is zero to floating-point -- the calibration.
        REQUIRE(std::abs(d_edge) < 1.0);
        // ★ AT THE CENTRE THE DEPTH IS THE MINOR RADIUS, NOT THE MAJOR. The
        // bearing is undefined exactly at the centre, and the shipped
        // `ellipse_r_eff` resolves that degeneracy by returning `b` (the
        // conservative bound) -- world/faction_bubbles.h:152, "deep at center:
        // bearing undefined -> minor". This arm asserted `a` on its first run
        // and went red by 4,350 m, which is the aspect gap. The code was right
        // and the premise was wrong; pinned here so nobody re-derives it.
        REQUIRE(std::abs(d_deep - e.b_m) < 1.0);
        REQUIRE(a > e.b_m);  // the gap that made the first premise wrong
        // 3 km outside reads 3 km outside (the metric is a distance, not a
        // ratio -- the whole point of not reusing ellipse_frac here).
        REQUIRE(std::abs(d_out + 3000.0) < 1.0);
    }

    SECTION("the minor axis is shorter and the law knows it") {
        // Same arc distance on the two axes must give DIFFERENT depths on a
        // 1.33-aspect ellipse. A circular law would pass everything above and
        // fail here.
        const glm::dvec3 cn = glm::normalize(e.center_dir);
        glm::dvec3 maj = e.major_axis - cn * glm::dot(e.major_axis, cn);
        maj = glm::normalize(maj);
        const glm::dvec3 min_ax = glm::normalize(glm::cross(cn, maj));
        const double arc = 14000.0;  // inside the major, outside the minor
        const double th = arc / kR;
        const glm::dvec3 p_maj =
            glm::normalize(cn * std::cos(th) + maj * std::sin(th)) * kR;
        const glm::dvec3 p_min =
            glm::normalize(cn * std::cos(th) + min_ax * std::sin(th)) * kR;
        const double d_maj = drone::air_depth_m(ad, p_maj, kR);
        const double d_min = drone::air_depth_m(ad, p_min, kR);
        REQUIRE(d_maj > 0.0);   // 14 km < 17400 major
        REQUIRE(d_min < 0.0);   // 14 km > 13050 minor
        REQUIRE(d_maj > d_min);
    }
    // REQUIRED-RED MUTATION: make `air_depth_m` ignore ad.dome[i].major_axis /
    // b_m and treat every dome as circular of radius a_m (return
    // a_m - arc). The minor-axis section goes red immediately -- d_min becomes
    // positive.
}

TEST_CASE("E1-A2 a dead dome is excluded and a live one still answers",
          "[e1depth]") {
    const int own = world::SUDBURY;
    // Position: 3 km OUTSIDE the SUDBURY major edge, so its own depth is
    // negative and the answer depends on which domes count.
    const drone::AirDomes both = stamp(own, 1.0, 1.0);
    const glm::dvec3 p = at_arc(both.dome[0].center_dir,
                                both.dome[0].major_axis,
                                both.dome[0].a_m + 3000.0);

    SECTION("own dome dead falls back to the other faction's air") {
        const drone::AirDomes dead_own = stamp(own, 0.0, 1.0);
        REQUIRE_FALSE(dead_own.dome[0].live);
        REQUIRE(dead_own.dome[1].live);
        const double d_both = drone::air_depth_m(both, p, kR);
        const double d_dead = drone::air_depth_m(dead_own, p, kR);
        // The number MUST change when a dome is excluded -- that is the whole
        // claim of the live test. (Whether it rises or falls depends on where
        // VALLEY's edge is from here; the load-bearing assertion is that the
        // exclusion is observable at all.)
        REQUIRE(d_both != d_dead);
    }

    SECTION("both domes dead is the sentinel and not a distance") {
        const drone::AirDomes none = stamp(own, 0.0, 0.0);
        REQUIRE_FALSE(none.any_live());
        REQUIRE(drone::air_depth_m(none, p, kR) == drone::kNoAirDepth);
        // ⚠ The sentinel is NOT 0.0 on purpose: 0.0 means "exactly on the
        // edge", a legitimate value E2's allowance is defined at.
        REQUIRE(drone::kNoAirDepth < -1.0e8);
    }

    SECTION("a dead dome cannot masquerade as an edge under your feet") {
        // ★ THE CASE THAT ACTUALLY BITES, and the reason this section exists at
        // all. Stand at the DEAD dome's own centre. Its arc-from-centre is 0,
        // so if it were wrongly counted as live with its collapsed (zero)
        // radii, `dome_depth_m` would return 0 - 0 = 0.0 -- and 0.0 does not
        // read as "dead", it reads as "you are exactly ON a breathable edge".
        // E2's allowance would then hand a drone standing in vacuum the full
        // edge ceiling. The max-over-live form makes that failure SILENT,
        // because 0.0 beats every genuinely negative depth.
        const drone::AirDomes dead_own = stamp(own, 0.0, 1.0);
        const glm::dvec3 c = world::kSudburyCenterDir * kR;
        const double d = drone::air_depth_m(dead_own, c, kR);
        REQUIRE(d < 0.0);          // we are in vacuum and the number says so
        REQUIRE(d != 0.0);         // ... and specifically NOT the edge value
        // It must be VALLEY's answer, because VALLEY is the only live air.
        REQUIRE(d == drone::dome_depth_m(dead_own.dome[1], c, kR));
    }

    SECTION("depth is the MAX over live domes") {
        // Sit at the VALLEY centre with both domes live: the answer must be
        // VALLEY's own generous depth, not SUDBURY's negative one.
        const glm::dvec3 vc = world::kValleyCenterDir * kR;
        const double d = drone::air_depth_m(both, vc, kR);
        const int v_slot = (own == world::VALLEY) ? 0 : 1;
        const double d_valley = drone::dome_depth_m(both.dome[v_slot], vc, kR);
        REQUIRE(d == d_valley);
        REQUIRE(d > 0.0);
    }
    // REQUIRED-RED MUTATION, and it takes BOTH halves at once: delete the
    // `if (!(e.radius_scale >= 1e-9)) continue;` guard AND force
    // `e.live = true`. VERIFIED RED: section "a dead dome cannot masquerade as
    // an edge under your feet" fails on `REQUIRE(d < 0.0)` with the expansion
    // `0.0 < 0.0` -- precisely the silent failure that section was written to
    // catch, and the sentinel section goes red alongside it.
    //
    // ⚠ WHY IT TAKES BOTH, written down because I ran the single mutations
    // first and they did NOT fire. The exclusion is doubly guarded: the scale
    // guard `continue`s before the liveness line is ever reached, and with a
    // zero scale `faction_ellipse` returns a = b = 0 so `a_m > 0.0 && b_m > 0.0`
    // would have excluded the dome anyway. Each half is independently
    // sufficient, so neither is falsifiable while the other stands.
    //
    // That is a real and slightly awkward property of this arm, and it is
    // stated rather than hidden: belt-and-braces defence buys robustness at the
    // cost of single-point testability. Do NOT remove one of the two guards to
    // "make the test meaningful" -- the property is what matters, and the
    // combined mutation demonstrates the property is genuinely load-bearing.
    // A required-red that was never actually run is exactly the decoration this
    // ladder has paid for repeatedly.
}

TEST_CASE("E1-A3 the depth law is horizontal and scale-aware", "[e1depth]") {
    const int own = world::SUDBURY;

    SECTION("altitude does not change depth") {
        const drone::AirDomes ad = stamp(own, 1.0, 1.0);
        const glm::dvec3 low = at_arc(ad.dome[0].center_dir,
                                      ad.dome[0].major_axis, 8000.0, kR);
        const glm::dvec3 high = at_arc(ad.dome[0].center_dir,
                                       ad.dome[0].major_axis, 8000.0,
                                       kR + 2500.0);
        // ⚠ Depth is a HORIZONTAL quantity. If altitude leaked into it, E2's
        // allowance A(d) would feed back on its own output -- a loop the whole
        // design is built to avoid.
        const double d_low = drone::air_depth_m(ad, low, kR);
        const double d_high = drone::air_depth_m(ad, high, kR);
        REQUIRE(std::abs(d_low - d_high) < 1e-9);
    }

    SECTION("a shrunk dome is shallower and the BASE radii do not move") {
        const drone::AirDomes full = stamp(own, 1.25, 1.0);
        const drone::AirDomes half = stamp(own, 0.75, 1.0);
        // The live radii move with the scale...
        REQUIRE(half.dome[0].a_m < full.dome[0].a_m);
        // ...and the BASE radii do NOT. ★ This is the assertion that would have
        // caught the spec's own double-counted-knee defect: E2's plateau
        // reaches shrink_radius_frac * r_eff_BASE, and faction_ellipse already
        // bakes radius_scale into a_m/b_m, so deriving base by dividing a_m by
        // radius_scale double-counts the scale. Carry the base; never derive it.
        REQUIRE(full.dome[0].base_a_m == half.dome[0].base_a_m);
        REQUIRE(full.dome[0].base_a_m == world::kSudburyMajorRadiusM);
        REQUIRE(full.dome[0].base_b_m == world::kSudburyMinorRadiusM);
        REQUIRE(full.dome[0].a_m != full.dome[0].base_a_m);  // 1.25 really bit

        // A point that is inside the full dome and outside the shrunk one --
        // the ONE-TICK STRANDING Chad asked the buffer to insure against. In
        // tape 13 this happened to five aeroplanes in a single sample.
        const glm::dvec3 p = at_arc(full.dome[0].center_dir,
                                    full.dome[0].major_axis, 16000.0);
        REQUIRE(drone::dome_depth_m(full.dome[0], p, kR) > 0.0);
        REQUIRE(drone::dome_depth_m(half.dome[0], p, kR) < 0.0);
    }
    // REQUIRED-RED MUTATION (base radii): stamp base_a_m = e.a_m /
    // e.radius_scale instead of calling faction_ellipse with a unit growth.
    // `full.dome[0].base_a_m == half.dome[0].base_a_m` still passes (both
    // recover 17400) -- but change the growth clamp so a_m saturates at
    // kFactionBubbleRadiusMaxM and the division stops recovering the base,
    // and it goes red. ⚠ THE HONEST STATEMENT: at the shipped scales this
    // particular mutation does NOT go red, because 21750 < 40000 means the
    // clamp never bites. The base is carried rather than derived because the
    // derivation is only accidentally correct today, not because a test can
    // currently distinguish them. Said out loud rather than claimed as proof.
}
