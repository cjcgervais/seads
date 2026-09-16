# R4a PHASE 0 — THE RIDER LOAD MODEL

**Status:** DERIVED, NOT BUILT. This document is the specification the next
agent implements. It invents nothing: every constant below carries the file it
was measured out of, and the two derived constants (`I_pitch`, the rider CG)
show their arithmetic.

**Spec authority:** `docs/SUDBURIAN_LADDER.md` §7 (Chad's grip-law ruling,
2026-08-16), staged §7.9 R4a. Prior context:
`docs/SESSION_HANDOFF_20260825_r3hands.md` §6, finding 2 — *"a point-mass rider
deletes the quantity the rung is about."*

**Scope of this phase:** an *instrument*. It measures a load. It changes no
behaviour, moves no golden, touches no tape pin. Chad dials `grip_capacity_n`
later, against numbers this instrument produced.

---

## 0. WHY THE FIRST MODEL WAS REFUTED, AND WHAT THAT FORCES

The plan's first attempt put a **point mass at the rider CG**. Two independent
red-team objections killed it, and both must survive into the build:

1. **A point mass can take no moment.** Three equations (ΣF = m a), and the
   thing the rung is about — *"he pivots up over the bars"* — is a **pitch
   moment** event. The instrument literally could not see it.
2. **A priority ordering ("seat first, hands take the residual") reports ZERO
   grip load on exactly the big upward bump that throws him.** On the up-stroke
   the seat really does carry everything; what loads the hands is the
   *fall-away* on the far side, and a residual-ordering model has no mechanism
   that lets the seat drop out.

A **rigid** rider held at seat + boards + hands is statically indeterminate in
3-D: nine unknown contact scalars against six Newton-Euler equations. So the
model must be reduced until it is determinate **at every step**, and the
reduction must be a physical statement, not a stiffness ratio.

The reduction below is a **planar sagittal rigid rod**: 3 DOF, 3 equations, and
a four-case contact enumeration in which *every case is exactly 3×3 and
closed-form*. There is no solver, no iteration, no compliance dial, and no
tolerance to tune.

---

## 1. FRAMES, SIGNS, AND THE MEASUREMENT MAP

### 1.1 The kernel body frame

`sim/sled.h` (K2 block) and `sim/sled.h:574`:

> Principal inertia, body axes (X pitch, Y yaw, Z roll)
> body axes **[+X right, +Y up, +Z AFT]**

`(right, up, aft)` is right-handed. Pitch is rotation about **body +X**;
positive `α_x` rotates +Y toward +Z, i.e. **nose UP**.

> ⚠ Naming: the plan calls the pitch inertia `I_yy`. In this frame the pitch
> axis is **X**, and it is the kernel's `SledParams::inertia.x` slot. This
> document calls it `I_pitch` and never `I_yy`, so nobody wires it to the wrong
> component.

The kernel's body-frame **origin is the system CG** (`SledState::position`), and
that origin **moves with the rider's lean** through `cg_off`
(`sim/sled.cpp:399`):

```
cg_off = (p.rider_mass_kg / p.mass_kg) * (-rider_lat_m, rider_up_m, -rider_fwd_m)
       = 0.264350 * d          (87.5 / 331.0 — sim/sled.h:586 and :392)
```

Every machine-fixed point is therefore at `nominal_body - cg_off` — exactly what
`patch_geometry()` (`sim/sled.cpp:55`) already does to the three patch mounts.
The rider's own CG sits at `seat_offset + d - cg_off = seat_offset + 0.735650 * d`.

### 1.2 The GLB model frame → body frame map

`assets/sled/indy650.glb` is authored with **+X = model right, +Y = up,
+Z = forward**. The map is stated in `sim/sled.h` (K2 block):

```
body = ( -m.x ,  m.y - DROP ,  -m.z )      DROP = cg_height_m - kSagDefaultM
                                                = 0.564 - 0.10 = 0.464 m
```
* `cg_height_m = 0.564` — `sim/sled.h:396`
* `kSagDefaultM = 0.10` — `render/rider_pose.h:75`

**The map is confirmed by three independent kernel numbers, not asserted:**

| GLB node (measured) | body via the map | kernel constant | source |
|---|---|---|---|
| `ski_L` x = −0.4635 | +0.4635 | `stance_m/2 = 0.927/2 = 0.4635` | `sim/sled.h:398` |
| `ski_L` z = +0.860  | −0.860  | `-ski_fwd_m = -0.86` | `sim/sled.h:402` |
| `render::rider_cg` = (0.00033, 0.81774, −0.35734) | (−0.00033, **0.35374**, **+0.35734**) | `kRiderSeatOffY_m 0.3537`, `kRiderSeatOffZ_m 0.3573` | `sim/sled.h` K2 |

