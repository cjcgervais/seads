// Pure weather variable (docs/little_planet_plan.md Stage 3 — the weather
// mechanism). The gate pins the Fable-vetted distribution/purity/smoothness
// HEADLESSLY (the module is raylib-free, a function of t_cel only). Each leg is
// shaped to catch a break; mutation targets noted per leg.
//
// The DISTRIBUTION test uses the DEFAULT WeatherParams (== the shipped design
// in config/world.toml [weather]) and pins a PROPERTY of the algorithm, so a
// config gate-retune (which is MEANT to move the mix) does not silently disarm
// it — the config VALUES are pinned separately in test_load_world. Bounds are
// Fable's (±8/±7/±4 pp), wide enough to survive a period retune (periods set
// tempo, not mix), tight enough to kill the single-sine and no-gate mutants.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "render/weather.h"

using render::weather_cell;
using render::weather_haze;
using render::WeatherAnchor;
using render::WeatherCellParams;
using render::WeatherParams;

namespace {
// Classify a haze amount into the three weather bands (Fable's definitions).
enum Band { kClear, kLight, kOvercast };
Band band_of(double h) {
    if (h < 0.05) return kClear;
    if (h < 0.65) return kLight;
    return kOvercast;
}

// A Fibonacci lattice of unit directions covering the sphere evenly — a
// PROBE grid for the localized field, generated INDEPENDENTLY of the field's
// own cell lattice (no coupling to the impl's constants). n large enough that
// the nearest probe to any cell center is well inside the inner cone.
std::vector<glm::dvec3> probe_dirs(int n) {
    std::vector<glm::dvec3> d;
    d.reserve(n);
    const double ga = 2.399963229728653;  // golden angle
    for (int i = 0; i < n; ++i) {
        const double z = 1.0 - 2.0 * (i + 0.5) / n;
        const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double phi = i * ga + 0.31;  // +offset so probes never ALIAS
        d.emplace_back(r * std::cos(phi), r * std::sin(phi), z);  // onto centers
    }
    return d;
}
}  // namespace

TEST_CASE("weather: haze stays in [0,1] over a long sweep") {
    const WeatherParams p;
    for (int i = 0; i <= 200000; ++i) {
        const double h = weather_haze(static_cast<double>(i), p);
        REQUIRE(h >= 0.0);
        REQUIRE(h <= 1.0);
    }
}

TEST_CASE("weather: the clear/haze/overcast distribution is ~58/34/7") {
    // S-bubbleweather (Chad's fly, 2026-08-09: weather "more frequently"):
    // gate_lo moved 0.12 -> -0.05, which is MEANT to move this mix. The design
    // target is therefore re-derived, NOT re-recorded to whatever the code
    // emits: lowering gate_lo by 0.17 on a bell-shaped signal shifts mass out
    // of the hard-zero plateau into the light band, which is exactly what the
    // measured 58/34/7 shows. Clear must still DOMINATE (it is still the most
    // common state) and overcast must still be RARE — those two properties are
    // the ones worth pinning, and both survive the retune.
    //
    // The single-sine tripwire is re-derived for the new gate rather than
    // carried over: overcast needs the gated signal >= 0.65, i.e. raw
    // s >= -0.05 + smoothstep^-1(0.65) * 1.15 ~ 0.636. A unit-amplitude single
    // sine sits above that (pi - 2 asin 0.636) / 2pi ~ 28% of the time, and the
    // 0.5-amplitude single sine never reaches it at all (fo -> 0). So the
    // margin band below catches the single-sine mutant from BOTH sides, and
    // the explicit ceiling stays as the named guard.
    // Sample 0..200000 s @ 1 s — >130 periods of the slowest sine, enough for
    // the time-average to converge (Fable's recipe). Mutation kills:
    //  - single sine (drop the sum to one term): overcast jumps to ~14% (the
    //    arcsine distribution piles mass at the extremes) -> fails the overcast
    //    upper bound.
    //  - no gate (return clamp(s,0,1) or a plain max(0,s-lo)/·): clear
    //  collapses
    //    below the lower bound (the hard-zero plateau is what makes 70% clear).
    const WeatherParams p;
    const int N = 200001;
    long clear = 0, light = 0, overcast = 0;
    for (int i = 0; i < N; ++i) {
        switch (band_of(weather_haze(static_cast<double>(i), p))) {
            case kClear:
                ++clear;
                break;
            case kLight:
                ++light;
                break;
            case kOvercast:
                ++overcast;
                break;
        }
    }
    const double fc = static_cast<double>(clear) / N;
    const double fl = static_cast<double>(light) / N;
    const double fo = static_cast<double>(overcast) / N;
    CHECK(fc == Catch::Approx(0.58).margin(0.08));   // clear still DOMINATES
    CHECK(fl == Catch::Approx(0.34).margin(0.07));   // light haze often
    CHECK(fo == Catch::Approx(0.075).margin(0.04));  // overcast still RARE
    CHECK(fo < 0.12);  // the single-sine tripwire (unit amplitude => ~28%)
    CHECK(fc > fl);    // clear remains the most common state
}

