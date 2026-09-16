# FIX-F4b offset-witness adjudication — why `test_maverick_offset.cpp` 120 m/SHAFT went red

**Instrument:** `D:\seads_sandboxes\ai-probes\f4\off\offprobe.cpp` (+ `geom.cpp`), built `-O0` from
repo headers/TUs. **Nothing in `D:\flight_sim2\seads-recon` was modified** — the only repo delta is
the pre-existing FIX-F4b `config/scenario.toml` edit and the untracked
`docs/ai_phase2_records/f4b_pitcrown_attribution.md`. Facts only; the ruling is Fable's.

---

## 0. TL;DR

1. The failure reproduces exactly (`ctest -R "off-axis"`, section at
   `test/unit/test_maverick_offset.cpp:251`, `REQUIRE_FALSE(out.crashed)` at line 268).
2. **SHAFT @ 120 m under `dive_lookahead_m = 250` dies a LATERAL-class death on the Errington
   approach-TRENCH wall, 378 m OUTSIDE the mouth, 20 m below grade, having never come within 215 m
   of the pit.** `n_up = +0.010`, `n_lat = +0.980`, nearest primitive `trench[0]`,
   `pit_rad = 358.5 m` against a 143 m rim, `pit_sd = +215.5`.
   **It is NOT the pit-floor class.** No death anywhere in this fixture, at either dial, at any
   offset, strikes the pit.
3. **It is not a uniform-field geometry artifact either.** The fixture net and the real-DEM net
   produce the *same* DIVE_IN aim line: at a 120 m offset the tick-0 bank command is
   **+7.28° (fixture) vs +7.29° (real DEM)** for a 250 m lead and **+4.81 / +4.82°** for 150 m. The
   bore is laterally dead-straight for the first 500 m in both nets. `mouth_sink` is 130.00 m below
   *local* grade in both; pit depth 220.0 and rim 143.0 in both.
4. The fixture-specific ingredient is the **ENTRY STATE, not the geometry**:
   `dive_in_drone_offset` points the nose *exactly at the mouth node* while displacing the aircraft
   laterally, so the bearing to `s=0` is zero **by construction**. In that reference the longer lead
   is *further* off the nose (+2.079° vs +1.375° at 120 m), so 250 injects **51 % more** initial
   bank command. Measured off the **bore tangent** — the reference a real TRANSIT hand-off uses
   (`transit_align_min = 0.85`, velocity aligned to `tan_h`) — the sign flips: 250 gives a
   **smaller** bearing (−6.951° vs −7.655°), which is exactly the desaturation the fleet probe
   measured. **The dial's sign of effect depends on which of the two legal entry references the
   aircraft arrives on.**
5. Net effect on this fixture: the survivable entry-offset envelope shrinks from **0–117 m**
   (dl 150) to **0–82 m** (dl 250). The test's chosen witness (120 m, idx 1) was the single
   knife-edge cell of the whole matrix — the one cell where trait choice decided the outcome.

---

## 1. Reproduction

```
$ ctest --test-dir build -C Debug -R "off-axis" --output-on-failure
1/3 Test #1: maverick tunnel run: off-axis Errington entries survive at 0/40 m;
             120 m survives for the SHAFT trait ..... ***Failed
  120 m offset survives (within TRANSIT's own transit_line_tol_m envelope)
  test_maverick_offset.cpp:268: FAILED: REQUIRE_FALSE( out.crashed ) with expansion: !true
```
0 m, 40 m and the anti-vacuity old-dials leg pass.

The standalone probe replicates `fly_offset_run` verbatim (same `uniform_field(300)`, same
`test_tp()`, same `tunnel_ground_params()`, same `dive_in_drone_offset`, same 300 s cap) and
reproduces it:

```
$ offprobe.exe --off=120 --idx=1              # repo dials, dl = 250
TRACE idx=1 off=120 dl=250 -> entered=1 crashed=1 patrol=0 ticks=732
$ offprobe.exe --dl=150 --off=120 --idx=1     # the old dial
TRACE idx=1 off=120 dl=150 -> entered=1 crashed=0 patrol=1 ticks=10973
```

---

## 2. HOW it dies under 250 — trace excerpt

`lat` = cross-track off the spine, `vert` = height above the centreline, `cbk/cgm` = *commanded*
bank/γ from a shadow `maverick_step`, `pit_ax/pit_rad` = pit-frame depth/radius (floor 220, rim 143),
`trsd` = trench sd, `agl` = height above the uniform 300 m shell.

