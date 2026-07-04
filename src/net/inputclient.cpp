// SEADS predictive INPUT CLIENT (netcode layer 17) — bit-for-bit mirror of tools/inputpredict_ref.py.
#include "inputclient.h"

#include "snapshot.h"           // seads::netsnap (decode protocol-7 frames + DEG2RAD)
#include "../replay/sha256.h"   // seads::sha256_hex

#include <algorithm>
#include <cstddef>

namespace seads {
namespace netpredict {

namespace {

// Integer phase select: largest start_tick <= t (mirrors session::phase_at / ref_kernel.run_scenario).
const session::Phase& phase_at(const session::AircraftSpec& a, unsigned t) {
    unsigned idx = 0;
    for (unsigned j = 0; j < a.n_phase; ++j) {
        if (a.sched[j].start_tick <= t) idx = j; else break;
    }
    return a.sched[idx];
}

// The own ship's KINEMATIC command (fire stripped): firing never moves the shooter, and the own
// ship's hp/ammo/kills are wire-sourced, so prediction is pure motion (see session::run_client).
Command own_kinematic_command_at(const session::AircraftSpec& a, unsigned t) {
    const session::Phase& p = phase_at(a, t);
    return Command{p.target_phi, p.target_g, p.throttle, false};
}

}  // namespace

std::vector<predict::OwnState> authoritative_own_states(const Rails& rails,
                                                        const session::Scenario& sc) {
    const session::AircraftSpec& own = sc.aircraft[session::OWN_ID];
    // Fresh single-aircraft kernel in the own start state (gamma 0), stepped by the own kinematic
    // schedule. Own kinematics are independent of the other aircraft absent a hit — this is the
    // authoritative own(0) trajectory the client is judged against (seamless / bounded).
    Kernel k(rails);
    k.add(own.lat, own.lon, own.psi, own.phi, own.alt, own.tas, 0.0);
    std::vector<predict::OwnState> states;
    states.reserve(sc.ticks + 1);
    states.push_back({k.lat(0), k.lon(0), k.psi(0), k.phi(0), k.alt(0), k.tas(0), k.gamma(0)});
    std::vector<const Envelope*> envs{own.env};
    for (unsigned t = 0; t < sc.ticks; ++t) {
        std::vector<Command> cmds{own_kinematic_command_at(own, t)};
        k.step(cmds, envs);
        states.push_back({k.lat(0), k.lon(0), k.psi(0), k.phi(0), k.alt(0), k.tas(0), k.gamma(0)});
    }
    return states;
}

ClientResult run_predictive_client(const Rails& rails, const Envelope* own_env,
                                   const predict::OwnState& start,
                                   const std::vector<Command>& local_cmds,
                                   const session::ServerFrames& frames, unsigned lag,
                                   const std::vector<std::int64_t>& drop_emit_ticks, bool reconcile,
                                   Source src, const std::vector<predict::OwnState>& canonical) {
    const unsigned ticks = static_cast<unsigned>(local_cmds.size());

    auto is_dropped = [&](std::int64_t emit_tick) {
        for (std::int64_t d : drop_emit_ticks)
            if (d == emit_tick) return true;
        return false;
    };
    auto frame_at = [&](std::int64_t emit_tick) -> const std::vector<std::uint8_t>* {
        for (const auto& f : frames)
            if (f.first == emit_tick) return &f.second;
        return nullptr;
    };

    predict::Predictor predictor(rails, own_env, start);

    ClientResult res;
    const bool judge = !canonical.empty();
    for (unsigned t = 1; t <= ticks; ++t) {
        // 1) predict the own ship this tick (kinematics only — the fire bit is dropped)
        Command c = local_cmds[t - 1];
        c.fire = false;
        predictor.predict(t, c);

        // 2) ingest the authoritative frame for server_tick st = t - lag, if delivered. Both sources
        // reconcile at the SAME ticks — the ones a frame actually lands on (emit ticks, delivered
        // under lag, not in the loss set); CANONICAL just snaps to the full-precision state instead of
        // the decoded one. Gating on the frame keeps the reconcile cadence identical to the wire.
        std::int64_t st = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(lag);
        if (reconcile && st >= 0 && !is_dropped(st)) {
            if (const std::vector<std::uint8_t>* wire = frame_at(st)) {
                if (src == Source::CANONICAL) {
                    if (judge) {
                        predictor.reconcile(static_cast<std::uint32_t>(st), canonical[st]);
                        ++res.delivered;
                        ++res.reconciles;
                    }
                } else {
                    netsnap::Snapshot dec;
                    std::size_t pos = 0;
                    if (netsnap::decode_snapshot(wire->data(), wire->size(), pos, dec)) {
                        ++res.delivered;
                        for (const auto& e : dec.entities) {
                            if (e.id == session::OWN_ID) {
                                predict::OwnState auth{
                                    e.lat_deg * netsnap::DEG2RAD, e.lon_deg * netsnap::DEG2RAD,
                                    e.bearing_deg * netsnap::DEG2RAD, e.phi_deg * netsnap::DEG2RAD,
                                    e.alt_m, e.tas_mps, e.gamma_deg * netsnap::DEG2RAD};
                                predictor.reconcile(static_cast<std::uint32_t>(st), auth);
                                ++res.reconciles;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // 3) hash the predicted own KINEMATIC snapshot (a reproducible cross-impl artifact)
        std::string h = predict::tick_hash(predictor.kernel(), t);
        res.own_tick_hash.push_back(h);

        // 4) judge against the canonical authoritative own trajectory (if supplied)
        if (judge) {
            const Kernel& kk = predictor.kernel();
            const predict::OwnState& tru = canonical[t];
            res.max_pos_err = std::max({res.max_pos_err,
                                        std::abs(kk.lat(0) - tru.lat), std::abs(kk.lon(0) - tru.lon)});
            std::string truth_h = predict::hash_state(rails, tru, t);
            if (h != truth_h) {
                res.in_sync = false;
                if (res.first_divergent < 0) res.first_divergent = static_cast<long>(t);
                res.heal_tick = -1;
            } else if (res.heal_tick < 0) {
                res.heal_tick = static_cast<long>(t);
            }
        }
    }

    std::vector<std::uint8_t> cat;
    for (const auto& hh : res.own_tick_hash) cat.insert(cat.end(), hh.begin(), hh.end());
    res.digest = sha256_hex(cat);
    return res;
}

}  // namespace netpredict
}  // namespace seads
