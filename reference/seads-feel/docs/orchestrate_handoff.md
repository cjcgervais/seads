# Orchestrator handoff — /orchestrate + Fleet Rig (FRESH AGENT START HERE)

**Branch:** `sandbox/world-sudbury`. **Last commit:** `9096f395` (Stage-3 day/night tuning — `git log`
for the live tip). **Gate:** build clean; flight **ctest 315/315 green** (Stage-3a/3b added 1
load_world leg over the 314 base; zero moved goldens — the kernel firewall held through all of Stage 3).
**You are the orchestrator** — drive render-layer modules through the tri-model pipeline. NEVER touch
`sim/`/`control/` (frozen kernel).

## LATEST SESSION (2026-07-09 #8 — scatter BLAZE retune + force-haze debug toggle)
- **Chad flew #7 and saw NOTHING** (twice) — root cause: two closed gates. (1) scatter is × the
  weather haze and weather is CLEAR ~70%, and the `]`/`[` scrub wheels the sun AND weather off the
  same `t_cel`, so a hazy front rarely coincides with a low sun (the halo was near-untestable in the
  wild). (2) the Mie window peaked at sun elev ~6–9° and was 0 at the horizon, so watching the sun
  SET showed no halo.
- **FIX 1 — DEBUG force-haze toggle `\`** (`89191a43f`, app-only): toggles full overcast so the
  scatter geometry can be flown independent of the weather roll. Seam-safe (render-only, interactive-
  only, gated `smoke_frames==0`), like the `]`/`[` scrub. Not the shipped look.
- **FIX 2 — the Mie "BLAZE RED" retune** (Chad's ask, `render/scatter_glsl.cpp`): the Mie is now a
  RED/orange SUNSET BLAZE — (a) the low-sun gate rises SHARP just above the horizon
  (`smoothstep(0,0.03,sunElevCos)`) so it blazes as the sun SETS and holds through low sun (fades by
  ~45°), still EXACTLY 0 below the horizon (night safety intact); (b) a BROAD horizon-reddening term
  (`rimV = 1-smoothstep(0,0.35,|vUp|)`, peaks at the rim, 0 at zenith AND nadir) so the low SKY blazes,
  not just a dot on the sun; (c) magnitude up (`u_mie * mieW * (hg + rimV*0.8)`, dropped Fable's ×0.5
  bound — Chad wants drama). Verified via a throwaway forced sunset smoke (reverted): a white-hot sun
  in a huge red-orange sky, plane silhouetted — it BLAZES. gate 327/327, zero moved goldens.
- **NOTE (honest):** this deliberately OVERSHOOTS Fable's "subtle/bounded" calibration — the forced
  full-overcast + low-sun case fills most of the frame orange. That is the maximal case (scales down
  with the real haze amount + sun height). If Chad finds it TOO much, the dials are `scatter_strength`
  (overall), `mie_tint` (hue), `mie_g` (halo tightness), or the `rimV*0.8` breadth in the shader.
- **OWED: Chad re-flies** — hold `\` (force overcast), scrub `]`/`[` down to a low sun; the sky should
  blaze red/orange. Then judge magnitude/hue and we dial. The Rayleigh (blue, high sun, horizon-
  weighted) is unchanged from #7.

## LATEST SESSION (2026-07-08 #7 — the haze-gated SCATTER landed; the FIRST real Gemini call)
- **LANDED (gate 327/327, zero moved goldens, Fable before+after red-teamed SOUND-WITH-FIXES):** env
  Stage 3 **atmospheric scatter** — the FIRST COLOR in the sky, and the FIRST real Gemini asset call.
  A `vec3 scatter(viewDir, sunDir, up, sunElevCos, hazeAmount)` (Gemini `gemini-2.5-pro` draft →
  `generated/scatter.glsl`, provenance in `generated/gen_log.jsonl`): Rayleigh **blue** (sun high,
  HORIZON-weighted per space-first) + a Mie **orange** forward-scatter halo ON the sun (Henyey-
  Greenstein, sun low) — both zero-gated below the horizon so night-under-haze reads grey. The WHOLE
  result × `uHazeDensity` (the weather gate) so clear air (~70%) stays EXACTLY dark-starry.
- **Where it lives (the single-source that kills the sky↔limb fork):** the reviewed body is in the
  PURE core `render/scatter_glsl.{h,cpp}` (`kScatterGLSL`, raylib-free, like `rig.cpp`'s mirror
  source) so the headless asset validator gates it. `sky.cpp`/`planet.cpp` concatenate it into BOTH
  FS's AFTER `kSkyUniformsGLSL`; a new shared `sky_color()` = `vec3(sky_luminance) + scatter(...)` is
  called by the sky FS main AND the planet `sky_aerial` — identical inputs at the limb, no fork.
- **Config (`[atmosphere]`):** `scatter_strength=1.0` (gain × haze; loader cap [0,4] — the AFTER P1
  wash bound), `mie_g=0.80` (HG asymmetry, cap [0,0.95)), `rayleigh_tint=[0.35,0.55,1.0]`,
  `mie_tint=[1.0,0.45,0.20]` (tint caps [0,2]). 4 uniforms (`u_rayleigh/u_mie/u_scatterStrength/
  u_mieG`) added to `set_atmosphere_uniforms` (the ONE setter both shaders call), the sky+planet loc
  sets, and the asset-validator SKY leg. `test_load_world` legs: values + mie_g/scatter_strength
  over-cap rejections.
- **★ Fable BEFORE-consult** (fresh context, pre-code): signs/domain SOUND; required fixes folded —
  Mie zero-gate at `sunElevCos=0` (P0), Rayleigh ×0.25 + Mie ×0.5 keeping 1/4π (P1), pin g=0.8.
- **Opus review of the Gemini draft caught 1 seam bug:** the draft's view term used a FIXED axis
  `vec3(0,1,0)` (banned on the sphere) — corrected to the local `up`, added as a `scatter()` param.
- **★ Fable AFTER red-team** (fresh context, cross-model): SOUND-WITH-FIXES, 0 P0 / 2 P1, both FOLDED:
  (P1-1) the draft's symmetric `view·up` Rayleigh blue-washed the zenith/nadir → re-weighted to the
  HORIZON (space-first: color at the rim, never a blue dome, dark zenith; also kills the 2× nadir
  ground-aerial over-tint); (P1-2) unbounded `scatter_strength` could wash the whole overcast sky →
  loader cap [0,4] + a rejection test. P2s recorded (g softened from pin to a clamped knob; blue-clip
  hue-shift near saturation; planet `sky_aerial` returns unclamped — a footgun only under a future HDR
  target).
- **Smoke-confirmed (throwaway forced-haze, reverted):** clear = unchanged dark-starry (no regression,
  scatter≡0); forced haze = subtle blue Rayleigh at the rim + dark zenith + the orange sun-halo, and
  the plane STILL silhouettes (the ×0.25 calibration held). Both shaders compile under real NVIDIA
  GL 3.30.
- **OWED: Chad flies it** — scrub `]`/`[` into a haze front at a LOW sun; the orange halo should hug
  the sun (report if it's on the anti-sun side — the sign trap), blue at the rim when the sun is high,
  grey (no color) at night. Fly-dials: `scatter_strength` (punch), `mie_g` (halo tightness), the two
  tints (hue), the Rayleigh horizon falloff width (the `0.5` in `scatter_glsl.cpp`).
- **NEXT — Stage 3 remainder:** the **dusk color BLEND** of the base band + the **open sun-bloom
  decision** (Chad offered; additive glow past the LDR disc — DISTINCT from the Mie halo, which is now
  the sanctioned sun-halo). Then Stage 3 closes.

## LATEST SESSION (2026-07-08 #6 — the WEATHER VARIABLE landed; read this first)
- **LANDED (gate 324/324, zero moved goldens, Fable red-teamed SOUND):** the Stage-3 **weather
  variable** — a pure deterministic `render::weather_haze(t_cel) -> haze[0,1]` (new `render/weather.{h,cpp}`
  in `seads_render_core`, mirrors `celestial.*`). Three incommensurate-prime-period sines (1499/547/197 s
  = the fronts) summed + a smoothstep GATE that hard-zeros the low band → **~70/25/5 clear/haze/overcast**
  (Fable before-consult measured 69.3/25.5/5.2 over 2M samples; C1-smooth, max slew 0.016/s). Chad ruled
  the **DYNAMIC-ARENA tempo** (fastest front ~197 s ~3.3 min → a front can roll in mid-fight). Phases
  tuned so **t=0 (spawn) reads CLEAR exactly**.
- **Wiring (no shader edit — value-only):** `app/main.cpp` per frame sets `atm.haze_density =
  weather_haze(t_cel, wparams) * haze_overcast_density`, feeding the SAME `uHazeDensity` uniform the
  Stage-2 sky band + ground aerial already read (both gated together — single-source holds). Rides the
  same TICK-DERIVED `t_cel` as the sun (AT-9-pinned, no clock; the `]`/`[` scrub wheels weather too).
- **Config migration:** the always-on `[atmosphere] haze_density_light=0.03` placeholder (the
  `[[haze-rare-not-envelope]]` whisper) was **RETIRED** → new `[atmosphere] haze_overcast_density=0.90`
  (full-overcast density the weather amount scales) + a new **`[weather]` block** (periods/weights/phases/
  gate; strict loader: periods>0, weights sum to 1, gate_lo<gate_hi, gate_lo∈(-1,1)). **Clear (~70% of the
  time) → uHazeDensity≈0 → the Chad-approved dark-starry space-first sky is now TRULY clear** (the 0.03
  whisper is gone). Smoke-confirmed the clear spawn renders.
- **★ Fable AFTER-red-team DONE (fresh context, cross-model): SOUND — 0 P0, 2 P1 (both TEST blind spots,
  no mechanism change), P2s recorded.** Fixed: **P1-1** the drop-the-fastest-sine mutant passed every
  distribution/spawn/C1 leg (Chad's tempo had no tripwire) → the slew test now asserts a params-derived
  LOWER bound (`max_delta >= 0.55·slew_sup`) so the fast front must contribute; **P1-2** the config/default
  fork + a hard-coded 0.020 C1 bound (AT-15 trap) → the C1 UPPER bound is now params-derived
  (`1.5·2π·Σ(wᵢ/Tᵢ)/(gate_hi−gate_lo)`) and a `test_load_world` leg LOCKS `[weather]` == the
  `WeatherParams{}` header defaults (so the property tests always cover the shipped design). P2s (record):
  the `max(0,·)`-gate mutant is caught only by the distribution light-band (thin margin); stale
  `haze_density_light` key is inert (require() only throws on missing) — acceptable; haze peaks ~0.968
  (never a literal 1.0, by design); no upper `gate_hi` bound / period-vs-sim_dt floor (absurd-config only).
- **OWED: Chad flies it** — scrub `]`/`[` to wheel through clear→light-haze→overcast; tune `gate_lo`/
  `gate_hi` (distribution), the periods (tempo), `haze_overcast_density` (overcast strength). On a retune,
  update BOTH `world.toml [weather]` AND the `render::WeatherParams` header defaults (the lock leg enforces
  it) so the property tests re-verify the new design.
- **NEXT — the haze-gated SCATTER GLSL** (the FIRST real Gemini call, verbose GLSL): Rayleigh blue
  (sun high) + Mie red/orange forward-scatter halo (sun low), the WHOLE contribution multiplied by
  `weather_haze` (this gate) so clear air stays dark-starry. Recipe drafted at
  `tools/gemini/recipes/scatter_glsl.txt`. Then dusk color blend. Also still OPEN: the sun bloom/glow
  decision (Chad offered; LDR disc saturates white). Scatter edits `kSkyGLSL` (Chad-approved base).

## LATEST SESSION (2026-07-08 #5 — Stage-3a tuned + Stage-3b day/night + live tuning; read this first)
- **Stage-3a Chad-flown + tuned (config-only):** sun disc/moving sun approved. Dials: `sun_intensity`
  1.25, `sun_angular_diameter_deg` 0.70 (2x), disc **halo REMOVED** (shader term + uSunGlareRad
  plumbing dropped — the sun halo is now the deferred Stage-3 haze-gated Mie forward-scatter, NOT a
  baked disc glare; `sun_glare_deg` PARKED with a comment). Rim thinner + more transparent:
  `sky_band_top_deg` 4.0→3.0, `sky_day` 0.82→0.60. (`c28c97e2`, `e9b41bd9`.)
- **Stage-3b LANDED (`5fc3d42c`, gate 315/315, zero moved goldens):** the planet FS day/night
  terminator — the bare `0.38 + 0.75*ndl` DIES → `albedo*(uNightFill + uGroundDayGain*ndl)`. Night side
  falls to `night_fill_min`=0.10 (a REAL dark floor, not flat grey — "dive into night to break
  contact"); day = albedo*~1.0 (full B&W detail, no blowout). New `[atmosphere] ground_day_gain=0.90`
  (decoupled from the disc sun_intensity), strict [0,2] loader + test_load_world leg. Smoke-confirmed a
  full-albedo DAY view AND a forced-night view (dark floor, plane silhouettes; debug backlight reverted).
- **OBSERVABILITY + config tuning (Chad "can't tell day from night" → iterated live):** the day/night
  was real but unwatchable at the ruled 1-h day (~static per dogfight) + spawn day-side. Fixes: (1) a
  **DEBUG time-scrub** (`71c77b15`) — hold `]`/`[` to wheel the sun (~4 s/day), seam-safe (adds
  `cel_time_offset` to `t_cel` only, interactive-only so the probe dump stays deterministic); (2) a
  **faster cycle** + a run of Chad fly-dials (`1dacdec5`→`a3be3c06`→`9096f395`), all config-only.
- **CURRENT TUNED DAY/NIGHT KNOBS (config/world.toml — the live values a fresh agent should NOT be
  surprised by):** `day_period_s=300` (5-min day; `year_period_s=3600`, the 12-days/yr integer ratio is
  a loader assert — keep `year = 12*day` on any re-dial), `night_fill_min=0.05` (dark night floor +
  ground ambient), `ground_day_gain=1.60` (bright day → strong day/night contrast; bright terrain blows
  toward white, Chad's brightness-over-detail call; loader cap [0,2]), `sun_angular_diameter_deg=1.40`
  (4x the original disc), `sun_intensity=2.0`, `sky_day=0.60`, `sky_band_top_deg=3.0`, `sky_space=0.02`.
- **OPEN DECISION (Chad offered, not yet answered): a sun BLOOM/glow.** The LDR disc saturates to pure
  white, so `sun_intensity` past ~1 is a no-op on the disc itself (headroom only). If Chad wants the sun
  to read more *radiant*, that's a real shader add (additive glow spilling past the edge) — DISTINCT
  from the fuzzy disc halo he had removed in 3a. Wait for his verdict; don't add it unprompted.
- **NEXT — Stage 3 remainder (unstarted):** the **weather variable** (deterministic clear/haze/overcast
  function of `t_cel`, seam-safe pure math — the natural next single mechanism; drives the haze amount)
  → then the **haze-gated scatter** (Rayleigh/Mie in `kSkyGLSL`, the sun halo/glow returns here,
  weather-gated — the FIRST real Gemini call, verbose GLSL) → dusk color blend. Scatter edits `kSkyGLSL`
  but the space-first base is now Chad-approved (tuned live), so it's unblocked. A weather-variable
  before-consult + a scatter-math Fable round are the natural Fable touchpoints.

## LATEST SESSION (2026-07-08 #4 — Stage-3a: sun disc + MOVING sun; read this first)
- **env Stage 3 STARTED. Chad ruled the sequence: sun disc + moving sun FIRST** (the
  base-independent visual increment) so it does NOT retune the un-flown SPACE-FIRST `kSkyGLSL`
  luminance base; the haze-gated **scatter** (which edits that base) waits for Chad's sky fly.
- **LANDED (`179b0933`, gate 314/314, zero moved goldens):** (a) MOVING sun — `main.cpp` holds
  `cel` past the loader and sets `info.sun_dir = -sun_dir(cel, t_cel)`, `t_cel = tick_count·sim_dt`
  (TICK-derived, AT-9-pinned, no clock in render). (b) SUN DISC — a **sky-FS-local** `sun_disc()`
  (deliberately NOT in shared `kSkyGLSL`, so it never bleeds into the planet aerial; the planet mesh
  occludes a below-horizon disc for free): fwidth 1px core + soft quadratic glare halo to
  `sun_glare_deg`. New uniforms `uSunAngRad/uSunGlareRad/uSunIntensity`; `render::SunParams` mapped
  from existing `[celestial]` (NO new config). (c) EYE-RELATIVE `sunDir` — `draw.cpp` places the
  finite sun at distance + takes the eye→scene dir, fed to BOTH sky pass AND planet aerial so the limb
  can't fork (trap-1).
- **Smoke-confirmed** the disc renders (bright core + soft glare on near-black space; forced into view
  via a throwaway `-cam_forward` swap, reverted). **★ Fable red-team DONE (fresh context, cross-model):
  SOUND — 0 P0 / 0 P1 / 4 P2**, all P2 cosmetic or already-guarded (the `distance_m` guard is the
  existing `make_celestial` `>=30*R` assert). Math + shader + seam certified.
- **OWED: Chad flies it** — turn toward the sun to confirm disc placement/size/glare; watch the sun
  crawl over a few minutes (~0.1°/s). Fly-verdict items the red-team flagged: the terminator crawls
  ~2.2° as you circumnavigate (eye-relative price of limb agreement); a sun ON the limb shows its glare
  halo hard-clipped by the planet silhouette (pass-ordering). Both expected — report if they read wrong.
- **NEXT (Stage 3 remainder, after Chad's sky fly):** the **weather variable** (deterministic
  clear/haze/overcast function of `t_cel`) + the **haze-gated scatter** (Rayleigh/Mie in `kSkyGLSL` —
  the FIRST real Gemini call, verbose GLSL) + planet-FS `sunIntensity`/night-floor (kills the `0.38`
  bare number) + dusk blend. Scatter rides the space-first base, so it wants Chad's sky-look fly first.

## LATEST SESSION (2026-07-08 #3 — space-first sky; read this + the blocks below)
- **RULING (Chad, flown): the sky is SPACE-FIRST** — overhead is near-black SPACE so the celestial layer
  (stars/moon/sun) reads against it; the atmosphere is only a THIN silver rim at the horizon; **a clear
  day shows space overhead.** Chad flew the earlier flat-0.82-silver dome and read it as "haze/grey
  everywhere" — a uniform lit dome is the WRONG model for a little planet in space. LANDED: `kSkyGLSL`
  `sky_luminance` rewritten to `mix(sky_space, band, bandW)`. The rim ANCHORS TO THE PLANET LIMB (not
  level): `bandW = 1 − smoothstep(0, sky_band_top, eView − uHorizonElev)`, `uHorizonElev = −acos(R/|eye|)`
  the horizon dip — a per-frame uniform set in draw_sky + draw_planet_mesh (same eye op, not a fork).
  Chad's first fly of a LEVEL-referenced band read "half the sky" (the limb sinks ~28° below level at
  2 km), so `sky_band_top_deg` is now the rim THICKNESS above the limb (`4.0`, a thin arc); other knob
  `sky_space=0.02`. Tonal ladder extended `space ≤ night ≤ dusk ≤ day` (loader + mutation test). RESOLVES
  red-team F1 (flat day sky) — NOT a Stage-3 deferral. Smoke-confirmed (`--smoke`, 2 km): near-black space
  overhead, plane silhouettes, thin silver arc on the limb. Memory: `[[sky-space-first]]`. **OWED: Chad
  re-flies + dials** (`sky_band_top_deg` thickness / `sky_space` darkness).
- **rig-B surface direction = PASS** (Chad flew: "surface direction looks correct") → **rig-B FULLY CLOSED**,
  no hinge flip. The only Fleet-Rig work left is deferred rig-C (cosmetic crash death-anim).
- **Prior this session (still true):** Stage 2 ★ red-team CERTIFIED (math proven); haze CLEAR-by-default
  (`haze_density_light 0.35→0.03`, `[[haze-rare-not-envelope]]`); the ~0.35 LIGHT-haze magnitude returns
  gated by the **Stage-3 weather variable** (core Stage-3 scope). **Gemini asset factory is LIVE** — key
  at `gemini_api/.env` (gitignored, newer `AQ.` format), `read_key` reads env-var-then-.env, validated
  against `gemini-2.5-pro`; first real call expected in Stage 3 (scatter GLSL).

## 2026-07-08 SESSION — the new kernel MERGED + rig-B landed (read this first)
- **`main` was merged into `sandbox/world-sudbury`** (`41dd2f8b`): the new flight kernel (MB-flaps/gear
  in `sim::Inputs`/`SimState`, MB instructor tuning, MB-7c HUD/vortices/wind/S-reticle) now sits under
  the world-build + Fleet Rig branch. 12-file union resolve, gate green, Fable-red-teamed SOUND. `[sky]`
  was superseded by `[atmosphere]`; `FrameInfo.aim_forward`→`reticle_dir`. (A newer untracked
  `docs/mission_b_instructor_plan.md` was preserved at `/tmp/mission_b_instructor_plan.LOCAL-newer.md` —
  the merge tracked main's older copy; reconcile that doc if you care.)
- **Fleet Rig rig-B is COMPLETE** (`72cadabf` law + `1d1a05e3` wiring + `99d78767` red-team):
  control surfaces (aileron/elevator/rudder) deflect from the commanded `sim::Inputs`, gear unfolds from
  the actual `state.gear`, prop is a throttle-scaled two-pass translucent blur disc — on the player AND
  the drones (both from the same commanded-Input source; `DroneState.last_inputs`). Signs DERIVED from
  the plant (not the rig-A comment); render-only firewall pinned (drone toggle bit-identity + forwarding
  legs). **OWED: Chad flies it** — confirm the felt surface DIRECTION/magnitude, the gear unfold, the
  prop alpha (one-line flip per hinge if backwards; `[fleet_rig]` knobs in `config/world.toml`).
- **Stage 2 ★ Fable AFTER-red-team DONE (2026-07-08):** verdict SOUND-WITH-FIXES — the sky/haze MATH is
  proven correct (frustum-exact under lens-shift+zoom, no horizon fork, seam clean), so **Stage 3's
  foundation is certified.** Two P1s are "mechanism silently nulled" LOOK/TUNING items routed to Chad's
  owed Stage-2 fly (flat monochrome day sky = F1, subsumed by Stage-3 scatter; limb-haze too weak = F2 =
  the `haze_density_light` fly-tune) + a P2 loader guard. Full write-up in `docs/little_planet_plan.md`
  Stage 2 block + `docs/lessons.md`.
- **NEXT (pick one):** **rig-C** (cosmetic crash death-anim, deferred) or **env Stage 3 = `scatter`**
  (`docs/little_planet_plan.md` — the first environment module through the engine). rig-A/rig-B are done.
  NOTE: Stage 3 EDITS `kSkyGLSL` (adds the haze-weighted Rayleigh/Mie scatter) — so it wants Chad's
  Stage-2 sky-look fly FIRST, or the scatter tune rides on an un-approved monochrome base.

## What this is (read these, in order)
1. `.claude/skills/orchestrate/SKILL.md` — invoke it: the `/orchestrate [module]` pipeline
   (Opus architect / Gemini asset factory / Fable math sniper), the gate stack, the guardrails
   ("where the consult is wrong about SEADS"). **Master this first.**
2. `docs/orchestrate_modules.md` — the module registry (env Stages 3–8 + Fleet Rig sub-modules).
3. `docs/fleet_rig_plan.md` — the render-only articulated-aircraft plan (the near-term work).
4. `docs/little_planet_plan.md` — source of truth for the environment track (Stage 3 next there).
5. `pre_spec_audits/fable_orchestrate_before_consult.md` — the Fable before-consult; **its P1s are
   already folded into the docs** (don't redo them). Verdicts summarized at the end of `fleet_rig_plan.md`.

## The tri-model workflow (token economy — the whole point)
- **You (Opus):** C++ data contracts, config blocks, integration into `render/`, the seam, gates.
  Do NOT hand-type large vertex arrays / GLSL boilerplate — that's Gemini's volume.
- **Gemini (`tools/gemini/seads_gen.py`):** GLSL bodies, mesh vertex tables / `bpy`, catalogs,
  terrain art. Output is a DRAFT you review + single-source + gate. It never touches the seam.
- **Fable (fresh isolated context, e.g. an Agent with `model: fable`):** stateless math AND the
  ★ separate-context red-team. Author ≠ red-teamer.

## State of the Fleet Rig — rig-A COMPLETE (A.1/A.2/A.3 + after-red-team, `b379fd89`, gate 270/270)
- **rig-A.1 DONE**: `render/rig.{h,cpp}` (PURE, `seads_render_core`) — `SceneNode`/`Rig`/`updateRig`,
  the transpose-correct `to_ray_fields` glm→raylib bridge, the 9-node rest-pose catalog. `test_rig.cpp`.
- **rig-A.2 DONE**: procedural `GenMeshCube(box_dims)` per node + the mirror VS/FS
  (`render::mirror_vs_source`/`mirror_fs_source` in `rig.cpp` — kept in the PURE core so the validator
  sees the source) + `draw.cpp` integration (`to_ray` Matrix via designated init, lazy `FleetRig`,
  `draw_aircraft` = `updateRig` + `DrawMesh(mesh, mat, to_ray(body_to_world·node.world))`, per-plane
  `SetShaderValue` color, gear retracted, DrawCube fallback). The planet `TextureCubemap` is hoisted to
  a lazy file-scope singleton (`ensure_planet`) so the rig samples the albedo read-only. `[fleet_rig]`
  config block (`reflectivity`/`fresnel_power`/`player_color`/`bandit_color`) + strict loader +
  `test_load_world` legs. Rest-pose geometry stayed STRUCTURAL in `rig.cpp` (not config).
- **rig-A.3 DONE**: `test/unit/test_asset_validator.cpp` — headless convention validator: shader
  declared-uniforms ⊆ allowlist (VS + FS, statement-based parser, rejects `#` directives, closes the
  clock backdoor), box dims positive + metre-scale two-sided, forward=−Z (catalog + assembled AABB),
  canonical unit hinge axes (per-node), uniform-scale parents. Two legs mutation-verified.
