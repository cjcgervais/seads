# F4b — the residual Errington mouth death ("mode B"): attribution + counterfactuals

**Status:** attribution only. Nothing in `D:\flight_sim2\seads-recon` was modified — verified by
`git status` staying clean and by cross-checking every headline result against `fleet.exe`, a binary
built entirely from repo headers. All probe code, binaries and logs live in
`D:\seads_sandboxes\ai-probes\f4\`.

Supersedes §4 "mode B" of `docs/ai_phase2_records/f4_tunnel_run_attribution.md` (the Rung-1 fix
`lookahead_m 220→500` / `centerline_gain 2.0→1.0` has since LANDED; this document is about what
survived it).

---

## 0. TL;DR — the mechanism, and the one label that was wrong

**It is not a crown. It is the PIT FLOOR.**

`n_up = -1.00` was read in the prior doc as "a ceiling/crown hit". The sign convention says the
opposite: outside the volume the SDF gradient points *away* from the open space, and the only
surfaces in this net whose gradient points **down** are floors. `bowl_sd`'s floor term is
`floor_sd = ax - bowl_depth` (`world/tunnel_net.cpp:260`), whose gradient is `-pit.axis` — i.e.
`n_up = -1` **exactly**. The forensics confirm it directly: at the fatal tick the nearest primitive is
the **pit**, at `pit_ax = 219.7 m` against `pit.bowl_depth = 220.0 m`.

> BOREAL crosses the Errington pit lip **132 m off the pit axis** (rim 143 m — 11 m of margin), 52 m
> off the bore spine, in a **saturated 55° plunge at 175 m/s with the bank command already pinned at
> the 65° cap**. `contains()` flips at grade — via the **trench** open-cut Bowl, not the pit — and
> DIVE_IN hands to RUN with `s_est = 0`, i.e. the reference centreline station is the bore mouth at
> the **bottom** of the crater. `bore_track`'s altitude residual then reads *"I am 220 m above the
> centreline"* and commands `-44°` — the **run gamma cap, saturated, for the next 0.5 s** — while the
> plane is already at −54.5° with 220 m of rock beneath it. Flown γ deepens to **−68.7°**. Over the
> same 1.4 s the saturated bank command drives cross-track monotonically **52 → 111 m**. The pull-out
> starts at t+1.07 s (γ recovers at ~31–40 °/s, ≈10 g — near the airframe limit) and needs **1.4–1.9 s**;
> it has **0.35 s**. The aircraft strikes the **flat crater floor at 112 m radius**, in the annulus
> between the bore-mouth ellipse (110 × 90 m; `tube_sd = +40.5 m`) and the 143 m rim, at 181.6 m/s.

Byte-identical every time: 19 of 25 in-net deaths over a 150-minute fleet sim are the *same* BOREAL
run at the *same* coordinates.

**Ranked fix:** `dive_lookahead_m 150 → 250` (config-only, one dial). Measured: the pit-floor class
goes **19/51 Errington entries → 0/40**, and BOREAL still flies the tunnel (its RUN occupancy goes
*up* 9 017 → 41 608 ticks). §5.

---

## 1. Baseline — confirming the residual under the landed Rung-1 dials

Verified live in `config/scenario.toml [maverick]`: `lookahead_m = 500.0`,
`dive_lookahead_m = 150.0`, `centerline_gain = 1.0`.

`fleet.exe` was rebuilt against the current repo (`-O0` exactly; `-O1/-O2` still segfault in this
scratch harness) and re-run. `fleet2.exe` is the extended arm (§2). Both reproduce identically.

| | 60 min | 150 min |
|---|---|---|
| Errington entries / in-net deaths | 17 / **5** | 51 / **24** |
| Murray entries / in-net deaths | 12 / 1 | 27 / 1 |
| **total** | 29 / **6 (21 %)** | 78 / **25 (32 %)** |
| surface crashes (outside the net) | 31 | 77 |

### Death-site clusters (150 min, deduplicated)

| n | callsign | site | nearest primitive | t after entry | dist from E mouth | speed | `n_up` | `n_lat` | `tube_sd<0` |
|---|---|---|---|---|---|---|---|---|---|
| **19** | **BOREAL** | **Errington PIT FLOOR** | **pit** | **1.45 s** | **145.0 m** | **181.6** | **−1.000** | 0.01 | no |
| 5 | STOPE | Errington bore ramp, lateral | tube | 3.48–3.51 s | 425 m | 178.7 | −0.03 | **1.00** | yes |
| 1 | BOREAL | Murray bowl wall on entry | bowl | 0.66 s | — | 179.6 | −0.67 | 0.74 | no |

**The pit-floor class is 76 % of all remaining in-net deaths.** All 19 are the identical event:

```
entry  [-8188.4 -1051.2 12647.0]  sd_prev -87.59  V 175.4  run_dir +1
death  [-8000.4 -1125.2 12489.3]  t_in 1.45 s     V 181.6  n_up -1.000
```

**Determinism:** the sim has no RNG and the maverick brain is a pure function of state, so the
periodic schedule re-flies the same run. Confirmed three ways: (i) two independently built binaries
(`fleet.exe`, `fleet2.exe`) produce identical coordinates; (ii) `--min=60` twice is byte-identical
(`diff cen_dl250.txt cen_dl250_b.txt` empty); (iii) the 60/90/150-minute horizons all reproduce the
same per-event fingerprints.

---

## 2. Harness extensions (probe-local only)

`fleet2.cpp` adds, on top of `fleet.cpp`:

* **SDF decomposition** — `sd_parts()` evaluates `tube_signed_distance`, `throat_signed_distance`,
  and a local mirror of the file-static `world::bowl_sd` for the Murray bowl, the Errington pit and
  each of the trench steps; the mirror is validated by requiring `min(parts) == signed_distance` to
  0.05 m (a mismatch is reported as `arena/core`). This is what names *which surface* was struck.
* **Pit-frame coordinates** — `pit_frame()` returns `(ax, rad)`: depth below `pit.surface_r` along
  `pit.axis`, and radius off that axis. The pit is a cylinder of `bowl_r = errington_pit_rim(110) =
  143 m` and `bowl_depth = mouth_sink_m + tube_height_m = 220 m` (`world/tunnel_net.cpp:838-856`;
  `open_floor_r = 0`, so no taper — T21).
* **Per-death forensics block** — nearest primitive, sd before/after, every component, pit-frame
  before/after, distance from the Errington mouth node, AGL, and the SDF gradient.
* **Targeted per-tick trace** (`--tracei=<drone> --tk0 --tk1 --trace=<file>`) with mode, `s_est`, net
  sd + owning primitive, cross-track/vertical offset from the spine, flown γ and φ, the *commanded*
  bank and γ from a shadow `maverick_step`, V, `contains`, the wall-guard latch, pit-frame `(ax,rad)`,
  each component sd, AGL and the reconstructed DIVE_IN aim elevation.
* **Per-drone mode census** (PATROL/TRANSIT/DIVE_IN/RUN/CLIMB_OUT tick occupancy + surface crashes
  per callsign) — the instrument that caught the `approach_alt` caveat in §5.
* **Counterfactual knobs**, all probe-local: `--dive_lookahead --approach_alt --lookahead
  --centerline_gain --track_gain --gamma_lookahead --line_tol --run_cap_deg --dive_cap_deg
  --bank_cap_deg --k_az --bank_p --tunnel_speed_max --wall_margin`, plus a probe-local
  `drone/maverick.h` copy (found first on `-I.`) carrying two experimental levers,
  `g_dive_sink_m` and `g_guard_tau_s`, both **0 by default and bit-identical to the repo header at 0**
  — proven by `fleet.exe` (repo header, no levers) reproducing the `approach_alt=350` arm exactly.
* `--pit_rim=<m>` mutates the *built* net's `pit.bowl_r` — a geometry counterfactual with no repo edit.

---

## 3. The canonical fatal run — BOREAL, tick 266067 → 266241

`fleet2.exe --min=45 --tracei=7 --tk0=265400 --tk1=266250 --trace=tr_boreal.txt`.
`lat` = cross-track from the spine, `vert` = height above the centreline, `cbk/cgm` = *commanded*
bank/γ, `pit_ax/pit_rad` = pit-frame depth/radius (floor at 220, rim at 143), `tube` = distance to
the bore ellipse, `wg` = wall-guard latch.

```
tick    mode     s_est   sd[owner]     lat   vert    gam    phi   cbk    cgm     V   in wg pit_ax pit_rad  tube
265448  DIVE_IN    0.0  684.2[pit]   +11.4 +839.6 -52.85 +17.24 +65.00 -55.00 141.3  0  0  -690.2  560.3  950.1   <- bank cmd SATURATES, 5.5 s out
265652  DIVE_IN    0.0  467.9[pit]   -17.5 +616.0 -65.52 +20.27 -65.00 -55.00 154.3  0  0  -473.9  437.3  705.7   <- gamma cmd pinned at the 55 deg cap throughout
266015  DIVE_IN    0.0   62.3[pit]   +41.3 +200.1 -54.65 -30.69 -65.00 -55.00 172.8  0  0   -68.3  166.2  230.9
266063  DIVE_IN    0.0    4.3[trench]+51.9 +143.1 -54.54 -19.42 -65.00 -55.00 175.2  0  0   -11.9  132.3  169.7   <- AT THE LIP: 132 m off axis (rim 143), 52 m off spine
266071  RUN        0.0  -85.1[trench]+53.7 +133.5 -54.50 -16.78 -65.00 -44.00 175.6  1  0    -2.4  127.0  159.8   <- contains() via the TRENCH; RUN commands -44 deg (SATURATED)
266119  RUN        0.0  -51.2[trench]+64.6  +75.1 -57.13  +1.99 -65.00 -44.00 178.0  1  0    55.5   99.7  100.0   <- still commanding full dive; 165 m of floor left
266175  RUN        0.0  -57.1[pit]   +77.0   +1.4 -67.18 +14.84 -65.00 -25.88 180.4  1  0   129.1   85.9   35.1   <- flown gamma -67.2, PAST both the 55 and 44 deg caps
266191  RUN        0.0  -55.2[pit]   +82.3  -21.0 -68.69 +10.90 -65.00 -20.09 180.8  1  0   151.5   87.8   19.2   <- gamma peak; pull-out begins (69 m of floor left)
266225  RUN        8.9  -21.3[pit]   +99.9  -64.3 -62.89  -3.77 -65.00  -8.90 181.5  1  1   198.7  101.6   20.0   <- WALL GUARD LATCHES: 0.133 s to impact
266241  RUN       20.8   -0.3[pit]  +111.5  -80.2 -57.54 -11.81 -65.00  -4.78 181.6  1  1   219.7  112.2   40.5   <- DEAD on the pit floor (220.0), 112 m off axis
```

**The surviving BOREAL run, same pilot, same dials** (tick 167451, `tr_boreal_ok.txt`) — the
discriminator is stark:

```
167440  DIVE_IN    0.0   12.5[pit]   +14.6 +149.1 -54.51 -17.37 -20.38 -55.00 172.7  0  0   -18.5   93.0  145.7   <- lip at 93 m off axis, bank cmd NOT saturated
167540  RUN        0.0 -123.5[pit]   +14.7  +33.5 -46.74  -6.02 +15.37 -34.04 177.2  1  0    96.5   14.9   16.0   <- converging on the axis
167600  RUN       67.9  -75.4[tube]   +9.1   +5.0 -32.10  +5.82 +14.23 -27.09 178.5  1  0   153.5   66.2  -75.4   <- inside the bore, clean
```

| at the pit lip | FATAL run | SURVIVING run | `dive_lookahead=250` run |
|---|---|---|---|
| radius off the pit axis | **132.3 m** (rim 143) | 93.0 m | **88.9 m** |
| cross-track off the spine | **+51.9 m** | +14.6 m | **−18.0 m** |
| commanded bank | **−65.00° (SATURATED)** | −20.38° | **+19.2°** |
| flown γ | −54.5° | −54.5° | −54.6° |
| V | 175.2 | 172.7 | 172.5 |
| subsequent `pit_rad` | 132 → 86 → **112 (diverges)** | 93 → **14.9 (converges)** | 89 → **24.8 (converges)** |

Speed and dive angle are **not** the discriminator (identical to 0.5 m/s and 0.1°). **Lateral entry
state is** — where in the crater the plunge arrives, and whether the roll channel is already clipped.

---

## 4. Answers

### (a) What surface is struck?

The **flat floor of the Errington entry pit** — the `floor_sd = ax - bowl_depth` half-plane of
`bowl_sd(p, net.pit)` (`world/tunnel_net.cpp:182-273`), built at `world/tunnel_net.cpp:838-859`:

```
net.pit.axis        = kTunnelMouthErrington
net.pit.surface_r   = terr[0]                                     (local terrain radius at the mouth)
net.pit.bowl_r      = errington_pit_rim(110) = 1.3 * 110 = 143.0 m
net.pit.bowl_depth  = mouth_sink_m + tube_height_m = 130 + 90     = 220.0 m
net.pit.open_floor_r= 0.0                                          (T21: cylinder, no cone taper)
```

Strike coordinates `[-8000.4, -1125.2, 12489.3]`, 145.0 m from the Errington mouth node
`[-8006.1, -1033.1, 12601.3]`; pit-frame `ax = 219.7 → 221.0` across the fatal tick (floor at 220.0),
`rad = 112.2 → 112.9` (rim 143.0). Component sds at the same point: **pit 1.0**, tube +40.5,
trench +89.6, bowl +11 676, throat +11 829. The gradient is `[0.535, 0.069, −0.842] = −pit.axis`,
hence `n_up = −1.000`.

So: **the crater floor annulus**, 112 m from the pit axis — 31 m inside the rim, and 40 m *outside*
the bore-mouth ellipse (semi-axes 110 × 90). The aircraft misses the bore opening and pancakes onto
the rock shelf beside it. There is no overhang, no terrain shell over the sunken bore, and no
upper boundary involved.

### (b) Why does the dive line pass through it?

It is not the DIVE_IN aim — that aim is **not an actuator** for the whole event:

* `cgm` (commanded γ) is pinned at **−55.00° = `dive_gamma_cap`** for the entire DIVE_IN, from 5.5 s
  out to the lip. `cbk` is pinned at **±65.00° = `bank_cap`** from tick 265448 to the lip.
  **Both channels are clipped**, so DIVE_IN is effectively open loop.
* **Measured proof:** shifting the DIVE_IN aim point vertically by ±40 m (`--dive_sink=±40`, the
  "sink the dive aim" experiment) produces a **byte-identical** 60-minute run — same 6 deaths, same
  coordinates, same `t_in`. A saturated command cannot see an aim perturbation.
* The knob that *does* bite is `dive_lookahead_m`, because it moves the aim point **along the bore**,
  i.e. it changes the *bearing*, and the bank command does periodically desaturate. See §5.

What actually flies the plane into the floor is the **DIVE_IN → RUN handover**:

1. `contains()` fires at grade, ~220 m above the crater floor, and — per the trace — it fires on the
   **trench** Bowl (`sd = -85.1[trench]`, pit only −16), i.e. the up-tangent approach cut admits the
   plane into the net *before* it is over the pit throat.
2. DIVE_IN immediately transitions to RUN (`maverick.h:824`), with `s_est` pinned at 0 — the reference
   station is the mouth node, **at the bottom of the crater**.
3. `bore_track`'s vertical channel is `clamp(slope_ff·γ_path + track_gain·alt_err, ±run_gamma_cap)`.
   With `alt_err ≈ −220 m` and `track_gain = 0.0045 rad/m`, the residual alone is −0.99 rad = −56.7°;
   plus the path's own −24.7° it saturates at **−44°** and stays there for 0.5 s. The autopilot is
   commanding a further full-cap dive at a plane already at −54.5° with 220 m of rock beneath it.
4. Flown γ consequently *deepens* to **−68.7°** — past both the 55° dive cap and the 44° run cap.
5. In parallel the roll channel stays clipped at −65° and cross-track grows monotonically
   **+52 → +111 m** (the same saturated-pure-pursuit geometry Rung 1 cured in the bore, here
   compressed into 1.4 s and 220 m of vertical room, where a 500 m lookahead cannot help because the
   whole event is shorter than the lookahead).
6. The pull-out begins at tick 266191 (t + 1.07 s), 69 m above the floor. Measured γ recovery is
   **30.8 °/s** over the last 0.35 s (peak 40 °/s over the last 0.13 s) — implying
   `n ≈ V·γ̇/g + cos γ ≈ 10 g`, i.e. it is already pulling near the airframe limit. To reach level
   from −57.5° needs **1.4–1.9 s**. It has 0.35 s.

**Why BOREAL?** Not speed (175.2 vs GULCH 173.6 / STOPE 173.3 — all survive) and not the trait
`daredevil 0.55` (GULCH is also 0.55). BOREAL's transit geometry delivers it to the lip **52 m off the
spine at 132 m pit radius with the bank command already clipped**; every survivor arrives inside
~20 m of the spine at <100 m radius with headroom in the roll channel. The offset is *generated
during* the saturated plunge, not admitted at the arm gate — confirmed by `--line_tol=100` (tightening
`transit_line_tol_m` 300 → 100) being **byte-identical to baseline**, and `--line_tol=40` starving the
tunnel to **zero entries**.

**Deterministic?** Yes — 19/19 identical, across two independently built binaries and three horizons.

### (c) Entry line, or guard?

**The entry line. The guard provably cannot help — three independent numbers:**

1. **It fires 0.133 s before impact.** Latch at tick 266225, `sd = −21.3`, closing on the floor at
   `Δsd/Δt = 1.34 m/tick × 120 = 161 m/s`. Arrest requires 1.4–1.9 s. Shortfall factor **11–14×**.
2. **A projected-contact latch has no admissible arming point.** At the moment the plane crosses into
   the net it is 220 m above the floor descending at `V·sin 54.5° = 143 m/s` — projected contact
   **1.55 s**. A `2.5 s` time-to-wall latch would therefore have to fire *before the aircraft is in the
   net at all*, where `sd > 0` and there is no wall to project onto. Even a latch at its own arrest
   requirement (1.4 s) leaves 0.15 s of margin against a 1.55 s budget, with 111 m of lateral
   divergence still to fix. **The aircraft is already inside its own arrest envelope when it enters the
   net.** This is why the prototype was not built: no threshold exists that both arms inside the net
   and leaves usable time.
3. **The guard's actuator is the wrong channel anyway.** `update_wall_guard` latching only doubles
   `centerline_gain` in `bore_track`'s **lateral** pull (`maverick.h:534`), and the comment there is
   explicit that the vertical channel is deliberately left alone ("doubling its gain would re-invite
   the PIO"). The fatal axis is vertical, and the lateral command it does touch was already clipped at
   the 65° cap for 1.4 s.

---

## 5. Discriminating experiments (60 min unless noted; probe-local overrides, no repo edit)

| # | arm | entries | in-net deaths | pit-floor class | lateral class | note |
|---|---|---|---|---|---|---|
| 0 | **baseline** (repo dials) | 29 | **6 (21 %)** | **4** | 1 | 150 min: 78 / **25 (32 %)**, pit class **19** |
| 1 | `dive_lookahead 150→250` | 28 | **2 (7 %)** | **0** | 2 | 90 min 3/42; **150 min 6/74 (8 %)**, pit class **0** |
| 2 | `dive_lookahead 150→350` | 28 | 2 (7 %) | **0** | 2 | same class eliminated |
| 3 | `approach_alt 500→350` | 34 | **0 (0 %)** | **0** | 0 | **150 min 0/89**; caveat below |
| 4 | `approach_alt 500→300` | 34 | **0 (0 %)** | 0 | 0 | same as #3 |
| 5 | `approach_alt 500→400` | 35 | 6 (17 %) | **0** | 5 | cliff: 400 already regresses to lateral |
| 6 | `approach_alt 500→450` | 29 | 5 (17 %) | 0 | 4 | |
| 7 | `approach_alt 500→650` | 27 | 3 (11 %) | 0 | 3 (Murray) | worse — moves failure to the Murray end |
| 8 | **`dive_sink = +40 m`** (aim deeper) | 29 | 6 (21 %) | 4 | 1 | **byte-identical to baseline** |
| 9 | **`dive_sink = −40 m`** (aim shallower) | 29 | 6 (21 %) | 4 | 1 | **byte-identical to baseline** |
| 10 | `track_gain 0.0045→0.0015` | 29 | 5 (17 %) | **4** | 0 | residual gain is not the sole lever |
| 11 | `run_gamma_cap 44→26°` | 38 | **38 (100 %)** | 1 | — | destroys the exit climb; total loss |
| 12 | `dive_gamma_cap 55→40°` | 17 | **17 (100 %)** | — | — | **0 Errington entries** — approach dies (matches the prior doc's 26° arm) |
| 13 | `transit_line_tol 300→100 m` | 29 | 6 (21 %) | 4 | 1 | byte-identical to baseline — the arm gate is not binding |
| 14 | `transit_line_tol 300→40 m` | **0** | 0 | — | — | starves the tunnel entirely |
| 15 | **`pit_rim 143→200 m`** (wider crater) | 29 | 6 (21 %) | **4** | 1 | **pit deaths byte-identical** — geometry exonerated |
| 16 | `approach_alt 350` + `dive_lookahead 250` | 34 | 0 (0 %) | 0 | 0 | identical to #3; #3 dominates |

Three of these are load-bearing *negative* results:

* **#8/#9 (dive_sink)** — the requested "sink the dive aim" experiment. Both signs, ±40 m, change
  **nothing at all** (identical event lists). This is the direct measurement that the DIVE_IN aim is
  saturated out of the loop, and it rules out entry-line *shaping* as a fix while confirming the
  mechanism in (b).
* **#15 (pit_rim)** — widening the crater 143 → 200 m leaves the pit deaths **bit-identical**, because
  the strike is at `rad = 112 m`, well inside even the current rim. The failure is a *vertical arrest*
  problem, not a *lateral clearance* problem. **The tunnel geometry is not the bug** (again).
* **#13 (line_tol)** — the fatal 52 m spine offset is generated *inside* the saturated plunge, not
  admitted by the arm gate.

### The `approach_alt = 350` caveat (the per-drone census earned this)

`approach_alt=350` scores 0/89 over 150 minutes and halves surface crashes (77 → 39) — but the mode
census shows **BOREAL stops flying the tunnel entirely**:

| callsign | baseline DIVE_IN / RUN ticks | `approach_alt=350` | `dive_lookahead=250` |
|---|---|---|---|
| **BOREAL** | 4 774 / 9 017 | **0 / 0** (336 960 ticks livelocked in TRANSIT) | **3 797 / 41 608** |
| CANARY | 1 105 / 0 | 3 779 / 51 386 | 1 115 / 0 |
| SLAGHEAP | 14 030 / 0 | 3 066 / 43 147 | 14 195 / 0 |

`approach_alt=350` buys its zero partly by removing the failing pilot from the runner pool (and
admitting two pilots who previously never completed a run). Fleet throughput does rise (78 → 89
entries), so it is not a net loss — but as a *fix for this defect* it is a substitution, not a repair,
and it sits on a cliff (400 already regresses).

`dive_lookahead = 250` is the honest repair: **BOREAL keeps running and now survives** (RUN occupancy
9 017 → 41 608 ticks, 3 Errington entries, 0 deaths).

---

## 6. RANKED minimal fix direction

**Rung 1 — config only, one dial: `config/scenario.toml [maverick] dive_lookahead_m 150 → 250.`**

The DIVE_IN aim point must lead far enough down-bore that the *bearing* solution stops clipping the
roll cap before the lip; at 150 m the aim sits inside the plunge's own turn geometry and the bank
command saturates 5.5 s out and never recovers. Measured at the lip, this moves BOREAL from
132 m pit radius / +52 m cross-track / −65° saturated bank to **88.9 m / −18.0 m / +19.2° unsaturated**,
and its pit radius then *converges* (89 → 24.8) instead of diverging.

| | baseline | `dive_lookahead = 250` |
|---|---|---|
| in-net deaths, 60 min | 6 / 29 (21 %) | **2 / 28 (7 %)** |
| in-net deaths, 90 min | — | 3 / 42 (7 %) |
| in-net deaths, 150 min | 25 / 78 (32 %) | **6 / 74 (8 %)** |
| **pit-floor class, 150 min** | **19** | **0** |
| BOREAL RUN ticks, 60 min | 9 017 | 41 608 |

`350` measures the same (2/28, class eliminated), so 250 is not a knife edge — the useful range is
open above 250. Everything left at 8 % is the **separate lateral bore-wall class** (STOPE, `n_lat`
0.99, 425–520 m in, `tube_sd < 0`) — the Rung-1 residual, not this defect.

**Rung 2 — logic edit, small and structural: don't let RUN out-dive the bore it is entering.**
The proximate command is `bore_track`'s `track_gain · alt_err` saturating the run γ cap *downward*
while the plane is inside the mouth pit. The residual term is measuring against a centreline station
at the crater floor, which is a meaningless altitude reference until the plane is actually in the
bore. The honest shape: while `s_ref` is still at (or extended before) the mouth node — i.e. the plane
is in the pit, not the tube — bound `cmd.target_gamma` below by the **path's own slope**
(`slope_ff·γ_path`, −24.7°) rather than by `run_gamma_cap` (−44°). That is a one-sided clamp in the
existing expression, it is inert everywhere else on the route, and it removes the 0.5 s of commanded
full-cap dive that turns a −54.5° arrival into a −68.7° one. **Not measured** — it needs a repo edit
and belongs behind a knob-off arm.

**Rung 3 — do NOT build the projected-contact guard.** §4(c): at the moment of net entry the projected
floor contact is 1.55 s and the arrest requirement is 1.4–1.9 s at ~10 g. No latch threshold both
arms inside the net and leaves usable time, and the guard's only actuator is the lateral gain while
the fatal axis is vertical. Fixing the guard would be fixing the wrong end of the chain.

**Rung 4 — do NOT touch the pit geometry.** `--pit_rim 143 → 200` is bit-identical (#15). The strike
is 31 m inside the existing rim.

**Rung 5 — certificate hole.** No certificate flies the **DIVE_IN → RUN handover at the sunken
Errington mouth**. The whole defect lives in the 174 ticks after `contains()` first goes true, where
`s_est = 0` makes the altitude reference the crater floor. A leg that drops a maverick at the pit lip
at the *measured* fatal entry state (132 m pit radius, 52 m cross-track, γ = −54.5°, V = 175, bank
command already at the cap) and asserts it reaches `tube_sd < 0` would have caught this. `fleet2.cpp`
+ its forensics block is a working template; `sd_parts()` is the piece worth porting — it is what
turned "`n_up = −1.00`, a ceiling" into "`pit`, `ax = 219.7` of 220.0, the crater floor".

---

## 7. Files

| file | what |
|---|---|
| `fleet2.cpp` | extended fleet arm (SDF decomposition, pit-frame forensics, targeted trace, mode census, counterfactual knobs) |
| `drone/maverick.h` | probe-local copy, found first on `-I.`; adds `g_dive_sink_m` / `g_guard_tau_s`, both no-ops at 0 |
| `f2_base60.txt`, `long_base.txt` | baseline 60 / 150 min with forensics |
| `tr_boreal.txt` | the fatal run, per tick (851 ticks) |
| `tr_boreal_ok.txt` | the surviving BOREAL run, same dials |
| `tr_boreal_dl250.txt` | the same pilot under `dive_lookahead=250` |
| `arm_dl250/dl350/aa300/aa350/aa400/aa450/aa650/ds40/dsm40/tg15/dc40/rc26/pr200/lt40/lt100/combo.txt` | the counterfactual arms |
| `long_aa350.txt`, `long_dl250.txt`, `dl250_90.txt` | 90 / 150-minute confirmations |
| `cen_base.txt`, `cen_aa350.txt`, `cen_dl250.txt`, `cen_dl250_b.txt` | per-drone mode census (+ the determinism diff) |
| `xcheck_aa350.txt` | independent-binary cross-check (`fleet.exe`, repo header only) |

Build (foreground, `-O0` exactly):

```
g++ -std=c++20 -O0 -g -I. -ID:/flight_sim2/seads-recon \
  -ID:/flight_sim2/seads-recon/build/_deps/glm-src \
  -ID:/flight_sim2/seads-recon/build/_deps/tomlplusplus-src/include \
  -o fleet2.exe fleet2.cpp \
  D:/flight_sim2/seads-recon/config/load_aircraft.cpp \
  D:/flight_sim2/seads-recon/config/load_game.cpp \
  D:/flight_sim2/seads-recon/config/load_scenario.cpp \
  D:/flight_sim2/seads-recon/config/load_world.cpp \
  D:/flight_sim2/seads-recon/sim/step.cpp \
  D:/flight_sim2/seads-recon/world/tunnel_net.cpp \
  D:/flight_sim2/seads-recon/world/heightfield.cpp \
  D:/flight_sim2/seads-recon/world/buildings.cpp
```