TEST_CASE("weather: spawn (t=0) is clear by construction") {
    // The default phases put s(0) below gate_lo, so the smoothstep gate returns
    // EXACTLY 0 — a clean clear spawn (the approved current look). Mutation: a
    // phase change that lifts s(0) above gate_lo breaks the exact-zero.
    const WeatherParams p;
    CHECK(weather_haze(0.0, p) == 0.0);
}

TEST_CASE(
    "weather: deterministic and pure (no state, frame-rate independent)") {
    const WeatherParams p;
    const double t = 12345.678;
    const double a = weather_haze(t, p);
    // Bit-exact on repeat.
    CHECK(weather_haze(t, p) == a);
    // Independent of any prior calls (guards against someone turning this into
    // a stateful incremental filter — which would also break frame-rate
    // independence, the AT-9 requirement).
    for (int i = 0; i < 1000; ++i)
        weather_haze(static_cast<double>(i) * 7.3, p);
    CHECK(weather_haze(t, p) == a);
    // Frame-rate independence (the AT-9 requirement): the caller passes t_cel =
    // tick_count * sim_dt (an EXACT multiply, not an accumulated sum), so the
    // same tick at any fps yields the same t_cel and thus the same haze. Since
    // this function reads ONLY t_cel, that reduces to "same argument => same
    // result" — pinned above. Here confirm it holds at a representative tick.
    const long long tick = 720;         // tick 720
    const double sim_dt = 1.0 / 120.0;  // params.sim_dt
    const double t_cel = static_cast<double>(tick) * sim_dt;
    CHECK(weather_haze(t_cel, p) == weather_haze(t_cel, p));
}

TEST_CASE("weather: slew is bounded AND non-trivial (C1 + tempo tripwire)") {
    // Both bounds are DERIVED FROM PARAMS (not a hard-coded number), so a legit
    // period/gate retune moves the guard with the mechanism (the AT-15
    // calibrated-constant trap) instead of false-failing.
    //
    // Analytic sup of |d haze/dt|: |smoothstep'(u)| <= 1.5, |ds/dt| <=
    // 2*pi*sum(wi/Ti), and the gate divides by (hi-lo). MVT => a 1 s finite
    // step can't exceed this sup.
    const WeatherParams p;
    constexpr double kTwoPi = 6.283185307179586;
    const double slew_sup = 1.5 * kTwoPi *
                            (p.weight1 / p.period1_s + p.weight2 / p.period2_s +
                             p.weight3 / p.period3_s) /
                            (p.gate_hi - p.gate_lo);
    double max_delta = 0.0;
    double prev = weather_haze(0.0, p);
    for (int i = 1; i <= 200000; ++i) {
        const double h = weather_haze(static_cast<double>(i), p);
        max_delta = std::max(max_delta, std::abs(h - prev));  // dt = 1 s
        prev = h;
    }
    // UPPER (C1 — no pops): a discontinuity (e.g. an un-clamped divide, or a
    // jump gate) would spike a step ABOVE the analytic sup. 1.02 is float
    // slack.
    CHECK(max_delta <= slew_sup * 1.02);
    // LOWER (tempo tripwire, P1-1): the FAST front (period3, ~3.3 min — Chad's
    // ruled dynamic-arena tempo) must actually contribute. Dropping the third
    // sine in weather.cpp halves the real slew (~0.46 of slew_sup) while the
    // params-derived sup is unchanged; the real mechanism reaches ~0.88 of it.
    // 0.55 separates cleanly and survives a period retune.
    CHECK(max_delta >= slew_sup * 0.55);
}

