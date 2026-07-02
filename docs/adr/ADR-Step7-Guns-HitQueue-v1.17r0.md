# ADR-Step7-Guns-HitQueue-v1.17r0 — Per-round hit granularity: the kernel hit event queue

**Status:** Accepted
**Date:** 2026-07-02
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL, rides ATM-Sphere v1.17r0** (kernel observable-output change; world_hash
untouched, ALL 10 goldens byte-identical)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Combat "moments" have been carried by two mechanisms, both deliberately coarser than a round:

- **`last_hit_by` (v1.16r0)** — a per-aircraft LAST-WRITER field: the index of the aircraft whose
  round most recently damaged it. It names the killer at hp≤0, but it keeps only ONE writer: two
  rounds landing on one tick leave a single attribution, the earlier shooter's credit lost.
- **The layer-6 event channel (rides v1.12r0)** — the server DERIVED hit/kill events by OBSERVING
  per-tick hp deltas around `step()`. Correct and kernel-untouched, but structurally lossy: a tick
  in which a target takes two rounds is ONE lumped "took D this tick" event, attributed via the
  last-writer field; which round crossed hp→0, and each round's own damage, are unrecoverable.

Every handoff since G2 named the fix: **per-round hit granularity needs a kernel event QUEUE, not a
last-writer field** — only the kernel knows, at hit time, WHICH round resolved against WHOM.

## 2) Decision

**(a) The queue (`tools/ref_kernel.py` ↔ `src/kernel/kernel.{h,cpp}`, mirrored).** The kernel gains
`hit_events` — a list of `HitEvent{target, attacker, damage, hp_before, hp_after, killed}` records,
one **per connecting round**, appended at hit time in `_advance_projectiles` /
`advance_projectiles_` (projectile array order within a tick ⇒ deterministic), **cleared at the top
of every step** (both overloads). Semantics:

- `attacker` = the striking round's `owner` (read directly off the round, not off `last_hit_by`);
- `hp_before`/`hp_after` are post-clamp reality; an overkill round's **effective loss is
  `hp_before − hp_after`**, while `damage` keeps the carried as-fired value;
- `killed` = 1 on exactly the round that crossed hp >0 → ≤0 (a corpse cannot be hit — G2's alive
  gate — so at most one kill per target, ever);
- the queue is **OBSERVABLE OUTPUT, not canonical state**: it is never serialized into
  `snapshot()`, so the world_hash cannot see it. `last_hit_by` (canonical, on-wire) is unchanged.

**(b) The consumer (`tools/event_ref.py` ↔ `src/net/event.{h,cpp}`).** The layer-6 event server now
sources events from the queue instead of re-deriving them from hp deltas. The `Event` wire record
is UNCHANGED (same 7 integer fields, same window/journal/dedup machinery): `damage_milli` is the
quantized effective loss, so per-tick sums equal the old lumped deltas, and **whenever no tick lands
two rounds on one target the per-round stream is bit-for-bit the old stream** — the sealed
SESSION-SK-001 `EVENT_DIGEST` (`06629a69…`) did not move. What changes is what the channel CAN
carry: same-tick multi-round damage arrives as distinct, separately-attributed events.

**(c) The granularity vector (`EVENT-MULTIHIT-001`).** A new cross-impl parity scenario in
`event_ref.py`/`gen_event_vectors.py`/`event_test_main.cpp`: twin P-47Ds symmetric about the
equator (lat ±0.2° ≈ ±52 m — inside the 60 m hit sphere of an equatorial target, outside each
other's at ~105 m) fire a 3-volley burst at one A6M2. By symmetry each volley's two rounds land on
the SAME tick ⇒ 6 events over 3 ticks: every tick two events on one target from two DIFFERENT
attackers (unrepresentable pre-queue), and the kill volley shows the overkill clamp (the A6M2 dies
at 22 hp to 12+12: shooter 0's round 22→10, shooter 1's round 10→0 with effective loss 10). The C++
harness reproduces events + digest (`8a071bb0…`) bit-for-bit (GCC+Clang) and asserts the structural
claims directly.

## 3) Why a kernel-side queue is finally justified (and why it is still no-seal)

Layer 6's doctrine note said events stay derivable outside the kernel *"so all goldens stay
byte-identical"* — and deferred per-round attribution as needing a kernel hook. v1.16r0 added the
minimal hook (one canonical f64, a seal). This rung completes it WITHOUT a seal because the queue
adds **no canonical state**: no snapshot bytes, no rails, no wire scales, no tuning. The receipt's
`validate_snapshot`/`validate_scenarios` gates prove all 10 goldens byte-identical; the hit-branch
arithmetic is untouched (one `before` temporary reads an existing value — no new float ops, and
**zero new det_math**, extending the guns arc's streak). It is the same trust boundary as
`Kernel::hp()` accessors: deterministic observable output the net layer may read, never feed back.

## 4) Consequences

- A renderer/kill-feed can show per-round impact sparks, split damage numbers, and correct
  per-shooter credit in shared kills — from either the kernel directly or the reliable channel.
- The event channel's redundancy/loss bound is unchanged (same window K=4, same journal); denser
  same-tick bursts simply shorten an event's window lifetime, exactly as before.
- The old hp-delta derivation remains a valid INVARIANT (per-tick sums), now gated as a property
  test rather than being the mechanism.
- Honest boundary: `HitEvent` is per-ROUND, not per-COMPONENT — region/component damage remains a
  possible future seal. The queue is also not exposed on any wire section itself; the layer-6
  session message remains the transport.

## 5) Gates (all green)

- **15/15 receipt gates PASS**, `receipt-ATM-Sphere_v1.17r0-<sha>.yml`; **ALL 10 GOLDENS
  BYTE-IDENTICAL** (Sphere `f2db95bd…` re-validated; Python ref re-run on Hit/Winchester/Sphere).
- **Property tests 145 → 153** (`tests/property/test_hit_queue.py`: queue empty when nothing
  connects / holds current step only / event matches the hit / multi-hit same tick SPLITS with
  distinct attackers + contiguous hp chain vs the last-writer field keeping one / killed marks the
  crossing round + overkill clamped + corpse unhittable / queue not hashed (snapshot size exact) /
  per-round reduces to hp-delta when single-hit / deterministic).
- **ctest 17/17 GCC + Clang** (`seads_event_test` gains the MULTIHIT leg — no new ctest target, so
  `guardian.yml` is UNCHANGED); `event_vectors.h` regenerated (sealed SESSION digests untouched,
  MULTIHIT block appended); all 14 generated headers `--check` in sync.
