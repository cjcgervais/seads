// SEADS recording playback — see playback.h.
#include "playback.h"

namespace seads {
namespace client {

bool Playback::load(const Recording& rec) {
    radius_m_ = rec.meta.radius_m;
    tick_hz_ = static_cast<int>(rec.meta.tick_hz ? rec.meta.tick_hz : 100);
    snap_hz_ = static_cast<int>(rec.meta.snap_hz ? rec.meta.snap_hz : 20);
    if (rec.frames.empty()) return false;
    for (const auto& f : rec.frames) buffer_.add(f);
    first_tick_ = rec.frames.front().server_tick;
    last_tick_ = rec.frames.back().server_tick;
    // ~1.5 snapshot intervals of delay so two received frames always bracket the render time.
    double snap_interval_ticks = static_cast<double>(tick_hz_) / static_cast<double>(snap_hz_);
    delay_ticks_ = 1.5 * snap_interval_ticks;
    return true;
}

std::vector<RenderEntity> Playback::sample(double render_tick) const {
    std::vector<RenderEntity> out;
    std::vector<netsnap::EntityState> states = buffer_.sample(render_tick);
    out.reserve(states.size());
    for (const auto& s : states) {
        RenderEntity e;
        e.id = s.id;
        e.lat_deg = s.lat_deg;
        e.lon_deg = s.lon_deg;
        e.alt_m = s.alt_m;
        e.bearing_deg = s.bearing_deg;
        e.phi_deg = s.phi_deg;
        e.tas_mps = s.tas_mps;
        e.pos = geo_deg_to_cartesian(s.lat_deg, s.lon_deg, s.alt_m, radius_m_);
        out.push_back(e);
    }
    return out;
}

}  // namespace client
}  // namespace seads
