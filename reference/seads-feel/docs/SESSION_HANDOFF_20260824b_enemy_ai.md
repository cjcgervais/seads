# ENEMY-AI — RUNG E16. HANDOFF 2026-08-24 (evening)

LAUNCH LINE: **"Read docs/SESSION_HANDOFF_20260824b_enemy_ai.md; take the next
enemy-AI rung."** Binding ledger: `docs/ENEMY_AI_E1_E2_SPEC.md` §E16 (the last
section). Supersedes `docs/SESSION_HANDOFF_20260824_enemy_ai.md`, whose OPEN
items 1 (THE VACUUM CRASH DEFECT) and 2 (E15, WAITING ON IT) are both **PAID**.

---

## ★★★ WHAT HAPPENED: THE AI WAS NOT FLYING BADLY. THERE WAS NOWHERE TO FLY.

The previous handoff had the mechanism right — outside the domes lift lapses
and is not floored the way torque is — and the fix one layer lower than anyone
had looked.

**`[atmosphere] deck_agl_m` says "full air below this height ABOVE THE SURFACE,
EVERYWHERE (the go-anywhere floor: you can still land outside a bubble)". The
code measures it from the SPHERE.** On the bare planet R6 shipped on, those are
the same sentence. Then the real DEM landed underneath and nothing went red.
Measured on the shipped `assets/sudbury_dem.png` (relief_scale 350 m):

    median terrain elevation                165 m
    surface above the full-air deck (120 m)  69%
    surface above its fade         (320 m)   26%

**Over most of the planet the go-anywhere breathable floor is underground** —
so a sortie crossing between the domes had no lane to fly, and the air-seek
dive (`docs/ai_vacuum_strand.md` rungs 1/1b) has been diving the fleet at a
deck that is not there. Your own ruling — the whole surface stays traversable
and landable — has been quietly false since the DEM bake.

## THE FIX, AND WHAT IT MEASURED

`[atmosphere] deck_terrain_relative = true` (walk-back: `false`, one word).
The deck is measured from `env->ground->radius_at` — the same elevation query
the crash surface uses, never a second source. Probe P-H, a whole 22-minute
match, **this dial the only thing moving and the AI not touched at all**:

    metric                      PRE-E16        terrain deck
    enemy crashes                 37             18       (1.65 -> 0.80/min)
    ...in air below 0.25          34              0
    ...on a raid sortie           19              2
    mean air where it died      0.11           1.00
    enemy on-station             42.0 s         46.2 s
    your pumps          surface dead, deep 18%  BOTH DEAD (first at 6.8 min)

★ You signed **0.94 crashes/min** on tape 7. The pre-E16 table was flying 1.65
in the fixture; it is now 0.80 — **0.89 with E15's tunnel rope also on**, which
is what actually ships (see below). ★ And the enemy got BETTER and SAFER at the
same time, which is what a removed defect looks like — a dialled balance trades.

## AND E15 IS UNBLOCKED — `transit_reach_s_per_km` 0.0 -> **11.0**

The tunnel rung the last session built, measured, and shipped OFF because it
cost 103 crashes. Same replay, same dial, on the fixed deck:

    old sphere deck    103 crashes   4.60/min
    terrain deck        20 crashes   0.89/min   <- still under your 0.94

That was the "one edit" the last handoff left ready. It is made. Walk-back: 0.0.

## HIS RULING THIS SESSION, VERBATIM (it is why both dials ship ON)

> "yes go for the effective fixes so that we can move on from here"

★ THAT IS WHY THE DECK SHIPS **ON** RATHER THAN BUILT-AND-OFF. The E15 precedent
from the day before was to build, measure and ship OFF when a rung could not be
paid for; this one CAN be paid for, it restores a contract the config already
claimed, and he asked for the effective fix rather than a menu. ⚠ But it is a
FLIGHT-MODEL CHANGE HE HAS NOT FLOWN — `sim/aero.h`, the one site — so the fly
checklist below leads with it, and the walk-back is one word. **If his stick
disagrees, that is the ruling and the AI numbers go back with it; do not argue
the measurement at him.**

