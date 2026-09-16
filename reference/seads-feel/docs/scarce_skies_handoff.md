# SCARCE SKIES — game-build handoff (for the next agent)

---
## ▶ START HERE — R6 CLOSED, R7 IS YOURS (2026-07-17; Chad: CHECKMARK, "positive and hopeful about our direction, the game loop and the mechanics")

**State: R6 / the ATMOSPHEREFIELD is CLOSED — flown + approved by Chad, kernel round SIGNED.**
Air is now SPATIAL: town "bubbles" you fly out of, the soft wall EMERGING from thinning ρ
(thrust + lift + authority starve together). `sandbox/fields-forge` @ tag
`game-R6-atmospherefield`, gate **557/557**, goldens **0**. Full pedigree: Fable-BEFORE
GREENLIGHT+4-conditions → build → adversarial rd1 SOUND-WITH-FIXES (2 P1s folded: a
`control::step` wiring AT + the HUD G-meter spatial migration) → rd2 CONFIRMED-GREENLIGHT.
You are already on `sandbox/fields-forge` (the main worktree) — read `scarce_skies_program.md`,
the ledger tail, then run the loop from **R7 · Escape-energy telemetry + the bandit bait predicate**.

**How R6 landed (do not re-litigate):** the spatialized primitive is **`atm_frac`**
(`sim/aero.h::atm_frac_at(position,env,p)`), NOT `rho_at` — the plant hoists ONE `f_atm` for
both thrust AND q. null ⇒ literal `atm_frac(altitude(pos,p),p)` bit-identical; live ⇒
`atm_frac(alt)·(1−∏(1−uᵢ))` complement-product union (deck + spherical-cap bubbles via
`atm_falloff`, exact 0/1 plateaus). `control::step` env was ALREADY threaded (R3) — the feared
control/ firewall exception was retired code. `sim/fields.h` holds `AtmosphereField` (H1 single
source). App: **B key** toggles the field live, spawn is at the bubble CENTER, `[atmosphere]`
keys, bezel BUBBLE tag + AIR%/THIN-AIR plate. The drone stall guard stays altitude-based
(config-time, no position — documented).

**Your R7 moves (MECHANICAL — pure shell off state, goldens unmoved):**
1. `E_spec` + a projected-2 s E=0 crossing exposed READ-ONLY to the shell — CONSUME
   `sim/fields.h::specific_energy` / `binding_energy_above`, **never re-derive** (H1).
2. Bandit `will_escape(state, t)` bait predicate (difficulty-gated) — today a baited bandit
   crossing E>0 keeps its autopilot (the designed carry-in from R5).
3. Adds `[bandit] escape_project_s`. GATE: green + goldens unmoved (shell only).

### ★ Chad canon from the R6 fly (2026-07-17) — fold as you reach each:
- **Escape-sky TRANSITION ZONE (do this in R7 / as an R5-revisit):** replace R5e's INSTANT
  tick-sever cliff with a SMALL transition — a *choking engine* as you cross, the tag ramping to
  red, felt like air thinning at the bubble edge/deck (the emergent mush he just approved). A
  short choke-and-fade band ABOVE the crossing, THEN the no-return latch. Emergent, not a scripted
  push. Folded to MASTER_PLAN §3.B.
- **Campaign bubble map (MASTER_PLAN §2.5, REDEFINED — supersedes the 4-bubble sketch):** TWO
  factions, each with a marked MINE = its tunnel entrance. **SUDBURY faction** (bubble = City of
  Sudbury; entrance = **Murray Mine**) vs the **CHELMSFORD COALITION** (bubble = Azilda +
  Chelmsford Central + Dowling incl. Onaping + Levack; entrance = **Errington Mine**, SW of
  Chelmsford = the Phase-3 tunnel mouth). Tunnel = the back door between them. Opens: player
  faction, mine coords, one-bubble-vs-cluster for the coalition.
- **Tunnel depth = 2000 m** (Chad re-confirmed, reverted a 1 km idea back to 2 km ≈ **Creighton
  Mine**). **THE BLACK STOPE** = the full-atmosphere dogfightable CHAMBER at the bottom (the
  Day-5 chamber spike, now named) — a **Blender HERO**. **Errington Mine RUINS** (buildings +
  conveyor belts) = heroes too. **Chad will provide a reference picture around the tunnel.** These
  feed Phase 3 (R8–R11) + the `blender-hero-forge` skill.
- **⚠ HEADS-UP — a V4 FLIGHT KERNEL effort is starting in a SEPARATE SANDBOX (Fable 5 will commit
  changes there).** The frozen kernel may evolve to v4; watch for a kernel reconciliation when it
  lands. Keep `fields-forge`/R6 clean + committed so it does not tangle. (A parallel
  ballistics/celestial agent ALSO commits on this branch — commit R6/R7 by EXPLICIT PATH, never
  `git add -A`.)

