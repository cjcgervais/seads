# NEXT-SESSION BRIEF — build the `stereoscope-sudbury` skill

**You are the next agent. Read this first, then `docs/stereoscope_sudbury_plan.md` (the plan), then
`docs/world_stereoscope_research.md` + `docs/stereoscope_fable_staged_plan.md` (the evidence).**

## Your mission (Chad's ruling)
Build **one skill, dedicated to the STEREOSCOPE SUDBURY plan and that plan ALONE** — an executable,
Fable-vetted encoding of the S0→S6 world buildout, so every future session invokes it to drive the work with
the discipline, rulings, gates, and traps baked in. It is a *specialization* of the existing `/orchestrate`
tri-model skill (`.claude/skills/orchestrate/SKILL.md`) narrowed to this one plan.

**Do NOT execute S0–S6 yet.** This next session BUILDS THE SKILL. Building the world starts after the skill
exists and is Fable-approved.

## The process Chad mandated (follow exactly)
1. **ULTRATHINK the skill design** — how should a skill for a 7-stage, multi-model, gated buildout be shaped?
   What does it encode vs defer? How does it enforce the per-stage gate + Fable check-ins + Chad-fly gates?
2. **Fable-5 BEFORE consult (fresh agent):** hand a fresh Fable-5 agent the plan + this brief and have it
   help *design* the skill — structure, what to encode, failure modes, how to make the gates un-skippable.
3. **Opus 4.8 codes the skill** (`.claude/skills/stereoscope-sudbury/SKILL.md` + any helper scaffolding).
4. **Fable-5 checks in at EVERY stage** of building the skill, and the finished skill is **double-checked by
   a Fable-5 AFTER red-team.** Approve a Fable-vetted skill; don't fix later.

## What the skill must encode (the content — all in the plan doc)
- **The 4 rulings (with the math, non-negotiable):** projection = azimuthal-equidistant at TRUE 1:1
  (`GROUND_R=R=15000`) with a procedural wilderness cap (kills the antipodal-pinch striation); **center =
  Sudbury–Chelmsford midpoint (~46.556, −81.11)**; depth = a real rlgl depth-texture FBO + FXAA; resolution =
  keep 8192 (detail moves to normal map + geometry, not texels).
- **The staged S0→S6 sequence** (beauty-per-effort × dependency), each with its goal, the Opus/Gemini/Blender/
  Fable roles, the P0/P1 traps, and a **Chad-fly gate flown OVER CHELMSFORD**:
  S0 projection re-bake + wilderness cap · S1 **lock the B&W stereoscope post stack + depth FBO** · S2 boreal
  trees (cross-quads, instanced) · S3 linework ribbons (black roads + light-green trails) · S4 building massing
  (OSM extrusion) · S5 hero silhouettes (headframe / parish church / smelter via Blender) · S6 polish (the
  depth trio, period buildings).
- **The /orchestrate gate stack, per stage:** asset validator → build → GLSL 330 → seam grep → `--smoke` +
  the `instances>1` assert → flight `ctest` LAST (**zero moved goldens**) → Fable red-team → Chad flies.
- **The 3 top traps** it must actively prevent: (1) baking any stored sphere-direction before S0 locks the
  projection; (2) post-chain colorspace / double-tone (desaturating the chroma planes or stacking `[tone]`+post);
  (3) green-gate blindness on instancing/post (count==1, undefined FS inputs) — smoke + validator are DELIVERABLES.
- **The seam (absolute):** the flight kernel (`sim/`+`control/`) is FROZEN; `render/` reads state, no clock, no
  fixed-axis basis; the runtime is projection-agnostic (projection lives offline).
- **The roles:** Opus = architect/seam/integration; Gemini (`gemini-2.5-pro`, `tools/gemini/seads_gen.py`) =
  verbose assets (GLSL, mesh/tree kits, terrain); Blender (`C:\Program Files\Blender Foundation\Blender 5.1`,
  `tools/blender/addon.py`) = hero building/tree models → glTF, gated by `test_asset_validator.cpp`; Fable =
  isolated math + red-team at every ★.

## Current codebase state (record it; the skill starts from here)
- Branch `sandbox/world-sudbury`, tree **GREEN (346/346)**, committed checkpoint this session.
- **Landed (transitional V2 — S0 supersedes the projection):** B&W newsreel value ladder, roads-as-lines,
  relief 350 + mesh-smooth (blur 100), HRDEM/CDEM **datum-matched + feathered** blend, cubemap 4096, the
  persistent **`SEADS_OBLIQUE` debug terrain cam** (`seads.exe --smoke`, `SEADS_OBL_UP`/`SEADS_OBL_BACK`).
- **Still the OLD enlarged projection** (`GROUND_R=CAPTURE/π`, ENLARGE 1.68×) — **S0's first job is to change
  it to true 1:1 + wilderness cap.** Assets are the transitional enlarged-B&W bake.
- **Raw DEM cached** (`offline_tool/source/dem_hr_4m.tif`, `dem_cd_4m.tif`) → S0 re-bake skips the slow HRDEM
  read (seconds, not 20 min). DSM COG + OSM snowmobile trails confirmed reachable.
- **Built vs scaffolded (from the capability map):** BUILT = terrain mesh + HeightField sampler, B&W albedo
  cubemap + mirror lakes, celestial/weather/scatter shaders, Blender export + headless glTF validator, the
  GIS bake. SCAFFOLDED (not coded) = instanced props (`render/props.*`, `world/props.*`, `[scatter]` stub), the
  full-screen post pass (no RenderTexture yet; `[tone]` config LOADS but is UNUSED). HARD = depth-texture (needs
  the rlgl FBO), MSAA-vs-RenderTexture (→ FXAA).

## Pointers
- `docs/stereoscope_sudbury_plan.md` — THE plan (rulings + S0→S6). The skill executes this.
- `docs/world_stereoscope_research.md` — the cited 6-section technique report (look/trees/buildings/linework/
  staging/references; INSIDE + Obra Dinn + 1940s Sudbury archives).
- `docs/stereoscope_fable_staged_plan.md` — Fable's staged consult with the projection/depth/resolution math.
- `.claude/skills/orchestrate/SKILL.md` — the base tri-model pipeline this skill specializes.
- `CLAUDE.md ## Threads` — the live thread pointer (updated to this).
