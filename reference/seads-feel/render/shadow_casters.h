#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "render/rig.h"  // aircraft_node_specs(): the MEASURED airframe dims

// ★ R5 ROW 9 — SHADOWS ON SNOW: the CASTER PROXY LIST (the CPU half of
// option (c), docs/snow_R5_immersion_ledger.md ROW 9).
//
// The mechanism is RECEIVER-SIDE: a small uniform array of capsule proxies is
// tested per-fragment in the receiving ground shader (render/planet.cpp) —
// each fragment casts a ray TOWARD THE SUN and occludes by the proxies it
// crosses. There is deliberately NO "where does the sun ray hit the ground"
// anywhere in this row: whatever surface rasterized the pixel receives the
// shadow at ITS OWN position, so the fence-3 failure class (a decal draped on
// facet+fold landing lift_m = 0.150 m UNDER the rider patch) is
// UNREPRESENTABLE, not merely avoided.
//
// This header is the PURE, headlessly-testable half (the shadow_probe.h /
// rig.h convention: glm + std only, ZERO raylib, no clock). It builds
// WORLD-SPACE capsules; render/draw.cpp rebases them to the camera eye in
// DOUBLE and casts to float (the draw.h double->float seam — proxy endpoints
// go to the GPU eye-relative, so float precision is O(view distance), never
// O(R)).
//
// ONE SHAPE ON PURPOSE: every proxy is a capsule (a sphere is a zero-length
// one). The consult's menu was capsules/spheres/discs; a single shape keeps
// the fragment loop branchless and the uniform layout flat, and the casters
// this row ships are all well fit by capsules (see the two builders below for
// the documented approximations). Adding a caster — the row-3 enemy aircraft,
// the row-4 Sudburians when they exist — is DATA: append capsules to
// FrameInfo::shadow_casters, nothing else moves.

