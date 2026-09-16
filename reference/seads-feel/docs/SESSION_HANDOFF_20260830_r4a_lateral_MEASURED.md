# R4a — THE LANDING RELEASE: THE LATERAL IS MEASURED, AND TWO THINGS BLOCK THE BUILD

**Session 2026-08-30, `D:\seads_sandboxes\r4a-superman`, branch `sandbox/r4a-grip`.**
Launch doc `docs/SESSION_HANDOFF_20260830_r4a_grip_release.md` is still the spec; this
is its first session's report. Commits `97383d61f`, `0f90dd6ba`, NOT pushed.

---

## §1 FIRST, THE TREE DID NOT BUILD — AND THE GATE COULD NOT SEE IT

The branch was handed over at `fa8584bdc` with a published gate of **1731/1737**. The
`seads` target had **never compiled** on it:

    app/main.cpp:6017:18: error: 'struct render::FrameInfo' has no member
    named 'cosmetic_projectiles'

`86b4bf4e1` (the merge of main's enemy-AI energy envelope) took **main's**
`app/main.cpp`, which assigns the S3-GUNS cosmetic tracer pool, alongside **our**
`render/draw.h` and `render/draw.cpp`, which never had it — that feature landed in
`1aa27ed8f`, after this branch forked. Git merged the two halves with no conflict and
they disagree. The merge commit's own message says, truthfully, that there was no
conflict in `app/`.

**No test TU links `app/main.cpp`, so CTEST was structurally blind to it and every leg
stayed green.** ★★★ A ctest count certifies that the TESTS built, never that the GAME
did.

★ AND THE HARNESS WOULD HAVE CAUGHT IT: `.claude/hooks/gate.sh` runs a plain
`cmake --build build --config Debug &&` before ctest, which builds every target
including `seads`. So the published 1731/1737 was a ctest count taken WITHOUT the
gate — a hand-run build and a hand-run ctest, in place of the one command that is the
gate. The trap that makes this easy: `cmake --build build -j 12 | tail -5` reports the
TAIL's exit code, not the compiler's, so a hand-run build says nothing while printing a
wall of green warnings. I hit exactly that on my own first build this session before
reading the code. **Run `.claude/hooks/gate.sh`. When you must run the steps by hand,
read the build's own `$?` before you believe the ctest number.**

FIXED in `97383d61f` by restoring main's copy of the two files, after verifying that
our branch's ONLY divergence in them was the missing feature (1 hunk in `draw.h`, 3 in
`draw.cpp`, all the same block) — so nothing of ours was reverted.

**BASELINE, RE-ESTABLISHED HONESTLY: `seads` builds, gate 1731/1737, the six documented
reds BY NAME** — probe P-F, `sled_slides_before_it_tips`, `sled_grip_ceiling`,
`sled_assist_reference_plane`, `sled_debug_sink`, and E12.1 (inherited from main).

⚠ `origin/main` is now **9 commits ahead** of this branch (a repo-wide LF pin in
`.gitattributes`, `LANES.toml`, the nightly-gate and red-SET baseline, the snow lane's
R6 notes). Deliberately NOT merged this session: a merge is exactly what broke the
tree, and the LF pin interacts with the CRLF trap this thread has already paid for.
Merge it as its own commit, and build the `seads` target explicitly afterwards.

---

## §2 THE LATERAL — MEASURED BEFORE IT WAS DESIGNED, AS ORDERED

`0f90dd6ba`, instrument only: nothing shipped is touched, no tape is touched.

`dmag` was a mean of **lengths** and the question is about a **direction**, so the same
station displacements are now also averaged as **vectors** on the body axes (+x right,
+y up, +z aft — the whole rig was already body-frame, so no transform is involved and
none can be got wrong), carried through each airborne window to touchdown, and graded
against four candidate drivers of a side. Every agreement is printed **beside its own
lagged-window control**, per the noise-floor law: 50 % is no information, and an arm
that does not beat its control has found nothing.

**Corpus: 45 distinct drives by md5 (157 tape files now — it was 116 files / 44 drives),
84 airborne windows, at the SHIPPED dials 0.02 / 3.0.**

### What is true of the whole corpus

