# RED-TEAM — S-tremor / `hand_net_window` — LENS: MECHANISM AND MUTATION

Fresh context, 2026-09-16. Lane `feel/tremor-netwindow`, tip `9ba9beb35`
("S-tremor (kernel v17 candidate): a windowed NET hand-live measure").
Re-derived from the artefact (`git diff origin/main -- control/ config/ app/ test/`),
not from the commit message.

**VERDICT: LAND-WITH-FIX.** One P0 and one P1 must be folded and re-measured
before this tip is put in front of Chad. The P0 is not a code slip — the
mechanism does not do what `control/params.h`, `control/controller.h`, the
toml block and the commit message all say it does, and no leg in the suite can
see the difference. The off arm, the frame-rate invariance and the hash-pin
method are all sound and are confirmed below.

---

## §1 METHOD

1. Read the whole kernel diff and the app-side plumbing it depends on
   (`app/instructor_tick.h` `aim_rate` / `step_frame` pending-delta handling,
   `control/controller.cpp` `Internal ns = internal`, the single `hand_rest`
   consumer at `controller.cpp:2358`).
2. Built `seads_tests` on the tip and ran the S-tremor block to reproduce the
   builder's own published numbers.
3. Wrote an exact transcription of the four-line recurrence into a standalone
   model and **validated it against the shipped instrument**: the model
   predicts the tremor's settled net rate as 0.59 / 0.59 / 0.57 / 0.56 / 0.52
   deg/s at 240 / 120 / 60 / 30 / 10 fps against the leg's measured
   0.59 / 0.59 / 0.58 / 0.56 / 0.52. Agreement to the printed digit at five
   frame rates, so the model is a trustworthy instrument for states the
   fixtures do not reach.
4. Wrote a throwaway in-code probe in `test/unit/test_loop_rollover.cpp`
   (`PROBE: the veto latency on a sweep that STARTS from a rested hand`) that
   drives **the shipped `app::step_frame`** — real counts, real `apply_mouse`,
   real ZOH smear — for 0.6 s of a resting hand and THEN a sustained
   deliberate sweep, measuring only what happens after the sweep begins.
   Reverted; the working tree is clean and this record is the only commit.
5. Verified the hash-pin **method** by swapping `origin/main`'s (pre-change)
   `control/controller.cpp` into this tree, rebuilding, and running the pin
   leg. Restored immediately.
6. Mutation-read every new leg: deleted the mechanism mentally and asked which
   CHECK reds.
7. Re-ran the lane battery myself on the restored tip —
   `ctest -R "righthand|tremor|loop_rollover|controller_load|params|load_controller|yawbudget|TAPE"`
   → **48/48 passed, 17.9 s** (`build/lane_ctest.log`). The builder's lane
   claim reproduces; so does every printed number in the S-tremor block. The
   suite being green is not in dispute — what it measures is.

Numbers below name their source: `[probe]` = the in-code probe through
`app::step_frame`; `[model]` = the validated transcription; `[leg]` = the
committed instrument's own printed output on this tip.

---

## §2 WHAT THE MECHANISM ACTUALLY IS

```
leak      = clamp(dt / hand_net_window, 0, 1)            // dt ≡ ap.sim_dt = 1/120
aim_net  += aim_rate_world*dt - leak*aim_net             // a one-pole filter, not a boxcar
aim_net_w+= dt              - leak*aim_net_w             // the "normaliser"
net_rate  = |aim_net| / aim_net_w                        // only while hand_live
live      = smoothstep(1 deg/s, 3 deg/s, net_rate)
hand_rest = min(hand_rest+dt, right_hand_rest) * (1 - live)
```

Two structural facts the record does not state, and both matter:

**(a) `aim_net_w` is not a normaliser, it is a constant.** Its recurrence
`w ← w(1−dt/W) + dt` has the fixed point `W` and *does not depend on the hand
at all* — `dt` is added on every tick, live or not. Starting from 0 it reaches
99 % of `W` in 5·W = 1 s of sim time and stays there for the rest of the
session. So `|aim_net| / aim_net_w` is, after the first second of any flight,
identically `|aim_net| / hand_net_window` — the un-normalised form that
`control/controller.h` says "was wrong… the veto arrived ~0.1 s late… and
leaked 22 deg/s of righting into it (measured; the normalised form leaks
none)".

