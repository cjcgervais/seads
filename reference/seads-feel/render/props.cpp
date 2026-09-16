#include "render/props.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "raymath.h"  // MatrixToFloatV / MatrixMultiply for the persistent instance buffer
#include "render/rig.h"           // to_ray_fields (glm mat4 -> raylib Matrix)
#include "render/sphere_param.h"  // HeightField
#include "rlgl.h"

namespace render {

namespace {

// Cap on how many new chunks are placed+packed per frame (amortize a boundary
// crossing so it never stalls in one frame; trees fade in, so a couple frames
// of streaming is invisible).
constexpr int kMaxChunkBuildsPerFrame = 2;

// Instanced VS: a per-instance WORLD-ABSOLUTE transform (persistent VRAM buffer)
// rebased to the eye on the GPU (uEye), a camera-uniform scale FADE toward the
// base (depth-safe — opaque geometry, no alpha; Fable H), and a mono lambert
// against the sun. Bound via SHADER_LOC_MATRIX_MODEL (instanceTransform) + mvp.
const char* kPropsVS = R"GLSL(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in mat4 instanceTransform;   // WORLD-ABSOLUTE transform (persistent VRAM buffer)
uniform mat4 mvp;
uniform vec3 uEye;         // world eye position: the per-frame camera rebase, done on the GPU
uniform vec3 uSunDir;      // world sun direction (eye-relative frame == world dirs)
uniform float uFadeStart;  // slant range (m) where the scale fade begins
uniform float uFadeEnd;    // slant range (m) where scale reaches 0
uniform float uAmbient;
uniform float uDiffuse;
out float vShade;
void main() {
    // The transform is world-absolute; (tree - eye) is the slant vector to the
    // tree (camera at origin, eye-relative scene). The GPU does the rebase, so a
    // cached buffer is never invalidated by camera motion — it never re-uploads.
    vec3 slant = instanceTransform[3].xyz - uEye;
    float dist = length(slant);
    float fade = 1.0 - smoothstep(uFadeStart, uFadeEnd, dist);
    vec3 vlocal = vertexPosition * fade;   // scale toward the root (local origin pinned)
    vec4 wpos = instanceTransform * vec4(vlocal, 1.0);
    wpos.xyz -= uEye;
    // The mesh normals are HORIZONTAL (nx,0,nz); the instance columns are
    // orthogonal with a non-uniform species scale, so mat3(M)*N and the correct
    // inverse-transpose agree after normalize ONLY because the up (scaled) row is
    // dead in a horizontal normal (Fable-after P2). A future slanted-quad normal
    // (y!=0) would need the inverse-transpose.
    vec3 N = normalize(mat3(instanceTransform) * vertexNormal);
    float diff = max(0.0, dot(N, normalize(uSunDir)));
    vShade = uAmbient + uDiffuse * diff;   // MONO — the silver post tones it
    gl_Position = mvp * wpos;
}
)GLSL";

// Mono FS: grayscale (r==g==b) so the S1 post split-tone reads it as SILVER
// terrain (saturation-gated), never as chroma — the planes stay the only color.
const char* kPropsFS = R"GLSL(#version 330
in float vShade;
out vec4 finalColor;
void main() {
    float s = clamp(vShade, 0.0, 1.0);
    finalColor = vec4(vec3(s), 1.0);
}
)GLSL";

// A procedural conifer: three vertical quads crossed at 60° (a fuller
// silhouette than a single billboard, and rotation-stable under sphere roll —
// billboards tumble). Each quad tapers from base_w at the root to a narrow tip
// at height H. Opaque, double-sided (backface culling is disabled at draw).
// Local frame: +Y up, root at y=0.
Mesh build_conifer_mesh(float height_m, float base_w) {
    const int kPlanes = 3;
    const float tip_w = 0.12f * base_w;
    Mesh m{};
    m.vertexCount = kPlanes * 4;
    m.triangleCount = kPlanes * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.normals =
        static_cast<float*>(MemAlloc(sizeof(float) * 3 * m.vertexCount));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * 3 * m.triangleCount));

    int vi = 0, ii = 0;
    for (int k = 0; k < kPlanes; ++k) {
        const float th = static_cast<float>(k) *
                         (3.14159265358979f / static_cast<float>(kPlanes));
        const float wx = std::cos(th), wz = std::sin(th);   // width axis (XZ)
        const float nx = -std::sin(th), nz = std::cos(th);  // plane normal (XZ)
        const int b = vi;
        auto put = [&](float x, float y, float z) {
            m.vertices[vi * 3 + 0] = x;
            m.vertices[vi * 3 + 1] = y;
            m.vertices[vi * 3 + 2] = z;
            m.normals[vi * 3 + 0] = nx;
            m.normals[vi * 3 + 1] = 0.0f;
            m.normals[vi * 3 + 2] = nz;
            ++vi;
        };
        put(-base_w * wx, 0.0f, -base_w * wz);    // base left
        put(base_w * wx, 0.0f, base_w * wz);      // base right
        put(tip_w * wx, height_m, tip_w * wz);    // tip right
        put(-tip_w * wx, height_m, -tip_w * wz);  // tip left
        const unsigned short q0 = static_cast<unsigned short>(b);
        m.indices[ii++] = q0;
        m.indices[ii++] = static_cast<unsigned short>(b + 1);
        m.indices[ii++] = static_cast<unsigned short>(b + 2);
        m.indices[ii++] = q0;
        m.indices[ii++] = static_cast<unsigned short>(b + 2);
        m.indices[ii++] = static_cast<unsigned short>(b + 3);
    }
    UploadMesh(&m, false);
    return m;
}

