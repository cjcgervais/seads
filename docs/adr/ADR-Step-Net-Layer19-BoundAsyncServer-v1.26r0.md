# ADR — Netcode layer 19: bound + async server — seat binding meets downstream hygiene (`broadcast_bound_async`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Two of the last three netcode layers grew from `broadcast_input` (layer 15b) along **orthogonal axes**:

* **Layer 16 (`broadcast_bidi`)** added the DOWNSTREAM output hygiene layers 11/12/15a had already
  proven on the fan-out path — per-client userspace send buffers (async output; no slow client can
  back-pressure the loop), an opt-in byte-cap (drop-slowest), and a liveness timeout (reap a silently-
  dead peer). But it left the client→aircraft binding positional and unauthenticated.
* **Layer 18 (`broadcast_bound`)** added the UPSTREAM binding — a join-order `SeatPolicy`, a `BIND-001`
  handshake, and per-seat authorization — so each client flies its own aircraft. But it kept
  `broadcast_input`'s BLOCKING `send_all` downstream: one slow client back-pressures the whole
  broadcast, and a silently-dead peer is never shed.

Both ADRs named the merge as the next step. Layer 16's: *"merging the binding with the layer-16
async/byte-cap/liveness hygiene is the natural follow-up — the two are orthogonal axes."* Layer 18's
rejected-alternatives list: *"the follow-up is a bound + async server plus, on top of it, bidirectional
late-join catch-up (now unblocked)."* Layer 19 is that product: **a bound server with the async
downstream hygiene** — the first server that is simultaneously multiplayer-safe upstream and
back-pressure-safe downstream.

The determinism stake is unchanged and is exactly why the merge is safe: the two axes touch **disjoint
machinery**. Binding is an admission filter on the upstream (plus downstream bookkeeping); hygiene is
purely downstream delivery. Neither touches the `CommandQueue`'s canonical selection, so the kernel's
output stays a pure function of the AUTHORIZED command SET.

## Decision

### `broadcast_bound_async` — a sibling of both `broadcast_bound` and `broadcast_bidi`

`src/net/boundasyncserver.{h,cpp}` adds `broadcast_bound_async`: `broadcast_bidi`'s async `select_rw()`
loop (bidiserver.cpp) with `broadcast_bound`'s three binding additions folded in (boundserver.cpp). It
is a **SIBLING** — it owns its own `BoundAsyncClient` struct (`broadcast_bidi`'s `BidiClient` — upstream
`StreamReassembler` + downstream `buf`/`off` send buffer + the three liveness fields — **plus** a
`seat`) and its own `flush_client`/`over_cap`/`enqueue_bytes`/`drop_client`/`reap_dead` helpers. The
sealed `broadcast.cpp` (layers 11/12/15a), `broadcast_bound`, and `broadcast_bidi` are **byte-for-byte
untouched** (all their bridges still pass). No new types, `Stats` fields, or accessors were needed —
layer 16 already added `Stats.capped`/`reaped`, layer 18 already added `Stats.cmds_unauth` and
`InputProducer::n_aircraft()`. This layer is purely a new `.cpp`/`.h` pair + its bridge + CMake wiring.

The merge is mechanical but has exactly three integration seams, all present in the source siblings:

**1. The seat rides the async client, freed on EVERY drop path.** `broadcast_bidi`'s `drop_client`
closes + erases + counts a leave; the bound-async `drop_client` first calls `seats.release(seat)`. Every
leave route funnels through it — clean EOF / malformed framing (readable handler), a fatal flush
(writable handler + the drain tail), a byte-cap shed, and a liveness reap — so a seat is returned to the
pool no matter *why* the client left. (In `broadcast_bound` only EOF and a fatal `send_all` could drop a
client; the async server adds the cap and reap drop routes, and each frees the seat.)

**2. `BIND-001` is ENQUEUED as the first downstream bytes, not blocking-sent.** In `broadcast_bound`
the handshake is a blocking `send_all` at accept time. Here the accepted socket goes non-blocking first
(the async invariant — nothing may wedge the loop), so the BIND record is `enqueue_bytes`'d into the
client's send buffer as its FIRST bytes, before any frame is ever enqueued. Because the send buffer is a
FIFO drained in order, the async flush delivers the BIND first — the same ordering guarantee, achieved
through the userspace buffer instead of a blocking write. A fatal flush of the handshake drops the
joiner (seat returned) before it becomes a member.

**3. Per-seat authorization, unchanged from layer 18.** `submit_payloads` takes the sender's seat and
submits a decoded command only when `seat_authorizes(seat, aircraft)` (`seat >= 0 && aircraft == seat`);
a foreign-aircraft command — or any command from a spectator — is dropped (`cmds_unauth`), byte-
identical to it never arriving.

### Why the merge preserves determinism

The two axes are independent by construction:

* **Authorization** can only *reject* upstream commands, and a rejected command is indistinguishable
  from one that never arrived — the `OUT_OF_RANGE` reject's determinism class. Seat assignment and the
  BIND record touch only bookkeeping and the downstream.
* **Hygiene** decides only WHEN each client's bytes move and WHICH slow/dead clients are shed. A shed
  client changes nothing about the produced frames nor any surviving client's bytes (layers 12/15a's
  invariant, re-proven here).

Neither touches the `CommandQueue`. So when N clients each upstream ONLY their own seat's commands and
their union is the whole scenario command set (delivered before each `apply_tick`), the produced frames
are **byte-identical to `session::build_server_frames`** — regardless of the seat permutation, the
upstream reorder/chunking, OR any downstream cap/liveness drop; a foreign command changes nothing at all.

### Honest scope

