# S-airdome — the atmosphere lives ONLY in the faction bubbles

Branch: `sandbox/bubble-atmosphere` (off `sandbox/kernel-v5-reconcile`).
Chad's ruling, 2026-08-09. Render-layer + one config/app flip. **NEVER touch `sim/` or
`control/`** (the kernel firewall). This spec is the contract; deviations must be reported,
not silently taken.

## 0. Chad's words (the spec of record)

> "I want to turn on the atmospheric haze at all times, but for that atmospheric haze to be
> constrained to the opposing factions' bubbles for the game loop. Lets turn off the escape sky
> for the game loop, and just have the blue haze and mie scatter during the day within the
> bubble and the only place that gets haze or any mie scatter is the bubbles. Make it so that the
> atmosphere is easily perceptible and beautiful blue light scatter and some white scatter. The
> sky can now have some color is my ruling, but what will be space-like is the locations with
> only a 200m deck of atmosphere and no bubble, it will look like space. Anywhere in the bubble
> zone looks like earth's atmosphere, and that zone is dynamic during gameplay."

Reading, item by item:

1. **Haze always on** — the weather roll no longer gates whether there is atmosphere. Air is
   always present *where the air field says there is air*.
2. **Constrained to the bubbles** — the visual atmosphere is a function of the SAME
   `sim::AtmosphereField` the plant flies (deck ∪ the two faction ellipses ∪ tunnel), not of a
   global scalar.
3. **Escape sky off for the game loop** — the `[gravity]` field must not be force-enabled by
   `[conquest] enabled`.
4. **Blue Rayleigh + white Mie inside, by day.** Beautiful and *easily perceptible*.
5. **The space-first ruling is RELAXED — inside a bubble only.** Outside (the 200 m deck-only
   world and the vacuum gap) the near-black space sky STANDS, unchanged.
6. **Dynamic** — the domes grow/shrink with conquest, live, with no extra plumbing.

## 1. The mechanism

### 1.1 One air field, three consumers, zero forks

The visual atmosphere amount at a world point is the **spatial** factor of
`sim::atm_frac_at` — the deck/bubble complement-product union — WITHOUT the altitude taper
`atm_frac(alt)` and WITHOUT the tunnel term (a mine interior has no sky).

Because a GLSL copy of that math is an H1 fork by construction, it is fenced two ways:

- **`render/air_field.h`** — a NEW pure, raylib-free, header-only CPU reference:
  `render::air_at(const glm::dvec3& pos, const AirField& f)` returning `[0,1]`. `AirField` is a
  plain render-side mirror struct (deck_agl_m, deck_soft_m, planet_R, up to
  `kMaxAirBubbles = 4` bubbles: center_dir, major_axis, a, b, ceiling_m, edge_soft_m,
  ceil_soft_m).
- **`test/unit/test_air_field.cpp`** — pins `render::air_at` against the LIVE
  `sim::atm_frac_at` (divided by `sim::atm_frac(alt)`, with the tunnel pointer null) over a
  scatter of ≥200 sample points spanning: bubble cores, both soft edges, above/below both
  ceilings, the vacuum gap between the ellipses, the deck, high vacuum, and the antipode.
  Tolerance 1e-9 (both double). **Mutation-verify**: flipping the ellipse `a`/`b`, dropping the
  deck term, or dropping the union complement each must FAIL the test.
- **`render/air_field_glsl.cpp`** — `kAirFieldGLSL`, a line-by-line float transliteration of
  `render::air_at`. Add a companion leg in `test_air_field.cpp` that pins the two side by side
  *in structure* is not possible without a GL context; instead pin what CAN be pinned: a
  headless SOURCE validator leg (the rig-A precedent) asserting the GLSL string contains the
  ellipse `r_eff` denominator form and the complement-product `1.0 - ...` union, and keep the
  two files adjacent with a load-bearing comment on both. State this coverage limit honestly in
  the report — do not claim the shader is pinned.

