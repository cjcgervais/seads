# ADR-Step7-Guns-G3-Roster-v1.11r0 — Guns Phase G3: per-airframe weapon roster + fire-rate

**Status:** Accepted
**Date:** 2026-06-30
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** ATM-Sphere v1.11r0 (proposed; bumps from v1.10r0)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

G1 (v1.9r0) gave the kernel deterministic ballistic rounds; G2 (v1.10r0) made them hit and kill,
with **global** gun constants (one generic gun, one global toughness). G3 — the final guns phase —
turns those globals into a **per-airframe weapon roster** so the 8 aircraft fight with their own
character: the P-47D's eight fast .50-cals on a rugged airframe vs the A6M2's slow, hard-hitting
20-mm cannons on a fragile one. This is mostly **data**, like B4 — but it adds real **fire-rate**,
which needs a small piece of per-aircraft state, so it is a seal that moves the goldens.

## 2) Decision

**Per-airframe weapon scalars (envelope data).** Four scalars join the envelope schema (appended to
`tools/envelopes.py AERO_FIELDS` and the `Envelope` struct, in order):

- `hp_start` — airframe toughness (starting hitpoints).
- `muzzle_v_mps` — muzzle velocity, added to the firer's TAS at spawn.
- `damage_per_round` — hitpoints removed per hit; **carried by the round** (set at spawn) so a round's
  lethality is fixed at fire time and the hit stays self-contained (the firer may be dead by impact).
- `rof_interval_ticks` — minimum ticks between shots (the fire-rate).

`PROJ_DRAG_K` and `PROJ_TTL_TICKS` stay **global** (a bullet is a bullet); `START_HP` stays the
global default for the no-arg/Sphere path (which has no envelope). The hit geometry (`HIT_RADIUS`,
`HIT_ALT_GATE`, `COS_HIT_ANGLE`) stays global (it reflects target size, not the weapon).

**Fire-rate (new per-aircraft state).** Each aircraft carries `fire_cd` (a fire-rate cooldown, in
ticks). In the spawn phase, **decrement-then-fire**: `if fire_cd>0: fire_cd−=1; if cmd.fire and alive
and fire_cd==0: spawn; fire_cd = rof_interval`. This makes held-trigger shots fall exactly
`rof_interval` ticks apart and is fully deterministic (array order, no wall-clock). A dead aircraft
(`hp≤0`) never fires.

**Spawn / hit plumbing.** `spawn_projectile_(owner, env)` reads the firer's `muzzle_v_mps` and
`damage_per_round`; the hit applies the round's **carried** `damage`. Scenario aircraft are seeded
with their envelope's `hp_start` (`Kernel::add` gained an `hp` parameter; the Sphere/no-arg path keeps
the 100 default).

**Serialization.** `fire_cd` is appended as the **9th per-aircraft f64** (after `hp`), and `damage`
as the **7th projectile f64** (after `gamma`). Both grow every snapshot, moving all 9 goldens. No new
det_math, no wire change (KIN-002 / protocol 3 unchanged; weapon wire transport still deferred).

## 3) The roster (initial balance pass)

| airframe | hp_start | muzzle (m/s) | dmg/round | rof (ticks) | character |
|---|---|---|---|---|---|
| P-47D   | 150 | 850 | 12 | 3 | 8×.50cal — durable, high volume |
| P-51    | 110 | 850 | 12 | 4 | 6×.50cal |
| Bf 109 F-4 | 100 | 750 | 25 | 8 | 20-mm motor-cannon + MGs |
| Ki-61   |  95 | 800 | 14 | 6 | 12.7-mm + 7.7-mm |
| A6M2    |  70 | 650 | 40 | 9 | 2×20-mm — glass cannon (fragile, slow, hard hit) |
| Yak-3   |  85 | 800 | 28 | 7 | ShVAK 20-mm + 12.7-mm, light |
| La-7    | 100 | 800 | 30 | 6 | multiple 20-mm |
| Spitfire Mk V | 100 | 800 | 26 | 5 | 2×20-mm Hispano + 4×.303 |

Like the B1/B4 aero, these are a **relative balance pass** (toughness ↔ lethality ↔ rate); retuning
stays data-only. P-47D is the toughest, A6M2 the most fragile; cannon airframes out-damage the .50-cal
platforms per round, which lean on a faster rate.

## 4) Why this is a seal (golden hashes move)

`fire_cd` (every aircraft) and `damage` (every round) grow each snapshot, and the scenario airframes
now carry per-airframe HP + weapon stats, so all 9 goldens move. The Gunfire/Hit scenarios also change
*behaviorally* (fire-rate gating cuts the round count; the A6M2's 70 HP changes the kill) — a real
content change, not just format growth. **Sphere** moves too (it gains `fire_cd=0`; trajectory + hp
byte-identical). All regenerated; **no new golden** (G3 reuses Gunfire/Hit, which now exercise the
roster), so `guardian.yml` is unchanged.

## 5) Determinism & verification

- **No new det_math.** Fire-rate is `+ - ==` on `fire_cd`; spawn/hit are lookups + the existing
  hit test. `lint_determinism` clean; no banned symbol, no FMA.
- **Mirror-first:** `ref_kernel.py` defines the roster + bytes; `kernel.cpp` bit-matches. All 9
  goldens reproduce **C++ ≡ Python under GCC + Clang** (18/18); guardian CI extends to MSVC +
  GCC/Clang × x64/AArch64.
- **Gates:** 12/12 receipt PASS at v1.11r0; ctest 7/7 GCC + Clang; **94 property tests**
  (+6: `test_weapon.py` roster sanity + relative character; `test_hit.py` fire-rate gating +
  per-airframe lethality; updated projectile/hit for per-env muzzle/damage).
- envelope_tables + lockstep/predict generated headers regenerated (envelope scalars + canonical
  snapshot moved); all 10 `--check` in sync.

## 6) Consequences

- **The guns arc (G1→G3) is complete:** deterministic ballistics, hit/damage, and a per-airframe
  roster with fire-rate. The kernel now models a full deterministic dogfight-gunnery loop.
- Natural no-seal follow-ups: a renderer that draws rounds + kills + HP; wire transport of hp/fire_cd
  + rounds for multiplayer (a netcode layer). Ammo counts, convergence, and component damage are
  possible future seals.

## 7) Alternatives considered

- **Look up damage via the firer's envelope on hit** (instead of carrying it on the round) — rejected:
  it couples `advance_projectiles_` to the envelope array and is wrong if the firer's weapon changes;
  carrying damage fixes lethality at fire time and keeps the hit self-contained.
- **Fire-rate via a global tick gate** (no per-aircraft state) — rejected: it can't express per-airframe
  rate or per-aircraft trigger timing; a cooldown is genuinely per-aircraft state.
- **Ammo counts now** — deferred: more state and balance surface; G3 is already a complete roster.
- **Per-airframe drag/ttl** — deferred: secondary to muzzle/damage/rate/toughness; kept global.
