# Vessel brief — de Havilland DHC-2 Beaver ("the bushplane")

Status: HYPOTHESIS. This is a delta table for a future Chad-flown sandbox, not
a tuned kernel. No config file is touched by this brief. Feel dials (deadzone,
pursuit, capture/rimshot, lean, coordination, compression's exact shape) are
NOT derived here — they start at vessel #1's values and get re-derived on the
stick, one dial at a time, same as every other feel thread in this repo. This
is the Whitewater Lake nordic float/ski archetype Chad named — the utility
plane, not a fighter.

## 1. Feel

Not a dogfighter and shouldn't pretend to be one — this is the plane you fly
to LOOK at the world, land somewhere absurd, or ferry something in. It's
heavy in roll (long span, big draggy struts and bracing), gentle in pitch, and
utterly unbothered by a stall — it should be nearly impossible to get hurt in.
If it ever ends up in a fight it should feel exactly like what it is: a
target. The interesting design question isn't "how does it fight" but "how
slow and forgiving can it be while still using the same plant/controller
math as everything else."

## 2. The real aircraft

de Havilland Canada DHC-2 Beaver (piston, wheel/float/ski-capable).
- Empty weight: 3,450 lb (1,565 kg) — **floatplane/STC-config figure**; the
  canonical landplane empty weight is 3,000 lb
- Max gross weight: 5,600 lb (2,540 kg) — **floatplane/STC-config figure**; the
  canonical landplane max gross is 5,100 lb (this brief keeps the heavier
  floatplane numbers below, matching the "Whitewater Lake nordic float/ski
  archetype" Chad named in the status line above)
- Wing area: 250 ft² (23.23 m²); span 48 ft 0 in (14.63 m)
- Engine: Pratt & Whitney R-985 "Wasp Junior", ~450 hp
- Historical W/S (max gross): ~22.4 lb/ft² ≈ 109 kg/m² — a light wing loading,
  the STOL/utility design goal, coincidentally close to the A6M2's
- Historical power loading: 450 hp / 5,600 lb ≈ 0.080 hp/lb (135 W/kg) — by
  far the lowest of the four vessels; this plane was never built to be fast
- Cruise: ~130 kn (240 km/h, 67 m/s); stall clean 60 mph (97 km/h, 27 m/s),
  full flap 45 mph (72 km/h, 20 m/s)

Sources: [de Havilland DHC-2 Beaver — AeroCorner](https://aerocorner.com/aircraft/de-havilland-dhc-2-beaver/),
[BEAVER DHC-2 Specifications — GlobalAir](https://www.globalair.com/aircraft-specifications/de-havilland/beaver-dhc-2-specifications/851),
[de Havilland Canada DHC-2 Beaver — Wikipedia](https://en.wikipedia.org/wiki/De_Havilland_Canada_DHC-2_Beaver).

## 3. Arcade scaling

- **Mass/S kept at real values** (2,540 kg / 23.23 m² — max gross weight, same
  "loaded" convention as the other two briefs). The real W/S (109 kg/m²) is
  low, close to the A6M2's — but the bushplane's identity should read as
  DRAGGY and PONDEROUS, not nimble, so that distinction is carried by Cd0,
  inertia, and authority instead (below), not by wing loading.
- **Cl_max raised slightly** to 2.0 (vs 1.8 elsewhere) — the one vessel where
  this is historically justified: a braced STOL wing section is literally
  designed for a high Cl_max, and this repo's own `[flaps]` mechanism
  (`dCl_flap = 0.8`) will stack on top of it at the combat/landing detents,
  so a strong clean Cl_max here pays off doubly for slow, forgiving flight.
- **T/W doubled from a same-methodology historical estimate, WITH A FLAGGED
  TENSION** (see §5 top-speed note): static-thrust approximation
  `T[lbf] = 10.41·HP^(2/3)·D[ft]^(1/3)` (D ≈ 8.5 ft / 2.59 m, the R-985's
  Hamilton Standard prop) gives T ≈ 1,248 lbf = 5,550 N, historical
  T/W ≈ 5,550 / 24,917 N ≈ **0.22**. Arcade ×2 → **T/W ≈ 0.44**. Applying the
  SAME convention as the fighters is honest and keeps the methodology
  uniform, but it makes the plant CAPABLE of an unrealistic ~520 km/h in level
  flight (§5) — the resolution proposed here is NOT to break the convention,
  but to pair it with a low, early compression knee (§4) so the controller
  makes the plane mushy and hard to fly well before it gets there. The plant
  stays honest; the felt top speed stays bushplane-slow.
- **Cd0 raised substantially** (0.025 → 0.045, ~1.8×): floats/skis, external
  struts, and bracing wires are real, large parasitic drag that nothing else
  in this table captures.
- **k_induced held at 0.015**, the global arcade energy ruling.

## 4. Delta table

`config/aircraft.toml` [airframe] (deltas from vessel #1):
| field | vessel #1 | Beaver hypothesis |
|---|---|---|
| mass | 3000.0 | **2540.0** |
| S | 16.0 | **23.23** |
| Cl_max | 1.8 | **2.0** |
| Cd0 | 0.025 | **0.045** |
| k_induced | 0.015 | 0.015 (unchanged) |
| T_max | 18000.0 | **11000.0** (T/W ≈ 0.441) |

`[inertia]` — scaled by mass·span² against vessel #1's assumed span ≈9.92 m,
span ratio 14.63/9.92, mass ratio 2540/3000 → combined scale ×1.833:
| field | vessel #1 | Beaver hypothesis |
|---|---|---|
| I_pitch | 9000.0 | **16500.0** |
| I_yaw | 12000.0 | **22000.0** |
| I_roll | 6000.0 | **11000.0** |

`[authority]` — **P0-2 FIX:** the previous table scaled `c` and `damp` by the SAME
factor (a ×1.833 inertia scale plus a ×0.5/×0.75 "derate" on top), which is a
no-op — steady rate `c/damp` is q-independent, so any shared multiplier on both
cancels and the "heavy ailerons" claim never actually reached the plant.
Instead: `damp` keeps its inertia-scaled value (sets the snap character via
`α_peak = c·q/I`); `c` is derived from a CHOSEN target steady rate per axis,
`c = damp · ω_target` (`vessel_ontology.md` §6 step 4's own rule). Targets
below are hypotheses for Chad's stick — genuinely heavy everywhere: roll
~120°/s steady, pitch ~90°/s, yaw ~70°/s:
| field | vessel #1 | Beaver hypothesis |
|---|---|---|
| c_pitch | 13.0 | **10.2** (= damp_pitch × 90°/s) |
| c_yaw | 16.0 | **13.4** (= damp_yaw × 70°/s) |
| c_roll | 24.0 | **9.2** (= damp_roll × 120°/s; well below vessel #1's 24.0 — the roll target itself is what reads as heavy now, not a scale trick) |
| damp_pitch | 4.7 | **6.5** (inertia-scaled, unchanged) |
| damp_yaw | 8.0 | **11.0** (inertia-scaled, unchanged) |
| damp_roll | 4.8 | **4.4** (inertia-scaled, unchanged) |
| q_att_floor | 280.0 | 280.0 (unchanged — pure dynamic pressure, mass/S-independent) |

Peak α at V=140 (q=9800, but this brief's `v_redline`=110 means V=140 is past
redline, `delta_max_eff` clamped at `min_frac`=0.25): pitch **1.5 rad/s²
(87°/s²)**, yaw **1.5 rad/s² (85°/s²)**, roll **2.1 rad/s² (117°/s²)** —
tiny even in raw torque terms, before the steady-rate targets above even come
into it; the Beaver is choked long before it reaches combat speed.

`[rate_clamps]` — **P0-2 FIX:** own ceiling instead of inheriting vessel #1's
260/55:
| field | vessel #1 | Beaver hypothesis |
|---|---|---|
| p_max | 260.0 | **120.0** deg/s — matches the 120°/s roll target exactly |
| yaw_max | 55.0 | **35.0** deg/s — ⚠ yaw_max is the INSTRUCTOR/override rudder ceiling (vessel #1's 55 is a ruled nerf); a Beaver must not out-rudder the fighter — 35 keeps the felt rudder genuinely heavy while the ~70°/s steady plant rate remains for physics (β bleed, S-wvane) |

`[compression]` — LOWER the knee aggressively, and lower `min_frac` too: this
is the honest fix for the "arcade plant can theoretically hit 520 km/h" tension
above — the plane should feel like it's running out of control authority well
before it's actually going fast for a fighter:
| field | vessel #1 | Beaver hypothesis |
|---|---|---|
| v_full | 140.0 | **70.0** |
| v_redline | 245.0 | **110.0** |
| min_frac | 0.4 | **0.25** |

`config/controller.toml` — inner-loop gains re-derived against the stability
walls (`K_w ≥ 4·I·K_theta_axis`, `K_w·sim_dt/I ≤ 0.5`, sim_dt=1/120; pitch/yaw
checked at `K_theta`=3.2 — note K_theta itself is a strong candidate to LOWER
for this vessel, a lazier pointing response fits "gentle utility plane" better
than the fighters' snap; the range below is checked at the shared 3.2 baseline
for consistency — **roll checked at `K_phi`=5.0, NOT `K_theta`**, the roll
outer loop's own gain):
| axis | I | legal K_w range | starting hypothesis (scaled ×1.833 with vessel #1) |
|---|---|---|---|
| pitch | 16500 | [211,200, 990,000] | K_w 275,000 / K_wi 110,000 |
| yaw | 22000 | [281,600, 1,320,000] | K_w 366,667 / K_wi 45,833 |
| roll | 11000 | [220,000, 660,000] | K_w 264,000 / K_wi 27,500 |

**P1-5 FIX:** the roll floor is `4·I_roll·K_phi = 4·11000·5.0 = 220,000`, not
the 140,800 a `K_theta`-based formula gave. The proposed `K_w_roll` is bumped
~20% above that corrected floor (220,000 → 264,000) instead of sitting
exactly on it — a zero-margin load is fragile under any future retune.

`[g_limits]`: n_max 6.0, n_min -3.0 — a sharp break from every fighter in this
set (real utility-category limits are roughly +3.8/-1.5 g; ~×1.6 arcade
generosity here, not the fighters' ×3–4). This is the vessel's real identity
lever: it should be nearly impossible to hurt this airframe by pulling hard,
because it should be nearly impossible to pull hard enough to matter.

Everything else (deadzone, pursuit, capture/rimshot, lean, coordination,
auto_level, push_gate, latches) — **re-derive on the stick, start at vessel
#1's values.**

## 5. Envelope targets (harness-verifiable)

- **Stall V** = √(2·m·g / (ρ·S·Cl_max)) = √(2·24,917 / (1·23.23·2.0))
  = √1,072.6 = **32.8 m/s** clean (real clean stall was 26.8 m/s — the gap is
  mostly the rho=1.0 vs real 1.225 kg/m³ convention plus using max-gross mass;
  full combat/landing flap (`dCl_flap=0.8` stacking to an effective
  Cl_max≈2.8) would drop this well below 30 m/s, matching the real airplane's
  STOL character).
- **Corner V** = stall V · √n_max = 32.8 · √6 = **80.3 m/s** — by a wide
  margin the lowest corner speed of any vessel here, consistent with "not
  built to pull G."
- **Level top speed** (T = D, same quadratic form as the other briefs):
  **V ≈ 145 m/s (≈ 521 km/h)** at T_max=11,000 N by the raw plant math — the
  flagged tension from §3. With the proposed compression knee (v_full=70,
  v_redline=110, min_frac=0.25) the plane loses most control effectiveness
  well below that speed, so the PRACTICAL top speed (what the controller can
  actually hold on target while diving/leveling) should read far lower — this
  is exactly the kind of claim item 6's missing top-speed instrument would
  let Chad verify against the real plant instead of the open-loop algebra.
- **Best sustained-turn speed estimate**: with n_max=6 far below where the
  thrust-limited turn curve would ever bind (the fighters' thrust-vs-lift
  crossover happens around n≈12–20; this vessel's structural cap of 6 is hit
  first), the sustained turn is **lift-limited above ~55 m/s** (below that the
  wing can't reach n=6 at all and the turn is thrust/AoA-limited instead) —
  best sustained turn sits right at corner speed, **V ≈ 80.3 m/s, n = 6, turn
  rate ≈ 0.722 rad/s ≈ 41.4°/s**. (Not a typo that this lands near the P-47's
  turn rate — it's a much smaller, tighter-radius turn at a third the speed,
  which is the honest STOL-wing story: modest turn RATE, very small turn
  RADIUS.)

## 6. Instrument gaps

Same three gaps named in the P-47/A6M2 briefs — no harness mode measures
stall V, level top speed, or a sustained-turn-rate sweep directly:
1. A **stall-speed mode** (ramp V down in 1g level flight, report the V where
   altitude can no longer be held) — would also be the natural place to
   verify the flaps-stacked stall speed claim above.
2. A **top-speed mode** (throttle to 1.0 in level flight, report converged V)
   — the most load-bearing gap for THIS vessel specifically: it's the only
   way to check whether the lowered compression knee actually tames the
   ~521 km/h plant math into a felt bushplane top speed, or whether the knee
   needs to go lower still.
3. A **sustained-turn-rate sweep** — lower priority here than for the
   fighters, since this vessel isn't meant to fight, but still useful to
   confirm the lift-limited-everywhere claim above.

## 7. The competitive dynamic

Not a combatant. Against any of the three fighters, the Beaver has no
defense worth naming — it's slower to accelerate, can't out-turn even the
P-47 (80.3 m/s corner vs 263.5 m/s — the P-47 turns at more than 3× the speed
but also a much larger radius, so even that comparison undersells how badly
this loses), and its 6g structural cap means it can't even attempt the kind of
snap-G defensive break the fighters use. Its role in this world is a target,
a mission objective, or Chad's own transport between fights — not something
that should ever need a "how does it fight vessel #1/#2" answer. If a future
sandbox wants it to be survivable against fighters, the honest lever is speed
(more T_max) or numbers (many drones), not G authority.

**RULING NEEDED (P1-2, methodology):** this brief's T/W convention (arcade ×2
on a freshly-computed static-thrust baseline) is NOT the same convention
vessel #1 was built on (vessel #1 measures only ×1.5 on the identical
formula) — a ~33% inter-vessel power inconsistency Chad must rule on before
these vessels' numbers are treated as comparably "arcade-scaled." See
`vessel_ontology.md` §6.

