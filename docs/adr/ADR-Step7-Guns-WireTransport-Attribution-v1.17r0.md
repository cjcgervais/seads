# ADR-Step7-Guns-WireTransport-Attribution-v1.17r0 — `last_hit_by` on the WEAPON-001 wire + `Event.attacker`

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor/Guardian
**Seal:** ATM-Sphere v1.17r0 (proposed; bumps from v1.16r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

v1.16r0 gave the kernel **attacker attribution**: each aircraft carries `last_hit_by` (the index of
the aircraft whose round most recently damaged it, or **-1 == never hit**), set at hit time from the
striking round's `owner`, persisting through death so at hp≤0 it names the **killer**. It became the
**11th per-aircraft canonical snapshot f64** — but, exactly as `ammo` was kernel-only at G4 (v1.13r0)
before it rode the wire at v1.14r0, **v1.16r0 deliberately kept `last_hit_by` OFF the WEAPON-001
wire** and named this reseal as the follow-up.

The visible gap was in TWO places, because attribution feeds both replication paths:
- **State path (snapshot wire):** a remote / late-join client could draw HP bars, kills, and a
  rounds-remaining counter off the decoded bytes — but not say *who* killed whom.
- **Event path (layer 6):** the reliable EVENT channel derived hit/kill *moments* from observed hp
  deltas, but an `Event` had no attacker field — the client's kill-feed could only say "AC1 died",
  never "AC0 downed AC1", even though the kernel now knows.

This ADR closes both. It is the fourth instance of the proven wire-reseal pattern
(KIN-001 v1.4r0 → WEAPON-001 v1.12r0 → ammo v1.14r0 → this).

## 2) Decision

**(a) Extend the WEAPON-001 per-aircraft record with `last_hit_by`, gated on snapshot protocol ≥ 6
(bump 5 → 6).** Appended inside the existing per-aircraft weapon loop, after `ammo_q`:

```
header (protocol, server_tick, n)
  GEO    section: n × (id, GeoPoint[lat,lon,bearing,alt])                 (GEO-001, geography-only)
  KIN    section: n × (id, phi_q, tas_q [, gamma_q])    [protocol >= 2]   (KIN-002 aux block)
  WEAPON section:                                        [protocol >= 4]   (WEAPON-001 aux block)
      n × (id, hp_q, fire_cd_q [, ammo_q] [, last_hit_by_q])   (ammo iff >=5; last_hit_by iff >=6)
      m  (projectile count)
      m × (pid, GeoPoint[lat,lon,bearing,alt], damage_q, ttl, owner)      live ballistic rounds
```

**Scale (new rail field `wire.weapon.lasthitby_scale = 1`).** `last_hit_by` is a **pure
integer-valued f64** (an aircraft index, or -1) — carried at **unit scale (1e0)**, exact AND compact,
exactly the `ammo` precedent. Unlike `ammo` it is **signed** (-1 == never hit); the GEO-001
ZigZag+LEB128 codec carries the sign natively (a -1 is one byte), so no special-casing.

**Why protocol 6, not extend protocol 5.** Same alignment argument as every prior bump: a protocol-5
decoder reading the extra per-aircraft varint would misalign the projectile count that follows. The
protocol gate keeps a protocol-5 frame byte-identical (proven by the back-compat self-test +
property test) and the field opt-in.

**(b) `Event.attacker` on the layer-6 reliable EVENT channel.** The server, which already derives a
hit/kill event from each observed hp delta, now also reads the target's `last_hit_by` **after the
step that dealt the damage** — by construction the striking round's owner — and stamps it into the
event as a 7th integer field `attacker` (encoded through the same GEO-001 i64 codec; defaults **-1 ==
unknown** so pre-attribution constructions still build). The kernel is still **observed, not
modified** — the hook (v1.16r0) made the observation possible. This is a **session-layer message
change, not a snapshot-wire section**, so it needs no protocol/rail of its own (the event framing was
never a sealed wire rail); its sealed artifact is the regenerated event parity digest.

**This is a TRANSPORT change, not a model change.** `last_hit_by` already exists as canonical state;
the `world_hash` source of truth (`Kernel::snapshot()`, raw LE-f64) is untouched. WEAPON-001 stays
lossy/downstream, never fed back as canonical. **No kernel / det_math / tuning / golden change** —
this is a seal only because the snapshot wire format is a sealed rail (constitution §1).

**Downstream riders (bundled; would not themselves need a seal):**
- **Session layer (netcode layer 5).** `serialize_world` passes `last_hit_by` (well-formed protocol-6
  frames); the reconstructed **client view** surfaces `last_hit_by_q` per aircraft ⇒ the
  whole-session **digest moves** (`fda717fe…` → `24f71845…`). `final_weapon_facts` / `FINAL_WEAPON`
  gain `last_hit_by`; the self-test asserts the client reconstructs **AC1 (A6M2) last_hit_by = 0
  (the P-47)** while the never-hit AC0/AC2 stay -1 — the attributed kill-feed, end-to-end over the
  lossy wire.
- **Event layer (netcode layer 6) — digest MOVES this time** (unlike v1.14r0, where events were
  untouched): the canonical event-log serialization gains the 7th field, so `EVENT_DIGEST`
  (`dfcc1aaf…` → `06629a69…`) and `BLACKOUT_DIGEST` (`94ae31ea…` → `90d6e67c…`) regenerate. The
  self-test now prints and asserts the attributed kill: `attacker=0 -> target=1 (AC0 downed AC1)`.
- **Recorder.** `seads_record` builds wire EntityStates with `k.last_hit_by(a)`, so a decoded
  recording carries attribution for the viewers.

## 3) Verification (gates)

- **Mirror-first.** `tools/snapshot_ref.py` extended first — `EntityState.last_hit_by` (default
  -1.0), `from_kernel`, encode/decode gated on `protocol >= 6`, self-test extended (exact -1/0/1
  round-trip + a protocol-**5** back-compat leg: carries ammo but NOT last_hit_by). Then
  `src/net/snapshot.{h,cpp}` mirrored bit-for-bit. Same for `event_ref.py` ↔ `event.{h,cpp}`
  (7-field Event) and `session_ref.py` ↔ `session.cpp` (client view + FINAL_WEAPON).
- **Cross-impl parity gates.** All four generated headers regenerated and in sync
  (`gen_{snapshot,weapon,session,event}_vectors.py --check` PASS): `seads_weapon_test` asserts
  byte-identical protocol-6 frames + exact `last_hit_by` round-trip; `seads_session_test` reproduces
  the new session digest; `seads_event_test` reproduces the new event + blackout digests.
  **ctest 10/10 under GCC and Clang.**
- **Property tests** (+2 ⇒ **128** total): `test_protocol5_omits_lasthitby` (a protocol-5 frame is
  strictly shorter and decodes last_hit_by = -1 while hp/ammo still round-trip — the older wire stays
  readable); `test_events_are_attributed_and_attribution_replicates` (every server event is
  attributed to the P-47, and under ANY loss pattern every reconstructed event reports the server's
  exact attacker). The weapon round-trip test now asserts `last_hit_by` carries EXACTLY (incl. -1).
- **Determinism invariant held: all 10 goldens byte-identical** — no `src/kernel/**`,
  `src/det_math/**`, or `data/tuning/**` file changed, so the canonical hash path is provably
  untouched. **No new golden, no new ctest target ⇒ `guardian.yml` unchanged.**

## 4) Consequences / boundaries

- **The attribution arc is closed end-to-end:** kernel hook (v1.16r0) → snapshot wire + reliable
  event channel (this seal). A remote client can render an attributed kill-feed ("P-47D downed
  A6M2") from either path: the freshest state frame or the exact-sequence event journal.
- **Forward compat:** protocol gating means lowering protocol (…4/5) still produces valid, shorter
  frames (property/self-tests). Both ends share `SNAPSHOT_PROTOCOL`.
- **What is NOT done (by design):** per-round hit granularity (which round hit, impact point) would
  need a kernel-side event *queue*, not a last-writer field — a different, bigger seal; wiring the
  kill-feed into the live `--fly` viewer HUD is renderer work (no-seal).
- **Next (optional, none blocking):** a genuinely cross-PROCESS transport (sockets) over the
  layer-5/6 frames; component/region damage; renderer meshes / guns in the live `--fly` path; or
  **B5** ISA atmosphere.
