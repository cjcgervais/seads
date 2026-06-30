// SEADS scenario runner — reproduces the scripted-timeline goldens (Turn/Climb/TurnClimb).
// Inputs come from the generated scenario_params.h + envelope_tables.h (bit-identical to
// tools/ref_kernel.py --scenario). The straight Sphere golden has its own runner (golden_main.cpp).
//
// Usage:
//   seads_scenario --id GOLDEN-SK-Turn-001 [--out run.bin]
// Prints: world_hash=<sha256 hex>
#include "kernel.h"
#include "golden_params.h"      // shared rails scalars (R_M, DT_S, G0, ATM_TOP_M, SOFT_M)
#include "scenario_params.h"    // scen::ALL, Scenario/AcSpec/Phase
#include "../replay/sha256.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace seads;

int main(int argc, char** argv) {
    const char* id = nullptr;
    const char* out_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--id") == 0 && i + 1 < argc) id = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_path = argv[++i];
    }
    if (!id) { std::fprintf(stderr, "usage: seads_scenario --id <GOLDEN-SK-...> [--out f]\n"); return 2; }

    const scen::Scenario* S = nullptr;
    for (unsigned k = 0; k < scen::N_ALL; ++k) {
        if (std::strcmp(scen::ALL[k]->id, id) == 0) { S = scen::ALL[k]; break; }
    }
    if (!S) { std::fprintf(stderr, "unknown scenario id: %s\n", id); return 2; }

    Rails rails;
    rails.R = golden::R_M;
    rails.dt = golden::DT_S;
    rails.g0 = golden::G0;
    rails.atm_top = golden::ATM_TOP_M;
    rails.soft = golden::SOFT_M;

    Kernel k(rails);
    for (unsigned a = 0; a < S->n_ac; ++a) {
        const scen::AcSpec& ac = S->ac[a];
        // AcSpec angles are already radians (pre-converted at generation time). G3 (v1.11r0): seed
        // the aircraft with its per-airframe starting hitpoints (gamma defaults to 0).
        k.add(ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, 0.0, ac.env->hp_start);
    }

    std::vector<Command> cmd(S->n_ac);
    std::vector<const Envelope*> env(S->n_ac);
    for (unsigned a = 0; a < S->n_ac; ++a) env[a] = S->ac[a].env;

    for (unsigned t = 0; t < S->ticks; ++t) {
        for (unsigned a = 0; a < S->n_ac; ++a) {
            const scen::AcSpec& ac = S->ac[a];
            unsigned idx = 0;                               // integer phase select: largest start_tick <= t
            for (unsigned j = 0; j < ac.n_phase; ++j) {
                if (ac.sched[j].start_tick <= t) idx = j; else break;
            }
            cmd[a].target_phi = ac.sched[idx].target_phi;
            cmd[a].target_g = ac.sched[idx].target_g;
            cmd[a].throttle = ac.sched[idx].throttle;
            cmd[a].fire = (ac.sched[idx].fire != 0u);   // G1 (v1.9r0) gun trigger
        }
        k.step(cmd, env);
    }

    std::vector<std::uint8_t> snap = k.snapshot(S->ticks);
    std::string wh = sha256_hex(snap);

    std::printf("scenario=%s ticks=%u\n", S->id, S->ticks);
    std::printf("final lat=%.17g lon=%.17g psi=%.17g alt=%.17g\n",
                k.lat(0), k.lon(0), k.psi(0), k.alt(0));
    std::printf("projectiles=%zu\n", k.proj_count());     // G1 (v1.9r0): live rounds in the snapshot
    std::printf("world_hash=%s\n", wh.c_str());

    if (out_path) {
        std::FILE* f = std::fopen(out_path, "wb");
        if (!f) { std::fprintf(stderr, "cannot open %s\n", out_path); return 2; }
        std::fwrite(snap.data(), 1, snap.size(), f);
        std::fclose(f);
        std::printf("wrote snapshot -> %s\n", out_path);
    }
    return 0;
}
