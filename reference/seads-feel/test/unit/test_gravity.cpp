// R5 — the GravityField Escape Ceiling (MASTER_PLAN §3.B; sim/fields.h).
// Fable-BEFORE-folded suite (2026-07-15): the analytic legs must exercise the
// erfc TAIL (an in-band pin + an independent quadrature cross-check — a
// sigma-vs-sigma*sqrt(2) argument bug passes a table-only suite), the far
// tail is clipped to exact zero (the 46 km subnormal shell is
// unrepresentable), and the climb legs bracket the discrete escape threshold
// (237.747 m/s at dt = 1/120; analytic 237.708) at 236 / 240 — margins ~45x
// the symplectic energy band (9.7 J/kg, zero secular drift).
//
// The climb flies S = 0 params (kills lift, drag, AND the S-wvane side force
// in one honest param — and ARMS the applied-impulse-radial tripwire for the
// whole flight, since thrust is also zero).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <limits>
#include <sstream>
#include <string>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "sim/state.h"
#include "sim/step.h"
#include "sim/world.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kDt = 1.0 / 120.0;  // fixed sim tick (SPEC §7)

// Zero-aero airframe (the test_ballistic pattern): S = 0, T_max = 0 — pure
// ballistic point, every energy claim exact up to the integrator.
sim::AircraftParams ballistic_params() {
    sim::AircraftParams p;
    p.R = 15000.0;
    p.g = 9.81;
    p.rho = 1.0;
    p.sim_dt = kDt;
    p.mass = 1.0;
    p.I_pitch = p.I_yaw = p.I_roll = 1.0;
    p.v_full = 1.0;
    p.v_redline = 2.0;
    p.min_frac = 1.0;
    p.v_dir_eps = 1.0;
    return p;
}

const sim::AircraftParams kWorld = ballistic_params();

// The §3.B design field (the GravityField defaults ARE the spec table).
const sim::GravityField kField{};
sim::Environment grav_only_env() {
    sim::Environment e;
    e.grav = &kField;
    return e;
}
const sim::Environment kEnv = grav_only_env();

// GENERIC sphere direction (lessons.md: no world axis may coincide with the
// truth — an axis-aligned fixture hides a fixed-axis bug).
const glm::dvec3 kDir = glm::normalize(glm::dvec3{0.37, -0.62, 0.53});

sim::SimState make_state(glm::dvec3 pos, glm::dvec3 vel) {
    sim::SimState s;
    s.position = pos;
    s.velocity = vel;
    return s;
}

// Independent literals (NOT derived from the implementation's constants —
// the cross-check must not share an expression with what it checks):
constexpr double kSqrtPiOver2 = 1.2533141373155003;    // sqrt(pi/2)
constexpr double kErfcInvSqrt2 = 0.31731050786291404;  // erfc(1/sqrt(2))

double v_esc(double h) {
    return std::sqrt(2.0 * sim::binding_energy_above(h, &kEnv, kWorld));
}

// Composite Simpson over [a, b] — the independent quadrature arm.
template <typename F>
double simpson(F f, double a, double b, int n /* even */) {
    const double dh = (b - a) / n;
    double acc = f(a) + f(b);
    for (int i = 1; i < n; ++i) {
        acc += f(a + i * dh) * (i % 2 == 1 ? 4.0 : 2.0);
    }
    return acc * dh / 3.0;
}

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_grav_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

}  // namespace

TEST_CASE("g_at: null env or null grav returns the literal p.g everywhere") {
    sim::Environment no_grav;  // non-null env, grav still null
    for (double h : {0.0, 120.0, 4000.0, 5000.0, 8000.0, 1.0e6}) {
        CHECK(sim::g_at(h, nullptr, kWorld) == kWorld.g);
        CHECK(sim::g_at(h, &no_grav, kWorld) == kWorld.g);
    }
    CHECK(sim::binding_energy_above(4000.0, nullptr, kWorld) ==
          std::numeric_limits<double>::infinity());
    CHECK(sim::binding_energy_above(4000.0, &no_grav, kWorld) ==
          std::numeric_limits<double>::infinity());
    // A constant-g world is infinitely bound: E_spec = -inf, escape
    // unrepresentable (will_escape is structurally false at R7).
    CHECK(sim::specific_energy(500.0, 4000.0, nullptr, kWorld) ==
          -std::numeric_limits<double>::infinity());
}

