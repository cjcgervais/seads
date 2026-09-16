# The lateral nose-down — the slice, and the unload that answers it

**Status (2026-09-15): KERNEL v16 CANDIDATE, FLOWN AND LIKED, NOT LANDED — ON HOLD FOR CHAD.**
Grounded in `D:\flight_sim2\seads-feel` branch **`feel/lateral-yawbudget`** (code at
`b4864512c`, handoff at `40325f297`), **not** in `reference/seads-feel/` (which is the v15
`main`). Every symbol below is cited from that branch. When it lands, re-ground this entry
in the snapshot and strike this paragraph.

## Lineage

TARGET 2 of the 2026-09-13 loop/lateral session. TARGET 1 (the held-loop rollover) landed
as kernel v15 S-righthand. This is the other half of the same fly report, and it is the one
Chad names as the last of the improvements: the aeroplane must follow a large sideways
mouse deflection without putting its nose in the dirt.

---

## 1. Feel

Chad, verbatim, across the thread:

> "when I make a large sideways deflection of my mouse and ask the plane to follow it, it
> noses down crashing me if I am near the deck."

> "I can do the vertical loops and immelmans without a hitch. The sideways deflections sent
> me packing dirt everytime, the upward one still exist too. But you got it mostly good work."

The governing law, the gun-director law:

> "the plane should follow my mouse; the best way to think is that I am directing my guns and
> an instructor would never crash me into the ground if I did not mount my mouse anywhere
> near there."

After the second unload flight:

> "I think it is okay now, could I be stalling in a sense when I am lower speed, like just a
> natural crash of consequence?"

> "I like it now, we can commit and push to main and seads before I run out of tokens"

What it feels like: you throw the aim hard to one side, level or a little above the
horizon. The aeroplane rolls hard, the nose comes round, and then it keeps going *down*
through the aim and does not stop until you are hundreds of metres lower, or in the ground.
It is not a stall. On his second run there were zero stalls, zero ballistic ticks, never
below 140 m/s, and the slices started at his *fastest* flying (V p50 229.6). What he feels
at the limit is the AoA limiter holding him just under the cap, which reads as the aeroplane
refusing to bite.

## 2. Principle

The fault is an **elevation overshoot that ends in a slice**. The aim sits above the
horizon. The nose passes through it and continues 80–108° below it. Meanwhile the bank has
gone past 90° (median 96–103°, peak 180°), so the **lift vector points below the horizon**.
From there the instructor's own correct instinct is the disease: the pitch channel commands
nose-UP in body frame on essentially every tick, and with the lift vector below the horizon
**the pull IS the dive**. Pulling harder turns the aeroplane harder into the ground.

Why nothing on the roll or rudder side could fix it (five actuators measured on his own
seeded dives, all rejected): roll cannot raise a nose, and the rudder is already claimed by
the aim. The pitch side is clamped too: on his worst events the commanded pitch rate sits
exactly on the AoA ceiling while the G ceiling never binds, so a pitch-side gain change is
truncated straight back. Only the G demand itself was left to act on.

So the answer is two coordination dials, each a strict-superset "0 = bit-identical" gate:

1. **Yaw budget** (`yaw_vert_budget`) — a *mitigation*: the rudder may spend at most the
   elevator's *spare* vertical authority. At knife-edge the elevator has none, so the
   digging rudder is trimmed to nothing exactly where his dives start. Recovers 36–62 m of
   a 124–490 m descent. It does not prevent the dive.
2. **Unload below the horizon** (`unload_below_horizon`) — the *fix*: when the lift vector
   is below the horizon AND the flight path is falling below the aim, remove the pull. The
   recovering roll is left at full rate, so the lift gets back above the horizon sooner with
   less nose-down accrued.

**The discriminator is the flight path, not the bank.** He flies past 90° of bank on
50–57% of the hard turns that *hold altitude*; past-90 is his normal cruise-fight attitude.
A bank-only gate armed on 72% of a scripted lat-90 roll-in, the flat-turn regression of
2026-07-06. What separates the two populations is γ, the flight-path elevation: good tracked
turns sit at γ p50 +19.4°, slices at −55.1°. Armed on (aim elevation − γ), the dial catches
89.5% of the slices and 3.1% of the good turns. **No dwell gate**: the slices are short
(0.87 s) and his good past-90 turns are longer (1.22 s), so a sustain gate separates them
backwards.

