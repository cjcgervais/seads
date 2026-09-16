# GROUND-INTERACTION BUILD SPEC v2 (rung GI, branch sandbox/winter-GI, base 3b89a61cd)
Authority: vehicle_program/RESEARCH_PACKET_C_GROUND_INTERACTION.md + the SPEC RED-TEAM of
2026-08-12 (verdict UNSAFE on v1; every P0/P1 folded here — where this spec and the packet
disagree, THIS FILE wins and §CORRECTIONS says why). Chad's RULED-GI-1 stands: honest arm ON,
rail 0.19, faceted toggle default OFF, mu_lat(Road) tuned in [0.55, 0.70].
Phase order (P1-11/P1-13): W1 → S2 → S1 → W2 → S3 → W3. Sonnet implements per phase → Fable
reviews diff → legs run. No subagent commits. All TEST_CASE names ASCII snake_case.

## §CORRECTIONS TO THE PACKET (red-team P0s — the packet stays as history, this is canon)
1. **BankBuildParams::lift_m STAYS 0.45** (packet §2 G2 row 2 is WRONG): the bank strip draws
   at facet + lift + depth_at (bank_mesh.cpp:167) and depth_at is unchanged by this build, so
   once physics carries the lift the strip ALREADY matches. Zeroing it would cut a 0.43 m
   terrace at every road edge. Keep it single-sourced from [ribbons] lift_m.
2. **The tip fence formula in packet §2 G4/§4 was wrong**: a_tip = g·arm/h_lat where h_lat is
   the load-weighted LATERAL-force application height (≈0.5317 m at frac=1 static), NOT
   cg_height − susp_rest (0.354, valid only at frac=0). At rail 0.19 + frac=1:
   a_tip ≈ 0.541 g — so the packet's "≤0.90×a_tip incl. lean-bite" fence is unsatisfiable in
   the ruled mu band (track_lat_mu 0.70 alone exceeds the budget). THE FENCE IS THEREFORE
   EMPIRICAL, not analytic — see S2. The analytic saturation ceiling is never simultaneously
   realised (tanh slip saturation + load transfer), which is why the shipped leg
   `sled_grip_ceiling_stays_below_the_tip_threshold` measures peak a_lat instead of asserting
   the table. We extend that instrument; we do not add a parallel arithmetic leg.
3. **plane_fit_load_weight was dropped in v1 — restored** (packet carried it MEASURED): S2.6.
4. **The Vermilion burial-distribution leg was dropped in v1 — restored**: W1.7.

## PHASE W1 — road deck registration. Files: world/snowpack.h/.cpp, config/load_world.cpp, tests. render/bank_mesh.h is NOT touched (correction 1).
1. Refactor FIRST (P1-3, one refactor serves W1+W2): private
   `CorridorEval corridor_eval(dir)` inside SnowpackField returning
   {reported_depth_term, geometry_depth_term, lift_m} from ONE lines->nearest hit;
   depth_base_at / depth_at / sample_at / drive_radius_at all consume it. No second nearest()
   call anywhere (snowpack.h:199-204 forbids the double cost).
2. `SnowParams::deck_lift_m = 0.0` default; single-sourced in load_world.cpp
   `s.deck_lift_m = w.ribbons.lift_m;` + check() mirror. ⚠ Mirror ONLY the load_world
   single-source pattern of ice_lift_m — NOT its application site: ice_lift_m enters reported
   depth (harmless, LakeIce unsinkable); deck_lift_m must enter the GEOMETRY/RADIUS term only,
   because TrailMain/Tributary ARE sinkable and the depth route retunes the signed kernel.
   Write that sentence as the header comment (P1-4).
3. Lift EXTENT follows the DRAWN envelope, per corridor class (P0-3):
   - plowed roads: lift constant across the roadway AND the bank ring set (out to
     rise+fall = 9 m), then ramp to 0 across the skirt — reuse/mirror the
     render::bank_ring_offsets knots so drawn and driven fall together;
   - groomed trails (no bank strip drawn): short feather at the ribbon edge over
     corridor_edge_m.
4. INV-1 statement update (P1-5): extend the test at test_snowpack.cpp:347 with a corridor
   fixture asserting the BOUNDED intended divergence (drive − (radius+depth) == lift on the
   roadway; == lift on the bank per W2 later), so drift is pinned, not blind.
