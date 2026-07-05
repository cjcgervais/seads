# ADR — Netcode layer 30: certificate PKI — one CA key, revocation, key rotation (`broadcast_authcert`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 27 (`broadcast_authsig`) made a client's seat a function of a **verified Ed25519 signature** over a
fresh server challenge, with the server holding only the client's **public key** (`PubkeyTable`). That is
real public-key identity — a full roster leak cannot forge. But layer 27's honest-scope caveat, flagged in
its own ADR and in the auth ADRs before it, was: *"keys are enrolled from a fixed roster; a production
system carries certificates / key rotation / revocation."* The `PubkeyTable` is a **fixed, per-client
roster**:

* **Enrollment** means editing the server — every new identity is a server change.
* There is **no revocation** — a compromised private key cannot be disowned; the identity keeps its seat.
* There is **no key rotation** — a client cannot replace its key without a coordinated server edit.

Layer 30 closes that caveat by moving trust from a per-client roster to a **certificate authority (CA)**.

The determinism stake is unchanged from layers 18/21/26/27: the authoritative kernel's output must stay a
pure function of the canonically-ordered, **authorized** command SET. A richer credential only sharpens
*which identities are admitted* — a non-CA / revoked / stale / forged credential must reject into the
**same admission class** an unknown token already did, so the frames still cannot depend on transport
order/chunking. And the cross-toolchain bit-identity promise still holds for free: the only crypto is
Ed25519/SHA-512, **reused verbatim from layer 27** (integer-only, transport, outside the kernel/world_hash
— the `det_math` transcendental ban does not even apply). **No new det_math (eighteenth consecutive
integration rung).**

## Decision

### `broadcast_authcert` — a sibling of `broadcast_authsig`

`src/net/authcertserver.{h,cpp}` adds `broadcast_authcert`: `broadcast_authsig`'s single-thread `select()`
loop (blocking `send_all` downstream — see scope) with the **enrolled-public-key verify replaced by a
CA-signed CERTIFICATE verify**. It is a **SIBLING** — it owns its own `AuthCertClient` struct (socket +
`StreamReassembler` + `seat`) and its own accept/loop code. `broadcast_authsig`, `broadcast_authmac`,
`broadcast_auth`, `broadcast_bound`, `broadcast_input`, and every async/catch-up sibling are
**byte-for-byte untouched** (all their bridges still pass). **`CHALLENGE-001` + `derive_nonce` (layer 26),
the Ed25519 challenge proof `authsig001::{sign,verify}_challenge` (layer 27), `BIND-001`, and
`seat_authorizes` are reused verbatim.** **No shared-file change at all**: `inputserver.h`'s `Stats`
already carries every field this layer reports.

**1. `CERT-001` — a CA-signed certificate (`src/net/cert001.{h,cpp}` ↔ `tools/cert_ref.py`).** The
certificate binds an identity to a seat, an epoch, and a public key, vouched for by the CA:

```
cert_body   = [cert_version 0x01] [ZigZag+LEB128 token] [ZigZag+LEB128 seat]
              [ZigZag+LEB128 epoch] [32 raw client-pubkey bytes]
certificate = cert_body [LEB128 ca_siglen = 64] [64 raw CA signature bytes]
    ca_sig = Ed25519_sign(ca_seed, cert_body)
```

`token/seat/epoch` ride the sealed `geo001` ZigZag+LEB128 codec; the client pubkey + CA signature are
raw/opaque ⇒ **no new codec primitive**. `verify_cert(ca_pubkey, cert)` verifies the CA signature over the
**exact `body` byte-slice recorded at decode** (never a re-encoding), so a self-signed or tampered
certificate fails. The CA issues certificates **offline**; the server only holds the CA public key.

**2. `HELLO-004` — the client's handshake response (same file).** In response to the challenge the client
sends both its certificate and a proof it holds the certified private key:

```
HELLO-004 = [version 0x04] [LEB128 certlen] [cert bytes] [LEB128 siglen = 64] [64 raw challenge-sig bytes]
    challenge_sig = Ed25519_sign(client_seed, nonce_le ‖ token_le)   (layer 27's proof, unchanged)
```

Version `0x04` is distinct from `HELLO-001/002/003` (`0x01/0x02/0x03`), all **frozen** — each decoder
rejects the others' versions.

**3. `CaTable` — the verifying server, trusting ONE CA public key (`src/net/authcertserver.{h,cpp}` ↔
`tools/cert_ref.CaTable`).** State: the 32-byte CA public key, a **revocation set** (revoked tokens), a
**per-token epoch floor** (rotation; default 0), and seat occupancy — **no per-client keys, no secret**.
`authenticate(cert, nonce, challenge_sig)` returns the certificate's designated seat only when **all** of:
* the certificate **verifies under the CA public key** (CA-signed, untampered);
* the token is **not revoked**;
* `epoch >= min_epoch[token]` (not **superseded by a rotation**);
* the seat is in range **and free**;
* the challenge signature **verifies under the certificate's embedded client public key** (possession);

otherwise **SPECTATOR** (`-1`) — a non-CA / tampered / revoked / stale / out-of-range / held-seat /
forged-possession credential all reject into the **exact same class** as an unknown token did in layer
21/26/27. `revoke`/`unrevoke` and `set_min_epoch` are the CRL / rotation controls; `release` frees a seat
on leave (reconnect reclaims its own seat).

**4. `BIND-001` + authorization (reused from layers 18/21/26/27).** The server replies with the same
one-time `BIND-001` naming the resolved seat, and authorizes upstream commands with the same
`seat_authorizes(seat, aircraft) := seat >= 0 && aircraft == seat`.

### The headline over layer 27

The server holds a **single CA public key**, not a per-client roster:
* **Enrollment without a server change** — the CA issues a certificate offline; the server verifies it. A
  new identity never touches the server.
* **Revocation** — a compromised identity is disowned with `revoke(token)`; its certificate, however
  valid, is rejected.
* **Key rotation** — a client replaces its key by re-certifying at a higher **epoch**; the server raises
  the floor with `set_min_epoch(token, e)`, retiring every older certificate for that identity without
  touching any other. A real certificate PKI.

### `CHALLENGE-001` / `HELLO-004` / `CERT-001` are transport metadata, NOT sealed wires

Like `BIND-001` / `HELLO-001..003` / the framing envelope, these records carry no simulation value, feed
no hash, and are not `rails.wire` blocks. The determinism claim rests on the **authorization filter**
(unchanged from layers 18/21/26/27), not on these records or the certificate. So this layer takes **no
seal** even though it adds two records + a table, exactly as the earlier HELLO/BIND records did.
Cross-impl byte parity is still pinned (shared known-encoding vectors — the `CERT-001` body
`[0x01,0x0E,0x02,0x00, 0x11×32]` and the `HELLO-004` `[0x04,0x02,0xAA,0xBB,0x01,0xCC]`, plus the reused
SHA-512/Ed25519 official/fixed vectors) — the discipline, not a seal trigger.

### Why the verified binding preserves determinism

Verification is an admission filter on the upstream, exactly like the signature/MAC/token lookup it
sharpens: it decides *which seat (if any)* each connection holds, and a command from an unbound seat
(non-CA, revoked, stale, spectator, or foreign) is indistinguishable from one that never arrived — the
`OUT_OF_RANGE` reject's determinism class. The certificate check, challenge, revocation/epoch state, and
`BIND-001` record touch only bookkeeping and the downstream; **the `CommandQueue` never sees a
credential**. So when N certified clients each upstream ONLY their own seat's commands and their union is
the whole scenario command set (delivered before each `apply_tick`), the produced frames are
**byte-identical to `session::build_server_frames`** — regardless of which identity connected in which
order, how the upstream bytes were reordered/chunked, or that a self-signed forger, a revoked identity, and
a stale-epoch client were also connected and upstreaming (all their commands dropped).

