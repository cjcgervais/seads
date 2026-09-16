#include "render/precip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace render {

namespace {

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
// A hashed scalar in [0,1) from the cell + a salt (distinct salts => independent
// streams for jitter.x/y/z and the phase offset, from one hash).
double hash01(int i, int j, int k, std::uint32_t salt) {
    std::uint32_t x = hash_cell(i, j, k) ^ (salt * 0x9E3779B9u);
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (x >> 8) * (1.0 / 16777216.0);
}

}  // namespace

glm::ivec3 precip_center_cell(const glm::dvec3& eye, double cell_size) {
    const double s = cell_size > 0.0 ? cell_size : 1.0;
    return glm::ivec3(static_cast<int>(std::lround(eye.x / s)),
                      static_cast<int>(std::lround(eye.y / s)),
                      static_cast<int>(std::lround(eye.z / s)));
}

PrecipSample precip_sample(const glm::ivec3& cell, const glm::dvec3& eye,
                           const glm::dvec3& local_up, double cell_size,
                           double box_half, double wrap_fade, double fall_phase) {
    // World-anchored jittered base: the integer cell + a per-cell hashed offset,
    // scaled to world metres. A pure fn of the WORLD cell => it does not move with
    // the eye (only the fall term uses local_up), so flying through the field
    // streams the flakes past the plane for free (no camera-frame snow-globe).
    const glm::dvec3 jit(hash01(cell.x, cell.y, cell.z, 1u),
                         hash01(cell.x, cell.y, cell.z, 2u),
                         hash01(cell.x, cell.y, cell.z, 3u));
    const glm::dvec3 base = (glm::dvec3(cell) + jit) * cell_size;
    // The fall: DOWN along local_up only (NO horizontal advection = Chad's no-wind
    // invariant). Each flake carries a hashed phase offset so their wraps are
    // staggered (not a synchronized blink). It falls exactly one cell_size then
    // wraps; the wrap fade below makes that reset C0.
    const double poff = hash01(cell.x, cell.y, cell.z, 7u);
    double f = fall_phase + poff;
    f -= std::floor(f);  // frac -> [0,1)
    const glm::dvec3 pos = base - local_up * (f * cell_size);
    // Radial boundary fade about the eye (-> 0 at the box edge) so the re-binning
    // as the box follows the eye is C0, and the fall reset (which happens across
    // the whole box) is doubly hidden. Beyond box_half => 0 (culled).
    const double d = glm::length(pos - eye);
    const double edge = 1.0 - smoothstep(box_half * 0.72, box_half, d);
    // Wrap fade: fade the flake in/out as its fall phase crosses the 0/1 wrap, so
    // the one-cell teleport is invisible (C0). wrap_fade=0 disables it.
    double wrap = 1.0;
    if (wrap_fade > 0.0)
        wrap = smoothstep(0.0, wrap_fade, f) *
               (1.0 - smoothstep(1.0 - wrap_fade, 1.0, f));
    return {pos, edge * wrap};
}

}  // namespace render
