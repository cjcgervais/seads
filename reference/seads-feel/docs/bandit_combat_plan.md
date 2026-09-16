# Bandit combat — design (v1, pre-build)

> **Fable pre-consult FOLDED (2026-07-13, verdict SOUND-WITH-FIXES).** Fixes now
> baked into the sections below:
> - **P0-A** Player death resolves PER-TICK inside `app::tick` right after
>   `combat_tick` (NOT after `step_frame`) — else ticks 2..N of a multi-tick
>   frame fly a dead corpse (still firing, frame-rate-dependent respawn = the AT-9
>   defect the crash path already forbids). It reuses the crash-branch reset so
>   `step_frame`'s existing respawn-neutralization covers the residual ticks.
> - **P1-1** enemy pool is ADVANCE-then-SPAWN (same order as `fire_tick`) so every
>   pool sweeps the same `[t,t+dt]` interval.
> - **P1-2** `respawn_in_place` also zeroes `engaged`/`engage_dwell`/`fire_cooldown`.
> - **P1-3** `pursue` epsilon-guards both degenerate normalizations (zenith/nadir
>   projection → `target_bank=0`; coincident range → no fire) — no NaN into `sim::step`.
> - **P1-4** on death-reset: deactivate the enemy pool AND an N-tick player invuln
>   counter gates the enemy→player sweep (anti spawn-camp; specified, not open).
> - **P1-5** `assign_engagements` runs PER-TICK inside `app::tick`, off post-step
>   `st.curr`, tie→lower index, hysteresis-holders keep their slots then remaining
>   fill nearest-first.
> - **P1-6** the generalized level term is EXACTLY
>   `-kLevelP*(sin_gamma - std::sin(target_gamma)) - kLevelD*w.x` (bit-identical at
>   `target_gamma=0`: `std::sin(0.0)==+0.0`, `x-0.0==x`). Re-run `test_drone` through
>   the new signature with `target_gamma=0` as the acceptance gate — no reassociation.

Turn the passive patrol drones into ATTACKERS: basic pursuit AI + return fire,
with the player now able to be shot down. Chad-ruled scope (2026-07-13):
- **Full stakes** — player gets HP, takes gun damage, and death → respawn
  airborne (reuse the existing crash-reset path). The dogfight can be LOST.
- **Basic pursuit** — steer the nose at the player, fire when roughly nose-on
  in range. Simple + readable; richer ACM (break turns, lead, energy) is a
  later iteration.
- **Difficulty-scaled** — a `difficulty` knob sets how many bandits hunt at once
  (+ their fire cadence). Nearest-N engage; the rest patrol.

**Firewall (the one hard rule):** everything here is WORLD-THREAD — `drone/`,
`combat/`, `weapon/`, and `app/` glue only. NOTHING touches `sim/` or `control/`
(the frozen kernel). Bandit AI reads the player `sim::SimState` READ-ONLY; enemy
rounds and player-HP live in world/combat state; player death is handled by the
app calling the SAME reset a crash already does. No new kernel coupling, no
clock, no rng (determinism: cadence = cooldown counters, geometry = pure funcs).

## Components

### 1. Perception + engagement selection (pure, `drone/`)
- `drone::assign_engagements(drones, player, max_engaged)` — a pure pass that
  flags the nearest `max_engaged` bandits as `engaged` (by chord distance to the
  player), the rest `engaged=false`. **Hysteretic** (house rule): an engaged
  bandit stays engaged until it exceeds a DISENGAGE range > the ENGAGE range, so
  the flag can't chatter at the boundary. `DroneState` gains `bool engaged` +
  `double engage_dwell` (or reuse age) for the hysteresis.
- **Called PER-TICK inside `app::tick`** (P1-5 — NOT once per render frame; a
  per-frame engaged set updates at render rate and diverges trajectories across
  frame rates = an AT-9 breach). Uses the POST-STEP player `st.curr`. Slot rule:
  currently-engaged bandits still inside DISENGAGE range KEEP their slots first;
  remaining `max_engaged` slots fill by nearest chord distance; tie → lower drone
  index (kill.h strict-`<`). `max_engaged` from the difficulty mapping.

### 2. Bandit AI outer loop (pure, `drone/`)
- Generalize the autopilot to hold a commanded FLIGHT-PATH ANGLE, not just level.
  The level term becomes EXACTLY (P1-6, no reassociation):
  `in.pitch = clamp(-kLevelP*(sin_gamma - std::sin(target_gamma)) - kLevelD*w.x, ±1)`.
  At `target_gamma=0` this is bit-identical to today (`std::sin(0.0)==+0.0`,
  `x-0.0==x`), so patrol/goldens are unmoved. `test_drone` MUST be re-run through
  the new `autopilot(s, ap, target_bank, target_gamma)` signature with
  `target_gamma=0` as the acceptance gate. Sign: too-shallow (sin_gamma below
  target) → negative error → positive pitch input → pitch-up = +wx (§7 body table).
