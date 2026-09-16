# R4a rung 2b — the erratic legs, attributed and fixed

**Branch** `sandbox/r4a-phase0` (worktree `D:/flight_sim2/seads-recon`). Not pushed.
**Launch the next session with:** read this file, then §7.

---

## 1. What Chad drove, and what it was

He ran the play build with `SEADS_BODY_BOARD=1` and reported, verbatim:

> "the legs were going all over erratically, broken"

Six gate legs were green. The suite and the drive disagreed, and the suite was
wrong.

**Root cause.** `render/body_chain.h` has stated since the table was measured
that the SEATED pose is illegal against the body chain's own seat keep-out — by
design, because he is sitting in it (pelvis 52.5 mm inside, thigh 38.6 mm). The
arming rung then made that same seated pin the **permanent attractor of the
return-to-pose spring** (`bin.pose_p[i] = pin[i]`). Spring pulls him into the
seat, constraint pass throws him out, Verlet reads every ejection as velocity —
every substep, forever. Measured on the shipped policy: the ankle station leaves
the drawn man by **0.49 m in one substep**, peaks at **1.21 m**, never settles.

`SEADS_BODY_BOARD=1` forces the blend weight to 1.0, so the drawn legs followed
that fight at full weight. **The flail predates rung 2** — it is the same thing
he reported on the arming drive as "flailing along mostly green". Rung 2 did not
create it; it made the legs show it.

**Why the gate could not see it.** All six legs graded the pure
`body_blend_legs` against a STATIC chain handed in as a fixture. The three
decisions that PRODUCE the chain — the pin/release branch, the spring's target,
the stiffness law — lived in `render/sled_model.cpp`, which CMake compiles only
into the `seads` executable. No test binary links that TU. The block's own
banner claimed it held "no arithmetic"; a branch on a stage weight, a choice of
spring target and a stiffness law are the stage machine.

---

## 2. What was built

| change | file | what it does |
|---|---|---|
| **the pose allowance** | `render/trail_chain.{h,cpp}` | `seat_escape_station` subtracts a per-station, per-face allowance. At the pose the push is exactly 0, so the spring has nothing to fight; deeper than the pose still pushes back — to the pose. |
| **the allowance, measured** | `render/body_drive.{h,cpp}` | `body_chain_seat_allowance` reads how deep the pose sits, using the SAME probes, frames and depth function the constraint pass runs. Re-run every frame. |
| **the policy, extracted** | `render/body_drive.{h,cpp}` | `body_chain_drive` now owns pin-or-release, the allowance, the stiffness rule and the step loop — **in the library, where ctest can reach it**. |
| **the wiring, reduced** | `render/sled_model.cpp` | gathers the rig and calls the drive. No decisions left in the exe-only TU. |
| **the LP settles** | `render/rider_load.{h,cpp}` | `rider_load_lp_step` snaps to exactly 0 below `kRiderLoadLpZeroFloor` (1e-4) on an exactly-zero target. |
| **per-side seat escape** | `render/body_blend.cpp` | the blend's escape now centres the limb box at the PROBE CENTRE carried through the frame, with world-axis support extents. It was centring on the joint and treating frame extents as world. |
| **per-limb grading** | `render/trail_chain.{h,cpp}`, `render/body_chain.cpp` | built, gated, **shipped OFF** — see §4. |

### The fix that was built and then rejected

Legalizing the spring target by **projecting** the seated pose out of the solid.
Measured cost: **pelvis moves 115 mm, worst leg station 73 mm** — a man sitting
above his own machine. The allowance buys the same peace for nothing.

---

## 3. The gate

`test/unit/test_body_drive.cpp`, four cases, all in the library.

1. **A released chain does not fight its own keep-out.** Two identical releases
   stepped in lockstep, differing only in whether the seat exists. Grades: it
   settles (last 0.5 s quiet); the keep-out may only hold him CLOSER to his
   pose, never further, while he is still at it; he stays inside 0.5 m.
2. **At arm 0 the chain is the drawn man, frozen** — bit-exact, `p_prev == p`.
3. **The seated pose is allowed, and then pushed nowhere** — the allowance is
   non-zero (he IS sitting in it) AND the keep-out then moves the pose by
   3.7e-9 m. Without the allowance this is 0.115 m at the pelvis.
4. **Leg stations graded per limb** — grades the mechanism on both settings.
   Deliberately does NOT assert which way `body_chain_fill` ships it (§4).

**Mutants killed on a fresh binary:** removing the allowance reproduces Chad's
drive (0.175 m ejection at a standstill, at arm = 0.01); reverting per-limb
grading fails the mechanism pin.

### Three of these legs failed honestly first, all the same way

Each forbade the mechanism instead of the defect:

- a peak per-substep bound forbade the 1 Hz spring's own gravity sag
  (g/ω² = 0.248 m);
- a two-sided seated-vs-seatless fork bound forbade the seat from HOLDING HIM
  UP, which is its job — the honest invariant is one-sided and scoped to while
  he is still at the pose;
- the straddle fixture put both probe boxes near the seat floor, where the cheap
  face for BOTH is straight down — the two rules agree there and the case proved
  nothing. **The fixture-no-op class, walked into again on the rung written to
  fix a gate that could not fail.**

---

## 4. ⚠ THE OPEN RULING — per-limb grading is OFF