> ⚠ **Named debt, already on the record: `OPEN-KWS1-SAG` (`sim/sled.h`, K2
> block).** The kernel's own patch mounts use `-(cg_height_m - susp_rest_m) =
> -0.354`, i.e. 0.10 m *higher* than the render mount this map uses. The 0.10 m
> is `kSagDefaultM`, the visual static sag, and it is a render dial with no
> kernel owner. **This model uses DROP = 0.464 because that is the convention
> `kRiderSeatOffY_m` already lives in**, and because every moment arm below is a
> *difference* between two points measured through the same map, so the drop
> cancels out of the moment equation entirely. It survives only in the
> `α × r` / `ω × (ω × r)` station terms, where a 0.10 m error in `r_y` is
> second order. Do not "fix" this without re-pinning K2 in the same commit.

### 1.3 The sagittal reduction

The model lives in the plane `x = 0`. Planar coordinates:

```
ζ  =  body +Z   (AFT positive)
η  =  body +Y   (UP  positive)
θ  =  pitch about body +X (nose up positive)
```

Left/right pairs (two grips, two boots) are collapsed to their **lateral mean**,
which lies on the centreline by construction. Lateral lean is **deferred**
(Chad's own R4a ruling, handoff §6) — this instrument is blind to roll, and says
so.

---

## 2. THE MEASURED CONSTANTS

### 2.1 Rider mass

```
m_r = 87.5 kg         sim/sled.h:586   (rider_mass_kg, split OUT of mass_kg 331.0)
g   = 9.80665 m/s^2   sim/sled.cpp:1636
m_r * g = 858.082 N
```

### 2.2 Segment table — the source of the CG and the inertia

`render::rider_segments()`, `render/rider_pose.cpp:140`. Fourteen segments,
`mass_frac` summing to exactly **1.000000** (verified). Joint positions read
straight out of the shipped `assets/sled/indy650.glb` by composing node
transforms — the same numbers `render/rider_rig.cpp` documents and
`test/unit/test_rider_pose.cpp` re-derives.

Segment CG = `prox + cg_frac * (dist - prox)`.

| segment | prox → dist | mass_frac | m [kg] | L [m] | seg CG body (η, ζ) | I_own | m·d² |
|---|---|---|---|---|---|---|---|
| trunk | pelvis → spine_03 | 0.4970 | 43.4875 | 0.3820 | (+0.3876, +0.4934) | 0.52873 | 0.85434 |
| head+neck | spine_03 → head | 0.0810 | 7.0875 | 0.2307 | (+0.7512, +0.2802) | 0.03143 | 1.16162 |
| upperarm_l | upperarm_l → lowerarm_l | 0.0280 | 2.4500 | 0.3440 | (+0.5937, +0.1897) | 0.01659 | 0.20990 |
| forearm_l | lowerarm_l → hand_l | 0.0160 | 1.4000 | 0.2700 | (+0.4502, −0.0459) | 0.00837 | 0.24073 |
| hand_l | hand_l (point) | 0.0060 | 0.5250 | 0.0000 | (+0.3679, −0.1746) | 0.00000 | 0.14864 |
| upperarm_r | upperarm_r → lowerarm_r | 0.0280 | 2.4500 | 0.3440 | (+0.5943, +0.1885) | 0.01680 | 0.21162 |
| forearm_r | lowerarm_r → hand_r | 0.0160 | 1.4000 | 0.2700 | (+0.4527, −0.0479) | 0.00817 | 0.24361 |
| hand_r | hand_r (point) | 0.0060 | 0.5250 | 0.0000 | (+0.3719, −0.1753) | 0.00000 | 0.14913 |
| thigh_l | thigh_l → calf_l | 0.1000 | 8.7500 | 0.4530 | (+0.2255, +0.4037) | 0.12772 | 0.16282 |
| shin_l | calf_l → foot_l | 0.0465 | 4.0688 | 0.4550 | (+0.0743, +0.0502) | 0.06986 | 0.70152 |
| foot_l | foot_l (point) | 0.0145 | 1.2688 | 0.0000 | (−0.1332, −0.1020) | 0.00000 | 0.56858 |
| thigh_r | thigh_r → calf_r | 0.1000 | 8.7500 | 0.4530 | (+0.2254, +0.4037) | 0.12771 | 0.16284 |
| shin_r | calf_r → foot_r | 0.0465 | 4.0688 | 0.4550 | (+0.0743, +0.0502) | 0.06985 | 0.70149 |
| foot_r | foot_r (point) | 0.0145 | 1.2688 | 0.0000 | (−0.1332, −0.1020) | 0.00000 | 0.56857 |

### 2.3 The rider CG — cross-checked against a kernel constant

```
r_G(model) = Σ m_i p_i / Σ m_i = ( 0.00033, 0.81774, -0.35734 )   [m]
r_G(body)  = ( -0.00033, +0.35374, +0.35734 )
```

`sim/sled.h`'s K2 block states `render::rider_cg` of the rest-posed Sudburian is
at model `(0.00033, 0.81774, -0.35734)` and derives
`kRiderSeatOffY_m = 0.3537`, `kRiderSeatOffZ_m = 0.3573`. **This derivation
reproduces both to the digit.** That is the cross-check that says the segment
table, the GLB read and the frame map all agree.

### 2.4 `I_pitch` about the rider CG — the arithmetic

Parallel-axis sum over the same table, about the body **X** (lateral) axis
through `r_G`:

```
I_pitch = Σ_i [ I_own,i  +  m_i ( dη_i² + dζ_i² ) ]

