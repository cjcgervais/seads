# ADR — Step 7 guns: Region damage + kill tally (seal ATM-Sphere v1.18r0)

**Status:** accepted · **Date:** 2026-07-02 · **Seal:** ATM-Sphere v1.17r0 → **v1.18r0**

## Context

The guns arc (G1→G4 + convergence + attribution + the per-round hit queue) models a complete
dogfight, but damage is a single scalar: every round drains one undifferentiated hp pool and the
only failure mode is death-at-zero. The roadmap has carried "component/region damage — the natural
consumer of per-round events" as the named next kernel seal since v1.12r0. Separately, attribution
(v1.16r0) records *who last hit me* but nothing tallies *victories*: a kill-feed can be derived,
but no canonical state answers "how many has this pilot downed?"

## Decision

### Region damage

Each airframe carries three functional **region sub-pools** beside the total hp — **ENGINE**
(0.375 × hp_start), **WING** (0.5 × hp_start), **TAIL** (0.25 × hp_start). Global exact-binary
fractions (3/8, 1/2, 1/4 — every roster hp_start is an integer, so every pool is exact in f64);
per-airframe region toughness is a future data-only tune. The pools are **independent thresholds,
NOT a partition**: a connecting round's damage books into the total hp (unchanged op order) AND
the struck region (clamped at 0; pass-through once dead).

**Region assignment is pure approach aspect.** At hit time,
`rel = wrap_pi(round.psi − target.psi)`:

| |rel| | region | reading |
|---|---|---|
| < π/4 | **TAIL** | fired from astern (round overtakes along the target's heading) |
| > 3π/4 | **ENGINE** | head-on (round flies against the target's heading) |
| otherwise | **WING** | beam attack (exactly π/4 lands in WING) |

`wrap_pi` + compares + `*`,`−` only ⇒ **zero new det_math** — the guns arc's streak holds through
its seventh seal. No new geometry (the hit point is already inside a 60 m sphere; a sub-60-m
lever arm cannot support finer position-based hit location honestly).

**A dead region degrades a LIVING aircraft** (hp≤0 death is unchanged and dominates):

* **ENGINE out** → thrust forced 0 at any throttle (a decelerating glider).
* **WING out** → the aerodynamic ceiling `n_aero` is **halved** (half the lifting surface; turn
  performance collapses; binds below the corner speed exactly like the B3 accelerated stall).
* **TAIL out** → control authority lost: the commanded (bank, load-factor) are overridden to
  **(0.0, 1.0)** — a straight 1-g mush. Throttle untouched (that is the engine's failure).

The tail-out override was deliberately chosen so that a target already flying straight and level
at 1 g is **bit-identical** through tail-out — every sealed scenario's victim (Hit-001's and
SESSION-SK-001's A6M2s, shot from astern) flies that way, which is what makes the reseal provably
additive (below) and leaves the sealed session/event digests untouched.

### Kill tally

`kills` — per-aircraft victory count, incremented on the **attacker** exactly when its round
crosses the target hp > 0 → ≤ 0 (the same crossing the HitEvent `killed` flag marks; the per-round
queue guarantees a shared kill credits exactly the crossing round's owner). It persists through
the attacker's own death (a posthumous kill counts, like last_hit_by persisting on a corpse).

### State, snapshot, queue

`engine_hp / wing_hp / tail_hp / kills` are canonical hashed state — the **12th–15th per-aircraft
snapshot f64s** (appended after last_hit_by). `HitEvent` gains a **`region`** field (0/1/2) —
observable output, still never hashed. All four fields are **OFF the WEAPON-001 wire this seal**
(deferred exactly as ammo at G4 and last_hit_by at v1.16r0; the transport is a future
transport-only reseal when a client wants to draw damage state).

## Golden movement (why this is a seal)

Appending 4 f64s grows every snapshot ⇒ **all 10 prior goldens move — but provably additive**:
stripping the 12th–15th f64 from each v1.18r0 golden reproduces its v1.17r0 hash **byte-for-byte**
(10/10 pre-reseal strip-diff PASS). Region damage + kills perturb **no** trajectory/hp/ammo/
projectile in any sealed scenario: GOLDEN-SK-Hit-001's A6M2 loses its tail (astern attack, pool
17.5 → 0 at the 2nd round) but tail-out is a no-op for a straight-and-level 1-g target, and its
killer's tally reads **kills = 1** — meaningful new state on an unchanged trajectory.

New golden **GOLDEN-SK-EngineOut-001** (700 ticks): a P-47D and an A6M2 meet head-on; a 4-round
burst connects at ticks 29/31/33/35, every round assigned ENGINE (|rel| = π), the engine pool
drains 26.25 → 0 on the 3rd round while total hp only reaches 22 — a LIVING aircraft with a dead
engine that decelerates thrustless (150 → ~131 m/s by tick 700) in level 1-g flight. World hash
`e9617633…fc3ba078`, C++ ≡ Python bit-for-bit under GCC + Clang.

## Consequences

* Rails 270→280 (`weapons.region_damage` doctrine block); seal v1.18r0.
* Only `lockstep_vectors.h` + `predict_vectors.h` regenerated (they hash `Kernel::snapshot()`);
  every wire-based vector header (`snapshot/weapon/session/event/interp/geo001/framing`) is
  **byte-identical** — the sealed SESSION-SK-001 digest `24f71845…c332` and EVENT digest
  `06629a69…` did not move. `scenario_params.h` regenerated for the new scenario.
* guardian.yml: the 11th golden joins the reference loop, the build-matrix `run_one` list, and
  the cross-toolchain aggregation list (first golden-list change since v1.13r0).
* Property tests 153 → **164** (+11 `test_region_damage.py`: pool derivation / aspect selects the
  region / double-booking + clamp / each region effect / the tail-out-noop strip property / kill
  tally exactly-once / shared-kill credits the crossing shooter / snapshot fields 12–15 /
  determinism); `test_hit_queue.py`'s snapshot-size gate updated to the 15-f64 record.
* Honest boundaries: region assignment is aspect-only (no round-vs-airframe geometry inside the
  hit sphere); regions do not (yet) ride the wire; no fire/fuel/pilot regions; per-airframe region
  fractions are a data-only follow-up. Each is a deliberate deferral, not an accident.
