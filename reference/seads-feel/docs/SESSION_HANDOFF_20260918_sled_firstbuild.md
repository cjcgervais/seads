# SESSION HANDOFF — THE SLED FIRST BUILD (B0 · B1 · B2a · B2b · B3)

**2026-09-18.** Lane `D:/seads_sandboxes/sled-ride-b1`, branch `feel/sled-ride-firstbuild`,
off `origin/main` **533c86409**. Spec: `docs/SLED_RIDE_AUDIT_20260918_ADDENDUM.md` §D and
`docs/sled_audit/ladder_v2.md` §4, both read whole in the read-only audit lane
`D:/seads_sandboxes/sled-audit` before a line was written.

**NOTHING IS PUSHED. NOTHING TOUCHES MAIN. This is the DRIVE build, not the landing build.**
Five env dials, zero `config/` edits, zero loader edits, zero `render/` edits, zero goldens moved.

**THE PLAY EXE, ABSOLUTE, THE ONE THIS LANE ACTUALLY BUILT:**
`D:/seads_sandboxes/sled-ride-b1/build-play/seads.exe`

---

## ⚠⚠ READ THIS FIRST — THIS DOCUMENT HAS BEEN FOLDED ONCE, AND TWO NUMBERS MOVED

The build was red-teamed twice (`docs/REDTEAM_20260918_sled_firstbuild_mechanism.md` and
`..._law-feel.md`) and **every P0 and P1 from both lenses is folded** in commit `eae9829ad`. Two
things in the sections below were **wrong** and are corrected rather than deleted, because the
correction is the record:

1. **§2.2's closing ruling — *"NO CEILING WAS REACHED. B1 = 0.15 needs no reduction; nor does
   1.0"* — IS STRUCK.** It was written on a fixture whose arms cannot differ. See **§2.2b**. Until a
   wall exists, **B1's ceiling is Chad's seat**.
2. **§4's drive checklist is SUPERSEDED BY §6**, and **B3's ladder moved from 0.15 / 0.3 / 0.5 to
   0.4 / 0.8 / 1.4** — not a retune, a **rescale**: the shed now reads the roost and the drivetrain
   instead of the bare slip, and the new numbers reproduce §4.5's *own* arithmetic. See **§5.2**.

**§5 is the fold log** (every finding, before → after, with the killing mutation that was RUN).
**§6 is the drive.** **§7 is the landing path.** **§8 is the gate on the folded tip.**

---

## §1 — WHAT WAS BUILT

| | dial | identity | env var | files touched |
|---|---|---|---|---|
| **B0** | `params.traction_mu` | `0.0` | `SEADS_SLED_TRACTION_MU` | `app/main.cpp` only |
| **B1** | `comfort.rolled_throttle_frac` | `0.0` | `SEADS_SLED_ROLLED_THROTTLE` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |
| **B2a** | `comfort.right_assist_max_ms` | `1.3888888888888888` | `SEADS_SLED_RIGHT_MAXSPD` | `app/main.cpp` only |
| **B2b** | `comfort.right_stand_shift_frac` | `1.0` | `SEADS_SLED_STAND_SHIFT` | `app/main.cpp` only |
| **B3** | `params.track_lat_slip_shed` | `0.0` | `SEADS_SLED_TAILSHED` | `sim/sled.h`, `sim/sled.cpp`, `app/main.cpp`, `test/harness/sled_tape.h` |

**B1, the split** (`sim/sled.cpp:291-307`). The one ternary became two statements, because the two
halves are different facts:

```
const double thr_in = hands_on ? clamp01(in.throttle) : 0.0;
const double throttle = s.rolled ? p.comfort.rolled_throttle_frac * thr_in : thr_in;
```

`!hands_on` keeps forcing an unconditional zero (R4a §7.4) — not this dial's business.
`frac * thr_in`, never `(1 - frac) * thr_in`; the complement form delivers FULL throttle at the
identity, and there is a leg whose only job is to red that slip.