I_own,i = (m_i L_i² / 12) * (1 - u_x,i²)      slender rod, u = unit prox→dist
```

The `(1 - u_x²)` factor is the rod's own inertia about a **lateral** axis; for
these segments `u_x ≈ 0` on the trunk and legs (they are sagittal) and non-zero
only on the arms, which spread laterally.

```
Σ I_own          =  1.00522 kg m^2      (14.2 % of the total)
Σ m d^2          =  6.08542 kg m^2      (85.8 %)
------------------------------------------------
I_pitch          =  7.09064 kg m^2
radius of gyration k = sqrt(I/m) = 0.2847 m
```

Free, from the same sum, for anyone who later wants the gyroscopic term (§4.5):

```
I_yaw  (about body Y) = 5.95988 kg m^2
I_roll (about body Z) = 5.94632 kg m^2
```

> **Honest limit, measured or stated:** `I_own` uses **slender rods**. Real
> limbs have radius, so `I_own` is a *lower* bound and `I_pitch` is slightly
> low. The size of that error was **not measured** — I did not measure segment
> radii off the proxy mesh — so it is stated, not estimated. Its leverage is
> bounded: `I_own` is 14.2 % of `I_pitch`, so even a 100 % error in `I_own` is a
> 14.2 % error in `I_pitch`. A cheap follow-up (measure the skin-weighted vertex
> radius per bone off `sudburian_proxy`) closes it if it ever matters.

### 2.5 The three contact stations

All measured out of `assets/sled/indy650.glb`, then mapped by §1.2. Sockets from
`render::machine_node_specs()` / `render::rider_chain_specs()`
(`render/rider_rig.cpp`).

| contact | model point | body (ζ, η) | ζ − ζ_G | η − η_G |
|---|---|---|---|---|
| **grip** — mean of `grip_socket_L` / `_R` | (0, 0.78670, +0.2690) | (**−0.26900**, +0.32270) | **−0.62634** | **−0.03104** |
| **boots** — mean of `foot_l` / `foot_r` | (0, 0.33081, +0.1020) | (**−0.10204**, −0.13319) | −0.45938 | −0.48693 |
| **seat** — seat top under the pelvis | (0, 0.59830, −0.5848) | (**+0.58484**, +0.13430) | +0.22750 | −0.21944 |
| rider CG | (0.00033, 0.81774, −0.35734) | (+0.35734, +0.35374) | 0 | 0 |

Provenance for each:

* **Grip.** `grip_socket_L/R` at model `(∓0.315, 0.786695, 0.269)`. The **bar**
  is where the hand force acts, not the wrist: `render/rider_rig.h` states the
  `hand_*` node is the **wrist** and `seat_sudburian.py` puts the **knuckle** on
  the bar, wrist 98 mm behind-and-above the socket. Use the socket.
* **Boots.** `render/rider_rig.h` says outright: *"the sockets are frame bolts,
  not the sole's spot"*, and `rider_chain_specs()` carries
  `rest_tip_minus_socket_m = (0, 0.003784, 0.272032)` — the foot is 272 mm
  forward of `board_socket_*`. Using the socket (body ζ = +0.170) instead of the
  boot (body ζ = −0.102) is a **272 mm** error in the most load-bearing arm in
  this model. **Use the foot node.**
  ⚠ `foot_*` is the **ankle**, not the sole's centre of pressure, which sits
  somewhat forward of it. Not measured. Moving ζ_board forward only *widens* the
  support interval of §4.3, which produces *lower* grip load — so the ankle is
  the conservative (narrower) choice, and it is stated rather than tuned.
* **Seat.** The `seat` mesh's top surface, world-transformed and profiled in
  0.05 m ζ bins: flat at model y = 0.5983 from z = −0.70 to z = 0.00, rising to
  0.6443 at the rear hump (z = −0.90). The pelvis joint is at model
  z = −0.5848, so the seat top directly beneath the hip joint centre is
  **y = 0.5983**, 86 mm below the joint — anatomically right for a hip centre
  above the ischia. ζ_seat is taken as the pelvis's own ζ.
  ★ In this model the seat is **frictionless and vertical**, so **η_seat never
  enters any equation** — only ζ_seat is load-bearing. It is recorded for the
  record, not consumed.

---

## 3. THE STATION KINEMATICS

The rider is held **rigidly** to the machine (this is the whole bias — §6). His
CG is a station on the machine, and his angular acceleration is the machine's.

For any point at body position `r` fixed in the machine, the **inertial**
acceleration, expressed in body axes:

```
a_station = a_body + α × r + ω × (ω × r)
```

* `a_body` — inertial acceleration of the body-frame **origin** (the system CG),
  in body axes.
* `α`, `ω` — body-frame angular acceleration / velocity.

The Euler (transport / Coriolis) terms are absent because `r` is *constant in
the body frame*: a rigidly-held rider has no velocity relative to the machine.

Applied to the rider:

```
r_G   = (0, kRiderSeatOffY_m + rider_up_m, kRiderSeatOffZ_m - rider_fwd_m) - cg_off
      = seated offset  +  0.735650 * d        (§1.1; d = (-lat, up, -fwd))
