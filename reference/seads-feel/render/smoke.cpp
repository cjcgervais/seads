#include "render/smoke.h"

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>  // normalize/dot/cross/length/smoothstep

#include "raymath.h"  // MatrixTranslate
#include "rlgl.h"

namespace render {

namespace {

// A camera-facing billboard at the puff's world center, sized in WORLD metres from
// the per-vertex normal.x (so puffs grow on screen as you approach). Offsetting in
// VIEW space is screen-aligned by construction — no zenith degeneracy flying down
// the plume axis (Fable cross-cutting P1). matModel = -eye translate (eye-relative
// rendering, camera at origin); matView/matProjection auto-set by raylib.
const char* kSmokeVS = R"GLSL(#version 330
in vec3 vertexPosition;      // puff center world position (draped stack top + rise)
in vec2 vertexTexCoord;      // unit corner [-1,1]^2
in vec3 vertexNormal;        // (size_m, alpha, 0) per vertex
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;
out vec2 vCorner;
out float vAlpha;
void main() {
    vec3 rel = (matModel * vec4(vertexPosition, 1.0)).xyz;   // puff - eye
    vec3 viewPos = (matView * vec4(rel, 1.0)).xyz;           // camera at origin
    viewPos.xy += vertexTexCoord * vertexNormal.x;           // world-metres billboard
    vCorner = vertexTexCoord;
    vAlpha = vertexNormal.y;
    gl_Position = matProjection * vec4(viewPos, 1.0);
}
)GLSL";

// A soft round MONO puff, alpha-blended. Low-saturation grey so the S1 post silvers
// it (the smoke is NOT the sanctioned chroma — that is CC3's hot slag).
const char* kSmokeFS = R"GLSL(#version 330
in vec2 vCorner;
in float vAlpha;
uniform vec3 uColor;
out vec4 finalColor;
void main() {
    float r = length(vCorner);
    if (r > 1.0) discard;
    float soft = 1.0 - smoothstep(0.0, 1.0, r);  // 1 center -> 0 edge
    soft *= soft;                                 // softer, cloudier falloff
    finalColor = vec4(uColor, vAlpha * soft);     // alpha-blended grey
}
)GLSL";

// Deterministic per-puff hash -> [-1,1] (time-independent: the plume trajectory is a
// pure function of the wrapped phase, no accumulation, no clock).
float hash1(int i, int salt) {
    // Unsigned multiply throughout — a signed `i * 73856093` overflows int UB at
    // i>=30 (Fable-AFTER P1); the wrap must be defined, not incidental.
    unsigned int h = static_cast<unsigned int>(i) * 73856093u ^
                     static_cast<unsigned int>(salt) * 19349663u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (static_cast<float>(h & 0xffffffu) / 8388607.5f) - 1.0f;  // zero-mean [-1,1]
}

}  // namespace

