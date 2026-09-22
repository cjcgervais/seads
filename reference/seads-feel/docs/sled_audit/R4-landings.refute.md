# REFUTATION — strand R4-landings

Adversarial re-derivation of every numeric and source claim in
`docs/sled_audit/R4-landings.md`. Read-only against main; worktree
`D:/seads_sandboxes/sled-audit` @ `ed0a43ce8`. No build was run, no probe was
built, no test suite was run, nothing was pushed.

**VERDICT: SOLID_WITH_FIXES.** The mechanism spine is real and re-derived
independently. Six claims are refuted as stated; four of the six fail on one
shared arithmetic fault, and one fails on a killing mutation that was run and
returned the wrong answer.

---

## R-1 (kills R4-05, R4-07; kills R4-06's corroboration; kills R4-10's numbers)
### THE MASS IS WRONG BY 26.4 %, AND THE SOURCE WARNS ABOUT THIS EXACT MISTAKE IN CAPITALS

`R4-landings.md:150` reads, in full:

    (331 + 87.5) × 9.80665 = **4104 N**.

`sim/sled.h:784-785`:

    double rider_mass_kg = 87.5;     // split OUT of mass_kg, NEVER added --
                                     // 87.5 is INSIDE 331, machine is 243.5

`sim/sled.cpp:224`: `const double mass = std::max(p.mass_kg, kEps);` — 331.0,
and that one `mass` is what every force in `step_sled` divides by, including
the `w_contact` gate and the `g_eff` the tapes record. The tape param header
the strand read its inertia off says so too: `# param mass_kg 331` (and
`# param rider_mass_kg 87.5` on its own line, one row of the same block).

**Correct W = 331 × 9.80665 = 3246.0 N.** Every figure below is the strand's
own method with the right weight.

| quantity | report | corrected | source |
|---|---|---|---|
| machine weight | 4104 N | **3246.0 N** | sled.h:543, sled.cpp:224, tape hdr |
| static sag x0 = W/3k | 0.02974 m | **0.02352 m** | susp_k 46000, sled.h:885 |
| stored x0→travel | 4603 J | **4626 J** | ½·3k·(0.26²−x0²) |
| gravity work over stroke | 945 J | **767.7 J** | W·(0.26−x0) |
| kinetic budget | 3658 J | **3858 J** | stored − gravity |
| bottoming vz | 4.18 m/s | **4.83 m/s** | √(2·KE/331) |
| equivalent free drop | 0.89 m | **1.19 m** | v²/2g |
| equivalent symmetric hang | 0.85 s | **0.985 s** | 2v/g |
| bump-stop force in g | 8.74 g | **11.05 g** | 35880/3246 |
| % of airs over it | ~30 % | **24.8 %** | 244/984 hangs ≥ 0.985 s |
| damper rate per m/s sink | 2.63 g | **3.33 g** | 10800/3246 |
| w_contact saturation | 2052 N | **1623 N** | 0.5·331·9.80665 |
| sink rate that saturates it | 0.19 m/s | **0.150 m/s** | 1623/10800 |
| per-patch sprung share | 139.5 kg | **110.3 kg** | 331/3 |
| c_crit = 2√(km) | 5066 N·s/m | **4505 N·s/m** | k 46000, m 110.3 |
| ζ = c/c_crit | 0.711 | **0.799** | susp_c 3600, sled.h:886 |
| f_n | 2.89 Hz | **3.25 Hz** | √(k/m)/2π |

Every correction moves the same way: the shipped kernel is HARSHER than the
strand reported, not softer. Direction survives; no number does.

**Killing mutation of this refutation:** an effective landing mass above
331 kg — e.g. if the patch solve ran against a sprung mass that excluded
unsprung parts, or if `step_sled` ever used a second mass. It does not:
`grep -n "p.mass_kg" sim/sled.cpp` returns exactly one line (224), and that one
local `mass` is threaded to gravity, `w_contact`, the C3 hull gate and the
integrator alike.

