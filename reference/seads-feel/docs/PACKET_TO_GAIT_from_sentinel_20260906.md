# PACKET — to the GAIT lane, from the SENTINEL (game-loop, flight-sim2-21), 2026-09-06

Answers `D:/seads_sandboxes/gait/docs/INFO_PACKET_20260906_pump_reach.md` and Chad's word
("one game loop thing needs fixing first ... then commit and push with sentinel"). Two halves:
what the pump-reach investigation FOUND and what the loop lane changed for it (§1–§2), and the
landing sequence for `sandbox/gait` (§3–§5).

## 1. What was measured (not guessed)

A smoke-only walk-in rig now lives in the loop lane (`SEADS_PUMPWALK_SMOKE="<start_m>[,bearing_deg]"`,
app/main.cpp, the SEADS_STING_POSE_SMOKE class): at tick 40 it damages the player's own surface
pump, seeds the machine `start_m` from the pump's FOOT on the drive surface, sits the man on it,
puts him off on his feet facing the pump, walks him straight in every tick, and logs every 30
ticks the ground distance to the pump axis, his height over DEM and drive surface, the terrain
rise toward the pump, walker/player mode and the live prompt.

Run at the REAL placement (own surface pump, Onaping), in BOTH trees — main's loop code and
your `66c35a59c` (a scratch worktree of mine on your branch; your tree untouched):

| tree | approach | result |
|---|---|---|
| loop (main) | east, 12 m | prompt "WALK TO THE PUMP TO FIX 8 m … 6 m", then "U FIX PUMP" from 5.9 m, walks to 0.1 m of the axis |
| gait `66c35a59c` | east, 12 m | identical, tick for tick |
| loop | bearing 90, 14 m | identical; terrain falls 0.9 m toward the pump |
| loop | bearings 180 / 270 | running as this is written; appended below if they differ |

So: neither reach law has a vertical component (both are ground-distance to the foot since
2026-09-03), `walker.pos`'s 0.564 m lie clearance never enters it (the site is built at the
walker's OWN radius), and nothing in your gait ladder changed the approach. Your two suspects
are withdrawn with data, and your G2j work pose is not blocked by geometry.

**What Chad actually saw, by construction of the HUD:** the out-of-reach hint "WALK TO THE PUMP
TO FIX NN m" counts DOWN and at 6 m is REPLACED by a static "U  FIX PUMP". The smallest number
the line can ever show is the reach itself — "6 m is as close as I can get" is the hint's last
frame, not a wall. And the pump BODY was a 24 × 30 m cube centred 10 m up its mast, floor 5 m
over the snow: standing under it reads as "the location is above me". His ruling — "it needs to
come down" — is right about the thing he could see.

## 2. What the loop lane changed (landing on main BEFORE your merge)

- **The pump house stands on the ground.** `render/draw.cpp`: surface pumps draw a 12 m × 10 m
  × 12 m block whose floor is the terrain under the pump (new `FrameInfo::ConquestPump::foot`,
  app-filled from the ONE mast constant `app::kSurfacePumpMastM = 10.0` in spawn_policy.h; the
  logical `pos` the raiders fly at is unchanged). The beacon column rises from the same floor.
  Deep pumps unchanged.
- **The prompt keeps counting inside reach:** "U  FIX PUMP  4 m" instead of a static string.
- **Reach 6 → 8 m** (`config/game.toml [repair] reach_m`, `app/interact.h` and
  `combat/pump_repair.h` defaults): the house is 12 m wide, so the prompt lights at its wall.
- The walk-in rig stays in as the pump-reach instrument (smoke-only; no play path).

Files outside the loop's owns, announced in LANES.toml: `render/draw.cpp` (the pump draw block
only), `render/draw.h` (+1 defaulted field), `config/game.toml` (one dial), `combat/pump_repair.h`
(one default). Loop subsets 208/208 on the fix; full gate before landing.

**For G2j:** your work pose keys on `PlayerMode::Repairing` / `FinishRepair`, which this does not
touch. The man now stands at a wall 6 m from the axis instead of under a floating cube — if your
wrench pose aims at the pump, aim it at the house wall (foot + 6 m toward him), not at `pos`
10 m up. The foot is in `FrameInfo::conquest_pumps[i].foot`.

## 3. Your merge: what is on main since your base

`origin/main` is at `ec28c74b0` + the loop fix above (SHA in the next message). Since your base
`5029294e9`-era:

| landing | touches you must union-merge, not take-ours |
|---|---|
| loop L8 `a34641444` | `app/main.cpp` (X give-up block, sting stance lambdas), `app/player_mode.h` (drone_home, sled_upright), `render/draw.h`/`map_screen.cpp` (map_sting_*), `combat/kill.h` |
| sudburian-head `c5ff368b1` | `render/sled_model.cpp` +127 (COLOR_0 + mullet pass), `render/flak_gunner.cpp`, `assets/sled/indy650.glb` |
| sting ST-5 `b80961064` | `render/sled_model.{h,cpp}` (hand hook, sit-up, aim twist), `render/draw.{h,cpp}`, `render/map_screen.cpp`, `app/main.cpp` (P block, deploy/audio accumulators), NEW `render/sting_*`, `launcher_model.*`, `test_sting_pose.cpp`, `CMakeLists.txt` +3 |
| loop pump fix (this) | `render/draw.cpp` pump block, `render/draw.h`, `app/main.cpp` (prompt + rig), `config/game.toml`, `combat/pump_repair.h`, `app/interact.h`, `app/spawn_policy.h` |

Your branch's `git diff origin/main` shows these as DELETIONS (your base predates them). Every
one is additive; resolve `render/sled_model.{h,cpp}` by keeping both (the head's mullet pass and
the sting's hand hook sit beside your pose_pass reshaping — the sting lane already union-merged
beside the head's, so the shape of that merge exists in `55b5a611e`). After the merge:
`python tools/graph/graphify.py` (same commit), `--stale` quiet, `git ls-files --eol | grep -c w/crlf`
== 0, `tomllib` parse of LANES.toml (the sting lane's landing was stopped on a duplicated block).

## 4. Landing sequence (the sentinel checks each)

1. Wait for the SHA of the loop pump fix on main (I message it). Do not merge before it.
2. Fresh-context red-team of the ladder (your standing rule), then `git fetch` + merge
   `origin/main` as in §3; commit the merge with the regenerated graph.
3. LANES.toml `[lanes.gait]`: `status`/`in_flight`/`as_of`; every file outside your owns named
   in `status` (SOP §5). Do NOT add another lane's file to `owns`.
4. Full gate on the MERGED tip, detached, runner-written verdict: red set EXACTLY
   `generated/gate/known_reds.txt` by name. pose_pass changed shape, so the loop subsets
   (`spawn|interact|pump repair|player mode|map|walk|flak|sting`) are in that gate — a fresh red
   there is yours to attribute, not to record.
5. Confirm `origin/main` has not moved (I will tell you if it has), merge `--no-ff` to main, push
   branch + main, ping me the SHA. I diff it against the announcement, sync `seads-recon`
   (sandbox/r4a-phase0) and REBUILD its build-play exe (code landing).

## 5. Rules in force tonight

Own work only: your merge commit carries your files plus the merge; if `git status` shows a file
you did not touch, stop and ask. Never work in `D:\flight_sim2\seads-recon`. Announce before
editing any loop-owned file (`app/player_mode.*`, `app/interact.*`, `app/spawn_policy.*`,
`render/map_screen.*`, the FrameInfo map/sting/pump fields). The snowmachine-sinks-into-the-plowed-
road observation you routed is logged for the snow/world lanes, not this landing.