// The rotation·species·scale columns for one tree (the trig — computed ONCE per
// visible-set change, Fable-after P1-1): radial up + a yaw about it, a
// species-shaped non-uniform scale (spruce tall/narrow, pine mid, birch
// short/broad — one mesh, silhouette via scale, so it stays ONE draw call).
void tree_columns(const world::TreeInstance& ti, glm::vec3& cx, glm::vec3& cy,
                  glm::vec3& cz) {
    const glm::dvec3 up = glm::normalize(ti.up);
    const glm::dvec3 ref =
        std::abs(up.y) < 0.99 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
    const glm::dvec3 right = glm::normalize(glm::cross(ref, up));
    const glm::dvec3 fwd = glm::cross(up, right);
    const double c = std::cos(ti.yaw), s = std::sin(ti.yaw);
    const glm::dvec3 r2 = right * c + fwd * s;  // yaw about up
    const glm::dvec3 f2 = -right * s + fwd * c;
    double hmul = 1.0, wmul = 1.0;  // species silhouette (h, w mul)
    if (ti.species == 0) {
        hmul = 1.15;
        wmul = 0.82;
    }  // spruce
    else if (ti.species == 2) {
        hmul = 0.78;
        wmul = 1.18;
    }  // birch
    const double sc = ti.scale;
    cx = glm::vec3(r2 * (sc * wmul));
    cy = glm::vec3(up * (sc * hmul));
    cz = glm::vec3(f2 * (sc * wmul));
}

// Model matrix from the pre-computed columns + the tree position (translation =
// pos - eye). Called at CHUNK BUILD time with eye = origin, so the packed
// transform is world-absolute; the per-frame eye rebase happens in the VS.
Matrix tree_matrix(const glm::vec3& cx, const glm::vec3& cy,
                   const glm::vec3& cz, const glm::dvec3& pos,
                   const glm::dvec3& eye) {
    const glm::dvec3 t = pos - eye;  // double, THEN cast
    glm::mat4 g(1.0f);
    g[0] = glm::vec4(cx, 0.0f);
    g[1] = glm::vec4(cy, 0.0f);
    g[2] = glm::vec4(cz, 0.0f);
    g[3] = glm::vec4(static_cast<float>(t.x), static_cast<float>(t.y),
                     static_cast<float>(t.z), 1.0f);
    const std::array<float, 16> f = to_ray_fields(g);
    return Matrix{f[0], f[1], f[2],  f[3],  f[4],  f[5],  f[6],  f[7],
                  f[8], f[9], f[10], f[11], f[12], f[13], f[14], f[15]};
}

// Create (first time / on grow) or update the PERSISTENT instance VBO from
// r.upload (16 floats/instance), and bind the 4 mat4 columns as per-instance
// vertex attributes on the mesh VAO (divisor 1). The buffer lives in VRAM;
// same-or-smaller rebuilds only stream new data (glBufferSubData), never
// re-create — the per-frame draw touches none of this. Mirrors the attribute
// layout of raylib's DrawMeshInstanced (stride = sizeof(Matrix), 4×Vector4).
void upload_instances(PropRenderer& r, int n) {
    const int bytes =
        n * 16 * static_cast<int>(sizeof(float));  // 64 B/instance
    if (r.instance_vbo == 0 || n > r.instance_cap) {
        if (r.instance_vbo) rlUnloadVertexBuffer(r.instance_vbo);
        rlEnableVertexArray(r.mesh.vaoId);
        r.instance_vbo = rlLoadVertexBuffer(r.upload.data(), bytes, true);
        const int loc =
            r.shader.locs[SHADER_LOC_MATRIX_MODEL];  // instanceTransform
        const int stride = 16 * static_cast<int>(sizeof(float));
        for (int k = 0; k < 4; ++k) {
            rlEnableVertexAttribute(loc + k);
            rlSetVertexAttribute(loc + k, 4, RL_FLOAT, false, stride,
                                 k * 4 * static_cast<int>(sizeof(float)));
            rlSetVertexAttributeDivisor(loc + k, 1);
        }
        rlDisableVertexArray();
        r.instance_cap = n;
    } else {
        rlUpdateVertexBuffer(r.instance_vbo, r.upload.data(), bytes, 0);
    }
}

}  // namespace