a_G   = a_body + α × r_G + ω × (ω × r_G)
α_r   = α                                      (rigid: same angular acceleration)
```

and to each contact station `r_i` (machine-fixed):

```
r_i   = nominal_body(§2.5)  -  cg_off
```

**Define the demand vector** — what the contacts must supply, per kilogram:

```
A ≡ a_G - g_body                    [m/s^2, body axes]
```

so that `Σ F_contact = m_r * A`. `g_body = Rᵀ * (-9.80665 * up_cg)`.

*Sanity, at rest:* `a_body = 0`, `α = ω = 0`, `g_body = (0, −9.80665, 0)`
⇒ `A = (0, +9.80665, 0)` ⇒ `ΣF = (0, +858.08, 0) N` — the contacts hold him up.
Correct.

*Sanity, in free flight:* `a_body = g`, and if `α = ω = 0` then `A = 0` and **no
contact carries anything.** A rigid rider in free-fall with the machine needs no
force — correct, and it is why the `α × r` and `ω × (ω × r)` terms are not
optional. **In flight they ARE the whole model.** That is superman.

---

## 4. THE THREE EQUATIONS, AND THE FOUR CASES

### 4.1 Unknowns

| contact | scalars | sign |
|---|---|---|
| **hands** at ζ_h | `H_ζ`, `H_η` (+ `M_h` in CASE F only) | **bilateral** — any sign |
| **seat** at ζ_s | `N_s` along body +η | **unilateral**, `N_s ≥ 0` |
| **boots** at ζ_b | `N_b` along body +η | **unilateral**, `N_b ≥ 0` |

**The seat and the boots are frictionless.** This is a modelling choice, and it
is the *conservative* one: with no tangential force below, **the whole fore-aft
demand goes to the hands**. Adding a boot-friction coefficient `μ` would (a)
introduce exactly the invented dial this rung forbids, and (b) only ever
*reduce* `grip_load_n`. `μ = 0` is therefore not a guess — it is the bound.
See §8, open item 1.

**The hands carry no couple in CASES S / A / B.** A fist on a round bar is
modelled as a pin. This is also conservative: with no wrist couple, the pitch
balance must be carried by the hand *force* through its 0.626 m arm, which makes
the reported force **larger**, not smaller. The couple appears only in CASE F,
where the hands are the sole contact and a pin cannot hold a rigid body — and
that couple **is** *"he pivots up over the bars."*

### 4.2 The equations

Moments are taken **about the rider CG**. For a force `F = (F_ζ, F_η)` at body
offset `(dζ, dη)` from the CG, the pitch moment about +X is

```
M_x = dη * F_ζ  -  dζ * F_η
```

```
(1)  ζ :   H_ζ                            = m_r A_ζ
(2)  η :   H_η + N_s + N_b                = m_r A_η                 ≡ V
(3)  θ :   dη_h H_ζ - dζ_h H_η
             - dζ_s N_s - dζ_b N_b        = I_pitch * α_x           (planar)
```

Note (1) is **decoupled and closed-form in every case**:

```
H_ζ = m_r * A_ζ          always
```

Define, once per substep:

```
V = m_r * A_η
K = I_pitch * α_x  -  dη_h * H_ζ       (the pitch demand the vertical forces
                                        must supply)
