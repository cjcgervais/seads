# ADR-Step7-Guns-Attribution-v1.16r0 — Attacker attribution (last_hit_by / "who fired the killing round")

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor/Guardian
**Seal:** ATM-Sphere v1.16r0 (proposed; bumps from v1.15r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

The guns arc (G1→G4 + convergence) models a full deterministic dogfight — ballistic rounds, hit/damage,
per-airframe roster/fire-rate, finite ammunition, boresight harmonization. The netcode built on top can
even reliably replicate the *moments* of combat: **layer 6** (`event.{h,cpp}`) DERIVES HIT and KILL
events by observing the authoritative kernel's `hp` deltas and ships them over the lossy wire with
redundancy, so a client reconstructs the exact hit/kill sequence bit-for-bit.

But nothing in the model records **WHO** dealt a hit. The kernel knows it at hit time — the striking
round carries `owner` (used since G2 to exclude the firer from its own hit test) — and then **throws it
away**. So the layer-6 kill-feed can say "A6M2 died at tick 54" but not "**P-47D** downed A6M2". Every
prior handoff explicitly deferred this as *"attacker attribution — needs a kernel-side event hook, its
own ADR."* This ADR is that hook.

## 2) Decision

**Add a per-aircraft canonical state `last_hit_by` — the index of the aircraft whose round most recently
damaged this one, or `NO_ATTACKER = -1` if it has never been hit — set at hit time from the striking
round's `owner`.**

In the projectile-advance hit branch (`_advance_projectiles` / `advance_projectiles_`), right where the
carried damage is already applied:

```
ac.hp = max(0, ac.hp - round.damage)     # (existing G3)
ac.last_hit_by = float(round.owner)      # v1.16r0: attribute the hit to the firing aircraft
```

It **persists through death** (never cleared), so at `hp <= 0` it names the **killer** — the owner of the
round that landed the killing blow. Because the layer-6 event derivation already lumps a tick's `hp`
delta into one event and reads kernel state per-tick, it can now read `last_hit_by` at the delta tick to
attribute that event (the v1.17r0 follow-up).

**Determinism:** `last_hit_by` is a **pure integer-valued state** (an aircraft index stored as an f64,
like `fire_cd`/`ammo`), assigned by copy — **NO new det_math**, preserving the guns arc's
zero-new-transcendental streak. The sentinel is the exact hex-float `-0x1.0p0` (`-1.0`), shared
bit-for-bit between `tools/ref_kernel.py` (`NO_ATTACKER`) and `src/kernel/kernel.cpp` (`NO_ATTACKER`).

**Canonical, hashed state:** `last_hit_by` is the **11th per-aircraft snapshot f64** (appended after
`ammo`), so it is hashed into the `world_hash` — the same additive-field mechanism by which `gamma` (B2),
`hp` (G2), `fire_cd` (G3), and `ammo` (G4) each grew the snapshot.

**Why this is a seal.** A new canonical field grows every aircraft's snapshot bytes, so the `world_hash`
of every golden moves → a golden hash changes → seal (Change-Control Law §2). Rails version 250→260.

**Design choice — canonical state vs. an ephemeral event log.** A kernel-side *hit-event ring* (per-round
attribution consumed each tick) was considered. It was rejected for this seal: (a) it would either be
un-hashed (invisible to the golden promise) or force a variable-length canonical block; (b) `last_hit_by`
mirrors the proven `hp`/`fire_cd`/`ammo` pattern exactly (fixed-width, additive, hashable, wire-ready);
(c) it gives the renderer a persistent "last damaged by" for free. The trade-off — one attacker per
target per tick rather than per-round — matches the existing per-tick event-derivation granularity, so
nothing downstream loses resolution. Per-round / multi-attacker granularity remains a future option.

## 3) Verification (gates)

- **Additive-only, proven byte-for-byte.** All **10 goldens move** (the snapshot grew one f64/aircraft),
  but the growth is *purely* the appended field: stripping the 11th per-aircraft f64 from each v1.16r0
  golden snapshot reproduces its **v1.15r0 hash byte-for-byte** (10/10 — a dry strip-diff). So attribution
  perturbs **no** trajectory, hp, fire_cd, ammo, gamma, or projectile. New hashes:
  - Sphere `f2db95bd…c185f29c` · Turn `9e85dd3a…` · Climb `4ab3d37d…` · TurnClimb `9ffd376f…`
  - Accel `52742a11…` · Pitch `2c1e360b…` · Stall `23b7507c…`
  - Gunfire `0e648539…` · **Hit `612e5407…`** · Winchester `f351ee04…`
- **Meaningful new behavior, tied to a golden.** GOLDEN-SK-Hit-001 now records the kill: the A6M2 ends at
  `hp 0` **and** `last_hit_by = 0` (the P-47D); the P-47D, never hit, stays `-1`. This is asserted both by
  the golden hash and directly by `test_golden_hit_scenario_records_the_kill`.
- **Cross-toolchain.** C++ `seads_golden` + `seads_scenario` reproduce all 10 new hashes under **GCC and
  Clang**, bit-identical to the Python reference (CI adds MSVC + AArch64).
- **Property tests** `tests/property/test_attribution.py` (**+7 ⇒ 126**): default is `NO_ATTACKER`; a hit
  attributes to the firer; no-hit (altitude-gated) keeps `-1`; attribution persists through death and
  names the killer; it is the 11th snapshot f64 (decode check); the Hit scenario records the kill; and it
  is deterministic across re-runs.
- **Blast radius is exactly the world_hash.** Attribution is **off-wire** this seal, so only the two
  generated vectors that hash `kernel.snapshot()` move — `lockstep_vectors.h` (desync tripwire) and
  `predict_vectors.h` (client-side-prediction canonical digest), both regenerated. The wire-based vectors
  (`snapshot`/`weapon`/`session`/`event`/`interp`/`geo001`) are **byte-identical** (attribution not on the
  WEAPON-001 wire yet — exactly as `ammo` was off-wire at G4). **No new golden, no new ctest target ⇒
  `guardian.yml` unchanged** (same 10 golden IDs).
- **Full suite:** 15/15 receipt gates PASS; ctest **10/10** under GCC and Clang; all generated headers in
  sync; det_math oracle / rails / probes green.

## 4) Consequences / boundaries

- **Last damager, not full assist list.** `last_hit_by` records only the most-recent damager. A kill-assist
  ledger (all contributors) or per-round attribution would need the ephemeral-log design above — deferred,
  not precluded.
- **Off-wire this seal.** A remote/late-join client cannot yet SEE the attribution. The **v1.17r0 follow-up**
  puts `last_hit_by` on the WEAPON-001 snapshot section (protocol 5→6, a transport-only wire reseal like
  `ammo` v1.14r0) and adds `Event.attacker` to the layer-6 channel → an **attributed kill-feed**. That
  follow-up moves the wire/session/event digests but keeps all 10 goldens byte-identical.
- **Persistence semantics.** Attribution is sticky (survives death) by design, so the corpse and the
  kill-feed agree on the killer. A "reset on respawn" rule is moot (SEADS has no respawn).
- **Next (optional, none blocking):** the v1.17r0 wire+event follow-up above; cross-PROCESS sockets over
  the layer-5/6 frames; component/region damage; **B5** ISA atmosphere; renderer meshes / guns in the live
  `--fly` path.
