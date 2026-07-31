# MB-aim — mouse sensitivity CURVE (aim resolution) — DESIGN + red-team ledger

*Status: PLAN RED-TEAMED (fresh-context Fable 5, one scoped round, 2026-07-08 —
**SOUND-WITH-FIXES**, all P0/P1 + cheap P2s folded, §7 ledger below) and IMPLEMENTED
(input/aim_curve.h + test_aim_curve.cpp, gate 230/230).
Chad's felt intent (2026-07-08, ruled FIRST priority): "fine at small hand movements
(tracking, the nested-reticle work), accelerating gain for big sweeps (flicks) — higher
effective aim resolution where it matters." Replaces the single linear `aim_sensitivity`
ceiling (docs/TEACHING.md §"The mouse has a ceiling").*

## 0. Legality rails (from the plan row — restated as the design's walls)

- **Memoryless pure gain shape.** No state, no filtering, no history — §9.1's ban is on
  smoothing, not shaping. A pure function of THIS frame's device sample.
- **Keyed on the frame-rate-NORMALIZED mouse rate** (pixels/second = delta ÷ frame_dt),
  NEVER the raw per-frame delta magnitude — a per-frame-delta curve makes the same physical
  hand motion rotate differently at 30 vs 240 fps (the S7-mouselevel frame-quantization class).
- **Knob-off arm = the pure linear gain, bit-identical** (strict superset, the S7/S9 proof shape).
- Nothing smoothed/slewed on mouse→aim→error→ω_des→Inputs (§9.1). The curve is a scalar
  magnitude on the raw delta — the S9-zoom `aim_gain_scale` class ("a scalar magnitude on the
  raw delta is not a basis smoothing"), except keyed on rate, which is exactly why the rate
  must be frame-normalized.

## 1. Where the curve lives — the DEVICE ACCRUAL boundary (the one place both inputs exist)

Today (main.cpp, instructor branch, per frame):

```
live = input::poll_live(live_dev, clamped_dt);
pending_dx += live.mouse_dx;
pending_dy += live.mouse_dy;
```

`pending_*` is consumed ONCE on the first tick of the next tick-bearing frame
(`app::step_frame`), applied to the aim as `apply_mouse(dx, dy, aim_sensitivity ·
aim_gain_scale)`. The per-frame delta and its frame_dt are BOTH in scope only at the accrual
site — downstream (`pending` at consume time) a carried delta may span several frames
(the 0-tick carry), so a consume-time curve would window the rate wrongly and re-open the
frame-rate dependence. Therefore:

```
const glm::dvec2 curved = input::aim_curve(live.mouse_dx, live.mouse_dy,
                                           clamped_dt, cparams);
pending_dx += curved.x;
pending_dy += curved.y;
```

with the pure function (new `input/aim_curve.h`, unit-tested; main.cpp's call is caller glue,
the documented residual class):

```
// rate-keyed acceleration gain, memoryless (§9.1-legal: shaping, not smoothing)
dvec2 aim_curve(double dx, double dy, double frame_dt, const CurveParams& p) {
    const double mag = std::sqrt(dx*dx + dy*dy);
    if (mag == 0.0) return {0.0, 0.0};                  // no motion: exact no-op
    const double rate = mag / std::max(frame_dt, kDtFloor);   // px/s, VECTOR rate
    double g = 1.0;
    if (p.gain_max > 1.0 && rate > p.knee) {
        const double t = std::min((rate - p.knee) / (p.rate_hi - p.knee), 1.0);
        g = 1.0 + (p.gain_max - 1.0) * std::pow(t, p.expo);
    }
    return {dx * g, dy * g};
}
```

Design points:
- **Vector magnitude** (not per-axis): a diagonal flick curves uniformly — per-axis curves
  would bend the delta's DIRECTION (a fast horizontal + slow vertical component would gain
  differently and rotate the resultant), which is a direction distortion on the raw path.
  A shared scalar gain preserves direction exactly.
- **Below the knee the gain is EXACTLY 1** (branch untaken): the precision zone is today's
  pure linear `aim_sensitivity`, bit-identical — small-hand-movement tracking is untouched.
- **`gain_max = 1.0` is the knob-off arm**: the branch is structurally skipped for every
  rate → returns `(dx·1.0, dy·1.0)` == `(dx, dy)` bit-exactly (IEEE `x·1.0 == x`). Strict
  superset.
