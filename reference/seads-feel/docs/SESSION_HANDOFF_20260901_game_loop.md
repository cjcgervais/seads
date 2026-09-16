# SESSION HANDOFF 2026-09-01 — the millwright loop, BUILT, awaiting Chad's drive

Lane `loop`, worktree `D:\seads_sandboxes\game-loop`, branch `sandbox/game-loop`.
Plan: `docs/PLAN_20260901_game_loop_millwright.md` (rulings R1–R6, architecture, rungs).
Exe for Chad: **`D:\seads_sandboxes\game-loop\build-play\seads.exe`** (RelWithDebInfo; the only
binary to judge feel on). Rebuild it with `cmake --build build-play --target seads` from the
worktree, never the plain `build`.

## 1. What is built (one commit per rung, all mutation-verified, subsets green)

| Rung | Commit | Delivers |
|---|---|---|
| L1 | 169430ec6 | `app/player_mode.h` outer machine {Pilot, Sled, Afoot, Repairing, OnGun}; the sled persists once seeded; **J** = context mount / dismount / board; `app/interact.h` sites (SledMount, PumpRepair, FlakGun, **AircraftBoard** — a 4th kind, added so you can get back into the cockpit); `app/player_mount.h` the ONE seam (grip + man together, KEY_R through it); `app/walker_place.h` placement stub (0.9 m off the left board). |
| L2 | 0e969cefe | `combat/pump_repair.h`: **U** near your dead/damaged SURFACE pump, 60 s to full; the dome is BANKED and the collapse is a MASK with both guards; loss records `shrink_taken`, a revive pays exactly that back (R6); the sudden-death clock is **HELD** while a repaired pump stands and resumes from the seconds left; `pr` event in the conquest tape. |
| L3 | fc6057433 | `app/spawn_menu.h` + `app/spawn_policy.h`: first spawn = **Aircraft / Snowmachine** (keys 1/2 or click); after a death the menu appears only while your surface pump is dead or damaged; Snowmachine = born SEATED on the sled `[spawn] sled_dist_m = 300` from the pump on your side, aircraft parked beside it. |
| L4 | 490fe1152, c8827bb5d | `render/map_screen.*` (the M map moved out of draw.cpp verbatim, then reworked): **mouse wheel zooms** ×1.25/notch up to 64× fit, follows you above 4×, trails + roads on at 4×, place labels at 8×; **FIX** / **ATTACK** labels with distance + bearing; sled and parked aircraft glyphs; constant-size pointer with the active body's heading. |
| L6 | 0bf26760c | **THE RESPAWN LOCK + THE CLOCK THAT RE-POINTS** (Chad's R9 as revised 2026-09-03). One match-wide latch `ConquestState::respawn_locked`, set the instant the match clock ARMS and never cleared: no player respawn (both death sites; the match resolves via the existing `planes_left <= 0` rule, deathmatch tie-break included), no AI crash respawn (an `inert` wreck + `on_ai_kill`, no score), no reinforcement wave — **both sides**. And the clock is now ONE POOL that RE-POINTS: the target is derived every sim tick from who is pumpless (`sync_countdown_target`), HELD with no target when nobody or everybody is, so "rebuild your own pump then kill their last one" turns the remaining seconds against them. `rl` tape row, "RESPAWNS LOCKED" HUD line. |
| L7 | 6be561822 | **TRAMPLED SNOW AT THE FLAK GUN** (Chad 2026-09-03: "a gun should not be placed on top of snow, just trample the snow down around the gun", "it should be trampled a little bigger than the gun pad itself; it's fun to drive right up to the flak gun in deep snow, I don't want a big 60 m cleared section"). The gun does NOT move — `fg.pos` is still bare terrain. `world::SnowpackField::tramples` is a new list of `TrampleDisk{dir, radius_m, feather_m, keep_m}` applied at the END of `ambient_depth_at` (the root term every channel derives from, so driven surface / drawn patch / sinkage / dash cannot fork); it only ever SUBTRACTS (`min(depth, keep_m)`), and an empty list is bit-identical. `app/main.cpp` registers one disk per gun beside the 60 m tree pad: **radius 3.5 m** (pad 1.524 + `st_approach` 1.70 + margin), **feather 1.5 m**, **keep 0.05 m** (packed, not bare). `render/snow_patch.h` `fold_lat_n` **9 → 33** (a 2.484 m lattice) so the 10 m hollow survives into the drawn rider patch instead of being bilinearly averaged away — measured cost **+0.32 ms/frame** (900-frame `--smoke`: 3.17 → 3.50 ms avg; 41 nodes was +0.83 ms and was rejected). Test `test/unit/test_gun_trample.cpp`. |
| fix | b4d241a6f | red-team blockers/majors: a J dismount ENDS the sled tape episode (grip is not in the pin roster); the fix will not run while he is face-down (walker.mode == Afoot by name); KEY_R narrowed to "on the machine, or Afoot and the machine is ROLLED"; "PUMP AT FULL" no longer overwritten by "FIX ABANDONED". Also fixed a pre-existing main defect: the J seed opened the sled tape twice (lost brace since e348ca053). |

Dials: `config/game.toml [repair] full_s=60 reach_m=6 repair_points=0`, `[spawn] sled_dist_m=300
aircraft_beside_m`, `[interact] sled_reach_m=2.5 aircraft_reach_m=3.0 gun_reach_m=3.0`;
`config/world.toml [map] zoom_step zoom_max follow_zoom trails_zoom minor_roads_zoom place_labels_zoom`.

## 2. DRIVE CHECKLIST (Chad) — open `D:\seads_sandboxes\game-loop\build-play\seads.exe`

1. **Spawn menu** appears at start: pick **2 = Snowmachine**. You should be SEATED on the sled
   ~300 m from your own surface pump, facing it, the aeroplane parked beside you.
   (Nothing is damaged yet at tick zero, so the machine is placed off your surface pump anyway.)
2. **Ride to the pump** (W throttle, A/D steer). Press **M**, roll the **mouse wheel**: the map
   zooms toward you, trails appear at 4×, your pointer keeps its size and shows your heading.
3. **Stop beside the pump** (|v| < 1 m/s), press **J**: you dismount and stand 0.9 m off the left
   board. WASD now walks him. Walk within 6 m of the pump.
4. To test the repair you need a dead pump: let the enemy raid kill it, or fly Aircraft first and
   die so the respawn menu offers Snowmachine (it only appears while your pump is damaged).
   Near the dead pump press **U**: "FIXING PUMP NN %" bar, ~60 s. Walk away = pauses. When done:
   **"PUMP BACK ON LINE"**, the dome regrows by what that loss took, the loss clock reads **CLOCK HELD**.
5. Walk back to the sled (within 2.5 m), press **J**: mounted. Walk to the aeroplane (3 m), **J**:
   in the cockpit, fly away. Press J again from the cockpit later: it re-seeds the sled beside you
   (open question 5 below).
6. **The gun.** Walk to your faction's flak gun (80 m from your pump, the diagonal-barrel glyph on the
   map). Within 3 m of its approach mark, standing, press **O**: "O  MAN THE GUN". Mouse aims, LMB
   fires, SPACE free-look, O again = "O  LEAVE THE GUN" and he steps off onto the mark facing away.
   Refusals: from the sled seat, from the cockpit, or face-down in the snow.
