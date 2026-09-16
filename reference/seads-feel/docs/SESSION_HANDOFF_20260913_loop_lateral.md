# SESSION HANDOFF — loop rollover + lateral nose-down (2026-09-13)

Lane: `feel/lean-lead-walkback` in `D:\flight_sim2\seads-feel`.
**LAUNCH LINE: read §0, then §6 (the lessons) before touching anything.**

---

## §0 STATUS

**READ THIS FIRST: the 2026-09-13 overnight lateral verdicts were VOID.**
Not "inconclusive" — void. They were measured on a state nobody ever flew.
The dial may be fine or useless; the battery could not tell, and reported
confident numbers anyway. §6.7 has the anatomy. The one-line version:

> The tape's row writer emitted **74** columns while its header named **52**.
> Every v5 seed field was on disk and named nowhere. The probe indexed by
> name, its reader returned **0.0** for anything it could not find, and so
> "Chad's 3 worst dives" replayed with zero body rates, a degenerate
> `(0,0,0,0)` aim quaternion and a freshly reset controller. Neither arm was
> ever in a dive, so ON == OFF — a fact about the harness, not the aeroplane.

**LANDED ON THE LANE (not pushed, awaiting Chad's land word):**

| SHA | what |
|---|---|
| `0a52e7f61` | S-lapguard: the lap latch (SUPERSEDED — removed below) |
| `039c65317` | **S-righthand**: `[auto_level] right_hand_rest` 0.25 |
| `37d5f5d83` | S-lapguard machinery REMOVED outright |
| `3d6de7ec1` | S-righthand red-team fold: the hand-rest clock is frame-rate invariant |
| `4768ab263` | docs: this handoff |
| `62ceaeff3` | feel tape v5 — **its "proved by a round-trip test" was an IN-MEMORY proof only** |
| `9b3aaec3e` | docs: handoff §0 |
| *(this)* | **tape emitter/reader correction** — §0.1 |

**S-righthand is the one thing on this lane Chad has confirmed on the stick:**
> "I can do the vertical loops and immelmans without a hitch."

That confirmation stands. It is a felt verdict on the real exe, and nothing in
this correction touches the kernel.

### §0.1 The correction (this commit)

1. **`app/feel_tape_columns.h`** — the column list, ONE definition. The writer
   prints its header from it; every reader resolves names through it. A
   hand-written header is what let 52 drift from 74.
2. **`app/feel_tape.h`** — the writer hook lifted out of `main.cpp` (which is
   not linked into the tests) so the round-trip test can drive **the shipped
   writer**. It previously round-tripped a struct it also owned, which is why
   it agreed with itself while the real emitter was broken.
3. **`test/harness/feel_tape.h`** — the reader. It **throws** on a missing
   required column, listing every one, and refuses a tape whose header count
   disagrees with its row width. Silent-zero is banned outright.
4. **`test_tape_roundtrip.cpp`** — now END-TO-END: fly → the real hook writes a
   CSV → the throwing reader parses it → seed mid-tape → replay. Plus a
   regression that a v4-era 52-column tape **throws** instead of defaulting.
   Measured: 74 written, 74 named, first replayed tick `|dphi| 2.5e-07 deg`.
   The in-memory case stays as a unit, but it is no longer the proof.
5. **`test_loop_rollover.cpp`** — its reader was POSITIONAL with v2-era
   offsets (`alt = v[29]`); at 74 columns that index is `hand_rest`, so the
   bench was grading altitude against the hand-rest timer. Now name-indexed.

### §0.2 The real lateral distributions

Read from the recorded columns directly — those are sound; only the *seeded
counterfactual* was void. **Sign, verified in source:**
`elev_gap = theta - aim_elev` = **nose − aim**, so **negative = nose BELOW
aim** = Chad's complaint. The first gate was reasoned about with this inverted.

| discriminator | diving | tracked | ratio |
|---|---|---|---|
| position: gap < −20° | 33.6% | 3.1% | **10.8×** |
| rate: sink > 20°/s (τ=0.1 s) | 34.4% (at onset) | 5.2% | 6.6× |

**The position gate separates better than the rate gate.** Weigh this before
building either: on the heaviest dive in the tape (t=146.5, `dAlt −93.8 m/s`)
the gap only moves `0.8 → −3.5°` and sink p50 is `1.2 °/s` — **neither gate
opens meaningfully on the worst event.**

### §0.3 The one real cost measured

The V250 hard-turn COST case is **scripted** (`harness::level_trim_state`, no
tape), so it was never void. At `yaw_vert_budget = 1.0`:

| case | OFF | ON |
|---|---|---|
| lat 40 | turn 33.88, yaw −27.79 | bit-identical |
| lat 90 | turn 41.01, yaw −30.85 | **turn 41.82, yaw −28.28** |

A genuine cost with, as of now, **no demonstrated benefit**. The prototype is
parked in the session scratchpad (`S-yawbudget_prototype.patch`), unbuilt on
the lane.

### §0.4 NEXT STEP — in order; do not skip step 1

1. **ONE fresh lateral tape from Chad on the fixed emitter.** Nothing about the
   lateral dive can be graded until this exists — his current tapes are
   52-column and the reader will now (correctly) refuse them.
2. Then the battery for real, with the **position-gated prototype, correctly
   signed** (`sag = aim_elev - dot(nose, up)`, positive = nose below) as the
   first candidate: it was never actually tested, and it separates best.
3. Only then consider the rate gate, and only if the position gate fails on
   DATA rather than on a harness fault.

**PARKED, awaiting Chad's ruling: the lateral nose-down.** He reports, on both
the lane exe and on `seads-recon` at main-v14: *"that one still crashes me to
the ground from lateral deflection"*, *"loops are good but still diving down on
laterals"*. The mechanism is found and MEASURED (§3). No pitch-side dial can
fix it. The remaining lever needs his word — see §3.4.

**Dials live on the lane:** `[auto_level] right_hand_rest = 0.25`,
`lean_lead = 0.3`, `lean_lead_lateral = true`, `[horizon_recovery] rate = 150`,
`straight_max = 12`. `lap_roll_frac` no longer exists.

---

## §1 CHAD'S RULINGS — VERBATIM. These are the spec.

**THE GUN-DIRECTOR LAW (the governing one for the lateral work):**
> "the plane should follow my mouse; the best way to think is that I am
> directing my guns and an instructor would never crash me into the ground if
> I did not mount my mouse anywhere near there."

> "If I keep inputting upward deflection the airframe should stay in its
> orientation right around the loop... an Immelmann, where I stop inputting
> deflection at the top, auto rights the airframe."

> "the loop is sustained by me sustaining the motion, if I change the motion it
> should change the behavior"

> "direction can be changed 180 degrees with pitch alone that is an immelman"

> a held pull must stay clean for **"as many as I wish"** consecutive loops.

> "when I make a large sideways deflection of my mouse and ask the plane to
> follow it, it noses down crashing me if I am near the deck."

> "I can do the vertical loops and immelmans without a hitch. The sideways
> deflections sent me packing dirt everytime, the upward one still exist too.
> But you got it mostly good work."

**STANDING (2026-07-06, still binding):** fix turn problems via
**coordination/skid, NOT a bank cap**. `maneuver_bank_max` was removed in
`be47e351d` because a cap made turns 44°/1.5 G and he rejected it.
`docs/section7_worklist.md:317` — "a bank cap flattens turns but they go
low-G... skid via coordination, not a bank cap."

**STANDING (2026-08-06):** inverted righting carries no added delay once the
rest condition is met. PRESERVED by `right_hand_rest` — the rest condition now
includes the hand; `inverted_delay`/`inverted_rate` untouched.

---

## §2 S-righthand — what it is and why

**The defect, decomposed from Chad's own tape** (`feel_tape_loop.csv`, 10134
ticks), at his loop apex t=18.02. The roll command jumps **−0.7 → −122.8 °/s
in ONE tick** while the wings move 0.01°:

```
t=18.017  cosPT +0.002  phi -0.80  phi_full   -0.80  rhd   0.25 deg ->   1.2
t=18.025  cosPT -0.002  phi -0.81  phi_full -179.19  rhd 178.64 deg -> 893.2
```

`cos_phi_theta` crosses zero because the **NOSE is past vertical** (theta 89.1),
not because the aeroplane is inverted. `unfold_bank` then reports the bank as
−179.2° instead of −0.8°. Two terms follow:

1. the FINE wings-hold via that 180° flip, faded by `wings_level_gate`
   (123 → 0.6 °/s over 0.13 s) — **DEFERRED, P3-a**, integrated per apex on his
   tape: 8.6 / 1.3 / 6.5 / 6.8 / 0.0 / 0.5 deg, inside the ~10° bound;
2. **MB-right at EXACTLY `inverted_rate`** — `wdz = −180.0 °/s` held through
   the apex. It arms because its "at rest" test is `err < blend_lo` and his
   tracking err there is 4.6°, with `inverted_delay = 0`. He is mid-loop with
   the nose 86° up and the mouse still moving.

**The dial:** MB-right's roll AUTHORITY scaled by
`smoothstep(0, right_hand_rest, hand_rest)`. Continuous, no latch.

**Measured on his flight** (tape 2, flown at 0.25): MB-right wanted to fire on
2344 ticks; the veto suppressed **2178 (92.9%)**. At the six apices, wings level
at every one (max |phi| **9.4°**) against 180.0 °/s un-vetoed.

