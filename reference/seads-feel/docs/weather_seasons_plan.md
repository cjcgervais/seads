# WEATHER + SEASONS — staged plan (the spec `weather_program.md` reads)

**Thread:** WORLD (stereoscope Sudbury), on `sandbox/world-sudbury`. **NEVER touch `sim/` or `control/`**
(frozen true-flight kernel — the world thread's one inviolable rule). Judged by **feel, not fidelity**;
do not gold-plate. Fable audits BEFORE and AFTER the plan and each landed stage (**Fable = consult only**,
never a builder — Opus builds, Fable red-teams the math).

## Chad's ruling (2026-07-12 — the ask, verbatim intent)
- **Next world step = WEATHER.** Build it out as a living experiential layer.
- **LOCALIZED, not global.** "Keep weather localized so we don't get overcast but microsystems that might
  pop up." → weather is a PATCH on the map (a squall over one region), clear elsewhere — NOT the current
  whole-sky uniform haze.
- **NO WIND** ("no wind of course just for looks"). Cells pulse in place (intensity fades in/out); they do
  NOT translate across the map, and precipitation falls STRAIGHT DOWN (along local-up), no horizontal
  advection. Wind is purely a future concern; here it's visual only.
- **SEASONS.** "Snow in winter / rain in the spring." Implement seasons as the framing layer.
- **Season is RANDOM every spawn.** "Make the timeline for seasonality random on every spawn — I won't
  implement a day-long timing system, or it will be random." → the season is a single RANDOM DRAW at spawn
  (app-owned RNG), NOT a function of `t_cel`/the celestial year. It does not change during a life.
- **STATIC season selectability (for building).** "One of the game loops I'll want is a winter-only loop, so
  make the seasons but allow me to set them to static — season selectability for the purposes of building."
  → a config override forces a chosen season (bypasses the random draw). This is how Chad will build the
  winter loop and how the gate/smoke stays deterministic.

## Fable BEFORE audit — VERDICT: SOUND-WITH-FIXES (agent a2979734a1acfebfb, 2026-07-12)
All 3 P0s + P1s FOLDED into the stages below (marked ★). Summary of what changed the design:
- **P0-1** budget×envelope was a double-gate → cells now ACTIVATE from the budget via ordered thresholds
  `env_i = smoothstep(θ_i, θ_i+w, weather_haze)`; distribution invariant restated honestly (budget duty
  cycle = "weather anywhere"; cell local peak = overcast — the map average necessarily drops).
- **P0-2** `uHazeDensity` has SIX shader consumers (not two) → full single-source list in W2, incl. the star
  FS (the exact fork the star_glsl comment warns about); `weather_local(dir)` uploaded to sky+planet+star.
- **P0-3** the sky pass has no ground point → ray-perigee sample `normalize(eye+max(0,−dot(eye,dir))·dir)`,
  which converges with the planet limb by construction (anti-fork).
- **P1-1** soft-OR combine `1−Π(1−env·falloff)` (not clamp); **P1-4** precip lattice WORLD-anchored + box
  follows eye + boundary fade + fall along local_up only; **P1-5** host in NEW `render/weather_glsl.*`
  because `render/sky.cpp` is OFF-LIMITS/dirty; **P1-6** season precedence pinned; **P1-3/P1-7** finished
  double scalars into the shader, one config struct sources the CPU+GLSL dual. W1 and W4 ruled SOUND as
  written (P2-5 confirmed the W4 approach). SHIP ORDER: W2a eye-scalar (zero-fork) before W2b per-fragment.

## Relationship to the EXISTING systems (cite these; reuse, don't fork)
- **The current weather variable is GLOBAL and stays the tempo source.** `render::weather_haze(t_cel, p)`
  (`render/weather.{h,cpp}`, pure, seam-safe) returns a scalar haze[0,1] from three incommensurate-prime
  sines through a smoothstep gate (~70/25/5 clear/haze/overcast, Fable-vetted). Today `app/main.cpp` sets
  `atm.haze_density = weather_haze(t_cel) * haze_overcast_density` → the single `uHazeDensity` uniform read
  by BOTH the sky band and the planet `sky_aerial` (single-source, no fork). **We LOCALIZE this**: the
  global scalar becomes the STORM BUDGET / tempo (how much weather is active right now), and a NEW spatial
  field decides WHERE on the sphere it lands.
- **The scatter GLSL** (`render/scatter_glsl.cpp`, `scatter(viewDir,sunDir,up,sunElevCos,hazeAmount)`) is
  already multiplied by the haze amount and single-sourced into sky + planet via `sky_color()`. If haze
  becomes per-fragment, `sky_color()` gets a per-fragment haze — scatter localizes for free.
- **Celestial "season" is a DIFFERENT thing — do not collide.** `[celestial]` axial tilt already swings the
  sun's declination over the `year_period_s` (3600 s) cycle. That is the SUN's seasonal declination and is
  independent of Chad's WEATHER season. Name ours `render::Season` / config `[seasons]`, kept distinct.
- **The seam (SPEC §9, greppable):** every animation phase is a PURE fn of `t_cel`
  (`app/main.cpp` `t_cel = tick_count*sim_dt + cel_time_offset`, AT-9-pinned, no clock); `render/` reads
  state and writes nothing. The season draw is an APP-owned value (like `spawn_state`), passed read-only
  into `FrameInfo`; `render/` NEVER rolls dice and NEVER reads a clock. Weather/season NEVER feed
  `sim/`/`control/`/drone AI — render-only lighting inputs, like the celestial wheel.

## The seam this rides (the recon — verify line numbers when building)
- **`t_cel` (no-clock phase source):** `app/main.cpp` (~1049-1052) `t_cel = tick_count*sim_dt + offset`.
- **Haze plumbing:** `atm.haze_density` set per-frame in the frame loop (`app/main.cpp` ~347 default 0,
  set from `weather_haze`); `render::Atmosphere`/`set_atmosphere_uniforms` is the ONE setter both the sky
  and planet FS call; `uHazeDensity` is the single localized target.
- **Spawn / respawn:** `app::spawn_state` + the `fr.respawned` in-frame reset block (~872). The season draw
  fires at the INITIAL spawn AND on each respawn (a fresh life = a fresh random season, unless static).
- **Local up / gravity (for straight-down precip):** `-normalize(position)` — NEVER a fixed axis (SPEC §6).
  Precipitation falls along `local_up`; the camera-anchored particle volume orients to the eye's local up.
- **Additive/translucent draw tier:** `render/draw.cpp` draw order sky→planet→water→ribbons→buildings→
  trees→aircraft→lamps(additive)→translucent props→post→HUD. Precip particles slot in the translucent tier
  (after opaque depth-writers, before post).
- **Config loader:** `config/world.toml` + the strict `load_world` + `test_load_world` legs (every new
  block needs a values leg + a lock-vs-defaults leg per the AT-15 config-relative-bounds lesson).
- **HUD tag:** the existing "GREATER SUDBURY" HUD text is the model for a small SEASON tag (draw-only).

---

## The staged ladder (build one row at a time; each → gate → Fable-after → commit AWAITING-FLY)

### W1 — SEASONS FRAMING (the skeleton; no heavy visuals yet)
The framing layer everything else gates on.
- `render::Season` enum { Winter, Spring, Summer, Autumn } + a pure `render::SeasonParams` (per-season
  tunables: precip type, snow-cover amount, haze bias, tint). NEW `render/season.{h,cpp}` in
  `seads_render_core` (pure, gate-pinned — the celestial/weather pattern).
- **App-owned random draw at spawn.** `app/main.cpp`: at the initial spawn AND on `fr.respawned`, pick the
  season. **★ Precedence, pinned (Fable-before P1-6):** `SEADS_SEASON` env (winter|spring|summer|autumn)
  > config `[seasons] static_season` (if not `"random"`) > smoke/probe-forced default > random draw from a
  `std::mt19937` seeded ONCE at process start from `std::random_device` (an app-local generator — NOT in
  sim/control; cosmetic, like the screenshot rig). The env sits ABOVE the config so a winter-loop
  `static_season` smoke is still overridable, and probe/`--smoke` is deterministic. The draw is NOT a fn of
  `t_cel`, touches neither the tick counter nor the sim seam (Fable-confirmed the seam is correct).
- **Config `[seasons]`:** `static_season = "random"` (or winter|spring|summer|autumn) + the per-season
  weight (probability of each when random) + the per-season dials W2/W3/W4 consume. Strict loader
  (enum-validated string; weights ≥0 AND **Σ weights > 0** — P2-4, all-zero = UB in the weighted pick; the
  lock-vs-defaults `test_load_world` leg).
- **App→render plumbing:** the chosen `Season` + its `SeasonParams` ride read-only in `FrameInfo`.
- **HUD season tag** (draw-only): small text near "GREATER SUDBURY" showing the active season (so Chad can
  see the draw + confirm the static override / random draw works).
- **Gate + smoke:** build + ctest 0-moved; A/B via `SEADS_SEASON=winter` vs `=summer`; smoke shows the tag.
- **Fable-before target (already in this plan):** the season is a pure enum + read-only input; the ONLY
  math is the weighted random pick (must be app-side, off the render seam, off the clock) and the
  probe-determinism gate. No sphere math yet.

### W2 — LOCALIZED WEATHER FIELD (microsystems, NO wind) — the load-bearing stage
Turn the global haze scalar into a spatial field so weather is a patch on the map.
**★ SHIP THE INTERIM EYE-SCALAR CUT FIRST (W2a), THEN THE PER-FRAGMENT FIELD (W2b).** Fable-before P2-6:
the eye-scalar cut is the ZERO-FORK configuration by construction (one scalar, all six consumers agree), so
it is the risk-ordered path, not just a perf fallback. W2b carries all the fork risk (P0-2/P0-3).
- **The field:** a pure `render::weather_cell(dir, t_cel, p) -> local[0,1]` over the sphere, where `dir` is
  a UNIT ground/sample direction. Construction (Fable-vetted):
  - A small set of `N` CELL CENTERS at FIXED directions on the sphere (a golden-spiral lattice — NOT random
    per frame; **P1-3**: centers/radii/thresholds are compile-time or `hash(i)` constants, NEVER an RNG
    draw — only the *season* may be random). Each cell a fixed angular radius. **Centers FIXED = no wind.**
  - **★ Cell activation is DERIVED FROM the budget, NOT multiplied by it (P0-1 — the fix for the
    double-gate).** Give each cell a fixed ordered threshold `θ_i ∈ (0,1)` and set
    `env_i = smoothstep(θ_i, θ_i + w, weather_haze(t_cel))` (optionally shaped by a slow per-cell `t_cel`
    modulation for organic pop-up). Then budget=0 ⇒ field ≡ 0 everywhere (inherits the exact-clear 70% AND
    the spawn-clear phase guarantee); budget high ⇒ cells fade in ONE AT A TIME (C1 in the budget — **P1-2**:
    never a discrete `round(budget·N)` count) and a cell's LOCAL peak reaches today's overcast.
  - **★ Combine by soft-OR, not clamp(Σ) (P1-1):** `local(dir) = 1 − Π_i (1 − env_i · falloff_i(dir))` —
    bounded [0,1] with no clamp, C1 everywhere, overlapping squalls saturate gracefully.
  - **Falloff in dot-space (P2-1):** `falloff_i = smoothstep(cosR_outer, cosR_inner, dot(dir, center_i))`
    (no per-fragment `acos`).
- **★ The distribution invariant, HONESTLY restated (P0-1):** localization LOWERS the map average by design,
  so we do NOT claim "map-integrated 70/25/5 preserved." The correct invariant: **the budget's duty cycle
  keeps ~70/25/5 for "weather active ANYWHERE," and a cell's LOCAL peak reaches overcast** (so the pilot's
  felt distribution UNDER a cell matches the old global one).
