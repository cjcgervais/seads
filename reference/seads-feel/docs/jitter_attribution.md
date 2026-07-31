# Rudder/elevator "flapping" at small deflections — offline attribution from the tapes

**Date:** 2026-07-30 · **Analyst:** offline, read-only (no code/config/test touched, no commits)
**Tapes:** `D:\flight_sim2\seads-recon\build\felt_flight_{2,3,4,5,6}.seadsrec`
(2026-07-30 11:39–11:41, all tagged
`sandbox/kernel-v5-reconcile@game-kernel-v5-37-g61753affd kernel=flight-kernel-v9-2026-07-29`)
**Method:** derived from the recording's own per-tick pins only. **The controller was never
re-run.** See §7 for exactly what was derived and how it was validated.

---

## 1. Feel language first (Chad, read this part)

You said: at ~2° of mouse deflection the **rudder jumps and flaps** — lots of high-frequency
*large* deflections instead of one smooth held deflection — and the **nose bounces inside the
mouse aim**. The elevator does it too on slight pitch input. At 5–10° everything is smooth.
Ailerons are fine.

**The tapes say you are reading the airplane exactly right, and the number is worse than
"flapping" suggests: the rudder is going to the STOP.**

Backing the plant's own torque law out of the recorded angular velocity gives the exact commanded
control-surface deflection every tick. Over 104 seconds of near-centre tracking across the five
tapes:

- The rudder makes a **full sign reversal with both lobes past 30% of full travel 6.2 times per
  second**. The elevator does it 3.3 times per second. The aileron does it 0.28 times per second.
- **10% of all near-centre ticks have the rudder pinned at ±100% deflection.** Median peak of one
  of these events is **1.00 — the hard stop.**
- Above 5° of aim error those same numbers collapse to **0.24/s rudder, 0.17/s elevator** — while
  the *aileron* rate goes UP (0.89/s). That is your report, measured: rudder + elevator flap in
  close, ailerons don't, and it stops when you deflect harder.

**What is doing it: the pool-ball capture machine (`[capture]`, S-rimshot v3), the arrival law.**
Not the aim feedforward, not the deadzone, not a gain ring. The give-away is the *size* of the
kick. When your aim sits half a degree off the nose, the plain pointing law would ask for about
3 °/s of yaw. The measured peak is **25–45 °/s — a median of 6.7× the pointing law, and 0.70× of
`sqrt(2·α·d)`**, which is the capture machine's dead-blow return law written out with the airframe's
own authority. The machine is the only thing in the kernel that commands that number, and the
number it commands lands where it is measured.

And the reason it stops at 5°: **ENGAGE is gated to `err < blend_lo`, and `blend_lo` is exactly
5.0°.** Above that the arrival is flown by the bank channel and the machine is structurally off —
which is why the ailerons are clean and the hard deflections feel smooth.

So the felt symptom is one mechanism doing its designed job at a scale where you can feel every
individual bounce: **each time your hand nudges the aim ~0.5–2° off the nose, the machine treats it
as a fresh arrival, slams the rudder to the stop, glances the rim, dead-blows home, and re-arms
about 125 ms later** — 2.7 to 4.8 of these per second while you track. Each one individually is
"correct". Stacked at 4/s they read as flapping.

**The knife-edge (second job) is a completely separate finding and, in my read, the more important
one.** The premature inversion is *not* the side-cone. See §6.

---

## 2. What each tape actually contains (identified from its own data, not filenames)

| tape | ticks / s | signature countables | content |
|---|---|---|---|
| `felt_flight_2` | 4624 / 38.5 s | 0 freelook, 0 key overrides, V 213–250, alt 1617–2382, nose elev −7…+30°, one 92° bank crossing, mouse-event ticks 30% (median 3 px) | **Long near-centre tracking cruise.** The densest small-deflection material in the set; 141 near-centre rudder events. |
| `felt_flight_3` | 2582 / 21.5 s | 2 isolated 0.1 s freelook taps (t=8.1 s, t=20.1 s), 0 keys, V 236–259, bank to 82°, **0 inverted ticks** | Tracking + medium banked turns, two freelook glances. Aim reconstruction anchors at t=8.08 s. |
| `felt_flight_4` | 4584 / 38.2 s | 3 freelook taps, **16 zoom ticks (RMB)**, nose elev **−77…+37°**, **|bank| to 180°, 593 inverted ticks**, alt 1004–2288 | **THE KNIFE-EDGE TAPE.** Contains the climb → mouse-down → roll-to-inverted → 77° dive event, plus four partial banks of the same shape. All §6 evidence comes from here. |
| `felt_flight_5` | 1622 / 13.5 s | 1 freelook tap, V 263–268 (flat), 11 sustained aileron episodes alternating ±45…71° bank, sparsest mouse (15%, median 5 px, p90 13 px) | **Rolling/turn-reversal tape.** Big discrete flicks, long hands-off coasts between them. |
| `felt_flight_6` | 2552 / 21.3 s | 0 freelook, **140 ticks of ROLL override keys** (the only keyboard use in the set), tightest mouse (median 2 px), nose elev −9…+9°, alt 657–1059 | **Tight low-level fine tracking with aileron keys.** Second-densest near-centre event tape (75 events, 3.4/s). |

None of the five contains a respawn; all fly full throttle; none reaches near-vertical nose-up (max
nose elevation anywhere = **+37°**, tape 4). Chad's "straight up then down" is, on the tape, a
**~35° climb followed by a mouse-down flick** — which matters for §6.

---

## 3. The as-flown table (correction to the brief)

The recordings were flown on `seads-recon@HEAD`. Two of the brief's premises do not match it:

| knob | brief said | **as-flown (`seads-recon@HEAD`)** | `seads-feel` working tree |
|---|---|---|---|
| `push_gate.side_cone_enter` / `_exit` | 27.5 / (35 queued) | **37.5 / 42.5** | 27.5 / 32.5 |
| `push_gate.side_pure_enter` / `_exit` (rung F, "sacred middle") | — | **18.0 / 23.0 (present)** | *does not exist* |
| `push_gate.horizon_enter` / `_exit` | — | **45.0 / 40.0** | 45.0 / 40.0 |
| `coordination.lean_gain` | — | **6.0** | 8.0 (uncommitted, post-dates the tapes) |

So the flown build **already had** the widened cone (37.5°) *and* the sacred-middle arm. The
`seads-feel` tree is simultaneously *older* on `push_gate` and *newer* on `lean_gain`; neither tree
is the flown one. All analysis below uses the `seads-recon@HEAD` values.

