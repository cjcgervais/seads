#include "render/tourist_map.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "raylib.h"
#include "render/bubble_map.h"
#include "render/copper_cliff_geo.h"  // kCcSuperstack (Copper Cliff label anchor)
#include "render/map_font.h"
#include "render/map_style.h"
#include "render/sudbury_gis.gen.h"  // baked lakes/roads/rivers/trails/airstrips
#include "render/sudbury_hero.gen.h"  // kSudburyHeroes[0] (Chelmsford church, label anchor)
#include "world/faction_bubbles.h"  // pump surface anchors (label text only)
#include "world/tunnel_geo.h"       // tunnel mouth dirs (label anchors)

namespace render {
namespace {

// The one style instance (see render/map_style.h). Defaults are the shipped
// look; app/main.cpp overwrites it from config/world.toml [map] at startup.
MapStyle g_map_style{};

// Arial base size: loaded once large and scaled DOWN by DrawTextEx, so every
// map label stays crisp at any size the layers ask for.
constexpr int kMapFontBase = 64;

}  // namespace

void set_map_style(const MapStyle& s) { g_map_style = s; }
const MapStyle& map_style() { return g_map_style; }

const Font& map_font() {
    static Font s_font{};
    static bool s_loaded = false;
    if (!s_loaded) {
        s_loaded = true;
        s_font =
            LoadFontEx("C:/Windows/Fonts/arial.ttf", kMapFontBase, nullptr, 0);
        if (s_font.texture.id == 0)
            s_font = LoadFontEx("C:/Windows/Fonts/arialbd.ttf", kMapFontBase,
                                nullptr, 0);
        if (s_font.texture.id == 0) s_font = GetFontDefault();
        SetTextureFilter(s_font.texture, TEXTURE_FILTER_BILINEAR);
    }
    return s_font;
}

namespace {

// RGB [0,1] (the config convention) -> raylib Color at the given alpha.
inline Color rgb(const glm::dvec3& c, int a = 255) {
    const auto q = [](double v) {
        return static_cast<unsigned char>(
            std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
    };
    return Color{q(c.x), q(c.y), q(c.z), static_cast<unsigned char>(a)};
}

// A neutral grey at lightness `l` (the basemap has NO chroma by ruling — the
// tactical layer owns every coloured pixel on this map).
inline Color grey(double l, int a = 255) { return rgb(glm::dvec3(l, l, l), a); }

// Screen-space projection helper — identical math to draw.cpp's local
// `to_screen` lambda (kept a tiny free function here since this TU is
// separate).
inline Vector2 to_screen_px(const MapProj& mp, const glm::dvec3& dir,
                            double scale_px_per_m, double cx, double cy) {
    const glm::dvec2 p = project_to_map(mp, dir);
    return Vector2{static_cast<float>(cx + p.x * scale_px_per_m),
                   static_cast<float>(cy - p.y * scale_px_per_m)};
}

inline bool in_padded_rect(const Vector2& p, float w, float h, float pad) {
    return p.x >= -pad && p.x <= w + pad && p.y >= -pad && p.y <= h + pad;
}

// Arial label helper (S-mapread): every basemap label goes through here, so
// the face and the tracking are single-sourced.
void label(const char* txt, float x, float y, float size, Color c) {
    DrawTextEx(map_font(), txt, Vector2{x, y}, size, size * 0.06f, c);
}

// Casing-then-fill polyline draw (bold casing first so intersections read
// clean, a thinner fill color on top) — screen-space, culled per-segment.
void draw_polyline_road(const std::vector<Vector2>& pts, float w, float h,
                        Color casing, float casing_w, Color fill,
                        float fill_w) {
    if (pts.size() < 2) return;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        if (!in_padded_rect(pts[i], w, h, 32.0f) &&
            !in_padded_rect(pts[i + 1], w, h, 32.0f))
            continue;
        DrawLineEx(pts[i], pts[i + 1], casing_w, casing);
    }
    if (fill_w <= 0.0f) return;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        if (!in_padded_rect(pts[i], w, h, 32.0f) &&
            !in_padded_rect(pts[i + 1], w, h, 32.0f))
            continue;
        DrawLineEx(pts[i], pts[i + 1], fill_w, fill);
    }
}

// Dashed thin line for trails — screen-space dash cadence (map styling only,
// no physical meaning).
void draw_polyline_dashed(const std::vector<Vector2>& pts, float w, float h,
                          Color color, float line_w, float dash_px,
                          float gap_px) {
    if (pts.size() < 2) return;
    float phase = 0.0f;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const Vector2 a = pts[i];
        const Vector2 b = pts[i + 1];
        if (!in_padded_rect(a, w, h, 32.0f) && !in_padded_rect(b, w, h, 32.0f))
            continue;
        const float seg_len =
            std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        if (seg_len < 1e-3f) continue;
        const Vector2 dir{(b.x - a.x) / seg_len, (b.y - a.y) / seg_len};
        float t = 0.0f;
        while (t < seg_len) {
            const float period = dash_px + gap_px;
            const float local = std::fmod(phase + t, period);
            const float remain_in_period =
                (local < dash_px) ? (dash_px - local) : 0.0f;
            const float step = (remain_in_period > 0.0f)
                                   ? std::min(remain_in_period, seg_len - t)
                                   : std::min(period - local, seg_len - t);
            if (remain_in_period > 0.0f) {
                const Vector2 p0{a.x + dir.x * t, a.y + dir.y * t};
                const Vector2 p1{a.x + dir.x * (t + step),
                                 a.y + dir.y * (t + step)};
                DrawLineEx(p0, p1, line_w, color);
            }
            t += step;
        }
        phase = std::fmod(phase + seg_len, dash_px + gap_px);
    }
}

