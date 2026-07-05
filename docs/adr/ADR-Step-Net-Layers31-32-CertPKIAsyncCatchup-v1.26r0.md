# ADR — Netcode layers 31 + 32: the CA-certificate credential folded onto the async & catch-up servers (`broadcast_authcert_async` + `broadcast_authcert_catchup`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 30 (`broadcast_authcert`) made a client's seat a function of a **CA-signed certificate** the server
verifies under ONE trusted certificate-authority public key (`CaTable`), with revocation and key rotation as
first-class controls — the honest-scope follow-up to layer 27's fixed `PubkeyTable` roster. But, exactly
like layers 18/21/26/27 before their async siblings, `broadcast_authcert` kept `broadcast_input`'s
**blocking `send_all`** downstream: one slow client back-pressures the whole broadcast, and a silently-dead
peer is never shed. And a client joining after frame 0 receives only the suffix from its accept point.

Those two gaps were already closed for the ASYMMETRIC-SIGNATURE credential by the 27→28→29 arc:
- **Layer 28** (`broadcast_authsig_async`) folded the layer-16/19 async/byte-cap/liveness downstream hygiene
  onto the signature credential.
- **Layer 29** (`broadcast_authsig_catchup`) added layer-20's windowed late-join catch-up.

Layers 31 + 32 **re-run that 27→28→29 step on the CERTIFICATE credential** (30→31→32): the same async hygiene
and windowed catch-up, now with admission gated on a verified CA certificate + possession proof instead of an
enrolled public key.

The determinism stake is unchanged: the authoritative kernel's output must stay a pure function of the
canonically-ordered, **authorized** command SET. Admission (which certified identity holds which seat, plus
per-seat authorization) is an upstream FILTER; hygiene is downstream DELIVERY; catch-up is a joiner's REPLAY
DEPTH. All three axes touch disjoint machinery, none touches the `CommandQueue`. **No new det_math** (the only
crypto is Ed25519/SHA-512, reused verbatim from layer 27, integer-only, outside the kernel/world_hash).

## Decision

### `broadcast_authcert_async` (layer 31) — a sibling of `broadcast_authcert` + `broadcast_authsig_async`

`src/net/authcertasyncserver.{h,cpp}` adds `broadcast_authcert_async`: `broadcast_authsig_async`'s async
`select_rw()` loop (non-blocking per-client userspace send buffers + opt-in byte-cap drop-slowest + liveness
reap) with the **enrolled-public-key verify replaced by the layer-30 CA-certificate verify**. It is a
**SIBLING** — it owns its own `AuthCertAsyncClient` struct (socket + upstream `StreamReassembler` + downstream
send buffer + the three liveness fields + `seat`) and its own `flush_client`/`enqueue_bytes`/`over_cap`/
`reap_dead`/`drop_client` helpers. The sealed `broadcast.cpp` AND `broadcast_bound` / `broadcast_bidi` /
`broadcast_bound_async` / `broadcast_auth` / `broadcast_auth_async` / `broadcast_authmac` / `broadcast_authsig`
/ `broadcast_authsig_async` / `broadcast_authcert` are all **byte-for-byte untouched**.

- On accept: send a fresh **CHALLENGE-001** nonce DOWN (blocking — the one non-async touch, matching
  layer 30's base + layer 28's HELLO read); read the client's **HELLO-004** = `[certificate, challenge_sig]`
  (bounded-blocking); `CaTable::authenticate(cert, nonce, sig)` verifies the certificate under the CA key
  (+ revocation + epoch floor) and the possession proof under the CERTIFIED client key, resolving the
  certificate's DESIGNATED seat (free) or `bind001::SPECTATOR`; then go non-blocking and **ENQUEUE** the
  BIND-001 as the first downstream bytes (the async flush delivers it first, not layer 30's blocking
  `send_all`).
