# HANDOFF — START HERE. R4a RUNG 2: THE DRAWN LEGS FOLLOW THE CHAIN.

> Launch line for the next session:
> **"Read `docs/SESSION_HANDOFF_20260828_r4a_legs.md`; do §8."**

Prior handoff: `docs/SESSION_HANDOFF_20260827_r4a_arming.md`. Its §7 set this
rung; its §3 deliberately deferred this half. This is that half, cut down to
the legs by Chad's ruling.

---

## 0. THE STATE

| branch | state |
|---|---|
| `sandbox/r4a-phase0` | this rung. Committed, **NOT pushed**. |
| `main` | unchanged. |

**The drawn rider now moves.** Until this rung the body chain was computed and
debug-drawn only; nothing the game drew was chain-driven. Now, when the running
boards go light, the drawn legs leave them and follow the chain. Everything
above the hips is still the signed R3 pose.

---

## 1. THE TWO RULINGS THIS RUNG WAS BUILT ON (Chad, 2026-08-28)

1. **Boots release on BOARD LOAD ALONE** — `LADDER §7.3` stage 2's own
   condition, not the combined stage weight. The draft keyed them on
   `stage_arm`, which is the PRODUCT of seat and board freedom: braking that
   unloaded the seat while planting the boards would have peeled his boots.
2. **LEGS ONLY.** Pelvis, spine, shoulder and arms stay R3. Stations 6..10 are
   exactly the bones the attributed-and-parked **coat sink** lives on, and
   re-posing them by a new rule in the same rung would put a known defect and a
   new mechanism in front of him at once. The torso is rung 3.

---

## 2. ★★★ WHAT THE TWO CONSULT ROUNDS CAUGHT, AND IT IS THE STORY OF THIS RUNG

Both rounds returned **UNSOUND**. Nothing was built until the third plan. Every
finding below was re-derived from source before it was acted on.

### 2.1 ROUND 1 — the gate could not have run

Five of the draft's six gate legs lived in `render/sled_model.cpp`, which
`CMakeLists.txt` compiles **only into the `seads` executable** — never into
`seads_render_core`, which is what `seads_tests` links. **No ctest can execute
one line of that file.** It is the `.blend` re-export lesson verbatim: a gate
that never compiled the thing it graded.

That is why `render/body_blend.*` exists and why `sled_model.cpp` got nothing
but gather / call / solve / settle.

Round 1 also killed the draft's placement rule. It put each leg's IK target at
the station's **probe centre** — but `measure_body_chain.py` builds probes from
*"the CPU-skinned `sudburian_proxy` vertices themselves"*, so a probe centre is
a **flesh-lobe centroid, not a joint**. Measured error: **11.6 mm at the thigh,
9.2 mm at the knee** (probe u 0.1933 vs joint 0.1817; 0.2592 vs 0.2684).

### 2.2 ★★★ ROUND 2 — the draft shipped Chad's own drive-found bug back in

The fix for stage 2 was `board_frac / board_frac_rest` — **the frozen at-rest
split.** That is the pre-§8.1 formula. The supports are frictionless and fall
as `cos(roll)`, so against a frozen upright reference **60° of roll on a PARKED
machine reads board_frac 0.1656 against a rest 0.3312 and peels the boots half
way in the driveway.**

Chad's first drive found exactly that defect on the stage weight ("it goes to
orange even if tipped over"), and §8.1 fixed it by making the reference LIVE.
The draft re-introduced it on a different signal, where the existing gate case
could not see it — case 8 grades `stage_arm`, not the boot release.

★ **The lesson, and it is a new one for this program's ledger: a fix is
attached to a SIGNAL, not to a codebase.** The live-reference discipline was
paid for once and then not carried across to the second consumer of the same
model. What stops the next instance is not vigilance — it is that
`rider_support_release` is now the ONE expression both stages read.

Round 2 also caught that the draft's foot pin, copied from `pose_pass:2226`
"exactly as it already does", writes `foot_w` — **whose translation is the
board weld** in the ordinary path; it only becomes the IK target inside the
weight-shift branch. Copying it verbatim would have welded the boot back onto
the running board while the thigh and calf trailed: the one thing this rung
exists to stop.

And it caught the gate's own reversibility leg as a **test that cannot fail** —
round 1 recommended it, the draft adopted it, and the pure function is
stateless, so an up-sweep/down-sweep over it is bit-identical by construction.
Dropped.

---

## 3. WHAT LANDED

