#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "render/camera.h"

// ★ ROAD-REPAIR E2 -- THE FLASH INSTRUMENT'S CAMERA.
//
// docs/road_repair/onaping_eyesores.md §3.4 said it out loud: "No z-fight
// FLICKER was captured: a screenshot is one frame."  The road deck pass
// (render/ribbons.cpp, glPolygonOffset(-2,-4)) and the bank pass
// (render/bank_mesh.cpp, the SAME (-2,-4)) carry ZERO relative depth bias, so
// bank-over-deck and deck-over-deck (14,076 overlapping pairs at 4,646 sites)
// are decided by a 24-bit fixed depth buffer with near 2 m / far 60 km.  A
// depth TIE is invisible in a still frame and screams in motion, and the two
// rungs that follow (F3 depth bias, F2 junction cut) cannot be graded without a
// number for it.
//
// Two things turn the tie into a measurable flicker:
//
//   1. A GRAZING camera.  Top-down, the deck and the bank differ by the ~0.45 m
//      [ribbons] lift_m over a tiny screen-space slope, and the offset's
//      slope-scaled term is near zero.  At ~8 deg the same pair of triangles
//      spans hundreds of depth-quantisation steps per pixel row and the
//      slope-scale term dominates -- which is exactly the attitude Chad flies
//      and drives at.
//   2. A MOVING EYE.  A tie broken one way at eye E is broken the other way at
//      E + 1 cm: the quantised depths cross.  One frame cannot see that; N
//      frames of the SAME camera with the eye walked a centimetre can.
//
// This header is the pure geometry of that camera.  It BUILDS NOTHING and
// CHANGES NOTHING; the sites are census rows (tools/road_repair/flash_sites.py)
// and every dial is measured, not guessed.
//
// PURE: glm + std + render/camera.h (a POD pose). ZERO raylib.

