# Mission B plan — "Instructor to Chad's delight": coordinated high-performance flight + the energy game

*Status: PLAN RED-TEAMED (SOUND-WITH-FIXES, folded; §5b), **RULED** (Chad's §4 answers).
**LANDED so far** (worktree `.claude/worktrees/mission-b`, branch `sandbox/mission-b`):
MB-1/MB-2 (2026-07-07, `888fe1a`+`da1d7b9`), MB-3 elevator step-1 + MB-5 pursuit_expo, MB-7a/7a2
envelope + compression knee, MB-atm, MB-right, push side-cone — and the 2026-07-08 overnight
batch: **MB-aim** (rate-keyed mouse curve, plan+diff red-teamed, docs/mb_aim_plan.md),
**MB-flaps + gear** (force-only plant devices, diff red-teamed; over-speed = speed-brake default
pending Chad's ruling at the stick), **MB-7c** (wind audio / AR AoA dial / vortices / energy
cluster). Gate **240/240**; SPEC §0 entries per mechanism. **NEXT: Chad FLIES the 2026-07-08
flight-log rows**, then MB-6 (D4 ballistic nose-to-wind), MB-4 yaw_scale retune, MB-9; MB-8 only
if flown-E2 + the flaps tax don't already deliver the jink-bleed intent.*

*Sources: `docs/next_missions.md` (Chad's words), a full code audit of `control/controller.cpp` /
`sim/step.cpp` / both TOMLs, and the deep-research pass on arcade-instructor + energy-model prior art
(§R below).*

---

## 0. The felt intent, mapped (Chad's words → mechanism → knob)

| Chad's words | What the code does today | The plan |
|---|---|---|
| Override rudder "top end needs a little nerfing" | Override yaw commands `±yaw_max` = 90°/s; since `c_yaw` 4→16 the plant delivers it (steady full-δ yaw ≈ 114°/s below compression — the 23°/s config comment is STALE) | D1: one shared yaw ceiling, lowered; override reaches it, cascade only approaches it |
| Cascade "needs to use more rudder (even skidding) in turns at high mouse-aim deflection"; "rudder based on the angle of my deflection" | Yaw pointing faded by `blend` (aim-error size) to 0.8 in MANEUVER; global `yaw_scale ≤ 1.2` ceiling forced by the AT-15 split-S corkscrew | D2: re-key the fade to BANK ALIGNMENT (`cos(bankErr)^power` — the same shape pitch already uses); aligned turn = full rudder, roll-through = ~zero → corkscrew disarmed by geometry, `yaw_scale` headroom unlocked |
| "Way higher gain on the elevator"; "up to 10 g's is okay"; high-g turning/pull-ups/dives | `n_max` = 16 (already > 10 — CONFIRM meaning, Q2); inner loop never formally tuned (flight-log steps 1–2 UNCHECKED; `K_wi_pitch` 20k vs the ~1.4e6 the step-1 procedure lands on) | D3: tune the inner loop FIRST (rate attainment ≈ the felt "gain"), then `c_pitch` / `pursuit_expo`, `[g_limits]` only after Q2 |
| "Banking is good" | bank-to-turn, `bank_align_power` 6, `p_max` 260, `c_roll` 24 | UNTOUCHED |
| "Highest deflection ALWAYS from the keyboard override" | Override drives the held axis to its in-envelope max rate; cascade yaw can currently command up to `yaw_scale·yaw_max` ≈ 108°/s > the override's 90 — the invariant is already soft on yaw | D1/D2 restore it structurally: one ceiling, override owns it (see the D1 ordering fix) |
| "Stall out and fall, but the instructor regains its coordination — not spin to crash — unless near the deck" | BALLISTIC attitude-hold points the nose at the HELD AIM on floor authority; no recovery shaping; coordination gated off | D4: ballistic points the nose INTO THE RELATIVE WIND (v̂) — weathervane, rebuild q, exit, cascade resumes. Altitude-blind = deck stays honest |
| Top speed up; zoom retention up; high-G bleeds more; "jittering… bleeding energy based on the AMOUNT OF DEFLECTION" | Top speed ≈ 167 m/s (600 km/h, `T_max` sized to it); drag = `q·S·(Cd0 + k·Cl²)` — NO deflection drag, NO sideslip drag (skid + jink are energetically FREE today) | D5: `T_max` up (top speed + zoom), `k_induced` up (G-bleed), NEW quadratic control-deflection drag term in the plant (the jink tax — SPEC §0 supersession, AT-12 mirrored) |
| "This will be an arcade shooter" | — | Feel over fidelity in every sizing below |

## 1. What does NOT move (the walls this plan is built against)

- **Raw mouse→aim** — untouched everywhere. No smoothing on mouse→aim→error→ω_des→Inputs (§9.1).
- **`K_theta` ≤ 3.2** — the PIO wall (AT-2 ring + deadzone hunt). Snap comes from authority/attainment,
  never from outer gain. ZOH ceiling `K_ω·dt/I ≤ 0.5` stands.
- **No derivative-on-error**, ever. No aim-rate feedforward either (see §6 Rejected — it is
  frame-quantized, so it breaks AT-9 the same way S7-mouselevel did).
- **Throttle passthrough** — the instructor never manages energy for the pilot; the energy game is
  played by the PILOT against the PLANT.
- **Override supremacy** (Chad requirement 4) — override suspends pursuit, drives the axis to the
  envelope max, integrator frozen. D1 *strengthens* this on yaw.
- **Auto-leveling** — none, anywhere (S7-mouselevel graveyard). D4 shapes the BALLISTIC pointing
  target only; the aim itself never moves and the cascade never re-levels.
- **Every gate hysteretic**; GROUNDED semantics; the §5 seam (sim/control pure, no clocks).
- **AT-12 tune-independence** — the energy gate must reconcile at ANY tune; D5c mirrors the new drag
  term on both sides by design.

## 2. Design decisions

### D1 — Rudder ceiling unification + override nerf (structural cleanup + one knob)

**Problem.** Two defects share a root:
(a) The override yaw rate (90°/s, reachable since `c_yaw`=16) is Chad's "gross rudder."
(b) `yaw_scale` multiplies AFTER `sqrt_law`'s `yaw_max` clamp
(`controller.cpp:415,445-447`), so the cascade's clamp-region cap is `yaw_scale·yaw_max`, not
`yaw_max`. (Red-team correction: the bank-to-turn branch's fade caps today's effective max at
~86°/s < the override's 90, so req-4 holds THERE; the real existing req-4 leaks are the PUSH
branch — no fade, reachable ~106°/s at the side-cone edge — and the coordination residual, noted
below.) Raising `yaw_scale` (D2's goal) would blow both open.

**Fix (one structural line + one retune):** move `yaw_scale` INSIDE the law — gain slot, not
post-scale: `sqrt_law(demand.y, K_theta·yaw_scale, aB_yaw, yaw_max)` — at BOTH call sites (`:415`
push AND `:445` bank-to-turn; leaving the push branch post-scaled would cap push yaw at
`yaw_scale·yaw_max` ≈ 130°/s at yaw_scale 2.0 — worse than today). Now `yaw_max` is a true shared
ceiling: the override reaches it (80 ms ramp), the cascade only approaches it at large aligned
deflection. The nerf is then ONE dial: `yaw_max` 90 → **~65°/s** (start; Chad tunes — "a little
nerfing", see Q1's coupling note). Requirement 4 holds on the yaw POINTING path by construction.

- **The coupling Chad must rule on (Q1):** requirement 4 makes any override nerf number ALSO the
  cascade's high-deflection rudder cap. Today's cascade max is ~86°/s; a 65 ceiling CUTS the
  instructor's big-deflection rudder ~25% even as D2 raises the small/mid-deflection rudder ~67%.
  If Chad wants the instructor's top-end kept, the nerf number is ~85–90, not 65.
- Coordination residual (red-team P2-1): cascade yaw = `K_coord·(−β) + gate·sqrt_law(...)` has no
  summed clamp, so a hard skid adds ~9°/s above the ceiling on the cascade side only. v1: document
  req-4 as "pointing ceiling shared; coordination residual exempt" (clamping the SUM would let a big
  skid eat the pointing budget). Revisit only if felt.
- Braking branch (`√(2·aB·e)`) is unscaled — correct: braking is physics, not gain.
- Knob-equivalence proof: at small demand both forms emit `K_theta·yaw_scale·e` identically; the
  golden moves only via the clamp region + the retune — deliberate re-record. Flight-log honesty:
  the MB-1 flight carries TWO behavior changes (ceiling drop + the clamp-region reshape) — say so
  in the row's hypothesis.
- Supersession log: scoped §0 note (a re-ordering of an existing gain, not a new mechanism).

### D2 — Bank-aligned rudder gate (THE cascade change: skid when aligned, clean when rolling)

**Problem.** "More rudder in turns keyed to my deflection" is blocked by the AT-15 corkscrew: yaw
pointing during the split-S ROLL-THROUGH corrupts the roll (budget_frac 0.998 at yaw_scale 2.0). The
current fade is keyed to `blend` (aim-error size), which saturates in ANY maneuver — it cannot
distinguish "tracking within an aligned bank" (where Chad wants MORE rudder) from "rolling through
inverted" (where yaw must be ~zero).

**Fix.** In the bank-to-turn branch (`controller.cpp:445-447`), replace the blend-keyed fade
`(1 − (1−yaw_maneuver_frac)·blend)` with a bank-alignment gate **composed through `blend` exactly
the way the pitch side already composes its align gate** (`pitch = w_push·((1−blend) + blend·align)`
at `:438` — red-team P0-1: a bare `have_bank ? gate : 1` steps ~23°/s across the `err == blend_lo`
boundary, because a lateral aim's `bank_eff` saturates near 90° the instant `have_bank` flips; the
blend composition is what makes it continuous everywhere):

```
yaw_gate = yaw_min_frac + (1 − yaw_min_frac) · f(bank_eff)          // have_bank; else yaw_gate := 1
yaw += ((1 − blend) + blend·yaw_gate) · sqrt_law(demand.y, K_theta·yaw_scale, aB_yaw, yaw_max)
```

- **Two candidate shapes for `f`, BOTH carried to the AT-15 re-run + Chad's fly (red-team P1-1 —
  which one is right is a felt-intent question, Q1b):**
  - **(a) `f = pow(max(0, cos(bank_eff)), yaw_align_power)`** — rudder waits for the roll
    (fades through the WHOLE roll-in). Maximum corkscrew margin, but deletes the mid-roll nose crab
    that `yaw_maneuver_frac = 0.8` was flown in to provide (S7-yaw, Chad-liked): a max lateral
    flick's first ~500 ms would be roll-at-p_max + pitch≈0 (align⁶) + rudder≈`yaw_min_frac` — the
    nose doesn't start toward the aim until the bank comes in.
  - **(b) `f = smoothstep(−band, 0, cos(bank_eff))`** (band ~0.15) — full rudder for
    `|bank_eff| ≤ 90°` (keeps S7-yaw's immediate crab through the entire roll-IN), fading to
    `yaw_min_frac` only past knife-edge — which is where the AT-15 corkscrew actually lives
    (roll-THROUGH, `bank_eff ≈ 180°`, cos < 0). Preserves today's flick feel AND disarms the
    split-S; whether rudder in the last 90° of a roll-through re-seeds the corkscrew is exactly
    what the AT-15 re-run answers.
  - Start with (b) — it is the strict "keep what Chad flew, remove only the poison" shape; (a) is
    the fallback if (b)'s AT-15 budget_frac degrades at raised `yaw_scale`.
- Aligned turn (`bank_eff→0`): gate = 1 — FULL rudder pointing; the skid grows with the aim
  deflection exactly as Chad asked ("rudder based on the angle of my deflection" — `demand.y` is that
  angle; the sqrt_law already scales with it).
- Roll-through (split-S, roll_latch, past knife-edge): gate → `yaw_min_frac` (start **0.1**) — the
  corkscrew mechanism is disarmed by geometry, not by a global ceiling.
- `cos` is even in `bank_eff` → the gate is immune to roll_latch sign transitions and the ±180°
  bank_error wrap — no new latch legs (red-team confirmed). Continuous fade, the S7-loop-invert
  precedent; hysteresis rule targets discrete switches.
- **Yaw-integrator during the gated roll-through (red-team P1-5, answered from the code):** at
  today's `K_wi_yaw·integ_cap` = 50k N·m vs ~129k yaw authority at combat q, the capped integral
  cannot saturate yaw and the unwinding anti-windup (`controller.cpp:568`) covers it — no freeze
  needed NOW. But after MB-3 raises `K_wi_yaw` (step-1 landing zone ~10⁶), a stale yaw integral
  wound during the deliberate aligned skid carries into the roll-through UNGATED (the gate
  multiplies pointing only) — MouseAimFlight's exact "stale rudder trim" corkscrew seed (§R.2).
  So: AT-15 split-S re-run is scheduled INSIDE MB-3 (build order §3), and the
  pre-registered fallback if it trips is gate-scaled integration or the §R.2 cross-axis unwind —
  a decision, not a scramble.
- Then RETUNE `yaw_scale` upward (1.2 → try 1.6–2.0, one dial per flight, AFTER the yaw inner loop
  is tuned — build order §3) — the ceiling that forced 1.2 no longer exists because AT-15's
  roll-through now sees `yaw_min_frac` of it. PIO caution (red-team P2-4): 1.6–2.0 puts the
  effective FINE yaw outer gain (`K_theta·yaw_scale` = 5.1–6.4) past the only measured PIO wall
  (3.2, measured on PITCH; yaw's wall has never been measured and its damping differs) — run
  `seads_harness step yaw` + a deadzone-dwell check before flying each increment.
- Precedent (§R.2): both open-source mouse-aim references run rudder as a CONTINUOUS proportional
  on the lateral aim error at all deflections and prevent corkscrew elsewhere (roll-channel
  scheduling; MouseAimFlight additionally subtracts a sideslip term in the roll loop and zeroes the
  YAW integrator whenever the pitch integrator zeroes in large maneuvers — stale rudder trim is a
  corkscrew seed). Red-team consideration for MB-2: does the raised `yaw_scale` need a yaw-integrator
  freeze/decay while the gate is at `yaw_min_frac`, or does the existing anti-windup already cover it?
- `yaw_maneuver_frac` is REMOVED (subsumed). PUSH-branch yaw (`:415`) left as-is in v1: push confines
  the aim to the vertical-plane cone (≤22.5°), so `demand.y` is small there; AT-15's mid-push legs
  stay the tripwire. (Open point for the red-team: does raised `yaw_scale` need the gate in the push
  branch too?)
- AT-15 work: re-run the split-S legs at the NEW `yaw_scale` (budget_frac must stay ~small); ADD a
  gate leg — open-loop, fixed state, sweep `bank_eff` 0→180°: yaw_gate monotone non-increasing,
  = 1 at 0, = `yaw_min_frac` past 90°. Mutation: re-key gate to `blend` → aligned-turn case fails.
- Supersession: §0-logged (S7-yaw's fade reshaped; same class as S7-push's re-keying).

### D3 — Elevator "way higher gain" (tuning-first, in strict order)

**Finding:** flight-log tuning-order steps 1–2 were never done (boxes unchecked). Step-1's own
measurement: inner-loop rate overshoot appears at `K_wi_pitch ≈ 2e6`; shipped value is 2e4 — two
orders below the procedure's landing point. Sluggish rate ATTAINMENT reads as "low elevator gain"
on the stick. Order (each its own flight):

1. **`K_wi_pitch`** up per the step-1 procedure (slight overshoot, back off 30%; ~1.4e6 territory).
   NOTE the coupling: the integrator's max sustained torque is `K_wi·integ_cap` — raising `K_wi`
   also raises the sustained-G attainment (the AT-11 preload lesson), so re-check AT-11's derived
   setpoint legs (config-relative — they should track).
2. **`K_w_pitch`** for damping headroom under ZOH < 0.5 (currently 150k·(1/120)/9000 = 0.139 — room).
3. **`c_pitch`** 11 → try 14 (authority = the snap onto the aim; plant + inversion move together, H1).
4. **`pursuit_expo`** 2.0 → try 3.0 (mid-deflection parabolic pull — "use the elevator for high-g
   turning" at big deflections).
5. **`[g_limits]`** ONLY after Q2 (below) — `n_max` is ALREADY 16; if Chad means "give me the felt
   10 g," the shortfall is attainment (steps 1–3), not the clamp. If he means "clamp at 10 for
   predictability," that is `n_max` 16→10, one dial, and the braking/AT-6/AT-11 config-relative legs
   track it by design.
- Yaw/roll inner loops get the same step-1 pass (`K_wi_yaw`, `K_wi_roll`) — D2's skid tracking is
  only as crisp as yaw-rate attainment.
- WALL: `K_theta` stays 3.2. All snap comes from the above.

### D4 — Ballistic recovery: nose into the wind ("stall, fall, catch it — don't spin in")

**Change (scoped, in the existing BALLISTIC branch, `controller.cpp:181-215`):** while ballistic
AND `pursuit` (no override held), the attitude-hold's pointing target becomes the RELATIVE WIND
`v̂` (the controller's own guarded copy) instead of the held aim:

```
target := vhat                    // [ballistic] recover_to_wind = true (bool; false = today's aim-hold)
```

**The knob is BINARY, not a blend weight** (red-team P1-3): the canonical stall — zoom climb, aim
held UP, plane falling straight DOWN — has aim ≈ −v̂, and any `normalize(mix(aim, vhat, t))` at
intermediate t is the S7-cam antiparallel NaN (`normalize(~0)`) poisoning ω_des. Both ENDPOINTS are
safe; the poisoned region is exactly what a fractional knob invites tuning into. Bool on/off, both
arms tested, and the antiparallel case (aim == −v̂) is an explicit test leg — the parallel-only
test is the S7-cam false banner.

- The plane weathervanes nose-down into the fall, rebuilds q, exits at `v_ballistic_exit` (40 m/s,
  hysteretic), the AoA filter reseeds on the exit edge (already shipped), the cascade resumes chasing
  the untouched aim, coordination comes back live — "regains its coordination."
- The AIM never moves (raw-mouse rule untouched); this shapes only what the low-authority
  attitude-hold points at, in a regime where chasing the aim is aerodynamically a lie anyway
  (the tail-slide α≈180° is why coordination/AoA are already gated off down there).
- Override supremacy preserved: any held key suspends the recovery exactly as it suspends the hold
  today (the `ns.pursuit` gate is already outside/above).
- Altitude-blind: no deck term — natural consequences, per Chad. Honest cost estimate (red-team
  P2-3): fall to the exit speed (~4 s, ~80+ m), then the wing can't pull hard until q rebuilds
  (n = 3 needs V ≈ 78 m/s) plus the pull-out arc — realistically **several hundred meters** of sky
  at today's T_max (less after E1). Q4 uses this number, not an optimistic one.
- Exit-speed honesty (red-team P2-2): `v_ballistic_exit` = 40 m/s is BELOW the 1-g stall speed
  (√(2mg/ρSCl_max) ≈ 45 m/s) — the cascade resumes on a wing that can pull ≤ ~0.8 g, so a
  still-held-up aim porpoises (pull → re-stall → re-enter). Part of the D4 flight: consider
  `v_ballistic_exit` 40 → ~55 (≈1.2·V_stall; one dial, hysteretic legs exist, AT-17 golden moves
  deliberately) — or pre-register the porpoise as accepted "pilot insists on the unflyable"
  behavior. Chad's call at the stick.
- `recover_to_wind = false` is the knob-off strict-superset arm (bit-identical to today — the
  golden proof shape every S7 mechanism used).
- Tests: (a) closed-loop zoom-to-stall with aim held UP: today's build holds the nose up and
  tail-slides; new build pitches through toward v̂, exits ballistic, re-acquires the aim — pin
  exit-tick speed ordering + no crash from a healthy-altitude stall; (b) knob-off bit-identity;
  (c) override-during-ballistic still suspends (existing leg extends); (d) v̂-guard: the held
  last_vhat at v→0 (poison-the-copy discipline, S4a class).
- Hysteresis: inherits the ballistic latch's — no new gate legs.
- Supersession: §0-logged amendment to §9.6's "attitude-hold toward targetDir."

### D5 — The energy game (plant + tune; the combat egg made real)

Sequenced so each dial is flown against the AT-12 speed-retention printout + `ctrl_fly` CSV
(instruments exist; NEVER gates):

1. **E1 `T_max`** 5800 → **~9000 N** (target: T=D near ~205 m/s ≈ 740 km/h; parasitic drag
   0.2·V² N at Cd0=0.025 ⇒ ~8.9 kN at 205). Also transforms zoom/climb (power/weight ×1.55) —
   fly BEFORE judging zoom retention. Watch: G-clamp pitch rate `(n−cos)·g/V` thins at the new top
   end; compression `min_frac` 0.4 nears redline — both may re-open D3 dials, which is why energy
   comes AFTER D3 in the schedule.
2. **E2 `k_induced`** 0.05 → try **0.08–0.12** — high-G bleed ("less retained for high-G… rewards
   efficient energy management"). Pure knob; AT-12 reconciles at any value by design.
3. **E3 deflection drag (NEW plant mechanism — the jink tax). FLY E2 FIRST:** the research
   (§R.4) found NO shipped arcade precedent for an explicit deflection-drag tax — the universal
   proxy is induced-drag-via-G, which E2 already turns up. If a flown E2 delivers the felt
   "jittering bleeds energy" (a jinking defender pulls G, so k·Cl² taxes it), E3 may be
   unnecessary — Chad calls it after flying E2. If built:
   `Cd = Cd0 + k_induced·Cl² + Cd_ctrl·Σᵢ (Inputᵢ·δ_max_eff(V))²` in `sim/step.cpp:66`.
   - Quadratic: the instructor's constant small housekeeping deflections (trim, coordination, ff)
     stay ~free; full-deflection jinking bleeds — "based on the AMOUNT OF DEFLECTION," and it taxes
     the D2 skid too (rudder is a control surface), which IS "energy rewards coordinated turns":
     the skid buys pointing at an energy price — a real tactical trade, not a freebie.
   - NO separate sideslip-β drag term in v1 (second AT-12 composition site, and β lies in the
     tail-slide; the rudder-deflection tax covers the felt intent with one mechanism).
   - Expression lives in `sim/aero.h` (the four-site single-source discipline); plant, AT-12's
     config-side reconstruction (DUPLICATED composition, per the AT-12 fork-detector design), and
     telemetry all call it. It reads the same float `sim::Inputs` the plant reads (the float-seam
     lesson).
   - Sizing start: `Cd_ctrl` such that full single-axis deflection at cruise ≈ +40% of Cd0
     (Cd_ctrl·δ² ≈ 0.01 at δ_max_eff(167)≈0.65 ⇒ Cd_ctrl ≈ **0.024**); tune by feel.
   - Tests: AT-12 extended (mutation `Cd_ctrl→0` in the plant fork side must blow the residual, like
     `k_induced→0` does); a first-principles pin (same state, full-deflection vs clean inputs, speed
     decays strictly faster; `Cd_ctrl = 0` ⇒ bit-identical plant — knob-off superset); goldens
     re-record deliberately.
   - Supersession: §0-logged plant amendment (§7's force model gains a term). AT-18 untouched
     (torque model unchanged).
4. **E4 `Cd0`** only if the flown zoom still bleeds after E1 (0.025 → 0.022, one dial).
- Side-effects audit: drones fly the same airframe — `T_max` changes drone trim speeds
  (`config/scenario.toml` may want a touch-up; the S8 loader tripwires reference `v_redline`, which
  does NOT move). The AT-15 boundary-tied constants reference `[regime]`/`[push_gate]`, not drag —
  unaffected. AT-6/AT-11 are config-relative — they track.

## 3. Build order (each lands: plan → implement → gate → red-team where flagged → fly → log row → tag)

| # | Item | Type | Red-team? |
|---|---|---|---|
| MB-1 | D1 yaw_scale-inside-law (both call sites) + `yaw_max` nerf (Q1's number) | structural line + 1 dial | with MB-2 (one scoped round) |
| MB-2 | D2 bank-aligned yaw gate, shape (b) first (+ AT-15 re-verify + gate leg) | mechanism (§0) | YES — cascade core |
| MB-3 | D3 step-1 inner loop, ALL THREE axes (`K_wi_pitch`/`K_wi_yaw`/`K_wi_roll`), then step-2 damping headroom. **Premise checklist BEFORE the dial** (red-team P2-5): the K_wi raise makes `integ_cap` near-vestigial — enumerate + re-derive the premise-keyed tests/comments (AT-11 preload legs, the 4d "unwind live below 53 m/s" pin, the toml `integ_cap`/`n_min` coupling comments) per the AT-15 banner discipline. **Then AT-15 split-S re-run** (the P1-5 integrator seed check; pre-registered fallback: gate-scaled integration or §R.2 cross-axis unwind) | dials (procedure exists) | no (but AT-15 re-run gates it) |
| MB-4 | D2 retune `yaw_scale` up (1.2→1.6→2.0, one per flight; `step yaw` + deadzone-dwell before each — P2-4) | dials | no |
| MB-5 | D3 `c_pitch`, `pursuit_expo`, (Q2 ⇒ maybe `n_max`) | dials | no |
| MB-6 | D4 ballistic nose-to-wind (+ possible `v_ballistic_exit` dial) | mechanism (§0) | YES — regime/latch class |
| MB-7a | E1 `T_max` | dial | no |
| MB-7b | E2 `k_induced` | dial | no |
| MB-7a2 | Compression-knee rescale per Q3b ruling (`v_full` 111→140, `v_redline` 208→245; `min_frac` stays 0.4) — check the S8 loader tripwire `muzzle ≥ 2·v_redline + g·kTMax` still holds at the new redline | dials | no |
| MB-aim | **AIM RESOLUTION — FIRST PRIORITY of the next context (Chad 2026-07-08).** A mouse sensitivity CURVE replacing the single linear `aim_sensitivity` gain (the known ceiling on WT-grade connectedness): fine precision at small hand movements (tracking, the nested-reticle work), accelerating gain for big sweeps (flicks) — higher effective aim resolution where it matters. LEGALITY RAILS: the curve must be a MEMORYLESS pure gain shape (no state, no filtering — §9.1's ban is on smoothing, not shaping) and must be keyed on the frame-rate-NORMALIZED mouse rate (pixels/second = delta/frame_dt, then integrated over the frame), NEVER on the raw per-frame delta magnitude — a per-frame-delta curve makes the same physical hand motion rotate differently at 30 vs 240 fps (the S7-mouselevel frame-quantization class). THE risk to red-team: AT-9's bit-identity pin across frame rates — reconcile the curve's design with that test's mouse-delivery model BEFORE coding (plan-stage red-team, the S7-hrz precedent). Config: curve knobs in `[ui]` beside `aim_sensitivity` (e.g. base gain + expo + a rate knee), knob-off arm = the pure linear gain, bit-identical | input/ mechanism (§0) | YES — mouse-path class (plan-stage AND diff) |
| MB-7c | **Energy legibility (red-team P1-6; Chad RULED Q6 + 2026-07-08 additions):** read-only, render/app-side, firewalled like the gunsight — (i) **wind-audio pitch/volume ∝ airspeed** (Chad: "esp the wind audio" — the bleed becomes audible; a dive roars, a zoom apex goes quiet; thin air may hush it slightly so the ceiling is audible too); (ii) **the AoA DIAL** (Chad 2026-07-08, his words): "an elegant in-line ANALOGUE artful dial showing AoA, sort of augmented-reality, big in size, offset to the side, but easy for the user to perceive the AoA" — an AR-style analogue gauge, not a number: needle sweeping toward the stall arc (aoa_max/stall bands marked), big, parked off-center; reads the SHARED `render/readout.h` extraction (never a re-derived AoA — the flat-instrument rule); (iii) trailing wingtip VORTICES near stall AoA / high G ("to show stall"); (iv) elegant HUD layout for altitude/speed/G with a speed-TREND cue. Explicitly **NO wing shake and NO camera shake** (both ruled out). NEVER control-path | render/audio/HUD item | no (read-only; S8 gunsight precedent) |
| MB-8 | E3 deflection drag (+ AT-12 extension) — ONLY if Chad opts in after flying E2 (§R.4: no genre precedent; induced drag may already deliver the felt intent) | plant mechanism (§0) | YES — integrator/energy class |
| MB-flaps | **Combat + landing flaps (Chad 2026-07-08: "still on the flight kernel — combat flaps and landing flaps").** Plant mechanism (SPEC §0): a 3-position flap state (clean / combat / landing), pilot-keyed, plant-side deploy SLEW (the throttle-slew precedent — flaps move over ~1-2 s, physics not UI). Aero shape v1: a lift-curve SHIFT `Cl += dCl_flap` (combat ~+0.3, landing ~+0.8 — lowers stall/corner speed at the same AoA cap) + a drag tax `Cd0 += dCd0_flap` (combat small, landing large) — force-only, so the torque model, controller inversion, and AT-18 are untouched; AT-12's mirror gains the same terms (the atm precedent). Combat flaps = the turn-fight dial (tighter sustained turn, pays in speed — pure combat-egg trade); landing flaps = the slow-flight/landing tool (no landing in v1, but slow-flight feel + the stall-recovery envelope benefit now). Speed guard: above a `v_flap_max`, auto-retract or effectiveness-compress (hysteretic; decide at plan time — ASK Chad which felt behavior). Instructor stays passthrough (flaps are pilot-managed lift/energy, the throttle precedent); `aoa_max` cross-check at load vs the flapped stall AoA | plant mechanism (§0) | YES — plant/energy class |
| MB-9 | E4 `Cd0` if needed; holistic re-fly of the whole envelope vs the drone fleet | dials | no |

Ordering rationale (red-team P1-4 applied): mechanisms first (MB-1/2 — structural, red-teamed
together), then the INNER LOOP before any feel dial ("don't tune feel on an untuned inner loop" —
the flight-log's own rule; `yaw_scale` is a feel dial and D2's skid is only as crisp as yaw-rate
attainment), then feel dials, then the envelope shift (E1 moves the speed regime the pitch dials
were tuned in — tune attainment at today's speeds, shift the envelope, re-check at MB-9). Chad
listed rudder first as an outcome priority, not a tuning sequence.

## 4. Questions for Chad — RULED (Chad, 2026-07-07; his words govern)

**Rulings:** Q1: **65°/s** (the shared ceiling; instructor top-end capped with it — accepted).
Q1b: **"nose should crab immediately and bank immediately"** → gate shape **(b)** confirmed (full
rudder through the roll-in; fade only past knife-edge). Q2: **`n_max` stays 16** — "10 g" was felt
attainment, so D3 delivers it; `[g_limits]` untouched. Q3b: **compression scales with the new top
speed** — "like regular physics but a little less… arcade… yes to compress especially at the
topmost speeds": noticeable, never disabling; the design intent is DIVE JUDGEMENT (a steep diver
pays in compression and can miss the pull-up; a defender can reverse a high diver; the smart play
is a shallower dive that keeps turning ability) → MB-7 gains a compression-knee rescale dial
(`v_full`/`v_redline` up with the envelope, `min_frac` stays arcade — start `v_full` 111→140,
`v_redline` 208→245, so full authority ≤ ~500 km/h, ~0.6 at the new level top, hard compression
only in steep dives). Q4: not directly ruled — fly the D4 default, present the porpoise/exit-speed
choice at the stick. Q5: deferred until E2 is flown (as planned). Q6: **YES, expanded** — wind
audio ∝ speed, **trailing vortices to show stall**, "elegant HUD" (altitude, speed, G, etc.),
explicitly **NO wing shaking** → MB-7c scope below. NEW felt targets (test-card maneuvers):
**a sustained CLIMBING SPIRAL when energy is high enough, and a well-controlled, beautiful
DESCENDING SPIRAL** — these become the E1/E2/D3 grading maneuvers alongside the drone chase
("an elegant experience").

*(Original questions kept below for the record.)*

1. **Override rudder nerf target — WITH the coupling (red-team P1-2):** the keyboard rudder tops at
   90°/s today, and under requirement 4 ("highest deflection ALWAYS keyboard") your nerf number
   becomes ONE shared ceiling that ALSO caps the instructor's maximum rudder. The instructor's
   effective max today is ~86°/s at big deflections. So: ~65°/s nerfs the keyboard AND cuts the
   instructor's top-end rudder ~25% (while D2 raises its small/mid-deflection rudder ~67%); ~85–90
   keeps the instructor's top end and barely nerfs the keyboard. Name the number knowing it moves
   both.
   **1b (felt intent, red-team P1-1):** "rudder based on the angle of my deflection" — does the
   nose start crabbing toward the aim IMMEDIATELY on a big lateral flick, while the wings are still
   rolling in (today's behavior, gate shape (b)), or should the rudder WAIT for the bank and come
   in with the established turn (shape (a))? Plan starts with (b) — today's mid-roll crab kept,
   only the past-knife-edge corkscrew region gated.
2. **"Up to 10 g's is okay":** the instantaneous clamp is ALREADY `n_max`=16 (raised 2026-07-05 for
   pitch-rate at speed). Do you mean (a) the felt/sustained pull should REACH ~10 g (then the fix is
   attainment — D3 steps 1–3 — and `n_max` stays 16), or (b) clamp it AT 10 for predictability
   (then `n_max` 16→10, and pitch rate at 300 m/s drops ~26→~17°/s — the reason it was raised)?
3. **Top speed:** name the target. Plan assumes ~740 km/h (205 m/s) level — but note T_max=9000
   actually trims out at ~210 m/s, PAST `v_redline` 208 (red-team P2-6): the entire new top end
   then flies at `min_frac`=0.4 authority (heavy controls). **3b:** should the compression knee
   (`v_full`/`v_redline`) move UP with the new top speed (keep controls lively at the new cruise),
   or is heavy-at-redline the desired dive-compression flavor?
4. **Stall recovery feel:** "instructor regains coordination" = the plan's nose-drops-into-the-fall,
   speed rebuilds, then it comes back to your cursor — realistically costing **several hundred
   meters** of sky at today's thrust (less after E1). Confirm that matches the picture (vs. e.g.
   wings-level-first, which touches the auto-level graveyard — not proposed). Also 4b: if you hold
   the aim skyward after recovery, it will climb, re-stall, and recover again (a porpoise) unless
   we raise the ballistic exit speed (~40→55) — porpoise or higher exit, your call at the stick.
5. **Jink tax strength:** at full triple-axis deflection, roughly how fast should energy visibly
   bleed — "noticeable in a sustained defensive weave" (start value) or "punishing"? (One
   coefficient; we'll fly it against a drone chase.) Asked only if you opt into E3 after flying E2.
6. **Energy legibility (red-team's "ONE addition"):** the energy game is only a game if you can
   perceive the bleed. v1 proposal: a speed-trend cue on the HUD + wind-audio pitch ∝ airspeed
   (read-only, render-side). In or out for this mission?

## 5. Verification plan (beyond per-item tests above)

- Gate after every item (build + ctest); goldens move on every TOML dial — re-record deliberately,
  eyeball the diff (flight-log discipline). Delete the stale-exe before mutation-verifies
  (the locked seads_tests.exe relink trap).
- Three §0 supersessions (D2, D4, E3) each get ONE scoped fresh-context Fable 5 diff red-team
  (adversarial-review skill), iterate on P0/P1 only.
- AT-15's banner constants: D2 touches the yaw fade AT-15 premises lean on — re-derive which knob
  each leg pins (the S7-push false-mutation lesson) and update the banner set.
- New-mechanism firewall legs: every knob-off arm (yaw gate at `yaw_min_frac=…` n/a — D2 has no off
  knob, its "off" is `yaw_scale` retune reverted; D4 `recover_to_wind=0`; E3 `Cd_ctrl=0`) proves
  bit-identity where claimed — differential AND absolute (the S8-drone P1 lesson).
- Fly protocol: every dial = a flight-log row (hypothesis first), stars, tag builds the hands liked.
  Tracking star graded against the drone fleet + gunsight TOT%. Test-card maneuvers (Chad-ruled):
  the SUSTAINED CLIMBING SPIRAL (only if energy is high enough — the Ps > 0 check made felt) and
  the controlled, beautiful DESCENDING SPIRAL, flown every E1/E2/D3 flight alongside the drone
  chase and the split-S/flick staples.

## 5b. Red-team ledger (fresh-context Fable 5, plan-stage, one scoped round — 2026-07-07)

Verdict: **SOUND-WITH-FIXES**. All P0/P1 findings are FIXED IN THIS TEXT (P0-1 blend-composed
gate → §D2; P1-1 two gate shapes + Q1b → §D2/§4; P1-2 shared-ceiling coupling + corrected D1
rationale → §D1/§4; P1-3 binary recovery knob + antiparallel leg → §D4; P1-4 inner-loop-before-
feel-dials reorder → §3; P1-5 integrator-seed AT-15 re-run + pre-registered fallback → §D2/§3;
P1-6 energy legibility → MB-7c/Q6). P2 findings folded as in-text notes (P2-1 coordination
residual → D1; P2-2 exit speed → D4; P2-3 altitude honesty → D4/Q4; P2-4 yaw PIO instrument →
D2/MB-4; P2-5 premise checklist → MB-3; P2-6 compression knee → Q3b; P2-7 E3 numbers verified:
sustained-5g elevator trim taxes ~0.8% of the induced term, housekeeping ~10⁻⁶ — the quadratic
keeps the leak negligible, recorded so MB-8's red-team needn't re-litigate). P3: yaw_scale moves
inside BOTH call sites (→ D1); MB-7 split into 7a/7b (→ §3); removing `yaw_maneuver_frac` touches
loader (`load_controller.cpp:118,191`), `params.h:133`, and the STALE comments that canonize the
old ceiling (`controller.toml`, `dogfight-systems/SKILL.md:87,139`) — update them with MB-2.
Headline (Front A): the transformative items are D3 (the never-tuned inner loop) + E1/E2 (a real
combat egg); the fixed weakest link was energy legibility (now MB-7c).

## 6. Considered and REJECTED (so the red-team doesn't re-propose them)

- **Aim-rate feedforward** (2-DOF tracking boost without raising `K_theta`): the aim moves in
  FRAME-quantized steps — a per-tick aim-delta term is frame-rate-dependent (breaks AT-9's
  bit-identity, the S7-mouselevel class), and smoothing it feeds a filtered signal into ω_des
  (§9.1 ban). Dead unless AT-9 itself is renegotiated. NOT proposed.
- **Raising `K_theta` past 3.2** — the PIO wall (AT-2 ring, deadzone hunt). Dead.
- **`pursuit_step` > 0** (near-centre snap floor) — rings AT-2 (documented in the TOML). Dead.
- **Sideslip-β drag term** — deferred; the deflection tax covers the intent with one mechanism (§D5.3).
- **Instructor-managed energy** (auto-throttle in turns) — violates throttle passthrough. Dead.
- **Deck-aware stall recovery** (inhibit recovery near ground or boost it) — Chad explicitly wants
  natural consequences; altitude-blind by design.

## R. Deep-research findings (25 claims, 3-vote adversarially verified, 0 killed; 21 sources)

**R.1 — War Thunder's Instructor** (archived official wiki "How the instructor works" +
warthunder.com/en/news/4366; the live wiki page is dead — archive-cached, the only primary source):
a full-authority envelope-protection layer — critical-AoA prevention, anti-stall/anti-spin,
auto-trim (released controls hold trajectory — the deadzone-holds-trim pattern SEADS already
ships), torque compensation, and **G-onset shaping** ("slows down g buildup" near the structural
limit — behavior documented, mechanism NOT). Manual keyboard input is layered UNDER the protection
and never bypasses it — which SEADS has matched since S7-ovr (override drives to the in-envelope
max). WT's actual rudder LAW is not publicly documented anywhere that survived verification.

**R.2 — The two open reference designs converge on the rudder pattern** (github.com/brihernandez/
MouseFlight; github.com/tetryds/MouseAimFlight): rudder = a continuous proportional term on the
body-frame LATERAL aim error at ALL deflections (MouseFlight: `yaw = clamp(localFlyTarget.x)`,
no regime gate); corkscrew is prevented in the ROLL channel (wings-level↔bank-into-target blend
scheduled on angle-off-target — SEADS's blend/bank_align_power analog), NOT by fading the rudder.
MouseAimFlight adds a sideslip-correction subtracted inside the roll error and CROSS-AXIS
anti-windup ("yaw integrals should be zeroed at the same time [as pitch]... because that happens in
large turns"). Direct support for D2's direction: free the rudder when tracking, kill the corkscrew
by bank geometry. (Caveat: MouseAimFlight's richest machinery lives in its superseded `Old/` tree —
iterated design precedent, not shipped tuning. MouseFlight also feeds a SMOOTHED camera basis into
its aim update — the exact RA9-banned pattern; reported as contrast, not recommendation.)

**R.3 — Boyd E-M theory** (the 1966 USAF report, everyspec.com): sustained turn is capped by the
specific-excess-power budget (Ps = 0); harder pulls buy instantaneous turn at negative Ps — speed/
altitude bleed. The transferable instrument is the turn-rate-vs-speed "combat egg" chart with Ps
contours: `k_induced` IS SEADS's Ps knob (induced drag rises with Cl² ∝ n²), `T_max` moves the
Ps=0 line up. Optional harness instrument (not a gate, HARNESS §4 class): an `em_sweep` printout —
sustained turn rate vs V at Ps=0 — so E1/E2 dials are tuned against the doctrine chart Chad named.

**R.4 — No deflection-drag precedent exists in any shipped arcade title** (verified across WT, IL-2
arcade, Crimson Skies, Ace Combat, WoWP, Sky Rogue, TCA, Project Wingman): the universal jink tax
is induced-drag-via-G. Closest precedent is Tiny Combat Arena's MISSILE energy model (gravity +
burnout + speed-loss-per-turn — deliberately exaggerated, legible rules; skywardfm.com interview).
Hence E3's re-ordering: fly E2 first; E3 is an innovation Chad opts into, not table stakes.

**R.5 — PIO doctrine, quantitative** (McRuer, NASA CR-4683): (a) keep aim-to-response
rate-command-like (K/s needs no pilot lead — the rate-PI cascade already is); (b) total effective
delay < 0.1 s design target, > 0.25 s is PIO-prone (SEADS: 120 Hz tick + 0.05 s AoA filter off the
tracking path — inside budget); (c) **rate limiting appears in almost all severe PIOs** — it
manufactures amplitude-dependent lag ("Be not stingy with rate limits!") — the citable reason this
plan does NOT add a WT-style G-onset rate shaper (SEADS's braking law + seek_law already shape
onset WITHOUT an in-loop rate limit), and why D1's nerf is a command CLAMP, not a slew; (d) severe
PIOs are triggered by TRANSITIONS (mode switches step-changing gain/lag) — independent validation
of the "every gate hysteretic" rule and the blend/fade-over-latch pattern this plan reuses (D2's
gate is a continuous fade, D4 inherits the ballistic latch).

**R.6 — Envelope-protection placement** (AIAA JGCD 2024, arxiv.org/pdf/2406.01246, F-16 sim):
protection as saturation of OUTER-loop commands upstream of the rate loop, limits computed
dynamically against attainable moment (deflection + rate limits) — expanded the usable envelope
34–55% vs fixed tables. Architectural validation of exactly where SEADS clamps (ω_des, upstream of
rate-PI + plant inversion) and of authority-aware clamps (the G/AoA clamps already recompute per
tick from V, cosΦθ, filtered α).