**Second, independent defect in R4-05, which the mass fix does not repair.**
The energy balance omits the damper entirely — and by the strand's OWN R4-06
the damper is the dominant landing force. A 4.83 m/s arrival is met by
3·3600·4.83 = 52 164 N of damping (16.1 g) on the first substep of contact,
which removes energy before the spring is anywhere near its stop. "Bottoming
is his normal landing" does not follow from this arithmetic. R4-05's stated
killing mutation (the equal-share assumption across three patches) is
**decorative**: it touches neither the mass error nor the missing damper.

---

## R-2 (kills R4-09 as stated)
### THERE IS A FOURTH NON-CONTACT-GATED TORQUE, IT SHIPS AT 2400 N·m, AND THE CLAIM'S OWN KILLING MUTATION SAYS IT ISN'T THERE

R4-09's killing mutation, verbatim: *"A torque term applied while
ground_contact == false that I missed. I grepped every torque_body and
angular_vel write in step_sled; the only non-contact-gated ones are the gyro
pair and the airborne exchange, all three dialled to 0."*

`grep -nE "torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns five
sites, not four:

    602   torque_body   += tq;                        (per-patch, contact)
    1608  torque_body.z += dbg_tq_c2;                 (C2, hull-load gated)
    1655  torque_body   += t_c3;                      (C3, hull_engage_lp gated)
    1792  torque_body.z += s.right_assist_nm_now;     (R4a STAND self-right)
    1881  torque_body.z += tq;                        (comfort assist, w_contact)

Line 1792's enclosing gate, `sim/sled.cpp:1679`, is:

    if (p.comfort.right_assist_nm > 0.0 && hands_on) {

and inside it the only further gates are a tilt ramp
(`right_tilt_lo/hi_rad` 0.25/0.35 rad = 14.3°/20.1°) and a hysteretic
low-passed HORIZONTAL-SPEED gate (`right_assist_max_ms`). **Ground contact
appears nowhere.** `normal_sum`, `side_normal_sum` and `w_contact` are not
read in that block.

And it is not dialled to zero. `config/scenario.toml`:

    right_assist_nm          = 2400.0                 # :2085
    right_assist_max_ms      = 1.3888888888888888     # :2088
    right_tilt_lo_rad        = 0.25                   # :2104

2400 N·m is 5.3× the comfort assist's `roll_stiff_nm` 450. The trap the
strand fell into is a stale in-source comment at `sim/sled.cpp:1662`, which
says the assist "is 0.0 by default and 0.0 for every existing tape" — true of
the `SledParams` member initializer (`sim/sled.h:224`) and false of the
shipped TOML that overrides it.

**What survives:** the aerodynamic half. `sim/sled.cpp:1932` is
`force += -0.5 * p.air_rho * cda * v_len * s.velocity`, applied at the CG with
no moment arm, and there is no other aero term — so "no aerodynamic moment of
any kind" is CONFIRMED. So is `w_contact`'s airborne zero (sled.cpp:1842-1844)
and the fact that the comfort assist writes only `torque_body.z` (1881).

**What must be restated:** not "zero attitude authority on every axis", but
"no aerodynamic moment, and the comfort assist is contact-gated — the one
airborne-reachable roll torque is the STAND self-right at 2400 N·m, shut off
above 1.39 m/s of low-passed horizontal speed, which in practice excludes
every send but a near-vertical drop."

**Killing mutation of this refutation:** if `hands_on` were false airborne, or
if `right_gs_lp` used a speed that stays high through a hang. `hands_on` is a
grip-state predicate, not a contact predicate; `right_gs_lp` is a first-order
lag on horizontal speed only (`v − dot(v,up)·up`), so a vertical drop from a
standstill reaches the gate. Rare in his driving — reachable in the kernel.

---

## R-3 (kills R4-06's corroboration clause; the MECHANISM stands)
### "TWO METHODS SHARING NO ARITHMETIC AGREEING TO THREE SIGNIFICANT FIGURES" IS THE MASS ERROR AGREEING WITH ITSELF

The mechanism is CONFIRMED by direct read. `sim/sled.cpp:705`:

    normal = p.susp_k * x + p.susp_c * xdot;
    if (x > p.susp_travel_m) normal += p.susp_k * p.susp_stop_k * (x - p.susp_travel_m);
    normal = std::max(normal, 0.0);

inside the per-patch loop, so `susp_k`/`susp_c` are per-patch rates (this
closes R4-06's own killing mutation in its favour). At touchdown `x` ≈ 0
(`x = clamp(d_total − z, …)` with `d_total` just crossing zero and `sink_m`
relaxed through the air), so the damper alone carries the first substep. It
is linear in `xdot`, there is no blow-off, and `std::max(normal, 0.0)` is the
only bound in the expression. **CONFIRMED, and it is the strand's real find.**

The corroboration is not. Re-derived from `tape_summary.json` myself: the
0.5–1.0 s bucket (n = 233) has a peak-|g_eff| p50 of 129.19 m/s²
= **13.17 g**. The DERIVED value at a 4.9 m/s arrival, with the correct
weight, is 3·3600·4.9/3246 = **16.30 g** — a 24 % miss. The reported
"13.16 g at 5 m/s" is 54000/**4104**. The two methods agreed because both
carried the same wrong denominator.

The direction of the residual is consistent with the strand's own sampling
caveat — `tools/sled_tape_audit.py:663-667` differences velocity across
consecutive **tick** records (dt 0.008333 s) while the kernel runs 12
substeps inside each tick, so a one-substep damper spike is smeared over
twelve and reads LOW. That is an argument the measured p50 should sit below
the derived value, which it does. It is not an argument that they match.

---

## R-4 (kills R4-01's headline clause; the landing statistics stand)
### "AIR IS ~A TENTH OF THE GAME" IS IN NO DATA ANYWHERE IN THE STRAND

`R4-landings.md:72` is headed "Air is a tenth of the game", and the section
body (lines 73-89) never cites an airborne fraction at all — it gives landings
per minute and the upright percentage and stops. The number has no derivation
in the file.

From the strand's own `tape_summary.json`, three ways:

- time-weighted `frac_airborne` = **6.46 %**
- Σ hang / Σ duration = 720.8 s / 12 364.8 s = **5.83 %**
- unweighted per-tape mean = **4.97 %**

A sixteenth, not a tenth.

**CONFIRMED in the same claim**, re-derived by me from the JSON: 984 air
events = 984 landings scored, 206.1 min, **4.775 landings/min**, 306 upright
at +0.5 s = **31.10 %**, 678 not. The `air_s` reading is also confirmed —
`sim/sled.cpp:2131-2135` clears the timer when `normal_sum + side_normal_sum
> 1.0` **or** `(r_after − floor_r) < 1.2 * p.cg_height_m`, else `+= h`. It is
a per-substep timer, not a latch. R4-01's killing mutation is real, was run,
and closes correctly.

Minor: "91-tape corpus" should read **84** — only 84 of the 91 `.sledtape`
files carry the `# world` / air block.

