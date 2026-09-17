# RED-TEAM — S-tremor / `[auto_level] hand_net_window` — LENS: LAW AND FEEL

Fresh context, 2026-09-16. Lane `feel/tremor-netwindow`, tip **`9ba9beb35`**
("S-tremor (kernel v17 candidate): a windowed NET hand-live measure"), worktree
`D:\seads_sandboxes\tremor`. Judged against the **gun-director law** (Chad
2026-09-12), the **2026-08-06 resting ruling**, and the kernel law (one dial,
0 = bit-identical, Chad flies each).

**VERDICT: LAND-WITH-FIX.** The mechanism is the right shape and the OFF arm is
clean, but the shipped artefact **fails Chad's own fly card** before he ever
opens it: the card says *"a slow 5 deg/s lateral drift while belly-up must NOT
right him"*, and the built exe rights him **14.4 deg** during exactly that
input, because the veto is 0.100 s late. At 1.5 deg/s and below the veto never
arrives at all and he gets the **entire** hands-off righting while his hand is
sweeping. P0-1 must be folded and re-measured before this goes in front of him;
P1-1 and P1-2 must be folded before it is called law-compliant.

---

## §1 METHOD

Everything below is measured, not argued.

1. Read the launch packet (`docs/SESSION_HANDOFF_20260915_tremor_debt.md`) and
   the v16 record §3/§4/§6, then the whole kernel diff
   (`git show HEAD -- control/ config/`) and the seams it touches:
   `control/controller.cpp` (the clock at ~line 250 and its single consumer,
   the `hand_gate` at ~2358), `app/instructor_tick.h` (the `aim_moved` /
   `aim_rate_world` CQ2 seam at 1114/1164 and the ZOH smear in `step_frame`),
   `app/sting.h` (the carried aim frame, 313-356), the deadzone `rest_dwell`
   latch (controller.cpp ~415), `app/feel_tape_fields.h`,
   `test/harness/feel_tape.h`.
2. **Re-ran the builder's gate verdict myself** —
   `py -3 tools/gate/gate_baseline.py check build/gate_tremor.log` →
   *"OK -- the red set is EXACTLY the baseline, member for member"*, 6 failed of
   2112. **Re-ran the lane battery myself** —
   `ctest --test-dir build -R "righthand|tremor|loop_rollover|controller_load|params|load_controller|yawbudget|TAPE"`
   → **48/48 passed, 15.21 s**. Both of the builder's claims hold.
3. **Built my own instrument** — six probe legs appended to
   `test/unit/test_loop_rollover.cpp`, reusing its rig (`app::step_frame`, the
   belly-up theta 170 fixture at V 230 / 6000 m, the shipped
   `config/controller.toml`, the `tr_on()` / `tr_off()` arms, the per-tick
   telemetry hook). Every run is 60 fps (`frame_dt = 2*sim_dt`), 2 s, and
   reports `integral |roll_right| dt` in degrees split at a mark time. Probes
   **reverted**; the source is kept, gitignored, at
   `D:\seads_sandboxes\tremor\build\rt_probe.inc` for the folder to reuse.
4. **Tested one candidate fix in the tree** (`control/controller.cpp`,
   one line), measured it, and **reverted it** (`git checkout --`). The
   committed diff of this red team is this file and nothing else.

> Worktree note for the runner: a SECOND agent was editing
> `test/unit/test_loop_rollover.cpp` in this same worktree while I worked (its
> block `PROBE: the veto latency on a sweep that STARTS from a rested hand`
> broke the build mid-session with an unterminated string literal). I removed
> only my own block and left theirs in the working tree untouched. **Two
> red-team lenses must not share one worktree.** Independently, that agent went
> for the same scenario I did, which is itself a signal.

---

## §2 WHAT IS CLEAN (checked, not assumed)

