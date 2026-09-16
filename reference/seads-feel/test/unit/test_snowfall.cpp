// ATMOSPHERE AS-2 — the SNOWFALL FIELD (render::snowfall_intensity).
//
// Chad, 2026-09-12: "we do get more snow ... Need more localized weather events
// not wind but appearance of snow in a very effective way."
//
// Today precip is gated by the HAZE scalar (weather_cell), whose distribution is
// Chad-signed at ~58/34/7 clear/light/overcast — so most of a winter flight is
// structurally dry, and the only way to get more snow out of that field would be
// to move a signed distribution. AS-2 instead gives the snow its OWN field built
// from the SAME machinery, beside the haze.
//
// WHAT THIS FILE PINS:
//   1. THE OFF VALUE. flurry_level == 0 && snow_floor == 0 => bit-identical to
//      weather_cell. Without this the rung cannot land.
//   2. THE HEAVY BAND IS THE SQUALL'S. snowfall >= squall everywhere, and
//      wherever the squall is heavy the snowfall equals it exactly. Until AS-5
//      the squall WAS weather_cell; AS-5 (Chad: "yes please do so") gives the
//      snow squall its own dials, so "more squall" is now deliberate -- and the
//      squall_own_dials OFF value is still bit-identical to weather_cell.
//   3. MORE SNOW, MEASURED. The snow-time share over a large (t_cel x dir) scan,
//      with generous margins — the number is REPORTED, not asserted tight.
//   4. The seam: pure, deterministic, bounded, C1-ish, NO WIND (a pure function
//      of dir and t_cel, with fixed cell centres).

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/glm.hpp>
#include <utility>
#include <vector>

#include "render/snowfall.h"
#include "render/weather.h"

using render::SnowfallParams;
using render::snowfall_intensity;
using render::weather_cell;
using render::WeatherCellParams;
using render::WeatherParams;

