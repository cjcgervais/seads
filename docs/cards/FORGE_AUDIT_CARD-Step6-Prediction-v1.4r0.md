# Forge Audit Card — Step 6 layer 4b: client-side prediction + KIN wire reseal (ATM-Sphere v1.4r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere **v1.4r0** (Tier-1 reseal — new wire fields/scales `wire.kin`; **no golden moved**)
**Realm:** ATM-only
**Scope:** (a) wire reseal — auxiliary **KIN-001** block carries `phi`/`tas`, snapshot
`protocol 1 → 2` as a second self-delimiting section (GEO-001 codec + vectors **untouched**);
(b) new client-side prediction harness (own `seads_predict` lib) + cross-impl parity gate.
Kernel math, `det_math`, and the canonical snapshot byte layout are **untouched** (only two
additive read-only getters `Kernel::phi()/tas()`).

## Gates & Results
- [x] spec_monotone_check.py — PASS (rails header v1.4r0, new `wire.kin` valid)
- [x] det_math_oracle.py — PASS (unchanged)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] geo001_ref.py self-test — PASS (GEO-001 codec byte-unchanged)
- [x] **snapshot_ref.py self-test — PASS (KIN section round-trips; protocol-1 back-compat)**
- [x] lockstep_ref.py + interp_ref.py self-tests — PASS (layers 3–4a still green)
- [x] **predict_ref.py self-test — PASS (seamless 300t; heal at tick 15; no-reconcile control breaks; lossy reseed bounded 8.7e-10)**
- [x] gen_snapshot_vectors.py + gen_predict_vectors.py: regenerate → --check — **byte-reproducible**
- [x] gen_*.py --check (all 9 headers incl. predict_vectors.h) — PASS
- [x] **seads_predict_test (GCC) — PASS: seamless, digest == reference, heals at 15, control trips**
- [x] **seads_predict_test (Clang) — PASS: seamless, digest == reference, heals at 15, control trips**
- [x] seads_snapshot_test (GCC + Clang) — PASS (KIN byte-exact within quantum)
- [x] ctest build-gcc + build-clang — **6/6** each (detmath, geo001, snapshot, lockstep, interp, predict)
- [x] validate_snapshot GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_scenarios GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes (GCC)
- [x] pytest tests/property — **52 passed** (44 → 52; +6 predict, +2 snapshot)
- [x] make_receipt.py --rail-change — overall **PASS** (new `predict` gate green; `rail_change: true`)
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## What this layer is
Predict the OWN aircraft forward from local input each tick (running the **real sealed kernel**),
buffer the local `(tick, Command)` inputs, and on an authoritative snapshot **reconcile**: snap to
the authoritative state at the snapshot's tick, drop inputs ≤ that tick, and **replay** the rest
forward. Correct prediction reconciles **seamlessly** (bit-exact); a desynced client is **healed**
exactly at the next snapshot. Completes the multiplayer-flight MVP loop (predict + interpolate +
desync tripwire + snapshot correction/late-join).

## Determinism & Invariants
- **Bit-exact path reconciles against the CANONICAL state** (raw f64 6-tuple), NOT the lossy
  GEO-001/KIN wire — same posture as the layer-3 tripwire. Proven exhaustively by a SHA-256 digest
  over every per-tick predicted hash == the Python reference (+ readable checkpoints), with a
  byte-reproducible hex-float scenario ⇒ CI `--check` green.
- **Reconcile is demonstrably load-bearing:** a 2⁻²⁰ m initial-alt desync diverges at tick 1 and
  heals at `HEAL_TICK = 15` **with** reconcile; **without** it the drift never recovers (asserted).
- **Lossy-wire reseed is bounded, not exact** (property test) — the realistic remote/late-join
  path; this is *why* `phi`/`tas` are on the wire (a kernel needs full state to step).
- **Net stays outside the kernel:** reconcile re-seeds via the public `Kernel::add()` + `step()`
  (no kernel-internals access); the buffer carries `Command`s, never wire bits; the wire is never
  fed back as canonical. Kernel kinematics + snapshot byte layout unchanged ⇒ goldens unmoved.

## Wire reseal (Tier-1)
GEO-001 stays **geography-only**: its codec and parity vectors are byte-unchanged. `phi`/`tas` go
in a new **KIN-001** section (`phi ×1e6` like `bearing`, `tas ×1e3` like `alt`), appended after the
GEO section and gated on `protocol ≥ 2`. The KIN id is checked against the GEO id (section-alignment
guard). Chosen over extending the GeoPoint (smaller blast radius on the sealed codec). See ADR §2a/§5.

## Library boundary (consistent with lockstep)
`seads_predict` is its **own** lib (links `seads_kernel`+`seads_replay`) because it drives the
kernel — not folded into the pure wire lib `seads_net`. Mirrors the `seads_lockstep` split.

## Affected Files
- config/rails/atm.json (wire.kin block; version 140; seal v1.4r0)
- tools/snapshot_ref.py, src/net/snapshot.{h,cpp} (KIN section, protocol 2, phi/tas)
- tools/gen_snapshot_vectors.py, src/net/snapshot_vectors.h (regenerated w/ phi/tas)
- src/net/snapshot_test_main.cpp (KIN round-trip assertions)
- tools/predict_ref.py (new — canonical reference + self-test + controls)
- src/net/predict.{h,cpp} (new — `seads_predict` lib)
- tools/gen_predict_vectors.py, src/net/predict_vectors.h (new — generated parity vectors)
- src/net/predict_test_main.cpp (new — `seads_predict_test`)
- tests/property/test_predict.py (new — 6 tests); tests/property/test_snapshot.py (+phi/tas, +2)
- src/kernel/kernel.h (additive read-only getters phi()/tas() — no math/layout change)
- CMakeLists.txt (seads_predict lib + seads_predict_test exe + ctest `predict_equal`)
- .github/workflows/guardian.yml (gen --check + reference self-test + predict test per leg)
- tools/make_receipt.py (`predict` gate; `--rail-change`; wire_hash from rails)
- docs/adr/ADR-Step6-Prediction-v1.4r0.md; docs/SEAL_CARD.md; docs/receipts/receipt-…

## Decision
**APPROVE**

**Sign-off:** Forge · net/tooling · 2026-06-28
