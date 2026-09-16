#pragma once
// PRECIPITATION placement core (docs/weather_seasons_plan.md W3) — the PURE,
// raylib-free, gate-pinned math for season-gated snow/rain. The raylib billboard
// renderer that consumes it lives in render/precip_draw.{h,cpp} (the seads exe);
// this unit is in seads_render_core so the app-binary-blind gate pins the
// load-bearing invariants headlessly.
//
// THE MECHANISM (Fable-before P1-3/P1-4, load-bearing):
//   - A WORLD-ANCHORED jittered lattice: particle base positions sit on a fixed
//     world cubic grid (integer cell + a deterministic per-cell jitter), so they
//     stay put in world space and STREAM PAST the plane for free as you fly — NOT
//     a camera-frame snow-globe (that reads as wind following you). The world-axis
//     grid is CELL BOOKKEEPING ONLY; it is never a visible direction (the jitter
//     breaks its regularity, and the only visible motion is the fall).
//   - The ONLY NET displacement is the DOWNWARD fall along local_up =
//     normalize(eye) — NO horizontal advection term (Chad's NO-WIND ruling). The
//     fall phase is a pure wrapped fn of t_cel (computed app-side in double).
//   - The BOX follows the eye (snaps to the grid) with a radial boundary alpha
//     fade -> 0 at the box edge, so re-binning as the eye moves is C0 (no pop).
//   - NEVER a tangent basis from a fixed world axis for anything visible (two
//     antipodal degeneracies; GO-ANYWHERE says someone flies through them). Only
//     local_up (radial) is used for motion; the world lattice is invisible.
//
// AS-3 (atmosphere rung, 2026-09-12) added three PURE look terms, every one with
// an OFF value that is bit-identical to the above:
//   - per-flake SIZE / ALPHA variety hashed from the cell (size_var/alpha_var=0),
//   - a hashed per-cell DENSITY cull driven by intensity (density_exp=0),
//   - a ZERO-MEAN lateral SWAY (sway_m=0). The sway is NOT wind: its axis AND
//     phase are hashed per cell (no two cells sway together) and its average over
//     one full fall cycle is exactly zero, so no flake and no field ever
//     TRANSLATES. test_precip_look.cpp pins both halves of that claim.
// AS-1 added the UNDERGROUND gate (precip_rock_alpha / precip_rock_mode): the
// tunnel-net SDF, injected as a callable so this layer stays pure and testable
// without a real world::TunnelNet.

#include <glm/glm.hpp>

namespace render {

// The integer world cell the eye-following box centers on: round(eye / cell_size)
// per axis. As the eye flies this shifts by whole cells => the box re-bins the
// world-anchored lattice (world positions unchanged; the visible SET changes).
glm::ivec3 precip_center_cell(const glm::dvec3& eye, double cell_size);

// One particle's world position + its fade alpha in [0,1]. `cell` is the integer
// WORLD cell (jitter + fall-phase offset are hashed from it, so the base is
// world-anchored and wind-free); `local_up` is the unit radial (normalize(eye));
// `fall_phase` is the app-owned wrapped fall phase in [0,1). The returned alpha
// is the geometry fade only (radial boundary fade x wrap fade x the AS-3 per-cell
// alpha variety / density cull) — the caller multiplies in the look opacity and
// the snowfall intensity.
struct PrecipSample {
    glm::dvec3 pos;  // world position (jittered base - local_up*fall + sway).
                     // ONLY meaningful when alpha > 0: the sampler short-
                     // circuits invisible flakes, so a culled sample's pos may
                     // be the pre-sway position (or the eye, for a density-
                     // culled cell). alpha == 0 means "do not draw", full stop.
    double alpha;    // geometry fade [0,1] (0 => cull)
    double size_mul = 1.0;  // AS-3 per-flake size multiplier (1 when size_var=0)
};

// AS-3 look/field tuning for precip_sample. EVERY field below defaults to its
// OFF value, so a default-constructed PrecipFieldParams with the three lattice
// numbers filled in reproduces the pre-AS-3 sampler EXACTLY.
struct PrecipFieldParams {
    double cell_size = 3.0;   // world grid spacing (m)
    double box_half = 21.0;   // radial boundary-fade radius (m)
    double wrap_fade = 0.12;  // vertical wrap fade fraction [0, 0.5)
    // --- AS-3, all OFF at 0 ---
    double sway_m = 0.0;       // zero-mean lateral sway amplitude (m)
    double size_var = 0.0;     // per-flake size  x [1-v, 1+v]
    double alpha_var = 0.0;    // per-flake alpha x [1-v, 1]
    double inner_fade_m = 0.0;  // AS-3 P1-4: radial fade to 0 INSIDE this
                                // radius, ramping to unchanged by 2x it. The
                                // FAR veil's flakes are 0.45 m and Chad already
                                // ruled 0.28 m "too big / in your face", so the
                                // veil must not reach the canopy: this is what
                                // makes it a veil at distance rather than a
                                // second near lattice. 0 => OFF.
    double density_exp = 0.0;  // hashed per-cell cull: keep iff hash01(cell) <
                               // keep_p, softened over density_soft. 0 => the
                               // cull is OFF entirely.
    double density_soft = 0.0;  // width in keep_p over which a culled cell fades
                                // back IN, so a squall breathing in and out adds
                                // flakes smoothly instead of popping whole cells
                                // on. 0 => the original hard step.
    double keep_p = 1.0;       // == precip_keep_p(intensity, density_exp). The
                               // CALLER fills this ONCE PER FRAME: it is
                               // constant across the lattice, and calling
                               // std::pow 19 000 times a frame to re-derive it
                               // was a measured share of the precip pass.
                               // Ignored when density_exp == 0.
};

// The per-cell KEEP PROBABILITY for the AS-3 density law: intensity^density_exp,
// clamped. Split out of precip_sample so the pow() is paid once per frame, and
// so the density law itself is directly testable.
double precip_keep_p(double intensity, double density_exp);

PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double fall_phase,
                           const PrecipFieldParams& fp);