**Split-S and Immelmann protection is the sign of that same quantity.** In a split-S the
aim is below the path, so the arm is identically zero. The Immelmann is bit-identical.
**The open question is the vertical loop** (see §5).

## 3. Math

Frame: `e.nose`, `e.vhat` (velocity direction), `e.local_up` unit vectors; `e.phi` bank;
`e.cos_phi_theta` = cos φ · cos θ, the term that hits zero at knife-edge and at the loop
apex; `aim_elev` = the aim's elevation as a dot against `local_up`; `pitch`, `yaw` the
commanded body rates before the AoA/G clamp sequence; `pitch_ceil` the AoA pushback ceiling
`K_aoa · (aoa_max − α_f)`; `smoothstep(lo, hi, x)` the usual clamped cubic.

**Dial 1, S-yawbudget** (applied BEFORE the S-straightline axis correction reads `yaw`, so
the pitch feedforward cancels the rudder contribution actually emitted):

```
yv_dig  = yaw · sin φ                       (the rudder's vertical component; < 0 = digging)
if yaw_vert_budget > 0 and cosΦθ > 0 and yv_dig < 0:
    sag_pre = aim_elev − nose · local_up    (nose below aim ⇒ positive)
    avail   = max(0, pitch_ceil − pitch) · cosΦθ     (elevator's SPARE vertical authority)
    budget  = yaw_vert_budget · avail
    if −yv_dig > budget:
        gate     = smoothstep(yaw_vert_gap_lo, yaw_vert_gap_hi, sag_pre)
        yb_scale = 1 − gate · (1 − budget / (−yv_dig))
        yaw     *= yb_scale
```

`avail` uses the pitch *before* the cancellation is spent, which breaks the circularity
budget → yaw → axis correction → pitch → budget. The gate opens at ONSET: `gap_lo` −5°
(trim starts before the nose reaches the aim), `gap_hi` +10°. Ungated, the dial collapses
the V250 lat-90 turn; the gate is load-bearing. Loader wall: refused when
`line_hold_ff == 0` (red-team P1-4; silently dead otherwise).

**Dial 2, S-unload** (after the roll limbs, before the AoA/G clamp):

```
bfv    = | atan2( sin φ, cosΦθ ) |                  (bank_full, the lift-vector angle)
bank_f = smoothstep(unload_bank_lo, unload_bank_hi, bfv)      (90° → 105°: the WEIGHT)
if bank_f > 0:
    g_dot = vhat · local_up                        (sin γ, the flight-path elevation)
    arm   = smoothstep(unload_gam_lo, unload_gam_hi, aim_elev − g_dot)   (the DISCRIMINATOR)
    if arm > 0 and pitch > 0:                      (SIGN GUARD: only PULL is removed)
        f      = 1 − unload_below_horizon · bank_f · arm
        pitch *= f
```

The `gam` edges are **differences of sines**, not angles: the toml says 5° and 20°, the
loader stores `sin(5°)` = 0.0872 and `sin(20°)` = 0.3420 (red-team P2 corrected the
mislabel). The sign guard (red-team P0): past 90° of bank a PUSH is the nose-raising input;
scaling it toward zero would disarm the recovery. Measured guarded vs unguarded was
bit-identical to six decimals on all five fixtures, so the defect was latent, not active.

**Values on the branch:** `yaw_vert_budget = 1.0`, `yaw_vert_gap_lo/hi = −5 / 10 °`;
`unload_below_horizon = 1.0`, `unload_bank_lo/hi = 90 / 105 °`, `unload_gam_lo/hi = 5 / 20 °`.
`frac 0.5` is nearly inert on the dives, so 1.0 is the only useful value and the fallback
is 0, not a smaller number.

**Measured (seeded exact from Chad's tapes):** t6deck AGL 0 → 80 m (the deck event
survives); t5worst dAlt −527 → −262; ev7 −606 → −538; ev1 −429 → −391. Honest costs:
t5worst turn rate 48.9 → 6.1 °/s (it unloads and stops turning on that one event); V250
lat-90 turn 41.85 → 39.54 °/s (−5.5%), the first COST regression a surviving dial has had;
3.1% of his good past-90 ticks lose G; past-90 dwell on descent ticks *rises* because the
aeroplane unloads there instead of pulling through — the gauge looks worse while the
outcome is better.

