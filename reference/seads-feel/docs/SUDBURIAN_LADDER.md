# THE SUDBURIAN — character ladder

Status: **★ LADDER SIGNED 2026-08-16. ★ R0 SIGNED 2026-08-16. ★ R1a SIGNED
2026-08-17 — "FLOATY" IS DEAD. ★★ R1b REJECTED BY CHAD 2026-08-17 AND REVERTED.
NEXT = R2. DO NOT REOPEN R1b.**

> **★★ R1b IS CLOSED, 2026-08-17.** R1b(c) was built, gated green (1230/1234,
> zero new reds, all 23 rider tests passing) and then **reverted at `8797bf88d`
> on Chad's eye**: *"The arms look way worse than before, I had signed off on
> this already and you were telling me problems were there I didn't see but I
> went on your recommendation. Revert the model character work, it was better
> before — then I think I was meant to sign and move on to R2."*
>
> `assets/sled/indy650.glb` is back to md5 `edf1ebdcfe70b68c41c444f651b6acb1`
> and `indy650.blend` is restored to the signed rig (0.304795 / 0.360867).
>
> **The lesson, and it outranks the geometry: a signed visual is EVIDENCE. When
> measurements disagree with a visual Chad has signed, the measurements are the
> thing on trial.** R1b's premise came from `render/rider_pose.h:746` ("the arms
> are too short"), which is arithmetically FALSE — 0.665662 m of shoulder→wrist
> is 108.25 % of Winter's 0.332·H, a 2.00 m man's arm on a 1.85 m target. The
> arms are LONG; the TORSO is short. Both belong to R2, where the mesh, the rig
> and the grip positions are authored together.
>
> **Full account: `docs/SESSION_HANDOFF_20260817.md`.**

Sign-off record:
- 2026-08-16 — ladder signed: *"you may begin ... go on the ladder."*
- **2026-08-16 — ★ R0 SIGNED** — *"I want to start on R0"*, given as the answer to
  "sign R0 or tell me what's short". Nothing was called short. R0 closes at
  attempt 1 of 2.
- **2026-08-16 — ★ EXPORT TARGET RULED: `main`.** *"Go to main."* This is
  option **(a)** of the open question in §2.2b — export → `seads-recon` (`main`) →
  commit → push → merge `main` down into the sandbox branch. It **overrides my
  recommendation of (b)**, and it is deliberate: the machine's asset precedent
  wins over keeping unsigned character art off `main`. Sandbox branches remain
  RECEIVE-only for the GLB. The open question in §2.2b is **CLOSED**.
- **2026-08-16 — ★ COSTUME REFERENCE SUPPLIED: the Klim Ripsa.** See §4.1. It is
  a **one-piece**, which contradicts §3/§4's "2-piece". One question is open to
  Chad in §4.1; it blocks **R2 art only**, nothing earlier.
- **2026-08-16 — ★ DEBUG-BUILD FINDING ACCEPTED.** *"yes okay on the last finding
  as well."* The game binary moves off `CMAKE_BUILD_TYPE=Debug`. See §10.
- 2026-08-16 — orchestration ruled: builds and coding go to **fresh-context Opus
  agents**; **independent verification always** (the builder never passes its own
  work); **graphify every rung** (`tools/graph/graph_query.py`, `check` green).
- Rung sign-offs are recorded here as they are earned. **No self-pass. Max 2
  attempts per rung.**

### ★ R0 — BUILT AND INDEPENDENTLY VERIFIED 2026-08-16. AWAITING CHAD.
Attempt 1 of 2. R0a and R0b each built by a fresh-context agent and each passed
a separate adversarial verifier that re-derived the numbers from the shipped
bytes rather than from the builder's report.

- Suite **1190/1186 → 1205/1201**, the same 4 pre-existing GI4 reds, **zero new red**.
- Proxy GLB byte-identical across **14** independent rebuilds (verifier's own),
  path/TMP/locale/env-independent; **hash is pinned to the Blender exporter
  version** (`asset.generator` "Khronos glTF Blender I/O v5.1.20") — that is the
  honest claim, not "byte-identical forever".
- Catalogue numbers re-derived from `indy650.glb` by an independent parser:
  max discrepancy **5 µm** (float32 print rounding), 200× inside tolerance.
- Fail-loud proven by **5 separate sabotages** across builder and verifier
  (silent −1 restored; premature `forearm_L`→`lowerarm_l` rename; socket
  renamed; 5 mm geometry drift; wrong parent). Every one went red and named the
  problem. All reverted, md5-proven byte-identical.
- `layer check OK`; `impact render/rider_rig.h` → **1 test TU** (defect 15
  retired for the binding path).
- Zero change under `sim/`, `control/`, `test/harness/`, `test/golden/`,
  `assets/`. `indy650.glb` byte-identical to HEAD.

### ★ DRAW COST — MEASURED, AND A RULING (R0 criterion 5)
Microbenchmark of the exact `sled_model.cpp` skinning block over the real
per-vertex data: **10 skinned prims, 9,069 verts, 19 joints**.

| build | cost |
|---|---|
| `-O2` (release-like) | **0.106 ms/frame** |
| `-O0 -g` (what `CMAKE_BUILD_TYPE=Debug` produces) | **4.73 ms/frame** |

GPU upload is 20 `UpdateMeshBuffer` calls / 217,656 B per frame — 13 MB/s at
60 fps, negligible bandwidth; the cost is the 20 driver calls, not the bytes.

**RULING: keep CPU skinning. Do NOT write a GPU skinning path now.** 0.106 ms
against the 0.5 ms target is 4.7× headroom, enough to absorb R2's larger LOD0.
Revisit only when R5 puts *several* characters on screen.

**★ BUT NOTE, AND IT IS A LIVE ISSUE BEYOND THIS LADDER:** the 4.73 ms figure is
what a **Debug** build actually pays, and both `build/` and `build_sudburian/`
are `CMAKE_BUILD_TYPE=Debug`. The remedy is to run the game binary at `-O2` /
`RelWithDebInfo`, not to write a shader. This is a whole-game observation, not a
rider one.

### ★★ R1a — SIGNED BY CHAD 2026-08-17 AT ATTEMPT 1 (`078e2beab`)

*"by the way the rider positioning is looking good! Its ready"* — signed after
three drives, at attempt **1** of 2. **The rider positioning is now an APPROVED
VISUAL and is frozen except where Chad names a change.**

**R1a's stated purpose is discharged: "floaty" is dead.** Defects 6, 7, 8, 9, 10,
11 and 14 landed; the four floaty defects were all in this rung and all shipped
before any new art, exactly as §6 required.

**Follow-on work continues on the signed rung** (`SUDBURIAN_R1A_ATTEMPT2_SPEC.md`),
judged on its own merits rather than against R1a's attempt budget: §A absorb's drop
moved `pelvis` → `root` so the knees bend (fixes F3, which Chad confirmed by eye);
§B the hit term shaped to kill the measured 60 mm single-tick step; §C
`kBootReseatM.y` corrected to the real deck plane; §D the torso hinge Chad ruled.
**★ §C and §D MOVE THE APPROVED POSITIONING. Chad was asked and ruled "Land both —
they fix real defects", so they proceed — but every change to the zero-input pose
must be reported explicitly.** He consented to the move; he did not consent to
being surprised by it.

### ★ THE FOLLOW-ON `9b9abceab` — INDEPENDENTLY VERIFIED 2026-08-17. FIT TO DRIVE.

Verifier built its own GLB parser + pose + IK + skin (~380 lines, no repo code) and
calibrated it against attempt 1's *published* figures **before** measuring anything
new — including reproducing the 14.351 mm rest dash clearance exactly on the same
mesh pair. Own ctest ×2, own md5, own pass over 299,172 tape ticks.

**GATES ALL CONFIRMED.** md5 `edf1ebdcfe70b68c41c444f651b6acb1` unchanged; zero diff
under `sim/`, `control/`, `test/harness/`, `test/golden/`; **1231 tests, 1227 pass,
exactly the 4 named pre-existing GI4 reds, identical across two runs, zero new red**;
all six tape gates green; **#382 green**; `layer check OK`; purity confirmed by code
reading (4 pose passes, closed-form `asin`, `constexpr` Newton count with a
`static_assert`, no accumulator, no clock, nothing crossing a frame).

**★ THE HEADLINE IS CONFIRMED AND IT IS THE MOST VALUABLE THING IN THE COMMIT.**
Every K reproduced to 4 decimals (0.8216 / 0.7969 / 0.7775 / 0.7663 / +0.8656),
pinned mass **42.2 %** exact, the 14-row mass table sums to 1.0. The verifier also
checked the *premise* at the kernel: `sim/sled.cpp:313` sets
`cg_off = (rider_mass_kg/mass)·(−lat, up, −fwd)`, which displaces the **whole** rider
mass — so "the visual rider CG must move by exactly `lean_fwd_m`" is the correct
honesty condition. **The 105.2 mm fore-aft CG lie was real, it was larger than the
90 mm D9 lie deleted for being one, and nothing had ever computed a rider CG to
catch it.**

**★ THE APPROVED VISUAL IS CONFIRMED TO ZERO MICRONS.** `root`, `pelvis`,
`spine1/2/3`, `neck`, `head`, `upperarm_L/R`, `forearm_L/R`, `hand_L/R`, `thigh_L/R`
— **0.0000 mm on every axis.** Only `shin_L` (−13.0013, −13.2293, −3.5633) and
`foot_L` (0, −25.0000, 0) move; sole 0.277000 → 0.252000. The hinge is zero at zero
input **analytically**, not numerically (`asin(sin φ) − φ = 0`).

### ★★ WHAT THE VERIFIER REFUTED — read before believing any stated bound

1. **CG residual 0.499 mm → REFUTED, it is 1.335 mm.** The builder's 1170 samples
   **omitted `steer`**, which moves `grip_socket_L/R` — the very nodes the hands are
   pinned to — so it is part of the reachable set. On 1800 cells including steer ±1
   the worst is **1.335 mm**. The documented contraction 0.054 is also wrong: true
   local slope spans **0.617–1.047**, giving worst |1−K/k̂| of **0.278**, 4–5× the
   stated figure. Two fixed steps still converge everywhere across 6,651 cells, no
   divergence at any corner. **105.2 → 1.3 mm is still the story.**
2. **★ "The knee stays inboard, so below-deck is footwell volume" → FALSE.** At the
   pose producing −59.1 mm the knee is at x −0.234, z +0.2328, and by
   point-in-triangle test it is **inside an actual deck triangle**. Over 960 poses the
   knee passes **through a `board_L/R` deck triangle in 74 (7.7 %)**, up to 59 mm.
   The stated mitigation for §A's one admitted cost does not hold. (It may be
   invisible *because* of OPEN-R1A-BOARD-NORMALS — that is not a defence.)
3. **★ The helmet contact is WORSE than reported.** Attempt 1 never came within
   **113.8 mm**; attempt 2 closes to **2.5 mm at fwd 0.25**, 1.1 mm at 0.35. And the
   builder quoted the helmet shell's 58 mm while **the chin curtain reaches y 0.7821
   — 110 mm below the cowl top line.** Verifier's judgement: **visibility HIGH** —
   the helmet is the most-watched part of the rider, 1–2.5 mm gaps read as clipping
   in motion, and `lean_return_tau_s = 0` puts you there in one palm swipe.
4. **Arm clamping understated and the LEG cost unreported.** Clamping is
   **19.4 % → 22.5 % (+3.1 points)**, not +1.2. And **worst leg reach 1.0373 →
   1.0750, leg clamping 0.3 % → 1.0 %** appears nowhere in the header or the ladder,
   despite §D item 3 explicitly requiring it.
4a. **Boundary table CONFIRMED (addendum sweep).** The verifier's own
   `(up x fwd)` first-contact boundary reproduces attempt 1 on 5 of 8 rows to the
   digit and the rest within 1.7 mm (its bisection step is 0.88 mm), and reproduces
   attempt 2 **exactly on 6 of 8 rows** -- and **better than claimed on both tuck
   rows** (up -0.10 and -0.05 measured 0.0686 vs the reported 0.0483 / 0.0677).
   Clean-travel gain at up 0 is **4.1x** on the verifier's numbers vs the builder's
   4.33x, and the gain is real across the WHOLE up axis (~1.4x at full stand).
   The table is complete; the +0.20/+0.25 rows I had ruled skippable landed on the
   published values to the digit (0.1354 / 0.1784). **Every claim in the work order
   is now closed.**

