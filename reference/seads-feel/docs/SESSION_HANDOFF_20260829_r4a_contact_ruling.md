# R4a — the contact ruling: both selectors cured, and where superman actually lives

**LANDED ON MAIN AND PUSHED: `c8d88d325`** (merge of `sandbox/r4a-phase0`,
51 commits). The branch is pushed too, and has since advanced to `58dddf94c`
(the graph regeneration this rung owed).
**Worktree** `D:/flight_sim2/seads-recon`, checked out on `sandbox/r4a-phase0`
— ⚠ SHARED with the audio lane, which commits onto whatever branch is checked
out here. Leave it on the sandbox branch.
**Gate 1613/1618** — the five known reds. The merged tree was byte-identical to
the gated branch, so that number is the merge's number, not a re-run.
**Launch the next session with:** read this file, then §7.

---

## 0. READ THIS FIRST — THE MERGE ORDER, AND WHAT MAIN LOOKS LIKE NOW

Chad first ruled that `sandbox/snow` pushes to main before R4a, then reversed
it: **R4a went first, and it is done — `8745ba453` is on main.**

**✅ CLOSED: the snow lane has been told it can push** (Chad relayed it
directly, 2026-08-29; no peer session held a channel to them). So **expect main
to advance under you** with their 28 commits — R5 immersion, snow shading, the
rider breath and cast-shadow rows, and a G1 gyro/rotor-precession term in
`sim/sled.*` that ships defaulted to 0.

**⚠ WHEN YOU MERGE MAIN BACK IN, `render/sled_model.cpp` WILL CONFLICT ON
`sag0`.** They deleted a duplicate env-read lambda in favour of the shared
`render::sled_sag0_m()` (in `render/rider_pose.h`). **Keep the shared call; do
not restore the lambda.** That is their fix for the machine's mesh and its own
cast shadow disagreeing by 10 cm. Value and override behaviour are identical —
nothing measured moves. Expect mechanical conflicts too in `app/main.cpp`,
`render/draw.{h,cpp}`, `render/sled_model.{h,cpp}`, `CMakeLists.txt` and all of
`generated/graph/*`.

**Their gyro cannot reach this rung's mechanism, and that is not luck** — see
§8's last entry. They touch none of `rider_load`, `body_chain`, `body_drive`,
`trail_chain`, `body_blend`, nor `air_s` / `ground_contact` / `rolled_grace_s`.

What they were sent, all on main:

1. `docs/PACKET_TO_SNOW_R5_from_r4a.md` — the answers to their R5 asks. Their
   `rider_radius = 0.25 m` need not be unmeasured: `render/body_chain.*` has the
   clothed body per station (shoulder half-width 0.2460 m, waist 0.1278 m), so
   their rider shadow capsule is currently shoulder-width from neck to hips.
2. **Seated helmet crown = 1.574660 m**, generator
   `assets/character/sudburian_src/measure_helmet_crown.py`. All five baked dent
   variants agree to 0.0 mm, so "which dent is canonical" is not a ruling. Their
   1.25 m is legacy-derived and looks ~0.32 m short — but the packet gives them
   pelvis and shoulder anchors to reconcile the datum rather than a verdict,
   because their number's origin is unknown to us.
3. ⚠ That generator's FIRST run printed 1.673951 m, confidently, and it looked
   fine. **Wrong by 99 mm** — bind space, not posed. Tell them to re-run the
   generator, never paste the table.

**ONE CONFLICT HAS A KNOWN-RIGHT RESOLUTION.** In `render/sled_model.cpp` they
deleted a duplicate `sag0` env-read lambda and pointed it at the shared
`render::sled_sag0_m()` (added in `render/rider_pose.h`). **Keep the shared
call; do not restore the lambda.** That is the fix that stopped the machine's
mesh and its own cast shadow disagreeing by 10 cm. Value and override behaviour
are identical — nothing measured moves.

