# RIDER AUTHORITY — READ BEFORE ANY RIDER OR BLENDER WORK

**Chad, 2026-08-18, after the overnight session built hands on the wrong rider
for the second time:** *"Need a note in this codebase to make sure the agents
doesn't use the old rider again and NEVER operate headlessly."*

This file is that note. It is linked from `CLAUDE.md`, from the current
handoff's launch line, and from `SUDBURIAN_LADDER.md` §1. If you are about to
touch a rider mesh, a rider rig, a rider pose, `indy650.blend`, or anything
under `assets/character/` or `assets/sled/indy650_src/`, you have already read
this or you are already wrong.

---

## 0. THE THREE STANDING LAWS (Chad, 2026-08-18)

1. **Read and follow the spec** (`SUDBURIAN_LADDER.md` for the rider) before
   planning. Conflicts become ONE question to Chad, never a silent re-derivation.
2. **Graph, not grep** — `tools/graph/graph_query.py` first; `graphify.py`
   regenerated in the same commit as any structural change.
3. **The right model, in the open session** — §1 and §2 below.

## 1. THERE ARE TWO RIDERS. ONLY ONE OF THEM IS THE JOB.

| | ★ **THE SUDBURIAN** — the ONLY rider under development | ✖ **THE LEGACY RIDER** — CONDEMNED, FROZEN |
|---|---|---|
| Blender objects | `sudburian_rig` (ARMATURE, **42 bones**; **44 from R2c-7s(e) 2026-08-20** -- scarf_s01/s02, the moving short scarf tail), `sudburian_proxy` (MESH), collection `SUDBURIAN_R2b` (IK targets/poles) | `rider_rig` (ARMATURE, **19 bones**), `rider_body`, `rider_mitt_L/R`, `rider_boot_L/R`, `rider_helmet`, `rider_shield`, `rider_chin_curtain`; collection `RIDER` |
| Asset | `assets/character/sudburian_proxy.glb` | the rider nodes **inside** `assets/sled/indy650.glb` |
| Source | `assets/character/sudburian_src/` (`sudburian_proxy.py`, `mitt_geom.py`, `seat_sudburian.py`, `apply_live.py`, `export_sudburian.py`, `splice_sudburian.py`) | `assets/sled/indy650_src/indy650_fitout.py` (`patch_mitts.py` / `patch_costume.py` deleted by R2c-M) |
| Spec | `docs/SUDBURIAN_LADDER.md` — the R0→R2 ladder, every rung signed by Chad's eye | none. It is the R0 placeholder that the ladder exists to REPLACE (`SUDBURIAN_LADDER.md` §1: *"this ladder is a repair-and-grow"* — the grow is the Sudburian; the legacy figure is what is being grown OUT OF) |
| Vertex groups / hands | `mittfront_01_l/r`, `thumb_01_l/r` — R2c-1..5, driven by Chad through four corrections | 106-vert blobs (R0) → 1,910-vert generated mitts (R2c-6, overnight, **wrong rider, unsigned**) |
| Status | **CURRENT.** R2c in progress. Every rung gated by Chad looking at the viewport | **DO NOT TOUCH.** No mesh, no rig, no pose, no material, no "quick improvement". The only permitted change is the one that REMOVES it — the swap that puts the Sudburian into `indy650.glb` in its place |

**How to tell which one you are holding, without reading a name:** count the
bones. **19 = legacy. ANY OTHER COUNT = Sudburian** — 44 today (§2.2's 42 plus
`scarf_s01/s02`, R2c-7s(e) 2026-08-20); confirm with `scarf_s01` present.
★ AMENDED 2026-08-21: this line used to read "42 = Sudburian", and by then the
Sudburian was 44 — the one test whose whole job is to stop you building on the
wrong rider had gone stale and FAILED ON THE CORRECT RIDER. The Sudburian's
count GROWS every time the character does; the legacy 19 is FROZEN and never
moves. So test against the frozen number, never the moving one, and do not
re-pin this to a new literal next rung. `bpy.data.objects[...].data.bones` in
Blender; the skin's `joints` array in the GLB. Names lie in this asset
(`throttle_block` is on the brake side; `_L` is −X) — bone counts do not.

### Why this keeps happening — and how to not be the third

`SUDBURIAN_LADDER.md` §1 opens with *"The rider is not primitives.
`assets/sled/indy650.glb` … carries 1 skin / 19 joints"* — that paragraph
describes the LEGACY rider, as the audited starting point. Two agents have now
read it as "the rider is in `indy650.glb`, so work there." The overnight
session of 2026-08-18 was told *"the hands reach the game"* and, because the
game loads `indy650.glb`, patched 7,576 triangles of mitts and a full costume
onto the 19-bone legacy figure, then found *"the rider's forearms pass through
his own handlebars"* — on the rider that is being deleted. All green. Wrong
rider. **"The game loads it" is not "it is the rider."** The game loading the
legacy figure is the DEBT; the job is to make the game load the Sudburian.