5. **Deep lean: quantified.** `d` exceeds `fwd` from **fwd ≈ 0.262 — the top 42 % of
   the range** — up to **+53.7 mm** deeper at full lean. Below that the new pose is
   *shallower* by up to 55.8 mm. The hinge itself costs **zero** clearance (gap
   unchanged at 14.351 mm for θ from −40° to +40°; the builder's "first closes
   between +30 and +40°" does **not** reproduce).
6. **★ "CG honesty everywhere" → FALSE. It is one axis of three.** On the shipped
   build the verifier measures lateral K **0.8134** (−28.0 mm at lat 0.15) and
   **0.8150** (**−64.7 mm** at lat 0.35 / up 0.25), vertical K **0.8036** (−49.1 mm at
   up +0.25) and **0.7837** (+21.6 mm at up −0.10). Not fixing them was *compliant* —
   the work order said not to re-derive those channels — but **the largest surviving
   lie is 64.7 mm, the same class of defect D9 was deleted for**, and
   `rider_pose.h`'s claim of "CG honesty everywhere" must be corrected.
7. **Three constants documented from PRE-§C geometry**, never re-taken after the
   reseat moved: `k_hat` 0.821607 → actually **0.819082**; leg `|cross|` 0.11552 →
   **0.12360**; leg fallback dots −0.8347/+0.8490 → **−0.8158/+0.8746**.
8. **§B's causal counts refuted** (headline numbers all confirmed exactly): "0.24–0.35 m
   in one tick, 7–29 times per tape" is **2/3/2/2 per long tape, 9 total**; "0 of 33
   ticks airborne" is **6 of 32 airborne**; and the hold band is only **0.105 m** wide
   so a 0.105 m jump suffices — 0.24 m was never the requirement. The theorem is sound
   but its floor is `kAbsorbDropM × 0.478`, since absorb never reaches 1.
9. **★ ZERO TEST COVERAGE ON THE NEW PATH, and the acceptance test is not pessimistic.**
   `impact render/sled_model.cpp` → **0 dependents, 0 test TUs**; `pose_pass`,
   `pose_and_solve_lean`, `capture_hinge`, `posed_joint_pos` have none. §D's acceptance
   test uses an analytic stand-in `true_cg(d) = d(0.8384 − 0.2030|d|)` **called
   "deliberately pessimistic" when its slope range 0.737–0.838 is NARROWER than the
   real 0.617–1.047.** Chad's drive is this code's first execution in a real binary.
10. **Bookkeeping repeats:** `generated_at_commit` again names the parent
    (`020426268`). And the builder rewrote the ladder status line, which its work order
    forbade — the text was the coordinator's, git cannot attribute it, and the verifier
    found no substantive contradiction. Audit-trail weakness, not an alteration.
11. **§C's reversed assertion is a WEAK guard.** `|reseat.y + rest_tip_minus_socket_m.y|
    < 0.001` would stay green if someone set `reseat.y` to the socket gap 0.041678.
    What actually catches it is a *different* test, by **71 µm** (171 µm difference
    against a 100 µm tolerance). The guard exists; it is thin and not where its own
    comment says it is.

### ★ THE DRIVE WATCH LIST — in order

1. **The helmet, first.** It meets the cowl from **56 % of forward lean** and grazes
   to full. Chin curtain 110 mm below the cowl top line. Highest-visibility risk.
2. **Deep forward lean is WORSE than attempt 1** beyond ~58 % of the range, by up to
   53.7 mm. The win is the first ~15 % (contact **4.1×** later) plus fore-aft CG honesty.
3. **Absorb is 3× smaller.** Worst one-tick body step 65.7 → 19.1 mm; the
   torso-into-hips motion Chad *saw* is 112 → 12 mm worst, **36 → 5 mm at p90**. The
   verifier expects a plausible "the torso stopped moving" report. `kAbsorbDropM`
   (0.040) is the one dial and the frontier is arithmetically fixed: worst step ≈
   p90 crouch / absorb-p90.
4. **The boots sit 25 mm lower, inside the tray.** Nothing above the hips moved.
5. **The deck is still not drawn from above** (OPEN-R1A-BOARD-NORMALS) — GLB work,
   not in this commit.

### R1a — attempt 1 as built and verified
Attempt 1 of 2. Work order `docs/SUDBURIAN_R1A_SPEC.md`. R1 was split because §6
asks it to repair defects 1-11 *while shipping on the current mesh*, and defects
1/3/4/5 are bone lengths inside a GLB whose `.blend` is not reproducible from
source (§2.2a). **R1a = pure C++, defects 6, 7, 8, 9, 10, 11, 14 — which is all
four "floaty" defects. R1b = defects 1/3/4/5, needs the live Blender session.**

Verified by a separate agent that re-derived every number from the shipped bytes
with its own GLB parser, its own ctest run, and its own evaluation of the shipped
`rider_absorb` over the tape columns — not by checking the builder's arithmetic
against itself.

- Suite **1205/1201 → 1224/1220**, the same 4 pre-existing GI4 reds (519, 520,
  553, 556), **zero new red**. The +19 is exactly `test/unit/test_rider_pose.cpp`.
- **All six tape gates replay bit-exact** (`tape_360_chad_repro`,
  `..._provocation`, `..._attempt1_provocation`, `tape_chad_flip_fence`,
  `sled_tape_round_trip...`, `sled_tape_off_arm...`). §0.1 discharged.
- `indy650.glb` md5 `edf1ebdcfe70b68c41c444f651b6acb1` **unchanged**; zero diff
  under `sim/`, `control/`, `test/harness/`, `test/golden/`. `layer check OK`.
- Every published headline number reproduced independently: the six GLB
  measurements, `kBootReseatM = (0, +0.066849, +0.050800)`, `dmax` 0.664662,
  0.730 / 0.738 / 0.971, worst arm ratio 1.420, absorb max **0.937133**, absorb
  never 1.0 across 316,040 ticks, absorb exactly 0 at rest.

**★ THREE VERIFIER FINDINGS THE BUILDER DID NOT REPORT.** None blocks the drive;
all are recorded here so they cannot be lost.

**F1 — the new D14 bend-plane fallback ANTI-MIRRORS, and a GREEN TEST PINS THE
WRONG INVARIANT.** A bend *normal* must anti-mirror to produce a mirrored *bend*,
because `cross` is handedness-flipping. The old world constant `(0,−1,0.1)` did
anti-mirror and was side-consistent; its real defect was pointing the knee the
wrong way (dot −0.2977 against both legs), not failing to mirror. The new
parent-frame fallback derives from the root's `+Z` column, which *mirrors*, so
the elbow direction flips: dot vs the authored normal is **−0.7955 / +0.7955**
on the arms and **+0.8490 / −0.8490** on the legs — right on one side of each
pair, inverted on the other. `TEST_CASE("the bend normal MIRRORS between
sides")` asserts the reflection of the **normal**, which is exactly the condition
that makes the bends asymmetric. **Latent, not live:** measured `|cross(st,se)|`
is 0.1098 (arms) / 0.1155 (legs) against a 1e-4 threshold, so the branch is
unreachable on the shipped asset and nothing on screen is wrong today.
**★ BUT R2's §2.1 pre-bends deliberately move chains toward straight, which is
exactly where this fires.** Fix before R2, and rewrite the test to assert that
`cross(n, dir)` mirrors, not that `n` does.

**F2 — D7 is a structural NO-OP on this asset; the visible fix is D6 alone.**
`board_socket_L/R` are direct children of `SLED_ROOT`, which the per-frame pose
loop never touches — so `nodes[n_board[s]].world` is a per-frame *constant* equal
to `rest_world[n_board[s]]`, and the new target is identically the old constant
shifted by 84 mm. The new code comment claims the boot "tracks the board through
steer, lean, stand, suspension and the whole body mount"; **it does not**, and
the comment it replaced said so correctly. The plumbing is right and future-proof
(it is what the spec asked for, and it is what makes R4's board-release possible)
but **nobody should expect D7 by itself to change a pixel.** Second undocumented
consequence: `world_override` on the foot freezes the boot's *orientation* too,
so there is now **zero ankle articulation under any input** — planted, but rigid.

**F3 — waking `absorb` makes the torso telescope into the hips, in single-tick
60 mm steps. Highest video risk on this rung.** Absorb's only consumer is
`t.y -= 0.12f * absorb` at the **pelvis**, and in this rig `thigh_L/R` are
children of **`root`, not `pelvis`**. So the drop moves torso/arms/head down
relative to the hip joints while the legs do not move — and D6/D7 have just
welded the boots rigid to the boards, so the legs cannot absorb either. At the
measured worst (0.937) that is **112.5 mm of torso sinking into the pelvis**.
p99 on the long tapes is 0.500 → 60 mm; p90 0.29 → 35 mm. It is spiky *by
construction* and correctly so (§0.1 forbids an accumulator): tick-to-tick
`|Δabsorb|` p99 is 0.033 but **max is 0.500 — a 60 mm pelvis jump in one
1/120 s tick**, because `susp_v` peaks past 90 m/s and the saturating map turns
a one-tick spike into most of the crouch. The builder followed the spec exactly
("keep the existing `0.12f` pelvis drop as its consumer for now") and reported
absorb's range honestly; it did not report what the drop does to the body. This
channel has never been on screen before — it was hardcoded 0 forever.

**Corrections to the builder's own framing, all in the safe direction:**
- D9's "19.1 mm of margin" is real but is consumed by **80 mm of lateral lean or
  30 mm of aft lean**. On the kernel-*reachable* set arm clamping goes
  **5.5 % → 17.3 %**, a **3.1×** worsening — the "23 % → 37 %" framing (a
  per-cell figure on a 200-cell grid) understates the builder's own finding.
- The D9 leg "before" of 1.076 was measured with the reseat already applied. The
  true old→new is **1.157 → 1.037**, so the leg win is *bigger* than reported.
- Absorb *helps* the arms and nobody noticed: the shoulder sits above the grip,
  so at full stand absorb 0.937 takes the arm ratio 0.971 → **0.843**. The worst
  arm cases need smooth ground *and* hard lean.
- Bookkeeping: the header's leg `|cross|` 0.1454 is the *pre-reseat* target and
  contradicts the code comment's 0.1155 (both true, different targets); the
  header's `susp_x` p90 row does not reproduce on `max`-of-3; the four long
  tapes cited as evidence live in `build/` and are **not committed**;
  `generated_at_commit` names the parent commit.

**★ OPEN FOR CHAD — D9, and it is a real fork.** Deleting the 90 mm shift was
the tape-safe default and the spec's stated test says proceed. But the arms
already clamped in ~6 % of the reachable box *before* this change — that is
defects 1/3/5 (forearm longer than upper arm, 0.38 m shoulders, a 1.6 m figure).
Deletion worsens a pre-existing condition rather than creating one. Two ways out:
**(a) fix the arm lengths in R1b** (Blender rig work, no kernel risk, removes the
reason the shift existed), or **(b) promote the shift into the kernel** so visual
and simulated CG agree — a kernel rung with a tape re-pin. **Recommend (a).**

### ★ CHAD DROVE R1a 2026-08-17 — ONE FELT DEFECT, MEASUREMENT IN FLIGHT

Verbatim: *"I didnt really see much for the bobbing pelvis ill have to check
again but one thing I notice is that when crouching near the front im sinking
down and smushing into the dash."*

**OPEN-R1A-DASH.** Both halves of that report matter and neither is yet resolved:

1. **The dash intersection is a NEW felt defect** and it is the one to fix. It
   maps to `lean_fwd_m` toward max with `lean_up_m` negative (the tuck). No
   clearance term against the console geometry exists anywhere in the pose path.
2. **F3 was NOT confirmed by eye.** The verifier called the pelvis telescoping the
   top video risk at 112 mm and 60 mm single-tick jumps; Chad did not see it.
   That is not the same as it being absent — he said he will check again.

**★ THE HYPOTHESIS BEING MEASURED, and it unifies both halves:** F3 and the dash
report may be **one defect seen from two angles**. Absorb drops the pelvis while
the thighs hang off `root`, so the torso telescopes *down*. Read from behind on
level ground that is a bob and easy to miss; read while already crouched forward
over the console it is the chest and helmet **sinking into the dash**. If that is
right, the fix is F3's fix — a different consumer for absorb, not a clearance
hack — and the two findings collapse into one.

Being measured now, read-only, before any dial moves: the exact rider-mesh /
machine-mesh pair and penetration depth; the `(fwd, up)` contact boundary as a
table; absorb's share versus the absorb = 0 baseline; a ruled in/out verdict with
a number on each candidate cause (raw root translation with no clearance term,
the deleted `q_stand` pitch — which should be irrelevant since it only fired on
*positive* `lean_up_m`, the F2 rigid boots removing leg compliance, `sag0` 0.10
drawing the machine 3-30× low and pulling the dash up); and critically **whether
this pre-dates R1a or R1a caused it.**

**★ CHAD ANSWERED IT HIMSELF, 2026-08-17:** *"reveal it the rider is pretty far
up by default."* So **R1a revealed the intersection, it did not cause it**, and
the root cause is the rider's **default mount height**, not a missing clearance
term. That is the felt-call owner's read and it is being verified in numbers, not
assumed.

**★ THE CHAIN THIS IMPLIES — and it says defect 6 was always a symptom.**
If the rider is mounted N mm too high, then the legs must over-extend downward to
reach the boards — **which is exactly what defect 6 measured** (`foot_L` 42 mm
below `board_socket_L`, sole 117 mm below). R1a fixed that symptom by raising the
boot target 66.8 mm onto the deck and left the cause untouched. So the honest fix
is a **rider mount re-seat derived from SEAT CONTACT, exactly the same class of
correction as `kBootReseatM` was from deck contact** — a `kRiderMountOffsetM`, the
same proven pattern rather than a new mechanism.

**★ AND THE RISK THAT MUST BE MEASURED BEFORE IT IS TOUCHED:** lowering the rider
changes every clearance and every IK reach *at once*, and it may push the D9 arm
problem the wrong way — a lower rider reaches **further** to the grips, on arms
that already clamp over ~17 % of the reachable lean box at worst ratio 1.420.
Being measured: arm and leg reach ratios across the box at the proposed offset,
dash clearance at the crouched-forward pose, and whether the helmet then fouls
anything else or drops below the windshield line.

**Whether this is R1a attempt 2 or R1b is itself open.** If the rig is authored at
the wrong height inside the `.blend`, it is R1b and needs Chad's Blender session;
if a render-side offset is legitimate and sufficient, it is R1a attempt 2.

**Attempt budget: R1a has used 1 of 2.** The fix lands as attempt 2, so it is
measured and root-caused first rather than dialled.

### ★ SECOND DRIVE, 2026-08-17 — F3 CONFIRMED, PLUS THREE MORE

Verbatim: *"I just drove again, I saw the torso go down on a bump. Not alot of
bumps more snowbank to see and the light I believe is oversaturated if the last
agents light work didnt get sent into the build-play, becasue I could barely see
to tell. And the HUD was right at foot level but they seemed to be in a good
place. The snowmachine does have footwells, I dont know if you think we could use
those too fo standing or casual touring."*

**★ F3 IS CONFIRMED BY EYE — "I saw the torso go down on a bump."** It is a real
visible defect, not just a number, and it is **separate** from the dash
intersection (which Chad attributed to mount height). Both must be fixed. The
unified-defect hypothesis above is therefore **REFUTED**: they are two defects.

**F3's fix is a consumer change, not a filter.** §0.1 forbids smoothing, so the
60 mm single-tick jump is a *shaping* problem. Being evaluated with numbers:
drive a coupled knee/hip crouch against the now-welded board target (the rigid
boot may be exactly the anchor that makes this work); split the drop between
pelvis and a spine bone; or move the drop to `root` so the whole body descends
and the leg IK absorbs it against the fixed board target. Each must report its
effect on the leg reach ratio (currently worst 1.037) and must need no kernel
change.

**★ OPEN-R1A-LIGHT — BLOCKING THE GATE.** He *could not see well enough to
judge*. Established and not to be re-litigated: the lighting work **is** in the
binary he drove — `build-play` was built from this worktree at HEAD and both
`50a3d8f6b` (overdriven headlight + gauges) and `bd223d607` (aim it down, dim the
barrel view) are ancestors. So this is the **post-fix** lighting, still blown out.
Under diagnosis. Leading suspicion, stated so it can be refuted: **a missing or
misconfigured tonemap/exposure**, because "could barely see to tell" describes
loss of *contrast and shape*, not mere brightness — a clipped highlight destroys
the shading that reveals form, and he was looking at **snowbanks**, i.e. a
near-white albedo where a bright light saturates soonest. Also being checked: the
beam's own intensity/width, and whether an emissive term is mis-targeted the way
`"gauge_face_L"` is (the real nodes are `indy650_gauge_face_L/R`, so that emissive
has never matched — if a similar mismatch lands on something large, that alone
would do it). **A same-night workaround is worth as much as a fix.**

**★ CHAD NARROWED IT, and it moves the suspect off the lamp entirely:**
*"So much white light from the sky, and even at night from something."*

Two facts, and the second localises the bug: the light comes from the **SKY**, and
it **survives nightfall**. Anything still flooding white light after sundown is a
term that **does not scale with sun elevation** — an ambient floor, a scatter term
clamped from below, or an unaccounted second source. The tonemap theory is
demoted to *compounding* rather than causal.

Named suspects, in order:
1. **A sky-ambient term that reads the sky COLOUR but not the sun's ALTITUDE.**
   Anything shaped like `max(ambient, k)` with non-zero `k` produces exactly this
   sentence. Leading hypothesis.
2. **The moon.** Moon geometry/texture demonstrably exists (a `world-sudbury` HEAD
   was once build-broken without a one-line `sky.h` `moon_tex` change). An
   overbright, sun-independent moon *is* "even at night from something".
3. **The atmosphere/scatter path** (`render/scatter_glsl.cpp`,
   `render/post_glsl.cpp`, `AtmosphereField`, `docs/bubble_atmosphere_spec.md`,
   `docs/airdome_report.md`) — in-scattering with a night floor, or the sky
   sampled as an IBL/ambient source whose magnitude never decays.
4. **Snow as the amplifier, not the cause.** Snow albedo is near 1.0, so it
   re-radiates nearly everything it receives; a modest ambient error becomes a
   blowout **on snow specifically**, which is why he saw it on snowbanks.

### ★★ OPEN-R1A-LIGHT — DIAGNOSED 2026-08-17, AND IT IS A WHOLE-GAME FINDING

Reproduced with `build-play\seads.exe --smoke` in drive mode at the densest street-
lamp cluster (236 lamps inside 500 m), measured on the ground band only (rows
55-92 %, cols 20-80 %, off the HUD gutters). **Night ground is BRIGHTER than
dusk** — 0.817 mean vs 0.693. At day, **17.08 % of the ground band is pure 255**,
and with post disabled **85.78 % of the raw scene is at or above 1.0.**

**★ BOTH OF CHAD'S NAMED SUSPECTS MEASURE ZERO.** Stated plainly because it
matters for how the next one of these is run: `SEADS_NO_LAMPS=1` is
**bit-identical** (street lamps contribute 0.000), and scatter contributes 0.000 —
`kScatterGLSL` already uses per-channel Rayleigh (`kChanW = vec3(0.55,0.85,1.35)`)
and gates **both** Rayleigh and Mie on `smoothstep(uDuskLo, uDuskHi, sunElevRad)`,
so scatter is exactly 0 by 8° below the horizon. It is not the scalar-coefficient
bug I predicted. **His observation was right and his attribution was wrong, and the
observation is what found it** — the lesson is to measure the split, not to chase
the named part.

Night attribution, as % of the 0.756 above the all-terms-off floor of 0.061:

| term | share |
|---|---|
| `[ground] night_glow` | **76.3 %** |
| `[moon] ground_gain` | **18.4 %** |
| `night_fill_min` | 0.7 % |
| street lamps / scatter / sparkle | **0.0 %** |

**CAUSE 1 — `uWinterNightGlow` carries NO SURFACE NORMAL (76 % of the night
flood).** `render/planet.cpp` `planet_fs()`:
`lit += vec3(albLum) * uWinterNightGlow * winterSurf * nightAmt;` — `nightAmt`
gates it, so **TOD does reach it**; but there is no `ndl`. It is a flat,
direction-independent add of ≈0.72 onto snow. **This is why the loss is of SHAPE,
not brightness: three quarters of the light on the snow is shadeless by
construction, so no exposure or tonemap can put form back.** Dial:
`config/world.toml:322 night_glow = [0.95, 1.02, 1.14]`, raised ×1.357 on a
2026-08-11 fly; the file's own note already says "drop to 0.55 if it reads too
bright."

**CAUSE 2 — `ground_day_gain` breaks the invariant its own comment states (88 % of
the DAY flood).** `vec3 lit = albedo * (uNightFill + uGroundDayGain * ndl);` with a
comment three lines above reading *"Day peak = albedo*(fill + gain) ~ albedo (full
B&W detail, no blowout)"* — which requires `fill + gain ≈ 1.0`. Shipped is
`0.05 + 1.60 = 1.65`, × `snow_albedo 0.92` = **1.518, i.e. 152 % of white**. Snow
clips at `ndl ≥ 0.648`: **every facet within 49.6° of facing the sun.**

**CAUSE 3 — there is no tonemap; the headroom is DISCARDED, not rolled off.**
`render/post_glsl.cpp` `scurve1()` opens with `x = clamp(x, 0.0, 1.0);`. The scene
RT genuinely is RGBA16F, so the headroom exists and is thrown away. **This falsifies
commit `50a3d8f6b`'s stated premise** ("a gain above 1 survives into the bloom
instead of clipping at white") — the halation taps re-apply `scurve()`, so the lens
emissive 9.0 is indistinguishable from ~1.2.

**CAUSE 4 — moon fill is not sun-gated.** `lit += albedo * uMoonFill * ndlMoon;`
with `ndlMoon` purely geometric, so `[moon] ground_gain = 0.55` **also stacks on
the 1.65 day gain in daylight.**

**★ CAUSE 5 — THE SNOWBANKS CHAD WAS TRYING TO READ ARE COMPLETELY UNLIT.** This
is the direct hit on *"not alot of bumps more snowbank to see."* `kBankFS`:
`vec3 c = uBase * (1.0 - cover*uDark*(1.0-skirt));` with
`BankLook::base{0.78,0.81,0.86}` — **no sun term, no normal, no night gate.** Same
for `kRibbonFS` (roads *and* winter trails; `trail_winter_color` is a constant
24 h) and the `uAmbient` 0.48 half of `render/buildings.cpp`. These measured 0 % of
*this* frame's flood but they are real bugs: **he was asking a flat-shaded constant
to show him shape.**

**★ CAUSE 6 — UNTESTED AND NOT RULED OUT: the SC1 helmet fog, and Chad's exact
case maximises it.** `app/main.cpp` → `render/post_glsl.cpp`. At the −28 °C snap,
`cold_excess ≈ 13 K × 0.010/s` **saturates `uFog` to 1.0 in 7.7 s of driving below
8 m/s**, and it clears *only* above 8 m/s. At `uFog = 1` the post adds **+0.215
luma at centre and +0.495 at the corners** of near-white, on a ground already at
0.89. **There is no off switch.** A 90-frame smoke is 1.5 s so it never fired in
any measured shot — but **standing still to watch the rider is precisely the case
that saturates it.** This must be tested before the light is called fixed.

**★ CAUSE 7 — AND THIS ONE IS AIMED AT THE SIGN-OFF GATE ITSELF.** The rider spans
p05-p95 luma **0.024-0.135 — 28 of 255 code values, 11 % of the range** — pinned at
the bottom of a 1.35 S-curve against a ground at 0.85-0.97. Silhouette contrast is
fine (0.81); **internal modelling of the limbs is nearly absent.** Removing the
glare does *not* fix this (rider 0.0570 → 0.0572). **So even with the flood fixed,
judging weight-in-motion on this rider is harder than it should be, and that is a
gate-validity problem, not a taste one.**

**WORKAROUND, ready tonight, no rebuild — `config/world.toml` is read at startup.**
Plus `SEADS_SLED_BEAM=0` to take the headlight out of the question:

| line | from | to |
|---|---|---|
| `config/world.toml:30` `ground_day_gain` | 1.60 | **0.95** |
| `:322` `night_glow` | [0.95,1.02,1.14] | **[0.55,0.59,0.66]** |
| `:1048` `[moon] ground_gain` | 0.55 | **0.30** |

Measured: day cel 0 goes 0.851 mean / **17.08 % clipped → 0.604 / 0.00 % clipped**;
night cel 120 goes 0.817 → **0.532**.

**CHAD RULES EVERY ONE OF THESE, one dial at a time.** Only `ground_day_gain
1.60 → 0.95` has a **non-taste** justification — it restores the `fill + gain ≈ 1.0`
invariant the shader already claims for itself, removing the clip *by construction*
(0.92 × 1.00 = 0.92) rather than by preference. Its cost: it dims **all** land in
daylight ~40 %, including barren rock at albedo 0.02-0.1 that is already near-black,
**and Chad signed off the 1.60 daylight look.** The `night_glow` cut is the bigger
felt win but is pure taste, and **the structurally right fix there is not a smaller
number — it is to give the term a normal dependence** (e.g. a hemispheric
`0.5 + 0.5*dot(shN, up)` weight) so snow keeps its brightness *and* regains shape.
That is a change to a flown look and must be specced, not slipped in. The tonemap
is worth doing regardless but is **not sufficient alone**: a shoulder at knee 0.85
maps 1.0→1.518 into 0.945→0.998, recovering only ~13 code values.

**★ ONE THING THAT WAS DELETED AND SHOULD NOT HAVE BEEN.** The diagnosing agent's
~35 smoke runs each wrote a `conquest_tape_*.jsonl` into the worktree root, and it
deleted **all 60** — the earliest timestamped 20:40 on 2026-08-16, so **a few
predated it and were not its to remove.** They were untracked and nothing
references them, but they are gone. Flagged in case one was wanted.

**OPEN-R1A-HUD (minor, filed not fixed).** *"the HUD was right at foot level but
they seemed to be in a good place."* Two facts in one sentence: the HUD overlays
at foot height, which is a layout problem; and **the feet themselves read as
correct** — which is the first positive signal on D6's re-seat.

**★ OPEN-R1A-FOOTWELLS — Chad's idea, and it is a good one.** A real sled has two
foot positions: the recessed **footwell** inboard (seated touring — lower, more
protected) and the flat **running boards** outboard (standing — wider, more
leverage). Selected by posture.

Recommended: **take it, and put the switch in R3**, because it is exactly what
R3's analog pose set is for — a second IK target selected by `lean_up_m` / the
stand input, a kernel scalar we already have, precisely the way the thumb throttle
is driven by `throttle`. **No new state, no new solver, no new bones.** But the
*measurement* belongs now, while the mount is being re-derived, because it may
change a conclusion: if seated feet belong on a surface lower and further inboard
than the board deck (0.277), then the seated leg extension differs from what
R1a's re-seat assumed and **`kBootReseatM` may be re-seating the boots onto the
wrong surface for the seated pose.** Being measured: whether footwell geometry
exists in the GLB at all (by geometry, not by name), its height versus the deck,
and whether it needs a socket node authored — if it exists with no socket, that is
an R2/export obligation.

### ★★ R1a FOLLOW-ON §A-§D — BUILT 2026-08-17, AWAITING INDEPENDENT VERIFICATION

Work order `docs/SUDBURIAN_R1A_ATTEMPT2_SPEC.md`. All four fixes landed. Numbers
were re-derived from the shipped bytes with an independent GLB parser + full CPU
skin that first reproduced every published R1a figure (`dmax` 0.6646620, arm
0.7301 / 0.9712, box worst arm 1.4204, leg 1.0373, `|cross|` 0.10982 / 0.11552,
absorb max 0.937133, and the dash figures 14.351 mm / contact at fwd 0.0185).
Full detail lives in `render/rider_pose.h`; this is the ladder-level record.

**★ THE FINDING THAT MATTERS MOST, AND IT REFUTES THE WORK ORDER'S PREMISE.**
§D's spec states that the shipped root translation "moves the rider's CG by
**exactly** `lean_fwd_m`" and is therefore already CG-honest. **Measured, it is
not.** With a Winter/Dempster segment CG over the real posed rig the response is
K = 0.8216 at fwd 0.10 falling to **0.7663 at fwd 0.45**, i.e. the shipped
fore-aft channel carries a **105.2 mm CG lie at full forward lean** (and +33.6 mm
at aft max) — **larger than the 90 mm D9 lie that was deleted for being one.**
Cause: the hands are IK-pinned to `grip_socket_L/R` and the boots to
`board_socket_L/R`, and 42.2 % of the rider's mass sits in those pinned chains.
Nobody had measured it because nothing computed a rider CG. The solved hinge
replaces that 105.2 mm with a measured max of **0.499 mm**.

**★ SECOND REFUTATION, §B.** The work order attributes the 60 mm single-tick step
to the HIT term (`susp_v` peaks past 90 m/s). Measured term by term over 299,172
ticks: `|d hold/dtick|` max is **1.0000** — the whole term in one tick — against
0.6524 for hit. It is the HOLD term, and no hit shaping can touch it (half raised
to 40 with a cube still leaves `|d absorb|` max at exactly 0.5000, the hold
weight). Root cause: `susp_x` is clamped to `[0, 2*susp_travel_m] = [0, 0.52]` and
moves 0.24-0.35 m in one tick 7-29 times per long tape, on bottom-out landings and
the contact loss right after. **And there is a theorem: absorb is a pure function
of one tick's state, so its worst single-tick step IS its full authority. No
shaping can reduce it below `kAbsorbDropM`.** A test now pins that.

**★ THIRD: the surface §C corrects does not exist, confirmed independently.**
`board_L/R` is a shallow TRAY: a flat deck sheet at y = **0.252000** (58
triangles, 0.16075 m², x |0.2100..0.3750|, z [-1.0100, +0.3100]) with 25 mm walls
(53 X-facing triangles, y 0.2520..0.2770, x |0.2110..0.3810|). 0.277 is the wall
top, not a floor. `kBootReseatM.y` 0.066849 → **0.041849**.

**★ OPEN-R1A-BOARD-NORMALS — MACHINE ART, FILED NOT FIXED (R1b / cowl ladder).**
`board_L/R` have **zero up-facing triangles**: all 58 horizontal triangles are
wound −Y with authored normals −Y (winding and normals agree, 115/115). With
backface culling in its default state **the running-board deck is not drawn from
above at all** and the boots stand on an invisible surface. This is very likely a
large part of what Chad saw. No byte of the GLB was touched.

**★ OPEN-R1A-RIG-ROLL — RIG OBSERVATION (R1b / R2).** Fixing F1 required finding
which rest-frame column ANTI-mirrors, since a bend *normal* must anti-mirror for
the *bend* to mirror. Measured with M = diag(−1,1,1): **+X anti-mirrors exactly
(dot −1.0000)**, +Y and +Z mirror (dot +1.0000) — so attempt 1's +Z was exactly
the wrong column and +X is exactly the right one. But against the authored normal
the +X fallback scores **+0.8616 on both arms and −0.8347 on both legs**: the rig's
arm and leg ROLLS disagree, so no single frame axis can point both an elbow and a
knee the right way. Side-consistency is now guaranteed; anatomical correctness on
a straight chain still needs §2.1's pre-bends (R2). Branch is unreachable on the
shipped asset and the tests pin that.

**★ OPEN-R1A-HELMET — A NEW CONTACT CLASS §D INTRODUCES, AND THE FIRST THING TO
WATCH ON VIDEO.** In attempt 1 the head group never comes within 60 mm of any
machine part at any forward lean. With the hinge it pitches down and forward: at
30 deg the helmet's lowest point drops y 1.0489 → **0.8346**, below the cowl top
line at 0.892, and the head-group gap closes fwd 0.15 → 32.4 mm, 0.20 → 16.6 mm,
**0.25 → contact with `indy650_cowl`** (chin curtain first), staying in contact to
full lean. A real rider does tuck behind the windshield line, so the direction is
right; the intrusion is not.

**★ THE COST OF CG HONESTY, STATED AS A FORK FOR CHAD.** Because the hinge's total
CG authority is only 54.8 mm (12.2 % of the 450 mm range — trunk+head is 57.8 % of
mass with its CG just 194.8 mm above the hip, defects 3/5), an HONEST full lean now
needs **0.5037 m** of root translation where attempt 1 used 0.450. So the first
15 % of travel is much cleaner (contact onset 0.0158 → 0.0686 m at up 0, a 4.33x
gain) and **deep lean intrudes deeper than before.** The alternative — cap the
translation and carry a stated CG error — is Chad's call, not this rung's.

**★ AFT LEAN IS A CG-SOLVED TRANSLATION, MEASURED OUT NOT OMITTED.** An aft hinge
costs 0.0968 of worst arm reach ratio and 7.6 points of clamping (1.4568 → 1.5536,
20.5 % → 28.1 % over a 2520-cell reachable sweep) and buys nothing — aft clearance
is 44.2 mm, three times the forward figure. `kHingeAftMaxRad = 0`.

**★ THE ZERO-INPUT POSE, because it is an APPROVED VISUAL.** At
(steer, lat, up, fwd, absorb) all zero, `pelvis`, `spine3`, `head`, `thigh_L/R` and
`hand_L/R` are **identical to the micron**, and so is the helmet bbox. The only
change is below the hip: boot sole y 0.277000 → **0.252000** (−25.0 mm, onto the
real deck), foot joint −25.0 mm, knee (`shin_L`) −13.0 mm in x (inboard),
−13.2 mm in y, −3.6 mm in z. Rider CG y 0.75558 → 0.75200 (−3.6 mm). **What a
person would notice: the boots sit 25 mm lower, inside the running-board tray
instead of level with its wall tops, and the knees drop and tuck ~13 mm. Nothing
from the hips up moves at all.**

**★ THE ONE DIAL IF THE LEAN ONSET READS AS A SNAP.** `kHingeDemandShare` (shipped
1.0 = hinge-first). Measured on Chad's own tapes, the trunk angular rate is
**0 deg/s at p90 and ~69 deg/s at p99 for every share**, with only the extreme
tail differing (max 1007 deg/s at share 1.0 vs 270 deg/s at 0.25) — while the
contact onset changes 3.3x (0.0686 m vs 0.0205 m). Shipped at 1.0 on those two
columns. CG honesty is independent of this dial.

### R0 follow-ups (cosmetic, fold into R1)
1. `test_rider_rig.cpp` — the `INFO("drifted: …")` sits inside the `for` body, so
   Catch2 scopes it out before `REQUIRE(bad.empty())`; a geometry drift fails as
   a bare `false` without naming the joint. One-line fix.
2. The comment above the skin loop still says "~4k verts"; it is **9,069**.
3. `build_sudburian*/` is not in `.gitignore`.
Author: orchestrated consult, 2026-08-16. Five strands: 3 read-only codebase
audits (rider runtime, asset pipeline, multi-mode requirements) + 2 context-free
expert consults (rig architecture, pipeline red-team) that were given no repo
access so they could not rationalise what we already built.

This document governs the rider/pilot/on-foot character. It follows the same
discipline as the Indy 650 cowl ladder (`assets/sled/indy650_src/COWL_HANDOFF.md`,
`v1.md`..`v11.md`): one rung at a time, one versioned `.py` per attempt, a
measured census per rung, **Chad signs each rung**, max 2 attempts per rung, no
self-pass.

---

## §0 THE STANDING LAWS

**§0.1 — DETERMINISM IS THE FIRST LAW.** The character's pose is a pure function
of `(kernel state, tick)` and writes **nothing** back. Render is downstream and
read-only — `render/draw.h:461` already calls this the "one-number rule". No
animation state persists across frames that is not reconstructible from sim
state. No `GetFrameTime()` accumulation into anything gameplay-visible. No
root motion driving sim position. No animation noise seeded from wall clock.

  *Gate, and it is non-negotiable:* `tape_360_chad_repro` and the full sled
  golden set replay **bit-exact** with the character system enabled AND with it
  compiled out. This test is written in R0, **before the character exists.**

  Note this protects us both ways: it also means a character glitch can never
  corrupt a signed sled tape.

**§0.2 — KERNEL UNHARMED (Chad's ruling, 2026-08-16).** The rename in R0 and the
rig work in R1 touch **zero** kernel symbols. Proof obligation, discharged in
§2.3: `sim/` contains no bone names; the tape (`test/harness/sled_tape.h:56-59,
93, 190-192`) pins rider *parameters and state in metres* — `rider_mass_kg`,
`lean_lat_seated_m`, `rider_lat_m`, ... — never a joint name. Any rung that
would move a kernel number STOPS and asks.

**§0.3 — SINGLE SOURCE, NO FORKS** (CLAUDE.md H1). Costume colours read existing
constants. The stripe blue is `render::kComplementBlue` (`render/team_color.h:72`),
never a retyped literal. See §4.

**§0.4 — THE GATE FOR ANYTHING THAT MOVES IS A VIDEO, NOT A CONTACT SHEET.**
This is the one process change from the nosepan ladder, and the red team was
emphatic about it: a still frame cannot reveal bad weight. Geometry rungs still
gate on a measured census + contact sheet. Motion rungs gate on a short looping
capture watched at speed, in game.

**§0.5 — LAYERING.** `tools/graph/layer_rules.toml`: `seads_render_core`,
`seads_tests`, `seads_harness` all `deny_ext = ["raylib"]`. Therefore **pure
pose/IK/blend math goes in `seads_render_core`** (where it becomes testable —
today `render/sled_model.cpp` has fan-in 0 and **zero test TUs**), and only mesh
upload / draw lives in the `seads` exe target. `graph_query.py check` must stay
green every rung.

**§0.6 — REFERENCES FIRST.** Every agent opens `assets/sled/indy650_src/X_rider.png`
and the sled refs and plans against them BEFORE committing geometry.
(Path corrected 2026-08-16: there is no `refs/` subdir.)

**§0.8 — LINE NUMBERS IN §1 ARE STALE.** The lighting/gauges commit `50a3d8f6b`
shifted `render/sled_model.cpp` by roughly **+215 lines**. The audit citations in
§1 were taken before it: skinning is now ~`:805-839`, vertex re-upload
~`:833-838`. **Re-locate by symbol, never by line number.** The defects
themselves are unaffected.

**§0.7 — ORDER OF WORK IS MOTION → RIG → MESH.** Counter-intuitive and almost
everyone gets it wrong. If the mesh is built first and the motion later proves
the arms intersect the chest, the mesh is re-made. Forbidden: mesh detail before
R1 is signed.

---

## §1 WHAT ALREADY EXISTS (audited, not assumed)

> ★★★ **READ `docs/RIDER_AUTHORITY.md` BEFORE THIS SECTION.** The paragraph
> below describes the **LEGACY rider** — the R0 audit of what the game shipped
> BEFORE this ladder. It is the figure being REPLACED, not the figure being
> built. **Do not do rung work on `rider_rig` / `rider_*` in `indy650.glb`.**
> The Sudburian is `sudburian_rig` (42 bones) in `assets/character/`. Two
> sessions have read this section as "work in `indy650.glb`" and built on the
> wrong rider with green gates; the 2026-08-18 overnight one was reverted.

The rider is **not** primitives. `assets/sled/indy650.glb` (4.1 MB, 197 nodes,
140 meshes, 113,036 verts) carries **1 skin / 19 joints / 0 animations**:

- Skinned: `rider_body` 7,363 v, `rider_boot_L` 852 v, `rider_boot_R` 854 v.
- Rigid children: `rider_helmet`, `rider_shield`, `rider_chin_curtain`,
  `rider_mitt_L/R`.
- `render/sled_model.cpp` does CPU linear-blend skinning (`:591-625`, 4
  weights/vert, `UpdateMeshBuffer` every frame), 2-bone analytic IK on four
  chains (`capture_chain :379`, `solve_chain :424`), and rest-pose bend-plane
  capture. It parses the GLB with cgltf directly, **not** `LoadModel`, because
  raylib flattens the hierarchy (`:190`).
- The analog inputs are wired end to end and are genuinely physical:
  `SledInputs.lean_lat/.lean_fwd/.stand` → `SledState.rider_lat_m/fwd_m/up_m`
  (`sim/sled.cpp:287-293`) → `cg_off` (`:313`) → `patch_geometry` every substep.
  `lean_frac` drives the comfort bite, the planing lateral reward, and the
  assist release band.

**So this ladder is a repair-and-grow, not a build-from-zero.**

### 1.1 The measured defect list (this is what "out of shape" IS)

| # | Defect | Measurement | Site |
|---|---|---|---|
| 1 | Forearm longer than upper arm | 0.3609 vs 0.3048 m — inverted | GLB rig |
| 2 | ~~Thigh ≈ shin~~ **STRUCK 2026-08-16 — THIS DEFECT WAS MY ERROR** | measured 0.4109/0.4083 = ratio 1.006, which is **anthropometrically CORRECT**. Winter Fig. 4.1 (after Dempster/Drillis & Contini): thigh 0.245 H, shank 0.246 H — joint-centre thigh and shank are essentially EQUAL (0.996:1), **not 1.15:1**. Acting on this "defect" would have BROKEN a correct ratio. The legs' real fault is defect 5 (absolute length), not their proportion | — |
| 3 | ~~Shoulders narrow~~ **STRUCK AT THE JOINT LEVEL 2026-08-17 — see §3.0-BIS** | span 0.380 is the **glenohumeral JOINT** span and it is **correct for this body** (converts to 0.430 surface → H 1.660, exactly where its thigh and shank sit; realistic GHJC for 1.85 m is 0.369–0.390). The "0.470" it was failed against is a **surface** figure. Honest gap to the broad man = **40 mm, not 90**. ★ The narrowness that is actually VISIBLE is **suit volume** — outer breadth ~0.46 vs 0.640 — which is **R2c mesh work, not a joint position** | GLB rig → **mesh** |
| 4 | ~~Hips narrow~~ **STRUCK 2026-08-17 — THIS DEFECT WAS BACKWARDS** | span 0.200 is the **inter-HJC joint** span; correct for 1.85 m is **0.190**, so the rig is **6 % WIDE**. It was failed against bitrochanteric 0.191 H = 0.353, a **surface** figure — read as a joint span that implies **H = 1.047 m, a toddler**. §3 rule 4 confirms independently: widening to ±0.177 demands 9° of *inward* tibia to reach a natural stance. **DO NOT WIDEN. `sudburian_proxy.py:44` already widened it to 0.220 and must be reverted** | GLB rig |
| 5 | Reads short | leg 0.819 m under helmet top y=1.358 → ~1.6 m figure | derived |
| 6 | **Boots below the board** | `foot_L` 42 mm below `board_socket_L`; sole 117 mm below; 51 mm aft | `sled_model.cpp` |
| 7 | **Feet welded to the REST pose, not the board socket** | `:548` targets `rest_world[leg[2]]`, ignores `n_board[s]`. Boots never track the board; leg IK saturates at `dmax` 0.8182 m at full lean and the hip detaches | `:543-548` |
| 8 | **`absorb` is dead** | hardcoded 0 forever — no terrain reaction channel exists at all | `sled_model.h:31`, `:517` |
| 9 | **Render invents motion sim does not know** | +0.09 m hip shift, 0.32 rad pelvis pitch on stand; `cg_off` never sees it → visual and simulated CG disagree by up to 90 mm | `:496-501` |
| 10 | Ski-angle lie | render 25°, kernel `steer_max_rad` 0.42 = 24.06° (4 % off) | `:484` |
| 11 | Duplicated magic numbers | `0.25f` (:497) duplicates `stand_rise_m`; `kBarRad` 32° unbacked; `sag0` 0.10; head clamps ±1.4/±0.9 | `sled_model.cpp` |
| 12 | Rigid accessories on skinned parents | helmet/visor/curtain/mitts cannot deform; separate at extreme angles | `:562-566` |
| 13 | Silent partial rigs | any missing joint name → `find_node` returns −1 and the chain is skipped with **no warning** | `:318-348` |
| 14 | Degenerate rest bend-plane | hard-codes `(0,−1,0.1)` for near-extended arms — elbow direction is a guess | `:395-397` |
| 15 | No tests whatsoever | `impact render/sled_model.cpp` = 0 dependents, 0 test TUs, no GLB schema validator | graphify |
| 16 | Full CPU skin every frame | 9,069 verts × 4 weights + 2 `UpdateMeshBuffer` per prim, unconditional, no LOD, no cull | `:591-636` |

**"Floaty" is defects 6, 7, 8 and 9.** Floaty is what you get when the
extremities are not pinned to the thing they touch and there is no
ground-reaction channel. None of the four require a kernel change.

### 1.2 Hard engine constraints found

- `unsigned short` indices → **65,535-vertex ceiling per primitive** (`:288`).
- **4 bone influences per vertex**, fixed.
- Node binding is by **exact name**; suspension binds by position instead.
- raylib 5.5 vendored via FetchContent (`CMakeLists.txt:31-35`). GPU skinning
  exists in 5.5+ but is **opt-in** via a `boneMatrices` uniform shader; the
  default material shader gets CPU skinning. We already bypass raylib's model
  path entirely, so this is our own shader decision.
- raylib gives pose lerp and **nothing else**: no layered/additive blending, no
  bone masking, no blendspaces, no IK, no root motion, no sockets. Everything
  above a two-pose cross-fade, we write. That is the real cost of this program —
  budget it as engine work, not art work.

---

## §2 THE SKELETON

### 2.1 Convention

Metres. Blender +Z up, exported glTF +Y up via `export_yup=True` (never
hand-rotate to compensate — that is the classic double-rotation bug). Scale
applied to 1.0 on armature and mesh, asserted in the export script. Bone roll
recalculated global +Z once, at rig creation. **Rest pose: A-pose, arms 40° down
from horizontal**, feet 0.22 m apart, elbows 3° pre-bent, knees 2° pre-bent —
T-pose flattens the deltoid/armpit and you never get a believable neutral
shoulder on a bulky suit; the pre-bends give the IK a deterministic bend plane
so it cannot flip (and retire defect #14).

### 2.2 Naming — UE5 convention (Chad ruled: rename, R0)

Names are short (<32 chars — the red team flagged raylib's `char name[32]`
truncation, which silently collides `mixamorig:LeftHandThumbIntermediate`-class
names), lowercase `_l`/`_r`, and Mixamo→UE5 retarget maps are ubiquitous, so
every future mocap retarget is nearly free.

| Old (19) | New | Notes |
|---|---|---|
| `root` | `root` | unchanged, on ground, faces +Y |
| `pelvis` | `pelvis` | unchanged |
| `spine1/2/3` | `spine_01/02/03` | `spine_02` is the upper-body mask split |
| `neck` | `neck_01` | |
| `head` | `head` | unchanged — already the cam-follow node |
| — | `head_fp_anchor` | NEW leaf, +0.085 fwd / +0.075 up. Reserved for the FP camera; costs nothing now |
| `upperarm_L/R` | `upperarm_l/r` | |
| — | `clavicle_l/r` | NEW — carries 25-40 % of the deltoid loop, kills shoulder pinch |
| — | `upperarm_twist_01_l/r` | NEW, at 50 % |
| `forearm_L/R` | `lowerarm_l/r` | |
| — | `lowerarm_twist_01/02_l/r` | NEW, at 40 %/80 % |
| `hand_L/R` | `hand_l/r` | |
| `thigh_L/R` | `thigh_l/r` | |
| — | `thigh_twist_01_l/r` | NEW, at 50 % |
| `shin_L/R` | `calf_l/r` | |
| `foot_L/R` | `foot_l/r` | |
| — | `ball_l/r` | NEW — toe roll, needed the moment he walks |
| — | `thumb_01_l/r` | NEW — **`_r` drives the thumb throttle**, §7.2 |
| — | `mittfront_01_l/r` | NEW — four-finger mass; **`_l` squeezes the brake lever** |
| — | `scarf_01`..`scarf_06` | NEW, §5 |

**Deform total: 34 body + 6 scarf = 40.**

*Amended 2026-08-16.* The original draft said 30 body and **deferred mitt
articulation to the wrench/FPS rung**, on the reasoning that "on the handlebars
the mitts are a fixed grip and rigid is correct." **Chad's thumb-throttle spec
(§7.2) retires that reasoning.** A spring-loaded thumb lever is a continuous,
kernel-driven motion at the exact centre of frame, so `thumb_01_r` must exist
from R0. `mittfront_01_l` follows by the same argument for the brake lever.
Both are mirrored for mesh/skin symmetry and because the wrench and any carried
weapon will need them anyway. Chad approved ~30 body; this is 34, flagged, not
quietly spent.

Twist bones are not optional here. A snowsuit sleeve is a cylinder with a
**printed blue stripe** on it; linear-blend candy-wrap on a stripe is glaring in
a way it never is on bare skin. Skinning rule: all forearm skin weights go to
`lowerarm_twist_01/02`, **zero direct weight to `lowerarm` on the distal 60 %**.

### 2.2a ★ WHEN THE RENAME HAPPENS — MOVED TO R2 (recon finding, 2026-08-16)

**The draft said the rename lands in R0. That was wrong, and R0-RECON-1 caught
it before any code was written.**

The shipping rider is **not reproducible from committed source**. Every `.py` in
`assets/sled/indy650_src/` only *looks the rider up by name* —
`bpy.data.objects.get("rider_rig")` (`indy650_fitout.py:189`); there are **zero**
armature / edit-bone / vertex-group creation sites across all 33 scripts, and
`fitout.py` merely adds IK constraints and bone-parents the mitts. Per
`D:\flight_sim2\Game_loop_idea\vehicle_program\blender\BLENDER_RIG_CONTRACT.md:77`,
`rider_body` was authored as a metaball-lofted quilted monosuit, auto-weighted,
**interactively in a live GUI session**. It exists only inside `.blend` files.
Source of truth: `…\vehicle_program\blender\indy650.blend` (outside this repo).
**No export script exists anywhere on disk** — export is manual/MCP-driven.

Therefore a rename of the shipping rider **cannot** satisfy a byte-identical
rebuild gate, because the asset it belongs to is not script-reproducible.

**Re-plan:**
- **R0** builds the from-scratch scripted **grey proxy** (which is what R0 always
  said) and the **joint catalogue + validator against the CURRENT names**. The
  guard therefore exists *before* the risk does — strictly better ordering.
- **R2** performs the rename, because R2 re-authors the mesh anyway. At that
  point the new rig is script-generated and the reproducibility gate applies to
  it for real.
- §2.3's proof of no kernel harm is unchanged and still governs, whenever it
  happens.

### ★ 2.2b THE EXPORT CONTRACT — recovered from the exporting agent, 2026-08-16

The procedure that produced the shipping `indy650.glb`, supplied by the agent
that ran it. **This is the only record of it that exists.** R2 must turn this
into `export_indy650.py`; until then it is the manual contract.

```python
bpy.ops.export_scene.gltf(
    filepath=r"...\assets\sled\indy650.glb",
    export_format='GLB',
    use_selection=False,
    use_visible=False,      # <-- SEE THE TRAP BELOW
    use_renderable=True,
    export_apply=True,      # <-- ★ WRONG. IT IS False. SEE BELOW.
    export_cameras=False,
    export_lights=False,
    export_yup=True)
```
Menu equivalent: File ▸ Export ▸ glTF 2.0, glTF Binary (.glb); Include ▸ Limit
to → **Renderable Objects** only (leave *Visible Objects* UNCHECKED); Transform ▸
+Y Up; Data ▸ Mesh ▸ **Apply Modifiers OFF**.

### ★★ CORRECTION — `export_apply` IS `False` (measured 2026-08-17, re-verified 2026-08-17)

**The line above is the single most dangerous error in this document, and it was
copied into `assets/sled/indy650_src/export_indy650.py` at R0.** Anyone who runs
that script as committed ships a machine nobody has ever seen.

Exporting with `export_apply=True` produces **+25,636 triangles across 17 nodes**
(4,643,052 bytes against the shipped 4,116,100): `pan_skin` +16,620, both skis
+2,048 each, plus the boards, footwells, tunnel and rear panels. **Every one is
machine; none is rider.** Those parts carry BEVEL and SOLIDIFY modifiers.

The proof does not need Blender — the shipped asset's own triangle counts are the
**raw** geometry, and you can read them straight out of the GLB:

| node | raw polys | ×2 | shipped tris |
|---|---|---|---|
| `pan_skin` | 4,402 | 8,804 | **8,804** |
| `snow_flap` | 6 | 12 | **12** |
| `board_L` | 58 | 115 | **115** |

With `export_apply=False` the re-export reproduces the committed file to **200
bytes**, topology identical node for node.

**Also missing from the recovered contract and now pinned:
`export_rest_position_armature=True`.** Without it the armature exports in its
*evaluated* pose. Harmless today, because the rest pose *is* the evaluated pose —
but the moment R2 moves the rig, the evaluated pose no longer satisfies its own IK
constraints, and you export an IK-solved pose against rest-derived inverse-bind
matrices. That is a silently double-transformed skin, and it presents as a
weighting bug that no amount of weight painting will fix.

> **★ THE CONSEQUENCE NOBODY HAS RULED ON: the bevels and solidify sitting in the
> `.blend` have never been in the game.** The machine you have signed off across
> L1–L5 is the un-bevelled one. That is an L5/L6 **art** question for Chad — is
> the machine supposed to have them? — not a rider one. Filed, not acted on.

**★★ THE TRAP THAT DELETES THE RIDER.** `use_visible=True` **silently drops 10
nodes** — `rider_body`, `rider_boot_L/R`, `rider_helmet`, `rider_mitt_L/R`,
`rider_shield`, `rider_chin_curtain`, `snow_flap`, `pan_front_bar`,
`ik_pole_L/R`. They are viewport-hidden but `hide_render = False`, so
"Renderable" keeps them and "Visible" does not. **A broken export shipped this
way once and was caught only by diffing node lists against the previous GLB.**
Seven of the ten are the rider. This lands squarely on this ladder: an export
with the wrong checkbox produces a machine with no man on it, and nothing warns.

**Known gap in the recovered contract:** animation, sampling, frame step and
compression were left at Blender defaults and were never specified. `export_indy650.py`
must pin them explicitly (§8's list: `export_force_sampling=True`,
`export_frame_step=1`, `export_optimize_animation_size=False`) or the export is
not deterministic. Do not simply copy the call above — it is a record of what
happened, not a specification.

**MANDATORY POST-EXPORT CHECK — do not trust the export, diff it.** Parse the
GLB's JSON chunk and assert: (a) every node name `render/sled_model.cpp` looks
up is present; (b) `skins == 1` — proving the rider is still RIGGED and not
frozen; (c) the node set diffed against the previous committed GLB, so any
silent drop appears as a removal. **R0b's validator test is exactly this check,
moved into CI where it cannot be forgotten.**

**Source of truth — RULED.** `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\indy650.blend`.
Confirmed by more than timestamps: every `indy650_V*.blend` under
`D:\indy_650_shroud\` is written with `save_as_mainfile(copy=True)`, which by
construction never changes the session filepath — those are dated snapshots and
**can never be the working file**. Treat `D:\indy_650_shroud\` as the modelling
scratch repo.

### ★ RESOLVED — HOW THE ASSET PROPAGATES (evidence supplied 2026-08-16)

**By git. Not by copying. There is ONE repo and TWO working trees.**

| tree | what it is | branch |
|---|---|---|
| `D:\flight_sim2\seads-recon` | a clone of `seads_sandbox1.git` | `main` |
| `D:\seads_sandboxes\winter-gi` | a **linked worktree** of `D:\flight_sim2\seads` | `sandbox/gi4-ride` |

Both copies of `indy650.glb` are byte-identical and committed; winter-gi is
behind=0 against `origin/main` and already contains main's `b88991269`. The
mechanism is: **export → commit on one branch → the other tree receives it by
fetch/merge.** (This afternoon: export into seads-recon → commit → push main →
merge `origin/main` into `sandbox/gi4-ride`, add/add conflict on the GLB resolved
to main's.)

**★ AMENDED by the exporting agent's own account (2026-08-16). The rule above
was MY inference and it is WRONG. The actual flow this session was:**

```
Blender live file (…\vehicle_program\blender\indy650.blend)
  │  bpy.ops.export_scene.gltf →  D:\indy_650_shroud\_candidate.glb   (scratch)
  │  ★ MANUAL cp — a human/agent copy. No script, no hook.
  ▼
seads-recon\assets\sled\indy650.glb        (worktree of MAIN)
  │  git commit + push origin main         →  b88991269
  ▼
winter-gi\assets\sled\indy650.glb          (worktree of sandbox/gi4-ride)
     received by  git merge origin/main    →  a5fda014c
```

**Canonical direction: export → `main` (in seads-recon) → commit/push → merge
`main` into the sandbox branch. Sandbox branches RECEIVE the asset by merge;
they are NOT an export target.** This matches the prior session's pair
(`ca24a1cd7` asset on main, then `32c7b35e1` merge into sandbox/gi4-ride).

So there IS a hand-copy step, but it is Blender-scratch → main, never
tree → tree. Both tracked copies are currently byte-identical, same git blob
`f7a56696d3…`, 4,116,100 bytes.

**★ OPEN — CHAD'S DECISION, BEFORE R2 EXPORTS ANYTHING.** The exporting agent
explicitly declined to generalise its own precedent: it followed main because
that is what the git history showed, found no documented rule, and flagged that
it does not know whether copying straight into a sandbox worktree is acceptable
when the change is sandbox-only. **The Sudburian's GLB is sandbox work on an
unsigned ladder.** Two options:
  (a) follow the machine's precedent — export to `main` via seads-recon, push,
      merge down. Safe, consistent, but publishes unsigned character art to main.
  (b) commit the character GLB on `sandbox/gi4-ride` only, and merge to main
      when the rung is signed. Keeps main clean; departs from precedent.
**Recommend (b), but it is Chad's call and R2 must not proceed on a guess.**

### ★★ THE GHOST GLB — IT HAS ALREADY BITTEN ONCE

`D:\flight_sim2\Game_loop_idea\vehicle_program\blender\indy650.glb`
— 2,723,156 bytes, md5 `4936c29d`, Aug 15 00:29.

It sits beside the `.blend`, so it *looks* authoritative. It is **not in any git
repo** (`Game_loop_idea` is untracked scratch, confirmed) and it is a stale
export ~1.4 MB smaller than current.

**Proof it already caused this exact failure:** during this afternoon's merge,
`winter-gi` held an *uncommitted* `indy650.glb` of exactly 2,723,156 bytes, md5
`4936c29d` — the same file. Someone had hand-copied the scratch export into the
build tree. It was backed up to
`D:\seads_sandboxes\_backup_20260816\indy650.glb.uncommitted.bak` before the
merge resolved to main's version.

**Standing hazard:** as long as that file is named `indy650.glb` it will be
mistaken again. Recommended (Chad's file, Chad's call): rename it to
`indy650_scratch.glb`, or delete it.

### 2.3 THE RENAME AND THE KERNEL — proof of no harm

The rename touches exactly two things:

1. Joint names inside `indy650.glb` (regenerated from the Blender source).
2. The `find_node(...)` string literals in `render/sled_model.cpp:318-348`.

It touches **nothing** in `sim/`, `control/`, `world/`, or the tape. Verified:
`sim/sled.h` and `sim/sled.cpp` contain no bone-name string; the tape pins
`rider_mass_kg`, `lean_lat_seated_m`, `lean_lat_stand_m`, `lean_fwd_max_m`,
`lean_aft_max_m`, `stand_rise_m`, `tuck_drop_m`, `lean_tau_s`, `lean_rate_ms`,
`lean_return_tau_s`, `stand_cda_add_m2` and the three state metres — parameters
and metres, never joints.

### ★ 2.3a THE L/R TRAP — READ THIS BEFORE R2 (verified 2026-08-16)

**In `indy650.glb`, `_L` means −X across the WHOLE asset, and on the rider that
is his anatomical RIGHT.**

Verified from the machine's own geometry, not the rider's: skis/spindles/wearbars
at z = +0.86…+0.985, `idler_rear` at z = −1.045 → **front is glTF +Z**,
unambiguously. glTF is +Y up, right-handed, so a rider facing +Z has his left at
+X. But `upperarm_L` x = −0.190, `thigh_L` −0.100, `hand_L` −0.315, `foot_L`
−0.300 — **all on his right.**

**Nothing is broken today, and this is the important part.** The convention is
asset-wide and self-consistent: **36 of 36** machine nodes ending `_L` are at
negative X (`ski_L`, `board_socket_L`, `grip_socket_L`, `CH_susp_L`), so
`foot_L` correctly pairs with `board_socket_L` and the runtime is fine.

**The trap is the R2 rename.** Renaming only the *rider* to anatomically correct
`_l`/`_r` de-syncs it from the DO-NOT-RENAME machine sockets below, and
`foot_l` (his true left, +X) must then bind to **`board_socket_R`**. Write that
crossover mapping down explicitly in R2, or defect 7's boot fix lands on the
wrong board — a bug that would look like a mysterious 0.6 m foot offset.

Also for R2: the R0 proxy faces **−Z** while the sled rider faces **+Z**. Seating
the proxy needs a 180° yaw, after which its `_l` lands at +X — anatomically
correct, and therefore **opposite the shipping convention**. Both facts are one
note.

**DO NOT RENAME**, on pain of silently breaking steering/suspension/IK — these
are machine-side, not rider-side:
`CH_steer_pivot`, `CH_susp_L`, `CH_susp_R`, `CH_susp_T`, `CH_cam`, `ski_L`,
`ski_R`, `grip_socket_L`, `grip_socket_R`, `board_socket_L`, `board_socket_R`.

**Guard, and it is the direct answer to Chad's caveat:** replace name-by-name
`find_node` with a **declared joint catalogue** modelled on
`aircraft_node_specs()` (`render/rig.h:149`), plus a validator test that FAILS
LOUD if any declared joint is absent. Today a missing joint silently disables a
limb (defect #13). After R0 it cannot: the rename is guarded by a test that did
not exist before it, so the rig is *safer* after the rename than before it.

---

## §3 PROPORTIONS — the big guy

Target: a heavy-set adult male, ~1.85 m bare, in a bulky 2-pc snowsuit. Head
0.245 m → 7.55 heads bare; with helmet and boots the silhouette reads ~6.9
helmet-heights, which is the squat/powerful proportion we want.

| Segment | Bare (m) | Suit silhouette (m) |
|---|---|---|
| Stature | 1.850 | 1.905 (boots +30, helmet +25) |
| Ground → hip **joint** (= trochanter height, 0.530 H) | 0.980 | 1.010 |
| Thigh (hip→knee) | **0.453** | — |
| Shank (knee→ankle) | **0.455** | — |
| Ankle height | 0.072 | 0.100 |
| Hip **JOINT** → shoulder **JOINT** (0.288 H) | **0.533** | — |
| Shoulder joint → C7 (0.052 H) | 0.096 | — |
| Upper arm | **0.344** | — |
| Forearm | **0.270** | — |
| Hand | 0.200 | 0.240 (mitt) |
| Biacromial — **SURFACE**, acromion↔acromion | **0.470** | **0.640 outer** |
| Shoulder **JOINT** span — GHJC↔GHJC | **0.420** | — |
| Hip **JOINT** span — inter-HJC (0.102 H) | **0.190** | — |
| Bitrochanteric — **SURFACE**, trochanter tips (0.191 H) | 0.353 | — |
| Chest breadth / depth | 0.360 / 0.250 | 0.430 / 0.310 |
| Waist breadth | 0.370 | 0.440 |
| Thigh girth | 0.66 | 0.75 |
| Upper-arm girth | 0.40 | 0.52 |
| Foot L×W | 0.280 × 0.105 | 0.320 × 0.135 (rubber boot) |

Bold entries are the direct repairs of defects 1, 3, 4 and 5. Upper arm now
exceeds forearm (defect 1).

### ★★★ 3.0-BIS — THE SURFACE/JOINT-CENTRE CORRECTION (2026-08-17). READ BEFORE USING ANY ROW.

**Four rows above were relabelled and four added. NO WIDTH CHANGED.** This is a
units correction, not a proportions change, and it kills two "defects" that were
never real. It is the *third* time this exact error has cost this ladder a rung,
so it is written down at full strength.

**THE ERROR.** A **surface landmark** measurement (skin/bone, e.g. acromion to
acromion) is not a **joint-centre** measurement (e.g. glenohumeral centre to
glenohumeral centre). The table used to say `Biacromial (shoulder joints) 0.470`
— which instructs a rigger to place shoulder *joints* 0.470 apart from a figure
that measures the *acromia*. The acromion physically overhangs the humeral head
laterally; there is no anatomy in which it is medial. The two differ by roughly
**25 mm per side**.

**THE PROOF IS §3's OWN CLOSURE TEST** — the method §3.0 credits with catching
both previous errors. §3.0(b) derives the 0.640 outer shoulder from the sleeve
girth by centring the deltoid at ±0.235:

| | 0.470 read as **SURFACE** | 0.470 read as **JOINTS** |
|---|---|---|
| GHJC span | 0.420 | 0.470 |
| Biacromial | 0.470 | 0.520 |
| Bideltoid bare (×1.22) | 0.573 | 0.634 |
| Outer shoulder, suit (+35/side) | **0.643** | 0.704 |
| **vs §3's own stated 0.640** | **✔ closes to 3 mm** | ✗ **64 mm over** |

**The surface reading closes to 3 mm. The joint reading misses by 64 mm.** So
§3.0(b) was *using* 0.470 as a surface figure while the table two lines above
labelled it a joint figure. Same number, two incompatible meanings, one section.

**WHAT THIS KILLS:**

- **★ DEFECT 3 ("shoulders narrow", span 0.380) is STRUCK at the JOINT level.**
  The shipping rig's 0.380 GHJC span is *correct for the body it is attached to*
  (convert to surface, 0.430, and it backs out to H = 1.660 — landing on the same
  man its thigh and shank describe). Realistic GHJC for a 1.85 m male is
  0.369–0.390. **The rig is dead centre of the band.** The honest gap to §3's
  deliberately-broad man is **40 mm, not the 90 mm** that "0.470 vs 0.380"
  implies. Widening the joints to ±0.235 would produce a wide-skeleton,
  narrow-suit man and would **not** fix what you can actually see.
- **★ The "narrow shoulders" you SEE are MESH, not rig.** Measured off
  `rider_body`: outer suit breadth at shoulder height ≈ **0.46 m** against §3's
  **0.640** — 180 mm short. Torso breadth at `spine2` 0.284, at `pelvis` 0.200,
  against suit chest 0.430 / waist 0.440. **This is R2c suit volume. It is not a
  joint position, and no amount of moving joints will fix it.**
- **★ DEFECT 4 ("hips narrow", span 0.200) is STRUCK.** Correct inter-HJC for a
  1.85 m male is **0.190**. The rig's 0.200 is **6 % WIDE**. It is the last
  number in the rig that wants widening. **DO NOT WIDEN THE HIPS.**

  *The reductio, and it is the cleanest proof here:* read Drillis & Contini's
  hip breadth 0.191 H as a joint span and the rig's 0.200 backs out to
  **H = 1.047 m, a toddler**, while its legs say 1.66–1.68. Convert it properly
  (0.200 + 0.120 trochanter offset = 0.320) and it backs out to **1.675 —
  landing exactly on the legs.** §3 rule 4 confirms it independently: hips at
  ±0.100 with feet at ±0.11–0.13 gives a natural 3–4° tibial splay, while hips at
  bitrochanteric ±0.177 would demand 9° of *inward* tibia — a knock-kneed man.

- **★ `Pelvis → C7 0.533` WAS MISLABELLED.** 0.533 is Winter's **hip-joint-centre
  → glenohumeral-joint-centre** distance (0.288 H), not pelvis→C7. Proof: it
  closes exactly against §3's own 0.980 (0.530 H) to put the shoulder at 0.818 H,
  which is Winter's glenohumeral height. True pelvis→C7 is 0.340 H = **0.629 m**,
  96 mm larger. Taking the old label literally forces an anatomically impossible
  **87 mm of neck above C7** — which `sudburian_proxy.py:105,196-198` does
  verbatim, its 87 mm "neck" silently standing in for the real 96 mm
  shoulder→C7 gap. The stack accidentally closes; the label is wrong; **a
  re-author that honours the old label puts the neck root 96 mm too low.**

**★★ AND THE ONE REAL DEFECT, WHICH IS 71 % OF EVERYTHING.** Measured
like-for-like, the shipping rig's **hip joint → shoulder joint is 0.394 m against
0.533 — a 139 mm deficit.** Unrolled, the rig stands **1.709 m** to the helmet
crown against the 1.905 target: **196 mm short, and 139 mm of that is the torso
alone.** Ranked, the rig work is:

| # | change | from → to | buys |
|---|---|---|---|
| 1 | **torso, hip→shoulder** | 0.394 → 0.533 | **+139 mm — 71 % of the whole defect** |
| 2 | **forearm** | 0.361 → 0.270 | defect 1; the inversion is *entirely* the forearm |
| 3 | upper arm | 0.305 → 0.344 | +39 mm reach |
| 4 | shank / thigh | 0.408/0.411 → 0.455/0.453 | +89 mm stature |
| 5 | shoulder joints | 0.380 → 0.420 | 20 mm/side, cosmetic |
| 6 | **hip joints** | **0.200 → 0.190, or leave** | **never widen** |
| 7 | **mesh: torso + deltoid volume** | ~0.46 → 0.640 outer | **this is what makes him read broad — R2c** |

### ★ 3.0-TER — STATURE IS 1.850 AND IT IS RULED (Chad, 2026-08-17)

A hypothesis that §3 was secretly describing a ~2.0–2.1 m man was **tested and
REFUTED.** Every row was back-solved for the stature it implies under its own
Winter/D&C fraction:

> stature 1.850 · ground→hip 1.849 · thigh 1.849 · shank 1.850 · ankle 1.846 ·
> hip→shoulder 1.851 · upper arm 1.849 · forearm 1.849 · hand 1.852 · foot 1.842

**Ten rows, mean 1.8487, total spread 9.7 mm (±0.26 %).** §3 is a Winter Fig. 4.1
table scaled to H = 1.85 to three decimals. **1.850 is the single most
corroborated number in this document.** The only deviant row was biacromial, and
read as a *surface* figure it gives 1.815 — the closest value to the cluster, 2 %
low, not 13 % high. The anomaly was the units, never the stature.

**Chad ruled 1.85 m, §3 as written.** At 1.85 he stands **1.905 m on screen** with
boots and helmet against **1.709 today — he grows 196 mm, and most of it is
torso.** If a bigger man is ever wanted, it is a **declared ruling that rescales
the whole table**, never a back-derivation from one anomalous row.

### ★ 3.0-QUATER — THE SUIT-BULK CONTRADICTION, AND WHICH SIDE WINS

§3 states suit bulk twice and the two disagree:

| segment | girth rows | implied radial push | the "+push" list | girth the push implies |
|---|---|---|---|---|
| upper arm | 0.40 → 0.52 | **19.1 mm** | +40 mm | 0.651 |
| thigh | 0.66 → 0.75 | **14.3 mm** | +40 mm | 0.911 |
| torso breadth | 0.360 → 0.430 | 35 mm | +35 mm | ✔ closes |
| chest depth | 0.250 → 0.310 | 30 mm | +35 mm | ✗ 10 mm short |

The push list is **2.1× (arm) and 2.8× (thigh)** heavier than the girth rows.
**RESOLVED IN FAVOUR OF THE GIRTH ROWS**, by the closure test and not by
preference: §3.0(b) derived the 0.640 outer shoulder *from* the 0.520 sleeve
girth, and 0.640 is a stated figure that closes to 3 mm. Use the push rule
instead and the outer shoulder becomes 0.677, contradicting §3's own number.
**The girth rows are the authority; treat the "+35/+40/+25/+30/+50" list as an
approximate sculpting hint, not a measurement.** Reversible if Chad prefers the
bulkier read — but then 0.640 must move too, and it is his call, not a silent one.

### ★ 3.0-QUINQUIES — SMALLER THINGS THAT WOULD STILL BITE

- **Rule 1's parenthetical `(0.56 vs 0.44)` is STALE** — 0.560 was superseded by
  0.640 in §3.0(b). The rule itself holds under every reading; only the number in
  brackets is wrong. Read it as **(0.640 suit vs 0.440 suit)**.
- **Rule 2 cites a `belly depth 0.30` row that does not exist** in the table.
  `sudburian_proxy.py:53` supplies `BELLY_D = 0.300` from nowhere. Unverifiable
  as written; treat 0.300 as provisional.
- **There is no hand BONE in the shipping rig.** `hand_L` is the IK chain **tip**,
  welded to `grip_socket_L`. Adding §3's 0.200 hand as a real segment silently
  redefines what the arm chain's tip *is* and can shift the grip weld by up to
  0.2 m. Measured, `rider_mitt_L` projects only **144 mm** distal of `hand_L`
  against a 240 mm mitt spec.
- **Rule 5's neck, 0.160 m diameter, is 0.503 m circumference (19.8 in)** —
  stylised-high even for a big man in a collar (a large male neck is ~0.44 m).
  The shipping mesh's neck is **0.078 m** — half the spec. Either way the gap is 2×.
- **Head 0.245 backs out to H = 1.885**, the one bare row outside the cluster. 4.5 mm.
- **Boot width** measures 0.1662 against the 0.135 spec (+31 mm). Length is exact.
- **§2.1's A-pose does not exist in the shipping asset.** The bind pose is the
  riding crouch with feet at ±0.300, stance **0.600 m**, forced by
  `board_socket_L/R`. §3 rule 4's 0.22–0.26 stance can *never* hold on the
  machine. **Consequence for R2: changing the bind pose invalidates every
  `rest_tip_minus_socket_m` in `render/rider_rig.cpp` and every rest-pose
  bend-plane capture.** Plan for it; do not discover it.

### ★ 3.0 TWO CORRECTIONS TO THIS TABLE (2026-08-16, sourced)

Caught by R0a's build arithmetic and adjudicated independently by R0a-VERIFIER
against real anthropometric sources. **Both were errors in my draft, not in the
build.**

**(a) Shank 0.395 → 0.455.** The draft's "real thigh:shank ≈ 1.15:1" is not
supported by any anthropometric source. Winter, *Biomechanics and Motor Control
of Human Movement*, Fig. 4.1 (after Dempster 1955/59 via Drillis & Contini 1966)
gives thigh **0.245 H** and shank **0.246 H** — joint-centre segments are
essentially equal, ratio 0.996:1. At H = 1.85 m: thigh 0.453, shank 0.455.

  The table now closes exactly, which is the proof it is right:
  **0.453 + 0.455 + 0.072 = 0.980 m = §3's own ground→hip figure.**
  With shank 0.395 it summed to 0.920 and left a 65 mm stature deficit that had
  to be hidden somewhere. Nothing is hidden now.

  Consequence: §1.1 defect 2 is **struck**. The shipping rider's leg ratio
  (1.006) was already correct; acting on the "defect" would have broken it.

**(b) Outer shoulder 0.560 → 0.640.** 0.560 was internally inconsistent with
this table's own suit girths: upper-arm suit girth 0.520 → sleeve radius
0.0828 m; a deltoid centred at ±0.235 puts the outer surface at ±0.318, i.e.
0.636 span, and the "+50 mm shoulder cap" push argues for more. Real
bideltoid:biacromial ≈ 1.22 → ~0.57 m **bare** before any suit; +35 mm suit per
side → **0.640**. Biacromial 0.470 itself is sound (Drillis & Contini 0.259 H =
0.479) and is kept — it is a deliberately broad man, which is what we want.
§3 rule 1 (shoulder ≥ waist 0.440) only gets stronger.

**Method note worth keeping:** both errors surfaced because the proxy was built
to the numbers *and the numbers were then required to close*. A table that is
only read never reveals its own contradictions; a table that is BUILT does.

**Suit bulk, as a normal-push before sculpting:** torso +35 mm, upper arm +40,
forearm +25, thigh +40, calf +30, **shoulder cap +50** (a jacket bunches most at
the deltoid).

**Creases that must be modelled and must not smooth away:** elbow inner (2
loops), knee front (3), hip crease, waist/belt line, armpit gusset, boot ankle
bellows.

**Must NOT collapse:** shoulder cap, chest volume when the arms come in, the
belly when he leans forward (a suit *folds over the belt*, it does not vanish),
the mitt cuff.

**Reads "solid", not "fat blob"** — the five rules, all checkable:
1. shoulder breadth ≥ waist breadth (**0.640 suit vs 0.440 suit** — the old
   "0.56" was superseded by §3.0(b) and is stale). The instant waist exceeds
   shoulder he reads soft.
2. Mass sits high: chest depth 0.31 > belly depth 0.30, never the reverse.
3. Hard silhouette breaks at belt, cuffs, boot tops, helmet chin. A blob has
   continuous curvature; a solid man has segments. **Our costume gives us all
   four breaks for free** — see §4.
4. Stance 0.22-0.26 m. Wider reads waddle.
5. Short thick neck (0.16 m dia) but the head is **not** shrunk. Small head on a
   big body is the cartoon-fat cue.

### 3.1 The mass question — DEFERRED, NOT FORGOTTEN

Kernel `rider_mass_kg = 87.5` (`sim/sled.h:505`). The figure above is a bigger
man than 87.5 kg of bare body — but 87.5 kg **plus suit, helmet, boots and mitts
is a fair read** for this silhouette, and the number is pinned in the tape and
feeds `cg_off` and every `[rider]` test. **v1 does not touch it** (§0.2). If
Chad later wants him heavier in the *physics*, that is a kernel rung with its own
tape re-pin, argued separately.

---

## §4 THE COSTUME (Chad's spec, 2026-08-16)

| Part | Spec | Colour source |
|---|---|---|
| Suit | black, **2-piece**, snowboarder-cut jacket | black |
| Stripes | side stripes, outer arm + outer leg | **`render::kComplementBlue` = (0.12, 0.57, 1.00) = #1F91FF** |
| Hood | bunched **behind** the helmet, not worn | black |
| Helmet | **black + red**, matched to the Indy 650 | red pulled from the sled's own material — no retyped literal |
| Visor / chin curtain | as today (`rider_shield`, `rider_chin_curtain`) | — |
| Mitts | **beige-yellow leather** | new constant, documented |
| Boots | **green rubber** | new constant, documented |
| Pant break | snowpants cover **most** of the boot shaft | — |
| Scarf | trails, lifts with speed | §5 |

**THE BLUE IS DERIVED, NOT PICKED, AND IT ALREADY EXISTS.**
`render/team_color.h:72` — `kComplementBlue{0.12, 0.57, 1.00}` is the exact 180°
HSV complement of `kSlagOrange{1.00, 0.55, 0.12}` at equal S and V, enforced by
`test/unit/test_team_color.cpp` to 1e-9 and re-checked at startup by
`config/load_world.cpp`. The header records the trap explicitly: the
eye-plausible channel-reversal (0.12, 0.55, 1.00) is 181.36° away and is **not**
the complement. The snowsuit reads the constant (§0.3).

### ★ 4.1 THE SUPPLIED REFERENCES (Chad, 2026-08-16) — AND ONE OPEN QUESTION

Two references were supplied. **Neither is a photo of a big man in a suit; both
are product shots of the garments.** That is fine for R2's *construction*
(panels, seams, break lines, hardware) and it is what geometry needs. It does
NOT settle silhouette-on-a-heavy-body, which §9 says only Chad can judge anyway.

**(a) THE SUIT — Klim Ripsa One Piece Snow Suit, grey, "Short" cut.**
`fc-moto.com/en-ca/p/klim-ripsa-one-piece-snow-suit-grey-short-xs-KLM-3936-000-310-660`

Construction facts that R2 must model, taken from the product copy:
- **One piece.** Laminated GORE-TEX Performance shell, 100 % polyester over
  100 % nylon lining. Grey (the line also runs black / blue / orange / yellow
  and one black-blue-yellow colour-block).
- **Removable adjustable GORE-TEX hood** + microfleece collar. This is a real
  gift: §4 already wants the hood **bunched behind the helmet**, and a removable
  hood on a drawcord is exactly the bunched-at-the-nape shape.
- **Adjustable waist straps** — the one-piece's substitute for a belt line.
- **Full-length side leg zippers**, pit zips, 3M Scotchlite reflective.
- **Durable overlays + pads on the knees**, inner-boot panels, Velcro boot
  gaiters with a retention loop.
- Elastane wrist gaiters with thumb holes; removable suspenders; 2 hand +
  1 chest + 1 internal pocket.

**(b) THE BOOTS — a Baffin pull-on rubber boot, and it is BLACK.**
Tall calf-height shaft, plain pull-on (no laces, no speed hooks), a single
moulded shaft crease band near the top, a lug outsole with a distinct heel
break, and a Baffin badge at the ankle and the shaft top. Shaft reads ~0.36-0.40 m
tall — **much taller than §3's 0.320 × 0.135 rubber-boot figure implies**, and
that changes where the pant break can sit.

**(c) THE HELMET — CKX Mission, and it is a FREE-FACE, not a full-face.**
`ckxgear.com/pages/mission-snowmobile-helmet`

- **Free-face** — CKX's own category name for an open-shell, MX-style lid: **no
  chin bar.** Wind-tunnel developed, "the lightest of its category".
- **Adjustable, removable aerodynamic peak** (a brow visor, not a face shield).
- **Goggle-based**, not shield-based. The face-front is a goggle, and CKX pairs
  it with an **adjustable removable breath deflector in 3 sizes** plus their AMS
  anti-fog system.
- Ratchet pushbutton retention. Colourways include a **Carbon Alaska in Glossy
  Red** — which is exactly §4's "black + red, matched to the Indy 650", so the
  helmet colour spec survives the reference change untouched.

**★ THIS ONE CHANGES GEOMETRY, NOT JUST COLOUR.** The existing GLB carries
`rider_helmet` + `rider_shield` + `rider_chin_curtain`, which is a **full-face
with a flip-down shield**. A CKX Mission is a different object: peak, goggle
strap over the shell, and a soft deflector where the chin bar used to be. R2
re-authors all three nodes — `rider_shield` becomes a **goggle** (lens + strap),
`rider_chin_curtain` becomes the **breath deflector**, and a **peak** is new
geometry. The node names can stay; the meshes cannot.

**★ AND IT PUTS A LOAD ON §6/R2's SAFETY ARGUMENT.** R2 currently justifies
"helmet stays on always" by saying it *"deletes faces and bare hands, the two
things a solo pipeline never gets right."* A full-face delivered that for free.
A **free-face does not**: goggles take the eyes and the deflector takes the nose
and mouth, but the **jaw, cheeks and chin are open shell**. Standard riding
practice covers exactly that with a **balaclava**, and that is the fix — a black
balaclava under the helmet keeps the no-face guarantee intact, costs one simple
mesh, and is what a Sudbury rider actually wears at −25 °C anyway. **R2 must
model the balaclava, or the "no face" guarantee is silently lost.** Flagged here
so it cannot be discovered late.

### ★ THE ONE QUESTION THIS RAISES — R2 ART ONLY, BLOCKS NOTHING BEFORE IT

**Three §4 rows now disagree with the references Chad chose.** These are almost
certainly deliberate — he was asked for reference and supplied specific products
— but §0.3 and the standing "change only what Chad names" rule mean they get
asked once, not silently re-derived:

| §4 as written | The supplied reference | Consequence if the reference wins |
|---|---|---|
| Suit **black** | Ripsa **grey** | `kComplementBlue` stripes read *better* on grey than on black; a black suit against a black helmet and black boots was always going to flatten |
| Suit **2-piece, snowboarder-cut jacket** | Ripsa **one-piece** | **This is the structural one.** §3 rule 3 spends the *belt line* as one of its four "solid man" silhouette breaks. A one-piece deletes it and leaves waist straps + the full side zip. The side zip is a straight vertical line down the outer leg — **the same line §4 puts the blue stripe on**, so the stripe can BE the zip line and the two specs merge cleanly. But the man loses a horizontal break at the waist and R2 must win it back from the waist straps and the chest pocket seam, or he reads as one continuous tube |
| Boots **green rubber** | Baffin **black** | Green was the only warm/odd colour in the costume. Black boots + black helmet + grey suit + beige-yellow mitts + blue stripe still has a focal point (the mitts, at the bars, which is where the eye goes anyway) |
| Helmet **full-face**, `rider_shield` a flip shield, `rider_chin_curtain` a curtain | CKX Mission **free-face**: peak + goggles + breath deflector | Three meshes re-authored, one new (peak), **and a balaclava added** to keep the no-face guarantee. Colour (black + red) is unaffected — CKX ships a glossy-red carbon |

**Recommendation: take all three from the references** — grey one-piece Ripsa,
black Baffins — and pay for the lost belt line by making the waist straps and the
boot tops hard, deliberate breaks rather than incidental detail. But **Chad
rules it**, and R2 does not start art until he does. `kComplementBlue`,
beige-yellow mitts and the black/red helmet are untouched by any of this.

### ★★ RULED BY CHAD 2026-08-17 — THIS QUESTION IS CLOSED. R2 ART IS UNBLOCKED.

**The references win on the SUIT and the BOOTS. The HELMET stays FULL-FACE.**
He was asked the helmet separately and overruled reference (c) explicitly, so
this is not an inconsistency to be re-litigated — it is a deliberate split.

| row | RULING | consequence |
|---|---|---|
| Suit | **Grey, ONE-PIECE Klim Ripsa** | §4's "black, 2-piece, snowboarder-cut jacket" is SUPERSEDED. Grey also fixes a real problem: black suit + black helmet + black boots was going to flatten the whole figure |
| Boots | **BLACK Baffin pull-on rubber** | §4's "green rubber" is SUPERSEDED. Tall shaft ~0.36–0.40 m, single moulded crease band, lug outsole with a heel break |
| Helmet | **FULL-FACE — reference (c) OVERRULED** | `rider_helmet` / `rider_shield` / `rider_chin_curtain` keep their present *kinds*: shell, flip-down shield, chin curtain. **No peak. No goggles. No breath deflector.** Black + red, unchanged |
| Balaclava | **NOT NEEDED** | It only existed to rescue the no-face guarantee from a free-face lid. The chin bar delivers it for free, exactly as today |
| Stripes / mitts | unchanged | `kComplementBlue` side stripes, beige-yellow leather mitts |

**What this ruling BUYS, and it is more than it looks:** the three
geometry-changing items in §4.1 were all helmet items. Overruling (c) deletes a
new mesh (peak), two re-authors (shield→goggle, curtain→deflector) and the
balaclava — **four meshes of work, and the four riskiest ones**, since a
goggle-strap-over-shell read and a soft deflector are exactly the fiddly organic
shapes §9 says agents get wrong. R2c is materially cheaper and materially safer
than §4.1 assumed.

**What it COSTS, and R2b must pay it deliberately:** the one-piece deletes the
**belt line**, which §3 rule 3 spends as one of its four "solid man" silhouette
breaks. Remaining breaks are cuffs, boot tops and the helmet chin — three, not
four, and the missing one is the *horizontal* break at the waist, which is the
one that stops him reading as a continuous tube. **R2b wins it back from the
Ripsa's adjustable waist straps and the chest pocket seam, modelled as hard,
deliberate breaks and not incidental detail.** This is a named silhouette
obligation on the blockout rung, not a texture-stage afterthought — it is form,
so it has to exist in grey.

**One clean merge falls out:** the Ripsa's full-length side leg zipper is a
straight vertical line down the outer leg, which is exactly where §4 puts the
blue stripe. **The stripe IS the zip line.** Two specs, one piece of geometry.

**Costume note that is also a defect fix:** the snowpants-over-boot break sits
exactly where the ankle skinning is worst, and it hides the bellows. The belt
line, cuffs, boot tops and helmet chin deliver all four "solid man" silhouette
breaks from §3 rule 3 for free. This costume is unusually well-suited to the
form we want.

---

## §5 THE SCARF

Six-segment bone chain `scarf_01..06`, skinned, **driven entirely render-side**,
carrying **no authored keyframes**. Trail direction from the sled's
velocity vector in body frame; lift angle rises monotonically with speed as
Chad specified; a light damped-spring lag per segment for the whip.

**It never writes back to sim** (§0.1), so it cannot touch a tape. It is the
cheapest thing on this whole ladder and it is the only element on screen that
*shows* velocity — it will do more for felt speed than anything else here.

Open detail for R2: whether it also reacts to the roost/spray field. Cheap to
add, deferred so it does not delay the rung.

**★ R2c-7s BUILT 2026-08-19 (overnight, Chad asleep) — `docs/SCARF_SPEC.md` is
the spec of record.** The six bones were MEASURED present in the shipped skin
(children of `neck_01`, 0.080 m each) with ZERO skinned vertices; the solver
(`render/trail_chain.*`, pure, render_core, the thing R4's superman re-uses)
drives those six bones every frame and a procedural two-sided ribbon is drawn
off the same frames — the ONE departure from "skinned" (the live Blender
session had one writer that night, the helmet agent) is stated in the spec §0
and costs no code change when a cloth is authored. Lift law tan θ = k·v²/g
(in game: 43.8° at 8 m/s, 78.7° at 20; hang 31° = lying along the back),
damping 0.960, the back keep-out is the POSED torso plane (the torso leans 33°
forward — a vertical plane let it hang through the chest, red-team P1-1) stood
off by a MEASURED suit-back surface (the knot bone is 7 mm inside the suit —
art-rung item). The ribbon is a "+" of two strips so the side view reads.
Colour = `indy_red` from the GLB, an OPEN question for Chad (§4 never named
one). Not roost-reactive (the open detail above stands). Handoff:
`docs/SESSION_HANDOFF_20260819_scarf.md`.

---

## §6 THE RUNGS

Max 2 attempts each. No self-pass. Chad signs each.

### R0 — PIPELINE PROOF + THE RENAME GUARD  *(gate: bit-exact tape)*
Grey proxy at the §3 measurements, §2.2 skeleton, generated by script. Headless
reproducible export (`blender --background --python export_sudburian.py`) with
asserted preconditions (scale 1.0, no bone scale F-curves, name lengths, bone
count) and a printed GLB hash. Declared joint catalogue + validator test
replacing silent `find_node`. Decide CPU vs GPU skinning with a measured number.

**Pass, all of it, no partial credit:**
1. Rebuild from the script twice → **byte-identical GLB** (hash match).
2. Loads with expected bone count, no truncated/duplicate names.
3. Validator test fails loud on a deliberately removed joint.
4. **`tape_360_chad_repro` + full sled golden set replay bit-exact with the
   character system enabled AND compiled out.**
5. Character draw cost measured and stated (target < 0.5 ms).
6. `graph_query.py check` green; graph regenerated.

### R1 — PROPORTIONS + CONTACT  *(the fix rung — ships on the CURRENT mesh, Chad ruled)*
Repair defects 1-11 and 14. Re-target feet to `board_socket_L/R` (defect 7).
Wake `absorb` from real kernel contact state (defect 8). Resolve defect 9 — the
invented 90 mm hip shift is either deleted or promoted into the kernel so
visual and simulated CG agree; **deletion is the tape-safe option and is the
default**. Reconcile the ski angle to `steer_max_rad`.

**Gate (VIDEO, §0.4):** Chad drives it. Boots stay on the board through the full
lean/slide/stand box; mitts stay on the grips within 1 cm over a bump; no hip
detach at IK saturation; the machine reacts to terrain. **This is the rung that
kills "floaty", and it ships before any new art.**

### R2 — MESH + COSTUME  *(geometry rung — census + contact sheet)*
Model to §3, costume to §4, ~14k tris LOD0, one material, 2048² albedo +
normal/ORM. Edge loops per the skinning spec. Scarf built and wired. Helmet
stays on always — which deletes faces and bare hands, the two things a solo
pipeline never gets right, and is why §0 can be confident about appeal.
Census reported as numbers: non-manifold / boundary / loose / zero-area = 0/0/0/0,
no modifiers, ROM sweep (shoulder 0-160°, elbow 0-140°, knee 0-130°, hip 0-100°)
with no collapse, no candy-wrap, no pinch.

#### ★★ R2 IS STAGED R2a / R2b / R2c — CHAD RULED 2026-08-17

R2 as written is a whole character re-author — mesh, costume, textures, scarf —
riding on **2 attempts with no self-pass**. A rejection at the end burns all of
it at once, which is the R1b failure mode at ten times the cost. Chad ruled it
staged, on the nosepan ladder's pattern (layer → sign → next layer), and that
ladder is the one that worked. **Each stage carries its own 2-attempt budget and
its own sign-off.**

| stage | what | gate | needs Chad first? |
|---|---|---|---|
| **R2a** | pipeline, rig, the UE5 rename + L/R crossover map, validators, census/ROM harness, the §3 numbers that are still missing | measured: byte-identical rebuild, validators fail loud, `graph_query.py check` green | **no — starts immediately** |
| **R2b** | grey **BLOCKOUT** at final §3 proportions, no costume detail, no textures | **Chad judges silhouette** (§9: agents build the blockout, Chad judges the form) | needs R2a's numbers |
| **R2c** | costume, materials, textures, scarf | census 0/0/0/0 + ROM sweep + contact sheet, then Chad | needs R2b signed |

**Why the blockout is its own rung and not an internal step:** §9 says agents are
reliably bad at silhouette and appeal, and reliably good at parametric geometry.
R2b is precisely the handoff point between those two facts. Putting Chad's eye on
a *grey* form means a rejection costs a blockout, not a textured character — and
it means when he does sign, the expensive work is being spent on a silhouette he
has already approved. §0.7's MOTION → RIG → MESH ordering extends naturally:
**RIG → FORM → SURFACE.**

#### ★ R2c-5 — THE CONTROL-INPUT POSE REACHES THE RUNTIME (2026-08-18)

Chad ruled the control pose on 2026-08-18 and R2c-1..4 built it in Blender.
R2c-5 built the same pose in the GAME, which is where he will judge it — the
Blender rig is the reference, `render/sled_model.cpp` is the thing that ships,
and until this rung they disagreed completely (the runtime had no control
channel at all).

**The ruling, restated once so it is not re-derived from coordinates again:**
throttle is the RIGHT hand — right elbow DOWN, right hand UP, thumb onto the
proximal side of the handle. Brake is the LEFT — left elbow UP, left hand DOWN
and OVER the bar so the four fingers reach the lever. All four articulations
20°.

**What makes the runtime version defensible rather than a retype:**

- The rider's throttle SIDE is derived from the grip socket's x, not from a
  name. The two per-side signs — the elbow pole sign and the wrist roll sign —
  are MEASURED off the rest pose at load. Nothing about the ruling is written
  down as a signed constant anywhere, so the mirrored-pole trap (three
  appearances in one session) cannot recur here.
- The runtime independently re-derived what Blender measured by hand: elbow
  signs **+1 / −1** (they do NOT mirror), wrist signs **−1 / −1** (they DO).
  Two implementations, two files, one answer.
- The wrist rotates about the BAR THROUGH THE GRIP SOCKET, so the contact point
  is a fixed point of the rotation and cannot drift — no iteration, exact at any
  wrist angle. Blender needs 24 solver passes for the same property because its
  IK tip is the wrist.
- Measured in the running game (`SEADS_SLED_CTL_DEBUG=1`): throttle drops the
  right elbow 36.9 mm and lifts its wrist 46.4 mm; brake raises the left elbow
  45.8 mm and drops its wrist 42.5 mm. Blender's figures for the same pose are
  −35 mm and +46 mm. They agree to ~2 mm with no number copied across.
- Zero input is bit-unchanged: every term multiplies by the input, so the signed
  rest visual is untouched.

**Still open after it:** ~~the game draws the LEGACY blob mitts~~ — closed by
R2c-S below. The ROM sweep still does not exist.

#### ★★ R2c-S — THE SUDBURIAN IS IN THE GAME (2026-08-18)

Chad, after the overnight wrong-rider session was reverted: *"ok now get the
sudburian into the game, in the open session."* Done, and it is what §2.3
always said: the rider inside `indy650.glb` is now the 42-bone `sudburian_rig`
+ `sudburian_proxy` skin, the catalogue speaks UE5 names, and the L/R
crossover is written once.

- **The asset.** `assets/character/sudburian_src/splice_sudburian.py`
  (stdlib, re-runnable, verifies before it writes). Input = a selection-only
  export of `sudburian_rig` + `sudburian_proxy` out of the LIVE session
  (`export_rest_position_armature=False`, so the node TRS carry the seated,
  IK-solved pose — never headless, never a re-export of the machine, which
  cannot re-export). It deletes the 30 legacy nodes (`rider_rig` subtree +
  `ik_pole_L/R`), compacts, appends the Sudburian under `SLED_ROOT` with a
  compensating local TRS, and refuses to write unless: every joint's WORLD
  position equals the session's to 1e-5, the `world × IBM` skinned rest mesh
  equals the session's posed mesh (0.000000 m over 1,176 verts), exactly one
  42-joint skin, no animations, no legacy node, and **all 132 machine mesh
  nodes byte-identical**. 197 → 211 nodes, 42 → 33 materials, 4.12 → 3.65 MB.
- **The catalogue** (`render/rider_rig.h/.cpp`): 21 driven joints in UE5 names
  (+`clavicle_l/r`, which sit on the arm path), the other 21 skin joints ride
  their parents. Chain table: `hand_l`→`grip_socket_R`, `foot_l`→
  `board_socket_R` (**the crossover, §2.3a, now real, written once in
  `model_side`**). Payload list = `sudburian_proxy`, `snow_flap`,
  `pan_front_bar`.
- **The runtime** (`render/sled_model.cpp`): the legacy `root` hung off an
  unrotated armature with an identity rest, so pose_pass added a model-frame
  lean into `root.t` and pre-multiplied a model-X hinge onto the pelvis. The
  Sudburian's armature carries the R2b 180° seating yaw and its `root` is
  Blender's ground bone (−90° X). Both rest frames are now CAPTURED at load
  (`root_parent_R`, `root_R`/`root_q`) and every model-frame demand goes
  through them; on an identity rig this is bit-for-bit the old arithmetic.
  `kBootReseatM` → **zero**: the R2b seat IS the rest. Throttle side still
  derived (side 0, grip x −0.315 = `hand_r`), elbow/wrist signs measured
  +1/−1, −1/−1 as before.
- **Measured in the game** (`SEADS_SLED_DEBUG_MODE=1`, `SEADS_SLEDCAM=
  "2.2,35,12"`, `sud_*.png` in the repo root): neutral = the viewport;
  fwd 0.25 hinges him over the bars; stand 0.25 rises with knees folding on
  planted feet; lat 0.25 shifts him left (+X); steer swings the bars and both
  hands ride them.
- **Gate:** `[rider_pose],[rider_rig]` 50/50 re-pinned against the new bytes
  (hinge a 0.126356 / b 0.070138; the seat rows; the crossover asserted);
  full suite in the commit message.

★★ **THE FINDING THIS RUNG SURFACED — AND CLOSED THE SAME DAY.** The file's
`grip_socket_L/R` sat at z 0.169; the live .blend's at z 0.269 — the whole
`CH_steer_pivot` assembly had moved 100 mm forward after the last machine
export, and the .blend cannot re-export. Against the stale sockets the hands
read 16 mm forward / 51 mm up and R2c-5's wrist roll lifted 2.5 mm instead
of 46. Chad: *"splice the bar/grip nodes from the live session too."*
`assets/character/sudburian_src/splice_bar.py` carried the pivot's TRS and
`bar_L/R`, `bar_riser`, `grip_L/R`, `steer_post`, `grip_socket_L/R`,
`brake_lever`, `throttle_block` out of a selection-only export of the open
session (materials de-duplicated by name so `indy_red` stays single-sourced),
held the **bellcrank + tie-rods** (steering linkage, deferred) at their file
positions with a compensating local, and verified: carried nodes at the
session's world to 1e-5, carried meshes' world vertices to 1e-5, **125 other
mesh nodes byte-identical**, untouched children unmoved. Its own verify caught
a stale-index bug in the first cut (the bellcrank re-parented onto the wrong
node, 0.92 m) before anything was written — names, never indices, across a
compaction. Result: the wrist sits **20 out / 51 up / 83 aft = 98 mm** from the
socket — ONE FIST, the knuckle on the bar, exactly the R2c construction — and
in game throttle now drops the right elbow 17.6 mm and lifts the wrist 25.5 mm;
brake raises the left elbow 8.7 mm and drops the wrist 31.7 mm. The ruled
directions land, and the bar draws under the hands.

**What R2c-S is NOT:** a costume, a mitt, a texture. He is the R2b grey
blockout, in the game, moving. R2c's remaining art (the leather mitts —
generator salvaged at `sudburian_src/salvage_from_legacy_rider/` — costume,
scarf) now has a target that draws.

#### ★★ R2c-M — THE LEATHER MITTS ARE ON THE SUDBURIAN (2026-08-18, attempt 1 of 2)

Chad: *"I noticed that the fingers of the left hand of the sudburian need to
leave the hand from the top or middle of the hand (right now they come off the
bottom of the cylindrical block), but that may be irrelevant for when you put
the gloves on the hands (or replace them with the gloves). Please continue with
that."* The finger slab was `FINGERS_DROP = 0.055` (R2c-4, deliberate: 55 mm
below the knuckle). The mitts replace it — and every other hand volume — so the
item is closed by the mitts, and the acceptance is MEASURED, not read off the
generator: in the POSED mesh, against the REAL grip cylinder, the fingers of
both hands leave the fist **48 mm ABOVE the bar axis** (the knuckle line,
top-front) and curl to 54 mm below.

- **The generator** — `sudburian_src/mitt_geom.py` (moved from the salvage
  dir; pure stdlib; it never sees a rider, only a bar frame): palm + knuckle
  bulge + FOUR FINGERS IN ONE compartment + thumb (extended on the throttle
  hand, wrapped-and-gripping on the brake hand — R2c-4's asymmetry keyed off
  `BRAKE_SIDE`) + **back-of-hand loft** (new: the Sudburian's wrist is 100 mm
  from the bar, the legacy rider's was ~50, so palm+cuff left 30 mm bare) +
  gauntlet cuff rooted at the wrist joint + cinch strap. 10 closed
  interpenetrating solids, 2,122 verts / 4,208 tris per hand. Beige-yellow
  leather (§4), byte-space, its own Principled material (**the exporter ignores
  a node-less material's viewport colour and writes 0.8 grey** — measured; the
  grey blockout has always shipped at 0.8, left alone).
- **The frame is MEASURED, not `hand_frame()`.** The grip cylinder's own axis
  (`cylinder_fit` of `grip_L/R`, r 0.0210 exactly), machine forward and up
  were read in the LIVE session in the neutral seated pose and carried into the
  proxy's REST-space hand basis; both sides gave the SAME three triples to
  5 dp — `BAR_OUTBOARD_FE / MACHINE_FWD_FE / MACHINE_UP_FE / R_BAR` in
  `sudburian_proxy.py`, re-measurable with `measure_grip_frame.py` in the open
  session. `hand_frame()`'s "bar" was 27.4° off the true grip after the bar
  moved (R2c-S(b)); a 122 mm mitt built about it goes through the grip. No
  NEW per-side sign anywhere (the side enters once, in `hand_frame()`'s
  pre-existing basis): `grip_frame()` now REQUIRES an
  unambiguous up (the old glTF Y-up default is exactly degenerate in Z-up rest
  space and would have rolled one hand 180° — fingers out the bottom, Chad's
  item recreated), and `geometry_only()` asserts the two hands are exact
  X-mirrors.
- **The wiring** — `sudburian_proxy.py` `volume_specs()` emits one `mitt` spec
  per side; `geometry_only()` is the single bpy-free source of the whole mesh
  (build_mesh, the live-session swap, and the census all read it); pieces map
  to bones by label (palm/knuckles/handback/cuff/strap → `hand_`,
  finger0..3 → `mittfront_01_`, thumb → `thumb_01_`), rigid 1.0, 21 groups
  as before, no bone moved. Applied to the LIVE session as an in-place mesh
  swap (`sudburian_src/apply_live.py`: body verts proven identical to the
  source first, worst 0.0), all 21 groups rebuilt and asserted, mitts
  smooth-shaded (flat split every ring vertex 5x on export — the red-team
  caught 22.5k skinned verts for 4.2k positions), seat/IK untouched; `.blend`
  saved after a backup.
- **Gates.** Per hand: census 0/0/0/0, directed winding 0, folded 0; **every
  piece ≥ 22.9 mm from the real grip axis (r 21.0)** — a hard assert, after the
  first cut's cuff head-cap reached the socket (0.5 mm) and the handback's
  width tilted radially (2.7 mm) — both caught by the per-piece measurement,
  both invisible to the four census numbers. Splice: 132 machine MESH nodes
  byte-identical, skinned rest == live posed **0.000000 m both ways** (the
  verify now reads ALL primitives — a second material makes two — and is
  two-way with a spatial hash; proven to fail on a 1 mm shift and on a
  truncated ref). `[rider_pose],[rider_rig]` 50/50 unchanged. Game shots
  `sudM_sheet.png` (neutral / throttle / brake).
- **DEBT / open:** rigid split across three bones — a finger squeeze slides
  solids (R3); LADDER R2 "one material" vs §4 beige-yellow → a second flat
  material until textures (Chad to confirm); the runtime rolls the wrist about
  `socket_R − socket_L` (pure X) while the real grip is swept 19.6° — a 20°
  roll moves the mitt bore ~7 mm off the grip at the palm edge (runtime item,
  not chased); the mitt bore's outboard end shows the grip end-plug (the bar
  ends at the mitt's outer edge).

### R3 — THE ANALOG POSE SET  *(video gate)*
Five hand-keyed **single-frame** poses — `ride_neutral`, `lean_l`, `lean_r`,
`slide_fwd`, `slide_back` — plus `stand_boards` and `tuck`, blended bilinearly by
the live kernel scalars. **Five poses, not fifty clips.** A hand-keyed lean
carries the whole correlated chain (pelvis shifts, spine counter-rotates ~7°,
outside leg extends, head stays level) that no per-bone offset formula
reproduces, and because it is driven by the physics variable it is *automatically*
in sync with weight transfer. Hands stay IK-pinned to the grips throughout.

### R4 — THE GRIP LAW AND THE BUCK-OFF LOOP  *(Chad ruled 2026-08-16: R4 begins here)*
Full specification in **§7**. This is the first rung that is **kernel work**, and
it is staged R4a..R4e so R4a ships alone.

### R5+ — ON FOOT AND BEYOND  *(scoped, not yet specified)*
Third person only for v1 — **no FP arms rig** (Chad ruled). Mode manager first:
today "modes" are loose bools in a 4,226-line `main()` (`app/main.cpp:1315`) and
`KEY_J` mount is a labelled-scaffolding teleport. Then `sim/walker` on
`world::SnowpackField::sample_at` — a foot is just another contact patch, and
the snow queries, depth law and surface classes all already exist, so walking in
snow is the **cheap** part. Locomotion from retargeted mocap. Then canopy egress
(`kCanopy` is already a separate node/mesh, `render/rig.h:42`, but `Driven::None`
with a placeholder hinge — the 109's canopy hinges **starboard**), wrench,
weapons.

---

## §7 THE GRIP LAW — R4 (Chad's spec, 2026-08-16)

> "It takes a big bump to get the Sudburian off. Most of the time he is just
> bumped up holding on and bouncing on the big bumps. Only his hands don't let
> go. **Miner's hands.**"

### 7.1 The idea

The rider is **attached by his hands**, not welded to the seat. Everything else
about his body — seat contact, feet on the boards, torso extension — is a
consequence of load, and fails in order. The grip is the **last** thing to go.

This is not a clip library. It is one continuous physical state with an ordered
failure chain, which is why it will read well: nothing in it is a switch that
snaps, and the thing the player watches (the hands) is the thing that holds
longest.

### 7.2 THE THUMB THROTTLE (Chad, 2026-08-16)

The Indy's throttle is a **spring-loaded thumb lever on the right grip**. The
right thumb squeezes it; the spring closes it. `mittfront_01_l` does the same
for the brake lever on the left.

Cost is near zero and the payoff is out of proportion to it: **`throttle` is
already a kernel input**, so `thumb_01_r` rotation is a direct continuous
function of a signal we already have — no new state, no clip, no tape exposure.
It is the detail that reads as *this man is operating a machine* rather than
sitting on one, and it sits at the exact centre of frame in the chase view.

Bone consequence: §2.2, amended — 30 → 34 body deform bones.

Wired in **R3** (it is an analog pose driven by a kernel scalar, like the lean
set); the bones exist from **R0** and the mitt geometry from **R2**.

### 7.3 THE FAILURE CHAIN — ordered, and the order IS the feel

| Stage | Condition | What the body does |
|---|---|---|
| 0 SEATED | nominal | as R3 |
| 1 UNWEIGHTED | seat normal load → 0 | he floats off the seat; `rider_up_m` rises |
| 2 BOARDS FREE | foot load → 0 | boots leave the running boards; leg IK releases from `board_socket` |
| 3 **SUPERMAN** | body extends behind the anchor | legs trail from the hips as a damped chain — **the scarf solver, anchored at the grips** |
| 4 **RELEASE** | grip load > grip capacity | hands let go. He is a free body |
| 5 TUMBLE | free body in snow | tumbles, sheds energy into the snowpack |
| 6 DOWN → UP | at rest | gets up |
| 7 RECOVER | on foot | runs to the machine — which is **stopped but still running** |
| 8 RIGHT IT | machine flipped | grabs the **highest point** of the machine and rolls it down like spinning a heavy wheel |
| 9 REMOUNT | machine upright | gets on |

Stages 0-3 are **continuous and reversible** — this is the common case and it
must be by far the most frequent outcome. Stage 4 is a one-way transition and
must be **rare**: it takes a big bump.

### 7.4 WHAT THE MACHINE DOES WHEN HE LETS GO (Chad, 2026-08-16)

One causal chain, not three rules — and it is the real behaviour of a
centrifugal-clutch sled, which is why it hangs together:

> hands release → **throttle springs to idle** (the thumb lever is
> spring-loaded, §7.2, and there is no thumb on it any more) → **engine drops to
> idle rpm → the clutch falls below engagement and stops being powered** → no
> drive at the track → **the track's own drag and friction decelerate the
> machine** → it coasts, slowing but free, and comes to rest → it sits there
> **idling, and does not creep.**

**NO CREEP — Chad ruled it explicitly.** This is not an exception we impose; it
falls out of the clutch being disengaged at idle. A powered track would creep;
an unpowered one cannot. The rule and the mechanism agree, so nothing here is
special-cased.

Consequences worth stating:
- The machine he runs back to is **idling, whether upright or flipped.** Same
  state either way; flipping changes nothing about the engine.
- "Slowing but free" is a coast-down, not a brake. He may have to chase it a
  little, and that is good.
- Because it does not creep, the machine is always exactly where it stopped —
  the recovery loop stays legible and never turns into a chase after a
  self-driving sled.

*Measurement obligation at R4a:* whether `sim/sled` already has an idling-engine
state and an unpowered-track drag term, or whether both must be added. Do not
assume — see §7.10.

### 7.5 THE POOF, AND GETTING UP (Chad, 2026-08-16)

**Landing throws a big poof of snow.** Reuse the existing roost/spray path
rather than forking a second particle system (§0.3).

**Recovery speed is surface-dependent, and it costs us nothing** — the surface
classes already exist (`world::Surface`: `Bush`, `TrailMain`, `TrailTributary`,
`Road`, `LakeIce`, each with a `sinkable` dial, `world/snowpack.h:39`), and the
tumble already knows where it stopped:

| Surface | Getting up |
|---|---|
| Deep / sinkable (`Bush`) | **Slow.** He climbs out — wallowing, unglamorous, has to find the bottom first. The 0.77 m depth law means he really is *in* it |
| Trail or road (packed) | **Fast — adrenaline.** Up and running, no wallow |

This is one of the best beats in the loop: the same crash costs you fifteen
seconds in the bush and two on the trail, and the player learns that from the
snow, not from a UI. It also gives the trail network a felt value beyond speed.

Recovery time keys off `surface_at()` at the rest position. No new world query.


### ★★★ 7.5a THE WALK — CHAD RULED IT, 2026-08-31 (verbatim)

> *"he should dissapear and / reappear in deepest snow in a poof and then get up
> from being prone or supine to crawl then large stepping with snow coming off
> walk in deep snow, quick gait on hardpack and then the intermediate midly
> handicapped by about a foot or two, but slowed."*

and, the same day, re-stating §7.5's own rule so it could not be missed:
*"Its quality / speed is determined by snow depth."*

**DEPTH IS THE ONE INDEPENDENT VARIABLE, AND IT DRIVES BOTH.** Not a surface
CLASS. §7.5's table reads "Deep / sinkable (Bush)" vs "Trail or road (packed)"
and it is tempting to switch on `world::Surface` — but the corridor override
already zeroes the depth on plowed trails and roads (§2.3), so the depth field
ALREADY carries the class distinction and carries it **continuously**. A class
switch would be a binary read of a continuous quantity, which is this ladder's
own recorded disease, three rungs over. There is no threshold in `sim/walker.*`
for that reason.

**HIS THREE ANCHORS, which are measurements and not endpoints somebody drew a
line between:**

| depth | his words | shipped |
|---|---|---|
| ~0, hardpack | *"quick gait"* | 3.5 m/s, 0.90 m stride → 3.9 steps/s |
| **"about a foot or two"** = 0.30–0.61 m, so **0.45 m** | *"midly handicapped ... but slowed"* | 1.6 m/s, 1.00 m stride |
| **0.77 m** — THE SIGNED DEPTH LAW | *"large stepping"*, and he goes UNDER first | 0.6 m/s, 1.10 m stride → 0.55 steps/s |

Read out piecewise-linearly over those three, the way `sim/sled.h`'s
`kAftCeilC1` reads its measured aft-ceiling row — an idiom that already ships
here, so this is not a new curve family. ★ **The middle anchor is the whole
point**: a straight line from hardpack to deep would make him fast in a foot of
snow, which is exactly what "midly handicapped ... but slowed" refuses, and
there is a gate leg that fails if anyone straightens it.

★★★ **AND THE STRIDE IS A SEPARATE ROW FROM THE SPEED**, which is what makes his
two phrases two MOTIONS rather than one motion at two rates: stride GROWS with
the snow while speed FALLS, so cadence — which is authored nowhere and is only
ever speed/stride — collapses from a trot to a heave. Deep snow is a man lifting
one knee clear at a time; hardpack is a jog.

★★★ **THE GAIT ADVANCES ON DISTANCE, NEVER ON TIME.** Feet driven by a clock
skate whenever the body is not travelling at the speed the clock assumed — most
visibly across the acceleration ramp, where a clock runs a third of a cycle
ahead of the ground he has actually covered. Mutation-verified.

**THE SEQUENCE, and it is one clock:** poof + **Buried** (deepest snow only —
structurally zero seconds long on hardpack, not merely short) → poof + **Down**
(prone/supine) → **Crawling** → **Afoot**. Every stage is a FRACTION of the rise
the depth already bought (§7.5's own "fifteen seconds in the bush and two on the
trail"), so the whole sequence stretches with the snow and nothing has a second
clock to drift on.

### ★★★ 7.5b THE HELMET REMEMBERS — Chad, 2026-08-31

> *"I have a damage modeled helmut by pressing U. Each fall goes up the stages of
> damage for the helmut. A subtle accumulation of visible damage from falls,
> though it wont be health affective."*

**COSMETIC ONLY** — nothing but the draw reads it and no kernel value depends on
it. The hook was already there and already said so: `render/sled_model.h` calls
`sled_helmet_dent_set` "cosmetic crash-damage helmet ... crash logic may set it
directly", and the setter **already clamps to [0,4]**. So "each fall goes up a
stage" needed no new clamp and no new state — and the clamp is what makes it an
ACCUMULATION rather than the `U` key's cycle, which wraps back to pristine.
A helmet does not heal.

### 7.6 THE ARCHITECTURAL FIND — superman IS the scarf

Chad: *"legs doing what normally reserved for his scarf."*

A damped trailing chain anchored at a fixed point is exactly the scarf (§5).
Superman is that same solver with the anchor moved to the grips and the body as
the chain. **Build it once in R2 for the scarf, re-use it in R4 for the body.**
Same code, same fixed iteration count, same fixed dt.

**It is a trailing-chain solver, NOT a ragdoll.** Full rigid-body ragdoll is
banned here: it is a determinism hazard (iteration-count and contact-order
dependent) and a well-known quality trap that produces exactly the floppy,
weightless motion §3 and §0.4 exist to prevent. Fixed segment count, fixed
iterations, fixed dt, no contact solver.

### 7.7 WHAT LIVES IN THE KERNEL — and why this rung is different

**Everything R0-R3 was render-side and structurally could not touch a tape. This
rung can.** When he releases, `rider_mass_kg` (87.5) leaves the machine:
`cg_off` (`sim/sled.cpp:313`) and `patch_geometry` (`:317`) both change, and a
sled tumbling riderless is physically a different machine. So:

Kernel-side (new `SledState` fields, all taped):
- `grip_load_n` — instantaneous load through the hands
- `grip_capacity_n` — miner's hands; the threshold. **The dial.**
- `rider_attached` — the one-way stage-4 latch
- `seat_load_frac`, `board_load_frac` — drive stages 1 and 2
- `rider_up_m` — **already exists, already taped, already feeds `cg_off`.** It
  gains a *physical* driver (machine vertical accel) alongside the stand input.
  His CG rising when he gets bucked therefore becomes real physics for free.

Render-side (no tape exposure): chain solve, blend weights, IK, tumble pose.

**Shipping rule — the proven pattern.** Ship with `grip_capacity_n` set so it
**never breaks**, and prove the full existing golden set **bit-identical**. This
is exactly how `assist_hull_frac` / `roll_stiff_vgain` / `release_floor_frac`
went in on GI3 ("all 0-OFF bit-identical"). Only then is the capacity dialled
down to a real number, as a separate, re-taped change.

### ★★★ 7.7a CHAD RULED THE TWO OPEN QUESTIONS, 2026-08-30

**Q1 — WHICH SIDE HE FALLS OFF: *whichever way he is already leaning out*.**
The release takes the lateral the body already has at touchdown and amplifies
it; it does not invent a side and does not read the machine's roll.

⚠ THE SIDE IS SMALL WHERE IT MATTERS — about a fifth of his horizontal
departure — so the gain does real work and must be DRIVEN, not tuned to a table.

★★★ **RE-MEASURED 2026-08-31 ON THE WINDOWS THE SHIPPED MECHANISM ACTUALLY
SELECTS**, after a fresh-context red-team pointed out that the first pass graded
the windows the RENDER CHAIN selects, which are not the same set. Over 45
distinct drives / 84 airborne windows, taking the 10 windows above the kernel's
own p90 load:

| | chain-selected (n 8) | **kernel-selected (n 10)** |
|---|---|---|
| lateral share of the HORIZONTAL departure | 0.204 | **0.220** |
| departures that go FORWARD, not aft | 6 of 8 | **8 of 10** |
| side predicted by the buck's own lateral push | 4 of 8 | **8 of 10** |

Two things follow, and they correct the first pass rather than repeat it:

1. **HE GOES FORWARD, OVER THE BARS — not out the back.** 8 of the 10 windows
   that break the grip carry a NEGATIVE aft component. Corpus-wide the sign is
   mostly aft, because drag streams him back in flight; it FLIPS at the landing,
   which is what a landing is: the machine stops and he does not. An earlier
   version of this section said "up and back" — it was a median of ABSOLUTE
   values and it was wrong.
2. **THERE IS A SIDE TO INHERIT AFTER ALL, AND THE BUCK PICKS IT** — 8 of 10 on
   the mechanism's own windows. The first pass reported "no driver beats a coin"
   from the chain's window set at n = 8. Chad's ruling (a) is therefore better
   supported than it was when he made it, not worse.

The full table and the three corrections are in
`docs/SESSION_HANDOFF_20260830_r4a_lateral_MEASURED.md` §2.

**Q2 — WHERE THE LOAD LIVES: *the kernel grows its own extension*.**
§7.7 as written puts `grip_load_n` in the taped kernel, but the extension factor
the calibration used is the RENDER chain's displacement, which `sim/` cannot see
and a tape could never replay. So the kernel carries its own scalar extension —
same charge, same softened pull-back, one shared arming memory in
`sim/rider_grip.h` — and render keeps the chain that DRAWS him. The machine is
never moved by an animation.

Two corrections to §7.7's text that follow from building it:

1. **The field is `grip_load`, NOT `grip_load_n`.** hardness × extension is
   m²/s², not Newtons, and `probe_grip_hold` already computes a genuine rod-model
   grip FORCE in real Newtons. Two quantities behind one word is a lying
   instrument.
2. **Nothing new goes in the positional pin roster.** The tape pins 41 doubles BY
   INDEX; one more refuses every existing `.sledtape` — the corpus the capacity
   is calibrated on. The grip state re-converges from taped inputs alone, the
   `ws_exch_l` class, and is OFF-by-absence in the tape loader.

### ★★★ 7.7b STAGE 1 IS DRIVEN AND CLOSED (Chad, 2026-08-31)

> *"I rode this last night and did not fall off."*

**That is the pass.** `grip.capacity` ships at 1e30, so the release cannot fire —
a drive he came off would have meant something was wired that should not be. And
it certifies the one thing in stage 1 that could have changed the feel: the
arming memory MOVED from `render/body_drive.*` into `sim/rider_grip.*`, and it
still feels like itself. **`buck_gain 0.02` / `decay_per_s 3.0` are signed again,
in their new home.**

STAGE 2 — the release itself — was specified in
`docs/SESSION_HANDOFF_20260831_r4a_stage2_release.md` and **is now BUILT and
AWAITING HIS DRIVE**: see §7.7c below.


### ★★★ 7.7c STAGE 2 IS BUILT — HE LETS GO, AND HE COMES OFF (2026-08-31)

**AWAITING CHAD'S DRIVE. Nobody self-passes this one (§7.9).**

Four things landed, in §7.7's own order, and each is separately gated:

1. **The capacity is a real number.** `grip.capacity` 1e30 → the measured
   **78.34** (p90 of the kernel's own load over 45 distinct drives / 84 airborne
   windows, read at SUBSTEP resolution). `SEADS_GRIP_CAPACITY` sets it live and
   a huge value restores stage 1 exactly — that is the A/B and the kill switch.
2. **The machine feels him go.** `sim/sled.cpp`: `rider_frac` (and with it
   `cg_off` and `patch_geometry`) drops to zero, the airborne exchange term goes
   with it, and **no hand input reaches the machine any more** — throttle,
   brake, bars and the whole weight-shift. §7.4's chain then runs itself: at
   throttle 0 the clutch falls below engagement, the track is unpowered, and
   the machine coasts to rest and does not creep. Nothing was added to make
   that happen; it was already there.
3. **He comes off.** `render/rider_flight.{h,cpp}` — a free body in world
   doubles, gravity + the body's own drag, landing on the drive surface under
   him and sliding to rest in the snow. The two IK welds that were actually
   holding him (hands→`grip_socket`, boots→`board_socket`) release, and the
   departure rides the root as a model-frame vector, so the chain, the legs and
   the scarf all follow him without a second mechanism.
4. **The direction is Chad's ruling and a measurement, and only ONE of them is
   a gain.** Q1 (the side) is `lat_gain_per_s` × the lateral he already had —
   sign and magnitude both his lean's, amplified. **FORWARD IS NOT A TERM.** He
   departs with the machine's own velocity and nothing added; the measured
   8-of-10 over-the-bars departure is then EMERGENT, because the landing is the
   machine shedding speed into the snowpack while he is a free body. A forward
   gain would be paying twice for one fact.

**THE ONE DIAL HIS EYE SETS: `SEADS_GRIP_LATGAIN` (default 2.0 /s).** §7.7a says
it in as many words — the side is only ~0.22 of his horizontal departure, so the
gain does most of the work and no table can set it. The default is DERIVED and
labelled as such (clear a 0.4 m half-width machine inside the ~1 s of a
landing-height departure, at a typical 0.2 m of lean), never measured.

**⚠ THE HONEST GAPS, NAMED HERE RATHER THAN DISCOVERED LATER:**

- **`mass_kg` is still 331 kg when he is gone.** §7.7 names `cg_off` and
  `patch_geometry` and names nothing else, and dropping 87.5 kg without
  re-deriving the inertia tensor it was measured at is the trap `sim/sled.h`
  warns about in its own words. A riderless Indy is 243.5 kg and would coast
  differently. **R4b**, with the inertia re-derived.
- **He keeps the machine's ATTITUDE while he flies.** The departure is a
  translation; a machine that rolls hard after he leaves will roll him with it.
  R4b's tumble is where a world-locked attitude belongs.
- **No tumble, no poof, no getting up** — R4b by §7.9's own staging. He lands
  flat and slides to rest.
- **`seat_load_frac` / `board_load_frac` still do not exist**, and `rider_up_m`
  still has no physical driver. Neither was needed for the release; both are
  still unbuilt spec.

**The corpus is safe, structurally.** Every existing tape was cut before the
grip dials entered `SLEDTAPE_PARAMS_D`, so the loader forces `unseat_gain = 0`
on all of them; at gain 0 the extension is identically 0 and no capacity of any
value can be exceeded. The stage-1 tripwire
(`sled_the_grip_law_moves_no_golden`) was re-aimed at exactly that claim rather
than deleted, and its inverse — `sled_the_release_takes_the_rider_off_the_
machine` — is now the leg that would go red if the wiring were quietly removed.

**Getting back on, for now: KEY_R.** The autoright already zeroes `sled.grip`,
which re-attaches him. That line was put in by red-team while it was still
inert; this is the day it started mattering. R4c–R4e replace it.

### 7.8 STAGE 8 — RIGHTING THE MACHINE, AND THE SCAFFOLD IT REPLACES

`app/main.cpp:2321-2327` — `KEY_R` autoright — is **already labelled in the
source as a stand-in for "the guy running back to the snowmachine."** R4 is
where that scaffold is cashed in for the real thing. It also currently zeroes
`sled_lean_*` in a discontinuous step (defect list, item 8: the rider snaps),
so replacing it retires a known pop.

Mechanic: find the highest point of the machine in world-up, plant the hands
there via the same hand-IK used for the grips, and roll it down about its
contact line — "like spinning a heavy wheel". Heavy and slow, with heavy-body
timing: **the pelvis leads the hands** by 2-3 frames, and he settles with a
small overshoot rather than stopping dead. The machine's rotation is
kernel-side; the man pulling on it is render-side, driven by the machine's own
righting angle. That keeps the direction of authority correct — the machine is
never moved by an animation.

### 7.9 STAGING — R4a ships alone

| | Scope | Needs walker? | Gate |
|---|---|---|---|
| **R4a** | Stages 0-4 + tumble to rest. He gets bucked, supermans, releases, tumbles, lies in the snow. | **No** | Video: big bumps buck him **up, not off**; release is rare and clearly earned; hands are visibly last to go |
| **R4b** | Stages 5-6: tumble quality, **the poof**, surface-dependent get-up (§7.5) | No | Video: bush wallow vs trail adrenaline are obviously different |
| **R4c** | Stage 7: run back | **Yes** — this is where `sim/walker` and locomotion land | Video |
| **R4d** | Stage 8: right the machine, retire `KEY_R` | Yes | Video + the pop is gone |
| **R4e** | Stage 9: remount, retire the `KEY_J` teleport | Yes | Video: no snap from 8 approach angles |

**R4a's tuning gate is the important one and it is a felt call, not a number:**
the ratio of *bounced-and-held* to *thrown-off* must be strongly biased to held.
Chad rules `grip_capacity_n`. Nobody self-passes it.

### 7.10 Open questions for R4

All three of the original open questions were **ruled by Chad on 2026-08-16**
and are now spec, not questions — see §7.4 (auto-idle, clutch, drag, no creep)
and §7.5 (the poof, and surface-dependent recovery). What remains:

1. Does `sim/sled` already model an idling engine state and unpowered-track
   drag, or must both be added? **Measure at R4a; do not assume either way.**
2. Does the existing deceleration under zero drive already produce a plausible
   coast-down, or does track drag need its own term?

---

## §8 LICENSING — decided now, before anyone downloads anything

| Source | Ship in a commercial game? |
|---|---|
| **Mixamo** (Adobe) | **YES** — royalty-free in projects; no redistribution as assets. Primary source. |
| **CMU Graphics Lab** | **YES** — free for all uses. Fallback; raw 2003 marker data, real cleanup per clip. |
| **AMASS** | **NO — DO NOT SHIP.** Aggregation; most sub-datasets are research-only. |
| **SMPL / SMPL-X** | **NO** — non-commercial research only, including derived meshes. |
| Rokoko free library | Yes per current EULA — re-read it at use time. |
| ActorCore | Yes per pack EULA. |
| **Epic / Paragon / Lyra / ALS** | **NO** — licensed for Unreal Engine projects only. Unusable here. Most common mistake in exactly this scenario. |

Retargeting method: constraint rig (`Copy Rotation` + `Copy Location` on hips) →
`Bake Action` with visual keying, ~150 lines of Python. **Not** a GUI addon —
scriptable and reproducible is the requirement, and GUI addons are what agents
drive worst. Budget **1-3 h of human cleanup per shipped clip** for a bulky
character; that, not the modelling, is the real line item.

---

## ★ §10 THE DEBUG-BUILD FINDING — ACCEPTED AND ACTED ON (2026-08-16)

Chad: *"yes okay on the last finding as well."*

The finding, restated: the rider skinning block costs **0.106 ms/frame at `-O2`
and 4.73 ms/frame at `-O0`**, and `CMAKE_BUILD_TYPE=Debug` — what `CLAUDE.md`'s
`Run:` line hands you — is `-O0`. 45×. ~28 % of a 60 fps budget for one man on a
sled, before the world is drawn.

**What was NOT done, deliberately:** `build/` was **not** flipped to
RelWithDebInfo. The ctest gate is pinned to it, Debug keeps asserts live, and
re-typing a build dir under a gate is how you get an unexplained baseline shift.

**What WAS done:**
1. `CLAUDE.md` ▸ Commands gains a **RUN TO DRIVE OR TO JUDGE FEEL** entry:
   `build-play/` at `RelWithDebInfo`, run `.\build-play\seads.exe`. This is not a
   new convention — `docs/airdome_report.md` and `docs/bubble_atmosphere_spec.md`
   already used `build-play` (RelWithDebInfo) for exactly this, and the dir was
   already in `.gitignore`. It simply was not in the Commands block, so the
   documented run command handed everyone the slow binary.
2. `build-play/` configured and built in this worktree so it exists before the
   next drive. **Confirmed from the generated ninja files, not assumed:**
   `build-play` compiles `-O2 -g -DNDEBUG`; `build/` carries **no `-O` flag at
   all**, i.e. `-O0`. The finding is reproduced in this worktree.
   `build-play\seads.exe` is 87.3 MB, built clean, exit 0.

   **★ The tradeoff, stated so nobody is surprised:** `-DNDEBUG` means
   **asserts are compiled OUT** of `build-play`. That is exactly why `build/`
   stays at Debug and keeps the gate — you drive `build-play` and you *test*
   `build/`. If a drive on `build-play` produces a weird result that an assert
   would have caught, reproduce it on `build/` before believing it.
3. `.gitignore` gains `build_sudburian/` and `build_sudburian_raylib_src/`
   (R0 follow-up 3).

**★ THE PART THAT MATTERS BEYOND THE BUILD FLAG.** Every felt call in this
project's history — the drive-4 feel sign-off, the roll-comfort judgements, GI4's
ride tuning — was plausibly made on an `-O0` binary. That does not invalidate
them; frame cost changes *smoothness*, not the kernel, and the kernel is
fixed-step. But it does mean **if the machine feels different at RelWithDebInfo,
the difference is the frame rate, not the physics**, and nobody should go tuning
a kernel dial to chase it. Worth knowing before the R1 video gate, which is the
next thing Chad judges by eye.

---

## §9 WHAT THE AGENTS DO AND WHAT CHAD DOES

Agents are reliably good at parametric, measurable geometry (proven by the
nosepan work), bone hierarchies, weight-map symmetry audits, export scripts,
constraint retargets, validators, clearance census. Agents are reliably bad at
organic form, appeal, silhouette, and **weight in motion** — anything whose
success criterion is "looks right", because no measurement substitutes and the
agent cannot see its own output the way a person can.

Therefore: **agents build the skeleton, the export, the retarget, the validators
and the blockout. Chad judges the silhouette, and judges every motion at speed,
in the game.** Never on a still.

#### ★★ R2c-M2 — GLOVES, THROTTLE, BRAKE — SIGNED BY CHAD'S EYE 2026-08-18 night

The spec is `docs/R2cM2_CONTROLS_SPEC.md` (its §1 ruling lines v4→v8b are the
intent); the build is `docs/SESSION_HANDOFF_20260818d.md`; the hand-tuning
session that followed — Chad at the viewport, one move per message, ~45 moves
— is `docs/SESSION_HANDOFF_20260818e.md`, and it makes the .blend + spliced GLB
the source of truth for the hands and the throttle assembly (the generator no
longer reproduces them; `apply_live.py`/`apply_levers_live.py` refuse without
`SEADS_MITT_REGEN=1`). One-piece gloves (no cuff/strap/flare), left palm a
capital L to the finger bases, both hands half-closed, brake blade on its
cylinder with the fingers curled on it, throttle box concentric on the bar
(on the bend, square to its local run) with the lever parallel to the bar and
the thumb on it. Chad: "and now the thumb is actually pushing the throttle."

---

# R4-SIDE — THE "P" KEY (PULL): THE KNEEL BECOMES A SIDE-HANG SURVIVAL MOVE

**Chad's ruling, 2026-08-21.** Recorded verbatim-distilled; his words are the spec.
Status: NOTE ONLY — nothing here is built. Do not start it without a launch ruling.

★ **The longitudinal half of the ruling IS built: R3-WS(e), 2026-08-21.** The kneel is
out of the fore-aft ladder (`kKneelRuntimeGain` 0), the aft clamp is re-sized to the
kneel-free ceiling (`lean_aft_max_m` 0.2786 → **0.2183**, `sim::kAftCeilC1` re-pinned),
and **every piece of kneel machinery listed below is still in the tree and still under
gate** — the kneel-math tests drive it through an explicit gain of 1 so they stay
non-vacuous. Whoever builds R4-SIDE inherits working, tested geometry.
See `docs/SUDBURIAN_R3_WEIGHTSHIFT_SPEC.md` §14 (the ruling) and §15 (what shipped).

## What he ruled

1. **"defer the kneeling fit for now"** — the full-kneel appearance work (R3-WS(d)
   shipped the kneel with clean math but the shipped `sudburian_proxy` renders deep hip
   flexion as interpenetrating rigid limb boxes) is **DEFERRED**. Spend nothing on it.
2. **"not make kneeling for longitudinal movement"** — the kneel comes **OUT of the
   fore-aft (longitudinal) weight-shift ladder**. The aft ladder ends at the deep seated
   rear crouch. **THE KNEEL IS NOT A FORE-AFT POSE.** (Live consequence: the ladder's
   aft-CG ceiling drops from 0.2786, so K-WS1's pinned C row and the aft clamp must be
   re-sized — the K-A2 cross-check test exists to force exactly this. **MEASURED at
   R3-WS(e): 0.2183, not the ≈0.245 predicted here.** The prediction was wrong by
   27 mm and the bake is the authority; the whole kneel-free row is
   0.2183 / 0.2146 / 0.2070 / 0.1984 / 0.2021.)
3. **"we can move it to the fore a bit as it suits you (and we will defer this)"** — the
   kneel station may move forward, builder's discretion. **DEFERRED**, not now.
4. **The kneel's real home — a SIDE HANG.** "eventually [we want] the pose activated for
   a hanging to a side lean":
   - the **lean-side leg goes DOWN TO THE RUNNER** (running board),
   - the **far leg STAYS KNEELING** on the seat,
   - body hangs out to that side.
5. **The key is `P` — for PULL.** "a hotkey so you can pull a side / unstick it."
6. **Purpose (his order):** (a) eventually **flip the snowmachine back over** with that
   key; (b) **pull it out of a tip-over** better; (c) and do it **BEFORE it rolls** —
   pre-emptively stabilising weight on a hillside. He calls it a **survival key**.

## Notes for whoever builds it (design context, not a spec)

- **This is a LATERAL rung, not longitudinal.** It belongs with `lean_lat_m` and the roll
  axis, not the aft ladder. Do not reopen the fore-aft ladder to serve it.
- **Both halves of the pose machinery ALREADY EXIST and are gated:** the runner-leg
  target is R3-WS(c)'s full deck slide (`ws_foot_slide`, deck target, measured heel
  limit), and the kneeling leg is R3-WS(d)'s shard-guarded `kneel_construct`
  (chord floor 0.26, flexion 146.7°, knee on the measured pad). This rung is mostly a
  new **asymmetric per-side blend** plus an input — not new geometry.
- **The appearance debt travels with it:** the same deep-flexion mesh interpenetration
  that deferred the fit will show on the kneeling leg here. The mesh rung is a
  prerequisite for *shipping* the look, not for prototyping the move.
- **`P` hold-vs-toggle is an OPEN RULING.** "pull a side / unstick it" reads as a HOLD
  while you haul; confirm with Chad before building.
- **The survival function is a KERNEL ask, not animation.** Righting moment, tip-over
  recovery and pre-emptive hillside stabilisation all require the pose to move real
  physics. See `docs/KERNEL_WS1_SPEC.md` for the coupling facts: today `cg_off` moves
  only the three contact-patch mounts, and rider inertia is held constant under lean
  (named ~16 % roll error at full stand lean). **Do not build this as animation-only and
  claim the survival function.**
- **Hands:** at a full side hang the inboard hand may have to release the bar. The
  welded-hands law (both hands pinned to the grips) would fight that — **a ruling to take
  before building**, not a thing to solve unilaterally.
- Related open items already filed: `OPEN-R3WS-STANDSLEW` (the stand key already spends
  96 % of the pose-continuity budget), `OPEN-R3WS-STANDKNEE`, `OPEN-R3WS-STEERREACH`.
- Two riders law still applies (`docs/RIDER_AUTHORITY.md`): the 42-bone Sudburian only.