- New pure `drone::pursue(s, player, ap, dp) -> PursueCmd{target_bank,
  target_gamma, fire}`:
  - `to_player = player.position - s.position`.
  - **Bank**: horizontal bearing error = angle of `to_player` projected into the
    bandit's local-horizontal plane, measured off the nose. Command
    `target_bank = clamp(k_az * bearing_err, ±max_bank)`, sign turning toward the
    player. (Bank-to-turn — the existing PD holds it.) **P1-3 guard:** if the
    horizontal projection length < eps (player near the bandit's zenith/nadir),
    hold `target_bank=0` (do NOT normalize a ~zero vector → NaN into `sim::step`).
  - **Gamma**: vertical bearing = elevation of `to_player` above the local
    horizon at the bandit. `target_gamma = clamp(k_el * elev, ±max_gamma)` → the
    bandit climbs/dives toward the player's altitude. NOTE (P2): pure-P gamma
    against gravity sits slightly below the full-clamp command — tuning only.
  - **Fire**: true iff the player is within the gun CONE (angle(nose, to_player)
    < `fire_cone`) AND within `[fire_range_min, fire_range_max]`. Range gate
    hysteretic; the CONE gate is deliberately NOT latched (the draining cooldown
    paces cadence — do not "fix" it into a latch). **P1-3 guard:** range < eps
    (coincident) → no fire. Basic = boresight (bandit points at the player nose-on).
  - Throttle: full while engaged (close the gap).
- `drone::tick(d, ap, dp, player, gw_enemy?, difficulty)` — nullable `player`:
  `player==nullptr` ⇒ the OLD patrol path bit-identically (test_drone unmoved).
  When engaged + player present: `pursue` sets bank/gamma/fire; else `program_bank`.

### 3. Enemy guns / return fire (pure, `weapon/` + `combat/`)
- ONE shared enemy projectile pool (not per-bandit): a `weapon::GunWorld`-like
  pool OR a plain `std::vector<weapon::Projectile>`. Per-bandit fire cooldown
  lives in `DroneState` (`double fire_cooldown`), decremented each tick (drains,
  never resets — the iter-9 trigger-tap lesson).
- When a bandit's `pursue.fire` is true and its cooldown ≤ 0: `weapon::spawn`
  ONE round from the bandit toward the player (boresight; a small optional lead
  = `player.vel * tof` later), push into the enemy pool, recharge cooldown by
  `1/bandit_rof_hz`. `spawn` already sets `prev_pos = pos` (kill-loop P0-1).
- **ADVANCE-then-SPAWN each tick (P1-1), structurally the same order as
  `fire_tick`:** first advance every active enemy round under its own per-round
  sphere-coherent gravity + retire (past `kMaxFlightTime` / under the crash
  sphere); THEN spawn this tick's new rounds. So a spawn-tick round sits inert at
  the muzzle (`prev_pos==pos`) and first sweeps NEXT tick — every pool sweeps the
  same `[t,t+dt]` interval, no mismatched-interval hit. Factor the advance-half
  into a shared helper so player and enemy rounds can't fork the integrator.
- Bandit gun geometry (P2): `muzzle_body = (0,0,0)` (CG — a single forward gun is
  enough threat for v1), `convergence_range ≥` the loader's 50 m floor (mid
  fire-range). `weapon::spawn` needs explicit `g`/`fire_dt` (no defaults).
- **Faction model (clean by construction):** two DISJOINT pools with disjoint
  target sets — player pool → drones only, enemy pool → player only. No friendly
  fire, no self-fire. v1: enemy rounds intentionally pass THROUGH other bandits.
- Bandit round config: `bandit_muzzle_speed`, `bandit_drag_k`, `bandit_damage`.

### 4. Player damage (new pass in `combat/kill.h`)
- Extend `combat_tick` (or a sibling `combat_player_tick`) to sweep every active
  ENEMY round against the PLAYER: swept sphere `player.prev→player.curr` vs
  `round.prev→round.curr`, radius `player_hit_radius_m`. On hit: subtract KE
  damage (SAME `dmg = D_ref·min(|v_rel|²/v_ref²,cap)·obliquity` model), retire the
  round, spawn a HitSpark at the player. **Two-pass discipline (P0-2):** all
  enemy rounds test the PRE-death player segment; THEN check death once.
- **Respawn invuln gate (P1-4):** the enemy→player sweep is SKIPPED while
  `cw.player_invuln_ticks > 0` (a counter decremented each tick), so in-flight
  rounds can't re-hit a fresh spawn. Set on death-reset (below).
- Player HP lives in `combat::CombatWorld` (`double player_hp`, init full). NOT in
  the kernel. `combat_tick` only DECREMENTS it; it does NOT respawn the player.