PropRenderer build_props(const std::string& asset_dir,
                         const world::TreeParams& p, const PropLook& look,
                         const std::vector<CutDisk>& cuts,
                         const world::CorridorMask& corridor) {
    PropRenderer r;
    r.params = p;
    r.look = look;
    r.cuts = cuts;
    r.corridor = corridor;
    if (p.gain > 1.0)
        // gain*D can exceed 1 where D=1; the acc-compare clamps it but the face
        // center (jac=1) then saturates before the edges (jac=0.71) -> a mild
        // ~1.4x density bias returns (Fable-after P2). Dial density via a
        // re-bake or cells_per_face past gain=1.
        TraceLog(
            LOG_WARNING,
            "PROPS: [trees] density_gain %.2f > 1 — face-center density bias "
            "possible; prefer gain<=1",
            p.gain);

    const std::string path = asset_dir + "/sudbury_treedensity.png";
    Image img = LoadImage(path.c_str());
    if (img.data == nullptr || img.width <= 0 || img.height <= 0) {
        TraceLog(LOG_WARNING,
                 "PROPS: %s missing — trees disabled (degraded, no scatter)",
                 path.c_str());
        return r;  // ok=false
    }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
    r.density.w = img.width;
    r.density.h = img.height;
    r.density.px.assign(static_cast<const std::uint8_t*>(img.data),
                        static_cast<const std::uint8_t*>(img.data) +
                            static_cast<std::size_t>(img.width) * img.height);
    UnloadImage(img);

    r.mesh = build_conifer_mesh(static_cast<float>(p.tree_height_m),
                                look.base_width_m);
    r.shader = LoadShaderFromMemory(kPropsVS, kPropsFS);
    r.shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(r.shader, "mvp");
    r.shader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(r.shader, "instanceTransform");
    r.loc_sun = GetShaderLocation(r.shader, "uSunDir");
    r.loc_fade_start = GetShaderLocation(r.shader, "uFadeStart");
    r.loc_fade_end = GetShaderLocation(r.shader, "uFadeEnd");
    r.loc_eye = GetShaderLocation(r.shader, "uEye");
    const int loc_amb = GetShaderLocation(r.shader, "uAmbient");
    const int loc_dif = GetShaderLocation(r.shader, "uDiffuse");
    SetShaderValue(r.shader, loc_amb, &r.look.ambient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, loc_dif, &r.look.diffuse, SHADER_UNIFORM_FLOAT);

    r.mat = LoadMaterialDefault();
    r.mat.shader = r.shader;
    r.ok = true;
    TraceLog(LOG_INFO,
             "PROPS: tree scatter ready (%dx%d density, %d cells/face)",
             r.density.w, r.density.h, p.cells_per_face);
    return r;
}

void unload_props(PropRenderer& r) {
    if (!r.ok) return;
    if (r.instance_vbo) rlUnloadVertexBuffer(r.instance_vbo);
    UnloadMesh(r.mesh);
    UnloadShader(r.shader);
    // LoadMaterialDefault's shader is ours (unloaded above); the default maps
    // are raylib-owned — do not UnloadMaterial (it would free the shared
    // default tex).
    r.ok = false;
}

