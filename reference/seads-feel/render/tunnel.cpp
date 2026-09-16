#include "render/tunnel.h"

#include <algorithm>          // std::max (T16 per-piece bound radius)
#include <cstdlib>            // getenv (A/B bypass)
#include <glm/common.hpp>     // glm::min/max (T16 per-piece AABB)
#include <glm/geometric.hpp>  // normalize (lamp region-up night gate)
#include <vector>

#include "raylib.h"
#include "raymath.h"        // MatrixTranslate
#include "render/lights.h"  // LampRenderer (T4b gaslamps reuse the glow tech)
#include "render/tunnel_lamp_hits.h"  // T5c: TunnelLampWorld (destructible)
#include "render/tunnel_mesh.h"
#include "rlgl.h"
#include "world/heightfield.h"
#include "world/tunnel_geo.h"  // kTunnelMouthErrington (P2-2 lamp-up fallback)
#include "world/tunnel_net.h"

namespace render {

namespace {

// VS: world-absolute vertex (baked at the true underground radius). The FS
// needs the EYE-RELATIVE world position for a well-conditioned dFdx facet
// normal and for the planet-local up. matModel is the pure -eye translate
// raylib sets on DrawMesh (== vertex - eye). texcoord.x carries the per-vertex
// depth cue.
const char* kTunnelVS = R"GLSL(#version 330
in vec3 vertexPosition;   // world-absolute (baked at underground radius)
in vec2 vertexTexCoord;   // .x = depth-cue brightness [0,1]
uniform mat4 mvp;
uniform mat4 matModel;    // raylib-set: translate -eye
out vec3 vPos;            // eye-relative world position (vertex - eye)
out float vCue;
void main() {
    vPos = (matModel * vec4(vertexPosition, 1.0)).xyz;
    vCue = vertexTexCoord.x;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

// FS: flat mono greybox. The facet normal is the screen-space derivative of the
// eye-relative position (no baked normals -> hard planes, the stereoscope
// look), eye-flipped so it faces the camera (culling is OFF, so winding is not
// load- bearing). A dim directional keyed off the planet-local up brightens
// FLOORS over CEILINGS (so the tube shape reads); the whole thing is multiplied
// by the per-vertex depth cue so deep tube reads near-black and the mouths
// brighter. Mono (r==g==b) so the S1 silver post tones it.
const char* kTunnelFS = R"GLSL(#version 330
in vec3 vPos;             // eye-relative world position
in float vCue;            // depth cue (T2) OR breach spill (T10 bore, baked 1)
uniform vec3 uEye;        // world eye (reconstruct up = normalize(worldPos))
uniform float uWallVal;   // base wall value (deepest ceiling)
uniform float uAmbient;   // ambient floor
uniform float uUpKey;     // local-up directional gain (floors brighter)
// T10/T11 — THE ARENA lighting. uCoreGain>0 => this piece is a CAVERN piece:
// add a radial "core key" (bright toward the buried centre, 1/(1+(d/uCoreFall)^2)
// falloff) + radius STRATA (crust/mantle value bands) so the layers read. The
// key is paint keyed toward the origin — it does NOT need a visible core, so
// T12 (which sealed the core away) keeps it exactly. uSpill>0 => bore breach
// spill: the last ~600 m of bore glows with core light (the angle-of-entry cue,
// baked into vCue).
uniform float uCoreGain;  // cavern core-key gain (0 = not a cavern piece)
uniform float uCoreFall;  // core-key falloff distance [m]
// T16 ENTRANCE-ART — DE-BLACK the surface entrances. uExterior>0.5 => a
// DAYLIGHT open-air piece (the open-pit / bowl walls, the Errington portal): it
// skips the underground crust/mantle STRATA override (which crushed the
// surface collars to base 0.11 == black), reading its own bright uWallVal
// (rock/concrete) under the daylight uAmbient instead.
uniform float uExterior;  // 1 = daylight exterior surface, 0 = underground
out vec4 finalColor;
void main() {
    vec3 worldPos = vPos + uEye;
    float r = length(worldPos);
    vec3 dpx = dFdx(vPos);
    vec3 dpy = dFdy(vPos);
    vec3 N = cross(dpx, dpy);
    float nl2 = dot(N, N);
    if (nl2 < 1.0e-12) { finalColor = vec4(vec3(uWallVal*uAmbient*vCue), 1.0); return; }
    N *= inversesqrt(nl2);
    vec3 toEye = normalize(-vPos);
    if (dot(N, toEye) < 0.0) N = -N;          // outward toward the camera
    vec3 up = worldPos / max(r, 1.0);         // planet-local radial
    // A surface whose normal points DOWN (dot(N,up) < 0) is a floor seen from
    // above -> brighten it; a ceiling (normal up) stays dim.
    float key = uUpKey * max(-dot(N, up), 0.0);
    float lit = uAmbient + key;
    float base = uWallVal;
    if (uCoreGain > 0.0) {
        // T10 CAVERN CEILING (mantle underside): keep its own bright base
        // (uWallVal = kCavernVal) — the strata darkening below is for the bore
        // WALLS as you descend, not the ceiling (a 0.07 mantle band crushes the
        // ceiling to black). Add the radial CORE KEY: brighten toward the glowing
        // core (toCore = -up; d = distance to origin = r).
        float ck = max(dot(N, -up), 0.0);     // facing the core
        float fall = 1.0 / (1.0 + (r/uCoreFall)*(r/uCoreFall));
        lit += uCoreGain * ck * fall;
        // T10.1 CEILING TEXTURE: a LOW-FREQUENCY value-noise on the world
        // direction (deterministic, no clock) so the mantle underside is not a
        // mathematically UNIFORM wash (the driver: "a uniform featureless grey,
        // no gradient, no features, no parallax"). ±~15% around the base — a
        // large-scale mottling that gives the surface a sense of extent/scale
        // between the ember points. Three sine octaves of the unit direction ->
        // a smooth, seamless field on the sphere.
        vec3 udir = worldPos / max(r, 1.0);
        float mott = sin(udir.x*7.0 + udir.y*3.0)
                   + sin(udir.y*6.0 - udir.z*4.0)
                   + sin(udir.z*5.0 + udir.x*2.0);
        base *= 1.0 + 0.15 * (mott / 3.0);   // ±15% low-freq variation
    } else if (uExterior > 0.5) {
        // T16 DAYLIGHT EXTERIOR (open-pit / bowl walls, portal): NO strata —
        // base stays uWallVal (bright rock / concrete), lit by the daylight
        // ambient + up-key. The depth cue (vCue) still gives a gentle
        // rim->floor gradient, but the high ambient keeps the pit readable, not
        // black (the "black ant mound" fix).
    } else {
        // T10 STRATA: radius value bands on the bore/wall pieces (the layers read
        // as you descend). crust r>13000 (kWallVal), mantle 10.4-13k darker, with
        // a thin brighter boundary line at the crust/mantle transition.
        if (r > 13000.0) {
            base = 0.11;                      // crust
        } else if (r > 10400.0) {
            base = 0.07;                      // mantle
            float dline = abs(r - 13000.0);
            if (dline < 120.0) base = 0.16;   // the crust/mantle boundary line
        }
    }
    // T10 BREACH SPILL: the bore's last ~600 m before a breach glows with core
    // light (vCue baked toward 1 there on kWall pieces only — see build).
    float val = clamp(base * lit * vCue, 0.0, 1.0);
    finalColor = vec4(vec3(val), 1.0);
}
)GLSL";

// Render-look constants (S9-zoom pattern: code constants, not config).
constexpr float kTunnelAmbient = 0.05f;  // night ambient floor
// T10/T11 — THE ARENA lighting constants. The arena ceiling (mantle underside)
// is dim, lit from below by the radial core-key (paint toward the origin; T12
// sealed away the visible core but kept the key).
constexpr float kCavernAmbient = 0.30f;  // cavern-ceiling ambient floor
constexpr float kCavernVal = 0.30f;      // cavern-ceiling base value
// T10.1 — CAVERN READABILITY (docs/tunnel_staging.md T10.1): the core-key gain
// is RAISED and the falloff SOFTENED so the near ceiling gets a visible
// GRADIENT (an orientation cue) and the far wall reads dim-but-not-black (the
// driver's verdict: at r=9500 everything but the core was pure black). Iterated
// on the TUNCAM_CAVERN screenshots.
constexpr float kCoreKeyGain = 1.10f;  // cavern radial core-key gain
constexpr float kCoreKeyFall =
    9000.0f;  // core-key 1/(1+(r/fall)^2) falloff [m]
constexpr float kChamberAmbient = 0.55f;  // lit side-chamber ambient floor
// T16 ENTRANCE-ART — THE DAYLIGHT EXTERIOR AMBIENT (de-black): the open-cut
// surfaces — the Errington entry pit + Murray open-pit bowl walls (kCollar) and
// the Errington portal frame (kPortal) — are OPEN TO THE SKY, so they get a
// daylight ambient floor instead of the kTunnelAmbient (0.05) cave floor that
// read them as black ant mounds. Paired with the FS uExterior path (which skips
// the underground strata override) so the surfaces read as sunlit rock/concrete.
constexpr float kExteriorAmbient = 0.60f;
// T8 — THE GREY CURTAIN FIX (2026-07-19, Chad's round-5 fly: "an opaque grey
// wall curtain ~1 km in after a RISE in the ramp — can't see where to go, it
// made me crash"). ATTRIBUTED (docs/tunnel_staging.md T8): NOT a closed volume
// — the T7 flythrough passes and the corridor CENTRELINE stays open >=1200 m
// ahead the whole route (a pure-geometry fact, pinned by the T8 sightline
// verifier). The curtain was a READABILITY failure of THIS floor key: at the
// Errington ramp->plateau SAG knuckle (arc ~3.8-5.0 km) the pilot descends the
// steep bore then the floor flattens up into the sightline; the floor facet
// there faces the camera near head-on, so max(-dot(N,up)) ~ 1 and the floor
// drew at ~0.55 key = a BRIGHT GREY MASS that painted OVER the crown/floor-edge
// lamp lines (the vanishing-point cue that carries the continuation), reading
// as an opaque wall. Dropping the key to 0.15 darkens the grazing floor so the
// LAMP LINES read through it and the continuation is visible (verified in the
// TUNCAM_BORE sag series s=3800..5200: the grey dome is gone, the crown lamps
// recede past the transition). The floor still keeps a subtle up-key sheen at
// normal grazing so the tube shape reads (verified at s=1000). The geometry is
// UNTOUCHED (the centreline was already open; a sag-curve pass would move the
// deep-plateau/junction goldens for a fix the contrast dial already delivers).
// This is the "at most one value dial on floor/cue contrast" T8 authorized.
constexpr float kTunnelUpKey = 0.15f;  // floor-vs-ceiling directional gain

struct TunnelRenderer {
    std::vector<Mesh> meshes;
    std::vector<float> wall_val;   // parallel to meshes — per-piece draw value
    std::vector<float> amb_val;    // parallel — per-piece ambient floor
    std::vector<float> core_gain;  // T10 — per-piece cavern core-key gain
    // T16 visibility culling (parallel to meshes): each piece's bounding sphere
    // (world-absolute metres) + its EXTERIOR/INTERIOR class, computed at build.
    std::vector<glm::dvec3> bound_center;
    std::vector<double> bound_radius;
    std::vector<bool> exterior;  // piece_is_exterior(kind)
    Shader shader{};
    Material mat{};
    int loc_eye = -1, loc_wall = -1, loc_amb = -1, loc_upkey = -1;
    int loc_coregain = -1, loc_corefall = -1;  // T10
    int loc_exterior = -1;                      // T16 daylight-exterior flag
    bool ok = false;
};

// The per-piece base draw value (T4b): the lit egg/chambers brighten over the
// void-black tube walls; collars stay solid-earth daylight.
float piece_val(PieceKind kind) {
    switch (kind) {
        case PieceKind::kChamber:
            return kChamberVal;
        case PieceKind::kCollar:
        case PieceKind::kTrench:  // T16: open-cut trench walls = daylight earth
            return kSkirtVal;
        case PieceKind::kFloor:
            return kFloorVal;
        case PieceKind::kCavern:
            return kCavernVal;
        case PieceKind::kPortal:
            return kPortalVal;  // T16: bright concrete portal frame
        case PieceKind::kWall:
        default:
            return kWallVal;
    }
}

// The per-piece ambient floor. The lit chambers + the arena ceiling read
// brighter; the daylight exterior surfaces (open-cut collars + the portal, T16)
// get the daylight floor; every underground tube/floor piece keeps the
// kTunnelAmbient cave floor.
float piece_ambient(PieceKind kind) {
    switch (kind) {
        case PieceKind::kChamber:
            return kChamberAmbient;
        case PieceKind::kCavern:
            return kCavernAmbient;
        case PieceKind::kCollar:
        case PieceKind::kPortal:
        case PieceKind::kTrench:
            return kExteriorAmbient;  // T16: daylight open-cut / portal surfaces
        default:
            return kTunnelAmbient;
    }
}

// T10: is this a CAVERN piece (gets the radial core key + strata)? The bore
// walls (kWall) also get strata (radius bands) but not the core key — the key
// is the ceiling's light. Return the core-key gain (0 = no key).
float piece_core_gain(PieceKind kind) {
    return kind == PieceKind::kCavern ? kCoreKeyGain : 0.0f;
}

TunnelRenderer g_tunnel;
bool g_tunnel_tried = false;

// The gaslamp glows (T4b), built lazily with the meshes. Reuses the proven
// street-lamp additive-glow renderer via build_lamp_renderer_from_points. The
// tunnel set is ALWAYS-ON (a contained mine); we bypass the night gate at draw
// time by feeding a sun dir that reads as deep night for the tunnel region.
// Two tiers by lamp intensity: the dim tube/chamber lamps and the bright
// Black-Stope-ring lamps (drawn larger so the 1800 m egg reads).
LampRenderer g_tunnel_lamps_dim;
LampRenderer g_tunnel_lamps_bright;
LampRenderer g_tunnel_lamps_ember;  // T10.1 — the km-scale cavern ember field
glm::dvec3 g_tunnel_lamp_up{0.0, 1.0, 0.0};  // region up (night-gate bypass)

// Lamps at or above this intensity draw in the BRIGHT tier (the beacon rings).
constexpr float kBrightLampCut = 2.0f;

// The three gaslamp look tiers: the dim tube/chamber/floor lamps, the bright
// beacon-ring lamps, and the T10.1 cavern EMBER tier (a large world glow so the
// ember reads across the km-scale cavern — an 8 m glow is invisible at 5-15
// km).
LampLook dim_lamp_look() {
    LampLook look;
    look.color = glm::vec3(1.0f, 0.96f, 0.90f);
    look.size_m = 8.0f;
    look.min_px = 3.0f;
    look.night_bypass = 1.0f;  // T10.1: the mine is always lit (robust for the
                               // full-sphere ceiling set — no reliance on the
                               // averaged night-gate-bypass sun dir)
    return look;
}
LampLook bright_lamp_look() {
    LampLook look = dim_lamp_look();
    look.size_m = 48.0f;  // T10.1: bigger world glow so the breach beacon rings
                          // read as rings of light from across the cavern
    look.min_px = 5.0f;   // T10.1: keep each beacon a legible dot at ~19 km so
                          // the far breach still reads as a RING of light
    look.brightness = 0.85f;
    return look;
}
// T10.1 — the CAVERN EMBER tier. Dim warm embers scattered on the ceiling for
// depth/parallax across the black shell (the "point lights, not wall shading"
// lesson at km scale). A LARGE world size so they read as glows at 5-15 km (a
// 8 m tube-lamp glow is invisible there); modest brightness — embers, not suns;
// warm-white <= the flown gaslamp warmth. A min_px floor keeps the FARTHEST
// embers a visible dot so the field never fully vanishes.
LampLook ember_lamp_look() {
    LampLook look;
    look.color = glm::vec3(1.0f, 0.93f, 0.82f);  // warm-white (ember)
    look.size_m = 90.0f;       // reads as a glow across the km-scale shell
    look.min_px = 2.0f;        // far floor: never smaller than a 2 px dot
    look.brightness = 0.42f;   // dim — embers, not suns
    look.core_bright = 0.5f;   // soft bulb
    look.night_bypass = 1.0f;  // T10.1: full-sphere ceiling set — always lit
    return look;
}

// (Re)build the three lamp-renderer tiers from explicit ALIVE positions (T5c:
// the destructible rebuild feeds only surviving lamps; the initial build feeds
// all). Unloads any prior GPU buffers first so a rebuild leaks nothing.
void set_lamp_tiers(const std::vector<glm::dvec3>& dim,
                    const std::vector<glm::dvec3>& bright,
                    const std::vector<glm::dvec3>& ember) {
    unload_lamp_renderer(g_tunnel_lamps_dim);
    unload_lamp_renderer(g_tunnel_lamps_bright);
    unload_lamp_renderer(g_tunnel_lamps_ember);
    g_tunnel_lamps_dim = build_lamp_renderer_from_points(dim, dim_lamp_look());
    g_tunnel_lamps_bright =
        build_lamp_renderer_from_points(bright, bright_lamp_look());
    g_tunnel_lamps_ember =
        build_lamp_renderer_from_points(ember, ember_lamp_look());
}

// Upload one pure piece as a raylib Mesh (positions + cue-in-texcoord.x). No
// normals (the FS derives a flat facet normal).
Mesh upload_piece(const TunnelPiece& p) {
    Mesh m{};
    const int vcount = static_cast<int>(p.cue.size());
    m.vertexCount = vcount;
    m.triangleCount = static_cast<int>(p.indices.size() / 3);
    m.vertices = static_cast<float*>(MemAlloc(sizeof(float) * 3 * vcount));
    m.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * 2 * vcount));
    for (int i = 0; i < vcount; ++i) {
        m.vertices[3 * i + 0] = p.positions[3 * i + 0];
        m.vertices[3 * i + 1] = p.positions[3 * i + 1];
        m.vertices[3 * i + 2] = p.positions[3 * i + 2];
        m.texcoords[2 * i + 0] = p.cue[i];  // depth cue
        m.texcoords[2 * i + 1] = 0.0f;
    }
    const int nidx = static_cast<int>(p.indices.size());
    m.indices =
        static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * nidx));
    for (int k = 0; k < nidx; ++k) m.indices[k] = p.indices[k];
    UploadMesh(&m, false);
    return m;
}

