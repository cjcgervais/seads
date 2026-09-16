#pragma once

// M-KEY MAP STYLE (S-mapread, Chad 2026-08-09): "The map is too cluttered.
// Need better contrast for objective markers and player markers, proper color
// needed for the markers. Let make allies green and change my color to green
// as well. Make the enemies red (and their markers red). Make sure that there
// are not conflicting or camouflaging colors on the map. Use arial font,
// newspaper greyscale for the look but objectives and playermarkers properly
// color coded."
//
// The one place every map tunable lives (CLAUDE.md's "no bare numeric gains
// in render/" rule, applied to the map's look): loaded from config/world.toml
// [map] by config/load_world.cpp and pushed ONCE at startup by app/main.cpp
// via set_map_style(). Both map TUs (render/tourist_map.cpp for the greyscale
// basemap, render/draw.cpp for the tactical layer) read the SAME struct
// through map_style(), so a colour can never fork between the two layers.
//
// Plain POD (glm + bool/double only, no raylib) so this header stays linkable
// anywhere; the accessor pair is implemented in render/tourist_map.cpp, which
// is only linked into the `seads` app target. Colours are RGB in [0,1]
// (the codebase's require_vec3 convention); alphas/fractions in [0,1]; sizes
// in screen pixels.
//
// ⚠ THE TEAM HUES DO NOT LIVE HERE (S-mapteam, 2026-08-09). Ally blue, enemy
// slag orange and the objective oxygen green are shared with the WORLD draw
// (aircraft livery, in-game name tags, pump bodies), so they live in the one
// cross-layer table `render::team_colors()` (render/team_color.h). This struct
// keeps only the map's own chrome — the greyscale plate, the neutral-contact
// grey, the halo, the sizes. Two parallel colour tables is exactly the fork
// CLAUDE.md's H1 rule forbids.
//
// COLOUR-BLIND SAFETY (the design ruling): orange/blue is the safest common
// pairing (it replaced green/red, the most-confused one). Hue is still never
// the only cue — every marker class owns its own SHAPE (ally = circle,
// enemy = square, objective = P&ID pump glyph, player = arrow) and the three
// tactical hues are separated in LIGHTNESS too (objective green luma ~0.39,
// ally blue ~0.51, enemy orange ~0.61). The basemap carries NO chroma at all,
// so nothing on it can camouflage a marker.

#include <glm/glm.hpp>

namespace render {

struct MapStyle {
    // ---- basemap (newspaper greyscale; all neutral, zero chroma) ----
    glm::dvec3 paper{0.900, 0.898, 0.892};  // newsprint stock
    glm::dvec3 ink{0.130, 0.130, 0.135};    // darkest printed ink
    glm::dvec3 water{0.738, 0.740, 0.744};  // lake/river grey plate
    double basemap_ink = 0.92;              // overall basemap opacity [0,1]
    double lake_label_span_m = 8000.0;      // label only lakes this big
    bool trails_visible = false;       // 176 dashed trails: the worst clutter
    bool minor_roads_visible = true;   // town street grid (light hairline)
    bool rivers_visible = true;        // river network (thin, no casing)
    bool zone_labels_visible = false;  // pump-zone place names (redundant)
    // S-mapteam (Chad 2026-08-09, verbatim): "rEMOVE ALL THE PLACE NAMES FROM
    // THE MAP PLEASE". The whole place-NAME layer — towns, airstrip names,
    // lake names — behind one dial, default OFF. Nothing is deleted from the
    // data: flip this to 1 and every name comes back exactly as before. The
    // airstrip DOTS, the compass rose, the scale bar and the title block are
    // chart FURNITURE, not place names, and are unaffected.
    bool place_labels_visible = false;

    // ---- map chrome (the team hues live in render/team_color.h) ----
    glm::dvec3 neutral_color{0.45, 0.45, 0.47};  // unfactioned contact
    glm::dvec3 outline_color{0.04, 0.04, 0.05};  // the contrast halo

    double marker_px = 5.5;       // ally/enemy/neutral marker half-size
    double objective_px = 9.0;    // objective pump-glyph radius (the P&ID
                                  // circle); bigger than a contact marker so
                                  // the wedge + base read at chart scale
    double player_px = 16.0;      // player arrow half-length (nose to tail)
    double outline_px = 2.2;      // halo thickness around every marker
    double territory_fill = 0.0;  // team wash inside a bubble [0,1];
                                  // 0 = outline only (no camouflage)
    double territory_outline_px = 3.0;  // bubble boundary stroke

