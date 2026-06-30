# ADR-Step7-Guns-G1-Projectiles-v1.9r0 — Guns Phase G1: deterministic ballistic projectiles in the kernel

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.9r0 (proposed; bumps from v1.8r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

The flight-model arc B1→B4 is complete: the kernel now flies a real 3-DOF energy/aero point mass and
the 8 airframes read true. The next milestone toward an actual **dogfight** sim is **weapons**. Per
`NEXT_STEPS.md` §7 this is a major new seal arc flagged for an **owner scope decision**, which the
owner made: build **guns Step 7**, with projectiles modeled as **deterministic ballistics in the
kernel** (not hit-scan). This ADR records G1 — the ballistic substrate — staged like B1→B4:

- **G1 (this ADR, v1.9r0):** rounds exist as deterministic kernel state, fly ballistically on the
  sphere, are hashed into the `world_hash`, and despawn on time-to-live / ground contact.
- **G2 (next, ~v1.10r0):** hit detection (round vs. aircraft on the sphere) + a damage model.
- **G3 (data, reseal):** per-airframe weapon roster (armament, muzzle velocity, fire-rate, convergence).

## 2) Decision

A fired round is a **point mass on the same sphere**, advanced by the **same closed-form
great-circle integrator** as an aircraft — specifically the **n=0 (no lift) / thrust=0**
specialization of the sealed 3-DOF step. Per tick, for each live round:

```
V    = tas
Vdot = -PROJ_DRAG_K*V*V - g0*sin(gamma)     # lumped quadratic drag + gravity along the path
Vnew = max(V + Vdot*dt, V_MIN)
gdot = (g0/Vnew) * (-cos(gamma))            # n=0  -> gamma bends down under gravity
gamma += gdot*dt
# psi unchanged (no turn force; the great-circle step carries the geodesic bearing)
lat,lon = great_circle_step(lat, lon, psi, Vnew*cos(gamma_new)*dt, R)
alt += Vnew*sin(gamma_new)*dt               # clamp [0, ATM_TOP]; alt<=0 => ground hit
ttl -= 1                                     # despawn when ttl reaches 0 OR on ground hit
```

This is **bit-for-bit the structure of `Kernel::step(cmd,env)`** with the lift/thrust/bank terms
removed, so it reuses `det_sin`/`det_cos`/`great_circle_step` and **adds NO new det_math primitive**
— the single biggest reason to model rounds this way (no MPFR-oracle expansion, no FMA/AArch64
divergence risk). A round inherits the firer's **post-step** muzzle state (`lat,lon,psi,alt,gamma`),
speed `tas + MUZZLE_V`, and an `owner` index; it does **not** advance on its spawn tick (it waits at
the muzzle). `Command` grows a `bool fire` trigger; one round is spawned per fired tick (a
deterministic placeholder rate — real per-weapon fire-rate is G3).

**State & serialization.** Projectiles are a second struct-of-arrays in the kernel
(`p_lat_,p_lon_,p_psi_,p_alt_,p_tas_,p_gamma_` + `p_ttl_,p_owner_`), iterated and compacted in array
order (deterministic; no pointer/address dependence). The **canonical snapshot** gains a trailing
**projectile block** after the aircraft block: `u32 n_projectiles, u32 pad`, then per round
`6×f64 [lat,lon,psi,alt,tas,gamma] + u32 ttl + u32 owner`. The block is **always present**
(n=0 for gun-less scenarios).

**Constants** (`MUZZLE_V=850 m/s`, `PROJ_DRAG_K=2.0e-4`, `PROJ_TTL_TICKS=250`) are **global** for G1
and live in `kernel.cpp`/`ref_kernel.py` as shared **hex-float** literals — exactly like `RHO0`/
`V_MIN`, they are kernel constants, **not** rails (per-weapon values move into the envelope in G3).

## 3) Why this is a seal (golden hashes move)

Appending the projectile block grows **every** snapshot's bytes, so all 7 prior goldens' hashes move
even though their **trajectories are byte-identical** (n=0, no rounds). This is the same honest
format-growth that B2 paid when `gamma` joined the state tuple — a fixed, self-describing canonical
format beats a conditional/ambiguous one. Per CLAUDE.md §2 a moved golden ⇒ a seal. The 6 scenario
goldens + **Sphere** (which moves this time — unlike B3/B4 — because the block is appended to the
no-arg path's snapshot too) are regenerated, plus the new **GOLDEN-SK-Gunfire-001** (2 gunners, 35
ballistic rounds airborne and hashed at tick 300).

## 4) Scope boundaries (explicit non-goals for G1)

- **No hit detection / damage** — rounds fly and despawn; they cannot yet harm an aircraft (G2).
- **No wire transport** — projectiles are NOT on the GEO-001/KIN wire (deferred exactly as phi/tas
  were pre-layer-4b). Multiplayer round replication is a later netcode layer. The renderer does not
  yet draw rounds (a later no-seal client task).
- **No per-airframe armament** — one generic gun, global constants (G3).
- **Heading is not geodesically transported** (`psi` fixed) — identical to the aircraft model's own
  approximation, kept for consistency; full bearing transport is out of scope.
- **`cos(gamma)→0`** never arises (no `/cos(gamma)` in the ballistic step — that term was the n≠0
  turn law), so the vertical singularity that constrains aircraft scenarios does not apply to rounds.

## 5) Determinism & verification

- **No new det_math.** Ballistic step uses `det_sin`/`det_cos` + `great_circle_step` + `+ - * /`,
  all already proven ≤ target ULP vs the MPFR oracle. No new banned symbol, no FMA.
- **Mirror-first:** `tools/ref_kernel.py` defines the round physics and the snapshot bytes; `kernel.cpp`
  bit-matches the op order. All 8 goldens reproduce **C++ ≡ Python under GCC + Clang** locally;
  guardian CI extends this to MSVC + GCC/Clang × x64/AArch64 (Gunfire added to all golden lists).
- **Gates:** 12/12 receipt PASS at v1.9r0; ctest 7/7 under GCC + Clang; **81 property tests**
  (+9 `tests/property/test_projectile.py`: drag bleeds speed, gravity arcs the round down, downrange
  monotonic, exact ttl despawn, ground-hit despawn, faster⇒more range/tick, determinism, the
  end-to-end `Command.fire`→post-step-muzzle spawn, and "no-fire/legacy-3-tuple spawns nothing").
- The lockstep/predict generated-vector digests moved with the canonical snapshot and were
  regenerated (byte-reproducible; `--check` green).

## 6) Consequences

- The kernel now has a second entity class; G2 (hit/damage) and G3 (roster) build on this substrate.
  `owner` is already carried (unused in G1) so G2 needs no projectile-format reseal for attribution.
- Backward-compatible inputs: `Command.fire` defaults false and the reference reads `commands[i][3]`
  only if present, so every prior 3-tuple caller (lockstep/predict refs, `test_energy`) is unchanged.
- Wire transport of rounds and a renderer that draws them are the natural next no-seal follow-ups;
  G2 is the next seal.

## 7) Alternatives considered

- **Hit-scan (instant ray)** — rejected by the owner: no travel time/drop/convergence, less authentic
  for WWII gunnery, and against the "deterministic projectile sim" spirit of the project.
- **Conditional projectile block (omit when n=0)** to keep Sphere byte-identical — rejected: it makes
  the canonical format ambiguous/non-self-describing for a purely cosmetic "anchor unchanged" win.
  B2 set the precedent that an honest fixed format that moves all goldens is correct.
- **Per-airframe weapon constants now** — deferred to G3 to keep G1 a tight, single-concern seal.
