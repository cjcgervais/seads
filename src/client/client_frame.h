// SEADS mouse-aim instructor — the planesphere / local-up frame adapter (Step: graft of the
// SOLUTION cascade onto the sealed ATM-Sphere kernel; client-side, PRESENTATION-ONLY).
//
// The sealed kernel stores each aircraft as the 7-tuple (lat, lon, psi, phi, alt, tas, gamma) on
// the ATM sphere. The mouse-aim instructor (and the camera) want a WORLD-CARTESIAN body frame —
// nose/right/up unit vectors + the local vertical — to do aim-error decomposition and bank-to-turn.
// This header is the SINGLE shared reconstruction that turns the kernel tuple into that frame.
//
// It reuses viewer_main's on-screen conventions EXACTLY (the same geo basis with the reflected Z
// that to_display()/local_basis() use, and the same right/up/nose recipe that draw_aircraft() draws
// with) so the aim math, the chase camera, and the drawn attitude all agree. A second, disagreeing
// frame convention anywhere is the bug this file exists to prevent.
//
// libm trig is fine here: this is downstream of the kernel, never feeds a bit back into the sim
// (det_math is a kernel-only rule). Pure functions + POD so it is unit-testable headlessly.
#pragma once
#include <cmath>
#include "globe.h"   // seads::client::Vec3, cross, dot, normalize, PI_C

namespace seads {
namespace client {

// Local geographic tangent basis at (lat, lon) in radians, in the DISPLAY frame (Z reflected to
// match to_display()'s un-mirroring — so a geographic right turn reads as a right turn on screen).
// up = radial outward, north = +lat tangent, east = +lon tangent. This is byte-identical to
// viewer_main.cpp's local_basis(); kept here as the shared home so both the instructor and the
// renderer read ONE definition.
inline void geo_basis(double lat, double lon, Vec3& up, Vec3& north, Vec3& east) {
    double cl = std::cos(lat), sl = std::sin(lat), co = std::cos(lon), so = std::sin(lon);
    up    = Vec3{ cl * co, sl, -cl * so};
    north = Vec3{-sl * co, cl,  sl * so};
    east  = Vec3{-so,      0.0, -co};
}

// The reconstructed world-Cartesian aircraft frame (all unit vectors except pos).
struct AircraftFrame {
    Vec3 pos;              // world position (metres, display frame) = (R + alt) * up
    Vec3 up;               // local_up (radial, outward) — "there is no global up"
    Vec3 fwd_level;        // heading direction in the local horizontal plane (from psi)
    Vec3 nose;             // nose / velocity direction (fwd_level pitched by the bank-canopy, gamma)
    Vec3 right;            // right-wing direction (rolled by bank phi) — the "wing" draw_aircraft uses
    Vec3 body_up;          // lift-vector / canopy-up (rigid, perp to nose & right)
    double speed;          // TAS (m/s), carried for the G-clamp V-division
    double cos_phi_theta;  // dot(body_up, up): the SIGNED gravity credit (never clamp >= 0)
};

// Reconstruct the frame from the kernel tuple. Mirrors draw_aircraft()'s right/up/nose recipe
// (viewer_main.cpp) with pitch = gamma, so the frame equals the drawn attitude bit-for-bit.
inline AircraftFrame make_frame(double lat, double lon, double psi, double phi,
                                double alt, double tas, double gamma, double R) {
    Vec3 up, north, east;
    geo_basis(lat, lon, up, north, east);

    AircraftFrame f;
    f.up = up;
    f.pos = up * (R + alt);
    f.fwd_level = normalize(north * std::cos(psi) + east * std::sin(psi));

    // Level-heading body axes, then rolled by the bank angle phi (positive = right wing down),
    // exactly as draw_aircraft() builds them.
    Vec3 right0 = normalize(cross(f.fwd_level, up));   // level right wing
    Vec3 up0    = cross(right0, f.fwd_level);          // level canopy-up (~ local up)
    f.right   = right0 * std::cos(phi) - up0 * std::sin(phi);           // "wing", rolled
    Vec3 b_up = up0 * std::cos(phi) + right0 * std::sin(phi);           // canopy-up, rolled
    f.nose    = normalize(f.fwd_level * std::cos(gamma) + b_up * std::sin(gamma));  // pitched by gamma
    f.body_up = normalize(cross(f.right, f.nose));     // rigid lift vector, perp to nose & right
    f.speed = tas;
    f.cos_phi_theta = dot(f.body_up, f.up);
    return f;
}

// Rodrigues rotation of v about unit axis a by angle theta (right-handed). Client-side libm.
inline Vec3 rotate_about(Vec3 v, Vec3 a, double theta) {
    double c = std::cos(theta), s = std::sin(theta);
    return v * c + cross(a, v) * s + a * (dot(a, v) * (1.0 - c));
}

// The minimal-rotation carry taking unit vector `from` onto unit vector `to`, applied to v
// (parallel transport on the sphere: from = local_up(t-1), to = local_up(t)). Degenerate-safe:
// near-parallel (dt tiny) -> identity; near-antiparallel is unreachable in one 100 Hz tick.
inline Vec3 transport(Vec3 v, Vec3 from, Vec3 to) {
    Vec3 axis = cross(from, to);
    double s = length(axis);
    if (s < 1e-12) return v;                 // no measurable rotation this tick
    double c = dot(from, to);
    if (c > 1.0) c = 1.0; else if (c < -1.0) c = -1.0;
    return rotate_about(v, axis * (1.0 / s), std::acos(c));
}

}  // namespace client
}  // namespace seads