```
Then (2) and (3) become, with `dζ_h = −0.62634`, `dζ_s = +0.22750`,
`dζ_b = −0.45938`:

```
(2)  H_η + N_s + N_b = V
(3)  -dζ_h H_η - dζ_s N_s - dζ_b N_b = K
```

Two equations, three unknowns. **That is the whole of the indeterminacy** — and
§4.3 removes it with a physical statement, not a ratio.

### 4.3 THE LUMP, AND WHY THE REDUNDANCY DOES NOT REACH THE INSTRUMENT

`N_s` and `N_b` are both **vertical** and both **unilateral**. Their resultant is
therefore a single vertical force `N_m ≥ 0` acting at a station `ζ_m`, and — for
non-negative `N_s`, `N_b` — that station lies **exactly** in
`[ζ_b, ζ_s] = [−0.10204, +0.58484]`:

```
N_m  = N_s + N_b
ζ_m  = (ζ_s N_s + ζ_b N_b) / N_m
```

This is a **bijection**, not an approximation: given `(N_m, ζ_m)` inside the
interval, the lever rule inverts it uniquely and non-negatively —

```
N_s = N_m (ζ_m - ζ_b)/(ζ_s - ζ_b)
N_b = N_m (ζ_s - ζ_m)/(ζ_s - ζ_b)        ζ_s - ζ_b = 0.68688 m
```

So the two lower unknowns are exactly `(N_m, ζ_m)`, and (2)+(3) are two
equations in **three** unknowns `(N_m, ζ_m, H_η)`. The **one** remaining
redundancy is `H_η` alone.

★★★ **THE CLOSURE.** In the regime where the machine's own support can balance
him — i.e. `N_m > 0` and `ζ_m` lands inside `[ζ_b, ζ_s]` — a rigid rider
**requires no vertical hand force at all**. Set `H_η = 0` and the system is
exactly determinate. Any non-zero `H_η` there is a *self-stress*: a man choosing
to pull on the bars. That is a muscle decision, not a kinematic requirement, and
this instrument measures the **kinematically required** load. When `ζ_m` cannot
land inside the interval, `H_η` is **forced**, and the model reports it.

The consequence worth stating: **the redundancy that made the naive model
indeterminate lives entirely inside CASE S, and inside CASE S it does not touch
`grip_load_n`.** The instrument is unique everywhere it is non-zero.

### 4.4 The enumeration — four closed-form cases, every one 3×3

```
STEP 0.   H_ζ = m_r A_ζ                  (equation 1, always)
          V   = m_r A_η
          K   = I_pitch α_x - dη_h H_ζ

STEP 1.   if V <= 0                                 -> CASE F
          ζ_m = ζ_G - K / V                         (from -(ζ_m-ζ_G) V = K)

STEP 2.   if  ζ_b <= ζ_m <= ζ_s                     -> CASE S
          if  ζ_m >  ζ_s                            -> CASE A
          if  ζ_m <  ζ_b                            -> CASE B
