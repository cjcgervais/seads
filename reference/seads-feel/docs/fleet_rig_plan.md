# Fleet Rig — render-only articulated aircraft (sub-plan)

The consult's "Module A: Neon Mirror Fleet." Replace the flat `DrawCube` aircraft
(`render/draw.cpp:73`) with an articulated, mirror-finished rig. **Render-only:** it reads
`sim::SimState` and draws; it NEVER writes `sim/`/`control/` and NEVER reads a clock (RA9 +
the HARD RULE). Run via `/orchestrate rig-A` → `rig-B` → (deferred) `rig-C`.

## Data contract (Opus — `render/rig.h`)
Fable-validated flat, topologically-sorted hierarchy (parent index < own index):
```cpp
struct SceneNode {
    glm::vec3 pos{0.0f};
    glm::quat rot{1,0,0,0};
    glm::vec3 scl{1.0f};
    int       parent = -1;      // index into the flat array, -1 = root
    glm::mat4 world{1.0f};      // cache, recomputed each frame
    uint8_t   flags = 0;        // bit0 attached, bit1 destroyed, bit2 animating
    float     hp   = 1.0f;
};
using Rig = std::vector<SceneNode>;
inline glm::mat4 local(const SceneNode& n) {
    return glm::translate(glm::mat4(1), n.pos) * glm::mat4_cast(n.rot) * glm::scale(glm::mat4(1), n.scl);
}
void updateRig(Rig& r) {                 // one linear pass, cache-friendly
    for (size_t i = 0; i < r.size(); ++i)
        r[i].world = (r[i].parent < 0) ? local(r[i]) : r[r[i].parent].world * local(r[i]);
}
```
Nodes (fixed indices): 0 Fuselage(root), 1 LeftWing, 2 LeftAileron, 3 RightWing,
4 RightAileron, 5 Rudder, 6 Elevator, 7 Propeller, 8 LandingGear. Meshes are metre-scale,
fuselage-local. Config the rest-pose TRS + deflection gains in `config/world.toml` (a new
`[fleet_rig]` block) — no bare numbers.
**Non-uniform `scl` is LEAF-ONLY** (Fable): `world = parent.world · local` shears every rotating
child under a non-uniformly-scaled parent, so a stretched-box wing with an aileron child would skew
the aileron as it deflects. Parents that carry rotating children stay uniform-scale; assert it in
the asset validator (below). Never give a child node a world-magnitude `pos` (precision, Claim 4).

## rig-A — geometry, hierarchy, mirror shader (static surfaces)

### glm → raylib matrix bridge (LOAD-BEARING — a raw memcpy transposes it)
raylib `Matrix` aggregate order is row-major (`m0,m4,m8,m12` = row 0); `glm::mat4` memory is
column-major. Use a transpose-correct helper in `render/draw.cpp` (anon namespace):
```cpp
Matrix to_ray(const glm::mat4& g) {
    return Matrix{ g[0][0],g[1][0],g[2][0],g[3][0],
                   g[0][1],g[1][1],g[2][1],g[3][1],
                   g[0][2],g[1][2],g[2][2],g[3][2],
                   g[0][3],g[1][3],g[2][3],g[3][3] };
}
```
Draw each node with `DrawMesh(node.mesh, aircraft_mat, to_ray(node.world))` — the retained-mode
idiom copied from `render/planet.cpp:257` (NOT the immediate `rlMultMatrixf` at `draw.cpp:81`).
`DrawMesh` composes `transform` with the rlgl stack, so bake everything into `node.world` and
leave the stack identity.

### Eye-relative rebasing (seam — never build a node from raw ~15 km coords)
Root world = `translate(rel(state.position, eye)) * mat4_cast(quat(orientation)) * scale(s)`,
where `rel()` is the existing double-subtract→float helper (`render/draw.cpp:22`). Children are
metre-scale local, so float TRS composition is precision-safe. This preserves the exact seam the
`DrawCube` path uses today.

