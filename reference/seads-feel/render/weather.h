#pragma once

#include <glm/glm.hpp>

// Pure weather variable (docs/little_planet_plan.md Stage 3 — the weather
// mechanism). A deterministic, seam-safe function of celestial time t_cel that
// returns a HAZE AMOUNT in [0,1]: 0 = clear (dark-starry sky), ~0.35 = light
// haze, ~1 = overcast. The app multiplies it by the overcast density and feeds
// the product into the SAME uWeatherAmt uniform the Stage-2 sky/aerial haze
// already reads (no shader change) — so clear weather (the ~70% common case)
// leaves the current Chad-approved space-first sky untouched, and only a hazy
// front brightens the horizon band + ground limb.
//
// PURE glm/std double — no raylib, no clock, in seads_render_core so the gate
// pins it headlessly (the celestial/sphere_param pattern). Time is TICK-DERIVED
// by the caller (t_cel = tick_count * sim_dt), so the result is frame-rate
// independent and identical at 30 vs 240 fps for the same t_cel — a hard
// requirement (AT-9-shaped; a wall-clock or frame-delta form is BANNED).
//
// SEAM (little_planet_plan.md standing rules, greppable): weather time NEVER
// feeds sim/ or control/ or drone AI — it is a render-only lighting input, like
// the celestial wheel.
//
// The math (Fable-5 before-consult, measured 69/25/5 clear/haze/overcast over
// 2M samples): a sum of three INCOMMENSURATE-prime-period sines (a slow
// quasi-random signal whose time-distribution is bell-shaped about 0) run
// through a smoothstep GATE that hard-zeros everything below gate_lo — so the
// signal sits at exactly clear most of the time and only its upper tail passes
// to haze/overcast. C1 (no pops); the periods set only the TEMPO, the gate sets
// the distribution.

namespace render {

// Weather tuning (config/world.toml [weather], mapped from cfg by the app so
// render/ does not depend on config/). Periods in seconds; weights/phases/gate
// dimensionless. Defaults are the Fable-vetted numbers.
struct WeatherParams {
    double period1_s = 1499.0;  // three incommensurate PRIME periods (s): the
    double period2_s = 547.0;   // fronts. LCM ~1.6e8 s (~5 sim-years), so no
    double period3_s = 197.0;   // exact short-period repeat within a session.
    double weight1 = 0.5;       // amplitude weights (sum 1) — the bell width.
    double weight2 = 0.3;
    double weight3 = 0.2;
    double phase1 = 3.6;  // fixed phases; chosen so t=0 (spawn) reads CLEAR
    double phase2 = 1.7;  // exactly (s(0) < gate_lo => haze(0)=0). Phase choice
    double phase3 =
        4.2;  // is distribution-invariant (Fable) — only spawn look.
    double gate_lo =
        -0.05;  // smoothstep gate low edge: signal below this = clear.
                // S-bubbleweather lowered this from 0.12 (Chad: weather "more
                // frequently"). SPAWN-CLEAR is the binding constraint: s(0) =
                // -0.098 with the phases above, so gate_lo must stay ABOVE
                // -0.098 or t=0 stops reading exactly clear.
    double gate_hi = 1.10;  // (0 exactly). >1 so haze only APPROACHES overcast.
};  // gate_lo/gate_hi are THE distribution knobs.

// The haze amount in [0,1] at celestial time t_cel [s]. Pure, deterministic,
// C1-smooth. 0 = clear; ~0.35 = light haze; approaching 1 = overcast.
double weather_haze(double t_cel, const WeatherParams& p);

// ---------------------------------------------------------------------------
// LOCALIZED WEATHER FIELD (docs/weather_seasons_plan.md W2 — "microsystems, no
// wind"). Turns the GLOBAL weather_haze BUDGET into a spatial patchwork of
// squalls: a FIXED golden-spiral lattice of N cells on the sphere, each cell
// ACTIVATED (not multiplied — Fable-before P0-1, the double-gate fix) by the
// budget through an ordered per-cell threshold, combined by soft-OR. The result
// is a local haze[0,1] at a unit sample direction; W2a feeds it at the eye's
// ground-track into the existing scalar uWeatherAmt ("fly into a squall, it
// clears as you leave"); W2b will evaluate the identical field per-fragment.
//
// KEY INVARIANTS (Fable-before, load-bearing):
//   - budget == 0  =>  field == 0 EVERYWHERE (inherits weather_haze's exact
//     ~70% clear AND the spawn-clear guarantee — a cell can only take from the
//     budget, never add).
//   - Cell centers are FIXED directions (a Fibonacci lattice + hash constants,
//     never an RNG draw — only the SEASON may be random) => NO WIND: squalls
//     pulse in place, they do not translate.
//   - Pure/deterministic/C1 in both `dir` and `t_cel`; no clock, no fixed axis.
//     Same seam as weather_haze (a render-only lighting input, never
//     sim/control).
struct WeatherCellParams {
    int cell_count =
        24;  // # microsystem cells on the sphere (Fibonacci lattice)
    double inner_deg =
        8.0;  // full local haze within this angle of a cell center
    double outer_deg =
        16.0;  // falloff to 0 by this angle (MUST exceed inner_deg)
    double thresh_lo =
        0.05;  // per-cell activation thresholds are spread across
    double thresh_hi =
        0.65;  // [lo,hi] (low-discrepancy) => cells pop in ~1-at-a-time.
               // S-bubbleweather NARROWED this band from 0.15/0.95: with the
               // old spread most cells needed a near-overcast budget and sat
               // dormant, so a typical budget lit ~one squall on the whole
               // sphere. Narrowing puts more cells in play at a modest budget.
    double env_width =
        0.15;  // smoothstep width in BUDGET over which a cell fades in