SmokeRenderer build_smoke_renderer(const HeightField& hf, const SmokeLook& look,
                                   const glm::dvec3& anchor_dir, double stack_h) {
    SmokeRenderer r;
    r.look = look;
    if (look.puffs <= 0) return r;  // ok=false — inert
    // 4 verts/puff with ushort indices caps the batch at 16384 puffs (Fable-AFTER
    // P2: silent wraparound above). The plume needs ~30, but clamp defensively.
    r.look.puffs = std::min(r.look.puffs, 16384);

    const glm::dvec3 dir = glm::normalize(anchor_dir);
    r.up = dir;
    r.anchor_top = dir * (hf.radius_at(dir) + stack_h);
    // Wind projected into the tangent plane; fall back if the hint is ~parallel to
    // the radial (Fable CC1 P1 — a silent NaN here poisons every puff).
    glm::dvec3 w = glm::dvec3(look.wind);
    glm::dvec3 wt = w - glm::dot(w, dir) * dir;
    if (glm::length(wt) < 1e-6) {
        glm::dvec3 ref = std::fabs(dir.y) < 0.9 ? glm::dvec3(0, 1, 0)
                                                : glm::dvec3(1, 0, 0);
        wt = glm::cross(dir, ref);
    }
    r.wtan = glm::normalize(wt);
    r.wtan2 = glm::normalize(glm::cross(dir, r.wtan));

    // A dynamic quad mesh: 4 verts/puff. texcoords (unit corners) + indices are
    // STATIC; vertices (centers) and normals (size, alpha) are rebuilt per frame.
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const int n = look.puffs;
    Mesh m{};
    m.vertexCount = n * 4;
    m.triangleCount = n * 2;
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.normals = static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    for (int i = 0; i < n; ++i) {
        for (int v = 0; v < 4; ++v) {
            const int vi = i * 4 + v;
            m.texcoords[vi * 2 + 0] = corner[v][0];
            m.texcoords[vi * 2 + 1] = corner[v][1];
            m.vertices[vi * 3 + 0] = 0.0f;  // filled per frame
            m.vertices[vi * 3 + 1] = 0.0f;
            m.vertices[vi * 3 + 2] = 0.0f;
            m.normals[vi * 3 + 0] = 0.0f;
            m.normals[vi * 3 + 1] = 0.0f;
            m.normals[vi * 3 + 2] = 0.0f;
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
    UploadMesh(&m, true);  // dynamic (positions + normals updated each frame)
    r.mesh = m;
    r.vbuf.assign(static_cast<std::size_t>(m.vertexCount) * 3, 0.0f);
    r.nbuf.assign(static_cast<std::size_t>(m.vertexCount) * 3, 0.0f);
    r.order.resize(static_cast<std::size_t>(n));

    r.shader = LoadShaderFromMemory(kSmokeVS, kSmokeFS);
    r.loc_color = GetShaderLocation(r.shader, "uColor");
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.ok = true;
    TraceLog(LOG_INFO, "SMOKE: Superstack plume (%d puffs)", n);
    return r;
}

void unload_smoke_renderer(SmokeRenderer& r) {
    if (!r.ok) return;
    UnloadMesh(r.mesh);
    UnloadShader(r.shader);
    r.ok = false;
}

void draw_smoke_renderer(SmokeRenderer& r, double phase, const glm::dvec3& eye) {
    if (!r.ok) return;
    const int n = r.look.puffs;
    const double rise = r.look.rise_m, drift = r.look.drift_m;
    const double r0 = r.look.r0_m, r1 = r.look.r1_m;
    const double jit = r.look.jitter_m, peak = r.look.opacity;

    // Per-puff pure-phase state. a in [0,1): birth at the stack top, death at the
    // top of the column. opacity -> 0 at BOTH a=0 and a=1 so a puff wrapping
    // 1->0 (the frac reset) is invisible — the loop-continuity invariant.
    struct Puff { glm::dvec3 pos; float size; float alpha; double depth; };
    std::vector<Puff> puff(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double a = phase + static_cast<double>(i) / static_cast<double>(n);
        a -= std::floor(a);  // frac in DOUBLE
        const glm::dvec3 jitter =
            jit * (static_cast<double>(hash1(i, 1)) * r.wtan +
                   static_cast<double>(hash1(i, 2)) * r.wtan2);
        const glm::dvec3 pos = r.anchor_top + r.up * (a * rise) +
                               r.wtan * (std::pow(a, 0.8) * drift) + jitter;
        const double fade =
            glm::smoothstep(0.0, 0.12, a) * (1.0 - glm::smoothstep(0.70, 1.0, a));
        Puff p;
        p.pos = pos;
        p.size = static_cast<float>(r0 + (r1 - r0) * a);
        p.alpha = static_cast<float>(peak * fade);
        p.depth = glm::length(pos - eye);
        puff[static_cast<std::size_t>(i)] = p;
    }
    // Back-to-front: farthest drawn first so alpha composites correctly.
    for (int i = 0; i < n; ++i) r.order[static_cast<std::size_t>(i)] = i;
    std::sort(r.order.begin(), r.order.end(), [&](int a, int b) {
        return puff[static_cast<std::size_t>(a)].depth >
               puff[static_cast<std::size_t>(b)].depth;
    });
    for (int k = 0; k < n; ++k) {
        const Puff& p = puff[static_cast<std::size_t>(r.order[static_cast<std::size_t>(k)])];
        const float px = static_cast<float>(p.pos.x);
        const float py = static_cast<float>(p.pos.y);
        const float pz = static_cast<float>(p.pos.z);
        for (int v = 0; v < 4; ++v) {
            const int vi = (k * 4 + v) * 3;
            r.vbuf[static_cast<std::size_t>(vi) + 0] = px;
            r.vbuf[static_cast<std::size_t>(vi) + 1] = py;
            r.vbuf[static_cast<std::size_t>(vi) + 2] = pz;
            r.nbuf[static_cast<std::size_t>(vi) + 0] = p.size;
            r.nbuf[static_cast<std::size_t>(vi) + 1] = p.alpha;
            r.nbuf[static_cast<std::size_t>(vi) + 2] = 0.0f;
        }
    }
    UpdateMeshBuffer(r.mesh, 0, r.vbuf.data(),
                     static_cast<int>(r.vbuf.size() * sizeof(float)), 0);
    UpdateMeshBuffer(r.mesh, 2, r.nbuf.data(),
                     static_cast<int>(r.nbuf.size() * sizeof(float)), 0);

    SetShaderValue(r.shader, r.loc_color, &r.look.color, SHADER_UNIFORM_VEC3);
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    // Alpha over the scene; depth-test ON (terrain occludes the plume base) but
    // depth-WRITE off (puffs never occlude each other — the CPU sort orders them).
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawMesh(r.mesh, r.mat, xf);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

}  // namespace render