TEST_CASE("g_at: full g through the fight band, C1 at h_g0, monotone above") {
    // EXACT equality below and AT the knee — the fight band and the grounded
    // regime's constant-g reads must never see a different number.
    for (double h : {-50.0, 0.0, 120.0, 2500.0, 4999.0, 5000.0}) {
        CHECK(sim::g_at(h, &kEnv, kWorld) == kWorld.g);
    }
    // Zero gradient at the knee (no felt line entering the band)...
    CHECK(sim::g_at(5000.0 + 1.0e-3, &kEnv, kWorld) ==
          Catch::Approx(kWorld.g).epsilon(1e-12));
    // ...one-sigma point pinned in closed form...
    CHECK(sim::g_at(6500.0, &kEnv, kWorld) ==
          Catch::Approx(kWorld.g * std::exp(-0.5)).epsilon(1e-12));
    // ...and strictly decreasing through the whole designed band.
    double prev = sim::g_at(5000.0, &kEnv, kWorld);
    for (double h = 5250.0; h <= 12000.0; h += 250.0) {
        const double g = sim::g_at(h, &kEnv, kWorld);
        CHECK(g < prev);
        prev = g;
    }
}

TEST_CASE("g_at: the far tail is EXACT zero past the clip (P1-2)") {
    // x >= 26 => 0.0 exactly: the subnormal shell (~45-46 km at defaults,
    // where normalize() precision would spuriously trip the radial
    // invariants) is unrepresentable.
    const double h_cut = 5000.0 + 26.0 * 1500.0;  // x = 26 -> 44 km
    CHECK(sim::g_at(h_cut, &kEnv, kWorld) == 0.0);
    CHECK(sim::g_at(h_cut + 1.0, &kEnv, kWorld) == 0.0);
    CHECK(sim::g_at(1.0e9, &kEnv, kWorld) == 0.0);
    // Just inside the clip the field is still a positive NORMAL double.
    const double g_in = sim::g_at(h_cut - 1500.0, &kEnv, kWorld);  // x = 25
    CHECK(g_in > 0.0);
    CHECK(std::isnormal(g_in));
}

TEST_CASE("binding energy: the 3.B escape table, pinned at h = 0 (P2-4)") {
    // K = g*sigma*sqrt(pi/2) against the INDEPENDENT sqrt(pi/2) literal — a
    // typo'd pi/2 in the implementation cannot self-confirm here.
    CHECK(sim::binding_energy_above(5000.0, &kEnv, kWorld) ==
          Catch::Approx(kWorld.g * 1500.0 * kSqrtPiOver2).epsilon(1e-12));
    // The design table (§3.B): the deck row is v_esc(0) = 367.40 — NOT
    // v_esc(120 AGL) = 364.2; pin the altitude the number actually belongs
    // to so the leg can't half-match either.
    CHECK(v_esc(0.0) == Catch::Approx(367.40).margin(0.05));
    CHECK(v_esc(4000.0) == Catch::Approx(237.708).margin(0.01));
    CHECK(v_esc(5000.0) == Catch::Approx(192.05).margin(0.05));
}

TEST_CASE("binding energy: in-band erfc pin (the sigma*sqrt(2) tripwire)") {
    // P1-3: every table number above sits at erfc(0) or below the band — a
    // sigma-vs-sigma*sqrt(2) argument bug passes ALL of them while forking
    // in-band binding ~2x (5852 -> 2894 J/kg at h_g0 + sigma). This pin
    // exercises the tail through the independent erfc(1/sqrt(2)) literal.
    CHECK(sim::binding_energy_above(6500.0, &kEnv, kWorld) ==
          Catch::Approx(kWorld.g * 1500.0 * kSqrtPiOver2 * kErfcInvSqrt2)
              .epsilon(1e-9));
}

