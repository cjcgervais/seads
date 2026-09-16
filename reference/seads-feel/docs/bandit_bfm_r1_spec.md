# Bandit BFM — rung R1 spec (the record, this base)

This document records the R1 implementation as it was PORTED onto
`sandbox/bfm-r1` (this tree — the "solid-ground"/maverick/raid/leash/atmosphere
world lineage), from a gate-green reference build authored against a different,
older code base. The reference's own `bfm::AimCmd` + `bfm::aim_at` do NOT exist
here — this tree already had a shared steering primitive
(`maverick::aim_at`, `drone/maverick.h:384`), so BFM reuses THAT instead of
carrying a second copy. See **Deltas from the reference** below for every place
this build diverges from the reference implementation's own choices.

## GOAL

A committed, dwell-latched BFM state machine so an ENGAGED bandit flies
energy-aware, graceful dogfight geometry (lag pursuit, high yo-yo, energy
extend) instead of bare pure pursuit. Decisions only — the bandit flies the same
plant through the same `drone::autopilot`/autothrottle/stall-limiter seam.
Default OFF = bit-identical to today.

## DESIGN — drone/bfm.h

`struct BfmParams { bool enabled = false; /* all dials below, with these
defaults, degrees stored as radians (converted at the TOML boundary like the
existing `[maverick]`/`[combat]` precedent) */ };`

Dials (defaults): `attack_range_m=1200`, `attack_release_frac=1.4`,
`lag_dist_m=220`, `lag_off_lo=20deg`, `lag_off_hi=60deg`,
`yoyo_closure_mps=40`, `yoyo_angle=45deg`, `yoyo_arm_s=0.5`,
`yoyo_gamma=35deg`, `yoyo_bank_frac=0.5`, `yoyo_time_s=3.5`,
`yoyo_exit_closure_mps=10`, `extend_energy_m=600` (specific-energy deficit,
metres), `extend_gamma=-10deg`, `extend_min_s=4`, `extend_max_s=10`,
`reenter_energy_m=200`, `frustration_s=20`, `intercept_speed_bump=25` (m/s,
Intercept/Extend only — see the deltas), `min_dwell_s=2.0`.

`struct BfmState { enum class Mode { Intercept, Offensive, Yoyo, Extend }; Mode
mode = Mode::Intercept; long long mode_ticks = 0; long long arm_ticks = 0; /*
yoyo overshoot-predictor arming counter */ };`

`struct BfmGeom { double range, closure, angle_off, e_delta; glm::dvec3 los;
bool degenerate; };`

`fn bfm_geom(const sim::SimState& s, const sim::SimState& player, const
sim::AircraftParams& ap)`:

- `rel = player.position - s.position; range = |rel|; los = rel/range` (guard
  `range < 1e-6`: return a degenerate-flagged geom the caller treats as
  hold-current-mode, zero command).
- `closure = -dot(player.velocity - s.velocity, los)` (positive = closing).
- `angle_off = acos(clamp(dot(nose, los), -1, 1))`, `nose = s.orientation *
  (0,0,-1)`.
- `e_delta = (alt_s + Vs^2/(2g)) - (alt_p + Vp^2/(2g))` using `sim::altitude`
  and `ap.g` — never a hardcoded 9.81.

`struct BfmCmd { double target_bank, target_gamma, speed_target; bool
guns_hot; };`

`fn bfm_step(BfmState& st, const sim::SimState& s, const sim::SimState& player,
const BfmParams& bp, const BfmDials& dl, const sim::AircraftParams& ap, double
dt) -> BfmCmd`. `BfmDials` is the small POD of DroneParams-owned dials the
machine needs, passed explicitly so `bfm.h` never has to include
`drone/drone.h` (the layering rule below): `k_az, k_el, max_bank, max_gamma,
lead_speed, lead_max_s, speed, pursue_speed_bump` — all mirroring
`DroneParams::pursue_*` fields this tree already owns (this base already has
`pursue_lead_max_s`, unlike the tree the reference was built on — no new lead
dial was needed).

