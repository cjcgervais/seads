# Vessel brief — A6M2 Zero Model 21 ("the turn fighter")

Status: HYPOTHESIS. This is a delta table for a future Chad-flown sandbox, not
a tuned kernel. No config file is touched by this brief. Feel dials (deadzone,
pursuit, capture/rimshot, lean, coordination, compression's exact shape) are
NOT derived here — they start at vessel #1's values and get re-derived on the
stick, one dial at a time, same as every other feel thread in this repo.

## 1. Feel

Light on the stick and eager to turn — this is the plane that wins a merge by
turning inside everything else in the sky and staying there. It climbs and
turns like nothing else, and the corner comes up fast and low. The trade is
honest: it's a paper kite. Anything with real speed that refuses to turn and
just keeps diving through will out-run and out-dive it every time, and if the
Zero ever tries to follow that dive its ailerons go stiff and heavy right when
it needs them light. Fight it low and slow, in circles, never in a race.

## 2. The real aircraft

A6M2 Zero (Mitsubishi), Type 0 Model 21.
- Empty weight: 3,704 lb (1,680 kg)
- Loaded weight: 5,313 lb (2,410 kg)
- Wing area: 241.5 ft² (22.44 m²); span 39 ft 4 in (11.99 m)
- Engine: Nakajima Sakae 12, 940 hp
- Historical W/S (loaded): ~22 lb/ft² ≈ 107 kg/m² — one of the LIGHTEST wing
  loadings of any WWII fighter, the source of its legendary turn/climb
- Historical power loading: 940 hp / 5,313 lb ≈ 0.177 hp/lb (291 W/kg — light
  power loading too, hence the exceptional climb)
- Vmax: 331 mph (533 km/h, 148 m/s)
- Stall: well below 60 kn (111 km/h, 31 m/s) — famous for docile, low-speed
  handling

Sources: [Mitsubishi A6M Zero specifications — LiquiSearch](https://www.liquisearch.com/mitsubishi_a6m_zero/specifications_a6m2_type_0_model_21),
[Mitsubishi A6M Zero — AeroCorner](https://aerocorner.com/aircraft/mitsubishi-a6m-zero/),
[Mitsubishi A6M2 Zero — WW2 Weapons](https://www.ww2-weapons.com/mitsubishi-a6m2-zero/).

## 3. Arcade scaling

- **Mass/S kept at real values** (2,410 kg / 22.44 m²) — same convention as
  vessel #1 and the P-47 brief. The very low real W/S (107 kg/m² vs vessel
  #1's 187.5 and the P-47's 228) is exactly the identity to carry through —
  lowest stall/corner speed of the three fighters, by construction.
- **Cl_max kept at 1.8**, same shared arcade cap as every other vessel — the
  low wing loading alone (not a Cl_max bump) does the turn-fighter work.
- **T/W doubled from a same-methodology historical estimate.** Static-thrust
  approximation `T[lbf] = 10.41·HP^(2/3)·D[ft]^(1/3)` (D ≈ 9.5 ft / 2.9 m,
  the Sakae 12's 3-blade prop) gives T ≈ 2,112 lbf = 9,394 N, historical
  T/W ≈ 9,394 / 23,642 N ≈ **0.40**. This is an ARCADE CHOICE, not a
  historical-superiority claim — real-world power LOADING actually favors the
  Bf-109F (≈0.209 hp/lb) over the Zero (0.177 hp/lb, per §2); the Zero's high
  T/W here is chosen so the turn-fighter climbs and matches its reputation in
  THIS game's convention, pending the inter-vessel power-methodology ruling
  (see §3's T/W note and §7). Arcade ×2 → **T/W ≈ 0.80**, the highest of any
  vessel here including the interceptor (0.61) — matches the real-world
  reputation, by game convention rather than by historical power loading.
- **Cd0 lowered slightly** (0.025 → 0.024): a light, clean, unarmored
  airframe — not a big change, the wing loading carries almost the whole
  identity.
- **k_induced held at 0.015**, the global arcade energy ruling.

## 4. Delta table

`config/aircraft.toml` [airframe] (deltas from vessel #1):
| field | vessel #1 | A6M2 hypothesis |
|---|---|---|
| mass | 3000.0 | **2410.0** |
| S | 16.0 | **22.44** |
| Cl_max | 1.8 | 1.8 (unchanged) |
| Cd0 | 0.025 | **0.024** |
| k_induced | 0.015 | 0.015 (unchanged) |
| T_max | 18000.0 | **19000.0** (T/W ≈ 0.804) |

`[inertia]` — scaled by mass·span² against vessel #1's assumed span ≈9.92 m,
span ratio 11.99/9.92, mass ratio 2410/3000 → combined scale ×1.167:
| field | vessel #1 | A6M2 hypothesis |
|---|---|---|
| I_pitch | 9000.0 | **10500.0** |
| I_yaw | 12000.0 | **14000.0** |
| I_roll | 6000.0 | **7000.0** |

`[authority]` — **P0-2 FIX:** the previous table scaled `c` and `damp` by the SAME
factor (a ×1.167 inertia scale plus a ×1.15 "roll boost" on top), which is a
no-op — steady rate `c/damp` is q-independent, so any shared multiplier on both
cancels and the "celebrated roll rate" claim never actually reached the plant.
Instead: `damp` keeps its inertia-scaled value (sets the snap character via
`α_peak = c·q/I`); `c` is derived from a CHOSEN target steady rate per axis,
`c = damp · ω_target` (`vessel_ontology.md` §6 step 4's own rule). Targets below
are hypotheses for Chad's stick: roll ~300°/s steady AT LOW SPEED (this brief's
own compression knee, `v_full` 110, already kills it by fighting speed — the
mechanism that encodes "celebrated low-speed roll, locks up in a dive," not a
hand-tuned derate), pitch ~165°/s, yaw ~120°/s:
| field | vessel #1 | A6M2 hypothesis |
|---|---|---|
| c_pitch | 13.0 | **15.8** (= damp_pitch × 165°/s) |
| c_yaw | 16.0 | **19.5** (= damp_yaw × 120°/s) |
| c_roll | 24.0 | **33.5** (= damp_roll × 300°/s) |
| damp_pitch | 4.7 | **5.5** (inertia-scaled, unchanged) |
| damp_yaw | 8.0 | **9.3** (inertia-scaled, unchanged) |
| damp_roll | 4.8 | **6.4** (inertia-scaled, unchanged) |
| q_att_floor | 280.0 | 280.0 (unchanged — pure dynamic pressure, mass/S-independent) |

Peak α at V=140 (q=9800, but this brief's `v_full`=110 means V=140 is ALREADY
past the compression knee, `delta_max_eff(140) = 0.775`): pitch **11.4 rad/s²
(655°/s²)**, yaw **10.6 rad/s² (606°/s²)**, roll **36.4 rad/s² (2,083°/s²)** —
the roll number is the "celebrated low-speed roll" showing up honestly: it's
already down from its full-authority value (V≤110) by the time V=140 is
reached, which is the point of pairing this target with the low `v_full`.

`[rate_clamps]` — **P0-2 FIX:** own ceiling instead of inheriting vessel #1's
260/55:
| field | vessel #1 | A6M2 hypothesis |
|---|---|---|
| p_max | 260.0 | **300.0** deg/s — matches the 300°/s low-speed roll target exactly (the compression knee, not this ceiling, is what tames it at speed) |
| yaw_max | 55.0 | **60.0** deg/s — ⚠ yaw_max is the INSTRUCTOR/override rudder ceiling, and vessel #1's 55 is a Chad-ruled feel nerf; the Zero's famously lively rudder earns a modest step ABOVE it (60), not the raw 120°/s plant capability — the pointed rudder past ~65 was #1's flown "gross rudder" territory |

`[compression]` — LOWER the knee, deliberately: the real Zero's famous
weakness was aileron lock-up at high speed (thin skin, unboosted controls).
This is the vessel's designed honesty wall — it must not be allowed to dive
away and keep fighting the way the P-47 can:
| field | vessel #1 | A6M2 hypothesis |
|---|---|---|
| v_full | 140.0 | **110.0** |
| v_redline | 245.0 | **190.0** |

`config/controller.toml` — inner-loop gains re-derived against the stability
walls (`K_w ≥ 4·I·K_theta_axis`, `K_w·sim_dt/I ≤ 0.5`, sim_dt=1/120; pitch/yaw
checked at `K_theta`=3.2, **roll checked at `K_phi`=5.0, NOT `K_theta`** — the
roll outer loop uses its own gain):
| axis | I | legal K_w range | starting hypothesis (scaled ×1.167 with vessel #1) |
|---|---|---|---|
| pitch | 10500 | [134,400, 630,000] | K_w 175,000 / K_wi 70,000 |
| yaw | 14000 | [179,200, 840,000] | K_w 233,333 / K_wi 29,167 |
| roll | 7000 | [140,000, 420,000] | K_w 168,000 / K_wi 17,500 |

**P1-5 FIX:** the roll floor is `4·I_roll·K_phi = 4·7000·5.0 = 140,000`, not the
89,600 a `K_theta`-based formula gave. The proposed `K_w_roll` is bumped ~20%
above that corrected floor (140,000 → 168,000) instead of sitting exactly on
it — a zero-margin load is fragile under any future retune.

`[g_limits]`: n_max 34.0, n_min -10.0 (slightly ABOVE vessel #1's 32/-9 — the
turn fighter should be able to use every bit of its low wing loading; note
the real Zero's fragility — no armor, no self-sealing tanks — is a durability
trait, not a G-limit, and this v1 game has no damage model to express it, so
it is not encoded as a stiffer structural cap here).

Everything else (deadzone, pursuit, capture/rimshot, lean, coordination,
auto_level, push_gate, latches) — **re-derive on the stick, start at vessel
#1's values.**

DESIGN NOTE (ontology R6, first predictive use): vessel #1 RETIRED the cue-ball
capture (carry = 0, 2026-07-30) because its high-T/W plant now closes gun
solutions on raw authority. The cue-ball is a compensation that scales INVERSELY
with plant authority — and the Zero, despite its high arcade T/W, has the set's
lowest corner authority at speed (the compression knee kills it in a dive). When
this vessel's sandbox opens, EVALUATE re-arming the parked capture machinery
(carry = 1) as a candidate character dial before inventing anything new: the
machinery is preserved in code and table precisely for this case.

## 5. Envelope targets (harness-verifiable)

- **Stall V** = √(2·m·g / (ρ·S·Cl_max)) = √(2·23,642 / (1·22.44·1.8))
  = √1,170.7 = **34.2 m/s** — clearly the lowest of the three fighters (vessel
  #1: 45.2 m/s; P-47: 49.8 m/s), matching the real-world reputation directly.
- **Corner V** = stall V · √n_max = 34.2 · √34 = **199.4 m/s** — the lowest
  corner speed of the three FIGHTERS in this set (the bushplane's is lower
  still, 80.3 m/s — it isn't a fighter, so it isn't part of this comparison).
- **Level top speed** (T = D, same quadratic form as the P-47 brief):
  **V ≈ 265 m/s (≈ 956 km/h)** at T_max=19,000 N — comparable to the other
  fighters' arcade top speeds despite the much lower absolute thrust, because
  the light, clean airframe pays little drag for it. If this reads as "too
  fast for a plane that's supposed to lose the straight-line race," the
  compression knee (already lowered above) is the intended lever, not T_max —
  the Zero should be ABLE to hit that number briefly but lose control
  authority doing it.
- **Best sustained-turn speed estimate**: the lift-limited/thrust-limited
  turn-rate crossover (same method as the P-47 brief) lands at
  **V ≈ 152 m/s, n ≈ 20, turn rate ≈ 74°/s** — roughly double the P-47's ~40°/s
  and at a much lower speed, the sharpest possible statement of "know your
  plane": the Zero should never try to win a fight above ~150 m/s, and should
  never try to win one by going faster.

## 6. Instrument gaps

Same three gaps as the P-47 brief (no dedicated harness mode measures stall V,
level top speed, or a sustained-turn-rate sweep — only fixed-condition
`step`/`track`/`ctrl_fly` runs exist today):
1. A **stall-speed mode** (ramp V down in 1g level flight, report the V where
   altitude can no longer be held).
2. A **top-speed mode** (throttle to 1.0 in level flight, report converged V).
3. A **sustained-turn-rate sweep** (repeat `ctrl_fly`'s closed-loop max-rate
   turn across a range of V, report n(V)/turn-rate(V)) — this is the one that
   would matter most for the Zero specifically, since its whole identity is
   the shape of that curve versus the other vessels'.

## 7. The competitive dynamic

This is the vessel that makes "know your plane vs your opponent's" concrete.
Against vessel #1: the Zero out-turns it comfortably (199 m/s corner vs ~256
m/s, ~74°/s sustained turn rate is a large multiple of what the interceptor
can hold) — WIN by dragging any fight down to a slow circle below ~150 m/s and
staying there; NEVER let the interceptor's superior compression knee (v_full
140 vs the Zero's 110) turn the fight into a speed contest. Against the P-47:
the same rule but sharper — the P-47 doubles down on being an energy fighter
with a raised compression knee specifically so it can dive and keep flying;
the Zero MUST NOT dive after it (its own knee is lowered specifically so this
goes badly) and must instead force the merge low and slow where its 74°/s
sustained turn is worth far more than the P-47's 40°/s. Against the bushplane:
irrelevant as a dogfight — the Beaver has neither the speed nor turn
performance to be a threat; treat it as a target, not an opponent.

**RULING NEEDED (P1-2, methodology):** this brief's T/W convention (arcade ×2
on a freshly-computed static-thrust baseline) is NOT the same convention
vessel #1 was built on (vessel #1 measures only ×1.5 on the identical
formula) — a ~33% inter-vessel power inconsistency Chad must rule on before
these vessels' numbers are treated as comparably "arcade-scaled." See
`vessel_ontology.md` §6.

