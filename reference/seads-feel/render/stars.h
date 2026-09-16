#pragma once

#include <glm/glm.hpp>

#include "raylib.h"
#include "render/celestial.h"  // CelestialParams (radec_to_dir at mesh build)
#include "render/sky.h"        // AtmosphereParams + set_atmosphere_uniforms

// Star field pass (docs/little_planet_plan.md Stage 4). A REAL bright-star
// catalog (render/star_catalog.gen.h, HYG-derived; Chad ruled real data) drawn
// as ONE instanced-quad mesh: 4 verts/star carrying the star's INERTIAL catalog
// unit dir + a corner offset + magnitude. A custom VS billboards each star in
// VIEW space (the star rides the eye at infinity — no eye+offset large-float
// cancellation, Fable-after P1-4); the sky WHEEL reaches it as the finished
// DrawMesh model matrix (matModel), never a raw large t (seam: no clock in
// render/; shaders get finished matrices). The FS makes a fwidth-crisp round
// core (zoom-correct like the sun disc), maps magnitude -> BRIGHTNESS (not
// size), and fades stars where the shared sky_luminance is bright AND under
// haze/overcast (the same weather gate the sky uses — single-source
// extinction).
//
// Draw order (Fable-after P2-3): sky quad -> STARS (additive, depth off) ->
// planet (depth on, drawn after) — the planet overdraws its silhouette so
// below-horizon stars are occluded for free, identical to the sun disc.

namespace render {

// [stars] tuning (config/world.toml, mapped from cfg by the app so render/
// stays free of config/). Angles converted at the config boundary.
struct StarParams {
    float brightness = 0.8f;  // overall gain (celestial.star_brightness)
    float mag_ref = 2.5f;     // magnitude that maps to brightness 1.0 (Pogson)
    float mag_limit = 5.5f;   // cull catalog stars fainter than this at build
    float size_px = 2.2f;  // constant on-screen core half-size [px], FOV-comp
    float glare_suppress_cos =  // cos(glare_suppress_deg): fade stars this near
        0.9998f;                //   the sun (default ~1.1 deg)
    float wash_lum = 0.14f;     // sky luminance above which stars wash out
    // S-starnight (Chad, 2026-08-09): how much the ALWAYS-ON dome air veils
    // stars, [0,1]. 0 = clear dome air never hides the starfield (his ruling —
    // only OVERCAST does, which the weather amount drives); 1 = the original
    // S-airdome behaviour where a dome's own air washed stars out.
    float air_extinction = 0.0f;
    float r_star_m =
        300000.0f;  // cosmetic view-space depth (< far plane; asserted)
};

struct StarRenderer {
    bool ok = false;
    Shader shader = {};
    Mesh mesh = {};  // 4 verts/star; static (built once from the catalog + cel)
    Material mat = {};
    int count = 0;  // stars kept after the mag_limit cull
    // Star-specific uniforms (the [atmosphere]/sun locs are resolved via the
    // shared set_atmosphere_uniforms path below).
    int loc_sun_dir = -1, loc_up = -1, loc_eye_alt = -1, loc_horizon_elev = -1;
    int loc_brightness = -1, loc_mag_ref = -1, loc_size_view = -1;
    int loc_glare_cos = -1, loc_wash_lum = -1, loc_r_star = -1;
    int loc_star_air_ext = -1;  // S-starnight
    int loc_moon_dir = -1, loc_moon_ang = -1;  // occlude stars behind the moon
    int loc_sky_space = -1, loc_sky_band_top = -1;
    int loc_sky_day = -1, loc_sky_dusk = -1, loc_sky_night = -1;
    int loc_dusk_lo = -1, loc_dusk_hi = -1, loc_night_fill = -1;
    int loc_dither = -1, loc_weather_amt = -1, loc_haze_scale = -1;
    int loc_rayleigh = -1, loc_mie = -1, loc_scatter_strength = -1,
        loc_mie_g = -1;
    AirFieldLocs air_locs;  // S-airdome (spec §1.1)
};

// Build the star mesh + shader. `cel` supplies the inertial equatorial basis
// (radec_to_dir); `sp` the visual knobs (mag_limit culls at build). Returns an
// unusable (ok=false) renderer if the shader fails to compile.
StarRenderer load_stars(const CelestialParams& cel, const StarParams& sp);
void unload_stars(StarRenderer& s);

// Draw the star field. `wheel` = sky_wheel(cel, t_cel) as a mat3 (inertial ->
// world), passed as the DrawMesh model matrix; `sun_dir` = eye-relative world
// light-travel dir (shared with the sky pass so extinction/glare match); `up` =
// normalize(eye); `eye_alt` = altitude [m]; `horizon_elev` = the limb dip. Runs
// inside BeginMode3D (matView/matProjection come from the mode3d context, so
// the lens-shift/zoom frustum applies). Additive blend, depth test/write off.
// air = the S-airdome AirField (spec §1.6/§1.9).
void draw_stars(const StarRenderer& s, const glm::mat3& wheel,
                const glm::vec3& sun_dir, const glm::vec3& up, float eye_alt,
                float horizon_elev, const StarParams& sp,
                const AtmosphereParams& atm, float fovy_deg, int viewport_h,
                const glm::vec3& moon_dir, float moon_ang_rad,
                const AirField& air);

}  // namespace render