// ---------------------------------------------------------------------------
// LOCALIZED WEATHER FIELD (W2 — render::weather_cell). Pins the invariants the
// app-binary-blind gate cannot otherwise reach: budget-0 => clear everywhere,
// bounded, ACTUALLY localized (a patchwork, not a global scalar), and that a
// cell ACTIVATES to local overcast (the P0-1 derived-from-budget fix, NOT the
// multiplied double-gate). Uses the DEFAULT params (== config/world.toml
// [weather_cell]); the config VALUES are pinned separately in test_load_world.

TEST_CASE("weather_cell: clear budget => field == 0 EVERYWHERE") {
    // The load-bearing invariant: a cell can only TAKE from the budget, never
    // add. weather_haze(0) == 0 by construction (spawn-clear), so the whole
    // field must be exactly 0 at spawn AND wherever the budget is clear. Kills a
    // mutant that adds a constant floor or reads a direct time term.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto probes = probe_dirs(2000);
    // Spawn (t=0) is clear by construction.
    for (const auto& d : probes) CHECK(weather_cell(d, 0.0, wp, cp) == 0.0);
    // And at every t where the budget itself is 0, the field is 0 everywhere.
    for (int i = 0; i < 4000; ++i) {
        const double t = static_cast<double>(i);
        if (weather_haze(t, wp) == 0.0) {
            for (const auto& d : probes)
                REQUIRE(weather_cell(d, t, wp, cp) == 0.0);
            break;  // one clear tick suffices for the everywhere-0 property
        }
    }
}

TEST_CASE("weather_cell: field stays in [0,1] over a dir x time sweep") {
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto probes = probe_dirs(400);
    for (int i = 0; i <= 20000; i += 7) {
        const double t = static_cast<double>(i);
        for (const auto& d : probes) {
            const double h = weather_cell(d, t, wp, cp);
            REQUIRE(h >= 0.0);
            REQUIRE(h <= 1.0);
        }
    }
}

TEST_CASE("weather_cell: weather is LOCALIZED (a patchwork, not a global haze)") {
    // The microsystem requirement (Chad's ruling): under an active budget, SOME
    // directions haze while OTHERS stay clear — the field must NOT be a single
    // global scalar. Scan t for a mid budget, then over the probe grid require a
    // large spread (a hazed cell) AND a still-clear region.
    //  - Kills "return budget everywhere" (spread would be ~0 — uniform).
    //  - The high-max leg ALSO kills the multiplied double-gate (P0-1): with
    //    activation DERIVED from the budget, a fully-activated cell CENTER
    //    reaches local overcast (~1) even at a MODERATE budget ~0.5; a
    //    budget-MULTIPLIED field would cap that cell at ~0.5.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto probes = probe_dirs(4000);  // dense: nearest probe to any center < ~2 deg
    bool tested = false;
    for (int i = 0; i < 200000; ++i) {
        const double t = static_cast<double>(i);
        const double budget = weather_haze(t, wp);
        if (budget < 0.45 || budget > 0.60) continue;  // a moderate budget window
        double lo = 1.0, hi = 0.0;
        for (const auto& d : probes) {
            const double h = weather_cell(d, t, wp, cp);
            lo = std::min(lo, h);
            hi = std::max(hi, h);
        }
        // A cell reaches local overcast (activation, not multiply) at budget~0.5.
        CHECK(hi > 0.90);
        // ...while clear regions remain (localized, not a global scalar).
        CHECK(lo < 0.10);
        tested = true;
        break;
    }
    REQUIRE(tested);  // the moderate-budget window must actually occur
}

TEST_CASE("weather_cell: deterministic and pure (no state, no clock)") {
    const WeatherParams wp;
    const WeatherCellParams cp;
    const glm::dvec3 d = glm::normalize(glm::dvec3(0.3, -0.7, 0.5));
    const double t = 8321.5;
    const double a = weather_cell(d, t, wp, cp);
    CHECK(weather_cell(d, t, wp, cp) == a);  // bit-exact on repeat
    for (int i = 0; i < 500; ++i)
        weather_cell(probe_dirs(4)[i % 4], i * 3.1, wp, cp);  // churn
    CHECK(weather_cell(d, t, wp, cp) == a);  // independent of prior calls
}

