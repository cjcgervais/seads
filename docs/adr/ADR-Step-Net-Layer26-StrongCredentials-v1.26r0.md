# ADR — Netcode layer 26: strong credentials — a verifying keyed MAC, not a bare token (`broadcast_authmac`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 21 (`broadcast_auth`) made a client's seat a function of its **identity**: the client presents a
`HELLO-001` credential and the server looks the `token` up in a pre-shared `CredentialTable` to a
designated seat. But the "credential" was an **abstracted i64 `token` the server merely LOOKED UP** — a
public identifier, like a username. Anyone who observed or guessed a token could claim that identity and
take its seat. Every auth ADR (21/22/23) flagged the same honest-scope gap in the same words: *"a
production system would carry a MAC/signature the server verifies against a secret; that is orthogonal
cryptography."* Layer 26 closes that gap: the credential becomes a **keyed MAC over a server-issued
challenge that the server VERIFIES against a pre-shared secret** — the client must now PROVE possession
of the secret, not merely name a token.

The determinism stake is unchanged from layers 18/21: the authoritative kernel's output must stay a pure
function of the canonically-ordered, **authorized** command SET. Verification only sharpens *which
identities are admitted* — a forged/stale/unknown credential must be rejected into the **same admission
class** an unknown token already was, so the frames still cannot depend on transport order/chunking.

The cross-toolchain bit-identity promise must survive the new "crypto". It does, because the MAC is
**SipHash-2-4** — 100% 64-bit *unsigned integer* arithmetic (add mod 2⁶⁴, xor, rotate), no float, no
libm, no FMA-sensitive path. It lives in transport, outside the kernel/world_hash, so the `det_math`
transcendental ban does not even apply — and there are no transcendentals to ban.

## Decision

### `broadcast_authmac` — a sibling of `broadcast_auth`

`src/net/authmacserver.{h,cpp}` adds `broadcast_authmac`: `broadcast_auth`'s single-thread `select()`
loop (blocking `send_all` downstream — see scope) with the **token LOOKUP replaced by a verifying
keyed-MAC challenge-response**. It is a **SIBLING** — it owns its own `AuthMacClient` struct (identical
to layer 21's `AuthClient`: socket + `StreamReassembler` + `seat`) and its own accept/loop code.
`broadcast_auth`, `broadcast_bound`, `broadcast_input`, and every async/catch-up sibling are
**byte-for-byte untouched** (all their bridges still pass). **BIND-001 and `seat_authorizes` are reused
verbatim**; the only new mechanism is *how the seat is proven*. **No shared-file change at all**:
`inputserver.h`'s `Stats` already carries every field this layer reports (`joins`, `leaves`,
`cmds_ok/unauth/stale/oob`).

**1. `SipHash-2-4` — the real keyed MAC (`src/net/siphash.{h,cpp}` ↔ `tools/siphash_ref.py`).** A
standard, well-known keyed PRF/MAC. `siphash24(k0, k1, data, len) -> u64`, 128-bit key, c=2 compression
+ d=4 finalization rounds, pure `std::uint64_t` ops (wrap mod 2⁶⁴ by definition ⇒ identical on every
toolchain/architecture). Pinned to the **official SipHash-2-4 reference vector** (key `00..0f`, message
`00..0e` → `0xa129ca6149be45e5`) — a genuine cross-project anchor, not just self-consistency. **No new
det_math (sixteenth-class zero-transcendental transport layer)**; it is *not* det_math at all — it is a
transport codec primitive, like ZigZag+LEB128.

**2. `CHALLENGE-001` — the server's freshness contribution (`src/net/authmac001.{h,cpp}`).** The instant
a client connects, the server sends a `CHALLENGE-001` record DOWN as the FIRST downstream framing frame
(before BIND): `CHALLENGE-001 = [version 0x01] [ZigZag+LEB128 nonce]`. The nonce is
`derive_nonce(session_key, accept_counter) = SipHash(session_key, counter_le)`, where `accept_counter`
increments per accepted socket. A **distinct nonce per connection** means a captured HELLO cannot be
replayed to a later connection (the later connection issues a different challenge). The session key seeds
the challenge; a production server draws it from a CSPRNG, the deterministic bridge passes a fixed seed
so the run is reproducible.

**3. `HELLO-002` — the client's identity claim + proof.** In response the client sends
`HELLO-002 = [version 0x02] [ZigZag+LEB128 token] [ZigZag+LEB128 mac]`, where
`mac = compute_mac(secret[token], nonce, token) = SipHash(secret, nonce_le || token_le)`. The MAC binds
the proof to BOTH the server's fresh nonce (freshness) and the claimed token (identity). Version `0x02`
keeps it distinct from layer 21's token-only `HELLO-001` (version `0x01`), which stays **byte-for-byte
frozen** — a v1 decoder rejects a v2 record and vice-versa. The mac u64 rides the sealed
`geo001::{encode_i64,decode_i64}` pipeline as its bit pattern ⇒ **no new primitive**.

