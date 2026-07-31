#include "render/planet.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/geometric.hpp>

#include "raymath.h"
#include "rlgl.h"

namespace render {

namespace {

// --- GLSL (330) --------------------------------------------------------------
// Per-fragment equirectangular sampling from the interpolated surface
// direction: no vertex UV seam, exact poles. Seam-safe mip gradients via
// textureGrad (unwrap the u derivative across the ±180° meridian). One
// directional sun + ambient so displaced terrain shades. Graticule derived
// from the same (u,v) — coplanar, no wire shell.
const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matNormal;
out vec3 fragNormal;
out vec3 fragDir;
void main() {
    fragDir = normalize(vertexPosition);      // planet-local surface direction
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* kFS = R"(#version 330
in vec3 fragNormal;
in vec3 fragDir;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 sunDir;      // world light-travel dir (sun -> scene)
uniform vec4 gridColor;   // rgb + alpha
uniform float uOffset;    // longitude alignment [0,1)
out vec4 finalColor;
const float PI = 3.14159265359;
void main() {
    vec3 d = normalize(fragDir);
    float u = fract(0.5 + atan(d.z, d.x) / (2.0 * PI) + uOffset);
    float v = clamp(0.5 - asin(clamp(d.y, -1.0, 1.0)) / PI, 0.0, 1.0);
    // Seam-safe gradients: unwrap the u-jump so the ±180° column doesn't
    // pull the mip selector to the coarsest level (a blurred seam band).
    vec2 dx = vec2(dFdx(u), dFdx(v));
    vec2 dy = vec2(dFdy(u), dFdy(v));
    dx.x = fract(dx.x + 0.5) - 0.5;
    dy.x = fract(dy.x + 0.5) - 0.5;
    vec3 albedo = textureGrad(texture0, vec2(u, v), dx, dy).rgb * colDiffuse.rgb;

    float ndl = max(dot(normalize(fragNormal), -normalize(sunDir)), 0.0);
    // Ambient keeps the night side readable (arcade: always see the planet);
    // the diffuse term shades terrain slopes so relief reads.
    vec3 lit = albedo * (0.38 + 0.75 * ndl);

    // Graticule every 15° (24 lon x 12 lat), 1px AA. Unwrap the u-derivative
    // so the ±180° seam column doesn't get a blown-up AA width (a false
    // full-strength meridian) when uOffset is off a gridline (red-team MINOR).
    vec2 g = vec2(u * 24.0, v * 12.0);
    float dux = (fract(dFdx(u) + 0.5) - 0.5) * 24.0;
    float duy = (fract(dFdy(u) + 0.5) - 0.5) * 24.0;
    vec2 gw = vec2(abs(dux) + abs(duy), fwidth(g.y));
    vec2 gd = abs(fract(g) - 0.5) / max(gw, vec2(1e-5));
    float line = min(gd.x, gd.y);
    float grid = 1.0 - clamp(line - 0.5, 0.0, 1.0);
    lit = mix(lit, gridColor.rgb, grid * gridColor.a);

    finalColor = vec4(lit, 1.0);
}
)";

// Upload a pure FaceMesh (built by render::fill_face — the shared, testable
// cubesphere geometry in sphere_param.h) into a raylib Mesh on the GPU. No
// texcoords: the shader samples the albedo from the interpolated direction
// (kFS), so per-vertex UVs were dead weight (removed in the sphere_param
// extraction). This is the ONLY raylib-facing half of the geometry now.
Mesh upload_face(const FaceMesh& fm) {
    Mesh m = {};
    m.vertexCount = static_cast<int>(fm.positions.size() / 3);
    m.triangleCount = static_cast<int>(fm.indices.size() / 3);
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * fm.positions.size()));
    m.normals =
        static_cast<float*>(MemAlloc(sizeof(float) * fm.normals.size()));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * fm.indices.size()));
    std::memcpy(m.vertices, fm.positions.data(),
                sizeof(float) * fm.positions.size());
    std::memcpy(m.normals, fm.normals.data(),
                sizeof(float) * fm.normals.size());
    std::memcpy(m.indices, fm.indices.data(),
                sizeof(unsigned short) * fm.indices.size());
    UploadMesh(&m, false);
    return m;
}

}  // namespace