Chad ruled per-limb grading for the leg stations on 2026-08-28. It is built and
gated and **not switched on**, and he re-ruled it that way after being shown
what follows.

He ruled it to kill a 0.4 m residual belonging to the **relocation** fix, which
was then abandoned. Against the allowance design it corrects nothing, and
turning it on costs: `test_body_chain` case 5 ("the DRAWN body stays out of the
seat") goes from **4.4 mm to 120 mm** of drawn-limb sink. Both limbs deep in the
solid each prefer their own lateral exit, those cancel, and the bone gets no
push at all — so the chain's keep-out stops being what holds his legs out of the
machine. Against his own words for this mechanism ("what must not enter the
machine is the DRAWN man"), that is not a trade to take unilaterally.

The straddle problem the ruling exists for IS handled — in
`render/body_blend.cpp`, per side, where the two limbs finally exist as separate
geometry. That is the half a single midline particle can never deliver.

**To re-arm:** flip one literal in `body_chain_fill` (`render/body_chain.cpp`),
which carries the whole story in its comment.

---

## 5. Open items

- **Stage 2's invisibility is not the chain's defect — RULED 2026-08-28, mine,
  and one line of Chad's overturns it.** He asked for a ruling rather than a
  question, so: the answer to "should the boots visibly float off the boards"
  is **yes, per §7.3's own words** — but the mechanism that owes it is NOT the
  body chain, and the chain is right to stay still.

  `rider_load.h` states the arm's law: the product `us * ub` is 1 **exactly at
  CASE F, "nothing below carries, which IS the body extends behind the
  anchor."** That sentence is §7.3's definition of **stage 3**. So `stage_arm`
  is stage 3's weight by construction, and the chain — stage 3's mechanism,
  "the scarf solver anchored at the grips" — correctly waits for stage 3's
  condition. Making it respond to board release alone would fork stage 2 into
  stage 3's weight and destroy the one property the arm was measured for.

  What §7.3 actually assigns stage 2 is a different body action: *"boots leave
  the running boards; leg IK releases from `board_socket`."* That is the **pose
  pass's rigid boot weld** to `board_socket_L/R` (`rider_pose.h`, D6/D7), and
  its signal already exists, already factored out, already gated:
  `rider_support_release(board_frac, board_ref)` — split out of the product in
  the arming rung precisely so stage 2 could read its own term without forking
  the model.

  **So this is a rung, not a flip:** blend the boot weld off by `ub`, in
  `rider_pose`, touching nothing in `body_drive` or `body_chain`. Small, but it
  is a change to the SIGNED R3 pose pass, so it is not being made inside a rung
  that is still awaiting his verdict. It goes after the drive.

  ⚠ The one claim here his eye can overturn: a man still carried by the seat,
  whose boards merely go light, has nothing lifting his feet — so the float
  should be **small and load-proportional**, not a visible kick.
- **The knee at full arm** is deflected 0.17 m further from its pose than it
  would be with no seat. Judged to be the seat stopping a sagging leg rather
  than fighting one, and the gate's excess invariant is scoped accordingly.
  Chad's eye rules on whether it reads right.
- **The ARM stations carry the same straddle defect** as the legs (grip ..
  upperarm are two limbs either side of the solid). Left alone deliberately:
  they are the bones the parked coat sink lives on.
- ~~**`render/body_chain.cpp` contains three raw NUL bytes**~~ — **CLOSED**
  2026-08-28, `b2919f820`. Fixed to `'\0'`; the file is text again, so content
  searches stop silently skipping the station table. Git had also stored the
  blob unnormalized (CRLF in the index, unlike every other `.cpp`), which is why
  that commit's diff is whole-file — the only content change is three literals.
  Gate re-run after: **1610/1615**, the five known reds, unchanged.
- **The coat sink on the spine** — the old parked defect, unchanged.

---

## 6. Drive card

Build: `cmake --build D:/flight_sim2/seads-recon/build-play --target seads`
Open: `D:\flight_sim2\seads-recon\build-play\seads.exe`

| env | what it does |
|---|---|
| `SEADS_BODY_BOARD=1` | ⚠ forces the **BLEND WEIGHT**, not the arm. The drawn legs follow the chain 100 % — but while `stage_arm <= 0` the chain IS the drawn man, so blending onto it is identity and **this switch alone can never show motion**. Chad drove it and reported "no body chain when I rode": correct behaviour, wrong card. |
| `SEADS_BODY_ARM=1` | forces `stage_arm` — **the only switch that makes the chain move.** Start here for anything you want to SEE. |
| `SEADS_BODY_CHAIN=1` | draws the chain and the graded surface boxes |
| `SEADS_BODY_BLEND=0` | the off switch, back to the signed R3 legs |
| `SEADS_BODY_ARM=x` | intermediate weights, for the stage-3 ramp without a jump |

The question is one sentence: **are the legs quiet now, and do they still trail
when the boards go light?** — and it takes TWO runs, because no single switch
answers both:

- `drive_body_board.bat` — the QUIET half. Nothing moving IS the pass.
- `drive_body_arm.bat` — the TRAIL half. `SEADS_BODY_ARM=1` is what releases
  the chain; you will be at full stage 3 while seated, which the game will
  never hold, so ignore that it looks odd.

---

## 7. Next

The torso rung (§8 of `docs/SESSION_HANDOFF_20260828_r4a_legs.md`) — but only
after Chad drives this. Nothing here has been seen moving by anyone.