Other as-flown values used: `capture.circle_deg = 0.55`, `carry = 1.0`, `rim_frac = 1.0`,
`engage_frac = 0.95`, `return_w = 400 °/s`; `aim_ff.gain = 0.3`, `tau = 0.01`;
`deadzone lo/hi = 0.03/0.05`, `rest_dwell = 0.15 s`; `regime blend_lo/hi = 5.0/9.0`;
`outer.K_theta = 3.2`, `coordination.yaw_scale = 2.0`, `pursuit.expo = 3.0`, `pursuit.step = 0`;
`ui.aim_sensitivity = 0.14`.

---

## 4. Evidence

### 4.1 The headline discriminator — "flap rate" by error band

A **flap** = a full sign reversal of the *derived commanded deflection* where both lobes exceed
0.30 of full travel. (Pooled over all five tapes.)

| regime | time | rudder flaps | elevator flaps | aileron flaps |
|---|---|---|---|---|
| **FINE, err < 5°** | 103.8 s | **648 (6.24 /s)** | **339 (3.27 /s)** | 29 (0.28 /s) |
| **MANEUVER, err ≥ 5°** | 29.2 s | 7 (**0.24 /s**) | 5 (**0.17 /s**) | 26 (**0.89 /s**) |

Rudder flapping is **26× denser** below 5° than above it; elevator **19×**; aileron runs the other
way (**3× denser above**). This reproduces Chad's report on all three axes and on both sides of the
threshold, and the threshold is `blend_lo = 5.0°` — the ENGAGE gate.

### 4.2 Excursion-event rate keyed on the error AT ONSET

Event = a run of |derived Input| > 0.55 (expanded out to 0.20).

| onset aim-to-nose error | time | rudder events | /s | elevator events | /s |
|---|---|---|---|---|---|
| inside the reticle circle (< 0.55°) | 41.8 s | 112 | **2.68** | 62 | **1.48** |
| 0.55 – 1° | 14.3 s | 56 | **3.90** | 16 | 1.12 |
| 1 – 2.5° | 21.3 s | 103 | **4.84** | 54 | **2.54** |
| 2.5 – 5° (FINE top) | 26.5 s | 85 | 3.21 | 44 | 1.66 |
| 5 – 9° (blend band) | 9.4 s | 10 | **1.07** | 6 | 0.64 |
| > 9° (MANEUVER) | 19.8 s | 16 | **0.81** | 8 | 0.40 |

The rate **peaks at 1–2.5° of error** — Chad's "~2°" — and falls 4–6× as soon as the error leaves
the FINE endgame. 112 rudder events fire with the aim *already inside the drawn reticle circle*.

### 4.3 Event anatomy (356 near-centre rudder events, pooled)

| quantity | p10 | median | p90 |
|---|---|---|---|
| error at onset | 0.15° | **1.10°** | 3.55° |
| duration | 17 ms | **58 ms** | 242 ms |
| ticks at ±100% deflection | 0 | **1** | 12 |
| peak deflection | 0.64 | **1.00 (the stop)** | 1.00 |
| sign reversals inside the event | 0 | **1** | 4 |
| lateral apex *past* centre | 0.00° | 0.01° | **0.28°** |
| error at closest approach | 0.04° | 0.64° | 3.11° |
| peak body yaw rate | 8.9 °/s | **25.7 °/s** | 43.7 °/s |

Read across: approach → **one** reversal → apex lands **inside** the 0.55° rim (p90 = 0.28°, rim
fraction ≈ 0.5) → home in ~58 ms. That is textbook mechanism **B** — approach / crossing / rim
glance ≤ circle_deg / dead-blow return over 50–100 ms / one reversal per event.

### 4.4 The amplitude law — the decisive quantitative test

For every event, the measured peak body rate was compared against two candidate demands computed
from the tape's own state (α = c·max(q,q_floor)·δ_max_eff(V)/I, all from `aircraft.toml`):

| axis | measured ÷ **plain pointing law** `K_θ·[yaw_scale]·e·(1+expo·e)` | measured ÷ **capture return law** `sqrt(2·α·e)` |
|---|---|---|
| yaw (n=346) | p10 2.11 · **median 6.71** · p90 32.9 | p10 0.41 · **median 0.70** · p90 1.67 |
| pitch (n=169) | p10 1.12 · **median 7.93** · p90 61.3 | p10 0.13 · **median 0.50** · p90 1.59 |

The pointing law under-predicts by ~7×. The capture machine's `sqrt(2·α·d)` dead-blow ceiling
predicts it within a factor of ~1.4. **Nothing else in the kernel commands that magnitude at that
error.**

### 4.5 Mechanism A — `[aim_ff]` per-pixel impulses: real, but ~50× too small

`aim_rate_ff` is pinned per tick, so the ff demand `0.3 × aim_rate` is *measured*, not inferred.

| tape | ff yaw demand, FINE ticks (median / p95) | ff pitch (median / p95) | quiet tick-to-tick ΔInput.yaw (median / p95) |
|---|---|---|---|
| f2 | 0.00 / 1.34 °/s | 0.00 / 3.96 °/s | 0.0016 / 0.165 |
| f3 | 0.00 / 2.25 | 0.00 / 5.05 | 0.0001 / 0.115 |
| f4 | 0.00 / 9.51 | 0.00 / 1.29 | 0.0001 / 0.132 |
| f5 | 0.00 / 0.72 | 0.00 / 2.36 | 0.0001 / 0.017 |
| f6 | 0.00 / 2.45 | 0.00 / 4.49 | 0.0012 / 0.110 |

The frame rate is ~60 fps (`frame_ticks` histogram is 1:2344 / 2:2261 in f2), and the ZOH smear
does produce a genuine **~60 Hz line in the rudder spectrum (f3: 59.9 Hz, amplitude 0.031)** — the
alternation between consuming and non-consuming ticks. But its amplitude is **~2–3% of full
deflection** against events that peak at **100%**. The quiet-tracking tick-to-tick ripple median is
1e-4…1.6e-3. Mechanism A is **present and correctly identified in the brief, but it is not the
felt symptom** — it is ~50× under it. (It would also *track the pixel cadence*: median inter-mouse
interval 17 ms. The events do not — see 4.7.)

### 4.6 Mechanism C — deadzone latch/unlatch: cleared

| tape | ticks with err < lo (0.03°) | ticks with ≥ `rest_dwell` of hand-rest behind them |
|---|---|---|
| f2 | 172 (3.7%) | 19% |
| f3 | 634 (24.6%) | 36% |
| f4 | 613 (13.4%) | 24% |
| f5 | 447 (27.6%) | 50% |
| f6 | 139 (5.5%) | 27% |

Two independent kills. (1) **Amplitude**: the unlatched sub-`lo` pointing is bounded per axis at
`K_θ·yaw_scale·lo ≈ 0.35 °/s` — **70× below** the measured 25 °/s peaks. (2) **Gating**: the latch
needs 0.15 s of hand-rest and any aim-moved tick unlatches instantly, but the symptom is loudest
while the hand is *moving* (band 1–2.5° carries 29–41% mouse-moved ticks). The 125–150 ms event
cadence in §4.7 is a coincidental near-match to `rest_dwell` and must not be read as the deadzone.

