// SEADS canonical tick-stamped Command queue — the ORDERING CONTRACT of netcode layer 15b.
//
// The determinism rail (CLAUDE.md §1) says the kernel is bit-for-bit reproducible. It stays that way
// under CLIENT input because the canonical thing is the tick-stamped command SET, never the order the
// bytes arrived over a lossy/reordering network. This queue is where that canonicalisation happens.
//
// CONTRACT (drop + hold-last, the policy chosen for layer 15b):
//   * Each command carries an apply_tick (the PRE-step tick it governs) and an aircraft index.
//   * DROP-AT-INGEST: a command whose apply_tick is BELOW the queue's floor — the next tick still to
//     be stepped — is already in the past and is rejected the instant it arrives (STALE). Since the
//     floor only advances as the authoritative producer consumes ticks, "past" is decided by the
//     producer's progress, not by wall-clock, and a rejected command is IDENTICAL to one that never
//     arrived. (An out-of-range aircraft index is likewise rejected: OUT_OF_RANGE.)
//   * CANONICAL SELECTION: at most one command applies per (apply_tick, aircraft). When several share
//     that key the winner is the one MAXIMAL under a total order on the wire-relevant fields
//     (seq first — the client's monotone tag — then the quantized phi/g/throttle/fire). Because the
//     order is over the commands' OWN bytes, the winner is a pure function of the SET: submit them in
//     any order / any chunking and the same command wins. (Clients SHOULD keep seq unique per key; the
//     field tiebreak only makes a duplicate-seq collision deterministic rather than arrival-ordered.)
//   * HOLD-LAST lives in the producer (see inputserver.h): a tick with no command for an aircraft
//     reuses that aircraft's last applied command. The queue itself only answers "is there a command
//     for exactly (tick, aircraft)?".
//
// The guarantee this buys (proved by seads_netinput_test / test_input001.py): given commands that
// arrive before their apply_tick is stepped (adequate lead), the authoritative kernel's output is
// invariant to upstream ORDER and CHUNKING — the input-direction analogue of the downstream layers'
// "lossy != nondeterministic". Late arrival is a separate, deterministic-given-progress DROP.
//
// Pure integer / std::map work — no det_math, no float ops on the sim path (the quantize used for the
// tiebreak is the same lossy wire quantize, never fed to the kernel). Single-threaded by design: the
// layer-15b server submits (from socket reads) and consumes (from the producer) on one thread.
#pragma once
#include <cstdint>
#include <map>
#include <utility>

#include "input001.h"

namespace seads {
namespace netinput {

class CommandQueue {
public:
    enum class Result { ACCEPTED, STALE, OUT_OF_RANGE };

    explicit CommandQueue(int64_t n_aircraft) : n_(n_aircraft) {}

    // Ingest one command. Rejected (no state change) if its aircraft is out of [0,n) or its
    // apply_tick is below the current floor. Otherwise it is stored as the winner for its
    // (apply_tick, aircraft) key iff it is maximal there under the canonical field order.
    Result submit(const input001::InputCommand& c);

    // Is there a command for exactly (tick, aircraft)? If so copy the winner into `out`.
    bool peek(int64_t tick, int64_t aircraft, input001::InputCommand& out) const;

    // Consume tick `tick`: drop every entry stamped for it and advance the floor to tick+1, so any
    // later submit for <= tick is STALE. Called by the producer once it has read this tick's winners.
    void consume(int64_t tick);

    int64_t floor() const { return floor_; }
    std::size_t buffered() const { return pend_.size(); }  // for tests/stats

private:
    int64_t n_;
    int64_t floor_ = 0;
    std::map<std::pair<int64_t, int64_t>, input001::InputCommand> pend_;  // (tick,aircraft) -> winner
};

}  // namespace netinput
}  // namespace seads
