// SEADS client-side prediction harness (netcode layer 4b). Mirrors tools/predict_ref.py.
//
// Predict your OWN aircraft forward from local input each tick, and reconcile against
// authoritative snapshots: snap the own aircraft to the authoritative state at the snapshot's
// tick, then REPLAY the buffered local inputs from that base forward to "now". A correctly-
// predicting client reconciles SEAMLESSLY (snap+replay reproduces the authoritative trajectory
// bit-for-bit); a client whose state has drifted is HEALED exactly at the next snapshot.
//
// Boundaries (doctrine — identical posture to lockstep):
//   * Net code stays OUTSIDE the kernel. The buffer carries sim Commands (target bank / climb),
//     NEVER wire bits. The bit-exact path reconciles against the CANONICAL authoritative state
//     (the raw f64 6-tuple), NOT the lossy GEO-001/KIN wire — consistent with lockstep comparing
//     the canonical snapshot. The lossy-wire reseed path (real remote / late-join) is bounded,
//     not exact (proven in tests/property/test_predict.py).
//   * Reconcile re-seeds by REBUILDING a kernel through the existing public Kernel::add() + step()
//     — the kernel's deterministic math and the canonical snapshot byte layout are UNTOUCHED.
//   * This harness DRIVES the kernel, so it links seads_kernel + seads_replay and is its OWN
//     library (seads_predict), kept separate from the pure wire codec lib (seads_net).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"  // seads::Kernel, Rails, Command, Envelope (from the kernel include dir)

namespace seads {
namespace predict {

// The canonical full-precision own-aircraft state a reconcile re-seeds from.
struct OwnState {
    double lat, lon, psi, phi, alt, tas;
};

// Canonical per-tick world_hash: SHA-256 of Kernel::snapshot(tick) (raw LE-f64, sealed layout).
std::string tick_hash(const Kernel& k, std::uint32_t tick);

// SHA-256 of the canonical 1-aircraft snapshot for `s` after `tick` ticks (no kernel stepping)
// — used to hash a recorded authoritative truth state for comparison.
std::string hash_state(const Rails& rails, const OwnState& s, std::uint32_t tick);

// A client predicting its OWN aircraft: a kernel copy + a ring buffer of the local
// (tick, command) inputs it has applied, so a reconcile can snap to an authoritative past
// state and replay the inputs since.
class Predictor {
public:
    Predictor(const Rails& rails, const Envelope* env, const OwnState& start);

    // Advance the local kernel one tick from local input, and remember the input.
    void predict(std::uint32_t tick, const Command& cmd);

    // Authoritative snapshot for `server_tick` arrived: snap the own aircraft to it, drop inputs
    // at/older than server_tick, and REPLAY the rest forward to re-derive "now". Re-seed is via
    // a fresh Kernel built through Kernel::add() (no kernel-internals access).
    void reconcile(std::uint32_t server_tick, const OwnState& auth);

    const Kernel& kernel() const { return k_; }

private:
    Rails rails_;
    const Envelope* env_;
    Kernel k_;
    std::vector<std::pair<std::uint32_t, Command>> buffer_;  // oldest first
};

struct PredictResult {
    std::vector<std::string> per_tick;  // predicted world_hash after each tick
    bool in_sync = true;                // predicted == truth EVERY tick
    long heal_tick = -1;                // first tick predicted==truth onward (1-based), -1 never
    long first_divergent = -1;          // first tick predicted != truth (1-based), -1 never
};

// Drive a Predictor over `timeline` (timeline[t-1] = command for tick t). Each tick: predict;
// then on a snapshot tick whose snapshot has had time to arrive (tick > lag) reconcile against
// truth_states[tick - lag]. Compares each predicted hash to truth_hashes[tick-1].
//   truth_states  : [0..ticks], truth_states[t] = authoritative state AFTER t ticks
//   truth_hashes  : [0..ticks-1], truth_hashes[t-1] = authoritative world_hash after t ticks
PredictResult run_prediction(const Rails& rails, const Envelope* env,
                             const OwnState& start,
                             const std::vector<Command>& timeline,
                             const std::vector<OwnState>& truth_states,
                             const std::vector<std::string>& truth_hashes,
                             unsigned snap_every, unsigned lag, bool reconcile);

}  // namespace predict
}  // namespace seads