`kAirFieldUniformsGLSL` declares the uniform block; `render::air_field_locs(Shader)` looks up
the locations into an `AirFieldLocs` struct; `render::set_air_field_uniforms(shader, locs,
field)` sets them. Call all three from EVERY shader that already calls
`set_atmosphere_uniforms` — `render/sky.cpp`, `render/planet.cpp`, `render/stars.cpp` — so the
sky, the ground limb, the water, and the stars can never disagree about where the air is.
Do NOT widen the existing `set_atmosphere_uniforms` signature (it is already 16 args); add the
new call beside it.

### 1.2 Optical depth along the ray — this is what makes the dome VISIBLE

`kAirFieldGLSL` also provides

```
float air_optical_depth(vec3 eyePos, vec3 dir, float maxDist, int steps)
```

a uniform-step trapezoid march of `air_at()` along the ray, returning
`sum(air * ds) / uAirTauScaleM` — a dimensionless optical depth, `uAirTauScaleM` being the
reference path length at which unit air reads as one optical depth (config
`atmosphere.tau_scale_m`, start at 12000).

`maxDist` for the sky pass = the distance to the planet-sphere intersection along `dir` if the
ray hits (it will not for above-horizon rays; the planet mesh occludes below-horizon sky
anyway), else `uAirMarchMaxM` (config `atmosphere.march_max_m`, start 90000 — long enough to
cross a whole 35 km dome and see the far one). Steps = `uAirStepsSky` (config
`atmosphere.march_steps_sky`, start 14).

The atmosphere amount that everything keys on is then

```
float airAmt = 1.0 - exp(-tau);      // [0,1), saturating inside a dome
```

**Why the march and not just the air at the eye:** it is the whole quality ask. It gives, for
free and correctly: a blue dome you can SEE from the outside and fly into; a limb that thickens
as you look along a chord of the dome; a vacuum gap that reads black even at 500 m; the far
faction's dome sitting on the horizon as a blue lens; and a dome that visibly grows when a pump
falls. No special-casing.

### 1.3 The sky (`render/sky.cpp`, `kSkyGLSL`)

`sky_luminance` and `sky_color` take the ray's `airAmt` as a new parameter (thread it — do not
read a global). Changes:

- The **space-first band** (`bandW`) is now **multiplied by `airAmt`**. Zero air ⇒ the silver
  rim disappears entirely and the sky is `uSkySpace` + stars: that IS the "looks like space"
  requirement, and it is structural, not a tuned coincidence.
- The **haze lift** uses `airAmt * uAirHazeDensity` in place of the old
  `uHazeDensity * exp(-alt/uHazeScale)` exp-shell. The exp-shell was a global stand-in for
  exactly the vertical structure the bubble ceiling now provides honestly; drop it in the sky
  path. (`uHazeScale` stays for the ground aerial fallback and for the weather term.)
- The **night airglow floor** `uNightFill` is also `* airAmt` — no airglow in vacuum.

### 1.4 The scatter (`render/scatter_glsl.cpp`) — the beauty budget

Chad relaxed space-first *inside the bubble*, so the Rayleigh term must stop being a rim-only
whisper. Rewrite `scatter()`:

- Signature gains `float airAmt` (replacing `hazeAmount` as the master gate).
- **Rayleigh**: a real sky-blue with the proper phase function
  `(3/16π)(1 + cos²θ)` about the sun, `× u_rayleigh × rayW × airAmt`, where
  `rayW = smoothstep(-0.05, 0.30, sunElevCos)` (day only; zero-gated below the horizon so night
  in a bubble stays dark and starry-through-thin-air, never blue). **Remove** the `horizonW`
  rim weighting — the whole point of the ruling is that the zenith inside a dome is BLUE. Keep
  a mild zenith-to-horizon *density* gradient instead, driven by `airAmt` itself (which already
  rises along horizon-grazing chords) — do not re-add a hand-authored one.
- **Mie / the white scatter**: two tints, blended by sun elevation —
  `u_mie_day` (near-white silver, e.g. `[0.92, 0.94, 0.98]`) blended to the existing
  `u_mie` orange as the sun drops (`smoothstep(0.30, 0.02, sunElevCos)`). Henyey-Greenstein
  forward halo about the sun + a broad low-elevation lift, both `× airAmt`. This is the
  "some white scatter" — the milky forward haze that makes air read as air.
