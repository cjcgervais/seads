# ADR — Netcode Layer 5: the server↔client SESSION loop (rides seal v1.12r0)

**Status:** Accepted · **Date:** 2026-06-30 · **Seal:** rides ATM-Sphere **v1.12r0** (no rail/golden/wire change)
**Tier:** 2 (net layer; commit + Chronicle receipt, like layer 4a interp). No `/seal`.

## Context

Steps 1–6 built the netcode stack *piece by piece* — the GEO-001 codec (layer 1), the 20 Hz
snapshot framing (layer 2, now protocol 4 with KIN-002 + WEAPON-001), the loopback lockstep desync
tripwire (layer 3), remote interpolation (layer 4a), and client-side prediction (layer 4b) — and
v1.12r0 put the gunnery state (hp / fire_cd / live rounds) on the wire. But each layer was proven in
*isolation*. Nothing yet **shipped the WEAPON-001 frames between two endpoints** and reconstructed a
whole dogfight from the received bytes. That end-to-end path is what makes multiplayer real: a client
that has only the wire must be able to draw the fight — remote positions, its own smooth motion, HP
bars, kills, tracer rounds.

## Decision

Add **netcode layer 5 — the SESSION loop** (`tools/session_ref.py` ↔ `src/net/session.{h,cpp}`,
gated by `seads_session_test` + `gen_session_vectors.py` → `session_vectors.h` + `tests/property/
test_session.py`), an in-process **server → transport → client** loop over the canonical
**SESSION-SK-001** scenario (the 3-ship gundemo shape: a P-47D guns down an A6M2 while a Spitfire
maneuvers). It **composes the existing layers** rather than adding new wire/kernel state:

- **Server** drives the sealed kernel over a scripted multi-aircraft dogfight and, every `snap_every`
  ticks (20 Hz), serializes the FULL protocol-4 world (`netsnap::encode_snapshot`) — every aircraft
  (GEO + KIN-002 + WEAPON hp/fire_cd) and every live round.
- **Transport** ships frames server→client with a fixed integer **latency** (`lag_ticks`) and
  **deterministic packet loss** (`drop_emit_ticks`).
- **Client** reconstructs the fight from the decoded bytes, splitting responsibility exactly as a real
  netcoded game does:
  - **OWN ship (id 0): PREDICTED @ now** via `predict::Predictor` (layer 4b), reconciled against each
    decoded frame — the realistic **LOSSY-decode** reseed path (snap to the dequantized wire state,
    replay buffered inputs). Firing never touches kinematics, so the predictor omits the fire bit and
    predicts pure motion; hp/rounds are wire-sourced.
  - **REMOTES: INTERPOLATED ~150 ms in the past** via `interp::SnapshotBuffer` (layer 4a).
  - **HP / KILLS / ROUNDS: from the freshest delivered frame's WEAPON section** (nearest-frame
    semantics — hp is discrete, rounds transient, so no interpolation), the same pattern the renderer's
    `sample_weapons` uses.

The reconstructed per-tick **client view** (own predicted geometry + remote interpolated geometry +
wire-sourced weapon state) is serialized to canonical bytes (every field through the **same GEO-001
integer quantize** the wire uses) and hashed; the whole-session SHA-256 **digest** is the cross-impl
parity artifact the C++ mirror reproduces bit-for-bit.

### Key decisions

1. **Lossy transport, still bit-exact.** Packet loss + quantization destroy *information* but are
   perfectly *reproducible*, and every reconstruction op is det_math (the predictor's kernel), pure
   IEEE +−×÷ (interp), or integer (quantize / transport lag+drop / freshest-frame compare). So the
   whole session reconstructs to the identical bytes on MSVC/GCC/Clang × x64/AArch64. This is a
   **stronger** proof than layer 4b's digest, which reconciled against *canonical* state — here the
   own ship reconciles against the **dequantized wire** (the realistic remote/late-join path) and the
   entire fight, kill included, still reproduces to the bit.
2. **Own = predict @ now, remote = interp @ past.** The own ship is rendered at "now" with zero delay
   (prediction); remotes ~150 ms behind (interpolation). This is the whole point of running both 4a
   and 4b together and is the first place they compose on one timeline.
3. **Weapons are wire-sourced, not local.** The client's HP/kills/rounds come strictly from the
   decoded WEAPON section — the kill *replicates* over the wire (`test_client_hp_mirrors_server_exactly`
   proves the client HP equals the server's authoritative HP to the quantum; hp is integer-valued so
   the 1e3-scale wire carries it losslessly).
4. **Its own library.** Like lockstep/predict, the session DRIVES the kernel (server + own predictor),
   so `seads_session` links `seads_predict` + `seads_net` + kernel/replay — it is NOT folded into the
   pure wire lib `seads_net`.
5. **No seal.** It composes the EXISTING protocol-4 wire; no rail/golden/kernel/det_math change. All 9
   goldens stay byte-identical (validate_snapshot/validate_scenarios PASS). Tier-2 ledger like interp.

## Alternatives considered

- **Reconcile the own ship against canonical state** (as layer 4b's bit-exact digest did) — rejected:
  it would not exercise the realistic decode path. Using the lossy wire is both more faithful AND still
  deterministic, so we get a stronger gate for free.
- **A new golden / scenario JSON** — rejected: SESSION-SK-001 is defined inline (self-contained
  hex-float vectors), exactly like PREDICT-SK-001, so it never enters the golden/seal machinery.
- **Prune the interp buffer** — deferred: the scenario is short (200 ticks / 41 frames), so we keep all
  delivered frames for a simpler deterministic freshest-frame scan. A real client would prune.

## Consequences

- New gate leg everywhere: `seads_session_test` (ctest `session_reconstruct`, 8/8 → **9/9** GCC+Clang),
  `gen_session_vectors.py --check`, the reference self-test, the `session` receipt gate (13 → **14**),
  and +6 property tests (100 → **106**). guardian.yml gains one CI leg per toolchain matrix cell.
- **Known limits (documented):** the own predictor can't foresee its OWN death (hp is wire-sourced), so
  a would-be-dead own ship keeps flying until the next frame — out of scope (SESSION-SK-001's own ship
  survives). The transport models latency + loss but not reordering/jitter (fixed lag preserves order).
- **Natural next steps:** a genuinely cross-*process* transport (sockets) over the same frames; hp/round
  interpolation or explicit kill/impact event messages; wiring this loop into the live viewer.