---

## CONFIRMED BY INDEPENDENT RE-DERIVATION (no fix owed)

- **R4-02** exact. My percentile pass over the pooled 984 hangs: p50 0.473,
  p90 1.734, p99 2.886, max 3.944 s; 238 ≥ 1 s = 24.2 %; 78 ≥ 2 s. Derived
  apex g(t/2)²/2 = 3.69 m and arrival g·t/2 = 8.51 m/s check at p90. The
  symmetric-flight killing mutation is real and cuts the stated way.
- **R4-03** substantively exact. My bucket p50s, converting the JSON's m/s² to
  g: 6.63 / 13.17 / 18.52 / 24.15 / 34.00 (report: 6.6 / 13.2 / 18.7 / 24.4 /
  34.0 — two buckets ~1 % apart on percentile interpolation, not substance).
  Worst event 2565.7 m/s² = **261.6 g** at 2.48 s hang in
  `sled_tape_16.sledtape` — exact. Note the `max` column is NOT monotonic
  (66.9 / 96.3 / 50.9 / 41.5); the claim is correctly stated on p50 only.
- **R4-04 numbers** exact: 146 `autoright_R`, 146 of 146 carrying
  `tilt_before_deg`, 0.708/min, min **69.3°**, p50 111.9° (report 112.0),
  max 174.1°, **zero** below 45°.
