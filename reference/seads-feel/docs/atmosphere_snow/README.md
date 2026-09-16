# ATMOSPHERE AS-1..AS-3 — the evidence shots

Lane `sandbox/atmosphere-snow`. Spec: `docs/SESSION_HANDOFF_20260912_atmosphere_snow.md` §3.

The PNGs beside this file are **gitignored** (about 3 MB each) but live on disk in
this directory. Every one is reproducible from the command under its heading, run
from the worktree root against `build-play/seads.exe` (RelWithDebInfo).

## The rig

Three smoke-only hooks. All are inert in live play — they are read only when
`smoke_frames > 0`, the same guard `SEADS_SMOKE_GEAR` / `SEADS_SMOKE_FIRE` use.

- `SEADS_SNOW_FORCE=<0..1>` — **new.** Forces `FrameInfo::precip_intensity`
  *after* the air gate, so a shot can show a named intensity (a flurry at 0.3, a
  squall at 1.0) without waiting for the storm budget and the eye's ground track
  to line up. It also prints one line per frame:

      [SNOW_ROCK] full_sd=<m> roofed_sd=<m> (<0 = inside; the GATE reads the
      ROOFED one) forced_intensity=<x>

  The line prints BOTH signed distances at the FINAL eye (after every TUNCAM
  override): `full_sd` is `world::TunnelNet::signed_distance`, `roofed_sd` is the
  new `roofed_signed_distance` that the gate actually reads. A --smoke run is not
  bit-reproducible frame for frame, so a dark screenshot alone cannot prove "zero
  flakes"; these numbers can, and the gap between them IS the open-pit fix.
- `SEADS_TUNCAM_PUMP="2,140,12"` — **already existed** (S-pumpcube). Puts the eye
  in the sealed DEEP stope chamber. Used unchanged.
- `SEADS_TUNCAM_MOUTH="150,0,5,errington"` — **already existed** (T6a). At 150 m
  the eye sits at `eye_sd = +6.4 m`, i.e. inside the mouth band with BOTH
  lattices straddling the rock, which is the state that exercises the per-flake
  gate. (`SEADS_TUNCAM_BORE` gained an optional third token `out` this rung, to
  face back out of the bore; the mouth cam framed the evidence better, so the
  shipped shot uses the mouth cam.)

`SEADS_NO_PRECIP=1` still bypasses the whole pass, unchanged.

## The shots

| file | command |
|---|---|
| `a_flurry_day.png` | `SEADS_SNOW_FORCE=0.3 ./build-play/seads.exe --smoke 90 a_flurry_day.png 0` |
| `b_squall_day.png` | `SEADS_SNOW_FORCE=1.0 ./build-play/seads.exe --smoke 90 b_squall_day.png 0` |
| `c_stope_force1.png` | `SEADS_SNOW_FORCE=1.0 SEADS_TUNCAM_PUMP=2,140,12 ./build-play/seads.exe --smoke 90 c_stope_force1.png 0` |
| `d_mouth_force1.png` | `SEADS_SNOW_FORCE=1.0 SEADS_TUNCAM_MOUTH=150,0,5,errington ./build-play/seads.exe --smoke 90 d_mouth_force1.png 0` |
| `j_openpit_murray.png` | `SEADS_SNOW_FORCE=1.0 SEADS_TUNCAM_MOUTH=80,0,5,murray ./build-play/seads.exe --smoke 90 j_openpit_murray.png 0` |
| `f_lowalt_squall_day.png` | `SEADS_SPAWN_ALT=300 SEADS_SNOW_FORCE=1.0 ./build-play/seads.exe --smoke 60 f_lowalt_squall_day.png 0` |
| `g_lowalt_squall_night.png` | same, trailing arg `150` |
| `h_lowalt_flurry_day.png` | same as `f`, with `SEADS_SNOW_FORCE=0.3` |
| `i_floor_day.png` / `i_floor_night.png` | same as `f`/`g`, with `SEADS_SNOW_FORCE=0.10` -- what a `snow_floor = 0.10` sky looks like |

Note: `TakeScreenshot` writes to the process CWD and strips any directory from
the path, so the files are moved into this directory after the run.

## What they show

- **(c) the stope, forced to 1.0: ZERO flakes.** `full_sd = roofed_sd =
  -1802.80 m` (the arena is roofed, so the roofed SDF bites), the whole-box
  decision is `AllInside`, and the measured precip pass in that frame is
  **0.007 ms** -- the pass is skipped outright, not drawn and hidden. The white
  lens shapes are the chamber lamps, not snow.