5. Tests: `snowpack_road_deck_registration_headless` (drive − radius == deck_lift + road_bare
   centreline; trail variant == deck_lift + trail_pack; shoulder samples at e = 3, 6, 9, 12 m
   assert the W1.3 envelope — constant, constant, ramp, 0);
   `snowpack_deck_lift_off_is_bit_identical` (deck_lift 0.0 → grid bit-identical);
   `sled_road_sinkage_is_exactly_zero` (sweep {dwell 60 s, 5..35 m/s}: sink==0 && creep==0
   exactly && the plow `loose` term == 0 on Road).
6. `network_burial_distribution` (P0-6, the Vermilion leg): extend the offline drape-gap tool
   with a --burial mode over the baked road stations + midsegments; gate signed p50 |·| ≤
   0.03 m; record the FULL two-sided distribution in docs/gi_measurements.md. If the offline
   tool can't run in this tree (venv/bake availability), the gate becomes a documented owed
   item — do not fake it with the synthetic leg (which is true by construction and certifies
   only arithmetic).

## PHASE S2 — roll honesty (BEFORE S1: brake numbers don't survive the arm change, P1-11). Files: sim/sled.h/.cpp, tools/sled_probe.cpp, tests.
1. `SledParams::bite_at_contact_frac = 1.0` (0.0 = mount application, bit-identical).
   TANGENTIAL forces only, application point
   `g.mount + (0, -max(0.0, p.susp_rest_m - susp_x[i] + sink_m[i]) * frac, 0)` — the max()
   clamp is deliberate (past the bump stop the raw term goes negative and would raise the
   point ABOVE the mount mid-impact; P2-3). Grep every tangential add_at site; the hull-loop
   sites (:805/:809) already act at their true contact and MUST NOT move (P2-2).
   NORMAL forces stay at the mount — NAMED APPROXIMATION, comment verbatim: "vertical force
   at the mount vs the contact differs by a roll torque ≈ L·N·sin(roll) — ~219 N·m at 20°
   roll, 23% of restoring, erring STABLE; deliberately kept at the mount, same class as the
   CARRIED-OPEN note above" (P1-1).
2. `track_rail_half_m` 0.14 → 0.19 (RULED).
3. Probe/leg decoupling fix (P1-2, do FIRST or every measurement below lies):
   `three_patch_a_tip()` gains an h_lat parameter; probe_mu_check/probe_grip and
   `sled_grip_ceiling_stays_below_the_tip_threshold` set `bite_at_contact_frac = 0.0`
   alongside `cg_height_m = susp_rest_m` for roll-decoupled measurement, and the a_tip they
   compare against uses h_lat = load-weighted contact height at frac=1.
4. Friction rows: Road mu_kin 0.140 → 0.22; RockOutcrop mu_kin 0.200 → 0.28 (add RockOutcrop
   to the empirical fence sweep, P2-8). mu_lat(Road): sweep {0.55, 0.60, 0.65, 0.70} through
   the EMPIRICAL instrument (below) and the 360 leg; ship the largest value whose measured
   peak a_lat (roll-decoupled) ≤ 0.90 × measured roll-onset a_lat, inside the ruled band.
5. THE EMPIRICAL FENCE (replaces the dead analytic leg): extend
   `sled_grip_ceiling_stays_below_the_tip_threshold` to sweep Bush/TrailMain/Road/RockOutcrop
   at frac=1: measured peak a_lat (decoupled per S2.3) vs measured tip onset (full-coupled
   full-lock sweep, first cell where rolled latches) — assert peak/onset ≤ 0.90 per surface.
6. `plane_fit_load_weight = 1.0` (restored, P0-5): weight the contact-plane fit by per-patch
   normal load, falling back to up_cg when Σnormal small — kills the 23°-tilted assist
   reference beside banks (404 N·m spurious assist). 0.0 = shipped unweighted fit.
   Leg: `sled_assist_reference_plane_is_load_weighted` (scripted airborne-ski-over-bank-face
   state; assist_nm < 50 N·m; KILL: weight → 0.0 reads > 400).