```
tick  mode     s      sd[owner]      lat     vert    gam    cbk    cgm     V  in wg pit_ax pit_rad  tube   trsd    agl
   0  DIVE_IN 0.0   716.6[pit]    -116.2  +889.8 -50.24  -7.28 -55.00 135.0  0  0 -722.6   782.7 1123.0  723.5  741.7
 150  DIVE_IN 0.0   618.9[pit]     -94.1  +780.1 -45.89 -61.82 -55.00 141.4  0  0 -624.9   641.1  953.7  624.4  637.8  <- bank cmd near cap
 200  DIVE_IN 0.0   573.6[pit]     -83.2  +732.0 -56.02 -65.00 -55.00 144.0  0  0 -579.6   603.1  895.1  578.8  591.1  <- SATURATES, never recovers
 500  DIVE_IN 0.0   239.4[trench]   +6.4  +384.4 -64.66 -65.00 -55.00 162.0  0  0 -241.7   447.1  564.5  239.4  248.1  <- cross-track CROSSES the spine
 700  DIVE_IN 0.0    10.4[trench] +126.8  +151.6 -53.90 -65.00 -55.00 171.9  0  0  -12.8   368.0  374.1   10.4   17.2
 710  DIVE_IN 0.0   -15.9[trench] +133.2  +139.9 -53.89 -65.00 -55.00 172.4  1  0   -1.3   364.9  364.5  -15.9    5.6  <- contains() via the TRENCH at grade
 711  RUN     0.0   -15.2[trench] +133.9  +138.7 -53.89 -65.00 -44.00 172.4  1  0   -0.1   364.5  363.5  -15.2    4.4  <- RUN commands -44 (saturated)
 715  RUN     0.0   -12.3[trench] +136.4  +134.1 -53.88 -65.00 -44.00 172.6  1  1    4.5   363.3  359.8  -12.4   -0.2  <- crosses grade INSIDE the trench cut
 732  RUN     0.0    -0.1[trench] +147.4  +114.2 -54.00 -65.00 -44.00 173.5  1  1   24.2   358.5  344.2   -0.1  -20.0  <- DEAD: exits the trench SIDE wall
DEATH tick=732 mode=RUN pos=[-8473.3 -1078.3 12669.6] nearest=trench sd=-0.12
      tube=344.2 pit=215.5 trench=-0.1 bowl=12181.6 throat=12334.0
      pit_ax=24.23 (pit depth 220.00)  pit_rad=358.53 (rim 143.00)
      n_up=+0.010  n_lat=+0.980  n_along=+0.177  trenchstep=0  agl=-20.02  V=173.5
```

**Mechanism, stated as measurement:**

* The roll channel **saturates at the −65° `bank_cap` by tick ~185 and never desaturates** for the
  remaining 550 ticks. Cross-track therefore runs **monotonically −116 → 0 (tick ~490) → +147 m**:
  the plane crosses the spine and keeps going. Classic saturated pure-pursuit divergence.
* `contains()` fires at tick 710 on the **trench** (the up-tangent approach open cut), at grade,
  **378 m short of the mouth node**, with `s_est` pinned at 0 — RUN then commands the −44° cap.
* Death is a **wall exit, not a floor pancake**: net `sd` climbs monotonically −15.9 → −0.1 while
  `agl` falls 5.6 → −20.0. The aircraft is flying *out through the trench's lateral wall* into
  solid rock 20 m under grade. `n_up = +0.010` (surface normal horizontal), `n_lat = +0.980`.
* The pit is never a factor: at death `pit_sd = +215.5 m`, `pit_rad = 358.5 m` against a 143 m rim,
  `tube_sd = +344.2 m`. It dies at the *outermost* trench step (`trench[0]`, depth 83.3 m,
  r = 150 m, centred 378.5 m from the mouth).

### The dl = 150 counterfactual — what path it takes instead

