# SLED KERNEL v3 CANDIDATE — N1 LEG WORK + N2 ICE BITE. THE LANDING PACKET (GATED, NOT DRIVEN)

Lane `feel/sled-legwork-n1n2`, worktree `D:\seads_sandboxes\sled-legwork`, off main `4acac47a2` (sled
kernel v2, tag `sled-kernel-v2-signed`). Real calendar date of this packet: **2026-09-22** (the lane's
docs date N2 as 2026-09-19 and N1 as 2026-09-22; the red-team record is dated 2026-09-19). Nothing on
this lane has been pushed anywhere; the fly tree `D:\flight_sim2\seads-recon` was opened read-only for
the six 09-17 tapes and never written (`git status --porcelain` there was never run, never needed).

**Chad's landing word, verbatim (carried into the flight-log row, the LANES.toml line and the tag body):**

> "please add the docs and addendum and these additions to the sled kernel ty. Use an efficient opus workflow to accomplish this, then push to main and seads-recon .. ty"

**Binding sentinel terms (agreed):** this lands **GATED, NOT DRIVEN** (the R4a grip-release precedent,
`233185252`). Tag **`sled-kernel-v3-gated`**, not `-signed`. The drive word is **OWED PER RUNG, N1 and N2
separately**, on the recon exe. `CLAUDE.md` names it a **CANDIDATE** kernel, not a signed one. His
sentence above goes in the tag body verbatim.

---

## §1 — WHAT IS ON THIS LANE (vs `4acac47a2`)

Five things, in commit order (`git log --oneline 4acac47a2..HEAD`):

| commit | what |
|---|---|
| `cf35d4019` | merge of `origin/audit/sled-ride` (`4aa8fed09`): the audit packet `docs/SLED_RIDE_AUDIT_20260918.md`, its addendum `docs/SLED_RIDE_AUDIT_20260918_ADDENDUM.md`, `docs/sled_audit/*` (strands, refutes, ladders, judges, X1 attribution, `x1_v17.json`), `tools/sled_tape_audit.py`, `tools/sled_tape_x1.py`, the `[lanes.sled-audit]` LANES section, two `.gitignore` lines. Docs and tools only; no kernel byte |
| `d1c6ace3e` + `301a48c37` | **N2** `ice_bite_mu` — the rung + its gate record |
| `9e0cddfdc` + `04e06b182` | **N1** `leg_work_nm` — the rung + its gate record |
| `c8a61c05c` + `00595735c` | the red-team **FOLD** (four P1s) + its gate record |
| (this commit) | landing docs: this file, the flight-log row, the LANES line, the CLAUDE.md line |

### 1.1 The two dials — one dial per rung, identity = v2 bit-exactly

| | **N2 `sim::SledComfort::ice_bite_mu`** | **N1 `sim::SledComfort::leg_work_nm` [N·m]** |
|---|---|---|
| what | additive lateral mu on the **steered patches (skis) only**, **LakeIce only** (SK-1a blend-weighted), full at rest, fading to **exactly `+0.0`** by `kIceBiteVrefMs = 8.0 m/s` (a constant, `sim/sled.cpp:130`) | the rider's **leg torque budget** for the two stuck attitudes the v2 STAND pendulum cannot reach: PITCHED (on its end) + **CTRL**, INVERTED (on its back) + **SHIFT**; ON_SIDE + SHIFT stays the v2 pendulum, byte-untouched |
| struct default = identity | **0.0** (`sim/sled.h:475`; a `> 0.0` branch — at the identity the term does not exist) | **0.0** (`sim/sled.h:595`; a structural branch — nothing inside is reached, **no state written**) |
| ships (TOML) | **0.25** `config/scenario.toml [sled_comfort] ice_bite_mu` (line 2143) | **2400** `config/scenario.toml [sled_comfort] leg_work_nm` (line 2165) |
| env override / **kill** | `SEADS_SLED_ICE_BITE`, band [0, 0.45], **`=0` = the 09-18 machine** | `SEADS_SLED_LEGWORK`, band [0, 4000], **`=0` = the 09-18 machine** |
| loader | `require` + `check` [0, 1] | `require` + `check >= 0` |
| banner | `[config] sled first-build: ... ice_bite_mu ...` | `... leg_work_nm ...` |
| tape roster | `X(ice_bite_mu)` in `SLEDTAPE_COMFORT_D`, tape-absent identity 0.0 | `X(leg_work_nm)`, tape-absent identity 0.0 |
| how the value was picked | **MEASURED, NOT FLOWN** — middle of the ladder 0.15/0.25/0.35 (N2 doc §2) | **MEASURED, NOT FLOWN** — 1500 (spec) never leaves the end, 1800 in 1.22 s, **2400 = one CTRL press, 0.73 s**, = the pendulum's own `right_assist_nm` (N1 doc §2.2) |
| new state | none | `SledState::leg_stage/leg_cand/leg_dwell_s/leg_press/leg_charge/leg_prev_stand/leg_nm_now` — derived, NOT in `SLEDTAPE_PIN_D`; enum `sim::LegStage` |

