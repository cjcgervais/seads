// SEADS remote interpolation buffer — bit-for-bit mirror of tools/interp_ref.py.
#include "interp.h"

namespace seads {
namespace interp {

double lerp(double a, double b, double alpha) {
    return a + (b - a) * alpha;  // exact op order shared with the reference
}

double lerp_angle_deg(double a, double b, double alpha) {
    double diff = b - a;
    if (diff > 180.0) diff = diff - 360.0;
    else if (diff <= -180.0) diff = diff + 360.0;
    double r = a + diff * alpha;
    // sequential (not else-if): a +360 correction on a tiny-negative r can round to exactly
    // 360.0, which the second test folds back to canonical [0,360).
    if (r < 0.0) r = r + 360.0;
    if (r >= 360.0) r = r - 360.0;
    return r;
}

double lerp_lon_deg(double a, double b, double alpha) {
    // longitude wraps at +/-180 (antimeridian): take the short way, output in (-180,180].
    double diff = b - a;
    if (diff > 180.0) diff = diff - 360.0;
    else if (diff <= -180.0) diff = diff + 360.0;
    double r = a + diff * alpha;
    if (r > 180.0) r = r - 360.0;
    if (r <= -180.0) r = r + 360.0;
    return r;
}

netsnap::EntityState interp_entity(const netsnap::EntityState& from,
                                   const netsnap::EntityState& to, double alpha) {
    netsnap::EntityState e;
    e.id = from.id;
    e.lat_deg = lerp(from.lat_deg, to.lat_deg, alpha);              // lat does not wrap
    e.lon_deg = lerp_lon_deg(from.lon_deg, to.lon_deg, alpha);      // lon wraps at +/-180
    e.bearing_deg = lerp_angle_deg(from.bearing_deg, to.bearing_deg, alpha);
    e.alt_m = lerp(from.alt_m, to.alt_m, alpha);
    return e;
}

void SnapshotBuffer::add(const netsnap::Snapshot& snap) {
    const int64_t t = snap.server_tick;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].server_tick == t) { frames_[i] = snap; return; }
        if (frames_[i].server_tick > t) {
            frames_.insert(frames_.begin() + static_cast<std::ptrdiff_t>(i), snap);
            return;
        }
    }
    frames_.push_back(snap);
}

void SnapshotBuffer::prune_before(int64_t tick) {
    std::vector<netsnap::Snapshot> keep;
    const netsnap::Snapshot* lower = nullptr;  // newest frame strictly older than `tick`
    for (const auto& f : frames_) {
        if (f.server_tick < tick) lower = &f;
    }
    if (lower) keep.push_back(*lower);
    for (const auto& f : frames_) {
        if (f.server_tick >= tick) keep.push_back(f);
    }
    frames_ = std::move(keep);
}

std::vector<netsnap::EntityState> SnapshotBuffer::sample(double render_tick) const {
    std::vector<netsnap::EntityState> out;
    if (frames_.empty()) return out;

    if (render_tick <= static_cast<double>(frames_.front().server_tick)) {
        for (const auto& e : frames_.front().entities) out.push_back(interp_entity(e, e, 0.0));
        return out;
    }
    if (render_tick >= static_cast<double>(frames_.back().server_tick)) {
        for (const auto& e : frames_.back().entities) out.push_back(interp_entity(e, e, 0.0));
        return out;
    }

    std::size_t i = 0;
    while (i + 1 < frames_.size() &&
           static_cast<double>(frames_[i + 1].server_tick) <= render_tick) {
        ++i;
    }
    const netsnap::Snapshot& f0 = frames_[i];
    const netsnap::Snapshot& f1 = frames_[i + 1];
    const double t0 = static_cast<double>(f0.server_tick);
    const double t1 = static_cast<double>(f1.server_tick);
    const double alpha = (render_tick - t0) / (t1 - t0);

    for (const auto& e : f0.entities) {
        const netsnap::EntityState* g = nullptr;
        for (const auto& c : f1.entities) {
            if (c.id == e.id) { g = &c; break; }
        }
        out.push_back(g ? interp_entity(e, *g, alpha) : interp_entity(e, e, 0.0));
    }
    return out;
}

}  // namespace interp
}  // namespace seads
