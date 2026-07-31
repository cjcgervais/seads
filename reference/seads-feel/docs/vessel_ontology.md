# Vessel Ontology — Vessel #1, the SEADS fighter

*The relational knowledge base for SEADS airframes. Every number is cited from the LIVE
config at the date below; every derivation shows its work so the next reader can re-run it.*

**As of 2026-07-30, branch `feel/kernel-v5`.** Sources: `config/aircraft.toml`,
`config/controller.toml`, `sim/aero.h`, `control/controller.cpp`, `docs/flight-log.md`.
R = 15 000 m, g = 9.81, rho = 1.0, sim_dt = 1/120 s (`aircraft.toml [world]`).

---

## 1. Feel — what Vessel #1 is, in the cockpit

She is a **hot-rodded 109/51-class single-seater with about twice the engine history gave
her**. Chad ruled that in on 2026-07-23 (rung D) and it is the single fact that most defines
how she flies: *"energy drops massively in turns for an arcade fighter... give me the power,
the airframe is being underserved."*

What that buys you in the seat:

- **She does not bleed.** At fighting speed you can hold the wing right up against its stall
  limit and the engine still pays for it. Below ~180 m/s a max-lift turn is *free* — the
  throttle covers the drag, you fly the whole circle at the same speed. Historical fighters
  make you choose between turn rate and staying fast; this one mostly doesn't.
- **She gets tighter the faster you go.** There is no classic corner speed inside the
  envelope — turn rate climbs monotonically with speed all the way to redline. The wing,
  not a structural G cap, is what ends the pull.
- **Nose authority is disproportionate to the airframe.** The roll answers like a much
  smaller aeroplane (~285°/s available, clamped to 260), pitch snaps rather than swings,
  and the rudder is real — it moves the flight path, not just the nose (`Cy_beta`, the
  S-wvane side force).
- **She gets heavy in a steep dive and only there.** Full control effectiveness holds to
  ~500 km/h; past that the stick progressively goes stiff, bottoming out at 40% around
  880 km/h. The dive is a judgement game: too steep and you may not make the pull-out.
- **She runs out of air, she does not hit a wall.** Above 4 km lift, thrust and control
  authority all thin together on one Gaussian; by 7 km she is a perch, not a turn-fighter.
  (STALE CAVEAT: this atmosphere table is computed at the pre-rung-D `T_max = 9000` baseline —
  the live `T_max = 18000` service ceiling is ~9.5 km, per §5 #15. Treat "7 km" as
  illustrative feel-shape, not the current number.)
- **She is docile slow.** Stall around 45 m/s clean, 37 with landing flap out.

If you want the one-line archetype: **an energy-fighter airframe with a turn-fighter's
budget, and the arcade engine that lets you fly it either way.**

---

## 2. Coordinates of the vessel

### 2.1 Mass, wing, engine

| Coordinate | Value | Provenance |
|---|---|---|
| mass `m` | 3000 kg | `aircraft.toml [airframe]` |
| wing area `S` | 16.0 m² | `aircraft.toml [airframe]` |
| `T_max` | 18 000 N | `aircraft.toml [airframe]` — v5 RUNG D, was 9000 |
| `Cl_max` | 1.8 | `aircraft.toml [airframe]` (§7 tune 2026-07-05, was 1.4) |
| `Cl_alpha` | 5.0 /rad | `aircraft.toml [airframe]` |
| `Cd0` | 0.025 | `aircraft.toml [airframe]` |
| `k_induced` | 0.015 | `aircraft.toml [airframe]` — v5 RUNG D, was 0.05 |
| `Cy_beta` / `Cd_beta` | 2.5 / 0.6 | `aircraft.toml [airframe]` (S-wvane) |
| inertia `I_pitch/I_yaw/I_roll` | 9000 / 12 000 / 6000 kg m² | `aircraft.toml [inertia]` |

Derived:

- Weight `W = m·g = 3000 × 9.81 =` **29 430 N**
- **T/W = 18 000 / 29 430 = 0.612** — the "historical ≈0.31" this repo has always quoted here
  is the repo's OWN pre-rung-D `T_max/W` (9000/29430 = 0.306), NOT an independently-sourced
  historical figure. On the SAME static-thrust convention the vessel briefs use
  (`T[lbf] = 10.41·HP^(2/3)·D[ft]^(1/3)`), the Bf-109F comes out at **T/W ≈ 0.41**, making
  vessel #1's true arcade multiplier **×1.5**, not ×2 (see §6 methodology note). **RULING
  NEEDED:** the P-47/A6M2/Beaver briefs each apply a clean ×2 to a freshly-computed
  static-thrust baseline; vessel #1 only reaches ×1.5 on the same formula — a ~33%
  inter-vessel power-convention gap.
- **W/S = 29 430 / 16 = 1839 N/m²** = **187.5 kg/m²** (Bf 109 F-4 ≈ 175, P-51D ≈ 193 —
  vessel #1 sits between them; the wing is historically honest, the engine is not)
- stall AoA `= Cl_max/Cl_alpha = 1.8/5.0 =` 0.36 rad = **20.63°**
- best L/D `= Cl*/Cd*` with `Cl* = sqrt(Cd0/k) = sqrt(1.6667) = 1.291`, `Cd* = 2·Cd0 = 0.05`
  → **L/D_max = 25.8**, minimum drag `D_min = W/(L/D) =` **1140 N**

### 2.2 Atmosphere / compression

`atm_frac(h) = exp(−(h − 4000)²/(2·2340²))` for h > 4000 (`[atmosphere]`, `sim/aero.h`).
`delta_max_eff(V)` = 1 below `v_full` 140, linear to `min_frac` 0.4 at `v_redline` 245
(`[compression]`, `sim/aero.h::delta_max_eff`).

### 2.3 The authority triplet

`τ = c_axis · max(q, q_att_floor) · delta_max_eff(V) · Input` (SPEC §7, `sim/aero.h`).
Damping: `τ_d = −damp_axis · max(q, q_att_floor) · ω`.

| axis | `c` | `damp` | `I` | peak α at V=140 | steady rate at full input |
|---|---|---|---|---|---|
| pitch | 13.0 | 4.7 | 9000 | 14.16 rad/s² (811°/s²) | `c/damp` = 2.766 rad/s = **158°/s** |
| yaw | 16.0 | 8.0 | 12 000 | 13.07 rad/s² (749°/s²) | 2.000 rad/s = **114.6°/s** |
| roll | 24.0 | 4.8 | 6000 | 39.20 rad/s² (2246°/s²) | 5.000 rad/s = **286.5°/s** |

`q_att_floor = 280 Pa` → floor crossover `sqrt(2·280/rho) =` **23.66 m/s**.
Note the steady rate `c/damp` is **q-independent** — dynamic pressure cancels; only the
compression ramp scales it (×0.4 at redline: pitch 63°/s, roll 115°/s).

### 2.4 G limits and protection

`[g_limits] n_max = 32.0`, `n_min = −9.0`; `[aoa] aoa_max = aoa_max_neg = 20.0°`,
`K_aoa = 10.0`, `filter_tau = 0.05 s`.
Pitch has **no separate rate clamp** — the G limits *are* it
(`controller.cpp:376-377`): `w_max_pitch = (n_max − cosΦθ)·g/V_clamp`,
`w_min_pitch = (n_min − cosΦθ)·g/V_clamp`, `V_clamp = max(V, v_min 10)`.

### 2.5 The cascade

| Dial | Value | File |
|---|---|---|
| `K_theta` (pitch+yaw pointing) | 3.2 /s | `controller.toml [outer]` |
| `K_phi` (roll) | 5.0 /s | `[outer]` |
| `k_b` (braking margin) | 0.7 | `[outer]` |
| `yaw_max` / `p_max` / `omega_bal` | 55 / 260 / 30 °/s | `[rate_clamps]` |
| `K_w_pitch/yaw/roll` | 150k / 200k / 120k N m s | `[inner]` |
| `K_wi_pitch/yaw/roll` | 60k / 25k / 15k N m | `[inner]` |
| `integ_cap` | 2.0 rad s | `[inner]` |
| `damp_ff_pitch/yaw/roll` | 1.0 / 1.0 / 1.0 | `[inner]` — full plant inversion, all three axes |
| `[pursuit] step / expo` | 0.0 / 3.0 | `[pursuit]` (step = NAMED WALL) |
| `[aim_ff] gain / tau` | 0.3 / 0.01 | S-aimff |
| `[deadzone] lo / hi / rest_dwell` | 0.03° / 0.05° / 0.15 s | `[deadzone]` |
| `[auto_level] rate / lean_gain / lean_max` | 5.5 /s / 8.0 / 30° | `[auto_level]` |
| `[auto_level] inverted_delay / rate` | 0.5 s / 180 °/s | `[auto_level]` |
| `[coordination] K_coord / center_frac / center_band / yaw_scale` | 1.0 / 0.0 / 1.5° / 2.0 | `[coordination]` |
| `[regime] blend_lo/hi / bank_align_power / pull_floor` | 5°/9° / 6.0 / 1.0 | `[regime]` |
| `[capture]` carry / rim_frac / circle_deg / return_w / engage_frac | 1.0 / 1.0 / 0.55° / 400°/s / 0.95 | S-rimshot v3 POOL BALL |
| `[capture]` glance_frac / depth_frac / depth_pow | 0.25 / 1.5 / 2.0 | `[capture]` |
| `[ui] aim_sensitivity` | 0.14 °/px | `[ui]` — Chad: *"thats the perfect pot right there"* |

Pointing law (`controller.cpp:26`):
`sqrt_law(e,K,aB,w_max) = sign(e)·min(K|e|, sqrt(2·aB·|e|), w_max)`, with
`aB = k_b · ang_accel_max_derived(...)`. Yaw uses `K_theta·yaw_scale` **inside** the gain
slot, so `yaw_max` stays a true shared ceiling with the keyboard override.

---

## 3. The relations

### R1 — Power ↔ sustained turn
Level sustained turn at load factor n: `T = q·S·Cd0 + k·n²W²/(q·S)`. Solve for n:

| V | q·S | thrust-limited n | lift-limited n (at `aoa_max`, Cl = 1.745) | binding | turn rate ω = g·sqrt(n²−1)/V | radius |
|---|---|---|---|---|---|---|
| 100 | 80 000 | 9.9 | **4.75** | LIFT | 26.1 °/s | 220 m |
| 140 | 156 800 | 13.0 | **9.30** | LIFT | 37.1 °/s | 216 m |
| 175 | 245 000 | 15.0 | **14.53** | LIFT | 46.5 °/s | 216 m |
| 200 | 320 000 | **15.7** | 18.98 | THRUST | 44.0 °/s | 260 m |
| 245 | 480 200 | **14.9** | 28.5 | THRUST | 34.1 °/s | 412 m |

**The sustained-turn peak is ~178 m/s (47.4°/s)** — that is vessel #1's *energy* corner, the
crossover where the lift-limited and thrust-limited curves cross (V=178 → n≈15.06, Cl=1.745).
The V=140/175 rows (LIFT still binds) and the V=200 row (THRUST already binds) straddle that
peak from either side — this table's own 44.0°/s at V=200 is already past the peak, not at it.
Below ~178 m/s the wing gives out before the engine does, which is the "she does not bleed"
feel: at V=140 you fly the max-lift circle at constant speed.

### R2 — W/S ↔ instantaneous turn and corner speed
`V_stall = sqrt(2W/(rho·S·Cl_max)) = sqrt(58 860/28.8) =` **45.2 m/s**.
Corner speed (where lift first reaches `n_max`) `= V_stall·sqrt(n_max) = 45.2·sqrt(32) =`
**255.7 m/s** (259.7 using `aoa_max` rather than `Cl_max`) — this is an explicit CONVENTION
SPLIT, not rounding noise: `Cl_max = 1.8` is the plant's hard stall cap, while `aoa_max = 20.0°`
(§2.4) is the CONTROLLER's separate, slightly tighter protection cap (stall AoA is 20.63°, so
`aoa_max` sits under it by design) — the two numbers answer "corner speed by the wing" vs
"corner speed by the pitch-rate ceiling" and are not interchangeable.
**That is past `v_redline` 245** — so `n_max = 32` is *unreachable* and instantaneous turn
rate rises monotonically to redline (V=140: 37°/s → V=200: 53°/s → V=245: 65°/s). This is
exactly the ruling in `[g_limits]`: *"at 32 the LIFT (Cl_max·q) owns the pitch ceiling at
every fighting speed; the clamp survives only as a far-field sanity bound."*

