#include "render/precip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render {

namespace {

constexpr double kTwoPi = 6.283185307179586;

// C1 smoothstep, edge-order-safe.
double smoothstep(double e0, double e1, double x) {
    double t = e1 > e0 ? (x - e0) / (e1 - e0) : (x >= e1 ? 1.0 : 0.0);
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// A deterministic bit-avalanche hash of an integer world cell -> uint32. The
// lattice jitter + fall-phase offset derive from it, so the base is a pure fn of
// the WORLD cell (world-anchored => no wind; two eyes see the same flake in the
// same world spot). Mixing all three coords avoids axis-planar structure.
std::uint32_t hash_cell(int i, int j, int k) {
    std::uint32_t x = static_cast<std::uint32_t>(i) * 0x8DA6B343u ^
                      static_cast<std::uint32_t>(j) * 0xD8163841u ^
                      static_cast<std::uint32_t>(k) * 0xCB1AB31Fu;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
// A hashed scalar in [0,1) from an ALREADY-MIXED cell hash + a salt (distinct
// salts => independent streams for jitter.x/y/z, the phase offset, the sway and
// the variety, from ONE hash_cell). Taking the cell hash as an argument matters:
// the sampler needs ~10 streams per cell and re-mixing the coordinates ten times
// was a measurable share of the precip pass at 19 000 cells a frame.
double hash01(std::uint32_t h, std::uint32_t salt) {
    std::uint32_t x = h ^ (salt * 0x9E3779B9u);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (x >> 8) * (1.0 / 16777216.0);
}

// A hashed UNIT world direction for the cell (uniform on the sphere). Used only
// as the SEED for the per-cell sway axis below — never drawn, never a measured
// direction.
glm::dvec3 hash_dir(std::uint32_t h, std::uint32_t salt) {
    const double z = 2.0 * hash01(h, salt) - 1.0;
    const double a = kTwoPi * hash01(h, salt + 1u);
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    return glm::dvec3(r * std::cos(a), r * std::sin(a), z);
}

// The per-cell SWAY AXIS: a unit tangent perpendicular to local_up, hashed per
// cell. NO FIXED WORLD AXIS is used to build it — the seed is the cell's own
// hashed direction, so there is no shared reference frame any two cells could
// sway in lockstep about (which is exactly what would read as wind). The second
// seed is the near-degenerate fallback (seed nearly parallel to local_up); its
// switch boundary is per-cell random, never a world-space seam.
glm::dvec3 sway_axis(std::uint32_t h, const glm::dvec3& local_up) {
    glm::dvec3 p = glm::cross(hash_dir(h, 31u), local_up);
    if (glm::length(p) < 0.35) p = glm::cross(hash_dir(h, 41u), local_up);
    const double l = glm::length(p);
    if (l < 1e-12) return glm::dvec3(0.0);  // measure-zero: no sway this frame
    return p / l;
}

}  // namespace

glm::ivec3 precip_center_cell(const glm::dvec3& eye, double cell_size) {
    const double s = cell_size > 0.0 ? cell_size : 1.0;
    return glm::ivec3(static_cast<int>(std::lround(eye.x / s)),
                      static_cast<int>(std::lround(eye.y / s)),
                      static_cast<int>(std::lround(eye.z / s)));
}

double precip_keep_p(double intensity, double density_exp) {
    if (!(density_exp > 0.0)) return 1.0;  // OFF: every cell kept
    return std::pow(std::clamp(intensity, 0.0, 1.0), density_exp);
}

PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double fall_phase,
                           const PrecipFieldParams& fp) {
    const std::uint32_t h = hash_cell(cell.x, cell.y, cell.z);
    // AS-3 DENSITY follows intensity: a hashed per-cell cull, so a flurry is a
    // FEW soft flakes and a squall is a dense fall — instead of the same field
    // at two opacities (which reads as fog, not as snowfall). Monotone in
    // intensity by construction (keep_p = intensity^e is increasing in
    // intensity); intensity == 0 => nothing kept; density_exp == 0 => keep_p is
    // 1 and every cell survives. Tested FIRST because it is one hash and it
    // retires the whole cell — at a 0.10 flurry that is three cells in four
    // skipped before any arithmetic.
    double cull = 1.0;
    if (fp.density_exp > 0.0) {
        const double hc = hash01(h, 23u);
        // Softened step (red-team P2b): a cell fades in over density_soft of
        // keep_p instead of appearing whole. density_soft == 0 reproduces the
        // hard threshold exactly. keep_p <= 0 (intensity 0) kills every cell
        // whatever the hash, including the hash that is exactly 0.
        cull = fp.keep_p <= 0.0
                   ? 0.0
                   : smoothstep(hc - std::max(fp.density_soft, 0.0), hc,
                                fp.keep_p);
        if (cull <= 0.0) return {eye, 0.0, 1.0};
    }
    const double cell_size = fp.cell_size;
    // World-anchored jittered base: the integer cell + a per-cell hashed offset,
    // scaled to world metres. A pure fn of the WORLD cell => it does not move with
    // the eye (only the fall term uses local_up), so flying through the field
    // streams the flakes past the plane for free (no camera-frame snow-globe).
    const glm::dvec3 jit(hash01(h, 1u), hash01(h, 2u), hash01(h, 3u));
    const glm::dvec3 base = (glm::dvec3(cell) + jit) * cell_size;
    // The fall: DOWN along local_up only (NO horizontal advection = Chad's no-wind
    // invariant). Each flake carries a hashed phase offset so their wraps are
    // staggered (not a synchronized blink). It falls exactly one cell_size then
    // wraps; the wrap fade below makes that reset C0.
    const double poff = hash01(h, 7u);
    double f = fall_phase + poff;
    f -= std::floor(f);  // frac -> [0,1)
    glm::dvec3 pos = base - local_up * (f * cell_size);
    // CHEAP REJECT, exact: the sway below moves the flake by at most sway_m, so
    // anything already further than box_half + sway_m from the eye is outside
    // the fade sphere no matter which way it sways. About half of the iterated
    // (2H+1)^3 CUBE lies outside the inscribed fade SPHERE, so this retires ~48%
    // of the cells before the sway/variety arithmetic runs.
    double d = glm::length(pos - eye);
    if (d > fp.box_half + fp.sway_m) return {pos, 0.0, 1.0};
    if (fp.inner_fade_m > 0.0 && d < fp.inner_fade_m - fp.sway_m)
        return {pos, 0.0, 1.0};  // inside the veil's hole, whichever way it sways
    // AS-3 SWAY (NOT WIND). A tiny lateral sinusoid in the tangent plane, driven
    // by the flake's OWN fall phase, on a per-cell hashed axis with a per-cell
    // hashed phase. Its mean over one full fall cycle is EXACTLY zero (the
    // integral of a sine over its period), so the flake has no net displacement
    // and the field has no net direction — the two properties "no wind" actually
    // names. sway_m == 0 => this block is bit-identically absent.
    if (fp.sway_m > 0.0) {
        const double sphase = kTwoPi * (f + hash01(h, 13u));
        pos += sway_axis(h, local_up) * (fp.sway_m * std::sin(sphase));
        d = glm::length(pos - eye);
    }
    // Radial boundary fade about the eye (-> 0 at the box edge) so the re-binning
    // as the box follows the eye is C0, and the fall reset (which happens across
    // the whole box) is doubly hidden. Beyond box_half => 0 (culled).
    const double edge = 1.0 - smoothstep(fp.box_half * 0.72, fp.box_half, d);
    // AS-3 P1-4 INNER FADE: 0 inside inner_fade_m, unchanged from 2x it out,
    // C0 across the ramp (one inner radius wide). The far veil is a layer of
    // big soft flakes at distance; without this hole they arrive in the pilot's
    // face at 0.45 m each, which is the exact note Chad already gave about
    // 0.28 m. 0 => OFF, and the near lattice ships it OFF.
    const double inner =
        fp.inner_fade_m > 0.0
            ? smoothstep(fp.inner_fade_m, 2.0 * fp.inner_fade_m, d)
            : 1.0;
    // Wrap fade: fade the flake in/out as its fall phase crosses the 0/1 wrap, so
    // the one-cell teleport is invisible (C0). wrap_fade=0 disables it.
    double wrap = 1.0;
    if (fp.wrap_fade > 0.0)
        wrap = smoothstep(0.0, fp.wrap_fade, f) *
               (1.0 - smoothstep(1.0 - fp.wrap_fade, 1.0, f));
    double alpha = edge * wrap * inner * cull;
    // AS-3 per-flake VARIETY: size x [1-v, 1+v], alpha x [1-v, 1]. Hashed from
    // the cell, so a flake keeps its size/opacity for its whole life and two eyes
    // agree — the variety is a property of the world, not of the frame.
    double size_mul = 1.0;
    if (fp.size_var > 0.0)
        size_mul = 1.0 + fp.size_var * (2.0 * hash01(h, 21u) - 1.0);
    if (fp.alpha_var > 0.0) alpha *= 1.0 - fp.alpha_var * hash01(h, 22u);
    return {pos, alpha, size_mul};
}

PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double cell_size,
                           double box_half, double wrap_fade,
                           double fall_phase) {
    PrecipFieldParams fp;
    fp.cell_size = cell_size;
    fp.box_half = box_half;
    fp.wrap_fade = wrap_fade;
    return precip_sample(cell, eye, local_up, fall_phase, fp);
}

double precip_rock_alpha(double sd, double alt, double band) {
    // A flake dies only where BOTH are true: inside the net, and under the
    // terrain. max() of the two "keep" terms, so either one saves it.
    if (!(band > 0.0)) return (sd < 0.0 && alt < 0.0) ? 0.0 : 1.0;
    return std::max(smoothstep(-band, band, sd), smoothstep(-band, band, alt));
}

PrecipRockMode precip_rock_mode(double d_eye, double eye_alt, double box_half,
                                double band) {
    const double reach = box_half + std::max(0.0, band);
    // Clear of the net entirely, OR the whole box is above the terrain: nothing
    // in it can be buried, so no per-flake test at all. The second clause is
    // what keeps ordinary flight cheap AND keeps the snow alive at sled height
    // over the shallow arena (see precip_rock_alpha's note).
    if (d_eye > reach || eye_alt > reach) return PrecipRockMode::AllOutside;
    if (d_eye < -reach && eye_alt < -reach) return PrecipRockMode::AllInside;
    return PrecipRockMode::MouthBand;
}

}  // namespace render
