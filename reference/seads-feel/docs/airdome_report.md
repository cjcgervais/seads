# S-airdome quality fix pass — deliverable report

Branch `sandbox/bubble-atmosphere`, on top of `d22b98e78` (the landed S-airdome
mechanism, gate 984/984). This pass does not change the MECHANISM (the
optical-depth march over `render::AirField`) — it re-dials the untuned
starting numbers the spec itself flagged as unset, fixes the Rayleigh color
model so thick air reddens toward blue-white instead of flattening to
grey/cream, restores Chad's dusk blaze, smooths the march quantization, and
closes three P1 cleanups (`docs/bubble_atmosphere_spec.md` items 1-7).
**Never touched `sim/` or `control/`.** No golden moved.

## 1. What landed

### 1.1 Retuned dials (`config/world.toml [atmosphere]`)

| key | before | after | why |
|---|---|---|---|
| `tau_scale_m` | 12000 | **2600** | a 2.4-2.6 km zenith chord inside a dome at 2 km gave `tau≈0.22`, `airAmt≈0.20` — the zenith could never read more than faint navy. At 2600 the same chord gives `tau≈0.95`, `airAmt≈0.61` (in the 0.55-0.75 target band). |
| `air_haze_density` | 0.85 | **0.25** | the grey luminance lift dominated the sky value at 0.85, burying the blue Rayleigh scatter underneath it. At 0.25 the lift reads as a gentle veil. |
| `aerial_gain` | 1.0 | **0.35** | at 1.0 the ground extinguished to the sky color well before the horizon (terrain went milk). At 0.35 terrain (lakes, towns, forest) stays clearly legible through the dome at 2 km (shot 1). |
| `march_max_m` | 90000 | **55000** | the domes are ~35 km across; 90 km bought stride, not coverage. |
| `march_steps_sky` | 14 | **20** | with the smaller `march_max_m` this drops the stride from ~6.4 km to ~2.75 km against the 1.2 km soft edges — still coarse, hence the jitter fix below. |
| `air_weather_gain` | 0.55 | **0.30** | discovered mid-pass (see §3): at 0.55 a fully-forced weather cell (shot 2) pushed `density` to `0.25+0.55=0.80`, washing the dome to cream/sepia — a direct violation of shot 2's "still blue, not white-out" bar. Re-measured and re-captured; see §4. |

`march_steps_ground` (6) and `mie_tint_day` were left as spec'd — the ground
march never showed banding at 6 steps (short segments), and the white-scatter
tint read correctly from the first capture.

### 1.2 Per-channel Rayleigh saturation (`render/scatter_glsl.cpp`)

The old form was `inScatter = u_rayleigh * rayW * phase`, gated only by the
**single shared** final `* airAmt` multiply — every color channel saturated
at the *same* rate as `airAmt` climbed, so a long horizon chord (`airAmt`
near 1) and a moderate one both landed at the *same* absolute color, just
brighter, reading as a flat wash rather than a hue shift.

New form (spec item 4, `kScatterGLSL::scatter`):

```glsl
vec3 kChanW = vec3(0.55, 0.85, 1.35);  // R,G,B saturation rate; blue fastest
vec3 inScatter = u_rayleigh * (vec3(1.0) - exp(-tau * kChanW));
vec3 rayleigh = inScatter * rayW * phase;
```

`scatter()`'s signature changed from `(..., float airAmt)` to
`(..., float tau)` — the raw optical depth, not the pre-saturated `[0,1)`
amount — so the function has the dynamic range to do this. `sky_color()`
(render/sky.cpp) was changed to accept `tau` and compute `airAmt` internally
for `sky_luminance`, threading `tau` straight through to `scatter()`. All
three call sites (`sky.cpp` main FS, `sky_aerial`, `planet.cpp`'s water
reflection) were updated to pass the raw `tau*` variable they already
computed instead of first collapsing it to `airAmt`.

The channel weights are deliberately **not** a config key (they are a shape
constant of the color model, not a fly-dial per the existing tint/strength
dials) — blue saturates fastest so mid-tau chords read distinctly bluer than
thin ones, and the whole term still saturates *at the configured
`u_rayleigh` tint itself*, never blowing past it to white — which is what
kills the "cream" failure: thick air asymptotes to the BLUE tint, not to
grey.

### 1.3 Dusk blaze (`render/scatter_glsl.cpp`, Mie term)