- **★ Fable AFTER-red-team DONE** (folded into `b379fd89`): P0 — raylib's default VS lacks
  `fragPosition`/`fragNormal`, so the mirror FS ran on undefined inputs (flat blue, no reflection) and
  the gate couldn't see it (no ctest runs `seads.exe`); fixed by authoring the explicit mirror VS.
  P1s — validator `;`-glued/comma smuggle escape (parser hardened), one-sided units check. Chad flew
  it on a real NVIDIA GL33 box; the mirror shades per-surface.
- **NEXT (pick one):**
- **rig-B** — state-driven animation: deflection from the **commanded `sim::Inputs`**
  (Fable-revised — NOT body rate), threaded read-only app→render (one field: `FrameResult`
  `app/instructor_tick.h:423` → `FrameInfo` `render/draw.h` → set in `main.cpp` after `step_frame`).
  Elevator + rudder need **sign flips** (verify per mesh hinge). Prop = `throttle` blur disc; if a
  solid blade, phase is an **app-owned accumulator** `phase += rpm·sim_dt` (NOT `rpm·t`). Const
  `draw_state` + a **differential firewall leg** (rig-anim on vs off ⇒ sim/control bit-identical) in
  the `app::tick`/ClosedLoop mirror. Reset anim state on respawn/GROUNDED.
