# rig-D handoff — real Bf 109 F-4 mesh (RESUME HERE)

The `/orchestrate` rig-D thread: replace the Fleet Rig's placeholder cubes with a real,
high-detail Bf 109 F-4, split into 30 separable damage-component nodes, mirror-finish (monochrome
full-saturation) body + glass canopy + matte pilot. 3rd-person only. **Full plan:**
`~/.claude/plans/shimmying-discovering-manatee.md`. **Live status:** `docs/fleet_rig_plan.md`
(rig-D section) — read it first. This file is the operational resume guide.

## THE METHOD — reference-driven, build to quality exactness (Chad's standing instruction)
Do NOT free-generate shapes and hope. For every component:
1. **Reference.** Generate an accurate Bf 109 F-4 ortho plate with Nano Banana Pro:
   `python tools/gemini/seads_gen.py --image --prompt "<precise ortho request>" --out generated/bf109/ref_<view>.png`
   (defaults to `nano-banana-pro-preview`; mime auto-corrects the extension, e.g. `.jpg`).
   `generated/bf109/ref_side.jpg` already exists (accurate side profile — reuse it; add top/front
   as needed for wings/planform).
2. **Trace, don't guess.** For precision-critical shape, Opus authors the bpy DIRECTLY (a station
   table traced from the reference — see `generated/bf109/fuselage_v3.py` as the template). Use
   Gemini 3.1 Pro (`seads_gen.py --prompt-file ...`, recipe `bf109_bpy`) for verbose/low-risk
   parts, but ALWAYS review the draft (Gemini has dropped `(` on list rows — parse-check every
   draft: `python -c "import ast; ast.parse(open('f.py').read())"`).
3. **Build + compare to the reference silhouette; iterate to exactness.** Don't accept "generic."
   The fuselage took v1→v2→v3 to match the reference (asymmetric turtledeck spine, flat belly).
4. **Export + verify.**

## Operational commands
```bash
# 1. Launch GUI Blender with the socket (REQUIRED — the addon refuses -b / headless):
"/c/Program Files/Blender Foundation/Blender 5.1/blender.exe" --python tools/blender/mcp_startup.py &
#    wait ~10s, then confirm: python tools/blender/bmcp.py info
# 2. Build a component (runs bpy in Blender via the socket — no Claude Code MCP reconnect needed):
python tools/blender/bmcp.py exec generated/bf109/<part>.py
# 3. Screenshot to assess (SEADS side view — Blender's +Z-up != SEADS -Z-fwd, so use a custom
#    view quat; see the exec snippets in this session's history / fuselage flow):
python tools/blender/bmcp.py shot generated/bf109/_<part>_side.png --max 1100
# 4. Export one GLB per node key — export_yup=False preserves the SEADS frame (raylib does NO
#    axis conversion), so nose stays on -Z:
#    bpy: bpy.ops.export_scene.gltf(filepath=ABS, export_format='GLB', use_selection=True,
#                                    export_yup=False, export_apply=True)
# 5. Verify: GLB POSITION bounds have Z reaching NEGATIVE (nose -Z) and exactly ONE mesh.
```
Gemini model = `gemini-3.1-pro-preview` (text/bpy), `nano-banana-pro-preview` (images). Blender MCP
telemetry is OFF in `.mcp.json`. The `mcp__blender__*` tools need a session reconnect to appear —
you do NOT need them; `bmcp.py` is the direct-socket path.

## Node authoring rules (the contract — render/rig.h enum, 30 nodes)
- Author each component in its LOCAL frame, origin at (0,0,0) = the node's rest attach point.
- **Hinged surfaces** (ailerons, elevators, rudder, gear struts/doors/tailwheel): origin ON the
  hinge line; mesh-local hinge axis CANONICAL (+X pitch/roll surfaces, +Y rudder, +Z gear
  retract, +X tailwheel) — bake real sweep/dihedral into the rest orientation, NOT a tilted axis.
- **Chirality (Fable P0-1):** trailing edge toward +Z; L/R parts true mirrors. Backwards is the
  DEFAULT export failure — always eyeball the exported orientation.
- One GLB per node key (`assets/bf109/<mesh_key>.glb`), IDENTITY node transform, apply all
  transforms in Blender before export (Fable P1-1). Material class per node is in `rig.h`.
- Metre scale; watertight, quads/tris, outward normals (a MIRROR shows bad normals).