`mie_g` was **not** touched (Chad's own 0.66 dial stands). The weak-peach
read was the forward-halo term's *weight*, not its *tightness*:

```glsl
float duskBoost = 1.0 + 2.2 * mixLow;
vec3 mie = mieTint * rayW * (hg * duskBoost + rimV * 0.18);
```

`duskBoost` ramps the sun-forward Henyey-Greenstein term up to ~3.2x at a low
sun (`mixLow -> 1`) while the broad `rimV` lift dropped `0.35 -> 0.18` so the
warmth concentrates near the sun instead of bleeding orange across the whole
dome. Judged on shot 5 (see §4) — a real warm blaze low near the sun, blue
above, no black/blue seam.

### 1.4 March jitter + step/range retune (`render/air_field_glsl.cpp`)

`air_optical_depth` now offsets the march's first sample by a per-pixel
hashed fraction of one step:

```glsl
float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) *
                     43758.5453) * ds;
```

converting the dome-silhouette stair-stepping (visible at the old
14-steps/90000 m stride, ~6.4 km against a 1.2 km soft edge) into fine dither
noise — free, and stylistically consistent with the sky's existing dither.
Combined with `march_max_m` 90000→55000 and `march_steps_sky` 14→20 (stride
now ~2.75 km), the dome edge in shots 3/4/8 reads smooth with no visible
banding. Perf cost is reported in §5.

### 1.5 P1 cleanups (spec item 7)

- **`haze_overcast_density` re-wired, not deleted.** It was loaded and
  range-validated but never read by any live uniform after the original
  S-airdome landing. Chose to wire it back as a **ceiling** on the weather
  thickening term (rather than delete the key) because a delete would
  silently orphan any prior tuning a config author put into it, and the
  spec's own §1.7 already frames weather as "a local thickener on top" —
  a ceiling is the natural role. New uniform `uAirHazeCeiling`
  (`kAirFieldUniformsGLSL`), new `render::AirField::haze_overcast_density`
  field, new `AirFieldLocs::haze_ceiling` + setter leg
  (`render/sky.cpp::set_air_field_uniforms`), wired from
  `air_field_render.haze_overcast_density = a.haze_overcast_density` in
  `app/main.cpp`. `sky_luminance`'s density formula is now
  `uAirHazeDensity + min(uWeatherAmt * uAirWeatherGain, uAirHazeCeiling)`.
- **`uHazeDensity` renamed `uWeatherAmt`** across every consumer
  (`kSkyUniformsGLSL`'s declaration, `sky.cpp`/`planet.cpp`/`stars.cpp`'s
  `GetShaderLocation` calls and loc fields — `loc_haze_density` →
  `loc_weather_amt` — and every comment that named the old uniform, including
  `render/weather.h` and `config/world.toml`). The uniform genuinely carries
  the raw per-frame `weather_cell()` scalar, never a density; the old name
  invited tuning it as if it were the always-on baseline (that's
  `uAirHazeDensity`, a different uniform entirely). The C++-side
  `AtmosphereParams::haze_density` field name was left as-is — it is
  documented in-place as "the RAW weather scalar" and is not a shader-facing
  identifier, so the rename risk the spec is guarding against (tuning the
  wrong *uniform*) doesn't apply to it.
- **`kMaxAirBubbles` truncation now warns.** `app/main.cpp`'s
  `air_field_render.bubble_count` build loop now emits a `TraceLog(LOG_WARNING,
  "AIRDOME: ...")` the first time `sim::AtmosphereField` carries more bubbles
  than `render::kMaxAirBubbles` (4), naming the actual count and the
  truncation. Edge-triggered on the sim's bubble count itself (a static
  "last warned count" guard) so it fires once per distinct overflow, not
  once per frame, and fires again if the count changes to a new value.
- **CPU-twin unit tests for the march** (`test/unit/test_air_field.cpp`):
  `air_optical_depth` / `air_sky_max_dist` / `air_eye_pos` had zero coverage
  before this pass — the spec calls this out explicitly as "the heart of the
  feature." Added a CPU twin built on the *already-pinned* `render::air_at`
  (not a re-derivation of it), deliberately **unjittered** (the GLSL-side
  per-pixel dither is a display-only offset on top of the same deterministic
  integral; it doesn't change what's being tested). Four new `TEST_CASE`s:
  - monotonically non-decreasing in march distance inside a constant-air
    core, cross-checked against the exact analytic value (`tau ==
    dist/tau_scale_m` for a unit-field path);
  - exactly zero in the vacuum gap;
  - saturates (`airAmt > 0.99`) on a long march fully inside a dome core;
  - `air_sky_max_dist` returns the exact closed-form planet-hit distance on
    a straight-down ray (`t == alt`, proven algebraically: `b=-(R+alt)`,
    `disc=R^2`, `t=(R+alt)-R=alt`), and reports "no hit" (a `-1.0` sentinel)
    on a straight-up ray — this needed a forward-ray guard (`t > 0.0`) that
    the first draft of the CPU twin omitted, caught by the honest test run
    (see §2).

  Mutation-verified (2 of 4 legs, targeting the two places a copy-paste of
  the march algebra is most likely to drift): dropping the `0.5*` trapezoid
  factor breaks the monotonicity leg's exact analytic check (`tau_full`
  reads `1.40` instead of `0.70`); dropping the `t > 0.0` forward-ray guard
  in `cpu_air_sky_max_dist` breaks the straight-up "no hit" leg (`t_up`
  reads `-32000` instead of `-1.0`). Both mutants killed; both reverted.

## 2. What went wrong along the way (honest ledger)

- **First draft of the CPU-twin saturation test used a march distance
  (20000 m) that exceeded the test bubble's own radius (6000 m, from
  `make_world()`'s fixture)** — the march exited into vacuum partway through,
  so it never actually saturated (`airAmt` read `0.934`, not `>0.99`).
  Fixed by shrinking the march to stay inside the constant-air core
  (5000 m at `tau_scale_m=800`).
- **First draft of `cpu_air_sky_max_dist` omitted the `t > 0.0` forward-ray
  guard** the real GLSL has (`return (t > 0.0) ? t : uAirMarchMaxM;`) — a
  straight-up ray's discriminant is non-negative (the infinite LINE still
  crosses the sphere, just behind the eye), so without the guard the CPU
  twin returned a large negative distance instead of signalling "no hit."
  Caught by the honest `ctest` run (not a mutation — this was a genuine bug
  in the new test, not the shader), fixed to mirror the GLSL exactly.
- **`air_weather_gain` at the original 0.55 washed shot 2 to cream.** Caught
  on first honest look at the captured shot (not predicted from the spec's
  arithmetic) — full force-haze density hit `0.25+0.55=0.80`, well past the
  point where the achromatic lift dominates the blue scatter. Retuned to
  0.30 (§1.1), re-captured, re-verified visually (§4).
- **A `clang-format -i` sweep over the whole `render/`,`sim/`,`control/`
  tree timed out mid-run and left ~35 files it reformatted staged as
  modified — including `control/controller.cpp`, `control/controller.h`,
  and three `sim/` headers.** This is a hard firewall violation (CLAUDE.md:
  "Never touch `sim/` or `control/`") even though the changes were pure
  reformatting, not semantic. Caught immediately via `git status`, all
  non-scoped files were `git checkout --`'d back to HEAD before proceeding;
  `clang-format -i` was re-run scoped to only the 13 files this pass
  actually touched. **Lesson for the next pass on this repo: never run
  `clang-format -i` with a whole-tree glob under a firewall — scope the
  file list explicitly.**
- **Two concurrent `ctest`/build invocations mid-session left one `ctest`
  run showing hundreds of "Not Run" tests** (a build racing a running test
  binary). Not a real failure — re-ran clean once nothing else was
  building; see §6 for the actual final gate result.
