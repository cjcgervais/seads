#include "render/precip_draw.h"

#include <algorithm>
#include <cmath>

#include "raymath.h"  // MatrixTranslate
#include "render/precip.h"  // the pure placement core (precip_sample / center_cell)
#include "rlgl.h"

namespace render {

namespace {

// VS: a stretched camera-facing billboard at the particle's world position. The
// quad is oriented so its LONG axis follows local_up's screen projection (rain
// streaks fall along local_up, not screen-up — GO-ANYWHERE-safe); snow uses
// halfL==halfW (a round quad, orientation irrelevant). matModel is DrawMesh's
// -eye translate; matView is the origin-camera view (eye-relative rendering).
const char* kPrecipVS = R"GLSL(#version 330
in vec3 vertexPosition;   // particle world center (updated per frame)
in vec2 vertexTexCoord;   // corner offset [-1,1]^2
in vec4 vertexColor;      // per-particle fade in .a (rgb unused)
uniform mat4 matModel;       // -eye translate (auto-set)
uniform mat4 matView;        // origin-camera view (auto-set)
uniform mat4 matProjection;  // projection (auto-set)
uniform vec3 uUpView;        // local_up in VIEW space (streak orientation)
uniform float uHalfW;        // streak/flake half-width (view metres)
uniform float uHalfL;        // streak half-length along up (== uHalfW for snow)
out vec2 vCorner;
out float vFade;
void main() {
    vec3 rel = (matModel * vec4(vertexPosition, 1.0)).xyz;  // center - eye
    vec3 vp = (matView * vec4(rel, 1.0)).xyz;               // camera at origin
    // Screen-plane basis: upS = screen direction of local_up, rightS perpendicular.
    // Degenerate only when local_up points along the view axis (looking straight
    // down/up the radial) — then streaks are end-on (~invisible), so the fallback
    // direction is harmless.
    vec2 upS = uUpView.xy;
    float ul = length(upS);   // = sin(angle(local_up, view axis)) = the foreshorten
    upS = ul > 1e-4 ? upS / ul : vec2(0.0, 1.0);
    vec2 rightS = vec2(upS.y, -upS.x);
    // Foreshorten the streak: as local_up tilts toward the view axis (looking down
    // the fall) the streak length -> width, so a near-radial view shows dots not
    // full-length streaks, and the degenerate direction becomes irrelevant (Fable
    // W3 P1). Snow is unaffected (uHalfL == uHalfW).
    float halfL = mix(uHalfW, uHalfL, clamp(ul, 0.0, 1.0));
    vp.xy += rightS * (vertexTexCoord.x * uHalfW) + upS * (vertexTexCoord.y * halfL);
    vCorner = vertexTexCoord;
    vFade = vertexColor.a;
    gl_Position = matProjection * vec4(vp, 1.0);
}
)GLSL";

// FS: a soft-edged fleck/streak, alpha = fade x soft radial falloff on the quad.
// Mono color (snow white / rain grey via the caller's opacity). Alpha-blended.
const char* kPrecipFS = R"GLSL(#version 330
in vec2 vCorner;
in float vFade;
uniform vec3 uColor;
out vec4 finalColor;
void main() {
    float r = length(vCorner);
    float a = vFade * smoothstep(1.0, 0.0, r);  // soft edge, 0 at the corner
    if (a <= 0.003) discard;
    finalColor = vec4(uColor, a);
}
)GLSL";

constexpr int kMaxHalfCells = 12;  // (2*12+1)^3 * 4 = 62500 verts < 65536 (ushort)

}  // namespace

