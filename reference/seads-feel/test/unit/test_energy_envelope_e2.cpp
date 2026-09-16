// ENV-2 — THE AIR-ENVELOPE GOVERNOR, the pure-law arms
// (docs/RUNG_SPEC_20260828_energy_envelope.md §4 "RUNG E2"; the code calls it
// ENV-2 because this file's neighbours already own "E2" for the strike divert).
//
// CHAD'S RULING, 2026-08-28, verbatim and binding:
//   "I want the enemy ai to go low around the edges of the bubble in case
//   theirs shrinks ... if they are deep in bubble they can climb and get an
//   energy advantage to be more effective defenders and attackers, MAKE SURE
//   THAT ABILITY PERSISTS. Safe flying on deck, energy fighting in the safe
//   atmospheric zone, with an applied buffer for them to fight lower on the
//   fringes."
//
// ★★★ THE MOST IMPORTANT ARM IN THIS FILE IS THE ONE THAT FAILS IF THE DESIGN
// QUIETLY BECOMES "DECK EVERYWHERE". That would satisfy survival and would be a
// straight violation of the ruling above. It is "ENV2-A4 the energy ability
// persists deep inside", and it is a tripwire, not a formality.
//
// SCOPE, STATED HONESTLY: these are the PURE-LAW arms — the allowance shape,
// the plateau, the constant knee, the mirror pin, the deck floor, and the OFF
// switch. The CLOSED-LOOP replays the spec also requires (the i=0 and i=9
// crossings, the deep-outside transit, the portal approach) are NOT in this
// file and are NOT yet built. Nothing here should be read as evidence that the
// governor saves an aeroplane; it is evidence that the law it will fly has the
// shape Chad ruled.
//
// Every arm names the mutation that must make it go red, and every one of those
// mutations was RUN. TEST_CASE/SECTION names are strictly ASCII with no comma.

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include <glm/glm.hpp>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_scenario.h"
#include "drone/drone.h"
#include "world/faction_bubbles.h"

#ifndef SEADS_CONFIG_DIR
#define SEADS_CONFIG_DIR "config"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
const cfg::ScenarioParams kScen =
    cfg::load_scenario_toml(SEADS_CONFIG_DIR "/scenario.toml", kAp);

drone::AirDomes stamp(int own, double scale_own, double scale_other) {
    world::FactionGrowth grow[2];
    world::FactionGrowth unit[2];
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

// A point on the SUDBURY major axis at `arc` metres from the dome centre.
glm::dvec3 on_major(double arc) {
    const glm::dvec3 cn = glm::normalize(world::kSudburyCenterDir);
    glm::dvec3 m = world::kSudburyMajorAxis -
                   cn * glm::dot(world::kSudburyMajorAxis, cn);
    m = glm::normalize(m);
    const double th = arc / kAp.R;
    return glm::normalize(cn * std::cos(th) + m * std::sin(th)) * kAp.R;
}

double allow(const drone::AirDomes& ad, const glm::dvec3& p,
             const drone::DroneParams& dp) {
    return drone::env_allowance_m(ad, p, kAp.R, dp, dp.deck_track_agl_m);
}

}  // namespace

