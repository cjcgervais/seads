#pragma once
// SNOWFALL FIELD (atmosphere rung AS-2, docs/SESSION_HANDOFF_20260912_atmosphere_snow.md).
//
// WHY THIS EXISTS (Chad, 2026-09-12): "Atmosphere rung needs to get done better
// so that we do get more snow ... Need more localized weather events not wind
// but appearance of snow in a very effective way that deepens immersion."
//
// Today precipitation is gated by exactly the same scalar as the HAZE — the W2
// `weather_cell` field. That field is a SQUALL field: it is designed so clear
// dominates (the haze distribution is Chad-signed at ~58/34/7), so most of a
// winter flight is structurally dry. Raising the haze dials to get more snow
// would move a signed distribution; that is forbidden.
//
// So the snow gets its OWN field, BESIDE the haze, built out of the SAME
// machinery (no fork): snowfall = max(squall, flurry, floor) where
//   - squall = weather_cell(...) exactly as today  -> the heavy band unchanged
//   - flurry = the same anchored cell machinery on its OWN BUDGET, at a LOWER
//     activation band, on a DIFFERENT lattice, scaled down
//   - floor  = an optional constant winter dusting (ships at 0)
//
// THE FLURRY'S OWN BUDGET (red-team P1-3, 2026-09-12). The first cut activated
// the flurry from the SAME weather_haze the squalls use. That budget is EXACTLY
// ZERO on its ~58% clear plateau, so no activation band whatsoever could put
// snow in the sky more than ~42% of the time, and the measured share came out
// at 17%. The constant floor was then doing all the work — and a constant
// dusting is the opposite of "localized weather events". So the flurry runs a
// SECOND INSTANCE of weather_haze: same three incommensurate front periods and
// weights (the same tempo — you still fly into and out of bands), a phase
// OFFSET so it is not the squall budget in disguise, and its own much lower
// gate_lo so the light band is live most of the time. The Chad-signed [weather]
// block is untouched: this is a second instance of the function, not a change
// to its dials.
//
// INVARIANTS (load-bearing, pinned by test/unit/test_snowfall.cpp):
//   - flurry_level == 0 && snow_floor == 0  =>  BIT-IDENTICAL to weather_cell,
//     i.e. today's `wfield` (every new dial has an OFF value).
//   - Pure / deterministic in (dir, t_cel); no clock, no fixed axis, no state.
//     C1 within each term; the max() joins are C0 (a kink, never a pop).
//     Same seam as weather_cell: a render-only input, never sim/control.
//   - NO WIND: cell centres are fixed functions of the index/anchor, so
//     flurries pulse IN PLACE. Nothing here translates.
//   - The result is still AIR-GATED by the caller (render::gate_weather_by_air),
//     exactly like the haze scalar — snow in vacuum is nonsense (S-domeround).
//
// PURE glm/std double, no raylib: lives in seads_render_core so the headless
// gate pins it.

#include <glm/glm.hpp>

#include "render/weather.h"

namespace render {

// Tuning for the snowfall field (config/world.toml [precip], mapped by the app
// so render/ does not depend on config/). Defaults = the ship values.
struct SnowfallParams {
    // The LIGHT, FREQUENT band. 0 => no flurry term at all (OFF value).
    double flurry_level = 0.35;
    // The flurry cells' activation band in ITS OWN budget. Much lower than the
    // squall band ([0.05, 0.65]) so a flurry cell lights as soon as there is
    // any flurry budget at all, instead of waiting for a near-overcast sky.
    double flurry_thresh_lo = 0.00;
    double flurry_thresh_hi = 0.65;  // AS-4 widened this from 0.35: with five
                                     // big cells instead of 23 small ones, a
                                     // narrow band lit them all at once and the
                                     // dome never dried out.
    // The flurry BUDGET's own gate: the low edge of weather_haze's smoothstep
    // for the flurry instance. The [weather] block ships -0.05, which hard-zeros
    // ~58% of the time; a much lower edge here is what makes the light band a
    // frequent, present part of a winter flight instead of a rarity.
    double flurry_gate_lo = -0.95;
    // Radians added to all three of the flurry budget's front phases, so the
    // flurry budget is not the squall budget in disguise (same tempo, different
    // moment). Distribution-invariant — it moves WHEN, never HOW OFTEN.
    //
    // AS-4 moved this 1.7 -> 2.6 for the SQUALL-TO-FLURRY HANDOVER: at 1.7 only
    // 0.29 of squall deaths still had light snow underneath, so a dying squall
    // usually CUT to dry. At 2.6 it is 0.86 -- the pilot gets a taper instead.
    // The two budgets share their three periods, so the phase is the only lever
    // that decorrelates their ups and downs; no state, no hysteresis.
    double flurry_phase_off = 2.6;
    // --- AS-4 EVENT DURATION (Chad's fly of aa9d78142: "there is not enough
    // durition of the weather events"). MEASURED FIRST (test_snowfall.cpp's
    // duration table, in-dome orbits): the TEMPO was never the problem -- a
    // stationary eye already sat in 20.9 min episodes. The PLANE was the
    // problem: at 142 m/s it crossed a 6 deg flurry cell in ~22 s, so its
    // median episode was 36 s with 13 s gaps -- snow 87% of the time, but
    // blinking. That reads as "not enough duration" even though the share is
    // high. So the dials that matter are SPATIAL: the flurry gets its own,
    // much larger cell scale, and fewer of them.
    //
    // All four have an OFF value that reproduces aa9d78142 bit-identically:
    // scale 1.0, degrees 0 (meaning "use the bubble_* values"), count 0
    // (meaning "bubble_cell_count + the lattice bump").
    // Multiplies the flurry budget's three front periods (1.0 = OFF). Scaling
    // all three together preserves their incommensurability, so it stretches
    // the tempo without introducing a short repeat.
    //
    // SHIPS AT 1.4, and the relationship is NOT monotonic -- MEASURED over a
    // 12-orbit ensemble: the plane's median episode is 172 s at 1.0, 312 s at
    // 1.4, and ~90 s by 1.8. The tempo has to beat against the time the eye
    // takes to cross a cell; too slow and the pattern freezes, so a fast eye's
    // wet/dry becomes purely spatial and chops at every boundary.
    //
    // WARNING: 1.4 is a RIDGE in scale, not a plateau -- 1.3/1.4/1.5 measure
    // 299/312/146 s for the plane. It IS a plateau in PHASE (2.2..3.1 all
    // hold), and every point in the 1.3-1.5 block keeps the sled above 7 min,
    // the share near 0.60 and the handover above 0.5; only the plane's median
    // swings. Chad's eye is the arbiter and this dial is one config line.
    double flurry_period_scale = 1.4;
    double flurry_inner_deg = 6.0;  // the flurry cell's own full-intensity
                                    // radius. 0 = use cp.bubble_inner_deg.
    double flurry_outer_deg = 14.0;  // its own falloff radius (> inner).
                                     // 0 = use cp.bubble_outer_deg.
    int flurry_cell_count = 5;      // cells per anchor. 0 = bubble_cell_count
                                    // + the lattice bump (the AS-2 rule).
    // An optional constant winter dusting, live even when both budgets are
    // clear. SHIPS AT 0: it is not a localized event, and with the flurry on its
    // own budget it is no longer needed to keep the sky from going bone dry.
    // Kept as a dial because it is the one knob that guarantees a floor.
    double snow_floor = 0.0;

