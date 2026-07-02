# ADR-Step7-Guns-WireTransport-RegionDamage-v1.19r0 — region pools + kill tally on the WEAPON-001 wire

**Status:** Accepted
**Date:** 2026-07-02
**Author:** Forge (Claude) — Forge/Auditor/Guardian
**Seal:** ATM-Sphere v1.19r0 (proposed; bumps from v1.18r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

v1.18r0 gave the kernel **region damage + the kill tally**: each airframe carries ENGINE/WING/TAIL
**region sub-pools** (0.375/0.5/0.25 × hp_start, drained by the striking round's approach aspect;
a dead region degrades a LIVING plane) and **`kills`** (+1 on the attacker per killing round).
They became the **12th–15th per-aircraft canonical snapshot f64s** — but, exactly as `ammo` was
kernel-only at G4 (v1.13r0) before riding the wire at v1.14r0 and `last_hit_by` kernel-only at
v1.16r0 before riding at v1.17r0, **v1.18r0 deliberately kept all four OFF the WEAPON-001 wire**
and named this reseal as the follow-up.

The visible gap: a remote / late-join client could draw HP bars, ammo counters, and an attributed
kill-feed off the decoded bytes — but not the **damage state** (is that A6M2 a thrustless glider?
is its tail shot away?) or a **scoreboard** (who has how many kills). The kernel knew; the wire
didn't carry it.

This ADR closes it. It is the **fifth instance of the proven wire-reseal pattern**
(KIN-001 v1.4r0 → WEAPON-001 v1.12r0 → ammo v1.14r0 → last_hit_by v1.17r0 → this).

## 2) Decision

**Extend the WEAPON-001 per-aircraft record with `engine_hp`, `wing_hp`, `tail_hp`, `kills`,
gated on snapshot protocol ≥ 7 (bump 6 → 7).** Appended inside the existing per-aircraft weapon
loop, after `last_hit_by_q`, in canonical snapshot order:

```
header (protocol, server_tick, n)
  GEO    section: n × (id, GeoPoint[lat,lon,bearing,alt])                 (GEO-001, geography-only)
  KIN    section: n × (id, phi_q, tas_q [, gamma_q])    [protocol >= 2]   (KIN-002 aux block)
  WEAPON section:                                        [protocol >= 4]   (WEAPON-001 aux block)
      n × (id, hp_q, fire_cd_q [, ammo_q] [, last_hit_by_q]
           [, engine_hp_q, wing_hp_q, tail_hp_q, kills_q])
                       (ammo iff >=5; last_hit_by iff >=6; region pools + kills iff >=7)
      m  (projectile count)
      m × (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl, owner)      live ballistic rounds
```

**Scales (new rail fields `wire.weapon.{enginehp,winghp,tailhp}_scale = 1000`,
`wire.weapon.kills_scale = 1`).** The region pools are quarter-integer-valued f64s (every roster
hp_start is an integer and the global fractions are exact-binary 0.375/0.5/0.25, drained by
integer damage) — carried at **milli scale (1e3), exactly like hp**, so every reachable value is
exact on the wire. `kills` is a **pure integer counter** — carried at **unit scale (1e0)**, exact
AND compact, exactly the ammo/last_hit_by precedent.

**Why protocol 7, not extend protocol 6.** Same alignment argument as every prior bump: a
protocol-6 decoder reading the four extra per-aircraft varints would misalign the projectile
count that follows. The protocol gate keeps a protocol-6 frame byte-identical (proven by the
back-compat self-test + property test) and the fields opt-in.

**This is a TRANSPORT change, not a model change.** All four fields already exist as canonical
state; the `world_hash` source of truth (`Kernel::snapshot()`, raw LE-f64) is untouched.
WEAPON-001 stays lossy/downstream, never fed back as canonical. **No kernel / det_math / tuning /
golden change** — this is a seal only because the snapshot wire format is a sealed rail
(constitution §1). Unlike v1.17r0 there is **no event-layer change**: the layer-6 `Event` record
is untouched and the sealed **EVENT digest `06629a69…` did not move** (`HitEvent.region` remains a
kernel-side observable; putting it on the event wire would be a separate decision with its own
consumer).

**Downstream riders (bundled; would not themselves need a seal):**
- **Session layer (netcode layer 5).** `serialize_world` passes the four fields (well-formed
  protocol-7 frames); the reconstructed **client view** surfaces them per aircraft ⇒ the
  whole-session **digest moves** (`24f71845…c332` → `7e275f2b…49eb`). `final_weapon_facts` /
  `FINAL_WEAPON` gain engine/wing/tail_milli + kills; the self-test asserts the client
  reconstructs, purely from bytes, that the astern-shot **A6M2's TAIL pool is 0** (engine/wing
  intact) and the **P-47 reads kills = 1** — the replicated damage state + scoreboard, end-to-end
  over the lossy wire.