namespace {

// The SHIP values of config/world.toml [precip] (AS-2 keys). Kept here as the
// reference point the measured shares below are measured AT — if the shipped
// dials move, this file's numbers are re-measured, not re-recorded.
SnowfallParams ship_params() {
    SnowfallParams sp;
    sp.flurry_level = 0.35;
    sp.flurry_thresh_lo = 0.00;
    sp.flurry_thresh_hi = 0.65;
    sp.flurry_period_scale = 1.4;
    sp.flurry_inner_deg = 6.0;
    sp.flurry_outer_deg = 14.0;
    sp.flurry_cell_count = 5;
    sp.flurry_gate_lo = -0.95;
    sp.flurry_phase_off = 2.6;
    sp.snow_floor = 0.0;  // SHIPS OFF: the flurry has its own budget now
    // AS-5: the snow squall on its own dials (read from [.as5sweep] and
    // neighbourhood-checked in [.as5ridge]; see the AS-5 section below).
    sp.squall_own_dials = true;
    sp.squall_inner_deg = 8.0;
    sp.squall_outer_deg = 15.0;
    sp.squall_cell_count = 3;
    sp.squall_gate_lo = -0.65;
    sp.squall_thresh_lo = 0.00;
    sp.squall_thresh_hi = 0.35;
    sp.squall_period_scale = 1.0;
    sp.squall_phase_off = 0.0;
    return sp;
}

// The two live air domes, as app/main.cpp builds them from the AirField: a
// centre direction, an angular radius, and the bubble's own major axis as the
// placement gauge. THE GAME ALWAYS RUNS THE ANCHORED OVERLOAD, so every share
// and overlap number below is measured through it -- the global (nullptr
// anchors) form is a fallback nothing flies.
struct Domes {
    render::WeatherAnchor a[2];
};
Domes ship_domes() {
    Domes d;
    d.a[0].center_dir = glm::normalize(glm::dvec3(0.31, 0.82, 0.24));
    d.a[0].radius_rad = 0.40;
    d.a[0].tangent_ref = glm::normalize(glm::dvec3(-0.82, 0.31, 0.11));
    d.a[1].center_dir = glm::normalize(glm::dvec3(-0.52, 0.19, 0.71));
    d.a[1].radius_rad = 0.35;
    d.a[1].tangent_ref = glm::normalize(glm::dvec3(0.19, -0.52, 0.37));
    return d;
}

// A low-discrepancy direction INSIDE an anchor cap -- "in-dome" sampling, which
// is the only time that counts: outside the domes the air gate zeroes the field
// anyway (S-domeround).
glm::dvec3 in_cap_dir(const render::WeatherAnchor& an, int i, int n) {
    const double golden_angle = 2.399963229728653;
    const glm::dvec3 c = glm::normalize(an.center_dir);
    glm::dvec3 seed = an.tangent_ref;
    if (glm::length(seed) < 1e-9) seed = glm::dvec3(0, 0, 1);
    glm::dvec3 e1 = glm::cross(c, seed);
    if (glm::length(e1) < 1e-12) e1 = glm::cross(c, glm::dvec3(1, 0, 0));
    e1 = glm::normalize(e1);
    const glm::dvec3 e2 = glm::cross(c, e1);
    const double rf = std::sqrt((static_cast<double>(i) + 0.5) / n);
    const double th = static_cast<double>(i) * golden_angle;
    const double ang = rf * an.radius_rad;
    const glm::dvec3 t = e1 * std::cos(th) + e2 * std::sin(th);
    return glm::normalize(c * std::cos(ang) + t * std::sin(ang));
}

// The anchored cell centre, re-derived here so the OVERLAP between the squall
// and flurry lattices can be measured from the test rather than asserted. This
// mirrors render/weather.cpp's anchor_cell_center exactly (an equal-area
// sunflower in the cap, theta = j * golden_angle in the tangent_ref basis) --
// if the two ever disagree, the overlap number stops meaning anything, which is
// why the value is PRINTED and not just compared.
glm::dvec3 anchored_centre(const render::WeatherAnchor& an, int j, int m,
                           double fill) {
    render::WeatherAnchor scaled = an;
    scaled.radius_rad *= fill;
    return in_cap_dir(scaled, j, m);
}


// ---------------------------------------------------------------------------
// AS-4 -- EVENT DURATION. Chad flew aa9d78142: "there is not enough durition of
// the weather events, I didnt see any snowing in the tunnel so pass for that."
//
// Duration AT THE EYE has two independent causes, and a dial chosen before they
// are separated is a guess:
//   TEMPO   -- how long a cell stays activated. A STATIONARY eye measures this
//              alone: it cannot leave a cell, so every episode it sees is the
//              budget crossing the cell's threshold and back.
//   SPATIAL -- how long it takes to fly through a lit cell. A MOVING eye adds
//              this to the tempo; the difference between the moving and
//              stationary medians is the spatial term.
// So the table below walks all three and prints both, and the dials are chosen
// from the numbers afterwards.
// ---------------------------------------------------------------------------

constexpr double kPlanetR = 15000.0;  // SEADS sphere radius [m]

struct DurationStats {
    double median_ep = 0.0, p90_ep = 0.0, median_gap = 0.0, share = 0.0;
    int episodes = 0;
};

double pctl(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(
        q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

// Walk an eye IN-DOME and collect the contiguous runs of snow > 0.05 and the
// gaps between them.
//
// THE PATH IS A SMALL CIRCLE inside the cap, not a great circle. The first cut
// of this measurement flew a great circle through the dome centre and produced
// a share of 0.10 with 555 s gaps -- both artefacts of the path leaving the
// 0.40 rad dome after ~85 s and then spending a whole 663 s planetary lap in
// vacuum, where the anchored field is zero by construction. What the pilot
// actually does inside a bubble is turn and stay in it, so the eye orbits at a
// fixed angular radius. `variant` selects the radius (0.25/0.45/0.65 of the
// cap) and the starting bearing, so one lucky orbit through one lucky cell
// cannot set the verdict.
//
// The field is sampled RAW -- no air gate. The gate is a separate geometric
// limit and folding it in would hide the thing being measured; the dome's own
// crossing time is reported beside the table instead.
DurationStats walk_eye(double speed_mps, const SnowfallParams& sp,
                       const WeatherParams& wp, const WeatherCellParams& cp,
                       const render::WeatherAnchor* an, int n_an, int variant,
                       int ens = 0, double dt = 5.0,
                       double span_s = 20000.0, bool squall_term = false,
                       double thr = 0.05) {
    const render::WeatherAnchor& a0 = an[variant % n_an];
    const glm::dvec3 c = glm::normalize(a0.center_dir);
    glm::dvec3 e1 = glm::cross(c, a0.tangent_ref);
    if (glm::length(e1) < 1e-12) e1 = glm::cross(c, glm::dvec3(1, 0, 0));
    e1 = glm::normalize(e1);
    const glm::dvec3 e2 = glm::cross(c, e1);
    // `ens` is an INDEPENDENT DRAW of the same experiment: a different set of
    // starting bearings, a different rotation of the radius assignment, and a
    // different slice of celestial time. Comparing draws at FIXED params is how
    // the measurement's own noise floor gets measured before any dial is read
    // (the probe noise-floor law -- arms that cannot differ must be shown not
    // to differ before a difference is believed).
    static const double kFrac[3] = {0.25, 0.45, 0.65};
    const double rho = kFrac[(variant / n_an + ens) % 3] * a0.radius_rad;
    const double phi0 = 0.7 * variant + 1.37 * ens;
    const double t0 = 613.0 * ens;
    // Angular rate along the small circle: the circle's own radius in metres is
    // R * sin(rho), so a given ground speed turns into this many rad/s of phi.
    const double ring_r = kPlanetR * std::sin(rho);
    const double omega = ring_r > 1.0 ? speed_mps / ring_r : 0.0;

    std::vector<double> eps, gaps;
    double run = 0.0;
    bool wet = false, first = true;
    long on = 0, n = 0;
    for (double t = 0.0; t < span_s; t += dt) {
        const double phi = phi0 + omega * t;
        const double t_cel = t0 + t;
        const glm::dvec3 tv = e1 * std::cos(phi) + e2 * std::sin(phi);
        const glm::dvec3 d =
            glm::normalize(c * std::cos(rho) + tv * std::sin(rho));
        // AS-5: `squall_term` times the heavy band ALONE (the events Chad
        // reads as weather), at its own threshold; otherwise the whole field.
        const double v =
            squall_term ? render::snowfall_squall(d, t_cel, wp, cp, an, n_an, sp)
                        : snowfall_intensity(d, t_cel, wp, cp, an, n_an, sp);
        const bool w = v > thr;
        ++n;
        if (w) ++on;
        if (first) {
            wet = w;
            run = dt;
            first = false;
            continue;
        }
        if (w == wet) {
            run += dt;
        } else {
            (wet ? eps : gaps).push_back(run);
            wet = w;
            run = dt;
        }
    }
    // Both the leading and the trailing run are CENSORED (we started and
    // stopped mid-state), so neither is recorded: the leading one is dropped
    // here, the trailing one is simply never pushed.
    if (!eps.empty() && !gaps.empty()) {
        if (eps.front() <= gaps.front())
            eps.erase(eps.begin());
        else
            gaps.erase(gaps.begin());
    }
    DurationStats st;
    st.median_ep = pctl(eps, 0.5);
    st.p90_ep = pctl(eps, 0.9);
    st.median_gap = pctl(gaps, 0.5);
    st.share = static_cast<double>(on) / n;
    st.episodes = static_cast<int>(eps.size());
    return st;
}

constexpr int kVariantsDefault = 48;

DurationStats walk_avg(double speed_mps, const SnowfallParams& sp,
                       const WeatherParams& wp, const WeatherCellParams& cp,
                       const render::WeatherAnchor* an, int n_an, int ens = 0,
                       int kVariants = kVariantsDefault,
                       bool squall_term = false, double thr = 0.05,
                       double span_s = 20000.0) {
    // The ensemble size is a PARAMETER because it had to be measured, not
    // guessed. Six orbits was tried first and was not enough -- the plane's
    // median swung ~250 s between neighbouring dial values, which is the
    // ensemble talking, not the field. See the noise-floor case below for the
    // number that settled it.
    DurationStats acc;
    for (int i = 0; i < kVariants; ++i) {
        const DurationStats s =
            walk_eye(speed_mps, sp, wp, cp, an, n_an, i, ens, 5.0, span_s,
                     squall_term, thr);
        acc.median_ep += s.median_ep / kVariants;
        acc.p90_ep += s.p90_ep / kVariants;
        acc.median_gap += s.median_gap / kVariants;
        acc.share += s.share / kVariants;
        acc.episodes += s.episodes;
    }
    return acc;
}

void print_duration_row(const char* tag, const DurationStats& s) {
    std::printf(
        "[AS-4 DURATION] %-22s median_ep %6.0f s (%4.1f min)  p90_ep %6.0f s  "
        "median_gap %6.0f s  share %.3f  n=%d\n",
        tag, s.median_ep, s.median_ep / 60.0, s.p90_ep, s.median_gap, s.share,
        s.episodes);
}

// A deterministic low-discrepancy unit direction (no RNG, so the measured
// numbers are reproducible byte-for-byte on any box).
glm::dvec3 sample_dir(int i, int n) {
    const double golden_angle = 2.399963229728653;
    const double z = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / n;
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = static_cast<double>(i) * golden_angle;
    return glm::dvec3(r * std::cos(phi), r * std::sin(phi), z);
}

}  // namespace

TEST_CASE("snowfall: flurry_level 0 + snow_floor 0 is BIT-IDENTICAL to weather_cell") {
    // THE LANDING CONDITION. Every new dial has an OFF value that reproduces
    // today exactly — so a red-team, or Chad, can turn the whole rung off in
    // config and get the shipped field back with no code change.
    const WeatherParams wp;
    const WeatherCellParams cp;
    SnowfallParams sp;
    sp.flurry_level = 0.0;
    sp.snow_floor = 0.0;
    for (int i = 0; i < 256; ++i) {
        const glm::dvec3 d = sample_dir(i, 256);
        for (double t : {0.0, 13.0, 197.0, 1234.5, 5000.0, 54321.0}) {
            const double a = snowfall_intensity(d, t, wp, cp, nullptr, 0, sp);
            const double b = weather_cell(d, t, wp, cp);
            REQUIRE(a == b);  // bit-identical, not Approx
        }
    }
}

TEST_CASE("snowfall: the anchored OFF value is bit-identical too") {
    // The game runs the ANCHORED overload (cells packed inside the live air
    // domes). Its OFF value must hold as well, or the OFF claim is only true in
    // a configuration nobody flies.
    const WeatherParams wp;
    const WeatherCellParams cp;
    SnowfallParams sp;
    sp.flurry_level = 0.0;
    sp.snow_floor = 0.0;
    render::WeatherAnchor an[2];
    an[0].center_dir = glm::normalize(glm::dvec3(0.3, 0.8, 0.2));
    an[0].radius_rad = 0.40;
    an[1].center_dir = glm::normalize(glm::dvec3(-0.5, 0.2, 0.7));
    an[1].radius_rad = 0.35;
    for (int i = 0; i < 256; ++i) {
        const glm::dvec3 d = sample_dir(i, 256);
        for (double t : {0.0, 91.0, 700.0, 4321.0}) {
            REQUIRE(snowfall_intensity(d, t, wp, cp, an, 2, sp) ==
                    weather_cell(d, t, wp, cp, an, 2));
        }
    }
}

TEST_CASE("snowfall: the SQUALL band is untouched (snow never weakens weather)") {
    // snowfall >= squall everywhere (it is a max), and wherever the squall is
    // HEAVY the snowfall IS the squall — the heavy band Chad already flies is
    // exactly the one he flew before. A mutant that blends or sums the terms
    // (pushing a squall past its own value inside a flurry) fails the equality.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    int heavy_seen = 0;
    for (int i = 0; i < 512; ++i) {
        const glm::dvec3 d = sample_dir(i, 512);
        for (int k = 0; k < 40; ++k) {
            const double t = 37.0 * k + 3.0;
            // AS-5: "the squall" is the snow's own squall term now; the
            // invariant is unchanged -- the max never weakens or blends it.
            const double squall =
                render::snowfall_squall(d, t, wp, cp, nullptr, 0, sp);
            const double snow = snowfall_intensity(d, t, wp, cp, nullptr, 0, sp);
            REQUIRE(snow >= squall - 1e-15);
            REQUIRE(snow >= 0.0);
            REQUIRE(snow <= 1.0);
            if (squall >= 0.8) {
                ++heavy_seen;
                REQUIRE(snow == squall);  // the heavy band, exactly
            }
        }
    }
    REQUIRE(heavy_seen > 0);  // the scan actually found heavy weather
}

TEST_CASE("snowfall: MORE SNOW - the measured in-dome share, from EVENTS alone") {
    // The ask, measured THROUGH THE ANCHORED OVERLOAD the game actually runs,
    // at directions INSIDE the domes (outside them the air gate zeroes the
    // field, so out-of-dome samples would only dilute the number).
    //
    // snow_floor is 0 here and 0 in the shipped config: this share is what the
    // flurry+squall EVENTS deliver on their own. That is the whole point of
    // giving the flurry its own budget -- the first cut activated it from the
    // squalls' weather_haze, which is EXACTLY zero on its ~58% clear plateau,
    // so no band could beat ~42% and the measured share was 17%. A constant
    // floor papering over that is the opposite of "localized weather events".
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    REQUIRE(sp.snow_floor == 0.0);  // events only, by construction
    SnowfallParams today;           // the pre-AS-2 field, for the comparison
    today.flurry_level = 0.0;
    today.snow_floor = 0.0;
    today.squall_own_dials = false;  // (struct default; stated for the reader)

    const Domes d = ship_domes();
    const int n_dir = 240, n_t = 500;
    long snow_today = 0, snow_now = 0, heavy_today = 0, heavy_now = 0,
         heavy_squall = 0, total = 0;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < n_dir; ++i) {
            const glm::dvec3 dir = in_cap_dir(d.a[k], i, n_dir);
            for (int q = 0; q < n_t; ++q) {
                const double t = 7.0 * q;  // 0..3493 s, ~17 fast-front periods
                const double a =
                    snowfall_intensity(dir, t, wp, cp, d.a, 2, today);
                const double b = snowfall_intensity(dir, t, wp, cp, d.a, 2, sp);
                if (a > 0.05) ++snow_today;
                if (b > 0.05) ++snow_now;
                if (a >= 0.8) ++heavy_today;
                if (b >= 0.8) ++heavy_now;
                if (render::snowfall_squall(dir, t, wp, cp, d.a, 2, sp) >= 0.8)
                    ++heavy_squall;
                ++total;
            }
        }
    const double f_today = static_cast<double>(snow_today) / total;
    const double f_now = static_cast<double>(snow_now) / total;
    const double h_today = static_cast<double>(heavy_today) / total;
    const double h_now = static_cast<double>(heavy_now) / total;
    std::printf(
        "[AS-2 MEASURED in-dome, floor OFF] snow>0.05: today %.3f -> now %.3f | "
        "heavy>=0.8: today %.4f -> now %.4f  (N=%ld)\n",
        f_today, f_now, h_today, h_now, total);

    // The target the director set: roughly 55-65% of in-dome time, from events.
    // A generous band -- this is a measurement with a design intent behind it,
    // not a golden number, and the dial that moves it (flurry_gate_lo) is one
    // line of config.
    CHECK(f_now > 0.45);
    CHECK(f_now < 0.80);
    CHECK(f_now > 3.0 * f_today);  // and it is a step change, not a nudge
    // THE HEAVY BAND IS THE SQUALL'S, AND ONLY THE SQUALL'S: the flurry is
    // capped at flurry_level (0.35) and can never reach 0.8. Until AS-5 that
    // made the heavy share equal TODAY's; AS-5 deliberately gives the squall
    // its own dials (Chad: "yes please do so"), so the heavy share now GROWS
    // -- and every bit of that growth is the squall term, none of it flurry.
    const double hs = static_cast<double>(heavy_squall) / total;
    std::printf("[AS-5 MEASURED in-dome] heavy>=0.8 from the squall term alone "
                "%.4f\n", hs);
    CHECK(h_now == Catch::Approx(hs).margin(1e-9));
    CHECK(h_now > h_today);
}

TEST_CASE("snowfall: the FLURRY is a different patchwork from the SQUALL") {
    // "Localized events" means the light band must not simply be the squall
    // band turned down — otherwise flying out of a squall turns ALL the snow
    // off at once and nothing new is localized. The flurry runs on a different
    // lattice, so at a budget where both are live there must exist directions
    // where the flurry is on and the squall is not.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    const Domes dm = ship_domes();
    int flurry_only = 0, squall_any = 0;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 512; ++i) {
            const glm::dvec3 d = in_cap_dir(dm.a[k], i, 512);
            for (int q = 0; q < 40; ++q) {
                const double t = 61.0 * q + 11.0;
                const double squall =
                    render::snowfall_squall(d, t, wp, cp, dm.a, 2, sp);
                const double snow =
                    snowfall_intensity(d, t, wp, cp, dm.a, 2, sp);
                if (squall <= 0.01 && snow > 0.05) ++flurry_only;
                if (squall > 0.05) ++squall_any;
            }
        }
    CHECK(squall_any > 0);   // the scan saw squalls
    CHECK(flurry_only > 0);  // and saw snow where there is NO squall
    std::printf("[AS-2 MEASURED in-dome] flurry-only samples %d, squall samples %d\n",
                flurry_only, squall_any);
}