void draw_props(PropRenderer& r, const HeightField& hf,
                const glm::vec3& sun_dir, const glm::dvec3& eye) {
    if (!r.ok) return;
    // Single-source the equirect longitude alignment from the height field (the
    // same u_offset the map + mesh use) so density samples where the terrain
    // is.
    r.density.u_offset = hf.u_offset;

    std::vector<std::uint32_t> vis = world::visible_chunks(eye, hf, r.params);
    std::sort(vis.begin(), vis.end());
    const int G = world::chunks_per_face(r.params);

    // Place + pack at most a FEW not-yet-cached visible chunks this frame
    // (amortized so a boundary crossing that brings several new chunks doesn't
    // stall in one frame — trees fade in, so streaming over a couple frames is
    // invisible). Each chunk's WORLD-ABSOLUTE float16 buffer is built ONCE here
    // and reused forever; camera motion never recomputes it.
    int builds = 0;
    for (std::uint32_t key : vis) {
        if (builds >= kMaxChunkBuildsPerFrame) break;
        if (r.cache.count(key)) continue;
        const int face = static_cast<int>(key / (G * G));
        const int rem = static_cast<int>(key % (G * G));
        const long drops0 = world::g_snowhill_drops;
        const auto inst =
            world::place_chunk(face, rem / G, rem % G, hf, r.density, r.params,
                               r.cuts, r.corridor);
        if (world::g_snowhill_drops != drops0)
            TraceLog(LOG_INFO, "SNOWHILL MASK: chunk f%d %d/%d dropped %ld",
                     face, rem / G, rem % G,
                     world::g_snowhill_drops - drops0);
        std::vector<float> buf(inst.size() * 16);
        for (std::size_t i = 0; i < inst.size(); ++i) {
            glm::vec3 cx, cy, cz;
            tree_columns(inst[i], cx, cy, cz);
            const Matrix m = tree_matrix(cx, cy, cz, inst[i].pos,
                                         glm::dvec3(0.0));  // world-abs
            const float16 f = MatrixToFloatV(m);
            std::copy(f.v, f.v + 16, buf.begin() + i * 16);
        }
        r.cache.emplace(key, std::move(buf));
        ++builds;
    }

    // The READY visible set = visible chunks already cached (vis is sorted, so
    // ready is too). Re-concatenate + re-upload ONLY when it changes.
    std::vector<std::uint32_t> ready;
    ready.reserve(vis.size());
    for (std::uint32_t key : vis)
        if (r.cache.count(key)) ready.push_back(key);

    if (ready != r.ready) {
        r.ready = ready;
        // Evict cached chunks no longer visible once the map grows past a
        // working-set bound (GO-ANYWHERE memory; Fable-after P1-2).
        if (r.cache.size() > 96) {
            for (auto it = r.cache.begin(); it != r.cache.end();) {
                if (!std::binary_search(vis.begin(), vis.end(), it->first))
                    it = r.cache.erase(it);
                else
                    ++it;
            }
        }
        std::size_t total = 0;
        for (std::uint32_t key : ready) total += r.cache[key].size();
        r.upload.resize(total);
        std::size_t off = 0;
        for (std::uint32_t key : ready) {
            const auto& b = r.cache[key];
            std::copy(b.begin(), b.end(), r.upload.begin() + off);
            off += b.size();
        }
        r.instance_count = static_cast<int>(total / 16);
        if (r.instance_count > 0) upload_instances(r, r.instance_count);
        if (r.instance_count > 0 && !r.warned_empty) {
            TraceLog(LOG_INFO,
                     "PROPS: %d trees over %zu chunks (persistent VRAM)",
                     r.instance_count, ready.size());
            r.warned_empty = true;
        }
    }
    if (r.instance_count <= 0) return;

    // The count==1 instancing trap ships GREEN off the build; the data-driven
    // count (never a literal) + test_asset_validator's >1-over-dense guarantee
    // guard it.
    assert(r.instance_count > 0);

    // Per-frame: ONLY uniforms. The camera rebase (world eye) is a single vec3;
    // the whole instance buffer stays put in VRAM.
    const glm::vec3 eye_w = glm::vec3(eye);
    const double reach = world::effective_reach(eye, hf, r.params);
    const float fade_end = static_cast<float>(reach);
    // Keep fade_start strictly below fade_end (fade_frac==0 would make
    // smoothstep(e,e,x) undefined in GLSL — Fable-after P2).
    const float fade_start = static_cast<float>(
        reach *
        (1.0 - std::max(1.0e-3, static_cast<double>(r.look.fade_frac))));
    const Matrix mvp =
        MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());

    SetShaderValueMatrix(r.shader, r.shader.locs[SHADER_LOC_MATRIX_MVP], mvp);
    SetShaderValue(r.shader, r.loc_eye, &eye_w, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_sun, &sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(r.shader, r.loc_fade_start, &fade_start,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(r.shader, r.loc_fade_end, &fade_end, SHADER_UNIFORM_FLOAT);

    rlDisableBackfaceCulling();  // cross-quads read from both sides
    rlEnableShader(r.shader.id);
    rlEnableVertexArray(r.mesh.vaoId);
    rlDrawVertexArrayElementsInstanced(0, r.mesh.triangleCount * 3, 0,
                                       r.instance_count);
    rlDisableVertexArray();
    rlDisableShader();
    rlEnableBackfaceCulling();
}

}  // namespace render
