#include "render/map_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/glm.hpp>

#include "raylib.h"
#include "render/bubble_map.h"   // the aeqd projection + ellipse boundary sampler
#include "render/map_font.h"     // the shared Arial face (S-mapread)
#include "render/map_style.h"    // config-driven colours / sizes / declutter
#include "render/team_color.h"   // S-mapteam: the ONE faction palette (map + world)
#include "render/team_kit.h"     // faction_color(): ABSOLUTE Valley blue / Sudbury orange
#include "render/tourist_map.h"  // the greyscale basemap under the tactical layer
#include "world/faction_bubbles.h"  // baked ellipse centres/axes
#include "world/tunnel_geo.h"       // the two tunnel mouth dirs

namespace render {

void draw_map_screen(const sim::SimState& state,
                     const sim::AircraftParams& params, const FrameInfo& info) {
    const int sw = GetScreenWidth();
    const int sh = GetScreenHeight();
    const int margin = 40;

    // ★★★ L4 — THE VIEW. Null map_view is the pre-L4 chart exactly (zoom 1 =
    // fit, centre 0, no follow), so a caller that forwards nothing still gets
    // the old picture. The app owns the struct so the zoom survives closing
    // and reopening the map.
    const MapView vw = (info.map_view != nullptr) ? *info.map_view : MapView{};

    // ★★★ THE ACTIVE BODY. After L1 the player is not always the aeroplane,
    // and a chart whose pointer is welded to the aircraft draws a parked plane
    // while the man walks. `map_body_valid` false falls back to `state` --
    // which IS the aeroplane, i.e. every frame before L4.
    const glm::dvec3 body_pos =
        info.map_body_valid ? info.map_body_pos : state.position;
    const glm::dvec3 body_fwd =
        info.map_body_valid ? info.map_body_fwd
                            : (state.orientation * glm::dvec3{0.0, 0.0, -1.0});
    const glm::dvec3 body_dir = (glm::length(body_pos) > 1e-12)
                                    ? glm::normalize(body_pos)
                                    : glm::dvec3{0.0, 1.0, 0.0};

    // Space-black backdrop, slight alpha so the 3D world dims (not
    // vanishes) behind the map. The map rect below currently fills the
    // whole window, so this only ever shows as a letterbox seam if that
    // ever changes (task ask: parchment INSIDE the map rect, black
    // letterbox OUTSIDE it).
    DrawRectangle(0, 0, sw, sh, Color{4, 6, 12, 235});

    const render::MapProj mp =
        render::make_map_proj(render::default_map_center(), params.R);

    // 2026-07-26 FIX: draw the LIVE (growth-scaled + loader-clamped)
    // ellipses, not the baked base constants. world::faction_ellipse is
    // the SAME geometry build_faction_bubbles feeds the plant, so the
    // drawn outline can never fork from the actual air edge -- and a
    // destroyed pump now visibly shrinks the victim's oval. At the default
    // {1,1} growth this yields the identical curve the baked version drew.
    world::FactionGrowth map_grow[2];
    for (int f = 0; f < 2; ++f) {
        map_grow[f].radius_scale = info.conquest_radius_scale[f];
    }
    render::EllipseParams ev;
    world::faction_ellipse(world::VALLEY, map_grow, ev.center_dir,
                           ev.major_axis, ev.major_radius_m,
                           ev.minor_radius_m);
    render::EllipseParams es;
    world::faction_ellipse(world::SUDBURY, map_grow, es.center_dir,
                           es.major_axis, es.major_radius_m,
                           es.minor_radius_m);

    const std::vector<glm::dvec3> valley_loop =
        render::sample_ellipse_boundary(ev, params.R, 64);
    const std::vector<glm::dvec3> sudbury_loop =
        render::sample_ellipse_boundary(es, params.R, 64);

    // Fit scale: uniform (no stretch), both ellipses + 20% margin visible
    // inside the screen minus the 40px bezel — letterboxed via min(w,h).
    double max_ext = 1.0;
    const auto scan_extent = [&](const std::vector<glm::dvec3>& loop) {
        for (const glm::dvec3& d : loop) {
            const glm::dvec2 p = render::project_to_map(mp, d);
            max_ext = std::max({max_ext, std::abs(p.x), std::abs(p.y)});
        }
    };
    scan_extent(valley_loop);
    scan_extent(sudbury_loop);
    max_ext *= 1.2;

    const double half_w = 0.5 * static_cast<double>(sw - 2 * margin);
    const double half_h = 0.5 * static_cast<double>(sh - 2 * margin);
    // ★ FIT IS STILL COMPUTED EXACTLY AS BEFORE, and it is still the whole
    // definition of zoom 1 -- the view stores a MULTIPLE of this, never a
    // px/m, so a resized window or a shrinking bubble moves the picture and
    // never the stored zoom (render/bubble_map.h's MapView banner).
    const double fit_px_per_m = std::min(half_w, half_h) / max_ext;
    const double scale_px_per_m = fit_px_per_m * vw.zoom;
    const double cx = 0.5 * sw;
    const double cy = 0.5 * sh;
    // ★ ABOVE follow_zoom THE CHART FOLLOWS THE BODY. Below it the map stays
    // centred on the fixed map centre, which is (0,0) in plane metres -- so at
    // zoom 1 the two subtractions below are exactly zero and every screen
    // position is bit-identical to the pre-L4 chart.
    //
    // ⚠ AND THE FOLLOW CENTRE IS QUANTISED TO A PIXEL GRID, WHICH IS NOT A
    // COSMETIC CHOICE. The parchment basemap under this layer is a baked
    // RenderTexture whose cache key INCLUDES the pan (render/tourist_map.h),
    // and the bake walks the entire Sudbury GIS -- 250 lakes and every ribbon
    // path. A centre that moved with the body every frame would therefore
    // re-bake the whole dataset every frame, which is not a slow map, it is a
    // frozen one. Snapping the centre to a 256 px lattice costs the pointer up
    // to 128 px of drift off dead-centre (invisible: it is still the biggest
    // marker on a screen it never leaves) and buys one re-bake per 256 px of
    // travel instead of one per frame.
    constexpr double kFollowQuantumPx = 256.0;
    // ★★★ ST-5 POLISH (Chad 2026-09-05, verdict d): "the zoom (large scale)
    // when in map mode needs to follow the sting when deployed not stay fixed
    // on the snowmachine or plane map marker location."
    //
    // The FOLLOW SUBJECT, and it is the only thing this ruling changes. While
    // the drone is airborne the zoomed chart tracks IT; the moment it is gone
    // the subject falls back to the body, which is every frame before this
    // rung. Deliberately a swap of the subject fed to the EXISTING follow
    // machinery -- the same projection, the same 256 px lattice snap, the same
    // one re-bake per 256 px of travel -- and not a second follow path: two
    // centres for one chart is how the basemap cache key forks.
    //
    // ⚠ AND IT IS GATED ON vw.follow_player, which is a pure read of the zoom
    // (render::map_view_follows). So the SMALL-SCALE overview is untouched:
    // below follow_zoom the centre is still the fixed map centre and the sting
    // is just a glyph moving across a stationary plate, exactly as before.
    const bool follow_sting =
        info.map_sting_glyph && glm::length(info.map_sting_pos) > 1e-12;
    const glm::dvec3 follow_dir =
        follow_sting ? glm::normalize(info.map_sting_pos) : body_dir;
    glm::dvec2 centre_m = vw.centre_m;
    if (vw.follow_player && scale_px_per_m > 0.0) {
        const glm::dvec2 pp = render::project_to_map(mp, follow_dir);
        const double q_m = kFollowQuantumPx / scale_px_per_m;
        centre_m = glm::dvec2(std::round(pp.x / q_m) * q_m,
                              std::round(pp.y / q_m) * q_m);
    }
    // Map Y is north-positive; screen Y is down-positive => flip. The pan/zoom
    // arithmetic itself is render/bubble_map.h's (map_plane_to_screen and its
    // inverse), NOT a second copy written out here: this TU is in the `seads`
    // target and no ctest can execute it, so anything spelled out inside this
    // lambda could only be tested by transcription.
    const render::MapScreen msc{scale_px_per_m, cx, cy, centre_m};
    const auto to_screen = [&](const glm::dvec3& dir) {
        const glm::dvec2 q =
            render::map_plane_to_screen(msc, render::project_to_map(mp, dir));
        return Vector2{static_cast<float>(q.x), static_cast<float>(q.y)};
    };

    // TOURIST-MAP BASEMAP (Chad's ask, 2026-07-28): a geographically-
    // correct parchment layer UNDER the tactical bubble map — real
    // baked lakes/roads/rivers/trails/labels, cached to a RenderTexture2D
    // and only rebuilt when the projection/window key changes (see
    // render/tourist_map.h). Drawn BEFORE the faction ellipses/pumps/
    // mavericks/player arrow so the tactical layer stays on top.
    // ★ THE DECLUTTER IS A FUNCTION OF ZOOM (render/map_style.h). At zoom 1
    // map_style_at_zoom is the identity on all three gated layers, so the
    // overview is the chart Chad signed off; trails arrive at 4x and the place
    // names at 8x, where they are what you are reading rather than noise.
    const render::MapStyle mst =
        render::map_style_at_zoom(render::map_style(), vw.zoom);

    // The basemap wants the screen pixel of the map-plane ORIGIN, which is
    // exactly where map_plane_to_screen puts (0,0) -- asked, not re-derived.
    const glm::dvec2 origin_px =
        render::map_plane_to_screen(msc, glm::dvec2(0.0, 0.0));
    render::draw_tourist_basemap(mp, sw, sh, scale_px_per_m, origin_px.x,
                                 origin_px.y, margin, &mst);

    // ---- TACTICAL LAYER (S-mapread, Chad 2026-08-09) ----
    // The basemap under this is pure NEWSPAPER GREYSCALE (zero chroma by
    // ruling, render/map_style.h), so every coloured pixel below belongs
    // to the tactical read and nothing on the plate can camouflage it.
    // ★★★ Team colour is VALLEY/SUDBURY-ABSOLUTE (Chad's ruling 2026-09-10,
    // verbatim: "Sudbury always has to be the orange team"). It used to be
    // PLAYER-RELATIVE -- his side always the ally blue, the other side always
    // the slag orange -- and the first player ever to fly for Sudbury found
    // the Valley painted orange under him. VALLEY is the blue and SUDBURY the
    // slag orange now, whoever is reading the map. The shipped Valley player
    // sees exactly the map he always saw. Those two hues still come
    // from the ONE cross-layer palette (render/team_color.h), the same one
    // the aircraft livery and the in-game name tags read, so an icon can
    // never disagree with the plane it points at. Hue is still never the
    // only cue: each class owns a SHAPE (ally circle / enemy square /
    // objective P&ID pump glyph / player arrow).
    const render::TeamColors& tc = render::team_colors();
    const auto mcol = [](const glm::dvec3& c, int a) {
        const auto q = [](double v) {
            return static_cast<unsigned char>(
                std::lround(std::min(1.0, std::max(0.0, v)) * 255.0));
        };
        return Color{q(c.x), q(c.y), q(c.z), static_cast<unsigned char>(a)};
    };
    // ⚠ c_ally / c_enemy are the PLAYER-RELATIVE slot names and they are kept
    // ONLY for the handful of things that really are player-relative. Anything
    // that codes a SIDE uses the two ABSOLUTE swatches below, or team_col().
    const Color c_ally = mcol(tc.ally, 255);
    const Color c_enemy = mcol(tc.enemy, 255);
    // ★★★ ABSOLUTE (Chad 2026-09-10: "Sudbury always has to be the orange
    // team"). Named for the GROUND, not for the viewer, so a reader can tell
    // at a glance which kind of colour a line is using.
    const Color c_valley = mcol(render::faction_color(0, tc), 255);
    const Color c_sudbury = mcol(render::faction_color(1, tc), 255);
    const Color c_obj = mcol(tc.objective, 255);
    // Chad wears HIS OWN SIDE'S colour like everyone on it (his ask) -- which
    // under the absolute law is his faction's hue, not a fixed blue. He stays
    // instantly findable by SHAPE and SIZE — the only arrow, the biggest
    // marker, the only one wearing a ring — never by owning a hue of his own.
    const int player_fac = info.conquest_player_faction;
    const Color c_player =
        mcol(render::faction_color(player_fac, tc), 255);
    const Color c_neutral = mcol(mst.neutral_color, 235);
    const Color c_out = mcol(mst.outline_color, 235);
    // ABSOLUTE: faction 1 (SUDBURY) is the orange, faction 0 (VALLEY) the
    // blue, with no reference to who is looking. `player_fac` above is still
    // read by the rest of this file (the FIX/ATTACK labels, the raid blink),
    // which are genuinely player-relative questions -- "is that mine" -- and
    // stay that way.
    const auto team_col = [&](int fac) {
        return fac == 1 ? c_enemy : c_ally;
    };
    const float halo = static_cast<float>(mst.outline_px);
    const Font& mf = render::map_font();
    // One label helper so every string on the map is Arial with a dark
    // halo behind it (contrast against the light plate AND against a
    // marker it may land on).
    const auto mtext = [&](const char* txt, float x, float y, float size,
                           Color c) {
        const Vector2 pos{x, y};
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                if (dx || dy)
                    DrawTextEx(
                        mf, txt,
                        Vector2{pos.x + dx * 1.4f, pos.y + dy * 1.4f}, size,
                        size * 0.06f,
                        Color{c_out.r, c_out.g, c_out.b, 210});
        DrawTextEx(mf, txt, pos, size, size * 0.06f, c);
    };

    const auto draw_region = [&](const std::vector<glm::dvec3>& loop,
                                 Color line_c, const char* tag) {
        if (loop.size() < 3) return;
        std::vector<Vector2> pts;
        pts.reserve(loop.size());
        for (const glm::dvec3& d : loop) pts.push_back(to_screen(d));
        // Territory FILL defaults to OFF (mst.territory_fill = 0): a
        // green/red wash under green/red markers is exactly the
        // camouflage Chad called out. The dial is kept so he can bring a
        // faint wash back if the outline alone reads too thin.
        if (mst.territory_fill > 0.0) {
            const int a =
                static_cast<int>(std::lround(mst.territory_fill * 255.0));
            Vector2 centroid{0.0f, 0.0f};
            for (const Vector2& p : pts) {
                centroid.x += p.x;
                centroid.y += p.y;
            }
            centroid.x /= static_cast<float>(pts.size());
            centroid.y /= static_cast<float>(pts.size());
            const Color fill{line_c.r, line_c.g, line_c.b,
                             static_cast<unsigned char>(a)};
            for (std::size_t i = 0; i < pts.size(); ++i)
                DrawTriangle(centroid, pts[i], pts[(i + 1) % pts.size()],
                             fill);
        }
        // Dark casing under the team stroke => the boundary survives
        // wherever it crosses a dark road or a grey lake.
        const float lw = static_cast<float>(mst.territory_outline_px);
        for (std::size_t i = 0; i < pts.size(); ++i)
            DrawLineEx(pts[i], pts[(i + 1) % pts.size()], lw + 2.0f,
                       Color{c_out.r, c_out.g, c_out.b, 170});
        for (std::size_t i = 0; i < pts.size(); ++i)
            DrawLineEx(pts[i], pts[(i + 1) % pts.size()], lw, line_c);
        // Name the airspace at the loop's northmost point.
        std::size_t top = 0;
        for (std::size_t i = 1; i < pts.size(); ++i)
            if (pts[i].y < pts[top].y) top = i;
        mtext(tag, pts[top].x - 44.0f, pts[top].y - 24.0f, 17.0f, line_c);
    };
    const bool valley_is_mine = (player_fac == 0);
    // ★★★ ABSOLUTE HUE, PLAYER-RELATIVE WORD. The Valley's air is blue and
    // Sudbury's is orange on every chart; which one is called ALLIED and which
    // ENEMY still depends on who is reading it. That split is the whole fix:
    // the WORDS answer "whose is it", the COLOUR says which ground it is.
    draw_region(valley_loop, c_valley,
                valley_is_mine ? "ALLIED AIR" : "ENEMY AIR");
    draw_region(sudbury_loop, c_sudbury,
                valley_is_mine ? "ENEMY AIR" : "ALLIED AIR");

    // The 2 tunnel mouths + a straight line between them (the
    // underground route hint — the vacuum gap corridor above ground has
    // no direct flight path, this is the ONLY connector). Deliberately
    // NEUTRAL DARK: it is infrastructure, not a tactical contact, so it
    // must not read as either team.
    {
        const Vector2 pe = to_screen(world::kTunnelMouthErrington);
        const Vector2 pm = to_screen(world::kTunnelMouthMurray);
        DrawLineEx(pe, pm, 2.0f, Color{c_out.r, c_out.g, c_out.b, 130});
        for (const Vector2& q : {pe, pm}) {
            DrawCircleV(q, 5.5f, c_out);
            DrawCircleV(q, 3.2f, Color{235, 235, 235, 255});
        }
    }

    // OBJECTIVES — the 4 pumps. Chad 2026-08-09: "Make the pump symbols
    // look like pump symbols and make the pump symbols green the same
    // green used for oxygen bottles in an ambulance."
    //
    // THE GLYPH is the standard P&ID / ISA-5.1 CENTRIFUGAL PUMP: a circular
    // casing with the discharge wedge cut out of it, sitting on a base
    // plinth. It is the symbol anyone who has read a plant drawing knows,
    // and at chart scale it survives as three unmistakable cues — round
    // body, a dark notch, a foot — none of which the ally CIRCLE or the
    // enemy SQUARE has. (The old amber diamond read as "generic marker".)
    //
    // THE HUE is medical-oxygen green (CGA C-9 / PANTONE 348 C — see
    // config/world.toml [teams] objective). These are OXYGEN pumps in an
    // atmosphere war, so the ambulance-cylinder green is not decoration,
    // it is the right sign for the thing.
    //
    // OWNERSHIP stays in the RING around the glyph, in the owner's team
    // colour, never in the fill — the pump is a fixed neutral green so the
    // ring is now the ONLY ownership cue and is drawn thicker than before.
    // Dead = hollow paper glyph with an X. Read from the SAME
    // conquest_pumps the 3D pump markers above draw (single-source, live).
    // ROUND 3 (Chad: "I need the pump symbol improved"). The round-2 glyph
    // was convincing at 480 px and a green BLOB at true chart scale, and
    // the reason is a legibility rule, not a drawing bug: at r ~ 9 px an
    // INTERNAL detail cut in near-black out of a dark-green disc has ~0.35
    // luma of contrast over a couple of pixels and simply averages away,
    // while the SILHOUETTE survives any downsample. So the discharge wedge
    // moved from a notch cut INTO the casing to a volute cone standing OUT
    // of it (which is also the truer ISA-5.1 centrifugal-pump symbol — a
    // circle with the discharge trapezoid rising off the top), the plinth
    // became a black baseplate that separates from the green by luma
    // instead of by an edge, and a paper-light shaft dot gives the casing
    // an interior. Four cues now, three of them in the outline:
    //   volute cone on top | round casing | light shaft | black baseplate.
    //
    // `part` selects which pieces to draw so the same builder can lay the
    // dark HALO pass (everything, oversized) under the body pass — the
    // round-2 halo skipped the wedge, which is why the cone would have had
    // no outline.
    // `pad` is the halo pass's outward growth in PIXELS, added to each
    // dimension rather than folded into `r`: a uniform scale would grow the
    // volute cone's height by pad*1.72 and cap the green nozzle with a
    // disproportionate black slab. Padding each offset keeps the outline a
    // constant-width stroke all the way round, which is what an outline is.
    const auto pump_glyph = [&](Vector2 p, float r, float pad, Color body,
                                bool detail) {
        // Discharge volute: an upward-opening cone whose apex is the
        // impeller centre and whose mouth stands PROUD of the rim, so it
        // changes the outline. Drawn FIRST so the casing overlaps its apex.
        //
        // ⚠ raylib's DrawTriangle culls anything not wound COUNTER-
        // CLOCKWISE in screen space (y down). This glyph shipped a silently
        // INVISIBLE wedge twice — once from the wrong winding, once from a
        // "fix" that ROTATED the vertex list instead of REVERSING it (a
        // rotation leaves winding unchanged). The gate cannot see it (no
        // ctest runs seads.exe); only the screenshot can. Sanity rule for
        // this file: a visible screen-space triangle has NEGATIVE
        // (v2-v1) x (v3-v1) z. Both triangles below were verified on a
        // screenshot, not by reasoning.
        const float mw = r * 0.86f + pad;  // volute mouth half-width
        const float mh = r * 1.72f + pad;  // mouth height above the centre
        const float aw = r * 0.30f + pad;  // apex half-width (a stub, not a
                                           // point: a point vanishes)
        const Vector2 v_bl{p.x - aw, p.y};
        const Vector2 v_br{p.x + aw, p.y};
        const Vector2 v_tl{p.x - mw, p.y - mh};
        const Vector2 v_tr{p.x + mw, p.y - mh};
        DrawTriangle(v_bl, v_br, v_tr, body);
        DrawTriangle(v_bl, v_tr, v_tl, body);
        // Baseplate: a squat foot WIDER than the casing (the P&ID
        // pedestal). In the body pass it is drawn in the halo ink, not in
        // the green — a black foot under a green disc separates by LUMA,
        // which is what survives at icon size; a green foot merged with
        // the green disc into one blob, which is what Chad is reporting.
        const float bw = r * 1.55f + pad, bh = r * 0.46f + 2.0f * pad;
        DrawRectangleV(Vector2{p.x - bw, p.y + r * 0.62f - pad},
                       Vector2{2.0f * bw, bh}, detail ? c_out : body);
        DrawCircleV(p, r + pad, body);
        if (!detail) return;
        // Shaft: a paper-light dot at the impeller centre. The ONE interior
        // cue, and it is LIGHT-on-dark (contrast ~0.5 luma against the
        // green) rather than the round-2 dark-on-dark notch.
        DrawCircleV(p, r * 0.30f, mcol(mst.paper, 255));
    };
    for (std::size_t pi = 0; pi < info.conquest_pumps.size(); ++pi) {
        const FrameInfo::ConquestPump& pu = info.conquest_pumps[pi];
        const Vector2 p = to_screen(glm::normalize(pu.pos));
        const float r = static_cast<float>(mst.objective_px);
        // The owner ring must clear the glyph's REAL extent, not the casing
        // radius. Round 2 sized every ring off `r` while the glyph fitted
        // inside `r`; round 3's volute cone reaches 1.72r above centre, so
        // an r-derived ring painted straight over the cone and the whole
        // silhouette improvement was invisible on the chart (it only showed
        // in the zoom, where the ring was proportionally thinner). Derive
        // the ring from the extent — then the glyph can grow without any
        // ring constant having to be re-tuned by hand.
        const float gext = r * 1.80f;  // enclosing radius of the glyph
        const Color owner = team_col(pu.faction);
        // COMPETITIVE rung: the raided pump pulses an AMBER alert ring —
        // amber (the objective's own hue), not red, so an alert can
        // never be misread as an enemy contact.
        if (info.conquest_pump_under_attack &&
            static_cast<int>(pi) == info.conquest_raided_pump &&
            std::fmod(GetTime(), 0.6) < 0.35) {
            DrawRing(p, gext + halo + 8.4f, gext + halo + 12.4f, 0.0f,
                     360.0f, 24, c_obj);
        }
        pump_glyph(p, r, halo, c_out, /*detail=*/false);  // contrast halo
        if (pu.alive) {
            pump_glyph(p, r, 0.0f, c_obj, /*detail=*/true);
        } else {
            pump_glyph(p, r, 0.0f, mcol(mst.paper, 255),
                       /*detail=*/false);
            DrawLineEx(Vector2{p.x - r * 0.6f, p.y - r * 0.6f},
                       Vector2{p.x + r * 0.6f, p.y + r * 0.6f}, 2.0f,
                       c_out);
            DrawLineEx(Vector2{p.x - r * 0.6f, p.y + r * 0.6f},
                       Vector2{p.x + r * 0.6f, p.y - r * 0.6f}, 2.0f,
                       c_out);
        }
        // OWNER RING — now the ONLY ownership cue (the fill is a fixed
        // neutral green), so it is thicker than the old one and sits on
        // its own dark casing so a blue ring survives over a grey lake and
        // an orange ring survives over dark ink.
        DrawRing(p, gext + halo + 1.6f, gext + halo + 6.4f, 0.0f, 360.0f,
                 28, Color{c_out.r, c_out.g, c_out.b, 210});
        DrawRing(p, gext + halo + 2.4f, gext + halo + 5.6f, 0.0f, 360.0f,
                 28, owner);
        if (pu.surface)
            DrawRing(p, gext + halo + 8.2f, gext + halo + 9.7f, 0.0f,
                     360.0f, 28, Color{owner.r, owner.g, owner.b, 150});

        // ★★★ L4 — THE OBJECTIVE, NAMED AND RANGED (Chad 2026-09-01: "more
        // accurate objectives"). Before this rung the chart drew four pumps
        // and left it to the pilot to remember which one was his and which
        // one was broken. Now the two that are actually a JOB say what the
        // job is, how far it is, and which way -- measured from the ACTIVE
        // BODY (aeroplane, machine or man), because "1.2 km" from a parked
        // aeroplane is a lie to a man on foot 4 km away.
        //
        // ⚠ SURFACE ONLY, AND THAT IS A RULING NOT AN OVERSIGHT: the two deep
        // pumps live in the stope, the plan's §4.1/§4.2 do not make them
        // player-repairable this rung, and labelling a job nobody can do is
        // worse than labelling nothing.
        if (pu.surface) {
            const bool mine = pu.faction == player_fac;
            const bool needs_fix = mine && (!pu.alive || pu.hp_frac < 1.0);
            const bool is_target = !mine && pu.alive;
            if (needs_fix || is_target) {
                const render::MapRangeBearing rb = render::map_range_bearing(
                    body_dir, glm::normalize(pu.pos), params.R);
                // Metres up to a kilometre, kilometres past it -- "27915 m"
                // is a number you have to parse, "27.9 km" is a distance you
                // can feel. The bearing is always three digits, because 090
                // is a bearing and 90 is a number.
                char ob[64];
                if (rb.dist_m < 1000.0)
                    std::snprintf(ob, sizeof ob, "%s  %.0f m  %03.0f",
                                  needs_fix ? "FIX" : "ATTACK", rb.dist_m,
                                  rb.bearing_deg);
                else
                    std::snprintf(ob, sizeof ob, "%s  %.1f km  %03.0f",
                                  needs_fix ? "FIX" : "ATTACK",
                                  rb.dist_m / 1000.0, rb.bearing_deg);
                // FIX is the objective green (it is a JOB, not a side);
                // ATTACK wears the TARGET PUMP'S OWN faction hue, absolute --
                // it used to be c_enemy, which painted a Valley target orange
                // for a Central City player.
                const Color oc = needs_fix ? c_obj : team_col(pu.faction);
                // The OWN job pulses; the enemy's does not. One moving thing
                // on the chart at a time, and it is the one that is yours.
                if (!needs_fix || std::fmod(GetTime(), 1.0) < 0.62)
                    mtext(ob, p.x + gext + halo + 12.0f,
                          p.y - 8.0f, 16.0f, oc);
            }
        }
    }

    // ★★★ L5 — FLAK GUNS (Chad 2026-08-27: "a marker for it on the map ...
    // easy to see and distinct"). PORTED VERBATIM IN INTENT from `origin/main`
    // (`render/draw.cpp`'s inline map block, which this file replaced), at the
    // same point in the draw order it held there: after the pump glyphs,
    // before the contacts. It carries no zoom gate, exactly like the pumps --
    // an air-defence position is an objective, not clutter, so it is on the
    // chart at fit as well as at 64x.
    //
    // SILHOUETTE-FIRST (the R3.1 rule): a squat MOUNT WEDGE carrying a proud
    // DIAGONAL BARREL with a muzzle bead -- the only diagonal stroke on the
    // chart, so it cannot be confused with the pump machine (round + cone),
    // the ally circle, the enemy square or the player arrow. Fill = the
    // objective green (the gun is pump-defence furniture -- ONE palette,
    // R2.1); ownership = the thin owner ring, the map's one ownership cue.
    //
    // TRUE-POSITION TRAP: the gun stands 80 m from its pump -- ~2 px at chart
    // scale, i.e. INSIDE the pump's own owner rings. The displacement is
    // `render::map_displace_from` (bubble_map.h), which is where main spelled
    // it out inline; it moved into the raylib-free header so the one rule with
    // a right and a wrong answer -- push it out along the TRUE pump->gun
    // bearing, only when it overlaps -- is a thing `seads_tests` can fail on.
    // This TU is app-only and no ctest links it (the banner in map_screen.h).
    for (const FlakDraw& fg : info.flak_guns) {
        const glm::dvec3 gdir = glm::normalize(fg.pos);
        Vector2 p = to_screen(gdir);
        const float fr =
            static_cast<float>(mst.objective_px) * 0.85f;  // glyph scale
        // find the OWN-faction surface pump it guards (single source: the
        // same conquest_pumps the glyphs above drew)
        int own = -1;
        for (std::size_t pi = 0; pi < info.conquest_pumps.size(); ++pi)
            if (info.conquest_pumps[pi].surface &&
                info.conquest_pumps[pi].faction == fg.faction)
                own = static_cast<int>(pi);
        if (own >= 0) {
            const FrameInfo::ConquestPump& pu = info.conquest_pumps[own];
            const glm::dvec3 pdir = glm::normalize(pu.pos);
            const Vector2 pp = to_screen(pdir);
            const float pump_ext =
                static_cast<float>(mst.objective_px) * 1.80f + halo + 12.4f;
            const float need = pump_ext + fr * 2.2f + 3.0f;
            // The world tangent bearing pump -> gun, carried through the SAME
            // projection two chart points separate by: a small step along it,
            // projected.
            const Vector2 q = to_screen(
                glm::normalize(pdir + (gdir - pdir) * 40.0));
            const glm::dvec2 out = render::map_displace_from(
                glm::dvec2(pp.x, pp.y), glm::dvec2(p.x, p.y),
                glm::dvec2(q.x, q.y), static_cast<double>(need));
            p = Vector2{static_cast<float>(out.x), static_cast<float>(out.y)};
        }
        // The glyph builder: mount wedge + baseplate + diagonal barrel +
        // muzzle bead. `pad` = constant-width halo growth in PIXELS (the R3.1
        // rule -- never a uniform scale).
        const auto flak_glyph = [&](Vector2 c, float r, float pad, Color body) {
            // barrel: 45 deg up-right, standing PROUD of the mount -- THE
            // distinct cue
            DrawLineEx(Vector2{c.x - r * 0.25f - pad, c.y + r * 0.15f},
                       Vector2{c.x + r * 1.15f + pad, c.y - r * 1.25f - pad},
                       r * 0.34f + 2.0f * pad, body);
            DrawCircleV(Vector2{c.x + r * 1.15f + pad, c.y - r * 1.25f - pad},
                        r * 0.24f + pad, body);  // muzzle bead
            // mount wedge (visible winding verified: (v2-v1)x(v3-v1)
            // z-negative in y-down screen space -- the R2.3 cull trap)
            const Vector2 v1{c.x - r * 0.95f - pad, c.y + r * 0.62f + pad};
            const Vector2 v2{c.x + r * 0.95f + pad, c.y + r * 0.62f + pad};
            const Vector2 v3{c.x, c.y - r * 0.45f - pad};
            DrawTriangle(v1, v2, v3, body);
            // black baseplate (luma separation, the R3.1 lesson)
            DrawRectangleV(
                Vector2{c.x - r * 1.1f - pad, c.y + r * 0.62f},
                Vector2{2.0f * (r * 1.1f + pad), r * 0.34f + 2.0f * pad},
                pad > 0.0f ? body : c_out);
        };
        flak_glyph(p, fr, halo, c_out);  // contrast halo pass
        flak_glyph(p, fr, 0.0f, c_obj);  // body pass
        // thin owner ring on its own dark casing (the pump convention, sized
        // to the glyph's real extent)
        const float fext = fr * 1.9f;
        const Color owner = team_col(fg.faction);
        DrawRing(p, fext + halo + 0.8f, fext + halo + 4.0f, 0.0f, 360.0f, 24,
                 Color{c_out.r, c_out.g, c_out.b, 200});
        DrawRing(p, fext + halo + 1.4f, fext + halo + 3.4f, 0.0f, 360.0f, 24,
                 owner);
    }

    // ★★★ L4 — THE BODIES HE IS NOT IN. A snowmachine parked four kilometres
    // back is the whole millwright loop's most losable object, and before this
    // the map had no idea it existed. Deliberately drawn as OUTLINE glyphs in
    // HIS OWN SIDE'S hue, not as contacts: they are places, not planes.
    // (Absolute, 2026-09-10: these are Chad's parked vehicles, so they wear
    // his faction's colour -- blue flying for the Valley, slag orange flying
    // for Central City -- exactly like his arrow and his aeroplane.)
    {
        const auto vehicle_glyph = [&](const Vector2 q, bool triangular,
                                       const char* tag) {
            if (triangular) {
                const Vector2 a{q.x, q.y - 8.0f}, b{q.x - 7.0f, q.y + 6.0f},
                    c{q.x + 7.0f, q.y + 6.0f};
                DrawLineEx(a, b, halo + 1.6f, c_out);
                DrawLineEx(b, c, halo + 1.6f, c_out);
                DrawLineEx(c, a, halo + 1.6f, c_out);
                DrawLineEx(a, b, 1.8f, c_player);
                DrawLineEx(b, c, 1.8f, c_player);
                DrawLineEx(c, a, 1.8f, c_player);
            } else {
                DrawRectangleV(Vector2{q.x - 8.0f, q.y - 5.0f},
                               Vector2{16.0f, 10.0f}, c_out);
                DrawRectangleV(Vector2{q.x - 6.0f, q.y - 3.0f},
                               Vector2{12.0f, 6.0f},
                               mcol(mst.paper, 255));
                DrawRectangleV(Vector2{q.x - 6.0f, q.y + 1.0f},
                               Vector2{12.0f, 2.0f}, c_player);
            }
            mtext(tag, q.x + 11.0f, q.y - 8.0f, 13.0f, c_player);
        };
        if (info.map_sled_glyph &&
            glm::length(info.map_sled_pos) > 1e-12)
            vehicle_glyph(to_screen(glm::normalize(info.map_sled_pos)), false,
                          "SLED");
        if (info.map_aircraft_glyph &&
            glm::length(info.map_aircraft_pos) > 1e-12)
            vehicle_glyph(to_screen(glm::normalize(info.map_aircraft_pos)),
                          true, "PLANE");
        // THE STING IN FLIGHT (Chad 2026-09-04): a FILLED ally diamond with a
        // heading tick along its velocity, so the drone reads as the one
        // moving thing of his on the map. Filled, not outline: it is not a
        // parked body, it is a round he has launched.
        if (info.map_sting_glyph &&
            glm::length(info.map_sting_pos) > 1e-12) {
            const glm::dvec3 sdir = glm::normalize(info.map_sting_pos);
            const Vector2 q = to_screen(sdir);
            const Vector2 n{q.x, q.y - 7.0f}, e{q.x + 7.0f, q.y},
                s{q.x, q.y + 7.0f}, w{q.x - 7.0f, q.y};
            DrawTriangle(n, w, s, c_out);
            DrawTriangle(n, s, e, c_out);
            const Vector2 n2{q.x, q.y - 4.5f}, e2{q.x + 4.5f, q.y},
                s2{q.x, q.y + 4.5f}, w2{q.x - 4.5f, q.y};
            // His own RPAS: his side's hue (absolute), like his arrow.
            DrawTriangle(n2, w2, s2, c_player);
            DrawTriangle(n2, s2, e2, c_player);
            // heading tick: the velocity's tangential part, projected the
            // same way the pointer's nose is (bubble_map's heading law).
            const double vl = glm::length(info.map_sting_fwd);
            if (vl > 1e-6) {
                static render::MapHeadingState s_sting_heading;
                const double hd = render::map_facing_heading_rad(
                    mp, info.map_sting_pos, info.map_sting_fwd,
                    mst.arrow_hold_sin, s_sting_heading);
                const float cs = std::cos(static_cast<float>(hd)),
                            sn = std::sin(static_cast<float>(hd));
                const Vector2 tip{q.x + 13.0f * sn, q.y - 13.0f * cs};
                DrawLineEx(q, tip, halo + 1.6f, c_out);
                DrawLineEx(q, tip, 2.0f, c_player);
            }
            mtext("STING", q.x + 11.0f, q.y - 8.0f, 13.0f, c_player);
        }
    }

    // CONTACTS — mavericks. ★★★ SHAPE IS PLAYER-RELATIVE, COLOUR IS
    // ABSOLUTE (Chad 2026-09-10). Ally = CIRCLE, enemy = SQUARE, unfactioned =
    // grey circle -- the SHAPE still answers "is he mine", which is what the
    // banner's shape language was always for. The FILL is now the contact's
    // own faction: Valley blue, Sudbury orange, whoever is reading the chart.
    // This line is the one Chad reported after the L11b fix ("map and ally
    // markers are still wrong color"): it was `fac == player_fac ? c_ally :
    // c_enemy`, a second copy of the player-relative law that team_col()
    // replaced ten lines up but that this loop never called. Every one on a
    // dark halo so it survives over a lake plate or a road. Skip inert ones.
    for (std::size_t i = 0; i < info.drones_draw.size(); ++i) {
        if (i < info.drones_alive.size() && info.drones_alive[i] == 0)
            continue;
        const Vector2 p =
            to_screen(glm::normalize(info.drones_draw[i].position));
        const int fac =
            i < info.drones_faction.size() ? info.drones_faction[i] : -1;
        const float r = static_cast<float>(mst.marker_px);
        // ONE pure decision, pinned by test_team_kit (TK13/TK14): shape from
        // "is he mine", colour from "whose ground". The draw below only obeys.
        const render::ContactStyle cs =
            render::contact_style(fac, player_fac, tc);
        if (cs.neutral) {
            DrawCircleV(p, r + halo, c_out);
            DrawCircleV(p, r * 0.8f, c_neutral);
        } else if (!cs.square) {
            DrawCircleV(p, r + halo, c_out);
            DrawCircleV(p, r, mcol(cs.color, 255));
        } else {
            const float s = r + halo;
            DrawRectangleV(Vector2{p.x - s, p.y - s}, Vector2{2 * s, 2 * s},
                           c_out);
            DrawRectangleV(Vector2{p.x - r, p.y - r}, Vector2{2 * r, 2 * r},
                           mcol(cs.color, 255));
        }
    }

    // PLAYER — an arrow in HIS OWN SIDE'S hue (c_player, absolute since
    // 2026-09-10: blue flying for the Valley, slag orange flying for Central
    // City): he wears his side's colour like everyone else (his ask). He is the
    // largest marker on the map, the only ARROW, and the only one wearing
    // a ring, so he stays instantly findable among the allied blues on
    // SHAPE and SIZE rather than on a private hue.
    //
    // S-maparrow: the heading is now the NOSE bearing with a hold guard,
    // NOT the ground track. See render/bubble_map.h's banner for the
    // attribution — velocity flips 180 deg in a tail-slide and its
    // tangential part free-spins on any near-vertical trajectory, which
    // is the "it gets spun around sometimes" Chad reported.
    //
    // ★ L4: THE ARROW IS NOW DRAWN ON THE ACTIVE BODY, not on the aeroplane.
    // The nose becomes the machine's forward or the man's heading when he is
    // on one -- his ask, "pointer good at both large and small scales", is
    // half about SIZE (below: the arrow is in screen px, so it is the same
    // arrow at fit and at 64x) and half about it being HIS pointer at all
    // when he is not flying. The hold guard is unchanged and still applies:
    // the walker's heading is a tangent vector, so its tangential fraction is
    // 1 and the guard never fires on him -- it only ever protected a nose
    // pointed at the sky.
    {
        static render::MapHeadingState s_map_heading;
        const Vector2 p = to_screen(body_dir);
        const double heading = render::map_facing_heading_rad(
            mp, body_pos, body_fwd, mst.arrow_hold_sin, s_map_heading);
        const float rad = static_cast<float>(heading);
        const float cs = std::cos(rad), sn = std::sin(rad);
        const auto rot = [&](Vector2 v) {
            // Screen-space rotation about the arrow's own pivot; heading
            // is measured clockwise from north (atan2(east,north)),
            // which is exactly the sense a screen-space (x right, y
            // down) rotation by +heading turns "up" toward — no extra
            // sign flip needed. (Verified against the map's own basis:
            // heading=+90 deg sends the tip (0,-r) to (+r,0) = east =
            // screen right, and map east IS screen right.)
            return Vector2{p.x + v.x * cs - v.y * sn,
                           p.y + v.x * sn + v.y * cs};
        };
        const float r = static_cast<float>(mst.player_px);
        DrawRing(p, r + 3.0f, r + 4.8f, 0.0f, 360.0f, 28,
                 Color{c_out.r, c_out.g, c_out.b, 200});
        DrawRing(p, r + 3.4f, r + 4.4f, 0.0f, 360.0f, 28, c_player);
        const Vector2 tip{0.0f, -r}, left{-r * 0.62f, r * 0.72f},
            right{r * 0.62f, r * 0.72f};
        const Vector2 a = rot(tip), b = rot(left), c = rot(right);
        // Fill first, then STROKE the three edges — an expanded second
        // triangle does NOT give a uniform outline (scaling a triangle
        // about its centroid thickens the halo unevenly per edge, which
        // is what the first cut of this drew: a black sliver on one side
        // only). Stroking the edges is uniform by construction.
        DrawTriangle(a, b, c, c_player);
        const float ow = halo;
        DrawLineEx(a, b, ow, c_out);
        DrawLineEx(b, c, ow, c_out);
        DrawLineEx(c, a, ow, c_out);
    }

    // Scale bar — dark ink on the light plate.
    //
    // ★ L4: IT IS NO LONGER A FIXED 10 km (Chad: "better local scale"). A bar
    // welded to one distance is a bar that is a third of the screen at fit and
    // eleven screens wide at 64x, which is the same as having no scale at all
    // at the zoom you most need one. Pick the largest 1/2/5 x 10^n metres that
    // still fits inside a target width -- the standard chart rule, and the
    // reason the printed number is always a number you can do arithmetic with.
    {
        const double target_px = 0.18 * static_cast<double>(sw);
        double bar_m = 1.0;
        if (scale_px_per_m > 0.0) {
            const double raw_m = target_px / scale_px_per_m;
            const double decade =
                std::pow(10.0, std::floor(std::log10(std::max(1.0, raw_m))));
            const double mant = raw_m / decade;
            bar_m = decade * (mant >= 5.0 ? 5.0 : (mant >= 2.0 ? 2.0 : 1.0));
        }
        const float bar_px = static_cast<float>(bar_m * scale_px_per_m);
        const float bx = static_cast<float>(margin);
        const float by = static_cast<float>(sh - margin);
        DrawLineEx(Vector2{bx, by}, Vector2{bx + bar_px, by}, 3.0f, c_out);
        DrawLineEx(Vector2{bx, by - 5}, Vector2{bx, by + 5}, 3.0f, c_out);
        DrawLineEx(Vector2{bx + bar_px, by - 5},
                   Vector2{bx + bar_px, by + 5}, 3.0f, c_out);
        char sb[48];
        if (bar_m >= 1000.0)
            std::snprintf(sb, sizeof sb, "%.0f km", bar_m / 1000.0);
        else
            std::snprintf(sb, sizeof sb, "%.0f m", bar_m);
        mtext(sb, bx, by - 22.0f, 16.0f, Color{245, 245, 245, 235});
    }

    // Corner status block: on its own dark panel so the text has real
    // contrast against the light plate (the old dim-blue-on-cream stack
    // was the least legible thing on the map). YOUR score is printed in
    // the ally green, THEIRS in the enemy red — same code as the markers.
    {
        char line[96];
        const int bw = 244, bh = 116;
        DrawRectangle(margin - 12, margin - 10, bw, bh,
                      Color{c_out.r, c_out.g, c_out.b, 205});
        DrawRectangleLines(margin - 12, margin - 10, bw, bh,
                           Color{235, 235, 235, 120});
        const int mine = valley_is_mine ? info.conquest_score_valley
                                        : info.conquest_score_sudbury;
        const int theirs = valley_is_mine ? info.conquest_score_sudbury
                                          : info.conquest_score_valley;
        const float lx = static_cast<float>(margin);
        const float ly = static_cast<float>(margin);
        // ★★★ THE LEGEND IS THE KEY TO THE CHART, so its swatches must be
        // the colours the chart actually uses. The WORDS stay player-relative
        // (OURS / THEIRS is exactly the question a score board answers); the
        // INK is each side's absolute hue, so "OURS" is orange for a Central
        // City player and the legend agrees with every dot on the map. They
        // used to be c_ally / c_enemy, which made the key disagree with the
        // chart for one of the two sides.
        const Color c_mine = team_col(player_fac);
        const Color c_theirs = team_col(player_fac == 0 ? 1 : 0);
        mtext("OURS", lx, ly, 18.0f, c_mine);
        mtext("THEIRS", lx + 118.0f, ly, 18.0f, c_theirs);
        std::snprintf(line, sizeof line, "%d", mine);
        mtext(line, lx, ly + 22.0f, 26.0f, c_mine);
        std::snprintf(line, sizeof line, "%d", theirs);
        mtext(line, lx + 118.0f, ly + 22.0f, 26.0f, c_theirs);
        std::snprintf(line, sizeof line, "PLANES %d",
                      info.conquest_planes_left);
        mtext(line, lx, ly + 56.0f, 18.0f, Color{240, 240, 240, 235});
        std::snprintf(line, sizeof line, "PUMPS %d/%d",
                      info.conquest_pumps_alive, info.conquest_pumps_total);
        mtext(line, lx, ly + 78.0f, 18.0f, c_obj);
    }

    // ★ L4: the view's own readout. A chart that can be zoomed must say what
    // zoom it is at, or "why does this look wrong" has no answer on screen.
    // Bottom-right, out of the score stack's and the scale bar's way.
    {
        char zl[64];
        std::snprintf(zl, sizeof zl, "%s  x%.1f%s",
                      info.map_body_tag != nullptr ? info.map_body_tag
                                                   : "AIRCRAFT",
                      vw.zoom, vw.follow_player ? "  FOLLOW" : "");
        mtext(zl, static_cast<float>(sw - margin - 210),
              static_cast<float>(sh - margin - 6), 16.0f,
              Color{240, 240, 240, 235});
    }
}

}  // namespace render