- Total scatter still `× u_scatterStrength`. `airAmt == 0 ⇒ exactly vec3(0)`: assert this in
  the code by construction (the final multiply), so vacuum can never take on color.

Target look, judged on the screenshots: inside a bubble at 2 km at midday, the sky above reads
as a clearly blue daytime sky with a brighter, whiter band toward the sun and the horizon;
crossing the hard edge outward, that blue drains to black over the ~1.2 km soft edge with the
stars coming back; looking back, there is a legible luminous blue dome sitting over the enemy
valley.

### 1.5 The ground aerial (`sky_aerial`, planet FS)

Extinction along the eye→fragment segment uses the SAME march at `uAirStepsGround`
(config, start 6) with `maxDist = dist`. Replace the `uHazeDensity * exp(-alt/uHazeScale)`
extinction with `1 - exp(-tau_seg * uAirAerialGain)`. Result: ground inside a dome gets honest
blue aerial perspective that deepens with distance; ground outside stays crisp and airless,
which is exactly the "little planet in vacuum" read Chad already approved for the deck world.

### 1.6 The stars (`render/stars.cpp`, `kStarFsMain`)

Stars currently dim by `uHazeDensity`. Switch to the same `airAmt` on the star ray: inside a
sunlit dome the sky washes them out; in the vacuum gap they are brilliant at any altitude;
inside a dome at night they are visible but slightly veiled. Same for the moon disc's haze gate
in `sky.cpp` and the aurora's `clear` factor.

### 1.7 Weather, after the change

Weather is no longer the on/off gate for atmosphere; it becomes a **local thickening of air
that already exists**. Per fragment:

```
density = uAirHazeDensity + uWeatherHaze * uAirWeatherGain
```

