// SEADS client-side prediction parity test (netcode layer 4b). Reproduces the predictor +
// reconcile loop over the scripted scenario in predict_vectors.h and asserts:
//   1) SEAMLESS — a correctly-predicting client reconciles invisibly: predicted == truth
//      every tick (snap+replay reproduces the authoritative trajectory bit-for-bit);
//   2) cross-impl parity — the SHA-256 digest over the whole nominal predicted per-tick hash
//      sequence equals the Python reference (tools/predict_ref.py), plus per-tick checkpoints;
//   3) HEAL — a state-desynced predictor (perturbed initial alt) diverges at tick 1 and is
//      reconciled back EXACTLY at HEAL_TICK, staying in sync thereafter;
//   4) negative control — the same perturbation WITHOUT reconcile stays broken forever (proves
//      the reconcile, not luck, is what heals).
// Exit 0 PASS, 1 FAIL.
#include "predict.h"
#include "predict_vectors.h"

#include "golden_params.h"     // sealed rails scalars (R_M, DT_S, G0, ATM_TOP_M, SOFT_M)
#include "kernel.h"
#include "../replay/sha256.h"

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

// Integer phase select: largest start_tick <= t (mirrors predict_ref.command_at).
static Command command_at(unsigned t) {
    unsigned idx = 0;
    for (unsigned j = 0; j < pred_vec::N_PHASE; ++j) {
        if (pred_vec::SCHED[j].start_tick <= t) idx = j; else break;
    }
    return Command{pred_vec::SCHED[idx].target_phi, pred_vec::SCHED[idx].target_g,
                   pred_vec::SCHED[idx].throttle};
}

static std::string sequence_digest(const std::vector<std::string>& seq) {
    std::vector<std::uint8_t> cat;
    for (const auto& h : seq) cat.insert(cat.end(), h.begin(), h.end());
    return sha256_hex(cat);
}

int main() {
    int fails = 0;
    const Rails rails = sealed_rails();
    const Envelope* env = pred_vec::ENVELOPE;
    const unsigned ticks = pred_vec::TICKS;

    // per-tick command timeline (timeline[t-1] = command for tick t)
    std::vector<Command> timeline;
    timeline.reserve(ticks);
    for (unsigned t = 1; t <= ticks; ++t) timeline.push_back(command_at(t - 1));

    const predict::OwnState start{pred_vec::START.lat, pred_vec::START.lon, pred_vec::START.psi,
                                  pred_vec::START.phi, pred_vec::START.alt, pred_vec::START.tas,
                                  0.0};  // own ship starts level (gamma=0)

    // --- truth (server) run: record canonical states[0..ticks] + per-tick hashes[1..ticks] ---
    std::vector<predict::OwnState> truth_states;
    std::vector<std::string> truth_hashes;
    truth_states.push_back(start);  // states[0] = initial
    {
        Kernel k(rails);
        k.add(start.lat, start.lon, start.psi, start.phi, start.alt, start.tas);
        for (unsigned t = 1; t <= ticks; ++t) {
            std::vector<Command> c{timeline[t - 1]};
            std::vector<const Envelope*> e{env};
            k.step(c, e);
            truth_states.push_back(predict::OwnState{k.lat(0), k.lon(0), k.psi(0),
                                                     k.phi(0), k.alt(0), k.tas(0), k.gamma(0)});
            truth_hashes.push_back(predict::tick_hash(k, t));
        }
    }

    // --- 1+2) nominal: seamless + digest + checkpoints ---
    predict::PredictResult nom = predict::run_prediction(
        rails, env, start, timeline, truth_states, truth_hashes,
        pred_vec::SNAP_EVERY, pred_vec::LAG_TICKS, /*reconcile=*/true);
    if (!nom.in_sync) {
        ++fails;
        std::printf("FAIL nominal not in sync (first divergent tick %ld)\n", nom.first_divergent);
    }
    if (nom.per_tick.size() != ticks) {
        ++fails;
        std::printf("FAIL nominal produced %zu hashes, expected %u\n", nom.per_tick.size(), ticks);
    }
    if (nom.per_tick.size() == ticks) {
        std::string digest = sequence_digest(nom.per_tick);
        if (digest != pred_vec::SEQUENCE_DIGEST) {
            ++fails;
            std::printf("FAIL sequence digest mismatch:\n  got %s\n  exp %s\n",
                        digest.c_str(), pred_vec::SEQUENCE_DIGEST);
        }
        for (int i = 0; i < pred_vec::CHECKPOINT_COUNT; ++i) {
            const auto& c = pred_vec::CHECKPOINTS[i];
            if (c.tick < 1 || c.tick > ticks || nom.per_tick[c.tick - 1] != c.hash) {
                ++fails;
                std::printf("FAIL checkpoint tick %u:\n  got %s\n  exp %s\n", c.tick,
                            c.tick >= 1 && c.tick <= ticks ? nom.per_tick[c.tick - 1].c_str()
                                                           : "<oob>",
                            c.hash);
            }
        }
    }

    // --- 3) heal: perturbed initial alt diverges at tick 1, reconciles back at HEAL_TICK ---
    predict::OwnState perturbed = start;
    perturbed.alt = start.alt + pred_vec::PERTURB_ALT;
    predict::PredictResult heal = predict::run_prediction(
        rails, env, perturbed, timeline, truth_states, truth_hashes,
        pred_vec::SNAP_EVERY, pred_vec::LAG_TICKS, /*reconcile=*/true);
    if (heal.first_divergent != 1) {
        ++fails;
        std::printf("FAIL perturbed predictor should diverge at tick 1, got %ld\n",
                    heal.first_divergent);
    }
    if (heal.heal_tick != static_cast<long>(pred_vec::HEAL_TICK)) {
        ++fails;
        std::printf("FAIL perturbed predictor should heal at tick %u, got %ld\n",
                    pred_vec::HEAL_TICK, heal.heal_tick);
    }

    // --- 4) negative control: same perturbation, NO reconcile -> stays broken ---
    predict::PredictResult broken = predict::run_prediction(
        rails, env, perturbed, timeline, truth_states, truth_hashes,
        pred_vec::SNAP_EVERY, pred_vec::LAG_TICKS, /*reconcile=*/false);
    if (broken.in_sync) {
        ++fails;
        std::printf("FAIL no-reconcile control stayed in sync (reconcile not load-bearing)\n");
    }

    if (fails == 0) {
        std::printf("PASS: prediction seamless %u ticks; digest matches reference; "
                    "perturbed heals at tick %u; tripwire real\n", ticks, pred_vec::HEAL_TICK);
        return 0;
    }
    std::printf("RESULT: prediction FAIL (%d mismatches)\n", fails);
    return 1;
}