TEST_CASE("weather_cell: NO WIND - the field is a pure fn of (dir, budget)") {
    // Chad's no-wind ruling: cells are FIXED and weather_cell reads t ONLY
    // through the weather_haze budget, so TWO well-separated times sharing a
    // budget value yield an IDENTICAL field at every direction (no advection, no
    // direct time term). A wind mutant (centers rotating with t) would drift the
    // cells between t1 and t2 and spike the per-direction difference. Find two
    // active times with near-equal budgets >500 s apart, require the field to
    // match at every probe within the tiny slack the budget mismatch allows.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto probes = probe_dirs(4000);
    // Bucket active times by budget (1e-4 bins). First collision >500 s apart is
    // our (t1,t2) with |budget1 - budget2| < 1e-4.
    std::vector<double> first_t(11000, -1.0);  // bins over budget in (0,1.1)
    double t1 = -1.0, t2 = -1.0;
    for (int i = 0; i < 200000 && t2 < 0.0; ++i) {
        const double t = static_cast<double>(i);
        const double b = weather_haze(t, wp);
        if (b < 0.40 || b > 0.95) continue;  // an ACTIVE, non-saturated budget
        const int bin = static_cast<int>(b * 1e4);
        if (bin < 0 || bin >= 11000) continue;
        if (first_t[bin] < 0.0) {
            first_t[bin] = t;
        } else if (t - first_t[bin] > 500.0) {
            t1 = first_t[bin];
            t2 = t;
        }
    }
    REQUIRE(t1 >= 0.0);
    REQUIRE(t2 > t1 + 500.0);
    // The field's Lipschitz constant in budget is bounded (each cell's
    // smoothstep' <= 1.5/env_width, N cells), so |budget diff| < 1e-4 permits a
    // field diff of only ~O(1e-2). A wind term would blow past this at a cell
    // edge. 0.05 separates cleanly.
    double max_diff = 0.0;
    for (const auto& d : probes)
        max_diff = std::max(max_diff, std::abs(weather_cell(d, t1, wp, cp) -
                                               weather_cell(d, t2, wp, cp)));
    CHECK(max_diff < 0.05);
}

// ---------------------------------------------------------------------------
// S-bubbleweather (Chad's fly, 2026-08-09: "add more weather as micro events
// that happen more frequently within the bubble"). The ANCHORED lattice packs
// cells INSIDE the domes instead of over the whole sphere, so the storm budget
// stops being spent on cells in vacuum that the air gate then zeroes.

namespace {
// Two well-separated anchor caps, neither axis-aligned (so no world axis
// coincides with an anchor and a fixed-axis placement bug cannot hide).
std::vector<WeatherAnchor> test_anchors() {
    WeatherAnchor a;
    a.center_dir = glm::normalize(glm::dvec3(0.3, 0.9, -0.2));
    a.radius_rad = 0.4;  // 6 km at R = 15 km
    WeatherAnchor b;
    b.center_dir = glm::normalize(glm::dvec3(-0.7, -0.5, 0.4));
    b.radius_rad = 0.5333;  // 8 km
    return {a, b};
}
}  // namespace

TEST_CASE("weather_cell anchored: zero anchors delegates BIT-IDENTICALLY") {
    // The fallback guarantee: a world with no bubbles still gets weather, and
    // every existing caller/test is untouched. Bit-exact, not approx.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto probes = probe_dirs(600);
    for (double t : {0.0, 311.0, 1234.5, 5000.0, 20000.0}) {
        for (const auto& d : probes) {
            CHECK(weather_cell(d, t, wp, cp, nullptr, 0) ==
                  weather_cell(d, t, wp, cp));
            const WeatherAnchor unused{};
            CHECK(weather_cell(d, t, wp, cp, &unused, 0) ==
                  weather_cell(d, t, wp, cp));
        }
    }
}

