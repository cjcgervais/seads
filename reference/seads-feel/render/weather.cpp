#include "render/weather.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render {

namespace {
constexpr double kTwoPi = 6.283185307179586;
}  // namespace

double weather_haze(double t_cel, const WeatherParams& p) {
    // The base quasi-random signal: three weighted sines at incommensurate
    // periods. Support [-1,1] (weights sum to 1), mass bell-shaped about 0 —
    // NOT a single sine (whose arcsine distribution piles mass at the extremes
    // and would make overcast far too common). Argument in DOUBLE: at a
    // session's t_cel (~1e4 s) a float sin(omega*t) argument error is already
    // visible shimmer; double is exact to ~1e-13 (the shader gets the finished
    // scalar, never sin(omega*t) in float32).
    const double s =
        p.weight1 * std::sin(kTwoPi / p.period1_s * t_cel + p.phase1) +
        p.weight2 * std::sin(kTwoPi / p.period2_s * t_cel + p.phase2) +
        p.weight3 * std::sin(kTwoPi / p.period3_s * t_cel + p.phase3);
    // Smoothstep GATE: everything below gate_lo maps to EXACTLY 0 (the ~clear
    // plateau — gate_lo sits above the signal's median, so most of the time it
    // is clear). Only the upper tail passes; gate_hi > 1 means the peak only
    // APPROACHES overcast. smoothstep (not max(0,·)) keeps it C1 at the joint,
    // so a front rolls in without a kink.
    const double denom = p.gate_hi - p.gate_lo;
    double u = denom > 0.0 ? (s - p.gate_lo) / denom : 0.0;
    u = std::clamp(u, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

namespace {
// C1 smoothstep, edge-order-safe (returns a clean step even if e0 >= e1).
double smoothstep(double e0, double e1, double x) {
    double t = e1 > e0 ? (x - e0) / (e1 - e0) : (x >= e1 ? 1.0 : 0.0);
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// A deterministic BIT-MIXED hash of cell index i, in [0,1). Used ONLY to ORDER
// the cells into evenly-spaced activation thresholds (cell_threshold below).
// CRITICAL (Fable W2a P1): this must NOT be an i*irrational form — the cell
// AZIMUTH is phi = i*golden_angle, so a golden-ratio threshold sequence
// resonates with it (spatial neighbors, at Fibonacci index gaps, would share
// thresholds and pop in together). A bit-avalanche hash is arithmetically
// UNRELATED to the linear azimuth, so the ordering is genuinely decorrelated.
double hash01(int i) {
    std::uint32_t x = static_cast<std::uint32_t>(i) * 2654435761u + 0x9E3779B9u;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return (x >> 8) * (1.0 / 16777216.0);  // top 24 bits -> [0,1)
}

// The per-cell activation threshold theta_i in [thresh_lo, thresh_hi]. The cells
// are RANKED by their hash and the rank spread EVENLY across [lo,hi], so the
// thresholds are a permutation with a GUARANTEED minimum gap (hi-lo)/n (Fable
// W2a P1's fix) AND decorrelated from spatial position (via hash01). O(n) per
// call — n is small (cell_count ~24). Ties broken by index (a stable order).
double cell_threshold(int i, int n, double lo, double hi) {
    const double hi_i = hash01(i);
    int rank = 0;
    for (int j = 0; j < n; ++j) {
        if (j == i) continue;
        const double hj = hash01(j);
        if (hj < hi_i || (hj == hi_i && j < i)) ++rank;
    }
    return lo + (hi - lo) * (static_cast<double>(rank) + 0.5) / n;
}

// The FIXED cell center i of n on the unit sphere — a Fibonacci (golden-spiral)
// lattice, evenly distributed, purely a function of (i, n) so the centers never
// move => no wind. Returns a unit vector.
// S-bubbleweather: the FIXED centre of cell j of m inside an anchor cap — an
// equal-area sunflower (r_frac = sqrt((j+0.5)/m), the same low-discrepancy
// idea as the sphere lattice, restricted to a disc) lifted onto the sphere by
// rotating the anchor direction by that angle. Purely a function of
// (anchor, j, m) => the centres never move, so NO WIND is preserved.
//
// The tangent basis is a placement GAUGE (see WeatherAnchor::tangent_ref).
// Preferring the caller's reference keeps the pattern locked to the bubble's
// own frame; the fallback picks the world axis LEAST aligned with the anchor,
// which is the standard well-conditioned choice (never near-parallel, so the
// cross product never degenerates).
glm::dvec3 anchor_cell_center(const WeatherAnchor& an, int j, int m) {
    const double golden_angle = 2.399963229728653;  // pi*(3 - sqrt5) [rad]
    const glm::dvec3 n = glm::normalize(an.center_dir);
    glm::dvec3 seed = an.tangent_ref;
    if (glm::length(seed) < 1e-9) {
        const glm::dvec3 abs_n(std::abs(n.x), std::abs(n.y), std::abs(n.z));
        seed = (abs_n.x <= abs_n.y && abs_n.x <= abs_n.z) ? glm::dvec3(1, 0, 0)
               : (abs_n.y <= abs_n.z)                     ? glm::dvec3(0, 1, 0)
                                                          : glm::dvec3(0, 0, 1);
    }
    glm::dvec3 e1 = glm::cross(n, seed);
    const double e1_len = glm::length(e1);
    if (e1_len < 1e-12) return n;  // degenerate reference: collapse to centre
    e1 /= e1_len;
    const glm::dvec3 e2 = glm::cross(n, e1);
    const double r_frac =
        std::sqrt((static_cast<double>(j) + 0.5) / static_cast<double>(m));
    const double theta = static_cast<double>(j) * golden_angle;
    const double ang = r_frac * an.radius_rad;
    const glm::dvec3 t = e1 * std::cos(theta) + e2 * std::sin(theta);
    return glm::normalize(n * std::cos(ang) + t * std::sin(ang));
}

glm::dvec3 cell_center(int i, int n) {
    const double golden_angle = 2.399963229728653;  // pi*(3 - sqrt5) [rad]
    const double z = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / n;  // (-1,1)
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = static_cast<double>(i) * golden_angle;
    return glm::dvec3(r * std::cos(phi), r * std::sin(phi), z);
}
}  // namespace

double weather_cell(const glm::dvec3& dir, double t_cel, const WeatherParams& wp,
                    const WeatherCellParams& cp) {
    // The storm BUDGET: the SAME global weather_haze tempo (~70% exact-clear),
    // reused as "how much weather is active right now." budget == 0 => every
    // cell's activation is 0 below => the field is 0 everywhere (the clear +
    // spawn-clear guarantees are inherited, not re-derived).
    const double budget = weather_haze(t_cel, wp);
    if (budget <= 0.0) return 0.0;

    const int n = std::max(1, cp.cell_count);
    const double deg2rad = 0.017453292519943295;
    // Falloff edges in DOT space (no per-fragment acos). inner (larger dot) =
    // full haze; outer (smaller dot) = 0.
    const double cos_inner = std::cos(cp.inner_deg * deg2rad);
    const double cos_outer = std::cos(cp.outer_deg * deg2rad);

    // Soft-OR combine: local = 1 - Prod_i (1 - env_i * falloff_i). Bounded [0,1]
    // with NO clamp, C1 everywhere, overlapping squalls saturate gracefully.
    double keep = 1.0;
    for (int i = 0; i < n; ++i) {
        // Activation DERIVED FROM the budget (not multiplied by it): each cell
        // has a fixed ordered threshold; it fades in over env_width of budget.
        // Distinct thresholds => cells pop in ~one at a time as the budget rises.
        const double theta =
            cell_threshold(i, n, cp.thresh_lo, cp.thresh_hi);
        const double env = smoothstep(theta, theta + cp.env_width, budget);
        if (env <= 0.0) continue;  // this cell is dormant at the current budget
        const double d = glm::dot(dir, cell_center(i, n));
        const double fall = smoothstep(cos_outer, cos_inner, d);
        keep *= (1.0 - env * fall);
    }
    return 1.0 - keep;
}

double weather_cell(const glm::dvec3& dir, double t_cel, const WeatherParams& wp,
                    const WeatherCellParams& cp, const WeatherAnchor* anchors,
                    int n_anchors) {
    // No anchors (no bubbles, or a caller that does not have them) => the
    // global lattice, BIT-IDENTICALLY. Weather never disappears just because
    // the anchored path has nothing to anchor to.
    if (anchors == nullptr || n_anchors <= 0)
        return weather_cell(dir, t_cel, wp, cp);

    // The SAME storm budget as the global form — inherited, not re-derived,
    // so the ~clear plateau and the spawn-clear guarantee still hold and a
    // cell can still only take from the budget, never add.
    const double budget = weather_haze(t_cel, wp);
    if (budget <= 0.0) return 0.0;

    const int m = std::max(1, cp.bubble_cell_count);
    const int n_total = m * n_anchors;
    const double deg2rad = 0.017453292519943295;
    const double cos_inner = std::cos(cp.bubble_inner_deg * deg2rad);
    const double cos_outer = std::cos(cp.bubble_outer_deg * deg2rad);
    const double fill = std::clamp(cp.bubble_fill_frac, 0.0, 1.0);

    // Thresholds are ranked across EVERY cell of EVERY anchor (one shared
    // ordering, not per-anchor), so squalls still pop in ~one at a time as the
    // budget rises rather than both domes lighting up in lockstep.
    double keep = 1.0;
    for (int k = 0; k < n_anchors; ++k) {
        WeatherAnchor an = anchors[k];
        an.radius_rad *= fill;
        for (int j = 0; j < m; ++j) {
            const int i = k * m + j;
            const double theta =
                cell_threshold(i, n_total, cp.thresh_lo, cp.thresh_hi);
            const double env = smoothstep(theta, theta + cp.env_width, budget);
            if (env <= 0.0) continue;  // dormant at the current budget
            const double d = glm::dot(dir, anchor_cell_center(an, j, m));
            const double fall = smoothstep(cos_outer, cos_inner, d);
            keep *= (1.0 - env * fall);
        }
    }
    return 1.0 - keep;
}

}  // namespace render