- **★ The sky-pass sample point (P0-3 — sky rays never touch ground):** sample at the view ray's near sphere
  intersection where it exists, else the ray perigee: `sampleDir = normalize(eye + max(0, −dot(eye,dir))·dir)`
  — continuous in `dir`, and at a grazing ray → the tangent point = where the planet limb fragment sits, so
  **sky-pass haze and planet-aerial haze converge at the limb by construction** (the anti-fork). Upward ray
  ⇒ perigee = eye ⇒ you correctly see haze overhead inside a cell. Put this mapping in the SHARED GLSL.
- **★ SINGLE-SOURCE across ALL SIX consumers of `uHazeDensity` (P0-2 — the plan first counted two):**
  (1) `sky_luminance` band haze; (2) the scatter gate in `sky_color`; (3) `sky_aerial` extinction;
  (4) the star FS — `hazeAtEye` extinction AND `sky_luminance`; (5) planet water — reflected-star dim,
  sun-glint dim, moon-glint dim; (6) aurora ground-glow `clearG`. A shared GLSL `weather_local(dir)` (cell
  centers/radii fixed uniforms + per-cell `env_i` scalars) uploaded by the ONE `set_atmosphere_uniforms`
  path to sky, planet, AND star shaders; `sky_luminance`/`sky_color`/`sky_aerial` take haze as a PARAMETER;
  **grep-kill every raw `uHazeDensity` read.** Ray consumers use the P0-3 mapping; eye/surface-anchored
  consumers (stars, water dims, `clearG`, the W3 precip gate) sample at their own local dir. Water reflected
  sky uses the FRAGMENT-LOCAL haze (P2-3: the lake sits under its own weather).
