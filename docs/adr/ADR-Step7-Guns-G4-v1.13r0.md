# ADR — Step 7 Guns / G4: Finite Ammunition (seal ATM-Sphere v1.13r0)

- **Status:** Accepted — sealed **ATM-Sphere v1.13r0** (2026-06-30)
- **Supersedes/extends:** ADR-Step7-Guns-G3-Roster-v1.13r0 (per-airframe weapon roster + fire-rate).
  Closes the kernel gunnery model: ballistics (G1) → hit/hitpoints (G2) → roster/fire-rate (G3) →
  **ammunition (G4)**.
- **Realm:** ATM-only. Rides the sealed sphere (R=15000, Δt=0.01, g₀=9.80665) unchanged.

## Context

Through G3 the kernel modelled a full dogfight-gunnery loop — but every gun had an **infinite
magazine**. That is a glaring gap in a WWII gun sim: ammunition endurance was a first-order tactical
constraint (the A6M2's two 20 mm cannon carried only ~60 rounds per gun — a few seconds of fire —
while the P-47D's eight .50-cals carried deep belts). Without it, the "glass cannon" character G3 gave
the A6M2 is only half-drawn: it hits hard and slowly, but it should *also* run dry fast.

## Decision

Add **finite ammunition** as a per-airframe kernel state, gating fire on a non-empty magazine.

1. **Envelope scalar `ammo_start`** (the magazine size, abstract rounds) joins
   `tools/envelopes.py::AERO_FIELDS` — the single source of truth that drives both `ref_kernel.py`
   and the generated C++ `Envelope` aggregate. Added to all 8 roster envelopes. Relative WWII
   endurance: **A6M2 fewest (100)**, then Yak-3 140, Spitfire 150, La-7 170, Ki-61 190, Bf 109 200,
   P-51 280, **P-47D most (340)**.

2. **Kernel state `ammo`** (per-aircraft SoA `ammo_`, seeded from `ammo_start`; no-arg/Sphere default
   `START_AMMO = 500.0`, a shared exact hex-float). Firing, already gated on `fire && alive &&
   fire_cd == 0`, gains **`&& ammo > 0`**; on a shot the round spawns, **`ammo -= 1`**, and `fire_cd`
   resets. An empty magazine ("Winchester") takes no shot and does **not** reset the cooldown — the
   gun simply falls silent while the trigger stays held. Decrement-then-fire order and the gate mirror
   `Kernel::step(cmd,env)` ↔ `ref_kernel.step_scenario` op-for-op.

3. **`ammo` is the 10th per-aircraft snapshot f64** (appended after `fire_cd`). It is canonical state
   hashed into the `world_hash`. Appending it grows every snapshot, so — exactly as `gamma` (B2),
   `hp` (G2) and `fire_cd` (G3) did — **all 9 prior goldens move**, their trajectories/hp/fire_cd
   byte-identical (verified: the Sphere golden's first 9 f64 + projectile block are byte-for-byte
   equal to the v1.12r0 golden; only `ammo = 500.0` is inserted).

4. **New golden `GOLDEN-SK-Winchester-001`** demonstrates the gate biting: an A6M2 flies wings-level
   and holds the trigger; with magazine 100 and rof 9 it looses one round every 9 ticks (ticks
   0, 9, …, 891 = 100 rounds), empties at tick 891, and from tick 892 the cannon is silent even
   though the trigger is still held. At tick 950 the snapshot shows `ammo = 0` and exactly the 22
   rounds fired in the last ttl=250 ticks still airborne.

### No new det_math

`ammo` is a pure integer-valued counter (a comparison, a subtraction) — like `fire_cd`. The banned-
symbol / MPFR-oracle surface is untouched (`det_sin`/`det_cos` + `+ − × ÷` only). No FMA, no libm.

### Wire transport deferred (deliberate, mirrors G3)

`ammo` lives in the canonical state but is **not** added to the WEAPON-001 snapshot wire in this seal
— exactly as `fire_cd` was kernel-state at G3 (v1.11r0) before it rode the wire at v1.12r0. The wire
is a lossy downstream subset; a remote client cannot yet show a "rounds remaining" counter. That is a
clean, self-contained follow-up (a WEAPON-001 → protocol bump), and keeping it out here leaves the
sealed wire/`weapon_vectors` parity gate byte-identical.

## Consequences

- **Goldens:** the 9 prior goldens move (ammo appended; constant/identical behaviour — no airframe
  depletes inside any of them, max shots in a prior golden = 7 ≪ smallest magazine 100) + the new
  `GOLDEN-SK-Winchester-001`. **10 goldens total.** C++ ≡ Python bit-for-bit under GCC + Clang for all
  ten; guardian.yml gains the Winchester id in its three golden loops.
- **Gates:** 15/15 receipt gates PASS; 116 property tests (+3: ammo character, exact-depletion cadence,
  no-underflow); ctest 10/10 GCC + Clang; 13 generated headers in sync.
- **Scope:** kernel (`kernel.{h,cpp}`), reference (`ref_kernel.py`), the envelope schema
  (`envelopes.py` + 8 JSONs + `flight_types.h`), the scenario/session/event builders (thread
  `ammo_start` where `hp_start` was threaded), the new scenario, and docs. The session/event **wire
  digests are unchanged** (`25fcc41e…`, `dfcc1aaf…`) — ammo is off-wire and no airframe depletes in
  the short SESSION-SK-001 fight.
- **Reseal rationale:** a golden `world_hash` change (state-vector growth + a new golden) ⇒ a seal per
  the Change-Control Law. Rails `version` 220 → 230.

## Alternatives considered

- **Store ammo as u32** (not f64). Rejected: the per-aircraft snapshot block is uniformly f64
  (lat…fire_cd); an integer-valued f64 keeps the block homogeneous and the codec trivial, matching
  `hp`/`fire_cd` (also integer-valued f64s).
- **Deplete inside an existing golden** (tune ammo so Gunfire/Hit run dry) instead of a new golden.
  Rejected: those windows are short (≤7 shots); forcing depletion there would need implausibly tiny
  magazines and would entangle the demonstration with unrelated hit geometry. A dedicated single-ship
  Winchester golden is the cleaner, unambiguous gate.
- **Wire the ammo counter in the same seal.** Rejected: unnecessary coupling; splitting state from
  transport is the proven rhythm (G3 → v1.12r0) and keeps the sealed wire parity gate untouched.
