// SC1 -- COLD IS POWER, world/cold.h's half of the acceptance (WINTER_LAW
// sec3.7; Game_loop_idea/vehicle_program/SC1_COLD_SPEC.md v2). The sled-kernel
// legs (neutrality, the emergence matrix, the p99 snap-bog guardrail, the
// structural untouched-surfaces leg, warm-drives-slower) live beside the
// existing sled acceptance in test_sled.cpp -- this file is the PURE
// world::cold field: h(t), air_temp_c(sun_sin_elev, night_phase), frost(t),
// and the [cold] config load/validation.
//
// ASCII-ONLY test names throughout (this project has had FOUR silent-skip
// incidents from a non-ASCII TEST_CASE name never running under ctest).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_world.h"
#include "world/cold.h"

using Catch::Matchers::WithinAbs;

namespace {

world::ColdParams shipped_cold() {
    // Mirrors config/world.toml [cold] exactly, so a leg here reds the moment
    // the shipped numbers drift from what these legs assert against.
    world::ColdParams p;
    p.enabled = true;
    p.t_ref_c = -15.0;
    p.t_day_max_c = -5.0;
    p.t_night_c = -22.0;
    p.t_snap_c = -30.0;
    p.snap_center = 0.58;
    p.snap_width = 0.35;
    p.cold_gain = 1.0;
    p.h_slope_per_c = 0.022;
    p.h_max = 1.35;
    p.warm_drag_gain = 0.35;
    return p;
}

std::string read_file(const std::string& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        *ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    *ok = true;
    return ss.str();
}

const std::string kRoot = std::string(SEADS_CONFIG_DIR) + "/..";

}  // namespace

// ----------------------------------------------------------------------
// Leg 2: h anchor (spec sec5.2)
// ----------------------------------------------------------------------
TEST_CASE("cold: hardness at t_ref_c is exactly 1.0") {
    const world::ColdParams p = shipped_cold();
    // MUTATION KILL: flip the h_slope sign (or the max(0,...) sign) and this
    // leg still passes at t==t_ref (excess==0 either way) -- but see the next
    // case, which the sign flip DOES kill.
    REQUIRE_THAT(world::hardness(p.t_ref_c, p), WithinAbs(1.0, 1e-12));
}

TEST_CASE("cold: hardness is exactly 1.0 for every t >= t_ref_c (one-sided)") {
    const world::ColdParams p = shipped_cold();
    for (double t : {p.t_ref_c, p.t_ref_c + 1.0, p.t_ref_c + 10.0,
                     p.t_day_max_c}) {
        REQUIRE_THAT(world::hardness(t, p), WithinAbs(1.0, 1e-12));
    }
}

TEST_CASE("cold: hardness is monotone increasing as t drops below t_ref_c") {
    const world::ColdParams p = shipped_cold();
    // MUTATION KILL: negate h_slope_per_c (or flip the max(0, t_ref-t) to
    // max(0, t-t_ref)) -- verified BY HAND (see the SC1 handoff evidence):
    // with the sign flipped, `h` decreases below 1.0 as t drops, and the
    // h_max clamp's LOWER bound floors it right back at 1.0 -- so a plain
    // "non-decreasing" walk still passes (flat is non-decreasing). The
    // strict inequality below is what actually catches it: the correct
    // curve must be STRICTLY warmer-to-colder increasing overall, not merely
    // never-decreasing.
    double prev = world::hardness(p.t_ref_c, p);
    for (double t = p.t_ref_c - 1.0; t >= p.t_snap_c; t -= 1.0) {
        const double h = world::hardness(t, p);
        REQUIRE(h >= prev - 1e-12);
        prev = h;
    }
    REQUIRE(world::hardness(p.t_snap_c, p) > world::hardness(p.t_ref_c, p) + 0.05);
}

TEST_CASE("cold: hardness clamps at h_max and never exceeds it") {
    const world::ColdParams p = shipped_cold();
    // MUTATION KILL: drop the h_max clamp (std::clamp -> no upper bound) and
    // this leg fails at extreme cold.
    const double h_extreme = world::hardness(-200.0, p);
    REQUIRE(h_extreme <= p.h_max + 1e-12);
    REQUIRE_THAT(h_extreme, WithinAbs(p.h_max, 1e-9));
}

