// SEADS predictive INPUT CLIENT (netcode layer 17) — the client half that closes the bidirectional
// loop opened by layer 15b/16.
//
// Layers 5–15a are server->client (a client only reads). Layer 15b/16 opened the UPSTREAM path: a
// client sends tick-stamped INPUT-001 Commands UP and the authoritative sealed kernel steps from them.
// But that alone forces the client to wait a full round-trip to see the result of its own input.
// Layer 17 is the missing half: the client predicts its OWN aircraft LOCALLY from the very commands it
// upstreams, and reconciles against the authoritative frames when they return — so control is instant
// and the correction is provably invisible when the loop is lossless and the input arrived in time.
//
// This is the layer-4b Predictor (predict.h) driven against the layer-15b/16 authoritative input
// server: run_predictive_client PREDICTS the own aircraft forward every tick from the LOCAL command
// timeline (the same commands sent upstream) and, as each authoritative snapshot arrives (under integer
// lag + a deterministic downstream loss set), RECONCILES the own ship — snap to the frame's state,
// drop consumed inputs, replay the rest to "now". Two reconcile SOURCES, mirroring predict_ref:
//   * CANONICAL — snap to the authoritative full-precision own state. A correctly-predicting client is
//     SEAMLESS: predicted == authoritative EVERY tick, the reconcile a zero-correction no-op (the
//     client's local sim IS the server's, offset only by latency). This is the round-trip theorem.
//   * WIRE — snap to the DECODED, lossy protocol-7 own state (the realistic path a real socket
//     delivers). Bounded, not bit-exact: the reconstructed own ship stays within the wire quantum of
//     the authoritative trajectory. Reproducible (the decode is deterministic) ⇒ its per-tick hash
//     sequence is a cross-impl parity digest, exactly like session_ref's.
//
// Boundaries (doctrine, identical to predict/session): net code stays OUTSIDE the kernel. The client
// DRIVES a kernel copy through the public Kernel::add()/step() (via predict::Predictor); decoded bits
// feed the RESEED, never the canonical sim. No kernel/det_math/rails/wire/golden change — this composes
// the EXISTING protocol-7 wire + sealed predictor, riding seal v1.26r0 (a no-seal integration rung, like
// session/event were). Every op is det_math (the predictor's kernel), the reproducible lossy decode, or
// integer transport ⇒ the digest reproduces bit-for-bit across MSVC/GCC/Clang x64/AArch64.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"    // seads::Kernel, Rails, Command, Envelope
#include "predict.h"   // seads::predict::{Predictor, OwnState}
#include "session.h"   // seads::session::{Scenario, ServerFrames, OWN_ID}

namespace seads {
namespace netpredict {

// Which authoritative truth a reconcile snaps the own ship to.
enum class Source {
    CANONICAL,  // full-precision own state (bit-exact / seamless; needs canonical[])
    WIRE,       // the decoded lossy protocol-7 own state (realistic; bounded)
};

struct ClientResult {
    std::vector<std::string> own_tick_hash;  // own(0) KINEMATIC world_hash after each tick (post reconcile)
    std::string digest;                      // SHA-256 over the concatenated hex — cross-impl parity
    unsigned delivered = 0;                  // authoritative frames the client ingested
    unsigned reconciles = 0;                 // reconcile calls performed
    // Judged against a supplied canonical own(0) trajectory (empty => left at defaults):
    bool in_sync = true;                     // predicted == canonical EVERY tick (bit-exact)
    long heal_tick = -1;                     // first tick predicted==canonical onward (1-based; -1 never)
    long first_divergent = -1;               // first tick predicted!=canonical (1-based; -1 never)
    double max_pos_err = 0.0;                // worst |dlat|,|dlon| (radians) vs canonical, any tick
};

// Predict the own aircraft (`own_env`/`start`) forward over `local_cmds` (local_cmds[t-1] is applied on
// the step advancing t-1 -> t; the fire bit is ignored — the own ship's motion never depends on it, and
// its hp/ammo are wire-sourced), reconciling against the authoritative input-server frames `frames`
// (emit_tick, protocol-7 bytes; own aircraft = session::OWN_ID within them) delivered under integer
// `lag` (server tick T reaches the client at tick T+lag) and the `drop_emit_ticks` downstream loss set.
// `src` selects the reconcile truth (CANONICAL requires `canonical` non-empty). When `reconcile` is
// false the own ship free-runs from prediction (a negative control). If `canonical` is non-empty
// (canonical[t] = authoritative own(0) 7-tuple AFTER t ticks, canonical[0] = start), in_sync / heal_tick
// / first_divergent (bit-exact vs canonical) and max_pos_err (bounded error) are filled.
ClientResult run_predictive_client(const Rails& rails, const Envelope* own_env,
    const predict::OwnState& start, const std::vector<Command>& local_cmds,
    const session::ServerFrames& frames, unsigned lag,
    const std::vector<std::int64_t>& drop_emit_ticks, bool reconcile, Source src,
    const std::vector<predict::OwnState>& canonical = {});

// The canonical authoritative own(0) 7-tuple trajectory [0..ticks] (canonical[t] = own state AFTER t
// ticks) for a Scenario, from the own aircraft's OWN kinematic phase schedule on a fresh single-aircraft
// kernel. The own ship's kinematics are independent of the other aircraft absent a hit, so this is the
// reference the wire-reconciled client is judged bounded against and the canonical-reconciled client
// tracks seamlessly. Mirrors predict_ref.run_truth's states, using session::phase_at semantics.
std::vector<predict::OwnState> authoritative_own_states(const Rails& rails,
    const session::Scenario& sc);

}  // namespace netpredict
}  // namespace seads