- **★ PROCESS (P1-5 — the mechanism edits an OFF-LIMITS file):** `render/sky.cpp` (home of `sky_color`,
  `kSkyUniformsGLSL`, `set_atmosphere_uniforms`) is on the cross-agent OFF-LIMITS list and `render/sky.h`
  is dirty in the tree NOW. So: host `weather_local()` + the sample-point mapping in a NEW
  `render/weather_glsl.{h,cpp}` (the `scatter_glsl.cpp` pattern, pure, validator-gated), and stage the
  unavoidable `sky.cpp` call-site edits as an explicit, coordinated, MINIMAL `git apply --cached` hunk —
  or sequence W2b behind the celestial agent's landing. W2a (eye-scalar) needs NO sky.cpp edit.
- **W2a interim (ship first):** feed `weather_cell` at the EYE's ground-track into the existing scalar
  `uHazeDensity` → "you fly into a squall, it clears as you leave." Zero fork (all six consumers read the
  one scalar). Then W2b upgrades to per-fragment for the distant-haze-bank look.
- **P1-7 (the unavoidable CPU/GLSL dual):** the precip gate needs a CPU `weather_cell` eval; fragments need
  the GLSL `weather_local`. ONE config struct sources every constant for both; the CPU copy's ONLY consumers
  are feel-tolerant (precip gate, eye-track scalar) — never anything that must pixel-match the shader; note
  the pairing in both files so a future edit moves both.
