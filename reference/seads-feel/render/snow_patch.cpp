#include "render/snow_patch.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace render {

namespace {

// Grid coordinate -> tangent offset in metres, centred on the anchor.
inline double node_offset_m(const SnowPatchParams& p, int i) {
    return (static_cast<double>(i) - 0.5 * (p.n_side - 1)) * p.cell_m;
}

inline double smoothstep01(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

}  // namespace

SnowPatchFrame snow_patch_frame(glm::dvec3 anchor_dir) {
    SnowPatchFrame f;
    f.c = glm::normalize(anchor_dir);
    // Seed the basis off the anchor's own smallest component rather than a
    // fixed world axis: a fixed axis degenerates where it aligns with c, which
    // is exactly the "no fixed axis anywhere in the tree" sphere invariant.
    const glm::dvec3 a(std::fabs(f.c.x), std::fabs(f.c.y), std::fabs(f.c.z));
    glm::dvec3 seed(1.0, 0.0, 0.0);
    if (a.y <= a.x && a.y <= a.z)
        seed = glm::dvec3(0.0, 1.0, 0.0);
    else if (a.z <= a.x && a.z <= a.y)
        seed = glm::dvec3(0.0, 0.0, 1.0);
    f.e = glm::normalize(glm::cross(seed, f.c));
    f.n = glm::normalize(glm::cross(f.c, f.e));
    return f;
}

// The (fractional) grid coordinate of coarse lattice index j. The lattice
// spans the SAME extent as the fine grid, endpoints included, so the corners
// coincide and nothing is extrapolated.
double lat_to_node(const SnowPatchParams& p, int fold_n, int j) {
    if (fold_n <= 1) return 0.5 * (p.n_side - 1);
    return static_cast<double>(j) * (p.n_side - 1) /
           static_cast<double>(fold_n - 1);
}

// snow_patch_dir at a FRACTIONAL grid coordinate. The coarse fold lattice does
// not land on integer nodes (159/8 = 19.875 per step), and rounding it would
// make the sample positions disagree with the positions the interpolation
// below assumes -- a small, permanent, invisible bias in the term that decides
// whether the patch sits on the world or under it.
glm::dvec3 patch_dir_f(const SnowPatchFrame& f, const SnowPatchParams& p,
                       double fx, double fy, double R) {
    const double x = (fx - 0.5 * (p.n_side - 1)) * p.cell_m;
    const double y = (fy - 0.5 * (p.n_side - 1)) * p.cell_m;
    const double r = std::sqrt(x * x + y * y);
    if (r < 1e-12) return f.c;
    const glm::dvec3 u = (f.e * (x / r)) + (f.n * (y / r));
    const double ang = r / R;
    return glm::normalize(f.c * std::cos(ang) + u * std::sin(ang));
}

glm::dvec3 snow_patch_dir(const SnowPatchFrame& f, const SnowPatchParams& p,
                          int ix, int iy, double R) {
    const double x = node_offset_m(p, ix);
    const double y = node_offset_m(p, iy);
    const double r = std::sqrt(x * x + y * y);
    if (r < 1e-12) return f.c;
    const glm::dvec3 u = (f.e * (x / r)) + (f.n * (y / r));
    const double ang = r / R;
    return glm::normalize(f.c * std::cos(ang) + u * std::sin(ang));
}

double snow_patch_rim_weight(const SnowPatchParams& p, int ix, int iy) {
    const double edge = 0.5 * (p.n_side - 1);
    // Chebyshev distance -> a square rim, matching the square grid, so the
    // fade band has uniform width all the way round.
    const double d = std::max(std::fabs(ix - edge), std::fabs(iy - edge));
    if (d >= edge) return 0.0;  // the buried outermost ring
    const double fade = std::max(1e-9, p.rim_fade_cells);
    return smoothstep01((edge - d) / fade);
}

void snow_patch_begin(SnowPatchBuild& b, const SnowPatchParams& p,
                      glm::dvec3 anchor_dir, double R,
                      const world::SnowpackField* field) {
    b.frame = snow_patch_frame(anchor_dir);
    b.n_side = p.n_side;
    b.R = R;
    b.radius.assign(static_cast<std::size_t>(p.n_side) * p.n_side, 0.0);
    // R5 row 3: compaction rides beside the radius, filled by the same
    // expensive pass. Zeroed here so a null-tracks build emits exactly 0.0f
    // into texcoord.y -- the pre-R5 value, bit-identically.
    b.pack.assign(static_cast<std::size_t>(p.n_side) * p.n_side, 0.0f);
    b.rows_done = 0;
    // ★ R4b: fill the coarse fold lattice ONCE per refresh, here, where the
    // frame is fixed. Null field (every headless fixture) leaves it empty and
    // fold_at() below returns 0 -- i.e. bit-identical to the pre-R4b patch.
    b.fold_n = std::max(2, p.fold_lat_n);
    b.fold.assign(static_cast<std::size_t>(b.fold_n) * b.fold_n, 0.0);
    if (field == nullptr) {
        b.fold_n = 0;
        b.fold.clear();
        return;
    }
    for (int jy = 0; jy < b.fold_n; ++jy)
        for (int jx = 0; jx < b.fold_n; ++jx) {
            const glm::dvec3 d =
                patch_dir_f(b.frame, p, lat_to_node(p, b.fold_n, jx),
                            lat_to_node(p, b.fold_n, jy), R);
            b.fold[static_cast<std::size_t>(jy) * b.fold_n + jx] =
                field->draw_fold_at(d);
        }
}

namespace {
// Bilinear read of the coarse fold lattice at fine-grid node (ix, iy). Empty
// lattice (no field injected) reads 0, which is the pre-R4b patch exactly.
double fold_at_node(const SnowPatchBuild& b, const SnowPatchParams& p, int ix,
                    int iy) {
    if (b.fold_n < 2 || b.fold.empty()) return 0.0;
    const double span = static_cast<double>(p.n_side - 1);
    const double step = span / static_cast<double>(b.fold_n - 1);
    const double gx = std::clamp(ix / step, 0.0, b.fold_n - 1.0);
    const double gy = std::clamp(iy / step, 0.0, b.fold_n - 1.0);
    const int x0 = std::min(b.fold_n - 2, static_cast<int>(gx));
    const int y0 = std::min(b.fold_n - 2, static_cast<int>(gy));
    const double tx = gx - x0;
    const double ty = gy - y0;
    const auto at = [&](int x, int y) {
        return b.fold[static_cast<std::size_t>(y) * b.fold_n + x];
    };
    const double a = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * tx;
    const double c =
        at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * tx;
    return a + (c - a) * ty;
}
}  // namespace

int snow_patch_step(SnowPatchBuild& b, const SnowPatchParams& p,
                    const world::HeightField& hf, int subdiv, int tiles,
                    const world::SnowpackField& field) {
    if (b.complete()) return 0;
    const int row0 = b.rows_done;
    const int row1 = std::min(b.n_side, row0 + std::max(1, p.rows_per_frame));
    for (int iy = row0; iy < row1; ++iy) {
        for (int ix = 0; ix < b.n_side; ++ix) {
            const glm::dvec3 d = snow_patch_dir(b.frame, p, ix, iy, b.R);
            // ★ THE PATCH CARRIES ONLY WHAT THE WORLD CANNOT DRAW.
            //
            // It used to sit on drive_radius_at -- "draw exactly what you
            // drive" -- and that was wrong, for a reason worth writing down
            // because it is not obvious: the driven surface is ground PLUS the
            // ambient snow depth (0.71 m of it in bush), and the surrounding
            // planet mesh has NO snow in it whatsoever. So the patch became a
            // ~0.9 m slab standing proud of the rest of the world, and the rim
            // fade turned that into a visible SQUARE. Chad, on the drive:
            // "there are no ruts, its depressing the whole square area."
            //
            // The ambient field does not need this mesh anyway: it is smooth
            // at 40 m ([snowpack] curv_probe_m) -- 2.6 cm of change per 2 m,
            // measured -- so it carries no detail a 0.5 m cell could show that
            // the 59 m planet mesh could not. What the planet mesh genuinely
            // CANNOT represent is the deformation: a rut is a 0.12 m step
            // across 0.6 m. That, and only that, is what this mesh is for.
            //
            // So: the planet mesh's own surface, a small clearance, and the
            // track. The patch is now seamless by construction -- interior and
            // rim differ by at most lift_m -- and it cannot make a slab.
            // ★ R4b: THE PATCH SITS ON THE SURFACE ITS NEIGHBOURS DRAW.
            //
            // The comment below says "the surrounding planet mesh has NO snow
            // in it whatsoever". That was true when it was written and R1 made
            // it FALSE: the planet mesh now carries the ambient fold. So the
            // patch, sitting on the bare facet plus 0.15 m, has been drawing
            // ~0.56 m BELOW the world around it -- MEASURED on this tree,
            // fold 0.713 m against lift_m 0.150 -- and the rut proper another
            // 0.55 m under that. It is also a fork in the sense this codebase
            // exists to forbid, and the arithmetic says so: the DRIVEN surface
            // is facet + fold + track (probe: drive_vs_mesh = -0.550 m, i.e.
            // the rut is half a metre below the drawn snow), while the patch
            // drew facet + track. Draw and drive disagreed by the whole fold.
            //
            // Adding the fold makes the patch and the planet mesh agree
            // OFF-track by construction -- which is exactly what killed the
            // original "0.9 m slab / depressing the whole square area" defect,
            // and why that defect does NOT come back: the slab existed because
            // the patch carried snow the neighbours did not. Now they both do.
            //
            // ★ AND THE FOLD IS NOT RIM-FADED. Only lift and the track fade at
            // the rim. Fading the fold would sink the patch edge ~0.7 m below
            // the planet mesh and cut a visible crater rim -- the square, in
            // negative.
            const double base = facet_radius_at(hf, d, subdiv, tiles) +
                                fold_at_node(b, p, ix, iy);
            const double w = snow_patch_rim_weight(p, ix, iy);
            // THE ANTI-FORK still holds where it matters: this is the SAME
            // TrackField the sled's own contact patch reads through
            // SnowpackField, never a second copy of the deformation.
            const double track =
                field.tracks != nullptr ? field.tracks->deform_at(d) : 0.0;
            // ★ R5 row 3 -- THE TRACK READS. The cut has geometry (above) and,
            // until this, zero tonal response: white-on-white under a high sun,
            // no shadow into a 0.55 m groove. compaction_at is the signal the
            // ledger says was thrown away -- same field, same direction, the
            // anti-fork by construction. It returns METRES (0 up to the
            // stamp's cut depth); normalized by min_depress_m (the Chad-signed
            // 0.55 m readability floor) so a floor-depth rut is 1.0, clamped
            // for the deep-bush cuts that exceed the floor. Scaled by the rim
            // weight so the tint fades out exactly where the track geometry
            // does (`track` is scaled by the same w below) -- no tinted stripe
            // running past the patch edge. At zero compaction every factor is
            // exactly 0.0, which is what fence 5 demands of this buffer.
            if (field.tracks != nullptr && w > 0.0) {
                const double norm = field.tracks->params().min_depress_m;
                if (norm > 0.0) {
                    const double c =
                        std::clamp(field.tracks->compaction_at(d) / norm, 0.0,
                                   1.0);
                    b.pack[static_cast<std::size_t>(iy) * b.n_side + ix] =
                        static_cast<float>(c * w);
                }
            }
            double r;
            if (w <= 0.0) {
                // The buried ring tucks under the planet mesh, which then
                // occludes it -- the patch has no visible edge.
                r = base - p.skirt_bury_m;
            } else {
                // lift_m keeps the interior off the planet mesh where there
                // is no track, so the two are never coplanar and cannot
                // z-fight.
                //
                // ⚠ R4b: THE OLD INVARIANT HERE IS DEAD AND MUST NOT BE
                // "RESTORED". It read: the groove is -0.12 and the berm +0.05,
                // so the interior stays strictly above base for any lift above
                // ~0.13 m. R4 cut the groove to 0.55 m, so lift_m 0.15 no
                // longer clears it and no sane lift would -- a 0.6 m lift is
                // the visible slab Chad already rejected. What replaced it is
                // structural, not a margin: `base` now carries the ambient
                // fold, so the rut is a depression IN the drawn snowpack (the
                // same one the sled drives), with ~0.7 m of snow above the
                // facet for it to cut into. The rut is SUPPOSED to go below
                // the surrounding snow surface -- that is what a rut is.
                r = base + (p.lift_m + track) * w;
            }
            b.radius[static_cast<std::size_t>(iy) * b.n_side + ix] = r;
        }
    }
    b.rows_done = row1;
    return row1 - row0;
}

void snow_patch_emit(const SnowPatchBuild& b, const SnowPatchParams& p,
                     std::vector<float>& pos, std::vector<float>& nrm,
                     std::vector<float>& uv) {
    const int n = b.n_side;
    const std::size_t nv = static_cast<std::size_t>(n) * n;
    pos.assign(nv * 3, 0.0f);
    nrm.assign(nv * 3, 0.0f);
    uv.assign(nv * 2, 0.0f);
    std::vector<glm::dvec3> pt(nv);
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix) {
            const std::size_t k = static_cast<std::size_t>(iy) * n + ix;
            pt[k] = snow_patch_dir(b.frame, p, ix, iy, b.R) * b.radius[k];
        }
    for (int iy = 0; iy < n; ++iy)
        for (int ix = 0; ix < n; ++ix) {
            const std::size_t k = static_cast<std::size_t>(iy) * n + ix;
            // Central differences where they exist, one-sided at the border.
            const std::size_t kxp =
                static_cast<std::size_t>(iy) * n + std::min(n - 1, ix + 1);
            const std::size_t kxm =
                static_cast<std::size_t>(iy) * n + std::max(0, ix - 1);
            const std::size_t kyp =
                static_cast<std::size_t>(std::min(n - 1, iy + 1)) * n + ix;
            const std::size_t kym =
                static_cast<std::size_t>(std::max(0, iy - 1)) * n + ix;
            glm::dvec3 nvec = glm::cross(pt[kxp] - pt[kxm], pt[kyp] - pt[kym]);
            const double len = glm::length(nvec);
            // Degenerate only if the patch is perfectly flat AND collapsed;
            // fall back to the radial, which is the correct normal there.
            nvec = (len > 1e-12) ? (nvec / len) : glm::normalize(pt[k]);
            // Orient outward. The cross product's sign depends on the frame's
            // handedness, which is fixed by snow_patch_frame -- but asserting
            // it here costs nothing and survives a frame change.
            if (glm::dot(nvec, glm::normalize(pt[k])) < 0.0) nvec = -nvec;

            pos[k * 3 + 0] = static_cast<float>(pt[k].x);
            pos[k * 3 + 1] = static_cast<float>(pt[k].y);
            pos[k * 3 + 2] = static_cast<float>(pt[k].z);
            nrm[k * 3 + 0] = static_cast<float>(nvec.x);
            nrm[k * 3 + 1] = static_cast<float>(nvec.y);
            nrm[k * 3 + 2] = static_cast<float>(nvec.z);
            // .x carries the rim weight so the fragment shader can fade this
            // patch's own normals in without a seam; .y carries the normalized
            // track compaction (R5 row 3). The channel is free on the patch
            // pass BECAUSE draw_snow_patch forces uSnowDepthMix to 0 there --
            // the depth exposure never reads it on this mesh. A build whose
            // pack buffer was never sized (a hand-rolled fixture) emits the
            // pre-R5 0.0f.
            uv[k * 2 + 0] =
                static_cast<float>(snow_patch_rim_weight(p, ix, iy));
            uv[k * 2 + 1] = (k < b.pack.size()) ? b.pack[k] : 0.0f;
        }
}

std::vector<std::uint16_t> snow_patch_indices(int n_side) {
    std::vector<std::uint16_t> idx;
    if (n_side < 2) return idx;
    idx.reserve(static_cast<std::size_t>(n_side - 1) * (n_side - 1) * 6);
    for (int iy = 0; iy + 1 < n_side; ++iy)
        for (int ix = 0; ix + 1 < n_side; ++ix) {
            const auto a =
                static_cast<std::uint16_t>(static_cast<int>(iy) * n_side + ix);
            const auto bb = static_cast<std::uint16_t>(a + 1);
            const auto c = static_cast<std::uint16_t>(a + n_side);
            const auto d = static_cast<std::uint16_t>(a + n_side + 1);
            idx.push_back(a);
            idx.push_back(c);
            idx.push_back(bb);
            idx.push_back(bb);
            idx.push_back(c);
            idx.push_back(d);
        }
    return idx;
}

}  // namespace render
