// R4 — solid ground ATs (MASTER_PLAN §3.A; scarce_skies_program.md rung R4;
// Chad's 2026-07-15 rulings: contact in the KERNEL via env.ground, landing v1
// is touch-and-stick). The mechanism under test is sim/ground.h, reached only
// through sim::step with a live Environment — exactly the shipped path.
//
// Discipline (CLAUDE.md ## Learned):
//  - GENERIC sphere points everywhere: no world axis coincides with local_up.
//  - Fixture-no-op guards: every leg asserts its baseline term is NON-TRIVIAL
//    (the crash leg proves the sink really exceeded the limit; the landing leg
//    proves the roll really happened) before checking the verdict.
//  - The null-env identity leg pins `nullptr` == all-null Environment
//    bit-exactly — the R3 proof, now with a live ground field in the build.
//  - Test GroundParams are the TEST'S OWN (the mechanism is graded against the
//    inputs it reads, never calibrated to today's game.toml).

#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "app/instructor_tick.h"
#include "config/load_aircraft.h"
#include "config/load_controller.h"
#include "sim/environment.h"
#include "sim/ground.h"
#include "sim/step.h"
#include "sim/world.h"
#include "test/harness/injector.h"
#include "test/harness/instructor.h"
#include "world/heightfield.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
const control::ControllerParams kCp =
    cfg::load_controller_toml(SEADS_CONFIG_DIR "/controller.toml", kAp);

// A GENERIC surface point + a non-radial heading (no axis aligned with up).
const glm::dvec3 kUp = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
const glm::dvec3 kHeading =
    glm::normalize(glm::cross(kUp, glm::dvec3{0.2, 0.9, 0.4}));

// Uniform terrain at `elev_m` above sea level: radius_at = R + q(elev), the
// normal is radial everywhere. Heights quantize to relief/65535 (~5 mm) — all
// placement below reads hf.radius_at, never R + elev_m.
world::HeightField uniform_field(double elev_m, double relief = 350.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

// A constant-grade ramp BAND centered on the test point kUp: height climbs
// 0 -> relief (350 m, real-world scale so the local surface radius stays ~R)
// across a v-band sized for tan(slope) = grade_tan at that radius. Narrow
// band + real heights, because a whole-planet ramp needs ~27 km of relief and
// the inflated local radius then deflates the actual grade (the first fixture
// bug this replaced). Rows are dense enough (h = 8192) that the 60 m normal
// probe spans ~10 linear bilinear cells.
world::HeightField ramp_field(double grade_tan) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 8192;
    hf.R = kAp.R;
    hf.relief_scale = 350.0;
    hf.u_offset = 0.0;
    const double v0 = world::equirect_uv(kUp, 0.0).y;  // the test point's v
    const double band = hf.relief_scale / (grade_tan * kPi * kAp.R);
    hf.px.resize(static_cast<std::size_t>(hf.w) * hf.h);
    for (int y = 0; y < hf.h; ++y) {
        const double v = (y + 0.5) / hf.h;
        const double f =
            std::min(std::max((v - (v0 - band / 2.0)) / band, 0.0), 1.0);
        for (int x = 0; x < hf.w; ++x)
            hf.px[static_cast<std::size_t>(y) * hf.w + x] =
                static_cast<std::uint16_t>(f * 65535.0 + 0.5);
    }
    return hf;
}

// The test's own contact dials (see header note). kContactH is NONZERO so the
// CG-above-wheels offset is pinned (a mechanism that ignores it puts the roll
// at the wrong radius and the surface-constraint assert below fails).
constexpr double kContactH = 1.5;

sim::GroundParams test_gp() {
    sim::GroundParams gp;
    gp.slope_limit_cos = std::cos(15.0 * kPi / 180.0);
    gp.friction = 0.08;
    gp.max_sink_ms = 5.0;
    gp.normal_probe_m = 60.0;
    gp.contact_height_m = kContactH;
    return gp;
}

sim::Environment ground_env(const world::HeightField& hf,
                            const sim::GroundParams& gp) {
    sim::Environment e;
    e.ground = &hf;
    e.ground_params = gp;
    return e;
}

// Place a level state `above_m` above the LOCAL CONTACT surface (terrain +
// contact height) with a set radial sink added to the tangential speed V.
sim::SimState above_terrain(const world::HeightField& hf, double V,
                            double above_m, double sink_ms) {
    const double r_g = hf.radius_at(kUp);
    sim::SimState s = harness::level_state(
        kAp, V, r_g - kAp.R + kContactH + above_m, kUp, kHeading);
    s.velocity -= sink_ms * kUp;
    return s;
}

// The meridian tangent at a point (toward the +y pole) — the ramp's grade
// direction. The high side is SAMPLED from the field itself, so the fixture
// can never share a sign bug with the mechanism under test.
glm::dvec3 meridian(const glm::dvec3& at) {
    const glm::dvec3 u = glm::normalize(at);
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, u));
    return glm::normalize(glm::cross(u, east));
}

glm::dvec3 toward_high_side(const world::HeightField& hf,
                            const glm::dvec3& at) {
    const glm::dvec3 m = meridian(at);
    const double s = 300.0 / kAp.R;
    const double r_p = hf.radius_at(glm::normalize(glm::dvec3(at) + s * m));
    const double r_m = hf.radius_at(glm::normalize(glm::dvec3(at) - s * m));
    return r_p > r_m ? m : -m;
}

// A grounded roll at speed `V` from `start_dir` along `heading` (no throttle).
sim::SimState grounded_roll_state(const world::HeightField& hf,
                                  const glm::dvec3& start_dir, double V,
                                  const glm::dvec3& heading) {
    const glm::dvec3 d = glm::normalize(start_dir);
    const double r_s = hf.radius_at(d) + kContactH;
    sim::SimState s = harness::level_state(kAp, V, r_s - kAp.R, d, heading);
    s.position = d * r_s;
    s.on_ground = true;
    return s;
}

}  // namespace

TEST_CASE(
    "ground: normal_at is radial on uniform terrain, tilted by the "
    "authored grade on a ramp") {
    const world::HeightField flat = uniform_field(200.0);
    REQUIRE(glm::dot(flat.normal_at(kUp, 60.0), kUp) > 1.0 - 1e-9);

    const double grade = std::tan(30.0 * kPi / 180.0);
    const world::HeightField ramp = ramp_field(grade);
    const double c = glm::dot(ramp.normal_at(kUp, 60.0), kUp);
    const double slope_deg = std::acos(std::min(c, 1.0)) * 180.0 / kPi;
    REQUIRE(slope_deg > 27.0);  // the 30-deg grade reads back (finite-diff +
    REQUIRE(slope_deg < 33.0);  // equirect tolerance)
}

TEST_CASE(
    "ground: a hard sink into terrain sets crashed: the KERNEL "
    "verdict, above sea level") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);

    sim::SimState s = above_terrain(hf, 120.0, 8.0, 25.0);
    sim::Inputs in{};  // hands off, no thrust
    bool contact = false;
    for (int t = 0; t < 240 && !contact; ++t) {
        const sim::SimState n = sim::step(s, in, kAp, &env, kAp.sim_dt);
        if (n.crashed || n.on_ground) {
            // Fixture-no-op guard: the sink at the verdict tick genuinely
            // exceeds the gentle limit — the crash leg is non-trivial.
            const double sink =
                -glm::dot(n.velocity, sim::local_up(n.position));
            if (!n.crashed) {  // landed instead — the fixture failed
                REQUIRE(n.crashed);
            }
            REQUIRE(sink > gp.max_sink_ms);
            REQUIRE(!n.on_ground);
            // The verdict is the kernel's terrain rule, NOT the old bare-
            // sphere rule: the wreck sits well above sea level.
            REQUIRE(sim::altitude(n.position, kAp) > 0.0);
            contact = true;
        }
        s = n;
    }
    REQUIRE(contact);
}

