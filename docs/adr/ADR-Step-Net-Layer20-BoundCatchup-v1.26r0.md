# ADR — Netcode layer 20: bidirectional late-join catch-up — bound + async + windowed replay (`broadcast_bound_catchup`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Every bidirectional layer since 15b named late-join catch-up as deferred, and layer 19
(`broadcast_bound_async`) declared it **doubly unblocked**: the client→aircraft binding is settled (a
replayed joiner gets a seat + BIND), *and* the async server already owns the per-client userspace send
buffers a catch-up prefix enqueue builds on. The download-only broadcast solved catch-up long ago —
layer 10 replays the missed prefix, layer 14 bounds that replay to the last `catchup_window` produced
frames (O(W) memory on a stream of any length). Layer 20 brings that machinery to the authoritative
bidirectional server: a client joining mid-fight is told its seat, replayed the frames it missed, then
fed the live stream — reconstructing (up to its window) the whole thing.

The determinism stake: catch-up is a **fourth orthogonal axis** beside the layer-18 upstream binding
(admission) and the layer-16 downstream hygiene (delivery). It decides only a joiner's **replay depth**
— how far back its prefix reaches. It touches neither the `CommandQueue`'s canonical selection nor which
bytes flow to any other client, so the merge is safe for the same reason layers 14 and 19 were.

## Decision

### `broadcast_bound_catchup` — a sibling of `broadcast_bound_async` with `broadcast_live`'s retention folded in

`src/net/boundcatchupserver.{h,cpp}` adds `broadcast_bound_catchup`: `broadcast_bound_async`'s async
`select_rw()` loop (boundasyncserver.cpp — seats + BIND + authorization + byte-cap + liveness) with
`broadcast_live`'s history-retention + windowed catch-up replay (broadcast.cpp) folded in. It is a
**SIBLING** — it owns its own `BoundCatchupClient` (identical to layer 19's `BoundAsyncClient`; catch-up
adds no per-client state — the retained history lives on the server) and its own
`flush_client`/`over_cap`/`enqueue_bytes`/`drop_client`/`reap_dead` helpers. The sealed `broadcast.cpp`,
`broadcast_bound`, `broadcast_bidi`, AND `broadcast_bound_async` are **byte-for-byte untouched** (all
their bridges still pass). The one shared-file change is purely additive: **`netinput::Stats` gains a
`trimmed` counter** (window evictions, the mirror of `netbcast::Stats.trimmed`) — the same additive
pattern by which layer 16 added `capped`/`reaped` and layer 18 added `cmds_unauth`.

Two things differ from layer 19, both lifted verbatim from `broadcast_live`:

**1. A `history` vector retains the produced payloads.** After each frame is enqueued to the current
members it is retained; when `catchup_window > 0` and the retained size exceeds the window the oldest is
evicted (`++Stats.trimmed`). `catchup_window == 0` retains all (layer-13 semantics, for a bounded run).

**2. `accept_all` replays the retained prefix after the BIND.** A client accepted at frame `fi` has its
`BIND-001` record enqueued first (layer 19), then the current `history` — `frames[max(0,fi-W):fi]` —
enqueued frame-by-frame through the same `enqueue_bytes`, before it enters the live stream at `fi`. So
its whole downstream delivery is `[BIND | frames[max(0,fi-W):fi] | live frames fi, fi+1, …]` =
`[BIND | frames[max(0,fi-W):]]`. A client present from the initial gather (history empty) receives
`[BIND | the whole stream]`. The byte-cap applies per replayed frame: a joiner whose replay backlog an
enqueue leaves above the cap is shed during replay (`++Stats.capped`) — never having become live, it is
**not** a join and **not** a leave, and its seat is returned (`broadcast_live`'s `accept_pending_async`
semantics, with the seat bookkeeping of `broadcast_bound_async`).

### Why the merge preserves determinism

The four axes are independent by construction. Authorization can only *reject* upstream commands (the
`OUT_OF_RANGE` determinism class); hygiene decides only WHEN bytes move and WHICH slow/dead clients shed;
catch-up decides only a joiner's REPLAY DEPTH. None touches the `CommandQueue`. So the produced stream
stays a pure function of the AUTHORIZED command SET, and **every client's delivery is a byte-exact window
of `[BIND | the produced stream]`**: when N seated clients each upstream only their own seat's commands
(their union the whole scenario), the produced stream is byte-identical to `session::build_server_frames`
and a mid-stream joiner is replayed a byte-exact contiguous suffix of it — regardless of seat
permutation, upstream reorder/chunking, any cap/liveness drop, or the replay window.

### Honest scope

* Catch-up retention is **O(catchup_window) frames** (`window == 0` = retain-all = O(stream), only for a
  bounded run — layer 14's boundary, inherited). Per-frame join service (accept + replay enqueue happen
  in the frame loop, one `select()` slice).
* The seat binding stays **UNAUTHENTICATED** (join-order position, not identity).

## Verification

