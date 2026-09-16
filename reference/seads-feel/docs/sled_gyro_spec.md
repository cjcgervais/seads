# SPEC — SLED ROTOR GYROSCOPICS (G1 BUILT, DEFAULT-OFF)

Branch `sandbox/snow`, worktree `D:\seads_sandboxes\sandbox_snow`. C++17, glm, pure `sim/` kernel.

**Status: G1 + G2 BUILT AND PINNED, BOTH DEFAULT-OFF. G3-G4 not built.**
Red-teamed context-free by Fable before any code was written; the review found three real errors in
this document and they are corrected in place below, marked **[RT]**. Rotor numbers came from a
parallel sourcing task and are recorded with their provenance.

## 0. WHAT LANDED (2026-08-27)

`k_gyro` defaults to **0.0**, which is bit-identical: pinned by
`sled_gyro_is_OFF_by_default_and_the_dial_gates_it_completely`, and by the full sled suite showing
the same 4 pre-existing GI4 reds and nothing new. **Chad has not flown it. Until he does, it is
built, not approved.**

Measured with the term armed (`[gyro]` legs, all passing):

| leg | result |
|---|---|
| yaw -> roll, sign measured | `L_r -108.9 kg m^2/s`, `d_roll -1.0166 rad/s` |
| pitch immunity | `d_yaw 0.000e+00`, `d_roll 0.000e+00` -- EXACTLY zero |
| roll -> yaw (antisymmetry) | `d_yaw +0.27483 rad/s` |
| value oracle vs closed form | relative error **0.0000** |

One second of airborne flight seeded at 0.6 rad/s of yaw develops **1.02 rad/s of roll** (58 deg/s).
This is not a rounding term.

**G2 (reaction), armed:** a throttle blip in the air pitches the nose up at **+0.594 rad/s
(34.1 deg/s)**, and the momentum ledger balances EXACTLY — `borrowed -0.092855` against
`-(L_end - L_start)/I_x = -0.092855`. Applied as a telescoping delta to the rate, never a torque.
The two dials are independent by construction; coupling them through `k_gyro` silently zeroed G2 on
its first run and was caught by its own knob-off leg.

### [RT] Three errors this document had, found by the red team

1. **Momentum does not refer by ratio squared.** Inertia does (`I*n^2`); angular momentum is
   `sum(I_i * w_i)` per shaft. The original 2.1 wrote the crank as `(I*n^2)*w_axle`, overstating it
   by the live CVT ratio -- and because a CVT *pins engine rpm*, the engine-side momentum is roughly
   CONSTANT with road speed rather than proportional to it. Modelling it the original way would have
   made the effect vanish at low speed, which is exactly where the crank dominates. **Fix:** G1 ships
   the TRACK SIDE ONLY, stated as such; the crank/clutches become G3, computed off `engine_rpm`.
2. **The stability bound in R-2 was the wrong law, and h was wrong by 12x.** `K*dt/I <= 0.5` is a ZOH
   ceiling for a damping-like term; `-w x L_r` is skew-symmetric and explicit Euler grows its
   amplitude at *every* step size, so there is no threshold to satisfy -- only a growth rate to size.
   And `substeps = 12`, so h is dt/12, not 1/120. Corrected: nutation rate
   `Omega_n = |L_r| / sqrt(I_y*I_z)` = 108.9/sqrt(186.9*49.7) ~ 1.13 rad/s; `Omega_n*h ~ 1.6e-3`;
   growth ~1e-6 per step. Negligible, and the existing `w x I*w` term already carries more of the
   same class.
3. **A dead test citation, inherited unverified.** This document cited
   `sled_air_shift_pitches_the_nose_DOWN_on_an_aft_throw`; no such test exists. The real one is
   `sled_airborne_lean_is_RESPONSIVE_at_k_nonzero`. The dead name was also in the SHIPPED kernel
   comment at `sim/sled.h`, inherited from there -- now corrected in both places. A comment that
   names a test is a claim about the tree.

### [RT] Gaps stated, not smuggled

- **The airborne brake-tap cannot emerge from this model.** `sim/sled.cpp:1250` computes
  `rep_belt = max(dv*v_cmd, max(v_bf, 0))`, so brake input can never pull belt speed *below* forward
  speed. Half of the iconic airborne attitude control (blip = nose up, **brake tap = nose down**) is
  therefore structurally unreachable until the belt readout is reopened -- which other consumers
  (rig, roost, rpm) read, so it is the largest blast radius of the set. It is G4, and it is gated on
  Chad asking for it.