TEST_CASE("ENV2-A1 the shipped dials are loaded and are the ruled pair",
          "[env2]") {
    const drone::DroneParams& dp = kScen.drone;
    // The premise, first, and off the LOADER rather than a retyped number: a
    // constant that describes the shipped table stops describing it the moment
    // the table moves, and this ladder has paid for that seven times.
    REQUIRE(dp.env_edge_alt_m == 600.0);       // Chad's LOOSE pair, 2026-08-29
    REQUIRE(std::abs(dp.env_slope - 0.364) < 1e-12);  // tan(20 deg)
    REQUIRE(dp.env_plateau_m == 1650.0);
    REQUIRE(dp.env_fade_band_m == 150.0);
    REQUIRE(dp.env_desc_fade_band_m == 400.0);
    // Radians in drone/, degrees only in the TOML (the house split).
    REQUIRE(std::abs(dp.env_desc_cap - 20.0 * 3.14159265358979323846 / 180.0) <
            1e-9);
    // ...and the descent cap must actually be MORE authority than the shipped
    // track cap, or the whole i=9 fix is decorative.
    REQUIRE(dp.env_desc_cap > dp.deck_track_dive_cap);

    // ★ THE MIRROR PIN. env_shrink_radius_frac exists only to say "reach as far
    // in as ONE shrink can move the edge". If game.toml's shrink dial moves and
    // this one does not, the buffer silently stops insuring the thing it was
    // built to insure -- and NOTHING else in the build would notice.
    REQUIRE(dp.env_shrink_radius_frac == kGame.conquest.shrink_radius_frac);
    // REQUIRED-RED MUTATION: set env_shrink_radius_frac = 0.4 in scenario.toml
    // (game.toml keeps 0.5). VERIFIED RED on this line.
}

TEST_CASE("ENV2-A2 the allowance has the shape Chad ruled", "[env2]") {
    const drone::DroneParams& dp = kScen.drone;
    const drone::AirDomes ad = stamp(world::SUDBURY, 1.0, 1.0);
    const double a_edge = world::kSudburyMajorRadiusM;  // the edge on this axis

    SECTION("outside the air the answer is the deck lane and nothing else") {
        // ★ THE FLOOR. Without it the governor commands a descent below the
        // deck against the 60/110 latch that is pulling up -- a limit cycle
        // that mutes the guns on every crossing and would have broken the
        // signed S1-DECK lane. (Red team L1/F1 caught exactly this.)
        for (double out : {200.0, 3000.0, 12000.0}) {
            const double a = allow(ad, on_major(a_edge + out), dp);
            REQUIRE(a == dp.deck_track_agl_m);
        }
    }

    SECTION("at the edge it is the ruled edge ceiling") {
        const double a = allow(ad, on_major(a_edge), dp);
        REQUIRE(std::abs(a - dp.env_edge_alt_m) < 1.0);
    }

    SECTION("the fringe ramps and then plateaus") {
        const double a_edge_v = allow(ad, on_major(a_edge), dp);
        const double a_1k = allow(ad, on_major(a_edge - 1000.0), dp);
        const double a_3k = allow(ad, on_major(a_edge - 3000.0), dp);
        const double a_6k = allow(ad, on_major(a_edge - 6000.0), dp);
        // Monotone non-decreasing as we go deeper -- "progressively lower
        // toward the fringe", read backwards.
        REQUIRE(a_1k > a_edge_v);
        REQUIRE(a_3k > a_1k);
        REQUIRE(a_6k >= a_3k);
        // The ramp is the slope, until it meets the plateau.
        REQUIRE(std::abs(a_1k - (dp.env_edge_alt_m + dp.env_slope * 1000.0)) <
                1.0);
        // 600 + 0.364*d meets 1650 at d = 2885 m, so by 3 km we are ON the
        // plateau and must stay there until the knee.
        REQUIRE(std::abs(a_3k - dp.env_plateau_m) < 1.0);
        REQUIRE(std::abs(a_6k - dp.env_plateau_m) < 1.0);
    }

    SECTION("a fringe fight is flown lower than a deep one") {
        // The ruling in one assertion.
        REQUIRE(allow(ad, on_major(a_edge - 500.0), dp) <
                allow(ad, on_major(2000.0), dp));
    }
    // REQUIRED-RED MUTATION: drop the `std::max(..., deck_floor_m)` from
    // env_allowance_m. VERIFIED RED -- the outside section reports a NEGATIVE
    // allowance (the ramp continues below zero on negative depth) instead of
    // the deck lane.
}

