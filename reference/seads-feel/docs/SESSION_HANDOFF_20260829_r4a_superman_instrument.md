# R4a — SUPERMAN IS THE BUCK (2026-08-29/30)

Branch `sandbox/r4a-phase0`, head **`86b4bf4e1`**, **NOT pushed**, tree clean,
**0 behind `origin/main`** (their energy envelope is merged in).
Gate **1731/1737**.

> ★★★ **READ §5 FIRST.** Chad ruled superman is **THE BUCK**, which inverts §1–§4:
> the pose spring is not the defect, it is HIM, and the real defect is a TIME
> CONSTANT. §1–§4 are true physics answering a question he did not ask.
>
> ★★★ **THE ANSWER HE IS WAITING ON A DRIVE FOR:**
> `SEADS_BUCK_GAIN=0.02 SEADS_BUCK_DECAY=3.0`, exe at
> `D:light_sim2\seads-reconuild-play\seads.exe` (built, ready).
> Unset either var = today's build, bit-identical, for the A/B.

## ⚠ THE GATE — SIX REDS, AND THE SIXTH IS NOT OURS

The five long-standing: `probe P-F`, `sled_slides_before_it_tips`,
`sled_grip_ceiling`, `sled_assist_reference_plane`, `sled_debug_sink`.
**Match on NAME, never index — indices moved when tests were added.**

The sixth is **`E12.1: the raider backfill keeps a faction's pump offense alive`**,
and it is **ATTRIBUTED, not assumed**: the enemy-AI handoff says E12/E12.1 were
GREEN in their tree at 1582/1588. Rather than argue from "my changes are
render-side and ship OFF" — an argument, not a measurement — I reverted my whole
source surface to `origin/main`'s inside the merged tree, rebuilt, and re-ran it:
**still red**. It arrives from main and belongs to that lane. Their own handoff
warns in bold against reporting "six reds, same as baseline"; this is that warning
paying off in the other direction.

---

## §0 STATE — what changed under you

`main` advanced by snow's whole R5 immersion line and is merged in at `78303c345`.
The `sled_sag0_m()` shared call survived; the local lambda did not come back, as
the previous handoff required.

**Chad ruled that the enemy-AI session's work is theirs to land, not mine.** That
session asked to take over this worktree; it is HELD and they have been told so.
Nothing of theirs was pushed or merged by me.

---

## §1 THE RUNG THAT WAS ASKED FOR, AND WHY IT BECAME A DIFFERENT ONE

The launch question was "why does the rider not superman", with a pointer from the
snow session: *the body chain is probably not fed relative wind.*

**That hypothesis is wrong at the plumbing level.** `render/sled_model.cpp:4148`:

    bin.wind_mps = -(model_from_body_b * rig.vel_body_mps);

The wind is wired, every frame, alongside the four frame terms. Read the call site
before forming a theory — the handoff said so and it was right.

### What is actually happening

Two numbers, both in the source, decide it:

| | value | where |
|---|---|---|
| body drag | `drag_k_per_m = 0.005` | `sled_model.cpp:3956` |
| pose spring | `pose_stiff_hz = 1.0 / max(arm, 1e-3)` Hz | `body_drive.cpp:118` |

At `arm = 1` — fully released, the loosest the stage machine can ever be — the
spring runs at 1.0 Hz, so `w² ≈ 39.5 s⁻²`. Honest body drag at 25 m/s delivers
`0.005 · 625 ≈ 3.1 m/s²`. **The spring outmuscles drag about 40:1** and holds him
within centimetres of the seated pose. Nothing in the design lets drag win.

**And the scarf works because it has no pose spring at all** (`pose_stiff_hz`
defaults 0, `n_pose` defaults 0) *and* 30× the drag — it is a ribbon. LADDER §7.6's
"superman IS the scarf" silently assumed scarf-like drag-to-mass. Same solver does
not imply same behaviour. That is the spec-level gap.

---

## §2 THE INSTRUMENT WAS MEASURING A DIFFERENT MECHANISM — TWO DEFECTS

This is why four correct pseudo-force terms could measure clean over 31 tapes and
the shipped rider still not superman.

1. **The probe had no pose spring.** It was written before the spring existed. So
   every number `probe_superman` ever published described a *springless* chain —
   not the thing in the game, and specifically not the term that dominates.