```
tick  mode     s      sd[owner]      lat     vert    gam    cbk    cgm     V  in wg pit_ax pit_rad  trsd    agl
 200  DIVE_IN 0.0   573.1[pit]     -85.4  +731.5 -57.37 -65.00 -55.00 144.1  0  0 -579.1   603.5  578.3  590.6
 300  DIVE_IN 0.0   468.4[pit]     -62.0  +622.6 -62.33 -20.69 -55.00 150.6  0  0 -474.4   541.6  472.9  483.6  <- DESATURATES
 350  DIVE_IN 0.0   412.7[pit]     -54.3  +565.1 -63.91 +41.68 -55.00 153.7  0  0 -418.7   511.6  417.0  427.0  <- roll REVERSES
 700  DIVE_IN 0.0    26.5[trench]  -69.8  +165.1 -55.10 +61.00 -55.00 171.7  0  0  -31.2   246.0   26.5   33.2
 723  DIVE_IN 0.0   -68.5[trench]  -71.8  +137.7 -55.27 +65.00 -55.00 172.9  1  0   -4.3   228.3  -68.5    6.0  <- enters 68 m INSIDE the cut
 724  RUN     0.0   -68.0[trench]  -71.8  +136.5 -55.27 +49.95 -44.00 172.9  1  0   -3.2   227.5  -68.0    4.9  <- bank cmd unsaturated
...  reaches the bore, CLIMB_OUT at tick 9307, PATROL at 10973 — survives.
```

The discriminator is identical in shape to the fleet doc's, with the **sign of the dial's effect
reversed**: under 150 the bank command **desaturates at tick ~300 and reverses**, parking the plane
at ≈ −70 m cross-track on the *inboard* side, entering the trench 68 m inside the cut and
converging. Under 250 it never desaturates.

| at the moment `contains()` fires | dl = 250 (FATAL) | dl = 150 (SURVIVES) |
|---|---|---|
| cross-track off the spine | **+133.2 m** | −71.8 m |
| net sd at entry | **−15.9 m** (grazing the wall) | −68.5 m (68 m inside) |
| commanded bank | **−65.00° SATURATED since tick 185** | +65.00°, unsaturated 300–720 |
| pit radius | 364.9 m | 228.3 m |
| flown γ | −53.89° | −55.27° |
| V | 172.4 | 172.9 |
| subsequent cross-track | 133 → **147, diverging** | −72 → −73, **flat/converging** |

Speed and dive angle are again **not** the discriminator (0.5 m/s, 1.4° apart). The lateral entry
state is.

---

## 3. Survival matrices (this fixture, `run_dir = +1`, offsets 0–200 m by 20 m)

`S` = reached PATROL · `C` = crashed · lowercase = never entered the net. Tick count in parens.

### 3a. NEW dial — `dive_lookahead_m = 250`

| offset m | idx 0 (BOREAL) | idx 1 (SHAFT) | idx 2 (STOPE) |
|---|---|---|---|
| 0 | S (10850) | S (10850) | S (10850) |
| 20 | S (10854) | S (10854) | S (10854) |
| 40 | S (10873) | S (10873) | S (10875) |
| 60 | S (10928) | S (10927) | S (10928) |
| 80 | S (10961) | S (10959) | S (10971) |
| **100** | **C (822)** | **C (822)** | **C (822)** |
| **120** | **C (732)** | **C (732)** | **C (732)** |
| 140 | C (724) | C (724) | C (724) |
| 160 | c (730) | c (730) | c (730) |
| 180 | c (737) | c (737) | c (737) |
| 200 | c (743) | c (743) | c (743) |

### 3b. OLD dial — `dive_lookahead_m = 150` (reproduces the TU authors' sweep exactly)

| offset m | idx 0 | idx 1 (SHAFT) | idx 2 |
|---|---|---|---|
| 0 | S (10850) | S (10850) | S (10850) |
| 20 | S (10852) | S (10852) | S (10852) |
| 40 | S (10859) | S (10859) | S (10859) |
| 60 | S (10874) | S (10874) | S (10878) |
| 80 | S (10914) | S (10913) | S (10918) |
| 100 | S (10956) | S (10953) | S (10958) |
| **120** | **C (1283)** | **S (10973)** | **C (1282)** |
| 140 | C (828) | C (828) | C (828) |
| 160 | c (709) | c (709) | c (709) |
| 180 | C (777) | C (777) | C (777) |
| 200 | C (776) | C (776) | C (776) |

The TU's comment ("idx 0/2 still crash at 120 m … idx 1 survives cleanly") is confirmed byte for
byte. **120 m / idx 1 is the ONLY trait-dependent cell in either matrix.**

### 3c. The envelope boundary, pinned to 1 m

| dial | last surviving offset | first fatal offset | first-fatal class |
|---|---|---|---|
| 150 | **117 m** (115 S, 118 C, idx 0) | 118 m | bore **tube** lateral wall, `n_lat = 0.935`, `tube_sd ≈ 0`, lat = +105.0 m, 369 m below grade, tick 1290 |
| 250 | **82 m** (82 S, 83 C, idx 0) | 83 m | **identical**: bore tube lateral wall, `n_lat = 0.936`, lat = +105.1 m, 380 m below grade, tick 1308 |