7. Legs:
   - `sled_lateral_force_acts_at_the_running_surface`: single-substep torque probe; ski arm
     [0.51, 0.57] static AND the arm MOVES with susp_x (assert the 2.5 g shift, not a fixed
     track band). KILL: frac → 0.
   - `sled_road_360_completes_without_rolling`: 8 cells (V {12,16,20,24} × steer ±1),
     full-lock + full brake, COMFORT AT SHIPPED VALUES: |yaw| ≥ 2π AND rolled == false in
     every cell — THE GATE. The comfort-OFF run of the same 8 cells is a RECORDED DISTRIBUTION
     in docs/gi_measurements.md, not a gate (red-team: at honest geometry the OFF-comfort
     no-roll clause is impossible by arithmetic — if the SHIPPED-comfort gate itself fails
     empirically, STOP THE BUILD and take the mu-band-vs-360 trade to Chad; do not quietly
     move track_lat_mu (rostered) or leave the band).
   - `sled_static_tilt_table` (NEW scaffolding — no cross-slope pattern exists, P1-14): build
     an analytic tilted-plane HeightField fixture; upright at 20° for 10 s, rolled by 30°.
   - launch/flight attitude re-measure (P1-12): the thrust arm grows 0.354→~0.52 (+47%
     pitch-up moment at the cap). Record launch pitch + the OPEN-SF1-FLIGHT-ROT summit
     rotation before/after in docs/gi_measurements.md. If the launch wheelie no longer
     settles (sled.h:396-404's own criterion), flag P0 to Fable before proceeding.
8. Re-measure + record as distributions: trip ladder, carve/drift envelope (findings, not
   regressions — drive-4 numbers were measured on the wrong arm).

## PHASE S1 — brake authority (tuned AFTER S2). Files: sim/sled.h/.cpp, tests.
1. `SurfaceDials::mu_brake = 999.0` sentinel default; table = GUESS starting points, RETUNE
   against the ordering leg (P1-10 keeps the packet's label): Bush 0.66, TrailMain 0.72,
   TrailTributary 0.70, Road 0.95, LakeIce 0.24, RockOutcrop 0.70, MineWorks 0.68. Every
   braced row in the dials table gets EVERY field explicit (mixed-length aggregate trap).
2. Brake force (P1-10 — mu_brake is the TOTAL Coulomb budget, the unconditional mu_kin term
   already acts on the track): `add_at(-brake * std::min(p.brake_force_n,
   std::max(0.0, (d.mu_brake - mu_kin_eff) * normal)) * fwd_t, <S2 application point>)`.
   brake_force_n 1201 → 2600. OFF pair (mu_brake 999 + 1201) reproduces min(1201, huge) =
   flat 1201 exactly.
3. Engine-brake stacking gets a DIAL (P0-4a): `bool engine_brake_stacks = true;` →
   `if (brake < 0.05 || p.engine_brake_stacks)`. false = byte-exact old gate. (Continuity
   nicety optional; do not gold-plate.)
4. Retune the mu_brake table on the post-S2 kernel until:
   `sled_brake_ordering_road_beats_snow_beats_ice` — 15 m/s stops [LakeIce, Bush, TrailMain,
   Road]: monotonic; Road decel in [0.58, 0.75] g (the upper bound guards P1-10's 0.81 g
   overshoot — braking above any physical pavement limit is the old bug with a new face);
   Road/LakeIce ≥ 2.0.
5. Legs: the ordering leg; `sled_brake_dies_when_the_track_unloads` (crest: brake force → 0
   airborne); OFF via the SHARED gi_off helper (below), not a per-phase config.

## PHASE W2 — snowbank hardpack (AFTER S2 — the class swap makes the crest tip-capable and
must land inside the S2 fence; P1-13). Files: world/snowpack.h/.cpp, tests.
1. Bank term through corridor_eval (built in W1): geometry keeps FULL bank_profile·gap;
   reported depth caps the bank contribution at `bank_pack_skin_m = 0.065`; ONE sentinel
   (< 0) disables cap AND class branch together.
2. surface_at(): `if (is_plowed(h.kind) && bank_profile(e)*gap > bank_class_min_m) return
   TrailMain;` with bank_class_min_m = 0.065 — placed after the corridor branch (:235-242),
   before the barren check (:247) (P2-6).
3. Legs: `snowbank_crest_sinkage_bounded` (< 0.02 m; the KILL disables the sentinel PAIR —
   note it proves the pair, `snowbank_crest_class_is_trailmain` carries the class alone,
   P2-4); `snowbank_inner_face_still_launches` (airtime ±10%);
   `snowbank_junction_gap_no_leak`; crest-carve probe re-run recorded (tip-capability check
   under the S2 fence, P1-13).

## PHASE S3 — comfort C-block. Files: sim/sled.h/.cpp, tests.
1. Hull: author `const glm::dvec3 pts[10]` — the six RC1 literals FIRST, byte-unchanged, four
   new points appended; `SledComfort::side_hull_points = 10` loops the first n (6 = RC1
   structural bit-identity; P1-9). New-point constraint is NUMERIC (P1-8): |x| = 0.58 ± 0.02
   (within ~0.017 m of the ski-outer 0.5835 or they never load), z spread nose ≈ −0.9 to
   rear ≈ +0.7. Requirement: at 90° settle, ≥4 points with pen ≥ +0.01 m (pen = drive_r −
   |w|, positive = penetrating; the test asserts at SETTLED state).
2. side_mu 0.30 → 0.55 GLOBAL — and the Bush stop-band below is RE-MEASURED at 0.55, not
   assumed (P1-15; the packet's "per-surface note" does not exist).
3. `hull_shear_width_m = 0.30` cohesion plough: capture `.surf` in the hull loop (the dials
   row is NOT currently in scope there, P1-16), apply `+ d.c_snow_pa · pen · width` on
   sinkable rows, 0.0 = OFF. MEASURE its magnitude in the Bush stop test and report it —
   at Bush c = 1200 it computes ~11 N vs ~300 N friction; if it stays <5% of the friction
   term, ship it OFF (0.0) and record the finding rather than shipping a decoration.
4. `side_yaw_mu = 0.35` yaw-arrest torque, all four guards (P1-6):
   - world-vertical axis via body transform: `torque_body += transpose(R) * (T * up_cg)`;
   - I_eff = up_cgᵀ·R·I·Rᵀ·up_cg (projected live, ≈158.7 at 90° roll — never I.y);
   - clamp |T| ≤ I_eff·|ω_vert|/h where h = dt/substeps (the SUBSTEP, not dt — 12× matters);
   - ε-guarded sign; gate on a NEW named state field `SledState::hull_engage_lp` (0.1 s
     low-passed side-load fraction; inert when side_yaw_mu == 0).
5. side_right_gain_nm + side_right_wref_rads SOLVED TOGETHER (P1-7): the gate multiplies the
   gain, and recovery runs |ω_roll| ≈ 5 rad/s >> wref, so 420 with the gate cannot right at
   all. Sweep (gain, wref) ∈ {420, 560, 700} × {1.8, 2.5, 3.6} on the L4 probe; ship the pair
   where W_C2/320 J ∈ [1.05, 1.75] (NOT 0.85 — below 1.0 cannot right) AND the L4 15 m/s
   self-right lives AND 2–8 m/s stays down. The L4 leg is the binding arbiter.
6. Legs: `sled_downed_spin_stops_within_bands` (Road (0.5 rad, π]; Bush band set from the
   S3.2 re-measure); `sled_righting_bias_cannot_outwork_the_barrier` (ratio band [1.05,
   1.75], far-side excursion < 60°); `sled_onside_l4_sweep_still_rights_with_momentum`
   (+ anti-magnetism 5 s |ω| < 1e-4 — accept the P2 note that hull spring equilibrium needs a
   settled start).

## SHARED OFF MECHANISM (P0-4b/c) — build in S2, every later phase extends it
`gi_off(SledParams&)` in the test support header (extends rc_off()): frac 0, rail 0.14,
Road.mu_kin 0.140, Road.mu_lat 0.55, RockOutcrop.mu_kin 0.200, mu_brake 999 all rows,
brake_force_n 1201, engine_brake_stacks false, plane_fit_load_weight 0, side_hull_points 6,
side_mu 0.30, side_yaw_mu 0, hull_shear_width 0, side_right_gain 1400, wref 1e9, + every RC1
comfort default. ONE leg at the end: `sled_gi_off_is_bit_identical_to_rc1` (2000-tick
scripted trail run vs recorded RC1 trace). Every phase's OFF leg calls gi_off(), never a
per-phase subset (subset legs rot as later phases land).

## PHASE W3 — hf_faceted_ground toggle (RULED: build, default OFF)
The facet sampler lives in render/ and world/ is render-free BY LAW (P2-7): inject a
`std::function<double(vec3)> facet_radius_fn` into SnowpackField (set by the app layer,
null = analytic). Default OFF leg `snowpack_faceted_ground_off_is_bit_identical` + a smoke
A/B printout (median |facet − analytic| on a road line) in docs/gi_measurements.md. If the
injection turns into a layering fight, STOP and report — it is a drive experiment.

## GATE
Full ctest with printed counts; flight goldens bit-unmoved (INV-7); the sled regression
surface is test_sled.cpp's TEST_CASEs (there are NO sled golden files — do not invent them;
P1-14); no perf clause (no perf leg exists; note frame cost qualitatively if visible);
winter_plan_query check exit 0; non-ASCII TEST_CASE grep; docs/gi_measurements.md carries
every re-measured distribution (Vermilion). Fences: TrailMain 37.96 / Bush 18.72 / planing
ordering / 50-ft coast (safe: brake=0 path untouched) / anti-magnetism / jumps-never-cut-
throttle. Baseline quotes inside legs (0.510 g etc.) re-derived post-S2 (P2-5).
