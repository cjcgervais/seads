# NEW-ASK MECHANISM CHECK — 2026-09-18

**Round 2 of the sled-ride audit.** Chad's five new asks of 2026-09-18 morning, each traced to the
exact mechanism in the shipped tree and answered with ONE dial in the fixed rung shape.

**Worktree** `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY.** No dial moved, no config edited, no golden touched, nothing built, no `ctest`, no
`seads.exe`, no tape opened this round (this is a CODE-ONLY pass — every number below is arithmetic
on the shipped source, or a MEASURED figure carried forward from round 1 with its source named).
**Files touched by this round: this file only.**

**Confidence vocabulary** MEASURED / DERIVED / LITERATURE / GUESS, labelled every time.
**Every numeric claim names its killing mutation.**

---

## 0. HIS WORDS, VERBATIM

From `docs/sled_audit/RULINGS_20260918_chad.md`, untouched:

> "86 to 91 all tapes are free riding, often rolling when hitting banks, throttle is all or nothing
> ive mitigated this by tapping and applying brake. Some of the being upside down on the ground is
> due to deliberate jumps for performing a flip off of a banks for fun but maybe only 10% of the time
> its a deliberate hit the bank hard to see what happens, other times its just going down the road
> and a ski hitting the bank on one side. a backflip sometime deliberate is not a roll over, throttle
> is cut unless im key pressing but if I key press throttle should ramp up, self righting with a
> press and I want to be able to self right by rocking bodyweight back an fourth while pressing
> stand on and off, gain pendulum momentum (not automatic re righting). I can only really turn
> sharply if I alternate gas/brake, but throttle should also be able to swing my tail around on
> account of the roost, esp with weight shifting of the sudburian.... Need to make it more dynamic
> with bodyweight (also for jump weight shift influence in air)... please continue automatically I
> have to go to work... bye and good luck. pace so as not to run out of tokens per session limit."

---

## 0b. THE FIVE ANSWERS IN ONE TABLE

| # | his words | mechanism, file:line | THE ONE DIAL | identity | plumbing |
|---|---|---|---|---|---|
| **N1** | *"if I key press throttle should ramp up"* (while rolled) | `sim/sled.cpp:292-293` — `s.rolled` forces the throttle to 0.0 | `[sled_comfort] rolled_throttle_frac` | **0.0** | `config/` — SledComfort is TOML-reachable today |
| **N2** | *"self right by rocking bodyweight back an fourth … gain pendulum momentum"* | `sim/sled.cpp:401-406` — the kernel's own brace **saturates the reach-box clamp** and takes his body away exactly while he shoves | `[sled_comfort] right_stand_shift_frac` | **1.0** (shipped) | **none — TOML today**, `config/scenario.toml:2115` |
| **N3** | *"throttle should … swing my tail around on account of the roost, esp with weight shifting"* | `sim/sled.cpp:1056` — the track's lateral μ is **0.70 flat**; there is no friction circle, so spinning the track costs it **no** sideways grip | `track_lat_slip_shed` | **0.0** | `sim/` — six-touch RC-8 plumbing + a 6-line hoist |
| **N4** | *"throttle is all or nothing ive mitigated this by tapping and applying brake"* | `sim/sled.cpp:1143-1211` — slip saturates the Janosi shear in ~0.1 of slip and the **drawbar cap `max_thrust_n` flattens whatever is left**; the thumb is a three-position switch | `throttle_power_frac` | **0.0** | `sim/` — six-touch RC-8 plumbing |
| **N5** | *"jump weight shift influence in air"* | `sim/sled.cpp:2096-2098` — K2 `k_air_shift`, built, gated, **ships at 0.0 with no loader path** | `k_air_shift` | **0.0** | `sim/` — six-touch RC-8 plumbing |

**⚠ Plumbing fact, re-verified this round.** `grep -n "sled_params\." app/main.cpp` shows exactly ONE
TOML→params bridge: `app/main.cpp:2477` `sled_params.comfort = scen.sled_comfort`. Everything else
the app writes onto `SledParams` is an env override or a world temperature. **A field living directly
on `SledParams` (`k_air_shift`, `traction_mu`, `plane_lat_lean_gain`, `shear_K_m`, `max_thrust_n`,
`track_lat_mu`, `slip_ref_rad`, `k_gyro`, `k_gyro_react`) has NO loader path at all** — it is the
six-touch RC-8 job the packet §3 names. A field on `SledComfort` is reachable from
`config/scenario.toml` **today**. N1 and N2 are therefore the two cheap ones.
*Killing mutation:* a second `sled_params.<field> = ` assignment anywhere in `app/` or `config/`.
MEASURED by grep over `app/main.cpp` and `config/`.

---

## N1 — "IF I KEY PRESS THROTTLE SHOULD RAMP UP" (while rolled)

### The felt problem, his words
> *"throttle is cut unless im key pressing but if I key press throttle should ramp up"*

### The mechanism — and the answer to "app or sim?"

`sim/sled.cpp:292-293`:

```cpp
const double throttle =
    (s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle);
```

**The app already does what he asks and the kernel already throws it away.** `app/main.cpp:8291-8293`
ramps the thumb at **2.5/s up, 6.0/s down** and `app/main.cpp:8447` ships it as `sin_.throttle`
unconditionally — the app never reads `sled.rolled` on this path. So `in.throttle` is *already* a
ramped, non-zero command while he is over. The kernel's `s.rolled` branch is the only thing between
his thumb and the track.

**⇒ The change is in `sim/`, and it cannot be anywhere else.** The app cannot undo a zero applied
downstream of it. An app-side "fix" would have to be a second, parallel throttle path — which is the
lying-instrument shape the kernel forbids by name. **DERIVED, structural.**

**And the blast radius is bigger than thrust.** `throttle` at :292 is read by five consumers:
the CVT drive blend (`:1137`), the commanded track speed (`:1140`), `roost_thrust` through `flux`
(`:1156, :1182`), the engine-brake gate (`:1234`), and the **belt/rpm readout** at `:1341-1361`
(`rep_rpm = 1700 + rpm_frac·6300`). So today, when he is on his side and presses W, the machine does
not just fail to move — **it stays silent at idle rpm and throws no roost.** There is no feedback of
any kind that the key was received. That is the whole felt signature of *"throttle is cut"*.
DERIVED, exact: `rpm_frac` is a function of `throttle` and `rep_belt` only.

### The persistence half, which is already his
`rolled` is not instantaneous: `sim/sled.cpp:246-259` requires the >75° attitude to hold for
`rolled_persist_s` (0.30 s) **while in ground contact within `rolled_grace_s`**, so a backflip does
not latch it. His ruling R4 (*"a backflip sometime deliberate is not a roll over"*) is already
honoured **in the kernel**; it is the audit's *counting* that still conflates the two (packet §6-B2).
MEASURED by reading the branch; the pinning leg is
`test/unit/test_sled.cpp:2521 sled_rolled_readout_never_latches_airborne`.

### THE ONE DIAL

**`[sled_comfort] rolled_throttle_frac`, identity `0.0`.**

```cpp
const double thr_in = hands_on ? clamp01(in.throttle) : 0.0;
const double throttle =
    s.rolled ? p.comfort.rolled_throttle_frac * thr_in : thr_in;
