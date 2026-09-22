# SLED KERNEL N1 — THE LEG WORK (`leg_work_nm`)

Lane `feel/sled-legwork-n1n2` in `D:\seads_sandboxes\sled-legwork`, off main `4acac47a2` (sled kernel v2),
on top of N2 (`d1c6ace3e` + `301a48c37`). Built 2026-09-22 (the lane docs date N2 as 2026-09-19). **Not
flown.** Build spec: the recon packet in the launch message (§2 N1); handoff words
`docs/SESSION_HANDOFF_20260918_sled_firstbuild_DRIVEN.md` §5 N1; drive words
`docs/DRIVE_WORDS_20260918_sled_firstbuild.md` runs 5 and 7.

Chad, run 5:

> "designate ctrl when not rolled on side, but stuck in bank upside down nose down vertical or nose up on
> track, rider attached can use legs to roll it over backward and on its side where it can then be weight
> shift mounted."

Chad, run 7:

> "that crouch mechanic built where sudburian can pull it over and on its side and then use shift to re
> right it for when vertically static in snow, also when fully upside down to extend legs with shift would
> put the sled up first then falling over on its side is the stage that another press of the shift can
> right you. TO make the r autoright key fully redundant."

---

## §1 — THE ONE DIAL

`sim::SledComfort::leg_work_nm` [N·m] — the rider's **leg torque budget** for the two stuck attitudes the
v2 STAND pendulum (`right_assist_nm`, roll axis) cannot reach. Struct default / identity **0.0** (a
structural branch: nothing inside is reached and **no state is written**, `leg_prev_stand` included);
**ships 2400** in `config/scenario.toml [sled_comfort]`; env `SEADS_SLED_LEGWORK` band [0, 4000] (0 = kill);
banner prints `leg_work_nm`; loader `require` + `check >= 0`; tape roster `X(leg_work_nm)` + tape-absent
identity `0.0`. The six-touch shape is v2's / N2's exactly.

### The ladder (every rung press-gated, one-way, never automatic)

| stuck attitude (band, hysteretic) | key | what the legs do | then |
|---|---|---|---|
| **PITCHED** — on its END: `\|up_body.z\| >= sin 50°` enter / `< sin 40°` exit, and `\|up_body.x\| < sin 35°` (not on a side) | **CTRL** (crouch) | `+leg_work_nm · charge · w_pump` about body **X** = nose **UP** ("roll it over **backward**": a nose-down machine's tail comes back down, a nose-up machine goes over onto its back) plus `0.5 · leg_work_nm` about body **Z** toward the side it already leans (the pendulum's own `dir` rule) so it lands on a **side** when it has any lean | on a side → SHIFT (v2 pendulum); on its back → SHIFT (stage 2); upright → done |
| **INVERTED** — on its BACK: `up_body.y <= cos 150°` enter / `> cos 140°` exit | **SHIFT** (extend legs) | `leg_work_nm · tanh(up_body.z / right_dir_eps) · charge · w_pump` about body **X** = lift the end that is already higher; the v2 pendulum fires on the **same** press about Z | off its back onto a side; the cadence rights it |
| **ON_SIDE** — `\|up_body.x\| >= sin 50°` | SHIFT | the v2 pendulum, **byte-untouched** | upright |

