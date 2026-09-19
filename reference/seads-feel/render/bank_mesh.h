#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

#include "render/sphere_param.h"  // HeightField, facet_radius_at, CutDisk
#include "world/snowpack.h"

// ★ SF2-BANKS — OREO SNOWBANKS (WINTER_LAW §3.6c, ruled Chad 2026-08-12 after
// the first street drive found the banks force-only invisible).
//
// The SF1 pattern generalized: ONE function, two consumers, zero fork. The
// bank's driven shape already lives in SnowpackField::depth_at (feather +
// bank_profile + junction gaps). The drawn bank is a ribbon-sibling strip
// whose every vertex radial is facet_radius_at(dir) + lift + depth_at(dir) --
// the rendered terrain facet (the ribbons' R4d anti-float discipline) plus THE
// REAL FIELD, sampled, never re-derived. This mesh cannot fork from the
// physics because it never re-implements it. It also closes BLOCK-VP1 LOCALLY:
// a band of the true driven snow surface becomes visible beside every plowed
// road. BLOCK-VP1 itself stays open and Chad's for the global field.
//
// ★ R1 NOTE (BLOCK-VP1): these rings sit on facet_radius_at -- the TERRAIN
// facet -- and NOT on drawn_radius_at, deliberately. The strip already ADDS
// depth_geometry_at itself, and drawn_radius_at now carries the folded ambient
// depth too, so draping on it would count the snowpack TWICE and lift every
// bank crest by a second ~0.7 m. The rule for any drape: sit on the drawn facet
// if you add no depth channel of your own, on the terrain facet if you do.
//
// PURE (this TU): glm + std + render/sphere_param (pure) + world/. ZERO
// raylib, so it lives in seads_render_core and test_bank_mesh pins the
// geometry headlessly (the tunnel_mesh precedent). Upload + the oreo shader +
// the draw pass live in render/ribbons.cpp (the app target).