```

At `0.0` this is **bit-identical** to the shipped line, term for term, for every tape and every
golden — the same `0.0` arrives by the same arithmetic. `!hands_on` still forces zero and **must**:
a man who has been thrown off has no thumb on the lever (R4a §7.4), and that half of the branch is
not this dial's business.

**Range** `0.0 .. 1.0`. **Suggested A/B** `0.35` — enough track speed to spin the track, throw roost
and make noise on his side, not enough to drive the machine across the snow on its roof. **GUESS on
the value; DERIVED on the shape.** Per NO GUESSING the number is his drive, not this document's.

### Invariant + killing mutation
- **Invariant (two-sided).** (a) With `rolled_throttle_frac = 0.0` the whole v17 tape corpus replays
  **byte-identical** — `sled_tape_round_trip_replays_bit_identical`. (b) With it non-zero,
  `test/unit/test_sled.cpp:2649 sled_onside_recovery_is_momentum_not_magnetism` **must still pass**:
  a downed machine that drives itself upright on track thrust is the magnetism cheat the kernel
  forbids by name. **This is the wall for this rung.**
- **Killing mutation.** Set the dial to 0.0 → the branch is arithmetically dead and the rung is
  cosmetic. Set it to 1.0 and watch `never_recovered`: if it *falls*, the throttle is righting the
  machine and the rung has become Rung 4 by the back door.
- **The honest limit.** A spinning track on a machine lying on its side applies its thrust at
  `tan_mount`, which is `cg_height_m` off the CG — so it *will* produce a roll moment. That is
  physics, not a cheat, but it is exactly the boundary the anti-magnetism leg polices, and it is why
  the value has to be small and his.

### Drive line
> Roll her deliberately off a bank. On your side, hold W for two full seconds. **Today:** silence,
> idle rpm, no roost, no motion. **With the dial:** the track spins, she makes noise and throws snow,
> and she may or may not walk herself around. Tell me whether the *noise and the roost alone* are
> what you were asking for, or whether you want her to actually move.

### Law check
- **NO GOVERNOR** — clear. Nothing here reads or moves the rider's mass. This dial **removes** a
  kernel-imposed zero and hands authority back to the player; it is the opposite of a governor.
  (`sim/sled.h:183`: *"What stays forbidden: a governor … and any `if(angle>X)` that scripts an
  outcome."* This rung **deletes** an `if(rolled)` that scripts an outcome.)
- **Wheelie is the feature** — untouched. The dial is inert unless `s.rolled`, and `rolled` cannot
  latch airborne.
- **Depth 0.77 m** — untouched.

---

## N2 — PENDULUM SELF-RIGHT: "ROCKING BODYWEIGHT BACK AND FORTH WHILE PRESSING STAND ON AND OFF"

### The felt problem, his words
> *"self righting with a press and I want to be able to self right by rocking bodyweight back an
> fourth while pressing stand on and off, gain pendulum momentum (not automatic re righting)"*

### Does rocking already do this? **HALF of it does, and it has since the seated self-right v2.**

The block is `sim/sled.cpp:1679-1798`, live at `right_assist_nm = 2400.0`
(`config/scenario.toml:2085` — note `sim/sled.h:224` defaults it to 0.0, so the **TOML is what arms
it**; anyone reading the header alone will conclude the mechanic is off).

**What already exists, verbatim from the source:**

1. **The pusher tires — press/release IS required.** `sim/sled.cpp:1712-1717`:
   ```cpp
   s.right_charge += h * ((1.0 - p_eff) * (1.0 - s.right_charge) / tau_rest
                          - p_eff * s.right_charge / tau_push);
   ```
   A contracting first-order ODE: holding STAND drains the budget with `right_charge_push_s = 0.6 s`
   (`config/scenario.toml:2100`), releasing refills it with `right_charge_rest_s = 0.7 s`. *"An
   infinite press injects FINITE energy and then nothing."* **A held key cannot beat a cadence.**
2. **The push is phase-locked to the swing.** `sim/sled.cpp:1774-1775`:
   ```cpp
   const double align = s.angular_vel.z * dir / om_eps;   // om_eps = 0.35 rad/s
   const double w_pump = 0.5 * (1.0 + std::tanh(align));
   ```
   Pushing **with** the roll rate gives up to 1.0; against it, ~0.0; at rest, exactly 0.5. This is
   textbook parametric pumping. The source says why it exists: *"Without this the torque nets ~zero
   work per cycle and no number of presses beats one — measured."*
3. **The kernel's own gate already tests the cadence.** `test/unit/test_sled_selfright.cpp:240`
   `"selfright: a committed press beats mashing"` — a 12 Hz mash fails at >90° where a 1.0 s/0.7 s
   cadence rights it.

**⇒ "gain pendulum momentum by pressing stand on and off" is BUILT, SHIPPED and GATED.** DERIVED
from the source, MEASURED by the existing legs.

### Why it does not read that way to his hand — four reasons, in order of size

**(i) THE BODYWEIGHT HALF IS SWALLOWED BY A CLAMP. This is the finding.**

`sim/sled.cpp:401-406`:

```cpp
const double lat_target_sr =
    std::clamp(target_lat_m + s.right_shift_cmd *
                                  p.comfort.right_stand_shift_frac *
                                  p.lean_lat_stand_m,
               -p.lean_lat_stand_m, p.lean_lat_stand_m);