**The escape-sky settlement (Chad-ruled R5, do not re-litigate — the transition-zone note above
REFINES the felt crossing, it does not undo the loss):** taper `[gravity] 4000 / 405.5`
(v_esc(4 km) = 100.0, HIS numbers); crossing E_spec = 0 with the field live is a GAME RULE loss —
`app::tick` severs the controls that tick (`LoopState.escape_claimed`, the with_bias seam),
falling back bound does NOT free the plane, only crash-respawn or field-off clears; HUD = amber
ESCAPE SKY / red NO RETURN (the tick's own latch) + the bezel tag; T toggles the field live;
`enabled = false` stays the shipped startup default (his call to flip).
`sim/fields.h` is the single source (`g_at` / `binding_energy_above` / `specific_energy`) — R7
consumes THESE, never re-derives.

**R6/R7 carry-ins:** the ~8-consumer ρ-migration (above) · weapon-g fork + pipper (move
TOGETHER) · projectile bare-sphere cull · tracers-pierce-houses (collider query ready) · bandit
building avoidance · **bandit escape** (a baited bandit crossing E>0 keeps its autopilot today —
lands with R7 `will_escape`) · **escaped-drift resolution** (a claimed drifting plane never
crashes; the loss needs an end — Apparition/respawn timer; Phase 6 economy prices it).

**Traps that bit THIS session (fuller corpus: docs/lessons.md + the closed blocks below):**
- **Non-ASCII in a Catch2 test name = ctest silently never runs it** ("No test cases matched"
  reads as FAILED but nothing ran). Bit TWICE in one day (§/— both). ASCII-only test names.
- raylib's `TakeScreenshot` DROPS directory prefixes — `--smoke N shots/x.png` writes
  `./x.png`; move it after.
- ctest `BAD_COMMAND` on a fresh exe = transient Windows Application Control block: rm exe,
  relink, re-run — not a red gate.
- Commit by EXPLICIT path only; the parallel ballistics/celestial agents' dirty files
  (`render/sky.h`, `docs/ballistics_*`, `generated/gen_log.jsonl`, `offline_tool/ballistic_*`)
  must survive untouched.
- An instrument that renders a PROJECTION as a verdict gets disproven by the pilot on the
  first flight — state the conditions under which the read is a theorem, or enforce it (the
  R5c→R5e arc).

**NOT PUSHED:** everything past `origin/sandbox/solid-ground`'s last push. Chad holds the
master merge deliberately — do not push or merge unasked.

---
## ⛔ (CLOSED 2026-07-15 EOD — Chad: "perfect!"; R5-FLY-5 signed the kernel round; kept for the R5 record) HALT @ R5-FLY (2026-07-15) — GravityField BUILT + double-reviewed; Chad ACTIVATES, flies the felt ceiling, SIGNS the kernel round

**State:** `sandbox/fields-forge` @ `40bdf61ac` (tag `game-R5-gravityfield` @ `d23846c92` + two
fold commits + R5b), gate **540/540** (528 + 12 new gravity ATs), goldens **0**, `[gravity]
enabled = false` SHIPPED — the field exists but the sky is still constant-g until Chad flips it.

**R5-FLY-1 already happened (2026-07-15): Chad flew with the field OFF** (enabled was still
false on disk — a config edit was the wrong activation surface) and reached 6500 m on ordinary
thin air, unsure whether the escape was on. **R5b landed same-day (his ask):** the **T key
toggles the field LIVE** (debug-key class, same as K/season — env is data sampled per tick) and
the bezel title shows **"ESCAPE SKY"** whenever the field is live, from config or key (visual
smoke `shots/r5b_smoke.png` confirms the tag). Params load unconditionally, so his `[gravity]`
dial edits apply through the toggle too.

**R5-FLY-2 also happened (same evening, field ON): two rulings, both BUILT as R5c
(`856870421`, gate 540/540, goldens 0, smoke `shots/r5c_smoke.png`):** (a) the bezel strip was
too subtle → a **HUD plate under the flaps gauge**: amber **ESCAPE SKY** while the field is
live and you're bound, **red NO RETURN** once E_spec > 0 (the R7 escape telemetry pulled
forward — app-side hysteretic latch off the kernel's own `sim::specific_energy`, ON at E > 0 /
OFF below −100 J/kg; T mid-drift is the rescue and flips the plate back). (b) the §3.B design
pair was too high/hard ("had to get to like 8000 m before I was drifting away") → **`[gravity]`
retuned to his numbers: h_g0_m 4000 / sigma_g_m 405.5 ⇒ v_esc(4000) = 100.0 exactly**; 222 at
2 km, ~243 at 1.5 km (a redline zoom now escapes from ~1.5 km up — combat escapes are PRESENT,
his stick judges), deck safe at 297. GravityField struct defaults stay the §3.B pair (the
analytic ATs pin the spec shape); the config owns the tune; loader-test pins updated in
lockstep. **The R5-FLY HALT still stands: felt-ceiling verdict + kernel-round sign-off.**

**R5-FLY-3 (same evening): the R5c tag LIED** — "it says no return but then I dove down and
recovered": E > 0 is irreversible only in VACUUM; with air, drag bleeds the surplus (good
gameplay, wrong label). **R5d (`e56e0da5c`) = the honest three-stage tag:** amber ESCAPE SKY
(bound) / orange **ESCAPING** (E > 0, a dive still saves you) / red **NO RETURN** (E > 0 AND
`atm_frac < [gravity] no_return_atm_frac` — lift/drag/thrust all dead, conservative field
freezes E > 0: provably permanent). New key `no_return_atm_frac = 0.01` (~11 km shipped), band
(0, 0.5]. Gate 540/540, goldens 0, smoke `shots/r5d_smoke.png`. Lesson logged in the ledger:
a projection-vs-actual instrument verdict must state the conditions under which it is a
THEOREM, or the pilot disproves it on the first flight.

