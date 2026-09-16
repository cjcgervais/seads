# ATMOSPHERE RUNG AS-1..AS-3 — snow that reads as snow, and none of it underground

Lane: `sandbox/atmosphere-snow`, worktree `D:\seads_sandboxes\atmosphere`, branched from
`origin/main` `ca73158fe` (2026-09-12). Director: Fable (this session). Builder: Opus.

## 0. Chad's ask (2026-09-12, verbatim intent)

> "Atmosphere rung needs to get done better so that we do get more snow, but not in the big
> stope. I noticed it there. Need more localized weather events not wind but appearance of snow
> in a very effective way that deepens immersion."

Read as a pilot, not a coder:
1. **NO SNOW IN THE STOPE.** He saw flakes falling inside the Murray/Errington tunnel net's big
   arena. That is a bug: the W3 lattice is world-anchored around the eye and knows nothing about
   rock overhead.
2. **MORE SNOW.** Today it snows only when a W2 weather cell is active *at the eye*, and the
   flakes live in a 21 m sphere around the eye. Most of a winter flight is dry. He wants snow to
   be a frequent, present part of the world.
3. **LOCALIZED EVENTS, NOT WIND.** Snow should come and go as bands/flurries/squalls you fly or
   drive into and out of. The standing NO-WIND ruling holds: no horizontal advection, ever.
4. **APPEARANCE THAT DEEPENS IMMERSION.** The snow must *read as snow*: depth, variety, a soft
   veil at distance, big soft flakes near the eye — not uniform dots in a globe.

## 1. What exists (cite, reuse, never fork)

- `render/precip.h/.cpp` — PURE placement core (`precip_sample`, `precip_center_cell`), in
  `seads_render_core`, gate-pinned by `test/unit/test_precip.cpp` (6 cases).
- `render/precip_draw.h/.cpp` — the raylib billboard renderer. ONE lattice, `(2H+1)^3` quads,
  H clamped to 12 (one ushort batch). `draw_precip_renderer(r, pose, season, intensity, phase)`.
- `render/draw.cpp:682-704` `g_precip` + `draw_precip`, called at `render/draw.cpp:3598` in the
  translucent tier (after smoke, before prop discs). `set_precip_build_params` from `[precip]`.
- `app/main.cpp:10127-10162` — `wfield = weather_cell(eye_track, ...)`, air-gated, then
  `info.precip_intensity = wfield;` and `info.precip_phase = frac(t_cel * rate)`.
- `render/weather.h/.cpp` — `weather_haze` (the budget) + `weather_cell` (anchored microsystems).
  **Do not change `[weather]` or `[weather_cell]` dials or their functions** — the haze
  distribution and spawn-clear are Chad-signed. Build the snow field BESIDE them.
- `app::inside_tunnel(env, pos)` (`app/instructor_tick.h:89`) = `env->tunnels->contains(pos)`,
  the SINGLE-SOURCE underground predicate (camera CAVECAM + reverb use it). `world::TunnelNet::
  signed_distance(p)` (`world/tunnel_net.h:389`) is the exact SDF (< 0 inside). `FrameInfo::
  tunnel_net` (`app/main.cpp:10010`) already hands render/ the live net pointer.