| file | what |
|---|---|
| `assets/character/.../measure_body_chain.py` | + emits **per-side JOINT offsets** for the leg stations. It already had both sides — every station is a named L/R pair and the midpoint threw the offset away. |
| `render/body_chain.{h,cpp}` | + the measured offset table, `body_chain_leg_joints()`, `body_chain_station_of()` (named lookup — no caller hard-codes 12 or 15). Header comment fixed: it named `render::probe_axes`, which **does not exist**; the frame builder is `chain_axes`. |
| `render/rider_load.{h,cpp}` | `rider_support_release()` factored out; `rider_stage_arm` is now its product. **Bit-identical** — the term returns `double` so the product is still formed in double and cast once. |
| `render/trail_chain.{h,cpp}` | + `trail_seat_escape()`, a pure wrapper over the SAME `effective_seat` + face scoring the chain's keep-out runs. Additive; nothing shipped calls it. |
| `render/body_blend.{h,cpp}` | **NEW, PURE, in `seads_render_core`.** The whole mechanism. |
| `render/sled_model.cpp` | the wiring, and nothing else. |
| `test/unit/test_body_blend.cpp` | the gate, 6 cases / 190 assertions. |

### 3.1 THE MECHANISM

```
  pose_pass(...)            <- UNCHANGED. The R3 drawn man.
  body_pin_eval(...)        <- the pin AND the spring target. FROM HERE ONLY.
  trail_chain_step(...)     <- UNCHANGED.
  === the leg blend: frames, blend, re-solve the two legs, re-settle ===
  the scarf block           <- untouched
  SKIN
```

The slot is load-bearing in both directions: after the chain so the targets are
this frame's, and before the scarf so the scarf's bone overrides are the last
word. The re-settle skips `world_override` nodes, so it structurally cannot
touch the scarf, the arms, the hands or the control bones.

### 3.2 ★★★ THE CLAMP IS ON THE WEIGHT, NOT ON THE POINT

The chain trails up to ~2.2 m; a leg reaches under 0.9 m. `solve_chain` clamps
an out-of-reach target into its shell **silently**, so the foot stops moving
while every caller believes it moved.

The obvious fix — clamp the point radially into the annulus — pulls it **toward
the hip, i.e. toward the machine**, and engages in the ordinary stage-3 state.
So the blend limits **how far along the segment the weight may go** instead
(`body_blend_reach_limit`, a ray/shell intersection). Every target stays ON its
own R3→chain segment, which is what keeps the blend monotone in the weight and
the fork measurable.

### 3.3 THE SEAT, BECAUSE THE BLEND IS A THIRD GEOMETRY

A lerp between two individually-legal points crosses non-convex free space: the
seated ankle is on the running board, the trailing ankle is aft and high, and
the chord between them cuts through the seat and tunnel. The chain's own
keep-out cannot see it — **it grades the chain**, and the blended drawn leg is
a third geometry nothing else grades. So the blended knee and ankle are pushed
out of the same seat solid, through one shared implementation.

---

## 4. THE GATE

**Full gate: see §7.** New suite: 6 cases, 190 assertions, and three mutants
killed on a binary proved fresh:

| mutant | leg that caught it |
|---|---|
| reach limit returns `w` always | "the ankle target stays inside the leg's own reach" |
| seat escape applied but discarded | "the blended leg is pushed out of the seat solid" |
| bend normal always returns the fallback | "the bend normal puts the knee where it was asked" |

Each leg carries its own **non-vacuity assertion first** — the fixture must
prove the mechanism fires before the property is graded.

★ **One leg failed honestly on its first run and the defect was in the TEST**:
it took the blend's own `w = 1` output as the segment's far end, but that output
is itself reach-limited, so the segment was being defined by the very clamp the
leg exists to grade. It now reconstructs the endpoint independently from the
measured table. 23 mm, and it would have passed forever.

⚠ **What the gate does NOT cover.** The wiring in `sled_model.cpp` is
unreachable by ctest. That is why it holds no arithmetic — the only defence
available, stated as such and not claimed as coverage.

⚠ **AND THE SMOKE DOES NOT SHOW THE RIDER.** `--smoke` was run twice (blend
forced full ON via `SEADS_BODY_BOARD=1`, and `SEADS_BODY_BLEND=0`): both exit
clean, so the binary boots, runs 90 frames and shuts down with the new code on
the path — no crash, no assert, no shader or link breakage, which is the class
of defect a green ctest is structurally blind to here. But that rig is the
AIRCRAFT probe: it spawns a plane at 2498 m against a bandit, and there is no
sled and no rider in frame. The two screenshots are byte-identical, and that is
EXPECTED — it is not evidence the blend works. **Nothing has yet SEEN these
legs move. That is Chad's drive, and it is the whole verdict.**

