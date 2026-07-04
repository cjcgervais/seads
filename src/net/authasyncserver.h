// SEADS authoritative AUTHENTICATED + ASYNC server (netcode LAYER 22) — identity binding merged with the
// layer-16/19 downstream OUTPUT HYGIENE. The last two axes of the bidirectional arc, composed.
//
// Layer 21 (broadcast_auth, authserver.h) made the client->aircraft binding a function of the client's
// IDENTITY: a joining client sends a HELLO-001 credential FIRST, the server looks the token up in a
// pre-shared CredentialTable to a DESIGNATED seat (invariant to join order; unknown token -> spectator),
// replies with a BIND-001 record, and AUTHORIZES upstream commands (own seat only). But that first
// authenticated cut — like layer 18 before it — kept broadcast_input's BLOCKING send_all downstream, so
// ONE slow client back-pressures the whole broadcast and a silently-dead peer that never sends EOF is
// never shed. Layer 19 (broadcast_bound_async, boundasyncserver.h) already brought the layer-16 async
// hygiene (non-blocking per-client send buffers + byte-cap drop-slowest + liveness reap) to the JOIN-ORDER
// bound server. Authentication (admission) and hygiene (delivery) are ORTHOGONAL axes — exactly as layer
// 19 (bound + async) composed layer 18's binding with layer 16's hygiene, layer 22 composes layer 21's
// IDENTITY binding with the same hygiene. It is layer 19 with the join-order SeatPolicy replaced by the
// layer-21 CredentialTable, or equivalently layer 21 with the blocking send_all replaced by the async loop.
//
// broadcast_auth_async is broadcast_bound_async's async loop (boundasyncserver.cpp) with broadcast_auth's
// identity handshake folded in (authserver.cpp) — a SIBLING of both, so the sealed broadcast.cpp AND
// broadcast_bound / broadcast_bidi / broadcast_bound_async / broadcast_auth are byte-for-byte UNTOUCHED
// (this file owns its own AuthAsyncClient + flush/enqueue/cap/reap/drop helpers; it is transport, outside
// the world_hash — no det_math, no seal, rides v1.26r0; NO shared-file or Stats change — layers 16/18
// already added capped/reaped/cmds_unauth, and HELLO-001 + CredentialTable + seat_authorizes are reused
// VERBATIM from layer 21). The three properties it composes, all pure TRANSPORT:
//   1. IDENTITY SEAT (layer 21) — on accept the server reads the client's FIRST upstream framing frame, a
//      HELLO-001 credential; CredentialTable::authenticate(token) resolves it to its DESIGNATED seat (free)
//      or bind001::SPECTATOR (unknown token or double-login). The seat is a function of the ROSTER,
//      invariant to join order. A leaver — clean EOF, fatal flush, byte-cap shed, OR liveness reap —
//      release()s its seat, so a reconnecting identity reclaims ITS OWN seat.
//   2. BIND + AUTHORIZATION (layers 18/21) — the resolved seat's one-time BIND-001 record is ENQUEUED as
//      the FIRST bytes of the client's downstream send buffer (before any snapshot), and a decoded command
//      is submitted to the CommandQueue ONLY when it names the client's own seat (seat_authorizes); a
//      foreign-aircraft command or any spectator command is DROPPED (Stats.cmds_unauth), byte-identical to
//      never arriving. Unlike layer 21's blocking send_all, here the BIND rides the same non-blocking send
//      buffer as every frame — it is simply enqueued first, so the async flush delivers it first.
//   3. ASYNC HYGIENE (layers 16/19) — non-blocking per-client send buffers (no client back-pressures the
//      loop) + opt-in byte-cap drop-slowest (cap_bytes) + liveness reap of a stalled peer (liveness_frames).
//
// DETERMINISM (the whole point, and why the merge is safe): authentication is an admission FILTER on the
// upstream (which identity holds which seat, and per-seat authorization) plus downstream bookkeeping (the
// BIND); the hygiene is purely the DOWNSTREAM transport (WHEN each client's bytes move, WHICH slow/dead
// clients are shed). Neither touches the CommandQueue's canonical selection. So the authoritative kernel's
// frames stay a pure function of the AUTHENTICATED, AUTHORIZED, canonically-ordered command SET: when N
// authenticated clients each upstream ONLY their own seat's commands and their union is the whole scenario
// (delivered before each apply_tick), the produced frames are BYTE-IDENTICAL to session::build_server_frames
// — regardless of which identity connected in which order, the upstream reorder/chunking, OR any downstream
// cap/liveness drop (a shed client changes nothing about the frames or any surviving client's bytes; a
// foreign/unauthenticated command changes nothing at all). The bridge (seads_netauthasync_test) proves it.
//
// Honest scope: the HELLO-001 handshake read at accept is bounded-BLOCKING (accept_deadline_ms — the one
// non-async touch, matching layer 21; a mid-stream joiner that connects but stalls before sending its HELLO
// is bounded by that deadline). No late-join CATCH-UP (that is layer 20's axis on the join-order server; an
// authenticated catch-up is the natural follow-up — orthogonal, replay-depth only). The "credential" is an
// opaque i64 the server looks up in a roster (a production system would carry a MAC/signature — orthogonal
// crypto).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "authserver.h"   // seads::netinput::CredentialTable (the layer-21 identity->seat roster)
#include "boundserver.h"  // seads::netinput::seat_authorizes (the layer-18 authorization predicate)
#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative authenticated + async server. Gather `min_initial` clients (bounded by
// accept_deadline_ms), reading each client's HELLO-001, authenticating its token against `creds` to a
// DESIGNATED seat, going non-blocking, and ENQUEUEING its BIND-001 handshake as the first downstream
// bytes; then for each produced frame read upstream commands (submitting only a client's OWN-seat
// commands), step the producer, and ENQUEUE the frame to every client's non-blocking userspace send
// buffer (async output; no client can back-pressure the loop).
//   * cap_bytes (0 = unbounded): a client whose pending backlog an enqueue leaves above the cap is shed
//     (drop-slowest; Stats.capped, also a leave — its seat is freed).
//   * liveness_frames (0 = no reaping): a client that makes NO receive progress (drains zero bytes AND
//     stays pending) for more than liveness_frames consecutive produced frames is reaped (Stats.reaped,
//     also a leave — its seat is freed); a slow-but-alive client is never reaped.
// `on_frame(fi)` fires at the TOP of iteration fi (before the upstream read + the step) — the test
// rendezvous hook, same contract as broadcast_auth / broadcast_bound_async. Returns Stats; ok iff the
// whole stream was produced. cmds_unauth counts commands dropped for naming a foreign aircraft (or coming
// from a spectator). The caller owns (and must enroll) `creds`, sized to the same n_aircraft as the
// producer/queue; the listener is NOT closed here (the caller owns it, matching broadcast_auth).
Stats broadcast_auth_async(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                           CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                           const std::function<void(std::size_t)>& on_frame = {},
                           std::size_t cap_bytes = 0, std::size_t liveness_frames = 0);

}  // namespace netinput
}  // namespace seads
