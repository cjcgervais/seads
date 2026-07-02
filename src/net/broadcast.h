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
//
// Layer 11 (ASYNC single-thread OUTPUT): broadcast_select's sends are still BLOCKING send_all —
// once a slow client's kernel buffers fill, the whole broadcast (every other client's frames)
// stalls behind it, and the layer-10 catch-up replay is a blocking burst. broadcast_async removes
// that back-pressure: every accepted client is non-blocking and owns a USERSPACE send buffer;
// each frame (and a joiner's catch-up prefix) is ENQUEUED, the kernel takes what it can now
// (send_some), and the remainder is flushed when the same single select() that services JOIN/LEAVE
// also reports the client WRITABLE (select_rw). The frame loop therefore never blocks on any one
// client: a slow client just accumulates buffer (bounded by the total stream size — an explicit
// byte-cap/drop policy is the honest boundary this layer leaves) while the others stream at full
// rate. After the last frame is enqueued, a bounded DRAIN phase flushes the stragglers (a client
// still pending when the drain deadline expires is dropped as a leave — fail-not-wedge). Delivered
// BYTES are identical to broadcast_select's: same frames, same order, same length prefix — the
// bridge (seads_netasync_test) proves a never-reading-during-broadcast client still reconstructs
// the sealed SESSION-SK-001 digest. Still TRANSPORT — no kernel/wire/golden/seal.
//
// Layer 12 (send-buffer BYTE-CAP + drop-slowest): layer 11's per-client send buffer is unbounded —
// fine for a precomputed finite stream, but pointed at an open-ended live stream a permanently-slow
// client accumulates server memory without bound. broadcast_async gains an opt-in `cap_bytes`
// (0 = unbounded = layer-11 behavior EXACTLY): whenever a frame enqueue leaves a client's PENDING
// userspace backlog above the cap, that client is dropped (drop-slowest — the clients that cannot
// keep up are shed; everyone else streams on untouched). The policy is applied uniformly, catch-up
// prefix replay included. A drop is deliberate hygiene, not an error: it is counted in `capped`
// (and, for a live client, also as a leave, exactly like a send failure). The delivered bytes of a
// SURVIVING client are unchanged — the cap decides only WHO is dropped, never WHICH bytes flow — and
// a dropped client's delivered bytes are always a clean byte-PREFIX of the encoded stream (the
// kernel-accepted prefix; the pending tail is discarded whole). Still TRANSPORT — no
// kernel/wire/golden/seal.
//
// Layer 13 (open-ended LIVE frame SOURCE): every layer so far broadcast a PRECOMPUTED finite
// payload list — the sealed session was run to completion before the first byte moved. A real
// server does the opposite: it steps the simulation BETWEEN sends. broadcast_live is
// broadcast_async's loop fed by a pull SOURCE (`FrameSource` — fills the next payload, returns
// false at end-of-stream): the loop never knows the frame count up front, services JOIN/LEAVE/
// writability once per produced frame, and enqueues each frame through the SAME per-client
// buffers, byte-cap, and drain machinery as layers 11/12 — so a source that never ends streams
// forever in bounded memory (cap_bytes sheds the laggards). With catchup=true the produced
// payloads are RETAINED as they are made (the history a mid-stream joiner is replayed) — for a
// finite stream that is exactly layer 10's memory shape, but for a genuinely open-ended source
// retention is O(stream): run an unbounded live stream with catchup=false (the honest boundary
// this layer leaves is bounded/windowed catch-up). The source is pulled synchronously once per
// iteration: socket service happens per frame, so a source that stalls stalls join service with
// it (frame pacing belongs to the caller — e.g. the demo server sleeps between pulls; nothing in
// this loop reads the wall clock). Delivered bytes are IDENTICAL to handing the same frames to
// broadcast_async as a vector — the bridge (seads_netlive_test) proves a live-stepped sealed
// session reconstructs the sealed digest, batch == incremental byte-for-byte. Still TRANSPORT —
// no kernel/wire/golden/seal.
//
// Layer 14 (bounded/windowed catch-up): layer 13's catch-up retains EVERY produced payload —
// O(stream) server memory, exactly what a genuinely open-ended source cannot afford (the honest
// boundary layer 13 named). broadcast_live gains an opt-in `catchup_window` (0 = retain all =
// layer-13 behavior EXACTLY): with a window W, only the LAST W produced payloads are retained —
// the oldest is evicted as each new frame lands (counted in `trimmed`) — so catch-up runs in
// O(W) memory on a stream of ANY length. A mid-stream joiner accepted at frame fi is replayed
// the retained window frames[max(0,fi-W):fi] and enters live at fi: its delivered stream is
// EXACTLY the contiguous suffix frames[max(0,fi-W):] — frame-aligned, no gap, no duplicate. The
// window decides only how far BACK a joiner's replay reaches, never which bytes flow to anyone
// else (it is consulted only at accept time; live clients' bytes are untouched). A joiner the
// window still fully covers (fi <= W) receives the WHOLE stream and reconstructs the sealed
// digest — the layer-13 degenerate case. Honest scope: a joiner beyond the window CANNOT
// reconstruct the full digest (the trimmed prefix is gone forever — that is the point of
// bounding memory); the claim is the transport delivered precisely frames[max(0,fi-W):], and
// the join frame is knowable from the first decoded server_tick, exactly the layer-9 law. Still
// TRANSPORT — no kernel/wire/golden/seal.
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
    std::size_t leaves = 0;       // clients dropped mid-stream (EOF/send-error, or a policy drop)
    std::size_t capped = 0;       // layer-12 policy drops: pending backlog exceeded cap_bytes (a
                                  // live client so dropped also counts as a leave; a joiner capped
                                  // during its catch-up replay was never live — capped only)
    std::size_t trimmed = 0;      // layer-14 window evictions: payloads dropped from the catch-up
                                  // history once it exceeded catchup_window (0 when unwindowed)
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