- **The 08-06 ruling is untouched.** In every probe run the hands-off half is
  bit-for-bit identical between the arms: `integral |roll_right| = 48.75 deg`
  over the first 0.40 s at the dial ON and at the dial OFF, in all sixteen RT1
  runs. The window adds **zero** delay to the still-hand path, exactly as the
  commit message claims. The outer v16 `hand_live` gate is what buys this and
  keeping it was the right call.
- **The OFF arm is structurally off.** All new arithmetic is inside
  `if (cp.hand_net_window > 0.0)`; the else branch is the v16 expression. The
  three hash pins pass (I ran them). Walk-back really is one line.
- **No new boolean.** The liveness is a smoothstep; nothing here can chatter.
- **The deadzone `rest_dwell` latch is untouched** — it reads `in.aim_moved`
  directly (controller.cpp ~418) and never sees `hand_rest` or the liveness.
- **Freelook and GROUNDED are safe**: both `aim_moved` and `aim_rate_world`
  carry the same CQ2 gate (instructor_tick.h 1114/1167), so both read zero,
  `hand_live` is false, and the OUTER gate runs the v16 path. `aim_net` keeps
  leaking toward zero through a freelook, which is the correct reading.
- **The MB-lean limb is not on this seam**: `hand_gate` multiplies only the
  MB-right righting emission.
- **The intermittent tremor is cured too** (RT5): a count every k-th frame,
  k = 1/2/3/5/10, all give 109.7-110.2 deg of righting at the dial ON against
  0.00/1.44/5.56/21.02/86.58 at v16. The cure is not brittle to gaps.
- **A single deliberate nudge costs almost nothing** (RT2): from a full gate, a
  1/2/3/5/10/20-count nudge leaves 61.50/61.15/59.83/58.50/59.25/59.25 deg
  against v16's 59.25 deg flat. Worst case +2.25 deg. Not a finding.

---

## §3 P0-1 — THE VETO IS LATE, AND BELOW ~2 deg/s IT NEVER ARRIVES

**The case the lane never ran.** Every existing leg starts its deliberate sweep
at t = 0, when `hand_rest` is already 0 and the gate is therefore 0 **by
construction**. That grades one half of the law — *a sweep must not let the
gate RISE*. The other half is the half a pilot lives in: he is belly-up with his
hand off, the gate has climbed to 1.0 and the aeroplane is rolling at the full
`inverted_rate` 180 deg/s, and **then** he puts his hand on. The law says that
input vetoes. Nothing in the lane measures it.

**RT1** — belly-up theta 170, hand off for 0.40 s (gate reaches 1.000), then a
sustained lateral sweep at a fixed angular rate, 60 fps, 2 s total:

| sweep | arm | gate on the first tick after the hand goes on | time to gate < 0.05 | integral \|roll_right\| AFTER the hand goes on |
|---|---|---|---|---|
| 0.5 deg/s | OFF (v16) | 1.000 | **0.008 s** | **1.50 deg** |
| 0.5 deg/s | **ON (shipped)** | 1.000 | **never** | **61.50 deg** |
| 1.0 deg/s | ON | 1.000 | never | 61.50 deg |
| 1.5 deg/s | ON | 1.000 | never | 61.45 deg |
| 2.0 deg/s | ON | 1.000 | 0.342 s | 48.22 deg |
| 3.0 deg/s | ON | 1.000 | 0.175 s | 25.94 deg |
| **5.0 deg/s** | **ON** | 1.000 | **0.100 s** | **14.42 deg** |
| 10 deg/s | ON | 1.000 | 0.050 s | 7.31 deg |
| 40 deg/s | ON | 1.000 | 0.017 s | 2.41 deg |

The OFF arm is `0.008 s / 1.50 deg` at **every** rate — v16 vetoes on the next
tick, whatever the size, which is the gun-director law as written. 61.50 deg is
the whole remaining hands-off righting: at 1.5 deg/s and below the pilot's
sustained, deliberate input is treated as **no hand at all**.

**Chad's fly card, as written in the launch packet §6, is the 5 deg/s row.** He
will rest his hand (that is what makes the gate climb), then start the drift.
The build rights him 14.4 deg through it.