7. **R** now only self-rights a machine you are on or one that is ROLLED next to you; thrown off
   with the machine upright 300 m away = walk back and J (the walk-back you ruled in).
8. **The trampled pad (L7).** Ride the machine **right up to the flak gun** — the deep snow should
   come all the way to the lip, no big cleared circle. Then stand at the gun, press **O**, and
   **elevate to full**: the pad is packed under you, deep snow at the lip, the sky is clear.
   (From OUTSIDE, at a distance, the gun still reads half-sunk — the 59 m planet mesh cannot be cut
   over a 7 m pad. That is the near-field resolution rung, not this one.)

Things to say out loud: is 300 m the right ride; is 60 s the right fix; does the map read at both
scales; does "PUMP AT FULL" / "PUMP BACK ON LINE" wording fit.

## 3. Rulings owed by Chad (defaults shipped)
1. ~~Clock on revive: pause + resume from remaining, or full disarm?~~ **ANSWERED (R7, 2026-09-03):
   "on the pause the clock yes, the clock is merely paused."** Shipped as built, and L6 took it
   further on his second ruling: the clock is one POOL of remaining time that RE-POINTS at whoever
   is pumpless, held (no target) when nobody or everybody is.
2. ~~**Deathmatch latch**: clear it on revive?~~ **ANSWERED (R8 + R9, 2026-09-03).** The latch
   STAYS (`cs.deathmatch` still terminal, still the points tie-break); a revive gives the bubble
   back at single-pump capacity; and the all-dead map now KEEPS its clock seconds so a rebuilt pump
   can turn them against the enemy. R9 on top: **no new respawns for either side once the clock has
   been initiated at all** — rung L6.