```

with `right_stand_shift_frac = 1.0` (`config/scenario.toml:2115`), `lean_lat_stand_m = 0.35 m`
(`sim/sled.h:787`), and `s.right_shift_cmd = side * p_eff * s.right_charge`
(`sim/sled.cpp:1791`), which reaches **±1.0** at a full STAND, full tilt, full charge.

**Arithmetic (DERIVED, exact):** at the top of a fresh press the brace term alone is
`1.0 × 1.0 × 0.35 = ±0.35 m` — **the entire reach box**. `target_lat_m = lean_lat_cmd × lat_reach_m`
is at most `±0.35 m` standing (`lat_reach_m` opens from `lean_lat_seated_m 0.15` to
`lean_lat_stand_m 0.35` with the rise, `sim/sled.cpp:329-332`). So:

| his rocking, during a full press | what the clamp does |
|---|---|
| same side as the brace | **discarded** — the sum is already at the clamp |
| opposite side | **subtracts from the brace** — his rock *fights* the shove |
| no lean | the brace has the whole box |

**So the two channels he named are ANTI-PHASED BY A CLAMP.** The kernel takes his body away at
exactly the instant he shoves, gives it back only as `right_charge` drains (over `tau_push = 0.6 s`)
and fully only on release, where `p_eff = 0` zeroes `right_shift_cmd` (`sim/sled.cpp:1795`).
**A pendulum is pumped by moving the mass IN PHASE with the swing. This kernel lets him move the
mass only in the half-cycle where he is NOT pushing.** That is precisely the difference between
"press a key and it comes up" and "rock it up" — and it is one clamp, not a missing system.

*Killing mutation for (i):* `right_stand_shift_frac = 0.0` → the brace vanishes and his lean owns the
box at all times; if the mechanic then feels like a pendulum, this diagnosis is confirmed and the
dial is the fix. If it feels *worse*, the brace was carrying the whole righting and the diagnosis is
dead — **that is the A/B, and it costs a TOML edit.**

**(ii) HIS ROCK HAS A REAL LEVER, AND IT IS SMALL.** It is not nothing: `sim/sled.cpp:1486`
`const glm::dvec3 ptc = pt - cg_off;` — **the hull contact points move with the rider's lean**, so a
downed machine's hull moment arms really do follow his body. Magnitude: `cg_off = rider_frac ×
displacement` with `rider_frac = 87.5/331 = 0.2644` (`sim/sled.cpp:436, 523`), so a full ±0.35 m
rock moves the arm by **±0.0925 m**. Against the machine's own 3 246.0 N that is **±300 N·m**, versus
gravity's tipping torque of **933.1 N·m** (packet §2f) and the assist's `right_assist_nm` of
**2 400 N·m**. DERIVED, exact arithmetic. **32 % of the gravity torque is a usable pendulum lever —
if he is allowed to apply it while pushing.**
*Killing mutation:* `side_hull_points = 0` (`sim/sled.cpp:1481-1482`) → no hull contact, no arm, and
the rock does nothing at all. Also: gravity and air drag act at the CG and **never** at `cg_off`
(`sim/sled.cpp:1927`), so the hull arm is the **only** path his rock has. That is worth knowing
before anyone proposes a bigger lean.

**(iii) THE ASSIST IS ARMED ON 2.6 % OF TICKS AND HE STANDS ON 25.6 %.** MEASURED, round 1,
packet §2(i), 86 578 v17 ticks: `stand > 0.5` = **25.6 %**; armed (`stand ∧ speed < 1.3889 m/s`) =
**2.62 %**; with the tilt ramp = **2.32 %**. `right_assist_max_ms = 1.3889 m/s` (5 km/h,
`config/scenario.toml:2088`) with re-arm at `×0.8 = 1.111 m/s`. Nine times out of ten, when he
presses STAND nothing in this block is even running.
*Killing mutation:* re-measure with `hands_on` pinned true (the round-1 figure is an upper bound
because `hands_on` is unpinned in the tape).

**(iv) "TIPPING OVER" IS MEASURED AGAINST THE PLANET.** `sim/sled.cpp:1681`
`tilt = acos(up_body.y)` — gravity tilt, not hill-relative. On a conformal 20° side-hill the gate
reads full authority (packet §2(i) → ladder Rung 7). A rock on a slope is graded against the wrong
angle. DERIVED.

### THE ONE DIAL

**`[sled_comfort] right_stand_shift_frac`, identity `1.0` (the shipped value).**

It is the only one of the four that **turns a channel ON** rather than retuning one already on, and
it is the only one that needs **zero plumbing**: the field exists (`sim/sled.h:340`), the loader
reads it (`config/load_scenario.cpp:532-533`), the TOML carries it
(`config/scenario.toml:2115`), and the tape pins it (`test/harness/sled_tape.h:87`).

**Range** `0.0 .. 1.0`. **Suggested A/B** `0.5` — the brace keeps half the box (his ruling
*"STANDING AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED"* is still honoured, at half strength) and his
rock gets **±0.175 m** of headroom = **±0.0462 m of hull arm = ±150 N·m**, a sixth of the gravity
torque, applied at a phase **he** chooses. DERIVED arithmetic; the value is his drive.

### Invariant + killing mutation
- **Invariant (two-sided).** (a) At `1.0` every self-right leg is **byte-identical**.
  (b) At `0.5`, `test/unit/test_sled_selfright.cpp:240` *"a committed press beats mashing"* must
  **still pass** — the cadence must still matter. (c) And the new one this rung is **for**:
  *"selfright: rights from full inversion at ANY lean"* (`:210`) must **start to discriminate** —
  a well-timed rock must right her in fewer seconds than a badly-timed one, from the same start.
  **No such leg exists today.** It is owed by this rung.
- **Killing mutation.** `right_stand_shift_frac = 1.0` → bit-identical, rung dead. `= 0.0` → the
  automatic brace is gone entirely, which **violates his own ruling** (*"standing automatically
  helps push you over righted"*), so 0.0 is the mutation that must FAIL a leg, not a candidate value.
- **⚠ THE WARNING THE TREE ALREADY WROTE ABOUT THIS EXACT DIAL.**
  `test/unit/test_sled_selfright.cpp:236-239`, verbatim:
  > *"⚠ At FULL lean mashing also succeeds now: the automatic stand-shift plus his own lean is enough
  > authority that cadence stops mattering. That is a real property of the shipped dials, recorded
  > rather than hidden, and it is **the thing to watch if the mechanic ever starts to feel free**."*
  **The kernel's own test file predicted this complaint.** Lowering the frac walks toward cadence
  mattering again, which is the direction he is asking for — and the recorded warning is the leg
  that proves it moved.

### Drive line
> On your side, press-and-release SHIFT on a rhythm — about one press per second — and **at the same
> time swing the mouse left-right in time with her rocking**. **Today:** the mouse does nothing while
> SHIFT is down, and only bites in the gaps. **At `right_stand_shift_frac = 0.5`:** your body is live
> the whole time and the rock should build. Tell me if the *timing* starts to matter — if a sloppy
> rhythm fails where a good one comes up, that is the pendulum you asked for.

### Law check
- **NO GOVERNOR** — this rung **moves toward the law**. `sim/sled.h:183`: *"nothing here reads or
  moves the rider's mass — the player's lean is the only thing that moves the rider."* The brace at
  `sim/sled.cpp:401-406` **is the kernel moving the rider's mass**, blessed by his
  *"standing automatically helps push you over righted"* ruling. Lowering the frac gives mass back to
  the player. It is the one rung in this document that removes kernel authority rather than adding.
- **Not automatic.** The block is inside `p_eff > 0`, which requires `in.stand > 0`. There is no
  path by which she rights herself without a press. His *"(not automatic re righting)"* is already
  the shipped law and this rung does not touch it.
- **Wheelie / depth 0.77** — untouched (`right_shift_cmd` is 0 in all ordinary riding).

---

## N3 — "THROTTLE SHOULD SWING MY TAIL AROUND ON ACCOUNT OF THE ROOST"

### The felt problem, his words
> *"I can only really turn sharply if I alternate gas/brake, but throttle should also be able to
> swing my tail around on account of the roost, esp with weight shifting of the sudburian"*

### The mechanism — why throttle cannot break the tail loose today

**(a) The track's lateral grip is a constant, and nothing the throttle does touches it.**
`sim/sled.cpp:1056-1060`:

```cpp
const double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;
double bite = -normal * mu_l * std::tanh(slip_ang / std::max(p.slip_ref_rad, kEps));
```

`track_lat_mu = 0.70` (`sim/sled.h:1193`), `slip_ref_rad = 0.300` (`sim/sled.h:1187`). The track's
sideways μ is the **same 0.70 whether the track is locked, rolling or spinning at 46 m/s**.
**There is no friction circle in this kernel** — packet §2(e), re-verified: `grep` finds only the
per-surface `mu_brake` budget (`sim/sled.cpp:1232-1246`), which caps *longitudinal* braking and never
charges lateral. **DERIVED, structural. This is the whole answer to his ask.** In a real machine the
tail comes around because the track spends its friction budget going forward and has none left going
sideways. Here the budget is infinite in one axis.

**(b) So the only thing throttle does to the rear is UNLOAD it, and that is second-order.**
`normal` is the one throttle-sensitive factor in the bite. Throttle lifts the nose (thrust at
`tan_mount`, `cg_height_m` below the CG) which *loads* the track — the wrong direction for a
slide — and planing lift unloads every patch together at speed (`sim/sled.h:1041-1055`). The audit's
own measurement: at the runaway attractor the running surfaces hold **62 N of 3 246.0 N**
(`sim/sled.h:970-975`) — at which point the track has no lateral bite *and no longitudinal one
either*, so she runs away straight rather than sliding the tail. MEASURED (probe `attractor`), and
it is why the felt answer is *"alternate gas/brake"*: the **brake** is the only control in the
machine that can break the rear loose, via `mu_brake`.

**(c) The weight-shift half already works and is pointed the wrong way for this ask.**
`lean_fwd`/aft moves `cg_off.z` and therefore the patch mounts (`sim/sled.cpp:523-527`,
`patch_geometry` at `:55-80`), so leaning aft **loads** the track and forward **unloads** it —
real weight transfer, emergent, no special term. DERIVED. But loading the track at a constant μ just
gives it *more* sideways grip. **Today, throwing your weight back makes the tail stick harder.**
That is the exact inverse of what he is describing on a real sled, where weight-back + throttle spins
the track and lets the rear step out.

**(d) The lean reward that exists is a grip multiplier, never a grip shedder.**
`sim/sled.cpp:1067` `bite *= 1.0 + lean_bite_gain * align_m` with `lean_bite_gain = 0.18`
(`sim/sled.h:404`) — and `align_m` is clamped ≥ 0 (`sim/sled.cpp:583-585`), the house rule that a
wrong-way lean is never a penalty. So **every existing lean path can only ADD bite**, including to
the track, which is the patch whose bite resists yaw. The tree already measured this as
self-defeating for an arc (0.18 → 0.45 took the lean-in carve from 30 m to 295–1037 m of radius,
`sim/sled.h:1057-1060`).

**(e) And the roost he names is already computed — it just never turns the machine.**
`sim/sled.cpp:1182-1184`:
```cpp
const double roost_thrust = p.roost_gain * flux * d.rho_eff * area * v_rel * v_rel;
double T = (shear + roost_thrust) * (slip >= 0.0 ? 1.0 : -1.0);
```
`flux = |slip| × avail` (`:1156`) is the ejected-mass rate — *the same number S5 draws the roost bar
from*. It is applied as **pure forward thrust at `tan_mount`**, which is on the centreline, so its
yaw moment is exactly zero. **He is right that the roost is the physical carrier of the tail swing,
and right that it does nothing for him today.** DERIVED, exact.

### THE ONE DIAL

**`track_lat_slip_shed`, identity `0.0`** — the missing friction ellipse, on the **track only**,
keyed on the track's own longitudinal slip, scaled by his lean.

```cpp
// at sim/sled.cpp:1056, after hoisting the track's `slip` (see below)
double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;
if (g.is_track && p.track_lat_slip_shed > 0.0)
    mu_l *= std::max(0.0, 1.0 - p.track_lat_slip_shed *
                                    std::abs(trk_slip) * (1.0 + align_m));