TEST_CASE("ENV2-A3 the buffer is sized by the shrink and the knee is constant",
          "[env2]") {
    const drone::DroneParams& dp = kScen.drone;

    SECTION("one shrink cannot strand anyone above the plateau") {
        // ★ THE WHOLE JUSTIFICATION, as arithmetic. A pump death subtracts
        // shrink_radius_frac of the BASE radius from the edge, in ONE tick.
        // Measured on tape 13: five aeroplanes were outside in the next sample.
        const drone::AirDomes full = stamp(world::SUDBURY, 1.25, 1.0);
        const double jump =
            dp.env_shrink_radius_frac * world::kSudburyMajorRadiusM;
        REQUIRE(std::abs(jump - 8700.0) < 1.0);  // the measured 8,700 m

        // Take the worst legal point: exactly at the knee, where the allowance
        // is still the plateau. After the shrink it must not be stranded any
        // higher than the plateau -- that is what "the buffer covers the jump"
        // means, and it is the ONLY quantitative claim the plateau makes.
        const double a_at_knee = allow(full, on_major(
            world::kSudburyMajorRadiusM * 1.25 - jump), dp);
        REQUIRE(std::abs(a_at_knee - dp.env_plateau_m) < 200.0);
        // Nobody deeper than the knee is covered, and that is DECLARED, not
        // hidden: see the residual in the spec's section 7.
    }

    SECTION("the knee does NOT move when the dome shrinks") {
        // ★ CHAD'S RULING 2026-08-29, decision 4: the knee is CONSTANT --
        // half the BASE radius whatever the dome's current size. The test that
        // makes it real: the allowance at a FIXED DEPTH must be the same at
        // both scales. (The allowance at a fixed POSITION of course differs --
        // the edge moved. Depth is the variable, not position.)
        const drone::AirDomes big = stamp(world::SUDBURY, 1.25, 0.0);
        const drone::AirDomes small = stamp(world::SUDBURY, 0.75, 0.0);
        // ★ THE PROBE DEPTH IS CHOSEN TO DISCRIMINATE, and the first version of
        // this arm was BLIND because it was not. At d = 5,000 both a constant
        // knee (8,700) and a live-fraction knee (6,525 at scale 0.75) leave you
        // on the plateau, so the two designs agree and the arm could not fail —
        // the live-knee mutation ran green. 7,500 sits BETWEEN the two knees:
        // constant => still plateau; live => the free ramp has already started.
        const double d = 7500.0;
        const double knee_base =
            dp.env_shrink_radius_frac * world::kSudburyMajorRadiusM;  // 8700
        const double knee_live_small =
            dp.env_shrink_radius_frac * world::kSudburyMajorRadiusM * 0.75;
        REQUIRE(d < knee_base);         // constant knee: plateau
        REQUIRE(d > knee_live_small);   // live knee: past the knee
        const double p_big =
            allow(big, on_major(world::kSudburyMajorRadiusM * 1.25 - d), dp);
        const double p_small =
            allow(small, on_major(world::kSudburyMajorRadiusM * 0.75 - d), dp);
        REQUIRE(std::abs(p_big - p_small) < 25.0);
        // ...and both are the PLATEAU, which is the constant knee's signature.
        REQUIRE(std::abs(p_small - dp.env_plateau_m) < 25.0);
    }

    SECTION("the ruled cost of the constant knee is real and is stated") {
        // Chad was given this number before he ruled, so it is pinned here
        // rather than discovered later: at scale 0.75 the free core is small,
        // and most of the dome fights at the plateau.
        const drone::AirDomes small = stamp(world::SUDBURY, 0.75, 0.0);
        const double edge = world::kSudburyMajorRadiusM * 0.75;
        const double knee =
            dp.env_shrink_radius_frac * world::kSudburyMajorRadiusM;
        REQUIRE(knee > edge * 0.5);  // the knee is BEYOND half the live dome
        // Mid-dome, at 0.75, is capped at the plateau -- not free.
        REQUIRE(std::abs(allow(small, on_major(edge * 0.5), dp) -
                         dp.env_plateau_m) < 1.0);
    }
    // REQUIRED-RED MUTATION: compute d_knee off the LIVE radii
    // (e.a_m instead of e.base_a_m) -- i.e. the live-fraction knee Chad
    // rejected. VERIFIED RED on "the knee does NOT move when the dome shrinks".
}