**R5-FLY-4: Chad REJECTED the dive-rescue ("no I want no return to be at 4000m at 100m/s") →
R5e (`cb943393d`, gate 545/545, goldens 0) = the ESCAPE CLAIM game rule:** the first live tick
E_spec > 0 with the field active, `app::tick` latches `LoopState.escape_claimed` and hands the
plant NEUTRAL `Inputs{}` (dead stick, throttle down) at the with_bias post-cascade seam — both
modes. Falling back bound does NOT free the plane; only crash-respawn or field-off (T) clears.
HUD is two states again (amber ESCAPE SKY / red NO RETURN = the tick's own latch);
`no_return_atm_frac` REMOVED. Frozen path structurally unclaimable (env/grav null). 6 ATs in
test_instructor_tick.cpp. **Carry-in: bandit escape (a baited bandit crossing E>0 still flies
its autopilot) lands with R7 will_escape.** Em-dash-in-test-name lesson bit a SECOND time
today — ASCII only.

**Review chain (all fresh-context):** Fable-BEFORE on the seam plan = SOUND-WITH-FIXES (all
folded: BOTH radial tripwires taper-guarded; tail clip x ≥ 26 makes the ~46 km subnormal shell
unrepresentable; in-band erfc pin + independent Simpson cross-check so a σ↔σ√2 bug can't pass) →
adversarial rd1 = SOUND-WITH-FIXES (P1 mutation-proven: a thrust-taper leak survived all 538
tests — folded as the in-band single-step DIFFERENTIAL leg: dv == the gravity delta exactly,
rotation zero-fork; P2 folded: tripwire relaxation scoped `alt > h_g0`, fight band keeps full
Section-1 strength even ACTIVE) → rd2 = **CONFIRMED-GREENLIGHT** (the P1 kill re-proven by live
mutation: fork 4.4e-3 vs margin 1e-12; tree verified restored).

**The mechanism (all single-source in `sim/fields.h` — R7 MUST consume these, never re-derive):**
`g_at` (null ⇒ the literal `p.g`; full g through the fight band; Gaussian above `h_g0`; EXACT 0
past the x ≥ 26 clip), `binding_energy_above` (closed erfc form; null ⇒ +∞ = infinitely bound),
`specific_energy` (E > 0 ⇔ escaped, exact for arbitrary trajectories — central conservative
field). Escape table verified: v_esc(0) = 367.4 (deck safe forever), v_esc(4 km) = 237.7 vs
redline 245 (the knife's edge), v_esc(5 km) = 192. Keys: `[gravity] enabled / h_g0_m 5000 /
sigma_g_m 1500` (bands [1000, 20000] / [50, 10000]). `env_ptr` now gates on
`Environment::any_live()` — a grav-only activation cannot silently no-op (R6/R11 inherit).

**⛔ NEXT ACTION = Chad:** set `[gravity] enabled = true` in config/game.toml, fly the felt
ceiling (checklist in the R5-FLY ledger row; the reply to Chad carries it INLINE per his
standing rule), tune `h_g0_m`/`sigma_g_m` on the stick, and SIGN the kernel round (the g_at
plant seam + the taper-scoped tripwire relaxation + the tail clip). Soft-lock note (same class
as the prop-break stranding): an ESCAPED player never crashes ⇒ drifts forever — if Chad wants
a manual respawn key, that's a new ask, park it with him. **After approval → R6 AtmosphereField
+ the ρ-migration** (~8 consumers, MASTER_PLAN §3.C census — every one migrates or it's a live
H1 fork; AT-18b gets the spatial-ρ leg).

**Traps this rung added:** non-ASCII in Catch2 test names silently breaks the ctest name-filter
round-trip on Windows ("No test cases matched" reads as a FAIL, but the test never ran — two
legs hit this; ASCII-only names). The climb-AT escape criterion is ENERGY + band-clearance at
t = 90 s, never "altitude diverges" (v_∞ = 32.8 m/s needs 392 s to reach 20 km).

---
## ⛔ (CONSUMED 2026-07-15 — R5 built same-day by the next agent; kept for the traps + carry-ins) START HERE — PHASE 1 CLOSED, R5 IS YOURS (2026-07-15 end-of-day; Chad: "prep the handoff to the next fable agent, set them up for success")

**State: R4 / Phase 1 SOLID GROUND is CLOSED — flown and approved by Chad through EIGHT fly
iterations.** `sandbox/fields-forge` is CREATED at the approved green HEAD **`652c168aa`**
(gate **528/528**, goldens 0, projection lock unchanged). Your first act:
`git checkout sandbox/fields-forge`, read `scarce_skies_program.md` (the harness — rules,
tuning contract, ladder, HALT gates), then run the loop from **R5 · GravityField**.

**Your R5 opening moves, in order (the program's own steps):**
1. **Step 0 is MANDATORY here** — first unproven kernel seam of the phase: spawn a fresh
   `model=fable` agent to red-team the Phase-2 seam plan BEFORE building (the §3.B taper
   integrator band, the config-flag activation path, how the climb-till-you-die AT pins the
   escape numbers). Log a `PLAN` row.
2. R5 = config-flagged radial-g taper (`[gravity] h_g0_m, sigma_g_m`, MASTER_PLAN §3.B):
   MECHANISM + the scripted climb AT verifying the §3.B escape numbers (~238 m/s from 4 km
   vs 245 redline). A deliberately-activated field MOVES goldens BY DESIGN — that
   activation is Chad's HALT, not yours to keep.
3. `/adversarial-review` (fresh context) is MANDATORY at this seam. Fold P0/P1, re-gate.
4. **→ HALT: Chad flies the felt ceiling + signs the kernel round.**

**Carry-ins for Phase 2** (all in the program's Phase-2 header): `atm_frac(position)` ~8
consumers (R6, census MASTER_PLAN §3.C), weapon-g fork, projectile bare-sphere cull,
tracers-pierce-houses (collider query ready for `weapon::fire_tick`), bandit building
avoidance.

**Chad's end-of-R4 rulings you must honor:**
- **GROUND EFFECT: DEFERRED** ("later once the whole map is done") — do not build it; takeoff
  WITH FLAPS is his confirmed technique (combat ~37 m/s / landing ~34 vs ~41 clean; the math
  is in the R4-FLY-8-1 ledger row). `[ground] friction` + `aircraft.toml T_max` stay HIS dials.
- **PARKED-ELEVATOR display discrepancy: DEFERRED** (cascade suspends aim-pursuit while an
  override is held — correct in flight, visual-only at rest). Both are in the program's
  DEFERRED section now.
- Fly checklists go INLINE in the reply to Chad, never only in this doc (standing rule).

**Traps that bit this phase (fuller corpus: docs/lessons.md + the program's Setup notes):**
- ctest `BAD_COMMAND` / "Process not started" on a freshly linked exe = a transient Windows
  Application Control block: rm the exe, relink, re-run — NOT a red gate.
- Generated GIS scalars: read the BAKE for units (`GisLake.span_m` = sqrt(area), NOT a radius).
- A per-edge contract tested on a one-edge fixture degenerates to a latch test — force the
  second edge.
- Commit by EXPLICIT path only (parallel agents share these branches; `shots/` is gitignored).
- Thumb-button codes are `[input]` game.toml keys; the "MB held: n" HUD line diagnoses any
  driver mismatch — never guess a mouse mapping again.

**NOT PUSHED:** everything past `origin/sandbox/solid-ground`'s last push. Chad holds the
master merge deliberately (kernel reconciliation, not housekeeping) — do not push or merge
to main unasked.

---
## ⛔ (CLOSED — Chad approved: takeoff-with-flaps works; ground effect + parked-elevator DEFERRED) HALT @ R4-FLY-8 (2026-07-15) — fly-7 verdicts built same-day; Chad re-flies, then FORK fields-forge

**State:** `sandbox/solid-ground` @ `e71ddf1de` (tag `game-R4fly7-fixes`), gate **528/528**,
goldens 0, review GREENLIGHT. Chad's fly-7: brake mechanics GOOD, BRAKE tag GOOD; three
issues, two built, one is HIS DECISION:

1. **"Won't let me take off or taxi" — two suspects, both now diagnosable on sight, no
   guessing:** (a) a hard-brake **PROP BREAK** (by-design dead stick — engine 0 kills
   thrust with almost no signal): a red **ENGINE OUT** plate now lights right of the flaps
   gauge whenever `damage.engine == 0`. (b) **thumb-code mismatch**: the R4i speculative
   `BACK(6)` down-bind could brake-on-throttle-up if his driver fires BACK on the forward
   thumb — codes are now `[input]` game.toml keys (up 4,5 / down 3, second slot UNBOUND),
   and an **"MB held: n"** HUD line prints the live code of any held button: read → set →
   re-run. `input/` stays config-free (caller-built `input::ThumbBinds`).
2. **"Splash is one small poof, should last the landing until stopped"** → the burst
   RE-SPAWNS while grounded above `[fx] rolling_min_speed_ms 3.0` (0.6 s cadence via the
   existing rate-limit scan; material re-tested per spawn so ice→shore switches
   splash→dust; dies with the roll). 0 = touchdown-only.
3. **"Pressing ailerons deflects the elevator too" (parked) — DIAGNOSED, NOT BUILT, Chad's
   call:** the rig map is clean (`elevator ← ctrl.pitch` only). The emitted PITCH command
   itself changes when a roll override is pressed because the cascade **suspends
   aim-pursuit while any override is held** (+ deadzone trim-hold) — correct and approved
   in flight; on a frozen parked plane it reads as cross-coupling. A "surfaces follow only
   the pilot at rest" behavior = a `control/` change = a sanctioned kernel round needing
   his sign-off. Options for him: accept the visual (kernel already makes it inert — dead
   stick at rest), or sanction the round.

**⛔ NEXT ACTION = Chad re-flies** (checklist in the R4-FLY-8 ledger row; reply carries it
inline). **After approval: fork `sandbox/fields-forge` → Phase 2 / R5** (program.md has the
precondition; carry-ins listed in the Phase-2 header + the R4-FLY-7 block below). Prop-break
soft-lock note: a broken prop far from a crash = stranded until a crash-respawn — if Chad
wants a manual respawn key or field repair, that's a new ask, park it with him.

---
## ⛔ (CLOSED same-day by R4-FLY-7 fixes) HALT @ R4-FLY-7 (2026-07-15) — the fly-6 fixes are BUILT; Chad flies, then FORK fields-forge

**State:** `sandbox/solid-ground` @ `2acb55744` (tag `game-R4hk-polish`) + docs commits. Gate
**528/528** (526 + 2 new ATs), goldens 0, adversarial review rd1 GREENLIGHT-WITH-FIXES →
rd2 **CONFIRMED-GREENLIGHT** (P2-1 test fold mutation-proven live). All four R4-FLY-6
verdicts landed:

1. **R4h · speed-scaled brake nose-dip** — `sim/ground.h ground_dynamics` part 3: dip rate
   × `min(1, sp / noseover_full_speed_ms)`. Crawl-stop peaks ~3.8° vs the 8° prop strike;
   braking from 35 m/s still breaks the prop. Dial `[ground] noseover_full_speed_ms 12.0`
   (0 = the old unscaled dip bit-identically; loader band [0, 100]).
2. **R4i · thumb dual-bind** — `EXTRA||FORWARD` throttles up, `SIDE||BACK` down-then-brake,
   BOTH input paths (Windows mice split each thumb across two raylib codes — Chad's
   throttle-up was most likely dead, which read as a stuck brake). Verified NO brake latch
   exists. MB4+MB5 held = throttle 0 + brake = the RUNUP, by design — documented, not fixed.
3. **R4j · HUD BRAKE tag** — red plate left of the flaps gauge, lit iff the EMITTED Inputs
   carry `wheel_brake > 0` (`info.player_inputs`, RA9 — cannot disagree with the wheels).
4. **R4k · touchdown FX** — `app::tick` reports the airborne→GROUNDED capture edge
   (per-edge, bounces re-report; mutation-proven AT); `main.cpp` picks the material
   (winter → ICE / lake equiv-disk r = span/√π → SPLASH / else DUST — span_m is sqrt(area),
   NOT a radius, review P1-1) and spawns a closed-form additive puff ring at the wheels,
   0.6 s rate-limited against the grace micro-cycle. Dials `[fx] touchdown_ref_speed_ms 40`
   / `touchdown_intensity 1.0` (band 0..2 == the render's cap). Smoke:
   `shots/r4polish_smoke.png` (app renders; the burst itself needs a live landing — fly
   evidence).

**⛔ NEXT ACTION = Chad FLIES the four fixes** (checklist in the ledger R4-FLY-7 row; the
reply to Chad carries it INLINE per his standing rule). **After his approval: fork
`sandbox/fields-forge` from this green HEAD → Phase 2 / R5 GravityField** (program.md has
the fork precondition + the ladder). Carry-ins unchanged: `atm_frac(position)` ~8
consumers, weapon-g fork, projectile bare-sphere cull, tracers pierce houses (collider
query ready for `weapon::fire_tick`), bandit building avoidance, grace-bank P2 (Chad's
stick judges), crash animation deferred, hero glTF colliders none, respawn-timing quirk
accepted-for-now.

---
## ▶ (CLOSED by R4-POLISH, same day) R4-FLY-6 verdict worklist (Chad, 2026-07-15, END OF DAY)

**State:** `sandbox/solid-ground`, HEAD past `ba1c7d36f` + ledger commits. Gate **526/526**
(the post-merge union suite), goldens 0. **Ballistics-forge is MERGED (129bc0cb7)** — guns,
bandit combat AI, component damage model, combat audio all live on the Environment seam;
that branch is CONSUMED, combat work continues HERE. Ground consequences live (ground loop /
wing chips via the damage model / brake nose-over → prop break → dead stick), controls need
airspeed, MB4/MB5 thumb throttle, contact 2.45, blue aimer.

**Chad's fly-6 verdicts** — stop mechanics GOOD, dead-stick-at-rest GOOD, wingtip-touch-on-
landing crash REGISTERS (accepted; crash ANIMATION deferred; a respawn-timing quirk noted,
OK for now). Four work items:
1. **Slow braking still tips forward** ("had to be careful to stop without nosing over") —
   scale the nose-dip pitch rate by ground speed in `sim/ground.h ground_dynamics` part 3:
   factor ~`min(1, sp/12.0)` (or a `[ground] noseover_full_speed_ms` dial) so a crawl-stop
   can't nose over but braking from speed still bites. Keep the prop-strike test unchanged.
2. **"Stopped keeps brake on + throttle up won't get me rolling"** — PRIME SUSPECT: his
   mouse's forward thumb maps to raylib `MOUSE_BUTTON_FORWARD` (5), not `EXTRA` (4) —
   throttle-up was simply dead (MB4/SIDE demonstrably worked: he braked). FIX: accept
   `EXTRA || FORWARD` for throttle-up and `SIDE || BACK` for down/brake in BOTH
   `input/raw_input.cpp` + `input/live_input.cpp`. Also note: holding MB4+MB5 together nets
   throttle 0 and keeps the brake (that IS the runup — document, don't "fix"). Verify no
   actual brake latch exists (there isn't — thumb_brake requires MB4 held).
3. **HUD brake indicator** near the ballistics flaps gauge (the color-coded FLAPS
   CLEAN/COMBAT/LANDING strip, centered low in `render/draw.cpp`): show a red/amber BRAKE
   tag when `wheel_brake > 0` — thread the value through `render::FrameInfo` exactly like
   the flaps state (RA9: display-only, read from the emitted Inputs).
4. **Touchdown FX** ("something to indicate touchdown — dust / splash in water / ice
   crystals on snow"): detect the airborne→on_ground capture tick in `app::tick` (the
   transition is visible as `!prev.on_ground && curr.on_ground`), thread a touchdown event
   (position + ground speed) to render, spawn a short particle burst at the wheels.
   Material by surface, v1: within a `kSudburyLakes` center/span circle → SPLASH; season
   winter → ICE CRYSTALS; else DUST. Reuse the combat-FX/smoke puff pattern
   (`render/smoke.*` / the merged combat FX pool). Cosmetic, render-only, no kernel.

**PROCESS (memory saved):** fly checklists go INLINE in the reply to Chad — never only in
this doc. This doc is for YOU (the next agent); the reply is for him.

**Carry-ins unchanged:** grace-bank ground-loop interaction (review P2, Chad's stick
judges); bandits take no ground/wing damage (player-only) + no building avoidance; tracers
pierce buildings (collider query ready for weapon::fire_tick); crash animation; hero glTF
colliders. After Chad approves fly-6 fixes → fork `sandbox/fields-forge` → Phase 2/R5
GravityField (atm_frac(position) ~8 consumers, weapon-g fork, projectile bare-sphere cull).

---
## ⛔ HALT @ R4-FLY-5 (2026-07-15) — the fly-4 verdicts are BUILT; Chad re-flies

**Chad's R4-TAILS fly verdicts**: road landing "very good" ✅, hillside worked ✅; four asks —
all landed this session (commits `eb69d47ef` tag `game-R4fg-fly4` + folds `a06e9024a`,
`7dc7804fd`). Gate **440/440**, goldens UNMOVED, lock hash UNCHANGED. Fable DESIGN consult
before the kernel edits (2 P0s caught pre-build) + a 3-round adversarial review after
(rd2 empirically REJECTED one of my test folds — the fixed test is now mutation-verified).

1. **"Sunk down in a lake near the shore"** → bake-side SHORE-BANK CAP: land within 150 m of
   water capped at lake-level + 3% grade (shore facet protrusion p90 3.1 → **0.50 m**; the
   M2 normal map stays uncapped so banks keep their steep look). Rocky-narrows tail (~2.3 m
   p99) remains — fly-watch.
2. **"I go right through houses"** → R4f BUILDING COLLISION: the bake already shipped 86,347
   per-footprint colliders; new pure `world/buildings` span-inserted index (exact spherical
   u-span), one crash surface (prism base = the same height field), `env.obstacles`
   null = ghosts. Dials: `[buildings] collide / inflate_m 3.0 / base_margin_m 5.0`.
   Known v1 honesty: equiv-area radii under-cover long warehouses (clip a CORNER and it may
   ghost — raise inflate_m if felt); the glTF heroes (Superstack etc.) have NO colliders yet.
3. **"Need a set of brakes"** → R4g WHEEL BRAKES: hold **B** on the ground. Passthrough like
   throttle; full brake + full throttle holds still (the runup). Dial: `[ground]
   brake_friction 0.35` (0.35 g extra decel ≈ 25 m/s → stop in ~75 m).
4. **"A hard turn made my wings sink below the ground"** → GROUNDED ROLL ALIGN: the airframe
   can no longer roll about its forward axis while grounded (pitch rotate + rudder steering
   fully live; wingtip-above-terrain pinned by AT). Dial: `[ground] roll_align_rate_deg_s
   120` (0 = the old free-roll ground, bit-identical).

**⛔ NEXT ACTION = Chad RE-FLIES** the five-point checklist in the ledger R4-FLY-5 row
(lake shore stance, house solidity, brakes on the roll, hard ground turn, and the standing
`contact_height_m 1.7` stance recommendation). **After his verdict:** R4b proper steering
remains opt-in; else fork `sandbox/fields-forge` from this green HEAD → **Phase 2 / R5
GravityField** (carry-ins unchanged: `atm_frac(position)` ~8 consumers, weapon-g fork,
projectile bare-sphere cull, + NEW: ballistics tracers still pierce houses — the collider
query is ready for `weapon::fire_tick` when the ballistics thread wants it; bandit-AI
building avoidance).

---
## ⛔ (CLOSED same-day by R4-FLY-4) HALT @ R4-TAILS-FLY (2026-07-15) — the R4 deferred items are CLEARED; Chad re-tests everything

**Chad's directive** (2026-07-15, verbatim intent): complete the R4 deferred items, fix the
roads elevated above terrain, do the proper 16-bit DEM bake — all before R5; rigorous
subagent review; he re-tests everything on return. All three rungs LANDED, gate **432/432**,
goldens UNMOVED, projection-lock hash UNCHANGED, Fable-BEFORE (GREENLIGHT-WITH-FIXES, 3×P0
folded) + adversarial-AFTER (**CONFIRMED-GREENLIGHT**, round 2). Commits `50f6e21c1`
(tag `game-R4c-e-proper-bake`) + `9e3e17c95` (review folds).

- **R4c · GENUINE 16-bit DEM** (closes the R1 deferral — rasterio present, cached 16 m
  blend): the bake quantizes the SAME float field to uint16 (**5 mm steps** vs the old
  ~1.4 m — the §3.A ground-roll precision) packed into the PNG's R/G bytes (raylib 5.5
  truncates 16-bit grayscale); `world::dem16_unpack` is the ONE bake↔loader convention and
  a legacy 8-bit gray decodes bit-identically through it (r·257 == (r<<8)|r — no format
  branch to fork). **Proof of shape-safety: new-vs-old DEM max delta = 128/65535 = exactly
  half an 8-bit LSB** — the terrain Chad approved is untouched, only finer. Every sibling
  asset came out byte-identical from the deterministic bake. (HRDEM/LiDAR source is still a
  separate deferred swap — the cache is CDEM-only, Fable P0-1.)
- **R4d · ROADS ON THE TERRAIN** (the "elevated roads" defect): measured root cause — the
  ribbons draped on `radius_at` (the FIELD) while the screen shows the mesh's FACETS, and
  the two diverge **p99 ≈ 6 m, max +60 m** at subdiv 200 (chord/station spacing was <0.1 m
  of it — no ribbon re-bake needed). Fix: **face tiling** (`[planet] tiles = 2` → 24
  watertight meshes, effective 399 verts/face-edge, bit-identical shared edges by
  construction) + **`facet_radius_at`** — roads/trails/rivers now drape on the RENDERED
  surface itself (same field, the mesh's own interpolation: the true anti-fork) +
  `[ribbons] lift_m` 1.2 → **0.45 measured clearance** (committed kink evidence:
  burial p90 0.17 / p95 0.29 / p99 0.82 m — `measure_drape_gap.py` §4b). The universal
  1.2 m hover is GONE; smoke `shots/r4d_low_day.png` shows roads hugging the ground.
- **R4e · FINE LAKE MIRRORS** (stance root fix, the deferred "finer lake-mirror bake"):
  water grid 250 → 100 m (mirror sag 1.04 → 0.17 m), `[water] surface_lift_m` 1.2 → 0.4.
  The stance trade COLLAPSED: the old 1.5-vs-2.7 wheels dilemma is now 1.5-vs-~1.9.

**⛔ NEXT ACTION = Chad RE-TESTS EVERYTHING** (his words). The fly checklist:
1. **Land a ROAD** — it should sit ON the terrain (no hover, no stitching). Dial:
   `[ribbons] lift_m 0.45` (raise → 0.7 if rough segments stitch; drop → 0.25 if hover reads).
2. **Land a LAKE** — stance vs the ice deck (`[water] surface_lift_m 0.4`; watch
   shimmer/holes at grazing → raise toward 0.7).
3. **Ground ROLL** — the 16-bit field kills the 1.4 m quantization bumps (the R1 goal).
4. **STANCE** — `[ground] contact_height_m` stays at his approved 2.0; with the decks
   dropped it now reads ~0.1 proud on decks / ~0.5 on fields — **recommend trying 1.7**.
5. **PERF/look** — tiles=2 doubles planet tris to ~1.9 M (24 meshes); watch fps;
   `tiles = 1` reverts wholesale (drape auto-conforms — it reads the live mesh config).

**Deferred (unchanged owners):** rig-D D.3 wheel GLBs (rig-D thread, `bf109_preview/` is
cross-agent); GisAirstrip rails + landable-lake gating (subsumed by generic contact, v1);
HRDEM/LiDAR source swap (bake-only, when the COG is healthy); R4b ground-roll steering
(Chad opt-in only). **After the fly:** fork `sandbox/fields-forge` from this green HEAD →
Phase 2 / R5 GravityField (the big kernel gate; carry-ins unchanged from the R4 handoff
below: `atm_frac(position)` primitive ~8 consumers, weapon-g fork, projectile bare-sphere cull).

---
## ✅ R4 COMPLETE — SOLID GROUND FLOWN + APPROVED (Chad, 2026-07-15: "its good!")

**Phase 1's "land the plane anywhere" is MET on Chad's stick**: he landed Lake Wanapitei
(rolled out to 25 m/s, took off again), then FIELDS and a ROAD — twice in one life. The
rung closed after four fly-iterations, each a config-or-mechanism response to his verdicts:
- **FLY-1** `4aba8fc9d` — `[ground] contact_height_m` (the CG was constrained to the
  terrain: no gear height existed, the hull buried).
- **FLY-2** `219d9de68` — acceptance tune (max_sink 3→6: 3.0 demanded a <3.4° slope at a
  50 m/s approach, unlandable; slope_limit 15→20: 15 clipped a normal flare's nose-up).
- **FLY-3** `e7020305d` — ROLLING WHEELS (`Driven::Wheel`, app-accumulated tick-derived
  angle, render stays clock-free) + the STANCE TRADE ([water]/[ribbons] render lifts
  2.0→1.2, contact_height 2.0 — one physics surface vs per-material render decks; the
  documented three-way trade lives in game.toml).
**Approved dials** (config/game.toml [ground]): slope 20° / friction 0.08 / max_sink 6.0 /
contact 2.0; world.toml water/ribbons lifts 1.2. Gate 427/427, goldens UNMOVED throughout.

**NEXT RUNG for the loop** (`scarce_skies_program.md`): **R4b ground-roll STEERING is the
cuttable tail — build it ONLY if Chad asks.** Otherwise fork **`sandbox/fields-forge` from
this green HEAD** and start **Phase 2 / R5: GravityField taper (§3.B)** — the BIG kernel
gate: config-flagged radial-g taper (h_g0 5000 m, σ_g 1500 m), climb-till-you-die AT
against the §3.B escape numbers, deep adversarial review on the integrator in the taper
band, then HALT for Chad to fly the felt ceiling. R5 carry-ins from the R3/R4 reviews:
`atm_frac(position,env,p)` is the Phase-2 primitive (~8 consumers, census §3.C);
`weapon::fire_tick`'s scalar g forks from the plant when gravity goes live; projectiles
still cull on the bare sphere (`fire_tick(..., ap.R)` — tracers pierce hillsides, R4
review P2).

**Deferred tails (logged, non-blocking):** finer lake-mirror bake (kills the stance trade
at its root — the ~1.0 m facet sag forces the deck lift); rig-D D.3 wheel GLBs (tyres are
placeholder cubes — the spin mechanism is in and pinned); GisAirstrip rails + landable-lake
gating (generic contact subsumes them in v1); genuine 16-bit source-DEM re-bake (R1,
infra-blocked); fly-watch on the lowered lifts (lake shimmer at grazing angles / road gaps
over rough terrain → raise back toward 1.5 in world.toml).

---
## (CLOSED 2026-07-15) HALT @ R4-FLY — SOLID GROUND IS BUILT; Chad flies the landing

**R4 DONE** (tag `game-R4-solid-ground`) on `sandbox/solid-ground`, gate 426/426, goldens
UNMOVED (all-null bit-identity holds with the ground field LIVE in the build). Chad ruled
the two R4 design questions (2026-07-15): **in-kernel via env.ground** + **touch-and-stick**.

What landed:
- **The kernel's first active Environment field.** `sim/ground.h` — terrain crash
  (`|pos| <= radius_at(dir)`), landing acceptance (gentle sink + upright + slope, all vs
  the terrain normal), GROUNDED touch-and-stick regime (radial surface constraint, rolling
  friction to an exact stop), honest takeoff (release when the airborne integration wins —
  no scripted rotate speed), fell-away/rising-wall direction logic, and the **re-attach
  grace** (sink <= 3·g·dt skips the attitude re-grade — red-team P0-1: a marginal liftoff
  releases precisely at high alpha, so re-grading attitude killed benign rollouts).
- **Single-source held:** `world::HeightField::radius_at` is the ONE elevation query
  (mesh, props, contact); the landing normal is its own central-diff `normal_at`;
  `render::planet_heightfield()` hands main.cpp the ONE post-blur field (never a re-read).
- **R3-review P1 honored:** `env` threaded NO-DEFAULT through `app::tick`/`step_frame`,
  `drone::tick`, `harness::measure_ang_accel_max`; `ClosedLoop` carries an `env` member
  (documented deviation — 114 golden sites fly null BY CONTRACT — with a firing AT).
  Crash predicate: when ground is live the KERNEL verdict rules in app AND drone (one
  crash surface; a sea-level lake touchdown is a LANDING, not the old altitude<=0 death).
- **Tuning contract:** `config/game.toml` `[ground] enabled / slope_limit_deg / friction /
  max_sink_ms / normal_probe_m` (strict loader; cross-check friction < T_max/(m·g) so a
  takeoff roll can accelerate) + `[deck] height_agl_m` Phase-2 placeholder. Chad tunes the
  table, never code. `enabled = false` = the bare-sphere world, bit-identical (A/B).
- **Reviews:** Fable red-team GREENLIGHT-WITH-FIXES → P0 + a directionless-slope follow-up
  FOLDED, re-review **CONFIRMED-GREENLIGHT** (mutations run empirically: grace off/inflated,
  direction sign flip, follow_m factor zeroed — all four kill the suite). 10 ground ATs.

**⛔ NEXT ACTION = Chad FLIES the landing FEEL** (MASTER_PLAN Phase-1 acceptance: land at
an airstrip, on lake ice, on a hillside within limits; take off again). Dials are the
`[ground]` placeholders above — his stick rules them. **Fly-watch (carried P1s, documented
in sim/ground.h):** (1) sink is accepted RADIALLY, so a fast level pass into a landable
slope sticks at full ground speed; (2) the radial constraint means sub-limit uphill rolls
pay no slope gravity; (3) a deliberate knife-edge "grace-skim" landing is representable.
If any offends on the stick, they are the next mechanism rungs. **R4b ground-roll
STEERING is the cuttable tail** (build only if Chad wants it before Phase 2).
After the fly + any feel iteration, the ladder continues at **Phase 2 / R5** (GravityField
taper — the next sanctioned kernel round, `sandbox/fields-forge` forked from this green HEAD).

---
## ⛔ HALT @ R3 (2026-07-14) — autonomous loop reached the kernel boundary
Branch `sandbox/solid-ground` (forked from `world-sudbury` green HEAD). The
`scarce_skies_program.md` loop ran Phase 0 + the mechanical half of Phase 1:

- **R1 DONE** (`b99037c36`, tag `game-R1-dem16`) — 16-bit HeightField pipeline:
  `px`→`uint16`, `sample01`÷65535, loader promotes the 8-bit DEM red channel ×257
  (**bit-identical**: r·257/65535 ≡ r/255, mesh UNMOVED) + a first-principles 16-bit
  precision tripwire AT. ctest 414/414, goldens unmoved.
  **DEFERRED — genuine sub-metre precision** needs a source DEM re-bake, which is
  infra-blocked here: `rasterio` not installed, source is a remote AWS S3 COG
  (HRDEM 1 m / CDEM 16 m), 24000² grid has OOM'd this box. The code pipeline is
  ready; only the data regen (+ a re-drape pass for compiled-in anchors) is parked.
- **R2 DONE** (`49a8a719a`, tag `game-R2-heightfield-module`) — `HeightField` +
  `equirect_uv` moved to a new glm-only `seads_world` lib (`world/heightfield.*`),
  linked by `seads_sim` so the coming `sim::Environment.ground` names ONE definition
  with no render↔sim dependency; `render::` re-exports via using-alias (no fork).
  `kSudburyRibbonPaths` deferred (it's in the generated `sudbury_gis.gen.h`; Phase 4
  moves it when the millwright AI needs it). Pure refactor, goldens unmoved, 414/414.
- **Fable-BEFORE** red-teamed the R3 seam → **GREENLIGHT**, three corrections folded:
  (1) the primitive to spatialize is **`atm_frac(position,env,p)`, NOT `rho_at`** (the
  plant calls `atm_frac` once at `sim/step.cpp:58` and reuses it for thrust AND q);
  (2) the ρ-consumer census **missed `controller.cpp:759`** (the S-dampff `q_eff` site);
  (3) `control::step` takes `const sim::Environment*` with **no default arg** (compiler
  enumerates all sites — a silent default is the Phase-2 fork class).

- **KERNEL BASE RESOLVED** (`078fb588a`) — Chad ruled "sync sealed kernel first". Merged
  main's **`sealed-kernel-v3`** into `solid-ground` (S-ffrad `|position|` ff, all-axes
  damp_ff, aim_sensitivity 0.14). Clean merge except the CLAUDE.md banner; the world thread
  never touched the kernel (firewall held), so controller goldens auto-took the sealed
  values and are now the bit-identity baseline. ctest 416/416.
- **R3 BUILT + adversarially GREENLIT** (`e49d85922`, tag `game-R3-environment-seam`) — the
  ONE sanctioned kernel-v4 round. New nullable `sim::Environment{ground,atm,grav,tunnels}`
  (`sim/environment.h`, all-null); threaded `const sim::Environment*` through `sim::step`
  (before `dt`) + `control::step` (before `dt`), **no default arg** so the compiler
  enumerated all ~95 call sites (now pass `nullptr`); `env` is `[[maybe_unused]]` — R3 is
  pure seam, nothing consumes it yet. **ALL-NULL BIT-IDENTITY PROVEN: goldens UNMOVED,
  ctest 416/416.** Fable-after red-team: **GREENLIGHT, P0 none** (env appears only in the two
  signatures, nowhere in either body — the 17-digit golden checkpoints are a genuine proof).

**⛔ NEXT ACTION = Chad's KERNEL SIGN-OFF on R3** (the sanctioned frozen-kernel evolution;
all-null bit-identical, so zero feel change by construction). Then the loop builds **R4**
(terrain-crash + GROUNDED landing) — where these **R3-review carry-forwards MUST be honored**:
- **P1 (R4, do first): the wrapper-`nullptr` trio must gain explicit no-default `env` params**
  — `app/instructor_tick.h` `tick()` (:250,359,363 + its own `dw/gw=nullptr` defaults),
  `drone/drone.h` `drone::tick` (:264), and the harness `ClosedLoop`/`injector`
  (`test/harness/instructor.h:144,147`, `injector.h:96`, `harness_main.cpp:80,128`). Today
  they hard-code `nullptr` (correct while all-null); once `ground` activates, drones/harness
  that DON'T get the same env fork the crash surface (player crashes on terrain, drones fly
  through). R4 threads the real `env` to all of them, no default.
- **P2 (Phase 2, note): `atm_frac(position,env,p)` is the primitive** (~8 consumers, incl.
  `weapon::fire_tick`'s scalar `g` — rounds fork from the plant when gravity goes live) +
  **doc drift**: `CLAUDE.md`, `SPEC.md`, `docs/TEACHING.md` still show env-less signatures.

R4 itself then HALTs for **Chad to fly the landing FEEL** (slope limit / friction / deck).

---


**Purpose of THIS doc:** hand the next agent everything needed to take the "Scarce Skies"
game plan to **Fable 5 for ONE MORE build review + enhancement pass** before implementation
starts, then begin Day 1. Read this top-to-bottom, then run the FABLE-5 BRIEF (below).

**The canon spec is `D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md`** (NOT in this git repo — it's
Chad's design folder). This handoff summarizes + points into it; the plan is the source of truth.

---

## 1. Where things stand (2026-07-14)

- **The plan is COMPLETE and Fable-reviewed GO.** MASTER_PLAN.md now holds the full game loop
  (`## 2·GL`), the three keystone designs (`§3`: Solid Ground/tunnel, escape-ceiling gravity taper,
  kernel-v4 Environment), the phase roadmap (`§4`), and the one-week execution plan (`§4·W`).
- **A thorough Fable-5 engineering analysis already ran** (per-system feasibility + risks + build
  order). Verdict **GO**. Its corrections are ALREADY folded into the plan:
  - the `rho_at(altitude)→rho_at(position)` migration is **~8 consumers, not 4** (census in §3.C) —
    every one migrates together or it's a live H1 fork; AT-18b already mutation-tests the ρ-fork.
  - the bubble edge is **thrust/lift STARVATION** ("mushy, engine dies, nose drops, you sink"), NOT
    a restoring "slide-back" wall (that would be scripted) — §2·GL.
  - **ground-roll STEERING is a cuttable Phase-1 tail**; the honest week-scope is land→GROUNDED→stop.
  - the **egg pit is stamped into the DEM offline** (a 2.5D radius dip, no overhang — keeps sim/render
    single-source), THEN quad-skip the actual hole + collar.
  - `HeightField::px` is `uint8_t` → the **16-bit DEM is a struct + `sample01` CODE change**, not
    offline-only.
  - threading `env` into `control::step`'s signature is a **structural control/ change** that earns
    its OWN adversarial-review round (not a Phase-1 rider).
- **Both tunnel anchors are Chad-CONFIRMED and on real geography** (locked in MASTER_PLAN §2·GL):
  - Tunnel ORIGIN = **Errington Mine #3, Balfour Township = `46.54611, -81.22083`** (Chelmsford side).
  - Egg-pit PORTAL = **Murray Mine open pit = `46.5144, -81.0657`** (MR 35 by Azilda).
  - Marker recon shots: `shots/mark_errington_mine3.png`, `shots/mark_murray_pit.png` (the pits do
    NOT render yet — no DEM stamp; that's the Day-4 offline step).
- **NOTHING is built. The flight kernel (`sim/`+`control/`) is FROZEN + untouched.** All world-art
  work this session (whole-map buildings, CC7 braided slag pour, church setback) is committed on
  `sandbox/world-sudbury` and **AWAITING Chad's fly** — do not conflate; the game is a NEW thread.

## 2. Open ⚑ decisions (Chad rules these on Day 1 — do NOT guess)
- Global deck height (plan: 120 m AGL, terrain-relative).
- Tunnel depth (Chad modified 2 km → **1 km**).
- Regional Rd 15 alignment for the tunnel route (Chad's ~25-yr memory — verify on the fly).
- Felt tuning intent: bubble-edge softness, pilot-O2 duration, raid distance (all Chad-stick-ruled).
- One-chamber-two-corners confirmed; single entrance confirmed ("only one way there").

## 3. The build order (dependency-honest; MASTER_PLAN §4·W has the day-by-day)
1. Phase 0: green build + Chad's ⚑ calls + amend the plan + **16-bit DEM re-bake** (struct + sampler).
2. HeightField → neutral `world/` module (+ move `kSudburyRibbonPaths` while you're there).
3. `sim::Environment{ground,atm,grav,tunnels}` nullable, **all-null bit-identical** (goldens unmoved),
   adversarial review. `rho_at(pos,env,p)` must literally CALL today's `rho_at(alt,p)` when `atm==null`.
4. Terrain-crash → landing → GROUNDED/stop (**Chad flies**: airstrip, lake ice, hillside). Ground-roll
   steering only if time survives.
5. Day-4 throwaway spike: DEM pit-stamp + portal quad-skip + collar + hysteretic "inside" predicate.
6. Day-5 spike: chamber volume + destructible instanced lamps with a **single-source `light_at()`**
   feeding BOTH the shader AND a stub AI-visibility readout (prove concealment is real, not placebo).
7. Phase 2 (fields: gravity taper + AtmosphereField + the ρ-consumer migration + AT-18 spatial legs) =
   NEXT week's gate — the big kernel round.

## 4. Non-negotiable guardrails for the next agent
- **KERNEL FIREWALL:** the world thread NEVER touches `sim/`/`control/`. Phase 1 is the ONE sanctioned
  kernel evolution — done deliberately, adversarially reviewed, and **Chad owns the landing FEEL** (his
  stick, one dial at a time; never tune to harness numbers). See CLAUDE.md `## Lessons learned`.
- **SINGLE-SOURCE / no fork (H1):** a sim-semantic expression lives ONCE. The ρ-migration and
  `light_at()` are the two live fork risks — pin them (one definition, consumed everywhere).
- **All-null == bit-identical** is the Environment safety proof: goldens MUST NOT move until a field
  is deliberately turned on. Gate every step (`.claude/hooks/gate.sh` = build + ctest).
- **Adversarial review** (fresh context, `/adversarial-review` or a Fable round) at every kernel gate.
- Commit by explicit path; a PARALLEL celestial/ballistics agent also commits here — never `git add -A`.

## 5. ▶▶ THE FABLE-5 BRIEF — run this next (the point of this handoff)
Hand Fable 5 (a fresh `model=fable` agent) ONE MORE build review + enhancement pass. It is NOT
re-litigating the design (already GO) — it is sharpening EXECUTION for the imminent Phase 0+1. Give it:

> You are Fable-5. The "Scarce Skies" plan (`D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md`) has your
> prior GO + folded corrections. Do a FINAL pre-implementation BUILD REVIEW + ENHANCEMENT of the
> IMMINENT work (Phase 0 + Phase 1 Solid Ground + the two Day-4/5 spikes). Read MASTER_PLAN.md §2·GL,
> §3.A, §3.C, §4·W and the seams: `sim/step.cpp`, `sim/aero.h` (the `rho_at`/`atm_frac` single source
> + the ~8 consumers), `control/controller.{h,cpp}` (the inversion + the `control::step` signature),
> `render/sphere_param.h` (HeightField/`radius_at`/`px` uint8), `world/props.*` (ChunkBound cull to
> reuse), `render/building_asset.*` (the sectioned-binary pattern for tunnel/chamber geo), the AT suite
> (esp. AT-18a/b ρ-fork, AT-12 energy, AT-0 frame). Deliver, concretely:
> (1) an EXACT `sim::Environment` signature + the `rho_at(position,env,p)` shape that is provably
>     all-null bit-identical, and the FULL ordered migration checklist of the ~8 consumers (file:line)
>     with the AT that guards each;
> (2) the smallest honest Phase-1 landing kernel (crash predicate + GROUNDED + stop) — what's IN vs the
>     cuttable ground-roll-steering tail — and the exact new ATs it needs (crash, land-on-slope, takeoff);
> (3) the DEM-pit-stamp + portal-surgery + hysteretic in-tunnel predicate plan (the watertightness of
>     the crash-exemption boundary is the correctness risk — spec the funnel hysteresis + its AT);
> (4) the `light_at()` single-source design (CPU+GLSL, one buffer/one falloff) + the signature-color
>     validation that proves AI-vs-render agree;
> (5) ENHANCEMENTS — anything that de-risks the week or improves the design you haven't said yet
>     (better sequencing, a cheaper spike, an AT that catches a subtle fork, a firewall-safer shape);
> (6) top 3 things that will BITE if rushed, and a final GREENLIGHT/HOLD for starting Day 1.
> Be the honest engineer; cite files. ~800 words.

Fold Fable's output back into MASTER_PLAN.md (and note it here), then Chad's Day-1 ⚑ calls start the build.

## 6. Pointers
- Canon: `D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md` (+ the lore source docs beside it).
- Kernel seams: `sim/step.cpp`, `sim/aero.h`; `control/controller.{h,cpp}`; `render/sphere_param.h`.
- Reuse patterns: `world/props.*` (chunk cull), `render/building_asset.*` (binary asset),
  `render/ribbons.*` + `kSudburyRibbonPaths` (trails/pipelines), `weapon/ballistics.*` (HP/hit-test),
  `render/lights.*` (instanced glows = gas lamps).
- Process: CLAUDE.md (`## Lessons learned`, `## Process`, kernel firewall); `docs/HARNESS.md`;
  `.claude/hooks/gate.sh`.
