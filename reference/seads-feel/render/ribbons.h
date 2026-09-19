#pragma once
// S3 draped LINEWORK ribbons (stereoscope-sudbury): roads + snowmobile trails
// as draped triangle strips on the planet's own height field. Consumes the
// baked polylines in render/sudbury_gis.gen.h (unit dirs + arc-length s +
// transverse v). Roads are MONO (the S1 silver post tones them); the snowmobile
// trail is the ONE sanctioned world-chroma accent (light green passes the
// saturation-gated post). render/ reads state, writes nothing, reads no clock —
// house law.
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/bank_mesh.h"     // SF2-BANKS: strip geometry (pure core)
#include "render/sphere_param.h"  // HeightField + facet_radius_at (the R4d drape)
#include "world/linework.h"       // WINTER S2: the promoted surface linework

namespace render {

// FELT look dials, from config/world.toml [ribbons] via draw.cpp (no bare look-
// constants in GLSL — house law). Colors are RGB [0,1].
struct RibbonLook {
    float lift_m = 2.0f;         // clearance above the rendered facet (R4d)
    // ★ ROAD-REPAIR / ONAPING SINK: the longest CHORD the drape may span
    // between two draped rungs, metres. The baked GIS rungs are 52.67 m apart
    // at the median and up to 79.91 m, and the mesh between them is FLAT --
    // measured, that puts the drawn deck up to 7.2 m above the surface the
    // machine stands on near the Valley pump ("I went into the road").
    // Subdivision happens at build time in render/ribbon_subdiv.h.
    // 0.0 == the identity: the byte-identical pre-cut mesh.
    double max_seg_m = 0.0;
    // ★ ROAD-REPAIR / ONAPING SINK, the TRANSVERSE half. A baked rung is
    // ONE quad across the WHOLE road (up to ~15 m), so the drawn deck at
    // the CENTRELINE -- where the machine rides -- is the flat chord
    // between the two edges, and no longitudinal split can touch it
    // (measured: 118 of the 133 residual >0.10 m segments near Valley).
    // max_tr_m is the longest chord ACROSS the deck; above 1 the column
    // count is forced EVEN so v = 0 is a real vertex row and the dashed
    // centreline still lands on the centreline. 0.0 == the identity.
    double max_tr_m = 0.0;
    // ★ ROAD-REPAIR F2 -- THE JUNCTION CUT radius (m). At a node where 3 or
    // more drawn ways end, every leg's deck is trimmed back to this radius and
    // ONE cap polygon is emitted over the node, so the crossing is a single
    // surface instead of N overlapping ones. Floored per node at that node's
    // widest leg half-width (a cut inside the widest deck cannot separate the
    // legs). 0.0 == the identity, BY AN EXPLICIT BRANCH: no plan is built, no
    // trim exists and not one vertex moves. See render/ribbon_junction.h.
    double junction_cut_m = 0.0;
    // ★ ROAD-REPAIR F3 -- THE OVER-BANK BIAS, in units of the shipped
    // (-2, -4) deck polygon offset. draw_ribbon_surfaces multiplies its own
    // pair by (1 + b); draw_bank_strips keeps (-2, -4). b > 0 is the only
    // relative depth bias the deck-vs-bank tie has ever had. 0.0 == the
    // identity BY AN EXPLICIT BRANCH -- the shipped call, verbatim.
    double over_bank_bias = 0.0;
    // ★ ROAD-REPAIR rung AA -- the centreline anti-alias width
    // multiplier. The dashed white line is one ternary in the deck FS on the
    // deck's own interpolated (s, v) with no derivative anywhere; once its
    // 0.312 m band is sub-pixel its hard edge is re-decided by 2 cm of eye
    // travel (F3 6.4 -- Chad's "the white line"). ⚠ That happens at ~292 m
    // (935 px/rad at screen centre, kChaseFovyDeg 60 at 1080p), NOT at the
    // rig's 70 m where the stripe is ~4 px wide, and a graze does not narrow
    // the stripe's transverse width -- the red-team fold corrected that claim.
    // This scales the screen-space footprint the stripe and the dash are box-
    // filtered over (an L2 gradient length, not fwidth's L1 sum, so the onset
    // does not swing with the view orientation).
    // 0.0 == the identity BY AN EXPLICIT BRANCH in the FS.
    float line_aa = 0.0f;
    glm::vec3 road_bed{0.10f};   // dark road surface (mono)
    glm::vec3 road_line{0.80f};  // light dashed centerline (mono)
    float road_center_frac =
        0.06f;  // |v| < this == the centerline stripe (thin)
    float road_dash_m = 7.0f, road_gap_m = 10.0f;
    // weathered asphalt ("less perfect"): a LIGHT fade + smooth low-freq
    // mottling, kept subtle so the road stays dark + continuous (no
    // erasure/flicker)
    float road_mottle = 0.14f;       // strength of the lighter mottled patches
    float road_mottle_frac = 0.25f;  // fraction of the length that mottles
    float road_fade = 0.10f;         // global light fade on the asphalt
    // snowmobile trail: a SUBTLE smooth brown earth path. Winter
    // groomer/corduroy lines return later with the weather-dynamics pass.
    glm::vec3 trail_color{0.37f, 0.27f, 0.19f};  // dark brown earth path
    float trail_mottle = 0.15f;  // subtle natural variation on the clay
    // ★ S1 WINTER TRAIL. The trail rendered as a BROWN CLAY EARTH PATH in a
    // permanently-winter world -- literally the "summer-coloured ground" §5
    // forbids. Under winter it is GROOMED SNOW.
    //
    // ★ It is deliberately DARKER and BLUER than the wild snowpack
    // (snow_albedo 0.92), not brighter. Packed snow really is denser, bluer
    // and slightly darker than fresh cover -- and a WHITE trail on a WHITE
    // world is LESS legible than the clay it replaces, which would have
    // shipped a regression that looked like the fix (S1_SPEC RT-1). The trail
    // reads by value contrast + corduroy TEXTURE, never by being bright.
    glm::vec3 trail_winter_color{0.80f, 0.84f, 0.90f};  // packed/groomed snow
    // ★ RULED (Chad, S2 fly): the groomer lines run PARALLEL to the trail's
    // length, not across it -- a drag pan leaves ridges ALONG the direction of
    // grooming. Spacing is therefore metres ACROSS the trail, and it wants to
    // be FINE; the shader anti-aliases analytically (fwidth) so it can be.
    float trail_corduroy = 0.055f;   // groomer line depth [0,1]
    float trail_corduroy_m = 0.28f;  // line spacing ACROSS the trail (m)
    // NOTE: rivers (kind 3) are NOT drawn here — they render as MIRROR water
    // via render/river_surfaces (the planet water branch).
    // build_ribbon_surfaces skips them.
};

struct RibbonSurfaces {
    bool ok = false;
    Shader shader{};
    Material mat{};
    // ★ ROAD-REPAIR: one OR MORE per baked GisRibbonPath. A subdivided road
    // path runs past raylib's unsigned-short index limit, so it is sliced;
    // `kinds` and `half_w_m` are parallel to THIS list, one entry per mesh.
    std::vector<Mesh> meshes;
    long vert_count = 0;  // the vertex bill, for the build log / the doc
    std::vector<int>
        kinds;  // parallel: 0 road_major / 1 road_minor / 2 trail / 3 river
    // Per-mesh half-width (m), the MEDIAN over that batch's own drawn vertex
    // pairs. The corduroy spacing is metres-ACROSS, so it needs a width -- and
    // taking it from the geometry rather than a look-constant is the same
    // INV-6 discipline world/linework.h uses for the physics corridor.
    std::vector<float> half_w_m;
    RibbonLook look;
    int loc_bed = -1, loc_line = -1, loc_center = -1, loc_line_aa = -1;
    int loc_dash = -1, loc_gap = -1, loc_trail = -1;
    int loc_mottle = -1, loc_mottle_frac = -1, loc_fade = -1;
    int loc_tcolor = -1, loc_tmottle = -1;
    int loc_winter = -1, loc_twcolor = -1, loc_cord = -1, loc_cordm = -1;
    int loc_halfw = -1;
};

// T24: the excavation clip (ribbon_indices_outside_cuts) lives in
// render/ribbon_clip.h — header-only and raylib-free so the ctest suite can
// pin it (this TU links raylib; seads_tests links none).

// Build one draped mesh per baked path. Every vertex sits at
// facet_radius_at(dir, subdiv, tiles) + lift — the RENDERED terrain surface
// (the mesh's own interpolation of the SAME height field), so the ribbon reads
// as ON the ground everywhere (R4d). Draping on radius_at was the floating-
// road defect: between mesh vertices the field rides metres above/below the
// facet the screen shows (measured p99 ≈ 6 m at subdiv 200 untiled), so roads
// hovered over dips and drowned under rises. lift_m is now a small CLEARANCE
// above the rendered facet, not a burial-tail guess. subdiv/tiles MUST be the
// live [planet] mesh build values (draw.cpp passes g_planet_cfg's) or the
// drape conforms to a mesh that isn't on screen. texcoords = (s, v).
// `cuts` (T24, default empty = bit-identical): the excavation clip above; a
// path fully swallowed by a cut builds no mesh.
// ★ WINTER S2 (WINTER_LAW §2.3): the baked ribbon centerlines, PROMOTED into
// the neutral world:: module so `sim/` can ask what surface it is on without
// depending on `render/`. Built ONCE, lazily, from the SAME
// kSudburyRibbonVerts this file rasterizes -- the centerline and half-width are
// RECOVERED from the drawn vertex pairs (world/linework.h), which is how INV-6
// ("the render ribbon width and the physics corridor width read the SAME
// number") holds by construction instead of by discipline.
//
// Waterway paths (kind 3) are skipped: a river ribbon is a render surface, not
// a drivable corridor, and it is 60% of the baked vertices.
const world::LineNetwork& sudbury_linework(double R_planet, double u_offset);

// ★ SF2-BANKS (WINTER_LAW §3.6c): the OREO SNOWBANKS -- the plowed banks the
// sled already drives, made visible. Geometry from render/bank_mesh (pure,
// samples the REAL SnowpackField::depth_at -- zero fork); this is the upload +
// oreo-speckle shader + draw pass, drawn with the ribbons' exact eye-relative
// xf / polygon-offset / cull-off block (red-team F7).
struct BankLook {
    glm::vec3 base{0.78f, 0.81f, 0.86f};  // snowbank white: darker/bluer than
                                          // wild pack (white-on-white is
                                          // illegible -- the S1 trail lesson)
    float speckle_density = 0.22f;  // fraction of ~22 cm grains showing gravel
    float speckle_dark = 0.55f;     // how dark a gravel fleck bites [0,1]
    float crest_smudge = 1.2f;      // extra fleck density at the crest band
    // ★ ROAD-REPAIR rung AA -- the gravel-speckle anti-alias width
    // multiplier. The oreo is a hash of 0.22 m grains in surface metres; past
    // one grain per pixel it is a hash of the EYE POSITION, which is 13.5-16.4
    // pp of the road-mask flicker (F3 6.3). This scales the grain footprint the
    // Nyquist gate is taken on. 0.0 == the identity BY AN EXPLICIT BRANCH.
    float speckle_aa = 0.0f;
};

// ★ ROAD-REPAIR: the light the bank strips are shaded and sparkled by. Every
// field is SINGLE-SOURCED at the call site (render/draw.cpp) from the same
// value the planet pass is handed that frame -- the bank owns no light of its
// own, so it cannot fall out of step with the ground it sits on. The sparkle
// lattice itself is render/snow_light_glsl, shared with the planet FS.
struct BankLight {
    glm::vec3 sun_dir{0.0f, -1.0f, 0.0f};   // light-travel: sun -> scene
    glm::vec3 moon_dir{0.0f, -1.0f, 0.0f};  // ditto for the moon
    float moon_fill = 0.0f;                 // phase-shaped moon gain
    float day_gain = 1.6f;                  // [atmosphere] ground_day_gain
    float night_fill = 0.05f;               // [atmosphere] night_fill_min
    glm::vec3 night_glow{0.0f};             // S1b winter-night neutral lift
    float snow_sparkle = 0.4f;              // night glint amplitude
    float snow_sparkle_sharp = 120.0f;
    float sun_sparkle = 0.4f;               // day glint amplitude
    float sun_sparkle_sharp = 120.0f;
    float winter = 1.0f;                    // season snow amount [0,1]
};

struct BankSurfaces {
    bool ok = false;
    Shader shader{};
    Material mat{};
    std::vector<Mesh> meshes;
    BankLook look;
    float crest_v = 0.4f;   // ring-normalized crest position (from the knots)
    float width_m = 9.0f;   // rise + fall (metres across, for grain scale)
    int loc_base = -1, loc_density = -1, loc_dark = -1, loc_smudge = -1;
    int loc_speckle_aa = -1;
    int loc_crest = -1, loc_width = -1;
    // ★ ROAD-REPAIR: the lit path.
    int loc_eye = -1, loc_sun = -1, loc_moon = -1, loc_moon_fill = -1;
    int loc_day_gain = -1, loc_night_fill = -1, loc_night_glow = -1;
    int loc_snow_sparkle = -1, loc_snow_sparkle_sharp = -1;
    int loc_sun_sparkle = -1, loc_sun_sparkle_sharp = -1;
    int loc_lit = -1;  // 0 = the pre-repair flat pass (SEADS_BANK_LIT=0)
};

BankSurfaces build_bank_surfaces(const HeightField& hf, int subdiv, int tiles,
                                 const world::SnowpackField& snow,
                                 const BankBuildParams& p, const BankLook& look,
                                 const std::vector<CutDisk>& cuts);
void unload_bank_surfaces(BankSurfaces& b);
void draw_bank_surfaces(BankSurfaces& b, const glm::dvec3& eye,
                        const BankLight& light);

RibbonSurfaces build_ribbon_surfaces(const HeightField& hf,
                                     const RibbonLook& look, int subdiv,
                                     int tiles,
                                     const std::vector<CutDisk>& cuts = {});
void unload_ribbon_surfaces(RibbonSurfaces& r);

// Draw the ribbon strips. Opaque, depth-on (writes the S6 depth FBO); run after
// the planet + water and before the translucent props. Eye-relative model
// matrix (the double->float seam) + a slope-scaled polygon offset (Fable P1-2
// z-fight).
// `winter` is the season snow amount [0,1] (0 = summer => bit-unchanged look).
void draw_ribbon_surfaces(RibbonSurfaces& r, const glm::dvec3& eye,
                          float winter);

}  // namespace render