### R3 — Authority ↔ snap-onto-aim
Snap is `c/I` (peak α), the sustained answer is `c/damp` (q-cancels), and what you actually
get is whichever of the three `sqrt_law` branches binds:
- **roll**: `K_phi·e` at 90° = 7.85 rad/s, brake branch 9.28 rad/s, **`p_max` 4.54 rad/s
  binds** → roll is ceiling-limited, and the plant's 286°/s > the 260°/s ceiling, so the
  ceiling is real (`[rate_clamps] p_max` comment: *"needs c_roll raised so the plant can
  actually reach it"* — retrodicts).
- **yaw**: linear branch `K_theta·yaw_scale = 6.4` hits `yaw_max` (0.96 rad/s) at
  **e = 8.6°**; beyond that the rudder is saturated by ruling, not by physics.
- **pitch**: brake branch at e=20° is 2.63 rad/s, `w_max_pitch` at V=140 is 2.17 rad/s
  (level flight, **cosΦθ = 1** by convention throughout this section — a banked/pitched
  attitude shifts the `(n − cosΦθ)·g/V` numerator and both G-clamp bounds with it) —
  but the **AoA limiter binds first** at n ≈ 9.3 → 0.65 rad/s. Pitch feel at fighting speed
  is an AoA-protection story, not a gain story.

### R4 — The PIO / ZOH walls
Loader (`controller.toml` header): `K_w ≥ 4·I·K_theta` and `K_w·sim_dt/I ≤ 0.5`.

| axis | ZOH `K_w·dt/I` | critical-damping cap `K_w/(4I)` | live outer gain |
|---|---|---|---|
| pitch | 150k/(120·9000) = **0.139** | 150k/36 000 = **4.167** | `K_theta` 3.2 |
| yaw | 200k/(120·12 000) = **0.139** | 200k/48 000 = **4.167** | `K_theta·yaw_scale` = **6.4** ⚠ |
| roll | 120k/(120·6000) = **0.167** | 120k/24 000 = **5.000** | `K_phi` **5.0 — exactly at the cap** |

Three findings worth carrying to vessel #2:
1. The **4.167 loader cap** matches CLAUDE.md's *"the wall is the loader's critical-damping
   cap 4.167"* exactly. `K_theta` 3.2 is a *flown* ceiling (deadzone re-engage hunt + AT-2
   ring), tighter than the analytic one.