2. **It filtered free fall out at the sample.** `keep = upright && aeff > 2.0`,
   with the note *"a rider in free fall does not superman, he floats."* The note is
   right that a chord-angle-from-down is meaningless with no down. But `aeff` is
   the **pseudo-field only** and carries no drag term, so the filter also discarded
   the regime the hunt cared about. The statistic was protected; the finding went
   with it.

### What was built

`tools/sled_probe.cpp` only — a measurement tool. **No shipped dial moved.**

- **Arm C5** — all four terms *plus* the shipped spring, transcribed verbatim from
  `render/trail_chain.cpp:566-572` (same critically-damped form, same
  `0.25/(π·dt)` clamp, same slot: after the field and the three per-particle
  terms, before damping). Verified by red-team as a faithful transcription.
- **`pose_hz` defaults 0**, so arms Cf and C4 are **bit-identical** to before and
  every previously published number still reproduces. Red-team verified this.
- **A wind-referenced metric** that stays defined at zero field.
- The report prints **before** the "too gentle" early-return, because a tape can be
  gentle in field tilt and still contain a fast jump.

---

## §3 THE MEASUREMENT — READ §4 BEFORE QUOTING IT, AND §5 BEFORE BUILDING ON IT

Free-fall window, downwind displacement from the pose, 1.75 m chain:

| tape | | p50 | p90 | max |
|---|---|---|---|---|
| 100 | shipped 1.0 Hz spring | 0.023 | 0.112 | 0.246 |
| 100 | **spring off** | 0.214 | 0.634 | 0.708 |
| 113 | shipped 1.0 Hz spring | 0.001 | 0.121 | 0.562 |
| 113 | **spring off** | 0.067 | 0.700 | 1.273 |

**The spring is priced at roughly half a metre of downwind travel.** On a 1.75 m
body, 1.27 m is Superman; 0.56 m is a man sitting still.

---

## §4 ⚠ WHAT THE FIRST CUT OF THIS RUNG CLAIMED THAT WAS FALSE

A fresh-context red-team caught two claims that were wrong. Both are retracted in
the source banner and, more importantly, **instrumented** — the tool now prints its
own limits.

**RETRACTED: "drag is the only live differential force in free fall."** False twice.
The `|g_eff| <= 2` window collapses only the *uniform* field, so the three
per-particle terms keep running — tape 100's free-fall window carries `om²L` median
**7.1**, max **118**, against drag's ~3. And the window admits 2 m/s² of residual
field, worth 0.051 m of spring equilibrium on its own, which is **larger than the
whole with-spring median it was being used to explain**.

The new isotropy control convicts it immediately: free-fall `perp` p50 **0.0735**
against `disp` p50 **0.0230** — the across-wind displacement is **three times** the
along-wind one. **The absolute free-fall `disp` is not a wind reading.**

**RETRACTED: "the RIDING median independently reproduces the closed form."** It used
the same `drag_k` and the same spring as the code being run — an internal
consistency check, not independent. And it assumed 25 m/s; the tape's measured
RIDING wind median is **15.9 m/s**, where the closed form predicts 0.032 m against a
measured 0.076. The agreement was a coincidence of the speed I picked.

**WHAT SURVIVES is the differential.** Both arms eat the identical contamination, so
the gap between them prices the spring honestly. **Quote the gap. Never quote the
absolute as a wind effect.**

**CORPUS CORRECTION:** the 116 `sled_tape_*.sledtape` files are **44 distinct
drives** by md5 — one content appears **23 times**. Any corpus claim uses 44. The
line "the constants were measured over 31, we now have 116" is wrong.

---

## §5 ★★★ CHAD RULED IT, 2026-08-29 — IT IS THE BUCK, AND MY DIAGNOSIS WAS INVERTED

His words, verbatim (also in `tools/sled_probe.cpp` above `struct Air`):

> "i mean the buck, big bumps buck him up not off, yes the initiate of the superman
> is the bump the delta v, the acceleration, he may freefall but during that he
> would pull himself back to the seat, If it is too big a bump he may land in
> superman pose, that second, unseated hit from the landing would then cause his
> grip to let go. Not every time but if the bump is hard enough, then the superman
> should sustain until the landing and the hardness of the landing casue for
> letting go or the bars and falling off to the side."

