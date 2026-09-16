# HANDOFF — 2026-08-18 (afternoon), winter-gi / `sandbox/gi4-ride`

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, then
`docs/RIDER_AUTHORITY.md`, then `docs/SESSION_HANDOFF_20260818c.md`, then
`docs/SUDBURIAN_LADDER.md` §6 R2c-M. Do the item Chad names."*

Graph not grep (`tools/graph/graph_query.py`); regenerate the graph in the
same commit as any structural change. Read the spec before planning. Work in
the OPEN Blender session, never headless. Supersedes `SESSION_HANDOFF_20260818b.md`
(still valid for R2c-S / R2c-S(b) history and the pipeline).

## 0. THE ONE-LINE STATE

| | |
|---|---|
| worktree / branch | `D:\seads_sandboxes\winter-gi` / `sandbox/gi4-ride` — **NOT pushed** |
| this rung | **R2c-M — leather mitts on the Sudburian, attempt 1 of 2, AWAITING CHAD'S EYE** (game shot: `sudM_sheet.png` repo root, gitignored; the viewport in his open session) |
| gate | `[rider_pose],[rider_rig]` 50/50; full suite **1237 / 1241** — the same four pre-existing GI4 `test_sled` cases, nothing new red |
| blend | `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\indy650.blend` saved 13:33 from the live session (pid 11540) after `D:\seads_sandboxes\_backup_20260818\indy650.blend.pre_R2cM_1322.bak` and `..._smooth_1332.bak`. Only `sudburian_proxy`'s mesh data (392 → 4,508 verts) and the new `sudburian_mitt_leather` material changed; rig, seat, IK targets, machine untouched. Seven 0-user `sudburian_proxy.00x` mesh datablocks are orphans (not saved) |
| the rider | 42-bone Sudburian, in `assets/sled/indy650.glb` (3.87 MB, 34 materials, proxy = 2 primitives, mitts smooth-shaded), on the live bar, WITH MITTS |

## 1. WHAT HAPPENED (short — the LADDER entry has the full account)

Chad's item: the left hand's fingers came off the BOTTOM of the fist
(`FINGERS_DROP`, R2c-4). Handoff-b NEXT item 1 was the mitts. The mitts
replace every hand volume, and the fingers now leave at the knuckle line, top
of the fist — MEASURED in the posed mesh against the real grip: +48 mm above
the axis, both hands.

Pipeline as run (this is what you re-run if the mitts change):
```
edit sudburian_src/mitt_geom.py / sudburian_proxy.py
   (python sudburian_src/mitt_geom.py           # generator self-test)
live session:  exec assets/character/sudburian_src/apply_live.py  (in-place mesh swap:
               proves body verts == source, rebuilds ALL 21 groups, smooth-shades the mitts,
               verifies the POSED verts against the REAL grip axis; does NOT save)
               -> look in the viewport; back up; save; say so
export selection (rig+proxy, posed) -> scratch/sudburian_seated.glb + ref.json
python assets/character/sudburian_src/splice_sudburian.py scratch/sudburian_seated.glb --ref scratch/sudburian_ref.json
cmake --build build-play --target seads ; shots (SEADS_SLEDCAM="2.6,-40,14")
```
`measure_grip_frame.py` (exec in the open session) re-derives the three
measured triples + R_BAR and must agree with the file to ~1e-5.

## 2. LESSONS (new ones only)

1. **`grip_frame`'s up default was a coin toss in Z-up space** — dot exactly
   0. A degenerate sign test is not a sign test; pass a measured up, assert
   the dot.
2. **The four census numbers cannot see "inside the grip".** Measure the
   radial distance of EVERY piece from the real axis; the cuff's hemispherical
   head cap reached the socket and the handback's width tilted radially, both
   with 0/0/0/0.
3. **The exporter ignores node-less material colours** (writes 0.8 grey).
   The grey blockout has shipped at 0.8 all along.
4. **verify(): all primitives, both ways.** A second material makes two
   primitives; one-way NN passes on missing geometry.
5. **Bash heredocs with `'''` inside were failing in this shell** — write
   edit scripts with the Write tool, run them with python.

## 3. OPEN / NOT MINE

- LADDER R2 "one material" vs §4 beige-yellow leather: a second flat material
  is the stand-in until textures — **Chad to confirm** (one question).
- Rigid 3-bone split of the mitt = R3 debt for the finger squeeze.
- Runtime wrist roll axis is `socket_R − socket_L` (pure X); the real grip is
  swept 19.6° — ~7 mm bore drift at 20°. Runtime item, unchased.
- Bar ends at the mitt's outer edge (grip end-plug visible in the bore).
- Levers unplaced / throttle paddle unbuilt (unchanged, Chad's ruling).
- ROM sweep still does not exist (the R2c gate).

## 4. NEXT (ask Chad)

1. Chad judges the mitts (attempt 1 of 2). Corrections → edit generator params,
   re-run §1.
2. Costume (§4.1 as RULED), scarf (§5), ROM sweep.
