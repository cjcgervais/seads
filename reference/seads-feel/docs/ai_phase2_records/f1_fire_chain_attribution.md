# F1 ATTRIBUTION — why no enemy round was ever fired at the player

Verifier: fresh-context Opus instrument-builder. **Attribution only — nothing in
`D:\flight_sim2\seads-recon` was edited.** Probe source + binaries + raw runs live
in `D:\seads_sandboxes\ai-probes\f1\`.

* `probe_f1.cpp` — headless reproduction of the real chain under the real loaders
* `run3.txt` — the run quoted throughout (all 10 cases, per-tick traces)
* `run1.txt` / `run2.txt` — earlier passes (kept for the record)

Build (do NOT use the repo build dir):

```
g++ -std=gnu++20 -O0 -g -I. -I build/_deps/glm-src -I build/_deps/tomlplusplus-src/include \
  -DSEADS_CONFIG_DIR='"D:/flight_sim2/seads-recon/config"' \
  -o D:/seads_sandboxes/ai-probes/f1/probe_f1.exe D:/seads_sandboxes/ai-probes/f1/probe_f1.cpp \
  sim/step.cpp world/heightfield.cpp world/buildings.cpp world/tunnel_net.cpp \
  control/controller.cpp weapon/ballistics.cpp \
  config/load_aircraft.cpp config/load_controller.cpp config/load_game.cpp \
  config/load_scenario.cpp config/load_world.cpp