void draw_lakes(const MapStyle& st, const MapProj& mp, float w, float h,
                double scale_px_per_m, double cx, double cy) {
    const Color fill = rgb(st.water);
    for (std::size_t li = 0; li < kSudburyWaterLakeCount; ++li) {
        const GisWaterLake& L = kSudburyWaterLakes[li];
        if (L.vtx_count < 3 || L.idx_count < 3) continue;
        for (int k = 0; k + 2 < L.idx_count; k += 3) {
            const int i0 = kSudburyWaterIndices[L.idx_off + k];
            const int i1 = kSudburyWaterIndices[L.idx_off + k + 1];
            const int i2 = kSudburyWaterIndices[L.idx_off + k + 2];
            const glm::dvec3 d0(kSudburyWaterVerts[L.vtx_off + i0].dir[0],
                                kSudburyWaterVerts[L.vtx_off + i0].dir[1],
                                kSudburyWaterVerts[L.vtx_off + i0].dir[2]);
            const glm::dvec3 d1(kSudburyWaterVerts[L.vtx_off + i1].dir[0],
                                kSudburyWaterVerts[L.vtx_off + i1].dir[1],
                                kSudburyWaterVerts[L.vtx_off + i1].dir[2]);
            const glm::dvec3 d2(kSudburyWaterVerts[L.vtx_off + i2].dir[0],
                                kSudburyWaterVerts[L.vtx_off + i2].dir[1],
                                kSudburyWaterVerts[L.vtx_off + i2].dir[2]);
            const Vector2 p0 = to_screen_px(mp, d0, scale_px_per_m, cx, cy);
            const Vector2 p1 = to_screen_px(mp, d1, scale_px_per_m, cx, cy);
            const Vector2 p2 = to_screen_px(mp, d2, scale_px_per_m, cx, cy);
            if (!in_padded_rect(p0, w, h, 16.0f) &&
                !in_padded_rect(p1, w, h, 16.0f) &&
                !in_padded_rect(p2, w, h, 16.0f))
                continue;
            DrawTriangle(p0, p1, p2, fill);
        }
    }
}

// Max plausible gap between two consecutive centerline STATIONS within one
// real segment. Chosen generously above the observed nominal station
// spacing (single/low-double-digit metres near dense curves, up to a few
// hundred metres on long straight stretches) so a real segment's own
// spacing variance never trips a false split, while still catching the
// splice between two concatenated real-world polylines (which lands far
// larger — kilometres) or an `s`-non-decreasing splice edge case. Verified
// visually (no spurious cross-map lines) in the smoke shots this mechanism
// was iterated against.
constexpr double kMaxStationGapM = 3000.0;