```

**Why this form.**
- **It is the honest missing physics**, not a new force: a contact patch that is sliding forward
  cannot also hold sideways. LITERATURE (friction ellipse / combined slip, universal in tyre and
  terramechanics models); DERIVED that this kernel has none.
- **It is track-only**, so it never touches the ski plate the carve lives on — the same discipline
  `plane_lat_lean_gain` uses at `sim/sled.cpp:1102` (*"lean buys ski plate, never track plate"*).
- **`align_m` is the weight-shift half, for free.** It is already computed, already clamped ≥ 0, and
  already requires a real yaw rate (|ω_up| > 0.02 rad/s ramping to 1.0 at 0.07,
  `sim/sled.cpp:583-585`). So: **lean into the turn + throttle = the tail comes around; wrong-way
  lean = exactly the shipped number, never a penalty.** That is *"esp with weight shifting of the
  sudburian"* in one existing variable.
- **Leaning aft still loads the track** (`normal` rises), so weight-back keeps *absolute* grip while
  spinning sheds the *fraction* — which is the real machine's behaviour and gives him two
  independent hands on the same slide.

**The one structural cost, named:** `slip` is computed inside the `g.is_track` thrust block at
`sim/sled.cpp:1143`, **after** the lateral block at `:1056`. The 6 lines that build it
(`engage_lo/hi`, `clutch_blend`, `drive`, `v_cmd`, `v_track`, `slip`) must be **hoisted** above the
lateral bite and read in both places. That is a pure refactor with a hard invariant: **at
`track_lat_slip_shed = 0.0` every golden must be byte-identical**, which is the proof the hoist
changed nothing. It must not be a second copy of the slip formula — two copies is how the CVT and
the rpm readout drift apart, and `sim/sled.cpp:1338-1341` already says so in its own words.

**Range** `0.0 .. 1.0`. **Suggested A/B** `0.5`: at full spin (`|slip| → 1`) with a straight machine
that is `track_lat_mu 0.70 → 0.35`; leaned into a developed yaw (`align_m → 1`) it goes to
**0.70 → 0.00**. At the 0.10–0.30 slips of ordinary driving it is a 5–15 % shed and invisible.
DERIVED arithmetic; the value is his drive.

### Invariant + killing mutation
- **Invariant (three-sided).** (a) `0.0` → every golden byte-identical, hoist included.
  (b) **Straight-line running at any throttle must be bit-identical in trajectory** — at
  `align_m = 0` and lockup slip the shed is ≤ 5 %, and a straight machine has no lateral slip for
  `mu_l` to act on anyway. (c) **Wrong-way lean must give exactly the shipped number** —
  `sled_wrong_way_lean_is_never_a_penalty` (`test/unit/test_sled.cpp:2546`) measured the last leak at
  the 4th decimal; this term must not open a new one.
- **Killing mutation.** `track_lat_slip_shed = 0.0` → branch dead. And the dangerous one:
  `track_lat_mu` is pinned by name at `test/unit/test_sled.cpp:1470`
  `sled_track_lat_mu_is_the_rostered_value` — **that leg reads the rostered constant**, so it must be
  re-read to confirm it tests the param and not the effective μ, or this rung silently invalidates it.
- **The honest limit, and it is large.** Shedding the track's lateral μ **lowers the roll threshold
  in a slide** — the machine tips when the surface can push harder sideways than 0.5097 g
  (packet §2b), and it *slides* when it cannot. Taking grip away from the track makes her slide
  **more** and tip **less** on the track patch, but it also removes the yaw resistance that keeps a
  ski-on-a-bank strike from becoming a spin. **⚠ N3 and the bank-strike rollover (his
  *"a ski hitting the bank on one side"*) pull in opposite directions and must be driven on the same
  night.** DERIVED. This is the one rung in this document that could make the roll complaint worse.

### Drive line
> Third gear on the trail, get her to 10–15 m/s, set a lean into a corner and **hold the throttle
> open instead of tapping the brake**. **Today:** she understeers and you reach for S. **With the
> dial:** the rear should step out under power, more the harder you lean into it, and back under you
> when you lift. Then do the same with **no lean** — it should barely move, because a wrong-way or
> absent lean is not supposed to buy anything.

### Law check
- **NO GOVERNOR** — clear. Nothing reads or moves the rider's mass; `align_m` is a readout of a lean
  the player commanded. It **removes** grip rather than adding a torque, so it cannot script an
  outcome.
- **Wheelie is the feature** — untouched: this is a lateral term, and `roost_thrust`, `shear` and the
  whole longitudinal path are bit-identical.
- **Depth 0.77 m** — untouched.
- **⚠ Drift canon (RC-3) is the ruling this rung needs.** Whatever produces the drift he signed, the
  packet §2(e) says **it is not a circle**. Adding half a circle to the track is a change to the
  drift's character and it is his word, not the lane's.

---

## N4 — "THROTTLE IS ALL OR NOTHING"

### The felt problem, his words
> *"throttle is all or nothing ive mitigated this by tapping and applying brake"*

### Where "binary" comes from — THREE stacked mechanisms, in order of size

**(1) THE DRAWBAR CAP FLATTENS THE TOP. This is the biggest one.**
`sim/sled.cpp:1207-1211`:
```cpp
const double cap = std::min(p.max_thrust_n,
                            (p.engine_power_w * breathing) / std::max(std::abs(v_fwd), 2.0));
