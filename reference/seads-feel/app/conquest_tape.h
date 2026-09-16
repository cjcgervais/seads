#pragma once

// =============================================================================
// CONQUEST TAPE (docs/conquest_tape_spec.md) — the AI primary-data recorder.
// Header-only, namespace seads_tape. Modeled on test/harness/recorder.h (the
// Recorder class + free serializer shape) but the wire format is JSONL
// (cqtape-v1, spec §3), not columns — the analyzer (tools/ai_tape.py, NOT
// built here) wants one self-describing record per line.
//
// FIREWALL (same discipline as seads_replay::Recorder): every method below
// takes CONST pointers/references and returns nothing that can feed back into
// a trajectory. on_tick reads app::TickInput/app::LoopState and the
// DroneWorld/CombatWorld/ConquestWorld/sim::Environment bundle exactly as
// step_frame received them (app/instructor_tick.h's ConquestTapeHook seam) —
// it cannot mutate any of them (every parameter is a pointer-to-const). No
// wall clock, no rng: the tick index (LoopState::tick_count) is the only time
// source, matching the hard rule in the spec banner.
//
// NEVER touches sim/, control/, test/golden/, or test/harness/recorder.h —
// this is a sibling module, not a graft onto the felt-flight tape.
// =============================================================================

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "app/instructor_tick.h"  // app::TickInput/LoopState/DroneWorld, inside_tunnel
#include "combat/conquest.h"      // combat::ConquestState/Pump/Outcome
#include "combat/kill.h"          // combat::CombatWorld, combat::is_dead
#include "drone/drone.h"          // drone::DroneState
#include "world/faction_bubbles.h"  // world::faction_ellipse
#include "world/tunnel_net.h"       // world::TunnelNet (via sim::Environment)

