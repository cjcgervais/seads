# F4 — AI tunnel deaths under the REAL dials: attribution

**Status:** attribution only. Nothing in `D:\flight_sim2\seads-recon` was modified.
All probe code, binaries and logs live in `D:\seads_sandboxes\ai-probes\f4\`.

---

## 0. TL;DR

The tunnel dials are **not** the cause. Under the real `config/game.toml [tunnel]`
dials the maverick dies **at exactly the same rate, at the same ticks, at the same
coordinates** as under the certificate's `test_tp()` dials (25/36 in-net deaths in
both arms, byte-identical event lists).

The cause is a **lateral control-authority shortfall on the Errington leg**:

> The maverick crosses the Errington mouth at **173–183 m/s** — 35 % above
> `tunnel_speed_max = 135`, the speed the bore autopilot is tuned for — in a
> **saturated 55° dive** against a bore that descends only **24.7°**, and usually
> a few degrees of bank the *wrong* way. The first ~1.4 s inside the net go to the
> pitch channel pulling 30° of dive out; because that pull happens while the plane
> is still banked left, it rotates the **track** 3.7° → 10° left of the bore axis
> while the cross-track is still ~zero. The roll channel is the only thing that can
> fix an azimuth error, and it is a first-order bank hold (`bank_p = 0.5`) whose
> command **saturates at the 65° `bank_cap` and stays saturated for the whole
> remaining 2.5 s**. At 182 m/s a 65°-bank turn has a **1 574 m radius** (≈10 °/s),
> so the azimuth error keeps *growing* to 24° before it starts coming back. The
> cross-track runs 0 → **−110 m** at up to 55 m/s. 110 m **is** `tube_width_m`,
> the bore's horizontal semi-axis. The plane leaves the tube sideways
> **580–720 m past the mouth, 3.5–4.1 s after entry**; `sim::step`'s tunnel yield
> stops applying that tick, `ground_contact` sees a state 400 m under terrain and
> fires the `deep_penetration_m` crash. `drone::tick` respawns it in place.

It is **not a PIO**. The entire event is one monotone saturated ramp — no
oscillation anywhere in the trace. The pure-pursuit lookahead is simply far too
short for the speed: `lookahead_m = 220` × (182/120) = **334 m effective**, which
is **0.21 ×** the achievable turn radius.

**Minimal fix direction (config-only, no logic edit):** raise
`scenario.toml [maverick] lookahead_m` 220 → ~500 and lower `centerline_gain`
2.0 → ~1.0. Measured: fleet in-net deaths **69 % → 21 %** (lookahead alone) and
**0/12 over 30 min / 6/29 over 60 min** for the pair. See §6.

---

## 1. What actually kills a drone in the net

`drone::tick` (drone/drone.h:1481) ends with

```cpp
const bool ground_live = env != nullptr && env->ground != nullptr;
if (ground_live ? d.curr.crashed : sim::altitude(...) <= 0.0) { respawn_in_place(...); }
```

`d.curr.crashed` is set by `sim::step` (sim/step.cpp:185-217):

```cpp
const bool in_tunnel = env->tunnels != nullptr && env->tunnels->contains(next.position);
if (!in_tunnel) { ground_contact(...); obstacle_contact(...); }
else            { next.on_ground = false; next.crashed = false; ... }
```

So **there is no in-net death predicate at all.** A drone dies the instant its
integrated position leaves `TunnelNet::contains()` — i.e. crosses a wall — because
the very next `ground_contact` call sees a CG ~400–2000 m below the terrain shell,
which exceeds `deep_penetration_m = 50` and is ruled a wall strike (`[ground]`
comment: "can only be a tunnel-wall graze into solid rock… so it is a CRASH").

The tape's `"net":1` on a `dc` event is the **previous tick's** flag
(`app/conquest_tape.h:361` prints `prev.pos` / `prev.net`), so a tape in-net death
literally means *inside the net one tick, outside the next* — a wall strike.
This is confirmed by the probe: `sd_before` at every fatal tick is between
**−0.02 and −0.73 m**.

---

## 2. Probe construction (fidelity)

| piece | how it is built | source mirrored |
|---|---|---|
| `AircraftParams` | `cfg::load_aircraft_toml(config/aircraft.toml)` | main.cpp:466 |
| `GameParams` | `cfg::load_game_toml(config/game.toml, ap)` | main.cpp:472 |
| `ScenarioParams` ⇒ `DroneParams` incl. `[maverick]` | `cfg::load_scenario_toml(config/scenario.toml, ap)` | main.cpp:473 |
| `HeightField` | `assets/sudbury_dem.png`, `dem16_unpack(r,g)`, `R=15000`, `relief=350`, `u_offset=0.806`, **no blur** (`procedural=1` forces `dem_blur_radius=0`) | render/planet.cpp:290-360 |
| `TunnelNet` | `world::build_tunnel_net(tp_from_game_toml, &hf)` — all 24 `[tunnel]` fields | main.cpp:1197-1225 |
| `Environment` | `ground`+`ground_params` from `[ground]`, `tunnels=&net` | main.cpp:1105-1227 |
| fleet spawn | 10 mavericks, conquest golden-angle disc scatter per faction centre, `spawn_alt=2000` | main.cpp:1377-1421 (verbatim) |
| player | `nullptr` (no pursuit, `d.engaged` never set) | conquest with foe none |

**Not modelled:** `env.atm` (bubbles), `env.obstacles` (buildings), the player, and
gunfire. Neither can touch the in-bore leg — `sim::atm_frac_at` overrides to full
air inside the net and buildings are surface prisms.

**Fidelity check — the harness reproduces the tape:**

| | tape `conquest_tape_1.jsonl` | probe (`fleet.exe --min=60`) | Δ |
|---|---|---|---|
| Errington net-entry point | `[-8149.018, -1029.223, 12671.018]` | `[-8149.3, -1027.9, 12671.9]` | **1.6 m** |
| Errington net-entry point | `[-8142.072, -994.207, 12679.546]` | `[-8141.6, -996.5, 12679.7]` | **2.3 m** |
| in-net death (drone 7) | `[-7498.222, -1071.303, 12654.546]`, 3.73 s, 652 m in | `[-7486.9, -1073.8, 12653.9]`, 3.79 s, 664 m in | **12 m / 0.06 s** |
| in-net death (drone 9) | `[-7581.663, -1268.039, 12602.332]`, 3.62 s, 629 m in | `[-7598.4, -1264.6, 12603.5]`, 3.50 s, 612 m in | **17 m / 0.12 s** |

Files: `fleet.cpp` (fleet arm), `probe.cpp` (fixture arm), `hf_load.h` (DEM),
logs `fleet60.txt` / `cert60.txt` / `both_run.txt` / `combo60.txt`,
per-tick traces `fleet_trace2.txt`, spine dumps `profile_REAL.txt` / `profile_CERT.txt`.

Build (from `D:\seads_sandboxes\ai-probes\f4`):

```
g++ -std=c++20 -O0 -g -I. -ID:/flight_sim2/seads-recon \
  -ID:/flight_sim2/seads-recon/build/_deps/glm-src \
  -ID:/flight_sim2/seads-recon/build/_deps/tomlplusplus-src/include \
  -o fleet.exe fleet.cpp \
  D:/flight_sim2/seads-recon/config/load_{aircraft,game,scenario,world}.cpp \
  D:/flight_sim2/seads-recon/sim/step.cpp \
  D:/flight_sim2/seads-recon/world/{tunnel_net,heightfield,buildings}.cpp