### 4.7 Mechanism D — inner-loop / PIO ring: refuted

Behaviour inside ≥ 0.5 s hands-completely-off stretches:

| tape | stretches | full-scale events inside | vs overall rate | median \|Input.yaw\| inside |
|---|---|---|---|---|
| f2 | 8 (6.4 s) | 2 → 0.31 /s | 3.84 /s | 0.001 |
| f3 | 6 (7.7 s) | 3 → 0.39 /s | 3.39 /s | 0.000 |
| f4 | 6 (8.1 s) | 4 → 0.49 /s | 1.73 /s | 0.000 |
| f5 | 7 (6.8 s) | 4 → 0.59 /s | 1.48 /s | 0.001 |
| f6 | 3 (4.9 s) | 1 → 0.21 /s | 3.53 /s | 0.002 |

The rudder goes **quiet** with the hand still (median deflection ≈ 0.001 of full travel) and the
event rate drops 4–17×. There is no fixed-frequency line: the spectra are broad 3–16 Hz content
whose only sharp feature is the 60 Hz frame line. A PIO ring would be amplitude-independent of
pixel events and would persist. It does not.

**Honest residual:** the 0.2–0.6 /s that *do* fire hands-off were inspected individually. Eight of
nine are legitimate arrivals still in progress when the hand stopped (error 0.48–10.9° at the
event, closing to 0.02–0.05°). Exactly **one** (tape 5, t = 9.59 s) is a genuine hands-off
micro-bounce at an already-tiny error (0.08° → 0.02°, peak 0.63). So the post-arrest re-engage the
`[capture]` block warns about ("the AT-4 hunt") exists but is rare — it is *not* the flapping.

### 4.8 Cadence: the events do not follow the pixel cadence

| tape | near-centre rudder events | median inter-event interval | median inter-mouse-tick interval |
|---|---|---|---|
| f2 | 68 (1.76 /s) | 150 ms | 17 ms |
| f3 | 67 (3.11 /s) | 125 ms | 17 ms |
| f4 | 49 (1.28 /s) | 67 ms | 17 ms |
| f5 | 15 (1.11 /s) | 967 ms | 17 ms |
| f6 | 72 (3.39 /s) | 125 ms | 17 ms |

Events fire at roughly **1/8th** the pixel-event rate. Mechanism A predicts 1:1 with the pixel
cadence (or the frame cadence); it does not hold. The observed 125–150 ms is what it takes the hand
to re-grow the error past the refractory clear radius (the 0.55° circle) after an arrest — the
capture machine's own "one bounce per arrival, cleared when the aim leaves the reticle" contract.

### 4.9 A worked event (tape 6, ticks 424–435, t = 3.53 s)

Small steady left drag (1–2 px/frame), error growing through 0.9°, rudder sitting at a smooth
+0.14. Then:

```
tick   err   lat    Input.yaw   ω_yaw
423   0.92  -0.81     +0.135     6.2   quiet tracking
424   0.82  -0.69     +1.000    14.6   ENGAGE — rudder to the stop
425   0.69  -0.52     +1.000    21.2
426   0.64  -0.44     +1.000    26.4   full speed toward centre
427   0.52  -0.18     +1.000    30.5
428   0.50  -0.01     -0.333    20.8   crossing centre; brake fires
429   0.52  +0.10     -0.289    13.6
430   0.52  +0.13     -0.787     3.1   rim glance (0.13° past centre)
431   0.51  +0.09     -0.700    -4.3
432   0.49  +0.02     -0.503    -8.3
433   0.50  +0.03     +0.822     1.4   dead-blow home
434   0.49  -0.04     +0.762     8.5
435   0.49  +0.02     -0.027     6.4   arrest — back to quiet
```

Full stop → reverse to −0.79 → reverse to +0.82 → out, in **~95 ms**, for a **one-degree** error.
That single sequence is what "jumping and flapping" is.

---

## 5. Verdict per candidate mechanism

