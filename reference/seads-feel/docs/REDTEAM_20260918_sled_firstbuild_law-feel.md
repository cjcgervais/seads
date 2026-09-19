# RED-TEAM — THE SLED FIRST BUILD, **LAW + FEEL LENS**

**2026-09-18.** Lane `D:/seads_sandboxes/sled-ride-b1`, branch `feel/sled-ride-firstbuild`.
Diff under review: `git diff 533c86409..HEAD` — one commit, `58f6e4dd7`.
Spec read whole first: `D:/seads_sandboxes/sled-audit/docs/SLED_RIDE_AUDIT_20260918_ADDENDUM.md` §D
and `D:/seads_sandboxes/sled-audit/docs/sled_audit/ladder_v2.md` §4.

**Everything below that is labelled MEASURED was run by this red-team**, against the lane's own
`build/seads_tests.exe` (the gate build, HEAD, tree clean at the time of the runs). No file in
`sim/`, `app/`, `test/`, `config/` or `generated/` was written by this pass. **Files written by this
pass: this one.**

**What this lens is not.** It is not the mechanism lens. It does not re-derive the hoist arithmetic
(that is done, and done well — §1). It attacks the **charter**, the **laws**, and **what Chad will
feel and be told**.

---

## §0 — THE VERDICT TABLE

| dial | verdict | why, in one line |
|---|---|---|
| **B0** `params.traction_mu` | **LAND-WITH-FIX** (one line on his sheet) | App-only, rostered, off by default — but it is the **only dial in this build that takes authority away**, and its own fence is a silenced baseline red. The sheet asks "did anything change?"; it must also ask the governor question. |
| **B1** `comfort.rolled_throttle_frac` | **LAND-WITH-FIX** | The split is exact and the `!hands_on` law is kept and legged. But **the wall does not exist** (P0-1), the header points at a leg that cannot see the dial (P2-8), and the product is never re-clamped (P1-4). |
| **B2a** `comfort.right_assist_max_ms` | **LAND** | Zero kernel, already rostered, env override in the blessed shape. No finding. |
| **B2b** `comfort.right_stand_shift_frac` | **LAND-WITH-FIX** (the fix is on his sheet, not in the code) | Zero kernel. But the leg's own four rows say the mechanic is a **subtraction, not a gain** (P1-5), and his sheet does not say so. |
| **B3** `params.track_lat_slip_shed` | **LAND-WITH-FIX** — and the fix is **a leg written before run 6**, not after | The hoist proof is the best work in the diff. But the dial's worst case — *"did she roll MORE"* — has **no green leg at any armed value**, and its natural fence is in `known_reds.txt` (P0-2). The track-only guard leg **cannot be killed by the mutation it names** (P1-3). |

**Nothing here is DO-NOT-LAND, and that is deliberate: nothing lands.** This is the drive build.
The identity claim — the thing that would make a DO-NOT-LAND — **holds, and is the strongest part of
the diff**: six of six tapes byte-identical through the differential hoist proof, both new fields
rostered in the same commit, both identities the struct default, full gate red set == the baseline
six by name. The findings below are about **what the build claims it proved**, and about **what Chad
is handed**.

---

## §1 — WHAT SURVIVES THE ATTACK (said first, because it is most of the diff)

1. **The B1 split is exact.** `(s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle)` →
   `thr_in = hands_on ? clamp01(in.throttle) : 0.0; throttle = s.rolled ? frac*thr_in : thr_in`.
   All four quadrants agree at `frac == 0.0` for every finite input, and `!hands_on` still forces an
   unconditional zero — **R4a §7.4 is kept, and `sled_rolled_throttle_never_reaches_a_handless_rider`
   (`test/unit/test_sled.cpp:4560`) is a real leg with a real killing mutation.** I tried to break
   it: collapsing the ternary back does drop the guard, and the leg reds. It is not vacuous.
2. **The hoist proof is the right proof, done better than the spec asked.** A baseline control found
   that 88/89/91 are already not bit-exact on `533c86409`, so the obligation was run as a
   **differential** — the hoist-only build reproduces all six probe outputs byte-identically,
   *including the divergence tick and field of the three already-divergent tapes*. That is stronger
   than the pass/fail form §D asked for, and it is reported as a report against the spec rather than
   quietly satisfied. `v_fwd` and `throttle` are both `const` at their definitions, so "nothing
   between the two sites writes them" is a compiler-enforced fact, not a claim.