TEST_CASE("binding energy: independent Simpson quadrature cross-check") {
    // The closed form is THE single source (the R7 telemetry consumes it) —
    // so it must be pinned against an arm it cannot share a bug with: brute
    // quadrature of g_at itself. Constant piece handled exactly; Simpson
    // runs only on the smooth Gaussian domain (C1 kink at h_g0 would cost
    // two orders of accuracy). Truncation at h_g0 + 10*sigma leaves
    // K*erfc(10/sqrt(2)) ~ 1e-18 J/kg — invisible at rel 1e-8.
    const double h0 = 5000.0, top = 5000.0 + 10.0 * 1500.0;
    const auto g_of_h = [](double h) { return sim::g_at(h, &kEnv, kWorld); };
    for (double h : {0.0, 4000.0, 5000.0, 6500.0, 8000.0}) {
        const double quad = (h < h0 ? kWorld.g * (h0 - h) : 0.0) +
                            simpson(g_of_h, std::max(h, h0), top, 8000);
        const double closed = sim::binding_energy_above(h, &kEnv, kWorld);
        REQUIRE(closed > 0.0);  // fixture-no-op guard: a real baseline term
        CHECK(quad == Catch::Approx(closed).epsilon(1e-8));
    }
}

TEST_CASE(
    "climb-till-you-die: 236 m/s from 4 km enters the band, falls "
    "back, and stays bound") {
    // Fable-bisected discrete threshold: 237.747 m/s (dt = 1/120). Launch
    // 1.75 m/s below it — the plane must top out INSIDE the band (apex
    // ~8424 m: the fixture-no-op guard demands it actually flew the taper)
    // and come home.
    sim::SimState s = make_state(kDir * (kWorld.R + 4000.0), kDir * 236.0);
    double max_E = -std::numeric_limits<double>::infinity();
    double apex = 0.0;
    int apex_tick = -1;
    for (int i = 0; i < 90 * 120; ++i) {
        s = sim::step(s, {}, kWorld, &kEnv, kDt);
        const double alt = sim::altitude(s.position, kWorld);
        const double E =
            sim::specific_energy(glm::length(s.velocity), alt, &kEnv, kWorld);
        max_E = std::max(max_E, E);
        apex = std::max(apex, alt);
        if (glm::dot(s.velocity, sim::local_up(s.position)) <= 0.0) {
            apex_tick = i;
            break;
        }
    }
    REQUIRE(apex_tick > 0);  // topped out within 90 s
    REQUIRE(apex > 5000.0);  // genuinely entered the taper band
    REQUIRE(apex < 9500.0);  // and did NOT reach the escape gate
    REQUIRE(max_E < 0.0);    // bound at every tick (energy honest)
    // Past apex it comes DOWN: strictly decreasing altitude for 20 s.
    double prev_alt = sim::altitude(s.position, kWorld);
    int rises = 0;
    for (int i = 0; i < 20 * 120; ++i) {
        s = sim::step(s, {}, kWorld, &kEnv, kDt);
        const double alt = sim::altitude(s.position, kWorld);
        if (alt >= prev_alt) ++rises;
        prev_alt = alt;
    }
    REQUIRE(rises == 0);
}

TEST_CASE(
    "climb-till-you-die: 240 m/s from 4 km escapes -- E_spec > 0 every "
    "tick, past h_g0 + 3 sigma and still climbing at t = 90 s") {
    // 2.25 m/s above the discrete threshold. NOT an altitude-divergence
    // check (v_inf = 32.8 m/s needs 392 s to reach 20 km): escape is the
    // ENERGY verdict — E_spec > 0 throughout — plus clearing the designed
    // band (9500 m at t = 90; Fable: crosses at ~72 s) with radial v still
    // positive. Also stays far below the 44 km tail clip.
    sim::SimState s = make_state(kDir * (kWorld.R + 4000.0), kDir * 240.0);
    double min_E = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 90 * 120; ++i) {
        s = sim::step(s, {}, kWorld, &kEnv, kDt);
        min_E = std::min(min_E,
                         sim::specific_energy(glm::length(s.velocity),
                                              sim::altitude(s.position, kWorld),
                                              &kEnv, kWorld));
    }
    REQUIRE(min_E > 0.0);  // never dipped bound — irreversible from launch
    REQUIRE(sim::altitude(s.position, kWorld) > 9500.0);
    REQUIRE(glm::dot(s.velocity, sim::local_up(s.position)) > 0.0);
}

