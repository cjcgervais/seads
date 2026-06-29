# ADR-Step6-Prediction-v1.4r0 — Client-side prediction + KIN wire reseal (netcode layer 4b)

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — kernel/tooling
**Seal:** ATM-Sphere **v1.4r0** (Tier-1 reseal — new wire fields/scales; no golden moved)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Netcode layers 1–4a are in (GEO-001 codec, 20 Hz snapshot framing, loopback lockstep tripwire,
remote interpolation). Layer 4b is the other half of server-authoritative state-sync: **predict
your OWN aircraft** forward from local input each tick and **reconcile** against authoritative
snapshots. Accurate reconciliation re-seeds a kernel from an authoritative state, and a kernel
needs the FULL per-aircraft state `(lat, lon, psi, phi, alt, tas)` to step. The wire (GEO-001)
carried only `(lat, lon, psi→bearing, alt)`; `phi` (bank) and `tas` (speed) had **no scale**.
Layer 2 documented this as a deferred field-scope: carrying them is a **rail reseal**. This ADR
performs that reseal and builds the prediction harness on top.

## 2) Decision

### 2a) Wire reseal — auxiliary KIN-001 block (keep GEO-001 geography-only)
Of the two options the layer-2 ADR named (extend GEO-001 scales **vs** an auxiliary
non-geographic block), we chose the **auxiliary block**:

- `config/rails/atm.json` `wire` gains a `kin` sub-block — **KIN-001**: `phi_scale = 1e6`
  (micro-degree, mirrors `bearing`), `speed_scale = 1e3` (mm/s, mirrors `alt`). Header
  `version 130 → 140`, seal `v1.3r0 → v1.4r0`.
- The snapshot framing becomes **two self-delimiting sections** (`SNAPSHOT_PROTOCOL 1 → 2`):
  1. **GEO section** `n × (id, GeoPoint[lat,lon,bearing,alt])` — GEO-001, **byte-unchanged**;
     the sealed `geo001` codec and its parity vectors are **untouched**.
  2. **KIN section** `n × (id, phi_q, tas_q)` — present iff `protocol ≥ 2`. Same ZigZag+LEB128
     field codec; `phi`/`tas` live **outside** the GeoPoint because they are not geography.
- `snapshot_ref.py` + `src/net/snapshot.{h,cpp}`: `EntityState`/`from_kernel` gain `phi`/`tas`;
  encode/decode append the KIN section; the KIN id is checked against the GEO id (section
  alignment guard). Protocol-1 snapshots still round-trip (back-compat; KIN omitted).

### 2b) Prediction harness — run the real kernel, reconcile bit-exact
Same proven shape as lockstep (Python reference ↔ generated vectors ↔ C++ digest-exact test):

- `tools/predict_ref.py` — canonical reference. A `Predictor` holds a kernel copy + a ring
  buffer of local `(tick, Command)`. `predict(cmd)` steps the **real sealed kernel** one tick;
  `reconcile(server_tick, auth)` snaps the own aircraft to the authoritative state, drops inputs
  `≤ server_tick`, and **replays** the rest forward. Scenario `PREDICT-SK-001`: 1 own aircraft,
  300 ticks, snapshots at 20 Hz (every 5 ticks), ~100 ms latency (`lag = 10` ticks).
- `src/net/predict.{h,cpp}` — C++ mirror in a **new `seads_predict` lib** (links
  `seads_kernel`+`seads_replay`; it drives the kernel, so — like `seads_lockstep` — it is NOT
  folded into the pure wire lib `seads_net`). Reconcile re-seeds by **rebuilding a kernel via
  the existing public `Kernel::add()` + `step()`** — kernel math and the canonical snapshot
  byte layout are **untouched**. The only kernel edit is two additive read-only getters
  (`phi()`, `tas()`), symmetric with the existing `lat/lon/psi/alt`.
- `tools/gen_predict_vectors.py` → `src/net/predict_vectors.h`; `seads_predict_test`
  (`predict_equal`): nominal **seamless** (predicted == truth every tick), whole-sequence
  **digest** matches the reference, per-tick checkpoints, **heal** control (perturbed initial
  alt diverges at tick 1, reconciles back exactly at `HEAL_TICK = 15`), **negative control**
  (same desync without reconcile stays broken).
- `tests/property/test_predict.py` — Hypothesis: seamless nominal, reconcile heals **any**
  state desync at the same tick, drift persists without reconcile, the input buffer stays
  **bounded** by the latency window (O(lag), not O(ticks)), and the **lossy-wire reseed is
  bounded** (not exact).
- Gate wiring: `guardian.yml` (gen `--check` + reference self-test + `seads_predict_test`
  per leg), `make_receipt.py` (`predict` gate), `CMakeLists.txt`.

