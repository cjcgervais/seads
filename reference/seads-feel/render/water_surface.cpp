#include "render/water_surface.h"

#include <cstdlib>

#include "external/glad.h"  // glPolygonOffset / glEnable (glad decls; impl in raylib)
#include "raymath.h"
#include "render/planet.h"
#include "render/sudbury_gis.gen.h"

namespace render {

WaterSurfaces build_water_surfaces(double lift_m) {
    WaterSurfaces w;
    for (std::size_t li = 0; li < kSudburyWaterLakeCount; ++li) {
        const GisWaterLake& L = kSudburyWaterLakes[li];
        if (L.vtx_count < 3 || L.idx_count < 3) continue;
        const double radius = L.elev_m + lift_m;

        Mesh m = {};
        m.vertexCount = L.vtx_count;
        m.triangleCount = L.idx_count / 3;
        m.vertices =
            static_cast<float*>(MemAlloc(sizeof(float) * 3 * L.vtx_count));
        m.normals =
            static_cast<float*>(MemAlloc(sizeof(float) * 3 * L.vtx_count));
        m.indices = static_cast<unsigned short*>(
            MemAlloc(sizeof(unsigned short) * L.idx_count));

        for (int i = 0; i < L.vtx_count; ++i) {
            const double* d = kSudburyWaterVerts[L.vtx_off + i].dir;
            // Absolute planet-centered world position (float), like the planet
            // faces — the VS does fragDir = normalize(vertexPosition) and the
            // eye-relative rebase happens in the model matrix at draw time.
            m.vertices[3 * i + 0] = static_cast<float>(d[0] * radius);
            m.vertices[3 * i + 1] = static_cast<float>(d[1] * radius);
            m.vertices[3 * i + 2] = static_cast<float>(d[2] * radius);
            // Radial normal (the mirror normal; the bake wrote water N = radial).
            m.normals[3 * i + 0] = static_cast<float>(d[0]);
            m.normals[3 * i + 1] = static_cast<float>(d[1]);
            m.normals[3 * i + 2] = static_cast<float>(d[2]);
        }
        for (int k = 0; k < L.idx_count; ++k)
            m.indices[k] = kSudburyWaterIndices[L.idx_off + k];  // LOCAL 0-based

        UploadMesh(&m, false);
        w.meshes.push_back(m);
    }
    w.ok = !w.meshes.empty();
    return w;
}

void unload_water_surfaces(WaterSurfaces& w) {
    for (Mesh& m : w.meshes) UnloadMesh(m);
    w.meshes.clear();
    w.ok = false;
}

void draw_water_surfaces(const Planet& p, const WaterSurfaces& w,
                         const glm::dvec3& eye) {
    if (!p.ok || !w.ok) return;
    // Only draw over a planet that actually carries the Sudbury landmask (water
    // context). Without it the planet FS water branch is inert and uForceWater
    // would shade the lake meshes as flat LAND discs floating at the lift height,
    // polygon-offset IN FRONT of the terrain — strictly worse than the faceting
    // this fixes (Fable-AFTER P1: the degraded landmask-load-fail path).
    if (p.has_water < 0.5f) return;
    // A/B bypass (seam-safe env gate), read ONCE — no per-frame getenv syscall.
    static const bool no_water = std::getenv("SEADS_NO_WATER") != nullptr;
    if (no_water) return;

    // Force the planet program's water branch ON for these meshes: the mesh must
    // read as water across its FULL OSM outline, incl. the eroded shore band
    // where the cubemap alpha is 0. Every OTHER uniform (sun/moon/up/eyeAlt/
    // aurora/scatter/haze) is already bound on this same program by
    // draw_planet_mesh this frame -> the lake shades identically to the planet's
    // own water pixels, so it can't fork from the limb (Fable ★3 / H1).
    const float on = 1.0f, off = 0.0f;
    SetShaderValue(p.shader, p.loc_force_water, &on, SHADER_UNIFORM_FLOAT);

    // Slope-scaled depth offset: pull the water toward the eye so it wins the
    // z-fight against the coincident flattened lakebed AND covers the coarse
    // shore facets that dip below the water at grazing range (Fable ★4: the 2 m
    // radial lift alone falls under the depth LSB ~ z^2/3.36e7 past ~8 km; the
    // slope factor is what saves the landing view).
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);

    const Matrix xf = MatrixTranslate(static_cast<float>(-eye.x),
                                      static_cast<float>(-eye.y),
                                      static_cast<float>(-eye.z));
    for (const Mesh& m : w.meshes) DrawMesh(m, p.mat, xf);

    glPolygonOffset(0.0f, 0.0f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    SetShaderValue(p.shader, p.loc_force_water, &off, SHADER_UNIFORM_FLOAT);
}

}  // namespace render
