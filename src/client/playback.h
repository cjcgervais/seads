// SEADS recording playback (Step 5 client). Wraps a decoded .seadsrec stream in the layer-4a
// interp::SnapshotBuffer and samples it at a render time chosen ~100 ms in the past, exactly as
// a live client would consume 20 Hz snapshots. DOWNSTREAM-ONLY: it never advances a kernel,
// never hashes, never writes the sim. The wall-clock -> render-tick mapping lives in the caller
// (the viewer); this lib is pure given a render tick, so it is fully unit-testable.
#pragma once
#include <cstdint>
#include <vector>

#include "interp.h"     // seads::interp::SnapshotBuffer (layer 4a)
#include "seadsrec.h"   // seads::client::Recording
#include "globe.h"      // seads::client::Vec3, geo_deg_to_cartesian

namespace seads {
namespace client {

// One interpolated aircraft ready to draw: world position on the globe plus the raw wire fields
// the HUD shows. Degrees/metres (wire units) are kept alongside the cartesian position.
struct RenderEntity {
    int64_t id = 0;
    Vec3 pos;             // world-space position (metres), globe frame
    double lat_deg = 0, lon_deg = 0, alt_m = 0;
    double bearing_deg = 0, phi_deg = 0, tas_mps = 0;
};

class Playback {
public:
    // Load decoded frames into the interpolation buffer. Returns false if the recording is empty.
    bool load(const Recording& rec);

    double radius_m() const { return radius_m_; }
    int tick_hz() const { return tick_hz_; }
    int snap_hz() const { return snap_hz_; }

    // Server-tick span of the recording (100 Hz tick units).
    int64_t first_tick() const { return first_tick_; }
    int64_t last_tick() const { return last_tick_; }

    // The recommended interpolation delay in ticks: ~one-and-a-half snapshot intervals so two
    // frames always bracket the render time despite 20 Hz cadence + jitter. (e.g. 100 ms @ 20 Hz.)
    double delay_ticks() const { return delay_ticks_; }

    // Sample the buffer at an absolute render tick (may be fractional). Returns interpolated
    // aircraft mapped to world space. Clamps/holds at the recording's edges (interp semantics).
    std::vector<RenderEntity> sample(double render_tick) const;

private:
    interp::SnapshotBuffer buffer_;
    double radius_m_ = 15000.0;
    int tick_hz_ = 100;
    int snap_hz_ = 20;
    int64_t first_tick_ = 0;
    int64_t last_tick_ = 0;
    double delay_ticks_ = 0.0;
};

}  // namespace client
}  // namespace seads
