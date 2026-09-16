# ENEMY AI — Rungs E1 (kill chain vs the ace) + E2 (the Black Stope offensive)

Fable spec, 2026-08-19. Chad's order (verbatim intent): "a truly formidable opponent that
will attack and kill me... smooth graceful killers up there who will underground strafe and
be capable of victory by pump destruction. They must actively and successfully enter the
tunnel, fly into the black stope and attack our allied pumps."

Worktree: D:\seads_sandboxes\enemy-ai (branch sandbox/enemy-ai @ 95d0b2c38, off the fly tree).
NO COMMITS until Chad releases the hold (mesh agents are landing on sandbox/audio).
Firewall: drone/, combat/, weapon/, app/instructor_tick.h + main.cpp glue, config/*.toml,
render tracer FX if needed, test/unit/. NEVER sim/, control/, goldens, test/harness/.

## THE MEASURED PROBLEM (tapes 83–88, ~81 min; full audit in the workflow journal)

- 22 enemy rounds total in six flights; 9 aimed at the player; 2 hits (~19 hp); ZERO player
  deaths ever recorded ('pd' absent). Tapes 86–88: zero rounds fired at anything.
- BFM Intercept 91–100% on every drone in every tape. Offensive ≤2%.
- Enemy strike orders on the allied deep pump (pumps[2]) armed 100% of every tape,
  on_station 0.0 s in 5 of 6 tapes, closest 4.9–18.5 km. Allied strikers DO kill the enemy
  deep pump (dead in 83/84) — the asymmetry is run-scheduling luck, not capability parity.
- Every in-net death in all tapes is a RUN-mode terrain crash; no combat has ever occurred
  underground.

## ATTRIBUTION (each gate cited; from the fire-chain + strike + tunnel audits)

E1 side — five gates starve the guns:
- G1 CLOSURE (binding): Offensive arms only < bfm_attack_range_m 1200 m (bfm.h:421-423).
  Intercept speed law clamp(v_player+25, 110, 85+90·align²) caps 175 m/s nose-on, 110 when
  turning (bfm.h:547-551, drone.h:1184-1195); airframe tops ~157 m/s (drone.h:576-585).
  Player: 259 m/s mean / 362 peak. The bandit can NEVER close any non-head-on geometry.
- G2 PHASE GAP: 2 s min_dwell after arming at 1200 m + fire band [60,600] m
  (scenario.toml:304-306) + 14° cone ⇒ guns_hot-AND-in-band measured 0.63 s per 90 s.
- G3 CONE vs LOS RATE: 55° bank cap ⇒ ~7°/s turn at fight speed vs 25–40°/s LOS from a
  crossing 259 m/s player at 400–600 m.
- G4 ENERGY BAIL: e_delta reached −3985 m at player peaks — still trips extend_energy_m 3000
  (bfm.h:450-453); Extend recovery needs e_delta > −200, structurally unreachable ⇒ every
  Extend is the full 16 s guns-cold; frustration_s 8 ends Offensive early.
- GRACE: no bank-rate slew anywhere (k_az 5.0 saturates 55° bank at 11° bearing —
  bang-bang roll); BFM transitions are step inputs (Extend snaps bank to 0, speed steps
  175↔110); engage/disengage is a discrete 3× gain jump.

E2 side — three locks keep them out of the stope:
- L1 STRIKE HAS NO TRANSIT VERB: StrikeOrder is armed on everyone every tick
  (instructor_tick.h:1260-1267) but its ONLY consumer is a parasitic divert requiring
  mode==RUN AND in_arena AND <6500 m (drone.h:1137-1154); maverick_step has zero
  StrikeOrder knowledge — run scheduling is trait-period patrol countdowns, and hold_runs
  freezes them for raid duty (4 of 10 pilots, always) or any foe <2500 m (drone.h:1105-1112).
- L2 THE DIVERT CAN'T REACH: pump at arena-center height ≈4.1 km underground, ~42° descent
  required vs the divert's 35° gamma cap (drone.h:404) inside a 90 s hardcoded bail budget;
  on-station needs 600 m + nose-on cos≥0.35 while STILL mode==RUN. No strike dials exist in
  any config.
- L3 NO GUNS UNDERGROUND, EVER: every underground mode is maverick_committed ⇒ pursue()/BFM
  bypassed, wants_fire forced false (drone.h:1113-1122, 1150), d.bfm reset per tick
  (pinned bit-identical by test_maverick.cpp:562); a NON-committed pursuer that follows the
  player underground trips terrain-avoid (AGL ≈ −2000) into a forced 26° climb with
  wants_fire=false (drone.h:1424-1444). Rounds themselves ARE underground-capable (T10) and
  the stope has full air — the lock is purely the decision layer. Strike damage is silent
  app-side DPS (raid_dps_frac × player battery), no tracers.

## RUNG E1 — CLOSE THE KILL CHAIN (surface; "smooth graceful killers")

E1.1 CLOSURE: new [drone] dial `intercept_speed_mps` (STRUCTURAL off-value: ≤ 0 means the
  legacy ceiling expression max(dl.speed+25, dl.speed+90·align²) runs bit-identical — the
  legacy ceiling is align-dependent, NOT a constant 175, so a numeric "175-equivalent"
  default cannot pass the knob-off differential; shipped 265). In Intercept/Extend with
  range > attack_range_m, the speed command ceiling becomes intercept_speed_mps (still
  align²-faded INTO turns — the ruled corner-speed law "chase fast, turn slow" is preserved
  verbatim; only the straight-line chase ceiling rises to the player's class).
  ★P0-2 fold — THE REAL LIMITER IS AUTOTHROTTLE DROOP, not an airframe wall: the
  drone.h:576-585 "~157 m/s" comment is STALE (predates the v5 RUNG D thrust bump; T=D is
  now ~245+). The autothrottle is proportional (throttle = 0.40 + 0.02·err), so holding a
  high speed needs a standing ~25-30 m/s error — a commanded 265 settles ≈235-240, still
  below the player's 259 mean. Fix DRONE-SIDE at the autothrottle seam: a speed-error /
  throttle feedforward dial (`throttle_ff`, 0 = off bit-identical). sim/ stays firewalled.
  ★Acceptance must pin ACHIEVED sustained intercept speed vs the stepped player, never the
  commanded value. ★Compression note: above v_redline 245 control deflection is at
  min_frac 0.4 — measure the E1.4 slew/blend tuning in that regime.
  ★P1-3 fold — NAMED RULED-PROPERTY REPEAL for Chad's fly ruling: scenario.toml's
  pursue_speed_bump text ("stays well under the player's redline (245), so a straight-line
  run still escapes — the fight stays beatable", 2026-07-26) is REPEALED by 265: a level
  straight-line run no longer outruns Intercept/Extend. Chad's "kill me" order is the
  authority; put this repeal in his fly checklist explicitly.
E1.2 PHASE GAP: `bfm_attack_range_m` 1200→2200 + `bfm_min_dwell_s` 2.0→0.5 +
  `fire_range_max` 600→900 (dials only, all exist or get TOML keys). Plus the SNAPSHOT gate:
  in pursue(), independent of BFM mode, if the full ballistic solution is inside the cone,
  coordinated, and range ∈ [fire_range_min, snapshot_range_m (dial, 900)], fire. This
  converts the head-ons and crossing merges an ace actually grants into real bursts.
  ★P1-2 fold — MUTE-ORDERING IS NORMATIVE: the snapshot gate lives INSIDE pursue()/the
  pursuit branch so the later defend/raid/leash-blend/terrain-avoid blocks still clear
  wants_fire in today's order. It fires at DRONE foes too (AI-vs-AI round volume rises);
  the range band [60, snapshot_range 900] + foe assignment structurally forbids long-range
  lobbing — REQUIREMENT with a probe: zero rounds spawned at >900 m target range.
  ★P2 fold — min_dwell 0.5 works against bfm.h's anti-chatter contract; the E1.4 C1 blends
  cover the felt side, and P-A pins a BFM transitions-per-minute bound.
E1.3 ENERGY BAIL: `bfm_extend_energy_m` 3000→6000 (effectively: bail only on true energy
  death, not on the player being faster); `bfm_reenter_energy_m` re-derived reachable
  (exit Extend on closure sign, not absolute e_delta — builder proposes, red-team checks);
  `bfm_frustration_s` 8→12.
E1.4 GRACE (also a gunnery fix — a slewed bank tracks smoothly): a bank-rate limiter on the
  autopilot's target_bank input, `bank_slew_dps` dial (shipped ~90°/s, 0 = off/bit-identical),
  applied at ONE seam (drone::tick where target_bank is finalized) so every producer
  (pursue/bfm/maverick/raid) inherits it; plus C1 smoothstep blends (~0.7 s) on BFM
  mode-change speed/bank targets. Knob-off must be bit-identical (house differential test).
E1.5 Difficulty: no damage change (one burst already ≈30 hp/hit — lethal if G1–G3 open).

## RUNG E2 — THE BLACK STOPE OFFENSIVE

E2.1 ON-ORDER RUNS: a live StrikeOrder on a living enemy deep pump COLLAPSES the pilot's
  patrol_countdown (staggered per index so entries stay one-at-a-time), for NON-raid-duty,
  NON-defend-duty pilots only (raiders keep the surface war; scrambled defenders stay on
  their pump). ★P1-1 fold — MERGE_HOLD STANDS UNTOUCHED: the collapse must NOT override
  hold_runs' merge component (foe < 2500 m) — overriding it re-creates the measured R4 bug
  (a committed TRANSIT marching a fighter away mid-merge = a free kill for an ace). The
  countdown collapses; the run FIRES the tick the merge ends. Collapse semantics: one-shot
  and idempotent (a standing order collapses the CURRENT countdown once; the post-run
  countdown reloads the trait period, then collapses again while the order stands — no
  per-tick re-zeroing). Knob-off: `strike_on_order` dial, false = today bit-identical.
  Entry direction: choose run_dir that enters from the pilot's own faction mouth (the
  target pump sits on the FAR side of the arena from there — this is load-bearing for the
  E2.2 gamma: near-mouth entry roughly doubles the required descent; write the coupling
  into the code comment). ★Applies to BOTH factions symmetrically (fair war — allied strike
  tempo also rises; matches can end without the player). FLAG IN CHAD'S FLY CHECKLIST:
  symmetric vs enemy-only is his call; symmetric ships.
E2.2 A DIVERT THAT REACHES: strike dials move to config ([combat] strike_* keys — engage_m,
  bail_s, gamma_cap, k_az/k_el, bank_cap, on-station envelope). gamma_cap 35°→50° (clears
  the required ~42° with margin; above the run/transit 45° precedent, below dive 65° — the
  50° bound is deliberate and documented), bail_s 90→150, and while diverting the RUN's
  exit_frac flip to CLIMB_OUT is deferred (the divert owns the mode until
  bail/pump-death/arena-exit; belt-and-braces — s_est stagnates off-spine anyway).
  ★P1-5 fold — LOADER + SAFETY CASE: every new strike_* key gets the same
  loader-vs-airframe checks as existing AI speed/gamma keys (load_scenario.cpp:641-712
  precedent). The divert currently steers with NO floor/wall guard; the safety case at 50°
  is (a) aim_at's error-nulling levels out at pump altitude and (b) the pump sits ~2 km
  above the arena floor — WRITE IT IN THE CODE, and the E2.4 arena-shell wall guard applies
  to the DIVERT too, not just the fight interrupt.
E2.3 UNDERGROUND STRAFE (visible): while diverting and inside strike_params envelope, spawn
  REAL rounds at the pump (they already fly in the net, T10); damage stays on the existing
  damage_pump DPS path (no enemy-round-vs-pump hit test — no double count, balance
  derivations untouched); the rounds are the tracer/strafe read + the audible warning. HUD
  raided_pump warning already exists.
  ★P0-1 fold — THE FIRE PATH MUST BE MADE STRAFE-CAPABLE EXPLICITLY: enemy_fire_tick
  (kill.h:458-468) gates on `d.engaged && d.wants_fire`, and the ballistic solve consumes
  d.gun_tgt_pos/gun_tgt_vel which only pursue() writes. A striker over an UNDEFENDED pump
  has foe=kFoeNone ⇒ engaged=false ⇒ zero rounds, and gun_tgt is stale (or {0,0,0} ⇒ rounds
  slewed toward planet center). REQUIRED: the strike-strafe path sets
  gun_tgt_pos = pump.pos and gun_tgt_vel = 0 SAME-TICK, and enemy_fire_tick accepts a
  striking drone WITHOUT engaged via an explicit strafe-fire clause. Knob-off bit-identical.
  P2 debt (log, don't build): rounds retire silently in rock — no wall-impact FX in the
  chamber; acceptable for the tracer read, polish later.
E2.4 THE CHAMBER FIGHT: extend the terrain-avoid exemption to any drone in_arena
  (drone.h:414-423 is the trusted predicate). While in_arena and a foe is within
  arena_fight_range (dial, 3000 m): committed strikers take a FIGHT INTERRUPT (same budget
  pattern as the strike bail) that re-enables pursue()+BFM with gammas clamped to the
  chamber (yoyo_gamma capped in_arena), and an arena-shell wall guard (reuse the bore wall
  guard's hysteretic pattern against the arena ellipsoid) as the fallback steering. The
  player defending the pump GETS SHOT AT underground. After the interrupt budget or foe
  loss, the divert/run resumes.
E2.5 DEEP-PUMP DEFENSE: stays out of scope this rung (assign_defense untouched) — the
  defender in the chamber is Chad.

## VICTORY CONDITION
With E2.1–E2.3, sustained strike traffic on pumps[2] must be able to actually destroy it
(victory by pump destruction). Probe must show pump-2 HP → 0 within a bounded sim when
undefended.

## ACCEPTANCE (primary-data law: probes reproduce the REAL chain; STEPPED player, never a
stamped-velocity parked state — the fixture-phantom trap)
- P-A "the ace gets shot at": headless 10-min conquest composition with a scripted
  maneuvering 250–300 m/s player (real loaders, real assign_foes/drone::tick order) →
  enemy rounds aimed at player ≥ 10× the tape baseline, ≥1 hit sequence; compare pre/post.
- P-B "the stope falls": undefended run → strike traffic enters the tunnel on order,
  on_station ≥ 30 s per window, pump 2 destroyed within 15 min.
- P-C "the chamber fight": player flown into the arena during a strike → enemy rounds
  spawned underground at the player; no terrain-avoid pull-up into the ceiling.
- Knob-off differential: all new dials at off-values ⇒ bit-identical trajectories.
- Gate: full ctest suite green in build/ (Debug); existing-test failures REPORTED verbatim,
  Fable adjudicates (welded fixture vs regression), never silently re-pinned.
- Final: Chad flies build-play; the new tape is the verdict.

## KNOWN RISKS / RED-TEAM TARGETS
- The corner-speed law and hold_runs were RULED mechanisms (R4/R5) — E1.1/E2.1 must not
  silently repeal them, only extend (chase ceiling; strike-order override scope).
- test_maverick.cpp:562 pins "committed run ignores the player" — E2.4 deliberately moves
  this for the in-arena case. ★P1-6 fold — EXPLICIT FIXTURE RULE: the pinned fixture parks
  the drone at 0.4·L with an engaged player 300 m away, which may be in_arena for the
  synthetic net, so the new interrupt would fire INSIDE the old pin. The re-scope must MOVE
  the fixture out of the arena (or disable arena_on there) so the outside-arena
  bit-identity pin stays honest, and ADD a new in-arena leg proving the interrupt engages.
  Never merely loosen the old assertion. Fable adjudicates the final shape.
- E1.4 bank-slew details (P2 folds): slew state resets in respawn_in_place; the slew is
  applied at the FINAL seam after the terrain-avoid clamp (slewing INTO safety clamps is
  fine, delaying the clamp is not); regression leg: tape-85-class bore runs must not crash
  MORE with the slew on (RUN crash count pinned not-worse in the fleet probe).
- Yo-yo gamma envelope vs the 2600 m chamber semi-axis; snapshot gate vs AI-vs-AI ammo spam
  at 24 km (snapshot must respect foe assignment + range band, never the 24 km drone-v-drone
  lobbing already seen).
- Certificate section A runs air_war=false for coverage reasons — new probes must not
  inherit that blindness.

## RUNG E3 — THE FELT PASS (Fable, 2026-08-20, from Chad's fly tape conquest_tape_1)

CHAD'S VERDICT: "they behaved the same as always, didn't engage." TAPE ATTRIBUTION (13:41,
sig VERIFIED, the new dials PROVEN live — Offensive 9-10%, allied strafe tracers in the
net, both enemy pumps destroyed by the allies): the on-order collapse put the 3 non-raid
ENEMY strikers into committed TRANSIT 53-80% of their lives toward a pump 20+ km away;
committed TRANSIT ignores the player, so they flew straight lines past an ace and were
executed (every enemy strike window ends "drone killed by fire" at rng_p 15-706 m — Chad
killed them point-blank; all three dead by 3:34, permanent in conquest). Dead or committed
enemies produced 5 rounds at the player, 0 hits. E2.1 made the SURFACE felt problem worse.

E3.1 TRANSIT FIGHT-YIELD: a committed TRANSIT (and ONLY TRANSIT — DIVE_IN/RUN/CLIMB_OUT
  stay committed, that ruling stands) with a foe inside `transit_fight_yield_m` (dial,
  shipped = raid_fight_yield 2500) ABORTS back to PATROL (clean give-up through the
  existing give-up path so countdown reload + strike_collapsed clearing stay coherent) and
  fights; the on-order collapse re-launches the run after the merge ends (merge_hold
  already holds the countdown during the fight). Knob-off 0 = today bit-identical. The
  transiting striker must no longer be free prey.
E3.2 CONCURRENT-STRIKER CAP: at most `strike_concurrent_max` (dial, shipped 1) pilots per
  faction may be in a collapse-TRIGGERED run pipeline (TRANSIT..CLIMB_OUT) at once; the
  others' countdowns stay collapsed-pending (order stands, launch waits). Natural
  trait-period runs are NOT capped (the old flavour stands). This keeps the enemy wing on
  patrol — where E1 makes them dangerous — instead of all-in-transit at match start.
E3.3 ACCEPTANCE: extend P-B/P-C unchanged (they must not regress); NEW probe leg P-D "the
  transiting striker fights back": stepped player parked on the transit corridor within
  2500 m -> the striker aborts, engages, fires; after the player leaves, the run
  re-launches and completes. Certificate must stay green as written. Full gate; verbatim
  failures; Fable adjudicates.

## RUNG E4 — VISIBILITY (Chad-ordered 2026-08-20; QUEUED behind E3; render lane)

Chad: "they are hard to see especially in the white scatter light, their tag and color is
also a bit hard to see." The ruled art direction already wants saturated punchy planes
against the B&W world — the scatter pass is washing them out.
FIREWALL: render/ + app draw glue + config only. NEVER sim/, control/, drone/ logic. The
flown-approved atmosphere/scatter visuals must be preserved when the new dials are off.
E4.1 AIRCRAFT HAZE EXEMPTION: drones receive only a partial share of the atmospheric
  scatter/haze at range — dial `aircraft_haze_frac` (1.0 = today bit-identical; shipped
  ~0.35). Applies to the drone meshes only, never terrain/sky (the limb must not fork —
  single-sourced sky_color stays untouched).
E4.2 TAG LEGIBILITY: dark outline/halo behind tag text (contrast on white sky AND dark
  ground); minimum on-screen tag size floor (never below readable px); enemy tint pushed
  to a deeper saturated red that cannot read as haze-grey; allies stay on the ruled
  kComplementBlue / slag-orange family (team_color.h is the single source — never a second
  literal).
E4.3 ENGAGEMENT GLINT: a subtle deterministic nav-light blink / canopy glint on enemies
  inside engage range (age-phased, no rng/clock). REAL GEOMETRY (DrawSphere/DrawCylinderEx)
  — DrawBillboard silently renders nothing in this engine.
Acceptance: screenshot pair (scatter-lit sky, enemy at 2-4 km) before/after via
  SEADS_SPAWN_ALT-style harness if available, else Chad's fly judges; all dials off =
  today's frame; gate green.

## RUNG E5 — REINFORCEMENT WAVES (Chad-ordered 2026-08-20; QUEUED behind E4)

Ruling: keep 5v5 density; the match must never go empty. A killed conquest pilot returns
as a FRESH pilot after `reinforce_delay_s` (dial, shipped ~75; 0 = off bit-identical =
today's permanent death), launching airborne from its faction's home side (reuse the
spawn scatter), fresh DroneState + trait identity, wreck stays. Kills stay meaningful
(the wave delay is earned breathing room). Also raise `max_engaged` pressure dial for the
fly (3 -> Chad's call, offer 4).
Constraints: deterministic (tick counters), respawn_drones=false semantics superseded
CONSCIOUSLY (this is the Chad ruling that changes it — name it in the toml comment);
score/radius_scale interaction checked (a reinforced pilot must not re-inflate a crushed
faction's radius_scale silently — read conquest.h before wiring); certificate respawn
bound (respawns < 40) re-examined, adjudicated not silently re-pinned.
Acceptance: probe — kill the enemy wing, waves refill it, the match still ends by pump
destruction; knob-off differential; full gate.
★MILLWRIGHT HORIZON CONSTRAINT (Chad-ruled 2026-08-20, memory millwright-tier-canon): the
airborne-respawn policy must be a SWAPPABLE seam, not welded — the coming Millwright tier
replaces the player's airborne respawn with land-at-a-marker, adds a per-respawn
pilot/millwright class choice (offered only while your pump is damaged), and dispatches
destroyable AI millwrights on pump damage. Build the wave respawn so that policy can be
substituted without re-plumbing.

## POST-BUILD LEDGER (Fable, 2026-08-19 late)
- Diff red-team verdict: SOUND-WITH-FIXES, no P0s. P1-1 (arena guard RUN-only clause vs
  its own safety case) FOLDED: striker_in_chamber = is_underground_mode (drone.h). P1-2
  RECORDED AS OPEN DEBT: no end-to-end composition certifies strikes + air war + defense
  SIMULTANEOUSLY any more (cert A = strikes-only, B = air-only, P-B undefended, P-C parked
  strikers) — Chad's fly IS that composition this time; a combined smoke leg is the named
  follow-up rung.
- E2.1 transit starvation attributed + fixed (transit_fix_grace_s 90, earned once at FIX
  capture, bounded 150+90; the livelock hypothesis was REFUTED by instrumentation — the
  150 s budget spanned both approach stages and pilots died aligned on the final leg).
- Adjudications: test_load_scenario re-pins (frustration 12 / reenter 3000 / attack 2200),
  yoyo mutant 0.3 (config-relative), certificate fixture re-derived to mirror the app's E2
  stamping + explicit strikes/air_war section isolation.
- P2 debts (polish, not blocking): no wall-impact FX for rounds retiring in rock; strafe
  tracers up to ~70 deg off-nose read as suppression; slew-vs-bore-guard only covered
  combined, not isolated; kDeepPumpHeightAboveFloor_m 30 is pre-existing dead code.

## RUNG E6 — THE RELENTLESS PASS (Chad's fly verdict on tape 2, 2026-08-20 midday)

CHAD (verbatim intent): died twice "for no reason" with random respawn; no enemy tunnel
flying; "mostly they run"; enemies fly fine in no-air and use it; revived enemies stayed
home; enemies poke a pump then abandon it — "they should relentlessly attack it and fight
me at the same time, not fly away to safety and then fight me. We are literally fighting
for air here"; and wave replacement kills victory-by-elimination.
TAPE 2 FACTS: 0 'pd' events (his deaths were NOT enemy fire — cause invisible to the
recorder), 1 hit all match; enemy dome rs crushed to 0.0 by 10:36 (vacuum everywhere on
their side); 8 'dr' revives but zero enemy tunnel entries; enemy revives after their dome
died; pump 0 died at 13:42 (slow grind, not a press).

E6.1 RECORDER FIRST — PLAYER DEATH CAUSE: extend the tape with a player-death event
  carrying the CAUSE (gun / terrain crash / O2-vacuum / component), emitted at the app's
  death-reset seam. Spec §amend + analyzer + selftest. Attribution of his two deaths
  comes from the NEXT tape, not from guessing. If the killer is O2/vacuum, flag the HUD
  warning legibility as its own follow-up (do not build HUD this rung).
E6.2 THE VACUUM TAX IS SYMMETRIC: attribute how the drone plant treats atm_frac
  (thrust/lift/speed). Chad's felt report says they fly fine in no-air. Fix: the drone
  plant pays the same class of vacuum penalty the player's plant pays (drone/ lane —
  dial, off = today), AND the AI must not choose vacuum as safety: Extend/flee headings
  prefer in-air destinations (reuse the leash's dome geometry; a bail into vacuum is a
  bail into a slow death, not an escape).
E6.3 THEY RUN TOO MUCH: retune the posture toward total war — "fighting for air".
  Extend is for ENERGY, not for escape: bfm_extend_max_s down (16 -> ~8), frustration up,
  and a new AGGRESSION POSTURE: while the pilot's faction pump is under attack OR its
  raid/strike target is alive and in range, Extend is suppressed (guns stay in the fight).
  Dials with off = today; measure with P-A (the rounds-on-ace bar must not regress).
E6.4 REVIVED ENEMIES MUST REJOIN THE WAR: attribute why post-revive enemies sat home —
  prime suspect = BubbleLeash vs a crushed dome (rs 0.0-0.25: the leash pins them inside
  a bubble that barely exists). Fix so a revived pilot with standing raid/strike orders
  flies them (leash yields to orders as it already does for raids — verify that exemption
  survives a dead dome), and enemy strike traffic actually enters the tunnel (tape 2 had
  ZERO enemy entries — re-check the E3.2 cap + fight-yield interaction under real combat).
E6.5 RELENTLESS RAIDS (Chad's ruling OVERRIDES the Phase-2 raid pause): a raider under
  player threat does NOT abandon the pump — the 1500 m raid-pause retreat is replaced by
  fight-in-place: keep the attack run pattern on the pump, take gun shots at the player
  when the geometry offers them (the E1 snapshot gate already allows out-of-BFM shots),
  break off ONLY for terrain/wall safety, never to "safety". raid_pause dials -> off by
  default (kept as dials for his walk-back).
E6.6 FINITE REINFORCEMENT POOL (Chad's ruling: elimination victory must be attainable):
  reinforce_pool_n dial (shipped ~4 per faction) — each faction has N wave revives per
  match; pool exhausted => waves stop => the existing wipe-victory path re-arms exactly
  (reinforcements_live goes false with the pool). Off semantics: pool <0 = infinite
  (today's E5), 0 = no waves (pre-E5). The HUD/score needn't show the pool this rung.
ACCEPTANCE: P-A not regressed; P-B/P-D/P-E updated where the pool/posture changes them
  (adjudicated, never silently); NEW probe P-F "the relentless raider": player parked
  700 m off a raided pump -> the raider keeps damaging the pump AND fires on the player
  within the window, never exits the pump area while both live; NEW probe P-G "pool
  exhaustion": kill waves through the pool -> waves stop -> wing wipe latches VICTORY.
  Full gate; verbatim failures; Fable adjudicates.
- Combined E3+E4+E5 red-team: SOUND-WITH-FIXES, no P0s. FOLDED: P1-1 fight_yield is
  PLAYER-FOE ONLY (an allied transit striker was aborting its stope run whenever the
  player flew formation within 2.5 km); P1-2 analyzer selftest now emits+asserts 'dr';
  P2-3 reinforcements_live stamped before the 6c AI-vs-AI sweep too; P2-1/2/4 comments.
- ⚠NAMED OPEN DEBT (drone-v-drone free prey): an enemy transit striker gunned by an
  ALLIED DRONE at close range still never yields — the full fix measures range to the
  ACTUAL foe, which touches signed merge_hold behavior = Chad's ruling when he wants it.
- max_engaged ADJUDICATED 4→3 (E5 builder's bisected ensemble: 4 attackers HALVE rounds
  on the ace — crowded geometry; finding at the dial in kill.h; "offer 4" spec text is
  consciously overruled on Chad's own goal metric, one character to flip back).
- Victory-by-wipe is SUSPENDED while waves are live (game.toml names Chad's supersession);
  waves stop for a crushed faction (no air to launch into); wave revives are 'dr' tape
  events (spec §amended) so attribution stays clean.

## RUNG E7 — THE KILLERS (Chad's ruling 2026-08-20 evening: "I want killers to contend
## with. That is a rule." + BOTH doctrines approved)

THE WALL (measured across every probe): the RULED 55-deg bank cap gives ~7 deg/s of turn
at fight speed vs the ace's 25-40 deg/s of LOS rate — no gun solution exists unless he
flies straight. Chad has now RULED the repeal for engaged fighters, BOTH doctrines:
E7.1 THE ACES TURN: per-pilot engaged bank cap raised via trait aggression — the top
  tier (aggression >= ~0.75: PITVIPER, SHAFT, MURRAY, NICKEL-class) fights at
  `ace_bank_cap_deg` (dial, shipped 72 ~= 3.2 g); mid-tier at ~65; the rest keep 55.
  Applies ONLY in the engaged pursue/BFM path — patrol/maverick/raid steering keeps the
  ruled 55 (the corner-speed law survives everywhere except a committed dogfight).
  Loader-checked vs airframe; measure G-load honesty (the drone plant must actually fly
  it, not command-and-mush) and the E1.4 slew at the higher rates.
E7.2 THE SLASHERS: a new BFM doctrine for the non-ace tier — PERCH/SLASH energy attacks:
  climb to a perch offset above the player, dive through at intercept speed with the
  snapshot gate hot, zoom back up on the far side, re-perch. Never sustained-turn. Uses
  the E1 speed + snapshot machinery as-is; the doctrine is a BFM mode addition (dwell
  rules, hysteresis, chamber-gamma clamps respected in_arena).
E7.3 REAL EVASION (the "killers" rule): defending drones under fire stop elevator-jink
  dipping — defensive BFM = break turn INTO the attacker's plane at their (new) bank
  authority, unloaded extensions, reversal when the attacker overshoots. Their evasion
  must read as flying, not bobbing. (Chad: "They only tried to dodge my shots with
  elevator dipping up and down.")
ACCEPTANCE: P-A ensemble re-run — the bar RISES: rounds at the ace and HITS must exceed
  the E6 numbers (39/18) meaningfully with the doctrine split on; a new P-H "the ace
  duel": stepped ace-course player vs one PITVIPER-class at shipped dials — require a
  sustained tracking solution >= 2 s at least once per 10 min and hits > E6 baseline;
  knob-off differentials per dial; full gate, verbatim failures, Fable adjudicates.
NAMED REPEAL for the record: the 55-deg engaged bank cap (R4/R5 corner-speed ruling) is
repealed BY CHAD for engaged fighters, tiered by trait. Walk-back = ace_bank_cap_deg 55.

## THE TWO DEATHS — UNRESOLVED, DEFERRED (Chad's ruling 2026-08-20)
Fable's tape-2 reading (shallow descent to ~+103/+127 m DATUM altitude at 270-290 m/s,
map open, then the jump) suggested CFIT into a ridge. CHAD REFUTES IT ON TESTIMONY: "I
was quite high up and I'd know if I were falling... I would know if I crashed." His
visual outranks the reading (house law) — note the tape records altitude above the DATUM
SPHERE, not local terrain, so a DEM spike / projection-seam artifact producing a phantom
crash while genuinely high is fully consistent with his account. ALL map-pause/hold/klaxon
proposals REFUSED AND DEFERRED. The E6.1 death-cause event (cause+pos+air on every death)
is the instrument that settles it passively on a future tape. Do not re-litigate.

## E6 RED-TEAM LEDGER (Fable, 2026-08-20 evening — SOUND-WITH-FIXES, folds applied)
- P1-1 FOLDED: fight-in-place gun block is PLAYER-FOE only (an allied raider was
  tracer-hosing the player; an enemy raider was an extra gun past max_engaged 3).
- Builder REFUSALS accepted on measurement: extend_max 8 HALVES rounds on the ace ("they
  still run" = NAMED RESIDUAL for Chad's ruling); the raid gun-pass cost all pump
  pressure for zero rounds (press-the-pump ships; raid_fight_in_place=false = walk-back).
- P2 watch items: enemy_waves_live ignores breathability (pool remaining + crushed dome =
  wipe veto true while every deploy refuses; bounded by sudden-death); both-crushed leash
  edge is symmetric-endgame only; the aggression posture disables the frustration escape
  at defended pumps — WATCH the next tape for orbit-spirals there (the R4 cure's valve).
- If the next tape shows vacuum-mush deaths (pd cause=crash with air<0.25), a hypoxia/O2
  mechanic + HUD warning is ITS OWN rung — do not bolt it onto E6.

## RUNG E6 — POST-BUILD LEDGER (Opus builder, 2026-08-20)

GATE: 1437/1441 in build/ (Debug). The 4 failures are the PRE-EXISTING GI4 sled
debt, verbatim: `sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.
Nothing else fails. New TU: `test/unit/test_enemy_ai_e6.cpp` (17 cases).

### E6.1 RECORDER — ATTRIBUTION AND FIX
MEASURED ROOT CAUSE of "died twice for no reason, zero 'pd' events": the `pd`
edge was `CombatWorld::deaths`, and that counter is incremented by exactly ONE
of the app's TWO death-reset branches (the component-death branch). A TERRAIN
CRASH reset booked nothing, so a crash death was STRUCTURALLY invisible — which
is precisely the class of death "for no reason, random respawn" describes.
FIX: a write-only ledger on CombatWorld (`death_events`/`death_cause`/
`death_pos`/`death_air_frac`/`death_in_net`/`death_damage`/`last_damage_src`),
booked at BOTH seams by `app::book_player_death`. Cause taxonomy
`crash|gun|ground|component`. ★ NO `o2` CAUSE EXISTS, and that is a finding, not
an omission: this build has NO hypoxia/O2 damage path at all — nothing anywhere
reduces a component for lack of air — so a vacuum death can only present as a
crash. The recorded `air` fraction is the honest instrument; the analyzer flags
any crash with air < 0.25 as a vacuum mush. Spec amended at
docs/conquest_tape_spec.md §10; analyzer + selftest updated.

### E6.2 VACUUM — ATTRIBUTION CAME OUT THE OTHER WAY
The felt report ("they fly fine in no-air and use it") is NOT a plant asymmetry.
`drone::tick`'s last act is `sim::step(d.curr, in, ap, env, dt)` — the SAME
kernel call with the SAME Environment the player flies, so thrust (T_max·f_atm)
and dynamic pressure (rho·f_atm) lapse identically. MEASURED (test "E6.2
attribution"): a level airframe at full power over 30 s holds altitude in full
air and mushes in vacuum, on the one shared `sim::step`. There is no drone-side
discount to remove — building one would have PENALISED the AI beyond the player.
WHAT IS ASYMMETRIC IS A BEHAVIOUR: the containment leash is switched off
wholesale for any ENGAGED drone (the R4 "a live fight overrides containment"
ruling), and BFM Extend is engaged-but-running-away — so an extend can carry a
bandit clean out of his own dome into air he cannot fly in. `extend_leash`
(shipped true) keeps the leash live for that ONE guns-cold mode; the other three
keep the R4 exemption verbatim. Measured: 7.5 s of extend from 6.5 km out on an
8 km dome ends CLOSER to the air with the dial on; off is bit-identical.

### E6.3 POSTURE — THE PROPOSED RETUNE IS REFUSED ON MEASUREMENT
P-A, 8-engagement ensemble x 10 min per arm, rounds ON THE ACE / hits:
  shipped R5 table (extend_min 8 / extend_max 16 / frustration 12) . 39 / 18
  the E6.3 proposal (4 / 8 / 12) .................................. 31 / 12
  frustration alone (8 / 16 / 16) ................................. 24 /  9
R5's LONG extend is load-bearing for the kill chain, for the reason R5 gave: a
short extend re-enters badly aligned and the run-in never converges into the
fire band. Shortening it buys "they run less" at the cost of HALF the rounds on
the ace — the one metric this ladder exists to raise, and the one E6's own
acceptance says must not regress. Raising frustration is worse (it holds a pilot
in an unresolved Offensive instead of re-merging). ALSO MEASURED:
`bfm_extend_energy_m` is a DEAD exit at this table (6000 -> 12000 changes
nothing, 39/18 either way) — frustration_s is the ONLY route into Extend that
ever fires. E6.3 therefore ships as the AGGRESSION POSTURE alone
(`aggression_range_m` 3000): Extend is suppressed while the pilot's own
objective is hot (a live DefendOrder, or a raid target inside the radius),
which is where Chad's complaint actually lives. The STRIKE order is
deliberately NOT a trigger — it is armed on 100% of every pilot's ticks on tape
2, so using it would be a permanent Extend repeal, not a posture. P-A with the
posture armed: 39 / 18 — unchanged, because that fixture arms no orders.
★ FOR CHAD: "mostly they run" is only half-answered. The blanket cure costs him
the guns; the posture cures it exactly where an objective is at stake.

### E6.4 REVIVED ENEMIES — ATTRIBUTION
Two candidate mechanisms were tested; ONE IS REFUTED.
  REFUTED — "a dead-or-reviving striker wedges the E3.2 cap": the cap's count
  loop already skips `inert` drones, so a wreck cannot hold a slot. Pinned by
  test "E6.4: a dead or reviving striker never wedges the concurrent cap".
  CONFIRMED — the CRUSHED-DOME LEASH. The app's leash arming falls back to the
  surviving faction's dome only when its own ellipse is LITERALLY degenerate
  (a <= 0 or b <= 0). A dome floored at kFactionScaleFloor is a live, tiny
  ellipse, so containment faithfully pinned the whole enemy wing inside a bubble
  that barely existed — tape 2: enemy radius_scale 0.0 from 9:17, revives at
  13:42 and 14:26, and they stayed home. `leash_min_radius_scale` (shipped 0.25)
  makes CRUSHED count as EXTINCT, so a crushed faction is leashed to the
  enemy's air — the same "confinement DRIVES the invasion" rule the extinct
  branch already runs. Off (0.0) is bit-identical.
TAPE-2 NUMBERS behind "ZERO enemy tunnel entries" (7 entries, all allied —
i=5,6,7,8,9): the three enemy strikers (i=2,3,4) spent 41-53% of their lives in
TRANSIT and never reached a mouth, and EVERY enemy strike window in the tape
ends "drone killed by fire" at rng_p 40-265 m. They were executed at
point-blank before a ~20 km transit could complete, six times. That is a
survivability problem (E5 waves + the E6.6 pool now govern it), not a scheduler
problem.

### E6.5 RELENTLESS RAIDS — WHAT SHIPPED AND THE MEASURED TENSION
Two halves, both shipped:
  APP SIDE `raid_no_pause` (game.toml [conquest], true): the FIX-F3 raid pause
  can no longer suspend a raid. The hysteresis latch still runs so the dials are
  one character from a walk-back. This is the "poke a pump then abandon it" the
  tape shows literally — the ORDER is dropped, which also kills the on-station
  DPS credit.
  DRONE SIDE `raid_fight_in_place` (scenario.toml [combat], true): a raider in a
  live merge KEEPS its attack run on the pump instead of being handed wholesale
  to the dogfight, and takes the E1.2 snapshot shots its run-in line offers.
PROBE P-F ("the relentless raider": a raider on order, a STEPPED player flying a
hard banked orbit 700 m over the pump he is defending, 90 s):
  PAUSE (today) . on_station 0.00 s | max range from pump 6388 m | 17 rounds
  PRESS (E6.5) .. on_station 3.05 s | max range from pump 5188 m |  0 rounds
★ THE MEASURED TENSION, recorded rather than asserted away: pressing the pump
and holding a 5-degree gun solution on a maneuvering defender are in real
conflict. A NOSE-DEVIATION "gun pass" was built and measured and is NOT shipped
— swinging onto the player whenever he entered the snapshot band cost ALL the
on-station time (3.05 -> 0.00 s) and still produced zero rounds, because the
deviation only ever gets fractions of a second before 300 m/s of closure carries
them past each other. The ruling says PRESS THE PUMP, so that wins the steering.
The rounds-on-the-ace bar belongs to P-A (unchanged at 39/18) and to the other
eight pilots of the wing, who are not raiders. ★ FOR CHAD: his raider now
refuses to leave, but a raider nose-locked on a pump is a poor shot — if he
wants the raider to fight him first and the pump second, that is a ruling, and
`raid_fight_in_place = false` is where it lives.

### E6.6 FINITE POOL
`reinforce_pool_n` on ReinforceParams/ConquestParams (shipped 4 per faction;
< 0 = infinite = rung E5, the off-value; 0 = no waves = pre-E5). Counted per
faction on DEPLOY, never on arm, so a refused wave ("no air to launch into")
costs a faction nothing — the two reasons a wreck stays down remain separately
attributable. `reinforcements_live` is now stamped from
`app::enemy_waves_live` = "can the faction whose wipe would END the match still
be refilled" — which is what that flag has always meant operationally. The wipe
route in conquest.h is UNCHANGED; it is simply no longer vetoed once the pool is
spent.
PROBE P-G ("pool exhaustion"): with the pool intact a full enemy wipe latches
nothing; with the enemy pool spent the same wipe latches VICTORY.
P-E ADJUDICATION: P-E builds its own ReinforceParams, whose `pool_n` defaults to
-1 (infinite), so its infinite-wave contract is preserved BY THE OFF-VALUE — no
assertion moved, and it passes unchanged (245 s).

### DIALS SHIPPED (all with a structural off arm + differential test)
  scenario.toml [combat]  extend_leash = true           (false = today)
                          aggression_range_m = 3000.0   (0 = today)
                          raid_fight_in_place = true    (false = today)
  game.toml [conquest]    reinforce_pool_n = 4          (-1 = today/E5)
                          leash_min_radius_scale = 0.25 (0.0 = today)
                          raid_no_pause = true          (false = today)
  E6.1's recorder ledger is behind no dial because it is behind no BEHAVIOUR:
  every field is write-only observation, pinned by "the death ledger is write
  only and moves no trajectory" (240 ticks flown with the ledger pre-poisoned,
  bit-identical).

## RUNG E7 — BUILD LEDGER PART 1 (Opus, 2026-08-20 night, picking up the Fable
## session mid-flight after a model switch)

STATE INHERITED: E7.1/E7.2/E7.3 fully written (bfm.h +430, drone.h +183, loader,
dials, tape enums, test_enemy_ai_e7.cpp 1057 lines), but the last edits to
bfm.h/drone.h/kill.h post-dated the test binary (unbuilt), and a `REQUIRE(false)`
dial sweep was left mid-flight in test_killchain_probe.cpp. Nothing was gated.

### E7.A TWO DEFECTS FOUND AND FIXED (both are "verify the artefact" cases)

1. ★ THE E7.3 ACCEPTANCE PROBE WAS MEASURING NOTHING. `under_the_gun` scripts an
   attacker welded to the bandit's six, but never booked a HIT — and the break's
   predicate is `under_fire AND geometry`, where under_fire means ROUNDS HAVE
   LANDED (the requirement Fable's own P-A measurement forced, added AFTER the
   fixture was written). Measured symptom: 0 defensive ticks with the dial fully
   armed, and the mode histogram flat at 0 for Perch/Slash/Defensive. The
   fixture now books through the SAME gun window the app's AI-vs-AI damage seam
   uses (`combat::ai_guns_on` at the drone fire band), so it models the real
   seam instead of inventing a cadence. E7 unit tests went 6/10 -> 10/10
   (116 assertions).

2. ★ THE `took_fire` BOOKING COVERED ONLY ONE OF THE TWO DAMAGE SEAMS.
   `combat::combat_tick` books it for PROJECTILE hits — which is only ever the
   PLAYER's guns. AI-vs-AI fire is an abstract DPS drain (`app/instructor_tick.h`
   ~1649) that spawns no projectile and booked nothing. Shipped as written, the
   defensive break would have armed ONLY against the player and every
   drone-on-drone fight in the sky would have kept the exact elevator bob Chad
   named. Booked at that seam too; still a write-only observation while
   bfm.defensive_range_m <= 0, so no trajectory moves at the off table.

### E7.B THE DECISION TABLE — E7.1's MECHANISM IS REFUTED ON MEASUREMENT
P-A ensemble, 8 x 10 min per arm (the ladder's own acceptance instrument):

  E6 (all E7 off) ......... rounds 39  hits 18  wants_fire 522  in_band 10940  crashes 0
  E7.1 only (tiered bank) . rounds 20  hits  7  wants_fire 257  in_band 10798  crashes 3
  E7.1 + E7.2 (slashers) .. rounds 13  hits  6  wants_fire 154  in_band 11125  crashes 3
  E7.1 + E7.3 (break) ..... rounds 20  hits  7  wants_fire 257  in_band 10798  crashes 3
  SHIPPED (E7.1+2+3) ...... rounds 13  hits  6  wants_fire 154  in_band 11125  crashes 3

And the dial sweep across the deadband lattice: dead45/120, dead60/150 and
dead70/100 all collapse to 3 rounds / 0 hits; ace65/mid60 gives 5 / 0. EVERY
E7.1 table is worse than no repeal at all, and the deadband — built to cure
exactly this — does not recover it.

THE ATTRIBUTION IS IN THE COLUMNS, not in an opinion: `in_band` is essentially
unchanged (10940 -> 10798) while `wants_fire` collapses 522 -> 257 -> 45. The
bandits arrive just as close for just as long. More bank does not stop them
GETTING there; it stops them POINTING once they are.

★ AND E7.1+E7.3 IS BIT-IDENTICAL TO E7.1 ALONE (20/7/257/10798 in both), which
is a finding about the INSTRUMENT: P-A's scripted player never fires, so
`combat::combat_tick` never runs in it and no drone is ever hit. E7.3 IS INERT
IN P-A BY CONSTRUCTION. The shipped table was therefore chosen on an instrument
that could not see one of the rung's three behaviours. E7.3's acceptance belongs
on `under_the_gun` (which now works), not on P-A.

### E7.C BUT THE REPEAL ITSELF IS REAL WHERE IT IS DELIVERED
G-honesty probe, PITVIPER through the real plant, everything ACHIEVED:
  * 100 deg off the nose (past ace_bank_track_hi, full tier authority):
    achieved bank 75.8 deg, n 5.41, V 109.8, NET TURN 27.8 deg/s — inside the
    25-40 deg/s LOS band this rung's own attribution says a gun solution needs,
    and plant-honest (g*sqrt(n^2-1)/V predicts 27.2).
  * 30 deg off the nose (inside the deadband, both arms handed the ruled 55):
    NET TURN 2.4 deg/s.
So the aeroplane can fly the repeal. What it cannot do is fly it while tracking.

### E7.D THE G-HONESTY PROBE WAS ASSERTING AGAINST A CAP THE MACHINE NEVER HANDS OUT
The probe predated the tracking deadband and demanded the flat 72 at all four
bearings; inside track_lo the ramp deliberately hands out 55. Rewritten to
measure the plant against `cap_handed_deg` — the cap the machine actually gave
this pilot at this bearing, mirrored from tick()'s own geometry — with the
payoff clause gated on FULL delivery. That gating is a measurement, not a hedge:
at 30 deg the ramp leaks 1.3 deg past the ruled cap and the ace measurably turns
SLOWER for it (2.21 vs 2.39 deg/s), because at that bearing the turn is
porpoising and 1.3 deg of bank moves the thrash more than it moves the turn.
The draft's "the raised bank reduces the porpoise" claim was WITHDRAWN: it was
measured at 30 deg, where the deadband hands BOTH arms the same 55, so it was
the same aeroplane flown twice (14.07 vs 14.21 deg — a null).

### E7.E ★★ THE ACTUAL WALL IS A PITCH LIMIT CYCLE, NOT THE BANK CAP
In a STEADY pinned-bearing turn at the ruled 55 deg with a level gamma command,
the airframe porpoises and burns G on nothing. Swept on the PRE-E7 table
(nothing here is entangled with the repeal), PITVIPER, 30 deg bearing:

  pursue_pitch_gain 2.2 (SHIPPED) : gamma swing 14.21 deg  n 5.51  turn 2.39  V 148.7
  pursue_pitch_gain 1.8 .......... gamma swing 12.68 deg  n 3.48  turn 2.10  V 150.3
  pursue_pitch_gain 1.4 .......... gamma swing  3.19 deg  n 1.22  turn 2.84  V 152.8
  pursue_pitch_gain 1.0 .......... gamma swing  0.25 deg  n 1.13  turn 2.62  V 153.4
  pursue_pitch_gain 0.7 .......... gamma swing  0.10 deg  n 1.13  turn 2.62  V 153.9

Holding a 55 deg turn needs n = 1/cos(55) = 1.74. The shipped gain pulls 5.51 —
roughly 4 G going into pitch thrash instead of turn — and swings the flight path
14 deg peak-to-peak while doing it. The cycle COLLAPSES between gain 1.8 and
1.4, and on the far side the net turn rate is HIGHER (2.84 vs 2.39) and the
aeroplane keeps 4 m/s more speed. At 10 deg and 50 deg bearing the same cliff is
present and milder (0.79 -> 0.12 deg; 3.22 -> 0.39 deg).

THIS IS ONE MECHANISM BEHIND BOTH OF CHAD'S COMPLAINTS. A nose thrashing 14 deg
vertically cannot settle for a gun solution (that is `wants_fire`), and it is
also, seen from the cockpit, "elevator dipping up and down" — arriving here from
a fixture that has nothing to do with evasion at all.

★ SAFETY NOTE FOR THE BUILD: `pursue_pitch_gain` is ALSO the terrain-avoidance
pull-up gain (drone.h ~2116) and the raid/defend/leash errand gain. Lowering it
globally would slow every pull-up — a CFIT risk. The correct shape is a SEPARATE
tracking pitch gain consumed at exactly the engaged pursue/BFM seam, the same
scoping discipline `engaged_bank_cap` already uses for the bank repeal.

### E7.F THE PITCH-GAIN SWEEP ON P-A — AND THE INSTRUMENT PROBLEM IT EXPOSED
P-A ensemble, 8 x 10 min per arm. `pursue_pitch_gain` lowered GLOBALLY in these
arms (the scoped E7.4 dial was built after this sweep; see the safety note):

  E6 baseline (pitch 2.2) . rounds 39  hits 18  wants_fire 522  in_band 10940  crashes 0
  E6 pitch 1.8 ............ rounds 16  hits  0  wants_fire 218  in_band  8703  crashes 1
  E6 pitch 1.4 ............ rounds 17  hits  6  wants_fire 229  in_band 10074  crashes 0
  E6 pitch 1.0 ............ rounds  6  hits  0  wants_fire  77  in_band 10209  crashes 1
  E6 pitch 0.7 ............ rounds 11  hits  1  wants_fire 151  in_band 12061  crashes 0
  E7.1 + pitch 1.4 ........ rounds 29  hits  3  wants_fire 415  in_band 11184  crashes 1
  E7.1 + pitch 1.0 ........ rounds 14  hits  0  wants_fire 182  in_band 11555  crashes 2
  E7.1 FLAT + pitch 1.4 ... rounds  5  hits  0  wants_fire  72  in_band 10754  crashes 2

TWO READINGS, and the second outranks the first:
1. Killing the limit cycle DOES recover most of what the bank repeal costs:
   E7.1 alone is 20 rounds / 257 pointing ticks, E7.1 + pitch 1.4 is 29 / 415.
   That is the largest single recovery any arm on this ladder has produced.
2. ★★ BUT THE E6 COLUMN IS NOT A RESPONSE CURVE. 39 -> 16 -> 17 -> 6 -> 11 as
   the gain steps monotonically down. Nothing about an aeroplane behaves like
   that. The ensemble is FULLY DETERMINISTIC (E6 reads 39/18/522/10940
   identically in three independent runs), so this is not run-to-run randomness
   — it is SENSITIVITY: the metric is chaotic in parameter space. A furball is a
   chaotic system and `rounds_at_player` is a rare-event count over it.

CONSEQUENCE, and it is the most important finding of this session: NO TABLE
MEASURED ON P-A CAN CARRY A RULING UNTIL THE METRIC'S RESOLUTION IS KNOWN. That
applies to E7.1's refutation (E7.B), to E7.4's recovery above, and to the E6.3
posture numbers this ladder already shipped on. The instrument for it is the
NOISE-FLOOR PROBE: arms that cannot possibly fight differently (the engaged cap
raised by half a degree to 55.5/56/58/60, flat) whose SPREAD is the metric's
floor. Credit for identifying this belongs to the parallel Fable builder agent;
its probe was destroyed in the two-writer collision below and has been rebuilt.

### E7.G PROCESS FAILURE THIS SESSION — TWO WRITERS ON ONE WORKTREE
A background "Build E7 killers rung" agent launched by the previous session was
STILL RUNNING in this worktree when this session picked the rung up, and neither
writer knew about the other for ~75 minutes. Consequences, recorded honestly:
  * this session's rewrite of the scratch sweep in test_killchain_probe.cpp
    TRUNCATED the other agent's noise-floor probe before it was ever run (no
    measurement was lost — it had not run — but the file was);
  * two `cmake --build` runs collided as `ld.exe: cannot open output file
    seads_tests.exe: Permission denied`, and one of those silently ran a STALE
    binary whose result was briefly believed (caught by an mtime check);
  * the E7.3 took_fire booking at the AI-vs-AI seam was written twice.
THE RULE THAT WAS MISSING: before writing a worktree, CHECK FOR LIVE AGENTS ON
IT. The house already forbids two ctest gates on one build dir; the same reason
forbids two writers on one worktree, and the failure mode is quieter.

### E7.H THE BUILD THIS SESSION LANDED — E7.4, SCOPED
`DroneParams::pursue_track_pitch_gain` (0 = off = bit-identical): the ENGAGED
tracking pitch gain, resolved once in tick() and consumed at exactly the two
engaged pursue/BFM seams. Every errand path keeps `pursue_pitch_gain` — the
terrain-avoidance PULL-UP runs on that dial (drone.h, the avoid branch), so a
global reduction is a CFIT risk and the sweep arms above (which DID lower it
globally) are not a shippable form. Loader-checked into [0, pursue_pitch_gain]:
this dial exists to repeal a measured over-gain, never to raise pitch authority
by a second unruled route. Ships at 0.0 pending the noise floor and Chad's call.

### E7.I ★★★ THE ACCEPTANCE INSTRUMENT WAS UNDER-RESOLVED — AND "39 ROUNDS" IS A
### LUCKY ENSEMBLE
THE NOISE FLOOR. Arms whose engaged bank cap differs by HALF A DEGREE (55.5,
56, 58, 60 flat, against the ruled 55) cannot fight differently by any physical
argument. Measured on P-A at the shipped kEnsemble 8:

  cap 55 (the ruled arm) . rounds 39  hits 18  wants_fire 522  in_band 10940
  cap 55.5 ............... rounds 20  hits  1  wants_fire 278  in_band 10671
  cap 56.0 ............... rounds 10  hits  0  wants_fire 132  in_band  9830
  cap 58.0 ............... rounds  4  hits  0  wants_fire  60  in_band  9844
  cap 60.0 ............... rounds 17  hits  0  wants_fire 240  in_band 12132

The spread across arms that CANNOT differ is 4-39 rounds and 0-18 hits. That
band contains every number this ladder has ruled on — E7.1's refutation, E7.2's
cost, E7.4's recovery, and the E6.3 posture table (39/18 vs 31/12 vs 24/9)
before them. `in_band` is comparatively stable (9830-12132, ~+/-10%) and is the
one column that was safe to reason from; `rounds`/`hits` were not.

THE CAUSE IS THE ENSEMBLE, NOT THE METRIC — and it is repairable. kEnsemble is
8, and those 8 engagements are 8 PHASE ROTATIONS OF ONE SCRIPT, so the average
is taken over a single family of initial conditions in a chaotic system.
`fly_arm` now takes the ensemble size explicitly (default kEnsemble, so every
existing caller is byte-unchanged) and the same 55-vs-55.5 pair was re-run as
the ensemble grows. ROUNDS PER ENGAGEMENT:

  n=8  : 55 = 4.875   55.5 = 2.500   (raw 39 vs 20;  hits 18 vs  1)  95% apart
  n=16 : 55 = 2.000   55.5 = 2.000   (raw 32 vs 32;  hits  7 vs  6)  identical
  n=32 : 55 = 2.156   55.5 = 2.719   (raw 69 vs 87;  hits 14 vs 21)  21% apart
  n=64 : 55 = 1.938   55.5 = 2.281   (raw 124 vs 146; hits 23 vs 28) 18% apart

★★ THE E6 BASELINE IS AN OUTLIER. At n=8 the ruled arm reads 4.875 rounds per
engagement; at every larger ensemble it settles to 1.94-2.16. The "39 rounds /
18 hits" that this ladder has treated as the number to beat is roughly 2.5x the
converged value, and it is the single luckiest cell in the sweep. Every E7 arm
was being compared against it.

WHAT THIS BUYS AND WHAT IT DOES NOT: at n=64 the instrument resolves a genuine
2x effect and cannot resolve 20%. Acceptance claims of the form "meaningfully
more rounds on the ace" are therefore answerable at n=64; claims of the form
"this table is 15% better" are not answerable at any ensemble this program can
afford, and should not be written into an acceptance again.

OWED: the E7 decision table re-taken at n=64 (running), and a ruling on whether
kEnsemble rises permanently — that changes the baselines of the P-A tests
already pinned, so it is a deliberate re-pin with Fable adjudicating, never a
silent one.

### E7.J THE CONVERGED DECISION TABLE (n=64) — AND THE REFUTATION IS WITHDRAWN
P-A re-taken at the ensemble that resolves a real 2x effect. RATES ARE PER
ENGAGEMENT, so arms are comparable:

  arm                            rounds/eng  hits/eng  wants_fire/eng  in_band/eng  crashes
  E6 (all E7 off) .............. 1.938       0.359     25.95           1313         7
  E7.1 (tiered bank) ........... 1.469       0.391     19.33           1258         4
  E7.1+E7.2 (slashers) ......... 0.750       0.328      9.78           1133         7
  E7.1+E7.3 (break) ............ 1.469       0.391     19.33           1258         4
  E7.4 alone (track pitch 1.4) . 1.469       0.438     20.27           1235         7
  SHIPPED (E7.1+2+3) ........... 0.750       0.328      9.78           1133         7
  SHIPPED + E7.4 ............... 0.953       0.172     12.89           1238         4

★★ THE E7.B REFUTATION OF E7.1 IS WITHDRAWN. It was measured on the n=8
instrument and does not survive the converged one. HITS PER ENGAGEMENT across
E6 / E7.1 / E7.1+E7.2 / E7.4 are 0.359 / 0.391 / 0.328 / 0.438 — a 20% band,
i.e. FLAT at this instrument's resolution. The bank repeal does not cost
lethality, and it HALVES THE CRASHES (7 -> 4). Chad's named repeal survives
measurement; the "39/18 -> 13/6 catastrophe" was the lucky-ensemble artefact
described in E7.I.

WHAT THE CONVERGED TABLE *CAN* RESOLVE (2x or better):
 1. E7.2 CHANGES CHARACTER, NOT LETHALITY. Rounds fall 2.6x (1.94 -> 0.75) while
    hits hold (0.359 -> 0.328). The slashers shoot far less and connect the
    same: fewer, better firing passes instead of spray. That is exactly what an
    energy-attack doctrine is supposed to do, and whether it READS as "killers"
    is a question for Chad's eye, not for this statistic.
 2. DO NOT STACK E7.4 ON THE FULL TABLE. SHIPPED+E7.4 is the one clearly bad
    cell: hits 0.172, a resolvable ~2x loss against every other arm. E7.4 ships
    at 0.0.
 3. E7.4 ALONE HAS THE BEST HIT RATE (0.438, +22% on E6 with 24% fewer rounds
    fired). Below resolution on its own, but it is the only arm that improves
    the goal metric at all, and it is the arm the deterministic plant
    measurement predicted. IT DESERVES ITS OWN RUNG, not a bolt-on here.

### E7.K THE LAST DIAL — THE TRACKING DEADBAND STAYS
The deadband was built to cure the P-A collapse that E7.I has since shown to be
an artefact, so its justification needed re-taking. At n=64:

  SHIPPED (deadband 30/90) . rounds/eng 0.750  hits 0.328  wants_fire 9.78  in_band 1133  crashes  7
  FLAT cap (as ruled) ...... rounds/eng 0.688  hits 0.297  wants_fire 9.19  in_band 1194  crashes 12

Rounds/hits/pointing differ by 8-10% — BELOW RESOLUTION, so on P-A the two are
indistinguishable and no performance claim may be made either way. The one thing
that stands out is CRASHES: 12 against 7, where every other arm in the converged
table sits at 4-7. The flat cap flies aces into the ground. That is directional
rather than proven (12 is ~1.9 sigma off 7), but since flat buys nothing
measurable, the deadband is kept on the low-risk argument alone. RECORDED so it
is not mistaken for a performance ruling. Walk-back remains one line:
ace_bank_track_hi_deg <= ace_bank_track_lo_deg gives the flat cap Chad's ruling
literally names.

### E7.L RE-PINS TAKEN THIS SESSION — ALL DELIBERATE, NONE SILENT
1. `probe P-A: the RUNG E7 decision table` — the acceptance
   `shipped.rounds > e6.rounds` is WITHDRAWN as unanswerable at kEnsemble 8 (see
   E7.I). The leg keeps the table as an attribution printout and now asserts
   only what n=8 carries: the off-arm is live, the shipped arm still hits, and
   in_band has not halved. It ALSO now pins the structural finding that the
   E7.1+E7.3 arm is bit-identical to E7.1 — E7.3 is inert in P-A because that
   fixture's player never fires — so nobody re-reads that column as evidence.
2. `probe P-A: the ace gets shot at` — the `10 x pre.rounds_at_player` bar is
   WITHDRAWN (a factor-of-ten claim resting on a PRE count of 2 rare events,
   inside a noise band of 4-39 for identical aeroplanes). Replaced by an
   absolute floor of 5 plus the resolvable in_band clause already in the leg.
   `post.player_hits > pre.player_hits` demoted to an INFO for the same reason.
3. `probe P-H: the ace duel` — the spec's `best_track_s >= 2.0` is NOT MET
   (~0.5 s) and is recorded as an OPEN RESIDUAL FOR CHAD rather than re-pinned
   away. ATTRIBUTION: the ace is foe for only ~24% of the run and spends ~78% of
   that in Intercept — it cannot CLOSE on a 275 m/s target. That is an E1.1
   closure residual upstream of everything E7 touches; no bank cap, doctrine or
   break can hold a solution on a man you never arrive behind. ITS OWN RUNG.
4. `E7.1 G honesty` — rewritten to measure the plant against the cap the machine
   actually hands out (E7.D), and its porpoise claim replaced by the wall/repeal
   pair that is actually true (E7.E).

### E7.M THE TABLE THIS SESSION SHIPS
UNCHANGED from Chad's ruled table — the measurements did not move it:
  ace_bank_cap_deg 72 / mid_bank_cap_deg 65 / deadband 30/90
  slash_doctrine true / bfm_perch_height_m 700
  bfm_defensive_range_m 1400
  pursue_track_pitch_gain 0.0   (NEW this session, shipped OFF = bit-identical)
Walk-backs, one line each: ace_bank_cap_deg 0 (the repeal), slash_doctrine false
(the slashers), bfm_defensive_range_m 0 (the break), and the deadband as E7.K.

### E7.N ★★★ E7.2 IS HELD OFF — THE SLASHERS HAVE NEVER FIRED A ROUND
FOUND BY THE FULL GATE, which is the first thing this session ran that put a
slasher through the REAL fire pipeline. `probe P-D: the transiting striker
fights back` — a SIGNED E3 rung — fails with `post.rounds_at_player = 0`. The
pilot it picks off the roster rule is CANARY (aggression 0.55, base tier), i.e.
exactly the tier the doctrine selector converts into a slasher. Cause isolated
by config alone, nothing else touched: `slash_doctrine = false` -> P-D passes
18/18; `true` -> 0 rounds. min_range also moves 118 m -> 282 m: he stands off.

★ THE COVERAGE GAP THAT LET IT THROUGH, recorded because it is the lesson: every
E7.2 test asserts the guns_hot MODE FLAG and nothing else — "hot in Slash, cold
in Perch and Defensive" (test_enemy_ai_e7.cpp, "the slash is the doctrines only
guns hot mode" calls bfm_step and reads `.guns_hot`). NOT ONE OF THEM COUNTS A
ROUND. The doctrine was proven to INTEND to shoot and never proven to shoot.
That is the house's own "VERIFY THE ARTEFACT, NOT THE PROCESS" trap arriving
again: every generator number perfect, and the aeroplane does not fire.

THE LIKELY MECHANISM, stated as a hypothesis because it is NOT yet measured: the
fire gate requires `coordinated` — the bandit flying down its own nose within
fire_align_cos — and a steep slashing dive is precisely where the flight path
lags the nose. The doctrine's whole geometry (dive through from 700 m above at
the raised chase ceiling) may be self-defeating against that gate. The converged
n=64 table is consistent with it from the other side: E7.2 cuts rounds 2.6x
(1.94 -> 0.75 per engagement) while hits hold — which reads as "fewer, better
passes" until P-D shows a hard zero in a fixture with a real pipeline.

DISPOSITION: `slash_doctrine = false` in the shipped table, with the rationale
written at the dial. THIS IS NOT AN OVERRIDE OF CHAD'S "BOTH DOCTRINES APPROVED"
RULING — it is a BLOCK ON A DEFECT. The doctrine ships the moment a slasher is
measured putting rounds downrange. OWED FOR THAT RUNG:
  (a) an E7.2 leg that counts REAL ROUNDS out of the real fire gate, not a mode
      flag — the acceptance this rung should have had;
  (b) measure `coordinated` and the fire cone THROUGH a slash, and fix whichever
      of the two the dive violates;
  (c) re-run P-D with the doctrine on; it is the regression sentinel.

### E7.O P-H's COMPARATIVE CLAIMS WITHDRAWN — n=1 CANNOT CARRY THEM
Same finding as E7.I, applied to a second fixture. P-H is ONE 10-minute duel.
Its history proves the point by itself: at the FLAT cap it read ON 4 rounds / 2
hits vs OFF 2 / 0 and its `on > off` clauses PASSED; at the shipped deadband it
reads ON 0 / 0 vs OFF 2 / 0 and they FAIL. Same rung, same code, opposite
verdict. The clauses now record the comparison and assert only the structural
facts n=1 can carry (the duel happened, the ace flew past 55 deg of bank, it was
not paid for in wrecks). The rung's quantitative ruling is the n=64 P-A table.

## RUNG E8 — THE ENERGY FIGHT (Chad's ruling 2026-08-21, after tape 100)
> "Ai ally's are pretty good at pump destruction, so are the enemies on
> surface, I have yet to see them successful at shutting down the pump in the
> tunnel. Need to give enemies more energy fighting abilities as that is how I
> am able to beat them. I almost dies to them last match though"

### E8.0 ★★★ THE ATTRIBUTION — AND IT REFUTES THE HANDOFF'S OWN FRAMING
From `conquest_tape_100.jsonl` (49:00, signature VERIFIED, his own flight).
The 2026-08-21 handoff named rung E1.1 "closure" as the next rung on the
premise that the ace "cannot CLOSE on a 275 m/s target". **THE TAPE REFUTES
THAT.** Over 581 s of an enemy holding him as foe:

    range < 2200 m (attack range) : 294 s   (50.6% of foe time)
    range <  900 m (fire band)    : 200 s   (34.4%)
    closest approach              : 14 m, 21 m, 34 m

They close FINE. What they cannot do is FIGHT once there:

    drone speed inside 900 m, median :  124 m/s
    player speed, same samples       :  202 m/s
    drone faster than the player     :  7.2% of samples
    nose-on-player (vel . LOS), med  : -0.86  (149 deg OFF him)
    nose within 30 deg of him        :  3% of in-close samples
    bfm mode inside 900 m            :  Intercept 51%, Offensive 25%
    Perch / Slash over the whole tape:  0% (the doctrine ships OFF)

They are ENERGY-DEAD, at 60% of his speed, pointed away. He flies rings around
them. That is his felt report, measured. It is also why only **5 rounds** were
aimed at him in 49 minutes — and those 5, from `i=4` at 897->477 m, took him
82.8 -> 17.0 hp in 1.3 s and killed his engine and left wing; he died on the
ground at 15:13 (`pd cause=ground`, eng 0.00 / wl 0.00). **The kill chain works
when it fires. It fires once per 49 minutes.**

★ THE MECHANISM, found in code, not inferred: every ENGAGED fight mode commands
off `dl.speed` — the PATROL cruise, `[combat] speed = 85`. Chad ruled that
number down 140->115->95->85 on 2026-07-14 for a different question entirely
("a couple flew away and would take long to catch"). It silently became the
speed they dogfight at. `bfm_intercept_speed_mps = 265` does not help: it is
armed only while `range > attack_range_m`, i.e. it switches OFF exactly where
the fight is. Offensive, Yoyo and Defensive never had a raised ceiling at all,
and Extend's is behind the same range gate — so the ENERGY REBUILD mode
rebuilds to 85 m/s. E1.1's own comment ("an extend that cannot out-run the
target it is extending FROM is not an energy rebuild, it is a slow death")
argued this and then gated the fix behind the condition that is false whenever
an Extend begins.

### E8.1 ★★★ THE E7.2 DEFECT — MEASURED, AND THE SPEC'S HYPOTHESIS IS REFUTED
E7.N owed this rung items (a) a leg counting REAL rounds and (b) a measurement
of `coordinated` and the fire cone THROUGH a slash. Both are now paid, by the
new **fire-gate witness** (`PursueCmd::w_*` / `DroneState::w_*` — a read-only
per-tick report written by the shipped gate expressions themselves, never a
re-derivation) and the new **probe P-S** (`test_stope_probe.cpp`), which runs
the P-D fixture with the doctrine on and censuses which clause is open.

    DOCTRINE ON : rounds=0  coordinated_open=100%  cone_open=1.1%  ALL_open=0%
    DOCTRINE OFF: rounds=17 coordinated_open=100%  cone_open=7.2%  ALL_open=3.7%
    IN SLASH    : cone_open = 0 of 950 gate ticks, best cone cos = -0.106

**`coordinated` NEVER VETOES ANYTHING — it measures 100% OPEN in both arms.**
The E7.N hypothesis is refuted. The real defect is the POINTING GEOMETRY: over
a whole slashing pass the nose never comes within **96 degrees** of the
ballistic lead point.

WHY, and it is arithmetic: the perch sat `perch_height_m` = 700 m above the
TRACKING lag point, `lag_dist_m` = 220 m behind him — essentially DIRECTLY
OVERHEAD. Diving from there needs `atan(700/220)` = **72.6 deg** of nose-down
against `pursue_max_gamma_deg` = **30**. The slasher cannot point at the target
it perched over. Perch's own comment already warned "arriving directly overhead
hands him a vertical reversal"; the DIALS never implemented it.

WHAT WAS SWEPT, and the honest result: `perch_lag_m` {0,1600,1700,1900,2000} x
`slash_time_s` {5,12,16} x `perch_release_frac` {0.45,0.10,0.05} moved the best
slash cone cos only -0.106 -> -0.033 and produced ZERO rounds in every arm. The
binding constraint is the dive envelope, and it is monotone in it:

    pursue_max_gamma  30 deg -> best slash cone cos -0.033  (92 deg off)
                      45 deg ->                     +0.221  (77 deg off)
                      60 deg ->                     +0.480  (61 deg off)

★ RULING TO TAKE: **even at DOUBLE the dive envelope the slash never gets
within 60 degrees of a firing solution.**
★★★ **THAT RULING IS REFUTED — see §E9.6.** On the SHIPPED probe at exactly
double the envelope the slasher reaches cone cos +0.797 (37 deg off) and puts
**2 rounds** downrange; with `bfm_perch_lag_m` 1200 it reaches +0.918 (23 deg
off). "ZERO rounds in every arm" is false on the artefact. The *conclusion*
(perch-and-dive is not shippable) survives; the mechanism and both headline
numbers do not. A diving attacker's depression angle
to its target GROWS as the pass closes (you close horizontally faster than you
descend), so a clamped dive can never converge onto a man below you. E7.2's
perch-and-dive doctrine is NOT FLYABLE in this airframe's envelope. It stays
`slash_doctrine = false` — but the disposition changes from HELD ON AN
UNEXPLAINED ZERO to **REFUTED ON MEASUREMENT**, which is a ruling Chad can act
on. `pursue_max_gamma` is NOT proposed as the fix: it is shared with the
terrain pull-up (the E7.4 CFIT warning applies verbatim).

WHAT SHIPPED FROM E8.1 ANYWAY, because the geometry was genuinely wrong:
  * `bfm_perch_lag_m` (dial, 0 = fall back to `lag_dist_m` = pre-E8 bit-for-bit)
    — the slash perch's OWN standoff, 1700 m.
  * ★ **the dive-envelope LOADER TRIPWIRE** — `atan(height/lag) <= 0.8 *
    pursue_max_gamma`, with the geometry in the failure message. THIS IS THE
    CHECK WHOSE ABSENCE LET AN UN-DIVEABLE PERCH SHIP GREEN, and it is the
    durable half of this item.

★ THE COVERAGE LESSON, restated because it recurred at full strength: every
E7.2 test asserted the `guns_hot` MODE FLAG and not one counted a round. The
doctrine was proven to INTEND to shoot and never proven to shoot. The new legs
name this trap explicitly and every E8 differential carries a NON-VACUITY
clause beside it.

### E8.2 THE ENGAGED FIGHT SPEED — WHAT SHIPPED
`bfm_fight_speed_mps` (dial, `<= 0` = the patrol cruise, structurally
bit-identical: `max(dl.speed, 0)` IS `dl.speed`). It replaces `dl.speed` as the
BASE that Offensive / Yoyo / Extend / Defensive command off. Every bump on top
is the same align^2-faded expression, so **the ruled corner-speed law is
untouched** — only the floor under it moves. Intercept is deliberately EXCLUDED
(it has E1.1's own target-keyed ceiling and is not a fight).

NAMED SCOPE, so the 2026-07-14 catchability ruling is not silently repealed:
patrol, maverick and raid steering never come through `bfm_step`, so no value
of this dial can reach them — a bandit RUNNING AWAY is exactly as catchable as
Chad ruled it. This is the E7.1 precedent verbatim (a ruled cap raised ONLY in
the engaged path). Walk-back is one line: `bfm_fight_speed_mps = 0`.

Loader: `> combat speed` (a value below the cruise is a silent no-op — fail
loud) and `<= 1.25 * v_redline`.

THE VALUE IS PICKED BY MEASUREMENT, not by choice: `probe P-A sweep: the
engaged fight speed` (hidden `[.e8sweep]`, n=32 x 10 min per arm) sweeps
{0, 120, 150, 180, 210} and reports RATES PER ENGAGEMENT, per the E7.I noise
law — n=64 resolves a genuine 2x effect and CANNOT resolve 20%, so no
acceptance here may be written as "better by X%".

### E8.3 THE TUNNEL PUMP — ATTRIBUTION ONLY, NOT BUILT
Chad: "I have yet to see them successful at shutting down the pump in the
tunnel." Tape 100 says he is right and says why. Pump 2 (the deep fac=0 pump)
ended the 49 minutes at **19620.0 hp — untouched**. Enemy strike windows on it
report `on_station = 0.0s` with closest approaches of 5633 / 6574 / 10805 /
13975 m. Of 18 tunnel entries in the whole tape, **17 were ALLIES**; the single
enemy entry (`i=3`, 38:56) ended in a terrain crash inside the net at 40:17.
Eight of the 18 entries ended in an in-net `dc` crash. Meanwhile the allies
killed BOTH enemy pumps, including the deep one (pump 3, 11:20) — so the
machinery works and it is the ENEMY side that never arrives.

Two candidate mechanisms, NEITHER measured yet (do not build on these):
  (a) the two enemy pilots carrying a deep-strike order (`i=0`,`i=1`) also
      carry `raid = 100%`, and E2.1's on-order collapse is for NON-raid pilots
      only — so the enemies ordered to the deep pump are structurally the ones
      barred from launching;
  (b) the in-net crash rate (8/18) may make the run unsurvivable regardless.

★ ALSO WORTH A RULING: the match was DECIDED at 12:14 and the remaining 36:45
were empty — score frozen 270-0, no enemy raid windows after 12:39, planes 8
-> 7 with no further reinforcement. E5's waves fired 3 times, all before 09:44.

### E8.4 ★★★ THE CONVERGED RULING — THE FIGHT-SPEED EFFECT IS NOT REAL,
### AND THE WALL ON BOTH ROADS IS POINTING
`probe P-A sweep: the engaged fight speed`, n=64 x 10 min per arm, RATES PER
ENGAGEMENT (the E7.I law: n=64 resolves a genuine 2x effect and CANNOT resolve
20%):

    fight_speed      0      120      150      180      210
    rounds/eng     1.47     1.80     1.52     1.67     1.55
    hits/eng       0.39     0.47     0.25     0.42     0.36
    wants_fire/eng 19.3     24.3     20.3     22.2     20.2
    in_band/eng    1258     1223     1334     1440     1463
    foe_min/eng    9.63     9.79     9.91     9.87    10.05
    crashes/eng    0.063    0.063    0.078    0.094    0.156

**THE EFFECT DOES NOT SURVIVE CONVERGENCE.** The entire spread on rounds and
hits is ~1.2x with no monotone shape — inside the band the law forbids ruling
on. NO ACCEPTANCE MAY CLAIM AN IMPROVEMENT, and none does: the E8.2 legs assert
only the structural off-identity and non-vacuity.

★ THE PROCESS RECORD, because it is the E7.I trap arriving on schedule and it
was nearly written up as a win: an n=32 pass of this same sweep read

    fight_speed      0      120      150      180      210
    rounds/eng     1.88     2.41     2.66     1.81     1.66
    hits/eng       0.28     0.59     0.56     0.31     0.31

— a clean INTERIOR OPTIMUM at 120–150 with a falloff either side, which is
exactly the shape a corner-speed trade should produce, agreed across three
columns. It was NOISE. At n=64 the peak arm (150) drops to 1.52 rounds and the
WORST hits column of all five (0.250 against 0.391 for the off arm). ★ A
plausible MECHANISM STORY that predicts the shape you observe is not evidence
that the shape is real — the noise floor is, and this instrument's floor eats a
1.2x. Doubling the ensemble was the whole difference between a shipped ruling
and a retracted one.

★ AND THE FINDING THAT DID SURVIVE, from the two STABLE columns: `in_band`
rises +16% and `foe_min` +4% monotonically with fight speed.
★★★ **NEITHER HALF OF THAT SENTENCE IS TRUE — see §E9.6.** On the table printed
directly above, `in_band` goes 1258 -> **1223** at the shipped arm (a DECREASE,
and the lowest of the five) and `foe_min` dips at 180: NOT monotone. An n=64
null floor on arms that cannot fight differently spans in_band +8.9% and
foe_min +1.7%, so most of the "+16%" is reproducible by half a degree of bank
cap. The two are also one observable, not two. **Faster fighters
buy PRESENCE and convert none of it into rounds.** Put beside E8.1 (a slash
pass that never gets within 96 deg of the lead point, and never within 60 deg
even at double the dive envelope) and beside Chad's tape (nose a median 149 deg
off him while inside 900 m for 200 s), the conclusion is one thing said three
ways:

  ★★★ **THE WALL IS POINTING, NOT ENERGY.** Every road this rung drove — raise
  the fight speed, fix the perch geometry, lengthen the pass, double the dive
  envelope — moved POSITION and left the GUN SOLUTION untouched. The next rung
  should attack the tracking law itself (E7.4's pitch limit cycle is the
  standing candidate: a nose thrashing 14 deg vertically cannot hold a
  solution, and it is BUILT and shipping at 0.0), not another dial that gets
  them closer to a man they cannot point at.

DISPOSITION: `bfm_fight_speed_mps = 120` ships FOR CHAD'S STICK, not on the
numbers — top arm on the hits column at BOTH ensembles, crash rate unchanged
(0.063, vs 0.156 at 210), and it answers his ruling. The harness cannot resolve
feel and never outranks the stick. Walk-back is one line: `0`.

## RUNG E9 — THE TRACKING LAW (E7.4, the pitch limit cycle)
Chad has not ruled a new ask here; this rung is the one **E8 itself named**, on
its own measurement: "★★★ THE WALL IS POINTING, NOT ENERGY. Every road this rung
drove — raise the fight speed, fix the perch geometry, lengthen the pass, double
the dive envelope — moved POSITION and left the GUN SOLUTION untouched." The
standing candidate was E7.4, the scoped tracking pitch gain, BUILT in the E7
session and shipped at 0.0 pending a value.

### E9.0 ★★ WHAT THE PREVIOUS RUNG LEFT — A DIAL WITH NO TESTS AT ALL
`pursue_track_pitch_gain` shipped with a loader bound and **not one test**: no
off-identity leg, no scoping leg, no plant leg. The CFIT safety claim its whole
scoped form exists for — "the terrain-avoidance PULL-UP and the errands keep
`pursue_pitch_gain`" — was a COMMENT. And the sweep that motivated it (E7.F)
lowered the gain GLOBALLY, so nothing had ever measured the dial that actually
ships. That gap is paid first, before any value is picked.

### E9.1 ★★★ THE FINE PLANT SWEEP — AND THE RULER IS THE GUN'S OWN CONE
`E7.4 plant sweep: the tracking pitch gain cliff` (hidden `[.e74plant]`), the
saturated turn: ONE aeroplane, ONE saturated command, no furball. This is
deliberate — spec E7.I's noise law kills any P-A claim under 2x, and a 14-deg
limit cycle collapsing to 0.2 is a 60x effect that needs no ensemble at all.
**THE VALUE IS PICKED ON THE PLANT; the furball is asked only what it costs.**

E7.E knew the cliff was "somewhere between 1.8 and 1.4". It is now resolved.
PITVIPER (ace tier, index 5), 30 deg bearing — the TRACKING regime, where the
E7.1 deadband hands both tiers the ruled 55:

    track_gain   2.2(off)  2.0    1.8    1.7    1.6    1.5    1.4    1.2   1.0
    swing (deg)   14.07  13.54  12.42  10.61   7.38   4.20   2.86   0.75  0.22
    n             5.52    5.08   3.34   2.37   1.68   1.32   1.21   1.15  1.13
    net turn      2.21    2.83   2.03   2.27   2.71   2.87   2.85   2.69  2.65
    V (m/s)     148.72  149.21 150.37 151.15 152.04 152.53 152.83 153.16 153.42
    (base tier reads within ~2% of these at 30 deg; n needed = 1/cos(55) = 1.74)

★ **THE BAR IS `fire_cone_deg`, NOT A PREFERENCE.** The gate needs the ballistic
lead point inside 14 deg of the nose. A limit cycle that swings the flight path
by more than HALF that cone has spent a quarter of the budget in each direction
before any tracking error, lead error or LOS rate is paid. **1.6 still swings
7.38 and fails the 7.0 bar; 1.5 is the first value inside it.** The leg asserts
it config-relative, so a cone retune moves the bar with it.

★★ AND ON THE NOSE IT IS NOT A TRADE AT ALL. At 1.5 the net turn rate is at its
MAXIMUM (2.87 deg/s against 2.21 at the shipped gain, +30%) and the aeroplane
keeps 3.8 m/s more speed. **The porpoise was eating the turn it appeared to be
buying** — 4 G of the 5.5 went into pitch thrash against a turn needing 1.74.

★★ THE COST IS OFF THE NOSE, AND IT IS NOT TIER-NEUTRAL. Abeam (90 deg) there is
no cycle to cure (swing < 0.2 deg at every gain) and the over-pull is doing real
turning work:

    90 deg bearing    2.2(off)   1.8    1.6    1.5    1.4    1.0
    ace   turn deg/s    27.80  26.61  26.32  26.19  26.05  25.28
    ace   min alt (m)    2209   2500   2500   2500   2500   2500   <-- ★
    base  turn deg/s    22.03  21.14  19.05  18.03  17.07  13.70
    base  V (m/s)       91.01  91.49  92.40  92.85  93.29  95.05

★ **THE ACE'S EXTRA ABEAM TURN RATE IS BOUGHT BY DIVING 290 m** (399 m at gain
2.0), and that stops entirely at 1.8 and below — a CFIT-relevant finding nobody
had taken, and an argument FOR the dial rather than against it. The base tier's
loss is real and unrewarded: 22.0 -> 18.0 deg/s at 1.5, paid back as ~2 m/s of
retained speed. That is a genuine energy-vs-turn trade off the nose, and it is
the one thing in this rung only Chad's stick can rule.

★ THE EQUIVALENCE CHECK, because E7.E's numbers were taken with the gain lowered
GLOBALLY and the dial that ships is SCOPED: at 1.8 / 1.4 / 1.0 the scoped and
global arms are **IDENTICAL to the last bit** at this seam. E7.E transfers.

SECOND FIXTURE, second signature: `under_the_gun` (E7.3's, a scripted attacker
welded to the six) reports the BOB as a flip COUNT rather than an amplitude —
gamma flips/min 25.5 (off) -> 21.0 (1.8) -> 12.0 (1.4). The same mechanism seen
by an instrument that shares nothing with the saturated turn.

### E9.2 THE SCOPING, MEASURED BIT-FOR-BIT (the safety leg)
`E7.4: the tracking gain never touches the errand or pull-up paths` proves the
claim three ways, each `==` on the raw final state:
  (a) the UNENGAGED bandit (no foe, no player pointer): identical;
  (b) the RAID errand, commanding a real descent onto a pump 30 km out and
      2300 m below — the exact kind of gamma command a pitch gain moves:
      identical, with a non-vacuity clause that it actually descended;
  (c) ★ the FORCED PULL-UP flown by an ENGAGED bandit, started at 150 m AGL
      (below `avoid_agl_enter_m` = 250): identical, with the avoid latch
      asserted live for the whole window and the climb-away asserted.
      (c) is the one that matters: an engaged tracker IS the pilot whose
      `level_p` this rung moves, and the avoid branch must still overwrite it.

★ MUTATION-VERIFIED 4/4, because a green test proves nothing until something
that should kill it does:

    M1 the avoid branch reads the dial ......... (c) RED
    M2 the raid errand reads the dial .......... (b) RED
    M3 the off sentinel `> 0` becomes `>= 0` ... off-identity RED (2 clauses)
    M4 the dial is ignored entirely ............ non-vacuity RED

### E9.2b ★★ THREE DEFECTS THIS RUNG FOUND IN THE GATE ITSELF
Shipping a non-zero value turned four gate legs red. NOT ONE of them was fixed
by moving the dial; each was a defect the dial exposed.

1. **`pre_e7()` AND THE DECISION TABLE'S E6 ARM DID NOT ZERO E7.4.** Both build
   "rung E6 exactly" by hand, and both predate the dial. The moment it shipped
   non-zero, the OFF arm of every differential in `test_enemy_ai_e7.cpp` was
   quietly carrying the cure. It surfaced as `E7.1 G honesty` reporting a 4.79
   deg swing where THE WALL it exists to pin is 14.21. Fixed at both helpers.
2. **★★★ THE DECISION TABLE'S SLASHER ROW HAD BEEN INERT SINCE E8.** The row
   built itself with `e71_72.slash_doctrine = shipped.slash_doctrine`, and E8
   ruled that FALSE on measurement. From that commit the row was bit-identical
   to the E7.1 row, and the table printed **four identical rows under four
   different labels** — a table that reads as a decomposition and is one arm
   shown four times. It passed the whole time. The row now arms the doctrine by
   definition and carries a NON-VACUITY clause against its own baseline. The
   table also gains the E7.4-alone row it decomposes to.
   ★ THE LESSON, and it is this ladder's own signature failure in a new
   costume: an arm defined by READING THE SHIPPED TABLE stops being an arm the
   moment the shipped table turns it off, and nothing goes red when it does.
3. **A DELIBERATE RE-PIN — `probe P-F`, clause (0).** `pause.on_station_s ==
   0.0` was true for every table up to E8. With the pitch cycle damped, the
   YIELDING raider's path through the merge is smoother and it drifts through
   the on-station envelope incidentally: 0.00 -> 1.18 s of 90. The premise is
   unchanged and the ruling's margin is intact (pressing 3.02 s, 2.5x), but an
   exact zero was never the claim. Re-pinned as a fraction of the pressing arm,
   recorded here, never silent.

★ AND A PROCESS NOTE, because it is E7.G arriving again and it nearly landed a
false number: the mutation loop restored `drone.h` after its last arm but did
NOT rebuild, so the next run of the E7.4 legs executed the **M4 mutant binary**
(the dial ignored) and reported "the cure does nothing" with the config reading
1.5. Caught in one step by the impossible number, not by the harness. A stale
binary is still the quietest way to be wrong in this repo.

### E9.3 ★★★ THE FURBALL — WHAT IT COSTS, AND THE FLOOR IT MEASURED ON ITSELF
`probe P-A sweep: the tracking pitch gain` (hidden `[.e74sweep]`), n=64 x 10 min
per arm, on the CURRENT shipped table (fight_speed 120, slash_doctrine false),
RATES PER ENGAGEMENT. New this rung: the POINTING columns, read off the E8.1
fire-gate WITNESS (`DroneState::w_cone_cos`, the shipped gate's own expression
against the ballistic lead) instead of a re-derivation against the raw LOS.

    track_gain     0.0(off)  2.1999    1.7     1.5     1.4     1.0
                             (FLOOR)
    rounds/eng      1.797    1.781    1.297   1.703   1.500   1.250
    hits/eng        0.469    0.453    0.578   0.531   0.406   0.297
    wants_fire/eng 24.25    24.02    17.58   23.14   20.36   16.75
    in_band/eng   1223.2   1226.9   1384.1  1398.5  1380.2  1403.9
    foe_min/eng     9.789    9.780    9.420   9.509   9.493   9.356
    crashes/eng     0.063    0.078    0.094   0.063   0.125   0.078
    cone_open/eng  24.25    24.02    17.61   23.17   20.39   16.78
    cone_open_frac  0.0198   0.0196   0.0127  0.0166  0.0148  0.0120
    mean_cone_deg  91.10    91.20    92.52   92.12   92.87   93.20

★ CROSS-VALIDATION, free and worth having: the OFF arm reproduces the E8.2
sweep's `fight_speed = 120` cell to the last digit (1.796875 / 0.468750 /
1223.218750 / 24.250000 / 9.788676 / 0.0625). Two independently written sweeps,
one shipped table, identical numbers.

★★ THE FLOOR ARM, AND THE HONEST READING OF IT. `track_gain = 2.1999` against a
global 2.2 is a 0.005% change in a pitch gain — no aeroplane can fly that
differently. It lands within 0.9% on rounds, 3.4% on hits, 0.3% on in_band. **DO
NOT READ THAT AS "THE NOISE FLOOR IS 1%".** All it proves is that the ensemble
is DETERMINISTIC and that a perturbation that small does not decorrelate the
furball inside 10 minutes: 115 rounds vs 114, 30 hits vs 29. The floor that
matters is the spread across arms that differ by a REAL amount, and this table
measures that too — see below. Taking the 1% as the floor would be exactly the
E8.4 trap wearing a new hat.

★★★ AND THE SPREAD ACROSS THE REAL ARMS SETTLES THE RUNG: 1.297 / 1.703 / 1.500
/ 1.250 rounds and 17.6 / 23.2 / 20.4 / 16.8 pointing ticks — a ±30% scatter
with NO MONOTONE SHAPE (1.7 is worse than both 1.5 and 1.4 on every column, and
no mechanism makes a HIGHER gain worse than a lower one on the same side of the
cliff). A response curve does not exist for this dial on this fixture.

★★ **A CORRECTION TO THIS SECTION, AND IT IS MINE (rung E9, on the E8 red
team's floor measurement).** The paragraph above originally continued: "put
beside a 1% reproducibility floor, that scatter cannot be sampling noise —
`rounds_at_player` is chaotic in PARAMETER space." **THAT INFERENCE USED THE
WRONG COMPARATOR, two sentences after this section warned against exactly
that.** The red team measured the REAL n=64 null floor on this same shipped
table — four arms whose `ace_bank_cap` differs by ≤1.5 deg, which cannot fight
differently — and it is far wider than my 2.1999 arm:

    ace_bank_cap  72.0     72.5     73.0     71.5      null span
    rounds/eng    1.797    1.766    1.578    1.859     1.578-1.859
    hits/eng      0.469    0.359    0.391    0.547     0.359-0.547
    in_band/eng   1223.2   1332.2   1310.2   1299.7    +8.9%

Against THAT floor the honest reading of my table changes, and it changes in a
way that is MORE useful than the one I wrote:
  * **track_gain 1.5 sits inside the null band on every column** (rounds 1.703
    in 1.578-1.859; hits 0.531 in 0.359-0.547). Indistinguishable from off —
    which is exactly the "costs nothing resolvable" the rung asked for.
  * **track_gain 1.0 falls BELOW the null band on both** (rounds 1.250, hits
    0.297). It is the one arm the furball CAN resolve, and it resolves it as
    WORSE. Which is a real result: do not chase the cliff past its knee.
  * the parameter-chaos claim is not needed and is not made. The scatter is
    consistent with the true null floor, and only the 1.0 arm escapes it.
★ The lesson for me, recorded: I flagged the 2.1999 arm's caveat correctly and
then used the number anyway in the very next inference. **A caveat you write and
then step over is worth less than no caveat at all.**

**SO: THE FURBALL RESOLVES NOTHING ABOUT THIS DIAL, IN EITHER DIRECTION — AND
THAT IS THE ANSWER THIS RUNG NEEDED FROM IT.** The question put to it was "does
the plant-picked value COST anything resolvable (2x)". Nothing here is 2x. The
kill chain has not collapsed: at 1.5 rounds are 1.703 against 1.797 (-5%) and
**hits are 0.531 against 0.469, i.e. UP 13%** on 4x the gate's data.

★ THE POINTING COLUMNS, and the correction they force on a first reading:
`cone_open_frac` FALLS with the dial (0.0198 -> 0.0166), which looks like the
rung failing at its own thesis. It is not: the ABSOLUTE count is flat
(24.25 -> 23.17, -4%) and the fraction falls because the DENOMINATOR grew —
in_band rises 14%. Which is the E8.4 finding again, from a different dial:

  ★★★ **`in_band` +14% (E7.4) beside `in_band` +16% (E8.2's fight speed). TWO
  UNRELATED DIALS, ON DIFFERENT SEAMS, BOTH BUY PRESENCE AND NEITHER CONVERTS
  IT.** Three rungs have now driven a road that ends on the same column. At
  some point that stops being a property of the dials and starts being a
  property of the FIXTURE — whether P-A's stepped scripted player can be shot
  at all. That question is OPEN and is put to the red team by name (E9.5); it
  is not answered here and nothing in this rung depends on the answer, because
  the value was picked on the plant.

### E9.4 THE RULING, AND WHAT IT RESTS ON
    pursue_track_pitch_gain  1.5    E7.4  ON   (was 0.0, built but unmeasured)
Everything else in the shipped table is UNCHANGED from E8:
    bfm_fight_speed_mps 120 / bfm_perch_lag_m 1700 / slash_doctrine false
    ace_bank_cap_deg 72 / mid_bank_cap_deg 65 / deadband 30/90
    bfm_defensive_range_m 1400
Walk-back is ONE LINE: `pursue_track_pitch_gain = 0.0` — structurally
bit-for-bit pre-E9, proven by two independent off-forms.

WHAT THE VALUE RESTS ON, stated so nobody has to reconstruct it:
  1. A DETERMINISTIC plant measurement, not an ensemble: 14.07 deg of flight-path
     swing collapsing to 4.20, and n falling 5.52 -> 1.32 against the 1.74 the
     turn needs. A 3x effect on a fixture with no chaos in it.
  2. A bar that is the GUN'S OWN CONE (`fire_cone_deg` 14, half of it), not a
     preference — and it is what rejects 1.6 (7.38 deg, over the 7.0 bar).
  3. On the nose it costs NOTHING: net turn +30%, speed +3.8 m/s.
  4. The furball cannot resolve it either way, and nothing collapsed there.
★ AND WHAT IT DOES NOT REST ON: any P-A number. This rung claims no furball
improvement and none is asserted anywhere in the gate.

THE NAMED TRADE, which is Chad's to rule and nobody else's: OFF the nose there
is no cycle to cure and the over-gain was doing real turning work. Abeam, the
BASE tier loses 22.0 -> 18.0 deg/s (it keeps ~2 m/s more speed for it); the ace
loses 27.8 -> 26.2 and stops paying for the difference with a 290 m dive. If the
enemies now feel like they turn WIDE, that is this trade landing and the walk-back
is one line.

### E9.5 OWED BY THIS RUNG
1. ★ CHAD'S FLY — the only instrument that can rule the abeam trade. Checklist
   in the handoff.
2. The independent fresh-context RED TEAM on rung E8 was commissioned in
   parallel with this rung (its report is owed against `dcc5c53c8`), and it
   carries two questions this rung has now made sharper:
   (a) can P-A's stepped scripted player express the defect Chad felt at all —
       THREE dials on three seams have now moved `in_band` and nothing else;
   (b) E7.J's "do not stack E7.4 on the full table" was measured with
       `slash_doctrine = TRUE`; the table ships it FALSE, so that warning was
       about an arm the game no longer flies. This rung proceeded on that
       reading and the n=64 table above is the direct measurement of the
       stacked case the warning was about.
3. NOT BUILT, unchanged from E8 and still the biggest open item by Chad's own
   words: the TUNNEL PUMP (E8.3) and the EMPTY SECOND HALF (the match decided
   at 12:14 with 36:45 of empty sky).

### E9.6 ★★★ THE E8 RED-TEAM LEDGER — 19 FINDINGS, ADJUDICATED
An independent fresh-context red team was run against `dcc5c53c8` in its own
detached worktree (`D:\seads_sandboxes\enemy-ai-rt`), per the standing rule and
E8's own OPEN item 2. It built all five targets INCLUDING the game, ran the full
gate twice, ran probe P-S four times, mutated six sources in a separate build
dir, wrote two new n=64 probes and re-derived tape 100 independently in Python.

★ ITS VERDICT ON THE GATE: **CONFIRMED, 1521/1525, the four GI4 sled failures
verbatim, at the shipped 120** — which pays the E8 handoff's OPEN item 1 from an
independent tree. (Its 5th failure was its own: `test_ballistic_truth.cpp:172`
hard-codes a path through a build dir literally named `build`. P2, pre-existing.)

★★ EVERY P0 BELOW WAS REPRODUCED IN THIS TREE BEFORE IT WAS BELIEVED. A report
is not evidence; the artefact is.

**P0-1 / P0-2 — "ZERO rounds in every arm" and "even at DOUBLE the dive envelope
the slash never gets within 60 degrees" are BOTH FALSE ON THE SHIPPED PROBE.**
REPRODUCED HERE, on the shipped P-S at the E9 table:

    perch_lag 1700  max_gamma 30 : rounds  0   best slash cone cos +0.113
    perch_lag 1700  max_gamma 45 : rounds  3                       +0.243
    perch_lag 1700  max_gamma 60 : rounds 14                       +0.537 (57 deg)
    perch_lag 1200  max_gamma 60 : rounds  0                       +0.919 (23 deg)

At exactly double the envelope the slasher fires **fourteen rounds**. The E8
ruling that closed E7.2 as REFUTED ON MEASUREMENT is itself refuted on the
measurement. ★ WHAT SURVIVES: the doctrine still cannot ship, but **for a
different reason than the one written** — not "not flyable in this airframe" but
"not flyable inside a dive envelope shared with the terrain pull-up." ★★★ AND
THAT MAKES A RUNG: `pursue_max_gamma` needs exactly the treatment E7.4 just gave
`pursue_pitch_gain` — a SCOPED engaged-tracking dive envelope, leaving the
pull-up alone. E8 said "pursue_max_gamma is NOT the fix on offer"; with the
scoping pattern now built and proven, it is.

**P0-3 — E8.1's MECHANISM is refuted by the dial its sweep never varied.**
`bfm_perch_height_m` is 700 in every shipped arm. The red team added arms
lowering it (all inside the 30 deg envelope, all clearing the tripwire): making
the dive TRIVIALLY fit makes the pointing MONOTONICALLY WORSE (H=700 +0.172 ->
H=150 -0.776). So the dive envelope is not the binding constraint; the binding
quantity is PITCH AUTHORITY, because `pursue_max_gamma` clamps the whole
tracking law's `target_gamma`. ★ The shipped loader tripwire — billed as "the
durable half" — therefore guards a quantity that does not predict the outcome.
It is KEPT (it is still a real geometric sanity check and it is cheap) but its
billing is withdrawn here.

**P0-4 — THE DOCUMENTED WALK-BACK DID NOT LOAD. FIXED.** `bfm_perch_lag_m = 0`
is named as the one-line off value in the handoff twice, in §E8.1, and at the
dial. Reproduced here: it throws at config load and takes the game AND the whole
test binary down. The cause is a contradiction the rung shipped inside itself —
0 falls back to `lag_dist_m` (the 220 m TRACKING point), which is EXACTLY the
pre-E8 geometry the tripwire exists to reject. ★ Nothing caught it because the
only leg using the off value sets it PROGRAMMATICALLY, bypassing the loader: a
test that never runs the path that ships. FIX: the tripwire is now gated on
`slash_doctrine` — a perch that cannot be dived is only a defect if a slasher
will fly it — and `load_scenario: the slash dive tripwire lets its own
walk-back load` pins BOTH directions through the REAL loader on a REAL TOML.

**P0-5 — THE FIRE-GATE WITNESS COULD LIE AND PROBE P-S STAYED GREEN. FIXED.**
The red team inverted the witness (`w_cone_cos = 1.0`, `w_coord_cos = -1.0` —
the exact opposite of both E8.1 findings) and P-S passed 7/7. The only assertion
touching it read `w_gate_live`, never a census VALUE. **Every number the E8.1
ruling rests on came out of an instrument no assertion checked.** ★ That is
E7.N's own lesson one level up: the doctrine was proven to INTEND to shoot and
never to shoot; the witness was proven to EXIST and never to be TRUE.
FIX, both halves: (a) the cosines are HOISTED into named locals in `pursue()`
and the gate is defined off those, so witness and gate cannot drift — E8 claimed
"the gate's own expressions, never a re-derivation" and had in fact re-typed
them three lines below, which is a fork that happens to live next door; (b) P-S
now pins the census PER FIELD on the arm that actually fires (`all_open`,
`coord_open`, `cone_open`, `band_open`, `cone_best`, `coord_best`). The
invariant is ONE-WAY on purpose: rounds imply an open gate, never the converse
(the `guns_hot` veto can hold fire with the gate wide open). RE-MUTATED HERE:
the inverted witness now turns P-S RED. Behaviour-neutrality of the hoist
verified on the deterministic probe — rounds 0/15, gate_ticks 7171/6009,
identical before and after.

**P0-6 — "the two stable columns move MONOTONICALLY" is FALSE on E8's own
printed table**, and the word shipped in three places (spec, commit, TOML).
`in_band` 1258 -> **1223** at the SHIPPED arm (a decrease, and the lowest of the
five); `foe_min` dips at 180. CORRECTED in §E8.4 and in `scenario.toml`.

**P1-9 / P1-10 — AND THE SURVIVOR DOES NOT SURVIVE.** The red team measured the
n=64 NULL FLOOR nobody had taken, on this table, on arms that cannot fight
differently (`ace_bank_cap` 71.5/72/72.5/73): `in_band` spans **+8.9%** and
`foe_min` **+1.7%**. So over half of E8.4's "+16% that survived" is reproducible
by half a degree of bank cap, and the +4% carries nothing. Worse, the two are
ONE OBSERVABLE, not two — `in_band` is a range-band subset of the same foe-gated
ticks `foe_min` sums — so "agreed across two stable columns" has the same shape
as the n=32 "agreed across three columns" that E8.4 correctly retracted. And
"top arm on the hits column at BOTH ensembles" is a selection over a column just
declared unresolvable, on two ensembles that share all 32 turn/altitude phase
pairs, beaten by an INERT null arm (hits 0.547 vs 0.469). ACCEPTED; the TOML
comment Chad reads is corrected; the DISPOSITION (120 ships for his stick, not
on the numbers, walk-back one line) stands — it was always honestly labelled.
★ That floor also corrected a claim of MY OWN in §E9.3. See it there.

**P1-8 — THE ATTRIBUTION AND THE FIX DO NOT MEET, and this is the sharpest
finding in the report.** E8.0's mechanism is "every ENGAGED fight mode commands
off `dl.speed`". True. But **Intercept is 51% of the in-close ticks in the very
tape being attributed**, and E8.2 excludes Intercept on the ground that "it has
E1.1's own target-keyed ceiling" — which inside `attack_range` collapses to
`dl.speed + pursue_speed_bump`, i.e. the keying is off exactly where the fight
is, the SAME defect the spec correctly identifies for `bfm_intercept_speed_mps`.
**The dial addresses 49% of its own diagnosis.** NOT FIXED HERE (it is a rung,
not a patch) and it is a better explanation of E8.4's null than "energy does not
matter". OPEN, and ranked in §E9.7.

**P1-7 / P1-12 / P2-17 / P2-18 — TAPE AND PROSE PRECISION.** "drone faster than
the player 7.2%" is presented as the SAME SAMPLES as the 124/202 pair; on those
samples it is **16.7%**. "The corner-speed law is untouched" is structurally
true and behaviourally not — `pursue_speed_bump` is `align^2`-faded, so off the
nose the turning speed rises 85 -> 120 (+41%). "82.8 -> 17.0 hp" understates: he
was at **100.0**. Three different figures are given for time-under-fire
(0.2 / 0.8 / 1.3 s). The foe-time PERCENTAGES divide by a drone-seconds
denominator; the raw seconds are robust. ACCEPTED, recorded, none fixed in the
E8 prose (it is a dated ledger entry, not a live artefact).

**P1-11 — a test named "bit identical" with no identity assertion. FIXED.**
`E8.1: the perch standoff off arm is bit identical` asserted `REQUIRE(moved)` —
the opposite — while its E8.2 sibling had a real one. It now measures the
identity its name promises: the `0` sentinel against `lag_dist_m` dialled in
explicitly, `==` on min range and all seven mode-tick counters.

**P1-13 — E8.3 QUOTES 4 OF 8 STRIKE WINDOWS AND OMITS THE ONE THAT CONTRADICTS
IT.** `i=3`'s second window reports `on_station = 2.8 s` at `closest_to_pump =
225.7 m` with `hp_delta = 0.0`, then crashes in the net at 40:17. An enemy DID
arrive at the deep pump, held station, did nothing, and died. ACCEPTED — and it
is direct evidence for E8.3's own untested hypothesis (b), which makes the pump
a better rung than §E8.3 makes it look.

**P1-14 — the E8.1 sweep the spec describes IS NOT IN THE CODE.** §E8.1 says
`{0,1600,1700,1900,2000} x {5,12,16} x {0.45,0.10,0.05}` (45 arms); the shipped
probe has SIX. CONFIRMED here by inspection. The described measurement is
UNVERIFIABLE from the artefact — it was run in a scratch edit and never
committed. ★ THE RULE THIS EARNS: a sweep a ruling rests on is a shipped artefact
or it did not happen.

**P2-15 / P2-16 — FIXED with P0-5** (the hoist; and all four witness fields are
now cleared per tick, not just the liveness flag).

**P2-19 — pre-existing, NOT fixed:** `test_ballistic_truth.cpp` hard-codes a
path through a build dir named `build`, so the gate silently requires that name.

### E9.7 ★★★ WHAT THE RED TEAM SAYS SHOULD COME NEXT — AND I AGREE
Its answer to "can P-A's stepped scripted player express the defect Chad felt"
is **no, and not close**, measured against his own tape:

    lateral g (med / p90 / max)   speed          |dV/dt|
    Chad, inside 900 m   4.30 / 16.6 / 25.0   90-309 m/s   med 2.6 m/s^2
    P-A scripted player  --   /  --  / 2.36   275 CONSTANT  0 by construction

The script's HARDEST turn is below Chad's MEDIAN in-close turn; his 90th
percentile is 7x the script's maximum; his speed varies 3.4x while the script's
never changes; and the script flies 275 m/s — **112% of `v_redline`**, a target
no bandit can ever match, permanently. ★★★ THE CONSEQUENCE, and it reframes
three rungs: **the ENERGY column is dead by construction.** Against a
constant-speed target above your own redline, no engaged base speed can buy
closure or an energy exchange — so E8.4's null was PREDICTED BY THE FIXTURE, not
measured about the mechanism. "The wall is pointing, not energy" is therefore
half a real finding and half an instrument artefact: pointing is the column the
instrument can still move; energy is the one it structurally cannot.
★ E9.3's own result is the fourth instance of the same dead end, and E9 already
recorded it independently (`in_band` +14% here beside +16% there). Two agents
reached it from different directions.

THE RANKED NEXT RUNGS, red team's order, and this rung concurs:
 1. ★★★ **THE INSTRUMENT.** Replay Chad's RECORDED trajectory out of
    `conquest_tape_100.jsonl` as the target into `drone::tick`, and count
    rounds / cone-seconds / gate census against the fight he actually flew.
    The house already owns the pattern — the sled rung's `tape_360_chad_repro`
    bit-exact tape gate — and the recorder already writes position and velocity
    at 5 Hz, so interpolation is the only work. Deterministic, primary data, his
    stick. Nothing else on this ladder is worth measuring first.
 2. ★★ **THE EMPTY SECOND HALF, and it is worse than E8.3 says.** The last tick
    ANY enemy held Chad as foe is **14:18 of a 48:59 match — 34:41, 71% of his
    session, with zero enemy contact.** (Also: "score frozen at 12:14" is loose,
    it moved to 14:19; and `planes 8 -> 7` at 15:13 is HIS OWN death, not
    attrition.) For a 49-minute session this dwarfs any dial on this ladder.
 3. **THE TUNNEL PUMP** (E8.3), with P1-13's correction: they DO arrive, hold
    2.8 s at 225 m, do nothing, and crash in the net.
 4. **THE SCOPED DIVE ENVELOPE** (from P0-1/2/3): E7.2 is not dead, it is
    clamped by a dial shared with the terrain pull-up — precisely the problem
    E7.4 just solved by scoping. At double the envelope the slasher fires 14
    rounds.
 5. **THE INTERCEPT CEILING** (P1-8): 51% of the in-close ticks the attribution
    named are in a mode the fix deliberately skipped.

## ★★★ RUNG E10 + E11 — SIGNED BY CHAD, 2026-08-23 (tape 7, 22:23)
> "check that last match, it was good, they attacked underground, had my first
> dogfight underground, more formidable opponents that shoot at me, and not
> insanely difficult, very nice balance for now, I think this qualifies as a
> game loop. I had fun, many suspenseful moments"

**THE GAME LOOP IS SIGNED.** That is the sentence this ladder has been trying to
earn since E1, and the numbers behind it are recorded here as the BASELINE any
future rung must not regress.

### E10/E11.S THE SIGNED MEASUREMENT (conquest_tape_7, signature VERIFIED)

    metric                     tape 100    tape 5   tape 6   TAPE 7 (SIGNED)
    rounds aimed at him        5 / 49 min    18       14      39 / 22 min
    rounds/min                     0.10     1.26     1.00           1.74
    time under fire                0.8 s     43 s    295 s          843 s
    ...as a share of the match       --       --       --            37%
    AI terrain crashes/min           --     0.93     1.50           0.94
    tunnel entries (enemy)      1 of 18        5       --   13 (5 with him down there)
    his deaths to enemy fire          1        0        0              0

★ His only death was at 02:50, a terrain crash with every component at 1.00 --
his own, not theirs. ★ "Not insanely difficult" is measured too: 39 rounds were
aimed at him and NONE of them hit. The balance he signed is a fleet that shoots
often and connects rarely.

### E11.V THE BUBBLE FIX, CONFIRMED ON HIS OWN FLIGHT
    07:44  pump 3 (enemy deep)    dies -> rs [1.25, 0.5]
    13:02  pump 1 (enemy surface) dies -> rs [1.5, 0.0]   <- ZERO, not 0.25
    13:04  countdown_faction = 1, countdown = 10.00 min, ticking
He quit at 22:23 with 36 s left on their clock. Pre-E11 this match would have
ended exactly as tape 6 did: a quarter-bubble that never collapses, no clock,
and a fleet loitering inside a dome that should not exist.

### E11.C AND THE CRASH RATE FELL WITHOUT BEING CONFIRMED
    tape 6 (E10 only):  21 crashes / 14.0 min = 1.50/min
    tape 7 (E10+E11):   21 crashes / 22.4 min = 0.94/min   (-37%)
⚠ RECORDED, NOT CLAIMED. The air-seek altitude fade is the only candidate that
touches this and it measured NULL on P-A -- because P-A never destroys a pump,
so its bubbles never shrink and the air-seek never fires there. One tape is not
a measurement of a rate. The honest status is unchanged: **the fixture that
could confirm this does not exist yet**, and building a conquest probe that
actually kills pumps is the standing next rung. 6 of the 21 were in the tunnel
net, which is a different defect again.

### E10/E11.N WHAT THIS RUNG DID **NOT** FIX, so nobody reads the signature wider
  * The tunnel-net collisions (6 of 21 crashes, RUN duty, near-level flight).
  * `probe P-F` clause (2) is RED and left red: with the tighter turn the
    YIELDING raider now stays 2.5 km from the pump and shoots 13 rounds while
    the PRESSING one ends 4.7 km away with 3 s on station of 90. E6.5's
    corroborating clause is dead and the ruling needs re-making by Chad -- it is
    not a test to bend a third time.
  * E8.2's Intercept exclusion (red team P1-8) and the E7.2 scoped dive envelope
    (red team P0-1/2/3) are still open rungs.

## ★★★ RUNG E12 — THE ENEMY CAN WIN (Chad's ruling, 2026-08-23, the night he
## signed the game loop)
> "the ai is doing what it is supposed to, lets now get out of easy mode and
> increase its ability to destroy pumps"

and, ruling the two follow-up questions in the same breath:
> difficulty: **"You can actually lose"** — tune it so an enemy raid chain will
> finish a pump if he never goes home; losing the match becomes a live outcome.
> lethality: **"They can get a bit deadlier too."**

★ THE SECOND ONE REVERSES A STANDING LINE IN THIS LADDER. E10/E11's handoff says
"more lethal is NOT the goal, he has ruled the balance twice." He has now ruled
the other way, on his own initiative, one day later. His words win, and the
lethality change ships as ONE separate dial with its own walk-back so it can be
reverted alone if his stick disagrees (§E12.3).

### E12.0 THE MEASUREMENT — HIS OWN SIGNED TAPE, READ AS AN INSTRUMENT
No new fixture was needed to find the defect: `conquest_tape_7.jsonl` already
carries per-drone `raid`/`strike` flags, `rpi`/`spi` target indices, positions
at 5 Hz and the pump HP vector. Decoding it:

    enemy damage delivered      20 835 of the 39 240 HP needed
                                (his surface pump to 61%, his DEEP pump to 32%)
    enemy on-station time       ~32 s in 22 min   (a pump falls in 30 s)
    enemy air-to-air kills      0. His squadron never lost a plane, never
                                dropped below 100 hp, all match.
    enemy reinforcement pool    4 per faction, ALL SPENT by t=517 s
    raider alive fraction       spawn 0: 35%   spawn 1: 40%
    raider median range         18.4 km / 14.5 km from the pump they were
                                ordered to destroy

★ THE DPS WAS NEVER THE PROBLEM. `raid_dps_frac` 0.5 kills a pump in 30 s of
on-station and the enemy earned 32 s across a whole match — it very nearly took
his deep pump anyway. What it could not do was *keep showing up*.

### ★★★ E12.1 THE FINDING — THE OFFENSE SWITCHES OFF AND NEVER SWITCHES BACK ON
`combat::faction_raider` ranked the FIXED maverick trait table — top-2
aggression per faction — and never asked whether those two pilots were alive.

    a raider's whole life is one transit: spawn ~28 km out, close at ~125 m/s,
    arrive at ~250-300 s, and die there (6 of their 9 deaths were his guns, at
    46-313 m). Then the E6.6 pool runs dry and they stay dead.
    -> from t=617 s the enemy still had THREE aeroplanes flying and NOT ONE of
       them carried a pump order. His surface pump took ZERO damage after
       minute ten. Twelve minutes of a twenty-two minute match.

★ IT IS THE E11 SHAPE AGAIN (memory `enemy-ai-e1-e2-rung`): a FACT about the
roster derived from something that is not the roster. There an accumulator
stood in for a fact; here a trait table stands in for who is flying. Both
survive every test in the suite because nothing ever asks the question.

FIX: `faction_raider(spawn_index, const bool* live = nullptr)` ranks over LIVING
same-faction pilots. `nullptr` IS the off-value and reproduces the pre-E12
designation bit-for-bit; the app builds the mask as a PRE-LOOP SNAPSHOT (the
striker-cap discipline) and passes it only under `[conquest] raid_backfill`.

### E12.I THE INSTRUMENT — PROBE P-H, THE CONQUEST MATCH
`test/unit/test_conquest_match.cpp`. The standing OPEN item 1 from the E10/E11
handoff, built: a whole 22-minute conquest match, real loaders, real arena and
domes, the app's own helpers, the app's tick order — and **a real player**.
Chad is REPLAYED out of tape 7 (trajectory at 5 Hz, the 9 pilots his guns
killed, the 2 enemy pumps his side destroyed), distilled by
`offline_tool/distill_conquest_tape.py` into
`test/golden/conquest/tape7_chad_replay.txt`. Its four honest limits are in the
file banner and every claim below carries them: the replay is OPEN LOOP, his
BANK is not in the tape, his side's pump offense is the replay and not the sim,
and there are NO BALLISTICS (attrition is the replayed kill schedule).

CALIBRATION against the match it replays, before any arm was believed:

    metric                    tape 7        P-H shipped arm
    enemy plane-seconds        4419              4430          (+0.2%)
    enemy on-station            ~32 s            34.4 s        (+7%)

It distributes its credit deeper than the tape did (flat terrain, no GIS relief
around the surface pump), so it is a RELATIVE instrument: read the deltas
between arms, never the absolute difficulty.

### E12.1 WHAT THE BACKFILL BOUGHT (P-H A/B, `raid_dps_frac` held at the
### tape's 0.5 so exactly one dial moves)

    arm                    raid duty   on-station   near   player pumps
    OFF (pre-E12)            1015 s      34.4 s    102 s   surface 85%, deep 0%
    ON  (live-roster mask)   2203 s      51.1 s    150 s   surface 0%, deep 30%
    delta                    +117%        +48%     +47%

★ THE ENEMY'S RAID DUTY MORE THAN DOUBLED WITHOUT ONE EXTRA AEROPLANE. Same
fleet, same attrition schedule, same 4430 enemy plane-seconds — the faction
simply stopped standing down when its two named pilots died.

### E12.2 THE DPS STEP — "you can actually lose", as a number
E12.1 alone takes ONE of his pumps in P-H's (enemy-favourable) frame, at
21.3 min. His ruling asks for a match he can lose, so the second dial moved:

    raid_dps_frac   pumps down   first lost   deep pump left
        0.50           1 of 2      21.3 min        30%
        0.65           1 of 2      17.8 min         9%     <- SHIPPED
        0.80           2 of 2       6.2 min         0%
        1.00           2 of 2       6.2 min         0%

★ 0.80 IS A CLIFF, NOT A STEP — it takes his first pump at SIX MINUTES, which
is not "you can lose", it is "you have already lost". The threshold is
arithmetic: the unopposed kill time is `pump_kill_seconds / raid_dps_frac` =
30 / 23 / 18.75 / 15 s, and ~19 s is short enough to fall inside ONE early
contiguous raid window while 23 s is not.
★ NOTE THE COUNTER-INTUITIVE COLUMN: on-station seconds FALL as the DPS rises
(51 -> 30), because a dead pump credits nothing more. On-station is an input,
not a score — read PUMPS DOWN.

### E12.3 "THEY CAN GET A BIT DEADLIER TOO"
`combat::difficulty_params` level 4: rof 8.0 -> 11.0, damage 11.0 -> 14.0 —
LEVEL 5's own values, so nothing is invented. `max_engaged` deliberately STAYS
at 3: the block above it is a measurement that raising it to 4 HALVES the
rounds that reach him (39 -> 19 on P-A) because a fourth attacker crowds the
same ace's geometry.

★★ THE LIMIT THAT DECIDES HOW TO READ HIS FLY: his signed tape has 39 rounds
aimed and ZERO hits; P-A's scripted player takes 18 hits out of the same 39.
**No fixture we own predicts CHAD's dodge**, so that gap cannot be closed from
a desk. ROF is the only lever that scales his exposure linearly and this is a
bounded step on it. His stick is the verdict.
★★ AND THE COUPLING, UN-MEASURED ON PURPOSE (P-H runs no ballistics):
`bandit_rof_hz * bandit_damage` is ALSO the AI-vs-AI DPS, so this raises
squadron attrition 75%. It cuts both ways — the enemy finally kills some of his
wing (which lost NOTHING all of tape 7) but faster enemy losses drain the E6.6
pool that feeds the raid. **If his fly says the pump pressure got weaker than
E12.1 predicts, this is the first suspect.**

### E12.G THE GATE (his ruling, as a test)
`E12: the enemy's pump offense does not regress below tape 7` — three clauses,
because the first two can each be met while giving the rung away: raid duty
rises, it REACHES the pumps (`near_s`), and a player pump actually falls.
⚠ n=1 TRAJECTORY. One tape is one trajectory, not a distribution. The raid-duty
clause is structural; the pump clauses are trajectory-bound and say so.

### E12.N WHAT E12 DID **NOT** DO
  * It did not touch the TRANSIT, which is the largest remaining lever and the
    one measured most clearly: a raider spawns ~28 km from its target, closes
    at ~125 m/s, and its whole ~250-300 s life is the commute. Wave placement
    is the transit tax. Left alone because moving it moves how the fleet
    ARRIVES, which is a feel change, and Chad has not seen this rung fly yet.
  * It did not touch `reinforce_pool_n` (4). The enemy spending its pool is
    real and E11's ruling stands: elimination must mean having had something
    and lost it.
  * It says NOTHING about lethality being right — E12.3 is a step in a
    direction he named, not a solved number.

### ★ E12.C AN UNASKED-FOR RESULT — P-H SEES THE CRASH CHAIN P-A COULD NOT
The E10/E11 handoff's OPEN item 2 diagnosed but could not confirm a chain:
pump dies -> bubble shrinks -> thin air -> air-seek dive -> redline -> control
authority compresses to `min_frac` -> the pull-up cannot rotate. FIVE candidate
fixes all measured NULL on P-A, "because P-A never destroys a pump, so its
bubbles never shrink, so the air-seek never fires."

P-H destroys pumps. Enemy terrain crashes across the DPS sweep, same fleet,
same trajectory, same attrition schedule -- only how fast the pumps fall:

    raid_dps_frac   0.50   0.65   0.80   1.00
    enemy crashes     18     19     38      48
    player pumps       1      1      2       2  (of 2, down)

★ The crash count TRIPLES exactly as the second pump starts falling early.
That is the first evidence the chain is real and not a story, and it is the
first thing this ladder has ever been able to say about it. It is NOT yet an
attribution -- the arms differ in pump deaths AND in every downstream thing a
pump death causes -- but the fixture that can now isolate it exists.

### ★★★ E12.F — CHAD FLEW IT (conquest_tape_8, 2026-08-24, 17.6 min)
> "They did go for our underground pump I saw it was 72% or so when I flew in
> there to check it. The enemies did destroy a surface pump. And they also
> killed me in a headon near the end where my allies finished them off. I would
> say to save and commit this for now.. It is a good easy / medium baseline for
> difficulty."

**BOTH HALVES OF HIS RULING LANDED, AND BOTH ARE FIRSTS FOR THIS LADDER.**

    08:13  enemy deep pump 3 dies (his side)
    11:39  ★ HIS SURFACE PUMP (0) DIES  -- the first pump this AI has ever taken
    14:00  enemy surface pump 1 dies
    16:53  ★ HE IS KILLED BY GUNS      -- the first time enemy fire has killed
           him. air=1.000 (full air, not a thin-air death), right wing shot
           OFF (comp 0.00), engine 0.70, structure 0.86.
    final  score 240-100. THE ENEMY IS ON THE SCOREBOARD FOR THE FIRST TIME.
           His deep pump survived at 78% (he read it ~72% in the stope).

### ★★★ E12.F1 THE COST, MEASURED — THE RAID WAS PAID FOR OUT OF THE HUNT
This is the honest headline and it names the next rung. Enemy duty allocation,
as a share of their own live plane-seconds:

    metric                       tape 7 (signed)   tape 8 (E12)
    enemy plane-seconds               4431            4747
    time HUNTING the player           848 s (19.1%)   393 s (8.3%)
    time on RAID duty                1016 s (22.9%)  1398 s (29.5%)
    engaged at all                   1876 s (42.3%)  1180 s (24.9%)
    rounds aimed at him              39 (1.74/min)   16 (0.91/min)
    his deaths to enemy fire              0               1
    AI crashes                      21 (0.94/min)   22 (1.25/min)

★ THE RAID AND THE HUNT ARE COMPETING FOR THE SAME FIVE AEROPLANES. E12.1 made
raid duty CONTINUOUS, so two of the enemy's live pilots are permanently on it —
and the time came out of hunting him. The signed 1.74 rounds/min fell to 0.91.
★ BUT THE ROUNDS THAT CAME WERE LETHAL: 39 rounds -> 0 kills became 16 rounds
-> 1 kill. E12.3 did what it was asked to.
★ HE DID NOT COMPLAIN AND RULED IT A GOOD BASELINE, so this is NOT a regression
to undo — it is the trade the rung actually made, written down, so nobody
later reads "rounds went down" as a defect or "pumps fell" as free.

### E12.F2 HIS TWO OBSERVATIONS, ANSWERED WITH MEASUREMENTS
**(a) "allies and enemies fly out of the bubble at the start, one AI each
side."** CONFIRMED, and it is BY DESIGN — it is the raid sortie, which
suppresses the containment leash on purpose ("the sortie deliberately leaves
the dome"). It is TWO per side, not one: the designated raiders. In tape 8 the
enemy pair (0, 1) crossed their dome edge at 106 s and 122 s and kept going
(frac 1.49 / 1.59 at 3 min, out to 2.3-2.6 over the match); his own pair (5, 8)
crossed at 83 s and 91 s and came back. Drone 3 also crossed at 76 s, but as a
tunnel STRIKER in TRANSIT — also leash-exempt.
★ E12.1 is what makes this permanent: raid duty is now always occupied, so
there is ALWAYS a pair outbound. What he is seeing is the rung working.

**(b) "are they affected by the no-air uncontrollability?"** YES, IDENTICALLY —
and this was already measured (test_enemy_ai_e6.cpp, "the drone plant pays the
player's vacuum tax"). The drone plant and the player plant are THE SAME
`sim::step` with THE SAME `sim::Environment`. The exact shape:
  * TORQUE is FLOORED: `tau = c * max(q, q_att_floor) * delta_max_eff(V)`, and
    `q_att_floor = 280 Pa` ([authority], aircraft.toml). So no matter how thin
    the air, the aeroplane can still POINT — for him and for them alike. That
    floor is why anything can fly out there at all.
  * LIFT / THRUST / DRAG are NOT floored: `rho_at = rho * atm_frac_at`, so they
    lapse to nothing. You keep the nose and lose the turn, the climb and the
    speed. That is the mush.
  * `min_frac = 0.4` is a SPEED compression floor (deflection at v_redline),
    NOT an air term — do not confuse the two.
★ So "they fly fine in no-air" remains a BEHAVIOUR finding, not a physics one,
exactly as E6.2 ruled: what lets them out there is the leash exemption, not a
plant discount. MEASURED in tape 8: his raiders spend 65-75% of their lives
outside their own dome, out to 2.9x its radius. **He is outside his own dome
34% of the match himself**, peaking at 1.98.

### E12.F3 THE ONE NUMBER THAT MOVED THE WRONG WAY
AI crashes 0.94 -> 1.25/min (+33%), against a rate he had called himself
content with. It is what P-H predicted: crashes climb with pump deaths (18 ->
48 across the DPS sweep), and tape 8 had THREE pump deaths to tape 7's two.
The crash chain is now the top mechanical rung, and P-H is the fixture that
can finally isolate it.

## ★★★ RUNG E13 — THE OUTNUMBERED ROSTER (Chad's ruling, 2026-08-24, on tape 8)
> "We could just increase the number of enemies now 2/1. We can be outnumbered
> and that will increase the difficulty."

The wing was 5/5. It is now **7 SUDBURY / 3 VALLEY** — the SAME ten
hand-authored pilots, re-split, so no callsign and no trait number is invented
and the simulated fleet size (and its cost) is unchanged. He flies VALLEY, so
he is outnumbered 7 to 3 AND thinner on support at once.
WALK-BACK: `kSudburyTeamSize` back to 5.

### E13.1 TWO CONSEQUENCES THAT HAD TO BE HANDLED, NOT LEFT TO DRIFT
**(a) RAID DUTY IS A PROPORTION, NOT A COUNT.** `faction_raider` shipped "the
top 2 by aggression", authored when both wings were 5 — two fifths on offense.
Left as a bare 2 across a 7/3 split that silently becomes 29% of the enemy and
**67% of his own squadron**, which would leave him ONE wingman actually
fighting beside him. Now `combat::raiders_for_team`, rounded to nearest,
floored at 1, capped at the wing: 7 -> 3, **5 -> 2 exactly (the shipped rule)**,
3 -> 1. WALK-BACK: return 2 unconditionally.

**(b) ★ THE SPAWN SLOT WAS COPY-PASTED IN SIX FILES.** `slot = i % 5` and
`frac = (slot + 0.5) / 5` appeared in `app/main.cpp` and FIVE test fixtures.
Across an uneven split those copies are not merely fragile, they are WRONG —
`i % 5` maps spawn 5 and spawn 0 to the same slot, and the denominator is no
longer either wing's size. Hoisted to `combat::team_slot` / `combat::team_size`;
one function, six callers.
★ THAT IS THE FOURTH TIME THIS LADDER HAS BEEN BITTEN BY THE SAME SHAPE
(`ai_guns_on`'s band, E8's decision table, E12's two, now this). The pattern is
worth stating as a law: **a constant that describes the shipped table stops
describing it the moment the table moves, and nothing goes red.**

### E13.2 WHAT IT MEASURES ON P-H — IT BUYS AIR PRESSURE, NOT PUMP PRESSURE
Same replay, same dials, only the roster differs:

    metric                    5/5 (E12)     7/3 (E13)
    enemy plane-seconds          4430          7117     (+61%)
    raid duty                    1982 s        2776 s   (+40%)
    enemy on-station             44.1 s        36.3 s   (-18%)
    his surface pump              0%            0%      (falls either way)
    his DEEP pump                  9%           43%     (HEALTHIER)
    first pump lost              17.8 min      16.3 min
    enemy crashes                  19            36     (+89%)

★ **THE PUMP OFFENSE GOT SLIGHTLY WORSE, NOT BETTER**, despite 40% more raid
duty and 40% more aeroplanes. The strike RUN column is where it went (357 ->
299 s): more enemies means more of them outside the dome, and **the crash count
nearly doubled** — a 40% bigger wing crashing 89% more often is a ~35% higher
per-plane crash rate. They are dying on the way rather than arriving.
★ So E13 is honestly an AIR-WAR change, and the difficulty it adds is the 7-v-3
dogfight he asked for. It does NOT compound E12's pump pressure; it slightly
taxes it. Anyone reading "more enemies" as "more pump damage" is wrong, and the
number is here so nobody has to guess.

### ⚠ E13.3 A NEW LOSS CONDITION HE HAS NOT SEEN YET
His squadron is 3 aeroplanes against 7, with the SAME per-faction reinforcement
pool of 4. Victory-by-wipe is live once a faction's pool is spent, so "all of
the player's planes are gone" is now a materially reachable DEFEAT that it was
not at 5/5. That is arguably exactly what "we can be outnumbered" should feel
like — but it is a new way to lose the match and it is un-flown. Flag on the
fly card.

### ⚠ E13.4 THE TAPE-7 CALIBRATION IS NOW INFORMATIONAL
P-H's calibration leg reproduced tape 7's attrition to +0.2% — AT 5/5, once,
before any arm was believed (§E12.I). E13 means the fixture can no longer fly
the world tape 7 was flown in, so that clause is now wing-SCALED and explicitly
demoted to informational in the file. **A fresh calibration needs a tape flown
at the shipped roster**, which his next fly produces. The clause still forbids
the failure that would actually invalidate an A/B: an order-of-magnitude drift,
or a fixture wired to the wrong faction's slots.

## ★★★ RUNG E14 — THE DEFENDER SLOT (Chad's fly report on tape 9, 2026-08-24)
> "that last match was pretty good! they didnt attack underground however, are
> there any being allocated to the underground?? ... enemy ai abandoned their
> own pump leaving me to attack it and only one stayed to defend but didn't
> engage me aggressively, the others went for the opposing bubble but they
> should have attacked me as they were close to their pump but abandoned the
> chase (2 of the 3) ... my 3 allies were just as effective as the whole 7
> enemies ... I believe I only destroyed 3 of them."

TAPE 9 (E13's first fly, 9.8 min): his SURFACE pump died at 8:34 (E12 still
working); his DEEP pump ended at **100%, untouched**; both enemy pumps died at
6:07; score 230-100; he took 3 kills, his ALLIES took 4, the enemy took 1.
**Three allies out-killed seven enemies 4 to 3.**

### ★★★ E14.A "ARE ANY BEING ALLOCATED TO THE UNDERGROUND?" — YES, AND NONE ARRIVE
Every enemy carried a standing strike order 100% of the time, and four of them
LAUNCHED. They just never got in:

    tape 9      launches  dives  runs  underground  TIMEOUTS
      ENEMY        10       1      0        0 s        8
      allies        4       2      2      130 s        0
    tape 8 (5/5, for the record — this is NOT an E13 regression)
      ENEMY        19       2      2      185 s       16
      allies       13       6      5      326 s        6

★ THE ENEMY'S TUNNEL OFFENSIVE CONVERTS 0-11%; HIS ALLIES' CONVERTS 38-50%.
A 3-4x asymmetry that has been there at least since tape 8 and that nobody had
ever looked at. The enemy transits STALL: three separate episodes ran the full
150 s timeout and ended 14.2 km from the target, having closed 0.2-2.3 km.
E13 did not cause it; E13 only made a shorter match round it down to zero.
**THIS IS THE NEXT RUNG** and it is un-diagnosed past "they stop closing".

### ★★★ E14.B "ONLY ONE STAYED TO DEFEND" — TWO SLOTS, ONE DEFENDER
He spent 87 s inside the 2500 m defend-threat radius of the enemy surface pump
while he killed it. Who was ordered to defend it:

    drone  defend%   median range to HIM   what it actually was
      6     99.7%          4.0 km          the ONE. Closed to 72 m.
      2     56.1%         12.9 km          mid TUNNEL RUN, never closed
      4     30.1%         12.6 km          mid TUNNEL RUN, never closed
    0,1,5    0%          28-30 km          the three RAIDERS, on their sortie

★ THE DEFECT: `combat::assign_defense` picks the NEAREST same-faction pilots,
and drones 2 and 4 were nearest — but `drone::tick` gates the ENTIRE defend
branch behind `if (!tunnel_mode)`, because a committed run must be free to
cross the vacuum on the deck. **So the two pilots holding defender slots were
structurally incapable of flying the order, and they consumed the slots while
doing it.** Two slots issued, one real defender. Exactly what he watched.
★ IT IS THIS LADDER'S FAVOURITE SHAPE AGAIN: an ORDER issued by one module
against a fact that a DIFFERENT module silently refuses to act on. Nothing goes
red; the pump just dies.
FIX: a committed tunnel disposition is not eligible for a defender slot, tested
on the SAME `maverick::is_tunnel_mode` predicate drone::tick gates on so the two
can never disagree. A wing with nobody else fields FEWER defenders, honestly.
Mutation-verified both ways.

### ⚠ E14.C "DIDN'T ENGAGE ME AGGRESSIVELY" — AND A FIX I BUILT AND THREW AWAY
The defend branch also sets `d.wants_fire = false` unconditionally, so I built
`defend_fight_in_place` to give defenders their guns back, mirroring E6.5's
raid-branch precedent — and **my own test proved it was a DEAD BRANCH.** The
window it needs is empty: `fight_hot` releases the defend branch at
`raid_fight_yield_m` = 2500 m, and the guns only open inside `snapshot_range_m`
= 900 m, so a pilot in the defend branch is ALWAYS out of gun range. It was
reverted, not shipped.
★ THE REAL ANSWER IS WORSE AND IT IS NOT THE DEFEND BRANCH. Drone 6 DID come
(72 m, BFM Offensive, out of the defend branch entirely). Across the whole
match the enemy held him as foe for 284 s, **49 s of it inside the [60, 900] m
gun band — and fired ZERO rounds at him.** All 13 fire events in tape 9 are his
OWN allies shooting enemy drones. Of that 49 s, 15 s was BFM Extend and 5 s
Defensive (both guns-cold by design), leaving **~29 s of Offensive/Yoyo/
Intercept inside the band with no fire intent at all.** That is the E1/E7 fire
chain, not the defence, and it is where "aggressive" actually lives.
⚠ Rounds aimed at him: 39 (tape 7) -> 16 (tape 8) -> **0** (tape 9). His time
held as a foe fell with it: 848 s -> 393 s -> 284 s. The DUTY collapse is
measured and attributable (E12's raid trade, then E13 putting 3 of 7 on raid);
the zero on top of it is a small sample and is NOT claimed as a separate effect.

## ★★★ RUNG E15 — STAGE A'S BUDGET (the tunnel offensive that never arrives)
The rung E14 named as next, diagnosed and fixed.

### E15.M THE MEASUREMENT — A TIMEOUT THAT WAS SECRETLY A RANGE LIMIT
Every enemy TRANSIT episode in tape 9, with where it started and how it ended:

    start dist to approach pt   flew      ended
        22.0 km                 38.0 s    PATROL  (fight-yield)
        15.9 km                150.2 s    PATROL  <- TIMEOUT, 4.7 km short
        14.9 km                150.0 s    PATROL  <- TIMEOUT, 5.4 km short
         9.5 km                150.0 s    PATROL  <- TIMEOUT, 7.7 km short
         5.4 km                116.0 s    DIVE_IN (the ONE that arrived)
     ...and the allies:
        16.2 km                149.4 s    DIVE_IN (by 0.6 s)
         5.7 km                116.8 s    DIVE_IN

★ THE THREE TIMEOUTS END AT EXACTLY 150.0-150.2 s — the RAW `transit_timeout_s`
— which means they never captured the FIX and so never earned the 90 s
`transit_fix_grace_s` this table already ships. **It is a STAGE A failure**, and
the E2.1 comment block had already named that shape ("Stage A ... costs 105-135 s
of the 150 s").
★ MEASURED effective transit closure: **91.5 m/s enemy, 102.3 m/s ally** — not
the 143 m/s airspeed, because of the FIX dogleg. So 16 km needs ~175 s of Stage A
and 22 km needs ~240 s. **A FIXED 150 s IS A RANGE LIMIT WEARING A TIMEOUT'S
CLOTHES: any run ordered past ~15 km is dead on launch, and nothing checks it.**

### ★ E15.G THE ASYMMETRY THAT LOOKED LIKE A MAP PROBLEM ISN'T ONE
The obvious hypothesis — "the Sudbury mouth is further from Sudbury than the
Valley mouth is from Valley" — is FALSE, and measuring it is what stopped a
plausible wrong fix. Arc distance from each faction's dome centre to its own
approach point: **ENEMY 12.4 km, ally 15.1 km.** The geometry FAVOURS the
enemy. His allies simply happened to launch closer, and converted 38-50%
against the enemy's 0-11%.

### E15.F THE FIX
`transit_reach_s_per_km` (0 = off, bit-identical): Stage A gets
`transit_timeout_s` PLUS this many seconds per km of the distance left to the
FIX, measured ONCE on the transit's first tick and latched in
`MaverickState::transit_budget_ticks`, cleared at every give-up exactly like the
two latches beside it so a go-around cannot buy a second allowance. Shipped
**11.0 = 1000 / 91.5**, the SLOWER of the two measured closures, so the budget
is sized on the arm that was failing.
★ ANTI-LIVELOCK SURVIVES WITH NO CAP DIAL TO INVENT: the distance is measured
once, at entry, and is bounded by the planet itself, so the budget is finite for
every reachable geometry. A livelocked pattern stays unrepresentable; it is
simply given an honest amount of rope.
★ Measured on probe P-B, same world, only the dial moving:

    arm                 launches   runs   TRANSIT TIMEOUTS
    flat 150 s             31       11          17
    distance-scaled        23       12           9      (-47% wasted)

Fewer transits are thrown away, one more run arrives, and the fleet needs FEWER
launches to do it.

### ★★★ E15.R IT SHIPS **OFF**, AND THE MEASUREMENT IS THE RUNG
The mechanism is correct and green. It cannot be paid for. Probe P-H, the same
replay, only this dial moving:

    s_per_km   his pumps           first lost   ENEMY CRASHES/min (signed 0.94)
       0.0     surface 0, deep 18%   16.5 min          1.65
       3.0     BOTH down              7.2 min          4.60
       6.0     BOTH down              7.2 min          4.60
      11.0     BOTH down              7.2 min          4.60

★ 3.0, 6.0 and 11.0 are IDENTICAL TO THE DIGIT. Past ~3 s/km no transit ends on
the timeout any more, so more rope is inert -- the entire effect is the FIRST
step, and that step nearly TRIPLES the crash count (37 -> 103 in 22 min).
★ THE CAUSE IS A DEFECT E15 DOES NOT FIX, and it is the one E14's forensics
already named: **TRANSIT is leash-EXEMPT**, so a striker crossing to the mouth
is outside its own dome in thin air, where lift lapses (`rho * atm_frac`) and
is NOT floored the way torque is. Tape 8 put 14 of 22 AI crashes outside the
dome. More rope for the tunnel is, mechanically, more time in the place they
die. The enemy would take both his pumps in 7 minutes and lose an aeroplane
every 13 seconds doing it.

★★★ **SO THE LADDER RE-ORDERS. THE BUBBLE-EXIT / VACUUM CRASH DEFECT NOW BLOCKS
THE TUNNEL RUNG**, and it is the same defect behind E12's crash column, E13's
19->36, tape 8's 14-of-22 and tape 9's stalled transits. It has stopped being a
side finding and is now the critical path. Fix it, then turn E15 on with ONE
edit -- the dial, its latch, its loader band and three tests are in place and
green, and the contract legs name their own rate rather than reading the
shipped 0.0, so they do not go vacuous while it waits.

## ★★★ RUNG E16 — THE DECK WAS MEASURED FROM THE WRONG SURFACE
The critical path E15 named, taken. **The AI was not flying badly out there.
There was nowhere to fly.**

### E16.M THE MEASUREMENT — FIRST, THE FIXTURE HAD TO SEE THE DEFECT
Probe P-H now carries the tape-8 wreck audit INSIDE itself: every enemy crash
is recorded with the state it was flying when it died, sampled pre-tick, and
the air read from `sim::atm_frac_at` — the same call the plant reads for lift.
The shipped arm reproduces Chad's tape exactly in shape:

    crashes 37 in 22.4 min (1.65/min, he signed 0.94)
    34 of 37 in air below 0.25      mean air at death 0.11
    16 of 37 steeper than -30 deg   mean gamma -28.5 deg
    37 of 37 faster than 150 m/s    mean speed 210 m/s
    by disposition: raid 19, tunnel 11, fight 5, patrol 2, defend 0

★ 30 of 37 wrecks are flying an OUTBOUND order. That is the leash-exempt
sortie crossing the gap between the domes, and it is the whole defect.

### ★★★ E16.A THE AIR MAP — THE MEASUREMENT THAT DECIDED THE RUNG
`[.e16air]` flies nothing. It samples the density field along the corridor a
raid actually crosses, every 50 m, at each candidate altitude, and reports how
much of that corridor is DEAD (air < 0.25 — where the wing has stopped
working):

    altitude    dead km        altitude    dead km
      200 m      0.00            1800 m     3.50
      350 m      3.10            2500 m     4.25
      500 m      3.10            3200 m     5.70
      800 m      3.15            4000 m     9.35
     1200 m      3.25

★ **THE CURVE IS MONOTONE AND IT KILLED THE OBVIOUS FIX.** The first cut of
this rung was a ballistic HOP — climb high inside your own air, coast across —
because 5 km of vacuum at 200 m/s from 3 km is ballistically crossable. It is
also exactly backwards: **the domes are DOMES**, so the higher you go the wider
the gap between them gets. Built and measured, the hop made it worse (thin-air
plane-seconds 501 -> 1097, impacts at -47 deg and 250 m/s). ★ AND THE 200 m ROW
IS THE FINDING: down at deck height the corridor is **continuously breathable,
end to end**.

### ★★★ E16.D THE ROOT CAUSE — A CONTRACT THAT STOPPED DESCRIBING THE WORLD
`sim::atm_frac_at`'s deck term is `atm_falloff(alt - deck_agl_m, deck_soft_m)`,
and `alt` is altitude above the SPHERE. `game.toml` describes the same dial as
"full air below this height **above the surface**, EVERYWHERE (the go-anywhere
floor: you can still land outside a bubble)", and `sim::AtmosphereField` itself
says "AGL is measured over the bare sphere at R in R6 (terrain-relative deck is
a later refinement)".

**On a bare sphere those are the same sentence. R6 landed on a bare sphere.
Then the real DEM landed under it and nothing went red** — this ladder's own
recurring law, paid for a sixth time. Measured on the shipped
`assets/sudbury_dem.png` at `relief_scale_m = 350`:

    median terrain elevation                165 m
    surface above the full-air deck (120 m)  69%
    surface above the deck's fade  (320 m)   26%

So over most of the planet the go-anywhere breathable floor is **underground**.
Chad's own ruling — the whole surface stays traversable and landable — has been
quietly false since the DEM bake, and the fleet has been ordered to cross a
corridor with no lane in it.

★ AND IT IS WHY EVERY EARLIER FIX FAILED. The air-seek dive
(`docs/ai_vacuum_strand.md` rungs 1/1b) is CORRECT: dive for the deck, because
"down is always a valid answer". It was pointing them at a deck that was not
there.

### E16.F THE FIX
`[atmosphere] deck_terrain_relative` (false = the R6 bare-sphere deck,
bit-for-bit). True measures the deck from `env->ground->radius_at` — the ONE
elevation query, the same field the crash surface uses, never a second source.
Read only where a ground field is live, so the frozen-kernel path is untouched.

★ MEASURED ON P-H, THE DIAL THE ONLY THING MOVING, THE AI NOT TOUCHED AT ALL:

    metric                    shipped    terrain deck
    enemy crashes              37          18        (1.65 -> 0.80/min)
    ...in air below 0.25       34           0
    ...on a raid sortie        19           2
    mean air at death        0.11        1.00
    enemy on-station          42.0 s      46.2 s
    his pumps            surface dead, deep 18%   BOTH DEAD (first at 6.8 min)

★ **THE ENEMY GOT BETTER AND SAFER AT THE SAME TIME**, which is the signature
of a defect being removed rather than a balance being dialled. The 18 that
remain die in FULL air (mean 1.00) — ordinary flying, 12 of them the known
tunnel-net collisions, which is a different defect and still open.

### ★★★ E16.R AND E15 IS UNBLOCKED — SHIPPED ON
The rung E15 measured and could not pay for. Same replay,
`transit_reach_s_per_km` at 11.0:

    old sphere deck    103 crashes   4.60/min
    terrain deck        20 crashes   0.89/min   <- under Chad's signed 0.94

`transit_reach_s_per_km = 11.0` now ships. Walk-back: 0.0.

### E16.X BUILT, MEASURED, AND DELIBERATELY NOT SHIPPED (do not rebuild these)

    arm                                          crashes   note
    A0 shipped                                     37      the baseline
    A1 air-seek dive OFF (dive_gamma 0)            33      -11%, not the defect
    A2 E11 runway fade (dive_agl_m 800)            36      NULL again, now on a
                                                           fixture that can see
    A3 "look before you dive" (a downward          37      NULL TO THE DIGIT:
       atm_frac probe gating the seek)                     near a dome the air
                                                           below IS better, so
                                                           the probe always
                                                           said yes
    the ballistic HOP (climb, then coast)          32-37   worse where it
                                                           counts: thin-air
                                                           seconds 501 -> 1097
    the LOW-CROSSING governor (cruise the lane)    38-42   unnecessary once the
                                                           deck is fixed, and
                                                           it cost the tunnel
                                                           offensive its whole
                                                           run budget (strike
                                                           RUN 277 s -> 0 s)

★ The two E16 mechanisms were REMOVED rather than shipped off: both are
measured harmful, and a dial that is only ever wrong is a trap for the next
session. Their numbers live here instead.

### ★★★ E16.T TWO DEFECTS THE RUNG'S OWN GATE HAD, AND BOTH ARE THE LADDER'S LAW
**(1) P-H NEVER READ THE SHIPPED DECK DIAL.** The first cut carried the deck's
frame in `ArmCfg` alone, so `build_world` kept the struct default and the
probe's arm labelled **SHIPPED** flew a world the game does not ship. It went
red honestly (E15's rope on the old deck = the 103-crash arm) and that red is
what caught it. ★ **The rung about a constant that stopped describing the
world committed exactly that defect in its own fixture.** Fixed at the source:
every atmosphere field is copied from config in `build_world`, and an arm
OVERRIDES (tri-state: -1 = whatever ships), never DEFINES. The tape-7 arms now
pin the WORLD as well as the dials, because tape 7 was flown on the bare-sphere
deck with the rope off.

**(2) TWO GATE CLAUSES INVERTED WHEN THE OFFENSE STARTED WORKING.** `E12: the
enemy's pump offense does not regress below tape 7` asserted raw
`enemy_raidduty_s` and `enemy_near_s`. Both counters only accrue **while the
target pump is alive** — `arm_raid` issues no order against a dead pump and the
credit loop skips one — so the arm that TAKES BOTH PUMPS AT MINUTE SIX banks
fewer of both than the arm that never gets there:

    arm            raid duty   near   pumps
    tape-7 world     2358 s   114.7   surface 60% alive at the buzzer
    shipped (E16)    2028 s   101.2   BOTH DEAD, first at 6.0 min

★ E12 had already written this warning for on-station seconds ("it is an INPUT,
not a score — read PUMPS DOWN") and the same trap was sitting in two other
counters wearing different names. **RE-DERIVED, NOT BENT**: both clauses are
now rates per PUMP-ALIVE SECOND — while there was something to raid, how much
of the wing was on the job — which is what they always meant and which an early
kill cannot game. The E12.1 backfill A/B carries the same normalization.

### E16.N WHAT THIS RUNG DOES **NOT** FIX
  * The tunnel-net collisions — 12 of the 18 remaining wrecks, near-level, in
    FULL air, underground. Now the largest single crash source and the obvious
    next rung.
  * ⚠ THE RENDER MIRROR IS NOT TERRAIN-RELATIVE. `render/air_field.h` (and its
    GLSL transliteration) keep the bare-sphere deck shell, so with this dial on
    the VISIBLE deck haze and the FLOWN deck disagree by up to the local relief.
    The visible atmosphere is bubble-only by design and the deck is invisible,
    so this is a documented divergence, not a fork of anything on screen —
    `test_air_field.cpp`'s cross-check builds its own field (flag false) and
    stays exact.
  * ⚠ P-H'S OWN BLIND SPOT, NAMED: the fixture's terrain is `uniform_field(300)`
    — exactly at the old deck's death altitude (its fade ends at 320 m), so on
    the shipped deck the probe has NO breathable lane anywhere outside a dome.
    That is harsher than the real DEM (median 165 m) and is why the AI-side
    low-crossing arms could not win there. The terrain-relative arm removes the
    blind spot rather than working around it.
