// SEADS remote interpolation buffer (netcode layer 4a). Mirrors tools/interp_ref.py BIT-FOR-BIT.
//
// Client-side, downstream-only: a renderer shows REMOTE aircraft ~100 ms in the past so it always
// has two received GEO-001 snapshots to interpolate between (smooth motion despite 20 Hz updates
// / jitter / loss). It consumes DECODED snapshots (netsnap::Snapshot) and produces interpolated
// EntityStates for display. It NEVER feeds the sim — net code stays strictly OUTSIDE the kernel.
//
// No reseal: interpolation uses only lat/lon/bearing/alt — all already on the GEO-001 wire. (Full
// client-side PREDICTION of your own aircraft needs phi/tas, which are NOT on the wire → a rail
// reseal, deferred to layer 4b.)
//
// Determinism: pure IEEE +,-,*,/ and comparisons (no transcendentals), so it stays in seads_net
// (which links NEITHER det_math nor the kernel). Under the strict-FP / no-FMA flags those ops are
// individually rounded and reproduce bit-for-bit cross-toolchain/cross-arch — proven by the
// generated-vector parity gate, exactly like the geo001/snapshot codecs.
//
// Choices (see interp_ref.py for the full rationale): LINEAR interp of lat/alt (not slerp — slerp
// needs transcendentals; chord error is negligible over ~100 ms and this is lossy display);
// SHORTEST-ARC interp of lon (wraps at ±180°, the antimeridian) and bearing (wraps at 360°);
// CLAMP/HOLD at the buffer edges (no extrapolation past the newest snapshot); entity set = the
// 'from' frame.
#pragma once
#include <cstdint>
#include <vector>

#include "snapshot.h"

namespace seads {
namespace interp {

// Exact op order shared with interp_ref.py.
double lerp(double a, double b, double alpha);            // a + (b - a) * alpha (no wrap)
double lerp_lon_deg(double a, double b, double alpha);    // shortest-arc, output (-180,180]
double lerp_angle_deg(double a, double b, double alpha);  // shortest-arc, output [0,360)

// Interpolate one entity (same id) between two wire EntityStates at fraction alpha.
netsnap::EntityState interp_entity(const netsnap::EntityState& from,
                                   const netsnap::EntityState& to, double alpha);

// Time-ordered buffer of decoded snapshots; samples interpolated entities at a render tick.
// Frames are kept sorted by server_tick ascending. add() expects non-decreasing server_tick
// (normal arrival); a duplicate tick replaces the prior frame. render_tick is in 100 Hz tick
// units (same base as server_tick) and may be fractional; the caller picks the ~100 ms delay.
class SnapshotBuffer {
public:
    void add(const netsnap::Snapshot& snap);
    void prune_before(int64_t tick);
    std::vector<netsnap::EntityState> sample(double render_tick) const;

    bool empty() const { return frames_.empty(); }
    const std::vector<netsnap::Snapshot>& frames() const { return frames_; }

private:
    std::vector<netsnap::Snapshot> frames_;  // ascending server_tick
};

}  // namespace interp
}  // namespace seads
