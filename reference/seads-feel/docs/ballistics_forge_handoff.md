# Ballistics-forge — handoff (RESUME HERE)

The **gun-ballistics realism** thread: give the Bf 109 battery predictable, realistic,
*satisfying* gunnery via the `/ballistics-forge` autoresearch loop (research WWII ballistics →
Fable pre-consult → Sonnet implement → gate → score → Fable red-team). Built 2026-07-12 via
`/agent-builder`. **World thread — NEVER touch `sim/` or `control/` (frozen kernel).** The kill-loop
(drone HP + hit detection + FX) landed iter 8 in the pure `combat/kill.h` module (see DONE below).

## WHERE THE WORK LIVES  ⚠ read this first
- **Worktree:** `D:\seads_sandboxes\ballistics-forge`  · **branch:** `sandbox/ballistics-forge`
  (off `sandbox/world-sudbury` HEAD — the ONLY branch with the guns leg; `main` and
  `sandbox/rigd-bf109` do NOT have it — `rigd-bf109` is the Bf 109 *mesh* rig, a different leg).
- **COMMITTED 2026-07-13** as `54cf6502d` ("ballistics: gun/combat leg …", iters 0–10, 32 files) +
  `c8a28968a` (iter-10 ledger row). **NOT PUSHED** — Chad chose "commit, don't push yet". The branch is
  152+ ahead of `main`, but `main` DIVERGED 15 commits, so promoting to `main` is a real MERGE (not a
  fast-forward) carrying 116 mostly-AWAITING-FLY world commits. Defer push / main-merge until Chad rules
  the scope. Rule stands: **commit/push only when he asks.**
- **⚠ UNCOMMITTED (SESSIONS 2 + 3) — a large pile awaiting Chad's fly then a commit.** SESSION 2
  (2026-07-13b): gun/explosion AUDIO, cannon-vs-MG timbre split, bandit speed 95, pipper parallax fix.
  SESSION 3 (2026-07-13c): the **BANDIT COMBAT AI** (§S2.4). Stage by EXPLICIT path when Chad asks:
  - NEW (S2): `render/gun_audio.h`, `render/gun_synth.h`, `test/unit/test_gun_audio.cpp`, `docs/bandit_combat_plan.md`
  - EDITED (S2): `app/main.cpp`, `render/draw.h`, `render/draw.cpp`, `config/scenario.toml`, `CMakeLists.txt`,
    `test/unit/test_load_scenario.cpp`, `docs/audio_handoff.md`, `docs/ballistics_forge_log.md`
  - EDITED (S3 bandit AI): `drone/drone.h`, `combat/kill.h`, `app/instructor_tick.h`, `app/main.cpp`,
    `config/load_scenario.{h,cpp}`, `config/scenario.toml`, `render/draw.{h,cpp}`,
    `test/unit/test_drone.cpp`, `test/unit/test_combat.cpp`, `test/unit/test_load_scenario.cpp`
  - BENCH ARTIFACTS — do NOT commit: `tools/gun_audio_preview.cpp`, `gun_preview.exe`, `gun_preview.wav`
  - NEVER commit the 4 build-cruft files (`render/sky.h`, `generated/gen_log.jsonl`, `offline_tool/sudbury_*.py`).
  **All firewall-clean, WORLD-thread.** Gate **439/439**.
- **BUILD GOTCHA:** `world-sudbury` HEAD does not build from a fresh checkout — `render/sky.cpp`
  uses `SkyRenderer::moon_tex` but the member lives only in Chad's *uncommitted* `render/sky.h` in
  the main worktree. This worktree already has that WIP synced (build-only cruft, STILL UNCOMMITTED after
  `54cf6502d`: `render/sky.h`, `generated/gen_log.jsonl`, `offline_tool/sudbury_config.py`,
  `offline_tool/sudbury_ribbon.py`). **Never `git add` that cruft** — stage only ballistics files by
  EXPLICIT path when Chad asks to commit.
- **Commands** (from the worktree root, git-bash): `cmake --build build --config Debug` /
  `ctest --test-dir build -C Debug --output-on-failure`. Current gate: **452/452 green** (was 421;
  Session 3 bandit-AI + maneuverability retune; Session 4 bandit speed 85 + COMPONENT DAMAGE MODEL).
- **SESSION 4 (2026-07-14):** bandit speed 95→85 (catchable + tighter turns), and a **component damage
  model** (`docs/damage_model_plan.md`, Fable pre-consult + impl red-team): engine/pilot/wing-break reshape
  the flight via a PURE transform on the frozen kernel's params (`combat/damage.h`) — identity at zero
  damage. Wired in `app/instructor_tick.h` + `combat/kill.h` + HUD panel. `test_damage.cpp` (18). NOT
  config-driven yet ([damage] toml = follow-up); ⓯ pending Chad's fly (hurt-plane feel).

