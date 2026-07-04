# ADR — Netcode layer 23: authenticated + async + catch-up server — identity binding + downstream hygiene + windowed late-join catch-up (`broadcast_auth_catchup`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

The authenticated arc reached its async rung at layer 22 (`broadcast_auth_async`): the layer-21 **identity
binding** (a `HELLO-001` credential looked up in a `CredentialTable` to a designated seat) composed with
the layer-16/19 **async downstream hygiene** (non-blocking per-client send buffers + byte-cap drop-slowest
+ liveness reap). But an *authenticated* client joining mid-fight still received only the SUFFIX from its
accept point — it could never reconstruct the whole stream. The join-order side already solved exactly
this: layer 20 (`broadcast_bound_catchup`) folded `broadcast_live`'s windowed catch-up onto the join-order
async server. Layer 22's honest-scope note named the follow-up explicitly: *"An authenticated catch-up
server (`broadcast_auth` + async + catch-up) is the natural next rung — orthogonal (admission vs delivery
vs replay-depth), mechanical the same way this layer was."*

Layer 23 is that rung — the **19 → 20** step (bound+async → bound+async+catch-up) re-run on the
authenticated server (**22 → 23**), exactly as layer 22 was the **18 → 19** step re-run on it. It is the
LAST rung of the authenticated arc. The determinism stake is unchanged: the authoritative kernel's output
must stay a pure function of the canonically-ordered, **authenticated + authorized** command SET, invariant
to which identity connected in which order, to upstream reorder/chunking, to any downstream cap/liveness
drop, AND now to any joiner's replay window.

## Decision

### `broadcast_auth_catchup` — a sibling of both `broadcast_auth_async` and `broadcast_bound_catchup`

`src/net/authcatchupserver.{h,cpp}` adds `broadcast_auth_catchup`: **`broadcast_auth_async`'s async
`select_rw()` loop** (authasyncserver.cpp — identity accept + non-blocking per-client send buffers +
byte-cap + liveness reap) with **`broadcast_live`'s history-retention + windowed replay folded in** (the
same two additions layer 20 made to `broadcast_bound_async`, mirrored from boundcatchupserver.cpp). It is a
**SIBLING** — it owns its own `AuthCatchupClient` struct (layer 22's `AuthAsyncClient` exactly: socket +
upstream `StreamReassembler` + downstream send buffer + the three liveness fields + `seat`; catch-up adds
NO per-client state — the retained `history` lives on the server) and its own
`flush_client`/`over_cap`/`enqueue_bytes`/`drop_client`/`reap_dead` helpers. `broadcast.cpp`,
`broadcast_bound`, `broadcast_bidi`, `broadcast_bound_async`, `broadcast_bound_catchup`, `broadcast_auth`,
and `broadcast_auth_async` are all **byte-for-byte untouched** (their bridges still pass).
**`CredentialTable`, `seat_authorizes`, `HELLO-001`, and `BIND-001` are reused verbatim** — the only
composition is *which mechanisms run in the same loop*. **No shared-file change at all**: `inputserver.h`'s
`Stats` already carries every field this layer reports — `joins`, `leaves`, `cmds_ok/unauth/stale/oob`,
`capped`/`reaped` (layer 16), `cmds_unauth` (layer 18), AND `trimmed` (layer 20). Layer 20 was the one that
added `trimmed`; layer 23 inherits it, so — like layers 19, 21, 22 — it needs no `Stats` edit.

The composed properties, all pure **transport**:

**1. Identity seat (layer 21).** On accept the server reads the client's FIRST upstream framing frame, a
`HELLO-001` credential, bounded by `accept_deadline_ms`; `CredentialTable::authenticate(token)` resolves it
to its designated seat (if free) or SPECTATOR (unknown token or double-login), invariant to join order. A
leaver — clean EOF, malformed framing, fatal flush, byte-cap shed, OR liveness reap — funnels through
`drop_client → CredentialTable::release`, so a reconnecting identity reclaims **its own** seat.

