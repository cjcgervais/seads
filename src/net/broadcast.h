// SEADS single-thread, select-based fan-out BROADCAST server (netcode layer 9). Dynamic join/leave.
//
// Layer 8 broadcast the frame stream to N clients that all connected BEFORE streaming began, one
// send loop per (already-connected) client. Layer 9 makes the fan-out a genuine single-threaded
// event loop: one select() over {listener} u {connected clients} multiplexes
//   * JOIN  — a client connecting mid-stream is accepted at the next frame boundary and receives
//             every frame FROM ITS JOIN POINT ONWARD (frame-aligned: a joiner never sees a partial
//             frame, because each payload is shipped atomically as one length-prefixed frame);
//   * LEAVE — a client that closes (EOF on recv) or errors on send is dropped from the broadcast
//             set without a thread-per-client and without disturbing the others.
// No wall-clock, no per-client thread: the server drives the sealed kernel's precomputed frame list
// and pushes each frame to whoever is currently connected. This is TRANSPORT — outside the kernel
// and world_hash, no det_math, no seal (rides v1.17r0, like layers 5-8).
//
// Determinism note: which contiguous SUFFIX a late joiner receives is a function of WHEN it is
// accepted, and that frame index is knowable from the first frame it decodes (its server_tick) — so
// the delivered stream is byte-exact for that join point regardless of OS timing. The determinism
// bridge (seads_netdyn_test) pins one join to an exact frame via the `on_frame` hook and checks
// each client got precisely frames[join:].
//
// Layer 10 (late-join CATCH-UP): with `catchup=true`, a client accepted mid-stream at frame fi is
// first REPLAYED the missed prefix frames[0:fi] (each length-prefixed, atomically) before it enters
// the live broadcast for frame fi onward — so it receives the WHOLE frame stream frames[0:] and can
// reconstruct the full sealed session digest exactly as a client present from frame 0, closing the
// layer-9 honest-scope gap (a bare late joiner missed the ticks before its join). The replay is a
// synchronous burst on the accepting select iteration (a slow catch-up joiner back-pressures the
// broadcast for that iteration — bounded by the prefix length; async per-client send buffers are a
// separate deferred layer). Still TRANSPORT — no kernel/wire/golden/seal.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "socket.h"

namespace seads {
namespace netbcast {

struct Stats {
    std::size_t frames_sent = 0;  // frames pushed to the broadcast set (== payloads.size() on success)
    std::size_t joins = 0;        // clients accepted (initial + dynamic late joins)
    std::size_t leaves = 0;       // clients dropped on EOF/send-error mid-stream
    bool ok = false;              // reached >=min_initial clients and sent every frame
};

// Broadcast `payloads` (each a whole self-delimiting protocol-6 snapshot) over `listener`, wrapping
// each in the layer-7 length prefix (framing::encode_frame) so joiners stay frame-aligned. Blocks
// (bounded by accept_deadline_ms of readability waits) until >=min_initial clients are connected
// before frame 0, then streams the frames, accepting dynamic joins and dropping leaves via one
// select() per frame. `on_frame(fi)` — if set — runs at the top of each frame iteration BEFORE the
// join/leave service + send; a test uses it to rendezvous a deterministic mid-stream join. The
// listener should already be non-blocking (set_nonblocking) so accept never wedges. Closes every
// client + the listener before returning.
Stats broadcast_select(netsock::socket_t listener,
                       const std::vector<std::vector<std::uint8_t>>& payloads,
                       std::size_t min_initial, int accept_deadline_ms,
                       const std::function<void(std::size_t)>& on_frame = {},
                       bool catchup = false);

}  // namespace netbcast
}  // namespace seads
