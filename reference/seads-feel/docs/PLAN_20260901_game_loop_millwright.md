# PLAN 2026-09-01 — THE MILLWRIGHT LOOP (lane `loop`, branch `sandbox/game-loop`)

*Written by the game-loop lane (session flight-sim2-21) from an Opus code audit of
`origin/main` a37b8246c, `sandbox/r4a-grip` adef92479, `sandbox/flak-gun` 4ab7ef566 and
`sandbox/enemy-ai`. Worktree `D:\seads_sandboxes\game-loop`. Base = origin/main + the r4a
walker merged at its FROZEN surface (adef92479, merge e5c2e9472).*

## 0. Chad's ask, verbatim intent (2026-09-01)

> fix the surface pumps to stop the game clock and revive a lost pump ... knowing from flying
> it's almost always death spawning in where we do, so the ability to fix a pump should be
> allowed through respawn straight into the snowmachine from a set distance away from the pump.
> So a menu for spawn of vehicle selection for the first time needs to be built. Sudburian
> stands and walks to the snowmachine, gets on with a button ... update the map to have less
> clutter and more accurate objectives, zoomable via mousewheel, better local scale ... pointer
> good at both large and small scales ... fix the pumps by driving up, getting off, then making
> the fix, the fix should take about a minute to full restore. And Sudburian can walk up to the
> flak gun and engage and operate it.

## 1. Rulings this ask makes (they supersede older canon where they conflict)

| # | Ruling (Chad, 2026-09-01) | What it supersedes | Built as |
|---|---|---|---|
| R1 | A **spawn-time vehicle menu** exists (first spawn: Aircraft / Snowmachine). | `WINTER_LAW.md` §1 "never a menu" and `app/main.cpp:1967-1972` "do not grow a menu on it". Reading: §1 forbids a menu for **in-world** mode switching. A spawn is not in the world — a respawn already is a non-diegetic event. Mount/dismount/repair stay diegetic (a key at a position). | `app/spawn_menu.h`, §4.3 |
| R2 | **Repair restores the pump fully in ~60 s.** | `docs/AUDIT_FINDINGS_20260825_synthesis.md:144-149` warns the AI raid takes 13–18 min per pump, so a 60 s fix wins by default **unless the mechanic is killed**. That is the loop's intended stake (millwright canon: strafing the repair run is the counter-play). Shipped as a dial. | `[repair] full_s = 60.0` |
| R3 | **A repaired pump stops the clock.** | `combat/conquest.h:429-432`, `config/game.toml:456-457`: countdown "armed once, never re-armed, no free extension". New law: the countdown **pauses** while the clocked faction has any pump alive again, and **resumes from the remaining time** if it is lost again. No free extension is preserved; re-arming is what repair *means*. ⚑ Chad may prefer full disarm/reset. | §4.2 |
| R4 | Respawn **on the snowmachine, seated, a set distance from the damaged pump**. | Millwright canon 2026-08-20 ("land near a snowmachine → run to it") and 2026-08-25 ("spawn a snowmachine on landing"). Now: spawn *on* it. | `[spawn] sled_dist_m = 300` |
| R5 | Sudburian **walks to the machine and mounts with a button**; **drives up, gets off, fixes**; **walks to the flak gun and operates it**. | Confirms `WINTER_LAW.md` §1 and ladder S8. | §4.1, §4.4 |

Not asked today and therefore NOT built: the wrench as a world object (§1 canon; queued as
the natural next rung), AI millwrights, the pilot land-at-marker respawn, the FPV drone.

## 2. What the audit found (facts the design stands on)

- **Pumps:** `combat/conquest.h:57-68` `Pump{pos,radius_m,hp,faction,surface,alive,max_hp}`;
  4 pumps, `[0]` Valley surface, `[1]` Sudbury surface, `[2]/[3]` deep; **index == faction for
  surface pumps** (relied on at `combat/raid.h:266`, `app/instructor_tick.h:2126`).
  `combat::damage_pump` (`:632-673`) is the **sole** writer of `hp` and `alive`. **No repair path
  exists anywhere.**