with `uWeatherHaze` still the existing W2a `weather_cell(eye_track, t_cel)` scalar the app
computes (unchanged — one scalar per frame, no per-fragment weather; W2b is out of scope). The
whole density is multiplied by `airAmt` downstream, so a squall over the vacuum gap is
invisible, which is correct. The `\` force-haze debug key keeps working (it drives
`uWeatherHaze` to 1). `]`/`[` still scrub the sun and weather.

**Flagged assumption** (Chad to overrule if wrong): weather survives as an additive thickener
inside the domes rather than being deleted. His words removed weather's power to *gate*
atmosphere, not the microsystems themselves.

### 1.8 Escape sky off for the game loop

`app/main.cpp` ~line 1153: `if (game.gravity.enabled || game.conquest.enabled)` →
`if (game.gravity.enabled)`. Update the comment above it to record the 2026-08-09 ruling
(supersedes "CONQUEST force-enables the escape-sky taper at startup (spec §1)"). `[gravity]
enabled` stays `false` in `config/game.toml`; the `T` debug toggle and the bezel/HUD "ESCAPE
SKY" plates stay exactly as they are. Check `test/unit/` for a leg that pins the conquest
force-enable — if one exists, it must be updated to pin the NEW ruling, never deleted.

### 1.9 Wiring the live field (the dynamic requirement)

`app/main.cpp` builds `render::AirField` **each frame** from the live `sim::AtmosphereField`
that `rebuild_conquest_bubbles` writes (and sets `enabled=false`/zero bubbles when `env.atm ==
nullptr`, e.g. the `B` key toggle) — copy, never re-derive. That single copy makes growth,
shrinkage, and a faction's extinction (radius → 0) show up in the sky with no extra code, and
makes it impossible for the visual dome to disagree with the flyable one.

## 2. Config (`config/world.toml [atmosphere]`, new keys, all loader-validated)

```
air_haze_density   = 0.85   # always-on in-air haze density (was weather-gated)
air_weather_gain   = 0.55   # how much a weather cell thickens air on top
tau_scale_m        = 12000  # path length of unit air = 1 optical depth
march_max_m        = 90000  # sky ray march cap
march_steps_sky    = 14
march_steps_ground = 6
aerial_gain        = 1.0
mie_tint_day       = [0.92, 0.94, 0.98]   # the WHITE scatter
```

Keep every existing key. `scatter_strength`, `mie_g`, `rayleigh_tint`, `mie_tint` stay as the
fly-dials. Range-check the new ones in `config/load_world.cpp` in the house style (steps ≥ 2
and ≤ 32, tau_scale_m > 0, gains ≥ 0, tints in [0,4]).

## 3. Firewall + gate

- No edit under `sim/` or `control/`. No golden may move — the change is render + one app
  branch that only changes which pointer `env.grav` gets at startup. If a controller/kernel
  golden moves, **STOP** and report.
- `.claude/hooks/gate.sh` (build + full ctest) must be green, with the new test included.
- Regenerate the code graph in the same commit: `python tools/graph/graphify.py`.
- `clang-format` per CLAUDE.md before committing.

## 4. Evidence required (the gate is structurally blind to shaders)

Build `build-play` (RelWithDebInfo) and capture `--smoke` screenshots. Use
`SEADS_SPAWN_ALT`; if a spawn *direction* override is needed for the outside-the-bubble shots,
add one as a smoke-only env hook following the existing `SEADS_SMOKE_GEAR` / `SEADS_OBLIQUE`
pattern (`SEADS_SMOKE_SPAWN_DIR="x,y,z"`, ignored unless `smoke_frames > 0`) — that is
sanctioned by this spec.

Required shots, into `docs/airdome_shots/`:

| # | Where | What must be true |
|---|-------|-------------------|
| 1 | Inside VALLEY dome, 2 km, midday | Blue sky, white forward-scatter toward the sun, blue aerial on the far terrain |
| 2 | Same spot, midday, `\` force-haze | Visibly thicker/milkier, still blue — not white-out |
| 3 | Vacuum gap between the domes, 3 km, midday | Near-black sky, stars visible, terrain crisp, thin ground-hugging veil from the 200 m deck only |
| 4 | 12 km up, both domes in frame, midday | Two legible luminous blue lenses over the two territories, black between them |
| 5 | Inside a dome at dusk | Warm orange Mie low, blue aloft, no black-vs-blue seam artefact |
| 6 | Inside a dome at night | Dark, faint airglow, stars slightly veiled — NOT blue |
| 7 | Vacuum gap at night | Brilliant stars, no airglow at all |
| 8 | Crossing the soft edge outbound (2 frames, ~1 km apart) | The blue drains smoothly — no banding, no pop |

Also report `--smoke` per-frame ms in `build-play` **before and after** the change at shot-1's
viewpoint (the march is the only new per-pixel cost; if it exceeds ~1.5 ms, drop
`march_steps_sky` and say so).

## 5. Traps this codebase has already paid for — read before coding

- The green gate never runs `seads.exe`: a shader that fails to compile ships green. Check the
  `SKY:`/`PLANET:`/`STARS:` TraceLog lines in every smoke run and paste them into the report.
- A dead uniform reads 0. If `air_at()` silently returns 0 everywhere the sky goes black and
  looks *deliberate*. Prove the field is live with a signature test: temporarily force
  `airAmt = 1` and confirm the sky floods blue everywhere, then revert. Report that you did it.
- `up = normalize(worldPos)` everywhere — no fixed world axis, no cached up. Any new frame math
  must be recomputed per sample point.
- Do not add a smoothing/lag to anything that feeds mouse→aim. Nothing here should go near
  `input/` or the aim frame at all.
- Every gate must be hysteretic or a smooth fade — no bare booleans on a dwelling signal.
- Do not re-record a golden. If one moves, stop.

## 6. Deliverable

A single clean commit on `sandbox/bubble-atmosphere` plus a report covering: what landed, the
gate result (N/N), the mutation-verify results, the screenshots with your own honest judgement
of each against the table above, the perf numbers, every deviation from this spec and why, and
an explicit list of what is NOT covered by a test.