Other conflicts to expect, all mechanical: `app/main.cpp`,
`render/draw.{h,cpp}`, `render/sled_model.{h,cpp}`, `render/rider_pose.h`,
`CMakeLists.txt`, and all of `generated/graph/*` (both lanes regenerated).
**No mechanism-file overlap** — they touch none of `rider_load`, `body_chain`,
`body_drive`, `trail_chain`, `body_blend`; we touch none of their snow shading,
shadow casters or gyro.

**Baseline, resolved:** `sled_assist_reference_plane_is_load_weighted` and
`sled_debug_sink_is_write_only` are two of the four GI4 sled debts and have been
red on `sandbox/enemy-ai` for weeks. If they move after a merge it is real and
not merge noise. (Confirmed by the enemy-AI session directly.)

---

## 1. WHAT CHAD DROVE, AND WHAT HE SAID

Four drives, in order:

1. **Forced (`SEADS_BODY_ARM=1`)** — "too much flipping around makes it a mess",
   "the trigger is too sensitive", "always orange no green". **All three were my
   drive card**, not the build: the colour is a ramp on `stage_arm` which I had
   nailed to 1, and at arm 1 the spring runs at its softest.
2. **Honest, logged** — the data that overturned everything (§2).
3. **The contact ruling** — "the legs were quiet... **it went really well**",
   but "**no supermaning yet**".
4. **The superman bisect** — "legs moving flickery erratic a lot of the time, a
   few frames of supermanning, and **the orange frames doing all the flying**."
   He was describing two selectors, one cured and one not (§3).

**NOT SIGNED.** The quiet half is his; superman is not built (§4).

---

## 2. THE DEFECT, MEASURED

`rider_stage_arm` was a FRICTIONLESS PLANAR model — it knew only what pressed
straight into the seat and the boards, so it could not tell "leaned over on
snow" from "thrown into the air". Measured on 9079 unforced frames with the
contact fields plumbed:

| situation | arm > 0.1 | |
|---|---|---|
| ground, upright | 5.9 % | fine |
| ground, leaned 15-45 (ordinary banked trail) | **42.2 %** | wrong |
| ground, leaned 45-90 | 53.1 % | wrong |
| ground, tipped >90 | 0.3 % | right (his STAND ruling) |
| air, upright | **100 %** | right, and must not move |
| air, INVERTED | 10.2 %, **40.8 % pinned at 0** | wrong |

Two ends of one defect: at tilt the live support saturates at exactly zero
through a solver case boundary while its zero-accel reference is still 0.37, so
`1 - frac/ref` reads a vanishing denominator as total loss; and past 90 deg both
references degenerate, the guard returns 0, and the chain WELDS to his seated
pose in the crash the ladder exists for.

**★ THE FIX COULD NOT LIVE IN THE ARITHMETIC, AND THAT WAS MEASURED.** Two
absolute-loss reformulations were replayed over all 7372 frames of the first
log: both cured the leaning to 0.00 % **and killed genuine air** (median arm
1.000 -> 0.210). The RATIO is what makes air work — it is scale-free.

---

## 3. WHAT WAS BUILT — CHAD'S RULING, APPLIED TWICE

He ruled: **teach the selector ground contact.**

    arm = free_frac * (no support possible at this attitude ? 1 : ratio)

`free_frac = rider_air_free_frac(air_s, rolled_grace_s)` — the kernel's OWN
contact window (0.20 s), not a number invented here. `rider_stage_arm` is
UNTOUCHED and keeps its own tests; the new law composes it.

**Then the same ruling a second time, on the blend.** `board_release` — the
weight the DRAWN body follows the chain by — still had BOTH halves of the same
defect. It crossed the visible band **1.97 times a second** (the flicker), and
its degenerate guard meant that on the **485 frames Chad spent inverted and
airborne, his legs followed the chain on ZERO of them.** The body could be
released and drawn as seated at the same instant. That was the missing superman.

Replayed over the same 9079 frames:

| | before | after |
|---|---|---|
| ground rows (all) | 4-53 % | **0.00 %** |
| air, upright | 100 % | **100 %** (untouched) |
| air, INVERTED | 0 % | **100 %** |
| superman-capable (both weights on) | 3.55 % | **10.76 %** |
| chain flying, legs planted | 7.21 % | **0.00 %** |
| blend flicker | 1.97 /s | **0.21 /s** |

