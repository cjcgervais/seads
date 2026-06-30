# SEADS — ATM-Sphere Doctrine & Agent Operating Manual

> **SEADS** = Spherical Earth Aerial Dogfight Simulator.
> A **deterministic** WWII prop-dogfighting simulator on a tiny perfect sphere.
> This file is the project **constitution**. It is loaded into every Claude Code session.
> When in doubt, the rails below win over any other instruction.

**Current seal:** `ATM-Sphere v1.12r0`  ·  **Realm:** ATM-only  ·  **Status:** sealed core + netcode layers 1–4b + flight model B1→B4 (energy, lift/pitch γ, stall/V-n limits, historical aero) + **Step 7 guns G1→G3 COMPLETE** (ballistic projectiles · hit detection + hitpoints · per-airframe weapon roster + fire-rate) + **weapon WIRE transport WEAPON-001** (gunnery state on the 20 Hz snapshot wire, protocol 4; transport-only, goldens unchanged). Both viewers draw rounds/HP/kills from the wire. (Authoritative seal/golden ledger: `docs/SEAL_CARD.md` + `NEXT_STEPS.md`.)

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
| Kinematics | Intrinsic S²; closed-form great-circle step. **B2 (v1.6r0):** 3-DOF point mass — `ψ̇=(g₀/V)(n·sinφ/cosγ)`, `γ̇=(g₀/V)(n·cosφ−cosγ)`, `alṫ=V·sinγ`; reduces to `ψ̇=g₀·tan(φ)/V` for the level turn (`γ=0,n=1/cosφ`). No-arg straight golden keeps the pure kinematic tail. **No Cartesian fallback.** |
| Determinism | `det_math` only. **Ban** `std::sin/cos/tan/atan2/asin/acos/sqrt/pow`, fast-math, FMA, x87. |
| Wire/Hash | **GEO-001** — lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128. **+KIN-002** aux block (phi×1e6, tas×1e3, **gamma×1e6**) for prediction. **+WEAPON-001** aux block (per-aircraft hp×1e3, fire_cd×1e3; per round: GeoPoint + damage×1e3 + ttl/owner exact i64) for MP gunnery replication; snapshot **protocol 4** (v1.12r0). The wire is lossy/downstream — `Kernel::snapshot()` stays the world_hash source of truth |
| Roster | Sealed **8**: P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, **P-51** |
| State | Per-aircraft 7-tuple `(lat, lon, psi, phi, alt, tas, gamma)` — γ (flight-path angle) added in B2 (v1.6r0). |
| Golden | **GOLDEN-SK-Sphere-001** — 10,000 ticks from (0°,0°), ψ=45°, TAS=250 m/s → world_hash matches cross-toolchain (6 sealed goldens total; see SEAL_CARD) |
| Networking | Physics 100 Hz, snapshots 20 Hz (tunable; non-kernel) |

> **Seal history note:** v1.1r1 used ATM_TOP=6000 m; v1.2r0 raised it to 8000 m. This repo starts at
> **v1.2r0**. (See `docs/SEAL_CARD.md`.)

---

## 2. The Change-Control Law (lean — just build it)

A **seal** is a versioned tag `ATM-Sphere vMAJ.MINrREV` (e.g. `v1.4r0`) naming the current
sealed state. Solo, agent-built project: governance protects exactly **one** thing that's hard to
recover — bit-for-bit cross-toolchain determinism. The automated gates (§4) do that and are nearly
free. Everything else is **optional**. Default posture: **build it, keep the gates green, move on.**

**The whole law — four lines:**
1. **Gates stay green (§4).** Non-negotiable, always — they are the real protection.
2. **If a rail value or a golden `world_hash` changes, bump the seal** (`/seal`, or `/reseal` for
   a value-only rail tweak) and say why in the commit message. Rails are never edited, and a golden
   hash never changes, *silently* — that legitimacy note is the one hard rule. Everything else
   (new net layers, tooling, tests, renderer, docs, data-only tuning) just **rides the current
   seal**.