// DECLUTTER (S-mapread): the basemap is now a NEWSPAPER PLATE — a hierarchy
// of neutral greys with only the top tier (major roads) drawn as a cased
// line. Everything below is a hairline, and the two densest layers (the 176
// dashed snowmobile trails, and the full river web) are dialable off; trails
// default OFF because on the before-shot they were the single largest source
// of visual noise under the tactical markers.
void draw_ribbons(const MapStyle& st, const MapProj& mp, float w, float h,
                  double scale_px_per_m, double cx, double cy) {
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (P.vtx_count < 4) continue;
        if (P.kind == 1 && !st.minor_roads_visible) continue;
        if (P.kind == 2 && !st.trails_visible) continue;
        if (P.kind == 3 && !st.rivers_visible) continue;

        std::vector<CenterlineSample> centerline;
        centerline.reserve(static_cast<std::size_t>(P.vtx_count) / 2);
        for (int i = 0; i + 1 < P.vtx_count; i += 2) {
            const GisRibbonVertex& a = kSudburyRibbonVerts[P.vtx_off + i];
            const GisRibbonVertex& b = kSudburyRibbonVerts[P.vtx_off + i + 1];
            const glm::dvec3 da(a.dir[0], a.dir[1], a.dir[2]);
            const glm::dvec3 db(b.dir[0], b.dir[1], b.dir[2]);
            CenterlineSample s;
            s.dir = glm::normalize(0.5 * (da + db));
            s.s = 0.5 * (static_cast<double>(a.s) + static_cast<double>(b.s));
            centerline.push_back(s);
        }
        const std::vector<std::vector<glm::dvec3>> polylines =
            split_into_polylines(centerline, mp.R, kMaxStationGapM);

        for (const std::vector<glm::dvec3>& poly : polylines) {
            std::vector<Vector2> pts;
            pts.reserve(poly.size());
            for (const glm::dvec3& d : poly)
                pts.push_back(to_screen_px(mp, d, scale_px_per_m, cx, cy));

            switch (P.kind) {
                case 0:  // road_major — the only cased line on the plate
                    draw_polyline_road(pts, w, h, rgb(st.ink), 4.2f, grey(0.48),
                                       1.9f);
                    break;
                case 1:  // road_minor — town grid, hairline only
                    draw_polyline_road(pts, w, h, grey(0.52, 170), 1.1f, BLANK,
                                       0.0f);
                    break;
                case 2:  // trail — dashed hairline (off by default)
                    draw_polyline_dashed(pts, w, h, grey(0.55, 150), 1.0f, 5.0f,
                                         5.0f);
                    break;
                case 3:  // river — thin, uncased (was a fat 3.4px casing)
                    draw_polyline_road(pts, w, h, grey(0.60, 190), 1.2f, BLANK,
                                       0.0f);
                    break;
                default:
                    break;
            }
        }
    }
}

// Simple label anti-overlap: a label is dropped if its anchor lands within
// `kLabelMinSepPx` of an already-placed one. The before-shot had "Belanger
// airstrip (behind Belanger Ford)" printed straight through "Greater Sudbury
// Airport", and "Matagamasi Lake" through "Kukagami Lake" — this is the
// declutter that fixes it without deleting the layer.
constexpr float kLabelMinSepPx = 54.0f;

// DATA-QUALITY label filter (S-mapread): a handful of baked OSM features
// carry a raw way-id instead of a name ("highway w246254652", seen printed
// across Copper Cliff on the before/after shots). That is noise, not a place
// name — drop anything carrying a run of 5+ consecutive digits. Real Sudbury
// place names never do.
bool label_is_junk(const char* s) {
    int run = 0;
    for (const char* p = s; *p; ++p) {
        if (*p >= '0' && *p <= '9') {
            if (++run >= 5) return true;
        } else {
            run = 0;
        }
    }
    return false;
}

bool claim_label_slot(std::vector<Vector2>& placed, const Vector2& p) {
    for (const Vector2& q : placed) {
        const float dx = q.x - p.x, dy = q.y - p.y;
        if (dx * dx + dy * dy < kLabelMinSepPx * kLabelMinSepPx) return false;
    }
    placed.push_back(p);
    return true;
}

