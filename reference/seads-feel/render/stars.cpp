#include "render/stars.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "render/rig.h"           // to_ray_fields (glm::mat4 -> raylib fields)
#include "render/scatter_glsl.h"  // kScatterGLSL (sky_color -> scatter dependency)
#include "render/star_catalog.gen.h"  // kStarCatalog (real HYG data)
#include "render/star_glsl.h"         // kStarVS + kStarFsBody (validated core)
#include "rlgl.h"

namespace render {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Assemble the full star FS: the shared sky GLSL (uniform block + scatter +
// sky/haze functions) followed by the hand-authored, asset-validated star body
// (its main() calls the shared sky_luminance for extinction). kStarVS + this
// order match how sky.cpp / planet.cpp concatenate the same shared strings.
std::string star_fs() {
    // DECLS before the shared sky GLSL (uHorizonElev must be visible when
    // sky_luminance is defined); MAIN after (it calls sky_luminance AND the
    // S-airdome march).
    return std::string("#version 330\n") + kStarFsDecls + kSkyUniformsGLSL +
           kAirFieldUniformsGLSL + kScatterGLSL + kAirFieldGLSL + kSkyGLSL +
           kStarFsMain;
}

// Build the static star mesh: 4 verts/star (a quad), packing the inertial
// catalog dir (position), the corner offset (texcoord), and the magnitude
// (normal.x). Culls fainter than mag_limit. The ushort index buffer caps vertex
// count at 65535 => <= 16383 stars; the baked catalog (mag <= 5.5) is well
// under.
Mesh build_star_mesh(const CelestialParams& cel, float mag_limit, int& kept) {
    const float kD2R = static_cast<float>(kPi / 180.0);
    std::vector<int> keep;
    keep.reserve(kStarCatalogCount);
    for (std::size_t i = 0; i < kStarCatalogCount; ++i)
        if (kStarCatalog[i].mag <= mag_limit)
            keep.push_back(static_cast<int>(i));
    if (keep.size() > 16383) keep.resize(16383);  // ushort index safety
    kept = static_cast<int>(keep.size());

    Mesh m = {};
    m.vertexCount = kept * 4;
    m.triangleCount = kept * 2;
    m.vertices =
        static_cast<float*>(MemAlloc(sizeof(float) * m.vertexCount * 3));
    m.texcoords =
        static_cast<float*>(MemAlloc(sizeof(float) * m.vertexCount * 2));
    m.normals =
        static_cast<float*>(MemAlloc(sizeof(float) * m.vertexCount * 3));
    m.indices = static_cast<unsigned short*>(
        MemAlloc(sizeof(unsigned short) * m.triangleCount * 3));

    const float corners[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int s = 0; s < kept; ++s) {
        const CatalogStar& star = kStarCatalog[keep[s]];
        const glm::dvec3 d =
            radec_to_dir(cel, star.ra_deg * kD2R, star.dec_deg * kD2R);
        const float dx = static_cast<float>(d.x), dy = static_cast<float>(d.y),
                    dz = static_cast<float>(d.z);
        for (int v = 0; v < 4; ++v) {
            const int vi = s * 4 + v;
            m.vertices[vi * 3 + 0] = dx;
            m.vertices[vi * 3 + 1] = dy;
            m.vertices[vi * 3 + 2] = dz;
            m.texcoords[vi * 2 + 0] = corners[v][0];
            m.texcoords[vi * 2 + 1] = corners[v][1];
            m.normals[vi * 3 + 0] = star.mag;
            m.normals[vi * 3 + 1] = 0.0f;
            m.normals[vi * 3 + 2] = 0.0f;
        }
        const unsigned short b = static_cast<unsigned short>(s * 4);
        const int ti = s * 6;
        m.indices[ti + 0] = b;
        m.indices[ti + 1] = b + 1;
        m.indices[ti + 2] = b + 2;
        m.indices[ti + 3] = b;
        m.indices[ti + 4] = b + 2;
        m.indices[ti + 5] = b + 3;
    }
    UploadMesh(&m, /*dynamic=*/false);
    return m;
}

}  // namespace

StarRenderer load_stars(const CelestialParams& cel, const StarParams& sp) {
    StarRenderer s;
    const std::string fs = star_fs();
    s.shader = LoadShaderFromMemory(kStarVS, fs.c_str());
    if (s.shader.id == 0) {
        TraceLog(LOG_WARNING, "STARS: shader compile failed");
        return s;
    }
    s.mesh = build_star_mesh(cel, sp.mag_limit, s.count);
    s.loc_sun_dir = GetShaderLocation(s.shader, "uSunDir");
    s.loc_up = GetShaderLocation(s.shader, "uUp");
    s.loc_eye_alt = GetShaderLocation(s.shader, "uEyeAlt");
    s.loc_horizon_elev = GetShaderLocation(s.shader, "uHorizonElev");
    s.loc_brightness = GetShaderLocation(s.shader, "uStarBrightness");
    s.loc_mag_ref = GetShaderLocation(s.shader, "uMagRef");
    s.loc_size_view = GetShaderLocation(s.shader, "uStarSizeView");
    s.loc_glare_cos = GetShaderLocation(s.shader, "uGlareCos");
    s.loc_wash_lum = GetShaderLocation(s.shader, "uWashLum");
    s.loc_star_air_ext = GetShaderLocation(s.shader, "uStarAirExt");
    s.loc_r_star = GetShaderLocation(s.shader, "uRStar");
    s.loc_moon_dir = GetShaderLocation(s.shader, "uMoonDir");
    s.loc_moon_ang = GetShaderLocation(s.shader, "uMoonAngRad");
    s.loc_sky_space = GetShaderLocation(s.shader, "uSkySpace");
    s.loc_sky_band_top = GetShaderLocation(s.shader, "uSkyBandTop");
    s.loc_sky_day = GetShaderLocation(s.shader, "uSkyDay");
    s.loc_sky_dusk = GetShaderLocation(s.shader, "uSkyDusk");
    s.loc_sky_night = GetShaderLocation(s.shader, "uSkyNight");
    s.loc_dusk_lo = GetShaderLocation(s.shader, "uDuskLo");
    s.loc_dusk_hi = GetShaderLocation(s.shader, "uDuskHi");
    s.loc_night_fill = GetShaderLocation(s.shader, "uNightFill");
    s.loc_dither = GetShaderLocation(s.shader, "uDither");
    s.loc_weather_amt = GetShaderLocation(s.shader, "uWeatherAmt");
    s.loc_haze_scale = GetShaderLocation(s.shader, "uHazeScale");
    s.loc_rayleigh = GetShaderLocation(s.shader, "u_rayleigh");
    s.loc_mie = GetShaderLocation(s.shader, "u_mie");
    s.loc_scatter_strength = GetShaderLocation(s.shader, "u_scatterStrength");
    s.loc_mie_g = GetShaderLocation(s.shader, "u_mieG");
    s.air_locs = air_field_locs(s.shader);
    s.mat = LoadMaterialDefault();
    s.mat.shader = s.shader;
    s.ok = true;
    TraceLog(LOG_INFO, "STARS: %d stars (mag <= %.1f)", s.count, sp.mag_limit);
    return s;
}

void unload_stars(StarRenderer& s) {
    if (!s.ok) return;
    UnloadMesh(s.mesh);
    UnloadShader(s.shader);
    s.ok = false;
}

void draw_stars(const StarRenderer& s, const glm::mat3& wheel,
                const glm::vec3& sun_dir, const glm::vec3& up, float eye_alt,
                float horizon_elev, const StarParams& sp,
                const AtmosphereParams& atm, float fovy_deg, int viewport_h,
                const glm::vec3& moon_dir, float moon_ang_rad,
                const AirField& air) {
    if (!s.ok || s.count == 0) return;

    // The star depth is cosmetic (eye-anchored => zero parallax) but must sit
    // INSIDE the clip volume — depth-test off does NOT bypass frustum clipping,
    // so an r_star past the far plane clips every star out (Fable-after P1-5).
    // Clamp config-relative to the actual far plane; the size compensates so
    // the on-screen pixel size is unchanged by the clamp.
    const float far_plane = static_cast<float>(rlGetCullDistanceFar());
    const float r_star = std::min(sp.r_star_m, 0.95f * far_plane);
    // FOV-compensated constant pixel size: a fixed on-screen core so S9 RMB
    // aim-zoom does NOT inflate the stars. size ∝ r_star·tan(fov/2).
    const float fov = fovy_deg * static_cast<float>(kPi / 180.0);
    const float size_view = sp.size_px * 2.0f * r_star * std::tan(0.5f * fov) /
                            std::max(1.0f, static_cast<float>(viewport_h));
    SetShaderValue(s.shader, s.loc_r_star, &r_star, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_size_view, &size_view, SHADER_UNIFORM_FLOAT);

    const float sd[3] = {sun_dir.x, sun_dir.y, sun_dir.z};
    const float up3[3] = {up.x, up.y, up.z};
    SetShaderValue(s.shader, s.loc_sun_dir, sd, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_up, up3, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_eye_alt, &eye_alt, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_horizon_elev, &horizon_elev,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_brightness, &sp.brightness,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_mag_ref, &sp.mag_ref, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_glare_cos, &sp.glare_suppress_cos,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_star_air_ext, &sp.air_extinction,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_wash_lum, &sp.wash_lum,
                   SHADER_UNIFORM_FLOAT);
    const float mdir[3] = {moon_dir.x, moon_dir.y, moon_dir.z};
    SetShaderValue(s.shader, s.loc_moon_dir, mdir, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_moon_ang, &moon_ang_rad,
                   SHADER_UNIFORM_FLOAT);
    set_atmosphere_uniforms(
        s.shader, atm, s.loc_sky_space, s.loc_sky_band_top, s.loc_sky_day,
        s.loc_sky_dusk, s.loc_sky_night, s.loc_dusk_lo, s.loc_dusk_hi,
        s.loc_night_fill, s.loc_dither, s.loc_weather_amt, s.loc_haze_scale,
        s.loc_rayleigh, s.loc_mie, s.loc_scatter_strength, s.loc_mie_g);
    set_air_field_uniforms(s.shader, s.air_locs, air);

    // The sky WHEEL as the DrawMesh model matrix (matModel). glm mat3 -> mat4
    // -> raylib field order (designated-init so a struct reorder can't
    // transpose).
    const std::array<float, 16> f = to_ray_fields(glm::mat4(wheel));
    const Matrix wm{.m0 = f[0],
                    .m4 = f[1],
                    .m8 = f[2],
                    .m12 = f[3],
                    .m1 = f[4],
                    .m5 = f[5],
                    .m9 = f[6],
                    .m13 = f[7],
                    .m2 = f[8],
                    .m6 = f[9],
                    .m10 = f[10],
                    .m14 = f[11],
                    .m3 = f[12],
                    .m7 = f[13],
                    .m11 = f[14],
                    .m15 = f[15]};

    // Additive over the already-drawn sky; depth test AND write off (the planet
    // drawn after occludes below-horizon stars for free — the sun-disc
    // pattern).
    BeginBlendMode(BLEND_ADDITIVE);
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    DrawMesh(s.mesh, s.mat, wm);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    EndBlendMode();
}

}  // namespace render
