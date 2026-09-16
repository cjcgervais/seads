# AI PHASE-2 FIX SPEC (Fable, 2026-08-06) — tape-attributed fixes

Evidence base: build-play/conquest_tape_1.jsonl (14:00 match, sig VERIFIED)
analyzed via tools/ai_tape.py; docs/ai_primary_data_handoff.md §4 findings.
Every fix below is keyed to a tape fact. HARD RULES unchanged (seal-pass):
Sonnet never commits, never touches sim/, control/, goldens, test/harness/.
ASCII test names. No clocks/rng in tick paths.

## FIX-F2 — CRASH-RESPAWN PLACEMENT (the vacuum respawn-strand pen)

TAPE FACT: 95 dc events in 14 min (i=7: 28, i=6: 22, i=9/i=2: 15/16 — both
factions); crash altitudes 20-145 m clustered ~[+14000, -2600..+3200,
-1100..+3200] — the stock +X scatter sits ~1-5 km OUTSIDE the Sudbury bubble
edge in thin air, so any crashed drone respawns into vacuum, mushes, crashes
again, forever. This IS Chad's "loitered ~2 km east of the sudbury bubble".

FIX (app layer only): when conquest is live, a crash-respawned drone is
RELOCATED into its own faction's breathable air on the SAME tick.
- drone::tick already reports the crash: `DroneTickResult.respawned`
  (currently discarded at app/instructor_tick.h ~1218).
- Extract the per-drone placement math from app::scramble_to_surviving_air
  (instructor_tick.h ~392: faction_ellipse -> tangent basis -> golden-angle
  bearing at 0.6*b arc -> level_state_at facing the dome centre) into ONE
  shared helper `app::place_in_faction_air(drone::DroneState&, int faction,
  const world::FactionGrowth grow[2], const sim::AircraftParams&,
  const app::DroneWorld&, int slot)` — scramble_to_surviving_air MUST call
  the same helper (single source; its loop supplies its own slot counter).
- In the drone loop: `const drone::DroneTickResult tr = drone::tick(...);`
  then if `tr.respawned && cq != nullptr`: faction =
  combat::maverick_faction(d.spawn_index); if that faction's dome is alive
  (grow[f].radius_scale > 0) place there, else place in the OTHER faction's
  dome if alive (the scramble rule), else leave the stock respawn (both
  domes gone — endgame, air exists nowhere). Use the drone's spawn_index as
  the golden-angle slot so placement is deterministic per pilot.
  Set d.prev = d.curr after placement (no interpolation streak).
- Null cq => bit-identical (the standing firewall).
- NOTE (flagged for Chad, do NOT implement): an alternative ruling is
  "terrain crash = death in conquest" (parity with gun kills). Chad has not
  ruled; this fix keeps the respawn but ends the vacuum pen.

TESTS (test/unit/test_conquest_respawn.cpp, new TU wired like siblings):
1. "conquest crash respawn lands in own faction air": build a cq world with
   both domes alive, force a drone crash (drive it into altitude<=0 via a
   dive with terrain null or use respawn_in_place + the app relocation call
   path through app::tick with a scripted crashing drone — the practical
   fixture: call the helper directly AND one app::tick integration leg
   where a drone's curr is set 1 m below the crash sphere so its tick
   crashes); assert post-tick position is inside its faction ellipse
   (world::faction_ellipse containment via the atm sampler or arc check)
   and atm_frac_at(pos) is near full air (>= 0.9) when the field is built.
2. "dead own dome relocates to the survivor": radius_scale[own]=0 =>
   lands in the other ellipse.
3. "both domes dead leaves the stock respawn": radius_scale both 0 =>
   position == the stock spawn_state scatter (bitwise vs a reference call).
4. "no conquest is bit identical": cq=nullptr, same crash, position ==
   stock respawn exactly.
5. "scramble and crash-respawn share the placement": call
   scramble_to_surviving_air and the helper with the same inputs/slot,
   assert identical positions (the single-source pin).

## FIX-F3 — THE RAID PAUSE GETS ITS OWN DIAL

TAPE FACT: enemy raid order on the player's pump dropped at rng_p = 6010 m
== dparams.engage_range (6000). The pause radius is welded to the FOE-
ASSIGNMENT ranges (instructor_tick.h ~1170: raid_range_ok reads
dw->dparams.engage_range/disengage_range), so the raider flees while the
player is 6 km out — unseeable — and reads as "they don't try" (Chad F3).

FIX: split the dial.
- New [conquest] config pair in config/game.toml: `raid_pause_engage_m`
  (default 1500.0) and `raid_pause_disengage_m` (default 2500.0), loaded in
  config/load_game.cpp into the conquest params struct alongside its
  existing fields; loader check: disengage > engage > 0 (the house
  hysteresis rule; see load_scenario.cpp:450 style).
- instructor_tick.h raid_range_ok latch reads THESE (via cq->params, which
  is already in scope in that block), not dparams. Comment updates: the
  pause exists so the player defends by showing up — 1.5 km is inside
  visual range of the pump fight.