Planet load_planet(const char* asset_dir, double R, double relief_scale,
                   int subdiv, double u_offset, int dem_blur_radius) {
    Planet p;
    p.R = R;
    if (subdiv < 2) subdiv = 2;
    if (subdiv > 256) subdiv = 256;
    if (dem_blur_radius < 0) dem_blur_radius = 0;

    char color_path[512], dem_path[512];
    std::snprintf(color_path, sizeof color_path, "%s/earth_color_5400.jpg",
                  asset_dir);
    std::snprintf(dem_path, sizeof dem_path, "%s/earth_dem_2048.png",
                  asset_dir);

    Image dem_img = LoadImage(dem_path);
    if (dem_img.data == nullptr) {
        TraceLog(LOG_WARNING, "PLANET: DEM load failed: %s", dem_path);
        return p;
    }
    // Soften sub-quad DEM detail the mesh can't resolve (reduces shading-normal
    // noise on steep coasts before it reaches the normals). Radius is coupled
    // to relief_scale (a heavier displacement needs a heavier blur) — tuned in
    // config/world.toml, never a bare number here.
    if (dem_blur_radius > 0) ImageBlurGaussian(&dem_img, dem_blur_radius);
    Color* px = LoadImageColors(dem_img);
    const int dw = dem_img.width, dh = dem_img.height;
    UnloadImage(dem_img);

    // Retain the PROCESSED (post-blur) height field as the SINGLE SOURCE the
    // mesh is built from and the future props / airstrip ground-contact read
    // (the H1 anti-fork; §1/§2). Copy the red channel; the raylib buffer is
    // then freed.
    p.height.w = dw;
    p.height.h = dh;
    p.height.R = R;
    p.height.relief_scale = relief_scale;
    p.height.u_offset = u_offset;
    p.height.px.resize(static_cast<std::size_t>(dw) * dh);
    for (std::size_t i = 0; i < p.height.px.size(); ++i)
        p.height.px[i] = px[i].r;
    UnloadImageColors(px);

    // Mesh and height field now read ONE source (p.height):
    // fill_face(p.height,…)
    // == p.height.radius_at at every vertex (pinned in test_sphere_param).
    for (int i = 0; i < 6; ++i)
        p.faces[i] = upload_face(fill_face(p.height, i, subdiv));

    p.color = LoadTexture(color_path);
    if (p.color.id == 0) {
        TraceLog(LOG_WARNING, "PLANET: color load failed: %s", color_path);
        for (int i = 0; i < 6; ++i) UnloadMesh(p.faces[i]);
        return p;
    }
    GenTextureMipmaps(&p.color);
    SetTextureFilter(p.color, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(p.color, TEXTURE_WRAP_REPEAT);

    p.shader = LoadShaderFromMemory(kVS, kFS);
    p.loc_sun_dir = GetShaderLocation(p.shader, "sunDir");
    p.loc_grid_color = GetShaderLocation(p.shader, "gridColor");
    p.loc_u_offset = GetShaderLocation(p.shader, "uOffset");

    p.mat = LoadMaterialDefault();
    p.mat.shader = p.shader;
    p.mat.maps[MATERIAL_MAP_DIFFUSE].texture = p.color;
    p.mat.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;

    float grid[4] = {0.85f, 0.90f, 1.0f, 0.18f};  // faint cool graticule
    float uoff = static_cast<float>(u_offset - std::floor(u_offset));
    SetShaderValue(p.shader, p.loc_grid_color, grid, SHADER_UNIFORM_VEC4);
    SetShaderValue(p.shader, p.loc_u_offset, &uoff, SHADER_UNIFORM_FLOAT);

    p.ok = true;
    TraceLog(LOG_INFO, "PLANET: built cubesphere %d faces, %d verts/face", 6,
             subdiv * subdiv);
    return p;
}

void unload_planet(Planet& p) {
    if (!p.ok) return;
    for (int i = 0; i < 6; ++i) UnloadMesh(p.faces[i]);
    UnloadTexture(p.color);
    UnloadShader(p.shader);
    // mat uses the shared shader/texture already unloaded; free the struct's
    // default maps array via UnloadMaterial would double-free the texture, so
    // just detach and drop.
    p.ok = false;
}

void draw_planet_mesh(const Planet& p, const glm::dvec3& eye,
                      const glm::vec3& sun_dir) {
    if (!p.ok) return;
    float sd[3] = {sun_dir.x, sun_dir.y, sun_dir.z};
    SetShaderValue(p.shader, p.loc_sun_dir, sd, SHADER_UNIFORM_VEC3);
    Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    for (int i = 0; i < 6; ++i) DrawMesh(p.faces[i], p.mat, xf);
}

}  // namespace render
