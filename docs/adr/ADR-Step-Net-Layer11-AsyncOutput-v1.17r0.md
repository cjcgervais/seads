# ADR-Step-Net-Layer11-AsyncOutput-v1.17r0 — Async single-thread output (per-client send buffers) over the select() broadcast

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events / layers 7–10)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Layers 9/10 (`ADR-Step-Net-Layer9-DynamicBroadcast-v1.17r0`, `ADR-Step-Net-Layer10-CatchUp-v1.17r0`)
made the fan-out a single-threaded `select()` event loop with dynamic join/leave and late-join
catch-up — but every send is still a **blocking `send_all`**. Layer 10's "boundaries" section named
the gap plainly: *"the replay is synchronous — a slow catch-up joiner back-pressures the broadcast
for its accepting iteration,"* and more generally: once **any** client's kernel socket buffers fill
(a stalled reader), the whole broadcast — every other client's frames — wedges behind it. It listed
the follow-up verbatim: *"async single-thread OUTPUT (writability `select` + per-client send
buffers, so a slow client can't back-pressure the broadcast)."*

Layer 11 is that rung. No client can now stall the frame loop: output is non-blocking through
per-client **userspace send buffers**, flushed exactly when the same single `select()` that
services JOIN/LEAVE reports the client **writable**. The delivered bytes are unchanged — same
frames, same order, same length-prefix envelope — so everything layers 5–10 proved about the byte
stream carries over verbatim.

## 2) Decision

**(a) Socket primitives (`src/net/socket.{h,cpp}`).**

- `send_some(s, buf, n)` — **one non-blocking send attempt**: returns the byte count the kernel
  accepted (≥0; `0` == `EWOULDBLOCK`/`WSAEWOULDBLOCK`, a full-buffer no-op) or `<0` on a fatal
  error. Unlike `send_all` it never blocks. POSIX retries `EINTR`; both sides sit behind the one
  existing `#ifdef _WIN32` boundary.