* **Bridge `seads_netboundcatchup_test`** (ctest `netboundcatchup_bridge`, native-x64 leg like layers
  7–19), over real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until the
  seated clients have sent, `on_frame(kJoin)` holds frame `kJoin` until the joiner connects), finite
  watchdog:
  * **LEG 1 — catch-up window regimes (the headline):** THREE clients seated 0/1/2 each upstream ONLY
    their own seat's commands (scrambled + chunked) from the initial gather ⇒ the produced stream is
    **byte-identical to `build_server_frames`**. A FOURTH client joins mid-stream at frame `kJoin` as a
    SPECTATOR (seats full) and — across **W ∈ {1, kJoin/2, retain-all}** — receives EXACTLY
    `[BIND(spectator) | frames[max(0,kJoin-W):]]` (frame-aligned, no gap/duplicate), with
    `trimmed == max(0,N-W)`. The three seated clients are byte-identical to the WHOLE stream under every
    window; when W covers the whole prefix the spectator receives the whole stream. `joins == 4`,
    `leaves == capped == reaped == cmds_unauth == 0`.
  * **LEG 2 — authorization composes with catch-up:** the mid-stream SPECTATOR upstreams the WHOLE
    scenario's commands; a spectator authorizes nothing, so every one is dropped (`cmds_unauth == 6`,
    the whole set) and the produced stream is unchanged — yet the spectator STILL catches up the whole
    stream byte-for-byte. Catch-up did not weaken authorization; authorization did not weaken catch-up.
  * **LEG 3 — hygiene composes with catch-up:** a LONG (~1 MB, 20 000-tick) stream through a pinned
    16 KiB kernel send buffer, `cap_bytes = 128 KiB`. A seated FAST client (hook-drained, `min_initial=1`)
    is byte-identical to its seat's reference and its commands drove the sim; a NON-READING client joining
    mid-stream draws a distinct real seat, is replayed the catch-up prefix, and has its replay backlog
    shed by the byte-cap (`capped == 1`, `reaped == 0`) — its seat freed on the drop. Its delivery is
    `[BIND | strict prefix of the catch-up stream]`. Whether the cap trips *during* the replay
    (never a member: `joins == 1`) or just after the joiner becomes live (`joins == 2`, a capped leave)
    is an OS loopback-buffering detail — the same policy outcome; the leg asserts the platform-invariant
    `capped == 1 ∧ reaped == 0 ∧ leaves == joins-1 ∧ joins ∈ {1,2}`.
* **Property tests +5 ⇒ 242** (`test_boundcatchup.py`, mirroring the `boundcatchupserver.cpp`
  retention/window model on the `bound_ref` + `framing_ref` codecs): a joiner at frame `fi` is delivered
  exactly `frames[max(0,fi-W):]` (retain-all = the whole prefix); a window of W evicts exactly
  `max(0,N-W)` retentions over a full run; catch-up is downstream of authorization (a spectator's
  rejected commands never change the produced/retained/replayed frames); the delivery byte stream is
  `[BIND | catch-up prefix | live suffix]`; and a non-reading joiner's replay backlog is shed by the cap
  as a strict byte-prefix of `[BIND | frames]` while a cooperative joiner gets the whole stream.
* **Gates:** full **ctest 25→26** (GCC + Clang, all sealed net bridges still reconstruct their digests,
  the new `netboundcatchup_bridge` PASS); property tests **237→242**; **all 15 goldens byte-identical**
  (Sphere `6914a994…` via `seads_golden`); determinism lint + det_math oracle + rails monotone + ceiling
  probes PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/boundcatchupserver.{h,cpp}`,
`src/net/netboundcatchup_test_main.cpp`, `tests/property/test_boundcatchup.py`; MODIFIED
`src/net/inputserver.h` (additive `Stats.trimmed`), `CMakeLists.txt` (`boundcatchupserver.cpp` into
`seads_netinput`, `seads_netboundcatchup_test` target, `netboundcatchup_bridge` ctest). No new Python
ref (BIND-001 / INPUT-001 / SeatPolicy refs already exist; catch-up introduces no wire). guardian.yml
UNCHANGED (ctest-only bridge, like layers 13–19).

## Alternatives rejected

* **Edit `broadcast_bound_async` in place to add catch-up.** Would fold history retention + prefix replay
  into layer 19's bridge-tested loop for zero benefit to it. The sibling keeps every prior loop
  byte-for-byte, matching the one-axis-per-layer discipline the whole netcode arc uses.
* **Replay the catch-up prefix with a blocking burst (like layer 10's `catch_up_client`).** A blocking
  write on a client the async loop must not wait on violates the async invariant — a slow joiner's replay
  would wedge the accept. Enqueueing the prefix into the same FIFO send buffer as every frame (after the
  BIND) drains it on writability like any pending bytes, and lets the byte-cap shed a joiner whose replay
  backlog is too large — the `[BIND | strict prefix]` bridge assertion proves it.
* **Make the byte-cap NOT apply to the replay prefix.** Then a non-reading joiner could accumulate an
  unbounded catch-up backlog, defeating the very hygiene layer 19 added. Applying the cap per replayed
  frame (as `broadcast_live` does) keeps a joiner bounded before it ever becomes live.
* **Seal the retained history / window as a `rails.wire` block.** Catch-up introduces no new wire — it
  replays already-produced, already-encoded protocol-7 payloads. Like layers 10/13/14 it is pure
  transport (no seal); only the additive `Stats.trimmed` counter is new, and it is off-wire bookkeeping.
* **Bundle authenticated binding or remote-aircraft prediction into this cut.** Each is its own axis;
  bundling would double the bridge surface. Deferred, named as scope.