TEST_CASE(
    "ground: gentle upright contact lands GROUNDED, rolls out, "
    "sticks at a full stop") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);

    sim::SimState s = above_terrain(hf, 45.0, 0.4, 1.0);
    sim::Inputs in{};  // throttle 0: land and roll out
    int touch_tick = -1;
    for (int t = 0; t < 600 && touch_tick < 0; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        if (s.on_ground) touch_tick = t;
    }
    REQUIRE(touch_tick >= 0);
    // Fixture-no-op guard: the touchdown carries real ground speed (a roll
    // actually happens; a from-rest fixture would no-op the friction leg).
    const double touch_speed = glm::length(s.velocity);
    REQUIRE(touch_speed > 20.0);
    // Constrained to the surface while rolling.
    REQUIRE(std::abs(glm::length(s.position) -
                     hf.radius_at(glm::normalize(s.position)) - kContactH) <
            1e-6);  // pins the CG-above-wheels offset

    // Roll-out: friction (+ drag) decays the ground speed to EXACTLY zero
    // (touch-and-stick: the stiction clamp is the stop) and it STAYS stopped.
    double prev_speed = touch_speed;
    bool stopped = false;
    for (int t = 0; t < 12000 && !stopped; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        REQUIRE(s.on_ground);
        const double sp = glm::length(s.velocity);
        REQUIRE(sp <= prev_speed + 1e-9);  // monotone decay on the roll-out
        prev_speed = sp;
        stopped = sp == 0.0;
    }
    REQUIRE(stopped);
    for (int t = 0; t < 120; ++t) s = sim::step(s, in, kAp, &env, kAp.sim_dt);
    REQUIRE(glm::length(s.velocity) == 0.0);
    REQUIRE(s.on_ground);
}

TEST_CASE(
    "ground: landing limits reject: hard sink, banked touchdown, "
    "over-limit terrain slope each crash") {
    const sim::GroundParams gp = test_gp();

    SECTION("banked touchdown (gentle sink, flat terrain)") {
        const world::HeightField hf = uniform_field(200.0);
        const sim::Environment env = ground_env(hf, gp);
        // 45-deg bank >> the 15-deg attitude acceptance; sink stays gentle.
        const double r_g = hf.radius_at(kUp);
        sim::SimState s =
            harness::flight_state(kAp, 45.0, r_g - kAp.R + kContactH + 0.4, kUp,
                                  kHeading, 45.0 * kPi / 180.0, 0.0, 0.0);
        s.velocity -= 1.0 * kUp;
        sim::Inputs in{};
        bool verdict = false;
        for (int t = 0; t < 600 && !verdict; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.crashed || s.on_ground) {
                // Guard: the sink really was gentle — ONLY the attitude check
                // can have produced this crash.
                REQUIRE(-glm::dot(s.velocity, sim::local_up(s.position)) <
                        gp.max_sink_ms);
                REQUIRE(s.crashed);
                REQUIRE(!s.on_ground);
                verdict = true;
            }
        }
        REQUIRE(verdict);
    }

    SECTION("terrain slope over the limit (gentle, upright)") {
        const world::HeightField hf = ramp_field(std::tan(30.0 * kPi / 180.0));
        const sim::Environment env = ground_env(hf, gp);
        // Guard: the fixture's slope genuinely exceeds the acceptance.
        REQUIRE(glm::dot(hf.normal_at(kUp, gp.normal_probe_m), kUp) <
                gp.slope_limit_cos);
        const double r_g = hf.radius_at(kUp);
        sim::SimState s = harness::level_state(
            kAp, 45.0, r_g - kAp.R + kContactH + 0.4, kUp, kHeading);
        s.velocity -= 1.0 * kUp;
        sim::Inputs in{};
        bool verdict = false;
        for (int t = 0; t < 600 && !verdict; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.crashed || s.on_ground) {
                REQUIRE(s.crashed);
                REQUIRE(!s.on_ground);
                verdict = true;
            }
        }
        REQUIRE(verdict);
    }
}

TEST_CASE(
    "ground: takeoff: a full-throttle grounded roll with pitch-up "
    "releases and climbs away") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);

    // Rolling at 70 m/s, level attitude (alpha = 0 -> no lift yet), GROUNDED.
    const double r_g = hf.radius_at(kUp);
    sim::SimState s =
        harness::level_state(kAp, 70.0, r_g - kAp.R + kContactH, kUp, kHeading);
    s.position = kUp * (r_g + kContactH);
    s.on_ground = true;

    sim::Inputs in{};
    in.throttle = 1.0f;
    in.pitch = 0.25f;  // rotate

    // Fixture-no-op guard: it really rolls grounded first (a fixture that
    // releases on tick 1 would make the regime a no-op).
    for (int t = 0; t < 30; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        REQUIRE(s.on_ground);
    }

    // Then the rotation builds lift until the airborne integration wins.
    bool climbed = false;
    double max_gain = 0.0;
    for (int t = 0; t < 2400 && !climbed; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        if (!s.on_ground) {
            max_gain = std::max(max_gain,
                                glm::length(s.position) - kContactH -
                                    hf.radius_at(glm::normalize(s.position)));
            climbed = max_gain > 20.0;  // genuinely flying, not a bounce
        }
    }
    REQUIRE(climbed);
}

TEST_CASE(
    "ground: nullptr env and all-null Environment fly the identical v3 "
    "world, bit for bit, flags never set") {
    // The same dive that crashes on terrain above — flown with (a) env =
    // nullptr and (b) a non-null Environment whose fields are ALL null. The
    // two must be BIT-IDENTICAL (the R3 proof, now with live ground code in
    // the build), and neither may ever set a ground flag.
    const world::HeightField hf = uniform_field(200.0);
    sim::SimState a = above_terrain(hf, 120.0, 8.0, 25.0);
    sim::SimState b = a;
    const sim::Environment all_null{};
    sim::Inputs in{};
    for (int t = 0; t < 240; ++t) {
        a = sim::step(a, in, kAp, nullptr, kAp.sim_dt);
        b = sim::step(b, in, kAp, &all_null, kAp.sim_dt);
        REQUIRE(a.position == b.position);
        REQUIRE(a.velocity == b.velocity);
        REQUIRE(a.orientation == b.orientation);
        REQUIRE(a.angular_vel == b.angular_vel);
        REQUIRE(!a.crashed);
        REQUIRE(!a.on_ground);
        REQUIRE(!b.crashed);
        REQUIRE(!b.on_ground);
    }
}