TEST_CASE("snowfall: the flurry LATTICE is not the squall lattice shrunk") {
    // Red-team P1-2. render::anchor_cell_center places cell j at
    // theta = j * golden_angle -- a function of j ALONE -- so changing only the
    // cell COUNT reuses the same bearings. MEASURED before the fix: 0.65 of the
    // flurry centres sat inside a squall's 2.5 deg inner radius.
    //
    // Zero overlap is NOT achievable and would be the wrong target: 16 discs of
    // 2.5 deg inner radius cover about 30% of an 18.3 deg cap, so ~0.30 is the
    // CHANCE baseline for any uncorrelated placement. The claim worth pinning is
    // that the flurry is no more coupled to the squall lattice than chance --
    // 0.543 was ~2x chance, which is what "the same lattice shrunk" looks like.
    // The fix is the RADIAL gauge (the flurry's own bubble_fill_frac, scaled);
    // both numbers are PRINTED so this stays a measurement.
    const WeatherCellParams cp;
    const Domes d = ship_domes();
    // AS-5: the SNOW squall runs its own lattice now (3 cells of 8 deg inner per
    // dome), so the flurry is measured against THAT lattice -- the haze's
    // 16x2.5 deg cells no longer make any snow. A fixed 0.30 chance baseline
    // stopped being true with it: 3 discs of 8 deg cover much more of the
    // flurry's placement cap, so chance is now MEASURED below, not quoted.
    const SnowfallParams shipped = ship_params();
    const int m_sq = shipped.squall_cell_count;  // 3
    // AS-4 REPLACED the count: the flurry is now FIVE big cells (6/14 deg), not
    // 23 small ones, so the overlap is re-measured at the shipped size rather
    // than carried over from AS-2. The bar is unchanged and so is the reason
    // for it -- the flurry must not be the squall lattice in disguise.
    const int m_fl = ship_params().flurry_cell_count;  // 5
    const double fill = cp.bubble_fill_frac;
    const double gauge = 0.85;  // render/snowfall.cpp kFlurryFillGauge
    const double inner_deg = shipped.squall_inner_deg;
    const double inner = std::cos(inner_deg * 0.017453292519943295);

    auto overlap = [&](double fill_fl) {
        int inside = 0, total = 0;
        for (int k = 0; k < 2; ++k)
            for (int j = 0; j < m_fl; ++j) {
                const glm::dvec3 fc = anchored_centre(d.a[k], j, m_fl, fill_fl);
                ++total;
                for (int q = 0; q < m_sq; ++q)
                    if (glm::dot(fc, anchored_centre(d.a[k], q, m_sq, fill)) >
                        inner) {
                        ++inside;
                        break;
                    }
            }
        return static_cast<double>(inside) / total;
    };
    const double naive = overlap(fill);           // the un-gauged placement
    const double gauged = overlap(fill * gauge);  // what ships
    // CHANCE: the share of a uniform point set over the flurry's own placement
    // cap (radius x fill x gauge) that lands inside a squall inner disc. An
    // uncorrelated flurry lattice would score about this.
    int in_c = 0;
    const int n_c = 2000;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < n_c; ++i) {
            const glm::dvec3 p = anchored_centre(d.a[k], i, n_c, fill * gauge);
            for (int q = 0; q < m_sq; ++q)
                if (glm::dot(p, anchored_centre(d.a[k], q, m_sq, fill)) > inner) {
                    ++in_c;
                    break;
                }
        }
    const double chance = static_cast<double>(in_c) / (2 * n_c);
    std::printf(
        "[AS-5 MEASURED] flurry centres inside a squall's %.1f deg inner "
        "radius: un-gauged %.3f -> gauged %.3f (chance %.3f)\n",
        inner_deg, naive, gauged, chance);
    // Only the ABSOLUTE bar is asserted. AS-4 cut the flurry to five big cells
    // per dome, so this measurement now has TEN centres in it -- at that size a
    // ratio between the gauged and un-gauged placements is one or two cells of
    // noise (0.100 vs 0.200 = 1 vs 2 of 10), and asserting on it would be
    // exactly the measurement-tuning this lane rejected when it threw out the
    // angular gauge. What still means something is that the flurry sits at or
    // below CHANCE against the squall lattice, which it does; the un-gauged
    // number is printed beside it so a future reader can see both.
    // AS-5: the bar is now "no worse than chance", with one centre of slack
    // (ten centres => 0.1 per centre). The old absolute 0.35 stood in for a
    // 0.30 chance that no longer holds.
    CHECK(gauged <= chance + 0.1 + 1e-12);
    (void)naive;
}