---

## 5. ★ OPEN, AND ONE IS A QUESTION FOR CHAD

1. **★ STAGE 2 IS INVISIBLE ON ITS OWN, AND THAT MAY BE RIGHT.** While
   `stage_arm <= 0` the chain is pinned to the drawn man every frame, and the
   pin's ankle station is the midline of the feet `pose_pass` just welded to
   the boards. So blending toward a pinned chain is ≈ identity: **boards going
   light produces no visible motion until stage 3 also arms.**
   I built it that way deliberately — §7.3 makes stage 2 a *release* ("leg IK
   releases from board_socket") and stage 3 the *motion* ("legs trail"). But it
   is your ladder. **Question: when the boards unload but the seat still
   carries him, should the boots visibly float off the boards, or is stage 2
   only the precondition for stage 3?**
2. **The chain has no joint limits.** Nothing stops a knee station folding
   anatomically backward; `solve_chain`'s bend normal keeps the drawn knee on
   its authored side. There the drawn leg cannot match the chain. Watch for it
   on the drive.
3. **`shin_1` / `shin_2` are interpolated stations with no joints** and the rig
   has no bones there, so the drawn shin is not pinned to them. The residual
   fork is a known gap, not a silent one.
4. **`chain_axes` roll is history-dependent** for a segment running along ±X;
   the legs could swap sides. Draw and constraint share the function so they
   swap together.
5. **The kneel foot-blend ordering is MOOT, measured not assumed**:
   `kKneelRuntimeGain = 0.0f` (Chad's 2026-08-21 ruling), so `ws.w_kneel` is
   identically zero at runtime and that path never runs. The blend is written
   to stay correct if the gain is ever restored.
6. **The coat sink** — still attributed, still parked, and this rung does not
   touch a torso bone.

---

## 6. THE DRIVE

**Build:** already built for you.
**Open:** `D:\flight_sim2\seads-recon\build-play\seads.exe`

Get on the sled. The felt question is one sentence: **when the boards go light,
do his legs leave the boards and trail — and do they stay out of the machine?**

| | |
|---|---|
| `SEADS_BODY_CHAIN=1` | draws the chain AND the per-station drawn-surface boxes, so you can see the graded body beside the drawn one. Colour ramps green→orange with the stage weight. |
| `SEADS_BODY_BOARD=<0..1>` | **forces the boot release** without needing air — the A/B handle. `1` = boots fully off the boards, parked. Start here. |
| `SEADS_BODY_BLEND=0` | the off switch. The rung reverts to the signed R3 legs. |
| `SEADS_BODY_CHAIN=2` | the attribution line (stage weight, raw vs filtered, frame accel). |

⚠ **Known and NOT this rung, so it is not a finding:** the torso stays seated
while the legs trail, because you ruled legs-only. And the **coat sink** on the
spine bones is a pre-existing parked defect — if you see the coat sitting into
him, that is the old one, not this.

---

## 7. THE GATE RESULT

**1605 / 1610**, on a clean configure + full build in an isolated build dir
(the tree's own `build/` was contended). The five failures are the known
pre-existing debts, unchanged in name and count from the arming rung:

```
 54 - probe P-F: the relentless raider keeps the pump and shoots back   (enemy-AI)
871 - sled_slides_before_it_tips_on_flat_snow          ┐
872 - sled_grip_ceiling_stays_below_the_tip_threshold  │ the four GI4
910 - sled_assist_reference_plane_is_load_weighted     │ sled debts
913 - sled_debug_sink_is_write_only                    ┘
```

The total moved 1604 → 1610: the six new cases, and **zero new failures**.
Graph regenerated in the same commit; `graph_query.py check` green.

---

## 8. THE NEXT RUNG

**The torso.** Stations 6..10 — shoulder, spine_03/02/01, pelvis — which turns
legs-trailing-off-a-seated-man into actual superman. It needs, and the round-1
review is explicit about this:

1. A **length-restoring pass** over the blended polyline. `pose_pass` has no
   spine IK (the four IK chains are the two arms and two legs; spine bones are
   plain FK composes), so lerping adjacent station positions independently
   shortens the links between them and nothing puts them back.
2. The **arm re-IK**: the shoulder moves, the hands do not (station 0 of the
   chain IS the grip), so the arms re-solve between a moved shoulder and an
   unmoved grip. `solve_chain`'s silent reach clamp is the hazard — the
   forearm tip disjoins from the welded hand.
3. **The coat sink is on those exact bones.** Decide before building whether it
   gets fixed first or explicitly pre-named on the drive card, or a parked
   defect will be attributed to the new mechanism.