2. `K_phi = 5.0` sits **exactly** on its cap — hence `[outer]`'s *"7 needed K_w_roll up"*.
3. **Yaw's effective outer gain 6.4 exceeds its own loader cap** because the loader checks
   bare `K_theta`, not `K_theta·yaw_scale`. With `damp_ff_yaw = 1` the honest damping ratio
   is `ζ = sqrt((K_w + damp·q_eff)/(4·I·K_theta·yaw_scale))` — I compute **0.95 @V140 /
   1.21 @V250**; the TOML quotes 0.91/1.15. Same conclusion (near-critical), small numeric
   disagreement — see §5.
4. **The 4.167 loader cap itself ignores `damp·q_eff` on every axis, not just yaw** — it's
   `K_w/(4·I)` with no damping term at all. That makes it a CONSERVATIVE bound everywhere
   (the true critical-damping point is always somewhat higher once the plant's own damping
   torque is counted, exactly as finding 3 shows for yaw); yaw is only the axis where the
   gap is big enough (the `yaw_scale` multiplier) to be worth naming separately.

### R5 — The compression knee
`delta_max_eff(205) = 1 + (0.4−1)·(205−140)/105 = 0.629` — TOML says *"~0.62 effective at
the new level top"* ✓. `v_full` 140 m/s = 504 km/h ("~500"), `v_redline` 245 = 882 km/h
("~880"). The knee is the **dive tax**: it never touches the fight band, and it never
touches drag — only control deflection.