**⚠ COVERAGE:** the broad gate gives this veto ~ZERO coverage — harness
`ClosedLoop::aim_moved` defaults FALSE, so every suite scenario flies a hand at
rest and the gate reads 1.0. **The apex probe in `test_loop_rollover.cpp` is
the sole instrument.** A green gate says nothing about this dial.

**DEBT (deferred, in `params.h`):** the TREMOR case — "hand is live" is ANY
nonzero aim motion, so a ±1-count/frame tremor while belly-up caps the gate
(integrated righting 1.76° vs 117.75° with a still hand). Cure = windowed NET
aim displacement; a mechanism change, not a dial.

---

## §3 THE LATERAL NOSE-DOWN — mechanism found, dial PARKED

### 3.1 It is not v14 and not the lap guard

`lean_lead` 0.3 vs 0.0 changes altitude by **≤3 m** on every lateral sweep, and
the 0.0 arm reproduces the pre-v14 baseline (`c75bc502d`) **to the metre**:

| lat °/s | lean_lead 0.3 | lean_lead 0.0 | pre-v14 |
|---|---|---|---|
| 150 | −566.2 m | −566.7 | **−566.7** |
| 300 | −67.8 | −66.0 | **−66.0** |

### 3.2 The mechanism, from his real trajectory (`feel_tape_lateral2.csv`)

Worst event t=49.50: **dAlt −448 m**, gap 72°, phi 68°, **V 181 → 181 (NO
BLEED)**, n_pk 15.5, push 0%.

```
t=49.48  n=10.2 V=181 phi=-17.8 theta= -8.5 gap= -7.3 blend=1.00 wdx=+46.8 r_man=103.2
t=50.98  n=12.8 V=183 phi=-60.7 theta=-26.0 gap=-20.4 blend=1.00 wdx=+38.5 r_man= 12.4
t=51.98  n=13.9 V=187 phi=-46.7 theta=-39.3 gap=-30.4 blend=1.00 wdx=+36.1 r_man= 25.7
```

**No term pitches the nose away from the aim.** `wdx` is a sustained
**+34…+47 °/s nose-UP** command throughout; `push_mode` 0%; `roll_hold` and
`roll_right` both 0.0. The nose falls because at 55–66° of bank that demand
buys only `cos(phi)` ≈ 40–57% of world-vertical rate.

### 3.3 THE CLAMP — why no pitch-side dial can fix it

Read from the live controller on his seeded dives:

| | t=47.983 | t=184.25 | t=144.5 |
|---|---|---|---|
| AoA peak | 19.0° | 19.2° | 18.4° |
| **AoA pushback binds** | **112/720** | **333/720** | **187/720** |
| G ceiling binds | 0/720 | 0/720 | 3/720 |

```
t=50.48  AoA 17.4  w_max 98.2  pitch_ceil 26.0  wdx 25.9   <- pinned to the ceiling
```

`wdx` sits **exactly on `pitch_ceil` = `K_aoa·(aoa_max − α_f)`** while the G
budget `w_max` is 3–4× higher. There is **no `q_max` pitch-rate cap** in the
config; the ceiling is AoA.

**This is why `pitch_bank_comp` (candidate a) was inert:** it scales the demand
*before* the clamp, and the AoA pushback truncates it straight back. Multiplying
a number the next line overwrites changes nothing. **(a) is buried.**
**(c), a V-aware G ceiling, is dropped** — there is no bleed to prevent (V is
steady or rising on every worst event).
**(b), `bank_yaw_trade`, is dead on arithmetic:** at ω = 26 °/s, V = 200,
`a_lat` = 90.8 m/s²; pure bank needs φ = 83.8° (n = 9.3); cutting to n = 6 needs
**3.35 g of fuselage side force**, which does not exist. Skid buys ~`Y/W`.

### 3.4 THE REMAINING LEVER — needs Chad's ruling, DO NOT BUILD

With pitch AoA-limited and G unbinding, the only lever is **the bank the turn
asks for at a given aim** (`bank_error` sizing). The trade to put to him:

- today a lateral aim commands a bank the pitch channel cannot support at its
  AoA margin, so the nose leaves the aim and the aeroplane descends;
- bank only as much as the pitch channel can still hold the aim's elevation at
  the current AoA margin — **aim-conditioned**, not a fixed ceiling;
- hard turns stay hard when the nose can follow; it softens **only** when the
  alternative is losing the aim into the ground, which is the gun-director law.

This is close to the `maneuver_bank_max` he rejected. The difference — fixed
ceiling vs aim-conditioned — is the whole question, and it is **his** call.

**THE INVARIANT to grade any fix** (measured on his tape): whenever
`|aim_world_elev| < 45°` and `err > blend_lo`, require
`|nose_world_elev − aim_world_elev| ≤ 30°`. **19 of 61 events violate it
today**, worst **100.3°** (at only 14° of bank).

---

## §4 THE FEEL TAPE + REPLAY — the instrument that settled all of this

**Recipe.** App-side, the `step_frame` `TickHook`; zero cost when unset.

```powershell
$env:SEADS_FEEL_TAPE="D:\flight_sim2\seads-feel\build-play\feel_tape_X.csv"; & "D:\flight_sim2\seads-feel\build-play\seads.exe"
```
fly, quit, then:
```powershell
$env:SEADS_FEEL_TAPE="...\feel_tape_X.csv"; .\build\seads_tests.exe "S-righthand: replay a recorded feel tape"
```

Every launch also appends the dials to `build-play\seads_launch.log` (Explorer
launches have no stderr — Chad: *"it just opens the game no config lines"*).
stderr, the launch log and the tape header come from ONE string, so a flight can
never be attributed to dials it did not fly.

**⚠ TAPE VERSION STATUS.** v5 (aim-frame quaternion + `Internal` latches +
`aoa_ceil`/`w_max_p`) is IN THE TREE BUT NOT YET SUFFICIENT. The round-trip test
(`test_tape_roundtrip.cpp` — flies a banked turn, re-seeds mid-window from the
v5 columns alone, any divergence is a missing column by construction) reads:

```
seed tick 300 (phi 87.6) -> 7.50 s: |dphi| 1.083 deg  |dtheta| 0.497  |dalt| 2.549 m
seed tick 600 (phi 15.9) -> 5.00 s: |dphi| 1.580 deg  |dtheta| 1.314  |dalt| 1.798 m
seed tick 900 (phi 64.3) -> 2.50 s: |dphi| 0.774 deg  |dtheta| 0.531  |dalt| 0.408 m
```

That is ~100× better than v4 (which diverged **~100° of roll** in 6 s) but not
the < 0.01° needed. **Still missing:** the S-rimshot capture block (`cap_ux`,
`cap_uy`, `cap_w_hold`, `cap_crossed`, `cap_err0`, `cap_d_allow`,
`cap_stall_ticks`, `cap_refractory`, `cap_inbound`, `cap_rim_t`), `ovr_ramp`,
`aim_rate_filt`, `any_override`, `push_mode`. A half-seeded `capture` is the
likeliest single culprit. **Finish against the round-trip; it needs no flight
from Chad.**

---

## §5 THE SCARS — do not rebuild these

- **`maneuver_invert_band`** (landed `31d109915`, walked back `157245d84`,
  removed): a `cos_phi_theta` fade on the maneuver roll limb is a SELF-LOCKING
  WALL — reaching inverted REQUIRES rolling through `cos == 0` and that limb is
  the only roll that does it, so the airframe parks at the knife-edge. Broke
  AT-15, the mouse-DOWN split-S and the capture jink.
- **`lap_roll_frac`** (removed `37d5f5d83`): the aim-laps-the-nose guard fired
  on **0 of 10134 ticks** of Chad's flying. Its premise (`target_body.z > 0`)
  occurs on zero ticks of his hand. Cost 272 m on a 300 °/s lateral sweep — a
  crash mode that did not exist before it.
- **`maneuver_bank_max`** (removed 2026-07-06 by Chad): see §1.

---

## §6 LESSONS — read these before the next rung

1. **TAPE THE PILOT FIRST.** Both removed mechanisms were built and validated
   against scripted sweeps **2–8× faster than Chad's actual hand** (his loop
   pull is 22 °/s median, p90 58; the probes used 90 and 180). The first tape
   falsified the entire premise in one reading: `lap` fired 0 times,
   `target_body.z > 0` never happened. **Do not build a feel mechanism before
   a tape of the manoeuvre exists.**
