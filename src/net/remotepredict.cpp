// SEADS REMOTE-AIRCRAFT PREDICTION (netcode layer 24) — bit-for-bit mirror of remotepredict_ref.py.
#include "remotepredict.h"

#include "snapshot.h"           // seads::netsnap (decode protocol-7 frames + DEG2RAD)
#include "interp.h"             // seads::interp::SnapshotBuffer (layer 4a baseline)
#include "../replay/sha256.h"   // seads::sha256_hex

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace seads {
namespace netremote {

namespace {

using predict::OwnState;

// A fresh single-aircraft kernel seeded at `s` (the reseed base a coast extrapolates from).
Kernel kernel_at(const Rails& rails, const OwnState& s) {
    Kernel k(rails);
    k.add(s.lat, s.lon, s.psi, s.phi, s.alt, s.tas, s.gamma);
    return k;
}

// Dead-reckon `base` forward `steps` kinematic ticks with the SEALED no-arg Kernel::step() (holds
// bank/gamma/speed; coordinated-turn great-circle). Returns the extrapolated kernel by value.
Kernel coast_to_now(const Rails& rails, const OwnState& base, unsigned steps) {
    Kernel k = kernel_at(rails, base);
    for (unsigned i = 0; i < steps; ++i) k.step();   // no-arg: the pure kinematic tail
    return k;
}

// Decode a wire frame and return the remote's lossy 7-tuple; ok=false if the remote is absent.
bool decode_remote7(const std::vector<std::uint8_t>& wire, std::int64_t remote_id, OwnState& out) {
    netsnap::Snapshot dec;
    std::size_t pos = 0;
    if (!netsnap::decode_snapshot(wire.data(), wire.size(), pos, dec)) return false;
    for (const auto& e : dec.entities) {
        if (e.id == remote_id) {
            out = OwnState{e.lat_deg * netsnap::DEG2RAD, e.lon_deg * netsnap::DEG2RAD,
                           e.bearing_deg * netsnap::DEG2RAD, e.phi_deg * netsnap::DEG2RAD,
                           e.alt_m, e.tas_mps, e.gamma_deg * netsnap::DEG2RAD};
            return true;
        }
    }
    return false;
}

// Frame table keyed by emit_tick (server_tick), pointing at the wire bytes.
std::unordered_map<std::int64_t, const std::vector<std::uint8_t>*> frame_map(
    const session::ServerFrames& frames) {
    std::unordered_map<std::int64_t, const std::vector<std::uint8_t>*> m;
    m.reserve(frames.size());
    for (const auto& f : frames) m[f.first] = &f.second;
    return m;
}

}  // namespace

RemoteResult run_remote_client(const Rails& rails, const std::vector<OwnState>& states,
                               const session::ServerFrames& frames, unsigned lag,
                               const std::vector<std::int64_t>& drop_emit_ticks, bool reconcile,
                               Source src, std::int64_t remote_id) {
    const unsigned ticks = states.empty() ? 0u : static_cast<unsigned>(states.size() - 1);
    const auto fmap = frame_map(frames);
    std::unordered_set<std::int64_t> drops(drop_emit_ticks.begin(), drop_emit_ticks.end());

    RemoteResult res;
    Kernel coaster = kernel_at(rails, states[0]);   // spawn state is known (learned on join/BIND)
    for (unsigned t = 1; t <= ticks; ++t) {
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(lag);
        bool reseeded = false;
        if (reconcile && st >= 0 && drops.find(st) == drops.end()) {
            auto it = fmap.find(st);
            if (it != fmap.end()) {
                OwnState base;
                bool have = false;
                if (src == Source::CANONICAL) {
                    base = states[static_cast<std::size_t>(st)];
                    have = true;
                } else {
                    have = decode_remote7(*it->second, remote_id, base);
                }
                if (have) {
                    coaster = coast_to_now(rails, base, lag);   // extrapolate st -> now = t
                    ++res.delivered;
                    reseeded = true;
                }
            }
        }
        if (!reseeded) coaster.step();                          // advance the running estimate one tick

        res.per_tick.push_back(predict::tick_hash(coaster, t));
        const OwnState& tru = states[t];
        res.max_pos_err = std::max({res.max_pos_err,
                                    std::abs(coaster.lat(0) - tru.lat),
                                    std::abs(coaster.lon(0) - tru.lon)});
    }

    std::vector<std::uint8_t> cat;
    for (const auto& hh : res.per_tick) cat.insert(cat.end(), hh.begin(), hh.end());
    res.digest = sha256_hex(cat);
    return res;
}

double coast_now_error(const Rails& rails, const std::vector<OwnState>& states,
                       const session::ServerFrames& frames, unsigned lag, unsigned lo, unsigned hi,
                       Source src, std::int64_t remote_id) {
    const unsigned ticks = states.empty() ? 0u : static_cast<unsigned>(states.size() - 1);
    const auto fmap = frame_map(frames);
    Kernel coaster = kernel_at(rails, states[0]);
    double worst = 0.0;
    for (unsigned t = 1; t <= ticks; ++t) {
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(lag);
        bool reseeded = false;
        if (st >= 0) {
            auto it = fmap.find(st);
            if (it != fmap.end()) {
                OwnState base;
                bool have = false;
                if (src == Source::CANONICAL) { base = states[static_cast<std::size_t>(st)]; have = true; }
                else { have = decode_remote7(*it->second, remote_id, base); }
                if (have) { coaster = coast_to_now(rails, base, lag); reseeded = true; }
            }
        }
        if (!reseeded) coaster.step();
        const OwnState& tru = states[t];
        if (t >= lo && t <= hi)
            worst = std::max({worst, std::abs(coaster.lat(0) - tru.lat),
                              std::abs(coaster.lon(0) - tru.lon)});
    }
    return worst;
}

double interp_now_error(const std::vector<OwnState>& states, const session::ServerFrames& frames,
                        unsigned lag, unsigned render_delay, unsigned lo, unsigned hi,
                        const std::vector<std::int64_t>& drop_emit_ticks, std::int64_t remote_id) {
    const unsigned ticks = states.empty() ? 0u : static_cast<unsigned>(states.size() - 1);
    const auto fmap = frame_map(frames);
    std::unordered_set<std::int64_t> drops(drop_emit_ticks.begin(), drop_emit_ticks.end());
    interp::SnapshotBuffer buf;
    double worst = 0.0;
    for (unsigned t = 1; t <= ticks; ++t) {
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(lag);
        if (st >= 0 && drops.find(st) == drops.end()) {
            auto it = fmap.find(st);
            if (it != fmap.end()) {
                netsnap::Snapshot dec;
                std::size_t pos = 0;
                if (netsnap::decode_snapshot(it->second->data(), it->second->size(), pos, dec))
                    buf.add(dec);
            }
        }
        std::vector<netsnap::EntityState> ents =
            buf.sample(static_cast<double>(t) - static_cast<double>(render_delay));
        const OwnState& tru = states[t];
        if (t >= lo && t <= hi) {
            for (const auto& e : ents) {
                if (e.id == remote_id) {
                    double lat_r = e.lat_deg * netsnap::DEG2RAD;
                    double lon_r = e.lon_deg * netsnap::DEG2RAD;
                    worst = std::max({worst, std::abs(lat_r - tru.lat), std::abs(lon_r - tru.lon)});
                }
            }
        }
    }
    return worst;
}

}  // namespace netremote
}  // namespace seads