**Root cause, two parts.**

1. *Intrinsic.* Any windowed-net measure must wait for the aim to travel
   `floor_rate * window` = 1 deg/s * 0.2 s = **0.2 deg** before it calls the
   hand live at all, and `3 deg/s * 0.2 s` = **0.6 deg** before the veto is
   complete. At 5 deg/s those are 40 ms and 120 ms.
2. *Not intrinsic — a defect in the normaliser.* Builder departure #1 claims
   `|aim_net| / aim_net_w` "equals a constant sweep rate EXACTLY on every tick
   including the first, so the veto is never late". That is true **only when
   both accumulators start from zero together**, i.e. at the first tick of the
   sim. `aim_net_w` is incremented by `dt` on **every** tick regardless of the
   hand (controller.cpp: the line sits outside `if (hand_live)`), so it
   saturates at `window` and stays there, while `aim_net` decays toward zero
   during the rest. After any pause — and a pause is the *only* way the gate
   can be nonzero — the normaliser divides by a full window that the signal has
   not had, and the measure reads `r * (1 - exp(-t/window))`: exactly the
   "reads low for its whole rise time" failure the builder says he fixed,
   displaced from t = 0 to every restart.

**The fix I measured** (one line, `control/controller.cpp`): gate the
normaliser's increment on the same bit that gates the measure —

    ns.aim_net_w = internal.aim_net_w + (hand_live ? dt : 0.0) -
                   leak * internal.aim_net_w;

so `aim_net_w` counts *live* time, both accumulators decay together through a
rest, and on the first live tick after any rest both are one tick old and the
ratio is the true instantaneous rate. Measured on the same RT1 fixture:

| sweep | shipped | with the one-line fix | v16 |
|---|---|---|---|
| 40 / 10 / 5 / 3 deg/s | 2.41 / 7.31 / 14.42 / 25.94 deg | **1.50 deg, veto in 0.008 s — identical to v16** | 1.50 deg |
| 2.0 deg/s | 48.22 deg | 3.59 deg (veto 0.033 s) | 1.50 deg |
| <= 1.5 deg/s | 61.5 deg, never | 34.2-61.5 deg, never | 1.50 deg |

The lane leg `S-tremor: a small slow deliberate sweep still vetoes` stays green
under the fix (re-run by name: 9 assertions, all passed). **Its cost, measured
and not hidden:** the fix makes the cure need one window of continuous motion
before it engages, so an *intermittent* tremor loses some of the cure —
RT5 at the dial ON goes 109.72/108.49/97.45/43.27/86.01 deg for k = 1/2/3/5/10
against the shipped 109.72/109.95/110.02/109.91/110.25. (Still better than v16
at k <= 5.) That is a real trade and the builder should either accept it, or
fold the same idea a different way (e.g. keep the shipped normaliser but hold
`hand_live_frac` at 1 until the hand has been continuously live for one window
— the discount must be *earned*, never granted to a hand that just arrived).

**Required with the fold, whichever shape:** a new gate leg that starts from a
gate of 1.0 and then puts the hand on — the RT1 rig, pinned at the 5 deg/s row.
Without it this failure can come back invisibly.

---

## §4 P1-1 — THE GUN-DIRECTOR LEG CANNOT RED ON WHAT IT NAMES

`test/unit/test_loop_rollover.cpp`, TEST_CASE *"S-tremor: a small slow
deliberate sweep still vetoes"* (ctest #2001) asserts `gate_max < 0.05` and
`hand_rest_max < 0.02` on a run where the hand sweeps from t = 0 with
`hand_rest` seeded at 0. The gate cannot rise in that run for reasons that have
nothing to do with the measure: the same assertions pass at the dial OFF, at
the dial ON, and (per §3) in a build whose veto is 340 ms late. It is a
non-regression leg dressed as the gun-director leg, and the file's own header
discipline (every leg carries a MUTATION note saying what reds it) is not met —
the stated mutation ("drop the saturation to ~6 deg/s") reds it, but the failure
the leg is *named* for does not.