### 5. Player death → respawn (P0-A: PER-TICK inside `app::tick`, app-level)
- **Resolved INSIDE `app::tick`, immediately after `combat_tick`** — NOT after
  `step_frame`. (After-frame would let ticks 2..N of a multi-tick frame fly a
  dead corpse — still firing, frame-rate-dependent respawn instant = the AT-9
  defect. Per-tick placement is exactly what makes the crash branch clean.)
- If `cw->player_hp <= 0`: run the SAME reset block the crash branch uses
  (`spawn_state`, `prev=curr`, `prev_up`, `aim` reseed to nose, `grounded=true`,
  `res.respawned=true` so `step_frame`'s existing respawn-neutralization handles
  the residual ticks for free), THEN restore `player_hp=full`, set
  `player_invuln_ticks=respawn_invuln_ticks`, DEACTIVATE the enemy pool (one
  deterministic loop — belt-and-suspenders with the invuln gate), bump `deaths`.
  Firewall intact: identical app-level mechanism as a crash, nothing into the kernel.
- HUD: a player HP bar + a "DOWNED — RESPAWNING" flash (clock-free, tick-driven,
  like the kill flash).

### 6. Difficulty (config, `[combat]`)
- `difficulty` is an **int level 1..5** (P2: type PICKED — not a float) → a pure
  `difficulty_params(int) -> {max_engaged, bandit_rof_hz, bandit_damage}` mapping
  (e.g. 1 → 1 attacker / slow / weak … 5 → several / fast / hard). Deterministic,
  no config overrides in v1 (difficulty IS the single scaling scalar). Any future
  aim-scatter derives from `age_ticks`+index, NEVER rng. Loader-validated
  (`difficulty ∈ [1,5]`; the mapping guarantees `max_engaged ≥ 0`, rof > 0);
  `player_hit_radius_m > 0` also loader-checked (P2).

## New `DroneState` fields (P1-2)
`engaged` (bool), `engage_dwell` (double, hysteresis), `fire_cooldown` (double).
**`respawn_in_place` MUST also zero all three** (a killed engaged bandit else
reborn kilometres away still `engaged` with a hot cooldown → beelines/fires from
spawn). One line added to the existing `respawn_in_place` field resets.

## Per-tick ORDER inside `app::tick` (mirrors the kill-loop two-pass)
1. advance player (existing).
2. `assign_engagements(drones, st.curr /*post-step*/, max_engaged)` — per-tick,
   hysteresis-holders keep slots then nearest fill, tie→lower index (P1-5).
3. advance each drone via `drone::tick(..., player=&st.curr, ...)` — AI steers,
   may set a pending fire.
4. `weapon::fire_tick` (player battery) — existing.
5. enemy pool: ADVANCE active rounds + retire, THEN spawn this tick's new rounds
   for firing drones (P1-1 advance-then-spawn; prev_pos inside `spawn`/`advance`).
6. `combat_tick`: pass 1 player-rounds→drones (existing) AND enemy-rounds→player
   (new, SKIPPED while `player_invuln_ticks > 0`), pass 2 kill drones; enemy-round
   damage accumulates vs the pre-death player. Decrement `player_invuln_ticks`.
7. **STILL INSIDE `app::tick`:** if `cw->player_hp <= 0` → the §5 reset block
   (crash-branch reset + restore HP + set invuln + deactivate enemy pool + bump
   deaths). `res.respawned=true` neutralizes the rest of the frame's ticks (P0-A).

## Firewall / determinism checklist (Fable, please red-team)
- Nothing in `drone/`,`weapon/`,`combat/` includes `sim/step`-writers or
  `control/`; the player is READ-ONLY input.
- `player==nullptr` / `difficulty` off ⇒ patrol path bit-identical (goldens).
- No clock, no rng anywhere; cadence = counters, geometry = pure funcs, any
  aim-scatter derived deterministically.
- prev_pos maintained inside weapon spawn/advance (no caller snapshot — the
  free-list phantom-segment trap, kill-loop P0-1).
- Two-pass so a same-tick burst can't wound a just-respawned player (P0-2).
- Every gate/latch hysteretic (engage range, fire range).
- Player death path is app-level reuse of the crash reset — confirm NO damage or
  HP value feeds back into `sim::step`/`control::step`.

## Open scope calls (v1 keeps these simple; note for Chad's fly)
- Boresight fire vs a small lead (basic = boresight; lead is a one-line add).
- Aim scatter / accuracy vs difficulty (v1 = perfect boresight; scatter later).
- Bearing-error is ±π discontinuous directly astern → deterministic bank-sign
  flip when the player is exactly behind; harmless, add a small deadband only if
  it visibly twitches (P2).
- (Resolved by the pre-consult, no longer open: single forward gun @ CG;
  spawn-camp handled by enemy-pool deactivate + `respawn_invuln_ticks`.)