TEST_CASE(
    "ground: app::tick crash predicate: a sea-level touchdown is a "
    "LANDING (never the old altitude<=0 death), a terrain crash "
    "respawns") {
    const sim::GroundParams gp = test_gp();

    SECTION("lake-ice touchdown at sea level: no respawn") {
        // Terrain AT sea level (radius_at == R) and contact height 0, so the
        // CG settles at altitude EXACTLY 0 — the old altitude<=0 rule would
        // kill this touchdown; the kernel verdict lands it. (A nonzero
        // contact height would park the CG above 0 and weaken this pin.)
        const world::HeightField hf = uniform_field(0.0);
        sim::GroundParams gp0 = gp;
        gp0.contact_height_m = 0.0;
        const sim::Environment env = ground_env(hf, gp0);
        app::LoopState st;
        const double r_g = hf.radius_at(kUp);
        st.curr =
            harness::level_state(kAp, 45.0, r_g - kAp.R + 0.4, kUp, kHeading);
        st.curr.velocity -= 1.0 * kUp;
        st.prev = st.curr;
        st.prev_up = sim::local_up(st.curr.position);
        st.aim.reseed(st.curr.orientation, st.prev_up);
        st.grounded = true;  // spawn tick pairing, as the app seeds it
        app::TickInput in;
        in.raw_mode = true;  // dead stick: land hands-off
        bool landed = false;
        for (int t = 0; t < 600 && !landed; ++t) {
            const app::TickResult r = app::tick(st, in, kAp, kCp, &env);
            REQUIRE(!r.respawned);
            landed = st.curr.on_ground;
        }
        REQUIRE(landed);
        REQUIRE(sim::altitude(st.curr.position, kAp) <= 1e-6);
    }

    SECTION("terrain crash above sea level: respawns") {
        const world::HeightField hf = uniform_field(200.0);
        const sim::Environment env = ground_env(hf, gp);
        app::LoopState st;
        st.curr = above_terrain(hf, 120.0, 8.0, 25.0);
        st.prev = st.curr;
        st.prev_up = sim::local_up(st.curr.position);
        st.aim.reseed(st.curr.orientation, st.prev_up);
        st.grounded = true;
        app::TickInput in;
        in.raw_mode = true;
        bool respawned = false;
        for (int t = 0; t < 240 && !respawned; ++t)
            respawned = app::tick(st, in, kAp, kCp, &env).respawned;
        REQUIRE(respawned);
        // The rebirth is the clean spawn (fresh flags included).
        REQUIRE(!st.curr.crashed);
        REQUIRE(!st.curr.on_ground);
        REQUIRE(sim::altitude(st.curr.position, kAp) > 1000.0);
    }
}

TEST_CASE(
    "ground: re-attach grace lands a micro-sag WITHOUT the attitude check; "
    "a real descent still takes the full acceptance") {
    // Red-team P0-1: a marginal liftoff releases precisely at high alpha, so
    // the re-contact must not re-grade attitude. Grace = sink <= 3*g*dt.
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);
    const double r_g = hf.radius_at(kUp);
    const double bank = 30.0 * kPi / 180.0;  // fails the 15-deg upright check

    SECTION("micro-sag (sink within the grace) re-attaches despite the bank") {
        sim::SimState s =
            harness::flight_state(kAp, 45.0, r_g - kAp.R + kContactH + 0.0005,
                                  kUp, kHeading, bank, 0.0, 0.0);
        s.velocity -= 0.02 * kUp;  // a whisper of sink: contact in 1-2 ticks
        sim::Inputs in{};
        bool verdict = false;
        for (int t = 0; t < 10 && !verdict; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.on_ground || s.crashed) {
                REQUIRE(s.on_ground);  // grace attached it
                REQUIRE(!s.crashed);
                verdict = true;
            }
        }
        REQUIRE(verdict);
    }

    SECTION("the same bank with a real (still gentle) sink crashes") {
        sim::SimState s =
            harness::flight_state(kAp, 45.0, r_g - kAp.R + kContactH + 0.01,
                                  kUp, kHeading, bank, 0.0, 0.0);
        s.velocity -= 0.6 * kUp;  // > 3*g*dt, < max_sink: gentle but graded
        sim::Inputs in{};
        bool verdict = false;
        for (int t = 0; t < 10 && !verdict; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.on_ground || s.crashed) {
                // Fixture guard: the sink is BETWEEN the grace and the gentle
                // limit, so ONLY the attitude check can produce this verdict.
                const double sink =
                    -glm::dot(s.velocity, sim::local_up(s.position));
                REQUIRE(sink > 3.0 * kAp.g * kAp.sim_dt);
                REQUIRE(sink < gp.max_sink_ms);
                REQUIRE(s.crashed);
                REQUIRE(!s.on_ground);
                verdict = true;
            }
        }
        REQUIRE(verdict);
    }
}

TEST_CASE(
    "ground: rolling on over-limit terrain: tracks a landable downhill, "
    "releases off a falling edge, crashes into a rising wall") {
    const sim::GroundParams gp = test_gp();

    SECTION("a within-limit downhill roll stays GROUNDED (pins follow_m)") {
        const world::HeightField hf = ramp_field(std::tan(10.0 * kPi / 180.0));
        const sim::Environment env = ground_env(hf, gp);
        const glm::dvec3 downhill = -toward_high_side(hf, kUp);
        sim::SimState s = grounded_roll_state(hf, kUp, 60.0, downhill);
        sim::Inputs in{};
        for (int t = 0; t < 600; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            REQUIRE(!s.crashed);
            REQUIRE(s.on_ground);  // the follow tolerance TRACKS the slope
        }
    }

    SECTION("rolling off a 45-deg falling edge releases airborne, no crash") {
        const world::HeightField hf = ramp_field(std::tan(45.0 * kPi / 180.0));
        const sim::Environment env = ground_env(hf, gp);
        // Start ON the high plateau, short of the crest, rolling toward it.
        const glm::dvec3 to_high = toward_high_side(hf, kUp);
        const glm::dvec3 start =
            glm::normalize(kUp + (300.0 / kAp.R) * to_high);
        // Fixture guard: the start is genuinely flat plateau.
        REQUIRE(glm::dot(hf.normal_at(start, gp.normal_probe_m),
                         glm::normalize(start)) > std::cos(2.0 * kPi / 180.0));
        sim::SimState s = grounded_roll_state(hf, start, 60.0, -to_high);
        sim::Inputs in{};
        int grounded_ticks = 0;
        bool released = false;
        for (int t = 0; t < 900 && !released; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            REQUIRE(!s.crashed);  // a falling edge NEVER crashes the roll
            if (s.on_ground) {
                ++grounded_ticks;
            } else {
                released = true;
            }
        }
        REQUIRE(grounded_ticks > 30);  // it really rolled first
        REQUIRE(released);             // then the edge released it
    }

    SECTION("rolling INTO a 45-deg rising wall crashes") {
        const world::HeightField hf = ramp_field(std::tan(45.0 * kPi / 180.0));
        const sim::Environment env = ground_env(hf, gp);
        // Start on the LOW flat, short of the wall base, rolling uphill.
        const glm::dvec3 to_high = toward_high_side(hf, kUp);
        const glm::dvec3 start =
            glm::normalize(kUp - (300.0 / kAp.R) * to_high);
        REQUIRE(glm::dot(hf.normal_at(start, gp.normal_probe_m),
                         glm::normalize(start)) > std::cos(2.0 * kPi / 180.0));
        sim::SimState s = grounded_roll_state(hf, start, 60.0, to_high);
        sim::Inputs in{};
        int grounded_ticks = 0;
        bool verdict = false;
        for (int t = 0; t < 900 && !verdict; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.crashed) {
                verdict = true;
            } else if (s.on_ground) {
                ++grounded_ticks;
            }
        }
        REQUIRE(grounded_ticks > 30);  // approached on the flat first
        REQUIRE(verdict);              // the wall is a crash, never a hoist
    }
}