The **boundary death class is unchanged by the dial** — the same STOPE-style lateral bore-wall
strike at the same +105 m cross-track. Only the offset at which the envelope closes moved,
117 m → 82 m (−30 %).

### 3d. Death-class census (every fatal cell, both dials)

| dial | offset | nearest | `n_up` | `n_lat` | site |
|---|---|---|---|---|---|
| 250 | 83–85 | tube | −0.32 | **+0.935** | bore lateral wall, ~380 m deep |
| 250 | 90–100 | trench[2] | **−1.000** | −0.006 | trench-step **FLOOR** (`ax = 130.1` of depth 130.0), 129 m deep |
| 250 | 105 | trench[1] | −0.013 | **−0.988** | trench lateral wall |
| 250 | 110–140 | trench[0] | +0.010 | **+0.97…0.98** | trench lateral wall, at/just below grade |
| 250 | 160–200 | (outside net) | +0.0…0.05 | **+0.96…1.00** | ploughs the trench rim at grade, `contains()` never true |
| 150 | 118–120 | tube | −0.32 | **+0.935** | bore lateral wall, ~369 m deep |
| 150 | 125–140 | trench[2] | **−1.000** | −0.005 | trench-step **FLOOR** |
| 150 | 160 | (outside net) | +0.010 | **−0.953** | trench rim at grade |
| 150 | 180–200 | trench[0] | +0.010 | **+0.98** | trench lateral wall |

**No cell, at either dial, at any offset or trait, dies on the `pit`.** The pit-floor class the
fleet probe measured (and eliminated) has no instance in this fixture. A floor-class death
(`n_up = −1.000`) *does* exist here — but it is the **trench step 2 floor**, and it exists under
**both** dials (250 at 90–100 m, 150 at 125–140 m); the dial shifted it 30 m in offset, it did not
create or remove it.

---

## 4. Fixture geometry vs the real DEM

`geom.exe` builds both nets side by side (fixture: uniform 300 m field + `test_tp()`; real:
`game.toml [tunnel]` over `assets/sudbury_dem.png`).

| | FIXTURE (uniform 300 m) | REAL (Sudbury DEM) |
|---|---|---|
| Errington mouth node | `[-8115.7, -1047.3, 12773.7]`, `|p| = 15169.99` | `[-8006.1, -1033.1, 12601.3]`, `|p| = 14965.25` |
| terrain radius at the mouth | 15299.99 (**+300.0** over R) | 15095.25 (**+95.25** over R) |
| **mouth below local grade** | **130.00 m** | **130.00 m** |
| `pit.surface_r` | 15299.99 | 15095.25 |
| `pit.bowl_depth` / `bowl_r` / `open_floor_r` | **220.0 / 143.0 / 0.0** | **220.0 / 143.0 / 0.0** |
| trench[0/1/2] depth | 83.33 / 106.67 / 130.0 | 83.33 / 106.67 / 130.0 |
| trench[0/1/2] radius | 150 / 150 / 150 | 150 / 150 / 150 |
| trench[0/1/2] `surface_r − R` | 299.99 / 299.99 / 299.99 (flat) | **81.80 / 86.27 / 90.43** (real 8.6 m tilt over the cut) |
| trench step distance from mouth | 378.5 / 285.3 / 200.3 | 369.4 / 278.1 / 195.6 |
| route L | 12043.6 | 11906.9 |
| bore lateral deviation, s = 0…500 | **0.00 m** (straight) | **0.00 m** (straight) |
| bore vertical drop at s = 150 / 250 | −62.22 / −104.60 | −62.72 / −105.45 |

**`mouth_sink_m` is measured against the LOCAL terrain radius in both builds**, so the flat 300 m
shell does *not* raise or lower the pit relative to grade — it only shifts the whole assembly
outward by ~205 m in absolute radius and flattens an 8.6 m real terrain tilt across the trench.

### The decisive number — the 250-lookahead aim line is the SAME in both nets

Bank command a fixture-style drone reads at the approach point (`850 m back, 500 m up, offset`),
`k_az = 3.5`, `bank_cap = 65°`:

| lateral offset | lead | bearing off the **bore tangent** | bearing off the **fixture NOSE** | ⇒ commanded bank |
|---|---|---|---|---|
| 120 m, FIXTURE | s = 150 | −7.655° | +1.375° | **+4.81°** |
| 120 m, FIXTURE | s = 250 | −6.951° | +2.079° | **+7.28°** |
| 120 m, REAL DEM | s = 150 | −7.671° | +1.378° | **+4.82°** |
| 120 m, REAL DEM | s = 250 | −6.966° | +2.084° | **+7.29°** |