### R6 — The seam that makes all of this one number
`τ` is built once in `sim/aero.h`; the controller **plant-inverts the same expression with
the same params** (`plant_invert`), and `damp_ff` adds back `damp·q_eff·ω_des`. A new
vessel changes `aircraft.toml` and the controller's inversion follows for free — that is
what makes derivation possible at all.

---

### R6 — Compensation-dial decay (a flown LAW, two instances)

**Dials tuned against a low-authority plant become BIASES when authority rises — expect
compensation dials to decay after every power/authority change.** Flown twice:

1. **Fly A (2026-07-28), `yaw_scale` 2.2 → 2.0** — Chad: "a little too much rudder bias...
   balance it out a bit." The extra rudder had been compensation for the pre-v5 plant.
2. **Fly-13 SUPERSEDED (2026-07-30), the `center_band` 1.5° rudder-first zone** — Chad's
   own attribution: "This ask was before I got the power and flight control authority. The
   more rudder was an attempt to get the nose to meet the mouse aim centre more
   deliberately. Now that the power has increased I want to try to balance the banking
   back in... My preference has evolved and I want to see if it is better with a more
   balanced approach with less crabbing."

3. **The POOL BALL released (2026-07-30), `[capture]` carry → 0** — Chad, confirming the
   jitter attribution's A/B on the stick: "The power plant is now doing a better job of
   following my mouse inputs so we can likely do away with the rigid cue ball conditional
   I was seeking in the past (which was the original ask that got previous agents to
   increase my power plant)." v4's DEFINING rung, retired by its own author once the
   plant caught up — the strongest instance yet: not just a dial, a whole flown-in
   MECHANISM was compensation. PARKED, not deleted (machinery stays; carry = 0), and
   the law's FIRST PREDICTIVE USE: the cue-ball is a compensation that scales
   INVERSELY with plant authority — retired on this high-T/W fighter, plausibly LIVE
   on a low-authority vessel (the A6M2 brief inherits this as a design note).
   Re-entry condition = Chad's rationale in reverse: if the closing ability against
   a diverging gun solution ever degrades, the park unwinds (parked-canon entry in
   knowledge_base.md carries the verbatim ruling).

Consequence for deriving vessel #2..N: after setting a new vessel's plant (T/W, authority
triplet), AUDIT every feel dial whose flown rationale was "the nose wouldn't come around" —
they were sized in the old plant's units. The S-dampff lesson ("every outer-loop wall
measured pre-ff is in SAGGED units") is the same law seen from the inner loop.

## 4. Where feel-space ENDS — the flown REJECTED walls

The most valuable data in the repo: dials Chad's hands ruled *out*.