**(b) The reset is multiplicative, so `live` acts like a step, not a ramp.**
`hand_rest* = dt(1−live)/live` is the equilibrium, so `live = 0.0625` already
halves the gate. The smoothstep's *upper* wall at 3 deg/s never bites.

---

## §3 FINDINGS

### P0-1 — The veto IS late in flight. The normaliser only works from a cold start, and every sweep leg starts cold.

`|aim_net|/aim_net_w = r` exactly on the first tick **only when both
accumulators start at zero**, i.e. only on tick 1 of a freshly constructed
`app::LoopState`. Every sweep arm in the suite does exactly that: LEG 2, the
40 deg/s arms of LEG 3, and every row of the window-sweep table start the
sweep on the first tick of a new fixture. In flight the pilot has been flying
for minutes: `aim_net_w = W`, and when a deliberate sweep begins from a hand
that was at rest `aim_net` starts at ~0, so the measured rate ramps as
`r(1 − e^{−t/W})` — the 5 deg/s sweep Chad's own fly card names reads
**0.22 deg/s on its first tick**, under the 1 deg/s floor, liveness 0, and the
hand-rest clock (already at its 0.25 s cap, gate 1.000) keeps the full
180 deg/s righting running *into the start of a deliberate input*.

`[probe]` belly-up θ170 through `app::step_frame`, 0.6 s of the scripted hand,
then a sustained sweep; integrated |roll_right| measured only after the sweep
starts:

| hand before | sweep | v16 (dial 0) gate@start / leak / veto | SHIPPED (0.20) gate@start / leak / veto | net_rate read on tick 1 |
|---|---|---|---|---|
| still  |  5 deg/s | 1.000 / **0.000 deg** / 0.000 s | 1.000 / **13.869 deg** @180 deg/s peak / **0.108 s** | 0.22 deg/s |
| still  | 12 deg/s | 1.000 / 0.000 deg / 0.000 s | 1.000 / 5.131 deg / 0.042 s | 0.52 deg/s |
| still  | 40 deg/s | 1.000 / 0.000 deg / 0.000 s | 1.000 / 1.152 deg / 0.008 s | 1.74 deg/s |
| ±1-count tremor | 5 deg/s | 0.000 / 0.000 deg / 0.000 s | 1.000 / **16.057 deg** / **0.117 s** | 0.36 deg/s |
| ±1-count tremor | 12 deg/s | 0.000 / 0.000 deg / 0.000 s | 1.000 / 6.074 deg / 0.042 s | 0.39 deg/s |
| ±1-count tremor | 40 deg/s | 0.000 / 0.000 deg / 0.000 s | 1.000 / 1.435 deg / 0.008 s | 1.44 deg/s |

("veto" = seconds after the sweep starts until `hand_gate < 0.01`.)

`[model]` the same runs with `aim_net_w` replaced by the constant `W` give
14.39 / 5.38 / 1.25 deg against the normalised 14.32 / 5.35 / 1.24 — i.e. the
normaliser, the one departure from §3 of the packet that the builder says cost
22 deg/s to find, **buys nothing outside the fixture**. It is dead code in
flight, and the two paragraphs in `controller.h` and `params.h` that justify
it are describing a transient that lasts one second per process launch.