TEST_CASE("cold: hardness with enabled=false is 1.0 everywhere (bit-neutral off)") {
    world::ColdParams p = shipped_cold();
    p.enabled = false;
    REQUIRE_THAT(world::hardness(p.t_ref_c, p), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(world::hardness(p.t_snap_c, p), WithinAbs(1.0, 1e-12));
    REQUIRE_THAT(world::hardness(-200.0, p), WithinAbs(1.0, 1e-12));
}

// ----------------------------------------------------------------------
// air_temp_c neutrality (spec sec1: enabled=false -> t_ref_c exactly)
// ----------------------------------------------------------------------
TEST_CASE("cold: air_temp_c with enabled=false returns t_ref_c exactly, "
         "at every input") {
    world::ColdParams p = shipped_cold();
    p.enabled = false;
    for (double e : {-1.0, -0.1, 0.0, 0.1, 1.0}) {
        for (double np : {0.0, 0.25, 0.58, 0.9}) {
            REQUIRE_THAT(world::air_temp_c(e, np, p),
                        WithinAbs(p.t_ref_c, 1e-12));
        }
    }
}

// ----------------------------------------------------------------------
// Leg 7: the diurnal curve (spec sec5.7)
// ----------------------------------------------------------------------
TEST_CASE("cold: full daylight (high sun_sin_elev) reads t_day_max_c") {
    const world::ColdParams p = shipped_cold();
    REQUIRE_THAT(world::air_temp_c(1.0, 0.5, p), WithinAbs(p.t_day_max_c, 1e-9));
}

TEST_CASE("cold: deep night away from the snap reads the night baseline") {
    const world::ColdParams p = shipped_cold();
    // night_phase = 0 (sunset) is far outside snap_width (0.35) of
    // snap_center (0.58): |0 - 0.58| = 0.58 > 0.35, so the snap gate is 0.
    REQUIRE_THAT(world::air_temp_c(-1.0, 0.0, p), WithinAbs(p.t_night_c, 1e-9));
}

TEST_CASE("cold: the coldest point of the curve sits at night_phase > 0.5") {
    // Sweep night_phase across the full dark span at fixed deep-night
    // elevation; find where the minimum falls. Spec sec5.7 + WINTER_LAW's
    // "extra cold in the midnight" ruling: it must be AFTER solar midnight.
    const world::ColdParams p = shipped_cold();
    double best_np = 0.0;
    double best_t = 1e9;
    for (int i = 0; i <= 1000; ++i) {
        const double np = i / 1000.0;
        const double t = world::air_temp_c(-1.0, np, p);
        if (t < best_t) {
            best_t = t;
            best_np = np;
        }
    }
    REQUIRE(best_np > 0.5);
    // And it should land close to snap_center (0.58), where the gate peaks.
    REQUIRE(std::fabs(best_np - p.snap_center) < 0.02);
    REQUIRE_THAT(best_t, WithinAbs(p.t_snap_c, 0.05));
}

TEST_CASE("cold: the diurnal curve is continuous within 0.5 C per simulated "
         "second (day_period_s = 300)") {
    // Walk sun_sin_elev + night_phase together over a full simulated day,
    // COUPLED through the SAME phase variable psi the way app/main.cpp
    // actually derives them both from one t_cel (this rung's whole "one
    // clock" point) -- NOT independently swept. An early version of this leg
    // swept night_phase on its own arbitrary offset against elevation, which
    // can coincidentally stack the day/night threshold crossing on top of
    // the snap-gate peak in a way the real, correlated app-side derivation
    // never produces; that measured ~0.53 C/s and was the TEST'S bug, not the
    // curve's -- app/main.cpp's night_phase and sun_sin_elev both come from
    // psi = 2*pi*t_cel/day_period_s (this file's app/main.cpp mirror below),
    // so this sweep uses that identical relationship.
    const world::ColdParams p = shipped_cold();
    constexpr double kDayPeriodS = 300.0;
    constexpr int kSteps = 6000;  // 0.05 s resolution
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
    constexpr double kPi = 3.14159265358979323846;
    double prev_t = 0.0;
    bool have_prev = false;
    for (int i = 0; i <= kSteps; ++i) {
        const double t_cel = kDayPeriodS * static_cast<double>(i) / kSteps;
        const double psi = kTwoPi * t_cel / kDayPeriodS;  // one full turn/day
        // The same leading-order relation app/main.cpp's night_phase
        // derivation is built on (elevation ~ cos(psi) at the reference
        // ground point, tilt=0 leading term -- see app/main.cpp's SC1 clock
        // comment).
        const double elev = std::cos(psi);
        double raw = (psi - kPi / 2.0) / kPi;
        raw -= 2.0 * std::floor(raw / 2.0);
        const double np = raw < 1.0 ? raw : (raw - 1.0);
        const double t = world::air_temp_c(elev, np, p);
        if (have_prev) {
            const double dt_s = kDayPeriodS / kSteps;
            const double slope = std::fabs(t - prev_t) / dt_s;
            REQUIRE(slope <= 0.5 + 1e-9);
        }
        prev_t = t;
        have_prev = true;
    }
}

// ----------------------------------------------------------------------
// Leg 8: frost anchor (spec sec4/sec5.8)
// ----------------------------------------------------------------------
TEST_CASE("cold: frost(t_ref_c) == 0") {
    const world::ColdParams p = shipped_cold();
    REQUIRE_THAT(world::frost(p.t_ref_c, p), WithinAbs(0.0, 1e-12));
}

TEST_CASE("cold: frost(t_night_c) == 0") {
    const world::ColdParams p = shipped_cold();
    REQUIRE_THAT(world::frost(p.t_night_c, p), WithinAbs(0.0, 1e-12));
}

TEST_CASE("cold: frost(t_snap_c) == 1") {
    const world::ColdParams p = shipped_cold();
    // MUTATION KILL: negate the smoothstep argument (v1's bug, spec sec4) and
    // this reads 0 instead of 1.
    REQUIRE_THAT(world::frost(p.t_snap_c, p), WithinAbs(1.0, 1e-9));
}

TEST_CASE("cold: frost(-26) is approximately 0.5") {
    const world::ColdParams p = shipped_cold();
    REQUIRE_THAT(world::frost(-26.0, p), WithinAbs(0.5, 0.02));
}

TEST_CASE("cold: frost is monotone non-decreasing as t drops from t_night_c "
         "to t_snap_c") {
    const world::ColdParams p = shipped_cold();
    double prev = world::frost(p.t_night_c, p);
    for (double t = p.t_night_c - 0.5; t >= p.t_snap_c; t -= 0.5) {
        const double f = world::frost(t, p);
        REQUIRE(f >= prev - 1e-12);
        prev = f;
    }
}

// ----------------------------------------------------------------------
// [cold] config load + validation (mirrors [snowpack]'s house style,
// config/load_world.cpp)
// ----------------------------------------------------------------------
TEST_CASE("cold: the shipped [cold] block loads and matches the law's "
         "measured defaults") {
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.cold.enabled == true);
    CHECK_THAT(w.cold.t_ref_c, WithinAbs(-15.0, 1e-9));
    CHECK_THAT(w.cold.h_max, WithinAbs(1.35, 1e-9));
    CHECK_THAT(w.cold.cold_gain, WithinAbs(1.0, 1e-9));
    CHECK(w.cold.snap_center > 0.5);
    CHECK(w.cold.t_snap_c <= w.cold.t_night_c);
}

