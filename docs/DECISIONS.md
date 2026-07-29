# Decisions

Standing decisions for the flight kernel. Each entry: date, decision, why, status.

---

## LIVE-BRANCH WATCH-ITEM — `feel/kernel-v5` moves past the seal (reconciliation is DONE)

**Resolved 2026-07-24:** the v4→v5 reconciliation merged. `main` in the game trees is
**`game-kernel-v5` (`36ee936e9`)** — the full game (tunnels, ballistics, Sudbury, Bf 109)
flies kernel v5, gate 797/797, first landing ever put down, Golden Felt Flight #1 flown on
that build. The old "feel branch diverges from a v4 main" danger no longer exists.

**Current resting state (2026-07-28):** the feel branch tip `cfe1bd7fe` is **sealed as
`flight-kernel-v6-2026-07-28`** — tag and branch backup both pushed to origin, and all
seven post-v5-seal commits grafted into the seads-recon conquest tree
(`sandbox/kernel-v5-reconcile` @ `5e27f237c`, gate 887/887, controller golden transferred
without re-record). Chad's word at the seal: "getting very near the point I don't touch
it again for a while." The watch discipline stays — a future session may move the branch
past v6 at any time:

- `reference/seads-feel/` is snapshotted at **`cfe1bd7fe` = the v6 seal (2026-07-28)** —
  current through the whole approved session, including the recorder graft. If the live
  tip has moved past that, the live tree is ground truth again until the next re-snapshot.
- **Every future session must check the branch state first** (read-only `git -C
  D:\flight_sim2\seads-feel log --oneline` / `git status` — never write there) before
  treating any dial value, snapshot, or cascade Code section as current. The branch has
  been observed to move between two commands of the same session.

---

## 2026-07-28 — Rudder-bias trim + S-relorient, Chad-approved ("okay we have a winner")

Two changes landed on `feel/kernel-v5` in one session, both flown and approved on Chad's
stick. Commits: `274432f35` (S-relorient), `385a43dbd` (yaw_scale), `468f2b352` (red-team
folds), `b2019cf43` (flight-log rows). Gate 380/380 at each step.

**1. Rudder trim: `yaw_scale` 2.2 → 2.0.** Chad's ask: "a little too much rudder bias in
the equation" — confirmed symptoms: nose sits crabbed / rudder always working, plus
violent snap-back at speed. No v5 commit had touched the yaw ladder; the pre-v5 tuning was
being exercised harder by v5's stronger energy model (higher V ⇒ the q-scaled yaw terms
bite more). 2.0 is the previously-flown MB-4 value, away from the AT-16 β wall.
**Chad's verdict carries a causal insight worth keeping:** "Now that the flight kernel was
given a more sufficient engine per weight ratio, the mouse aim and nose is responding
better without the need of so much rudder... it feels much better now to not have to chase
the mouse with so much rudder but now the plant is able to respond." — i.e. rung D's T/W
0.61 is *why* less rudder authority is needed: the airframe can now follow the aim with
lift instead of skidding onto it with yaw. Pre-agreed fallback rungs (NOT taken — symptom
resolved): `Cy_beta 2.5 → 1.5` if speed snap-back survived; `center_frac 0.0 → 0.3` if
crab-at-rest survived (⚠ that one walks back the Rung-M1 "nose in the MIDDLE" ruling and
was flagged as such). Walk-back: 2.2. Full ladder history:
`docs/cascade/rudder-coordination-ladder.md`.

**2. S-relorient: every freelook release fires the ORIENT verb.** Chad's ask: releasing
freelook should auto-orient (the double-tap behavior, automatic). Mechanism: on the
freelook release edge, the aim snaps to guarded velocity and the camera hard-cuts behind
the flight path — the same tested path the S-orient double-tap runs. Deliberate
exceptions: sub-stall/ballistic releases land on the nose (velocity lies there), and a
release while an override key is still held keeps legacy behavior (pilot is actively
maneuvering). Knob: `release_orient` in `[freelook]` — **optional-with-default-false in
the loader** (fixtures untouched, knob-off bit-identical legacy), `true` in the shipped
toml. Walk-back: one line, `release_orient = false`. The double-tap still works and is now
redundant; retiring it is an open question for Chad.

**Process notes worth preserving:** the plan-stage audit (this repo's session) caught a
real ordering defect — the snap must fire BEFORE the S7-hrz horizon-recovery capture so
the up-righting measures against the new forward on the same tick (release-orient
therefore rights the horizon slightly *better* than the double-tap did). The fresh-context
diff red-team came back SOUND-WITH-FIXES; its one real find (nothing pinned the fire as
one-shot — a re-fire-every-tick mutant survived the whole suite) was folded and
mutation-verified. 7 new test legs + 4 loader legs.

**Status:** LANDED and Chad-approved on `feel/kernel-v5`; in `reference/seads-feel/` as
of the 2026-07-28 re-snapshot; not yet reconciled to the game trees. Cascade entry:
`docs/cascade/freelook-orient-verbs.md`.

---

## 2026-07-28 — Auto-right quickening: `inverted_delay` 1.0 → 0.5 s ("3/3")

Same-day follow-up ask, flown and approved ("yes perfect as expected 3/3!" — the third of
three approvals that session). One TOML dial on the MB-right mechanism (Chad 2026-07-07:
"need to roll over on bank after about 2 s no gross inputs if belly up... slow roll off
ailerons"): the belly-up **rest timer** before the wings slow-roll upright halves;
`inverted_rate` stays 180°/s (the roll itself is unchanged, it just arms sooner). Landed
`e1684fdbb`, gate 380/380; verdict logged `cfe1bd7fe`.

Not a delicate change, and the reasoning is worth keeping: single dial, roll rate
untouched, and the scripted golden never dwells inverted so no goldens moved. One test
tripped **deliberately** — the "inverted plane at rest STAYS inverted" leg carries a
config-relative premise calibrated to the 1.0 s dial (`REQUIRE(window > 60)` ticks); at
0.5 s the inside-the-delay window is 48 ticks. That is the repo's designed tripwire for
exactly this kind of retune: the premise was re-derived honestly (floor 36 ticks, reasoning
in the comment) and the mutant it guards (un-gated wings-hold righting the plane) was
re-verified to die in the shorter window.

**Fly sentinel (standing):** a loop apex or slow roll where the hand rests a full half
second now auto-rights sooner. If it starts stealing inverted maneuvers, walk-back is
1.0, or 0.75 splits the difference.

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