// The PRE-AS-3 signature, kept so the original gate cases call the exact same
// code path they always did (the OFF-value proof is a delegation, not a copy).
PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double cell_size,
                           double box_half, double wrap_fade, double fall_phase);

// ---------------------------------------------------------------------------
// AS-1 — NO SNOW UNDERGROUND.
//
// Chad, 2026-09-12: "we do get more snow, but not in the big stope. I noticed it
// there." The W3 lattice is world-anchored around the EYE and knows nothing
// about rock overhead, so flying the Murray/Errington arena rained flakes
// through solid rock. The fix is a per-FLAKE gate on the tunnel net's signed
// distance — not a per-eye one, because standing in a mouth looking out you must
// still see the snow OUTSIDE while the flakes under the rock are gone.
//
// The SDF is INJECTED as a plain callable (world/ must not leak into
// seads_render_core, and the test must not need a real TunnelNet). `ctx` is
// opaque; the contract is world::TunnelNet::signed_distance: < 0 == inside the
// rock-enclosed void, > 0 == outside it.
using PrecipSdf = double (*)(const void* ctx, const glm::dvec3& p);

// The flake's alpha multiplier: 0 only when the flake is BOTH inside the tunnel
// net (sd < 0) AND below the local terrain (alt < 0); 1 otherwise; a C0-monotone
// smoothstep across +/- `band` metres on each. band <= 0 degenerates to a hard
// step. Pure; this IS the gate.
//
// WHY THE SECOND TERM (MEASURED, not assumed — it is the difference between the
// rung and a regression): world::TunnelNet::signed_distance reads NEGATIVE for a
// band of ordinary OPEN AIR above the ground near the shallow arena. Measured at
// the default spawn on 2026-09-12: eye_sd = alt - 101 m, so every eye below
// ~98 m AGL — i.e. the whole of a sled ride — reports "inside the net". The
// tunnel volume is an ellipsoid union sealed by the terrain, not a cavity carved
// out of it, so "inside the net" alone is NOT "under rock". Gating on the SDF by
// itself would have deleted the snow at exactly the altitude Chad drives at, and
// the --smoke shot at 23 m AGL showed precisely that: open sky, bare ground, not
// one flake. A flake ABOVE the terrain has rock nowhere above it by definition,
// so it always falls.
double precip_rock_alpha(double sd, double alt, double band);

// The per-frame whole-box decision, from the EYE's signed distance and its
// height above the local terrain. The point is that the per-flake SDF (a full
// primitive scan) runs ONLY where the box actually straddles rock:
//   AllOutside — no per-flake test: either the box is entirely clear of the net,
//                or the whole box is above the terrain (nothing can be buried).
//   AllInside  — the whole box is inside the net AND under the terrain: skip the
//                draw entirely.
//   MouthBand  — the box straddles: the per-flake gate runs.
enum class PrecipRockMode { AllOutside, MouthBand, AllInside };
PrecipRockMode precip_rock_mode(double d_eye, double eye_alt, double box_half,
                                double band);

}  // namespace render