**2. BIND + authorization through the async buffer (layers 18/21/22).** The resolved seat's one-time
`BIND-001` record is **enqueued as the first bytes** of the client's downstream send buffer; the async
flush delivers it first. A decoded command is submitted to the `CommandQueue` ONLY when
`seat_authorizes(seat, aircraft)`; a foreign/spectator command is dropped (`cmds_unauth`).

**3. Async hygiene (layers 16/19/22).** Non-blocking sends + per-client send buffers (no back-pressure) +
opt-in `cap_bytes` byte-cap drop-slowest (`capped`) + `liveness_frames` reap (`reaped`).

**4. Late-join catch-up (layer 20) — the new axis.** The server retains the produced payloads in a
`history` vector (the last `catchup_window`, or ALL when `window==0`; oldest evicted per new frame ⇒
`trimmed`). In `accept_all`, right after enqueueing the client's `BIND-001`, the retained prefix (the
current `history`, itself already windowed) is enqueued frame-by-frame, before the joiner enters the live
stream. So a client accepted at frame `fi` is delivered `[BIND | frames[max(0,fi-W):fi] | live frames fi,
fi+1, …]` = `[BIND | frames[max(0,fi-W):]]`. A client present from the initial gather (history empty)
receives `[BIND | the whole stream]`. The replay rides the SAME non-blocking send buffer (no
back-pressure), and the byte-cap applies per replayed frame — a joiner whose replay backlog trips the cap
is shed DURING replay (`capped`, seat returned), never a live member (not a join/leave).

### Why the composition preserves determinism

The four axes touch **disjoint** machinery. Authentication + authorization are an **admission filter** on
the upstream (which identity holds which seat, plus per-seat authorization — the `OUT_OF_RANGE`
determinism class). The hygiene is the **downstream delivery** (WHEN each client's bytes move, WHICH
slow/dead clients are shed). Catch-up decides only a joiner's **replay depth** (how far back its prefix
reaches). NONE touches the `CommandQueue`'s canonical selection. So when N authenticated clients each
upstream ONLY their own seat's commands and their union is the whole scenario (delivered before each
`apply_tick`), the produced frames are **byte-identical to `session::build_server_frames`** — regardless of
connect order, upstream reorder/chunking, any downstream cap/liveness drop, OR any joiner's replay window —
and every client's delivery is a byte-exact window of `[BIND | the produced stream]`. The headline over
layer 20: because the seat is bound by **identity**, a seat freed by a **mid-replay byte-cap shed** is
reclaimed by its **own** identity — the join-order policy could only promise *some* free seat.

### Honest scope (this cut)

* **The `HELLO-001` handshake read at accept is bounded-blocking** (`accept_deadline_ms`) — the one
  non-async touch, matching layers 21/22. All downstream sends (BIND + catch-up prefix + frames) are async.
* **Catch-up retention is O(`catchup_window`) frames** (`window==0` = retain-all = O(stream), only for a
  bounded run — layer 14's boundary, inherited). Per-frame join service (accept + replay enqueue happen in
  the frame loop, one `select()` slice).
* **The credential is an opaque i64 in a roster** (a production system would carry a MAC/signature —
  orthogonal cryptography), unchanged from layer 21. This closes the authenticated arc; a real credential
  MAC and remote-aircraft prediction remain the natural next picks.

## Verification