## READ THESE (the thread's memory)
- `docs/ballistics_forge_log.md` — the **ledger**, one row per iteration; `E` is the spine.
- `docs/ballistics_research_notes.md` — research findings + **both Fable verdicts** + the impl spec.
- `docs/ballistics_reference.{json,md}` — the ground-truth tables (the metric's target).
- `.claude/skills/ballistics-forge/SKILL.md` — the loop. Resume by invoking **`/ballistics-forge`**.

## SESSION 2 (2026-07-13b) — UNCOMMITTED, gated 421/421, awaiting Chad's fly then a commit

Chad's arc this session: "iter-10 is good" → "add gun + explosion SOUND, then bandits that shoot back" →
after flying the sound: "guns good but I only hear ONE sound for two gun types; bandits still too fast;
lead indicator looks wrong (floating above the plane)." Everything below is the response. **Nothing here
is committed.** The FIRST fly-priorities for the next agent are the ⓯ items.

### S2.1 — COMBAT AUDIO (gun-fire + explosion/hit) — LANDED, Fable red-teamed, gate 421/421
Two EVENT-driven procedural layers (not continuous like wind/engine), following the `wind_synth.h` bench.
- `render/gun_audio.h` — pure constants + envelope helpers (`gun_shot_amp`/`explosion_amp`/`hit_amp`), unit-tested.
- `render/gun_synth.h` — `GunSynth` (machine-gun stutter: two shot-schedule phase accumulators at the
  composite battery rates 3×cannon 12 Hz / 2×MG 19 Hz; each shot a band-passed noise crack in a 24-voice
  ring; **cannon voices add a decaying sub-bass sine "thump"** for weight) + `CombatSfxSynth` (polyphonic
  explosion + hit one-shot voice pool).
- Glue `app/main.cpp`: `gun_stream` driven by `fire_held && !raw_mode`; `sfx_stream` edge-detects the
  monotone `cw.kills`/`cw.hits` (≤8 voices/frame). Firewall-clean (read-only, fixed-seed PRNG, no clock),
  skipped in smoke, unloaded on exit. Volumes gun 0.55 / sfx 0.85 (code trims).
- **Cannon vs MG TIMBRE SPLIT (Chad "only one sound for two types"):** cannon deep/broad/long
  (135 Hz, Q0.7, 32 ms) + 78 Hz sub-thump; MG high/tight/short (780 Hz, Q1.7, 6 ms). Burst spectrum is now
  bimodal (48% sub / 14%+25% MG bands / 3% old-overlap midrange). Sub-mix retuned 0.85→0.30 (was drowning MG).
- **Fable red-team: SOUND-WITH-FIXES** — folded P1 (phase-jitter clamp killed the "late" half → metronomic
  brrrt; clamp removed) + added a silence-after-fire regression test. Deferred P2s (for Chad's ear): tanh
  soft-clip on stacked booms, per-voice tau wiring, precomputed decay coeff, first-crack phase reset.
- **AUDITION:** `gun_preview.wav` (worktree root) — offline render of the exact synths (regen:
  `g++ -O2 -std=c++17 -I. tools/gun_audio_preview.cpp -o gun_preview.exe && ./gun_preview.exe`).
- ⓯ PENDING Chad's ear: brrrt punch, cannon/MG distinctness, boom/tink read, mix vs wind/engine.

### S2.2 — BANDIT SPEED 115→95 m/s (Chad "still cant catch em")
`config/scenario.toml [drone] speed`. ~72 m/s cruise closure. Loader floor 1.5·V_stall ≈ 68 clears. Two
`test_load_scenario` value-pins re-derived 115→95. ⓯ PENDING Chad's fly (catchable now?).

### S2.3 — PIPPER PARALLAX DISPLAY FIX (Chad "lead indicator looks wrong, floating above the plane")
**Fable math diagnosis first (Chad: "figure it out with math, not testing"): the lead SOLVER is CORRECT** —
a round fired down `lead_dir` hits a 95 m/s crosser to 0.47 m @300 m (8 m sphere); NO pipper↔round fork.
The "above" was 100% CHASE-CAM PARALLAX: the diamond was drawn with `project_dir` (a direction at INFINITY)
while the bandit is an eye-projected finite point, and the eye sits ~9 m above the gun line → the diamond
floated 23–39 mrad high, swinging with bank.
- **Fix (render-only, `render/draw.{h,cpp}` + `app/main.cpp`):** draw the diamond at the WORLD IMPACT
  POINT `B = engaged-bandit-interpolated.pos + vel·ttl`, projected FROM THE EYE (new `DrawInfo::gunsight_lead_point`).
  Now it sits AHEAD of the bandit by the true lead (never on him — Chad keeps sight of the bank), parallax
  baked in; tracers flow into it; RED "fire-now" cue unchanged. Gate blind to the exe, so Fable's hit-proof
  is the verification. ⓯ PENDING Chad's eye: does the lead marker now read right (esp. banked / crossing)?
- **DEFERRED — sub-metre lead-SOLVER accuracy folds (Fable-specified, NOT yet done):** these touch
  `render/gunsight.cpp` `lead_solution` and would move exact-value pinned tests, so they need a careful fold
  WITH test re-derivation (not a rushed edit). Priority order + one-line fixes are in the ledger's deferred
  row and Fable's derivation: **P2-4** cant along BODY-up not world-up (bank-dependent, up to 2.5 m @450/60° —
  biggest, matters for the deflection shots that ARE banked); **P2-1** cant charged on the relative dragged
  distance `log1p(k·m·t)/k` not the world-lead distance (~0.3 m); **P2-2** solve `p` from the +2.87 m muzzle
  not the CG (~0.27 m); **P2-3** droop half-step for coherence w/ the cant Δ (~0.02 m, optional).

### S2.4 — BANDIT COMBAT AI — ⚡ IMPLEMENTED (Session 3, 2026-07-13c), UNCOMMITTED, gate 439/439
Chad: "start with the bandits being able to shoot me and seeking me out in merges. Make them good but not
that good (tuneable)." Built per **`docs/bandit_combat_plan.md`** (the Fable-vetted design), all fixes
folded. World-thread only; player READ-ONLY; no clock, no rng. **⓯ PENDING Chad's FLY** — the numbers are
headless-tuned; the feel (are they good-but-beatable? do they seek convincingly in a merge? is the return
fire fair at difficulty 3?) is his call. All knobs are live in `scenario.toml [combat]` (no recompile).

**What landed:**
- **`drone/drone.h`** — `DroneState` gains `engaged`/`fire_cooldown`/`wants_fire` (reset in
  `respawn_in_place`+`spawn_drone`, P1-2). `autopilot` generalized to hold a commanded `target_gamma`
  (P1-6 exact `sin_gamma - std::sin(target_gamma)`, defaults 0 = bit-identical level hold — the firewall).
  New pure `pursue(s, player, dp) → PursueCmd{target_bank,target_gamma,fire}` (bank-to-turn on horizontal
  bearing via `atan2` in the local-horizontal plane, climb/dive on elevation, boresight fire in cone+range;
  P1-3 coincident/zenith guards). New pure `assign_engagements(drones, player, max_engaged, dp)` — nearest-N
  attackers, HYSTERETIC via the engage/disengage range band (holders keep slots first, tie→lower index).
  `tick` takes a nullable `player`; nullptr OR not-engaged ⇒ patrol bit-identical.
- **`combat/kill.h`** — `difficulty_params(int 1..5) → {max_engaged, bandit_rof_hz, bandit_damage}` (the
  master threat scalar). `CombatSetup` config struct. `CombatWorld` gains the DISJOINT `enemy_pool` +
  `player_hp`/`deaths`/`ticks_since_death`/`player_invuln_ticks`. Extracted shared `ke_damage` +
  `spark_reflection` helpers (combat_tick refactored onto them, BIT-IDENTICAL — the KE-value tests still
  pass). New `enemy_fire_tick` (advance-then-spawn P1-1; cooldown drains-never-resets, iter-9 lesson) and
  `combat_player_tick` (swept enemy-round vs player segment, KE damage, invuln gate P1-4, two-pass P0-2).
- **`app/instructor_tick.h`** — per-tick order (P1-5): `assign_engagements` on the post-step player →
  drones pursue → player `fire_tick` → `enemy_fire_tick` → `combat_tick`(player→drones) +
  `combat_player_tick`(enemy→player) + `fx_tick` → **player death→respawn resolved PER-TICK (P0-A)** reusing
  the crash reset (`res.respawned` neutralizes the frame's residual ticks) + restore HP + arm invuln +
  deactivate enemy pool + bump deaths. Bandit hunting gates on `cw != nullptr` so drone-only paths stay
  bit-identical (pinned by the existing firewall tests).

## SESSION 5 (2026-07-14) — DAMAGE MODEL CONFIG-DRIVEN — UNCOMMITTED, gate 456/456
The Session-4 follow-up (that note's "NOT config-driven yet — [damage] toml = follow-up"). The 17
`combat::DamageParams` feel/floor dials moved from hardcoded struct defaults to a **`[damage]`
scenario.toml** block, so Chad can tune the hurt-plane feel with NO recompile (unblocks the S4 ⓯
"hurt-plane feel" fly item). Files: `config/scenario.toml` (+`[damage]`), `config/load_scenario.{h,cpp}`
(parse + strict validation — divisors >0, the four controllability floors in (0,1], `cd0_cap_mult` ≥1,
severity losses in [0,1], routing extents bounded), `app/main.cpp` (`cw.damage_params = scen.damage`),
`test/unit/test_load_scenario.cpp` (+4: defaults-mirror-struct pin, `component_hp`=0 / floor=0 /
`cd0_cap_mult`<1 rejections). The committed toml values MIRROR the struct defaults EXACTLY ⇒ the config
path is bit-identical to before, and `damage_zero` still short-circuits the whole transform at zero damage
— no golden moved. App boots + parses `[damage]` (smoke ok). **NOT committed** (commit only when Chad asks;
stage by explicit path — this joins the existing uncommitted pile, NEVER the 4 build-cruft / 3 bench files).

## SESSION 6 (2026-07-14) — AUDIO SOFT-CLIP (Fable P2 fold) — UNCOMMITTED, gate 460/460
The Session-2 deferred Fable P2 "tanh soft-clip on stacked booms". Both synth output buses (`GunSynth`
~line 181, `CombatSfxSynth` ~line 306) HARD-clipped (`if(s>1)s=1`) — a furball (many explosion + hit
voices at once, now common with the Session-3 bandit combat AI) overshoots ±1 and the hard corner distorts
harshly. Replaced with a shared `render::soft_clip` (`render/gun_audio.h`): **identity for
`|s| ≤ kSoftClipThreshold = 0.9`** (single-sound timbre UNTOUCHED — below-knee samples bit-identical, so
every existing audio golden holds), then a **C¹ tanh knee** saturating smoothly toward ±1 above it (value
`= t` and slope `= 1` at the threshold ⇒ no audible kink; an extreme overshoot still saturates to the
ceiling, but gradually, never a corner). Files: `render/gun_audio.h` (constant + `soft_clip`),
`render/gun_synth.h` (both clip sites), `test/unit/test_gun_audio.cpp` (+4 pure/synth pins). Render-only,
PURE, fixed-seed PRNG (firewall intact). Gate **460/460**. Knee width tunable via `kSoftClipThreshold`
(a code constant, per the audio module's "fly-dials are code constants" convention — Chad's ear can widen
it). **NOT committed** — joins the uncommitted pile; NEVER the 4 build-cruft / 3 bench files.

## SESSION 4 detail (bandit combat AI [combat] block, for reference)
- **`config/`** — `scenario.toml [combat]` block (difficulty + player_hp/hit_radius/invuln + bandit gun +
  engage/disengage + pursuit gains/max_bank(≤45° docile cap)/max_gamma + fire cone/range). Loaded +
  strictly validated in `load_scenario.{h,cpp}`; threaded into `cw.setup` in `main.cpp`.
- **`render/`** — enemy tracers (venomous yellow-green tint, same comet pass, one code path via a
  `draw_pool` lambda), player **HP bar** (green→amber→red) + DEATHS + centered "DOWNED — RESPAWNING" flash;
  `DrawInfo` + `main.cpp` wiring. Confirmed live: a hands-off smoke has the player HP drop into the 60s from
  bandit fire (faction-disjoint pools ⇒ genuine return fire), HUD renders, no crash.
- **Tests (+18 cases, gate 400/421→439):** `test_drone` (target_gamma bit-identity, pursue bank/gamma/fire
  signs + NaN guard, engaged-turns-to-face, assign nearest-N + hysteresis); `test_combat` (difficulty
  clamp/monotone, enemy_fire spawn + rof-cadence, player damage, invuln, end-to-end death→respawn);
  `test_load_scenario` ([combat] loads, difficulty range, disengage>engage, bank-cap).

**MANEUVERABILITY + GUNNERY TUNING PASS (same session, after Chad flew "they dont seek me, shoot me,
they just fly away forever"):** a headless diagnostic (player chasing the committed fleet, logging
engage/bank/nose-on/hits per second) root-caused it: the bandits DID engage + bank, but (1) the patrol
roll gain (kBankP=0.15) took ~9 s to even reach 45° bank, and (2) a 45°/95 m/s LEVEL turn is only ~6°/s —
far too slow to bring guns onto a player, so they flew lazy circles away and never fired. Fixes, all
diagnostic-verified (bandit now reverses from nose-on −0.98 → +0.99 and fires bursts; live smoke: player
HP drops to ~60):
- **Snappy combat autopilot** — `autopilot` gains its own `bank_p`/`level_p` overrides (default to the soft
  patrol constants ⇒ patrol bit-identical); a pursuing bandit passes strong combat gains (0.45 / 2.2) so it
  reaches bank in ~2 s without over-gained hunting.
- **Steeper bank + hard pull** — `pursue_max_bank` 45°→55° (stall-safe at 95 m/s, full power; loader cap
  raised to 55°). A `pursue_pull` term adds nose-up when hard-banked FAR off the nose (tightens the reversal)
  with a TRACKING DEADBAND (zero pull inside ~30° so it settles the guns instead of ballooning past).
- **Deflection LEAD** (the plan's deferred one-liner) — aims where the player will be, leading by the
  player's velocity RELATIVE to the bandit (the same model the player's pipper uses; leading by absolute
  velocity mis-aims for a fast shooter). `pursue_lead_speed` (~= bandit muzzle speed).
- **Stability fire-gate** — hold fire while HARD cross-controlling (a fired round inherits body velocity and
  would spray sideways); `fire_align_cos` (~35°). Shoot through a normal tracking pass.
- **Burst cadence** — `difficulty_params` rof was 0.8–3.5 Hz (too slow to ever connect); now 3–11 Hz BURST
  with lower per-hit damage (4–14) so a tracking pass sprays and lands a hit or two. Wider engagement
  (engage 2500→3200, disengage 3500→4200; more bandits hunt).
- **Behavior trade (documented, Chad's fly-call):** the aggressive pull CLIMBS into hard turns and bleeds
  energy (a bandit that over-turns ends up slow — beatable). Reliable gun solutions still need the player to
  present a merge (a straight-fleeing player at higher speed is the hardest case; hits are sparse there).

**⓯ Tuning dials for Chad's fly** (all `scenario.toml [combat]`): `difficulty` 1..5 is the master knob
(attackers/cadence/damage). Aggression: `pursue_bank_gain`/`pursue_max_bank_deg`/`pursue_pull_deg`. Gunnery:
`pursue_lead_speed` (lower = worse shots), `fire_cone_deg`, `fire_range_max`. If they still feel too passive,
raise the gains/pull; if too deadly, drop `difficulty` or `pursue_lead_speed`. DEFERRED (richer ACM, next
iteration): energy management (don't bleed to stall), plane-correct pull (level turns not climbing turns),
proper full lead solution, aim scatter, multiple guns.

### S2.4-OLD — the original design note (superseded by the above; kept for the P0 rationale)
Chad-ruled scope: **full stakes**, **basic pursuit**, **difficulty-scaled**. The folded Fable pre-consult
caught a real **P0** — player death must resolve PER-TICK inside `app::tick`, not after `step_frame`, or it
re-ships the AT-9 frame-rate defect. Design in **`docs/bandit_combat_plan.md`**.

## DONE (committed `54cf6502d`, gated; latest first)

### iteration 10 — KE DAMAGE + WHITE/RED PIPPER + ALL-BANDIT RANGE + CATCHABLE BANDITS — Fable-vetted, gated 400/400
**Trigger:** Chad flew and asked for "full ballistics" polish, then (after re-flying) "bandits too fast to
catch — put distance on ALL of them." Two Fable-5 consults drove the hard math (returned in-conversation;
NOT the older firing-mechanism packet in `ballistics_fable_consult.md`).
- **(A) KE damage model (`combat/kill.h`, `weapon/ballistics.*`):** flat `hp-=damage` → real ballistics
  `dmg = D_ref·min(|v_rel|²/v_ref², 2)·clamp(cosθ, 0.3, 1)` — range falloff (drag-slowed `p.vel`),
  obliquity (incidence vs impact→center at the SAME `best_s`), overspeed-merge cap 2×. `Projectile.damage`
  REINTERPRETED as REFERENCE damage (at `|v_rel|==v_ref`, normal hit); new `Projectile.v_ref` = muzzle
  speed, set in `weapon::spawn`. Config `cannon_damage=30`/`mg_damage=6` KEPT (now reference dmg; ~4
  point-blank cannon hits kill, more at range). Constants `kMinImpactSpeed2/kObliquityFloor/kOverspeedCap2`
  + `kFxRefDamage` in kill.h.
- **(B) Lead diamond WHITE→RED (`render/draw.cpp`):** replaced the green/amber/red 3-state — the pipper is
  WHITE off-solution and snaps RED on `gunsight_on_target` (valid+in-range+in-cone ⇒ "fire now");
  out_of_envelope folds into white (on_target can't be true out of envelope).
- **(C) Per-bandit range (`render/draw.cpp`):** slant range under EVERY in-front bandit (green fine print,
  auto m→km), and it NEVER fades — the old BANDIT tag faded to invisible past 3 km, so a chased bandit
  vanished from the HUD (the "couldn't find them" gap). BANDIT tag fade extended 1500/3000 → 4000/9000 m.
  The engaged-only readout was removed (uniform now); `DrawInfo::gunsight_target_index` left populated but
  unused. Chad: all-bandit is right FOR NOW (proximity-gating is a future nicety).
- **(D) Hit FX (new pure `combat/fx_curves.h`, Fable):** `spark_particle`/`fireball_state`/`debris_particle`
  — closed-form in `(seed,i,age,energy01)`, golden-angle/Fibonacci lattice, drag-consistent (λ=0.10 ==
  `fx_tick`), exact burnout to 0. `Fx` gained `dir` (reflected shot) + `energy01` (= dealt-dmg/30 spark
  intensity); `fx_spawn` params defaulted. `draw.cpp` FX loop rewritten (≤16 sparks + fireball + 12 debris).
- **(E) Catchable bandits:** bandit cruise **140→115 m/s** (`scenario.toml speed` + `drone.h` default) so the
  player (cruise ~167 / redline 245) has ~50 m/s cruise closure. Loader floor `speed≥1.5·V_stall(~68)` clears.
- **Tests:** helper sets `v_ref` (center hit still deals exactly `damage`); +4 KE-physics cases
  (falloff/obliquity/cap/NaN-guard), +4 fx-curve cases (determinism/bounds/burnout/gravity-sag);
  `test_load_scenario` re-pinned 140→115. **Gate 400/400.**
- **FLY-CONFIRMED (Chad, 2026-07-13b): "yes that's good."** The KE damage, white→red pipper, all-bandit
  range, and hit FX all pass his eye. Residual feel items folded into SESSION 2 below (bandits still too
  fast → 95; gun sound → two-timbre split; pipper looked "above" → parallax display fix).

### iteration 9 — HONEST PIPPER + FEEDBACK + TRUE-SCALE BANDITS — red-team-driven, gated 395/395
**Trigger:** Chad flew the kill-loop and hit a wall — a dead-astern target past ~950 m took NO hits
under sustained fire (a close side shot killed fine). The iter-8 Fable red-team root-caused it (with a
standalone harness against the real code): NOT a combat bug — a **system honesty gap**. Model A's
pure-lead pipper compensated NO droop; the cant zeroes droop only at 500 m; so dead-astern rounds sank
18 m low at 1000 m (past the 15 m sphere) = permanent miss, INVISIBLE because tracers died at 1.5 s
(before impact) and drones drew 4× oversize (1200 m looked like 300 m). Chad ruled **"both"** (honest
pipper + feedback) plus **true-scale bandits with labels + a live range/closure readout**.
- **(A) Ballistic-honest, cant-aware pipper (`render/gunsight.cpp`):** the droop term is RESTORED in the
  world-frame solve (shared `dragged_droop_dist`) but the gun cant's known rise is SUBTRACTED
  (`≈ range·Δ/conv`, `Δ = weapon::harmonization_rise(hub_muzzle,…)` — single-sourced, not re-derived), so
  there is NO double-comp: at 500 m it reduces to iter-7 behavior (cant covers droop → pipper≈boresight),
  beyond 500 m it auto-holds-over the residual. Uses the PRIMARY (hub cannon) cant as the one pipper
  reference (Chad: "doesn't have to be perfect"). `lead_solution` gained optional
  `conv`/`hub_muzzle`/`hit_radius` params (default 0 → old pure-lead, so untouched callers still compile).
  The FIXED cyan boresight reticle is unchanged. **Pin (test #25):** nose-on-pipper, REAL `weapon::spawn`
  round, dead-astern receding target → HIT at 300/500/800/1000 m (miss 0.28/0.47/0.75/**0.91 m**).
- **(B) Feedback:** `tracer_lifetime_s` 1.5→**3.5 s** (tracers reach the target, fall-of-shot visible);
  HitSpark now angular-sized (reads at 1 km); **3-state pipper colour** — GREEN on-target, AMBER
  valid-but-off-aim, RED out-of-envelope (`!sol.valid || |predicted_miss| > hit_radius`).
- **(C) True scale:** `[drone] size` 4→**1.0** (real dims — kills the range illusion); `hit_radius_m`
  15→**9 m** (true half-span ~5 m + arcade margin). `combat::CombatParams` default matched.
- **(D) BANDIT labels:** billboarded "BANDIT" text above each drone (`render/draw.cpp`, projected like the
  reticle), fading 1500→3000 m — restores identifiability now that bandits are small.
- **(E) Range/closure readout:** the TOT HUD line now shows live `RNG` + `CLO ±m/s` (closing/opening),
  from a new `OnTargetMeter::closure_rate_now` (relative velocity along LOS — read-only, clock-free).
- **(F) Clean bugs fixed:** explosion debris no longer double-advances velocity (`draw.cpp` base=`f.pos`,
  `fx_tick` already integrates); **trigger-tap exploit closed** (`weapon/ballistics.cpp:131` cooldown now
  DRAINS on release, not resets → cyclic RoF enforced regardless of tapping — pinned test #22); the
  promised-but-missing P0-2 spawn-safety assertion added to combat test 6.
- **Gate 395/395 (author-verified)**; single-source `expm1`/`log1p` still ×1 each; firewall/determinism
  intact (no player/kernel writes, no clock/rng added). Renders clean (`iter9_fwd.png`).
- **NOT fly-checked (all fly-tunable):** BANDIT-label clutter/readability, true-scale visibility, 3.5 s
  tracer feel, 9 m hit-radius forgiveness, the RED-cue threshold (currently `|miss|>hit_radius` — may want
  a wider band), and the `CLO` sign convention.

### iteration 8 — THE KILL-LOOP (guns → battles) — Fable-vetted design, gated 394/394
Turns "guns that fire" into actual combat: drone HP + swept projectile-vs-drone hit detection +
damage + destroy→respawn + kill/hit FX. Chad ruled **medium tankiness** (aimed-burst kill),
**kill FX in this pass**, **Fable pre-consult first**. The pre-consult caught 4 design-P0s pre-build.
- **New pure module `combat/kill.h`** — the ONE sanctioned cross-writer: writes `Projectile.active`,
  drone `hp` (via the drone's own respawn helper), and its OWN FX pool — NEVER the player/kernel.
  `combat_tick(std::vector<weapon::Projectile>&, std::vector<drone::DroneState>&, CombatWorld&, ap,
  DroneParams&, dt)` runs in `step_frame` right after `weapon::fire_tick` (`app/instructor_tick.h`),
  gated `if (cw && dw && gw)`; **`cw==nullptr` ⇒ bit-identical to pre-kill-loop** (firewall test relies
  on this). Takes `vector<DroneState>&` not `app::DroneWorld` (no layering inversion).
- **P0-1 (fixed):** `weapon::Projectile.prev_pos` maintained INSIDE weapon (`spawn` sets it =pos,
  `advance` sets it =pos before integrating) — NOT a caller snapshot (fire_tick reuses free slots →
  a snapshot would phantom-instakill). `damage` added to `GunSpec`+`Projectile`, copied at spawn like
  `drag_k` (no Round→damage table).
- **P0-2 (fixed):** two-pass resolve — Pass 1 accumulates damage / retires rounds vs PRE-kill segments;
  Pass 2 respawns any `hp<=0` drone + spawns its explosion. Trailing burst rounds feed the fireball; a
  drone respawned this tick is untouchable until next tick.
- **P0-3 (fixed):** `hit_radius_m=15` tracks the RENDERED scale (drone drawn `size=4`×~9.925 m span),
  NOT physical span — else tracers pass through the visible wings without registering. Config `[drone]`.
- **P0-4 (fixed):** single `drone::respawn_in_place` helper, `hp` resets on BOTH kill and ground-crash.
- **Swept collision:** segment-vs-moving-sphere in the RELATIVE frame (`r0=prev_pos−d.prev.pos`,
  `dr=Δproj−Δdrone`, closest-approach `s`, hit ⇔ `|r0+s·dr|²≤R²`). One round → smallest-`s` victim,
  tie → lower index. No tunneling at ~6.7–9 m/tick.
- **FX:** age-driven, clock-free, NO rng (`FxPool` free-list; debris on a Fibonacci lattice keyed by a
  monotone `spawn_counter`). Copies pos/vel AT SPAWN (P3c — never references the drone, so a respawn
  can't streak it). Spark ~0.2 s / ~2 m; explosion ~1.5 s / ~12 m fireball + 8 debris. Render draws the
  pool additively via `info.combat_fx` (mirrors the tracer pass). HUD: `kills` counter + 2 s red flash
  driven by `ticks_since_kill` (combat's own counter — no player clock).
- **Numbers (fly-tunable config):** `[drone] hp=100, hit_radius_m=15`; `[guns] cannon_damage=30`
  (→ **4 cannon hits kill**), `mg_damage=6`. Loader checks `hp>0`, `radius>0`, `cannon>mg>0`.
- **Gate 394/394 (author-verified: 383 + 11 combat pin tests)** — incl. **firewall memcmp** (player
  bit-identical), **determinism**, swept no-tunnel (both endpoints outside R), earliest-victim/tie,
  4-hits-kill+respawn, same-tick overkill, crash-respawn-resets-hp, spawn-tick-no-phantom. Renders clean
  (`kill_fx_fwd.png` — app runs with combat wired; no kill in a hands-off shot).
- **NOT fly-checked:** the explosion/spark LOOK (size/colour/debris — code constants in `render/draw.cpp`
  ~1167-1220 + `combat/kill.h` `kSparkLifetime`/`kExplosionLifetime`) needs Chad to down a drone. Also
  DEFERRED by design: a respawn DELAY (killed drone currently respawns instantly at its far scatter slot).

### iteration 7 — MODEL A FIXED-SIGHT (P0 fix) + Chad's taste calls — red-team-driven, gated 383/383
**Trigger:** the iter-6 Fable red-team found a **P0** (pipper double-compensated droop against the new
gun cant → nose-on-pipper landed ~3.2 mrad HIGH, ~1 m@300 m / 1.6 m@500 m; the cross-check was blind
because it bypassed `weapon::spawn`). Chad flew, confirmed, and ruled **MODEL A** (authentic Bf 109
Revi fixed-sight): the gun cant is the SOLE gravity-droop compensator; the lead pipper becomes a
PURE-LEAD/deflection indicator; the fixed reticle is sourced from the gun boresight.
- **P0 fixed (`render/gunsight.cpp`):** droop term removed from `lead_solution` — intercept is now
  `I = p + v_tgt·t` (was `− dragged_droop(t)`), `(void)grav` at :92. World-frame velocity-inheritance
  solve + FIX-2 lag KEPT (those were correct, not the bug). `dragged_droop_dist` stays live via
  `weapon::harmonization_rise`, so the single-source helper isn't orphaned.
- **Fixed boresight reticle (`render/draw.cpp`, `draw.h`, `app/main.cpp`):** new cyan cross-pip
  (`{80,210,240}`) drawn along `orient·(0,0,−1)` (single-sourced with the convergence sightline, via
  `info.gunsight_boresight`) — rounds pass THROUGH it at conv range. The §9.2 white control-reticle /
  nose-marker pair is untouched (that's flight-control feedback, not the gunsight). Lead diamond stays
  as the now-pure-lead deflection aid.
- **Cross-check re-pointed (moved golden, authorized):** `pipper_miss` now fires via `weapon::spawn`
  (the real canted battery) instead of `r.vel = shooter_vel + muzzle·lead_dir` — so it can never again
  be blind to the cant. Tight ≤1 m at the 500 m convergence range; ≤2.5 m at 300 m (sub-convergence
  over-comp is now by-design model-A physics). `test_gunsight` "gravity raises aim" now asserts y≈0.
- **Taste calls:** `convergence_range_m` 300→**500**; `cannon_drag_k_per_m` 0.001565→**0.00080**
  (arcade: ~46%→~67% v@500 m; cant Δ auto-recomputes — bands now cannon 1.5–3.5 mrad, MG 0.8–2.8 mrad).
  Tracers → **orange/red** comet (cannon `(1.0,0.30,0.02)`, MG `(1.0,0.55,0.04)`) with deterministic
  **sin² "slag"** dark modulation (no wall-clock); **15 tracer look-dials lifted to `[guns]` config**
  (no bare tuning numbers left in `render/` for tracers). Evidence: `tracer_slag_fwd.png` / `_side.png`.
- **Red-team P1/P2 all cleaned:** rolled-attitude frame oracle (catches body-vs-world Δ bugs);
  conv-TOF envelope guard in the loader (rejects conv whose harmonized TOF > 9 s); dead `t_cap_mono`
  deleted; `g`/`fire_dt` default args removed (no re-fork-by-omission); oracle fixtures aligned to
  `world.toml`. **Single-source audit:** `std::expm1`/`std::log1p` each appear EXACTLY once (gunsight.h).
- **Gate 383/383 (author-verified** — Sonnet misreported 380; the real ctest is 383, new Catch2 cases
  live inside already-registered binaries). Build clean.
- **NOT flown since the rework** — the slag rhythm (`tracer_slag_dark=0.75`, `tracer_slag_period_m=1.8`)
  and the new gunnery feel (pure-lead pipper, flatter cannon, 500 m harmonization) are Chad taste calls.

### iteration 6 — HARMONIZATION ELEVATION (rounds go through the reticle) — Fable-vetted, gated
**The ask (Chad):** fired rounds must pass THROUGH the reticle at the harmonisation range,
not droop below it. **Fix:** each gun now aims at the raised body-frame point `(0, +Δ, −conv)`
(was `(0,0,−conv)`) — angled up by exactly the gravity droop the round loses over its flight,
so the drooping path CROSSES the sightline at `convergence_range` (300 m). `Δ` is a fixed
body-frame **+Y cant** (realistic — bolted at the butts, attitude-independent; the dynamic
deflection instrument stays the lead pipper).
- **`weapon::harmonization_rise(muzzle_body, conv, muzzle_speed, drag_k, g, fire_dt)`** — the
  pure config function returning `Δ`. 3-pass fixed point over the SHARED drag laws. **g=0 ⇒ Δ=0
  exactly** (bit-identical vacuum seam). Cannon **Δ=0.963 m** (~3.2 mrad), MG **Δ=0.707 m** (~2.4 mrad);
  both classes still cross the SAME sightline point despite different speed/drag.
- **Single-source (Fable P0):** the droop / TOF / discretisation-lag laws were lambdas INSIDE
  `lead_solution` — now hoisted to pure inline helpers in `render/gunsight.h`
  (`dragged_tof`, `dragged_droop_dist`, `drag_lag_dist`). Pipper + round + harmonisation all call
  ONE law. Enforced: `std::expm1` / `std::log1p` each appear **exactly once** in the tree (in gunsight.h).
- **API:** `muzzle_world_dir` / `spawn` now take `g` + `fire_dt` (threaded from `fire_tick`, which
  already had `ap.g`/`ap.sim_dt`); `fire_tick` signature UNCHANGED. Only call sites: `fire_tick` + `test_guns.cpp`.
- **Moved golden (intentional, re-derived NOT re-recorded):** the old convergence test asserted the
  STRAIGHT ray hit `(0,0,−conv)` — now wrong by design (the ray aims high). New oracle FIRES the real
  round (`spawn`+`advance` under g & drag), interpolates the sightline crossing, and requires
  **≤15 mm vertical / 5 mm lateral** for all 5 muzzles. Catches no-compensation (≈0.95 m low),
  sign-flip (~1.9 m low), vacuum-Δ (drag dropped, ~0.27 m low). Plus a g=0 regression seam.
- **BY-DESIGN residual (documented in-code — do NOT "fix" into a velocity coupling):** the cant is
  computed at the resting nominal, so in a 150 m/s dive the inherited speed shrinks the droop and the
  cannon crosses ~0.27 m HIGH at 300 m (vs ~0.95 m low before). Correct deflection aim = the pipper.
- **Files:** `render/gunsight.{h,cpp}`, `weapon/ballistics.{h,cpp}`, `test/unit/test_guns.cpp`. Gate **383/383**.
- **NOT flown yet** — physics-verified by the integrated oracle; the gate is blind to `seads.exe`,
  but the crossing point IS the reticle in steady level flight so a screenshot adds little.

## DONE (iteration 1 — CONVERGED, Fable-vetted, gated, UNCOMMITTED)
1. **Drag model.** Vacuum integrator → per-round quadratic drag, exact update `v/(1+k|v|dt)`
   (`weapon/ballistics.cpp advance` via the single-source `apply_drag` helper), config-tabled
   (`[guns] cannon_drag_k_per_m`, `mg_drag_k_per_m`). Metric **E: 1.19 → 0.043**, all reference
   cells within tolerance. `drag_k=0` is bit-identical to the old vacuum path (existing pins hold).
2. **Pipper de-forked (was a P0).** `render/gunsight.cpp lead_solution` re-derived in the WORLD
   frame with the SAME shared `k`, plus discretization-lag + closed-form dragged-droop corrections,
   so a round fired down the solved lead HITS even for a moving shooter: **7–14 mm** miss at
   300/400/500 m (was a 6.5 m fork). Oracle: the moving-shooter cross-check in `test/unit/test_guns.cpp`.
3. **Drag constant single-sourced** — `app/main.cpp` loads `world.toml` first, then derives the
   gunsight `k` from `world.guns.cannon_drag_k` (no scenario.toml duplicate → no silent fork).
4. **Truth harness** `test/unit/test_ballistic_truth.cpp` fires each gun flat at dt=1/120, dumps
   `build/ballistics_measured.json`; score with
   `python offline_tool/ballistic_score.py --measure file:build/ballistics_measured.json`.
5. **Tracers rendered + polished.** App builds the 5-gun `weapon::GunWorld` from `world.guns`,
   samples LMB→`fire_held`, passes `&gw` to `step_frame`; `render/draw.cpp` draws a layered additive
   comet per round (soft glow + hot tapered core + white-hot head), view-distance-sized so bolts
   read at 20 m AND 500 m. Verify headless: `SEADS_SMOKE_FIRE=1 ./build/seads.exe --smoke 150 shot.png`
   (add `SEADS_RIGCAM=1 SEADS_RIG_AZ=90 SEADS_RIG_DIST=120` for a side/arc view). Evidence:
   `tracer_fwd.png`/`polish_fwd.png` + `_side` in the worktree root.

## ROUND CHOICES (Chad-ruled)
- Cannon = **MG 151/20 M-Geschoß** 805 m/s, k=1.565e-3 (shared with MG-FF/M — same 92 g projectile).
- Cowl MG = **SmK-v 855 m/s** (Chad's call — the -v round the cowl guns actually fired), using the
  sS-validated k=7.10e-4 as a **flagged proxy** (no open SmK-v firing table; SmK is lighter/pointed
  → likely decays slightly faster — a research-refine item).
- Ground truth is drag-model-derived (open Schusstafel are archive-locked); the real 7.92 sS German
  firing table validates the constant-k model to **<0.4%**.

## OPEN — NEXT AGENT, roughly in priority
1. **⓯ CHAD FLIES SESSION 2 — the gate here is his stick/ear, do this FIRST.** All landed + gated 421/421
   but render/audio is BLIND to the gate:
   (a) **Guns sound (S2.1):** two distinct weapons now? brrrt punchy? boom/tink read? mix vs wind/engine?
   Audition `gun_preview.wav` or fly. Fly-dials are code constants in `render/gun_audio.h`.
   (b) **Bandit speed 95 (S2.2):** catchable now?
   (c) **Pipper parallax fix (S2.3):** does the lead diamond read right — sitting ahead of the bandit, no
   longer floating above, especially banked / on a crosser? This was the headline complaint.
2. **COMMIT Session 2 once Chad's happy** (he must ask — commit/push only on request). Stage ONLY the
   SESSION-2 file list in "WHERE THE WORK LIVES" by EXPLICIT path; NEVER the 4 build-cruft files or the
   3 bench artifacts (`gun_preview.*`, `tools/gun_audio_preview.cpp`).
3. **Sub-metre pipper accuracy folds (S2.3 DEFERRED)** — Fable-specified, careful. Fold P2-4 (body-up cant,
   biggest) + P2-1 + P2-2 into `render/gunsight.cpp` `lead_solution` WITH test re-derivation (they move
   exact-value pinned `test_gunsight`/`test_guns` asserts — re-derive with documented intent, do NOT
   re-record blindly; the tolerance-based fire-down-pipper cross-checks should tighten, not break). Chad
   won't perceive sub-metre visually, so this is accuracy hygiene, not urgent.
4. **BUILD THE BANDIT COMBAT AI (S2.4)** — the big feature. `docs/bandit_combat_plan.md` is the spec
   (design + Fable pre-consult folded). Two gate-safe stages: pure modules (defaulted-off, tree stays
   green + bandits keep patrolling) → flip on in the app glue (player wired in, per-tick death resolution,
   HP HUD, enemy tracers + enemy-fire audio [reuse S2.1 synths]). Then gate → Fable red-team → Chad flies.
5. **Deferred polish (none blocking):** audio P2 timbre tweaks (tanh soft-clip, per-voice tau, first-crack
   phase); muzzle flash (low payoff from chase view); respawn delay (killed drone pops back instantly);
   proximity-gate the all-bandit range labels if a big fleet clutters; SmK-v cowl-MG proxy-k research refine.

## DISCIPLINE (SEADS house rules that bit us / matter here)
- **The green gate is BLIND to `seads.exe`** — ctest never runs the app, so tracer/render changes
  need a `--smoke` screenshot to verify (a human/parent views the PNG). Gate green ≠ visuals correct.
- **Firewall:** `weapon/`+`render/` read `sim::SimState`, never write; nothing feeds `sim/`/`control/`;
  no clock in `render/` (tracer fade uses the tick-derived projectile `age`).
- **Single-source or fork** — the round integrator and the pipper MUST consume ONE `k`. Two copies
  silently re-fork (that was the P0). Same for any new shared ballistic term.
- **Moved golden → STOP**, confirm intent, never re-record to pass.
- Author ≠ red-teamer: Fable consults run in a FRESH isolated context (`Agent model: fable`);
  implementation goes to Sonnet (`Agent model: sonnet`). One scoped Fable round per landed mechanism.