Gates, all reused, no new dials: hands on (`grip.attached`), ground contact (`air_s <= rolled_grace_s`,
the rolled latch's own), the pendulum's hysteretic low-passed speed gate (`right_assist_armed`, 4.0 / 3.2
m/s), **the kernel's own rolled latch (`s.rolled`: tilt > 75 deg held `rolled_persist_s` in contact — the
fact that makes R legal; red-team fold 2026-09-19, `docs/sled_legwork/REDTEAM_2026-09-19.md` F1: without
it a machine parked UPRIGHT on a 52 deg flank armed PITCHED and one CTRL backflipped it, omega 11.0 rad/s,
1.23 s airborne; post-fold omega 0.65 == identity)**, a `rolled_persist_s` (0.30 s) **dwell** in the band
before a stage ARMS (silently — the app-only
HUD names it), a **rising edge** of the stage's own key AFTER it armed (a key already held when the machine
got stuck never fires; release ends the press), the pendulum's own charge ODE (`right_charge_push_s` /
`right_charge_rest_s`: a hold spends finite energy, a release refills) and pump weight
(`right_pump_omega_eps`), and the torque stops the substep the attitude leaves the stage's exit band or a
gate drops (the stage DISARMS; a fresh dwell is owed — one-way). Constants (`sim/sled.cpp` anon namespace):
`kLegPitchEnter/Exit`, `kLegSideBand`, `kLegInvEnter/Exit`, `kLegRollShare 0.5`.

New state (`SledState`, all derived, the `right_assist_armed` class, **not** in `SLEDTAPE_PIN_D`):
`leg_stage`, `leg_cand`, `leg_dwell_s`, `leg_press`, `leg_charge`, `leg_prev_stand`, `leg_nm_now`. Enum
`sim::LegStage` beside `Patch`. CTRL while riding stays the tuck (`rider_up_m → -tuck_drop_m`); the
attitude gate lives in the kernel stage, `app/main.cpp:8681` untouched.

### ⚠ Two places the build departed from the recon spec, by measurement

1. **Stage 1 direction is "backward" (nose UP, +X), not "toward level".** The spec assumed every stage
   starts from an unstable equilibrium. Measured (§2.1): the nose-up-on-the-tail pose is a **stable
   two-contact rest** at tilt 105.5° (tail + rear-top hull). Pushing it toward level (nose down) fights
   ~900 N·m of gravity through top dead centre; at 1500 the CTRL cadence never moved it (8 presses / 12 s,
   ω peak 0.38 rad/s). His word is "roll it over backward": for nose-up that is over onto its back/side,
   for nose-down it is the tail coming down — both are a nose-UP body rotation, one constant sign by the
   measured law. Backward goes with gravity, which is what legs can do.
2. **The shipped value is 2400, not 1500.** 1500 never leaves the end in either direction; 1800 leaves in
   1.22 s; 2400 = one press, 0.73 s, and is the pendulum's own `right_assist_nm`. Ladder 1800 / 2400 /
   3200, band [0, 4000].

### ⚠ The shipped value was picked by measurement, not by his seat
Stated in `sim/sled.h`, the toml, `app/main.cpp`, `drive_sled_n1_legwork.bat` and here.

---

## §2 — THE MEASUREMENTS (`build\seads_tests.exe "legwork rest sweep"`, `"legwork ladder probe"`, the `[legwork]` legs)

Fixtures: flat Bush field, `posed_raw(pitch, roll)` spawns the coarse hull CLEAR of the floor at every
attitude, idle 3 s to settle (`settled_pose`), dt 1/120, 12 substeps, shipped toml.

### 2.1 Rest poses (the recon's §0.4 precondition), pre-edit kernel

| pose | depth 0.5 / 1.0 / 2.0 m | rest after 3 s | band |
|---|---|---|---|
| nose-DOWN 90° / 80° / 70° | leaves the end band at 0.67 / 0.83 / 1.52 s | tilt 173.7° / 177.0°, `up_body` (0, −0.99, −0.11) | **INVERTED** — nose-down is NOT a rest pose on flat snow; it falls onto its back and IS the stage-2 case. A nose held by a BANK is his real nose-down case and has no flat fixture; the PITCHED band covers it with the same expression |
| nose-DOWN 60° / nose-UP 60° | leaves at 0.47 / 0.70 s | tilt 2.9° / 5.2° (upright) | — |
| nose-UP 70° / 80° / 90° | never leaves | tilt 106.7° / 105.4° / 105.5°, `up_body` (0, −0.27, −0.96) | **PITCHED** — his "nose up on track", a stable two-contact rest |
| inverted 180° | — | tilt 173.5°, `up_body` (0, −0.99, −0.11) | **INVERTED** |