TEST_CASE(
    "an ACTIVE field is bit-identical below the band (fight band "
    "untouched by activation)") {
    // The real airframe, full aero, generic attitude inputs, 10 s at 1000 m:
    // with grav ACTIVE vs env NULL the trajectories must be EXACTLY equal —
    // g_at == p.g below h_g0 is an identity, not an approximation. This is
    // what makes the activation HALT purely about the ceiling.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, kDir));
    sim::SimState a = make_state(kDir * (ap.R + 1000.0), east * 100.0);
    sim::SimState b = a;
    sim::Inputs in{};
    in.pitch = 0.25f;
    in.roll = -0.15f;
    in.throttle = 0.8f;
    for (int i = 0; i < 10 * 120; ++i) {
        a = sim::step(a, in, ap, &kEnv, ap.sim_dt);
        b = sim::step(b, in, ap, nullptr, ap.sim_dt);
    }
    REQUIRE(a.position == b.position);
    REQUIRE(a.velocity == b.velocity);
    REQUIRE(a.orientation == b.orientation);
    REQUIRE(a.angular_vel == b.angular_vel);
    REQUIRE(sim::altitude(a.position, ap) < 5000.0);  // fixture honest
}

TEST_CASE(
    "in-band single-step differential: the ACTIVE field changes "
    "gravity and ONLY gravity (thrust/aero leak tripwire)") {
    // Adversarial-review P1 (mutation-proven gap): a taper leak into thrust
    // (or q, drag, lift, torque) survived the whole suite — every
    // taper-exercising leg was thrust-free and every thrust-carrying leg sat
    // below the band. This leg flies the REAL airframe IN the band with
    // throttle and airspeed live, steps ONCE from the identical state with
    // the field active vs null: the velocity delta must be EXACTLY the
    // gravity delta, and rotation must not fork at all.
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const glm::dvec3 pos = kDir * (ap.R + 6500.0);  // h_g0 + sigma: in-band
    const glm::dvec3 east =
        glm::normalize(glm::cross(glm::dvec3{0.0, 1.0, 0.0}, kDir));
    sim::SimState s0 = make_state(pos, east * 100.0);
    s0.throttle = 0.8;  // thrust live from tick one (no slew ramp needed)
    sim::Inputs in{};
    in.pitch = 0.25f;
    in.roll = -0.15f;
    in.throttle = 0.8f;
    const sim::SimState a = sim::step(s0, in, ap, &kEnv, ap.sim_dt);
    const sim::SimState b = sim::step(s0, in, ap, nullptr, ap.sim_dt);
    // Fixture-no-op guards: the taper is genuinely engaged and the force
    // terms are genuinely live (airspeed + throttle nonzero).
    const double g_band = sim::g_at(6500.0, &kEnv, ap);
    REQUIRE(g_band < ap.g - 1.0);
    REQUIRE(glm::length(s0.velocity) > 0.0);
    REQUIRE(ap.T_max > 0.0);
    // Config-relative disarm guard (AT-15 class, review rd2): the thrust-leak
    // arm of this leg needs a live atmosphere at 6500 m — an atm retune that
    // kills f_atm here must fail loudly, not silently gut the coverage.
    REQUIRE(sim::atm_frac(6500.0, ap) > 0.1);
    // dv = (g_at - g) * dt * gravity_dir, component-exact: any leak into a
    // non-gravity force term forks this equality.
    const glm::dvec3 expected =
        (g_band - ap.g) * ap.sim_dt * sim::gravity_dir(pos);
    const glm::dvec3 dv = a.velocity - b.velocity;
    CHECK(dv.x == Catch::Approx(expected.x).margin(1e-12));
    CHECK(dv.y == Catch::Approx(expected.y).margin(1e-12));
    CHECK(dv.z == Catch::Approx(expected.z).margin(1e-12));
    // Torque path sees no gravity: rotation must be bit-identical.
    REQUIRE(a.orientation == b.orientation);
    REQUIRE(a.angular_vel == b.angular_vel);
    REQUIRE(a.throttle == b.throttle);
}

TEST_CASE(
    "Environment::any_live: every field singly arms the app env gate "
    "(P2-1 revert tripwire)") {
    // The main.cpp env_ptr gate reads THIS predicate; ctest cannot run the
    // app, so pin the predicate a ground-only-gate revert would have to
    // route around. The pointees are never dereferenced — nullness is the
    // whole contract — so opaque non-null pointers are honest here.
    sim::Environment e;
    CHECK(!e.any_live());
    static int dummy = 0;
    e.grav = &kField;
    CHECK(e.any_live());
    e.grav = nullptr;
    e.ground = reinterpret_cast<const world::HeightField*>(&dummy);
    CHECK(e.any_live());
    e.ground = nullptr;
    e.atm = reinterpret_cast<const sim::AtmosphereField*>(&dummy);
    CHECK(e.any_live());
    e.atm = nullptr;
    e.tunnels = reinterpret_cast<const world::TunnelNet*>(&dummy);
    CHECK(e.any_live());
    e.tunnels = nullptr;
    e.obstacles = reinterpret_cast<const world::BuildingColliders*>(&dummy);
    CHECK(e.any_live());
    e.obstacles = nullptr;
    CHECK(!e.any_live());
}

