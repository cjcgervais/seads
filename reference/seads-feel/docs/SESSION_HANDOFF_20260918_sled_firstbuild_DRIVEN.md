# SLED FIRST BUILD — DRIVEN 2026-09-18 evening. Chad's verdicts, the driven values, and the next rungs

Lane `feel/sled-ride-firstbuild`, worktree `D:\seads_sandboxes\sled-ride-b1`, off `origin/main 533c86409`.
Build record: `docs/SESSION_HANDOFF_20260918_sled_firstbuild.md` (§1–§7). Words verbatim, in order:
`docs/DRIVE_WORDS_20260918_sled_firstbuild.md`. MAIN NOT TOUCHED. Nothing landed.

## 1. The exe he drove
`D:\seads_sandboxes\sled-ride-b1\build-play\seads.exe`, **RelWithDebInfo rebuild 19:47** on code tip
`eae9829ad`. The workflow's first build-play was `Debug`; his run-1 word on it was "took forever to
start and ran jittery slow"; rebuilt as RelWithDebInfo (his fly tree's type), re-driven: "it was the
same". LESSON: `build-play` = RelWithDebInfo always; `build/` (gate) = Debug.
His tapes from tonight: `D:\seads_sandboxes\sled-ride-b1\sled_tape_1..N.sledtape` (untracked).

## 2. The seven arms — his word and the DRIVEN VALUE per dial

| run | dial | value | his word (verbatim) | status |
|---|---|---|---|---|
| 1 | none | identity | "it was the same" | identity CONFIRMED by his hand |
| 2 | `SEADS_SLED_TRACTION_MU` → `params.traction_mu` | **3.0** | "a little different jump was more stable right at the start ... 2 is approved" | **APPROVED** |
| 3 | `SEADS_SLED_ROLLED_THROTTLE` → `comfort.rolled_throttle_frac` | **0.15** | "NOTICED NO DIFFERENCE" → tape: rpm 2645 with W held while rolled, thrust 0 N every rolled tick (track never touched ground; exe has no engine sound) → "3 IS APPROVED" | **APPROVED, unobserved** (see §3.3) |
| 4 | `SEADS_SLED_RIGHT_MAXSPD` → `comfort.right_assist_max_ms` | **4.0** | "4 is approved I can land upright more often" | **APPROVED** |
| 5 | `SEADS_SLED_STAND_SHIFT` → `comfort.right_stand_shift_frac` | **0.5** | "5 is approved" | **APPROVED** |
| 6 | `SEADS_SLED_TAILSHED` → `params.track_lat_slip_shed` | 0.4 → 0.8 → **1.4** | 0.4 "it needs more fishtail"; 0.8 "a little more fishtail even still"; 1.4 "okay good" | **APPROVED at 1.4** (no roll complaint at any step) |
| 7 | all five at the values above | | "yes very good" | **COMBINATION APPROVED** |

## 3. RULINGS STILL OWED BEFORE LANDING (ask by name)
1. **WHAT LANDS, and HOW.** Five dials were driven and approved in one A/B exe. The one-dial-per-kernel-
   version law says one at a time. Chad: land all five together as one version, or one per version?
   (The sentinel raised this; "yes very good" on run 7 is a feel word, not this ruling.)
2. **Tag form**: a lane tag (`sled-firstbuild-signed`) or a sled-kernel version number.
3. **Run 3 (`rolled_throttle_frac 0.15`) is approved on the tape, not on his senses**: no engine sound
   in this exe (the open sled-audio bug), and the downed track never touches ground on flat snow. Land it
   as approved, or hold it until the sound bug is fixed and he can hear it? His call.

## 4. LANDING PATH (sentinel terms, agreed 2026-09-18)
- Env is the DRIVE path; TOML/loader six-touch per dial is the LANDING path. `traction_mu` and
  `right_assist_max_ms` / `right_stand_shift_frac` already have loader paths (`[sled_comfort]` for the
  comfort pair; `traction_mu` is a SledParams field and needs its own); `track_lat_slip_shed` and
  `rolled_throttle_frac` are NEW fields needing the full six touches each.
- **(b) must be proven per dial:** the TOML-path exe reproduces the env-path exe bit-exactly on his six
  09-17 tapes AND at the driven values (golden diff prints nothing). Any dial that cannot: **(a)** he
  re-drives the TOML-path exe before his landing word is taken on it. The packet names (a)/(b) per dial.
- Post-merge kernel proof names exactly: `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`,
  `test/harness/sled_tape.h`, `test/unit/test_sled.cpp`, `test/unit/test_sled_selfright.cpp`
  (+ CMakeLists/graph if touched); `control/`, `controller.toml`, `sim/ground.h`, `sim/step.cpp`,
  `test/golden/` EMPTY. Full gate alone from clean on the landing tip, verdict by name; red-team records
  committed (they are); flight-log row; LANES status verbatim; annotated tag; merge origin/main once;
  ping the sentinel with the packet; WAIT for GO. The seven `drive_sled_firstbuild_*.bat` at the repo
  root are KEPT files (drive_body_*.bat precedent).

## 5. NEXT RUNGS HE ASKED FOR TONIGHT (not built; each its own lane, one dial, red-team, his drive)

### N1 — THE LEG WORK: make R fully redundant (his words, runs 5 and 7)
Run 5: "designate ctrl when not rolled on side, but stuck in bank upside down nose down vertical or nose
up on track, rider attached can use legs to roll it over backward and on its side where it can then be
weight shift mounted."
Run 7: "that crouch mechanic built where sudburian can pull it over and on its side and then use shift
to re right it for when vertically static in snow, also when fully upside down to extend legs with shift
would put the sled up first then falling over on its side is the stage that another press of the shift
can right you. TO make the r autoright key fully redundant."
Reading (verify with him): a STAGED, press-gated, rider-powered righting ladder that covers every stuck
attitude, so R is never needed:
- stuck **vertical / nose-down / nose-up in snow** (not on side): CTRL (crouch) = rider pulls/kicks her
  over BACKWARD onto her SIDE;
- **fully upside down**: SHIFT = extend legs, lift the sled up first (onto its end), it then FALLS onto
  its side;
- **on side**: SHIFT (existing runs 4/5 righting, pendulum) rights her.
Each stage is one press, one-way, with a hysteretic stuck detector (pitch band + contact + speed ~0),
hands welded (rider attached is his precondition), leg torque budgeted against the tumble ruling (kill
the tumble, never the authority), never automatic. CTRL today = TUCK while riding, so the crouch binding
is attitude-gated. STAND rights / LEAN throws stay separate systems.

### N2 — LAKE ICE GRIP (run 7)
"on lake ice the ski runners are not digging in to the ice and there is no turn authority on it, at
least at high speed it given limits turning I think it is less grip at lower speeds than I would like."
Reading: keep the high-speed limit (real), raise LOW-speed ski bite on LakeIce. Today `sim/sled.cpp:186`
LakeIce `mu_lat 0.22`, `c_snow 20000`, `phi 12`, `rho_eff 0`, `mu_brake 0.24`; carbide/runner bite is
not a separate term. Candidate = a speed-shaped ski bite on ice (one dial, identity 0), measured against
his tape 90 (90 s of ice, zero rollovers) as the baseline; surface-gated so Road/Trail/Bush do not move.

### Carried from the audit
`class_blend_m` A/B (road-repair's, R2), `k_air_shift` value, mouse CPI, brakes 0.80 g, and the
sled-audio bug (no engine sound in this exe and in seads-recon).
