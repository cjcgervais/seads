#include "render/lights.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"  // MatrixTranslate
#include "render/camera.h"  // kChaseFovyDeg (the RESTING fov — the min-px floor reference)
#include "render/sudbury_gis.gen.h"
#include "rlgl.h"

namespace render {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kLampsPerBatch = 16000;  // *4 verts < 65536 (ushort indices)

// VS: a camera-facing billboard at the lamp's draped world position, sized to a
// ~CONSTANT screen pixel size (the offset scales with view depth so the
// perspective divide cancels) — so a lamp stays a visible glowing dot from
// altitude, the whole point. matModel is raylib's DrawMesh transform (-eye
// translate); matView is the origin-camera view (eye-relative rendering, camera
// at origin).
const char* kLampVS = R"GLSL(#version 330
in vec3 vertexPosition;      // lamp world position (draped at radius_at+h)
in vec2 vertexTexCoord;      // corner offset [-1,1]^2
uniform mat4 matModel;       // -eye translate (auto-set)
uniform mat4 matView;        // origin-camera view (auto-set)
uniform mat4 matProjection;  // projection (auto-set)
uniform float uSizeM;        // world glow radius (view-space metres) — up-close size
uniform float uMinPx;        // = min_px * 2*tan(fov/2)/viewport_h (per-z constant-px floor)
out vec2 vCorner;
out vec3 vUp;                // lamp radial (world) for the night gate
void main() {
    vUp = normalize(vertexPosition);
    vec3 rel = (matModel * vec4(vertexPosition, 1.0)).xyz;   // lamp - eye
    vec3 viewPos = (matView * vec4(rel, 1.0)).xyz;           // camera at origin
    // HYBRID size: a WORLD-metres offset (grows on screen as you approach) OR a
    // per-z offset that projects to a constant MIN pixel floor (so far lamps stay
    // visible), whichever is bigger. So lamps are real glows up close AND legible
    // dots from altitude (Chad: they vanished up close at constant px).
    float off = max(uSizeM, uMinPx * (-viewPos.z));
    viewPos.xy += vertexTexCoord * off;
    vCorner = vertexTexCoord;
    gl_Position = matProjection * vec4(viewPos, 1.0);
}
)GLSL";

// FS: an additive glow (hot core + soft halo), gated to NIGHT by the lamp's own
// sun elevation (so lamps switch on at dusk with the sky). Near-white so the S1
// post keeps it silver (planes stay the only chroma).
const char* kLampFS = R"GLSL(#version 330
in vec2 vCorner;
in vec3 vUp;
uniform vec3 uSunDir;        // world light-travel dir (sun -> scene)
uniform vec3 uColor;
uniform float uBright;       // soft-halo strength (the aura)
uniform float uCoreBright;   // hot-core strength (the bulb — reads as a LIT lamp up close)
uniform float uCoreSharp;    // core falloff sharpness (higher = tighter bulb)
uniform float uNightBypass;  // 1 = always night (lit) — the tunnel ceiling ember
                             // field spans a FULL sphere, so no single sun dir
                             // reads night for every lamp; T10.1. Default 0 =
                             // the normal sun-elevation gate (street lamps).
out vec4 finalColor;
void main() {
    float r = length(vCorner);
    if (r > 1.0) discard;
    // TWO-PART glow: a soft wide HALO (the aura) + a tight bright CORE (the bulb).
    // Up close the core reads as an ACTUAL illuminated lamp instead of a vague
    // soft haze (Chad: lights look dim up close); from altitude the whole quad
    // shrinks to the min-px dot and both terms sum into a bright point. Additive;
    // the core is kept TIGHT (small footprint) so a dense residential street does
    // not blow out to a white bar (Chad: illuminate up close WITHOUT oversaturation).
    float halo = exp(-r * r * 5.5);
    float core = exp(-r * r * uCoreSharp);
    float sunEl = dot(-uSunDir, vUp);                    // sun elevation over the lamp
    float night = 1.0 - smoothstep(-0.14, 0.10, sunEl);  // 1 night .. 0 day (dusk band)
    night = max(night, uNightBypass);                    // T10.1 always-lit set
    float lit = (halo * uBright + core * uCoreBright) * night;
    finalColor = vec4(uColor * lit, 1.0);  // additive
}
)GLSL";

