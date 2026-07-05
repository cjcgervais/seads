# ADR — Netcode layers 28 + 29: the signed credential folded onto the async & catch-up servers (`broadcast_authsig_async` / `broadcast_authsig_catchup`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 27 (`broadcast_authsig`, ADR-Step-Net-Layer27) made the client→aircraft binding a function of a
**verified Ed25519 signature**: on accept the server issues a fresh `CHALLENGE-001` nonce, the client
answers `HELLO-003 = [token, signature]`, and `PubkeyTable::authenticate` verifies the signature under the
enrolled **public key** before binding the token's designated seat — the server holds no secret capable of
signing, so a roster leak cannot impersonate. But — exactly like the token-auth server layer 21 before its
async/catch-up siblings — layer 27 kept the **blocking `send_all`** downstream and offered **no late-join
catch-up**. The token-auth arc had already run this exact staging: layer 21 → 22 (async) → 23 (catch-up).
Layers 28 + 29 re-run that staging on the **signed** credential, as layer 27's ADR named ("fold the
verifying credential onto the async/catch-up servers … mechanical").

The determinism stake is unchanged from layers 22/23: admission (which identity **proves** which seat, plus
per-seat authorization) is an upstream filter that never touches the `CommandQueue`; the async hygiene is
downstream delivery; catch-up is a joiner's replay depth. All orthogonal. The cross-toolchain bit-identity
promise is untouched — the only new work per layer is a socket loop, no new crypto (Ed25519/SHA-512 from
layer 27 reused verbatim, integer-only).

## Decision

### Layer 28 — `broadcast_authsig_async` (`src/net/authsigasyncserver.{h,cpp}`)