- **Clock:** no continuous drain. Loss = step: `score += 100`, dome `radius_scale` shrinks by
  `shrink_*_frac` 0.5 (accumulator, self-diagnosed as the disease at `:501-507`),
  `collapse_bubble_if_pumpless` zeroes it (`:511-520`), `arm_countdown_if_bubble_lost` arms the
  600 s sudden death once (`:526-540`), ticked at `app/instructor_tick.h:2473`.
  `app/conquest_tape.h:196,314-320,482` assumes `alive` is monotone true→false.
- **Defence latch is loss-only** (`app/instructor_tick.h:1478-1491`) so a repair tick will not
  scramble defenders onto their own mechanic. Already millwright-safe.
- **Respawn:** two hardcoded `spawn_state` calls (`app/instructor_tick.h:1348`, `:2363`),
  always airborne at R+2000 m, 140 m/s. **No player spawn policy hook.** The E5 seam
  (`combat/reinforce.h:130-154`, `app/conquest_world.h:101`) is AI waves only; its banner
  `:28-46` names the millwright seam and says `deploy` returning false = "no class chosen yet".
- **Sled:** does not exist until `KEY_J` constructs one 5 m right of the parked aircraft
  (`app/main.cpp:4001-4043`); mode = two loose bools `drive_mode` / `sled_seeded`. `KEY_R`
  autoright is labelled scaffolding to be cashed in by the walk-back.
- **Walker (r4a-grip only):** `sim/walker.{h,cpp}`, `WalkerMode` Riding→Falling→Buried→Down→
  CrawlProne→CrawlKnees→Afoot, player-driven W/S/A/D (same keys as the sled, disambiguated by
  `hands_on`). `walker_remount` = full state wipe (`sim/walker.cpp:192`), called only from J and
  R. **No proximity test, no placement entry, no "reached the machine".** Frozen public surface
  (their message, adef92479): `step_walker`, `walker_throw`, `walker_remount`, `WalkerInputs`,
  `WalkerState::pos/heading`, `mode` read-only (never pin its values).
  ⚠ The sled tape arms `snow_field.sample_tap` around the tick loop; **any new code that calls
  `sample_at` inside that window makes every tape unreplayable** (their finding).
  ⚠ The walker is deliberately not in the tape; the moment he can move the machine (repair,
  righting, gun) the tape must grow.
- **Flak gun (flak-gun branch only, 135 behind main, 15 files uncommitted):** `KEY_O` mount with a
  **30 m proximity gate** already (`app/main.cpp:3525`), `st_approach` baked and pinned but read by
  nothing, `app::kFlakAiOperate=false`. `app/main.cpp` +1163 there. **Nothing of it is on main.**
- **Map:** one 440-line inline block `render/draw.cpp:5531-5971`, one-shot ellipse fit for scale
  (`:5573-5586`), **no zoom, no mouse, no pan**; the player arrow (`:5878-5921`) is good;
  trails and place labels off by `render/map_style.h:52-61`.
- **Input:** no gameplay key table; 30 inline `IsKeyPressed` sites in `app/main.cpp`. Free keys:
  F, G is gear (both paths), **V reserved for Chad's cockpit view**, H semi-free in flight only.
- **Lanes:** `combat/*` is owned by `lanes.ai` (announce before editing); `app/main.cpp`,
  `render/draw.{h,cpp}`, `CMakeLists.txt`, `generated/graph/*` are where every lane collides.

## 3. Architecture — one outer mode machine, three seams, no second remount

