# ADR — Netcode layer 18: multi-client seat binding — each client its own aircraft (`broadcast_bound`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Every bidirectional layer since 15b flagged the same deferral in its honest-scope note: the
**client→aircraft binding was POSITIONAL and UNAUTHENTICATED**. Concretely, `broadcast_input`
(layer 15b) and `broadcast_bidi` (layer 16) store a connected client as nothing but a socket in a
vector — there is **no association between a socket and an aircraft**. A decoded `INPUT-001` command
carries an `aircraft` index, and the `CommandQueue` accepts it as long as that index is in
`[0, n_aircraft)` (it rejects only `OUT_OF_RANGE`). So any connected client can steer *any* aircraft,
and a client has no way to learn which aircraft is meant to be "its own" — the single scripted test
driver simply sends every aircraft's commands and it happens to work. That is adequate for a
determinism bridge; it is not a multiplayer server.

Layer 16's ADR named this explicitly as the blocker for the rest of the arc: *"a bidirectional
catch-up must first settle the positional client→aircraft binding … who a replayed joiner is allowed
to command."* This layer settles the binding itself — the load-bearing prerequisite.

The determinism stake is unchanged: the authoritative kernel's output must stay a pure function of the
canonically-ordered command SET. A binding is an **admission filter** on the upstream (the same class
as the existing `OUT_OF_RANGE` reject) plus a deterministic seat assignment and a downstream
handshake — none of which may make the frames depend on transport order/chunking.

## Decision

### `broadcast_bound` — a sibling of `broadcast_input`

`src/net/boundserver.{h,cpp}` adds `broadcast_bound`: `broadcast_input`'s single-thread `select()`
loop (blocking `send_all` downstream — see scope) with three transport additions, each outside the
world_hash. It is a **SIBLING** — it owns its own `BoundClient` struct (a `broadcast_input` `RxClient`
plus a `seat`). `broadcast_input` and `broadcast_bidi` are **byte-for-byte untouched** (their bridges
still pass). `inputserver.h` gains two purely additive things: `Stats.cmds_unauth` (0 for
`broadcast_input`/`bidi`, which never check it) and an inline `InputProducer::n_aircraft()` accessor
used to size the seat pool.

**1. Join-order `SeatPolicy`.** `assign()` hands each joiner the LOWEST free aircraft index in
`[0, n_aircraft)`; when every seat is taken, further joiners are **SPECTATORS** (seat `-1`,
receive-only). `release(seat)` frees a seat on leave, so the lowest free seat is reused. Pure integer
bookkeeping — a deterministic function of the join/leave event order (a transport fact, not a sim
one). Mirrored in `tools/bound_ref.SeatPolicy`.

**2. `BIND-001` handshake.** Right after accept, the server sends the client a one-time `BIND-001`
record (`src/net/bind001.{h,cpp}` ↔ `tools/bound_ref.py`) naming its seat + the world size, as the
FIRST framing frame on its downstream, before any snapshot. The client no longer *guesses* its
aircraft — it is TOLD. Wire:
`BIND-001 = [version 0x01] [ZigZag+LEB128 seat] [ZigZag+LEB128 n_aircraft]`, `seat = -1` for a
spectator (ZigZag carries the sign, as `last_hit_by` does on WEAPON-001). The two integers reuse the
sealed `geo001::{encode_i64,decode_i64}` pipeline ⇒ **no new primitive, no det_math**.

**3. Upstream authorization.** A client's decoded command is submitted to the `CommandQueue` ONLY when
it names the client's own seat — `seat_authorizes(seat, aircraft) := seat >= 0 && aircraft == seat`.
A command for any other aircraft, or any command from a spectator, is DROPPED (`cmds_unauth`), which
is byte-identical to it never arriving.

### `BIND-001` is transport metadata, NOT a sealed wire

The determinism claim of layer 18 rests on the **authorization filter**, not on this record. `BIND-001`
carries no simulation value, feeds no hash, and is not a `rails.wire` block — it is modelled on the
**layer-7 framing envelope** (which likewise has a Python ref + a byte-pin but is not a sealed rail
and took no seal). So this layer takes **no seal** even though it adds a wire, exactly as the framing
envelope did. Cross-impl byte parity is still pinned (shared known-encoding vector in `bound_ref.py`
and the bridge) because the codebase pins every wire — the discipline, not a seal trigger.

### Why the binding preserves determinism

Authorization is an admission filter on the upstream: it can only *reject* commands, and a rejected
command is indistinguishable from one that never arrived — precisely the `OUT_OF_RANGE` reject's
determinism class. The seat assignment and `BIND-001` record touch only bookkeeping and the
downstream. So when N clients each upstream ONLY their own seat's commands and their union is the whole
scenario command set (delivered before each `apply_tick`), the produced frames are **byte-identical to
`session::build_server_frames`**, regardless of which client drew which seat or how the upstream bytes
were reordered/chunked; a foreign-aircraft command changes nothing.

### Honest scope (this first binding cut)

* **Blocking downstream.** `broadcast_bound` uses `broadcast_input`'s blocking `send_all` base (a
  cooperative client reads concurrently). Merging the binding with the layer-16 async/byte-cap/liveness
  downstream hygiene (`broadcast_bidi`) is the natural follow-up — the two are **orthogonal axes**
  (binding is upstream admission; hygiene is downstream delivery), and the codebase builds one axis per
  layer. This is the same staging layer 15b→16 used (15b blocking, 16 added hygiene).