### Mirror shader + per-plane color (Gemini `mirror_glsl` + Opus integrate)
Copy the planet shader lifecycle (`render/planet.cpp:200-224`): `LoadShaderFromMemory`,
`GetShaderLocation`, wire the cubemap sampler by hand
(`shader.locs[SHADER_LOC_MAP_CUBEMAP]` + `mat.maps[MATERIAL_MAP_CUBEMAP].texture = env`).
- **Per-plane color:** `SetShaderValue(shader, loc_color, &rgb, VEC3)` set per plane before its
  `DrawMesh` — player = saturated hero color, bandits = crimson (matching today's livery split).
- **Env cubemap:** reuse the grayscale planet albedo cubemap. **One structural refactor:** it is
  today a `static Planet` local inside `draw_planet` (`render/draw.cpp:46`) — hoist the
  `TextureCubemap` to a scope both `draw_planet` and the aircraft draw can see (or pass it in).
  The rig samples it **read-only**.
- Reflection is simplified by eye-relative space (camera at origin): `viewDir = normalize(fragRel)`,
  `reflect(viewDir, N)` samples the env in world directions (translation-only rebasing preserves
  directions — Fable-confirmed) — mirror the planet's `fragRel` convention.
- **Normal handling (Fable Claim 2 P1):** `N` must be a UNIT world normal — transform by
  `mat3(transpose(inverse(model)))` (or keep all mirror-node scale UNIFORM and use `mat3(model)`)
  and **renormalize in the fragment shader** (interpolated normals shrink; a non-unit N makes
  `reflect` wrong, not just dim). Pin the cubemap face orientation with a labeled test cubemap
  (+X/−Z known colors) — a generated cubemap with a different up/forward looks plausibly wrong.

## rig-B — state-driven animation (read-only)

**Deflection source — commanded `Input`, NOT body rate (revised per Fable before-consult).**
In this plant `Input` literally IS the normalized surface deflection (`τ = c·q·δ_max_eff·Input`) —
the true, causally-correct surface position. Body rate only matches in trim and reads mushy/laggy
in transients (the nose visibly moves *before* the surface deflects); `angular_accel` is a noisy
differentiated worst-of-both — rejected. So the **PRIMARY** path threads the last-applied
`sim::Inputs` as ONE read-only field app→render (`FrameResult` `app/instructor_tick.h:423` →
`FrameInfo` `render/draw.h` → set in `main.cpp` after `step_frame`; `FrameResult` drops inputs
today at `:429`, so this is a deliberate, RA9-safe read-only add). Body rate is the FALLBACK if the
Inputs field is ever unavailable.

- elevator ∝ elevator Input → node 6 `rot`; **sign flip** (Fable: naive `∝` is inverted for elevator)
- rudder ∝ rudder Input → node 5; **sign flip** (naive `∝` inverted for rudder)
- ailerons ∝ aileron Input, antisymmetric L/R → nodes 2, 4
- *signs are verified PER NODE against the authored mesh hinge axis (a flipped hinge re-flips them);
  Fable's rate-derived table flagged elevator+rudder as the two needing a flip under the natural
  +X/+Y hinge convention.*
- gear = `f(altitude(position, params))` — always retracted in flight (no landing in v1) → node 8
- **prop:** `throttle`-scaled translucent **blur disc** above a threshold rpm (time-free); optional
  solid blades below threshold crossfading in. If a solid spinning blade is ever wanted, its phase
  is an **app-owned accumulator** `phase += rpm(throttle)·sim_dt` wrapped mod 2π (NOT `rpm·anim_time`
  — that spins backwards on any throttle chop and dies to float drift over a session). Render reads
  the wrapped phase; never a clock. Gains all in `[fleet_rig]`.

**Firewall (executable, per Fable Claim 5):** pass `draw_state` by `const&`/value (compiler-enforced
no-write); add a **differential leg** — same seed, N ticks with the rig-anim update called vs. not,
REQUIRE sim/control state bit-identical at max_digits10 (not a tolerance band) — in the
`app::tick`/ClosedLoop mirror (no ctest runs `seads.exe`, so the rig-anim update must be a callable
unit). Reset all rig-anim/interpolation state on respawn/GROUNDED (the `CameraOrbit`-residual class).

## rig-D — real Bf 109 F-4 mesh (Blender-MCP + Gemini 3.1 Pro) — IN PROGRESS
Replaces the 9 `GenMeshCube` placeholders with a real, HIGH-DETAIL Bf 109 F-4 (Chad's fidelity +
granularity override, 2026-07-09), authored in Blender, split into ~26 SEPARABLE damage-component
nodes (makes parts separable; damage MECHANICS stay deferred = rig-C). Body keeps the mirror finish
(monochrome full-saturation); NEW material classes = glass canopy + matte pilot. 3rd-person only.
Full plan: `~/.claude/plans/shimmying-discovering-manatee.md`. Five sub-phases D.0–D.4; Fable
before-consult folded (1 P0 chirality + 4 P1 — see the plan). Tri-model: Opus architect / Gemini
`gemini-3.1-pro-preview` bpy + `nano-banana-pro-preview` textures / Fable math sniper.

- **D.0 tooling — DONE + verified (2026-07-09).** `tools/gemini/seads_gen.py` → model
  `gemini-3.1-pro-preview` (gemini-3.5-pro does NOT exist) + Nano Banana Pro image output (`--image`),
  retry-hardened; recipe `recipes/bf109_bpy.txt`. Blender MCP: `uv`+`blender-mcp 1.6.4` installed,
  addon vendored (`tools/blender/addon.py`, registers in Blender 5.1), telemetry DISABLED in
  `.mcp.json`, `mcp_startup.py` (GUI launch) + `bmcp.py` (direct-socket client — drives Blender from
  the shell, NO Claude Code reconnect needed). Verified: get_scene_info / execute_code /
  get_viewport_screenshot all green. Socket needs a GUI Blender (addon refuses `-b` by design).
- **D.1 node contract + validator — NEXT.** `render/rig.h`/`rig.cpp`: NodeIndex 9→~26 (rest TRS at
  real Bf 109 F-4 proportions; canonical hinge axes via `rest_rot`-absorbs-sweep per Fable P1-2;
  material class + mesh key + hinge chord-side per node); extend `Driven`/`apply_deflection`.
  `config/world.toml [fleet_rig]` + `config/load_world.*`: flap gain, glass/matte params, asset root.
  `test/unit/test_asset_validator.cpp`: cgltf headless GLB parse (P1-1), activate real-vertex
  AABB/winding/hinge-on-line legs, add chirality leg (P0-1) + identity-node-transform leg (P1-1).
- **D.2 modeling — DONE (2026-07-09). Method: REFERENCE-DRIVEN (Chad's call).** All 30 nodes built
  reference-driven in `generated/bf109/build_all.py` (one assembly builder: transcribes the rig.cpp
  SPEC, computes world placement, screenshots the whole aircraft vs `ref_side.jpg`), exported to
  `assets/bf109/*.glb` (export_yup=False; verified 1 mesh/node + SEADS frame + L/R mirror +
  hinge-origin-on-line). Fuselage re-exported with a fuller aft cone (the v3 needle tail read as
  detached from the empennage in the full-assembly check). See `docs/rigd_handoff.md` for the D.3
  rig.cpp box_dims/rest_pos reconciliation deltas (incl. the gear TOP-PIVOT + axle-X wheel fixes).
  The established loop (kept for future re-touch / rig-C):
  1. Nano Banana Pro (`--image`) → accurate ortho reference plate (`generated/bf109/ref_side.jpg`
     is a clean, accurate Bf 109 F-4 side profile — spinner/cowl, forward cockpit, rounded fin).
  2. Opus TRACES the reference into a deterministic bpy (station table etc.) — for precision-
     critical shapes author directly rather than roll Gemini; Gemini authors the verbose/low-risk
     parts. Author in LOCAL frame (origin = node rest point; hinge nodes' origin ON the hinge line).
  3. `bmcp.py exec` builds it in a GUI Blender (`mcp_startup.py`), `bmcp.py shot` screenshots a
     clean SEADS side view (custom view quat — Blender's +Z-up ≠ SEADS -Z-fwd) to COMPARE vs the
     reference silhouette; iterate.
  4. Export one GLB per node key, `export_yup=False` (preserves SEADS frame; raylib does no axis
     conversion). Verify GLB bounds (nose on −Z) + exactly one mesh (P1-1).
  All meshes: asymmetric-loft fuselage; superellipse-loft cowl; ogive spinner; disc prop; framed
  canopy; airfoil-loft wings (taper + dihedral + LE sweep); extrude-outline surfaces (fin, rudder,
  stabs, elevators, ailerons, flaps, wingtips, gear doors); tube-built gear struts (top-pivot) +
  axle-X wheels + tail wheel; beveled radiator + exhaust stacks. Still TODO in D.3: `manifest.toml`
  (node→file/hinge/chord/material) + the rig.cpp reconciliation.
- **D.3 ingestion / D.4 gate+red-team+fly** — per the plan. Ingestion: per-node
  `LoadModel` in `ensure_fleet()`; mirror/glass/matte routing; opaque→single translucent pass
  (depth-test on/write off, global back-to-front per surface, prop disc additive — Fable P1-4).

## rig-C — collision death animation (DEFERRED)
Cosmetic tear-off on crash→respawn only (no landing/damage gameplay). Fable's `ApplyCollisionDamage`
input contract (from `gemini_graphics_consult/from_fable.md` item 3) is the starting packet:
per-node AABB/mass/HP + impact impulse → impulse attenuated down the subtree, `spawnDebris(idx,
vel, angVel)`. Debris are render-side particles; the sim still just resets.

## Asset-validator ctest — PREREQUISITE to the first Gemini asset (Fable's #1 fix)
The one failure class that is systematic across the whole Gemini factory, invisible to build +
controller-goldens, and plausible enough to ship: **convention drift in generated data** (a glTF
+Z-forward plane flying tail-first; cm-vs-m units → 100× plane; a part-mesh with origin at its
centroid instead of on its hinge line → aileron sweeps THROUGH the wing with signs perfect). Land a
load-time validator ctest (the loader-tripwire pattern) BEFORE ingesting any generated mesh, and
run it in the gate stack thereafter. It asserts, per committed mesh + `[fleet_rig]` catalog entry:
(a) catalog span/length/height match the mesh AABB within tol (**units**); (b) nose lies on **−Z**
(AABB/marker asymmetry — **forward axis**); (c) signed volume > 0 and Σ n·(v−centroid) > 0
(**winding/outward normals** — the mirror needs them); (d) **every hinge node's origin lies on its
declared hinge line** (catalog declares hinge origin+axis; check the mesh hinge-edge verts within ε);
(e) parent-of-rotating-child nodes are **uniform-scale**; (f) each shader's active-uniform set ⊆ an
**allowlist** (closes a stray `u_time`/clock or state backdoor in one stroke).

## Gate stack (Track B)
**asset validator green** → build → GLSL 330 compiles → **seam grep clean** (no clock in `render/`;
no `sim/`/`control/` write; shader uniforms ⊆ allowlist) → **rig-anim firewall differential leg**
(rig-on vs rig-off ⇒ sim/control bit-identical) → **flight `ctest` green** (render-only ⇒ controller
goldens bit-identical — that IS the regression proof) → Chad flies for feel → **separate-context
Fable ★ red-team** of the hierarchy math + deflection mapping + the cubemap-share refactor. Commit
green before any mutation-revert; `rm` a locked `seads_tests.exe` and confirm the relink first.

## Fable before-consult findings — folded (2026-07-08; packet `pre_spec_audits/fable_orchestrate_before_consult.md`)
Fresh-context Fable-5 red-team of the 7 load-bearing claims (SOUND-WITH-FIXES; no P0). Verdicts +
where each fix landed:
- **C1 to_ray bridge — CORRECT.** Fix (P2): use designated initializers (`.m0=…,.m12=…`) or a
  `static_assert`/unit test (`to_ray(translate(1,2,3))` lands 1,2,3 in `.m12/.m13/.m14`) so a raylib
  field-reorder can't silently transpose. → add `to_ray` unit test in rig-A.
- **C2 eye-relative reflection — CORRECT.** Fixes folded into the mirror-shader section (inverse-
  transpose/uniform-scale normal + renormalize in FS; labeled test cubemap). Parallax on the plane's
  own reflection is accepted cosmetic.
- **C3 deflection — CORRECT-WITH-FIX.** Source changed rate→**Input** (primary); elevator+rudder
  **sign flips**; reject `angular_accel`. Folded into rig-B.
- **C4 hierarchy order/precision — CORRECT.** Angular error ~1.2e-7 rad (sub-pixel even zoomed);
  worst absolute ~8 mm at 60 km, no cancellation. Rule "no child holds world-magnitude pos" folded
  into the data contract.
- **C5 RA9 firewall — CORRECT-WITH-FIX.** Restated (it's downstream-only, NOT stateless — interp +
  phase are legit render-side state). const `draw_state` + differential leg + respawn reset folded
  into rig-B.
- **C6 prop phase — CORRECT-WITH-FIX.** Accumulator not `rpm·t`; blur disc above threshold, solid
  blades below. Folded into rig-B.
- **C7 asset factory — the asset-validator ctest above** (Fable's single highest-priority fix).
- **Unquestioned assumptions surfaced:** leaf-only non-uniform scale (folded into data contract +
  validator); slerp double-cover if orientation is interpolated (`dot<0 ⇒ negate q`); translucent
  blur-disc draw order (after opaque, depth-write off) — both build-time notes for rig-A/B.

## Critical files
- `render/draw.cpp` — `draw_aircraft` rewrite, `rel()`, `to_ray()`, call sites (`:202-204`), cubemap hoist (`:46`)
- `render/planet.cpp` — shader/material/cubemap + `DrawMesh(mesh,mat,Matrix)` idiom to copy (`:200-260`)
- `render/rig.h` (new) — `SceneNode`/`Rig`/`updateRig`, node index enum, aircraft rig builder
- `render/draw.h` — `FrameInfo` (only if the fallback Inputs field is threaded)
- `config/world.toml` + `config/load_world.*` — new `[fleet_rig]` block (rest-pose TRS + gains)
- `sim/state.h` — `angular_vel`/`throttle` (the already-available deflection source; READ only)
