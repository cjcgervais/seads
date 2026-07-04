// SEADS authoritative BOUND + ASYNC + CATCH-UP server (netcode LAYER 20) — bidirectional late-join
// catch-up. The layer-19 bound + async server grows the one capability every bidirectional layer since
// 15b flagged as deferred: a client that joins mid-fight is REPLAYED the frames it missed.
//
// Layer 19 (broadcast_bound_async, boundasyncserver.h) composed the layer-18 seat binding (a join-order
// SeatPolicy + a BIND-001 handshake + upstream authorization) with the layer-16 async downstream hygiene
// (non-blocking per-client send buffers + byte-cap + liveness reap). But a client joining after frame 0
// still received only the SUFFIX from its accept point — it could never reconstruct the whole stream.
// The unbound broadcast already solved exactly this on the download-only path: layer 10 replays the
// missed prefix, and layer 14 bounds that replay to the last `catchup_window` produced frames (O(W)
// memory on a stream of any length). Layer 19's ADR named the merge UNBLOCKED — "the binding is settled
// AND this async server owns the per-client send buffers a catch-up prefix enqueue builds on: a replayed
// joiner gets a seat + BIND, then the catch-up prefix, then the live suffix." Layer 20 is that merge.
//
// broadcast_bound_catchup is broadcast_bound_async (boundasyncserver.cpp) with broadcast_live's
// history-retention + windowed-replay folded in — a SIBLING so the sealed broadcast.cpp AND
// broadcast_bound / broadcast_bidi / broadcast_bound_async are byte-for-byte UNTOUCHED (this file owns
// its own BoundCatchupClient + flush/enqueue/cap/reap/drop helpers; it is transport, outside the
// world_hash — no det_math, no seal, rides v1.26r0). It keeps ALL THREE of layer 19's binding + hygiene
// properties and adds ONE:
//   4. LATE-JOIN CATCH-UP — the server retains the produced payloads (the LAST `catchup_window` of them,
//      or ALL when window==0), and a client accepted at frame fi has that retained prefix
//      history[max(0,fi-W):fi] ENQUEUED right after its BIND-001 record — before it enters the live
//      stream at frame fi. So its whole downstream delivery is [BIND | frames[max(0,fi-W):fi] | live
//      frames fi, fi+1, ...] = [BIND | frames[max(0,fi-W):]]. A client present from the initial gather
//      (history empty) receives [BIND | the whole stream]. `Stats.trimmed` counts window evictions.
//
// The replay rides the SAME non-blocking userspace send buffer as every frame (it is simply enqueued
// first, after the BIND), so a slow joiner's catch-up cannot back-pressure the loop — it drains on
// writability like any pending bytes, and the byte-cap applies to each replayed prefix frame too (a
// joiner whose replay backlog an enqueue leaves above the cap is shed during replay, counted `capped`,
// and — never having become live — is NOT a leave and its seat IS returned).
//
// DETERMINISM: catch-up is a fourth ORTHOGONAL axis. It only decides a joiner's REPLAY DEPTH (how far
// back its prefix reaches) — it touches neither the CommandQueue's canonical selection (upstream
// authorization is unchanged) nor which bytes flow to any other client. So the authoritative kernel's
// frames stay a pure function of the AUTHORIZED, canonically-ordered command SET, and every client's
// delivery is a byte-exact window of [BIND | the produced stream]: when N seated clients each upstream
// only their own seat's commands (their union the whole scenario), the produced stream is BYTE-IDENTICAL
// to session::build_server_frames and a mid-stream joiner is replayed a byte-exact contiguous suffix of
// it. The bridge (seads_netboundcatchup_test) proves the catch-up window regimes, the authorization
// composition (a spectator's commands are all rejected yet it still catches up), and the hygiene
// composition (a non-reading joiner's catch-up backlog is shed by the byte-cap).
//
// Honest scope: catch-up retention is O(catchup_window) frames (window==0 = retain-all = O(stream),
// only for a bounded run — layer 14's boundary, inherited); per-frame join service (accept + replay
// enqueue happen in the frame loop, one select() slice). The seat binding stays UNAUTHENTICATED
// (join-order position, not identity).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "inputserver.h"   // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "boundserver.h"   // seads::netinput::{SeatPolicy, seat_authorizes}
#include "socket.h"        // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative bound + async + catch-up server. Gather `min_initial` clients (bounded by
// accept_deadline_ms), assigning each a join-order seat and enqueueing its BIND-001 handshake as the
// first downstream bytes; then for each produced frame read upstream commands (submitting only a
// client's OWN-seat commands), step the producer, and ENQUEUE the frame to every client's non-blocking
// userspace send buffer. A client accepted mid-stream is additionally replayed the retained catch-up
// prefix (enqueued right after its BIND, before the live suffix).
//   * cap_bytes (0 = unbounded): a client whose pending backlog an enqueue leaves above the cap is shed
//     (drop-slowest; Stats.capped). A live member shed this way is also a leave (its seat freed); a
//     mid-replay joiner shed before becoming live is capped but NOT a join/leave (its seat is returned).
//   * liveness_frames (0 = no reaping): a client making no receive progress for more than
//     liveness_frames consecutive produced frames is reaped (Stats.reaped, also a leave; seat freed).
//   * catchup_window (0 = retain ALL produced payloads): retain only the LAST `catchup_window`, evicting
//     the oldest per new frame (Stats.trimmed). A joiner at frame fi is replayed exactly
//     frames[max(0,fi-catchup_window):fi]; window==0 replays frames[0:fi] (whole prefix).
// `on_frame(fi)` fires at the TOP of iteration fi (before the upstream read + the step) — the test
// rendezvous hook, same contract as broadcast_bound_async. Returns Stats; ok iff the whole stream was
// produced. Closes every client + does NOT close the listener (the caller owns it).
Stats broadcast_bound_catchup(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                              std::size_t min_initial, int accept_deadline_ms,
                              const std::function<void(std::size_t)>& on_frame = {},
                              std::size_t cap_bytes = 0, std::size_t liveness_frames = 0,
                              std::size_t catchup_window = 0);

}  // namespace netinput
}  // namespace seads
