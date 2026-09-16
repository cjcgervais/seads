#pragma once
// ★ R5 rows 4/5/6 — the DRAW side of render/sled_plumes.h. Mirrors the CC1
// Superstack technique (render/smoke.cpp): camera-facing alpha billboard
// quads, per-vertex normal.xyz = (size_m, alpha, 0), one CPU back-to-front
// sort, depth-test ON / depth-write OFF — plus a per-vertex COLOR channel so
// roost (near-white), exhaust (grey) and breath (white) composite correctly
// in ONE sorted batch instead of three fighting draw calls. What it does NOT
// take from smoke.cpp is the simulation model: that plume is a pure function
// of a wrapped phase off a STATIC anchor, wrong for a moving emitter — the
// motion lives in render/sled_plumes.h's app-owned trails. render/ reads
// state, writes nothing, reads no clock (ages arrive inside the puffs).
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sled_plumes.h"

namespace render {

struct SledPlumesRenderer {
    bool ok = false;
    Shader shader{};
    Material mat{};
    Mesh mesh{};               // (kRoostMax+kExhaustMax+kBreathMax)*4 verts, DYNAMIC
    std::vector<float> vbuf;   // positions (eye-relative, rebuilt per frame)
    std::vector<float> nbuf;   // normal.xyz = (size_m, alpha, 0) per vert
    std::vector<unsigned char> cbuf;  // per-vertex RGBA
};

// Lazy-build on first draw (needs a live GL context); no-op if it failed.
void draw_sled_plumes(SledPlumesRenderer& r, const SledPlumes& plumes,
                      const glm::dvec3& eye);
void unload_sled_plumes_renderer(SledPlumesRenderer& r);

}  // namespace render
