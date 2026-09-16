#include "render/snowfall.h"

#include <algorithm>
#include <cmath>

namespace render {

namespace {
// The flurry lattice is the SQUALL lattice with this many extra cells. It is
// pure BOOKKEEPING, not a felt number: the cell centres are a golden-spiral
// lattice of n points, so evaluating at n and at n + bump gives two different
// point sets. A prime bump keeps the two from sharing a sub-pattern.
constexpr int kFlurryLatticeBump = 7;

// THE FLURRY'S PLACEMENT GAUGE (red-team P1-2, 2026-09-12). Inside an anchor cap
// render::anchor_cell_center places cell j at theta = j * golden_angle and
// radius sqrt((j+0.5)/m) * cap_radius * bubble_fill_frac. The BEARING sequence
// is a function of j ALONE, so changing only the cell COUNT reuses the same
// bearings: with 16 squall cells and 23 flurry cells the first 16 flurry centres
// sat on the same 16 bearings at slightly different radii. MEASURED: 11 of 23
// flurry centres inside a squall's 2.5 deg inner radius, ~0.65 of them --
// against a CHANCE baseline of ~0.30 (16 discs of 2.5 deg inner radius cover
// about 30% of an 18.3 deg cap, so some coincidence is packing, not
// correlation). 0.65 is twice chance: the lattices really were coupled.
//
// The gauge that fixes it is RADIAL, not angular. An angular offset was tried
// first and rejected as measurement-tuned: the overlap swung between 0.00 and
// 0.65 depending on the exact rotation, because the bearing offset only has to
// land near some multiple of the golden angle to re-couple the two sets, and
// which rotation is "good" changes the moment anyone touches
// bubble_cell_count. Scaling the flurry's own bubble_fill_frac separates the
// two sets by RADIUS at every bearing, and it holds across cell-count jitter:
// measured 0.196 at the ship counts, worst 0.222 over 14/16/18/20 squall cells.
// It is the same class of change as the threshold band and the cell count that
// are already set on this private copy -- a placement choice, not a mechanism.
constexpr double kFlurryFillGauge = 0.85;

// The flurry's OWN storm budget parameters: the same fronts (same periods, same
// weights => the same tempo, so a flurry still rolls in and out), offset in
// phase so it is not the squall budget in disguise, and gated much lower so the
// light band is live most of the time instead of ~42% at best.
WeatherParams flurry_budget_params(const WeatherParams& wp,
                                   const SnowfallParams& sp) {
    WeatherParams fw = wp;
    fw.phase1 += sp.flurry_phase_off;
    fw.phase2 += sp.flurry_phase_off;
    fw.phase3 += sp.flurry_phase_off;
    fw.gate_lo = sp.flurry_gate_lo;
    // AS-4: the flurry's fronts may run SLOWER than the haze's. Scaling all
    // three together preserves their incommensurability (the ratios are
    // unchanged, so the LCM is still astronomically long) -- it stretches the
    // tempo without introducing a short repeat.
    if (sp.flurry_period_scale > 0.0) {
        fw.period1_s *= sp.flurry_period_scale;
        fw.period2_s *= sp.flurry_period_scale;
        fw.period3_s *= sp.flurry_period_scale;
    }
    return fw;
}
}  // namespace

double snowfall_squall(const glm::dvec3& dir, double t_cel,
                       const WeatherParams& wp, const WeatherCellParams& cp,
                       const WeatherAnchor* anchors, int n_anchors,
                       const SnowfallParams& sp) {
    // OFF: the haze's own field, untouched -- the pre-AS-5 squall exactly.
    if (!sp.squall_own_dials)
        return weather_cell(dir, t_cel, wp, cp, anchors, n_anchors);

    // AS-5: a second instance on private copies. [weather]/[weather_cell] are
    // read, never written; the haze keeps its signed field.
    WeatherParams sw = wp;
    sw.phase1 += sp.squall_phase_off;
    sw.phase2 += sp.squall_phase_off;
    sw.phase3 += sp.squall_phase_off;
    sw.gate_lo = sp.squall_gate_lo;
    if (sp.squall_period_scale > 0.0) {
        // All three together: the ratios survive, so no short repeat appears.
        sw.period1_s *= sp.squall_period_scale;
        sw.period2_s *= sp.squall_period_scale;
        sw.period3_s *= sp.squall_period_scale;
    }
    WeatherCellParams sc = cp;
    sc.thresh_lo = sp.squall_thresh_lo;
    sc.thresh_hi = std::max(sp.squall_thresh_hi, sp.squall_thresh_lo);
    if (sp.squall_cell_count > 0) sc.bubble_cell_count = sp.squall_cell_count;
    if (sp.squall_inner_deg > 0.0) sc.bubble_inner_deg = sp.squall_inner_deg;
    if (sp.squall_outer_deg > 0.0) sc.bubble_outer_deg = sp.squall_outer_deg;
    // Same guard as the flurry: a degenerate falloff is a hard edge.
    if (sc.bubble_outer_deg <= sc.bubble_inner_deg)
        sc.bubble_outer_deg = sc.bubble_inner_deg * 1.5 + 1e-6;
    return weather_cell(dir, t_cel, sw, sc, anchors, n_anchors);
}

double snowfall_intensity(const glm::dvec3& dir, double t_cel,
                          const WeatherParams& wp, const WeatherCellParams& cp,
                          const WeatherAnchor* anchors, int n_anchors,
                          const SnowfallParams& sp) {
    // THE SQUALL: the heavy band. With squall_own_dials off this is today's
    // weather_cell exactly (the bit-identical OFF value); AS-5 gives it its own
    // cell scale and gate. Computed first and never scaled.
    const double squall =
        snowfall_squall(dir, t_cel, wp, cp, anchors, n_anchors, sp);

    // THE FLURRY: the same machinery, its own budget, a lower activation band, a
    // different lattice on a rotated gauge, scaled down. Skipped entirely at
    // level 0 so the OFF value costs nothing and cannot perturb the result by a
    // rounding bit.
    double flurry = 0.0;
    if (sp.flurry_level > 0.0) {
        WeatherCellParams fp = cp;
        fp.thresh_lo = sp.flurry_thresh_lo;
        fp.thresh_hi = std::max(sp.flurry_thresh_hi, sp.flurry_thresh_lo);
        fp.cell_count = cp.cell_count + kFlurryLatticeBump;
        // AS-4: the flurry's own CELL SCALE. This is the dial that fixes the
        // duration Chad felt missing -- a 142 m/s plane crossed a 6 deg cell in
        // ~22 s, so the light band blinked. Bigger cells, fewer of them: the
        // pilot flies INSIDE one for minutes instead of through a string of
        // them. 0 => the bubble values, i.e. the pre-AS-4 field exactly.
        fp.bubble_cell_count =
            sp.flurry_cell_count > 0 ? sp.flurry_cell_count
                                     : cp.bubble_cell_count + kFlurryLatticeBump;
        if (sp.flurry_inner_deg > 0.0) fp.bubble_inner_deg = sp.flurry_inner_deg;
        if (sp.flurry_outer_deg > 0.0) fp.bubble_outer_deg = sp.flurry_outer_deg;
        // outer MUST exceed inner or the falloff smoothstep degenerates into a
        // hard edge; clamp rather than trust the caller.
        if (fp.bubble_outer_deg <= fp.bubble_inner_deg)
            fp.bubble_outer_deg = fp.bubble_inner_deg * 1.5 + 1e-6;
        fp.bubble_fill_frac = cp.bubble_fill_frac * kFlurryFillGauge;
        const WeatherParams fw = flurry_budget_params(wp, sp);
        flurry = sp.flurry_level *
                 weather_cell(dir, t_cel, fw, fp, anchors, n_anchors);
    }

    // THE FLOOR: a constant dusting, live even when both budgets are clear.
    // Ships at 0 (see SnowfallParams) — it is not a localized event.
    const double floor_v = std::clamp(sp.snow_floor, 0.0, 1.0);

    // max(), not a sum: a flurry inside a squall must not push the squall past
    // 1, and the floor must never brighten a live band. The heavy band is then
    // EXACTLY the squall's heavy band (what AS-2 requires). The joins are C0.
    const double v = std::max(std::max(squall, flurry), floor_v);
    return std::clamp(v, 0.0, 1.0);
}

}  // namespace render