PrecipRenderer build_precip_renderer(const PrecipLook& look) {
    PrecipRenderer r;
    r.look = look;
    const double s = look.cell_size_m > 0.0f ? look.cell_size_m : 1.0;
    // Size the iterated block so the fade sphere is PROVABLY inside it (Fable W3
    // P0): a flake's nearest excluded cell can be as close as (H-1.5)*cell (jitter
    // is one-sided + the fall subtracts a cell), so H must exceed box_half/cell by
    // 1.5, and the effective fade radius must not exceed (H-1.5)*cell. That kills
    // both the re-bin pop AND the clamp-wall (when box_half/cell > kMaxHalfCells the
    // fade radius shrinks with H instead of leaving a full-alpha wall at the edge).
    int H = static_cast<int>(std::ceil(look.box_half_m / s + 1.5));
    H = std::clamp(H, 2, kMaxHalfCells);
    r.fade_radius_m =
        std::min(static_cast<double>(look.box_half_m), (H - 1.5) * s);
    if (static_cast<double>(look.box_half_m) / s + 1.5 > kMaxHalfCells)
        TraceLog(LOG_INFO,
                 "PRECIP: box clamped to H=%d cells (fade radius %.1fm) — one "
                 "ushort batch",
                 H, r.fade_radius_m);
    r.half_cells = H;
    const int span = 2 * H + 1;
    r.particle_count = span * span * span;

    // A static index buffer (two tris/quad) + pre-sized vertex/texcoord/color
    // buffers; vertices+colors are refilled each frame (UpdateMeshBuffer). The
    // texcoords (corner offsets) are constant.
    Mesh m{};
    m.vertexCount = r.particle_count * 4;
    m.triangleCount = r.particle_count * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * 2 * m.vertexCount));
    m.colors = static_cast<unsigned char*>(
        MemAlloc(sizeof(unsigned char) * 4 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));
    static const float corner[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int p = 0; p < r.particle_count; ++p) {
        for (int v = 0; v < 4; ++v) {
            const int vi = p * 4 + v;
            m.texcoords[vi * 2 + 0] = corner[v][0];
            m.texcoords[vi * 2 + 1] = corner[v][1];
            m.vertices[vi * 3 + 0] = 0.0f;  // filled per frame
            m.vertices[vi * 3 + 1] = 0.0f;
            m.vertices[vi * 3 + 2] = 0.0f;
            m.colors[vi * 4 + 0] = 255;
            m.colors[vi * 4 + 1] = 255;
            m.colors[vi * 4 + 2] = 255;
            m.colors[vi * 4 + 3] = 0;  // culled until filled
        }
        const unsigned short b = static_cast<unsigned short>(p * 4);
        const int ti = p * 6;
        m.indices[ti + 0] = b;
        m.indices[ti + 1] = b + 1;
        m.indices[ti + 2] = b + 2;
        m.indices[ti + 3] = b;
        m.indices[ti + 4] = b + 2;
        m.indices[ti + 5] = b + 3;
    }
    UploadMesh(&m, /*dynamic=*/true);  // vertices+colors change each frame
    r.mesh = m;

    r.shader = LoadShaderFromMemory(kPrecipVS, kPrecipFS);
    r.loc_upview = GetShaderLocation(r.shader, "uUpView");
    r.loc_halfw = GetShaderLocation(r.shader, "uHalfW");
    r.loc_halfl = GetShaderLocation(r.shader, "uHalfL");
    r.loc_color = GetShaderLocation(r.shader, "uColor");
    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.verts.assign(static_cast<std::size_t>(3) * 4 * r.particle_count, 0.0f);
    r.cols.assign(static_cast<std::size_t>(4) * 4 * r.particle_count, 0);
    r.ok = true;
    TraceLog(LOG_INFO, "PRECIP: %d particles (H=%d, cell=%.1fm)",
             r.particle_count, H, static_cast<double>(look.cell_size_m));
    return r;
}

void unload_precip_renderer(PrecipRenderer& r) {
    if (!r.ok) return;
    UnloadMesh(r.mesh);
    UnloadShader(r.shader);  // LoadMaterialDefault maps are raylib-owned
    r.ok = false;
}

