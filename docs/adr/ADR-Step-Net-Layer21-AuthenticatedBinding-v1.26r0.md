# ADR — Netcode layer 21: authenticated binding — seat by identity, not join-order position (`broadcast_auth`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 18 (`broadcast_bound`) settled the client→aircraft binding to a **join-order `SeatPolicy`**: the
first socket to connect got seat 0, the next seat 1, and so on. That enforces "one client, one
aircraft", but every bidirectional ADR since flagged the same remaining caveat in its honest-scope
note: the binding is **POSITIONAL and UNAUTHENTICATED**. A client's seat depends on *who else is
connected*, not on *who the client is* — a reconnecting player gets a different aircraft depending on
join order, and nothing ties a socket to an identity. Layer 18's ADR named it as scope: *"the binding
is join-order with no identity — it enforces one client, one aircraft, not who you are. Seat =
position, still."*

Layer 21 settles that last caveat: the binding becomes a function of the client's **identity**.

The determinism stake is unchanged from layer 18: the authoritative kernel's output must stay a pure
function of the canonically-ordered, **authorized** command SET. Authentication only decides *which
seat (if any)* an identity holds — an admission decision, the same class as the join-order policy it
replaces — so it must not make the frames depend on transport order/chunking.

## Decision

### `broadcast_auth` — a sibling of `broadcast_bound`

`src/net/authserver.{h,cpp}` adds `broadcast_auth`: `broadcast_bound`'s single-thread `select()` loop
(blocking `send_all` downstream — see scope) with the **join-order `SeatPolicy` replaced by an identity
handshake**. It is a **SIBLING** — it owns its own `AuthClient` struct (identical to layer 18's
`BoundClient`: socket + `StreamReassembler` + `seat`) and its own accept/loop code. `broadcast_bound`,
`broadcast_input`, `broadcast_bidi`, and the async/catch-up siblings are **byte-for-byte untouched**
(all their bridges still pass). **BIND-001 and `seat_authorizes` are reused verbatim** — the only new
mechanism is *how the seat is chosen*. **No shared-file change at all**: `inputserver.h`'s `Stats`
already carries every field this layer reports (`joins`, `leaves`, `cmds_ok/unauth/stale/oob`), exactly
as layer 19 needed none.

**1. `HELLO-001` handshake (the new upstream record).** A joining client's FIRST upstream framing frame
is a `HELLO-001` record (`src/net/hello001.{h,cpp}` ↔ `tools/auth_ref.py`) carrying its opaque i64
credential `token` — the upstream mirror of the server's downstream `BIND-001` reply. The server reads
it at accept, BEFORE assigning a seat (the inverse of layer 18, where the seat came first and the
client only read its BIND). Wire:
`HELLO-001 = [version 0x01] [ZigZag+LEB128 token]`. The integer reuses the sealed
`geo001::{encode_i64,decode_i64}` pipeline ⇒ **no new primitive, no det_math (fifteenth consecutive
zero-transcendental layer)**. A client that sends no valid HELLO (malformed, wrong version, or EOF
during the handshake) is dropped — never a member.

**2. `CredentialTable` — identity → seat.** A pre-shared roster maps each enrolled token to a
**DESIGNATED seat** in `[0, n_aircraft)`. `authenticate(token)` returns:
* the token's designated seat, when the token is enrolled AND that seat is currently free;
* **SPECTATOR** (`-1`), when the token is unknown (no credential) OR its seat is already held by a live
  session (a **double-login** — the identity is valid but already seated).
`release(seat)` frees a seat on leave, so a reconnecting identity reclaims **its own** seat (not the
lowest free). Mirrored in `tools/auth_ref.CredentialTable`. The seat for a given token is a function of
the **roster**, invariant to join order — only double-login *contention* depends on order (and that is
a rejection, never a different seat).

**3. `BIND-001` + authorization (reused from layer 18).** The server replies with the same one-time
`BIND-001` record naming the resolved seat (or `-1` for a spectator) as the client's first downstream
frame, and authorizes upstream commands with the same
`seat_authorizes(seat, aircraft) := seat >= 0 && aircraft == seat`. A command for any other aircraft,
or any command from a spectator (unknown/rejected identity), is DROPPED (`cmds_unauth`) — byte-identical
to it never arriving.