```
(`-O1`/`-O2` segfault in this scratch harness; `-O0` is what all results below used.)

---

## 3. Result — the real net, the real schedule, no forced fixture

`./fleet.exe --min=60` (60 min sim, 10 mavericks, real dials):

| entry mouth | net entries | in-net deaths | clean exits |
|---|---|---|---|
| **Errington** | 27 | **23 (85 %)** | 7 |
| **Murray** | 9 | **2 (22 %)** | 4 |
| total | 36 | **25 (69 %)** | 11 |

(+33 surface crashes outside the net — a separate, unrelated finding.)

Death sites (25 events, deduplicated by site):

| n | site | t after entry | dist from Errington mouth | speed | surface normal | `tube_sd<0` |
|---|---|---|---|---|---|---|
| **15** | Errington ramp, lateral | 3.50 – 4.10 s | 469 – 581 m | 181.6 – 183.5 | `n_lat` **0.98–1.00** | yes |
| **6** | Errington pit crown | 1.45 s | 145 m | 181.6 | `n_up` **−1.00** (ceiling) | no (pit volume) |
| **2** | Errington ascent leg (entered at Murray) | 63.1 – 63.3 s | 1 622 – 1 642 m | 123.5 | `n_lat` **0.99** | yes |
| **1** | Murray bowl on exit | 75.6 s | (286 m from Murray) | 122.3 | mixed | no |
| **1** | Murray mouth on entry | 0.66 s | — | 179.6 | mixed | no |

**Every leg of the failure is the Errington leg**, in both travel directions
(24 of 25 deaths). The Murray end, with its 450 m open bowl, is survivable —
exactly the asymmetry the tape shows (Errington-entry drones died in ~4 s;
Murray-entry drones lived 19 s and 54 s and died far down the line).

---

## 4. Mechanism — the trace

`fleet.exe --min=15 --trace=…` for GULCH's fatal Errington run (drone 0, dies at
tick 34986). Columns: `lat` = cross-track from the spine (+ = right of travel),
`vert` = height above the centreline, `gamma` = flight-path angle, `phi` =
`control::extract` bank (== actual), `cmd_bk` = `maverick_step`'s commanded bank,
`az_err` = azimuth of the bore axis relative to the plane's own track.

```
   tick  mode     s_est     sd      lat    vert   gamma    phi  cmd_bk  az_err     V
  34608  RUN        0.0 -120.6   +17.17  +45.71  -49.08 -15.78   +2.16   +3.7°  177.0   <- crossed mouth 55° nose-low
  34644  RUN       15.9  -97.5   +13.22  +14.15  -42.19  -8.42  +15.92   +7.8°  178.1   <- pitch pulling out of the dive
  34680  RUN       67.5  -73.7    +6.25   +3.26  -32.26  +3.20  +31.89  +10.5°  178.7   <- vertical error nulled; track already 10° off
  34704  RUN      102.7  -71.0    +0.59   +0.54  -25.92 +11.84  +38.57  +10.2°  178.8   <- on the centreline, wrong heading
  34764  RUN      190.8  -75.1   -13.60   +4.62  -19.16 +33.18  +65.00   +9.7°  179.2   <- command SATURATES at bank_cap
  34840  RUN      ~330   -63.0   -36.58        …        +51.61  +65.00  +17.9°  180.3   <- error still GROWING
  34912  RUN      ~415   -34.6   -71.18        …        +57.28  +65.00  +24.0°  181.5   <- peak error, turn finally biting
  34960  RUN      471.8  -13.1   -96.53   -6.83  -29.50 +59.08  +65.00  +22.1°  182.3   <- wall_guard latches (0.3 s left)
  34986  RUN      509.1   -0.06 -109.08  -10.52  -29.19 +59.79  +65.00  +21.0°  182.8   <- DEAD, lat == -tube_width