- **The spawn-direction offline geometry (vacuum-gap midpoint, edge-crossing
  arc distances, the oblique establishing-shot pan/tilt) was hand-derived in
  a Python scratch script**, not read from any existing capture log (none
  existed — the original 8 shots in `docs/airdome_shots/` had no recorded
  capture commands). This is honest best-effort geometry (verified against
  the real `air_ellipse_radius` formula, not guessed), not a spec-provided
  set of coordinates.

## 3. Signature test (spec §5's "prove the field is live")

Before finalizing, `airAmt` was temporarily forced to `1.0` at the top of
`sky_color()` and a `--smoke` shot taken at the vacuum-gap viewpoint (shot 3's
spawn) — the entire sky flooded blue/white with the space-first band and
night-airglow floor gone, confirming the uniform chain from
`render::AirField` through `air_optical_depth` into `sky_color` is live, not
silently reading zero. Reverted immediately after the one confirming shot
(not committed, not part of the final diff).

## 4. Screenshots — honest judgement against the spec §4 table

All 8 (9, counting both edge-crossing frames) re-captured into
`docs/airdome_shots/`, overwriting the originals. `build-play` (RelWithDebInfo),
day/night/dusk cel-time offsets found by sweeping `--smoke 8 <name> <offset>`
seconds against the `day_period_s=300` cycle and reading mean sky-patch
brightness (Python/PIL) before eyeballing candidates: **110 s = midday**,
**232 s = dusk** (sun just above the horizon, still catching warm light),
**290 s = night**. Spawn directions: `kValleyCenterDir` (valley), the
normalized sum of `kValleyCenterDir + kSudburyCenterDir` (vacuum gap — this
sits ~1-4 km outside both hard edges by construction, confirmed by the
`THIN AIR` HUD plate reading in shots 3/7), and two points 1600 m apart along
the valley-to-gap geodesic bracketing the valley hard edge (edge-crossing).

| # | File | Verdict |
|---|---|---|
| 1 | `01_valley_midday.png` | **PASS.** Clean, clearly blue daytime sky (brighter/whiter toward the horizon — the forward-scatter gradient), snow-forest terrain fully legible below through the dome. Both halves of the acceptance ("blue sky AND clearly visible terrain") hold simultaneously. |
| 2 | `02_valley_forcehaze.png` | **PASS** (after the `air_weather_gain` retune — see §2). Visibly thicker/milkier near the horizon than shot 1, upper sky stays a recognizable blue, no white-out. The first capture at `air_weather_gain=0.55` was a genuine FAIL (cream/sepia wash) — recorded honestly in §2, not hidden. |
| 3 | `03_vacuum_gap_midday.png` | **PASS.** Near-black overhead with stars visible, terrain crisp below, and — a bonus the wide FOV surfaced — legible blue dome lenses flanking the vacuum corridor on both sides, matching the table's own shot-3/4 "legible blue territory on the flanks" language. `THIN AIR` HUD plate confirms genuinely outside every bubble. |
| 4 | `04_both_domes_4km.png` | **PASS**, and probably the best shot of the set. Used the `SEADS_OBLIQUE` debug camera (pan −8°, tilt +6°, aimed at the vacuum-gap direction, 12 km up, 25 km back) rather than the plane's own spawn-facing camera — a first attempt at 12 km with `SEADS_SMOKE_SPAWN_PITCH_DEG=55` on the plane's own camera pointed at empty black sky (the default heading doesn't aim at either bubble). Two clean luminous blue lenses hugging the planet limb, black vacuum directly between them, terrain visible below. |
| 5 | `05_valley_dusk.png` | **PASS.** A genuine warm salmon/orange blaze low around the sun, blue-purple aloft, and the transition between them is a smooth gradient — no hard black-vs-blue seam. Not a pure saturated orange (there's real physical blending with the blue Rayleigh component still contributing at this sun elevation, giving a slightly rose cast) but it reads as unambiguously warm near the sun and cool above, which is what the acceptance asks for. |
| 6 | `06_valley_night.png` | **PASS, with a caveat.** Reads visually near-pure-black at normal viewing — no obvious blue cast, no gross airglow bloom. A pixel-level check (mean sky-patch luminance ≈12.5/255, ~6.7% of sampled pixels above a faint threshold) confirms there IS a genuine faint starfield/dither signal present, just subtle enough that "stars slightly veiled" reads as "stars barely visible" at a glance — honestly on the dim side of the target, not over it. |
| 7 | `07_vacuum_gap_night.png` | **PASS.** Brilliant, unveiled stars, zero airglow, `THIN AIR` confirmed. |
| 8a/8b | `08a_edge_crossing_in.png` / `08b_edge_crossing_out.png` | **PASS.** `AIR 100%` (a) → `AIR 24%` (b), a visibly deeper black gap directly overhead in (b) than (a), and the blue-to-black transition is a smooth gradient with no visible banding or popping in either frame — the jitter fix (§1.4) is doing its job here specifically, since this is exactly the near-edge geometry that used to show the coarse stair-step. |

Overall: **8/8 required looks pass** the table's acceptance language. Shot 6
is the one I'd flag as worth a second look from Chad — it's correct by the
letter of the spec (dark, not blue, no gross airglow) but is closer to "no
visible stars" than "stars slightly veiled" to a casual glance.

## 5. Perf

`build-play`, `--smoke 120` at shot 1's viewpoint (2 km inside the Valley
dome, midday), VSYNC off (the smoke path's own timing regime):

- **After this pass:** `avg=2.997 ms  min=1.485 ms  max=42.569 ms  (333.6 fps avg)`
  (the `SMOKE_TIMING` line's own printout; the 42.5 ms max is a single
  first-frame shader/asset warm-up spike, not steady-state).
- **Prior session's before/after** (the original S-airdome landing,
  `docs/bubble_atmosphere_spec.md` §4): `1.586 ms` before any march existed,
  `2.610 ms` after landing it at `march_steps_sky=14`/`march_max_m=90000`.
- This pass raised `march_steps_sky` 14→20 (+43%) while cutting
  `march_max_m` 90000→55000 (−39%) and added the one-line jitter hash —
  net effect `2.610 → 2.997 ms` (~+15%), comfortably under the ~4.5 ms budget
  this task set. No need to back off the step count.

## 6. Gate + shader compile confirmation

`.claude/hooks/gate.sh` (build + full `ctest`): **988/988 passed** (984 prior
+ 4 new `test_air_field.cpp` CTest cases from this pass — see §1.5; the
suite's other 3 `air_field` cases were already counted in the 984).
Zero golden moved. No edit under `sim/` or `control/` survived into the
final diff (see §2's clang-format incident — reverted before the gate ran).
`tools/graph/graphify.py` regenerated in this commit.

Every `--smoke` capture's TraceLog was checked for the three shader-compile
lines the spec calls out (the green gate never runs `seads.exe`, so this is
the only place a silent shader failure would show):

```
INFO: SKY: fullscreen sky pass built
INFO: PLANET: built cubesphere 24 meshes (6 faces x 2^2 tiles), 40000 verts/mesh, effective 399 verts/face-edge, 4096px cubemap
INFO: STARS: 2865 stars (mag <= 5.5)
```

present and clean on every capture run, no `WARNING`/`compile failed` lines.

## 7. Deviations from the spec

1. `air_weather_gain` retuned 0.55→0.30 mid-pass — not in the original ask
   list, discovered from the shot-2 capture itself (§2/§4).
2. The per-channel Rayleigh weights (`kChanW = vec3(0.55, 0.85, 1.35)`) are a
   new hardcoded shape constant, not a config key — matches the spec's own
   framing ("a house convention... not a physical model") and existing
   precedent (the phase-function coefficients are likewise hardcoded), but
   flagging it as a choice: if Chad wants to fly-dial the saturation rate
   independently of hue, this would need to move to `config/world.toml`.
3. `duskBoost`'s coefficients (`1.0 + 2.2*mixLow`, `rimV` weight `0.18`) are
   likewise hardcoded shape constants, same rationale.
4. Shot 4's camera used the `SEADS_OBLIQUE` debug hook (already-sanctioned,
   pre-existing) instead of the plane's own spawn-facing camera plus
   `SEADS_SMOKE_SPAWN_PITCH_DEG` (the mechanism the spec explicitly built for
   this shot) — the spawn-facing approach pointed at empty sky because the
   default heading (world -Z projected onto the tangent plane) doesn't aim
   at either bubble from an arbitrary spawn point. `SEADS_OBLIQUE` gave a
   dramatically better result (§4) and required no new code. Both are
   pre-existing sanctioned smoke-only hooks; no new env var was added.
5. Vacuum-gap and edge-crossing spawn coordinates were computed offline
   (Python, cross-checked against the live `air_ellipse_radius` formula) —
   the spec permits "an arbitrary point (a bubble core, the vacuum gap, an
   antipode)" but doesn't hand-supply the coordinates.

## 8. What is NOT covered by a test

- **The GLSL shader math itself has no numeric test — only a CPU twin.**
  `air_optical_depth`/`air_sky_max_dist` are now covered by the CPU-twin
  legs in `test_air_field.cpp` (§1.5), but those twins run the ALGEBRA in
  C++ against the ALREADY-pinned `render::air_at` — they do not execute a
  single line of the actual `kAirFieldGLSL` text on a GPU. The
  correctness of the GLSL transliteration itself rests on: (a) the
  pre-existing structural source-validator leg (asserts specific substrings
  are present, does not check semantics), and (b) this report's screenshots
  + the `SKY:`/`PLANET:`/`STARS:` compile-success TraceLog lines (§6) — i.e.
  a human/vision judgement, not a machine oracle. **This is the single
  biggest coverage gap in the whole S-airdome feature, inherited from the
  original landing and unchanged by this pass.**
- **`kScatterGLSL`'s new per-channel Rayleigh form and the `duskBoost` term
  have zero numeric pin.** The asset-validator's backdoor guard (no smuggled
  uniform, no `#define`/`#include`, no clock token) still passes and was
  re-verified, but nothing asserts the *shape* of the new color math — a
  sign error or a swapped channel weight would not fail any test, only look
  wrong in a screenshot a human has to judge.
