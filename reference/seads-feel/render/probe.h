#pragma once

#include <algorithm>
#include <glm/glm.hpp>

// Target-visibility probe oracle (docs/world_build_plan.md §4; little_planet
// Stage 0). "The one instrument the art direction most needs, and nobody had":
// a deterministic measure of how well a plane POPS against the procedural
// ground/sky behind it, so every Legibility sortie carries a NUMBER instead of
// a vibe — and the ground-busyness-vs-plane-pop collision is caught the moment
// field contrast is tuned, not four phases later.
//
// PURE and raylib-free (seads_render_core) so the gate pins the arithmetic
// headlessly; the framebuffer READBACK that feeds it is caller glue in the app
// (app/main.cpp --probe), like the rest of main.cpp — no ctest runs seads.exe
// (the honest ledger). This header owns ALL the deterministic math the probe
// depends on — placement geometry AND the contrast reduction — which IS the
// mutation-verified part (Fable red-team P1-2: no load-bearing pure math left
// in the untested glue).
//
// Two contrast channels, matching the art direction (world_build_plan §6): a
// plane pops by LUMINANCE (silhouette / wing-glint read) AND by CHROMA (the
// saturated livery is the ONLY color in a B&W world). A plane is legible if it
// beats the local background patch on EITHER channel — so the probe reports
// both, never collapsing them to one score, and reports the PEAK pixel beside
// the mean so a small-but-bright target at range still registers (P1-1: the
// astern plane is sub-pixel at 1-3 km, so a mean-only patch conflates "small"
// with "low contrast" and the far range cells carry no signal).