## The 30 nodes (mesh_key in rig.cpp aircraft_node_specs)
**DONE (30/30) — D.2 modeling COMPLETE (2026-07-09).** All 30 built reference-driven in ONE
maintainable assembly builder `generated/bf109/build_all.py` (transcribes the rig.cpp SPEC so
world placement is COMPUTED, screenshots the whole aircraft against `ref_side.jpg`), exported to
`assets/bf109/<key>.glb` (export_yup=False, verified: 1 mesh/node, SEADS frame preserved —
fuselage long axis stays on Z=7.6 — nose parts reach -Z, L/R true mirrors, hinged surfaces
origin-on-line). Fuselage RE-EXPORTED: the aft cone was held FULLER to the sternpost (the v3
needle tail read as detached from the empennage in the assembly check) — supersedes the prior
`fuselage.glb`. Verify report: `generated/bf109/_export_report.txt`; AABBs: rerun `python
/tmp/glb_verify.py`-style parse or see build_all SPEC.
- **Body:** engine_cowl, spinner, prop_blades(disc), canopy(glass), pilot(matte) ✓
- **Wings:** wing_l/r (airfoil loft, taper+dihedral+LE sweep), aileron_l/r, flap_l/r, wingtip_l/r ✓
- **Empennage:** vert_stab, rudder, hstab_l/r, elevator_l/r (stab TEs extended to meet hinges) ✓
- **Gear:** gear_door_l/r, gear_strut_l/r (TOP-PIVOT origin), wheel_l/r (axle X), tail_wheel,
  radiator, exhaust_l/r ✓
Then write `assets/bf109/manifest.toml` (node → glb / hinge origin+axis+chord side / material).

### Fable gear consult (2026-07-09) — RULINGS + deltas for D.3
Isolated stateless geometry consult (node table only). Verdicts:
- **Q1 retract axis:** 1-DOF about +Z suffices IF `R_world = rest_rot ∘ Rz(θ)` (animated inner,
  rest_rot outer). ALREADY satisfied — `apply_deflection` does `rest_rot * angleAxis(...)`. D.3
  just BAKES splay+rake into `rest_rot` (currently identity). Splay harmless; world-+Z + rake fails.
- **Q2 stance:** deck angle θ ≈ 12° nose-up, stable tripod (mains 0.91 m fwd of CG, ~85/15 load).
  Reconcile tailwheel contact to −0.76 (component stack), not −0.80. Classic 109 ≈14° = main strut
  +~0.2 m if Chad wants the steeper sit (PENDING his feel call).
- **Q3 doors — CHAD RULED (a), 2026-07-09:** the Bf 109 F has NO wheel-bay doors; KEEP the two
  `kLeftGearDoor`/`kRightGearDoor` nodes but re-purpose as STRUT-MOUNTED FAIRINGS — re-parent to the
  strut, ride the strut sweep (drop `Driven::Gear` on the door → static child of strut, or same
  sweep as parent). Keeps 30 nodes. Retract sweep gain ≈115° (2.0 rad) minus baked splay.
- **Q4 compression (out of scope):** if landing ever goes real → 1-DOF slide along strut-local −Y,
  ~0.18 m mains / 0.08 m tail. Record only; NOT built (landing = frozen-kernel, separate plan-mode).

### D.3 rig.cpp RECONCILIATION (the box_dims/rest_pos deltas the meshes require)
The D.1 contract dims were the placeholder-cube estimates; the real meshes differ. At ingestion,
set each `box_dims` = the exported GLB POSITION AABB (the D.1b validator's AABB-vs-box_dims leg
then passes FIRING). The notable ones + the two KINEMATIC fixes already baked into build_all's SPEC:
- **Gear (kinematic, must mirror into rig.cpp):** struts authored TOP-PIVOT (origin on the hinge
  line, mesh extends -Y to -1.05) so retraction rotates about the top, not mid-strut. So
  `wheel_l/r.rest_pos` {0,-0.65,0} → **{0,-1.05,0}** (strut bottom), and `wheel_l/r.box_dims`
  {0.65,0.65,0.25} → **{0.25,0.65,0.65}** (axle on X = correct rolling). `gear_strut` box y 1.2→1.08.
- **Stabs:** `hstab_l/r` box z 1.1→1.29, `vert_stab` box z 1.3→1.19 / y 1.4→1.42 (TEs extended so
  elevator/rudder hinges meet the stab TE — no gap).
- **Wings:** `wing_l/r` box y 0.28→0.42 (airfoil thickness + dihedral), z 2.0→2.05.
- **Bodies:** fuselage {0.95,1.35,7.6}→{1.06,1.31,7.6}; engine_cowl {..1.15..}→{1.06,1.08,1.6};
  canopy {0.75,0.55,1.9}→{0.69,0.53,1.9}; rudder z 0.7→0.55; radiator y 0.35→0.30.

## After D.2 (the rest of rig-D)
- **D.1b** (`test/unit/test_asset_validator.cpp`): add the cgltf headless-parse legs — real-vertex
  AABB-vs-box_dims, winding, hinge-origin-on-line, **chirality (P0-1)**, one-mesh/identity-transform
  (P1-1). Use vendored `build/_deps/raylib-src/src/external/cgltf.h`. Land WITH the first ingested
  GLB so each leg is tested FIRING (not a fixture-no-op).