Why this is P0 and not a residual: it contradicts a **standing ruling** (the
gun-director law, Chad 2026-09-12 — "any auto-righting is vetoed while the
hand MOVES"), at the exact instant the law is about (the transition from
resting to aiming), with up to 16 deg of commanded righting roll at the full
inverted_rate clamp; and the record put in front of the pilot asserts the
opposite in four places (commit message "the veto is never late to a real
sweep"; `controller.h` "the veto is never late"; `params.h` "from the first
tick"; toml "a sustained 5 deg/s drift reads 5 deg/s… holds the gate under
0.05"). The suite is all-green while this is true, so nothing would catch it
later either.

**FIX (2 lines, verified in the model).** Clear the accumulators whenever the
hand is not live, so the cold-start exactness applies to every *rising edge*
of `hand_live` rather than once per process:

```cpp
if (!hand_live) { ns.aim_net = glm::dvec3{0.0}; ns.aim_net_w = 0.0; }
else            { ns.aim_net = …; ns.aim_net_w = …; }   // as today
```

`[model]` with that fold: still→sweep leaks **0.00 deg at 5 / 12 / 40 deg/s
with the veto on tick 1** (exactly v16), and the tremor cure is untouched
(integrated-gate proxy 321.2 deg with and without the fold — a tremor is live
on *every* tick, because `aim_rate_world` is ZOH-smeared across the frame, so
the reset never fires during one). It also strengthens the tape argument in
§4: a row recorded on a resting tick then seeds these fields *exactly* rather
than approximately.

Residual the fold does NOT remove: **tremor → deliberate sweep** (the pilot's
hand is already trembling on the mouse and he starts to aim) still leaks
10.26 deg over 0.083 s `[model]` / 16.06 deg over 0.117 s `[probe]`. That is
inherent — a "nets to nothing" discriminator needs a window of observation and
there is no edge to hang a fail-live on. It must be **written down as a
measured residual and ruled on by Chad**, the way v16 recorded its own
(`SESSION_HANDOFF_20260915_kernel_v16.md` §2), not asserted away. Shortening
the window to 0.10 s halves the latency and still cures a ±1-count tremor
(`[leg]` integ 108.77, net 1.04 deg/s) at the cost of all the floor headroom —
that trade is his to make, with both numbers in front of him.

**Also required either way:** a leg that starts the sweep from a rested hand.
Without it the suite certifies "a real sweep is vetoed on tick 1", which is
true only of the fixture. MUTATION: today, deleting the whole normaliser reds
LEG 2 (`hand_rest_max < 0.02`) — so the legs do have teeth *at the cold
start*; what no leg has is any sensitivity to the state the game is actually
in.

### P1-1 — The effective wall is ~1.25–1.6 deg/s, not the documented 1–3 deg/s, and a sustained deliberate sweep below ~1.2 deg/s is now never vetoed at all.

Because the reset is multiplicative, the gate's steady state collapses long
before the smoothstep saturates. `[model]` sustained deliberate sweep, gate at
equilibrium:

| sweep rate | 0.5 | 0.8 | 1.0 | 1.2 | 1.3 | 1.4 | 1.5 | 1.6 | 2.0 | 3.0 deg/s |
|---|---|---|---|---|---|---|---|---|---|---|
| hand_gate | 1.000 | 1.000 | 1.000 | 0.998 | 0.523 | 0.200 | 0.086 | 0.040 | 0.003 | 0.000 |

Two consequences.

1. The safety argument in `controller.cpp` — "SATURATION 3 deg/s — below the
   slowest deliberate input anybody has named … so a real hand of ANY size
   reads fully live" — reads the wrong number. The decision actually happens
   at ~1.3 deg/s, which is **×2.2** the measured tremor rate (0.57 deg/s), not
   the ×5.2 the 3 deg/s wall suggests. The margin the design believes it has
   is less than half of what it has.
2. A genuinely slow deliberate drift — under ~1.2 deg/s, i.e. 2.4 deg of
   reticle travel over two seconds — is now **fully un-vetoed** (gate 1.000)
   where v16 vetoed it 100 %. That is a new hole in the gun-director law that
   the 1 deg/s floor creates, and it is a *size* threshold in rate units: the
   record's "the discriminator is the SHAPE of the motion, not its size" is
   only half true.

Note also that the wall's position is a function of `sim_dt` and
`right_hand_rest` (`hand_rest* = dt(1−live)/live`), neither of which is
mentioned — retuning either moves this dial's behaviour silently.

**FIX.** (a) Put the *effective* wall in `params.h`, derived: gate 0.5 at
live = 0.0625, i.e. net rate ≈ 1.27 deg/s at sim_dt 1/120 and
right_hand_rest 0.25, and state the ×2.2 margin over the tremor rather than
the ×5.2 implied by the saturation. (b) Either move `kNetFloorRate`
deliberately to the rate you intend to be the wall and let the saturation do
real work, or make the reset additive (`hand_rest -= live·k·dt` style) so the
smoothstep's shape is what the pilot feels. (c) Add a leg at ~1.0 and ~1.5
deg/s so the boundary is pinned as a number and cannot drift.

### P2-1 — The cure's envelope is narrow and undocumented, and between the regimes the righting authority PULSES.

The measure is an absolute angular rate, so the tremor's reading scales
linearly with counts × `aim_sensitivity` (a shipped user dial,
`config/controller.toml:1280`). `[model]`, integrated-gate proxy over 2 s
(hands-off reference on the same proxy = 324 deg):

| tremor | ±1 | ±2 | ±3 | ±4 | ±6 counts/frame |
|---|---|---|---|---|---|
| settled net rate | 0.57 | 1.14 | 1.71 | 2.28 | 3.4 deg/s |
| integ proxy | 324 | 295 | **71** | **5.0** | 0.5 deg |

| sens (deg/count) | 0.14 | 0.25 | 0.35 | 0.50 | 0.70 |
|---|---|---|---|---|---|
| integ proxy, ±1 count | 324 | 303 | 261 | **13.6** | 0.9 |

So the fix covers a ±1–2 count tremor at the shipped 0.14 deg/count and is
gone by ±4 counts or by sensitivity ≈ 0.5. At the boundary (±3 counts) the
liveness oscillates with the tremor — `gate_max 0.31` with integ 71 — i.e. the
righting authority *modulates at the tremor frequency* rather than resolving
either way. That is not boolean chatter (the design is right that no new
boolean exists) but it is an oscillating authority, and the record's "no new
threshold to chatter" should not be read as "nothing oscillates".

**FIX.** State the envelope (counts × sensitivity) in `params.h` and the
handoff; add a ±3-count leg so the boundary is pinned; and consider expressing
the floor as counts-per-window (i.e. scaled by `aim_sensitivity`) so a
sensitivity change cannot silently revert the dial to v16.

### P2-2 — `SEADS_TAPE_OPT_FIELDS` is a silent-degradation door with no version guard.

The narrow exception is justified for these four fields and the rulebook
comment in `app/feel_tape_fields.h` is the right shape. But the *mechanism* is
"column absent ⇒ seed 0, silently": nothing distinguishes a genuine pre-column
tape from a truncated one, from one written by a build in which the writer
lost the columns, and nothing is printed when a default is taken. The tape's
own v5 banner already prints `hand_net_window`, so the guard is cheap.

**FIX.** Refuse the omission when the tape's banner says
`hand_net_window > 0` (such a tape must carry the columns), and print one line
naming each column that was defaulted when the exception is taken.

### P3-1 — `|aim_net| / aim_net_w` is an unguarded division.

`aim_net_w ≥ dt > 0` is an assumption about the caller, not a construction:
with `dt == 0` on the first live tick `aim_net_w` is 0 and the ratio is 0/0 →
NaN → `smoothstep` propagates it through `std::clamp` → `hand_live_frac` NaN →
`hand_rest` NaN → `hand_gate` NaN → a NaN roll demand. Unreachable through
`app::step_frame` and `harness::ClosedLoop` (both pass `ap.sim_dt`), but
`control::step` is a public entry point and `app::sting_step` takes `dt` as a
parameter. **FIX:** divide by `std::max(ns.aim_net_w, 1e-12)`, or guard the
whole measure with `if (hand_live && ns.aim_net_w > 0.0)`.

### P3-2 — The frame-rate leg's first stated mutation cannot fire.

LEG 3 says "MUTATION: measure the window in TICKS (leak := a fixed per-tick
constant) and the 10 fps / hitch arms diverge". `dt` is `ap.sim_dt` on every
call site, so `leak = dt/window` **is already a fixed per-tick constant** —
that mutation is the identity. The leg's real teeth are its second mutation
(dropping the ZOH smear), and the frame-rate invariance comes entirely from
`aim_rate_world` being `rotation/(N·sim_dt)` ZOH'd across the frame's N ticks,
not from the units of the leak. Fix the note so a future reader does not trust
a mutation that cannot red.