- **R4-13** arithmetic exact: 0.193 × (20/0.0815) / 158.7 = 0.2984 rad/s =
  **17.1 °/s**; 25.6 °/s at 30→0; 22.2 °/s at 20→46. Demands 80/40/23.5 °/s.
  The chain is confirmed at `sim/sled.cpp:1959-1967`
  (`w_rotor = rep_belt / drive_radius_m`, `L_raw = −rotor_inertia * w_rotor`).
- **R4-14** exact and load-bearing. `sim/sled.cpp:1354`:
  `rep_belt = std::max(dv * v_cmd_b, std::max(v_bf, 0.0));` — brake appears
  nowhere, and `w_rotor` is a pure function of `rep_belt`, so `dL` from a
  brake application is identically zero at any dial value. The source's own
  "this kernel's belt is KINEMATIC" note is at sled.cpp:2062-2066 as quoted.
- **R4-12** CONFIRMED, and its UNVERIFIED-3 is now **CLOSED IN ITS FAVOUR**:
  `grep -n "k_gyro\|k_air_shift\|gyro_react\|rotor_inertia" config/*.toml`
  returns nothing. Defaults 0.0 at sled.h:755, 768, 859; used at sled.cpp:1969,
  2068-2070, 2096-2099. The tape headers even carry `# param k_air_shift 0`.
- **R4-19**'s UNVERIFIED-4 is **CLOSED IN ITS FAVOUR**, and the claim is
  stronger than written. `sim/rider_grip.h` (295 lines) is a one-way
  instrument: `grip_step` consumes `g_eff_mag` and `dt_s` and writes
  `load` / `load_lp` / `extension_m` against `capacity` 70.0. The call site
  (`sim/sled.cpp:2039-2046`) adds no force or torque back onto the body. The
  kernel's rider absorbs exactly zero landing energy on any path, not just the
  suspension path.
- **R4-10's structure** CONFIRMED (step, not ramp; `torque_body.z` only;
  800 × 3 = 2400 N·m ÷ I.z 49.7 = 48.3 rad/s²; `inertia.z 49.7` verified in
  the tape param header, `roll_stiff_nm 450` / `roll_damp_nms 800` /
  `roll_ref_rad 0.14` verified in `config/scenario.toml` AND in every tape's
  cparam block). Only the two numbers in R-1's table are wrong.
- **Chad's words, all verbatim, all located.**
  `ROLL_COMFORT_HANDOFF.md:20` — "…land on **my skis more often after a
  roll** (even though R works). But not every time"; `:38` — "Make it less
  honest but dont ruin it"; `:41` — "arcadey to a degree so that it is fun.,
  SLiding banging, punchy, jumps. Just make it more stable";
  `WINTER_LAW.md:395` — "A too steep side approach at speed should also roll
  the snowmachine"; `gi4_ride_handoff.md:35` — "should be able to launch in
  the air" / "Only 2 of 40 land upright". No paraphrase-into-a-stronger-claim
  found anywhere in the strand.

---

## FIXES OWED (claim survives, the supporting statement does not)

1. **R4-04's killing mutation is FALSE as written.** It says "only
   `episode_start` and `autoright_R` appear in the corpus, so this is closed
   for this kernel." The pooled `override_classes` are
   `{episode_start: 84, autoright_R: 146, mount_or_other: 9}`. A third class
   exists, 9 records, uninspected — which is precisely the class the mutation
   was written to rule out. The numbers survive; the mutation must be re-run
   against those 9.