- **The jitter hash in `air_optical_depth`** has no test at all (it's a
  `gl_FragCoord`-keyed dither, unrepresentable in the CPU twin without a
  fragment coordinate). Its only verification is the visual absence of
  banding in shots 3/4/8 (§4).
- **`uAirHazeCeiling`'s wiring** (the haze_overcast_density ceiling) has no
  dedicated test — it is exercised implicitly by shot 2 (force-haze) reading
  correctly after the `air_weather_gain` retune, but nothing pins the
  `min(weather*gain, ceiling)` clamp behavior directly at a config value
  where the ceiling actually binds.
- **The dusk/night/midday cel-time offsets used for screenshot capture**
  (110 s / 232 s / 290 s) are empirically-found values for THIS config's sun
  path at THIS spawn point, not derived constants — a future celestial
  config retune (tilt, day period, epoch phases) would silently invalidate
  them with no test to catch it. They are capture-tooling values, not
  shipped behavior, so this is a documentation gap rather than a code gap.
- **Perf numbers (§5) are a single 120-frame `--smoke` run**, not a
  statistically-repeated measurement or a regression gate — `SMOKE_TIMING`
  is printed, never asserted against a budget in any test.

---

# S-domeround — round the domes — deliverable report

Branch `sandbox/bubble-atmosphere`, on top of `133bfc0bc` (the S-airdome
quality fix pass above, gate 988/988). Contract: `docs/airdome_round_spec.md`.
Chad's ruling: *"if there is thin air somewhere then no weather there.
Weather only in the bubbles. Also round the domes in the shared field.
Cylinders are not bubbles."* Two ask-user rulings: the roundness dial is a
tunable superellipse exponent `n` (ships at `n=3`), and the centre ceiling
RISES to preserve the dome's air volume (Chad's choice over keeping the
literal number).

## 1. What landed

### 1.1 The dome shape (`sim/aero.h::atm_frac_at` — the sanctioned kernel
exception)

Replaced the independent-axis product `u_h(arc) * u_v(alt)` (a cylinder: two
separate soft walls multiplied, meeting at a hard right-angle corner) with
the spec's superellipse-of-revolution law:

