# ADR-Step7-Guns-WireTransport-Ammo-v1.14r0 — `ammo` on the WEAPON-001 snapshot wire

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor/Guardian
**Seal:** ATM-Sphere v1.14r0 (proposed; bumps from v1.13r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

G4 (seal v1.13r0) closed the kernel gunnery model with **finite ammunition**: each envelope carries
`ammo_start`, firing is gated on `ammo > 0`, and an empty magazine goes silent ("Winchester").
`ammo` became the **10th per-aircraft canonical snapshot f64** (hashed into the `world_hash`) — but,
exactly as `fire_cd` was kernel-only at G3 before it rode the wire at v1.12r0, **G4 deliberately kept
`ammo` OFF the WEAPON-001 snapshot wire** and named "put ammo on the wire" as the follow-up.

The visible gap: the WEAPON-001 wire (v1.12r0) lets a remote / late-join client draw HP bars, tracer
rounds, and kills straight off the decoded bytes — but **not a rounds-remaining counter**, because
`ammo` wasn't transmitted. A wire-only client could see a plane still pulling the trigger with an
empty magazine and have no way to know it was Winchester. This ADR closes that last gap. It is the
third instance of a pattern already executed twice (KIN-001 at v1.4r0, WEAPON-001 at v1.12r0):
a canonical state field graduates onto the sealed wire as a small, transport-only reseal.

## 2) Decision

**Extend the WEAPON-001 per-aircraft record with `ammo`, gated on snapshot protocol ≥ 5 (bump 4 → 5).**
The weapon section already carried `n × (id, hp_q, fire_cd_q)` per aircraft; `ammo_q` is appended
inside that per-aircraft loop, present iff `protocol >= 5`:

```
header (protocol, server_tick, n)
  GEO    section: n × (id, GeoPoint[lat,lon,bearing,alt])                (GEO-001, geography-only)
  KIN    section: n × (id, phi_q, tas_q [, gamma_q])   [protocol >= 2]   (KIN-002 aux block)
  WEAPON section:                                       [protocol >= 4]   (WEAPON-001 aux block)
      n × (id, hp_q, fire_cd_q [, ammo_q])                                (ammo_q iff protocol >= 5)
      m  (projectile count)
      m × (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl, owner)      live ballistic rounds
```

**Scale (new rail field `wire.weapon.ammo_scale = 1`).** `ammo` is a **pure integer counter**
(magazine rounds, kernel-side an integer-valued f64). Unlike `hp`/`fire_cd`/`damage` (quantized ×1e3),
`ammo` is carried at **unit scale (1e0)** — for an integer domain that is both **exact and compact**
(a magazine of 340 is a 2-byte ZigZag+LEB128 varint, not 3), matching the semantic of `ttl`/`owner`
(which are carried exactly) while keeping the per-aircraft loop's `quantize/dequantize` code shape
homogeneous. `ammo` is always ≥ 0, so no sign concern.

**Why protocol 5, not extend protocol 4.** A protocol-4 decoder reading a frame with the extra
per-aircraft `ammo_q` would misalign the projectile count that follows. Bumping the protocol keeps a
protocol-4 frame byte-identical to before (proven by the back-compat self-test / property test), and
the protocol gate makes the new field opt-in — the same shape decision as KIN-001→KIN-002.

**This is a TRANSPORT change, not a model change.** `ammo` already exists as canonical state (the
`world_hash` source of truth is `Kernel::snapshot()`, raw LE-f64, untouched here). WEAPON-001 is a
separate, **lossy, downstream** transport, never fed back as canonical. Therefore **no kernel /
det_math / golden change** — the only reason this is a seal at all is that the wire format is a sealed
rail (§1 of the constitution): touching the protocol/scales requires a reseal.

**Downstream riders (bundled; would not themselves need a seal):**
- **Session layer (netcode layer 5).** `serialize_world` now passes `ammo` (so the protocol-5 frame
  is well-formed), and the reconstructed **client view** (`encode_client_view`) surfaces `ammo_q` per
  aircraft, so the client's per-tick view — and its whole-session digest — now carries the
  rounds-remaining counter. `final_weapon_facts` / `FINAL_WEAPON` gain `ammo`, and the session parity
  test asserts it. **The session digest moves** (a no-seal net-layer artifact regenerated from the
  reference); the reconstructed fight now shows the P-47 walking its magazine down (340 → 333 over the
  20-tick burst) while the never-firing A6M2 / Spitfire stay full — the feature, end-to-end over the
  lossy wire.
- **Event layer (netcode layer 6) is untouched and byte-identical.** It derives hit/kill events by
  observing kernel `hp` deltas, and its digests are over the event log (seq/tick/target/hp) — never
  over `ammo` or raw frame bytes. `event_vectors.h` is byte-identical across this reseal (proven by
  `--check` before regeneration).
- **Recorder / renderer.** `seads_record` builds wire EntityStates with `ammo` and the decoded
  `trajectory.js` now emits an `"ammo"` array per frame, so the web viewer HUD can show a
  rounds-remaining counter straight off the wire.

## 3) Verification (gates)

- **Mirror-first.** `tools/snapshot_ref.py` (the reference that *defines* the wire bytes) extended
  first — `EntityState.ammo`, `from_kernel`, encode/decode gated on `protocol >= 5`, and an extended
  self-test (protocol **4** back-compat: carries hp/fire_cd + rounds but **not** ammo; protocol 3/2/1
  omit the weapon block entirely). Then `src/net/snapshot.{h,cpp}` mirrored it bit-for-bit.
- **Cross-impl parity gate** (mirrors det_math/geo001/snapshot exactly): `tools/gen_weapon_vectors.py`
  → `src/net/weapon_vectors.h` (hex-float/integer literals ⇒ byte-reproducible, CI `--check` stable)
  → `seads_weapon_test` (`weapon_byteexact`) asserts **byte-identical encode** of full protocol-5
  GEO+KIN+WEAPON frames and **round-trip decode** (hp/fire_cd/ammo within one quantum; ttl/owner
  EXACT). Cases now carry varied magazines incl. a Winchester (ammo 0) corpse and the full 8-aircraft
  roster with distinct magazines.
- **Session parity gate.** `tools/gen_session_vectors.py` → `src/net/session_vectors.h` regenerated
  (new `SEQUENCE_DIGEST`, checkpoints, and `FINAL_WEAPON` with `ammo`); `seads_session_test`
  (`session_reconstruct`) reproduces it bit-for-bit under GCC and Clang.
- **Property tests** `tests/property/test_weapon_wire.py` (+1 ⇒ **117** total): round-trip within
  quantum incl. `ammo`; new `test_protocol4_omits_ammo` (a protocol-4 frame is strictly shorter and
  decodes ammo = 0 while hp/fire_cd still round-trip); existing self-delimiting / id-misalignment /
  negative-count tests still green.
- **Determinism invariant held:** **all 10 goldens byte-identical** (Sphere `40ff6dd2…59315881`, …,
  Winchester `473bcad9…7630ad66`) under GCC **and** Clang — no `src/kernel/**`, `src/det_math/**`, or
  `data/tuning/**` file changed, so the canonical hash path is provably untouched. **No new golden**,
  **no new ctest target** ⇒ `guardian.yml` is unchanged (it already runs all 10 goldens + the weapon /
  session legs). Local: ctest **10/10** under GCC and Clang; all 4 generated headers in sync.

## 4) Consequences / boundaries

- **What is NOT done (by design):** the layer-4a interpolation buffer still interpolates GEO+KIN only;
  a wire-driven client reads `ammo` (like hp/fire_cd/rounds) off the **decoded Snapshot** directly
  (it's a discrete counter, not a lerp target).
- **Forward compat:** protocol gating means lowering protocol (1/2/3/4) still produces valid, shorter
  frames (proven by the property/self-tests). Both ends share `SNAPSHOT_PROTOCOL`.
- **Guns arc fully wired.** With `ammo` on the wire, the entire G1→G4 gunnery model (ballistics, hit /
  damage, roster / fire-rate, finite ammunition) is now both canonical **and** replicable over the
  20 Hz wire.
- **Next (optional, none blocking):** a real cross-PROCESS transport (sockets) shipping these frames;
  attacker attribution (a kernel event hook, its own ADR); renderer meshes / guns in the live `--fly`
  path; or a new seal (gun convergence / component-damage, **B5** ISA atmosphere).