- **rig-C** (deferred) — cosmetic crash death-anim only.

## Owed
- **rig-A is fully wrapped** — Chad flew it, the after-red-team is folded, the lessons are in CLAUDE.md.
  Nothing outstanding on rig-A except OPTIONAL feel tuning (`config/world.toml [fleet_rig]` mirror
  knobs) whenever Chad wants to run the flight-log loop on it.
- **Next module — pick one and run it through `/orchestrate`:**
  - **rig-B** (state-driven surface deflection; see the rig-B section above). The FIRST live Gemini
    call would come here only if you want authored meshes over the procedural boxes — the rig-A.3
    validator now gates that ingest. Confirm the Gemini model id with Chad first (still a placeholder).
  - **env Stage 3 = `scatter`** (`docs/little_planet_plan.md`) — the first ENVIRONMENT module through
    the engine (color scatter / weather variable).
- When a Gemini asset IS generated, run the asset-validator (rig-A.3) in the gate stack on it; land any
  new validator legs it needs (mesh-AABB-vs-catalog, winding/outward-normals) — those activate when a
  real mesh FILE (not a procedural cube) is ingested.

## Gotchas
- **Gemini model is `gemini-2.5-pro`** (Chad's call; default in `seads_gen.py`, override via `--model` /
  `SEADS_GEMINI_MODEL`). `--dry-run` costs nothing. **The key is LIVE** (`gemini_api/.env` →
  `GEMINI_API_KEY="..."`, gitignored; `read_key` reads env-var-then-.env; validated 2026-07-08 against
  `gemini-2.5-pro`). rig-A/rig-B used procedural boxes + hand-authored shaders, so **no real generation
  call has happened yet** — the first is expected in Stage 3 (scatter GLSL). When a Gemini asset IS
  generated, run it through the gate stack (asset-validator first; a real mesh FILE also needs the
  mesh-AABB/winding legs). It writes DRAFTS to `generated/` — review + single-source + gate.
