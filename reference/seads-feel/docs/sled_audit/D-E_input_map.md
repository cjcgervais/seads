# D-E — INPUT MAP AUDIT
## The transfer functions between Chad's hands and `sim::SledInputs`

**Strand:** D-E (Q5 of `Game_loop_idea/vehicle_program/SLED_RIDE_AUDIT_PLAN_20260917.md` §3 —
*"Is the input mapping (300 px lean, 2.0/s steer, 2.5/6.0 thumb) the right transfer function for a
weight-position control?"*)
**Date:** 2026-09-18. **Worktree:** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`.
**Read-only.** No kernel, config, golden or `sim/` file was touched. No `seads.exe` run, no ctest
run, no probe built. Every number below comes from (a) reading `app/main.cpp` / `sim/sled.*` /
vendored raylib, or (b) a read-only pass over Chad's own `.sledtape` files in
`D:/flight_sim2/seads-recon/build-play`.

**Tape instrument:** `D:/seads_sandboxes/sled-audit/docs/sled_audit/de_input.py`
(this strand's own; it does **not** modify strand D-B's `tools/sled_tape_audit.py`). Raw per-tape
output is committed beside it as `D-E_input_metrics_a.json` and `D-E_input_metrics_b.json`. Column order
taken from `test/harness/sled_tape.h` via D-B's documented header, not guessed: `T` = 54 tokens,
`tok[4]` = steer command, `tok[5]` = lean_lat, `tok[2]` = throttle, `tok[39]` = `steer_actual`
(pin offset 28), `tok[40]` = `belt_speed_ms` (pin 29); `O` = 45 tokens, `tok[30]` = `steer_actual`.

**Primary set:** the 9 tapes recorded on the three signed kernels that carry today's SK-1d input map
— `sled_tape_{85,86,87,88,89,90,91}` (v15/v17, 2026-09-13 and 2026-09-17) plus `76` and `79`.
**20.55 minutes of Chad's own driving, 147,000 ticks.**

---

## 0. The one-paragraph answer

The sled has four driving axes. **Exactly one of them is a position control — the mouse lean — and
it is the only one Chad has ever praised.** Steer (A/D) and thumb (W) are *rate* controls with a
decay, and a rate control with a decay has only two equilibria: the two rails. Chad's 2026-08-25
complaint *"I am not enough able to hold a steady turn"* was attributed by M11.1 to the then-binary
±1 steer map. **SK-1c/SK-1d replaced the binary map with an analog accumulator and did not change
the set of holdable values.** Measured on his own current-kernel drives: **301 steer excursions,
zero of them held a partial steer angle; the median excursion's plateau is 0.0167 s — one frame.**
Latency is *not* the defect (≈33–50 ms motor-to-photon, inside the band where MacKenzie & Ware find
no measurable cost). The defect is that three of the four axes have no *hold*.

---

## 1. Chad's words, verbatim, with provenance

| # | Words | Source |
|---|---|---|
| C1 | *"I am not enough able to hold a steady turn as I am using wsad and it wont carve so well is this something perhaps adding this snow could ammend or is it better to go straight to kernel"* | `seads-recon/docs/snowform_measurements.md` §M11 header, **Chad, 2026-08-25** |
| C2 | *"steering is too slow"* | quoted in `app/main.cpp:8236-8241` (SK-1d comment), **Chad, 2026-08-25** |
| C3 | *"I dont like the steering via mouse coupled with lean they have to be two independent things, part of what is so fun on the road is driving by lean."* | quoted in `app/main.cpp:6407-6412` (SK-1d comment), **Chad, 2026-08-25** |
| C4 | *"the balance mechanism works really good. Just allow the balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the necessary condition of not rolling over."* | `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0** (drive 4, 2026-08-12) |
| C5 | *"I've seen lots of snowmobiles drift around corners and then with throttle, straighten out. The feel is really good. Don't lose the feel"* | same file, **§0** |
| C6 | *"I need a key for now that lets me autoright until we get the guy running back to the snowmachine"* | quoted in `app/main.cpp:8279-8283` (DRIVE-2 comment) |
| C7 | *"rider animated with helmet, mouse aim used to SHIFT WEIGHT, qweasd to drive, freelook, and the rider head faces the same way you look so you always see the back of the helmet. That way there is continuity between kernels in the use of mouse aim."* | `Game_loop_idea/vehicle_program/BELL.md:25`, routed as a Chad ruling 2026-08-11 |

**C4 and C3 are the fence around this strand.** He has signed the lean *mechanism* as good and has
ruled that lean and steer must stay independent. Nothing below proposes coupling them, and the one
lean rung proposed (§6 E4) changes only the hand-to-command scale, never the law.

---

## 2. The transfer functions, exactly as shipped

All line numbers are `D:/seads_sandboxes/sled-audit` @ `ed0a43ce8`, which is main's tip.

### 2.1 The clock
`params.sim_dt = 1/120 s` (`config/aircraft.toml:10`). `frame_dt = GetFrameTime()`
(`app/main.cpp:6044-6047`). `app::Accumulator` (`app/loop.h`) turns `frame_dt` into whole ticks.
**All four command integrators advance once per FRAME on `frame_dt`; the kernel consumes the
resulting `sim::SledInputs` on every tick of that frame** (`app/main.cpp:8426-8437` builds `sin_`,
`8461-8468` runs `for (int t = 0; t < fr.ticks; ++t) sled = sim::step_sled(sled, sin_, ...)`).
The command path is therefore a **zero-order hold at frame rate on a 120 Hz plant**.

*Measured:* `frame_gap_ticks_p50 = 2.0` in **all 9 tapes** → he drives at **60 fps**, two ticks per
command update. The plant's effective command bandwidth is 60 Hz, half its tick rate.
**Killing mutation:** if the integrators were moved inside the tick loop and advanced on `sim_dt`,
the tape's inter-step gap would become 1 tick and this number would read 1.0.

### 2.2 Lean — mouse — a POSITION control (relative, unbounded travel, no home)
`app/main.cpp:6414-6427`:
```
const double px_full = 300.0;                       // full lean ~ a palm swipe
sled_lean_lat = std::clamp(sled_lean_lat - live.mouse_dx / px_full, -1.0, 1.0);
sled_lean_fwd = std::clamp(sled_lean_fwd - live.mouse_dy / px_full, -1.0, 1.0);
```
Downstream (`sim/sled.cpp:325-334`, `406-413`): `target_lat_m = lean_lat_cmd * lat_reach_m`, where
`lat_reach_m` opens from `lean_lat_seated_m 0.15` to `lean_lat_stand_m 0.35`
(`sim/sled.h:786-787`) with how far the rider has actually risen; `rider_lat_m` then slews with
`lean_tau_s 0.18` **and** `lean_rate_ms 1.40` (`sim/sled.h:818,825`). **The command is a position
and the kernel treats it as a position.** This axis is correctly shaped.

Three properties of the *hand* half, all code-derived:
- **`live.mouse_dx` is RAW device counts, not accelerated pixels.** `app/main.cpp:2249` calls
  `DisableCursor()`; vendored raylib's `DisableCursor` sets `GLFW_RAW_MOUSE_MOTION = GLFW_TRUE`
  (`build/_deps/raylib-src/src/platforms/rcore_desktop_glfw.c:1032`). Windows "enhanced pointer
  precision" does **not** reach this axis. *(This kills the obvious "OS mouse accel is corrupting
  the lean" hypothesis — it is false.)*
- **No deadband, no curve, no quantization guard.** One raw count = 1/300 = 0.00333 lean units =
  **0.50 mm** of rider lateral offset seated. Contrast `input/aim_curve.h`, the house's own
  mouse-conditioning primitive for the *aircraft* aim axis, which carries a frame-rate-normalised
  rate key, an `aim_curve_quant_px` sub-pixel guard and a bit-identical knob-off arm. **The sled's
  lean axis bypasses that primitive entirely.** (Given C3 and C4 this is arguably correct — an
  honest linear position map — but it is a deliberate asymmetry that nobody has recorded.)
- **Full-scale travel = 600 raw counts** (−1 to +1 at 300 counts per unit). At an assumed 800 CPI
  that is **19.0 mm of desk travel from full-left lean to full-right lean**. The CPI is an
  assumption (§7), the 600 counts is not.

### 2.3 Steer — A/D — a RATE control with a decay
`app/main.cpp:8244-8256`:
```
const double steer_key_rate = 2.0 * frame_dt;
if (steer_l) sled_steer_cmd = std::min( 1.0, sled_steer_cmd + steer_key_rate);
if (steer_r) sled_steer_cmd = std::max(-1.0, sled_steer_cmd - steer_key_rate);
if (steer_l == steer_r) { const double back = 3.0 * frame_dt; /* decay toward 0 */ }
```
Then `sim/sled.cpp:299-306`: `steer_actual = slew_toward(steer_actual, in.steer, 0.0,
p.steer_rate_per_s /*2.0*/, h)`, and `delta = steer_actual * steer_max_rad /*0.42*/`
(`sim/sled.h:881`, `sim/sled.h:1186`).

**The player's key is a rate command on a rate-limited follower.** The reachable *steady states* of
`sled_steer_cmd` are exactly **{−1, 0, +1}**:
- A held → +2.0/s → pinned at +1 after **0.500 s**.
- released → −3.0/s → pinned at 0 after **0.333 s**.
- Any intermediate value is a transient. Because both rates are *constant* (not proportional to the
  error), a pulse-width hold has a **neutral, not stable, equilibrium**: net drift = `5.0f − 3.0`
  for key-down duty `f`, zero at **f = 0.600**, with **no restoring force toward any particular
  value**. To hold steer = 0.5 ± 0.05 through a 2 s corner he must keep cumulative key-down time
  accurate to **±10 ms**, which is below human key-timing resolution.

### 2.4 Thumb — W — the same shape as steer
`app/main.cpp:8211-8214`: `+2.5/s` while W is down, `−6.0/s` otherwise. Reachable steady states
**{0, 1}**: full open in **0.400 s**, full closed in **0.167 s**; neutral duty **f = 0.706**. The
spring-return is deliberate and named in the comment (*"off the key it snaps shut fast — that
closure is what makes the CVT free-roll band reachable at all"*), and it is the fastest ramp in the
whole input map — 2.4× the opening rate.

### 2.5 Q/E and C — position-shaped
`app/main.cpp:8258-8261`: Q/E move `sled_lean_lat` at 2.0/s with **no decay** — a trim wheel, i.e.
position-shaped, consistent with the mouse. `app/main.cpp:8263-8267`: C zeroes **all three** of
`sled_lean_lat`, `sled_lean_fwd`, `sled_steer_cmd`.

### 2.6 The map in one table

| axis | device | shape | reachable steady states | has a home? |
|---|---|---|---|---|
| lean_lat / lean_fwd | mouse (raw counts) | **position**, relative, 300 counts/unit | continuous [−1,1] | **no** (only C) |
| lean_lat | Q/E | position (rate-limited travel, holds) | continuous [−1,1] | no |
| steer | A/D | **rate + decay** | **{−1, 0, +1}** | yes (3.0/s) |
| throttle | W | **rate + decay** | **{0, 1}** | yes (6.0/s) |
| brake | S | binary | {0, 1} | yes |
| stand | SHIFT/CTRL | binary tri-state | {−1, 0, +1} | yes |

---

## 3. MEASURED findings from Chad's tapes

All fractions are of ride ticks in the named tape. 9 tapes, 20.55 min, 147k ticks.

### D-E-01 — **SK-1c/SK-1d did not close M11.1. Zero of 301 steer excursions held a partial angle.**
**Confidence: MEASURED.**

M11.1 (`snowform_measurements.md`) recorded the then-binary map and concluded *"There is no way to
HOLD a partial steering angle on the keyboard."* SK-1c/SK-1d replaced binary ±1 with the analog
accumulator of §2.3. The old tapes prove the old map (`kernel-v13g-signed`, 287,003 ticks:
`|steer|` interior occupancy **0.0000** — the command was literally never between the rails). The
new tapes prove the accumulator works as an accumulator — and prove the *hold* never arrived.

Per-tape, current map. "Held partial" = an excursion whose peak was < 0.98 and which spent ≥ 0.25 s
at that peak:

| tape | kernel | excursions | **held partial** | plateau p50 [s] | at full lock | zero | in transit |
|---|---|---|---|---|---|---|---|
| 86 | v17 | 39 | **0** | 0.0167 | 0.159 | 0.564 | 0.277 |
| 88 | v17 | 27 | **0** | 0.0167 | 0.219 | 0.560 | 0.221 |
| 87 | v17 | 29 | **0** | 0.0167 | 0.202 | 0.635 | 0.163 |
| 89 | v17 | 48 | **0** | 0.0167 | 0.111 | 0.682 | 0.206 |
| 90 | v17 | 14 | **0** | 0.0958 | 0.054 | 0.863 | 0.083 |
| 91 | v17 | 57 | **0** | 0.0167 | 0.325 | 0.418 | 0.257 |
| 85 | v15 | 32 | **0** | 0.0958 | 0.170 | 0.676 | 0.154 |
| 79 | v15 | 42 | **0** | 0.0167 | 0.125 | 0.734 | 0.141 |
| 76 | v14 | 13 | **0** | 0.0167 | 0.020 | 0.918 | 0.062 |
| **Σ** | | **301** | **0** | | | | |

The median excursion's plateau is **0.0167 s = one frame at 60 fps** — the command passes through
its peak in a single command update. Between **8.3 % and 27.7 %** of ride time the bar command sits
at a value that, by §2.3, cannot be held.

**Why it matters (C1):** *"I am not enough able to hold a steady turn as I am using wsad."* His
sentence is still literally true of the machine he drove last night.

**Killing mutation:** set the release rate `back` (`app/main.cpp:8253`) to 0.0. `sled_steer_cmd`
then becomes a pure integrator with a hold, and `steer_exc_held_partial` must go from 0/301 to
non-zero on the next drive of comparable length. If it does not, this finding is wrong.

### D-E-02 — **The self-centre (3.0/s) outruns the kernel's bar slew (2.0/s), so the command lies to the HUD on every straightening.**
**Confidence: MEASURED + DERIVED.**

`app/main.cpp:8237-8243` argues the release is 3.0 *so that* "straightening is never the slow half",
and warns in the same breath that "a command that outruns the slew just queues up and lies to the
HUD". **It fixed that on the push and reintroduced it on the release.** `steer_rate_per_s = 2.0`
caps `steer_actual` (`sim/sled.cpp:303`), so from full lock the *command* reaches 0 in 0.333 s while
the *bars* need 0.500 s.

DERIVED ceiling of the divergence: cmd(t)=1−3t, bar(t)=1−2t, max gap at t=0.333 s = **0.333**, plus
one tick of quantization = **0.350**.
MEASURED `cmd_minus_bar_abs_max`: **0.3500** in tapes 86, 87, 88, 89, 79; 0.3663 in 85; 0.4602/0.4608
in 76/90. (Tape 91's 0.983 is a `hands_on == false` episode, not this mechanism — the kernel forces
`steer_target = 0` while the app command persists.)
MEASURED `bar_behind_during_decay_p90` = **0.250 – 0.328** across all 9 tapes, over 570–671 decaying
ticks per tape.

**Why it matters (C1, C2):** he raised "steering is too slow" and the fix matched the *push* to the
kernel. The *release* was left 50 % faster than the machine can act, so for roughly a third of a
second after every corner the command, the HUD and the rider's hands are showing him up to 35 % of
full lock that the skis are not doing.

**Killing mutation:** change `back` from `3.0` to `2.0`. `cmd_minus_bar_abs_max` must collapse to
one tick of quantization (≈0.017) on tapes with no `hands_on` break, and
`bar_behind_during_decay_p90` to ≈0.

### D-E-03 — **The throttle is binary in practice: it is never held at a partial value longer than its own ramp.**
**Confidence: MEASURED.**

`thr_mid_dwell_max` (longest uninterrupted stay in 0.05 < throttle < 0.95) across the 9 tapes:
**0.350, 0.467, 0.517, 0.467, 0.350, 0.367, 0.633, 0.500, 0.458 s.** The up-ramp alone
(0.05→0.95 at 2.5/s) is **0.360 s**. Every "modulated throttle" in 20 minutes of driving is the
ramp, not a hold. WOT occupancy 0.037–0.772; zero occupancy 0.154–0.928; the residue
(0.017–0.126) is transit.

`thr_closed_but_engaged_frac` (throttle 0, belt ≥ `clutch_engage_ms 3.25`, i.e. the 420 N engine
brake is on) = **0.036–0.224**. He enters the engine-brake band every time he lets go, and reaches
it in **0.167 s**.

**Why it matters (C5):** *"drift around corners and then with throttle, straighten out. The feel is
really good."* The canonical move needs a throttle he can *meter* through the corner; the map gives
him a switch with a 0.4 s rise and a 0.17 s fall.

**Killing mutation:** raise `2.5` (`app/main.cpp:8212`) and lower `6.0` (`:8214`) toward each other.
If `thr_mid_dwell_max` stays pinned at the ramp time under the new rates, the throttle is binary for
a reason other than the ramp and this finding is wrong.

### D-E-04 — **The lean command has no zero. It is at exact zero for 0.1–0.2 % of ride time, and C is never pressed.**
**Confidence: MEASURED.**

In the five tapes that are real rides (86, 87, 88, 91, 85 — as opposed to 76/79/90, which are mostly
parked: `cvt_freecoast_frac` 0.48–0.89):

| tape | lean exact-zero frac | mean \|lean\| | \|lean\| > 0.98 | C presses/min |
|---|---|---|---|---|
| 86 | 0.00105 | 0.685 | 0.282 | **0** |
| 88 | 0.00202 | 0.536 | 0.201 | **0** |
| 87 | 0.00182 | 0.530 | 0.285 | **0** |
| 91 | 0.00236 | 0.665 | **0.436** | **0** |
| 85 | 0.00173 | 0.729 | 0.147 | **0** |

The rider's body is at the edge of its reach box 15–44 % of the time and at the machine's centreline
essentially never. This is the signature of a **relative** position control with no spring: the
hand's zero and the sled's zero are not the same object, and nothing re-registers them. C exists
(`:8263`) and he has never used it in 20 minutes.

**Why it matters (C3, C4):** he likes driving by lean and has signed the mechanism. This finding is
**not** a complaint about the law — it is a measurement that the axis he drives with spends its life
pinned or drifting rather than centred, and it is the input-side half of D-E-05.

**Killing mutation:** add a spring return on the lean command (any non-zero decay toward 0 when the
mouse is still). `lean_exact_zero_frac` must rise by orders of magnitude. **Do not ship this** —
C3/C4 fence it; it is stated here only as the test of the claim.

### D-E-05 — **The rolling band is a TRANSIT, and the crossing rate — not the dwell — is what tracks rollovers.**
**Confidence: MEASURED (correlation), n = 9, does not reach p < 0.05.**

GI_DRIVE1 §S1 (`GI_DRIVE1_HANDOFF.md:27-30`): *"FULL lean-into is the SAFEST case (8/8 clean, faster
laps); the rolling region is HALF lean-into and lean-against at v0=20 — the intermediate band a live
mouse hand crosses constantly."*

Measured occupancy of |lean| ∈ [0.3, 0.7] on the current map: **0.055 – 0.353** of ride time, entered
**5.2 – 29.7 times per minute**, median uninterrupted dwell **0.083 – 0.458 s**.

Against `rollovers_past90_per_min` from strand D-B's `tape_summary.json` (same 9 tapes):

| predictor | Pearson r (n=9) |
|---|---|
| **band entries / min** | **+0.815** |
| band occupancy fraction | +0.261 |
| lean sign reversals / min | +0.627 |
| throttle WOT fraction | +0.607 |
| steer excursions / min | +0.198 |
| A/D key presses / min | +0.059 |

Controlling for ride intensity (WOT fraction), the partial correlation
r(entries, rollovers | WOT) = **+0.693** (df = 6; the two-tailed 5 % critical value at df = 6 is
≈0.707, so **this does not reach conventional significance and must not be reported as established**).

The *shape* is the point and it is what GI_DRIVE1 predicted: **occupancy barely correlates, crossings
strongly do.** The band is not a place he sits; it is a gate he passes through, and the input map
sets how often. Because full-scale lean travel is 600 raw counts (§2.2), a single ordinary mouse
sweep crosses the whole band in ~120 counts.

**Why it matters (C4):** *"tighten a turn instead of having to be the necessary condition of not
rolling over."* If the crossing is the exposure, the cheapest lever is the hand scale, not the law.

**Killing mutation:** raise `px_full` (`app/main.cpp:6415`) from 300 to 600. Band entries per minute
must fall for the same driving; if rollovers/min do not follow, the correlation was ride intensity
and this finding is wrong.

### D-E-06 — **★ AUTORIGHT (R) recentres the lean but not the bars. 4 of 6 rightings in the current tapes restored the machine at FULL LOCK.**
**Confidence: MEASURED. This is a defect, not a tuning question.**

`app/main.cpp:8287-8365` — the R block zeroes `sled_lean_lat` and `sled_lean_fwd`
(**lines 8356-8357**) and does **not** zero `sled_steer_cmd`. C (`:8263-8267`) zeroes all three. The
kernel's `steer_actual` is not in the override's reset list either.

Read directly out of the `O` records and the `T` record immediately preceding each:

| tape | tick | `steer_actual` **after** the autoright | command before |
|---|---|---|---|
| 86 | 5804 | **1.0** | 1.0 |
| 88 | 46589 | **1.0** | 1.0 |
| 88 | 48179 | 0.4666 | 0.4667 |
| 91 | 6372 | **1.0** | 1.0 |
| 91 | 15130 | **1.0** | 1.0 |
| 85 | 60106 | 0.6452 | 0.6497 |

**Six rightings, zero at centre, four at full lock.** R sets the machine upright, on the surface,
with all motion killed — and with the handlebars still hard over and the command still asking for
hard over. The moment he opens the thumb (0.400 s to WOT) he is in a full-lock turn he did not ask
for, on a machine that M11.2/M12 say rolls at *any* non-zero steer on Road.

**Why it matters (C6):** *"I need a key for now that lets me autoright until we get the guy running
back to the snowmachine."* A recovery key that hands the machine back mid-corner is not a recovery.

**Killing mutation:** add `sled_steer_cmd = 0.0;` beside lines 8356-8357. The `O` record's
`steer_actual` will still be non-zero for the override tick (the kernel field is not reset), but the
bar must then slew to centre within 0.500 s instead of being re-commanded to lock. If the next
tapes still show `steer_actual` pinned at 1.0 for > 0.5 s after an `O`, the app-side command was not
the cause.

**Context for the size of the prize (all 84 parseable tapes, 206 min):** 146 R presses, **144 of
them at a body tilt ≥ 75.06°** (the kernel's own rolled threshold) — so R is being used exactly as
designed, not as a teleport. Tilt-before-R: p10 99.6°, p50 112.0°, p90 157.6°, max 174.1°. Against
1032 past-90 rollovers of which 716 were never self-recovered, **R is pressed for only ~20 % of
unrecovered rollovers** — he mostly does not reach for it. *(R rate by kernel: v13g 1.079/min,
pre-reconcile 0.722, v17 0.416, v15 0.120 — but v17 and v15 rest on 5 and 2 presses respectively and
must not be read as a trend.)*

### D-E-07 — **Latency is NOT the defect. ≈33–50 ms motor-to-photon, inside the no-measurable-cost band.**
**Confidence: DERIVED (from measured 60 fps + read raylib ordering).**

Budget, per frame at the measured 60 fps (`frame_gap_ticks_p50 = 2.0`, all 9 tapes):
1. Event age at poll: **0 – 16.7 ms** (mean 8.3). raylib polls in `EndDrawing`, *after*
   `SwapScreenBuffer` (`build/_deps/raylib-src/src/rcore.c`, `EndDrawing`: swap → wait → `PollInputEvents`).
2. Poll → the sled tick that consumes it: **~0 ms**. The mouse lean block (`:6414`), the key block
   (`:8191+`) and the sled tick loop (`:8461`) are all in the *same* frame iteration; `sin_` is built
   at `:8426` and consumed at `:8468`. There is no cross-frame carry on the sled path — `pending_dx`
   is the aircraft's accrual and is explicitly zeroed for the sled at `:6430`.
3. Tick → swap: **≤ 16.7 ms**.
4. Swap → photons: **≥ one refresh interval**, not measured.

Total ≈ **33–50 ms**. MacKenzie & Ware (INTERCHI '93) measured 8.3 / 25 / 75 / 225 ms: at 225 ms
movement time +63.9 % and error rate +214 %; the effect is "easily measured" at 75 ms; error rates
stay under 5 % through 75 ms. **This budget sits below their first measurable step.**

**Why it matters:** it kills a tempting rung. "Add input smoothing / prediction / a lower-latency
path" is not supported by anything in this strand, and the one-frame ZOH (§2.1) costs at most 16.7 ms
of an already-cheap budget.

**Killing mutation:** if a display-side measurement (which this strand did not make) put his actual
end-to-end latency above ~75 ms, this finding is void and latency re-enters the ladder.

### D-E-08 — **There is no deadband anywhere in the map, and the sled bypasses the house's own mouse-conditioning primitive.**
**Confidence: MEASURED (code).**

No axis has an inner or outer deadzone: steer, thumb, lean and the kernel's `clamp`s are all pure.
The aircraft aim axis, by contrast, runs `input::aim_curve` with `aim_curve_quant_px` (a documented
sub-pixel guard against fps-scaled rate noise), `aim_curve_knee`, `aim_curve_gain_max` and a
bit-identical knob-off arm. The sled's lean reads `live.mouse_dx` **raw, upstream of that call**
(`:6425` vs `:6230`).

Because raw motion is on (§2.2) and there is no deadband, one device count moves the rider 0.50 mm
seated. That is *not* harmful on its own — it is honest and fine-grained. It is recorded here
because (a) nobody has written it down, and (b) it is the reason a lean-side deadband rung would be
a *new* mechanism rather than a tuning of an existing one, which matters under C4.

---

## 4. Answering the four questions the strand was set

**Q: effective input latency through the fixed-tick path?** ≈33–50 ms; not the problem (D-E-07).

**Q: dead zones?** None, on any axis (D-E-08).

**Q: saturation?** Steer command at a rail **77–98 %** of ride time (zero + full lock). Throttle at a
rail **87–98 %**. Lean at a rail 15–44 % and at centre 0.1–0.2 % (D-E-01, 03, 04).

**Q: the intermediate lean band a live hand crosses?** Occupied 5.5–35.3 % of ride time, entered
5.2–29.7 ×/min; the **crossing rate** correlates with rollovers at r = +0.815 (partial +0.693
controlling for WOT), the occupancy fraction barely at all (D-E-05).

**Q: is steer being a rate, not a position, why "can't hold a carve" reads as unstable?** **It is
half the answer and must not be sold as the whole one.** The input map cannot express a held steer
angle — proven analytically (§2.3) and measured 0/301 (D-E-01). *But M11.2/M12 separately measured
that the plant has no carveable steer angle on Road at any speed* (every steer from 0.02 to 0.26
rolls past 140° under a 12 s speed-held sweep). **A steer position mode alone would buy him a more
reliable rollover, not a carve.** The input rung is necessary and not sufficient; the plant half
belongs to strands D-A / R1.

**Q: do thumb ramps interact with the CVT back-drive band?** Yes, and the interaction is *by design
and working*: the 6.0/s closure is 2.4× the 2.5/s opening precisely so the free-coast band
(`clutch_engage_ms 3.25`) is reachable, and the tapes show it reached (`cvt_freecoast_frac`
0.044–0.893; `thr_closed_but_engaged_frac` 0.036–0.224). The cost is that the *same* asymmetry makes
partial throttle unreachable (D-E-03). This is a genuine trade, not a bug.

**Q: what does the R key usage say?** It is used as designed (144/146 presses past the rolled
threshold), it is used for only ~20 % of unrecovered rollovers, and **it hands the machine back with
the bars hard over** (D-E-06).

---

## 5. Literature

| Source | What it supports here |
|---|---|
| MacKenzie, I. S. & Ware, C. (1993). *Lag as a Determinant of Human Performance in Interactive Systems.* INTERCHI '93, 488-493. https://www.yorku.ca/mack/CHI93b.html — lags 8.3/25/75/225 ms; at 225 ms MT +63.9 % (911→1493 ms), errors +214 % (3.6→11.3 %), bandwidth −46.5 %; effect "easily measured" at 75 ms. | **D-E-07**: the 33–50 ms budget is below the first measurable step. LITERATURE. |
| Order-of-control literature on rate vs position control (Zhai, *Human Performance in Six Degree of Freedom Input Control*, ch. 2.3, http://etclab.mie.utoronto.ca/people/shumin_dir/papers/PhD_Thesis/Chapter2/Chapter23.html; Kantowitz & Elvers, *Fitts' Law with an Isometric Controller: Effects of Order of Control and Control-Display Gain*, J. Motor Behavior 20(1), 1988): "the majority of studies concluded that rate control is inferior to position control"; position control wins at high index of difficulty, rate control only at coarse/low-ID tasks; zero-order = position, first-order = velocity. | **D-E-01/02**: holding a specific carve angle is a *high*-ID task, exactly where rate control is measured to lose. LITERATURE. |
| Campbell et al. (2008), *Fitts' Law Predictions with an Alternative Pointing Device (Wiimote)*, HFES 52(19): first-order (rate) control produced smaller effective distances than zero-order on the same device. https://journals.sagepub.com/doi/abs/10.1177/154193120805201904 | Same-device evidence that the *order of control*, not the device, is the variable. LITERATURE. |
| Jibb Smart, *7 Building Blocks for Better Controls* (Game Developer / GyroWiki): on winding stick steering — "**Engaging the stick is like grabbing the steering wheel, and releasing the stick is like letting go**", with the wheel allowed to "slip through" at configurable rates on partial release; and on inner deadzones — "Even modern games and controllers have a noticeable deadzone in the middle, which makes it harder to make very small left-right steering adjustments", with outer deadzones "much less intrusive … because players are not trying to move from one side of it to the other." https://www.gamedeveloper.com/game-platforms/7-building-blocks-for-better-controls | **E1/E2**: shipped-game precedent for a steering axis that *holds*, with a tunable slip rate — which is exactly the one-dial shape of E2, and it names the slip rate as the dial. LITERATURE. |

---

## 6. Candidate rungs — at most 5, one-dial shape, with identity values

Every rung below is (a) one named, config-tunable dial in a new `[sled_input]` block, (b) **bit-identical
to today at its identity value**, and (c) justified by a tape event **and** one of Chad's words.
None of them touches `sim/`, a golden, or a signed kernel dial. Ranked.

### E1 — `[sled_input] steer_centre_per_s` — **identity 3.0** (today's `back`, `app/main.cpp:8253`)
**Rule it to 2.0**, matching the kernel's `steer_rate_per_s`.
*Tape event:* `cmd_minus_bar_abs_max = 0.3500` in 5 of 9 tapes, DERIVED ceiling 0.350;
`bar_behind_during_decay_p90` 0.250–0.328 over 570–671 decaying ticks/tape (D-E-02).
*Chad's word:* C1 — *"I am not enough able to hold a steady turn"*; and SK-1d's own stated law, "a
command that outruns the slew just queues up and lies to the HUD."
*Why first:* smallest possible change, removes a self-inflicted lie, cannot make anything less
holdable than it already is, and does not touch the hold structure he may want kept.
*Measure:* `cmd_minus_bar_abs_max` must fall to ≈ one tick.

### E2 — `[sled_input] steer_hold_frac` — **identity 0.0** (today)
Scales the self-centre: `back = steer_centre_per_s * (1 − steer_hold_frac) * frame_dt`. At **1.0** the
bars hold where you left them (Jibb Smart's "grabbing the wheel"); C and the existing hands-off law
still centre them. E1 is the special case `steer_centre_per_s = 2.0, hold_frac = 0`.
*Tape event:* **0 of 301** steer excursions held a partial angle; median plateau one frame (D-E-01).
*Chad's word:* C1, verbatim and unamended since 2026-08-25.
*The honest caveat, on the record:* M12/M11.2 measured that **no carveable steer angle exists on Road
at any speed**, so E2 alone makes the rollover *more repeatable*, not less. **E2 should not be flown
before or without the plant-side rung from D-A/R1.** Shipping it alone risks confirming his complaint
harder.
*Measure:* `steer_exc_held_partial` > 0; `steer_exc_saturated_frac` down from today's 0.25–0.64.

### E3 — **R recentres the bars** — *identity = the line's absence* (today)
Add `sled_steer_cmd = 0.0;` beside `app/main.cpp:8356-8357`, so the autoright block recentres the
same three commands C already does. Not a dial — a one-line correction of an asymmetry.
*Tape event:* 6 autorights in tapes 85/86/88/91; **0 at centre, 4 at `steer_actual = 1.0`** (D-E-06).
*Chad's word:* C6 — *"I need a key for now that lets me autoright."*
*Why high:* it is the only finding in this strand that is unambiguously a defect rather than a feel
judgement, it costs one line, and it is orthogonal to E1/E2.
*Measure:* no `O` record followed by `steer_actual` > 0.1 sustained past 0.5 s.

### E4 — `[sled_input] lean_px_full` — **identity 300.0** (`app/main.cpp:6415`)
Rule it up (600 halves the band-crossing rate per unit of hand motion and doubles full-scale hand
travel from ~19 mm to ~38 mm at 800 CPI). **Changes only the hand-to-command scale — not the lean
law, not the reach box, not `lean_tau_s`, and it does not couple lean to steer.**
*Tape event:* rolling-band entries 5.2–29.7/min correlate with rollovers at r = +0.815 (partial
+0.693 controlling for WOT), while band *occupancy* correlates at only +0.261 (D-E-05); lean at exact
zero 0.1–0.2 % of ride time (D-E-04).
*Chad's word:* C4 — *"the balance mechanism works really good … ENHANCE ability"* — and C3 —
*"part of what is so fun on the road is driving by lean"*, which is why this rung changes the *scale*
and nothing else.
*Caveat:* the correlation does not reach p < 0.05 at n = 9, and a coarser axis is a real feel change
to something he signed. **This is a dial to put under his hand on a drive, not a value to pick for
him.**
*Measure:* `lean_band_entries_per_min` down; `lean_gt_0p98_frac` down; his verdict.

### E5 — `[sled_input] thumb_close_per_s` — **identity 6.0** (`app/main.cpp:8214`) — **LOWEST, direction not established**
*Tape event:* `thr_mid_dwell_max` 0.350–0.633 s across all 9 tapes vs a 0.360 s up-ramp — the throttle
is never held partial (D-E-03).
*Chad's word:* C5 — *"drift around corners and then with throttle, straighten out."*
*Why last, and stated plainly:* the fast closure is deliberate and load-bearing (it is what makes the
CVT free-roll band reachable, and the tapes show him living in that band 4–89 % of the time). His
canonical move is a throttle *punch*, which the current map serves well. **I have a measurement that
the throttle is binary and no word of his that says it should not be.** This rung is listed so it is
on the record, not because the evidence carries it.

---

## 7. What this strand refuses to claim

- **His mouse CPI and Windows sensitivity are unknown.** The "600 raw counts full-scale" is measured
  from `px_full = 300.0`; the "19 mm of desk travel" assumes 800 CPI and is **GUESS** in that factor.
  Ask him, or read it off his mouse, before any E4 number is picked.
- **No end-to-end latency was measured.** D-E-07's budget is DERIVED from the measured 60 fps and the
  read raylib ordering. Display and GPU queue latency are not in it.
- **The D-E-05 correlation is n = 9 and does not reach p < 0.05.** It is a shape that matches an
  independent prior prediction (GI_DRIVE1 S1), not an established effect. It is confounded with ride
  intensity; the partial correlation is reported and is also short of significance.
- **I cannot separate Q/E lean trim from mouse lean in the tape.** Both write `sled_lean_lat`; the
  tape records the command, not the device. Every lean statement here is about the *command*.
- **The v15/v17 R-key rates rest on 2 and 5 presses.** No trend is claimed from them.
- **I did not build or run `seads_sled_probe`, `seads.exe` or ctest.** No number here comes from a
  simulation I ran; every number is either read from source or read from a tape he recorded.
- **"Can't hold a carve" is not fully explained by this strand.** The input half is proven
  (0/301). The plant half — M11.2/M12's finding that no carveable angle exists on Road at any speed —
  is owned by D-A/R1 and is the reason E2 carries a do-not-ship-alone warning.
- **No control subject.** "0 of 301" is Chad on this map; it is not evidence that *no* player could
  ever pulse-hold an angle, only that the equilibrium structure of §2.3 makes it a ±10 ms timing task
  and that he does not do it.

---

*Instrument, for reproduction:*
`python docs/sled_audit/de_input.py <tape…>` from the audit worktree, against
`D:/flight_sim2/seads-recon/build-play/sled_tape_{76,79,85,86,87,88,89,90,91}.sledtape`.
Tapes opened read-only; nothing in `seads-recon` was written.