- **`kDtFloor` = 1e-4 s** guards the ÷frame_dt (the vMin-floor discipline; a 0-dt frame
  cannot manufacture an infinite rate). main.cpp already clamps frame_dt (`clamped_dt`), the
  floor is the in-function guard so the PURE function is total.
- Continuous everywhere (no latch — a fade, not a gate; the hysteresis rule targets discrete
  switches). Monotone in rate; capped at `gain_max` for rate ≥ `rate_hi` (a wild flick cannot
  spin the aim unboundedly).

## 2. What it deliberately touches / does not touch

- **Freelook orbit deltas are curved too** (pending is shared; the orbit consumes
  `consumed_*`). Deliberate: one hand feel everywhere. Documented, cosmetic path.
- **RMB-zoom `aim_gain_scale` composes downstream unchanged** (per tick, binary button
  function) — zoomed precision still halves whatever the curve emits. Orthogonal knobs.
- **Raw mode (F1)**: untouched — poll_raw path never accrues pending.
- **Controller/sim/goldens**: untouched. The curve is upstream of the aim frame; control::step
  sees only `aim.forward()`. No golden can move (same argument as S9-zoom's RA9-safety note).
- **`aim_sensitivity` stays 0.15** — the below-knee mapping today == yesterday exactly, so the
  flight-log row attributes ONE change (the acceleration). Chad may later drop the base for
  finer tracking; that's a separate dial (noted in the row).

## 3. AT-9 reconciliation — THE design risk, addressed head-on

AT-9's guarantee (test_at9.cpp banner): identical TICK-INDEXED input ⇒ bit-identical
trajectory. Its scripted flicks are injected DIRECTLY into `pending_*` at common tick
boundaries. The curve sits UPSTREAM of `pending_*`, so:

1. **AT-9 as written is untouched and still pins what it pins** — the tick loop is
   frame-rate independent given delivered deltas. The test's injection is now "post-curve"
   by definition; its banner gains one sentence saying so.
2. **The curve's OWN frame-rate invariance gets a dedicated unit leg** (the decomposed
   guarantee): the same physical hand motion — constant rate R px/s sampled at frame_dt
   2⁻⁶ s vs 2⁻⁸ s (exact powers of two; per-frame delta = R·frame_dt exact, rate division
   returns exactly R) — accrued over the same wall time yields the EXACT same total curved
   delta (`==`, no tolerance: summing 2ᵐ equal fp values doubles exactly). Mutation that
   must fail: re-key the gain on the raw per-frame delta `mag` (drop the ÷frame_dt) — the
   slow schedule's 4× per-frame delta then gains differently and the totals diverge.