    // Arrow degeneracy guard: hold the last valid bearing while the nose is
    // within asin(arrow_hold_sin) of straight up/down. 0 = hold disabled.
    double arrow_hold_sin = 0.15;

    // ---- L4: THE DECLUTTER IS NOW A ZOOM, NOT A BOOL --------------------
    // (Chad 2026-09-01: "update the map to have less clutter and more
    // accurate objectives, zoomable via mousewheel, better local scale".)
    //
    // * THE THRESHOLDS ADD, THEY NEVER SUBTRACT. Each `*_zoom` is the zoom
    // (in multiples of FIT) at or above which that layer comes ON in ADDITION
    // to its own visible bool -- `on = visible || zoom >= zoom_threshold`.
    // That is what keeps the two rulings compatible: Chad's S-mapread purge
    // ("trails are the worst clutter", "REMOVE ALL THE PLACE NAMES") is about
    // the chart at FIT, where you are looking at 30 km of ground and the names
    // are noise; his L4 ask is about the chart at 8x, where you are looking at
    // one road junction and the names are the point. A layer he turned off
    // stays off on the overview and comes back when you go looking for it.
    //
    // WARNING: THE DEFAULTS ARE CHOSEN SO ZOOM == FIT DRAWS EXACTLY WHAT IT
    // DREW BEFORE THIS RUNG. At zoom 1: trails false||1>=4 = false, roads
    // true||1>=4 = true, place labels false||1>=8 = false -- the same three
    // answers as today, which is the leg test_map_screen.cpp pins. Set
    // trails_zoom to 1.0 and that leg goes red; that is the mutation it
    // exists for.
    double trails_zoom = 4.0;        // 176 dashed sled trails: local scale only
    double minor_roads_zoom = 4.0;   // already on by default; this only bites
                                     // if minor_roads_visible is dialled off
    double place_labels_zoom = 8.0;  // names return when a name is what you
                                     // are actually reading

    // ---- L4: THE VIEW ---------------------------------------------------
    // Wheel step per notch (geometric, like the freelook dolly: every notch is
    // the same PERCENTAGE, so the steps are fine at fit and coarse at 64x),
    // the top of the clamp in multiples of fit (the bottom is 1 = fit, by
    // construction -- there is nothing to see further out than both bubbles),
    // and the zoom ABOVE which the view stops being a chart and starts being
    // a follow-cam.
    double zoom_step = 1.25;
    double zoom_max = 64.0;
    double follow_zoom = 4.0;
};

// * THE PER-ZOOM STYLE, AS A PURE FUNCTION OF THE SHIPPED ONE. Returns a COPY
// with the three zoom-gated visibility bools resolved for `zoom`; every other
// field is carried through untouched. Pure and raylib-free on purpose -- this
// is the half of the declutter a gate can execute (render/map_screen.cpp,
// which draws it, is in the `seads` target only and no ctest runs the exe).
inline MapStyle map_style_at_zoom(const MapStyle& base, double zoom) {
    MapStyle s = base;
    s.trails_visible = base.trails_visible || zoom >= base.trails_zoom;
    s.minor_roads_visible =
        base.minor_roads_visible || zoom >= base.minor_roads_zoom;
    s.place_labels_visible =
        base.place_labels_visible || zoom >= base.place_labels_zoom;
    return s;
}

// The five bits the tourist basemap's cache key has to carry, so a zoom that
// changes WHAT is drawn can never blit a texture baked with the other answer.
// (The scale itself already moves with zoom, so the key would in practice
// change anyway -- this makes it true by construction instead of by luck.)
inline int map_declutter_bits(const MapStyle& s) {
    return (s.trails_visible ? 1 : 0) | (s.minor_roads_visible ? 2 : 0) |
           (s.rivers_visible ? 4 : 0) | (s.zone_labels_visible ? 8 : 0) |
           (s.place_labels_visible ? 16 : 0);
}

// Set ONCE at startup from config/world.toml [map]. Never called by the
// tests/harness (which never draw), so the defaults above are what they see.
void set_map_style(const MapStyle& s);
const MapStyle& map_style();

}  // namespace render
