// SEADS reliable combat-EVENT channel parity test (netcode layer 6). Reruns the whole
// server->transport->client event channel over the SESSION-SK-001 scenario in event_vectors.h and
// asserts the client reconstructs the event stream BIT-FOR-BIT identically to the Python reference
// (tools/event_ref.py):
//   1) SERVER LOG — the derived events (six integer fields each) match EXPECTED_EVENTS (5 hits
//      walking the A6M2's hp 70 -> 0, then the kill);
//   2) FULL RECONSTRUCTION — under the scenario's isolated single drops the client's applied journal
//      equals the server log, and its EVENT_DIGEST matches (the redundant journal delivered every
//      event, even though 5 frames dropped);
//   3) KILL — the reconstructed kill event (precise tick + final hp) matches;
//   4) BLACKOUT (reliability bound) — under a K-consecutive-frame blackout the client recovers
//      exactly BLACKOUT_APPLIED_SEQS (aged-out early hits lost, journal resyncs, kill still lands)
//      and its BLACKOUT_DIGEST matches — a cross-impl proof of the loss bound;
//   5) ACCOUNTING — windows emitted (N_WINDOWS) / delivered (DELIVERED) match.
// Exit 0 PASS, 1 FAIL.
#include "event.h"
#include "event_vectors.h"

#include "golden_params.h"    // sealed rails scalars (R_M, DT_S, G0, ATM_TOP_M, SOFT_M)
#include "kernel.h"

#include <cstdio>
#include <vector>

using namespace seads;

static Rails sealed_rails() {
    Rails r;
    r.R = golden::R_M;
    r.dt = golden::DT_S;
    r.g0 = golden::G0;
    r.atm_top = golden::ATM_TOP_M;
    r.soft = golden::SOFT_M;
    return r;
}

static bool eq_event(const event::Event& a, const event_vec::ExpectEvent& e) {
    return a.seq == e.seq && a.tick == e.tick && a.target == e.target &&
           a.damage_milli == e.damage_milli && a.hp_after_milli == e.hp_after_milli &&
           a.killed == e.killed && a.attacker == e.attacker;   // v1.17r0: attributed kill-feed
}

