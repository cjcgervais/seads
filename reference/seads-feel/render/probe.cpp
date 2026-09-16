#include "render/probe.h"

#include <cmath>

namespace render {

namespace {
// Michelson contrast of two non-negative luminances (0 if both ~0).
double michelson(double a, double b) {
    const double s = a + b;
    return s > 1e-9 ? std::fabs(a - b) / s : 0.0;
}
}  // namespace

ProbeContrast probe_contrast(const PatchStats& plane, const PatchStats& bg) {
    ProbeContrast c;
    c.lum_plane = plane.mean_lum();
    c.lum_bg = bg.mean_lum();
    c.chroma_plane = plane.mean_chroma();
    c.chroma_bg = bg.mean_chroma();
    // No contrast without BOTH patches (P1-2: an empty bg would make
    // michelson(plane,0) = 1.0 — a spurious "fully visible").
    if (plane.n > 0 && bg.n > 0) {
        c.lum_michelson = michelson(c.lum_plane, c.lum_bg);
        c.chroma_delta = c.chroma_plane - c.chroma_bg;
        // Peak EXCEEDANCE of the background's OWN range (P1-3): a plane pops
        // only if its brightest pixel beats the brightest GROUND pixel, or its
        // darkest beats the darkest — comparing against the bg's SAME-order
        // extreme cancels the ground-texture variance floor (a plane MAX vs a
        // bg MEAN reads as a pop on ANY busy ground, with no plane at all).
        const double bright = plane.max_lum > bg.max_lum
                                  ? michelson(plane.max_lum, bg.max_lum)
                                  : 0.0;
        const double dark = plane.min_lum < bg.min_lum
                                ? michelson(plane.min_lum, bg.min_lum)
                                : 0.0;
        c.lum_peak_michelson = std::max(bright, dark);
        c.chroma_peak = plane.max_chroma > bg.max_chroma
                            ? plane.max_chroma - bg.max_chroma
                            : 0.0;
    }
    return c;
}

ProbePlacement probe_placement(const glm::dvec3& pos, const glm::dvec3& nose,
                               double base_radius, double terrain_radius,
                               double extra_depress, double range,
                               ProbeGeometry geom) {
    ProbePlacement p;
    const double r = glm::length(pos);
    if (r < 1e-9) return p;  // degenerate (origin) — leave defaults
    const glm::dvec3 up = pos / r;

    // Local-horizontal forward: strip any pitch so the depression is from true
    // level. Fall back to an arbitrary perpendicular if the nose is ~vertical.
    glm::dvec3 fwd_h = nose - glm::dot(nose, up) * up;
    if (glm::length(fwd_h) < 1e-6) {
        const glm::dvec3 seed = std::abs(up.x) < 0.9
                                    ? glm::dvec3{1.0, 0.0, 0.0}
                                    : glm::dvec3{0.0, 1.0, 0.0};
        fwd_h = seed - glm::dot(seed, up) * up;
    }
    fwd_h = glm::normalize(fwd_h);

    // Horizon dip against the BASE sphere R: descending below R crosses
    // GUARANTEED terrain (surface >= R everywhere). acos domain-guarded (r > R
    // for any positive altitude, so this only clamps under pathological input).
    const double cd = std::clamp(base_radius / r, -1.0, 1.0);
    p.horizon_dip = std::acos(cd);

    if (geom == ProbeGeometry::Below) {
        // DEPRESS below the horizon so the sightline crosses guaranteed ground.
        p.depress = p.horizon_dip + extra_depress;
        p.dir = glm::normalize(std::cos(p.depress) * fwd_h -
                               std::sin(p.depress) * up);
        // Reaches guaranteed ground? Ray-sphere vs the BASE radius, near root.
        const double b = 2.0 * glm::dot(pos, p.dir);
        const double c0 = r * r - base_radius * base_radius;
        const double disc = b * b - 4.0 * c0;
        if (disc >= 0.0) {
            const double t = (-b - std::sqrt(disc)) * 0.5;  // near intersection
            if (t > 0.0) {
                p.ground_dist = t;
                p.below_horizon = true;
            }
        }
    } else {
        // ELEVATE above local horizontal so the ray climbs OFF the sphere and
        // the bandit silhouettes against SKY (the look-up cell). extra_depress
        // is the elevation above horizontal here; depress is reported negative.
        const double elev = extra_depress;
        p.depress = -elev;
        p.dir = glm::normalize(std::cos(elev) * fwd_h + std::sin(elev) * up);
        // below_horizon stays false: an upward ray reaches no guaranteed
        // ground.
    }

    p.target = pos + p.dir * range;
    // Burial: the target must sit above the highest possible terrain. Direct
    // radius test — unambiguous even when the eye is inside the relief shell
    // (low altitude), where a ray-sphere burial test degenerates.
    p.target_alt = glm::length(p.target) - base_radius;
    p.above_terrain = glm::length(p.target) > terrain_radius;
    // Above-geom validity: the elevated sightline is clear only when the EYE
    // sits above the relief top; an eye inside the relief shell can be occluded
    // by a near peak before the upward ray climbs clear. Harmless for Below.
    p.clear_of_terrain = r >= terrain_radius;
    return p;
}

PixelCoord ndc_to_pixel(double ndc_x, double ndc_y, double lens_shift_ndc,
                        int screen_w, int screen_h) {
    PixelCoord px;
    px.x = static_cast<int>((ndc_x * 0.5 + 0.5) * screen_w);
    px.y = static_cast<int>((0.5 - (ndc_y - lens_shift_ndc) * 0.5) * screen_h);
    return px;
}

}  // namespace render