T = std::clamp(T, -cap, cap);
```
**`throttle` appears nowhere in this expression.** `max_thrust_n = 2272.0 N`
(`sim/sled.h:961` — *"0.70 g measured full-throttle accel … from ~500 straight-line tests"*) and
`engine_power_w = 76 400` (`sim/sled.h:948`). Below **33.6 m/s** (`76 400 / 2 272`) the power term
never binds, so **the cap is the constant 2 272 N at every speed he actually rides.**

DERIVED arithmetic, exact, track patch `area = 1.14 × 0.38 = 0.4332 m²` (`sim/sled.h:553`), track
carrying ~80 % of 3 246.0 N ⇒ `sigma = 5 995 Pa`:

| surface | `c_snow_pa`, `phi_deg` (`sim/sled.cpp:162-193`) | `tau_max` | **fully-developed shear** | vs the 2 272 N cap |
|---|---|---|---|---|
| Bush | 1 200 Pa, 22° | 3 622 Pa | **1 569 N** | under — cap binds only once roost is added |
| TrailMain | 9 000 Pa, 28° | 12 188 Pa | **5 280 N** | **2.3× over** |
| Road | 25 000 Pa, 30° | 28 461 Pa | **12 330 N** | **5.4× over** |

**On the groomed trail and on the road — 39 % of tape 91 — the shear term alone exceeds the drawbar
cap at about 3 % of slip.** From there to full throttle, the commanded thrust is **exactly
2 272.0 N**, the same number, every tick. The thumb is not connected to anything.
*Killing mutation:* any of those `tau_max` rows reading below `2 272 / 0.4332 = 5 245 Pa` — Bush is
the only one that does.

**(2) THE JANOSI SHEAR SATURATES IN A TENTH OF A SLIP.**
`sim/sled.cpp:1172-1175`: `shear = area · tau_max · (1 − exp(−|slip|·g.len / shear_K_m))` with
`shear_K_m = 0.055` (`sim/sled.h:935`) and `track_len_m = 1.14` (`sim/sled.h:553`) ⇒ the exponent is
**20.727 × |slip|**. DERIVED, exact:

| `|slip|` | 0.02 | 0.05 | 0.10 | 0.15 | 0.20 | 0.30 |
|---|---|---|---|---|---|---|
| fraction of full shear | 0.339 | 0.645 | **0.874** | 0.955 | 0.984 | **0.998** |

*Killing mutation:* `shear_K_m ≥ 0.35` would put the knee out past 0.3 of slip — but K is a measured
terramechanics modulus (**LITERATURE:** Janosi–Hanamoto K for snow/sand is 0.01–0.05 m), so 0.055 is
honest and **`shear_K_m` is NOT the dial**. Moving it to buy feel is tilting the hardware.

**(3) AND THE THUMB'S LOWER HALF IS A BRAKE.**
`sim/sled.cpp:1137-1145`: `drive_t = clamp((throttle − 0.02)/0.08)` — the **entire** clutch
engagement happens between thumb **0.02 and 0.10**, which the app's 2.5/s ramp crosses in **0.032 s**.
Above thumb 0.10, `drive = 1` and `v_track = throttle × 46.0` (`track_speed_max_ms`,
`sim/sled.h:936`), so `slip = (46·thr − v_fwd) / max(46·thr, 1)` and **slip is negative — reverse
shear, i.e. a track brake — for every thumb below `v_fwd / 46`.** Clamped by the same 2 272 N, that
is up to **0.70 g of retardation**.

**The thumb at 10 m/s, DERIVED, exact:**

| thumb | 0 → 0.217 | 0.217 → 0.30 | 0.30 → 1.00 |
|---|---|---|---|
| what it is | **reverse shear — a brake, up to 0.70 g** | the entire live band | flat: shear ≥ 99.7 % developed, cap binding |
| share of travel | 22 % | **8 %** | **70 %** |

At 25 m/s the neutral point moves to thumb 0.543; at 40 m/s to 0.870. **The thumb is a
three-position switch — brake / a sliver / pinned — and the sliver slides up the travel with speed.**
That is his sentence, as arithmetic. And it explains his own mitigation exactly: *"tapping and
applying brake"* is the only way to reach a middle, because the middle of the lever **is** a brake.

**(4) The input device, for completeness.** `app/main.cpp:8291-8293`: W ramps the thumb **2.5/s up,
6.0/s down** — the release is **2.4× faster than the build**, so a tapped middle decays 2.4× faster
than it took to make. There is no analog axis anywhere: `sin_.throttle` has exactly one source
(`app/main.cpp:8447`) and it is a keyboard key. MEASURED, round 1, packet §1.3: v17 throttle duty
**WOT 52 % / zero 40 % / modulated 7 %** — the modulated band is 7 % because it is physically
unreachable, not because he does not want it.

### THE ONE DIAL

**`throttle_power_frac`, identity `0.0`.**

```cpp
// at sim/sled.cpp:1207, replacing the cap line
double cap = std::min(p.max_thrust_n,
                      (p.engine_power_w * breathing) / std::max(std::abs(v_fwd), 2.0));