`broadcast_auth_async`'s async loop (layer 22: non-blocking per-client userspace send buffers + opt-in
byte-cap drop-slowest + liveness reap) with the **token lookup replaced by the layer-27 challenge/signature
verify**. A **SIBLING** of both `broadcast_authsig` and `broadcast_auth_async` — it owns its own
`AuthSigAsyncClient` struct (layer 22's `AuthAsyncClient` field-for-field) and its own
`flush_client`/`over_cap`/`enqueue_bytes`/`drop_client`/`reap_dead` helpers, so the sealed `broadcast.cpp`
and **every** prior server (`broadcast_bound` / `broadcast_bidi` / `broadcast_bound_async` /
`broadcast_bound_catchup` / `broadcast_auth` / `broadcast_auth_async` / `broadcast_auth_catchup` /
`broadcast_authmac` / `broadcast_authsig`) are **byte-for-byte untouched**. `PubkeyTable` +
`seat_authorizes` + `CHALLENGE-001`/`derive_nonce` + `HELLO-003` + `BIND-001` are **reused verbatim**; **no
shared-file or `Stats` change** (layers 16/18 already added `capped`/`reaped`/`cmds_unauth`). The one
structural difference from layer 22's accept path: the accept handshake is a **challenge-response** — on
accept the server sends the fresh `CHALLENGE-001` nonce DOWN (blocking `send_all`, while the socket is still
blocking), reads the client's `HELLO-003` (bounded-blocking), verifies the signature to a seat, then goes
non-blocking and **enqueues the `BIND-001`** as the first bytes of the async send buffer (after the
already-sent CHALLENGE). So the two non-async touches at accept are the CHALLENGE send + the HELLO read
(layer 27's blocking base + layer 22's HELLO read). Everything downstream is layer 22 verbatim.

### Layer 29 — `broadcast_authsig_catchup` (`src/net/authsigcatchupserver.{h,cpp}`)

Layer 28's async loop with `broadcast_live`'s **history-retention + windowed catch-up replay** (via
`broadcast_auth_catchup`, layer 23) folded in — a SIBLING of both `broadcast_authsig_async` and
`broadcast_auth_catchup`. Two additions over layer 28, both lifted verbatim from layer 23: a **`history`
vector** retains the produced payloads (the last `catchup_window`, or ALL when `window==0`; oldest evicted
per new frame ⇒ `Stats.trimmed`, added by layer 20, inherited), and `accept_all` **enqueues the retained
prefix right after the `BIND-001`** ⇒ a joiner accepted at frame fi is delivered `[CHALLENGE | BIND |
frames[max(0,fi−W):]]`; the byte-cap applies per replayed frame (a joiner whose replay backlog trips the
cap is shed during replay — `capped`, seat returned, never a live member). Because seats are bound by
IDENTITY, a shed/reaped seat is reclaimed by its **own** identity on reconnect.

### `CHALLENGE-001` / `HELLO-003` / `BIND-001` are transport metadata, NOT sealed wires

Unchanged from layers 18/21/26/27: connection metadata, feed no hash, not `rails.wire` blocks. Both layers
take **no seal**. Cross-impl byte parity is still pinned (the layer-27 SHA-512/Ed25519/HELLO-003 vectors,
reused).

### Why the composition preserves determinism

The signature verify is an admission filter on the upstream (which identity holds which seat + per-seat
authorization); the async hygiene is downstream delivery (WHEN each client's bytes move, WHICH slow/dead
clients shed); catch-up is a joiner's replay depth. Disjoint machinery; **none touches the `CommandQueue`'s
canonical selection**. So N signed clients each upstreaming ONLY their own seat's commands (union = the whole
scenario, delivered before each `apply_tick`) produce frames **byte-identical to
`session::build_server_frames`** — regardless of connect order, upstream reorder/chunking, any downstream
cap/liveness drop, any joiner's replay window, or a forger (holding the whole public roster) also connected
and upstreaming (all dropped). Every client's delivery is a byte-exact window of `[CHALLENGE | BIND | the
produced stream]`.

### Honest scope

Same as layers 22/23/27: the CHALLENGE send + HELLO read at accept are bounded-blocking (the two non-async
touches); catch-up retention is O(`catchup_window`) frames (window==0 = retain-all = O(stream), bounded run
only); keys are a fixed enrolled roster (a production PKI carries certificates / rotation / revocation — the
named follow-up).

## Verification

* **Bridge `seads_netauthsig_async_test`** (ctest `netauthsig_async_bridge`, gcc+clang), over real
  127.0.0.1 sockets, no sleeps (cv+notify rendezvous), finite watchdog:
  * **LEG 1** — 3 identities (token order ≠ seat order: 100→2, 200→0, 300→1) each SIGN a fresh challenge and
    fly their designated seat (scrambled/chunked) THROUGH the async path ⇒ each downstream (after CHALLENGE +
    BIND) byte-identical to `build_server_frames`; `cmds_ok==6`, unauth/stale/oob/capped/reaped all 0.
  * **LEG 2** — a valid seat-0 client's foreign commands + a **FORGER's** (right token, wrong private key →
    bad signature → spectator) commands ALL dropped (`cmds_unauth==15`) through the async path; both see the
    aircraft-0-only world byte-for-byte.
  * **LEG 3 (A liveness reap @ cap=0 / B byte-cap shed @ liveness=0)** — a LONG (~1 MB) stream through a
    pinned 16 KiB kernel buffer: FAST (token 200→seat 0, hook-drained) byte-identical to the aircraft-0-only
    reference + its commands drove the sim; DEAD (token 300→seat 1) proves its credential then reads/sends
    nothing ⇒ dropped by the policy under test + its seat freed (`leaves==1`), delivered `[CHALLENGE |
    BIND(seat 1) | strict frame-prefix]`.
* **Bridge `seads_netauthsig_catchup_test`** (ctest `netauthsig_catchup_bridge`, gcc+clang):
  * **LEG 1** — 3 signed identities ⇒ produced stream byte-identical to `build_server_frames`; a 4th
    unknown-token (999) SPECTATOR joins at kJoin and across W ∈ {1, kJoin/2, retain-all} receives EXACTLY
    `[CHALLENGE | BIND(spectator) | frames[max(0,kJoin−W):]]`, `trimmed`=30/24/0; seated identities
    byte-identical to the whole stream under every window.
  * **LEG 2** — the spectator upstreams the WHOLE set, all rejected (`cmds_unauth==6`), produced stream
    unchanged, spectator STILL catches up the whole stream.
  * **LEG 3** — a non-reading mid-stream DEAD identity (token 300→seat 1) has its catch-up replay backlog
    shed by the byte-cap (`capped==1`) + its seat freed, while a hook-drained seated FAST (token 200→seat 0)
    stays byte-identical to its reference; DEAD delivered `[CHALLENGE | BIND(seat 1) | strict prefix]`
    (platform-invariant: `capped==1, reaped==0, leaves==joins−1, joins∈{1,2}`).
* **Property tests +7** (`test_authsigservers.py`): signed admission is downstream- and replay-blind;
  signed frames order-invariant; a signed seat is freed on ANY drop and reclaimed by its OWN identity; a
  forger (wrong key) is admitted to nothing; BIND-first / dead-client-is-BIND+prefix; the catch-up window
  suffix + `trimmed` count; catch-up is authorization-blind. (Over the layer-27 `PubkeyTable` + Ed25519
  signing.)
* **Gates:** full **ctest 32→34** (GCC + Clang: the two new bridges PASS on both, all sealed net bridges
  still reconstruct `966aca05…`); property tests **304→311**; **all 15 goldens byte-identical** (Sphere
  `6914a994…`, scenario goldens via `lockstep_equal`, WEAPON-001 via `weapon_byteexact`); sealed
  session/event digests unmoved.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes, protocol-7,
session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, no seal.** Diff: NEW
`src/net/authsigasyncserver.{h,cpp}`, `src/net/authsigcatchupserver.{h,cpp}`,
`src/net/netauthsigasync_test_main.cpp`, `src/net/netauthsigcatchup_test_main.cpp`,
`tests/property/test_authsigservers.py`, this ADR; MODIFIED `CMakeLists.txt` (two servers into
`seads_netinput`, two test targets, two ctest entries). **No new Python ref** (reuses `authsig_ref`'s
PubkeyTable + Ed25519 + the input001/framing/bound refs + the layer-20 window model). **No shared-file
change** (no `Stats`/accessor edit). guardian.yml UNCHANGED (ctest-only bridges, like layers 13–27).

## Alternatives rejected

* **One combined server (async + catch-up in a single new file).** Layer 23 kept async (22) and catch-up
  (23) as separate siblings; mirroring that keeps each server a minimal delta over its predecessor and each
  bridge focused. (This ADR is combined because the two siblings are the one requested "fold onto async/
  catch-up" task and share all machinery — but the code stays two siblings, matching 22/23.)
* **Edit `broadcast_auth_async`/`broadcast_auth_catchup` to take a `PubkeyTable`.** Would fold the challenge/
  signature verify into loops layers 22/23 depend on, for zero benefit to them. The sibling keeps those
  loops byte-for-byte, matching the one-axis-per-layer discipline (as layer 27 was a sibling of layer 26).
* **Enqueue the CHALLENGE into the async buffer instead of a synchronous send.** The client must have the
  nonce before it can sign its HELLO, and the server must have the HELLO before it can resolve a seat and
  send the BIND — so the handshake is inherently a bounded round-trip at accept (as layer 27). Sending the
  tiny CHALLENGE synchronously (before `set_nonblocking`) is the simplest correct form; the BIND still rides
  the async buffer (delivered first there).
* **A new Python ref / new pins.** The credential primitives (Ed25519/SHA-512/HELLO-003) and their vectors
  are layer 27's, reused unchanged; the async/catch-up composition is a socket property proven by the
  bridges + the model-level property tests over the existing refs. No new sealed surface.