| | |
|---|---|
| lateral as a share of the touchdown excursion | **p50 0.27**, p90 0.84, max 0.95 |
| windows over 10 % lateral | 34 of 84 |
| the side survives peak → touchdown | **43 of 47 (91.5 %)** |
| side predicted by the buck's own lateral push | **66.0 %** (n 47) vs **51.1 %** control |
| side predicted by roll rate at touchdown | **53.3 % (n 45) vs 44.2 % control** — see the corrections |
| side predicted by his own lean | 34.0 % — i.e. 66 % to the side OPPOSITE his lean |

The launch doc's "nothing in the chain does lateral yet" is **false for the render
chain**: the four pseudo-force terms already make a side, it is coherent, and it is
weakly driven by the bump itself. Lean and the buck's lateral push are not two findings
— they are one axis with opposite signs (they disagree 74 % of the time).

### ★★★ AND IN THE WINDOWS THAT ACTUALLY BREAK THE GRIP, IT IS NOT THERE

Above the capacity of 57.4, the median touchdown excursion is

    UP  1.25 m      |AFT|  0.45 m      LATERAL  0.067 m

and **no driver separates from a coin at n = 8**.

★★★ **THREE CORRECTIONS TO THIS BLOCK FROM THE FRESH-CONTEXT RED-TEAM — AND ONE OF
THEM INVERTS A CONCLUSION.**

1. **HE IS NOT GOING BACK. HE IS MOSTLY GOING FORWARD.** The 0.45 m was a median of
   |aft|. The signed values in those 8 windows are `-0.142 +0.529 -0.716 -0.166
   -0.709 +0.889 -0.136 -0.449` — **6 of 8 NEGATIVE, which is forward, over the
   bars.** Corpus-wide the sign is mostly aft (drag streaming him back); it **flips
   in exactly the windows that break the grip**, which is what a landing should do —
   the machine stops and he keeps going. "Up and back" was wrong, and it was written
   into the ladder as spec.
2. **THE ROLL ROW WAS SIGN-INVERTED.** Roll-right is NEGATIVE ω_z (CLAUDE.md) and the
   probe negates it; the published 46.7 % / 55.8 % is the UN-negated driver.
   Corrected: **53.3 % against a 44.2 % control — +9 points, the second-strongest
   driver measured**, behind the buck's lateral push (+14.9). Not significant at
   n≈45, but the summary's DIRECTION was inverted on the very option that was also
   flagged as the most legible on screen.
3. **"4 % OF IT" DIVIDED BY AN AXIS THAT CANNOT CARRY A SIDE.** `up` in this rig is
   `mean(L)·(1−cosθ)` — a rectified rotation measure, **never negative in any of the
   84 windows**. Against the HORIZONTAL excursion the lateral share is **~20 % in the
   release windows** (59 % corpus-wide), not 4 %. The conclusion survives — 20 % of
   azimuth is still weak — but it was overstated about 5×.

**So: when he lets go he is going UP, forward as often as back, with a weak but real
sideways component.**

### ★★★ AND THE WHOLE FINDING WAS GRADED ON THE WRONG WINDOWS — RE-MEASURED

The red-team's sharpest hit: these 8 windows were selected by the CHAIN's product
> 57.4, and the shipped kernel selects a different set. Re-measured on the 10 windows
above the kernel's own p90 load, at substep resolution:

| | chain-selected (n 8) | **kernel-selected (n 10)** |
|---|---|---|
| lateral share of the HORIZONTAL departure | 0.204 | **0.220** |
| goes FORWARD, not aft | 6 of 8 | **8 of 10** |
| side predicted by the buck's lateral push | 4 of 8 | **8 of 10** |
| overlap between the two sets | — | 6 of 10 |

**This partly RESCUES the finding rather than killing it.** The sets differ, but the
lateral share is the same either way (~0.21 of horizontal), the forward departure gets
STRONGER, and the buck's lateral push predicts the side **8 of 10** on the mechanism's
own windows — where the first pass reported "no driver beats a coin" at n = 8.

So "the release cannot inherit a side" was **too strong**. It can inherit a weak one,
the buck picks it, and Chad's ruling (a) — amplify the lateral he already has — is
better supported now than when he made it.