| Dial | Tried | Chad's words | Mechanism |
|---|---|---|---|
| `[auto_level] lean_gain` | **16** (with rate 5.5) | *"no it doesn't, it seems more ready to bank than it does allow me to do rudder flicking, the magnet for yaw isn't strong enough to pull it into the middle"* | Lean closes centering via **bank**. Loop gain `rate × lean_gain`: approved 27 → **rejected 88**. Live today = 5.5×8 = **44**, inside the bracket, and rationale is TURN ENTRY, not centering (flight-log 2026-07-30). |
| `[coordination] yaw_scale` | **2.4** | — (rejected pre-fly) | Breached the **AT-16 β wall**: peak sideslip 5.02° > 5.0° in the sustained coordinated turn. Landed 2.2, later trimmed to 2.0 (*"a little too much rudder bias... balance it out a bit"*, 2026-07-28). |
| `[coordination] K_coord` | **0.8**, fallback **0.9** | — (rejected pre-fly) | Same wall from the other side: β 5.87° / 5.33° > 5.0°. **Both global rudder dials are walled** — which is precisely why `center_frac` (the near-center relief) exists. |
| `[outer] K_theta` | **> 3.2** | — | Level flight's nose HUNTS in the deadzone re-engage AND the step RINGS (AT-2). Analytic cap is 4.167; the *felt* cap is 3.2. E2's 3.6 pair was measured on the sagged (pre-`damp_ff`) plant and is **STALE**. |
| `[g_limits] n_min` | **−10** | — | *"so violent it looped the airframe (unsustainable, broke AT-11)"* (`[g_limits]` comment). Live −9; walk-back −8. |
| `[pursuit] step` | **any > 0** | *"skip — the discontinuity IS the defect"* | A rate floor is a **sign relay across e = 0**; with S-dz-motion keeping pointing unlatched while the hand moves, slow tracking overtakes the aim and buzzes at ±step. AT-2 went 1 → 7 reversals at 4°/s. **NAMED WALL, all felt values.** |
| S-aimclamp (24° aim cone) | mechanism | *"this feels wrong... the whole thing broke. I can[']t roll over with a flick, I just do a shallow banked turn... I don't want to change the feel, just don't want my aim going off screen"* | The **aim-to-nose error magnitude IS the maneuver-commitment signal** (bank_error saturation, MANEUVER dwell, MB-lean's shallow-bank leg). Cap the error → cap the commitment. |
| S-retclamp (display-only reticle clamp) | mechanism | *"if I deflect full left it starts to turn then goes into a long dive and tries to come back around"* | **DISPLAY-ONLY ≠ PILOT-LOOP-NEUTRAL.** The reticle is the human's error feedback; a pinned edge marker under-reports the true aim, the hand under-corrects, and the plane honestly flies the full far-aim carve. Plant was bit-identical and it still broke the flying. |
| `[deadzone] lo/hi` | **0.03/0.06** (at that time) | — (rejected pre-fly) | Below the mechanism's own **physics floor**: the MB-lean magnet equilibrium (~0.076°) sat above the whole circle, so the nose could never rest inside. Threshold-shaving past a physics floor is theater. |
| S-dz-motion `rest_dwell` | 0.15 → 0 → 0.15 | A/B: *"there are bigger steps now... smooth curves should feel smooth not stepped"* | The gated build **won** the A/B. Recorded here because the first fly read as a regression and only the deliberate A/B settled it. |
| `[camera] key_anchor_rate` (S-keychase) | 6.0 | *"a disorienting snap to the chase cam which throws off my aim"* → *"Only the precedence of the freelook push shall do that"* | Flown-APPROVED once, then RETIRED: it was approved against a parked-aim scenario while Chad flies mouse **and** keys at once. **Keys change trajectory, never the camera.** |

**Standing rule that falls out of the last two rows:** an audit, a green gate and a
bit-identical plant prove nothing about a human-in-the-loop change. Only the fly does.

---

## 5. Retrodiction self-check

Re-deriving vessel #1's known behavior from its coordinates alone.

| # | Quantity | Predicted from coordinates | Known / measured in repo | Verdict |
|---|---|---|---|---|
| 1 | Stall speed clean | `sqrt(2·29430/(1·16·1.8))` = **45.2 m/s** | *"stall speed ~45"* — `[flaps]` comment | ✅ |
| 2 | Stall speed, landing flap (Cl 2.6) | **37.6 m/s** | *"~45 → ~37 m/s"* — `[flaps]` | ✅ |
| 3 | `q_att_floor` crossover | `sqrt(2·280/1)` = **23.66 m/s** | *"~23.7 m/s"* — `[authority]` | ✅ |
| 4 | Full-deflection steady yaw rate | `c_yaw/damp_yaw` = 2.0 rad/s = **114.6 °/s** | *"at c_yaw=16 the plant delivers ~114 deg/s"* — `[rate_clamps]` | ✅ exact |
| 5 | Full-deflection steady roll rate vs `p_max` | **286.5 °/s** > 260 | *"needs c_roll raised so the plant can actually reach it"* — `[rate_clamps]` | ✅ |
| 6 | Compression at ~205 m/s | **0.629** | *"~0.62 effective at the new level top"* — `[compression]` | ✅ |
| 7 | `v_full`/`v_redline` in km/h | **504 / 882** | *"~500 km/h" / "~880 km/h"* — `[compression]` | ✅ |
| 8 | Minimum drag, HISTORICAL table (k=0.05) | `W/(L/D 14.14)` = **2081 N** | *"T_max·f == minimum drag (2081 N)"* — `[atmosphere]` | ✅ exact |
| 9 | Max level speed, HISTORICAL table (T 9000, k 0.05) | solve `0.2V⁴ − 9000V² + 5.41e6 = 0` → **210.7 m/s** | *"level T = D moves ~600 → ~740-750 km/h (~205-210 m/s)"* — `[airframe]` | ✅ **this validates the drag model** |
| 10 | ZOH margins | 0.139 / 0.139 / 0.167 | *"ZOH margin 0.139 has room"* — CLAUDE.md | ✅ |
| 11 | `K_theta` analytic cap | `150k/(4·9000)` = **4.167** | *"the loader's critical-damping cap 4.167"* — CLAUDE.md | ✅ exact |
| 12 | `K_phi` cap | `120k/(4·6000)` = **5.000** = live `K_phi` | *"7 needed K_w_roll up"* — `[outer]` | ✅ |
| 13 | Corner speed vs redline | `45.2·sqrt(32)` = **255.7 m/s** > 245 | *"at 32 the LIFT owns the pitch ceiling at every fighting speed"* — `[g_limits]` | ✅ |
| 14 | **Max level speed, CURRENT table** | solve `0.2V⁴ − 18000V² + 1.624e6 = 0` → **299.8 m/s (1079 km/h)** | **no live number exists** — the `[airframe]` "205-210 m/s" line is stale (written at T 9000 / k 0.05) | ⚠ **unverified** |
| 15 | **Service ceiling, CURRENT table** | `18000·atm_frac = 1140` → f = 0.0633 → h = **9.5 km** | `[airframe]`: *"service ceiling rises ~1 km"* from 8 km, i.e. ~9 km | ⚠ **0.5 km over** |
| 16 | **Steady rates vs the `[authority]` comment** | at c = 7/4/15 (the era of that comment): **85.3 / 28.6 / 179 °/s** | comment: *"~85/23/180 deg/s"* | ⚠ pitch/roll exact; **yaw 28.6 vs 23 (+24%)** — the comment's 23°/s likely already folds in a compression factor near `delta_max_eff ≈ 0.80` at that era's redline: 28.6×0.80 = 22.9 ≈ 23, closing most of the gap |
| 17 | **Yaw damping ratio with `damp_ff_yaw`=1** | `sqrt((200k + 8·q)/(4·12000·3.2·2.0))` = **0.95 @V140 / 1.21 @V250** | `[inner]`: *"0.91@V140 / 1.15@V250"* | ⚠ ~5% high, same conclusion |
| 18 | Pitch rate at n_max=16 era, V=300 | `16·9.81/300` = **30 °/s** (cosΦθ=1: 28.1) | `[g_limits]`: *"16 gives ~26 deg/s at 300 m/s"* | ⚠ ~15% high |

**Honest limits of the ontology, stated rather than hidden:**

- **#14 / #15 are the real gap.** The v5 RUNG D power ruling (T_max ×2, `k_induced` ÷3.3)
  changed the whole energy envelope and **no repo number has been re-measured against it**.
  The `[airframe]` and `[atmosphere]` comments still describe the 9000 N / k=0.05 vessel.
  A future vessel derivation should re-measure level top speed and service ceiling in the
  harness before trusting either. My model reproduces the *historical* numbers exactly
  (#8, #9), which is why I trust the derivation and distrust the stale comments.
- **#16, #17, #18 are small quantitative disagreements** (5–24%). Each is a comment written
  during a different tune; none changes a ruling. They mark that **prose comments in the
  TOMLs are dated snapshots, not live oracles** — the formulas here are.
- **The instantaneous-turn relation R2 is derived, not flown.** There is no flight-log row
  in which Chad rules on "does turn rate keep rising to redline". It retrodicts the
  `[g_limits]` design comment, and nothing more.

---

## 6. How to derive a new vessel

Ordered procedure. Each step is checkable against §2–§3.

1. **Pick the archetype's real numbers** — historical mass, wing area, `Cl_max`, and the
   real T/W and W/S. (P-47D: ~6600 kg, S 27.9 m², W/S ≈ 237 kg/m², T/W ≈ 0.28. A6M2:
   ~2400 kg, S 22.4 m², W/S ≈ 107, T/W ≈ 0.28. Bushplane: W/S ≈ 100–110 (Beaver class),
   T/W ≈ 0.20.)
2. **Apply the arcade conventions, and say which you applied.** Vessel #1 runs
   **T/W 0.61 vs the historical ~0.31 — an ARCADE ×2 power ruling by Chad, 2026-07-23,
   rung D**, paired with `k_induced` 0.05 → 0.015 (induced drag ÷3.3). If a new vessel is
   meant to sit in the same *game*, apply the same two multipliers to its historical
   numbers; if it is meant to feel heavier/lazier, do so **deliberately and log the ruling**.
   Keep W/S historical — that is what makes the archetypes distinguishable.
   **Methodology caveat (ruling needed):** the "~0.31" above is this repo's own pre-rung-D
   `T_max/W`, not an independently-sourced historical figure. Computed fresh with the SAME
   static-thrust formula the vessel briefs use (`T[lbf]=10.41·HP^(2/3)·D[ft]^(1/3)`), the
   Bf-109F comes out at **T/W≈0.41**, meaning vessel #1's actual arcade multiplier is
   **×1.5**, not ×2 — while the P-47/A6M2/Beaver briefs each apply a clean ×2 to their OWN
   freshly-computed static-thrust baselines. That is a **~33% inter-vessel power-convention
   inconsistency**: either raise vessel #1 toward a true ×2 (≈24 kN) or accept the fighters
   as intentionally out-gunning the interceptor 33% ahead of methodology — Chad's call.
3. **Derive the plant table** (`aircraft.toml`):
   `mass`, `S`, `T_max = arcade_TW · m · g`, `Cl_max`, `Cl_alpha`, `Cd0`, `k_induced`,
   inertias scaled roughly by `m · span²`. Sanity-check with §5's formulas:
   `V_stall`, `L/D_max`, `D_min`, max level `V`, corner speed vs `v_redline`.
4. **Pick the authority triplet from the FELT rates, not from torque.** Because dynamic
   pressure cancels, `steady rate = c/damp` — so choose the rate you want, choose `damp`
   for the snap character (`α_peak = c·q/I`), then `c = damp · ω_target`. Check
   `p_max`/`yaw_max` are *reachable* (vessel #1: 286 > 260 ✓).
5. **Set the compression knee** where the archetype should start going heavy
   (`v_full`/`v_redline`/`min_frac`) and the atmosphere taper for its service ceiling
   (solve `T_max·atm_frac = D_min`).
6. **Re-derive the cascade gains against the new plant, using the documented walls:**
   - `K_w_axis` from the ZOH ceiling `K_w·dt/I ≤ 0.5` (target ≈ 0.14–0.17 as vessel #1 does).
   - `K_theta ≤ K_w/(4·I)` is the *loader* cap; expect the **felt** cap to be lower
     (vessel #1: 3.2 vs 4.167). Start below and let Chad's fly raise it.
   - `K_phi` likewise, and note vessel #1 sits exactly on its cap — a heavier vessel with
     more `I_roll` needs `K_w_roll` up before `K_phi` can move.
   - `yaw_scale` is walled by the **AT-16 5° sideslip** gate, not by stability. Verify
     before the fly; both global rudder dials (yaw_scale up, K_coord down) breach it.
   - `n_min` is walled by "does it loop the airframe" (AT-11), `n_max` mostly by the wing.
   - **`[pursuit] step` stays 0.** That wall is structural, not vessel-specific.
   - `damp_ff_*` = 1.0 (full plant inversion) unless a vessel-specific reason appears; it
     completes the inversion the integrator would otherwise have to source.
7. **Fly it one dial at a time.** Every gain above is a hypothesis until Chad's hands rule.
   Log each in `docs/flight-log.md` with hypothesis → observed, and add any new rejected
   wall to §4 of this file — that section is the asset, not the tables.
