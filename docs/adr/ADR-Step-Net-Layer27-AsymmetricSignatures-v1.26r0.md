# ADR — Netcode layer 27: asymmetric signatures — a public-key identity, the server holds only a public key (`broadcast_authsig`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 26 (`broadcast_authmac`) made a client's seat a function of a **verified** credential: on accept
the server issues a fresh `CHALLENGE-001` nonce, the client answers `HELLO-002 = [token, mac]` with
`mac = SipHash(secret[token], nonce‖token)`, and the server recomputes the MAC to verify possession of a
pre-shared secret. That is strictly stronger than layer 21's bare-token lookup — a forger who knows only
the (public) token is rejected. But it is a **symmetric** MAC: the server holds a shared secret per
token, so the server itself (or anyone who leaks its roster) can **forge** any client's proof. Every auth
ADR (21/22/23) and layer 26 flagged the same next hardening in the same words: *"an asymmetric signature
(public-key identity: the server holds only a public key, never the client's secret, so a server-side
roster leak cannot impersonate a client) is the named next hardening."* Layer 27 closes it.

The determinism stake is unchanged from layers 18/21/26: the authoritative kernel's output must stay a
pure function of the canonically-ordered, **authorized** command SET. A stronger credential only sharpens
*which identities are admitted* — a forged/stale/unknown credential must be rejected into the **same
admission class** an unknown token already was, so the frames still cannot depend on transport
order/chunking.