void draw_labels(const MapStyle& st, const MapProj& mp, float w, float h,
                 double scale_px_per_m, double cx, double cy) {

    // AIRSTRIP DOTS always draw: an airstrip is a place you can PUT A PLANE
    // DOWN, i.e. tactical furniture, and a dot is a symbol, not a name. Kept
    // outside the place-name gate below so the purge removes text only.
    for (std::size_t i = 0; i < kSudburyAirstripCount; ++i) {
        const GisAirstrip& A = kSudburyAirstrips[i];
        const glm::dvec3 d(A.anchor[0], A.anchor[1], A.anchor[2]);
        const Vector2 p = to_screen_px(mp, d, scale_px_per_m, cx, cy);
        if (!in_padded_rect(p, w, h, 0.0f)) continue;
        DrawCircleV(p, 3.0f, rgb(st.ink, 230));
    }

    // THE PLACE-NAME PURGE (S-mapteam, Chad 2026-08-09: "rEMOVE ALL THE PLACE
    // NAMES FROM THE MAP PLEASE"). Towns, pump-zone text, airstrip names and
    // lake names — every place NAME on the chart — behind ONE dial, default
    // off. Nothing below is deleted: `[map] place_labels_visible = 1` restores
    // the whole layer byte-for-byte. The compass rose, the scale bar and the
    // "GREATER SUDBURY / tactical chart" title block are chart FURNITURE, not
    // place names, and are drawn elsewhere (draw_compass_and_title) — they
    // survive by design, as do the map's TACTICAL tags (ALLIED AIR / ENEMY
    // AIR), which are drawn by the tactical layer in draw.cpp.
    if (!st.place_labels_visible) return;

    const Color label_c = rgb(st.ink, 230);
    const float fs_lake = 14.0f;
    const float fs_place = 17.0f;

    // Place-name tier FIRST so the (more important) named towns win every
    // overlap contest against the lake tier below.
    std::vector<Vector2> placed;

    struct Place {
        glm::dvec3 dir;
        const char* name;
    };
    const Place places[] = {
        {glm::dvec3(kSudburyHeroes[0].dir[0], kSudburyHeroes[0].dir[1],
                    kSudburyHeroes[0].dir[2]),
         "CHELMSFORD"},
        {kCcSuperstack, "COPPER CLIFF"},
        {world::kTunnelMouthErrington, "ERRINGTON"},
        {world::kTunnelMouthMurray, "MURRAY"},
    };
    for (const Place& pl : places) {
        const Vector2 p = to_screen_px(mp, pl.dir, scale_px_per_m, cx, cy);
        if (!in_padded_rect(p, w, h, 0.0f)) continue;
        if (!claim_label_slot(placed, p)) continue;
        label(pl.name, p.x + 8.0f, p.y - 8.0f, fs_place, label_c);
    }

    if (st.zone_labels_visible) {
        const Place zones[] = {
            {world::kPumpValleySurface, "Onaping / Dowling"},
            {world::kPumpSudburySurface, "Coniston"},
        };
        for (const Place& z : zones) {
            const Vector2 p = to_screen_px(mp, z.dir, scale_px_per_m, cx, cy);
            if (!in_padded_rect(p, w, h, 0.0f)) continue;
            if (!claim_label_slot(placed, p)) continue;
            label(z.name, p.x + 8.0f, p.y + 12.0f, fs_lake, label_c);
        }
    }

    // Airstrips (flight-relevant, so they outrank the lake tier).
    for (std::size_t i = 0; i < kSudburyAirstripCount; ++i) {
        const GisAirstrip& A = kSudburyAirstrips[i];
        if (label_is_junk(A.name)) continue;
        const glm::dvec3 d(A.anchor[0], A.anchor[1], A.anchor[2]);
        const Vector2 p = to_screen_px(mp, d, scale_px_per_m, cx, cy);
        if (!in_padded_rect(p, w, h, 0.0f)) continue;
        if (!claim_label_slot(placed, p)) continue;
        label(A.name, p.x + 7.0f, p.y + 3.0f, fs_lake, label_c);
    }

    // Named lakes above the size threshold (the dial Chad can raise/lower).
    for (std::size_t i = 0; i < kSudburyLakeCount; ++i) {
        const GisLake& L = kSudburyLakes[i];
        if (!lake_label_eligible(L.span_m, st.lake_label_span_m)) continue;
        if (label_is_junk(L.name)) continue;
        const glm::dvec3 d(L.dir[0], L.dir[1], L.dir[2]);
        const Vector2 p = to_screen_px(mp, d, scale_px_per_m, cx, cy);
        if (!in_padded_rect(p, w, h, 0.0f)) continue;
        if (!claim_label_slot(placed, p)) continue;
        label(L.name, p.x + 6.0f, p.y - 6.0f, fs_lake, grey(0.34, 220));
    }
}