NOTE on layering: `bfm.h` does NOT include `drone/drone.h`; `drone.h` includes
`bfm.h`.

**Steering primitive:** every mode call site steers via `maverick::aim_at(s,
aim_point, k_az, k_el, bank_cap, gamma_cap) -> maverick::Steer{target_bank,
target_gamma}` (`drone/maverick.h:384`), THIS tree's existing shared bank-to-
turn + climb/dive law. `bfm.h` carries no `AimCmd` type and no re-derived
bank/gamma law of its own.

**Lead point single-source:** `inline glm::dvec3 lead_point(const
sim::SimState& s, const sim::SimState& target, double lead_speed, double
lead_max_s)` lives in `bfm.h`, extracted VERBATIM from `drone::pursue()`'s own
lead math (`drone/drone.h:576-581` before this port). `pursue()` now calls it
instead of computing `t_lead`/`aim` inline — every existing `pursue()` unit
test stays green, the refactor's no-op evidence.

### STATES

Every transition below is subject to: `mode_ticks >= min_dwell_s/dt` before ANY
exit, except entry into Yoyo which uses its own arm counter; on every mode
change reset `mode_ticks` and `arm_ticks`.

**Intercept** (initial):

- cmd: `maverick::aim_at(s, lead_point(s, player, dl.lead_speed,
  dl.lead_max_s), dl.k_az, dl.k_el, dl.max_bank, dl.max_gamma)`;
  `speed_target = dl.speed + bp.intercept_speed_bump`; `guns_hot = false`.
- -> Offensive when `range < attack_range_m`.

**Offensive**:

- Pure<->lag blend: `w = smoothstep(lag_off_lo, lag_off_hi, angle_off)`; `aim =
  mix(lead_point, lag_point, w)` where `lag_point = player.position -
  player_vhat * lag_dist_m` (guard `|player.velocity| < 1 m/s`: `lag_point =
  player.position`). Steer via `maverick::aim_at` with the pursuit gains/caps.
  `speed_target = dl.speed + dl.pursue_speed_bump`. `guns_hot = true`.
- -> Yoyo: (`closure > yoyo_closure_mps` AND `angle_off > yoyo_angle`)
  sustained `arm_ticks >= yoyo_arm_s/dt` (increment when both hold, reset to 0
  when either fails — the overshoot predictor).
- -> Extend: `e_delta < -extend_energy_m`, OR `mode_ticks > frustration_s/dt`.
- -> Intercept: `range > attack_range_m * attack_release_frac` (hysteretic
  release).

**Yoyo** (committed climb — convert closure to altitude):