**Fix:** keep it, rename it to what it grades ("a sweep never lets the clock
start"), and add the RT1 leg beside it as the gun-director leg proper.

---

## §5 P1-2 — THE LAW CLAIM IS OVERSTATED: A DELIBERATE WIGGLE IS DISCOUNTED

The commit message, `params.h` and `controller.toml` all state the gun-director
law is *preserved* / *intact*. It is not preserved in general; it is preserved
for **sustained one-way** motion above ~2 deg/s (and, after P0-1 is folded,
above ~3 deg/s from any starting condition). Motion that reverses inside the
window is discounted whether or not it is deliberate, because the measure is
blind to the difference by design.

**RT6** — a deliberate tracking oscillation (working the reticle back and forth
over a jinking bandit), amplitude in degrees of aim, belly-up, 2 s, 60 fps.
`integral |roll_right|`:

| amplitude | 1 Hz | 2 Hz | 4 Hz | v16 (all) |
|---|---|---|---|---|
| +/-0.25 deg | **109.57** | 96.55 | 106.44 | 0.00-0.04 |
| +/-0.50 deg | **98.35** | 34.40 | 11.54 | 0.00-0.04 |
| +/-1.0 deg | 23.97 | 5.44 | 1.66 | 0.00-0.04 |
| +/-2.0 deg | 4.28 | 0.94 | 0.24 | 0.00-0.04 |

A +/-0.5 deg reticle sweep at 1 Hz is roughly seven mouse counts each way at the
shipped sensitivity — a visible, deliberate hand working a target — and it is
rewarded with 98 deg of instructor roll and a gate of 0.999. The honest
statement of the mechanism is: **any hand motion whose NET travel over the last
0.2 s is under ~0.2 deg is treated as no hand, and under ~0.6 deg as a
fractional hand — deliberate or not.** That is not a bug to be patched out (no
net-window measure can separate those two); it is a **ruling** Chad has to make,
and the packet's own §2 wording ("a real hand movement of any size must still
veto") says he has not made it yet.

**Fix:** state the exception in `params.h`, `controller.toml` and the handoff in
those terms, print the RT6 table as a measured-residual leg the way the
random-walk leg already does, and put the single question to him with the fly
card: *"belly-up, hand working the reticle in a half-degree wobble — do you want
to be righted or not?"*

---

## §6 P2-1 — THE CURE'S MARGIN IS A FUNCTION OF `aim_sensitivity`

The two rate walls are absolute (1 and 3 deg/s) but the quantity they judge is
`counts * aim_sensitivity / window`, so the cure's headroom is set by a dial in
a different block. **RT4**, the tremor leg re-run with `aim_sensitivity` swept
(shipped 0.14 deg/count):

| aim_sensitivity | 0.07 | 0.14 (shipped) | 0.20 | 0.28 | 0.40 | 0.56 |
|---|---|---|---|---|---|---|
| integral \|roll_right\|, tremor, dial ON | 109.82 | 109.72 | 109.56 | 107.72 | 85.09 | **4.84** |
| settled net rate [deg/s] | 0.29 | 0.58 | 0.83 | 1.17 | 1.67 | 2.34 |

At ~2.9x the shipped sensitivity the cure is degrading; at 4x **the debt is
back** and nothing says so. `controller.toml` line ~207 explicitly invites a
hardware call here ("hardware DPI up Nx + aim_sensitivity down Nx") — that
particular *pair* is neutral for this measure (counts up, degrees-per-count
down, net unchanged), which is worth saying out loud, but a bare sensitivity or
DPI raise is not.

**Fix (cheapest):** say it in `params.h` and add RT4 as a printed sweep leg, so
the validity band is on the record. **Fix (better, still one dial):** express
the floor in counts of net travel per window rather than deg/s —
`floor_rate = 1.5 * cp.aim_sensitivity / cp.hand_net_window` is 1.05 deg/s at
the shipped values, i.e. essentially the same wall today, but it means "under
about one and a half counts of NET travel per window is not a hand", which is
what the debt actually is and is invariant to the sensitivity.

---

## §7 P2-2 — `SEADS_TAPE_OPT_FIELDS` IS AN UNENFORCED ESCAPE HATCH

The exception is well argued and, for these two fields, correct: they are
short-memory state the recorded `aim_dx`/`aim_dy` reconstruct, and re-recording
Chad's 2026-09-13 dives is impossible. Two problems.

1. **Nothing enforces the rule.** The ban on long-memory and latched state is a
   comment. The next agent who hits a stale-tape red has a named, precedented,
   one-line way to make it go away. Fix: pin the opt list — a test that asserts
   the exact opt column names (`i_aim_netx/y/z`, `i_aim_net_w`) and nothing
   else, so adding a fifth field reds the gate and forces the argument.
2. **The safety argument names the dangerous interval.** "The only error is that
   a replay's first ~0.2 s scores the hand as stiller than it was" is precisely
   the fill window in which §3 shows the righting leaks. It is harmless for the
   six dive fixtures (they are not belly-up at a full gate), but the sentence
   should not be left standing as "harmless in general" — say *why* it is
   harmless for those six tapes.
3. **Doc defect (P3):** the new `static_assert(sizeof(control::Internal) == 280)`
   comment in `app/feel_tape_fields.h` says `aim_net`/`aim_net_w` "are carried in
   `SEADS_TAPE_INT_FIELDS` above". They are in `SEADS_TAPE_OPT_FIELDS`.

---

## §8 P3 — SMALLER THINGS, FOR THE RECORD

- **`0/0` is unreachable but ungarded.** `hand_net_rate = |aim_net| /
  ns.aim_net_w` is safe because `aim_net_w >= dt`, and `sim_dt > 0` is
  loader-enforced (`config/load_aircraft.cpp:92`), so the app cannot reach it.
  A direct caller with `dt == 0` gets NaN into `hand_live_frac`, `hand_rest`,
  `hand_gate` and the roll demand. If the fold touches that line anyway,
  `std::max(ns.aim_net_w, 1e-12)` costs nothing.
- **One dial, but two walls.** The kernel law is met in the letter (the walls
  are constants with a why-comment, not dials) and the walk-back is genuinely
  one line. But the *felt* boundary is the product `floor_rate * window`, set
  jointly by a constant and the dial. The handoff should say the felt quantity
  is 0.2 deg of net travel, not 0.2 s.
- **The Sting carries the accumulator** (`app/sting.h:355-356`): its cascade
  gets `aim_moved`/`aim_rate_world` only on the consuming call, with no ZOH
  smear, so the drone's window fills on a different cadence than the
  aeroplane's. Pre-existing v16-class seam, no regression from this lane, and
  the Sting is never belly-up-righted in practice. Noted so the next lane does
  not rediscover it.
- **Chad's confirmation flight set is untouched.** The loop (hand held, high
  net rate), the Immelmann and the split-S (hand stops, outer gate, unchanged)
  all sit far from the discounted band; the rolling-loop roll-demand hash pin
  holds at both dial values. What he *will* feel is only the belly-up cases in
  §3 and §5 — which is why the fly card has to name them explicitly.

