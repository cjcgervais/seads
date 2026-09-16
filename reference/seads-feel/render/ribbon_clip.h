#pragma once
// T24 (fly round-16 tail, Chad on the Errington right flank: "there is the
// snowmachine trail and another line. There is almost overhang there") — the
// ribbon indices CLIPPED by the excavation cut disks. The ribbons drape on
// the heightfield, but where the terrain is CUT (the Errington pit + the
// trench steps) the ground is gone and the draped lines hovered across the
// void — the "illusory / almost overhang" read. Header-only and RAYLIB-FREE
// (unlike render/ribbons.cpp) so the ctest suite pins the shipped clip
// directly — the route-the-live-path-THROUGH-the-tested-function discipline.
#include <cstddef>
#include <vector>

#include "render/sphere_param.h"     // CutDisk + dir_in_any_cut (ONE metric)
#include "render/sudbury_gis.gen.h"  // the baked ribbon paths

namespace render {

// The baked path's triangle list with every triangle dropped whose ANY vertex
// direction lies inside ANY cut disk — the IDENTICAL rule and dir_in_any_cut
// metric fill_face drops terrain triangles with, so a ribbon ends exactly
// where the ground does. Indices are LOCAL 0-based (as baked). Empty cuts =>
// the baked list verbatim (the bit-identical no-op).
inline std::vector<unsigned short> ribbon_indices_outside_cuts(
    std::size_t path_index, const std::vector<CutDisk>& cuts, double R) {
    const GisRibbonPath& P = kSudburyRibbonPaths[path_index];
    std::vector<unsigned short> kept;
    kept.reserve(static_cast<std::size_t>(P.idx_count));
    // Per-vertex verdict once (verts are shared by adjacent strip quads).
    std::vector<char> in_cut(static_cast<std::size_t>(P.vtx_count), 0);
    if (!cuts.empty())
        for (int i = 0; i < P.vtx_count; ++i) {
            const GisRibbonVertex& V = kSudburyRibbonVerts[P.vtx_off + i];
            in_cut[static_cast<std::size_t>(i)] =
                dir_in_any_cut(glm::dvec3(V.dir[0], V.dir[1], V.dir[2]), R,
                               cuts)
                    ? 1
                    : 0;
        }
    for (int k = 0; k + 2 < P.idx_count; k += 3) {
        const unsigned short i0 = kSudburyRibbonIndices[P.idx_off + k];
        const unsigned short i1 = kSudburyRibbonIndices[P.idx_off + k + 1];
        const unsigned short i2 = kSudburyRibbonIndices[P.idx_off + k + 2];
        if (in_cut[i0] || in_cut[i1] || in_cut[i2])
            continue;  // hovering over a cut void: drop (fill_face's rule)
        kept.push_back(i0);
        kept.push_back(i1);
        kept.push_back(i2);
    }
    return kept;
}

}  // namespace render
