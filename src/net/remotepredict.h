// SEADS REMOTE-AIRCRAFT PREDICTION (netcode layer 24) — bit-for-bit mirror of
// tools/remotepredict_ref.py. Predict a REMOTE aircraft to "now" by dead-reckoning coast.
//
// The prediction story so far has two halves. Layer 4b/17 predict the OWN aircraft by replaying the
// LOCAL command timeline the client upstreams (control is instant; over a lossless loop the
// correction is invisible — the round-trip theorem). Layer 4a renders REMOTE aircraft by
// INTERPOLATION, ~100 ms in the PAST between the two freshest received snapshots (smooth, but
// structurally LATE — a remote is drawn where it WAS). Layer 24 closes that gap: predict the remote
// to NOW too.
//
// A client does NOT have a remote's input commands (authorization: it only ever sees its OWN seat's
// commands — every bidirectional layer 15b-23 enforces this). All it has is the remote's KINEMATIC
// STATE on the wire (lat/lon/psi/phi/alt/tas/gamma, the GEO-001 + KIN-002 sections). So the honest
// model is DEAD-RECKONING COAST: seed a one-aircraft kernel from the remote's freshest authoritative
// snapshot and advance it with the SEALED NO-ARG Kernel::step() — the "pure kinematic tail" that
// holds bank / flight-path angle / speed and propagates the great circle (the Sphere-golden tail).
// When a fresher snapshot arrives (under integer lag + a downstream loss set), RESEED to it and
// re-extrapolate forward to now. No input replay — a remote has no local inputs.
//
// Honest bound: dead-reckoning assumes the last kinematics HOLD, so it is NOT the authoritative
// dynamics (which integrate the remote's real commands). It TRACKS "now" far tighter than
// interpolation's structural render-lag when the remote flies quasi-steadily, and stays BOUNDED
// across a maneuver BECAUSE of the reconcile (a no-reconcile control drifts without bound — the
// remote analogue of layer 17's heal). Never bit-exact — there is no round-trip theorem for a
// remote the client never had inputs for. Two reconcile SOURCES (like inputpredict): CANONICAL
// (full-precision reseed) and WIRE (decoded lossy reseed); each yields a reproducible parity digest.
//
// Boundaries (doctrine, identical to predict/inputclient): net code stays OUTSIDE the kernel. The
// client DRIVES a kernel copy through the public no-arg Kernel::step(); decoded bits feed the
// RESEED, never the canonical sim. No kernel/det_math/rails/wire/golden change — composes the
// EXISTING protocol-7 wire + the sealed no-arg kinematic tail, riding seal v1.26r0 (a no-seal
// integration rung). Every op is det_math (the no-arg tail), the reproducible lossy decode, or
// integer transport ⇒ the digest reproduces bit-for-bit across MSVC/GCC/Clang x64/AArch64.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"    // seads::Kernel, Rails
#include "predict.h"   // seads::predict::OwnState (the 7-tuple), tick_hash
#include "session.h"   // seads::session::ServerFrames

namespace seads {
namespace netremote {

// Which authoritative truth a reseed snaps the coasted remote to.
enum class Source {
    CANONICAL,  // full-precision authoritative 7-tuple (lossless downstream)
    WIRE,       // the decoded lossy protocol-7 remote 7-tuple (realistic; bounded)
};

struct RemoteResult {
    std::vector<std::string> per_tick;  // coasted-remote KINEMATIC world_hash after each tick
    std::string digest;                 // SHA-256 over the concatenated hex — cross-impl parity
    unsigned delivered = 0;             // authoritative frames reseeded from
    double max_pos_err = 0.0;           // worst |dlat|,|dlon| (radians) vs the authoritative "now"
};

// Layer 25 — the SMOOTHED display result (run_remote_client_smoothed).
struct SmoothResult {
    std::vector<std::string> per_tick;  // DISPLAYED (blended) remote world_hash after each tick
    std::string digest;                 // SHA-256 over the concatenated hex — cross-impl parity
    unsigned delivered = 0;             // authoritative frames reseeded from
    double max_pos_err = 0.0;           // worst |dlat|,|dlon| (radians) vs "now" — the traded lag
    double max_jump = 0.0;              // worst single-tick |Δstate| across the 7-tuple — the pop
};

// Predict the remote (id `remote_id` within the wire frames) to "now" each tick by dead-reckoning
// the freshest delivered authoritative snapshot. `states[t]` is the authoritative 7-tuple AFTER t
// ticks (states[0] = spawn). Each tick t: if the frame for server_tick st = t - lag is delivered
// (an emit tick present in `frames`, not in `drop_emit_ticks`), reseed to that authoritative state
// (CANONICAL = states[st]; WIRE = the decoded remote 7-tuple) and coast forward `lag` kinematic
// ticks to now; otherwise advance the running estimate one more no-arg tick. When `reconcile` is
// false the coast free-runs from the spawn state (a negative control that drifts without bound).
RemoteResult run_remote_client(const Rails& rails,
                               const std::vector<predict::OwnState>& states,
                               const session::ServerFrames& frames, unsigned lag,
                               const std::vector<std::int64_t>& drop_emit_ticks, bool reconcile,
                               Source src, std::int64_t remote_id = 0);

// Layer 25 — the same dead-reckoning coast, but the DISPLAYED remote is BLENDED toward each reseed
// target instead of SNAPPED: geometric error-decay smoothing that hides the maneuver-correction pop.
// `smooth` in (0,1]: 1.0 == run_remote_client (hard snap, a degenerate identity — smoothed(1) hashes
// the same coast state each tick); smaller == smoother + laggier. The coast (`coaster`) is stepped /
// reseeded byte-identically to run_remote_client; only the rendered 7-tuple is blended (net code
// stays OUTSIDE the kernel). The blend is pure IEEE sub/mul/add (no transcendental, no FMA under
// -ffp-contract=off) ⇒ the displayed remote's per-tick hash sequence is a cross-impl parity digest.
SmoothResult run_remote_client_smoothed(const Rails& rails,
                                        const std::vector<predict::OwnState>& states,
                                        const session::ServerFrames& frames, unsigned lag,
                                        const std::vector<std::int64_t>& drop_emit_ticks,
                                        bool reconcile, Source src, double smooth,
                                        std::int64_t remote_id = 0);

// The coast's "now" position error (worst |dlat|,|dlon| radians vs authoritative states[t]) over
// the tick window [lo, hi] — the metric compared against interp_now_error to show the coast removes
// interpolation's render lag. Reseeds every delivered frame (no loss), source-selectable.
double coast_now_error(const Rails& rails, const std::vector<predict::OwnState>& states,
                       const session::ServerFrames& frames, unsigned lag, unsigned lo, unsigned hi,
                       Source src, std::int64_t remote_id = 0);

// The layer-4a INTERPOLATION baseline's "now" position error over [lo, hi]: a SnapshotBuffer fed
// each delivered frame, sampled at render_tick = t - render_delay (server_tick units), compared to
// the authoritative state AT tick t. This is the structural render-lag the coast removes.
double interp_now_error(const std::vector<predict::OwnState>& states,
                        const session::ServerFrames& frames, unsigned lag, unsigned render_delay,
                        unsigned lo, unsigned hi,
                        const std::vector<std::int64_t>& drop_emit_ticks, std::int64_t remote_id = 0);

}  // namespace netremote
}  // namespace seads