* **No late-join catch-up** — still inherited. The binding it needs is settled (layer 18), so a bound
  catch-up is genuinely UNBLOCKED (a replayed joiner would get a seat + BIND, then the catch-up prefix),
  but it is a distinct layer, not this cut. The async server owns per-client send buffers, which is the
  machinery a catch-up prefix enqueue would build on — the door is open.
* **Unauthenticated.** Seat is still join-order position, not identity.

## Verification

* **Bridge `seads_netboundasync_test`** (ctest `netboundasync_bridge`, native-x64 leg like layers 7–18),
  over real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until clients have
  sent), finite watchdog:
  * **LEG 1 — binding survives the async path (the merge anchor):** THREE clients each learn their seat
    from `BIND` and upstream ONLY that seat's commands (distinct scrambles: reversed @1 B, forward @7 B,
    reversed @3 B) while the server runs the FULL async path (`cap_bytes=0, liveness_frames=0`). Each
    client's downstream — after its BIND record — is **byte-identical to `build_server_frames`**,
    regardless of the seat permutation. Distinct BIND seats in `[0,3)`; `cmds_ok == 6`,
    `unauth == stale == oob == capped == reaped == 0`, `joins == 3`. (Layer-18 LEG 1 through the async
    send path.)
  * **LEG 2 — authorization survives the async path:** ONE client seated at 0 upstreams the WHOLE
    scenario's commands; only its own-seat commands take effect (`cmds_ok == 3`, `cmds_unauth == 3`), so
    the downstream is the **aircraft-0-only world byte-for-byte**, through the async buffers.
  * **LEG 3 — a dead client is bounded WITHOUT disturbing the sim (hygiene composes with binding):** TWO
    clients on a LONG (~1 MB downstream, 20 000-tick) scenario through a pinned 16 KiB kernel send
    buffer. FAST is seated, reads its BIND, upstreams ONLY its own seat's commands (scrambled), and is
    drained every frame by the `on_frame` hook ⇒ never dropped, its downstream (after BIND)
    **byte-identical to the reference for the seat it drew**, its own-seat commands drove the sim
    (`cmds_ok == n_own`, `unauth == 0`). DEAD connects, reads/sends nothing ⇒ dropped by the policy under
    test and its **seat freed** (`leaves == 1`): **sub-leg A** liveness reap at `cap=0`
    (`reaped == 1, capped == 0`), **sub-leg B** byte-cap shed at `liveness=0` (`capped == 1,
    reaped == 0`). DEAD's delivered bytes are its **BIND record followed by a strict prefix** of the
    frame stream — the async buffer delivered the handshake FIRST. The two runs independently drew FAST
    into seats 1 and 0 (accept-order nondeterminism), and the per-seat reference matched each — the leg
    is robust to it by construction, like layer-18 LEG 1.
* **Property tests +5 ⇒ 237** (`test_boundasync.py`, composing `bound_ref` binding with the
  `test_bidi` downstream delivery models): authorization is downstream-blind (the authorized set + the
  produced frames are a function of the seat-partitioned upstream only, independent of any cap/liveness
  knob); the produced frames are order-invariant on the authorized set; a seat is freed on ANY drop
  reason (EOF / fatal flush / capped / reaped, under a randomised join/mixed-drop interleaving — live
  seats always distinct + lowest-free, released reused); the `BIND-001` record is the first downstream
  record so a dropped client is `[BIND | strict frame-prefix]`; and a dead client's reap never changes a
  survivor's bytes nor the produced frames.
* **Gates:** full **ctest 24→25** (GCC + Clang, all sealed net bridges still reconstruct `966aca05…`,
  the new `netboundasync_bridge` PASS on both); property tests **232→237**; **all 15 goldens
  byte-identical** (Sphere `6914a994…` via `seads_golden`); determinism lint + det_math oracle + rails
  monotone + tuning + ceiling probes PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed
session/event digests unmoved, no seal.** Diff: NEW `src/net/boundasyncserver.{h,cpp}`,
`src/net/netboundasync_test_main.cpp`, `tests/property/test_boundasync.py`; MODIFIED `CMakeLists.txt`
(`boundasyncserver.cpp` into `seads_netinput`, `seads_netboundasync_test` target, `netboundasync_bridge`
ctest). No Python ref, no `Stats`/accessor change (layers 16 + 18 already added everything needed).
guardian.yml UNCHANGED (ctest-only bridge, like layers 13–18).

## Alternatives rejected

* **Edit `broadcast_bound` or `broadcast_bidi` in place to gain the other axis.** Would fold seats +
  handshake into `broadcast_bidi`'s bridge-tested loop, or the async buffers into `broadcast_bound`'s,
  changing code layers 16/17/18 depend on for zero benefit to them. The sibling keeps every prior loop
  byte-for-byte, matching the one-axis-per-layer discipline the whole netcode arc uses.
* **Deliver `BIND-001` with a blocking `send_all` (as layer 18 does) inside the async server.** A
  blocking write on a client the loop must not wait on violates the async invariant — one unresponsive
  joiner would wedge the accept. Enqueueing the BIND as the first buffered bytes gives the identical
  "BIND before any frame" ordering through the non-blocking machinery; the bridge's `[BIND | prefix]`
  assertion proves the ordering holds even for a client that is later shed mid-stream.
* **Free the seat only in `reap_dead`/the cap path, trusting EOF elsewhere.** A seat must be returned on
  *every* leave or the async drop routes (cap/reap) would strand seats the blocking server never had.
  Centralising the release in `drop_client` (which every route calls) makes it impossible to add a leave
  route that forgets the seat; the "seat freed on any drop reason" property pins it.
* **Bundle late-join catch-up into this cut.** Catch-up is its own axis (prefix replay) and is now
  unblocked; bundling it would double the bridge surface and repeat the mistake layer 15b avoided by
  staging. Deferred, named as scope — it is the natural next layer on top of this one.