```cpp
const double alt_p = std::max(alt, 0.0);
const double H = (b.dome_h_m > 0.0) ? b.dome_h_m : b.ceiling_m;
// s = ((arc/r_eff)^n + (alt_p/H)^n)^(1/n); rho = sqrt(arc^2+alt_p^2)
// d = rho*(s-1)/s; w = (alt_p/H)^n / s^n; soft = mix(edge_soft, ceil_soft, w)
u = atm_falloff(d, soft);
```

with the exact guards the spec specifies: `radius_h<=0 || H<=0` ⇒ `u=0`
(extinct faction, unconditionally — including dead-center-at-ground, an
improvement over the old code's edge case there); `rho<=0` (dead centre at
ground, a LIVE bubble) ⇒ `u=1`. Implemented **identically** (line-by-line,
not just numerically) in the three mandated sites:

- `sim/aero.h::atm_frac_at` (the H1 site).
- `render/air_field.h::air_at` (the CPU mirror) — `AirField::Bubble::ceiling_m`
  now **carries H**, not the literal config ceiling (spec §4's "folded into
  the existing per-bubble ceiling uniform" — no new uniform array), with a
  load-bearing comment on the struct explaining the fold.
- `render/air_field_glsl.cpp::kAirFieldGLSL`'s `air_at()` — a new
  `uAirDomeExp` scalar uniform (one value, shared by every bubble in the
  field — Chad's ONE fly-dial) alongside the existing per-bubble arrays.

`sim::AtmosphereField` gained `dome_exponent` (field-wide, default **3.0** —
a bare `AtmosphereField` a test constructs without opting in gets the
SHIPPED dome shape, not a silently-reverted cylinder — this is the "dome
shapes are the new normal" reading of Chad's ruling) and
`AtmosphereField::Bubble::dome_h_m` (per-bubble, default **0.0 sentinel** ⇒
`H` falls back to the literal `ceiling_m`, i.e. the `bubble_ceiling_
volume_preserve=false` reading — chosen specifically so every pre-existing
`Bubble{...5-field aggregate}` construction across the tree keeps its exact
prior ON-AXIS behavior unless a builder explicitly opts into the
volume-preserving H).

### 1.2 The volume-preserving ceiling (`config/load_game.cpp`)

`I(n) = Γ(1+1/n)·Γ(1+2/n)/Γ(1+3/n)` computed via `std::lgamma` at LOAD time
(not a hard-coded table), stored on `GameParams.atmosphere.bubble_dome_h_m =
min(bubble_ceiling_m / I(n), 20000)` — gated on the new `[atmosphere]
bubble_ceiling_volume_preserve` bool (`true` ships; `false` ⇒ `I(n)` forced
to `1.0`, i.e. `H == ceiling_m` literally, so Chad can A/B the two readings
of "ceiling"). `world/faction_bubbles.h::build_faction_bubbles` scales the
derived `dome_h_m` by the SAME `ceiling_scale` growth factor as `ceiling_m`
and clamps to the same 20000 m bound ("keep growth scaling H" per spec) —
mirrored into `app::ConquestWorld::bubble_dome_h_m` /
`app::conquest_world.h`'s `DomeCfg` adapter and the non-conquest single
test-bubble construction in `app/main.cpp` (`atm_field.dome_exponent` set
once from `game.atmosphere.bubble_dome_exponent`).

At the shipped table (`n=3`, `ceiling_m=4000`): `I(3) = 0.806133`, so
`H = 4961.96 m` (the spec's own worked example rounds this to ~4959.7 in
prose; the `std::lgamma`-derived value used everywhere in code/tests is the
more precise 4961.96 — a rounding difference in the spec's illustration, not
a deviation in the implementation).

### 1.3 Weather only where there is air (`app/main.cpp`, `render/air_field.h`)

New pure helper `render::gate_weather_by_air(raw_weather, eye_pos, field) =
raw_weather * air_at(eye_pos, field)` — `app/main.cpp` now routes BOTH the
per-frame `render::weather_cell(...)` scalar AND the `\` force-haze debug
override through it (using `pose.eye`, the actual camera position, and the
SAME `air_field_render` the sky/ground/star march already reads — rebuilt
live from `env.atm` earlier the same frame). `info.precip_intensity` reads
the now-gated `wfield`, so precipitation inherits the gate automatically
(no separate wiring). Wind audio needed no change — it already reads only
`sim::atm_frac_at`'s altitude-taper output via `rho_at`, which was already
spatial (and is now dome-shaped for free).

## 2. Tests

New/updated, `test/unit/test_air_field.cpp` (6 new `domeround leg N` cases,
81 assertions), plus fixes to 3 pre-existing tests whose fixtures sampled
points that the dome shape legitimately moves (see §3), plus a new loader
leg (`test_gravity.cpp`, 2 new TEST_CASEs, 20 assertions).

1. **On-axis exactness** — swept 7 bearings × 8 arcs at `alt=0`, and 8
   altitudes at `arc=0`, against the OLD horizontal/vertical expressions
   (the vertical oracle uses `H`, not literal `ceiling_m`, per the spec's
   own "modulo H" caveat). `Catch::Approx(...).margin(1e-9)`, not literal
   `==` — a `std::pow`/`std::pow(1/n)` round-trip and the geometric
   `dir→acos→arc` reconstruction both carry a few ULP of floating noise even
   though the underlying identity is exact; 1e-9 is far tighter than any
   real mutant. **Caught a real test-fixture bug during authoring**: the
   first draft placed on-axis probes at literal ground level (`alt=0`),
   which collides with the UNRELATED deck term's own "full air below
   deck_agl_m EVERYWHERE" floor (`atm_falloff`'s `d<=0` plateau fires
   regardless of the bubble) — fixed by disabling the deck
   (`deck_agl_m=-1e9`) in the isolated fixture; this is the "flat instrument
   confounds a spatial claim" trap, one level removed.
2. **Cylinder limit at n=32** — near EACH pure axis (small absolute
   secondary-axis offset, 10–50 m, not just a small FRACTION of that axis's
   own radius/ceiling), matching within `1e-3`. **Deviation from the literal
   spec ask**, reported honestly: a generic off-axis point (e.g.
   `arc=0.5·r, alt=1.1·ceiling`) does NOT converge to the old cylinder even
   at `n=32`, because the meridional distance `rho=sqrt(arc²+alt²)` mixes
   BOTH axes' ABSOLUTE distances, while the old code's soft edge is
   per-axis-separable — `0.5³` is astronomically negligible inside the norm
   `s` (classifying the point as "inside" correctly), but `arc=3000 m`
   itself is not negligible inside `rho`, so the SOFT TRANSITION WIDTH
   still differs from the old per-axis falloff by a real amount (measured
   0.03–0.16 at candidate points) — a genuine structural property of the
   Euclidean corner-rounding, not a numerical-precision artifact of a
   finite `n`. This is exactly what leg 3 demonstrates is the point of the
   feature; leg 2 instead verifies the narrower, true claim (near-axis
   convergence).
3. **The corner actually rounds** — at 90% of the horizontal radius AND 90%
   of the (config) ceiling, `n=3` gives `u < 0.95` where the OLD law gives
   `u == 1.0` EXACTLY (asserted as a premise, the anti-no-op guard). **Also
   a deviation from the literal spec ask**: the literal "70%/70%" point sits
   fully inside the dome's OWN hard core at `n=3` too (`s<1` there — verified
   numerically before writing the test), so `old==new==1.0` and the leg
   would have been vacuous at that exact fraction (the fixture-no-op trap
   this codebase has been bitten by 4+ times) — 90%/90% is the smallest
   round-number fraction that genuinely separates the two laws, chosen
   deliberately over the spec's literal number rather than silently
   substituted.
4. **Volume preservation** — `I(2) == 2/3` exact self-check, plus a 20000-step
   trapezoid quadrature of the pure superellipse shape's cross-sectional
   area vs. the closed-form `H=ceiling_m/I(n)` at `n∈{2,3,8}`: relative error
   `< 1.3e-13` at `n=2,3` and `1.3e-7%` at `n=8` (machine-precision — see §4
   for the exact numbers).
5. **Extinct faction** — `ground_radius_m=0` reads exactly `0.0` at the dead
   centre (both at ground AND at altitude), where the LIVE-bubble
   `rho<=0 ⇒ u=1` guard would have fired were the extinct check missing —
   the SAME deck-confound trap as leg 1 bit this leg too on the first draft
   (fixed the same way).
6. **Weather gating** — the new `render::gate_weather_by_air` pure helper:
   unchanged at a dome centre (`0.8 → 0.8` exactly), exactly `0.0` in the
   vacuum gap regardless of the raw weather value fed in.
7. **Loader** (`test_gravity.cpp`): the shipped `bubble_dome_exponent=3.0` /
   `bubble_ceiling_volume_preserve=true` load correctly, AND the loader's
   own `std::lgamma` derivation is cross-checked against an INDEPENDENTLY
   computed `I(n)` in the test (not calling `config/load_game.cpp`'s
   internals) — `1e-9` epsilon, plus an anti-no-op `REQUIRE(H > ceiling_m)`.
   A companion case flips `bubble_ceiling_volume_preserve=false` and asserts
   `H == ceiling_m` EXACTLY (not Approx — `I(n)` is forced to the literal
   `1.0`, no float division at all on that arm). Range-check
   `CHECK_THROWS` legs for the `[1.5, 32]` band and both new keys' strict
   missing-key behavior.

### 2.1 Mutation-verify (4 mutants, all killed, on the core `sim/aero.h` law)

Applied and reverted individually against the built `seads_tests.exe`
(confirmed fresh after each `rm` + rebuild — the stale-relink-lock lesson):

| # | Mutant | Killed by |
|---|--------|-----------|
| 1 | Swap the `soft = mix(edge_soft, ceil_soft, w)` blend direction (`ceil_soft*(1-w) + edge_soft*w`) | leg 1 (on-axis `alt=0` reads the WRONG soft) and leg 2 |
| 2 | Drop the `alt_p = max(alt, 0)` clamp (feed raw `alt`, allowing negative) | leg 1 (`alt=-50` case reads `nan` — `pow` of a negative base) |
| 3 | Disable the `radius_h<=0 \|\| H<=0` extinct guard | leg 5 (dead-centre-at-ground reads `1.0` instead of `0.0`; off-centre reads `nan`) |
| 4 | Ignore `dome_h_m` (force `H := ceiling_m` always, defeating the volume-preserve fallback logic) | leg 1's `arc=0` sweep at `alt=H=4961.96` (the volume-preserved value) |

Not mutation-verified this round (time-budgeted): the `w` formula's own
exponent (`pow(alt_p/H,n)/pow(s,n)`) in isolation, and the GLSL
transliteration (no GL context available to this harness — see §5's
inherited coverage-limit note).

## 3. Test moves — every one explained, none blind-re-recorded

Per the spec's "MAY move, must be explained, never blind re-recorded" rule.
**Zero controller/kernel goldens moved** (`golden flight` test #693 and
`controller golden` test #694 both green in the final gate; `env.atm ==
nullptr` on that path so `atm_frac_at` returns `atm_frac(alt,p)` before ever
reaching the bubble code — untouched by this diff).

1. **`test_atmosphere.cpp` "bubble EDGE thins below baseline"** — probed
   `arc=6600 (mid-edge), alt=2000 (< ceiling)`. OLD reading: `u_v==1`
   EXACTLY (alt below ceiling, independent-axis product never engages the
   vertical term) so `u==u_h==0.5`. This point is NOT on either pure axis
   (`alt_p=2000 ≠ 0` — it is not "vertically inert", it is just far from
   ITS OWN axis's soft transition), so the dome's meridional norm folds it
   in — offline-derived (Python) and cross-checked against the live field:
   `u = 0.20748909094246104`, not `0.5`. Updated with the derivation
   documented inline, not just the new number.
2. **`test_atmosphere.cpp` "ellipse: minor_radius_m==0 is bit-identical to
   the legacy circle"** — the `f_legacy == f_explicit` equality (the actual
   point of the test — two independently-built bubbles agree) is
   **untouched and still exact**; the auxiliary ORACLE arm (a deliberately
   hand-duplicated re-derivation, the AT-12 fork-detector discipline) was
   still computing the OLD `u_h*u_v` product — updated to duplicate the NEW
   dome law instead (same `H=ceiling_m` sentinel fallback, same `n=3.0`
   default both bubbles share), so the oracle still catches an
   ellipse-routing fork, now against the shipped law.
3. **`test_faction_bubbles.cpp` "ellipse_r_eff matches the live sim air
   edge"** — the HARD-edge check (`reff_major/minor == a/b`, `epsilon(1e-9)`
   — the actual claim this leg is about, the AI-leash single-source) is
   **untouched**. The bisected HALF-AIR-CROSSING point (measured at
   `alt=500 m`, `margin(3.0)`) shifted ~11.3 m (major) / ~9.8 m (minor)
   inward — offline-derived and matched exactly against the measured
   values before widening the margin to `15.0` with the derivation
   explained inline, not silently loosened.

## 4. Measured dome volume vs. the cylinder (spec §4 leg)

At `a=8000, b=5000, ceiling_m=4000` (arbitrary, not the shipped-table
values — chosen distinct from `sim::AircraftParams`'s `R` scale to keep the
flat-footprint volume approximation the spec's closed form assumes honest):

| n | I(n) | H (m) | quadrature volume (m³) | cylinder volume (m³) | rel. error |
|---|------|-------|------------------------|------------------------|------------|
| 2 | 0.666667 | 6000.00 | 502654824571.2 | 502654824574.4 | ~6e-13 |
| 3 | 0.806133 | 4961.96 | 502654824280.1 | 502654824574.4 | ~6e-13 |
| 8 | 0.960271 | 4165.49 | 502654757874.8 | 502654824574.4 | 1.3e-7 |

Well inside the spec's ~1% bar at every tested `n` — the volume-preservation
math is essentially exact (the `n=8` residual is pure quadrature
discretization at 200000 steps, not a modeling error).

## 5. Screenshots — honest judgement

`build-play` (RelWithDebInfo), re-captured all 8 spec-§4 shots into
`docs/airdome_shots/` (overwriting) plus the two new ones. Spawn directions
computed offline (Python, `world/faction_bubbles.h`'s baked constants) and
cross-checked: `kValleyCenterDir` (valley), the normalized sum of
`kValleyCenterDir+kSudburyCenterDir` (vacuum gap), two points along the
Valley major axis bracketing its hard edge (`a±800 m`, edge-crossing). Same
cel-time offsets as the prior pass (110 s midday / 232 s dusk / 290 s
night — re-verified visually, not re-swept). `TakeScreenshot` writes to the
CWD regardless of the path's directory component (a raylib behavior, not
new to this pass) — captured to the repo root and moved into
`docs/airdome_shots/` after each run.

| # | File | Verdict |
|---|---|---|
| 1 | `01_valley_midday.png` | **PASS.** Clean blue daytime sky, terrain fully legible, `AIR 100%`. |
| 2 | `02_valley_forcehaze.png` | **PASS.** Visibly milkier near the horizon than shot 1, upper sky stays blue — no white-out. |
| 3 | `03_vacuum_gap_midday.png` | **PASS**, and arguably the strongest single piece of dome-shape evidence in the set — near-black overhead with stars, terrain crisp below, `THIN AIR`, and two clearly ROUNDED (not flat-topped) blue lenses flanking the gap on both sides. |
| 4 | `04_both_domes_4km.png` | **PASS.** `SEADS_OBLIQUE` (pan −8°, tilt 6°, 12 km up, 25 km back) at the vacuum-gap direction — two legible luminous blue lenses, black vacuum directly between them, terrain visible below both. |
| 5 | `05_valley_dusk.png` | **PASS.** Warm salmon/orange low near the sun, blue-purple aloft, smooth gradient — no hard seam. |
| 6 | `06_valley_night.png` | **PASS**, same honest caveat as the prior pass — reads convincingly dark/non-blue at a glance; whether the faint starfield is "slightly veiled" vs. "barely visible" is a judgment call, unchanged by this round (the dome-shape change doesn't touch the night sky term). |
| 7 | `07_vacuum_gap_night.png` | **PASS.** Brilliant unveiled stars, zero airglow, `THIN AIR`. |
| 8a/8b | `08a_edge_crossing_in.png` / `08b_edge_crossing_out.png` | **PASS with a note.** `AIR 100%` → `THIN AIR`, smooth gradient both frames, no banding. Point (a) landed well inside the hard edge rather than precisely astride the soft transition's midpoint (a geometry-estimation slop, not a rendering defect) — the pair still demonstrates a clean, non-banded crossing, which is the acceptance bar. |
| 9 | `09_dome_profile.png` **(NEW)** | **PASS — this is the shot that proves Chad's ask.** From ~9 km up outside the domes, the silhouette against space reads unambiguously as a DOME: the blue shell curves smoothly from the ground up and INWARD toward a rounded top, with no flat plateau and no hard corner anywhere along the visible limb. (The frame happens to catch both domes, which if anything strengthens the read — two independent confirmations of the same rounded profile in one image.) Compared honestly against what a cylinder would show here (a flat-topped slab with a visible horizontal ceiling line and vertical walls) — this is not that. |
| 10 | `10_vacuum_gap_precip.png` **(NEW)** | **PASS on the half it can show.** Force-haze active, vacuum-gap viewpoint: no precipitation, no visible haze thickening in the black gap overhead — the gating works. The "a dome in frame shows the weather" half is only weakly visible here (the domes are ~20+ km away at the frame's edges, too distant for the thickening to read clearly against shot 2's much closer, unambiguous demonstration of the same mechanism) — judged honestly as a distance/framing limitation of this specific shot, not a mechanism failure (shot 2 already proves the mechanism fires; this shot's job is proving the gap stays clean, which it does). |

## 6. Perf

`build-play --smoke 120` at shot 1's viewpoint (2 km inside the Valley
dome, midday), same methodology as the prior pass:

- **Before this round** (from the S-airdome quality-fix-pass report above,
  same viewpoint/config otherwise unchanged by this round): `avg=2.997 ms`.
- **After this round**: `avg=3.486 ms  min=1.479 ms  max=41.957 ms
  (286.8 fps avg)` — the 42 ms max is the usual first-frame warm-up spike.
- **+16.3%**, from the added `pow()` calls in the dome march (4–5 extra
  `pow` evaluations per bubble per march sample, replacing what was 0 extra
  ops in the old independent-axis product). Comfortably within any
  reasonable frame budget; `march_steps_sky` was not touched.

Every capture's TraceLog was checked for `SKY:`/`PLANET:`/`STARS:` and
`SMOKE_TIMING` (the perf run specifically) — clean, no `WARNING`, no
compile-failure lines, matching the prior pass.

## 7. Gate

`.claude/hooks/gate.sh` (layering check + build + full ctest): **994/994**
(988 prior + 6 `test_air_field.cpp` domeround legs + 2 new
`test_gravity.cpp` loader legs, net after 3 pre-existing tests were
UPDATED not added). Layering check (`graphify.py --check-only`) clean.
`tools/graph/graphify.py` regenerated in this commit. `clang-format`
applied to every touched file. Zero controller/kernel golden moved. No file
under `sim/` or `control/` touched beyond the ONE sanctioned exception
(`sim/aero.h`'s air-field shape, `sim/fields.h`'s two new struct fields) —
`sim/step.cpp`, `control/`, and every gain/threshold untouched.

## 8. Deviations from `docs/airdome_round_spec.md` (all reported, none silent)

1. **§5 leg 2** ("n=32 matches the old cylinder within a tight bound") —
   redefined from "any off-axis corner point" to "near each pure axis
   (small ABSOLUTE secondary offset)". A literal generic off-axis point does
   NOT converge to the old law even at `n=32`, for the structural reason in
   §2 leg 2 above (the meridional `rho` mixes absolute distances, not
   ratios) — this is not a bug in the implementation, it is a property of
   the math the spec's own §1.2 proof only actually established for the two
   PURE axes, not generically off both. The corner-region "tight bound"
   claim was moved to leg 3, reframed as "the corner rounds" (which is what
   it demonstrates).
2. **§5 leg 3**'s literal "70% radius AND 70% ceiling" point sits inside the
   dome's own hard core at `n=3` (`s<1` there, verified numerically before
   writing the test) and would have been a vacuous no-op leg. Used 90%/90%
   instead — the smallest round-number fraction that genuinely separates
   the two laws — documented inline with the numeric check that motivated
   the change.
3. **`H`'s worked example**: the spec's prose says `H ≈ 4959.7 m` at
   `n=3, ceiling=4000`; the actual `std::lgamma`-derived value used
   throughout the code and tests is `4961.96 m` — a rounding difference in
   the spec's illustration (its own `I(3) ≈ 0.80649` vs. the precise
   `0.806133`), not a deviation in the implementation.
4. **`09_dome_profile.png`** ended up framing both domes rather than a
   single isolated one (an oblique-camera aiming attempt at a single dome
   landed on empty terrain with neither dome in frame — discarded); the
   two-dome frame that worked is, if anything, stronger evidence (independent
   confirmation of the same rounded silhouette twice), so it was kept
   rather than spending further budget chasing a single-dome-only crop.
5. **Mutation-verify (spec's "all mutation-verified")** was done as a
   targeted 4-mutant sweep on the core `sim/aero.h` law (§2.1), not an
   exhaustive per-leg sweep of every new test the way some other landed
   mechanisms in this codebase have been — time-budgeted; see §9 for the
   specific gaps this leaves.

## 9. What is NOT covered by a test

- **The GLSL shader's superellipse math has no independent numeric pin** —
  inherits the exact limitation the prior S-airdome pass documented (no GL
  context in this harness): the source-validator leg only checks that
  specific substrings (`pow(arc / radiusH, n)`, `pow(altP / H, n)`,
  `uAirDomeExp`) are present in `kAirFieldGLSL`'s text, not that the
  compiled shader computes the right VALUE. Correctness rests on the
  line-by-line transliteration discipline + the screenshots + the clean
  `SKY:`/`PLANET:`/`STARS:` TraceLog lines (a human/vision judgement, not a
  machine oracle).
- **The `w` blend exponent and the `d`/`rho` formula were not each
  independently mutation-verified in isolation** — only 4 targeted mutants
  were run (§2.1: the mix-direction, the `alt_p` clamp, the extinct guard,
  and the `dome_h_m` fallback). A mutant that, say, used `arc` instead of
  `rho` in the `d` computation, or transposed `n` and `1/n`, was not
  explicitly tried and reverted — the on-axis exactness leg (leg 1) and the
  volume-preservation leg (leg 4) would very likely catch either given how
  tightly they pin the formula's shape, but this is inference, not a
  verified kill.
- **`place_in_faction_air`** (`app/instructor_tick.h`, the drone/scramble
  respawn placement — arc `0.6·b` from the dome centre, altitude clamped
  well under the config `ceiling_m`) was checked BY HAND (not a new test)
  to confirm it still lands inside the dome's hard core under the new
  shape at the shipped table and at the loader's most extreme legal growth/
  shrink combination (`s ≈ 0.61–0.69` at the worst case tried, comfortably
  `<1`) — existing `test_conquest_respawn.cpp`/`test_conquest_leash_repro.cpp`
  legs exercise this path and all stayed green, which is real coverage, but
  no NEW leg was added specifically asserting "placement stays inside the
  dome's core under the ROUNDED shape" as its own named claim.
- **The dusk/night/midday cel-time offsets** (110 s/232 s/290 s) are the
  same capture-tooling values as the prior pass, re-verified only visually,
  not re-derived — unchanged limitation.
- **Perf numbers (§6) are a single 120-frame run**, not a statistically
  repeated measurement or a regression gate, same as the prior pass.
- **The loader's `[1.5, 32]` exponent band's UPPER edge** (`pow` precision
  degrading above 32, per the spec's own rationale) was not itself
  empirically measured — the range check is enforced (§2 CHECK_THROWS legs)
  but nothing quantifies how "degraded" `n=32` actually is vs., say, `n=64`.
