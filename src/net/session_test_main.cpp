// SEADS server<->client SESSION loop parity test (netcode layer 5). Reruns the whole
// server->transport->client session over the SESSION-SK-001 scenario in session_vectors.h and
// asserts the client reconstructs the dogfight BIT-FOR-BIT identically to the Python reference
// (tools/session_ref.py):
//   1) SEQUENCE_DIGEST — SHA-256 over the reconstructed per-tick client-view hashes matches (the
//      entire fight — own predicted ship + interpolated remotes + wire-sourced HP/kills/rounds —
//      reconstructs to the identical bytes, even though the transport is lossy);
//   2) CHECKPOINTS — the client-view hash at selected ticks matches (readable localization);
//   3) KILL REPLICATION — the client's freshest-frame per-aircraft (hp_milli, dead, ammo) matches
//      FINAL_WEAPON (A6M2 dead, P-47 + Spitfire alive — the gun kill crossed the wire; and the
//      P-47's rounds-remaining counter replicated from the wire — v1.14r0);
//   4) TRANSPORT accounting — frames emitted (N_FRAMES) / received (DELIVERED) match under the
//      deterministic packet-loss set;
//   5) reconcile is LOAD-BEARING — turning the own-ship wire reconcile off changes the
//      reconstructed digest (the lossy reseed genuinely feeds the client view).
// Exit 0 PASS, 1 FAIL.
#include "session.h"
#include "session_vectors.h"

#include "geo001.h"            // geo001::quantize (for the weapon-fact comparison)
#include "golden_params.h"    // sealed rails scalars (R_M, DT_S, G0, ATM_TOP_M, SOFT_M)
#include "kernel.h"

#include <algorithm>
#include <cstdio>
#include <string>
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

int main() {
    int fails = 0;
    const Rails rails = sealed_rails();

    const session::SessionResult res = session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/true);

    // --- 1) whole-session digest + tick count ---
    if (res.per_tick.size() != sess_vec::TICKS) {
        ++fails;
        std::printf("FAIL produced %zu view hashes, expected %u\n",
                    res.per_tick.size(), sess_vec::TICKS);
    }
    if (res.digest != sess_vec::SEQUENCE_DIGEST) {
        ++fails;
        std::printf("FAIL session digest mismatch:\n  got %s\n  exp %s\n",
                    res.digest.c_str(), sess_vec::SEQUENCE_DIGEST);
    }

    // --- 2) checkpoints ---
    if (res.per_tick.size() == sess_vec::TICKS) {
        for (int i = 0; i < sess_vec::CHECKPOINT_COUNT; ++i) {
            const auto& c = sess_vec::CHECKPOINTS[i];
            if (c.tick < 1 || c.tick > sess_vec::TICKS || res.per_tick[c.tick - 1] != c.hash) {
                ++fails;
                std::printf("FAIL checkpoint tick %u:\n  got %s\n  exp %s\n", c.tick,
                            (c.tick >= 1 && c.tick <= sess_vec::TICKS)
                                ? res.per_tick[c.tick - 1].c_str() : "<oob>",
                            c.hash);
            }
        }
    }

    // --- 3) KILL REPLICATION: the freshest client frame reproduces the combat outcome ---
    if (!res.has_final_wframe) {
        ++fails;
        std::printf("FAIL client never received a frame (no weapon state to replicate)\n");
    } else {
        std::vector<netsnap::EntityState> wents = res.final_wframe.entities;
        std::sort(wents.begin(), wents.end(),
                  [](const netsnap::EntityState& a, const netsnap::EntityState& b) { return a.id < b.id; });
        if (static_cast<int>(wents.size()) != sess_vec::FINAL_WEAPON_COUNT) {
            ++fails;
            std::printf("FAIL final weapon count %zu != expected %d\n",
                        wents.size(), sess_vec::FINAL_WEAPON_COUNT);
        } else {
            for (int i = 0; i < sess_vec::FINAL_WEAPON_COUNT; ++i) {
                const auto& f = sess_vec::FINAL_WEAPON[i];
                const auto& e = wents[i];
                std::int64_t hp_milli = geo001::quantize(e.hp, netsnap::HP_SCALE);
                int dead = e.hp <= 0.0 ? 1 : 0;
                std::int64_t ammo = geo001::quantize(e.ammo, netsnap::AMMO_SCALE);
                std::int64_t last_hit_by = geo001::quantize(e.last_hit_by, netsnap::LASTHITBY_SCALE);
                if (e.id != f.id || hp_milli != f.hp_milli || dead != f.dead || ammo != f.ammo
                    || last_hit_by != f.last_hit_by) {
                    ++fails;
                    std::printf("FAIL weapon fact id=%lld: got (hp_milli=%lld, dead=%d, ammo=%lld) "
                                "exp (id=%lld, hp_milli=%lld, dead=%d, ammo=%lld)\n",
                                static_cast<long long>(e.id), static_cast<long long>(hp_milli),
                                dead, static_cast<long long>(ammo), static_cast<long long>(f.id),
                                static_cast<long long>(f.hp_milli), f.dead,
                                static_cast<long long>(f.ammo));
                }
            }
        }
    }

    // --- 4) transport accounting ---
    if (res.n_frames != sess_vec::N_FRAMES) {
        ++fails;
        std::printf("FAIL emitted %u frames, expected %u\n", res.n_frames, sess_vec::N_FRAMES);
    }
    if (res.delivered != sess_vec::DELIVERED) {
        ++fails;
        std::printf("FAIL client received %u frames, expected %u\n",
                    res.delivered, sess_vec::DELIVERED);
    }

    // --- 5) reconcile is load-bearing: no-reconcile reconstruction differs (the lossy wire reseed
    //        genuinely perturbs the predicted own ship in the client view) ---
    const session::SessionResult no_rec =
        session::run_session(rails, sess_vec::SCENARIO, /*reconcile=*/false);
    if (no_rec.digest == res.digest) {
        ++fails;
        std::printf("FAIL no-reconcile digest equals reconcile digest (reconcile not load-bearing)\n");
    }

    if (fails == 0) {
        std::printf("PASS: session reconstructs %u ticks bit-for-bit; digest matches reference; "
                    "kill replicated over the wire (A6M2 dead, P-47/Spitfire alive); "
                    "%u/%u frames delivered\n",
                    sess_vec::TICKS, res.delivered, res.n_frames);
        return 0;
    }
    std::printf("RESULT: session FAIL (%d mismatches)\n", fails);
    return 1;
}
