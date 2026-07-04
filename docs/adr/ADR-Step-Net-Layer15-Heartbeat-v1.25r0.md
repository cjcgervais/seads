# ADR — Netcode layer 15a: heartbeat / liveness-timeout LEAVE (`liveness_frames`, no-seal, rides ATM-Sphere v1.25r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.25r0** (transport-only, no reseal)

## Context

Every LEAVE through layer 14 is **explicit**: a client sends a clean TCP EOF (`recv == 0`), a
`send` fatally errors (peer gone), or layer 12 sheds it by backlog **size** (`cap_bytes`). None of
these fire for a **silently-dead** client — a process killed with the connection half-open, or a
network partition. TCP delivers no EOF for minutes (keepalive / retransmit timeout), so from the
server's single `select()` such a client is:

* **not readable** — no data arrives and no EOF byte has been delivered yet, so `reap_leavers_async`
  never probes it;
* **not writable** — once it stopped reading, its kernel + our userspace buffers filled, so
  `flush_writable` makes no progress on it;
* **not over any cap** when `cap_bytes == 0` — so on an **open-ended live stream** (layer 13) its
  userspace backlog grows without bound, *forever*.

Layer 12's `cap_bytes` almost covers it — a dead client's backlog does eventually cross a positive
cap and it is shed. But that conflates two different failures: a client that is *behind* (backlog
large but still draining) and a client that is *gone* (draining nothing). A large cap tolerates the
first; only a **staleness** signal catches the second promptly, and at `cap_bytes == 0` nothing
catches it at all. Every handoff since layer 9 carried "a heartbeat/timeout LEAVE for silently-dead
clients" as the named free pick; this is it.

## Decision

### `liveness_frames` on `broadcast_live`

One new defaulted parameter (`std::size_t liveness_frames = 0`; **0 = no liveness reaping =
layer-14 behavior exactly**). A client that makes **no receive progress** — drains zero bytes to
the kernel **and** stays pending — for more than `liveness_frames` **consecutive produced frames**
is presumed dead and **reaped**: dropped and counted in the new `Stats.reaped` (and, being a live
member, also in `leaves`, exactly like a layer-12 cap drop).

### The liveness signal — cumulative kernel-accepted bytes

`BufClient` gains `sent_total` (the running count of bytes the client's kernel has accepted,
accumulated in `flush_client`), plus `idle_frames` and `last_sent`. Once per produced frame, after
that frame is enqueued/flushed, `reap_dead_async` runs:

```
if (!c.pending() || c.sent_total > c.last_sent) {   // fully drained OR bytes moved this frame
    c.idle_frames = 0;                               // progress: the deadline restarts
    c.last_sent = c.sent_total;
} else if (++c.idle_frames > liveness_frames) {      // silent past the deadline
    drop_client(...); ++st.reaped;                   // reap (also a leave)
}
```

The reset on `!pending()` (fully caught up) or `sent_total` advancing means a **slow-but-alive**
client — one draining *any* bytes each frame — is **never** reaped; only a client that has stopped
receiving entirely accumulates `idle_frames` to the deadline. `sent_total` is inert (a cheap
counter) when the policy is off — `reap_dead_async` early-returns on `liveness_frames == 0` and
never reads the liveness fields, so layers 13/14 are bit-for-bit unchanged.

### Orthogonal to layer 12 (staleness vs size)

`cap_bytes` bounds a client's backlog by **size** (drop-slowest); `liveness_frames` reaps a stalled
client by **staleness** (drop-dead). They compose: a dead client is now bounded **even at
`cap_bytes == 0`** — its backlog grows for at most `liveness_frames + O(buffer)` frames, then it is
reaped. The bridge proves the reap leg at `cap_bytes == 0` precisely to isolate the new signal.

### Frame-denominated, live-path only

* **Frame count, not wall-clock** — doctrine forbids reading time in the transport loop; the caller
  expresses "T seconds silent" as `T × 20` frames (near-uniform 20 Hz snapshots), exactly as
  layer 14's window is frame-denominated.