// Layer-11 async-output variant of broadcast_select — SAME contract and delivered bytes, but no
// client can back-pressure the frame loop: sends are non-blocking into per-client userspace send
// buffers, flushed on select() writability; the catch-up prefix is enqueued, not burst. After the
// frame loop a bounded drain phase flushes stragglers (still-pending at deadline => dropped as a
// leave). `frames_sent` counts frames ENQUEUED to the then-current broadcast set; ok == every frame
// enqueued (per-client delivery shortfalls surface as `leaves`, exactly like a send failure did).
// Layer 12: `cap_bytes` (0 = unbounded, layer-11 behavior exactly) bounds each client's pending
// userspace backlog — a client left above the cap by an enqueue (live frame OR catch-up prefix
// frame) is dropped and counted in `capped` (drop-slowest; see the file header).
Stats broadcast_async(netsock::socket_t listener,
                      const std::vector<std::vector<std::uint8_t>>& payloads,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame = {},
                      bool catchup = false, std::size_t cap_bytes = 0);

// Layer-13 pull source: fill `payload` with the next whole snapshot payload and return true, or
// return false at end-of-stream (payload is then ignored). Called exactly once per frame
// iteration; may compute (e.g. step the sealed kernel — session::FrameProducer) between calls.
using FrameSource = std::function<bool(std::vector<std::uint8_t>&)>;

// Layer-13 LIVE variant of broadcast_async — the SAME gather/enqueue/cap/drain machinery, but the
// frames are pulled from `source` one at a time as the loop runs (the frame count is unknown up
// front; the stream ends when the source says so). `on_frame(fi)` fires after frame fi is
// produced and BEFORE it is enqueued (same rendezvous semantics as the batch loops). With
// catchup=true every produced payload is retained so a mid-stream joiner is replayed the full
// missed prefix (O(stream) server memory when unwindowed).
// Layer 14: `catchup_window` (0 = retain all, layer-13 behavior exactly) bounds the retained
// history to the LAST catchup_window payloads — a joiner accepted at frame fi is replayed
// frames[max(0,fi-catchup_window):fi] (evictions counted in `trimmed`), so an open-ended source
// can run catch-up in O(window) memory; see the file header for the delivered-suffix law.
// `frames_sent` counts frames produced + enqueued to the then-current broadcast set; ok == the
// initial gather succeeded and the source was drained to its end.
Stats broadcast_live(netsock::socket_t listener, const FrameSource& source,
                     std::size_t min_initial, int accept_deadline_ms,
                     const std::function<void(std::size_t)>& on_frame = {},
                     bool catchup = false, std::size_t cap_bytes = 0,
                     std::size_t catchup_window = 0);

}  // namespace netbcast
}  // namespace seads
