#pragma once

#include <vector>

#include "combat/kill.h"  // FxPool, FxKind — PURE (glm+std, zero raylib)
#include "render/camera.h"
#include "render/flak_model.h"  // FlakDraw: the flak-gun carry
#include "render/plane_legibility.h"  // rung E4: LegibilityParams (visibility)
#include "render/plane_viz.h"  // hero-plane visibility cycle (Feature A)
#include "render/season.h"     // the weather season (W1) — HUD tag + W2/W3 gate
#include "render/sky.h"        // AtmosphereParams + the sky pass (Stage 2)
#include "render/sphere_param.h"   // render::CutDisk (T2 portal surgery)
#include "world/snowhill.h"        // SF1: the snow-mountain scatter mask
#include "world/snowpack.h"        // SF2-BANKS: the bank build samples the field
#include "render/stars.h"          // StarParams + the star pass (Stage 4)
#include "render/vortex.h"         // wingtip vortex trails (MB-7c)
#include "render/wingtip_smoke.h"  // dual wingtip airshow smoke (Feature B)
#include "render/shadow_casters.h"  // R5 row 9: ShadowCaster proxy list
#include "render/sled_plumes.h"    // R5 rows 4/5/6: roost / exhaust / breath
#include "sim/params.h"
#include "sim/state.h"

// ★ R4c: the man on foot. Forward-declared for the same reason SledRig does it
// -- this header is on every render TU's path and he is carried by pointer.
namespace sim {
struct WalkerState;
// ★★★ GAIT LADDER G1: his feet, stepped beside the walker in app/. Same
// forward-declare reasoning as `WalkerState` above.
struct GaitState;
}

// ★ L4: the M-key map's view state (zoom / centre / follow). Same reason as
// the walker above -- carried by pointer, so render/bubble_map.h stays off
// every render TU's include path.
namespace render {
struct MapView;
}
#include "weapon/ballistics.h"  // Projectile, Round — PURE (glm+std, zero raylib)

namespace sim {
struct Environment;  // R6: draw_frame reads it (pointer only) so the HUD
                     // G-tape samples the spatial density the plant flies.
}  // namespace sim

// raylib drawing. Reads state, NEVER writes (SPEC §5) — everything comes in
// by const ref and nothing flows back. This is the double->float seam: the
// world is re-based to the camera eye in DOUBLE first (positions become
// O(chase distance..horizon), not O(R)), then cast — a float cast of raw
// 15 km coordinates would quantize at ~2 mm and shimmer.

namespace world {
struct HeightField;  // world/heightfield.h (R4 — the sim ground query)
struct TunnelNet;    // world/tunnel_net.h (T1 — the greybox geometry source)
struct Raster8;      // world/raster.h (S2 — the fractional landmask, INV-8)
struct LineNetwork;  // world/linework.h (S2 — the promoted surface linework)
}  // namespace world