- **(j) the open Murray pit: SNOW FALLS.** `full_sd = -157.91 m` -- the full SDF
  says "inside the net", so the first cut of AS-1 deleted every flake here, a
  regression against main. `roofed_sd = +86.56 m`: there is no rock overhead,
  this is a hole in daylight, and the shot shows the snow falling into it. That
  gap between the two numbers is red-team P1-1, closed.
- **(d) the Errington mouth**: `full_sd = +6.43`, `roofed_sd = +12.79`, both
  lattices in the mouth band with the per-flake gate live.
- **(f)/(g)** are the look shots that matter for immersion: 299 m over the winter
  forest, big soft veil flakes behind small near ones. The snow is legible over
  BOTH the dark sky and the bright snow ground in day and night cells -- softer
  over white ground, but it does **not** vanish, so `rim_dark` ships at 1.0
  (OFF). The dial is there at ~0.85 if Chad wants more bite.
- **(a)/(h) at 0.3**: the density law -- a flurry is a few soft flakes, not the
  same field dimmed.
- **(i) the floor-only sky at 0.10: NOT clearly visible.** Over the dark sky it
  is a handful of faint specks; over the bright snow ground it is invisible. So
  `snow_floor` SHIPS AT 0.0 -- at the level that would have been needed to matter
  it stops being a dusting, and at 0.10 it is a cost with no picture. The flurry's
  own budget (AS-2) is what carries "more snow" now.

## Ledger for Chad (numbers, not asks)

- **Veil sway speed.** `veil_sway_m = 0.60` over the veil's 13.3 s fall cycle
  (8 m cell / 0.60 m/s) is a peak lateral speed of **0.28 m/s**, against a
  **0.60 m/s** fall. Its time-average is exactly zero and its direction is hashed
  per cell, so nothing translates -- but that is the number a strict reading of
  NO-WIND would want to see. `sway_m = 0.25` near is 0.31 m/s peak over a 5 s
  cycle. Both are one config line from 0.
- **The rock cache's arming frame.** The first frame that enters a mouth band
  pays the full per-cell SDF sweep (~14 ms, one frame) before the cache is warm;
  steady state is 0.76 ms. Entering and leaving repeatedly does NOT re-arm (the
  cache is keyed by world cell, so the entries survive).
- **The cache invalidates on a pointer compare** (`rock_cache_ctx != rock_ctx`).
  A rebuilt TunnelNet that landed at the same address would keep stale entries.
  Nothing in the app rebuilds the net mid-session today, so this is a note, not
  a bug.

## AS-4 -- EVENT DURATION (Chad's fly of aa9d78142)

> "there is not enough durition of the weather events, I didnt see any snowing
> in the tunnel so pass for that."

AS-1 passed his eye. The duration note was DIAGNOSED before any dial moved --
`test_snowfall.cpp` prints the table below on every run, so the claim is a
measurement, not a story. Three eyes on twelve in-dome orbits each (both domes x
three radii x two bearings), 20 000 s per orbit, 5 s steps:

| eye | BEFORE median | BEFORE gap | AFTER median | AFTER gap |
|---|---|---|---|---|
| stationary (tempo alone) | 1083 s (18.0 min) | 78 s | 802 s (13.4 min) | 108 s |
| sled 20 m/s | 508 s (8.5 min) | 67 s | 575 s (9.6 min) | 113 s |
| plane 142 m/s | **36 s (0.6 min)** | 13 s | **312 s (5.2 min)** | 28 s |

Uniform in-dome light-snow share 0.57 -> 0.645. Squall-to-flurry handover
0.309 -> 0.726. Flurry/squall lattice overlap 0.200 at the new cell size
(chance is ~0.30).

**The diagnosis.** The TEMPO was never the problem: a parked eye already sat in
18-minute episodes. The PLANE was. At 142 m/s it crossed a 6 deg flurry cell in
about 22 s, so its median episode was 36 s with 13 s gaps -- snowed on 87% of the
time and still starved, because the field BLINKED. That is what "not enough
duration" feels like, and it is SPATIAL, so the dials that fix it are the cell
scale, not the periods.