- FLAG IN THE COMMIT MESSAGE + fly card: 1500/2500 are Fable defaults
  pending Chad's stick ruling.

TESTS (same TU or test_conquest.cpp-adjacent, whichever fits the fixture):
6. "raid pause latch reads the conquest dial not the combat ranges": drive
   the latch open-loop across the new thresholds (the S6 dwell-with-ripple
   discipline — assert 1 switch hysteretic, and assert a player at 5 km
   with engage_range=6000 no longer suspends raid duty).
7. Loader: reject disengage <= engage; reject <= 0. (load_game test file
   precedent.)

## FIX-F1 — THE ENERGY BAIL KILLS EVERY FIGHT AGAINST THE PLAYER
(Opus probe attribution, D:\seads_sandboxes\ai-probes\f1\ATTRIBUTION.md)

TAPE+PROBE FACTS: wants_fire never opened because bfm.h's Offensive->Extend
specific-energy bail (e_delta < -bfm_extend_energy_m, 600 m) trips on 100% of
foe==player ticks — Chad flies ~259 m/s vs the bandit's ~110, deficit ~2800 m
forever — so every bandit leaves the ONLY guns-hot mode exactly min_dwell
(2.0 s) after entering it, and Extend's recovery (e_delta > -200) is
unreachable so it runs the full 16 s. Guns-hot duty 2 s/18 s, spent OUTSIDE
the 600 m fire band (Offensive arms at 1200 m). AI-vs-AI fights are
energy-matched and exit via frustration_s = 8.0 instead — the healthy arm.
Second-order (probe-measured): Intercept flies dl.speed +
intercept_speed_bump (~110 m/s total) while only Offensive/Yoyo get the
pursue bump — a stern chase never closes (0 Offensive entries from 2 km even
vs a 120 m/s player); the 2026-07-26 closing-speed fix never reached the
mode that does the closing.

THE FIX (Opus's minimal set, exactly):
1. config/scenario.toml [combat]: `bfm_extend_energy_m` 600.0 -> 3000.0
   (comment: scaled so a player-speed mismatch is not an auto-loss; the
   frustration_s=8.0 timer is Offensive's governing exit, matching the
   healthy AI-vs-AI behavior). Dial — flagged for Chad's stick.
2. config/scenario.toml [combat]: `bfm_attack_range_m` 1200.0 -> 750.0
   (guns-hot dwell lands inside the fire band fire_range_max=600; release
   stays *attack_release_frac). Dial — flagged for Chad's stick.
3. drone/bfm.h Intercept command (~line 525): speed_target becomes
   `dl.speed + std::max(bp.intercept_speed_bump, dl.pursue_speed_bump)` —
   Intercept inherits the pursuit closing bump. NOTE dl.pursue_speed_bump at
   the app call site is ALREADY the align^2-faded bump_eff (the corner-speed
   law), so max() keeps the off-nose fade intact; nose-on stern chases get
   the full bump. One line + comment.

TESTS:
8. New bfm leg (test_bfm.cpp is off-limits — new TU test_bfm_fastplayer.cpp
   or fold into test_conquest_respawn.cpp's TU only if the fixture fits; a
   new TU is cleaner): "offensive persists against a much faster player" —
   open-loop bfm_step with geometry pinned at range 500 m, e_delta ~ -2800
   (build states with the speed gap), assert mode stays Offensive past
   3x min_dwell and exits only via the frustration clock; mutation lever =
   extend_energy_m at the old 600 (leg must FAIL there). Also assert
   guns_hot true during the spell.
9. "intercept inherits the closing bump": bfm_step in Intercept with
   dl.pursue_speed_bump > intercept_speed_bump => speed_target uses the
   larger; with pursue bump 0 (patrol-grade) => old value bit-identical.
10. HONESTY RULE: if ANY existing test (test_bfm etc.) fails under the new
   dials, do NOT touch it — record the failure verbatim in
   BUILD_NOTES_FIXES.md and stop that item for Fable's review (config-
   relative legs should survive; a welded-constant failure is a finding).

## BUILD/VERIFY
Worktree D:\seads_sandboxes\ai-fix-forge (branch sandbox/ai-fixes). If the
build dir is unconfigured, configure with the dependency redirects:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src
  -DFETCHCONTENT_SOURCE_DIR_GLM=D:/flight_sim2/seads-recon/build/_deps/glm-src
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=D:/flight_sim2/seads-recon/build/_deps/catch2-src
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=D:/flight_sim2/seads-recon/build/_deps/tomlplusplus-src
Then cmake --build build; ctest --output-on-failure (full suite once at the
end; report the count). BUILD_NOTES_FIXES.md at the worktree root.

## OUT OF SCOPE (other agents own these)
F1 fire-chain fix (awaiting the Opus probe attribution), F4 tunnel-run fix
(awaiting the Opus real-dials probe), tools/ai_tape.py, any dc/tape format
change.