TEST_CASE("snowfall AS-4: the DURATION table (tempo vs spatial, measured)") {
    // Chad flew aa9d78142: "there is not enough durition of the weather events".
    // THE DIAGNOSIS CAME FIRST. Three eyes on in-dome orbits:
    //   stationary -> TEMPO alone (it cannot leave a cell)
    //   sled 20 m/s, plane 142 m/s -> tempo PLUS the time to fly through a cell
    //
    // MEASURED on the AS-3 field (flurry_inner/outer 0/0 = the bubble values,
    // 23 cells, band [0.00,0.35]):
    //   stationary  median 1252 s (20.9 min)   -- the tempo was never the problem
    //   sled         median  510 s ( 8.5 min)
    //   plane        median   36 s ( 0.6 min), gaps 13 s, share 0.87
    // The plane was snowed on 87% of the time and still felt starved, because at
    // 142 m/s it crossed a 6 deg cell in ~22 s and the field BLINKED. Spatial,
    // not temporal. Hence AS-4's dials are the cell scale, not the periods.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    const Domes d = ship_domes();
    // BEFORE: the AS-3 field, reached through the OFF values of the four AS-4
    // dials (scale 1.0, degrees 0 => the bubble values, count 0 => the AS-2
    // rule). Measured on the SAME twelve orbits as the after rows, so the two
    // halves of the table are comparable -- an earlier six-orbit ensemble was
    // rejected as too noisy to choose dials from.
    SnowfallParams before = sp;
    before.flurry_period_scale = 1.0;
    before.flurry_inner_deg = 0.0;
    before.flurry_outer_deg = 0.0;
    before.flurry_cell_count = 0;
    before.flurry_thresh_hi = 0.35;
    before.flurry_phase_off = 1.7;
    before.squall_own_dials = false;  // AS-5 did not exist on aa9d78142
    std::printf("[AS-4 DURATION] --- BEFORE (aa9d78142, the AS-3 field) ---\n");
    print_duration_row("stationary (tempo)", walk_avg(0.0, before, wp, cp, d.a, 2));
    print_duration_row("sled 20 m/s", walk_avg(20.0, before, wp, cp, d.a, 2));
    print_duration_row("plane 142 m/s", walk_avg(142.0, before, wp, cp, d.a, 2));

    std::printf("[AS-4 DURATION] --- AFTER (shipped AS-4) ---\n");
    const DurationStats st = walk_avg(0.0, sp, wp, cp, d.a, 2);
    const DurationStats sl = walk_avg(20.0, sp, wp, cp, d.a, 2);
    const DurationStats pl = walk_avg(142.0, sp, wp, cp, d.a, 2);
    print_duration_row("stationary (tempo)", st);
    print_duration_row("sled 20 m/s", sl);
    print_duration_row("plane 142 m/s", pl);
    // The geometric ceiling nobody can dial around: on a closed path inside the
    // dome the median episode is about (share x lap time), and the dome is only
    // 12 km across. A 3 min median at plane speed therefore REQUIRES a high
    // in-dome share -- duration and "localized" are in direct tension up there,
    // and the shipped values are the knee, not a free lunch.
    std::printf(
        "[AS-4 DURATION] dome crossing at 142 m/s: %.0f s; at 20 m/s: %.0f s "
        "(cap radius %.2f rad on R=%.0f m)\n",
        2.0 * d.a[0].radius_rad * kPlanetR / 142.0,
        2.0 * d.a[0].radius_rad * kPlanetR / 20.0, d.a[0].radius_rad, kPlanetR);

    // The targets, with generous bands -- these are measurements with a design
    // intent, not golden numbers.
    CHECK(pl.median_ep > 150.0);   // plane: the ask was ~3 min; measured ~3.8
    CHECK(sl.median_ep > 300.0);   // sled:  the ask was ~6 min; measured ~6.6
    CHECK(st.median_ep > 300.0);   // and a parked eye is not worse than a moving one
    // IT MUST STILL BE EVENTS. Real gaps at every speed, or this is permanent
    // snow wearing an event's clothes.
    CHECK(pl.median_gap > 10.0);
    CHECK(sl.median_gap > 30.0);
    CHECK(pl.share < 0.95);
}