TEST_CASE(
    "ground: the harness ClosedLoop threads env: contact FIRES through "
    "the instructor loop") {
    // The R3-review P1 deviation's contract (test/harness/instructor.h): the
    // env member exists so a ground scenario can fly the same world, and its
    // AT must assert the contact fires — a forgotten env fails loudly HERE.
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);
    const double r_g = hf.radius_at(kUp);

    sim::SimState s0 =
        harness::level_state(kAp, 120.0, r_g - kAp.R + 60.0, kUp, kHeading);
    // Aim steeply below the horizon: the instructor flies it into the hill.
    const glm::dvec3 nose = s0.orientation * glm::dvec3{0.0, 0.0, -1.0};
    const glm::dvec3 aim0 =
        glm::normalize(nose - std::tan(30.0 * kPi / 180.0) * kUp);
    harness::ClosedLoop loop(s0, aim0);
    loop.env = &env;
    bool fired = false;
    for (int t = 0; t < 1200 && !fired; ++t) {
        loop.tick(1.0, kAp, kCp);
        fired = loop.state.crashed || loop.state.on_ground;
    }
    REQUIRE(fired);  // the loop consumed env: the terrain verdict happened
}
// ==========================================================================
// R4g — wheel brakes (Chad's fly ask, 2026-07-15).
// ==========================================================================

// Held brake shortens the rollout; brake 0 is bit-identical to the pre-brake
// kernel (a strict superset); an airborne brake-hold changes NOTHING (the
// GROUNDED regime is the only consumer — brakes cannot fly the plane).
TEST_CASE("ground R4g: wheel brake shortens the rollout; 0 = bit-identical") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.brake_friction = 0.5;  // the TEST'S OWN dial (config-relative rule)
    const sim::Environment env = ground_env(hf, gp);

    auto rollout_ticks = [&](float brake) {
        sim::SimState s = grounded_roll_state(hf, kUp, 30.0, kHeading);
        sim::Inputs in{};
        in.wheel_brake = brake;
        for (int t = 0; t < 20000; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            REQUIRE(!s.crashed);
            if (glm::length(s.velocity) == 0.0) return t;
        }
        return -1;
    };
    const int free_roll = rollout_ticks(0.0f);
    const int braked = rollout_ticks(1.0f);
    REQUIRE(free_roll > 0);
    REQUIRE(braked > 0);
    // Fixture-no-op guard + the mechanism: the decel ratio must show the
    // brake term ((friction+brake)/friction ~ 7.25x here). Allow slack for
    // aero drag (which brakes both runs equally).
    REQUIRE(braked * 3 < free_roll);

    // brake = 0 bit-identity: with the field live but the pedal up, the
    // trajectory equals a GroundParams with brake_friction 0 (the pre-R4g
    // kernel) — the strict-superset proof at the mechanism level.
    sim::GroundParams gp0 = test_gp();  // brake_friction defaults 0
    const sim::Environment env0 = ground_env(hf, gp0);
    sim::SimState a = grounded_roll_state(hf, kUp, 30.0, kHeading);
    sim::SimState b = a;
    sim::Inputs in{};  // wheel_brake 0
    for (int t = 0; t < 600; ++t) {
        a = sim::step(a, in, kAp, &env, kAp.sim_dt);
        b = sim::step(b, in, kAp, &env0, kAp.sim_dt);
    }
    REQUIRE(a.position == b.position);
    REQUIRE(a.velocity == b.velocity);

    // Airborne: a held brake is inert (no consumer outside GROUNDED).
    sim::SimState f0 = above_terrain(hf, 80.0, 400.0, 0.0);
    sim::SimState f1 = f0;
    sim::Inputs brk{};
    brk.wheel_brake = 1.0f;
    sim::Inputs clean{};
    for (int t = 0; t < 240; ++t) {
        f0 = sim::step(f0, clean, kAp, &env, kAp.sim_dt);
        f1 = sim::step(f1, brk, kAp, &env, kAp.sim_dt);
        REQUIRE(!f0.on_ground);  // fixture: stays airborne for the whole leg
    }
    REQUIRE(f0.position == f1.position);
    REQUIRE(f0.velocity == f1.velocity);
}

// A full-throttle roll under full brakes HOLDS (the runup — the stiction
// clamp pins the speed at exactly zero); releasing the brakes lets the same
// throttle accelerate. Driven through control::step so the leg fails if
// either controller emit site drops the field (the Fable-flagged fork).
TEST_CASE("ground R4g: brake-hold runup pins the plane; release rolls") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    // v5 RECONCILIATION (2026-07-24): 0.5 -> 0.60 — the rung-D fighter
    // engine raised T_max/(m g) to 0.612 and the old fixture brake no longer
    // satisfied the premise (matches the shipped game.toml re-derivation).
    gp.brake_friction = 0.60;  // friction+brake > T_max/(m g); friction alone <
    const sim::Environment env = ground_env(hf, gp);
    REQUIRE(gp.friction < kAp.T_max / (kAp.mass * kAp.g));
    REQUIRE(gp.friction + gp.brake_friction > kAp.T_max / (kAp.mass * kAp.g));

    sim::SimState s = grounded_roll_state(hf, kUp, 0.0, kHeading);
    s.throttle = 1.0;
    control::Internal ci = control::reset();
    control::Input in;
    in.target_dir_world = kHeading;  // aim ahead, stick neutral-ish
    in.throttle = 1.0;
    in.wheel_brake = 1.0;
    in.grounded = false;  // past the spawn tick: the live regime
    for (int t = 0; t < 400; ++t) {
        const control::Output o =
            control::step(s, in, ci, kAp, kCp, &env, kAp.sim_dt);
        ci = o.internal;
        s = sim::step(s, o.inputs, kAp, &env, kAp.sim_dt);
        REQUIRE(s.on_ground);
    }
    REQUIRE(glm::length(s.velocity) == 0.0);  // pinned through the seam

    in.wheel_brake = 0.0;  // brakes off, same throttle
    for (int t = 0; t < 400; ++t) {
        const control::Output o =
            control::step(s, in, ci, kAp, kCp, &env, kAp.sim_dt);
        ci = o.internal;
        s = sim::step(s, o.inputs, kAp, &env, kAp.sim_dt);
    }
    REQUIRE(glm::length(s.velocity) > 5.0);  // the roll accelerates again
}

// ==========================================================================
// R4g — grounded roll alignment (Chad: "a hard turn made my wings sink
// below the ground").
// ==========================================================================

