# S-domeround — the bubbles become DOMES, and weather lives only in air

Branch: `sandbox/bubble-atmosphere`, on top of `133bfc0bc`. Chad's rulings, 2026-08-09:

> "The weather logic has to make sense, if there is thin air somewhere then no weather there.
> Weather only in the bubbles. Also round the domes in the shared field. Cylinders are not
> bubbles. The flight kernel is fine, I want dome shapes not cylinders."

Plus his two AskUserQuestion rulings this session:
- **Dome profile = a TUNABLE ROUNDNESS DIAL** (superellipse exponent `n`), shipping at `n = 3`.
- **Centre ceiling RISES to preserve air volume** (a dome holds less than a cylinder of the
  same height; he chose to keep the arena's volume rather than keep the number).

## 0. This one touches the kernel — deliberately

`sim/aero.h::atm_frac_at` is the plant's air field. Chad has explicitly authorized changing it
("the flight kernel is fine, I want dome shapes"). This is the ONE sanctioned exception to the
firewall in this thread; it does not open `sim/` or `control/` to anything else. In particular:
**do not touch `sim/step.cpp`, the controller, any gain, or any other file under `sim/` or
`control/` beyond the air-field shape described here.**

Expected consequences, to be reported honestly, not hidden:
- Controller goldens fly with `env.atm == nullptr` and MUST stay bit-identical. If one moves,
  **STOP** — that means the null path was disturbed, which is a bug, not a ruling.
- Bubble/conquest/drone-leash behavioural tests MAY move. Each move must be explained and shown
  to be the intended dome shape, never re-recorded blind.

## 1. The dome shape

### 1.1 The law

Today (a cylinder): `u = atm_falloff(arc - r_eff, edge_soft) * atm_falloff(alt - ceiling, ceil_soft)`.

New (a superellipse of revolution about the bubble axis). With
- `arc` = great-circle surface distance to the bubble centre [m] (unchanged),
- `r_eff` = the existing direction-dependent ellipse radius (unchanged — the ground footprint
  stays a true ellipse; `world::ellipse_r_eff` is untouched),
- `alt_p = max(alt, 0)` (below ground the dome term must not bite; the tunnel term owns that),
- `H` = the volume-preserving ceiling (§1.2),
- `n` = `bubble_dome_exponent`:

```
s   = pow( pow(arc/r_eff, n) + pow(alt_p/H, n), 1/n )     // 1.0 exactly ON the dome surface
rho = sqrt(arc*arc + alt_p*alt_p)                          // meridional distance from centre
d   = rho * (s - 1) / s                                    // signed distance to the surface [m]
w    = pow(alt_p/H, n) / pow(s, n)                         // 0 at the ground, 1 at the zenith
soft = mix(edge_soft, ceil_soft, w)
u    = atm_falloff(d, soft)
```

### 1.2 Why this exact form (do not "simplify" it)

It **reduces exactly to the current code on both axes**, which is what makes it safe:
- At `alt_p = 0`: `s = arc/r_eff`, `rho = arc`, so `d = arc - r_eff` and `w = 0` ⇒
  `soft = edge_soft`. **Bit-for-bit today's horizontal edge.**
- At `arc = 0`: `s = alt_p/H`, `rho = alt_p`, so `d = alt_p - H` and `w = 1` ⇒
  `soft = ceil_soft`. **Bit-for-bit today's ceiling** (modulo H, §1.2).
- As `n → ∞`, `s → max(arc/r_eff, alt_p/H)` — the cylinder returns. The dial spans the whole
  family, so `n` large is a true A/B against the old shape.

Only the CORNER between the two axes changes. That corner is exactly what Chad rejected.

Guards: `s <= 0` (dead centre at ground level, `rho == 0`) ⇒ `d = -r_eff` (deep inside, `u = 1`);
`r_eff <= 0` or `H <= 0` (an extinct faction) ⇒ `u = 0`, matching today's extinct-bubble
behaviour. Keep the existing `edge_soft == 0` / `ceil_soft == 0` extinct handling intact.

### 1.3 The volume-preserving ceiling

A superellipse dome of centre height `H` over the same elliptical footprint holds

```
V = pi * a * b * H * I(n),   I(n) = Gamma(1+1/n) * Gamma(1+2/n) / Gamma(1+3/n)
```

Verified: `I(2) = 2/3` exactly — the true half-ellipsoid volume. `I(3) ≈ 0.80649`,
`I(8) ≈ 0.96027`, `I(inf) = 1` (the cylinder).

So to hold the same air as today's cylinder of height `bubble_ceiling_m`:

```
H = bubble_ceiling_m / I(n)
```

At `n = 3`, `H = 4000 / 0.80649 ≈ 4959.7 m`.

Compute `I(n)` at LOAD time via `std::lgamma` (not a hard-coded table — it must track `n`), in
`config/load_game.cpp`, and store the derived `H` on the bubble. Gate it on a config flag
`bubble_ceiling_volume_preserve` (default `true`); `false` ⇒ `H = bubble_ceiling_m` literally,
so Chad can A/B the two readings of "ceiling". Clamp `H` to the existing
`world::kFactionBubbleCeilingMaxM` (20000) as growth already is, and keep growth scaling `H`.

## 2. Weather only where there is air

Chad: *"if there is thin air somewhere then no weather there. Weather only in the bubbles."*

Today the VISUAL haze is air-gated per fragment, but the weather SCALAR itself is not — so
precipitation and the weather HUD/audio still believe there is weather over vacuum. Fix it at
the SOURCE so every consumer agrees (the H1 discipline):

In `app/main.cpp`, where `render::weather_cell(eye_track, t_cel, ...)` is computed, multiply the
result by the local air fraction at the EYE:

```
w_field *= sim::atm_frac_at(draw_state.position, env_ptr, params) / atm_frac(alt)   // spatial only
```

Use the render-side `render::air_at(pos, air_field)` for this — it is already built, already
the spatial-only factor, and already pinned against the sim (`test_air_field.cpp`). Do NOT
re-derive the ratio inline.

Consequences that must follow automatically (verify each, do not assume):
- Precipitation (`render/precip*`) stops in the vacuum gap — it is gated by the same scalar.
- The `\` force-haze debug key still works, but is itself air-gated (forcing overcast in vacuum
  must produce nothing).
- Wind/weather audio, if it reads the field, goes quiet outside the domes.

The per-fragment visual gate stays as it is; this change makes the SCALAR honest so the
non-visual consumers stop lying.

## 3. Config (`config/game.toml [atmosphere]` — the felt-values table)

```
bubble_dome_exponent = 3.0    # superellipse roundness. 2 = true ellipsoid dome,
                              # 3 = domed with a slight shoulder (Chad's ship value),
                              # 8+ = the old flat-topped cylinder. Chad's fly-dial.
bubble_ceiling_volume_preserve = true   # derive the centre height so a dome holds the
                                        # same air as the old cylinder (H = ceiling/I(n));
                                        # false = ceiling_m is the literal centre height
```

Loader-validate: `bubble_dome_exponent` in `[1.5, 32]` (below ~1.5 the shape goes
diamond/pinched and the corner reappears inverted; above 32 `pow` precision degrades and it is
a cylinder anyway).

## 4. The three mirrors must all move together

`atm_frac_at` (sim), `render::air_at` (C++ mirror), and `kAirFieldGLSL`'s `air_at` (shader) are
the SAME law in three places. Change all three in this commit, and extend the existing
`test_air_field.cpp` cross-check — it already pins render-vs-sim over 640+ points and will FAIL
loudly if you update one and forget another. That is the fence working; do not weaken it.

`n` and `H` must ride through to the shader as uniforms (`uAirDomeExp`, and `H` folded into the
existing per-bubble ceiling uniform). `render/bubble_map.h` draws the GROUND outline at
`alt = 0`, which §1.2 proves is unchanged — verify, do not assume.

## 5. Tests (all mutation-verified)

New legs in `test/unit/test_air_field.cpp` and/or `test_faction_bubbles.cpp`:
1. **On-axis exactness**: at `alt = 0` across many bearings, the new law equals the OLD
   horizontal expression bit-for-bit; at `arc = 0`, equals the old vertical expression with `H`.
   (Mutant: perturb `d` or `soft` blending ⇒ must fail.)
2. **Cylinder limit**: at `n = 32`, the field matches the old cylinder within a tight bound at
   corner sample points. (Mutant: invert the `w` blend ⇒ fail.)
3. **The corner actually rounds**: at a point at 70% radius AND 70% ceiling, `n = 3` gives
   STRICTLY LESS air than the old cylinder. This is the leg that proves the feature exists —
   without it every other leg passes on a no-op. (This codebase has been bitten 4+ times by a
   fixture that makes the mechanism a no-op; do not let this one be vacuous.)
4. **Volume preservation**: numerically integrate the dome volume (a simple quadrature over
   `alt`) and check it matches `pi*a*b*ceiling_m` within ~1%, at `n = 2, 3, 8`. `I(2) == 2/3`
   exactly is a free analytic self-check — assert it.
5. **Extinct faction**: `radius_scale = 0` still yields exactly zero air everywhere.
6. **Weather gating**: a unit leg that the gated weather scalar is 0 in the vacuum gap and
   unchanged at a dome centre.

## 6. Gate + evidence

- Full `.claude/hooks/gate.sh` green. **Controller goldens MUST NOT move** (they fly null-atm).
  Any bubble/conquest test that moves: explain it, show it is the dome, never blind re-record.
- Regenerate `python tools/graph/graphify.py` in the same commit; clang-format per CLAUDE.md.
- Re-capture the 8 shots from `docs/bubble_atmosphere_spec.md` §4 into `docs/airdome_shots/`,
  PLUS two new ones:
  - `09_dome_profile.png` — from outside at range, the dome silhouette against space. It must
    read as a DOME, with no flat top and no hard corner. This is the shot that proves Chad's ask.
  - `10_vacuum_gap_precip.png` — in the gap during active weather: no precipitation, no haze,
    while a dome in frame shows the weather.
- Report to `docs/airdome_report.md` (append a new section): the gate number, which tests moved
  and why, the measured dome volume vs the cylinder, perf, and an explicit list of what is NOT
  covered by a test.

Judge the shots yourself before committing. If the dome still reads as a cylinder, say so.