- **`.gitignore`:** `*.png`/`*.csv` are globally ignored; generated art is negated via
  `!generated/**/*.png`; catalogs must be `.toml`/`.txt`, not `.csv`.
- **Seam is sacred:** `render/` reads state, never writes, never reads a clock (`GetTime`/
  `GetFrameTime`/`chrono`). Anything needing time is app-owned (`tick_count·sim_dt`).
- **Commit green BEFORE any mutation-revert** (a `git checkout` to undo a mutant destroys
  uncommitted edits). `rm` a locked `seads_tests.exe` and confirm the relink before trusting a
  "mutant not caught" (the stale-exe trap).
- LF→CRLF git warnings on Windows are benign.
- Commands: configure `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`; build
  `cmake --build build --config Debug`; test `ctest --test-dir build -C Debug`.

---

## LATEST SESSION #8 — Sudbury real-GIS terrain (2026-07-09, AUTONOMOUS overnight)

Chad OVERRODE the 2026-07-07 hand-paint ruling (`world_build_plan.md §7`): the whole planet is now
**Greater Sudbury from REAL open GIS data** — all lakes, real airstrips, float-plane water landings.
Built + committed autonomously while Chad slept, in 5 gated increments (`9ad5359a2`..`cda615d4c`).

**What landed (all on `sandbox/world-sudbury`, gate 331/331, zero moved goldens, kernel untouched):**
- **Offline tool `offline_tool/`** (Python+GDAL venv; NOT shipped; `source/`+`.venv` gitignored). Fetches
  CDEM elevation (windowed COG read, EPSG:3979) + OSM hydrography (**1293 lakes ≥340 m**, 2106 named) +
  OSM aeroway. Projection = **azimuthal-equidistant about the OSM boundary centroid (46.59,-81.04),
  true 1:1, disk-cropped to r=π·R=47.1 km** → the sphere fills exactly, ONE antipodal pinch in remote
  wilderness (no back cap, no fill, no inflation). ONE inverse-map resample to the equirect the runtime
  bakes. Geo round-trips 2e-14; equirect inverse matches `equirect_uv` exactly (self-tested).