★ **AND IT IS NOT AN AVERAGING ARTEFACT, WHICH WAS CHECKED AND NOT ASSUMED.** The
vector mean CAN cancel where the mean-of-lengths cannot, so a small lateral could have
been a chain thrashing symmetrically. It is not: `|vector mean| / mean-of-lengths` is
**0.991 at the median** (0.87 at p10) in the release windows — the stations are moving
together, and the direction they are moving together in is up and aft.

★ **THE RELEASE CANNOT INHERIT ITS SIDE. IT HAS TO BE GIVEN ONE** — and which side that
is is a felt call, so it is Chad's, not a coin this rig is entitled to flip for him.

★ **CAPACITY 57.4 RE-CONFIRMED ON THE GROWN CORPUS**: 8 of 84 windows exceed it —
**9.5 %** against his "maybe 10 % of jumps". Still true by construction, not by tuning.

Reproduce with:

    SEADS_PROBE_CSV=1 SEADS_GRIP_GAIN=0.0669 ./build/seads_sled_probe.exe superman <tape>

17 CSV fields after `path`; the human-readable table still stops at 12 rows, so any
corpus claim uses the CSV. ⚠ The earlier recipe here named `SEADS_BUCK_GAIN` /
`SEADS_BUCK_DECAY` and omitted `SEADS_GRIP_GAIN`, which emits the two kernel columns
as zeros. The buck pair now DEFAULTS to the shipped, signed 0.02 / 3.0 in the probe
(it used to default OFF while the game ships ON, so the rig's two arms silently ran on
different memories), and `SEADS_GRIP_GAIN` is what arms the kernel's own grip.

---

## §3 THE TWO QUESTIONS THAT BLOCK THE BUILD

### Q1 — FOR CHAD, AND IT IS A FELT CALL: WHICH SIDE?

He said "falling off to the side". The measurement says the release windows carry no
usable side of their own, so one of these has to be ruled:

- **(a) AMPLIFY THE SIDE THAT IS ALREADY THERE.** Whatever lateral the chain has at
  touchdown picks the direction and the release multiplies it. Honest — it is driven by
  his real bump — and it is coherent 91.5 % of the time. But it is 0.067 m in the
  windows that matter, so the gain would be doing most of the work.
- **(b) THE SIDE THE BUMP PUSHED HIM.** The field's lateral at the buck: the strongest
  thing measured (66 % vs a 51 % control) and *mostly the mirror of his own lean* — he
  goes over the outside, away from where he was hanging.
- **(c) THE MACHINE'S ROLL AT TOUCHDOWN.** Reads as pure noise here (46.7 % vs 55.8 %),
  but it is the most legible on screen: he goes off the low side.
- **(d) DOWNHILL — THE TERRAIN'S SIDE.** Not measured yet; cheap to add.

My recommendation is **(b)**, with (a) as its magnitude: the bump chose the side when it
threw him, and the bump is the initiator his own ruling names. But this is his eye, not
my table.

### Q2 — ARCHITECTURAL, AND IT IS A CONFLICT WITH §7.7 AS WRITTEN

LADDER §7.7 puts `grip_load_n` in `SledState`, **taped**. The calibrated equation is

    grip_load = landing hardness × EXTENSION FROM THE POSE AT TOUCHDOWN

and the extension factor is **the render chain's displacement**, which the kernel cannot
see: `sim/` may not import `render/` (hard rule), and a taped kernel field driven by a
render solve could never replay — the probe replays the kernel with no render at all.
So one of:

- **(A) THE KERNEL GROWS ITS OWN EXTENSION.** §7.7 already gestures at it: `rider_up_m`
  "gains a *physical* driver (machine vertical accel)". The release then reads a
  kernel-visible unseating, and render keeps the chain that DRAWS it. This keeps the
  direction of authority right — the machine is never moved by an animation.
  **Recommended.**
- **(B) THE RELEASE STAYS RENDER-SIDE** and only its CONSEQUENCE is kernel — the 87.5 kg
  leaving, `cg_off` and `patch_geometry` changing. Cheaper, but it makes a physical
  event depend on a render solve, which is the firewall inverted.