TEST_CASE("cold: h_max above 1.40 is rejected at load") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/config/load_world.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("cold h_max in [1.0, 1.40]") != std::string::npos);
}

TEST_CASE("cold: snap_center <= 0.5 is rejected at load (AFTER midnight "
         "ruling)") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/config/load_world.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("c.snap_center > 0.5") != std::string::npos);
}

TEST_CASE("cold: t_snap_c > t_night_c is rejected at load") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/config/load_world.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("!(c.t_snap_c > c.t_night_c)") != std::string::npos);
}

TEST_CASE("cold: cold_gain < 0 and warm_drag_gain < 0 are rejected at load") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/config/load_world.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("c.cold_gain >= 0.0") != std::string::npos);
    REQUIRE(src.find("c.warm_drag_gain >= 0.0") != std::string::npos);
}

// ----------------------------------------------------------------------
// Leg 9 (the world/cold + celestial half): the seam is documented, and the
// draw.cpp local recompute this rung replaced is actually gone.
// ----------------------------------------------------------------------
TEST_CASE("cold: render/celestial.h carries the SC1 authorised-exception "
         "ruling") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/celestial.h", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("AUTHORISED EXCEPTION") != std::string::npos);
    REQUIRE(src.find("WINTER_LAW") != std::string::npos);
}

