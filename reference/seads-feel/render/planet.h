#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "raylib.h"
#include "render/sky.h"  // AtmosphereParams + shared kSkyGLSL aerial perspective
#include "render/sphere_param.h"
#include "world/raster.h"
#include "world/snowpack.h"  // R1: SnowParams, the fold the mesh is laid on  // WINTER S2: the retained CPU landmask (INV-8)

// Textured, DEM-displaced planet (render-only; the physics crash surface is
// STILL the perfect sphere at R — SPEC §6, altitude = length(position) - R).
// All relief lives in the visible mesh; nothing here flows back into sim/.
//
// Geometry is a CUSTOM CUBESPHERE (6 face meshes): uniform texel density and
// NO pole singularity, unlike a UV sphere (equirectangular-on-UV pole pinch).
// raylib's Mesh.indices is unsigned short (<=65536 verts), so each cube face
// is its own mesh; all six share one material/shader.
//
// The color map is resampled at load into a GL CUBEMAP (render::bake_equirect_
// cubemap) and sampled PER-FRAGMENT by the interpolated surface direction
// (texture(cubemap, fragDir)) — NO runtime (u,v) lat/lon anywhere, hardware-
// filtered, pole-free by construction (docs/world_build_plan.md §2 seam fix).
// The old equirect-in-shader path and its lat/long graticule (a rendered fixed-
// frame anchor, the memorized-global-up channel the vision kills) are gone.

