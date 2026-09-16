# HANDOFF — 2026-08-18 (night), winter-gi / `sandbox/gi4-ride` → main + seads-recon

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, then
`docs/RIDER_AUTHORITY.md`, then THIS file (§1 first — the hands are now
HAND-TUNED in the .blend and the generator must NOT be re-applied), then
`docs/SESSION_HANDOFF_20260818d.md` for the R2c-M2 build. Do the item Chad
names."*

## 0. THE ONE-LINE STATE

| | |
|---|---|
| this rung | **R2c-M2 + Chad's hand-tuning session — SIGNED BY CHAD'S EYE 2026-08-18 night** ("and now the thumb is actually pushing the throttle … great job"). Committed on `sandbox/gi4-ride`, merged to `main`, pushed; `seads-recon` (Chad's fly tree, `sandbox/audio`) fast-forwarded to it |
| the file | `indy650.blend` saved 22:32 (backup `_backup_20260818/indy650.blend.pre_R2cM2_handtune_2232.bak`); `assets/sled/indy650.glb` spliced (bar: 125 other nodes byte-identical; rider: 132 machine nodes byte-identical, skinned rest == live posed 0.000001 m both ways) |
| gate | `[rider_pose],[rider_rig]` 50/50 (two arm rows re-pinned off the bytes; arm-mirror clauses relaxed BY RULING, see §2); full suite in the commit message; graph regenerated, `graph_query.py check` OK |

## 1. ★★★ THE HANDS ARE HAND-TUNED. THE GENERATOR IS NO LONGER THE SOURCE.

Chad drove ~45 one-move-per-message edits in the live session with the lead
as operator ("you are my eyes"), all as MESH/POSE edits on the live proxy and
machine objects — NOT through `mitt_geom.py`/`control_geom.py`:

- LEFT fingers: flipped 180° about their bone axis, seated on the knuckle
  line, walked (up/forward/medial/aft, ±10° turns, a 5° pinky hinge, plane
  aligned to the back of the hand, 180° about the plane normal, order swap by
  MESH ISLANDS index↔pinky/middle↔ring, +1°/+7°/+9° raises about the index
  knuckle, ⅛-in slides, pinky slimmed 8 % and re-seated, +½ in) — final
  centroid ≈ world (0.344, −0.343, 0.789).
- LEFT palm (`hand_l`): rebuilt from the saved mesh into a **capital L** —
  heel wrapping the bar (short leg), long leg straight to the finger bases
  (74 mm), turned 5° medial, then re-derived aimed at the finger-base
  centroid.
- Sleeves (`lowerarm_l/r` wrist ring): extended 49 mm past the wrist (the
  coat covers the wrist; the earlier "slide the wrist ring down the hand" was
  REVERTED — arms read short).
- Grips/bars: +1 in outboard each side, subtle 4 × 6 mm end flange
  (`grip_L/R` 16 → 40 verts; `bar_L/R` end ring moved).
- Poses: `ik_tgt_lowerarm_r` +6.35 mm aft then +12.7 mm outboard;
  `ik_tgt_lowerarm_l` +¼ in outboard then out to the flange (+29 mm) with the
  fingers counter-shifted so they stayed put; fingers then +⅜ in outboard,
  +½ in up.
- Throttle assembly (`throttle_block`): paddle 15 % shorter, base widened to
  the hinge (55 mm); box concentric on the bar (drop removed), lever swung
  parallel to the bar at bar-centre height; slid ⅜ + ¾ + 1 in inboard (now on
  the bar's bend), box centred on the bar's local centreline and turned 22°
  so the bar goes square through its centre. Chad: "now the thumb is actually
  pushing the throttle."

**Consequences (protective SOP, in force):**
1. `apply_live.py` and `apply_levers_live.py` now REFUSE unless
   `SEADS_MITT_REGEN=1` — running them replaces the tuned meshes from a
   generator that does not reproduce them. That flag means "Chad ruled a
   regeneration"; do not set it otherwise.
2. The `.blend` + the spliced GLB are the source of truth for the hands and
   the throttle assembly. `export_live.py` (now in `sudburian_src/`) +
   `splice_bar.py` + `splice_sudburian.py` is the ONLY path from the file to
   the game. Both splices verify before writing.
3. If the mitts ever need regenerating, the generator's parameters must first
   be re-fit to the tuned mesh (a future rung); until then treat
   `mitt_geom.py`/`control_geom.py` as history.
4. Blender GUI undo (Ctrl+Z) can roll back scripted edits made through MCP
   — it did once tonight (rolled the palm back). Check the markers in §3
   after any undo.
5. Exact reverts were done by reloading the affected vertex group from the
   SAVED file (`bpy.data.libraries.load` on a copy of the .blend — never a
   second Blender, never headless) and replaying; approximate inverse
   transforms drifted 3.7 mm once and were caught.

## 2. TEST RE-PINS (all measured, all commented in the code)

- `render/rider_rig.cpp` arm rows `rest_tip_minus_socket_m`: L
  (0.053508, 0.045236, −0.094443), R (−0.031599, 0.049249, −0.093675) — the
  hands slid along the bars independently, so the rows are no longer mirrors.
- `test_rider_pose.cpp`: fist bound 0.110 → 0.130 m; the arm rows' mirror
  clause now checks height/aft to 1 cm and leaves the along-bar component
  free (legs stay exact); the anti-mirroring frame-column test tolerates
  2e-2 on the upperarm (0.5° asymmetry) — a re-rolled bone still reads ~1.4.

## 3. MARKERS (re-check these after any undo / before any save)

`grip_L` verts 40 · `ik_tgt_lowerarm_l` ≈ (0.3412+0.0292·A…) ≈ world
(0.369, −0.175, 0.832) · `ik_tgt_lowerarm_r` ≈ (−0.3466, −0.1753, 0.836) ·
left-finger centroid ≈ (0.344, −0.343, 0.789) · sleeve ends 49 mm past the
wrist · proxy 3082 verts, rig 42 bones · `throttle_block` 332 verts, box on
the bar's bend square to its local run.

## 4. NEXT
Chad's eye is on the game build; then costume → scarf → ROM sweep; R3 wires
`throttle` to a rotation of the paddle about its box-vertical pin.