Two riders on Q2, both measured, both real:

1. ⚠ **THE TAPE PIN ROSTER IS POSITIONAL — 41 doubles, read by index.** `sim/sled.h`
   says so twice, and both `right_assist_*` and `ws_exch_l` were deliberately kept OUT
   of it for exactly this reason: adding a pinned field **refuses every existing
   `.sledtape`**, i.e. the 45-drive corpus this rung's capacity was calibrated on. New
   kernel state must be derived and non-pinned, or the corpus dies.
2. ⚠ **`grip_load_n` IS NOT NEWTONS.** hardness × extension has units of m²/s², and the
   capacity 57.4 lives in those units. Meanwhile `probe_grip_hold` already computes a
   genuine rod-model grip force in real Newtons (hundreds of N). Shipping the product
   under a `_n` name puts two different quantities behind one word — a lying instrument
   by this repo's own definition. Either drop the `_n`, or convert deliberately and
   re-calibrate. Not silently.

---

## §4 CHAD RULED BOTH, 2026-08-30 — AND STAGE 1 IS BUILT ON HIS RULING

**Q1 = "whichever way he's already leaning out."** The release amplifies the
lateral the body already has at touchdown; it does not invent a side and does not
read the machine's roll. ⚠ And §2 says that side is 0.067 m in the windows that
matter, so the GAIN does most of the work and is a felt call — it gets driven, not
tuned to a table.

**Q2 = "the kernel grows its own extension."** Built as `sim/rider_grip.{h,cpp}`.

### What landed

- **The arming memory MOVED DOWN into `sim/`, verbatim, and is re-exported from
  `render/body_drive.h` under its old names.** One implementation now feeds the
  drawn body, the kernel and the probe; every existing call site and test compiles
  unchanged. The move is byte-for-byte because what Chad signed was measured
  through those exact float operations.
- **`sim::GripState` on `SledState`**: the memory, a scalar extension, the load
  (hardness × extension) and the one-way `attached` latch. **Not in the positional
  pin roster** — it re-converges from taped inputs alone — and **OFF-by-absence in
  the tape loader**, prophylactically, before it can ever matter.
- **The capacity ships at 1e30 — it cannot break.** That is §7.7's stage 1, and it
  is executed rather than promised by `sled_the_grip_law_moves_no_golden`: two
  machines differing only in the grip dials, one of which DOES release during the
  run, driven 20 s over identical ground, every dynamic field bit-identical.

### ★ THE GAIN IS MEASURED — AND THE KERNEL'S SCALAR REALLY IS THE CHAIN

`unseat_gain` is calibrated against the drawn chain over the same 45 drives, both
quantities read at the same touchdown:

| gain | kernel ext p90 | chain d_land p90 | ratio | rank corr |
|---|---|---|---|---|
| 0.001 | 0.0123 | 0.8222 | 0.015 | +0.948 |
| 0.01 | 0.1230 | 0.8222 | 0.150 | +0.958 |
| 0.03 | 0.3689 | 0.8222 | 0.449 | +0.966 |
| **0.0669** | **0.8226** | **0.8222** | **1.000** | **+0.965** |

The **shape was already right and only the scale was free**: the rank correlation
is ~+0.95 at every gain, and extension is exactly linear in the dial. One scalar
tracks a five-station solve's order of excursions at +0.965.

### ⚠ AND THE CAPACITY IS 78.34, NOT 57.4 AND NOT 75.72

57.4 is the p90 of the PROBE's windowed CHAIN product. The kernel's own load, read at
the SUBSTEP resolution its latch fires at, has p90 **78.34** (p50 4.13, p99 427).
**78.34 is what stage 2 dials in**, after he drives it.

⚠ The first number published was **75.72**, and it was read off the per-TICK state the
replay hands back — a 1-in-12 subsample of a quantity whose whole meaning is its peaks,
against the probe's own written warning not to read a threshold there. The kernel now
writes the load into `SledDebugSubstep` and the probe reads that. The correction is
small (+3.5 %); the method was wrong either way, and a threshold read off a subsample
is right only by luck.

### Mutation-verified, by running them