cap *= 1.0 - p.throttle_power_frac * (1.0 - throttle);
T = std::clamp(T, -cap, cap);
```

**Why the cap and not the shear.** The cap is the only term in the longitudinal path that is
**independent of the thumb** — and it is the one that binds. `v_cmd` is already proportional to
throttle; `shear` and `roost_thrust` already respond to it; the cap throws all of that away. An
engine at 30 % thumb makes roughly 30 % of its power, and `max_thrust_n`'s own header calls itself
*"0.70 g measured **full-throttle** accel"* (`sim/sled.h:955-960`) — **the number is already labelled
as the full-throttle case.** Scaling it by the thumb is reading the header, not inventing physics.

**At `0.0` it is bit-identical:** the factor is exactly 1.0 and the expression is the shipped one.
**At `1.0`, WOT is still bit-identical** (`throttle = 1` ⇒ factor 1.0), so his top speed, his
launch wheelie and his jump approach are untouched **by construction**.

| thumb | cap at `frac = 1.0` | Bush thrust today | Bush thrust at `frac = 1.0` |
|---|---|---|---|
| 0.25 | 568 N | 2 272 N (capped) | **568 N** |
| 0.50 | 1 136 N | 2 272 N (capped) | **1 136 N** |
| 0.75 | 1 704 N | 2 272 N (capped) | **1 704 N** |
| 1.00 | 2 272 N | 2 272 N | **2 272 N — identical** |

**Range** `0.0 .. 1.0`. **Suggested A/B** `1.0` (fully proportional) with `0.5` as the half-way arm.
DERIVED arithmetic; the value is his drive.

### Invariant + killing mutation
- **Invariant (three-sided).** (a) `0.0` → every golden byte-identical.
  (b) **WOT must be byte-identical at every value of the dial** — a new leg,
  `sled_throttle_power_frac_is_inert_at_WOT`, is owed and does not exist.
  (c) `test/unit/test_sled.cpp:891 sled_thrust_is_capped_by_snow_not_engine` must still pass:
  this dial scales an **engine** ceiling and must not become the snow's job — that is
  `traction_mu`'s (ladder Rung 5) and the two must be driven on separate nights or their
  attributions cannot be told apart.
- **Killing mutation.** `throttle_power_frac = 0.0` → the factor is 1.0 and the rung is dead.
  And the one that would expose a mistake: drive at `1.0` and measure time-to-40 m/s at **full**
  thumb — if it moves at all, the dial is leaking into WOT and the form is wrong.
- **The honest non-identity, named rather than discovered later.** At `frac > 0` and `throttle = 0`
  the cap becomes 0, which removes a small residual coast term in the **3.0–3.5 m/s clutch band**
  (`clutch_engage_ms 3.25 ± 0.25`, `sim/sled.cpp:1120-1124`) where `T *= drive + (1−drive)·blend` is
  not yet zero. Everywhere else closed-throttle `T` is already ~0. If that residual turns out to be
  felt, the asymmetric form (`cap_drive` scaled, `cap_brake` left alone) is the fallback — but it is
  two numbers, so it is not this rung.
- **Second-order effect he should be told about:** proportional thrust also means **proportional
  roost**, because `roost_thrust` is part of `T` and `flux` is what S5 draws the bar from. A
  half-thumb roost is a thing he has never seen.

### Drive line
> From a stop on the trail, hold W **half a second and let go**, repeatedly, and watch the speedo.
> **Today:** every tap is the same shove — the machine does not care how long you held it past about
> a tenth of a second. **With the dial:** short taps should give small shoves. Then find a corner and
> try to hold a steady 15 m/s on the thumb alone, without touching S. If you can do that, the rung
> paid.

### Law check
- **NO GOVERNOR** — clear, and it needs stating because a *cap* is the shape a governor takes.
  This one is **commanded by the player's own thumb** and is exactly 1.0 at full thumb: it gives the
  player a control he does not have, and takes nothing at the top. Nothing reads or moves the rider's
  mass.
- **Wheelie is the feature** — **byte-identical at WOT**, which is where the wheelie lives
  (packet §3: *"WOT should lift the skis"*, felt item 4). A part-throttle wheelie becomes harder;
  that is the point of a throttle.
- **Depth 0.77 m** — untouched.

---

## N5 — "JUMP WEIGHT SHIFT INFLUENCE IN AIR" (K2 `k_air_shift`)

### The felt problem, his words
> *"Need to make it more dynamic with bodyweight (also for jump weight shift influence in air)"*

### What ON does

`sim/sled.cpp:2096-2098`, the whole term:
```cpp
if (p.k_air_shift > 0.0 && !ground_contact) {
    const glm::dvec3 dl = exch_l_now - s.ws_exch_l;
    s.angular_vel -= (p.k_air_shift * dl) / I;
}
```
with `exch_l_now = mu_exch · (r_rel × v_rel)` built every substep at `sim/sled.cpp:409-448`:
`mu_exch = m_r(m − m_r)/m = 87.5 × 243.5 / 331 = **64.365 kg**` (the reduced mass),
`r_rel` = the rider's seat offset `(0, 0.3537, 0.3573)` (`sim/sled.h:139-140`) **plus** his
displacement, and `v_rel` = his displacement rate.

**It is a momentum exchange, not a torque.** Applied straight to the rate, never through
`torque_body`, and **it telescopes**: once the rider stops moving, `dl` is zero and every borrowed
rad/s has been handed back. **A throw buys an ATTITUDE, never a rate.** It is gated on
`!ground_contact` — *any* running surface or hull point touching kills it (`sim/sled.cpp:538-540`) —
and pinned by three legs: `sled_airborne_lean_is_inert_at_k_zero` (`test/unit/test_sled.cpp:1505`),
`sled_airborne_lean_is_RESPONSIVE_at_k_nonzero` (`:1538`), `sled_air_shift_is_inert_while_any_patch_touches`
(`:1596`).

**The authority, DERIVED exactly.** Integrating the telescoping term over one full one-way throw,
`Δθ = (k / I_axis) · mu_exch · (arm) · (travel)`, with `I = (158.7, 186.9, 49.7)` kg·m²
(`sim/sled.h:777`):

| throw, at `k_air_shift = 1.0` | travel | arm | axis | **attitude bought** |
|---|---|---|---|---|
| full **lateral** sweep, standing (+0.35 → −0.35 m) | 0.70 m | y 0.3537 | **roll** (I_z 49.7) | **18.4°** |
| … its yaw side-effect | 0.70 m | z 0.3573 | yaw (I_y 186.9) | 4.9° |
| full **fore→aft** throw (+0.45 → −0.2183 m) | 0.668 m | y 0.3537 | **pitch** (I_x 158.7) | **5.5°** |

Reach box from `sim/sled.h:786-802`: `lean_lat_stand_m 0.35`, `lean_fwd_max_m 0.45`,
`lean_aft_max_m 0.2183` (the measured seated aft ceiling, stand-dependent through `kAftCeilC1`).
*Killing mutation:* `p.inertia` — the whole table scales as 1/I, and I is measured at 331 kg with the
rider aboard; a re-derived riderless inertia (the R4b debt, `sim/sled.cpp:437-447`) moves every row.

### THE HONEST SIGN, stated before he flies it

**Throwing his weight AFT pitches the nose DOWN.** DERIVED, exact:
his body above the CG moving rearward carries `+L_x`; the chassis takes the negative; and this
kernel's own convention is `+ω_x = nose UP` (`test/unit/test_sled.cpp:4341-4346`, verbatim:
*"this kernel's own K-WS1 leg documents that +omega.x is nose-UP"*). So aft ⇒ `−ω_x` ⇒ **nose down**.

**This is correct physics and it is the opposite of what a rider expects**, and he should be told
before the drive rather than after. Conservation says the rider and the machine counter-rotate; the
lever a real rider actually uses to bring the nose up in the air is the **throttle**, not his body —
and this kernel already has that term, built, gated and also shipped at zero:
`k_gyro_react` (`sim/sled.h:768`, `sim/sled.cpp:2068-2071`), pinned by
`test/unit/test_sled.cpp:4341 sled_gyro_reaction_pitches_the_nose_UP_on_a_throttle_blip` —
*"Real riders describe it without naming it — 'too much throttle off the lip causes your nose to
come up'."*

**⇒ The pair is the honest offer.** `k_air_shift` gives him **roll and yaw** in the air (18.4° and
4.9° a sweep, the axes where his body is the right lever); `k_gyro_react` gives him **pitch** off the
thumb (the axis where it isn't). Offering `k_air_shift` alone and letting him discover that leaning
back drops the nose is the lying-instrument shape. **He asked for one dial; the truth is two, and
the second one is already built. That is a ruling, not a build.**

### THE ONE DIAL

**`k_air_shift`, identity `0.0`.** **Starting value to offer his drive: `1.0`.**

**Why 1.0 and not a tuned number.** `k` is the *fraction of exact conservation*: at 1.0 the chassis
takes precisely the angular momentum the rider's body spent, no more. **`k > 1.0` manufactures
angular momentum from nothing** — it is the one value range in this dial that is a cheat, and it
should be fenced by the range `check(...)` at load, not left to a typo. `k = 1.0` is therefore both
the physically honest value **and** the ceiling, and the only question his drive has to answer is
whether 18.4° of roll per full sweep is *enough* — if it is not, the answer is a bigger reach box
(`lean_lat_stand_m`) or a re-derived airborne inertia, **not** a `k` above 1.

DERIVED throughout. The 16 % constant-inertia approximation under full standing lean is named at
`sim/sled.cpp:481-484` and rides along here; it is not this rung's to fix.

### Invariant + killing mutation
- **Invariant (three-sided).** (a) `0.0` → byte-identical, and the pair
  `sled_airborne_lean_is_inert_at_k_zero` / `..._is_RESPONSIVE_at_k_nonzero` is the shipped gate for
  exactly this. (b) `sled_air_shift_is_inert_while_any_patch_touches` must still pass — **no
  ground-contact steering**, the anti-cheat leg. (c) **A throw-and-return must net exactly zero
  attitude change** (the telescope): out 18.4°, back −18.4°. If a full cycle leaves a residue, the
  every-substep carry at `sim/sled.cpp:2103` has broken and the term has become a free impulse.
- **Killing mutation.** `k_air_shift = 0.0` → branch dead. Flip the sign of `dl` → the leg at
  `test/unit/test_sled.cpp:1538` should fail; if it passes, it is measuring magnitude and is
  sign-blind (the RT P0-1 defect class that hid a wrong `delta` sign for rungs).
- **⚠ The tape trap, already handled and worth not re-breaking.** `app/main.cpp:8398-8410` zeroes
  `sled.ws_exch_l` on an autoright, and its comment records the measurement:
  *"without this line `sled_tape_round_trip_replays_bit_identical` diverges at the override tick
  (orientation.w) whenever `k_air_shift > 0`."* The field is **not** in the tape's pin roster, so the
  replay rebuilds it as zero. Turning this dial on makes that line load-bearing for the first time.
- **The honest limit.** `mass` is still 331 kg and `p.inertia` is still measured at 331 kg even after
  the rider lets go (`sim/sled.cpp:437-447`, named as a real gap, R4b's). So the airborne exchange is
  computed on a machine that still carries a rider's worth of swing. At `k = 1.0` the error is
  second-order; it is not zero.

### Drive line
> Find a bank you like, jump it, and **in the air only**: (1) swing the mouse hard left, then hard
> right — she should roll about 18° each way and **stop** when you stop, not keep turning; (2) throw
> your weight **back** — the nose should go **DOWN**, which is real and will feel wrong the first
> three times; (3) blip the thumb off the lip — that is the lever that brings the nose **up**, and it
> is off today. Tell me whether you want (3) armed on the same night, and whether 18° a sweep is
> "dynamic" or whether you want the reach box opened.

### Law check
- **NO GOVERNOR** — clear, and this is the strongest case of the five. The term exists *because*
  the player moved the rider's mass; it reads position and velocity only, is exactly zero hands-off
  (`rider_frac` and `mu_exch` both go to 0 at `!hands_on`, `sim/sled.cpp:436-448`), and it telescopes
  to zero. It cannot manufacture a sustained rate.
- **Wheelie is the feature** — untouched: gated on `!ground_contact`, and the launch wheelie is a
  ground-contact event.
- **Depth 0.77 m** — untouched.
- **No `if(angle > X)`** — there is no attitude branch anywhere in the term.

---

## 6. WHAT THIS ROUND DID NOT DO

1. **No tape was opened.** N4's "binary" claim is arithmetic on the source; the *duty* figures
   (WOT 52 / zero 40 / modulated 7 %) are MEASURED and carried from round 1, packet §1.3. A tape leg
   that measures the **distribution of `in.throttle` between 0.10 and `v_fwd/46`** would either
   confirm or kill the three-position-switch reading, and it is free and unrun.
2. **N2's diagnosis is arithmetic, not a measurement.** The claim *"the clamp discards his rock"* is
   exact algebra on `sim/sled.cpp:401-406`, but nobody has instrumented `lat_target_sr` against
   `target_lat_m` on a real righting. `right_shift_cmd` **is** in the tape's pin roster
   (`test/harness/sled_tape.h:87` pins the param; the state field is not) — so the cheapest
   confirmation is a probe, not a drive.
3. **N3 and the roll complaint pull against each other** and that is stated at the rung, not hidden.
   They must not be driven on the same night as `class_blend_m` (ladder Rung 1) or the attribution
   is lost.
4. **Ruling R2 and R5–R10, R12–R14 remain open** (packet §4). Nothing here consumes them.
5. **Nothing is built.** Two of the five (N1, N2) are `config/` edits reachable today; three
   (N3, N4, N5) are six-touch RC-8 plumbing jobs in a fresh lane.

**Cheapest first, if he wants a single night:** **N2** (`right_stand_shift_frac` 1.0 → 0.5, one TOML
line, zero plumbing, answers the ask he wrote the most words about) and **N1**
(`rolled_throttle_frac`, one kernel line + the six touches he already has for `SledComfort`).
**N4 is the largest felt change in the document** and it is the one that makes N3 drivable at all —
a proportional thumb is the precondition for steering with it.