### Honest scope (this cut)

* **One self-signed root CA — no chain of trust.** A single CA verifies leaf certificates directly; there
  are no intermediate certificates / path validation depth. Adding a chain is a mechanical extension
  (verify each link up to the trusted root) orthogonal to the binding mechanics this layer proves.
* **In-memory CRL / epoch floors.** Revocation and rotation state live in the server process. A production
  system distributes them (a signed CRL / OCSP responder / short-lived certificates). The *effect* on
  admission is what this layer proves; the *distribution* is orthogonal.
* **Blocking downstream.** `broadcast_authcert` uses `broadcast_authsig`'s blocking `send_all` base.
  Merging the certificate verify with the layer-16/19/22/28 async hygiene, or layer-20/23/29 catch-up, is
  the natural mechanical follow-up (swap the seat-assignment call, as layers 22/28 did over 21/27).
* **The session key is a passed-in seed** (fixed in the bridge for reproducibility; CSPRNG in production).
  Determinism depends only on the nonce being fresh per connection, which holds under any seed. Ed25519
  signing itself needs no randomness.

## Verification

* **Bridge `seads_netauthcert_test`** (ctest `netauthcert_bridge`, native-x64 leg like layers 7–29), over
  real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous — `on_frame(0)` blocks until every client has
  sent its commands, so they are ingested before their `apply_tick`), finite watchdog:
  * **LEG 1 — CA-certified seats, invariant to join order (the headline):** THREE clients each hold a
    distinct CA-issued certificate AND the matching private seed, are challenged, present the certificate +
    SIGN, and are bound to the seat their certificate DESIGNATES, deliberately with **token order ≠ seat
    order** (100→2, 200→0, 300→1). Each upstreams ONLY that seat's commands (distinct scrambles: reversed
    @1 B, forward @7 B, reversed @3 B); their union is the whole INPUT-SK-001 command set, so each client's
    downstream is **byte-identical to `build_server_frames`** (31 frames). Asserts the SPECIFIC seat per
    token, `cmds_ok == 6`, `cmds_unauth == stale == oob == 0`, `joins == 3`, every `challenge_ok`. The
    server enrolled only the ONE CA public key.
  * **LEG 2 — CA-verify + revocation + rotation boundary (the new capabilities):** FOUR clients upstream
    the WHOLE scenario. Client A holds a genuine token-200 certificate + correct seed → seat 0; its foreign
    commands dropped. Client B presents a **SELF-SIGNED** (non-CA) certificate → SPECTATOR. Client C holds
    a genuine, correctly-signed token-300 certificate the server has **REVOKED** → SPECTATOR. Client D
    holds a genuine token-100 certificate whose epoch is **STALE** after the server rotated that identity
    (`set_min_epoch(100, 5)`) → SPECTATOR. All four receive the IDENTICAL aircraft-0-only world
    byte-for-byte. `cmds_ok == 3`, `cmds_unauth == 21` (A's 3 foreign + B/C/D's 6 each), A's `BIND` seat
    `== 0`, B/C/D's `BIND` seat `== -1`.
  * **LEG 3 — SHA-512 + Ed25519 pins + `CERT-001`/`HELLO-004` codec + `CaTable` verify (in-process):** the
    OFFICIAL `SHA-512("abc")` vector; the Ed25519 fixed-seed pin (public key + signature, confirmed vs the
    `cryptography` library in `ed25519_ref.py`); the `CERT-001` body known-encoding pin
    `[0x01,0x0E,0x02,0x00, 0x11×32]` + the `HELLO-004` pin `[0x04,0x02,0xAA,0xBB,0x01,0xCC]` (both vs
    `cert_ref.py`), round-trip with real certificates/signatures, wrong-version rejects; and `CaTable`
    (CA-verify, **self-signed reject**, **stolen-cert-wrong-key** (forged possession) **reject**,
    **replay-reject** of a valid proof under a different nonce, **revoke/un-revoke**, **epoch-floor
    rotation**, unknown→spectator, double-login→spectator, release-then-reclaim-OWN-seat) + the reused
    `seat_authorizes`.