- **Gate + smoke:** clear regions read dark-starry (unchanged), a cell region hazes; A/B a forced-cell
  offset (a `t_cel` where `weather_haze` is high) vs a clear offset. Verify NO whole-sky uniform haze remains.

### W3 — PRECIPITATION (snow=winter, rain=spring; NO wind)
The season-gated payoff, only where a weather cell is active.
- **★ A WORLD-ANCHORED jittered lattice with only the BOX following the eye (Fable-before P1-4).** NOT a
  camera-frame snow-globe (that reads as wind following you). Particle positions are a world-space jittered
  lattice re-binned as the eye moves (so relative streaming past the plane emerges for free during horizontal
  flight); the world-axis grid is CELL BOOKKEEPING ONLY — never a visible direction. The ONLY visible
  displacement is the DOWNWARD fall along `local_up = −normalize(pos)` (phase a pure fn of `t_cel`,
  wrap-safe, computed in double app-side per P1-3); NO horizontal advection term (Chad's no-wind). Streak
  orientation = `local_up` only. **Never build a tangent basis from a fixed reference axis for anything
  visible** (two antipodal degeneracies; GO-ANYWHERE says someone flies through them).
- **★ Boundary fade (P1-4b):** an alpha envelope → 0 at the box boundary (a radial fade about the eye) so
  particles entering/leaving the box are C0 (no pop), which also hides the re-binning.
- **Season gate:** Winter → SNOW (slow, large, soft white flecks, gentle vertical drift); Spring → RAIN
  (fast, thin, near-vertical grey streaks). Summer/Autumn → none (dry). Rendered mono/silver by default —
  snow reads white (fine in B&W), rain reads as grey streaks (no chroma exception needed).
- **Weather gate:** precip intensity = season-precip × `weather_cell(eyeDir, t_cel)` → it only snows/rains
  when the eye is under an active microsystem, and fades as you leave it (ties W3 to W2 — no precip in
  clear air).
- **Draw:** a new `render/precip.{h,cpp}` in the translucent tier (after opaque depth-writers, before
  post); additive/alpha; night-gated brightness is not needed (precip is lit by ambient). `SEADS_NO_PRECIP=1`
  A/B; the phase is app-owned (`tick_count`) never a clock.
- **Gate + smoke:** `SEADS_SEASON=winter` under a forced weather cell shows snow; `=spring` shows rain;
  `=summer` shows none; clear-air shows none.
- **Fable-before target:** the particle wrap is C0-continuous (no pop at the wrap), the fall is EXACTLY
  along local-up (no horizontal advection — the no-wind invariant), phase is pure `t_cel` (no clock), and
  the volume tracks the eye without a fixed world axis (SPEC §6).

### W4 — SEASONAL GROUND (winter snow-cover; the winter-loop payoff) — OPTIONAL / LAST
So a winter spawn LOOKS like winter from altitude (for the winter-only loop Chad wants to build).
- A season TINT applied to the planet albedo in the planet FS: Winter → whiten toward snow (lift + slightly
  desaturate — already mono, so a luma lift with slope-darkening so cliffs/rock still read); the whitening
  weighted so lakes/rivers (water mask) stay dark mirror water, and steep rock faces keep some grey (a
  snow-accumulation-by-slope term). Spring/Summer/Autumn → identity (or a faint autumn dim). Single dial
  `winter_snow_cover` [0,1].
- Single-sourced in the planet FS from the season uniform (no new terrain bake — a shader tint, so it
  toggles instantly with the static-season override, which is exactly what the winter-loop building needs).
- **★ Fable-vetted (P2-5): a MULTIPLICATIVE albedo lift BEFORE the lighting products** preserves the
  terminator (`ndl` untouched), the moon fill, and the aerial (applied after); MASK by the existing `water`
  variable so the mirror branch's `lit` base isn't brightened; slope term `dot(shN, normalize(fragDir))`
  uses only FS-available seam-safe vectors — no fixed axis. NOTE: whitened albedo × `uNightFill` brightens
  winter nights (physically right for snow) and WILL move the deep-night `--smoke` baseline — expect it,
  don't "fix" it.
- **Deferrable:** if scope runs long, W4 is where the loop stops and hands to Chad (W1–W3 already deliver
  "snow in winter / rain in spring" as weather + precip; W4 is the ground-cover polish for the loop).

---

## Guardrails (from CLAUDE.md + the copper-cliff loop, do not relearn the hard way)
- **Kernel firewall:** never touch `sim/`/`control/`; the flight goldens must not move (`git diff --stat`
  the golden dir every gate — moved = STOP, do not re-record).
- **Cross-agent files are OFF-LIMITS:** `render/sky.h`, `render/sky.cpp`, `generated/gen_log.jsonl`,
  `bf109_preview/`, `docs/ballistics_*`, `offline_tool/ballistic_*`, `app/*_synth.h`, `render/*_audio.h`,
  `assets/audio/`. Commit by EXPLICIT path, NEVER `git add -A`. When you MUST edit a file another agent has
  uncommitted work in (`app/main.cpp`), isolate your hunk with `git apply --cached` and verify
  `git diff --cached` shows only your lines.
- **The green gate is blind to the app binary** — no ctest runs `seads.exe`. Every visual stage needs a
  `--smoke` shot read back + the headless asset/source validator; author your own VS if you touch shaders
  (raylib's default VS emits only fragTexCoord/fragColor).
- **Verify emissive/gated features with a SIGNATURE COLOR at a KNOWN cell, not a confounded metric** (the
  windows-twinkle lesson) — e.g. force a season + a weather cell + a signature tint, read that exact cell.
- **Config-relative bounds:** derive every test bound from the config the mechanism reads (AT-15).
- **Mono/silver by default:** no new chroma. Snow = white (fine). Rain = grey. No warm exception here.