```

**CASE S — SEATED (the common case).** The machine holds him; the hands carry
shear only.
```
H_η = 0
N_m = V
N_s = N_m (ζ_m - ζ_b)/(ζ_s - ζ_b)    >= 0 automatically
N_b = N_m (ζ_s - ζ_m)/(ζ_s - ζ_b)    >= 0 automatically
grip force = (H_ζ, 0)
```

**CASE A — SEAT EDGE.** The demand wants the machine to push AFT of the seat; it
cannot. Pin the support at the seat, hands take the rest.
```
d_s = ζ_s - ζ_G = +0.22750 ;  d_h = ζ_h - ζ_G = -0.62634
H_η = (K + d_s V) / (d_s - d_h)            d_s - d_h = ζ_s - ζ_h = +0.85384
N_s = V - H_η       ;   N_b = 0
```
`ζ_m > ζ_s` implies `K + d_s V < 0`, hence `H_η < 0` (the hands **pull down**)
and `N_s = V - H_η > V > 0`. **CASE A can never re-drop** — proved, not guarded.

**CASE B — BOARD EDGE.** The demand wants the machine to push FORWARD of the
boots; it cannot. Pin at the boots.
```
d_b = ζ_b - ζ_G = -0.45938
H_η = (K + d_b V) / (d_b - d_h)            d_b - d_h = ζ_b - ζ_h = +0.16696
N_b = V - H_η       ;   N_s = 0
if N_b < 0  ->  CASE F                     (this check IS needed)
```
⚠ `d_b - d_h = 0.167 m` is the **smallest denominator in the model** — the boots
and the grips are only 167 mm apart fore-aft. CASE B is therefore the
numerically stiff branch: a 1 mm error in either station is a 0.6 % error in
`H_η`. It is well-conditioned (never singular), but this is the case to watch in
a probe trace, and the case a station re-measurement moves most.

**CASE F — FREE (superman, and the throw).** Nothing below carries. The hands
are the only contact, and a pin cannot hold a rigid body in pitch, so the wrists
supply a couple.
```
N_s = N_b = 0
H_η = V
M_h = I_pitch α_x  -  ( dη_h H_ζ  -  dζ_h H_η )
```

Four cases. Each is a division and a multiply. No iteration, no tolerance, no
convergence check; deterministic by construction.

### 4.5 What the planar reduction drops, stated

* **The gyroscopic term.** The full moment equation is
  `M = I_G α + ω × (I_G ω)`; the planar model keeps only `I_pitch α_x`. The
  dropped X-component is `ω_y ω_z (I_roll − I_yaw)`. With the §2.4 numbers
  `I_roll − I_yaw = −0.0136 kg m²` — **the rider's yaw and roll inertias are
  equal to within 0.23 %**, so this term is essentially zero for *this* body in
  *this* pose. That is a measurement, not an assumption, and it is why the
  planar drop is cheap here. Re-check it if the pose changes materially.
* **Lateral / roll entirely.** Deferred by Chad's own R4a ruling. The instrument
  is blind to a side-throw and must not be read as if it were not.
* **Limb compliance.** §6.

---

## 5. THE INSTRUMENT

### 5.1 What is measured

| field | units | meaning |
|---|---|---|
| `grip_load_n` | N | `hypot(H_ζ, H_η)` — the force through **both hands combined**, instantaneous, this substep |
| `grip_load_lp` | N | the same, low-passed, τ = 0.1 s (§5.3) |
| `grip_moment_nm` | N·m | `M_h`, the wrist couple. **Non-zero only in CASE F** — this is the "pivots up over the bars" quantity the point mass could not see |
| `grip_f_aft_n` | N | `H_ζ`, signed, + = AFT |
| `grip_f_up_n` | N | `H_η`, signed, + = UP |
| `seat_load_n` | N | `N_s` |
| `board_load_n` | N | `N_b` |
| `support_n` | N | `N_m` = `N_s + N_b` |
| `support_z_m` | m | `ζ_m`, the support resultant's station — **the state variable of the enumeration**; watching it walk out of `[−0.102, +0.585]` IS the buck-off |
| `rider_case` | int | 0 = S, 1 = A, 2 = B, 3 = F |
| `rider_alpha_x` | rad/s² | the `α_x` consumed |
| `rider_acc_body[3]` | m/s² | `A`, the demand vector |

`seat_load_frac` / `board_load_frac` of §7.7 fall out later as
`N_s / (m_r g)` and `N_b / (m_r g)` — **not defined here**, because R4a Phase 0
adds no kernel state.

### 5.2 ★★ WHY THE INSTRUMENT IS `grip_load_lp` AND NOT A PEAK

`grip_load_n` is sampled **per substep**:

```
h = dt / p.substeps                       sim/sled.cpp:199-200
p.substeps = 12                           sim/sled.h:968
```
and `dt` is **not the same on the two paths**:

| path | dt | source | h |
|---|---|---|---|
| the game | 1/120 s | `config/aircraft.toml:10` (`sim_dt`) → `params.sim_dt` → `app/main.cpp:3337` | **0.694 ms** |
| tapes / fixtures | per-tape (commonly 1/60) | `test/harness/sled_tape.h:140` writes `dt` and `substeps` into the header | **1.389 ms** |

So the peak of `grip_load_n` is an artefact of a sampling rate that **already
differs by 2× between the game and the gate**, and `substeps` is a **taped
parameter** (`test/harness/sled_tape.h:140, 160, 370, 404`). A threshold pinned
to a peak would silently move the moment anyone retunes `substeps` — and nothing
would go red. That is *the law* this repo has paid for five times: *a constant
that describes the shipped table stops describing it the moment the table
moves.*

**Therefore `grip_capacity_n` is compared against `grip_load_lp`, never against
`grip_load_n`.** `grip_load_n` is recorded for attribution only.

### 5.3 The low-pass — the house pattern, and its one wrinkle

The house pattern is `SledState::hull_engage_lp` (`sim/sled.cpp:1462-1473`):

```cpp
s.hull_engage_lp = slew_toward(s.hull_engage_lp, engage_target, 0.1, 1e9, h);
```
`slew_toward` is `sim/sled.cpp:87`; `tau = 0.1 s`, `rate_cap = 1e9` (unbounded —
the filter is the only shaping), `h` is the **substep**, never `dt`.

Use exactly that: `tau = 0.1 s`, `rate_cap = 1e9`, `h`.

⚠ **The wrinkle, measured:** every caller clears the sink each tick —
`test/harness/sled_tape.h:509` and `tools/sled_probe.cpp:2839` both do
`sink.substeps.clear()`. A filter whose state lives in `SledDebugSubstep` and is
seeded from `substeps.back()` therefore **restarts every tick**, and a 0.1 s τ
over a 16.7 ms window only travels 15 % — it would not be a low-pass at all.

Two ways out. **The compliant one is (a).**

* **(a) — RECOMMENDED, zero extra `sim/` surface.** The sink carries
  `grip_load_n` only. The **consumer** (`tools/sled_probe.cpp`) owns the
  continuous filter, running the identical
  `slew_toward(lp, grip_load_n, 0.1, 1e9, h)` across the whole run. Also write
  `grip_load_lp` into the substep record for the intra-tick trace, seeded from
  `substeps.back().grip_load_lp` when non-empty and from `grip_load_n` on the
  first substep of a tick — **and label it in the header comment as
  intra-tick-only**, so nobody thresholds against it.
* **(b) — one line, but it is a QUESTION, not a decision.** Put
  `double grip_load_lp = 0.0;` on **`SledDebugSink`** (not the substep). Both
  callers clear `substeps` but keep the sink object, so it would be continuous
  and correct. **This is outside the stated constraint** ("add fields to
  `SledDebugSubstep`"), so it is surfaced here rather than taken. It remains
  zero-risk in the same sense — the sink is null on the shipped path.

---

## 6. ★★★ THE BIAS, ON THE RECORD

**This model computes the load required to hold the rider RIGIDLY. Real limbs
comply. Every number it reports is a CONSERVATIVE UPPER BOUND.**

The compliance is not hypothetical and it is not unmeasured — it already ships:
`render::rider_absorb()` (`render/rider_pose.cpp:109`) returns a `[0,1)` absorb
fraction from the suspension's own compression and rate, with a measured,
saturating distribution (`kAbsorbHoldOnFrac`, `kAbsorbHoldHalfFrac`,
`kAbsorbHitHalfMs`, `kAbsorbHoldWeight`, `kAbsorbHitWeight` —
`render/rider_pose.h`). A real rider's knees, hips, elbows and shoulders take a
large fraction of a spike before the grip ever sees it. This model gives them
**zero** travel.

Three named conservatisms stack in the same direction:

1. **Rigid limbs.** No absorb. The largest of the three, and unquantified here.
2. **Frictionless seat and boots (§4.1).** The whole fore-aft demand is routed to
   the hands. Adding boot friction can only reduce `grip_load_n`.
3. **No wrist couple in CASES S / A / B (§4.1).** The pitch balance must go
   through the hand *force* on a 0.626 m arm rather than through a couple, which
   inflates the force.

One conservatism runs the **other** way and is stated so it is not hidden:

4. **`I_pitch` is a slender-rod lower bound (§2.4)**, so the pitch-driven part of
   the demand is slightly *under*-reported — bounded at ≤ 14.2 % of `I_pitch`,
   and only where `α_x` dominates.

**Consequence for the dial:** when Chad tunes `grip_capacity_n` against these
numbers, the capacity he lands on will be **larger** than a real miner's hands.
That is fine and intended — the capacity is a felt call against *this*
instrument's scale, not a physiological constant, and §7.9 says outright that
nobody self-passes it. But it must **never** be quoted as "a man can hold N
newtons." It cannot be compared to a grip-strength table. Say so wherever the
number is written down.

---

## 7. IMPLEMENTATION NOTES FOR THE NEXT AGENT

### 7.1 Where the code goes

The sink's single write site is `sim/sled.cpp:1594-1627` (`if (dbg) { ... }`),
and it sits **before** gravity and drag are added to `force`
(`sim/sled.cpp:1629-1636`) and **before** `alpha_body` is computed
(`sim/sled.cpp:1646-1648`). The rider model needs both.

**Add a SECOND `if (dbg)` block** immediately after

```cpp
const glm::dvec3 alpha_body = (torque_body - glm::cross(s.angular_vel, Iw)) / I;
```
and **before** `s.angular_vel += alpha_body * h;`, writing into
`dbg->substeps.back()`. At that point, in scope and complete:

| need | expression | frame |
|---|---|---|
| `a_body` | `glm::transpose(R) * (force / mass)` | body — `force` is WORLD and already carries gravity + drag |
| `g_body` | `glm::transpose(R) * (-9.80665 * up_cg)` | body |
| `ω` | `s.angular_vel` (pre-update) | body — `sim/sled.h:981` |
| `α` | `alpha_body` | body |
| `cg_off` | in scope from `sim/sled.cpp:399` | body |
| `m_r` | `p.rider_mass_kg` | — |

`force / mass` is the **system** CG acceleration and `mass = p.mass_kg = 331.0`
includes the rider — which is correct: the body-frame origin *is* the system CG,
and this is the machine's actually-measured motion.

The `if (dbg)` placement is what makes this zero-risk: **the sink is null on the
shipped path**, and `sled_debug_sink_is_write_only` is the standing proof
(handoff §2 — 14402 of its 14403 assertions pass, and the single red is a
non-vacuity check on the GI4 release band, not a firewall failure).

### 7.2 Constants

Declare the measured constants as `static constexpr double` **inside** the
`if (dbg)` block, so nothing new exists on the shipped path:
`kIPitch 7.09064`, `kGripZ -0.26900`, `kGripY +0.32270`, `kSeatZ +0.58484`,
`kBootZ -0.10204`; read the mass as `p.rider_mass_kg` and the rider's seated
offset from the existing `kRiderSeatOffY_m` / `kRiderSeatOffZ_m` in `sim/sled.h`
rather than re-typing them.

> **QUESTION, not a decision.** A pure, unit-testable free solver
> (`sim/rider_load.h`, header-only, `glm` + `std` only, included only from the
> `if (dbg)` block) is what the house standard would normally want — this repo
> requires mutation-verified test legs, and a 30-line block buried inside a
> substep loop is hard to mutate-test. But it adds a **file** to `sim/`, which is
> outside the stated constraint. **Ask before taking it.** If refused, inline it
> and test through `SledDebugSink` end-to-end instead.

### 7.3 The hard constraints this rung inherits

* `sim/` changes are **fields on `SledDebugSubstep`, computed inside
  `if (dbg)`** — nothing else. No kernel default, no `SledParams` value, no
  golden.
* **Nothing goes on the tape PIN roster.** It is **positional**, and a 42nd
  double breaks all 19 of Chad's tapes (handoff §6). Derived kernel fields are
  **not taped** — Chad's own ruling.
* No `render/`, no `assets/`, no `indy650.blend`, no GLB.
* Build the game: `cmake --build build-play --target seads`.
  Build tests:      `cmake --build build --target seads_tests`.
  **Never two ctest gates against one build dir at once.**
* Regenerate `generated/graph/` in the same commit as any structural change
  (standing law 2).

### 7.4 Verification targets — reproduce these before believing anything

Reproduce §2.3 and §2.4 first; if `rider_cg` does not come out at
`(0.00033, 0.81774, −0.35734)` the GLB read or the frame map is wrong and
nothing downstream is worth reading.

Then three hand-computable cases:

**(i) AT REST.** `a_body = 0`, `α = ω = 0`, machine level.
```
A = (0, +9.80665, 0)   V = 858.08 N   H_ζ = 0   K = 0
ζ_m = ζ_G = +0.35734   -> inside [-0.10204, +0.58484]   -> CASE S
N_m = 858.08 N ; N_s = 573.9 N ; N_b = 284.2 N
grip_load_n = 0.0
```
★ **This is the leg that would have caught the refuted model.** A "support pinned
at the seat, hands take the residual" solve at rest reports a standing
**228.6 N** through the hands (`N_s = 2.75297 H_η`, `3.75297 H_η = 858.08`). A
man sitting still holds **nothing**.

**(ii) PURE VERTICAL BUMP, no rotation.** `a_body = (0, +3g, 0)`, `α = 0`.
```
A = (0, +39.227, 0)   V = 3432.3 N   K = 0   ζ_m = ζ_G  -> CASE S
grip_load_n = 0.0
```
★ This is **correct**, and it is the red team's point from the other side: a big
*symmetric* upward bump does **not** load the hands. What loads them is **pitch**
and **fall-away**. An instrument that lit up here would be measuring the wrong
thing.

**(iii) AIRBORNE, PITCHING NOSE-DOWN — the throw.** `a_body = g` (free-fall),
`α_x = −10 rad/s²`, `ω_x = −4 rad/s`, `r_G = (0, 0.35374, 0.35734)`.
```
α × r_G          = (0, +3.5734, -3.5374)
ω × (ω × r_G)    = (0, -5.6598, -5.7174)
A = a_G - g      = (0, -2.0864, -9.2548) m/s^2
V = m_r A_η      = -182.6 N   <= 0                     -> CASE F
H_ζ = m_r A_ζ    = -809.8 N   (forward)
H_η = V          = -182.6 N   (down)
M_h = I α_x - (dη_h H_ζ - dζ_h H_η)
    = -70.91 - ( (-0.03104)(-809.8) - (-0.62634)(-182.6) )
    = -70.91 - ( 25.14 - 114.37 ) = +18.32 N m
