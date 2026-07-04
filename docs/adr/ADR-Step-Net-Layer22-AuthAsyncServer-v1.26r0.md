# ADR — Netcode layer 22: authenticated + async server — identity binding merged with the downstream hygiene (`broadcast_auth_async`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

The bidirectional server arc grew two independent axes on top of the **join-order** `SeatPolicy`:
layer 19 (`broadcast_bound_async`) added the layer-16 **async downstream hygiene** (non-blocking
per-client send buffers + byte-cap drop-slowest + liveness reap), and layer 20 (`broadcast_bound_catchup`)
added **late-join windowed catch-up**. Layer 21 (`broadcast_auth`) then made the client→aircraft binding
a function of the client's **identity** (a `HELLO-001` credential looked up in a `CredentialTable` to a
designated seat) — but only as a sibling of the **blocking** base (`broadcast_bound`). So authentication
and the production-grade downstream hygiene **could not be used together**: an authenticated server still
back-pressured on one slow client and never shed a silently-dead peer. Every bidirectional ADR named this
merge as scope; layer 21's honest-scope note called it out explicitly: *"Merging identity binding with the
layer-16/19 async/byte-cap/liveness hygiene … is the natural follow-up — the same orthogonal axes the
codebase composes one layer at a time (18 → 19 → 20). Authentication is independent of all three, so those
merges are mechanical."*

Layer 22 is that merge for the async axis — the **18 → 19** step re-run on the authenticated server
(**21 → 22**). The determinism stake is unchanged: the authoritative kernel's output must stay a pure
function of the canonically-ordered, **authenticated + authorized** command SET, invariant to which
identity connected in which order, to upstream reorder/chunking, AND to any downstream cap/liveness drop.

## Decision

### `broadcast_auth_async` — a sibling of both `broadcast_auth` and `broadcast_bound_async`

`src/net/authasyncserver.{h,cpp}` adds `broadcast_auth_async`: **`broadcast_bound_async`'s async
`select_rw()` loop** (boundasyncserver.cpp — non-blocking per-client send buffers, opt-in byte-cap,
liveness reap) with the **join-order seat assignment replaced by `broadcast_auth`'s identity handshake**
(authserver.cpp). It is a **SIBLING** — it owns its own `AuthAsyncClient` struct (layer 19's
`BoundAsyncClient` exactly: socket + upstream `StreamReassembler` + downstream send buffer + the three
liveness fields + `seat`) and its own `flush_client`/`over_cap`/`enqueue_bytes`/`drop_client`/`reap_dead`
helpers. `broadcast.cpp`, `broadcast_bound`, `broadcast_bidi`, `broadcast_bound_async`,
`broadcast_bound_catchup`, and `broadcast_auth` are all **byte-for-byte untouched** (their bridges still
pass). **`CredentialTable`, `seat_authorizes`, `HELLO-001`, and `BIND-001` are reused verbatim** — the
only composition is *which two mechanisms run in the same loop*. **No shared-file change at all**:
`inputserver.h`'s `Stats` already carries every field this layer reports (`joins`, `leaves`,
`cmds_ok/unauth/stale/oob`, and — from layer 16 — `capped`/`reaped`), exactly as layers 19 and 21 each
needed none.

The three composed properties, all pure **transport**:

**1. Identity seat (layer 21).** On accept the server reads the client's FIRST upstream framing frame,
a `HELLO-001` credential (`[version 0x01] [ZigZag+LEB128 token]`), bounded by `accept_deadline_ms`;
`CredentialTable::authenticate(token)` resolves it to its **designated** seat (if free) or **SPECTATOR**
(unknown token or double-login). The seat is a function of the roster, invariant to join order. A leaver —
clean EOF, malformed framing, fatal flush, byte-cap shed, OR liveness reap — funnels through `drop_client`,
which calls `CredentialTable::release`, so a reconnecting identity reclaims **its own** seat.