**What it settles:**

1. **The bump initiates it** — "the delta v, the acceleration". That is the four
   pseudo-force terms, **already built and already shipping**. Not drag, not the
   relative wind. Free fall is where superman is *carried*, never where it is born.
2. **★★★ THE POSE SPRING IS NOT THE DEFECT. IT IS HIM.** "he may freefall but during
   that he would PULL HIMSELF BACK TO THE SEAT" is precisely what the spring does,
   and he has ruled that ordinary case **IN**. **Softening it — where §3 and the
   first cut of §5 pointed — would have DELETED the behaviour he asked for.**
3. **The question is DURATION**: "the superman should sustain until the landing",
   but only "if the bump is hard enough", and "not every time".
4. **The landing is a SECOND event**: an unseated hit releasing the grip, "falling
   off to the side" — lateral, not straight back. Nothing of this is built.

### THE DEFECT, MEASURED — it is a TIME CONSTANT

New buck → sustain → landing window analysis. SUSTAIN = how much of his furthest
excursion is still there at touchdown.

| SUSTAIN p50 | | | | |
|---|---|---|---|---|
| **by buck** (the ruling predicts a RISE) | <20 | 20-60 | 60-200 | 200+ |
| tape 113 | 0.361 | 0.831 | 0.670 | 0.770 |
| tape 100 | 0.913 | 0.876 | 0.687 | 0.968 |
| **by time in the air** | <0.15 s | 0.15-0.40 | 0.40-1.0 | 1.0 s+ |
| tape 113 | 0.963 | 0.670 | **0.269** | **0.322** |
| tape 100 | 0.946 | 0.870 | **0.234** | **0.223** |

**FLAT against buck strength — even at `buck` 262 — and a MONOTONE COLLAPSE against
air time, in both tapes independently.** Sustain is governed by *how long he is
airborne*, not by how hard he was bucked. Past ~0.4 s the pull-back always wins and
superman cannot reach the landing. That is exactly his complaint, and no
displacement percentile can see it because they pool the whole tape.

### WHAT THE NEXT RUNG MUST DO — and what is NOT ruled

The pull-back needs to **know how hard he was thrown**. Today the spring returns him
at a fixed 1 Hz regardless, so the buck's magnitude has no memory. His ruling wants a
hard buck to keep him out until he lands.

⚠⚠ **NO THRESHOLD IS TO BE WELDED.** "not every time / if the bump is hard enough" is
a MAGNITUDE DEPENDENCE. This ladder's own recorded disease, three times over, is a
**binary read of a continuous quantity**, and the four terms already produce superman
*emergently* (0.00% occupancy on cruise tapes, 2–11.8% on bush) with no threshold
imposed. Build the relation, not a cutoff.

**STILL UNRULED — ask before building:**
- The **shape** of the buck→sustain relation. Mine to measure, his to pick.
- **The landing release.** "the hardness of the landing" causing grip loss, and
  "falling off to the side" — LADDER §7.7 puts `grip_load_n` / `grip_capacity_n` /
  `rider_attached` in the KERNEL and taped, and §7.7's shipping rule is explicit:
  ship with capacity so it NEVER breaks, prove goldens bit-identical, and only then
  dial it as a separate re-taped change.
- Whether the sustain lives in the spring's stiffness, its target, or a separate
  arming memory. Three different mechanisms, one felt outcome.

---

## §5c THE ARMING MEMORY — BUILT, GATED, SHIPPED OFF, AND CALIBRATED

`78e848d3c`. Gate **1674/1679** — the five known reds, plus five new legs green.
Mechanism, gate legs and the reasoning live in `render/body_drive.h`'s banner.

### THE CALIBRATION — pooled over the 44 DISTINCT drives, 84 airborne windows

`grip_load = landing hardness x extension at touchdown`, decay fixed at 1.0/s:

| buck_gain | p50 (an ORDINARY jump) | p90 = the capacity | separation |
|---|---|---|---|
| 0 (today) | 2.23 | 23.2 | 10.3x |
| 0.005 | 2.73 | 36.6 | 13.4x |
| 0.01 | 3.13 | 60.8 | 19.4x |
| **0.02** | **3.73** | **91.2** | **24.4x** |
| 0.03 | 5.52 | 134.1 | 24.3x |
| 0.05 | 5.72 | 156.5 | 27.3x |

