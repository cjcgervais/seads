# R1a — ATTEMPT 2 OF 2. Build spec.

Governed by `docs/SUDBURIAN_LADDER.md`. Attempt 1 was `078e2beab`, independently
verified, driven twice by Chad. This is the last attempt on R1a, so **measure
before you change, and report anything you could not close.**

Every number below was measured by an independent read-only agent using its own
GLB parser with full parent chains and full CPU skinning, replicating
`sled_model_draw` exactly. Its harness reproduced every published R1a figure
(`dmax` 0.6646620, arm 0.7301 / 0.9712, box worst 1.4204, leg worst 1.0373,
`|cross|` 0.10982) before it measured anything new. **You may trust these numbers
as a starting point but you must re-derive any you depend on.**

## Chad's rulings, 2026-08-17 — these are decisions, not suggestions

1. **"Pretty far up" means FAR FORWARD, up the machine — not high.** The fix
   direction is **aft / hinge**, never down. The seat measurement (he sits 65 mm
   *low*, hips only 40 mm above the cushion, backside 72 mm inside the seat mesh)
   is a **separate R1b problem** and is out of scope here.
2. **The dash fix is: HINGE THE TORSO instead of translating the root.** He chose
   this over an aft mount offset, over promoting the shift into the kernel, and
   over waiting for R1b. See §D — and read §D's CG requirement before you write a
   line of it.
3. **Build all four fixes now**, in this attempt.

## Standing laws (unchanged, and §0.2 is absolute)

Zero changes under `sim/`, `control/`, `test/harness/`, `test/golden/`. Zero
change to `assets/sled/indy650.glb` — md5 it before and after
(`edf1ebdcfe70b68c41c444f651b6acb1`). Pose stays a pure function of
`(kernel state, tick)`: **no accumulator, no `GetFrameTime`, no smoothing, no
persistent animation state.** Pure math in `seads_render_core`. Re-locate every
site by symbol. All six tape gates must replay bit-exact. Baseline suite is
**1224/1220 at `5ee1bf4d6`** with the same 4 pre-existing GI4 reds (519, 520,
553, 556); land zero new red.

---

## §A — Move absorb's consumer from `pelvis` to `root` (fixes F3)

**F3 is confirmed by eye.** Chad: *"I saw the torso go down on a bump."*