2. **A NAME-FILTERED `ctest -R` IS NOT A GATE.** `ctest -R "...|cascade|
   acceptance"` filters TEST NAMES — `AT-15: …` and `roll_target_mix: …`
   contain neither word, so the subset skipped exactly the tests that caught a
   4-new-red regression, and it was reported green. **Run
   `tools/gate/gate_baseline.py check`, or name the tests.**
3. **SCRIPTED PROBES MUST MATCH HIS HAND.** A probe that flies a different
   manoeuvre grades nothing: the first S-righthand probe reached `p_max` via
   the maneuver limb where his tape shows MB-right owning the apex at −180 —
   ON and OFF hashed IDENTICAL. Craft the state at his MEASURED condition.
4. **A test file's tail can be silently truncated** by a `s[:a] + new` rewrite;
   the fast set then passes because the legs are gone. Count `TEST_CASE`s after
   any scripted edit.
5. **Python edits rewrite LF files as CRLF** (`app/main.cpp` is LF; the rest of
   this checkout is CRLF). Normalise before committing or the diff is 24k lines.
6. **Gate a `build-play` rebuild on `tasklist` IN ITS OWN STEP** — twice this
   session a rebuild ran while a session was open because the check and the
   build shared a command.

7. **§6.7 — THE VOID BATTERY. A reader that defaults is a reader that lies.**
   This one cost a night, so it gets the full anatomy.

   Three defects composed:

   - `app/main.cpp`'s row writer grew to **74** columns (v4's per-tick seed,
     then v5's aim quaternion and `control::Internal`), but its header string
     was hand-written and stopped at **52**. The data was on disk, named
     nowhere.
   - The probe's CSV reader resolved columns BY NAME and returned **`0.0`**
     for any name it could not find — no warning, no error.
   - So seeding "Chad's 3 worst dives" silently produced `angular_vel = 0`, an
     aim quaternion of `(0,0,0,0)`, and a freshly reset controller. The
     aeroplane was not diving. Both arms of the counterfactual followed the
     same non-dive and the dial measured **ON == OFF** — which was then
     reported as evidence about the dial.

   Why nothing caught it: `test_tape_roundtrip.cpp` round-tripped an
   **in-memory `struct Snap` that the test itself owned**, and the writer it
   was supposedly proving lived in `main.cpp`, which is not linked into the
   tests. Its comment `// EXACTLY the v5 tape columns` was an assertion, not a
   check. Commit `62ceaeff3`'s "proved by a round-trip test" was therefore
   true of the struct and false of the tape.

   The same class bit a second time in the same file set:
   `test_loop_rollover.cpp`'s reader was POSITIONAL with v2-era offsets
   (`alt = v[29]`). Once the row reached 74 columns that index is `hand_rest`,
   so the bench was grading altitude against the hand-rest timer.

   **The rules that follow, now enforced in code:**
   - A reader **NEVER** defaults a missing column. `harness::FeelTape::load`
     takes an explicit required-name list and **throws**, naming every missing
     column and both column counts.
   - The writer never spells a header and the reader never spells an index.
     Both derive from `app::kFeelTapeColumns` (`app/feel_tape_columns.h`).
   - A format proof must run **the shipped writer**. If the writer is
     unreachable from the tests, MOVE IT (`app/feel_tape.h`) rather than
     simulate it. The end-to-end case asserts header-count == row-width, which
     is the exact divergence that was invisible for a night.
   - **Check the sign against the source, not against a remembered table.**
     `elev_gap = theta - aim_elev` = nose − aim, so NEGATIVE is nose-below-aim.
     A whole gate was reasoned about with this inverted.

   The general lesson, which is the expensive one: **the battery agreed with
   itself at every step.** The tests passed, the probe ran, the numbers were
   plausible and internally consistent. Nothing was wrong except that the
   instrument was not connected to the aeroplane. When a counterfactual
   reports "no difference", suspect the harness BEFORE believing the dial —
   and prove the seed reproduces the recorded truth before reading anything
   downstream of it.

---

## §7 WHAT TO DO NEXT

1. Finish the tape v5 column set against `test_tape_roundtrip.cpp` until all
   three seeds read < 0.01°. No flight needed.
2. Put §3.4 (aim-conditioned `bank_error` sizing) to Chad as ONE question with
   the trade named. Do not build it first.
3. Chad's land word on S-righthand (`3d6de7ec1`), then push the lane.
4. Deferred, with numbers: P3-a (§2), the TREMOR debt (§2), the 40 °/s V140
   slow-pull rollover (untouched by any dial, pinned ON==OFF so it stays
   visible).