- **Recorder.** `seads_record` builds wire EntityStates with the four kernel accessors and emits a
  `"kills"` array beside `"hp"`/`"ammo"` in the JSON mirror (the wire-sourced scoreboard for the
  web HUD).
- **Framing vectors.** `framing_vectors.h` regenerates because its example payloads are whole
  default-protocol snapshot frames (now protocol 7); the layer-7 framing codec itself
  (`LEB128(len)||payload`) is byte-unchanged and payload-agnostic.

## 3) Verification (gates)

- **Mirror-first.** `tools/snapshot_ref.py` extended first — `EntityState.{engine_hp,wing_hp,
  tail_hp,kills}` (defaults 0.0), `from_kernel`, encode/decode gated on `protocol >= 7`, self-test
  extended (quarter-fraction pools + exact-integer kills round-trip + a protocol-**6** back-compat
  leg: carries last_hit_by but NOT the pools/kills). Then `src/net/snapshot.{h,cpp}` mirrored
  bit-for-bit. Same for `session_ref.py` ↔ `session.cpp` (client view + FINAL_WEAPON).
- **Cross-impl parity gates.** Four generated headers regenerated and in sync
  (snapshot/weapon/session/framing `--check` PASS); the other nine (geo001, interp, lockstep,
  predict, event, scenario, golden, envelope, detmath) verified **byte-identical** — the inverse
  fingerprint of a transport-only change (a kernel seal moves lockstep/predict and spares the wire
  vectors; this moves the wire vectors and spares lockstep/predict). `seads_weapon_test` asserts
  byte-identical protocol-7 frames + quantum-exact round-trip of all four fields;
  `seads_session_test` reproduces the new session digest + the extended FINAL_WEAPON;
  `seads_event_test` reproduces the UNCHANGED event digests. **ctest 17/17 under GCC and Clang**
  (all five socket bridges — netloop/multiclient/netdyn/netcatchup/netcap — reconstruct the new
  sealed session digest over real 127.0.0.1 sockets).
- **Property tests** (+2 ⇒ **166** total): `test_protocol6_omits_regions` (a protocol-6 frame is
  strictly shorter and decodes pools/kills = 0 while hp/ammo/last_hit_by still round-trip — the
  older wire stays readable); `test_region_damage_and_scoreboard_replicate_under_loss` (under ANY
  extra packet-loss pattern the client's freshest frame reconstructs AC1 tail-out + AC0 kills=1
  exactly). The weapon round-trip test now asserts the pools within one milli quantum and `kills`
  EXACTLY.
- **Determinism invariant held: all 11 goldens byte-identical** — no `src/kernel/**`,
  `src/det_math/**`, or `data/tuning/**` file changed (receipt gates validate_snapshot +
  validate_scenarios PASS against the sealed hashes). **No new golden, no new ctest target ⇒
  `guardian.yml` unchanged.**

## 4) Consequences / boundaries

- **The region-damage arc is closed end-to-end:** kernel sub-pools + degradation + tally
  (v1.18r0) → snapshot wire (this seal). A remote client can render an engine-out glider, a
  shot-away tail, and a live scoreboard from the freshest state frame.
- **Forward compat:** protocol gating means lowering protocol (…5/6) still produces valid,
  shorter frames (property/self-tests). Both ends share `SNAPSHOT_PROTOCOL`.
- **What is NOT done (by design):** `HitEvent.region` stays off the layer-6 event wire (no
  consumer yet; the state wire now carries the resulting pools, which is what a HUD draws);
  per-airframe region toughness stays a data-only envelope follow-up; drawing the damage state in
  the live `--fly` viewer HUD is renderer work (no-seal).
- **Next (optional, none blocking):** renderer polish (damage state + kill-feed + scoreboard in
  the live `--fly` path — every field now rides the wire); **B5** ISA atmosphere (a seal); an
  open-ended live frame SOURCE feeding `broadcast_async`; or per-airframe region toughness
  (data-only envelopes + a kernel consumer — its own ADR).