## 3) Rationale
- **GEO-001 stays geography-only.** `phi`/`tas` are not geographic; an auxiliary KIN block keeps
  the highest-risk sealed codec (and its byte vectors) frozen and makes “this is bank/speed, not
  geography” explicit in the format. Smallest blast radius on the determinism core.
- **Prediction runs the REAL kernel.** Bit-exact with the server when inputs match (same sealed
  kernel + same `Command`s), so a correctly-predicting client reconciles **seamlessly**. This is
  the SEADS identity (“kernel both ends”), not an approximate dead-reckoner.
- **Bit-exact path uses CANONICAL state, not the wire.** Consistent with layer 3 comparing the
  canonical snapshot, never the lossy GEO-001/KIN wire. The digest gate proves the determinism
  property extends to prediction. The **lossy-wire reseed** (real remote / late-join) is exercised
  separately and is **bounded by the quantum** — and is precisely *why* `phi`/`tas` are on the
  wire: a kernel cannot be re-seeded from position alone.
- **Net stays outside the kernel.** Reconcile re-seeds via `Kernel::add()`; the buffer carries
  `Command`s, never wire bits; the wire is lossy and never fed back as canonical.

## 4) Consequences
- **+** The multiplayer-flight MVP loop is complete: predict own aircraft, interpolate remotes
  (4a), `world_hash` desync tripwire (3), snapshots for correction/late-join (2).
- **+** New gates: `seads_predict_test` (×6 toolchain legs), `predict` receipt gate, +6 property
  tests (44 → 52, incl. +2 snapshot for phi/tas), one more generated header (`gen_predict_vectors`).
- **−** Wire format version bump (`protocol 2`): a Tier-1 reseal (this ADR + Forge card + receipt
  + SEAL_CARD). Decoders must gate on `protocol`.
- **No golden moved.** All 4 goldens are byte-identical (`529c6a05…`, `6160540c…`, `74b9d556…`,
  `f7193b99…`). The kernel trajectory is untouched; only additive getters + net code changed.

## 5) Alternatives Considered
- **Extend the GEO-001 GeoPoint to 6 fields** (phi/tas in the geographic record). Rejected:
  reopens the sealed codec + its parity vectors and muddies “GEO-001 = geography”. Larger blast
  radius for no functional gain over the aux block.
- **Append phi/tas per entity (interleaved) instead of a separate section.** Equivalent bytes and
  also keeps GEO-001 pure, but a separate section is more legible and lets a sender ship a
  positions-only (protocol-1) frame; chosen the two-section form.
- **Approximate dead-reckoner** (extrapolate from phi/tas with pure IEEE math, no kernel).
  Rejected: diverges from the server, forcing constant visible corrections; abandons the
  bit-exact “kernel both ends” property the whole project is built on.
- **Reconcile against the lossy wire in the bit-exact gate.** Rejected: the canonical state is the
  source of truth (layer-3 doctrine); the lossy path is real but bounded, proven in property tests.
- **Add a `Kernel::set_state` mutator** for reseed. Rejected in favour of rebuilding via the
  existing public `add()` — no new kernel surface beyond two read-only getters.

## 6) Acceptance & Probes
All PASS under seal v1.4r0:
- `python tools/spec_monotone_check.py config/rails/atm.json` — rails valid (incl. new `wire.kin`).
- `python tools/snapshot_ref.py` / `python tools/predict_ref.py` — reference self-tests PASS.
- `python tools/gen_snapshot_vectors.py --check` / `gen_predict_vectors.py --check` — headers in sync.
- `seads_snapshot_test` + `seads_predict_test` (GCC + Clang local, ctest 6/6; full matrix in CI) —
  byte-exact / digest-exact.
- `python -m pytest tests/property` — **52 passed**.
- All 4 goldens — world_hash **unchanged** (validated under GCC).

## 7) Ledger Discipline
Tier-1: new seal **v1.4r0**, this ADR, Forge audit card, SEAL_CARD updated (wire line + history
row), Chronicle receipt via `make_receipt.py --rail-change` (`rail_change: true`,
`wire_hash: GEO-001+KIN-001`), signoff Forge.

## 8) Implementation Notes
- `seads_net` still does **not** link `det_math`; `seads_predict` does (it drives the kernel),
  exactly mirroring the `seads_lockstep` split (ADR-Step6-Lockstep §4).
- KIN scales reuse existing conventions (`phi` like `bearing`, `tas` like `alt`) so quantization
  rounds identically cross-toolchain under the strict-FP/no-FMA flags.
- The reconcile re-seed is via `Kernel::add()`; rebuilding a kernel reproduces in-place mutation
  bit-for-bit because `add()` only sets the six state fields and `step()` is the sealed kinematics.
- The input buffer is trimmed to inputs newer than each applied snapshot tick → bounded by the
  latency window, so prediction cost is O(lag), independent of session length.