- **D.3** (`render/draw.cpp` `ensure_fleet()`): per-node `LoadModel` from `assets/bf109/` keyed by
  the manifest (keep DrawCube fallback); material routing mirror/glass/matte; draw passes = opaque
  then ONE translucent pass (depth-test on / write off, global back-to-front per surface, prop disc
  additive — Fable P1-4). Glass/matte params → `config/world.toml [fleet_rig]` + `config/load_world.*`.
- **D.4:** full gate stack (validator → build → GLSL → seam grep → firewall differential → flight
  ctest LAST → visual smoke incl. extreme-pose gear sweep) → separate-context Fable ★ red-team →
  Chad flies. Commit green before any mutation-revert.

## State of the tree (as of this handoff)
- Committed on `sandbox/world-sudbury`: D.0 tooling (`c8fe49e4a`), D.1 30-node contract
  (`1bd25c010`), D.2 fuselage+method (`54fe34f61`), **D.2 COMPLETE — 29 remaining components +
  fuller-tail fuselage re-export (this commit).**
- Pre-existing UNCOMMITTED working changes NOT ours (Sudbury + env Stage 3): `config/world.toml`,
  `config/load_world.*`, `offline_tool/*`, `render/{celestial,draw,planet,sky}.*`, `app/main.cpp`,
  `test/unit/test_load_world.cpp`, `generated/gen_log.jsonl` — LEAVE THEM ALONE; the D.2 commit
  stages ONLY `assets/bf109/*.glb` + `generated/bf109/build_all.py` + docs.
- **D.1b DONE** (`3213d3681`): `test/unit/test_asset_validator.cpp` parses the real GLBs via cgltf
  (raylib's vendored copy; CMake wires the include dir + `SEADS_ASSET_DIR`; seads_tests links no
  raylib so `CGLTF_IMPLEMENTATION` is the sole copy). Five real-vertex legs FIRING: one-mesh/identity
  (P1-1), AABB-vs-box_dims (unit slip), hinge-origin-on-line, chirality true-mirror (P0-1,
  nearest-neighbour), outward winding (signed volume). The chirality leg caught + fixed a real
  aileron/flap mirror bug. box_dims reconciled in `rig.cpp` (same commit). Gate green 343 cases.