// A GROUNDED airframe holding full aero roll input stays wings-level against
// the terrain plane: the WINGTIP never sweeps below the local surface (the
// Fable-named killer test — it fails immediately if the alignment edit sits
// in the pre-rotation slot the integrator overwrites). Pitch stays free
// (rate about body +X unaffected by the align), and the body roll rate is
// pinned at zero while grounded.
TEST_CASE(
    "ground R4g: grounded roll aligns to the terrain plane; wingtips stay "
    "above ground under full aero roll") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.roll_align_rate = 120.0 * kPi / 180.0;  // the test's own dial
    const sim::Environment env = ground_env(hf, gp);

    // Start with a REAL bank (10 deg about the body forward axis) + a live
    // roll rate — the "hard turn dipped a wing" state, made deterministic
    // (aero roll authority at ground speeds is too weak to bank the fixture
    // itself: measured ~1.75 deg after 3.75 s of full aileron — a fixture
    // no-op for the leveling leg).
    sim::SimState s = grounded_roll_state(hf, kUp, 45.0, kHeading);
    {
        const glm::dvec3 fwd = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        s.orientation = glm::normalize(glm::angleAxis(10.0 * kPi / 180.0, fwd) *
                                       s.orientation);
        s.angular_vel.z = 0.6;  // a live roll rate the align must also kill
    }
    sim::Inputs in{};
    in.roll = 1.0f;  // full aero roll held the whole run
    in.throttle = 0.4f;
    const double halfspan = 5.0;  // Bf 109 ~9.9 m span; 10 deg -> tip 0.87 m
                                  // below the CG plane, above ground at 1.5 m
                                  // contact height throughout the leveling
    double max_ail_rate = 0.0;
    bool leveled = false;
    for (int t = 0; t < 900; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        if (!s.on_ground) break;  // (should not lift at 45 m/s, throttle 0.4)
        if (t > 0)  // the seeded 0.6 rad/s dies on the FIRST grounded tick
            max_ail_rate = std::max(max_ail_rate, std::abs(s.angular_vel.z));
        // The killer invariant: both wingtips above the local terrain.
        const glm::dvec3 right = s.orientation * glm::dvec3{1.0, 0.0, 0.0};
        for (double sgn : {-1.0, 1.0}) {
            const glm::dvec3 tip = s.position + sgn * halfspan * right;
            const glm::dvec3 td = glm::normalize(tip);
            REQUIRE(glm::length(tip) >
                    hf.radius_at(td) - 0.05);  // 5 cm numerical slack
        }
        const glm::dvec3 bu = s.orientation * glm::dvec3{0.0, 1.0, 0.0};
        if (glm::dot(bu, glm::normalize(s.position)) > 0.9999) {
            leveled = true;  // 10 deg at 120 deg/s: level inside ~0.1 s
            if (t > 30) break;
        }
    }
    REQUIRE(s.on_ground);
    REQUIRE(leveled);
    // Roll rate pinned every grounded tick after the first (the align zeroes
    // omega_z post-integration).
    REQUIRE(max_ail_rate == 0.0);

    // Alignment OFF (rate 0) is the pre-R4g kernel: the same seeded bank
    // PERSISTS (nothing on the ground levels attitude) — the fixture-no-op
    // guard proving the leveling above was the mechanism, not the fixture.
    sim::GroundParams gp0 = test_gp();  // roll_align_rate defaults 0
    const sim::Environment env0 = ground_env(hf, gp0);
    sim::SimState p = grounded_roll_state(hf, kUp, 45.0, kHeading);
    {
        const glm::dvec3 fwd = p.orientation * glm::dvec3{0.0, 0.0, -1.0};
        p.orientation = glm::normalize(glm::angleAxis(10.0 * kPi / 180.0, fwd) *
                                       p.orientation);
    }
    sim::Inputs hands_off{};  // no aileron: pure persistence of the bank
    hands_off.throttle = 0.4f;
    for (int t = 0; t < 120 && p.on_ground; ++t)
        p = sim::step(p, hands_off, kAp, &env0, kAp.sim_dt);
    const glm::dvec3 bup0 = p.orientation * glm::dvec3{0.0, 1.0, 0.0};
    REQUIRE(glm::dot(bup0, glm::normalize(p.position)) <
            std::cos(5.0 * kPi / 180.0));  // still banked well past 5 deg
}

// Pitch freedom under alignment: a held pitch input while GROUNDED still
// produces pitch rate (the rotate is untouched — only the roll axis is
// constrained; a shortest-arc body_up->n mutant would kill this).
TEST_CASE("ground R4g: the align leaves the takeoff rotate (pitch) live") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.roll_align_rate = 120.0 * kPi / 180.0;
    const sim::Environment env = ground_env(hf, gp);

    sim::SimState s = grounded_roll_state(hf, kUp, 40.0, kHeading);
    sim::Inputs in{};
    in.pitch = 1.0f;
    double max_pitch_rate = 0.0;
    for (int t = 0; t < 60 && s.on_ground; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        max_pitch_rate = std::max(max_pitch_rate, std::abs(s.angular_vel.x));
    }
    REQUIRE(max_pitch_rate > 0.01);  // the elevator still rotates the nose
}

// ==========================================================================
// R4f — building collision (Chad: "I go right through houses").
// ==========================================================================

namespace {
// Two prisms near the generic test point: one AT the test heading ~400 m out
// (the target), one far away (the index must not smear). Radius/height are
// the test's own.
world::BuildingColliders test_buildings(const world::HeightField& hf) {
    world::BuildingColliders bc;
    std::vector<world::BuildingColliders::Prism> prisms;
    const glm::dvec3 tgt = glm::normalize(kUp + (400.0 / kAp.R) * kHeading);
    prisms.push_back({tgt, 20.0, 12.0});
    const glm::dvec3 far_p = glm::normalize(glm::dvec3{-0.7, 0.1, 0.7});
    prisms.push_back({far_p, 30.0, 25.0});
    bc.build(std::move(prisms), 256, 128, 0.0, 5.0, hf.R);
    return bc;
}
}  // namespace

// Flying INTO a building prism below its roof = crash; the same pass above
// the roof clears; a lateral offset outside radius+inflate clears. Null
// obstacles = bit-identical (the strict-superset proof).
TEST_CASE(
    "ground R4f: building prisms crash a through-flight, clear an "
    "over-flight, and null is bit-identical") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.obstacle_inflate_m = 3.0;
    gp.obstacle_base_margin_m = 5.0;
    const world::BuildingColliders bc = test_buildings(hf);
    sim::Environment env = ground_env(hf, gp);
    env.obstacles = &bc;

    const double base =
        hf.radius_at(glm::normalize(kUp + (400.0 / kAp.R) * kHeading));

    const glm::dvec3 prism_dir =
        glm::normalize(kUp + (400.0 / kAp.R) * kHeading);
    // The untrimmed raw-sim pass SINKS (~2.5 m/s at this trim — no
    // controller holds altitude), so the approach is kept SHORT (100 m,
    // ~1.5 s) and the track altitude high enough that terrain stays out of
    // reach for every leg; the crash verdict reports the distance to the
    // prism so the through-leg can PROVE it hit the building, not the
    // ground (the first fixture sank into terrain 143 m short and the
    // "crash" verdict lied about its cause).
    double crash_prism_dist = -1.0;
    auto fly_leg = [&](double alt_above_base,
                       const glm::dvec3& lateral_off) -> bool {
        crash_prism_dist = -1.0;
        const glm::dvec3 start = glm::normalize(
            kUp + (300.0 / kAp.R) * kHeading + lateral_off / kAp.R);
        sim::SimState s = harness::level_state(
            kAp, 70.0, base + alt_above_base - kAp.R, start, kHeading);
        s.throttle = 0.6;
        sim::Inputs in{};
        in.throttle = 0.6f;
        for (int t = 0; t < 150; ++t) {
            s = sim::step(s, in, kAp, &env, kAp.sim_dt);
            if (s.crashed) {
                const glm::dvec3 d = glm::normalize(s.position);
                crash_prism_dist = glm::length(d - prism_dir) * kAp.R;
                UNSCOPED_INFO("crash t="
                              << t << " prism_dist=" << crash_prism_dist
                              << " above_terrain="
                              << glm::length(s.position) - hf.radius_at(d));
                return true;
            }
        }
        return false;
    };

    // Through the prism band (CG at base+8 < roof at base+12): crash, AND
    // the crash happened AT the prism (inside radius 20 + inflate 3 + a
    // tick of travel) — the fixture-no-op guard against a terrain sink.
    REQUIRE(fly_leg(8.0, glm::dvec3{0.0}));
    REQUIRE(crash_prism_dist >= 0.0);
    REQUIRE(crash_prism_dist < 20.0 + 3.0 + 2.0);
    // Over the roof (+30 m): clear (same track, only altitude differs).
    REQUIRE(!fly_leg(30.0, glm::dvec3{0.0}));
    // 80 m lateral miss (radius 20 + inflate 3 << 80): clear.
    REQUIRE(!fly_leg(8.0, 80.0 * glm::normalize(glm::cross(kHeading, kUp))));

    // Null-obstacles identity: the same through-flight with obstacles null
    // matches a pre-R4f Environment tick-for-tick (and does NOT crash —
    // proving the through-leg's crash above was the OBSTACLE field).
    sim::Environment env_null = ground_env(hf, gp);  // obstacles nullptr
    const glm::dvec3 start = glm::normalize(kUp + (300.0 / kAp.R) * kHeading);
    sim::SimState a =
        harness::level_state(kAp, 70.0, base + 8.0 - kAp.R, start, kHeading);
    sim::SimState b = a;
    sim::Inputs in{};
    in.throttle = 0.6f;
    for (int t = 0; t < 150; ++t) {
        a = sim::step(a, in, kAp, &env_null, kAp.sim_dt);
        REQUIRE(!a.crashed);
        b = sim::step(b, in, kAp, &env_null, kAp.sim_dt);
    }
    REQUIRE(a.position == b.position);
}