Whole drive 18.5 % -> 12.0 %: **the trigger did not get rarer, it got right.**

Commits: `f31c46872` (stage weight), `1fef76ea1` (red-team fixes),
`3b72ca4f7` (blend weight).

---

## 4. ★★★ THE NEXT RUNG: NOTHING PULLS HIM INTO SUPERMAN

Drive 3, 19219 honest frames. **The trigger is correct** — green on the ground,
orange on the jump, `arm > 0.9` on 5.26 %, airborne 8.02 %. On his five big
sends (2.4-2.75 s of air) **`arm` sat at 1.000 and `brel` at 0.97+ for 144
consecutive frames.** Both weights fully on.

And still: "a bit of movement from the chain, not drawn in character yet."

**So it is a THIRD thing, downstream of both weights.** In free fall the machine
and his body fall together, so there is no relative force to pull the chain off
his pose — the return spring simply holds it where it was. What little motion he
saw is rotation only.

**Superman needs a term that does not exist yet.** The obvious place to look
first: `render/trail_chain.h` already carries `wind_mps` and `drag_k_per_m`
(0.153/m, `tan(theta) = drag_k * v^2 / g`) — that is how the SCARF trails. Check
whether the body chain is simply never fed the relative wind the scarf gets.
**Do not assume it; read `TrailChainInput` at the body's call site and see what
is actually passed.** If the wind is already there and the trail still is not,
the question is a different one and this section is wrong.

---

## 5. OPEN ITEMS

- **The ground-ejection narrowing, HIS TO CONFIRM.** The legs can no longer
  release at ALL while the machine touches — and `air_s` also resets on CG
  proximity (1.2x cg height), so **a throw over the bars where the machine stays
  down can never arm stage 3.** He chose "contact blend" over "require air", but
  the kernel's contact signal is binary so the two collapsed into one. Told to
  him; not yet ruled on.
- **The `[sled_comfort]` grace dial now sets two things** — the roll readout AND
  how fast the rider's body frees up in the air. A second-consumer note stands
  at the dial (`sim/sled.h`). ⚠ Its documented OFF config is "grace HUGE", which
  would make the legs never trail again, silently, with no red test.
- **Stage 2's boot weld** — RULED but not built: stage 2 should visibly lift the
  boots, via `rider_support_release` on the pose pass's `board_socket` weld. It
  is a change to the SIGNED R3 pose pass, so it waits.
- **The knee at full arm** is 0.17 m further from its pose than with no seat.
  His eye rules.
- **Per-limb leg grading** built, gated, SHIPPED OFF (`body_chain_fill`, one
  literal). Turning it on costs `test_body_chain` case 5 4.4 mm -> 120 mm.
- **The coat sink on the spine** — the old parked defect.
- **Spawning already airborne is untested.** `SEADS_SPAWN_ALT` puts the machine
  in the air at seed, where `air_s` grows and the new law arms superman
  immediately. That is arguably correct — he IS airborne — but nobody has
  looked at it, and it is the one domain of the contact law with no evidence
  behind it. Every other edge is gated: negative air time clamps to 0, an
  absent window falls back to binary rather than to "never free", a degenerate
  reference off the ground reads as total freedom and on the ground is
  multiplied away, and an invalid load solve forces the arm to 0.
- **The snow lane's rider-shadow taper** — they have the measured widths
  (shoulder 0.2460 m, waist ~0.13) and the measured helmet crown (1.574660 m,
  `docs/PACKET_TO_SNOW_R5_from_r4a.md`), and deferred baking them: a capsule has
  one radius, so a taper needs a second capsule, budget 9 -> 10 of 16, which
  eats room enemy aircraft need. **Chad's call, on a row he has not flown.**

---

## 6. DRIVE CARD

Build: `cmake --build D:/flight_sim2/seads-recon/build-play --target seads`
Open: `D:\flight_sim2\seads-recon\build-play\seads.exe`

