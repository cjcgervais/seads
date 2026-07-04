// SEADS authoritative BOUND + ASYNC server (netcode LAYER 19) — layer-18 seat binding merged with the
// layer-16 downstream OUTPUT HYGIENE. The two most recent bidirectional servers, composed.
//
// Layer 18 (broadcast_bound, boundserver.h) settled the client->aircraft binding: a join-order
// SeatPolicy seats each client, a one-time BIND-001 handshake tells it its aircraft, and the server
// AUTHORIZES upstream commands (a client may steer only its own seat). But that first binding cut kept
// broadcast_input's BLOCKING send_all downstream, so ONE slow client back-pressures the whole broadcast
// and a silently-dead peer that never sends EOF is never shed — exactly the problems layers 11/12/15a
// already solved on the fan-out path, and that layer 16 (broadcast_bidi, bidiserver.h) brought to the
// UNBOUND bidirectional server. Layer 16 and layer 18 grew from broadcast_input along ORTHOGONAL axes:
// layer 16 added downstream hygiene (async send buffers + byte-cap + liveness reap), layer 18 added the
// upstream binding (seats + BIND + authorization). Layer 19 is their product: a bound server with the
// async downstream hygiene.
//
// broadcast_bound_async is broadcast_bidi's async loop (bidiserver.cpp) with broadcast_bound's three
// binding additions folded in (boundserver.cpp) — a SIBLING of both, so the sealed broadcast.cpp AND
// broadcast_bound / broadcast_bidi are byte-for-byte UNTOUCHED (this file owns its own BoundAsyncClient
// + flush/enqueue/cap/reap/drop helpers; it is transport, outside the world_hash — no det_math, no seal,
// rides v1.26r0). The three additions, all pure TRANSPORT:
//   1. SEAT ASSIGNMENT — a join-order SeatPolicy (netinput::SeatPolicy, boundserver.h) hands each joining
//      client the LOWEST free aircraft index in [0, n_aircraft); further joiners are SPECTATORS (seat -1,
//      receive-only). A leaver — by clean EOF, a fatal flush, a byte-cap shed, OR a liveness reap —
//      frees its seat for reuse. Deterministic in the join/leave order (a transport fact).
//   2. BIND HANDSHAKE — right after accept, the server ENQUEUES the client's one-time BIND-001 record
//      (bind001.h) as the FIRST bytes of its downstream send buffer, before any snapshot frame. Unlike
//      layer 18 (a blocking send_all), here the BIND rides the same non-blocking userspace send buffer
//      as every frame — it is simply the first thing enqueued, so the async flush delivers it first.
//   3. UPSTREAM AUTHORIZATION — a decoded command is submitted to the CommandQueue ONLY when it names the
//      client's own seat (seat_authorizes: seat >= 0 && aircraft == seat). A foreign-aircraft command —
//      or any command from a spectator — is DROPPED (Stats.cmds_unauth), byte-identical to it never
//      arriving (the same class as the queue's OUT_OF_RANGE reject).
//
// DETERMINISM (the whole point, and why the merge is safe): the two axes touch DISJOINT machinery. The
// binding is an admission FILTER on the upstream (authorization) plus downstream bookkeeping (seats +
// BIND) — it cannot make the output depend on transport order/chunking any more than OUT_OF_RANGE did.
// The hygiene is purely the DOWNSTREAM transport — WHEN each client's bytes move and WHICH slow/dead
// clients are shed. Neither touches the CommandQueue's canonical selection. So the authoritative kernel's
// frames stay a pure function of the AUTHORIZED, canonically-ordered command SET: when N clients each
// upstream ONLY their own seat's commands and their union is the whole scenario command set (delivered
// before each apply_tick), the produced frames are BYTE-IDENTICAL to session::build_server_frames —
// regardless of the seat permutation, the upstream reorder/chunking, OR any downstream cap/liveness drop
// (a shed client changes nothing about the frames or any surviving client's bytes; a foreign command
// changes nothing at all). The bridge (seads_netboundasync_test) proves all three.
//
// Honest scope (this composition cut): still NO late-join CATCH-UP — a spectator joining mid-fight gets
// only the suffix from its join point (a bound catch-up is now UNBLOCKED — the binding is settled: a
// replayed joiner would get a seat + BIND, then the catch-up prefix — but it is a separate layer). The
// seat binding stays UNAUTHENTICATED (join-order position, not identity).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "inputserver.h"   // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "boundserver.h"   // seads::netinput::{SeatPolicy, seat_authorizes}
#include "socket.h"        // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative bound + async server. Gather `min_initial` clients (bounded by
// accept_deadline_ms), assigning each a join-order seat and enqueueing its BIND-001 handshake as the
// first downstream bytes; then for each produced frame read upstream commands (submitting only a
// client's OWN-seat commands), step the producer, and ENQUEUE the frame to every client's non-blocking
// userspace send buffer (async output; no client can back-pressure the loop).
//   * cap_bytes (0 = unbounded): a client whose pending backlog an enqueue leaves above the cap is shed
//     (drop-slowest; Stats.capped, also a leave — its seat is freed).
//   * liveness_frames (0 = no reaping): a client that makes NO receive progress (drains zero bytes AND
//     stays pending) for more than liveness_frames consecutive produced frames is reaped (Stats.reaped,
//     also a leave — its seat is freed); a slow-but-alive client is never reaped.
// `on_frame(fi)` fires at the TOP of iteration fi (before the upstream read + the step) — the test
// rendezvous hook, same contract as broadcast_bound / broadcast_bidi. Returns Stats; ok iff the whole
// stream was produced. cmds_unauth counts commands dropped for naming a foreign aircraft (or coming from
// a spectator). Closes every client + listener.
Stats broadcast_bound_async(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                            std::size_t min_initial, int accept_deadline_ms,
                            const std::function<void(std::size_t)>& on_frame = {},
                            std::size_t cap_bytes = 0, std::size_t liveness_frames = 0);

}  // namespace netinput
}  // namespace seads