* **Property tests +14** (`test_cert.py`): `CERT-001` body + `HELLO-004` known-encoding pins; certificate
  round-trip + CA-verify over random `(token, seat, epoch)`; `HELLO-004` round-trip; version enforcement;
  **self-signed certificate rejected** (seat left free) — the PKI headline; **tampered certificate
  rejected**; a valid certificate binds the designated seat **invariant to auth order** over a random
  bijection; **stolen-cert-wrong-key rejected** while the genuine holder succeeds; **replay-reject** under
  a different nonce; **revocation targets exactly one identity** + un-revoke restores; **key rotation**
  supersedes the old epoch while a re-issued higher-epoch/new-key certificate authenticates; no
  double-booking + own-seat reclaim under a randomised join/leave interleaving; the reused `seat_authorizes`.
* **Gates:** full **ctest 34→35** (GCC + Clang, all sealed net bridges still reconstruct their sealed
  digests, the new `netauthcert_bridge` PASS on both); property tests **311→325**; **all 15 goldens
  byte-identical** (Sphere `6914a994…`); `spec_monotone_check` + `det_math_oracle` PASS; `cert_ref.py`
  selftest PASS.

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire snapshot bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved, no seal.** Diff: NEW `src/net/cert001.{h,cpp}`, `src/net/authcertserver.{h,cpp}`,
`src/net/netauthcert_test_main.cpp`, `tools/cert_ref.py`, `tests/property/test_cert.py`, this ADR;
MODIFIED `CMakeLists.txt` (`cert001.cpp` + `authcertserver.cpp` into `seads_netinput`,
`seads_netauthcert_test` target, `netauthcert_bridge` ctest). **No shared-file change** (no `Stats`/accessor
edit); no new crypto/Python ref (Ed25519/SHA-512 from layer 27 reused). guardian.yml UNCHANGED (ctest-only
bridge, like layers 13–29).

## Alternatives rejected

* **Keep the fixed `PubkeyTable` (layer 27).** It cannot deliver the headline — no enrollment without a
  server change, no revocation, no rotation. A CA is the standard answer to exactly those three needs.
* **A separate revocation record on the wire.** Revocation is server admission state, not something a
  client sends. Modeling it as a CRL the CA distributes and the server consults keeps the client protocol
  identical to a normal login (present a certificate) — a revoked client cannot opt out of its own
  revocation.
* **Bind the seat outside the certificate (cert = identity only, seat via a roster).** That would keep a
  per-client server roster for seats, re-introducing the enrollment problem the CA removes. Signing the
  seat INTO the certificate lets the CA fully authorize an identity offline.
* **Certificate chains / intermediate CAs now.** Genuinely useful at scale, but a mechanical extension
  (verify each link to the trusted root) orthogonal to the leaf-binding + revocation + rotation mechanics
  this layer establishes. Deferred to keep one axis per layer.
* **Short-lived certificates instead of an epoch floor.** A validity window (not-before / not-after) is the
  production analogue, but it needs a trusted clock — and wall-clock time is banned near the sim by
  doctrine. An **epoch** is a monotone, clock-free rotation counter that gives the same "retire the old
  key" effect deterministically. A time window can layer on top later where a trusted clock exists.
* **Edit `broadcast_authsig` to take a `CaTable`.** Would fold the certificate verify into a loop layers
  27–29 depend on, for zero benefit to them. The sibling keeps those loops byte-for-byte, matching the
  one-axis-per-layer discipline.
* **Make the records sealed `rails.wire` blocks (and reseal).** They carry no sim state and feed no hash —
  connection metadata, the `BIND-001`/`HELLO-*`/framing-envelope category, which took no seal. A reseal
  would misrepresent a transport handshake as a determinism-critical wire.
