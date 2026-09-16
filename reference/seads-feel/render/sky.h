#pragma once

#include <glm/glm.hpp>

#include "raylib.h"
#include "render/air_field.h"       // AirField (S-airdome, spec §1.1)
#include "render/air_field_glsl.h"  // kAirFieldUniformsGLSL + kAirFieldGLSL

// Sky pass + shared aerial-perspective GLSL (docs/little_planet_plan.md Stage
// 2). A fullscreen quad drawn FIRST (depth-test off) replaces ClearBackground
// with a per-pixel sky: its vertex NORMALS are the frustum CORNER RAYS
// (render:: frustum_corner_rays, screen-linear, unnormalized), so the FS
// normalizes per-pixel and the horizon tracks the lens-shifted/zoomed scene
// exactly (P0).
//
// MONOCHROME at a frozen epoch (Stage 2): a zenith->horizon day gradient
// blended day<->dusk<->night by SUN ELEVATION over local_up + an exp-shell haze
// + dither. Color scatter (Rayleigh/Mie), the weather variable, and the moving
// sun are Stage 3.
//
// SINGLE-SOURCE: kSkyGLSL is the ONE definition of the sky/haze math, string-
// concatenated into BOTH this sky FS AND the planet FS (aerial perspective), so
// the sky and the ground limb can never disagree at the horizon (plan trap-1).