```

(`-O2` segfaults inside toml++ under this mingw 14.2 — unrelated to the finding;
`-O0`/`-g` is correct and was used for every number below.)

---

## 0. Config sources — verified, no fork

Printed by the probe through the shipped loaders (`run3.txt` head):

| value | source | value |
|---|---|---|
| `DroneParams` (incl. `bfm`) | `config/scenario.toml` `[drone]` + `[combat]` via `cfg::load_scenario_toml` | `speed=85`, `bfm.enabled=1` |
| `all_vs_player` | `config/game.toml` `[conquest]` | **false** → `combat::assign_foes` team path |
| `difficulty` | `scenario.toml [combat]` | 4 → `max_engaged=3`, `rof=8 Hz` |
| fire discipline | `scenario.toml [combat]` | cone 14.00°, band `[60, 600]`, `fire_align_cos=0.820` (34.9°) |
| BFM handover | `scenario.toml [combat]` | `attack_range_m=1200`, `min_dwell_s=2.0` |
| speed bumps | `scenario.toml [combat]` | `pursue_speed_bump=90`, **`bfm_intercept_speed_bump=25`** |
| `sim_dt` | `aircraft.toml` | 0.008333 (120 Hz; the tape samples every 24 ticks = 5 Hz) |

`game.toml` **does not** override any `[combat]` / `DroneParams` value. The
suspected config-source fork does not exist at the loader level — but there *is*
a dial fork inside `bfm.h` (blocker C below).

---

## 1. THE COMPONENT THAT NEVER OPENS

**`drone/bfm.h:450-453` — the Offensive→Extend *specific-energy* bail.**

```cpp
} else if (may_exit &&
           (g.e_delta < -bp.extend_energy_m ||            // <-- THIS
            st.fight_ticks > dwell_ticks(bp.frustration_s, dt))) {
    enter(BfmState::Mode::Extend);
```

`e_delta` (`bfm.h:310-330`) is **specific energy as a height**, `h + V²/2g`,
*bandit minus player*. `bfm_extend_energy_m = 600 m`.

The bandit's own autothrottle caps it at `dp.speed + pursue_speed_bump = 85+90 =
175 m/s` and it only actually reaches ~110-118 m/s in a fight. Chad flew the
match at a **mean 259 m/s / peak 362 m/s**. Purely from the velocity term:

```
e_delta ≈ (110² − 259²) / (2·9.81) ≈ −2 800 m
```

which is **4.7× past the −600 m trip**. So the moment `may_exit` becomes true —
i.e. exactly `bfm_min_dwell_s = 2.0 s` after entering Offensive — the bandit
declares itself energy-beaten and leaves the only guns-hot mode.

It then cannot come back quickly either: Extend exits (`bfm.h:501-503`) on
`e_delta > −reenter_energy_m` (`−200 m`) or `extend_max_s = 16 s`. Against a
faster player the recovery branch is **unreachable by construction** (the bandit
cannot out-energy a plane that is permanently 140 m/s faster), so every Extend
runs the full **16 s** timeout.

**Net duty cycle against a fast player: 2 s guns-hot, then ≥16 s guns-cold,
forever.** `guns_hot` is set at `bfm.h:545` and *only* there — Offensive is the
sole mode allowed to shoot — and `drone/drone.h:1239` is
`d.wants_fire = bc.guns_hot && fire_pc.fire;`.

The second half of the mechanism is a **phase mismatch**: Offensive opens at
range < `attack_range_m = 1200 m`, but the shot needs range ≤ `fire_range_max =
600 m`. In a head-on merge at ~360 m/s closure, the 2 s Offensive window burns
1200 m → ~480 m; only the last ~0.3 s is inside the fire band, and by then the
merge has swung the target off the nose. **The 60-600 m fire band is flown almost
entirely in Extend, guns cold.**

### Probe evidence (`run3.txt`)

Two drones (`assign_foes`, `max_engaged=3` from difficulty 4), player scripted
straight-and-level, `sim_dt` ticks, `env=null` unless noted.

| case | Offensive duty | Offensive exits | mean `e_delta` | in-band ticks: Off / Ext | `pursue().fire` | **`wants_fire`** | rounds |
|---|---|---|---|---|---|---|---|
| head-on 3 km, **player 250 m/s (tape speed)** | 480/6681 = **7.2 %** | **2× →Extend(energy)** | **−2661 m** | **76 / 647** | **100** | **0** | **0** |
| head-on 3 km, player 85 m/s (drone-matched) | 1922/13352 = 14.4 % | 2× →Extend(frustration) | **+314 m** | 1064 / 184 | 147 | **147** | fires |
| stern chase 2 km, player 250 m/s | **0 %** | — | −2558 m | 0 / 0 | 0 | 0 | 0 |
| stern chase 2 km, player 120 m/s | **0 %** | — | −87 m | 0 / 0 | 0 | 0 | 0 |
| sustained tail 600 m, player 90 m/s | 30.0 % | 9× frustration | +260 m | 3287 / 3362 | 1414 | **1040** | fires |
| sustained tail 400 m, player 60 m/s | 13.3 % | 4× frustration | +506 m | 1528 / 1849 | 857 | **513** | fires |

The head-on-250 row is the tape reproduced exactly: **`pursue().fire` was true on
100 ticks and `wants_fire` on zero — every one of those 100 ticks was flown
GUNS COLD** (`pursue().fire while GUNS COLD = 100`). The fire discipline in
`pursue()` was *not* the blocker in that case; the BFM veto was.

Per-tick trace, head-on 250 m/s (columns: range, mode, mode_ticks, may_exit,
attack-range, guns_hot, cone°, bearing°, in-band, in-cone, coordinated,
pursue-fire, wants_fire, engaged, foe, atm, arm_ok, V):

```
   600  0  1242.8 Intercept   600  1    0    0     8.5    7.5   0    1    1     0    0  1   -2 1.00  1  102.2
   660  0  1066.7 Offensive    45  0    1    1    11.2    9.7   0    1    1     0    0  1   -2 1.00  1  103.8   <- guns hot + IN CONE, but 1067 m > 600 m band
   720  0   889.3 Offensive   105  0    1    1     8.2    3.4   0    1    1     0    0  1   -2 1.00  1  106.8   <- ditto, 889 m
   780  0   710.5 Offensive   165  0    1    1    24.8   26.1   0    0    1     0    0  1   -2 1.00  1  109.2
   840  0   531.3 Offensive   225  0    1    1    28.1   23.1   1    0    1     0    0  1   -2 1.00  1  110.3   <- finally IN BAND, cone now 28 deg
   900  0   352.2 Extend       45  0    1    0    16.3   19.6   1    0    1     0    0  1   -2 1.00  1  110.8   <- min_dwell expired -> ENERGY BAIL, guns cold
   960  0   172.2 Extend      105  0    1    0    39.8   34.8   1    0    1     0    0  1   -2 1.00  1  110.6
  1020  0    17.4 Extend      165  0    1    0   153.3  129.4   0    0    1     0    0  1   -2 1.00  1  112.1   <- 17 m merge, guns cold
```

Offensive ran **exactly 240 ticks = 2.000 s = `min_dwell_s`** on both entries;
Extend ran **1921 ticks = 16.0 s = `extend_max_s`** on both. Neither is a
coincidence — they are the floor and the ceiling, and nothing in between ever
happened.

### Atmosphere cross-check (requirement 4)

Ran the same two geometries with a real `sim::AtmosphereField` built from
`game.toml [atmosphere]` (deck 120/200, bubble r=6000 / ceiling 4000 / softs):

| case | `atm_frac` | `climb_arm_ok` | Offensive duty | `wants_fire` |
|---|---|---|---|---|
| head-on 250, INSIDE bubble | ~1.0 | true | 7.2 % (identical) | **0** |
| head-on 250, BARE DECK at 2 km | ~0 | false | 7.1 % | **0** |
| tail 600 / 90 m/s, INSIDE bubble | ~1.0 | true | 27.8 % | 1040 |
| tail 600 / 90 m/s, BARE DECK at 2 km | ~0 | false | 6.7 % | **0** |

The atmosphere is **not** a gate on the fire chain (no atm term appears in
`guns_hot`, `pursue().fire`, or `enemy_fire_tick`). `climb_arm_ok` only gates the
**Yoyo arm** (`bfm.h:440`), and Yoyo armed **0 times in every case**. Thin air is
a *second-order aggravator only*: outside a bubble the bandit cannot hold station
(bare-deck tail case never got inside 600 m at all) and its `e_delta` gets worse
(−1096 m vs +174 m). It never changes the attribution.

---

## 2. Cross-check against the tape's own rows

`build-play/conquest_tape_1.jsonl` (cqtape-v1, 50 506 lines; tag legend from
`tools/ai_tape.py` header: `bfm 0=Intercept 1=Offensive 2=Yoyo 3=Extend`,
`foe -1=none -2=player`).

| tape fact | value |
|---|---|
| `d` rows | 41 990 (5 Hz × 10 drones × ~14 min) |
| `wf` (wants_fire) true | **0 of 41 990** |
| rows with `foe == -2` (player) | 762 |
| bfm mix on `foe == -2` | Intercept 472, **Offensive 36**, Yoyo 34, **Extend 220** |
| player speed | **mean 259 m/s, max 362 m/s** |
| drone speed | mean 148 m/s |
| **`e_delta` on `foe == -2` rows** (recomputed `h + V²/2g`, g=9.81) | mean **−2565 m**, median −2355, range [−3985, −1306] |
| **fraction of those rows with `e_delta < −600` (the Extend trip)** | **100.0 %** |
| **fraction with `e_delta > −200` (the Extend *recovery* branch)** | **0.00 %** |

**The claim survives its own falsification test.** The mechanism predicts
`e_delta < −600` on essentially every player-facing tick and `e_delta > −200`
essentially never; the tape gives 100.00 % and 0.00 %. Offensive-vs-player is
4.7 % of player-facing rows — the probe predicts 7.2 %, same order, and the tape
number is lower because the real player also maneuvered and broke range.

Two secondary tape facts worth recording:

* AI-vs-AI Offensive runs have a **median of 7.4 s and max 8.0 s** = the
  `frustration_s = 8.0` timeout, not the energy bail. Drone-vs-drone `e_delta ≈
  0`, so the energy branch never fires there. **The energy bail is specific to
  the player**, which is exactly why the AI shoots at each other (4 `dk` + 1 `da`
  event in the tape) and never at Chad.
* AI-vs-AI never even reaches the fire band: of 9 875 drone-foe samples, **8**
  were inside 60-600 m, and exactly **1** was simultaneously in band and in cone.
  `bfm=3, foe=-1` rows (2 077) are all `inert` frozen wrecks — not an anomaly.

---

## 3. Ranked blockers — the whole chain, not one link

**P0 — Offensive→Extend energy bail vs a faster player** (`drone/bfm.h:450-453`,
`extend_energy_m = 600`; recovery `bfm.h:501-503`, `reenter_energy_m = 200`).
Caps guns-hot at 2 s per 18 s cycle and puts the guns-cold Extend exactly on top
of the 60-600 m fire band. **Tape: 100.0 % of player-facing ticks trip it, 0.00 %
can recover.** This is the component that never opens.

**P1 — Offensive window (≤1200 m) and fire band (≤600 m) are out of phase.**
Even with the bail fixed, the 2 s dwell floor plus a 1200 m arming range means a
head-on merge spends most of its guns-hot time *outside* `fire_range_max`. Probe:
`guns_hot AND in_band = 76` ticks (0.63 s) in a 90 s engagement, vs `in_band`
total 723. The bandit is best-pointed (cone 8-11°) at 900-1100 m — precisely
where the range gate forbids the shot.

**P2 — Intercept flies at the OLD speed bump; the 2026-07-26 fix never reached
it.** `bfm.h:525` uses `dl.speed + bp.intercept_speed_bump` = 85+25 = **110 m/s**,
while `bfm.h:544/566` (Offensive/Yoyo) use `dl.speed + dl.pursue_speed_bump` =
85+90 = **175 m/s**. Intercept is the mode that has to *close*, and it is the slow
one. Probe, stern chase 2 km: **Offensive entries = 0** and `min_range` never
drops below 1988 m in 90 s — even against a **120 m/s** player (mean `e_delta`
only −87 m, so energy is *not* the blocker here; raw speed is). This is the
chicken-and-egg that makes P0 unreachable-to-fix on its own: the bandit cannot
get to 1200 m to become Offensive in the first place from any tail geometry.
`scenario.toml`'s own comment for `pursue_speed_bump = 90` states the reason
("a pursuer can never close a tail chase at all if it is slower than the target")
— the dial exists and the BFM Intercept path silently ignores it.

**P3 — `fire_range_max = 600` vs `bandit_convergence = 250` / `attack_range_m =
1200`.** Cosmetic ranking only: 600 m is a defensible gun range, but it is half
the arming range, which is what creates P1.

**NOT blockers (verified healthy):**

* `combat::enemy_fire_tick` (`combat/kill.h:434-470`) has **no** gate beyond
  `!inert && engaged && wants_fire` plus a cooldown that *drains every tick and
  never resets*. In every probe case where `wants_fire` opened, rounds spawned
  (`ticks-with-live-round` 1476-4863). Difficulty 4 → `rof = 8 Hz` → 0.125 s
  period, far shorter than any realistic firing window. **The spawn side is fine.**
* `fire_range_min = 60` — eats only the last ~0.4 s of a merge, where the cone is
  already 150°+. Probe: it rejected essentially nothing.
* `fire_align_cos = 0.82` (34.9° coordinated gate) — rejected **12 of ~28 800**
  ticks in one case, 0 in all others.
* `combat::assign_foes` / `engaged` / `foe` — worked perfectly in probe and tape
  (`engaged` true on 100 % of probe ticks; the tape holds `foe==-2` for long
  spans). Not the problem.
* `atm_frac` / `climb_arm_ok` — see §1; not on the fire path.
* Config loading — no fork; `scenario.toml` is the single source of `DroneParams`.

---

## 4. Recommended MINIMAL fix (not implemented)

The chain needs all three of P0, P1, P2 to be healthy; **P0 alone is one line of
config.** In priority order:

1. **P0, config-only, zero code:** make the energy bail measure something a bandit
   can actually win. Either
   * raise `bfm_extend_energy_m` above the structural player-vs-bandit deficit
     (≥ 3000 m at today's speeds — effectively disarming the energy branch and
     leaving `frustration_s = 8.0` as the sole Offensive exit, which is what
     already governs the healthy AI-vs-AI fights), **or**
   * (better, one-line code) clamp the energy comparison to the *closure* fight
     rather than raw airspeed — e.g. compare `e_delta` only when
     `g.range > bp.attack_range_m` so a bandit that is already inside the merge
     never breaks off on an energy number it can never beat.

   Recommended smallest honest change: **`bfm_extend_energy_m` 600 → 3000** in
   `config/scenario.toml [combat]`, which is a pure dial and is exactly the
   quantity the tape shows to be mis-scaled (100 % trip rate).

2. **P2, one line:** `drone/bfm.h:525` — Intercept should use
   `dl.speed + std::max(bp.intercept_speed_bump, dl.pursue_speed_bump)` (or
   simply `dl.pursue_speed_bump`), so the closing mode inherits the 90 m/s bump
   the config already ships. Without this, a tail geometry never reaches
   Offensive at all (probe: 0 entries in 180 s).

3. **P1, one dial:** close the phase gap between `bfm_attack_range_m = 1200` and
   `fire_range_max = 600` — either drop `bfm_attack_range_m` to ~700-800 so the
   guns-hot dwell lands *inside* the fire band, or raise `fire_range_max` to
   ~900. The first is cheaper and does not change the ballistics story.

Expected result, from the probe's own healthy cases: with `e_delta` no longer
tripping, the head-on-250 case's 100 already-true `pursue().fire` ticks become
100 `wants_fire` ticks, and at `rof = 8 Hz` that is ~7 rounds per merge — pressure,
not an aimbot. Re-run `probe_f1.exe` after any change; the head-on-250 case is the
regression that matters (`wants_fire` must go 0 → non-zero).
