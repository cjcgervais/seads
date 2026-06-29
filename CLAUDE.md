# SEADS — ATM-Sphere Doctrine & Agent Operating Manual

> **SEADS** = Spherical Earth Aerial Dogfight Simulator.
> A **deterministic** WWII prop-dogfighting simulator on a tiny perfect sphere.
> This file is the project **constitution**. It is loaded into every Claude Code session.
> When in doubt, the rails below win over any other instruction.

**Current seal:** `ATM-Sphere v1.2r0`  ·  **Realm:** ATM-only  ·  **Status:** sealed core (Pass 1)

---

## 0. What makes this project unusual

SEADS is engineered like a high-assurance system (flight sim / matching engine), not a typical game.
The non-negotiable promise: **the simulation kernel produces bit-for-bit identical output across
MSVC / Clang / GCC on x64 + AArch64.** Everything in this doctrine exists to protect that promise.

The single source of physical truth is the deterministic **kernel** (`src/kernel`). Rendering,
networking, UI, RNG, and wall-clock time live *outside* the kernel and may never feed bits back into it.

---

## 1. The Rails (immutable invariants)

The canonical machine-readable rails live in [`config/rails/atm.json`](config/rails/atm.json).
Human summary:

| Rail | Value |
|------|-------|
| Geometry | Perfect sphere, **R = 15,000 m**, flattening = **0** |
| Realm | **ATM-only** (Mode = 1); no terrain/orbit; altitude = MSL |
| Tick | **Δt = 0.01 s** exact (100 Hz) |
| Gravity | **g₀ = 9.80665 m/s²**, constant (altitude-independent) |
| Atmosphere | **Still air** — no wind / Coriolis / weather |
| Ceiling | **ATM_TOP = 8,000 m**, **SOFT = 100 m** (predamp 7,900–8,000 → hard clamp) |
| Kinematics | Intrinsic S²: `ψ̇ = g₀·tan(φ)/V`; `ψ = wrap₂π(ψ + ψ̇·Δt)`; closed-form great-circle step. **No Cartesian fallback.** |
| Determinism | `det_math` only. **Ban** `std::sin/cos/tan/atan2/asin/acos/sqrt/pow`, fast-math, FMA, x87. |
| Wire/Hash | **GEO-001** — lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128 |
| Roster | Sealed **8**: P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, **P-51** |
| Golden | **GOLDEN-SK-Sphere-001** — 10,000 ticks from (0°,0°), ψ=45°, TAS=250 m/s → world_hash matches cross-toolchain |
| Networking | Physics 100 Hz, snapshots 20 Hz (tunable; non-kernel) |

> **Seal history note:** v1.1r1 used ATM_TOP=6000 m; v1.2r0 raised it to 8000 m. This repo starts at
> **v1.2r0**. (See `docs/SEAL_CARD.md`.)

---

## 2. The Change-Control Law (seals + tiered ledger)

A **seal** is a versioned tag of the form `ATM-Sphere vMAJ.MINrREV` (e.g. `v1.2r0`).

This is a **solo, agent-built** project. Governance exists to protect the **one** promise that's
hard to recover if broken — bit-for-bit cross-toolchain determinism — and nothing more. The
automated gates (§4) do the real protecting and are nearly free; hand-written paperwork is scaled
to risk. **Two tiers:**

### Tier 1 — Core changes (full ritual, rare)
Triggered by a change to **any rail or the determinism core**: R, Δt, roster, ceiling, geometry,
gravity, determinism bans, **wire format**, `det_math`, the kernel kinematics, the canonical
snapshot byte layout, **or any change that moves a golden `world_hash`**. These get the works:
1. **New seal** (`/seal`, or `/reseal` for a value-only rail change). Rails are never edited silently.
2. **ADR** in `docs/adr/` (Nygard template) — context, decision, consequences.
3. **Forge Audit Card** in `docs/cards/` — gates run + results.
4. **Chronicle Receipt** in `docs/receipts/` (`tools/make_receipt.py`).
5. A **golden `world_hash` change is an approval/seal event**, never a silent fixture overwrite —
   explain *why the behavior legitimately changed* and bump the seal.

