#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <vector>

#include "render/sphere_param.h"  // HeightField, facet_radius_at
#include "world/snowpack.h"

// ★ SF3-A — THE RIDER SNOW PATCH (Chad, 2026-08-26: "are we gonna build the
// snow now so its visible? So that it deforms when we drive through it?").
//
// THE DEFECT IT CLOSES. SnowpackField::depth_at is computed, and the sled
// DRIVES it, but nothing ever DRAWS it: the planet mesh is built from the DEM
// alone at ~59 m cells (config/world.toml:601-602) and render/planet.cpp:255
// says so in its own words -- "today's snowCover is a render-only
// approximation ... that predates the depth function, so this is a stub on a
// stub, kept only so the layer is visible before S2 exists." S2 exists. The
// measured consequence is that the rider drives over a p50 0.682 m field of
// snow relief (docs/snow_build_handoff.md, the SF3-0 census) and sees a flat
// grey sheet.
//
// THE PATTERN IS NOT NEW. render/bank_mesh.h:17 already draws the plow banks
// as facet_radius_at(dir) + lift + depth_at(dir) -- the rendered terrain facet
// plus THE REAL FIELD, sampled, never re-derived. This is that formula on a
// disc that follows the rider instead of a strip beside the road, so it cannot
// fork from the physics for exactly the same reason.
//
// ★ WHY IT CANNOT FLASH. The jump build died half-buried and flashing (§1 of
// the handoff) because a drawn mesh and the driven surface disagreed. This
// patch sits on facet_radius_at -- the SAME quantity the surrounding planet
// mesh interpolates -- plus a strictly positive lift. MEASURED on this tree at
// the shipped [planet] subdiv/tiles: field-minus-facet is p50 +0.091 m, worst
// case -0.086 m, so a lift_m of 0.45 clears the planet mesh by >= 0.36 m
// everywhere. The two surfaces cannot intersect, so there is no z-fight to
// fight. The rim is buried BELOW the facet (the bank_mesh skirt precedent), so
// the patch has no visible edge either.
//
// ★ WHY IT REFRESHES INCREMENTALLY. MEASURED on this tree: depth_at costs
// ~1.06 us/call (ambient_depth_at ~0.77 us -- the cost is the finite-difference
// slope/curvature sampling, not the corridor search, so it does not optimize
// away). A full 96x96 rebuild is 9.8 ms, 59% of the whole 60 Hz budget. So the
// expensive pass (radius) fills a few ROWS per frame into a back buffer and
// swaps when complete; the cheap pass (positions + normals, pure arithmetic)
// runs once at the swap. Cost is bounded by rows_per_frame, never by n_side.
//
// PURE (this TU): glm + std + render/sphere_param (pure) + world/. ZERO
// raylib, so it lives in seads_render_core and can be pinned headlessly --
// the bank_mesh / tunnel_mesh precedent.

