#include "render/sky.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "raymath.h"
#include "render/aurora_glsl.h"
#include "render/scatter_glsl.h"
#include "rlgl.h"

namespace render {

// The [atmosphere] uniform block — declared identically in the sky FS and the
// planet FS so the shared functions below resolve the same names in both.
const char* const kSkyUniformsGLSL = R"(
uniform float uSkySpace;
uniform float uSkyBandTop;
uniform float uSkyDay;
uniform float uSkyDusk;
uniform float uSkyNight;
uniform float uDuskLo;
uniform float uDuskHi;
uniform float uNightFill;
uniform float uDither;
// RENAMED from uHazeDensity (quality fix pass item 7): this carries the RAW
// per-frame weather_cell() scalar [0,1], not a haze DENSITY — the old name
// invited tuning it as if it were the always-on baseline (that is
// uAirHazeDensity, in the AirField block). See render/sky.h AtmosphereParams
// haze_density for the C++-side field this still maps from.
uniform float uWeatherAmt;
uniform float uHazeScale;
uniform vec3 u_rayleigh;         // scatter: desaturated blue tint (sun high)
uniform vec3 u_mie;              // scatter: hot orange halo tint (sun low)
uniform float u_scatterStrength; // scatter: overall gain (x haze amount)
uniform float u_mieG;            // scatter: Henyey-Greenstein forward asymmetry
// S-sunglare (Chad 2026-08-09, "the suns glare is too intense"): the Mie
// forward-halo coefficients, previously baked constants in kScatterGLSL. THE
// glare knob is uMieHaloGain; uMieDuskBoost is how much the halo strengthens
// as the sun drops; uMieRimLift is the broad low-elevation warm lift, which is
// deliberately separate so trimming the glare does not also flatten the band.
uniform float uMieHaloGain;
uniform float uMieDuskBoost;
uniform float uMieRimLift;
)";