### P3-3 — Three comment/fact mismatches in load-bearing places.

- `app/feel_tape_fields.h`, the size static_assert: "both are carried in
  `SEADS_TAPE_INT_FIELDS` above" — they are in `SEADS_TAPE_OPT_FIELDS`, which
  is precisely the distinction the whole exception turns on.
- `control/controller.h`, Telemetry: `hand_net_rate` is "written EVERY tick"
  but is 0 on every non-live tick by construction (the measure runs inside
  `if (hand_live)`). A tape analyst reading 0 cannot tell "the hand netted to
  nothing" from "the hand was off the mouse" without also reading `hand_rest`
  — say so, or write the measure on every tick.
- `controller.cpp` calls the accumulator a "leaky window integral"; it is a
  one-pole filter whose time constant is the dial, so `hand_net_window` is a
  time *constant*, not a boxcar width, and ~3× it is where the memory really
  ends. The loader's 1 s upper wall reads differently under that reading.

### P3-4 — The loader's lower wall admits values where the dial is live but does nothing.

`0 ≤ hand_net_window ≤ 1` accepts e.g. 0.05, where `[leg]` shows the debt is
back (integ 3.78 deg, gate 0.017) — a live dial that silently behaves like
v16. And below `sim_dt` the leak clamps to 1 and the measure degenerates to
the instantaneous rate. Safe, but silent. Worth either a lower wall of a few
ticks or a comment naming the degenerate band.