namespace seads_tape {

// ---------------------------------------------------------------------------
// fnv1a — REIMPLEMENTED locally (spec §2: "do NOT include that header"). Same
// offset-basis/prime as seads_replay::fnv1a (test/harness/recorder.h), so a
// cross-tool signature check would agree, but the two are structurally
// independent files.
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628257ull;

inline std::uint64_t fnv1a_update(std::uint64_t h, const std::string& s) {
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

// Sample cadence (spec §2.1): every 24th tick at 120 Hz sim -> 5 Hz.
constexpr int kSampleEvery = 24;

// game-AI-R4 foe constants mirrored here only as documentation (drone.h owns
// the real values; kFoeNone/kFoePlayer are read off drone::DroneState::foe
// directly — no re-derivation).

// Enum documentation (spec §3.1 — kept verbatim here AND in tools/ai_tape.py's
// header-adjacent comment, per the spec):
//   bfm  0=Intercept 1=Offensive 2=Yoyo 3=Extend 4=Perch 5=Slash 6=Defensive (4-6 = RUNG E7; 0-3 unchanged)      (drone::DroneState::bfm.mode)
//   mav  0=PATROL 1=TRANSIT 2=DIVE_IN 3=RUN 4=CLIMB_OUT (drone::DroneState::mav.mode)
// RUNG B2 (2026-08-26) d-row additions — kept verbatim in the spec §3.1 table
// and in tools/ai_tape.py's header comment, per the spec's own three-mirror
// rule:
//   ta  drone::DroneState::terrain_avoid_engaged (0|1) — the hard-deck latch.
//   gc  cmd_gamma, RADIANS: the FINAL commanded gamma handed to the autopilot
//       this tick (after every clamp) — the command that produced "pos".
//   bc  cmd_bank, RADIANS: likewise for bank (after the E1.4 slew).
//   af  air_frac, 0..1: sim::atm_frac_at at the post-tick position on this row.
// RUNG S1-DECK (2026-08-26) d-row addition, same three-mirror rule:
//   ds  drone::DroneState::deck_scope (0|1) — the DECK-SCOPE latch: "the
//       shipped 400 m release altitude has no air over my own ground point",
//       i.e. this drone is flying the 60/110 m deck band, not the in-bubble
//       250/400 one. Together with `ta` it says which band a latch tick was.
// ENV-1 (2026-08-29) d-row addition, same three-mirror rule:
//   ed  drone::DroneState::air_depth, METRES: signed horizontal depth into the
//       nearest LIVE dome's air — POSITIVE inside (how far in), NEGATIVE
//       outside (how far from the nearest breathable edge), 0.0 exactly on it.
//       ⚠ -1e9 is the "no live dome anywhere" sentinel, NOT a distance; gate on
//       it before using the value. Max over live domes, so it does not care
//       whose air it is. This is the quantity ENV-2's altitude allowance A(d) is a
//       function of, recorded one rung BEFORE any behaviour reads it.

class ConquestTape {
   public:
    explicit ConquestTape(const std::string& tag) : hash_(kFnvOffsetBasis) {
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "{\"t\":\"hdr\",\"v\":\"cqtape-v1\",\"tag\":\"%s\","
                      "\"dt\":%.6f,\"sample_every\":%d}\n",
                      tag.c_str(), 1.0 / 120.0, kSampleEvery);
        // hdr is NOT hashed (spec §3.0: "hdr excluded so the app can write it
        // immediately") — appended straight into the pending buffer.
        pending_ += buf;
    }

    // Called once per sim tick by the app::instructor_tick ConquestTapeHook.
    // tick = st.tick_count. Every pointer is nullable and read-only.
    void on_tick(const app::TickInput& in, const app::LoopState& st,
                 const app::DroneWorld* dw, const combat::CombatWorld* cw,
                 const app::ConquestWorld* cq, const sim::Environment* env) {
        (void)in;
        const long long k = st.tick_count;

        // §9 P1-2: the pumps table is a ONE-SHOT static line, emitted right
        // after hdr and never again — the very first on_tick with a live cq.
        if (!pmp_emitted_ && cq != nullptr) {
            emit_pmp(k, *cq);
            pmp_emitted_ = true;
        }

        if (k % kSampleEvery == 0) {
            emit_sample(k, st, dw, cw, cq, env);
        }

        // BUBBLES: at first on_tick (bub_emitted_ false) and again whenever
        // either faction's radius_scale changed vs the snapshot (spec §2.1).
        if (cq != nullptr) {
            bool changed = !bub_emitted_;
            if (bub_emitted_) {
                for (int f = 0; f < 2; ++f) {
                    if (cq->state.radius_scale[f] != prev_radius_scale_[f]) {
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) {
                emit_bubbles(k, *cq);
                bub_emitted_ = true;
                for (int f = 0; f < 2; ++f)
                    prev_radius_scale_[f] = cq->state.radius_scale[f];
            }
        }

        // §9 P0-1: ef needs NO snapshot — the round's own spawn contract
        // (active && age==0 && prev_pos==pos) is checked every tick,
        // including the very first (a slot that is retired-and-refilled in
        // the SAME enemy_fire_tick call never shows a true->true edge, which
        // is what the old slot-edge detector missed 90% of the time).
        emit_ef_spawns(k, dw, cw, st);

        // EVENTS: derived every tick from the snapshot, but the FIRST on_tick
        // only initializes the snapshot and emits nothing (spec §3.3).
        if (has_snapshot_) emit_events(k, dw, cw, cq, st, env);

        update_snapshot(dw, cw, cq, st, env);
        has_snapshot_ = true;
    }

    // Drain accumulated lines into out (append), clear the buffer. Returns
    // false if nothing was pending.
    bool drain(std::string& out) {
        if (pending_.empty()) return false;
        out += pending_;
        pending_.clear();
        return true;
    }

    // The footer signature line — the running fnv1a over every body byte
    // appended since construction (hdr excluded; every append() call below
    // feeds the hash the moment a line is produced, independent of drain()
    // timing, so footer() is correct whether or not the caller has drained
    // yet).
    std::string footer() const {
        char buf[64];
        std::snprintf(buf, sizeof buf, "{\"t\":\"sig\",\"fnv1a\":\"%llu\"}\n",
                      static_cast<unsigned long long>(hash_));
        return buf;
    }

   private:
    std::string pending_;
    std::uint64_t hash_;
    bool has_snapshot_ = false;
    bool bub_emitted_ = false;
    bool pmp_emitted_ = false;  // §9 P1-2: the static pumps line, one-shot
    double prev_radius_scale_[2] = {1.0, 1.0};

    // ---- previous-tick snapshot (edge detection, spec §3.3 + §9 P0-2) -----
    struct DroneSnap {
        bool inert = false;
        bool net = false;
        int mav_mode = 0;
        int foe = drone::kFoeNone;
        long long age_ticks = 0;  // §9 P0-2: dc is an age_ticks RESET, not a
                                  // position-jump/hp-reset guess
        glm::dvec3 pos{0.0};
        glm::dvec3 vel{0.0};
    };
    std::vector<DroneSnap> prev_drones_;
    bool prev_pump_alive_[combat::kNumPumps] = {true, true, true, true};
    // ★ L6: the respawn-lock edge. One-way in the state, so this only ever
    // goes false->true and the row can only ever be emitted once.
    bool prev_respawn_locked_ = false;
    bool prev_player_net_ = false;
    long long prev_deaths_ = 0;
    long long prev_death_events_ = 0;  // E6.1: BOTH death branches
    long long prev_player_hits_ = 0;

    // E6.1: the wire spelling of a death cause. ONE table, mirrored verbatim
    // in docs/conquest_tape_spec.md §3.3 and tools/ai_tape.py.
    static const char* cause_tag(combat::CombatWorld::DeathCause c) {
        switch (c) {
            case combat::CombatWorld::DeathCause::kCrash: return "crash";
            case combat::CombatWorld::DeathCause::kGun: return "gun";
            case combat::CombatWorld::DeathCause::kGround: return "ground";
            case combat::CombatWorld::DeathCause::kComponent: return "component";
            case combat::CombatWorld::DeathCause::kGiveUp: return "giveup";
            default: return "none";
        }
    }

    // ---- append: the ONE place a body byte enters the buffer + the hash ---
    void append(const std::string& line) {
        pending_ += line;
        hash_ = fnv1a_update(hash_, line);
    }

    // ---- sample rows (spec §3.1) -------------------------------------------
    void emit_sample(long long k, const app::LoopState& st,
                     const app::DroneWorld* dw, const combat::CombatWorld* cw,
                     const app::ConquestWorld* cq,
                     const sim::Environment* env) {
        emit_player(k, st, cw);
        if (dw != nullptr)
            for (const drone::DroneState& d : dw->drones)
                emit_drone(k, d, env);
        if (cq != nullptr) emit_conquest(k, *cq);
    }

    void emit_player(long long k, const app::LoopState& st,
                     const combat::CombatWorld* cw) {
        const double hp = cw != nullptr ? cw->player_hp : 0.0;
        const int alive =
            cw != nullptr ? (combat::is_dead(cw->damage) ? 0 : 1) : 1;
        // RUNG S3-GUNS: cumulative COSMETIC rounds spawned. The one number the
        // fly report quotes for "are there enemy rounds in the air?". A
        // cosmetic round carries damage 0.0 and is handed to no damage sweep
        // (combat/kill.h), so this counter can never explain a point of lost HP
        // — it explains the LOOK of one. Write-only observation, like every
        // other field on this row.
        const long long cr = cw != nullptr ? cw->cosmetic_rounds : 0;
        char buf[380];
        std::snprintf(
            buf, sizeof buf,
            "{\"t\":\"p\",\"k\":%lld,\"pos\":[%.2f,%.2f,%.2f],"
            "\"vel\":[%.2f,%.2f,%.2f],\"hp\":%.1f,\"alive\":%d,\"cr\":%lld}\n",
            k, st.curr.position.x, st.curr.position.y, st.curr.position.z,
            st.curr.velocity.x, st.curr.velocity.y, st.curr.velocity.z, hp,
            alive, cr);
        append(buf);
    }

    void emit_drone(long long k, const drone::DroneState& d,
                    const sim::Environment* env) {
        const bool net = app::inside_tunnel(env, d.curr.position);
        char buf[720];
        std::snprintf(
            buf, sizeof buf,
            "{\"t\":\"d\",\"k\":%lld,\"i\":%d,\"pos\":[%.3f,%.3f,%.3f],"
            "\"vel\":[%.3f,%.3f,%.3f],\"hp\":%.1f,\"inert\":%d,\"eng\":%d,"
            "\"foe\":%d,\"wf\":%d,\"bfm\":%d,\"mav\":%d,\"raid\":%d,"
            "\"strike\":%d,\"def\":%d,\"leash\":%d,\"fs\":%d,\"net\":%d,"
            "\"rpi\":%d,\"spi\":%d,\"ta\":%d,\"gc\":%.4f,\"bc\":%.4f,"
            "\"af\":%.3f,\"ds\":%d,\"vt\":%d,\"ed\":%.1f}\n",
            k, d.spawn_index, d.curr.position.x, d.curr.position.y,
            d.curr.position.z, d.curr.velocity.x, d.curr.velocity.y,
            d.curr.velocity.z, d.hp, d.inert ? 1 : 0, d.engaged ? 1 : 0,
            d.foe, d.wants_fire ? 1 : 0, static_cast<int>(d.bfm.mode),
            static_cast<int>(d.mav.mode), d.raid.active ? 1 : 0,
            d.strike.active ? 1 : 0, d.defend.active ? 1 : 0,
            d.leash_engaged ? 1 : 0, d.friendly_side ? 1 : 0, net ? 1 : 0,
            d.raid.pump_idx, d.strike.pump_idx,
            d.terrain_avoid_engaged ? 1 : 0, d.cmd_gamma, d.cmd_bank,
            d.air_frac, d.deck_scope ? 1 : 0,
            d.raid.via_tunnel ? 1 : 0, d.air_depth);
        append(buf);
    }

    // §9 P1-2: the static pumps table — ONE line, right after hdr, never
    // repeated (positions %.1f; fac/surf mirror combat::Pump's own fields).
    void emit_pmp(long long k, const app::ConquestWorld& cq) {
        const combat::ConquestState& cs = cq.state;
        std::string line = "{\"t\":\"pmp\",\"k\":" + std::to_string(k) +
                           ",\"pos\":[";
        for (int i = 0; i < combat::kNumPumps; ++i) {
            char seg[64];
            std::snprintf(seg, sizeof seg, "[%.1f,%.1f,%.1f]%s",
                          cs.pumps[i].pos.x, cs.pumps[i].pos.y,
                          cs.pumps[i].pos.z,
                          i + 1 < combat::kNumPumps ? "," : "");
            line += seg;
        }
        line += "],\"fac\":[";
        for (int i = 0; i < combat::kNumPumps; ++i) {
            line += std::to_string(cs.pumps[i].faction);
            if (i + 1 < combat::kNumPumps) line += ",";
        }
        line += "],\"surf\":[";
        for (int i = 0; i < combat::kNumPumps; ++i) {
            line += cs.pumps[i].surface ? "1" : "0";
            if (i + 1 < combat::kNumPumps) line += ",";
        }
        line += "]}\n";
        append(line);
    }

    void emit_conquest(long long k, const app::ConquestWorld& cq) {
        const combat::ConquestState& cs = cq.state;
        char buf[400];
        std::snprintf(
            buf, sizeof buf,
            "{\"t\":\"cq\",\"k\":%lld,\"pump_hp\":[%.3f,%.3f,%.3f,%.3f],"
            "\"rs\":[%.3f,%.3f],\"score\":[%d,%d],\"out\":%d,\"pua\":%d,"
            "\"rp\":%d,\"cd_f\":%d,\"cd_s\":%.3f,\"planes\":%d,\"dm\":%d}\n",
            k, cs.pumps[0].hp, cs.pumps[1].hp, cs.pumps[2].hp,
            cs.pumps[3].hp, cs.radius_scale[0], cs.radius_scale[1],
            cs.score[0], cs.score[1], static_cast<int>(cs.outcome),
            cq.pump_under_attack ? 1 : 0, cq.raided_pump,
            cs.countdown_faction, cs.countdown_s, cs.planes_left,
            cs.deathmatch ? 1 : 0);
        append(buf);
    }

    // ---- bubbles row (spec §3.2) -------------------------------------------
    void emit_bubbles(long long k, const app::ConquestWorld& cq) {
        world::FactionGrowth grow[2];
        app::faction_growth_from_state(cq.state, grow);
        std::string line = "{\"t\":\"bub\",\"k\":" + std::to_string(k) +
                           ",\"f\":[";
        for (int f = 0; f < 2; ++f) {
            glm::dvec3 c{0.0}, maj{0.0};
            double a = 0.0, b = 0.0;
            world::faction_ellipse(f, grow, c, maj, a, b);
            char seg[256];
            std::snprintf(seg, sizeof seg,
                          "{\"c\":[%.6f,%.6f,%.6f],\"maj\":[%.6f,%.6f,%.6f],"
                          "\"a\":%.1f,\"b\":%.1f}%s",
                          c.x, c.y, c.z, maj.x, maj.y, maj.z, a, b,
                          f == 0 ? "," : "");
            line += seg;
        }
        line += "]}\n";
        append(line);
    }

    // ---- event rows (spec §3.3) --------------------------------------------
    void emit_events(long long k, const app::DroneWorld* dw,
                     const combat::CombatWorld* cw,
                     const app::ConquestWorld* cq, const app::LoopState& st,
                     const sim::Environment* env) {
        // PLAYER HIT / PLAYER DEATH.
        if (cw != nullptr) {
            if (cw->player_hits > prev_player_hits_) {
                char buf[128];
                std::snprintf(
                    buf, sizeof buf, "{\"t\":\"ph\",\"k\":%lld,\"n\":%lld,\"hp\":%.1f}\n",
                    k, cw->player_hits - prev_player_hits_, cw->player_hp);
                append(buf);
            }
            // RUNG E6.1 — PLAYER DEATH, WITH ITS CAUSE (spec §3.3 amended).
            // The edge is CombatWorld::death_events, which BOTH app death
            // seams book — not `deaths`, which only ever counted component
            // deaths and left every terrain crash invisible (Chad's tape 2:
            // two deaths, zero 'pd'). Fields are pure observation; the tape
            // stays a read-only observer.
            if (cw->death_events > prev_death_events_) {
                char buf[400];
                std::snprintf(
                    buf, sizeof buf,
                    "{\"t\":\"pd\",\"k\":%lld,\"cause\":\"%s\","
                    "\"pos\":[%.3f,%.3f,%.3f],\"air\":%.4f,\"net\":%d,"
                    "\"comp\":[%.3f,%.3f,%.3f,%.3f,%.3f]}\n",
                    k, cause_tag(cw->death_cause), cw->death_pos.x,
                    cw->death_pos.y, cw->death_pos.z, cw->death_air_frac,
                    cw->death_in_net ? 1 : 0, cw->death_damage.engine,
                    cw->death_damage.pilot, cw->death_damage.wing_left,
                    cw->death_damage.wing_right, cw->death_damage.structure);
                append(buf);
            }
        }

        // §9 P0-2: DRONE KILLED BY FIRE (dk, kill-list membership, unchanged)
        // / DRONE SHOT DOWN AI-vs-AI (da, NEW — inert rise WITHOUT kill-list
        // membership: instructor_tick.h's AI-vs-AI gunnery site sets inert
        // directly, never through killed_spawn_indices) / DRONE CRASHED (dc,
        // REDEFINED — an age_ticks RESET, i.e. drone::respawn_in_place fired;
        // a terrain crash in conquest never rises inert, so this is the ONLY
        // signal a true crash/respawn leaves). dk/da share the same fields
        // (both are "shot down"); dc keeps its own field set.
        if (dw != nullptr) {
            const std::vector<int>* killed =
                cw != nullptr ? &cw->killed_spawn_indices : nullptr;
            for (std::size_t i = 0; i < dw->drones.size(); ++i) {
                const drone::DroneState& d = dw->drones[i];
                const bool has_prev = i < prev_drones_.size();
                if (!has_prev) continue;  // no baseline to edge-detect against
                const DroneSnap& prev = prev_drones_[i];
                bool killed_this_tick = false;
                if (killed != nullptr) {
                    for (int si : *killed) {
                        if (si == d.spawn_index) {
                            killed_this_tick = true;
                            break;
                        }
                    }
                }
                if (killed_this_tick) {
                    emit_shootdown(k, "dk", d, prev, st.curr.position);
                    continue;  // a killed drone is never also reported da/dc
                }
                const bool rose_inert = !prev.inert && d.inert;
                if (rose_inert) {
                    emit_shootdown(k, "da", d, prev, st.curr.position);
                    continue;
                }
                // E5 REINFORCEMENT (before the age_ticks branch, or a wave
                // revive — which also resets age_ticks — reads as a phantom
                // 'dc' terrain crash and poisons the attribution): a slot
                // going inert -> alive is a wave, its own 'dr' event.
                const bool reinforced = prev.inert && !d.inert;
                if (reinforced) {
                    char buf[400];
                    std::snprintf(
                        buf, sizeof buf,
                        "{\"t\":\"dr\",\"k\":%lld,\"i\":%d,\"pos\":[%.3f,%.3f,"
                        "%.3f]}\n",
                        k, d.spawn_index, d.curr.position.x, d.curr.position.y,
                        d.curr.position.z);
                    append(buf);
                    continue;
                }
                if (d.age_ticks < prev.age_ticks) {  // the respawn reset
                    char buf[400];
                    std::snprintf(
                        buf, sizeof buf,
                        "{\"t\":\"dc\",\"k\":%lld,\"i\":%d,\"pos\":[%.3f,%.3f,"
                        "%.3f],\"mav\":%d,\"net\":%d,\"vel\":[%.3f,%.3f,%.3f]}\n",
                        k, d.spawn_index, prev.pos.x, prev.pos.y, prev.pos.z,
                        prev.mav_mode, prev.net ? 1 : 0, prev.vel.x, prev.vel.y,
                        prev.vel.z);
                    append(buf);
                }
            }
        }

        // TUNNEL TRANSITION — drones then the player, net flag edge either way.
        if (dw != nullptr) {
            for (std::size_t i = 0; i < dw->drones.size(); ++i) {
                if (i >= prev_drones_.size()) continue;
                const drone::DroneState& d = dw->drones[i];
                const bool net = app::inside_tunnel(env, d.curr.position);
                if (net == prev_drones_[i].net) continue;
                char buf[256];
                std::snprintf(
                    buf, sizeof buf,
                    "{\"t\":\"tn\",\"k\":%lld,\"who\":%d,\"in\":%d,"
                    "\"pos\":[%.3f,%.3f,%.3f]}\n",
                    k, d.spawn_index, net ? 1 : 0, d.curr.position.x,
                    d.curr.position.y, d.curr.position.z);
                append(buf);
            }
        }
        {
            const bool pnet = app::inside_tunnel(env, st.curr.position);
            if (pnet != prev_player_net_) {
                char buf[256];
                std::snprintf(
                    buf, sizeof buf,
                    "{\"t\":\"tn\",\"k\":%lld,\"who\":-2,\"in\":%d,"
                    "\"pos\":[%.3f,%.3f,%.3f]}\n",
                    k, pnet ? 1 : 0, st.curr.position.x, st.curr.position.y,
                    st.curr.position.z);
                append(buf);
            }
        }

        // PUMP DEATH — alive edge.
        if (cq != nullptr) {
            for (int i = 0; i < combat::kNumPumps; ++i) {
                const combat::Pump& pu = cq->state.pumps[i];
                if (prev_pump_alive_[i] && !pu.alive) {
                    char buf[128];
                    std::snprintf(buf, sizeof buf,
                                  "{\"t\":\"pk\",\"k\":%lld,\"pump\":%d,"
                                  "\"fac\":%d}\n",
                                  k, i, pu.faction);
                    append(buf);
                }
            }
        }

        // ★★★ L2 — PUMP REPAIRED, the OPPOSITE alive edge (millwright rung,
        // docs/PLAN_20260901_game_loop_millwright.md §4.2). Until this rung
        // `alive` was MONOTONE true->false and this file assumed it: only one
        // of the two edges had a row, so a revive replayed as nothing at all.
        //
        // ⚠ A NEW TAG IS ADDITIVE, WHICH IS WHY IT IS A NEW TAG. Not one
        // existing row changes shape, so a tape recorded before this rung
        // decodes byte-for-byte as it always did, and a session with no repair
        // in it emits no `pr` row at all. The dome scales ride along because a
        // revive is the ONE event that can move them upward, and the `bub`
        // row's own change-detector reads the same pair on the same tick.
        if (cq != nullptr) {
            for (int i = 0; i < combat::kNumPumps; ++i) {
                const combat::Pump& pu = cq->state.pumps[i];
                if (!prev_pump_alive_[i] && pu.alive) {
                    char buf[192];
                    std::snprintf(buf, sizeof buf,
                                  "{\"t\":\"pr\",\"k\":%lld,\"pump\":%d,"
                                  "\"fac\":%d,\"rs\":[%.3f,%.3f]}\n",
                                  k, i, pu.faction, cq->state.radius_scale[0],
                                  cq->state.radius_scale[1]);
                    append(buf);
                }
            }
        }

        // ★★★ L6 — RESPAWNS LOCKED, the match-wide latch (Chad's ruling R9,
        // 2026-09-03). Emitted ONCE, on the rising edge, so a tape shows the
        // exact tick from which nobody on either side got another aeroplane --
        // the fact every later "why is the sky emptying" question is answered
        // by. `rs` rides along because the arm and the collapse are the same
        // event and reading them off one row beats correlating two.
        //
        // ⚠ ADDITIVE, LIKE `pr`: a new tag, no existing row changed, and a
        // session that never arms the clock emits no `rl` row at all -- so
        // every tape recorded before this rung decodes byte-for-byte as it did.
        if (cq != nullptr && !prev_respawn_locked_ &&
            cq->state.respawn_locked) {
            char buf[192];
            std::snprintf(buf, sizeof buf,
                          "{\"t\":\"rl\",\"k\":%lld,\"cf\":%d,"
                          "\"cs\":%.3f}\n",
                          k, cq->state.countdown_faction,
                          cq->state.countdown_s);
            append(buf);
        }
    }

    // §9 P0-2: dk ("shot down by fire") and da ("shot down AI-vs-AI") share
    // the exact same field set, built from the PREVIOUS-tick snapshot (its
    // state when it died, before any wreck handling) — only the tag differs.
    void emit_shootdown(long long k, const char* tag,
                        const drone::DroneState& d, const DroneSnap& prev,
                        const glm::dvec3& player_pos) {
        char buf[400];
        const double rng_p = glm::length(prev.pos - player_pos);
        std::snprintf(
            buf, sizeof buf,
            "{\"t\":\"%s\",\"k\":%lld,\"i\":%d,\"pos\":[%.3f,%.3f,%.3f],"
            "\"foe\":%d,\"rng_p\":%.3f,\"mav\":%d,\"net\":%d}\n",
            tag, k, d.spawn_index, prev.pos.x, prev.pos.y, prev.pos.z,
            prev.foe, rng_p, prev.mav_mode, prev.net ? 1 : 0);
        append(buf);
    }

    // §9 P0-1: ENEMY ROUND SPAWNED — checked every tick against every active
    // round's OWN spawn contract (kill.h: active && age==0 && prev_pos==pos),
    // never a slot-index edge (a slot retired-and-refilled inside the SAME
    // enemy_fire_tick call never shows a true->true transition — the old
    // detector's 90%-missed regression). No snapshot needed.
    void emit_ef_spawns(long long k, const app::DroneWorld* dw,
                        const combat::CombatWorld* cw,
                        const app::LoopState& st) {
        if (cw == nullptr) return;
        for (const weapon::Projectile& round : cw->enemy_pool) {
            if (!round.active || round.age != 0.0) continue;
            if (round.prev_pos != round.pos) continue;
            emit_ef(k, dw, round, st.curr.position);
        }
    }

    // Shooter attribution (spec §3.3): the non-inert drone minimizing distance
    // to the round's spawn position; > 100 m => shooter -1 (never guess).
    void emit_ef(long long k, const app::DroneWorld* dw,
                const weapon::Projectile& round,
                const glm::dvec3& player_pos) {
        int shooter = -1;
        double best_d = 1e300;
        if (dw != nullptr) {
            for (const drone::DroneState& d : dw->drones) {
                if (d.inert) continue;
                const double dist = glm::length(d.curr.position - round.pos);
                if (dist < best_d) {
                    best_d = dist;
                    shooter = d.spawn_index;
                }
            }
        }
        if (shooter >= 0 && best_d > 100.0) shooter = -1;
        double rng_p = -1.0;
        int tgt_foe = drone::kFoeNone;
        if (shooter >= 0 && dw != nullptr) {
            for (const drone::DroneState& d : dw->drones) {
                if (d.spawn_index != shooter) continue;
                tgt_foe = d.foe;
                rng_p = glm::length(d.curr.position - player_pos);
                break;
            }
        }
        char buf[256];
        std::snprintf(buf, sizeof buf,
                      "{\"t\":\"ef\",\"k\":%lld,\"shooter\":%d,\"att_m\":%.3f,"
                      "\"rng_p\":%.3f,\"tgt_foe\":%d}\n",
                      k, shooter, shooter >= 0 ? best_d : -1.0, rng_p,
                      tgt_foe);
        append(buf);
    }

    // ---- snapshot update ----------------------------------------------------
    void update_snapshot(const app::DroneWorld* dw,
                         const combat::CombatWorld* cw,
                         const app::ConquestWorld* cq,
                         const app::LoopState& st,
                         const sim::Environment* env) {
        if (dw != nullptr) {
            prev_drones_.resize(dw->drones.size());
            for (std::size_t i = 0; i < dw->drones.size(); ++i) {
                const drone::DroneState& d = dw->drones[i];
                DroneSnap& s = prev_drones_[i];
                s.inert = d.inert;
                s.net = app::inside_tunnel(env, d.curr.position);
                s.mav_mode = static_cast<int>(d.mav.mode);
                s.foe = d.foe;
                s.age_ticks = d.age_ticks;
                s.pos = d.curr.position;
                s.vel = d.curr.velocity;
            }
        }
        if (cw != nullptr) {
            prev_deaths_ = cw->deaths;
            prev_death_events_ = cw->death_events;  // E6.1
            prev_player_hits_ = cw->player_hits;
        }
        if (cq != nullptr) {
            for (int i = 0; i < combat::kNumPumps; ++i)
                prev_pump_alive_[i] = cq->state.pumps[i].alive;
            prev_respawn_locked_ = cq->state.respawn_locked;  // ★ L6
        }
        prev_player_net_ = app::inside_tunnel(env, st.curr.position);
    }
};

}  // namespace seads_tape