3. **Honest scope** (extends AT-9's existing latency note): a hand rate that VARIES within a
   frame quantizes to the frame's average — finer frames resolve the variation better. That
   is sampling, inherent to any per-frame device poll (identical in class to today's "< 1
   frame input latency"), NOT the gain variance the rail forbids: for the rate signal the
   device actually reports, the gain is frame-rate-invariant.
4. **Why this is not S7-mouselevel**: that trap was a frame-quantized signal driving a
   PER-TICK modification of a control-driving quaternion (state: an idle timer). Here there
   is no per-tick mechanism and no state — the per-frame delta itself (already frame-
   quantized, inherently) is scaled once, at its own boundary, by a pure function of itself.

## 4. Config + loader

`[ui]` (beside `aim_sensitivity`, controller.toml):

```
aim_curve_knee     = 250.0   # [px/s] below this: pure linear (precision zone untouched)
aim_curve_rate_hi  = 2500.0  # [px/s] rate at which the gain reaches gain_max
aim_curve_gain_max = 2.5     # [x] flick acceleration ceiling (1.0 = curve OFF, bit-identical)
aim_curve_expo     = 1.5     # [-] shape between knee and rate_hi (1 = linear ramp)
```

Loader checks: `knee ≥ 0`, `rate_hi > knee`, `gain_max ≥ 1`, `expo > 0`. Params land in
`ControllerParams` (the `aim_sensitivity` precedent — [ui] is already loaded there).
Defaults ship ON (Chad flies the felt change; the off arm is one knob).

## 5. Tests (all in a new test_aim_curve.cpp + one banner edit)

1. **Shape**: g==1 exactly at rate ≤ knee (probe: below, AT the knee boundary-exact);
   monotone across the band; g == gain_max exactly at/above rate_hi; continuity at the knee
   (t=0). Probe INSIDE the band against the config-recomputed formula oracle (the MB-rud
   band-placement lesson: sample inside + at the exact edge, never only outside).
2. **Knob-off bit-identity**: gain_max = 1.0 ⇒ output == input bit-exact across a sweep of
   rates including > rate_hi (the strict-superset arm).
3. **Frame-rate invariance** (§3.2 above, exact ==, mutation-verified vs the raw-delta
   re-key AND a `frame_dt`-ignoring constant-window mutant).
4. **Direction preservation**: curved output ∥ input (cross == 0 up to fp) at a steep
   diagonal — kills any per-axis-curve refactor.
5. **Zero-delta / dt-floor totality**: (0,0) → (0,0) exact; frame_dt = 0 does not NaN/Inf.
6. Loader checks pinned in test_load_controller (the existing pattern).

## 7. Plan red-team ledger (fresh-context Fable 5, one scoped round — FOLDED)

Verdict **SOUND-WITH-FIXES**, no P0. How each finding landed in the implementation:

- **P1-1 (clamped_dt is the wrong rate window):** main.cpp passes the RAW `frame_dt`
  (GetFrameTime) to `aim_curve`, never `clamped_dt` — the sim-stall clamp under-reports wall
  time and would turn a debugger-pause of slow tracking into a spurious full-gain flick.
  `kAimCurveDtFloor` stays as the in-function totality guard.
- **P1-2 (the sums-over-frames exactness claim was FALSE):** sequential `+=` of equal
  full-mantissa values is only accidentally exact to 4 terms and breaks at 8+ — the planned
  totals-across-schedules leg would have false-failed an honest implementation and grown a
  tolerance hide-band. Replaced by the rigorous per-call **power-of-two scale invariance** leg:
  `aim_curve(4δ, 4dt) == 4·aim_curve(δ, dt)` bit-exact (rounding commutes with 2^k scaling),
  probed in-band, at the cap, and sub-knee; kills the raw-delta re-key and constant-window
  mutants.
- **P1-3 (integer-pixel rate noise at high fps):** a 1-2 px frame at 240-1000 fps reads an
  fps-scaled rate (up to 1000 px/s) on a SLOW hand — acceleration inside the precision zone,
  the rail's forbidden outcome, invisible to real-valued test deltas (the MB-atm vacuous-leg
  class). Fix: the `aim_curve_quant_px` guard (deltas ≤ 3 px never curve) + knee raised
  250 → 400 (headroom over the 240 fps 1-2 px band) + an end-to-end quantized-device leg
  (constant 160 px/s hand through a floor quantizer at 64 vs 256 fps, totals exactly equal;
  `quant_px → 0` mutant inflates the fast run).
- **P2-1:** knob-off arm returns FIRST, structurally (no sqrt/no -0.0/denormal reasoning).
- **P2-2:** the min() cap pinned by the only probe that can see it (ABOVE rate_hi); gain_max
  pinned with the binary-friendly shipped default against the composed expression; the
  direction leg uses a tolerance (a direction claim, not a frame-rate one).
- **P2-4a:** the freelook ORBIT consumes the same curved deltas — flagged in the toml + the
  flight-log row as a second felt change riding this flight (a fast sweep reaches up to
  gain_max× farther).
- **P2-4b:** the first focused frame's delta after a focus loss (or startup) is DROPPED in
  main.cpp (`refocus_drop`) — raylib's cursor-warp delta would otherwise be amplified gain_max×.
- **P2-5:** DPI dependence documented in `[ui]`.
- **P2-3 (Jensen variance of a ramping flick, few-%–20% across fps):** accepted honest scope,
  inherent to any memoryless per-frame rate key; noted in the flight-log row so "feels
  different on the laptop" attributes to sampling, not a bug.

## 6. Open points for the red-team (answered — kept for the record)

- Is the accrual-site placement blind to anything the consume-site sees? (The 0-tick carry
  sums two frames' curved deltas — each curved in its OWN frame window; is the sum-of-curved
  vs curve-of-sum difference a felt or legality issue? Design says: sum-of-curved is the
  CORRECT integral ∫g(r)·r dt; curve-of-sum would be the wrong window.)
- The `consumed_*` orbit report now carries curved deltas — any test that pins
  `consumed == offered` (test_at9 forwarding leg) still holds because injection is
  post-curve; confirm no OTHER consumer assumes raw-device units.
- kDtFloor value vs main.cpp's existing clamped_dt (redundant guard acceptable?).
- Default knob values sane for a 1080p mouse? (feel numbers — Chad tunes; only sanity here.)