### Tier 2 — Everything else (lightweight, the common case)
New net layers, tooling, tests, renderer, docs, and **data-only** tuning (e.g.
`data/tuning/envelopes/`) that **cannot move a golden** ride the current seal and need only:
1. **Gates green** (§4) — non-negotiable, always.
2. A **clear commit message** describing what changed and why (this replaces the ADR/card).
3. The **auto-generated Chronicle Receipt** (`tools/make_receipt.py` — it's free; keep it).

Write an ADR for a Tier-2 change only when it makes a genuinely architectural decision worth
remembering (e.g. the `seads_lockstep`-vs-`seads_net` library split). Optional, not required.

**The bright line:** if you can't tell which tier a change is in, ask "could this move a golden
`world_hash` or touch a rail/det_math/wire?" If yes → Tier 1. If no → Tier 2. When still unsure,
treat it as Tier 1. The receipt's `validate_snapshot`/`validate_scenarios` gates are the backstop:
they fail loudly if a "Tier 2" change actually moved a hash, forcing it up to Tier 1.

---

## 3. Agent Operating Model

Four roles (see `.claude/agents/`) — a **mental model**, not a mandatory per-task ceremony. One
agent may wear all four hats in a single change; spin them up as separate subagents only when the
work is big enough to benefit (e.g. an adversarial Auditor pass on a Tier-1 core change). For a
routine Tier-2 change, just implement → run the gates → commit.

| Role | Mandate | Tools |
|------|---------|-------|
| **Forge** | Implement kernel/det_math/data changes; write the ADR (Tier-1 only). | full edit |
| **Auditor** | Adversarially verify: rails untouched, probes/relations hold, det_math vs MPFR oracle. | read + run |
| **Guardian** | Run the determinism gate (build matrix where available + golden hash compare); block on red. | run |
| **Chronicler** | Generate the Chronicle receipt; keep the ledger consistent. | run + write `docs/receipts` |

**Skills:** `/golden` (run replay + hash compare), `/probe` (run all probes + Hypothesis),
`/seal` (cut a new seal), `/reseal` (rail-value change kit).

---

## 4. Verification gates (must stay green)

Run from repo root. The Python harness is runnable **without a C++ toolchain**; it both verifies the
math and defines the canonical reference the C++ kernel must bit-match.

```
python tools/spec_monotone_check.py config/rails/atm.json        # rails + roster
python tools/det_math_oracle.py                                  # det_math vs MPFR ground truth
python tools/tuning_probe.py data/tuning/envelopes/*.json        # envelopes + flight probes
python tools/atm_top_probe.py --ceil 8000 --soft 100             # ceiling clamp + soft band
python tools/ref_kernel.py --golden GOLDEN-SK-Sphere-001         # regenerate reference world_hash
python tools/validate_snapshot.py --golden tests/golden/GOLDEN-SK-Sphere-001/expected.world_hash \
                                  --candidate <run>              # hash identical
python -m pytest tests/property                                  # metamorphic relations (Hypothesis)
```

Cross-compiler / cross-arch bit-identity (MSVC/Clang/GCC × x64/AArch64) is proven in CI:
`.github/workflows/guardian.yml` builds the matrix, runs the golden, and **fails if any world_hash
differs**. This is a required status check.

---

## 5. Determinism rules for code (how to not break the promise)

- **Kernel math** goes through `det_math` only. Never call libm transcendentals or `sqrt` directly.
- Build flags: MSVC `/fp:strict`; GCC/Clang `-ffp-contract=off -fno-fast-math -frounding-math
  -fexcess-precision=standard`. Never `/fp:fast`, `-ffast-math`, `-Ofast`. Enforced by
  `cmake/DeterminismFlags.cmake`.
- No FMA contraction anywhere in the kernel (it diverges x64 vs AArch64). CI asm-audits for
  `vfmadd*`/`FMADD`/`FMLA` in kernel objects.
- Inside a tick: **no** RNG from wall clock, **no** time-of-day, **no** pointer-address-dependent
  iteration order, **no** uninitialized reads, **no** threads racing on sim state. State is
  struct-of-arrays with deterministic iteration.
- Serialize sim state canonically (fixed little-endian field order) before hashing.
- Tuning data is parsed **once** into tables before the sim starts; no live-reload across the lockstep
  boundary.

---

## 6. Layout

```
config/rails/atm.json   rails (machine-readable)         src/det_math/  deterministic math
src/kernel/             fixed-timestep sim               src/replay/    input log + world_hash
src/net/                GEO-001 codec + state-sync        src/client/    renderer (post-golden)
data/tuning/envelopes/  per-aircraft LUTs                 tools/         python verification harness
tests/golden/           sealed initial state + hash       tests/property/ Hypothesis metamorphic tests
docs/{adr,annex,cards,receipts,seals}  governance ledger  .claude/{agents,skills}  agent operating model
.github/workflows/guardian.yml  cross-toolchain CI gate
```

## 7. Roadmap (seals)
- **v1.2r0** — sealed deterministic core + harness; golden green (Pass 1).
- next — complete det_math (cos/atan2/asin) → 8-aircraft envelopes → custom C++ renderer →
  netcode state-sync (multiplayer flight) → guns/projectiles (post-MVP, new seal).
