# RED-TEAM — THE SLED FIRST BUILD, MECHANISM + MUTATION LENS
**2026-09-18.** Lane `D:/seads_sandboxes/sled-ride-b1`, branch `feel/sled-ride-firstbuild`,
diff `533c86409..58f6e4dd7`. Spec: `SLED_RIDE_AUDIT_20260918_ADDENDUM.md` §D +
`docs/sled_audit/ladder_v2.md` §4 (read whole in `D:/seads_sandboxes/sled-audit`, read-only).

**HOW THIS PASS WORKED.** Every claim below marked MEASURED was produced by editing `sim/sled.cpp`
or appending a probe `TEST_CASE` **in this lane only**, rebuilding `build/seads_tests.exe`, running
it, and then restoring the file byte-for-byte. The tree is back at the commit (`git status` = the
handoff's uncommitted §4 GATE and the sibling law-feel red-team doc, nothing else). Chad's fly tree
and the audit lane were never written. No `ctest` gate was re-run.

**THE HEADLINE.** The **identity is sound** — I could not construct a reachable state where the
no-env exe's *simulation* differs from main. The roster law is paid, the hoist is a genuine pure
refactor, and the `frac * thr_in` form is correct and mutation-proven. **What does not survive is the
evidence around two of the dials:** B1's wall was never armed and cannot be, and B3's shed fires
hardest exactly where the throttle is not.

---

## VERDICTS

| dial | verdict | why |
|---|---|---|
| **B0** `traction_mu` | **LAND** | `app/main.cpp` only, already rostered, already a `> 0.0` branch at `sim/sled.cpp:1297`. Zero new surface. |
| **B1** `rolled_throttle_frac` | **LAND-WITH-FIX** | Kernel edit and identity are clean and mutation-proven. The fix is documentary and not optional: **P1-1** — the wall verdict in the commit message is unsupported and the drive card is wrong about what run 3 can answer. |
| **B2a** `right_assist_max_ms` | **LAND** | One env line, zero kernel, already rostered. |
| **B2b** `right_stand_shift_frac` | **LAND** | One env line, zero kernel. The new pendulum leg is real and its report against §4.4's false positive control is correct. One soft assertion, **P3-3**. |
| **B3** `track_lat_slip_shed` | **LAND-WITH-FIX for the DRIVE; DO-NOT-LAND as a kernel law in this form** | Identity and hoist are proven. But **P1-2** (the shed fires at full strength with the thumb closed) and **P1-3** (the dial is named for the roost and does not read it) make run 6's two owed answers **unattributable** as the card now stands. |

---

## P1-1 — THE WALL IS STRUCTURALLY UNREACHABLE, AND "NO CEILING WAS REACHED" IS UNSUPPORTED
`test/unit/test_sled.cpp:4592` (`sled_rolled_throttle_the_wall_at_the_driven_value`);
`sim/sled.h`, the `rolled_throttle_frac` block ("★ THE WALL this value is chosen against");
the commit message ("No ceiling was reached: 0.15 needs no reduction, nor does 1.0").

The leg's own printf, run on the committed build:

```
[B1 WALL] frac=0.00 v_side= 0.0  final_tilt= 106.22 rpm= 1700.0 thrust=     0.00 roost=0.00000
[B1 WALL] frac=0.15 v_side=15.0  final_tilt= 106.20 rpm= 2645.0 thrust=     0.00 roost=0.00000
[B1 WALL] frac=1.00 v_side=15.0  final_tilt= 106.20 rpm= 8000.0 thrust=     0.00 roost=0.00000
```

`thrust_n` and `roost_flux` are **exactly zero in all nine cells** and `final_tilt` is
character-identical across `frac`. The leg replaced `:2649`'s closed thumb with a held thumb and
added an `engine_rpm` liveness assertion — but `engine_rpm` is computed at body level
(`sim/sled.cpp:1421-1445`), **outside** the contact block. So the liveness assertion proves the dial
reaches the *instrument*; the wall assertion (`tilt_deg_of(s) > 45.0`) is still measuring a machine
on which the dial has **no mechanical authority at all**. The named killing mutation — "raising the
dial until she walks herself over" — cannot be executed on this fixture at any value.

**I went looking for a fixture that CAN arm it and there is none.** MEASURED, this pass:

* **a bank.** `cross_slope_field(grade)` at 0.30 and 0.60, downed 110°, W held, frac 0 / 0.15 / 1.0:
  `thrust=0.00 roost=0.00000 track_slip=+0.0000` in every cell, `final_tilt` identical across frac
  (93.09° at grade 0.30, 86.77° at 0.60). At grade 1.00 she is not `rolled` at all by tick 240
  (tilt 4.41°), so that row is not a wall test either. **The handoff's §2.3 hope that "the bank
  strike is the run that can answer the second question" is not supported: a cross-slope does not
  give a rolled machine's track any contact.**
* **the recovery window** — the one place `rolled` can still be latched while the track re-touches.
  110° drop, side-slide **15 / 20 / 25 / 30 m/s**, W held, **1800 ticks (30 s)**, frac 0.0 vs 1.0:
  `max|T| = 0.00`, **ticks where (`rolled` && |thrust| > 1 N) = 0**, `min_tilt` 104.76–104.77°,
  `final_tilt` 106.22° — identical between frac 0.0 and 1.0.

That is 24 cells across flat, three grades, 70–178° (the handoff's own sweep), side-slide 0–30 m/s
and windows to 30 s, and the track patch **never once enters the thrust block while rolled**. The
mechanical reason sits upstream of this diff: the ground query is a radial height sample, so a
machine lying on her side has nothing under her track to load.

**What this costs.** (a) *"No ceiling was reached: 0.15 needs no reduction, nor does 1.0"* is a
safety verdict read off a fixture where the mechanism it guards does not exist — precisely the
vacuous-probe shape this project's own probe-noise-floor law forbids, and the same criticism the leg
levels at `:2649`. (b) The drive card sends Chad to run 3(b) expecting the bank strike might make her
move. It will not.

**FIX.**
1. Strike the "no ceiling" sentence from the commit message and handoff §2.2. Replace it with the
   measured statement: **at every rolled attitude and speed reachable through `step_sled`, this dial
   moves `engine_rpm` and `belt_speed_ms` and NOTHING else — there is no value at which she drives
   herself upright, because a rolled machine's track has no contact to push on.**
2. Rewrite the leg's comment to say the wall is **structurally unreachable, measured**, and keep the
   leg as the `engine_rpm` liveness net it actually is. Rename it
   `sled_rolled_throttle_reaches_the_engine_and_not_the_track`.
3. Put it at the top of the drive card in his words: **"At any value she will NOT move. Judge the
   noise. If you want her to move while she is over, that is a different rung — the track has
   nothing under it when she is on her side."**

---

## P1-2 — THE SHED FIRES AT FULL STRENGTH WITH THE THUMB CLOSED
`sim/sled.cpp:1147-1150` (the shed), reading `trk_slip` built at `sim/sled.cpp:1104-1106`.

MEASURED this pass. `in.throttle = 0.0f`, no brake, no steer, flat 0.30 m snow, 6 ticks after
`settle`:

```
[RT coast] v0= 1.5  gs=0.736  track_slip=-0.73219  thrust=-0.00
[RT coast] v0= 2.0  gs=1.176  track_slip=-1.00000  thrust=-0.00
[RT coast] v0= 3.0  gs=1.992  track_slip=-1.00000  thrust=-0.00
[RT coast] v0= 5.0  gs=2.582  track_slip=-1.00000  thrust=-0.00
[RT coast] v0=10.0  gs=5.340  track_slip=+0.00000  thrust= 0.00
```

The arithmetic behind it: thumb closed means `drive == 0`; below `clutch_engage_ms − 0.25 = 3.0 m/s`
`clutch_blend == 0`, so `v_track == 0` and
`trk_slip = clamp((0 − v_fwd) / max(0.0, 1.0)) = −1` for any `v_fwd ≥ 1 m/s`. **|trk_slip| = 1 is
the largest value the dial can ever see, and it is reached at zero throttle.** The shipped kernel
neutralises this for thrust one line later — `T *= drive + (1.0 - drive) * clutch_blend;`
(`sim/sled.cpp:1277`), which is why `thrust` reads 0.00 above. **The shed has no such decouple.**

**Failure scenario, in his language.** He clips a bank, she slows through 3 m/s, he is off the
throttle and fighting the bars. At `shed = 0.5` the track's lateral μ is multiplied by
`max(0, 1 − 0.5·1·(1+align_m))` — **half gone with no lean, all of it gone with a full lean, at zero
throttle.** Two consequences: (i) **closing the throttle does not hook the tail back up** below
~3.3 m/s, which is the opposite of the sentence the dial is built from; (ii) run 6's second owed
answer, *"DID SHE ROLL MORE"*, lands in exactly this regime and is then attributed to a throttle dial
that was not open.

**FIX** (identity-preserving; both operands are patch-scope locals already sitting two lines above):

```cpp
if (g.is_track && p.track_lat_slip_shed > 0.0)
    mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed * std::abs(trk_slip) *
                                    (drive + (1.0 - drive) * clutch_blend) *
                                    (1.0 + align_m));
```

Same decouple factor the thrust uses, so the shed exists exactly where the drivetrain is connected.
Still a branch, still 0.0-identical. **Killing mutation for the leg that should carry it:** delete
the decouple factor and a "shed at closed thumb, 2 m/s" leg reds.

---

## P1-3 — THE DIAL IS NAMED FOR THE ROOST AND DOES NOT READ IT
`sim/sled.cpp:1147-1150` vs `sim/sled.cpp:1237` (`flux`); `sim/sled.h`, the `track_lat_slip_shed`
comment.

His sentence is *"throttle should also be able to swing my tail around **on account of the roost**"*.
The kernel already owns the roost as one number — `flux = std::abs(trk_slip) * avail`
(`sim/sled.cpp:1237`, the file's own "★★ THE ONE NUMBER, TWO CONSUMERS"). The shed reads
`|trk_slip|` and **never consults `avail`**.

`avail = clamp01(loose / roost_ref_depth_m) * (1 − bury)` with
`loose = d.sinkable ? gs.depth_m : 0.0`. Road, LakeIce, RockOutcrop and MineWorks are the
non-sinkable rows, so on them `avail == 0` and `roost_flux == 0` — **already pinned** by the existing
leg `sled_road_sinkage_is_exactly_zero` (`test/unit/test_sled.cpp:2705`,
`REQUIRE(s.roost_flux == 0.0)` at rest and at 5–35 m/s). `trk_slip` is surface-blind. **So on a
plowed road — "just going down the road", his words, the surface his loudest old complaint lives on —
this dial sheds the track's lateral grip at full strength while there is by construction no roost to
swing anything with.** Same on lake ice.

This is an attack on the **decision**, not on the lane: §D specified `|trk_slip|` and the lane built
it exactly. But the spec's own justification is a friction ellipse — *"what it spends forwards it does
not have sideways"* — and a track on bare road is not spending anything forwards in the roost sense.

**FIX.** Hoist `bury` / `loose` / `avail` with the other six lines (every input they read —
`s.sink_m[i]`, `gs.depth_m`, `d.sinkable`, `p.track_clearance_m`, `p.roost_ref_depth_m` — is already
in scope at the hoist site, `sim/sled.cpp:1088`) and write the shed on `flux`:

```cpp
mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed * flux * (1.0 + align_m));
```

`flux` becomes the ONE NUMBER's **third** consumer, which is what that comment is for, and the
identity is untouched (still a `> 0.0` branch). If the audit wants `|trk_slip|` kept, then
**B3-vs-BANK must be re-asked as a ruling with the road case named**, because the term as built is a
road dial as much as a snow dial.

---

## P2-1 — `sled_tail_shed_is_track_only` CANNOT RED ON ITS NAMED MUTATION
`test/unit/test_sled.cpp:4684`. The false claim is welded into `sim/sled.cpp:1141-1142`
("★ `g.is_track` IS LOAD-BEARING … sled_tail_shed_is_track_only reds if the guard is dropped") and
into the `sim/sled.h` `track_lat_slip_shed` comment.

**MEASURED.** I deleted `g.is_track &&` from `sim/sled.cpp:1147`, rebuilt `seads_tests`, ran the leg:
`All tests passed (3 assertions in 1 test case)`.

Why it cannot red: `trk_slip` is a **per-patch local, declared `= 0.0` and assigned only inside
`if (g.is_track)`** (`sim/sled.cpp:1098-1107`). On a ski patch it is exactly `0.0`, so the mutant's
factor is `max(0.0, 1.0 − shed·0·(1+align_m)) == 1.0` and `mu_l *= 1.0` is bit-identical. The guard
is **redundant by scope**, not load-bearing, and no leg watches it.

**FIX.** Two options, in preference order.
1. **Strike the false sentence** from `sim/sled.cpp:1141-1142` and `sim/sled.h`, and rename the leg
   `sled_tail_shed_is_inert_where_the_track_slip_is_zero` — which is what it actually proves and is
   still worth having.
2. If the guard is to stay *claimed* as load-bearing, the thing that must be pinned is the
   **declaration scope**, because that is what makes it redundant: the hazard is a future lane
   hoisting `trk_slip` to substep scope as an "optimisation", after which a ski patch processed after
   the track silently inherits the track's slip and the guard becomes the only protection — with,
   today, no leg on it. A source-shape leg (this repo has the precedent) asserting the declaration
   sits inside the per-patch loop is the only killable form.

---

## P2-2 — `sled_tail_shed_hoist_is_a_pure_refactor` DOES NOT RED ON ITS NAMED MUTATION EITHER
`test/unit/test_sled.cpp:4634`.

**MEASURED.** I re-added a second copy of the slip formula at the old site —
`trk_slip = std::clamp((v_track - v_fwd) / std::max(v_track, 1.0), -1.0, 1.0);` inside
`if (g.is_track)` — rebuilt and ran. The golden line printed **character-identical** and the leg
**PASSED**. It had to: the recompute reads the same `const` inputs and lands on the same bits. The
other half of the claimed mutation ("hoisting past anything that writes `throttle` or `v_fwd`") is
unreachable by construction — both are `const double`.

So the leg is a forward regression net, not the hoist proof. **The hoist proof is the six-tape
differential in handoff §2.1, and that one is good** — the hoist-only build reproduces all six probe
outputs byte-identically, including the divergence tick and field of the three tapes that were
already divergent on the base.

Two further notes: it is the **only 17-digit absolute-trajectory golden in the whole unit suite**
(every other identity leg in `test_sled.cpp` compares two arms — `same_state`,
`a.position.x == b.position.x`), so it is pinned to this toolchain, these flags and this GLM; and its
comment names none of them.

**FIX.** Strike the "leaving a second copy of the slip formula at the old site" clause. Name the
compiler, the flags and the GLM version the golden was measured on in the comment, and say in one
line that the real proof is §2.1's differential.

---

## P2-3 — THE BANNER REPORTS ENV *PRESENCE*, NOT ENV *ACCEPTANCE*
`app/main.cpp:2511-2577`.

`env_dial` is careful — a non-numeric value warns to **stderr** and keeps the default, which is the
right call and well argued. But the banner rebuilds its `armed` string from
`if (const char* e = std::getenv(n))` alone. So:

* `SEADS_SLED_TAILSHED=0,15` (a European decimal, or a typo) → `env_dial` rejects it and keeps 0.0,
  and the banner still prints `(env: SEADS_SLED_TAILSHED=0,15)` on the one line whose entire purpose
  is *"a run's own log says what it was flown at"*.
* `SEADS_SLED_TAILSHED=` (set empty) → `if (!e || !*e) return;` — **no warning at all** — and the
  banner lists it as armed.
* `while (end && *end == ' ') ++end;` skips **spaces only**. A trailing tab, `\r` (a `.bat`, or any
  CRLF-sourced env file on this machine) or newline falls back to the default with only a stderr line
  that will scroll past under a game launch.

**FIX.** Make `env_dial` return `bool`, true only on acceptance; build `armed` from the return value;
warn on the empty-string case too; and use `std::isspace(static_cast<unsigned char>(*end))` for the
trailing skip.

---

## P2-4 — TAPES CUT ON THIS BUILD CANNOT BE LOADED BY ANY BUILD WITHOUT THE TWO NEW ROSTER ENTRIES
`test/harness/sled_tape.h:459` and `:474` — `if (!hit) return fail("unknown param", line);` and
`fail("unknown cparam", line)`.

The writer emits the **whole** roster (`Writer::begin`, `# param <name> <v>` / `# cparam …`), so
every tape Chad cuts tonight carries `# param track_lat_slip_shed` and
`# cparam rolled_throttle_frac`. `load` **hard-fails** on a name the build does not declare.
Therefore `seads_sled_probe.exe tape <tonight's tape>` built from **main**, from the **audit lane**,
or from the **sentinel tree** will refuse to open them. The Python instrument
(`tools/sled_tape_x1.py`) is tolerant and will still parse them; the C++ replay — the thing that
produces `VERDICT: bit-exact` and the `dial gap` line — will not.

This is the price of the roster law, not a defect in it, but **nothing in the handoff says so** and
the whole point of the drive is to hand those tapes to the next attribution round.

**FIX.** One line in the handoff and on the drive card: *"tonight's tapes replay ONLY on a build
carrying the two new roster entries — use
`D:/seads_sandboxes/sled-ride-b1/build/seads_sled_probe.exe`, and do not delete this lane's build
until the tapes have been read."*

---

## P2-5 — NOTHING PINS `align_m` INSIDE THE NEW TERM
`test/unit/test_sled.cpp:2546` (`sled_wrong_way_lean_is_never_a_penalty`).

The ladder names this leg as B3's guard — *"use the raw signed lean instead of `align_m` ⇒ red"*. It
constructs `const sim::SledParams p_on;` and a `lean_bite_gain`-only `p_off`, so
`track_lat_slip_shed == 0.0` in **both** arms and the new branch never executes. Nothing in the suite
distinguishes `align_m` from a raw signed lean inside the shed, and nothing distinguishes the shipped
`(1.0 + align_m)` from the named fallback form `align_m` — so the fallback could be applied, or
applied by accident, and no leg would notice.

**FIX.** Two arms on the existing B3 straight-line leg, using its own `sep()` helper:
`REQUIRE(sep(0.15f, -1.0f) == sep(0.15f, 0.0f));` — a wrong-way lean must buy the shed exactly
nothing. Under the raw-signed mutation the wrong-way arm's factor becomes `1 + (−1) = 0`, the shed
vanishes on that arm alone and the equality breaks. Add
`REQUIRE(sep(0.0f, 1.0f) > sep(0.0f, 0.0f));` to pin that the `1.0 +` form sheds with no lean **and**
more with lean, which separates it from the fallback.

---

## P2-6 — 0.5 IS A SPIN-OUT VALUE, NOT A TAIL-SWING VALUE, AND THE CARD DOES NOT SAY SO
The drive card ladders B3 as 0.15 → 0.3 → 0.5. MEASURED authority at 0.5, on the lane's own fixtures:

* `sled_tail_shed_is_inert_in_a_straight_line`'s own printf on the committed build:
  `crab=9.77529e-16 m   leaned=14.0473 m` — **14 m of trajectory separation over 10 s** at
  `lean_lat = 1.0`, WOT, zero steer.
* A corridor run, WOT / steer 0.6 / lean 0.7, 5 s from 10 m/s: shed 0.0 ends at **13.07 m/s**,
  shed 0.15 at **11.53 m/s**, shed 0.5 at **1.42 m/s with yaw −1.48 rad/s** — she spun and stopped.

**FIX.** Put those three numbers on the card beside run 6, so a spin at 0.5 reads as the dial working
rather than as a broken build, and say that 0.3 is the top of the useful band on this evidence.

---

## P3 — SMALLER, NAMED
* **P3-1 (B1, identity, unreachable).** `throttle = 0.0 * thr_in` is not the shipped literal `0.0`
  when `in.throttle` is NaN: `clamp01` is `std::clamp`, which returns NaN, and `0.0 * NaN = NaN`
  where the old ternary returned `0.0`. `-0.0` is benign (traced through `v_cmd`, `drive_t` and
  `rpm_frac`: all land on the same bits). Not reachable from the keyboard; recorded so the next lane
  does not have to re-derive it.
* **P3-2 (B1, no range clamp).** `SEADS_SLED_ROLLED_THROTTLE=5` produces `throttle == 5.0`, escaping
  the `clamp01` invariant every downstream reader was written against — `v_cmd = throttle *
  p.track_speed_max_ms` has no clamp (`rpm_frac` survives because it re-applies `clamp01`). Also note
  the wall leg's `armed.engine_rpm == 1700.0 + frac * 6300.0` is only valid for `frac ≤ 1`.
* **P3-3 (B2b).** In `selfright_a_timed_rock_beats_a_mistimed_one`, `at_quarter` is `16.0 − timed`
  because the mistimed arm never rights, so `REQUIRE(at_quarter > at_candidate)` is a statement about
  `total_s`, not about the mechanism. Assert the discrete fact instead (`mistimed.t_right < 0.0` at
  0.25) and keep the ordering only between 1.0 and 0.5.
* **P3-4 (LAW).** "Wheelie/drift/backflip untouched by construction and **proven by a leg**" — there
  is no leg named for it in the diff. The coverage is the 2144-test gate equalling the baseline six
  by name, which is adequate but is not the named leg.
* **P3-5 (wording).** The exe is not byte-identical to main: the banner `std::printf` is
  **unconditional**, so a no-env run emits one extra `[config]` line. Nothing in the gate or the
  tools parses it. The commit message should read *"the simulation is bit-identical"*.
* **P3-6 (pre-existing, not this diff, but it is what B3 is built on).** `roost_flux` is already
  non-zero while coasting with the thumb shut: MEASURED 0.15867 at 1.18 m/s and 0.20013 at 1.99 m/s,
  `in.throttle = 0`. `trk_slip` was a poor proxy for "spending the budget forwards" at low speed
  before this diff; B3 now builds a second mechanism on it. See P1-2.

---

## WHAT I TRIED TO BREAK AND COULD NOT
* **Env parsing as an identity leak.** With nothing set, all five `env_dial` calls return before
  touching `sled_params`; there is exactly one `sim::SledParams` in `app/main.cpp` (`:2474`) and
  nothing reads it between `:2477` and the env block. `sled_tape.begin(..., sled_params, ...)`
  (`app/main.cpp:6102`) records the armed values, so a tape can never disagree with its drive.
* **The banner as a side effect.** It touches `stdout` only, after the assignments, and reads
  `getenv` a second time — no state.
* **Roster order.** The header is written as `# param <name> <value>` lines and `load` matches **by
  name** (the `SLEDTAPE_R` chain), so inserting `track_lat_slip_shed` between `track_lat_mu` and
  `plane_fit_load_weight`, and `rolled_throttle_frac` between `rolled_grace_s` and `roll_stiff_nm`,
  is order-free. Both new defaults are `0.0` = the OFF value, so the tape-absent rule at
  `sled_tape.h:392-418` needs no new pre-set and every older tape reconstructs the kernel it was cut
  on.
* **`dial_gap` on an OLD tape.** It now names `rolled_throttle_frac, track_lat_slip_shed` and the
  probe downgrades the verdict to "bit-exact ON A KERNEL THIS TAPE NEVER SAW". That is the machinery
  working. §D's proof obligation demanded `dial gap: none` **and** predicted the gap in the same
  document — the spec contradicts itself, the lane resolved it the right way and recorded both
  tables.
* **The hoist changing a value the lateral bite reads a tick earlier.** `throttle` (`:308`) and
  `v_fwd` (`:1019`) are both `const double` and nothing between the new site (`:1088`) and the old one
  (`:1206`) can write them; there is no `continue` between the two sites; `mu_l` is read only by
  `bite`. The tripwire `grep -c "v_track - v_fwd) / std::max(v_track" sim/sled.cpp` reads **1** (the
  loose form still reads 2 — `roost_thrust`'s `v_rel` — exactly as §4.3's fold said).
* **The B1 complement slip.** I wrote `(1.0 - p.comfort.rolled_throttle_frac) * thr_in`, rebuilt, and
  `sled_rolled_throttle_frac_zero_is_the_shipped_zero` **redded** (`8000.0 == 1700.0`). That leg does
  its job, and `sled_rolled_throttle_never_reaches_a_handless_rider` correctly ignored the mutation.
* **The gate verdict.** 6 of 2144, red set == the baseline six by name, `gate_baseline.py check`
  exit 0, `grep -cE '^ *[0-9]+/2144 Test '` = 2144 so the log is complete. The four snow reds were
  re-proved pre-existing by stashing. No objection.
* **Graph provenance.** Re-running `tools/graph/graphify.py` changes only
  `generated_at_commit: 533c86409 → 58f6e4dd7`; main's own graph stamps its parent too (`533c86409`
  carries `d9ee51b6a`), so this is the house convention, not drift. File restored.

---

## THE SHORTEST PATH TO LAND-WORTHY
1. Strike the "no ceiling was reached" verdict; publish the measured replacement (**P1-1**) and put
   *"at any value she will NOT move"* at the top of the drive card.
2. Gate the shed on the drivetrain (**P1-2**) — three tokens, identity-preserving — before Chad
   drives run 6, or run 6's second answer is not attributable.
3. Strike the two false "killed by" claims (**P2-1**, **P2-2**) from `sim/sled.cpp`, `sim/sled.h` and
   the two leg comments. A leg that cannot be killed must not be *described* as a wall.
4. One line on the drive card about the tape lock-in (**P2-4**) and one about 0.5 being a spin
   (**P2-6**).
5. Take **P1-3** to Chad as a ruling, not as a patch: *"you said 'on account of the roost' — do you
   want the tail to let go on a plowed road, where there is no roost at all?"*