* **Bridge `seads_netauthcatchup_test`** (ctest `netauthcatchup_bridge`, GCC + Clang native-x64 like
  layers 7–22), over real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` holds until
  the seated clients' commands are in, `on_frame(kJoin)` holds until the mid-stream joiner has connected),
  finite watchdog:
  * **LEG 1 — identity binding + catch-up window regimes (the headline):** THREE identities present tokens
    with **token order ≠ seat order** (100→2, 200→0, 300→1) from the initial gather, learn their seat from
    `BIND`, and upstream ONLY that seat's commands (distinct scrambles/chunkings) ⇒ the produced stream is
    **byte-identical to `build_server_frames`** (31 frames) and each draws EXACTLY its designated seat. A
    FOURTH client joins mid-stream at `kJoin` with an unknown token (999 → SPECTATOR, seats full) and
    across **W ∈ {1, kJoin/2, retain-all}** receives EXACTLY `[BIND(spectator) | frames[max(0,kJoin-W):]]`,
    frame-aligned, `trimmed == max(0, N-W)` (30 / 24 / 0). The three seated identities stay byte-identical
    to the whole stream under every window.
  * **LEG 2 — authentication + authorization compose with catch-up:** the mid-stream unknown-token
    spectator upstreams the WHOLE scenario; all 6 rejected (`cmds_unauth == 6`), the produced stream is
    unchanged, and it STILL catches up the whole 31-frame stream byte-for-byte.
  * **LEG 3 — hygiene composes with identity + catch-up:** on a long scenario (~4000 frames) through a
    pinned 16 KiB kernel send buffer, FAST (token 200 → seat 0, hook-drained) is byte-identical to the
    aircraft-0-only reference + its commands drove the sim; DEAD (token 300 → seat 1) joins mid-stream,
    reads nothing, and has its catch-up REPLAY backlog shed by the byte-cap (`capped=1, reaped=0`) + its
    **seat freed** — delivered `[BIND(seat 1) | strict prefix]`. Because seats are bound by identity, DEAD
    is deterministically seat 1 (the layer-20 leg had to discover a dynamic real seat). Platform-invariant
    (`leaves == joins-1`, `joins ∈ {1,2}`): shed-during-replay vs joined-then-capped is OS loopback-buffer
    timing — the same policy outcome.
* **Property tests +5 ⇒ 260** (`test_authcatchup.py`, composing `auth_ref`'s `CredentialTable` with the
  layer-20 retained-history/window model): the authenticated catch-up window delivers exactly
  `frames[max(0,fi-W):]`; `trimmed == max(0, N-W)`; catch-up is downstream of authentication (an
  unknown-token spectator never changes the produced frames, its catch-up prefix is a byte-window of that
  same stream); `[BIND | prefix | live]` byte ordering; and the headline — a non-reading joiner shed
  DURING replay is a strict byte-prefix of `[BIND | frames]` AND its designated seat is freed and
  reclaimed by its OWN identity (not the lowest free).
* **Gates:** full **ctest 28→29** (GCC + Clang, all sealed net bridges still reconstruct their digests, the
  new `netauthcatchup_bridge` PASS on both); property tests **255→260**; **all 15 goldens byte-identical**
  (Sphere `6914a994…` via the Python reference); rails/roster (SPEC MONOTONE) + det_math oracle PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/authcatchupserver.{h,cpp}`,
`src/net/netauthcatchup_test_main.cpp`, `tests/property/test_authcatchup.py`, this ADR; MODIFIED
`CMakeLists.txt` (`authcatchupserver.cpp` into `seads_netinput`, `seads_netauthcatchup_test` target,
`netauthcatchup_bridge` ctest). **No shared-file/Stats change** (`trimmed` already added by layer 20;
`CredentialTable`/`seat_authorizes`/`HELLO-001`/`BIND-001` reused verbatim). **No new `_ref.py`** (the
reference is `auth_ref.py`'s `CredentialTable` + `input001_ref`/`framing_ref`/`bound_ref`, reused).
guardian.yml UNCHANGED (ctest-only bridge, like layers 13–22).

## Alternatives rejected

* **Edit `broadcast_auth_async` (or `broadcast_bound_catchup`) to add the missing axis in place.** Would
  fold catch-up into the loop layer 22 depends on (or identity into the loop layer 20 depends on), for zero
  benefit to them. The sibling keeps both loops byte-for-byte, matching the one-axis-per-layer discipline
  the whole arc follows (18 → 19 → 20 mirrored as 21 → 22 → 23).
* **Add a per-identity catch-up `Stats` field.** The replay depth and the drop reason are already counted
  (`trimmed`/`capped`); the seat's fate is bookkeeping inside `drop_client`. A per-identity breakdown adds
  surface for no determinism-relevant fact — the bridge pins seat-freed + reclaim directly.
* **Make `HELLO-001` a sealed `rails.wire.hello` block.** It is transport metadata (the layer-7
  framing-envelope category, like `BIND-001`), read before the seat exists and never hashed. Sealing it
  would conflate transport with the world_hash rail — unchanged from layer 21.
