# HANDOFF — 2026-08-18 (evening), winter-gi / `sandbox/gi4-ride`

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, then
`docs/RIDER_AUTHORITY.md`, then `docs/SESSION_HANDOFF_20260818b.md`, then
`docs/SUDBURIAN_LADDER.md` §6 R2c-S. Do the item Chad names."*

Query `tools/graph/graph_query.py` instead of grepping; regenerate the graph
(`python tools/graph/graphify.py`) in the SAME commit as any structural change.
Read the spec before planning. Work in the OPEN Blender session, never headless.

---

## 0. THE ONE-LINE STATE

| | |
|---|---|
| worktree | `D:\seads_sandboxes\winter-gi` |
| branch | `sandbox/gi4-ride` — **NOT pushed** |
| HEAD | `f671776a4` — *R2c-S(b): the live handlebar assembly is in the file* |
| gate | **1237 / 1241** — the same four `test/unit/test_sled.cpp` cases as before (GI4 debt: `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`). Nothing new red. `[rider_pose],[rider_rig]` 50/50 |
| blend | `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\indy650.blend` — NOT in this worktree. Saved 2026-08-18 12:03 from the live session (legacy rider + rodin candidate re-hidden). Backups: `D:\seads_sandboxes\_backup_20260818\` |
| Blender | a GUI session was OPEN all day (pid 11540). Connect over MCP and read its inventory first; if it is gone, launch one yourself with `tools/blender/mcp_startup.py` (GUI). NEVER `--background` on this file |
| the rider | **THE SUDBURIAN IS IN THE GAME.** `assets/sled/indy650.glb` carries the 42-bone `sudburian_rig` + `sudburian_proxy` (R2b grey blockout, seated), on the LIVE handlebar. The legacy 19-bone rider is GONE from the file |
| working tree | clean apart from untracked `conquest_tape_*.jsonl` and gitignored `sud*.png` / `*.sledtape` |

---

## 1. WHAT HAPPENED TODAY (in order)

1. **Morning — the wrong-rider overnight was found and reverted.** The
   previous overnight agent built "R2c-6 leather mitts" and "R2c-7 costume"
   on the 19-bone LEGACY rider inside `indy650.glb`, saved Chad's open .blend
   twice, and left the viewport with 131 objects hidden. Geometry was intact
   (measured against the autosave/.blend1/quit.blend). Chad ruled: revert.
   `a37041ef1` reverts `402440b75` + `e0aaf725f` (GLB byte-identical to
   R2c-5 again; the one real fix, `export_indy650.py`'s `.new.glb`, kept).
   The mitt GENERATOR was salvaged, unwired, to
   `assets/character/sudburian_src/salvage_from_legacy_rider/` (README says
   what is reusable). Live session unhidden, legacy rider + rodin candidate
   re-hidden, saved.
2. **The law.** `docs/RIDER_AUTHORITY.md` (`f1504f950`) + the three standing
   laws at the top of `CLAUDE.md` (`b410fbf81`): read+follow the spec; graph
   not grep; the right model in the open session. Guards:
   `indy650_fitout.py` refuses without `SEADS_LEGACY_RIDER_REMOVAL=1`;
   `tools/blender/mcp_startup.py` refuses `--background`.
3. **R2c-S — the Sudburian into the game** (`f18ad1781`). Ladder §6 R2c-S
   has the full account. Short form: selection-only POSED export of the rig +
   proxy from the open session → `splice_sudburian.py` swaps it into
   `indy650.glb` (30 legacy nodes out, 132 machine mesh nodes byte-identical,
   skinned rest = live posed to 0.000000 m) → catalogue in UE5 names with the
   L/R crossover written once → runtime root/pelvis frames CAPTURED not
   assumed → `kBootReseatM` = 0 → tests re-pinned.
4. **R2c-S(b) — the live handlebar too** (`f671776a4`). R2c-S measured the
   file's grip sockets 100 mm AFT of the live bar (stale machine export).
   Chad: *"splice the bar/grip nodes from the live session too."*
   `splice_bar.py` carried `CH_steer_pivot` + bars/grips/sockets/levers;
   bellcrank + tie-rods HELD (steering linkage is deferred). Wrist now one
   fist (98 mm) from the socket = knuckle on the bar; throttle drops the
   right elbow 17.6 mm / lifts the wrist 25.5 mm, brake +8.7 / −31.7 mm.

---

## 2. THE PIPELINE, AS IT NOW STANDS (this is what you will re-run)

```
open Blender session (Chad's, or launch your own GUI one)
  │  select sudburian_rig + sudburian_proxy
  │  bpy.ops.export_scene.gltf(use_selection=True, export_apply=False,
  │      export_yup=True, export_skins=True, export_animations=False,
  │      export_rest_position_armature=False,   # <-- POSED node TRS
  │      export_def_bones=False)                # <-- all 42, incl. root
  ▼  scratch/sudburian_seated.glb  (+ a ref.json of live posed verts, optional)
python assets/character/sudburian_src/splice_sudburian.py <seated.glb> [--ref r.json] [--dry-run]
python assets/character/sudburian_src/splice_sudburian.py --check <seated.glb>
  ▼  assets/sled/indy650.glb  (verified on read-back; machine bytes untouched)
python assets/character/sudburian_src/splice_bar.py <bar_live.glb> [--ref r.json]
  (only when the bar assembly moves in the .blend; select CH_steer_pivot +
   the ten bar/grip children, same export options minus the skin ones)
```

Both scripts are stdlib, re-runnable, and REFUSE to write unless every
verification passes. Rules they encode: names, never node indices, across a
compaction (the bar splice's own verify caught a 0.92 m bellcrank jump from
exactly that); the machine's mesh bytes are fingerprinted before and after;
the exporter's `world × IBM` convention matches the runtime's (measured
0.000000 m — do not "fix" either side).

**What changes when the rider is re-exported:** anything pinned in
`render/rider_rig.cpp` (segment lengths, `rest_tip_minus_socket_m`) and the
hinge constants in `test_rider_pose.cpp` (`a 0.126356 / b 0.070138`). The
tests name the joint that drifted; re-measure, do not loosen.

---

## 3. ★ THE LESSONS

1. **Two riders existed and two agents built on the wrong one.** Tell them
   apart by BONE COUNT (42 vs 19), never by "the game loads it". Law:
   `docs/RIDER_AUTHORITY.md`. The legacy figure is now GONE from the file, so
   the trap is smaller — but `indy650_src/*.py` still look it up by name and
   the .blend still contains `rider_rig` (hidden). Do not resurrect it.
2. **The running Blender session is the truth, not the handoff timestamp.**
   The overnight agent treated the live session as stale. Connect first, read
   `bpy.data.filepath` / `is_dirty` / the inventory, and work THROUGH it.
3. **Verify the artefact, not the process.** Every splice compares WORLD
   positions and skinned vertices against the session's evaluated mesh, and
   fingerprints the machine's bytes. Both scripts caught real bugs of mine
   before writing (stale indices; a wrong-child re-parent).
4. **Derive frames, don't assume them.** The legacy `root` had identity rest
   under an unrotated armature; the Sudburian's armature is yawed 180° and
   its root is a −90° X ground bone. `sled_model.cpp` now captures
   `root_parent_R` / `root_R` / `root_q` at load and pushes every model-frame
   demand (lean, absorb, hinge) through them. Identity on the old rig, right
   on the new one.
5. **A stale export bites through the geometry it defines.** The 100 mm bar
   staleness showed up as "the wrist roll lifts 2.5 mm instead of 46" — a
   symptom two rungs away from its cause. When a measured offset looks like
   a fist but points the wrong way, suspect the DATUM before the pose.
6. **The saved file and the session are two saves apart at any moment.**
   Back up before every save of Chad's file, say that you saved, and say
   what changed (`_backup_20260818/`).

---

## 4. OPEN / NOT MINE

- **The .blend cannot re-export the machine**: `gas_cap`, `indy650_dash_apron`,
  `indy650_steer_boot` are gone from it (still in the GLB), `sudburian_rig`
  sits renderable in the scene root, and it carries unsigned L5/L6. The
  splice route exists BECAUSE of this; a proper machine re-export is a Chad
  ruling with those three settled first.
- **Bellcrank + tie-rods** stayed at their file positions while the steering
  head moved 100 mm forward. Under the hood; steering is deferred by Chad.
- **Levers unplaced, throttle paddle unbuilt** (SESSION_HANDOFF_20260818 §5).
- **ROM sweep** still does not exist (R2c gate).
- **Head cam-follow** on the Sudburian's `head` bone axes is untested by eye
  (yaw about local Y, pitch about local X — same convention as the legacy
  head bone; check once in the running game).
- **R2c-5 magnitudes vs Blender**: game −17.6/+25.5 vs Blender −35/+46 for
  the throttle. Same directions; the difference is the roll pivot (socket vs
  knuckle target). Chad judges by eye; not chased.
- **Export target ruling** (ladder §2.2b): the GLB changed on the sandbox
  branch, not main. Merge to main when Chad signs.

---

## 5. NEXT (the ladder's order — ask Chad which)

R2c remaining art, on a target that now draws:
1. **Leather mitts** — generator salvaged at
   `assets/character/sudburian_src/salvage_from_legacy_rider/mitt_geom.py`
   (bar-datum'd, census incl. directed-edge winding + dihedral; README says
   what is wrong-rider). Wire INTO `sudburian_proxy.py`'s hand volumes with
   `anchor = centre` (the bar), in the live session, Chad watching. Then
   re-export → `splice_sudburian.py`.
2. **Costume** (§4.1 as RULED: grey one-piece, kComplementBlue stripes,
   black boots, full-face helmet + balaclava question), **scarf** (§5).
3. **ROM sweep** (the R2c gate).

---

## 6. COMMANDS

```sh
# gate (Git Bash). NEVER run two ctest gates against one build dir.
cmake --build build && ctest --test-dir build -C Debug --output-on-failure

# the rider tests only
./build/seads_tests.exe "[rider_pose],[rider_rig]"

# the game (RelWithDebInfo; Debug lies about feel)
cmake --build build-play --target seads

# shots -- fields: steer,sL,sR,sT,lat,up,fwd,absorb,thr,brk ; needs ~40 frames
SEADS_SLED_CTL_DEBUG=1 SEADS_SLED_DEBUG_MODE=1 SEADS_SLEDCAM="2.2,35,12" \
  SEADS_SLED_RIG_SMOKE="0,0,0,0,0,0,0,0,1,0" ./build-play/seads.exe --smoke 40 x.png

# the asset checks
python assets/character/sudburian_src/splice_sudburian.py --check <seated.glb>
python tools/graph/graph_query.py check
```

Today's shots: `sud_sheet.png` (neutral / fwd / stand / lat / thr / steer,
pre-bar) and `sud2_sheet.png` (neutral / thr / brk / steer, live bar), repo
root, gitignored.