// One quad-mesh for lamps [lo, hi): 4 verts/lamp at the draped world position +
// corner offsets, 2 tris each (ushort local indices).
Mesh build_lamp_mesh(const HeightField& hf, int lo, int hi) {
    const int n = hi - lo;
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    Mesh m{};
    m.vertexCount = n * 4;
    m.triangleCount = n * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    for (int i = 0; i < n; ++i) {
        const GisLightPoint& L = kSudburyLamps[lo + i];
        const glm::dvec3 d(L.dir[0], L.dir[1], L.dir[2]);
        const double radius = hf.radius_at(d) + static_cast<double>(L.h);
        const float px = static_cast<float>(d.x * radius);
        const float py = static_cast<float>(d.y * radius);
        const float pz = static_cast<float>(d.z * radius);
        for (int v = 0; v < 4; ++v) {
            const int vi = i * 4 + v;
            m.vertices[vi * 3 + 0] = px;
            m.vertices[vi * 3 + 1] = py;
            m.vertices[vi * 3 + 2] = pz;
            m.texcoords[vi * 2 + 0] = corner[v][0];
            m.texcoords[vi * 2 + 1] = corner[v][1];
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
    UploadMesh(&m, false);
    return m;
}

// One quad-mesh for explicit world-absolute points [lo, hi) — the tunnel-lamp
// parallel of build_lamp_mesh (no heightfield drape; positions are final).
// Corners stay in [-1,1] (the FS discards r>1); per-lamp intensity is handled
// at the app seam by drawing brighter lamps in a separate, larger-size batch.
Mesh build_point_mesh(const std::vector<glm::dvec3>& pts, int lo, int hi) {
    const int n = hi - lo;
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    Mesh m{};
    m.vertexCount = n * 4;
    m.triangleCount = n * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    for (int i = 0; i < n; ++i) {
        const glm::dvec3& d = pts[lo + i];
        const float px = static_cast<float>(d.x);
        const float py = static_cast<float>(d.y);
        const float pz = static_cast<float>(d.z);
        for (int v = 0; v < 4; ++v) {
            const int vi = i * 4 + v;
            m.vertices[vi * 3 + 0] = px;
            m.vertices[vi * 3 + 1] = py;
            m.vertices[vi * 3 + 2] = pz;
            m.texcoords[vi * 2 + 0] = corner[v][0];
            m.texcoords[vi * 2 + 1] = corner[v][1];
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
    UploadMesh(&m, false);
    return m;
}

// The lamp shader is IDENTICAL for every renderer (dim/bright/ember street +
// tunnel tiers all use kLampVS/kLampFS), so compile+link it ONCE and share it.
// A destructible-lamp kill rebuilds the tier meshes every dirty frame; recompiling
// three full GLSL programs synchronously on the render thread was the shootout
// hitch. This caches the program + its uniform locations on first use; the cached
// Shader is never freed (raylib frees the GL context at process exit).
struct LampShader {
    Shader shader{};
    int loc_sun = -1, loc_sizem = -1, loc_minpx = -1, loc_color = -1,
        loc_bright = -1, loc_core_bright = -1, loc_core_sharp = -1,
        loc_night_bypass = -1;
};

const LampShader& ensure_lamp_shader() {
    static LampShader s = [] {
        LampShader t;
        t.shader = LoadShaderFromMemory(kLampVS, kLampFS);
        t.loc_sun = GetShaderLocation(t.shader, "uSunDir");
        t.loc_sizem = GetShaderLocation(t.shader, "uSizeM");
        t.loc_minpx = GetShaderLocation(t.shader, "uMinPx");
        t.loc_color = GetShaderLocation(t.shader, "uColor");
        t.loc_bright = GetShaderLocation(t.shader, "uBright");
        t.loc_core_bright = GetShaderLocation(t.shader, "uCoreBright");
        t.loc_core_sharp = GetShaderLocation(t.shader, "uCoreSharp");
        t.loc_night_bypass = GetShaderLocation(t.shader, "uNightBypass");
        return t;
    }();
    return s;
}

// Point a renderer at the shared, persistent lamp shader (no per-build compile).
void attach_lamp_shader(LampRenderer& r) {
    const LampShader& s = ensure_lamp_shader();
    r.shader = s.shader;
    r.loc_sun = s.loc_sun;
    r.loc_sizem = s.loc_sizem;
    r.loc_minpx = s.loc_minpx;
    r.loc_color = s.loc_color;
    r.loc_bright = s.loc_bright;
    r.loc_core_bright = s.loc_core_bright;
    r.loc_core_sharp = s.loc_core_sharp;
    r.loc_night_bypass = s.loc_night_bypass;
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
}

}  // namespace

LampRenderer build_lamp_renderer_from_points(
    const std::vector<glm::dvec3>& positions, const LampLook& look) {
    LampRenderer r;
    r.look = look;
    const int total = static_cast<int>(positions.size());
    if (total <= 0) {
        TraceLog(LOG_INFO, "TUNNEL LAMPS: none placed — inert");
        return r;  // ok=false
    }
    for (int lo = 0; lo < total; lo += kLampsPerBatch)
        r.meshes.push_back(build_point_mesh(
            positions, lo, std::min(lo + kLampsPerBatch, total)));
    attach_lamp_shader(r);  // shared persistent program (no per-build compile)
    r.ok = true;
    TraceLog(LOG_INFO, "TUNNEL LAMPS: %d gaslamps (%zu batches)", total,
             r.meshes.size());
    return r;
}

LampRenderer build_lamp_renderer(const HeightField& hf, const LampLook& look) {
    LampRenderer r;
    r.look = look;
    const int total = static_cast<int>(kSudburyLampCount);
    if (total <= 0) {
        TraceLog(LOG_INFO, "LAMPS: none baked — inert");
        return r;  // ok=false
    }
    for (int lo = 0; lo < total; lo += kLampsPerBatch)
        r.meshes.push_back(
            build_lamp_mesh(hf, lo, std::min(lo + kLampsPerBatch, total)));
    attach_lamp_shader(r);  // shared persistent program (no per-build compile)
    r.ok = true;
    TraceLog(LOG_INFO, "LAMPS: %d street lamps (%zu batches)", total,
             r.meshes.size());
    return r;
}

void unload_lamp_renderer(LampRenderer& r) {
    if (!r.ok) return;
    for (Mesh& m : r.meshes) UnloadMesh(m);
    r.meshes.clear();
    // The shader is the shared, persistent program (ensure_lamp_shader) — NEVER
    // unload it here: every renderer aliases the same Shader, so freeing it would
    // break the surviving tiers, and a dirty-rebuild would re-alias a freed
    // program. It is compiled once and lives for the process (raylib frees the GL
    // context at exit). Only this renderer's own GPU meshes are released above.
    r.shader = Shader{};  // drop the alias so no draw path reads a stale handle
    // Free the material's heap-allocated maps array (LoadMaterialDefault per
    // attach) or every lamp-kill rebuild leaks ~3 of them (red-team F1). The
    // shader alias must be detached FIRST: UnloadMaterial unloads any
    // non-default shader it holds, which would delete the SHARED lamp program
    // out from under the surviving tiers. With id 0 the GL delete is a no-op
    // and the default-texture maps are skipped by id check.
    r.mat.shader = Shader{};
    UnloadMaterial(r.mat);
    r.mat = Material{};
    r.ok = false;
}

void draw_lamp_renderer(LampRenderer& r, const glm::vec3& sun_dir,
                        const glm::dvec3& eye, float /*fovy_deg*/,
                        int viewport_h) {
    if (!r.ok) return;
    // The min-px floor is referenced to the RESTING fov, NOT the live (possibly
    // RMB-zoomed) fov. The shader's matProjection uses the live fov, so at rest
    // the floor projects to exactly min_px pixels (far lamps stay legible from
    // altitude) but under gunsight zoom it MAGNIFIES with the scene like the
    // world-metres term — otherwise a screen-constant floor pins distant lamps
    // at min_px while the town balloons 2.8x around them, and they wash out /
    // vanish (Chad: zooming into distant lights dims them). Resting-referenced
    // = they grow when you zoom in to inspect the far town.
    const float fov = static_cast<float>(kChaseFovyDeg * kPi / 180.0);
    // per-z factor so uMinPx*(-z) projects to min_px pixels AT THE RESTING fov
    const float min_px = r.look.min_px * 2.0f * std::tan(0.5f * fov) /
                         std::max(1.0f, static_cast<float>(viewport_h));
    SetShaderValue(r.shader, r.loc_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_sizem, &r.look.size_m, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_minpx, &min_px, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_color, &r.look.color, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_bright, &r.look.brightness,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_core_bright, &r.look.core_bright,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_core_sharp, &r.look.core_sharp,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_night_bypass, &r.look.night_bypass,
                   SHADER_UNIFORM_FLOAT);

    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    // Additive over the scene; depth-test ON (terrain/hills occlude a lamp
    // behind them) but depth-WRITE off (glows never occlude each other). Cull
    // off (the billboard can wind either way).
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    for (Mesh& m : r.meshes) DrawMesh(m, r.mat, xf);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

}  // namespace render