// A grounded taxi INTO the prism also crashes (consistent with the rising-
// wall rule), and the base-margin band tolerates the terrain the footprint
// spans: a CG BELOW base-margin (deep valley under a hilltop record) clears.
TEST_CASE(
    "ground R4f: taxi into a building crashes; below the base band clears") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.obstacle_inflate_m = 3.0;
    gp.obstacle_base_margin_m = 5.0;
    const world::BuildingColliders bc = test_buildings(hf);
    sim::Environment env = ground_env(hf, gp);
    env.obstacles = &bc;

    // Taxi from 300 m out, straight at the prism, 20 m/s ground roll.
    sim::SimState s = grounded_roll_state(
        hf, glm::normalize(kUp + (100.0 / kAp.R) * kHeading), 20.0, kHeading);
    sim::Inputs in{};
    in.throttle = 0.3f;
    bool crashed = false;
    for (int t = 0; t < 3000 && !crashed; ++t) {
        s = sim::step(s, in, kAp, &env, kAp.sim_dt);
        crashed = s.crashed;
    }
    REQUIRE(crashed);

    // Direct band check on the query itself: 10 m below base (margin 5) is
    // OUTSIDE the prism band — no hit (the false-crash class the margin
    // exists to kill); 2 m below (within margin) IS a hit.
    const glm::dvec3 tgt = glm::normalize(kUp + (400.0 / kAp.R) * kHeading);
    const double tbase = hf.radius_at(tgt);
    REQUIRE(!bc.hit(tgt * (tbase - 10.0), hf, 3.0, 5.0));
    REQUIRE(bc.hit(tgt * (tbase - 2.0), hf, 3.0, 5.0));
    REQUIRE(bc.hit(tgt * (tbase + 11.0), hf, 3.0, 5.0));   // just under roof
    REQUIRE(!bc.hit(tgt * (tbase + 13.0), hf, 3.0, 5.0));  // just over roof
}

// The APP-CHAIN brake legs (adversarial review P1 — the moved-consumer trap:
// the controller emit sites had coverage, the app forwarding sites had NONE,
// so deleting `ci.wheel_brake = in.wheel_brake` or the FrameInput->TickInput
// forward shipped a brakeless live mode through a green gate). Two legs, one
// per forwarding site: app::tick (TickInput -> control::Input) and
// app::step_frame (FrameInput -> TickInput). Each drives a grounded rollout
// brake-on vs brake-off and requires divergence — a dropped forward makes the
// two runs identical.
namespace {
app::LoopState grounded_loop(const world::HeightField& hf) {
    app::LoopState loop;
    loop.curr = grounded_roll_state(hf, kUp, 30.0, kHeading);
    loop.curr.throttle = 0.0;
    loop.prev = loop.curr;
    loop.prev_up = sim::local_up(loop.curr.position);
    loop.aim.reseed(loop.curr.orientation, loop.prev_up);
    loop.grounded = false;  // past the spawn tick: the live regime
    loop.internal = control::reset();
    return loop;
}
}  // namespace

TEST_CASE("ground R4g: the APP chain forwards the brake (tick + step_frame)") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = test_gp();
    gp.brake_friction = 0.5;
    const sim::Environment env = ground_env(hf, gp);

    // Leg 1: app::tick (TickInput.wheel_brake -> ci.wheel_brake).
    auto run_ticks = [&](double brake) {
        app::LoopState loop = grounded_loop(hf);
        for (int t = 0; t < 240; ++t) {
            app::TickInput in;
            in.throttle = 0.0;
            in.wheel_brake = brake;
            app::tick(loop, in, kAp, kCp, &env);
        }
        return glm::length(loop.curr.velocity);
    };
    const double v_free = run_ticks(0.0);
    const double v_braked = run_ticks(1.0);
    REQUIRE(v_free > 0.0);  // fixture: the free roll is still rolling
    REQUIRE(v_braked < v_free - 1.0);  // the brake reached the plant

    // Leg 2: app::step_frame (FrameInput.wheel_brake -> TickInput forward).
    auto run_frames = [&](double brake) {
        app::LoopState loop = grounded_loop(hf);
        app::Accumulator accum(kAp.sim_dt);
        double pdx = 0.0, pdy = 0.0;
        for (int f = 0; f < 24; ++f) {
            app::FrameInput fin;
            fin.throttle = 0.0;
            fin.wheel_brake = brake;
            app::step_frame(loop, accum, 10.0 * kAp.sim_dt, fin, pdx, pdy, kAp,
                            kCp, &env, nullptr);
        }
        return glm::length(loop.curr.velocity);
    };
    const double fv_free = run_frames(0.0);
    const double fv_braked = run_frames(1.0);
    REQUIRE(fv_free > 0.0);
    REQUIRE(fv_braked < fv_free - 1.0);
}

// ==========================================================================
// R4-FLY-5 — grounded CONSEQUENCES (Chad's ruling: damage, not prevention).
// ==========================================================================

namespace {
sim::GroundParams fly5_gp() {
    sim::GroundParams gp = test_gp();
    gp.ground_ang_damp = 1.5;
    gp.wing_halfspan_m = 5.0;
    gp.ground_loop_lat_g = 0.5;
    gp.brake_friction = 0.5;
    gp.brake_pitch_rate = 30.0 * kPi / 180.0;
    gp.prop_strike_pitch_rad = 8.0 * kPi / 180.0;
    return gp;
}
}  // namespace

// Controls need AIRSPEED on the ground: at a STOP the rudder produces no yaw
// rate (the q_att_floor is dropped while grounded — Chad: "rudder should not
// be operable when I am at a stop"), and residual rotation DECAYS through the
// tires instead of persisting forever. At rolling speed the same rudder
// genuinely yaws (the fixture-no-op guard).
TEST_CASE(
    "ground R4-FLY-5: dead stick at a stop, live rudder at speed, "
    "residual spin decays") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = fly5_gp();
    const sim::Environment env = ground_env(hf, gp);

    // Parked: full rudder for 2 s produces (essentially) no yaw rate.
    sim::SimState s = grounded_roll_state(hf, kUp, 0.0, kHeading);
    sim::Inputs in{};
    in.yaw = 1.0f;
    for (int t = 0; t < 480; ++t) s = sim::step(s, in, kAp, &env, kAp.sim_dt);
    REQUIRE(std::abs(s.angular_vel.y) < 1e-4);

    // Residual spin at rest DIES through the tires (no aero damping at q=0).
    sim::SimState r = grounded_roll_state(hf, kUp, 0.0, kHeading);
    r.angular_vel.y = 0.5;
    sim::Inputs off{};
    for (int t = 0; t < 720; ++t) r = sim::step(r, off, kAp, &env, kAp.sim_dt);
    REQUIRE(std::abs(r.angular_vel.y) < 0.01);

    // Rolling at 35 m/s: the same rudder yaws for real (true q authority).
    sim::SimState m = grounded_roll_state(hf, kUp, 35.0, kHeading);
    double max_yaw_rate = 0.0;
    for (int t = 0; t < 120 && m.on_ground && !m.crashed; ++t) {
        m = sim::step(m, in, kAp, &env, kAp.sim_dt);
        max_yaw_rate = std::max(max_yaw_rate, std::abs(m.angular_vel.y));
    }
    REQUIRE(max_yaw_rate > 0.02);
}

