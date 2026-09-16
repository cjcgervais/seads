# SESSION HANDOFF 2026-09-07 — game-loop lane, rung L10: THE ENGINE FIX

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260907_game_loop_engine_fix.md; do §1."
Worktree `D:\seads_sandboxes\game-loop`, branch `sandbox/game-loop`.
Never work in `D:\flight_sim2\seads-recon` (Chad's fly tree, branch `sandbox/r4a-phase0`) except to
merge `origin/main` into it and rebuild `build-play`.

## §0 The ask, verbatim

> "Hi, I want to allow the sudburian to fix the airplane engine when it says engine out, the same
> way the sudburian can fix the pump. Please complete this task now"

"The same way" was taken as the spec and it drove every decision below.

## §1 What to do first

1. **Chad flies it.** Nothing lands until he does — the lane's standing law. The checklist is §6.
2. **The gate is already GREEN on this tip** (`99786fae5`, 2026-09-08), quoted by name from
   `python tools/gate/gate_baseline.py check build/.gate_ctest.log`:

       gate: 6 failed of 1993
       baseline: 6 known reds
       OK -- the red set is EXACTLY the baseline, member for member.

   The 17 new engine-repair legs run as tests #777 onward and pass. Re-gate only if the tip moves
   (a merge of main counts). Detached, via `gate_run.sh` through `Start-Process bash.exe` — a killed
   ctest leaves no summary line and `gate_baseline.py check` refuses it, which is exactly what
   happened to this rung's first run and why it was discarded rather than reported.
3. Then land per SOP, sync `seads-recon`, rebuild `build-play`, tell him the exe time.
4. Three readings in §4 are REPORTED, not ruled. If he rules differently, each is one dial or one
   predicate — none of them is a rebuild.

## §2 What he can do now that he could not

Land or crash-land with a dead engine. Get off the aeroplane. Walk up to it. Press **U**. The
Sudburian turns the same wrench he turns at a pump, in the same work pose, with the same progress
bar, and the engine comes back. Walk away mid-job and it pauses exactly where he left it; come back
and it resumes from there. The engine crosses zero on the first turn, so the dead stick is a live
one again before the job is finished — that is the damage model's own law (`T_max` scales with
engine health), not a new one.

Before this rung a dead stick on the ground was permanent for the rest of that life. The only path
back was `combat::reset_damage`, which runs on respawn — so the only way to fix your aeroplane was
to die in it. That is not a repair loop.

The prop strike is very probably the engine he is looking at: `app/instructor_tick.h` writes
`damage.engine = 0.0` flat on a nose-down scrape, which is the "hard brake breaks the prop BY
DESIGN" behaviour from R4-FLY-7.

## §3 How it is built — everything reused, nothing re-decided

| Law | Pump | Engine |
|---|---|---|
| key | U | U (the same key, the same event) |
| who | off the machine, on his feet | identical — `ModeContext::man_upright`, the walker answers by enumerator name |
| where | reach measured ALONG THE GROUND | identical — the law moved into `combat/repair_reach.h` and both CALL it |
| when | on the SIM TICK, `full_s` from nothing to whole | identical shape, its own dial |
| out | same key, or walk away; a finish is a different sentence | identical |
| pose | the G2j work pose | identical — `work_now` is `mode == Repairing`, target-blind |

**New files (this lane owns them):**

