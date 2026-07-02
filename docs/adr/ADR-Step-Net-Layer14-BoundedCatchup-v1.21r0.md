# ADR — Netcode layer 14: bounded/windowed catch-up (`catchup_window`, no-seal, rides ATM-Sphere v1.21r0)

**Status:** accepted · **Date:** 2026-07-02 · **Seal:** rides **v1.21r0** (transport-only, no reseal)

## Context

Layer 13's catch-up retains **every** produced payload — O(stream) server memory. Its ADR named
that as the honest boundary it left: "a genuinely open-ended source should run `catchup=false`
(bounded/windowed catch-up is the boundary this layer leaves)", and every handoff since carried
it as a free pick. A live server that never ends needs a middle option between "joiners get the
whole history" (unbounded memory) and "joiners get nothing" (no replay at all): replay *the
recent past*, in bounded memory.

## Decision

### `catchup_window` on `broadcast_live`

One new defaulted parameter (`std::size_t catchup_window = 0`; 0 = retain all = layer-13 behavior
**exactly**). With a window W, the retained history holds only the **last W produced payloads**:
as each new frame lands, the oldest retention is evicted (counted in the new `Stats.trimmed`).
The eviction happens at the single point where history grows — the replay path
(`accept_pending_async` with `upto = history.size()`) already sends "the whole retained history",
which now *is* the window; no replay-side change at all.

### The delivered-suffix law

A joiner accepted at frame fi is replayed `frames[max(0,fi-W):fi]` and enters live at fi — its
delivered stream is **exactly the contiguous suffix `frames[max(0,fi-W):]`**: frame-aligned, no
gap, no duplicate, and the start frame is knowable from the first decoded `server_tick` (the
layer-9 law, reached further back). The window is consulted **only at accept time** — it never
changes which bytes flow to a live client (the early client's stream is byte-identical under any
window).

### Honest boundaries

* A joiner beyond the window **cannot reconstruct the full sealed digest** — the trimmed prefix
  is gone forever; that is the point of bounding memory. The claim is the transport delivered
  precisely `frames[max(0,fi-W):]`. A joiner the window still fully covers (fi ≤ W) receives
  `frames[0:]` and reconstructs the sealed digest — the layer-13 degenerate case, recovered
  exactly.
* The window is a **frame count**, not a byte count (frames are near-uniform 20 Hz snapshots;
  a byte-denominated window would buy imprecision for no memory benefit). The layer-12 `cap_bytes`
  stays the byte-denominated knob, on the *outgoing* side.
* `broadcast_select` / `broadcast_async` are untouched (layers 9–12 verbatim; batch mode holds
  the whole payload vector by construction, so a window there bounds nothing).

## Verification

* **Bridge `seads_netwindow_test`** (ctest `netwindow_bridge`, native-x64 CI legs like layers
  7–13; all legs `catchup=true` against a live-stepped `session::FrameProducer`):
  * **LEG 0** (W=1, minimal) — the joiner rendezvoused at J receives exactly `frames[J-1:]`;
    `trimmed == N-1`.
  * **LEG 1** (1<W<J, partial) — the joiner receives exactly `frames[J-W:]` byte-for-byte; the
    EARLY client is byte-identical to the batch reference AND reconstructs the sealed
    SESSION-SK-001 digest; `trimmed == N-W`.
  * **LEG 2** (W ≥ N, degenerate) — `trimmed == 0` and the joiner receives the WHOLE stream,
    reconstructing the SAME sealed digest — layer-13 recovered exactly.
* **Property tests +2 ⇒ 184** (`test_broadcast.py` layer-14 section): `live_deliver_windowed` —
  after j pulls a W-window retains exactly `frames[max(0,j-W):j]` with `trimmed == max(0,j-W)`,
  and replay ++ live == `encode_stream(frames[max(0,j-W):])`, a contiguous canonical suffix
  (W ≥ j ⇒ the whole stream); the window never changes a live client's delivered bytes under any
  kernel-acceptance pattern, and window=0 equals the layer-13 model bit-for-bit.
* **Demo:** `seads_netserver [port] [n] [catchup] [async] [cap_bytes] [live] [window]` —
  smoke-verified cross-process (live=1 catchup=1 window=10: `trimmed=31`, a real
  `seads_netclient` present from frame 0 reconstructed the sealed digest `21aaab49…`).

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes, or
tuning touched ⇒ all 13 goldens byte-identical, no digest moved, no seal.** ctest 18→19 GCC +
Clang; guardian.yml gains the layer-14 bridge step (native x64 legs).

## Alternatives rejected

* **A byte-denominated window**: frames are near-uniform snapshots, so a frame count is exact
  where bytes would be approximate; and it would blur into layer 12's `cap_bytes`, which is a
  different quantity (per-client outgoing backlog, not shared history).
* **A time-denominated window (last T seconds)**: requires wall-clock in the transport loop —
  doctrine forbids it (nothing in the loop may read time); the caller can express "T seconds" as
  T×20 frames itself.
* **Windowing `broadcast_async` too**: batch mode owns the whole payload vector regardless, so a
  window bounds no memory there — it would change sealed-bridge-tested functions for zero gain.
* **A ring buffer instead of `vector::erase(begin())`**: the eviction moves W vector *handles*
  (pointer-sized moves, not payload bytes) once per frame — O(W) trivial work against a socket
  write per frame; the plain vector keeps `accept_pending_async`'s contract (contiguous
  `payloads[0:upto]`) untouched across all six callers.