The six-touch landing shape is v2's exactly (`sim/sled.h` default, `config/scenario.toml`, loader clamp,
banner print, tape header, tape-absent identity). The v2 split is kept on purpose: the struct default is
the identity forever because it doubles as the tape-absent reconstruction; the driven/shipped value lives
in the TOML.

### 1.2 The leg-work stages (every stage press-gated, one-way, never automatic)

Gates, all reused, no new dials: hands on (`grip.attached`), ground contact (`air_s <= rolled_grace_s`),
the pendulum's hysteretic speed gate (`right_assist_armed`, 4.0 / 3.2 m/s), **the kernel's own rolled
latch `s.rolled`** (tilt > 75° held `rolled_persist_s` in contact — the same fact that makes R legal;
red-team fold F1), a 0.30 s dwell in the band before a stage ARMS, then a **rising edge** of the stage's own
key (a key already held when the machine got stuck never fires), the pendulum's own charge ODE and pump
weight; the torque stops the substep the attitude leaves the exit band.

| stage | band | key | torque | measured (flat Bush, shipped toml, dt 1/120) |
|---|---|---|---|---|
| PITCHED (on its end) | `\|up_body.z\| >= sin 50°` enter / `< sin 40°` exit, `\|up_body.x\| < sin 35°` | **CTRL** | `+X` nose-UP ("backward", his run-5 word) + `0.5×` about Z toward the existing lean | nose-up rest (tilt 105.5°, a stable two-contact pose): off its end **0.73 s, 1 press**, lands on its back (tilt 150.4°, ω peak 1.94 rad/s, air 0.00, grip held); identity: never (105.1° after 6 s) |
| INVERTED (on its back) | `up_body.y <= cos 150°` enter / `> cos 140°` exit | **SHIFT** | `tanh(up_body.z / right_dir_eps)` about X = lift the higher end; the v2 pendulum fires on the same press | upright **1.43 s** vs **1.62 s at identity** (the v2 pendulum alone already rights the flat inverted fixture); \|z\| peak 0.39 vs 0.12; grip held, air 0.00, 1 press |
| ON_SIDE | `\|up_body.x\| >= sin 50°` | SHIFT | v2 pendulum, **byte-untouched** | v2 digits |

R: `git diff 4acac47a2 -- app/main.cpp app/player_mode.h | grep -c KEY_R` = **0**. R stays bound and
untouched; the ladder makes it unnecessary, not absent. CTRL while riding is still the tuck
(`rider_up_m` −0.1000, leg `legwork_ctrl_is_still_a_tuck_when_riding`).

### 1.3 Files this lane touches — `git diff --stat 4acac47a2..HEAD`, 61 files before this commit