TEST_CASE("snowfall AS-4: the in-dome share survives the bigger cells") {
    // AS-2's number, re-measured on the AS-4 field: sampling the cap uniformly
    // (not along an orbit -- an orbit sits in the better-covered middle and
    // reads high), how often is there light snow? The bigger cells must not turn
    // the dome permanently white.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    const Domes d = ship_domes();
    long wet = 0, tot = 0;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 160; ++i) {
            const glm::dvec3 dd = in_cap_dir(d.a[k], i, 160);
            for (int q = 0; q < 300; ++q) {
                if (snowfall_intensity(dd, 11.0 * q, wp, cp, d.a, 2, sp) > 0.05)
                    ++wet;
                ++tot;
            }
        }
    const double share = static_cast<double>(wet) / tot;
    std::printf("[AS-4 MEASURED] uniform in-dome light-snow share %.3f (N=%ld)\n",
                share, tot);
    CHECK(share > 0.45);
    // AS-5 MOVED THIS BAR, deliberately and in the open. AS-4 measured 0.645
    // against an upper bar of 0.75. AS-5's ruling (Chad: "yes please do so" to
    // "cells 3-4x wider and lit more of the time, a squall of 1-2 min in the
    // air, more of them per flight") pushes it to ~0.79: several 1-2 min
    // squalls per 10 min of flying CANNOT fit in a dome that stays dry most of
    // the time -- the geometry forces it ([.as5gate] prints the whole trade;
    // gate -0.25 keeps 0.71 but gives ~2 squalls per 10 min). His words beat
    // the director's older band; the bar now guards "not permanently white".
    CHECK(share < 0.85);
}