namespace render {

// Per-pixel decomposition, inputs in [0,1] (a framebuffer byte / 255).
// Rec.709 luma; chroma = a cheap saturation proxy (max-min): a GRAY background
// patch has chroma ~0 at ANY brightness, so chroma isolates "the plane is the
// only saturated thing" exactly.
inline double pixel_luminance(double r, double g, double b) {
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}
inline double pixel_chroma(double r, double g, double b) {
    const double mx = std::max(r, std::max(g, b));
    const double mn = std::min(r, std::min(g, b));
    return mx - mn;
}

// Mean AND extremes of a sampled patch, accumulated pixel-by-pixel by the
// readback glue then handed to the oracle. The extremes drive the PEAK stat
// (the brightest/darkest/most-saturated pixel) so a sub-pixel target isn't
// averaged into invisibility.
struct PatchStats {
    double lum_sum = 0.0;
    double chroma_sum = 0.0;
    double max_lum = 0.0;
    double min_lum = 1.0;
    double max_chroma = 0.0;
    long long n = 0;
    void add(double r, double g, double b) {
        const double l = pixel_luminance(r, g, b);
        const double ch = pixel_chroma(r, g, b);
        lum_sum += l;
        chroma_sum += ch;
        if (l > max_lum) max_lum = l;
        if (l < min_lum) min_lum = l;
        if (ch > max_chroma) max_chroma = ch;
        ++n;
    }
    double mean_lum() const {
        return n > 0 ? lum_sum / static_cast<double>(n) : 0.0;
    }
    double mean_chroma() const {
        return n > 0 ? chroma_sum / static_cast<double>(n) : 0.0;
    }
};

// The contrast verdict: plane patch vs local background patch.
struct ProbeContrast {
    double lum_plane = 0.0, lum_bg = 0.0;
    double chroma_plane = 0.0, chroma_bg = 0.0;
    // Michelson luminance contrast |Lp-Lb|/(Lp+Lb) in [0,1] (0 = invisible by
    // brightness, 1 = one patch black). SYMMETRIC by design: a dark plane on
    // bright snow scores exactly like a bright plane on dark bush — legibility
    // is |difference|, NOT sky>ground (the AT-6 "don't gate a phantom" lesson;
    // moonlit winter snow is legitimately brighter than the plane).
    double lum_michelson = 0.0;  // MEAN plane pixel vs mean background
    // The plane's peak EXCEEDANCE of the background's OWN range (Fable r2 P1-3:
    // comparing a plane MAX against a bg MEAN reports "plane pops" for ANY
    // textured ground, because max-of-N > mean — it INVERTS the instrument as
    // the ground gets busy). One-sided vs the SAME-order bg extreme, so the
    // ground-variance floor cancels: a plane pops only if its brightest pixel
    // beats the brightest ground pixel, or its darkest beats the darkest.
    double lum_peak_michelson = 0.0;
    // Chroma lead: how much MORE saturated the plane is than the background.
    // The background is gray in a B&W world, so this ~= the plane's own chroma
    // — the "saturated livery beats a monochrome field" read, independent of
    // brightness. May go NEGATIVE (colorful ground, gray plane) — not clamped.
    double chroma_delta = 0.0;  // MEAN
    double chroma_peak = 0.0;  // most-colored plane pixel BEYOND the ground max
    // All contrasts are 0 (no-data) unless BOTH patches have samples (P1-2: an
    // empty background must not read as michelson 1.0 = "fully visible").
};

ProbeContrast probe_contrast(const PatchStats& plane, const PatchStats& bg);

// Where to place the probe bandit so it silhouettes against the procedural
// GROUND on the R-radius sphere (Fable red-team P0-1: a fixed small depression
// below local HORIZONTAL points at the sky — the horizon DIPS
// acos(R/|pos|) = 14.7 deg at 500 m .. 33.6 deg at 3 km). We depress the
// sightline BELOW the horizon (dip + margin) and place the target at `range`
// along it; the pixels around and beyond the target are then ground.
//
// TWO radii (Fable r2 P0-1 — using one for both puts the ray above the base
// sphere while flagging the row valid):
//  - `base_radius` (R): drives the dip and the "reaches guaranteed ground"
//    test. Terrain lives in [R, R+relief], so a ray that descends below R MUST
//    cross terrain; a ray that only grazes the relief top may pass over a
//    valley into the sky.
//  - `terrain_radius` (R+relief, conservative): the burial test — the target is
//    valid only if it sits ABOVE the highest possible terrain.
//
// `pos` = player world position, `nose` = player world nose dir (level-ish;
// projected to the local horizontal plane so a pitched airframe still depresses
// from true level), `extra_depress` = margin below the horizon (Below) / above
// the local horizontal (Above) [rad], `range` = slant distance to the bandit
// [m]. PURE + testable (the arithmetic P0-1 lived in).
//
// TWO geometries (Stage 2, plan P1 "probe covers GEOMETRY"):
//  - Below: DEPRESS below the horizon so the bandit silhouettes against GROUND.
//    Valid measurement needs `below_horizon && above_terrain`.
//  - Above: ELEVATE above local horizontal so the ray climbs off the sphere and
//    the bandit silhouettes against SKY (the look-up cell). `below_horizon`
//    stays false; validity needs `clear_of_terrain` (the eye above the relief
//    top — an eye inside the relief shell can be occluded by a near peak before
//    the upward ray climbs clear).
enum class ProbeGeometry { Below, Above };

struct ProbePlacement {
    glm::dvec3 dir{0.0, 0.0, -1.0};    // world unit sightline from the player
    glm::dvec3 target{0.0, 0.0, 0.0};  // world bandit position at `range`
    double horizon_dip = 0.0;          // acos(base_radius/|pos|) [rad]
    double depress = 0.0;      // depression below horizontal [rad] (Above: < 0)
    double ground_dist = 0.0;  // slant distance to base_radius sphere [m]
    double target_alt = 0.0;   // |target| - base_radius [m]
    bool below_horizon =
        false;  // the sightline reaches GUARANTEED ground (< R)
    bool above_terrain =
        false;  // target sits above the relief top (not buried)
    bool clear_of_terrain =
        false;  // eye above relief top (Above-geom validity)
};

ProbePlacement probe_placement(const glm::dvec3& pos, const glm::dvec3& nose,
                               double base_radius, double terrain_radius,
                               double extra_depress, double range,
                               ProbeGeometry geom = ProbeGeometry::Below);

// The probe camera looks straight AT the injected bandit so it stays on-screen
// at ANY altitude — a level chase camera loses a high-alt below-horizon target
// off the bottom of the frame (the (dip + margin) depression exceeds the
// half-FOV above ~1800 m). Faithful to gameplay (the player pitches to engage).
// PURE; pinned in test_probe so the on-screen guarantee is not left in the
// un-ctested app glue (Fable P1-1). The app sets pose.target = place.target,
// which makes the rendered AND measured camera-forward this exact vector.
inline glm::dvec3 probe_camera_forward(const ProbePlacement& place,
                                       const glm::dvec3& eye) {
    return glm::normalize(place.target - eye);
}

// NDC -> integer pixel, the same lens-shift mapping draw.cpp's reticle uses
// (the merge left the draw path on the float `to_pxf` for the S-reticle; this
// int version differs only by <=1 px truncation, and the probe samples a patch
// around the center, so it stays consistent with the drawn scene). NDC +y = up;
// the lens shift the 3D frustum applied is subtracted so overlay locks to it.
struct PixelCoord {
    int x = 0;
    int y = 0;
};
PixelCoord ndc_to_pixel(double ndc_x, double ndc_y, double lens_shift_ndc,
                        int screen_w, int screen_h);

}  // namespace render
