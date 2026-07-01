// SEADS server<->client SESSION loop (netcode layer 5). Mirrors tools/session_ref.py BIT-FOR-BIT.
#include "session.h"

#include "geo001.h"
#include "../replay/sha256.h"

#include <algorithm>
#include <cstddef>

namespace seads {
namespace session {

namespace {

// Integer phase select: largest start_tick <= t (mirrors ref_kernel.run_scenario / session_ref).
const Phase& phase_at(const AircraftSpec& a, unsigned t) {
    unsigned idx = 0;
    for (unsigned j = 0; j < a.n_phase; ++j) {
        if (a.sched[j].start_tick <= t) idx = j; else break;
    }
    return a.sched[idx];
}

Command server_command_at(const AircraftSpec& a, unsigned t) {
    const Phase& p = phase_at(a, t);
    return Command{p.target_phi, p.target_g, p.throttle, p.fire};
}

// The OWN ship's KINEMATIC command (no fire bit): firing never touches kinematics, and the own
// ship's hp/rounds are wire-sourced, so the predictor predicts pure motion (see session_ref.py).
Command own_kinematic_command_at(const AircraftSpec& a, unsigned t) {
    const Phase& p = phase_at(a, t);
    return Command{p.target_phi, p.target_g, p.throttle, false};
}

// Serialize the kernel's FULL world to a protocol-5 wire frame (every aircraft: GEO + KIN-002 +
// WEAPON hp/fire_cd/ammo; every live round: GEO + damage + ttl/owner). Aircraft/projectile id = SoA
// index (rounds are transient — a per-frame index is all the client needs to draw + count them).
std::vector<std::uint8_t> serialize_world(const Kernel& k, std::int64_t server_tick) {
    netsnap::Snapshot s;
    s.server_tick = server_tick;
    for (std::size_t i = 0; i < k.count(); ++i) {
        s.entities.push_back(netsnap::from_kernel(
            static_cast<std::int64_t>(i), k.lat(i), k.lon(i), k.psi(i), k.alt(i),
            k.phi(i), k.tas(i), k.gamma(i), k.hp(i), k.fire_cd(i), k.ammo(i)));
    }
    for (std::size_t j = 0; j < k.proj_count(); ++j) {
        s.projectiles.push_back(netsnap::proj_from_kernel(
            static_cast<std::int64_t>(j), k.proj_lat(j), k.proj_lon(j), k.proj_psi(j),
            k.proj_alt(j), k.proj_damage(j),
            static_cast<std::int64_t>(k.proj_ttl(j)), static_cast<std::int64_t>(k.proj_owner(j))));
    }
    std::vector<std::uint8_t> out;
    netsnap::encode_snapshot(s, out);
    return out;
}

void enc_geo(std::vector<std::uint8_t>& out, const netsnap::EntityState& e) {
    geo001::GeoPoint p{e.lat_deg, e.lon_deg, e.bearing_deg, e.alt_m};
    geo001::encode_point(p, out);
}
void enc_geo_proj(std::vector<std::uint8_t>& out, const netsnap::ProjectileState& p) {
    geo001::GeoPoint g{p.lat_deg, p.lon_deg, p.bearing_deg, p.alt_m};
    geo001::encode_point(g, out);
}

// Serialize one reconstructed client view to canonical bytes (byte-order MUST match
// session_ref.encode_client_view): header, OWN predicted geometry @ now, REMOTES interpolated @
// render_tick, WEAPONS + ROUNDS from the freshest delivered frame (nullptr => empty sections).
std::vector<std::uint8_t> encode_client_view(std::int64_t client_tick, std::int64_t render_tick,
                                             const netsnap::EntityState& own,
                                             std::vector<netsnap::EntityState> remotes,
                                             const netsnap::Snapshot* wframe) {
    std::vector<std::uint8_t> out;
    geo001::encode_i64(client_tick, out);
    geo001::encode_i64(render_tick, out);
    // OWN — predicted @ now (full kinematic state)
    enc_geo(out, own);
    geo001::encode_i64(geo001::quantize(own.phi_deg, netsnap::PHI_SCALE), out);
    geo001::encode_i64(geo001::quantize(own.tas_mps, netsnap::SPEED_SCALE), out);
    geo001::encode_i64(geo001::quantize(own.gamma_deg, netsnap::GAMMA_SCALE), out);
    // REMOTES — interpolated @ render_tick (geometry only), ascending id
    std::sort(remotes.begin(), remotes.end(),
              [](const netsnap::EntityState& a, const netsnap::EntityState& b) { return a.id < b.id; });
    geo001::encode_i64(static_cast<std::int64_t>(remotes.size()), out);
    for (const auto& e : remotes) {
        geo001::encode_i64(e.id, out);
        enc_geo(out, e);
    }
    // WEAPONS + ROUNDS — from the freshest delivered frame (nearest-frame; nullptr => empty)
    if (wframe == nullptr) {
        geo001::encode_i64(0, out);  // no weapon data yet
        geo001::encode_i64(0, out);  // no rounds
    } else {
        std::vector<netsnap::EntityState> wents = wframe->entities;
        std::sort(wents.begin(), wents.end(),
                  [](const netsnap::EntityState& a, const netsnap::EntityState& b) { return a.id < b.id; });
        geo001::encode_i64(static_cast<std::int64_t>(wents.size()), out);
        for (const auto& e : wents) {
            geo001::encode_i64(e.id, out);
            geo001::encode_i64(geo001::quantize(e.hp, netsnap::HP_SCALE), out);
            geo001::encode_i64(e.hp <= 0.0 ? 1 : 0, out);  // dead flag (kill replicated)
            geo001::encode_i64(geo001::quantize(e.fire_cd, netsnap::FIRECD_SCALE), out);
            geo001::encode_i64(geo001::quantize(e.ammo, netsnap::AMMO_SCALE), out);  // rounds remaining (v1.14r0)
        }
        geo001::encode_i64(static_cast<std::int64_t>(wframe->projectiles.size()), out);
        for (const auto& p : wframe->projectiles) {
            geo001::encode_i64(p.id, out);
            enc_geo_proj(out, p);
            geo001::encode_i64(geo001::quantize(p.damage, netsnap::DAMAGE_SCALE), out);
            geo001::encode_i64(p.ttl, out);
            geo001::encode_i64(p.owner, out);
        }
    }
    return out;
}

// The delivered frame with the largest server_tick <= render_tick (freshest weapon truth), or
// nullptr if none has arrived yet. buf.frames() are ascending server_tick.
const netsnap::Snapshot* freshest_frame(const interp::SnapshotBuffer& buf, std::int64_t render_tick) {
    const netsnap::Snapshot* chosen = nullptr;
    for (const auto& f : buf.frames()) {
        if (f.server_tick <= render_tick) chosen = &f; else break;
    }
    return chosen;
}

}  // namespace

SessionResult run_session(const Rails& rails, const Scenario& sc, bool reconcile) {
    const unsigned ticks = sc.ticks;

    // --- server: drive the authoritative kernel; emit protocol-5 frames at 20 Hz -------------
    Kernel server(rails);
    for (unsigned i = 0; i < sc.n_aircraft; ++i) {
        const AircraftSpec& a = sc.aircraft[i];
        server.add(a.lat, a.lon, a.psi, a.phi, a.alt, a.tas, 0.0, a.env->hp_start, a.env->ammo_start);
    }
    // frames as (emit_tick, bytes) ascending — emits are snap_every apart, so a small vector.
    std::vector<std::pair<std::int64_t, std::vector<std::uint8_t>>> frames;
    frames.emplace_back(0, serialize_world(server, 0));  // initial world (pre-step)
    for (unsigned t = 1; t <= ticks; ++t) {
        std::vector<Command> cmds;
        std::vector<const Envelope*> envs;
        cmds.reserve(sc.n_aircraft);
        envs.reserve(sc.n_aircraft);
        for (unsigned i = 0; i < sc.n_aircraft; ++i) {
            cmds.push_back(server_command_at(sc.aircraft[i], t - 1));
            envs.push_back(sc.aircraft[i].env);
        }
        server.step(cmds, envs);
        if (t % sc.snap_every == 0) frames.emplace_back(t, serialize_world(server, t));
    }

    auto is_dropped = [&](std::int64_t emit_tick) {
        for (unsigned d = 0; d < sc.n_drops; ++d)
            if (sc.drop_emit_ticks[d] == emit_tick) return true;
        return false;
    };
    auto frame_at = [&](std::int64_t emit_tick) -> const std::vector<std::uint8_t>* {
        for (const auto& f : frames)
            if (f.first == emit_tick) return &f.second;
        return nullptr;
    };

    // --- client: predict own @ now, interpolate remotes, reconstruct HP/kills/rounds from wire -
    const AircraftSpec& own = sc.aircraft[OWN_ID];
    predict::OwnState start{own.lat, own.lon, own.psi, own.phi, own.alt, own.tas, 0.0};
    predict::Predictor predictor(rails, own.env, start);
    interp::SnapshotBuffer buf;

    SessionResult res;
    res.n_frames = static_cast<unsigned>(frames.size());
    for (unsigned t = 1; t <= ticks; ++t) {
        // 1) predict own ship this tick (kinematics only)
        predictor.predict(t, own_kinematic_command_at(own, t - 1));
        // 2) ingest a delivered frame: interp buffer + own reconcile (lossy dequantized reseed)
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(sc.lag_ticks);
        if (st >= 0 && !is_dropped(st)) {
            if (const std::vector<std::uint8_t>* wire = frame_at(st)) {
                netsnap::Snapshot dec;
                std::size_t pos = 0;
                if (netsnap::decode_snapshot(wire->data(), wire->size(), pos, dec)) {
                    buf.add(dec);
                    ++res.delivered;
                    if (reconcile) {
                        for (const auto& e : dec.entities) {
                            if (e.id == OWN_ID) {
                                predict::OwnState auth{
                                    e.lat_deg * netsnap::DEG2RAD, e.lon_deg * netsnap::DEG2RAD,
                                    e.bearing_deg * netsnap::DEG2RAD, e.phi_deg * netsnap::DEG2RAD,
                                    e.alt_m, e.tas_mps, e.gamma_deg * netsnap::DEG2RAD};
                                predictor.reconcile(static_cast<std::uint32_t>(st), auth);
                                break;
                            }
                        }
                    }
                }
            }
        }
        // 3) reconstruct the client view
        std::int64_t render_tick =
            static_cast<std::int64_t>(t) - static_cast<std::int64_t>(sc.render_delay);
        const Kernel& kk = predictor.kernel();
        netsnap::EntityState own_ent = netsnap::from_kernel(
            OWN_ID, kk.lat(0), kk.lon(0), kk.psi(0), kk.alt(0),
            kk.phi(0), kk.tas(0), kk.gamma(0), kk.hp(0), kk.fire_cd(0));
        std::vector<netsnap::EntityState> remotes;
        for (const auto& e : buf.sample(static_cast<double>(render_tick)))
            if (e.id != OWN_ID) remotes.push_back(e);
        const netsnap::Snapshot* wframe = freshest_frame(buf, render_tick);
        if (wframe != nullptr) {
            res.has_final_wframe = true;
            res.final_wframe = *wframe;
        }
        std::vector<std::uint8_t> view =
            encode_client_view(static_cast<std::int64_t>(t), render_tick, own_ent, remotes, wframe);
        res.per_tick.push_back(sha256_hex(view));
    }

    // digest over the concatenated ASCII-hex per-tick view hashes (matches session_ref)
    std::vector<std::uint8_t> cat;
    for (const auto& h : res.per_tick) cat.insert(cat.end(), h.begin(), h.end());
    res.digest = sha256_hex(cat);
    return res;
}

}  // namespace session
}  // namespace seads
