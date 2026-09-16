#include "render/planet.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/geometric.hpp>
#include <string>

#include "raymath.h"
#include "render/cone_glsl.h"
#include "render/ribbons.h"  // R1: sudbury_linework, for the fold mask
#include "world/snowpack.h"
#include "render/scatter_glsl.h"
#include "render/snow_light_glsl.h"
#include "rlgl.h"
// Raw GL for the ONE thing rlgl doesn't expose: cubemap mip generation
// (rlLoadTextureCubemap only sets a mip filter with a pre-baked chain, and
// GenTextureMipmaps binds GL_TEXTURE_2D). glad's decls only (impl is compiled
// into raylib); the loader ran at InitWindow, before the first-frame build.
#include "external/glad.h"

namespace render {

float sun_sparkle_dial() {
    // ★ R5 row 8 -- SUN SPARKLE. The dial, the SEADS_TRACK_PACK precedent to
    // the letter: read once (getenv on the draw path is not free), validated
    // range, warn-and-default on bad input, TraceLog the value, 0 kills the
    // row (the A/B baseline). A multiplier on SnowParams::sun_sparkle, whose
    // base anchors to the shipped night-sparkle amplitude -- the ladder for
    // Chad's seat sweep is 0 / 0.5 / 1 / 2 / 4.
    //
    // ★ ROAD-REPAIR: promoted out of the draw_planet body to a named
    // function because the SF2 bank strips are lit now and read the same
    // amplitude. One dial, two consumers: SEADS_SPARKLE=0 must kill the glint
    // on the bank as well as on the field, or the A/B baseline is a lie.
    static const float kSparkleDial = [] {
        const char* e = std::getenv("SEADS_SPARKLE");
        if (e == nullptr) return 1.0f;
        const float v = static_cast<float>(std::atof(e));
        if (v < 0.0f || v > 4.0f) {
            TraceLog(LOG_WARNING,
                     "PLANET: SEADS_SPARKLE=\"%s\" outside [0,4] -- "
                     "IGNORED, using 1.0",
                     e);
            return 1.0f;
        }
        TraceLog(LOG_INFO, "PLANET: sun sparkle SEADS_SPARKLE=%.2f",
                 static_cast<double>(v));
        return v;
    }();
    return kSparkleDial;
}

// ★ THE BARREN SHED LAW, single-sourced from world::SnowpackParams (INV-9).
// These are DEFAULTED from the sim's own struct -- not re-typed -- so the
// shipped law is identical even if app/main.cpp never calls the setter (tests
// and the harness never draw). 0.75 was previously hard-coded at the use site
// AND in world/snowpack.h; that duplication is exactly the drift INV-9 forbids,
// and Chad's "reads too light" fly finding is what surfaced it.
namespace {
const world::SnowParams kSnowDefaults{};
double g_shed_k = kSnowDefaults.k_barren;
double g_shed_lo = kSnowDefaults.barren_shed_lo;
double g_shed_hi = kSnowDefaults.barren_shed_hi;
// ★ THE SLOPE GATE (Chad's fly ruling, 2026-08-11), single-sourced the same
// way as the shed curve above: defaulted FROM world::SnowParams so a caller
// that never sets the law (tests/harness, which never draw) still ships
// Chad's ruling rather than a bypass.
double g_shed_slope_x_lo = kSnowDefaults.barren_slope_x_lo();
double g_shed_slope_x_hi = kSnowDefaults.barren_slope_x_hi();
double g_shed_flat_frac = kSnowDefaults.barren_flat_shed_frac;

// ★ R1 THE DRAWN FOLD (BLOCK-VP1). The shipped [snowpack] dials, pushed in
// from app/ before the planet loads -- load_planet cannot read config itself,
// and the fold must use the SAME numbers the sled drives on or the drawn and
// driven surfaces fork, which is the whole defect this rung closes.
world::SnowParams g_fold_params = kSnowDefaults;
bool g_fold_enabled = false;  // OFF until app/ says winter: a procedural or
                              // summer planet has no snow to fold.
// R3: what the planet pass last set uSnowDepthMix to, so the snow-patch draw
// can put it back instead of guessing. File-static beside g_fold_enabled for
// the same reason -- draw_snow_patch takes a const Planet&.
float g_planet_depth_mix = 0.0f;

}  // namespace

void set_barren_shed_law(double k, double lo, double hi, double slope_x_lo,
                         double slope_x_hi, double flat_frac) {
    g_shed_k = k;
    g_shed_lo = lo;
    g_shed_hi = hi;
    g_shed_slope_x_lo = slope_x_lo;
    g_shed_slope_x_hi = slope_x_hi;
    g_shed_flat_frac = flat_frac;
}

void set_planet_snow_fold(const world::SnowParams& sp, bool enabled) {
    g_fold_params = sp;
    // A/B switch, the SEADS_NO_M2 precedent: this changes what Chad sees on
    // every square metre of the planet, so it must be judgeable against its
    // own absence without a rebuild.
    g_fold_enabled = enabled && std::getenv("SEADS_NO_SNOWFOLD") == nullptr;
}

namespace {

// --- GLSL (330) --------------------------------------------------------------
// Per-fragment albedo from a GL CUBEMAP sampled by the interpolated surface
// direction: no runtime (u,v) lat/lon, no vertex UV seam, hardware-filtered,
// pole-free. One directional sun + ambient so displaced terrain shades. The old
// equirect-in-shader path + lat/long graticule are gone (world_build_plan §2).
const char* kVS = R"(#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;   // SF3-A: .x = the snow patch rim weight
                          // R3:    .y = per-vertex AMBIENT SNOW DEPTH, metres
                          //        (PLANET mesh); on the RIDER PATCH mesh .y
                          //        is normalized track COMPACTION instead (R5
                          //        row 3) -- safe because draw_snow_patch
                          //        forces uSnowDepthMix to 0 there and only
                          //        that draw arms uTrackPackMix.
uniform mat4 mvp;
uniform mat4 matNormal;
uniform mat4 matModel;     // raylib-set: the eye-relative transform (translate -eye)
out vec3 fragNormal;
out vec3 fragDir;
out vec3 fragRel;          // EYE-RELATIVE world position (for aerial perspective)
out float vRim;            // SF3-A: 1 in the patch interior, 0 at the rim
out float vSnowDepth;      // R3: metres of ambient snowpack at this vertex
void main() {
    fragDir = normalize(vertexPosition);      // planet-local surface direction
    vRim = vertexTexCoord.x;
    vSnowDepth = vertexTexCoord.y;
    fragNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragRel = (matModel * vec4(vertexPosition, 1.0)).xyz;  // == vertex - eye
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

// The planet FS calls the SHARED kSkyGLSL aerial perspective (Stage 2, P1-5),
// so the ground limb fades into the SAME horizon haze the sky pass draws — no
// hard dark limb, no fork. Built at load: "#version 330" + ins + the planet's
// own uniforms + the shared [atmosphere] uniforms + the shared functions +
// main.
std::string planet_fs() {
    return std::string(
               "#version 330\n"
               "in vec3 fragNormal;\n"
               "in vec3 fragDir;\n"
               "in vec3 fragRel;\n"
               "in float vRim;\n"
               "in float vSnowDepth;\n"
               // SF3-A THE RIDER SNOW PATCH. The M2 normal cubemap is baked from the
               // DEM and knows nothing of the snow depth field, so a patch drawn with
               // it would carry its relief in SILHOUETTE and shade perfectly flat.
               // This mixes the patch's OWN geometric normal in, weighted by vRim so
               // it fades out exactly where the patch fades into the surrounding
               // planet mesh -- no shading seam at the rim. The planet pass sets it
               // to 0, and mix(x, y, 0) == x, so that pass is bit-identical to the
               // pre-SF3-A shader and no golden can move.
               "uniform float uVertexNormalMix;\n"
               "uniform samplerCube cubemap;\n"
               // M2 object-space normal map: crisp per-fragment surface normal
               // (planet-local) baked from the UNBLURRED elevation, so the
               // coarse ~100 m mesh gets sharp slope shading. Replaces the mesh
               // normal in the LAND diffuse; water keeps its own radial normal
               // (below).
               "uniform samplerCube normalCube;\n"
               "uniform float uHasNormalMap;\n"  // 0 for Earth (no M2 map):
                                                 // fall back to the mesh normal
               "uniform vec3 sunDir;\n"
               "uniform vec3 uUp;\n"  // normalize(eye)
               "uniform float uEyeAlt;\n"
               "uniform float uHorizonElev;\n"
               "uniform float uGroundDayGain;\n"
               // Mirror-silver lakes (Inc 2): the landmask rides the cubemap
               // ALPHA; uHasWater gates the branch OFF for the Earth (RGB)
               // cubemap (whose alpha is a meaningless 1.0).
               "uniform float uReflectivity;\n"
               "uniform float uSparkleSharp;\n"
               "uniform float uHasWater;\n"
               // Dedicated lake mirror-mesh: when 1 the fragment is forced to
               // render as water regardless of the cubemap alpha (the mesh must
               // read as water across its FULL OSM outline, incl. the eroded
               // shore band). 0 for the planet pass.
               "uniform float uForceWater;\n"
               // Moonlight (Stage 5): a 2nd directional light. uMoonDir is
               // light-travel (moon->scene), mirroring sunDir. uMoonFill is the
               // phase-shaped gain (0 at new moon -> the dark side stays dark
               // for the break-contact tactic; ~ground_gain at full moon).
               "uniform vec3 uMoonDir;\n"
               "uniform float uMoonFill;\n"
               "uniform float uMoonSparkle;\n"
               // Aurora ground glow (Stage 8): a faint green wash on the night
               // ground under the auroral OVAL (same colatitude ring as the sky
               // curtain, keyed to the inertial spin axis).
               "uniform vec3 uSpinAxis;\n"
               "uniform float uAurOvalC;\n"
               "uniform float uAurOvalW;\n"
               "uniform float uAurGroundGlow;\n"
               "uniform vec3 uAurTintLo;\n"
               // Procedural STAR sparkle reflected in the water (lakes AND
               // rivers, single-sourced here). uStarReflect = brightness gain,
               // uStarDensity = cells per unit reflected-dir. Night-gated +
               // haze-dimmed below.
               "uniform float uStarReflect;\n"
               "uniform float uStarDensity;\n"
               // W4 seasonal ground (winter snow-cover). uSeasonSnow is the
               // per-life amount (0 outside Winter => identity); uSnowAlbedo
               // the mono snow target the land whitens toward; uSnowSlopeLo the
               // flatness (dot(N,localUp)) below which slopes shed snow.
               "uniform float uSeasonSnow;\n"
               "uniform float uSnowAlbedo;\n"
               "uniform float uSnowSlopeLo;\n"
               // ★ R3 THE DEPTH-KEYED EXPOSURE. uSnowDepthMix crossfades the
               // coverage term from the legacy render-only stub (a private
               // slope mask + a private barren shed, both of which PREDATE the
               // depth field and disagree with it) onto vSnowDepth -- the ONE
               // analytic field the sled drives, delivered per-vertex.
               // 0.0 SHIPS and is bit-identical: mix(a,b,0) == a, and the
               // barren block below is faded by the same dial, so nothing on
               // the planet moves until Chad arms it. uSnowFullDepth is the
               // depth at which coverage saturates to full snow.
               "uniform float uSnowDepthMix;\n"
               "uniform float uBarrenShedKeep;\n"
               "uniform float uSnowFullDepth;\n"
               // ★ S1 lake ice. uIceAlbedo is the frozen-lake value the water
               // albedo freezes toward (BELOW uSnowAlbedo so the shoreline
               // stays readable); uIceReflectFrac / uIceGlintFrac damp the
               // summer mirror and its glints rather than deleting them.
               "uniform float uIceAlbedo;\n"
               "uniform float uIceReflectFrac;\n"
               "uniform float uIceGlintFrac;\n"
               // S1b winter night: uWinterNightGlow is the neutral lift snow
               // and ice keep after dark (moon/starlight off a high-albedo
               // surface); uIceNightReflectFrac drops the sky-mirror further at
               // night so the ice stops taking the black sky's colour.
               "uniform vec3 uWinterNightGlow;\n"
               "uniform float uIceNightReflectFrac;\n"
               // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon
               // AND starlight). uSnowSparkle is the night glint amplitude,
               // uSnowSparkleSharp the exponent.
               "uniform float uSnowSparkle;\n"
               "uniform float uSnowSparkleSharp;\n"
               // ★ R5 row 3 — THE TRACK READS. On the PATCH pass texcoord.y
               // carries per-vertex track COMPACTION, normalized [0,1] (see
               // render/snow_patch.h), and this dial tints the snow albedo
               // toward packed snow by it. 0 on the planet pass and the lake
               // mirror (where texcoord.y is ambient DEPTH in metres, not
               // compaction) -- clamp(0 * x) == 0 and mix(a, b, 0) == a keep
               // those passes bit-identical. Armed only in draw_snow_patch.
               "uniform float uTrackPackMix;\n"
               // ★ R5 row 8 — SUN SPARKLE. uSunSparkle is the daylight glint
               // amplitude (SnowParams::sun_sparkle × the SEADS_SPARKLE dial;
               // 0 kills the row exactly — the term is MULTIPLIED by it last,
               // and x*0 == 0, +0 is exact on the non-negative lit). Same
               // facet lattice as the night sparkle above: a stable function
               // of WORLD POSITION only (no clock, no screen-space term), so
               // the glints are nailed to the ground and animate ONLY as the
               // eye or sun moves. uSparkleCompactArm arms the compaction
               // SUPPRESSION — the one signal, two consumers rule
               // (sim/sled.h roost_flux precedent): texcoord.y carries
               // COMPACTION only on the patch pass, so this arm is 0 on the
               // planet pass and the lake mirror (where .y is DEPTH in
               // metres) and 1 only inside draw_snow_patch, the
               // uTrackPackMix discipline exactly. clamp(0 * x) == 0 keeps
               // the planet pass unsuppressed and bit-exact.
               "uniform float uSunSparkle;\n"
               "uniform float uSunSparkleSharp;\n"
               "uniform float uSparkleCompactArm;\n"
               // ★ R5 row 9 — SHADOWS ON SNOW: receiver-side analytic
               // occluder proxies (option (c), the row's ruled architecture).
               // uShadowA[i].xyz/.w = EYE-RELATIVE capsule endpoint A +
               // radius; uShadowB[i].xyz = endpoint B (draw.cpp rebases the
               // world doubles at the seam). Each fragment casts a ray TOWARD
               // THE SUN and soft-tests the capsules — the shadow lands at
               // the rasterized fragment's own position on WHATEVER surface
               // drew it (planet mesh, lifted rider patch, lake mirror), so
               // the fence-3 under-the-world failure class cannot exist here.
               // uShadowCount == 0 is the row's fence: the guarded blocks in
               // main() are never entered and the pixel expression is the
               // shipped one VERBATIM — an early-out on caster count, never a
               // multiply by ~1.0. uShadowTanSun = tan(sun angular RADIUS)
               // from the SHIPPED 1.40 deg disc: penumbra half-width =
               // tan * slant, so the shadow SOFTENS with caster height for
               // free and CRISPS as he descends — that sharpening IS the
               // landing cue (probe: pen 7.3 m @300 m, 0.37 m @flare) and no
               // fixed blur may defeat it. uShadowTint is the full-umbra
               // multiplier on the DIRECT-SUN term only (uNightFill ambient
               // survives inside the shadow — snow shadows are sky-lit,
               // darker AND bluer per the ribbons.h ruling, never black).
               "uniform int uShadowCount;\n"
               "uniform vec4 uShadowA[16];\n"
               "uniform vec4 uShadowB[16];\n"
               "uniform vec3 uShadowTint;\n"
               "uniform float uShadowStrength;\n"
               "uniform float uShadowTanSun;\n") +
           kConeUniformsGLSL +
           kSkyUniformsGLSL + kAirFieldUniformsGLSL + kScatterGLSL +
           kAirFieldGLSL + kSkyGLSL + kConeGLSL +
           // Procedural reflected stars: quantize the reflected dir to cells;
           // ~6% of cells host a SOFT point (smoothstep falloff = the AA, no
           // fwidth -> no UB in the non-uniform water branch, and the soft size
           // kills sub-pixel aliasing as the reflected dir sweeps; the parallax
           // crawl is fine). Deterministic (no clock). Returns [0,1]. NOT added
           // to the sky pass -> the real starfield is untouched (the reflection
           // is procedural, doesn't match real positions).
           // ★ ROAD-REPAIR: p_hash13 and the whole snow-sparkle lattice now
           // arrive from render/snow_light_glsl -- the SAME string the SF2
           // bank strips concatenate, so a bank crest and the field beside it
           // glint off ONE lattice (that header says why the sparkle used to
           // stop at every road edge). The expressions are unchanged.
           kSnowSparkleGLSL +
           "float water_stars(vec3 dir){\n"
           "    vec3 g = dir * uStarDensity;\n"
           "    vec3 cell = floor(g);\n"
           "    float h = p_hash13(cell);\n"
           "    if (h < 0.85) return 0.0;\n"  // ~15% of cells host a star
           "    vec3 off = (vec3(p_hash13(cell+11.1), p_hash13(cell+27.3),\n"
           "                     p_hash13(cell+41.7)) - 0.5) * 0.7;\n"
           "    float d = length(fract(g) - 0.5 - off);\n"
           "    float star = smoothstep(0.16, 0.0, d);\n"  // SOFT point (the
                                                           // AA)
           "    return star * (0.4 + 0.6 * p_hash13(cell + 5.1));\n"  // varied
                                                                      // brightness
           "}\n"
           // ★ R5 row 9 — the per-fragment proxy occlusion. P is the
           // fragment's EYE-RELATIVE position (fragRel — same space as the
           // uShadow endpoints, so float error is O(view distance)); toSun is
           // the unit ray toward the sun. For each capsule: closest approach
           // between the sun ray (t >= 0) and the segment (s in [0,1]) by the
           // standard clamped two-parameter minimization; `miss` is the
           // signed clearance of the ray past the capsule surface; `pen` is
           // the penumbra HALF-width at the occluder distance t
           // (tan(sun radius) * t — distance-proportional softness, the
           // landing cue). Visibility is the smooth 0.5 + miss/(2*pen) ramp:
           // a proxy smaller than the local penumbra never reaches full
           // umbra (the honest antumbra washout the probe measured beyond
           // ~400 m slant), and per-proxy visibilities MULTIPLY (disjoint
           // casters compose; overlaps double-count slightly toward dark,
           // monotone and cheap). t == 0 (caster anti-sunward) leaves the
           // fragment lit except within ~radius of the proxy itself, which
           // reads as a thin contact-occlusion ring at the machine's own
           // ground line. Deterministic: no clock, no screen-space term
           // (fence 6).
           "float shadow_sun_occ(vec3 P, vec3 toSun) {\n"
           "    float vis = 1.0;\n"
           "    for (int i = 0; i < uShadowCount; ++i) {\n"
           "        vec3 A = uShadowA[i].xyz;\n"
           "        float r = uShadowA[i].w;\n"
           "        vec3 u = uShadowB[i].xyz - A;\n"
           "        vec3 w = P - A;\n"
           "        float b = dot(toSun, u);\n"
           "        float c = dot(u, u);\n"
           "        float d = dot(toSun, w);\n"
           "        float e = dot(u, w);\n"
           "        float den = c - b * b;\n"
           "        float t = (den > 1e-6) ? (b * e - c * d) / den : -d;\n"
           "        float s = (c > 1e-6) ? clamp((e + t * b) / c, 0.0, 1.0)\n"
           "                             : 0.0;\n"
           "        t = max(s * b - d, 0.0);\n"
           "        s = (c > 1e-6) ? clamp((e + t * b) / c, 0.0, 1.0) : 0.0;\n"
           "        float miss = length(w + t * toSun - s * u) - r;\n"
           "        float pen = max(uShadowTanSun * t, 1e-2);\n"
           "        vis *= clamp(0.5 + 0.5 * miss / pen, 0.0, 1.0);\n"
           "    }\n"
           "    return clamp((1.0 - vis) * uShadowStrength, 0.0, 1.0);\n"
           "}\n"
           "out vec4 finalColor;\n"
           "void main() {\n"
           "    vec4 texel = texture(cubemap, normalize(fragDir));\n"
           "    vec3 albedo = texel.rgb;\n"
           // M2: decode the object-space normal (planet-local). On water the
           // bake wrote N=radial, so this equals the mirror normal there. Earth
           // has no M2 map -> fall back to the mesh normal (uHasNormalMap=0).
           "    vec3 surfN = normalize(texture(normalCube, "
           "normalize(fragDir)).xyz "
           "* 2.0 - 1.0);\n"
           "    vec3 shN = (uHasNormalMap > 0.5) ? surfN : "
           "normalize(fragNormal);\n"
           "    shN = normalize(mix(shN, normalize(fragNormal), "
           "uVertexNormalMix * vRim));\n"
           // B5 SHATTER CONES. Gated three ways so the triplanar block is only
           // reached on near, steep, shock-fabric rock -- and all three gates
           // are spatially coherent (whole cliff faces agree), so warps take
           // the same branch and the dynamic branch is genuinely cheap.
           // One fetch carries both masks: r = shock fabric, g = barren.
           // uHasFabric guards the Earth/no-asset case, where the sampler is
           // bound to raylib's default WHITE 1x1 -- without it, barren would
           // read 1.0 everywhere and snow would vanish off the whole planet.
           "    vec4 fab = texture(coneCube, normalize(fragDir));\n"
           "    float barren = uHasFabric * fab.g;\n"
           "    float coneW = 0.0;\n"
           // ★ Fly 4 (2026-08-11): "texture the blackest areas... shatter
           // cone? I feel these would just read as texture." coneV is the
           // cone lattice's scalar VALUE channel (below, coupled into
           // face-dark) -- hoisted here, defaulted to 0.5 (cone_value's own
           // "no relief" neutral), so the face-dark expression can read it
           // in UNIFORM control flow whether or not cones are on/near/far.
           "    float coneV = 0.5;\n"
           "    if (uConeDetail > 0.0) {\n"
           "        float cm = uHasFabric * fab.r;\n"
           "        float slope = 1.0 - abs(dot(shN, normalize(fragDir)));\n"
           "        float sg = smoothstep(uConeSlopeLo, "
           "min(uConeSlopeLo + 0.10, 0.999), slope);\n"
           "        float df = 1.0 - smoothstep(uConeFadeNear, uConeFadeFar, "
           "length(fragRel));\n"
           "        coneW = uConeDetail * cm * sg * df;\n"
           "    }\n"
           "    if (coneW > 0.002) {\n"
           // fragDir * R is the sphere-surface point in metres: a stable,
           // seam-free planet-local parameterization for the lattice (relief is
           // tiny against R, so using the sphere point instead of the displaced
           // one costs no visible swim).
           "        vec3 conePt = normalize(fragDir) * 15000.0;\n"
           "        coneV = cone_value(conePt, shN);\n"
           "        shN = cone_detail_normal(conePt, shN, coneW);\n"
           "    }\n"
           // Landmask coverage (mip-filtered => a soft coverage fraction at
           // distance, so 300+ small lakes fade rather than strobe — Fable #5).
           "    float water = uHasWater * max(texel.a, uForceWater);\n"
           "    float ndl = max(dot(shN, -normalize(sunDir)), 0.0);\n"
           // W4 winter snow-cover: whiten the LAND albedo toward snow, MASKED
           // off water (1-water keeps lakes dark mirror; the water branch
           // overwrites lit anyway) and reduced on steep slopes (rock faces
           // keep grey). Applied to ALBEDO before the lighting products, so the
           // terminator (ndl), moon fill, and aerial are preserved (Fable
           // P2-5). Flatness = the surface normal vs the fragment's LOCAL UP
           // (normalize(fragDir) is the radial on the sphere) — flat ground
           // holds snow, cliffs shed it; NO fixed axis. uSeasonSnow=0
           // (non-winter) => identity.
           // Hoisted so the barren SLOPE GATE below can reuse it (Chad's fly
           // ruling 2026-08-11) rather than re-computing the same dot twice.
           "    float cosSlope = dot(shN, normalize(fragDir));\n"
           "    float snowFlat = smoothstep(uSnowSlopeLo, 1.0, cosSlope);\n"
           // ★ R3: the exposure term, crossfaded from the stub onto the field.
           // `snowFlat` is the legacy render-only mask; `depthCover` is the
           // real analytic depth arriving per-vertex, ramped to full coverage
           // at uSnowFullDepth. WINTER_LAW 3.2/6c.1 asks for exactly this: the
           // layer stops computing a private exposure and reads f() instead,
           // so the rock can no longer show where the sled is still sinking.
           //
           // ⚠ vSnowDepth already contains BOTH the slope shed and the barren
           // shed (they live inside ambient_depth_at -- see world/snowpack.cpp
           // "THE BLACK-ROCK SHED"), which is why the barren multiply below is
           // faded out by the same dial. Arming one without the other would
           // shed the black rock TWICE and over-blacken Chad's barrens.
           "    float depthCover = smoothstep(0.0, max(uSnowFullDepth, 1e-4), "
           "vSnowDepth);\n"
           "    float snowCover = uSeasonSnow * (1.0 - water) *\n"
           "                      mix(snowFlat, depthCover, uSnowDepthMix);\n"
           // ---- PROVISIONAL SNOW STUB -- to be REPLACED at WINTER_LAW S2 ----
           // WINTER_LAW.md 3.2/6c.1 is explicit: exposure must come from the ONE
           // shared analytic depth field, depth = f(slope, aspect, curvature,
           // elevation, drainage), and a layer must NOT compute a private
           // exposure mask -- or the rock shows where the sled is still sinking.
           // That contract is correct and this term does not satisfy it: today's
           // snowCover is a render-only approximation (uSeasonSnow * (1-water) *
           // snowFlat) that predates the depth function, so this is a stub on a
           // stub, kept only so the layer is visible before S2 exists.
           //
           // The PHYSICS in it is real and should survive, but as an INPUT to
           // f(), not as a second multiply here: low-albedo soil-denuded rock
           // absorbs sun and its knobs are wind-scoured, so blackened ground
           // genuinely carries less snow. Proposed interface -- barren becomes a
           // scour term inside the depth function, and this line is deleted:
           //     depth *= (1.0 - k_barren * barren)      // k_barren ~ 0.75
           // WINTER_LAW 5 sanctions exactly this ("key against a provisional
           // depth stub with the SAME signature so the swap is one line").
           // ★ THE SAME SHED LAW THE SIM USES (INV-9), not a lookalike:
           // smoothstep(lo, hi, barren), with lo/hi/k arriving as uniforms from
           // world::SnowpackParams so the constants have ONE home. GLSL
           // smoothstep is the same Hermite 3t^2-2t^3 as world::smoothstep01,
           // so the two consumers agree by construction rather than by comment.
           // Derivation of 0.10/0.60 lives in world/snowpack.h.
           "    float bshed = smoothstep(uBarrenShedLo, uBarrenShedHi, barren);\n"
           // ★ THE SLOPE GATE (Chad's fly rulings 2026-08-11 x2): flat
           // barrens read as still wintered; sloped/steep barren faces are
           // the blackest. Mirrors world::SnowpackField::barren_slope_gate
           // from the SAME constants (INV-9) -- runs on 1-cos(slope) so both
           // consumers can produce it exactly.
           // ★ KEYED ON A MACRO NORMAL, NOT shN -- measured on the shipped
           // assets: the normal MAP's per-texel slopes are 4 m
           // micro-roughness (p50 17-19 deg on barren ground even where the
           // landform is flat), so a gate on shN half-opens EVERYWHERE and
           // Chad's second fly read "flats still too dark, faces not black"
           // is exactly that. The macro landform (barren p50 ~3 deg, faces
           // p95+ >= ~11 deg) is the thing his ruling is about, and it
           // matches the sim consumer's 40 m probe scale, so INV-9's two
           // consumers agree in SCALE as well as constants. Micro detail
           // keeps its say via snowFlat(shN).
           // ★ THE MACRO NORMAL IS A MIP-SMOOTHED normalCube SAMPLE, not
           // fragNormal (fly 3): the mesh normal is interpolated across
           // ~59 m triangles, so the gate bent at triangle edges exactly
           // where the snow->rock band sits -- Chad's "really triangulated"
           // spot near Kelly Lake. textureLod at a ~90 m footprint is
           // per-pixel smooth (the cubemap has a full mip chain), same
           // scale, no facets. fragNormal remains only as the no-asset
           // fallback.
           "    vec3 macroN = (uHasNormalMap > 0.5)\n"
           "        ? normalize(textureLod(normalCube, normalize(fragDir), "
           "uBarrenNormalLod).xyz * 2.0 - 1.0)\n"
           "        : normalize(fragNormal);\n"
           "    float cosMacro = dot(macroN, normalize(fragDir));\n"
           "    float bgate = uBarrenFlatShed + (1.0 - uBarrenFlatShed) *\n"
           "                  smoothstep(uBarrenSlopeXLo, uBarrenSlopeXHi, "
           "1.0 - cosMacro);\n"
           // ★ R3: faded out as the depth channel is armed -- vSnowDepth has
           // ALREADY paid this shed (k_barren x barren_shed x barren_slope_gate
           // inside ambient_depth_at). At uSnowDepthMix = 1 the FADE collapses to
           //
           // ★★★ uBarrenShedKeep BREAKS THE WELD, AND CHAD RULED IT (2026-08-27).
           // The one-home argument below holds ONLY while the depth field can
           // actually EXPRESS the shed -- i.e. while full_depth is deep enough
           // that shed ground lands BELOW saturation. He drove the sweep and
           // chose full_depth = 0.10 m ("I see intermediate and the whiter tone
           // is better"), and at 0.10 EVERYTHING saturates, so the depth field's
           // own black-rock shed goes invisible: measured, sloped rock went
           // 0.230 (the coverage he approved) -> 0.431. His tone ruling and his
           // black-rock fence were welded to ONE dial, so satisfying either one
           // broke the other. They are now TWO dials.
           //
           // max(), not a product or a sum: the shed survives at whatever the
           // LARGER of the two demands, so keep = 0 is EXACTLY the pre-ruling
           // expression (pinned by a test) and keep = 1 holds the fence at any
           // mix. No new endpoint behaviour, and nothing double-sheds -- max()
           // cannot shed harder than one full application.
           //
           // (the original note, still true of the FADE half:)
           // the identity multiply and the shed has exactly ONE home, which is
           // what the "PROVISIONAL SNOW STUB" note above says the endpoint is.
           // `bshed`/`bgate` stay live regardless -- FACE-DARK below reads them
           // as COLOUR, not exposure, and Chad's blackest faces are his ruling.
           "    snowCover *= (1.0 - max(1.0 - uSnowDepthMix, uBarrenShedKeep) * "
           "uBarrenSnowShed * bshed * bgate);\n"
           "    albedo = mix(albedo, vec3(uSnowAlbedo), snowCover);\n"
           // ★ R5 row 3 — the packed-track tint. The rut has geometry and no
           // tone: white-on-white under a high sun casts no shadow into a
           // 0.55 m groove, so the cut must read by VALUE, and the ruling on
           // how is already written down at render/ribbons.h:37-47 -- packed
           // snow is DARKER and BLUER than the wild snowpack, never brighter
           // (a white trail on a white world is less legible than the clay it
           // replaced, S1_SPEC RT-1). The target colour MIRRORS
           // trail_winter_color {0.80, 0.84, 0.90} (vs snow_albedo 0.92);
           // re-derive it here if that ruling ever moves.
           //
           // sqrt() is Chad's SMOKIER ruling (R5 ledger, his option 4), in one
           // term: compaction arrives as a plateau with a ~0.24 m smoothstep
           // skirt, and the sqrt stretches the skirt's low values upward so
           // the tint blooms softly past the geometric cut instead of banding
           // at it -- haze, not a painted groove. sqrt(0) == 0 exactly and
           // clamp/mix pass 0 through exactly, so zero compaction -- every
           // untracked pixel, and the whole planet pass at uTrackPackMix = 0
           // -- is the shipped pixel to the bit (fence 5). Scaled by snowCover
           // so only SNOW takes the tint: shed rock and water keep their own
           // albedo, and the tint desaturates by construction (a mix toward a
           // near-neutral constant), reading as density, not dirt.
           "    float packT = clamp(uTrackPackMix * sqrt(max(vSnowDepth, "
           "0.0)), 0.0, 1.0);\n"
           "    albedo = mix(albedo, vec3(0.80, 0.84, 0.90), packT * "
           "snowCover);\n"
           // ★ FACE-DARK (Chad's fly 3): "the really steep slopes need to be
           // even blacker so it stands out more." Past the full-shed point
           // (uBarrenSlopeXHi) the shed has nothing left to remove -- the
           // rock is already bare -- so the steepest barren faces darken the
           // ALBEDO itself toward void-black (self-shadowed blast rock).
           // RENDER-ONLY: colour, not exposure -- the depth law and INV-9
           // are untouched. Gated by bshed so only mapped barren rock
           // darkens; continuous ramp so no decal edge.
           // ★ Fly 4: normal-only cone detail is invisible on ~0.02 albedo
           // (deepened face-dark, Change 1 above), so coneV -- the cone
           // lattice's scalar VALUE -- modulates the face-dark STRENGTH
           // itself: ridges (coneV above the 0.5 neutral) keep more albedo,
           // grooves (below 0.5) darken further, so within the cone's
           // 140-420 m fade the black faces resolve into charcoal striae
           // ridges over void-black grooves. No new fade term is added --
           // coneW already carries the cone block's own anti-moire distance
           // fade, so fdMod collapses to exactly 1.0 (identity) once coneW
           // -> 0, matching the untextured far-field face-dark exactly.
           "    float fdMod = 1.0 - 0.35 * (coneV - 0.5) * 2.0 * coneW;\n"
           // ★ Fly 5 ("smooth and shiny black... the most steep parts need to
           // be mottled a lot"): a flat multiply preserves the baked mottle's
           // RATIO but crushes its ABSOLUTE range -- x0.05 leaves 1-3 DN,
           // invisible, so the steepest faces read as smooth paint. The
           // mottle's bright patches now RESIST face-dark: fdKeep maps the
           // incoming rock luminance across the baked mottle's span
           // (PAL_BARREN lum 0.158 x clip [0.6, 1.4] => [0.095, 0.221] --
           // ★ these two literals MIRROR C.BARREN_MOTTLE_LO/HI x PAL_BARREN;
           // re-derive them if either bake constant moves) and scales the
           // kill down by up to uBarrenFaceMottle at the bright end. Grooves
           // stay void-black; charcoal patches survive; the steepest rock is
           // the MOST mottled. uBarrenFaceMottle = 0 restores the flat kill.
           "    float rockLum = dot(albedo, vec3(0.299, 0.587, 0.114));\n"
           "    float fdKeep = 1.0 - uBarrenFaceMottle *\n"
           "        clamp((rockLum - 0.095) / 0.126, 0.0, 1.0);\n"
           "    albedo *= 1.0 - uBarrenFaceDark * fdMod * fdKeep * bshed *\n"
           "        smoothstep(uBarrenSlopeXHi, uBarrenFaceDarkXHi, "
           "1.0 - cosMacro);\n"
           // ★ S1 LAKE ICE. The land whitens above; the lakes FREEZE here. Same
           // mechanism deliberately — an ALBEDO change BEFORE the lighting
           // products, so terminator/moon-fill/aerial survive exactly as they do
           // for snow (Fable P2-5). uSeasonSnow=0 => identity, so summer is
           // bit-unchanged.
           //
           // ★ uIceAlbedo is BELOW uSnowAlbedo on purpose. §6b.2 makes this
           // plane the drivable ice and §3.5 punches through it, so "where does
           // the lake end and the shore begin" is a survival read. Equal values
           // erase the shoreline. The mirror is damped in the water branch
           // below rather than removed — real ice keeps a weak sheen, and
           // deleting the branch would fork the shared river path.
           "    float iceCover = uSeasonSnow * water;\n"
           "    albedo = mix(albedo, vec3(uIceAlbedo), iceCover);\n"
           // ★ S1b NIGHT: the sun-elevation night factor, hoisted ABOVE the
           // lighting so the winter surfaces can use it. Same form the star
           // reflection already used below — which now reads THIS one, so the
           // two can never drift apart.
           "    float sunElHere = dot(-normalize(sunDir), normalize(fragDir));\n"
           "    float nightAmt = 1.0 - smoothstep(-0.10, 0.05, sunElHere);\n"
           "    float dist = length(fragRel);\n"
           "    vec3 viewDir = fragRel / max(dist, 1e-3);\n"
           // S-airdome (spec §1.5/§1.6): the LOCAL air amount at this ground
           // fragment's own world position (a single air_at() sample, not a
           // march — this feeds LOCAL dimming effects: the lake glint anti-
           // glare and the aurora ground glow's clear-air factor, both about
           // "how much air is right here", unlike sky_aerial's ground-segment
           // MARCH below which answers "how much air is between eye and
           // here").
           "    vec3 fragWorldPos = air_eye_pos() + fragRel;\n"
           "    float airHere = air_at(fragWorldPos);\n"
           // Day/night terminator (Stage 3b — the 0.38 bare number dies): the
           // night side falls to uNightFill (a real dark floor, not flat grey),
           // the day side rises by uGroundDayGain*ndl. ndl (surface normal vs
           // the moving eye-relative sun) IS the sun-elevation curve on a
           // sphere, so a moving sun sweeps a smooth terminator. Day peak =
           // albedo*(fill
           // + gain) ~ albedo (full B&W detail, no blowout).
           // ★ R5 row 9 — SHADOWS ON SNOW. THE EARLY-OUT IS THE FENCE: with
           // zero casters neither guarded block below is entered, shadowOcc
           // stays the literal 0.0, and the lit expression is the SHIPPED
           // line verbatim — every pixel on the planet is bit-identical
           // (pinned by test_snow_shadows, the row-3 tint pattern). fragRel
           // is already eye-relative, the same space as the uShadow
           // endpoints; -normalize(sunDir) is the ray toward the sun, the
           // exact convention ndl uses.
           "    float shadowOcc = 0.0;\n"
           "    if (uShadowCount > 0) {\n"
           "        shadowOcc = shadow_sun_occ(fragRel, -normalize(sunDir));\n"
           "    }\n"
           "    vec3 lit = albedo * (uNightFill + uGroundDayGain * ndl);\n"
           // The shadow multiplies THE DIRECT-SUN TERM ONLY, per channel:
           // subtract what the occlusion removes of albedo*gain*ndl, tinted
           // so full umbra lands at uShadowTint x the direct sun (darker AND
           // bluer — the ribbons.h:37-47 ruling rows 3 and 8 obeyed; a snow
           // shadow is lit by blue sky). uNightFill survives inside the
           // shadow BY CONSTRUCTION — it is outside the subtraction — so the
           // shadow can never go black. Guarded by the same early-out: at
           // zero casters this statement does not exist in the pixel's
           // dataflow.
           "    if (uShadowCount > 0) {\n"
           "        lit -= albedo * (uGroundDayGain * ndl * shadowOcc) *\n"
           "               (vec3(1.0) - uShadowTint);\n"
           "    }\n"
           // Moonlight (Stage 5): a 2nd directional light, ndl byte-identical
           // to the sun's form. uMoonFill is phase-scaled (0 at new moon), so
           // the moon-facing hemisphere gets silver fill ONLY near full moon
           // and the dark side stays genuinely dark at new moon (the
           // break-contact tactic).
           "    float ndlMoon = max(dot(shN, -normalize(uMoonDir)), 0.0);\n"
           "    lit += albedo * uMoonFill * ndlMoon;\n"
           // ★★ S1b WINTER NIGHT GLOW (Chad, 2026-08-10): "it should just lose
           // brightness at night, but the stars and moon should be reflecting
           // off the snow and ice making it white. That's the beauty of a
           // winter night." Before this, snow and ice fell to albedo*uNightFill
           // and read as if they had MELTED — the world went back to looking
           // like summer the moment the sun set.
           //
           // This is the physically right term, not a cheat. A snowfield really
           // is bright on a clear winter night: albedo ~0.9 against ~0.1 for
           // summer ground returns roughly 9x the incident starlight/skyglow,
           // and multiple scattering between the snowpack and the sky compounds
           // it — the "snow glow" of anyone who has walked a field at night.
           // uNightFill cannot express it, being a flat floor that knows
           // nothing about what it is illuminating.
           //
           // NEUTRAL (one scalar broadcast to vec3) ON PURPOSE: snow must LOSE
           // BRIGHTNESS, never CHANGE HUE. A tinted lift is exactly what makes
           // it read as a different material — which is the bug being fixed.
           // ★ S1b-2 (Chad, second night report): "in the dark of night the land
           // goes to a dark shade, snow needs to STAY WHITE."
           //
           // The first attempt added a FLAT lift. That was the wrong FORM, not
           // merely too small a number: a constant raises snow and dark rock by
           // the same amount, so it greys the whole scene toward the middle and
           // can never make snow read white without also lifting everything that
           // should stay black. Cranking it would have washed the world out.
           //
           // Night illumination MULTIPLIES albedo. Modelled that way, snow
           // (albedo 0.92) and a bare rock face (~0.1) separate about 9:1 for
           // free, which IS the winter night: a white ground with near-black
           // verticals on it. §2.5 calls those dark verticals load-bearing, and
           // this form protects them where a flat lift erodes them.
           //
           // Still gated on winterSurf, so slopes that SHED snow (snowFlat)
           // stay dark and cliffs keep reading as rock against the white.
           // ★ and it is added NEUTRAL, scaled by albedo LUMINANCE rather than
           // per-channel albedo. Measured: multiplying per-channel left open
           // snow at RGB (0.782, 0.749, 0.693) — R/B = 1.13, a visible cream
           // cast, when Chad's requirement is literally "snow needs to stay
           // WHITE".
           //
           // The cause is not the night term, it is that `winter_snow_cover` is
           // 0.85, so 15% of the warm base terrain albedo always shows through
           // the snow mix. Multiplying by that albedo amplifies the tint. Using
           // its LUMINANCE keeps the 9:1 snow-vs-rock separation that makes the
           // dark verticals read, while the light added is pure white — which is
           // also what moon and starlight physically are.
           "    float winterSurf = max(snowCover, iceCover);\n"
           "    float albLum = dot(albedo, vec3(0.299, 0.587, 0.114));\n"
           "    lit += vec3(albLum) * uWinterNightGlow * winterSurf * nightAmt;\n"
           // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon AND
           // starlight). Computed in UNIFORM control flow (fwidth() inside
           // the non-uniform `if (water > 0.0)` branch below is UB per the
           // GLSL spec -- the same reasoning that keeps the sun/moon lake
           // glint fwidth() calls hoisted above that branch), and placed
           // BEFORE it.
           // Planet-local cell lattice on the unit fragment direction: ~0.4 m
           // cells.
           // Planet-local cell lattice on the unit fragment direction: ~0.4 m
           // cells, sparse SOFT hosts, a per-cell jittered facet -- all of it
           // now render/snow_light_glsl, shared with the bank strips.
           "    vec3 spc = snow_sparkle_cell(fragDir);\n"
           "    float spHost = snow_sparkle_host(spc);\n"
           "    vec3 spN = snow_sparkle_facet(spc, shN);\n"
           // Glint: reflect the moon about the facet, dot toward the eye. Eye
           // motion animates it -- NO clock, same determinism rule as
           // water_stars. The STAR share of the light rides the gate's
           // additive kStar below (a facet lit by starlight glints from
           // "somewhere in the sky"; keying the geometry to the moon dir is
           // visually indistinguishable and keeps ONE anti-moired term). The
           // fwidth LOD fade and the 400-600 m cutoff live inside the shared
           // function; the call is at the top level of main(), the uniform
           // control flow fwidth() requires (unchanged discipline).
           "    float spGlint = snow_sparkle_glint(spN, uMoonDir, viewDir,\n"
           "                                       uSnowSparkleSharp, spHost, dist);\n"
           // Gate: night only, on winter surfaces (snow OR ice -- barren
           // cores shed snowCover so bare rock does not sparkle), lit by
           // stars ALWAYS (kStar -- load-bearing: Chad explicitly wants
           // starlight sparkle, it must NOT be moon-gated) plus the
           // phase-scaled moon.
           "    float kStar = 0.15;\n"
           "    float spGate = nightAmt * winterSurf * (kStar + uMoonFill * "
           "ndlMoon);\n"
           "    float snowSparkle = clamp(spGlint * spGate, 0.0, 1.0) * "
           "uSnowSparkle;\n"
           // ★ R5 row 8 — SUN SPARKLE, the daylight sibling of the block
           // above, and row 3's partner: the virgin field glitters, the
           // packed rut does not, so the cut reads by CONTRAST instead of by
           // darkening (ribbons.h S1_SPEC RT-1: never brighten the track).
           // REUSES the night lattice (spc/spHost/spJit/spN): same ~0.4 m
           // planet-local cells keyed on normalize(fragDir) — pure WORLD
           // POSITION, no clock, no screen term, so glints are nailed to the
           // ground and appear/disappear only as eye or sun moves (fence 6:
           // no crawl, no boil). Sun-keyed glint, same reflect convention as
           // the moon's.
           "    float ssGlint = snow_sparkle_glint(spN, sunDir, viewDir,\n"
           "                                       uSunSparkleSharp, spHost, dist);\n"
           // ★ THE COMPACTION SUPPRESSION — one signal, two consumers. Same
           // sqrt skirt as row 3's packT, deliberately: where the tint blooms
           // smoky, the sparkle dies, so the two reads of compaction_at agree
           // in SHAPE as well as source. Compaction 1 -> suppressed to
           // exactly 0; compaction 0 (and the WHOLE planet pass + lake
           // mirror, arm = 0: clamp(0 * x) == 0) -> fully unsuppressed.
           "    float ssPack = clamp(uSparkleCompactArm * sqrt(max(vSnowDepth, "
           "0.0)), 0.0, 1.0);\n"
           // Gate: DAY only (1 - the night amount the night sparkle uses, so
           // the two hand over at the same terminator), sun-facing (ndl —
           // shadow-side facets do not glint), and SNOW only via snowCover —
           // NOT winterSurf: fence 3 says rock, water and ICE stay untouched,
           // and iceCover is deliberately excluded here (packed lake ice is
           // not faceted powder).
           "    float ssGate = (1.0 - nightAmt) * ndl * snowCover * (1.0 - "
           "ssPack);\n"
           // ★ R5 row 9: a facet inside a sun shadow has no direct sun to
           // glint — the row-8 sparkle dies with the direct term. Guarded by
           // the same caster-count early-out (never a multiply by ~1.0), so
           // at zero casters row 8's shipped expression is untouched to the
           // bit and Chad's signed SEADS_SPARKLE=1.0 look cannot move.
           "    if (uShadowCount > 0) { ssGate *= 1.0 - shadowOcc; }\n"
           "    float sunSparkle = clamp(ssGlint * ssGate, 0.0, 1.0) * "
           "uSunSparkle;\n"
           // Mirror-silver lakes: reflect the eye->frag ray about the water
           // normal (= local up = surface dir) and sample the SHARED sky_color
           // (up = n), so a lake and the planet limb add the IDENTICAL sky at
           // the horizon — no fork. Sparkle is the sun glint, LOD-faded so a
           // distant few-texel lake doesn't strobe (Fable #5).
           // Reflection ray + sun-glint dot + its screen derivative are
           // computed for EVERY fragment (uniform control flow) — fwidth() in
           // the non-uniform water branch is undefined per the GLSL spec, so
           // the anti-strobe LOD fade would be driver-dependent (Fable red-team
           // P1).
           "    vec3 wN = normalize(fragDir);\n"
           "    vec3 wRefl = reflect(viewDir, wN);\n"
           // Clamp the reflected ray to the LOCAL horizon (Fable P1-1): a
           // DRAPED river on a valley wall viewed from below can reflect wRefl
           // DOWNWARD -> sky_color below the horizon; mirror the sub-horizon
           // component back up. No-op for lakes (eye always above -> dot >= 0).
           "    wRefl = normalize(wRefl - 2.0 * min(0.0, dot(wRefl, wN)) * "
           "wN);\n"
           // Reflected STAR sparkle, computed in UNIFORM control flow (no
           // fwidth -> no UB in the water branch): night-gated (sun below the
           // local horizon wN) + haze-dimmed. Consumed inside the water branch
           // below.
           // Reuses the hoisted nightAmt (identical expression, computed once)
           // so the star reflection and the winter night glow can never
           // disagree about when night is.
           "    float wNight = nightAmt;\n"
           // A flat mirror reflects a tiny solid angle, so a top-down water
           // view would sample almost ONE star cell everywhere (invisible).
           // Scatter the star-sample dir by a STATIC per-location micro-ripple
           // (hash of the surface position, no clock) so a full starfield
           // shimmers on the water — the sky reflection itself stays a calm
           // mirror (only the star sampling is jittered).
           "    vec3 rc = floor(wN * 3000.0);\n"  // wN = normalize(fragDir):
                                                  // unit -> stable hash
           "    vec3 rjit = (vec3(p_hash13(rc), p_hash13(rc + 7.1), "
           "p_hash13(rc + 13.3)) - 0.5) * 0.5;\n"
           "    float wStars = water_stars(normalize(wRefl + rjit)) * wNight * "
           "clamp(1.0 - airHere, 0.0, 1.0);\n"
           "    float wSd = max(dot(wRefl, -normalize(sunDir)), 0.0);\n"
           "    float wSdFw = fwidth(wSd);\n"
           // Moon glint (Stage 5): the reflected-ray dot vs the moon + its
           // screen derivative, computed in UNIFORM control flow (fwidth in the
           // branch is UB — the same Fable fix the sun glint got). Phase-scaled
           // by uMoonFill.
           "    float wSdMoon = max(dot(wRefl, -normalize(uMoonDir)), 0.0);\n"
           "    float wSdMoonFw = fwidth(wSdMoon);\n"
           "    if (water > 0.0) {\n"
           // S-airdome (spec §1.3): skyRefl's airAmt is a proper march from
           // THIS fragment (not the eye) out along the reflected ray — the
           // reflection looks skyward from the water surface, so its "how
           // much air between here and the sky" answer is generally
           // different from the eye's own.
           "        float rMax = air_sky_max_dist(fragWorldPos, wRefl);\n"
           "        float tauR = air_optical_depth(fragWorldPos, wRefl, rMax, "
           "uAirStepsSky);\n"
           "        vec3 skyRefl = sky_color(wRefl, sunDir, wN, uEyeAlt, "
           "tauR);\n"
           "        float sp = pow(wSd, uSparkleSharp);\n"
           "        sp *= 1.0 - smoothstep(0.0, 0.6, wSdFw * uSparkleSharp);\n"
           // Overcast that hides the sun disc also dims its lake glint.
           "        sp *= clamp(1.0 - airHere, 0.0, 1.0);\n"
           // Moonlit-silver lake glint: a weaker, phase-scaled specular of the
           // moon (the hero shot). Same anti-strobe + haze dimming as the sun.
           "        float spMoon = pow(wSdMoon, uMoonSparkle) * uMoonFill;\n"
           "        spMoon *= 1.0 - smoothstep(0.0, 0.6, wSdMoonFw * "
           "uMoonSparkle);\n"
           "        spMoon *= clamp(1.0 - airHere, 0.0, 1.0);\n"
           // Reflected stars: additive, clamped (no blowout onto the near-black
           // night mirror). Single-sourced here so lakes AND rivers reflect the
           // same sparkle.
           "        float stars = clamp(wStars * uStarReflect, 0.0, 1.0);\n"
           // ★ S1: under winter the lake is ICE, so the mirror is DAMPED, not
           // deleted. Ice keeps a weak sheen and a much weaker glint; a hard
           // branch would fork the river path that shares this code, and a
           // mirror-strength ice sheet reads as open water you can land on.
           // uSeasonSnow = 0 => both factors are exactly 1.0 and summer is
           // bit-unchanged.
           // ★ S1b: and the SECOND cause of Chad's night report — the ice was
           // going dark *and shifting hue*, which a neutral brightness floor
           // cannot explain. It was MIRRORING THE NIGHT SKY. Even at 18%, a
           // near-black blue sky mixed into an already-dim surface drags the
           // lake toward blue-black, so the ice read as open water again.
           //
           // Real lake ice at night is not a dark mirror; it is a pale sheet
           // carrying moon and starlight. So the sky-mirror falls further after
           // dark — while the star/moon GLINT terms below are deliberately left
           // alone, because those reflections are precisely what Chad asked to
           // keep ("the stars and moon should be reflecting off the snow and
           // ice"). Damping the sky and keeping the glints is the whole fix.
           "        float iceRefl = mix(1.0, uIceReflectFrac * "
           "mix(1.0, uIceNightReflectFrac, nightAmt), uSeasonSnow);\n"
           "        float iceGlint = mix(1.0, uIceGlintFrac, uSeasonSnow);\n"
           "        vec3 wcol = mix(lit, skyRefl, uReflectivity * iceRefl) + "
           "vec3((sp + spMoon + stars) * iceGlint);\n"
           "        lit = mix(lit, wcol, water);\n"
           "    }\n"
           // Snow sparkle: additive, clamped, land-only (the lake mirror owns
           // its own star glints); placed before sky_aerial so distance haze
           // dims sparkle like everything else.
           "    lit += vec3(snowSparkle) * (1.0 - water);\n"
           // ★ R5 row 8: the sun sparkle rides the same ADDITIVE highlight
           // path, NOT albedo — snow albedo 0.92 leaves no headroom below the
           // diffuse white, and a real crystal glint is specular: it must
           // exceed the field to read. Additive after the water branch,
           // land-masked, and before sky_aerial so haze dims glints like
           // everything else. uSunSparkle = 0 adds exactly 0.0 (fence 1).
           "    lit += vec3(sunSparkle) * (1.0 - water);\n"
           // Aurora ground glow (Stage 8): a faint green EMISSIVE wash under
           // the oval, night-gated + cloud-suppressed. Added AFTER the lit
           // factor (so it does not interact with the moon fill, Fable-after
           // P2-2), keyed to the SAME oval colatitude ring as the sky curtain
           // (NOT max(dot,â), which peaks at the pole not the oval).
           "    float rhoF = acos(clamp(dot(normalize(fragDir), uSpinAxis), "
           "-1.0, 1.0));\n"
           "    float dF = (rhoF - uAurOvalC) / max(uAurOvalW, 1e-4);\n"
           "    float ring = exp(-dF * dF);\n"
           "    float nightG = 1.0 - smoothstep(-0.10, 0.05, "
           "dot(-normalize(sunDir), uUp));\n"
           // S-airdome (spec §1.6): the SAME local air_at() sample supersedes
           // the exp-shell clear-air factor (a dome's own weather still hides
           // the glow; the vacuum gap never has it to hide).
           "    float clearG = 1.0 - clamp(airHere, 0.0, 1.0);\n"
           "    lit += uAurTintLo * (uAurGroundGlow * ring * nightG * "
           "clearG);\n"
           // Aerial perspective: fade the ground toward the sky along the view
           // ray (fragRel is eye-relative, so the subtraction is
           // precision-safe).
           "    lit = sky_aerial(lit, viewDir, dist, sunDir, uUp, uEyeAlt);\n"
           "    finalColor = vec4(lit, 1.0);\n"
           "}\n";
}

// Upload a pure FaceMesh (built by render::fill_face — the shared, testable
// cubesphere geometry in sphere_param.h) into a raylib Mesh on the GPU. The
// shader samples the albedo from the interpolated DIRECTION (kFS), so there are
// still no UVs in the texture sense.
//
// ★ R3: texcoord .y CARRIES THE PER-VERTEX AMBIENT SNOW DEPTH, in metres,
// when fill_face produced one. .x IS DELIBERATELY LEFT AT 0: it is vRim, the
// SF3-A snow-patch rim weight, and on the planet pass it must stay 0. It would
// be inert either way today (the planet pass sets uVertexNormalMix = 0, which
// multiplies vRim out entirely), but "it is currently multiplied by zero" is
// not a contract -- writing depth into .x would silently become a
// normal-blend weight the day that uniform is ever armed on this pass.
//
// No fold bound (Earth, a procedural planet, SEADS_NO_SNOWFOLD) => `depths` is
// EMPTY => no texcoord buffer is uploaded at all, the attribute reads its 0
// default, and uSnowDepthMix is forced to 0 at the draw so the shader keeps its
// legacy exposure. That is what makes the A/B honest rather than approximate.
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
    // The depth channel, when present, as a vec2 texcoord (0, depth_m).
    if (fm.depths.size() == static_cast<std::size_t>(m.vertexCount)) {
        m.texcoords = static_cast<float*>(
            MemAlloc(sizeof(float) * fm.depths.size() * 2));
        for (std::size_t i = 0; i < fm.depths.size(); ++i) {
            m.texcoords[i * 2 + 0] = 0.0f;           // vRim -- never depth
            m.texcoords[i * 2 + 1] = fm.depths[i];   // metres of ambient snow
        }
    }
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
                   int subdiv, int tiles, double u_offset, int dem_blur_radius,
                   int cubemap_size, bool procedural, double water_reflectivity,
                   double water_sparkle, const std::vector<CutDisk>& cuts) {
    Planet p;
    p.R = R;
    p.water_reflectivity = static_cast<float>(water_reflectivity);
    p.water_sparkle = static_cast<float>(water_sparkle);
    if (subdiv < 2) subdiv = 2;
    if (subdiv > 256) subdiv = 256;
    if (tiles < 1) tiles = 1;
    if (tiles > 4) tiles = 4;  // 6*16 meshes / ~6.1M tris — the sane ceiling
    if (dem_blur_radius < 0) dem_blur_radius = 0;
    if (cubemap_size < 1) cubemap_size = 1;
    // The Sudbury DEM is pre-blurred + lakes pre-flattened offline; a runtime
    // re-blur would soften the mirror-flat lakes back into bumps (Fable fix
    // #2).
    if (procedural) dem_blur_radius = 0;

    char color_path[512], dem_path[512];
    std::snprintf(color_path, sizeof color_path, "%s/%s", asset_dir,
                  procedural ? "sudbury_color.png" : "earth_color_5400.jpg");
    std::snprintf(dem_path, sizeof dem_path, "%s/%s", asset_dir,
                  procedural ? "sudbury_dem.png" : "earth_dem_2048.png");

    Image dem_img = LoadImage(dem_path);
    if (dem_img.data == nullptr) {
        TraceLog(LOG_WARNING, "PLANET: DEM load failed: %s", dem_path);
        return p;
    }
    // Soften sub-quad DEM detail the mesh can't resolve (reduces shading-normal
    // noise on steep coasts before it reaches the normals). Radius is coupled
    // to relief_scale (a heavier displacement needs a heavier blur) — tuned in
    // config/world.toml, never a bare number here. ONLY on a plain grayscale
    // DEM (the earth stand-in): any multi-channel format is presumed the R4c
    // 16-bit hi/lo pack, where independent per-channel blur would corrupt the
    // height (the pack ships pre-blurred from the offline bake). Allowlist,
    // not blocklist — a future RGBA/palette pack fails SAFE (adversarial
    // review P3-3).
    if (dem_blur_radius > 0) {
        if (dem_img.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
            ImageBlurGaussian(&dem_img, dem_blur_radius);
        else
            TraceLog(LOG_WARNING,
                     "PLANET: packed (non-grayscale) DEM — dem_blur_radius "
                     "ignored");
    }
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
    // Decode the height store through the ONE dem16 convention (R4c,
    // world/heightfield.h): a 16-bit-packed RGB DEM carries hi/lo in R/G; a
    // legacy 8-bit grayscale DEM expands to r == g == b in LoadImageColors, and
    // dem16_unpack(r, r) == r*257 — the bit-identical promote R1 pinned. One
    // unconditional path, no format branch to fork.
    for (std::size_t i = 0; i < p.height.px.size(); ++i)
        p.height.px[i] = world::dem16_unpack(px[i].r, px[i].g);
    UnloadImageColors(px);

    // Mesh and height field now read ONE source (p.height):
    // fill_face(p.height,…)
    // == p.height.radius_at at every vertex (pinned in test_sphere_param).
    // R4d: each cube face is tiles×tiles sub-meshes (watertight by the global-
    // index construction; shared-edge bit-identity pinned in
    // test_sphere_param).
    // ★ R1: THE MESH FILL MOVED. It used to sit here, immediately after the
    // DEM decode. It now runs AFTER the landmask and barren rasters load,
    // because the fold it is laid on reads them: the black-rock shed and the
    // ice blend are terms of ambient_depth_at, and folding without them would
    // bury Chad's barrens and push the mesh through the lake mirrors. Nothing
    // between here and there touches p.faces except the two failure-path
    // cleanups, which are no-ops on an empty vector.

    // Resample the equirect albedo into a GL cubemap at load, so the shader
    // samples by fragDir (no runtime lat/lon; §2 seam fix). Format to tight
    // RGB8 and hand the raw bytes to the PURE, testable resampler; upload here.
    Image col = LoadImage(color_path);
    if (col.data == nullptr) {
        TraceLog(LOG_WARNING, "PLANET: color load failed: %s", color_path);
        for (Mesh& fm : p.faces) UnloadMesh(fm);
        p.faces.clear();
        return p;
    }
    ImageFormat(&col, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    // Sudbury: the lake landmask rides the cubemap ALPHA (RGBA bake). Load the
    // single-channel mask; if it matches the albedo dims, bake RGBA and gate
    // the water FS branch on. Earth stays RGB (no meaningful alpha).
    std::vector<std::uint8_t> faces;
    int channels = 3;
    if (procedural) {
        char mask_path[512];
        std::snprintf(mask_path, sizeof mask_path, "%s/sudbury_landmask.png",
                      asset_dir);
        Image mask = LoadImage(mask_path);
        if (mask.data != nullptr) {
            ImageFormat(&mask, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
            if (mask.width == col.width && mask.height == col.height) {
                faces = bake_equirect_cubemap_rgba(
                    static_cast<const std::uint8_t*>(col.data),
                    static_cast<const std::uint8_t*>(mask.data), col.width,
                    col.height, u_offset, cubemap_size);
                channels = 4;
                p.has_water = 1.0f;
                // ★ WINTER S2: RETAIN the mask as a CPU raster too. The
                // snowpack function and the surface classifier both need the
                // water FRACTION (INV-8) on the CPU -- ice depth, the LakeIce
                // class, and §3.5's punch-through all key off it -- and
                // re-loading the PNG somewhere else would be a second read of
                // the same asset that could drift in convention. Same buffer,
                // same u_offset, one source.
                p.landmask.w = mask.width;
                p.landmask.h = mask.height;
                p.landmask.u_offset = u_offset;
                p.landmask.px.assign(
                    static_cast<const std::uint8_t*>(mask.data),
                    static_cast<const std::uint8_t*>(mask.data) +
                        static_cast<std::size_t>(mask.width) * mask.height);
            } else {
                TraceLog(
                    LOG_WARNING,
                    "PLANET: landmask dims %dx%d != albedo %dx%d; no water",
                    mask.width, mask.height, col.width, col.height);
            }
            UnloadImage(mask);
        } else {
            TraceLog(LOG_WARNING, "PLANET: landmask load failed: %s",
                     mask_path);
        }
    }
    if (channels == 3) {
        faces = bake_equirect_cubemap(
            static_cast<const std::uint8_t*>(col.data), col.width, col.height,
            u_offset, cubemap_size);
    }
    UnloadImage(col);

    // Own the row-alignment precondition: RGB8 face rows are cubemap_size*3
    // bytes, so a size not a multiple of 4 shears on upload IFF
    // GL_UNPACK_ALIGNMENT is 4. rlLoadTextureCubemap never sets it (works today
    // only because rlglInit left it at 1) — pin it here so any legal size in
    // [1,4096] is safe (red-team P2-2; the S8 "own the precondition, don't rely
    // on a sticky global" lesson). RGBA rows are always 4-aligned; harmless.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const int fmt = channels == 4 ? PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
                                  : PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    const unsigned int cid =
        rlLoadTextureCubemap(faces.data(), cubemap_size, fmt, 1);
    if (cid == 0) {
        TraceLog(LOG_WARNING, "PLANET: cubemap upload failed");
        for (Mesh& fm : p.faces) UnloadMesh(fm);
        p.faces.clear();
        return p;
    }
    // Box-filter mip chain so the planet doesn't minification-alias at the far
    // horizon (a 1024px face subtends few pixels there). Raw GL: bind the
    // cubemap, generate, switch the min filter to trilinear-across-mips.
    int mips = 1;
    for (int s = cubemap_size; s > 1; s >>= 1) ++mips;
    glBindTexture(GL_TEXTURE_CUBE_MAP, cid);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    // Fable-after (M2 P1-1): seamless cube-face filtering. GL 3.3 core defaults
    // to per-face CLAMPED bilinear, which seams the shading along the 12 cube
    // edges (worse on the mip'd normal cubemap at the far limb). Global state,
    // set once; fixes the albedo cubemap too.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    p.cubemap = TextureCubemap{.id = cid,
                               .width = cubemap_size,
                               .height = cubemap_size,
                               .mipmaps = mips,
                               .format = fmt};

    // M2 object-space normal map -> a 2nd cubemap sampled by fragDir (Sudbury
    // only; Earth falls back to the mesh normal via uHasNormalMap=0). Mirrors
    // the albedo cubemap path: same resampler, mip chain, unpack alignment.
    if (procedural) {
        char normal_path[512];
        std::snprintf(normal_path, sizeof normal_path, "%s/sudbury_normal.png",
                      asset_dir);
        Image nrm = LoadImage(normal_path);
        if (nrm.data != nullptr) {
            ImageFormat(&nrm, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
            std::vector<std::uint8_t> nfaces = bake_equirect_cubemap(
                static_cast<const std::uint8_t*>(nrm.data), nrm.width,
                nrm.height, u_offset, cubemap_size);
            UnloadImage(nrm);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            const unsigned int nid =
                rlLoadTextureCubemap(nfaces.data(), cubemap_size,
                                     PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
            if (nid != 0) {
                glBindTexture(GL_TEXTURE_CUBE_MAP, nid);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                p.normalCube =
                    TextureCubemap{.id = nid,
                                   .width = cubemap_size,
                                   .height = cubemap_size,
                                   .mipmaps = mips,
                                   .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8};
                p.has_normal_map = 1.0f;
            } else {
                TraceLog(LOG_WARNING, "PLANET: normal cubemap upload failed");
            }
        } else {
            TraceLog(LOG_WARNING, "PLANET: normal map load failed: %s",
                     normal_path);
        }
    }

    // Debug A/B (smoke only, seam-safe env gate): SEADS_NO_M2 forces the mesh-
    // normal fallback so a shot pair isolates the M2 normal map's contribution.
    if (std::getenv("SEADS_NO_M2")) p.has_normal_map = 0.0f;

    // B5 shock-fabric mask -> a 3rd cubemap (Sudbury only). Absence is not an
    // error: the layer is optional and the feature simply stays off, so an
    // older asset set still runs.
    //
    // ★ ONE READ, TWO CONSUMERS (merge fold). This single decode serves both
    // the RED channel (B5 shock fabric -> the cubemap below, art) and the GREEN
    // channel (§6c.1 `barren` -> a CPU raster, which feeds the SNOWPACK as
    // depth *= 1 - k_barren*barren, physics). The winter merge arrived with its
    // own LoadImage of this same file; that was the "second read of the same
    // asset that could drift in convention" the landmask comment above warns
    // against -- one u_offset, one channel convention, one source. Absent file
    // => empty raster => the shed is off, with no branch anywhere.
    if (procedural) {
        char cone_path[512];
        std::snprintf(cone_path, sizeof cone_path, "%s/sudbury_cones.png",
                      asset_dir);
        Image cim = LoadImage(cone_path);
        if (cim.data != nullptr) {
            ImageFormat(&cim, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
            // G == barren, taken BEFORE the upload and independent of it: the
            // snowpack must get its field even if the GPU cubemap fails.
            {
                const auto* src = static_cast<const std::uint8_t*>(cim.data);
                p.barren.w = cim.width;
                p.barren.h = cim.height;
                p.barren.u_offset = u_offset;
                p.barren.px.resize(static_cast<std::size_t>(cim.width) *
                                   cim.height);
                for (std::size_t i = 0; i < p.barren.px.size(); ++i)
                    p.barren.px[i] = src[3 * i + 1];
                TraceLog(LOG_INFO,
                         "WINTER S2: barren field loaded (%dx%d, G of %s)",
                         p.barren.w, p.barren.h, "sudbury_cones.png");
            }
            std::vector<std::uint8_t> cfaces = bake_equirect_cubemap(
                static_cast<const std::uint8_t*>(cim.data), cim.width,
                cim.height, u_offset, cubemap_size);
            UnloadImage(cim);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            const unsigned int cid =
                rlLoadTextureCubemap(cfaces.data(), cubemap_size,
                                     PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
            if (cid != 0) {
                glBindTexture(GL_TEXTURE_CUBE_MAP, cid);
                glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
                glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR_MIPMAP_LINEAR);
                glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
                p.coneCube =
                    TextureCubemap{.id = cid,
                                   .width = cubemap_size,
                                   .height = cubemap_size,
                                   .mipmaps = mips,
                                   .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8};
                p.cone_detail = 1.0f;
                p.has_fabric = 1.0f;
            } else {
                TraceLog(LOG_WARNING, "PLANET: cone cubemap upload failed");
            }
        } else {
            // Not a warning on either count: the fabric layer is optional, and
            // an absent barren field is a SUPPORTED state, not a degraded one.
            TraceLog(LOG_INFO, "PLANET: no shock-fabric map (%s); "
                               "shatter-cone detail off, barren snow-shed OFF",
                     cone_path);
        }
    }

    // ★ R1 THE FOLD (BLOCK-VP1) -- the drawn surface finally carries the snow.
    //
    // Until this rung the renderer draped everything on the BARE DEM while the
    // sled drove on DEM + snow depth: a ~0.77 m gap in bush, everywhere, so the
    // machine floated above its own world and no amount of local dressing could
    // fix it. The fold is not a new surface -- it is a TERM OF the field the sled
    // already drives, so folding it moves the drawn world toward the driven one.
    //
    // The provider is built HERE, locally, because load_planet owns every input
    // it needs and app/ cannot hand one over: the SnowpackField app/ builds does
    // not exist until after this function returns its height field.
    world::SnowpackField fold;
    fold.p = g_fold_params;
    if (const char* mm = std::getenv("SEADS_MASK_M"))
        fold.p.draw_mask_m = std::atof(mm);  // R2 sweep, see draw.cpp
    if (const char* wd = std::getenv("SEADS_WATER_DILATE_M"))
        fold.p.draw_water_dilate_m = std::atof(wd);
    fold.hf = &p.height;
    if (!p.landmask.empty()) fold.landmask = &p.landmask;
    if (!p.barren.empty()) fold.barren = &p.barren;
    // The corridor mask needs the plow network. sudbury_linework is a pure,
    // statically-cached function of (R, u_offset) -- both known here -- and it
    // reads the SAME baked kSudburyRibbonVerts the GPU draws, so the swale the
    // mesh gets cannot land on a different road than the ribbon does.
    if (g_fold_enabled) fold.lines = &sudbury_linework(R, u_offset);
    const world::SnowpackField* foldp = g_fold_enabled ? &fold : nullptr;

    p.faces.reserve(static_cast<std::size_t>(6) * tiles * tiles);
    for (int i = 0; i < 6; ++i)
        for (int ty = 0; ty < tiles; ++ty)
            for (int tx = 0; tx < tiles; ++tx)
                p.faces.push_back(upload_face(fill_face(
                    p.height, i, subdiv, tx, ty, tiles, cuts,
                    kPlanetCutSplitDepth, foldp)));
    TraceLog(LOG_INFO, "PLANET: snow fold %s",
             g_fold_enabled ? "ON (mesh carries the ambient snowpack)"
                            : "OFF (bare DEM -- pre-R1 behaviour)");
    // The drape reference every road/bank/river must now sit on. Terrain-only
    // facet_radius_at stays untouched for the DRIVE injection (app/main.cpp).
    p.fold_params = g_fold_params;
    p.fold_active = g_fold_enabled;
    if (std::getenv("SEADS_NO_CONES")) p.cone_detail = 0.0f;

    const std::string fs = planet_fs();
    p.shader = LoadShaderFromMemory(kVS, fs.c_str());
    p.loc_sun_dir = GetShaderLocation(p.shader, "sunDir");
    // Aerial-perspective uniforms (the shared kSkyGLSL haze the sky pass
    // shares).
    p.loc_up = GetShaderLocation(p.shader, "uUp");
    p.loc_eye_alt = GetShaderLocation(p.shader, "uEyeAlt");
    p.loc_horizon_elev = GetShaderLocation(p.shader, "uHorizonElev");
    p.loc_ground_day_gain = GetShaderLocation(p.shader, "uGroundDayGain");
    p.loc_reflectivity = GetShaderLocation(p.shader, "uReflectivity");
    p.loc_sparkle = GetShaderLocation(p.shader, "uSparkleSharp");
    p.loc_star_reflect = GetShaderLocation(p.shader, "uStarReflect");
    p.loc_star_density = GetShaderLocation(p.shader, "uStarDensity");
    p.loc_season_snow = GetShaderLocation(p.shader, "uSeasonSnow");
    p.loc_snow_albedo = GetShaderLocation(p.shader, "uSnowAlbedo");
    p.loc_snow_slope_lo = GetShaderLocation(p.shader, "uSnowSlopeLo");
    p.loc_snow_depth_mix = GetShaderLocation(p.shader, "uSnowDepthMix");
    p.loc_barren_shed_keep =
        GetShaderLocation(p.shader, "uBarrenShedKeep");
    p.loc_snow_full_depth = GetShaderLocation(p.shader, "uSnowFullDepth");
    p.loc_ice_albedo = GetShaderLocation(p.shader, "uIceAlbedo");
    p.loc_ice_reflect = GetShaderLocation(p.shader, "uIceReflectFrac");
    p.loc_ice_glint = GetShaderLocation(p.shader, "uIceGlintFrac");
    p.loc_night_glow = GetShaderLocation(p.shader, "uWinterNightGlow");
    p.loc_ice_night_reflect =
        GetShaderLocation(p.shader, "uIceNightReflectFrac");
    p.loc_snow_sparkle = GetShaderLocation(p.shader, "uSnowSparkle");
    p.loc_snow_sparkle_sharp =
        GetShaderLocation(p.shader, "uSnowSparkleSharp");
    // ★ R5 row 8 — sun sparkle + the compaction-suppression arm.
    p.loc_sun_sparkle = GetShaderLocation(p.shader, "uSunSparkle");
    p.loc_sun_sparkle_sharp =
        GetShaderLocation(p.shader, "uSunSparkleSharp");
    p.loc_sparkle_compact_arm =
        GetShaderLocation(p.shader, "uSparkleCompactArm");
    // ★ R5 row 9 — shadow proxy uniforms. The array locations are queried as
    // "uShadowA[0]" (the GLSL-spec-guaranteed spelling for the first element
    // of a uniform array; the bare name is driver-dependent).
    p.loc_shadow_count = GetShaderLocation(p.shader, "uShadowCount");
    p.loc_shadow_a = GetShaderLocation(p.shader, "uShadowA[0]");
    p.loc_shadow_b = GetShaderLocation(p.shader, "uShadowB[0]");
    p.loc_shadow_tint = GetShaderLocation(p.shader, "uShadowTint");
    p.loc_shadow_strength = GetShaderLocation(p.shader, "uShadowStrength");
    p.loc_shadow_tan_sun = GetShaderLocation(p.shader, "uShadowTanSun");
    p.loc_has_water = GetShaderLocation(p.shader, "uHasWater");
    p.loc_force_water = GetShaderLocation(p.shader, "uForceWater");
    p.loc_moon_dir = GetShaderLocation(p.shader, "uMoonDir");
    p.loc_moon_fill = GetShaderLocation(p.shader, "uMoonFill");
    p.loc_moon_sparkle = GetShaderLocation(p.shader, "uMoonSparkle");
    p.loc_spin_axis = GetShaderLocation(p.shader, "uSpinAxis");
    p.loc_aur_oval_c = GetShaderLocation(p.shader, "uAurOvalC");
    p.loc_aur_oval_w = GetShaderLocation(p.shader, "uAurOvalW");
    p.loc_aur_ground_glow = GetShaderLocation(p.shader, "uAurGroundGlow");
    p.loc_aur_tint_lo = GetShaderLocation(p.shader, "uAurTintLo");
    p.loc_sky_space = GetShaderLocation(p.shader, "uSkySpace");
    p.loc_sky_band_top = GetShaderLocation(p.shader, "uSkyBandTop");
    p.loc_sky_day = GetShaderLocation(p.shader, "uSkyDay");
    p.loc_sky_dusk = GetShaderLocation(p.shader, "uSkyDusk");
    p.loc_sky_night = GetShaderLocation(p.shader, "uSkyNight");
    p.loc_dusk_lo = GetShaderLocation(p.shader, "uDuskLo");
    p.loc_dusk_hi = GetShaderLocation(p.shader, "uDuskHi");
    p.loc_night_fill = GetShaderLocation(p.shader, "uNightFill");
    p.loc_dither = GetShaderLocation(p.shader, "uDither");
    p.loc_weather_amt = GetShaderLocation(p.shader, "uWeatherAmt");
    p.loc_haze_scale = GetShaderLocation(p.shader, "uHazeScale");
    p.loc_rayleigh = GetShaderLocation(p.shader, "u_rayleigh");
    p.loc_mie = GetShaderLocation(p.shader, "u_mie");
    p.loc_scatter_strength = GetShaderLocation(p.shader, "u_scatterStrength");
    p.loc_mie_g = GetShaderLocation(p.shader, "u_mieG");
    p.air_locs = air_field_locs(p.shader);
    // LoadShader only auto-locates texture0..2 — the cubemap sampler must be
    // wired by hand so DrawMesh binds MATERIAL_MAP_CUBEMAP (slot 7) to it.
    p.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
        GetShaderLocation(p.shader, "cubemap");
    p.loc_has_normal_map = GetShaderLocation(p.shader, "uHasNormalMap");
    p.loc_vertex_normal_mix =
        GetShaderLocation(p.shader, "uVertexNormalMix");
    p.loc_track_pack_mix = GetShaderLocation(p.shader, "uTrackPackMix");
    p.loc_cone_detail = GetShaderLocation(p.shader, "uConeDetail");
    p.loc_cone_scale = GetShaderLocation(p.shader, "uConeScale");
    p.loc_cone_fade_near = GetShaderLocation(p.shader, "uConeFadeNear");
    p.loc_cone_fade_far = GetShaderLocation(p.shader, "uConeFadeFar");
    p.loc_cone_slope_lo = GetShaderLocation(p.shader, "uConeSlopeLo");
    p.loc_cone_axis = GetShaderLocation(p.shader, "uConeAxis");
    p.loc_has_fabric = GetShaderLocation(p.shader, "uHasFabric");
    p.loc_barren_snow_shed =
        GetShaderLocation(p.shader, "uBarrenSnowShed");
    p.loc_barren_shed_lo = GetShaderLocation(p.shader, "uBarrenShedLo");
    p.loc_barren_shed_hi = GetShaderLocation(p.shader, "uBarrenShedHi");
    p.loc_barren_slope_x_lo = GetShaderLocation(p.shader, "uBarrenSlopeXLo");
    p.loc_barren_slope_x_hi = GetShaderLocation(p.shader, "uBarrenSlopeXHi");
    p.loc_barren_flat_shed = GetShaderLocation(p.shader, "uBarrenFlatShed");
    p.loc_barren_normal_lod = GetShaderLocation(p.shader, "uBarrenNormalLod");
    p.loc_barren_face_dark = GetShaderLocation(p.shader, "uBarrenFaceDark");
    p.loc_barren_face_dark_x_hi =
        GetShaderLocation(p.shader, "uBarrenFaceDarkXHi");
    p.loc_barren_face_mottle =
        GetShaderLocation(p.shader, "uBarrenFaceMottle");

    p.mat = LoadMaterialDefault();
    p.mat.shader = p.shader;
    p.mat.maps[MATERIAL_MAP_CUBEMAP].texture = p.cubemap;
    p.mat.maps[MATERIAL_MAP_CUBEMAP].color = WHITE;
    // The M2 normal cubemap rides MATERIAL_MAP_IRRADIANCE — DrawMesh binds that
    // slot as a CUBEMAP too (like CUBEMAP/PREFILTER), so wiring its sampler loc
    // here lets DrawMesh bind normalCube with no manual GL in the draw loop.
    if (p.normalCube.id != 0) {
        p.shader.locs[SHADER_LOC_MAP_IRRADIANCE] =
            GetShaderLocation(p.shader, "normalCube");
        p.mat.maps[MATERIAL_MAP_IRRADIANCE].texture = p.normalCube;
        p.mat.maps[MATERIAL_MAP_IRRADIANCE].color = WHITE;
    }
    // The shock-fabric cubemap rides MATERIAL_MAP_PREFILTER — the third slot
    // DrawMesh binds as a cubemap, same trick as IRRADIANCE above.
    if (p.coneCube.id != 0) {
        p.shader.locs[SHADER_LOC_MAP_PREFILTER] =
            GetShaderLocation(p.shader, "coneCube");
        p.mat.maps[MATERIAL_MAP_PREFILTER].texture = p.coneCube;
        p.mat.maps[MATERIAL_MAP_PREFILTER].color = WHITE;
    }
    // Cone dials. Lattice cell 2.5 m == cones of roughly 1-2.5 m, the upper end
    // of the Sudbury field range (smaller would be invisible from a cockpit).
    // The fade is short on purpose: this is a detail you meet on a strafing run
    // or a low pass, not a texture the whole map wears.
    {
        const float scale = 1.0f / 2.5f, near_m = 140.0f, far_m = 420.0f;
        // Ground-level riding (whole globe ridable): 0.24 gated cones to
        // faces steeper than ~40 deg, which on rolling Shield is almost
        // nowhere a rider goes. 0.04 opens it to ~16 deg and up. This is
        // MORE correct, not a loosening: the classic Sudbury cone exposure
        // is bald glacially-scoured whaleback rock, which is gently sloped.
        const float slope_lo = 0.04f;
        // ★ FROM world::SnowpackParams, never re-typed here. 0.75 used to be
        // hard-coded at this line AND in world/snowpack.h -- two copies of one
        // law, which is the drift INV-9 exists to forbid. The defaults below
        // mirror the header only so a caller that never sets them still gets
        // the shipped law; app/main.cpp overwrites them from the loaded config.
        const float snow_shed = static_cast<float>(g_shed_k);
        const float shed_lo = static_cast<float>(g_shed_lo);
        const float shed_hi = static_cast<float>(g_shed_hi);
        // The slope gate (Chad's fly ruling 2026-08-11): see set_barren_shed_law.
        const float shed_slope_x_lo = static_cast<float>(g_shed_slope_x_lo);
        const float shed_slope_x_hi = static_cast<float>(g_shed_slope_x_hi);
        const float shed_flat_frac = static_cast<float>(g_shed_flat_frac);
        SetShaderValue(p.shader, p.loc_cone_scale, &scale, SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_cone_fade_near, &near_m,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_cone_fade_far, &far_m,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_cone_slope_lo, &slope_lo,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_cone_detail, &p.cone_detail,
                       SHADER_UNIFORM_FLOAT);
        // Apices point up-range, toward the impact point. The hero direction
        // (+Z, sudbury_geo.D_HERO) is the basin centre on this map, so the fans
        // stay geologically coherent instead of scattering at random.
        const float axis[3] = {0.0f, 0.0f, 1.0f};
        SetShaderValue(p.shader, p.loc_cone_axis, axis, SHADER_UNIFORM_VEC3);
        SetShaderValue(p.shader, p.loc_has_fabric, &p.has_fabric,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_shed_lo, &shed_lo,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_shed_hi, &shed_hi,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_snow_shed, &snow_shed,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_slope_x_lo, &shed_slope_x_lo,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_slope_x_hi, &shed_slope_x_hi,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_flat_shed, &shed_flat_frac,
                       SHADER_UNIFORM_FLOAT);
        // ★ Fly 3: the slope-gate's macro normal is a mip-smoothed
        // normalCube sample. Pick the mip whose footprint is ~90 m on the
        // ground -- the mesh-cell/sim-probe scale -- from this planet's own
        // cubemap geometry (face edge spans a quarter arc, pi/2 * R).
        const float texel_m =
            static_cast<float>((3.14159265358979323846 / 2.0) * R /
                               std::max(1, cubemap_size));
        const float lod = std::max(
            0.0f, std::log2(90.0f / std::max(texel_m, 1e-3f)));
        SetShaderValue(p.shader, p.loc_barren_normal_lod, &lod,
                       SHADER_UNIFORM_FLOAT);
    }

    p.ok = true;
    TraceLog(LOG_INFO,
             "PLANET: built cubesphere %d meshes (6 faces x %d^2 tiles), %d "
             "verts/mesh, effective %d verts/face-edge, %dpx cubemap",
             static_cast<int>(p.faces.size()), tiles, subdiv * subdiv,
             tiles * (subdiv - 1) + 1, cubemap_size);
    return p;
}

void unload_planet(Planet& p) {
    if (!p.ok) return;
    for (Mesh& fm : p.faces) UnloadMesh(fm);
    p.faces.clear();
    UnloadTexture(p.cubemap);  // UnloadTexture(id) deletes the cubemap texture
    if (p.normalCube.id != 0) UnloadTexture(p.normalCube);
    UnloadShader(p.shader);
    // mat uses the shared shader/texture already unloaded; free the struct's
    // default maps array via UnloadMaterial would double-free the texture, so
    // just detach and drop.
    p.ok = false;
}

// ★ SEADS_R3_FULLDEPTH -- THE SATURATION SWEEP SEED (declared in planet.h).
// Chad asked for this on 2026-08-27 after a red team found that
// smoothstep(0, full_depth, vSnowDepth) SATURATES BELOW THE MAP'S MEDIAN DEPTH
// (ship 0.70 m vs a measured census p50 of 0.77 m, config/world.toml:372) --
// over half of all land clipped at full coverage, 0.77 m shading bit-identically
// to 1.75 m. That makes it the prime suspect in his "there is no intermediate".
//
// THE VALUE ACTUALLY DRAWN IS SnowParams::full_depth, which app/main.cpp now
// owns and steps live on PgUp/PgDn -- this function only SEEDS that, so the env
// var still works and the on-screen readout is correct on frame 1. One
// authority, read once. He flew a sweep and could not tell which value he was
// on ("I didnt see what the ladder was at"); an uncontrolled A/B is not a
// measurement, which is why the number is now on screen and not only in a log.
//
// ⚠ ANYTHING <= 0 IS REFUSED, DELIBERATELY. atof turns "", "on", or any typo
// into 0.0, and a saturation depth of 0 saturates the coverage smoothstep
// everywhere and paints the WHOLE PLANET full-snow white. The mix dial can fail
// safe toward the legacy stub; this one cannot fail safe toward zero.
float r3_full_depth_env_override() {
    static const float kEnv = [] {
        const char* e = std::getenv("SEADS_R3_FULLDEPTH");
        if (e == nullptr) return -1.0f;
        const float v = static_cast<float>(std::atof(e));
        if (v <= 0.0f) {
            TraceLog(LOG_WARNING,
                     "PLANET: SEADS_R3_FULLDEPTH=\"%s\" is not a positive depth "
                     "in metres -- IGNORED, using the ship value",
                     e);
            return -1.0f;
        }
        TraceLog(LOG_INFO,
                 "PLANET: R3 saturation depth SEEDED to %.3f m by "
                 "SEADS_R3_FULLDEPTH (PgUp/PgDn steps it live)",
                 static_cast<double>(v));
        return v;
    }();
    return kEnv;
}

void draw_planet_mesh(const Planet& p, const glm::dvec3& eye,
                      const glm::vec3& sun_dir, const glm::vec3& up,
                      float eye_alt, const AtmosphereParams& atm,
                      const glm::vec3& moon_dir, float moon_fill,
                      float moon_sparkle, const AuroraParams& aurora,
                      const SnowParams& snow, const AirField& air,
                      const ShadowSet& shadows) {
    if (!p.ok) return;
    // ★ R5 row 9 — SHADOWS ON SNOW. The count is set EVERY frame,
    // unconditionally (fence 2's real content: never trust a stale uniform
    // from a previous frame) — 0 arms the shader's early-out and the frame is
    // bit-identical to pre-row-9. Unlike uTrackPackMix/uSparkleCompactArm
    // there is deliberately NO per-pass disarm-and-restore: those uniforms
    // reinterpret texcoord.y, which means something different on each mesh,
    // while a sun occluder occludes identically for every surface this
    // program draws — the rider patch and the lake mirror INHERITING these is
    // exactly option (c) working (one shader edit, every receiver, no seam at
    // the patch rim), the same frame-global class as sunDir itself.
    SetShaderValue(p.shader, p.loc_shadow_count, &shadows.count,
                   SHADER_UNIFORM_INT);
    if (shadows.count > 0) {
        SetShaderValueV(p.shader, p.loc_shadow_a, shadows.a,
                        SHADER_UNIFORM_VEC4, shadows.count);
        SetShaderValueV(p.shader, p.loc_shadow_b, shadows.b,
                        SHADER_UNIFORM_VEC4, shadows.count);
        SetShaderValue(p.shader, p.loc_shadow_tint, &shadows.tint,
                       SHADER_UNIFORM_VEC3);
        SetShaderValue(p.shader, p.loc_shadow_strength, &shadows.strength,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_shadow_tan_sun, &shadows.tan_sun,
                       SHADER_UNIFORM_FLOAT);
    }
    float sd[3] = {sun_dir.x, sun_dir.y, sun_dir.z};
    float up3[3] = {up.x, up.y, up.z};
    SetShaderValue(p.shader, p.loc_sun_dir, sd, SHADER_UNIFORM_VEC3);
    const float mdir[3] = {moon_dir.x, moon_dir.y, moon_dir.z};
    SetShaderValue(p.shader, p.loc_moon_dir, mdir, SHADER_UNIFORM_VEC3);
    SetShaderValue(p.shader, p.loc_moon_fill, &moon_fill, SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_moon_sparkle, &moon_sparkle,
                   SHADER_UNIFORM_FLOAT);
    const float sax[3] = {aurora.spin_axis.x, aurora.spin_axis.y,
                          aurora.spin_axis.z};
    SetShaderValue(p.shader, p.loc_spin_axis, sax, SHADER_UNIFORM_VEC3);
    SetShaderValue(p.shader, p.loc_aur_oval_c, &aurora.oval_center_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_aur_oval_w, &aurora.oval_width_rad,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_aur_ground_glow, &aurora.ground_glow,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_aur_tint_lo, aurora.tint_low,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(p.shader, p.loc_up, up3, SHADER_UNIFORM_VEC3);
    SetShaderValue(p.shader, p.loc_eye_alt, &eye_alt, SHADER_UNIFORM_FLOAT);
    // Horizon dip: the limb sits acos(R/|eye|) below level (grows with
    // altitude). The sky rim anchors to this so it stays a thin arc on the limb
    // (uHorizonElev
    // <= 0). Same trivial op the sky pass runs on the same eye — not a fork.
    const float horizon_elev =
        static_cast<float>(-std::acos(std::fmin(1.0, p.R / glm::length(eye))));
    SetShaderValue(p.shader, p.loc_horizon_elev, &horizon_elev,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_ground_day_gain, &atm.ground_day_gain,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_reflectivity, &p.water_reflectivity,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_sparkle, &p.water_sparkle,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_star_reflect, &p.water_star_reflect,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_star_density, &p.water_star_density,
                   SHADER_UNIFORM_FLOAT);
    // W4 seasonal ground (winter snow-cover): a shader tint on the LAND albedo.
    SetShaderValue(p.shader, p.loc_season_snow, &snow.cover,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_snow_albedo, &snow.albedo,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_snow_slope_lo, &snow.slope_lo,
                   SHADER_UNIFORM_FLOAT);
    // ★ R3: the depth-keyed exposure, CLAMPED TO 0 WHENEVER THE MESH CARRIES NO
    // DEPTH CHANNEL -- without this, SEADS_NO_SNOWFOLD (or the Earth) would read
    // the attribute's 0 default and paint the whole planet bare rock, which is a
    // far worse artifact than the stub it replaces.
    //
    // ★ IT KEYS ON p.fold_active, THE PER-PLANET TRUTH RECORDED WHEN THIS
    // PLANET'S MESH WAS BUILT (set beside p.fold_params at build), NOT on the
    // mutable file-static g_fold_enabled. Red team 2026-08-27: the two agree
    // today only because set_planet_snow_fold has exactly one call site, before
    // the lazy ensure_planet. The moment anyone adds the season toggle that
    // app/main.cpp already predicts, a later flip to true would arm mix=1.0
    // against meshes built with EMPTY depths and paint the planet bare -- and
    // the default is now 1.0, so that is planet-wide, not cosmetic. A flag that
    // can change after the mesh is built cannot be the mesh's witness.
    {
        // ★ The A/B, the SEADS_NO_SNOWFOLD / SEADS_NO_M2 precedent: this
        // changes how every square metre of winter ground is EXPOSED, so it
        // must be judgeable against its own absence without a rebuild.
        // SEADS_SNOWDEPTH=0 turns it OFF, =0.5 sweeps it, unset = the ship
        // value in SnowParams, which is 1.0 since Chad ruled it in 2026-08-27.
        // Read once -- getenv per frame on the draw path is not free.
        static const float kEnvMix = [] {
            const char* e = std::getenv("SEADS_SNOWDEPTH");
            if (e == nullptr) return -1.0f;  // unset: defer to SnowParams
            const float v = static_cast<float>(std::atof(e));
            return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        }();
        const float want = kEnvMix >= 0.0f ? kEnvMix : snow.depth_mix;
        const float depth_mix = p.fold_active ? want : 0.0f;
        g_planet_depth_mix = depth_mix;  // what draw_snow_patch restores to
        SetShaderValue(p.shader, p.loc_snow_depth_mix, &depth_mix,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_snow_full_depth, &snow.full_depth,
                       SHADER_UNIFORM_FLOAT);
        // ★ The fence dial (see the shed line). Clamped so no caller can
        // ask for a shed harder than one full application.
        const float keep_raw = snow.barren_shed_keep;
        const float shed_keep =
            keep_raw < 0.0f ? 0.0f : (keep_raw > 1.0f ? 1.0f : keep_raw);
        SetShaderValue(p.shader, p.loc_barren_shed_keep, &shed_keep,
                       SHADER_UNIFORM_FLOAT);
    }
    // S1 lake ice — see SnowParams in planet.h for why ice_albedo is NOT
    // snow_albedo (the shoreline is a survival read, §6b.2 / §3.5).
    SetShaderValue(p.shader, p.loc_ice_albedo, &snow.ice_albedo,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_ice_reflect, &snow.ice_reflect_frac,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_ice_glint, &snow.ice_glint_frac,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_night_glow, &snow.night_glow,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(p.shader, p.loc_ice_night_reflect,
                   &snow.ice_night_reflect_frac, SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_snow_sparkle, &snow.snow_sparkle,
                   SHADER_UNIFORM_FLOAT);
    // ★ FACE-DARK (fly 3): the ramp END, converted degrees -> 1-cos(x) with
    // the SAME convention as world::SnowParams::barren_slope_x_* (the ramp
    // START arrives as uBarrenSlopeXHi from the shed law itself).
    {
        const float fd_x_hi = 1.0f - std::cos(
            snow.barren_face_dark_hi_deg *
            3.14159265358979323846f / 180.0f);
        SetShaderValue(p.shader, p.loc_barren_face_dark,
                       &snow.barren_face_dark, SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_face_dark_x_hi, &fd_x_hi,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_barren_face_mottle,
                       &snow.barren_face_mottle, SHADER_UNIFORM_FLOAT);
    }
    SetShaderValue(p.shader, p.loc_snow_sparkle_sharp,
                   &snow.snow_sparkle_sharp, SHADER_UNIFORM_FLOAT);
    // ★ R5 row 8 — SUN SPARKLE. The dial, the SEADS_TRACK_PACK precedent to
    // the letter: read once (getenv on the draw path is not free), validated
    // range, warn-and-default on bad input, TraceLog the value, 0 kills the
    // row (the A/B baseline). A multiplier on SnowParams::sun_sparkle, whose
    // base anchors to the shipped night-sparkle amplitude — the ladder for
    // Chad's seat sweep is 0 / 0.5 / 1 / 2 / 4.
    {
        const float sun_sparkle = sun_sparkle_dial() * snow.sun_sparkle;
        SetShaderValue(p.shader, p.loc_sun_sparkle, &sun_sparkle,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(p.shader, p.loc_sun_sparkle_sharp,
                       &snow.sun_sparkle_sharp, SHADER_UNIFORM_FLOAT);
    }
    SetShaderValue(p.shader, p.loc_has_water, &p.has_water,
                   SHADER_UNIFORM_FLOAT);
    // Planet pass never forces water (the dedicated lake mesh flips this to 1
    // for its own draw, then back to 0 — render/water_surface.cpp).
    const float force_water_off = 0.0f;
    SetShaderValue(p.shader, p.loc_force_water, &force_water_off,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(p.shader, p.loc_has_normal_map, &p.has_normal_map,
                   SHADER_UNIFORM_FLOAT);
    // SF3-A: the planet pass never mixes in vertex normals. mix(x, y, 0) is
    // exactly x, so this pass renders bit-identically to the pre-SF3-A
    // shader and no golden can move.
    const float planet_vnmix = 0.0f;
    SetShaderValue(p.shader, p.loc_vertex_normal_mix, &planet_vnmix,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 3: the packed-track tint is OFF for the planet pass -- its
    // texcoord.y is ambient snow DEPTH in metres, not compaction, and
    // mix(a, b, 0) == a keeps this pass bit-identical to pre-R5. Set every
    // frame (not trusted to a restore) for the same reason planet_vnmix is.
    const float planet_pack_mix = 0.0f;
    SetShaderValue(p.shader, p.loc_track_pack_mix, &planet_pack_mix,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 8: the compaction-suppression arm is OFF for the planet pass,
    // same dual-role reason as planet_pack_mix above — texcoord.y is DEPTH in
    // metres here, and clamp(0 * x) == 0 leaves the whole planet fully
    // sparkled (the field, not the patch disc, carries the row: a patch-only
    // sparkle would paint a circle around the rider). Set every frame, not
    // trusted to a restore.
    const float planet_sparkle_arm = 0.0f;
    SetShaderValue(p.shader, p.loc_sparkle_compact_arm, &planet_sparkle_arm,
                   SHADER_UNIFORM_FLOAT);
    set_atmosphere_uniforms(
        p.shader, atm, p.loc_sky_space, p.loc_sky_band_top, p.loc_sky_day,
        p.loc_sky_dusk, p.loc_sky_night, p.loc_dusk_lo, p.loc_dusk_hi,
        p.loc_night_fill, p.loc_dither, p.loc_weather_amt, p.loc_haze_scale,
        p.loc_rayleigh, p.loc_mie, p.loc_scatter_strength, p.loc_mie_g);
    set_air_field_uniforms(p.shader, p.air_locs, air);
    Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    for (const Mesh& fm : p.faces) DrawMesh(fm, p.mat, xf);
}


// ★ SF3-A THE RIDER SNOW PATCH -- upload + draw.
//
// The arrays raylib owns must be its own allocation (UnloadMesh frees them),
// so the CPU vectors are copied in rather than aliased. n_side is fixed for
// the life of the mesh; a change re-uploads from scratch.
void snow_patch_gl_upload(SnowPatchGL& g, int n_side,
                          const std::vector<float>& pos,
                          const std::vector<float>& nrm,
                          const std::vector<float>& uv,
                          const std::vector<unsigned short>& idx) {
    const int nv = n_side * n_side;
    if (nv <= 0 || pos.size() != static_cast<std::size_t>(nv) * 3) return;

    if (g.uploaded && g.n_side == n_side) {
        // Stream into the existing VBOs. Buffer ids follow raylib's fixed
        // layout: 0 vertices, 1 texcoords, 2 normals.
        UpdateMeshBuffer(g.mesh, 0, pos.data(),
                         static_cast<int>(pos.size() * sizeof(float)), 0);
        UpdateMeshBuffer(g.mesh, 1, uv.data(),
                         static_cast<int>(uv.size() * sizeof(float)), 0);
        UpdateMeshBuffer(g.mesh, 2, nrm.data(),
                         static_cast<int>(nrm.size() * sizeof(float)), 0);
        return;
    }
    snow_patch_gl_unload(g);

    Mesh m = {};
    m.vertexCount = nv;
    m.triangleCount = static_cast<int>(idx.size() / 3);
    m.vertices = static_cast<float*>(RL_MALLOC(pos.size() * sizeof(float)));
    m.normals = static_cast<float*>(RL_MALLOC(nrm.size() * sizeof(float)));
    m.texcoords = static_cast<float*>(RL_MALLOC(uv.size() * sizeof(float)));
    m.indices = static_cast<unsigned short*>(
        RL_MALLOC(idx.size() * sizeof(unsigned short)));
    std::memcpy(m.vertices, pos.data(), pos.size() * sizeof(float));
    std::memcpy(m.normals, nrm.data(), nrm.size() * sizeof(float));
    std::memcpy(m.texcoords, uv.data(), uv.size() * sizeof(float));
    std::memcpy(m.indices, idx.data(), idx.size() * sizeof(unsigned short));
    UploadMesh(&m, true);  // dynamic: the patch re-streams as the rider moves
    g.mesh = m;
    g.uploaded = true;
    g.n_side = n_side;
}

void snow_patch_gl_unload(SnowPatchGL& g) {
    if (!g.uploaded) return;
    UnloadMesh(g.mesh);
    g.mesh = {};
    g.uploaded = false;
    g.n_side = 0;
}

void draw_snow_patch(const Planet& p, const SnowPatchGL& g,
                     const glm::dvec3& eye) {
    if (!g.uploaded || !p.ok) return;
    // The one uniform that differs from the planet pass: let this mesh's own
    // geometric normals into the light model, faded by the per-vertex rim
    // weight so the patch dissolves into the planet mesh instead of ending.
    const float on = 1.0f;
    SetShaderValue(p.shader, p.loc_vertex_normal_mix, &on,
                   SHADER_UNIFORM_FLOAT);
    // ★ R3: THE PATCH KEEPS THE LEGACY EXPOSURE. It builds its own mesh and
    // writes texcoord .y = 0 (render/snow_patch.cpp), so it carries NO depth
    // channel -- and with the depth-keyed exposure armed, a zero there reads as
    // "no snow" and would paint a bare-ground rectangle on the ground directly
    // under the rider, which is the most visible pixel in the game.
    //
    // Forced to 0 for this draw rather than plumbed with a real depth, because
    // §3.4 of the session handoff retires this whole layer at R4: its job is
    // already done by the fold (R1) and belongs to a path ribbon (R4). Giving a
    // doomed layer a new field dependency buys nothing. Restored below with the
    // normal mix, same reason -- a later pass must not inherit either setting.
    const float patch_depth_mix = 0.0f;
    SetShaderValue(p.shader, p.loc_snow_depth_mix, &patch_depth_mix,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 3 — ARM THE PACKED-TRACK TINT, on this draw only. Forcing the
    // depth mix to 0 above is exactly what frees texcoord.y on this mesh, so
    // the patch carries per-vertex COMPACTION there (render/snow_patch.cpp)
    // and the tint dial reads it. Strength is Chad's seat dial, the
    // SEADS_R3_FULLDEPTH precedent: SEADS_TRACK_PACK, a plain multiplier on
    // the normalized compaction before the sqrt-softened clamp. 0 kills the
    // row (A/B against its own absence). Values > 1 saturate the rut centre
    // and widen the readable skirt. Read once -- getenv on the draw path is
    // not free.
    //
    // â SHIP VALUE 0.45, RULED BY CHAD FROM THE SEAT 2026-08-28. He flew the
    // 0.65 candidate first and asked to come DOWN, then ruled 0.45: "I like
    // it lets move on". The candidate was a shade heavy -- at 0.65 a
    // floor-depth rut runs about two thirds of the way to the full packed
    // colour, which starts reading as a painted stripe rather than snow that
    // has been driven on. 0.45 keeps the smoky band and gives the sqrt skirt
    // more of the total, which is the SOFTNESS he asked for ("a bit o
    // smoke"). Do not raise it back toward 0.65 on the argument that the
    // track would be "more visible" -- more visible is not the goal, and
    // ribbons.h/S1_SPEC RT-1 is the standing warning about exactly that
    // instinct.
    // â  HONEST CAVEAT ON THE RULING: two seads.exe instances were live during
    // that sweep (a stale one outlived a taskkill), and he judged from "the
    // one that is open ... I think it is at 0.45". He was told and ruled
    // anyway. The value is his; the PROVENANCE is one notch weaker than the
    // R3/R4 rulings, which were single-instance. If this ever reads wrong,
    // re-sweep the ladder before suspecting the code.
    static const float pack_mix = [] {
        const char* e = std::getenv("SEADS_TRACK_PACK");
        if (e == nullptr) return 0.45f;
        const float v = static_cast<float>(std::atof(e));
        if (v < 0.0f || v > 4.0f) {
            TraceLog(LOG_WARNING,
                     "PLANET: SEADS_TRACK_PACK=\"%s\" outside [0,4] -- "
                     "IGNORED, using 0.45",
                     e);
            return 0.45f;
        }
        TraceLog(LOG_INFO, "PLANET: track pack tint SEADS_TRACK_PACK=%.2f",
                 static_cast<double>(v));
        return v;
    }();
    SetShaderValue(p.shader, p.loc_track_pack_mix, &pack_mix,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 8 — ARM THE COMPACTION SUPPRESSION, this draw only. This mesh
    // is the one whose texcoord.y is normalized COMPACTION, so the sun
    // sparkle may read it here: compaction 1 kills the glint entirely and the
    // rut reads matte against the glittering field — row 3's contrast without
    // touching row 3's dial. A separate arm from pack_mix ON PURPOSE:
    // SEADS_TRACK_PACK=0 (the row-3 kill switch) must not un-suppress the
    // sparkle, and SEADS_SPARKLE=0 must not touch the tint — two rows, two
    // kill switches, one signal.
    const float sparkle_arm_on = 1.0f;
    SetShaderValue(p.shader, p.loc_sparkle_compact_arm, &sparkle_arm_on,
                   SHADER_UNIFORM_FLOAT);
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    DrawMesh(g.mesh, p.mat, xf);
    // Restore, so a later pass sharing this shader (the lake mirror mesh) is
    // not silently handed the patch's setting.
    const float off = 0.0f;
    SetShaderValue(p.shader, p.loc_vertex_normal_mix, &off,
                   SHADER_UNIFORM_FLOAT);
    // ...and put the depth mix back to whatever the planet pass last used, for
    // the reason the line above exists: the lake mirror mesh shares this
    // program. Inert today (the water branch masks snow exposure by (1-water)
    // before it is read), but leaving a pass to inherit a neighbour's uniform
    // is how the next consumer of this shader inherits a silent wrong value.
    SetShaderValue(p.shader, p.loc_snow_depth_mix, &g_planet_depth_mix,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 3: disarm the packed-track tint the same way, same reason --
    // the lake mirror mesh shares this program and its texcoord.y is NOT
    // compaction. Never leave a later pass to inherit a neighbour's uniform.
    const float pack_off = 0.0f;
    SetShaderValue(p.shader, p.loc_track_pack_mix, &pack_off,
                   SHADER_UNIFORM_FLOAT);
    // ★ R5 row 8: disarm the compaction suppression too, same reason — the
    // lake mirror mesh shares this program and its texcoord.y is not
    // compaction; an inherited arm would suppress sparkle by sqrt(DEPTH).
    const float sparkle_arm_off = 0.0f;
    SetShaderValue(p.shader, p.loc_sparkle_compact_arm, &sparkle_arm_off,
                   SHADER_UNIFORM_FLOAT);
}

}  // namespace render