★★★ **THE KNEE IS AT 0.02, AND IT IS MEASURED, NOT PICKED.** Separation *plateaus*
there (24.4 -> 24.3) while the cost in ordinary jumps jumps 48% (3.73 -> 5.52). Past
0.02 you pay in every ordinary landing and buy no extra rarity. That is the honest
bound; **`0.01`-`0.02` is the band, and Chad's drive picks inside it.**

★ **WHY THE SHAPE IS RIGHT for "at the further end, a little bit rare":** the median
barely moves across the whole sweep while the tail explodes (p99 109 -> 938). Ordinary
jumps stay ordinary; only the hard ones get dramatic. That IS his spec.

★ **THE CAPACITY IS NOT A CHOSEN NUMBER.** It is the p90 of his own measured
distribution, so "10% of jumps" holds by construction. At the knee it is **91.2**.

### ✅ CALIBRATION FINISHED — decay IS swept, and it changes the answer

`decay_per_s` is **not free and not a taste dial**: it is bounded BELOW by how fast he
must re-seat himself after touching down. Measuring that bound picks it.

| gain | decay | ordinary | capacity | SEP | recovery p90 / max |
|---|---|---|---|---|---|
| 0.01 | 1.0 | 3.13 | 60.8 | 19.4x | 2.07 s / 9.96 s |
| 0.02 | 1.0 | 3.73 | 91.2 | **24.4x** | 6.05 s / 20.08 s ← **UNUSABLE** |
| **0.02** | **3.0** | **3.20** | **57.4** | **17.9x** | **1.03 s / 6.59 s ← SHIP** |
| 0.02 | 6.0 | 3.20 | 44.6 | 13.9x | |
| 0.04 | 3.0 | 4.03 | 90.6 | 22.4x | |
| 0.08 | 6.0 | 4.73 | 103.4 | 21.8x | |

★★★ **THE BEST-SEPARATION ARM IS THE WRONG ONE.** 0.02/1.0 wins on separation and
leaves him soft for **20 s** after the worst landing. A rider limp for twenty seconds
is not supermanning. The constraint, not the score, picks the dial.

★★★ **SHIP `buck_gain 0.02`, `decay_per_s 3.0`, capacity `57.4`.** Separation 17.9x
against today's 10.3x; ordinary-jump cost barely moves (2.23 -> 3.20); he is back to
normal about a second after landing. The 6.6 s worst case is the single most violent
hit in 44 drives, where being rattled is arguably right.

⚠⚠ **AND THE UPPER BOUND WAS SILENTLY UNMEASURED UNTIL THE LAST HOUR.** The CSV format
string carried six `%.4f` for seven values, so `mem_land` was passed and never
printed. The empty column read as `0.00 s` for every arm and I nearly reported it as
evidence the concern did not exist. ★ **A column that is not printed reads as a zero,
and a zero looks like a measurement.** Count conversions against arguments; varargs
will not.

⚠ **I GOT THIS WRONG ONCE, MID-SESSION, OFF PARTIAL DATA.** I told Chad higher gain
*reduces* separation. It does the opposite, monotonically. The partial read compared
a complete arm against an incomplete one. **Never quote a sweep before every arm has
the same n.**

### STILL UNBUILT — the landing release

Grip actually letting go, and "falling off to the side". LADDER 7.7 puts
`grip_load_n` / `grip_capacity_n` / `rider_attached` in the **KERNEL and taped**, so
it ships with capacity set to never break, goldens proven bit-identical, and is
dialled as a separate re-taped change. The equation and its calibration are ready.

---

## §5b SUPERSEDED — what this session believed before the ruling

Kept because it is still TRUE physics and still bounds the design, but it answers a
question Chad did not ask. **Do not build from it.**


**The lever is the pose spring at full release, and it is a FEEL DIAL.**

Under the NO GUESSING law a tape sweep is a legitimate *instrument* — it measures
what a candidate produces on his real kinematics — but it **cannot select the
value**. Picking the number whose output "looks like superman" is tweaking a
constant until a desired picture appears, which is the named forbidden move. The
protocol the law permits:

1. The instrument is now honest (this rung).
2. Measure the physically-derivable half properly — per-limb `ρ·Cd·A/2m` off the
   shipped GLB, rather than one whole-body lump. That is a measurement of the
   artefact and is legal. **It will not reach 0.153; nothing physical will.**
3. Present the measured ladder for the one genuine feel dial behind the existing
   `SEADS_BODY_POSE_HZ` A/B, and **Chad's drive picks the rung.**

⚠ Do not simply raise `drag_k` to the scarf's 0.153. It is a ribbon's number; the
0.005 is the physically honest `ρ·Cd·A/(2m)` for an 87.5 kg body and is one of the
few numbers here that is *not* a guess.

### The thing that most needs his ruling first

**LADDER §7.3 specifies Superman as a BUCKING behaviour** — stage 3 follows stages 1
and 2, and his own R4a acceptance is *"big bumps buck him up, not off."* That is an
impact regime, where the four terms fire. It is **not** sustained free fall, and the
probe's original author wrote *"a rider in free fall does not superman, he floats."*

So there is a real question underneath all of this that no measurement settles:

> **Did Chad ever mean free-fall superman at all, or does he mean the buck?**

Everything in §3 measures free fall. If he means the buck, the instrument should be
pointed at the impact window instead, and the spring may not be the defect at all.
**Ask him before building against either reading.**

---

## §6 REMAINING KNOWN LIMITS OF ARM C5 (red-team P1-2, unfixed by ruling)

C5 is a *coarse chain carrying the shipped spring*, not the shipped chain:

1. Pose target is the straight rest layout, 5 × 0.35 m, vs the shipped 16-link
   measured seated pin recomputed each frame.
2. Anchor is the fixed GLB grip vs the shipped `ci.anchor = in.pin[0]`.
3. The shipped released chain runs `body_chain_seat_allowance`; `constrain4` is
   distance-only. Mostly inert when streaming *away* from the seat, but it is a
   mechanism C5 does not carry.
4. `rig.pose_hz` is hardcoded 1.0 rather than `body_pose_hz()` (unlinkable TU), so
   a `SEADS_BODY_POSE_HZ` override silently forks probe from game.

The spring-pricing delta and the "can it stream at all at 1 Hz" bound survive all
four. The absolute station geometry does not.

---

## §7 THE OTHER RUNG (cheap, closed)

`9a99f9350` — snow's R5 row-9 shader test was green **only in the tree that wrote
`planet.cpp`**. `core.autocrlf` converts on **checkout, not on write**, so their
authored copy is LF and my merged copy is CRLF; `read_file` opens `std::ios::binary`
and two patterns span a line break, so `\n` cannot match `\r\n`. Fixed at the read.

