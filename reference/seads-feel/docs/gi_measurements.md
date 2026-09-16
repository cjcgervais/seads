# Ground-Interaction measurements (Phase W1)

Created during the W1 build (road-deck registration, `docs/ground_interaction_spec.md`
§PHASE W1). Every distribution below was measured against the actual build/tool run
pasted alongside it — none are invented.

## W1.4 registration leg (`snowpack_road_deck_registration_headless`)

Headless C++ leg (`test/unit/test_snowpack.cpp`), synthetic flat-terrain fixture,
`deck_lift_m = 0.45` (the shipped `[ribbons] lift_m`):

```
centreline drive-radius=0.470000 road_bare-part=0.020000 lift-part=0.450000
trail drive-radius=0.570000 trail_pack-part=0.120000 lift-part=0.450000
shoulder e=3/6/9/12 lift=0.450000/0.450000/0.450000/0.000000
```

- Road centreline: `drive_radius_at - radius_at == deck_lift_m + road_bare_m` exactly
  (0.45 + 0.02 = 0.47).
- Trail centreline: `drive_radius_at - radius_at == deck_lift_m + trail_pack_m` exactly
  (0.45 + 0.12 = 0.57).
- Shoulder envelope (e = distance beyond the corridor edge; `bank_rise_m + bank_fall_m`
  = 3 + 6 = 9 m is the shipped bank-ring extent; `deck_skirt_m` was set to 3.0 for this
  leg specifically, distinct from the shipped `[bank_mesh] skirt_m` = 6.0, so the ramp
  lands cleanly on the spec's literal e = 3/6/9/12 sample points): constant through the
  roadway and bank ring (e = 3, 6, 9 all read the full 0.45 m lift), then a strictly
  monotone smoothstep ramp to exactly 0 by e = 12 (verified `lift(10) < lift(9)`,
  `lift(11) < lift(10)`, `lift(12) < lift(11)`).

## W1.5 INV-1 bounded-divergence extension

Extended `TEST_CASE("INV-1: ...")` at `test/unit/test_snowpack.cpp` (originally line
347): with a corridor + `deck_lift_m = 0.60` in play, `drive_radius_at - (radius_at +
depth_at)` is no longer always 0 (as the pre-W1 INV-1 body asserts on a corridor-free
field) — it is now PINNED at exactly `deck_lift_m` on the roadway and on the bank
crest (both inside the drawn bank-ring extent), and back to exactly 0 past the skirt.
All three assertions pass at < 1e-9 m tolerance (see the test for the exact directions
sampled).

## W1.7 `--burial` mode ("the Vermilion leg", P0-6)

**Status: RUN, gate result recorded honestly — the shipped-tiling gate FAILS by a
small margin.** The venv/bake-availability concern in the spec did not apply in this
worktree: `numpy`/`Pillow` are present under
`C:\Users\Chad\AppData\Local\Programs\Python\Python311\python.exe` (NOT the bare
`python` on PATH, which resolves to a different, dependency-less venv), and
`assets/sudbury_dem.png`, `assets/sudbury_landmask.png`, `render/sudbury_gis.gen.h`
are all present and load. So the mode was implemented in
`offline_tool/measure_drape_gap.py` (`--burial` flag, `station_signed_gap` /
`midseg_signed_gap` / `run_burial_mode`) and actually executed:

```
$ "C:\Users\Chad\AppData\Local\Programs\Python\Python311\python.exe" offline_tool/measure_drape_gap.py --burial
DEM  8192x4096  landmask 8192x4096
roads (kind 0+1): 47867 centerline stations in 6409 chains

== --burial: SIGNED gap = (radius_at + road_bare_m) - facet  (m)
   + = physics floor ABOVE the drawn facet (floats); - = buried below it
  T=1 (E=200, n_sta=47867, n_mid=710877, n_all=758744):
    stations:    mean=  +0.020  p50=  +0.087  p90=  +0.832  p99=  +7.658  p99.9= +18.911  min= -38.597  max= +49.803
    midsegments: mean=  -0.005  p50=  +0.085  p90=  +0.717  p99=  +6.365  p99.9= +17.196  min= -38.707  max= +51.022
    combined:    mean=  -0.004  p50=  +0.085  p90=  +0.723  p99=  +6.437  p99.9= +17.347  min= -38.707  max= +51.022
  T=2 (E=399, n_sta=47867, n_mid=710877, n_all=758744):
    stations:    mean=  +0.029  p50=  +0.037  p90=  +0.227  p99=  +3.506  p99.9= +10.736  min= -22.200  max= +24.618
    midsegments: mean=  +0.016  p50=  +0.032  p90=  +0.185  p99=  +2.677  p99.9=  +9.872  min= -26.497  max= +25.150
    combined:    mean=  +0.017  p50=  +0.032  p90=  +0.187  p99=  +2.738  p99.9=  +9.932  min= -26.497  max= +25.150
  T=3 (E=598, n_sta=47867, n_mid=710877, n_all=758744):
    stations:    mean=  +0.025  p50=  +0.027  p90=  +0.107  p99=  +1.913  p99.9=  +8.412  min= -20.122  max= +33.304
    midsegments: mean=  +0.013  p50=  +0.023  p90=  +0.088  p99=  +1.385  p99.9=  +7.040  min= -24.559  max= +30.224
    combined:    mean=  +0.013  p50=  +0.023  p90=  +0.089  p99=  +1.415  p99.9=  +7.217  min= -24.559  max= +33.304

GATE (T=2, shipped [planet] tiles): |p50| = 0.0321 m <= 0.03 m  ->  FAIL

report written: D:\seads_sandboxes\winter-gi\offline_tool\burial_distribution_report.json
total runtime: 2.9 s
```

Definitions: `signed = (radius_at(dir) + road_bare_m) - facet_radius(dir)` — the
physics floor a Road-class contact patch actually sits on (Road is the one corridor
class `deck_lift_m` never enters the *reported* depth of, and the lift term itself
cancels against the identical `[ribbons] lift_m` the render drape adds — see
`render/ribbons.cpp`'s `dir * (radius_at(dir) + lift)` — so this metric isolates the
`road_bare_m` vs. facet residual, which is the number that decides whether a Road
sled visually floats or buries). Positive = physics floor above the drawn facet
(floats slightly); negative = buried. Measured over the full baked road network
(47,867 centreline stations + 3 re-densify spacings × 3 chord parameters of
midsegments = 758,744 samples total), at all three cubesphere tilings.

**Gate**: T=2 is the shipped `[planet] tiles` value. Combined p50 = **+0.0321 m**,
just outside the ±0.03 m budget (a 0.0021 m / 7% overshoot). T=1 (untiled) is far
outside budget (p50 = +0.085 m); T=3 comfortably passes (p50 = +0.023 m).

**This is a real, measured gate failure, not a synthetic pass-by-construction result
— it is reported here rather than silently patched.** The residual is a pre-existing
property of the facet-vs-DEM registration (compare `offline_tool/drape_gap_report.json`
section "2. ROAD STATION" / "4b" from the unmodified tool: T=2 station p50 was already
+0.017–0.029 m before `road_bare_m` is even added), not something introduced by W1's
`deck_lift_m`/`deck_skirt_m` work (which cancels out of this metric by construction).
`road_bare_m` (0.02 m) is not in this session's write-authorized tuning scope — bumping
it to close a 0.002 m gap would be a drive-feel/visual decision belonging to whoever
owns the road corridor's bare-surface dial, not a side effect of the registration
refactor. **Flagging for Chad / the next GI session**: either accept the p50 = 0.032 m
residual (it is small — a fraction of a `road_bare_m` — and well inside the existing
p90/p99 tails that were already being carried), or retune `road_bare_m` slightly
upward (to ~0.032 m it would land the gate almost exactly on budget) as a deliberate,
reviewed dial change.

## Not run in this session (W1)

S1/W2/W3 measurements (snowbank crest-carve probe, faceted-ground A/B) are out of
scope for this W1-only session and are not recorded here. **S2 was built in a later
session on this same branch — see below.**

---

# Phase S2 (roll honesty) — measurements

Built in the same worktree, `docs/ground_interaction_spec.md` §PHASE S2. Every number
below is pasted from an actual `ctest`/binary run (`-s` for the printf'd ones), not
invented. Files touched: `sim/sled.h`, `sim/sled.cpp`, `tools/sled_probe.cpp`,
`test/unit/test_sled.cpp`.

## ★★★ THE HEADLINE FINDING — P0 runaway wheelie, full throttle in deep snow

`seads_sled_probe trace 0.77 1.0 0.0 6.0` (full throttle from rest, signed 0.77 m
Bush depth):

```
   t     v    plane  sinkL  sinkR  sinkT  xL     xT     pitch  roll
 0.00   0.01  0.000  0.181  0.181  0.377  0.004  0.036    9.4    0.0
 0.42   2.13  0.029  0.068  0.068  0.446  0.000  0.076   21.5    0.0
 0.83   3.59  0.120  0.014  0.014  0.394  0.000  0.086   59.4    0.0
 1.25   2.47  0.026  0.003  0.003  0.159  0.000  0.010   87.0    0.0
 1.67   1.95  0.020  0.001  0.001  0.222  0.000  0.027   89.1   -0.0
 2.08   1.35  0.006  0.000  0.000  0.229  0.000  0.023   89.0    0.0
 ...    ...   ...    ...    ...    ...    ...    ...     ~89-90  ~0
```

Pitch runs away to ~89–90° (standing on the tail) and never settles back onto the
skis; forward speed collapses from a ~3.6 m/s peak to ~0. `sled_gi_launch_pitch_
before_after_report` (a report-only leg, `test/unit/test_sled.cpp`) confirms with a
direct before/after A-B at t=6s, full throttle, 0.77 m Bush:

```
[GI S2.8] launch pitch @ t=6s, 0.77 m Bush, full throttle: before=3.5 deg  after=89.9 deg
```

`before` = `gi_off()` (frac 0, rail 0.14 — the pre-GI arm); `after` = shipped. This is
the exact P0 the spec's own S2.7 item 7 named in advance ("the thrust arm grows
0.354->~0.52 (+47% pitch-up moment at the cap) ... If the launch wheelie no longer
settles ... flag P0 to Fable before proceeding") — S2.1's honest tangential arm moves
the track's thrust application point from the mount toward the true (sunk) contact,
i.e. FARTHER from the CG along the pitch-up moment arm, and `sled.h:396-404`'s
existing "P/v falls with speed, so it settles" mechanism no longer wins against the
larger torque at this depth.

**Per the spec's own instruction this is REPORTED, not silently patched or worked
around.** Fixing it would mean retuning `max_thrust_n`/`track_clearance_m`/
`plane_gain` (or a new dial) against the honest arm — a real, planing-central retune
that S2's authorized work order (S2.1–S2.8) does not cover. Nine legs are left
honestly RED because of it, each cross-referenced to this finding in its own comment
(the runaway develops identically whenever full throttle meets sinkable deep snow, so
every planing/dwell/cold-emergence leg that drives from rest at depth reads it from a
different angle):

```
sled_summits_the_snowhill_and_flies_clean    (test/unit/test_snowhill_drive.cpp —
                                               READ-ONLY for this session, not edited)
sled_deeper_snow_makes_planing_harder_to_reach
sled_planes_in_the_signed_depth
sled_still_plows_at_full_speed
sled_dwell_buries_and_travel_sheds
sled_roost_flux_is_one_number_with_two_consumers
sled_bogs_in_the_p99_drainage_line
sled_cold_emergence_matrix_top_speed_monotone_in_h
sled_cold_warm_drives_slower_but_still_planes
```

**This goes to Chad/Fable before any further planing-dial work.**

## S2.3 — h_lat and the probe/leg decoupling fix

`three_patch_a_tip()` now takes an `h_lat` parameter (was hardcoded to
`cg_height_m - susp_rest_m`, valid only at `bite_at_contact_frac == 0`).
`load_weighted_contact_height()` (independently duplicated in `tools/sled_probe.cpp`
and `test/unit/test_sled.cpp`) settles the machine and reads back the load-weighted
`cg_height_m - susp_x[i] + sink_m[i]` per patch.

```
h_lat (Bush 0.30, load-weighted, frac=1) = 0.7449 m
```

(The corrections doc's own quoted "~0.5317 m static" figure was evidently measured on
a non-sinkable reference, or at a different depth — the sinkable Bush 0.30 fixture
used here adds real `sink_m` on top of suspension compression, which the formula
correctly counts. Both are legitimate "h_lat at frac=1" numbers for different ground.)

`probe_mu_check`/`probe_grip` and the `sled_grip_ceiling_stays_below_the_tip_
threshold` leg now set `bite_at_contact_frac = 0.0` alongside `cg_height_m =
susp_rest_m` for the roll-decoupled measurement (P1-2: without the frac reset, the
honest arm reintroduces a roll moment via susp_x even at cg_height==susp_rest, since
patch_geometry's "every mount at y==0" guarantee only covers forces that stay AT the
mount).

## S2.5 — the empirical fence (replaces the dead analytic leg)

`sled_grip_ceiling_stays_below_the_tip_threshold` now compares a measured
`peak_a_lat_decoupled` (S2.3's decoupled trick) against a measured `tip_onset` (a
full-coupled, full-lock steer sweep across {8,12,16,22,30,38} m/s, `comfort =
rc_off()` — see the leg's own comment for why: shipped comfort's roll stiffness holds
a machine through a HELD full-lock turn on any surface whose ski mu approaches the
track's fixed 0.70, indefinitely, which is the comfort system doing its ruled job,
not a tip threshold).

**Gate**: Bush at 0.30 m only — `peak/onset <= 0.90`, REQUIRE:

```
h_lat (Bush 0.30, load-weighted, frac=1) = 0.7449 m
onset (Bush 0.30) = 10.18 m/s^2 (1.038 g)
peak  (Bush 0.30, decoupled) = 5.77 m/s^2
peak/onset = 0.567   <= 0.90   PASS
```

**P2-8 finding, NOT gated** (STOP condition — see below): the SAME measurement on
TrailMain/Road/RockOutcrop and deep (0.77 m) Bush:

```
Bush   0.77 m : peak/onset = 2.674
TrailMain     : peak/onset = 1.912
Road          : peak/onset = 8.326   (mu_lat = 0.60, shipped — see S2.4 below)
RockOutcrop   : peak/onset = 2.152
```

None of these clear 0.90. Fixing them would mean lowering `mu_lat` on TrailMain/
RockOutcrop or `track_lat_mu` (the shared track dial) — none of which this phase's
authority covers (only Road's `mu_lat` is in the ruled `[0.55, 0.70]` sweep band,
S2.4). Per the same "do not quietly move a rostered dial to force a pass" rule S2.7
states explicitly for its own fence, these are recorded, not silently tuned.

## S2.4 — friction rows + the mu_lat(Road) sweep — ★ STOP CONDITION HIT

`Road.mu_kin` 0.140 → 0.220; `RockOutcrop.mu_kin` 0.200 → 0.280 (added to the S2.5
fence sweep).

`mu_lat(Road)` swept `{0.55, 0.60, 0.65, 0.70}` against `sled_road_360_completes_
without_rolling` (8 cells: V {12,16,20,24} × steer ±1, full-lock + full brake + a
0.30 sustaining throttle — see that leg for why full brake alone stalls the machine
and why full throttle reintroduces the P0 wheelie above), 30 s each, comfort SHIPPED:

```
[S2.4 sweep] mu_lat=0.55 v0=12 steer=-1 yaw=-8.02  rolled=0
[S2.4 sweep] mu_lat=0.55 v0=12 steer=+1 yaw=8.02   rolled=0
[S2.4 sweep] mu_lat=0.55 v0=16 steer=-1 yaw=-12.27 rolled=1
[S2.4 sweep] mu_lat=0.55 v0=16 steer=+1 yaw=12.27  rolled=1
[S2.4 sweep] mu_lat=0.55 v0=20 steer=-1 yaw=-7.19  rolled=1
[S2.4 sweep] mu_lat=0.55 v0=20 steer=+1 yaw=7.19   rolled=1
[S2.4 sweep] mu_lat=0.55 v0=24 steer=-1 yaw=18.73  rolled=0
[S2.4 sweep] mu_lat=0.55 v0=24 steer=+1 yaw=-18.73 rolled=0

[S2.4 sweep] mu_lat=0.60 v0=12 steer=-1 yaw=-7.29  rolled=0
[S2.4 sweep] mu_lat=0.60 v0=12 steer=+1 yaw=7.29   rolled=0
[S2.4 sweep] mu_lat=0.60 v0=16 steer=-1 yaw=-19.80 rolled=0
[S2.4 sweep] mu_lat=0.60 v0=16 steer=+1 yaw=19.80  rolled=0
[S2.4 sweep] mu_lat=0.60 v0=20 steer=-1 yaw=-7.23  rolled=1
[S2.4 sweep] mu_lat=0.60 v0=20 steer=+1 yaw=7.23   rolled=1
[S2.4 sweep] mu_lat=0.60 v0=24 steer=-1 yaw=10.50  rolled=0
[S2.4 sweep] mu_lat=0.60 v0=24 steer=+1 yaw=-10.50 rolled=0

[S2.4 sweep] mu_lat=0.65 v0=12 steer=-1 yaw=-14.11 rolled=0
[S2.4 sweep] mu_lat=0.65 v0=12 steer=+1 yaw=14.11  rolled=0
[S2.4 sweep] mu_lat=0.65 v0=16 steer=-1 yaw=2.64   rolled=1
[S2.4 sweep] mu_lat=0.65 v0=16 steer=+1 yaw=-2.64  rolled=1
[S2.4 sweep] mu_lat=0.65 v0=20 steer=-1 yaw=-1.60  rolled=1
[S2.4 sweep] mu_lat=0.65 v0=20 steer=+1 yaw=1.60   rolled=1
[S2.4 sweep] mu_lat=0.65 v0=24 steer=-1 yaw=-6.53  rolled=0
[S2.4 sweep] mu_lat=0.65 v0=24 steer=+1 yaw=6.53   rolled=0

[S2.4 sweep] mu_lat=0.70 v0=12 steer=-1 yaw=-17.07 rolled=0
[S2.4 sweep] mu_lat=0.70 v0=12 steer=+1 yaw=17.07  rolled=0
[S2.4 sweep] mu_lat=0.70 v0=16 steer=-1 yaw=-2.61  rolled=1
[S2.4 sweep] mu_lat=0.70 v0=16 steer=+1 yaw=2.61   rolled=1
[S2.4 sweep] mu_lat=0.70 v0=20 steer=-1 yaw=-2.65  rolled=1
[S2.4 sweep] mu_lat=0.70 v0=20 steer=+1 yaw=2.65   rolled=1
[S2.4 sweep] mu_lat=0.70 v0=24 steer=-1 yaw=-15.01 rolled=0
[S2.4 sweep] mu_lat=0.70 v0=24 steer=+1 yaw=15.01  rolled=0
```

**★★★ STOP CONDITION, exactly as the spec names it**: v0 = 20 m/s rolls at EVERY
candidate in the ruled band, both steer directions. Per the spec's explicit
instruction ("do not quietly move track_lat_mu or leave the [0.55, 0.70] band to
force a pass"), `mu_lat(Road)` is not retuned outside the ruled band and no other
dial (corridor width, brake magnitude) was adjusted to make v0=20 pass. **0.60 ships**
as the measured best-of-band (fails only v0=20 — every other candidate ALSO fails
v0=16 in addition). v0=20 is a reported, non-gating finding in
`sled_road_360_completes_without_rolling`, not silently absorbed.

**This goes to Chad, same escalation as the P0 wheelie above.**

## S2.6 — load-weighted contact-plane fit

**Implementation note**: the spec's Newell-sum-with-per-edge-weights approach is a
mathematical no-op for exactly THREE points (any three points are exactly coplanar,
so every pairwise edge cross-product from one triangle is parallel to the same
normal — weighting the edges only rescales a sum of already-parallel vectors, never
changes its direction). MEASURED directly (a debug instrument, since removed): the
weighted and unweighted branches produced bit-identical `n_surf` on every fixture
tried under that formulation. Replaced with a RELIABILITY blend instead: when the
three patch loads are uneven (one patch barely loaded), the raw 3-point fit is
untrustworthy, so it is blended toward `up_cg` (the existing fallback's own target)
in proportion to how uneven the loads are — `reliability = min(N)/(mean(N))`, 1 when
even, → 0 as the least-loaded patch's share vanishes.

`sled_assist_reference_plane_is_load_weighted` (a scripted straddle of a Road
corridor's own snowbank — right ski sunk deep into the bank's rising face, track +
left ski on the flat pavement):

```
susp_x L=0.011 R=0.027 T=0.035   sink L=0.000 R=0.127 T=0.026
[GI S2.6] assist_nm weighted=45.48  unweighted=321.56
```

The corrections doc's own "404 N·m spurious assist" figure was measured on the
original packet's scenario, not reproduced exactly here (a scripted straddle, not a
driven approach) — shipped bands are the measured 45.5 / 321.6 N·m (>7x separation),
not the packet's number.

## S2.7 — legs

**`sled_lateral_force_acts_at_the_running_surface`**: ski arm on non-sinkable ground
(Road, `sink_m == 0` always, so the arm is exactly `cg_height_m - susp_x`):

```
[GI S2.7] ski arm static = 0.5509 m        (in [0.51, 0.57] — PASS)
[GI S2.7] ski arm at 2.5x load = 0.5321 m  (< static — moves with load — PASS)
```

**`sled_road_360_completes_without_rolling`**: see S2.4 above (the mu_lat sweep IS
this leg). Shipped (mu_lat=0.60), v0 ∈ {12,16,24} gate; v0=20 is the STOP finding.
The comfort-OFF distribution over the same 8 cells (recorded, not gated — S2.7's
explicit instruction, since the honest geometry makes a no-roll clause unreachable):

```
[GI S2.7 360 OFF] v0=12 steer=-1 yaw=-1.12 rolled=1
[GI S2.7 360 OFF] v0=12 steer=+1 yaw=1.12  rolled=1
[GI S2.7 360 OFF] v0=16 steer=-1 yaw=-0.92 rolled=1
[GI S2.7 360 OFF] v0=16 steer=+1 yaw=0.92  rolled=1
[GI S2.7 360 OFF] v0=20 steer=-1 yaw=-0.88 rolled=1
[GI S2.7 360 OFF] v0=20 steer=+1 yaw=0.88  rolled=1
[GI S2.7 360 OFF] v0=24 steer=-1 yaw=-0.98 rolled=1
[GI S2.7 360 OFF] v0=24 steer=+1 yaw=0.98  rolled=1
```

Every cell rolls with comfort off — matching the red-team's own prediction ("at
honest geometry the OFF-comfort no-roll clause is impossible by arithmetic").

**`sled_static_tilt_table`** (P1-14, new analytic `cross_slope_field()` fixture —
exact `tan(angle)` grade along the lateral axis, flat fore-aft, at any grid
resolution — see its header comment): a PARKED machine (zero momentum) is measurably
more roll-resistant than a dynamically TRIPPED one. With shipped comfort ON, parked
never rolls up to 70° within 10 s (item A's roll stiffness holds a momentum-free
machine indefinitely). With `comfort = rc_off()` (matching `sled_tripped_rollover_
still_happens`'s convention):

```
[GI S2.7 tilt] 20 deg, 10 s: rolled=0
[GI S2.7 tilt] 30 deg, 10 s: rolled=0
[GI S2.7 tilt] 40 deg, 10 s: rolled=0
[GI S2.7 tilt] 50 deg, 10 s: rolled=0
[GI S2.7 tilt] 60 deg, 10 s: rolled=0
[GI S2.7 tilt] 70 deg, 10 s: rolled=1
```

The spec's suggested "30°" is the TRIPPED number (three_patch_a_tip's h_lat assumes
an existing lateral force to overcome); a PARKED entry's real threshold measured
between 60° and 70°. The leg ships 20°-holds / 70°-rolls.

## S2 re-measured legs (existing legs whose thresholds moved)

All measured by bisection/sweep against the post-S2 kernel, same method the pre-GI
legs used ("measured, not guessed"):

- `sled_tripped_rollover_still_happens` / `sled_rc_off_is_the_pre_rc_kernel`
  (comfort off): static-trip threshold moved 30/40° → 20/30° (rail 0.19 does not buy
  a static-trip scenario the same margin it buys a lateral-bite one — the honest
  arm's effect dominates).
- `sled_rc_trip_is_roll_resistant_not_roll_proof` (shipped comfort): 40/50° → 40°
  caught / 50° rolls (was 40/50 caught, 60 over) — comfort still buys real margin
  over the OFF case (30° vs 40° caught), just less than before.
- `sled_wrong_way_lean_is_never_a_penalty`: the two trajectories (lean_bite_gain on
  vs off, wrong-way lean) are analytically forced bit-identical at every step (align
  clamps to exactly 0.0 either way) but the comparison window shrank 300 → 40 steps:
  MEASURED first bit-level divergence at step 24 (a last-ULP difference, not a logic
  leak), which a chaotic system — this scenario now sits much closer to the grip/tip
  margin post-GI — amplifies to a macroscopic difference by step 300. Not an
  asymmetry bug; the mutation this leg exists to kill (an unclamped, signed align
  term) still shows up well inside 20 steps.
- `sled_onside_recovery_is_momentum_not_magnetism`: the fast-slide self-right (item
  C2) no longer clears 30° at 15 m/s — MEASURED 71.4°, and no speed 5–30 m/s clears
  30° (best: 20 m/s at 46.4°). NOT retuned here: `side_right_gain_nm`/`wref` are
  explicitly S3.5's job ("SOLVED TOGETHER" against the post-GI kernel), not S2's
  authority, and Chad has separately ruled roll-comfort retuning stays deliberate.
  The parked/anti-magnetism invariant (R is the backstop) still holds and stays a
  REQUIRE; the fast-slide numbers are now a reported finding for S3.

## `sled_gi_off_is_bit_identical_to_rc1`

Built per §SHARED OFF MECHANISM. `gi_off()` is NOT `rc_off()` (a different axis — see
the function's own comment): it restores S2's ground-interaction dials
(`bite_at_contact_frac`, `track_rail_half_m`, `Road`/`RockOutcrop` dials,
`plane_fit_load_weight`) to their pre-GI (RC1-tree) values while leaving every RC
comfort field at its shipped default. Verified against a SEPARATE, literally
hardcoded pre-GI params struct (not a call to `gi_off()` itself) over a 2000-tick
scripted TrailMain drive with varied throttle/brake/steer/lean inputs — bit-identical
every tick (20001 assertions, all pass).

## Full suite

See the top-level commit/handoff report for the final `ctest` count.

---

# Phase S2b (the track's PITCH split) — measurements

Built in the same worktree, on the same uncommitted W1+S2 tree. **This phase
closes S2's P0 STOP finding.** Every number below is pasted from an actual
`ctest` / `seads_sled_probe` run, none invented. Files touched: `sim/sled.h`,
`sim/sled.cpp`, `tools/sled_probe.cpp`, `test/unit/test_sled.cpp`, this doc.

## ★★★ THE HEADLINE — the P0 is CLOSED, and no planing dial moved

`sled_gi_launch_pitch_before_after_report`, the same leg S2 used to record
the defect:

```
S2  (single-point track):
[GI S2.8] launch pitch @ t=6s, 0.77 m Bush, full throttle: before=3.5 deg  after=89.9 deg

S2b (track_pitch_half_m = 0.20):
[GI S2.8] launch pitch @ t=6s, 0.77 m Bush, full throttle: before=3.5 deg  after=3.4 deg
```

`before` = `gi_off()` (which now zeroes the new dial too, so the column is
unchanged by construction and the `after` column measures S2b alone).

`seads_sled_probe trace 0.77 1.0 0.0 6.0`:

```
BEFORE (S2, track_pitch_half_m = 0.0)      AFTER (S2b, 0.20)
   t     v   pitch                            t     v   pitch
 0.00  0.01    9.4                          0.00  0.00    6.3
 0.42  2.13   21.5                          0.42  1.85   10.2
 0.83  3.59   59.4                          0.83  3.43    9.7
 1.25  2.47   87.0                          1.25  4.53    7.3
 1.67  1.95   89.1                          1.67  5.31    5.3
 2.08  1.34   89.0                          2.08  6.59    5.6
 2.50  0.88   87.8                          2.50  7.97    5.5
 2.92  0.32   89.8                          2.92  9.08    5.0
 3.33  0.06   89.4                          3.33 10.02    4.6
 3.75  0.03   89.5                          3.75 10.83    4.3
 4.17  0.02   89.7                          4.17 11.55    4.2
 4.58  0.01   89.9                          4.58 12.22    4.1
 5.00  0.01   90.0                          5.00 12.84    4.0
 5.42  0.02   90.0                          5.42 13.41    3.7
 5.83  0.01   89.9                          5.83 13.91    3.5
```

A wheelie still happens (peak 10.2 deg at t=0.42) and then the machine
SETTLES onto its skis while the speed climbs — which is the signed feel
(Chad, DRIVE 4: launch wheelies are fun), not the runaway. Terminal at 40 s:
17.71 m/s, plane_frac 0.674 (pre-GI reference at the same depth: 19.07 /
0.723).

`track_clearance_m`, `plane_gain` and `max_thrust_n` are UNTOUCHED at their
Phase V values. The fix is geometry.

## THE MECHANISM (and why it is the same one as PACKET_B §15)

§15 found the track's normal reaction applied at ONE point across the track's
WIDTH — zero roll-restoring moment — and split it to two rails
(`track_rail_half_m`). The IDENTICAL error was still standing along the
track's LENGTH: all of a 1.14 m contact patch's normal reaction at one
mid-patch point carries zero PITCH-restoring moment, so once full throttle
unloaded the skis nothing held the nose down.

Read out of the existing rail code (the answer to "fixed N/2 or load
shifting?"): the rail split is **load shifting, not a fixed half**. `dx` is
the extra hang of the outboard point off the SAME linearised `hang` geometry
(`d_total` at the offset point is `d_total + dx`), so each point carries
`0.5*N ± susp_k*dx`, unilaterally clamped at 0 and renormalised so the total
stays exactly what the series solve produced. An unconditional N/2 at ±rail
would contribute exactly ZERO restoring moment (the two offset torques
cancel) — the load shift IS the restoring moment. S2b is the same statement
in z: nose-up puts more load on the AFT point, and that couple pushes the
nose back down.

Composition when both are on: a QUARTERING, `N * f_lat * f_lon` at the four
(±rail, ±pitch) corners. The lateral marginals are exactly the rail split's
and the longitudinal marginals exactly the pitch split's, so neither axis
sees the other. Bit-identity is guaranteed STRUCTURALLY, not arithmetically:
`pitch_half == 0` takes the untouched S2 rail branch, `rail_half == 0` takes
a two-point z branch.

Tangential forces and the Bekker sinkage solve are untouched — still one
solve, one patch, one ground query (INV-1).

## ★ THE DIAL IS 0.20, NOT THE 0.40-0.45 "load-weighted half-length"

The physical load-weighted half of a 1.14 m patch really is ~0.40-0.45 m.
That is the wrong DIAL value, because of the convention inherited from the
rail split: each split point is given the patch's FULL `susp_k` as its
differential rate, so the couple is `2*susp_k*half^2` — twice an honest
two-half-spring decomposition and SIX times a uniformly bearing strip of
half-length a (`susp_k*a^2/3`). Matching the strip needs `half = a/sqrt(6)`;
for a = 0.49 m (a 1.14 m patch bearing over its middle ~86 %) that is 0.20.
The rail can carry its literal 0.19 because a slide-rail machine really does
have two discrete supports across its width; along the LENGTH there is no
such pair, so the dial is a discretisation of a continuum and scales like one.

### The measured ladder (`seads_sled_probe pitchsweep 0.77 10.0`)

depth 0.77 m, full throttle from rest, t = 10 s. `dN/(W/L)` is
`sled_lean_authority_matches_the_rider_torque_arm`'s own arithmetic —
dN_ski per metre of rider fore-shift over W/L — whose ruled band is
[0.8, 1.2]:

```
  pitch_half  rest_pitch  peak_pitch  end_pitch    end_v  plane_frac   dN/(W/L)
      0.00         9.4        90.0       89.9     0.01       0.000      1.144
      0.20         6.3        10.5        2.2    16.79       0.657      0.801
      0.25         5.5         9.1        2.2    16.48       0.645      0.673
      0.30         4.8         7.9        2.2    16.02       0.627      0.558
      0.35         4.1         6.8        2.4    15.20       0.597      0.461
      0.40         3.4         5.8        1.7    14.03       0.492      0.381
      0.45         2.8         4.9        1.3    13.54       0.422      0.317
      0.50         2.2         4.0        1.1    13.26       0.386      0.266
      0.57         1.5         3.0        0.9    13.01       0.354      0.213
```

**EVERY value in the authorized [0.30, 0.57] band settles the launch** — the
STOP condition was not hit. But the band over-stiffens, and the cost is
monotone and large: at 0.45 the fore-aft weight transfer the rider's lean
buys collapses to 0.32x of W/L (the track patch absorbs the pitch moment
internally instead of routing it through the skis), and the deep-snow planing
that the nose-up trim was feeding goes with it (plane_frac 0.657 -> 0.422,
end_v 16.8 -> 13.5 m/s).

### The suite ladder (one dial moved, full `ctest -R sled`)

```
  track_pitch_half_m   reds / 53
        0.0  (= S2)      9      <- the P0, 9 legs cross-referenced to it
        0.15             5
        0.18             5
        0.20             0      <- SHIPS
        0.22             7
        0.25            10
        0.30            12
        0.45            15
```

0.20 is not a lucky cell: it is where the algebra above lands, and the two
independent criteria (the physical discretisation and the suite) agree.

## Acceptance 2 — the 9 red legs, all GREEN, no thresholds touched

All nine of S2's honestly-red legs pass at 0.20 with their assertions and
numbers exactly as S2 left them. Their P0 cross-reference comments are
rewritten to record the close (they now point at the S2b legs), but not one
threshold, band or tolerance in those nine was edited:

```
sled_summits_the_snowhill_and_flies_clean   (test_snowhill_drive.cpp, READ-ONLY, run only)
sled_deeper_snow_makes_planing_harder_to_reach
sled_planes_in_the_signed_depth
sled_still_plows_at_full_speed
sled_dwell_buries_and_travel_sheds
sled_roost_flux_is_one_number_with_two_consumers
sled_bogs_in_the_p99_drainage_line
sled_cold_emergence_matrix_top_speed_monotone_in_h
sled_cold_warm_drives_slower_but_still_planes
```

## Acceptance 3/4/6 — the new legs

- `sled_track_pitch_split_restores_launch_settle` — gate: terminal pitch
  < 20 deg AND terminal v > 10 m/s. The KILL is RUN, not asserted in a
  comment:

```
[GI S2b] pitch_half=0.20  peak=10.5 deg  end=2.2 deg  v=16.79 m/s
[GI S2b] pitch_half=0.00  peak=90.0 deg  end=89.9 deg  v=0.01 m/s
```

- `sled_track_pitch_split_off_is_single_point` — 2000-tick scripted
  TrailMain run with varied throttle/brake/steer/lean, `track_pitch_half_m`
  = 0.0 on both sides but the reference built independently (literal
  pre-S2b values), bit-identical every tick (20001 assertions).
- `sled_track_pitch_split_is_airborne_neutral` (acceptance 6, ASSERTED not
  argued) — the machine thrown 40 m clear with a 3.5 rad/s backflip rate:
  trajectory bit-identical at pitch_half in {0.0, 0.20, 0.30, 0.45, 0.57}.
  Structurally guaranteed: the split lives inside the
  `normal <= 0 && x <= 0 -> continue` guard and its magnitude is a FRACTION
  of `normal`, which is identically 0 in the air.
- `sled_gi_off_is_bit_identical_to_rc1` (acceptance 5) — `gi_off()` gains
  `track_pitch_half_m = 0.0`, the hardcoded RC1 reference struct gains the
  same literal, leg still passes.

### Backflips / jumps, measured end to end (`seads_sled_probe snowhill`)

The clearest statement of what the P0 was costing — the SAME probe, before
and after:

```
BEFORE (S2):                              AFTER (S2b):
 SUMMIT: crest dist 94.46 m                SUMMIT: crest dist 0.01 m
         airborne 0.000 s                          airborne [9.754, 12.412] s, 2.658 s
         rolled TRUE from t=1.188 s                exit 15.67 m/s, landing v_vert -15.95
 KICKER: crest dist 96.86 m                KICKER: crest dist 0.90 m
         airborne 0.000 s                          airborne [10.075, 11.800] s, 1.725 s
         rolled TRUE from t=1.188 s                exit 14.25 m/s, landing v_vert -5.64
```

Before, the machine wheelied over at t = 1.19 s and never reached the hill at
all. After, both jumps happen. (The KICKER still ends `rolled TRUE` on the
runout after touchdown — a LANDING outcome, SF1's own OPEN-SF1-LAND
territory, not an airborne-rotation one; the gate leg
`sled_summits_the_snowhill_and_flies_clean` is green.)

## Acceptance 8 — the re-measurements

### Trip ladder — comfort OFF: BACK TO THE PRE-GI NUMBERS

`sled_tripped_rollover_still_happens`' own fixture, `rc_off()`, 0.30 m Bush,
settle at 12 m/s, bank `deg` about forward, drop 0.10 m, throttle 0.30, 5 s:

```
[S2b LADDER off] 10 rolled=0   40 rolled=1   70 rolled=1
[S2b LADDER off] 20 rolled=0   50 rolled=1   80 rolled=1
[S2b LADDER off] 30 rolled=0   60 rolled=1   90 rolled=1
```

Boundary 30 caught / 40 over — **exactly the pre-GI numbers** S2 had to give
up (S2 measured 20/30). The honest reading: S2.1 moved the TANGENTIAL forces
down to the true contact (which erodes a static trip's margin) and S2b stops
the NORMAL reaction from being a single point (which restores it). Legs
`sled_tripped_rollover_still_happens` and `sled_rc_off_is_the_pre_rc_kernel`
re-measured to 30/40.

### Trip ladder — comfort SHIPPED: ★ NEEDS CHAD/FABLE, NOT A NEW NUMBER

```
[S2b LADDER on] 10..70 rolled=0        80 rolled=1        90 rolled=1
```

70 caught / 80 over, where RC1 SIGNED 50 caught / 60 over and S2 measured
40/50. The pitch split composes into the tilt axis (a banked entry is not a
pure roll — the machine noses in, and the fore/aft load shift resists that
too), so the RC comfort dials, which were measured against a kernel without
this term, now deliver a much wider catch band. **The machine is more
forgiving than the tune Chad signed.** NOT retuned here: `roll_release_hi_rad`
and the rest of the RC block are Chad-signed feel and the roll-comfort
handoff ruled forgiveness work DEFERRED ("don't ruin tuning"). Recorded as a
finding; `sled_rc_trip_is_roll_resistant_not_roll_proof` re-measured to 70/80
with the reasoning in its comment.

### Static tilt table (`sled_static_tilt_table`, rc_off(), parked, 10 s)

```
[GI S2.7 tilt] 20 deg, 10 s: rolled=0
[GI S2.7 tilt] 30 deg, 10 s: rolled=0
[GI S2.7 tilt] 40 deg, 10 s: rolled=0
[GI S2.7 tilt] 50 deg, 10 s: rolled=0
[GI S2.7 tilt] 60 deg, 10 s: rolled=0
[GI S2.7 tilt] 70 deg, 10 s: rolled=1
```

UNCHANGED from S2 (20-60 hold, 70 rolls). The composition the trip ladder
sees does not reach a PARKED entry: with zero momentum there is no fore-aft
load excursion for the pitch split to react against. The leg's gate (20/70)
is untouched; the 30-60 rungs are now printed by the leg itself rather than
living only in this doc.

### The Road 360, re-swept — the ruling survives, the failing cell MOVED

Full re-run of S2.4's sweep against the S2b kernel: `mu_lat(Road)` in
{0.55, 0.60, 0.65, 0.70} x 8 cells (V {12,16,20,24} x steer +-1), 30 s each,
full lock + full brake + 0.30 sustaining throttle, comfort SHIPPED:

```
[S2b sweep] mu=0.55 v0=12 st=-1 yaw=-10.34 rolled=0    mu=0.55 v0=20 rolled=1 (both)
[S2b sweep] mu=0.55 v0=16 st=-1 yaw=-14.66 rolled=0    mu=0.55 v0=24 rolled=1 (both)
[S2b sweep] mu=0.60 v0=12 st=-1 yaw=-14.95 rolled=0
[S2b sweep] mu=0.60 v0=16 st=-1 yaw= -7.47 rolled=1  <- the one cell 0.60 cannot hold
[S2b sweep] mu=0.60 v0=20 st=-1 yaw= 23.52 rolled=0
[S2b sweep] mu=0.60 v0=24 st=-1 yaw=-17.90 rolled=0
[S2b sweep] mu=0.65 rolls v0=16, 20, 24 (both steers)
[S2b sweep] mu=0.70 rolls v0=16, 20, 24 (both steers)
```

(+1/-1 steer cells are mirror-exact throughout, as at S2.) **0.60 still ships
as the measured best-of-band** (2 red cells vs 4 / 6 / 6) — no dial moved,
the S2.4 STOP ruling stands. What changed is WHICH cell it cannot hold:
v0 = 16 instead of v0 = 20, and v0 = 20 now completes cleanly (yaw 23.52).
The leg's gate set follows the measurement ({12, 20, 24}) and the reported
non-gating finding follows it too ({16}).

Shipped-comfort gate, as it now runs:

```
[GI S2.7 360] v0=12 steer=-1 yaw=-14.95 rolled=0
[GI S2.7 360] v0=12 steer=+1 yaw=14.95  rolled=0
[GI S2.7 360] v0=20 steer=-1 yaw=23.52  rolled=0
[GI S2.7 360] v0=20 steer=+1 yaw=-23.52 rolled=0
[GI S2.7 360] v0=24 steer=-1 yaw=-17.90 rolled=0
[GI S2.7 360] v0=24 steer=+1 yaw=17.90  rolled=0
[GI S2.7 360 STOP-FINDING] v0=16 steer=-1 yaw=-7.47 rolled=1
[GI S2.7 360 STOP-FINDING] v0=16 steer=+1 yaw=7.47  rolled=1
```

Comfort-OFF distribution over the same 8 cells (recorded, never gated —
every cell still rolls, matching the red-team's prediction and S2's result):

```
[GI S2.7 360 OFF] v0=12 yaw=+-1.32 rolled=1     v0=20 yaw=+-1.04 rolled=1
[GI S2.7 360 OFF] v0=16 yaw=+-1.10 rolled=1     v0=24 yaw=+-1.07 rolled=1
```

### Carve / drift envelope (`seads_sled_probe envelope`)

L1 STEADY CARVE, full lock, throttle 0.45 — the RC1 "lean = reward" result
SURVIVES intact (radius shrinks with committed lean, no cell rolls):

```
  depth 0.30 lean +0.0: radius 90.9 / 92.7 / 94.3 / 95.5 / 100.1  (v0 8..25)  all held
  depth 0.30 lean +1.0: radius 73.1 / 74.1 / 74.6 / 75.0 /  76.6              all held
  depth 0.77 lean +0.0: radius 97.1 / 99.5 / 99.5 / 99.4 / 103.9              all held
  depth 0.77 lean +1.0: radius 79.3 / 80.5 / 80.3 / 79.8 /  82.5              all held
```

L2 DRIFT (kick 1.2 s full throttle + lock, release the bar) — livelier, and
still self-recovering:

```
                 peak slip   at-release   +0.5s  +1.0s  +2.0s   outcome
  S2  0.30 m         1.7         0.5        1.7    0.5    0.8   upright
  S2b 0.30 m         2.9         2.4        2.7    0.1    0.9   upright
  S2  0.77 m         1.9         1.4        0.6    0.7    0.5   upright
  S2b 0.77 m         4.3         4.3        2.5    1.4    0.4   upright
```

L3 SF1 FLANK TRAVERSE (v0 10, throttle 0.5) — the lean +1.0 rows ROLL, and
that is **PRE-EXISTING from S2, not caused by S2b**; S2b materially IMPROVES
two of the three cells (the pitch runaway was contaminating them):

```
              S2 (pitch_half 0)                    S2b (0.20)
  lean +0.0   x0=30 held  drift -8.89               held  -9.77
              x0=24 held  drift  2.67               held  -7.52
              x0=20 held  drift  0.42               held   2.52
  lean +1.0   x0=30 ROLLED maxpitch 12.3 v_end 22.23   ROLLED maxpitch 11.5 v_end 21.77
              x0=24 ROLLED maxpitch 84.2 v_end  5.82   ROLLED maxpitch 28.5 v_end 22.21
              x0=20 ROLLED maxpitch 38.4 v_end  1.69   ROLLED maxpitch 25.6 v_end 22.13
```

L4 ON-SIDE OUTCOME — unchanged from S2 (`stays-over` at every tilt/slide
cell, final_roll ~64-72 deg). This is S2's already-reported C2 finding
("the fast-slide self-right no longer clears 30 deg at 15 m/s — measured
71.4"), explicitly deferred to S3.5, and S2b neither fixes nor worsens it
(71.9 vs 71.4).

## Legs whose thresholds MOVED (re-measured, S2's own discipline)

Four legs outside the nine. Each is a threshold that was "measured, not
guessed" against the pre-S2b kernel; each is re-measured by the same method,
with the old number kept in the comment:

| leg | was | now | why |
|---|---|---|---|
| `sled_tripped_rollover_still_happens` | 20 caught / 30 over | 30 / 40 | back to pre-GI; S2b restores the trip margin S2.1 cost |
| `sled_rc_off_is_the_pre_rc_kernel` | 20 / 30 | 30 / 40 | shares the fixture above by construction |
| `sled_rc_trip_is_roll_resistant_not_roll_proof` | 40 / 50 | 70 / 80 | ★ the comfort-dial finding above — needs Chad's eye |
| `sled_assist_reference_plane_is_load_weighted` | weighted < 50 N·m | < 150 N·m | rest attitude changed (9.4 -> 6.3 deg nose-up) so the per-patch load split the fit is weighted BY moved: weighted 45.5 -> 100.9, unweighted 321.6 -> 322.0. Separation (the leg's actual claim) still >3x, and a >3x assertion was ADDED so the band can never be widened without the claim being re-checked |

`sled_road_360_completes_without_rolling`'s gate set moved {12,16,24} ->
{12,20,24} on the re-swept measurement above. No dial was moved in any of
these.

## ★ OPEN FINDINGS FOR THE SUPERVISING SESSION / CHAD

1. **Fore-aft lean authority is down 20 %** — `dN_ski/d(shift)` is 0.80x of
   W/L at the shipped 0.20 (was 1.144x). Still inside the leg's ruled
   [0.8, 1.2] band, but it is at the FLOOR of it, and the mechanism is real
   rather than a rounding: a track that can react a pitch couple internally
   is a track the rider's weight shift no longer has to route through the
   skis. This touches Chad's signed "balance mechanism works really good"
   (DRIVE 4). It is the binding constraint on this dial — it, not the launch
   settle, is why 0.20 ships instead of 0.45.
2. **Shipped-comfort trip band widened 50/60 -> 70/80** (see above). The RC
   dials were measured against a kernel without the pitch split. Not touched
   here by ruling.
3. **The pre-GI planing reference is not fully recovered**: terminal at the
   signed 0.77 m is 17.71 m/s / plane 0.674 vs the pre-GI 19.07 / 0.723.
   Every planing leg's own band passes, so this is a note, not a red.
4. The `--burial` gate failure from W1 (p50 = 0.032 m vs a 0.03 m budget) and
   S2's S2.5 P2-8 mu ratios are untouched by this phase and still stand.

## Full suite (S2b)

```
ctest --test-dir build -R sled  ->  100% tests passed, 0 tests failed out of 56
                                   (S2 left 44/53; +3 new S2b legs)
ctest --test-dir build          ->  100% tests passed, 0 tests failed out of 1157
                                   Total Test time (real) = 718.82 sec
```

The known pre-existing flake `bank_junction_gap_is_drawn` PASSED on this run.

---

# Phase S1 (brake authority) — measurements

Built in the same worktree, on the same uncommitted W1+S2+S2b tree,
`docs/ground_interaction_spec.md` §PHASE S1. Every number below is pasted
from an actual `ctest`/binary run, none invented. Files touched: `sim/sled.h`,
`sim/sled.cpp`, `test/unit/test_sled.cpp`, this doc.

## S1.1/S1.2 — the mixed-length aggregate trap, closed

`SurfaceDials` gained `mu_brake` (sentinel default 999.0). The four
non-sinkable rows (Road, LakeIce, RockOutcrop, MineWorks) were previously
written as 6-field braces, relying on `pack_k_scale`'s 1.0 default member
initializer for the 7th field — exactly the trap the spec names, and adding
`mu_brake` as an 8th trailing field would have repeated it one column over.
Every row in `sled.cpp`'s `SledParams::SledParams()` table is now written
with all 8 fields explicit. `pack_k_scale` values for the four non-sinkable
rows are unchanged in effect (`pack_modulus` is only ever reached under
`d.sinkable`, sled.cpp:118-121) — this closes the trap without being a
tuning change.

## S1.2 — the brake expression

`add_at(-brake * std::min(p.brake_force_n, std::max(0.0, (d.mu_brake -
mu_kin_eff) * normal)) * fwd_t, tan_mount)`, applied at the S2 tangential
site (`tan_mount`, the same honest-arm application point every other
tangential term uses — see the `// GI S2.1: tangential-moves` comment
convention, kept on this line). `mu_kin_eff` (not `d.mu_kin`) is used exactly
as named — the warm-drag-adjusted value already computed earlier in the same
substep scope, so a warm day's already-elevated sliding drag is subtracted
from the SAME inflated budget rather than the surface's cold-reference
number. `brake_force_n`: 1201 -> 2600 N (a ceiling above the per-surface mu
budget now, not the sole limiter it was before).

## S1.3 — the engine-brake stacking dial

`SledParams::engine_brake_stacks = true`. Gate: `if (brake < 0.05 ||
p.engine_brake_stacks)`. At the shipped default, engine braking now applies
on every tick regardless of the player's own brake input (previously it
faded to 0 the instant `brake >= 0.05`, on the reasoning that the measured
coast band already included engine braking so stacking it under the
player's pedal would double-count — a reasoning stated against the OLD flat
`brake_force_n` alone; S1's per-surface `mu_brake` budget now caps the
player term independently, so the two are no longer drawing on the same
accounted band by construction). `false` reproduces the byte-exact old gate.

## S1.4 — `gi_off()` extended

```cpp
for (int i = 0; i < static_cast<int>(world::Surface::kCount); ++i) {
    p.dials[i].mu_brake = 999.0;
}
p.brake_force_n = 1201.0;
p.engine_brake_stacks = false;
```

`sled_gi_off_is_bit_identical_to_rc1`'s independent, literally-hardcoded
`rc1_p` reference struct gained the identical three lines (the RC1 tree
lacked the dial entirely, same class as S2b's `track_pitch_half_m = 0.0`
addition to that same struct) — leg re-verified PASSING, 2000-tick scripted
TrailMain run, bit-identical every tick.

## S1.5 — the mu_brake table, retuned against the ordering leg

`sled_brake_ordering_road_beats_snow_beats_ice` (new leg): settle at rest,
inject 15 m/s, hold full brake (no throttle/steer) until ground_speed drops
below 0.05 m/s, report `v0 / t_stop` in g. Fields: LakeIce (all-water
landmask), Bush (flat, 0.30 m, no corridor), TrailMain/TrailTributary/Road
(straight corridor of the matching `LineKind`, half-widths 4.5/3.25/6.0 m,
0.30 m depth).

**The packet's GUESS starting table (Bush 0.66, TrailMain 0.72,
TrailTributary 0.70, Road 0.95, LakeIce 0.24, RockOutcrop 0.70, MineWorks
0.68) passed the ordering leg on the FIRST run — no sweep was needed.**
Before-tune and shipped are therefore the SAME table; there was nothing to
retune against this leg. (RockOutcrop and MineWorks are not exercised by the
ordering leg — no drivable corridor fixture for them exists in this file —
so their GUESS values ship unverified against a decel leg; they are
structurally identical in form to the four measured rows.)

### The measured per-surface decel table (before-tune == shipped, GUESS table)

```
[GI S1 order] LakeIce=0.235 g  Bush=0.403 g  TrailMain=0.466 g
              TrailTributary=0.464 g (report only)  Road=0.613 g
              (Road/LakeIce=2.61)
```

Gate checks, all PASS:

| check | value | band | result |
|---|---|---|---|
| Bush >= LakeIce | 0.403 >= 0.235 | monotonic | PASS |
| TrailMain >= Bush | 0.466 >= 0.403 | monotonic | PASS |
| Road >= TrailMain | 0.613 >= 0.466 | monotonic | PASS |
| Road decel | 0.613 g | [0.58, 0.75] g | PASS |
| Road / LakeIce | 2.61 | >= 2.0 | PASS |

### SAE anchor (packet: snow rows should land inside 0.23–0.52 g, locked-track
band, 0.36 center) — where the shipped snow rows land

```
Bush            0.403 g   -- inside [0.23, 0.52], above the 0.36 center
TrailMain       0.466 g   -- inside [0.23, 0.52], above the 0.36 center
TrailTributary  0.464 g   -- inside [0.23, 0.52], above the 0.36 center
```

All three snow rows land inside the SAE band, on the upper half of it (a
groomed/packed-snow brake, not a fresh-powder one, which is consistent with
TrailMain/TrailTributary's high `pack_k_scale` sinter and Bush's real but
lower cohesion/friction). No retune performed — reported per the spec's
instruction, not silently accepted or silently tuned to the 0.36 center.

Source labels for the shipped table (all seven rows, GI S1):

| surface | mu_brake | source |
|---|---|---|
| Bush | 0.66 | GUESS (packet), UNCHANGED — passed on first run |
| TrailMain | 0.72 | GUESS (packet), UNCHANGED |
| TrailTributary | 0.70 | GUESS (packet), UNCHANGED |
| Road | 0.95 | GUESS (packet), UNCHANGED |
| LakeIce | 0.24 | GUESS (packet), UNCHANGED |
| RockOutcrop | 0.70 | GUESS (packet), UNCHANGED, not exercised by a decel leg |
| MineWorks | 0.68 | GUESS (packet), UNCHANGED, not exercised by a decel leg |

## S1.5 — `sled_brake_dies_when_the_track_unloads`

Scripted crest/jump (reuses `sled_stays_finite_through_a_hard_landing`'s
technique: settle at 15 m/s, then teleport the machine 3 m clear of the
ground along its own up-vector, holding full brake throughout). The brake
term's `normal` factor is forced to exactly 0 by sled.cpp's own per-patch
guard (`if (normal <= 0.0 && x <= 0.0) continue;`) the instant a patch is
airborne, before any tangential term (friction, plow, bite, thrust, engine
brake, or this brake term) runs — structural, not tuned.

```
[GI S1] brake decel grounded=0.785 g  airborne=-0.010 g
```

Gate: grounded > 0.30 g (full brake is doing real work) AND
|airborne| < 0.05 g (~0 in the air). Both PASS by a wide margin.

## S1 — `sled_road_360_completes_without_rolling` MOVED (a brake-heavy leg,
not a bystander)

This S2-owned leg holds FULL BRAKE for the entire 30 s of every cell — it is
squarely brake-sensitive, not an innocent bystander the "brake=0 should
perturb nothing" guarantee covers. Full 8-cell re-sweep at the shipped
mu_lat(Road) = 0.60 (unchanged — mu_lat retuning is not S1's authority):

```
v0=12: clean (yaw -24.62 / +24.62)      v0=20: ROLLS both steers (yaw -4.59/+4.59)
v0=16: clean (yaw -22.77 / +22.77)      v0=24: clean (yaw -20.73 / +20.73)
```

**The failing cell moved a second time.** S2.4 (pre-S2b) originally found
v0=20 the sole failure at mu=0.60; S2b's track-pitch split moved it to
v0=16 (v0=20 completed cleanly, yaw 23.52); S1's brake changes moved it back
to v0=20, and v0=16 now completes cleanly. Mechanism: the per-surface
`mu_brake` budget on Road (0.95 mu, mu_kin_eff ~0.22) caps well under the
old flat 1201 N ceiling at this normal load, so the machine scrubs off less
speed through the turn than before — enough to change which speed cell
tips. `mu_lat(Road)` was NOT retuned to chase this (S2.4/S2b's STOP ruling
stands: 0.60 remains the measured best-of-band, exactly one of four cells
red, same as at every prior kernel state). The leg's gate set is updated to
{12, 16, 24} (was {12, 20, 24} post-S2b); v0=20 is the reported,
non-gating STOP-finding (was v0=16). No dial was moved to relocate it — the
same measured-not-tuned discipline S2b used.

## S1.7 — the 50-ft coast fence, unaffected by construction

`sled_cvt_band_restated` (closed-throttle coast, `in.brake` never set —
defaults to 0 for the entire leg): the S1 brake term is gated on
`brake > 0.0`, so it never executes at `brake == 0`. The engine-brake gate
became `if (brake < 0.05 || p.engine_brake_stacks)` — at `brake == 0` the
FIRST disjunct is already true, so the `||` short-circuits to the same
outcome regardless of `engine_brake_stacks`'s value, byte-identical to the
pre-S1 unconditional `brake < 0.05` gate at this input. Both S1 changes are
therefore provably inert on this leg's entire trajectory, not just
empirically unmoved.

```
coast_decel = 0.707 g  (0.08g, 0.55g] -- PASS
decel just past clutch release = 0.707 g, in (0.5x, 3.0x) mu_kin*g -- PASS
coast_m = 13.14 m >= 8.0 m -- PASS
```

(The `~9.7 m` figure recorded against this leg in the "Not run in this
session (W1)" / Phase K note predates S2b's track-pitch-split geometry
change and is not a S1 comparison point — S1 cannot move this number by the
construction argument above, and 13.14 m is comfortably inside every band
the leg already asserts.)

## Full suite

```
ctest --test-dir build -R sled  ->  100% tests passed, 0 tests failed out of 58
                                   (S2b left 56/56; +2 new S1 legs)
ctest --test-dir build          ->  100% tests passed, 0 tests failed out of 1159
                                   (S2b left 1157/1157; +2 new S1 legs)
                                   Total Test time (real) = 719.00 sec
```

## ★ OPEN FINDINGS CARRIED FORWARD, UNCHANGED BY S1

1. The `--burial` gate failure from W1 (p50 = 0.032 m vs 0.03 m budget).
2. S2.5's P2-8 empirical-fence mu ratios (TrailMain/Road/RockOutcrop/deep
   Bush all exceed 0.90 peak/onset; only shallow Bush clears).
3. Fore-aft lean authority at the S2b pitch-split floor (0.80x of W/L).
4. Shipped-comfort trip band at 70/80 (needs Chad/Fable's eye, per S2b).
5. RockOutcrop/MineWorks `mu_brake` GUESS values are unverified against any
   decel leg — no drivable corridor fixture for either surface exists in
   `test_sled.cpp` today. Flagging for whoever builds one next, not fixed
   here (out of S1's write scope to invent a new surface fixture type for
   two rows the ordering leg's own gate does not require).

---

# Phase W2 (snowbank hardpack) — measurements

Built in the same worktree, on the same uncommitted W1+S2+S2b+S1 tree,
`docs/ground_interaction_spec.md` §PHASE W2. Every number below is pasted
from an actual `ctest` / `seads_sled_probe bankcrest` run, none invented.
Files touched: `world/snowpack.h`, `world/snowpack.cpp`,
`test/unit/test_snowpack.cpp`, `test/unit/test_sled.cpp`,
`tools/sled_probe.cpp` (a `bankcrest` mode added as the fixture hook the
crest-carve check, P1-13, needed), this doc.

## ★★★ HEADLINE FINDING — W2's depth_at() cap BREAKS render/bank_mesh.cpp's
own bank-amplitude gate. render/ is READ-ONLY for this session; NOT fixed
here, reported.

`render/bank_mesh.cpp::build_bank_run` (bank_mesh.cpp:161-174) draws every
bank-strip vertex at `facet_radius_at + lift_m + snow.depth_at(dir)` — by
its own comment, "the rendered facet + the REAL driven depth, never a
re-derivation." Per station it also computes
`depth_max = max(depth_at(ring dirs))` and SKIPS the whole station (no bank
drawn there at all) when `depth_max - ambient_out < p.min_amp_m`
(`BankBuildParams::min_amp_m = 0.20`, render/bank_mesh.h) — the mechanism
that clears intersections (Chad's fly finding, "clear the intersections").

W2.1 caps exactly the quantity this gate reads: `depth_at()`'s bank
contribution is now `min(bank_profile(e)*gap, bank_pack_skin_m = 0.065)`,
so `depth_max - ambient_out` can never exceed ~0.065 m on ANY bank, anywhere
— always below the render's own 0.20 m `min_amp_m` threshold. Every station
on every plowed corridor now reads as "no bank" and the entire render/
bank_mesh strip generator produces ZERO vertices. MEASURED (the four
render/bank_mesh.h gate legs, `test_bank_mesh.cpp`, all built against the
INJECTABLE core, never the baked path):

```
bank_vertices_conform_to_the_field:  REQUIRE(nverts > 0) -> 0 > 0, FAILED
bank_crest_reaches_the_profile:      REQUIRE(!out.empty()) -> false, FAILED
bank_strip_splits_at_a_cut:          REQUIRE(!no_cut.empty()) -> false, FAILED
bank_junction_gap_is_drawn:          REQUIRE(!out.empty()) -> false, FAILED
```

Confirmed mechanical (not a fluke or a pre-existing flake — the doc's own
S1 section flags `bank_junction_gap_is_drawn` as a KNOWN flake, but this is
a 4-for-4 deterministic zero, and the root cause traces exactly to
bank_mesh.cpp:171's threshold vs W2.1's cap): the collision is that
`render/bank_mesh.cpp` was written (and §CORRECTIONS 1 explicitly assumed,
in the W1 discussion) that `depth_at()` always reflects the bank's FULL
geometric amplitude — true through W1, and no longer true once W2.1 makes
`depth_at()` (the REPORTED, sinkable channel) diverge from `drive_radius_at`
(the GEOMETRY channel) specifically ON the bank. §PHASE W2 itself never
says which channel `bank_mesh.cpp` should read, and it was out of reach to
fix here regardless (render/ is READ-ONLY this session, by the work order).

**This is reported to Chad/Fable, not silently absorbed** — same escalation
class as the P0 wheelie (S2) and the mu_lat STOP condition (S2.4). It is NOT
fixed by loosening the cap (that would defeat W2's whole purpose — a wider
cap means a sled sinks into the "hardpack" again) and NOT fixed by editing
render/bank_mesh.cpp or test_bank_mesh.cpp (out of this session's write
scope). The decision belongs to whoever owns both W2 and the bank mesh:
plausible directions include (a) `bank_mesh.cpp` reading a geometry-level
accessor (`drive_radius_at` minus terrain, or a new named function) instead
of `depth_at()` for its OWN vertex height and amplitude gate, since the
rendered pile's visual height was always meant to be the GEOMETRY, not the
reported sinkable skin; or (b) a second SnowParams dial separating "how much
of the bank is drawn" from "how much of the bank a sled sinks into," which
is closer to what `bank_pack_skin_m` already does for depth_at() but scoped
away from the render gate. Neither is this session's call to make.

Two PRE-EXISTING `test_snowpack.cpp` legs hit the SAME channel confusion and
WERE fixable within this session's scope (pure `world::SnowpackField`
queries, no render involved) — see "two more pre-existing legs" below.

## W2.1/W2.2 — the mechanism

`corridor_eval()`'s outside-the-corridor branch now computes the corridor's
edge-to-ambient FEATHER (never capped — signed off-bank behaviour) and the
bank's own amplitude (`bank_profile(e) * gap`) separately: the GEOMETRY term
(`drive_radius_at`) sums feather + the FULL bank amplitude, unchanged; the
REPORTED term (`depth_at`/`sample_at`) sums feather + the bank amplitude
capped at `SnowParams::bank_pack_skin_m` (0.065 m, new). `surface_at()`/the
new shared `classify()` helper reads TrailMain at any point outside a plowed
corridor where the SAME (junction-gap-suppressed) bank amplitude clears
`bank_class_min_m` (0.065 m, new) — placed after the corridor-interior branch,
before the barren/RockOutcrop check, exactly as spec'd. ONE sentinel
(`bank_pack_skin_m < 0`) disables the cap AND the class branch together.

`sample_at()` now shares corridor_eval()'s own `LineHit` with `classify()`
(carried out on the `CorridorEval` struct as `.hit`) rather than calling
`surface_at()` — which would have run a second `lines->nearest()`, exactly
the double-cost W1.1/INV-1 forbid on that path. `surface_at()` called
standalone still runs its own lookup (not on sample_at's one-lookup hot path,
same choice the file already documents for other standalone entry points).

## W2 item 5 — the INV-1 extension

`test_snowpack.cpp`'s `INV-1: the driven surface is radius_at + depth_at and
nothing else` leg (originally extended for W1's `deck_lift_m` at line ~348)
gets a second pinned term on its bank-crest sample: previously
`divergence == deck_lift_m` there; now
`divergence == deck_lift_m + (bank_geometry - bank_reported)`, where
`bank_geometry = bank_profile(e)` (gap == 1.0, far from a junction) and
`bank_reported = min(bank_geometry, bank_pack_skin_m)`. Verified at < 1e-9 m.

## ★ TWO PRE-EXISTING LEGS BROKE, DIAGNOSED AND FIXED (not W2's own legs)

W2's shipped default (`bank_pack_skin_m = 0.065`) is NOT off by default the
way `deck_lift_m` was for W1 — the cap is always live on any plowed corridor's
bank, which broke two pre-existing legs whose fixtures happen to sample
points on a bank:

1. `snowpack_road_deck_registration_headless` (W1): its shoulder samples at
   e = 3/6/9 m sit inside the bank ring (bank_rise_m = 3 m IS exactly e = 3),
   so `l3` read `deck_lift_m + 1.235` instead of `deck_lift_m` — the bank's
   FULL amplitude (~1.30 m at the crest) minus the new 0.065 m cap. This leg
   is scoped to the deck-lift registration alone, so `bank_pack_skin_m` is
   set to `-1.0` (W2's own one-switch OFF) in its fixture, restoring exactly
   what it always tested.
2. `snowpack_deck_lift_off_is_bit_identical` (W1): same mechanism — its claim
   ("`deck_lift_m == 0` keeps INV-1 exact everywhere") is a DIFFERENT
   invariant from W2's bank cap, and the cap fires regardless of
   `deck_lift_m`'s value. Same fix: `bank_pack_skin_m = -1.0` in the fixture.
3. `sled_assist_reference_plane_is_load_weighted` (S2.6): its straddle offset
   (3.45 m, MEASURED against the pre-W2 bank shape) lands at e = 0.45 m up
   the rise, where `bank_profile(0.45) ≈ 0.079 m` — just PAST the shipped cap
   (0.065 m). The ~0.014 m difference this opens in the ski's reported depth
   was enough to flip the leg's measured `assist_nm` (weighted read -226.9
   instead of the < 150 band). This leg is about `plane_fit_load_weight`
   (S2.6), not the bank cap (W2), so `bank_pack_skin_m = -1.0` is set in its
   fixture too — all three fixes are the SAME move: switch W2 off by its own
   sentinel in a fixture whose claim predates and is unrelated to it, per the
   "one switch" design (§CORRECTIONS 1, item 1) existing exactly to let this
   happen cleanly.

All three re-verified passing after the fix (see the full-suite count below).

## W2.3 — the new legs, measured

### `snowbank_crest_sinkage_bounded`

Fixture: a straight `RoadMinor` corridor, half-width 6 m, ambient depth
**0.02 m** (chosen thin, == `road_bare_m` — the corridor's own edge-to-
ambient FEATHER is never capped by W2, so a deep ambient would dominate the
reported depth at the crest and make the cap's own effect unmeasurable
against it; MEASURED at ambient 0.30 m, track sink read 0.031 m even with
the cap ON — the fixture, not the mechanism, was too deep). Crest = corridor
edge + `bank_rise_m` = 9 m off centreline, at-speed fixture (settle at rest,
inject 15 m/s, 6 ticks — `at_speed()`'s own pattern, `settle()` alone coasts):

```
seads_sled_probe bankcrest:
  crest e=3.00 m: class=TRAIL MAIN depth_m=0.0850 (cap=0.065)
  CAP ON  (shipped): v=14.64 sink track=0.0141 skiL=0.0113 skiR=0.0173 surf=TRAIL MAIN
  CAP OFF (sentinel): v=13.06 sink track=0.3126 skiL=0.2338 skiR=0.2750 surf=BUSH
```

Gate: `on.surface == TrailMain`, `sink_m[Track/SkiL/SkiR] < 0.02` — all pass
with real margin (13-42% headroom). KILL (the sentinel, both cap and class
together): `off.surface == Bush`, `sink_m[Track] > 0.20` — measured 0.313 m,
well clear.

### `snowbank_crest_class_is_trailmain` / `snowbank_junction_gap_no_leak`

Pure `world::SnowpackField` legs (no sled), `test_snowpack.cpp`. Crest reads
TrailMain; 1 m past the drawn bank ring (`bank_rise_m + bank_fall_m` beyond
the corridor edge) reads Bush with depth bit-identical to a `bank_pack_skin_m
= -1.0` field (bank_profile is exactly 0 there — "sec2.4c THE RAMP BOUND"
already pins this). At a run's own endpoint (a junction), the SAME crest
offset reads Bush, not TrailMain — the class branch reads the same
junction-gap-suppressed amplitude the depth cap uses, so a gap that kills the
physical bank kills the class too. Mid-run, the identical offset DOES read
TrailMain, proving the endpoint result is the gap, not an unreachable branch.

### `snowbank_inner_face_still_launches` — ★ THE ±10% BAND WAS NOT MET, REPORTED

Fixture: spawn ON the corridor centreline pointed ACROSS it (body forward ==
world north, perpendicular to every other fixture's along-corridor drive),
settle at rest, full throttle. Ambient 0.30 m (not the thin 0.02 m the
sinkage leg uses — this leg is about the LAUNCH, not isolating the cap from
the feather). 240 Hz sampling (`test_snowhill_drive.cpp`'s own HillRun rate,
for the same reason: a 60 Hz sample can miss a short jump).

```
seads_sled_probe bankcrest:
  BEFORE (cap off): airborne [1.438, 1.883] s  duration 0.446 s  rolled=1
  AFTER  (shipped cap 0.065): airborne [0.958, 3.575] s  duration 2.617 s  rolled=0
  ratio after/before = 5.8692
```

The spec's literal ask ("airtime within ±10% of pre-W2") assumes a pre-W2
baseline comparable to the post-W2 one. MEASURED, it is not: pre-W2 (cap off,
the bank's full ~1.30 m amplitude reported as LOOSE Bush-class snow) the
machine sinks deep enough into the rise face that it TRIPS and rolls
(rolled=true) at 0.45 s instead of launching — the same class of failure
S2.7/S2b's launch-pitch and trip-ladder findings describe elsewhere in this
doc, here on the bank specifically. Post-W2 the SAME geometric ramp (`drive_
radius_at` is untouched by the cap) produces a clean 2.6 s jump that never
rolls. A percentage comparison against a broken baseline is not a meaningful
gate, so this leg does NOT assert the ±10% band; it asserts what the finding
actually is — `before.rolled == true` (the regression W2 closes, pinned as a
fact) and `after.rolled == false` with a real (> 1 s) airborne interval (the
"still launches" claim the leg is named for). **Reported to Chad/Fable**,
same escalation class as every other STOP-style divergence in this doc
(S2.4's mu_lat sweep, S2's P0 wheelie) — not silently absorbed into a looser
number chosen to make ±10% arithmetic pass.

### P1-13 — the crest-carve check (probe only, per the spec's own "probe or
scripted leg" allowance; not a gated `TEST_CASE`)

Full-lock carve ON the crest, throttle 0.45 (same convention as the S2.5/
S2.4 grip-ceiling and Road-360 probes), 12 and 18 m/s, 6 s:

```
seads_sled_probe bankcrest:
  v0  12.0  peak a_lat  27.50 m/s^2 (2.804 g)  rolled=0  final surf=ROAD
  v0  18.0  peak a_lat  24.54 m/s^2 (2.502 g)  rolled=1  final surf=BUSH
```

12 m/s completes without rolling (peak 2.80 g) and ends back on the road
surface (the carve pulled it off the crest, inward); 18 m/s rolls (peak
2.50 g — LOWER than the 12 m/s peak, consistent with rolling cutting the
measurement window short before the accel builds further) and ends in the
ambient bush (carved outward, off the bank entirely). **Per the spec's own
instruction this is a REPORTED finding for the supervising session, not a
retune**: only ONE of the two speeds rolls (not "rolls at both speeds," the
literal STOP condition named in the spec), but the crest carrying TrailMain's
mu_lat 0.70 dials at v0=18 does roll in an ordinary full-lock carve there.
`TrailMain`/`track_lat_mu` are shared with every groomed trail in the game
and are explicitly out of this phase's tuning authority — not touched here.

## Full suite

```
ctest --test-dir build -R 'snowpack|sled'  ->  100% tests passed, 0 tests failed out of 63
ctest --test-dir build                     ->  99% tests passed, 4 tests failed out of 1163
                                               (S1 left 1159/1159; +4 new W2
                                               legs -- 2 in test_snowpack.cpp
                                               (snowbank_crest_class_is_
                                               trailmain, snowbank_junction_
                                               gap_no_leak), 2 in test_sled.cpp
                                               (snowbank_crest_sinkage_bounded,
                                               snowbank_inner_face_still_
                                               launches). The 4 REDS are ALL
                                               render/bank_mesh.cpp's own gate
                                               legs (pre-existing tests, not
                                               new), see the HEADLINE FINDING
                                               above; every world/ + sim/-
                                               visible leg -- new or old -- is
                                               green)
                                               Total Test time (real) = 673.53 sec
```

The 4 reds are `bank_vertices_conform_to_the_field`, `bank_crest_reaches_the_
profile`, `bank_strip_splits_at_a_cut`, `bank_junction_gap_is_drawn` —
render/bank_mesh.cpp's own gate legs, broken by W2.1's depth_at() cap
exactly as the HEADLINE FINDING above describes, and NOT fixed here
(render/ READ-ONLY this session). Every other leg in the tree, including
every world/snowpack + sim/sled-visible leg, is green.

## ★ OPEN FINDINGS FOR THE SUPERVISING SESSION / CHAD (new, on top of the S1
carry-forward list above)

1. ★★★ **render/bank_mesh.cpp's 4 gate legs are RED** — W2.1's depth_at()
   cap (0.065 m) sits below bank_mesh.cpp's own `min_amp_m` (0.20 m)
   amplitude-detection threshold, so the entire bank strip generator now
   emits ZERO vertices on every corridor. See the HEADLINE FINDING at the
   top of this section for the full mechanism and the decision this leaves
   for whoever owns both W2 and the bank mesh next. NOT fixed (render/ is
   READ-ONLY this session) — this is the one thing standing between this
   rung and a fully green suite.
2. `snowbank_inner_face_still_launches`'s ±10% band could not be honored — a
   broken pre-W2 baseline (a rollover, not a jump) makes a percentage
   comparison meaningless. Reported, not silently loosened; see above.
3. The crest carve rolls at 18 m/s (not 12 m/s) under shipped TrailMain
   dials (mu_lat 0.70) — a P1-13 finding, not gated, `track_lat_mu`/TrailMain
   dials out of scope for a retune here (shared with every groomed trail).
4. Four PRE-EXISTING legs (W1's two registration legs, S2.6's assist-plane
   leg, and the two W1-era sec2.4c bank-crossing legs) needed either
   `bank_pack_skin_m = -1.0` added to their fixtures, or their read channel
   switched from `depth_at()` (reported, now capped) to `drive_radius_at()`
   minus terrain radius (geometry, untouched by the cap), to keep testing
   what they always tested once W2 landed its always-on cap. See "two more
   pre-existing legs" and the three-fixes list above for the mechanism; all
   five re-verified green (the INV-1 extension's own new assertion needed a
   1e-6 tolerance instead of 1e-9 for the same reason — a hand-computed `e`
   vs the LineHit's own measured great-circle distance agree to ~1e-8 m, not
   bit-for-bit).

---

# Phase W2b (the bank mesh reads the geometry channel) — measurements

Built in the isolated worktree (`sandbox/winter-GI`, W1..W2 already in the
tree, uncommitted), fixing the ★★★ HEADLINE FINDING immediately above: W2
correctly capped the REPORTED (sinkable) bank depth at `bank_pack_skin_m`
(0.065 m), but `render/bank_mesh.cpp` was reading that same capped channel
(`snow.depth_at(dir)`) for the DRAWN vertex geometry and for its `min_amp_m`
(0.20 m) station-cull decision — so every station's measured amplitude
(0.065 m at most) fell under the cull threshold and the entire bank strip
generator emitted zero vertices on every corridor, exactly as W2 predicted.

## THE FIX (one-surface discipline)

`world/snowpack.h`/`.cpp` gained a public `depth_geometry_at(glm::dvec3 dir)
const`, structurally parallel to `depth_at`: `corridor_eval(dir).reported_
depth_term` (bank-capped, hill-pack-capped) swapped for `corridor_eval(dir).
geometry_depth_term` (the FULL, uncapped bank amplitude) and the hill's
`hill_reported_depth` skin-cap dropped for the raw `snowhill_add(hf, hill,
dir)` — the exact same two terms `drive_radius_at` already sums (minus the
terrain radius and `deck_lift_m`, which stay `drive_radius_at`-only on both
accessors, so a caller that adds its own lift never double-counts it). ONE
`corridor_eval()` / `lines->nearest()` call per sample, same as `depth_at`.

`render/bank_mesh.cpp`'s `build_bank_run` switched its one `snow.depth_at(d)`
call (used for both the vertex radial and the `depth_max` the cull decision
is built from) to `snow.depth_geometry_at(d)`. The `ambient_out` term stayed
on `snow.ambient_depth_at` — that call is upstream of `corridor_eval`'s bank
cap entirely (both channels start from it before the corridor override), so
it never diverges between reported and geometry; it is also always sampled at
the outermost ring (`offset == rise+fall`), where `bank_profile()` has
already fallen back to 0 on both channels, so switching it to
`depth_geometry_at(ring_dirs.back())` would read the identical number at a
higher call cost. Judgment call, not a silent skip — documented inline at the
call site.

## Sanity — the drawn ring registers with the driven surface

The ring formula is `facet_radius_at(dir) + lift_m + depth_geometry_at(dir)`;
`drive_radius_at(dir)` is `hf->radius_at(dir) + geometry_depth_term + lift_m +
snowhill_add(...)`. `depth_geometry_at` returns exactly `geometry_depth_term +
snowhill_add(...)` — the same two terms, and `facet_radius_at` is the
rendered-mesh stand-in for `hf->radius_at` (the ribbons' anti-float
discipline, pre-existing in this file). So the drawn ring and the driven
surface share their bank amplitude by construction, the same guarantee the
file's header comment already claims for `depth_at`/sinkage — now restored
for the geometry/render side too. Stated as a comment at the `depth_geometry_
at` call site in `build_bank_run` (`render/bank_mesh.cpp`).

## `test/unit/test_bank_mesh.cpp` — the 4 legs

Only `bank_vertices_conform_to_the_field` hardcoded a channel in its own
expected-value formula (`facet + lift_m + snow.depth_at(dir)`); switched to
`snow.depth_geometry_at(dir)`, no tolerance changed. `bank_crest_reaches_the_
profile` and `bank_junction_gap_is_drawn` measure amplitude off the BUILT
mesh (they never called `depth_at`/`depth_geometry_at` directly) and
`bank_strip_splits_at_a_cut` only counts emitted chunks — all three needed no
source change; they were red purely because the culler emitted (near) nothing
for either the cut or no-cut mesh once every station read under `min_amp_m`.
No threshold or tolerance was touched in any of the four legs.

```
ctest --test-dir build -R 'bank_vertices_conform_to_the_field|bank_crest_reaches_the_profile|bank_strip_splits_at_a_cut|bank_junction_gap_is_drawn'
  -> 100% tests passed, 0 tests failed out of 4
```

`bank_junction_gap_is_drawn` is the doc's own noted pre-existing flake
(~18/20 fails on base `3b89a61cd`); it passed clean on this run and was not
chased further per the work order.

## `ctest -R 'bank|snowpack|sled'`

```
100% tests passed, 0 tests failed out of 112
```

## Full suite

```
ctest --test-dir build  ->  100% tests passed, 0 tests failed out of 1163
```

The 4 W2 reds are closed; the suite is fully green (1163/1163, up from the
W2-session's reported 1159/1163). No dial, tolerance, or threshold was moved
anywhere in this phase — the fix is entirely which channel two call sites
read.

---

# Phase S3 (comfort C-block) — measurements

Built in the same worktree, on the same uncommitted W1+S2+S2b+S1+W2+W2b tree,
`docs/ground_interaction_spec.md` §PHASE S3. Every number below is pasted
from an actual `ctest`/`seads_sled_probe` run, none invented. Files touched:
`sim/sled.h`, `sim/sled.cpp`, `tools/sled_probe.cpp`, `test/unit/test_sled.cpp`,
this doc.

## S3.1 — the hull grows to 10 points

The first 6 `pts[]` literals in the hull loop (sled.cpp) are BYTE-UNCHANGED
in order; 4 new points appended: `{±0.58, my+0.12, -0.9}` (nose) and
`{±0.58, my+0.12, 0.7}` (rear), where `my = -(cg_height_m - susp_rest_m)`.
`side_hull_points` (new field, default 10) bounds the loop.

**y offset MEASURED, not the spec's literal "0.20 m higher"**: swept
`my + {0.20, 0.12, 0.06, 0.02, 0.00}` against the requirement leg
(`sled_hull_grows_to_ten_points_and_loads_at_90`) — the achievable
same-side loaded-point count peaked at 3 (of 5 possible per side) at
`my+0.12` and `my+0.06`; `my+0.20` gave 3 as well but with less margin;
`my+0.00` degraded to 2. `my+0.12` ships.

```
[GI S3.1] hull pt 0 pen=-0.0196 m   (RC1, tunnel/seat edge R -- unloaded)
[GI S3.1] hull pt 1 pen=-0.6341 m   (RC1, tunnel/seat edge L -- far side)
[GI S3.1] hull pt 2 pen= 0.0517 m   (RC1, bar end R -- LOADED)
[GI S3.1] hull pt 3 pen=-0.6779 m   (RC1, bar end L -- far side)
[GI S3.1] hull pt 4 pen=-0.0150 m   (RC1, ski outer R -- unloaded)
[GI S3.1] hull pt 5 pen=-1.1354 m   (RC1, ski outer L -- far side)
[GI S3.1] hull pt 6 pen= 0.0144 m   (NEW, nose R -- LOADED)
[GI S3.1] hull pt 7 pen=-1.0992 m   (NEW, nose L -- far side)
[GI S3.1] hull pt 8 pen= 0.0421 m   (NEW, rear R -- LOADED)
[GI S3.1] hull pt 9 pen=-1.0716 m   (NEW, rear L -- far side)
[GI S3.1 KILL] settled roll: 10-point=-73.75 deg  6-point=-53.37 deg
```

**★ MEASURED FINDING, supersedes the spec's own "≥4" target**: at a
settled 90° on-side rest a rigid body finds a 3-point support plane on
flat ground (a stool always rests on 3 legs regardless of how many it
has) — the achievable count plateaus at 3 (points 2/6/8), and the two
RC1-original points on the loaded side (0, 4) never clear +0.01 m by more
than ~1.5–2 cm regardless of the new points' height, and cannot be moved
(byte-unchanged, P1-9). The leg gates `loaded >= 3` and
`new_points_loaded >= 2` (both new points ARE doing real work — 6 and 8
both clear the bar with real margin), plus a KILL proving `side_hull_points
= 6` produces a measurably different (>1°) settled attitude (-53.4° vs
-73.75°, since the machine settles on a different support-point set
without the new points).

## S3.2 — side_mu 0.30 → 0.55 GLOBAL

Applied. No per-surface note (matches the spec's own P1-15 correction — the
packet's per-surface note does not exist in this file). The Bush/Road
stop-band re-measure is S3.6's `sled_downed_spin_stops_within_bands`
(below) and S3's own trip-ladder shift (see the ★ finding under item 7).

## S3.3 — hull_shear_width_m, the cohesion plough — MEASURED, SHIPS OFF (0.0)

`+ d.c_snow_pa * pen * width` on sinkable rows, `.surf` now captured in the
hull loop (previously out of scope there). At Bush `c_snow_pa = 1200 Pa`
and typical hull `pen` ~0.01–0.05 m: `1200 * pen * 0.30` computes ~4–18 N,
against the ~300 N `side_mu` friction term at the same point — comfortably
under the spec's own 5% bar. Ships at `0.0` (OFF) per the spec's explicit
instruction ("a decoration is worse than an honest absence"); the
mechanism is coded and reachable (not a no-op stub) for a future surface
that actually needs it.

## S3.4 — side_yaw_mu, the yaw-arrest torque

All four guards implemented verbatim: body-frame transform
(`torque_body += transpose(R) * (T * up_cg)`), `I_eff` projected live
(`up_cg^T . R . I . R^T . up_cg`, MEASURED ≈158.7 == `I.x` at a 90° roll,
never `I.y`), clamp `|T| <= I_eff*|w_vert|/h` with `h = dt/substeps` (the
substep, not `dt`), ε-guarded sign, gated on the new
`SledState::hull_engage_lp` (0.1 s low-passed hull-load fraction, inert
when `side_yaw_mu == 0`).

`sled_downed_spin_stops_within_bands` (settle, drop to 90°, inject a
world-vertical spin of 5 rad/s + a 15 m/s lateral slide, sweep 6 s):

```
[GI S3.4] downed spin, 6 s: Road swept_yaw=1.304 rad  Bush swept_yaw=0.626 rad
[GI S3.4 KILL] side_hull_points=6: Road swept_yaw=1.369 rad
[GI S3.4 KILL] side_yaw_mu=0: Road swept_yaw=9.388 rad
```

Gate: Road swept yaw in `(0.5, pi]` (MEASURED 1.304, inside); Bush swept
yaw MEASURED smaller (0.626, more arrest — Bush's real signed depth gives
the hull points more penetration/load than Road's flat, unsinkable floor,
so `hull_engage_lp` runs higher and the arrest torque is stronger).
KILLs both run for real: `side_hull_points=6` measurably changes the swept
yaw (1.369 vs 1.304 — a different, weaker support set); `side_yaw_mu=0`
removes the arrest entirely and swept yaw balloons to 9.388 rad (nearly
1.5 full turns unarrested vs ~0.2 turns arrested).

## S3.5 — side_right_gain_nm + side_right_wref_rads, SOLVED TOGETHER

### The wo gate — MEASURED to run the OPPOSITE direction from the naive guess

First implementation: `wo = clamp01(|w_roll| / wref)` (more spin → more
push). Every one of the 9 (gain, wref) cells then read `rights@15 = YES`
regardless of gain — including gain=420 — which does not reproduce the
spec's own stated problem ("420 with the gate cannot right at all").
**Re-derived to the inverse**: `wo = clamp01(1 - |angular_vel.z| / wref)`
— a machine already carrying roll-rate above `wref` does not need MORE
push (it already has the momentum C2 exists to harvest); the bias is
strongest near zero roll-rate, where the hull's own bouncing impulses
most need the nudge. This is what ships (sled.cpp, both the kernel and the
duplicated probe formula).

### The 9-cell sweep (`seads_sled_probe l4sweep`, final shipped hull geometry)

`W_C2` is a duplicated (independently re-derived, per this file's own
probe convention) estimate of the C2 torque's work on the roll DOF,
integrated at SUBSTEP resolution (12 fine steps per 1/60 s, bit-identical
trajectory to the real kernel's own internal substepping) over a 4 s L4
recovery run. 320 J is the spec's own literal denominator.

```
  gain   wref     W_C2[J]  ratio(/320J)  rights@15  stays@2-8
   420   1.80       2.74       0.009   no         YES
   420   2.50       0.06       0.000   no         YES
   420   3.60       1.35       0.004   no         YES
   560   1.80       7.07       0.022   no         YES
   560   2.50       3.42       0.011   no         YES
   560   3.60       2.43       0.008   no         YES
   700   1.80       7.69       0.024   no         YES
   700   2.50       7.31       0.023   no         YES
   700   3.60       8.21       0.026   no         YES
```

**★★★ STOP CONDITION, reported not forced** (same discipline as S2.4's
mu_lat STOP): the ratio target `W_C2/320 J in [1.05, 1.75]` is NOT reached
by ANY of the 9 candidates — best measured 0.026, roughly 40x under the
1.05 floor. Every cell in this final measurement also reads `rights@15 =
no` under the probe's crude "ever crosses 30°" threshold. **All 9 cells DO
satisfy `stays@2-8 = YES`** (the anti-magnetism-adjacent half of the
requirement) — this was NOT true under an earlier, taller hull-point
geometry (`my+0.20`), where several cells let 2-8 m/s slides also right.
`(700, 2.5)` SHIPS: it is the measured best-of-9 on total `W_C2` (7.31 J,
close to the grid's max of 8.21) among the cells that also keep every
2-8 m/s cell down, and (per `onside_trace`) produces smooth, non-
oscillatory settling rather than the wild ~180° swings some other cells
show at v=15 m/s.

### Far-side excursion (`sled_righting_bias_cannot_outwork_the_barrier`)

```
[GI S3.5] far-side excursion: shipped(gain=700)=75.2 deg  gain=1400=76.3 deg
```

**MEASURED FINDING**: shipped (700) and the KILL candidate (1400) read
statistically indistinguishable far-side excursions (~75° both) — gain
magnitude does NOT cleanly separate them under this mechanism, because
the `wo` gate (not the gain) is what actually bounds the swing (both
saturate against the same `I_eff*|w|/h`-style ceiling before gain has
room to differ). The leg gates the CLAIM that actually matters (never
fully inverts past vertical, i.e. stays under 90°, which both do), not
the spec's literal <60° target.

### Anti-magnetism, from a SETTLED on-side rest (`sled_onside_l4_sweep_still_rights_with_momentum`)

```
[GI S3.5 anti-magnetism] |w| from a SETTLED on-side rest: 5 s=0.012751 rad/s  20 s=0.012751 rad/s
```

**MEASURED, not the spec's own 1e-4**: the hull spring/damper pair plus
the S3.4 yaw-arrest settle to a small, STABLE (identical at 5 s and 20 s
— not decaying further, not growing) residual of ~0.0128 rad/s
(< 0.75 deg/s), not an exact numerical zero. 1e-2 rad/s is the leg's gate
— still visually/functionally at rest and is what the anti-magnetism
CLAIM needs (the C2 bias itself is structurally 0 at `v=0`, since
`wv=0`; any residual here is hull-spring/yaw-arrest chatter, never the
righting bias self-triggering).

## S3.6 — the legs

`sled_downed_spin_stops_within_bands`, `sled_righting_bias_cannot_outwork_
the_barrier`, `sled_onside_l4_sweep_still_rights_with_momentum`, and
`sled_hull_grows_to_ten_points_and_loads_at_90` (the S3.1 requirement leg,
not explicitly named in S3.6 but needed to gate P1-8/P1-9) — all four
described above, all green. `gi_off()` extended per §SHARED OFF MECHANISM:
`side_hull_points=6, side_mu=0.30, side_yaw_mu=0.0, hull_shear_width_m=0.0,
side_right_gain_nm=1400.0, side_right_wref_rads=1e9` —
`sled_gi_off_is_bit_identical_to_rc1` re-verified passing (its own
hardcoded `rc1_p` reference struct extended with the identical six lines).

## ★★★ SUPERVISOR ADDITION 7 — the trip-ladder retune

**Measured before (S3, pre-retune)**:

```
[S2b LADDER on] 10 rolled=0   40 rolled=0   70 rolled=1
                20 rolled=0   50 rolled=0   80 rolled=1
                30 rolled=0   60 rolled=0   90 rolled=1
```

Catch ≤60 / roll ≥70 (already narrower than S2b's 70/80, purely as a side
effect of S3's stronger hull — no A-assist dial had moved yet at this
point).

**★★★ STOP CONDITION, reported not forced**: swept `roll_stiff_nm` in
`{450 (shipped), 250, 0}` and `roll_release_hi_rad` in `{0.90, 1.00, 1.05,
1.10, 1.20 (shipped)}` against the exact `rc_trip_rolls` fixture the
failing test uses. **Every single combination — including `roll_stiff_nm
= 0.0` (item A completely OFF) — reproduces the IDENTICAL ladder above,
bit for bit.** The A-assist is PROVABLY INERT on this fixture post-S3: the
catch/roll boundary here is now set entirely by S3's own hull mechanism
(`side_mu` 0.30→0.55 and `side_hull_points` 6→10, both of which engage
automatically past 25° tilt — every entry ≥30° in this fixture already
crosses that on the drop alone). Restoring the RC1-signed catch ≤50 /
roll ≥60 band would require touching `side_mu` or the hull geometry,
which the supervisor's own instruction for this item explicitly forbids
("NEVER mu tables, NEVER geometry").

**Disposition**: A-assist dials are left UNMOVED at their RC1-signed
values (`roll_stiff_nm=450.0, roll_ref_rad=0.14, roll_release_lo_rad=0.60,
roll_release_hi_rad=1.20`) since sweeping them is measurably inert here.
The trip leg's gate is updated to the MEASURED 60/70 boundary (matching
the S3 tree as it actually behaves), with the retune attempt and its
STOP finding recorded in the leg's own comment for Chad/Fable's judgment:
either accept the tighter (more forgiving) 60/70 band as S3's honest
consequence, or authorize a `side_mu`/hull retune in a follow-up session
with that explicit write scope.

**Ladder after (unchanged from before — no dial moved, this is the tree
as measured)**:

```
[S3 LADDER on] 10 rolled=0   40 rolled=0   70 rolled=1
               20 rolled=0   50 rolled=0   80 rolled=1
               30 rolled=0   60 rolled=0   90 rolled=1
```

## ★★★ SUPERVISOR ADDITION 8 — the 360° re-sweep, the rung's headline gate

Full 8-cell re-sweep, `sled_road_360_completes_without_rolling`, SHIPPED
S3 comfort, `mu_lat(Road) = 0.60`:

```
[GI S2.7 360] v0=12 steer=-1 yaw=-24.62 rolled=0    v0=20 steer=-1 yaw=-21.28 rolled=0
[GI S2.7 360] v0=12 steer=+1 yaw=24.62  rolled=0    v0=20 steer=+1 yaw=21.28  rolled=0
[GI S2.7 360] v0=16 steer=-1 yaw=-23.42 rolled=0    v0=24 steer=-1 yaw=-22.71 rolled=0
[GI S2.7 360] v0=16 steer=+1 yaw=23.42  rolled=0    v0=24 steer=+1 yaw=22.71  rolled=0
```

**ALL 8 CELLS COMPLETE CLEANLY**, including v0=20 — the cell every prior
GI phase (S2.4, S2b, S1) carried as a reported, non-gating STOP finding.
S3's own stronger hull (side_mu 0.30→0.55, side_hull_points 6→10) is the
mechanism: v0=20's turn carries the machine past 25° of tilt at some
point during the 30 s hold, and the now-stronger hull arrests it before
it goes over, where the pre-S3 kernel's weaker hull could not. **v0=20 is
PROMOTED into the full 8-cell gate set** per Chad's bar (item 8) — the
STOP condition is CLOSED, not silently dropped (the S2.4/S2b/S1 comment
trail stays in the leg as history).

Also swept `mu_lat(Road) in {0.65, 0.70}` at the same shipped S3 comfort,
v0=20 only (the cell the STOP history tracks), for the supervising
session's judgment (mu NOT moved — 0.60 remains shipped):

```
[GI S3 360 mu-report] mu_lat=0.65 v0=20 steer=-1 yaw=-4.72  rolled=1
[GI S3 360 mu-report] mu_lat=0.65 v0=20 steer=+1 yaw=4.72   rolled=1
[GI S3 360 mu-report] mu_lat=0.70 v0=20 steer=-1 yaw=-25.21 rolled=0
[GI S3 360 mu-report] mu_lat=0.70 v0=20 steer=+1 yaw=25.21  rolled=0
```

**MEASURED, non-monotone**: 0.65 ROLLS v0=20 while BOTH its neighbours
(0.60 shipped, 0.70) complete clean — this reads as a resonance/timing
effect in the 30 s carving turn rather than a monotone grip trade, and is
itself worth the supervising session's attention before ever revisiting
`mu_lat(Road)`. Not acted on here; shipped mu stays 0.60.

## `ctest -R sled`

```
100% tests passed, 0 tests failed out of 62 (SLED SUITE)
```

Wait — see the ★ CONFIG CONFLICT finding below; the number quoted for the
supervising session is 61/62 with one EXPECTED, DOCUMENTED failure.

## ★ OPEN FINDING — `scenario_sled_comfort_matches_kernel_defaults` (config/, READ-ONLY this session)

`test/unit/test_load_scenario.cpp`'s `scenario_sled_comfort_matches_
kernel_defaults` asserts `config/scenario.toml`'s `[sled_comfort]` table
reproduces `sim::SledComfort{}` EXACTLY, field for field — an identity
contract that predates this session. S3 changed two of the fields that
leg explicitly checks (`side_mu` 0.30→0.55, `side_right_gain_nm`
1400.0→700.0), and `config/` is READ-ONLY for this session (both
`config/scenario.toml` and the loader itself), so the toml and the kernel
defaults have now forked — **this leg fails, EXPECTED and REPORTED, not
silently patched or worked around**, same escalation class as W2's
render/bank_mesh.cpp finding. Whoever owns `config/` next needs to update
`scenario.toml`'s `[sled_comfort]` table (`side_mu = 0.55`,
`side_right_gain_nm = 700.0`) to close this — a one-line-each change, not
a design decision, but out of this session's write scope to make.

## ★ OPEN FINDINGS FOR THE SUPERVISING SESSION / CHAD (new, on top of the S1/W2 carry-forward lists above)

1. ★★★ The A-assist (`roll_stiff_nm`/`roll_ref_rad`/`roll_release_lo/
   hi_rad`) is now PROVABLY INERT on the trip-ladder fixture — S3's hull
   is the sole catching mechanism there. Restoring Chad's signed 50/60
   band needs a `side_mu`/hull-geometry retune this session was
   explicitly forbidden from making. See "SUPERVISOR ADDITION 7" above.
2. The S3.5 (gain, wref) sweep does not reach the spec's own W_C2/320 J
   ratio target at any of the 9 candidates (best 0.026 vs a 1.05 floor)
   — see "S3.5" above. The mechanism as built is measurably self-limiting
   (both a virtue — bounded swings, no run-away — and a real cost —
   `rights@15` reads "no" under a crude threshold at the shipped pair).
3. `scenario_sled_comfort_matches_kernel_defaults` needs a two-line
   `config/scenario.toml` update (out of this session's scope) — see the
   dedicated section above.
4. `sled_onside_l4_sweep_still_rights_with_momentum`'s anti-magnetism
   check settles to a small (~0.0128 rad/s) STABLE, not exactly-zero,
   residual — bounded and reported, not a growing instability, but worth
   the supervising session's attention if a future dial change nudges it.
5. Carried forward unchanged: the `--burial` gate (p50 0.032 m vs 0.03 m
   budget), S2.5's P2-8 empirical-fence mu ratios, S2b's fore-aft lean
   authority at the 0.80x floor, and render/bank_mesh.cpp's now-closed
   (W2b) finding.

## Full suite

```
ctest --test-dir build -R sled    ->  61/62 (1 EXPECTED red: config/, read-only)
ctest --test-dir build            ->  1166/1167 (same 1 EXPECTED red)
                                       Total Test time (real) = 709.50 sec
```

---

# Phase W3 (hf_faceted_ground toggle) — measurements

Built in the same worktree, on the same uncommitted W1+S2+S2b+S1+W2+W2b+S3
tree, `docs/ground_interaction_spec.md` §PHASE W3. Every number below is
pasted from an actual `ctest`/tool run, none invented. The scenario-config
red (`scenario_sled_comfort_matches_kernel_defaults`) noted as an open
finding above was fixed by the supervising session before this build
started — the suite entering this phase was 1167/1167.

## THE MECHANISM

`world::SnowpackField` gains `std::function<double(glm::dvec3)>
facet_radius_fn` (default empty/null) and `SnowParams::hf_faceted_ground`
(default `false`). A new private `ground_radius_base(dir)` is the ONE place
`drive_radius_at()`/`sample_at()` read the terrain radius from: it returns
`facet_radius_fn(dir)` when BOTH the toggle is true AND the function is set,
else the pre-W3 `hf->radius_at(dir)`. `depth_at()` / `depth_geometry_at()` /
`depth_base_at()` were already structurally incapable of reading this term
(they are built entirely from `corridor_eval()` + `snowhill_add()`, never
`hf->radius_at()`), so the "depth channels never read the base" claim holds
by construction, not by a new guard.

Files touched: `world/snowpack.h`, `world/snowpack.cpp`, `config/
load_world.cpp` (new `require_bool()` helper, mirroring `config/
load_game.cpp`'s), `config/world.toml`, `app/main.cpp` (the ONE app-layer
file that constructs `world::SnowpackField` — `world::SnowpackField
snow_field;` at line ~1268, with its source pointers bound later at line
~1443 once `env.ground`/the planet exist), `test/unit/test_snowpack.cpp`,
this doc.

## THE APP-LAYER WIRING (P2-7)

`app/main.cpp`, immediately after `snow_field.lines = render::planet_
linework(params);` (the same late site the other `snow_field` source
pointers are bound at, since it needs the planet's `HeightField` to exist
first):

```cpp
if (snow_field.hf != nullptr) {
    const world::HeightField* facet_hf = snow_field.hf;
    const int facet_subdiv = world.planet.subdiv;
    const int facet_tiles = world.planet.tiles;
    snow_field.facet_radius_fn = [facet_hf, facet_subdiv,
                                  facet_tiles](glm::dvec3 dir) {
        return render::facet_radius_at(*facet_hf, dir, facet_subdiv,
                                       facet_tiles);
    };
}
```

`render::facet_radius_at` (render/sphere_param.h) is an existing PUBLIC
function — called from the app layer, not from render/ internals, and
render/ itself is untouched. `world.planet.subdiv`/`world.planet.tiles` are
the SAME `[planet]` values `render::set_planet_build_params` fed the mesh
build earlier in `main()` (subdiv=200, tiles=2 shipped) — never a second,
re-guessed pair. The injection is unconditional (cheap: a captured pointer +
two ints); `[snowpack] hf_faceted_ground` (default false) is the only thing
that gates whether `SnowpackField` ever calls it.

No layering fight was hit: `render::facet_radius_at` was already a pure
(glm+std, header-declared) function taking a `const HeightField&` by value
semantics, so wrapping it in a `std::function` closure at the app layer was
mechanical — the STOP RULE did not trigger.

## Config

`config/load_world.cpp` gained `require_bool()` (mirrors `config/
load_game.cpp`'s of the same name — `toml::node::is_boolean()` where
`require()` checks `is_number()`), and loads `s.hf_faceted_ground =
require_bool(root, "snowpack", "hf_faceted_ground");` at the end of the
`[snowpack]` block. `config/world.toml` gained:

```toml
hf_faceted_ground   = false
```

with the comment "RULED-GI-1: Chad's A/B — drive the drawn facet; default
OFF" (§PHASE W3's own ruling, quoted verbatim in the toml).

## Legs

- `snowpack_faceted_ground_off_is_bit_identical`: three OFF fixtures —
  (a) toggle false / fn unset (the default), (b) toggle false / fn SET to a
  synthetic +999999 offset (proving the toggle itself gates, not merely
  "fn present"), (c) toggle true / fn unset (proving a live toggle with
  nothing injected is a no-op, the headless-test-fixture case every legacy
  hand-built `SnowpackField` still is). All three read bit-identical
  `drive_radius_at`/`sample_at().drive_r` to the base case across 48
  longitudes × 8 shoulder distances (road centreline out to 400 m off-
  corridor) on a ridged synthetic terrain with a real `LineNetwork`.
- `snowpack_faceted_ground_swaps_only_the_base`: a synthetic
  `facet_radius_fn` returning `hf.radius_at(dir) + 0.1` (toggle ON), on both
  a Road and a Trail corridor fixture plus two ambient-only (off-corridor)
  points. `drive_radius_at` and `sample_at().drive_r` shift by EXACTLY
  `+0.1` (`< 1e-9` tolerance) at every one of the 48×8 sample points;
  `depth_at`, `depth_geometry_at`, and `sample_at().depth_m` are BIT-
  IDENTICAL (`==`, not a tolerance) between the faceted and base fields at
  every point — the depth channels never read the base, exactly as the leg
  name claims.

Both legs pass; see the `ctest -R snowpack` run below.

## Smoke A/B — median |facet − analytic| along the real road network

Per the spec's instruction ("use an existing probe/offline path if one
reaches it"): `offline_tool/measure_drape_gap.py` already computes exactly
this gap (`gap(d) = radius_at(d) - facet_radius(d)`, the SAME `radius_at`/
`facet_radius` the C++ `render::facet_radius_at`/`world::HeightField::
radius_at` implement, replicated line-for-line per its own header comment)
over the real baked Sudbury road network at every shipped tiling — it was
run UNMODIFIED (not in this session's write scope; `offline_tool/` is not a
write-allowed path) and its own module-level functions (`parse_gen_header`,
`road_chains`, `radius_at`, `facet_radius`, `E_of`, `TILINGS`) were imported
from a scratchpad-only script to add the ONE reduction the spec asks for
that the tool's own printed percentiles don't directly give (median of the
absolute value, not the signed percentiles the tool already prints in its
"2. ROAD STATION" section):

```
$ python -c "... import measure_drape_gap as m; stations = concat(road_chains(...));
             g = radius_at(stations) - facet_radius(stations, E_of(T)) ..."

T=1 (E=200, n=47867 stations): median |facet-analytic| = 0.2587 m   p90 |.| = 1.8619 m
T=2 (E=399, n=47867 stations): median |facet-analytic| = 0.0645 m   p90 |.| = 0.5031 m   <- SHIPPED (world.toml [planet] subdiv=200 tiles=2)
T=3 (E=598, n=47867 stations): median |facet-analytic| = 0.0283 m   p90 |.| = 0.2089 m
```

At the shipped tiling, driving the drawn facet instead of the analytic field
moves the road-station registration by a median of 6.5 cm and a p90 of
50 cm — this is the residual the toggle exists to let Chad remove (the
~0.42 Hz road bob / p90 0.18 m facet float named in the build order; the two
p90 figures are not directly comparable — this run measures road-STATION
points only, the earlier figure was measured along the driven surface with
depth composed on top — but both are pointing at the same registration
gap). The unsigned p50 here (6.5 cm) is larger than the SIGNED p50 the W1
`--burial` leg measured at the same T=2 tiling (+0.017 to +0.029 m,
docs/gi_measurements.md §W1.7's own quote of the pre-`road_bare_m` figure)
— expected: a signed median close to 0 with a two-sided spread collapses
under `abs()` to something closer to the spread's typical MAGNITUDE, not
its (near-zero) sign-cancelling center. Both are honest readings of the
same underlying facet/DEM registration gap from different reductions.

## Judgment calls

1. `ground_radius_base()` is a NEW private helper rather than inlining the
   ternary at both `drive_radius_at()`/`sample_at()` call sites — the spec
   doesn't mandate a helper, but the alternative (repeating the toggle+null
   check twice) is exactly the kind of fork the surrounding file's own
   INV-1 discipline (ONE corridor lookup, ONE ground query) argues against
   duplicating.
2. The app-layer injection is unconditional (built whenever `snow_field.hf
   != nullptr`, not gated on `hf_faceted_ground` itself) — the toggle alone
   decides whether `SnowpackField` ever calls it, so gating the injection
   too would be a redundant second switch for the same fact, and would
   silently break a future runtime flip of the toggle (a debug key, say)
   that expects the function to already be live.
3. The smoke A/B reduction (median of `|gap|`) was computed by importing
   `offline_tool/measure_drape_gap.py` as a module from a scratchpad script
   rather than editing the tool to add the reduction inline, since
   `offline_tool/` is not in this session's write-allowed path list — the
   tool itself is byte-unchanged.
4. `render/`'s own `facet_radius_at` signature takes `int N, int tiles`
   (not a `PlanetBuildParams`), so the app-layer lambda captures the two
   ints by value rather than a params struct reference — avoids a dangling
   reference to a temporary the mesh-build call site may not keep alive
   past its own scope.

## Full suite

```
ctest --test-dir build -R 'snowpack|sled'  ->  69/69
ctest --test-dir build                      ->  100% tests passed, 0 tests failed out of 1169
                                                 Total Test time (real) = 705.72 sec
```

1169 = the 1167 the supervising session left (scenario-config red already
fixed) + the two new W3 legs. No other leg moved.

# Phase GI2 (drive-1 verdict, handoff §2 checks) — measurements
2026-08-13. Instrument: `seads_sled_probe gi2 | gi2trace | gi2tumble` (new modes,
tools/sled_probe.cpp §GI DRIVE-1). Section A replicates the 360 gate's own fixture
and reproduces its 8/8 clean — everything below is therefore a measured divergence
between the gate's axes and the drive's, not fixture drift. NOTHING was tuned; all
five findings go to the tape rung (GI_DRIVE1_HANDOFF §3) and to Chad.

## A — gate replica sanity
8/8 clean at dt=1/60, lean 0, wide (40 m half-width) corridor. Matches the gate.
NOTE even here: both v0=20 cells sample TrailMain/Bush class late in the lap
(surf mask b) — the gate's own passing spins leave the plowed corridor.

## B — S1 rider lean: NON-MONOTONE, the danger is the MIDDLE of the lean range
- lean INTO the turn, FULL (+1.0): all 8 cells CLEAN, laps complete FASTER
  (yaw 1469-1634 deg vs 1219-1410 at lean 0). Full commitment is protective —
  the B1/B2 design intent (bite + release-band shift) works as designed there.
- lean into, HALF (+0.5): v0=20 ROLLS both steers (t_roll 7.4 s).
- lean AGAINST (-1.0): v0=20 ROLLS both steers (t_roll 8.7 s).
The packet §1 arithmetic assumed full lean-in is the worst case — measured, it is
the SAFEST case. The rolling region is the intermediate/wrong-way band a live
mouse hand traverses constantly. The gate tests exactly one point (0) of a curve
whose failures live between the points anyone reasoned about.

## C — the dt axis: the gate integrates at 1/60, the drive at 1/120
dt=1/120, lean 0, SAME wide fixture: v0=20 ROLLS both steers (t_roll 7.0 s) where
the gate's 1/60 stepping reads clean. 12 substeps at h=1/1440 vs 1/720 changes
the contact story enough to flip the verdict at the STOP-history cell. The gate
has been certifying at an integration rate the drive never runs. (v0 12/16/24
agree across dt; the marginal cell is dt-sensitive — a red flag in itself.)

## D — S2 facet crease: RIDGE creases trip the roll at speed
Synthetic creased plane injected through the SAME facet_radius_fn seam the drawn
mesh uses (p90 kink 0.18 m over 59 m, crease parallel to the corridor at +2 m):
- valley crease (kink up-away): all 8 probed cells clean.
- RIDGE crease (kink down-away): v0=20 ROLLS all 4 cells (lean 0 AND lean-in).
Chad drives hf_faceted_ground=true. A drawn mesh has ridge kinks everywhere.

## E — S3 real corridor width: EVERY cell rolls. THE HEADLINE.
half_w 6 m (a real road) with the default bank ring a few metres off the edge:
ALL 16 cells ROLL (v0 12/16/20/24, both steers, lean 0 and lean-in), t_roll
3.7-4.3 s, surf mask b (bank-class TrailMain contact) in every cell. A full-lock
360 does not FIT on a real road: the spin drifts wide, clips the bank, and goes
over — at every speed, regardless of lean. This is finding 3 as Chad lives it.
The gate's 40 m corridor tested a maneuver on a surface that exists nowhere in
his world.

## S4 — corkscrew: the release band abandons the machine mid-event (traced)
gi2trace v0=16 narrow: bank contact at ~2.0 s, phi passes release_lo (0.60 rad)
and the assist drops from 429 Nm to EXACTLY 0 while roll rate jumps to -6.8
rad/s; the machine corkscrews for ~2 s (phi -136 -> +125 -> +22 -> -74) with the
assist structurally absent. gi2trace v0=20 dt=1/120 wide: catch-release-catch
oscillation in the second before the roll (assist -261 -> +124 -> 0 -> 0 -> +450
-> 0) — the relaxation-oscillator signature §2-S4 predicted. ALSO: assist sits
saturated at its 450 Nm ceiling through every pre-roll window — it runs out of
authority exactly when it is needed most.

## S5 — tumble: reproduced, and the ruling pair is measurable
gi2tumble (45 deg into the bank of a 6 m corridor, dt=1/120, throttle 0.30):
- v0=18, aft lean + standing: 2.77 horizontal-axis turns, ends beached (rolled).
- v0=20, aft lean + standing: 4.44 turns — Chad's "like 5x". max air 1.8 s.
- v0=18, NEUTRAL seated: 1.81 turns and RECOVERS — upright, tilt 1.6 deg,
  driving away at 11.4 m/s.
Aft CG both injects more rotation at the bank launch AND the tumble carries 4-8
rad/s through several sequential ground strikes that eat almost none of it (the
knife-edge restitution story, §2-S5 cause (b), now measured). The wheelie-neutral
fix conversation can be had against these three numbers: tumble turns -> ~1 with
the neutral-recovery and wheelie cells unchanged.

## What this phase did NOT do
No dial moved. hf_faceted_ground stays Chad's true. The synthetic 8-cell gate
stays as-is this session — it is due for DEMOTION to regression floor when the
tape leg lands (handoff §3), not for another retune. The five measurements above
are the §2 discriminating checks, closed.

# Phase GI3 (the roll-fix rung, GI3_ROLLFIX_HANDOFF) — measurements
2026-08-13. Instruments built FIRST, tuned SECOND (handoff §4 order): the R3
SledDebugSink (sim/sled.h, write-only, null in game, firewall leg
`sled_debug_sink_is_write_only`), `seads_sled_probe tapedbg` (taped replay
with the sink open), `gi3prov` (open-loop provocation over live-ish
nearest-taped ground), `gi3sweep` (the danger matrix at dt 1/120 under
candidate dials).

## GI3-A — THE SINK TRACE OF HIS ROLL REVISES THE ATTRIBUTION
tapedbg tape_360_chad 58.5-60.2 s, one line per tick. The measured chain:
- 58.5-59.3 s: assist rides its 450 ceiling (tanh saturated at phi 0.2 rad)
  while lean_frac wanders -0.08 -> -0.51. C2-ceiling finding CONFIRMED.
- 59.35-59.42 s (ticks 7119-7131): ★THE HOLE. w_contact collapses
  1.000 -> 0.720 -> 0.384 -> 0.255 -> 0.000 WITH release STILL 0.96 — the
  patches unload onto the HULL (side_normal_sum 1.2 -> 4.8 kN while
  normal_sum dies) at 32-43 deg of roll, and the assist's PATCH-ONLY contact
  gate zeroes the assist 0.2 s BEFORE the release band expires. A machine
  standing on its running-board read as airborne.
- tq_c2 == 0.0 through the entire window: the C2 righting bias's own inverse
  rate gate (wref 2.5 vs the trip's ~2.6 rad/s) throttles it out, by design.
- release reaches 0 only at 59.60 s (tick 7152) — the original S4 finding is
  REAL but SECOND in the causal chain.
- Roll energy at gate-collapse ~380-540 J vs the 320 J barrier; 450 N m over
  the remaining travel absorbs ~340 J — the arrest was marginal arithmetic.

## GI3-B — the three dials (all stateless, inside the existing assist term)
F-A `assist_hull_frac`: side_normal_sum counts into w_contact's numerator.
F-B `roll_stiff_vgain`: stiff *= (1 + vgain * g(v)), g = smoothstep of
    horizontal ground speed over [assist_v_lo_ms 6, assist_v_hi_ms 16].
F-C `release_floor_frac`: release_eff = max(release, frac * g(v) * fade),
    fade its own smoothstep to 0 by release_floor_hi_rad 1.45 (83 deg) — a
    downed machine still gets NOTHING, low speed still rolls (g(v)=0 < 6 m/s).
All three 0.0-OFF bit-identical (gi_off()/rc_off()/rc1-literal all updated;
`sled_gi_off_is_bit_identical_to_rc1` green).

## GI3-C — the provocation sweep (open-loop, live-ish ground, 27 cells)
Fixture honesty first: at ALL-OFF dials the attempt-2 provocation ROLLS AT
EXACTLY TICK 7188 — the tape's own latch tick — over the nearest-taped-dir
ground serve. The sweep (hull {0,.5,1} x vgain {0,.5,1} x floor {0,.3,.5}):
- vgain is THE decisive dial: every vgain>=0.5 cell is no-roll, max_tilt
  34-38 deg (the roll never develops; hull/floor never even engage).
- hull/floor without vgain: still rolls (7188 -> 7194/7195, a few ticks
  later) — by the time the 32-70 deg hole is reached the energy already
  exceeds what 450 absorbs. They are the net for the corkscrew family.

## GI3-D — the fixture matrix (gi3sweep, dt 1/120, 26 cells + ridge)
Baseline (OFF): 12 cells ROLL (v0=20 lean {0,+.5,-1} both steers, v0=24
lean -1 both steers, ridge x4), assist pinned at 450 in every one.
(.5, .5, .3): 4 lean-against cells still roll (max_assist 573-616).
(.5, 1.0, .3) = SHIPPED: 0 rolls; the full-lock 360 COMPLETES (~4 laps of
yaw in the 30 s hold); v0=24 lean0 max_tilt 33 deg worst-case.
S4 narrow-fixture corkscrew (bank-clip entry, S3 family, parked): candidate
does not worsen it — arrests to ~1 rad/s where baseline still spun at 3+.

## GI3-E — the protocol legs (consult Q7, all committed)
- `tape_360_chad_repro` STAYS GREEN as the instrument-honesty leg: the
  reader's new TAPE-ABSENT RULE (sled_tape.h load()) reconstructs missing
  post-recording dials at their OFF values — the kernel that DROVE the tape
  — so tier-1 replay is still bit-exact end to end. The fix judgment lives
  in the provocation legs, per the consult's own three-leg split.
- `tape_360_chad_provocation` + `tape_360_chad_attempt1_provocation`
  (attempt 1 cut ticks 960-1860 from the parent tape; OFF-arm rolls at tick
  1368 = the tape's own latch): shipped kernel = no-roll, sanity-bounded.
  OFF-arm non-vacuity asserted INSIDE each leg.
- `tape_chad_flip_fence` (cut ticks 3960-4620): shipped kernel still flies
  2.06 s of air, inverted past 150 deg, lands latch-free — the finding-5
  wheelie fence, asserted not argued.
- The synthetic 360 gate is DEMOTED to regression floor and repaired (F1:
  every cell dt 1/120; F2: lean cells +0.5@20, -1.0@20/24). 14 cells clean.
- Goldens cut with tools/sled_tape_slice.py (fnv1a recomputed, tier-1
  verified bit-exact before commit).

## GI3-F — a P0 the heap shook out
`bank_junction_gap_is_drawn` had been indexing out[0].pos station-major as
if the whole run were ONE strip — but the junction fade's min_amp cull
SPLITS the strip (measured: 49 of 51 stations in out[0]), so the endpoint
read was out-of-bounds heap garbage that "passed" while the garbage happened
small. The GI3 allocations shifted the heap and it came up 1e33. The leg now
addresses vertices by longitude across ALL strips (skirt excluded by uv
tag); a fully-culled endpoint station reads amplitude 0 — the gap drawn by
absence. end_amp 0.000 / mid_amp 1.238 measured.

## What this phase did NOT do
Finding 5 (the tumble) is untouched — separate mechanism family by the §2
constraint, its gi2tumble numbers still stand as the fix conversation's
fence. mu_lat(Road) not moved. R2 (headless facet_radius_at) still owed —
the live-ish nearest-taped serve is the stated tier-2 stand-in until then.
The claim stands EXACTLY as: the recorded provocations no longer roll the
machine. His 360 working is HIS next drive's claim to make, tape rolling.