* **`broadcast_live` only** — the open-ended path is where a silent client is unbounded. The batch
  `broadcast_async` / `broadcast_select` broadcast a finite precomputed stream with a bounded,
  fail-not-wedge drain phase already, so a dead client there is bounded by the stream size; adding
  the knob there would change sealed-bridge-tested functions for no gain.

### Honest scope

**Which** frame a dead client crosses the threshold is OS-timing — how many frames its kernel and
userspace buffers absorb before `send_some` stalls — exactly like layer 12's shed frame, and
deliberately unasserted. The deterministic claims are: a never-reading client **is** reaped within
a bounded number of frames (backlog bounded), and a keeping-up client is **never** reaped.

## Verification

* **Bridge `seads_netheartbeat_test`** (ctest `netheartbeat_bridge`, native-x64 CI legs like
  layers 7–14):
  * **LEG 1** (reap at `cap_bytes == 0`) — a ~7.5 MiB synthetic stream pulled from a live
    `FrameSource` through a pinned tiny kernel send buffer, to TWO clients, `cap_bytes == 0`
    (so **only** liveness can drop anyone). FAST is drained by the `on_frame` hook every frame
    (the netcap pattern — never park the producer on a consumer it is the only flusher for) ⇒
    `sent_total` advances, never reaped, receives the whole stream byte-identically. DEAD reads
    nothing ⇒ `reaped == 1`, `leaves == 1`, `capped == 0`; its delivered bytes are a strict
    byte-**prefix** of the encoded stream; the stream finishes un-wedged.
  * **LEG 2** (healthy-path immunity) — the sealed SESSION-SK-001 stream, live from
    `session::FrameProducer`, to a continuously-reading EARLY client with an **aggressive**
    `liveness_frames == 1`. `reaped == 0`, `leaves == 0`; EARLY receives all frames and
    reconstructs the sealed digest `966aca05…`. Enabling the policy on a client that keeps up
    changes nothing.
* **Property tests +2 ⇒ 205** (`test_broadcast.py` layer-15a section): `live_deliver_liveness`
  mirrors `flush_client` + `reap_dead_async` — a client that stops receiving at frame *d* (with
  more than `liveness` silent frames after) is reaped and its delivery is a clean byte-prefix,
  strictly short of the whole; `liveness == 0` delivers everything (disabled bit-for-bit); and a
  client whose kernel always accepts everything is never reaped by any deadline (the mirror of the
  layer-12 healthy-client immunity test).
* **Demo:** `seads_netserver [port] [n] [catchup] [async] [cap_bytes] [live] [window] [liveness]` —
  the eighth positional arg; live-path only (rejected otherwise).

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes, or tuning
touched ⇒ all 15 goldens byte-identical, no digest moved, no seal.** ctest 19→20 GCC + Clang;
guardian.yml gains the layer-15a bridge step (native x64 legs, like layers 7–14).

## Alternatives rejected

* **An application-level PING/PONG heartbeat.** The broadcast is one-way server→client by design
  (unexpected client→server bytes are ignored); a bidirectional ping would invent a client-upstream
  protocol (that is layer 15b, input upstreaming — a separate, larger step) and add a message the
  wire does not carry. Receive-progress is the liveness signal already available server-side with no
  protocol change.
* **A time-denominated timeout (last T seconds silent).** Requires wall-clock in the transport loop
  — doctrine forbids it; the caller expresses T seconds as T×20 frames.
* **Reusing `cap_bytes` alone.** A cap conflates *behind* with *gone*: it tolerates a large-but-
  draining backlog only by raising the threshold, which then also delays shedding a dead client,
  and at `cap_bytes == 0` never sheds one. Staleness is the orthogonal quantity.
* **A backlog-bytes staleness signal (reap when `pending_bytes` stops shrinking).** Fragile under
  the steady-state where each frame adds ≈ what drains (backlog constant but the client is alive);
  cumulative `sent_total` is monotone and unambiguous — a live client's advances every frame.
* **Reaping in `broadcast_async` too.** Batch mode's finite stream + bounded drain already bounds a
  dead client; the knob would touch sealed-bridge-tested functions for zero benefit.