TEST_CASE("weather_cell anchored: clear budget => field == 0 EVERYWHERE") {
    // The load-bearing invariant is INHERITED, not re-derived: a cell can only
    // take from the budget. Kills a mutant that adds a floor in the anchored
    // branch, which the global-form version of this test cannot see.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto anchors = test_anchors();
    const auto probes = probe_dirs(1500);
    for (const auto& d : probes)
        CHECK(weather_cell(d, 0.0, wp, cp, anchors.data(),
                           static_cast<int>(anchors.size())) == 0.0);
    for (int i = 0; i < 4000; ++i) {
        const double t = static_cast<double>(i);
        if (weather_haze(t, wp) == 0.0) {
            for (const auto& d : probes)
                REQUIRE(weather_cell(d, t, wp, cp, anchors.data(),
                                     static_cast<int>(anchors.size())) == 0.0);
            break;
        }
    }
}

TEST_CASE("weather_cell anchored: the field lives INSIDE the anchors") {
    // The mechanism itself: under an active budget, weather appears within an
    // anchor cap and is EXACTLY zero far outside every cap. This is what makes
    // the budget buy weather in the domes rather than in vacuum. Mutating the
    // placement back to the global lattice fails the far-field leg.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto anchors = test_anchors();
    const int na = static_cast<int>(anchors.size());
    // Find a strongly active budget.
    double t_active = -1.0;
    for (int i = 0; i < 200000; ++i) {
        if (weather_haze(static_cast<double>(i), wp) > 0.80) {
            t_active = static_cast<double>(i);
            break;
        }
    }
    REQUIRE(t_active >= 0.0);

    // Inside: some direction within an anchor cap must be meaningfully hazed.
    double max_in = 0.0;
    for (const auto& d : probe_dirs(20000)) {
        bool inside = false;
        for (const auto& an : anchors)
            if (std::acos(std::clamp(glm::dot(d, an.center_dir), -1.0, 1.0)) <
                an.radius_rad)
                inside = true;
        if (inside)
            max_in = std::max(
                max_in, weather_cell(d, t_active, wp, cp, anchors.data(), na));
    }
    INFO("max field inside an anchor = " << max_in);
    CHECK(max_in > 0.5);

    // Outside: beyond every cap plus the squall's own outer radius, EXACTLY 0.
    const double outer_rad = cp.bubble_outer_deg * 0.017453292519943295;
    int checked_outside = 0;
    for (const auto& d : probe_dirs(20000)) {
        bool far = true;
        for (const auto& an : anchors)
            if (std::acos(std::clamp(glm::dot(d, an.center_dir), -1.0, 1.0)) <
                an.radius_rad * cp.bubble_fill_frac + outer_rad + 1e-9)
                far = false;
        if (!far) continue;
        ++checked_outside;
        REQUIRE(weather_cell(d, t_active, wp, cp, anchors.data(), na) == 0.0);
    }
    // Fixture-no-op guard: the outside region must actually be sampled.
    REQUIRE(checked_outside > 1000);
}

TEST_CASE("weather_cell anchored: MORE weather in a dome than the global form") {
    // Chad's actual ask, as a measurement rather than an assertion of intent:
    // over a sweep of times, sample only directions inside the domes and
    // compare mean local haze. The anchored form must deliver materially more,
    // or the mechanism did not do its job.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto anchors = test_anchors();
    const int na = static_cast<int>(anchors.size());
    std::vector<glm::dvec3> in_dome;
    for (const auto& d : probe_dirs(6000)) {
        for (const auto& an : anchors) {
            if (std::acos(std::clamp(glm::dot(d, an.center_dir), -1.0, 1.0)) <
                an.radius_rad) {
                in_dome.push_back(d);
                break;
            }
        }
    }
    REQUIRE(in_dome.size() > 100);
    double sum_anchored = 0.0, sum_global = 0.0;
    int n = 0;
    for (int i = 0; i < 20000; i += 37) {
        const double t = static_cast<double>(i);
        for (const auto& d : in_dome) {
            sum_anchored += weather_cell(d, t, wp, cp, anchors.data(), na);
            sum_global += weather_cell(d, t, wp, cp);
            ++n;
        }
    }
    const double mean_anchored = sum_anchored / n;
    const double mean_global = sum_global / n;
    INFO("mean in-dome haze: anchored = " << mean_anchored
                                          << " global = " << mean_global);
    CHECK(mean_anchored > 2.0 * mean_global);
}