void build_once(const world::TunnelNet& net, const world::HeightField* ground,
                double err_collar_m, double mur_collar_m, double cell_arc_m,
                TunnelLampWorld* lamp_world) {
    g_tunnel_tried = true;
    const TunnelMeshData data =
        build_tunnel_mesh(net, ground, err_collar_m, mur_collar_m, cell_arc_m);
    if (data.pieces.empty()) {
        TraceLog(LOG_WARNING, "TUNNEL: empty greybox — nothing to draw");
        return;
    }
    for (const TunnelPiece& p : data.pieces) {
        if (p.indices.size() < 3) continue;
        g_tunnel.meshes.push_back(upload_piece(p));
        g_tunnel.wall_val.push_back(piece_val(p.kind));
        g_tunnel.amb_val.push_back(piece_ambient(p.kind));
        g_tunnel.core_gain.push_back(piece_core_gain(p.kind));  // T10
        // T16 — per-piece bounding sphere (AABB centre + max vertex reach) and
        // visibility class, computed once from the world-absolute vertices.
        glm::dvec3 lo{1e30}, hi{-1e30};
        const std::size_t vc = p.cue.size();
        for (std::size_t v = 0; v < vc; ++v) {
            const glm::dvec3 q{p.positions[3 * v + 0], p.positions[3 * v + 1],
                               p.positions[3 * v + 2]};
            lo = glm::min(lo, q);
            hi = glm::max(hi, q);
        }
        const glm::dvec3 c = 0.5 * (lo + hi);
        double r = 0.0;
        for (std::size_t v = 0; v < vc; ++v) {
            const glm::dvec3 q{p.positions[3 * v + 0], p.positions[3 * v + 1],
                               p.positions[3 * v + 2]};
            r = std::max(r, glm::length(q - c));
        }
        g_tunnel.bound_center.push_back(c);
        g_tunnel.bound_radius.push_back(r);
        g_tunnel.exterior.push_back(piece_is_exterior(p.kind));
    }
    if (g_tunnel.meshes.empty()) return;
    g_tunnel.shader = LoadShaderFromMemory(kTunnelVS, kTunnelFS);
    g_tunnel.loc_eye = GetShaderLocation(g_tunnel.shader, "uEye");
    g_tunnel.loc_wall = GetShaderLocation(g_tunnel.shader, "uWallVal");
    g_tunnel.loc_amb = GetShaderLocation(g_tunnel.shader, "uAmbient");
    g_tunnel.loc_upkey = GetShaderLocation(g_tunnel.shader, "uUpKey");
    g_tunnel.loc_coregain = GetShaderLocation(g_tunnel.shader, "uCoreGain");
    g_tunnel.loc_corefall = GetShaderLocation(g_tunnel.shader, "uCoreFall");
    g_tunnel.loc_exterior = GetShaderLocation(g_tunnel.shader, "uExterior");
    // T12 — the emissive core sphere + its glow billboard are GONE (the core is
    // sealed away; the arena is lit by the radial core-key + embers alone).
    g_tunnel.mat = LoadMaterialDefault();
    g_tunnel.mat.shader = g_tunnel.shader;
    g_tunnel.ok = true;
    TraceLog(LOG_INFO, "TUNNEL: greybox %zu pieces uploaded",
             g_tunnel.meshes.size());

    // GASLAMPS (T4b/T5c): the placement is the app-owned TunnelLampWorld when
    // present (destructible — deaths persist across respawn); else place here
    // (legacy all-alive). Initialize the world on first build if empty.
    if (lamp_world != nullptr && lamp_world->lamps.empty())
        lamp_world->init(place_tunnel_lamps(net));
    const std::vector<TunnelLamp> placed =
        lamp_world != nullptr ? lamp_world->lamps : place_tunnel_lamps(net);
    if (!placed.empty()) {
        // Region up (night-gate bypass): the sun dir fed at draw is +region_up,
        // so -uSunDir = -up => sunEl ~ -1 for every lamp => always night/lit.
        // Averaged over ALL placed lamps (not just alive) so the bypass dir is
        // stable as lamps die.
        glm::dvec3 up_sum{0.0};
        for (const TunnelLamp& L : placed) up_sum += glm::normalize(L.pos);
        // P2-2 guard: the two bores lie on nearly-opposite hemispheres, so the
        // averaged direction can be near-zero — normalize would NaN. Fall back
        // to the Errington mouth direction (a stable night-gate bypass dir).
        g_tunnel_lamp_up =
            glm::length(up_sum) > 1e-6
                ? glm::normalize(up_sum / static_cast<double>(placed.size()))
                : glm::normalize(world::kTunnelMouthErrington);
        // Build the three tiers from the ALIVE lamps (T5c + T10.1 ember tier).
        std::vector<glm::dvec3> dim, bright, ember;
        for (std::size_t i = 0; i < placed.size(); ++i) {
            const bool alive =
                lamp_world != nullptr ? lamp_world->alive[i] : true;
            if (!alive) continue;
            if (placed[i].intensity == kCavernEmberIntensity)
                ember.push_back(placed[i].pos);  // T10.1 ceiling ember field
            else
                (placed[i].intensity >= kBrightLampCut ? bright : dim)
                    .push_back(placed[i].pos);
        }
        set_lamp_tiers(dim, bright, ember);
        if (lamp_world != nullptr) lamp_world->dirty = false;
    }
}