---

## §9 RANKED FINDINGS

| id | sev | finding | fold |
|---|---|---|---|
| **P0-1** | P0 | The veto is 0.10 s late at 5 deg/s (14.4 deg of righting into a deliberate input) and never arrives at or below 1.5 deg/s, when the input starts from a rested hand — the only state in which the gate is nonzero. Fails Chad's own fly card. | gate the `aim_net_w` increment on `hand_live` (measured: restores v16 exactly at >= 3 deg/s), or hold liveness at 1 until the hand has been live for one window; **add the RT1 leg**; re-measure and re-gate. |
| **P1-1** | P1 | `S-tremor: a small slow deliberate sweep still vetoes` grades a gate that is 0 by construction; it cannot red on the failure it is named for. | rename to what it grades, add the RT1 leg as the gun-director leg. |
| **P1-2** | P1 | "Gun-director law preserved" is overstated: a deliberate +/-0.5 deg 1 Hz tracking wobble gets 98.35 deg of righting (v16: 0.00). | restate as a bounded, named exception (net travel under ~0.2 deg in the window is not a hand); print the RT6 table as a residual leg; put the question on the fly card. |
| **P2-1** | P2 | The cure dies at ~3x `aim_sensitivity` (4.84 deg at 0.56 deg/count) with no guard, comment or leg. | document the band + add the RT4 sweep leg, or express the floor as counts-of-net-travel per window. |
| **P2-2** | P2 | `SEADS_TAPE_OPT_FIELDS` is enforced only by a comment; its safety argument names the interval P0-1 shows is dangerous. | pin the opt column list in a test; narrow the wording to the six fixtures. |
| **P3-1** | P3 | `feel_tape_fields.h` size-assert comment misfiles the new fields under `SEADS_TAPE_INT_FIELDS`. | one-line comment fix. |
| **P3-2** | P3 | `0/0` NaN on a `dt == 0` direct call (unreachable in the app). | `std::max(ns.aim_net_w, 1e-12)` if that line is touched. |
| **P3-3** | P3 | The Sting's cascade feeds the new accumulator without the ZOH smear. | note in the handoff; pre-existing. |

