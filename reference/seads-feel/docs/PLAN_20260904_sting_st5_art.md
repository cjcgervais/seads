# PLAN 2026-09-04 — ST-5 ART: hero Sting GLB + gunstock launcher + seat-deploy animation

Lane: `sandbox/sting-st5` in `D:\seads_sandboxes\sting-rpas`, base = origin/main
5029294e9 + merge of `sandbox/game-loop` @ 59c660470 (the P-from-the-seat kernel,
per `docs/PACKET_TO_STING_ANIM_from_gameloop_20260904.md`). Chad's ask (2026-09-04):
replace the procedural Sting with a hero-quality Blender model resembling the
Ukrainian Wild Hornets Sting; build the gunstock launcher; 3 s deployment animation
from the seat (P alone, no J — kernel already built and flown); he approves the
model LIVE in the Blender viewport. Kernel flight/launch/kill is DONE — this rung is
visuals only. Audio stays deferred (not in tonight's ask).

## Ruled architecture (from the code-seam survey, file:line verified)

1. **Sting model** = new `assets/drone/sting_src/` (build scripts + exporter) →
   `assets/drone/sting.glb`. Blend lives OUTSIDE the repo:
   `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\sting.blend` (own file —
   never indy650.blend; its full-export is a known mount-crasher).
   **Model frame law: nose = −Z, +Y up, total length 1.2 m** — matches the
   primitive block (`render/draw.cpp:3225-3269`) so `info.sting_draw` drops in
   unchanged. Arms at z=−0.25, stub wings z=+0.15 are reference stations, the hero
   shape refines them per the reference sheet (`docs/STING_REFERENCE_SHEET.md`).
2. **Renderer** = new `render/sting_model.{h,cpp}` cloned from `flak_model.cpp`
   (rigid cgltf load, parents-before-children order, per-prim material tint,
   lazy-load + graceful miss). Tint = quantized `render::team_colors().ally` (the
   flak idiom, one authority). Registered in the app-shell source list
   (`CMakeLists.txt:178-180`), NOT seads_render_core (raylib ban).
   Dial: `SEADS_STING_MODEL=0` → primitives (bit-identical fallback); hide the
   primitives only when the model is *ready* (loaded), the flak_gunner discipline.
3. **Launcher** = second GLB `assets/drone/launcher.glb` (same src dir), rigid,
   with REQUIRED stations `st_grip_l`, `st_grip_r`, `st_shoulder`, `st_sight`,
   `st_muzzle` (fail-loud in the exporter, <32-char names). Placement is solved
   from the HANDS + AIM AXIS (the flak_pose lesson at `render/flak_pose.h:89-110`:
   weld hands, derive the shoulder contact).
4. **Deploy animation** = app-owned scalar `sting_deploy_t` stepped on clamped_dt
   (the `flak_ext_t` pattern, `app/main.cpp:4826-4847`): 0→1 over 3.0 s eased when
   `sting_shouldered` rises, CUT to 0 the frame the stance is lost (packet §3.5 —
   cut, not only blend). Shipped to render as ONE new FrameInfo field
   (default 0 = bit-identical). The launcher pose interpolates: stowed frame
   (under-seat, tucked beside the tunnel) → shouldered frame (stock at shoulder,
   sight on the aim ray from `sting_aim_dir`). The rider's arms follow the grips
   via the shortest-arc joint-posing idiom (`flak_gunner.cpp:600-622`) applied to
   the seated rider's arm chain; pelvis/legs keep the seated pose.
   Smoke dial: `SEADS_STING_DEPLOY="t"` pins the blend (mirrors `SEADS_FLAK_EXT`).
5. **Launch-origin reconciliation** (packet §2): after the shouldered pose exists,
   MEASURE the muzzle height over the sled frame origin on the posed rider and
   send the number back to the game-loop lane (they move the 1.35 m dial).
6. **Free look**: no new camera work — the deploy/aim pose is drawn in 3rd person;
   the existing freelook orbit sees it.

## Build phases (each verified in the live viewport; Chad approves the look)

- **A — hero Sting model** (Opus builder, live Blender session on this machine,
  MCP port 9876, fresh file). Reference: `docs/STING_REFERENCE_SHEET.md` +
  `D:\flight_sim2\Game_loop_idea\Reference_pics\sting\`. Hero quality: real motor
  pods, prop discs, camera pod, antennae, panel splits, material slots
  (carbon-dark vs body — body slots get the ally tint in-engine).
- **B — exporter + renderer wiring**: exporter = flak's `_repo_root()` +
  REQUIRED-station fail-loud wearing indy650's tmp→verify→atomic-move `run()`;
  `render/sting_model.{h,cpp}`; primitive block becomes the fallback arm.
  Build `--target seads`, sting subset + seam subset.
- **C — gunstock launcher model** (same session): compact NLAW-ish gunstock with
  a rail cradle holding the Sting, station empties, cool factor mandatory.
- **D — deploy animation + pose**: `sting_deploy_t`, stowed→shouldered launcher
  path, arm chain, FrameInfo field, `test/unit/test_sting_pose.cpp` property test
  (welds hold, no limb stretch, cut works). Measure + packet the launch origin.
- **E — gate + land**: per-round subsets only while iterating; ONE full detached
  gate when the session's work is committed (the paid-twice lesson); red set ==
  baseline BY NAME via `tools/gate/gate_baseline.py check`; PACKET back to the
  game-loop lane; land per `docs/CONTRIBUTION_SOP.md` after Chad signs.

## Traps honoured
- Never headless; never another lane's Blender session (sudburian_head's was on
  9876 earlier tonight — verify the scene answering the port before ANY write).
- Single writer: exactly one agent drives the session at a time.
- No absolute paths in exporters/build scripts (the export_flak cross-worktree law).
- `.blend` saves ≠ export: the GLB ships only through the exporter's verify gate.
- Announce edits to `app/main.cpp` P block / `app/player_mode.h` /
  `render/draw.h` FrameInfo to the game-loop lane (SOP §5) — one field ask is
  pre-authorized shape per their packet ("we will add sting_seated in one line if
  you want it named").
- Commit only this lane's own changes; kills scoped by worktree path.
