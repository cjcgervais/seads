# SESSION HANDOFF 2026-09-03 (evening) — the millwright loop is LANDED; STAND BY and WATCH THE MERGES

Lane `loop` (LANES.toml `[lanes.loop]`), worktree `D:\seads_sandboxes\game-loop`, branch
`sandbox/game-loop`, clean and pushed. Previous handoff (the drive checklist, rung table, rulings):
`docs/SESSION_HANDOFF_20260901_game_loop.md`. Plan: `docs/PLAN_20260901_game_loop_millwright.md`.

## 0. Chad's standing instruction (2026-09-03, verbatim intent)
> other agents are currently fixing for me: one working on the scarf, one working on the
> snowmachine and the Sudburian's legs, and another building a whole other mechanic (an RPAS
> interceptor operated by the Sudburian for when away from the flak gun). Please stand by ...
> make the handoff to the next instance and warn about other agents currently working. I would
> like them to watch for all of their work when it merges to make sure it goes well.

So: **this lane is in STAND-BY. Do not start a new rung unprompted.** The job until Chad says
otherwise is to watch the other lanes' landings on main for anything that breaks the loop, and to
be ready to merge main into this branch and re-gate when asked.

## 1. Where everything is

| What | Where |
|---|---|
| main | `7b0ecf82a` = the whole loop L1–L7 + the reach fix + the two 09-03 rulings. Every landing gated green by NAME (the baseline six: E12.1, probe P-F, four snow sled reds; the nightly's seventh, "ballistic truth harness", is green in lived-in trees and red only in a fresh checkout). |
| Chad's fly tree | `D:\flight_sim2\seads-recon` on `sandbox/r4a-phase0` @ `9b9aab1c0`, contains main 7b0ecf82a, exe `D:\flight_sim2\seads-recon\build-play\seads.exe` (19:58). **Never work in that tree** (Chad's rule); the enemy-ai lane has been merging main into it at request. |
| This lane's exe | `D:\seads_sandboxes\game-loop\build-play\seads.exe` (same code as main). |
| Nightly main gate | DOWN as of tonight — three runs killed in the BUILD phase (zero `error:` lines, no `ninja: build stopped`, `FAILED:` target lines = compiler processes terminated). Last trustworthy STATUS is for e3d7d0c27. Do not consume it until republished. |

## 2. The loop, as landed (keys)
Spawn menu: 1 Aircraft / 2 Snowmachine (Snowmachine dead until an own surface pump is dead or
damaged; spawn 1 km out, your side, facing the pump). **J** mount / dismount / board (landing +
J = your sled, any time). **U** fix (8 m along the ground to the pump's FOOT -- 2026-09-06, was 6; the pump HOUSE now
STANDS ON THE GROUND, 12 m wide, the logical point stays 10 m up its mast = `app::kSurfacePumpMastM`;
"WALK TO THE PUMP TO FIX NN m" within 120 m, and inside reach "U  FIX PUMP  N m" keeps counting;
60 s; dome regrows; CLOCK HELD). Pump-reach INSTRUMENT: `SEADS_PUMPWALK_SMOKE="<start_m>[,bearing]"
build/seads.exe --smoke 4000` walks him in at the REAL placement and logs the ground distance +
prompt every 30 ticks -- run it before believing any "can't reach the pump" report. **M** + wheel = the map. **O** man the gun (Afoot,
upright, 3 m of st_approach; packed 3.5 m pad in the snow field; gun on bare terrain). **R** self-
right only on the machine or beside a rolled one. **Y** = helmet-dent debug (moved off U).
**X held 1.5 s** = GIVE UP (2026-09-04, Chad: "a way I can respawn in a snowmachine / plane"):
legal on foot / on the machine / mid-drone / mid-repair, NOT in the cockpit (crash it) or on the gun;
books `giveup`, spends a life, reborn in the aircraft, same spawn menu (Snowmachine only while a pump
of yours is damaged); under the R9 lock it only says "NO RESPAWNS". The machine stays where it was.
Spawn menu's dead Snowmachine row now says HOW it comes alive (landing + J, or a respawn while a pump
is damaged). The M map draws the flying Sting as a filled ally diamond + heading tick, tag STING.
**P ON THE MACHINE** (2026-09-04, Chad, for the seat-deploy animation lane): P shoulders the launcher
from the SEAT (no J), click launches from the seat, the flight's end hands back the bars
(`PlayerModeState::drone_home`, `ModeContext::sled_upright` = !sled.rolled; rolled machine = "RIGHT THE
MACHINE FIRST"). One stance lambda in main.cpp (`sting_stance`) answers pos/heading/eye height for the
shoulder cam and the launch; the animation lane reads `info.sting_shouldered` + `player.mode == Sled`.
Match law: respawns lock for BOTH sides when the clock first arms; the clock is one pool that points
at whoever is pumpless, holds when both or neither have a pump, expires against its target; wipe
after the lock = VICTORY at once.

## 3. ⚠ CONCURRENT LANES (Chad's list) — WATCH THEIR MERGES
1. **Scarf** — render-side rider costume. Touches `render/sled_model.*`, `render/rider_*`, the GLB.
   Loop exposure: the walker/rider draw path (hide flags `sm.hide_rider || g_rider_hidden`), the
   F-pose at the gun.
2. **Snowmachine + the Sudburian's legs** — the r4a lane (`sandbox/r4a-grip` → landed 233185252;
   next work unknown). Touches `sim/walker.*`, `sim/rider_grip.*`, `sim/sled.*`, `render/sled_model.*`.
   Loop exposure: **the frozen walker surface this lane builds on** (`step_walker`, `walker_throw`,
   `walker_remount`, `WalkerInputs`, `WalkerState::pos/heading/mode` read by NAME) and the placement
   stub `app/walker_place.h` (theirs to replace at R4e). Any change to `WalkerMode` enumerators or
   to what `walker_remount` does hits L1/L5/L7 directly.