---

## §4 WHAT I TRIED TO BREAK AND COULD NOT (confirmations)

- **The OFF arm is bit-identical.** `Internal ns = internal;` at
  `controller.cpp:167`, every new expression sits inside
  `if (cp.hand_net_window > 0.0)`, the `else` branch is the v16 expression
  character for character, and `aim_net` / `aim_net_w` are copied and never
  written. The only unconditional new statements are two writes to
  `out.telem`, and no control path reads telemetry. I could not construct an
  input that leaks new arithmetic into the golden path.
- **The hash-pin method is genuine, not self-consistency.** I checked out
  `origin/main`'s `control/controller.cpp` into this tree, rebuilt, and ran the
  pin leg: `off_arm 71176a88ee5d8a8f (pre 71176a88ee5d8a8f) v16
  7102cf59b6460f33 (pre 7102cf59b6460f33)` — both recorded hashes are produced
  by the pre-change control law. (`controller.h` / `params.h` carry only struct
  members and comments, so compiling the old `.cpp` against the new headers is
  the pre-change law exactly.) Restored immediately; tree clean.
- **No frame is double-counted or missed.** `app::step_frame` holds the mouse
  in `pending_dx/dy` and consumes it on the first tick that actually runs, so a
  frame that advances zero ticks loses nothing; the consuming tick computes
  `rotation/(N·sim_dt)` and ZOH's it over ticks 2..N, so
  `Σ aim_rate_world·dt` over a frame is exactly the rotation `apply_mouse`
  applied. `aim_moved` (one tick) is used only for the boolean; the integral
  uses the smeared rate. This is the one place the design is exactly right and
  the reasoning in the code is correct.
- **Roundoff cannot read as a hand.** The 1 deg/s floor is 1.7e-2 rad/s
  against a 1e-17 residue; and `leak = clamp(dt/W, 0, 1)` cannot go negative or
  overshoot, so the accumulator cannot oscillate or diverge for any value the
  loader accepts. A tiny window degrades to "the instantaneous rate", which is
  fully live for a tremor — i.e. safely back to v16.
- **The hands-off path is untouched.** The outer `hand_live` gate is what does
  it: the tick the hand stops, `hand_live_frac` is 0 and the clock climbs, and
  `[leg]` measures the two arms bit-identical (110.25 deg, same righting tick
  count, `hand_rest_max` equal).
- **No new boolean exists**, so there is no per-tick chatter; the only
  oscillation found is the authority modulation at the tremor frequency in the
  boundary band (P2-1).

---

## §5 VERDICT

**LAND-WITH-FIX.**

Blocking before Chad flies it:

1. **P0-1** — fold the `!hand_live ⇒ clear the accumulators` change (or an
   equivalent that makes the veto instant on the rising edge of `hand_live`),
   re-measure the table in §3, add the "rested hand, then a sweep" leg, and
   rewrite the four places that claim the veto is never late so they state the
   real residual (tremor → sweep, ~0.1 s / ~10–16 deg) instead.