| # | mechanism | verdict | why |
|---|---|---|---|
| **B** | **`[capture]` pool-ball arrival machine** | **OWNS THE SYMPTOM** | Amplitude matches `sqrt(2·α·d)` to 0.70× and beats the pointing law by 6.7–7.9×; anatomy is approach / 1 reversal / apex inside the rim (p90 0.28° vs circle 0.55°) / 58 ms; rate peaks at 1–2.5° error; **vanishes above `blend_lo`=5°, exactly its ENGAGE gate**, which is also exactly where Chad says it goes smooth; pitch+yaw only (the pointing law's axes), aileron unaffected. |
| **A** | `[aim_ff]` per-pixel impulses | **PRESENT, SECONDARY (~2–3%)** | The 60 Hz frame line is real in the spectrum (a=0.031) and the pinned `aim_rate_ff` gives it directly; but the demand is 0.7–9.5 °/s p95 and the resulting quiet ripple is ~1e-3 of full travel. Also fails the cadence test (events run at 1/8 the pixel rate). Would be felt as a fine buzz, not a flap. |
| **C** | deadzone latch/unlatch | **CLEARED** | Bounded at ~0.35 °/s (70× under measurement); the latch cannot arm while the hand moves, which is when the symptom is loudest. The 125–150 ms cadence coincidence with `rest_dwell` is not causal (it tracks the refractory clear radius, not hand-rest). |
| **D** | inner-loop / PIO ring | **REFUTED** | Rudder goes quiet (median 0.001) in hands-off stretches; event rate drops 4–17×; no fixed-frequency line. One genuine hands-off micro-bounce in 34 s of hands-off tape. |

**Composition statement:** the felt symptom is ~97% mechanism **B** by amplitude, with **A** riding
on top as a small 60 Hz texture. C and D are not contributors.

---

## 6. Knife-edge appendix — the premature inversion

### 6.1 What is actually on the tape

Only `felt_flight_4` contains a roll past 90°. **Maximum nose elevation anywhere in the five tapes
is +37°** — there is no near-vertical zoom. The manoeuvre Chad calls "straight up then down" is,
on the pins, a **climb to +35° followed by a mouse-down flick**.

The aim frame was reconstructed (§7) and is **anchored 2.2 s before this event** (a freelook tap at
t = 20.83 s re-welds the aim to the nose; the reconstruction's error at that anchor was **0.05°**),
so the aim geometry below is trustworthy to well under a degree.

### 6.2 The event, with the push-gate legs evaluated tick by tick (tape 4)

```
   t      nose_elev   bank    aim vs WORLD horizon   side-cone   |bankErr|   Input.roll
 22.92s     +36.5°    +1.8      36.4° ABOVE            0.0°         178°        +0.05
 23.00s     +35.8°    +1.3      32.4° ABOVE            0.1°         180°        +0.04
 23.08s     +34.1°    -1.4      27.1° ABOVE            0.8°         172°        +1.00   <-- roll-over fires
 23.17s     +32.4°   -10.2      21.7° ABOVE            0.9°         165°        +1.00
 23.50s     +26.5°   -48.8       4.2° ABOVE            0.9°         129°        +1.00
 23.83s     +20.1°   -87.0      13.9° BELOW            1.2°          91°        +1.00
 24.00s     +13.0°  -105.9      19.0° BELOW            2.2°          70°        +1.00   (inverted, cosΦθ -0.27)
 24.42s      -7.1°  -150.3      26.7° BELOW            6.7°          10°        +0.42
 24.83s     -29.0°  -163.1      45.5° BELOW            3.9°           4°        +0.14   <-- horizon leg FINALLY passes
 25.42s     -57.9°  -160.7      69.0° BELOW            3.5°           2°        +0.08
```

**Push-gate leg audit at the roll-over onset (t = 23.08 s), against the as-flown table:**

| leg | requirement | measured | pass? |
|---|---|---|---|
| `blend > 0` | err > 5° | err = 7.1° | ✅ |
| `|bank_eff| > push_gate_bank` | > 120° | 172° | ✅ |
| `target_body.z ≤ push_down_z_enter` | down-angle ≤ 88° | 7.1° | ✅ |
| `aim_side < push_side_enter` | < 37.5° | **0.8°** | ✅ (by 36.7°) |
| horizon arm: `aim_elev < push_horizon_enter` | aim ≥ 45° **below** horizon | aim is **27.1° ABOVE** the horizon | ❌ |
| sacred-middle arm: `aim_side < 18° && aim_elev < 0` | aim **below** horizon at any depth | aim is **above** the horizon | ❌ (fails on `aim_elev < 0`, not on the cone) |

Every leg passes except the two that gate on **where the aim sits relative to the WORLD horizon.**
`push_mode` therefore cannot arm, `|bankErr| ≈ 172–180°` (the aim is almost exactly *below the
nose*), the roll latch engages past `roll_on = 135°`, and bank-to-turn does the only thing it can:
**full aileron, roll 180°.** The plane goes inverted and then pulls, which is the felt "premature
inversion". Push *still* never arms afterwards — by the time the aim finally reaches 45° below the
horizon (t = 24.83 s) `|bankErr|` has collapsed to 4°, failing the `> 120°` leg.

### 6.3 The estimated off-axis angle at which the roll-over fired

**0.8°.** Not 27.5°, not 37.5° — the aim was essentially *dead in the vertical plane*. The
side-cone was never the discriminator on this event.

The same signature repeats across the tapes (this is where Chad's "some partial banks" live):

| tape | t | nose elev | aim vs horizon | **side-cone** | \|bankErr\| | bank reached |
|---|---|---|---|---|---|---|
| f4 | 2.98 s | +30.1° | 23.9° **above** | **0.2°** | 177° | 52° (partial) |
| f4 | 12.05 s | +16.2° | 7.9° **above** | **1.3°** | 171° | 25° (partial) |
| f4 | 14.47 s | +12.6° | 6.7° **above** | **0.5°** | 172° | 17° (partial) |
| f4 | **23.05 s** | **+34.8°** | **28.9° above** | **0.8°** | 173° | **156° — INVERTED** |
| f4 | 31.87 s | +17.2° | 10.9° **above** | **0.1°** | 178° | 22° (partial) |
| f3 | 15.91 s | +18.0° | 16.1° **above** | **0.9°** | 180° | 81° (partial) |
| f2 | 14.70 s | +3.9° | 3.9° **above** | 10.2° | 178° | 92° (partial) |

**One full inversion, six partial banks — matching Chad's "1–2 premature inversions and some
partial banks" — and the side-cone angle is under 1.3° in six of the seven.**

### 6.4 The knife-edge finding, stated plainly

> Whenever you are **climbing** and flick the mouse **down**, the aim lands *below your nose* but
> still *above the world horizon*. In that window neither push arm can arm — the wide arm needs
> the aim **45° below the horizon**, and the sacred-middle arm needs it below the horizon **at
> all** — so the bank-to-turn law owns a 180° bank error and rolls you over. The side-cone had
> nothing to do with it: you were within a degree of dead-centre in your own vertical plane every
> time.
>
> **Corollary: widening `side_cone_enter` (the queued 27.5 → 35 rung) cannot touch this, and the
> flown build already ran 37.5.** The leg that fired is `horizon_enter` / the sacred-middle arm's
> `aim_elev < 0` condition. This is the same class as the note already in the table — rung E's
> `horizon_enter = 45` "blankets everything below the horizon" — except the failure here happens
> *above* the horizon, which rung F's `aim_elev < 0` does not reach.

### 6.5 Was the aim recoverable? Yes — and what the recorder still needs

The brief allowed that the aim might be unrecoverable. It is **not**: `app::tick`'s aim path is a
pure function of recorded inputs and recorded states, so it replays exactly (§7). Validated on two
tapes against the freelook nose-weld anchor: **0.05° residual after 2500 ticks (tape 4)** and
**0.41° after 970 ticks (tape 3)**.

What is genuinely **not** derivable, and should be pinned in a future recording:

1. **`aim.q` (4 doubles)** — kills the replay dependency entirely and makes tapes readable without
   the exact caller-side source at that commit. *Highest value per byte.*
2. **`control::Output.telem`** — specifically `omega_des[3]`, `push_mode`, the regime `blend`, and
   the `[capture]` machine's **state / phase / engage-and-exit flags**. Today the capture machine's
   ENGAGE is *inferred* from the amplitude law (§4.4), not *measured*. This is the single field
   that would turn §5's verdict from a strong inference into a direct observation.
3. **`control::Output.inputs[3]`** — the emitted deflections. I recovered them exactly by inverting
   the plant (max |derived| = 1.000000000, hitting the clamp exactly), but that inversion is only
   valid while the plant law is unchanged; a pinned copy is a tamper-proof cross-check.
4. **The resolved `ControllerParams` (or a hash of `controller.toml`) in the tape header.** I had
   to fetch the table out of git and discovered the brief's premise was two knobs stale. A tape
   should be self-describing about what it was flown on.
5. **`deadzone latched`** (one bit) — would have closed mechanism C by observation rather than by
   amplitude bound.

The recorder is `test/harness/recorder.h`; all five are additive POD fields on `TickRecord`
(`serialize_body` / `read_records` extend by appending, and the fnv1a signature keeps it
tamper-loud).

---

## 7. Method, and what would falsify this

### 7.1 What was derived, and how

**Rule 1 was honoured: `control::step` was never executed.** Two derivations only:

1. **Commanded deflections** — algebraic inversion of the *plant* law in `sim/step.cpp`:
   `Input = (I·(ω' − ω)/dt + damp·Q·ω) / (c·Q·δ_max_eff(V))`, with `Q`, `δ` and `ω` all taken from
   the *pre-tick* pin. **Self-validating:** across all 15 959 ticks the maximum derived magnitude is
   **exactly 1.000000000** and *zero* ticks exceed it — the derived signal lands on the plant's own
   `clamp(±1)` to the last bit. A wrong inversion cannot do that.
2. **The aim frame** — replay of `input/aim_frame.h` + `input/aim_state.h` + `control/transport.h`
   driven only by recorded `aim_dx/dy`, `aim_gain_scale`, `freelook`, `orient`, `override_mask` and
   the recorded position/orientation pins. No controller state is involved. Validated against the
   freelook nose-weld anchor (§6.5).

**Known limits, stated:** the tapes start mid-flight, so the aim frame is seeded `aim := nose` at
tick 0 and carries an unknown constant seed offset until the first freelook weld. `felt_flight_2`
and `felt_flight_6` contain **no** freelook and are therefore never anchored — their absolute
pointing errors carry that seed offset. This does **not** affect §4's conclusions, which are all
*relative* (event rates, amplitude ratios, band contrasts, hands-off contrasts), and the two
anchored long tapes bound the seed drift at ≤0.4° over 8 s. §6 uses only tape 4, anchored 2.2 s
before the event.

Scripts live in the session scratchpad (`.../scratchpad/jitter/`: `seadsrec.py`, `ident.py`,
`analyze.py`, `events.py`, `mech.py`, `knife.py`, `knifewin.py`, `rollscan.py`, `flap.py`,
`duty.py`, `hands_off.py`). Nothing in the repo was modified.

### 7.2 The falsification test — jitter verdict

**Cheapest decisive experiment: one A/B fly with `[capture] carry = 0.0`** (the documented
structural OFF — "the machine never engages, every expression tree bit-identical legacy"), same
tracking task, recorded.

Predictions if mechanism **B** owns the symptom:

- FINE (err<5°) rudder **flap rate falls from 6.2 /s to well under 1 /s**; elevator from 3.3 to
  under 0.5 /s.
- Ticks at ±100% rudder in FINE fall from **10.3% to ≲1%**.
- Peak yaw rate at ~0.5° error falls from ~25–45 °/s to the pointing law's **~3–6 °/s**, i.e. the
  `measured ÷ sqrt(2αd)` median drops from 0.70 toward ~0.1.
- Aileron flap rates are unchanged in both bands (the machine never touched roll).
- The 60 Hz spectral line **survives** at the same amplitude (that is mechanism A, untouched).

**If the flapping survives `carry = 0`, this attribution is wrong.** Next suspect in that case is
mechanism A — set `[aim_ff] gain = 0.0` (bit-identical legacy) and re-record; if it survives *that*
too, the amplitude bounds in §4.6/§4.7 are violated and the fault is somewhere I have not modelled.

A pure-offline falsifier also exists and is cheaper still: **land pin #2 from §6.5** (the capture
state/phase in the telemetry) and re-record one 30 s tracking tape. If the ENGAGE flag is *not*
raised on the ticks where the rudder slams, mechanism B is refuted with no fly at all.

### 7.3 The falsification test — knife-edge verdict

Re-fly a climb-to-~35°-then-flick-down with the recorder pinning `push_mode` (§6.5 pin #2).
Prediction: `push_mode` reads **false** at the roll-over onset, with `aim_elev > 0` (aim above the
horizon) as the only failing leg while `aim_side < 2°`. If `push_mode` is already **true** at the
onset, or if `aim_side` at the onset is anywhere near 27–38°, the §6 attribution is wrong and the
side-cone is back in play.

---

## 8. What this means for the dial ladder

**Dials are Chad's. This section names owners and pre-registers couplings only — it recommends no
values.**

1. **The near-centre rudder/elevator flapping is owned by `[capture]`** — the pool-ball arrival
   machine, at arrival errors far below the drawn reticle circle. It is not a gain problem, not a
   deadzone problem, and not an inner-loop stability problem; those three are cleared by
   measurement in §5 and should not be touched for this symptom. The mechanism's own documented
   structural off-switch is `carry = 0.0`, which is also the A/B arm the falsification test needs.
2. **The behaviour is *by design* per the v3 POOL BALL rulings** ("any deflection at all… Always,
   even mid-track", "no size threshold, no park gate"). What the tape adds is the *density*: at
   1–2.5° of error the machine fires **4.8 times a second**, and 112 of 356 events fire with the
   aim already inside the reticle ring. Any change here is a **re-ruling of universality at small
   arrival scale**, not a tune — it needs Chad's words before anyone writes code.
3. **`[aim_ff]` is a real but sub-perceptual second-order contributor** (~2–3% of full deflection,
   a 60 Hz frame line). If the flapping is fixed and a fine buzz remains, this is the next owner —
   in that order, one dial at a time, never both.
4. **The knife-edge inversion is owned by `[push_gate]`'s horizon legs** — `horizon_enter` and the
   sacred-middle arm's `aim_elev < 0` condition. **It is NOT owned by `side_cone_enter`**, and the
   queued 27.5 → 35 rung is inert against it (the flown build already ran 37.5, and the measured
   off-axis angle at every rollover onset was 0.1–1.3°). Anyone reaching for the cone dial to fix
   Chad's inversion will move a dial that cannot reach the symptom and will move it in a tree whose
   `push_gate` is already two rungs behind the flown one.
5. **Pre-registered coupling on any horizon-leg change:** rung E's `horizon_enter = 45` exists
   because at 1° a long lateral drag whose aim dipped a hair below the horizon became push-eligible
   and produced Chad's "repeated dive". Any relaxation of the above-horizon case must not re-open
   that: the discriminating quantity in the tapes is `|bankErr| ≈ 172–180°` (aim almost exactly
   *below the nose*, i.e. a genuine pull-through) versus rung E's lateral case, and that
   near-astern-below signature is *not* currently a leg of the gate.
6. **Tree hygiene, flagged for whoever picks this up:** `seads-feel` and `seads-recon@HEAD` have
   diverged in both directions (§3). Nothing here should be planned against the `seads-feel`
   `controller.toml` as if it were the flown table.

---
---

# Addendum: `felt_flight_12–14` — the 5–10° "knife-edge boundary" bounce

**Date:** 2026-07-30 (same session, follow-up) · offline, read-only, no commits
**Tapes:** `D:\flight_sim2\seads-recon\build-play\felt_flight_{12,13,14}.seadsrec`, flown
12:03–12:04, same kernel tag (`…g61753affd kernel=flight-kernel-v9-2026-07-29`).
**Same pinned rules:** derived from the per-tick pins only; the controller was never re-run;
content identified from the data.

**Chad's report under test:** at 5–10° of deflection, *already deflected and adding a little more
slowly*, the cascade "bounces… on a knife edge boundary reconciling the nose to the mouse aim
circle."
**Hypothesis under test:** the lean-cap saturation shelf (at `lean_gain 8` the lean target
flat-tops from `|az| = lean_max/lean_gain = 3.75°`) meeting the regime blend band
(`blend_lo 5` / `blend_hi 9`).

---

## A0. Which table these tapes were actually flown on — read this first

The brief pointed at `seads-feel@HEAD`. That is **not** the table `build-play\seads.exe` loads;
it loads `D:\flight_sim2\seads-recon\config\controller.toml`, whose working copy carries **two**
uncommitted edits — `lean_gain 6 → 8` **and** `carry 1.0 → 0.0` — and whose **mtime is 12:17:38**,
i.e. **13 minutes after these tapes were recorded.** So the file's state at 12:03 had to be
established from the data, not from the file.

**Established from the pins: `carry` was NOT 0 on these tapes.** The capture machine's signature is
fully intact:

| capture-presence metric (FINE, err < 5°) | old tapes (lean 6) | **new tapes 12–14** |
|---|---|---|
| rudder \|Input\| > 0.55 | 22.0% of ticks | **24.7%** |
| rudder saturated at ±100% | 10.3% of ticks | **11.3%** |
| near-centre peak rate ÷ plain pointing law | 6.71× | **8.19×** |
| near-centre peak rate ÷ `sqrt(2·α·d)` (the dead-blow law) | 0.70 | **0.79** |

Had `carry = 0` been live, §7.2 of the main report predicted saturation collapsing to ≲1% and the
`sqrt(2αd)` ratio dropping toward ~0.1. It did neither. **These are the lean arm ("test-flip 1"),
NOT the `carry = 0` falsification arm.** The falsification test from §7.2 therefore remains
**unrun** — a tape recorded after 12:17 will carry it.

`lean_gain = 8` is taken on the coordinator's word plus the loop-log commit; the tape format carries
no table pin (§6.5 pin #4 again). The data is *consistent* with it (§A4 Test 1) but does not prove
it. The two candidate tables also differ in `side_cone_enter` (27.5 in `seads-feel`, 37.5 in
`seads-recon`) and in whether the sacred-middle arm exists — **nothing in these tapes discriminates
that**, because every rollover onset measured sits at ~2° off-plane, far inside both (§A7).

---

## A1. What each new tape contains (from its own data)

| tape | ticks / s | signature countables | content |
|---|---|---|---|
| `felt_flight_12` | 2892 / 24.1 s | 0 freelook, 0 keys, 0 zoom, V 245–270, alt 801–1142, nose elev **−5…−1°** (flat), \|bank\| to 52°, **0 inverted**, mouse 19% (median 2 px) | **The clean slow-add tape.** Level, gentle, repeated shallow lateral adds. Best seed fit of the three (p10 err 0.007°). |
| `felt_flight_13` | 4280 / 35.7 s | 0 freelook, 0 keys, V 249–271, alt 776–1063, nose elev −8…+5°, \|bank\| to **81°**, 0 inverted, mouse 31% (median 2.8 px, max 42) | **The main 5–10° material.** Sustained banked laterals with slow adds punctuated by ~12 px flicks. 5 of the 11 qualifying slow-add segments. |
| `felt_flight_14` | 1004 / **8.4 s** | 0 freelook, 0 keys, nose elev **−55…+36°**, \|bank\| to **165°**, **344 inverted ticks** | **A short knife-edge repeat** — one climb→flick-down→roll-to-inverted, same signature as §6 (see A7). |

None has a freelook tap, so none is anchored. The record-start aim offset was therefore **fitted**
(2 dof, the aim frame's own up/right axes) by minimising the 10th-percentile pointing error — the
closed loop parks at the deadzone circle whenever the hand settles, so the true seed is the one
whose quiet floor lands there. **Method validated against the two anchored tapes, where the true
seed is known to be ~0:** it recovered **0.009°** (flight_3) and **0.078°** (flight_4). Fitted
seeds here: flight_12 (+0.00, +0.04)°, flight_13 (+0.28, +0.20)°, flight_14 (+0.08, −0.26)°;
fitted p10 residuals 0.007 / 0.439 / 0.315°. **Absolute error accuracy ≈ ±0.1° on flight_12,
≈ ±0.4° on 13/14** (those two rarely settle). All conclusions below rest on *oscillation
amplitudes and crossing counts*, which are seed-independent.

---

## A2. Flap-density table — the lean-8 arm of the record

Same definition as §4.1 (full sign reversal of the derived commanded deflection, both lobes > 0.30).

| regime | time | rudder | elevator | aileron |
|---|---|---|---|---|
| **lean 8, FINE err < 5°** | 56.6 s | **5.12 /s** | **2.58 /s** | 0.57 /s |
| **lean 8, MANEUVER err ≥ 5°** | 11.5 s | 0.35 /s | 0.26 /s | 0.61 /s |
| *(lean 6, FINE)* | *103.8 s* | *6.24 /s* | *3.27 /s* | *0.28 /s* |
| *(lean 6, MANEUVER)* | *29.2 s* | *0.24 /s* | *0.17 /s* | *0.89 /s* |

The main report's near-centre finding **reproduces unchanged on the new table**: rudder and elevator
flap 15× and 10× denser below 5° than above; the aileron does not. `lean_gain 8` did not move it
(as expected — the capture machine owns it, and `lean_gain` is a roll-channel knob).

---

## A3. The slow-add segments — the material Chad is describing

**Definition (from the pins):** ≥ 0.4 s continuous with aim-to-nose error in **3.5–11°**, and the
hand moving with a **median per-frame delta ≤ 6 px** — i.e. *already deflected, adding a little
more, slowly*. 11 such segments, 7.8 s, across the three tapes.

| tape | window | err min/max | **5°-crossings** | \|az\| range | % past the 3.75° shelf | rudder / elev / **aileron** flaps | bank min/max | \|Input.roll\| p95 |
|---|---|---|---|---|---|---|---|---|
| 12 | 8.10–8.61 | 3.51 / 8.51 | 2 (3.93/s) | −8.45…−3.13 | 92% | 0 / 0 / 0 | −29.5 / −2.0 | **1.00** |
| 12 | 12.70–13.19 | 3.51 / 8.78 | 4 (8.14/s) | −8.69…−3.43 | 92% | 0 / 0 / 1 | −25.8 / −1.3 | **1.00** |
| 12 | 22.10–22.85 | 3.62 / 10.10 | 4 (5.33/s) | −10.06…−3.42 | 97% | 0 / 0 / 1 | −51.8 / +2.2 | **1.00** |
| 13 | 11.66–12.18 | 3.58 / 10.92 | 3 (5.81/s) | −10.89…−3.43 | 94% | 0 / 0 / 2 | −56.8 / −31.6 | **1.00** |
| 13 | **13.41–14.08** | 4.23 / 10.69 | 2 (2.96/s) | −10.58…−3.89 | 100% | 0 / 0 / **4** | −72.3 / −55.7 | **1.00** |
| 13 | 15.09–16.16 | 3.68 / 10.89 | 3 (2.81/s) | +3.57…+10.51 | 99% | 0 / 0 / 2 | −74.1 / −4.9 | **1.00** |
| 13 | 21.82–22.27 | 3.51 / 6.02 | 2 (4.44/s) | +2.67…+5.50 | 31% | 0 / 0 / 0 | +10.9 / +23.9 | 0.35 |
| 13 | 27.95–28.77 | 3.50 / 4.82 | 0 (0.00/s) | +3.22…+4.72 | 78% | 0 / 0 / 0 | +4.1 / +28.5 | 0.49 |
| 14 | 1.23–1.65 | 3.53 / 5.46 | 4 (9.60/s) | +1.34…+2.02 | 0% | 0 / 0 / 0 | −0.8 / +0.2 | 0.06 |
| 14 | 2.18–3.73 | 3.62 / 10.92 | 7 (4.52/s) | +1.18…+3.53 | 0% | 0 / 0 / 0 | −19.5 / +3.2 | **1.00** |
| 14 | 5.54–6.10 | 4.97 / 10.87 | 4 (7.16/s) | −4.96…−0.84 | 33% | 0 / 0 / 0 | −164.6 / −161.0 | 0.12 |
| **pooled** | **7.8 s** | | **35 (4.48/s)** | | | **0 / 0 / 10 (1.28/s)** | | |

### The worked trace (flight_13, t = 13.41–14.06 s) — this *is* the bounce

```
   t     err   az     regime | Input.roll   w_roll    bank
 13.41  10.65 -10.54  MANEU  |   +0.92     +112 deg/s  -56.2
 13.48   9.52  -9.43  MANEU  |   +0.62      +83        -62.9   rolling INTO the turn
 13.53   7.68  -7.60  BLEND  |   +0.02      +48        -66.1
 13.56   6.95  -6.85  BLEND  |   -0.77      -10        -66.3   <-- aileron reverses
 13.58   6.33  -6.19  BLEND  |   -1.00      -65        -65.0   FULL OPPOSITE AILERON
 13.63   5.34  -5.07  BLEND  |   -1.00     -103        -60.3   rolling BACK OUT
 13.66   4.91  -4.60  FINE   |   +0.29      -46        -58.4   <-- crosses 5 deg, hands back
 13.71   4.23  -3.89  FINE   |   -0.34      -30        -56.9
 13.73   5.74  -5.53  BLEND  |   -0.01      -28        -56.0   (12 px mouse add)
 13.76   8.58  -8.44  BLEND  |   +1.00      +27        -55.9   FULL AILERON THE OTHER WAY
 13.83   7.50  -7.39  BLEND  |   +0.50      +70        -61.2
 13.93   6.66  -6.43  BLEND  |   -0.14       -1        -64.0   <-- reverses again
 13.98   8.91  -8.74  BLEND  |   +1.00      +65        -65.2   (12 px add) FULL AILERON again
```

Full-scale aileron **reversals** at ~2 Hz, ±100 °/s of roll rate, ~10° of bank swung back and
forth — while the pilot adds 0–12 px at a time. Rudder and elevator do **not** reverse anywhere in
this window. That is the felt "bounce on a knife-edge boundary."

---

## A4. Is the *lean shelf* the boundary? Two tests

### Test 1 — does full aileron switch on at each table's OWN shelf?

If the shelf is causal, the knee must move with `lean_max/lean_gain`: **5.00°** of `az` at gain 6,
**3.75°** at gain 8. Fraction of ticks with the aileron at ±100%, binned by `|az|`, conditioned on
err ≤ 11° and |bank| ≤ 60° (so the two arms are compared at matched geometry):

| \|az\| bin | **lean 6** (shelf 5.00) | **lean 8** (shelf 3.75) |
|---|---|---|
| 0 – 1° | 2.5% (10423 ticks) | 0.0% (4179) |
| 1 – 2° | 3.3% (1178) | 2.3% (1000) |
| 2 – 3° | 1.7% (713) | 0.7% (845) |
| 3 – 3.75° | 6.6% (363) | 1.0% (515) |
| **3.75 – 5°** | 14.1% (326) | **3.3%** (364) ← *the new shelf region is the QUIETEST part of the sweep* |
| **5 – 6°** | **19.4%** (175) | **31.2%** (125) ← **the knee, both tables** |
| 6 – 8° | 56.7% (201) | 65.4% (133) |
| 8 – 11° | 90.8% (141) | 93.5% (201) |

**The knee sits at ≈ 5° in BOTH tables.** It did not move to 3.75° when the shelf did. Worse for the
hypothesis: at gain 8 the flat-top region 3.75–5° is *four times quieter* than the same band at
gain 6 (3.3% vs 14.1%) — the shelf does not produce full aileron, it *suppresses* it (the plane is
already sitting at the leaned bank, so there is nothing to catch up).

5° is `blend_lo`. It is not `lean_max/lean_gain` at either gain.

### Test 2 — where in the error range do the strong aileron reversals land?

Reversals with both lobes > 0.5, binned by the error at the reversal tick:

| | 0–1 | 1–2 | 2–3 | 3–4 | **4–5** | 5–6 | 6–7 | 7–8 | 8–9 | 9–10 | median err |
|---|---|---|---|---|---|---|---|---|---|---|---|
| lean 6 (n=46) | 5 | 1 | 3 | 2 | **16** | 2 | 4 | 8 | 1 | 4 | **4.77°** |
| lean 8 (n=21) | 1 | 1 | 2 | 2 | **11** | 3 | 1 | 0 | 0 | 0 | **4.47°** |

A clean pile-up in the **4–5° bin** on both tables — the reversals are locked to `blend_lo`, and
they existed at gain 6 too.

### What gain 8 *did* change

At matched `|az|` (Test 1, so this is not a "he flew harder" confound), gain 8 saturates the
aileron **more** exactly where the handover happens (5–6° bin: 19.4% → **31.2%**), and in the
slow-add segments as a whole:

| slow-add metric | lean 6 | **lean 8** |
|---|---|---|
| qualifying material | 21.2 s / 15 segments | 7.8 s / 11 segments |
| **5°-crossings** | 2.22 /s | **4.48 /s** |
| aileron flaps | 0.85 /s | **1.28 /s** |
| **aileron at ±100%** | 5.3% of ticks | **27.2%** of ticks |
| median segment peak \|bank\| | 21° | 29° |

(These five are *not* a controlled A/B — different hands, different tasks, 7.8 s vs 21.2 s. Test 1
is the confound-resistant one.)

---

## A5. Answers

**(a) Does the error oscillate ACROSS ~5°, or stay within the band?**
**It oscillates ACROSS it, hard.** 35 crossings of the 5° line in 7.8 s of slow-add material =
**4.48 crossings/s** (lean 6: 2.22/s). Typical segment swings 3.5 → 8.8° and back inside 0.5 s.
This is boundary hunting, not band residence — and it happens on *both* tables.

**(b) Is the bounce the roll channel or pitch/yaw?**
**Unambiguously the ROLL channel.** In 7.8 s of slow-add: **rudder flaps 0.00/s, elevator flaps
0.00/s, aileron flaps 1.28/s**, with the aileron at **full deflection 27.2%** of ticks, ±100 °/s
roll-rate reversals, and the strong reversals piling up in the 4–5° error bin. Pitch and yaw are
single-sided there. This does support "held-bank stepping" — but the stepping is between the
**FINE roll-hold target and the MANEUVER bank-to-turn target**, not between lean-shelf levels
(§A4).

**(c) Does the capture machine fire above `blend_lo`?**
**No — the ENGAGE gate holds.** The slow-add segments contain 9 (lean 8) / 13 (lean 6) runs of
\|Input.yaw\| or \|Input.pitch\| > 0.55 whose onset error is above 5°, but **zero** of them carry
the capture anatomy: zero rudder or elevator sign reversals in the whole 7.8 s, no rim glance, no
dead-blow return. They are single-lobe pointing/coordination demands raised by the pilot's own
12-px add and then decayed. Confirmed gated off above `blend_lo`, exactly as documented.

---

## A6. Verdict on the shelf hypothesis: **REFUTED (as the cause) — the boundary is `blend_lo` itself**

The bounce is real, it is the roll channel, and it is a genuine regime-boundary hunt. But it is
**not** the lean-cap shelf meeting the band:

- the full-aileron knee sits at **5°** on both tables and did **not** move to 3.75° when the shelf
  did (Test 1);
- the new 3.75–5° flat-top region is the **quietest** part of the `az` sweep at gain 8 (3.3% vs
  14.1% at gain 6) — the opposite of what the hypothesis predicts;
- the strong reversals pile up at err ≈ 4.5–4.8° on **both** tables (Test 2), i.e. the hunt
  pre-dates `lean_gain 8`.

**What the tapes show instead** (mechanism, stated so it can be red-teamed): at `blend_lo` the roll
channel is handed between two targets that *disagree by tens of degrees of bank*. Below 5° the
roll-hold chases `held_bank`, whose lean target is `clamp(lean_gain·az, ±lean_max)` — **capped at
30°**, and (per `controller.cpp`) the lean update itself only runs while `err < blend_lo`, so it
freezes the moment the error crosses. Above 5° the bank-to-turn term blends in, and for a 5–10°
*lateral* aim bank-to-turn wants **60–90°** of bank (the documented `bank_error` saturation). In the
worked trace the aeroplane sits at −56…−72° of bank while the FINE target is pinned at **−30°
flat** — so each dip under 5° commands full aileron *out* of the turn and each rise over 5°
commands full aileron *back in*. `blend` is 0.000 at err 5.0 and only 0.156 at err 6.0, so the
handover is steep exactly where the error is hunting.

`lean_gain 8` **aggravates the amplitude** (matched-`az` aileron saturation at the handover
19.4% → 31.2%; slow-add crossings 2.22 → 4.48/s; segment peak bank 21° → 29°) by steepening the
sub-cap lean loop, so the roll channel arrives at the boundary with more bank and more rate. It did
not create the boundary.

**Bearing on a `lean_max` rung (precondition answer, no values recommended):** `lean_max` is *one of
the two* quantities that set the size of the target disagreement the boundary hands back and forth
— the other is what bank-to-turn asks for at a 5–10° lateral aim, which is not a `lean_*` knob at
all. So a `lean_max` rung **can** reach this symptom, but the hypothesis that motivated it (the
3.75–5° shelf) is refuted, and the ruling should be re-framed around the **FINE-vs-MANEUVER roll
target gap at `blend_lo`**, not around the shelf. Two further couplings are visible in the tapes and
should be pre-registered before anything moves: (1) the lean update is *itself* gated `err <
blend_lo`, so the FINE target is frozen for the entire time the error sits above the boundary — any
fix that only changes the cap leaves that freeze in place; (2) `blend` reaches only 0.156 by err 6°,
so the first degree above the boundary is still ~85% roll-hold, which is why the reversal is
full-scale rather than a blend.

## A7. Knife-edge: independent replication on the new table

`felt_flight_14`, t = 3.48–4.98 s, reproduces §6 exactly:

| | nose elev | aim vs world horizon | **side-cone** | \|bankErr\| | result |
|---|---|---|---|---|---|
| f14 @ 3.48 s | **+28.1°** | **22.2° ABOVE** | **2.2°** | 157° | rolls to **157° — inverted** |
| f14 @ 6.55 s | −43.7° | at the horizon | 11.8° | 180° | the recovery roll back (163°) |

Climbing, mouse flicked down, aim below the nose but still **above the world horizon**, off-plane
angle **2.2°** — neither push arm can arm, bank-to-turn owns a 157° bank error, full aileron,
inverted. **Third independent instance of the §6.4 finding, and again the side-cone is nowhere near
binding** (2.2° against a 27.5° or 37.5° threshold — the tape does not even discriminate which of
the two candidate tables was live, because both pass by an order of magnitude).

## A8. What would falsify the A6 verdict

Re-record slow-add 5–10° material with `[regime] blend_lo` moved (say to 3° or 7° — one dial, one
fly). **Prediction:** the aileron-saturation knee in the Test-1 `|az|` table and the reversal
pile-up in the Test-2 error histogram **both move with `blend_lo`**, while `lean_max/lean_gain`
stays put. If the knee instead stays at 5° with `blend_lo` moved, A6 is wrong. Conversely, moving
`lean_max` alone should shift the *amplitude* of the reversals (the target gap) without moving the
knee — if it moves the knee, the shelf is back in play.

The cheap offline version is the same as before: land §6.5 pin #2 (`telem` → `blend`, `held_bank`,
and the emitted `omega_des`) and one 30 s tape settles it with no fly at all — `held_bank` would
make the target gap directly *visible* instead of inferred from bank and aileron.
