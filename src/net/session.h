// SEADS server<->client SESSION loop (netcode layer 5). Mirrors tools/session_ref.py.
//
// The layer that finally USES the wire transport between two endpoints. A SERVER drives the
// authoritative sealed kernel over a scripted multi-aircraft dogfight and, at the 20 Hz snapshot
// cadence, serializes the FULL protocol-4 world into a WEAPON-001 wire frame. An in-process
// TRANSPORT ships those frames server->client with a fixed integer latency + deterministic packet
// loss. The CLIENT reconstructs the whole dogfight from the bytes it receives, composing the four
// earlier netcode layers:
//   * OWN ship (id 0): PREDICTED @ now from the local input timeline (layer 4b, seads_predict),
//     reconciled against each decoded wire frame (the realistic LOSSY-decode reseed path).
//   * REMOTE ships: INTERPOLATED ~150 ms in the past (layer 4a) from the decoded GEO frames.
//   * HP / KILLS / tracer ROUNDS: read from the decoded WEAPON section of the freshest delivered
//     frame (nearest-frame semantics — hp is discrete, rounds transient, so no interpolation).
//
// The reconstructed per-tick CLIENT VIEW is serialized to canonical bytes (all fields through the
// SAME GEO-001 integer quantize) and hashed; the whole-session SHA-256 digest is the cross-impl
// parity artifact this C++ mirror must reproduce BIT-FOR-BIT vs the Python reference. Why it is
// bit-exact even though the transport is LOSSY: loss/quantization destroy INFORMATION but are
// perfectly REPRODUCIBLE, and every reconstruction op is det_math (the predictor's kernel), pure
// IEEE +-*/ (the interp sampler), or integer (quantize/transport) — all reproduce across
// MSVC/GCC/Clang x64/AArch64. See session_ref.py for the full rationale.
//
// Boundaries (doctrine): net code stays OUTSIDE the kernel. The server DRIVES the kernel (like
// lockstep/predict) so this is its OWN lib (seads_session, links seads_predict + seads_net +
// kernel + replay), not folded into the pure wire lib. Decoded bits feed the client view + the
// predictor RESEED, never the canonical sim. No rail/golden/wire change — it composes the EXISTING
// protocol-4 wire, riding seal v1.12r0 (a Tier-2 net layer, like interp was).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"    // seads::Kernel, Rails, Command, Envelope (kernel include dir)
#include "snapshot.h"  // seads::netsnap (wire frames)
#include "interp.h"    // seads::interp::SnapshotBuffer (layer 4a)
#include "predict.h"   // seads::predict::Predictor (layer 4b)

namespace seads {
namespace session {

constexpr int64_t OWN_ID = 0;  // the local player's aircraft (predicted, not interpolated)

// One command-timeline phase (integer phase select, largest start_tick <= t). Mirrors the
// config/scenarios/*.json schedule shape used by ref_kernel.run_scenario.
struct Phase {
    unsigned start_tick;
    double target_phi;  // radians (pre-converted)
    double target_g;
    double throttle;
    bool fire;
};

// One aircraft in the session scenario: envelope, start state (radians/metres/m s^-1), schedule.
struct AircraftSpec {
    const Envelope* env;
    double lat, lon, psi, phi, alt, tas;
    const Phase* sched;
    unsigned n_phase;
};

// The SESSION-SK-001 scenario (server-authoritative): aircraft + snapshot cadence + transport
// latency + render delay + deterministic packet loss (emitted frames at these server ticks are
// never delivered).
struct Scenario {
    const AircraftSpec* aircraft;
    unsigned n_aircraft;
    unsigned ticks;
    unsigned snap_every;
    unsigned lag_ticks;
    unsigned render_delay;
    const int64_t* drop_emit_ticks;
    unsigned n_drops;
};

struct SessionResult {
    std::vector<std::string> per_tick;   // per-tick reconstructed client-view hash
    std::string digest;                  // SHA-256 over the concatenated per-tick hex (parity)
    unsigned delivered = 0;              // frames the client received
    unsigned n_frames = 0;               // frames the server emitted
    bool has_final_wframe = false;       // did the client ever receive a frame?
    netsnap::Snapshot final_wframe;      // freshest frame at the last tick (replicated weapon truth)
};

// Run the full server->transport->client session. When reconcile=false the client never corrects
// its predicted own ship against the wire (a negative control for the property tests).
SessionResult run_session(const Rails& rails, const Scenario& sc, bool reconcile);

}  // namespace session
}  // namespace seads
