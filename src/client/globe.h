// SEADS globe projection math (Step 5 client). PRESENTATION-ONLY — runs entirely outside the
// kernel, det_math and the world_hash, so it is free to use libm trig (std::sin/cos/...): it
// never feeds a bit back into the deterministic sim. Maps sphere (lat, lon, alt) on the ATM
// rail (R = 15 km) into a right-handed world for a camera, and projects to normalized screen
// coordinates. Pure functions + small POD types so every invariant is unit-testable headlessly
// (no window, no GPU) — the GUI/web viewers are thin shells over exactly these functions.
//
// Frame convention: Y is the globe's polar (spin) axis (latitude ±90° -> ±Y). Longitude 0 lies
// on +X; longitude +90° on +Z. Right-handed.
#pragma once
#include <cmath>

namespace seads {
namespace client {

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
    double n = length(a);
    return n > 0 ? Vec3{a.x / n, a.y / n, a.z / n} : a;
}

constexpr double PI_C   = 3.14159265358979323846;
constexpr double DEG2RAD_C = PI_C / 180.0;

// Surface/air point on the globe: latitude & longitude in RADIANS, altitude in metres above the
// rail surface, R the rail radius. (r = R + alt) outward along the geographic direction.
inline Vec3 geo_to_cartesian(double lat_rad, double lon_rad, double alt_m, double R) {
    double r = R + alt_m;
    double cl = std::cos(lat_rad);
    return {r * cl * std::cos(lon_rad), r * std::sin(lat_rad), r * cl * std::sin(lon_rad)};
}

// Inverse of geo_to_cartesian (for round-trip tests / picking). Returns lat,lon in radians.
inline void cartesian_to_geo(Vec3 p, double R, double& lat_rad, double& lon_rad, double& alt_m) {
    double r = length(p);
    lat_rad = (r > 0) ? std::asin(p.y / r) : 0.0;
    lon_rad = std::atan2(p.z, p.x);
    alt_m = r - R;
}

// Degrees convenience (the wire carries degrees).
inline Vec3 geo_deg_to_cartesian(double lat_deg, double lon_deg, double alt_m, double R) {
    return geo_to_cartesian(lat_deg * DEG2RAD_C, lon_deg * DEG2RAD_C, alt_m, R);
}

// True if a point on (or just above) the sphere faces the camera — i.e. it is on the near
// hemisphere and not occluded by the globe body. Outward normal at p is p itself (from origin).
inline bool on_near_side(Vec3 surface_point, Vec3 eye) {
    return dot(eye - surface_point, normalize(surface_point)) > 0.0;
}

// Orbit camera placement: azimuth/elevation in radians around `target` at `dist` metres.
// elevation +pi/2 looks straight down the +Y pole. Returns the eye position.
inline Vec3 orbit_eye(double az_rad, double el_rad, double dist, Vec3 target) {
    double ce = std::cos(el_rad);
    Vec3 dir{ce * std::cos(az_rad), std::sin(el_rad), ce * std::sin(az_rad)};
    return target + dir * dist;
}

struct Camera {
    Vec3 eye{0, 0, 0};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 1, 0};
    double fov_rad = 1.0;   // vertical field of view
    double aspect = 16.0 / 9.0;
    double znear = 1.0;
    double zfar = 1.0e6;
};

// Project a world point to normalized device coordinates. sx,sy in [-1,1] when on screen
// (sy up). `depth` is view-space distance (>0 in front of the camera). Returns false when the
// point is behind the camera (depth <= 0); sx,sy are then meaningless.
inline bool project(const Camera& cam, Vec3 world, double& sx, double& sy, double& depth) {
    // Right-handed look-at basis: forward = target-eye, right = forward x up, up' = right x forward.
    Vec3 f = normalize(cam.target - cam.eye);
    Vec3 r = normalize(cross(f, cam.up));
    Vec3 u = cross(r, f);
    Vec3 rel = world - cam.eye;
    double vz = dot(rel, f);   // distance in front of camera
    depth = vz;
    if (vz <= 0) { sx = sy = 0; return false; }
    double vx = dot(rel, r);
    double vy = dot(rel, u);
    double tan_half = std::tan(cam.fov_rad * 0.5);
    sx = (vx / vz) / (tan_half * cam.aspect);
    sy = (vy / vz) / tan_half;
    return true;
}

}  // namespace client
}  // namespace seads