- **Baked assets** (committed): `assets/sudbury_{dem,color,landmask}.png` (2048×1024) + generated
  `render/sudbury_gis.gen.h` (250 named lakes + 8 airstrips).
- **Inc 1** terrain swap (`ground.use_procedural` toggle → `load_planet` selects sudbury assets, runtime
  DEM blur forced 0 since lakes are pre-flattened offline).
- **Inc 2** RGBA cubemap (albedo + landmask alpha) + **mirror-water FS branch** (reflect the view ray
  about `n=normalize(fragDir)`, sample the SHARED `sky_color` so lake+limb can't fork; LOD-faded sun
  sparkle). New pure `bake_equirect_cubemap_rgba` + tripwire test.
- **Inc 3** landing-ready data: `test_sudbury_gis.cpp` compiles the header + pins the inert data (unit
  dirs, radial+orthonormal airstrip bases from mapped endpoints, sane elevations). No landing mechanics.
- **Inc 4** palette pop (deep forest→grey rock→blue-black lakes) + HUD "GREATER SUDBURY".
- **Fable red-team: SOUND-WITH-FIXES** — projection/single-source/mirror geometry all VERIFIED correct;
  folded both P1s (fwidth hoisted out of the non-uniform water branch; lake target uses
  `representative_point()` not centroid) + P2s (right-hand basis, flatten-all-lakes, R-assert, feather
  constant, overcast dims glint).