- cmd: bank = toward player capped at `yoyo_bank_frac * pursue_max_bank`
  (via `maverick::aim_at` at the player position, then clamp the returned
  bank); `target_gamma = +yoyo_gamma` (overrides `aim_at`'s gamma);
  `speed_target = dl.speed + dl.pursue_speed_bump`; `guns_hot = false`.
- exit (after min dwell): `closure < yoyo_exit_closure_mps` OR `mode_ticks >
  yoyo_time_s/dt` -> Offensive.

**Extend** (committed energy rebuild):

- cmd: `target_bank = 0`, `target_gamma = extend_gamma` (shallow unload dive,
  continue current heading); `speed_target = dl.speed +
  bp.intercept_speed_bump`; `guns_hot = false`.
- exit: (`mode_ticks >= extend_min_s/dt` AND `e_delta > reenter_energy_m`) OR
  `mode_ticks > extend_max_s/dt` -> Intercept.

## WIRING — drone/drone.h

`pursue()` (:560) is UNCHANGED except its lead math now calls
`bfm::lead_point(s, player, dp.pursue_lead_speed, dp.pursue_lead_max_s)` in
place of the old inline `t_lead`/`aim` computation — bit-identical, single
source.

`DroneParams` gains `bfm::BfmParams bfm{};` (beside `maverick`); `DroneState`
gains `bfm::BfmState bfm{};`, reset to `{}` in `respawn_in_place` (same pattern
as `mav`).

`tick()`'s engaged branch (inside `if (!maverick_committed) { if (player &&
d.engaged) { ... } }`): when `dp.bfm.enabled`, build a `bfm::BfmDials` from
`dp`'s pursuit fields, call `bfm::bfm_step`, and take `target_bank`/
`target_gamma`/`speed_target` from the returned `BfmCmd`; `bank_p =
dp.pursue_bank_gain`; `level_p = dp.pursue_pitch_gain`; `aoa_protect = true`;
`d.wants_fire = bc.guns_hot && drone::pursue(d.curr, *player, dp).fire` (fire
DISCIPLINE — cone/range/coordination — stays single-source in `pursue()`; BFM
only supplies the guns-hot veto). When `!dp.bfm.enabled`: the existing pursue
path, byte-for-byte unchanged. Everything downstream of the engaged branch
(raid/leash, the flyable-air ceiling + air-seek dive, hard-deck terrain
avoidance, the pursuit climb-throttle feedforward, the autopilot, the
autothrottle) is UNTOUCHED and unreordered — BFM only ever changes what feeds
`target_bank`/`target_gamma`/`speed_target` at the top of the branch; every
downstream override still overrides.

## CONFIG

`scenario.toml [combat]`: `bfm_enabled = false` plus the dials (degrees in the
TOML where the param is an angle; suffix `_deg` in the TOML key, converted
once at the loader boundary — the existing `[maverick]`/pursuit precedent).
Loader validation (`config/load_scenario.cpp`): `bfm_yoyo_gamma_deg` in `(0,
80]`; `bfm_extend_gamma_deg` in `[-45, 0)`; every dwell/time dial `> 0`;
`bfm_attack_release_frac > 1`; `bfm_yoyo_exit_closure_mps <=
bfm_yoyo_closure_mps` (so a yo-yo can always end before its own entry
predicate can re-arm); fail loud on violation (this loader's house style).

## TESTS — test/unit/test_bfm.cpp (ASCII names only)

1. **OFF-ARM DIFFERENTIAL**: `dp.bfm` default `{}` => assert `dp.bfm.enabled ==
   false`, and a differential leg: tick a drone with `bfm.enabled=false` and
   assert its DroneState (curr position/orientation/velocity, wants_fire) is
   bit-identical to a copy ticked through the same N with a BfmParams carrying
   different dial values but `enabled=false` (proves every dial is dead while
   disabled).
2. **OPEN-LOOP TRANSITION PINS** (the S6 discipline — drive predicates on FIXED
   synthetic states, never a closed-loop chase): (a) min-dwell: a state whose
   exit predicate holds from tick 0 exits exactly at `ceil(min_dwell_s/dt)`
   (config-derived count, +/-1); (b) yoyo arm: closure/angle_off oscillating
   across the thresholds at every-other-tick never arms; holding both for
   `yoyo_arm_s` arms exactly once; (c) attack-range hysteresis: range
   dwelling+rippling around `attack_range_m` produces 1 transition, and re-entry
   requires crossing `attack_release_frac` (mutation lever: set release frac to
   1.0 and the ripple leg must fail).
3. **YOYO ENERGY EXCHANGE** (non-vacuous): closed-loop 1v1, forced overshoot
   geometry (bandit fast + player crossing); REQUIRE the machine actually enters
   Yoyo (baseline > eps discipline), then during the Yoyo dwell assert speed
   decreases AND altitude increases (the trade), and AoA never exceeds the plant
   stall angle (the limiter holds — derived from the airframe params,
   config-relative, never a literal).
4. **LAG BLEND SHAPE**: probe `w` at `angle_off = lag_off_lo` (w==0
   boundary-exact), at 25% into the band (mid-band asymmetric point — NOT the
   midpoint, which is the smoothstep-flip fixed point), and at `lag_off_hi`
   (w==1). Assert the aim point moves monotonically from `lead_point` toward
   `lag_point`.
5. **DEGENERACY**: coincident player (range<eps) -> zero command, guns cold,
   mode held, no NaN; near-stationary player -> `lag_point` guard holds, no NaN.
6. **LEAD SINGLE-SOURCE**: `bfm::lead_point`'s value against the hand-computed
   `t_lead` formula including the cap, PLUS a leg pinning `drone::pursue()`'s
   bank to the bit-exact composition `maverick::steer_bank(s, normalize(
   bfm::lead_point(...) - s.position), pursue_k_az, pursue_max_bank)` —
   `pursue()`'s own inline bearing law on this tree is, expression-for-
   expression, `maverick::steer_bank`, so this pins the fed-in lead point is
   the single source without duplicating a second law inside the test. The
   existing `drone::pursue` cases in `test_drone.cpp` are the other half of
   this evidence (they pin the lead-refactor as a no-op through pursue's own
   front door).

Every leg: REQUIRE non-vacuous baselines. Fixtures follow this tree's
`test_drone.cpp` patterns (`kAp`, `drone::spawn_drone`, `nose_of`/`right_of`).

## GATE

Build + full ctest green. ZERO moved goldens (if any golden fails, STOP and
report — do not re-record).

---

## Deltas from the reference implementation

The reference was built against an older tree (`sandbox/kernel-v5-reconcile`)
with neither `drone/maverick.h` nor `tools/graph/`. This base has both, plus a
substantially larger `drone::tick()` (maverick tunnel runs, conquest raid/
leash, terrain avoidance, the flyable-air ceiling, climb-throttle
feedforward) and an already-present `DroneParams::pursue_lead_max_s`. Ported
deltas, all deliberate (per the port instructions, not independent
re-design):

1. **`bfm::AimCmd` + `bfm::aim_at` (the reference's own steering type) are
   DELETED.** This tree's `maverick::aim_at` (`Steer{target_bank,
   target_gamma}`, no `pull_gain`/`to_hat`/`valid`) is the single steering
   primitive every BFM mode call site uses instead. `maverick::aim_at`'s own
   `len < 1e-6` guard covers the aim-point-degenerate case (zero `Steer`);
   `bfm_geom`'s own `range < 1e-6` guard covers the player-coincident case —
   between the two, no `valid`/NaN-guard flag was needed on the steering
   return.
2. **`bfm::BfmParams::offensive_speed_bump` is DROPPED.** This tree already
   owns `DroneParams::pursue_speed_bump` for the in-fight speed (the pre-BFM
   pursuit path uses it too); Offensive/Yoyo read it via `BfmDials::
   pursue_speed_bump` instead of carrying a second copy. Intercept/Extend keep
   their OWN `BfmParams::intercept_speed_bump` (a run-in/energy-rebuild speed,
   independently tunable from the in-fight bump).
3. **`bfm::BfmDials` carries `lead_max_s` from day one** (mirroring
   `DroneParams::pursue_lead_max_s`, which this base already has) — the
   reference tree lacked that dial and its BFM machine passed a hardcoded
   `0.0` (uncapped) lead everywhere; here Intercept/Offensive's lead point is
   capped exactly like `pursue()`'s own fire-solution lead.
4. **`pursue()`'s bank/gamma steering law itself is NOT refactored to call any
   shared `aim_at`.** Only its lead-point math changed (now
   `bfm::lead_point(...)`, replacing the old inline `t_lead`/`aim`
   computation) — its bank/elevation/pull math stays exactly as it was,
   byte-for-byte, per the port instructions. The single-source pin for
   `pursue()`'s bank is therefore against `maverick::steer_bank` (the shared
   bearing-only primitive `pursue()`'s inline law already reproduces
   expression-for-expression), not against a BFM-owned `aim_at`.
5. **One extra loader check, inherited from the reference's own note:**
   `bfm_yoyo_exit_closure_mps <= bfm_yoyo_closure_mps`, so a yo-yo can always
   end before its own entry predicate re-arms.

## Red-team folds (post-port fix batch)

A red-team pass on the ported machine found a P1 limit cycle and several
untested/incorrect claims. Folded, in order:

1. **FIX-1 (P1, Offensive<->Yoyo limit cycle).** `BfmState` gains
   `fight_ticks` (continuous Offensive+Yoyo engagement time, survives the
   Offensive<->Yoyo boundary, reset only on entry to Intercept/Extend) and
   `yoyo_cooldown` (armed to `dwell_ticks(min_dwell_s, dt)` on every
   Yoyo->Offensive exit; while > 0 the yo-yo predictor's `arm_ticks` is held
   at 0). Without this, a bandit whose overshoot predictor stays continuously
   satisfied could leave Offensive again the instant it returned from a
   yo-yo (re-entry only ever needed `yoyo_arm_s`, well under `min_dwell_s`),
   spending the whole fight ping-ponging with ~zero guns-hot time. The
   frustration exit is now keyed to `fight_ticks`, not `mode_ticks`, so that
   same stuck bandit still eventually breaks off to Extend instead of
   resetting its clock on every Yoyo entry.
2. **FIX-2 (P1, untested claims).** Three claims the original test suite
   never actually exercised, now pinned: the `guns_hot` veto reaching
   `drone::tick`'s `d.wants_fire` (all three cold modes, plus the Offensive
   positive case); the Extend command + its exit timing (both the deep-
   deficit timeout and, see FIX-7, the recovery exit); and that
   `respawn_in_place` zeroes every `BfmState` field, including the two new
   FIX-1 counters.
3. **FIX-3 (P2, yoyo gamma escapes the envelope).** `bp.yoyo_gamma` is now
   clamped into `[-dl.max_gamma, dl.max_gamma]` at the Yoyo command site (the
   struct default retuned 35->30 deg to match `pursue_max_gamma_deg`); the
   loader also rejects a table value above `pursue_max_gamma_deg`.
4. **FIX-4 (P2, yo-yo in thin air).** `BfmDials` gains `climb_ok` (default
   true); `drone::tick` sets it from the SAME `sim::atm_frac_at` threshold
   (`avoid_air_frac_full`) the flyable-air ceiling downstream already reads.
   While `!climb_ok` the yo-yo predictor cannot arm, and an ACTIVE Yoyo
   aborts to Offensive immediately (bypassing `min_dwell` -- a maneuver the
   air cannot fly is aborted, not flown), still arming the FIX-1 cooldown on
   the way out.
5. **FIX-5 (P2, stale bfm state across disengage/maverick).** `d.bfm` is now
   reset to `BfmState{}` on every tick whose bfm branch is NOT taken (a
   committed maverick run, no player, not engaged, or bfm disabled) — a
   bandit that disengages mid-fight, loses its player pointer, or gets pulled
   into a maverick run no longer resumes a half-flown yo-yo/extend/cooldown
   when it returns to bfm control later.
6. **FIX-6 (P2, loader coverage).** New `test_load_scenario.cpp` legs: the
   committed `[combat]` bfm block loads/converts, plus three rejections
   (`bfm_attack_release_frac = 1.0`, `bfm_yoyo_exit_closure_mps = 999.0`,
   `bfm_enabled` as a string).
7. **FIX-7 (P3, `reenter_energy_m` dead dial).** Extend's re-entry predicate
   was `e_delta > reenter_energy_m` (a SURPLUS) -- for a shallow unloaded
   dive that essentially never happens before the `extend_max_s` timeout, so
   every real exit was via the timeout and the dial was dead. Changed to
   RECOVERY semantics: `e_delta > -reenter_energy_m` (the residual deficit
   has shallowed to better than `-reenter_energy_m`). Loader adds
   `reenter_energy_m < extend_energy_m` (the recovery threshold must be a
   shallower deficit than the one that triggered the extend).
8. **FIX-8 (P3, yoyo energy-trade test half non-discriminating).** The
   closed-loop trade leg gained a control arm: identical setup with
   `yoyo_gamma = 0` (struct-level), asserting the real arm's altitude gain
   exceeds the control arm's by a real margin -- the original leg would have
   passed even if the Yoyo command dropped its gamma override, as long as
   some maneuver gained a little altitude by chance.
9. **FIX-9 (P3, `lead_max_s` semantics).** One pinned line: `lead_max_s <= 0`
   means UNCAPPED (matched against a huge-cap stand-in), the intentional
   semantics change from the old inline `std::min(t_lead, lead_max_s)` call
   sites this was ported from.
10. **FIX-10 (P3, config-relative disarms).** Loader adds
    `bfm_yoyo_arm_s < bfm_yoyo_time_s` and `bfm_attack_range_m >
    fire_range_max` (else Offensive's merge range never overlaps the range
    band `pursue()` requires to fire, and guns never go hot at all).

## Red-team folds, round 3

A third red-team pass on the air-ceiling wiring found a P1 flap and several
untested/leaking dials. Folded, in order:

1. **FIX-A (P1, air-abort flapping — replaces the round-2 `climb_ok` step).**
   `BfmDials::climb_ok` (a bare bool) is replaced with `climb_t` (a double,
   default `1.0`): the SAME flyable-air RAMP value `drone::tick`'s downstream
   ceiling block computes from `sim::atm_frac_at` — `t = clamp((frac -
   avoid_air_frac_hard)/(avoid_air_frac_full - avoid_air_frac_hard), 0, 1)` —
   not a re-derived threshold comparison. `drone::tick` now hoists ONE
   `atm_frac` sample before the bfm branch and feeds the SAME value to both
   the bfm `climb_t` computation and the downstream ceiling block (no more
   two independent `atm_frac_at` calls that could read different positions on
   the same tick). `BfmParams` gains two dials with a HYSTERESIS band (the
   house rule — every gate hysteretic): `climb_arm_t` (0.6, Yoyo may ARM only
   above this) and `climb_abort_t` (0.25, an ACTIVE Yoyo aborts only below
   this, well below `climb_arm_t`). Without the band, a single shared
   threshold flapped: a yo-yo's OWN climb crosses the ramp boundary as it
   climbs, so killing an in-progress yo-yo the instant it dipped below the
   SAME value the arm gate used re-armed and re-killed it every tick (a
   measured 1-tick flap loop). Between the two thresholds the yo-yo now
   COMPLETES on its normal exits (closure killed / `yoyo_time_s`) — a dip
   into the band no longer touches it. Loader adds `bfm_climb_arm_t` /
   `bfm_climb_abort_t` with `0 <= abort_t < arm_t <= 1`. The header banner
   now documents BOTH ordering-contract exceptions explicitly (Yoyo entry via
   `arm_ticks`; Yoyo air-abort exit via `climb_t < climb_abort_t`) — the only
   two ways a mode can beat `min_dwell_s`.
2. **FIX-B (P2, gamma clamp untested/inert).** The round-2 FIX-3 clamp
   (`bp.yoyo_gamma` into `[-dl.max_gamma, dl.max_gamma]` at the Yoyo command
   site) was implemented but never exercised at a value that actually escapes
   the envelope. New direct-construction leg: `yoyo_gamma = 80 deg`,
   `dl.max_gamma = 0.524`, forced Yoyo, one step -> `cmd.target_gamma ==
   dl.max_gamma`. Loader rejection leg: `bfm_yoyo_gamma_deg = 31.0` (>
   `pursue_max_gamma_deg` 30) throws.
3. **FIX-C (P2, `yoyo_cooldown` leaks across engagements).** `enter()` now
   also resets `st.yoyo_cooldown = 0` whenever entering `Intercept` or
   `Extend` — the two modes that actually leave the fight, alongside the
   existing `fight_ticks` reset. Without this, a stale cooldown from a
   previous engagement (e.g. a Yoyo->Offensive exit right before a Yoyo->
   Extend-via-frustration or a range-release back to Intercept) could
   silently disarm the very first yo-yo predictor of the NEXT engagement.
4. **FIX-D (P3, cooldown duty-cycle dial).** `BfmParams::yoyo_cooldown_s`
   (default 2.0, == the old `min_dwell_s` reuse) is the Yoyo->Offensive
   cooldown length, no longer piggybacking on `min_dwell_s` directly — the
   guns-hot spell between yo-yos is now its own dial, independent of the
   dwell floor against every other exit. Loader key `bfm_yoyo_cooldown_s`,
   checked `> 0`.
5. **FIX-E (P3, loader-check coverage + one missing check).** New check:
   `bfm_yoyo_time_s > bfm_min_dwell_s` — else `may_exit` (gated on
   `min_dwell_s`) can mask the `yoyo_time_s` timeout behind the dwell floor (a
   mode-length inversion). Plus rejection legs for every previously-
   uncovered check: `bfm_reenter_energy_m >= bfm_extend_energy_m`,
   `bfm_yoyo_arm_s >= bfm_yoyo_time_s`, `bfm_attack_range_m <=
   fire_range_max`, `bfm_yoyo_time_s <= bfm_min_dwell_s` (the new check
   above), `bfm_climb_abort_t >= bfm_climb_arm_t`.
6. **FIX-F (P3 minors).** (1) The Extend-timeout open-loop leg gains a
   REQUIRE that the fixture's range is ALSO past Offensive's own range-
   release radius (`attack_range_m * attack_release_frac`) — proving the
   Extend entry resolves via the Offensive transition switch's else-if
   PRECEDENCE (Extend's energy/frustration predicate checked before the
   range-release check), not because the fixture happens to dodge the range
   check. (2) `BfmParams::yoyo_gamma`'s default was already spelled `30.0 *
   kPi / 180.0` (matching `pursue_max_gamma_deg`'s 30); `DroneParams::
   pursue_max_gamma`'s comment now cross-references that mirror explicitly
   (the 0.524 value itself is unchanged — do not retune it to "fix" the
   mirror; retune `bfm_yoyo_gamma_deg` instead). (3) This appendix.

## Round-4 fold (the arm-side inversion, review round 3's P1)

Round 3's `climb_arm_t = 0.6` opened the arm gate INSIDE the downstream
air-seek dive zone: the ceiling's seek term is keyed to `frac <
avoid_air_frac_full` (not to the fade ramp), and `min(faded climb, seek)`
always picks the negative seek — so a yo-yo armed at `climb_t in [0.6, 1.0)`
(i.e. `atm_frac in [~0.52, 0.70)` at shipped dials) flew a guns-cold DESCENT
that could neither abort (climb_t far above `abort_t`) nor exit on closure
(diving raises closure), always timing out and re-arming (a measured
inversion loop). Fix (structural, review-verified free at the flap/duty-cycle
numbers): ARMING is now gated on `BfmDials::climb_arm_ok`, computed in
`drone::tick` as `atm_frac >= avoid_air_frac_full` — exactly "the air-seek
dive is OFF" — and `climb_arm_t` is DELETED (dial, TOML key, loader check).
The abort side is unchanged (`climb_t < climb_abort_t`, loader now checks
`0 <= abort_t < 1`; the structural arm boundary sits at `climb_t == 1.0`
exactly, so the hysteresis gap is guaranteed). Tests re-keyed: the ripple leg
drives `climb_arm_ok`, the in-band completion leg drops the arm gate AND dips
`climb_t` to 0.4 mid-yo-yo (must complete on the timeout, not abort), the
loader rejection legs become `climb_abort_t = 1.0` rejected plus a
`bfm_yoyo_cooldown_s = 0` rejection (the round-3 P3).
