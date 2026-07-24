# Decisions

Standing decisions for the flight kernel. Each entry: date, decision, why, status.

---

## RECONCILIATION WATCH-ITEM — `feel/kernel-v5` diverges from `main` (pushed as backup only)

**The active kernel Chad is flight-testing is NOT on `main`.** It's on
`D:\flight_sim2\seads-feel`, branch `feel/kernel-v5`, HEAD `d1e7dbe6b` as of late 2026-07-23
(rungs A through E plus the grafted felt-flight recorder, gate 372/372) — pushed to origin
(backup only, no workflow change), still diverging from `main`
(which is still v4, `d68de7d91`). Every rung-A/A2/C/D/E decision below lives only on that
branch until it merges.

**Every future session must check `feel/kernel-v5`'s branch/merge state first** (`git -C
D:\flight_sim2\seads-feel log --oneline main..feel/kernel-v5`, or check whether it's merged)
before treating anything in this file, `docs/cascade/push-gate-knife-edge.md`, or
`tuning/evc2026-v5-rungE.md` §2 as shipped-to-main. Any work baselining off `main` alone is a
full generation behind. Re-verify against a fresh snapshot of `D:\flight_sim2\seads-feel`
before assuming these rungs are still current — a live session may have moved past rung E
already.

---

## 2026-07-23 — The v5 rung ladder (rungs A → E), `feel/kernel-v5`

Chad flew `main` (v4-approved) and reported small, smooth mouse adjustments made the
elevator/rudder overshoot the aim circle and bounce — "at those small deflections it is
treating them like they are big deflections." Five rungs of measured, Chad-approved fixes
followed, each with its own dial and its own explicit walk-back/kill value (see
`tuning/evc2026-v5-rungE.md` §2 for the full table with pre-rung values). In order:

- **Rung A — S-truedepth** (`dc0b1d01c`): a capture event's "glance depth" is capped at its
  own momentum-earned stopping distance rather than a fixed rim target. Dial:
  `capture_depth_frac = 1.5` (walk back to `≤ 0` for the v4 fixed-rim behavior).
- **Rung A2 — sub-wall curve** (`c8e98afa3`): Chad's fly-1 verdict — "better, just needs a
  little more" on small/fine adjustments. Dial: `capture_depth_pow = 2.0` (walk back to
  `1.0` = rung A bit-identically).
- **Rung C — hold-the-line** (`f523177e9`): Chad's fly-2 ruling — "It should hold the line of
  my mouse inputs and try to get to my mouse until full stall — it might sink a bit as I
  begin the stall but the nose should stay where my mouse is asking." Dials: `K_aoa: 5.0 →
  10.0`, `pull_floor: 0.0 → 1.0` (0.0 is the structural OFF / bit-identical legacy tree).
- **Rung D — the arcade energy model** (`c0625ede1`): Chad's ruling — "give me the power — I
  had been intuiting all along that the airframe is being underserved" (this **supersedes**
  his own earlier 2026-07-08 `T_max = 9000` ruling). Dials: `k_induced: 0.05 → 0.015`,
  `T_max: 9000 → 18000` (T/W 0.31 → 0.61), `n_max: 16 → 32`. **Correction:** an earlier draft
  of this repo's tuning notes had these dial directions backwards — `0.05`/`9000`/`16` are
  the values you walk BACK TO (pre-rung-D), not rung D's shipped values.
- **Rung E — the knife edge + the red arrow** (`89447aba5`): see the entry below.

Full detail, measured before/after grids, and red-team notes for every rung:
`reference/seads-feel/docs/v5_kernel_handoff.md`.

---

## 2026-07-23 — Push/split-S commitment gate moved to 45° (rung-E)

**Decision:** The push/split-S commitment gate's `push_horizon_enter` threshold moves from
1.0° below horizon to **45.0°**, with `push_horizon_exit` at 40.0° (5° hysteresis band).
Commitment now requires BOTH lateral deflection AND genuine down-aim past 45° below horizon.

**Why:** At 1.0°, the old side-cone gate degenerated at big lateral deflections — a long
lateral drag whose aim merely grazed slightly below the horizon (an artifact of the
aim-frame's own arc, not player intent) became eligible for committed nose-down, producing
"mystery dives" the player never asked for. Chad's own words: "I think it's the knife edge
set too high on the horizon... nose-down with the bank over should need my down input too,
past like 45 degrees." Moving the gate to 45° means a shallow lateral graze can never reach
it, while a genuine big flick (hard lateral + mouse well down) still commits cleanly and can
carry a full split-S through past inverted.

**Companion decision:** the aim reticle stays raw/unclamped (it is not re-pinned to the
screen edge when off-frame); a separate red arrow is drawn at the screen edge pointing at the
true aim direction whenever it goes off-screen. This makes the otherwise-invisible
below-horizon curve of an off-screen lateral drag visible, while the 45° gate makes it
harmless even when unnoticed.

**Status:** LANDED on `feel/kernel-v5` (commit `89447aba5`, 2026-07-23) — this is real,
committed code, not a proposal. **Pushed to origin** (as of the same evening), and **not
reflected** in either the
`reference/evc2026/` snapshot (a different, prior-generation Luau kernel that never had this
mechanism) or in `main` (still v4). See `docs/cascade/push-gate-knife-edge.md` and the
reconciliation watch-item above.

---

## Standing constraint — Camera independence for motion sickness

**Decision:** The camera may lag, ease, and visually swing toward the aim direction or
heading, but it must never be an input to the flight control law — only a one-way,
downstream function of state. No camera-derived quantity (FOV, orbit angle, zoom factor,
lag-eased heading) may feed back into `pitch`/`roll`/`yaw`/throttle.

**Why:** This is the single mechanism standing between "the camera looks like it's swinging
toward centre" (the intended illusion — see `docs/cascade/mouse-aim-instructor-cascade.md`)
and actual motion sickness. If camera motion ever leaked into control, the mismatch between
commanded and actual aircraft response would be subtle, constant, and — especially on
SEADS's small non-euclidean sphere where curvature is always present — very hard for a
player to build a stable mental model around (see
`docs/cascade/spherical-earth-non-euclidean.md`).

**Evidence this is taken seriously in the existing code:** `BirdController.client.luau`'s
right-click aim-zoom is explicitly documented as "AWARENESS-ONLY... cannot affect flying" and
its steering is suspended while it's active specifically so the FOV change can't feed back
through the camera-projection aim into pitch/bank.

**Status:** standing, not a one-time decision. Applies to both EvC2026 and SEADS camera work.

---

## 2026-07-23 — Recorder graft landed: SOUND-WITH-ONE-FIX, schema gains raw_flap/raw_gear

The felt-flight recorder proposal was grafted into seads-feel (`d1e7dbe6b` on
`feel/kernel-v5`, gate 372/372) after a four-gate review run, not trusted: symbol walk,
tick-level tap at the accumulator seam (AT-9), structural read-only, and the differential
leg on the real spherical plant (same seed, recorder on/off, `LoopState` bit-identical;
480-tick round-trip replay onto stored pins; tamper signature verified). Two new permanent
tests in the gate.

**The one fix, and the lesson:** the proposal flattened `raw_in` as
pitch/yaw/roll/throttle, but `sim::Inputs` also carries `flap_cmd`/`gear_cmd`, which
raw-mode flights command inside `raw_in`. A recorded raw-mode flight with flaps would have
replayed with a clean airframe — bit-perfect divergence of exactly the silent kind this
harness exists to kill. Schema now carries `raw_flap`/`raw_gear`. Changed while the
`.seadsrec` format was still v1-unshipped, so it was free; a day later it would have been a
migration. **Standing rule: when flattening a struct into a recording schema, walk every
field of the source struct, not the fields you remember.** Proposal copies in
`harness/seads_recorder_proposal/` are synced to the grafted versions
(`test/harness/recorder.h`, `test/unit/test_recorder_firewall.cpp` at `d1e7dbe6b`), which
are now authoritative. Deliberate scope note: the in-game record toggle (main.cpp key
wiring) lands as its own small change at first real recorded flight.
