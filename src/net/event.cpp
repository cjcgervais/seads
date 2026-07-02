// SEADS reliable combat-EVENT channel (netcode layer 6). Mirrors tools/event_ref.py BIT-FOR-BIT.
#include "event.h"

#include "geo001.h"           // geo001::{quantize, encode_i64, decode_i64}
#include "snapshot.h"         // netsnap::HP_SCALE (sealed 1e3 quantum)
#include "../replay/sha256.h"

#include <cstddef>

namespace seads {
namespace event {

namespace {

// Integer phase select: largest start_tick <= t (mirrors session_ref / ref_kernel.run_scenario).
const session::Phase& phase_at(const session::AircraftSpec& a, unsigned t) {
    unsigned idx = 0;
    for (unsigned j = 0; j < a.n_phase; ++j) {
        if (a.sched[j].start_tick <= t) idx = j; else break;
    }
    return a.sched[idx];
}

Command server_command_at(const session::AircraftSpec& a, unsigned t) {
    const session::Phase& p = phase_at(a, t);
    return Command{p.target_phi, p.target_g, p.throttle, p.fire};
}

void put_event(std::vector<std::uint8_t>& out, const Event& e) {
    geo001::encode_i64(e.seq, out);
    geo001::encode_i64(e.tick, out);
    geo001::encode_i64(e.target, out);
    geo001::encode_i64(e.damage_milli, out);
    geo001::encode_i64(e.hp_after_milli, out);
    geo001::encode_i64(e.killed, out);
    geo001::encode_i64(e.attacker, out);   // v1.17r0: attributed kill-feed
}

// Encode the last-K window of events with tick <= emit_tick (mirrors event_ref.window_at/encode_window).
std::vector<std::uint8_t> encode_window(const std::vector<Event>& events, std::int64_t emit_tick) {
    std::vector<const Event*> avail;
    for (const auto& e : events)
        if (e.tick <= emit_tick) avail.push_back(&e);
    std::size_t start = avail.size() > static_cast<std::size_t>(EVENT_WINDOW_K)
                            ? avail.size() - EVENT_WINDOW_K : 0;
    std::vector<std::uint8_t> out;
    geo001::encode_i64(static_cast<std::int64_t>(avail.size() - start), out);
    for (std::size_t i = start; i < avail.size(); ++i) put_event(out, *avail[i]);
    return out;
}

// Decode an event window (mirrors event_ref.decode_window). Returns events ascending by seq.
std::vector<Event> decode_window(const std::vector<std::uint8_t>& data) {
    std::vector<Event> win;
    std::size_t pos = 0;
    std::int64_t n = 0;
    if (!geo001::decode_i64(data.data(), data.size(), pos, n)) return win;
    for (std::int64_t i = 0; i < n; ++i) {
        Event e{};
        geo001::decode_i64(data.data(), data.size(), pos, e.seq);
        geo001::decode_i64(data.data(), data.size(), pos, e.tick);
        geo001::decode_i64(data.data(), data.size(), pos, e.target);
        geo001::decode_i64(data.data(), data.size(), pos, e.damage_milli);
        geo001::decode_i64(data.data(), data.size(), pos, e.hp_after_milli);
        geo001::decode_i64(data.data(), data.size(), pos, e.killed);
        geo001::decode_i64(data.data(), data.size(), pos, e.attacker);   // v1.17r0
        win.push_back(e);
    }
    return win;
}

}  // namespace

std::vector<std::uint8_t> encode_event_log(const std::vector<Event>& applied) {
    std::vector<std::uint8_t> out;
    geo001::encode_i64(static_cast<std::int64_t>(applied.size()), out);
    for (const auto& e : applied) put_event(out, e);
    return out;
}

EventResult run_events(const Rails& rails, const session::Scenario& sc,
                       const std::int64_t* drops, unsigned n_drops) {
    const unsigned ticks = sc.ticks;
    const std::int64_t* drop_set = drops != nullptr ? drops : sc.drop_emit_ticks;
    const unsigned n_drop = drops != nullptr ? n_drops : sc.n_drops;

    // --- server: drive the authoritative kernel; DERIVE events by observing hp deltas -----------
    Kernel server(rails);
    for (unsigned i = 0; i < sc.n_aircraft; ++i) {
        const session::AircraftSpec& a = sc.aircraft[i];
        server.add(a.lat, a.lon, a.psi, a.phi, a.alt, a.tas, 0.0, a.env->hp_start, a.env->ammo_start);
    }

    EventResult res;
    std::vector<Event>& events = res.events;

    // frames as (emit_tick, window_bytes) ascending — one per snap_every ticks, plus the tick-0 frame.
    std::vector<std::pair<std::int64_t, std::vector<std::uint8_t>>> windows;
    windows.emplace_back(0, encode_window(events, 0));   // tick-0 frame (empty window)

    for (unsigned t = 1; t <= ticks; ++t) {
        std::vector<double> hp_before(sc.n_aircraft);
        for (unsigned i = 0; i < sc.n_aircraft; ++i) hp_before[i] = server.hp(i);

        std::vector<Command> cmds;
        std::vector<const Envelope*> envs;
        cmds.reserve(sc.n_aircraft);
        envs.reserve(sc.n_aircraft);
        for (unsigned i = 0; i < sc.n_aircraft; ++i) {
            cmds.push_back(server_command_at(sc.aircraft[i], t - 1));
            envs.push_back(sc.aircraft[i].env);
        }
        server.step(cmds, envs);

        // OBSERVE hp deltas -> events (array order = ascending target index; deterministic)
        for (unsigned i = 0; i < sc.n_aircraft; ++i) {
            const double before = hp_before[i];
            const double after = server.hp(i);
            if (after < before) {
                std::int64_t d_milli = geo001::quantize(before, netsnap::HP_SCALE)
                                     - geo001::quantize(after, netsnap::HP_SCALE);
                std::int64_t killed = (before > 0.0 && after <= 0.0) ? 1 : 0;
                // v1.17r0: the kernel set last_hit_by(i) to the striking round's owner THIS tick ->
                // the attacker for this observed hp delta (an attributed hit/kill event). last_hit_by
                // is always an exact integer-valued double (an index, or -1), so the cast is exact.
                std::int64_t attacker = static_cast<std::int64_t>(server.last_hit_by(i));
                events.push_back(Event{static_cast<std::int64_t>(events.size()),
                                       static_cast<std::int64_t>(t),
                                       static_cast<std::int64_t>(i), d_milli,
                                       geo001::quantize(after, netsnap::HP_SCALE), killed, attacker});
            }
        }
        if (t % sc.snap_every == 0) windows.emplace_back(t, encode_window(events, t));
    }
    res.n_windows = static_cast<unsigned>(windows.size());

    // --- transport + client: ship windows (fixed lag + packet loss), apply the redundant journal --
    auto is_dropped = [&](std::int64_t emit_tick) {
        for (unsigned d = 0; d < n_drop; ++d)
            if (drop_set[d] == emit_tick) return true;
        return false;
    };
    auto window_at = [&](std::int64_t emit_tick) -> const std::vector<std::uint8_t>* {
        for (const auto& w : windows)
            if (w.first == emit_tick) return &w.second;
        return nullptr;
    };

    std::int64_t next_seq = 0;
    for (unsigned t = 1; t <= ticks; ++t) {
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(sc.lag_ticks);
        if (st < 0 || is_dropped(st)) continue;
        const std::vector<std::uint8_t>* wbytes = window_at(st);
        if (wbytes == nullptr) continue;
        ++res.delivered;
        for (const Event& e : decode_window(*wbytes)) {
            if (e.seq >= next_seq) {
                res.applied.push_back(e);
                next_seq = e.seq + 1;
            }
        }
    }

    res.digest = sha256_hex(encode_event_log(res.applied));
    return res;
}

}  // namespace event
}  // namespace seads