TEST_CASE("snowfall AS-4: the squall-to-flurry HANDOVER (is the taper a cut?)") {
    // The director's question, answered as a measurement and NOT built into a
    // mechanism: when a squall dies under the pilot, is the flurry beneath it
    // still on, so he sees a long taper instead of a cut to nothing?
    //
    // No hysteresis is added. This field is pure, and a stateful "hold" would
    // put memory in render/ -- which is exactly what the seam forbids. If the
    // number is bad, the honest fix is coverage, not state.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const SnowfallParams sp = ship_params();
    SnowfallParams squall_only = sp;
    squall_only.flurry_level = 0.0;
    const Domes d = ship_domes();
    int deaths = 0, caught = 0;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < 200; ++i) {
            const glm::dvec3 dd = in_cap_dir(d.a[k], i, 200);
            bool prev_sq = false;
            for (int q = 0; q < 900; ++q) {
                const double t = 5.0 * q;
                const double sq =
                    render::snowfall_squall(dd, t, wp, cp, d.a, 2, sp);
                const bool now_sq = sq > 0.05;
                if (prev_sq && !now_sq) {
                    ++deaths;
                    if (snowfall_intensity(dd, t, wp, cp, d.a, 2, sp) > 0.05)
                        ++caught;
                }
                prev_sq = now_sq;
            }
        }
    const double frac =
        deaths > 0 ? static_cast<double>(caught) / deaths : 0.0;
    std::printf(
        "[AS-4 MEASURED] squall deaths %d, flurry still on underneath %d "
        "(%.3f)\n",
        deaths, caught, frac);
    REQUIRE(deaths > 0);
    // A taper most of the time is the bar, and AS-4 clears it by moving ONE
    // pure dial: at flurry_phase_off 1.7 this measured 0.309 -- a dying squall
    // usually CUT to dry -- and at 2.6 it is 0.86. No state was added. The two
    // budgets share their three periods, so their ups and downs are correlated
    // however the cells are placed; the phase offset is the only lever that
    // separates them, and it is still a pure function of t_cel.
    //
    // AS-5 MOVED THIS BAR, in the open: with the snow squall on its own lower
    // gate (-0.65) its deaths land at different moments of the shared fronts,
    // and the taper measures ~0.56 (0.68 at gate -0.05, [.as5gate]). Still a
    // taper more often than a cut. The squall_phase_off dial is the pure lever
    // if Chad's eye reads the cuts; it was NOT dialled here, so no second
    // number was tuned against this measurement.
    CHECK(frac > 0.5);
}

TEST_CASE("snowfall AS-4 NOISE", "[.noise]") {
    const WeatherParams wp;
    const WeatherCellParams cp;
    const Domes d = ship_domes();
    for (int nv : {6, 12, 24, 48}) {
        double lo = 1e9, hi = -1e9, sum = 0.0;
        double slo = 1e9, shi = -1e9;
        for (int e = 0; e < 4; ++e) {
            const DurationStats pl =
                walk_avg(142.0, ship_params(), wp, cp, d.a, 2, e, nv);
            const DurationStats sl =
                walk_avg(20.0, ship_params(), wp, cp, d.a, 2, e, nv);
            lo = std::min(lo, pl.median_ep);
            hi = std::max(hi, pl.median_ep);
            sum += pl.median_ep / 4.0;
            slo = std::min(slo, sl.median_ep);
            shi = std::max(shi, sl.median_ep);
        }
        std::fflush(stdout);
        std::printf(
            "[NOISE] orbits %3d | plane median mean %5.0f spread %5.0f "
            "(%.0f..%.0f) | sled spread %5.0f\n",
            nv, sum, hi - lo, lo, hi, shi - slo);
    }
}

// ---------------------------------------------------------------------------
// AS-5 -- SNOW-SQUALL ON ITS OWN DIALS. Chad flew 6aa0d79fb: "I just flew it
// and its still no good, I see very few weather events and they barely last
// 15s?" Ruling (via the sentinel, 2026-09-12): "yes please do so" -- give the
// snow squall its own cell size and gate beside the signed haze block, sized
// for the plane.
// ---------------------------------------------------------------------------

namespace {
// One AS-5 measurement row: the plane's SQUALL episodes (squall term > thr) on
// in-dome orbits, plus how many squalls start per 10 minutes of flight.
struct SquallRow {
    double median_ep, median_gap, share, per10;
};
SquallRow squall_row(double speed, const SnowfallParams& sp, int ens,
                     int variants, double thr = 0.5) {
    const WeatherParams wp;
    const WeatherCellParams cp;
    const Domes d = ship_domes();
    const double span = 20000.0;
    const DurationStats s = walk_avg(speed, sp, wp, cp, d.a, 2, ens, variants,
                                     true, thr, span);
    return {s.median_ep, s.median_gap, s.share,
            s.episodes / (variants * span / 600.0)};
}
}  // namespace

TEST_CASE("snowfall AS-5: squall_own_dials OFF is BIT-IDENTICAL to weather_cell") {
    const WeatherParams wp;
    const WeatherCellParams cp;
    const Domes dm = ship_domes();
    SnowfallParams sp = ship_params();
    sp.squall_own_dials = false;
    // Poison every squall dial: with the master switch off none may be read.
    sp.squall_inner_deg = 9.0;
    sp.squall_outer_deg = 19.0;
    sp.squall_cell_count = 3;
    sp.squall_gate_lo = -0.7;
    sp.squall_thresh_lo = 0.0;
    sp.squall_thresh_hi = 0.2;
    sp.squall_period_scale = 2.0;
    sp.squall_phase_off = 1.0;
    for (int i = 0; i < 256; ++i) {
        const glm::dvec3 d = sample_dir(i, 256);
        const glm::dvec3 e = in_cap_dir(dm.a[i % 2], i, 256);
        for (double t : {0.0, 91.0, 700.0, 4321.0, 54321.0}) {
            REQUIRE(render::snowfall_squall(d, t, wp, cp, nullptr, 0, sp) ==
                    weather_cell(d, t, wp, cp));
            REQUIRE(render::snowfall_squall(e, t, wp, cp, dm.a, 2, sp) ==
                    weather_cell(e, t, wp, cp, dm.a, 2));
        }
    }
}