Snow depth 0.5 / 1.0 / 2.0 m: identical to the printed digits (the poses rest on the hull, not the snow).
`grip.attached` true, `air_s` 0.00 throughout. The stage readout arms in the measured band with the dial
shipped and `leg_nm_now == 0.0` (nothing moved: pinned by `legwork_rest_pose_holds_before_any_press`).

Sign law (`legwork_pitch_sign_is_measured`): +0.5 rad/s about body X on a settled upright machine raises
the nose's radial component 0.065331 → 0.069074 — **+angular_vel.x is nose UP**; nose-down 60° reads
`up_body.z` > 0.5, nose-up 60° reads < −0.5 — **nose-down is +up_body.z**.

### 2.2 The ladder (depth 0.5 m; 1.0 and 2.0 m print the same digits)

Stage 1: nose-up rest, CTRL cadence 1.0 s held / 0.7 s off, "off its end" = on a side, on its back, or
upright. Stage 2: inverted rest, SHIFT cadence 1.0 / 0.7 s, to `tilt < right_tilt_lo_rad`.

| budget [N·m] | stage 1: t off end | presses | lands | grip | stage 2: |z| peak | t upright | presses | grip |
|---|---|---|---|---|---|---|---|---|
| 0 (identity) | never (12 s) | 8 | stays on end, tilt 105.1° | 1 | 0.12 | 1.62 s | 1 | 1 |
| 1500 (spec) | never | 8 | stays | 1 | 0.25 | 1.52 s | 1 | 1 |
| 1800 | 1.22 s | 1 | its back | 1 | 0.29 | 1.47 s | 1 | 1 |
| 2000 | 0.94 s | 1 | its back | 1 | 0.32 | 1.44 s | 1 | 1 |
| 2200 | 0.82 s | 1 | its back | 1 | 0.35 | 1.43 s | 1 | 1 |
| **2400 (ships)** | **0.73 s** | **1** | its back (tilt 150.4° at the crossing; ω peak 1.94 rad/s, gs peak 1.30 m/s, air 0.00) | 1 | **0.39** | **1.43 s** | 1 | 1 |
| 3200 | 0.56 s | 1 | its back | 1 | 0.56 | 1.47 s | 1 | 1 |
| 4000 (band top) | 0.46 s | 1 | its back | 1 | 0.70 | 1.36 s | 1 | 1 |

Read honestly:
- **Stage 1 works from 1800 up, one press.** On the flat fixture a 10° roll seed settles out on the
  two-contact rest before the press, so the machine lands on its **back** (the roll share is 0 at zero
  lean; a real bank keeps the lean and the side landing) — and its back is exactly stage 2's fixture, his
  run-7 ladder.
- **Stage 2's lift is real but does not stand the machine on its end on flat snow**: |z| peak rises
  monotonically 0.12 → 0.39 (2400) → 0.70 (4000) but never reaches sin 50° = 0.766 — standing a 331 kg
  machine on its end from its back is ~3,000 N·m sustained, above the band. What rights it is the v2
  pendulum on the same press, and **the pendulum alone already rights the flat inverted fixture in 1.62 s**
  (its latched brace seeds `dir` at the 180° dead point); the lift shortens that to 1.43 s. His "fully
  upside down and stuck" is a snow/bank case this flat fixture cannot reproduce; whether the lift is what
  gets him off his back there is **his drive's question** (§7).
- The rider stayed welded (`grip.attached`) on every row; nothing ever left the ground (`air_s` 0.00).

### 2.3 The identity legs (4000 vs 0, whole-state `SLEDTAPE_PIN_D` equality every tick)