3. Stope (deep) pumps: repairable? (shipped NO — no walker ground down there.)
4. ~~F~~ RULED 2026-09-01: the fix key is **U** (F was the flap cycle in both input paths and did nothing on foot). the helmet-dent debug cycle that shared U was moved to Y (Chad: "move the cosmetics off the key")
   (cosmetic, their file — flagged to them, not edited).
5. **J from the cockpit re-seeds** the sled beside the plane even when one exists elsewhere.
   Keep (convenience) or force the walk-to?
6. A dead pump under repair is **invulnerable** for the 60 s (damage_pump refuses dead pumps); the
   only counter-play is killing the mechanic. Intended?
7. The wrench as a world object — next rung?

## 4. Open / blocked
- ~~**L5 flak walk-up**: blocked on the flak lane.~~ **BUILT 2026-09-03** (the flak arc landed on
  main and merged in at `098e81887`). Two commits: the map glyph main's inline block carried and
  the merge dropped, re-ported into `render/map_screen.cpp`; then the walk-up itself — the FlakGun
  interact site at the gun's baked `st_approach` mark, and O gated on Afoot + upright + 3 m. The
  old 30 m from-the-saddle rule is deleted. Drive it: park, **J** to get off, walk behind the gun
  until the prompt says **"O MAN THE GUN"**, press **O**; **O** again puts you back on the mark.
  Decision, not accident: the walk-up gate sits at the O KEYPRESS (player-mode table); `flak_man`
  itself is ungated so the flak lane's headless smoke rig (`SEADS_FLAK_MANNED`) still mans gun 0
  directly. `st_approach` is TRAIN-parented: transformed by traverse only, never the cradle path.
- The floored regrow is repair-ORDER dependent at tuned dials (not at shipped 0.5).
- `ai_tape.py` renders `pr` rows (minor pass, see git log after b4d241a6f).
- Gate: **GATED GREEN 2026-09-02 on 128783aa6 (main 96cd0bf19 merged): 1828/1834, red set == the six known reds BY NAME** (E12.1, probe P-F, four snow sled reds). Two earlier runs were killed by other lanes' machine-wide ctest kills (both lanes scoped them since). Was:
  `generated/gate/known_reds.txt` by NAME. Two ai-lane reds (E12.1, probe P-F) and four snow sled
  reds are the baseline.
- No DF1-* tests exist on this branch (they live on sandbox/enemy-ai); deck-scope neighbours green.

## 5. Lane law learned this session
- Three plan defects were caught by the ai lane BEFORE code because it read conquest.h before
  answering: the dome scale is a path-dependent, floored accumulator — never derive it, never
  invert it; bank it and mask the collapse with BOTH guards; record what a loss took and pay
  exactly that back.
- A "free key" from an audit must be checked against BOTH input paths.
- A persistent object needs a way back into everything it replaced (AircraftBoard).