3. **RPAS interceptor operated by the Sudburian when away from the flak gun** — a NEW mechanic, new
   lane. Loop exposure: it will want a `PlayerMode` (OnDrone?) and an InteractSite kind, the same
   seams as the gun (`app/player_mode.h`, `app/interact.h`, `player_mode_transition`), the walker
   hide flag, the sled tape (a man who can launch a drone is a man who moves things), and probably
   `combat/`. If they add a mode by raw assignment instead of the transition table they will strand
   `repairing/repair_pump` (the L6-fix-pass class); if they gate on `drive_mode` it no longer exists.
4. Also live: **enemy-ai** (`sandbox/enemy-ai`, combat/*, raid.h — the pump-attack rung is next and
   inherits the "+10 m mast" placement property) and **flak-gun** (st_grip_l/r unheld, suffix-paired
   boots still open).

**How to watch:** after each landing on main, in THIS worktree: `git fetch; git merge origin/main`
(expect only `generated/graph/*` to conflict → `python tools/graph/graphify.py`, never hand-merge),
`cmake --build build`, then the loop's subsets:
`ctest --test-dir build -C Debug -R "player mode|mount seam|dismount|interact|pump repair|spawn|map|flak|gun trample|respawn|walker|conquest"`.
Anything red that is not one of the six baseline names is a finding: report it as an OBSERVATION
(test name, commit, repro command) to the owning lane per `docs/CONTRIBUTION_SOP.md` §4 — never a
diagnosis of their code. Then the full gate detached (§5) before this branch lands anything again.

## 4. Rulings still owed by Chad (defaults shipped)
Stope (deep) pumps repairable? (no). J from the cockpit re-seeds the sled beside the plane vs
walking to it (re-seeds). A dead pump under repair is unshootable for the 60 s (yes, by construction).
Refusal wordings at the gun ("GET OFF THE MACHINE FIRST" etc., mine). 3.5 m pad / 5 cm floor / brass
on the pad (his eye). Slotless-faction fixtures re-point the clock where the old code nulled it
(fixture-only). A locked match keeps ticking with the player as a wreck (no match-end freeze exists
anywhere). The wrench as a world object = the natural next rung. The near-field snow resolution rung
(planet mesh is 59 m cells, single-sided; a pad-sized hollow is visible only from under the sheet;
the cut needs gun sites known before `load_planet` and finer cut leaves) = the rung Chad asked about.

## 5. Lane law learned (do not relearn the expensive way)
- **Gate detached** via a wrapper `.sh` through `Start-Process bash.exe` (`gate_run.sh` in the
  worktree); `setsid nohup` from the tool shell does not survive. Tripwires in order: non-ASCII
  TEST_CASE, `graphify --stale`, **CRLF** (`git ls-files --eol | grep w/crlf` must be 0; fix from a
  committed tree: `git rm --cached -r . -q && git reset --hard`), then build + ctest + baseline by NAME.
- **Never kill ctest by process name.** Two lanes did tonight and killed each other's gates
  (no summary line → gate_baseline refuses). Filter `Win32_Process.CommandLine` on YOUR worktree path.
- A rejected tool call can still have run: **check `git status` after any interruption**.
- **A test that cannot fail for the real reason**: the pump reach passed 1900 tests with man and
  pump at synthetic points while the shipped pump sits 10 m up its mast. Fixtures near placed objects
  come from the REAL placement.
- **Read the other lane's code before answering it** (the ai lane caught three plan defects that
  way before a line was cut: the dome scale is a path-dependent, floored accumulator — bank it,
  mask the collapse with BOTH guards, record what a loss took and pay exactly that back).
- Chad's rulings can arrive mid-build: message the running agent, do not restart it.
- Cross-session messages: repo work only; Chad knows and allows it (asked 2026-09-03).