2. **R4-08's K = 98 000 holds only at depth exactly 0.77 m.**
   `pack_modulus` (`sim/sled.cpp:111-117`) returns
   `snow_hardness * (kc/b + kphi) * pow(pack_ref_depth_m / depth, pack_soften)`
   with `pack_soften` 0.90 (sled.h:901) — and all 84 tape headers read
   `depth_base=0.850`. In the recorded world K = 98 000 × (0.77/0.85)^0.9 =
   **89 700**, so the energy rows run ~8.5 % high against the corpus they are
   read beside. The arithmetic itself checks out at 0.77 m (area
   1.14 × 0.38 = 0.4332 m², sled.h:553; ∫A·K·z^n dz = 772 / 2844 / 8545 J at
   0.30 / 0.50 / 0.77 m), the series structure is confirmed
   (`sim/sled.cpp:663-706`, one shared `d_total` split between `susp_k*(d−z)`
   and `A·K·z^n` by two Newton steps), and the strand's own
   `z ≤ min(d_total, z_cap)` caveat is the honest one. Note also that
   `d_total = p.susp_rest_m − hang` is only bounded by 0.21 m while `hang ≥ 0`;
   nothing in the expression forbids a negative `hang`.
3. **R4-17's line anchor is stale.** "rolled on a cross-slope landing" is at
   `config/world.toml:695`, not 507-509. The string exists verbatim.
4. **Corpus homogeneity — state it once, in the strand's favour.** The 84
   tapes span git describes from `pre-reconcile-20260821-58` to
   `kernel-v17-tremor-signed-53`, which looks like a mixed-kernel pool. It is
   not, for this strand's purposes: all 84 carry identical comfort cparams
   (`roll_stiff_nm 450`, `roll_damp_nms 800`, `assist_hull_frac 0.5`,
   `roll_stiff_vgain 1`, `release_floor_frac 0.3`) and identical
   `depth_base=0.850`. Pooling is defensible for the comfort and suspension
   path. It is NOT established for anything the terrain-clip lane touched, and
   no claim in the strand should be read as covering `facet_contact`.

---

## SIGNED-LAW CHECK

No claim in the strand contradicts the signed law.
- **Depth 0.77 fixed** — R4-08 uses it as the Bekker stiffness REFERENCE
  (`pack_ref_depth_m`, sled.h:900, "★ Chad's signed median. NOT a free dial"),
  not as a dial to move. Clean.
- **No governor** — R4-20 proposes a window and an attitude condition on a
  torque term that already exists and already fires; R4-12/R4-13 propose
  arming dials that ship at zero. Neither is an automatic rider-balance
  governor. `ROLL_COMFORT_HANDOFF.md §0b`'s NO GOVERNOR is about moving the
  rider's mass automatically; nothing here does. Clean.
- **One surface / wheelie kept / ragdoll banned** — untouched by every claim.
- The strand recommends nothing and changes nothing; it is a reading. Clean.

---

## WHAT SURVIVES, IN ONE PARAGRAPH

The landing spike in this kernel is the damper, it is linear in sink rate,
and it has no blow-off (`sim/sled.cpp:705`). The air is attitude-dead in the
ways that matter — no aero moment, three written-and-commented rotation dials
all shipping at zero and unreachable from TOML (`sled.h:755/768/859`) — and
arming the reaction wheel today would buy nose-up only, because `rep_belt`
cannot fall below body forward speed (`sled.cpp:1354`). 984 landings in
206.1 minutes, 31.1 % of them upright at +0.5 s, 146 R-presses every one past
69.3° of tilt. All of that is measured and holds. But nothing in this strand's
force, pressure or damping-ratio arithmetic may be quoted until the weight is
corrected from 4104 N to 3246 N — every such figure is 26.4 % low, and the
strand's headline corroboration is that error shaking hands with itself.
