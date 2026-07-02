// SEADS recording playback — see playback.h.
#include "playback.h"

#include <cmath>

namespace seads {
namespace client {

bool Playback::load(const Recording& rec) {
    radius_m_ = rec.meta.radius_m;
    tick_hz_ = static_cast<int>(rec.meta.tick_hz ? rec.meta.tick_hz : 100);
    snap_hz_ = static_cast<int>(rec.meta.snap_hz ? rec.meta.snap_hz : 20);
    if (rec.frames.empty()) return false;
    for (const auto& f : rec.frames) buffer_.add(f);
    events_ = rec.events;  // layer-6 journal (empty for a v1 recording)
    types_ = rec.types;    // airframe type codes (empty pre-v3 -> generic mesh fallback)
    first_tick_ = rec.frames.front().server_tick;
    last_tick_ = rec.frames.back().server_tick;
    // ~1.5 snapshot intervals of delay so two received frames always bracket the render time.
    double snap_interval_ticks = static_cast<double>(tick_hz_) / static_cast<double>(snap_hz_);
    delay_ticks_ = 1.5 * snap_interval_ticks;
    return true;
}

const netsnap::Snapshot* Playback::nearest_frame(double render_tick) const {
    const std::vector<netsnap::Snapshot>& frames = buffer_.frames();
    if (frames.empty()) return nullptr;
    // Frames are ascending server_tick; walk to the latest at/before render_tick. If render_tick
    // precedes the whole buffer, hold the first frame (matches interp edge semantics).
    const netsnap::Snapshot* fr = &frames.front();
    for (const auto& f : frames) {
        if (static_cast<double>(f.server_tick) <= render_tick) fr = &f; else break;
    }
    return fr;
}

std::vector<RenderEntity> Playback::sample(double render_tick) const {
    std::vector<RenderEntity> out;
    std::vector<netsnap::EntityState> states = buffer_.sample(render_tick);
    // Position + heading interpolate smoothly (layer 4a); bank/speed/gamma are NON-geographic and
    // are NOT carried by the interp, so snap them from the nearest received frame by id. Attitude
    // updates at the snapshot cadence (20 Hz) while position stays smooth — imperceptible for a
    // roll/pitch, and consistent with how hp/rounds are sampled.
    const netsnap::Snapshot* nf = nearest_frame(render_tick);  // not 'near' — MSVC legacy macro
    out.reserve(states.size());
    for (const auto& s : states) {
        RenderEntity e;
        e.id = s.id;
        e.lat_deg = s.lat_deg;
        e.lon_deg = s.lon_deg;
        e.alt_m = s.alt_m;
        e.bearing_deg = s.bearing_deg;
        if (nf) {
            for (const auto& w : nf->entities) {
                if (w.id == s.id) {
                    e.phi_deg = w.phi_deg;
                    e.tas_mps = w.tas_mps;
                    e.gamma_deg = w.gamma_deg;
                    break;
                }
            }
        }
        e.pos = geo_deg_to_cartesian(s.lat_deg, s.lon_deg, s.alt_m, radius_m_);
        out.push_back(e);
    }
    return out;
}

WeaponView Playback::sample_weapons(double render_tick) const {
    WeaponView wv;
    const netsnap::Snapshot* fr = nearest_frame(render_tick);
    if (!fr) return wv;
    wv.hp.reserve(fr->entities.size());
    for (const auto& e : fr->entities) {
        RenderHp h;
        h.id = e.id;
        h.hp = e.hp;
        h.ammo = e.ammo;
        // last_hit_by / kills are integer-valued on the wire (unit scale); round, don't truncate.
        h.last_hit_by = static_cast<int64_t>(std::llround(e.last_hit_by));
        h.engine_hp = e.engine_hp;
        h.wing_hp = e.wing_hp;
        h.tail_hp = e.tail_hp;
        h.kills = static_cast<int64_t>(std::llround(e.kills));
        wv.hp.push_back(h);
    }
    wv.rounds.reserve(fr->projectiles.size());
    for (const auto& p : fr->projectiles) {
        RenderRound r;
        r.lat_deg = p.lat_deg; r.lon_deg = p.lon_deg; r.alt_m = p.alt_m; r.owner = p.owner;
        r.pos = geo_deg_to_cartesian(p.lat_deg, p.lon_deg, p.alt_m, radius_m_);
        wv.rounds.push_back(r);
    }
    return wv;
}

}  // namespace client
}  // namespace seads