void draw_precip_renderer(PrecipRenderer& r, const CameraPose& pose, Season season,
                          double intensity, double fall_phase) {
    if (!r.ok) return;
    // Season gate: Winter => snow, EVERY other season => rain.
    //
    // S-wetseason (Chad's fly, 2026-08-09: "I didnt see any weather in the
    // bubbles"). SUPERSEDES W3's "Summer/Autumn = DRY". That rule made HALF of
    // all flights structurally incapable of showing precipitation — the season
    // is a uniform random draw at spawn, so two of the four outcomes returned
    // here before the weather field was ever consulted, and the only remaining
    // cue was a subtle haze thickening. The anchored in-dome squalls were
    // firing; there was simply nothing drawn for them in a dry season.
    // Summer rain is also the honest reading of "micro events" — a squall you
    // can fly into. Winter still owns snow (and the W4 ground snow-cover).
    float half_w, half_l, opacity;
    if (season == Season::Winter) {
        half_w = half_l = r.look.snow_size_m;  // round
        opacity = r.look.snow_opacity;
    } else {
        half_w = r.look.rain_size_m;
        half_l = r.look.rain_streak_m;  // elongated along local_up
        opacity = r.look.rain_opacity;
    }
    // Weather gate: only under an active microsystem (fades as you leave). Clear
    // air / between cells => nothing to draw.
    if (intensity <= 0.004) return;

    const glm::dvec3 eye = pose.eye;
    const double r_eye = glm::length(eye);
    if (r_eye <= 0.0) return;
    const glm::dvec3 local_up = eye / r_eye;  // unit radial (never a fixed axis)
    const double cell = r.look.cell_size_m;
    const double box_half = r.fade_radius_m;  // Fable P0: <= (H-1.5)*cell => in-block
    const double wrap = r.look.wrap_fade;
    const glm::ivec3 c0 = precip_center_cell(eye, cell);
    const int H = r.half_cells;
    const double alpha_scale = opacity * std::clamp(intensity, 0.0, 1.0);

    // Fill the per-frame vertex + color buffers from the pure sampler.
    int p = 0;
    for (int di = -H; di <= H; ++di)
        for (int dj = -H; dj <= H; ++dj)
            for (int dk = -H; dk <= H; ++dk) {
                const glm::ivec3 cellijk(c0.x + di, c0.y + dj, c0.z + dk);
                const PrecipSample s = precip_sample(cellijk, eye, local_up, cell,
                                                     box_half, wrap, fall_phase);
                // Vertices are world positions (matModel translates by -eye); near
                // the eye (within box_half) the float cast is safe.
                const glm::vec3 wp = glm::vec3(s.pos);
                const unsigned char a = static_cast<unsigned char>(
                    std::clamp(s.alpha * alpha_scale, 0.0, 1.0) * 255.0 + 0.5);
                for (int v = 0; v < 4; ++v) {
                    const int vi = (p * 4 + v);
                    r.verts[vi * 3 + 0] = wp.x;
                    r.verts[vi * 3 + 1] = wp.y;
                    r.verts[vi * 3 + 2] = wp.z;
                    r.cols[vi * 4 + 0] = 255;
                    r.cols[vi * 4 + 1] = 255;
                    r.cols[vi * 4 + 2] = 255;
                    r.cols[vi * 4 + 3] = a;
                }
                ++p;
            }
    UpdateMeshBuffer(r.mesh, 0, r.verts.data(),
                     static_cast<int>(sizeof(float) * r.verts.size()), 0);
    UpdateMeshBuffer(r.mesh, 3, r.cols.data(),
                     static_cast<int>(sizeof(unsigned char) * r.cols.size()), 0);

    // local_up in VIEW space (for the streak orientation): project onto the camera
    // basis. The view basis: -fwd = view +z, right = view +x, trueUp = view +y.
    const glm::dvec3 fwd = glm::normalize(pose.target - eye);
    glm::dvec3 right = glm::cross(fwd, pose.up);
    const double rl = glm::length(right);
    right = rl > 1e-9 ? right / rl : glm::dvec3(1, 0, 0);
    const glm::dvec3 vup = glm::cross(right, fwd);
    const glm::vec3 up_view(static_cast<float>(glm::dot(local_up, right)),
                            static_cast<float>(glm::dot(local_up, vup)),
                            static_cast<float>(glm::dot(local_up, -fwd)));
    SetShaderValue(r.shader, r.loc_upview, &up_view, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_halfw, &half_w, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_halfl, &half_l, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_color, &r.look.color, SHADER_UNIFORM_VEC3);

    // Eye-relative: vertices are world positions, matModel translates by -eye.
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    // Alpha over the scene; depth-test ON (terrain/planes occlude flakes behind
    // them) but depth-WRITE off (flakes never occlude each other). Cull off (the
    // billboard can wind either way).
    BeginBlendMode(BLEND_ALPHA);
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawMesh(r.mesh, r.mat, xf);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

}  // namespace render