---

## §10 WHAT I CHANGED

Nothing but this file. Probes appended to `test/unit/test_loop_rollover.cpp`
and the one-line experiment in `control/controller.cpp` were **reverted**
(`git checkout -- control/controller.cpp`; the probe block removed by line
range). Probe source preserved, gitignored, at `build/rt_probe.inc`. A second
agent's probe block was in the same file in the working tree when I finished; I
did not touch it and it is not part of this commit.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

---

# FOLD ADDENDUM — 2026-09-16, by the folding session (lane `feel/tremor-netwindow`)

Your P0-1 and both P1s are folded. Your one-line fix is the one that shipped,
your RT1 rig is now a committed leg, and your RT5 / RT6 tables are committed
legs too. Numbers below are printed by those legs on the folded tree.

## P0-1 — FOLDED, your shape

`control/controller.cpp`:

    ns.aim_net_w = internal.aim_net_w + (hand_live ? dt : 0.0) -
                   leak * internal.aim_net_w;

chosen over the mechanism lens's "clear both accumulators when the hand is not
live" for one measured reason: with the gated increment both accumulators leak
together through a rest, so their RATIO survives a pause — a hand that stops
mid-sweep is still read at its own rate when it resumes, and an intermittent
tremor keeps the memory of its own netting-to-nothing. Clearing throws that away
and pays for it on exactly your RT5 arm. In the long-rest limit the two are the
same, which is the limit the finding is about. The scar is written at the line,
in `controller.h`, in `params.h` and in the toml block, and the four places that
claimed "the veto is never late" now say what is true.