---

## ★★★ YOUR FLY CHECKLIST (two dials shipped, both one-word walk-backs)

1. **THE ONE YOU WILL FEEL: low air outside the domes.** Fly out of your bubble
   and descend. Below ~120 m over the GROUND (not over the sphere) you should
   now find air anywhere on the planet — over ridges, over the barrens, over
   the far side — and be able to fly and land there. That is the go-anywhere
   floor working for the first time since the DEM bake. **If it reads as too
   generous — if leaving the dome no longer feels like leaving something —
   walk it back with `deck_terrain_relative = false` and say so; the AI fix
   lives or dies with your call on this.**
2. **Do they still crash into hills?** They should, sometimes — 18 wrecks
   remain and every one of them now dies in FULL air, i.e. ordinary flying. But
   the ones falling out of the sky between the bubbles should be gone.
3. **Can you still lose — MORE so?** ⚠ This is the one to watch after the air.
   In the fixture BOTH your pumps go down, the first at **6.0 min** with the
   tunnel rope on (6.8 without it) — against a shipped arm that used to leave
   your deep pump at 18%. Their raiders now survive the trip, so the offense
   you ruled "a good easy / medium baseline" is landing more of what it always
   intended. ★ P-H is ENEMY-FAVOURABLE and RELATIVE (open-loop replay, flat
   terrain, no ballistics) so do not read 6.0 min as a prediction — but do
   expect real pressure. If it has gone past "you can actually lose" into
   unfair, the walk-backs in order are `transit_reach_s_per_km` 11.0 -> 0.0,
   then `raid_dps_frac` 0.65 -> 0.5, then `raid_backfill` false. **Do not walk
   back the deck to fix difficulty** — that dial is the world, not the balance.
4. **Does the tunnel offensive ARRIVE now?** That is E15's whole content: a
   striker ordered past ~15 km used to be dead on launch. Watch for underground
   contact and enemies coming up out of the mouths.
5. **E14 is still unflown** (the defender slot — attack their surface pump and
   see whether more than one shows up).

---

## STATE

- Worktree `D:\seads_sandboxes\enemy-ai`, branch `sandbox/enemy-ai`, on top of
  the five E12-E15 commits. **NOT PUSHED** (main still waits on your word).
- Gate: see the commit message for the exact count; the only reds are the
  pre-existing five (4 GI4 sled debts + `probe P-F` clause (2), red on purpose
  by your ruling — **do not bend it a third time**).
- Zero `assets/` files touched.
- FLY BUILD relinked: `cmake --build build-play --target seads`.
- Graph regenerated in-tree, `graph_query.py check` OK.

## WHAT SHIPPED

    deck_terrain_relative      true    E16   game.toml      [was: absent/false]
    transit_reach_s_per_km     11.0    E15   scenario.toml  [was 0.0 = OFF]

Both walk-backs are one word, and they are INDEPENDENT: the deck is the world,
the rope is the tunnel offensive's budget. `deck_terrain_relative = false`
restores the R6 bare-sphere deck bit-for-bit (the flag is read at ONE site,
`sim::atm_frac_at`, and gated on a live ground field).

## ★ THE INSTRUMENTS, AND HOW TO RE-RUN THEM

Everything above is reproducible in this tree. Both sweeps are HIDDEN (a `[.`
tag) because they fly a whole 22-minute match per arm — they are measuring
instruments the builder runs, never gate legs. A P-H arm is ~45-60 s; run them
in the background and **never relink while one is running** (a failed build
leaves the old binary and it WILL answer you).

    ./build/seads_tests.exe "[.e16air]" -s        # the air map. Flies nothing:
                                                  #   samples sim::atm_frac_at
                                                  #   along the raid corridor,
                                                  #   every 50 m, per altitude.
                                                  #   THE table that decided
                                                  #   this rung. ~1 s.
    ./build/seads_tests.exe "[.e16sweep]" -s      # the candidate arms: pre-E16
                                                  #   baseline, the three old
                                                  #   null fixes, E15's rope on
                                                  #   the old deck, and the
                                                  #   terrain deck with and
                                                  #   without the rope. ~7 min.
    ./build/seads_tests.exe "[.e15sweep]" -s      # E15's own trade curve.
    ./build/seads_tests.exe "[.e12sweep]" -s      # the difficulty dial curve.