- Season is static Winter (`[seasons] static_season = "winter"`); `Season::Winter` = snow.
- Smoke rig: `--smoke` screenshots; `\` key forces haze 1.0 (air-gated). `SEADS_NO_PRECIP=1` bypass.
- `docs/weather_seasons_plan.md` W2/W3 (the original design + Fable-before verdicts).

## 2. The rungs (build in order; each: build-play + own tests green before the next)

### AS-1 — NO SNOW UNDERGROUND (the bug; mandatory, exact)

- Gate each flake by the tunnel-net SDF at the FLAKE's position, not only the eye:
  `alpha *= smoothstep(-band, +band, signed_distance(flake_pos))`, band ≈ 2 m. Standing in the
  mouth looking out, the snow outside the mouth stays visible; a flake under rock is gone.
- Keep it cheap: compute `d_eye = signed_distance(eye)` once per frame. If `d_eye > box_half +
  band` → no per-flake test (all outside). If `d_eye < -(box_half + band)` → skip the draw
  entirely (all inside). Only in the mouth band does the per-flake SDF run. Measure the cost of
  the per-flake path at the mouth (report ms); if it exceeds 0.5 ms, cull to the per-cell
  center instead of per-flake and say so.
- The net pointer comes from `FrameInfo::tunnel_net` (already plumbed). `tunnel_net == nullptr`
  ⇒ bit-identical to today.
- Apply it to BOTH lattices of AS-3.
- Tests (pure, `test_precip.cpp` or a new `test_precip_underground.cpp`): a flake at
  `sd = -10` → alpha 0; `sd = +10` → alpha unchanged; the band is C0-monotone; null net ⇒
  identity. Make the SDF an injected callable in the pure layer so the test needs no real net.

### AS-2 — MORE SNOW: a winter SNOWFALL field beside the haze (mandatory)

New PURE fn in `render/weather.h/.cpp` (or a new `render/snowfall.h/.cpp` in render_core):

```
double snowfall_intensity(dir, t_cel, wp, cp, anchors, n, const SnowfallParams& sp)
```
= `max(flurry, squall)` where
- `squall = weather_cell(...)` exactly as today (the heavy band under a live microsystem, 1.0).
- `flurry` = a LIGHT, FREQUENT snowfall: the same anchored cell machinery evaluated with a
  LOWER activation band (`flurry_thresh_lo/hi`, e.g. 0.00/0.35 — cells light at a small
  budget) and a spatial patchiness of its own (a second lattice phase / offset so flurry
  cells are NOT the same cells as the squalls), scaled to `flurry_level` (≈ 0.35). Plus an
  optional winter floor `snow_floor` (≈ 0.10) so a winter sky is almost never bone dry.
- Same seam as `weather_cell`: pure, deterministic in `t_cel` + `dir`, C1, no clock, no fixed
  axis, `dir` unit. NO WIND: cell centres fixed; things pulse in place.
- Air gate: run the result through `render::gate_weather_by_air` exactly like `wfield` (snow
  in vacuum is nonsense; the S-domeround ruling stands).
- TARGET, MEASURED not asserted: over a 2M-sample scan (the `test_weather.cpp` sampler style)
  of `t_cel` at random in-dome eye directions, snow > 0.05 at least ~60% of the time and the
  heavy band (≥ 0.8) roughly what `weather_cell` gives today (the squall share is unchanged).
  Pin the ratio band in a test with generous margins, and PRINT the measured numbers in the
  report.
- Dials in `config/world.toml [precip]` (+ `config/load_world.*` strict loader + its test):
  `flurry_level`, `flurry_thresh_lo`, `flurry_thresh_hi`, `snow_floor`. `flurry_level = 0`
  and `snow_floor = 0` ⇒ bit-identical to today's `wfield`.
- Wire: `info.precip_intensity = snowfall_intensity(...)` in `app/main.cpp` (announced edit).
  The HAZE path (`atm.haze_density = wfield`) is UNTOUCHED.

### AS-3 — THE LOOK: snow that reads as snow (mandatory)

1. **Two lattices, one renderer type.** NEAR = today's (cell 3 m, box 21 m, small flakes). FAR
   "veil" = a second `PrecipRenderer` (cell ≈ 8 m, box ≈ 70 m — inside the H=12 cap:
   (12−1.5)·8 = 84 m), bigger flakes (≈ 0.45 m), lower opacity (≈ 0.35). Draw FAR then NEAR.
   Same fall SPEED in m/s for both (rate_hz = speed / cell), so the two layers agree; make
   `snow_speed_mps` the dial (≈ 0.9 m/s) and derive the rates; keep `snow_rate_hz` readable
   or retire it cleanly (loader test updated).
2. **Per-flake variety, hashed from the cell (deterministic, pure):** size × [0.6, 1.4],
   opacity × [0.6, 1.0]. Extend `PrecipSample` (or a sibling) with `size_mul`/`alpha_mul`.
3. **Density follows intensity, not just alpha.** Thin the field at low intensity by a hashed
   per-cell cull (`keep if hash01(cell) < intensity^0.6` or similar) AND scale alpha. A flurry =
   a few soft flakes; a squall = a dense fall. Test: monotone in intensity; intensity 0 ⇒ none.
4. **Zero-mean sway (NOT wind).** A tiny lateral sinusoid on each flake in the tangent plane,
   amplitude `sway_m` (≈ 0.25 m near, ≈ 0.6 m far), phase hashed per cell, driven by the fall
   phase. Its time-average is exactly zero — no net displacement, no shared direction (the
   per-cell phase AND axis are hashed, so no two cells sway in lockstep). Pin with a test: mean
   lateral displacement over one full phase cycle == 0 (to 1e-9), |lateral| ≤ sway_m, and
   `sway_m = 0` ⇒ bit-identical to today (the existing NO-WIND test must still pass with sway
   off; write its sway-on twin that asserts zero mean). **Flag this dial to Chad in the report**
   — it is the one thing here that a strict reading of "no wind" could object to.
5. **Contrast check.** White flakes over a white winter ground and a bright sky can vanish.
   Take smoke shots at DAY and NIGHT. If day flakes disappear, add a faint darker soft rim in
   the FS (`uColor` mixed toward a dial `rim_dark` ≈ 0.85 at the edge) — mono, no chroma. Report
   what you saw; do not gold-plate.
6. **Perf.** Both lattices at full intensity: measure the precip pass (pmark "smoke_precip" or a
   local timer) in build-play. Budget ≤ 1.0 ms. Report the number.

## 3. Evidence Chad and the director will look at (produce these; put them in
`docs/atmosphere_snow/` on the lane, PNGs gitignored or small)

Add a SMOKE-ONLY rig (env vars, only honoured under `--smoke`, like `SEADS_STING_POSE_SMOKE`):
- `SEADS_SNOW_FORCE=<0..1>` forces `precip_intensity` (post air-gate) for screenshots.
- `SEADS_SMOKE_EYE_STOPE=1` puts the eye inside the tunnel-net ARENA (use `tunnel_net.arena.
  center`), and `=2` at the Murray mouth looking out. If an existing cave-cam/spawn rig already
  does this, use it and say which.

Shots (day + night where it matters): (a) surface flurry (0.3), (b) surface squall (1.0),
(c) stope interior at force 1.0 — MUST show zero flakes, (d) mouth looking out — flakes outside,
none under rock, (e) BEFORE shot from `origin/main` for (b) so the look change is visible.

## 4. Laws (non-negotiable)

- `sim/` and `control/` untouched. render/ reads state, reads no clock, writes nothing; every
  time-varying input arrives from the app as a finished scalar/phase.
- No fixed world axis in anything visible; only `local_up = normalize(eye)` and hashed
  per-cell tangent axes derived from it.
- NO WIND: no horizontal advection term; sway is zero-mean by construction and by test.
- Every new dial has an OFF value that is bit-identical to today; `SEADS_NO_PRECIP=1` still
  bypasses everything.
- Pure core stays in `seads_render_core` (headless gate); raylib only in `precip_draw.cpp`.
- ASCII test names; LF endings; `python tools/graph/graphify.py` in the SAME commit as any
  structural change, then `--stale` clean; `--check-only` layering green.
- CLAUDE.md "Commands" verbatim for configure/build/test. For the exe Chad flies, build
  `build-play` (RelWithDebInfo, Ninja — the recipe `D:\seads_sandboxes\cam-smooth\build-play\
  CMakeCache.txt` shows), never `build/` Debug.
- Commit on the lane as you go (SOP §2), never to main, never push main. Push the lane branch.
- Announce every non-owned file you touch (SOP §5) in a NEW `[lanes.atmosphere]` section of
  `LANES.toml` (owns: `render/precip*`, `render/snowfall*`, `docs/atmosphere_snow/*`,
  `docs/SESSION_HANDOFF_*atmosphere*`, `test/unit/test_precip*.cpp`, `test/unit/test_snowfall*.cpp`)
  and register new test files in `tools/gate/lane_map.toml` under `atmosphere`. Expected
  announced edits: `app/main.cpp` (the intensity wire + smoke rig), `render/draw.{h,cpp}` (second
  renderer + net gate), `config/world.toml` + `config/load_world.{h,cpp}` (+ its test),
  `CMakeLists.txt` (+ sources/tests — minimal, one line each), `.gitignore` if PNGs.
- Do NOT run the full ctest gate (it is ~1 h and the harness memory guard kills background
  ctest); run your own tests + `test_precip`/`test_weather`/`test_load_world`/`test_air_field`
  targets and the `--smoke` run. The director runs the full gate detached after red-team.

## 5. Report back (the director reads this, Chad reads a digest)

- Lane tip SHA(s), files touched (owned vs announced), test names added + results.
- Measured: snow-time share (flurry / heavy), precip pass ms (near+far, full), mouth-band ms.
- Screenshot paths (a)–(e).
- The exact absolute path of the build-play exe.
- Anything you chose that Chad might want to rule on (the sway dial; flurry_level; snow_floor;
  whether spawn should read snowy or dry).
- Anything left undone, plainly.
