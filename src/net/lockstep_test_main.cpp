// SEADS loopback lockstep parity test (netcode layer 3). Drives two C++ kernels through the
// scripted timeline in lockstep_vectors.h and asserts:
//   1) in-sync — both kernels produce an identical per-tick world_hash for every tick (the
//      desync tripwire holds);
//   2) cross-impl parity — the SHA-256 digest over the whole per-tick hash sequence equals the
//      Python reference (tools/lockstep_ref.py), i.e. every tick's canonical snapshot is
//      bit-identical C++ <-> reference; plus per-tick checkpoints for readable localization;
//   3) negative control — a 1-part-in-a-million altitude desync in one kernel TRIPS the
//      tripwire (proves the comparator is real, not a no-op).
// Exit 0 PASS, 1 FAIL.
#include "lockstep.h"
#include "lockstep_vectors.h"

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

// Build a kernel from the scenario; optionally perturb aircraft `pidx` altitude by `dalt`.
static Kernel build_kernel(int pidx = -1, double dalt = 0.0) {
    Kernel k(sealed_rails());
    for (unsigned a = 0; a < ls_vec::N_AIRCRAFT; ++a) {
        const auto& ac = ls_vec::AIRCRAFT[a];
        double alt = ac.alt + (static_cast<int>(a) == pidx ? dalt : 0.0);
        k.add(ac.lat, ac.lon, ac.psi, ac.phi, alt, ac.tas);
    }
    return k;
}

// Expand the per-aircraft phase schedules into a per-tick command timeline (integer phase
// select: largest start_tick <= t — mirrors scenario_main.cpp / ref_kernel.run_scenario).
static std::vector<std::vector<Command>> build_timeline() {
    std::vector<std::vector<Command>> tl(ls_vec::TICKS,
                                         std::vector<Command>(ls_vec::N_AIRCRAFT));
    for (unsigned t = 0; t < ls_vec::TICKS; ++t) {
        for (unsigned a = 0; a < ls_vec::N_AIRCRAFT; ++a) {
            const auto& ac = ls_vec::AIRCRAFT[a];
            unsigned idx = 0;
            for (unsigned j = 0; j < ac.n_phase; ++j) {
                if (ac.sched[j].start_tick <= t) idx = j; else break;
            }
            tl[t][a] = Command{ac.sched[idx].target_phi, ac.sched[idx].target_g,
                               ac.sched[idx].throttle};
        }
    }
    return tl;
}

static std::string sequence_digest(const std::vector<std::string>& seq) {
    std::vector<std::uint8_t> cat;
    for (const auto& h : seq) cat.insert(cat.end(), h.begin(), h.end());
    return sha256_hex(cat);
}

int main() {
    int fails = 0;

    std::vector<std::vector<Command>> timeline = build_timeline();
    std::vector<const Envelope*> env(ls_vec::N_AIRCRAFT);
    for (unsigned a = 0; a < ls_vec::N_AIRCRAFT; ++a)
        env[a] = ls_vec::ENVELOPES[ls_vec::AIRCRAFT[a].env_idx];

    // --- 1) two identical kernels stay in sync every tick ---
    Kernel a = build_kernel(), b = build_kernel();
    std::vector<std::string> seq;
    lockstep::LockstepResult r = lockstep::run(a, b, env, timeline, &seq);
    if (!r.in_sync) {
        ++fails;
        std::printf("FAIL lockstep diverged at tick %ld:\n  A=%s\n  B=%s\n",
                    r.divergent_tick, r.hash_a.c_str(), r.hash_b.c_str());
    }
    if (r.ticks != ls_vec::TICKS || seq.size() != ls_vec::TICKS) {
        ++fails;
        std::printf("FAIL lockstep ran %u ticks (%zu hashes), expected %u\n",
                    r.ticks, seq.size(), ls_vec::TICKS);
    }

    // --- 2a) whole-sequence digest matches the Python reference (exhaustive cross-impl) ---
    if (seq.size() == ls_vec::TICKS) {
        std::string digest = sequence_digest(seq);
        if (digest != ls_vec::SEQUENCE_DIGEST) {
            ++fails;
            std::printf("FAIL sequence digest mismatch:\n  got %s\n  exp %s\n",
                        digest.c_str(), ls_vec::SEQUENCE_DIGEST);
        }
    }

    // --- 2b) per-tick checkpoints match (readable divergence localization) ---
    for (int i = 0; i < ls_vec::CHECKPOINT_COUNT && seq.size() == ls_vec::TICKS; ++i) {
        const auto& c = ls_vec::CHECKPOINTS[i];
        if (c.tick < 1 || c.tick > ls_vec::TICKS || seq[c.tick - 1] != c.hash) {
            ++fails;
            std::printf("FAIL checkpoint tick %u:\n  got %s\n  exp %s\n",
                        c.tick, c.tick >= 1 && c.tick <= ls_vec::TICKS
                                    ? seq[c.tick - 1].c_str() : "<oob>",
                        c.hash);
        }
    }

    // --- 3) negative control: a tiny altitude desync MUST trip the tripwire ---
    Kernel a2 = build_kernel(), b2 = build_kernel(2, 0x1p-20);  // ~9.5e-7 m on AC2
    lockstep::LockstepResult rn = lockstep::run(a2, b2, env, timeline, nullptr);
    if (rn.in_sync) {
        ++fails;
        std::printf("FAIL negative control did not trip (perturbed kernel stayed in sync)\n");
    } else if (rn.divergent_tick != 1) {
        ++fails;
        std::printf("FAIL negative control tripped at tick %ld, expected 1\n", rn.divergent_tick);
    }

    if (fails == 0) {
        std::printf("PASS: lockstep in sync %u ticks x %u aircraft; digest matches reference; "
                    "tripwire trips on desync\n", ls_vec::TICKS, ls_vec::N_AIRCRAFT);
        return 0;
    }
    std::printf("RESULT: lockstep FAIL (%d mismatches)\n", fails);
    return 1;
}