// The ONE definition of the sky/haze math (plan single-source). sunDir/up/alt
// are PARAMETERS (name-agnostic), so the sky FS and the planet FS pass their
// own; only the [atmosphere]/[air field] uniforms are referenced by name.
// Requires kSkyUniformsGLSL + kAirFieldUniformsGLSL + kAirFieldGLSL
// concatenated before it (sky_luminance/sky_color reference air_at/
// air_optical_depth's uniforms only indirectly, via the caller-supplied
// `airAmt` parameter — spec §1.3's "thread it, do not read a global").
//
// S-airdome (spec §1): the space-first band and the haze lift are now GATED
// BY `airAmt` (the caller's optical-depth march result, spec §1.2) instead of
// the old exp-shell (uWeatherAmt * exp(-alt/uHazeScale)) — the exp-shell was
// a global stand-in for exactly the vertical/lateral structure the AirField
// (deck + bubble domes) now provides honestly. uHazeScale is kept declared
// (existing key) but the sky/ground/star/moon/aurora paths below no longer
// read it — see the deliverable report for this superseded-but-kept key.
const char* const kSkyGLSL = R"(
float sky_hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}
// dir: unit view ray. sunDir: light-travel dir (sun->scene). up: normalize(eye).
// alt: eye altitude [m]. airAmt: the caller's air_optical_depth-derived amount
// in [0,1) for THIS ray (spec §1.2) — 0 in vacuum, saturating inside a dome.
// Returns a MONOCHROME luminance in [0,1].
float sky_luminance(vec3 dir, vec3 sunDir, vec3 up, float alt, float airAmt) {
    float eSun  = asin(clamp(dot(-sunDir, up), -1.0, 1.0));  // sun elevation
    float eView = asin(clamp(dot(dir, up),     -1.0, 1.0));  // view elevation
    // The horizon BAND's peak luminance by sun elevation: two segments meeting at
    // the horizon (eSun=0 => dusk) so day<->dusk<->night all read.
    float band = (eSun >= 0.0)
        ? mix(uSkyDusk, uSkyDay,   smoothstep(0.0, uDuskHi, eSun))
        : mix(uSkyNight, uSkyDusk, smoothstep(uDuskLo, 0.0, eSun));
    // SPACE-FIRST outside any dome, RELAXED inside one (Chad's 2026-08-09
    // ruling, spec §0/§1.3): bandW = 1 at the limb (legacy behaviour outside a
    // dome, where airAmt -> 0 anyway per the deck/vacuum geometry) x airAmt —
    // zero air => the silver rim/band vanishes entirely => uSkySpace + stars,
    // structurally, not a tuned coincidence.
    float bandW = (1.0 - smoothstep(0.0, uSkyBandTop, max(eView - uHorizonElev, 0.0))) * airAmt;
    float lum = mix(uSkySpace, band, bandW);
    // S-airdome haze lift (spec §1.3/§1.7): ALWAYS-ON baseline air
    // (uAirHazeDensity) plus a local weather thickening (uWeatherAmt — the
    // raw weather_cell scalar, not pre-scaled — x uAirWeatherGain, CEILED at
    // uAirHazeCeiling so haze_overcast_density stays a real ceiling rather
    // than an orphaned key — quality fix pass item 7), the whole density
    // scaled by airAmt (so a squall over the vacuum gap is invisible). Lifts
    // toward `band` (the CURRENT sun-elevation horizon luminance), NOT the
    // hardcoded day value — haze thickens whatever light is actually around;
    // chasing uSkyDay unconditionally would wash a dome's NIGHT sky toward
    // daytime grey the instant the always-on baseline is nonzero (caught on
    // the first night screenshot — see the deliverable report). The
    // always-on baseline itself was cut ~3.4x in the quality fix pass
    // (0.85 -> 0.25, config/world.toml) — at the old value this grey lift
    // dominated the sky value and buried the blue Rayleigh scatter below.
    float weatherTerm = min(uWeatherAmt * uAirWeatherGain, uAirHazeCeiling);
    float density = uAirHazeDensity + weatherTerm;
    float haze = airAmt * density;
    lum += haze * (band - lum);
    lum = max(lum, uNightFill * bandW);  // faint horizon airglow floor (night);
                                        // bandW already carries airAmt, so this
                                        // is exactly 0 in vacuum (spec §1.3).
    lum += (sky_hash(gl_FragCoord.xy) - 0.5) * uDither;  // anti-banding
    return clamp(lum, 0.0, 1.0);
}
// (The scatter() function is defined in the PURE render core —
// render/scatter_glsl.cpp kScatterGLSL — and concatenated in BEFORE this block
// so it is declared before sky_color() calls it. It lives there so the headless
// asset validator can gate the Gemini-generated body without a GL context.)
//
// The full sky COLOR: the monochrome luminance + the air-gated scatter.
// SINGLE-SOURCED so the sky pass and the planet aerial add the IDENTICAL color
// at the limb (no fork, plan trap-1). `tau` (the raw optical depth, NOT the
// saturated airAmt) is threaded through to scatter() so its per-channel
// Rayleigh has the dynamic range to redden/whiten out on long chords instead
// of flattening (quality fix pass item 4); sky_luminance still wants the
// saturated [0,1) amount, computed from tau here. NOT clamped here — the sky
// FS adds the sun disc on top, then clamps.
vec3 sky_color(vec3 dir, vec3 sunDir, vec3 up, float alt, float tau) {
    float airAmt = 1.0 - exp(-tau);
    float lum = sky_luminance(dir, sunDir, up, alt, airAmt);
    float sunElevCos = dot(-sunDir, up);
    return vec3(lum) + scatter(dir, sunDir, up, sunElevCos, tau);
}
// Aerial perspective (spec §1.5): blend a lit ground color toward the sky
// COLOR along the view ray, by an optical-depth march over the eye->fragment
// SEGMENT (uAirStepsGround steps, maxDist = dist) — honest blue aerial that
// deepens with distance INSIDE a dome, and stays crisp/airless outside one
// (the deck-only "little planet in vacuum" read). Reuses the ONE segment
// march for both the ground->sky blend fraction (aerial_gain-scaled, spec
// §1.5) and sky_color's airAmt argument (the plain 1-exp(-tau) form, spec
// §1.2) rather than a second full-length sky march — a deliberate perf/
// complexity trade (see the deliverable report).
vec3 sky_aerial(vec3 lit, vec3 dir, float dist, vec3 sunDir, vec3 up, float alt) {
    vec3 eyePos = up * (uAirPlanetR + alt);
    float tauSeg = air_optical_depth(eyePos, dir, dist, uAirStepsGround);
    float aerial = 1.0 - exp(-tauSeg * uAirAerialGain);
    vec3 skyCol = sky_color(dir, sunDir, up, alt, tauSeg);
    return mix(lit, skyCol, clamp(aerial, 0.0, 1.0));
}
)";

namespace {

const char* kSkyVS = R"(#version 330
in vec3 vertexPosition;  // NDC corner (x,y in [-1,1])
in vec3 vertexNormal;    // the frustum corner ray (screen-linear, unnormalized)
out vec3 vDir;
void main() {
    vDir = vertexNormal;
    gl_Position = vec4(vertexPosition.xy, 1.0, 1.0);  // fill the screen, far z
}
)";