- `select_rw(rfds, wfds, timeout, readable, writable)` — `select_readable` generalized with a
  **write set**: one `select()` watches readability (joins, leaves) AND writability (a clogged
  client's kernel buffer opened up). Winsock ignores nfds / POSIX `max(fd)+1` over both sets.
- `set_sndbuf` / `set_rcvbuf` — pin `SO_SNDBUF`/`SO_RCVBUF` (test instrumentation: the bridge pins
  tiny kernel buffers so a blocking send provably wedges where the async path must not; on a
  listener, accepted sockets inherit the value).

**(b) The async broadcast loop (`src/net/broadcast.{h,cpp}` — `netbcast::broadcast_async`).**
Same signature and contract as `broadcast_select` (incl. `on_frame` + `catchup`); `broadcast_select`
itself is **untouched** (layers 9/10 behavior verbatim, their bridges keep gating it). Differences:

- Every accepted client goes **non-blocking** and owns a `BufClient` userspace send buffer
  (`buf[off:]` = the pending tail; compaction only when fully drained, so memory is bounded by the
  bytes still owed ≤ the total encoded stream).
- `enqueue_bytes` appends the length-prefixed frame and **opportunistically flushes** via
  `send_some` — an unclogged client never accumulates a buffer at all (the fast path is the old
  behavior minus the ability to block).
- The **frame loop never blocks on any one client**: one `select_rw(0ms)` per frame services JOIN
  (listener readable), LEAVE (client readable at EOF), and **flush** (client writable); then frame
  `fi` is enqueued to every live client. A slow client just accumulates buffer while the others
  stream at full rate.
- The **layer-10 catch-up prefix is ENQUEUED, not burst**: `accept_pending_async` appends
  `frames[0:fi]` to the joiner's buffer — closing layer 10's named boundary (a slow catch-up joiner
  back-pressured its accepting iteration).
- After the last frame is enqueued, a **bounded DRAIN phase** flushes stragglers: progress-bound
  (finite bytes owed) plus an idle cap (~30 s with zero writability progress) — a client still
  pending at the deadline is dropped as a **leave** (fail-not-wedge), mirroring a send failure.
  `frames_sent` counts frames **enqueued** to the then-current set; `ok` == every frame enqueued
  (per-client shortfalls surface as `leaves`, exactly as before).

**(c) The async-output determinism BRIDGE (`seads_netasync_test`).** Two legs, all rendezvous
cv+`notify_all` (no `std::future` — MinGW `call_once` caveat) pinned by `on_frame` (**no sleeps**):

- **LEG 1 (no back-pressure, volume):** a synthetic **~8 MiB** frame list (512 × 16 KiB,
  deterministic fill) streamed to one **SLOW** client that reads **nothing** while the server runs,
  through pinned tiny kernel buffers (`set_sndbuf(listener, 16 KiB)` — inherited by the accepted
  socket, disabling send autotuning; best-effort `set_rcvbuf` on the client; a never-reading
  receiver's window holds near its initial size regardless, so kernel capacity is a few hundred KiB
  ≪ the stream). A blocking layer-9/10 server **provably wedges** at that capacity — the watchdog
  would fail the test. The bridge asserts the async frame loop instead reached the **last** frame
  (the `on_frame` hook observes it) while SLOW had read **0 bytes**; SLOW then drains to EOF and
  its stream must be **byte-identical** to the encoded frame list, with **zero leaves** (the drain
  delivered, not dropped).
- **LEG 2 (sealed-session fidelity, async + catch-up):** the exact layer-10 bridge shape run
  through `broadcast_async(catchup=true)` — EARLY (from frame 0) and CATCHUP (rendezvoused to frame
  J=20, prefix **enqueued**) both receive the whole 41 frames byte-identically and reconstruct the
  **same** sealed SESSION-SK-001 digest (`24f71845…c332`) as the in-process `run_session` reference
  (**not re-derived**). Buffered output adds zero information and zero nondeterminism.

**(d) The demo server exposes it (`seads_netserver [port] [num_clients] [catchup] [async]`).** A
fourth optional CLI arg (default `0`) selects `broadcast_async` — the human demo shares the same
loops the CI bridges gate (no untested divergence).

**Why NO-SEAL.** Nothing sealed is touched: the framing envelope, the protocol-6 snapshot bytes,
wire scales, rails, `det_math`, the kernel, and every golden are unchanged. Buffering already-
emitted frames in userspace before the same socket write is pure transport over the existing byte
stream — the same category as interp / predict / session / events / layers 7–10.

## 3) Verification (gates)

- **Determinism bridge:** `seads_netasync_test` (ctest `netasync_bridge`, under the existing
  `option(SEADS_SOCKET_TESTS ON)` + `if(NOT CMAKE_CROSSCOMPILING)` guard): both legs PASS — GCC
  **and** Clang, **12/12 (gcc) + 8/8 (clang)** stress reruns clean. Local ctest **16/16** (was
  15/15; + `netasync_bridge`) both toolchains.
- **Layer-9/10 regression:** `broadcast_select` is untouched; `netdyn_bridge` and
  `netcatchup_bridge` still pass. The layer-7/8 bridges are unaffected.
- **Property tests** `tests/property/test_broadcast.py` (**+2 ⇒ 143**): a pure send-buffer model
  (`sendbuffer_deliver`, mirroring `enqueue_bytes`/`flush_client`: append the encoded frame, the
  kernel accepts an arbitrary byte count per flush — 0 == EWOULDBLOCK — then the drain emits the
  tail) proved **chunking-invariant**: for ANY acceptance pattern (including all-stall and
  all-greedy corners) the delivered bytes are exactly `encode_stream(frames)` — buffering changes
  WHEN bytes move, never WHICH bytes or their order; plus composition with the layer-7 codec
  (enqueue → arbitrary partial flushes → arbitrary TCP re-chunking → `StreamReassembler` == the
  original frames). The timing-independent core of the bridge.
- **Gates wired:** `guardian.yml` — `seads_netasync_test` is a default target, so the whole-project
  build step build-only-smokes it on **all five** legs; the async bridge runs on the **native x64**
  legs only (MSVC/GCC/Clang), exactly like the layer-7/8/9/10 bridges.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 11 never touched the
  kernel. No new golden; the guardian golden matrix is unchanged.
- **Full baseline green:** 15/15 receipt gates PASS, 143 property tests, ctest 16/16 GCC+Clang.

## 4) Consequences / boundaries

- **What is NOT done (by design):** the per-client buffer is **unbounded** (in practice bounded by
  the total encoded stream, since frames are precomputed) — an explicit **byte-cap / drop-slowest
  policy** for open-ended live streams is the honest boundary this layer leaves; not blocked.
  Likewise the drain phase *waits* for a slow reader (bounded) rather than prioritizing it.
- **Delivered bytes are provably unchanged:** the bridge checks delivered bytes (not wall-clock),
  and the property model proves delivery is invariant to the kernel's acceptance pattern — so every
  byte-stream statement from layers 5–10 (suffix-exactness, catch-up = whole stream, digest
  reconstruction) holds verbatim under async output.
- **Backward compatibility:** `broadcast_select` is byte-for-byte the layer-10 code; nothing
  changes unless a caller opts into `broadcast_async`. `seads_netserver` keeps its
  `[port] [num_clients] [catchup]` CLI and gains an optional `[async]` arg.
- **Next (optional, none blocked):** a send-buffer byte-cap + drop-slowest policy (the live-stream
  hygiene rung); per-round hit granularity (a kernel event QUEUE — its own ADR); renderer polish
  (guns + kill-feed in the live `--fly` path); or an optional new seal (component/region damage;
  **B5** ISA atmosphere).