```
app/player_mode.h        enum class PlayerMode { Pilot, Sled, Afoot, Repairing, OnGun };
                         struct PlayerModeState { PlayerMode mode; ... }
                         // OUTER machine. The walker's WalkerMode stays the SOLE authority for
                         // the on-foot sub-state (Falling/Buried/Down/Crawl*/Afoot). The outer
                         // machine never asks "is he upright"; it asks the walker.
app/interact.h           struct InteractSite { kind (SledMount|PumpRepair|FlakGun), pos_w,
                         reach_m, faction, index };  nearest_site(pos, mode) -> the prompt
app/player_mount.h       player_mount_request()   // THE seam. R4e (r4a lane) replaces its body.
                         // Lands TOGETHER with the KEY_R re-attach (grip + man in one place) —
                         // r4a's stranded-man bug: the two must never be re-attached separately.
app/walker_place.h       walker_place_afoot(WalkerState&, pos, heading) // STUB owned by this
                         // lane until R4e; sets pos/heading and asks for Afoot by enumerator
                         // name, never by value. He appears standing at the dismount point.
combat/pump_repair.h     PumpRepair state + repair_pump_tick(); the ONLY second writer of hp/alive
app/spawn_policy.h       PlayerSpawnChoice {Aircraft, Snowmachine}; choose → place
app/spawn_menu.h         the overlay (raylib, keys 1/2 + mouse), first spawn + damaged-pump respawn
render/map_screen.{h,cpp} the M-key map, extracted from draw.cpp, with MapView {zoom, centre}
```

Keys (all diegetic, all gated on position):
- `J` **context mount/dismount**: in a parked aircraft = seed+mount (unchanged); on the sled,
  stopped (|v| < 1 m/s) = **dismount** → Afoot beside the machine (the walker's `pos` placed
  0.9 m off the left running board, heading = sled heading); Afoot within `[interact]
  sled_reach_m = 2.5` of the sled = **mount** via `player_mount_request()`.
- `U` **interact** (Chad 2026-09-01; was F, which is the flap cycle): Afoot within `[repair] reach_m = 6.0` of an OWN dead-or-damaged surface pump
  → mode Repairing; progress runs while he stays in reach; leaving reach or `F` again pauses;
  full at `[repair] full_s = 60`. HUD: "FIXING PUMP  37 %" + a bar. Deep pumps: NOT repairable
  by the player in this rung (question for Chad; the stope has no walker ground).
- `O` **flak mount** (flak branch key): gate becomes "Afoot within `[interact] gun_reach_m = 3.0`
  of the gun's `st_approach`" once the flak lane lands; until then the site exists and the key
  says "FLAK GUN NOT ON THIS BRANCH".
- `M` map; **mouse wheel zooms** while the map is open (wheel is already dolly in freelook —
  the map consumes it first when open).

## 4. Rungs, in build order (sequential; they all touch `app/main.cpp` and share one build dir)

### 4.1 L1 — MODE + PERSISTENT SLED + MOUNT/DISMOUNT (`app/`)
- Introduce `PlayerMode`; `drive_mode`/`sled_seeded` become reads of it (minimal churn in
  the 7,000-line `main()`; no behaviour change while Pilot/Sled).
- The sled persists once seeded: it has a world position when nobody is on it (kernel steps it
  with zero inputs, `hands_on=false`, as it does today after a fall).
- `J` dismount/mount as §3. The walker is stepped exactly as R4c steps him (inside the sim tick,
  not per frame) and **outside the tape's tap window** (disarm across the step, as r4a did).