**B3, the hoist.** The six lines that build the track's longitudinal slip (`engage_lo`, `engage_hi`,
`clutch_blend`, `drive`, `v_cmd`/`v_back`/`v_track`, `trk_slip`) moved from inside `if (g.is_track)`
to patch scope just above the lateral bite. The old block now **reads** `clutch_blend`, `drive`,
`v_track`, `trk_slip` — it does not recompute them. **Tripwire** (ladder_v2 §4.3's corrected form)
reads **1**, and `roost_thrust`'s `v_rel` at `sim/sled.cpp:1222` is untouched.

⚠ The tripwire pattern must never be written verbatim in a comment — a first draft quoted it in the
stub comment and the tripwire read 2 against a correct edit. The comments are now worded around it
and say so.

**B3, the branch** (`sim/sled.cpp`, after `mu_l` is assigned and before the `tanh`):

```
if (g.is_track && p.track_lat_slip_shed > 0.0)
    mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed * std::abs(trk_slip) * (1.0 + align_m));
```

`align_m`, never the raw signed lean (a wrong-way lean is never a penalty). `g.is_track` is
load-bearing. A branch, so `0.0` is the shipped expression byte for byte.

**The roster law, paid in the same commit.** `X(rolled_throttle_frac)` → `SLEDTAPE_COMFORT_D`,
`X(track_lat_slip_shed)` → `SLEDTAPE_PARAMS_D` (`test/harness/sled_tape.h`). `traction_mu` and
`right_assist_max_ms` were already rostered.

**The app half** (`app/main.cpp`, immediately after `SEADS_GRIP_CAPACITY` and **before**
`sim::SledState sled;`, so the tape records the flown value). Env parsing is `strtod` with a
full-string check: **a non-numeric value warns to stderr and KEEPS the default** — `atof("abc")` is
`0.0`, and `0.0` is the identity for three of the five, so a typo would have looked exactly like
"the dial did nothing".

**★ FOLDED (P2-7 / P2-3 / P3-10):** the no-conversion test now runs **first** (the trailing-space skip
ran before it, so `SEADS_SLED_STAND_SHIFT=" "` silently wrote **0.0** — not the identity for
`right_stand_shift_frac` or `right_assist_max_ms`); `std::isspace` replaces the `' '`-only skip;
`nan`/`inf` are rejected; an out-of-band value **warns and still applies** (the kill-switch
precedent); and `env_dial` returns a **bool** so the banner's armed list is built from
**acceptance**, never from `getenv` presence. MEASURED live on the play exe:
`SEADS_SLED_TRACTION_MU=nan SEADS_SLED_STAND_SHIFT=' ' SEADS_SLED_TAILSHED=0,15` → three warnings,
three defaults kept, banner reads `(env: none -- identity, bit-identical to main)`.

One banner line always prints:

```
[config] sled first-build: traction_mu X rolled_throttle_frac X right_assist_max_ms X
         right_stand_shift_frac X track_lat_slip_shed X (env: ...)
```

With nothing armed the `(env: ...)` field reads `none -- identity, bit-identical to main`.

---

## §2 — THE PROOFS, WITH OUTPUT

### 2.1 THE HOIST PROOF — run BEFORE the branch was written, as §4.3 requires

`build/seads_sled_probe.exe tape D:/flight_sim2/seads-recon/build-play/sled_tape_<N>.sledtape`,
N = 86…91, tapes opened read-only, nothing written to his fly tree.

**⚠ REPORT AGAINST THE SPEC, AND IT IS WHY A BASELINE CONTROL WAS RUN FIRST.** §D asks that each of
the six print `VERDICT: bit-exact` **and** `dial gap: none`. **Three of the six do not replay
bit-exact on the LANE BASE, before a single line was edited.** Measured on a clean build of
`533c86409`:

| N | baseline (unmodified `533c86409`) | exit |
|---|---|---|
| 86 | replayed 11430/11430 · dial gap: none · **VERDICT: bit-exact** | 0 |
| 87 | replayed 14272/14272 · dial gap: none · **VERDICT: bit-exact** | 0 |
| 88 | replayed 1837/11896 · GROUND KEY MISMATCH at sample 186528 · FIRST DIVERGENCE tick 46589 field `position.x` · **NOT bit-exact** | 1 |
| 89 | replayed 6355/15256 · FIRST DIVERGENCE tick 68315 field `velocity.x` · **NOT bit-exact** | 1 |
| 90 | replayed 12990/12990 · dial gap: none · **VERDICT: bit-exact** | 0 |
| 91 | replayed 14833/20734 · FIRST DIVERGENCE tick 14832 field `velocity.x` · **NOT bit-exact** | 1 |

The cause is in the tape headers, not in this lane: 86–90 were driven on
`kernel-v17-tremor-signed-33-g4b8c68698` and 91 on `…-53-g7b377c6f0`, neither of which is this
lane's base. **So the hoist proof was run as a DIFFERENTIAL, which is strictly stronger than the
pass/fail form**: the hoist must not change the replay result, including not changing *where* the
three already-divergent tapes diverge.

**HOIST-ONLY BUILD (the six lines moved, no new field, no branch, no roster entry):**

| N | probe output vs baseline | verdict |
|---|---|---|
| 86 | **byte-identical** | bit-exact, dial gap: none |
| 87 | **byte-identical** | bit-exact, dial gap: none |
| 88 | **byte-identical** | same divergence, same tick 46589, same field |
| 89 | **byte-identical** | same divergence, same tick 68315, same field |
| 90 | **byte-identical** | bit-exact, dial gap: none |
| 91 | **byte-identical** | same divergence, same tick 14832, same field |

**Six of six byte-identical. The hoist is a pure refactor, measured.**

**FINAL BUILD (all five dials, both new fields rostered), env unset:** replay lines, divergence
ticks, divergence fields and `rolled` event counts are **byte-identical to baseline on all six**.
The **only** difference anywhere in the output is the dial gap, which now honestly names the two
dials that did not exist when Chad drove:

```
!! DIAL GAP: 2 dial(s) this build declares are ABSENT from the tape and took TODAY'S
  struct defaults, not the values in force when it was driven:
   rolled_throttle_frac, track_lat_slip_shed
VERDICT: bit-exact ON A KERNEL THIS TAPE NEVER SAW -- it reproduces, but 2 dial(s)
  above were supplied by this build, not by the drive.
```

**That is `dial_gap` doing its job, not a regression** — §4 predicted it in these words ("`dial_gap`
reports the new names honestly"). `dial gap: none` is only obtainable on a build that does not
declare the new dials, which is the hoist-only build above. Both tables are recorded so neither
claim has to be taken on trust.

### 2.2 THE WALL — `sled_onside_recovery_is_momentum_not_magnetism` at B1 = 0.15

**Run literally**, by temporarily moving the struct default to `0.15`, rebuilding, running the
existing leg, and restoring: **PASSED** (1 assertion, 1 test case).

**⚠ AND IT PASSED VACUOUSLY, WHICH IS THE FINDING.** That leg drives `const sim::SledInputs idle;`
— the thumb is closed — so it cannot see this dial at all. Proof: its own report-only printfs are
**character-identical** at `0.0` and at `0.15`:

```
final_tilt_deg(5) = 106.22   (10) = 106.21   (15) = 106.20
             (20) = 105.94   (25) = 110.71   (30) = 107.28
```

So a new leg was written to carry the wall — same 110° drop, same parked/sliding arms, **W HELD**,
at 0.0 / 0.15 / 1.0 and v_side 0 / 5 / 15 m/s.

### ⚠⚠ 2.2b — AND THAT LEG IS DARK TOO. THE RULING WRITTEN ON IT IS STRUCK.

**FOLDED RED-TEAM P0-1 (law+feel) / P1-1 (mechanism), 2026-09-18.** This section first ended with the
sentence *"NO CEILING WAS REACHED. B1 = 0.15 needs no reduction; nor does 1.0."* **THAT SENTENCE IS
REMOVED AND MUST NOT BE QUOTED.** It was written on a fixture whose arms cannot differ.

MEASURED, all nine cells, the assertion expansions at full precision:

```
106.21816119284737567   106.21708989882516505   106.20226219859530659   <- frac 0.00
106.21816119284737567   106.21708989882516505   106.20226219859530659   <- frac 0.15
106.21816119284737567   106.21708989882516505   106.20226219859530659   <- frac 1.00
```

**Three distinct numbers, one per `v_side`, ZERO per `frac`, identical to the 17th digit**, with
`thrust_n` 0.00 N and `roost_flux` 0.00000 in every cell. The leg indicted `:2649` for vacuity (its
thumb is closed) and then committed the same error one fixture over: **`:2649` is dark because the
thumb is shut; this one is dark because the track is in the air.** Opening the thumb changed which
readout moved (`engine_rpm`, computed OUTSIDE the contact block) and changed nothing the wall is
about.

**AND THE WALL WAS HUNTED BEFORE THIS WAS ACCEPTED.** The mechanism pass looked for any fixture that
arms it: `cross_slope_field` at 0.30 and 0.60 (`thrust=0.00 roost=0.00000 slip=+0.0000` at frac
0/0.15/1.0 — so §D's hope that run 3(b), the bank strike, would arm it in a test is wrong too), and a
side-slide recovery window at 15/20/25/30 m/s over 1800 ticks (`max|T| = 0.00`, **zero ticks with
`rolled` && |thrust| > 1 N**). **24 cells. The track patch never once entered the thrust block while
rolled.**

**THE CORRECTED STATEMENT:** *no ceiling was **measured**, because no fixture in this build puts a
downed machine's track on the ground. The 0.15 candidate stands on §D's arithmetic, not on a wall.*
**UNTIL A WALL EXISTS THIS VALUE'S CEILING IS CHAD'S SEAT**, and his sheet says so.

The leg is renamed **`sled_rolled_throttle_is_dark_on_flat_ground`** and now PINS the darkness —
`REQUIRE(s.thrust_n == 0.0)` and `REQUIRE(s.roost_flux == 0.0)` in every cell — so the day a fixture
or a kernel change gives that track a face to push on, the gate reds and somebody has to come back
and write the wall. `sim/sled.h:225-228`'s WALL paragraph is replaced by the measurement.

### 2.3 ⚠⚠ THE FINDING THAT CHANGES WHAT RUN 3 CAN ANSWER — READ THIS BEFORE HE DRIVES

Measured sweep, flat analytic ground, `rolled` at 70° / 76° / 80° / 90° / 110° / 178°, W held, dial
at 0.00 / 0.15 / 0.50 / 1.00:

| readout | 0.00 | 0.15 | 0.50 | 1.00 |
|---|---|---|---|---|
| `engine_rpm` | 1700.0 | 2645.0 | 4850.0 | 8000.0 |
| `track_slip` | 0.000 | 0.000 | 0.000 | 0.000 |
| `roost_flux` | 0.00000 | 0.00000 | 0.00000 | 0.00000 |
| `thrust_n` | 0.00 | 0.00 | 0.00 | 0.00 |
| `sink_m[track]` | 0.000 | 0.000 | 0.000 | 0.000 |
| final tilt | 106.22° | 106.22° | 106.22° | 106.22° |
| ground speed | 0.004–0.011 m/s | identical | identical | identical |

**The dial reaches the engine and NOTHING ELSE.** On a downed machine on flat ground the track patch
has no ground contact, so the whole `if (g.is_track)` thrust block never runs: **no roost, no
thrust, no motion, at any value up to 1.0.** The same sweep with the machine upright (0°–60°) shows
the normal WOT numbers (`thrust_n` 2272 N, `roost_flux` 0.238), so the instrument works.

**What this means for the drive checklist.** Run 3(a) as written — *"roll her over at walking pace,
no bank, no speed, hold W for two full seconds"* — **will produce the engine note and nothing else,
at any value.** That is not a broken dial; it is where a track on flat ground has nothing to push
on. Run 3(b), the bank strike, is the run that can answer the second question, because a bank face
is the only thing a sideways track can find. **Chad should be told this before he drives, or he will
read run 3(a) as "the dial does nothing" and stop.**

Whether the *noise alone* is what he was asking for is still exactly his call, and the sound and HUD
do follow `engine_rpm`.

### 2.4 B2b — the pendulum, and a report against §4.4's positive control

`selfright_a_timed_rock_beats_a_mistimed_one`, from 178°, same STAND cadence (1.0 s on / 0.7 s off),
`lean_lat` a square wave keyed to the sign of `angular_vel.z`:

```
[B2 pendulum] frac=1.00  timed=  1.48 s  mistimed=  1.77 s
[B2 pendulum] frac=0.50  timed=  1.48 s  mistimed=  2.06 s
[B2 pendulum] frac=0.25  timed=  1.51 s  mistimed=  NEVER (ends at 107.7°)
[B2 pendulum] frac=0.00  timed=  1.57 s  mistimed=  NEVER (ends at 107.4°)
```

**Two reports.**

1. **THE SIGN CONVENTION IS THE OPPOSITE OF THE OBVIOUS ONE.** Body +Z is BACKWARD (body −Z is
   forward, SPEC §7), so a positive `angular_vel.z` is a roll toward NEGATIVE `lean_lat`. The arm
   that adds energy to the swing is `lean_lat = -sign(angular_vel.z)`. A leg written the naive way
   would assert that the *mistimed* arm is faster and would be permanently red for a reason that has
   nothing to do with the mechanism. Measured, named in the leg, not assumed.
2. **§4.4's POSITIVE CONTROL IS FALSE AS WRITTEN.** It predicts *"at 1.0 it should show **no**
   discrimination."* **It shows 0.29 s of discrimination at 1.0.** The reason is in the code:
   `sim/sled.cpp:417-421` is
   `lat_target_sr = clamp(target_lat_m + right_shift_cmd * frac * lean_lat_stand_m, ±lean_lat_stand_m)`
   — his lean is **added** to the brace, never **replaced** by it, so at 1.0 the brace can only
   saturate the clamp on one side. The discrimination shrinks; it does not vanish.

So the leg does **not** assert the false control. It asserts the rung's real claim, which the data
supports strongly: **the advantage of a timed rock GROWS as the brace is turned down** — 0.29 s at
1.0, 0.58 s at 0.5, and at 0.25 and 0.0 a mistimed rock never rights her from inversion at all.

### 2.5 THE SUBSET GATE

```
ctest --test-dir build -C Debug -R "sled|rider|tape|selfright" -j4
98% tests passed, 4 tests failed out of 168     (341.16 s)
```

The four are, **by name**, exactly the four `snow`-lane entries in the committed known-red set
`generated/gate/known_reds.txt`:

```
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

**Proven pre-existing, not inherited on faith:** the four were re-run on the lane with every change
stashed, and they fail with **byte-identical numbers** (0.42257279222223798 / 2.62256439833490473 /
341.37576434061895725 / `saw_release_under_one == false`).

`python tools/gate/gate_baseline.py check build/subset_firstbuild.log` → **"No new reds."** It also
reports the two `ai`-lane baseline reds as "now PASSING"; they are not — this is a **subset** log and
the `-R` filter never selected them. The baseline was **not** re-recorded.

---

## §3 — THE LEGS, EACH WITH ITS KILLING MUTATION

| leg | file | asserts | KILLED BY |
|---|---|---|---|
| `sled_rolled_throttle_frac_zero_is_the_shipped_zero` **(new)** | `test_sled.cpp` | rolled, hands on, W held, dial 0.0, `v_bf ≤ 3.0` asserted not assumed ⇒ `engine_rpm == 1700.0` exactly, belt within 1e-3 of `max(v_bf,0)` and `< 1.0` | `(1 - frac) * thr_in` instead of `frac * thr_in` — the complement delivers FULL throttle at the identity: rpm 8000, belt 46 m/s |
| `sled_rolled_throttle_never_reaches_a_handless_rider` **(new)** | `test_sled.cpp` | grip broken, dial **1.0**: W held and W closed give the **same machine bit for bit**, rpm stays 1700 | collapsing the two statements back into one ternary, dropping the `hands_on` guard |
| `sled_rolled_throttle_is_dark_on_flat_ground` **(new; renamed in the fold from `..._the_wall_at_the_driven_value`)** | `test_sled.cpp` | **NOT A WALL — it pins the DARKNESS.** 110°, **W HELD**, frac 0.0/0.15/1.0 × v_side 0/5/15: she stays over, and `thrust_n == 0.0` / `roost_flux == 0.0` in every cell. `rpm == 1700 + frac*6300` proves the dial was live | any change that lets a rolled machine's track develop thrust on flat ground — and then the ruling must be re-taken and a real wall written. Also killed by the dial failing to reach the engine |
| `sled_rolled_throttle_is_clamped_at_use` **(new, fold)** | `test_sled.cpp` | in band (0.0/0.15/0.5/1.0) `rpm == 1700 + frac*6300` exactly; out of band (1.5/2.5/100) she is the frac == 1.0 machine **bit for bit** | dropping `clamp01` from the product. ⚠ WRITTEN ON `engine_rpm` ALONE IT PASSED ITS OWN MUTATION (`rpm_frac` re-clamps) — the observable had to be widened to `same_state` |
| `sled_tail_shed_keeps_the_slide_before_the_tip` **(new, fold P0-2)** | `test_sled.cpp` | 12 cells × arms 0.0/0.4/1.4: nothing latches `rolled`, she is visibly turning, and the armed/unarmed max-roll ratio stays under 1.03 (worst measured **1.0238**) | a shed value that lets the track's lateral mu fall far enough to trip her, or a dial that quietly makes the flat-snow roll worse |
| `sled_tail_shed_is_dark_where_there_is_no_roost` **(new, fold P1-3)** | `test_sled.cpp` | on a plowed road (`roost_flux == 0.0`, `|track_slip| = 0.394`) the arms are **bit-identical**; on snow the same inputs separate by **6.73 m** | writing the shed on `std::abs(trk_slip)` instead of `trk_flux` |
| `sled_tail_shed_is_dark_with_the_thumb_shut` **(new, fold P1-2)** | `test_sled.cpp` | thumb closed at 1.5/2.0/3.0 m/s, where `|track_slip|` peaks at 1.00000 and `roost_flux` at 0.16–0.20: arms **bit-identical**; the open-thumb control separates by 0.31 m | deleting the `(drive + (1-drive)*clutch_blend)` factor from the shed |
| `sled_tail_shed_hoist_is_a_pure_refactor` **(new)** | `test_sled.cpp` | hermetic 600-tick steered+leaned drive with the dial absent, final position/velocity/orientation pinned as an exact golden | a second copy of the slip formula at the old site, or hoisting past anything that writes `throttle`/`v_fwd` — either moves this state in the 1st–2nd significant figure |
| `sled_tail_shed_is_track_only` **(new)** | `test_sled.cpp` | with `track_lat_mu = 0.0` the dial is arithmetically inert on the track, so any difference can only come from a **ski**: 300 ticks WOT + steer + lean, dial 0.0 vs 0.5, **bit-identical** | dropping the `g.is_track` guard — the skis shed, the carve moves, the arms diverge |
| `sled_tail_shed_is_inert_in_a_straight_line` **(new, and it carries a report)** | `test_sled.cpp` | straight-line separation `9.78e-16 m` bounded under `1e-12`, beside `14.05 m` with a lean of 1.0 on the identical fixture and dial — a **separation**, not an epsilon | the shed applied as an additive term on the bite instead of a multiplier on `mu_l` (an additive form survives a zero slip angle and moves the straight arm by metres) |
| `selfright_a_timed_rock_beats_a_mistimed_one` **(new)** | `test_sled_selfright.cpp` | the timed-rock advantage **grows** as the brace falls: gap(0.25) > gap(0.5) > gap(1.0), and gap(0.5) > 0 | `right_stand_shift_frac` not reaching the rider's lateral target (an override writing a field nobody reads) ⇒ the gaps come out equal. Also killed by a fixture holding a CONSTANT lean — that measures which *side* he picked, not *when* he went there |
| `sled_wrong_way_lean_is_never_a_penalty` (existing) | `test_sled.cpp` | **unmoved** — green in the subset | the raw signed lean instead of `align_m` |
| `sled_track_lat_mu_is_the_rostered_value` (existing) | `test_sled.cpp` | **discharged, no action** — it builds `const sim::SledParams p;`, the shed is 0.0 inside it | — |
| `sled_tape_round_trip_replays_bit_identical` (existing) | `test_sled_tape.cpp` | green — a new rostered dial survives the text round trip | removing a roster entry ⇒ the drive can disagree with its own tape |

**⚠ THE ONE PROOF OBLIGATION THAT COULD NOT BE DISCHARGED AS SPECIFIED.** §4.4 requires
`sled_tail_shed_is_inert_in_a_straight_line` on "a hermetic flat fixture … where `slip_ang == 0`",
and says: if the fixture crabs, **report the crab, do not relax the leg**. **It crabs, and no
fixture reachable through `step_sled` does not.** Measured: zero steer, zero lean, WOT — the arms
separate on **tick one** by `3.4e-20 m`, reaching `9.8e-16 m` / `4.3e-15 m/s` at 600 ticks. The track
patch's `v_lat` is ~`1e-17 m/s`, not 0, because `fwd_t`, `right_t` and `v_patch` are built by
`normalize` and `cross` at a radius of `6.371e6 m` — an exactly-zero slip angle **is not
representable there**, so an exact-identity leg on it would be a permanently red gate.

**So the crab is reported, here and in the leg's own comment, and the leg asserts a SEPARATION
against arms that really do differ** (`9.78e-16 m` straight, `14.05 m` leaned, `4.85 m` at steer
0.15 — fifteen orders of magnitude), with a bound twelve orders below the smallest real effect
measured. That is not an epsilon hiding a force. **The straight-line shed is REAL and is the part
that can make a bank strike worse; this leg does not say otherwise, and its comment says so in those
words.**

---

## §4 — THE DRIVE — **SUPERSEDED BY §6. DO NOT DRIVE FROM THIS SECTION.**

This section carried the pre-fold checklist. **Two of its numbers are now wrong** and it is left here
only so nobody wonders where it went:

* it laddered B3 as **0.15 / 0.3 / 0.5**. After the red-team fold the dial reads the roost and the
  drivetrain instead of the bare slip, which rescales it: **the ladder is 0.4 / 0.8 / 1.4** (§5.2).
* it inherited the struck sentence about B1's ceiling (§2.2b).

**THE DRIVE CHECKLIST IS §6**, with the seven `.bat` launchers, the absolute exe path, the four
measured B2b rows and the two owed B3 answers.

---

## §5 — THE RED-TEAM, AND THE FOLD LOG

Two fresh-context adversarial passes ran against the pre-fold tip `58f6e4dd7`, both read the spec
whole first, both measured against this lane's own gate build:

* `docs/REDTEAM_20260918_sled_firstbuild_mechanism.md` — **MECHANISM + MUTATION LENS.** Verdicts:
  B0 LAND, B1 LAND-WITH-FIX, B2a LAND, B2b LAND, **B3 LAND-WITH-FIX for the drive and
  DO-NOT-LAND as a kernel law in this form.** It reverted every source mutation it made and
  confirmed `git diff` empty.
* `docs/REDTEAM_20260918_sled_firstbuild_law-feel.md` — **LAW + FEEL LENS.** Verdicts: B0
  LAND-WITH-FIX (one line on his sheet), B1 LAND-WITH-FIX, B2a LAND (no finding), B2b
  LAND-WITH-FIX (on his sheet, not in code), B3 LAND-WITH-FIX (the fix is a leg written **before**
  run 6). It wrote nothing outside its own report.

**Both passes agree the identity claim is sound and is the strongest part of the diff**, and neither
could construct a reachable state where the no-env exe's simulation differs from main.

### 5.1 THE FOLD LOG — every P0 and P1 from both lenses, with before → after

**Every fold below names a killing mutation and THE MUTATION WAS RUN.** Six mutations were applied to
`sim/sled.cpp`, rebuilt and measured; all six red. A seventh (`M4`) did not compile and was replaced.

| # | finding | BEFORE | AFTER | mutation run |
|---|---|---|---|---|
| **P0-1** law+feel / **P1-1** mech | B1 has no wall; a ruling was written on a fixture whose arms cannot differ | `sled_rolled_throttle_the_wall_at_the_driven_value`; handoff §2.2 ended *"NO CEILING WAS REACHED. B1 = 0.15 needs no reduction; nor does 1.0"*; `sim/sled.h:225-228` named `:2649` as THE WALL | the sentence is **struck** (§2.2b carries the correction and the 24-cell hunt); the leg is renamed **`sled_rolled_throttle_is_dark_on_flat_ground`** and PINS `thrust_n == 0` / `roost_flux == 0`; `sim/sled.h` says **UNTIL A WALL EXISTS THIS VALUE'S CEILING IS CHAD'S SEAT** | the pin itself: any change that lets a rolled track develop thrust on flat ground reds it |
| **P0-2** law+feel | B3's worst case had no green leg at any armed value, and its fence is a silenced baseline red | nothing; `sled_slides_before_it_tips_on_flat_snow` is in `known_reds.txt` **and** blind to the dial (`const SledParams p;`) | **`sled_tail_shed_keeps_the_slide_before_the_tip`** — hermetic, 12 cells × arms 0.0/0.4/1.4, DIFFERENTIAL not :1074's absolute bound. MEASURED: nothing latches `rolled` at any value, worst armed/unarmed roll ratio **1.0238**, several cells get *better* | a shed that trades roll for drift on flat snow blows the 1.03 ratio by a wide margin |
| **P1-2** mech | the shed fired at full strength with the thumb closed | `mu_l *= max(0, 1 − shed·\|trk_slip\|·(1+align_m))` | `× (drive + (1−drive)·clutch_blend)` — the **same** decouple the thrust uses at `:1277` | **M1** delete the factor → `sled_tail_shed_is_dark_with_the_thumb_shut` **RED** |
| **P1-3** mech | the dial is named for the roost and does not read it | `\|trk_slip\|`, surface-blind — full-strength shed on a plowed road where `roost_flux == 0` by construction | `trk_flux = \|trk_slip\| · avail`; `bury`/`loose`/`avail` hoisted with the slip; `flux` becomes the ONE NUMBER's **third** consumer | **M2** read the bare slip → `sled_tail_shed_is_dark_where_there_is_no_roost` **RED** |
| **P1-3** law+feel / **P2-1** mech | `sled_tail_shed_is_track_only` cannot red on its named mutation, and has no positive control | both arms at `track_lat_mu = 0.0` (the dial inert in both); `sim/sled.cpp:1141-2` and `sim/sled.h` claimed the leg reds if the guard is dropped | a **positive control** at the shipped `track_lat_mu = 0.70` (sep 1.74 m) runs first; the false claim is struck in both files; the real killing mutations are named (machine-level slip, or hoisting the slip block out of its own `if (g.is_track)`) | **M6** dial forced not to reach the kernel → **RED** (the control) |
| **P1-4** law+feel | B1's product is never re-clamped; the env path has no range check the TOML path has | `s.rolled ? frac * thr_in : thr_in` — `SEADS_SLED_ROLLED_THROTTLE=1.5` put 1.5 into `v_cmd`, `engine_rpm` (11150), the moment arm, the HUD | `clamp01(frac * thr_in)` — the identity function for every `frac ≤ 1`; plus a warn-and-still-apply band in `env_dial` (the `SEADS_GRIP_CAPACITY` precedent: a huge value is the kill switch, so a refusal would be wrong) | **M5** drop `clamp01` → `sled_rolled_throttle_is_clamped_at_use` **RED** |
| **P1-5** law+feel | B2b is a subtraction sold as a gain; the leg's own rows say so and his sheet does not | four rows in §2.4 only | the four rows are on **his sheet above run 5** with the honest sentence and the owed ruling (§5.2 #2); the leg gains a **third free-running human-cadence arm** (`phase == 0`, phase-locked to the press, blind to `angular_vel.z`) | the two new REQUIREs: a dial that only works for a 120 Hz controller reds them |

**FOLDED TOO, because they live inside the very functions the P1s touched** (recorded so nobody
thinks they were carried):

| # | finding | AFTER |
|---|---|---|
| **P2-7** | `env_dial` validated in the wrong order — the trailing-space skip ran BEFORE the no-conversion test, so `SEADS_SLED_STAND_SHIFT=" "` passed validation and wrote **0.0**, which is not the identity for `right_stand_shift_frac` (1.0) or `right_assist_max_ms` (1.3889) | `end == e` tested FIRST; `std::isspace` not `' '`; `!std::isfinite` rejected (`strtod` accepts `nan`/`inf`) |
| **P2-3 / P3-10** | the banner reported env **presence**, not **acceptance** — a rejected `0,15` and an empty value both printed as armed | `env_dial` returns `bool`; the banner is built from the return value; the empty case now warns |
| **P2-2** | the hoist golden's killing mutation is false (a re-added second copy of the slip formula prints character-identical, MEASURED) | restated: it is a forward regression net, **the hoist proof is §2.1's six-tape differential**; its toolchain (Windows 11 / GCC / Ninja / Debug / vendored GLM) is named in the comment |
| **P2-5** | nothing pinned `align_m` inside the new term | a pin on `sled_tail_shed_is_inert_in_a_straight_line`. ⚠ **AND A REPORT AGAINST THE RED-TEAM'S OWN FORM** — see §5.3 |

### 5.2 THE CONSEQUENCE OF THE FOLD THAT CHANGES HIS DRIVE — READ THIS ONE

**THE B3 LADDER MOVES. 0.15 / 0.3 / 0.5 ARE DEAD; THE LADDER IS 0.4 / 0.8 / 1.4.**

Multiplying by `flux` and by the drivetrain decouple **rescales the dial**. `avail` runs ≈ 0.23 at WOT
on 0.30 m snow (MEASURED `roost_flux` 0.230261 straight, 0.230372 leaned), so the old candidates buy
about a quarter of what §4.5 derived them to buy. Re-derived on **§4.5's own arithmetic** — a tenth of
the track's lateral grip in a straight line at the first candidate, a third at the last:

| shed | straight-line loss | loss in a developed yaw | leaned separation over 10 s |
|---|---|---|---|
| **0.4** | 9.2 % | 18.4 % | 1.64 m |
| **0.8** | 18.4 % | 36.9 % | 3.92 m |
| **1.4** | 32.2 % | 64.5 % | 10.77 m |

The env band for `SEADS_SLED_TAILSHED` is widened to **[0, 5]** for this reason: **it stopped being a
fraction when it started reading one.** A value outside the band still applies and still warns.

**AND THE MECHANISM PASS'S SPIN-OUT FINDING DOES NOT SURVIVE THE FOLD.** P2-6 measured a corridor run
(WOT / steer 0.6 / lean 0.7, 5 s from 10 m/s) ending at **1.42 m/s with yaw −1.48 rad/s** at the old
`shed = 0.5` — a spin. On the folded form the *same* corridor reads **18.20 m/s at 0.5** and **17.54
m/s at 3.0**, and never latches `rolled`. Its 14.05 m straight-line separation at 0.5 is now 2.13 m.
Those three numbers are corrected here rather than carried onto his card, because carrying them would
have told him a spin was expected where it is not.

### 5.3 REPORTS AGAINST THE RED-TEAM ITSELF (both measured, both in the legs' own comments)

1. **P2-5's proposed pin is not an identity.** It asks for `sep(0.15, −1.0) == sep(0.15, 0.0)`. Both
   arms do have `align_m == 0`, so the shed FACTOR matches — but the two arms fly different
   trajectories (the lean moves `lean_bite_gain`, the ski plate and the weight transfer), so their
   separations are different numbers and the equality reds on a correct kernel.
   **Worse, two further forms of the pin PASSED their own killing mutation before the third worked:**
   (a) on a lightly-steered fixture a wrong-way lean **is not reachable at all** — the LEAN drives the
   yaw, so `lean_frac · sign(w_up)` comes out positive and the machine is always leaning into the turn
   it is causing; only a FULL-LOCK steer owns the yaw sign, which is the construction `:2546` already
   uses and explains; (b) a 20-tick window is inside `align_m`'s own ramp (it needs |w_up| > 0.02
   rad/s and saturates at 0.07), so both forms are still at `align == 0` there and the mutation
   changes nothing. The pin is now a **ratio over a developed full-lock corner**: no-lean separation
   **1.02962 m**, wrong-way separation **0.81365 m**, ratio **0.7902**. MUTATION RUN: raw signed lean
   → **RED**; lean-gated fallback (`(1.0 + align_m)` → `align_m`) → **RED**.
2. **P0-1's third fix cannot be discharged.** Law+feel asks for a wall "on a slope or against a step".
   The mechanism pass had already hunted exactly that across 24 cells (`cross_slope_field` 0.30/0.60,
   the side-slide recovery window at 15/20/25/30 m/s over 1800 ticks) and found `thrust = 0.00` and
   **zero** ticks with `rolled` && |thrust| > 1 N. The first two parts of the fix are done; the third
   is **reported as not achievable in this build**, which is why `sim/sled.h` now says the ceiling is
   his seat rather than promising a leg.

### 5.4 CARRIED, NOT FOLDED — the P2 and P3 list, so nobody rediscovers them

**P2 (law+feel)**
* **P2-6** — B0 is the only dial in this build that TAKES AUTHORITY AWAY, its fence
  (`sled_grip_ceiling_stays_below_the_tip_threshold`) is a silenced baseline red, and no leg
  exercises `traction_mu` at 3.0. *Partly answered on his sheet:* run 2 now carries the governor
  question in his language. **No leg was written.**

**P2 (mechanism)**
* **P2-6** — "0.5 is a spin-out value and the card does not say so." **Superseded by the fold**
  (§5.2): the three numbers behind it do not hold on the shipped form. The card carries the new
  ladder instead.

**P3 (both lenses)**
* **P3-1** — NaN-throttle identity edge: `0.0 * NaN = NaN` where the old ternary returned `0.0`.
  Unreachable from the keyboard; `−0.0` traced through `v_cmd`/`drive_t`/`rpm_frac` and is benign.
* **P3-2** — *partly folded*: the kernel now clamps, but nothing stops a silly env value being
  applied (deliberately — it is the kill switch), and the band warning is a `fprintf`, not a gate.
* **P3-3** — in the pendulum leg `at_quarter` is `16.0 − timed` because the mistimed arm never
  rights, so `REQUIRE(at_quarter > at_candidate)` is a statement about `total_s` as much as about the
  mechanism. **Carried:** the discrete fact (`mistimed.t_right < 0` at 0.25) is printed but not
  asserted.
* **P3-4** — "wheelie/drift/backflip untouched by construction and proven by a leg": there is no leg
  *named* for it. The coverage is `tape_chad_flip_fence` (green, but it replays with both new dials
  ABSENT ⇒ struct defaults ⇒ **identity only**) plus the 2148-test gate equalling the baseline six by
  name. See §6.4.
* **P3-5** — the exe is **not byte-identical** to main: the banner `printf` is unconditional. The
  claim is that the **simulation** is bit-identical. Nothing in the gate or the tools parses that
  line.
* **P3-6** — pre-existing, not this diff: `roost_flux` is already non-zero while coasting with the
  thumb shut (MEASURED 0.15867 at 1.18 m/s). B3 now builds on it; the decouple (P1-2) is what stops
  that mattering below the engage speed.
* **P3-9 / P2-4** — **TONIGHT'S TAPES ARE LANE-LOCKED AND THIS IS THE LINE THAT SAYS SO.**
  `test/harness/sled_tape.h:459,474` **hard-fail** on an unknown `param`/`cparam`, and the writer
  emits the whole roster — so every `.sledtape` Chad cuts on this build carries
  `track_lat_slip_shed` and `rolled_throttle_frac` and **can only be replayed by a build carrying
  those two roster entries**: `D:/seads_sandboxes/sled-ride-b1/build/seads_sled_probe.exe`. It
  errors, it does not warn, and `kMagic` is unchanged so nothing signals why. The Python instrument
  (`tools/sled_tape_x1.py`) is tolerant and will still parse them. **DO NOT DELETE THIS WORKTREE OR
  ITS `build/` WHILE ITS TAPES ARE STILL THE EVIDENCE.**
* **P3-10** — *folded* (the banner half). The second half — `sled_rolled_throttle_frac_zero_is_the_
  shipped_zero` never approaching the `≤ 3.0 m/s` clause it calls load-bearing (MEASURED `v_bf =
  0.000707`) — is **carried**.

### 5.5 RULINGS AND REPORTS THIS BUILD RAISES — HIS, unless marked

1. **B1 ON FLAT GROUND IS THE ENGINE NOTE ONLY** (§2.3, §2.2b). Measured, not predicted, and hunted
   for 24 cells. Does that change what he wants the dial to be, or is the bank strike the whole
   point?
2. **B2b IS A SUBTRACTION, NOT A GAIN** (§2.4, fold P1-5). The timed arm never improves — 1.48 /
   1.48 / 1.51 / 1.57 s at 1.00 / 0.50 / 0.25 / 0.00. The whole widening gap is the *mistimed* arm
   degrading. **Did you want the rock itself to PAY?** If yes, `right_stand_shift_frac` is the wrong
   dial and the rung needs a term converting his lean **rate** into righting torque — another dial,
   another night.
3. **N2-CONFLICT, SHARPENED.** On 2026-08-26 he ruled *"I dont understand why yo are clonlating the
   lean mechanism with the righting"*; B2b's whole value is that the **lean** now decides whether the
   **righting** works. It moves in R11's direction (it takes authority from the automatic brace and
   gives it to his hands) and it is press-gated, so it cannot make anything more automatic — but the
   conflict is real and run 5 carries the question.
4. **B3-vs-BANK**, after run 6's two answers. The gate now answers the flat-snow half (§5.1 P0-2);
   the bank half is a **tripped** rollover and no flat fixture reaches it.
5. **THE SHED'S FORM IS A DEVIATION FROM THE SPEC.** `ladder_v2.md` §4.3 specifies `|trk_slip|`
   alone; this lane ships `trk_flux × (drive + (1−drive)·clutch_blend)` on the mechanism red-team's
   P1-2 and P1-3. **Correct the ladder, or leave the specification standing beside the correction?**
   *(Owed to the audit lane, not to Chad.)*
6. **THE `:2649` WALL IS VACUOUS FOR B1** and so is its replacement. Should the ladder stop naming a
   wall for this dial until one exists?
7. **§4.4's POSITIVE CONTROL IS FALSE** (§2.4) — the brace ADDS to his lean, it does not replace it,
   so discrimination persists at 1.0 (0.29 s).
8. **THREE OF THE SIX v17 TAPES DO NOT REPLAY BIT-EXACT ON MAIN** (§2.1), before any edit, because
   they were recorded on sandbox tips that are not this base. **Is that a debt anybody owns?** It
   makes §D's proof obligation unrunnable in its literal form for every future lane.
9. **`slip_ang == 0` IS AN UNREACHABLE FIXTURE** (§3) at planet scale.
10. **THE TRIPWIRE CANNOT BE QUOTED IN A COMMENT** (§1) — it counts itself. Worth a line in
    `ladder_v2` §4.3 so the next lane does not lose the same hour.

---

## §6 — THE DRIVE CHECKLIST

**THE PLAY EXE, ABSOLUTE, THE ONE THIS LANE ACTUALLY BUILT:**
`D:/seads_sandboxes/sled-ride-b1/build-play/seads.exe`

**THE SEVEN LAUNCHERS, in the lane root, one per arm** — double-click, or run from a shell. Each one
sets its own env, launches that exe, and carries its own question in its header:

```
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_1_baseline.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_2_B0_traction_mu.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_3_B1_rolled_throttle.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_4_B2a_right_maxspd.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_5_B2b_stand_shift.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_6_B3_tailshed.bat
D:\seads_sandboxes\sled-ride-b1\drive_sled_firstbuild_7_all_together.bat
```

**⚠ AT THE TOP OF HIS SHEET, IN THESE WORDS:** *"If the rider is off the bars, W is MEANT to do
nothing — check the rider before you judge the dial."* (The HUD's grip readout is
`sled_grip_attached`. DERIVED: it takes about **3.8 g held for a second**, or **10 g for a sixth of a
second**, to throw him off — so on an ordinary bank clip he is still holding on and the dial should
speak.)

**⚠ AND, FROM §2.3, BEFORE HE JUDGES RUN 3:** *"On flat ground, with her on her side, W gives you the
engine note and nothing else at any setting — the track has nothing to push on. The bank strike is
the run that can answer whether you want her to MOVE."*

**⚠ AND THE TAPES:** every `.sledtape` cut on this build can only be replayed by
`D:/seads_sandboxes/sled-ride-b1/build/seads_sled_probe.exe`, until the two new fields land on main.
Main's probe, the sentinel's tree and every other lane will **error**, not warn. **Do not delete this
worktree or its `build/` while its tapes are the evidence.**

**Keys.** W throttle · S brake · **A/D bars** · **mouse = lean** (dx → lateral, dy → fore/aft; Q/E are
the keyboard lean) · **LEFT SHIFT = STAND**, LEFT CTRL = tuck · R autoright · C recentre · H swaps the
HUD.

**ONE VARIABLE AT A TIME. TAPE EVERY ONE. WATCH THE `[config] sled first-build` BANNER** — it prints
every value in force and names only the env vars this build **accepted**, so "did you have it armed?"
is never a question anybody answers from memory.

### 1 — BASELINE, nothing set · `drive_sled_firstbuild_1_baseline.bat`

Ride the way you rode last night, two minutes. **This run must feel exactly like tonight's main.** If
anything feels different, **STOP** — an identity is broken and nothing below is readable.
Banner must read `(env: none -- identity, bit-identical to main)`.

### 2 — B0, the contact ceiling, ALONE · `SEADS_SLED_TRACTION_MU=3.0`

`set SEADS_SLED_TRACTION_MU=3.0 && seads.exe`

`docs/snowform_measurements.md` §M8.1 says μ ≥ 3.0 leaves your traverse byte-identical (14.1 / 14.5 /
15.3 m/s) while killing the Bush runaway. **TWO QUESTIONS:** *did anything change at all?* — and,
**folded from the red-team**, *did anything you could do before become impossible? A climb you used to
make, a bank you used to get up.* **If yes, that outranks everything below.** This is the only dial in
the build that TAKES AUTHORITY AWAY, and it is your 2026-08-24 ruling finally being tested.

### 3 — B1, throttle while rolled, B0 still armed · `SEADS_SLED_ROLLED_THROTTLE=0.15`

`set SEADS_SLED_TRACTION_MU=3.0 && set SEADS_SLED_ROLLED_THROTTLE=0.15 && seads.exe`

**(a) FIRST, THE GENTLE ONE.** Roll her over at walking pace with your hands **on** the bars — no
bank, no speed. On your side, **hold W for two full seconds**. **EXPECT THE ENGINE NOTE AND NOTHING
ELSE**, at any value up to 1.0 (§2.3). That is not a broken dial; it is where a track on flat ground
has nothing to push on.
**(b) THEN THE BANK STRIKE**, the way it actually happens to you. A bank face is the only thing a
sideways track can find.
**THE QUESTION IS TWO QUESTIONS:** (a) is the **noise and roost alone** what you were asking for?
(b) do you want her to actually **MOVE** — and how far? A machine that drives itself upright is the
thing the kernel forbids by name, and your answer sets the value. **0.15 has no measured ceiling
above it and no measured floor below it: the ceiling is your seat.**

### 4 — B2a, the righting speed gate, ALONE · `SEADS_SLED_RIGHT_MAXSPD=4.0`

`set SEADS_SLED_RIGHT_MAXSPD=4.0 && seads.exe`

Today the righting pump only arms below **1.39 m/s** (5 km/h) and it was open only **13–20 %** of the
time you were over — **that gate, not the press, is the measured blocker.** Tip her over and press
SHIFT **while she is still sliding**, not only once she has stopped.
**THE QUESTION:** is she now trying to come up at moments where she used to ignore you? And the honest
second one: **does she come up too easily?** — this widens a number you have been driving for weeks.

### 5 — B2b, the pendulum, B2a still armed · `SEADS_SLED_STAND_SHIFT=0.5`

`set SEADS_SLED_RIGHT_MAXSPD=4.0 && set SEADS_SLED_STAND_SHIFT=0.5 && seads.exe`

On your side, **press-and-release SHIFT on a rhythm — about one press a second — and swing the mouse
left-right in time with her rocking.** Today the mouse does nothing while SHIFT is down and only bites
in the gaps. At 0.5 your body is live the whole time.

**⚠ READ THESE FOUR MEASURED ROWS BEFORE YOU ANSWER. Seconds to right from 178°:**

| `right_stand_shift_frac` | timed rock | mistimed rock | free-running human rhythm |
|---|---|---|---|
| **1.00** (shipped) | **1.48 s** | 1.77 s | 1.45 s |
| **0.50** (this run) | **1.48 s** | 2.06 s | 1.45 s |
| **0.25** | **1.51 s** | never (ends 107.7°) | — |
| **0.00** | **1.57 s** | never (ends 107.4°) | — |

**Turning this down does NOT make a good rock faster. It makes a bad rock fail.** The timed column is
flat — it gets slightly *slower*. Every bit of the widening gap is the mistimed arm degrading.
**THE QUESTION: does the TIMING start to matter?** — and then the one the table forces:
**is that what "gain pendulum momentum" meant, or did you want the ROCK ITSELF TO PAY?**
**AND ONE MORE, because this build sharpens a ruling of yours:** on 2026-08-26 you said *"I dont
understand why yo are clonlating the lean mechanism with the righting."* **This mixes them on
purpose.** Does that read as the rocking you asked for, or as the thing you rejected?
**If it feels worse — if she will not come up at all — say so and stop.** Then try **0.25** and
**0.75** on the same bank if you have the patience.

### 6 — B3, the tail swing, ITS OWN RUN · `SEADS_SLED_TAILSHED=0.4`

`set SEADS_SLED_TAILSHED=0.4 && seads.exe`

**⚠ NOT ON THE SAME RUN AS ANYTHING ELSE.**
**⚠ THE FIRST CANDIDATE IS 0.4, NOT 0.15 — the number moved because the DIAL moved.** After the
red-team fold the shed reads the **roost** (`|trk_slip| × avail`) and the **drivetrain**, not the bare
slip, and `avail` runs ≈ 0.23 at WOT, so 0.4 now buys what 0.15 was derived to buy: **9.2 % of the
track's lateral grip in a straight line, 18.4 % in a developed yaw** (§5.2).

Third gear on the trail, 10–15 m/s. **Set a lean into a corner and hold the throttle open instead of
tapping the brake.** The rear should step out under power, more the harder you lean into it, and come
back under you when you lift. **Then do the same corner with NO lean** — it should move less.

**⚠ THIS RUN OWES TWO ANSWERS, WRITTEN SEPARATELY, NOT ONE:**
**(i) THE DRIFT** — did the tail come around under power, and did leaning into it do the work?
**(ii) THE BANK — DID SHE ROLL MORE?** This dial takes yaw resistance away from the track, which is
the thing that keeps a ski-on-a-bank strike from becoming a spin. **(ii) OUTRANKS (i).**
**What the gate can already tell you, and what it cannot:** on FLAT SNOW, 12 cells × three arms, she
never latches `rolled` and the worst armed/unarmed roll ratio is **1.0238** — flat snow is fine at
every value on this ladder. **A bank strike is a TRIPPED rollover and no flat fixture reaches it**, so
(ii) is still yours and nothing here answers it.
**If the banks got worse, the fix is the lean-gated fallback form (`(1.0 + align_m)` → `align_m`), NOT
a smaller number.** If 0.4 is inaudible on both counts, step to **0.8**; **1.4 only after that.**

### 7 — ALL FIVE TOGETHER · one run, one line for the combination

`set SEADS_SLED_TRACTION_MU=3.0 && set SEADS_SLED_ROLLED_THROTTLE=0.15 && set SEADS_SLED_RIGHT_MAXSPD=4.0 && set SEADS_SLED_STAND_SHIFT=0.5 && set SEADS_SLED_TAILSHED=0.4 && seads.exe`

### 8 — one line per run afterwards

"good" / "worse" / "didn't notice" — **plus B3's two separate sentences and B0's traverse sentence.**
That is next round's R1, taken while it is fresh.

**NOT IN THIS DRIVE, DELIBERATELY:** `class_blend_m` (rung 6 — a different night, on TrailMain, and it
is road-repair's), `throttle_power_frac` (rung 4 — N4-PAIR ruling owed), `k_air_shift` (rung 5 —
N5-PAIR ruling owed).

---

## §7 — LANDING, WHEN HE SAYS SO

**NOTHING HERE IS A LANDING AND NOTHING HERE IS ON MAIN.** This is the drive build: five env vars,
zero `config/` edits, zero loader edits, zero `render/` edits, zero goldens moved. The landing path is
different work and it is per dial.

1. **HIS WORD FIRST.** A dial lands only with a value he drove and signed. No value in this build has
   been felt; the feel is his seat. A dial he says nothing about does **not** land.
2. **THE SIX TOUCHES, PER DIAL.** Each landing dial becomes a real TOML key:
   `config/scenario.toml` (the key, with his sentence in the comment) · `config/load_scenario.cpp`
   twice (the `require` and the range check) · `test/unit/test_load_scenario.cpp` (the key is
   required, and the range check rejects out-of-band) · `sim/sled.h` (the comment carries the signed
   value and the reason) · the tape roster is **already paid** for both new fields, so nothing is owed
   there. The env var stays beside it as the kill switch and the A/B — the `SEADS_GRIP_CAPACITY`
   shape — it does not get removed.
   ⚠ `[sled_comfort]` is the only TOML→`SledParams` bridge (`app/main.cpp` `sled_params.comfort =
   scen.sled_comfort;`), so **B0 and B3 are `SledParams` fields with no bridge at all** — landing them
   means adding one, which is a bigger edit than the four comfort dials and should be its own step.
3. **A RED-TEAM IN FRONT OF THE LANDING, not behind it** (standing rule). The landing red-team grades
   the *decision* — the value, the bridge, the ruling it rests on — not the code this lane already
   red-teamed twice.
4. **RE-GATE FROM CLEAN, DETACHED**, on the landing tip, and check the red set **by name** against
   `generated/gate/known_reds.txt`. The 2026-08-30 lesson: by name, never by count. Let
   `tools/gate/gate_baseline.py check` write the verdict.
5. **ANNOUNCE TO THE SENTINEL BEFORE ANY PUSH TO MAIN** — one line, before, docs included. An
   unannounced handoff push has broken another lane's fast-forward before. **This lane has pushed
   nothing to main and has therefore announced nothing; the first person to land owes that line.**
6. **REGENERATE THE GRAPH IN THE SAME COMMIT** as any structural change, and update
   `[lanes.sled-ride-firstbuild]` in `LANES.toml` (status, `in_flight`, `as_of`) at the end of the
   session.
7. **THE TAPE DEBT GOES WITH IT.** The moment the two new fields are on main, every other tree can
   read this lane's tapes again. Until then §5.4 P3-9 stands and this worktree's `build/` is the only
   reader.

---

## §8 — THE GATE ON THE FOLD, `eae9829ad`

**Tree gated:** `eae9829add63363e401f609e370430865f44b772` (short `eae9829ad`) on
`feel/sled-ride-firstbuild`, tree clean of code at the time of the run (only `docs/`, `LANES.toml`
and the seven `.bat` launchers moved afterwards — **no `sim/`, `app/`, `test/`, `config/`,
`generated/` or `CMakeLists.txt` file changed after this gate**, so its verdict carries to the final
tip). Build dir `D:/seads_sandboxes/sled-ride-b1/build`, rebuilt first. Run DETACHED per the harness
law:

```
ctest --test-dir build -C Debug --output-on-failure -j4 > build/gate_eae9829ad.log 2>&1 &
```

nothing else touching `build/` while it ran, and the play exe was built into the SEPARATE
`build-play/` dir.

**RESULT: 6 failed of 2148. 99 % passed. 983.91 s wall.**

Every one of the 2148 produced a result line (`grep -cE '^ *[0-9]+/2148 Test ' = 2148`) — the log is
complete, not truncated. Silence was not counted as a pass. (2144 → 2148 is the four new fold legs:
`sled_rolled_throttle_is_clamped_at_use`, `sled_tail_shed_keeps_the_slide_before_the_tip`,
`sled_tail_shed_is_dark_where_there_is_no_roost`, `sled_tail_shed_is_dark_with_the_thumb_shut`.)

**THE VERDICT IS THE RUNNER'S, NOT MINE.** `python tools/gate/gate_baseline.py check
build/gate_eae9829ad.log`, exit 0:

```
gate: 6 failed of 2148
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

**THE RED SET == MAIN'S BASELINE SIX, BY NAME** (not by count — the 2026-08-30 lesson):

| # | test | lane (`known_reds.txt`) |
|---|---|---|
| 79 | probe P-F: the relentless raider keeps the pump and shoots back | ai |
| 119 | E12.1: the raider backfill keeps a faction's pump offense alive | ai |
| 1291 | `sled_slides_before_it_tips_on_flat_snow` | snow |
| 1292 | `sled_grip_ceiling_stays_below_the_tip_threshold` | snow |
| 1330 | `sled_assist_reference_plane_is_load_weighted` | snow |
| 1333 | `sled_debug_sink_is_write_only` | snow |

**NEW REDS: NONE.** Nothing outside the baseline failed, so there is no assertion text to quote. This
lane neither fixed nor inherited those six and **did NOT re-record the baseline**.

**THE IDENTITY RE-PROOF, RUN AFTER THE FOLD** (`build/seads_sled_probe.exe tape <abs path>`, Chad's
six v17 tapes in `D:/flight_sim2/seads-recon/build-play`, opened read-only, nothing written there):

| N | replay | verdict | vs the PRE-FOLD run |
|---|---|---|---|
| 86 | 11430/11430 | bit-exact | **byte-identical** |
| 87 | 14272/14272 | bit-exact | **byte-identical** |
| 88 | 1837/11896, first divergence tick **46589** field `position.x` | not bit-exact (pre-existing) | **byte-identical** |
| 89 | 6355/15256, first divergence tick **68315** field `velocity.x` | not bit-exact (pre-existing) | **byte-identical** |
| 90 | 12990/12990 | bit-exact | **byte-identical** |
| 91 | 14833/20734, first divergence tick **14832** field `velocity.x` | not bit-exact (pre-existing) | **byte-identical** |

Same three tapes, same ticks, same fields, same two-name dial gap. **The fold touched `sim/sled.cpp`
in three places and moved not one bit of the replay.** `sled_tail_shed_hoist_is_a_pure_refactor`'s
17-digit golden is also UNMOVED by the roost hoist, and the tripwire
`grep -c "v_track - v_fwd) / std::max(v_track" sim/sled.cpp` reads **1**.

**THE PLAY EXE, SMOKE-LAUNCHED, BOTH ARMS:**
`D:/seads_sandboxes/sled-ride-b1/build-play/seads.exe`, built **2026-09-18 12:37:46 −0700**.

```
no env   -> [config] sled first-build: traction_mu 0 rolled_throttle_frac 0
            right_assist_max_ms 1.388888889 right_stand_shift_frac 1
            track_lat_slip_shed 0 (env: none -- identity, bit-identical to main)

=0.15    -> [config] sled first-build: traction_mu 0 rolled_throttle_frac 0.15
            right_assist_max_ms 1.388888889 right_stand_shift_frac 1
            track_lat_slip_shed 0 (env: SEADS_SLED_ROLLED_THROTTLE=0.15)
```

Log kept at `D:/seads_sandboxes/sled-ride-b1/build/gate_eae9829ad.log`.


---

## APPENDIX — THE FIRST GATE, ON THE PRE-FOLD TIP `58f6e4dd7`

(kept for the record; the gate that counts is §8, on `eae9829ad`)

## §4 GATE — THE FULL CTEST, DETACHED, ON THE LANE TIP

**Tree gated:** `58f6e4dd70f5f4107b6e2097a4cd1ffaa487eb29` (short `58f6e4dd7`) on
`feel/sled-ride-firstbuild`, tree clean, nothing pushed. Build dir
`D:/seads_sandboxes/sled-ride-b1/build`, rebuilt first (`cmake --build build` — up to date,
relink only). Run DETACHED per the harness law (`ctest --test-dir build -C Debug
--output-on-failure -j4 > build/gate_58f6e4dd7.log 2>&1`, status file polled at 60 s), nothing
else touching `build/` while it ran.

**RESULT: 6 failed of 2144. 99% passed. 1022.22 s wall.**

Every one of the 2144 produced a result line (`grep -cE '^ *[0-9]+/2144 Test ' = 2144`) — the log
is complete, not truncated. Silence was not counted as a pass.

**THE VERDICT IS THE RUNNER'S, NOT MINE.** `python tools/gate/gate_baseline.py check
build/gate_58f6e4dd7.log`, exit 0:

```
gate: 6 failed of 2144
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
```

**THE RED SET == MAIN'S BASELINE SIX, BY NAME** (not by count — the 2026-08-30 lesson):

| # | test | lane (known_reds.txt) |
|---|---|---|
| 79 | probe P-F: the relentless raider keeps the pump and shoots back | ai |
| 119 | E12.1: the raider backfill keeps a faction's pump offense alive | ai |
| 1291 | sled_slides_before_it_tips_on_flat_snow | snow |
| 1292 | sled_grip_ceiling_stays_below_the_tip_threshold | snow |
| 1330 | sled_assist_reference_plane_is_load_weighted | snow |
| 1333 | sled_debug_sink_is_write_only | snow |

**NEW REDS: NONE.** Nothing outside the baseline failed, so there is no assertion text to quote.

The four snow entries are the same four the §2.5 subset gate found and proved pre-existing by
stashing every change — this full run confirms it at 2144 scale rather than 168. This lane neither
fixed nor inherited them and did NOT re-record the baseline (§6.4 stands).

Log kept at `D:/seads_sandboxes/sled-ride-b1/build/gate_58f6e4dd7.log`.