grip_load_n    = hypot(809.8, 182.6) = 830.1 N   (187 lbf, both hands)
grip_moment_nm = +18.3 N m
```
★★★ Note what happened: `a_body` is **exactly free-fall**, so a point-mass model
would report **zero**. The entire 830 N comes from `α × r` and `ω × (ω × r)` —
the station terms. **That is the quantity the point mass deleted.**

### 7.5 The mutation legs to write

Per the house standard, each new test leg must be **mutation-verified red**:

1. Flip the sign of `dζ_h` — must break (i) or (iii).
2. Replace the boot station with the `board_socket` (the 272 mm trap of §2.5) —
   must move `ζ_m`'s interval and flip a case in a trace.
3. Delete the `ω × (ω × r_G)` term — must break (iii) and **only** (iii).
4. Delete the `α × r_G` term — must break (iii).
5. Hard-code CASE S — must break (iii).
6. Use `dt` instead of `h` in the low-pass — must break a τ-response leg.
7. Zero `I_pitch` — must break the CASE F moment.
8. Verify `sled_debug_sink_is_write_only` still passes its 14402 bit-identity
   assertions. The single pre-existing red is the GI4 release-band non-vacuity
   check and is unrelated — **prove that by revert-and-compare, do not assert
   it.**

---

## 8. OPEN ITEMS — ASK, DO NOT DECIDE

1. **Boot friction.** §4.1 models the boots frictionless. Real boots grip. Adding
   `μ_boot` re-introduces an indeterminate unknown *and* a dial. Recommendation:
   leave it at zero (the conservative bound) for Phase 0 and revisit only if a
   trace shows implausible fore-aft hand loads on the flat.
2. **`grip_load_lp`'s home.** §5.3 (a) vs (b). (b) is one line on
   `SledDebugSink` and is the physically right filter; it is outside the stated
   constraint.
3. **A testable free solver in `sim/rider_load.h`.** §7.2.
4. **The sole's centre of pressure** vs the ankle node, §2.5. Not measured; the
   ankle is the conservative choice.
5. **`I_own`'s slender-rod bound**, §2.4. Closeable by measuring skin-weighted
   bone radii off `sudburian_proxy`; leverage bounded at 14.2 %.
6. **`OPEN-KWS1-SAG`**, §1.2 — pre-existing, named in `sim/sled.h`, inherited
   not created.