    // S-bubbleweather (Chad's fly, 2026-08-09: "add more weather as micro
    // events that happen more frequently within the bubble"). Dials for the
    // ANCHORED lattice below — cells packed inside a dome, at a squall scale
    // sized to a bubble rather than to the whole sphere.
    int bubble_cell_count = 16;     // cells per anchor region (>=1). Sized by
                                    // MEASUREMENT, not taste: at 8 the small
                                    // (6 deg) anchored cells covered no more
                                    // of a dome than the old 24 big (16 deg)
                                    // sphere cells did, so "more weather in
                                    // the bubble" was not actually delivered
                                    // (test_weather pins the comparison).
    double bubble_inner_deg = 2.5;  // full local haze within this angle (deg)
    double bubble_outer_deg = 6.0;  // falloff to 0 by here (MUST exceed inner)
    double bubble_fill_frac =
        0.80;  // fraction of the anchor's radius the cell centres spread
               // over, in (0,1] — keeps squalls off the fading dome rim
};

// S-bubbleweather: an anchor region for the localized field — cells are
// distributed INSIDE this spherical cap instead of over the whole sphere.
//
// WHY (the attribution): the global lattice put 24 cells on the sphere while
// the domes cover only ~8% of it, so ~2 cells fell in breathable air and the
// storm budget was mostly being spent on cells sitting in vacuum, where the
// air gate zeroed them. Chad's ruling ("weather only in the bubbles") is
// therefore satisfied not just by GATING weather off outside the domes but by
// GENERATING it inside them — the gate stays as the hard guarantee, this makes
// the budget actually buy weather the pilot can fly into.
struct WeatherAnchor {
    glm::dvec3 center_dir{0.0, 1.0, 0.0};  // unit; the dome centre direction
    double radius_rad = 0.4;               // cap angular radius [rad]
    // Optional in-plane reference for the placement basis (a bubble's
    // major_axis). Zero => derived from center_dir. This is a GAUGE for
    // WHERE cells sit, never an up/level measurement — no sphere invariant
    // reads it. Supplying it keeps the pattern rigidly tied to the bubble's
    // own frame, so a dome that grows/shrinks never re-shuffles its squalls.
    glm::dvec3 tangent_ref{0.0};
};

// Localized haze[0,1] at unit direction `dir` (a ground/sample direction on the
// sphere) and celestial time t_cel [s]. Uses weather_haze(t_cel, wp) as the
// storm BUDGET; returns 0 wherever/whenever the budget is clear. Pure,
// deterministic, C1. `dir` is assumed unit-length (the caller normalizes).
double weather_cell(const glm::dvec3& dir, double t_cel,
                    const WeatherParams& wp, const WeatherCellParams& cp);

// The ANCHORED form (S-bubbleweather): identical machinery — same budget, same
// ordered-threshold activation, same soft-OR — but the cell centres are packed
// into the given anchor caps (an equal-area sunflower per cap) at the
// bubble_* scale, instead of spread over the whole sphere.
//
// n_anchors <= 0 delegates to the global overload above BIT-IDENTICALLY, so
// every existing caller and test is untouched and a world with no bubbles
// still gets weather.
//
// Invariants carried over unchanged: budget == 0 => 0 everywhere (spawn-clear
// and the ~clear-plateau inherit); centres are a pure function of the anchor
// and the index, so squalls still pulse in place with NO WIND; pure/
// deterministic/C1; no clock, no fixed axis in any measurement.
double weather_cell(const glm::dvec3& dir, double t_cel,
                    const WeatherParams& wp, const WeatherCellParams& cp,
                    const WeatherAnchor* anchors, int n_anchors);

}  // namespace render