// T16 — the per-frame draw gate from the net's broad-phase bound sphere + the
// two surface mouths + the app's player-inside-net boolean. One place both the
// wall draw and the lamp draw read, so they cull identically.
static TunnelDrawGate frame_draw_gate(const glm::dvec3& eye,
                                      const FrameInfo& info) {
    const world::TunnelNet& net = *info.tunnel_net;
    const glm::dvec3 mouth_e =
        net.spine.empty() ? net.bound_center : net.spine.front().pos;
    const glm::dvec3 mouth_m =
        net.spine.empty() ? net.bound_center : net.spine.back().pos;
    // Underground is keyed off the EYE (not the plane): the camera is what sees
    // the interior — this is correct for freelook / external / smoke cameras
    // that detach from the aircraft. ONE contains() call, guarded by the T16
    // broad-phase so it is ~20 ns when the eye is far above ground (the common
    // case) and only full cost when the eye is near/inside the net — where the
    // interior is drawn anyway.
    const bool underground = net.contains(eye);
    return tunnel_draw_gate(eye, net.bound_center, net.bound_radius, mouth_e,
                            mouth_m, underground);
}

}  // namespace

void draw_tunnel(const glm::dvec3& eye, const glm::vec3& sun_dir,
                 const FrameInfo& info, const world::HeightField* ground,
                 double errington_collar_outer_m, double murray_collar_outer_m,
                 double terrain_cell_arc_m) {
    (void)sun_dir;  // the interior is lit by the depth cue, not the sun
    if (info.tunnel_net == nullptr) return;
    static const bool no_tunnel = std::getenv("SEADS_NO_TUNNEL") != nullptr;
    if (no_tunnel) return;  // A/B bypass (smoke + Chad's fly), read once
    if (!g_tunnel_tried)
        build_once(*info.tunnel_net, ground, errington_collar_outer_m,
                   murray_collar_outer_m, terrain_cell_arc_m,
                   info.tunnel_lamps);
    if (!g_tunnel.ok) return;

    // T16 — the per-frame visibility gate (EXTERIOR surface pieces vs INTERIOR
    // tube/arena pieces), computed ONCE from the player position. Far above
    // ground (Ramsey Lake) the interior pieces — the two-sided 336x168 arena +
    // the whole tube — are skipped; the surface skirts/headframe still draw out
    // to the generous exterior range.
    const TunnelDrawGate gate = frame_draw_gate(eye, info);

    // Two-sided (the FS eye-flips the facet normal): the pilot flies INSIDE the
    // tube so the inward wall faces the camera, but a greybox seen from either
    // side must draw.
    rlDisableBackfaceCulling();
    const Matrix xf =
        MatrixTranslate(static_cast<float>(-eye.x), static_cast<float>(-eye.y),
                        static_cast<float>(-eye.z));
    const glm::vec3 eye_f(static_cast<float>(eye.x), static_cast<float>(eye.y),
                          static_cast<float>(eye.z));
    const float upkey = kTunnelUpKey;
    const float corefall = kCoreKeyFall;
    SetShaderValue(g_tunnel.shader, g_tunnel.loc_eye, &eye_f,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(g_tunnel.shader, g_tunnel.loc_upkey, &upkey,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(g_tunnel.shader, g_tunnel.loc_corefall, &corefall,
                   SHADER_UNIFORM_FLOAT);
    // Per-piece draw value (T4b): collars = solid daylight earth (kSkirtVal),
    // the lit egg/chambers brighten over the void-black tube walls (kWallVal).
    // Per-piece AMBIENT (T9 WELL LIT): the lit egg gets kEggAmbient (a bright
    // cavern), all else kTunnelAmbient — set INSIDE the loop so the egg reads
    // well-lit without touching the tube/floor key.
    for (std::size_t i = 0; i < g_tunnel.meshes.size(); ++i) {
        // T16 cull: EXTERIOR pieces draw within the generous draw distance of
        // their own bounds; INTERIOR pieces only when the gate says so
        // (underground / near a mouth). One branch, no per-piece SDF.
        if (g_tunnel.exterior[i]) {
            const double d =
                glm::length(eye - g_tunnel.bound_center[i]) -
                g_tunnel.bound_radius[i];
            if (!gate.exterior || d > kTunnelExteriorDrawDist_m) continue;
        } else if (!gate.interior) {
            continue;
        }
        const float wall = g_tunnel.wall_val[i];
        const float amb = g_tunnel.amb_val[i];
        const float cgain = g_tunnel.core_gain[i];
        const float ext = g_tunnel.exterior[i] ? 1.0f : 0.0f;  // T16 daylight
        SetShaderValue(g_tunnel.shader, g_tunnel.loc_wall, &wall,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(g_tunnel.shader, g_tunnel.loc_amb, &amb,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(g_tunnel.shader, g_tunnel.loc_coregain, &cgain,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(g_tunnel.shader, g_tunnel.loc_exterior, &ext,
                       SHADER_UNIFORM_FLOAT);
        DrawMesh(g_tunnel.meshes[i], g_tunnel.mat, xf);
    }
    rlEnableBackfaceCulling();
}

void draw_tunnel_lamps(const glm::dvec3& eye, const glm::vec3& /*sun_dir*/,
                       const FrameInfo& info) {
    if (info.tunnel_net == nullptr) return;
    static const bool no_tunnel = std::getenv("SEADS_NO_TUNNEL") != nullptr;
    if (no_tunnel) return;

    // T5c: a shot killed a lamp this frame (world.dirty) — rebuild both tiers
    // from the surviving (alive) lamps so the dead lamp goes dark. A wholesale
    // rebuild is fine (a rare event). Deaths persist across respawn (world
    // state, not life state) — the app never resets `alive`.
    if (info.tunnel_lamps != nullptr && info.tunnel_lamps->dirty) {
        set_lamp_tiers(
            info.tunnel_lamps->alive_positions(kBrightLampCut, false),
            info.tunnel_lamps->alive_positions(kBrightLampCut, true),
            info.tunnel_lamps->alive_ember_positions());
        info.tunnel_lamps->dirty = false;
    }

    // Guard: lamps are built as part of build_once() (triggered by the first
    // draw_tunnel() call). If no tier is ready yet, nothing to draw.
    if (!g_tunnel_lamps_dim.ok && !g_tunnel_lamps_bright.ok &&
        !g_tunnel_lamps_ember.ok)
        return;

    // GASLAMPS (T4b): additive glows, drawn AFTER every opaque depth-writer
    // (aircraft/trees/etc.) — depth-test ON, write OFF, per the world-sudbury
    // draw-order lesson. Night-gate BYPASSED (a mine is lit day and night):
    // feed a sun dir == +region_up so every lamp reads deep night.
    const glm::vec3 tunnel_sun(static_cast<float>(g_tunnel_lamp_up.x),
                               static_cast<float>(g_tunnel_lamp_up.y),
                               static_cast<float>(g_tunnel_lamp_up.z));
    const int vh = GetScreenHeight();
    // T16 — the same visibility gate the wall draw uses. Beyond the exterior
    // draw distance nothing lights; the INTERIOR ember + dim tube/floor tiers
    // are skipped unless the player is underground / at a mouth; the bright tier
    // (mouth + bowl-rim beacons are surface landmarks, breach beacons interior)
    // draws within the exterior range so the mouths stay findable from the air.
    const TunnelDrawGate gate = frame_draw_gate(eye, info);
    if (!gate.exterior && !gate.interior) return;
    // T10.1 — the ember field first (dimmest), then the dim tube lamps, then
    // the bright beacon rings (additive, order-independent, but drawn
    // dim->bright for clarity).
    if (g_tunnel_lamps_ember.ok && gate.interior)
        draw_lamp_renderer(g_tunnel_lamps_ember, tunnel_sun, eye, 0.0f, vh);
    if (g_tunnel_lamps_dim.ok && gate.interior)
        draw_lamp_renderer(g_tunnel_lamps_dim, tunnel_sun, eye, 0.0f, vh);
    if (g_tunnel_lamps_bright.ok && (gate.exterior || gate.interior))
        draw_lamp_renderer(g_tunnel_lamps_bright, tunnel_sun, eye, 0.0f, vh);
    // T12 — the core glow billboard is GONE (the core is sealed away).
}

void unload_tunnel() {
    unload_lamp_renderer(g_tunnel_lamps_dim);
    unload_lamp_renderer(g_tunnel_lamps_bright);
    unload_lamp_renderer(g_tunnel_lamps_ember);  // T10.1 ember field
    if (!g_tunnel.ok) return;
    for (Mesh& m : g_tunnel.meshes) UnloadMesh(m);
    g_tunnel.meshes.clear();
    g_tunnel.wall_val.clear();
    g_tunnel.amb_val.clear();
    g_tunnel.core_gain.clear();
    g_tunnel.bound_center.clear();
    g_tunnel.bound_radius.clear();
    g_tunnel.exterior.clear();
    UnloadShader(g_tunnel.shader);
    g_tunnel.ok = false;
}

}  // namespace render
