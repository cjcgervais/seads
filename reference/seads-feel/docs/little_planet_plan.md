# SEADS World — Environment Pivot: "The Little Planet" (visionary plan)

## Context — why this change
The ratified world plan (`docs/world_build_plan.md` / `docs/world_art_direction.md`, both 2026-07-07)
built toward a **static** "bright silver-newsreel sky / air-over-a-place / noon-everywhere / no
day-night" world. On 2026-07-07 Chad ruled a **pivot**: the world becomes a *small tilted planet in
space* with a full living sky — sun, moon, stars, day/night, and live seasons. This doc is the
source-of-truth visionary plan; it **supersedes the sky/sun/celestial portions** of the world plan and
**retains** the ground pillars (B&W monochrome, Sudbury/Northern-Ontario, saturated chroma planes as the
only color, mirror-silver lakes, tactical legibility as the prime directive).

Process (Chad's ruling): *audit before AND after the plan; approve a vetted plan now, not fix later.*
**APPROVED 2026-07-07** after a fresh-context **Fable 5 before-consult** (which produced the staged build
order below) + a **Fable after-red-team** of the finalized plan (caught 2 P0s + 6 P1s, all folded — no
vision element moved). This is the live source of truth for the world/celestial build.

**Timing note (the pivot's cheapest argument):** the old P1 ladder is at "YOU ARE HERE / step 1" — the
silver sky has NOT been flown yet, so **nothing tuned against the old sky exists to re-fly.** The pivot
lands at the one moment it costs zero rework.

## The reframe (state it out loud, or someone reaches for a blue sky)
This does not *retire* B&W — it **generalizes** it. The old flat silver was one exposure of a world that
now has a full tonal cycle. The sky stays **strictly monochrome**: luminous silver-gray day → graphite
dusk → near-black moonlit night → black space with white stars, a white sun disc, a silver moon. The sky
is monochrome MOST of the time — **but OCCASIONAL weather brings bounded atmospheric-SCATTERING color**
(Chad 2026-07-08): Rayleigh **blue** with the sun high, **Mie red/orange at sunset**, haze-gated and
horizon-weighted so it never becomes a permanent blue dome. This is a *deliberate, bounded relaxation* of
"strictly monochrome" for the sunset drama — planes stay the PRIMARY chroma (the scatter color is
occasional, mostly at the sunset limb, desaturated, and a plane still silhouettes against it). Result:
"planes are the primary chroma" holds — black space and a silver night remain the dominant backdrops; a
saturated plane against star-flecked black is still the most readable frame the game will produce.

## The vision (Chad, 2026-07-07 — every element load-bearing; NONE dropped)
- **Full-space look.** Stars/sun/moon everywhere; the flat sky is retired (generalized, above). Visibly
  a little planet in space.
- **Sun · Moon · Stars.** **Moonlit nights** — moonlight is the night fill-light that keeps "can I see
  the other plane?" alive after dark. Silver moon + moonlit silver lakes = the hero shot.
- **Named constellations** — **Big Dipper, Little Dipper, Orion's Belt, Polaris** — real, recognizable, a
  **diegetic celestial compass** (found via the Dipper pointer stars → Polaris on the spin axis; the sky
  wheels around it). *Added by Fable, ratified:* a small **southern asterism** (a Southern-Cross-like
  kite) marking −â so the compass works in BOTH hemispheres.
- **Day / night.** Planet spins on a tilted axis; sun/moon/stars wheel. *Emergent jewel:* the terminator
  crawls the ground at only **~26 m/s** (ω·R) while planes fly ~200 m/s — noon and midnight coexist ~47 km
  apart, ~4 min flying. You can **outrun the sunset**, hold golden hour, or **dive into the night side to
  break contact** — a real tactic from a lighting feature.
- **Seasons — FULL, SLOW, LIVE.** Tilt drives seasonal sun elevation + day length; ground changes live:
  **winter SNOW** (bright-white land, **grey wind-polished ice lakes** — never white-on-white), **summer**
  dark bush, spring/fall transitions (**patchy melt** — snow retreats uphill, not a global crossfade;
  **lake freeze/thaw lags** the sun by a config fraction). Snow is a Sudbury hero look.
- **Fast arcade time:** **1-hour days** (spin 3600 s), **a year every 12 h** (⇒ 12 days/year, each season
  ≈ 3 h ≈ 3 day/night cycles). Daily spin ≈ 0.1°/s — alive across a session, ~static within one dogfight.
  *Real twilight lasts ~2 min by construction* (short 1-h day), which mercifully bounds the dusk
  contrast trough.
- **Sun smaller & much closer** (artistic license — "it balances out"): a small disc at a finite
  position, brightness a decoupled config knob; the residual parallax over the flight *is* the "small
  nearby star" feel.
- **Weather, not a fixed envelope** (Chad 2026-07-08) — CLEAR most of the time (stars visible at ~any
  altitude), LIGHT HAZE sometimes, OVERCAST rarely (a ~7 km cloud deck). When haze is present,
  **atmospheric SCATTERING paints the sky**: Rayleigh **blue** with the sun high, **Mie red/orange at
  sunset** (long optical path) + a forward-scatter halo around the sun — *apparent sometimes*, strength
  tied to the haze amount. Clear/thin air = dark starry (minimal scatter). Stars are the default backdrop,
  not something you climb above a haze deck to find. The physical air (drag/lift/ceiling ~8 km) is the
  flight branch's separate, provisional model.
- **B&W Northern-Ontario Sudbury below** throughout.
- *Added by Fable, ratified charm:* a **north-pole cairn/flag** prop where â pokes through (off the
  Sudbury hero content) — "fly to the North Pole" becomes a 10-min expedition with Polaris at the zenith
  as arrival proof. *Deferred, config-off, named so nobody smuggles it in:* a **grayscale aurora**
  (silver curtains, zero chroma) — the one future bold move that fits the palette.

## Seam constraints (SPEC §5/§6/§9 — non-negotiable; the flight kernel is sacred)
- Every new element is **LIGHTING / BACKGROUND only**; nothing feeds a smoothed/slewed basis into aim or
  camera-rotation. Seam-safe by construction.
- The sim sphere is **INERTIALLY FIXED.** "The planet spins/orbits" = the **celestial sphere wheels about
  a fixed tilt axis â on a render clock**; the ground never moves in world coords. A rotating GROUND is
  **BANNED** (breaks the sim frame).
- **Time is TICK-DERIVED, not wall-clock:** `t_cel = tick_count · sim_dt`, with three independent phase
  offsets (see Structural model). **`tick_count` does NOT exist yet** (verified — `Accumulator` carries
  only `acc_`, `LoopState` has no counter, `main.cpp frames` is frame-not-tick) — Stage 1 ADDS
  `long long tick_count` to `LoopState` (`app/instructor_tick.h`), incremented once at the top of
  `app::tick` (both the app AND `step_frame` drive `app::tick`, so both advance it in lockstep);
  **monotone across crash-reset / F1 / raw mode** (do NOT reset it in the crash branch — celestial time
  survives death). *NOTE (Stage-1 red-team correction):* the harness `ClosedLoop` (`test/harness/instructor.h`)
  is a SEPARATE mirror type that does NOT call `app::tick` and carries no counter — this is moot because
  `tick_count` feeds only render, never `control::step`, so the mirror-equivalence tests cannot see it (the
  S7-hrz "the mirror cannot REPRESENT the mechanism" class). New `LoopState` field ⇒ the moved-field
  discipline: extend `test_at9` `loopstate_eq` + the hand-composed reference, mutation-verify (drop the
  increment; reset it on crash). **LANDED 2026-07-08** (commit — the increment + the two AT-9 pins +
  `loopstate_eq`; both mutations verified). Wall clock is
  seam-*legal* (sky never touches aim) but plan-*banned*: it makes the probe matrix non-reproducible and
  drifts at 30 vs 240 fps (the S7-mouselevel class). **Seam grep gains: no
  `GetTime|GetFrameTime|std::chrono|time(|clock(` on any celestial/sky path.**
- **Celestial time NEVER feeds `sim/`, `control/`, or drone AI** (pre-banned, greppable) — a night-ops
  mechanic would be a deliberate `scenario.toml` ruling, never a render read.
- **No bare numbers** → new `world.toml` blocks (below). The existing bare `0.38 + 0.75·ndl` ambient in
  the planet FS (`planet.cpp:51`) moves to config here.
- **Flight `ctest` is the last gate step, always** — the map may not cost the kernel anything.

## Structural model (Fable-formalized; ratified)
**One inertial celestial frame, one per-frame wheel.** Inertial frame holds: the star catalog (fixed unit
dirs), the sun on the ecliptic circle `ŝ(t)=cos(ω_year t+φ_y)ê₁+sin(ω_year t+φ_y)ê₂`, the moon on its own
inclined circle `(ω_moon t+φ_m)`. One map to world per frame: `W(t)=R(â, ω_day t+φ_d)` applied to ALL
celestial content. **`day_period_s` is the SOLAR day (what the player feels)** — the sidereal wheel rate is
DERIVED `ω_day = 2π/day_solar ± ω_year` (a raw `2π/day` puts noon-to-noon off by ~1/12; the year assert is
against SOLAR days). The epoch is **three INDEPENDENT phase offsets** `φ_d=epoch_day_frac`,
`φ_y=epoch_year_frac`, `φ_m=epoch_moon_frac` — NOT one epoch time (so the probe sets moon phase
independently of season × time-of-day; without this the Stage 5/6 full-moon vs new-moon cells at the same
season+time are unreachable). Consequences, all unit-pinnable:
- **Polaris fixed:** `W(t)·â = â` (identity — â is the axis).
- **Seasons fall out:** `sinδ(t)=dot(ŝ(t),â)=sin(tilt)·sin(ω_year t)` — ±tilt over the year.
- **Winter/summer constellations fall out:** stars + sun share the daily wheel, drift only at ω_year → sun
  laps the catalog once/year (Orion far from the sun in winter = a winter constellation).
- **Day/night is SPATIAL:** half the sphere is always dark → night legibility is *always live*, not one
  matrix cell. Place the hero Sudbury content at a **mid axis-angle (~45–65°)** so it gets real day/night
  swing (a content-placement choice, not a coordinate system); keep towns off the ±â caps.

**Pure module:** `render/celestial.{h,cpp}` — glm-only, **CPU double**, zero raylib, in
`seads_render_core` (the `sphere_param` extraction pattern → headlessly ctest-pinned). Shaders receive
only finished **vectors/matrices** (sun dir, moon dir, sky-frame rotation, season phase ∈ [0,1)), never a
raw large `t` (float `sin(ω·43200)` is driver garbage).

**Sky = per-pixel view-ray + altitude function** replacing `ClearBackground` (`draw.cpp:160`): a
fullscreen quad drawn first inside `BeginMode3D` (depth-write off), **corner rays computed CPU-side from
the SAME `off_center_frustum` FrustumBounds + eased `info.fovy_deg`** the world uses (draw.cpp:167-181) —
or the horizon swims against the world on every lens-shift/RMB-zoom ease. Below envelope = lit atmosphere
value (day↔dusk↔moonlit-night by sun elevation over `local_up=normalize(eye)`); above = space. Stars/sun/
moon draw in this pass; the planet mesh occludes below-horizon sky for free.
- **Scattering (cheap single-scatter, NOT a physical Preetham/Hosek model — do not gold-plate):** the
  `kSkyGLSL` adds a Rayleigh term (blue, ∝ view-sun angle, sun high) + a Mie term (forward-scatter halo
  around the sun + sunset reddening from the long low-sun optical path). The WHOLE scatter contribution is
  **scaled by the current haze-weather amount**, so clear/thin air stays dark-starry and only hazy weather
  colors the sky. Sun elevation drives the hue (high → blue, low → red/orange). A few shader lines; it
  rides the same single-source `kSkyGLSL` the water reflects (so hazy sunsets mirror in the lakes for
  free).

**Sun disc:** finite position `sun_distance_m·dir` (≥30·R, loader-asserted); **`sunDir` uniform computed
eye-relative each frame** (exact for disc + specular; ≤~2° far-diffuse error, invisible) — else the mirror
lakes flash ~2° *beside* the disc (origin- vs eye-relative at 450 km). `fwidth`-smoothed edge + small glare
halo (zoom-correct via fwidth).

**Lighting model = TWO directional lights:** `sunDir/sunIntensity` + `moonDir/moonIntensity`, each with an
elevation-curve intensity, plus the `night_fill_min` floor. **Stop there** — no HDR, no libration, no
earthshine. Moon phase = one `dot(moonDir, sunDir)` term in the disc shader.

**Seasons = shader-procedural over the existing bake (NOT N cubemaps):** extend the albedo bake to **RGBA8**
(A = landmask, needed by water anyway) + a small **R8 height cubemap** (~6 MB @1024). Winter is a shader
function: `snow = smoothstep(…, seasonPhase − noise3(fragDir·k) − heightBias(h))` (patchy, uphill retreat)
and `lake_ice∈[0,1]` (lagged phase) lerping the water branch mirror→grey-ice. One `seasonPhase` uniform +
per-season config rows; **per-season authored EXPOSURE** (never adaptive auto-exposure — a smoothed
frame-quantized signal in the look). No re-bake, no hitch, no second asset.

**Single-source the sky GLSL** at the string level: one `kSkyGLSL` constant concatenated into the sky pass
FS, the planet FS (haze color), and the later water branch (world plan trap-1: sky and water must never
disagree at the horizon — now with more inputs, so it matters more).

**Polaris vs §6 (ruled): NOT a violation.** â is render-only *content* (drawn, never read by any
up/bank/level/aim/telemetry computation); the compass lives in the player's head, exactly like the
world-fixed terrain. Provisos: (1) **naming discipline** for the seam grep — use `spin_axis`, `polar_dir`,
`declination`, `ra_deg/dec_deg` (authored *sky content*, converted to unit dirs once in the loader); NEVER
`north`/`heading`/`lat`/`lon`; (2) **no HUD element computes a bearing from â** (that would be an
instrument reading a global axis — the diegetic compass is the whole point).

**`planet.cpp` evolution (pole-free cubemap untouched):** FS keeps `texture(cubemap, fragDir)`; adds
uniforms `moonDir`, `sunIntensity/moonIntensity`, `seasonPhase`, `eyePos`; varying `fragPos`; A-channel
landmask; the shared `kSkyGLSL`. The bare `0.38+0.75·ndl` becomes `night_fill_min` + intensity curves.

## Config surface (new `world.toml` blocks; `[sky]`→`[atmosphere]` migration is part of Stage 2)
```toml
[celestial]
orbit_normal = [0,1,0]        tilt_deg = 23.0        tilt_lean = [1,0,0]   # loader derives unit spin_axis
day_period_s = 3600.0   # SOLAR day; sidereal wheel derived = 2pi/day +- omega_year
year_period_s = 43200.0   # asserted integer x SOLAR day_period_s (12 days/yr)
epoch_day_frac = 0.35  epoch_year_frac = 0.45  epoch_moon_frac = 0.0   # 3 INDEPENDENT phase offsets
epoch_mode = "fixed"   # fixed|persist (persist: write once on clean exit, absence=>fixed, never on probe path)
sun_angular_diameter_deg = 0.35   sun_distance_m = 450000.0  # >=30*R   sun_intensity = 1.0  sun_glare_deg = 3.0
moon_angular_diameter_deg = 1.5   moon_period_s = 5400.0   moon_inclination_deg = 8.0  # >= disc-overlap bound
moon_intensity = 0.12         star_brightness = 0.8   star_filler_count = 400  star_filler_seed = 7
# [[celestial.stars]] name, ra_deg, dec_deg, mag  — authored catalog incl. Dippers/Orion/Polaris/southern kite
[atmosphere]
# WEATHER, not a permanent envelope (Chad 2026-07-08): the sky is CLEAR most of the
# time (stars visible — NO fixed haze-top / star-onset altitude; stars are the
# DEFAULT, at ~any altitude), LIGHT HAZE sometimes, OVERCAST rarely. Haze is a
# weather STATE driven by a slow DETERMINISTIC weather variable (a function of t_cel,
# seam-safe — like seasons), never a fixed altitude gradient.
weather_clear_frac = 0.70  weather_haze_frac = 0.25  weather_overcast_frac = 0.05  # rough weights
haze_density_light = 0.35  haze_scale_m = 2300.0  # LIGHT + low when present (not a wall)
cloud_deck_m = 7000.0      # overcast deck altitude (the ~7 km perch zone) — RARE
# Atmospheric scattering — apparent SOMETIMES, strength MULTIPLIED by the current haze amount
# (clear/thin air => ~0 => dark starry). Cheap single-scatter, not a physical model. The ONLY
# colored sky entries — weather-gated + horizon-weighted so planes stay the primary chroma.
scatter_strength = 1.0            # overall gain (x current haze weather)
rayleigh_tint = [0.35, 0.55, 1.0] # blue day sky, sun high (desaturated)
mie_tint      = [1.0, 0.45, 0.20] # red/orange sunset + forward sun halo, sun low
# Physical air (drag/lift/ceiling ~8 km, density h0=4000/sigma~2340) is the FLIGHT
# branch's model — separate from this VISUAL weather, and PROVISIONAL (not yet flown).
sky_day = 0.82  sky_dusk = 0.30  sky_night = 0.05   dusk_lo_deg = -8.0  dusk_hi_deg = 6.0
night_fill_min = 0.10     # THE legibility floor (starlight/airglow; moon adds on top)
horizon_definition = 0.08  dither = 0.004
[seasons]
lake_freeze_lag_frac = 0.06   transition_noise_m = 900.0   snow_height_bias_m = 250.0
[seasons.summer] snow_cover=0.0 lake_ice=0.0 exposure=1.00 ground_gain=1.00
[seasons.winter] snow_cover=1.0 lake_ice=1.0 exposure=0.80 ground_gain=1.05   # spring/fall interpolate
```

## Adversarial requirements (folded as design constraints + probe assertions)
- **P0 — Night = silhouette-against-sky, GATED ON PLANE-CONTRAST.** Design intent: above the horizon,
  planes read as dark shapes crossing the brighter moonlit sky + a moon-glint spec (brightness, not
  chroma) + contrails as night tracers. The **universal night gate is the per-cell plane-vs-background
  CONTRAST floor** (the probe already measures it) — NOT a blanket "sky>ground": moonlit winter snow is
  legitimately brighter than the night sky (a dark plane on bright snow reads fine — don't gate a phantom,
  the AT-6 lesson). Keep sky>ground ONLY as an assertion on the **snow-free** (summer/fall) night cells.
  Floor: `night_fill_min` (no-moon geometry never goes black). Settled by **two Stage-0 throwaway mocks**
  with PINNED values (encode the claim, not a vibe): (a) full-moon — clear = `sky_night + moon`, ambient =
  `night_fill_min` + moon; (b) new-moon floor — clear = `sky_night` alone, ambient = `night_fill_min`.
  Chad's two verdicts map 1:1 onto the two knobs. Debrief question: at the new-moon floor, is a
  **below-horizon** bandit (seen against dark ground, no sky behind) findable at all? — the sub-case no
  sky-side knob can fix; its answer scopes `night_glint_boost` + contrail-tracers as load-bearing vs polish.
- **P0 — Deterministic time + pure module land FIRST** (Stage 1) or the probe matrix / A-Bs / season pins
  have no reproducible baseline (the re-baseline trap).
- **P0 — Sky pass frustum-exact** under lens-shift + zoom (Stage 2) or the backdrop swims on the aim path.
- **P1 — Winter lakes = grey ice** (darker than snow, weak icy spec) + **probe assert lake-vs-land contrast
  ≥ floor in ALL seasons** (300 beacons must not vanish white-on-white).
- **P1 — Per-season exposure** (snow blows out a summer-tuned `[tone] contrast`); constant per season,
  never adaptive.
- **P1 — Dusk contrast trough:** short `dusk_lo/hi` band (~2 min anyway), `horizon_definition` floor,
  sun-elevation-independent plane rim below a floor (read of last resort).
- **P1 — Stars as fwidth quads, not GL_POINTS** (points strobe): magnitude→size+brightness, extinction
  fade by local sky luminance, glare suppression near the disc.
- **P1 — Dither** the twilight/night gradients (8-bit banding); synergizes with the grain pass.
- **P1 — Ground haze lands WITH the sky (Stage 2), not later:** the planet FS must CALL the same
  `kSkyGLSL` aerial perspective (`eyePos` uniform + `fragPos` varying) or the horizon is a hard dark limb
  against a bright hazed sky (single-sourcing the string prevents a fork, not a missing CALL). Stage-2
  probe adds a **horizon-continuity check** (sample a screenshot column across the limb; step ≤ config).
- **P1 — Probe covers GEOMETRY, not just season×time:** ≥2 view geometries per cell
  (target-above-horizon vs target-below-horizon — the below-horizon night read is the sky>ground guarantee's
  blind spot), plus two fixed cells — **noon-space-backdrop** (high-alt look-up: chroma plane on pure
  black ≠ on silver sky) and **terminator-straddle** (split-luminance background; the 26 m/s terminator
  crosses fights routinely).
- **P1 — Doc supersession hygiene FIRST:** edit `world_build_plan.md` (step-2 "noon sun" → "celestial
  sun"; §6 DO-NOT "day/night terminator" → formal supersession note; "bright newsreel sky" → the reframe)
  and migrate `[sky]`→`[atmosphere]` in `world.toml`+`load_world.{h,cpp}` in ONE commit (strict loader
  throws on stale keys).
- **P2:** eclipse — inclination makes overlap RARE, not impossible (conjunction AT a node still overlaps,
  like real eclipses); rule a draw order (sun over moon) so it renders sanely + keep the Stage-8 revisit;
  re-verify sparkle under a *moving* sun; optional HUD night-dim (keyed to sun·local_up, local_up
  recomputed, render-cosmetic, only if Chad reports glare — reticle contract untouched); pin "Orion in
  winter" (`angle(sun,Belt)>120°` at winter solstice).

## Build order (8 stages; each: single mechanism → config-tabled → deterministic gate where one exists → **flight `ctest` green LAST** → one-knob Chad flights in `docs/flight-log.md` under a new **Legibility** star → separate-context red-team at ★ boundaries; nothing coupled lands at once)

**GROUND-TRACK INTERLEAVE (P0-2 — the 8 celestial stages do NOT stand alone):** the retained ground work
from the OLD plan interleaves and is PREREQUISITE to later stages. (i) old P1 step 3 (Sudbury/placeholder
source → **RGBA8 cubemap with A=landmask + lake-flatten-before-blur**, world_build_plan §2 traps 2/3) and
(ii) the **water branch** (old §2, reflecting the Stage-2 `kSkyGLSL`) both land **between Stage 3 and
Stage 5** under the old plan's own gates; **the RGBA8/landmask bake moves OUT of Stage 6 into here.** Old
P2 (the **plane spec+glint shader**) is prerequisite to Stage 5's `night_glint_boost`. Stage 5's
moonlit-lake hero shot and Stage 6's `lake_ice` REQUIRE the water branch — without this interleave an agent
reaches Stage 6 with Earth continents, no landmask, and nothing to ice.

- **Stage 0 — Mocks + instruments (pre-code de-risk, throwaway).** Night mock + winter mock (settle
  `night_fill_min` + winter palette, the two scariest cells, zero architecture); fix the
  `SEADS_SPAWN_ALT`→`--smoke` quirk; build the **target-visibility probe parameterized `(season_frac,
  day_frac)`**. *Chad flies both mocks — these are the ratification evidence.*
  **NIGHT VERDICT (2026-07-08, flown — PASS, validates the proposed defaults):** full-moon (ground floor
  `0.22`) "pretty good," raisable later; new-moon floor (`0.10`) ground reads fine ⇒ `night_fill_min≈0.10`,
  moon `+~0.12`. **Caveat Chad surfaced:** the mock's flat GREY night sky is the WRONG stand-in — the real
  new-moon sky is near-black space + stars (Stage 4), so the below-horizon / no-stars-behind read rides on
  the moon-glint + contrails (stays load-bearing, as planned). **WINTER VERDICT (2026-07-08, flown):
  PASS — "looks great"** (snow-white ground; the contrast-inversion read holds); the composed
  **winter-night** (snow under moonlight) also read "great" — the hero shot confirmed. Both Stage-0 mocks
  cleared, throwaways reverted. **AUTONOMOUS STAGE-0 WORK LANDED (2026-07-08, gate 233/233):** (i) the
  `SEADS_SPAWN_ALT`→`--smoke` quirk is FIXED — `spawn_state` gained a defaulted `alt_m` (crash-branch
  respawn stays env-free/deterministic), main.cpp reads the env caller-side for the INITIAL spawn only,
  bit-identical default (no golden moved); (ii) the **target-visibility probe** is built — pure oracle
  `render/probe.{h,cpp}` (Rec.709 luminance + max−min chroma, plane-vs-bg Michelson + peak-EXCEEDANCE
  contrast, sphere-aware `probe_placement`), the `--probe ALT RANGE [csv] [season] [day]` rig in main.cpp
  (deterministic hands-off, injects one bandit depressed below the sphere horizon, framebuffer readback →
  CSV), pinned by `test/unit/test_probe.cpp` (13 legs). `(season,day)` are the API surface for Stages 1+,
  inert+logged now. **TWO fresh-context Fable 5 red-team rounds** (both found real P0s — round 1: flat-6°
  depression measured plane-vs-sky; round 2: dip/burial radius conflation re-opened it at alt≲1800 m +
  peak biased by ground variance — all P0/P1 fixed, 6 mutations verified).
- **STAGE 1 ★ LANDED (2026-07-08, gate 248/248).** `render/celestial.{h,cpp}` (pure glm-double in
  `seads_render_core`): `make_celestial` (tilted spin axis â, ecliptic/equatorial bases, sidereal wheel),
  `sky_wheel`/`sun_dir`/`moon_dir`/`sun_declination`/`season_phase`/`radec_to_dir`/`star_world`;
  `test_celestial.cpp` (12 legs). `[celestial]` config block + strict loader (`require_vec3`) +
  `test_load_world` legs. `tick_count` on `LoopState`, bumped at the top of `app::tick`, monotone across
  crash, `test_at9` pins (frame-rate independence + no-reset-on-crash + `loopstate_eq`). Seam clean
  (celestial time never reaches sim/control/drone; no wall-clock). **ONE fresh-context Fable 5 ★ red-team**
  (SOUND-WITH-FIXES, physics verified independently correct): two P1 coverage holes fixed (the moon's
  diurnal wheel was wheel-invariant-blind; nonzero epoch phases were droppable) + P2s (RA handedness pin,
  season_phase coverage, comments) — all mutation-verified. **NEXT: Stage 2 ★** (sky pass + ground haze at
  a frozen epoch; the app finally CONSUMES `make_celestial`, so the deferred `sun_distance >= 30*R` check
  lands there, `>=` not `>` — the committed 450000 sits exactly on the boundary).
- **Stage 1 ★ — Pure celestial core (no visuals).** `render/celestial.*` in `seads_render_core`;
  `[celestial]` block + strict loader; **add `tick_count` to `LoopState`** (P0-1: increment top of
  `app::tick`; monotone across crash/F1/raw; extend `test_at9` `loopstate_eq` + reference); `t_cel`
  threaded app→render as data; seam-grep additions. *Gate:* unit pins — **solar-day return** (sun azimuth
  over a fixed ground point recurs after exactly `day_period_s`; mutation: raw ω_day fails by ~1/13 day),
  period closure, Polaris invariance, ±sin(tilt)/equinox-zero declination, sun-laps-stars,
  Orion-winter-angle, **moon-phase-independent-of-season**, moon-inclination assert; mutation-verify (flip
  wheel sign, break year coupling, drop the tick increment).
- **STAGE 2 ★ LANDED (2026-07-08, gate 257/257; `f8ec4c5a` 2a → `834e69db` 2b → `cfce4fbe` 2c).** Three
  ordered sub-increments (plan: `~/.claude/plans/adaptive-inventing-karp.md`; a Fable-5 before-consult of
  that plan found 2 P0s + 5 P1s, all folded pre-code): **2a** probe robustness (`render::probe_camera_forward`
  points the cam AT the target — fixed the >1800 m off-frame blindness; `ProbeGeometry` Below/Above +
  `clear_of_terrain`; one shared mutated pose for render+measure, P0-1); **2b** `[sky]`→`[atmosphere]`
  (strict stale-key reject) + `make_celestial(cfg,R)` throws on `sun_distance<30*R` (`>=`, boundary +
  mutation pinned); **2c** `render::frustum_corner_rays` (pinned vs an INDEPENDENT glm::frustum oracle, not
  project_dir — P0-2 normalize-before-interp caught), single-source `kSkyGLSL` (monochrome day↔dusk↔night +
  exp-shell haze) in BOTH the sky FS and planet FS aerial perspective (no horizon fork), frozen
  `sun_dir(cel,0)`, day-epoch pin. **★ Fable AFTER-red-team DONE (2026-07-08, separate-context, cross-model): verdict
  SOUND-WITH-FIXES — load-bearing math PROVEN correct by algebra:** frustum-exact under lens-shift+zoom
  (the unshifted-±1 quad paired with shift-folded U-extents inverts `off_center_frustum` exactly; w=1 ⇒
  linear interp reproduces the affine ray; per-pixel normalize can't NaN, |V|²≥1), the shared-`kSkyGLSL`
  horizon CANNOT fork (same dir/up/alt at the limb), degenerate seeds MATCH `project_dir`, seam/clock/NaN/
  float32 clean. **Two P1s — both "mechanism silently nulled", NOT math errors, both ROUTED TO CHAD'S OWED
  FLY (not blocking Stage 3):** (F1) the monochrome day sky self-nulls its zenith→horizon gradient whenever
  `base` saturates to `uSkyDay` (`haze·(uSkyDay−base)≡0` in full daylight ⇒ flat silver day + dither) — the
  header "day gradient" overclaims for Stage 2; that view-angle day variation ARRIVES with the Stage-3 haze-
  weighted Rayleigh term, so no Stage-2 shader change (fly-judge, or a 1-line `haze·(1−base)` iff Chad wants a
  monochrome day gradient now — a retune). (F2) `horizon_definition` is loaded+range-checked but consumed by
  nothing — it is the parked bound for the DEFERRED horizon-continuity column-check (below); its substance
  (limb step ~0.3 ≫ 0.08, haze too weak to hide the limb) IS the `haze_density_light` fly-tune already owed.
  P2 (Fable deferred): loader admits a dusk band not straddling 0 → GLSL `smoothstep(edge0>edge1)` UB; harden
  `check(dusk_lo<0 && dusk_hi>0)` when next touching the loader (shipped −8/+6 is fine).
  **UPDATE (2026-07-08 #3 — Chad flew it): F1 RESOLVED, not a Stage-3 deferral.** The flat 0.82 silver
  dome read as "haze/grey everywhere"; Chad ruled the sky **SPACE-FIRST** — near-black zenith (celestial
  reads against space), a THIN silver rim at the horizon, "clear to see space." `kSkyGLSL` `sky_luminance`
  rewritten to `mix(sky_space, band, bandW)` with the rim ANCHORED TO THE LIMB: `bandW = 1 − smoothstep(0,
  sky_band_top, eView − uHorizonElev)`, `uHorizonElev = −acos(R/|eye|)` the horizon dip (a level-referenced
  band read "half the sky" at 2 km — the limb sinks ~28° below level). Knobs `sky_space=0.02` +
  `sky_band_top_deg=4.0` (rim THICKNESS above the limb); ladder extended `space ≤ night ≤ dusk ≤ day`
  (loader + mutation test); smoke-confirmed. Memory `[[sky-space-first]]`. **NEXT: Chad re-flies + dials**
  (`sky_band_top_deg` thickness / `sky_space` darkness) + F2's limb (haze now a `0.03` whisper). Deferred: an
  automated horizon-continuity column-check (no `--horizon` sampler yet — `horizon_definition` its bound).
- **Stage 2 ★ — Sky pass + GROUND HAZE at a FROZEN epoch** (supersedes old ladder step 1). Fullscreen sky,
  corner rays from shared `FrustumBounds`, single-source `kSkyGLSL` (day gradient↔dusk↔night + exp-shell
  haze + envelope thin-out + dither); **the planet FS CALLS the SAME `kSkyGLSL` aerial perspective (add
  `eyePos` uniform + `fragPos` varying) in this SAME increment** (P1-5); sun frozen (no motion yet);
  `[sky]`→`[atmosphere]` migration + doc edits. *Gate:* headless corner-ray↔`project_dir` round-trip;
  **horizon-continuity check** (screenshot column across the limb, step ≤ config); probe day baseline;
  sky/world lock under zoom + lens shift. *Chad flies:* the altitude ladder (deck haze → stars-out).
- **Stage 3 — Sun disc + moving sun + terminator + weather/scattering.**
  **SUB-INCREMENT 3a LANDED (2026-07-08, `179b0933`, gate 314/314; Chad-ruled to go FIRST as the
  base-independent visual, so the scatter's `kSkyGLSL` edit waits for Chad's space-first sky fly):**
  MOVING sun (`info.sun_dir = -sun_dir(cel, t_cel)`, `t_cel = tick_count·sim_dt`, no clock in render)
  + eye-relative sun DISC (sky-FS-local `sun_disc()`, fwidth core + glare halo, uniforms from existing
  `[celestial]`, eye-relative `sunDir` fed to sky+planet so the limb can't fork). ★ Fable red-team
  SOUND (0 P0 / 0 P1 / 4 cosmetic P2). Chad flew + tuned (sun 1.25, disc 2x, halo REMOVED — the halo
  is now the deferred haze-gated Mie scatter; rim thinner+more-transparent: sky_band_top_deg 3.0,
  sky_day 0.60).
  **SUB-INCREMENT 3b LANDED (2026-07-08, `5fc3d42c`, gate 315/315):** the planet FS day/night
  terminator — the bare `0.38 + 0.75*ndl` DIES, replaced by `albedo*(uNightFill + uGroundDayGain*ndl)`.
  Night side falls to night_fill_min=0.10 (real dark floor, not flat grey — the "dive into night to
  break contact" feature); day = albedo*~1.0 (full B&W detail, no blowout). New `[atmosphere]
  ground_day_gain=0.90` (decoupled from disc sun_intensity) + strict [0,2] loader + test leg.
  Smoke-confirmed day + forced-night. Chad-flown + tuned live (config-only): a **debug time-scrub**
  (hold `]`/`[`, seam-safe render-only `cel_time_offset`) + a faster cycle + brightness/size dials —
  live values in `docs/orchestrate_handoff.md` "CURRENT TUNED DAY/NIGHT KNOBS" (day 5-min, night 0.05,
  ground_day_gain 1.60, disc 1.40°/int 2.0). OPEN: a sun bloom/glow (Chad offered; the LDR disc
  saturates white so sun_intensity is headroom-only) — awaits his verdict.
  **SUB-INCREMENT 3c — the WEATHER VARIABLE — LANDED (2026-07-08, gate 324/324, zero moved goldens,
  Fable red-teamed SOUND):** pure `render::weather_haze(t_cel)->haze[0,1]` (new `render/weather.{h,cpp}`),
  three incommensurate-prime-period sines + a smoothstep gate → ~70/25/5 clear/haze/overcast (measured
  69.3/25.5/5.2). Wired value-only into the existing `uHazeDensity` (`atm.haze_density =
  weather_haze(t_cel)*haze_overcast_density`), same tick-derived `t_cel` as the sun. Retired the always-on
  `haze_density_light=0.03` placeholder → `[atmosphere] haze_overcast_density=0.90` + a new `[weather]`
  block. Chad ruled the dynamic-arena tempo (fastest front ~197 s); spawn reads clear. Red-team fixes: the
  slew test's LOWER + UPPER bounds are params-derived (kills the drop-fast-sine mutant + the AT-15
  hard-constant trap); a `test_load_world` leg locks `[weather]`==`WeatherParams{}` defaults. OWED: Chad
  flies (scrub `]`/`[`). **Remainder of Stage 3 (haze-gated scatter [Rayleigh/Mie, the sun halo returns
  here] + dusk color blend) is UNSTARTED** — the scatter is the first real Gemini call (verbose GLSL),
  riding this weather gate. Eye-relative disc (fwidth+glare);
  `sunDir` time-varying; planet FS `sunIntensity`+night floor (the `0.38` bare number dies); dusk blend.
  **Weather variable** (clear/haze/overcast, a slow deterministic function of `t_cel`) + the **haze-gated
  scattering** in `kSkyGLSL` (Rayleigh blue high-sun, Mie red/orange sunset + sun halo; scaled by the haze
  amount so clear = dark starry). **RULING (Chad 2026-07-08, was lost at the code layer — [[haze-rare]]):
  CLEAR must DOMINATE — the default sky is clean silver day / dark-starry night; haze + scatter are RARE
  (~0.70 clear / 0.25 haze / 0.05 overcast). The Stage-2 `haze_density_light=0.35` ships as an ALWAYS-ON
  constant (no weather gate yet) — that permanent haze is the placeholder this stage REPLACES; until the
  gate lands, the default must be set to CLEAR so the build reads right, NOT a standing grey wash.** *Gate:*
  probe dusk + night(no-moon) contrast ≥ floor; disc↔sparkle
  alignment; **frame-pacing determinism (AT-9-shaped: same tick schedule at 30 vs 240 fps ⇒ bit-identical
  celestial dump** — a bare same-input⇒same-output pins nothing); weather is a pure `t_cel` function (no
  wall-clock). *Chad flies:* a dusk chase (a hazy red sunset); outrun the terminator once.
- **Stage 4 — Stars, constellations, Polaris + southern asterism.** Star quads; authored catalog +
  integer-index hashed filler; extinction fade; glare suppression. *Gate:* catalog transform pin
  (RA/Dec→world, Polaris==â); probe night cell unchanged. *Chad flies:* HUD off, navigate home by
  Dippers→Polaris (the mission's star turn).
- **Stage 5 ★ — Moon + moonlight (+ water later, reflecting all of it).** Moon disc + honest phase;
  `moonDir/moonIntensity` second light; `night_glint_boost` on the plane when that shader lands. *Gate:*
  probe night(full-moon) + night(new-moon⇒floor holds); **sky>ground luminance assertion at night**.
  *Chad flies:* THE moonlit fight (the P0 mock for real).
- **Stage 6 — Seasons palette, statically A/B'd.** (RGBA/landmask bake already landed in the interleave.)
  Adds the **R8 height cubemap** + snow function (patchy noise + height bias) + lake-ice lerp + per-season
  exposure; **season interpolation + snow drive are CIRCULAR** (functions of a periodic mod-1 signal — no
  year-wrap sawtooth); debug `season_lock`.
  *Gate:* probe **full matrix** (4 seasons × {noon, dusk, full-moon, no-moon}) — plane/lake-vs-land/
  horizon floors all cells. *Chad flies:* summer vs winter A/B; the winter dogfight.
- **Stage 7 — Unlock the live year.** Remove `season_lock`; `lake_freeze_lag_frac`; `epoch_mode=persist`
  (off for probe); deterministic debug `time_scale`. *Gate:* time-scaled full-year soak — 12 waypoints,
  no cell below floor, no palette discontinuity. *Chad flies:* a long session across a season boundary —
  does it feel *alive*.
- **Stage 8 — Polish (each its own knob-flight):** south-sky refinement; north-pole cairn (rides the P3
  props pipeline); HUD night-dim if reported; eclipse revisit; sparkle-under-moving-sun re-verify.

## Standing rules (all stages)
Celestial time never feeds `sim/`/`control/`/drone (greppable); all celestial math CPU-double in the pure
module, shaders get vectors; one `kSkyGLSL` string across sky+planet+water; every probe floor is a config
number next to the knob it guards (a retune moves the guard — the AT-15 calibrated-to-today's-table
lesson); a moved probe baseline/golden HALTS the loop — confirm intent, never re-baseline to pass; author
≠ red-teamer across models at each ★.

## Verification
Per stage: build → shaders compile GLSL 330 → seam grep (no `sim/`/`control/` write; no
`lat|lon|heading`/fixed-axis/`GetTime`; no smoothing on an aim/rotation basis) → target-visibility probe
across the relevant season×time cells → **flight `ctest` green** → separate-context red-team at ★ → Chad
flies (one knob/flight). The two Stage-0 mock sorties are the cheapest place to catch the only risk big
enough to bend the plan — fly them first.