The cross-toolchain bit-identity promise must survive real public-key "crypto". It does, because the
whole primitive is **integer arithmetic**: SHA-512 is 64-bit unsigned int ops (add/xor/and/rotate);
Ed25519 field/group arithmetic is a fixed 16-limb bignum (`gf[16]`, TweetNaCl's representation) — no
float, no libm, no `__int128`, no FMA-sensitive path. It lives in transport, outside the
kernel/world_hash, so the `det_math` transcendental ban does not even apply — and there are no
transcendentals to ban. Ed25519 signing is **deterministic** (RFC 8032: the per-message nonce is a hash
of the key + message), so a given (seed, message) yields one canonical 64-byte signature — byte-identical
between the C++ transcription of TweetNaCl and the RFC-8032 Python reference.

## Decision

### `broadcast_authsig` — a sibling of `broadcast_authmac`

`src/net/authsigserver.{h,cpp}` adds `broadcast_authsig`: `broadcast_authmac`'s single-thread `select()`
loop (blocking `send_all` downstream — see scope) with the **symmetric-MAC verify replaced by an Ed25519
SIGNATURE verify**. It is a **SIBLING** — it owns its own `AuthSigClient` struct (socket +
`StreamReassembler` + `seat`) and its own accept/loop code. `broadcast_authmac`, `broadcast_auth`,
`broadcast_bound`, `broadcast_input`, and every async/catch-up sibling are **byte-for-byte untouched**
(all their bridges still pass). **`CHALLENGE-001` + `derive_nonce` (from layer 26), `BIND-001`, and
`seat_authorizes` are reused verbatim**; the only new mechanism is *how the seat is proven*. **No
shared-file change at all**: `inputserver.h`'s `Stats` already carries every field this layer reports.

**1. `SHA-512` — the hash Ed25519 is built on (`src/net/sha512.{h,cpp}` ↔ `tools/sha512_ref.py`).**
Standard FIPS 180-4, `hash(data, len) -> 64 bytes`, pure `std::uint64_t` ops. Pinned to the **official
NIST vector** `SHA-512("abc")`. A standalone, separately-pinnable primitive (as SipHash was for the
layer-26 MAC). **No new det_math** (transport codec primitive, integer-only).

**2. `Ed25519` — the public-key signature (`src/net/ed25519.{h,cpp}` ↔ `tools/ed25519_ref.py`).** The C++
side is a faithful transcription of **TweetNaCl** (public-domain, minimal, known-correct, portable —
field elements are `gf = i64[16]`, so no `__int128` and byte-identical on every toolchain/arch), with the
hash `H` wired to our `sha512::hash`. The Python side is the **RFC 8032 reference algorithm** (Bernstein's
slow, obviously-correct reference). Because signing is deterministic and the signature/public-key bytes
are canonical, the two agree byte-for-byte — anchored by a **fixed-seed pin** (seed `00..1f`, message
`"SEADS-ed25519-pin"` → a specific 32-byte public key + 64-byte signature) that is **independently
confirmed against the `cryptography` library's Ed25519** (a widely-audited external implementation) in
`ed25519_ref.py`. API: `public_key(seed) -> pk[32]`, `sign(seed, msg) -> sig[64]`,
`verify(pk, msg, sig) -> bool`. **No new det_math.**

**3. `CHALLENGE-001` — the server's freshness contribution, reused from layer 26.** On accept the server
sends `CHALLENGE-001 = [version 0x01] [ZigZag+LEB128 nonce]` DOWN as the FIRST downstream frame (before
BIND), `nonce = derive_nonce(session_key, accept_counter) = SipHash(session_key, counter_le)`. A distinct
nonce per connection defeats replay of a captured HELLO. **Only the proof becomes asymmetric — the
freshness mechanism is layer 26's, unchanged** (still SipHash-based; `authmac001` is reused).

**4. `HELLO-003` — the client's identity claim + Ed25519 signature (`src/net/authsig001.{h,cpp}`).** In
response the client sends
`HELLO-003 = [version 0x03] [ZigZag+LEB128 token] [LEB128 siglen] [siglen raw signature bytes]`,
`sig = Ed25519_sign(private_seed[token], nonce_le ‖ token_le)` — the same 16-byte challenge message shape
layer 26 MAC'd (freshness ‖ identity). `token` + `siglen` ride the sealed `geo001` codec; the 64
signature bytes are raw/opaque ⇒ **no new codec primitive**. Version `0x03` is distinct from `HELLO-001`
(`0x01`, layer 21) and `HELLO-002` (`0x02`, layer 26), both **frozen** — each decoder rejects the others'
versions.

**5. `PubkeyTable` — the verifying roster, holding ONLY public keys.** `enroll(token, seat, pubkey[32])`
registers a token's designated seat AND its 32-byte Ed25519 **public** key (no secret). `authenticate(
token, nonce, sig)` returns:
* the token's designated seat, when the token is enrolled, its seat is free, **AND**
  `Ed25519_verify(pubkey, nonce‖token, sig)` succeeds;
* **SPECTATOR** (`-1`) otherwise — unknown token, double-login (seat held), OR a **bad signature**
  (forgery / wrong key / stale nonce). A forgery is rejected into the **exact same SPECTATOR class** as an
  unknown token. Mirrored in `tools/authsig_ref.PubkeyTable`.

**6. `BIND-001` + authorization (reused from layers 18/21/26).** The server replies with the same one-time
`BIND-001` naming the resolved seat, and authorizes upstream commands with the same
`seat_authorizes(seat, aircraft) := seat >= 0 && aircraft == seat`.

### The headline over layer 26

The `PubkeyTable` holds **no secret capable of signing** — only public keys. A full **roster leak** (every
enrolled public key) still cannot impersonate any client, because a valid `HELLO-003` requires the private
seed the server never sees. Layer 26's server-held secret *could* forge; this one cannot. That is real
public-key identity — the named next hardening, delivered.

### `CHALLENGE-001` / `HELLO-003` are transport metadata, NOT sealed wires

Like `BIND-001` / `HELLO-001` / `HELLO-002` / the framing envelope, both records carry no simulation
value, feed no hash, and are not `rails.wire` blocks. The determinism claim rests on the **authorization
filter** (unchanged from layers 18/21/26), not on these records or the signature. So this layer takes
**no seal** even though it adds a record + two crypto primitives, exactly as BIND/HELLO did. Cross-impl
byte parity is still pinned (shared known-encoding vector `[0x03,0x0E,0x02,0xAA,0xBB]` for a
token-7/2-byte-sig HELLO, the SHA-512 and Ed25519 official/fixed vectors) — the discipline, not a seal
trigger.

### Why the verified binding preserves determinism

Verification is an admission filter on the upstream, exactly like the MAC/token lookup it sharpens: it
decides *which seat (if any)* each connection holds, and a command from an unbound seat (forger,
spectator, or foreign) is indistinguishable from one that never arrived — the `OUT_OF_RANGE` reject's
determinism class. The signature check, challenge, and `BIND-001` record touch only bookkeeping and the
downstream; **the `CommandQueue` never sees a credential**. So when N authenticated clients each upstream
ONLY their own seat's commands and their union is the whole scenario command set (delivered before each
`apply_tick`), the produced frames are **byte-identical to `session::build_server_frames`** — regardless
of which identity connected in which order, how the upstream bytes were reordered/chunked, or that a
**forger** (holding the whole public roster) and an unknown-token spectator were also connected and
upstreaming (all their commands dropped).

### Honest scope (this cut)

* **Fixed roster, no PKI.** Keys are enrolled from a pre-shared roster; a production system carries
  certificates / key rotation / revocation. Orthogonal to the binding mechanics this layer proves.
* **Blocking downstream.** `broadcast_authsig` uses `broadcast_authmac`/`broadcast_auth`'s blocking
  `send_all` base. Merging the verifying signature with the layer-16/19/22 async hygiene, or layer-20/23
  catch-up, is the natural mechanical follow-up (swap the seat-assignment call, as layer 22 did over 21).
* **The session key is a passed-in seed.** A production server derives it from a CSPRNG; the bridge fixes
  it for reproducibility. Determinism depends only on the nonce being fresh per connection, which holds
  under any seed. (Ed25519 signing itself needs no randomness — it is deterministic.)

## Verification

* **Bridge `seads_netauthsig_test`** (ctest `netauthsig_bridge`, native-x64 leg like layers 7–26), over
  real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every client has
  sent its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — verified public-key seats, invariant to join order (the headline):** THREE clients each
    hold a distinct token AND its correct private seed, are challenged, SIGN, and are bound to the seat
    their token DESIGNATES, deliberately with **token order ≠ seat order** (100→2, 200→0, 300→1). Each
    upstreams ONLY that seat's commands (distinct scrambles: reversed @1 B, forward @7 B, reversed @3 B);
    their union is the whole INPUT-SK-001 command set, so each client's downstream is **byte-identical to
    `build_server_frames`** (31 frames). Asserts the SPECIFIC seat per token, `cmds_ok == 6`,
    `cmds_unauth == stale == oob == 0`, `joins == 3`, every `challenge_ok`.
  * **LEG 2 — verify + authorization boundary (the new capability):** THREE clients upstream the WHOLE
    scenario. Client A holds token 200 AND its correct seed → seat 0; its foreign commands dropped.
    Client B knows token 300 but signs with the **WRONG private key** → bad signature → SPECTATOR; ALL
    commands dropped — *even holding the whole public roster, an attacker cannot sign*. Client C presents
    an UNKNOWN token 999 → SPECTATOR; all dropped. All three receive the IDENTICAL aircraft-0-only world
    byte-for-byte. `cmds_ok == 3`, `cmds_unauth == 15` (A's 3 foreign + B's 6 + C's 6), A's `BIND` seat
    `== 0`, B's and C's `BIND` seat `== -1`.
  * **LEG 3 — SHA-512 + Ed25519 pins + `HELLO-003` codec + `PubkeyTable` verify (in-process):** the
    OFFICIAL `SHA-512("abc")` vector; the Ed25519 fixed-seed pin (public key + signature, confirmed vs the
    `cryptography` library) + a tamper reject; the `HELLO-003` codec (known-encoding pin
    `[0x03,0x0E,0x02,0xAA,0xBB]` vs `authsig_ref.py`, round-trip with real 64-byte sigs, wrong-version +
    not-interchangeable rejects); and `PubkeyTable` (verify, **forgery-reject** with wrong key,
    **replay-reject** of a valid signature under a different nonce, unknown→spectator,
    double-login→spectator, release-then-reclaim-OWN-seat) + the reused `seat_authorizes`.
* **Property tests +16** (`test_authsig.py`): SHA-512 official vector + determinism + message
  sensitivity; Ed25519 fixed pin + round-trip + determinism + rejects tampered signature / wrong message /
  wrong public key; `HELLO-003` round-trip + version enforcement + the pin; `PubkeyTable` — a valid
  signature binds the designated seat invariant to auth order over a random bijection; a forged/wrong-key
  credential is rejected (seat left free) while the genuine holder still succeeds; a replayed signature
  under a different nonce is rejected; unknown→spectator; no double-booking + own-seat reclaim under a
  randomised join/leave interleaving; the challenge is fresh per accept; the reused `seat_authorizes`.
* **Gates:** full **ctest 31→32** (GCC + Clang, all sealed net bridges still reconstruct `966aca05…`, the
  new `netauthsig_bridge` PASS on both); property tests **288→304**; **all 15 goldens byte-identical**
  (Sphere `6914a994…`, scenario goldens via `lockstep_equal`, WEAPON-001 protocol-7 via `weapon_byteexact`
  — all PASS on GCC); sealed **session + event digests unmoved** (`session_reconstruct`, `event_reliable`
  PASS); `sha512_ref.py` / `ed25519_ref.py` / `authsig_ref.py` selftests PASS (Ed25519 confirmed vs
  `cryptography`).

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/sha512.{h,cpp}`, `src/net/ed25519.{h,cpp}`,
`src/net/authsig001.{h,cpp}`, `src/net/authsigserver.{h,cpp}`, `src/net/netauthsig_test_main.cpp`,
`tools/sha512_ref.py`, `tools/ed25519_ref.py`, `tools/authsig_ref.py`, `tests/property/test_authsig.py`,
this ADR; MODIFIED `CMakeLists.txt` (`sha512.cpp` + `ed25519.cpp` + `authsig001.cpp` + `authsigserver.cpp`
into `seads_netinput`, `seads_netauthsig_test` target, `netauthsig_bridge` ctest). **No shared-file
change** (no `Stats`/accessor edit). guardian.yml UNCHANGED (ctest-only bridge, like layers 13–26).