* **No late-join catch-up** — inherited; now *unblocked* (the binding it needed is settled), but out of
  this cut.
* **Unauthenticated.** The binding is join-order with no identity — it enforces "one client, one
  aircraft", not "who you are". Seat = position, still.

## Verification

* **Bridge `seads_netbound_test`** (ctest `netbound_bridge`, native-x64 leg like layers 7–17), over
  real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every client has
  sent its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — per-seat authorization (the multiplayer leg):** THREE clients each connect, learn their
    seat from the `BIND` handshake, and upstream ONLY that seat's commands (distinct scrambles: reversed
    @1 B/send, forward @7 B, reversed @3 B). Their union is the whole INPUT-SK-001 command set, so each
    client's downstream is **byte-identical to `build_server_frames`** — regardless of the seat
    permutation the accept order produces (each client's rule is "given my seat, send that aircraft's
    commands", so the leg is robust to accept-order nondeterminism). Each `BIND` names a distinct seat
    in `[0,3)`, `n_aircraft == 3`; `cmds_ok == 6`, `cmds_unauth == stale == oob == 0`, `joins == 3`.
  * **LEG 2 — foreign-aircraft rejection (the authorization boundary):** ONE client seated at 0
    upstreams the WHOLE scenario's commands — its own AND every other aircraft's. The server accepts
    only its own-seat commands and DROPS the rest, so the downstream is **byte-identical to a world
    where only aircraft 0 was ever commanded** (the others hold neutral): the foreign commands changed
    NOTHING. `cmds_ok == 3` (aircraft 0's phases), `cmds_unauth == 3` (aircraft 1's + 2's), `oob == 0`;
    `BIND` seat `== 0`.
  * **LEG 3 — seat policy + `BIND-001` codec (in-process):** the join-order `SeatPolicy` (fill 0..n-1,
    then spectators; reuse the lowest free seat on release), the `seat_authorizes` predicate, and the
    `BIND-001` codec — a shared known-encoding pin `[0x01,0x02,0x06]` (seat 1, n 3) vs `bound_ref.py`,
    round-trip incl. the spectator `-1`, and a rejected wrong version byte.
* **Property tests +8 ⇒ 232** (`test_bound.py`): `BIND-001` round-trip (incl. spectator) + version
  enforcement + the known-encoding pin; the `SeatPolicy` under a randomised join/leave interleaving
  (live seats always distinct, always the lowest free, released seats reused, policy state matches a
  shadow set); the `seat_authorizes` predicate; and the authorization *composition* — a per-seat
  partition of a command set accepts exactly the whole set while any foreign command drops.
* **Gates:** full **ctest 23→24** (GCC + Clang, all sealed net bridges still reconstruct `966aca05…`,
  the new `netbound_bridge` PASS on both); property tests **224→232**; **all 15 goldens byte-identical**
  (Sphere `6914a994…` via `seads_golden` on GCC + Clang); determinism lint + the four probe gates PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed
session/event digests unmoved, no seal.** Diff: NEW `src/net/bind001.{h,cpp}`,
`src/net/boundserver.{h,cpp}`, `src/net/netbound_test_main.cpp`, `tools/bound_ref.py`,
`tests/property/test_bound.py`; MODIFIED `src/net/inputserver.h` (`Stats` +`cmds_unauth`,
`InputProducer::n_aircraft()`), `CMakeLists.txt` (`bind001.cpp` + `boundserver.cpp` into
`seads_netinput`, `seads_netbound_test` target, `netbound_bridge` ctest). guardian.yml UNCHANGED
(ctest-only bridge, like layers 13–17).

## Alternatives rejected

* **Enforce the binding inside `CommandQueue`.** The queue is the canonical ordering contract — a pure
  function of the command SET, deliberately blind to sockets/seats. Authorization is a *who-sent-this*
  question the queue cannot see; it belongs in the server, which decides WHICH commands to submit. The
  queue stays pure.
* **Edit `broadcast_input`/`broadcast_bidi` to take a seat map.** Would fold seats + the handshake into
  the bridge-tested loops layers 15b–17 depend on, for zero benefit to them. The sibling keeps those
  loops byte-for-byte, matching the layer-15b/16 discipline.
* **Make `BIND-001` a sealed `rails.wire` block (and reseal).** It carries no sim state and feeds no
  hash — it is connection metadata, exactly the framing envelope's category, which took no seal. A
  reseal would misrepresent a transport handshake as a determinism-critical wire.
* **Merge the layer-16 async hygiene into this cut.** Binding (upstream admission) and hygiene
  (downstream delivery) are orthogonal; bundling both doubles the surface of one bridge and repeats the
  mistake layer 15b avoided by staging. Deferred, named as scope — the follow-up is a bound + async
  server plus, on top of it, bidirectional late-join catch-up (now unblocked).
* **Reuse seats across the whole run (no release).** Freeing a seat on leave is one bool and makes the
  policy match real churn; not reusing would strand seats and cap the server's lifetime capacity for no
  gain. Reuse is unit-tested (LEG 3 + the randomised property).