int main() {
    int fails = 0;
    const Rails rails = sealed_rails();

    // WINDOW_K sanity: the generated header and the library must agree on the redundancy depth.
    if (event::EVENT_WINDOW_K != event_vec::WINDOW_K) {
        ++fails;
        std::printf("FAIL EVENT_WINDOW_K %d != header WINDOW_K %d\n",
                    event::EVENT_WINDOW_K, event_vec::WINDOW_K);
    }

    // --- standard drops: full reconstruction ---
    const event::EventResult res = event::run_events(rails, sess_vec::SCENARIO);

    // 1) server-derived log matches expected
    if (static_cast<int>(res.events.size()) != event_vec::EXPECTED_EVENT_COUNT) {
        ++fails;
        std::printf("FAIL derived %zu events, expected %d\n",
                    res.events.size(), event_vec::EXPECTED_EVENT_COUNT);
    } else {
        for (int i = 0; i < event_vec::EXPECTED_EVENT_COUNT; ++i) {
            if (!eq_event(res.events[i], event_vec::EXPECTED_EVENTS[i])) {
                ++fails;
                const auto& a = res.events[i];
                std::printf("FAIL server event %d: got (seq=%lld tick=%lld tgt=%lld dmg=%lld "
                            "hp=%lld kill=%lld)\n", i,
                            (long long)a.seq, (long long)a.tick, (long long)a.target,
                            (long long)a.damage_milli, (long long)a.hp_after_milli,
                            (long long)a.killed);
            }
        }
    }

    // 2) full reconstruction: applied journal == server log, and the digest matches the reference
    if (res.applied.size() != res.events.size()) {
        ++fails;
        std::printf("FAIL applied %zu != server %zu events (should fully reconstruct under "
                    "isolated single drops)\n", res.applied.size(), res.events.size());
    }
    if (res.digest != event_vec::EVENT_DIGEST) {
        ++fails;
        std::printf("FAIL event digest mismatch:\n  got %s\n  exp %s\n",
                    res.digest.c_str(), event_vec::EVENT_DIGEST);
    }

    // 3) kill event faithfully reconstructed
    {
        const event::Event* kill = nullptr;
        for (const auto& e : res.applied) if (e.killed) { kill = &e; break; }
        if (kill == nullptr) {
            ++fails;
            std::printf("FAIL no kill event reconstructed on the client\n");
        } else if (!eq_event(*kill, event_vec::EXPECTED_EVENTS[event_vec::KILL_INDEX])) {
            ++fails;
            std::printf("FAIL kill event mismatch: got (seq=%lld tick=%lld tgt=%lld hp=%lld)\n",
                        (long long)kill->seq, (long long)kill->tick, (long long)kill->target,
                        (long long)kill->hp_after_milli);
        }
    }

    // 4) transport accounting
    if (res.n_windows != event_vec::N_WINDOWS) {
        ++fails;
        std::printf("FAIL emitted %u windows, expected %u\n", res.n_windows, event_vec::N_WINDOWS);
    }
    if (res.delivered != event_vec::DELIVERED) {
        ++fails;
        std::printf("FAIL delivered %u windows, expected %u\n", res.delivered, event_vec::DELIVERED);
    }

    // 5) BLACKOUT reliability bound (cross-impl): a K-consecutive-frame blackout loses exactly the
    //    aged-out early hits; the journal resyncs and still delivers the kill. Recovered seqs +
    //    digest must match the reference.
    const event::EventResult blk = event::run_events(
        rails, sess_vec::SCENARIO, event_vec::BLACKOUT_DROPS, event_vec::BLACKOUT_N_DROPS);
    if (static_cast<int>(blk.applied.size()) != event_vec::BLACKOUT_APPLIED_COUNT) {
        ++fails;
        std::printf("FAIL blackout recovered %zu events, expected %d\n",
                    blk.applied.size(), event_vec::BLACKOUT_APPLIED_COUNT);
    } else {
        for (int i = 0; i < event_vec::BLACKOUT_APPLIED_COUNT; ++i) {
            if (blk.applied[i].seq != event_vec::BLACKOUT_APPLIED_SEQS[i]) {
                ++fails;
                std::printf("FAIL blackout applied[%d] seq=%lld, expected %lld\n", i,
                            (long long)blk.applied[i].seq,
                            (long long)event_vec::BLACKOUT_APPLIED_SEQS[i]);
            }
        }
    }
    if (blk.digest != event_vec::BLACKOUT_DIGEST) {
        ++fails;
        std::printf("FAIL blackout digest mismatch:\n  got %s\n  exp %s\n",
                    blk.digest.c_str(), event_vec::BLACKOUT_DIGEST);
    }
    // the blackout must be genuinely lossy (fewer events than the full run) — else it proves nothing
    if (blk.applied.size() >= res.applied.size()) {
        ++fails;
        std::printf("FAIL blackout not lossy (recovered %zu >= full %zu) — vacuous bound\n",
                    blk.applied.size(), res.applied.size());
    }

    // 6) PER-ROUND GRANULARITY (EVENT-MULTIHIT-001, cross-impl): twin shooters land both rounds of
    //    every volley on the SAME tick -> two attributed events per volley (the pre-hit-queue
    //    hp-delta derivation lumped them into one, credited to the last writer); the kill volley
    //    shows the overkill clamp (effective loss < carried damage). Events + digest must match the
    //    Python reference bit-for-bit, and the structural per-round claims must hold.
    const event::EventResult mh = event::run_events(rails, event_vec::MH_SCENARIO);
    if (static_cast<int>(mh.events.size()) != event_vec::MH_EXPECTED_EVENT_COUNT) {
        ++fails;
        std::printf("FAIL multihit derived %zu events, expected %d\n",
                    mh.events.size(), event_vec::MH_EXPECTED_EVENT_COUNT);
    } else {
        for (int i = 0; i < event_vec::MH_EXPECTED_EVENT_COUNT; ++i) {
            if (!eq_event(mh.events[i], event_vec::MH_EXPECTED_EVENTS[i])) {
                ++fails;
                const auto& a = mh.events[i];
                std::printf("FAIL multihit event %d: got (seq=%lld tick=%lld tgt=%lld dmg=%lld "
                            "hp=%lld kill=%lld atk=%lld)\n", i,
                            (long long)a.seq, (long long)a.tick, (long long)a.target,
                            (long long)a.damage_milli, (long long)a.hp_after_milli,
                            (long long)a.killed, (long long)a.attacker);
            }
        }
        // structural per-round claims: every tick carries TWO events on the one target, attributed
        // to the two DIFFERENT shooters (0 then 1, projectile array order) — except the corpse
        // half of the kill volley never lands ("a dead aircraft can't be hit" keeps pairs whole
        // here because the kill IS the volley's second round).
        for (std::size_t i = 0; i + 1 < mh.events.size(); i += 2) {
            const auto& a = mh.events[i];
            const auto& b = mh.events[i + 1];
            if (!(a.tick == b.tick && a.target == 2 && b.target == 2 &&
                  a.attacker == 0 && b.attacker == 1)) {
                ++fails;
                std::printf("FAIL multihit volley at events %zu/%zu: want same tick, target 2, "
                            "attackers 0 then 1\n", i, i + 1);
            }
        }
        const auto& kill = mh.events.back();
        const auto& prev = mh.events[mh.events.size() - 2];
        if (!(kill.killed == 1 && kill.attacker == 1 && kill.hp_after_milli == 0 &&
              kill.damage_milli == prev.hp_after_milli && kill.damage_milli < 12000)) {
            ++fails;
            std::printf("FAIL multihit kill: want shooter 1's overkill round clamped to the "
                        "remaining hp\n");
        }
        std::int64_t total = 0;
        for (const auto& e : mh.events) total += e.damage_milli;
        if (total != 70000) {   // the A6M2's full hp, walked exactly to 0
            ++fails;
            std::printf("FAIL multihit damage sum %lld != 70000\n", (long long)total);
        }
    }
    if (mh.applied.size() != mh.events.size()) {
        ++fails;
        std::printf("FAIL multihit applied %zu != server %zu (should fully reconstruct)\n",
                    mh.applied.size(), mh.events.size());
    }
    if (mh.digest != event_vec::MH_EVENT_DIGEST) {
        ++fails;
        std::printf("FAIL multihit digest mismatch:\n  got %s\n  exp %s\n",
                    mh.digest.c_str(), event_vec::MH_EVENT_DIGEST);
    }
    if (mh.n_windows != event_vec::MH_N_WINDOWS || mh.delivered != event_vec::MH_DELIVERED) {
        ++fails;
        std::printf("FAIL multihit accounting: %u/%u windows, expected %u/%u\n",
                    mh.delivered, mh.n_windows, event_vec::MH_DELIVERED, event_vec::MH_N_WINDOWS);
    }

    if (fails == 0) {
        std::printf("PASS: event channel reconstructs %d events bit-for-bit (5 hits + 1 kill); "
                    "digest matches; full recovery under isolated drops; K=%d blackout loses %zu "
                    "aged-out hits then resyncs (kill delivered); %u/%u windows delivered; "
                    "multihit granularity: %d per-round events (2 per volley, distinct attackers, "
                    "overkill clamped) reconstructed bit-for-bit\n",
                    event_vec::EXPECTED_EVENT_COUNT, event::EVENT_WINDOW_K,
                    res.applied.size() - blk.applied.size(), res.delivered, res.n_windows,
                    event_vec::MH_EXPECTED_EVENT_COUNT);
        return 0;
    }
    std::printf("RESULT: event FAIL (%d mismatches)\n", fails);
    return 1;
}
