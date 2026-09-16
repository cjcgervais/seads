# /orchestrate — module catalog

The registry the `/orchestrate [module]` skill dispatches on. Each module runs the tri-model
pipeline (blueprint → Gemini asset → Fable math → integrate → gate stack → ★ red-team → Chad
flies). See `.claude/skills/orchestrate/SKILL.md` for the pipeline + guardrails.

Two tracks: **A. Environment** (drives `docs/little_planet_plan.md`, the source of truth —
this catalog is just the engine index) and **B. Fleet Rig** (`docs/fleet_rig_plan.md`).

Legend for the model columns: **G** = Gemini asset factory (`tools/gemini/`), **F** = Fable
math sniper (fresh context), **O** = Opus in-repo (contract + integration + gates). Every
module is Opus-integrated; G/F are the offloaded pieces.

## Track A — Environment (little_planet Stages 3–8)

| Module | Stage | Gemini (asset) | Fable (math) | Notes |
|---|---|---|---|---|
| `scatter` | 3 | haze-gated Rayleigh/Mie scatter GLSL for `kSkyGLSL` (recipe `scatter_glsl`) | dusk-blend curve; weather as a pure `t_cel` function | sun disc + moving sun + terminator land here too |
| `stars` | 4 | star-quad shader + authored RA/Dec catalog table (`.toml`, not `.csv`) | RA/Dec→world transform; Polaris==spin_axis pin | constellations: Dippers/Orion/Polaris + southern kite |
| `moon` | 5 | moon disc + phase shader | second directional light; phase = `dot(moonDir,sunDir)` | + water branch reflecting `kSkyGLSL` |
| `seasons` | 6 | snow/ice shader fn; **R8 height cubemap + RGBA8 Sudbury landmask bake** (biggest terrain-art job) | circular season-phase drive (no year-wrap sawtooth) | per-season exposure; lake-ice lerp |
| `live-year` | 7 | — | `lake_freeze_lag_frac` lag math | unlock `season_lock`; `epoch_mode=persist` |
| `polish` | 8 | grayscale aurora fBm shader; lightning fractal-tree; north-pole cairn mesh | aurora distortion math; lightning midpoint-displacement | each its own knob-flight |

**Track-A gate specifics** (from little_planet_plan): the target-visibility probe
(`render/probe.*`) on the stage's season×time×geometry cells + `ctest` LAST + ★ red-team at
stage boundaries. A moved probe baseline HALTS the loop.

## Track B — Fleet Rig (render-only articulated aircraft)

Full design: `docs/fleet_rig_plan.md`. Run in sub-modules:

| Sub-module | Gemini (asset) | Fable (math) | Opus (integrate) |
|---|---|---|---|
| `rig-A` | low-poly block mesh vertex tables / `bpy` (fuselage/wings/ailerons/rudder/elevator/prop/gear); mirror shader (recipe `mirror_glsl`) | flat topo-sorted hierarchy eval | **asset-validator ctest FIRST**; `SceneNode`/`Rig` in `render/`; glm→raylib `to_ray()` bridge (+ unit test); hoist planet cubemap read-only; replace the `DrawCube` stack |
| `rig-B` | — | deflection sign mapping; gear-unfold slerp; prop phase accumulator | drive surfaces from last-applied `sim::Inputs` (read-only, Fable-revised — NOT rate); `throttle`-scaled prop blur disc; const `draw_state` + differential firewall leg |
| `rig-C` (deferred) | debris meshes | `ApplyCollisionDamage` impulse propagation | cosmetic death-anim on crash→respawn only |

**Track-B gate specifics:** asset-validator ctest FIRST (units/forward=−Z/hinge-origin/winding/
uniform-scale/shader-uniform allowlist — Fable's #1 fix, see `fleet_rig_plan.md`) + seam grep (no
clock in `render/`, no `sim/`/`control/` write) + rig-anim firewall differential leg + `ctest`
green (render-only ⇒ cannot move a controller golden; that byte-identity IS the regression proof) +
Chad flies for feel + ★ red-team of the rig math and the cubemap-share refactor.

## Rollout order (approved plan)
1. Infrastructure (this catalog + the skill + `tools/gemini/`) — done.
2. **`rig-A` → `rig-B`** — prove the pipeline on the self-contained Fleet Rig (no celestial dependency).
3. **`scatter` (Stage 3)** — first environment module through the engine.
