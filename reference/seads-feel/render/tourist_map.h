#pragma once

// TOURIST-MAP BASEMAP (Chad's ask, 2026-07-28): "place the map as a layer
// under the tactical bubble map, geographically correct, make it look like a
// tourist map." Pure DISPLAY, drawn UNDER the M-key tactical overlay
// (render/draw.cpp's map_open block) — reads only the already-baked GIS
// constants (render/sudbury_gis.gen.h: lakes/roads/rivers/trails, named
// lakes, airstrips) + a couple of already-baked landmark directions
// (world/tunnel_geo.h, render/sudbury_hero.gen.h, render/copper_cliff_geo.h).
// Writes nothing, feeds nothing back into mouse->aim/control — same firewall
// class as bubble_map.h.
//
// This header is kept RAYLIB-FREE (glm + std only) so its geometry math is
// linkable into seads_tests without pulling in the GPU, mirroring
// render/bubble_map.h's split. The actual screen-space bake (RenderTexture2D
// cache + DrawTriangle/DrawLineEx/DrawText calls) lives in
// render/tourist_map.cpp, which is only added to the `seads` app target.
//
// RIBBON CENTERLINES: a baked GisRibbonPath is a TRIANGLE-STRIP-style buffer
// — consecutive vertex PAIRS are the left (v=-1) / right (v=+1) edge of one
// cross-section station, walked in path order (verified against
// render/ribbons.cpp's build_path_mesh, the same buffer's 3D consumer). The
// station's centerline point is the midpoint of that pair. A single
// GisRibbonPath batch concatenates MANY real-world disjoint polylines (the
// offline bake's own comment on GisRibbonPath) — splitting them apart is
// this header's one nontrivial job (split_into_polylines).

#include <cmath>
#include <vector>

#include <glm/glm.hpp>

#include "render/bubble_map.h"  // MapProj (the shared aeqd projection frame)
#include "render/map_style.h"   // MapStyle + the per-zoom declutter (L4)

namespace render {

// One centerline station: the sampled unit direction (midpoint of a ribbon
// path's L/R edge pair) and that path's own cumulative arc-length `s` at
// this station.
struct CenterlineSample {
    glm::dvec3 dir{0.0, 0.0, 0.0};
    double s = 0.0;
};

// Splits ONE ribbon path's sequential centerline samples into disjoint
// polylines. The GIS bake concatenates multiple real-world disjoint
// road/river/trail segments into a single path buffer; `s` resets toward 0
// at the start of each new segment, and even where a splice doesn't produce
// a clean reset, a huge physical jump between consecutive stations is never
// a real drawn segment. A sample starts a NEW polyline whenever:
//   (a) its `s` is LESS than the previous sample's `s` (a new segment
//       began), or
//   (b) the great-circle distance from the previous sample's dir exceeds
//       `max_gap_m` (guards a same-direction splice the `s` reset alone
//       wouldn't catch).
// Polylines shorter than 2 points are dropped (nothing to draw). Empty
// input returns an empty vector.
inline std::vector<std::vector<glm::dvec3>> split_into_polylines(
    const std::vector<CenterlineSample>& centerline, double R,
    double max_gap_m) {
    std::vector<std::vector<glm::dvec3>> out;
    std::vector<glm::dvec3> cur;
    double last_s = 0.0;
    bool have_last = false;

    for (const CenterlineSample& c : centerline) {
        bool split = false;
        if (have_last) {
            if (c.s < last_s) {
                split = true;
            } else {
                const double dp =
                    glm::clamp(glm::dot(cur.back(), c.dir), -1.0, 1.0);
                const double gap = R * std::acos(dp);
                if (gap > max_gap_m) split = true;
            }
        }
        if (split) {
            if (cur.size() >= 2) out.push_back(cur);
            cur.clear();
        }
        cur.push_back(c.dir);
        last_s = c.s;
        have_last = true;
    }
    if (cur.size() >= 2) out.push_back(cur);
    return out;
}

// Size-threshold label culling: only named lakes at/above `threshold_m`
// span get a map label (keeps the small-lake clutter off a tourist map that
// has 250 baked named lakes but room for maybe a dozen labels).
inline bool lake_label_eligible(double span_m, double threshold_m) {
    return span_m >= threshold_m;
}

// A screen-space cache key: the basemap bake is only ever invalidated by a
// change to one of these (window size, the live projection scale/pan, or
// the map's plane-center direction) — never per-frame. Plain doubles (no
// raylib types) so the key stays raylib-free and comparable with `==`.
struct TouristMapKey {
    int sw = -1;
    int sh = -1;
    double scale_px_per_m = 0.0;
    double cx = 0.0;
    double cy = 0.0;
    double center_x = 0.0;
    double center_y = 0.0;
    double center_z = 0.0;
    // ★ L4: WHICH LAYERS the bake was made with (render::map_declutter_bits).
    // The zoom-gated declutter changes WHAT is drawn, not just where, and a
    // cache key that only knew the scale would be relying on "the scale always
    // moves too" -- true today, and exactly the kind of luck this repo has
    // paid for. -1 = the pre-L4 default (nothing baked yet).
    int declutter_bits = -1;
};

inline bool operator==(const TouristMapKey& a, const TouristMapKey& b) {
    return a.sw == b.sw && a.sh == b.sh &&
           a.scale_px_per_m == b.scale_px_per_m && a.cx == b.cx &&
           a.cy == b.cy && a.center_x == b.center_x &&
           a.center_y == b.center_y && a.center_z == b.center_z &&
           a.declutter_bits == b.declutter_bits;
}
inline bool operator!=(const TouristMapKey& a, const TouristMapKey& b) {
    return !(a == b);
}

// Draws the cached parchment basemap (lakes/roads/rivers/trails/labels/
// compass/title) into the CURRENT raylib render target, blitting a cached
// RenderTexture2D and rebuilding it only when the (screen size, projection
// scale/pan, map-plane center) key changes. Implemented in
// render/tourist_map.cpp (raylib calls; only linked into the `seads` app
// target — this declaration alone stays raylib-free so seads_tests can
// still include this header for the pure geometry helpers above without
// pulling in the GPU). `margin` matches the caller's existing HUD margin so
// the compass/title block can stay clear of the corner score stack.
// `style` (L4) overrides the global map_style() for this bake -- the caller
// passes render::map_style_at_zoom(map_style(), zoom) so a zoomed chart can
// carry trails / names the overview declutters away. Null = the global, which
// is every call site that existed before L4, unchanged.
void draw_tourist_basemap(const MapProj& mp, int sw, int sh,
                          double scale_px_per_m, double cx, double cy,
                          int margin, const MapStyle* style = nullptr);

}  // namespace render