TEST_CASE(
    "load_game: [gravity] loads strictly; the bands reject the forks "
    "they exist for") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");
    const cfg::GameParams g =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", ap);
    // Shipped table: the field exists but activation is CHAD'S (his HALT).
    // 5000/1500 (the 3.B design pair) -> 4000/405.5 per Chad's R5-FLY-2
    // ruling: escape at ~100 m/s from 4 km.
    CHECK(g.gravity.enabled == false);
    CHECK(g.gravity.h_g0_m == Catch::Approx(4000.0));
    CHECK(g.gravity.sigma_g_m == Catch::Approx(405.5));
    // h_g0 floor: a taper reaching into the terrain (~550 m max) would fork
    // grounded p.g reads vs flight g_at.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "h_g0_m = 4000.0", "h_g0_m = 500.0"),
                   "h0low"),
        ap));
    // sigma floor: a one-tick-jumpable band guts the designed knife edge.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "sigma_g_m = 405.5", "sigma_g_m = 10.0"),
                   "siglow"),
        ap));
    // Ceilings too (adversarial-review P3: deleting an upper bound in the
    // loader shipped green with floor-only reject legs).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "h_g0_m = 4000.0", "h_g0_m = 25000.0"),
                   "h0high"),
        ap));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "sigma_g_m = 405.5", "sigma_g_m = 20000.0"),
                   "sighigh"),
        ap));
    // Strict: a missing key throws (never a silent default).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "h_g0_m = 4000.0", ""), "h0gone"), ap));
}

TEST_CASE(
    "load_game: [tunnel] T11 arena bands + the SHOULDER tripwire reject "
    "their forks") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");
    // Shipped T11 table loads.
    const cfg::GameParams g =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", ap);
    CHECK(g.tunnel.arena_a_m == Catch::Approx(4200.0));  // T13 centering
    CHECK(g.tunnel.arena_c_m == Catch::Approx(2600.0));
    CHECK(g.tunnel.arena_depth_m == Catch::Approx(1500.0));

    // Oblate: arena_a < arena_c throws (the escape ruling).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "arena_c_m       = 2600.0",
                               "arena_c_m       = 9000.0"),
                   "notoblate"),
        ap));

    // THE SHOULDER TRIPWIRE (mutation guard: shoulder -> apex-only). A WIDE +
    // SHALLOW arena (arena_a 9000, arena_depth 800) puts the oblate SHOULDER
    // shallowest point above the min-cover bound (rho_max ~ 15090 > R -
    // min_cover - 100 = 14840) while the APEX (rho ~ 14200) still clears it.
    // The real shoulder formula REJECTS this config; an apex-only mutant would
    // ACCEPT it (the shoulder pokes through the terrain unnoticed). The two
    // edits must both apply.
    {
        std::string s = replace_all(src, "arena_a_m       = 4200.0",
                                    "arena_a_m       = 9000.0");
        s = replace_all(s, "arena_depth_m   = 1500.0",
                        "arena_depth_m   = 800.0");
        CHECK_THROWS(cfg::load_game_toml(write_temp(s, "shoulder"), ap));
    }

    // T12: the shaft is gone (core sealed) — the shaft-radius band check with
    // it. A missing arena key still throws.

    // Strict: a missing arena key throws.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "arena_a_m       = 4200.0", ""),
                   "arenagone"),
        ap));
}