**4. `SecretTable` — the verifying roster.** `enroll(token, seat, k0, k1)` registers a token's
designated seat AND its 128-bit secret. `authenticate(token, nonce, mac)` returns:
* the token's designated seat, when the token is enrolled, its seat is free, **AND**
  `mac == SipHash(secret, nonce||token)` (the proof verifies);
* **SPECTATOR** (`-1`) otherwise — unknown token, double-login (seat held), OR a **bad MAC** (forgery /
  wrong secret / stale nonce). A forgery is rejected into the **exact same SPECTATOR class** as an
  unknown token. Mirrored in `tools/authmac_ref.SecretTable`.

**5. `BIND-001` + authorization (reused from layers 18/21).** The server replies with the same one-time
`BIND-001` naming the resolved seat, and authorizes upstream commands with the same
`seat_authorizes(seat, aircraft) := seat >= 0 && aircraft == seat`.

### `CHALLENGE-001` / `HELLO-002` are transport metadata, NOT sealed wires

Like `BIND-001` / `HELLO-001` / the layer-7 framing envelope, both records carry no simulation value,
feed no hash, and are not `rails.wire` blocks. The layer-26 determinism claim rests on the
**authorization filter** (unchanged from layers 18/21), not on these records or the MAC. So this layer
takes **no seal** even though it adds two wires + a MAC primitive, exactly as BIND-001/HELLO-001 did.
Cross-impl byte parity is still pinned (shared known-encoding vectors `[0x01,0x0E]` for a nonce-7
challenge and `[0x02,0x0E,0x02]` for a token-7/mac-1 HELLO, plus the official SipHash vector) because the
codebase pins every wire — the discipline, not a seal trigger.

### Why the verified binding preserves determinism

Verification is an admission filter on the upstream, exactly like the token lookup it sharpens: it
decides *which seat (if any)* each connection holds, and a command from an unbound seat (forger,
spectator, or foreign) is indistinguishable from one that never arrived — the `OUT_OF_RANGE` reject's
determinism class. The MAC check, challenge, and `BIND-001` record touch only bookkeeping and the
downstream; **the `CommandQueue` never sees a credential**. So when N authenticated clients each upstream
ONLY their own seat's commands and their union is the whole scenario command set (delivered before each
`apply_tick`), the produced frames are **byte-identical to `session::build_server_frames`** — regardless
of which identity connected in which order, how the upstream bytes were reordered/chunked, or that a
**forger** and an unknown-token spectator were also connected and upstreaming (all their commands
dropped). This is strictly stronger than layer 21: there, knowing the (public) token was enough to take a
seat; here a client with the wrong secret is rejected, and the bridge pins that a forger who knows the
token gets **no** aircraft.

### Honest scope (this cut)