namespace render {

struct Planet {
    bool ok = false;
    // Cubesphere face meshes: 6*tiles² (R4d face tiling — each cube face is
    // tiles×tiles sub-meshes so the effective grid can pass the ushort
    // per-mesh index cap; tiles=1 == the legacy 6-mesh planet).
    std::vector<Mesh> faces;
    Material mat = {};  // shared: lighting shader + cubemap albedo
    TextureCubemap cubemap =
        {};  // albedo baked to a GL cubemap (sampled by dir)
    // M2 object-space normal map: a 2nd cubemap sampled by fragDir for crisp
    // slope shading on the coarse mesh (Sudbury only; Earth has none).
    TextureCubemap normalCube = {};
    // B5 shatter cones: the shock-fabric mask (r = 0..1), sampled by fragDir
    // like the albedo/normal cubemaps. Rides MATERIAL_MAP_PREFILTER.
    TextureCubemap coneCube = {};
    int loc_cone_detail = -1, loc_cone_scale = -1;
    int loc_cone_fade_near = -1, loc_cone_fade_far = -1;
    int loc_cone_slope_lo = -1, loc_cone_axis = -1;
    int loc_has_fabric = -1, loc_barren_snow_shed = -1;
    int loc_barren_shed_lo = -1, loc_barren_shed_hi = -1;
    // The slope gate (Chad's fly ruling 2026-08-11): see set_barren_shed_law.
    int loc_barren_slope_x_lo = -1, loc_barren_slope_x_hi = -1;
    int loc_barren_flat_shed = -1;
    // Fly 3: mip-smoothed macro normal + steep-face albedo darkening.
    int loc_barren_normal_lod = -1;
    int loc_barren_face_dark = -1, loc_barren_face_dark_x_hi = -1;
    int loc_barren_face_mottle = -1;
    float cone_detail = 0.0f;   // master strength; 0 = the feature is OFF
    float has_fabric = 0.0f;    // 1 when the shock-fabric cubemap is loaded
    Shader shader = {};
    int loc_sun_dir = -1;
    // Aerial-perspective uniforms (Stage 2: the planet FS calls the shared
    // kSkyGLSL so the ground limb matches the sky at the horizon).
    int loc_up = -1, loc_eye_alt = -1, loc_horizon_elev = -1;
    int loc_ground_day_gain = -1;
    int loc_sky_space = -1, loc_sky_band_top = -1;
    int loc_sky_day = -1, loc_sky_dusk = -1, loc_sky_night = -1;
    int loc_dusk_lo = -1, loc_dusk_hi = -1, loc_night_fill = -1;
    int loc_dither = -1, loc_weather_amt = -1, loc_haze_scale = -1;
    int loc_rayleigh = -1, loc_mie = -1, loc_scatter_strength = -1,
        loc_mie_g = -1;
    AirFieldLocs air_locs;  // S-airdome (spec §1.1)
    // Mirror-silver lakes (Inc 2): the landmask rides the cubemap ALPHA; the
    // water FS branch reflects the shared sky_color. uHasWater gates it OFF for
    // the Earth (RGB) cubemap whose alpha is a meaningless 1.0.
    int loc_reflectivity = -1, loc_sparkle = -1, loc_has_water = -1;
    // Procedural reflected stars in the water (lakes + rivers, single-sourced).
    int loc_star_reflect = -1, loc_star_density = -1;
    // W4 seasonal ground (winter snow-cover).
    int loc_season_snow = -1, loc_snow_albedo = -1, loc_snow_slope_lo = -1;
    int loc_snow_depth_mix = -1, loc_snow_full_depth = -1;  // R3
    int loc_barren_shed_keep = -1;  // R3: the black-rock fence dial
    // S1 lake ice
    int loc_ice_albedo = -1, loc_ice_reflect = -1, loc_ice_glint = -1;
    int loc_night_glow = -1, loc_ice_night_reflect = -1;
    int loc_snow_sparkle = -1, loc_snow_sparkle_sharp = -1;
    int loc_has_normal_map = -1;
    // SF3-A: 0 for the planet pass (bit-identical to pre-SF3-A), 1 for the
    // rider snow patch, whose own geometric normals must reach the light
    // model -- the M2 normal cubemap knows nothing about the snow field.
    int loc_vertex_normal_mix = -1;
    // ★ R5 row 3: packed-track tint strength. 0 for the planet pass (and the
    // lake mirror mesh) -- texcoord.y is ambient DEPTH there, not compaction,
    // and mix(x, y, 0) == x keeps that pass bit-identical. Armed ONLY inside
    // draw_snow_patch (whose mesh carries compaction in texcoord.y) and
    // restored to 0 on the way out, the loc_vertex_normal_mix discipline.
    int loc_track_pack_mix = -1;
    // ★ R5 row 8 — SUN SPARKLE. loc_sun_sparkle/_sharp are the daylight glint
    // amplitude and exponent. loc_sparkle_compact_arm is the ARM for the
    // compaction SUPPRESSION: it follows loc_track_pack_mix's discipline
    // exactly (0 every frame on the planet pass and the lake mirror, where
    // texcoord.y is DEPTH in metres, not compaction; 1 only inside
    // draw_snow_patch; restored to 0 on the way out). It is a SEPARATE arm
    // from the tint dial ON PURPOSE: SEADS_TRACK_PACK=0 must kill the tint
    // without un-suppressing the sparkle in the rut — two rows, two kill
    // switches, ONE compaction signal.
    int loc_sun_sparkle = -1, loc_sun_sparkle_sharp = -1;
    int loc_sparkle_compact_arm = -1;
    // ★ R5 row 9 — SHADOWS ON SNOW (receiver-side analytic occluder proxies).
    // uShadowCount is the shader's EARLY-OUT: 0 casters => the guarded blocks
    // in the FS are never entered and the lit expression is the SHIPPED one
    // verbatim (fence: zero casters => shadow factor exactly 1.0, pinned in
    // test_snow_shadows). Unlike the texcoord.y-role uniforms above these are
    // NOT per-pass armed/disarmed: a caster occludes the sun identically for
    // every surface the program draws (planet mesh, rider patch, lake
    // mirror), which is the whole point of option (c) — so they are set once
    // per frame on the planet pass, like sunDir, and inherited.
    int loc_shadow_count = -1, loc_shadow_a = -1, loc_shadow_b = -1;
    int loc_shadow_tint = -1, loc_shadow_strength = -1;
    int loc_shadow_tan_sun = -1;
    // Dedicated lake mirror-mesh (render/water_surface.*): forces the water FS
    // branch ON for those meshes so they read as water across their FULL
    // outline (incl. the eroded shore band where the cubemap alpha is 0). 0 for
    // the planet pass. The water pass reuses this SAME program so
    // aurora/sky_aerial/ every sky uniform is identical -> the lake cannot fork
    // from the limb (H1).
    int loc_force_water = -1;
    float has_normal_map =
        0.0f;  // 1.0 when the M2 normal cubemap loaded (Sudbury)
    // Moonlight (Stage 5): a 2nd directional light + moonlit-lake glint.
    int loc_moon_dir = -1, loc_moon_fill = -1, loc_moon_sparkle = -1;
    // Aurora ground glow (Stage 8).
    int loc_spin_axis = -1, loc_aur_oval_c = -1, loc_aur_oval_w = -1;
    int loc_aur_ground_glow = -1, loc_aur_tint_lo = -1;
    float water_reflectivity = 0.0f, water_sparkle = 200.0f;
    float water_star_reflect = 0.0f,
          water_star_density = 0.0f;  // reflected-star dials
    float has_water =
        0.0f;  // 1.0 when the cubemap carries a real landmask alpha
    double R = 0.0;
    // The persistent PROCESSED height field (post-blur), the SINGLE SOURCE the
    // mesh was built from and the future props / airstrip ground-contact query
    // read (docs/world_build_plan.md §1/§2). Retained after the raylib DEM
    // buffer is freed — the H1 anti-fork. Populated once the DEM loads; may be
    // non-empty even when !ok (a later color/cubemap failure) — gate on `ok`,
    // not on this field.
    HeightField height;
    // ★ WINTER S2: the lake landmask as a CPU raster, retained alongside the
    // cubemap alpha it was baked into. world::Raster8, so the snowpack function
    // reads the shore FRACTION through the same sampler everything else does
    // (INV-8: `mask > 0` puts a band of half-water at every shore). Empty on
    // the Earth stand-in, which is exactly "no water data" and switches the
    // ice term off by data.
    world::Raster8 landmask;
    // ★ WINTER S2 / §6c.1: the black-rock agent's `barren` field, the GREEN
    // channel of assets/sudbury_cones.png (R = shock fabric, G = barren). It is
    // an INPUT TO OUR SNOW DEPTH via the k_barren shed, so it is loaded here
    // beside the landmask rather than anywhere else -- one read of one asset.
    // Empty when the file is absent (this tree, until sandbox/barrens-layer
    // merges), which switches the shed off BY DATA and needs no branch.
    world::Raster8 barren;