**The ceiling nobody can dial around.** A dome is 0.40 rad, about 12 km across; a
plane crosses it in 85 s. On any closed path inside it the median episode is
roughly (share x lap time), so a multi-minute median at plane speed REQUIRES a
high in-dome share. Duration and "localized" are in direct tension up there. The
shipped values sit at the knee, not past it -- gaps survive at every speed
(28 s plane, 113 s sled, 108 s parked).

**Two honest warnings about the dials.**
- `flurry_period_scale` is NOT monotonic and 1.4 is a RIDGE, not a plateau:
  1.3/1.4/1.5 measure 299/312/146 s for the plane. It IS a plateau in phase
  (2.2..3.1 all hold). Everything in the 1.3-1.5 block keeps the sled above
  7 min, the share near 0.60 and the handover above 0.5; only the plane's median
  swings. A first pass measured this on six orbits instead of twelve and read the
  slope BACKWARDS (it looked like slowing the fronts always hurt); the ensemble
  was widened and the claim corrected.
- The squall-to-flurry handover was fixed with `flurry_phase_off` ALONE (1.7 ->
  2.6), no state and no hysteresis. The two budgets share their three periods, so
  their ups and downs correlate however the cells are placed; the phase is the
  only pure lever that separates them.

## AS-5 -- SNOW-SQUALL ON ITS OWN DIALS (Chad's fly of 6aa0d79fb)

Chad: "I just flew it and its still no good, I see very few weather events and they
barely last 15s? What is happenning here?" The sentinel's read: the events he sees are
the SQUALL, which was the haze's `weather_cell` on the signed `[weather_cell]` bubble
dials (~3 km cells, lit only while the haze budget clears -0.05). Ruling, via the
sentinel: **"yes please do so"** -- give the snow squall its own cell size and gate.

Built: `render::snowfall_squall`, a second instance of `weather_cell` on private params
(`[precip] squall_*`). `squall_own_dials = 0` is `weather_cell` bit-identically. The haze
path is untouched and `[weather]`/`[weather_cell]` parse identical to main.

Measured (`test_snowfall.cpp`; squall = squall term > 0.5; in-dome orbits, 48 per draw):

| | plane median squall | squalls / 10 min | median gap | heavy (>=0.8) in-dome share |
|---|---|---|---|---|
| today (`weather_cell`) | 26 s | 2.7 | 24 s | 0.067 |
| AS-5 shipped (8/15 deg, 3 cells, gate -0.65) | 86 s | 2.9 | ~55 s | 0.315 |

Noise floor: four independent draws agree to within 3 s at the pick. One-dial neighbours
(`[.as5ridge]`): inner 7/9 deg = 54/105 s, outer 13/17 = 57/105 s, count 2/4 = 58/130 s,
gate +-0.1 = 81/86 s, thresh_hi +-0.1 = 89/79 s, period 1.2 = 92 s. No ridge.

**The trade (for Chad's eye, not decided by a number):** several 1-2 min squalls per
10 min can only fit if heavy snow covers about a third of in-dome time. The gate ladder
(`[.as5gate]`):

| squall_gate_lo | plane median | per 10 min | uniform snow share | heavy share | taper |
|---|---|---|---|---|---|
| -0.05 | 79 s | 1.8 | 0.671 | 0.147 | 0.684 |
| -0.25 | 79 s | 2.2 | 0.712 | 0.227 | 0.668 |
| -0.35 | 106 s | 2.4 | 0.730 | 0.266 | 0.690 |
| -0.45 | 117 s | 2.5 | 0.748 | 0.304 | 0.616 |
| -0.55 | 86 s | 2.7 | 0.766 | 0.338 | 0.553 |
| **-0.65 (ships)** | **86 s** | **2.9** | 0.786 | 0.368 | 0.561 |

(-0.35/-0.45 is a bump, so it is not a candidate. The heavy-share column here uses 5 s
sampling over 4500 s; the gated test's 0.315 uses a different time scan.) -0.65 ships
because the ruling said "lit more of the time, more of them per flight". -0.25 is the
gentler plateau, one config line away. Two AS-4 bars moved in the open because of this:
uniform snow share < 0.75 is now < 0.85, and the squall-to-flurry taper > 0.6 is now > 0.5.
`squall_phase_off` is the pure lever for the taper and was deliberately not dialled.
The snow squall no longer rides the haze's cells, so heavy snow can fall under lighter
haze than before.

Probes: `seads_tests "[.as5sweep]"`, `"[.as5ridge]"`, `"[.as5gate]"`, `"[.noise]"`.