| leg | script | first divergence |
|---|---|---|
| `legwork_never_touches_a_send_or_a_moving_machine` | 20 m/s hop (+6 m/s radial, ~1.2 s airborne, lands), CTRL 1.5 s then SHIFT | −1 (never); end air 0.00, tilt 3.0°, stage 0 |
| same | 5 m/s level run under throttle 0.5, CTRL then SHIFT | −1; end gs 13.48, stage 0 |
| `legwork_is_press_gated_not_automatic` | nose-up, no key, 10 s | −1; stage 1 (armed, moved nothing) |
| same | nose-up, CTRL held from tick 0 for 10 s | −1; stage 1 (held key never fires) |
| same | inverted, no key, 10 s | −1; stage 2 |
| same | inverted, SHIFT held from tick 0 for 10 s | −1 (the pendulum runs in both arms; the legs never fire) |
| `legwork_ctrl_is_still_a_tuck_when_riding` | level drive, CTRL held 4 s | −1; `rider_up_m` −0.1000, stage 0 |
| `legwork_identity_zero_is_bit_exact_on_every_stage_fixture` | three fixtures × scripted cadences, dial 0.0 | == the 17-digit goldens printed by the hidden `legwork golden printer` on the PRE-edit build (the literals live in the test) |

---

## §3 — THE GOLDEN REPLAY (bytes)

Seven tapes, read-only: `D:\flight_sim2\seads-recon\build-play\sled_tape_86..91.sledtape` and
`D:\seads_sandboxes\sled-ride-b1\sled_tape_9.sledtape`. `build\seads_sled_probe.exe tape <abs path>`,
pre = `replay_pre_n1/` (lane tip `301a48c37` + the N1 constants only, before the block), post =
`replay_post_n1/` (this build, identity by tape-absent reconstruction), filter
`grep -v -E "DIAL GAP|dial\(s\)|dial gap|^    [a-z_]"`.

### 3.1 At the identity — the filtered diff prints nothing on all seven

Raw diff, by construction only (86–91): `!! DIAL GAP: 3 dial(s)` → `4 dial(s)`; the dial list
`rolled_throttle_frac, ice_bite_mu, track_lat_slip_shed` → `rolled_throttle_frac, ice_bite_mu,
leg_work_nm, track_lat_slip_shed`; `VERDICT: ... but 3 dial(s)` → `4 dial(s)`. tape_9: `1 dial(s)` →
`2 dial(s)`, `ice_bite_mu` → `ice_bite_mu, leg_work_nm`. Nothing else.

| tape | replayed | first divergence | rolled tape / replay | exit |
|---|---|---|---|---|
| 86 | 11430/11430 | — | 1464 / 1464 | 0 |
| 87 | 14272/14272 | — | 31604 / 31604 | 0 |
| 88 | 1837/11896 | tick 46589 `position.x` (ground key mismatch at sample 186528) | 45908 / 45908 | 1 |
| 89 | 6355/15256 | tick 68315 `velocity.x` | −1 / −1 | 1 |
| 90 | 12990/12990 | — | 87761 / 87761 | 0 |
| 91 | 14833/20734 | tick 14832 `velocity.x` | 2317 / 2317 | 1 |
| tape_9 (run 7) | 3983/20890 | tick 3982 `position.x` (ground key mismatch at 338486) | 3210 / 3210 | 1 |