TEST_CASE("snowfall AS-5: own dials set to the signed values IS weather_cell") {
    // The squall instance is the SAME machinery, not a fork: switched ON with
    // every dial copied from [weather]/[weather_cell] (or on its pass-through
    // value) it reproduces the haze's field bit-for-bit. A mutant that drops a
    // dial, reorders the phase add, or re-derives the cell loop fails here.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const Domes dm = ship_domes();
    SnowfallParams sp = ship_params();
    sp.squall_own_dials = true;
    sp.squall_inner_deg = 0.0;  // pass-through
    sp.squall_outer_deg = cp.bubble_outer_deg;
    sp.squall_cell_count = 0;  // pass-through
    sp.squall_gate_lo = wp.gate_lo;
    sp.squall_thresh_lo = cp.thresh_lo;
    sp.squall_thresh_hi = cp.thresh_hi;
    sp.squall_period_scale = 1.0;
    sp.squall_phase_off = 0.0;
    int live = 0;
    for (int i = 0; i < 256; ++i) {
        const glm::dvec3 e = in_cap_dir(dm.a[i % 2], i, 256);
        for (double t : {0.0, 91.0, 700.0, 4321.0, 54321.0}) {
            const double a = render::snowfall_squall(e, t, wp, cp, dm.a, 2, sp);
            REQUIRE(a == weather_cell(e, t, wp, cp, dm.a, 2));
            if (a > 0.0) ++live;
        }
    }
    REQUIRE(live > 0);  // not a vacuous 0 == 0
    // And each ACTIVE dial really is read: moving any one alone changes the
    // field somewhere (the fixture-no-op class).
    auto differs = [&](SnowfallParams q) {
        for (int i = 0; i < 256; ++i) {
            const glm::dvec3 e = in_cap_dir(dm.a[i % 2], i, 256);
            for (double t : {91.0, 700.0, 4321.0, 9000.0, 12345.0})
                if (render::snowfall_squall(e, t, wp, cp, dm.a, 2, q) !=
                    weather_cell(e, t, wp, cp, dm.a, 2))
                    return true;
        }
        return false;
    };
    SnowfallParams q = sp; q.squall_inner_deg = 4.0;      CHECK(differs(q));
    q = sp; q.squall_outer_deg = 10.0;                    CHECK(differs(q));
    q = sp; q.squall_cell_count = 5;                      CHECK(differs(q));
    q = sp; q.squall_gate_lo = -0.5;                      CHECK(differs(q));
    q = sp; q.squall_thresh_lo = 0.0;                     CHECK(differs(q));
    q = sp; q.squall_thresh_hi = 0.3;                     CHECK(differs(q));
    q = sp; q.squall_period_scale = 1.5;                  CHECK(differs(q));
    q = sp; q.squall_phase_off = 0.9;                     CHECK(differs(q));
}

TEST_CASE("snowfall AS-5 SWEEP", "[.as5sweep]") {
    // Hidden probe: the dial grid the shipped values were read from. Run with
    //   seads_tests "[.as5sweep]"
    SnowfallParams base = ship_params();
    base.squall_own_dials = true;
    std::printf("[AS-5 SWEEP] in out cnt gate thi scale | plane med_ep gap share per10\n");
    for (double in : {4.0, 6.0, 8.0})
        for (double outm : {2.0, 2.5})
            for (int cnt : {3, 4, 6})
                for (double gate : {-0.05, -0.35, -0.65})
                    for (double thi : {0.65, 0.35})
                        for (double sc : {1.0, 1.4}) {
                            SnowfallParams sp = base;
                            sp.squall_inner_deg = in;
                            sp.squall_outer_deg = in * outm;
                            sp.squall_cell_count = cnt;
                            sp.squall_gate_lo = gate;
                            sp.squall_thresh_lo = 0.0;
                            sp.squall_thresh_hi = thi;
                            sp.squall_period_scale = sc;
                            const SquallRow r = squall_row(142.0, sp, 0, 24);
                            std::printf(
                                "[AS-5 SWEEP] %4.1f %4.1f %d %5.2f %4.2f %3.1f | "
                                "%5.0f %5.0f %.3f %4.1f\n",
                                in, in * outm, cnt, gate, thi, sc, r.median_ep,
                                r.median_gap, r.share, r.per10);
                            std::fflush(stdout);
                        }
}

