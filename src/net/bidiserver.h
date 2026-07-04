// SEADS BIDIRECTIONAL server (netcode LAYER 16) — the layer-15b upstream input path merged with the
// layer-11/12/15a downstream OUTPUT HYGIENE.
//
// Layer 15b (broadcast_input, inputserver.h) opened the bidirectional axis: a client sends tick-stamped
// INPUT-001 Commands UP into the authoritative sealed kernel, and the server streams the resulting
// snapshots back DOWN. But that first cut kept the two halves deliberately simple — the downstream send
// was BLOCKING send_all, so ONE slow client back-pressures the whole broadcast (every other client's
// frames stall behind its full kernel buffer), and a silently-dead peer that never sends EOF is never
// shed. Every one of those problems was already SOLVED on the server->client-only path (layers 11-15a,
// broadcast.cpp): per-client userspace send buffers so no client can back-pressure the loop (async
// output, layer 11), an opt-in byte-cap that sheds a client whose backlog grows without bound
// (drop-slowest, layer 12), and a liveness timeout that reaps a client making no receive progress (a
// silent dead peer, layer 15a). Layer 16 brings those to the bidirectional server.
//
// broadcast_bidi is broadcast_input's loop with broadcast_live's downstream machinery folded in — a
// SIBLING of both, so the sealed layer-11..15a code in broadcast.cpp is byte-for-byte UNTOUCHED (this
// file owns its own BidiClient + flush/enqueue/cap/reap helpers; it is transport, outside the
// world_hash — no det_math, no seal, rides v1.26r0). Each iteration:
//   1. on_frame(fi) — the test rendezvous hook (block until a client has sent its commands, so they
//      are ingested before their apply_tick), exactly broadcast_input's contract.
//   2. one select_rw() over {listener} u {clients-readable} u {pending-clients-writable}:
//        * listener readable  -> ACCEPT (each joiner goes non-blocking; no catch-up prefix — see the
//          honest-scope note below);
//        * client WRITABLE    -> flush its userspace send buffer (layer-11 async output);
//        * client READABLE    -> DRAIN all available upstream bytes (framing::StreamReassembler ->
//          input001::decode_command -> CommandQueue::submit), or LEAVE on EOF/error. This is the ONE
//          place layer 16 differs from broadcast_live's readable handling: a readable client here is
//          usually sending COMMANDS (n>0, fed to the queue), not leaving — only recv<=0 is a leave.
//   3. producer.next() — step the sealed kernel one frame, its per-tick Commands taken from the queue
//      (with hold-last); false ends the stream.
//   4. ENQUEUE that frame to every client's send buffer (non-blocking; a fatal send drops the client),
//      then apply the layer-12 byte-cap (a client left above cap_bytes is shed: capped + leave).
//   5. reap_dead() — the layer-15a liveness reap: a client that drained zero bytes AND stayed pending
//      for > liveness_frames consecutive produced frames is presumed dead and reaped (reaped + leave).
// After the stream ends, a bounded drain flushes stragglers (still-pending at the deadline => dropped),
// exactly broadcast_async/live's tail.
//
// DETERMINISM (the whole point): the merge changes only the DOWNSTREAM transport — WHEN each client's
// bytes move and WHICH slow/dead clients are shed. It does NOT touch the upstream ingest or the
// CommandQueue, so the authoritative kernel's output stays a pure function of the canonically-ordered
// command SET: given commands delivered before their apply_tick, the produced frame stream is invariant
// to upstream order/chunking AND to any downstream cap/liveness drop (a shed client changes nothing
// about the frames or any surviving client's bytes). The determinism bridge (seads_netbidi_test) proves
// this: scrambled upstream commands driven through the FULL async path reproduce build_server_frames
// byte-for-byte to a cooperative reader, a never-reading client is reaped/capped without disturbing the
// cooperative client's byte-identical stream, and the produced frames are unchanged.
//
// Honest scope (first bidirectional-hygiene cut, deferred like layer 15b's boundaries): the downstream
// still has NO late-join CATCH-UP (layers 10/13/14) — a spectator that joins mid-fight gets only the
// suffix from its join point; a bidirectional catch-up must first settle the positional client->aircraft
// binding (layer 15b's other deferral). Client->aircraft binding stays positional and unauthenticated.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative bidirectional server with downstream output hygiene. Gather `min_initial`
// clients (bounded by accept_deadline_ms), then for each produced frame: read upstream commands, step
// the producer, and ENQUEUE the frame to every client's non-blocking userspace send buffer.
//   * cap_bytes (0 = unbounded, layer-15b behavior for the downstream sizing): a client whose pending
//     backlog an enqueue leaves above the cap is shed (drop-slowest; Stats.capped, also a leave).
//   * liveness_frames (0 = no reaping): a client that makes NO receive progress (drains zero bytes AND
//     stays pending) for more than liveness_frames consecutive produced frames is reaped (Stats.reaped,
//     also a leave) — a silently-dead peer that never sends EOF; a slow-but-alive client is never reaped.
// `on_frame(fi)` fires at the TOP of iteration fi (before the upstream read + the step), same as
// broadcast_input. Returns Stats; ok iff the whole stream was produced. Closes every client + listener.
Stats broadcast_bidi(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                     std::size_t min_initial, int accept_deadline_ms,
                     const std::function<void(std::size_t)>& on_frame = {},
                     std::size_t cap_bytes = 0, std::size_t liveness_frames = 0);

}  // namespace netinput
}  // namespace seads