Agreement to **0.02°**. The uniform field changes the aim line by nothing measurable.

### What DOES differ — the entry reference, and it flips the dial's sign

The two columns above are the same geometry read against two different references, and they
disagree in sign:

* **Off the bore tangent** (the reference a real `TRANSIT → DIVE_IN` hand-off produces: the arm gate
  requires `dot(v̂, tan_h) > transit_align_min = 0.85`, i.e. velocity within ~32° of the horizontal
  bore direction, from an approach point that is *on* the bore line): 250 gives
  **|bearing| 6.951° < 7.655°** — a **9 % smaller** bearing. Longer lead = softer pursuit =
  desaturation. **This is the fleet-probe effect** (BOREAL: 132 m pit radius / −65° pinned → 88.9 m /
  +19.2° unsaturated).
* **Off the fixture's NOSE**: `dive_in_drone_offset` sets
  `fwd = normalize(entry - approach)` — the nose points *exactly at the mouth node*, so the bearing
  to `s = 0` is **0.000° by construction**. Everything down-bore is then a *positive* bearing that
  **grows with the lead**: +1.375° at 150 → +2.079° at 250, i.e. **+51 %**. Longer lead = *larger*
  initial bank command. That extra 2.5° of tick-0 bank is what starts the roll early, saturates the
  channel by tick 185, and never lets it back off.

Both references are legal states of the mode machine (`transit_align_min` admits up to 31.8° of
track-angle error; the fixture's convergence at 120 m offset is 9.03°). The fixture's construction
happens to sit on the reference where the dial's sign is inverted.

---

## 5. Answer to the class question (Q3)

**Not the pit-floor class.** The pit is never the nearest primitive at any death in this fixture at
either dial; at the 120 m/250 death `pit_sd = +215.5 m` and `pit_rad = 358.5 m` against a 143 m rim.
The strike is 378 m *outside* the mouth, 20 m below grade, and 344 m clear of the bore ellipse.

**It is a LATERAL-class death** (`n_lat = +0.980`, `n_up = +0.010`) — the same family as the fleet
probe's STOPE lateral class, produced by the same saturated-pure-pursuit divergence, but located on
the Errington **approach trench** rather than the bore ramp, because the divergence here is large
enough (+147 m cross-track) to leave the cut before the bore is ever reached.

**It is not a uniform-field / `test_tp()` geometry artifact.** The pit and trench geometry, the
`mouth_sink`-below-grade relationship, and — decisively — the 250-lookahead aim line and its
commanded bank are identical to the real-DEM net to within 0.02°. The flat shell removes only an
8.6 m terrain tilt across the trench steps and shifts the assembly ~205 m outward in absolute
radius; neither touches the entry line.

**The fixture-specific ingredient is the entry STATE.** `dive_in_drone_offset`'s nose-at-the-mouth
heading makes the s = 0 bearing identically zero, which inverts the sign of `dive_lookahead_m`'s
effect on the initial bank command relative to a tangent-aligned TRANSIT hand-off. Under that
reference the FIX-F4b dial makes the entry *worse*, and the fixture's survivable offset envelope
shrinks 117 m → 82 m. Under the tangent reference — the one the fleet flies — the same dial makes
it better, which is what the 150-minute fleet measurement recorded (pit class 19 → 0, in-net deaths
32 % → 8 %).

Both measurements are honest. They are measurements of the same dial against two different entry
references.

---

## 6. Files

| file | what |
|---|---|
| `off/offprobe.cpp`, `offprobe.exe` | standalone replica of `fly_offset_run` + per-tick trace + SDF decomposition + pit-frame forensics + per-trait sweep; `--dl=` `--off=` `--idx=` `--trace=` `--sweep` |
| `off/geom.cpp`, `geom.exe` | fixture-net vs real-DEM-net geometry + aim-line comparator |
| `off/tr_120_dl250.txt` | the fatal SHAFT @ 120 m run, per tick (733 ticks) |
| `off/tr_120_dl150.txt` | the same run under the old dial (survives, 10973 ticks) |
| `off/sweep_dl250b.txt`, `off/sweep_dl150b.txt` | the two survival matrices with per-cell death forensics |
| `off/geom.txt`, `off/geom_bearing.txt` | the geometry tables of §4 |
