# Vessel brief — P-47D Thunderbolt ("the energy fighter")

Status: HYPOTHESIS. This is a delta table for a future Chad-flown sandbox, not a
tuned kernel. No config file is touched by this brief. Feel dials (deadzone,
pursuit, capture/rimshot, lean, coordination, compression's exact shape) are
NOT derived here — they start at vessel #1's values and get re-derived on the
stick, one dial at a time, same as every other feel thread in this repo.

## 1. Feel

Big, heavy, and stubborn to turn — this is the plane that wins by refusing to
fight on your terms. It doesn't snap into a turn; it has to be muscled around
one, and if you try to out-turn a Zero in it you will lose. What it has is a
huge motor and a strong airframe: it dives away from anything, it dives HARD,
and it keeps control authority deep into that dive when a lighter plane's
stick has already gone to jelly. The right way to fly it is boom-and-zoom —
altitude and speed are the weapon, not the wing.

## 2. The real aircraft

P-47D Thunderbolt (Republic), the definitive production variant.
- Empty weight: 9,900 lb (4,491 kg)
- Combat/loaded weight: 14,000 lb (6,350 kg); max overload 17,000 lb (7,711 kg)
- Wing area: 300 ft² (27.87 m²); span 40 ft 9.4 in (12.43 m)
- Engine: Pratt & Whitney R-2800-59 "Double Wasp", rated ~2,000 hp continuous,
  war-emergency power up to ~2,300–2,535 hp with water injection on late blocks
- Historical W/S (combat weight): 46.7 lb/ft² ≈ 228 kg/m² — one of the heaviest
  wing loadings of any WWII single-seat fighter
- Historical power loading: 2,000 hp / 14,000 lb ≈ 0.143 hp/lb (236 W/kg)
- Vmax: 428 mph (689 km/h, 191 m/s) at 30,000 ft (a high-altitude figure —
  above this world's ~8 km soft ceiling, so treat as an upper bound)
- Stall (clean, indicated): 111–134 mph (178–215 km/h, 50–60 m/s); landing
  config 91–110 mph (146–177 km/h, 41–49 m/s)

Sources: [Republic P-47 Thunderbolt — AeroCorner](https://aerocorner.com/aircraft/republic-p-47-thunderbolt/),
[Republic P-47D Thunderbolt — Military Machine](https://militarymachine.com/ww2-aircraft/p-47d-thunderbolt),
[The Complexity of a WWII P-47 Thunderbolt's Powerplant — Lyncean Group](https://lynceans.org/all-posts/the-complexity-of-a-ww-ii-p-47-thunderbolts-powerplant/).

## 3. Arcade scaling

- **Mass/S kept at real values** (6,350 kg / 27.87 m²) — same convention this
  repo already used for vessel #1 (3,000 kg / 16.0 m² tracks a real Bf-109F
  closely). This is what carries the "heavy, high-wing-loading energy fighter"
  identity into the game.
- **Cl_max kept at 1.8**, same as vessel #1 — Cl_max here is already an arcade
  cap (real WWII Cl_max clean is ~1.3–1.5), not a literal figure; the wing-
  loading DELTA (228 vs 187.5 kg/m² for vessel #1) is what should read as
  "heavier, worse-turning" — Cl_max stays a shared constant across vessels by
  design.
- **T/W doubled from a same-methodology historical estimate.** The R-2800 has
  no literal "thrust" figure (it's a prop engine), so the historical baseline
  is estimated with the standard static-thrust approximation
  `T[lbf] = 10.41 · HP^(2/3) · D[ft]^(1/3)` (D ≈ 13 ft, the P-47's 4-blade
  Curtiss Electric prop), giving T ≈ 3,886 lbf = 17,285 N, historical
  T/W ≈ 17,285 / 62,294 N ≈ **0.28**. Arcade ×2 → **T/W ≈ 0.55**.
  **METHODOLOGY NOTE (ruling needed):** vessel #1's own "historical ≈0.31" is
  the repo's pre-rung-D `T_max/W`, not an independent figure computed the same
  way — on this SAME static-thrust formula the Bf-109F comes out at
  T/W ≈ 0.41, so vessel #1's actual arcade multiplier is ×1.5, not the clean
  ×2 this brief (and the A6M2's, and the Beaver's) applies to a
  freshly-computed baseline. A ~33% inter-vessel power-convention gap; see
  `vessel_ontology.md` §6 and this brief's §7.
- **Cd0 raised ~28% over vessel #1** (0.025 → 0.032): the P-47's big radial
  cowl and large frontal area are real drag, and nothing else in this table
  captures airframe bulk.
- **k_induced held at 0.015**, the same arcade energy ruling — the induced-drag
  generosity is a global convention, not a per-vessel dial.

## 4. Delta table

`config/aircraft.toml` [airframe] (deltas from vessel #1):
| field | vessel #1 | P-47 hypothesis |
|---|---|---|
| mass | 3000.0 | **6350.0** |
| S | 16.0 | **27.87** |
| Cl_max | 1.8 | 1.8 (unchanged) |
| Cd0 | 0.025 | **0.032** |
| k_induced | 0.015 | 0.015 (unchanged) |
| T_max | 18000.0 | **34000.0** (T/W ≈ 0.546) |

`[inertia]` — scaled by mass·span² against vessel #1's assumed span ≈9.92 m
(the Bf-109F this table's header cites), span ratio 12.43/9.92, mass ratio
6350/3000 → combined scale ×3.33:
| field | vessel #1 | P-47 hypothesis |
|---|---|---|
| I_pitch | 9000.0 | **30000.0** |
| I_yaw | 12000.0 | **40000.0** |
| I_roll | 6000.0 | **20000.0** |

`[authority]` — **P0-2 FIX:** the previous table scaled `c` and `damp` by the SAME
factor, which is a no-op — the steady rate `c/damp` is q-independent, so a shared
scale cancels and every "handling identity" claim below it (heavy roll, etc.) was
never actually reaching the plant. Instead: `damp` keeps its inertia-scaled value
(it sets the snap character via `α_peak = c·q/I`); `c` is derived from a CHOSEN
target steady rate per axis, `c = damp · ω_target` (`vessel_ontology.md` §6 step 4's
own rule). Targets below are hypotheses for Chad's stick: roll ~200°/s steady
(below vessel #1's 260°/s ceiling — "heavy, has to be muscled around a turn"),
pitch ~140°/s, yaw ~100°/s:
| field | vessel #1 | P-47 hypothesis |
|---|---|---|
| c_pitch | 13.0 | **34.2** (= damp_pitch × 140°/s) |
| c_yaw | 16.0 | **41.9** (= damp_yaw × 100°/s) |
| c_roll | 24.0 | **45.4** (= damp_roll × 200°/s) |
| damp_pitch | 4.7 | **14.0** (inertia-scaled, unchanged) |
| damp_yaw | 8.0 | **24.0** (inertia-scaled, unchanged) |
| damp_roll | 4.8 | **13.0** (inertia-scaled, unchanged) |
| q_att_floor | 280.0 | 280.0 (unchanged — pure dynamic pressure, mass/S-independent) |

Peak α at V=140 (q=9800, full compression effectiveness since V=140 is below this
brief's own `v_full` 160): pitch **11.2 rad/s² (640°/s²)**, yaw **10.3 rad/s²
(588°/s²)**, roll **22.3 rad/s² (1,275°/s²)** — snappier per-tick than vessel #1
in raw torque (bigger `damp`), but the STEADY rates above are what actually reads
as "heavy" in the seat; the peak is how fast it gets there, not how far.

`[rate_clamps]` — **P0-2 FIX:** each vessel now needs its OWN ceiling instead of
inheriting vessel #1's shared 260/55 — at the corrected `c` values above, leaving
`p_max`/`yaw_max` at 260/55 would just let the P-47 reach vessel #1's own roll
rate and erase the "heavier" identity a second time:
| field | vessel #1 | P-47 hypothesis |
|---|---|---|
| p_max | 260.0 | **220.0** deg/s — above the 200°/s roll target (non-binding), still well under vessel #1's 260 |
| yaw_max | 55.0 | **50.0** deg/s — ⚠ yaw_max is the INSTRUCTOR/override rudder ceiling (vessel #1's 55 is a Chad-ruled nerf, not a plant limit); the plant's steady c/damp yaw (~100°/s) stays available to physics, but the pointed/keyboard rudder must NOT exceed the flown fighter's ceiling on a heavier ship — 50, slightly under #1's 55 |

`[compression]` — raise the knee: the P-47 was legendarily strong in a dive
(structurally, though real pilots still hit true compressibility above ~500
mph); reward diving-away play instead of punishing it early:
| field | vessel #1 | P-47 hypothesis |
|---|---|---|
| v_full | 140.0 | **160.0** |
| v_redline | 245.0 | **290.0** |

`config/controller.toml` — the inner-loop gains MUST re-derive with I (SPEC's
stability walls: `K_w ≥ 4·I·K_theta_axis` and `K_w·sim_dt/I ≤ 0.5`, sim_dt=1/120;
pitch/yaw checked at `K_theta` = 3.2, **roll checked at `K_phi` = 5.0, NOT
`K_theta`** — the roll outer loop uses its own gain, `config/load_controller.cpp`):
| axis | I | legal K_w range | starting hypothesis (scaled ×3.33 with vessel #1) |
|---|---|---|---|
| pitch | 30000 | [384,000, 1,800,000] | K_w 500,000 / K_wi 200,000 |
| yaw | 40000 | [512,000, 2,400,000] | K_w 670,000 / K_wi 83,333 |
| roll | 20000 | [400,000, 1,200,000] | K_w 480,000 / K_wi 50,000 |

**P1-5 FIX:** the roll floor is `4·I_roll·K_phi = 4·20000·5.0 = 400,000`, not the
256,000 a `K_theta`-based formula gave. The proposed `K_w_roll` is bumped ~20%
above that corrected floor (400,000 → 480,000) instead of sitting exactly on it —
a zero-margin load is fragile under any future retune of `K_phi` or `I_roll`.

`[g_limits]`: n_max 28.0, n_min -8.0 (slightly under vessel #1's 32/-9 — a
heavier airframe, less snap-G, more of a "won't out-turn you" identity; the
real airframe's legendary structural toughness is a durability trait this v1
game has no damage model to express, so it isn't encoded here).

Everything else (deadzone, pursuit, capture/rimshot, lean, coordination,
auto_level, push_gate, latches) — **re-derive on the stick, start at vessel
#1's values.** These are feel dials, not physics-derived, and CLAUDE.md's "one
dial at a time, Chad flies every change" rule applies in full once this
vessel is actually flown.

## 5. Envelope targets (harness-verifiable)

All formulas use this world's `rho = 1.0`, `g = 9.81`.

- **Stall V** = √(2·m·g / (ρ·S·Cl_max)) = √(2·62,294 / (1·27.87·1.8))
  = √2,483.5 = **49.8 m/s** (real clean stall midpoint ≈ 54.7 m/s — same order,
  the gap is the rho=1.0 vs real 1.225 kg/m³ convention already baked into
  vessel #1).
- **Corner V** = stall V · √n_max = 49.8 · √28 = **263.5 m/s** (where Cl_max
  meets the proposed n_max=28 G-cap; unlike vessel #1's 255.7 m/s corner
  (which sits PAST its 245 m/s redline and is therefore unreachable), the
  P-47's 263.5 m/s corner sits INSIDE its proposed 290 m/s redline —
  reachable, not just academic, though the compression knee still reduces
  effective authority well before it in practice).
- **Level top speed** (T = D): solving
  `0.5·ρ·S·Cd0·V² + 2·k·(m·g)² / (ρ·S·V²) = T_max` (quadratic in V²; this exact
  form reproduces vessel #1's own documented "T=9000 → ~205-210 m/s" note in
  aircraft.toml when its numbers are substituted, confirming the method) gives
  **V ≈ 276 m/s (≈ 993 km/h)** at T_max=34,000 N. NOTE: this is **24 m/s SLOWER**
  than vessel #1's own current ~299.8 m/s (`vessel_ontology.md` §5 #14) — if
  Chad wants the P-47 historically-truer "clearly faster in a straight line
  than the interceptor" read, raise T_max toward ~40–42 kN (T/W ≈ 0.65–0.68).
- **Best sustained-turn speed estimate**: the speed where the lift-limited
  turn-rate curve (`n_Cl(V) = Cl_max·0.5·ρ·S·V²/(m·g)`) crosses the
  thrust-limited curve (`n_thrust(V)` solved from the same T=D quadratic at
  general n) — below it turning is lift-limited (more speed = more available
  G), above it thrust-limited (more speed = less sustainable G). Numerically
  this crossover lands at **V ≈ 174 m/s, n ≈ 12.2, turn rate ≈ 40°/s** — the
  P-47's best sustained corner, well below its 264 m/s structural corner speed.

## 6. Instrument gaps

The harness has no mode to directly verify stall V, level top speed, or the
sustained-turn crossover above — `step`/`track` probe rate response at a FIXED
V, and `ctrl_fly` runs one AT-12-style sustained turn at fixed conditions, not
a sweep. Missing instruments (in-scope future work, not a tuning task):
1. A **stall-speed mode**: hold 1g level flight, ramp V down, report the V at
   which the plant can no longer hold altitude (Cl saturates at Cl_max).
2. A **top-speed mode**: hold level flight, throttle to 1.0, report the
   converged equilibrium V (verifies the T=D quadratic against the real plant,
   not just the algebra).
3. A **sustained-turn-rate sweep**: run `ctrl_fly`'s closed-loop max-rate turn
   at a range of V, report achieved n(V) and turn-rate(V) — would directly
   confirm or refute the crossover-speed estimate above against the real
   cascade (not just the open-loop energy math).

## 7. The competitive dynamic

Against vessel #1 (the balanced interceptor): the P-47 loses any sustained
turning fight (263.5 m/s corner vs vessel #1's ~256 m/s is close on paper, but
the P-47's REDUCED roll authority — heavier ailerons means LESS authority, a
220°/s ceiling vs vessel #1's 260°/s — and lower n_max mean it gets there
slower and holds less G once there) — it must disengage vertically, not
horizontally. Against the A6M2: never, ever turn with it below ~180 m/s; the
Zero's corner speed is 24% lower and its sustained turn rate is roughly
double. The P-47's answer is energy — dive away, climb back, re-engage on its
terms, and use the raised compression knee to keep control authority in the
dive that kills a chasing Zero's ailerons first. Against the bushplane: not a
fight, it's a strafing run — the Beaver has neither the speed nor the G to
evade.

**RULING NEEDED (P1-2, methodology):** this brief's T/W convention (arcade ×2
on a freshly-computed static-thrust baseline) is NOT the same convention
vessel #1 was built on (vessel #1 measures only ×1.5 on the identical
formula) — a ~33% inter-vessel power inconsistency Chad must rule on before
these vessels' numbers are treated as comparably "arcade-scaled." See
`vessel_ontology.md` §6.