    // --- AS-5 SNOW-SQUALL ON ITS OWN DIALS (Chad via the sentinel, 2026-09-12:
    // "yes please do so"). His fly of 6aa0d79fb: "I see very few weather events
    // and they barely last 15s". The events he SEES are the SQUALL, and until
    // AS-5 the squall WAS weather_cell on the signed [weather_cell] bubble dials:
    // ~3 km cells (2.5/6 deg), lit only while the haze budget clears -0.05.
    // A 142 m/s plane crosses one of those in 15-22 s. That is his "15 s".
    //
    // So the snow squall gets the SAME move the flurry got in AS-2/AS-4: a
    // second instance of the cell machinery on its own private params, beside
    // the signed block. The HAZE path (app/main.cpp's wfield) still calls
    // weather_cell on [weather]/[weather_cell] and is untouched; only the snow
    // reads these.
    //
    // THE MASTER OFF VALUE: squall_own_dials == false => the squall term is
    // weather_cell(dir, t_cel, wp, cp, ...) exactly and every squall_* dial below
    // is ignored. With it true, each dial still has a per-dial pass-through
    // (degrees 0 / count 0 = the bubble value, scale 1.0, phase 0), and setting
    // gate/threshold to the [weather]/[weather_cell] values reproduces
    // weather_cell bit-identically (pinned) -- it IS the same machinery.
    bool squall_own_dials = false;
    double squall_inner_deg = 0.0;     // full-intensity radius. 0 = bubble
    double squall_outer_deg = 0.0;     // falloff radius (> inner). 0 = bubble
    int squall_cell_count = 0;         // cells per anchor. 0 = bubble count
    double squall_gate_lo = -0.05;     // the squall budget's clear-plateau edge
    double squall_thresh_lo = 0.05;    // per-cell activation band, in budget
    double squall_thresh_hi = 0.65;
    double squall_period_scale = 1.0;  // x the three front periods (1 = OFF)
    double squall_phase_off = 0.0;     // rad added to the three phases (0 = OFF)
};

// The SQUALL term alone (the heavy band, peak 1.0). snowfall_intensity is
// max(this, flurry, floor). Exposed so the AS-5 measurements can time squall
// episodes separately from the light band. squall_own_dials == false =>
// weather_cell(...) bit-identically.
double snowfall_squall(const glm::dvec3& dir, double t_cel,
                       const WeatherParams& wp, const WeatherCellParams& cp,
                       const WeatherAnchor* anchors, int n_anchors,
                       const SnowfallParams& sp);

// The snowfall intensity in [0,1] at unit direction `dir` and celestial time
// t_cel [s]. `wp`/`cp`/`anchors` are exactly the haze path's parameters — the
// squall term IS weather_cell, unchanged, so the heavy band the pilot flies
// into is the same one the haze shows.
//
// With sp.flurry_level == 0 and sp.snow_floor == 0 this returns
// weather_cell(dir, t_cel, wp, cp, anchors, n_anchors) bit-identically.
double snowfall_intensity(const glm::dvec3& dir, double t_cel,
                          const WeatherParams& wp, const WeatherCellParams& cp,
                          const WeatherAnchor* anchors, int n_anchors,
                          const SnowfallParams& sp);

}  // namespace render