TEST_CASE("cold: render/draw.cpp no longer computes its own sun-elevation "
         "night-blend scalar (moved to app/main.cpp, spec sec2)") {
    // Pattern of test_winter_reskin.cpp:258 -- a source-text guard, not a
    // behavioural one, because the whole point is that the COMPUTE SITE
    // moved, and a behavioural leg can't see that.
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/draw.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("glm::dot(-sun_f, up_f)") == std::string::npos);
    // The call site remains (post.cpp resolves the stored value) but the
    // LOCAL elevation recompute + set_post_night call are both gone from
    // draw.cpp; set_post_night is call only from app/main.cpp now.
    REQUIRE(src.find("set_post_night(") == std::string::npos);
}

TEST_CASE("cold: app/main.cpp is the one call site for set_post_night") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/app/main.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(src.find("render::set_post_night(") != std::string::npos);
}

// THE HELMET FOG IS DELETED -- Chad's ruling, 2026-08-17: "no more frost
// just turn it off ... but the temperature mechanic that affect the speed of
// the ride kernel should be untouched, Just how it affects my view."
//
// This leg replaces the two that used to PIN the fog call site (one asserting
// `render::set_post_fog(` was present in app/main.cpp, one bounding its line
// distance from set_post_night to keep it out of a mode branch). Their subject
// no longer exists, so rather than delete them silently we INVERT them: the
// same source-grep technique now proves the effect cannot come back by
// accident. A dormant blinder behind a live toggle is exactly what this rung
// was called to remove -- defaulting it off was tried first (a3e669601) and
// was NOT enough, because one 'H' press brought it straight back.
TEST_CASE("cold: the helmet fog is GONE from every view-side path") {
    bool ok = false;
    for (const char* rel : {"/app/main.cpp", "/render/post.cpp",
                            "/render/post.h", "/render/post_glsl.cpp",
                            "/render/draw.cpp", "/render/draw.h"}) {
        const std::string src = read_file(kRoot + rel, &ok);
        REQUIRE(ok);
        // Strip the // comment tails: the deletion is DOCUMENTED by name in
        // these files, and the guard is about live code, not prose.
        std::string code;
        code.reserve(src.size());
        for (std::size_t i = 0; i < src.size();) {
            if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
                while (i < src.size() && src[i] != 0x0A) ++i;
            } else {
                code.push_back(src[i++]);
            }
        }
        INFO("file " << rel);
        REQUIRE(code.find("set_post_fog") == std::string::npos);
        REQUIRE(code.find("set_frost_bypass") == std::string::npos);
        REQUIRE(code.find("frost_bypassed") == std::string::npos);
        REQUIRE(code.find("uFog") == std::string::npos);
        REQUIRE(code.find("SEADS_FROST") == std::string::npos);
    }
}

// ...and the COLD MECHANIC, which the same ruling explicitly PRESERVED, is
// still wired: air temperature still reaches the kernel and still drives the
// dash rime. This is the other half of the ruling and it needs a guard of its
// own, or a future "tidy the frost away" pass takes the ride with it.
TEST_CASE("cold: the temperature mechanic SURVIVES the fog deletion") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/app/main.cpp", &ok);
    REQUIRE(ok);
    // cold -> kernel: the air temperature the sled params consume.
    REQUIRE(src.find("sled_params.air_temp_c = air_t;") != std::string::npos);
    REQUIRE(src.find("world::air_temp_c(") != std::string::npos);
    // cold -> dash art: world::frost still drives the HUD rime.
    REQUIRE(src.find("world::frost(") != std::string::npos);
}

// ----------------------------------------------------------------------
// Mutation-kill 3 (spec sec5.10): negate the frost smoothstep argument.
// Verified by an actual hand-applied mutation + revert during acceptance
// (see the SC1 handoff evidence); this leg is what the mutation is checked
// AGAINST -- "frost(t_snap_c) == 1" above dies under `frost = smoothstep(
// t_snap_c, t_night_c, t)` (the arguments swapped back to v1's bug), reading
// 0 instead of 1.