namespace render {

struct SnowPatchParams {
    // Grid. snow_patch_indices returns u16 (raylib's Mesh::indices is
    // unsigned short), so n_side^2 must stay under 65,535 -- i.e. n_side <=
    // 255. Today 160 -> 25,600 verts, so there is real headroom here, and it
    // is the ONLY lever on the draw-side serration (handoff R4 §5.1).
    int n_side = 160;
    // ★ SIZED BY THE TRACK, NOT BY THE AMBIENT FIELD. The ambient depth field
    // was the original target, but it is smooth at 40 m (measured: 2.6 cm per
    // 2 m step) so ANY cell size draws it identically. A RUT is ~1.2 m across,
    // so the cell has to be a fraction of that or the deformation aliases into
    // nothing -- the feature the mesh exists to show would be invisible at the
    // resolution chosen for the feature it cannot show.
    // 160 x 0.5 m = a 79.5 m patch; 160^2 = 25,600 verts, inside the u16 cap.
    double cell_m = 0.5;
    // Clearance of the patch interior above the PLANET MESH. Small on purpose:
    // the patch no longer carries the ambient snow depth (see the .cpp), so
    // this is the entire offset between the patch and the world around it, and
    // any visible amount of it would be the square edge all over again. Floor:
    // it must exceed the deepest groove (depress_m 0.12) or the patch dips
    // through the planet mesh and z-fights -- the flashing class.
    double lift_m = 0.15;
    // The outermost ring sinks this far BELOW the facet so the patch ends
    // inside the terrain and shows no edge (bank_mesh skirt_bury_m precedent).
    // Only needs to clear lift_m now that the interior sits low.
    double skirt_bury_m = 0.25;
    // Depth and lift fade to zero across this many cells at the rim, so the
    // patch meets the surrounding planet mesh tangentially rather than in a
    // step.
    double rim_fade_cells = 10.0;
    // Rows of the radius grid filled per frame, and the ONE dial that bounds
    // the cost: 16 rows of 160 nodes at the measured ~1.06 us/call was ~2.7 ms.
    // It also sets the refresh latency (n_side/rows_per_frame frames), which is
    // what decides how promptly a fresh rut appears behind the machine.
    //
    // ★ R4b: HALVED, because the radius pass now also evaluates draw_fold_at
    // per node (the patch sits on the surface its neighbours draw -- see the
    // .cpp). Measured on this tree, that roughly doubled the per-node cost, so
    // 8 holds the SAME per-frame budget and pays for it in latency instead:
    // 20 frames, ~0.33 s, ~5 m of travel at 15 m/s on an 80 m patch. If the
    // fold is ever cached or the patch resolution is raised (handoff R4 §5.1),
    // re-measure this rather than assuming it.
    //
    // ★ R5 row 3: the radius pass ALSO calls TrackField::compaction_at per
    // node now (one extra nearest() -- a 3x3x3 hash-grid probe, no field
    // sampling). Measured against the pass's dominant costs (facet_radius_at
    // + the fold), it is the same order as the deform_at call the pass already
    // makes; if a profile ever shows the radius pass over budget, the honest
    // fix is a combined deform+compaction query on TrackField (both run off
    // the SAME Nearest), not dropping the tint to the cheap pass.
    int rows_per_frame = 16;
    // Side of the coarse ambient-fold lattice (see SnowPatchBuild::fold).
    //
    // It WAS 9 -- a ~9.9 m step across the 79.5 m patch, four times finer than
    // the 40 m scale the ambient field is smoothed at, so the interpolation
    // error was far below the millimetre the draw could show. That reasoning
    // was sound for an ambient field with no features finer than 40 m, and L7
    // put one in it.
    //
    // ★★★ L7: THE TRAMPLED DISK IS 3.5 m WITH A 1.5 m FEATHER, so the whole
    // hollow is 10 m across -- ONE lattice step at 9. A feature that lands
    // between two nodes is bilinearly averaged into nothing, and the gun's pad
    // would be trampled in the DRIVEN surface and still drawn buried. (It
    // cannot be caught by testing at the patch anchor: 9 is ODD, so there is a
    // node exactly at the centre and a disk centred on the anchor survives by
    // coincidence. The gunner's eye is NOT at the gun.)
    //
    // 33 = a 2.484 m step, so the 3.5 m flat holds at least one node in every
    // placement (the worst-case anchor offset puts the four nearest nodes
    // 1.76 m out) and the drawn hollow cannot be averaged away.
    //
    // ★ THE COST IS MEASURED, NOT ASSUMED (900-frame --smoke on this tree,
    // baseline re-run twice for the noise floor):
    //     9  -> avg 3.148 / 3.178 / 3.180 ms   p95 5.20 ms
    //     33 -> avg 3.497 ms                    p95 8.97 ms   (+0.32 ms/frame)
    //     41 -> avg 4.007 ms                    p95 11.91 ms  (+0.83 ms/frame)
    // 41 (a 2.0 m step) was the first choice and it is measurably expensive, so
    // 33 is the coarsest lattice at or under 2.5 m that keeps the disk. The
    // cost is a BURST, not a spread: snow_patch_begin fills the whole lattice
    // on ONE frame of each refresh (n_side/rows_per_frame = 10), which is why
    // p95 moves four times as far as the average. Amortizing that fill the way
    // the radius pass already is would buy back the hitch and is the honest
    // next move if the patch resolution is ever raised again -- measure it,
    // do not assume it.
    int fold_lat_n = 33;
};

// A local tangent basis at the patch anchor. The grid is laid out in THIS
// frame, so it is rebuilt whenever the patch re-anchors -- never carried.
struct SnowPatchFrame {
    glm::dvec3 c{0.0, 0.0, 1.0};  // anchor direction (unit)
    glm::dvec3 e{1.0, 0.0, 0.0};  // tangent "east" (unit, perp c)
    glm::dvec3 n{0.0, 1.0, 0.0};  // tangent "north" (unit, perp c and e)
};

// Build a tangent basis at `anchor_dir`. The seed axis is chosen off the
// anchor itself (never a fixed world axis -- the sphere-invariant rule), so
// there is no pole where the basis degenerates.
SnowPatchFrame snow_patch_frame(glm::dvec3 anchor_dir);

// The node direction at grid coordinate (ix, iy), by the exponential map on a
// sphere of radius R -- a great-circle step, not a gnomonic one, so the grid
// stays metric-true out to the rim instead of stretching with distance.
glm::dvec3 snow_patch_dir(const SnowPatchFrame& f, const SnowPatchParams& p,
                          int ix, int iy, double R);

// The rim weight at (ix, iy): 1 in the interior, smoothstepping to 0 over
// rim_fade_cells, and exactly 0 on the outermost ring (which is buried).
double snow_patch_rim_weight(const SnowPatchParams& p, int ix, int iy);

// The in-progress build. `radius` is the expensive product: one facet_radius_at
// plus one depth_at per node.
struct SnowPatchBuild {
    SnowPatchFrame frame;
    std::vector<double> radius;  // n_side*n_side, planet-local metres
    // ★ R4b: THE AMBIENT FOLD, ON A COARSE LATTICE, BILINEARLY INTERPOLATED.
    // The patch has to sit on the folded surface (see the .cpp) or it draws
    // below the world around it -- but evaluating draw_fold_at per 0.5 m node
    // measured 4.4x the whole patch cost (1.16 -> 5.10 ms/frame, RelWithDebInfo
    // on this box), which is a third of a 60 Hz budget for a term that CANNOT
    // vary at that scale: [snowpack] curv_probe_m is 40 m and world/snowpack.h
    // states the ambient function IS the homogenizer -- 2.6 cm of change per
    // 2 m, measured. So it is sampled on a (fold_lat_n)^2 lattice spanning the
    // whole patch and interpolated. 81 calls per refresh instead of 25,600.
    std::vector<double> fold;  // fold_lat_n * fold_lat_n
    // ★ R5 row 3 — PER-NODE PACKED-SNOW COMPACTION, NORMALIZED [0,1]. Filled by
    // the EXPENSIVE pass (snow_patch_step -- it is a TrackField lookup, and the
    // cheap emit sweep is pure arithmetic by contract), emitted as texcoord.y.
    // TrackField::compaction_at returns METRES (n.depress_m * groove, i.e. 0 up
    // to the stamp's own cut depth); the normalizer is TrackParams::
    // min_depress_m (0.55, the Chad-signed readability floor), clamped to 1, so
    // a full floor-depth rut reads 1.0 and a thin-snow cut reads
    // proportionally less. Already scaled by the rim weight here so the tint
    // fades out exactly where the track geometry does. Exactly 0.0 wherever
    // compaction_at is exactly 0.0 (0/x == 0, clamp keeps it) -- the fence-5
    // bit-identical property starts in this buffer.
    std::vector<float> pack;  // n_side*n_side, normalized compaction
    int fold_n = 0;
    int rows_done = 0;
    int n_side = 0;
    double R = 0.0;  // the sphere the grid is laid on (hf.R)
    bool complete() const { return n_side > 0 && rows_done >= n_side; }
};

// Start a fresh build anchored at `anchor_dir`. Allocates and resets progress.
void snow_patch_begin(SnowPatchBuild& b, const SnowPatchParams& p,
                      glm::dvec3 anchor_dir, double R,
                      const world::SnowpackField* field = nullptr);

// Fill up to `p.rows_per_frame` more rows of b.radius. THE EXPENSIVE CALL.
// Returns the number of rows filled this call (0 when already complete).
int snow_patch_step(SnowPatchBuild& b, const SnowPatchParams& p,
                    const world::HeightField& hf, int subdiv, int tiles,
                    const world::SnowpackField& field);

// The cheap sweep: a completed radius grid becomes planet-local float
// positions, outward normals, and a texcoord whose .x carries the rim weight
// (the shader reads it to blend this patch's own normals in without a seam)
// and whose .y carries the normalized track compaction (R5 row 3 -- free on
// the patch pass because draw_snow_patch forces uSnowDepthMix to 0 there).
// Pure arithmetic -- no field sampling (compaction was already resolved into
// SnowPatchBuild::pack by the expensive pass).
void snow_patch_emit(const SnowPatchBuild& b, const SnowPatchParams& p,
                     std::vector<float>& pos, std::vector<float>& nrm,
                     std::vector<float>& uv);

// The triangle list for an n_side grid. Static for a given n_side.
std::vector<std::uint16_t> snow_patch_indices(int n_side);

}  // namespace render