namespace app {

// A flash camera site: a MULTI-LEG junction picked out of
// docs/road_repair/onaping_junction_overlap.tsv by
// tools/road_repair/flash_sites.py (clusters of overlapping deck pairs inside
// 40 m, ranked by pair count).  `pairs` and `deck_overlap_max_m` are that
// script's output for the site, carried here so the camera can never drift
// from the census row that justified it.
struct FlashSite {
    const char* name;
    glm::dvec3 dir;  // unit surface direction of the junction node
    int pairs;       // overlapping drawn-deck segment pairs inside 40 m
    double deck_overlap_max_m;
    double valley_km;  // range from the Valley pump (the census's own column)
    const char* note;
};

// THE THREE SITES.  Re-derive with:
//   py -3 tools/road_repair/flash_sites.py --top 4
inline const FlashSite* flash_sites(int* n_out) {
    static const FlashSite kSites[] = {
        {"B",
         {-0.920102836, 0.391398752, 0.014757628},
         3,
         21.1,
         3.397,
         "Onaping site B -- the urban junction cluster the eyesore rung "
         "carried its verdict on (47.9 m from that rung's spawn dir)"},
        {"J1",
         {0.551621693, -0.421497713, 0.719759116},
         26,
         26.1,
         32.093,
         "the DENSEST deck-overlap site on the planet: 26 overlapping pairs "
         "inside 40 m"},
        {"J2",
         {0.821380329, -0.209902062, 0.530354107},
         17,
         27.6,
         36.922,
         "the second densest, and the largest single deck overlap of the "
         "three (27.6 m)"},
    };
    if (n_out != nullptr)
        *n_out = static_cast<int>(sizeof(kSites) / sizeof(kSites[0]));
    return kSites;
}

// Name lookup, or "x,y,z" parsed as a raw direction (so a fourth site needs no
// rebuild). Returns nullptr when the name matches nothing.
inline const FlashSite* flash_site_by_name(const char* name,
                                           FlashSite* scratch) {
    if (name == nullptr || name[0] == '\0') return nullptr;
    int n = 0;
    const FlashSite* s = flash_sites(&n);
    for (int i = 0; i < n; ++i)
        if (std::strcmp(s[i].name, name) == 0) return &s[i];
    double x = 0.0, y = 0.0, z = 0.0;
    if (scratch != nullptr && std::sscanf(name, "%lf,%lf,%lf", &x, &y, &z) == 3) {
        const glm::dvec3 d(x, y, z);
        if (glm::length(d) > 1.0e-9) {
            scratch->name = "custom";
            scratch->dir = glm::normalize(d);
            scratch->pairs = 0;
            scratch->deck_overlap_max_m = 0.0;
            scratch->valley_km = 0.0;
            scratch->note = "caller-supplied direction";
            return scratch;
        }
    }
    return nullptr;
}

// The rig's dials. Every default is justified in
// docs/road_repair/onaping_flash_E2.md §2:
//   dist_m 70 + eye_h_m 10 => atan(10/70) = 8.13 deg of grazing, the shallow
//   end of the 30-70 m / 10-20 m window, i.e. the worst case for the depth
//   tie. jitter_m 0.02 is 2 cm, ~1/1000 of the near plane -- far below any
//   geometric change and far above one depth ULP at this range.
struct FlashCamParams {
    double dist_m = 70.0;    // ground range from the junction node to the eye
    double eye_h_m = 10.0;   // eye height above the local drawn ground
    double az_deg = 0.0;     // which side of the node the eye stands on
    double target_h_m = 0.0; // aim at the deck surface itself
    double jitter_m = 0.02;  // per-frame eye walk ALONG the view direction
    int frames = 8;
};

// A deterministic tangent basis at `n`: never a function of any sim state, so
// two runs with the same site give the same two vectors to the bit.
inline void flash_tangents(const glm::dvec3& n, glm::dvec3* t1, glm::dvec3* t2) {
    const glm::dvec3 a = (std::fabs(n.z) < 0.9) ? glm::dvec3(0.0, 0.0, 1.0)
                                                : glm::dvec3(1.0, 0.0, 0.0);
    *t1 = glm::normalize(glm::cross(a, n));
    *t2 = glm::cross(n, *t1);
}

// THE CAMERA.  `ground_r` returns the DRAWN ground radius at a surface
// direction (the caller injects render::drawn_radius_at through the snowpack
// field's own binding -- the same surface the census measured, never a second
// one).  `frame` walks the eye `frame * jitter_m` metres along the view.
inline render::CameraPose flash_camera(
    const glm::dvec3& node_dir, const FlashCamParams& p, int frame,
    const std::function<double(glm::dvec3)>& ground_r) {
    const glm::dvec3 n = glm::normalize(node_dir);
    glm::dvec3 t1(0.0), t2(0.0);
    flash_tangents(n, &t1, &t2);
    const double az = p.az_deg * (3.14159265358979323846 / 180.0);
    const glm::dvec3 h = t1 * std::cos(az) + t2 * std::sin(az);

    const double r_node = ground_r(n);
    const glm::dvec3 target = n * (r_node + p.target_h_m);

    // Step out along the surface, then re-seat on the ground THERE: over 70 m
    // on R = 15 km the chord drops 0.16 m, and the terrain itself moves more.
    const glm::dvec3 ne = glm::normalize(n * r_node + h * p.dist_m);
    const double r_eye = ground_r(ne);
    glm::dvec3 eye = ne * (r_eye + p.eye_h_m);

    const glm::dvec3 view = glm::normalize(target - eye);
    eye += view * (static_cast<double>(frame) * p.jitter_m);

    render::CameraPose pose;
    pose.eye = eye;
    pose.target = target;
    pose.up = glm::normalize(eye);
    return pose;
}

// The grazing angle actually achieved [deg] -- reported, not assumed, because
// the terrain under the eye is not the terrain under the node.
inline double flash_graze_deg(const render::CameraPose& pose) {
    const glm::dvec3 v = pose.target - pose.eye;
    const glm::dvec3 up = glm::normalize(pose.eye);
    const double d = glm::length(v);
    if (!(d > 1.0e-9)) return 0.0;
    return std::asin(std::clamp(-glm::dot(v, up) / d, -1.0, 1.0)) *
           (180.0 / 3.14159265358979323846);
}

}  // namespace app
