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
           a.killed == e.killed;
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

    if (fails == 0) {
        std::printf("PASS: event channel reconstructs %d events bit-for-bit (5 hits + 1 kill); "
                    "digest matches; full recovery under isolated drops; K=%d blackout loses %zu "
                    "aged-out hits then resyncs (kill delivered); %u/%u windows delivered\n",
                    event_vec::EXPECTED_EVENT_COUNT, event::EVENT_WINDOW_K,
                    res.applied.size() - blk.applied.size(), res.delivered, res.n_windows);
        return 0;
    }
    std::printf("RESULT: event FAIL (%d mismatches)\n", fails);
    return 1;
}