- **The kinematic belt spins up for free.** The reaction term (G2) will charge the chassis for
  momentum whose energy was never charged to the engine. Acceptable at this fidelity; recorded so
  nobody discovers it as a surprise.
- **Idle crank momentum is not zero.** Once G3 exists, `engine_rpm` floors at 1700, so a stationary
  machine carries a small permanent `L_r`. "Stopped implies L_r = 0" will be false.

## 0.1 THE ROTOR NUMBERS, AND WHERE THEY CAME FROM

`rotor_inertia_track_kgm2 = 0.193`, `drive_radius_m = 0.0815`.

The belt this kernel already describes is a **real Camso track**, and that is checkable rather than
asserted: Camso's own definition is `pitch x segments = length`, and `2.52 in x 48 = 120.96 in =
3.0724 m` is EXACTLY `track_belt_circumference_m`. `track_lug_height_m 0.0184 m = 0.7244 in` is
their 0.725 in / 18 mm class. That identifies a 15 in x 121 in 9997R at a manufacturer-published
**36 lb = 16.25 kg**. `drive_radius_m` is the pitch radius of the matching 8-tooth 2.52 in driver,
`N*p/2pi = 0.0815 m`.

Independent corroboration: this kernel's own 8000 rpm at 46 m/s needs an engine:axle ratio of
**1.484**; real gearing (0.80 CVT shift-out x 1.857 chaincase) gives **1.486** -- 0.1 %.

`0.193` is the momentum-equivalent inertia referred to the drive axle, `L(46 m/s)/w_axle =
108.9/564.4`, summing belt + idlers + axle + chaincase + jackshaft. **Weakest inputs** (crank and
clutch inertia) are deliberately EXCLUDED from G1 -- no published source exists for either, and they
are the ratio-squared trap. Overall bracket on the full rotor was +/-40 %, but the felt-vs-rounding
conclusion does not flip anywhere in that range.

---



Owner ruling, 2026-08-27 (Chad, verbatim): *"is there mention of the gyroscopic effect on stability
in this kernel?"* → on being shown it is absent and knowingly deferred: *"I think we should build
it. Because then it has the foundation it needs."*

---

## 1. WHAT EXISTS TODAY

### 1.1 Rigid-body gyroscopics: PRESENT and correct
`sim/sled.cpp:1698-1701` integrates the full Euler equation:

```cpp
const glm::dvec3 Iw = I * s.angular_vel;
const glm::dvec3 alpha_body = (torque_body - glm::cross(s.angular_vel, Iw)) / I;
s.angular_vel += alpha_body * h;
```

`sim/sled.cpp:1711` names `ω × Iω` "the gyroscopic term", in a comment explaining why the airborne
rider-momentum exchange is applied to the RATE and not through `torque_body` — precisely so it is
not mistaken for an external moment. That reasoning is load-bearing and the change below must not
break it.

### 1.2 Rotor gyroscopics: ABSENT, and deferred on purpose
`sim/sled.h:641-642`, verbatim:

> *"A PERSISTING rate would have to come from the track as a reaction wheel, which is not this
> rung."*

`sim/sled.h:579` is the only inertia in the file — `glm::dvec3 inertia{158.7, 186.9, 49.7}`, the
CHASSIS. There is **no rotating-mass inertia** for belt, axles, bogies, clutches, or crank anywhere
in `sim/`.

### 1.3 Conventions this spec must obey
- Body axes (`sim/sled.h:109`): **+X right, +Y up, +Z AFT**; right-handed. Nose is −Z.
- Inertia axis roles (`sim/sled.h:574`): **X = pitch, Y = yaw, Z = roll**.
- Yaw sign (`sim/sled.h:45`): **positive yaw rate is nose LEFT**; `steer = +1` steers LEFT, pinned by
  `sled_steer_plus_one_yaws_nose_left`.
- Published kernel readouts already available: `belt_speed_ms`, `track_slip`, `engine_rpm`,
  `ground_speed_ms` (`sim/sled.h:1064-1071`).
- Known belt geometry: `track_belt_circumference_m = 3.0724`, `track_width_m = 0.38`,
  `track_len_m = 1.14`, `track_lug_height_m = 0.0184`. Machine+rider `mass_kg = 331.0`,
  `rider_mass_kg = 87.5`.
- **`sim/` and `control/` are a FROZEN kernel.** This change is authorised by the owner
  specifically; it does not reopen the kernel for anything else, and it ships DEFAULT-OFF until he
  rules it on the stick.

---

## 2. THE PHYSICS

A rigid body carrying an internal rotor of angular momentum **L_r** (expressed in body axes) obeys

&nbsp;&nbsp;&nbsp;&nbsp;**I·ω̇ + ω × (I·ω + L_r) + L̇_r = τ_ext**

so

&nbsp;&nbsp;&nbsp;&nbsp;**ω̇ = I⁻¹ ( τ_ext − ω × (I·ω + L_r) − L̇_r )**

Two distinct physical effects fall out of the one term:

1. **Precession** — `−ω × L_r`. Because the rotor spins about the LATERAL axis, this couples
   **yaw ↔ roll** and leaves pitch untouched (a pitch rate is parallel to L_r, so the cross product
   vanishes). This is the "gyroscopic effect on stability" of the ask.
2. **Reaction / spin-up torque** — `−L̇_r`, about the lateral axis, i.e. **pitch**. Spinning the
   track up pitches the machine. This is exactly the "track as a reaction wheel" that
   `sim/sled.h:641` defers.

### 2.1 The rotor's angular momentum
&nbsp;&nbsp;&nbsp;&nbsp;**L_r = k_gyro · I_rotor · (belt_speed_ms / r_drive) · d̂**

where `d̂` is the lateral unit vector and `I_rotor` is the total drivetrain inertia REFERRED TO THE
DRIVE AXLE (belt + drive axle + idlers/bogies + clutches and crank through their gear ratios
squared).

**Direction, derived (to be CONFIRMED BY TEST, never typed — see §5):** for forward travel the
track's ground run moves aft, so the top of the drive sprocket moves forward (−Z). With
`ω × r = v`, a point at `r = R·Ŷ` moving at `v = −V·Ẑ` requires ω along **−X̂** in this
right-handed frame. So **L_r points to the machine's LEFT during forward travel.**

### 2.2 The numbers that do not exist yet
`I_rotor` and `r_drive` are NOT in the tree and must not be invented (house law: every number
describing a real thing comes from a measurement or the owner's words). A parallel research task is
sourcing real machine specs — belt mass, drive sprocket pitch/teeth, bogie masses, clutch inertia,
crank inertia, chaincase and clutch ratios. **This spec is written to be correct independent of
those values**; they set only the magnitude and therefore whether the effect is felt.

Sanity bound available today: chassis roll inertia is **49.7 kg·m²** and yaw **186.9 kg·m²**. If
`L_r` at 46 m/s belt speed is of order tens of kg·m²/s, precession torque at a 1 rad/s yaw rate is of
order tens of N·m, i.e. of order 1 rad/s² of roll — a felt effect. If it is of order 1 kg·m²/s, it is
a rounding term. **The research result decides whether this rung ships at all.**

---

## 3. THE PROPOSED CHANGE (minimal diff)

At `sim/sled.cpp:1698`, replace

```cpp
const glm::dvec3 Iw = I * s.angular_vel;
const glm::dvec3 alpha_body = (torque_body - glm::cross(s.angular_vel, Iw)) / I;
```

with

```cpp
const glm::dvec3 L_r = rotor_momentum(p, s);          // 0 when k_gyro == 0
const glm::dvec3 Iw = I * s.angular_vel + L_r;        // rotor rides INSIDE the cross product
const glm::dvec3 alpha_body =
    (torque_body - glm::cross(s.angular_vel, Iw) - dL_r_dt) / I;