std::string sky_fs() {
    return std::string(
               "#version 330\n"
               "in vec3 vDir;\n"
               "uniform vec3 uSunDir;\n"
               "uniform vec3 uUp;\n"
               "uniform float uEyeAlt;\n"
               "uniform float uHorizonElev;\n"
               "uniform float uSunAngRad;\n"
               "uniform float uSunIntensity;\n"
               "uniform vec3 uMoonDir;\n"  // light-travel (moon->scene)
               "uniform float uMoonAngRad;\n"
               "uniform float uMoonIntensity;\n"
               "uniform sampler2D texture0;\n"  // real NASA near-side albedo
                                                // (material diffuse)

               // Aurora (Stage 8): the inertial spin axis + its perpendicular
               // azimuth basis, the periodic phase (sin,cos), the oval +
               // curtain knobs, the shell radius + |eye| for the ray-shell hit,
               // and the green/teal tints. Intensity 0 = disabled.
               "uniform vec3 uSpinAxis;\n"
               "uniform vec3 uAurRefA;\n"
               "uniform vec3 uAurRefB;\n"
               "uniform vec2 uAurPhaseSC;\n"
               "uniform float uAurIntensity;\n"
               "uniform float uAurOvalC;\n"
               "uniform float uAurOvalW;\n"
               "uniform float uAurCurtain;\n"
               "uniform float uAurShellR;\n"
               "uniform float uAurHazeSup;\n"
               "uniform vec3 uAurTintLo;\n"
               "uniform vec3 uAurTintHi;\n"
               "uniform float uEyeRadius;\n") +
           kSkyUniformsGLSL + kAirFieldUniformsGLSL + kScatterGLSL +
           kAirFieldGLSL + kSkyGLSL + kAuroraGLSL +
           // Sun disc — SKY-FS LOCAL (deliberately NOT in the shared kSkyGLSL):
           // the disc must never enter the planet FS aerial-perspective blend,
           // and a below-horizon disc is occluded for free by the planet mesh
           // (drawn after, depth on). uSunDir is EYE-RELATIVE light-travel
           // (draw.cpp), so -uSunDir is the eye->sun direction. angle(dir,sun)
           // -> a fwidth-smoothed 1px core (zoom-correct), additive white on
           // the monochrome sky; the final clamp keeps it in range. NO halo:
           // the sun glare/halo is a Stage-3 haze-gated Mie forward-scatter
           // (Chad ruled the always-on disc halo off, 3a), so a clear-air disc
           // is a clean edge.
           "float sun_disc(vec3 dir) {\n"
           "    float ang = acos(clamp(dot(dir, -uSunDir), -1.0, 1.0));\n"
           "    float fw = max(fwidth(ang), 1e-4);\n"
           "    float core = 1.0 - smoothstep(uSunAngRad - fw, uSunAngRad + "
           "fw, ang);\n"
           "    return core * uSunIntensity;\n"
           "}\n"
           // Moon disc — sky-FS-local like the sun (never in the shared
           // kSkyGLSL; the planet mesh occludes a below-horizon moon for free).
           // Silver, monochrome. HONEST PHASE by SPHERE-NORMAL lighting (Stage
           // 5): treat the disc as a sphere — reconstruct the
           // VISIBLE-hemisphere normal (the -sqrt term; +sqrt is the FAR side
           // and globally inverts the phase, Fable P0-1) and light it by the
           // direction to the sun. Yields a true crescent<->gibbous terminator,
           // not a fixed half.
           "float moon_disc(vec3 dir, float airAmt) {\n"
           "    vec3 toMoon = -uMoonDir;\n"  // eye -> moon
           "    float ang = acos(clamp(dot(dir, toMoon), -1.0, 1.0));\n"
           "    float fw = max(fwidth(ang), 1e-4);\n"
           "    float core = 1.0 - smoothstep(uMoonAngRad - fw, uMoonAngRad + "
           "fw, ang);\n"
           "    if (core <= 0.0) return 0.0;\n"
           // Disc-plane basis with dU toward CELESTIAL NORTH (uSpinAxis
           // projected) so the maria "face" stays roughly upright (north up) as
           // the moon crosses the sky, instead of spinning with an arbitrary
           // basis. Guard the degeneracy when the moon is near the pole axis.
           "    vec3 up0 = abs(dot(uSpinAxis, toMoon)) < 0.99 ? uSpinAxis : "
           "vec3(1.0,0.0,0.0);\n"
           "    vec3 dR = normalize(cross(up0, toMoon));\n"
           "    vec3 dU = cross(toMoon, dR);\n"
           // Disc-local coords in [-1,1] = tangential offset / sin(angRad).
           "    float s = max(sin(uMoonAngRad), 1e-4);\n"
           "    float x = dot(dir, dR) / s;\n"
           "    float y = dot(dir, dU) / s;\n"
           "    float r2 = clamp(x*x + y*y, 0.0, 1.0);\n"
           "    vec3 n = x*dR + y*dU - sqrt(1.0 - r2)*toMoon;\n"
           "    vec3 toSun = -uSunDir;\n"
           "    float lit = smoothstep(-0.05, 0.05, dot(n, toSun));\n"
           // The FACE: the REAL near-side albedo (NASA/JPL/USGS full-moon,
           // public domain; assets/moon_nearside.png) — the actual maria + ray
           // craters, not a blank disc. dU is celestial-north so the disc uv
           // maps north-up: uv=(x*0.5+0.5, 0.5-y*0.5). Multiplies the lit
           // factor.
           "    lit *= texture(texture0, vec2(x*0.5+0.5, 0.5-y*0.5)).r;\n"
           // Haze-gate the disc by the SAME airAmt the sky/stars use
           // (S-airdome, spec §1.6, supersedes the Fable red-team P1-1
           // exp-shell fix): a dome's own overcast must still dim the moon, and
           // the vacuum gap must never dim it — a single AirField-derived
           // amount can't fork the two the way a raw uWeatherAmt read did.
           "    return core * uMoonIntensity * lit * (1.0 - clamp(airAmt, 0.0, "
           "1.0));\n"
           "}\n"
           // Aurora wrapper (hand-authored; the CURTAIN color is the Gemini
           // kAuroraGLSL). Ray-shell geometry (mirroring render::
           // aurora_shell_colatitude — the eye is ALWAYS inside the shell, one
           // positive root) -> the hit point's colatitude + azimuth about the
           // INERTIAL â, then the curtain, gated by night (sun below horizon) +
           // clear air (cloud hides it). Sky-FS-local like the discs: the
           // planet mesh occludes a below-horizon hit for free.
           "vec3 aurora(vec3 dir, float airAmt) {\n"
           "    if (uAurIntensity <= 0.0) return vec3(0.0);\n"
           "    float sunElev = dot(-uSunDir, uUp);\n"
           "    float night = 1.0 - smoothstep(-0.10, 0.05, sunElev);\n"
           "    if (night <= 0.0) return vec3(0.0);\n"
           // S-airdome (spec §1.6): the clear-air factor is airAmt-gated (a
           // dome's own weather still hides the aurora; the vacuum gap never
           // does), superseding the exp-shell haze read.
           "    float clear = 1.0 - clamp(airAmt, 0.0, 1.0) * uAurHazeSup;\n"
           "    vec3 eye = uUp * uEyeRadius;\n"
           "    float b = dot(eye, dir);\n"
           "    float c = uEyeRadius*uEyeRadius - uAurShellR*uAurShellR;\n"
           "    float t = -b + sqrt(max(b*b - c, 0.0));\n"
           "    vec3 H = normalize(eye + t*dir);\n"
           "    float pc = dot(H, uSpinAxis);\n"
           // Guard the pole where az = atan(0,0) is undefined (within ~1.8
           // deg); mask it regardless of oval width (Fable P2).
           "    if (abs(pc) > 0.9995) return vec3(0.0);\n"
           "    float rho = acos(clamp(pc, -1.0, 1.0));\n"
           // Coarse band early-out (the snaking two-band system spans roughly
           // [center - 4w, center + 12w] with the fold + the long top tails);
           // the curtain owns the exact band math.
           "    float dc = (rho - uAurOvalC) / max(uAurOvalW, 1e-4);\n"
           "    if (dc < -4.0 || dc > 12.0) return vec3(0.0);\n"
           "    float az = atan(dot(H, uAurRefB), dot(H, uAurRefA));\n"
           // LAYER limb-brightening (Chad 2026-07-09): a thin emitting shell is
           // BRIGHT where the view ray grazes it (long optical path) and dim
           // where it crosses steeply — the aurora hugs the limb as a layer,
           // not a bloom.
           "    float cosCross = abs(dot(dir, H));\n"
           "    float limb = clamp(0.35 / max(cosCross, 0.12), 0.6, 3.0);\n"
           "    vec3 col = aurora_curtain(rho, az, uAurPhaseSC, uAurOvalC, "
           "uAurOvalW, uAurCurtain, uAurTintLo, uAurTintHi);\n"
           "    return col * limb * uAurIntensity * night * clear;\n"
           "}\n"
           "out vec4 finalColor;\n"
           "void main() {\n"
           "    vec3 dir = normalize(vDir);\n"
           // S-airdome (spec §1.2): the optical-depth march IS what makes the
           // dome visible from outside — maxDist is the eye->planet-sphere hit
           // along dir if the ray hits (below-horizon; the planet mesh
           // occludes it anyway), else the march cap.
           "    vec3 eyePos = air_eye_pos();\n"
           "    float maxDist = air_sky_max_dist(eyePos, dir);\n"
           "    float tau = air_optical_depth(eyePos, dir, maxDist, "
           "uAirStepsSky);\n"
           "    float airAmt = 1.0 - exp(-tau);\n"
           "    vec3 col = sky_color(dir, uSunDir, uUp, uEyeAlt, tau);\n"
           "    col += aurora(dir, airAmt);\n"
           "    col += vec3(sun_disc(dir));\n"
           "    col += vec3(moon_disc(dir, airAmt));\n"
           "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
           "}\n";
}