- `combat/repair_reach.h` — the ground-distance reach law, ONE copy. Extracted from
  `pump_in_repair_reach`, which now calls it; that function keeps its name, signature and behaviour
  (it is the pump's policy seam and two callers hold it). The only behaviour delta is a
  `reach_m > 0` guard the old body lacked, and config forbids a non-positive reach anyway.
- `combat/engine_repair.h` — the second writer of `DamageState::engine`. **Fixes the engine and
  nothing else**: wings, pilot and structure are untouched, so `combat::is_dead` (a separated wing,
  a dead pilot, a broken structure — a KILL) is never undone by a man with a wrench.
- `test/unit/test_engine_repair.cpp` — 17 legs, no window, no world.

**Changed:**

- `app/player_mode.h` — `RepairTarget{None,Pump,Engine}` on `PlayerModeState`, plus
  `ModeContext::engine_in_reach`. **Deliberately NOT a sixth `PlayerMode`**: the man is doing the
  identical thing in both jobs, so the MODE is the posture and the TARGET is the noun. A
  `FixingEngine` mode would have duplicated every arm of the table that mentions repair.
  `player_mode_update` now ends each job by ITS OWN site leaving reach, and `player_mode_force`
  clears the target with the rest of the hook.
- `app/interact.h` — `SiteKind::EngineRepair`, `InteractDials::engine_reach_m`, the prompt, and the
  `Repairing`-legality of both repair kinds.
- `app/main.cpp` — the site (registered FIRST, see §4c), the dials, the tick, the sentences, the
  caption, and the walk-in probe (§5).
- `config/game.toml` + `config/load_game.{h,cpp}` — `[repair] engine_reach_m` / `engine_full_s`
  and two validations.
- `render/draw.{h,cpp}` — one defaulted `FrameInfo::repair_what`; the progress caption names its
  job instead of hard-coding "FIXING PUMP". Null or unset draws exactly what it drew before.
- `CMakeLists.txt`, `LANES.toml`, `generated/graph` (regenerated in the same change).

⚠ **ANNOUNCE (SOP 5), outside this lane's `owns`:** `app/main.cpp`, `config/game.toml`,
`config/load_game.{h,cpp}`, `render/draw.h` (+1 defaulted field), `render/draw.cpp` (the caption),
`CMakeLists.txt` (+1 test), `combat/pump_repair.h` (body of one function now calls the shared law).

## §4 THE THREE READINGS — reported to Chad, not decided here

**(a) The site is live whenever `engine < 1.0`, not only at the plate's `<= 0`.** He said "when it
says engine out", and the ENGINE OUT plate lights at zero. But the pump he compared it to is
repairable while dead **or merely damaged**, and "the same way" is the ask. The narrower rule would
mean a half-shot engine can only be finished off or landed with. If he wants the plate to be the
gate, it is one predicate: `engine_needs_repair`.

**(b) `engine_full_s = 45 s` is a DIAL, not a ruling.** His "about a minute" was a ruling about a
PUMP, and it is a balance number: a raid needs 13–18 minutes to take a pump, so a minute-long fix
is a real race. A parked aeroplane has no such clock, so a minute of standing still is a minute of
nothing happening. One line in `config/game.toml`.

**(c) With both in reach, the PUMP wins.** Land a dead stick at your own damaged pump and he is
inside both reaches. The pump is the one with a match clock running against it and the aeroplane is
not going anywhere. Named in the transition table and mirrored in the out-of-reach hint, so the
hint never advertises a job the U key would not have taken. Both KEYS stay armed regardless — J
still boards — because `mode_context_from` resolves per kind.

## §5 The instrument — run it before believing any "I can't reach it" report

    SEADS_ENGINEWALK_SMOKE="<start_m>[,bearing_deg]" build/seads.exe --smoke <ticks>

The pump rung's own law (2026-09-06), applied to the new site before anyone can report a floor that
is not there. At tick 40 it breaks the engine the way a prop strike does, parks the aeroplane, seeds
the machine `start_m` out, mounts him, steps him off on his feet, walks him straight in, presses U
through the REAL transition when the site lights, and logs every 30 ticks: ground distance, engine
health, walker mode, walker speed, player mode, repair target, site kind, progress and the live
prompt. Smoke-only by construction — nothing outside `--smoke` can set its flag.

**The run that certifies this rung** (`SEADS_ENGINEWALK_SMOKE="8" build/seads.exe --smoke 2600`):

| tick | ground dist | prompt | mode | target | engine |
|---|---|---|---|---|---|
| 60 | 5.97 m | `J  GET ON` | Afoot | none | 0.000 |
| 180 | 5.38 m | `WALK TO THE PLANE TO FIX THE ENGINE  5 m` | Afoot | none | 0.000 |
| 240 | 5.01 m | same, still counting | Afoot | none | 0.000 |
| — | 5.00 m | **U pressed through the real transition, action = BeginRepair** | | | |
| 270 | 4.87 m | `U  FIX ENGINE  5 m` | Repairing | Engine | 0.005 |
| 2580 | 4.74 m | `U  FIX ENGINE  5 m` | Repairing | Engine | 0.433 |

The site lights at exactly the 5 m dial. The bar tracks the engine's own health with no second
counter. 0.433 of the engine in 2330 ticks is 19.4 s of sim time, which is 0.431 of a 45 s job —
the duration is the dial's, to three figures.

**Two facts it MEASURED that no one should have to rediscover:**

1. **The whole sled + walker + gait tick is gated on `player.off_aircraft() && player.sled_seeded`.**
   A man with no seeded machine cannot take a step. That is CORRECT for the shipped game — `Afoot`
   is only reachable by stepping off a snowmachine — but it silently stalls any rig that forgets to
   seed one. The first build of this probe stood him in the snow for thousands of ticks.
2. **A `--smoke` run keeps flying the aeroplane.** Parking it once was not enough; it rolled away
   from the walking man at roughly 11 m/s while he closed at 0.73. The probe now holds the fixture
   parked every frame. A bench rig may clamp its fixture; it must never clamp the thing under test,
   and the things under test here are the reach law, the prompt, the key and the wrench.

## §6 The fly checklist (Chad)

Exe: `D:\flight_sim2\seads-recon\build-play\seads.exe` — **only after main is merged and rebuilt**;
until then fly this lane's own `D:\seads_sandboxes\game-loop\build-play\seads.exe`.

1. Take off, then break the engine the easy way: land and brake hard, or scrape the nose. The
   **ENGINE OUT** plate lights right of the hidden gauge and the ENG bar reads zero.
2. Stop the aeroplane on the ground. Press **J** to get off it.
3. Walk toward the aeroplane. Out past the reach the HUD says
   `WALK TO THE PLANE TO FIX THE ENGINE  <n> m` and counts down.
4. Inside 5 m the line becomes `U  FIX ENGINE  <n> m` and keeps counting, so it reads as arriving.
5. Press **U**. Note reads FIXING ENGINE, the work pose starts, the bar reads `FIXING ENGINE  n%`.
6. The engine crosses zero on the first turn — note reads ENGINE TURNING OVER, and the **ENGINE
   OUT plate goes out where you are standing** (L10b, below). It sits in the same place on foot as
   it does in the cockpit: bottom centre, just right of where the flaps gauge lives.
7. At full, note reads ENGINE REBUILT and the offer disappears.
8. Walk away mid-fix: FIX ABANDONED. Walk back and press U: it resumes from where it stopped, not
   from zero.
9. Press **J** to board. The ENG bar reads full and the ENGINE OUT plate is gone. Fly it away.

**What to tell us:** whether 45 s feels right, whether 5 m is where you expect the prompt to light,
and whether you want the offer at all for a merely damaged engine or only at ENGINE OUT (§4a).

## §6b L10b — THE PLATE IS VISIBLE AFOOT (Chad 2026-09-08: "yes make the plate visible afoot too")

Reported in the first pass, ruled the same night, shipped here. The plate was gated on
`!info.sled_active` under R1c, the mode-leak rule that hides AIRCRAFT READOUTS in drive mode ("a
snowmachine has no flaps"). ENGINE OUT is not that kind of thing: it is a standing FACT about the
aeroplane parked over there, and since L10 it is a fact you act on precisely when you are not in it.

- The gate moved OUT of `render/` and into the app, which owns the player mode:
  `FrameInfo::show_engine_out`, defaulted true, set by `app/main.cpp`.
- Visible in the cockpit, on the machine, on his feet, at the gun and mid-wrench.
- **Not** in the sting drone. That is not fussiness: a drone flight fills the screen with another
  aircraft's camera, and a plate reading ENGINE OUT over it would be read as that aircraft's engine.
  It is the one mode where the sentence would be false.
- The BRAKE tag beside it stays aircraft-only. It is a wheel readout anchored to a hidden gauge and
  it means nothing off the aeroplane — the two were gated together and should never have been.
- The anchor is unchanged in every mode. `x/y/w/h` are pure functions of screen size, computed
  whether or not the gauge is drawn, so there is one place to learn. The slot is otherwise empty off
  the aeroplane and the sled dash is far left at x = 24.

## §7 Traps recorded this session

- The Bash tool's heredoc mangles backslashes: `"...\\n"` in a Python heredoc reaches the file as
  `\n` and a patch silently fails to match. Write the script to the scratchpad, run the file.
  (Already in memory as `bash-heredoc-backslash-trap`; it bit again here.)
- `app/main.cpp` contains a literal NUL byte inside a character literal near the interact hint,
  which is why `grep` calls it a binary file. Use `grep -a`, and match on `'\x00'` when patching
  that line.
- Never name a local `near` or `far` in a test — ancient Windows macros.
- A summed-`dt` stopwatch in a test drifts a few ulps off the exact multiple; compare with an
  epsilon or the arithmetic that is behaving correctly fails the leg.
- A background `seads_tests.exe` holds the binary open and the next link fails with
  "cannot open output file: Permission denied". Stop your own task first — and scope it to your own
  task, never a machine-wide kill by image name.