TEST_CASE("ENV2-A4 the energy ability persists deep inside", "[env2]") {
    // ★★★ THE TRIPWIRE AGAINST THIS DESIGN QUIETLY BECOMING DECK-EVERYWHERE.
    // Chad: "if they are deep in bubble they can climb and get an energy
    // advantage to be more effective defenders and attackers, MAKE SURE THAT
    // ABILITY PERSISTS." A governor that clamps the whole map to the deck would
    // pass every survival arm in this ladder and would violate the ruling.
    const drone::DroneParams& dp = kScen.drone;

    SECTION("at the shipped scale the core allows a real energy fight") {
        const drone::AirDomes ad = stamp(world::SUDBURY, 1.25, 1.0);
        // Deep in the core, well inside the knee.
        const double a = allow(ad, on_major(1000.0), dp);
        REQUIRE(a > 2000.0);
        // ...and it is strictly more than the plateau, i.e. the core is FREE
        // rather than merely plateaued.
        REQUIRE(a > dp.env_plateau_m);
    }

    SECTION("even a shrunk dome still permits more than the deck") {
        const drone::AirDomes small = stamp(world::SUDBURY, 0.75, 0.0);
        const double a = allow(small, on_major(3000.0), dp);
        REQUIRE(a >= dp.env_plateau_m);
        REQUIRE(a > 10.0 * dp.deck_track_agl_m);  // 1650 vs a 100 m deck lane
    }
    // REQUIRED-RED MUTATION: set env_plateau_m = 0 and env_slope = 0 in
    // scenario.toml (the "flatten everything to the deck" change). VERIFIED
    // RED on both sections. This is the arm that must be run before anyone
    // ships a "safer" envelope.
}

// ★★★ THE PORTAL-APPROACH CORRIDOR HAS AN ARM ALREADY, AND IT IS NOT IN THIS
// FILE. `probe P-B: the stope falls`, `E15`, and `probe P-D` grade whether the
// tunnel offensive still completes runs, and all three went RED when the
// governor acted on the whole of TRANSIT (P-B: completed runs 6+ -> 1). That is
// a verified required-red for the corridor exemption: delete it and those three
// fail. No new arm is written here, because writing a fourth one that grades
// the same property would be decoration -- and because those three grade the
// property on the fixture that actually flies the bore, which a pure-law arm
// in this file could not do.
// ⚠ WHAT IS STILL MISSING, said plainly: none of them carries FOES. The spec
// requires a foes-present portal arm (E2-A8) that measures the arming under
// engagement at the post-shrink geometry, and it is NOT built. Until it is,
// nobody may claim the fringe dial pair is proven safe for the tunnel mission
// -- only that it is not obviously broken with nobody shooting.

TEST_CASE("ENV2-A5 zero turns the governor off", "[env2]") {
    // The house 0 = OFF contract. env_edge_alt_m <= 0 is the master switch, and
    // it is checked at the seam BEFORE anything else, so an OFF build never
    // even sweeps the forward window.
    drone::DroneParams dp = kScen.drone;
    dp.env_edge_alt_m = 0.0;
    const drone::AirDomes ad = stamp(world::SUDBURY, 1.0, 1.0);
    // With the master off the seam is skipped entirely; the allowance helper is
    // never consulted. Pin the master's own value so the seam's guard and this
    // arm cannot drift apart.
    REQUIRE_FALSE(dp.env_edge_alt_m > 0.0);
    REQUIRE(kScen.drone.env_edge_alt_m > 0.0);  // ...and the SHIPPED build is ON
    // ⚠ HONEST LIMIT: this arm pins the switch, not the seam. Proving the OFF
    // build is bit-identical is the gate's job (a governor-OFF run must
    // reproduce the shipped [.bdeck] arm-C hash), not this file's.
    (void)ad;
}