3. **The tripwire is right and the self-reference trap is documented.** `grep -c "v_track - v_fwd) /
   std::max(v_track" sim/sled.cpp` == 1; `roost_thrust`'s `v_rel` at `sim/sled.cpp:1265` is
   untouched; the comment at `:1209-1228` is worded *around* the pattern so it does not count itself.
4. **The roster law is paid in the same commit**, both fields, and `traction_mu`
   (`test/harness/sled_tape.h:67`) and `right_assist_max_ms` (`:82`) were already there. `dial_gap`
   reports the two new names honestly instead of lying "bit-exact".
5. **`align_m`, not the raw signed lean.** `sim/sled.cpp:601-606` clamps it ≥ 0, so a wrong-way lean
   is never a penalty and `sled_wrong_way_lean_is_never_a_penalty` stays green. The house rule holds.
6. **`g.is_track` is present on both the hoisted block and the shed.** Lean buys ski plate, never
   track plate — the `plane_lat_lean_gain` discipline is kept (even though the *leg* that claims to
   prove it does not, P1-3).
7. **The handoff is honest where it could have been quiet.** §2.3 (B1 reaches the engine and nothing
   else on flat ground), §2.4 (the spec's positive control is false), §3 (`slip_ang == 0` is
   unreachable), §5.4 (three tapes already divergent on main). Four reports against the lane's own
   spec, all measured, none of them flattering. That is the standard.

**On the charter.** I could not find `ROLL_COMFORT_HANDOFF` anywhere in this worktree
(`git ls-files | grep -i roll`, `grep -rl "governor" docs/`) — **I am reporting that rather than
pretending to have read §0/§0b.** I judged "no governor" against its operative meaning in this
project: *no mechanism that silently caps or removes what the rider can do*. On that reading B1, B2a,
B2b and B3 all **add** rider authority or **return** it (B2b takes it from the machine's automatic
brace and gives it to his hands). **B0 is the exception and the only one that removes** — see P2-6.

---

## §2 — FINDINGS