```

Read it:

1. **Entry state is wrong, not the geometry.** The spine here is a clean, straight,
   constant −24.7° ramp (`profile_REAL.txt` nodes 0–15: gamma −24.72 → −24.99°,
   chord-straight to <2 m). There is no kink, no bend, nothing to clip.
   `profile_CERT.txt` is **identical** over this stretch.
2. **The plunge is 30° steeper than the bore.** `dive_gamma_cap = 55°` is
   *saturated* through the whole DIVE_IN (`gamma ≈ −54.5°` for the last 1.5 s
   before the mouth). The bore descends 24.7°.
3. **The pull-out is flown banked, and that is what makes the azimuth error.**
   From tick 34608 to 34704 the *vertical* error goes +45 m → +0.5 m (the pitch
   channel does its job) — but over the same 0.8 s the **track azimuth error grows
   3.7° → 10.2°** with the cross-track still ≈0. The plane is on the centreline
   pointing the wrong way.
4. **Speed.** 177–183 m/s in the bore. `tunnel_speed_max = 135` is the dial the
   comments call *"the honest bore-speed ceiling the closed-loop autopilot threads"*
   — the DIVE_IN plunge overshoots it by 35 % and the autothrottle cannot shed it
   (throttle floors at 0; gravity does the rest).
5. **The roll channel is saturated and too slow.**
   `cmd_bk` hits the 65° `bank_cap` at tick 34764 and never leaves it; `phi` lags
   to 59.8° by impact (first-order hold at `bank_p = 0.5`). At 182 m/s a 65° bank
   gives `R = V²/(g·tan 65) =` **1 574 m** — measured turn rate ~10 °/s. Over the
   2.5 s of saturation the azimuth error still *grows* to 24° before reversing.
6. **The wall guard is decorative here.** `update_wall_guard` latches on
   `sd > −margin` (margin 14–28 m by trait). It fires at tick 34944 — **0.35 s
   before impact**, at −88 m cross-track and 55 m/s of closing rate. All it does is
   double `centerline_gain`, which pushes an already-saturated bank command
   further into the clamp. It cannot save anything.
7. **The lookahead is the wrong scale.** `la = lookahead_m · max(0.5, V/120)` =
   `220 · 1.52` = **334 m**, against a 1 574 m turn radius. And the bank aim point
   is `ahead + cg·(here − pos)`: at −60 m of cross-track with `cg = 2` that is a
   120 m lateral pull against 334 m of lead → 20° bearing → `k_az = 3.5` → 70°
   command, already past the cap. The proportional signal is dead for the whole
   event; the loop is effectively open.

**Cause chain:** saturated 55° dive → 183 m/s + banked pull-out → 10° track error
with zero cross-track → roll command saturated at 65° for 2.5 s → 1.6 km turn
radius cannot bend 10–24° inside a 220 m bore → cross-track reaches 110 m
(`tube_width_m`) at 580–720 m → outside `contains()` → `deep_penetration_m` crash.

The second, smaller site (6/25, BOREAL only, 145 m in, `n_up = −1.00`) is the same
disease one stage earlier: the saturated dive line clips the **crown of the sunken
Errington mouth pit** before the bore proper. `mouth_sink_m = 130`, pit rim radius
only **143 m** (vs Murray's 450 m bowl) — there is essentially no margin at the home
mouth for a 55° plunge that arrives off-axis.

---

## 5. Cross-check against `conquest_tape_1.jsonl`

The 4 in-net `dc` events, all `mav:3` (RUN):

| tape | entered | via | died | t in net | dist from entry | dist from Errington mouth |
|---|---|---|---|---|---|---|
| i7 | k 30046 `[-8149,-1029,12671]` | **ERR** | k 30494 `[-7498,-1071,12655]` | **3.73 s** | 652 m | 512 m |
| i9 | k 60160 `[-8142,-994,12680]` | **ERR** | k 60595 `[-7582,-1268,12602]` | **3.62 s** | 629 m | 485 m |
| i2 | k 48708 `[3554,-4597,13996]` | MUR | k 50966 `[372,-3387,13170]` | 18.82 s | 3 503 m | 8 721 m |
| i4 | k 31742 `[3552,-4604,13996]` | MUR | k 38165 `[-5715,-902,9690]` | 53.52 s | 10 868 m | 3 707 m |

Probe (60 min, real dials) reproduces the Errington pair to **12–17 m and 0.1 s**
(§2 table) and clusters 15 of its 25 deaths in that same 469–581 m band. The two
"~600 m in 4 s" deaths are **the Errington mouth**, and what is there under the real
dials is exactly what §4 describes: a straight 24.7° ramp entered at 55° nose-down,
183 m/s, with 110 m of side clearance and a 1.6 km turn radius.

The Murray-entry pair (19 s / 54 s) also matches in kind: the probe's two
Murray-entry deaths happen at **63.1 s / 63.3 s, 1 622 m from the Errington mouth**,
laterally (`n_lat 0.99`) on the far Errington **ascent** — i.e. Murray-entry drones
also die on the Errington leg, just after crossing the arena. Tape i4 dies 3.7 km
from Errington, same leg. (Timing differs because the tape run had a player in the
world, which perturbs the schedule and the transit.)

Player counterfactual, same tape: `who:-2` entered at Errington k 63053 and exited
at Murray k 79023 — **133 s, no crash**. A human flies the entry line at ~100 m/s
and does not enter in a saturated 55° dive.

---

## 6. Discriminating experiments

### 6a. REAL vs CERT tunnel dials — the dials are innocent

`test/unit/test_maverick.cpp::test_tp()` differs from `config/game.toml [tunnel]` in
exactly three live fields:

| field | CERT (`test_tp`) | REAL (`game.toml`) |
|---|---|---|
| `floor_height_m` | 0.0 | **20.0** |
| `arena_a_m` | 7350.0 | **4200.0** (T13 centering) |
| `chambers_on` | true | **false** |
| `headframe_on` | (unset → false) | true (no SDF — visual only) |

Plus the certificates build the net over a **uniform 300 m** `HeightField`, not the
Sudbury DEM.

Fleet arm, 60 min, identical seedless schedule, real DEM under both:

| tunnel dials | Errington entries / deaths | Murray entries / deaths | total |
|---|---|---|---|
| **REAL** | 27 / **23** | 9 / 2 | 36 / **25 (69 %)** |
| **CERT** | 27 / **23** | 9 / 2 | 36 / **25 (69 %)** |

Event lists are identical tick-for-tick. Reason: all the differing fields live in the
**arena/floor/chamber**, and 24 of 25 deaths happen on the Errington **bore ramp**,
where `profile_REAL.txt` and `profile_CERT.txt` agree to the last digit (§4.1).
`floor_height_m` only truncates 20 m off the bottom of a 90 m vertical semi-axis; the
deaths are lateral, at 110 m.

> **The historic "certificates never ran the real dials" gap is real but is not the
> bug.** Running them on the real dials changes nothing.

### 6b. What the certificates actually missed — the entry state

`test_maverick.cpp::dive_in_drone` drops the maverick at **exactly** the approach
point, **exactly** on the bore tangent, with **zero** cross-track. `probe.exe`
re-flies that fixture verbatim while sweeping the approach point laterally
(±40 / ±100 / ±200 m). 10 traits × 7 offsets × 2 mouths × 2 dial sets:

| entry mouth | \|lat offset\| | REAL dials | CERT dials |
|---|---|---|---|
| Errington→Murray | **0 m** (the certificate) | **0/10 died** | **0/10 died** |
| Errington→Murray | 40 m | **20/20 died** | 20/20 died |
| Errington→Murray | 100 m | 20/20 died | 20/20 died |
| Errington→Murray | 200 m | 20/20 died | 20/20 died |
| Murray→Errington | 0 m | 0/10 died | 0/10 died |
| Murray→Errington | 40 m | 0/20 died | 0/20 died |
| Murray→Errington | 100 m | 20/20 died | 20/20 died |
| Murray→Errington | 200 m | 20/20 died | 20/20 died |

The certificate leg is **green under the real dials** — it is honest for what it
tests. But it tests a measure-zero perfect entry. **40 m of approach cross-track at
Errington is already 100 % fatal**, and the mode machine's own arm gate legally
admits `transit_line_tol_m = 300 m` of cross-track before it will fire DIVE_IN.
That is the blindness.

### 6c. Knob counterfactuals (fleet arm; probe-local overrides, no repo edit)

| arm | in-net deaths / entries | Errington deaths / entries |
|---|---|---|
| **baseline** (30 min) | 11/16 (69 %) | 9/11 |
| `dive_cap_deg = 26` | 7/7 (100 %) | 0/0 — *the Errington approach stops working entirely* |
| `approach_alt = 200` | 13/16 (81 %) | 12/12 |
| `k_az = 8, bank_p = 1.5` | 1/1 | 0/0 — *fleet can no longer reach a mouth* |
| `centerline_gain = 0.7` | 9/15 (60 %) | 9/10 |
| `lookahead = 350` | 10/16 (62 %) | 10/11 |
| **`lookahead = 500`** | **1/12 (8 %)** | **1/7** |
| `lookahead = 800` | 1/12 (8 %) | 1/7 |
| **`lookahead = 500` + `centerline_gain = 1.0`** | **0/12 (0 %)** | **0/7** |

At 60 min (larger N):

| arm | in-net deaths / entries | Errington deaths / entries |
|---|---|---|
| baseline | 25/36 (**69 %**) | 23/27 (85 %) |
| `lookahead = 500` | 6/29 (**21 %**) | 6/18 (33 %) |
| `lookahead = 500` + `centerline_gain = 1.0` | 6/29 (**21 %**) | 5/17 (29 %) — **4 of the 5 are the same repeated BOREAL pit-crown clip**, not the lateral mode |

The lateral bore-wall mode — 15 of 25 baseline deaths — is essentially **eliminated**
by the lookahead change alone. What survives at 60 min is the *pit-crown* clip at
145 m (§4, mode B), a distinct defect at the mouth geometry, not in the bore.

This is the signature of the diagnosis: the lookahead is the one knob that changes
the saturated-pursuit geometry, and it is the one knob that fixes it.

---

## 7. Recommended MINIMAL fix direction (not implemented)

**Rung 1 — config only, `config/scenario.toml [maverick]`.**
`lookahead_m: 220 → 500` and `centerline_gain: 2.0 → 1.0`. The pure-pursuit aim
distance must be comparable to the *achievable turn radius at the speed actually
flown* (1.6 km at 182 m/s / 65° bank), and the cross-track term must not dominate
the lead vector into permanent `k_az` saturation. Measured effect above. Zero logic
change; both are already declared feel dials.

**Rung 2 — the honest speed, small and structural.**
`tunnel_speed_max = 135` is fiction inside `DIVE_IN`: the plunge is flown at the
saturated 55° `dive_gamma_cap` and arrives 35 % hot with no way to shed it (the
autothrottle floors at 0). Either bound the dive by the energy it will carry
(cap `dive_gamma_cap` by current speed vs `tunnel_speed_max`, or start the descent
further back so the plunge sits nearer the bore's own −24.7°), **or** simply accept
the speed and size rung 1 to it. Do *not* just lower `dive_gamma_cap` — the
`dive_cap_deg = 26` arm killed the Errington approach outright.

**Rung 3 — make the wall guard a guard.**
`update_wall_guard` latches on present `signed_distance`; measured, it fires
0.35 s before impact with 55 m/s of closing rate — it can never act in time.
It should latch on *projected* wall contact (closing rate × time-to-wall against the
roll time constant), and when it latches the response should be something the plane
can execute, not a doubling of an already-clamped gain.

**Rung 4 — close the certificate hole (this is what would have caught it).**
`test_maverick.cpp::dive_in_drone` must be parameterised over the entry states the
mode machine legally produces: cross-track up to `transit_line_tol_m = 300 m`, entry
bank up to the transit cap, and the real dive-carried speed. And at least one leg
must fly the **whole** `PATROL → TRANSIT → DIVE_IN → RUN → CLIMB_OUT` pipeline over
the **real DEM** with the **real `[tunnel]` dials** and assert a survival rate, not
just a single hand-placed drop. `fleet.cpp` in this directory is a working template.

**Not a fix:** touching the tunnel geometry. Under both dial sets the Errington ramp
is a straight, clean 24.7° bore and the run dies in it for control reasons.
(Though the 143 m Errington pit rim vs Murray's 450 m bowl is worth a separate look —
it owns failure mode B, the 145 m crown clip.)
