# ADR — Netcode layer 33: certificate chains / intermediate CAs — path validation up to a trusted root (`broadcast_authcertchain`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-05 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Layer 30 (`broadcast_authcert`) moved trust to a certificate authority: the server holds ONE CA public
key and each client presents a CA-signed CERT-001 binding its (token, seat, epoch, pubkey). Its honest-
scope caveat named exactly one follow-up: *"ONE self-signed root CA (no intermediate certificates /
chain depth)."* A production PKI does not sign every leaf with the root — it delegates issuance to
**intermediate CAs** (each itself certified by the root), so the root key can stay offline and issuance
scales. Layer 33 closes that gap: the client presents a **chain** and the server validates the **path**.

This is the last named open item in the netcode roadmap; with it, the authenticated/PKI arc
(21 → 26 → 27 → 30 → 33) has no remaining honest-scope caveats at the credential level.

## Decision

`broadcast_authcertchain` (`src/net/authcertchainserver.{h,cpp}`) is a SIBLING of `broadcast_authcert`:
its `select()` loop, seat binding, BIND-001 reply, `seat_authorizes`, and the CHALLENGE-001 nonce +
`derive_nonce` freshness are reused VERBATIM. Every prior server is byte-for-byte UNTOUCHED. Only the
credential grows from a single certificate to a **chain**:

- **HELLO-005** (`cert001.{h,cpp}` ↔ `tools/cert_ref.py`) = `[version 0x05][LEB n_certs] n×([LEB certlen]
  [cert])[LEB siglen][challenge sig]`. The chain is LEAF-FIRST, up toward the root; the root is NOT in
  the chain (the server holds it, exactly as layer 30 held the single CA key). Version 0x05 is distinct
  from HELLO-001..004 (all frozen). Each link is an ordinary CERT-001, reused verbatim — an
  intermediate's certificate binds ITS OWN (token, seat, epoch, pubkey) where the pubkey is the
  intermediate's signing key and the token is its identity (so a whole intermediate can be
  revoked/rotated). The challenge signature is `Ed25519_sign(leaf_seed, nonce‖leaf_token)` — layer 27's
  proof, reused. ZERO new det_math (nineteenth consecutive): token/seat/epoch/lengths ride the sealed
  GEO-001 codec; pubkeys + signatures are raw.
- **`cert001::verify_chain(root_pubkey, chain, max_depth)`** — the pure signature-path check: `1 ≤
  len ≤ max_depth`, each `chain[i]` signed by `chain[i+1].pubkey`, and `chain.back()` signed by the
  trusted root. Mirror of `cert_ref.verify_chain`.
- **`CaChainTable`** — the verifying roster: the root public key, a max depth, a revocation set, per-
  token epoch floors, and seat occupancy. `authenticate(chain, nonce, challenge_sig)` binds the LEAF's
  seat only when: the whole PATH verifies to the root, NO link's token is revoked, EVERY link's epoch
  ≥ its floor, the leaf's seat is free, AND the challenge signature verifies under the LEAF key; else
  SPECTATOR. Because the checks apply to every link, **revoking or rotating an INTERMEDIATE invalidates
  every leaf beneath it** — real chain-of-trust semantics. A depth-1 root-signed leaf is a valid
  degenerate chain (the layer-30 case).

## Consequences

- **Transport-only ⇒ no seal.** No `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes,
  protocol-7, session/event codec, or tuning touched. All 15 goldens byte-identical (Sphere
  `6914a994…`); sealed session/event digests unmoved; `guardian.yml` unchanged (ctest-only bridge, like
  every layer 13–32). No shared-file/`Stats` change; no new det_math or crypto (Ed25519/SHA-512 reused).
- **Determinism composes verbatim.** Any invalid chain rejects into the SAME SPECTATOR class —
  authentication stays an admission filter on the upstream that never touches the CommandQueue. So N
  chained clients each upstreaming ONLY their own seat's commands produce frames byte-identical to
  `session::build_server_frames`, regardless of connect order, byte reorder/chunking, chain depth, or a
  forger also connected (all dropped).
- **Verified locally (gcc + clang), all green.** BRIDGE `seads_netauthcertchain_test` (ctest **37→38**
  `netauthcertchain_bridge`): LEG 1 — 3 depth-2 chained identities (token order ≠ seat order
  100→2/200→0/300→1; scrambled/chunked) ⇒ 31 frames byte-identical; LEG 2 — a self-signed leaf (no
  path) + a leaf under a REVOKED intermediate + a STALE-epoch (rotated) leaf + foreign commands all
  rejected (`cmds_unauth=21`), all see the aircraft-0-only world byte-for-byte; LEG 3 — SHA-512 +
  Ed25519 pins + HELLO-005 codec pin (vs `cert_ref`) + `verify_chain` (self-signed/wrong-issuer/over-
  depth reject) + `CaChainTable` (chain seat, forged reject, intermediate subtree revoke, intermediate +
  leaf rotation, depth-1 root leaf, double-login→spectator, reclaim-own-seat). +15 property tests
  (`test_certchain.py`). `cert_ref.py` selftest PASS; layer-30 bridge regression PASS.

## Honest scope

A chain terminating at ONE trusted root (max depth bounded; no cross-signing or multiple trust
anchors). The revocation set + epoch floors are in-memory server state (a production CRL/OCSP
distributes them). Downstream send is blocking `send_all` (layer 30's base); the async/byte-cap/liveness
and catch-up folds are the natural mechanical follow-up (as 30 preceded 31/32), should full arc symmetry
be wanted — but the chain CAPABILITY, the named gap, is delivered here.

## Alternatives considered

- **A new record type per intermediate.** Rejected — reusing CERT-001 for every link keeps the codec and
  `verify_cert` untouched; an intermediate is just a certificate whose subject is a CA key.
- **Fold onto the async/catch-up servers now (layers 34/35).** Deferred — mechanical siblings of 31/32
  that add symmetry, not capability; the roadmap named only the chain, and this closes it.