### P0-1 — **B1 HAS NO WALL. THE NEW WALL LEG'S ARMS CANNOT DIFFER, AND A RULING WAS WRITTEN ON IT.**
`test/unit/test_sled.cpp:4592` (`sled_rolled_throttle_the_wall_at_the_driven_value`);
handoff §2.2 last line; commit message `58f6e4dd7` ("No ceiling was reached: 0.15 needs no
reduction, nor does 1.0.")

**MEASURED, this pass**, `build/seads_tests.exe "sled_rolled_throttle_the_wall_at_the_driven_value" -s`:

```
[B1 WALL] frac=0.00 v_side= 0.0  final_tilt= 106.22 rpm= 1700.0 thrust=     0.00 roost=0.00000
[B1 WALL] frac=0.15 v_side= 0.0  final_tilt= 106.22 rpm= 2645.0 thrust=     0.00 roost=0.00000
[B1 WALL] frac=1.00 v_side= 0.0  final_tilt= 106.22 rpm= 8000.0 thrust=     0.00 roost=0.00000
```

and the assertion expansions, at full precision, **all nine cells**:

```
106.21816119284737567   106.21708989882516505   106.20226219859530659     <- frac 0.00
106.21816119284737567   106.21708989882516505   106.20226219859530659     <- frac 0.15
106.21816119284737567   106.21708989882516505   106.20226219859530659     <- frac 1.00
```

**Three distinct numbers, one per `v_side`, zero per `frac`, identical to the 17th digit.**
`thrust_n` is 0.00 N and `roost_flux` is 0.00000 in every cell: the track never touches, so the whole
`if (g.is_track)` thrust block never runs. **The arms of this leg CANNOT differ at any value of the
dial.** It is the project's own probe-noise-floor law, broken — *measure the noise floor with arms
that cannot differ **before** you rule* — and the ruling was written anyway:
**"NO CEILING WAS REACHED. B1 = 0.15 needs no reduction; nor does 1.0."**

The leg's own docstring indicts `:2649` for exactly this shape ("that pass IS the failure", "the
vacuous-probe shape this project has paid for before") and then commits the same error one fixture
over: `:2649` fails because the *thumb* is closed; `:4592` fails because the *track* is in the air.
Opening the thumb changed which readout moved (`engine_rpm`, computed **outside** the contact block)
and changed nothing that the wall is about. `REQUIRE(armed.engine_rpm == 1700.0 + frac*6300.0)` is a
non-vacuity check on the **wrong half of the machine**: it proves the dial reached the engine, not
that the engine reached the ground.

The handoff is honest about the physics (§2.3 says the dial reaches the engine and nothing else) and
then draws the opposite conclusion three paragraphs earlier (§2.2: no ceiling was reached). **Both
sentences are in the same document and only one of them is supported.** "Nor does 1.0" is the
dangerous half: it reads as licence to hand him 1.0, from a fixture that could not have shown danger
at 100.

**CONCRETE FIX (three parts, all cheap):**
1. **Strike the ruling.** Handoff §2.2 last line and the commit body: replace
   *"NO CEILING WAS REACHED. B1 = 0.15 needs no reduction; nor does 1.0"* with
   *"NO CEILING WAS MEASURED, because no fixture in this build puts a downed machine's track on the
   ground. The 0.15 cap stands on §D's arithmetic, not on a wall."*
2. **Rename the leg to what it measures:** `sled_rolled_throttle_is_dark_on_flat_ground`, and move
   the wall claim out of its name and docstring. A leg called "the wall" that cannot fail is worse
   than no leg, because the next lane will trust it.
3. **Then write the wall that can fail.** The only fixture that arms it is one where the track finds
   a face. Cheapest form that stays hermetic: drop her on her side **into a slope or against a step**
   (`flat_field()` → the tilted/stepped field already used by the bank-strike legs) so
   `sink_m[track] > 0` and `thrust_n > 0`, then assert `thrust_n > 0` (the non-vacuity that is
   missing today) **and** that final tilt does not fall below the `rolled` release band at 1.0.
   Until that leg exists, **B1's ceiling is his seat and nothing else** — say so on his sheet.

---

### P0-2 — **B3's WORST CASE HAS NO GREEN LEG AT ANY ARMED VALUE, AND ITS FENCE IS A SILENCED BASELINE RED.**
`generated/gate/known_reds.txt`; `test/unit/test_sled.cpp:1074`; `sim/sled.cpp:1147-1150`.

`sled_slides_before_it_tips_on_flat_snow` (`:1074`) is the leg that pins Chad's signed feel — *"a
real snowmobile on flat snow SLIDES OUT before it TIPS OVER"*. Its own **KILLED BY** line, verbatim:

> `track_rail_half_m = 0` (the centreline track), **or restoring the old mu_lat/track_lat_mu — either
> puts the grip ceiling back above the tipping threshold.**

**B3 is a multiplier on `track_lat_mu`.** It is the named killing mutation of that leg, applied
deliberately, by a dial. And:

* the leg is **in `known_reds.txt`** (`snow  sled_slides_before_it_tips_on_flat_snow`), so the gate
  is structurally unable to report it either way;
* even if it were green it would be **blind to the dial**, because it builds `const sim::SledParams
  p;` — the same reason §4.4 discharged `sled_track_lat_mu_is_the_rostered_value`. That discharge was
  correct for *that* leg and was quietly generalised to this one, which is not the same thing at all:
  one is a rostered-value assertion, the other is **the feel Chad signed**;
* `sled_grip_ceiling_stays_below_the_tip_threshold` (`:1253`), the other half of the same fence, is
  **also** in `known_reds.txt`;
* **MEASURED / DERIVED**: `align_m ≤ 1.0` (`sim/sled.cpp:601-606`, `clamp01(...)*smoothstep`), and
  `|trk_slip| ≤ 1.0` by its own clamp, so at `shed = 0.5` the factor is
  `max(0, 1 − 0.5·1.0·2.0) = 0.0` — **at the ladder's own last step the track's lateral μ is exactly
  zero.** That is not "loses two thirds in a developed yaw" (§4.5 run 6, P1-3's arithmetic, which
  assumed `|trk_slip| ≈ 0.667`); at a standstill or a spinning track it is a *total* loss, and a
  standstill-at-WOT is reachable by pressing W.

So the dial's stated worst case — *(ii) DID SHE ROLL MORE*, which §D says **outranks** the drift —
is carried entirely by one sentence on a drive sheet, with **zero** test coverage. The ladder's §4.4
B3 leg list does not contain a slide-before-tip leg either: **this gap is inherited from the spec,
and this is the pass that was supposed to catch it.**

**CONCRETE FIX — write this leg before run 6, not after:**
`sled_tail_shed_keeps_the_slide_before_the_tip`, hermetic, **not** a re-use of the red `:1074`
(a green leg must not depend on a silenced one): the same flat-snow full-lock speed sweep
(`{4, 8, 12, 16, 20, 25} m/s` × depth `{0.30, 0.77}`), three arms `shed = 0.0 / 0.15 / 0.5`, assert
(a) max roll under the same threshold `:1074` uses, (b) she is **visibly turning** while she refuses
(the same anti-"does nothing" clause), (c) the 0.0 arm is bit-identical to the shipped trajectory.
**KILLED BY:** any shed value that lets the track's μ fall far enough to trip her. If the 0.5 arm
reds, that is the B3-vs-BANK answer **before** he spends a tank of fuel on run 6, and §D's ordering
(ii over i) is honoured by the gate instead of by a sentence.

---

### P1-3 — **`sled_tail_shed_is_track_only` CANNOT BE KILLED BY THE MUTATION IT NAMES, AND HAS NO POSITIVE CONTROL.**
`test/unit/test_sled.cpp:4684`; the guard at `sim/sled.cpp:1147`; the declaration at `sim/sled.cpp:1075`.

The leg's stated killing mutation is *"dropping the `g.is_track` guard — the skis shed, the carve
moves, the arms diverge."* **It does not, and the diff's own structure is why.**

`trk_slip` is declared at **patch-loop scope** (`sim/sled.cpp:1075`), initialised to `0.0`, and
**assigned only inside `if (g.is_track)`** (`:1077-1107`). On a ski patch it is therefore **exactly
`0.0`**. Delete `g.is_track &&` from `:1147` and the ski path evaluates

```
mu_l *= std::max(0.0, 1.0 - shed * std::abs(0.0) * (1.0 + align_m));   // == mu_l *= 1.0
```

`mu_l *= 1.0` is bit-identical for every finite `mu_l`, including `±0.0`. **The mutation is a
no-op. The leg passes under its own killer.** This lane's banner, four lines above the leg, says
*"a leg that cannot be killed is not a gate"*.

Worse, the leg has **no positive control**: both arms set `track_lat_mu = 0.0`, so the dial is
arithmetically inert on the track in **both** arms. If `track_lat_slip_shed` were wired to a field
nobody reads — the exact failure mode `selfright_a_timed_rock_beats_a_mistimed_one` names as *its*
killer — this leg would still pass.

The guard is still **correct and worth keeping** (it is the `plane_lat_lean_gain` discipline, and it
is the only thing protecting the ski plate the day someone hoists the slip block out of its own
`if (g.is_track)`). The defect is the *leg*, and the false confidence it buys.

**CONCRETE FIX (same file, ~8 lines):**
1. Add the positive control to the same leg: a third arm at the **shipped** `track_lat_mu = 0.70`,
   `shed = 0.5`, on the identical fixture, and `REQUIRE_FALSE(same_state(...))` against its own
   `shed = 0.0` twin — so the leg proves the dial does *something* on this fixture before it proves
   it does *nothing* to the ski.
2. Restate the killing mutation honestly, because the real one is different and is the one a future
   lane will commit: **replacing the patch-local `trk_slip` with a machine-level slip**
   (`s.track_slip`, which is non-zero on every patch), **or hoisting the slip block out of its own
   `if (g.is_track)`.** Either makes the guard load-bearing and reds this leg. Name both.

---

### P1-4 — **B1's PRODUCT IS NEVER RE-CLAMPED; THE ENV PATH HAS NO RANGE CHECK THE TOML PATH HAS.**
`sim/sled.cpp:308-309`; `app/main.cpp:2516-2532`.

```cpp
const double throttle =
    s.rolled ? p.comfort.rolled_throttle_frac * thr_in : thr_in;
```

`thr_in` is `clamp01`'d. **The product is not.** `SEADS_SLED_ROLLED_THROTTLE=1.5` — a plausible
fat-finger at 22:00, or a deliberate *"give me more"* — puts `throttle = 1.5` into every consumer:
`v_cmd = throttle * track_speed_max_ms` (1.5× the belt's top speed), `engine_rpm = 1700 +
throttle*6300` (11 150 rpm), `drive_t`, the weight-transfer moment arm, the HUD and the engine sound.
That is **outside the kernel's documented `[0,1]` input domain for `throttle`**, reached only while
rolled, and **the banner prints `1.5` as if it were legitimate.** The TOML route this build
deliberately avoided has a `require` + range check; the env route it chose has neither, and this is
the one dial of the five whose name says *fraction*.

Same class, quieter: `SEADS_SLED_TAILSHED=-0.15` is **silently inert** — the `> 0.0` guard at
`sim/sled.cpp:1147` skips it — while the banner lists it as armed. He types a negative expecting the
opposite sign, gets identity, and reads it as "the dial does nothing".

**CONCRETE FIX:**
* `sim/sled.cpp:308-309` →
  `const double throttle = s.rolled ? clamp01(p.comfort.rolled_throttle_frac * thr_in) : thr_in;`
  **Identity is preserved exactly**: at `frac == 0.0`, `clamp01(0.0)` is the same `+0.0`; for every
  `frac ≤ 1.0` and `thr_in ∈ [0,1]` the product is already in range and the clamp is the identity
  function. The tape corpus replays unchanged.
* `app/main.cpp`, in `env_dial`: a per-dial sane band, warned not enforced —
  `rolled_throttle_frac` and `right_stand_shift_frac` outside `[0,1]`, `track_lat_slip_shed`
  negative, `traction_mu` or `right_assist_max_ms` negative ⇒ one `fprintf` to stderr **and still
  apply it** (the `SEADS_GRIP_CAPACITY` precedent is explicit that a huge value is the kill switch;
  the warning is the fix, not a refusal).

---

### P1-5 — **B2b IS A SUBTRACTION SOLD AS A GAIN, AND THE LEG'S OWN FOUR ROWS SAY SO. HIS SHEET DOES NOT.**
`test/unit/test_sled_selfright.cpp:208`; handoff §2.4; drive sheet §4 run 5.

His sentence: *"I want to be able to self right by **rocking bodyweight** back an fourth while
pressing stand on and off, **gain pendulum momentum**."*

**MEASURED, this pass** (`build/seads_tests.exe "selfright_a_timed_rock_beats_a_mistimed_one"`), plus
the 0.00 row from handoff §2.4:

| `right_stand_shift_frac` | **timed rock** | mistimed rock |
|---|---|---|
| 1.00 (shipped) | **1.48 s** | 1.77 s |
| 0.50 (the drive value) | **1.48 s** | 2.06 s |
| 0.25 | **1.51 s** | never (ends 107.7°) |
| 0.00 | **1.57 s** | never (ends 107.4°) |

**The timed arm does not improve. It gets slightly worse** — 1.48 → 1.48 → 1.51 → 1.57 s. Every bit
of the widening gap comes from the **mistimed** arm degrading. So what B2b delivers is not *"gain
pendulum momentum"*; it is *"a bad rhythm is now punished"*. The rocking he asked for buys him
**nothing he did not already have at the shipped 1.0** — it only stops being free.

That may still be the feel he wants (a mechanic you can fail is a mechanic), and the run-5 question
*"does the TIMING start to matter?"* will honestly get a **yes**. But a yes there is **compatible
with the mechanic being pure subtraction**, and his sheet does not contain the one number that would
let him tell the difference. This is the shape memory already names — *the kernel clamp was a
REDUCTION* (weight-shift program) — arriving again under a new name.

Second, smaller, but it is why the table above is not the whole story: **the "timed" arm is a 120 Hz
sign-following bang-bang controller**, `lean_lat = -sign(angular_vel.z)` re-evaluated every step
(`test_sled_selfright.cpp:190-193`). It is perfect-information feedback, not a human rhythm. The leg
therefore proves *a feedback controller beats an anti-feedback controller* — which is nearly a
tautology for a pendulum — and **does not establish that a hand on a mouse at ~0.6 Hz accumulates
anything.** §4.4 specified the fixture this way, so the lane followed the spec; the spec is what is
wrong.

**CONCRETE FIX (no kernel change; two parts):**
1. **Put the four rows in his sheet, above run 5**, with the honest reading in one sentence:
   *"Turning this down does not make a good rock faster (1.48 s at every setting). It makes a bad
   rock fail. Question: is that what 'gain pendulum momentum' meant, or did you want the rock itself
   to pay?"* — because if he wanted **gain**, `right_stand_shift_frac` is the wrong dial entirely and
   the rung needs a term converting his lean **rate** into righting torque. That is a different dial
   and a different night, and it should be raised as an owed ruling in handoff §5 now, not discovered
   after run 5.
2. **Add a third arm to the leg at a human cadence** — the same free-running 1.0 s/0.7 s square wave
   as `in.stand`, phase-locked to the press and **not** to `angular_vel.z` — and report whether
   *that* beats the mistimed arm at 0.5. If a free-running rock shows no advantage, the rung's claim
   is not established for a hand, and that is a finding worth more than the leg's current pass.

---

### P2-6 — **B0 IS THE ONLY DIAL IN THIS BUILD THAT TAKES SOMETHING AWAY, AND HIS SHEET DOES NOT ASK THE GOVERNOR QUESTION.**
`app/main.cpp:2535`; `sim/sled.cpp:1296-1305`; `generated/gate/known_reds.txt`.

B1, B2a, B2b and B3 all hand authority to the rider. **B0 caps what the track can deliver** — it is
a ceiling by construction. It is a *contact* ceiling (load-proportional, physics-shaped, "a track
cannot push harder than the snow it stands on"), not a speed or input governor, and it ships at 0.0
= off, so the charter is not broken. But `SEADS_SLED_TRACTION_MU=3.0` on run 2 is the one launch in
the seven where **something he can do today may become impossible**, and:

* the fence with the matching name, `sled_grip_ceiling_stays_below_the_tip_threshold`
  (`test/unit/test_sled.cpp:1253`), is **in `known_reds.txt`** — the gate cannot speak to it;
* no leg in this diff exercises `traction_mu` at 3.0 at all;
* run 2's question is *"did anything change at all?"*, which is a **detection** question, not an
  **authority** question. A rider answers "no" to it and moves on.

**CONCRETE FIX (one line on his sheet, run 2):** add the second question, in his language —
*"and did anything you could do before become impossible? A climb you used to make, a bank you used
to get up. If yes, that outranks everything below."* `docs/snowform_measurements.md` §M8.1's claim
that μ ≥ 3.0 leaves his traverse byte-identical is the prediction; this is the question that tests it.

---

### P2-7 — **`env_dial` VALIDATES IN THE WRONG ORDER AND SILENTLY WRITES 0.0 FOR A WHITESPACE VALUE.**
`app/main.cpp:2519-2528`.

```cpp
char* end = nullptr;
const double v = std::strtod(e, &end);
while (end && *end == ' ') ++end;          // <-- runs BEFORE the no-conversion test
if (end == e || (end && *end != '\0')) { /* warn, keep default */ return; }
*dst = v;
```

`SEADS_SLED_STAND_SHIFT=" "` (a stray space — Windows `set VAR= `, a pasted launch line, a `.bat`):
`strtod` performs **no conversion** and, per the standard, stores `nptr` in `*endptr`, so `end == e`.
The `while` then **advances `end` past the spaces**, so by the time `end == e` is tested it is false,
`*end` is `'\0'`, validation passes, and `*dst = 0.0`. **That is exactly the silent zero the comment
five lines above says this code prevents** — and 0.0 is **not** the identity for
`right_stand_shift_frac` (1.0) or `right_assist_max_ms` (1.3889). He would fly a materially different
machine with a banner that says the dial is armed at 0.

Same function, same class: `strtod` parses `"nan"` and `"inf"`, both of which pass the full-string
check today. `SEADS_SLED_TRACTION_MU=nan` poisons every contact force in the kernel. And only `' '`
is skipped — a trailing `\r` from a CRLF `.bat` takes the warn-and-keep path, which is the safe
direction, but by luck rather than by design.

**CONCRETE FIX:**
```cpp
char* end = nullptr;
const double v = std::strtod(e, &end);
if (end == e) { warn_keep_default(); return; }                 // no conversion -- FIRST
while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
if (*end != '\0' || !std::isfinite(v)) { warn_keep_default(); return; }
*dst = v;
```

---

### P2-8 — **`sim/sled.h:225-228` STILL NAMES A WALL THE DIFF ITSELF PROVES CANNOT SEE THE DIAL.**
`sim/sled.h:225`.

> `// ★ THE WALL this value is chosen against is`
> `// `sled_onside_recovery_is_momentum_not_magnetism`: a downed machine must`
> `// not drive itself upright on track thrust. Raise this until that leg`
> `// passes only because she walked herself over and THE PASS IS THE FAILURE.`

`test/unit/test_sled.cpp:4592`'s own docstring, and handoff §2.2, prove that leg drives
`const sim::SledInputs idle;` and **cannot see `rolled_throttle_frac` at all**. The header is where
the next lane looks first, and it points at a leg the same commit calls useless. Combined with P0-1
(the replacement is also dark), the header currently promises a ceiling that exists nowhere.

**CONCRETE FIX:** replace those four lines with the measured truth —
*"⚠ THERE IS NO WALL YET. `:2649` drives a closed thumb and cannot see this dial;
`sled_rolled_throttle_the_wall_at_the_driven_value` holds W but, on flat ground, a downed machine's
track has no contact — `thrust_n` and `roost_flux` are 0.00 at every value up to 1.0, and the final
tilt is identical to the 17th digit. The wall needs a fixture where the track finds a face.
UNTIL IT EXISTS THIS VALUE'S CEILING IS CHAD'S SEAT."*

---

### P3-9 — **TONIGHT'S TAPES ARE LANE-LOCKED, AND NOTHING SAYS SO.**
`test/harness/sled_tape.h:461` and `:475` (`if (!hit) return fail("unknown param", line);`);
`kMagic` at `:46` unchanged.

The reader **hard-fails on an unknown `param`/`cparam` name**. Tonight's writer emits
`track_lat_slip_shed` and `rolled_throttle_frac`, which no other build in the project declares. So
**every `.sledtape` Chad records tonight is unreadable by main's `seads_sled_probe`, by the
sentinel's tree and by every other lane** — it errors, it does not warn — and `kMagic` is still
`"seads-sled-tape v1"`, so nothing signals why. `dial_gap` covers the *other* direction (build
declares, tape lacks); this direction has no machinery at all. Handoff §5.4 names the mirror problem
(three v17 tapes already un-replayable on main) and stops one step short of this one.

**CONCRETE FIX:** one line in handoff §5 as a report — *"tonight's tapes can only be replayed by
this lane's `build/seads_sled_probe.exe` until the two fields land on main; do not delete this
worktree while its tapes are still the evidence"* — plus the same line in the reply that hands him
the exe path.

---

### P3-10 — TWO SMALLER ONES, NAMED SO THEY ARE NOT REDISCOVERED

* **The banner calls an empty env var "armed".** `app/main.cpp:2555-2566` lists a name whenever
  `getenv` is non-null; `env_dial` returns early and silently on `!*e`. `SEADS_SLED_TAILSHED=`
  therefore prints in `(env: ...)` while the dial is at identity. The value columns still print the
  truth, so it is readable — but the one field he will skim is the one that lies. Skip empty strings
  in the banner loop too.
* **`sled_rolled_throttle_frac_zero_is_the_shipped_zero` (`:4521`) does not exercise the clause it
  calls load-bearing.** MEASURED: `v_bf = 0.000707 m/s` against a `≤ 3.0` assertion — four orders of
  slack. The `3.0` clause is real and the comment explaining it is right, but the leg never goes
  near it, so a future edit that breaks the clutch-blend boundary will not red here. Add a second
  arm at `v_bf ≈ 4 m/s` asserting rpm **above** idle, so the clause is a measurement and not a note.

---

## §3 — THE FOUR CHARTER QUESTIONS, ANSWERED DIRECTLY

**1. Wheelie / drift / backflip untouched — the leg that proves each.**

| mechanic | the leg | status |
|---|---|---|
| wheelie + backflip | `tape_chad_flip_fence` (`test/unit/test_sled_tape.cpp:587`) — his landed 2.06 s flip replayed open-loop, must still fly and land with no `rolled` latch | **GREEN in the subset.** It replays with `t.params`, where both new dials are **absent** ⇒ today's struct defaults ⇒ identity. It proves the flip at identity and **is silent at every armed value.** |
| drift / slide-before-tip | `sled_slides_before_it_tips_on_flat_snow` (`test/unit/test_sled.cpp:1074`) | **RED, and in `known_reds.txt`.** Blind to B3 anyway (`const SledParams p;`). **This is P0-2.** |
| lean asymmetry | `sled_wrong_way_lean_is_never_a_penalty` (`:2546`) | **GREEN and unmoved** — the `align_m` choice holds it. |

**So: wheelie and backflip are fenced at identity by construction and by a real leg. Drift is not
fenced at all.** At identity the by-construction argument is sound for all three (B1 is a branch on
`s.rolled`, B3 is a branch on `> 0.0`), and that is what a drive build owes. **At armed values
nothing is fenced**, and handoff §6 should say so in as many words: *"this build refuses to claim
that wheelie, drift or backflip are unchanged at any ARMED value — only at identity."*

**2. STAND rights / LEAN throws — is B2b's rocking press-gated or automatic?**
**Press-gated, and the diff does not weaken that.** `sim/sled.cpp:417-421` scales
`s.right_shift_cmd` — which is 0 unless the self-right is active — and it is **added** to his
commanded `target_lat_m`, then clamped. Turning the frac **down** removes automatic authority and
leaves his hands; it cannot make anything more automatic. B2b therefore moves **in the direction
R11 ruled** (*"not automatic re righting"*).
**But N2-CONFLICT is live and this build sharpens it.** On 2026-08-26 he ruled *"I dont understand
why yo are clonlating the lean mechanism with the righting"*; B2b's whole value is that the **lean**
now decides whether the **righting** works. §E flags it as owed and not blocking, and I agree it does
not block a drive — but run 5's question should carry it: *"you told us once not to mix the lean into
the righting. This mixes them on purpose. Does that read as the rocking you asked for, or as the
thing you rejected?"* One sentence, and the ruling comes back with the drive instead of after it.
**C2-PRESS is untouched by this diff** (the C2 side-righting term at `sim/sled.cpp:1622-1700` still
has no press gate at all) — correctly out of scope, still owed.

**3. Throttle-while-rolled vs the R4a hands-off law.**
**Kept, deliberately, and legged.** `!hands_on` forces an unconditional zero at every value of the
dial including 1.0 (`test/unit/test_sled.cpp:4560`, real killing mutation, not vacuous). The drive
sheet carries P0-1's line at the top — *"If the rider is off the bars, W is MEANT to do nothing"* —
which is the other half of the same law. **No finding.**

**4. Is any candidate value a harness-tuned number?**
**No — and this is the cleanest part of the build.** `B0 = 3.0` comes from
`docs/snowform_measurements.md` §M8.1; `B1 = 0.15` from §D's P0-2 cap; `B2a = 4.0` from X1's measured
13-20 % gate-open fraction; `B2b = 0.5` with 0.25/0.75 offered beside it; `B3 = 0.15` from P1-3's
WOT-duty arithmetic, with 0.3 and 0.5 as *later* steps. **Every one is offered to his hand with the
reason attached, and no value in the diff was moved to make a test pass.** The one caveat is P0-1:
`0.15` is described as capped *"until the wall leg has been read at the value"*, the wall was read,
and it read nothing — so 0.15 stands on §D's arithmetic alone. Say that, rather than "no ceiling was
reached".

---

## §4 — WHAT MUST HAPPEN BEFORE HE LAUNCHES

**Blocking the drive (cheap, all of it):**
1. **P0-1** — strike *"NO CEILING WAS REACHED … nor does 1.0"* from the handoff and the commit body;
   rename the leg; fix `sim/sled.h:225-228` (P2-8). **The claim is the danger, not the code.**
2. **P0-2** — write `sled_tail_shed_keeps_the_slide_before_the_tip` at 0.15 and 0.5 and run it.
   If 0.5 reds, he is told before run 6 instead of after it.
3. **P1-5** — put the four timed/mistimed rows on his sheet above run 5, with the one honest
   sentence, and raise the "did you want the rock to *pay*?" ruling in §5.

**Blocking a future landing, not this drive:**
4. **P1-3** — the track-only leg's positive control and its real killing mutation.
5. **P1-4** — `clamp01` on the product; warn on out-of-band env values.
6. **P2-7** — the `env_dial` ordering fix.

**Reports, no action owed:** P2-6, P3-9, P3-10.

---

## §5 — LANE HYGIENE NOTE (not a finding against the diff)

At the time of this pass the working tree carried an **uncommitted** `TEST_CASE` in
`test/unit/test_sled.cpp` — `ZZ_redteam_coasting_shed_probe`, tagged `[zzprobe]`, marked *"RED-TEAM
TEMPORARY PROBE (not for commit)"*, ending in `REQUIRE(true)`. It was **not** present when this pass
began (`git status` showed only the handoff as ` M`), so a sibling pass is writing to the same
worktree. It is not mine. **It must not be committed**, and note that Catch2 registers it regardless
of its tag, so `ctest` will discover and always-pass it if the tree is rebuilt with it in place.
All measurements in this document were taken against `build/seads_tests.exe` as the gate built it
from `58f6e4dd7`, and every number reproduces the handoff's own reported figures exactly.

---

## §6 — WHAT THIS RED-TEAM REFUSES TO CLAIM

1. **That it read `ROLL_COMFORT_HANDOFF §0/§0b`.** It is not in this worktree and I did not find it.
   §1's charter judgement is made against the operative meaning of "no governor" in this project and
   is marked as such.
2. **That the identity claim is wrong.** It is not. Six of six tapes byte-identical, the differential
   proof is stronger than the spec asked for, the roster law is paid in the same commit, and the full
   gate's red set is the baseline six by name. **P0-1 and P0-2 are about claims and coverage, not
   about the identity.**
3. **That B3 is safe or unsafe.** Nobody knows, including this pass — that is P0-2's whole point.
   `sled_tail_shed_is_inert_in_a_straight_line`'s own numbers (9.78e-16 m straight against 14.05 m
   leaned, measured again this pass) say the dial is very large where it bites.
4. **That anything here is FELT.** Nothing was driven. **The feel is his seat.**

**Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>**