namespace render {

struct BankBuildParams {
    double station_m = 16.0;  // longitudinal resample spacing (red-team F5)
    double skirt_m = 6.0;     // outer burial ramp run (F3)
    double skirt_bury_m = 0.5;  // skirt ring sinks this far below the facet
    // MUST be [ribbons] lift_m -- single-sourced at the call site, never a
    // second dial (F8: an independent lift steps the bank against the road).
    float lift_m = 0.45f;
    // ★ Chad's street drive ("clear the intersections"): a station whose
    // MEASURED bank amplitude (max ring depth - ambient) is below this emits
    // nothing -- the junction gaps depth_at already carries become real holes
    // in the geometry instead of flat white film across the crossings.
    double min_amp_m = 0.20;
    // ★ ROAD-REPAIR: the burial skirt is drawn as this many sub-rings across
    // skirt_m instead of one, and the strip's EXCESS over the terrain facet
    // (lift + drawn depth at the outer bank ring -> -skirt_bury_m) rides a
    // smoothstep across them. One quad could only draw the whole fall as a
    // CREASE -- a hard angular break the moment the strips are lit. At 1 this
    // is the pre-repair single-quad skirt, geometrically (not the same radial:
    // see the excess law in the .cpp).
    int skirt_rings = 3;
    // ★ ROAD-REPAIR: emit a culled station as a taper CAP and break there,
    // instead of breaking BEFORE it and leaving the strip's last full-height
    // station as an open wall. false is the pre-repair behaviour, kept as the
    // A/B arm the census reports against -- with skirt_rings 1 and
    // chord_tol_m 0 alongside it, the builder reproduces the shipped geometry
    // exactly (skirt_rings 1 puts the single skirt ring at smoothstep(1) == 1,
    // i.e. facet - skirt_bury_m, which is where it always was).
    bool taper_caps = true;
    // ★ ROAD-REPAIR: the cross-section chord tolerance (metres). The ring set
    // is refined by bisection until no chord of the composed profile misses
    // the curve by more than this. The shipped 7-knot set missed by 0.178 m at
    // the crest shoulder -- a visible facet on a 1.30 m bank.
    double chord_tol_m = 0.05;
    // ★ ROAD-REPAIR (census_after_banks.md §2, the named follow-up): the
    // SHORT station length used where the junction gap is FADING. The taper
    // cap closed the junction wall by converting a vertical drop into a fall
    // spent over ONE 16 m station -- which reads as a 4 deg ramp to the eye
    // and as a > 0.5 m step to the metric, and p50/p90 of the drawn crest step
    // paid for it (0.230 -> 0.272 / 1.020 -> 1.474). Resampling the approach
    // at this spacing instead spends the same fall over three or four short
    // stations. 0.0 = OFF and the resampler is BIT-IDENTICAL (a branch).
    //
    // ⚠ There is NO second dial for "where the gap is fading": the window is
    // [snowpack] bank_gap_m itself, the number the amplitude fade is already
    // written in, handed down by build_bank_strips below. A separate width
    // here could drift out of step with the fade it exists to resolve.
    double junction_station_m = 0.0;
    // Filled in by build_bank_strips from the SnowpackField it was handed --
    // never by a caller, and null whenever junction_station_m is 0.
    const world::LineNetwork* junction_net = nullptr;
    double junction_near_m = 0.0;
    // ★ ROAD-REPAIR, THE ONAPING ROAD-EDGE PASS, RUNG 2 -- THE DRAWN APRON.
    // Measured (docs/road_repair/onaping_edge.md §1): within 2.5 km of the
    // Valley pump 12.3 % of plowed stations have a metre or more of TERRAIN
    // falling away outward across the 6 m burial skirt (p90 1.112 m, worst
    // +4.266 m -- a 33 deg shelf). The hill is real and rung 2 may never
    // flatten it; the defect is that the drawn strip STOPS at the skirt, so
    // past the skirt the only thing the eye is shown is the planet mesh,
    // which off the corridor sits BELOW the surface the machine is placed on
    // (float_4x_m p50 -0.522 m near Valley) -- you ride out over a shelf whose
    // drawn ground is half a metre under your skis.
    //
    // The apron is drawn geometry that continues the strip outward FROM the
    // outer skirt ring ALONG THE DRIVEN SURFACE (SnowpackField::drive_radius_at
    // -- sampled, never re-derived: the one-fn-two-consumers law this whole
    // module is built on), for as far as it takes the drawn planet mesh and
    // the driven surface to agree within `apron_tol_m`, capped at `apron_m`.
    // Then it buries at `skirt_bury_m` on the SAME smoothstep the skirt uses,
    // so the apron can no more end in mid-air than the skirt can (F3).
    //
    // ⚠ apron_m == 0.0 is the IDENTITY: no apron strip is built, not one
    // vertex or index of the existing strips moves, and the builder is the
    // pre-rung-2 builder by a branch. A station whose own side's terrain drop
    // across the skirt is under `apron_min_drop_m` emits NO apron either --
    // flat ground pays nothing.
    double apron_m = 0.0;
    double apron_tol_m = 0.25;
    double apron_min_drop_m = 0.5;
    // ★ ROAD-REPAIR F1 (2026-09-12) -- THE BANK YIELDS TO THE DECK.
    // Chad, after the Onaping landing: "alot of eyesores in the intersections
    // and corners of roads, weird cut angles and verticies, z flashing all
    // over." Measured (docs/road_repair/onaping_eyesores.md §3.1): the road
    // stack has NO junction cut anywhere, and the bank foot is placed inside
    // the DRAWN asphalt at 75.6 % of stations near Valley (p95 +1.72 m, max
    // +7.46 m at the mitered corners) and past the junction fade on 95.1 % of
    // the 9,368 junction legs. That was always true; rung 8's longitudinal
    // subdivision dropped the deck off its 53 m floating chord (up to +7.2 m
    // in the air) onto the drawn terrain and stopped hiding it.
    //
    // A station whose bank rings land more than `deck_yield_m` INSIDE a drawn
    // road deck -- its own mitered corner, or a neighbouring way's -- is
    // demoted from FULL to CAP: it is emitted as a taper cap and the strip
    // BREAKS there, which is R1's machinery with a second predicate, not a
    // second mechanism.
    //
    // ★ WHY A CAP IS NOT A WALL HERE, which is the whole reason this is legal.
    // The SnowpackField already has no bank at a point inside a foreign
    // corridor: bank_profile rides e = dist - half_w of the NEAREST corridor,
    // and inside road B the nearest corridor IS B, so e < 0 and the profile is
    // zero. The rings over foreign asphalt are therefore already flat, and the
    // cap the strip ends on is flat by the field's own law -- no amplitude is
    // invented, no amplitude is removed, and the anti-fork line is untouched.
    // What is removed is COVERAGE: snow-coloured, gravel-speckled triangles
    // lying coincident with the asphalt, which is both the white wedge Chad
    // sees and the surface the depth buffer cannot separate.
    //
    // ⚠ deck_yield_m == 0.0 is the IDENTITY: the predicate is never evaluated
    // (one branch), not one vertex moves, and no query is paid for. The value
    // is a TOLERANCE, so bigger is MORE permissive: it is the depth of deck
    // overlap the bank is allowed to keep before it yields.
    double deck_yield_m = 0.0;
    // The apron's ring count is DERIVED, not a fourth dial: the apron rings
    // are laid at the skirt's own ring spacing (skirt_m / skirt_rings), so an
    // apron quad is the same size as a skirt quad. Clamped to [1, 6].
    int max_apron_rings = 6;
    int max_rings = 16;  // refinement ceiling (before the skirt rings)
    // u16 ceiling. The builder clamps this to 65535/rings-per-station itself,
    // so a denser section can never overflow the u16 index type (it would
    // have, silently, at 15 rings x 8000 stations).
    int max_stations_per_mesh = 8000;
};

// One drawable strip chunk (one side of one run, <= max_stations_per_mesh).
struct BankStripCPU {
    std::vector<float> pos;           // xyz world-absolute, 3 per vert
    std::vector<float> uv;            // (s_m, v) -- v = e/(rise+fall); the
                                      // skirt rings ride 1.0 -> 1.25
    std::vector<float> nrm;           // ★ ROAD-REPAIR: unit normals, 3 per vert
    std::vector<std::uint16_t> idx;   // triangle list, local
    // ★ ROAD-REPAIR RUNG 2: the strip's own rings-per-station, carried rather
    // than passed alongside, because the APRON strips have a different ring
    // count from the bank strips and a reader that assumed one number for the
    // whole batch (bank_strip_continuity did) would mis-read them.
    int rings = 0;
    bool is_apron = false;
};

// ★ ROAD-REPAIR -- THE DRAWN-BANK CONTINUITY METRIC. Measured on the BUILT
// strips (never re-derived from the dials), so it reports the mesh Chad sees:
//
//  * `step`      -- |radial(i+1,r) - radial(i,r)| between consecutive stations
//                   of one strip at the SAME ring index. This is the jag: a
//                   strip that reads as continuous has steps of the order of
//                   the driven surface's own station-to-station change.
//  * `end_amp`   -- at each strip's two terminal stations, the height the bank
//                   still stands above its own outer ring. An open end is a
//                   vertical white wall of exactly this height; a tapered cap
//                   is bounded by min_amp_m by construction.
struct BankStripContinuity {
    long pairs = 0;
    long steps_over_50cm = 0;
    double step_p50 = 0.0, step_p90 = 0.0, step_p99 = 0.0, step_max = 0.0;
    // ★ THE COMPARABLE ONE. `step_*` above is a MAX OVER RINGS, so a denser
    // ring set reports a larger number for the same surface -- it cannot be
    // put beside the census's driven step, which is ONE lateral. `crest_*` is
    // the step at the single ring nearest the crest, which is exactly the
    // lateral the census samples its own crest row at.
    long crest_pairs = 0;
    double crest_p50 = 0.0, crest_p90 = 0.0, crest_p99 = 0.0, crest_max = 0.0;
    long strip_ends = 0;
    double end_amp_p50 = 0.0, end_amp_p99 = 0.0, end_amp_max = 0.0;
};

// `nr` is rings-per-station (bank_ring_offsets().size() + skirt_rings), i.e.
// the same number the builder used. `crest_v` is the ring-normalized crest
// position (BankSurfaces::crest_v -- the ring whose composed profile is
// tallest); negative leaves the crest_* fields at zero. Pure.
BankStripContinuity bank_strip_continuity(
    const std::vector<BankStripCPU>& strips, int nr, double crest_v = -1.0);

// The cross-section ring offsets (metres beyond the ribbon edge), derived from
// the live [snowpack] dials: the sorted union of the BANK knots and the
// FEATHER knots (red-team F2 -- the true crest of the composed curve sits
// where the feather is still rising while the bank has barely fallen, NOT at
// `rise`). The skirt rings are appended by the builder, not listed here.
//
// ★ ROAD-REPAIR: the union set is then REFINED by bisection until no chord of
// the composed section misses the curve by more than `chord_tol_m` (0 or
// negative disables the refinement and returns the shipped 7-knot union). The
// composed section it is measured against is `feather_amp * smoothstep(e/ce) +
// bank_profile(e)` -- the same two families the union was built from, in
// metres, with `feather_amp = base_m - road_bare_m` standing in for the
// per-station ambient (the ring OFFSETS are one list for every station on the
// map, so the refinement criterion has to be one list too; the feather's
// AMPLITUDE is the only part that varies, and base_m is its shipped scale).
std::vector<double> bank_ring_offsets(const world::SnowParams& sp,
                                      double chord_tol_m = 0.0,
                                      int max_rings = 16);

// ★ ROAD-REPAIR RUNG 2 -- THE APRON'S REACH, as ONE function so the mesh and
// the census can never disagree about where the apron is (the census's
// `apron_gap_m` has to know which offsets are covered by drawn geometry; if it
// re-derived that rule it would be a ruler measuring a different mesh).
//
//   `up`        the station's centreline direction (unit).
//   `out_dir`   the unit outward tangential, ALREADY side-signed.
//   `foot_off`  half_w + the outermost BANK ring offset -- the bank foot, the
//               same offset render::road_census measures skirt_drop_m at.
//   `skirt_m`   the burial skirt run; the apron starts at foot_off + skirt_m.
//   `cap_m`     the maximum reach. The MESH passes BankBuildParams::apron_m;
//               the CENSUS passes its own probe cap, because a ruler has to be
//               able to say how far the apron WOULD have had to go.
//
// Returns 0.0 when the station does not qualify (its own side's terrain drop
// across the skirt is under `apron_min_drop_m`, or cap_m <= 0), otherwise the
// reach in metres, QUANTIZED TO THE RING GRID -- the mesh cannot express a
// reach between two rings, so the ruler may not claim one either.
double bank_apron_reach_m(const world::SnowpackField& snow,
                          const std::function<double(glm::dvec3)>& facet_r,
                          const std::function<double(glm::dvec3)>& drawn_r,
                          glm::dvec3 up, glm::dvec3 out_dir, double foot_off,
                          double skirt_m, double apron_tol_m,
                          double apron_min_drop_m, int apron_rings,
                          double cap_m);

// The apron's ring count for a given dial set -- derived from the skirt's own
// ring spacing, never a dial of its own. See BankBuildParams::max_apron_rings.
int bank_apron_ring_count(const BankBuildParams& p);

// ★ ROAD-REPAIR F1 -- WHAT THE YIELD COST, read off the build that just ran
// rather than re-derived. `stations` is the number of FULL bank stations the
// predicate was asked about (it is not asked when deck_yield_m is 0, so a zero
// here is also how you tell the identity apart from a clear map); `capped` is
// how many of those were demoted to a taper cap. The penetration percentiles
// are over ALL `stations`, bucketed at 5 cm, so one armed run at any value
// reports the whole distribution and the dial can be chosen from it instead of
// guessed. Thread-safe (relaxed atomics), and order-independent: the numbers
// do not depend on how the run pool happened to schedule.
struct BankDeckYieldStat {
    long long stations = 0;
    long long capped = 0;
    double pen_p50 = 0.0, pen_p90 = 0.0, pen_p99 = 0.0, pen_max = 0.0;
};
BankDeckYieldStat bank_deck_yield_stat();
void reset_bank_deck_yield_stat();

// ★ ROAD-REPAIR F1 -- THE DIAL SWEEP, off ONE armed run. The share of measured
// stations whose penetration exceeds `thresh_m`, i.e. the share that WOULD
// yield at that dial value. The histogram is independent of the value the
// build actually ran at (every FULL station is measured, then compared), so a
// single armed build answers "what does 0.10 cost, what does 1.00 cost" for
// every candidate -- which is how the shipped value was chosen rather than
// guessed. Returns 0 when nothing was measured.
double bank_deck_yield_fraction_over(double thresh_m);

// ★ THE INJECTABLE CORE (the test seam). One continuous RUN of one road:
// per-station centerline dirs + arc s (m) + half-width (m), already resampled.
// Emits both sides' strip chunks into `out`. Stations inside a CutDisk are
// skipped (F4 -- the ribbon T24 mirror), which may split a run into more
// chunks. Returns the number of vertices emitted.
long build_bank_run(const std::vector<glm::dvec3>& ctr,
                    const std::vector<float>& s_arc,
                    const std::vector<double>& half_w, const HeightField& hf,
                    int subdiv, int tiles, const world::SnowpackField& snow,
                    const BankBuildParams& p,
                    const std::vector<CutDisk>& cuts,
                    std::vector<BankStripCPU>& out);

// ★ ONE RECOVERED RUN of one baked ribbon path: the centreline stations, the
// arc-length s of each, and the per-station half-width -- already resampled to
// BankBuildParams::station_m. This is the ONE resampling; build_bank_strips
// and the road-gap census both consume it, so the census can never measure a
// station set the banks were not built on ("one function, two consumers, zero
// fork").
struct BankRun {
    int path_id = -1;  // index into kSudburyRibbonPaths
    int kind = 0;      // GisRibbonPath::kind (0 road_major .. 3 river)
    std::vector<glm::dvec3> ctr;
    std::vector<float> s;
    std::vector<double> hw;
};

// Recover + resample every run of every baked path whose kind is in `kinds`.
// PURE (reads only the baked table + hf.R), which is what lets the census
// re-walk exactly the bank stations without a second densify rule.
std::vector<BankRun> recover_bank_runs(const HeightField& hf,
                                       const BankBuildParams& p,
                                       const std::vector<int>& kinds);

// The baked iterator: walks kSudburyRibbonPaths, kinds 0/1 (PLOWED roads)
// only -- trails are groomed not plowed (§2.4c), rivers are water. Recovers
// centerline + half-width PER STATION from the drawn L/R vertex pairs (INV-6,
// the linework recovery -- never the ribbons' per-path median), BREAKS runs
// where arc s is non-increasing (red-team F1: the three "paths" are
// concatenated batches of thousands of streets), and resamples each run at
// station_m. `ms_out`/`verts_out` feed the build log (F5: no silent budgets).
std::vector<BankStripCPU> build_bank_strips(const HeightField& hf, int subdiv,
                                            int tiles,
                                            const world::SnowpackField& snow,
                                            const BankBuildParams& p,
                                            const std::vector<CutDisk>& cuts,
                                            long* verts_out = nullptr);

}  // namespace render