**Smoke shots in repo root for Chad to open:** `sudbury_low.png` (relief), `sudbury_nadir.png` +
`sudbury_oblique.png` (lakes — used a throwaway nadir cam, since REVERTED), `sudbury_final.png`.

### CHAD'S MORNING CHECKLIST (fly-verdicts + 2 real decisions)
1. **FLY Inc 1** — does it read as Sudbury? pinch remote/unobtrusive? relief legible? (dials:
   `world.toml [ground] relief_scale_m`=600 gives 2.4× exaggeration; `[planet] u_offset` rotates which
   terrain sits under the +X spawn sub-point — right now spawn is upland, not downtown/a lake).
2. **FLY Inc 2 lakes** + **DECIDE the silver**: the space-first (near-black zenith) sky makes lakes read
   DARK except at grazing / sun-glint (Fable water-trap #1, physically correct). To make them POP silver
   we'd add a **water-only reflected-sky floor** (a new small knob) or brighten the water tint — YOUR
   aesthetic call. Dials today: `[water] reflectivity`=0.9, `sparkle_sharpness`=200.
3. **FLY Inc 4 palette** — `offline_tool/sudbury_config.py PAL_*` + `ROCK_SLOPE_DEG`; edit + re-run
   `offline_tool/.venv/Scripts/python.exe build_sudbury.py` to rebake (30 s, data cached).
4. **CONFIRM private airstrip coords** — Belanger ("behind Belanger Ford") + Garson are APPROX (flagged
   `confirmed=false` in the header); OSM had **Sudbury Airport (04/22, 12/30), Ramsey + Azilda seaplane
   bases, Coniston, a 01/19 strip**. Give me real coords for Belanger/Garson and I'll rebake.
5. **KNOWN soft spot** (accepted): named-lake disambiguation picks the LARGEST same-named body (there are
   many "Long Lake"/"Kelly Lake" in the district), so a hero lake's LABEL/landing-target may point at a
   distant namesake. The landmask RENDERS every lake correctly; only the named-target picks need your eye.

### To rebake after any offline edit
`cd offline_tool && .venv/Scripts/python.exe build_sudbury.py` (re-reads `world.toml` u_offset/relief +
`aircraft.toml` R; `source/` cache means no re-download). Then rebuild + `--smoke` to LOOK.

---

## ⚠️ V2 WIP — UNPLANNED, DO NOT BUILD ON WITHOUT PLAN + FABLE (2026-07-09)

Chad flew V1 and found it too low-res + no towns visible. He RULED V2: **stick to the Sudbury BASIN,
ENLARGED + highly resolved, WITH visible towns**, and chose **true 3D hero landmarks** (Superstack/
headframes via Blender→glTF + a new raylib prop renderer) + **install what's needed** (Blender installing,
task `bw6n63atl`). Footprint = basin+towns ~28 km.