namespace render {

// One-time GPU-independent setup (clip planes for a 15 km planet — raylib's
// default far plane is 1000 m). Call once after InitWindow.
void init_draw();

// R4 solid ground: the render-built, post-blur/post-lake-flatten persistent
// heightfield — the ONE elevation source (mesh displacement, prop drape, and
// now the sim terrain contact via sim::Environment.ground; the H1 anti-fork).
// Builds the planet on first call (needs a live GL context + the planet build
// params set, exactly like the first draw). Returns nullptr if the planet
// failed to load — the caller leaves Environment.ground null and the world
// stays the bare sphere at R.
const world::HeightField* planet_heightfield(const sim::AircraftParams& params);

// ★ WINTER S2 (WINTER_LAW §2.2/§2.3): the three sources the analytic snowpack
// function needs, handed out from the ONE place they are already loaded.
//   - landmask: the FRACTIONAL water coverage (INV-8) retained from the same
//     buffer that became the cubemap alpha; null on the Earth stand-in.
//   - linework: the promoted road/trail centerlines with widths RECOVERED from
//     the drawn ribbon vertices (INV-6); null when nothing is baked.
// Both build the planet on first call, like planet_heightfield.
const world::Raster8* planet_landmask(const sim::AircraftParams& params);
// The black-rock agent's `barren` (sudbury_cones.png GREEN). An INPUT TO OUR
// SNOW DEPTH via the k_barren shed, not merely a shading term -- INV-9 says
// they supply barren and we own depth, so there is exactly one read of it.
// Null until sandbox/barrens-layer merges, which switches the shed off by data.
const world::Raster8* planet_barren(const sim::AircraftParams& params);
const world::LineNetwork* planet_linework(const sim::AircraftParams& params);

// Cubesphere build parameters, sourced from config/world.toml (no bare geometry
// numbers in render/ — docs/world_build_plan.md §1). Set ONCE at startup via
// set_planet_build_params() BEFORE the first frame builds the planet. The
// defaults reproduce the pre-config planet, so an unset caller (tests/harness,
// which never draw) is unchanged.
struct PlanetBuildParams {
    double relief_scale = 600.0;  // metres of radial displacement at full-white
    int subdiv = 200;             // cubesphere verts per face edge
    int tiles = 1;                // R4d: tiles×tiles sub-meshes per face (1..4)
    double u_offset = 0.806;      // longitude alignment of the map (fraction)
    int dem_blur_radius = 3;      // DEM softening (coupled to relief_scale)
    int cubemap_size = 1024;      // albedo cubemap per-face edge (texels)
    bool procedural =
        false;  // A/B: false = Earth assets, true = Sudbury real-GIS bake
    double water_reflectivity =
        0.9;  // [water] mirror strength of the sky in calm lakes
    double water_sparkle = 200.0;  // [water] sun-sparkle specular exponent
    double water_surface_lift_m =
        2.0;  // [water] dedicated lake mirror-mesh radial lift (m)
    double water_star_reflect =
        0.5;  // [water] reflected procedural-star brightness [0,1]
    double water_star_density =
        220.0;  // [water] reflected-star cell density (cells/unit dir)
    // T2 portal surgery: the mouth cut disks carved into the terrain mesh at
    // load. Empty (the default) => bit-identical terrain (fill_face drops no
    // triangle). Filled by the app from the TunnelNet mouths under the [tunnel]
    // gate (each disk = a mouth dir + kMouthCutFactor*tube_radius).
    std::vector<render::CutDisk> cuts{};
};
void set_planet_build_params(const PlanetBuildParams& p);

// ★ The barren snow-shed law, from [snowpack] in world.toml. Defined in
// render/planet.cpp and defaulted there FROM world::SnowParams, so the
// shader's curve and the sim's depth absorb are one law (INV-9). Declared on
// this header because app/ includes draw.h, not planet.h.
// slope_x_lo/slope_x_hi/flat_frac are the SLOPE GATE (Chad's fly ruling
// 2026-08-11): flat barrens hold snow, sloped faces are the blackest. The
// degree->x conversion happens once, in world::SnowParams -- callers pass
// barren_slope_x_lo()/_x_hi() and barren_flat_shed_frac, never re-derived
// here.
void set_barren_shed_law(double k, double lo, double hi, double slope_x_lo,
                         double slope_x_hi, double flat_frac);

// ★ R1 THE DRAWN FOLD (BLOCK-VP1). Hand the planet the shipped [snowpack] dials
// and arm the fold, so the cubesphere carries the ambient snowpack instead of
// draping on the bare DEM. Same reason as the shed law above for living on this
// header: app/ includes draw.h, not planet.h.
//
// ⚠ MUST be called BEFORE the planet loads -- the mesh is LAID ON the fold, so
// arming it afterwards silently does nothing. `enabled` false, or
// SEADS_NO_SNOWFOLD in the environment, keeps the pre-R1 bare-DEM mesh for the
// A/B.
void set_planet_snow_fold(const world::SnowParams& sp, bool enabled);

// S2 boreal tree scatter (stereoscope-sudbury). Plain POD (like
// PlanetBuildParams) so draw.h stays raylib-free; draw.cpp translates it into
// world::TreeParams + render::PropLook. Set ONCE at startup from
// config/world.toml [trees]. Defaults off (enabled=false) so tests/harness —
// which never draw — are unaffected.
struct TreeBuildParams {
    bool enabled = false;
    double density_gain =
        1.0;  // acceptance = density * gain * jacobian (the fly-dial)
    double min_scale = 0.72, max_scale = 1.5;
    double tree_height_m = 16.0;     // trunk height at scale 1 (mesh + cull)
    double base_width_m = 4.5;       // crown half-spread at scale 1
    double render_range_m = 6000.0;  // hard fade-out distance cap
    double ambient = 0.16,
           diffuse = 0.72;      // mono shading (feeds the silver post)
    double fade_frac = 0.16;    // scale-fade band as a fraction of the reach
    int cells_per_face = 1280;  // grid cells per cube-face edge (~18 m)
    int chunk_cells = 256;      // cells per chunk edge (cull/cache unit)
    unsigned int seed = 0x5EAD5EED;
    // ★ S2b (WINTER_LAW §2.4b): metres of canopy cleared BEYOND the drawn
    // ribbon half-width along every trail and road. Chad flew S2 and found
    // trees standing in the snowmobile trails; §2.4b rules that the corridor
    // must suppress the large-tree scatter, and that the resulting cleared gap
    // IS what makes a woods trail identifiable. Negative disables the mask
    // (bit-identical to the pre-corridor scatter) — the A/B lever.
    double corridor_margin_m = 3.0;
    // T6 scatter exclusion: portal cut disks (same geometry as planet cuts).
    // Empty => bit-identical to the pre-cut scatter.
    std::vector<CutDisk> cuts;
    // ★ SF1 (WINTER_LAW §3.6b): the St. Charles snow-mountain footprint
    // clears the canopy like the corridors do -- a snowform under trees is
    // invisible and undrivable. Same params + heightfield the snowpack
    // drives (value copy + non-owning hf, the snow_field pattern); disabled
    // => bit-identical scatter.
    world::SnowhillParams snowhill;
    const world::HeightField* snowhill_hf = nullptr;
};
void set_tree_build_params(const TreeBuildParams& p);
// Append TREE-ONLY cut disks after the fact (the flak gun-pad clearings --
// they must not reach planet_cuts, which also cuts terrain). Call before the
// first frame builds the scatter (the set_tree_snowhill late-bind precedent).
void add_tree_cuts(const std::vector<CutDisk>& extra);
// SF1: the snowhill mask sources bind LATE (the world HeightField exists only
// after the planet build), at the same site the snowpack binds its sources.
// Must be called before the first frame builds the scatter.
void set_tree_snowhill(const world::SnowhillParams& hill,
                       const world::HeightField* hf);

// ★ SF2-BANKS (WINTER_LAW §3.6c): the oreo snowbanks. Config POD set at load;
// the snow field binds LATE (set_bank_snow_sources, after the planet build, at
// the same site the snowpack binds -- the set_tree_snowhill pattern). NO lift
// dial here: the bank lift is single-sourced from [ribbons] lift_m (F8).
struct BankBuildCfg {
    bool enabled = false;
    double station_m = 16.0;
    double skirt_m = 6.0;
    double skirt_bury_m = 0.5;
    double speckle_density = 0.22;
    double speckle_dark = 0.55;
    double crest_smudge = 1.2;
    double min_amp_m = 0.20;  // clear-the-intersections threshold
    int skirt_rings = 3;        // ★ ROAD-REPAIR: burial-skirt sub-rings
    double chord_tol_m = 0.05;  // ★ ROAD-REPAIR: section chord tolerance (m)
    // ★ ROAD-REPAIR: short stations where the junction gap is fading (0 = off)
    double junction_station_m = 0.0;
    // ★ ROAD-REPAIR ONAPING RUNG 2: the drawn apron (0 = off, the identity).
    double apron_m = 0.0;
    double apron_tol_m = 0.25;
    double apron_min_drop_m = 0.5;
};
void set_bank_build_params(const BankBuildCfg& c);
void set_bank_snow_sources(const world::SnowpackField* snow);

// ★ SF3-A: bind the snow field the RIDER PATCH samples, plus the shipped
// [planet] subdiv/tiles the drawn mesh was built at (so the patch rim can
// meet the same facet the planet interpolates). Same late-binding shape as
// set_bank_snow_sources above, and for the same reason: world/ is
// render-free by law, so app/ is the one layer holding both.
void set_snow_patch_sources(const world::SnowpackField* field, int subdiv,
                            int tiles);

// S3 draped linework ribbons (stereoscope-sudbury). Plain POD (raylib-free,
// like TreeBuildParams) so draw.h stays clean; draw.cpp translates it into a
// render::RibbonLook. Set ONCE at startup from config/world.toml [ribbons].
// Defaults off (enabled=false) so tests/harness — which never draw — are
// unaffected. Colors are RGB [0,1]; roads MONO (the S1 post silvers them), the
// trail is the ONE sanctioned world-chroma (light-green passes the post gate).
struct RibbonBuildParams {
    bool enabled = false;
    double lift_m = 2.0;         // radial lift above the terrain (Fable P1-2)
    // ★ ROAD-REPAIR / ONAPING SINK: the longest CHORD the drape may span
    // between two draped rungs (m). 0.0 == the byte-identical identity.
    double max_seg_m = 0.0;
    // ★ ROAD-REPAIR / ONAPING SINK: the longest chord ACROSS the deck (m).
    // 0.0 == the identity.
    double max_tr_m = 0.0;
    glm::dvec3 road_bed{0.10};   // dark road surface (mono)
    glm::dvec3 road_line{0.80};  // light dashed centerline (mono)
    double road_center_frac =
        0.06;  // |v| < this == the centerline stripe (thin)
    double road_dash_m = 7.0, road_gap_m = 10.0;
    double road_mottle =
        0.14;  // lighter mottled-patch strength (subtle; road stays dark)
    double road_mottle_frac = 0.25;  // fraction of the length that mottles
    double road_fade = 0.10;         // global light fade on the asphalt
    glm::dvec3 trail_color{0.37, 0.27, 0.19};  // dark brown earth trail path
    double trail_mottle = 0.15;  // subtle natural variation on the clay path
    // S1 winter trail: groomed packed snow (darker+bluer than wild snow) +
    // groomer corduroy. See render/ribbons.h RibbonLook for why it is not white.
    glm::dvec3 trail_winter_color{0.80, 0.84, 0.90};
    double trail_corduroy = 0.055;  // groomer cross-line depth [0,1]
    double trail_corduroy_m = 1.6;  // groomer cross-line spacing (m of arc)
    // rivers (kind 3) are drawn as MIRROR water (render/river_surfaces), NOT
    // here.
    // T24 excavation clip: the SAME cut disk list the terrain fill and tree
    // scatter consume (single source — app/main.cpp's planet_cuts), so a
    // draped road/trail can never hover across a cut void. Empty (the
    // default) => bit-identical ribbons.
    std::vector<CutDisk> cuts;
};
void set_ribbon_build_params(const RibbonBuildParams& p);

// S4 building massing (stereoscope-sudbury). Plain POD (raylib-free) so draw.h
// stays clean; draw.cpp translates it into a render::BuildingLook. Set ONCE at
// startup from config/world.toml [buildings]. Defaults off (enabled=false) so
// tests/harness — which never draw — are unaffected. Values are mono luminance
// fractions [0,1]; the town is MONO (the S1 post silvers it, planes stay
// chroma).
struct BuildingBuildParams {
    bool enabled = false;
    double wall_val = 0.42;  // vertical-wall base value (mono)
    double roof_val = 0.62;  // roof/cap base value (mono; lighter than walls)
    double ambient = 0.35;   // flat ambient term
    double diffuse = 0.75;   // N·L sun term gain
    double height_scale = 1.0;  // vertical exaggeration of the baked heights
    glm::dvec3 window_color{1.0, 0.98,
                            0.95};  // night window-light tint (near-white)
    double window_bright = 0.9;     // night window emissive strength (0 = off)
    double window_lit_frac = 0.5;   // fraction of a lit building's windows on
};
void set_building_build_params(const BuildingBuildParams& p);

// Street-lamp night point lights (stereoscope-sudbury). Plain POD (raylib-free)
// -> render::LampLook. Set ONCE at startup from config/world.toml
// [streetlamps]. Defaults off so tests/harness are unaffected.
struct LampBuildParams {
    bool enabled = false;
    glm::dvec3 color{1.0, 0.98, 0.95};  // lamp glow tint (near-white, low-sat)
    double brightness = 0.55;           // soft-halo (aura) additive strength
    double core_bright = 1.0;  // hot-core (bulb) strength — lit up close
    double core_sharp = 34.0;  // core tightness (higher = crisper bulb)
    double size_m = 3.5;       // world glow radius (m) — the up-close size
    double min_px = 2.5;       // far floor: min on-screen radius (px)
};
void set_lamp_build_params(const LampBuildParams& p);

// Precipitation (docs/weather_seasons_plan.md W3). Plain POD (raylib-free) ->
// render::PrecipLook. Set ONCE at startup from config/world.toml [precip].
// Defaults off so tests/harness (which never draw) are unaffected. Season-gated
// (Winter=snow / Spring=rain / else dry) and weather-gated (only under an
// active W2 cell). Mono/silver: snow white, rain grey (no new chroma).
struct PrecipBuildParams {
    bool enabled = false;
    double cell_size_m = 3.0;  // world lattice spacing (flake spacing, m)
    double box_half_m =
        21.0;  // half-extent of the eye-following box (fade radius, m)
    double wrap_fade =
        0.12;  // vertical wrap fade fraction (C0 hides the fall reset)
    glm::dvec3 color{0.90, 0.93, 0.97};  // near-white mono flake/streak tint
    double snow_size_m = 0.28;           // Winter flake radius (m)
    double snow_rate_hz = 0.20;          // Winter fall cycles/s (slow drift)
    double snow_opacity = 0.75;          // Winter alpha at full intensity
    double rain_size_m = 0.05;           // Spring streak half-width (m)
    double rain_streak_m =
        1.30;                    // Spring streak half-length along local_up (m)
    double rain_rate_hz = 1.50;  // Spring fall cycles/s (fast)
    double rain_opacity = 0.55;  // Spring alpha at full intensity (grey read)
};
void set_precip_build_params(const PrecipBuildParams& p);

// CC1 Superstack smoke (Living Copper Cliff, docs/copper_cliff_plan.md). Plain
// POD (raylib-free) -> render::SmokeLook. Set ONCE at startup from
// config/world.toml [smoke]. Defaults off so tests/harness (which never draw)
// are unaffected. The plume is MONO (the S1 post silvers it — NOT the CC3
// chroma exception).
struct SmokeBuildParams {
    bool enabled = false;
    int puffs = 28;
    glm::dvec3 color{0.60, 0.60, 0.62};  // mono grey (low-sat -> silver)
    double rise_m = 900.0;
    double drift_m = 560.0;
    double r0_m = 26.0, r1_m = 155.0;  // puff radius birth -> death (expands)
    double opacity = 0.42;             // peak per-puff alpha
    double jitter_m = 16.0;  // fixed lateral scatter (breaks the line)
    glm::dvec3 wind{1.0, 0.15,
                    0.0};  // world wind hint (projected to the tangent)
};
void set_smoke_build_params(const SmokeBuildParams& p);

// CC2 slag-pot trains (Living Copper Cliff, docs/copper_cliff_plan.md). Plain
// POD (raylib-free) -> render::TrainLook. Set ONCE at startup from
// config/world.toml [train]. Defaults off so tests/harness (which never draw)
// are unaffected. Mono steel (silvered by the S1 post).
struct TrainBuildParams {
    bool enabled = false;
    int pots = 8;              // slag-pot cars behind the loco
    double car_gap_m = 24.0;   // CC5: car spacing = the pour cadence (F8)
    double tip_span_m = 14.0;  // CC5: arc each pot tips over the dump edge
    double lift_m = 1.4;       // drape above the terrain (rail head)
    glm::dvec3 color{0.46, 0.46, 0.48};  // mono steel (low-sat -> silver)
    double ambient = 0.34, diffuse = 0.72;
};
void set_train_build_params(const TrainBuildParams& p);

// CC3 slag pour (Living Copper Cliff, docs/copper_cliff_plan.md). Plain POD
// (raylib-free) -> render::SlagLook. Set ONCE at startup from config/world.toml
// [slag]. Defaults off so tests/harness (which never draw) are unaffected. The
// mound
// + pot are MONO; the lava cascade is the ONE sanctioned WARM-CHROMA exception.
struct SlagBuildParams {
    bool enabled = false;
    double mound_radius_m = 150.0, mound_top_r_m = 62.0, mound_height_m = 45.0;
    double ridge_length_m =
        700.0;  // CC4 ridge crest length (the "proper length" dial)
    double crest_width_m = 24.0;   // CC4 flat top width carrying the track
    double face_angle_deg = 35.0;  // CC4 MEAN pour-face angle of repose
    int benches = 3;  // CC6 terrace steps down the pour face (1=planar)
    glm::dvec3 mound_color{0.10, 0.10, 0.11};  // dark matte slag (mono)
    double ambient = 0.30, diffuse = 0.55;
    int rivers = 3;  // molten streaks down the pour face
    double river_halfwidth_m = 7.0;
    double glow = 1.0;           // emissive lava gain
    double night_boost = 0.7;    // extra emissive when the sun is down
    double pour_rate_hz = 0.05;  // pour cycles per s of t_cel (~one every 20 s)
    double lift_m = 1.5;
};
void set_slag_build_params(const SlagBuildParams& p);

struct TunnelLampWorld;  // render/tunnel_lamp_hits.h (T5c destructible lamps)

// Per-frame HUD context the draw layer can't derive from state alone. The
// physics readouts (V/ALT/G/AoA/bank) are NOT here — draw_frame computes them
// through the shared render::flight_readout so there is one correct-frame
// source (SPEC §12; HARNESS §1 row 5). The nose marker is derived from state.
struct FrameInfo {
    int fps = 0;
    // Monotonic render-frame ordinal, app-owned (the app owns the fixed-dt
    // loop; render/ reads no clock). Sole use: the S1 post-pass animated-grain
    // seed (render::post) — a COSMETIC counter, never a feel/aim signal.
    int frame_count = 0;
    // T25 (fly round-16 tail, Chad: "stuttering a couple of times in the
    // tunnel and also while shooting in the sky") — the STUTTER ATTRIBUTION
    // instrument. APP-OWNED clock (house law: render/ reads no clock): when
    // non-null, draw_frame calls it once after each named pass and the app's
    // profiler records the elapsed lap. nullptr (the default, and always
    // unless SEADS_PROF is set) => zero overhead, bit-identical draw.
    void (*prof_mark)(const char*) = nullptr;
    // ★★★ L3 -- THE APP'S OWN LAST OVERLAY (the spawn menu, app/spawn_menu.h).
    // Same shape and same argument as prof_mark: an APP-OWNED callback that
    // draw_frame merely CALLS, once, after every pass and after the bezel, with
    // the frame still open. render/ knows nothing about what it draws -- the
    // menu is app policy (which vehicle you are born in) and the plan puts it
    // in app/, so the alternative was a second BeginDrawing/EndDrawing pair per
    // frame, which presents the world once WITHOUT the menu and flickers.
    // nullptr (the default, and always except while the menu is open) => zero
    // overhead, bit-identical draw.
    void (*overlay)(void*) = nullptr;
    void* overlay_ctx = nullptr;
    // The active WEATHER SEASON (docs/weather_seasons_plan.md W1). Chosen
    // APP-SIDE at spawn (a random draw, or the config/env static override);
    // render/ only READS it (HUD tag now; W2/W3/W4 precip + tint gate on it).
    render::Season season = render::Season::Summer;
    // R5b: the GravityField (escape ceiling) is LIVE this frame — set by the
    // app from env.grav != nullptr ([gravity] enabled or the T-key toggle).
    // Read by the bezel tag AND the R5c HUD plate under the flaps gauge.
    bool escape_sky = false;
    // R5e (Chad fly-4: "I want no return to be at 4000m at 100m/s"): the
    // tick's ESCAPE CLAIM — E_spec crossed 0 with the field live, and
    // app::tick SEVERED the controls that same tick (the loss is enforced,
    // not predicted — fly-3's dive-recovery is unrepresentable now). The
    // plate mirrors LoopState.escape_claimed, so it can never disagree with
    // the airframe. Red NO RETURN; cleared by respawn or the T rescue.
    bool escape_claimed = false;
    // R6: the AtmosphereField bubble is LIVE this frame (env.atm != nullptr,
    // [atmosphere] enabled or the B-key toggle). Read by the bezel "BUBBLE"
    // tag AND the air-density plate under the flaps gauge. air_frac is the
    // LOCAL air fraction at the plane (0..1: the spatial atm_frac_at, so it
    // encodes both the vertical taper and the bubble edge) — the read-on-sight
    // "am I leaving the breathable air?" cue (the R5 lesson: world-state a
    // pilot must ACT on belongs in a plate, not a subtle bezel line).
    bool bubble_live = false;
    double air_frac = 1.0;
    // Precipitation (W3): the app-owned wrapped fall phase (frac(t_cel*rate),
    // rate season-selected app-side; render/ reads no clock) and the
    // weather-cell intensity at the eye (== the W2 field the haze uses) so
    // precip falls ONLY under an active microsystem and fades as you leave.
    // Both read-only.
    double precip_phase = 0.0;      // wrapped fall phase [0,1)
    double precip_intensity = 0.0;  // weather_cell(eyeDir) [0,1] (0 => dry)
    // Seasonal ground (W4): winter snow-cover applied to the LAND albedo. cover
    // is the per-life amount (0 outside Winter => identity); albedo/slope_lo
    // are config look constants. Consumed as a render::SnowParams at the planet
    // draw.
    float winter_snow = 0.0f;   // snow amount [0,1] (0 = identity)
    float snow_albedo = 0.92f;  // mono snow target the land whitens toward
    float snow_slope_lo =
        0.55f;              // dot(N, localUp) below which slopes shed snow
    float ice_albedo = 0.78f;        // S1 frozen-lake albedo (< snow_albedo)
    float ice_reflect_frac = 0.18f;  // S1 mirror kept under winter
    float ice_glint_frac = 0.25f;    // S1 glint kept under winter
    glm::vec3 night_glow{0.70f, 0.75f, 0.84f};  // S1b COOL night light on snow/ice
    float ice_night_reflect_frac = 0.35f;  // S1b extra mirror damp after dark
    // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon AND
    // starlight). Night-only per-cell glint on snow/ice; 0 disables it.
    float snow_sparkle = 0.4f;          // night glint amplitude [0,1]
    float snow_sparkle_sharp = 120.0f;  // glint exponent (>= 8)
    // Fly 3 (2026-08-11): steepest barren faces darken the albedo itself.
    float barren_face_dark = 0.95f;         // albedo kill at full ramp [0,1] (fly 4: 0.65 -> 0.85)
    float barren_face_dark_hi_deg = 20.0f;  // full face-dark at/above this
    float barren_face_mottle = 0.6f;        // mottle resists face-dark (fly 5)
    // ★ WINTER S2 THE SURFACE READOUT (WINTER_LAW §2.3, the rung's fly
    // deliverable). What surface is under the aircraft's ground track, and how
    // deep the snow is there. Computed APP-SIDE from world::SnowpackField --
    // render/ prints it and evaluates nothing, so the number on screen is the
    // same number the contact patches will read at S3 and cannot become a
    // second opinion about the world. null label => no field (plate hidden).
    const char* surface_label = nullptr;
    double snow_depth_m = 0.0;
    // The NEAREST corridor, named and measured, even when the ground track is
    // not on it. Without this the plate is unreadable from a plane: a road
    // corridor is 12-22 m wide and a trail 5 m, so flying down one is a needle
    // to thread, and "BUSH" alone tells a pilot nothing about whether the
    // linework under him is right. <0 = nothing within corridor_search_m.
    const char* surface_near_label = nullptr;
    double surface_corridor_m = -1.0;
    // ★ THE DEPTH ON THAT CORRIDOR, evaluated at the closest point on its
    // CENTERLINE (LineHit::foot) rather than under the aircraft.
    //
    // Chad's fly-1 finding, and it was an instrument defect rather than a
    // field one: "I couldn't gauge a depth on the trail as it was more
    // difficult to get a reading flying fast over those." A trail is ~5 m wide
    // and the plane crosses it at 150 m/s, so the ONE surface whose depth most
    // needs judging is the one the plate could never hold. Reporting the
    // corridor's own depth removes the need to fly down it at all -- exact, no
    // latch, no held state. Same defect shape as the earlier nearest-corridor
    // NAME fix: an instrument that only reads where you cannot fly reports
    // nothing.
    double surface_corridor_depth_m = 0.0;
    // ★★ WINTER S3 THE SLED DASH (§3's deliverable, and the spec is a
    // subtraction: NO AIRSPEED, NO FLAPS). A sled dash reads ground speed, what
    // is under the machine, and how it is carrying itself -- the aircraft's
    // instruments are meaningless on it and printing them would be the fastest
    // way to make the mode switch feel cosmetic. All values are READ from
    // sim::SledState; render/ evaluates nothing.
    bool sled_active = false;
    double sled_speed_ms = 0.0;
    double sled_plane_frac = 0.0;  // continuous -- never a plow/plane flag
    double sled_roost_flux = 0.0;  // the ONE product S5's roost will scale off
    double sled_depth_m = 0.0;
    double sled_susp_x[3] = {0.0, 0.0, 0.0};  // ski L, ski R, track [m]
    // ★ SUDBURIAN R1a, defect 8. Suspension compression RATE, same patch order,
    // m/s. A pure READ of sim::SledState::susp_v beside susp_x -- nothing under
    // sim/ moved to add it. It is the "a hit is ARRIVING" half of the rider's
    // terrain-reaction channel (render::rider_absorb).
    double sled_susp_v[3] = {0.0, 0.0, 0.0};
    double sled_sink_m[3] = {0.0, 0.0, 0.0};
    const char* sled_surface = nullptr;
    bool sled_rolled = false;
    // A REFUSED mount, stated. null = nothing to say.
    const char* sled_note = nullptr;
    // ★ L1 THE MILLWRIGHT LOOP: the DIEGETIC prompt -- "J  GET ON",
    // "U  FIX PUMP". null = the player is not standing at anything, and that is
    // the whole of "a key near nothing shows nothing". Unlike `sled_note` this
    // is not a wall-clock transient: it is on exactly while he is in reach, so
    // it can never paint over a HUD it does not belong to.
    const char* interact_prompt = nullptr;
    // ★ L2 THE REPAIR CLOCK: 0..1 while the man is actually turning the
    // wrench, NEGATIVE when he is not. A signed sentinel rather than a second
    // bool, because "how far along" and "is he working" are one fact and two
    // fields would eventually disagree about it.
    double repair_frac = -1.0;
    // ★ L10: WHAT he is fixing, for the one caption that names it. A static
    // string the app owns for the frame (the `sled_note` / `interact_prompt`
    // precedent -- render/ never owns this pointer). Defaulted to the pump so
    // an app that never sets it draws exactly what it drew before.
    const char* repair_what = "PUMP";
    // ★★★ L10b (Chad 2026-09-08: "yes make the plate visible afoot too").
    // WHETHER THE ENGINE OUT PLATE MAY BE DRAWN AT ALL -- decided by the APP,
    // which owns the player mode, and merely obeyed here. It replaced a
    // `!info.sled_active` test in render/, which was the R1c mode-leak rule
    // ("a snowmachine has no flaps, so hide the aircraft readouts in drive
    // mode") applied to a plate that is NOT a readout of the thing you are
    // driving: it is a standing fact about your aeroplane, and the man walking
    // up to fix that aeroplane's engine was the one player who could never see
    // it. Defaults TRUE: the fact is true whatever you are sitting on.
    bool show_engine_out = true;
    // ★ SC1 COLD IS POWER (WINTER_LAW §3.7). Pure reads, like every other
    // sled_* field -- app evaluates world::air_temp_c/hardness, render/ only
    // draws. sled_cold_valid gates the whole dash temp readout: an instrument
    // must not read a confident constant when [cold] enabled=false (spec §4,
    // P2-4).
    bool sled_cold_valid = false;
    double sled_air_temp_c = 0.0;
    // SOFT SNOW heads-up (warm daytime): a FIXED dash slot, never sled_note
    // (that channel is a wall-clock transient not gated on sled_active — it
    // would paint the aircraft HUD).
    bool sled_soft_note = false;
    // frost = smoothstep(t_night_c, t_snap_c, t), NO negation (spec §4;
    // v1's `-t` was identically zero). [0,1]; drives the glyph-rim speckle +
    // rising sublimation wisps on the dash text.
    double sled_frost = 0.0;
    // Pose + the geometry the body is drawn from. Passed in rather than
    // recomputed so the visual cannot acquire a second opinion about where the
    // machine is or how far its suspension has travelled (§2.4c.1).
    glm::dvec3 sled_pos{0.0};
    glm::dmat3 sled_basis{1.0};
    double sled_cg_h = 0.0, sled_rest = 0.0, sled_stance = 0.0;
    double sled_ski_fwd = 0.0, sled_track_aft = 0.0;
    // ★ GLTF WIRING RUNG: the hero-sled rig channels -- all straight reads
    // of sim::SledState (steer_actual, rider_lat/fwd/up), the same numbers
    // the kernel integrated this tick (one-number rule, §2.4c.1).
    double sled_steer = 0.0;  // [-1,1], +1 = LEFT
    // ★ R2c-5: the two control inputs the rider's ARMS answer. Straight reads
    // of the sim::SledInputs the kernel stepped with -- the one-number rule
    // again, so the pose cannot disagree with the machine's own throttle.
    // R4a self-right: the signed brace/push channel the leg animation is
    // posed from. [-1,1], + = bracing LEFT so the RIGHT leg shoves. 0 when
    // not righting, and then the signed R3 foot pose is untouched.
    double sled_right_push = 0.0;
    double sled_throttle = 0.0;  // [0,1]
    double sled_brake = 0.0;     // [0,1]
    double sled_rider_lat_m = 0.0;  // + LEFT
    double sled_rider_fwd_m = 0.0;  // + forward
    double sled_rider_up_m = 0.0;   // + standing, - tucked
    // D3.ii weight dot: rider displacement NORMALIZED to the live reach box
    // (the box opens with standing, so app/ computes this where the params
    // live; render/ only draws the dot).
    double sled_weight_x = 0.0, sled_weight_y = 0.0;  // [-1,1] +LEFT/+fwd
    double sled_stand_frac = 0.0;                     // [0,1] seated->stood
    // Drive freelook angles (rad): the rig's HEAD follows the camera (§9d)
    double sled_cam_yaw = 0.0, sled_cam_pitch = 0.0;
    // ★ THE SCARF CHANNEL (docs/SCARF_SPEC.md §4). Three pure reads: the sled's
    // WORLD velocity (draw.cpp rotates it into the body frame with sled_basis
    // -- the rig wants body, and only draw.cpp has both), the fixed sim ticks
    // this frame, and the sim dt. The scarf advances on TICKS, never on wall
    // time, so a --smoke shot is reproducible and no tape can ever see it.
    glm::dvec3 sled_vel_ms{0.0};  // world [m/s], = sim::SledState::velocity
    // *** SK-1c: which presentation of the Sudburian-back HUD is live.
    // true = the X-RAY grid on his back (Chad's primary); false = the
    // bottom-of-screen bar (the A/B fallback he asked be kept). H toggles.
    bool sled_hud_xray = true;
    // ★ R3 SATURATION SWEEP, LIVE. The depth at which snow coverage saturates,
    // in metres; <= 0 means "use the SnowParams ship value". app/main.cpp owns
    // it, seeds it from render::r3_full_depth_env_override(), and steps it on
    // PgUp/PgDn so Chad can A/B the "no intermediate" question from the seat
    // without relaunching. Drawn on screen whenever it is off the ship value --
    // he flew a sweep once and could not tell which rung he was on.
    float r3_full_depth = 0.0f;
    int sled_ticks = 0;           // fixed sim ticks consumed this frame
    // *** SK-1c AUDIT ITEM 1: the CUMULATIVE tick count (app/main.cpp's
    // sled_tick_no, "the tape's only time source"). sled_ticks above is the
    // ticks consumed THIS FRAME -- a constant 2 at steady 60 fps / 120 Hz --
    // so anything that animates must integrate it (the scarf does) or read
    // THIS. A raw sled_ticks phase does not advance, and what little it does
    // is frame-rate DEPENDENT: the opposite of AT-9 discipline.
    long sled_tick_no = 0;        // cumulative sim ticks since drive start
    double sled_dt_s = 0.0;       // the sim dt [s]
    // ★ R4a: the non-inertial frame the scarf (and later the body) hangs in.
    // See render/sled_model.h's SledRig for what each one is for.
    glm::dvec3 sled_omega_body_rps{0.0};  // body [rad/s], = ::angular_vel
    long sled_epoch = 0;  // ++ on every write to `sled` outside step_sled
    // ★★★ R4a THE ARMING RUNG: the three sim::SledParams the rider-load
    // rod model is baked from. app/ has the params; render/ must not keep a
    // copy of them. See render/sled_model.h's SledRig.
    double sled_rider_mass_kg = 0.0;
    double sled_mass_kg = 0.0;
    double sled_cg_height_m = 0.0;
    // ★ R4a INSTRUMENT ONLY (SEADS_BODY_CHAIN=2). The rider-load selector is
    // a FRICTIONLESS PLANAR model: it knows how hard the seat and boards push
    // straight up, and nothing else. So it cannot tell "leaned over on snow"
    // from "inverted in the air" -- Chad's honest drive measured stage 3
    // arming on 77 % of frames tilted 45-90 deg, and PINNING past 90 deg
    // where both of its references degenerate to zero. Deciding between those
    // two needs the one fact the selector lacks: was there ground under him.
    // These are pure reads of SHIPPED sim::SledState -- nothing under `dbg`,
    // no kernel change, and nothing consumes them but the log line.
    double sled_air_s = 0.0;        // ::air_s, seconds since last contact
    // ★ NOW SHIPPED, not instrument: the window `free_frac` is built from.
    // sim::SledComfort::rolled_grace_s -- the kernel's OWN 'how long off the
    // ground before this stops counting as contact'. See rider_load.h.
    double sled_air_grace_s = 0.0;
    double sled_hull_engage = 0.0;  // ::hull_engage_lp, on-side hull load frac
    double sled_susp_sum_m = 0.0;   // sum ::susp_x, per-patch compression [m]
    // ★★★ R4a §7.3 STAGE 4: `sim::SledState::grip.attached`, the one-way latch
    // the kernel flips when hardness x extension beats the capacity. A pure
    // read of shipped kernel state, exactly like `sled_rolled` beside it. TRUE
    // is "he is still holding on", which is every frame before this rung.
    bool sled_grip_attached = true;
    // ★★★ R4c: the man once he is off the machine, stepped by the kernel in
    // app/. Non-owning; null is legal -- see render/sled_model.h's
    // SledRig::walker.
    const sim::WalkerState* sled_walker = nullptr;
    // ★★★ GAIT LADDER G1: his feet, non-owning, stepped beside `sled_walker`
    // in app/ off the same `sim::WalkerState`. NULL is legal -- see
    // `render/sled_model.h`'s `SledRig::gait`.
    const sim::GaitState* sled_gait = nullptr;
    // ★★★ GAIT LADDER G2i: how far off the ground his SHIFT hop has carried
    // him (`sim::HopState::height_m`), and G2j: the pump-repair work pose --
    // whether he is turning the wrench, where in the ratchet cycle he is, and
    // the one-shot hammer blow that settles the machine. All plain values (no
    // pointer, no ownership): the app steps the pure state and hands over the
    // three numbers a pose needs, exactly like `sled_absorb` above.
    double sled_hop_height_m = 0.0;
    bool sled_work_on = false;
    double sled_work_phase = 0.0;
    bool sled_work_blow_on = false;
    double sled_work_blow = 0.0;
    // ★ G2g FOOTSTEPS: app-owned ring of stance-entry pins (world pos on
    // the sampled ground + his heading at the plant), stamped only in snow.
    // Render draws them and holds none of it. NULL / 0 is legal.
    struct Footprint {
        glm::dvec3 pos;  // the pin, ON the sampled ground
        glm::vec3 fwd;   // his heading at the plant (world, unit-ish)
    };
    const Footprint* footprints = nullptr;
    int footprint_n = 0;
    int footprint_head = 0;  // ring head: prints age oldest-first from here
    // (::rolled is already carried above as `sled_rolled` -- reuse it, do not
    //  add a second copy of the same kernel flag.)
    bool raw_mode = false;  // debug raw-stick mode (no instructor, no reticle)
    bool freelook = false;  // reticle nests in the cursor ("aim locked" cue)
    // S-reticle: the DISPLAY-EASED reticle direction (render::reticle_smooth
    // — the true aim plus a hard-capped <= quant_px*sensitivity display lag
    // that hides the integer-mouse staircase). NOT the raw aim: any future
    // consumer needing the TRUE aim must read loop.aim.forward(), never this
    // (this field's only legitimate reader is the reticle projection).
    glm::dvec3 reticle_dir{0.0, 0.0, -1.0};  // world unit; reticle draw only
    // Current vertical FOV [deg] (RMB-zoom eases this from kChaseFovyDeg toward
    // kZoomFovyDeg). draw_frame feeds it to the Camera3D, the off-center lens
    // frustum, AND the reticle/nose/pipper projection — ONE source, so the
    // reticle can never drift from the scene it centers against. The caller
    // computes the matching lens_shift_ndc at this SAME fov.
    double fovy_deg = kChaseFovyDeg;
    // Vertical lens shift (NDC, SPEC §9.2 framing): slides the 3D scene AND the
    // reticle/nose overlay down together so the resting reticle rests at screen
    // center. Computed by the caller via render::lens_shift_ndc; 0 = no shift.
    double lens_shift_ndc = 0.0;
    // Target drones (SPEC §0 S8-drone): the interpolated bandit states, each
    // drawn as an enemy-tinted aircraft (at drone_scale size) in the player's
    // view. The drones never touch the camera (it always frames the player).
    // Empty by default => no drones drawn (the player draw is unchanged).
    std::vector<sim::SimState> drones_draw{};
    double drone_scale = 1.0;
    // CONQUEST no-respawn: parallel to drones_draw (same index/order). 0 = a
    // killed wreck (inert): the draw skips its aircraft + prop so it stops
    // rendering while the index mapping to the gunsight stays intact. EMPTY
    // (the default) => every drone drawn (bit-identical to the pre-conquest
    // path).
    std::vector<char> drones_alive{};
    // Lead-angle gunsight (SPEC §0 S8-drone): the pipper + time-on-target HUD.
    // draw_frame READS the meter snapshot (computed once per sim tick in
    // app::tick, single source) and only PROJECTS + draws it — no render-time
    // solve. gunsight_lead is the world lead direction; on_target => green.
    bool gunsight_active = false;      // draw the gunsight (instructor mode)
    bool gunsight_has_target = false;  // a bandit is engaged (draw the pipper)
    glm::dvec3 gunsight_lead{0.0, 0.0, -1.0};  // world lead dir (where to aim)
    // Lead PIPPER position as a WORLD POINT: the impact point B = engaged
    // bandit's interpolated position + its velocity * time-to-intercept — where
    // the round and the target meet. Drawn by PROJECTING THIS POINT FROM THE
    // EYE (like the bandit markers), NOT the lead DIRECTION at infinity: that
    // makes the diamond sit ahead of the bandit by the true lead with the
    // chase-cam parallax baked in (Fable 2026-07-13: the infinity projection
    // floated it 23-39 mrad high, swinging with bank — the "pipper above the
    // plane" report). RED-cue logic is unchanged (gunsight_on_target,
    // nose-vs-lead_dir cone).
    glm::dvec3 gunsight_lead_point{
        0.0};                         // world impact point B (project from eye)
    bool gunsight_on_target = false;  // nose on the solution + in range (green)
    bool gunsight_out_of_envelope =
        false;               // shot won't connect at this range (red; Task B.3)
    double tot_frac = 0.0;   // time-on-target fraction [0,1] (HUD %)
    double tot_range = 0.0;  // range to the engaged bandit [m]
    double tot_closure = 0.0;  // closure rate [m/s] (+ = closing; Task E)
    // Engaged-target slot (Chad's "distance to target below the target"): the
    // index into drones_draw of the bandit the lead computer is solving, or -1
    // when none engaged. draw_frame projects THIS drone and prints the range
    // (tot_range) just below it in small green — the range readout that pairs
    // with the lead diamond. Parallel to drones_draw (same order as dw.drones).
    int gunsight_target_index = -1;
    // Fixed boresight reticle (Task B, iter-7): the world unit direction of the
    // harmonized gun boresight/sightline — the direction the canted rounds are
    // guaranteed to cross at convergence range. Locked to the airframe
    // (projects to a stable screen point in steady flight; moves only as the
    // aircraft rotates). Derived from orient*(0,0,-1)*conv sightline in
    // app/main.cpp. SINGLE-SOURCE with weapon::harmonization_rise: both use the
    // (0,0,-conv) body-frame sightline transformed by shooter.orientation. Draw
    // only when gunsight_active (instructor mode).
    glm::dvec3 gunsight_boresight{0.0, 0.0,
                                  -1.0};  // world unit dir of gun sightline
    // Fleet Rig mirror finish (fleet_rig_plan.md; /orchestrate rig-A.2). FELT
    // knobs from config/world.toml [fleet_rig], read-only: the env-reflection
    // strength, the Fresnel rim exponent, and the two per-plane chroma colors
    // (player = hero, bandit = crimson). The rig geometry is structural
    // (render/rig.cpp). Defaults reproduce a plausible mirror if unset (tests
    // never draw). RA9-safe: strictly downstream render data.
    double rig_reflectivity = 0.45;
    double rig_fresnel_power = 3.0;
    glm::vec3 rig_player_color{0.20f, 0.55f, 0.95f};
    glm::vec3 rig_bandit_color{0.90f, 0.25f, 0.15f};
    // RUNG E4 VISIBILITY: the [plane_legibility] dials (env-wash fraction, tag
    // halo/size floor, enemy tint shift, engagement glint). The DEFAULTS are
    // the OFF values, so every caller that does not fill this in (tests, the
    // probe, the harness) draws exactly today's frame.
    render::LegibilityParams legibility{};
    // rig-B state-driven surface deflection (render-only, RA9). The COMMANDED
    // control Inputs pose the Fleet Rig's ailerons/elevator/rudder:
    // player_inputs for the player, drone_inputs[i] for each drone (empty or
    // short => that plane draws at rest — e.g. the frozen probe target). Gear
    // and prop are NOT threaded — they read state.gear / state.throttle from
    // the SimState passed to the draw. Deflection MAGNITUDES + prop alpha are
    // the config feel-knobs (degrees at the config edge). Defaults reproduce a
    // plausible rig if unset (tests never draw).
    sim::Inputs player_inputs{};
    std::vector<sim::Inputs> drone_inputs{};
    // R4 ground-roll wheel spin (cosmetic, RA9): the player's accumulated tyre
    // angle [rad], APP-owned and TICK-derived (fr.ticks * sim_dt while
    // on_ground — render reads no clock, the prop-disc discipline). Drones
    // never land; their tyres draw at rest. Default 0 = rest bit-exactly.
    double player_wheel_roll_rad = 0.0;
    double rig_aileron_deg = 18.0;      // aileron throw at full roll
    double rig_elevator_deg = 20.0;     // elevator throw at full pitch
    double rig_rudder_deg = 22.0;       // rudder throw at full yaw
    double rig_gear_deploy_deg = 85.0;  // gear swing, deployed -> in-bay
    double rig_prop_disc_alpha =
        0.30;  // prop blur-disc opacity at full throttle
    double rig_prop_idle_alpha = 0.06;  // prop blur-disc opacity at idle
    // Feature A: hero-plane visibility mode (key 'N' cycles it in main.cpp).
    // Mirror == the original look; NeonRim/Searchlight/Glow/Halo are cosmetic.
    PlaneViz plane_viz = PlaneViz::Mirror;
    // Celestial sun + the [atmosphere] tuning: the sky pass + the planet aerial
    // perspective. sun_dir is the world light-travel direction (sun -> scene),
    // now MOVING (Stage 3a: main.cpp sets it from sun_dir(cel, t_cel) each
    // frame; frozen at the epoch through Stage 2). `sun` carries the disc
    // visual params (angular radius, glare, intensity, finite distance for the
    // eye-relative disc). draw.cpp derives the eye-relative sun direction from
    // sun_dir + sun.distance_m + the eye and feeds the SAME dir to the sky pass
    // AND the planet aerial so the limb cannot fork (plan trap-1).
    glm::dvec3 sun_dir{-0.45, -0.75, -0.48};
    AtmosphereParams atmosphere{};
    // S-airdome (docs/bubble_atmosphere_spec.md §1.9): the render-side mirror
    // of the LIVE sim::AtmosphereField, copied per frame by app/main.cpp
    // (never re-derived) so the sky/ground/star atmosphere can never disagree
    // with the flyable dome. Default enabled=false => air_at() reads 1.0
    // everywhere (full air, spatially uniform — the null-env spatial factor),
    // so an unset caller (tests/smoke) never renders a phantom vacuum.
    AirField air_field{};
    SunParams sun{};
    // Moon (Stage 5): the world moon LIGHT-TRAVEL dir (moon -> scene, mirroring
    // sun_dir), the illuminated fraction (0 new .. 1 full, computed CPU-pure
    // from the celestial core), and the [moon] visual knobs. The moon is
    // treated at infinity (a pure direction). Defaults reproduce a plausible
    // half-moon so an unset caller (tests/smoke) is unchanged.
    glm::dvec3 moon_dir{0.30, 0.80, 0.52};
    double moon_phase_frac = 0.5;
    MoonParams moon{};
    // Aurora (Stage 8): the [aurora] knobs + inertial basis (set once from cel)
    // and the per-frame periodic phase vec2(sin,cos). intensity 0 (default) =>
    // disabled, so an unset caller draws no aurora.
    AuroraParams aurora{};
    glm::vec2 aurora_phase_sc{0.0f, 1.0f};
    // Star field (Stage 4): the [stars] render knobs + the per-frame sky WHEEL
    // (mat3 from sky_wheel(cel, t_cel) — a FINISHED matrix, never a raw large t
    // in render/) + a pointer to the derived CelestialParams (the one-time star
    // mesh build reads its equatorial basis via radec_to_dir). celestial ==
    // nullptr => no stars drawn (tests/smoke that never build cel are
    // unchanged).
    StarParams stars{};
    glm::mat3 sky_wheel{1.0f};
    const CelestialParams* celestial = nullptr;
    // MB-7c energy legibility (read-only, S8 gunsight firewall). All
    // defaulted so an unset caller (smoke/tests) draws the pre-7c frame.
    // AoA dial limits (from controller/aircraft config, set once by the
    // caller): the protection clamp's aoa_max [rad] each side, and the
    // plant's stall alpha Cl_max/Cl_alpha [rad]. aoa_max <= 0 hides the dial.
    double aoa_max = 0.0;      // [rad] protection limit, positive side
    double aoa_max_neg = 0.0;  // [rad] protection limit, negative side (>0)
    double stall_alpha = 0.0;  // [rad] plant stall (red arc start)
    // Speed trend [m/s^2], display-smoothed by the caller (cosmetic — off
    // every control path): the HUD's chevron cue for "gaining/bleeding".
    double speed_trend = 0.0;
    // MB HUD status stack: the COMMANDED devices (main.cpp F-cycle / G-toggle
    // latches), display-only — positions come from state.flap/state.gear (the
    // slewed plant truth). Same display-state precedent as raw_mode/freelook.
    int flap_mode = 0;           // commanded detent: 0 CLEAN/1 COMBAT/2 LANDING
    bool gear_down_cmd = false;  // commanded gear latch
    // Wingtip vortex trails (MB-7c iii), owned/updated by the caller;
    // nullptr = none drawn.
    const VortexTrails* vortices = nullptr;
    // Feature B: dual wingtip airshow smoke trails. nullptr = none drawn
    // (pre-Feature path, tests, smoke). (Feature A's plane_viz member lives
    // in the rig block above — one member, both threads.)
    const WingtipSmokeTrails* wingtip_smoke = nullptr;
    // ★ R5 rows 4/5/6: the sled ROOST / EXHAUST / RIDER-BREATH puff trails.
    // App-owned (app/main.cpp updates them off SledState each render frame,
    // the wingtip_smoke precedent); nullptr = none drawn (flight, tests).
    const SledPlumes* sled_plumes = nullptr;
    // ★ R5 row 9 — SHADOWS ON SNOW: the CASTER PROXY LIST (world-space
    // capsules, render/shadow_casters.h), built APP-SIDE each frame from
    // measured geometry (aircraft: aircraft_node_specs box_dims; sled:
    // SledParams) and ALTITUDE-GATED there against the drive surface (a
    // caster in the tunnel network must not shadow the ground over its
    // head). READ-ONLY downstream: draw.cpp rebases the endpoints to the eye
    // and uploads them to the planet shader's proxy array — nothing here
    // feeds input, control, or sim. EMPTY (the default, and the
    // SEADS_SHADOWS=0 kill) => the shader's caster-count early-out =>
    // every pixel bit-identical to pre-row-9. Adding a caster class later
    // (enemy aircraft, Sudburians) is appending to this vector — data, not
    // surgery.
    std::vector<render::ShadowCaster> shadow_casters{};
    // The [shadows] look dials (config/world.toml), scaled by the
    // SEADS_SHADOWS dial app-side. strength = occlusion of the DIRECT-SUN
    // term at full umbra [0,1]; tint = the full-shadow multiplier on that
    // term (darker AND bluer than neutral — the ribbons.h:37-47 ruling; the
    // uNightFill ambient survives inside the shadow by construction, so a
    // snow shadow reads sky-lit, never black). Defaults draw the shipped
    // frame for every caller that never fills them in.
    float shadow_strength = 0.0f;
    glm::vec3 shadow_tint{1.0f, 1.0f, 1.0f};
    // Tracer streaks (rig-D guns). Const view of the app-owned GunWorld pool;
    // nullptr = no tracers drawn (pre-guns path, tests, raw mode). The render
    // pass is READ-ONLY: no clock, no writes, no sim/control touch (firewall
    // §5). tracer_lifetime_s / tracer_len_m are config-sourced (no bare numbers
    // in render/); colors by kind (cannon warm-white / MG cooler).
    const std::vector<weapon::Projectile>* projectiles = nullptr;
    // ENEMY tracers (bandit combat AI): const view of the app-owned enemy pool
    // (combat::CombatWorld::enemy_pool); nullptr = none drawn. Same comet pass
    // as the player battery but forced to the hostile tint (tracer_enemy_rgb)
    // so incoming fire reads as a distinct threat. READ-ONLY, clock-free
    // (firewall).
    const std::vector<weapon::Projectile>* enemy_projectiles = nullptr;
    // RUNG S3-GUNS — COSMETIC tracers: const view of
    // combat::CombatWorld::cosmetic_pool, the rounds spawned to give the
    // abstracted AI-vs-AI and pump-raid damage a visible source. nullptr =
    // none drawn. Same comet pass again, but tinted PER ROUND off
    // weapon::Projectile::friendly rather than forced hostile — this pool
    // carries ALLIED AI-vs-AI fire too, and allied fire that reads as incoming
    // is a bug on screen. THIS POINTER IS THE POOL'S ONLY READER OUTSIDE
    // combat::cosmetic_fire_tick: render is read-only and can deal no damage,
    // which is half of the cosmetic guarantee (combat/kill.h has the other).
    const std::vector<weapon::Projectile>* cosmetic_projectiles = nullptr;
    // Combat FX (kill-loop): const view of the app-owned FxPool; nullptr = none
    // drawn. age-driven, additive, clock-free.
    const std::vector<combat::Fx>* combat_fx = nullptr;
    // R4-FLY-6 touchdown burst scale ([fx] touchdown_intensity, config-
    // sourced like the tracer knobs — no bare feel numbers in render/).
    // Scales count/size/brightness of FxKind::Touchdown together.
    double touchdown_fx_intensity = 1.0;
    // R4-FLY-7 thumb-binding diagnosis: bitmask of raylib mouse buttons
    // (0..7) held this frame, sampled by the caller. The HUD prints the held
    // codes so a driver's thumb-button mapping is read off the screen and
    // set in [input] game.toml — never guessed again. 0 = nothing drawn.
    int mouse_buttons_held = 0;
    // Kill count + flash (kill-loop HUD).
    long long kill_count = 0;
    long long ticks_since_kill = 1 << 30;
    // Player stakes HUD (bandit combat AI): the health bar + "DOWNED" flash.
    // The bar draws only when hp_max > 0 (a combat session); ticks_since_death
    // drives the respawn flash (clock-free, tick-derived, like
    // ticks_since_kill).
    double player_hp = 0.0;
    double player_hp_max = 0.0;  // 0 => no HP bar drawn (no combat)
    long long death_count = 0;
    long long ticks_since_death = 1 << 30;  // saturating; 0 on a death tick
    // CONQUEST overlay (Scarce Skies MASTER_PLAN §4; placeholder-grade). The
    // app reads combat::ConquestState once per frame into these; draw_frame
    // draws a small status stack + a VICTORY/DEFEAT banner and the pump world
    // markers. conquest_active gates the WHOLE overlay — false (the default)
    // draws nothing new (bit-identical). All display-only (no writes).
    bool conquest_active = false;
    int conquest_planes_left = 8;
    int conquest_score_valley = 0;   // score[world::VALLEY]
    int conquest_score_sudbury = 0;  // score[world::SUDBURY]
    int conquest_pumps_alive = 0;    // of the four pumps
    int conquest_pumps_total = 4;
    // ★★★ L6 (Chad's ruling R9, 2026-09-03): the match clock has been
    // initiated, so neither side gets another aeroplane. Drawn beside the pump
    // count, because the pump count is WHY. false = nothing new drawn.
    bool conquest_respawn_locked = false;
    int conquest_outcome = 0;  // 0 PLAYING, 1 VICTORY, 2 DEFEAT
    // Pump world markers (VALLEY = cool, SUDBURY = warm, near-white per the
    // mono rule). A surface pump draws a TALL beacon column (visible from km
    // away); a dead pump draws dark/extinguished. Empty => no markers.
    struct ConquestPump {
        glm::dvec3 pos{0.0};
        int faction = 0;  // 0 VALLEY (cool), 1 SUDBURY (warm)
        bool alive = true;
        bool surface = true;   // surface pumps get the tall beacon column
        double hp_frac = 1.0;  // hp/max_hp — the near-pump HUD readout (fly-3:
                               // a 15 s HP budget with no fraction visible
                               // read as "indestructible")
        // The terrain point under a surface pump (== pos for a deep pump).
        // The BODY is drawn standing here; `pos` stays the logical point the
        // raiders fly at. App-filled (app/spawn_policy.h's mast law). LAST,
        // defaulted, so the positional aggregate init in main.cpp holds.
        glm::dvec3 foot{0.0};
    };
    std::vector<ConquestPump> conquest_pumps{};
    // COMPETITIVE rung (enemy pump raids): true while an enemy raider is
    // on-station over the player-faction surface pump -> a flashing "PUMP UNDER
    // ATTACK" warning + a blinking map marker. conquest_raided_pump is that
    // pump's index into conquest_pumps (-1 = none). Default off = no warning.
    bool conquest_pump_under_attack = false;
    int conquest_raided_pump = -1;
    // FLAK (docs/FLAK_GUN_SPEC.md §7 F-PLACE/F-LOAD): the ground guns, one
    // per SURFACE pump on its threat flank, app-placed once at conquest
    // init and copied here each frame like conquest_pumps. Empty => nothing
    // drawn (bit-identical off-arm).
    std::vector<FlakDraw> flak_guns{};
    // FLAK F-FIRE/F-SIGHT: the manned gun's pool (tracers, cannon tint),
    // drum readout, and the solved lead point for the sight pipper. All
    // read-only views/copies; defaults draw nothing.
    const std::vector<weapon::Projectile>* flak_projectiles = nullptr;
    // STAGE D: the AI-manned guns' pools. One entry per AI gun; drawn through
    // the SAME draw_pool with the SAME flak tracer look, so their tracer
    // streams are indistinguishable from the player's (that is the point --
    // the valley is defended whether or not he is on a gun). Empty => the
    // loop body never runs.
    std::vector<const std::vector<weapon::Projectile>*> flak_ai_projectiles{};
    bool flak_manned = false;
    int flak_manned_gun = -1;  // index into flak_guns; suppresses its signal
    // F-POSE: true when the camera is OUTSIDE the gunner -- the free-look
    // pullout in play, or a SEADS_FLAKCAM smoke rig. It gates the close
    // near plane (only the tight sight view needs it) and nothing else.
    bool flak_cam_external = false;
    // F-POSE: how solidly to draw the man, 0 = not at all. Carries the
    // pullout's fade-in so he cannot pop into existence inside the lens;
    // down the sight it is 0 (Chad's 2026-08-31 ruling: a clean sight
    // picture, no body). Written by the flak camera block.
    float flak_gunner_alpha = 0.0f;
    int flak_rounds_left = 0;
    double flak_reload_left_s = 0.0;
    // ★ STING RPAS (the sting lane, 2026-09-03): the P-key FPV interceptor.
    // sting_flying gates the world draw (primitive-built until the GLB rung)
    // and the flight HUD; sting_shouldered gates the launcher crosshair +
    // prompt. All read-only copies; zero defaults draw nothing, so a session
    // that never presses P is bit-identical.
    bool sting_flying = false;
    sim::SimState sting_draw{};     // the drone, for the world draw
    // The Sting's carried aim (its own AimFrame's forward) — drawn as the
    // SAME neon reticle circle + red nose crosshair pair the aeroplane flies
    // by, because it runs the same cascade (Chad's fly ruling 2026-09-03).
    glm::dvec3 sting_aim_dir{0.0, 0.0, -1.0};
    bool sting_shouldered = false;  // launcher up -> crosshair
    // ★★★ ST-5 PHASE D, THE SEAT DEPLOY. The app-stepped blend the launcher
    // pose interpolates on: 0 = stowed beside the tunnel, 1 = stock on his
    // shoulder, ~3 s between them (render::sting::deploy_step). RAW, not
    // eased -- the ease is applied at the pose so the duration stays a
    // measurable fact. Default 0 draws nothing, so a session that never
    // presses P is bit-identical.
    float sting_deploy = 0.f;
    // ★★★ ST-5 POLISH (Chad 2026-09-05: "the sting propellors need to spin
    // when flying"). The four props' SPIN ANGLE [rad], accumulated app-side on
    // clamped_dt off the shared rate law (render/sting_audio.h's
    // sting_spin_rate / sting_rail_spin_rate) and wrapped mod 2pi there so a
    // 60 s flight cannot grind float precision away. ONE field for both draw
    // sites (the flying drone and the one riding the launcher rail).
    //
    // ⚠ APP-OWNED BY LAW, not by taste: this banner's own rule is that render
    // reads no wall clock, so a phase derived from GetTime() inside draw.cpp
    // would also drift out of step with the smoke rig's fixed-dt frames and
    // make the spin uncertifiable by screenshot. Default 0 = the rest pose,
    // bit-identical to the pre-polish frame.
    float sting_prop_phase = 0.f;
    // ★★★ POLISH FLY-2 (Chad: the spin must "visibly vary" yet stay
    // perceivable, blurring rather than strobing). TWO fields now, because a
    // prop draw needs two different facts:
    //
    //   sting_prop_phase   WHERE the blades are -- and it is the APPARENT
    //                      angle, accumulated app-side at the soft-knee rate
    //                      (render::sting_apparent_spin_rate). Capping the
    //                      RATE at the accumulator, rather than scaling the
    //                      phase at the draw, is what keeps the angle
    //                      continuous: a phase multiplied by a rate-dependent
    //                      factor jumps backwards the instant the throttle
    //                      moves.
    //   sting_prop_rate    HOW FAST they are really turning [rad/s], which is
    //                      what the blur disc's opacity ramps on. It cannot be
    //                      recovered from the phase (that only carries the
    //                      clamped rate), so it ships as its own number.
    //
    // Default 0 = still blades, no disc: bit-identical to the pre-polish frame.
    float sting_prop_rate = 0.f;
    // ★★★ POLISH FLY-2 (Chad, verbatim): "I am looking right into the body of
    // the sudburian". THE CLOSE NEAR PLANE, and it is a ROOT CAUSE not a
    // garnish: the 3D pass runs a 2 m near plane and the shouldered launcher
    // views sit 1.5-1.6 m off the man, so the plane was slicing his torso open
    // and showing its inside. True whenever a shouldered-launcher camera owns
    // the frame (the over-shoulder aim view AND the new third-person freelook
    // orbit); render swaps in the flak sight's already-certified 0.15 m plane
    // for exactly those frames, the same trade F-SIGHT made for the same
    // reason. Default false = the 2 m plane, untouched.
    bool sting_close_cam = false;
    // ★★★ ST-5: is he shouldering it FROM THE SEAT. This is a mode fact and
    // it arrives FROM the app (the draw.h banner law -- render must not know
    // PlayerMode), filled from the same `seated` branch of main.cpp's
    // `sting_stance` lambda that decides the LAUNCH ORIGIN. Pre-authorized by
    // name in the game-loop lane's packet §2.
    //
    // ⚠ IT CANNOT BE DERIVED HERE, and the obvious derivation is wrong:
    // `sled_active` is true whenever a sled is SEEDED, afoot included, and
    // `sled_grip_attached` is a CRASH latch, not a mode. Deriving it would
    // let the launcher pose and the launch origin disagree about where the
    // man is -- "the missile is born out of his hands" is the packet's own
    // phrasing of that failure.
    bool sting_seated = false;
    int sting_left = 0;             // launches remaining (Chad: 3 per match)
    double sting_battery01 = 0.0;   // battery fraction [0,1]
    // fly-3 ("I didnt notice if I had all that hud to inform me of speed and
    // altitude"): the drone's own SPD/ALT readout, app-computed (altitude is
    // length(pos) - R and render/ holds no R of its own).
    double sting_speed_mps = 0.0;
    double sting_alt_m = 0.0;
    // fly-5: the DRONE's own local air fraction (the plane's AIR plate reads
    // the parked aeroplane's position and lies for a drone at the bubble
    // edge). Drawn as its own boxed plate in the sting HUD stack, gated on
    // bubble_live like the plane's.
    double sting_air_frac = 1.0;
    int sting_turns_left = 0;       // large turns left before the destruct
    double sting_warn_s = -1.0;     // >= 0 -> destruct countdown showing
    glm::dvec3 flak_lead_point{0.0};
    bool flak_lead_valid = false;
    // Hit/kill confirmation envelopes (Chad 2026-08-29), app-decayed off the
    // PLAYER-ONLY cw.hits/cw.kills deltas while manned; 0 = nothing drawn.
    double flak_hitmark = 0.0;
    double flak_killmark = 0.0;
    // FLAK STAGE B (the immersion ladder 2/4) -- ALL COSMETIC. The two
    // envelopes are app-stepped from the undrained spawned_accum (see the
    // frame-order contract in render/flak_gun.h) and read-only here;
    // flak_shot_seq is the deterministic FX phase (a shot count, never a
    // clock). Zero defaults draw nothing, so an unmanned/idle gun is
    // bit-identical.
    double flak_flash = 0.0;   // muzzle-flash envelope
    double flak_blast = 0.0;   // snow-blast intensity
    int flak_shot_seq = 0;     // rounds fired: the blast's boil phase
    // STAGE C: the spent-brass ring, app-owned and app-stepped (main.cpp);
    // read-only here. nullptr or an empty pool => not one cylinder is drawn.
    const flak::BrassPool* flak_brass = nullptr;
    // The flak's OWN tracer look (config [guns] flak_tracer_*): a 20 mm
    // ground gun walking fire into the sky reads longer and hotter than the
    // fighter's rounds. Aircraft tracers are untouched by these.
    glm::vec3 flak_tracer_rgb{1.0f, 0.38f, 0.05f};  // red-orange
    float flak_tracer_base = 1.55f;      // width multiplier vs the plane's
    float flak_tracer_len_mult = 2.1f;   // streak length = tracer_len_m * this
    float flak_tracer_lum_mult = 1.30f;  // brighter core/glow/head
    // Nearest ALIVE pump within engagement eyeshot (app-computed): its
    // hp/max_hp, or -1.0 when no alive pump is near. Drawn as a "PUMP nn%"
    // HUD line so sustained fire visibly bites (fly-3). Read-only.
    double conquest_near_pump_frac = -1.0;
    // Per-drone faction (world::VALLEY=0 / world::SUDBURY=1), PARALLEL to
    // drones_draw/drones_alive (same index/order — combat::maverick_faction
    // (drone spawn_index), single-source with the conquest score bookkeeping).
    // Empty => the M-KEY MAP draws mavericks unfactioned (neutral tint); a
    // conquest session always populates this 1:1 with drones_draw.
    std::vector<int> drones_faction{};
    // game-AI-R4 (red-team P1-1): per-drone HOSTILITY for the 3D tag —
    // single-sourced from DroneState::friendly_side (the same flag the round
    // tag uses), NOT raw faction, so the furball toggle (everyone hostile)
    // never paints a hunting plane cyan. Empty => every drone tags BANDIT
    // (the legacy non-conquest pass, bit-identical).
    std::vector<char> drones_friendly{};
    // conquest_player_faction: which side the PLAYER flies for this session
    // (world::VALLEY=0 / world::SUDBURY=1) — used only to tint the player
    // marker / decide "enemy vs friendly" maverick tint on the M-KEY map.
    int conquest_player_faction = 0;
    // LIVE per-faction bubble growth (combat::ConquestState::radius_scale,
    // indexed world::VALLEY / world::SUDBURY). 2026-07-26 FIX: the M-KEY map
    // used to draw both ovals straight off the BAKED world::k*RadiusM
    // constants, so the drawn bubble could never move no matter what happened
    // to the pumps -- exactly Chad's "I destroyed enemy pumps but their bubble
    // will not shrink down". draw_frame now feeds these through
    // world::faction_ellipse (the SAME growth-scaled + clamped geometry
    // build_faction_bubbles emits), so the drawn outline is the air edge.
    // {1,1} = ungrown, which reproduces the old baked drawing exactly.
    double conquest_radius_scale[2] = {1.0, 1.0};
    // SUDDEN-DEATH MATCH CLOCK (Chad, 2026-07-26). countdown_faction < 0 =
    // disarmed => the readout is not drawn at all. When armed it is the faction
    // that lost its bubble and must win before countdown_s hits zero; the HUD
    // says whether that is YOU or THEM, because the same clock means opposite
    // things to the two sides.
    int conquest_countdown_faction = -1;
    double conquest_countdown_s = 0.0;
    // ★ L2 (Chad, 2026-09-01: "fix the surface pumps to STOP THE GAME
    // CLOCK"). The clock is still ARMED and still shown -- it is HELD, not
    // disarmed, and it resumes from these very seconds if the pump is lost
    // again -- so the readout stays and gains a legend beside it.
    bool conquest_countdown_paused = false;
    // ★ L6: the clock POOL exists (combat::countdown_active). The readout is
    // gated on this rather than on countdown_faction >= 0, because a HELD clock
    // has no target and must still show its seconds -- those seconds are what a
    // later re-point spends.
    bool conquest_countdown_armed = false;
    // THE DEATHMATCH (Chad, 2026-08-30): every pump on the map is dead, so the
    // clock is null, the bubbles are gone, and it is last plane standing with
    // points breaking a tie. Drawn in the clock's own slot so the readout never
    // goes silent at the exact moment the rules change. false = nothing drawn.
    bool conquest_deathmatch = false;
    // M-KEY FULL-SCREEN BUBBLE MAP (Chad's ask, 2026-07-25): a top-down
    // aeqd overlay of the two faction ellipses + the vacuum gap, the player,
    // the 4 pumps, the 2 tunnel mouths, and the mavericks. Pure DISPLAY — the
    // 3D world keeps rendering underneath (this is drawn as a HUD overlay
    // pass, never a mode swap), flight input stays live. map_open false (the
    // default) draws nothing new — bit-identical to every pre-map frame.
    bool map_open = false;
    // ★★★ L4 — THE MAP'S VIEW AND ITS ACTIVE BODY
    // (docs/PLAN_20260901_game_loop_millwright.md §4.4).
    //
    // `map_view` is a NON-OWNING pointer at app state that persists across
    // open/close (the zoom you left the chart at is the zoom you come back
    // to). NULL is the pre-L4 chart exactly: fit, centred, no follow. Held by
    // pointer and forward-declared above so `render/bubble_map.h` does not
    // land on every render TU's include path for one struct.
    const MapView* map_view = nullptr;
    // ★ THE ACTIVE BODY — the thing the player IS, which after L1 is not
    // always the aeroplane. The player pointer is drawn here and every
    // objective range/bearing is measured FROM here. `map_body_valid` false =
    // fall back to `state`, which is what every frame before L4 drew, so a
    // caller that forwards nothing gets the old chart.
    //
    // ⚠ IT IS A POSITION AND A FORWARD, NOT A MODE. draw_frame is not allowed
    // to know what `app::PlayerMode` is (render never includes app/), and a
    // transcribed copy of the mode enum here would be the second authority
    // this repo keeps paying for. The app resolves the mode to a body; the map
    // draws the body.
    bool map_body_valid = false;
    glm::dvec3 map_body_pos{0.0};  // world position of the active body
    glm::dvec3 map_body_fwd{0.0};  // world forward of it (need not be unit)
    const char* map_body_tag = nullptr;  // "AIRCRAFT"/"SNOWMACHINE"/"ON FOOT"
    // The bodies he is NOT in get a glyph, so "where did I leave the machine"
    // is a map question and not a memory question. False = draw nothing.
    bool map_sled_glyph = false;
    glm::dvec3 map_sled_pos{0.0};
    bool map_aircraft_glyph = false;
    glm::dvec3 map_aircraft_pos{0.0};
    // The Sting in flight (Chad 2026-09-04: "my sting needs an indicator as
    // it moves across terrain in the actual map"). Drawn whenever the drone
    // is airborne, whatever body the keys are on. Forward = its velocity.
    bool map_sting_glyph = false;
    glm::dvec3 map_sting_pos{0.0};
    glm::dvec3 map_sting_fwd{0.0};
    // Component damage panel (damage_model_plan.md): per-component health [0,1]
    // (engine/pilot/wings/structure). Drawn as small labelled bars beside the
    // HP summary when player_hp_max > 0. 1.0 all round = a pristine plane.
    double dmg_engine = 1.0;
    double dmg_pilot = 1.0;
    double dmg_wing_left = 1.0;
    double dmg_wing_right = 1.0;
    double dmg_structure = 1.0;
    double tracer_lifetime_s = 0.0;  // [s] max age to draw (config [guns])
    double tracer_len_m = 8.0;       // [m] streak length back along velocity
    glm::vec3 tracer_cannon_rgb{1.0f, 0.30f,
                                0.02f};           // 20mm orange/red (iter-7)
    glm::vec3 tracer_mg_rgb{1.0f, 0.55f, 0.04f};  // 7.92mm brighter orange
    // ENEMY tracer tint (bandit combat AI): a venomous yellow-green, clearly
    // NOT the player's warm orange, so incoming rounds read as hostile at a
    // glance.
    glm::vec3 tracer_enemy_rgb{0.75f, 1.0f, 0.25f};
    // Tracer comet look dials (iter-7 Task E: no bare numbers in render/).
    float tracer_cannon_base = 1.0f;     // 20mm width multiplier
    float tracer_mg_base = 0.72f;        // 7.92mm width multiplier (narrower)
    float tracer_enemy_base = 0.85f;     // enemy round width multiplier
    float tracer_r_core_frac = 0.0011f;  // core radius = frac * view_dist
    float tracer_r_core_min = 0.05f;     // minimum core radius [m]
    float tracer_r_glow_mult = 3.0f;     // glow radius = mult * r_core
    float tracer_glow_lum = 0.5f;        // glow layer luminance fraction
    float tracer_glow_alpha = 0.45f;     // glow additive alpha
    float tracer_core_lum = 1.0f;        // core layer luminance
    float tracer_core_alpha = 0.95f;     // core additive alpha
    float tracer_head_r_mult = 1.5f;     // head radius = mult * r_core
    float tracer_head_lum = 1.5f;        // head luminance (white-hot)
    float tracer_head_alpha = 1.0f;      // head alpha
    float tracer_slag_dark = 0.75f;  // slag fleck darkness [0=none, 1=black]
    float tracer_slag_period_m =
        1.8f;  // world-space repeat of dark slag chunks [m]
    float tracer_glow_len_mult =
        1.7f;  // glow tail length = tracer_len_m * mult
    // CC1 Superstack smoke phase (Living Copper Cliff): the FINISHED wrapped
    // puff phase frac(t_cel*smoke_rate) in [0,1), computed in DOUBLE by the app
    // — render/ never sees raw t_cel (house law; Fable P1: a float32 t_cel
    // stutters at ~10 h). The whole plume derives from this one scalar. Default
    // 0 is a valid frame.
    double smoke_phase = 0.0;
    // CC2 slag-pot train phase (Living Copper Cliff): the FINISHED wrapped
    // loco-head phase frac(t_cel*train_rate) in [0,1) (loco at s = phase*L),
    // computed in DOUBLE by the app — render/ never sees raw t_cel. Default 0
    // is a valid frame.
    double train_phase = 0.0;
    // CC3 slag pour phase (Living Copper Cliff): the FINISHED wrapped pour
    // phase frac(t_cel*pour_rate) in [0,1) (pot tip + lava front derive from
    // it), computed in DOUBLE by the app — render/ never sees raw t_cel.
    // Default 0 is valid.
    double pour_phase = 0.0;
    // S-cues (comfort program, [comfort] in controller.toml): two peripheral
    // orientation cues, pure HUD, DEFAULT OFF. alpha 0 => the draw is SKIPPED
    // entirely (strict superset — the shipped default frame is unchanged).
    // draw_frame reads local_up = normalize(state.position) and the bank from
    // the shared flight_readout, both fresh, never cached (SPEC §6.1).
    double cue_horizon_alpha = 0.0;     // [0..1] ghost-horizon opacity; 0 = OFF
    double cue_horizon_gap_frac = 0.3;  // [0..1) center exclusion disk radius
    double cue_bank_arc_alpha = 0.0;    // [0..1] bank-arc opacity; 0 = OFF
    // S-carets (comfort program, REC-2): screen-edge threat indicators for
    // OFF-SCREEN drones. alpha 0 => the caret pass is SKIPPED entirely (strict
    // superset — shipped default frame unchanged). Drawn for drones in
    // drones_draw not visible in the frustum; the engaged pipper target (which
    // has its own diamond) is skipped. Pure HUD read of drones_draw + pose.
    double cue_caret_alpha = 0.0;  // [0..1] edge-caret opacity; 0 = OFF
    // REC-6: the STYLIZED COCKPIT FRAME — a screen-anchored peripheral canopy
    // interior (the steady-state REST FRAME, docs/comfort_research.md §3.3).
    // alpha 0 => the draw is SKIPPED entirely (strict superset). Cosmetic HUD,
    // off every aim/camera/control path. Ships 0.35 (Chad wants to SEE it).
    double cue_cockpit_alpha = 0.35;  // [0..1] cockpit-frame opacity; 0 = OFF
    // T2 Errington tunnel greybox: the T1 net the interior meshes are built
    // FROM (single-source with the collision volume). Set by the app under the
    // SAME gate as env.tunnels ([tunnel] enabled). nullptr => no tunnel draw
    // (the lazy build never fires) => the shipped default frame is unchanged.
    const world::TunnelNet* tunnel_net = nullptr;
    // T5c destructible gaslamps: the app-owned lamp world (positions + per-lamp
    // alive state + a dirty flag). nullptr => the renderer places its own lamps
    // (all alive, legacy T4b path). When present, draw_tunnel_lamps rebuilds
    // its additive-glow buffers from the ALIVE lamps whenever a shot flips
    // `dirty`.
    render::TunnelLampWorld* tunnel_lamps = nullptr;
    // NOTE (kernel-v5 reconcile): flight-kernel-v5 carried its own copy of the
    // Fleet Rig knobs (rig_reflectivity/rig_fresnel_power/rig_*_color,
    // player_inputs/drone_inputs/player_wheel_roll_rad, rig_aileron_deg/
    // rig_elevator_deg/rig_rudder_deg/rig_gear_deploy_deg/
    // rig_prop_disc_alpha/rig_prop_idle_alpha) — this tree's FrameInfo
    // (above, ~line 377) already defines every one of them byte-identically
    // (this tree's own rig-B/rig-D work), so v5's copy was dropped as a
    // duplicate rather than grafted.