## Alternatives rejected

* **Keep the symmetric MAC (layer 26).** It cannot deliver the headline — a server-held secret can forge,
  and a roster leak impersonates. Asymmetric verification is the whole point of this layer.
* **A home-rolled or exotic signature.** Ed25519 is *the* modern standard, deterministic (no RNG to get
  wrong), integer-only (reproducible cross-toolchain), and has official RFC-8032 test vectors +
  independent libraries to pin against. Rolling our own would forfeit that anchor for no gain.
* **secp256k1 / ECDSA.** Also viable, but Ed25519's determinism (no per-signature nonce RNG) and its
  minimal, portable, public-domain TweetNaCl reference make it the lower-risk, byte-reproducible choice.
* **A hash-based (Lamport/Merkle) signature.** Genuinely asymmetric and even simpler (no curve), but a
  Merkle tree bounds a keypair to `2^h` authentications, complicating the unlimited-reconnect story layers
  21/26 exercise. Ed25519 re-signs each fresh challenge with no state.
* **Verify inside `PubkeyTable`/`CommandQueue` vs. at admission.** The queue is the canonical ordering
  contract — a pure function of the command SET, blind to sockets/seats/credentials. The signature check
  is a *did-this-identity-prove-itself* question that belongs at admission (same reasoning as layers
  18/21/26). The queue stays pure.
* **Edit `broadcast_authmac`/`broadcast_auth` to take a `PubkeyTable`.** Would fold the signature verify
  into loops layers 21–26 depend on, for zero benefit to them. The sibling keeps those loops byte-for-byte,
  matching the one-axis-per-layer discipline.
* **Client-first handshake / static signature (no nonce).** A client-supplied nonce cannot give the server
  freshness; a static signature over the token is a replayable password. A **server-issued** challenge is
  what buys replay resistance, reused verbatim from layer 26.
* **Make the records sealed `rails.wire` blocks (and reseal).** They carry no sim state and feed no hash —
  connection metadata, the `BIND-001`/`HELLO-001`/`HELLO-002`/framing-envelope category, which took no
  seal. A reseal would misrepresent a transport handshake as a determinism-critical wire.