Kernel and its six touches: `sim/sled.h` (+198), `sim/sled.cpp` (+217), `app/main.cpp` (+60/−),
`config/load_scenario.cpp` (+17), `config/scenario.toml` (+34), `test/harness/sled_tape.h` (10 lines).
Legs: `test/unit/test_sled.cpp` (+214, `[icebite]`), `test/unit/test_sled_legwork.cpp` (NEW, 949 lines,
`[legwork]`, registered in `CMakeLists.txt` +2), `test/unit/test_load_scenario.cpp` (+31),
`test/unit/test_sled_tape.cpp` (+6). Probe: `tools/sled_probe.cpp` (+291: `icebite`, `tape2` modes,
probe-only replay overrides). Graph: `generated/graph/*` regenerated in the rung commits. Launchers
(KEPT files, `drive_body_*.bat` precedent): `drive_sled_n1_legwork.bat`, `drive_sled_n2_icebite.bat`.
Docs: `docs/SLED_KERNEL_N1_LEGWORK.md`, `docs/SLED_KERNEL_N2_ICEBITE.md`,
`docs/sled_legwork/REDTEAM_2026-09-19.md`, the audit packet + addendum + `docs/sled_audit/*`, `LANES.toml`
(the sled-audit section from the merge; this commit adds the sled-legwork section), `.gitignore` (+2).
This commit adds `docs/flight-log.md`, `CLAUDE.md`, this file.

**THE FORBIDDEN SET IS EMPTY**, by command, not assertion:

```
$ git diff --stat 4acac47a2 -- control/ config/controller.toml sim/ground.h sim/step.cpp test/golden/
(prints nothing, 0 lines)
```

`control/`, `config/controller.toml`, `sim/ground.h`, `sim/step.cpp`, `test/golden/` are all EMPTY in the
diff. No golden moved. Kernel v17 and the T2 facet-contact rung are untouched.

---

## §2 — THE PROOF

### 2.1 THE NO-INPUT ARM: with no CTRL/SHIFT leg work and off lake ice, this exe IS sled kernel v2

Stated as the sentinel asked for it: **with no CTRL/SHIFT leg work and off lake ice, the exe reproduces
sled kernel v2 bit-exactly on the v2 tapes and goldens — the golden diff prints nothing.** Three pieces
of evidence, each from its own report:

**(a) Golden replay at the identity, seven tapes** — the six 09-17 tapes
`D:\flight_sim2\seads-recon\build-play\sled_tape_86..91.sledtape` (read-only) and his run-7 tape
`D:\seads_sandboxes\sled-ride-b1\sled_tape_9.sledtape`, `build\seads_sled_probe.exe tape <abs path>`,
filter `grep -v -E "DIAL GAP|dial\(s\)|dial gap|^    [a-z_]"` (the lines that change by construction
because `SLEDTAPE_COMFORT_D` grew):

- N2 (`docs/SLED_KERNEL_N2_ICEBITE.md` §3.1, `replay_pre/` = probe built from `cf35d4019` vs
  `replay_post/`): *"tape_86: filtered diff prints nothing 11430/11430, rolled 1464/1464 … tape_87 …
  14272/14272 … tape_88 … 1837/11896 … FIRST DIVERGENCE tick 46589 position.x (pre-existing) … tape_89 …
  6355/15256 … tick 68315 velocity.x (pre-existing) … tape_90 … 12990/12990, rolled 87761/87761 … tape_91 …
  14833/20734 … tick 14832 velocity.x (pre-existing) … tape_9: filtered diff prints nothing 3983/20890 …
  tick 3982 position.x (pre-existing on this lane base)"*. *"The only raw differences are DIAL GAP: 2 → 3,
  the dial list gaining ice_bite_mu, and the VERDICT count."*
- N1 (`docs/SLED_KERNEL_N1_LEGWORK.md` §3.1, `replay_pre_n1/` vs `replay_post_n1/`): *"the filtered diff
  prints nothing on all seven"*; same replayed counts (86 11430/11430, 87 14272/14272, 88 1837/11896, 89
  6355/15256, 90 12990/12990, 91 14833/20734, tape_9 3983/20890), same divergers at the same tick and
  field, rolled tape == replay on all (1464, 31604, 45908, −1, 87761, 2317, 3210). Raw diff = DIAL GAP 3→4,
  the dial list adding `leg_work_nm`, the VERDICT count. Nothing else.
- FOLD (`docs/sled_legwork/REDTEAM_2026-09-19.md` §3, `unset SEADS_SLED_ICE_BITE SEADS_SLED_LEGWORK`,
  `replay_fold/` on the folded probe): filtered diff lines vs `replay_pre/` (v2 base, both dials absent) =
  **0, 0, 0, 0, 0, 0, 0**; raw diff lines vs `replay_post_n1/` = **0, 0, 0, 0, 0, 0, 0**. *"Every diff
  prints nothing."*