2. **P1-1** — correct the wall numbers in `params.h` / `controller.cpp` and
   pin the ~1.0 and ~1.5 deg/s boundary; flag to Chad that a sustained
   deliberate drift under ~1.2 deg/s is no longer vetoed, because that is a
   ruling, not an implementation detail.

Then re-gate (the fold touches `control/controller.cpp` only; the off-arm
pins and the hands-off leg must both still be bit-identical) and hand him the
fly card with the residual written on it.

Everything else in §3 is P2/P3 and can ride the same fold. The walk-back is
unchanged and still one line: `config/controller.toml hand_net_window → 0`
= v16.

---

*Method note, per the lane law: every number above names its source. `[probe]`
numbers come from a throwaway TEST_CASE in `test/unit/test_loop_rollover.cpp`
driving the shipped `app::step_frame` on tip `9ba9beb35`, built in
`D:\seads_sandboxes\tremor\build`, reverted before this commit (the working
tree carries only this file). `[model]` numbers come from a transcription of
the four-line recurrence validated against the committed legs' own printed
output at five frame rates. `[leg]` numbers are the committed instrument's
output on this tip.*

---

# FOLD ADDENDUM — 2026-09-16, by the folding session (lane `feel/tremor-netwindow`)

Both lenses are folded in ONE commit on top of `307e30be6`. Every number below
is `[leg]`: printed by a committed leg in `test/unit/test_loop_rollover.cpp`,
run through the shipped `app::step_frame` on the folded tree. The dial count is
still ONE (`[auto_level] hand_net_window`), the OFF arm is still structurally
off, and the three hash pins are still green.

## P0-1 — FOLDED (the shape chosen is the law/feel lens's, and why)

Both lenses found this independently and proposed different one-liners:

* mechanism lens: CLEAR both accumulators whenever the hand is not live;
* law/feel lens: add `dt` to the normaliser ONLY on a live tick.

**Folded: the law/feel shape** — `ns.aim_net_w = internal.aim_net_w +
(hand_live ? dt : 0.0) - leak * internal.aim_net_w;`. It is strictly the better
of the two for the same P0 cure, and the reason is the RATIO: with the gated
increment both accumulators leak together through a rest, so `|aim_net| /
aim_net_w` is preserved across a pause instead of being thrown away. A hand
that stops mid-sweep is therefore still read at its own rate when it resumes
(conservative — fail to the veto), and an INTERMITTENT tremor keeps the memory
of its own netting-to-nothing across the gaps. Clearing discards both, which
costs the cure on exactly the arm the law/feel lens measured as the fold's
weak point (RT5). In the long-rest limit the two are identical, which is the
limit P0-1 is about.

MEASURED on the folded tree, the new leg `S-tremor: a sweep from a FULL gate
vetoes on the next tick` (ctest #2002, the red team's RT1 rig: belly-up
theta 170, hand off 0.40 s so the gate reaches 1.000, then a sustained sweep;
integral |roll_right| AFTER the hand goes on):

| sweep | v16 (dial 0) | SHIPPED, folded | shipped, pre-fold (this record §3) |
|---|---|---|---|
| 3 deg/s | 1.50 deg / 0.008 s | **1.50 deg / 0.008 s** | 25.94 deg / 0.175 s |
| 5 deg/s | 1.50 / 0.008 | **1.50 / 0.008** | 14.42 / 0.100 |
| 10 deg/s | 1.50 / 0.008 | **1.50 / 0.008** | 7.31 / 0.050 |
| 40 deg/s | 1.50 / 0.008 | **1.50 / 0.008** | 2.41 / 0.017 |

The hands-off half is 48.75 deg in both arms at every rate (the 08-06 ruling,
bit-identical, asserted `==`). **The cure is NOT paid for here:** the tremor leg
still reads 109.72 deg ON against 0.00 OFF, and the still-hand reference 110.25
— unchanged to the digit from the pre-fold tip, because a ±1-count tremor is
live on every tick (the ZOH smear) and the gating never fires inside one.

MUTATION, RUN NOT ARGUED: reverting the fold to `+ dt` and rebuilding reds the
new leg on 7 of its 28 assertions and reprints this record's own §3 numbers
(25.94 / 14.42 / 7.31 / 2.41 deg). The leg has teeth in the state the game is
actually in.