* **Symmetric MAC, not an asymmetric signature.** This is a pre-shared 128-bit secret the *server holds*
  (a shared-secret MAC with a server-contributed nonce for freshness) — strictly stronger than a bare
  token. An **asymmetric signature** (public-key identity: the server holds only a public key, never the
  client's secret, so a server-side roster leak cannot impersonate a client) is the named next hardening.
  The MAC↔signature choice is orthogonal to the binding mechanics this layer proves.
* **Blocking downstream.** `broadcast_authmac` uses `broadcast_auth`'s blocking `send_all` base. Merging
  the verifying credential with the layer-16/19/22 async hygiene, or layer-20/23 catch-up, is the natural
  follow-up — the same orthogonal axes (admission vs delivery vs replay-depth) the codebase composes one
  layer at a time. The credential change is confined to the accept handshake, so those merges are
  mechanical (swap the seat-assignment call, as layer 22 did over layer 21).
* **The session key is a passed-in seed.** A production server derives it from a CSPRNG; the bridge fixes
  it only for reproducibility. The determinism story depends on the nonce being *fresh per connection*,
  which it is under any seed.

## Verification

* **Bridge `seads_netauthmac_test`** (ctest `netauthmac_bridge`, native-x64 leg like layers 7–25), over
  real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every client has
  sent its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — verified identity seats, invariant to join order (the headline):** THREE clients each hold
    a distinct token AND its correct secret, are challenged, PROVE possession, and are bound to the seat
    their token DESIGNATES, deliberately with **token order ≠ seat order** (100→2, 200→0, 300→1). Each
    upstreams ONLY that seat's commands (distinct scrambles: reversed @1 B, forward @7 B, reversed @3 B);
    their union is the whole INPUT-SK-001 command set, so each client's downstream is **byte-identical to
    `build_server_frames`** (31 frames). Asserts the SPECIFIC seat per token, `cmds_ok == 6`,
    `cmds_unauth == stale == oob == 0`, `joins == 3`, every `challenge_ok`.
  * **LEG 2 — verify + authorization boundary (the new capability):** THREE clients upstream the WHOLE
    scenario. Client A holds token 200 AND its correct secret → seat 0; its foreign commands dropped.
    Client B knows token 300 but presents a **FORGED mac** (wrong secret) → SPECTATOR; ALL commands
    dropped — *exactly what layer 21 could not stop*. Client C presents an UNKNOWN token 999 → SPECTATOR;
    all dropped. All three receive the IDENTICAL aircraft-0-only world byte-for-byte. `cmds_ok == 3`
    (aircraft 0's phases), `cmds_unauth == 15` (A's 3 foreign + B's 6 + C's 6), A's `BIND` seat `== 0`,
    B's and C's `BIND` seat `== -1`.
  * **LEG 3 — SipHash vector + `CHALLENGE-001`/`HELLO-002` codecs + `SecretTable` verify (in-process):**
    the OFFICIAL SipHash-2-4 vector (`0xa129ca6149be45e5`); the two record codecs (known-encoding pins
    `[0x01,0x0E]` / `[0x02,0x0E,0x02]` vs `authmac_ref.py`, round-trip, wrong-version + not-interchangeable
    rejects); and `SecretTable` (verify, **forgery-reject** with wrong secret, **replay-reject** of a
    valid mac under a different nonce, unknown→spectator, double-login→spectator,
    release-then-reclaim-OWN-seat) + the reused `seat_authorizes`.
* **Property tests +14 ⇒ 288** (`test_authmac.py`): SipHash official vector + determinism +
  key/message sensitivity; `CHALLENGE-001`/`HELLO-002` round-trip + version enforcement + the pins;
  `SecretTable` — a valid proof binds the designated seat invariant to auth order over a random bijection;
  a forged/wrong-secret credential is rejected (seat left free) while the genuine holder still succeeds; a
  replayed mac under a different nonce is rejected; unknown→spectator; no double-booking + own-seat reclaim
  under a randomised join/leave interleaving; the challenge is fresh per accept; the reused
  `seat_authorizes`.
* **Gates:** full **ctest 30→31** (GCC + Clang, all sealed net bridges still reconstruct `966aca05…`, the
  new `netauthmac_bridge` PASS on both); property tests **274→288**; **all 15 goldens byte-identical**
  (Sphere `6914a994…`, scenario goldens via `lockstep_equal`, WEAPON-001 protocol-7 via `weapon_byteexact`
  — all PASS on GCC); sealed **session + event digests unmoved** (`session_reconstruct`, `event_reliable`
  PASS); `siphash_ref.py` + `authmac_ref.py` selftests PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/siphash.{h,cpp}`, `src/net/authmac001.{h,cpp}`,
`src/net/authmacserver.{h,cpp}`, `src/net/netauthmac_test_main.cpp`, `tools/siphash_ref.py`,
`tools/authmac_ref.py`, `tests/property/test_authmac.py`, this ADR; MODIFIED `CMakeLists.txt`
(`siphash.cpp` + `authmac001.cpp` + `authmacserver.cpp` into `seads_netinput`, `seads_netauthmac_test`
target, `netauthmac_bridge` ctest). **No shared-file change** (no `Stats`/accessor edit — layers 18/21
already added everything reported). guardian.yml UNCHANGED (ctest-only bridge, like layers 13–25).

## Alternatives rejected

* **Verify inside `CredentialTable`/`CommandQueue`.** The queue is the canonical ordering contract — a
  pure function of the command SET, blind to sockets/seats/credentials. The MAC check is a
  *did-this-identity-prove-itself* question that belongs at admission (the server's accept path), which
  decides WHICH commands to submit. The queue stays pure (same reasoning as layers 18/21).
* **Edit `broadcast_auth` to take a `SecretTable` + session key.** Would fold the challenge-response into
  the loop layers 21–23 depend on, for zero benefit to them. The sibling keeps that loop byte-for-byte,
  matching the one-axis-per-layer discipline.
* **Client-first handshake (no server challenge).** A client-supplied nonce/timestamp cannot give the
  server freshness — an attacker replays the whole captured HELLO. A **server-issued** challenge is what
  buys replay resistance; it costs one extra tiny downstream frame at accept, contained in the sibling.
* **Static MAC over the token only (no nonce).** That is a replayable password: an eavesdropper captures
  `(token, mac)` once and reuses it forever. Binding the MAC to a fresh per-connection nonce is the point.
* **A home-rolled hash / truncated SHA.** SipHash-2-4 is the standard short-input keyed MAC, is trivially
  reproducible cross-language (integer-only), and has an official test vector to pin against. Rolling our
  own would forfeit that anchor for no gain.
* **Make the records sealed `rails.wire` blocks (and reseal).** They carry no sim state and feed no hash —
  connection metadata, the `BIND-001`/`HELLO-001`/framing-envelope category, which took no seal. A reseal
  would misrepresent a transport handshake as a determinism-critical wire.
* **Ship the asymmetric signature now.** MAC vs signature is orthogonal to the binding mechanics this
  layer proves (identity is now *verified*, not merely *named*). Deferred and named as the next hardening,
  the same staging discipline every auth layer used.