The three pre-existing divergers (88/89/91, the GROUND KEY replay class) diverge at the same tick and
field as on v2's landing (`docs/SLED_KERNEL_V2_LANDING.md` §2.2); tape_9's tick-3982 divergence is
pre-existing on this lane base (cut on the sled-ride-b1 lane's kernel). None of them moved on any commit
of this lane.

**(b) Hermetic goldens, 17 digits, recorded on the PRE-edit build, green on the post-edit build:**
`sled_ice_bite_zero_is_the_identity` (600-tick full-lock ice drive at dial 0.0, v_end 4.6 m/s);
`legwork_identity_zero_is_bit_exact_on_every_stage_fixture` (three fixtures × scripted cadences at dial
0.0; the printout is reproduced verbatim in the red-team record §2.1, `n1_golden_pre.txt`).

**(c) Whole-state identity legs (`SLEDTAPE_PIN_D` equality every tick):** N2 — 20 m/s full lock on ice,
0.45 vs 0 and shipped-toml vs shipped-with-0: identical (the fade reaches exactly `+0.0`); Bush /
TrailMain / Road at 3 m/s, 0.45 vs 0: identical (ice-only). N1 — 4000 vs 0 on a 20 m/s hop (+6 m/s
radial, lands), a 5 m/s level run, nose-up / inverted with no key for 10 s, nose-up / inverted with the
key HELD from tick 0, a level drive under CTRL, and (fold F3) an upright machine on a flat / 46° / 52°
flank with CTRL then SHIFT edges: first divergence **−1 (never)** on every one.

What the shipped values change, and where, is stated honestly: at 0.25 the tapes that start parked on
the lake diverge at tick 12 (86/88/90) and 87 at 42381; at 2400 only tape_9 moves (tick 3982 → **3282**,
0.60 s after its rollover at 3210 — his SHIFT fired the legs). That is why the struct default is 0.0
forever and the shipped value lives in the TOML.

### 2.2 The gate, by name (runner's verdict, never hand-written)

Code tip **`c8a61c05c`** (the fold), tree clean, `ctest --test-dir build -C Debug -j4 --output-on-failure`
launched DETACHED (`Start-Process cmd /c`, polled), log `build/gate_c8a61c05c.log`, mtime 2026-09-22
11:05:52 vs commit 10:47:24 (+18m28s), `Total Test time (real) = 1028.76 sec`:

```
99% tests passed, 6 tests failed out of 2165

$ python tools/gate/gate_baseline.py check build/gate_c8a61c05c.log      (exit 0)
gate: 6 failed of 2165
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

The six BY NAME: `79 probe P-F: the relentless raider keeps the pump and shoots back`; `119 E12.1: the
raider backfill keeps a faction's pump offense alive`; `1301 sled_slides_before_it_tips_on_flat_snow`;
`1302 sled_grip_ceiling_stays_below_the_tip_threshold`; `1340 sled_assist_reference_plane_is_load_weighted`;
`1343 sled_debug_sink_is_write_only` — the same six as `gate_n2_icebite.log` (6/2156), `gate_n1_legwork.log`
(6/2164), the red-team's rerun `gate_redteam_04e06b182.log` (6/2164) and v2's `gate_3990cde63.log`
(6/2152). **Nothing new.**

**ctest -N delta:** 2152 (v2 landing tip) → 2156 (N2: the four `[icebite]` legs) → 2164 (N1: the eight
`[legwork]` legs, `#955–#962`) → **2165** (fold F3: `#963 legwork_never_arms_on_an_upright_machine_on_a_bank`).
Every added leg `Passed` in the gate. The twelve existing `selfright*` legs, now reading `leg_work_nm = 2400`
from the shipped toml, stay green — no re-pin owed.

Build for the gate: the lane's own `build/`, Ninja Debug, WinLibs LLVM. `build/gate_c8a61c05c.status`
reads `0` — NOT ctest's exit (cmd `%ERRORLEVEL%` parse-time expansion); the verdict is the runner's log
check above.

### 2.3 The red-team and its fold (`docs/sled_legwork/REDTEAM_2026-09-19.md`)

Two independent passes on `04e06b182`, eighteen findings. **Every P1 folded on the lane; P2/P3 recorded,
not folded. No dial value moved; both dials stay one-dial.**

| fold | finding | what changed | proof |
|---|---|---|---|
| **F1** | P1-11 (+ closes P2-2): PITCHED read against RADIAL up, so a machine parked UPRIGHT on a ≥ 50° bank armed and one CTRL backflipped it | `sim/sled.cpp` leg gate `gated = hands_on && contact && s.right_assist_armed && s.rolled` — the legs are legal exactly where R is | 52.2° flank, parked upright, CTRL: pre-fold leg_nm 2049 / ω 11.00 / air 1.23 s / inverted 1; **post-fold stage 0 / leg_nm 0 / ω 0.65 / first divergence −1 == identity**. Stage-1/2 fixture digits unchanged (0.73 s tilt 150.4 ω 1.94; 1.43 s vs 1.62 s) |
| **F2** | P1-12: stage-1 leg could not tell the kick's sign apart | `legwork_ctrl_from_nose_up_kicks_it_off_its_end_within_6_s` now `REQUIRE(inverted(r.s)); REQUIRE(!upright(r.s, p))` | mutation M8 (toward-level sign) **KILLED** (`test_sled_legwork.cpp:545`, mutant lands upright tilt 14.1); it survived on `04e06b182` |
| **F3** | P1-13: no leg observed the attitude band | new `legwork_never_arms_on_an_upright_machine_on_a_bank` (flat / 46° / 52.2° SF1 ridge flank, CTRL then SHIFT edges) | stage NONE through CTRL, leg_nm 0, 4000 vs 0 first divergence −1 on all three rows; on `04e06b182` the 52° row would red. M10 (`kLegPitchEnter = 0`) still survives — provably unreachable after F1 (rolled AND not-on-side ⇒ \|z\| > 0.7775 > sin 50°), recorded honestly |
| **F4** | P1-1: v2 pendulum-alone righting from full inversion pinned nowhere | identity arm `REQUIRE(b.t_right > 0.0 && <= 12.0)` | pins the 1.62 s |

Post-fold unit legs: `[legwork],[icebite]` → `All tests passed (176 assertions in 13 test cases)`.

Recorded, not folded (none landing blockers): #12 the CTRL sign ruling (his seat, §4); #15 the v2
pendulum's `w_tilt` reads pitch as tilt (v2 bytes, pendulum's owner); #14/#3 the legs' borrowed speed gate
is frozen TRUE when `right_assist_nm` is toml-killed (F1's rolled latch still holds them past 75°); #4
env band [0,0.45]/[0,4000] vs loader [0,1]/≥ 0 asymmetry (loader comment says deliberate); #8 ON_SIDE
readout has no hysteresis (HUD flicker); #16 the N2 identity-leg "KILLED BY" comment overstated; #9
probe-only env override names; #10 the LANES.toml line (this commit).

---

## §3 — CHAD'S DRIVE CHECKLIST (the two drive words owed, N1 and N2 SEPARATELY)

**The exe:** `D:\flight_sim2\seads-recon\build-play\seads.exe` — **RelWithDebInfo**, rebuilt in the fly
tree AFTER the recon merge of main (a Debug build-play was "jittery slow" on 09-18; check
`build-play\CMakeCache.txt` `CMAKE_BUILD_TYPE:STRING=RelWithDebInfo` before handing him the path).
`tasklist | findstr /I seads.exe` must print nothing before the rebuild (locked-exe trap). With every
`SEADS_SLED_*` unset the banner must read
`[config] sled first-build: traction_mu 3 rolled_throttle_frac 0.15 right_assist_max_ms 4 right_stand_shift_frac 0.5 track_lat_slip_shed 1.4 ice_bite_mu 0.25 leg_work_nm 2400 (env: none -- SLED KERNEL v2 + N2 + N1 shipped defaults)`
(the format string at `app/main.cpp:2677`).
Launchers: `drive_sled_n1_legwork.bat`, `drive_sled_n2_icebite.bat` at the repo root (they point at that
exe).

Keys unchanged: W throttle, S brake, A/D bars, mouse = lean (Q/E keyboard lean), **LEFT SHIFT** = STAND /
legs lift, **LEFT CTRL** = tuck / legs kick, **R autoright (unchanged, still works)**, C recentre.

### N1 — the leg work (one drive word owed)

Every stage: get STUCK (stopped, under 4 m/s, on the ground, rider on), wait about a third of a second
for the HUD to name the stage, then ONE fresh press — a key already held when you got stuck does nothing;
release and press.

1. **Stuck NOSE-UP on the track / on its tail** (a bank, a drift). HUD: `STUCK ON END -- CTRL: legs kick it
   over`. Press **CTRL** once. Expected: it goes over BACKWARD onto its back (or a side if it had any lean)
   in under a second. Then **SHIFT** (stage 2 or the 09-18 STAND) rights it. Question: one press or
   several? Violent → say so, it is 1800; nothing → 3200.
2. **Stuck NOSE-DOWN in a bank** (nose buried, tail up). HUD: `STUCK ON END`. Press **CTRL** once.
   Expected: the tail comes back down (same "backward" sign). No flat fixture exists for this — your seat
   is the only measurement.
3. **Fully UPSIDE DOWN** (in snow, where the 09-18 rocking did not get you off your back). HUD:
   `UPSIDE DOWN -- SHIFT: legs lift it`. Press **SHIFT** once: legs lift the high end and the same press
   rocks it — it should come off its back onto a side. Press **SHIFT** again: the STAND rights it.
4. **On its side.** HUD: `ON ITS SIDE -- SHIFT rights it`. **SHIFT** = the 09-18 STAND, unchanged.
5. **Did you ever NEED R?** (R still works; the goal is that you never reach for it.)

A/B in one line, then relaunch: `set SEADS_SLED_LEGWORK=0` (the 09-18 machine, legs dead), or
`set SEADS_SLED_LEGWORK=1800` / `3200` (the ladder). The banner names the value in force.

### N2 — lake-ice ski bite (one drive word owed)

On the lake, three speeds, full bars:

1. **Walking pace (under 20 km/h):** does the nose come around now? (measured: at ~3.7 m/s yaw 27 → 40
   deg/s, radius 7.8 → 5.3 m at 0.25).
2. **~25 km/h:** is the gain still there or already gone? It is designed to be mostly gone by 29 km/h
   (8 m/s) — say if the fade is too early (that is the 8 m/s constant, a second question, not the dial).
3. **50+ km/h:** SHOULD FEEL IDENTICAL to 09-18 (at 20 m/s the whole state is bit-identical at every
   value up to 0.45).
4. Not enough at walking pace → 0.35; a hook that wants to spin you → 0.15.

A/B in one line, then relaunch: `set SEADS_SLED_ICE_BITE=0` (the 09-18 machine), or
`set SEADS_SLED_ICE_BITE=0.15` / `0.35` (the ladder).

To A/B both dials off at once (the full v2 machine): `set SEADS_SLED_ICE_BITE=0` and
`set SEADS_SLED_LEGWORK=0`. Every other `SEADS_SLED_*` stays unset.

---

## §4 — RULINGS OWED (ask by name)

1. **N1 drive word** and **N2 drive word**, separately, on the recon exe (§3). Until both are given, the
   tag is `sled-kernel-v3-gated` and CLAUDE.md says CANDIDATE.
2. **THE SIGN of the CTRL kick (red-team P1-12 / #12).** At 2400 both signs leave the nose-up rest in one
   press. **Backward (+1, ships, his run-5 word "roll it over backward"):** onto its BACK, tilt 150.4°, ω
   peak 1.94 rad/s, then SHIFT (stage 2) — the run-7 two-rung ladder. **Toward level (−1):** onto its
   TRACK, upright at tilt 13.1°, ω peak 3.43 rad/s, no second rung. The stage-1 leg PINS the shipped
   landing; if his seat takes toward-level, the sign and that REQUIRE move together (one line each).
3. **Always-on-its-side vs the accepted back landing:** the 0.5 roll share at exact zero lean lands the
   machine on its back, not a side; a seeded side (`right_seed_frac` under CTRL, today 0) is one line if
   he wants "always on its side".
4. **`track_lat_mu` surface-blindness** (carried from N2): ice ski:track was 0.22:0.70, is now 0.47:0.70;
   a per-surface track mu is its own rung.
5. **The 8 m/s fade constant** (`kIceBiteVrefMs`) if the N2 drive says the fade is too early — a second
   dial, not this one.
6. **Env/loader band asymmetry** (#4): should 0.45 / 4000 hold at the loader too, or stay [0,1] / ≥ 0
   (a toml walk-back never refused)?
7. **The v2 pendulum's `w_tilt` reads pitch as tilt** (#15): SHIFT on a parked 52°-pitched upright
   machine tumbles it in BOTH arms (v2 bytes, the pendulum's owner, now visible in the suite as F3's
   52° row: rolled ticks 55, ω 4.96, divergence −1).
8. Carried from the audit packet, still owed: R1 (a word on the last six drives), R3 second half (rolled
   latch hard edge at 75.06°, no release hysteresis), R5 (drift ladder), R6 second half (STAND has no
   contact gate), R9 (lean frozen while the Sting is shouldered), R10/R12/R13 (reports against signed
   numbers), N5-PAIR (`k_air_shift`).

---

## §5 — CARRIED, NOT BUILT (this lane built two dials and nothing else)

- **`[snowpack] class_blend_m` 0.0 vs 1.0 A/B** — audit rung 1 / R2, Chad's own reserved A/B on the
  road-repair lane (`LANES.toml` road-repair, "THE STICK RULING"); needs no build; belongs to the lane that
  owns `world/snowpack.*`, not this one.
- **`k_air_shift` value** — audit rung 5, N5-PAIR ruling owed (throwing your weight in the air vs the
  nose-drop as a known).
- **Mouse CPI** — audit R8: no lean-scale number can be picked without it.
- **Brakes 0.80 g on snow** — audit R14, a REPORT against literature 0.4–0.5, no question asked yet.
- **Missing engine sound in seads-recon** (the sled-audio bug) — the reason v2's `rolled_throttle_frac`
  was approved on the tape and not on his ears; still open in the recon exe.
- N2's `sled_ice_bite_never_reaches_the_track` leg (the debug sink's `roll_tq[kRollBite]` sums over
  patches; carried by the `!g.steered` mutant run instead); the tape-86 open-loop total-rolled-tick rise
  (434 → 677 off ice after divergence onto stale served ground — an instrument limit, N2 doc §3.3); the
  1.03 m / 63 s parked creep on ice with the bars turned (pre-existing kernel behaviour, reported).
- N1's nose-down flat fixture (nose-down is not a rest pose on flat snow — it falls onto its back at
  0.67 s); stage 2 "reaches the end band" reported, not required (standing a 331 kg machine on its end
  from its back is ~3000 N·m sustained, above the band).

---

## §6 — THE HUMAN LANDING STEPS (nothing below this line was done by this lane)

In v2's order (`docs/SLED_KERNEL_V2_LANDING.md` §10), with this lane's names:

1. **ANNOUNCE TO THE SENTINEL, BEFORE ANY PUSH TO MAIN** — the one-paragraph packet (lane
   `feel/sled-legwork-n1n2`, tip, code tip `c8a61c05c`, gate `6 failed of 2165` the baseline six by name,
   the shared files of §1.3, the forbidden set empty). **WAIT FOR GO.**
2. `git fetch origin main && git merge-base --is-ancestor origin/main feel/sled-legwork-n1n2 && echo FF-OK`;
   `git push origin <sha>:refs/heads/main` — fast-forward only, no force. (`origin/main` was `4acac47a2` at
   this commit, unmoved since the lane's base; if it has moved, merge once + re-gate first.)
3. Annotated tag **`sled-kernel-v3-gated`** at the pushed tip, body = his sentence verbatim:
   *"please add the docs and addendum and these additions to the sled kernel ty. Use an efficient opus workflow to accomplish this, then push to main and seads-recon .. ty"*
   — and the words GATED NOT DRIVEN, drive word owed per rung (N1, N2).
4. Resync `D:\flight_sim2\seads-recon`: `git status --porcelain` EMPTY first, then
   `git fetch origin && git merge --ff-only origin/main`.
5. `tasklist | findstr /I seads.exe` prints nothing; rebuild `build-play` **RelWithDebInfo**; confirm the
   banner line of §3; hand him the two `.bat` files.
6. His two drive words → then, and only then, a `-signed` tag and the CLAUDE.md word CANDIDATE comes off.