- **D.3 + D.3b(core) DONE (2026-07-10 session, this commit) — the REAL Bf 109 now flies in-app.**
  `render/draw.cpp` `ensure_fleet` LoadModel's every `assets/bf109/<key>.glb` (export_yup=False, one
  mesh/GLB, identity node transform → `model.meshes[0]` is already SEADS-frame; Models kept alive,
  never unloaded — Fable lifetime rule; per-node GenMeshCube fallback on a missing/empty GLB). Material
  routing by `NodeSpec::material` via `fleet_material()` (ONE switch): Mirror = the mirror shader;
  Matte = default-shader neutral (pilot + tyres); Glass = translucent tint, drawn in the per-plane
  translucent pass (`draw_prop`, now prop-disc + canopy) AFTER all opaque bodies. Verified in-engine
  (front/side/nose smoke shots) — reads as a real 109, gear deploys planted & splayed.
  **D.3b RUGGED GEAR (Fable consult 2026-07-10, folded):** splay σ=12° + rake r=5° baked into strut
  `rest_rot` (wide ~2.1 m planted tripod for rough fields/lakes); wheels carry a counter-rotation
  (Rx(−5°)·Rz(±9°)) → ~3° tyre camber not the full 12°; **P0-1 fixed** — RIGHT main/door hinge = −Z
  (mirror retract; a shared +Z swept the right leg across the belly). Doors kept fuselage-parented
  Driven::Gear but given the SAME splay/rake + mirrored hinge as their strut so they track it WITHOUT
  the risky enum reorder. Test pins: `test_rig` "main gear splays outboard and retracts mirror-correct"
  (splay + P0-1 mutation guard), gear-unfold test now measures swing RELATIVE to the (splayed) rest;
  `test_asset_validator` pins both hinge signs. Gate 351/351, zero goldens moved.
  - **DEBUG HOOKS added (smoke-only, env-gated, SEADS_OBLIQUE-style — keepers for asset work):**
    `SEADS_SMOKE_GEAR=1` forces gear down in a smoke shot; `SEADS_RIGCAM=1` + `SEADS_RIG_AZ/EL/DIST`
    orbit the PLANE (0°=behind, 180°=nose-on) to frame the gear/canopy/underside.
  - **DEFERRED (next rig-D session, all noted by Fable, none blocking Chad's fly):** the true
    strut-fairing enum reorder (door node index after strut — renumbers 20–25, grep all node-index
    literals + goldens first); matte/glass are UNLIT (default shader) so the pilot/tyres won't track
    the Stage-3 moving sun — a 5-line lambert matte FS + adding the new shaders to the rig-A.3 uniform
    allowlist closes it; per-plane back-to-front sort of the translucent pass (canopies of far planes);
    per-node wheel/tailwheel scale (1.10/1.15 — needs `scl` plumbed from spec, leaf+uniform so safe);
    crisp 3-blade mesh below ~15% throttle (hysteretic) instead of the always-disc. `manifest.toml`
    NOT written — `rig.cpp aircraft_node_specs()` is the SINGLE source (mesh_key + material + hinge +
    rest_pos); a duplicate TOML would fork (repo H1 rule).
  - **NEXT = Chad flies the 109** (3rd person, gear up + `SEADS_SMOKE_GEAR` down); then the deferred
    polish list + the D.4 full-gate/red-team pass, and rig-B's PENDING surface-direction fly-verdict.
- **D.3 ROUND 2 — control-surface INSET + wingtip WELD DONE (2026-07-10, Chad's verdict: gear perfect;
  surfaces must be closely-attached, hinged, INSET into the wing/stab/fin, elegant; wingtips welded).**
  Fable consult 2026-07-10 (2nd) drove it. Key insight: the wing loft is LINEAR in span-fraction s, so
  the TE + hinge lines are exactly STRAIGHT — and an inset surface would be BURIED in the wing's own
  aft 28% unless the wing is NOTCHED. So `build_all.py` was reworked:
  - `build_wing` now cuts BAYS: ribs truncate at chord-frac `F_WALL=0.66` over the flap/aileron spans
    (doubled ribs at each bay boundary → clean vertical side walls; the aft points collapse onto the
    wall and remove_doubles welds them). Single-source wing geometry helpers (`wing_chord/le_z/yc/
    thick/x`, `airfoil_h`) so surfaces + wingtip derive from the SAME loft.
  - New shared `build_ctrl_surface` (arc-nose loft): a circular LE concentric with the hinge → the slot
    gap to the bay wall is CONSTANT at every deflection (real set-back hinge; no poke-through). Used for
    aileron/flap/elevator/rudder. Dihedral+sweep baked into `rest_rot` (`surfL=Rz(−2.395°)Ry(2.239°)`,
    `surfR` mirror) so the canonical +X hinge sits on the swept wing TE — same trick as the gear splay.
  - `build_wingtip` re-authored: inboard rib = EXACT copy of the wing s=1 rib → zero-gap butt weld;
    rounds off elliptically. Empennage TEs straightened to z=0.675 so elevator/rudder meet a straight
    edge (constant 15/20 mm slot). box_dims reconciled to the re-exported AABBs (measured, within the
    `max(0.03, 0.06·box)` validator tol). rest_rot transcribed to a preview `REST_ROT` dict (export
    still zeroes rotation → GLB stays the FLAT local mesh).
  - **Tests:** hinge-origin `lo.z` threshold relaxed −0.06→−0.12 (the arc nose bulges ≤r ahead of the
    set-back hinge — still far from a centroid); the +pitch/+roll legs now measure surface pose RELATIVE
    to rest (the tilt breaks exact absolute antisymmetry); NEW `rig-D: wing surfaces carry the mirrored
    dihedral tilt` tripwire (guards against a future wing reloft silently regressing to flat plates).
    Gate 352/352, zero goldens moved. Verified in-engine (`bf109_preview/cs_after_*.png`): inset flush
    surfaces + welded tips. **DEFERRED still:** lambert matte/glass; the true strut-fairing enum reorder;
    per-plane translucent sort. NEXT = Chad flies it (watch the aileron/elevator/rudder hinge in a roll/
    pitch/yaw, and the wing-root/tip seams).
- **CONCURRENCY WARNING (2026-07-09):** a PARALLEL celestial agent was committing to
  `sandbox/world-sudbury` with `git add -A` and editing `render/` (aurora/moon/sky) — it swept
  staged rig-D files into its commit once before I recovered + re-committed atomically. Before D.3
  (which edits `render/draw.cpp`), confirm no other agent is active on this branch, or work in an
  isolated worktree; ALWAYS stage rig-D files explicitly + commit atomically.
- Rebuild/re-view the assembly any time: launch Blender w/ `mcp_startup.py`, then
  `bmcp.py exec generated/bf109/build_all.py` + the `/tmp/set_view.py` side view (quat
  (0.7071,0,-0.7071,0), ORTHO) → `bmcp.py shot`. Re-export via the `/tmp/export_all.py` pattern.
