# PACKET — to the Sting deploy/launch ANIMATION lane, from the game-loop lane (2026-09-04)

Chad's word, same evening: "I told another agent who will build the sting deployment
animation ... that I will get you to make it so that I can press P on the snowmachine
as it will be based on deployment from the seat to the Sudburian's hands to take aim
for launch directionally toward enemy AI planes." That switch is BUILT, FLOWN by Chad
("all is well"), and is being landed on `origin/main` tonight (merge of
`sandbox/game-loop` @ `59c660470`; the merge SHA is in `LANES.toml [lanes.loop]`).
Merge `origin/main` before you cut geometry against the P key.

## 1. What changed in the loop (read the tree, not this list)

| Where | What |
|---|---|
| `app/player_mode.h` | `DroneKey` now deploys from **Afoot (upright)** OR **Sled (`ctx.sled_upright`)**. `PlayerModeState::drone_home` records which; `EndDrone` returns him THERE (seat launch -> back on the bars, foot launch -> back on his feet). `ModeContext::sled_upright` = `!sled.rolled`, filled by main.cpp. |
| `app/main.cpp` (P block) | `sting_stance_ok()` = (Afoot && man_upright) \|\| (Sled && !sled.rolled). `sting_stance(pos, heading, head_h)`: on the machine (or flying a sting launched from it) the SEAT is the stance: `pos = sled.position`, `heading = sled forward (orientation * (0,0,-1))`, `head_h = 1.35 m`; else the walker, `head_h = 1.7 m`. The over-the-shoulder aim camera and the LAUNCH ORIGIN (`pos + local_up * head_h`) both read it. Rolled machine -> "RIGHT THE MACHINE FIRST" (the old "GET OFF THE MACHINE FIRST" is gone). |
| driving while shouldered | Mouse goes to `sting_aim_az/el` (as on foot); A/D/W still reach the machine. While the sting FLIES, `bars = driving() && Riding` is false, so the thumb releases and the machine coasts to a stop under him. |
| `app/main.cpp` (X hold) | NEW give-up key: X held 1.5 s in any mode but Pilot/OnGun replays the aircraft crash path (books `DeathCause::kGiveUp`, spends a life, reborn in the aircraft, same spawn menu). A flying sting dies silently with the mode change (the existing silent-kill). Nothing for you to draw; you just need to know the mode can be FORCED to Pilot under a shouldered or flying sting. |
| `render/draw.h` + `render/map_screen.cpp` | `map_sting_glyph/pos/fwd`: the M map draws the flying sting as a filled ally diamond + heading tick, tag STING. Not your surface; announced because it is in `FrameInfo` next to the fields you will read. |
| `combat/kill.h`, `app/conquest_tape.h`, `docs/conquest_tape_spec.md`, `tools/ai_tape.py` | `DeathCause::kGiveUp = 5`, wire tag `giveup`. |

## 2. What the animation can read (all already in `render::FrameInfo`, nothing new added for you)

- `info.sting_shouldered` — the launcher is in his hands, aim view live. TRUE on foot AND on the seat.
- `info.sting_flying`, `info.sting_draw` (a `sim::SimState`), `info.sting_aim_dir`, `info.sting_left`.
- Seated vs afoot: render must not know `PlayerMode` (draw.h banner). The sled rider draw already knows whether a man is on the machine (`render/sled_model.*`, the rider that the flak lane hides with `sled_rider_hide_set`). Seated shoulder = `sting_shouldered && rider on the machine`; afoot shoulder = `sting_shouldered && the walker is drawn`. If you need a single bool for "shouldered from the seat", ASK before adding it: the loop lane owns `FrameInfo`'s mode-derived fields and will add `sting_seated` in one line if you want it named.
- The launch origin the sim uses is `sled.position + up * 1.35` (seated) / `walker.pos + up * 1.7` (afoot). If your deploy animation ends the launcher somewhere else, say so: the two must agree or the missile is born out of his hands. The 1.35 m is a placeholder eye/launcher height over the machine's frame origin, NOT a measured seat height — measure yours on the posed rider and tell us the number; we will move the dial.

## 3. Sequence the animation has to match (as flown)

1. Seated, machine on its skis, stings left: **P** -> "LEAD THE TARGET -- CLICK TO LAUNCH, P TO STOW", aim view from the seat (`sting_shouldered = true`). Deploy animation: seat -> hands.
2. Mouse aims, **click** -> "STING AWAY", mode Sled -> Drone, chase cam. Launch animation: off the shoulder along `sting_aim_dir`.
3. Flight ends (any of Target / SelfDestruct / Battery / Ground / Manual) -> mode Drone -> Sled, he is on the bars again, thumb live. Stow/return pose.
4. **P** while shouldered = "LAUNCHER STOWED" without a launch (reverse of 1).
5. Falls, mounts, respawn, X give-up: `sting_shouldered` drops the frame the stance is lost. Your pose must be able to cut, not only blend.

## 4. Rules we both live under

- SOP §5: announce every file you touch outside your own `owns` in `LANES.toml`. `app/main.cpp`'s P block, `app/player_mode.h`, `render/draw.h` FrameInfo fields are the loop lane's; message us before editing them (session `flight-sim2-21`, or a PACKET back in `docs/`).
- Commit only your own changes. Chad's word tonight: no agent commits work that is not its own. If a merge of main pulls in someone else's edit alongside yours, that is a merge commit, not your commit; if `git status` shows a file you did not touch, STOP and ask.
- Never work in `D:\flight_sim2\seads-recon` (Chad's fly tree). Lanes live in `D:\seads_sandboxes\<lane>`.
- The full gate: detached wrapper `.sh` via `Start-Process bash.exe`, verdict from `tools/gate/gate_baseline.py check` (red set == `generated/gate/known_reds.txt` BY NAME). Never kill ctest by process name.

## 5. Where to launch from

`docs/SESSION_HANDOFF_20260903_game_loop_NEXT.md` §2 (keys, incl. the X hold and P on the
machine) and `docs/SESSION_HANDOFF_20260903_sting_rpas.md` (the sting's own lane; ST-5
GLB/pose/audio was the deferred item this animation work picks up).
