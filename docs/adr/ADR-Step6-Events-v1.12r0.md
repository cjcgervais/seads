# ADR — Netcode Layer 6: the reliable combat-EVENT channel (rides seal v1.12r0)

**Status:** Accepted · **Date:** 2026-06-30 · **Seal:** rides ATM-Sphere **v1.12r0** (no rail/golden/wire change)
**Tier:** 2 (net layer; commit + Chronicle receipt, like layer 4a interp / layer 5 session). No `/seal`.

## Context

Layer 5 (the SESSION loop) reconstructs a whole dogfight from the wire, replicating combat **state**
— HP, positions, tracer rounds — via *nearest-frame* semantics. That works because HP is
**idempotent**: every later frame re-states it, so a dropped frame is harmless (the next one heals
it). But the *interesting* combat moments are **transient events**, and those are **not** idempotent:

- a **HIT** — target T lost D hitpoints *this* tick (drives the impact spark / damage number / the
  HP-bar flash),
- a **KILL** — T's hp crossed to ≤0 at *this* server tick (drives the kill-feed at the right instant).

Reading these off the nearest **state** frame is lossy in a way HP is not: the client sees only the
*aggregate* hp in whichever frame arrives, so it cannot tell one 12-damage hit from two, it learns of
the kill **late** (at the next delivered frame, not the tick it happened), and if the frame carrying
the moment is **dropped** the moment is smeared or lost entirely. Layer 5 could tell you "the A6M2 is
dead"; it could not faithfully tell you "the A6M2 took 12, 12, 12, 12, 12, then died at tick 54."

## Decision

Add **netcode layer 6 — a reliable combat-EVENT channel** (`tools/event_ref.py` ↔
`src/net/event.{h,cpp}`, `seads_event` lib), gated by `seads_event_test` +
`gen_event_vectors.py` → `event_vectors.h` + `tests/property/test_event.py`. It reuses the
**SESSION-SK-001** scenario (same 3-ship gundemo) and composes the existing transport — it adds a
**reliable event stream alongside** the snapshot frames, not new sealed-wire state.

- **Server DERIVES events by OBSERVING the authoritative kernel.** Each tick it snapshots every
  aircraft's hp *before* and *after* `step()`; any aircraft whose hp strictly decreased yields one
  `Event{seq, tick, target, damage_milli, hp_after_milli, killed}` (all integers — hp/damage
  quantized by the sealed `HP_SCALE=1e3`; hp is integer-valued so 1e3 carries it losslessly).
  **The kernel is NOT modified** (no event buffer, no new state) — pure observation between steps — so
  **all 9 goldens stay byte-identical**.
- **Redundant delivery.** Every 20 Hz frame carries an **event window**: the last `EVENT_WINDOW_K = 4`
  events by seq. This is a **session-layer message**, NOT a new section of the sealed snapshot wire —
  so there is **no wire reseal** (the same boundary layer 5 respects: net code composes the wire, it
  does not change the rail).
- **Client applies a de-duped journal.** It keeps `next_seq` (0 initially) and, on each *delivered*
  frame, appends every windowed event with `seq ≥ next_seq` in ascending seq (advancing `next_seq`).
  Events already applied are deduped; a permanently-lost early event does **not** stall later ones —
  the client resyncs from the freshest seq it can see (no head-of-line blocking).

The reconstructed event log is serialized canonically (through the same GEO-001 varint the wire uses)
and hashed; the whole-run SHA-256 **`EVENT_DIGEST`** is the cross-impl parity artifact the C++ mirror
reproduces bit-for-bit.

### Key decisions

1. **Redundancy, not retransmission-on-ack.** Piggybacking the last K events on every frame gives
   *eventual reliable delivery* with **zero round-trips and no client→server channel** (this loop is
   one-way server→client, like a spectate/replay stream). The cost is tiny (K·6 varints/frame).