- WASD: Afoot → `WalkerInputs`; Sled → sled; never both (today's `hands_on` disambiguation kept).
- Camera: Afoot uses the walker chase (R4c already has one); Repairing = same.
- Tests (`test/unit/test_player_mode.cpp`): transition table; dismount places him beside the
  machine on the drive surface; mount refused beyond reach; mount at reach calls the seam
  exactly once; a mutation that skips the KEY_R co-attach turns a leg red.

### 4.2 L2 — REPAIR + THE CLOCK (`combat/pump_repair.h`, hooks in `conquest.h`, `instructor_tick.h`)
- `repair_pump_tick(ConquestState&, idx, dt)`: `hp += max_hp * dt / full_s`, clamp; at
  `hp >= max_hp` → `alive = true`, `hp = max_hp`, `score[faction] += repair_points` (dial, 0 ok).
- **The dome scale is NOT derived from the alive set** (corrected 2026-09-01 after the ai lane's
  review: `radius_scale` and `ceiling_scale` are path-dependent accumulators, the destroyer grows
  per kill at `conquest.h:712-719`; tape 15's 1.5× enemy dome is a Chad-reported defect that a
  derivation would silently delete, and deriving radius alone desyncs the ceiling pair). **And it is
  not inverted either** (second correction, same reviewer): the shrink is floored, so it is not
  invertible, and a faction can be both destroyer and victim, so a count-based restore drops the
  growth term (kill 1 → 1.15, lose 2 → 0.15, collapse → 0; truthful revive 0.65, count formula
  0.50). **Design: bank the value, mask the collapse.** `banked_radius_scale[2]` /
  `banked_ceiling_scale[2]` carry today's arithmetic unchanged; the live scales are
  `live = !faction_owns_pump_slot || faction_has_pump; scale = live ? banked : 0` — BOTH guards
  of `collapse_bubble_if_pumpless` (`:541-546`): a slotless faction (single-pump fixtures,
  `:507-512`) keeps its 1.0 dome from tick zero; only the second guard is about repair. (Third
  correction, same reviewer.) A revive
  lifts the mask; nothing is reconstructed. **R6 (Chad, 2026-09-01): "yes the dome grows back if
  made operational again."** Because the shrink is floored, the amount a loss took is RECORDED on
  the shrink lines (`shrink_taken_*[slot] = before − after`, arithmetic unchanged) and a completed
  repair adds exactly that back to the banked value, lumped at completion. Exact by construction:
  a revive can never exceed the pre-loss banked value; the destroyer's growth is untouched.
  **Invariant (test it every tick in the fixture):** `shrink_taken_*[slot]` is nonzero ONLY while
  that slot is dead-and-unpaid — set on the shrink (overwrite, never accumulate), zeroed the
  instant it is paid, asserted zero for every ALIVE slot. Arms: repair the same slot twice with
  no intervening loss → banked unchanged by the second; a damaged-but-alive pump completing a
  repair moves the dome by nothing (regrow is revival-gated, not repair-gated). Loss code is not edited, so loss-only
  sequences are bit-identical by construction; DF1-* / deck-fight tests must not move (the scale
  feeds `drone::deck_scope`). The `deathmatch` latch is left terminal — question for Chad.
- Countdown: `countdown_paused` when the clocked faction regains a live pump; resume from the
  remaining seconds on re-loss (R3). `conquest_countdown_tick` skips while paused. HUD shows
  "CLOCK HELD".
- `rebuild_conquest_bubbles` from the sim tick after a revive (AT-9: never a frame clock).
- `app/conquest_tape.h`: add a `PumpRepaired` event so `alive` false→true replays; old tapes
  decode unchanged.
- Tests (`test/unit/test_pump_repair.cpp`): 60 s to full at dt 1/120; partial then loss keeps
  the damage; revive restores the dome scale by the derived rule; countdown pauses/resumes with
  no free extension (mutation: resume-from-600 turns red); defence latch does not fire on a
  repair tick; existing conquest goldens unmoved.

### 4.3 L3 — SPAWN MENU + SPAWN ON THE MACHINE (`app/spawn_policy.h`, `app/spawn_menu.h`)
- First spawn: menu **Aircraft / Snowmachine**. Respawn after death: menu offered **only while
  an own surface pump is dead or damaged** (millwright canon); otherwise Aircraft, as today.
- Snowmachine spawn: sled placed `[spawn] sled_dist_m = 300` from the own damaged surface pump,
  on the bearing from that pump **toward the own bubble centre** (your side of it), grounded to
  `drive_radius_at`, heading toward the pump, rider **seated** (mode Sled). The aircraft is
  parked beside it (grounded, zero speed) so "walk to the plane and take off" remains possible.
- The two hardcoded `spawn_state` calls route through one `player_spawn(policy)` function.
- Menu is the ONLY non-diegetic screen; the game is paused under it.
- Tests: spawn distance/bearing within tolerance on the drive surface; menu gating on pump
  state; Aircraft path bit-identical to today's `spawn_state`.

### 4.4 L4 — THE MAP (`render/map_screen.{h,cpp}`)
- Extract the block from `render/draw.cpp:5531-5971` unchanged first (one commit, pixel-identical
  by construction), then:
- `MapView { double zoom; dvec2 centre_m; bool follow_player; }`; wheel = ×1.25 per notch,
  clamp [fit, 64×fit]; at zoom > 4×fit the view **follows the player**; inverse projection
  (screen→world) lives beside the forward one and is tested as a round trip.
- **Declutter by zoom**: far = ellipses, 4 pumps, tunnel, player, contacts; near (≥ 4×) = trails
  and roads on (`map_style` dials become per-zoom), place labels on at ≥ 8×.
- **Objectives**: the own damaged pump pulses with "FIX" + distance/bearing; enemy surface pump
  "ATTACK"; the sled and the parked aircraft get glyphs when the player is not in them.
- **Pointer**: the existing arrow, scaled in screen px (constant size at every zoom), heading
  from the active body (aircraft / sled / walker).
- Tests: projection round trip at three zooms; zoom clamp; follow threshold.

### 4.5 L5 — FLAK WALK-UP (blocked on the flak lane)
- Prerequisite, not ours: the flak lane commits its 15 uncommitted files and merges main
  (`app/main.cpp` +1163, `step_frame` signature). Then this lane adds the `FlakGun` interact
  site (`st_approach`, reach 3 m, Afoot only) and the O key gate reads it.
- Until then L1 registers the site kind and the prompt only.

### 4.6 Red-team, then gate
- A fresh-context adversarial review of the DECISIONS (right seam? right file? does the derived
  scale really reproduce the goldens? does anything call `sample_at` in the tap window?) before
  the gate, per the standing rule. Fix pass. Then the ~57 min gate **detached** on `build/`;
  red set must equal `generated/gate/known_reds.txt` by NAME. Then `build-play --target seads`.

**Dead vs damaged (asked by the ai lane):** Chad's words are "revive a **lost** pump", so a DEAD
surface pump is repairable, which makes `collapse_bubble_if_pumpless` and the armed countdown
reversible states. The `deathmatch` latch (`cs.deathmatch`, never cleared) is left terminal
until Chad rules on it. The 2026-08-30 "armed once, never re-armed" doctrine predates repair
existing; the pause is entered only BY a repair, so without one nothing changes and the
existing arm test (`test/unit/test_conquest.cpp` ~1062) stays green. **⚠ SUPERSEDED IN PART by R9
as revised (§5a): that test's "the all-dead map nulls the seconds" assertion contradicts the
re-pointing ruling and was changed in L6 to assert the seconds are KEPT and never extended. The
"arms once / no free extension" claim itself is intact and still tested.**

## 5a. ★ RULINGS 2026-09-03 (Chad, verbatim intent) — answers to Q1/Q2
> on the pause the clock yes, the clock is merely paused. On the deathmatch: once all pumps are
> destroyed, if they get fixed the clock pauses but NO NEW RESPAWNS occur for a team after two pumps
> have been destroyed. You can repair a pump to pause the match-end clock; it will allow the bubble
> back to the capacity of a single functioning pump, giving the team a better home base to defend
> their newly fixed pump from and also to stage an attack on the enemy AI.

- R7 **The clock is PAUSED, never disarmed, by a repair** (shipped as built).
- R8 **The deathmatch latch stays**; a repair after all pumps died gives the bubble back at
  single-pump capacity (shipped: mask lifts + the recorded shrink returns).
- R9 **REVISED by Chad, 2026-09-03 — the trigger is THE CLOCK, and the lock is MATCH-WIDE.** His
  words: *"no respawns once the clock has been activated. The clock can be paused and a bubble
  revived, but once the match clock is initiated once, even if paused, then there are no new
  respawns from either side, and win or lose is determined by elimination (team deathmatch, kill
  them all) or by destroying their remaining pumps and having one of yours repaired: it makes the
  clock be advantage for whoever has a remaining functional pump. So you could rebuild your own
  pump then destroy the remaining enemy pumps, which would turn the clock's remaining time against
  your enemy. Still no respawns."*

  Two mechanisms, built as rung **L6**:

  1. **THE RESPAWN LOCK.** ONE match-wide latch `ConquestState::respawn_locked`, set the first time
     `arm_countdown_if_bubble_lost` actually arms — not on a faction going pumpless, not per team.
     One-way: no repair, revive, hold or re-point clears it. From that instant nobody on either
     side gets a new aeroplane: the player's crash site and component-death site refuse (no menu,
     no `player_spawn`, and the match resolves through the EXISTING `planes_left <= 0` rule so the
     deathmatch points tie-break applies), the AI crash respawn leaves an `inert` wreck + a
     roster death (`on_ai_kill`, no score), and `reinforce_tick` refuses every wave for both
     factions. Read through `combat::respawns_locked(cs)`; visible as a `rl` tape row (once, on the
     latch tick) and a "RESPAWNS LOCKED" HUD line beside the pump count.

     **And the VICTORY-BY-WIPE VETO reads it.** `combat::faction_can_reinforce` — the one
     predicate shared by the wave machine and `ConquestState::reinforcements_live` — answers
     the lock FIRST, so a locked match stamps the veto false and "kill them all" resolves
     VICTORY on the tick the last enemy dies. Without that, an R8 repair puts the veto's two
     old terms (breathable enemy dome, pooled revives) back on their feet and holds a won
     match open on waves that can never come: the tape-3 stalemate, one rung later.
  2. **THE CLOCK IS ONE POOL THAT RE-POINTS.** `countdown_s` is written exactly twice in the
     codebase — once at the arm, once by the tick that spends it — and never again. The TARGET is
     DERIVED every sim tick from the fact (`sync_countdown_target`): exactly one slot-owning
     faction pumpless → the clock RUNS against it; none pumpless (a repair) or BOTH pumpless (every
     pump dead) → HELD, `countdown_faction = -1`, seconds kept. `countdown_active` now reads the
     POOL (`countdown_pool_live`), not the target, so a held clock is still drawn with its seconds.
     `null_countdown_if_all_pumps_dead` keeps `cs.deathmatch = true` and still nulls the TARGET
     (the half of the 2026-08-30 ruling that saved his match) but **no longer zeroes the seconds**
     — otherwise the rebuild-then-kill sequence he describes would have no time left to turn.
     Arming still happens ONCE; a re-point is a re-target of the same pool, never a re-arm.

## 5. Questions for Chad (defaults chosen so nothing blocks)

1. R3: pause-and-resume (built) or full disarm on revive?
2. Deep (stope) pumps: player-repairable at all? (default NO this rung)
3. Spawn distance 300 m and "your side of the pump" — right? (dial `[spawn] sled_dist_m`)
4. The wrench: next rung, or fold into this one (adds ~a day)?
5. On-foot sequencing: R4d/R4e (righting, real remount) are r4a's; this lane's placement stub
   stands in until he sequences R4e.

## 6. Lane hygiene
- `LANES.toml [lanes.loop]` registered. Announced edits to shared/other-lane files: `CMakeLists.txt`
  (new TUs + tests), `combat/conquest.h` + new `combat/pump_repair.h` (ai lane's path — additive,
  announced in the packet to the ai lane), `render/draw.{h,cpp}` (map extraction), `app/main.cpp`.
- Every session ends committed and pushed to `sandbox/game-loop`. Graph regenerated in the same
  commit as any structural change. The kernel (`sim/`, `control/`) is not touched.