// Ground loop: a lateral impulse beyond the tire limit while rolling =
// crash ("turn too fast -> rotate and crash"); the same roll WITHOUT the
// impulse survives (fixture-no-op guard). The impulse is injected as a
// velocity dogleg — the physical class (aero side force at taxi yaw) is
// slower to build in a fixture, and the mechanism grades the applied
// LATERAL acceleration whatever produced it.
TEST_CASE("ground R4-FLY-5: over-limit lateral acceleration ground-loops") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = fly5_gp();
    const sim::Environment env = ground_env(hf, gp);

    // DIRECT decomposition legs (review P1 — the along-track exclusion was a
    // proven mutation survivor: deleting it left the whole suite green).
    // ground_contact is graded straight-on: a grounded state whose proposed
    // velocity carries a 2 g PURE ALONG-TRACK impulse (hard braking class)
    // must NOT loop; the same 2 g PURE LATERAL must (limit 0.5 g).
    {
        const sim::SimState s0 = grounded_roll_state(hf, kUp, 30.0, kHeading);
        const glm::dvec3 up0 = glm::normalize(s0.position);
        const glm::dvec3 vhat = glm::normalize(s0.velocity);
        const glm::dvec3 side = glm::normalize(glm::cross(up0, vhat));

        sim::SimState along = s0;
        along.velocity -= vhat * (2.0 * kAp.g * kAp.sim_dt);  // brake class
        sim::ground_contact(s0, along, hf, gp, kAp, 0.0, kAp.sim_dt);
        REQUIRE(!along.crashed);   // along-track EXCLUDED (delete the
        REQUIRE(along.on_ground);  // exclusion and this leg fails)

        sim::SimState lat = s0;
        lat.velocity += side * (2.0 * kAp.g * kAp.sim_dt);
        sim::ground_contact(s0, lat, hf, gp, kAp, 0.0, kAp.sim_dt);
        REQUIRE(lat.crashed);  // lateral past the tire limit = ground loop
    }

    // End-to-end: drive full rudder at speed until either crash or 6 s.
    sim::SimState t = grounded_roll_state(hf, kUp, 40.0, kHeading);
    sim::Inputs rudder{};
    rudder.yaw = 1.0f;
    rudder.throttle = 0.5f;
    bool looped = false;
    for (int k = 0; k < 1440 && !looped; ++k) {
        t = sim::step(t, rudder, kAp, &env, kAp.sim_dt);
        if (t.crashed) looped = true;
        if (!t.on_ground && !t.crashed) break;  // lifted off instead
    }
    // With a 0.5 g tire limit a full-rudder 40 m/s ground turn must loop.
    REQUIRE(looped);

    // Fixture-no-op guard: the same roll with rudder centered survives.
    sim::SimState c = grounded_roll_state(hf, kUp, 40.0, kHeading);
    sim::Inputs straight{};
    for (int k = 0; k < 480; ++k) {
        c = sim::step(c, straight, kAp, &env, kAp.sim_dt);
        REQUIRE(!c.crashed);
    }
}

// Nose-over: sustained full braking at speed pitches the nose down and fires
// prop_strike once past the clearance angle; gentle/no braking never does,
// and releasing the brake settles the nose back toward the terrain plane.
TEST_CASE("ground R4-FLY-5: hard braking noses over and strikes the prop") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = fly5_gp();
    const sim::Environment env = ground_env(hf, gp);

    sim::SimState s = grounded_roll_state(hf, kUp, 35.0, kHeading);
    sim::Inputs brk{};
    brk.wheel_brake = 1.0f;
    bool struck = false;
    double min_fwd_n = 1.0;
    for (int t = 0; t < 600 && !struck; ++t) {
        s = sim::step(s, brk, kAp, &env, kAp.sim_dt);
        REQUIRE(!s.crashed);
        const glm::dvec3 up = glm::normalize(s.position);
        const glm::dvec3 n = hf.normal_at(up, gp.normal_probe_m);
        const glm::dvec3 fwd = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        min_fwd_n = std::min(min_fwd_n, glm::dot(fwd, n));
        struck = s.prop_strike;
    }
    REQUIRE(struck);
    // Fixture-no-op guard: the nose GENUINELY dipped below the plane.
    REQUIRE(min_fwd_n < -std::sin(gp.prop_strike_pitch_rad) + 1e-9);

    // No braking: a plain rollout never strikes the prop.
    sim::SimState f = grounded_roll_state(hf, kUp, 35.0, kHeading);
    sim::Inputs coast{};
    for (int t = 0; t < 600; ++t) {
        f = sim::step(f, coast, kAp, &env, kAp.sim_dt);
        REQUIRE(!f.prop_strike);
    }

    // Release after a dip: the tail settles back (nose returns toward the
    // terrain plane instead of freezing nose-down).
    sim::SimState d = grounded_roll_state(hf, kUp, 35.0, kHeading);
    for (int t = 0; t < 60; ++t) d = sim::step(d, brk, kAp, &env, kAp.sim_dt);
    const glm::dvec3 up_d = glm::normalize(d.position);
    const glm::dvec3 n_d = hf.normal_at(up_d, gp.normal_probe_m);
    const double dipped =
        glm::dot(d.orientation * glm::dvec3{0.0, 0.0, -1.0}, n_d);
    REQUIRE(dipped < -0.01);  // a real dip happened in 60 braked ticks
    sim::Inputs rel{};
    for (int t = 0; t < 240; ++t) d = sim::step(d, rel, kAp, &env, kAp.sim_dt);
    const double settled =
        glm::dot(d.orientation * glm::dvec3{0.0, 0.0, -1.0}, n_d);
    REQUIRE(settled > dipped + 0.005);  // recovering toward the plane
    REQUIRE(settled < 0.02);            // and not overshooting past it
}

// R4-FLY-6 (Chad: "slow braking still tips forward — had to be careful to
// stop without nosing over"): the dip rate fades with ground speed
// (x min(1, sp/noseover_full_speed_ms)). Three legs: the fixture-no-op
// guard first — with the dial OFF this fixture's full-brake 6 m/s stop
// GENUINELY strikes (the felt defect exists here, the mechanism isn't a
// no-op); then the dial rescues the crawl; then braking from speed still
// strikes (the consequence survives the fix).
TEST_CASE("ground R4-FLY-6: the brake nose-dip fades at crawl speed") {
    const world::HeightField hf = uniform_field(200.0);

    auto brake_strikes = [&](double dial_ms, double speed) {
        sim::GroundParams gp = fly5_gp();
        gp.noseover_full_speed_ms = dial_ms;
        const sim::Environment env = ground_env(hf, gp);
        sim::SimState s = grounded_roll_state(hf, kUp, speed, kHeading);
        sim::Inputs brk{};
        brk.wheel_brake = 1.0f;
        bool struck = false;
        for (int t = 0; t < 720 && !struck; ++t) {
            s = sim::step(s, brk, kAp, &env, kAp.sim_dt);
            struck = s.prop_strike;
        }
        return struck;
    };
    REQUIRE(brake_strikes(0.0, 6.0));    // baseline: unscaled, a slow stop
                                         // noses over (Chad's defect)
    REQUIRE(!brake_strikes(12.0, 6.0));  // scaled: the crawl-stop is safe
    REQUIRE(brake_strikes(12.0, 35.0));  // from speed the prop still breaks
}