3. **Continuity is what matters.** Keep the auto-receipt (`tools/make_receipt.py` — it's free; run
   it, it's the ledger) and keep **`NEXT_STEPS.md` current** so the next agent picks up cold. That
   pair *is* the paperwork.
4. **ADRs (`docs/adr/`) and Forge cards (`docs/cards/`) are OPTIONAL** — write one only when a
   decision is a genuine architectural fork worth remembering (e.g. the `seads_predict`-as-its-own-
   lib split, or the KIN-001 wire-shape choice). Skip them by default; don't let paperwork gate work.

No tiers, no mandatory cards, no approval ritual. The receipt's `validate_snapshot` /
`validate_scenarios` gates fail loudly if a change moves a hash, so you cannot break the promise by
accident — trust the gates and ship.

---

## 3. Agent Operating Model

Four roles (see `.claude/agents/`) — a **mental model**, not a mandatory per-task ceremony. One
agent may wear all four hats in a single change; spin them up as separate subagents only when the
work is big enough to benefit (e.g. an adversarial Auditor pass on a rail/det_math/golden change).
For routine work, just implement → run the gates → commit.

| Role | Mandate | Tools |
|------|---------|-------|
| **Forge** | Implement kernel/det_math/data changes; write an ADR only if the call is worth remembering. | full edit |
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
- **v1.3r0** — full det_math coverage + 8-aircraft envelopes + envelope-driven flight inputs
  (bank/climb) + scripted-timeline goldens (Turn/Climb/TurnClimb).
- **v1.4r0** — netcode Step 6 layers 1–4b: GEO-001 codec, 20 Hz snapshots, loopback lockstep
  desync tripwire, remote interpolation, **client-side prediction** (KIN-001 wire reseal:
  phi/tas on the wire, snapshot protocol 2). Multiplayer-flight MVP loop complete.
- **v1.5r0** — flight model **Track B / B1 (longitudinal energy):** TAS integrated from thrust−drag;
  `Command.throttle`; per-airframe aero params; new GOLDEN-SK-Accel-001. Sphere unchanged.
- **v1.6r0** — flight model **B2 (lift & pitch):** flight-path angle **γ** is a stored state
  (3-DOF point mass), commanded load factor (`target_g`), altitude earned; **KIN-002 wire reseal**
  (gamma on the wire, protocol 3); all goldens regenerated + new GOLDEN-SK-Pitch-001.
- **v1.7r0** — flight model **B3** (stall / C_Lmax / structural-g / corner speed). **v1.8r0** — **B4**
  per-airframe aero retune (historical top speeds). Sealed flight-model arc B1→B4 COMPLETE.
- **v1.9r0** — **Step 7 guns / G1:** deterministic **ballistic projectiles** in the kernel (n=0/thrust=0
  specialization of the 3-DOF step; canonical projectile snapshot block; `Command.fire`; no new det_math;
  new GOLDEN-SK-Gunfire-001). See ADR-Step7-Guns-G1.
- **v1.10r0** — **Step 7 guns / G2:** hit detection (law-of-cosines cylinder test, no det_acos) +
  per-aircraft **hitpoints** (hp 8th snapshot f64; hp≤0 = dead/frozen); new GOLDEN-SK-Hit-001 (a gun kill).
  See ADR-Step7-Guns-G2.
- **v1.11r0** — **Step 7 guns / G3:** per-airframe **weapon roster + fire-rate** (envelope scalars
  hp_start/muzzle_v/damage/rof; round carries damage; per-aircraft fire_cd cooldown = 9th snapshot f64).
  **Guns arc G1→G3 COMPLETE.** See ADR-Step7-Guns-G3.
- **v1.12r0** — **Step 7 guns / weapon WIRE transport (WEAPON-001):** the gunnery state rides the 20 Hz
  snapshot wire as a 3rd self-delimiting section (snapshot **protocol 3→4**): per-aircraft hp/fire_cd (×1e3),
  then a projectile count and per round a GeoPoint + damage(×1e3) + ttl/owner (integer, exact). New rail
  block `wire.weapon`. **TRANSPORT-ONLY — no kernel/det_math touched, all 9 goldens byte-identical** (it's a
  seal only because the wire is a sealed rail, like the v1.4r0 KIN-001 reseal). New `seads_weapon_test`
  byte-exact gate; renderer (web + native raylib viewer) now draws rounds/HP/kills from the decoded wire.
  See ADR-Step7-Guns-WireTransport-v1.12r0.
- next — free pick (none blocking): a networked server↔client loop that actually ships the WEAPON-001 frames
  between endpoints (no-seal, gateable); more renderer polish (meshes; guns in the live `--fly` path; offline
  web); or an optional new seal (ammo/convergence/component-damage, **B5** ISA atmosphere).