TEST_CASE("weather_cell anchored: NO WIND - centres are fixed in the anchor") {
    // Same no-wind ruling as the global form, re-pinned on the anchored branch
    // (a differential against the global test would be blind to a wind term
    // introduced only here). Two times sharing a budget must agree everywhere.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const auto anchors = test_anchors();
    const int na = static_cast<int>(anchors.size());
    const auto probes = probe_dirs(3000);
    std::vector<double> first_t(11000, -1.0);
    double t1 = -1.0, t2 = -1.0;
    for (int i = 0; i < 200000 && t2 < 0.0; ++i) {
        const double t = static_cast<double>(i);
        const double b = weather_haze(t, wp);
        if (b < 0.40 || b > 0.95) continue;
        const int bin = static_cast<int>(b * 1e4);
        if (bin < 0 || bin >= 11000) continue;
        if (first_t[bin] < 0.0) {
            first_t[bin] = t;
        } else if (t - first_t[bin] > 500.0) {
            t1 = first_t[bin];
            t2 = t;
        }
    }
    REQUIRE(t1 >= 0.0);
    double max_diff = 0.0;
    for (const auto& d : probes)
        max_diff = std::max(
            max_diff, std::abs(weather_cell(d, t1, wp, cp, anchors.data(), na) -
                               weather_cell(d, t2, wp, cp, anchors.data(), na)));
    CHECK(max_diff < 0.05);
}

TEST_CASE("weather_cell anchored: a dome's squalls are locked to ITS frame") {
    // The pattern must not re-shuffle as a dome grows or shrinks in the
    // conquest loop — the centres are placed by ANGLE within the cap, so
    // scaling the radius scales the pattern rather than permuting it. Pins
    // that supplying a tangent_ref (the bubble's own major_axis, as main.cpp
    // does) makes the placement independent of the world-axis fallback.
    const WeatherParams wp;
    const WeatherCellParams cp;
    auto anchors = test_anchors();
    const glm::dvec3 n = glm::normalize(anchors[0].center_dir);
    glm::dvec3 tan_ref = glm::dvec3(0.0, 0.0, 1.0) -
                         n * glm::dot(glm::dvec3(0.0, 0.0, 1.0), n);
    anchors[0].tangent_ref = glm::normalize(tan_ref);
    anchors[1].tangent_ref = anchors[0].tangent_ref;
    // An ACTIVE budget: at a clear budget the field is 0 everywhere and BOTH
    // legs below pass vacuously (the fixture-no-op class — the first cut of
    // this test used a hardcoded t whose budget was 0 and the rotated-gauge
    // leg reported a difference of exactly 0 while proving nothing).
    double t_active = -1.0;
    for (int i = 0; i < 200000; ++i) {
        if (weather_haze(static_cast<double>(i), wp) > 0.60) {
            t_active = static_cast<double>(i);
            break;
        }
    }
    REQUIRE(t_active >= 0.0);
    const auto probes = probe_dirs(500);
    // Deterministic and repeatable with a reference supplied.
    for (const auto& d : probes) {
        const double a = weather_cell(d, t_active, wp, cp, anchors.data(), 2);
        CHECK(weather_cell(d, t_active, wp, cp, anchors.data(), 2) == a);
    }
    // And a DIFFERENT reference genuinely moves the pattern (so the field is
    // not accidentally ignoring tangent_ref — the fixture-no-op class).
    auto rotated = anchors;
    const glm::dvec3 alt = glm::normalize(glm::cross(n, anchors[0].tangent_ref));
    rotated[0].tangent_ref = alt;
    double max_diff = 0.0;
    for (const auto& d : probe_dirs(8000))
        max_diff = std::max(
            max_diff,
            std::abs(weather_cell(d, t_active, wp, cp, anchors.data(), 2) -
                     weather_cell(d, t_active, wp, cp, rotated.data(), 2)));
    INFO("max field change under a rotated placement gauge = " << max_diff);
    CHECK(max_diff > 0.1);
}