```

- `k_gyro = 0.0` by default ⇒ `L_r ≡ 0` ⇒ **bit-identical to today**, provable at the bit and the
  knob-off arm that makes the golden reproduce.
- The rotor momentum is added INSIDE the existing cross product, not as a separate torque, because
  that is what the equation says and it keeps ONE gyroscopic expression rather than two that can
  fork.
- `dL_r_dt` is a genuine torque and is therefore subtracted at the same site; it is NOT routed
  through `torque_body`, for the same reason `sim/sled.cpp:1711` keeps the momentum exchange out of
  it.

### 3.1 New params (`sim/sled.h`)
```cpp
double k_gyro = 0.0;          // MASTER DIAL. 0 = off, bit-identical. Chad's stick moves it.
double rotor_inertia_kgm2 = 0.0;   // drivetrain inertia referred to the drive axle
double drive_radius_m = 0.0;       // belt speed -> rotor rate
```
All three default to values that make the term identically zero, so an unset config cannot silently
arm it.

### 3.2 New state (`sim/sled.h`, reported never branched on)
```cpp
double rotor_momentum_kgm2s = 0.0;   // |L_r|, so the HUD/probe can show it
```

---

## 4. THE RISKS THIS DOCUMENT EXISTS TO SURFACE

**R-1 — `L̇_r` spikes.** Belt speed is `drive · throttle · 46` and can step hard on a throttle stab
or a clutch blend. A raw per-step finite difference `(L_now − L_prev)/h` could produce a large
one-tick pitch impulse. Options: differentiate the SMOOTHED belt speed, rate-limit `L̇_r`, or omit
the reaction term in rung 1 and ship precession only. **Which is right?**

**R-2 — Explicit-integration stability.** The added coupling is a cross product on the state being
integrated; at large `L_r` this is a rotation-like operator with rate `|L_r| / I`, and the existing
integrator is explicit at `h = 1/120`. The ZOH ceiling the controller docs use is `K·dt/I ≤ 0.5`.
For roll: `|L_r|·h / I_z ≤ 0.5` ⇒ `|L_r| ≤ 0.5 · 49.7 · 120 ≈ 2982 kg·m²/s`. Comfortable if `L_r` is
tens — but is that the right stability criterion for a cross-coupled term, or does it need the
semi-implicit treatment?

**R-3 — Airborne momentum exchange interaction.** `sim/sled.cpp:1703+` (K-WS1/K2) applies a rider
momentum delta directly to the rate, deliberately bypassing the gyroscopic term, and its comment
says a persisting rate "would have to come from the track as a reaction wheel." This rung ADDS that
reaction wheel. Do the two now double-count, contradict, or compose correctly?

**R-4 — Ground contact.** Should `L_r` exist when the track is stopped but the machine is moving
(coasting, engine off), and when airborne with the track still spinning? The airborne case is where
riders actually use precession, so the term must NOT be gated on ground contact — but confirm.

**R-5 — Does this belong in `sim/sled.cpp` at all,** or is the honest first rung a measurement-only
probe that computes `L_r` from published state and REPORTS the coupling magnitude without applying
it, so the owner can be shown the number before any dynamics move?

**R-6 — Sign.** Per `sim/sled.h:646` — *"THE SIGN IS MEASURED BY TEST, never typed"* — §2.1's derived
direction must be pinned by a behavioural test, not asserted. What is the right test shape? A
candidate: airborne, spin the rotor, apply a known yaw rate, assert the roll rate develops in the
predicted direction and reverses when `L_r` reverses.

---

## 5. WHAT MUST PIN IT

1. **Knob-off is bit-identical.** `k_gyro = 0` reproduces the existing golden byte-for-byte.
2. **Precession sign**, behavioural, per R-6 — and the mutation that flips it must FAIL.
3. **Pitch is untouched by precession** (a pitch rate is parallel to `L_r`): a pure pitch rate with
   `k_gyro = 1` produces no yaw or roll from this term.
4. **Energy/consistency**: the term does no work — `ω · (ω × (Iω + L_r)) ≡ 0` — so it must not
   change kinetic energy. A drift here means a sign or ordering slip.
5. **Reaction term** (if shipped): spinning the rotor up produces a pitch impulse whose sign is
   pinned by test, and whose integral over a spin-up/spin-down cycle returns to zero (it telescopes,
   exactly like the rider exchange).
6. **No automated test may judge FEEL.** These pin correctness only; the owner's drive rules whether
   `k_gyro` is 1.0, something else, or 0.

---

## 6. QUESTIONS FOR THE RED TEAM

**Q1.** Is the §3 formulation correct and minimal, and is putting `L_r` inside the existing cross
product the right call versus a separate torque term?

**Q2.** Rule on R-1 (the `L̇_r` spike): ship precession only in rung 1, or include reaction with a
smoothed/limited derivative? If smoothed, off what signal and with what time constant, derived from
what?

**Q3.** Rule on R-2: is `|L_r|·h/I ≤ 0.5` the right stability bound for a cross-coupled term at
`h = 1/120`? If not, what is, and does this need semi-implicit treatment?

**Q4.** R-3: does the rotor reaction wheel double-count against the K-WS1 airborne rider exchange?
Read `sim/sled.cpp:1703-1760` and `sim/sled.h:620-660` and rule.

**Q5.** R-4: correct gating of `L_r` across ground contact / airborne / coasting / engine-off.

**Q6.** R-5: is a report-only probe the honest first rung, given the owner is asleep and the effect's
magnitude is not yet known?

**Q7.** Are §5's pins sufficient, and what would you add? In particular, what mutation would survive
them?

**Q8.** Anything this spec has missed. Prior rungs in this project have been rejected for shipping
something plausible that was not what was asked; assume the same standard.