**Rule:** the Sudburian goes INTO the game. Nothing goes onto the legacy rider.
If a task seems to require improving `rider_*` anything, stop — the task is
mis-stated or you have the wrong rider — and ask.

### Machine guard

`indy650_fitout.py` refuses to run unless `SEADS_LEGACY_RIDER_REMOVAL=1` is set
in the environment (`patch_mitts.py` / `patch_costume.py` carried the same
guard until R2c-M deleted them, 2026-08-18). That flag
means one thing: *"I am executing the swap that removes the legacy rider, on
Chad's instruction."* Setting it for any other reason is the violation this
file exists to prevent.

---

## 2. BLENDER: NEVER HEADLESS. ONE SESSION. THE OPEN ONE IS THE TRUTH.

Chad keeps `indy650.blend` OPEN in a GUI Blender session, often for days, and
that session is where the Sudburian lives and where he watches the viewport.
The standing rule from `blender-work-in-gui` / `blender-single-writer`,
restated with teeth:

1. **NEVER run Blender in `--background` / `-b` on `indy650.blend`** or on any
   `.blend` that may be open in a GUI. Not to "just read it". Not to export.
   Not to patch. Not on a copy you then save back. A background process
   loading the file while the GUI holds it produces two divergent truths and
   the loser is whichever one saves second — that is how work vanishes.
2. **NEVER open a second GUI Blender on the same file.** Same reason.
3. **The running session is not stale.** Before assuming anything about the
   file on disk, connect to the live session over MCP
   (`mcp__blender__get_scene_info`, `execute_blender_code`) and read the
   inventory FROM IT. `tasklist | grep -i blender` tells you it exists;
   `bpy.data.filepath` + `bpy.data.is_dirty` tell you what it holds. If a
   session is up and holds `indy650.blend`, THAT is the working file and you
   work THROUGH IT, in the viewport he can see. Handoff timestamps are older
   than the session; the session wins.
4. **If no session is up,** launch one YOURSELF, in the GUI:
   `"C:/Program Files/Blender Foundation/Blender 5.1/blender.exe" --python tools/blender/mcp_startup.py`
   then open the file. `mcp_startup.py` now refuses to run under
   `--background` (except its own `--keepalive` socket smoke, which loads no
   file).
5. **Re-read the inventory after every reconnect**, and never save the file
   without saying so — a save from your MCP call IS a save of Chad's session.
6. **Do not kill his session.**

### The one sanctioned background use

`assets/character/sudburian_src/build_proxy.sh` runs
`blender --background --factory-startup --python export_sudburian.py`. It opens
**no .blend file at all** — it builds the proxy from `sudburian_proxy.py` into
an empty factory scene and writes `sudburian_proxy.glb`, twice, proving the
bytes reproduce. It never touches `indy650.blend` and never touches the live
session. That is the ONLY headless Blender invocation permitted in this repo,
and it stays that way only as long as it opens no file. **Do not generalise
from it.** If Chad rules even that out, delete it and export from the live
session; do not argue.

---

## 3. WHAT THE 2026-08-18 OVERNIGHT SESSION LEFT (for the record)

Measured 2026-08-18 morning against the live session (pid 11540), its 04:16
autosave, the 05:48 `.blend1`, and the 08-17 `quit.blend`:

- The Sudburian is INTACT: 42 bones, 392-vert proxy, R2c hand vertex groups
  present, identical to the 04:16 autosave.
- The machine is INTACT: zero vertex-count changes on any object except
  `rider_mitt_L/R` (106 → 1,910, the legacy blobs replaced — wrong rider).
- The "butchered" viewport is a **HIDE-FLAG state**, not deleted geometry: 131
  objects carry `hide_viewport` / eye-hidden that were visible in the 08-17
  `quit.blend` (`arm_end_L` … `staging_snow`, alphabetical, plus the whole
  `RIDER` collection). It reverses with `for o in bpy.data.objects: o.hide_set(False); o.hide_viewport=False`
  — run in the LIVE session, and only when Chad says so.
- `gas_cap`, `indy650_dash_apron`, `indy650_steer_boot` are absent from the
  .blend (present in `quit.blend` 08-17 00:50; absent by the 04:16 autosave).
  Still in the shipped `indy650.glb`. Which session deleted them is not
  recoverable from the backups; recorded, not attributed.
- One orphan empty `SLED_ROOT.001` (no collection) arrived with the 10:51 save.
- Commits `e0aaf725f` (R2c-6 mitts) and `402440b75` (R2c-6a/R2c-7 costume) plus
  an uncommitted diff to `indy650.glb` / `patch_*.py` / `mitt_geom.py` are all
  LEGACY-RIDER work. Whether to revert them is Chad's ruling; nothing in them
  touched `sim/`, and none of it is signed.
