// SEADS loopback lockstep harness — bit-for-bit mirror of tools/lockstep_ref.py.
#include "lockstep.h"

#include "../replay/sha256.h"  // seads::sha256_hex

namespace seads {
namespace lockstep {

std::string tick_hash(const Kernel& k, std::uint32_t tick) {
    return sha256_hex(k.snapshot(tick));
}

bool apply_inputs(Kernel& a, Kernel& b,
                  const std::vector<Command>& cmd,
                  const std::vector<const Envelope*>& env,
                  std::uint32_t tick,
                  std::string& ha, std::string& hb) {
    a.step(cmd, env);
    b.step(cmd, env);
    ha = tick_hash(a, tick);
    hb = tick_hash(b, tick);
    return ha == hb;
}

LockstepResult run(Kernel& a, Kernel& b,
                   const std::vector<const Envelope*>& env,
                   const std::vector<std::vector<Command>>& timeline,
                   std::vector<std::string>* per_tick_hashes) {
    LockstepResult r;
    for (std::size_t t = 0; t < timeline.size(); ++t) {
        const std::uint32_t tick = static_cast<std::uint32_t>(t + 1);
        std::string ha, hb;
        bool ok = apply_inputs(a, b, timeline[t], env, tick, ha, hb);
        r.ticks = tick;
        r.hash_a = ha;
        r.hash_b = hb;
        if (per_tick_hashes) per_tick_hashes->push_back(ha);
        if (!ok) {
            r.in_sync = false;
            r.divergent_tick = static_cast<long>(tick);
            break;
        }
    }
    return r;
}

}  // namespace lockstep
}  // namespace seads