Every P-H `report()` now prints the CRASH FORENSICS — count, rate against your
signed 0.94/min, how many died in air below 0.25, how many steep, how many
fast, the means, and the DISPOSITION each wreck was flying (defend / raid /
tunnel / fight / patrol, with duty-seconds beside each so it reads as a rate).
That disposition column is what named this rung's target and what proved the
fix; use it before theorising about any future crash report.

⚠ **AN ARM OVERRIDES CONFIG, IT NEVER DEFINES IT.** `ArmCfg`'s world/dial
fields are tri-state (`-1` = whatever ships). The first cut of the E16 arms
carried the deck's frame in `ArmCfg` alone and `build_world` never read the
shipped one, so the arm labelled SHIPPED flew a world the game does not ship.
See §E16.T — and keep the tape-7 arms pinning the WORLD as well as the dials,
because tape 7 was flown on the bare-sphere deck with the rope off.

## ★ WHERE THE WORK LIVES (this rung's whole footprint)

    sim/fields.h            the flag + its rationale (pure data)
    sim/aero.h              THE one site: atm_frac_at's deck term. `alt -
                            deck_ref` collapses to |pos| - radius_at(dir), so
                            it is exact AGL and p.R cancels -- a heightfield
                            with a different R cannot skew it.
    config/game.toml        [atmosphere] deck_terrain_relative = true
    config/load_game.{h,cpp}  the strict loader (require_bool, no default)
    app/main.cpp            one line copying it into the live field
    config/scenario.toml    E15's transit_reach_s_per_km = 11.0
    drone/drone.h           ★ COMMENTS ONLY. drone::tick is bit-identical to
                            the pre-E16 tick; the two mechanisms built this
                            rung were removed, and what remains are the notes
                            that stop the next session rebuilding them.
    test/unit/test_atmosphere.cpp   the two new gate legs (mutation-verified)
    test/unit/test_conquest_match.cpp  P-H's crash forensics, the air map, the
                            candidate sweep, the config-read fix and the two
                            re-derived rate clauses

## OPEN, RANKED

1. ★★ **THE TUNNEL-NET COLLISIONS.** 12 of the 18 remaining wrecks: near-level,
   in FULL air, underground. Now the largest single crash source by a mile, and
   a completely different defect from the one E16 fixed. ★ THE START IS ALREADY
   BUILT: P-H's disposition column reports `tunnel N/duty-seconds` per arm, and
   it stayed at 10-14 wrecks across every arm in the sweep — flat against the
   deck, the rope, the air-seek and the roster, which is exactly what a
   geometry defect looks like next to an air defect. Decode where in the net
   they hit (the `[.e16sweep]` arms already fly it) before touching a dial;
   E10/E11 named it first and it has now outlived four rungs.
2. ★★ **THE FIRE CHAIN — "not aggressive"** (carried forward unchanged): ~29 s
   of Offensive/Yoyo/Intercept inside the gun band produced zero fire intent in
   tape 9. E1/E7 territory, P-A is the fixture; E8.2's Intercept exclusion and
   the E7.2 scoped dive envelope are still open there.
3. ★ **THE RENDER MIRROR IS NOT TERRAIN-RELATIVE.** `render/air_field.h` + its
   GLSL transliteration keep the bare-sphere deck shell, so the VISIBLE deck
   haze and the FLOWN deck now disagree by up to the local relief. The visible
   atmosphere is bubble-only by design and the deck is invisible, so nothing on
   screen forks — but it is a real divergence and it is written down here
   rather than discovered later. `test_air_field.cpp`'s cross-check builds its
   own field (flag false) and stays exact.
4. ★ **THE HUNT/RAID CONTENTION** (carried forward): a ROSTER question for you,
   not a dial, and not a regression to chase.
5. **THE TRANSIT STALL, second half**: E15 explains the timeouts, not drone 4
   covering 1.8 km in 150 s. Negative closure / FIX overshoot unexamined.
6. **A NEW LOSS CONDITION YOU HAVE NOT SEEN**: 3 allies vs 7 with one pool of 4
   makes victory-by-wipe against you materially reachable — and E16 just made
   their raiders survive the trip.
7. `probe P-F` clause (2) — your ruling to re-make. Do not bend it.

---

## ★★★ PROCESS PAID FOR THIS SESSION

* ★★★ **A PLANT LAW IS ONLY AS GOOD AS THE WORLD CONTRACT IT WAS WRITTEN
  AGAINST, AND A BAKE CAN REPEAL A CONTRACT WITH NOTHING GOING RED.** Five
  successive AI fixes across two ladders were sound, green, and aimed at a
  fleet obeying orders correctly into a world that had stopped honouring its
  own documented floor. When a behaviour resists sound fixes, stop fixing the
  behaviour and MEASURE THE ENVIRONMENT it is being asked to survive.
* ★★★ **CODE AND CONFIG DESCRIBING ONE DIAL IN TWO FRAMES IS A BUG WAITING FOR
  AN ASSET.** "Above the surface" vs `alt - deck_agl_m`. It even had a comment
  admitting it — "terrain-relative deck is a later refinement" — which is how a
  deferral becomes a defect: the world moved and the note did not.
* ★★★ **THE PLAUSIBLE FIX WAS BACKWARDS AND ONE TABLE KILLED IT.** The
  ballistic HOP (climb high, coast across the gap) is ballistically sound and
  measured WORSE, because a dome is widest at its base: the corridor's dead
  length runs 0.00 km at 200 m to 9.35 km at 4000 m. Ten minutes of sampling a
  field beat a day of building against an assumption.
* ★★ **BUILD THE FORENSICS INTO THE FIXTURE BEFORE BELIEVING THE TAPE.** P-H
  now records every wreck's air, gamma, speed and DISPOSITION. The disposition
  column is what pointed at the outbound sortie (raid 19 / tunnel 11 of 37) and
  what proved the fix (thin-air deaths 34 -> 0).
* ★★ **A FIXTURE'S TERRAIN IS A DIAL TOO, AND P-H'S WAS PINNED AT THE WORST
  VALUE**: `uniform_field(300)` sits exactly at the old deck's death altitude
  (fade ends at 320 m), so the probe had no lane anywhere — harsher than the
  real DEM. Named in §E16.N rather than worked around.
* ★★★ **THE RUNG'S OWN FIXTURE COMMITTED THE DEFECT THE RUNG IS ABOUT.** P-H
  carried the deck's frame in its arm struct and never read the shipped one, so
  the arm labelled SHIPPED flew a world the game does not ship. It went red
  honestly and that red is what caught it. An arm now OVERRIDES config, never
  DEFINES it — and the tape-7 arms pin the WORLD as well as the dials.
* ★★★ **TWO GATE CLAUSES INVERTED WHEN THE OFFENSE STARTED WORKING.** Raid duty
  and near-seconds only accrue while the target pump is ALIVE, so the arm that
  takes both pumps at minute six banks less of both than the arm that never
  arrives (2358 s vs 2028 s while being strictly worse). E12 had already
  written that warning for on-station seconds and the same trap was sitting in
  two other counters under different names. Re-derived as rates per pump-alive
  second — what they always meant — never bent to green.
* ★ **TWO MECHANISMS WERE REMOVED, NOT SHIPPED OFF.** Both measured harmful; a
  dial that is only ever wrong is a trap for the next session. Their arms and
  numbers are in §E16.X so nobody rebuilds them.