namespace render {

// The atmosphere tuning (config/world.toml [atmosphere], mapped from cfg by the
// app so render/ does not depend on config/). Angles in radians (converted at
// the config boundary). Fed to both the sky FS and the planet FS as uniforms.
struct AtmosphereParams {
    float sky_space = 0.02f;  // zenith/overhead SPACE (near-black backdrop)
    float sky_band_top_rad =
        0.07f;              // silver rim thickness ABOVE the planet limb
    float sky_day = 0.82f;  // horizon-band day luminance
    float sky_dusk = 0.30f;
    float sky_night = 0.05f;
    float dusk_lo_rad = -0.14f;
    float dusk_hi_rad = 0.10f;
    float night_fill_min = 0.10f;
    float ground_day_gain =
        0.90f;  // planet ground diffuse gain at full sun (3b)
    float dither = 0.004f;
    // S-airdome (spec §1.7, supersedes the Stage-3 weather GATE): the app sets
    // this to the RAW weather_cell(...) scalar [0,1] each frame — no longer
    // pre-scaled by haze_overcast_density, since haze is now ALWAYS on (the
    // AirField spatial factor, sampled via the optical-depth march) and
    // weather is only a local THICKENER on top (uAirHazeDensity + uWeatherAmt
    // * uAirWeatherGain, both inside render/air_field.h::AirField, threaded as
    // uniforms by set_air_field_uniforms). Default 0 = no weather thickening
    // (still-honest baseline air), so a frame that forgot to set it never
    // phantom-thickens.
    float haze_density = 0.0f;
    float haze_scale_m = 2300.0f;
    // Atmospheric scatter (Stage 3): the only sky color, gated by haze_density
    // (so clear air stays dark-starry). scatter_strength defaults 0 so a frame
    // that forgot to map config reads CLEAR (no phantom color), matching the
    // haze_density=0 default. Tints/g default to the config values for a lone
    // shader test.
    float scatter_strength = 0.0f;  // overall scatter gain (x haze amount)
    float mie_g = 0.80f;            // Henyey-Greenstein forward asymmetry
    float rayleigh_tint[3] = {0.35f, 0.55f,
                              1.0f};           // desaturated blue, sun high
    float mie_tint[3] = {1.0f, 0.45f, 0.20f};  // hot orange halo, sun low
    // S-sunglare (Chad 2026-08-09): the Mie forward-halo coefficients, lifted
    // out of kScatterGLSL's baked constants. mie_halo_gain is THE glare dial.
    // Defaults are the post-fly values, not the pre-fly ones the constants
    // held (gain was an implicit 1.0 with a 2.2 dusk boost, which double-
    // counted against the S-sungate fix and read as flare).
    float mie_halo_gain = 0.30f;   // overall sun-forward halo gain
    float mie_dusk_boost = 1.0f;   // extra halo as the sun drops (x(1+this))
    float mie_rim_lift = 0.18f;    // broad low-elevation warm lift
};

// The sun VISUAL params (docs/little_planet_plan.md Stage 3 — sun disc), mapped
// from the [celestial] config by the app. The disc is drawn in the SKY FS only
// (not the shared kSkyGLSL) so it never bleeds into the planet's aerial blend;
// the planet mesh occludes a below-horizon disc for free. distance_m places the
// finite sun so draw.cpp can compute an EYE-RELATIVE sun direction (exact disc
// + specular; ~2 deg origin-vs-eye parallax at 450 km would else flash the
// mirror lakes beside the disc when the water branch lands).
struct SunParams {
    float ang_radius_rad = 0.00305f;  // sun_angular_diameter_deg/2
    float intensity = 1.0f;           // disc brightness gain
    float distance_m = 450000.0f;     // finite sun distance (>= 30*R)
};

// The moon VISUAL params (docs/little_planet_plan.md Stage 5), mapped from
// [celestial] (angular size) + [moon] (the render knobs) by the app. The disc
// is drawn sky-FS-local like the sun (occluded by the planet mesh for free);
// its silver face carries an HONEST phase (sphere-normal lit by the sun). The
// ground_gain/fill_lo drive the planet FS moonlight (a 2nd directional light,
// phase-scaled so full moon lights the night side and new moon stays dark —
// the "dive into the dark to break contact" tactic); sparkle_sharpness the
// moonlit-lake glint. The moon is treated at INFINITY (a pure direction, unlike
// the finite sun) so no eye-relative correction is needed.
struct MoonParams {
    float ang_radius_rad = 0.0131f;  // moon_angular_diameter_deg/2 (1.5 deg)
    float disc_intensity = 0.75f;    // silver disc brightness (additive)
    float ground_gain = 0.55f;       // moonlight ground fill at FULL moon
    float fill_lo = 0.45f;           // phase-shaping knee (smoothstep low edge)
    float sparkle_sharpness = 90.0f;  // moonlit-lake glint specular exponent
};

// Aurora borealis (docs/little_planet_plan.md Stage 8, pulled forward). A
// green/ teal curtain on the auroral OVAL about the INERTIAL spin axis â (the
// fixed geographic pole — the oval does NOT wheel with the stars). Night-only,
// cloud- suppressed. A deliberate bounded 2nd palette relaxation (like the
// sunset scatter). spin_axis / ref_a / ref_b are the inertial â + its
// equatorial perpendicular basis (from the celestial core, set once); shell_r_m
// = R + height. intensity 0 => off (the default, so an unset caller is clean).
struct AuroraParams {
    float intensity = 0.0f;         // overall gain; 0 = disabled
    float oval_center_rad = 0.35f;  // oval colatitude from â [rad]
    float oval_width_rad = 0.12f;   // oval band half-width [rad]
    float curtain_scale = 8.0f;     // ribbons per radian of azimuth
    float shell_r_m = 465000.0f;    // R + aurora_height_m (eye always inside)
    float ground_glow = 0.15f;      // faint green night-side ground glow
    float haze_suppress = 1.0f;     // how strongly cloud hides the aurora [0,1]
    float tint_low[3] = {0.15f, 1.00f, 0.45f};   // green base
    float tint_high[3] = {0.10f, 0.70f, 0.90f};  // teal tips
    glm::vec3 spin_axis{0.0f, 1.0f, 0.0f};       // â (inertial), set from cel
    glm::vec3 ref_a{1.0f, 0.0f, 0.0f};           // equator_x (⟂ â), azimuth ref
    glm::vec3 ref_b{0.0f, 0.0f, 1.0f};           // equator_y (⟂ â), azimuth ref
};

// The shared GLSL: uniform declarations + the sky/haze functions. Concatenated
// after "#version 330" and any shader-specific ins/outs. Both including shaders
// declare the same uniform block via kSkyUniformsGLSL, so the names match.
extern const char* const kSkyUniformsGLSL;  // the [atmosphere] uniforms
extern const char* const kSkyGLSL;          // sky_luminance + sky_aerial

// S-airdome (spec §1.1): the AirField uniform locations, resolved once at
// load and shared by every shader that concatenates kAirFieldUniformsGLSL
// (sky.cpp, planet.cpp, stars.cpp). A SEPARATE struct/lookup/setter from
// AtmosphereParams's — set_atmosphere_uniforms keeps its existing 16-arg
// signature unchanged; set_air_field_uniforms is a new call made BESIDE it.
struct AirFieldLocs {
    int enabled = -1;
    int deck_agl = -1, deck_soft = -1, planet_r = -1;
    int bubble_count = -1;
    int bubble_center_dir = -1, bubble_major_axis = -1;
    int bubble_a = -1, bubble_b = -1;
    int bubble_ceiling = -1, bubble_edge_soft = -1, bubble_ceil_soft = -1;
    int dome_exp = -1;  // uAirDomeExp (S-domeround)
    int tau_scale = -1, march_max = -1, steps_sky = -1, steps_ground = -1;
    int haze_density = -1, weather_gain = -1, aerial_gain = -1;
    int haze_ceiling = -1;  // uAirHazeCeiling (quality fix pass item 7)
    int mie_tint_day = -1;
};

// Look up every uniform kAirFieldUniformsGLSL declares on `sh` (a shader that
// concatenated it). Missing/optimized-out uniforms resolve to -1, which
// raylib's SetShaderValue*/SetShaderValueV silently no-op on.
AirFieldLocs air_field_locs(const Shader& sh);

// Sets every AirField uniform on `sh` from `field` — geometry (deck + up to
// kMaxAirBubbles bubbles) AND the render-tuning dials bundled into AirField
// (haze/weather/aerial/tau/march/mie-day). Call beside set_atmosphere_uniforms
// (spec §1.1) — never folded into it.
void set_air_field_uniforms(const Shader& sh, const AirFieldLocs& locs,
                            const AirField& field);

struct SkyRenderer {
    bool ok = false;
    Shader shader = {};
    Mesh quad =
        {};  // 4-vert NDC quad; normals updated per frame to the corners
    Material mat = {};
    int loc_sun_dir = -1, loc_up = -1, loc_eye_alt = -1, loc_horizon_elev = -1;
    int loc_sun_ang = -1, loc_sun_intensity = -1;
    int loc_moon_dir = -1, loc_moon_ang = -1, loc_moon_intensity = -1;
    Texture2D moon_tex =
        {};  // real NASA near-side albedo (material diffuse map)
    int loc_spin_axis = -1, loc_aur_ref_a = -1, loc_aur_ref_b = -1;
    int loc_aur_phase = -1, loc_aur_intensity = -1, loc_aur_oval_c = -1;
    int loc_aur_oval_w = -1, loc_aur_curtain = -1, loc_aur_shell_r = -1;
    int loc_aur_haze_sup = -1, loc_aur_tint_lo = -1, loc_aur_tint_hi = -1;
    int loc_eye_radius = -1;
    int loc_sky_space = -1, loc_sky_band_top = -1;
    int loc_sky_day = -1, loc_sky_dusk = -1, loc_sky_night = -1;
    int loc_dusk_lo = -1, loc_dusk_hi = -1, loc_night_fill = -1;
    int loc_dither = -1, loc_weather_amt = -1, loc_haze_scale = -1;
    int loc_rayleigh = -1, loc_mie = -1, loc_scatter_strength = -1,
        loc_mie_g = -1;
    AirFieldLocs air_locs;  // S-airdome (spec §1.1)
};

SkyRenderer load_sky();
void unload_sky(SkyRenderer& s);

// Draw the fullscreen sky. `corners` are the 4 frustum corner rays in
// screen-NDC order [BL,BR,TR,TL] (render::frustum_corner_rays), `up` =
// normalize(eye), `eye_alt` = altitude [m], `sun_dir` = world light-travel dir
// (sun->scene). Depth test/write and backface culling are disabled around the
// draw (the sky is the backdrop; the planet mesh drawn AFTER occludes
// below-horizon sky).
// air = the S-airdome AirField (spec §1.9), copied per frame from the live
// sim::AtmosphereField (never re-derived) so the sky can never disagree with
// the flyable dome.
void draw_sky(const SkyRenderer& s, const glm::dvec3 corners[4],
              const glm::vec3& sun_dir, const glm::vec3& up, float eye_alt,
              float horizon_elev, const AtmosphereParams& atm,
              const SunParams& sun, const glm::vec3& moon_dir,
              const MoonParams& moon, const AuroraParams& aurora,
              const glm::vec2& aurora_phase_sc, float eye_radius,
              const AirField& air);

// Set the [atmosphere] uniforms on ANY shader that concatenated
// kSkyUniformsGLSL (the sky FS and the planet FS) — one place, so the two never
// disagree.
void set_atmosphere_uniforms(const Shader& sh, const AtmosphereParams& atm,
                             int loc_sky_space, int loc_sky_band_top,
                             int loc_sky_day, int loc_sky_dusk,
                             int loc_sky_night, int loc_dusk_lo,
                             int loc_dusk_hi, int loc_night_fill,
                             int loc_dither, int loc_weather_amt,
                             int loc_haze_scale, int loc_rayleigh, int loc_mie,
                             int loc_scatter_strength, int loc_mie_g);

}  // namespace render