Mesh make_ndc_quad() {
    Mesh m = {};
    m.vertexCount = 4;
    m.triangleCount = 2;
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 12));
    m.normals = static_cast<float*>(MemAlloc(sizeof(float) * 12));
    m.indices =
        static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * 6));
    const float pos[12] = {-1, -1, 0, 1,  -1, 0,
                           1,  1,  0, -1, 1,  0};  // BL BR TR TL
    std::memcpy(m.vertices, pos, sizeof pos);
    std::memset(m.normals, 0, sizeof(float) * 12);  // filled per frame
    const unsigned short idx[6] = {0, 1, 2, 0, 2, 3};
    std::memcpy(m.indices, idx, sizeof idx);
    UploadMesh(&m, /*dynamic=*/true);  // normals updated each frame
    return m;
}

}  // namespace

SkyRenderer load_sky() {
    SkyRenderer s;
    const std::string fs = sky_fs();
    s.shader = LoadShaderFromMemory(kSkyVS, fs.c_str());
    if (s.shader.id == 0) {
        TraceLog(LOG_WARNING, "SKY: shader compile failed");
        return s;
    }
    s.loc_sun_dir = GetShaderLocation(s.shader, "uSunDir");
    s.loc_up = GetShaderLocation(s.shader, "uUp");
    s.loc_eye_alt = GetShaderLocation(s.shader, "uEyeAlt");
    s.loc_horizon_elev = GetShaderLocation(s.shader, "uHorizonElev");
    s.loc_sun_ang = GetShaderLocation(s.shader, "uSunAngRad");
    s.loc_sun_intensity = GetShaderLocation(s.shader, "uSunIntensity");
    s.loc_moon_dir = GetShaderLocation(s.shader, "uMoonDir");
    s.loc_moon_ang = GetShaderLocation(s.shader, "uMoonAngRad");
    s.loc_moon_intensity = GetShaderLocation(s.shader, "uMoonIntensity");
    // The real near-side lunar albedo (NASA/JPL/USGS, public domain). Bilinear
    // so the small disc stays smooth; clamp so the disc edge never wraps. Bound
    // as the material's DIFFUSE map => raylib auto-binds it to the FS sampler
    // "texture0" on DrawMesh (the codebase pattern; SetShaderValueTexture
    // doesn't take with DrawMesh).
    s.moon_tex = LoadTexture(SEADS_ASSET_DIR "/moon_nearside.png");
    if (s.moon_tex.id != 0) {
        SetTextureFilter(s.moon_tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(s.moon_tex, TEXTURE_WRAP_CLAMP);
    } else {
        TraceLog(LOG_WARNING, "SKY: moon_nearside.png load failed");
    }
    s.loc_spin_axis = GetShaderLocation(s.shader, "uSpinAxis");
    s.loc_aur_ref_a = GetShaderLocation(s.shader, "uAurRefA");
    s.loc_aur_ref_b = GetShaderLocation(s.shader, "uAurRefB");
    s.loc_aur_phase = GetShaderLocation(s.shader, "uAurPhaseSC");
    s.loc_aur_intensity = GetShaderLocation(s.shader, "uAurIntensity");
    s.loc_aur_oval_c = GetShaderLocation(s.shader, "uAurOvalC");
    s.loc_aur_oval_w = GetShaderLocation(s.shader, "uAurOvalW");
    s.loc_aur_curtain = GetShaderLocation(s.shader, "uAurCurtain");
    s.loc_aur_shell_r = GetShaderLocation(s.shader, "uAurShellR");
    s.loc_aur_haze_sup = GetShaderLocation(s.shader, "uAurHazeSup");
    s.loc_aur_tint_lo = GetShaderLocation(s.shader, "uAurTintLo");
    s.loc_aur_tint_hi = GetShaderLocation(s.shader, "uAurTintHi");
    s.loc_eye_radius = GetShaderLocation(s.shader, "uEyeRadius");
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
    s.quad = make_ndc_quad();
    s.mat = LoadMaterialDefault();
    s.mat.shader = s.shader;
    // The moon albedo rides the material DIFFUSE map -> DrawMesh binds it to
    // the FS sampler "texture0" (the moon disc samples it). If the load failed
    // the default 1x1 white texture stays (moon reads as a blank bright disc).
    if (s.moon_tex.id != 0)
        s.mat.maps[MATERIAL_MAP_DIFFUSE].texture = s.moon_tex;
    s.ok = true;
    TraceLog(LOG_INFO, "SKY: fullscreen sky pass built");
    return s;
}

void unload_sky(SkyRenderer& s) {
    if (!s.ok) return;
    if (s.moon_tex.id != 0) UnloadTexture(s.moon_tex);
    UnloadMesh(s.quad);
    UnloadShader(s.shader);
    s.ok = false;
}

// S-airdome (spec §1.1). Array uniforms are looked up via "name[0]" (the
// portable GLSL convention raylib's GetShaderLocation expects for the first
// element of an array) and set with SetShaderValueV over `field.bubble_count`
// (never the full kMaxAirBubbles — an unset tail element would read as a
// bogus bubble at direction (0,0,0) otherwise, but the shader loop already
// gates on uAirBubbleCount so the tail is simply never read).
AirFieldLocs air_field_locs(const Shader& sh) {
    AirFieldLocs l;
    l.enabled = GetShaderLocation(sh, "uAirEnabled");
    l.deck_agl = GetShaderLocation(sh, "uAirDeckAglM");
    l.deck_soft = GetShaderLocation(sh, "uAirDeckSoftM");
    l.planet_r = GetShaderLocation(sh, "uAirPlanetR");
    l.bubble_count = GetShaderLocation(sh, "uAirBubbleCount");
    l.bubble_center_dir = GetShaderLocation(sh, "uAirBubbleCenterDir[0]");
    l.bubble_major_axis = GetShaderLocation(sh, "uAirBubbleMajorAxis[0]");
    l.bubble_a = GetShaderLocation(sh, "uAirBubbleA[0]");
    l.bubble_b = GetShaderLocation(sh, "uAirBubbleB[0]");
    l.bubble_ceiling = GetShaderLocation(sh, "uAirBubbleCeilingM[0]");
    l.bubble_edge_soft = GetShaderLocation(sh, "uAirBubbleEdgeSoftM[0]");
    l.bubble_ceil_soft = GetShaderLocation(sh, "uAirBubbleCeilSoftM[0]");
    l.dome_exp = GetShaderLocation(sh, "uAirDomeExp");
    l.tau_scale = GetShaderLocation(sh, "uAirTauScaleM");
    l.march_max = GetShaderLocation(sh, "uAirMarchMaxM");
    l.steps_sky = GetShaderLocation(sh, "uAirStepsSky");
    l.steps_ground = GetShaderLocation(sh, "uAirStepsGround");
    l.haze_density = GetShaderLocation(sh, "uAirHazeDensity");
    l.weather_gain = GetShaderLocation(sh, "uAirWeatherGain");
    l.aerial_gain = GetShaderLocation(sh, "uAirAerialGain");
    l.haze_ceiling = GetShaderLocation(sh, "uAirHazeCeiling");
    l.mie_tint_day = GetShaderLocation(sh, "uMieTintDay");
    return l;
}

void set_air_field_uniforms(const Shader& sh, const AirFieldLocs& locs,
                            const AirField& field) {
    const float enabled = field.enabled ? 1.0f : 0.0f;
    SetShaderValue(sh, locs.enabled, &enabled, SHADER_UNIFORM_FLOAT);
    const float deck_agl = static_cast<float>(field.deck_agl_m);
    SetShaderValue(sh, locs.deck_agl, &deck_agl, SHADER_UNIFORM_FLOAT);
    const float deck_soft = static_cast<float>(field.deck_soft_m);
    SetShaderValue(sh, locs.deck_soft, &deck_soft, SHADER_UNIFORM_FLOAT);
    const float planet_r = static_cast<float>(field.planet_R);
    SetShaderValue(sh, locs.planet_r, &planet_r, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, locs.bubble_count, &field.bubble_count,
                   SHADER_UNIFORM_INT);
    const int n = std::clamp(field.bubble_count, 0, kMaxAirBubbles);
    if (n > 0) {
        float center_dir[kMaxAirBubbles * 3];
        float major_axis[kMaxAirBubbles * 3];
        float a[kMaxAirBubbles], b[kMaxAirBubbles];
        float ceiling[kMaxAirBubbles], edge_soft[kMaxAirBubbles],
            ceil_soft[kMaxAirBubbles];
        for (int i = 0; i < n; ++i) {
            const AirField::Bubble& bub = field.bubbles[i];
            center_dir[i * 3 + 0] = static_cast<float>(bub.center_dir.x);
            center_dir[i * 3 + 1] = static_cast<float>(bub.center_dir.y);
            center_dir[i * 3 + 2] = static_cast<float>(bub.center_dir.z);
            major_axis[i * 3 + 0] = static_cast<float>(bub.major_axis.x);
            major_axis[i * 3 + 1] = static_cast<float>(bub.major_axis.y);
            major_axis[i * 3 + 2] = static_cast<float>(bub.major_axis.z);
            a[i] = static_cast<float>(bub.a);
            b[i] = static_cast<float>(bub.b);
            ceiling[i] = static_cast<float>(bub.ceiling_m);
            edge_soft[i] = static_cast<float>(bub.edge_soft_m);
            ceil_soft[i] = static_cast<float>(bub.ceil_soft_m);
        }
        SetShaderValueV(sh, locs.bubble_center_dir, center_dir,
                        SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(sh, locs.bubble_major_axis, major_axis,
                        SHADER_UNIFORM_VEC3, n);
        SetShaderValueV(sh, locs.bubble_a, a, SHADER_UNIFORM_FLOAT, n);
        SetShaderValueV(sh, locs.bubble_b, b, SHADER_UNIFORM_FLOAT, n);
        SetShaderValueV(sh, locs.bubble_ceiling, ceiling, SHADER_UNIFORM_FLOAT,
                        n);
        SetShaderValueV(sh, locs.bubble_edge_soft, edge_soft,
                        SHADER_UNIFORM_FLOAT, n);
        SetShaderValueV(sh, locs.bubble_ceil_soft, ceil_soft,
                        SHADER_UNIFORM_FLOAT, n);
    }
    const float dome_exp = static_cast<float>(field.dome_exponent);
    SetShaderValue(sh, locs.dome_exp, &dome_exp, SHADER_UNIFORM_FLOAT);
    const float tau_scale = static_cast<float>(field.tau_scale_m);
    SetShaderValue(sh, locs.tau_scale, &tau_scale, SHADER_UNIFORM_FLOAT);
    const float march_max = static_cast<float>(field.march_max_m);
    SetShaderValue(sh, locs.march_max, &march_max, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, locs.steps_sky, &field.march_steps_sky,
                   SHADER_UNIFORM_INT);
    SetShaderValue(sh, locs.steps_ground, &field.march_steps_ground,
                   SHADER_UNIFORM_INT);
    const float haze_density = static_cast<float>(field.haze_density);
    SetShaderValue(sh, locs.haze_density, &haze_density, SHADER_UNIFORM_FLOAT);
    const float weather_gain = static_cast<float>(field.weather_gain);
    SetShaderValue(sh, locs.weather_gain, &weather_gain, SHADER_UNIFORM_FLOAT);
    const float aerial_gain = static_cast<float>(field.aerial_gain);
    SetShaderValue(sh, locs.aerial_gain, &aerial_gain, SHADER_UNIFORM_FLOAT);
    const float haze_ceiling = static_cast<float>(field.haze_overcast_density);
    SetShaderValue(sh, locs.haze_ceiling, &haze_ceiling, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, locs.mie_tint_day, field.mie_tint_day,
                   SHADER_UNIFORM_VEC3);
}

void set_atmosphere_uniforms(const Shader& sh, const AtmosphereParams& atm,
                             int loc_sky_space, int loc_sky_band_top,
                             int loc_sky_day, int loc_sky_dusk,
                             int loc_sky_night, int loc_dusk_lo,
                             int loc_dusk_hi, int loc_night_fill,
                             int loc_dither, int loc_weather_amt,
                             int loc_haze_scale, int loc_rayleigh, int loc_mie,
                             int loc_scatter_strength, int loc_mie_g) {
    SetShaderValue(sh, loc_sky_space, &atm.sky_space, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_sky_band_top, &atm.sky_band_top_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_sky_day, &atm.sky_day, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_sky_dusk, &atm.sky_dusk, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_sky_night, &atm.sky_night, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_dusk_lo, &atm.dusk_lo_rad, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_dusk_hi, &atm.dusk_hi_rad, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_night_fill, &atm.night_fill_min,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_dither, &atm.dither, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_weather_amt, &atm.haze_density,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_haze_scale, &atm.haze_scale_m, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_rayleigh, atm.rayleigh_tint, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, loc_mie, atm.mie_tint, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, loc_scatter_strength, &atm.scatter_strength,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, loc_mie_g, &atm.mie_g, SHADER_UNIFORM_FLOAT);
    // S-sunglare: the three Mie-halo dials are looked up HERE rather than
    // threaded as three more int params — the S-airdome spec explicitly froze
    // this function's 16-arg signature, and every one of its three call sites
    // (sky/planet/stars) needs these, so a local lookup costs 9 driver queries
    // a frame and keeps the seam the spec drew. A missing/optimized-out
    // uniform resolves to -1, which SetShaderValue silently no-ops on.
    SetShaderValue(sh, GetShaderLocation(sh, "uMieHaloGain"), &atm.mie_halo_gain,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, GetShaderLocation(sh, "uMieDuskBoost"),
                   &atm.mie_dusk_boost, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, GetShaderLocation(sh, "uMieRimLift"), &atm.mie_rim_lift,
                   SHADER_UNIFORM_FLOAT);
}

void draw_sky(const SkyRenderer& s, const glm::dvec3 corners[4],
              const glm::vec3& sun_dir, const glm::vec3& up, float eye_alt,
              float horizon_elev, const AtmosphereParams& atm,
              const SunParams& sun, const glm::vec3& moon_dir,
              const MoonParams& moon, const AuroraParams& aurora,
              const glm::vec2& aurora_phase_sc, float eye_radius,
              const AirField& air) {
    if (!s.ok) return;
    // Push the current corner rays into the quad's normal buffer (VBO index 2).
    const float n[12] = {
        static_cast<float>(corners[0].x), static_cast<float>(corners[0].y),
        static_cast<float>(corners[0].z), static_cast<float>(corners[1].x),
        static_cast<float>(corners[1].y), static_cast<float>(corners[1].z),
        static_cast<float>(corners[2].x), static_cast<float>(corners[2].y),
        static_cast<float>(corners[2].z), static_cast<float>(corners[3].x),
        static_cast<float>(corners[3].y), static_cast<float>(corners[3].z)};
    UpdateMeshBuffer(s.quad, 2, n, sizeof n, 0);

    const float sd[3] = {sun_dir.x, sun_dir.y, sun_dir.z};
    const float up3[3] = {up.x, up.y, up.z};
    SetShaderValue(s.shader, s.loc_sun_dir, sd, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_up, up3, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_eye_alt, &eye_alt, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_horizon_elev, &horizon_elev,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_sun_ang, &sun.ang_radius_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_sun_intensity, &sun.intensity,
                   SHADER_UNIFORM_FLOAT);
    const float md[3] = {moon_dir.x, moon_dir.y, moon_dir.z};
    SetShaderValue(s.shader, s.loc_moon_dir, md, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_moon_ang, &moon.ang_radius_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_moon_intensity, &moon.disc_intensity,
                   SHADER_UNIFORM_FLOAT);
    const float sa[3] = {aurora.spin_axis.x, aurora.spin_axis.y,
                         aurora.spin_axis.z};
    const float ra[3] = {aurora.ref_a.x, aurora.ref_a.y, aurora.ref_a.z};
    const float rb[3] = {aurora.ref_b.x, aurora.ref_b.y, aurora.ref_b.z};
    const float psc[2] = {aurora_phase_sc.x, aurora_phase_sc.y};
    SetShaderValue(s.shader, s.loc_spin_axis, sa, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_aur_ref_a, ra, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_aur_ref_b, rb, SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_aur_phase, psc, SHADER_UNIFORM_VEC2);
    SetShaderValue(s.shader, s.loc_aur_intensity, &aurora.intensity,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_oval_c, &aurora.oval_center_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_oval_w, &aurora.oval_width_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_curtain, &aurora.curtain_scale,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_shell_r, &aurora.shell_r_m,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_haze_sup, &aurora.haze_suppress,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s.shader, s.loc_aur_tint_lo, aurora.tint_low,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_aur_tint_hi, aurora.tint_high,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(s.shader, s.loc_eye_radius, &eye_radius,
                   SHADER_UNIFORM_FLOAT);
    set_atmosphere_uniforms(
        s.shader, atm, s.loc_sky_space, s.loc_sky_band_top, s.loc_sky_day,
        s.loc_sky_dusk, s.loc_sky_night, s.loc_dusk_lo, s.loc_dusk_hi,
        s.loc_night_fill, s.loc_dither, s.loc_weather_amt, s.loc_haze_scale,
        s.loc_rayleigh, s.loc_mie, s.loc_scatter_strength, s.loc_mie_g);
    set_air_field_uniforms(s.shader, s.air_locs, air);

    // Backdrop: always drawn, never occludes. Depth test off (draws over the
    // cleared far-depth), depth write off (the planet drawn after occludes it),
    // culling off (the fixed NDC winding needn't be reasoned about).
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    DrawMesh(s.quad, s.mat, MatrixIdentity());
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
}

}  // namespace render