// R4-FLY-6 — the touchdown REPORT (the render-FX seam): app::tick reports
// the airborne->GROUNDED capture EDGE — with the capture tick's position +
// ground speed — exactly as often as the edge occurs (a bounce re-reports;
// a grounded rollout tick never does). Pure report: state is untouched.
TEST_CASE("ground R4-FLY-6: app::tick reports the touchdown capture edge") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = test_gp();
    const sim::Environment env = ground_env(hf, gp);
    app::LoopState st;
    const double r_g = hf.radius_at(kUp);
    st.curr = harness::level_state(
        kAp, 45.0, r_g - kAp.R + gp.contact_height_m + 0.4, kUp, kHeading);
    st.curr.velocity -= 1.0 * kUp;  // gentle sink onto the field
    st.prev = st.curr;
    st.prev_up = sim::local_up(st.curr.position);
    st.aim.reseed(st.curr.orientation, st.prev_up);
    st.grounded = true;  // spawn-tick pairing, as the app seeds it
    app::TickInput in;
    in.raw_mode = true;  // dead stick: land hands-off

    int edges = 0, reports = 0;
    double first_report_speed = 0.0;
    bool was_ground = false, bounced = false;
    for (int t = 0; t < 900; ++t) {
        const app::TickResult r = app::tick(st, in, kAp, kCp, &env);
        const bool edge = st.curr.on_ground && !was_ground;
        if (edge) ++edges;
        if (r.touchdown) {
            ++reports;
            REQUIRE(edge);  // reported exactly ON the capture tick
            REQUIRE(r.touchdown_pos == st.curr.position);
            if (reports == 1) first_report_speed = r.touchdown_speed;
        }
        was_ground = st.curr.on_ground;
        // Force a BOUNCE after the first landing (review P2-1: with a
        // single edge, `reports == edges` degenerates to 1 == 1 and a
        // first-edge-only LATCH mutation survives): pop the rolling
        // airframe a hand's width off the field; gravity sags it back and
        // the re-capture is a SECOND honest edge that must re-report.
        if (!bounced && reports == 1 && st.curr.on_ground && t > 60) {
            st.curr.on_ground = false;
            st.curr.position += sim::local_up(st.curr.position) * 0.1;
            was_ground = false;
            bounced = true;
        }
    }
    REQUIRE(bounced);
    REQUIRE(edges >= 2);        // landing + the forced bounce both captured
    REQUIRE(reports == edges);  // one report PER EDGE — a latch fails here
    REQUIRE(first_report_speed > 10.0);  // the 45 m/s approach's ground speed
}

// Wingtip strike: a grounded airframe banked far enough that a tip sweeps
// below the terrain sets wing_strike with the CORRECT SIGN, wings-level
// never does, and the event is TRANSIENT (re-derived, clears when level).
TEST_CASE("ground R4-FLY-5: a banked wingtip below terrain sets wing_strike") {
    const world::HeightField hf = uniform_field(200.0);
    const sim::GroundParams gp = fly5_gp();  // halfspan 5, contact 1.5:
    // tip below terrain needs sin(bank) > 1.5/5 -> bank > ~17.5 deg
    const sim::Environment env = ground_env(hf, gp);

    auto banked = [&](double bank_deg) {
        sim::SimState s = grounded_roll_state(hf, kUp, 20.0, kHeading);
        const glm::dvec3 fwd = s.orientation * glm::dvec3{0.0, 0.0, -1.0};
        s.orientation = glm::normalize(
            glm::angleAxis(bank_deg * kPi / 180.0, fwd) * s.orientation);
        return sim::step(s, sim::Inputs{}, kAp, &env, kAp.sim_dt);
    };
    // +25 deg about FWD rolls RIGHT (right-hand rule about the NOSE axis —
    // note body +omega_z is about the AFT axis, the opposite handedness):
    // the RIGHT tip (+X) drops -> wing_strike must report +1.
    const sim::SimState r = banked(+25.0);
    REQUIRE(r.wing_strike == +1);
    const sim::SimState l = banked(-25.0);
    REQUIRE(l.wing_strike == -1);
    const sim::SimState level = banked(0.0);
    REQUIRE(level.wing_strike == 0);
    // Transient: a strike tick followed by a level tick clears the event.
    sim::SimState after = r;
    const glm::dvec3 fwd = after.orientation * glm::dvec3{0.0, 0.0, -1.0};
    after.orientation = glm::normalize(
        glm::angleAxis(-25.0 * kPi / 180.0, fwd) * after.orientation);
    after = sim::step(after, sim::Inputs{}, kAp, &env, kAp.sim_dt);
    REQUIRE(after.wing_strike == 0);
}

// R4-FLY-5 — ground events feed the COMPONENT DAMAGE MODEL (the app seam:
// kernel detects, app::tick routes into the SAME DamageState ballistic hits
// feed). Two legs: a banked scrape chips the STRUCK wing (and only it); a
// braking nose-over kills the engine (dead stick). Null CombatWorld ignores
// the events (bit-identity — every pre-combat caller).
TEST_CASE("ground R4-FLY-5: wing scrape + prop strike feed the damage model") {
    const world::HeightField hf = uniform_field(200.0);
    sim::GroundParams gp = fly5_gp();
    const sim::Environment env = ground_env(hf, gp);

    // Wing scrape: grounded, banked right past the tip-strike angle, rolling.
    {
        app::LoopState loop = grounded_loop(hf);
        const glm::dvec3 fwd =
            loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        loop.curr.orientation = glm::normalize(
            glm::angleAxis(25.0 * kPi / 180.0, fwd) * loop.curr.orientation);
        combat::CombatWorld cw;
        const double wl0 = cw.damage.wing_left, wr0 = cw.damage.wing_right;
        for (int t = 0; t < 60; ++t) {
            app::TickInput in;
            app::tick(loop, in, kAp, kCp, &env, nullptr, nullptr, &cw);
            if (loop.curr.crashed) break;
        }
        // +25 deg about fwd = roll RIGHT: the RIGHT wing chips, left intact.
        REQUIRE(cw.damage.wing_right < wr0);
        REQUIRE(cw.damage.wing_left == wl0);
    }

    // Prop strike: full brakes from speed until the nose-over fires.
    {
        app::LoopState loop = grounded_loop(hf);
        combat::CombatWorld cw;
        REQUIRE(cw.damage.engine == 1.0);
        bool dead_stick = false;
        for (int t = 0; t < 900 && !dead_stick; ++t) {
            app::TickInput in;
            in.wheel_brake = 1.0;
            app::tick(loop, in, kAp, kCp, &env, nullptr, nullptr, &cw);
            dead_stick = cw.damage.engine <= 0.0;
        }
        REQUIRE(dead_stick);
    }

    // Null CombatWorld: the same scrape ticks run event-ignored (no crash, no
    // damage sink — the pre-combat caller class).
    {
        app::LoopState loop = grounded_loop(hf);
        const glm::dvec3 fwd =
            loop.curr.orientation * glm::dvec3{0.0, 0.0, -1.0};
        loop.curr.orientation = glm::normalize(
            glm::angleAxis(25.0 * kPi / 180.0, fwd) * loop.curr.orientation);
        for (int t = 0; t < 30; ++t) {
            app::TickInput in;
            app::tick(loop, in, kAp, kCp, &env);
        }
        SUCCEED("null cw: events ignored, no deref");
    }
}