    // F9 RECORD (v5 kernel-v5-reconcile): app-owned recording state, pure HUD
    // read — draw_frame never touches seads_replay::Recorder itself. `now_s`
    // is GetTime()'s already-sampled value (main.cpp reads no extra clock on
    // render's behalf; draw_frame itself calls GetTime() elsewhere in this
    // file for the aim-buddy tremble, so reading it again here is the same
    // discipline, not a new violation).
    bool recording = false;  // true while F9 is capturing this frame
    // The GetTime() timestamp the last save completed at; -1e18 = no toast
    // pending. draw_frame fades the "SAVED ..." text out over 3 s from here.
    double rec_saved_at_s = -1e18;
    char rec_saved_name[64] = {0};  // e.g. "felt_flight_3.seadsrec"

    // ★ LIGHT TUNE (Chad, 2026-08-17: "which button do I press to turn off
    // frost? And lighting tuning?"). A DEBUG readout so the three measured
    // causes of the light flood can be dialled BY EYE in-game and the settled
    // number copied into config/world.toml by hand. Pure app-owned display
    // state -- render/ prints it and evaluates nothing; the dial VALUES are
    // applied by the app into the ordinary atmosphere/moon/night_glow fields
    // above, so this block feeds no shader and no kernel.
    //
    // tune_armed = false (nothing pressed) draws ABSOLUTELY NOTHING, which is
    // the acceptance criterion: a pristine launch is pixel-identical.
    bool tune_armed = false;
    int tune_dial = 0;  // 0 = ground_day_gain, 1 = night_glow, 2 = moon gain
    float tune_day_gain = 0.0f;   // [atmosphere] ground_day_gain, live value
    float tune_glow_mul = 1.0f;   // scalar multiplier on the shipped triple
    glm::vec3 tune_glow{0.0f};    // [ground] night_glow, live triple
    float tune_moon_gain = 0.0f;  // [moon] ground_gain, live value
};

void draw_frame(const sim::SimState& state, const sim::AircraftParams& params,
                const CameraPose& pose, const FrameInfo& info,
                const sim::Environment* env);

}  // namespace render
