#include "render/sled_plumes_draw.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <glm/glm.hpp>

#include "raymath.h"  // MatrixTranslate
#include "rlgl.h"

namespace render {

namespace {

// The CC1 billboard shader (render/smoke.cpp) + a per-vertex color so three
// differently-tinted trails share one back-to-front batch. Offsetting in VIEW
// space is screen-aligned by construction (no degenerate axis looking down
// the plume). matModel = -eye translate (eye-relative; camera at origin).
const char* kPlumeVS = R"GLSL(#version 330
in vec3 vertexPosition;      // puff center, world (eye-relative via matModel)
in vec2 vertexTexCoord;      // unit corner [-1,1]^2
in vec3 vertexNormal;        // (size_m, alpha, 0) per vertex
in vec4 vertexColor;         // trail tint
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
out vec2 vCorner;
out float vAlpha;
out vec3 vColor;
void main() {
    vec3 rel = (matModel * vec4(vertexPosition, 1.0)).xyz;
    vec3 viewPos = (matView * vec4(rel, 1.0)).xyz;
    viewPos.xy += vertexTexCoord * vertexNormal.x;   // world-metres billboard
    vCorner = vertexTexCoord;
    vAlpha = vertexNormal.y;
    vColor = vertexColor.rgb;
    gl_Position = matProjection * vec4(viewPos, 1.0);
}
)GLSL";

const char* kPlumeFS = R"GLSL(#version 330
in vec2 vCorner;
in float vAlpha;
in vec3 vColor;
out vec4 finalColor;
void main() {
    float r = length(vCorner);
    if (r > 1.0) discard;
    float soft = 1.0 - smoothstep(0.0, 1.0, r);  // 1 center -> 0 edge
    soft *= soft;                                 // cloudy falloff
    finalColor = vec4(vColor, vAlpha * soft);
}
)GLSL";

constexpr std::size_t kPlumeQuadMax =
    kRoostMax + kExhaustMax + kBreathMax + kBurstMax;

// Per-trail LOOK constants (the wind_audio.h/vortex.h "tunables are render
// code constants" precedent). Colors follow the ribbons.h/S1_SPEC RT-1 ruling:
// nothing here is BRIGHTER than the snow — the roost is fresh thrown powder
// (near-white, a shade cool), the exhaust reads by being DARKER (grey), the
// breath is a faint white mist.
struct TrailLook {
    glm::vec3 color;
    float r0, r1;       // [m] radius birth -> death
    float peak_alpha;   // before the age envelope
    float life;         // [s] — matches the bookkeeping constant
};
constexpr TrailLook kRoostLook{{0.90f, 0.92f, 0.96f}, 0.35f, 1.40f, 0.55f,
                               static_cast<float>(kRoostLife)};
// ★ R4c THE POOF. Bigger and longer-lived than the roost -- a body going into
// powder throws far more snow than a track paddle -- but deliberately NOT
// brighter than the snow it came out of (the RT-1 ruling this file already
// keeps for the roost). Same family, louder.
constexpr TrailLook kBurstLook{{0.93f, 0.94f, 0.97f}, 0.45f, 2.20f, 0.60f,
                               static_cast<float>(kBurstLife)};
constexpr TrailLook kExhaustLook{{0.42f, 0.43f, 0.45f}, 0.10f, 0.55f, 0.34f,
                                 static_cast<float>(kExhaustLife)};
constexpr TrailLook kBreathLook{{0.92f, 0.93f, 0.95f}, 0.06f, 0.38f, 0.26f,
                                static_cast<float>(kBreathLife)};

struct DrawPuff {
    glm::dvec3 pos;
    float size, alpha;
    glm::vec3 color;
    double depth;
};