Five mutations, five reds: kernel reads `attached` (caught only after the leg was
strengthened — see below), the memory stops softening the pull-back, the charge
ignores its gain, the product becomes a sum, the latch re-attaches.

★★★ **THE FIRST DRAFT OF THE INERTNESS LEG PASSED THE FIRST MUTATION.** Its live
arm had capacity 1.0 and never actually released on a flat ball, so it compared two
machines whose mechanism had barely run. The fix was to the LEG — capacity 0 and
`REQUIRE_FALSE(attached)` — not to the mutation. An A/B that proves equality must
prove its two arms differed first.

---

## §5 WHAT IS BUILT, WHAT IS NOT

- BUILT: the honest baseline (§1), the lateral instrument (§2), and §7.7 STAGE 1
  (§4) — the kernel-side grip, live, calibrated, and unable to fire.
- NOT BUILT, deliberately: **the release itself.** Nothing reads `attached`; the
  87.5 kg never leaves the machine; there is no lateral throw and no tumble. That
  is stage 2, it moves tapes, and it is Chad's drive.
- **THE FRESH-CONTEXT RED-TEAM RAN, 2026-08-31 (two lenses, both told to REFUTE),
  and its findings are folded in.** What SURVIVED: stage-1 inertness in the committed
  code (they caught two of their own mutations red), the verbatim `buck_step` move
  (diff-checked byte for byte, float-for-float, same operator set), the body-frame
  decomposition and the `rider_lat_m` sign, the lagged control, the n=8 hedging, and
  every corpus figure in §2 — reproduced on a corpus they rebuilt themselves.

  WHAT THEY BROKE, AND WHAT WAS DONE ABOUT IT:

  | finding | fix |
  |---|---|
  | The OFF-by-absence lines made the grip dials **unrecordable** — no roster entry, so no tape could ever turn them back on, and `dial_gap` (COMFORT-only) could never name them | dials added to `SLEDTAPE_PARAMS_D`; the gap now covers the params side too; the comment that said this "stops mattering at stage 2" is corrected — that is when it STARTS |
  | `dynamics_identical` compared **10 of 41** pinned doubles while claiming "every dynamic field"; a mutation zeroing `assist_nm`/`air_s` on release survived | the leg now expands `SLEDTAPE_PIN_D` itself, so it cannot fall behind the roster. Mutation re-run: **caught** |
  | The "replays from taped inputs alone" leg was a test that could not fail, and `attached` is a one-way latch that CANNOT re-converge | leg renamed to what it proves; `app/main.cpp`'s KEY_R autoright now zeroes `sled.grip` beside `ws_exch_l` — inert today, load-bearing at stage 2 |
  | Two mutations survived: the memory's **discharge-before-peak-hold order** (which the law argues for at length) and the negative-extension clamp | two new legs; both mutations re-run and **caught** |
  | The capacity was read off a **tick subsample** of a peak-valued quantity | the kernel writes the grip into `SledDebugSubstep`; capacity re-derived at substep resolution: **78.34** |
  | The probe armed the kernel's memory but not the chain's, so the rig's two arms ran on different memories | both arms read one pair, defaulting to the shipped 0.02 / 3.0 |
  | The four rank correlations in the gain sweep are **one statistic printed four times** (rank order is invariant under a positive scalar) | recorded as such in `sim/rider_grip.h`; the claim no longer leans on them |
  | `-9.81` typed in the grip block while the kernel's gravity is `9.80665`, making "in free fall it is ZERO" false | single-sourced |
  | The "seated man" leg passed `dt = 0` — a zero-time step, not a seated rider | rewritten to a real seated case vs a stretched one |
  | "the kernel's and the drawn body's copy see the same history" — **false**: render steps the memory once per FRAME, the kernel per SUBSTEP | corrected in `sim/rider_grip.cpp`; shared function is not shared history |

- **NOTHING FROM THE RED-TEAM IS OUTSTANDING.** Stage 1 is built, gated,
  mutation-verified, independently reviewed, and its review's findings are fixed and
  re-verified.
- Stage 2's shape, in order: dial `capacity` to **75.72**, wire `attached` into
  `cg_off` / `patch_geometry`, give the departure Chad's ruled side (amplify the
  lateral he already has), re-tape, and hand him a drive.