| bat | what it is |
|---|---|
| `drive_body_log.bat` | **the honest run.** Nothing forced, clears stale switches, `SEADS_BODY_CHAIN=2`, logs to `%TEMP%\body_chain_drive2.log` |
| `drive_body_superman.bat` | the BISECT: chain forced, blend REAL. Says out loud that it proves nothing about the trigger |
| `drive_body_board.bat` / `drive_body_arm.bat` | older A/B cards |

Colour: **GREEN = welded to his pose, ORANGE = released.** Green on the ground
and orange in the air is the mechanism working.

⚠ **`SEADS_BODY_ARM` forces `stage_arm`; `SEADS_BODY_BOARD` forces only the
BLEND.** Mislabelling those cost two drives.

---

## 7. NEXT

1. **§4 — find out why the chain does not trail in free fall.** Read
   `TrailChainInput` at the body's call site before forming any theory.
2. If snow has landed, merge main in first and take the `sag0` resolution in
   §0. Re-gate before believing anything: their branch changes `sim/sled.*`,
   and two of our five baseline reds live in the sled tests.

---

## 8. THE LESSONS, AND THEY ARE NEARLY ALL ONE LESSON

- **★★★ A FORCED BUILD CANNOT ANSWER A QUESTION ABOUT THE THING IT FORCES.** I
  wrote a card that nailed the trigger open and then asked him to judge the
  trigger. He reported the forcing as a defect, correctly.
- **★★★ WHEN YOU CURE A LAW, GREP FOR EVERY OTHER CONSUMER OF THE SAME BROKEN
  SHAPE.** The blend weight had the identical defect. It was flagged to me and I
  filed it as a future rung's problem. It was live, and it was the whole reason
  the rung looked unfinished.
- **★★ A HEADER THAT DOCUMENTS A LAW WITHOUT ITS CLAMP WILL MIS-TEACH ITS OWN
  AUDITOR.** `body_drive.h` gave `base/max(arm,1e-3)` without the 9.55 Hz
  stability clamp, so I inherited "stiffness runs to 1000 Hz" into my own
  premise. Every arm below ~0.1 is visually pinned.
- **★★ THE FIXTURE-NO-OP, THIRD APPEARANCE ON THIS RUNG.** My first gate pinned
  continuity only at `frac = 0` — the one input where both branches agree by
  construction. 18 of 41 assertions could not fail.
- **★★ POLICY IN AN EXE-ONLY TU, ON THE RUNG THAT CURED EXACTLY THAT.** Four
  lines of `free_frac` derivation went into `sled_model.cpp`, which no test
  links. Now in the library with its own case.
- **★ MEASURE IN THE POSE.** `measure_helmet_crown.py`'s first run printed a
  confident crown height that was **99 mm wrong**, because `Scene.tris()` returns
  raw BIND vertices for a skinned node. `measure_neck_ceiling_posed.py` already
  exists in this tree because Chad caught that same mistake once.
- **★ THE GRAPH IS PART OF THE CHANGE, NOT PAPERWORK AFTER IT.** I added three
  library functions across this rung and regenerated `generated/graph/` in none
  of those commits, so for the length of the rung the graph described a
  `rider_load.h` that no longer existed — and CLAUDE.md tells the next session
  to query the graph INSTEAD of grepping. Fixed in `58dddf94c`, but it should
  never have been a separate commit.
- **⚠ I AMENDED A COMMIT THAT WAS ALREADY PUSHED**, reaching for `--amend` to
  fold the graph in. Caught immediately (`git status` against
  `origin/sandbox/r4a-phase0`), reset back to the pushed hash, and recommitted
  on top; main and the remote were never touched. On a branch other people
  build on, `--amend` after a push is a rewrite, not a tidy-up.
- **★★★ GATE ON WHAT THE OTHER LANE CANNOT CHANGE.** Chad's first instinct was
  to key the trigger on velocity and force. The snow lane's branch adds a gyro
  roll term — which would have moved an attitude- or acceleration-keyed trigger
  silently, in a way neither lane would have connected. Contact is a kernel
  fact; attitude is downstream of anything that touches torque.