namespace render {

// Uniform-array budget in the planet FS (the consult's ~8-16). The shipped
// casters use 9 (aircraft 3 + sled 6); draw.cpp truncates anything past this.
// ⚠ ROW-3-ENEMIES BUDGET NOTE: at 3 capsules per airframe the remaining 7
// slots seat TWO enemy aircraft; the ledger's plan (CPU-cull to the nearest
// few) is the mechanism that keeps a bigger furball inside the cap.
constexpr int kMaxShadowCasters = 16;

// One world-space capsule occluder. a/b are the segment endpoints (DOUBLE,
// world frame — rebased eye-relative only at the draw seam).
struct ShadowCaster {
    glm::dvec3 a{0.0};
    glm::dvec3 b{0.0};
    double radius = 0.0;  // [m]
};

// ★ FENCE 5 — THE ALTITUDE GATE. True when a caster anchored at `pos` sits at
// or above the drive surface (radius `surface_r` metres from the planet
// centre, from the ONE height source world::HeightField::radius_at), within
// `margin_m` of slack for suspension sink / contact penetration. A sled in
// the tunnel network is tens of metres UNDER the DEM surface and must not
// shadow the ground over its head — the proxy test itself cannot know about
// terrain (option (c)'s honest limit), so the gate lives here, at the list.
inline bool shadow_caster_above_surface(const glm::dvec3& pos,
                                        double surface_r, double margin_m) {
    return glm::length(pos) >= surface_r - margin_m;
}

// --------------------------------------------------------------------------
// CASTER 1 — HIS AIRCRAFT. Three capsules, every dimension MEASURED from
// render::aircraft_node_specs() box_dims accumulated through the parent chain
// (the exact method the row-9 probe used — tools/shadow_probe.cpp:112 — which
// yielded span 10.10 m / length 8.90 m / chord 2.40 m; structural parents all
// carry identity rest_rot, so the positional sum is exact to the centimetre).
//   - FUSELAGE: capsule along body Z spanning the full measured length
//     (spinner tip to elevator trailing edge), radius = half the fuselage
//     box's larger cross dimension.
//   - WING: capsule along body X spanning the measured span, radius =
//     half-chord, at the wing group's measured y/z. DOCUMENTED APPROXIMATION:
//     a capsule of radius chord/2 is rotationally symmetric about the span
//     axis, so it stands chord-thick (2.40 m) vertically where the real wing
//     is 0.58 m — from a high sun the footprint is the exact span x chord
//     stadium; at grazing sun the shadow is slightly too wide chord-wise
//     (where it is already a >100 m smear by the probe's own numbers).
//   - TAILPLANE: same form over the hstab+elevator group (span/chord/centre
//     all accumulated from the four tail nodes).
// The prop disc (3.0 m, 0.11 m thin) is deliberately NOT a proxy — a disc
// face-on to the body Z reads as a giant sphere under a capsule-only scheme,
// and its real shadow is a near-invisible sliver. Named limit, not an
// accident.
// --------------------------------------------------------------------------

struct AircraftShadowDims {
    // Fuselage capsule (body frame, along Z).
    double fus_z_min = 0.0, fus_z_max = 0.0, fus_radius = 0.0;
    // Wing capsule (along X).
    double half_span = 0.0, wing_radius = 0.0, wing_y = 0.0, wing_z = 0.0;
    // Tailplane capsule (along X).
    double tail_half_span = 0.0, tail_radius = 0.0;
    double tail_y = 0.0, tail_z = 0.0;
};

inline AircraftShadowDims aircraft_shadow_dims() {
    const auto& specs = aircraft_node_specs();
    // Accumulated body position of node i through the parent chain.
    auto acc = [&](std::size_t i) {
        glm::dvec3 p = glm::dvec3(specs[i].rest_pos);
        for (int par = specs[i].parent; par >= 0; par = specs[par].parent)
            p += glm::dvec3(specs[par].rest_pos);
        return p;
    };
    auto is_tail = [](const char* k) {
        return std::strcmp(k, "hstab_l") == 0 ||
               std::strcmp(k, "hstab_r") == 0 ||
               std::strcmp(k, "elevator_l") == 0 ||
               std::strcmp(k, "elevator_r") == 0;
    };
    AircraftShadowDims d;
    double min_z = 1e9, max_z = -1e9, max_x = 0.0;
    double t_min_z = 1e9, t_max_z = -1e9, t_max_x = 0.0;
    double t_y_sum = 0.0;
    int t_n = 0;
    for (std::size_t i = 0; i < specs.size(); ++i) {
        const glm::dvec3 p = acc(i);
        const glm::dvec3 h = 0.5 * glm::dvec3(specs[i].box_dims);
        min_z = std::min(min_z, p.z - h.z);
        max_z = std::max(max_z, p.z + h.z);
        max_x = std::max(max_x, std::abs(p.x) + h.x);
        if (std::strcmp(specs[i].mesh_key, "fuselage") == 0)
            d.fus_radius = 0.5 * std::max(double(specs[i].box_dims.x),
                                          double(specs[i].box_dims.y));
        if (std::strcmp(specs[i].mesh_key, "wing_l") == 0) {
            d.wing_radius = 0.5 * double(specs[i].box_dims.z);  // half-chord
            d.wing_y = p.y;
            d.wing_z = p.z;
        }
        if (is_tail(specs[i].mesh_key)) {
            t_min_z = std::min(t_min_z, p.z - h.z);
            t_max_z = std::max(t_max_z, p.z + h.z);
            t_max_x = std::max(t_max_x, std::abs(p.x) + h.x);
            t_y_sum += p.y;
            ++t_n;
        }
    }
    d.fus_z_min = min_z;  // the probe's length = max_z - min_z (8.90 m)
    d.fus_z_max = max_z;
    d.half_span = max_x;  // the probe's span = 2 * max_x (10.10 m)
    d.tail_half_span = t_max_x;
    d.tail_radius = 0.5 * (t_max_z - t_min_z);  // tail chord / 2
    d.tail_y = t_n > 0 ? t_y_sum / t_n : 0.0;
    d.tail_z = 0.5 * (t_min_z + t_max_z);
    return d;
}

// Append the aircraft's three capsules at pose (pos, body->world orient).
// Endpoints are inset by the radius so the capsule's OVERALL extent equals
// the measured one (a capsule adds its radius past each endpoint).
inline void aircraft_shadow_proxies(std::vector<ShadowCaster>& out,
                                    const glm::dvec3& pos,
                                    const glm::dquat& orient) {
    static const AircraftShadowDims d = aircraft_shadow_dims();
    auto W = [&](const glm::dvec3& body) { return pos + orient * body; };
    // Fuselage, along body Z (nose = -Z).
    out.push_back({W({0.0, 0.0, d.fus_z_min + d.fus_radius}),
                   W({0.0, 0.0, d.fus_z_max - d.fus_radius}), d.fus_radius});
    // Wing, along body X.
    const double wx = std::max(d.half_span - d.wing_radius, 0.0);
    out.push_back({W({-wx, d.wing_y, d.wing_z}), W({wx, d.wing_y, d.wing_z}),
                   d.wing_radius});
    // Tailplane, along body X.
    const double tx = std::max(d.tail_half_span - d.tail_radius, 0.0);
    out.push_back({W({-tx, d.tail_y, d.tail_z}), W({tx, d.tail_y, d.tail_z}),
                   d.tail_radius});
}

// --------------------------------------------------------------------------
// CASTER 2 — THE SLED (machine + rider, SIX capsules). ★ Chad's row-9 ruling
// (2026-08-28): "SNOWMACHINE SHADOW NEEDS BETTER PROFILE THAT ACTUALLY
// FOLLOWS AND PROJECTS THE MEASURED OUTLINE ... JUST BECAUSE ITS LARGE SCALE
// IT IS REALLY NOTICEABLE IF ITS OFF". The old two-capsule proxy projected a
// 1.06 m-wide lozenge nose to tail; the real plan-view signature — two thin
// runners at the stance, one wide belt on the centreline aft, a broad hood
// forward, the tunnel/seat spine, a leaning rider — is what these six carry.
//
// PRIMITIVE CHOICE, STATED: capsules, not boxes. The receiving shader's soft
// penumbra needs the SIGNED CLEARANCE of the sun ray past the occluder
// surface; for a capsule that is one closed-form segment-segment distance
// (the shipped loop), while a ray-to-OBB clearance is a branchy multi-case
// distance the whole planet + patch + lake mirror would pay per fragment per
// caster, plus 3+ extra vec4 uniforms per caster. The rounding a capsule adds
// is bounded by each proxy's own radius (0.0675 m at a ski tip — which really
// IS rounded/upturned; 0.19 m at the belt ends — which hide under the tunnel
// shadow), below the shadow's own penumbra at any slant past ~6 m. "ONE SHAPE
// ON PURPOSE" (top of file) survives: the fragment loop stays branchless.
//
// GEOMETRY SOURCES — the NO GUESSING ledger:
//   - kernel chart (sim/sled.h:398, MEASURED-primary): stance, ski width,
//     track width — passed in by the app (this header stays sim-free).
//   - the SHIPPED GLB (assets/sled/indy650.glb, accessor min/max through the
//     node transforms, measured 2026-08-28): every model-frame constant
//     below. The GLB was authored to the kernel chart and agrees where they
//     overlap (kingpin x = stance/2 = 0.4635 EXACT, ski box 0.135 x 0.950 =
//     ski_width_m x ski_len_m, belt width 0.381 vs track_width_m 0.38); the
//     ski's VISUAL span sits ~0.29 m ahead of the kernel CONTACT patch
//     (mesh z 0.670..1.620 vs patch centre 0.86) — the drawn machine is the
//     GLB, the shadow follows the DRAWN machine, so the GLB span wins.
//   - the drawn machine's own mount + articulation law
//     (render/sled_model.cpp pose_pass + mount): model y=0 lands at body
//     y = -(cg_h - sag0); unsprung parts translate by (susp_x - sag0); skis
//     yaw about their kingpins by steer * ski_steer_max_rad. Reproduced here
//     term for term so the shadow cannot disagree with the pixels above it.
//
// Model frame (the GLB's): +Z = nose, +X = the rider's LEFT, y=0 = the
// running surface at authored (sag0) stance. Model -> body is R_y(180 deg)
// plus the mount drop — computed in one place below, never as ad-hoc sign
// flips.

// ★ MEASURED MODEL-FRAME CONSTANTS (indy650.glb, 2026-08-28 — method above).
namespace sled_glb {
// ski_L/ski_R meshes: x half-width 0.0675 (= chart ski_width_m/2, so the
// live radius comes from the CHART via SledShadowDims, not retyped here).
constexpr double kSkiZMin = 0.670, kSkiZMax = 1.620;  // visual ski span
constexpr double kSkiBotY = 0.001;                    // runner underside
constexpr double kSkiPivotZ = 0.955;  // CH_kingpin_L/R accumulated origin
// track_belt mesh — the visible LOOP footprint (z span 1.405), NOT the
// 1.14 m kernel contact patch (the ledger's named 2.5x trap is the belt
// CIRCUMFERENCE; the plan-view loop is what a shadow shows).
constexpr double kBeltZMin = -1.009, kBeltZMax = 0.396;
// tunnel mesh (x +-0.210, underside y 0.240) + seat (bbox top y 0.663):
// stacked they span y 0.240..0.663 = 0.423, and a radius-0.210 capsule
// stands 0.420 — top lands 3 mm shy of the seat crown. Near-exact fit.
constexpr double kTunnelHalfW = 0.210;
constexpr double kTunnelZMin = -1.010, kTunnelZMax = 0.330;
constexpr double kTunnelBotY = 0.240;
constexpr double kSeatTopY = 0.663;  // seat bbox top (front hump crown)
// pan_skin (x +-0.449, underside y 0.127) + indy650_shroud (top y 0.733):
// the hood/belly-pan stack, the machine's widest plan footprint.
constexpr double kPanHalfW = 0.449;
constexpr double kPanZMin = 0.200, kPanZMax = 1.340;
constexpr double kPanBotY = 0.127, kShroudTopY = 0.733;
// rest pelvis z (render/rider_pose.h MEASURED -0.584840) and the head's
// measured forward offset (row-6 breath anchor: body -0.10 => model +0.10).
constexpr double kPelvisZ = -0.5848;
// ★ 2026-08-29 CORRECTION. kHeadZ WAS 0.10 AND IT WAS NEVER A MEASUREMENT: it
// was the row-6 breath anchor's offset — 0.10 m out from the NECK, a RELATIVE
// value — used as an ABSOLUTE head coordinate. The R4a session measured the
// posed helmet off this same GLB (assets/character/sudburian_src/
// measure_helmet_crown.py, CPU-skinned, asserts 44 skin joints = Sudburian):
// helmet_sudburian spans z -0.440089..-0.034555. +0.10 is OUTSIDE the helmet's
// entire z extent. kPelvisZ matches that table's pelvis z (-0.5848) EXACTLY,
// which is what proves the two frames are the same one — so this is a
// like-for-like correction, not a datum conversion.
constexpr double kHeadZ = -0.237322;  // helmet z-extent midpoint, MEASURED
}  // namespace sled_glb

struct SledShadowDims {
    // -- kernel chart reads (sim::SledParams, app-passed; sim/sled.h:398) --
    double cg_h = 0.0;         // cg_height_m (0.564)
    double half_stance = 0.0;  // stance_m/2 — equals the GLB kingpin |x|
                               // 0.4635 EXACTLY (pinned in test_snow_shadows)
    double ski_half_w = 0.0;   // ski_width_m/2 (0.0675; GLB ski box agrees)
    double track_half_w = 0.0;  // track_width_m/2 (0.19; GLB belt 0.1905)
    // -- the drawn machine's mount law (render/sled_model.cpp) --------------
    double sag0 = 0.0;  // render::sled_sag0_m() — the ONE static-sag source
    // -- articulation, straight kernel-state reads --------------------------
    double steer_rad = 0.0;  // steer_actual * ski_steer_max_rad(), + = LEFT
                             // (the SAME product the drawn ski yaws by)
    double susp_m[3] = {0.0, 0.0, 0.0};  // susp_x: [0] = model +X ski
                                         // (kernel L — sled_model.cpp binds
                                         // kernel L to the +X node by
                                         // position), [1] = -X ski,
                                         // [2] = track skid
    double rider_lat_m = 0.0;  // + LEFT = model +X (sim/sled.h:1096)
    double rider_fwd_m = 0.0;  // + forward = model +Z
    double rider_up_m = 0.0;   // + standing
    // -- rider silhouette ---------------------------------------------------
    // ★ 1.574660 m — the seated helmet CROWN, MEASURED (R4a's
    // assets/character/sudburian_src/measure_helmet_crown.py, re-run in this
    // tree against the byte-identical GLB; all five dent variants agree to
    // 0.0 mm, so the canonical is helmet_sudburian and "which dent" is not a
    // ruling). ⚠ IT WAS 1.25 AND THAT WAS NOT A MEASUREMENT — the comment
    // claimed "MEASURED" while COMPOSING it from rider_pose.h's cowl top 0.892
    // plus a head-group allowance. rider_pose.h holds helmet CLEARANCE numbers
    // (lowest point, cowl top line); it has no crown. The shadow's head was
    // 0.32 m short. Under NO GUESSING a composed number wearing a MEASURED
    // label is the failure mode, not the fallback.
    double helmet_top = 0.0;
    double rider_radius = 0.0;  // DOCUMENTED APPROXIMATION — no measured
                                // clothed-torso width exists in the tree;
                                // the caller states its value and why
};

// Append the sled's six capsules at pose (pos = CG, basis = body->world,
// body frame +X right / +Y up / -Z fwd — sim/sled.h:77). Emission order:
// ski model+X (kernel L), ski model-X, track belt, tunnel+seat, hood, rider.
inline void sled_shadow_proxies(std::vector<ShadowCaster>& out,
                                const glm::dvec3& pos, const glm::dmat3& basis,
                                const SledShadowDims& d) {
    namespace g = sled_glb;
    // model -> world: R_y(180) (model +Z nose -> body -Z fwd, model +X left
    // -> body -X) then the mount drop — model y=0 lands at body
    // -(cg_h - sag0), the sled_model.cpp DRIVE-2 law, so the shadow sits at
    // the same height the drawn machine does (the old proxy used -cg_h and
    // inherited the "sag0 too deep" defect that fix removed from the mesh).
    const double drop = d.cg_h - d.sag0;
    auto W = [&](const glm::dvec3& m) {
        return pos + basis * glm::dvec3(-m.x, m.y - drop, -m.z);
    };
    // SKIS — thin runners at the stance, radius = the chart's half ski
    // width. Each yaws about the vertical through ITS OWN kingpin by the
    // same angle the drawn ski does (R_y(180) commutes with a +Y spin, so
    // replicating the model-frame rotation then mapping is exact), and rides
    // its own suspension channel: axis y follows susp_x - sag0 exactly like
    // the CH_susp translation in pose_pass — ski bottom lands at body
    // (susp_x - cg_h), the kernel's own contact height.
    const double sr = d.ski_half_w;
    const double ca = std::cos(d.steer_rad), sa = std::sin(d.steer_rad);
    for (int side = 0; side < 2; ++side) {
        const double sx = (side == 0 ? 1.0 : -1.0) * d.half_stance;
        const double sy = g::kSkiBotY + sr + (d.susp_m[side] - d.sag0);
        auto ski_pt = [&](double z) {  // +Y spin about the kingpin: +steer
            const double rz = z - g::kSkiPivotZ;  // takes +Z toward +X (left)
            return glm::dvec3(sx + sa * rz, sy, g::kSkiPivotZ + ca * rz);
        };
        out.push_back({W(ski_pt(g::kSkiZMin + sr)), W(ski_pt(g::kSkiZMax - sr)),
                       sr});
    }
    // TRACK BELT — the wide centreline belt, radius = the chart's half track
    // width, spanning the measured loop footprint, on the skid's suspension
    // channel. Belt underside authored at model y 0 => axis one radius up.
    const double tr = d.track_half_w;
    const double ty = tr + (d.susp_m[2] - d.sag0);
    out.push_back({W({0.0, ty, g::kBeltZMin + tr}),
                   W({0.0, ty, g::kBeltZMax - tr}), tr});
    // TUNNEL + SEAT — the narrow aft spine, chassis-fixed (sprung mass: no
    // susp channel, exactly like the drawn tunnel). Radius = the measured
    // tunnel half-width; axis set so the capsule spans tunnel underside to
    // 3 mm shy of the measured seat crown (see kSeatTopY note).
    const double cr = g::kTunnelHalfW;
    const double cy = g::kTunnelBotY + cr;
    out.push_back({W({0.0, cy, g::kTunnelZMin + cr}),
                   W({0.0, cy, g::kTunnelZMax - cr}), cr});
    // HOOD — the broad forward pan/shroud stack, chassis-fixed. Radius = the
    // measured pan half-width (plan footprint exact: 0.898 x the measured z
    // span). DOCUMENTED APPROXIMATION: a capsule is round, so it stands
    // 0.898 m tall where the measured stack is 0.606 (pan 0.127 to shroud
    // 0.733); the axis is centred on that stack, splitting the error both
    // ways — top reaches 0.879, 1.3 cm shy of the measured cowl top 0.892
    // (so the cowl needs no proxy of its own), bottom dips 0.019 below the
    // authored ground line (reads as contact occlusion at the pan, not a
    // hole). Top-down the footprint is exact; only a grazing sun sees the
    // extra height, where the shadow is already a long smear.
    const double hr = g::kPanHalfW;
    const double hy = 0.5 * (g::kPanBotY + g::kShroudTopY);
    out.push_back({W({0.0, hy, g::kPanZMin + hr}), W({0.0, hy, g::kPanZMax - hr}),
                   hr});
    // RIDER — a forward-leaning capsule from the seat to the helmet: bottom
    // touches the measured seat crown at the measured rest-pelvis z, top
    // reaches the measured helmet height over the bars (head 0.10 m ahead of
    // the CG, the row-6 anchor). The whole capsule translates with the
    // kernel's rider lean channels — the same (lat, up, fwd) model-frame
    // vector pose_pass feeds the rig root — so a hung-off rider's shadow
    // hangs off with him.
    const double rr = d.rider_radius;
    const glm::dvec3 lean{d.rider_lat_m, d.rider_up_m, d.rider_fwd_m};
    out.push_back(
        {W(glm::dvec3(0.0, g::kSeatTopY + rr, g::kPelvisZ) + lean),
         W(glm::dvec3(0.0, d.helmet_top - rr, g::kHeadZ) + lean), rr});
}

}  // namespace render
