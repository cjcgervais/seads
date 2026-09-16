# R1a — THE CODE HALF OF R1. Build spec.

Read `docs/SUDBURIAN_LADDER.md` first — it governs. This file is the work order
for R1a only. Attempt 1 of 2.

## Why R1 is split, and what R1a is NOT

`docs/SUDBURIAN_LADDER.md` §6 says R1 repairs "defects 1-11 and 14" and in the
same breath says R1 **ships on the CURRENT mesh (Chad ruled)**. Those two
sentences cannot both be satisfied: defects **1, 3, 4, 5** are wrong bone
*lengths and spans inside the GLB rig*, and changing them means re-authoring the
armature in a `.blend` that (per §2.2a) is not reproducible from committed
source. So R1 splits on exactly the line the R0 recon already found:

- **R1a (this file) — pure C++, no Blender, no new art.** Defects **6, 7, 8, 9,
  10, 11, 14**. This is where "floaty" actually lives (§1.1: *"Floaty is defects
  6, 7, 8 and 9"*) — all four are in R1a. It ships on the current mesh and Chad
  can drive it before any character art exists.
- **R1b — later, needs the live Blender session.** Defects 1, 3, 4, 5. Chad has
  not yet been asked for a Blender window; do not start it.

**R1a therefore fully delivers the rung's stated purpose** ("this is the rung
that kills floaty, and it ships before any new art") without touching the rig.

## Standing laws that bind this work

- **§0.1 DETERMINISM.** Pose is a pure function of `(kernel state, tick)`.
  Nothing written back. No `GetFrameTime()` accumulation. No new persistent
  animation state. **Every value you add must arrive FROM `sim::SledState`.**
- **§0.2 KERNEL UNHARMED.** Zero changes under `sim/`, `control/`,
  `test/harness/`, `test/golden/`. You may *read* new kernel fields; you may not
  add, rename or re-tune one. If a fix seems to need a kernel number, **STOP and
  ask** — do not improvise.
- **§0.5 LAYERING.** Pure pose/IK math goes in `seads_render_core`
  (raylib-denied, therefore testable). Only mesh upload/draw stays in the `seads`
  exe target. `python tools/graph/graph_query.py check` green, graph regenerated
  **in the same commit**.
- **§0.8 LINE NUMBERS ARE STALE.** Re-locate every site by symbol, never by the
  line numbers quoted in §1.1.
- **§0.6 REFERENCES FIRST.** Open `assets/sled/indy650_src/X_rider.png` before
  committing geometry decisions.

## The work

### D7 + D6 — feet to the board socket (this is the big one)

`sled_model_draw`'s leg IK targets `wpos(sm.rest_world[sm.leg[s][2]])` — the
**rest-pose foot position**, a constant. `sm.n_board[s]` is bound and then never
used for the leg target. So the boots track nothing: they hang in the rest pose
while the hips lean away, the leg IK saturates at `dmax` 0.8182 m at full lean,
and the hip visibly detaches. That is the floaty read.

Fix it the same way the arms already work — and copy that pattern deliberately,
because it is correct and proven. The arms capture
`sm.hand_off[s] = inverse(rest_world[n_grip[s]]) * rest_world[arm[s][2]]`
once at load, then each frame target `nodes[n_grip[s]].world * hand_off[s]`.
Do exactly this for the legs with `n_board[s]`, i.e. capture a `foot_off[s]`
at load and target `nodes[n_board[s]].world * foot_off[s]`.

**D6 is then a correction applied to that captured offset, not a second
mechanism.** Measured: `foot_L` sits 42 mm below `board_socket_L`, sole 117 mm
below, and 51 mm aft. Re-seat the captured offset so the **sole** rests on the
board's top surface and the ankle sits over it, rather than baking the authored
error in forever. State the numbers you chose and where each came from.

**★ THE TRAP — READ §2.3a BEFORE YOU TOUCH THIS.** In this GLB, `_L` means −X
across the whole asset, which on the rider is his anatomical RIGHT. It is
self-consistent — 36 of 36 machine nodes ending `_L` are at negative X — so
`foot_L` correctly pairs with `board_socket_L` **today**. Do not "fix" the
pairing. The crossover only becomes real at the R2 rename. If you find yourself
about to swap an index here, you have misread §2.3a; stop.

Also: `sm.nodes[sm.leg[s][2]].world` should be pinned and `world_override` set,
mirroring the hand, so the settle pass does not undo it. Check whether the boot
mesh is a child that needs the same treatment.

### D8 — wake `absorb`

`SledRig::absorb` is documented in `render/sled_model.h` as *"no kernel state
yet -- live 0"* and is hardcoded 0 except when the smoke-test env var forces it.
There is no terrain-reaction channel at all, so the man does not react to the
ground. Give it a real driver **read from existing kernel state**:

- `sim::SledState::susp_x[kPatches]` (compression, m) is **already plumbed** to
  render as `info.sled_susp_x[3]` and used for `srig.susp_m`.
- `sim::SledState::susp_v[kPatches]` (compression **rate**, m/s) exists in
  `sim/sled.h` and is **not** plumbed. Rate is the honest bump signal — a
  compression *held* is a load, a compression *arriving* is a hit.

**Recommended:** plumb `susp_v` through the render info struct exactly as
`susp_x` already is (read-only, no kernel change), and drive `absorb` from it.
Compose absorb in `seads_render_core` as a **pure function** with unit tests —
that is the whole point of §0.5 — and keep the existing `0.12f` pelvis drop as
its consumer for now. Clamp to [0,1]. **No smoothing that persists across
frames** (§0.1): if you want temporal shape, derive it from a kernel quantity,
not from an accumulator you own.

If plumbing `susp_v` turns out to need a kernel-side change of any kind, fall
back to deriving absorb from `susp_x` above static sag (already plumbed, zero
risk) and **say so in your report** rather than reaching into `sim/`.

### D9 — the invented 90 mm hip shift

`stand_k` drives a `+0.09f * stand_k` forward hip shift and a `0.32f * stand_k`
pelvis pitch. `cg_off` in `sim/sled.cpp` never sees either, so the visual and
simulated CG disagree by up to 90 mm.

**Ladder default is DELETE, and delete is the tape-safe option (§6).** But note
the counter-evidence in the source: that block is labelled `★ DRIVE-2 FIX` and
carries Chad's own complaint — *"standing only pulls arms and legs in an
unnatural direction"*. It was added to fix a felt defect he reported.

**So: delete it, but do not lose the fix.** The straight vertical rise
hyperextends the welded arms; that is a real problem and it is *why* the shift
was invented. Once D7 puts the feet on the boards, the leg chain is no longer
fighting a fixed rest target and the stand may read correctly without the
invention. Your job is to **measure that**, not assume it. Report: with the
shift deleted and D7 in, does arm IK still saturate at full stand? If it does,
the honest answer is that the shift must be *promoted into the kernel* — which
is a kernel rung — so **STOP and report it as an open question for Chad**
rather than quietly keeping a 90 mm lie.

### D10 — the ski-angle lie

`kSkiRad = 25°`; kernel `steer_max_rad = 0.42` = 24.06°. Read the kernel constant
rather than retyping either number (§0.3, single source). `kBarRad = 32°` is
unbacked — leave the value alone unless you can source it, but comment where it
came from or mark it explicitly unsourced.

### D11 — the magic numbers

`0.25f` in the `stand_k` divisor duplicates kernel `stand_rise_m`; `sag0` 0.10 is
an env-overridable literal; head clamps ±1.4/±0.9 are unexplained. Single-source
what has a kernel owner, name and comment what does not. **Do not change any
value while renaming it** — a rename and a re-tune in one commit is unreviewable.

### D14 — the degenerate rest bend plane

`capture_chain` hard-codes `(0,-1,0.1)` when the rest chain is near-extended, so
the elbow direction is a guess and can flip. The ladder's structural answer is
§2.1's pre-bends (elbows 3°, knees 2°) — but that is an R2 rig change. For R1a,
make the fallback **deterministic and documented**: derive the bend normal from
the chain's own parent frame rather than a world-space constant, and add a unit
test that a near-extended chain produces a stable, non-flipping bend plane
across a sweep. If you cannot do better than a constant, say so plainly.

## Gates — all of them, no partial credit

1. **`tape_360_chad_repro` + the full sled golden set replay BIT-EXACT.** This is
   §0.1 and it is non-negotiable. R1a is render-side, so it *structurally* cannot
   move a tape — which means any tape movement is a bug you introduced, not a
   tradeoff to argue about.
2. Suite: **1205/1201 baseline at commit `82262a68c`**, the same 4 pre-existing
   GI4 reds, **zero new red**. Run `ctest --test-dir build -C Debug
   --output-on-failure`. **Never run two ctest gates against one build dir.**
3. New unit tests in `seads_render_core` for every pure function you add
   (absorb composition, foot-offset capture, bend-plane stability).
4. `python tools/graph/graph_query.py check` green; graph regenerated in-commit.
5. `indy650.glb` **byte-identical** to HEAD (md5 it before and after).
6. Zero diff under `sim/`, `control/`, `test/harness/`, `test/golden/`.
7. Fold in the three R0 cosmetic follow-ups from the ladder: the `INFO(...)`
   scoped inside the `for` body in `test_rider_rig.cpp`, and the "~4k verts"
   comment that should read **9,069**. (The `.gitignore` item is already done.)

## Reporting

Report **measurements, not adjectives**. Specifically:
- the numbers you chose for the foot re-seat and their derivation;
- absorb's driver, its range over a real drive, and what it does at rest;
- the D9 answer: does arm IK still saturate at full stand with the shift gone?
- md5s and test counts before/after.

**You do not pass your own work** (ladder §sign-off record). A separate
verifier re-derives everything from the built artefacts. Do not sign R1a, do not
edit the ladder's status line, and do not commit to `main` — this is
`sandbox/gi4-ride` work.

The gate for R1 as a rung is a **VIDEO** (§0.4) and it is Chad's, not yours.