### `HELLO-001` is transport metadata, NOT a sealed wire

Like `BIND-001`, `HELLO-001` is modelled on the **layer-7 framing envelope**: it carries no simulation
value, feeds no hash, and is not a `rails.wire` block. The layer-21 determinism claim rests on the
**authorization filter** (unchanged from layer 18), not on this record. So this layer takes **no seal**
even though it adds a wire, exactly as BIND-001 and the framing envelope did. Cross-impl byte parity is
still pinned (shared known-encoding vector `[0x01,0x0E]` for token 7 in `auth_ref.py` and the bridge)
because the codebase pins every wire — the discipline, not a seal trigger.

### Why the identity binding preserves determinism

Authentication is an admission filter on the upstream, exactly like the join-order policy it replaces:
it decides *which seat (if any)* each connection holds, and a command from an unbound/foreign seat is
indistinguishable from one that never arrived — the `OUT_OF_RANGE` reject's determinism class. The
credential lookup and `BIND-001` record touch only bookkeeping and the downstream. So when N
authenticated clients each upstream ONLY their own seat's commands and their union is the whole
scenario command set (delivered before each `apply_tick`), the produced frames are **byte-identical to
`session::build_server_frames`** — regardless of *which identity connected in which order*, how the
upstream bytes were reordered/chunked, or that an unknown-token spectator was also connected and
upstreaming (its commands are all dropped). This is strictly stronger than layer 18's claim: there we
could only assert "distinct seats" (the specific assignment was accept-order-dependent); here each
identity draws its **specific** designated seat, so the bridge pins the exact seat per token.

### Honest scope (this cut)

* **Blocking downstream.** `broadcast_auth` uses `broadcast_bound`'s blocking `send_all` base (a
  cooperative client reads concurrently). Merging identity binding with the layer-16/19 async/byte-cap/
  liveness hygiene, or with layer-20 windowed catch-up, is the natural follow-up — the same orthogonal
  axes the codebase composes one layer at a time (18 → 19 → 20). Authentication is independent of all
  three (admission vs delivery vs replay-depth), so those merges are mechanical.
* **The credential is an opaque i64 looked up in a roster.** A production system would carry a
  MAC/signature the server verifies against a secret; that is orthogonal cryptography. This layer is the
  **binding mechanics** — identity → seat, and "you cannot command a seat you were not bound to" — not
  the credential's cryptographic strength. The determinism story depends only on the roster being a
  fixed function of identity, which it is.

## Verification

