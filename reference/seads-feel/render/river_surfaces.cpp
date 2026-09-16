#include "render/river_surfaces.h"

#include <cstdlib>

#include "external/glad.h"  // glPolygonOffset / glEnable (glad decls; impl in raylib)
#include "raymath.h"        // MatrixTranslate
#include "render/planet.h"
#include "render/ribbon_clip.h"  // T24-river excavation clip (pure, test-pinned)
#include "render/sudbury_gis.gen.h"

namespace render {

RiverSurfaces build_river_surfaces(const HeightField& hf, double lift_m,
                                   int subdiv, int tiles,
                                   const std::vector<CutDisk>& cuts) {
    RiverSurfaces r;
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.kind != 3) continue;  // only rivers; roads/trails stay on the ribbon FS
        if (P.vtx_count < 3 || P.idx_count < 3) continue;
        // T24-river: drop every triangle hovering over an excavation cut void —
        // the IDENTICAL single-source rule/metric the road/trail ribbons and the
        // terrain itself use (render/ribbon_clip.h). Empty cuts -> the baked list
        // verbatim (bit-identical no-op).
        const std::vector<unsigned short> kept =
            ribbon_indices_outside_cuts(pi, cuts, hf.R);
        if (kept.empty()) continue;  // fully swallowed by a cut

        Mesh m = {};
        m.vertexCount = P.vtx_count;
        m.triangleCount = static_cast<int>(kept.size()) / 3;
        m.vertices =
            static_cast<float*>(MemAlloc(sizeof(float) * 3 * P.vtx_count));
        m.normals =
            static_cast<float*>(MemAlloc(sizeof(float) * 3 * P.vtx_count));
        m.indices = static_cast<unsigned short*>(
            MemAlloc(sizeof(unsigned short) * kept.size()));

        for (int i = 0; i < P.vtx_count; ++i) {
            const double* dd = kSudburyRibbonVerts[P.vtx_off + i].dir;
            const glm::dvec3 d(dd[0], dd[1], dd[2]);
            // Draped world position (float) on the RENDERED terrain facet (R4d,
            // same conformance as roads — radius_at rode metres above/below the
            // facets the screen shows), lifted a small clearance for z-fight.
            const double radius = drawn_radius_at(hf, d, subdiv, tiles) + lift_m;
            m.vertices[3 * i + 0] = static_cast<float>(d.x * radius);
            m.vertices[3 * i + 1] = static_cast<float>(d.y * radius);
            m.vertices[3 * i + 2] = static_cast<float>(d.z * radius);
            // Radial normal = local up = the mirror normal (water is locally
            // horizontal even where the strip slopes down the valley).
            m.normals[3 * i + 0] = static_cast<float>(d.x);
            m.normals[3 * i + 1] = static_cast<float>(d.y);
            m.normals[3 * i + 2] = static_cast<float>(d.z);
        }
        for (std::size_t k = 0; k < kept.size(); ++k)
            m.indices[k] = kept[k];  // LOCAL 0-based, T24-river-clipped

        UploadMesh(&m, false);
        r.meshes.push_back(m);
    }
    r.ok = !r.meshes.empty();
    return r;
}

void unload_river_surfaces(RiverSurfaces& r) {
    for (Mesh& m : r.meshes) UnloadMesh(m);
    r.meshes.clear();
    r.ok = false;
}

void draw_river_surfaces(const Planet& p, const RiverSurfaces& r,
                         const glm::dvec3& eye) {
    if (!p.ok || !r.ok) return;
    // Same water-context guard as the lakes: without the Sudbury landmask the planet
    // FS water branch is inert and uForceWater would paint flat land discs.
    if (p.has_water < 0.5f) return;
    static const bool no_rivers = std::getenv("SEADS_NO_RIVERS") != nullptr;
    if (no_rivers) return;  // A/B bypass, read once

    // Force the planet program's water branch ON for these meshes (single-sourced
    // mirror; every sky/scatter/moon/aurora/star uniform is already bound this frame by
    // draw_planet_mesh, so a river shades identically to the planet's own water pixels).
    const float on = 1.0f, off = 0.0f;
    SetShaderValue(p.shader, p.loc_force_water, &on, SHADER_UNIFORM_FLOAT);

    // Slope-scaled depth offset (like the lakes) so the thin draped strip wins the
    // z-tie against the coincident terrain at grazing range. Where a river runs INTO a
    // lake the two water surfaces overlap — benign (identical shading; the depth flip
    // is color-invisible).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);

    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    for (const Mesh& m : r.meshes) DrawMesh(m, p.mat, xf);

    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    SetShaderValue(p.shader, p.loc_force_water, &off, SHADER_UNIFORM_FLOAT);
}

}  // namespace render
