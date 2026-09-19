#pragma once

#include <glm/glm.hpp>
#include <string>

#include "render/plane_legibility.h"  // render::LegibilityParams IS the
                                      // [plane_legibility] block (rung E4)
#include "world/cold.h"      // world::ColdParams IS the [cold] block (SC1)
#include "world/snowpack.h"  // world::SnowParams IS the [snowpack] block

// TOML -> WorldParams (docs/world_build_plan.md §1, §5): the single source for
// every WORLD/ART threshold. Lives in config/ (with the data), never in world/
// or render/ (no bare numbers there). Strict like load_aircraft /
// load_scenario: every key present, every value sane, or it throws. Angles
// convert from degrees (the config boundary) to radians here, once.
//
// SEPARATE from aircraft.toml / controller.toml — the map never reads those. R
// is NOT here: the planet radius is the sim's (passed as params.R),
// single-source, never mirrored (the H1 lesson; §1 "R is single-source, not
// mirrored").

namespace cfg {

struct WorldParams {
    // Sky/atmosphere (little_planet_plan.md Stage 2; supersedes [sky]). A
    // MONOCHROME luminance model at a frozen epoch: a zenith->horizon day
    // gradient blended day<->dusk<->night by sun elevation over local_up, an
    // exp-shell haze, a night legibility floor, and dither. Color scatter
    // (Rayleigh/Mie) + the weather variable arrive in Stage 3. Elevation band
    // stored in radians (converted at the config boundary).
    struct {
        double sky_space =
            0.0;  // zenith/overhead SPACE luminance (near-black) [0,1]
        double sky_band_top_rad =
            0.0;                   // silver rim thickness above the limb [rad]
        double sky_day = 0.0;      // horizon-band day luminance [0,1]
        double sky_dusk = 0.0;     // horizon-band dusk luminance [0,1]
        double sky_night = 0.0;    // horizon-band night luminance [0,1]
        double dusk_lo_rad = 0.0;  // sun elevation below which night fully sets
        double dusk_hi_rad = 0.0;  // sun elevation above which full day holds
        double night_fill_min =
            0.0;  // THE legibility floor [0,1] + night ground ambient
        double ground_day_gain =
            0.0;  // ground diffuse gain at full sun (Stage 3b)
        double horizon_definition =
            0.0;              // max sky<->ground luminance step [0,1]
        double dither = 0.0;  // twilight/night dither amount [0,1]
        double haze_overcast_density =
            0.0;                    // haze density at full overcast [0,1]
        double haze_scale_m = 0.0;  // exp-shell haze scale height (m, >0)
        // Atmospheric scatter (Stage 3): the ONLY sky color, gated by the haze
        // amount (clear air => 0 => dark-starry). Rayleigh blue (sun high) +
        // Mie orange forward-scatter halo (sun low). scatter_strength >= 0 (0 =
        // off), mie_g in [0, 0.95) (HG blows up at g->1), tint components in
        // [0, 4].
        double scatter_strength = 0.0;  // overall scatter gain (x haze amount)
        double mie_g = 0.0;             // Henyey-Greenstein forward asymmetry
        glm::dvec3 rayleigh_tint{0.0};  // Rayleigh blue tint RGB
        glm::dvec3 mie_tint{0.0};       // Mie orange halo tint RGB
        // S-sunglare (Chad 2026-08-09): the Mie forward-halo coefficients,
        // lifted out of the scatter GLSL's baked constants. halo_gain is THE
        // glare dial; all three >= 0 (a negative gain subtracts light).
        double mie_halo_gain = 0.0;   // overall sun-forward halo gain
        double mie_dusk_boost = 0.0;  // extra halo as the sun drops
        double mie_rim_lift = 0.0;    // broad low-elevation warm lift
        // S-airdome (docs/bubble_atmosphere_spec.md §2, Chad's 2026-08-09
        // ruling): the atmosphere is ALWAYS on where the AtmosphereField says
        // there is air (deck + faction bubbles), never weather-gated. These
        // feed render::AirField (the render-side mirror + optical-depth march
        // tuning), NOT sim/ — the plant's own spatial factor is unchanged.
        double air_haze_density = 0.0;  // always-on in-air haze density [0,1]
        double air_weather_gain = 0.0;  // weather-thickening gain, >= 0
        double tau_scale_m = 0.0;    // path length of unit air == 1 tau (m, >0)
        double march_max_m = 0.0;    // sky ray march cap (m, >0)
        int march_steps_sky = 0;     // [2,32]
        int march_steps_ground = 0;  // [2,32]
        double aerial_gain = 0.0;    // ground extinction gain, >= 0
        glm::dvec3 mie_tint_day{0.0};  // the WHITE scatter tint RGB
    } atmosphere;
    // Weather variable (little_planet_plan.md Stage 3). RAW config at the TOML
    // boundary (seconds / dimensionless) — the app maps this to a
    // render::WeatherParams and calls render::weather_haze(t_cel) per frame.
    // Kept in cfg (not a render type) so config does not depend on render, like
    // [celestial]. A pure deterministic t_cel -> haze[0,1]; gate_lo/gate_hi set
    // the ~70/25/5 clear/haze/overcast mix, the periods set the tempo.
    struct {
        double period1_s = 0.0, period2_s = 0.0,
               period3_s = 0.0;  // front periods (s)
        double weight1 = 0.0, weight2 = 0.0,
               weight3 = 0.0;                             // amplitudes (sum 1)
        double phase1 = 0.0, phase2 = 0.0, phase3 = 0.0;  // fixed phases
        double gate_lo = 0.0,
               gate_hi = 0.0;  // smoothstep gate (distribution knobs)
    } weather;
    // [weather_cell] — the LOCALIZED WEATHER FIELD
    // (docs/weather_seasons_plan.md W2). RAW config at the boundary (angles in
    // deg, thresholds dimensionless); the app maps this to a
    // render::WeatherCellParams and calls render::weather_cell(dir, t_cel) per
    // frame. The [weather] scalar becomes the storm BUDGET; these place the
    // microsystem cells on the sphere. Kept in cfg (not a render type) so
    // config does not depend on render/, like [weather].
    struct {
        int cell_count = 0;      // # microsystem cells (>= 1)
        double inner_deg = 0.0;  // full-haze cone radius (deg)
        double outer_deg = 0.0;  // falloff-to-0 radius (deg, > inner_deg)
        double thresh_lo = 0.0;  // per-cell activation thresholds spread across
        double thresh_hi = 0.0;  // [lo,hi], each in (0,1), lo < hi
        double env_width =
            0.0;  // budget smoothstep width a cell fades in over (> 0)
        // S-bubbleweather: the ANCHORED (in-bubble) lattice's own scale —
        // cells packed inside each dome, sized to a bubble not to the sphere.
        int bubble_cell_count = 0;      // cells per bubble (>= 1)
        double bubble_inner_deg = 0.0;  // full-haze cone radius (deg)
        double bubble_outer_deg = 0.0;  // falloff-to-0 radius (deg, > inner)
        double bubble_fill_frac =
            0.0;  // fraction of the dome radius cells spread over, (0,1]
    } weather_cell;
    // [seasons] — the WEATHER SEASON framing (docs/weather_seasons_plan.md W1).
    // RAW config at the boundary (an already-resolved season index + draw
    // weights) so config does not depend on render/ (like
    // [celestial]/[weather]). The app maps static_season/weights to a
    // render::Season chosen once per spawn (env SEADS_SEASON > static_season >
    // smoke default > weighted random).
    struct {
        int static_season = -1;      // -1 = "random" (draw at spawn); else a
                                     // render::Season index [0,3] forcing that
                                     // season (winter-loop building / smoke).
        double weight_winter = 0.0;  // weighted-random draw weights per season
        double weight_spring =
            0.0;  // (each >= 0, SUM > 0). Only consulted when
        double weight_summer = 0.0;  // static_season == -1.
        double weight_autumn = 0.0;
    } seasons;
    // [precip] — W3 precipitation (docs/weather_seasons_plan.md). RAW config at
    // the boundary (metres / Hz / [0,1]); the app maps it to a
    // render::PrecipLook. Season-gated (Winter=snow/Spring=rain/else dry) +
    // weather-gated (W2 cell at the eye). Kept in cfg (not a render type) so
    // config does not depend on render/.
    struct {
        bool enabled = false;
        double cell_size_m =
            3.0;  // world lattice spacing / flake spacing (m, > 0)
        double box_half_m =
            21.0;  // eye-following box half-extent / fade radius (m, > 0)
        double wrap_fade = 0.12;  // vertical wrap fade fraction ([0, 0.5))
        glm::dvec3 color{0.90, 0.93,
                         0.97};      // near-white mono flake/streak tint
        double snow_size_m = 0.28;  // Winter flake radius (m, > 0)
        double snow_speed_mps =
            0.60;  // AS-3: Winter fall SPEED (m/s, >= 0). REPLACES
                   // snow_rate_hz — two lattices at different cell sizes can
                   // only agree on a speed. rate_hz = speed / cell_size_m.
        double snow_opacity = 0.75;  // Winter alpha at full intensity ([0,1])
        double rain_size_m = 0.05;   // Spring streak half-width (m, > 0)
        double rain_streak_m =
            1.30;  // Spring streak half-length along local-up (m, > 0)
        double rain_rate_hz = 1.50;  // Spring fall cycles/s (>= 0)
        double rain_opacity = 0.55;  // Spring alpha at full intensity ([0,1])
        // --- AS-2: the snowfall field (render::snowfall_intensity). ---
        double flurry_level = 0.0;      // light-band peak [0,1] (0 = OFF)
        double flurry_thresh_lo = 0.0;  // flurry activation band in BUDGET,
        double flurry_thresh_hi = 0.65; //   lo <= hi, both in [0,1]
        // AS-4 EVENT DURATION: the flurry's own cell scale + tempo. OFF values
        // (scale 1.0, degrees 0, count 0) reproduce the AS-3 field exactly.
        double flurry_period_scale = 1.4;  // x the three front periods (> 0)
        double flurry_inner_deg = 6.0;     // 0 => cp.bubble_inner_deg
        double flurry_outer_deg = 14.0;    // 0 => cp.bubble_outer_deg
        double flurry_cell_count = 5.0;    // 0 => bubble_cell_count + 7
        double flurry_gate_lo = -0.95;  // the flurry BUDGET's own clear-plateau
                                        //   edge (< gate_hi; [weather] is -0.05)
        double flurry_phase_off = 2.6;  // rad added to the flurry budget's three
                                        //   front phases (>= 0)
        double snow_floor = 0.0;        // constant winter dusting [0,1] (0=OFF)
        // --- AS-5: the snow SQUALL on its own dials (render::snowfall_squall).
        // squall_own_dials 0 => weather_cell on [weather]/[weather_cell]
        // exactly, every squall_* key below ignored (the OFF value).
        bool squall_own_dials = true;
        double squall_inner_deg = 8.0;     // 0 => cp.bubble_inner_deg
        double squall_outer_deg = 15.0;    // 0 => cp.bubble_outer_deg
        double squall_cell_count = 3.0;    // 0 => cp.bubble_cell_count
        double squall_gate_lo = -0.65;     // squall budget clear edge, (-1,1)
        double squall_thresh_lo = 0.0;     // activation band in budget,
        double squall_thresh_hi = 0.35;    //   lo <= hi, both in [0,1]
        double squall_period_scale = 1.0;  // x the three front periods (> 0)
        double squall_phase_off = 0.0;     // rad added to the phases (>= 0)
        // --- AS-1: the underground gate. ---
        double rock_band_m = 2.0;  // smoothstep half-width at the rock (m, >=0)
        // --- AS-3: look/variety/veil. Each 0 (or 1 for rim_dark) is OFF. ---
        double size_var = 0.0;     // flake size  x [1-v, 1+v], v in [0,1)
        double alpha_var = 0.0;    // flake alpha x [1-v, 1], v in [0,1)
        double density_exp = 0.0;   // per-cell cull exponent (>= 0)
        double density_soft = 0.0;  // cull fade-in width in keep_p ([0,1))
        double sway_m = 0.0;       // NEAR zero-mean sway amplitude (m, >= 0)
        double veil_sway_m = 0.0;  // FAR zero-mean sway amplitude (m, >= 0)
        bool veil_enabled = false;      // the FAR veil lattice on/off
        double veil_cell_size_m = 8.0;  // far lattice spacing (m, > 0)
        double veil_box_half_m = 70.0;  // far fade radius (m, >= cell)
        double veil_size_m = 0.45;      // far flake radius (m, > 0)
        double veil_opacity = 0.35;     // far alpha at full intensity ([0,1])
        double veil_inner_fade_m = 0.0;  // veil hole radius around the eye (m,
                                         //   >= 0; < veil_box_half_m)
        double rim_dark = 1.0;          // flake edge darkening ([0,1], 1 = OFF)
    } precip;
    struct {
        double reflectivity = 0.0;       // mirror strength of the sky [0,1]
        double sparkle_sharpness = 0.0;  // specular pow() exponent (>0)
        double min_lake_m = 0.0;         // authored lake floor (m, >0)
        double surface_lift_m =
            0.0;  // dedicated lake mirror-mesh radial lift (m, >0)
        double star_reflect =
            0.0;  // reflected procedural-star brightness [0,1]
        double star_density =
            0.0;  // reflected-star cell density (cells/unit dir, >0)
    } water;
    struct {
        double relief_scale_m =
            0.0;  // radial displacement at full-white height (m)
        double field_scale_m = 0.0;  // farmland/bush patchwork cell (m)
        bool procedural = false;     // A/B: false = Earth DEM, true = Sudbury
        // W4 seasonal ground (winter snow-cover); a shader tint on the LAND
        // albedo.
        double winter_snow_cover =
            0.0;  // winter snow amount [0,1] (0 = no snow)
        double snow_albedo =
            0.0;  // mono snow target the land whitens toward [0,1]
        double snow_slope_lo =
            0.0;  // flatness below which slopes shed snow [0,1)
        // S1 LAKE ICE. ice_albedo is deliberately BELOW snow_albedo: telling
        // the lake from the shore is the read that §6b.2 (drivable ice) and
        // §3.5 (punch-through) both depend on.
        double ice_albedo = 0.0;        // frozen-lake albedo [0,1]
        double ice_reflect_frac = 0.0;  // mirror kept under winter [0,1]
        double ice_glint_frac = 0.0;    // glint kept under winter [0,1]
        // S1b winter night: snow/ice lose brightness but stay WHITE.
        glm::dvec3 night_glow{0.70, 0.75, 0.84};  // COOL night light on snow/ice
        double ice_night_reflect_frac = 0.0;  // extra mirror damp after dark
        // Snow sparkle (Chad, twice: snow "appears to sparkle" in moon AND
        // starlight). Night-only per-cell glint on snow/ice; 0 disables it.
        double snow_sparkle = 0.0;        // night glint amplitude [0,1]
        double snow_sparkle_sharp = 0.0;  // glint exponent (>= 8)
        // Fly 3 (2026-08-11): steepest barren faces darken the albedo.
        double barren_face_dark = 0.0;         // albedo kill at full ramp
        double barren_face_dark_hi_deg = 0.0;  // full face-dark at/above
        double barren_face_mottle = 0.0;       // mottle resists face-dark
    } ground;
    // ★ R5 row 9 — SHADOWS ON SNOW ([shadows]): receiver-side analytic
    // occluder proxies in the planet shader (docs/snow_R5_immersion_ledger.md
    // ROW 9, option (c)). Look dials only — caster GEOMETRY is measured in
    // code (render/shadow_casters.h), never configured. strength/tint shape
    // the DIRECT-SUN term inside a shadow; the ambient night-fill survives by
    // construction (a snow shadow is sky-lit, not black).
    struct {
        bool enabled = false;
        double strength = 0.0;  // occlusion of the direct sun at full umbra
                                // [0,1] (x the SEADS_SHADOWS dial at the app)
        glm::dvec3 tint{0.0};   // full-umbra direct-sun multiplier RGB —
                                // darker AND bluer (b > r enforced: the
                                // ribbons.h/S1_SPEC RT-1 packed-snow ruling)
        double underground_margin_m = 0.0;  // fence 5: casters more than this
                                            // far below the drive surface
                                            // (tunnel network) cast nothing
    } shadows;
    // ★ WINTER_LAW §2.2 -- THE SNOWPACK FUNCTION's dials, [snowpack]. The
    // struct is world::SnowParams ITSELF, not a config-side mirror of it: a
    // mirror is a second place a dial can be renamed or dropped, and §2.1c
    // makes this function the ride-feel dial for the whole winter game. There
    // is nothing here the world module does not own.
    world::SnowParams snowpack;
    // ★ SF1 -- WINTER_LAW §3.6b, the St. Charles snow mountain. The struct is
    // world::SnowhillParams ITSELF (the [snowpack]/[cold] anti-mirror rule).
    // The anchor frame comes from world/snowhill_geo.gen.h at load, lock-hash
    // checked against the baked GIS projection.
    world::SnowhillParams snowhill;
    // ★ SF2-BANKS -- WINTER_LAW §3.6c, the oreo snowbanks. Render-only dials
    // (the bank GEOMETRY is SnowpackField::depth_at, sampled -- no shape dial
    // exists here on purpose, and NO lift: it single-sources [ribbons] lift_m).
    struct {
        bool enabled = false;
        double station_m = 16.0;
        double skirt_m = 6.0;
        double skirt_bury_m = 0.5;
        double speckle_density = 0.22;
        double speckle_dark = 0.55;
        // ★ ROAD-REPAIR rung AA -- the gravel-speckle anti-alias
        // width multiplier. The oreo albedo is a deterministic hash of ~22 cm
        // grains in surface metres; once a grain is under a pixel, which grain
        // a pixel samples is decided by where the EYE is, and 2 cm of eye
        // travel re-samples the whole field. That is 13.5-16.4 pp of the
        // road-mask flicker Chad reported (onaping_flash_F3.md 6.3). This dial
        // scales the grain footprint the Nyquist gate is taken on: 1.0 fades
        // the binary speckle onto its own mean coverage over a grain period of
        // 2 px down to 1 px. The gate's footprint is the ISOTROPIC one (the
        // Frobenius norm of the grain-per-pixel Jacobian), not fwidth's L1
        // sum, so the onset does not swing with the view orientation -- the
        // red-team fold. 0.0 == the identity, BY AN EXPLICIT BRANCH.
        // SEADS_SPECKLE_AA REPLACES this value (clamped to [0, 4]).
        double speckle_aa = 0.0;
        double crest_smudge = 1.2;
        double min_amp_m = 0.20;
        int skirt_rings = 3;        // ★ ROAD-REPAIR: skirt sub-rings
        double chord_tol_m = 0.05;  // ★ ROAD-REPAIR: section chord tolerance
        // ★ ROAD-REPAIR: short stations where the junction gap is fading.
        double junction_station_m = 0.0;
        // ★ ROAD-REPAIR F1 -- the bank yields to the drawn deck.
        // deck_yield_m == 0.0 is the identity (the predicate is never run).
        double deck_yield_m = 0.0;
        // ★ ROAD-REPAIR ONAPING RUNG 2 -- the drawn apron past the skirt.
        // apron_m == 0.0 is the identity (no apron strip is built at all).
        double apron_m = 0.0;
        double apron_tol_m = 0.25;
        double apron_min_drop_m = 0.5;
    } bank_mesh;
    // ★ SC1 -- WINTER_LAW §3.7 "COLD IS POWER". The struct is world::ColdParams
    // ITSELF, not a config-side mirror (same reasoning as [snowpack] just
    // above): there is nothing here the world module does not own.
    world::ColdParams cold;
    struct {
        int subdiv = 0;  // cubesphere verts per face edge (2..256)
        int tiles = 0;   // R4d face tiling: tiles×tiles sub-meshes per
                         // face (1..4); effective grid tiles*(subdiv-1)+1
        double u_offset = 0.0;    // longitude alignment of the map (fraction)
        int dem_blur_radius = 0;  // DEM softening (>=0; coupled to relief)
        int cubemap_size = 0;     // albedo cubemap per-face edge (texels, >0)
    } planet;
    // S2 boreal tree scatter (stereoscope-sudbury). FELT fly-dials; placement
    // is the hash-Poisson scatter in world/props.*. enabled=false leaves the
    // world treeless (the pre-S2 look).
    struct {
        bool enabled = false;
        double density_gain =
            1.0;  // acceptance scale (the moonscape-density fly-dial)
        double min_scale = 0.72, max_scale = 1.5;  // per-tree size jitter range
        double height_m = 16.0;                    // trunk height at scale 1
        double base_width_m = 4.5;       // crown half-spread at scale 1
        double render_range_m = 6000.0;  // hard fade-out distance cap (m)
        double ambient = 0.16,
               diffuse = 0.72;    // mono shading (feeds the silver post)
        double fade_frac = 0.16;  // scale-fade band as a fraction of the reach
        // S2b (WINTER_LAW sec2.4b): canopy cleared BEYOND the drawn ribbon
        // half-width along trails/roads. Negative disables the mask.
        double corridor_margin_m = 3.0;
        int cells_per_face = 1280;  // grid cells per cube-face edge (~18 m)
        int chunk_cells = 256;      // cells per chunk edge (cull/cache unit)
    } trees;
    // S3 draped linework ribbons (stereoscope-sudbury). Roads MONO (silvered by
    // the S1 post); the snowmobile trail is the ONE sanctioned world-chroma.
    // Colors are RGB [0,1]; the geometry is baked (render/sudbury_gis.gen.h),
    // these are the FELT look dials only.
    struct {
        bool enabled = false;
        double lift_m = 2.0;  // radial lift above the terrain (Fable P1-2)
        // ★ ROAD-REPAIR / ONAPING SINK: the longest CHORD the drape may
        // span between two draped rungs (m). The baked GIS rungs are 52.67 m
        // apart at the median; the mesh between them is FLAT, so over a
        // concave grade break the drawn deck bridges above the ground the
        // machine stands on. 0.0 == the byte-identical identity.
        double max_seg_m = 0.0;
        // ★ ROAD-REPAIR / ONAPING SINK, the TRANSVERSE half: the longest
        // chord ACROSS the deck (m). A rung is ONE quad across the whole
        // road, so the centreline -- where the machine rides -- is a flat
        // chord between the two edges. 0.0 == the identity.
        double max_tr_m = 0.0;
        // ★ ROAD-REPAIR F2 -- THE JUNCTION CUT radius (m). No junction cut
        // exists anywhere in the road stack: each baked way is draped as its
        // own quad strip and nothing clips one deck against another, so a
        // square-cut deck end stands across its neighbour's pavement and two
        // near-coplanar decks share one polygon offset. At a node where 3+
        // ways end, F2 trims every leg back to this radius and emits ONE cap
        // over the node. 0.0 == the identity, by an explicit branch.
        double junction_cut_m = 0.0;
        // ★ ROAD-REPAIR F3 -- THE OVER-BANK BIAS. The deck pass and the
        // bank pass ship the IDENTICAL glPolygonOffset(-2,-4), so the two
        // surfaces have ZERO relative depth bias and which one wins is decided
        // by the last bit of a 24-bit depth value -- stable in a still frame,
        // flipping in motion (docs/road_repair/onaping_flash_E2.md §1). This
        // dial scales the DECK pass's pair to (-2(1+b), -4(1+b)); the bank pass
        // is untouched, so b > 0 puts the deck on top everywhere they tie.
        // Units are the shipped pair. 0.0 == the identity, BY AN EXPLICIT
        // BRANCH (the old glPolygonOffset call, verbatim).
        double over_bank_bias = 0.0;
        // ★ ROAD-REPAIR rung AA -- the centreline anti-alias width
        // multiplier. The dashed white line is one hard ternary in the deck FS
        // with no derivative anywhere, while the trail corduroy in the same
        // shader has had the fwidth fade all along; once the 0.312 m stripe is
        // sub-pixel, 2 cm of eye travel re-decides it (onaping_flash_F3.md 6.4
        // -- Chad: "the white line"). ⚠ The red-team fold corrected the RANGE:
        // that is ~292 m (935 px/rad at screen centre, kChaseFovyDeg 60 at
        // 1080p), not the rig's 70 m where the stripe is ~4 px wide, and a
        // graze foreshortens the road along s, never the stripe across v.
        // This dial scales the screen-space footprint the stripe and the dash
        // are box-filtered over. 0.0 == the identity, BY AN EXPLICIT BRANCH.
        // SEADS_LINE_AA REPLACES this value (clamped to [0, 4]).
        double line_aa = 0.0;
        glm::dvec3 road_bed{0.10};   // dark road surface (mono)
        glm::dvec3 road_line{0.80};  // light dashed centerline (mono)
        double road_center_frac =
            0.06;  // |v| < this == the centerline stripe (thin)
        double road_dash_m = 7.0, road_gap_m = 10.0;
        double road_mottle =
            0.14;  // lighter mottled-patch strength (subtle; road stays dark)
        double road_mottle_frac = 0.25;  // fraction of the length that mottles
        double road_fade = 0.10;         // global light fade on the asphalt
        glm::dvec3 trail_color{0.37, 0.27,
                               0.19};  // dark brown earth trail path
        double trail_mottle =
            0.15;  // subtle natural variation on the clay path
        // S1 winter trail (groomed packed snow + corduroy)
        glm::dvec3 trail_winter_color{0.80, 0.84, 0.90};
        double trail_corduroy = 0.0;    // groomer cross-line depth [0,1]
        double trail_corduroy_m = 1.6;  // groomer cross-line spacing (m)
        // rivers (OSM waterway) render as MIRROR water, NOT ribbons — see
        // [water].
    } ribbons;
    // S4 building massing (stereoscope-sudbury). Real OSM footprints ->
    // extruded flat/gabled/hipped prisms (baked in render/sudbury_gis.gen.h);
    // these are the FELT look dials only. MONO (silvered by the S1 post);
    // sun-lit flat facets.
    struct {
        bool enabled = false;
        double wall_val = 0.42;  // vertical-wall base luminance (mono)
        double roof_val = 0.62;  // roof/cap base luminance (mono; lighter)
        double ambient = 0.35;   // flat ambient term
        double diffuse = 0.75;   // N·L sun-diffuse gain
        double height_scale =
            1.0;  // vertical exaggeration of the baked heights
        glm::dvec3 window_color{
            1.0, 0.98, 0.95};  // night window-light tint (near-white, low-sat)
        double window_bright = 0.9;  // night window emissive strength (0 = off)
        double window_lit_frac =
            0.5;  // fraction of a lit building's windows on
    } buildings;
    // Street-lamp night point lights (stereoscope-sudbury). Baked positions;
    // these are the look dials. Near-white (low-sat -> silver via the S1 post).
    struct {
        bool enabled = false;
        glm::dvec3 color{1.0, 0.98, 0.95};  // glow tint (near-white, low-sat)
        double brightness = 0.55;  // soft-halo (aura) additive strength
        double core_bright = 1.0;  // hot-core (bulb) strength — lit up close
        double core_sharp = 34.0;  // core tightness (higher = crisper bulb)
        double size_m = 3.5;       // world glow radius (m) — up-close size
        double min_px = 2.5;       // far floor: min on-screen radius (px)
    } streetlamps;
    // CC1 Superstack smoke (Living Copper Cliff). A mono drifting plume off the
    // Copper Cliff Superstack; pure-phase billboard puffs. MONO (silvered by
    // the S1 post — the CC3 hot slag is the chroma exception, not this).
    struct {
        bool enabled = false;
        double puffs = 28.0;                 // billboard count in the column
        glm::dvec3 color{0.60, 0.60, 0.62};  // mono grey (low-sat -> silver)
        double rise_m = 900.0;               // vertical rise over one puff life
        double drift_m = 560.0;  // downwind horizontal drift over a life
        double r0_m = 26.0;      // puff radius at birth (m)
        double r1_m = 155.0;     // puff radius at death — expands rising
        double opacity = 0.42;   // peak per-puff alpha
        double jitter_m = 16.0;  // fixed lateral scatter (breaks the line)
        glm::dvec3 wind{1.0, 0.15,
                        0.0};  // world wind hint (projected to tangent)
        double rate_hz =
            0.03;  // plume cycle rate (puff birth->death per s of t_cel)
    } smoke;
    // CC2 slag-pot trains (Living Copper Cliff). A loco + pots circulating a
    // hand-authored spur at Copper Cliff. Mono steel (silvered by the S1 post).
    struct {
        bool enabled = false;
        double pots = 8.0;         // slag-pot cars behind the loco
        double car_gap_m = 24.0;   // CC5: car spacing = the pour cadence (F8)
        double tip_span_m = 14.0;  // CC5: arc each pot tips over the dump edge
        double lift_m = 1.4;       // drape above the terrain (rail head)
        glm::dvec3 color{0.46, 0.46, 0.48};  // mono steel (low-sat -> silver)
        double ambient = 0.34, diffuse = 0.72;
        double rate_hz = 0.0011;  // loop cycles per s of t_cel (~4-5 m/s haul)
    } train;
    // CC3 slag pour (Living Copper Cliff) — the marquee. Dark mound + tipping
    // pot + the WARM-CHROMA lava cascade (the one sanctioned chroma exception,
    // Chad's call).
    struct {
        bool enabled = false;
        double mound_radius_m = 150.0, mound_top_r_m = 62.0,
               mound_height_m = 55.0;
        // CC4 (2026-07-13): the dump is a LINEAR slag RIDGE, not a cone — a
        // flat crest carrying the track + a steep pour face. mound_height_m is
        // reused as the ridge crest height; ridge_length_m is Chad's "proper
        // length of the hill ridge" dial.
        double ridge_length_m = 700.0;  // crest length along the dump edge (m)
        double crest_width_m = 24.0;    // flat top width carrying the track (m)
        double face_angle_deg = 35.0;  // MEAN pour-face slope (angle of repose)
        double benches =
            3.0;  // CC6 terrace steps down the pour face (1=planar)
        glm::dvec3 mound_color{0.10, 0.10, 0.11};  // dark matte slag (mono)
        double ambient = 0.30, diffuse = 0.55;
        double rivers = 3.0;  // molten streaks down the pour face
        double river_halfwidth_m = 7.0;
        double glow = 1.0;           // emissive lava gain
        double night_boost = 0.7;    // extra emissive when the sun is down
        double pour_rate_hz = 0.05;  // pour cycles per s of t_cel (~one/20 s)
        double lift_m = 1.5;
    } slag;
    // Fleet Rig (fleet_rig_plan.md; /orchestrate rig-A.2). The render-only
    // mirror-finish aircraft: FELT knobs only (rest-pose geometry is structural
    // in render/rig.cpp). Colors are RGB fractions [0,1] — the ONLY saturated
    // entries in the render config (noir ground, chroma planes).
    struct {
        double reflectivity = 0.0;   // env-reflection strength over body [0,1]
        double fresnel_power = 0.0;  // Fresnel rim exponent (>0)
        // (per-plane chroma moved to [teams] - S-mapteam 2026-08-09)
        // rig-B state-driven deflection (fleet_rig_plan.md). Surface throws in
        // DEGREES at full command (converted to radians at the render
        // boundary); prop blur-disc opacity floor/ceiling [0,1].
        double aileron_deg = 0.0;      // aileron throw at |roll| = 1 (>0)
        double elevator_deg = 0.0;     // elevator throw at |pitch| = 1 (>0)
        double rudder_deg = 0.0;       // rudder throw at |yaw| = 1 (>0)
        double gear_deploy_deg = 0.0;  // gear swing, deployed -> in-bay (>0)
        double prop_disc_alpha =
            0.0;  // prop blur opacity at full throttle [0,1]
        double prop_idle_alpha = 0.0;  // prop blur opacity at idle [0,1]
    } fleet_rig;
    // [plane_legibility] — RUNG E4 VISIBILITY (Chad 2026-08-20: "they are hard
    // to see especially in the white scatter light, their tag and color is
    // also a bit hard to see"). The struct IS the block (the [cold]/[snowpack]
    // pattern): render::LegibilityParams, one definition, so the loader and the
    // draw layer cannot fork. Its DEFAULTS are the OFF values — every dial at
    // its off value reproduces the flown-approved frame exactly.
    render::LegibilityParams legibility{};
    // Gun battery (rig-D guns leg; weapon/ballistics.h). The Bf 109 F-4/R1 fit:
    // 1 hub 20mm + 2 cowl 7.92mm MG + 2 wing 20mm gondolas, all converging on
    // one trigger. Muzzle tips are ASSEMBLY body coords (+X right/+Y up/-Z
    // nose) measured from the Blender build (generated/bf109/build_all.py).
    // SCAFFOLD: loaded now, CONSUMED next session when firing is wired (the
    // physics module is weapon/ballistics.*). muzzle speeds/rof in SI;
    // convergence floored >= 50 m so no muzzle (esp. the boresight hub) fires
    // backward (Fable P1-3).
    struct {
        double convergence_range_m = 0.0;  // harmonisation range [m] (>= 50)
        double cannon_speed_mps = 0.0;     // 20mm MG151/20 muzzle speed [m/s]
        double mg_speed_mps = 0.0;         // 7.92mm MG17 muzzle speed [m/s]
        double cannon_rof_hz = 0.0;        // 20mm cyclic rate [rounds/s] (>0)
        double mg_rof_hz = 0.0;            // 7.92mm cyclic rate [rounds/s] (>0)
        double cannon_drag_k =
            0.0;                 // 20mm quadratic drag constant [1/m] (>= 0)
        double mg_drag_k = 0.0;  // 7.92mm quadratic drag constant [1/m] (>= 0)
        double tracer_lifetime_s = 0.0;  // tracer trail fade [s] (render)
        double tracer_len_m = 8.0;       // streak length [m] (render)
        glm::dvec3 tracer_cannon_rgb{1.0, 0.95,
                                     0.7};  // 20mm tint (now orange/red)
        glm::dvec3 tracer_mg_rgb{0.9, 0.9,
                                 1.0};  // 7.92mm tint (now brighter orange)
        // Tracer comet look dials (iter-7 Task E: lifted from render/ inline
        // magic).
        double tracer_cannon_base = 1.0;  // 20mm width multiplier
        double tracer_mg_base = 0.72;     // 7.92mm width multiplier (narrower)
        double tracer_r_core_frac = 0.0011;  // core radius = frac * view_dist
        double tracer_r_core_min = 0.05;     // minimum core radius [m]
        double tracer_r_glow_mult = 3.0;     // glow radius = mult * r_core
        double tracer_glow_lum = 0.5;        // glow layer luminance fraction
        double tracer_glow_alpha = 0.45;     // glow layer additive alpha
        double tracer_core_lum = 1.0;        // core layer luminance
        double tracer_core_alpha = 0.95;     // core layer additive alpha
        double tracer_head_r_mult = 1.5;  // head sphere radius = mult * r_core
        double tracer_head_lum = 1.5;     // head luminance (white-hot)
        double tracer_head_alpha = 1.0;   // head alpha
        double tracer_slag_dark =
            0.75;  // slag fleck darkness [0=none, 1=black]
        double tracer_slag_period_m =
            1.8;  // world-space repeat of dark slag chunks [m]
        double tracer_glow_len_mult =
            1.7;                     // glow tail length = tracer_len_m * mult
        glm::dvec3 muzzle_hub{0.0};  // hub 20mm (Motorkanone) muzzle tip
        glm::dvec3 muzzle_cowl_l{0.0};  // left cowl 7.92mm MG
        glm::dvec3 muzzle_cowl_r{0.0};  // right cowl 7.92mm MG
        glm::dvec3 muzzle_wing_l{0.0};  // left wing 20mm gondola
        glm::dvec3 muzzle_wing_r{0.0};  // right wing 20mm gondola
        double cannon_damage = 0.0;     // [HP] damage per 20mm cannon hit (> 0)
        // FLAK dials (docs/FLAK_GUN_SPEC.md §7.4); facts live in flak_gun.h.
        double flak_damage = 0.0;       // [HP] reference damage per flak hit
        double flak_reload_s = 0.0;     // [s] drum swap
        double flak_selfdestruct_s = 0.0;  // [s] tracer self-destruct (0 = off)
        // [m] proximity-fuze radius ADD on the flak-vs-aircraft hit test
        // (0 = the bare 20 mm, direct hits only). Widens combat_tick's swept
        // radius for the FLAK pool only; the aircraft gun never sees it.
        double flak_prox_radius_m = 0.0;
        // FLAK TRACER LOOK (Stage B, immersion ladder 2/4) -- PURELY
        // COSMETIC, and separate from the tracer_* keys above because the
        // aircraft look is SIGNED and must not move: a ground gun walking
        // 20 mm fire into the sky has to read longer, hotter and redder than
        // the fighter's rounds do at the same range.
        glm::dvec3 flak_tracer_rgb{1.0, 0.38, 0.05};  // red-orange
        double flak_tracer_base = 1.55;      // width multiplier
        double flak_tracer_len_mult = 2.1;   // streak = tracer_len_m * this
        double flak_tracer_lum_mult = 1.30;  // core/glow/head luminance gain
        double mg_damage = 0.0;         // [HP] damage per 7.92mm MG hit (> 0)
    } guns;
    // S1 stereoscope post pass (docs/stereoscope_s1_plan.md; render/post.cpp is
    // the ONE tone owner — the scene is authored MONO, the planes are the only
    // chroma). ALL values are DISPLAY-space (Fable-BEFORE 2026-07-10: no ACES —
    // a scene-linear tonemap would double-tone AND drain plane chroma). Tints
    // are near-neutral RGB MULTIPLIERS of luminance (a silver PRINT, not a
    // duotone remap): (1,1,1) = pure grey; cool shadow -> warm highlight = the
    // split-tone.
    struct {
        double contrast = 0.0;  // display sigmoid steepness c (>0; 1=identity)
        double lift = 0.0;      // black lift [0,1)
        double grain = 0.0;     // grain amount k_g [0,0.25] (minimal, Chad)
        glm::dvec3 split_shadow{1.0};  // cool silver shadow tint (R<1<B)
        glm::dvec3 split_hi{1.0};      // warm silver highlight tint (R>1>B)
        // S1b-3: what split_hi becomes at full night. Daylight is untouched.
        glm::dvec3 split_hi_night{1.0};
        double sat_c0 =
            0.0;  // split-tone chroma-gate deadband (mono below->silver)
        double sat_c1 =
            0.0;  // split-tone chroma-gate knee (chroma above->pass)
        double sat_dark =
            0.0;  // near-black chroma floor m_dark (kills shadow halo)
        double halation = 0.0;  // warm bloom strength [0,1]
        double halation_threshold =
            0.0;  // luma above which halation blooms [0,1]
        glm::dvec3 halation_tint{
            1.0};               // warm halation tint (a print-artifact chroma)
        double vignette = 0.0;  // corner exposure falloff [0,1]
        double fxaa = 0.0;      // FXAA blend amount [0,1] (0=off)
    } tone;
    // S6 far-field-only depth of field (stereoscope-sudbury). Everything within
    // focus_start_m stays crisp; the distant limb/background blurs to radius_px
    // by focus_end_m. sky_coc caps the far-sky blur (protect the space-first
    // stars). enabled=false == the pre-S6 crisp look. View-space metres /
    // screen px.
    struct {
        bool enabled = false;
        double focus_start_m = 4000.0;  // crisp out to here (view-space m)
        double focus_end_m =
            25000.0;             // full blur reached here (> focus_start_m)
        double radius_px = 2.5;  // blur disc radius at full CoC (screen px)
        double sky_coc = 0.4;    // far-sky CoC cap [0,1]
    } dof;
    // S6 "printed card" halftone/dither MODE (stereoscope-sudbury). OFF by
    // default (enabled=false -> the post pass is bit-exact identity). When on,
    // the mono silver world becomes a screen-locked newsprint print (dots or
    // ordered dither); the aircraft (chroma) pass through smooth (the
    // split-tone sat gate). style: 1 = AM halftone dots, 2 = ordered (Bayer)
    // dither.
    struct {
        bool enabled = false;
        int style = 1;          // 1 halftone dots · 2 ordered dither
        double scale_px = 4.0;  // dot-cell size (screen px); floored >= 2
        double angle_rad =
            0.7853982;      // screen-grid rotation (mode 2 ignores it)
        double soft = 1.0;  // mode1: px AA-width mult; mode2: tonal crossfade
        double ink = 0.05;  // ink darkness (× split_shadow -> cool near-black)
        double grain_mul =
            0.25;  // grain scale on the mono print (planes keep full)
    } halftone;
    struct {
        double margin_rad = 0.0;  // horizon over-cull pad (radians, >=0)
    } horizon_cull;
    struct {
        int seed = 0;                  // determinism anchor (>=0)
        double furniture_scale = 0.0;  // landmark up-scale (>=1)
    } scatter;
    struct {
        double frame_budget_ms = 0.0;  // (>0)
        int max_draw_calls = 0;        // instanced ceiling (>=1)
    } perf;
    // Celestial system (little_planet_plan.md Stage 1). RAW config at the TOML
    // boundary (degrees / seconds / [0,1) fractions) — the app maps this to a
    // render::CelestialConfig and calls render::make_celestial (which owns the
    // structural asserts: integer year, non-parallel lean). Kept in cfg (not a
    // render type) so config does not depend on render. The star CATALOG lands
    // in Stage 4 (rendering); Stage 1 is the scalar/vector wheel only.
    struct {
        glm::dvec3 orbit_normal{0.0, 1.0, 0.0};  // ecliptic pole
        double tilt_deg = 0.0;                   // axial tilt
        glm::dvec3 tilt_lean{1.0, 0.0, 0.0};     // which way â leans
        double day_period_s = 0.0;               // SOLAR day
        double year_period_s = 0.0;
        double epoch_day_frac = 0.0;
        double epoch_year_frac = 0.0;
        double epoch_moon_frac = 0.0;
        double sun_angular_diameter_deg = 0.0;
        double sun_distance_m = 0.0;
        double sun_intensity = 0.0;
        double sun_glare_deg = 0.0;
        double moon_angular_diameter_deg = 0.0;
        double moon_period_s = 0.0;
        double moon_inclination_deg = 0.0;
        double moon_intensity = 0.0;
        double star_brightness = 0.0;
    } celestial;
    // Star field (little_planet_plan.md Stage 4). The CATALOG is a compiled
    // header (render/star_catalog.gen.h, real HYG data); these are the render
    // knobs the app maps to a render::StarParams. Bright-star realism:
    // magnitude
    // -> BRIGHTNESS (Pogson), a constant FOV-compensated pixel size, extinction
    // by the shared sky luminance + haze, glare suppression near the sun.
    struct {
        double mag_limit =
            0.0;               // cull catalog stars fainter than this (<= bake)
        double mag_ref = 0.0;  // magnitude mapping to brightness 1.0 (Pogson)
        double size_px = 0.0;  // constant on-screen core half-size [px] (>0)
        double glare_suppress_deg = 0.0;  // fade stars this near the sun (>=0)
        double wash_lum = 0.0;  // sky luminance above which stars wash [0,1]
        // S-starnight (Chad 2026-08-09): how much the ALWAYS-ON dome air veils
        // stars, [0,1]. Ships 0 — clear dome air must NOT hide the starfield;
        // only overcast (the weather amount) does.
        double air_extinction = 0.0;
        double r_star_m = 0.0;  // cosmetic view-space depth (>0, < far plane)
    } stars;
    // Moon LOOK knobs (little_planet_plan.md Stage 5). Orbital params (period,
    // inclination, angular size) live in [celestial]; these are the render
    // knobs the app maps to a render::MoonParams. Moonlight is a phase-scaled
    // 2nd directional light on the planet (full moon lights the night side; new
    // moon stays dark for the break-contact tactic).
    struct {
        double disc_intensity = 0.0;  // silver disc brightness [0,4]
        double ground_gain = 0.0;  // moonlight ground fill at full moon [0,2]
        double fill_lo = 0.0;      // phase-shaping knee [0,1)
        double sparkle_sharpness = 0.0;  // moonlit-lake glint exponent (>0)
    } moon;
    // Aurora borealis (little_planet_plan.md Stage 8). A green/teal curtain on
    // the auroral oval about the spin axis; the app maps this to a
    // render::AuroraParams (angles -> radians, shell_r_m = R + height_m, the
    // inertial basis from the celestial core). enabled=0 forces intensity 0.
    struct {
        bool enabled = false;
        double intensity = 0.0;        // curtain gain (>=0)
        double oval_center_rad = 0.0;  // oval colatitude from â [rad]
        double oval_width_rad = 0.0;   // oval band half-width [rad] (>0)
        double curtain_scale = 0.0;    // ribbons per radian of azimuth (>0)
        double height_m = 0.0;         // shell altitude (>0; > max flyable alt)
        double anim_rate = 0.0;        // curtain drift rate [rad/s] (>=0)
        double ground_glow = 0.0;      // night-ground glow under the oval [0,1]
        double haze_suppress = 0.0;    // cloud suppression [0,1]
        glm::dvec3 tint_low{0.0};      // green base RGB
        glm::dvec3 tint_high{0.0};     // teal tips RGB
    } aurora;
    // [map] — the M-KEY tactical chart (S-mapread, Chad 2026-08-09). Colours
    // are RGB [0,1]; sizes are screen pixels. The basemap entries are all
    // NEUTRAL greys by ruling (the plate carries no chroma so nothing on it
    // can camouflage a marker); the tactical entries are the only colour on
    // the map. Mapped 1:1 onto render::MapStyle by app/main.cpp.
    struct {
        glm::dvec3 paper{0.0};
        glm::dvec3 ink{0.0};
        glm::dvec3 water{0.0};
        double basemap_ink = 0.0;        // plate strength [0,1]
        double lake_label_span_m = 0.0;  // min lake span to earn a label (>=0)
        bool trails_visible = false;
        bool minor_roads_visible = false;
        bool rivers_visible = false;
        bool zone_labels_visible = false;
        bool place_labels_visible = false;  // ALL place names (towns/lakes/
                                            // airstrip names). 0 = Chad's ask
        glm::dvec3 neutral_color{0.0};
        glm::dvec3 outline_color{0.0};
        double marker_px = 0.0;
        double objective_px = 0.0;
        double player_px = 0.0;
        double outline_px = 0.0;
        double territory_fill = 0.0;  // [0,1], 0 = outline only
        double territory_outline_px = 0.0;
        double arrow_hold_sin = 0.0;  // [0,1); 0 = guard off
        // ★ L4 — the view + the per-zoom declutter (plan §4.4). Zoom is a
        // MULTIPLE OF FIT (1 = both bubbles on screen), never a px/m scale.
        double zoom_step = 0.0;          // per wheel notch; <=1 = wheel OFF
        double zoom_max = 0.0;           // top of the clamp, in fits
        double follow_zoom = 0.0;        // ABOVE this the chart follows him
        double trails_zoom = 0.0;        // layers come ON at/above these
        double minor_roads_zoom = 0.0;   // zooms IN ADDITION to their bool
        double place_labels_zoom = 0.0;  // (>1 each: fit must not declutter)
    } map;
    // [teams] — the ONE faction palette (S-mapteam, Chad 2026-08-09). Shared by
    // the map tactical layer AND the world draw (aircraft livery, ALLY/BANDIT
    // tags, Chad's callsign tag, pump bodies) via render::set_team_colors(), so
    // a faction hue can never fork between the chart and the sky. `enemy` is
    // ALSO the slag `hot` stop (render/team_color.h kSlagOrange feeds the lava
    // shader), and `ally` must be its EXACT 180 deg HSV complement — the loader
    // enforces that executably, it is not a comment.
    struct {
        glm::dvec3 ally{0.0};       // Chad's side (player-relative)
        glm::dvec3 enemy{0.0};      // the other side = slag orange
        glm::dvec3 objective{0.0};  // pumps: medical-oxygen green (neutral)
    } teams;
    // [pump_frame] — the neon team wire cube framing a pump (S-pumpcube, Chad
    // 2026-08-09). Ownership legibility for the two DEEP pumps, which sit in a
    // sealed unlit chamber with no beacon column. Colours come from [teams]
    // (one palette, never two); only the geometry/strength lives here, plus the
    // unowned/dead grey. See render/pump_frame.h.
    struct {
        bool enabled = false;
        bool deep_only = true;       // stope pumps only (Chad's literal ask)
        double half_extent_m = 0.0;  // cube half-edge (m) > kDeepPumpShellM
        int layers = 0;              // nested wire shells (neon thickness)
        double layer_step_m = 0.0;   // shell spacing (m)
        double brightness = 0.0;     // additive strength of the inner shell
        glm::dvec3 neutral_color{
            0.0};  // unowned / dead frame (never a team hue)
    } pump_frame;
};

WorldParams load_world_toml(const std::string& path);

}  // namespace cfg
