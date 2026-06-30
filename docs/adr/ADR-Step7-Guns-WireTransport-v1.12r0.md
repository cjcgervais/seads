# ADR-Step7-Guns-WireTransport-v1.12r0 — Weapon WIRE transport (WEAPON-001 snapshot section)

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.12r0 (proposed; bumps from v1.11r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

The guns arc (G1→G3, seals v1.9r0–v1.11r0) made the kernel model a full deterministic
dogfight-gunnery loop: ballistic rounds, hit detection, per-aircraft hitpoints, and a per-airframe
weapon roster with fire-rate. All of that is **canonical kernel state** (hashed into the
`world_hash`). But the **20 Hz snapshot wire** — the byte stream a server transmits and a remote /
late-join client decodes — only carried geography (GEO-001) and the flight kinematics needed to
re-seed a kernel for prediction (KIN-002: phi/tas/gamma). The gunnery state (hitpoints, fire-rate
cooldown, and the live rounds) was **NOT on the wire**; G1→G3 explicitly deferred it, exactly as
phi/tas were deferred before the layer-4b reseal.

The visible symptom: the renderer's `seads_record --gundemo` drew HP bars, tracer rounds, and kills
by reading them **directly out of the local kernel** (an out-of-band side channel), because the wire
couldn't carry them. That only works in a single-process recording; a real multiplayer client — which
sees *only* the wire — could not replicate the dogfight. This ADR closes that gap.

## 2) Decision

**Add a third snapshot section, WEAPON-001, gated on snapshot protocol ≥ 4 (bump 3 → 4).** The
snapshot already framed two self-delimiting sections (GEO, then — protocol ≥ 2 — KIN). WEAPON-001 is
a third, appended after KIN, present iff `protocol >= 4`:

```
header (protocol, server_tick, n)
  GEO    section: n × (id, GeoPoint[lat,lon,bearing,alt])               (GEO-001, geography-only)
  KIN    section: n × (id, phi_q, tas_q [, gamma_q])   [protocol >= 2]  (KIN-002 aux block)
  WEAPON section:                                       [protocol >= 4]  (WEAPON-001 aux block)
      n × (id, hp_q, fire_cd_q)                                          per-aircraft gunnery state
      m  (projectile count)
      m × (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl, owner)     live ballistic rounds
```

**Scales (new rail block `wire.weapon`, all ZigZag+LEB128 like GEO-001):**
- `hp ×1e3`, `fire_cd ×1e3`, `damage ×1e3` — small magnitudes (O(1)..O(150)); the kernel's values are
  integer-valued so this is exact in practice but transmits any double faithfully. `hp` may be
  negative on a dead frame; ZigZag handles the sign.
- `ttl`, `owner` — integer counters (kernel `u32`: the round's ticks-remaining and the firing aircraft
  index). Carried as **exact i64** (NOT quantized) — a kill must replicate with the right attribution
  and a round must fade at the right tick.
- The projectile's `bearing` reuses the GEO-001 GeoPoint slot for the round's heading `psi` (free, and
  useful for oriented tracer rendering).

**Why a separate section, not extending GeoPoint / KIN.** Keeping GEO-001 geography-only preserves the
sealed codec and its parity vectors byte-for-byte; keeping the KIN-002 framing untouched means a
protocol-3 frame is byte-identical to before. This is the same shape decision made for KIN-001 (a 2nd
section rather than a wider GeoPoint) at v1.4r0.

**This is a TRANSPORT change, not a model change.** `hp`/`fire_cd`/`damage` already exist as canonical
state (the world_hash source of truth is `Kernel::snapshot()`, raw LE-f64). WEAPON-001 is a separate,
**lossy, downstream** transport, never fed back as canonical (CLAUDE.md determinism rules). Therefore
**no kernel / det_math / golden change** — the only reason this is a seal at all is that the wire
format is a sealed rail (§1 of the constitution): touching the protocol/scales requires a reseal.

**Downstream rider (bundled, would not itself need a seal).** `seads_record` now builds the wire
EntityStates with hp/fire_cd and populates the projectile list (`netsnap::proj_from_kernel`), then
sources the renderer's HP bars + tracer rounds + kills from the **decoded** wire — retiring the
kernel side-channel (`VizFrame`/`capture_viz` deleted; net −code). `--gundemo` now proves the weapon
wire end to end: the trajectory.js the web viewer reads is `protocol: 4` and shows the A6M2's HP
falling 70→0 and rounds tagged `owner:0` straight off the decoded bytes.

## 3) Verification (gates)

- **Mirror-first.** `tools/snapshot_ref.py` (the reference that *defines* the wire bytes) extended
  first — `ProjectileState`, `from_kernel`/`proj_from_kernel`, encode/decode of the WEAPON section,
  and an extended self-test (protocol 1/2/3/4 back-compat: each lower protocol omits exactly the
  later sections). Then `src/net/snapshot.{h,cpp}` mirrored it bit-for-bit.
- **Cross-impl parity gate** (mirrors det_math/geo001/snapshot exactly): `tools/gen_weapon_vectors.py`
  → `src/net/weapon_vectors.h` (hex-float/integer literals ⇒ byte-reproducible, CI `--check` stable)
  → `src/net/weapon_test_main.cpp` (`seads_weapon_test`, ctest `weapon_byteexact`) asserts **byte-
  identical encode** of full GEO+KIN+WEAPON frames and **round-trip decode** (hp/fire_cd/damage within
  one quantum; ttl/owner EXACT). Cases: empty world, golden start, a mixed furball (P-47 hp150 vs a
  dead hp0 corpse + a glass-cannon A6M2, rounds with distinct damage/ttl/owner at antimeridian/ceiling
  extremes), and the full 8-aircraft roster with a 12-round cloud.
- **Property tests** `tests/property/test_weapon_wire.py` (+6 ⇒ **100**): round-trip within quantum,
  protocol-3-omits-weapon, self-delimiting under trailing bytes, `proj_from_kernel` exact counters,
  WEAPON id-misalignment rejected, negative projectile count rejected.
- **Gates wired:** `make_receipt.py` 13th gate `weapon_codec`; `guardian.yml` gen `--check` +
  `seads_weapon_test` per leg (MSVC/GCC/Clang × x64/AArch64). Local: ctest **8/8** under GCC **and**
  Clang; receipt **13/13 PASS**.
- **Determinism invariant held:** **all 9 goldens byte-identical** (Sphere `f28ac561…a0d4a475`, …,
  Hit `1a460976…4ec901ee`) under GCC and Clang — the transport never touched the kernel. **No new
  golden**, so the guardian golden matrix is unchanged (it only gains the weapon test leg).

## 4) Consequences / boundaries

- **What is NOT done (by design):** the layer-4a interpolation buffer (`interp.{h,cpp}`) still
  interpolates GEO+KIN only; a wire-driven client reads hp/fire_cd/rounds off the **decoded Snapshot**
  directly (they are discrete / transient, not lerp targets). Carrying hp through interp is optional
  future polish, deliberately out of this seal to keep it tightly scoped to the snapshot framing.
- **Projectile identity** is per-frame (the recorder uses the SoA index as the wire id); the renderer
  treats rounds as a per-frame point cloud, so no cross-frame round tracking is implied or needed.
- **Forward compat:** protocol gating means an older decoder reading a protocol-4 frame stops cleanly
  at its known sections only if it checks the count — in practice both ends share `SNAPSHOT_PROTOCOL`.
  Lowering protocol (1/2/3) still produces valid, shorter frames (proven by the property tests).
- **Next (optional):** a real server/transport loop that actually ships these frames between processes;
  hp/round interpolation or event (kill/impact) messages; ammo/convergence/component-damage; **B5** ISA
  atmosphere. None blocked by this seal.