void draw_compass_and_title(const MapStyle& st, int sw, int /*sh*/,
                            int margin) {
    const Color ink = rgb(st.ink, 225);
    // Compass rose: top-right, well clear of the top-left score stack and
    // the bottom-left scale bar. Greyscale (the rose's old red needle was
    // chroma competing with the enemy markers).
    const float ccx = static_cast<float>(sw - margin - 46);
    const float ccy = static_cast<float>(margin + 46);
    DrawCircleLines(static_cast<int>(ccx), static_cast<int>(ccy), 30.0f, ink);
    DrawLineEx(Vector2{ccx, ccy + 26}, Vector2{ccx, ccy - 26}, 1.5f, ink);
    DrawLineEx(Vector2{ccx - 26, ccy}, Vector2{ccx + 26, ccy}, 1.5f,
               grey(0.45, 180));
    const Vector2 tip{ccx, ccy - 26}, l{ccx - 7, ccy - 12},
        r{ccx + 7, ccy - 12};
    DrawTriangle(tip, l, r, ink);
    label("N", ccx - 5.0f, ccy - 48.0f, 16.0f, ink);

    // Title/legend block beneath the rose.
    const float tx = static_cast<float>(sw - margin - 200);
    const float ty = static_cast<float>(margin + 84);
    label("GREATER SUDBURY", tx, ty, 20.0f, ink);
    label("tactical chart", tx, ty + 24.0f, 13.0f, grey(0.38, 200));
}

}  // namespace

void draw_tourist_basemap(const MapProj& mp, int sw, int sh,
                          double scale_px_per_m, double cx, double cy,
                          int margin, const MapStyle* style) {
    static RenderTexture2D s_tex{};
    static bool s_valid = false;
    static TouristMapKey s_key{};

    // L4: the caller may hand in a PER-ZOOM style (map_style_at_zoom) so a
    // zoomed-in chart can carry layers the overview declutters away. Null =
    // the shipped global, which is every pre-L4 call site unchanged.
    const MapStyle& st = (style != nullptr) ? *style : g_map_style;

    const TouristMapKey key{sw,
                            sh,
                            scale_px_per_m,
                            cx,
                            cy,
                            mp.center.x,
                            mp.center.y,
                            mp.center.z,
                            map_declutter_bits(st)};

    if (sw <= 0 || sh <= 0) return;

    if (!s_valid || key != s_key) {
        if (s_valid) UnloadRenderTexture(s_tex);
        s_tex = LoadRenderTexture(sw, sh);
        if (s_tex.id == 0) {
            s_valid = false;
            return;
        }
        s_valid = true;
        s_key = key;

        BeginTextureMode(s_tex);
        ClearBackground(Color{0, 0, 0, 0});
        // Newsprint stock across the whole map rect (the rect IS the whole
        // render target here — the caller only blits the texture inside its
        // own letterboxed backdrop).
        DrawRectangle(0, 0, sw, sh, rgb(st.paper));
        // Very cheap paper vignette: darken toward the edges a touch.
        DrawRectangleGradientV(0, 0, sw, static_cast<int>(sh * 0.08),
                               Color{0, 0, 0, 34}, Color{0, 0, 0, 0});
        DrawRectangleGradientV(0, static_cast<int>(sh * 0.92), sw,
                               static_cast<int>(sh * 0.08), Color{0, 0, 0, 0},
                               Color{0, 0, 0, 34});

        draw_lakes(st, mp, static_cast<float>(sw), static_cast<float>(sh),
                   scale_px_per_m, cx, cy);
        draw_ribbons(st, mp, static_cast<float>(sw), static_cast<float>(sh),
                     scale_px_per_m, cx, cy);
        draw_labels(st, mp, static_cast<float>(sw), static_cast<float>(sh),
                    scale_px_per_m, cx, cy);
        draw_compass_and_title(st, sw, sh, margin);
        EndTextureMode();
    }

    if (!s_valid) return;
    // RenderTexture2D's colour attachment is Y-flipped vs screen space —
    // draw it back with a negative-height source rect (the standard raylib
    // render-texture blit idiom).
    const Rectangle src{0.0f, 0.0f, static_cast<float>(s_tex.texture.width),
                        -static_cast<float>(s_tex.texture.height)};
    const Rectangle dst{0.0f, 0.0f, static_cast<float>(sw),
                        static_cast<float>(sh)};
    DrawTexturePro(s_tex.texture, src, dst, Vector2{0, 0}, 0.0f, WHITE);
    // `basemap_ink` = ONE knob that pushes the whole printed plate back
    // behind the tactical layer, without re-baking the cache. Implemented as
    // a PAPER-coloured veil over the plate (NOT as blit alpha — fading the
    // blit would reveal the near-black overlay backdrop underneath and turn
    // the sheet dark, which would LOWER the contrast this whole pass exists
    // to raise). Veiling with paper keeps the sheet white and fades only the
    // ink, exactly like a screened-back newspaper underlay.
    const double veil =
        1.0 - std::min(1.0, std::max(0.0, st.basemap_ink));
    if (veil > 0.0) {
        DrawRectangle(0, 0, sw, sh,
                      rgb(st.paper,
                          static_cast<int>(std::lround(veil * 255.0))));
    }
}

}  // namespace render