TEST_CASE(
    "load_game: [atmosphere] loads strictly; the bands reject the forks "
    "they exist for (R6)") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");
    const cfg::GameParams g =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", ap);
    // Shipped table: 2026-07-26 Chad LIFTED the R6 fly HALT ("flip it") — the
    // AtmosphereField ships LIVE, so the conquest air war is physical (with it
    // false, env.atm was null and every bubble was ignored by atm_frac_at).
    CHECK(g.atmosphere.enabled == true);
    CHECK(g.atmosphere.deck_agl_m == Catch::Approx(120.0));
    CHECK(g.atmosphere.bubble_radius_m == Catch::Approx(6000.0));
    CHECK(g.atmosphere.bubble_ceiling_m == Catch::Approx(4000.0));
    CHECK(g.atmosphere.bubble_edge_soft_m == Catch::Approx(1200.0));
    // S-domeround (docs/airdome_round_spec.md §1.3): the shipped dial +
    // the loader's own std::lgamma DERIVATION of H, cross-checked against
    // an INDEPENDENTLY computed I(n) here (not calling config/load_game.cpp's
    // internals) -- a real fork detector on the loader's own arithmetic, not
    // just the abstract volume-preservation math (test_air_field.cpp's leg 4
    // proves the FORMULA; this proves the LOADER actually applies it to the
    // shipped bubble_ceiling_m/bubble_dome_exponent).
    CHECK(g.atmosphere.bubble_dome_exponent == Catch::Approx(3.0));
    CHECK(g.atmosphere.bubble_ceiling_volume_preserve == true);
    {
        const double n = g.atmosphere.bubble_dome_exponent;
        const double i_n = std::exp(std::lgamma(1.0 + 1.0 / n) +
                                    std::lgamma(1.0 + 2.0 / n) -
                                    std::lgamma(1.0 + 3.0 / n));
        const double expected_h = g.atmosphere.bubble_ceiling_m / i_n;
        REQUIRE(expected_h > g.atmosphere.bubble_ceiling_m);  // anti-no-op:
            // a dome genuinely needs a TALLER centre height than the
            // cylinder it replaces (I(n) < 1 for any finite n)
        CHECK(g.atmosphere.bubble_dome_h_m ==
              Catch::Approx(expected_h).epsilon(1e-9));
    }
    // Edge-softness FLOOR: a sub-km edge is a <1 s density cliff = a scripted
    // wall (Fable-BEFORE §5), the exact thing the km-scale band exists to
    // forbid.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_edge_soft_m = 1200.0",
                               "bubble_edge_soft_m = 50.0"),
                   "edgelow"),
        ap));
    // Radius floor: a sub-km bubble has no flyable interior.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_radius_m = 6000.0",
                               "bubble_radius_m = 500.0"),
                   "radlow"),
        ap));
    // Ceiling must clear the deck (else there is no breathable air to fly in).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_ceiling_m = 4000.0",
                               "bubble_ceiling_m = 100.0"),
                   "ceillow"),
        ap));
    // Deck band: a deck reaching to 3 km is not a breathable-floor tune.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(
            replace_all(src, "deck_agl_m = 120.0", "deck_agl_m = 3000.0"),
            "deckhigh"),
        ap));
    // Strict: a missing key throws (never a silent default).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_edge_soft_m = 1200.0", ""),
                   "edgegone"),
        ap));
    // S-domeround: the exponent range floor/ceiling (spec §3) and the two
    // new keys' own missing-key strictness.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_dome_exponent = 3.0",
                               "bubble_dome_exponent = 1.0"),
                   "domeexplow"),
        ap));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_dome_exponent = 3.0",
                               "bubble_dome_exponent = 64.0"),
                   "domeexphigh"),
        ap));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_dome_exponent = 3.0", ""),
                   "domeexpgone"),
        ap));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_ceiling_volume_preserve = true", ""),
                   "volpresgone"),
        ap));
}

TEST_CASE(
    "load_game: [atmosphere] bubble_ceiling_volume_preserve=false reads H "
    "literally (H == ceiling_m, no I(n) division)") {
    const sim::AircraftParams ap =
        cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");
    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");
    const cfg::GameParams g = cfg::load_game_toml(
        write_temp(replace_all(src, "bubble_ceiling_volume_preserve = true",
                               "bubble_ceiling_volume_preserve = false"),
                   "volpresfalse"),
        ap);
    CHECK(g.atmosphere.bubble_ceiling_volume_preserve == false);
    // EXACT, not Approx: I(n)==1 structurally when volume_preserve is off,
    // so H is the literal ceiling with no division rounding at all.
    CHECK(g.atmosphere.bubble_dome_h_m == g.atmosphere.bubble_ceiling_m);
}