- `drop_client` calls `creds.release(seat)` on **every** drop path (EOF, fatal flush, byte-cap shed, liveness
  reap) ⇒ a seat freed by a shed/reap is reclaimed by its OWN identity (the certificate's designated seat).
- `CaTable` + `seat_authorizes` + `CHALLENGE-001`/`derive_nonce` + `HELLO-004`/`CERT-001` + `BIND-001` are
  reused **verbatim**. **No shared-file/`Stats` change** — layers 16/18 already added `capped`/`reaped`/
  `cmds_unauth`.

### `broadcast_authcert_catchup` (layer 32) — a sibling of `broadcast_authcert_async` + `broadcast_authsig_catchup`

`src/net/authcertcatchupserver.{h,cpp}` adds `broadcast_authcert_catchup`: layer 31's loop with
`broadcast_live`'s two catch-up additions folded in (lifted verbatim through `broadcast_authsig_catchup`):
- a **`history`** vector retains the produced payloads (the last `catchup_window`, or ALL when `window==0`;
  oldest evicted per new frame ⇒ `Stats.trimmed`, inherited from layer 20);
- `accept_all`, after enqueueing the BIND-001, **ENQUEUES** the retained catch-up prefix
  `history[max(0,fi−W):fi]` right after the BIND ⇒ a joiner's whole delivery is `[BIND | frames[max(0,fi−W):]]`.
  The byte-cap applies per replayed frame (a joiner whose replay backlog trips the cap is shed during
  replay — `capped`, its seat returned, never a live member).

Own `AuthCertCatchupClient` (identical to layer 31's — catch-up adds no per-client state) + own downstream
helpers ⇒ every prior server byte-for-byte untouched. **No shared-file/`Stats` change, no new det_math, no new
crypto/Python ref** (Ed25519/SHA-512 + `cert_ref.py`'s `CaTable`/CERT-001/HELLO-004 reused).

## Determinism argument

Four orthogonal axes — **admission** (CA-certificate verify + authorization, never touches the CommandQueue)
× **delivery** (async byte-cap/liveness hygiene) × **replay-depth** (catch-up window) — ⇒ N certified clients
each upstreaming ONLY their own seat's commands compose to `session::build_server_frames` **byte-identical**
regardless of connect order, upstream reorder/chunking, any downstream cap/liveness drop, OR any joiner's
replay window; a self-signed / revoked / stale-epoch forger holding a CA-signed roster still gets no seat, and
every client's delivery is a byte-exact window of `[BIND | the produced stream]`.

## Verification

- **`seads_netauthcert_async_test`** (ctest **35→36** `netauthcert_async_bridge`, gcc+clang): LEG 1 — 3
  certified identities (token order ≠ seat order 100→2/200→0/300→1; scrambled/chunked) through the async path
  ⇒ byte-identical to `build_server_frames` (31 frames), zero unauth/capped/reaped; LEG 2 — a seat-0 client's
  foreign commands + a SELF-SIGNED cert + a REVOKED token + a STALE-epoch (post-rotation) cert ALL dropped
  (`cmds_unauth=21`), all see the aircraft-0-only world byte-for-byte; LEG 3A/3B — a dead certified client
  (token 300 → seat 1) reaped (cap=0) / byte-cap shed (liveness=0) + its seat freed while a hook-drained
  seated FAST (token 200 → seat 0) stays byte-identical to its aircraft-0-only reference, DEAD delivered
  `[CHALLENGE | BIND(seat 1) | strict prefix]`.
- **`seads_netauthcert_catchup_test`** (ctest **36→37** `netauthcert_catchup_bridge`, gcc+clang): LEG 1 — 3
  certified identities ⇒ byte-identical; a 4th SELF-SIGNED-cert SPECTATOR joins at kJoin and across
  W ∈ {1, kJoin/2, retain-all} receives EXACTLY `[BIND(spectator) | frames[max(0,kJoin−W):]]`,
  trimmed=30/24/0; LEG 2 — the spectator upstreams the WHOLE set, all rejected (`cmds_unauth=6`), produced
  stream unchanged, spectator STILL catches up the whole stream; LEG 3 — a dead certified client's catch-up
  backlog shed by the byte-cap (`capped=1`) + its seat freed, DEAD delivered `[CHALLENGE | BIND(seat 1) |
  strict prefix]`.
- **+8 property tests** (`tests/property/test_authcertservers.py`): certified admission is downstream- and
  replay-blind, frames order-invariant, a certified seat is freed on any drop and reclaimed by its own
  identity, BIND-first + dropped-client-is-prefix, a dead client never changes a survivor's bytes, the
  catch-up window suffix + trimmed count, catch-up is authorization-blind, and revocation/rotation reject into
  the spectator class. ⇒ property total **325→333**.
- **TRANSPORT-ONLY**: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes, protocol-7,
  session/event codec, or tuning touched ⇒ **all 15 goldens byte-identical** (Sphere `6914a994…`), sealed
  session/event digests unmoved (`966aca05…`). **guardian.yml UNCHANGED** (ctest-only bridges, like layers
  13–30).

## Scope / honest limits

The CHALLENGE send + HELLO-004 read at accept are bounded-BLOCKING (the two non-async touches). Catch-up
retention is O(`catchup_window`) frames (window==0 = retain-all = O(stream), bounded runs only). ONE
self-signed root CA (no intermediate certificate chains — that is the next honest-scope follow-up, orthogonal
to this hygiene fold); the revocation set + epoch floors are in-memory server state (a production system
distributes a CRL/OCSP).

## Files

NEW `src/net/authcertasyncserver.{h,cpp}`, `src/net/authcertcatchupserver.{h,cpp}`,
`src/net/netauthcertasync_test_main.cpp`, `src/net/netauthcertcatchup_test_main.cpp`,
`tests/property/test_authcertservers.py`, this ADR. MODIFIED `CMakeLists.txt` (`authcertasyncserver.cpp` +
`authcertcatchupserver.cpp` into `seads_netinput`; `seads_netauthcert_async_test` +
`seads_netauthcert_catchup_test` targets; `netauthcert_async_bridge` + `netauthcert_catchup_bridge` ctests).
No shared-file change; no new crypto/Python ref.