NEW LEG — `S-tremor: a sweep from a FULL gate vetoes on the next tick`
(ctest #2002), your RT1 rig, committed with your mark at 0.40 s:

| sweep | shipped (pre-fold) | FOLDED | v16 |
|---|---|---|---|
| 3 deg/s | 25.94 deg / 0.175 s | **1.50 deg / 0.008 s** | 1.50 / 0.008 |
| 5 deg/s (Chad's fly-card row) | 14.42 / 0.100 | **1.50 / 0.008** | 1.50 / 0.008 |
| 10 deg/s | 7.31 / 0.050 | **1.50 / 0.008** | 1.50 / 0.008 |
| 40 deg/s | 2.41 / 0.017 | **1.50 / 0.008** | 1.50 / 0.008 |

Hands-off half 48.75 deg in both arms at every rate, asserted `==`. The cure is
unchanged: tremor ON 109.72 deg vs OFF 0.00, still hand 110.25.

MUTATION RUN (not argued): reverting to `+ dt`, rebuilding and running the leg
reds 7 of 28 assertions and reprints your table exactly — 25.94 / 14.42 / 7.31 /
2.41 deg. The failure cannot come back invisibly now.

YOUR MEASURED COST, PINNED — new leg `S-tremor: an intermittent tremor, the
fold's measured price` (#2005): ON 109.72 / 108.49 / 97.45 / 43.27 / 86.01 at
k = 1/2/3/5/10 against v16's 0.00 / 1.44 / 5.56 / 21.02 / 86.58 — your numbers
reproduce to the digit. The leg pins "never worse than v16" (±3 deg at k = 10,
where the two arms converge). Trade taken deliberately: the gun-director law is
a standing ruling, the cure is this lane's proposal, so the law wins the tie.
Your alternative shape (hold liveness at 1 until the hand has been continuously
live for one window) is recorded in the handoff as the other way to buy the same
law back if Chad wants the intermittent cure instead.

## P1-1 — FOLDED, exactly as you asked

The leg is RENAMED to what it grades — `S-tremor: a sweep never lets the clock
start` (#2001) — with your reasoning at the head of it (the gate is 0 by
construction; it passes at the dial off, at the dial on, and in a build whose
veto is 340 ms late), and #2002 above is the gun-director leg proper beside it.
#2002 carries an explicit NON-VACUITY `REQUIRE(gate_at_mark > 0.99)` so it can
never be green because the gate was 0.

## P1-2 — FOLDED as WORDS + a printed leg + a fly-card question

No patch, as you said. The claim "the gun-director law is preserved" is struck
from `params.h` and `controller.toml`; both now carry the bounded named
exception verbatim: *any hand motion whose NET travel over the last
hand_net_window is under ~0.2 deg is treated as no hand, and under ~0.6 deg as a
fractional hand — deliberate or not.* Your RT6 is committed as
`S-tremor: the tracking-wobble residual, measured not hidden` (#2004), printing
the table and pinning its shape (a ±0.25 deg 1 Hz wobble is discounted at 109.57
deg; a ±2 deg one is a hand again at 4.28 deg; v16 vetoes all twelve rows at
0.00-0.04). The question goes on the fly card as you wrote it: *belly-up, hand
working the reticle in a half-degree wobble — righted or not?*

Also folded: the slow-drift hole you and the mechanism lens both land on is
pinned as numbers by `S-tremor: the felt wall of the veto, measured` (#2003) —
61.50 / 61.50 / 34.24 / 3.59 / 1.50 / 1.50 deg at 0.5 / 1.0 / 1.5 / 2.0 / 3.0 /
5.0 deg/s — and `controller.cpp` now derives the FELT wall (~1.30 deg/s, from
the multiplicative reset) instead of quoting the 3 deg/s saturation.

## Your P2/P3

* **P3-1 (0/0)** folded: `std::max(ns.aim_net_w, 1e-12)`.
* **P3 doc defect** folded: `feel_tape_fields.h` says `SEADS_TAPE_OPT_FIELDS`.
* **P2-1 (aim_sensitivity band)** and **P2-2 (the OPT-fields door)** NOT folded —
  P2, outside this fold's P0/P1 mandate, untouched by the fold's arithmetic, and
  carried into the handoff as named debt. Your better fix (express the floor as
  counts of net travel per window) is a felt retune of a dial Chad has not flown
  yet; it goes to him as a ruling, not into a fold.
* **P3-3 (the Sting's cascade cadence)** noted, pre-existing, carried forward.

## Worktree note, taken

Two lenses shared this worktree and it cost a broken build. The folding session
ran alone in it; both probe blocks are gone from the tree and the only committed
test changes are the four legs above.

## Gate

Lane battery on the folded tree: **52/52 green**. `gate_baseline.py lint`: OK.
Full gate re-run on the folded tip, verdict quoted by name in the commit.