bool build(SledPlumesRenderer& r) {
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const int n = static_cast<int>(kPlumeQuadMax);
    Mesh m{};
    m.vertexCount = n * 4;
    m.triangleCount = n * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.normals =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.colors = static_cast<unsigned char*>(
        MemAlloc(sizeof(unsigned char) * 4 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    for (int i = 0; i < n; ++i) {
        for (int v = 0; v < 4; ++v) {
            const int vi = i * 4 + v;
            m.texcoords[vi * 2 + 0] = corner[v][0];
            m.texcoords[vi * 2 + 1] = corner[v][1];
            m.vertices[vi * 3 + 0] = 0.0f;
            m.vertices[vi * 3 + 1] = 0.0f;
            m.vertices[vi * 3 + 2] = 0.0f;
            m.normals[vi * 3 + 0] = 0.0f;
            m.normals[vi * 3 + 1] = 0.0f;
            m.normals[vi * 3 + 2] = 0.0f;
            m.colors[vi * 4 + 0] = 255;
            m.colors[vi * 4 + 1] = 255;
            m.colors[vi * 4 + 2] = 255;
            m.colors[vi * 4 + 3] = 255;
        }
        const unsigned short b = static_cast<unsigned short>(i * 4);
        const int ti = i * 6;
        m.indices[ti + 0] = b;
        m.indices[ti + 1] = b + 1;
        m.indices[ti + 2] = b + 2;
        m.indices[ti + 3] = b;
        m.indices[ti + 4] = b + 2;
        m.indices[ti + 5] = b + 3;
    }
    UploadMesh(&m, true);  // dynamic: positions/normals/colors per frame
    r.mesh = m;
    r.vbuf.assign(static_cast<std::size_t>(m.vertexCount) * 3, 0.0f);
    r.nbuf.assign(static_cast<std::size_t>(m.vertexCount) * 3, 0.0f);
    r.cbuf.assign(static_cast<std::size_t>(m.vertexCount) * 4, 255);
    r.shader = LoadShaderFromMemory(kPlumeVS, kPlumeFS);
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.ok = r.shader.id != 0;
    if (r.ok)
        TraceLog(LOG_INFO, "SLED PLUMES: renderer built (%d quads cap)", n);
    return r.ok;
}

void gather(const std::vector<SledPlumePuff>& puffs, const TrailLook& look,
            const glm::dvec3& eye, std::vector<DrawPuff>& out) {
    for (const SledPlumePuff& p : puffs) {
        const float lf =
            std::clamp(static_cast<float>(p.age) / look.life, 0.0f, 1.0f);
        // Fast fade-in (0 -> 6% of life) and a soft tail-out from 55%.
        const float fade_in = std::min(1.0f, lf / 0.06f);
        const float fade_out =
            1.0f - std::clamp((lf - 0.55f) / 0.45f, 0.0f, 1.0f);
        const float a = look.peak_alpha * fade_in * fade_out * fade_out;
        if (a <= 0.004f) continue;
        DrawPuff d;
        d.pos = p.pos;
        d.size = look.r0 + (look.r1 - look.r0) * lf;
        // Per-puff size scatter off the SEEDED hash (no clock, no boil).
        d.size *= 0.8f + 0.4f * smoke_hash(p.seed, 7);
        d.alpha = a;
        d.color = look.color;
        d.depth = glm::length(p.pos - eye);
        out.push_back(d);
    }
}

}  // namespace

void draw_sled_plumes(SledPlumesRenderer& r, const SledPlumes& plumes,
                      const glm::dvec3& eye) {
    if (plumes.roost.empty() && plumes.exhaust.empty() &&
        plumes.breath.empty() && plumes.burst.empty())
        return;
    static bool tried = false;
    if (!r.ok) {
        if (tried) return;
        tried = true;
        if (!build(r)) return;
    }
    static std::vector<DrawPuff> draw;  // scratch, render-thread only
    draw.clear();
    draw.reserve(kPlumeQuadMax);
    gather(plumes.burst, kBurstLook, eye, draw);
    gather(plumes.roost, kRoostLook, eye, draw);
    gather(plumes.exhaust, kExhaustLook, eye, draw);
    gather(plumes.breath, kBreathLook, eye, draw);
    if (draw.empty()) return;
    // Back-to-front so alpha composites correctly across ALL three trails.
    std::sort(draw.begin(), draw.end(),
              [](const DrawPuff& a, const DrawPuff& b) {
                  return a.depth > b.depth;
              });
    const int n = static_cast<int>(draw.size());
    for (int k = 0; k < n; ++k) {
        const DrawPuff& p = draw[static_cast<std::size_t>(k)];
        // Eye-relative in DOUBLE, cast after the subtraction (a raw float
        // world position at planet radius is metres of slop).
        const glm::dvec3 rel = p.pos - eye;
        const float px = static_cast<float>(rel.x);
        const float py = static_cast<float>(rel.y);
        const float pz = static_cast<float>(rel.z);
        const unsigned char cr = static_cast<unsigned char>(p.color.x * 255.0f);
        const unsigned char cg = static_cast<unsigned char>(p.color.y * 255.0f);
        const unsigned char cb = static_cast<unsigned char>(p.color.z * 255.0f);
        for (int v = 0; v < 4; ++v) {
            const std::size_t vi = static_cast<std::size_t>((k * 4 + v) * 3);
            r.vbuf[vi + 0] = px;
            r.vbuf[vi + 1] = py;
            r.vbuf[vi + 2] = pz;
            r.nbuf[vi + 0] = p.size;
            r.nbuf[vi + 1] = p.alpha;
            r.nbuf[vi + 2] = 0.0f;
            const std::size_t ci = static_cast<std::size_t>((k * 4 + v) * 4);
            r.cbuf[ci + 0] = cr;
            r.cbuf[ci + 1] = cg;
            r.cbuf[ci + 2] = cb;
            r.cbuf[ci + 3] = 255;
        }
    }
    // Zero the SIZE and alpha of the unused tail: a zero-size billboard
    // rasterizes nothing, so the fixed-capacity mesh costs only its live span.
    for (int k = n; k < static_cast<int>(kPlumeQuadMax); ++k) {
        for (int v = 0; v < 4; ++v) {
            const std::size_t vi = static_cast<std::size_t>((k * 4 + v) * 3);
            r.nbuf[vi + 0] = 0.0f;
            r.nbuf[vi + 1] = 0.0f;
        }
    }
    UpdateMeshBuffer(r.mesh, 0, r.vbuf.data(),
                     static_cast<int>(r.vbuf.size() * sizeof(float)), 0);
    UpdateMeshBuffer(r.mesh, 2, r.nbuf.data(),
                     static_cast<int>(r.nbuf.size() * sizeof(float)), 0);
    UpdateMeshBuffer(r.mesh, 3, r.cbuf.data(),
                     static_cast<int>(r.cbuf.size() * sizeof(unsigned char)),
                     0);
    // Positions were already made eye-relative in DOUBLE above, so the model
    // matrix is identity (NOT the smoke.cpp -eye translate: that subtracts in
    // float32 on the GPU, metres of jitter at planet radius).
    const Matrix xf = MatrixIdentity();
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlDisableDepthMask();  // depth-TEST stays on: the machine occludes puffs
    DrawMesh(r.mesh, r.mat, xf);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

void unload_sled_plumes_renderer(SledPlumesRenderer& r) {
    if (!r.ok) return;
    UnloadMesh(r.mesh);
    UnloadShader(r.shader);
    r.ok = false;
}

}  // namespace render