**2. BIND + authorization (layers 18/21), through the async buffer.** The resolved seat's one-time
`BIND-001` record is **enqueued as the first bytes of the client's downstream send buffer** (before any
snapshot) — unlike layer 21's blocking `send_all`, it rides the same non-blocking userspace buffer as
every frame and the async flush delivers it first. A decoded command is submitted to the `CommandQueue`
ONLY when `seat_authorizes(seat, aircraft)`; a foreign-aircraft command or any spectator command is
dropped (`cmds_unauth`) — byte-identical to never arriving.

**3. Async hygiene (layers 16/19).** Non-blocking sends + per-client send buffers (no client
back-pressures the loop) + opt-in `cap_bytes` byte-cap drop-slowest (`capped`) + `liveness_frames` reap of
a client that makes no receive progress (`reaped`). A shed/reaped client's seat is freed (it is a leave).

### Why the composition preserves determinism

The two axes touch **disjoint** machinery. Authentication is an **admission filter** on the upstream
(which identity holds which seat, plus per-seat authorization) — the same `OUT_OF_RANGE` determinism class
as the join-order policy it replaces. The hygiene is purely the **downstream transport** (WHEN each
client's bytes move, WHICH slow/dead clients are shed). Neither touches the `CommandQueue`'s canonical
selection. So when N authenticated clients each upstream ONLY their own seat's commands and their union is
the whole scenario command set (delivered before each `apply_tick`), the produced frames are
**byte-identical to `session::build_server_frames`** — regardless of which identity connected in which
order, the upstream reorder/chunking, OR any downstream cap/liveness drop (a shed client changes nothing
about the frames or any surviving client's bytes; a foreign/unauthenticated command changes nothing at
all). The headline over layer 19: because the seat is bound by identity, a seat freed by a **byte-cap
shed or liveness reap** is reclaimed by its **own** identity — the join-order policy could only promise
*some* free seat.

### Honest scope (this cut)

* **The `HELLO-001` handshake read at accept is bounded-blocking** (`accept_deadline_ms`) — the one
  non-async touch, matching layer 21. All downstream sends (BIND + frames) are async. A mid-stream joiner
  that connects but stalls before sending its HELLO is bounded by that deadline (a cooperative client
  sends HELLO first thing, as the bridge does).
* **No late-join catch-up.** That is layer 20's axis (replay-depth), still on the join-order server. An
  *authenticated catch-up* server (`broadcast_auth` + async + catch-up) is the natural next rung —
  orthogonal (admission vs delivery vs replay-depth), mechanical the same way this layer was.
* **The credential is an opaque i64 in a roster** (a production system would carry a MAC/signature —
  orthogonal cryptography), unchanged from layer 21.

## Verification

* **Bridge `seads_netauthasync_test`** (ctest `netauthasync_bridge`, GCC + Clang native-x64 like layers
  7–21), over real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every
  client has sent its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — identity binding survives the async path (the merge anchor):** THREE clients each present a
    distinct token with **token order ≠ seat order** (100→2, 200→0, 300→1), learn their seat from `BIND`,
    and upstream ONLY that seat's commands (distinct scrambles: reversed @1 B, forward @7 B, reversed @3 B)
    while the server runs the FULL async path (`cap=0, liveness=0`). Each client's downstream — after its
    BIND — is **byte-identical to `build_server_frames`** (31 frames), each draws EXACTLY its designated
    seat, `cmds_ok == 6`, `cmds_unauth == stale == oob == capped == reaped == 0`, `joins == 3`.
  * **LEG 2 — authentication + authorization survive the async path:** TWO clients upstream the WHOLE
    scenario through the async path. Client A (valid token 200 → seat 0): foreign-aircraft commands dropped.
    Client B (unknown token 999 → spectator): ALL commands dropped. Both receive the **aircraft-0-only
    world byte-for-byte**. `cmds_ok == 3`, `cmds_unauth == 9` (A's 3 foreign + B's 6), zero hygiene drops.
  * **LEG 3 — a dead authenticated client is bounded without disturbing the sim (hygiene composes with
    identity):** on a long scenario (~4000 frames, ~1 MB downstream) through a pinned 16 KiB kernel send
    buffer, FAST (token 200 → seat 0, hook-drained) upstreams only its own seat's commands (scrambled) ⇒
    byte-identical downstream (after BIND) to the aircraft-0-only reference, its 3 commands drove the sim;
    DEAD (token 300 → seat 1, reads/sends nothing) is dropped by the policy under test (**sub-leg A**
    liveness reap at `cap=0` → `reaped=1`; **sub-leg B** byte-cap shed at `liveness=0` → `capped=1`) and
    its **seat freed** (`joins=2, leaves=1`), delivering `[BIND(seat 1) | strict frame-prefix]` (the async
    buffer sent the handshake first). Because seats are bound by identity, FAST is deterministically seat 0
    and DEAD seat 1 — no accept-order ambiguity (the layer-19 leg had to discover FAST's seat dynamically).
* **Property tests +5 ⇒ 255** (`test_authasync.py`): authentication is downstream-blind (the authenticated
  + authorized set, hence the produced frames, is independent of any cap/liveness value; an unknown-token
  spectator contributes nothing); authenticated frames are order-invariant; a credential seat is freed on
  ANY drop reason (EOF/flush/capped/reaped) and reclaimed by its **own** identity under a randomised
  join/mixed-drop interleaving (the headline vs the layer-19 join-order model); BIND-001 is the first
  downstream record so a shed client is a strict byte-prefix of `[BIND | frames]`; a dead authenticated
  client's drop never changes a survivor's bytes nor the produced frames.
* **Gates:** full **ctest 27→28** (GCC + Clang, all sealed net bridges still reconstruct their digests, the
  new `netauthasync_bridge` PASS on both); property tests **250→255**; **all 15 goldens byte-identical**
  (Sphere `6914a994…` via `seads_golden` on GCC + the Python reference); rails/roster (SPEC MONOTONE) +
  det_math oracle PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/authasyncserver.{h,cpp}`, `src/net/netauthasync_test_main.cpp`,
`tests/property/test_authasync.py`, this ADR; MODIFIED `CMakeLists.txt` (`authasyncserver.cpp` into
`seads_netinput`, `seads_netauthasync_test` target, `netauthasync_bridge` ctest). **No shared-file change**
(no `Stats`/accessor edit — layers 16 + 18 already added `capped`/`reaped`/`cmds_unauth`; `HELLO-001` /
`CredentialTable` / `seat_authorizes` reused verbatim). guardian.yml UNCHANGED (ctest-only bridge, like
layers 13–21). No new `_ref.py` (the reference is `auth_ref.py`'s `CredentialTable` + `input001_ref` /
`framing_ref` / `bound_ref`, reused).

## Alternatives rejected

* **Jump straight to authenticated catch-up (fold identity onto layer 20).** Collapses two rungs and one
  over-broad bridge. The codebase composes one axis per layer (18 → 19 → 20); mirroring that on the
  authenticated side (21 → 22 → 23) keeps each change small and each determinism bridge focused.
* **Edit `broadcast_bound_async` to take a `CredentialTable`.** Would fold the HELLO handshake + identity
  lookup into the loop layers 19/20 depend on, for zero benefit to them. The sibling keeps that loop
  byte-for-byte, matching the one-axis-per-layer discipline (exactly as layer 21 kept `broadcast_bound`).
* **Reuse layer 21's blocking BIND `send_all` in the async server.** A blocking handshake send on a
  non-blocking accepted socket back-pressures the loop and contradicts the whole point of the async axis.
  Enqueuing the BIND as the first downstream bytes (layer 19's approach) delivers it first through the same
  FIFO send buffer — identical ordering, no back-pressure.
* **Add a new `Stats` field for "capped/reaped by identity".** The drop reason is already counted
  (`capped`/`reaped`); the seat's fate is bookkeeping inside `drop_client`. A per-identity breakdown adds
  surface for no determinism-relevant fact — the bridge pins seat-freed + reclaim directly.