* **Bridge `seads_netauth_test`** (ctest `netauth_bridge`, native-x64 leg like layers 7–20), over real
  127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every client has sent
  its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — identity-based seats, invariant to join order (the headline):** THREE clients each present
    a distinct token and are bound to the seat their token DESIGNATES, deliberately with **token order ≠
    seat order** (token 100→seat 2, 200→seat 0, 300→seat 1). Each learns its seat from `BIND`, upstreams
    ONLY that seat's commands (distinct scrambles: reversed @1 B, forward @7 B, reversed @3 B), and —
    because the roster is a fixed function of identity — draws EXACTLY its designated seat regardless of
    which thread's connection the OS accepted first. Their union is the whole INPUT-SK-001 command set,
    so each client's downstream is **byte-identical to `build_server_frames`**. The bridge asserts the
    SPECIFIC seat per token (not just "distinct"), `cmds_ok == 6`, `cmds_unauth == stale == oob == 0`,
    `joins == 3`.
  * **LEG 2 — authentication + authorization boundary:** TWO clients upstream the WHOLE scenario. Client
    A presents a valid token bound to seat 0 → its foreign-aircraft commands are dropped. Client B
    presents an UNKNOWN token → seated as a SPECTATOR → ALL its commands dropped (no credential → no
    aircraft → steers nothing). Both receive the IDENTICAL downstream — **the aircraft-0-only world
    byte-for-byte** — so neither the foreign commands nor the unauthenticated client changed anything.
    `cmds_ok == 3` (aircraft 0's phases), `cmds_unauth ==` (A's foreign) + (all of B's), `oob == 0`; A's
    `BIND` seat `== 0`, B's `BIND` seat `== -1`.
  * **LEG 3 — `HELLO-001` codec + `CredentialTable` (in-process):** the codec (shared known-encoding pin
    `[0x01,0x0E]` for token 7 vs `auth_ref.py`, round-trip incl. a negative token, wrong-version
    reject), and the `CredentialTable` (identity→seat invariant to enrollment/auth order,
    unknown→spectator, double-login→spectator, release-then-reclaim-OWN-seat), and the reused
    `seat_authorizes` predicate.
* **Property tests +8 ⇒ 250** (`test_auth.py`): `HELLO-001` round-trip (incl. negatives) + version
  enforcement + the known-encoding pin; the `CredentialTable` seat-is-a-function-of-identity under a
  shuffled authentication order over a random bijection; unknown-token→spectator; no double-booking +
  own-seat reclaim under a randomised join/leave interleaving; the reused `seat_authorizes`; and the
  authentication+authorization *composition* (three enrolled identities + an unknown spectator authorize
  exactly the whole seated set, the spectator nothing).
* **Gates:** full **ctest 26→27** (GCC + Clang, all sealed net bridges still reconstruct `966aca05…`,
  the new `netauth_bridge` PASS on both); property tests **242→250**; **all 15 goldens byte-identical**
  (Sphere `6914a994…` via `seads_golden` on GCC + Clang); `auth_ref.py` selftest PASS; determinism lint
  + the four probe gates PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed
session/event digests unmoved, no seal.** Diff: NEW `src/net/hello001.{h,cpp}`,
`src/net/authserver.{h,cpp}`, `src/net/netauth_test_main.cpp`, `tools/auth_ref.py`,
`tests/property/test_auth.py`, this ADR; MODIFIED `CMakeLists.txt` (`hello001.cpp` + `authserver.cpp`
into `seads_netinput`, `seads_netauth_test` target, `netauth_bridge` ctest). **No shared-file change**
(no `Stats`/accessor edit — layer 18 already added `cmds_unauth`). guardian.yml UNCHANGED (ctest-only
bridge, like layers 13–20).

## Alternatives rejected

* **Enforce identity inside `CommandQueue`.** The queue is the canonical ordering contract — a pure
  function of the command SET, blind to sockets/seats/identities. Authentication is a *who-sent-this*
  question the queue cannot see; it belongs in the server, which decides WHICH commands to submit. The
  queue stays pure (the same reasoning as layer 18's authorization).
* **Edit `broadcast_bound` to take a credential map.** Would fold the HELLO handshake + identity lookup
  into the loop layers 18–20 depend on, for zero benefit to them. The sibling keeps that loop
  byte-for-byte, matching the codebase's one-axis-per-layer discipline.
* **Make `HELLO-001` a sealed `rails.wire` block (and reseal).** It carries no sim state and feeds no
  hash — it is connection metadata, exactly the `BIND-001` / framing-envelope category, which took no
  seal. A reseal would misrepresent a transport handshake as a determinism-critical wire.
* **Reject unknown credentials by dropping the connection instead of seating a spectator.** Seating a
  spectator reuses the existing receive-only path (an unknown peer can watch but not play, and its
  commands are the already-tested `cmds_unauth` drop) and keeps the accept path free of a special
  hard-close branch. A hard reject is a trivial policy variant on top; the determinism-relevant fact —
  an unauthenticated identity drives no seat — holds either way and is what the bridge pins.
* **Assign a fresh lowest-free seat per identity (identity only gates admission).** That would keep the
  positional caveat (a reconnecting player still lands on a different aircraft). Binding the token to a
  *designated* seat is the whole point — the seat is now a property of *who you are*, which the
  invariant-to-join-order LEG 1 assertion pins.
* **Fold in async/catch-up now.** Authentication (admission), hygiene (delivery), and catch-up
  (replay-depth) are orthogonal; bundling repeats the surface-doubling layers 15b/18 avoided by staging.
  Deferred, named as scope — mechanical merges onto layers 19/20.