    // R1: what the mesh was actually laid on, retained so the drape helpers can
    // rebuild the identical fold provider without guessing at the dials.
    world::SnowParams fold_params{};
    bool fold_active = false;

};

// Loads color+DEM from asset_dir, builds the displaced cubesphere. relief_scale
// is the metres of radial displacement at full-white DEM (255); ocean (0) stays
// at R. subdiv is vertices-per-edge per face (<=256 for the ushort index cap).
// dem_blur_radius softens sub-quad DEM detail before it reaches the normals
// (coupled to relief_scale; a heavier displacement needs a heavier blur).
// cubemap_size is the per-face edge (texels) the equirect albedo is resampled
// into at load (bake_equirect_cubemap); u_offset is baked in there.
// Returns Planet{ok=false} on any asset/GPU failure (caller falls back).
// u_offset rotates the map in longitude (fraction of 360°) so chosen terrain
// sits under a given surface point — e.g. steep mountains under the spawn
// sub-point for legible relief.
// procedural=true loads the Sudbury real-GIS bake
// (assets/sudbury_{dem,color}.png) instead of the Earth stand-in, and forces
// the runtime DEM blur OFF (the Sudbury DEM is pre-blurred + lakes
// pre-flattened offline; a runtime re-blur would re-bump the flat lake surfaces
// — offline_tool/build_sudbury.py, Fable fix #2). tiles: R4d face tiling — each
// cube face is built as tiles×tiles sub-meshes (effective grid
// tiles*(subdiv-1)+1 verts per face edge) so the render mesh can track
// radius_at past the ushort per-mesh cap. NO default: the compiler enumerates
// every caller (the R3 no-default-arg lesson). cuts: T2 portal surgery — mouth
// cut disks carved into the terrain mesh (any triangle with a vertex inside a
// disk is dropped). Empty (default) => the mesh is bit-identical to the pre-T2
// planet.
Planet load_planet(const char* asset_dir, double R, double relief_scale,
                   int subdiv, int tiles, double u_offset, int dem_blur_radius,
                   int cubemap_size, bool procedural = false,
                   double water_reflectivity = 0.9,
                   double water_sparkle = 200.0,
                   const std::vector<CutDisk>& cuts = {});

void unload_planet(Planet& p);

// ★ Set the barren snow-shed law from config ([snowpack] in world.toml). Call
// once at startup BEFORE the planet builds. Left unset it equals
// world::SnowParams' own defaults, so the shader and world/snowpack.cpp
// agree BY CONSTRUCTION rather than by comment -- the 0.75 that used to be
// typed in both places is gone (INV-9).
// slope_x_lo/slope_x_hi/flat_frac are the SLOPE GATE (Chad's fly ruling
// 2026-08-11: flat barrens hold snow, sloped faces are the blackest) --
// world::SnowpackField::barren_slope_gate's three constants, mirrored in the
// shader off the SAME source.
void set_barren_shed_law(double k, double lo, double hi, double slope_x_lo,
                         double slope_x_hi, double flat_frac);

// ★ R1 (BLOCK-VP1): hand the planet the shipped [snowpack] dials and arm the
// DRAWN FOLD. MUST be called before the planet loads -- the mesh is laid on the
// fold, so arming it afterwards would do nothing. `enabled` false (or
// SEADS_NO_SNOWFOLD in the environment) keeps the pre-R1 bare-DEM mesh.
void set_planet_snow_fold(const world::SnowParams& sp, bool enabled);

// Draws the six faces eye-relative (world re-based to the eye, matching the
// double->float seam in draw.cpp). sun_dir is the world-space light travel
// direction (from the sun toward the scene); up = normalize(eye), eye_alt =
// altitude [m], atm = the [atmosphere] tuning (for the shared kSkyGLSL aerial
// perspective so the ground limb matches the sky at the horizon).
// moon_dir is the world moon LIGHT-TRAVEL direction (moon -> scene, mirroring
// sun_dir); moon_fill is the phase-shaped moonlight gain (ground_gain *
// smoothstep(fill_lo, 1, phase_frac), 0 at new moon); moon_sparkle the lake
// glint exponent.
// W4 seasonal ground snow-cover: the winter whitening of the LAND albedo
// (masked off water, slope-reduced). `cover` is the per-life amount (0 outside
// Winter => identity); `albedo`/`slope_lo` are config look constants. A shader
// tint (no terrain re-bake) so it toggles instantly with the static-season
// override.
// ★ WINTER S1 — LAKE ICE. §6b.2: the lake plane IS the drivable ice, and §3.5
// punches through it, so this is a gameplay read, not decoration. The lakes were
// still a summer MIRROR (sky reflection + sun/moon/star glints) in a permanently
// winter world.
//
// Ice is applied the SAME way snow is — as an ALBEDO change before the lighting
// products — so the terminator, moon fill and aerial are preserved exactly as
// Fable P2-5 required for snow. The mirror is then damped rather than deleted:
// lake ice does keep a weak sheen, and deleting the branch would fork the
// water/river path that shares it.
//
// ★ ice_albedo is deliberately NOT snow_albedo. Telling the LAKE from the SHORE
// is the safety read that §3.5 and §6b.2 both depend on; if ice and land snow
// are the same value the shoreline disappears. See S1_SPEC.md D5.
// ★ R3 SWEEP SEED. Returns the SEADS_R3_FULLDEPTH override in metres, or -1 if
// unset/invalid. app/main.cpp seeds its LIVE (PgUp/PgDn) value from this so the
// on-screen readout is right on frame 1 and the env var keeps working. Logs
// once, on first call. See SnowParams::full_depth for why <= 0 is refused.
float r3_full_depth_env_override();

// ★ ROAD-REPAIR: the SEADS_SPARKLE multiplier on SnowParams::sun_sparkle,
// read once and validated (render/planet.cpp). Named so the SF2 bank strips
// can multiply by the SAME dial -- one dial, two consumers, and the A/B
// kill (SEADS_SPARKLE=0) really does kill every glint in the frame.
float sun_sparkle_dial();

struct SnowParams {
    float cover = 0.0f;      // winter snow amount [0,1] (0 = identity)
    float albedo = 0.92f;    // mono snow target value the land whitens toward
    float slope_lo = 0.55f;  // dot(N, localUp) below which slopes shed snow
    // ★ R3 THE DEPTH-KEYED EXPOSURE DIAL. 0 = the legacy render-only stub (a private
    // slope mask + a private barren shed, both predating the depth field);
    // 1 = coverage read from the ONE analytic field, delivered per-vertex by
    // FaceMesh::depths. Crossfaded, not switched, so he can sweep it.
    //
    // ⚠ FORCED TO 0 when no fold is bound (Earth, procedural, SEADS_NO_SNOWFOLD)
    // -- the attribute would read a default 0 and paint the planet bare. The
    // clamp lives at the draw in planet.cpp, not here, so no caller can
    // accidentally arm it against a mesh that carries no depth channel.
    // ★ SHIPS ARMED. Chad drove it 2026-08-27 with SEADS_SNOWDEPTH=1 and ruled
    // it in: "I saw the shatter cones, the black of the barren rocks are still
    // there, good." That drive WAS the acceptance test for this dial -- the
    // only thing arming exposure could break is his black-rock fence, and his
    // eye says it held (the measurement agreed: sloped rock 0.199 vs the 0.230
    // he had already approved). It also discharged §5's owed cone re-probe.
    //
    // ⚠ What he ruled in is the EXPOSURE, not the look being finished. The same
    // drive reported "I didn't see much difference until I was in the deep
    // snow ... there is no intermediate". A first pass blamed that entirely on
    // the homogenizer ceiling (world/snowpack.h:23, 2.6 cm per 2 m) and wrote
    // "never answered by moving this number" here. THAT WAS WRONG, and a red
    // team caught it -- see full_depth below, which SATURATES BELOW THE MAP'S
    // MEDIAN DEPTH and is a live suspect, and sim/sled.h:551, where the planing
    // law pins sinkage at 0.080 m at EVERY planing depth. Both are real causes
    // this dial does not own. Do not re-write a prohibition here.
    //
    // The A/B survives the flip: SEADS_SNOWDEPTH=0 turns it off without a
    // rebuild, =0.5 sweeps it, unset now means ARMED.
    float depth_mix = 1.0f;
    // Depth at which coverage saturates to full snow. MEASURED, not picked --
    // SEADS_R3_PROBE sweeps it and prints depth-mode coverage against the
    // legacy stub, bucketed by the slope Chad's black-rock fence keys on:
    //
    //   full_depth   open ground   sloped rock >=12   steep rock >=20
    //     (legacy)      0.992           0.230              0.094
    //      0.20 m       0.988           0.373              0.178
    //      0.35 m       0.986           0.307              0.156
    //      0.50 m       0.982           0.250              0.133
    //   >> 0.70 m       0.946           0.199              0.107
    //      0.90 m       0.859           0.163              0.088
    //
    // 0.70 is the shallowest value where the fence does not REGRESS: his
    // sloped black rock comes out at 0.199, blacker than the 0.230 he flew and
    // approved. Below it the mechanism would put MORE snow on the faces he
    // ruled three times should be the blackest thing on the planet. The open
    // ground it costs (0.992 -> 0.946) is not a loss of snow in open country
    // -- it is scoured crests finally reading THIN, which is the entire point
    // of keying exposure to depth instead of to a slope mask.
    //
    // ⚠ It is still a TRADE and the numbers only say it is defensible, not
    // that it is right. His eye rules it.
    //
    // ✅ RULED 2026-08-27. The red team found that this SATURATES BELOW THE MAP'S
    // MEDIAN DEPTH (census p5 0.37 / p50 0.77 / p95 1.14 / p99 1.75 m,
    // config/world.toml:372), so at the old 0.70 ship value more than half of all
    // land was clipped and 0.77 m shaded bit-identically to 1.75 m. That is what
    // put the dial on Chad's stick. He swept it and went the OTHER way, to 0.10 --
    // see the ship-value note below for why that is coherent and not a mistake.
    //
    // ✅ SEADS_R3_FULLDEPTH SWEEPS THIS AT THE DRAW (wired 2026-08-27 on Chad's
    // ask; planet.cpp, beside the SEADS_SNOWDEPTH read). No rebuild, and values
    // <= 0 fall back to this ship value rather than painting the planet white.
    // It was probe-only before, while the drive card claimed otherwise.
    // ★★★ SHIP VALUE 0.30 m -- CHAD DROVE THE LADDER TWICE, 2026-08-27.
    // First call: "0.10 for the win I see intermediate and the whiter tone is
    // better". Then, on the ride that followed: "i like it but the rocks cut
    // kinda straight edged best go to 0.3". He stepped it live on PgUp/PgDn
    // with the value on screen, so both verdicts are ATTRIBUTABLE to a known
    // rung -- unlike his first, unreadable sweep.
    //
    // ★ WHY 0.10 CUT STRAIGHT-EDGED, because it is the same mechanism as the
    // win and the next agent needs both halves. Coverage is
    // smoothstep(0, full_depth, depth): the SMALLER this is, the narrower the
    // depth band over which snow fades to rock. At 0.10 m that band is thinner
    // than the field's own variation across a rock edge, so the transition
    // collapses to a near-binary cut and reads as a straight line drawn on the
    // terrain -- the classic hard-alpha look. 0.30 widens the band ~3x and the
    // edge follows the rock again. It does NOT undo the win: 0.30 still
    // saturates far below the census p50 of 0.77 m, so exposure is still
    // near-flat across ridden ground and the fold's relief still does the work.
    //
    // ⚠ WHY 0.10 WORKS IS NOT WHAT THE SWEEP WAS AIMED AT, and the next agent
    // must not "correct" it back up. At 0.10 essentially ALL snow saturates, so
    // depth-keyed exposure stops varying -- and THAT is the point: the varying
    // exposure was FIGHTING the R1 fold's own relief, washing out the hollows
    // and crests the geometry already carries. Flat exposure lets the SHAPE do
    // the work. The "intermediate" he sees is geometric, not tonal.
    // The red team's saturation finding (below) is what put this rung on the
    // table; the ANSWER it produced was the opposite end of the dial.
    //
    // ⚠ IT COSTS THE FENCE, WHICH IS WHY barren_shed_keep EXISTS. This far below
    // saturation the depth field can no longer express the black-rock shed, so
    // with the shed welded to depth_mix sloped rock measured 0.230 -> 0.431.
    // With the shed on its own dial it comes out at 0.145 -- see below.
    float full_depth = 0.30f;
    // ★★★ THE BLACK-ROCK FENCE DIAL. How much of the legacy barren shed survives
    // while depth-keyed exposure is armed, [0,1]. 0 = the pre-2026-08-27
    // expression exactly (the shed faded out entirely by depth_mix, on the
    // argument that vSnowDepth already carries it); 1 = the shed holds at full
    // strength no matter the mix.
    //
    // SHIPS AT 1.0 because full_depth = 0.10 makes the depth field's own shed
    // invisible, and Chad's blackest-slopes ruling outranks a tidy one-home
    // argument. This dial is the ONLY reason his tone call and his fence call
    // can both be satisfied -- they were welded to one dial and could not.
    // Sweep it with SEADS_R3_SHEDKEEP; SEADS_R3_PROBE models it in the DEPTH
    // column, so the fence stays MEASURABLE rather than argued.
    float barren_shed_keep = 1.0f;
    float ice_albedo = 0.78f;      // frozen-lake albedo (BELOW snow: the shore reads)
    float ice_reflect_frac = 0.18f;  // mirror strength kept under winter [0,1]
    float ice_glint_frac = 0.25f;    // sun/moon/star glint kept under winter [0,1]
    // S1b winter NIGHT (Chad 2026-08-10): snow/ice must lose brightness after
    // dark without losing WHITENESS. night_glow is a neutral lift on snow+ice;
    // ice_night_reflect_frac drops the sky-mirror further so the ice stops
    // taking the black night sky's colour.
    glm::vec3 night_glow{0.70f, 0.75f, 0.84f};  // COOL night light on snow/ice
    float ice_night_reflect_frac = 0.35f;  // extra mirror damp after dark [0,1]
    // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon AND
    // starlight). Night-only per-cell glint on snow/ice; 0 disables it.
    float snow_sparkle = 0.4f;        // night glint amplitude [0,1]
    float snow_sparkle_sharp = 120.0f;  // glint exponent (>= 8)
    // ★ R5 row 8 — SUN SPARKLE (Chad: "1 is the winner"). Daylight crystal
    // glint on UNDISTURBED snow, row 3's partner: the virgin field glitters,
    // the packed rut does not, so the cut reads by CONTRAST without the
    // Chad-vetoed darkening. Base amplitude and exponent deliberately ANCHOR
    // to the shipped night sparkle above (0.4 / 120) — the one glint family
    // already in the game, not an invented curve; SEADS_SPARKLE multiplies
    // the amplitude at the draw (0 kills the row, the A/B baseline).
    float sun_sparkle = 0.4f;         // day glint amplitude (× SEADS_SPARKLE)
    float sun_sparkle_sharp = 120.0f; // day glint exponent (>= 8)
    // ★ FACE-DARK (Chad's fly 3, 2026-08-11): past the shed law's full-shed
    // slope the steepest BARREN faces darken the albedo itself toward
    // void-black ("even blacker so it stands out"). RENDER-ONLY art -- the
    // depth law (INV-9) is untouched, which is why these live here and not
    // in world::SnowParams. The ramp START is the shed law's
    // barren_slope_hi_deg (12), so the two curves chain with no gap.
    float barren_face_dark = 0.95f;        // albedo kill at full ramp [0,1] (fly 4: 0.65 -> 0.85)
    float barren_face_dark_hi_deg = 20.0f; // full face-dark at/above this
    // Fly 5: the baked mottle's bright patches RESIST face-dark by this
    // fraction -- a flat multiply crushes the mottle's absolute range, so
    // the steepest faces read as smooth paint without it.
    float barren_face_mottle = 0.6f;
};

// ★ R5 row 9 — the per-frame occluder proxy set, GPU-ready. Built by
// render/draw.cpp from FrameInfo::shadow_casters: endpoints are EYE-REBASED
// IN DOUBLE there and cast to float (the draw.h double->float seam), so the
// shader's capsule test runs in eye-relative coordinates with O(view
// distance) float error, never O(R). count == 0 is the row's kill state —
// the FS early-outs on it and every guarded term vanishes from the pixel's
// expression (the fence-5 analogue, pinned in test_snow_shadows).
struct ShadowSet {
    int count = 0;                       // live capsules in a/b [0..16]
    glm::vec4 a[16] = {};                // xyz = eye-relative endpoint A,
                                         // w = capsule radius [m]
    glm::vec4 b[16] = {};                // xyz = eye-relative endpoint B
    float strength = 0.0f;               // occlusion at full umbra [0,1]
                                         // ([shadows] strength x the
                                         // SEADS_SHADOWS dial, app-clamped)
    glm::vec3 tint{1.0f, 1.0f, 1.0f};    // full-shadow direct-sun multiplier
                                         // (darker AND bluer — [shadows] tint,
                                         // the ribbons.h ruling)
    float tan_sun = 0.0f;                // tan(sun angular RADIUS) — penumbra
                                         // half-width per metre of slant;
                                         // single-sourced from the SAME
                                         // SunParams.ang_radius_rad the drawn
                                         // disc uses ([celestial] 1.40 deg)
};

// air = the S-airdome AirField (spec §1.9), copied per frame from the live
// sim::AtmosphereField so the ground aerial can never disagree with the sky.
void draw_planet_mesh(const Planet& p, const glm::dvec3& eye,
                      const glm::vec3& sun_dir, const glm::vec3& up,
                      float eye_alt, const AtmosphereParams& atm,
                      const glm::vec3& moon_dir, float moon_fill,
                      float moon_sparkle, const AuroraParams& aurora,
                      const SnowParams& snow, const AirField& air,
                      const ShadowSet& shadows);

// ★ SF3-A THE RIDER SNOW PATCH -- the GPU side. The CPU geometry comes from
// render/snow_patch.h (pure, testable); this owns only the raylib Mesh and
// the draw. It shares the PLANET MATERIAL deliberately: same shader, same
// cubemaps, same atmosphere uniforms, so the patch is lit as ground and
// cannot read as a disc pasted on the world. The only difference in the pass
// is uVertexNormalMix = 1, which lets the patch's own normals reach the
// light model (see the uniform's comment in the FS).
struct SnowPatchGL {
    Mesh mesh = {};
    bool uploaded = false;
    int n_side = 0;
};

// Upload or re-upload. The first call uploads dynamic; later calls with the
// same n_side stream into the existing VBOs.
void snow_patch_gl_upload(SnowPatchGL& g, int n_side,
                          const std::vector<float>& pos,
                          const std::vector<float>& nrm,
                          const std::vector<float>& uv,
                          const std::vector<unsigned short>& idx);
void snow_patch_gl_unload(SnowPatchGL& g);

// Draw AFTER draw_planet_mesh -- it reuses every uniform that pass set, and
// restores uVertexNormalMix to 0 on the way out so any later pass sharing
// this shader is unaffected.
void draw_snow_patch(const Planet& p, const SnowPatchGL& g,
                     const glm::dvec3& eye);

}  // namespace render