Identical to `replay_pre_n1/` line for line after the filter, and to N2 §3.1 / v2 §2.2 (the three
pre-existing divergers and tape_9's pre-existing tick-3982 divergence on this lane base).

### 3.2 At the shipped 2400 (`SEADS_SLED_LEGWORK=2400`, probe-only override)

Tapes **86, 87, 88, 89, 90, 91: identical to the identity replay** (same replayed counts, same first
divergence, same rolled ticks) — tape 91's 296 rolled ticks (first at 2317) never armed a stage with a
fresh CTRL/SHIFT edge before its pre-existing divergence at 14832. **tape_9 (his run 7, SHIFT while
rolled): first divergence moves from tick 3982 to tick 3282 `position.x`** (ground key mismatch at sample
220900), 72 ticks = 0.60 s after its rollover at tick 3210 — the 0.30 s dwell armed a stage and his press
fired the legs. Parsed (red-team, `tape9_attitude.py`, columns per `sled_tape.h:190`): the band is
**INVERTED** (`up_body.y` −0.92 ≤ cos 150° at the edge), entered ~tick 3236, dwell 36 ticks = 0.30 s, the
SHIFT rising edge at tick **3282**. Ticks 3190–3208 sit on the end (`up_body.z` −0.99..−0.87) with `rolled
0`: R (`autoright_legal` needs `sled_rolled` afoot) does not cover that 0.15 s window and, after the
rolled-latch fold, neither do the legs until the latch fills at 3210 — accepted, the latch is the kernel's
word for "stuck". Re-replayed on the fold: `3283/20890`, `rolled tape=3210 replay=3210`, unchanged. That is the honest reason the struct default is 0.0 forever and 2400 lives in the toml:
at a driven value his tape replays a different machine from the one that cut it.

---

## §4 — THE LEGS (`test/unit/test_sled_legwork.cpp`, tag `[legwork]`, registered in `CMakeLists.txt`)

`legwork_rest_pose_holds_before_any_press`, `legwork_pitch_sign_is_measured`,
`legwork_identity_zero_is_bit_exact_on_every_stage_fixture`,
`legwork_ctrl_from_nose_up_kicks_it_off_its_end_within_6_s` (shipped: t ≤ 6 s, ≤ 3 presses, exactly 1
measured, grip held, never airborne; identity: never leaves the band),
`legwork_shift_from_inverted_goes_on_end_then_rights` (shipped: |z| peak > identity's, upright ≤ 12 s,
grip held, never airborne; identity: never on its end; "reaches the end band" REPORTED not required),
`legwork_never_touches_a_send_or_a_moving_machine`, `legwork_is_press_gated_not_automatic`,
`legwork_ctrl_is_still_a_tuck_when_riding`, **`legwork_never_arms_on_an_upright_machine_on_a_bank`** (red-team
fold: flat / 46 / 52 deg SF1 ridge flank, upright, coasting to rest below the speed gate, CTRL then SHIFT
rising edges — stage NONE through the CTRL press, 4000 vs 0 whole-state identical; the 52 deg row's SHIFT
fires the v2 pendulum in BOTH arms, recorded as the pendulum's P2). Fold pins in the existing legs: stage 1
REQUIREs the landing band it claims (`inverted`, not any of three — the toward-level mutant now reds); stage
2's identity arm REQUIREs the v2 pendulum-alone righting (`t_right` 1.62 s ≤ 12 s). Hidden probes: `legwork rest sweep`, `legwork golden printer`,
`legwork ladder probe`. Plus: `scenario_sled_comfort_matches_kernel_defaults` (toml 2400, struct 0.0),
`scenario_sled_comfort_rejects_a_v2_dial_out_of_band` (−1.0 rejects),
`sled_tape_absent_dials_replay_at_the_identity_not_the_v2_default` (absent + identity 0.0).

Not built from the spec: `legwork_ctrl_from_nose_down_lands_on_a_side` (nose-down is not a flat rest pose,
§2.1 — the pitched stage is exercised from the nose-up fixture with the same expression);
`legwork_shift_from_inverted_goes_on_end` as a REQUIRE (the end band is not reached at any value in the
band on flat snow, §2.2 — pinned as the lift's |z| gain instead).

---

## §5 — R, THE TRIPWIRE, THE TUMBLE RULING

`git diff 4acac47a2 -- app/main.cpp app/player_mode.h | grep -c KEY_R` = **0**. R stays bound and
byte-untouched; the ladder makes it unnecessary, not absent.

The tumble ruling (audit rung 8, "a backflip is not a rollover"; DRIVE_WORDS "kill the tumble, never the
authority"): this block applies **pitch** torque on a grounded machine, the axis that rung was refused
on. It is not that term because it is contact-gated, speed-gated, press-gated and stage-gated; a send never
sees it (§2.3: 20 m/s hop, 4000 vs 0, bit-identical).

Existing selfright legs at `shipped()` now read `leg_work_nm = 2400` and measure the stacked machine —
the gate (§6) says whether any moved.

---

## §6 — GATE (on `9e0cddfdc`; the red-team's rerun and the FOLD gate are in
`docs/sled_legwork/REDTEAM_2026-09-19.md` §5)

⚠ The red-team found this log's mtime (09:51:44, 1039 s) predates the commit (09:41:35) by the exe relink
(`.ninja_log` 09:53); its own rerun on the HEAD exe, `build/gate_redteam_04e06b182.log`, is the gate that
holds for `04e06b182`: 6/2164, baseline six by name, `gate_baseline.py check` exit 0.

**THE FOLD GATE (the one that holds for this lane's tip): `build/gate_c8a61c05c.log`** on fold commit
`c8a61c05c` (2026-09-22 10:47:24; log mtime 11:05:52, +18m28s), detached, `Total Test time (real) =
1028.76 sec`, **6 failed of 2165** (2164 + `#963 legwork_never_arms_on_an_upright_machine_on_a_bank`),
`python tools/gate/gate_baseline.py check build/gate_c8a61c05c.log` exit 0: "OK -- the red set is EXACTLY
the baseline, member for member" — 79, 119, 1301, 1302, 1340, 1343 by the same six names as below.


Full `ctest --test-dir build -C Debug -j4 --output-on-failure`, launched detached (`Start-Process`, log
`build\gate_n1_legwork.log`, 1039.33 s real): **6 failed of 2164** (2156 + the 8 `[legwork]` legs, all
`Passed` in the gate: #955–#962). Verdict written by the runner, `python tools/gate/gate_baseline.py check
build\gate_n1_legwork.log`, exit **0**: "OK -- the red set is EXACTLY the baseline, member for member" —
`probe P-F`, `E12.1`, `sled_assist_reference_plane_is_load_weighted`,
`sled_slides_before_it_tips_on_flat_snow`, `sled_debug_sink_is_write_only`,
`sled_grip_ceiling_stays_below_the_tip_threshold`. The twelve existing `selfright*` legs, now reading
`leg_work_nm = 2400` from the shipped toml, stay green (240 assertions) — no re-pin owed.

Build: Ninja, Debug, WinLibs LLVM (`build/CMakeCache.txt`), the lane's own `build/`. Chad's exe is a
RelWithDebInfo `build-play` in `D:\flight_sim2\seads-recon`, rebuilt there after the push — never here.

---

## §7 — OPEN FOR HIS DRIVE (`drive_sled_n1_legwork.bat`)

1. **Nose-up on the track, stuck**: does one CTRL put it over? Onto its back (then SHIFT) or onto a side
   (then SHIFT)? If the kick is violent, 1800; if nothing, 3200.
2. **Nose-down in a bank**: does CTRL bring the tail back down? (No flat fixture exists for this; the
   torque sign is the same "backward".)
3. **Upside down in snow**: does SHIFT bring it up and over where the 09-18 rocking alone did not? That is
   the question the flat fixture cannot answer (§2.2).
4. **Did he ever need R?**
5. Ruling owed: the roll share (0.5) at exact zero lean lands the machine on its back rather than a side —
   accepted here; a seeded side (the brace's `right_seed_frac` under CTRL, today 0) is one line if he
   wants "always on its side".
5b. **Ruling owed (red-team P1-12): THE SIGN of the CTRL kick.** At the shipped 2400 BOTH signs leave the
   nose-up rest in one press. **Backward (+1, ships, his run-5 word)**: onto its BACK, tilt 150.4°, ω peak
   1.94 rad/s, then SHIFT (stage 2) — the run-7 two-rung ladder. **Toward level (−1)**: onto its TRACK,
   upright at tilt 13.1°, ω peak 3.43 rad/s, no second rung. §1's "1500 never leaves it toward level" is
   true at 1500 only. The stage-1 leg now PINS the shipped landing (its back); if his seat takes
   toward-level, the sign and that REQUIRE move together (one line each).
6. Carried from N2: `track_lat_mu` surface-blindness.
