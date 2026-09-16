# HANDOFF — 2026-08-18 (evening), winter-gi / `sandbox/gi4-ride`

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, then
`docs/RIDER_AUTHORITY.md`, then `docs/R2cM2_CONTROLS_SPEC.md` (§1's ruling
lines v4→v8b ARE the current intent), then this file. Do the item Chad names."*

Supersedes `SESSION_HANDOFF_20260818c.md` (still valid for R2c-M history).

## 0. THE ONE-LINE STATE

| | |
|---|---|
| worktree / branch | `D:\seads_sandboxes\winter-gi` / `sandbox/gi4-ride` — **NOT committed, NOT pushed** (Chad rules commits) |
| this rung | **R2c-M2 — the gloves, the throttle and the brake, attempt 2 of 2, AWAITING CHAD'S EYE.** In the .blend (SAVED 18:33 after `_backup_20260818/indy650.blend.pre_R2cM2v8_1833.bak`), in `assets/sled/indy650.glb` (both splices verified), in the game (`sudM2_*.png` repo root, gitignored). Viewport renders: scratchpad `m2v8_*.png` |
| gate | `[rider_pose],[rider_rig]` 50/50; full suite **1237 / 1241** — the same four pre-existing GI4 `test_sled` cases (536/537/570/573), nothing new red; graph regenerated, `graph_query.py check` OK |
| the rider | 42-bone Sudburian; ONE-piece gloves (palm+knuckles+4 fingers+thumb+slim dorsum; cuff/strap/flare DELETED; no J-hook; sleeve ends inside the glove 13.4 mm); both hands the same half-closed fist (closure 22.2° / 20.9°) |
| the controls | `throttle_block` (rider RIGHT, world −X): black box on the straight grip run a −0.129..−0.097 dropped 25 mm, red 16×16×6 kill cap on top, box-vertical pin square on the aft face, straight 30° lever L 0.106 (taper 40°→24°, arc section from the pin), thumb bar-parallel with underside on the lever top (0.98 mm, contact at the root); `brake_lever` (rider LEFT, +X): black master cylinder + red blade ON its pivot boss (Q0 f 0.042 → tip 0.075), fingers curled onto it (0.35–0.95 mm) |

## 1. WHAT HAPPENED (short — the spec's ruling lines carry the full trail)

Chad's items on R2c-M attempt 1, in order of arrival: cuff connect / throttle
wedge / thumbs consistent / throttle placement 45° / brake piece + fingers on
it (this session's opening ask); ruling 7 (names+colours+sides swapped); v4
(straight paddle, vertical pin, bar plane, curve across the width); v5 (box
inboard, red kill cap, wedge wide→narrow); v6 (further medial, thumb parallel
to the bar); v7 (open the hand, thumb back 1–2 in); v8 (hardware square first,
drop 1 in, lever longer/shallower/gentler taper, ONE glove piece, no cuff, no
J-hook, right hand = left hand set back); v8b (blade attached to the cylinder,
fingers half-closed both hands, G9 deleted). Each was built, gated, rendered
and looked at before the next arrived; the .blend was saved at v5, v7, v8c.

Process: Opus built (two builders), Fable red-teamed the spec, verified v5
independently, and finished v8 when the second Opus builder hit a wall; the
lead did the arithmetic and the rulings between them (Chad's process ruling).

## 2. THE PIPELINE, AS RUN

```
edit sudburian_src/{control_geom.py, mitt_geom.py, sudburian_proxy.py}
python control_geom.py ; python mitt_geom.py         # rest-space self-tests, all gates
live: exec apply_levers_live.py                       # throttle_block/brake_lever mesh swap, materials, containment
live: exec apply_live.py                              # mitts in place, posed-world gates
live: exec verify_controls_live.py                    # G1..G6, G11, world==rest solve, renders
   -> back up; save; SAY SO
live: exec <scratch>/export_live.py                   # rider (posed) + bar assembly + both ref.json
python splice_bar.py scratch/bar_live.glb --ref scratch/bar_ref.json          # 125 other nodes byte-identical
python splice_sudburian.py scratch/sudburian_seated.glb --ref scratch/sudburian_ref.json  # 132 machine nodes, 0.000000 both ways
cmake --build build-play --target seads ; shots ; cmake --build build ; ctest
```
`export_live.py` lives in the session scratchpad — copy it into
`sudburian_src/` if you want it (it is 40 lines: selection export with
`export_rest_position_armature=False`, then the two ref JSONs).

## 3. LESSONS (new ones only)

1. **Photos of other machines are not the reference; Chad's machine is.** My
   v1 and the red-team's v2/v3 paddle readings both lost to his words.
2. **Hardware first, then the hand.** Every time a solver bent the lever or
   the lug to reach the thumb, Chad saw it as sloppy. Place the box, square the
   pin, size the lever; then move the hand.
3. **A visibility gate can push hardware off its mount** (G9 put the brake
   blade 36 mm off the cylinder). Gates encode the spec, not the reviewer.
4. **A 30 mm setback puts the knuckle line inside the tube** — solved 0.000;
   the "opened hand" reads through the half-closed fingers instead.
5. **The bend segment vs the grip axis**: a box on the bend has a local
   vertical that diverges 14° from the grip's; a bar-parallel thumb cannot rest
   on a lever pinned there. The furthest-medial box that keeps the hardware in
   the grip's plane is the inboard end of the straight run (a −0.099).
6. **`revert_mainfile()` is how a builder should bail out of a broken live
   state** — it discards only unsaved work; say so.
7. Vertex order in the proxy is load-bearing (splice + live apply index it);
   never re-sort `vols`.

## 4. OPEN / FOR CHAD

- Lever L 0.106 exits under the palm; contact is at the thumb ROOT (the
  lever's plan line crosses under a bar-parallel thumb at ~28 % from the root,
  so distal contact is geometrically unreachable without tilting the lever) —
  his eye decides length/height.
- Hand setback solved 0.000 vs his 30 mm (knuckle line enters the tube).
- Block containment lateral margin +0.35 mm at the bend start (ruled floor 2 mm
  needs the block 8 mm further outboard, under the thumb tip) — his call.
- ⚠ `indy_rubber` renders WHITE in workbench viewport shots (viewport diffuse
  0.8 vs Principled 0.018) — black in the game. Untouched, signed material.
- ROM sweep still does not exist; runtime wrist roll axis item unchanged; R3
  throttle animation hook = rotation about the box-vertical pin toward closed.