TEST_CASE("snowfall AS-5 RIDGE", "[.as5ridge]") {
    // Hidden probe: noise floor (4 independent draws x 48 orbits) at each
    // candidate and at one-dial-at-a-time neighbours, so a pick sits on a
    // PLATEAU (probe noise-floor law) rather than a lucky ridge.
    struct C { double in, outm; int cnt; double gate, thi, sc; };
    auto run = [](const char* tag, const C& c) {
        SnowfallParams sp = ship_params();
        sp.squall_own_dials = true;
        sp.squall_inner_deg = c.in;
        sp.squall_outer_deg = c.in * c.outm;
        sp.squall_cell_count = c.cnt;
        sp.squall_gate_lo = c.gate;
        sp.squall_thresh_lo = 0.0;
        sp.squall_thresh_hi = c.thi;
        sp.squall_period_scale = c.sc;
        double lo = 1e9, hi = -1e9, m = 0, p10 = 0, sh = 0, gap = 0;
        for (int e = 0; e < 4; ++e) {
            const SquallRow r = squall_row(142.0, sp, e, 48);
            lo = std::min(lo, r.median_ep);
            hi = std::max(hi, r.median_ep);
            m += r.median_ep / 4; p10 += r.per10 / 4; sh += r.share / 4;
            gap += r.median_gap / 4;
        }
        std::printf("[AS-5 RIDGE] %-10s %4.1f %4.1f %d %5.2f %4.2f %3.1f | "
                    "med %5.0f (%4.0f..%4.0f) gap %4.0f share %.3f per10 %.1f\n",
                    tag, c.in, c.in * c.outm, c.cnt, c.gate, c.thi, c.sc, m, lo,
                    hi, gap, sh, p10);
        std::fflush(stdout);
    };
    {
        // TODAY: the squall on the signed haze dials, at both thresholds.
        const WeatherParams wp; const WeatherCellParams cp; const Domes d = ship_domes();
        for (double thr : {0.05, 0.5}) {
            const DurationStats s = walk_avg(142.0, ship_params(), wp, cp, d.a, 2,
                                             0, 48, true, thr, 20000.0);
            std::printf("[AS-5 RIDGE] TODAY thr %.2f plane med %5.0f gap %4.0f "
                        "share %.3f per10 %.1f\n", thr, s.median_ep, s.median_gap,
                        s.share, s.episodes / (48 * 20000.0 / 600.0));
        }
    }
    // The four first candidates were read from [.as5sweep]; 8/15 is the pick
    // between them (inner 3.2x / outer 2.5x today's cell -- the "3-4x wider"
    // Chad said yes to).
    for (const C& c0 : {C{8, 1.875, 3, -0.65, 0.35, 1.0}}) {
        run("CENTER", c0);
        C c = c0; c.in -= 1; run("in-1", c);
        c = c0; c.in += 1; run("in+1", c);
        c = c0; c.outm -= 0.25; run("outm-", c);
        c = c0; c.outm += 0.25; run("outm+", c);
        c = c0; c.cnt -= 1; run("cnt-1", c);
        c = c0; c.cnt += 1; run("cnt+1", c);
        c = c0; c.gate -= 0.1; run("gate-", c);
        c = c0; c.gate += 0.1; run("gate+", c);
        c = c0; c.thi -= 0.1; run("thi-", c);
        c = c0; c.thi += 0.1; run("thi+", c);
        c = c0; c.sc = 1.2; run("sc1.2", c);
    }
}

TEST_CASE("snowfall AS-5 GATE LADDER", "[.as5gate]") {
    // Hidden probe. The squall gate barely moves the plane's episode length
    // (it sets HOW OFTEN cells light, the cell size sets how long a crossing
    // lasts), so the gate is the lever that trades heavy-snow share against
    // squalls-per-flight. Every metric that has a bar in this file, per rung.
    const WeatherParams wp;
    const WeatherCellParams cp;
    const Domes d = ship_domes();
    for (double gate : {-0.05, -0.25, -0.35, -0.45, -0.55, -0.65}) {
        SnowfallParams sp = ship_params();
        sp.squall_gate_lo = gate;
        double med = 0, p10 = 0, osh = 0;
        for (int e = 0; e < 4; ++e) {
            const SquallRow r = squall_row(142.0, sp, e, 48);
            med += r.median_ep / 4; p10 += r.per10 / 4; osh += r.share / 4;
        }
        long wet = 0, heavy = 0, tot = 0, deaths = 0, caught = 0;
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < 160; ++i) {
                const glm::dvec3 dd = in_cap_dir(d.a[k], i, 160);
                bool prev = false;
                for (int q = 0; q < 900; ++q) {
                    const double t = 5.0 * q;
                    const double sq = render::snowfall_squall(dd, t, wp, cp, d.a, 2, sp);
                    const double sn = snowfall_intensity(dd, t, wp, cp, d.a, 2, sp);
                    if (sn > 0.05) ++wet;
                    if (sn >= 0.8) ++heavy;
                    ++tot;
                    const bool now = sq > 0.05;
                    if (prev && !now) { ++deaths; if (sn > 0.05) ++caught; }
                    prev = now;
                }
            }
        std::printf("[AS-5 GATE] gate %5.2f | plane med %4.0f per10 %.1f orbit-sq %.3f | "
                    "uniform snow %.3f heavy %.3f | handover %.3f\n", gate, med, p10,
                    osh, double(wet) / tot, double(heavy) / tot,
                    deaths ? double(caught) / deaths : 0.0);
        std::fflush(stdout);
    }
}

TEST_CASE("snowfall AS-5: the plane's SQUALL episodes (the number that matters)") {
    // The sentinel's contract: "the number that matters is the PLANE'S median
    // squall episode and squall count per ~10 min flight; target 1-2 min
    // episodes, several per flight". A squall here is the squall term > 0.5 --
    // above the flurry's 0.35 peak, so nothing light can be counted as one.
    //
    // MEASURED (48 in-dome orbits, 20 000 s each; [.as5ridge] shows 4 draws
    // agree to a few seconds, and every one-dial neighbour stays 54-130 s):
    //   TODAY (weather_cell squall)  plane median  26 s, 2.7 / 10 min
    //   AS-5  8/15 deg x3, gate -0.65 plane median  86 s, 2.9 / 10 min, gaps 61 s
    SnowfallParams today = ship_params();
    today.squall_own_dials = false;
    const SnowfallParams sp = ship_params();
    const SquallRow t0 = squall_row(142.0, today, 0, 48);
    std::printf("[AS-5 SQUALL] TODAY plane 142 m/s  median_ep %4.0f s  gap %4.0f s  "
                "share %.3f  %.1f per 10 min\n", t0.median_ep, t0.median_gap,
                t0.share, t0.per10);
    SquallRow pl{};
    for (double v : {0.0, 20.0, 142.0}) {
        const SquallRow r = squall_row(v, sp, 0, 48);
        std::printf("[AS-5 SQUALL] AS-5  eye %5.0f m/s  median_ep %4.0f s  gap %4.0f s  "
                    "share %.3f  %.1f per 10 min\n", v, r.median_ep, r.median_gap,
                    r.share, r.per10);
        if (v == 142.0) pl = r;
    }
    CHECK(t0.median_ep < 40.0);    // the diagnosis: today's squall is a blink
    CHECK(pl.median_ep > 60.0);    // 1-2 min, with the band's own slack
    CHECK(pl.median_ep < 150.0);
    CHECK(pl.per10 > 2.0);         // several per flight
    CHECK(pl.median_gap > 20.0);   // and still separate EVENTS, not one blizzard
    CHECK(pl.share < 0.75);
}