Mechanism, measured: absorb's only consumer is `t.y -= kAbsorbPelvisDropM *
absorb` at **`pelvis`**, but `thigh_L/R` are children of **`root`**. So the legs
cannot respond at all — the measured leg reach ratio is **0.5324 at every absorb
value from 0 to 0.937**. The torso telescopes into the hips instead of the knees
bending.

**Fix:** apply the drop at `root`. The thighs then descend with the torso, and
the boots — welded to `board_socket` by attempt 1 — force the leg IK to fold.
The rigid boot is the anchor that makes this work.

Measured consequences of `absorb_at = root`:

| pose | quantity | pelvis (today) | root |
|---|---|---|---|
| seated, absorb 0 → 0.937 | knee y | 0.5925 → **0.5925** (frozen) | 0.5925 → **0.5731** (bends) |
| seated | leg ratio | 0.4477 → 0.4477 | 0.4477 → **0.3510** |
| stand | leg ratio | 0.7124 → 0.7124 | 0.7124 → **0.5885** |
| both | **arm ratio** | 0.7301 → 0.6639 | **identical** |

- **Arm cost is exactly zero.** The shoulder chain runs root→pelvis→spine either
  way, so the shoulder drops the same amount under every consumer. Verify this
  yourself; it is the reason this fix is safe.
- **The leg ratio only ever moves AWAY from the 1.0 clamp.** The recorded box
  worst 1.0373 occurs at absorb = 0, which is identical under every consumer, so
  **the worst-case number on record cannot be worsened by this change.**

**The one real cost, and it is pre-existing:** at `fwd 0.45` the knee is *already*
27.8 mm below the board deck plane at absorb 0 (0.2242 vs deck 0.2520). Full
root-drop at absorb 0.937 takes it to 0.1301 — **122 mm below the deck.**
**Therefore split the drop: ~0.6–0.7 to `root`, the remainder at `pelvis`.** The
measured split keeps most of the knee bend (crouch knee 0.1765 vs 0.1301 at full
root) for half the excursion. Name the split as a constant, single-sourced, and
**report the knee-below-deck figure at your chosen split.** If §D's hinge changes
the full-forward knee pose (it should — the hinge stops translating the lower
body), re-measure this *after* §D and pick the split on the combined result.

## §B — Shape the hit term (kill the 60 mm single-tick step)

Measured tick-to-tick `|Δabsorb|`: p50 0.0001, p99 0.033, **max 0.500 — a 60 mm
pelvis jump in one 1/120 s tick.** Cause: `hit = v / (v + kAbsorbHitHalfMs)` with
`kAbsorbHitHalfMs = 4.0` while `max(+susp_v)` peaks past **90 m/s**, so a
one-tick spike delivers most of the crouch.

§0.1 forbids filtering, so **this is a shaping problem, not a smoothing one.**
Three pure levers, all render-side; choose with measurement, not taste:
- raise `kAbsorbHitHalfMs` toward the measured p99 band (measured p90 ≈ 1–2.5 m/s,
  p99 ≈ 23 m/s; 4.0 currently sits just above p90, which is why ordinary chatter
  moves it);
- apply a power to the `hit` term;
- reduce `kAbsorbPelvisDropM`.

**Report the resulting `|Δabsorb|` p99 and max over the same tapes**, and the new
absorb distribution (p50/p90/p99/max) so it can be compared against the recorded
0.010–0.050 / 0.26–0.35 / 0.500 / 0.937133. Do not make absorb inert — Chad saw
the torso move and the channel reacting to terrain is a *gate criterion*; the
defect is the step, not the existence.

## §C — Correct `kBootReseatM.y`: the surface it was derived from does not exist

`render/rider_pose.h` documents `+0.066849` as derived from "`board_L`
running-board mesh, y bbox = [0.252, 0.277] → DECK TOP SURFACE = +0.277".

**0.277 is not a deck.** It is the top edge of the **outboard lip** (53 X-facing
triangles, y 0.2527–0.2687). The actual deck sheet is flat at **y = 0.2520**,
spanning x |0.210…0.375|, z −0.990…+0.305, with the lip ramping up to 0.277 at
x |0.381|.

**Correct value: `0.252000 − 0.210151 = +0.041849`** — 25.0 mm less. The boots
currently float **25.0 mm above the running board.**

Measured cost: dash clearance −1.35 mm, leg clamping 0.4 % → 0.8 %, arm worst
unchanged at 1.4204. Take it — it is a correctness fix and the number it replaces
was derived from a surface that isn't there. **Rewrite the comment so it records
the deck sheet at 0.2520 and explicitly warns that the bbox top is the lip.**

**★ ALSO REPORT, DO NOT FIX (it is GLB work, R1b/cowl ladder):** `board_L/R` have
**zero up-facing triangles** — all 58 horizontal triangles are wound −Y with
authored normals −Y (winding and normals agree, 115/115). With backface culling
in its default state the deck **is not drawn from above at all**, so the boots
stand on an invisible surface. This is very likely a large part of what Chad saw.
File it in the ladder as an OPEN item against the machine art; do not touch the GLB.

## §D — HINGE THE TORSO (Chad's ruling) — and the CG requirement is the whole job

### The measured problem

`root` carries the three kernel metres rigidly, so forward lean is **450 mm of
rigid fore-aft body translation against 14.35 mm of clearance**. Measured
boundary at `up 0`: contact begins at **fwd = 0.0185 m — 4.1 % of the range.**
**The rider is inside the console over 71–96 % of the forward-lean axis at every
stand value**, and `sled_lean_fwd` is a persistent mouse accumulator with
`lean_return_tau_s = 0.0`, so full lean is **one palm swipe** away. Worst
offenders: `rider_body` (spine3) vs `indy650_gauge_box` 5.93 mm and
`indy650_dash_apron`, plus `rider_helmet` vs `indy650_cowl` at high absorb.
The knee (`shin_L`) vs `indy650_console_rear` is the contact that starts first.

This pre-dates attempt 1 (pre-R1a boundary was 0.0492 m; the boot reseat lifting
the knee 13.2 mm cost 30.7 mm of usable travel). **Attempt 1 revealed and
worsened it; it did not create it.** Chad confirmed that reading.

### ★ THE DESIGN CONSTRAINT THAT MAKES THIS HONEST — read twice

A naive hinge is **exactly the defect D9 just deleted**: render inventing motion
the kernel does not know, so visual and simulated CG disagree. Do not repeat it.

But note what the current code actually gets right: translating the whole body by
`lean_fwd_m` moves the rider's CG by **exactly** `lean_fwd_m`, which is precisely
what `cg_off` in `sim/sled.cpp` applies. **The present translation is CG-honest.**
A hinge that merely rotates the torso by some invented angle would break that.

**Therefore the hinge must be SOLVED, not authored: choose the hip hinge angle
such that the rider's mass-weighted CG displacement equals `lean_fwd_m`.**

That gives all three properties at once:
- **CG honesty is preserved** — visual and simulated CG still agree, to the
  tolerance you report. This is strictly better than an invented angle and it
  keeps §0.1/§0.2 satisfied without a kernel change.
- **The dash clears**, because the pelvis and knees stop translating forward. The
  knee-vs-`console_rear` contact — the one that starts at 4.1 % of travel — is
  caused by whole-body translation and should largely disappear.
- **It is what a real rider does.** A rider leaning into the bars hinges at the
  hips; he does not slide bodily forward on the seat.

Requirements:
- Hinge about the **hip axis**, forward positive, on the same `spine`/`pelvis`
  chain the pose already poses. Hands stay IK-pinned to `grip_socket` (they
  already are); feet stay pinned to `board_socket`.
- Compute CG from the **rider segments only** with a documented mass
  distribution. §3's proportion table and the standard segment fractions it cites
  (Winter, after Dempster) are the source — do **not** invent weights, and cite
  what you use. If a full segment model is too much, use a defensible reduced
  model and **state its error against the full one**.
- **Solve, don't iterate to a tolerance you did not state.** Fixed iteration
  count or a closed form; deterministic; no accumulator.
- Aft lean (`lean_fwd_m < 0`, to `lean_aft_max_m` 0.25) must also work — hinge
  backward, or state plainly why it stays a translation.
- Keep the *lateral* and *vertical* channels as they are. This ruling is about
  the fore-aft axis only. Do not re-derive `lean_lat_m` or `lean_up_m`.
- **`lat_reach` is not constant** — `sim/sled.cpp` sets
  `lat_reach = 0.15 + 0.20 · clamp01(up / 0.25)`, so lateral reach is only 0.15 at
  a tuck. Sample the genuinely reachable set, not a rectangular box.

### What to report for §D, and it is the acceptance evidence

1. **CG error**: max |visual CG − `lean_fwd_m`| in mm across the reachable set.
   This is the number that says whether the hinge is honest. If it cannot be
   driven near zero, **stop and say so** rather than shipping a second 90 mm lie.
2. **The new contact boundary table**, same axes and format as the measured one
   (`up` × `fwd`), so it can be diffed against contact-at-4.1 %-of-travel.
3. Arm and leg reach ratios across the reachable set vs the recorded 1.4204 /
   1.0373 and 22.9 % / 0.4 % clamping. **The hinge rotates the shoulders forward,
   so it will change arm reach — report whether it helps or hurts.**
4. Whether the helmet now fouls anything, or drops below the windshield line.

---

## Gates — all of them

1. All six tape gates bit-exact (`tape_360_chad_repro`, `..._provocation`,
   `..._attempt1_provocation`, `tape_chad_flip_fence`,
   `sled_tape_round_trip...`, `sled_tape_off_arm...`).
2. Suite ≥ 1224 with the same 4 pre-existing reds, zero new red.
   `ctest --test-dir build -C Debug --output-on-failure`. **Never two gates on one
   build dir. Do not use `build-play` for tests** — it is RelWithDebInfo/NDEBUG.
3. New unit tests in `seads_render_core` for every pure function: the split
   absorb consumer, the shaped hit term, the corrected reseat, and **the hinge's
   CG-matching solve** (this one especially — assert the CG error bound).
4. `python tools/graph/graph_query.py check` green, graph regenerated in-commit.
5. `indy650.glb` md5 unchanged. Zero diff under `sim/`, `control/`,
   `test/harness/`, `test/golden/`.
6. Fix the F1 test defect while you are here: `TEST_CASE("the bend normal MIRRORS
   between sides")` asserts that the **normal** mirrors, which is the condition
   that makes the *bends* asymmetric — `cross` is handedness-flipping, so a bend
   normal must **anti**-mirror. Rewrite it to assert that `cross(n, dir)` mirrors,
   and fix the fallback so the elbow/knee direction does not flip between sides
   (measured dots: −0.7955/+0.7955 arms, +0.8490/−0.8490 legs). It is unreachable
   on today's asset (0.10982 / 0.11552 vs a 1e-4 threshold) but R2's pre-bends
   move chains toward straight, which is where it fires.

## Reporting

Numbers, not adjectives. State every constant you introduced and its derivation.
**You do not pass your own work** — an independent verifier will re-derive it.
Do not sign the rung, do not edit the ladder's status line, do not push, do not
commit to `main`. Commit with explicit paths only (untracked `conquest_tape_*.jsonl`
must not be committed).

**This is the last attempt on R1a.** If any of §A–§D cannot be done honestly,
build the rest and report the one you stopped on. A partial, honest attempt 2 is
worth more than four fixes with one lie in them.
