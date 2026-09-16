# S-airdome / S-domeround — HANDOFF

> **⚑ SIBLING THREAD ON THIS BRANCH (2026-08-09): the M-KEY MAP pass —
> S-mapread / S-maparrow. Read `docs/map_handoff.md`** (arrow attribution, the declutter ledger,
> the colour scheme, the Arial licensing trade, and Chad's fly card). It touches
> `render/bubble_map.h`, `render/tourist_map.cpp`, `render/draw.cpp`, `render/map_style.h`,
> `render/map_font.h`, `config/world.toml [map]` — no overlap with the sky/air work below.

## FRESH SESSION START HERE  (refreshed end of session 2026-08-09)

**Branch:** `sandbox/bubble-atmosphere` (off `sandbox/kernel-v5-reconcile`). NOT merged, NOT pushed.
**Tip:** `4eead3543`. Working tree clean. Gate **1024/1024 green**, zero goldens moved —
independently re-verified on a freshly relinked `seads_tests.exe`, not taken on an agent's report.

### ⚠ THE ORDER OF PLAY CHAD SET (2026-08-09, his words)

> *"lets write the handoff and I will continue in seads-recon after the dem gets baked, committed
> and pushed."*

**A DEM BAKE IS IN FLIGHT IN A DIFFERENT SANDBOX** — `D:\seads_sandboxes\world-dem` on
`sandbox/world-dem` (plus an `offline_tool` venv python). **That lands FIRST.** Do not start new
work on this branch, and do not merge anything here, until the DEM is baked, committed and pushed.
This branch is a linked worktree of the same repo, so the bake cannot touch these files — but the
two threads must not be interleaved.

**Consequence for anyone measuring performance:** any `[SMOKE_TIMING]` line or `SEADS_PROF=1` run
taken while that bake is running is CONTENDED and meaningless. Do not tune against it. The
underground thread has a real history of perf regressions (the lamp recompile storm Chad caught on
the stick), so when the stope work is judged, measure it AFTER the bake finishes.

### Status: everything below is BUILT + GATED and AWAITING CHAD'S FLY

Nothing in this session has met his stick except where a verdict is quoted. Two fly verdicts landed
mid-session ("It good!" on the bubbles, "Very good." on the map) — both are recorded against the
specific commits they judged, NOT against the tip.

**To fly:** `cd D:\flight_sim2\seads-recon` then `.\build-play\seads.exe` (run from the repo root so
`assets/` + `config/` resolve). Rebuild with `cmake --build build-play --target seads`. ⚠ If the
link fails with `cannot open output file seads.exe: Permission denied`, a `seads.exe` is still
running — close it, do not kill blind.

### The session's commits, oldest first

| Commit | Thread | What |
|---|---|---|
| `d22b98e78` | sky | S-airdome: the atmosphere becomes spatial — visible air only where the air field says there is air |
| `133bfc0bc` | sky | Quality fix pass: rescaled the optical depth, killed the white-out, restored the dusk blaze, jittered the march |
| `452f71a0b` | sky | S-domeround: the bubbles become DOMES in the shared field + weather gated to air |
| `4c9dfca65` | sky | **S-skyfix** — whitewash off, scatter sun-gate fixed, air march bounded, weather anchored in the domes |
| `f72424fc2` | sky | **S-sunglare / S-starnight / S-wetseason** — the glare double-count, stars under clear dome air, the dry-season weather gate |
| `09d040dc2` | world | **S-winterloop** — default season is WINTER (Chad's ruling) |
| `07e003f78` | map | **S-mapread / S-maparrow** — greyscale chart, declutter, the arrow attribution |
| `951ee6323` | map | **S-mapteam** — one faction palette (slag orange + its exact complement), pump glyphs, place names removed |
| `4eead3543` | map + world | **S-pumpglyph / S-pumpcube** — the pump reads as a pump at true scale, and as CLAIMED (neon team wire cube) in the black stope |

### The in-flight agent LANDED and was verified — `4eead3543`

**S-pumpglyph + S-pumpcube.** Gate **1024/1024** on a freshly relinked `seads_tests.exe`
(re-verified by the supervising agent, not taken on report), zero goldens, `sim/`+`control/` clean.

Both claims were checked BY LOOKING, not by reading the report:
- `docs/map_shots/pump_cube_enemy_far.png` — at **700 m across the chamber** the enemy pump is
  unmistakably orange-framed; `pump_cube_ally_stope.png` is unmistakably blue. The mechanism works.
- `docs/map_shots/map_r3_pump_scale.png` — at **true 1:1** the glyph reads as a green machine with a
  dark funnel and a black foot, distinct from the ally circle and the enemy square.

Two findings worth carrying (both in `docs/lessons.md`):
- **The agent's own first cut painted the owner ring over the improvement** — the ring was sized off
  the old casing radius while the new discharge cone stands proud of it. INVISIBLE in the zoomed
  shot, visible only at true scale. Rings now derive from the glyph's real extent.
- **GL core profile clamps `glLineWidth` to 1**, so a width-based "neon" glow ships as a hairline.
  The thickness is three nested shells 0.4 m apart instead. Same invisible-primitive class as the
  `DrawTriangle` winding trap — the gate cannot see either; only a screenshot can.

Stope vs surface, found in `combat/conquest.h` not assumed: pumps **0/1 are surface**, pumps **2/3
are the deep pair**, one per faction. `[pump_frame] deep_only = 0` extends the cube to every pump.

**Its declared gaps (still open):** no shot from INSIDE the cube exists (a 26 m body in a 40 m frame
puts the edges behind the eye before they enter frame — only Chad's fly settles whether that
matters); the dead-pump grey and neutral-faction frames are unit-legs only; and **no performance
number was taken at all**, deliberately, because the DEM bake was contending the box — a
`[SMOKE_TIMING]` line would have been meaningless. **Measure perf AFTER the bake.**

⚠ **A note on the Stop-hook gate:** it threw a red mid-session (`Unable to find executable` /
`resource busy or locked`) purely because it fired into the middle of the subagent's own ctest run
and tried to relink the test binary underneath it. It was NOT a broken tree — the docs commit that
triggered it touched zero code. If two gates can overlap, expect this; re-run once idle.

## S-skyfix — Chad's four fly issues and what was done (2026-08-09)

| His words | Attribution | Fix | His dial |
|---|---|---|---|
| "lets lose the white scatter" / "whitewashed with glare" | the daytime Mie tint | `mie_tint_day` → `[0,0,0]`. Day = pure blue Rayleigh; the dusk orange blaze is a separate tint and is untouched | `mie_tint_day` (restore `[0.92,0.94,0.98]`) |
| "mie scatter only occurs after the sun is much higher... about 1/4 up. Maybe the scatter is referenced to ground level" | **NOT ground-referenced** — `up == normalize(eye)` already, and at 200 m the horizon dip is 0.45°. It was `scatter()`'s own gate: `sunElevCos` is the SINE of elevation, so `smoothstep(-0.05, 0.30, ·)` only reached full at **17.5°** and sat at **0.057 AT the horizon** | gate on the sun-elevation window the sky's own dusk blend already uses (`uDuskLo`/`uDuskHi`) — one source of truth instead of a third pair of edges. Full from 5.7° up, **0.60** at the horizon | `dusk_lo_deg` / `dusk_hi_deg` |
| "tire track artifact across my screen"… "more near the boundaries" | **march STRIDE, not the dither.** 20 steps across a blind 55 km ⇒ ds = **2750 m** against a **1200 m** edge_soft — the dome shell was sampled coarser than the shell itself, which bands it, world-anchored, worst at the boundary | `air_march_range` brackets the segment that can contain air, so the same steps land *inside* it (ds drops to a few hundred m). Empty bracket ⇒ tau exactly 0. Plus the large-argument `sin` hash → interleaved gradient noise | `march_steps_sky` if any residual |
| "add more weather as micro events that happen more frequently within the bubble" | the global lattice put 24 cells on the **whole sphere** while the domes cover **~8%** of it — the storm budget was mostly spent on cells in vacuum that the air gate then zeroed | cells now **anchored inside each dome** (equal-area sunflower per cap, locked to the bubble's own frame). Rate cranked: `gate_lo` 0.12→−0.05, thresh band 0.15/0.95→0.05/0.65 | `bubble_cell_count`, `bubble_inner_deg`/`bubble_outer_deg`, `gate_lo` |

**The bound is derived, not padded** — `|p−c|² ≤ alt² + s²(1 + alt/R)` — and the property that
matters (*the bracket never clips real air*) is pinned by brute-force scan, not by re-deriving the
algebra. **`bubble_cell_count` was sized by MEASUREMENT:** at 8 the small anchored cells covered no
more of a dome than the old big sphere cells did, and the comparison test failed honestly before
being raised to 16.

**Deliberately re-derived, not re-recorded:** the weather distribution moved 70/25/5 → **58/34/7**
because `gate_lo` was *meant* to move it. The test states the new design target and re-derives the
single-sine tripwire for the new gate. Spawn-clear is preserved and is the binding constraint on
`gate_lo`: `s(0) = −0.098`, so it must stay above that.

**A/B evidence** (same celestial cell, before vs after): `docs/airdome_shots/skyfix_*.png` (local
only — `*.png` is gitignored). Midday wash reduced, boundary striations gone, dome reads crisper.

## S-skyfix ROUND 2 — Chad's second fly (2026-08-09)

Verdict: **"the bubbles now look good."** Four remaining issues, all attributed:

| His words | Attribution | Fix | His dial |
|---|---|---|---|
| "the suns glare is too intense" | **A double-count I introduced.** The sun disc has no halo and the high-sun Mie tint is now 0, so the glare is the Mie forward halo at LOW sun. Its `3.2×` dusk boost had been tuned when the old gate crushed low sun to **0.057**; S-sungate raised that to ~**0.60**, so the same boost multiplied out ~10–17× too bright. The boost was compensating for the very defect that got fixed | the three halo coefficients lifted out of the GLSL into config; gain set so the low-sun peak lands ~2× the pre-S-sungate look | **`mie_halo_gain`** ← THE glare dial |
| "at night the stars should be visible, there is an atmosphere but no overcast" | S-airdome §1.6's `ext *= 1 − airAmt` faded stars in proportion to the **always-on** air, which reaches 0.55–0.75 at a dome zenith — so a clear night inside a bubble lost most of its starfield | star extinction now driven by the **weather** amount (itself air-gated, so nonzero only inside a dome). Clear dome air → stars burn; fly into a squall → they go out | `air_extinction` (0 ships; 1 = old behaviour) |
| "I didnt see any weather in the bubbles" | **Not the anchoring — the season.** W3 made Summer/Autumn **dry**, and the season is a uniform random draw at spawn, so **half of all flights** returned before the weather field was ever consulted. The squalls were firing with nothing drawn for them | Winter = snow, **every other season = rain**. Plus `air_weather_gain` 0.30→0.40 so the haze contrast reads even before precip | `air_weather_gain`, `[precip] rain_*` |
| "the bubbles now look good" | — | — | — |

**Star A/B:** `docs/airdome_shots/starnight_before.png` (`air_extinction=1`, the old behaviour) vs
`starnight_after.png` (ships, `=0`), same night cell at AIR 65% — visibly richer starfield.

⚠ **The season gate is now stated in TWO places** — `app/main.cpp`'s `precip_rate` and
`render/precip_draw.cpp`'s draw gate. They must not fork; each carries a comment naming the other.

## ⭐ WHAT IS ACTUALLY OPEN — Chad's fly list, in priority order

Everything here is built and gated; none of it has met his stick.

1. **The stope wire cube + pump glyph v2** (`4eead3543`) — the framing is confirmed on screen from
   OUTSIDE. What is NOT known: how it reads from **inside** the cube (edges fall behind the eye), and
   whether the glyph is now right on his own eye. Fly through a claimed pump both ways.
2. **Ally vs enemy PLANES and their NAME TAGS in game.** This is the one thing Chad explicitly asked
   to "make sure" of and **it was never seen on screen** — a hands-off `--smoke` flies level and the
   conquest squadrons spawn ~7 km away in their own bubbles. The recolour is correct by construction
   AND by a compiler-enforced sweep (the parallel colour tables were DELETED, so every consumer
   failed to compile until re-pointed at `team_colors()`) — but not by eye. **Fly it.**
3. **The map arrow's two real failure modes were never seen either.** The fix is verified against
   real motion (the marker travels east-south, the arrow points east-south), but the defects it
   repairs — the tail-slide 180° flip and the near-vertical noise — are covered by UNIT LEGS ONLY,
   because level smoke flight cannot produce them. **Pull vertical, hammerhead, hold a tail-slide,
   and watch whether the arrow stays put.**
4. **`mie_halo_gain` (0.30) is the dial I have least evidence for.** It was set between "too intense"
   (his words) and "the blaze should still exist" — a bracketed guess only his eye settles. Expect to
   move it.
5. **Weather should now actually appear** — winter means snow, and the squalls are anchored inside
   the domes. If it still reads sparse, `air_weather_gain` and `bubble_cell_count` are the dials.

### Smaller open items, none blocking

- **His callsign went gold → ally blue.** Follows the rule, but gold made him unmistakable among his
  own side. One-line revert.
- **The bottom-left footer lost contrast** — `SEADS · GREATER SUDBURY · R=15 km · WINTER · BUBBLE` is
  pale blue on the new light-grey plate. Flagged twice, not yet fixed; it was never in a brief.
- **The in-game reticle ring is green**, which is now also the pump/objective colour. Different
  contexts, probably fine, but he will see both.
- **Arial is loaded from `C:\Windows\Fonts`, not vendored** (not redistributable). Liberation Sans is
  metric-compatible and free — a one-line swap in `map_font()` if SEADS ever ships. HIS RULING.
- **The season rule is stated in TWO places** — `app/main.cpp`'s `precip_rate` and
  `render/precip_draw.cpp`'s draw gate. They must not fork; each names the other in a comment. A
  third consumer should force a shared mapping instead of a third copy.
- `haze_overcast_density` was rewired as a ceiling on the weather contribution rather than orphaned.
  Confirm that reading (carried over, still unanswered).

## Honest coverage ledger for THIS session

- **No ctest runs `seads.exe`** — every visual claim above rests on smoke screenshots that were
  actually viewed, or on code inspection. Where a shot could not be produced, it is stated.
- **The GLSL march still has no numeric test** — a CPU twin plus source-substring pins only. A silent
  C++/shader divergence in `air_optical_depth` would not be caught.
- **Perf was NOT measured this session**, and cannot be trusted until the DEM bake is done.
- Two agent-reported gate counts were re-verified independently on freshly relinked binaries; one
  agent self-reported having built over a live test binary (this repo's stale-binary trap). **Do not
  take an agent's gate count on report — force `rm build/seads_tests.exe` and re-run.**

Specs: `docs/bubble_atmosphere_spec.md` (mechanism), `docs/airdome_round_spec.md` (the dome law +
its derivation). Full ledger incl. deviations: `docs/airdome_report.md`. Evidence:
`docs/airdome_shots/` + `docs/map_shots/` (⚠ `*.png` is gitignored — these are LOCAL ONLY and will
not survive a fresh clone).

## Chad's rulings this session (2026-08-09) — the spec of record

1. Atmospheric haze **always on**, never weather-gated, but **only inside the faction bubbles**.
2. **Escape sky OFF for the game loop** (the conquest force-enable is removed; `T` still toggles).
3. Blue Rayleigh + **white** Mie by day inside the bubble; the sky **may have colour now** — the
   space-first ruling is RELAXED INSIDE A DOME ONLY. Outside stays near-black space.
4. "Easily perceptible and beautiful."
5. Areas with only the 200 m deck and no bubble **look like space**.
6. The bubble zone is **dynamic during gameplay**.
7. **"If there is thin air somewhere then no weather there. Weather only in the bubbles."**
8. **"Round the domes in the shared field. Cylinders are not bubbles. The flight kernel is fine,
   I want dome shapes not cylinders."** ⇐ his explicit authorization to edit the plant's air field.
9. AskUserQuestion rulings: dome profile = **a tunable roundness dial**; centre ceiling **rises to
   preserve air volume**.

## What is built

**The air is one field, read by everything.** `sim::atm_frac_at` is the truth. `render::air_at`
(C++) and `kAirFieldGLSL::air_at` (shader) mirror it; `test/unit/test_air_field.cpp` cross-checks
render against sim over 640+ points and fails loudly on divergence. The renderer marches the view
ray through that field, so haze / Rayleigh / Mie / the horizon band / star + moon dimming / ground
aerial perspective all exist exactly where air exists. Vacuum is black **by construction** — every
term carries a final `× airAmt`, so zero air is exactly zero, not a tuned approximation.

**Dynamic for free:** the uniforms are rebuilt each frame from the live `sim::AtmosphereField` that
`rebuild_conquest_bubbles` writes. Domes grow, shrink and go extinct visually with no extra code,
and the picture cannot disagree with the flyable volume.

**The dome law** (`sim/aero.h`, mirrored twice). Superellipse of revolution:
```
s    = ((arc/r_eff)^n + (alt_p/H)^n)^(1/n)      // == 1 exactly ON the surface
rho  = sqrt(arc^2 + alt_p^2)
d    = rho*(s-1)/s                               // signed distance to the surface [m]
w    = (alt_p/H)^n / s^n                         // 0 at ground, 1 at zenith
soft = mix(edge_soft, ceil_soft, w)
u    = atm_falloff(d, soft)
```
**Why it was safe to touch the kernel:** it reduces EXACTLY to the old code on both axes — at
`alt=0` it is `d = arc - r_eff`, `soft = edge_soft` (the old horizontal edge); at `arc=0` it is
`d = alt - H`, `soft = ceil_soft` (the old ceiling). Only the CORNER rounds. As `n → ∞` the
cylinder returns, so the dial spans the whole family and `n=32` is a true A/B against the old shape.

**Volume-preserving centre height:** `H = ceiling_m / I(n)`, `I(n) = Γ(1+1/n)Γ(1+2/n)/Γ(1+3/n)`,
computed at load via `std::lgamma` so it re-solves whenever `n` moves. Analytic self-check:
`I(2) == 2/3` exactly (the true half-ellipsoid volume). At n=3, 4000 → **4962 m**. Measured dome
volume matched the old cylinder to 1.3e-7%.

**Weather** is gated at the SOURCE (`render::gate_weather_by_air`, called in `app/main.cpp`), not in
the shader, so haze, precipitation and the `\` force-haze key all read one air-gated scalar. Precip
stops in the vacuum gap automatically because it keys off that same value.

## The dials Chad flies (all in config, no bare numbers in code)

`config/game.toml [atmosphere]`
| Dial | Ships | Meaning |
|---|---|---|
| `bubble_dome_exponent` | 3.0 | roundness. 2 = true ellipsoid, 3 = domed with a slight shoulder, 8+ = the old cylinder |
| `bubble_ceiling_volume_preserve` | true | false ⇒ `ceiling_m` is the literal centre height (arena shrinks) |

`config/world.toml [atmosphere]` — these were RETUNED by eye in `133bfc0bc`; the originals were the
spec's untuned guesses and they white-outed the screen:
| Dial | Ships | Was | Meaning |
|---|---|---|---|
| `tau_scale_m` | 2600 | 12000 | path length of unit air = 1 optical depth. **The master dial.** |
| `air_haze_density` | 0.25 | 0.85 | always-on grey veil. Higher ⇒ milkier |
| `air_weather_gain` | 0.30 | 0.55 | how much a weather cell thickens air |
| `aerial_gain` | 0.35 | 1.0 | ground extinction. Higher ⇒ terrain disappears |
| `march_steps_sky` / `march_max_m` | 20 / 55000 | 14 / 90000 | march resolution |

**If Chad says "too washed out"** → `tau_scale_m` up and/or `air_haze_density` down.
**"Can't see the ground"** → `aerial_gain` down.
**"Zenith not blue enough"** → `tau_scale_m` DOWN (this is the one that is counter-intuitive; see
the attribution below).
**"Border feels too low / cramped"** → `bubble_dome_exponent` up, or volume-preserve off.

## The attribution that mattered (do not re-derive it, and do not lose it)

The first build looked wrong — washed-out horizon AND a dim zenith at once — and the instinct was to
blame the ray march. It was **one mis-scaled dial**: `tau_scale_m = 12000` against a vertical air
column of only ~2.6 km. A zenith ray could reach at most `airAmt ≈ 0.20` (structurally incapable of
being blue) while a 30 km horizon chord saturated at ~0.92. A ~12:1 imbalance. Undersampling was
real but secondary — the fix was rescaling to 2600 plus a per-pixel march jitter, not more steps.

**Lesson for the ledger:** when a look fails at both ends of a gradient simultaneously, suspect the
scale that maps path length to opacity before suspecting the sampler.

## Honest coverage ledger — what is NOT pinned

- **The GLSL march has no numeric test.** Only a CPU twin plus a source-substring check. A silent
  C++/shader divergence in `air_optical_depth` would not be caught by any automated test.
- **No ctest runs `seads.exe`** (standing project convention), so the `main.cpp` glue — the
  per-frame `AirField` rebuild, the weather gate call site, the `[gravity].enabled`-only wiring — is
  verified only by the smoke shots.
- **Visual quality is eye-judged only.** No colorimetric regression test exists.
- `SEADS_SMOKE_SPAWN_PITCH_DEG` is dead code (`app::spawn_state` projects out vertical spawn
  heading by design). Left in the tree, does nothing. Remove or fix if it ever matters.
- Three bubble/conquest tests moved with the dome shape; each was re-derived offline and explained
  in `docs/airdome_report.md`, never blind re-recorded. Controller goldens are untouched by
  construction (they fly `env.atm == nullptr`).

## Kernel firewall status

**Deliberately breached, once, on Chad's explicit ruling #8.** Exactly two files under `sim/`:
`sim/aero.h` (the dome law) and `sim/fields.h` (the `dome_h_m` / `dome_exponent` fields).
`sim/step.cpp`, all of `control/`, and every gain are untouched. Zero golden files changed.
**This does not reopen `sim/`** — the firewall stands for everything else in the world thread.

## Review trail

- Implementer report (in-session): honest, flagged its own failing shots.
- Independent cold red-team on `d22b98e78`: verdict UNSOUND. Its structural findings (H1 fencing,
  uniform wiring, concatenation order) were sound and it correctly caught the march undersampling.
  **Three of its visual calls were wrong** — it reported "no blue lens visible anywhere" and "no
  orange anywhere" on shots that plainly had both, and it flagged a missing report that had been
  delivered in-session rather than to a file. Verify agent claims against the actual artifact.
- Every screenshot was viewed directly by the supervising agent, not taken on report.

## Open questions for Chad (non-blocking)

1. ~~Weather survives as an additive thickener **inside** the domes.~~ **ANSWERED 2026-08-09:**
   *"the weather shall exist only where the bubbles exist and hence where there is an atmosphere."*
   The microsystems stay; S-skyfix went further and now GENERATES them inside the domes rather than
   merely gating them off outside.
2. `haze_overcast_density` was rewired as a ceiling on the weather contribution rather than
   orphaned. Confirm that reading.
3. **NEW (S-skyfix):** the wider sun gate also brightens **low-sun** scatter, since it multiplies
   Rayleigh as well as Mie — that is the restored dusk blaze, but if dawn/dusk now reads as a NEW
   wash, `dusk_hi_deg` up (narrows the window) or `scatter_strength` down is the lever, and it is a
   pure config change. Midday is unambiguously less washed than before (A/B shots).
