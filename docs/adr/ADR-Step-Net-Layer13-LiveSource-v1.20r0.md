# ADR — Netcode layer 13: open-ended LIVE frame source (`broadcast_live`, no-seal, rides ATM-Sphere v1.20r0)

**Status:** accepted · **Date:** 2026-07-02 · **Seal:** rides **v1.20r0** (transport-only, no reseal)

## Context

Every socket layer so far (7–12) broadcast a **precomputed finite payload list**: the sealed
session ran to completion, `build_server_frames` returned a vector, and the loops indexed
`frames[fi]` of a known size. Layer 12's own doc named the target it was hygiene FOR — "pointed
at an open-ended live stream" — and every handoff since has carried "an open-ended live frame
SOURCE feeding `broadcast_async` incrementally" as a free pick. A real server does the opposite
of precomputing: it **steps the simulation between sends** and does not know how long the stream
is.

## Decision

### `session::FrameProducer` — the incremental server half

The authoritative kernel moves inside a producer object; `next(emit_tick, payload)` steps it on
demand to the next 20 Hz emit point and serializes that frame (tick-0 pre-step world first),
returning false at scenario end. **`build_server_frames` is reimplemented ON the producer** —
batch and incremental bytes are identical by construction, and the refactor is gated by the
sealed session digest (`d0e94e2e…`, unchanged: `session_vectors.h --check` in sync, ctest
`session_reconstruct` + all six socket bridges green).

### `netbcast::broadcast_live` — the pull-source loop

`FrameSource = std::function<bool(std::vector<uint8_t>&)>` (fill the next payload / false at
end). `broadcast_live` is `broadcast_async`'s loop with the vector index replaced by one source
pull per iteration — same gather, same per-frame `select_rw` JOIN/LEAVE/writability service, same
per-client userspace buffers, the same layer-12 `cap_bytes` shed policy, the same bounded drain.
The loop **never knows the frame count**; only the source ends the stream. With `catchup=true`
each produced payload is **retained as it is made**, so a mid-stream joiner is replayed exactly
`frames[0:fi]` — layer-10 semantics from a stream that never existed as a whole.
`broadcast_select` / `broadcast_async` are untouched (layers 9–12 verbatim).

### Honest boundaries

* **Catch-up retention is O(stream)** — for a finite scenario that is exactly layer 10's memory
  shape, but a genuinely open-ended source should run `catchup=false` (bounded/windowed catch-up
  is the boundary this layer leaves).
* **The source is pulled synchronously once per iteration** — socket service happens per frame,
  so a source that stalls stalls join service with it. Frame pacing belongs to the caller (the
  demo may sleep between pulls); nothing in the loop reads the wall clock.
* `Stats.ok` for a live run means "initial gather succeeded and the source was drained to its
  end" — there is no expected frame count to compare against.

## Verification

* **Bridge `seads_netlive_test`** (ctest `netlive_bridge`, native-x64 CI legs like layers 7–12):
  * **LEG 0** — `FrameProducer` pulled to exhaustion equals `build_server_frames` byte-for-byte
    (count, emit_ticks, payloads) + idempotent exhaustion (no frame past the end).
  * **LEG 1** (live socket, `catchup=false`) — a server whose kernel is stepped INSIDE the
    broadcast loop streams to an EARLY client (byte-identical to the batch reference, reconstructs
    the sealed SESSION-SK-001 digest) and a rendezvoused mid-stream joiner (receives exactly
    `frames[J:]`).
  * **LEG 2** (live socket, `catchup=true`) — the joiner is replayed the on-the-fly retained
    history and receives the WHOLE stream byte-identically, reconstructing the same sealed digest.
* **Property tests +2 ⇒ 177** (`test_broadcast.py` layer-13 section): `live_deliver` — a pull
  model with no `len()` — delivers, under any kernel-acceptance pattern, exactly the batch
  send-buffer model's bytes (== `encode_stream`); the retained history after j pulls is exactly
  `frames[0:j]`, so replay + live suffix reconstructs the whole stream for any join point.
* **Demo:** `seads_netserver [port] [n] [catchup] [async] [cap_bytes] [live]` — `live=1` serves
  from the producer (inherently async; `cap_bytes` applies); a real `seads_netclient` against it
  reconstructed the sealed digest cross-process.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes, or
tuning touched ⇒ all 12 goldens byte-identical, no digest moved, no seal.** ctest 17→18 GCC +
Clang; guardian.yml gains the layer-13 bridge step (native x64 legs).

## Alternatives rejected

* **A push API (caller calls `server.send(frame)`)**: inverts control and forces the caller to
  re-own the select loop, join/leave service, and drain — the exact machinery layers 9–12
  hardened. Pull keeps one loop owner and lets the batch path be expressed on top of it.
* **Retro-fitting a source parameter into `broadcast_async`**: an overload changing the sealed
  bridge-tested function's shape risks silent behavioral drift; a sibling function reusing the
  same static helpers keeps layers 11/12 verbatim and diff-provably untouched.
* **A ready()/poll source (service sockets while the source is idle)**: real need only appears
  with wall-clock pacing, which the doctrine keeps out of the transport loop; deferred with the
  windowed catch-up as future layers.