**The grading invariant** (from his tape, before the dials): whenever |aim elevation| < 45°
and err > blend_lo, require |nose elevation − aim elevation| ≤ 30°. 19 of 61 events
violated it, worst 100.3°.

## 4. Code

Branch `feel/lateral-yawbudget`, `D:\flight_sim2\seads-feel` (shared repo `D:\flight_sim2\seads`):

- `control/params.h` — `ControllerParams::yaw_vert_budget`, `yaw_vert_gap_lo/hi`,
  `unload_below_horizon`, `unload_bank_lo/hi`, `unload_gam_lo/hi`; the `SEADS_FEEL_DIALS(X)`
  list (`right_hand_rest`, `yaw_vert_budget`, `unload_below_horizon`) and the
  `SEADS_FEEL_DIALS_OFF` macro whose first proposed fix was undefined behaviour (a reference
  bound to itself; the shipped binding name is `seads_feel_dials_obj_`). The measured
  actuator table lives in the comment above `yaw_vert_budget`.
- `control/controller.cpp` — the S-yawbudget block (guard `cp.yaw_vert_budget > 0.0 &&
  e.cos_phi_theta > 0.0`, writes `out.telem.yaw_budget_scale`) immediately before the
  S-straightline `parasitic` / `w_axis` axis correction; the S-unload block (guard
  `cp.unload_below_horizon > 0.0`, writes `out.telem.unload_scale`) after the roll-limb
  telemetry, before S-aimff.
- `config/controller.toml` `[coordination]` — the dials with their measurement ledger in
  the comments. `config/load_controller.cpp` — degrees → radians for the bank edges,
  degrees → `sin` for the `gam` edges, the [0,1] walls, the `line_hold_ff == 0` refusal.
- `test/unit/test_yawbudget.cpp` — the S-unload legs (split-S sign pin, the five seeded
  onset fixtures); `test/unit/test_tape_roundtrip.cpp` and `app/feel_tape.h` — the feel-tape
  v5 instrument that seeded them. NOT yet folded (red-team P1-3): a ramp-vs-step pin per
  smoothstep band and a direct non-push split-S leg (AT-15 spends 208 of 240 ticks in
  `push_mode`, where the block is unreachable).
- Not in `reference/seads-feel/` (v15). The v15 snapshot carries `right_hand_rest` and
  `lean_lead_lateral` only.

## 5. The open question — Chad's, not the agent's

The unload **arms during the back half of a pure vertical loop**: nose below the horizon,
inverted, aim still ahead. v15 was signed on "I can do the vertical loops and immelmans
without a hitch." On a scripted loop it arms on 25.9% of the loop and the loop finishes
57.6 m *higher* than without it; the red-team's own loop scenario read 232 m *lower*
(unload only) / 110 m lower (both on). The two disagree in sign and neither has been
reproduced against the other. Either way the loop's shape changes against v15's signature.
The Immelmann is bit-identical, 0% armed. The earlier claim "loop protection verified on
his hands, 0 of 723 ticks" only examined the *climbing* half (nose above +60°); the dial
arms on the *back* half.

**The question, as put in the handoff:** land anyway, narrow the arm so it cannot fire in
a loop, or drop the unload and keep only the yaw budget? Also uncleared: a deliberate
split-S flown with the aim held UP (409 of 1523 nose-below-60 ticks armed on his tape;
attitude alone cannot tell it from a slice). Ask him to fly one and say whether it felt
held back.

## Scars (do not rebuild)

- `maneuver_invert_band` (landed `31d109915`, walked back `157245d84`): a `cosΦθ` fade on
  the maneuver roll limb is a self-locking wall at the knife-edge.
- `lap_roll_frac` (removed `37d5f5d83`): fired on 0 of 10134 ticks of his flying; cost
  272 m on a lateral sweep.
- `maneuver_bank_max` (removed 2026-07-06 by Chad): "a bank cap flattens turns but they go
  low-G... skid via coordination, not a bank cap." The §3.4 aim-conditioned bank sizing
  discussed in the handoff is adjacent to this and is his call, not to be built first.
- Candidate (a) `pitch_bank_comp`: inert, because it scales the demand before the AoA
  clamp truncates it. Candidate (b) `bank_yaw_trade`: dead on arithmetic (3.35 g of side
  force does not exist). Candidate (c) V-aware G ceiling: no bleed to prevent.