Fourth instance of a family `.gitattributes` documents three times ("green where the
file was freshly baked, red on every fresh checkout").

⚠ **MSYS `grep`, `cat -A` and `sed` text-mode strip CRs** and will tell you a CRLF
file is clean. Only a byte-level read proves it.

★ The test's **name still outruns its reach** — it claims a dataflow early-out and
asserts source *formatting*, so a genuinely unguarded write to `shadowOcc` passes it
green while a harmless reformat fails it. Recorded, not fixed: it is snow's leg.

---

## §8 LESSONS THIS RUNG PAID FOR

- ★★★ **An instrument written before a mechanism keeps measuring the world without
  it.** The probe was faithful the day it was written and silently stopped being so
  when the pose spring landed. Nothing goes red when this happens. Before trusting
  any rig, diff its model against the shipped one *term by term*.
- ★★★ **A filter that protects a statistic can bury the finding.** `aeff > 2.0` was
  defensible and correct for its own metric, and it discarded exactly the regime
  under investigation. When you exclude data, say what question the exclusion makes
  unanswerable.
- ★★ **A metric needs a control that can convict it.** The isotropy rows exist only
  because the red-team attacked the "adrift projects to zero" claim — and they then
  convicted my own headline within one run.
- ★★ **A closed form agreeing with a measurement is not confirmation when both use
  the same constants** — and check the value you assumed for the free variable
  before calling it a match.
- ★ **Count your corpus.** 116 files were 44 drives.

---

## §9 THE NEXT SESSION — START HERE

### 1. IS THERE A VERDICT ON THE DRIVE? That gates everything.

Chad has the exe and the dial (`SEADS_BUCK_GAIN=0.02 SEADS_BUCK_DECAY=3.0`).
**Ask for the verdict before building anything.** What his words most likely mean:

| He says | It means | Do |
|---|---|---|
| "floaty on small bumps" | gain too high | drop to 0.01 (§5c table) |
| "still snaps back too fast" | gain too low, or decay too high | gain 0.04 at decay 3.0 |
| "stays loose after landing" | decay too low | 6.0 — costs separation 17.9x → 13.9x |
| "good" | ship it | fold 0.02/3.0 in as the default, re-gate, and the **landing release** is next |

⚠ **ONE DIAL PER DRIVE.** Both at once and neither is attributable.

### 2. THE LANDING RELEASE — the only unbuilt half of his ruling

"that second, unseated hit from the landing would then cause his grip to let go
... and falling off to the side." The equation and its number are READY:
`grip_load = landing hardness x extension at touchdown`, **capacity 57.4** at the
recommended dial (the p90 of his own 44-drive distribution, so his 10% holds by
construction).

⚠⚠ **IT IS KERNEL WORK AND IT IS TAPED.** LADDER §7.7 puts `grip_load_n`,
`grip_capacity_n` and `rider_attached` in `SledState`. The shipping rule is
explicit and is not optional: **ship with capacity set so it NEVER breaks, prove
the full existing golden set BIT-IDENTICAL, and only then dial it down as a
separate, re-taped change.** That is how `assist_hull_frac`, `roll_stiff_vgain`
and `release_floor_frac` all went in.

★ "Falling off to the SIDE" is **lateral** and nothing in the chain does that yet.
Do not assume it falls out of the existing terms — measure it first.

### 3. WHAT IS BUILT AND WHERE

| Thing | Where | State |
|---|---|---|
| Arming memory (policy + state) | `render/body_drive.{h,cpp}` | built, 5 gate legs, **ships OFF** |
| Wiring + `SledModel::body_buck` | `render/sled_model.cpp` | built, env-dialled |
| Sustain / grip instrument | `tools/sled_probe.cpp` `superman` | built, `SEADS_PROBE_CSV=1` dumps windows |
| CRLF test fix | `test/unit/test_snow_shadows.cpp` | closed |

### 4. TRAPS THIS SESSION PAID FOR — read before repeating them

- ★★★ **A column that is not printed reads as a ZERO, and a zero looks like a
  measurement.** Six `%.4f` for seven values silently dropped `mem_land`; the empty
  column read as "0.00 s recovery" for every arm and nearly shipped the WRONG dial
  (0.02/1.0 leaves him limp for 20 s). Count conversions against arguments.
- ★★★ **Never quote a sweep before every arm has the same `n`.** I called the
  separation trend backwards off a complete arm vs an incomplete one.
- ★★★ **An instrument written before a mechanism keeps measuring the world without
  it, and stays green.** The probe had no pose spring for the entire life of the
  spring. Diff a rig's model against the shipped one term by term.
- ★★★ **A filter that protects a statistic buries the finding.** `aeff > 2.0` was
  right for its own metric and discarded the whole regime under investigation.
- ★★ **A metric needs a control that can convict it.** The isotropy rows killed my
  own headline within one run of adding them.
- ★★ **`core.autocrlf` converts on CHECKOUT, not on write** — a source-text test is
  green only in the tree that WROTE the file. MSYS `grep`/`cat -A`/`sed` LIE about
  CRLF; byte-read or it is not evidence.
- ★ **Count your corpus.** 116 tape files are **44 distinct drives** (one content
  appears 23 times). Every corpus claim uses 44.
- ★ **A locked exe silently fails the relink** (`ld returned 1`) — stop background
  runs before rebuilding, and `rm` the exe to prove freshness.

### 5. HOUSEKEEPING

- **Nothing is pushed.** 14 commits ahead of `origin/main`, 0 behind.
- The **enemy-AI session** landed their envelope to main and has been told
  `seads-recon` is released. They asked to take this worktree; Chad ruled their
  work is theirs to land — **do not push or merge their branch for them.**
- `build/` = Debug, the gate. `build-play/` = RelWithDebInfo, what Chad drives.
  Never run two ctest gates against one build dir. In `build-play` build the
  **`seads` target only** — the test TUs `#error` outside Debug by design.