2. **The failure bound is explicit and tested.** An event with seq S rides every frame from its
   creation until K newer events push it out — up to K frames of redundancy (**fewer** in a dense
   burst: the two earliest SESSION-SK-001 hits ride only 3 frames, which is exactly why a 3-frame
   blackout suffices to lose them). So S is lost **only if every frame carrying it is dropped** — a
   burst covering its whole in-window lifetime (at most K consecutive frames). Under isolated single
   drops (SESSION-SK-001 drops emits 30/55/80, never two in a row) the reconstruction is **complete**
   (`applied == server log`, digests equal). The tail —
   including the **kill** — rides *every* later frame (no newer event pushes it out once firing
   stops), so it is essentially always delivered. `event_vectors.h` carries a **blackout vector**
   (`BLACKOUT_DROPS = {40,45,50}`) that covers the whole in-window lifetime of the two earliest hits
   (seq 0,1): the client recovers exactly `{2,3,4,5}` — the aged-out hits permanently lost, the
   journal **resyncs**, the kill still delivered — and its `BLACKOUT_DIGEST` is reproduced by C++,
   proving the bound cross-impl. `test_event.py` proves the general bound on a controlled 10-event
   stream for every blackout offset.
3. **No head-of-line blocking (resync).** A lost event must not freeze the channel. `next_seq` jumps
   past an aged-out gap on the next window, so the client keeps delivering newer events (a kill after
   a blackout still lands). This is the deliberate difference from strict sequenced-reliable delivery,
   which would stall forever on the missing seq.
4. **Events are DERIVED, not authored by the kernel.** Doctrine keeps net code outside the kernel; the
   server *drives* the kernel (like lockstep/predict/session) and *observes* hp deltas to synthesize
   events. This is why the goldens don't move and no seal is needed.

## Determinism

Event derivation observes det_math hp (bit-exact cross-toolchain by the golden promise) and quantizes
it to integers; windowing, the transport's integer lag/drop, and the seq dedup are pure integer
logic. So the reconstructed event log serializes to identical bytes on MSVC/GCC/Clang × x64/AArch64,
and `EVENT_DIGEST` / `BLACKOUT_DIGEST` are the cross-impl equality checks — same construction as
geo001 / snapshot / session. Verified locally: `seads_event_test` PASS under GCC **and** Clang
(digests bit-identical), 15 receipt gates PASS, 113 property tests, all 9 goldens byte-identical.

## Consequences / scope

- **What ships:** the reference + C++ mirror + parity gate + 7 property tests + the CI legs
  (gen `--check`, ref self-test, `seads_event_test` per matrix cell) + this ADR + receipt.
- **Deliberately deferred (honest scope):**
  - **Attacker attribution.** HP-delta observation gives *target* + *damage* + *kill*, but not *who
    fired the round*. Per-round attacker attribution would need a kernel-side event hook (a kernel
    touch), so it is out of scope here; the event set is exactly what authoritative-state observation
    yields. A follow-up could add a `killer`/`attacker` field via such a hook (its own ADR).
  - **Per-round granularity.** Multiple rounds landing on one target in the *same* tick lump into a
    single "took D this tick" event (the authoritative per-tick hp delta). This is the right grain for
    a damage-number/impact-flash; a per-round stream would again need a kernel hook.
  - **Wiring events into the live `--fly` viewer** (kill-feed / damage numbers off a real transport)
    is renderer work, not gated here.

## Alternatives considered

- **Put events in the snapshot wire (protocol 5).** Rejected: that is a **wire reseal** (v1.13r0) for
  something that doesn't need to be in the sealed rail. Keeping the event window a session-layer
  message keeps this a no-seal net layer and leaves the protocol-4 wire untouched.
- **Strict sequenced-reliable (head-of-line blocking).** Rejected: one permanently-lost event would
  freeze all future events — worse than the redundant-journal-with-resync, which degrades gracefully.
- **A kernel event buffer.** Rejected for now: it touches the sealed kernel; observation is
  doctrine-cleaner and keeps the goldens byte-identical.

## Verification

`python tools/event_ref.py` (reference self-test: derivation + full reconstruction + kill + the
synthetic K-consecutive bound) · `python tools/gen_event_vectors.py --check` · `seads_event_test`
(ctest `event_reliable`, GCC+Clang) · `python -m pytest tests/property/test_event.py`
(7 properties) · `python tools/make_receipt.py` (gate `event`). Rides seal **v1.12r0**;
no rail / golden / wire / kernel / det_math change.