THE PRICE, MEASURED AND PINNED (new leg `S-tremor: an intermittent tremor, the
fold's measured price`, #2005 — a count every k-th frame): the discount must now
be EARNED by continuous motion, so

| k | 1 | 2 | 3 | 5 | 10 |
|---|---|---|---|---|---|
| dial ON, folded | 109.72 | 108.49 | 97.45 | 43.27 | 86.01 |
| v16 | 0.00 | 1.44 | 5.56 | 21.02 | 86.58 |

Still better than v16 at every k (the leg pins `ON >= v16 - 3 deg`). That is the
trade and it is taken deliberately: the gun-director law is a STANDING RULING
and the cure is this lane's proposal, so the law wins the tie.

The residual this record names (tremor → deliberate sweep) is unchanged in kind
and is now on the fly card rather than asserted away, together with the 0.10 s
window alternative.

## P1-1 — FOLDED as documentation + two pinned legs; the RETUNE is NOT taken

(a) DONE — `control/controller.cpp` now derives the FELT wall beside the two
constants: the reset is multiplicative, `hand_rest* = dt(1-live)/live`, so the
gate is halved at `live = 1/16`, i.e. **~1.30 deg/s** at sim_dt 1/120 and
right_hand_rest 0.25 — ×2.2 the measured tremor (0.52..0.59 deg/s), not the
×5.2 the 3 deg/s saturation implies — and the note says out loud that the wall
moves with `sim_dt` and `right_hand_rest`, which are not this dial.

(b) NOT TAKEN, deliberately. Moving `kNetFloorRate` or making the reset additive
changes what Chad will FEEL on a dial he has not yet flown once; that is a
ruling, not a fold. It is written into the handoff as the alternative trade
beside the 0.10 s window.

(c) DONE — new leg `S-tremor: the felt wall of the veto, measured` (#2003) pins
the boundary as numbers, from a FULL gate:

| sweep | 0.5 | 1.0 | 1.5 | 2.0 | 3.0 | 5.0 deg/s |
|---|---|---|---|---|---|---|
| integ \|roll_right\| after the hand goes on | 61.50 | 61.50 | 34.24 | 3.59 | 1.50 | 1.50 deg |

(v16 reference at 1.0 deg/s: 1.50 deg.) The leg asserts the HOLE as a fact —
1.0 deg/s is not vetoed, 3.0 is, monotone in between — so it reds in either
direction: if the hole ever closes, somebody has to tell Chad.

## Also folded from the P2/P3 list

* **P3-1** — the division is now `std::max(ns.aim_net_w, 1e-12)` (the line was
  being touched anyway).
* **P3-2** — the frame-rate leg's first mutation note, which could not fire, is
  struck and the reason recorded in place.
* **P3-3** — `feel_tape_fields.h` now says `SEADS_TAPE_OPT_FIELDS` (and why
  that distinction is the whole exception); `controller.h`'s Telemetry comment
  now says the measure runs only while the hand is live, so a 0 means "netted
  to nothing OR hand off".

## NOT folded (and why)

* **P2-1 / law-feel P2-1** (the cure's envelope in counts × `aim_sensitivity`)
  and **P2-2** (the `SEADS_TAPE_OPT_FIELDS` door) — P2, outside this fold's
  mandate (P0/P1), both untouched by the fold's arithmetic, and both carried
  forward in the handoff as named debt. The sensitivity band in particular
  wants the *counts-per-window* form of the floor, which is a felt retune of the
  same class as P1-1(b) and belongs to Chad's ruling, not to a fold.
* **P3-4** (the loader's lower wall admits a live-but-inert window) — P3; the
  band is now named in the toml block rather than walled off, because walling it
  off would make the window sweep leg (which runs 0.05) unrunnable.

## Gate

Lane battery on the folded tree: **52/52 green** (`righthand|tremor|
loop_rollover|controller_load|params|load_controller|yawbudget|TAPE`), including
the four new legs. `gate_baseline.py lint`: OK. Full gate re-run on the folded
tip and quoted BY NAME in the commit message.