**PROCESS MISS:** implementation of V2 was started WITHOUT a plan-mode plan or a Fable-5 BEFORE-consult
(unlike V1). STOP and do it right: enter plan mode → Fable BEFORE-consult the V2 architecture (the
enlarge projection change, normal-map tangent-space shading in the planet FS, the prop renderer's
placement/orient/cull math, the Blender headless pipeline) → implement → Fable AFTER.

**Uncommitted WIP (working tree, `offline_tool/` ONLY; nothing committed):**
- `sudbury_config.py`: CAPTURE_RADIUS_M=28000, GROUND_R=CAPTURE/π, ENLARGE≈1.68×, center (46.53,-81.02),
  4096×2048 textures, HRDEM_COG (1 m LiDAR `ON-SPL_ON_Greater_Sudbury_UTM17_2022-1m-dtm.tif`), urban/road
  palette + `NORMAL_W/H`. `sudbury_geo.py`: theta=rho/GROUND_R (enlargement) — self-test PASSES (2e-14).
  `sudbury_fetch.py`: `fetch_dem` (generic HRDEM), `fetch_landuse`, `fetch_roads`. `build_sudbury.py`:
  new center + calls `B.build(frame, elev, transform, lakes, landuse, roads, u_offset)`.
- **BROKEN:** `sudbury_bake.py` still has the OLD `build(frame, elev, transform, lakes, u_offset)`
  signature — not yet updated for landuse/roads/normal-map/hi-res. The tool will not run until bake.py
  is finished (or the WIP is reverted).
- V1 committed assets (`assets/sudbury_*.png`, `render/sudbury_gis.gen.h`) are still the WHOLE-municipality
  1:1 bake — re-running the (fixed) tool overwrites them with the basin-enlarged V2.
- Planned but NOT started: normal-map bake + planet FS normal sampling; OSM urban/roads rasterization in
  the albedo; Blender landmark meshes; the raylib prop renderer; world.toml cubemap_size 1024→2048.

To revert the WIP and replan from clean V1: `git checkout offline_tool/` (nothing else is dirty).
