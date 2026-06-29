// SEADS client-side prediction harness — bit-for-bit mirror of tools/predict_ref.py.
#include "predict.h"

#include "../replay/sha256.h"  // seads::sha256_hex

namespace seads {
namespace predict {

std::string tick_hash(const Kernel& k, std::uint32_t tick) {
    return sha256_hex(k.snapshot(tick));
}

// Build a throwaway 1-aircraft kernel in state `s` and hash its snapshot after `tick` ticks.
static Kernel kernel_with(const Rails& rails, const OwnState& s) {
    Kernel k(rails);
    k.add(s.lat, s.lon, s.psi, s.phi, s.alt, s.tas);
    return k;
}

std::string hash_state(const Rails& rails, const OwnState& s, std::uint32_t tick) {
    return tick_hash(kernel_with(rails, s), tick);
}

Predictor::Predictor(const Rails& rails, const Envelope* env, const OwnState& start)
    : rails_(rails), env_(env), k_(rails) {
    k_.add(start.lat, start.lon, start.psi, start.phi, start.alt, start.tas);
}

void Predictor::predict(std::uint32_t tick, const Command& cmd) {
    std::vector<Command> c{cmd};
    std::vector<const Envelope*> e{env_};
    k_.step(c, e);
    buffer_.emplace_back(tick, cmd);
}

void Predictor::reconcile(std::uint32_t server_tick, const OwnState& auth) {
    // Re-seed to the authoritative state, then replay buffered inputs newer than server_tick.
    // Rebuild via the public Kernel::add() — no access to kernel internals.
    Kernel fresh = kernel_with(rails_, auth);
    std::vector<std::pair<std::uint32_t, Command>> remaining;
    remaining.reserve(buffer_.size());
    for (const auto& tc : buffer_)
        if (tc.first > server_tick) remaining.push_back(tc);
    std::vector<const Envelope*> e{env_};
    for (const auto& tc : remaining) {
        std::vector<Command> c{tc.second};
        fresh.step(c, e);
    }
    k_ = std::move(fresh);
    buffer_ = std::move(remaining);
}

PredictResult run_prediction(const Rails& rails, const Envelope* env,
                             const OwnState& start,
                             const std::vector<Command>& timeline,
                             const std::vector<OwnState>& truth_states,
                             const std::vector<std::string>& truth_hashes,
                             unsigned snap_every, unsigned lag, bool reconcile) {
    PredictResult r;
    Predictor p(rails, env, start);
    const std::uint32_t ticks = static_cast<std::uint32_t>(timeline.size());
    for (std::uint32_t t = 1; t <= ticks; ++t) {
        p.predict(t, timeline[t - 1]);
        if (reconcile && (t % snap_every == 0) && (t > lag)) {
            std::uint32_t server_tick = t - lag;
            p.reconcile(server_tick, truth_states[server_tick]);
        }
        std::string h = tick_hash(p.kernel(), t);
        r.per_tick.push_back(h);
        if (h != truth_hashes[t - 1]) {
            r.in_sync = false;
            if (r.first_divergent < 0) r.first_divergent = static_cast<long>(t);
            r.heal_tick = -1;
        } else if (r.heal_tick < 0) {
            r.heal_tick = static_cast<long>(t);
        }
    }
    return r;
}

}  // namespace predict
}  // namespace seads
