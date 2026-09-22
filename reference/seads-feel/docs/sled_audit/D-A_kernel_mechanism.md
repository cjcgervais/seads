# D-A — KERNEL MECHANISM AUDIT (sim/sled.h, sim/sled.cpp, sim/rider_grip.h)

Strand **D-A**. Audit worktree `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`,
HEAD `ed0a43ce8`. **READ-ONLY against main.** Nothing in `sim/`, `control/`, `config/`,
`test/golden/` was opened for write. No dial changed, no test run, no probe built, nothing
pushed. The only file this strand wrote is this one.

**Tree identity, verified, because every claim below is "on today's code":**
`git diff --stat HEAD origin/main -- sim/sled.h sim/sled.cpp sim/rider_grip.h config/scenario.toml config/world.toml`
returns **empty**, and `git merge-base --is-ancestor HEAD origin/main` is **true**
(origin/main is `533c8640966ef82b311d138959da6b8187d7e4e3`, 2026-09-17 22:44). So the kernel
text audited here *is* main's kernel text, byte for byte.

**Confidence vocabulary.** MEASURED (a number somebody instrumented, with the instrument
named) · DERIVED (arithmetic done here from stated inputs) · LITERATURE (a published or
prior-document statement, evidence that someone *said* it) · GUESS (labelled, every time).

**Every numeric claim carries its killing mutation** — the change to the tree that would
make the claim false. A claim without one is decoration.

**Tape evidence** is from `docs/sled_audit/tape_summary.json` (strand D-B's tool,
`tools/sled_tape_audit.py`, 91 tapes). Where I cite it I name the bucket and I have
**spot-verified the tool's own definition** of the field in the source (line numbers given).
I did not edit that tool — it is D-B's file.

---

## 0. The headline, before the detail

Six things this audit establishes that the tree's own comments do not say:

1. **§9.6's law is HALF TRUE on today's code.** The destabilising half is exactly as
   claimed and *worse* than documented. The restoring half has been **refuted since
   2026-08-14**: `plane_lift_split_frac = 1.0` gives the machine a restoring roll couple
   that scales with **v², not contact load** — the only restoring roll term that survives an
   unloaded machine. (§2)
2. **`sim/sled.h:226` is a lying instrument.** "Hysteretic like every gate in this kernel"
   — the self-right speed gate is the **ONLY** gate in the kernel with two thresholds. All
   38 other gates/latches/bands are single-valued ramps, hard edges, or one-way latches. (§3)
3. **Three config dials differ from the kernel default, and the identity test does not
   cover any of them.** `right_assist_nm` **0.0 → 2400.0**, `right_charge_push_s`
   1.0 → 0.6, `right_dir_eps` 0.1736 → 0.04. Two of the three are justified *in the same
   file* by comments that describe different numbers. (§4)
4. **The R4a self-right is the only roll-torque term in the kernel with NO contact gate at
   all**, it ships at 2400 N·m, and its tilt gate reads **radial up, not the contact plane** —
   the exact defect red-team P1-1/2 removed from the hull terms and left standing here. (§6.2)
5. **`gi4_ride_handoff.md`'s 4106 N is wrong.** The kernel's machine weighs **3246.0 N**
   (`mass_kg` 331 already contains the rider). Every "X N of its 4106 N" in the GI4 trace
   understates the contact collapse by 26.5 %. (§2.4)
6. **SK-1a — the fix for the bank-strike superspin Chad actually reported — is present in
   the kernel and unreachable on main**, because `[snowpack] class_blend_m = 0.0` makes
   `surf_mix` identically 0 and the blend branch dead. (§5.7)

---

## 1. THE FORCE / TORQUE PATH, ONE SUBSTEP

`step_sled` at `sim/sled.cpp:216`. `n = max(1, p.substeps)` = **12**, `h = dt/n`
(`sled.cpp:220-221`). At the shipped tick rate (dt = 1/120) that is **1440 Hz**, h = 6.944e-4 s.
MEASURED provenance of the 12: `sim/sled.h:1211-1220` — re-measured at the 331 kg retotal,
8 substeps reads 1.8e-3 on the 2 m drop, 12 reads 9.2e-4.

The order **is** the model (`sled.cpp:9-20`). Numbered as it executes:

| # | block | file:line | what it writes |
|---|---|---|---|
| 1 | frame: `R`, `body_up`, `body_fwd`, `up_cg`, `world_omega` | sled.cpp:236-240 | locals |
| 2 | `rolled` readout + dwell integrator | sled.cpp:254-262 | `s.rolled_hold_s`, `s.rolled` |
| 3 | `hands_on` → throttle / brake gating | sled.cpp:291-294 | locals (throttle, brake) |
| 4 | steer slew (rate-capped, `steer_rate_per_s` 2.0/s) | sled.cpp:299-306 | `s.steer_actual`, `delta` |
| 5 | rider reach box + K1 aft-ceiling curve | sled.cpp:312-379 | `lat_reach_m`, `aft_max_eff` |
| 6 | rider slew (lat incl. self-right brace, fwd clamped to `aft_max_eff`, up) | sled.cpp:401-413 | `s.rider_lat_m/_fwd_m/_up_m` |
| 7 | K2 exchange momentum `L_exch = μ (r_rel × v_rel)` | sled.cpp:423-468 | `exch_l_now` (spent at #28) |
| 8 | `cda = air_cda + stand_cda_add_m2 · rise_frac` | sled.cpp:469-471 | local |
| 9 | composite `cg_off = rider_frac · (−lat, up, −fwd)`; `patch_geometry` | sled.cpp:523-527 | `geom[3]` |
| 10 | **per-patch loop** (3 patches) | sled.cpp:608-1262 | see 10a–10l |
| 10a | ONE ground query `snow.sample_at(up_i)` + SK-1a dial blend | sled.cpp:610-636 | `gs`, `d` |
| 10b | suspension ⊗ snow **in series**, 2 Newton iterations → `normal` | sled.cpp:650-709 | `s.susp_x[i]`, `s.susp_v[i]`, `normal_sum`, `patchN[i]` |
| 10c | sinkage **rate process** (Bekker `z` + `creep_m`, two timescales) | sled.cpp:716-749 | `s.creep_m[i]`, `s.sink_m[i]` |
| 10d | air test `if (normal<=0 && x<=0) continue;` → `ground_contact` | sled.cpp:751-767 | `ground_contact` |
| 10e | planing lift, split 4-way by **draft** (`plane_lift_split_frac`) | sled.cpp:774-864 | `add_at` ×4 (kRollLift), `lift_sum` |
| 10f | normal reaction at 4 / 2 / 1 points (rail × pitch quartering) | sled.cpp:913-976 | `add_at` ×4 (kRollNormal) |
| 10g | tangential application point `tan_mount` (`bite_at_contact_frac` 1.0) | sled.cpp:988-993 | local |
| 10h | sliding friction `−μ_kin_eff · normal · v̂` | sled.cpp:1014-1026 | `add_at` (kRollFric) |
| 10i | plow drag `−½ρ v_fwd² · w · immersion · plow_cd` | sled.cpp:1033-1040 | `add_at` (kRollPlow) |
| 10j | lateral μ bite `−normal · μ_l · tanh(slip/slip_ref) · (1+B1)` | sled.cpp:1053-1069 | `add_at` (kRollBite) |
| 10k | planing lateral plate — **BANKED, not applied** | sled.cpp:1083-1111 | `platN/F/R/On/Steer[i]` |
| 10l | track only: clutch blend → slip → bury/avail/**flux** → shear + roost → engine cap → **traction cap** → thrust; engine brake; player brake | sled.cpp:1114-1261 | `add_at` ×3 (kRollDrive) |
| 11 | plate **second pass**, mean-normalised over steered patches, applied | sled.cpp:1294-1335 | `add_at` (kRollPlate) |
| 12 | belt speed + engine rpm readouts (body-level, never patch-level) | sled.cpp:1343-1361 | `rep_belt`, `rep_rpm` |
| 13 | contact-plane fit `n_surf` (load-weighted reliability blend) → `phi_surf` | sled.cpp:1372-1425 | locals |
| 14 | **C1** hull: 10 unilateral spring-dampers + μ + cohesion plough | sled.cpp:1438-1535 | `add_at` (kRollHull), `side_normal_sum`, `ground_contact` |
| 15 | **C2** righting bias → `torque_body.z` **directly** | sled.cpp:1545-1609 | `torque_body.z` |
| 16 | `hull_engage_lp` (0.1 s LP) + **C3** yaw arrest → `torque_body` | sled.cpp:1622-1657 | `s.hull_engage_lp`, `torque_body` |
| 17 | **R4a self-right** → `torque_body.z` + brace command | sled.cpp:1679-1800 | `s.right_*`, `torque_body.z` |
| 18 | **A + B2** roll assist (+GI3 F-A/F-B/F-C) → `torque_body.z` | sled.cpp:1819-1886 | `s.assist_nm`, `torque_body.z` |
| 19 | debug sink write #1 (write-only) | sled.cpp:1892-1925 | `dbg` |
| 20 | air drag + gravity, **at the CG** (never at a mount) | sled.cpp:1931-1933 | `force` |
| 21 | `plane_frac` accumulators | sled.cpp:1935-1939 | `rep_plane_*` |
| 22 | integrate linear: `v += (F/m)h; x += v h` | sled.cpp:1942-1943 | `s.velocity`, `s.position` |
| 23 | rotor `L_raw`, `L_r = k_gyro·L_raw`, `Iω + L_r`, `α = (τ − ω×(Iω+L_r))/I` | sled.cpp:1959-1974 | `alpha_body` |
| 24 | sink write #2 (`a_body`, `alpha_body`) | sled.cpp:1989-1999 | `dbg` |
| 25 | `grip_step` — buck memory, extension ODE, load, 0.1 s LP, one-way latch | sled.cpp:2028-2051 | `s.grip` |
| 26 | gyro roll bucket; **G2** reaction as a RATE delta | sled.cpp:2057-2074 | `s.angular_vel.x`, `s.gyro_prev_L` |
| 27 | `ω += α h` | sled.cpp:2075 | `s.angular_vel` |
| 28 | **K2** airborne exchange, RATE delta, gated `!ground_contact` | sled.cpp:2096-2102 | `s.angular_vel`, `s.ws_exch_l` |
| 29 | quaternion integrate + renormalise | sled.cpp:2103-2106 | `s.orientation` |
| 30 | last-resort penetration rail | sled.cpp:2114-2121 | `s.position`, `s.velocity` |
| 31 | `air_s` bookkeeping | sled.cpp:2131-2135 | `s.air_s` |

**Structural facts worth naming:**

* **Only #22/#27/#29 integrate.** Every force term reaches the body through `add_at`
  (`sled.cpp:594-606`), which is the whole of §2.4c.1: `torque = r_body × (Rᵀ f_world)`.
  Four terms bypass `add_at` and write `torque_body` (or the rate) directly: **C2** (1608),
  **C3** (1655), **self-right** (1792), **assist A** (1881), plus the two momentum deltas
  **G2** (2070) and **K2** (2098) which write `angular_vel` and never `torque_body` —
  deliberately, so the gyroscopic cross product does not treat them as external moments
  (`sled.h:760-767`, `sled.cpp:2084-2086`).
* **`ω` used by every torque term is the substep's PRE-update `ω`** (advanced at #27), and
  `position`/`velocity` used by the contact loop are the PRE-update ones (advanced at #22).
  Semi-implicit on both channels; consistent.
* **One ordering carry:** `right_shift_cmd` is written at #17 and consumed at #6, i.e. one
  substep = 1/1440 s later (`sled.h:1263-1266`). Deterministic and documented.
* **One ordering trap that is documented and correct:** `hands_on` is read ONCE at #3 and
  reused at #7/#9 rather than re-reading `s.grip.attached`, because `grip_step` runs at #25
  (`sled.cpp:452-457`).

---

## 2. THE CONTACT-SCALING ASYMMETRY — §9.5/§9.6 VERIFIED, AND REFUTED

`docs/gi4_ride_handoff.md` **§9.6** states, verbatim:

> **"Every restoring roll term in this kernel is scaled by contact load. Every
> destabilising one is not."**

The table under it (`docs/gi4_ride_handoff.md:566-573`) lists three restoring terms
(suspension normal, μ bite, roll assist) against two destabilising ones (planing lift, GI4
plate). That law was written **2026-08-13**. Two dials have landed since. Here is the full
roster on today's code.

### 2.1 Restoring roll terms

| term | file:line | what it scales with | contact-load scaled? |
|---|---|---|---|
| suspension normal, rail split | sled.cpp:927-945, 950-968 | `normal` (patch load), couple saturates at `normal · track_rail_half_m` | **YES** |
| suspension normal, pitch split (roll-neutral, pitch-restoring) | sled.cpp:916-949 | `normal` | **YES** |
| ski pair (two patches at ±stance/2) | sled.cpp:66-71 + 10f | `N_R − N_L` at ±0.4635 m | **YES** |
| lateral μ bite | sled.cpp:1053-1069 | `normal · μ_l · tanh(slip/0.30) · (1 + 0.18·align)` | **YES** |
| sliding friction | sled.cpp:1025 | `μ_kin_eff · normal` | **YES** |
| player brake | sled.cpp:1250-1256 | `min(2600, max(0,(μ_brake − μ_kin_eff)·normal))` | **YES** (since GI S1) |
| roll assist A + damping | sled.cpp:1876-1881 | `(−stiff·tanh(φ/0.14) − 800·ω_z) · release_eff · w_contact` | **YES**, via `w_contact` — but `stiff` itself now grows with **speed** (F-B) |
| C1 hull normal + μ | sled.cpp:1513-1518 | `side_k·pen − side_c·v_n`, its own contact | **YES** (hull contact) |
| C2 righting bias | sled.cpp:1606-1608 | `700 · w_v(v_side) · w_t(φ_surf) · w_side · w_load · w_ω` | **YES** via `w_load` |
| C3 yaw arrest (roll spill) | sled.cpp:1645-1655 | `I_eff·|ω_vert|/h · 0.35 · hull_engage_lp` | **YES** via `hull_engage_lp` |
| **planing lift, rail/pitch split couple** | **sled.cpp:806-847** | **`½ ρ_eff v_tan² A sin α · plane_gain · draft`, couple ≤ `lift · rail_half`** | **NO — v²** |
| **R4a self-right** | **sled.cpp:1776-1792** | **`2400 · p_eff · charge · dir · w_pump`** | **NO — nothing at all** |
| G1 gyro precession | sled.cpp:1969-1972, 2057 | `k_gyro · L_raw`, `k_gyro = 0` | dead (§5) |

### 2.2 Destabilising roll terms

| term | file:line | what it scales with | contact-load scaled? |
|---|---|---|---|
| planing lift, single-point residual at the mount | sled.cpp:840-849 | `½ ρ_eff v_tan² A sin α · draft` | **NO — v²** |
| GI4 planing lateral plate | sled.cpp:1088-1090, 1333 | `½ ρ_eff v_tan² A sin(slip_clamped) · 0.3 · draft` | **NO — v²** (`plane_lat_load_frac` ships **0.0**) |
| track thrust | sled.cpp:1170-1224 | `area·c_eff·(1−e^…)` + `roost_gain·flux·ρ_eff·A·v_rel²`, capped by `min(2272, P·breathing/max(|v_fwd|,2))` | **NO** (`traction_mu` ships **0.0**, so the CONTACT ceiling at 1219-1222 is a dead branch) |
| plow drag | sled.cpp:1036-1039 | `½ ρ_eff v_fwd² · width · immersion · plow_cd` | **NO — v²** |
| gravity at the CG against the patch offsets | sled.cpp:1933 + `patch_geometry` | `m g`, arm = `cg_height_m` minus the support polygon | constant |

### 2.3 VERDICT

**The destabilising half of §9.6 is CONFIRMED on today's code, unchanged.** Both dials
that were built to break it — `traction_mu` (GI4 §9.7 item 2) and `plane_lat_load_frac`
(§8.3 item 4) — **ship at 0.0 and are structural dead branches**. Confidence: MEASURED
(read off the shipped `config/scenario.toml` + `sim/sled.h` defaults; both are also
`0.0` in all six v17-era tapes' `params`).
*Killing mutation:* set `traction_mu > 0` or `plane_lat_load_frac > 0` in `sim/sled.h`;
the branches at `sled.cpp:1219` and `sled.cpp:1320` then execute and both terms become
load-scaled.

**The restoring half is REFUTED in its universal form, and has been since 2026-08-14.**
`plane_lift_split_frac = 1.0` (`sled.h:1165`, landed `8cf63ad88` 2026-08-14) splits the
planing lift across `rail_half`/`pitch_half` **by draft**, producing a roll couple of

&nbsp;&nbsp;&nbsp;&nbsp;`τ_roll = lift · (lfr − lfl) · track_rail_half_m`, saturating at
`lift_track · 0.19 N·m` when one rail is clear of its draft (sled.cpp:813-826, 838-847).

At the §9 attractor's measured `lift_track = 2194 N` that ceiling is **DERIVED: 417 N·m**,
and the split's own measurement (`sled.h:1124-1127`, MEASURED) reads the lift's roll moment
flipping **−145 N·m → +289 N·m** at the attractor. So there is now a restoring roll term
that **grows with v² and survives with zero patch load** — precisely the property §9.6 says
no restoring term has.
*Killing mutation:* set `plane_lift_split_frac = 0.0` (`sim/sled.h:1165`). `lfr = lfl = 0.5`
is analytically the mount, the four `add_at` calls collapse to one, the couple is identically
zero, and §9.6's universal is restored (that is the pre-2026-08-14 kernel, §9.2's −216 N·m).

**A second, weaker refutation:** GI3 F-B (`roll_stiff_vgain = 1.0`, `sled.cpp:1863-1864`)
makes the assist ceiling `450 · (1 + 1.0·g(v))` — **900 N·m** above `assist_v_hi_ms` 16 m/s,
450 N·m parked. So the largest restoring term is now *jointly* scaled by contact load **and**
by speed. It still cannot survive zero contact (`w_contact` multiplies the whole thing),
so this refines §9.6 rather than breaking it.
*Killing mutation:* `roll_stiff_vgain = 0.0` — the branch at `sled.cpp:1850` is skipped and
`stiff` is the flat 450.

**A third, structural refutation, and the serious one:** the **R4a self-right**
(`sled.cpp:1679-1800`) adds up to **2400 N·m** to `torque_body.z` and reads **no contact
quantity whatsoever** — not `normal_sum`, not `side_normal_sum`, not `ground_contact`, not
`w_contact`. Its only gates are tilt off **radial up**, a low-passed **horizontal** speed,
`in.stand`, and `hands_on`. See §6.2.
*Killing mutation:* grep the block for any of `normal_sum`, `side_normal_sum`,
`ground_contact`, `w_contact`, `patchN` — there are zero occurrences between `sled.cpp:1679`
and `sled.cpp:1800`. Add any one of them as a multiplier and the claim dies.

### 2.4 ⚠ THE DENOMINATOR IN §9 IS WRONG — 4106 N vs 3246 N

`docs/gi4_ride_handoff.md` §2 writes: *"Static weight is (331 + 87.5) × 9.81 = **4106 N**"*,
and §9.2/§9.3 then report "62 N of its 4106 N", "2845 N on a 4106 N machine", "913 N of the
4106 N it weighs — 22 %".

**The kernel's machine weighs 3246.0 N.** `sled.cpp:1933` is
`force += -9.80665 * mass * up_cg` with `mass = max(p.mass_kg, kEps)` (`sled.cpp:224`) and
`mass_kg = 331.0` documented as *"machine + rider, TOTAL"* with `rider_mass_kg = 87.5`
*"split OUT of mass_kg, NEVER added"* (`sled.h:543`, `sled.h:784-785`).
DERIVED: 331 × 9.80665 = **3245.99 N**.

**`rider_mass_kg` never enters a force anywhere in the kernel.** Its only two uses are
`rider_frac` (sled.cpp:459) and `mu_exch` (sled.cpp:464), both ratios.
*Killing mutation:* `grep -n "rider_mass_kg" sim/sled.cpp` returns lines 431 (comment), 459,
464 and nothing else. If the kernel added the rider on top, changing `rider_mass_kg` would
change the weight; it does not.

**Consequence, and it runs the safe way:** the GI4 trace's contact fractions are
**understated**. At the attractor the running surfaces hold 62/3246 = **1.91 %** (not 1.51 %)
and the planing lift carries 2845/3246 = **87.6 %** of the weight (not 78 %). The contact
collapse §9.6 describes is *worse* than the document that named it. Confidence: DERIVED from
MEASURED inputs (the 62 N / 2845 N are §9.2's measurements; only the denominator is recomputed).

**This is not cosmetic for one term.** Two live gates carry the same denominator and are
therefore *correct* while the doc is wrong:
* `w_contact = clamp01((normal_sum + 0.5·side_normal_sum) / (0.5·mass·9.80665))`
  (`sled.cpp:1842-1844`) → denominator **1623.0 N**, i.e. the assist saturates at half the
  machine's weight on the patches. DERIVED.
* C2/C3's `w_load` denominator `0.02·mass·9.80665` (`sled.cpp:1566`, `1625`) → **64.92 N**,
  which is the "saturates at 2 % of weight (~65 N)" the comment claims. DERIVED, and the
  comment is right.
*Killing mutation for both:* change `mass_kg`; both denominators move with it. They are not
literals.

---

## 3. HYSTERESIS AUDIT — EVERY GATE, LATCH AND BAND

`sim/sled.h:226-228` claims, of the self-right speed gate: *"**Hysteretic like every gate in
this kernel**: it releases at this speed and re-arms at 0.8x."*

**That sentence is false.** It is the only gate in the kernel with two thresholds.
*Killing mutation:* `grep -n "rearm\|hyster" sim/sled.cpp sim/sled.h` returns exactly one
implementation site (`sled.cpp:1695`, `right_assist_rearm_frac`) plus its own comments.
Produce a second gate anywhere in `step_sled` whose arm and disarm thresholds differ, and the
claim dies. I found none in 39.

| # | gate / latch / band | file:line | threshold(s) | hysteretic? |
|---|---|---|---|---|
| 1 | `rolled` attitude | sled.cpp:254 | `dot(body_up,up_cg) < cos(1.31) ` = **75.06°** | **NO** — single hard edge |
| 2 | `rolled` dwell | sled.cpp:255-262 | rise `+h` while attitude ∧ `air_s ≤ 0.20 s`; decay `−2h`; latch at `hold ≥ 0.30 s`; cap 0.80 s | asymmetric **rate** (2:1), not angle hysteresis; **latch-off is instantaneous** |
| 3 | grip release | rider_grip.cpp:96 | `load_lp > capacity` **70.0** | **ONE-WAY LATCH** (never re-attaches in-kernel; R4e is the remount) |
| 4 | clutch engage | sled.cpp:1120-1126 | smoothstep `v_fwd` over **[3.00, 3.50] m/s** | **NO** — a continuous ramp that deliberately replaced a hard step (§8 P1-2: "a hard step at 480 Hz limit-cycles on downhills") |
| 5 | drive (thumb) blend | sled.cpp:1137-1139 | smoothstep `throttle` over **[0.02, 0.10]** | **NO** |
| 6 | engine-brake gate | sled.cpp:1237 | `brake < 0.05` **OR** `engine_brake_stacks` | **DEAD** at the shipped `true`; a hard edge if ever set false |
| 7 | player brake | sled.cpp:1250 | `brake > 0` ∧ `v_fwd > 1e-9` | **NO**, and the force does **not** taper with `v_fwd` — see §3.1 |
| 8 | patch-in-air | sled.cpp:751 | `normal ≤ 0 ∧ x ≤ 0` | **NO**; force is continuous through it |
| 9 | `axis_dot` clamp | sled.cpp:650 | `max(dot(body_up,up_i), 0.25)` binds past **75.52°** | hard clamp (caps the geometry divisor at 4×) |
| 10 | SK-1a dial blend | sled.cpp:632 | `gs.surf_mix > 0` | **DEAD on main** — see §5.7 |
| 11 | lift split branch | sled.cpp:806 | `plane_lift_split_frac > 0 ∧ (rail_half > ε ∨ pitch_half > ε)` | structural branch |
| 12 | rail/pitch unilateral clamps | sled.cpp:919-920, 930-931, 954-955 | `max(0.5N ± susp_k·dx, 0)` | one-sided saturation, value-continuous |
| 13 | bump stop | sled.cpp:706-707 | `x > susp_travel_m` **0.26 m** → `+ susp_k·5·(x−0.26)` | C0 continuous, slope step ×6 |
| 14 | `susp_x` clamp | sled.cpp:704 | `[0, 2·susp_travel_m]` = **[0, 0.52] m** | hard |
| 15 | hull engage | sled.cpp:1438-1443 | branch at `|φ_surf| > 25°`; smoothstep `w_side` over **[25°, 35°]** | **NO**; `w_side = 0` at the branch edge so the value is continuous |
| 16 | C2 slide-speed ramp | sled.cpp:1555-1559 | `clamp01((v_side − 1.5)/(6.0 − 1.5))` m/s | **NO** |
| 17 | C2 hull-load ramp | sled.cpp:1565-1566 | `clamp01(side_normal_sum / 64.92 N)` | **NO** |
| 18 | C2 roll window | sled.cpp:1574-1581 | smoothstep `|φ_surf|` over **[40°,50°]** × **[140°,125°]** | **NO** |
| 19 | C2 roll-rate gate | sled.cpp:1603-1605 | `clamp01(1 − |ω_z| / 2.5 rad/s)` — **inverse**, fades OUT with spin | **NO** |
| 20 | C3 gate | sled.cpp:1630-1632 | `side_yaw_mu > 0` ∧ `hull_engage_lp > 1e-9` ∧ `|ω_vert| > 1e-4` | **NO**; `hull_engage_lp` is a 0.1 s LP (memory, not hysteresis) |
| 21 | C3 ceiling clamp | sled.cpp:1645-1648 | `T ∈ ±I_eff·|ω_vert|/h` | hard clamp, defensive |
| 22 | **self-right speed gate** | **sled.cpp:1694-1699** | **disarm at `right_gs_lp > 1.3889 m/s`, re-arm at `< 1.1111 m/s`** | **★ YES — the only one** |
| 23 | self-right tilt ramp | sled.cpp:1683-1687 | smoothstep tilt over **[0.25, 0.35] rad = [14.32°, 20.05°]**, measured off **radial up** | **NO** |
| 24 | self-right direction | sled.cpp:1760-1763 | `tanh(e / 0.04)`, `e = −up_body.x − 0.1736·braced` | **NO**; near-`sgn` at the shipped ε — see §4.3 |
| 25 | self-right brace latch | sled.cpp:1788-1791 | side held while `right_shift_cmd ≠ 0.0` (exact-zero test), cleared on release | **LATCH** |
| 26 | self-right charge | sled.cpp:1714-1717 | contracting ODE, `τ_push` 0.6 s / `τ_rest` 0.7 s, clamp [0,1] | memory, continuous |
| 27 | B1 turn-sign ramp | sled.cpp:586-592 | smoothstep `|ω·up_cg|` over **[0.02, 0.07] rad/s** × `clamp01(sign-matched lean)` | **NO** |
| 28 | B2 release shift | sled.cpp:1826-1829 | `ext = 0.15 rad · clamp01(lean_frac·sgn φ)` | **NO** |
| 29 | A release band | sled.cpp:1830-1834 | smoothstep `a = max(0,|φ|−ext)` over **[0.60, 1.20] rad = [34.4°, 68.8°]** | **NO** |
| 30 | A release floor (F-C) | sled.cpp:1869-1874 | `max(release, 0.3·g(v)·fade)`; fade smoothstep over **[0.60, 1.45] rad**; `g(v)` smoothstep over **[6, 16] m/s** | **NO** |
| 31 | A contact gate | sled.cpp:1842-1844 | `clamp01((normal_sum + 0.5·side_normal_sum) / 1623.0 N)` | **NO** |
| 32 | K2 airborne gate | sled.cpp:2096 | `k_air_shift > 0 ∧ !ground_contact` (binary: ANY patch or hull penetration) | binary; **DEAD** at `k_air_shift` 0.0 |
| 33 | G2 reaction branch | sled.cpp:2068 | `k_gyro_react > 0` | **DEAD** at 0.0 |
| 34 | traction cap branch | sled.cpp:1219 | `traction_mu > 0` | **DEAD** at 0.0 |
| 35 | plate load-share branch | sled.cpp:1320 | `plane_lat_load_frac > 0 ∧ steered ∧ mean > ε` | **DEAD** at 0.0 |
| 36 | hull cohesion branch | sled.cpp:1525 | `hull_shear_width_m > 0` | **DEAD** at 0.0 |
| 37 | penetration rail | sled.cpp:2117 | `r_after < floor_r` | hard; asserted never to fire in the goldens |
| 38 | `air_s` reset | sled.cpp:2131-2132 | `normal_sum + side_normal_sum > **1.0 N**` **OR** `(r − floor_r) < 1.2·cg_height =` **0.6768 m** | **NO** — two single-valued hard edges |
| 39 | denominator floors | sled.cpp:1054, 1144, 1210 | `max(|v_fwd|, 0.5)`, `max(v_track, 1.0)`, `max(|v_fwd|, 2.0)` | continuous floors |

### 3.1 Two hard edges that can chatter, DERIVED

**(a) `rolled` → throttle, at 75.06°.** `throttle = (s.rolled || !hands_on) ? 0.0 : …`
(`sled.cpp:292-293`). `rolled` requires `attitude_now` **instantaneously** (254, 261) — so
once `rolled_hold_s` has saturated, `rolled` follows the 75.06° test **substep by substep**,
at 1440 Hz, with no hysteresis and no de-latch delay. The C1 hull comment
(`config/scenario.toml:2124`) states a downed machine *"rests ~65-75 deg instead of flopping
inverted"* — **75.06° sits inside that resting band**. A machine settling on its hull near
that attitude will toggle `rolled` (and with it the whole throttle) on contact jitter.
Confidence: DERIVED (code structure + the resting band from the shipped comment).
*Killing mutation:* give `attitude_now` a second threshold (e.g. latch at 75.06°, release at
70°), or require `rolled_hold_s` to fall to 0 before clearing. Either kills the chatter path.
**UNVERIFIED on tape** — no instrument in `tape_summary.json` counts `rolled` transitions per
second; `kernel_rolled_flag_s` is a total, not an edge count.

**(b) player brake at `v_fwd > 1e-9`.** `sled.cpp:1250-1256` applies the full brake force
whenever `v_fwd` exceeds 1e-9, with **no taper**. At `v_fwd = 1e-8` the term is at full
magnitude; at `v_fwd = 0` it is exactly zero; the force can drive `v_fwd` negative inside one
substep and then vanish. This is a discontinuous term at a stop. Confidence: DERIVED.
*Killing mutation:* multiply by `tanh(v_fwd / v_ref)` (any `v_ref > 0`) — the discontinuity
goes and the magnitude is unchanged above `v_ref`. Or: remove the `v_fwd > kEps` test and the
term reverses sign at zero, which is the failure this edge exists to avoid.

---

## 4. CONFIG-vs-DEFAULT DRIFT — `config/scenario.toml [sled_comfort]` vs `sim/sled.h`

**Method.** Parsed all 35 numeric keys under `[sled_comfort]` (`config/scenario.toml:1994-2142`)
and all 39 member initialisers of `struct SledComfort` (`sim/sled.h:187-529`), compared
exactly. MEASURED (this session, arithmetic-free comparison of parsed doubles).

`config/load_scenario.cpp:480-485` states, verbatim:

> *"The committed toml reproduces `sim::SledComfort{}` **exactly** (bit-neutral load,
> asserted by `scenario_sled_comfort_matches_kernel_defaults`)"*

and `app/main.cpp:2480` repeats it: *"Committed values == kernel defaults, bit-neutral."*

**Both sentences are false for three fields, and the named test does not check any of them.**
`test/unit/test_load_scenario.cpp:70-103` checks **22** of the 35 loaded keys. The **13
`right_*` keys are not in the test at all** — and three of them are exactly where the drift is.
*Killing mutation:* add `CHECK(s.sled_comfort.right_assist_nm == d.right_assist_nm);` to that
TEST_CASE. It goes red immediately.

| field | sled.h default | scenario.toml | the comment that justifies it | verdict |
|---|---|---|---|---|
| `right_assist_nm` | **0.0** (sled.h:224) | **2400.0** (:2085) | scenario.toml:2061-2084 argues **300 → 900 → 1500** and closes *"Chad rules the final value on his drive"* | **the comment never mentions 2400** |
| `right_charge_push_s` | **1.0** (sled.h:262) | **0.6** (:2100) | :2095-2096 *"derived from the machine's own measured rock half-period (**~0.93 s**): one press ~ one upswing"*; sled.h:260-261 gives the same derivation for **1.0** | **0.6 ≠ 0.93 ≠ 1.0** |
| `right_dir_eps` | **0.1736** (sled.h:274) | **0.04** (:2102) | :2097-2098 *"dir_eps/seed_frac are **sin(10 deg)**"*; sled.h:270-274 *"full authority everywhere except a **~10 deg cone**… 0.1736 = sin(10 deg)"* | **0.04 = sin(2.29°)** |

Everything else in the loaded roster matches exactly, including all three **GI3** dials
(`assist_hull_frac` 0.5, `roll_stiff_vgain` 1.0, `release_floor_frac` 0.3) and
`roll_damp_nms` 800.0.

### 4.1 `right_assist_nm` 0.0 → 2400.0

`sim/sled.h` is **honest** here: *"★ SHIPS 0-OFF … the tape-absent preset pins it to 0.0 —
so all 31 existing tapes replay bit-exactly **while the game's config turns it on**"*
(sled.h:219-223). So the DEFAULT/CONFIG split is deliberate and documented.

What is not honest is the **argument** in `scenario.toml`. The file's own block sizes 300 N·m
(a pure static weight shift), rejects it, sizes 900, records that 900 stalls at 46°, proposes
1500 for a "one-press ceiling of 70°", and then **ships 2400** with no line explaining it.
`sim/sled.h:247-249` states this repo's law for exactly this case, verbatim:

> *"and the TESTS PROVED 4000 N m while the config SHIPPED 900 — they certified a kernel
> nobody ran. **THE LAW: a constant that describes the shipped table stops describing it the
> moment the table moves.**"*

The tests were fixed (`test/unit/test_sled_selfright.cpp:221` runs `shipped()`, which loads
`scenario.toml`, and asserts `dflt.right_assist_nm == 0.0 && p.right_assist_nm > 0.0`). The
**comment** was not.
*Killing mutation:* none needed — the claim is that 2400 appears in no justifying sentence.
Produce one in `config/` or `sim/` and it dies. `grep -rn "2400" config/scenario.toml sim/sled.h`
returns only the assignment.

**Tape corroboration, MEASURED:** all six `kernel-v17-tremor-signed` tapes carry
`cparams.right_assist_nm = 2400.0`. So 2400 is what Chad actually drove.

### 4.2 `right_charge_push_s` 1.0 → 0.6

The press budget drains as `charge' = −p_eff·charge/τ_push` (`sled.cpp:1714-1716`). DERIVED:
the total torque-impulse a single held press can deliver is
`∫ τ dt = right_assist_nm · τ_push · w_pump_avg`. At the shipped pair (2400, 0.6) with
`w_pump` running 0.5→1.0 that is **≈ 1080 N·m·s**, against **≈ 1800 N·m·s** if the header's
1.0 shipped. So the config is buying a *shorter, harder* press than the header describes.
*Killing mutation:* set `right_charge_push_s = 1.0` — the integral changes by 1.67× and every
`rock()` leg in `test_sled_selfright.cpp` re-times.

### 4.3 `right_dir_eps` 0.1736 → 0.04 — the dead-cone is 2.3°, not 10°

`dir = tanh(e / right_dir_eps)` (`sled.cpp:1762-1763`). DERIVED: `|dir| ≥ 0.96` at
`|e| ≥ 1.946·ε`, i.e. at `ε = 0.1736` the direction is saturated outside **|e| ≈ 0.338**
(≈ 19.7° of attitude error, the "~10 deg cone" the comment means as a half-scale) and at
`ε = 0.04` it saturates outside **|e| ≈ 0.078** (≈ **4.5°**). The eps itself corresponds to
`asin(0.04) =` **2.29°**, not the documented `asin(0.1736) =` **10.0°**.

**The practical consequence: at the shipped value the direction term is effectively `sgn()`.**
The whole point of the tanh (sled.h:268-274) was to avoid *"a sgn() discontinuity to chatter on
at the top"*. At ε = 0.04 the cone where it is not already saturated is 4.5° wide.
*Killing mutation:* set `right_dir_eps = 0.1736` (the header value the comment describes); the
saturation boundary moves 4.5° → 19.7° and the near-inversion behaviour changes measurably.

### 4.4 Four SledComfort fields are NOT loadable from config at all

| field | kernel default | consumed at |
|---|---|---|
| `side_hull_points` | **10** | sled.cpp:1481-1484 (hull point count) |
| `hull_shear_width_m` | **0.0** | sled.cpp:1525-1532 (cohesion plough, OFF) |
| `side_yaw_mu` | **0.35** | sled.cpp:1630-1648 (C3 yaw arrest, **ON**) |
| `side_right_wref_rads` | **2.5** | sled.cpp:1603-1605 (C2 rate gate, **ON**) |

`config/scenario.toml:2133-2134` admits it in passing (*"the geometry half is the 10-point
hull line + the side_yaw_mu arrest channel, **kernel-default dials**"*). Two of the four are
**live, shipped mechanisms** that Chad cannot touch without a recompile. That is against
**RC-8**, verbatim from `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0b:

> *"**NEW RC-8 — EVERY MECHANISM IS A DIAL**: all new terms land as named, config-tunable
> parameters with measured defaults, so Chad can tune finesse without a recompile."*

*Killing mutation:* `grep -n "side_yaw_mu\|side_right_wref_rads" config/` returns one comment
line and no `require(...)`. Add the two `require` calls plus the toml keys and the finding dies.

### 4.5 And the whole of `SledParams` is compile-time only

`app/main.cpp` writes exactly **two** things into `sled_params`: `comfort = scen.sled_comfort`
(2481) and, from an env var, `grip.capacity` (2492-2493).
*Killing mutation:* `grep -n "sled_params\." app/main.cpp` → lines 2481, 2493 (writes), 2587
and 3697 (reads). Nothing else.

So `traction_mu`, `plane_lat_gain`, `plane_lat_lean_gain`, `plane_lat_load_frac`,
`plane_lift_split_frac`, `k_gyro`, `k_gyro_react`, `k_air_shift`, `roll`-adjacent geometry
(`track_rail_half_m`, `track_pitch_half_m`), `aft_ceiling_curve`, `lean_return_tau_s` and
every mass/geometry number are **recompile-only**. Under RC-8 that is 12+ named mechanisms
Chad cannot A/B from the seat.

---

## 5. DIALS THAT SHIP OFF OR DEAD — AND WHAT EACH WOULD DO ON

| dial | file:line | shipped | OFF is structural? | what ON would do |
|---|---|---|---|---|
| `k_gyro` | sled.h:755 | **0.0** | YES — `L_r` is the exact zero vector, `Iω` bit-identical (sled.cpp:1969-1972) | Rotor precession: a yaw rate makes a **roll** torque and vice versa, through `ω × (Iω + L_r)`. `L_raw = −0.193 · v_belt/0.0815` kg·m²/s → DERIVED **−108.9 kg·m²/s at 46 m/s**. Pitch is immune by construction (parallel to `L_r`). |
| `k_gyro_react` | sled.h:768 | **0.0** | YES — branch at sled.cpp:2068 | G2 reaction wheel: spinning the track up/down throws `−dL/dt` into **pitch** as a rate delta. The delta telescopes, so a spin-up/down cycle nets exactly zero. This is "blip the throttle in the air and the nose comes up". ⚠ named gap (sled.cpp:2064-2067): the belt is kinematic, so it spins up for free — the chassis is charged for momentum the engine was never charged for. |
| `k_air_shift` | sled.h:859 | **0.0** | YES — branch at sled.cpp:2096 | Airborne momentum exchange: `dω = −k·I⁻¹·dL_exch`. Sizing (sled.h:848-849, LITERATURE-from-comment): μ·y·v_rel ≈ 64.4·0.44·1.40 = 40 kg·m²/s over `I.x` 158.7 → **~0.25 rad/s of transient pitch**. Produces an attitude change, never a sustained rate. |
| `plane_lat_lean_gain` | sled.h:1061 | **0.0** | inert while `plane_lat_gain` is 0; here it is 0 itself | The **aimed** lean reward: `lat *= 1 + gain·align` on **steered patches only** (sled.cpp:1101-1102). This is the one that tightens an arc; B1's machine-wide `lean_bite_gain` is measured self-defeating (sled.h:1092-1097, MEASURED: 0.18 → 0.45 took the lean-in carve from **30 m to 295-1037 m** because the track out-gains the skis). |
| `plane_lat_load_frac` | sled.h:1090 | **0.0** | YES — branch at sled.cpp:1320 | Mean-normalised load share on the plate, restoring the inside/outside asymmetry the μ bite gets free. **Hypothesis FALSIFIED, not unfinished** (sled.h:1081-1089, MEASURED): tip onset 0.318 g → 0.334 g (~5 % of the 1.035 g gap) and the 16/20 m/s carve cells bifurcate into a −28° non-turning runaway. Kept wired as the record. |
| `lean_return_tau_s` | sled.h:829 | **0.0** | YES — `slew_toward` targets the commanded position, full stop | An auto-centring spring on the rider's lateral axis. Deliberately off: *"side-hilling is HELD for many seconds, and an auto-centring lateral axis cannot side-hill (the mousepad runs out)"* (sled.h:826-828). Turning it on would be a **governor** in the §0b sense. |
| `traction_mu` | sled.h:1006 | **0.0** | YES — branch at sled.cpp:1219 | The **only CONTACT ceiling** on thrust; both existing caps are ENGINE ceilings. MEASURED (`docs/snowform_measurements.md` §M8.1): at `mu ≥ 0.6` the §9 Bush runaway does not develop (`t(|roll|>20°)` 6.22 s → 1.33 s, `v_end` 24.8 m/s rising → 0.1 m/s); at `mu ≥ 3.0` Chad's traverse is **byte-identical to baseline** (14.1/14.5/15.3 m/s). ⚠ M8.2/M8.3: it is **inert on Road by construction** (`rho_eff = 0`, no planing, full contact), so it does **not** fix the bank-strike Chad reported. |
| `hull_shear_width_m` | sled.h:479 | **0.0** | YES — branch at sled.cpp:1525 | Cohesion plough on the hull's sliding contacts. MEASURED inert (sled.h:470-476): ~11 N against ~300 N of the `side_mu` term beside it, <5 %. Ships off *"per the spec's own instruction (a decoration is worse than an honest absence)"*. |
| `class_blend_m` (`[snowpack]`, world.toml:645) | world/snowpack.cpp:722 | **0.0** | YES — branch; `surf_mix` stays 0 so `sled.cpp:632` never blends | See §5.7 — this is the big one. |
| `right_assist_min_tilt_rad` | sled.h:342 | 0.35 | — | **DEAD.** Loaded (`load_scenario.cpp:514-515`), taped (`sled_tape.h:82`), **never read by `sled.cpp`**. The RAMP (`right_tilt_lo/hi_rad`) owns the gate; scenario.toml:2094 admits it (*"kept for the loader"*). *Killing mutation:* `grep -n right_assist_min_tilt_rad sim/sled.cpp` → zero hits. |
| `right_seed_frac` | sled.h:286 | 0.1736 | — | **ALIVE BUT RE-PURPOSED, and the config comment still describes the deleted behaviour.** It multiplies `braced` (the **latched brace side**, sled.cpp:1756-1761), never the rider's lean. `sled.cpp:1731-1742` records Chad's correction and the deletion. But `config/scenario.toml:2098-2099` still reads *"a full **LEAN** counts like 10 deg of attitude error — **his lean is what picks the side**"*, which is precisely what he ruled out. |

### 5.7 ⚠ SK-1a IS PRESENT IN THE KERNEL AND UNREACHABLE ON MAIN

`sled.cpp:630-636` blends the per-patch `SurfaceDials` across a class transition, gated on
`gs.surf_mix > 0.0`. `surf_mix` is written in exactly one place —
`world/snowpack.cpp:722-746` — inside `if (p.class_blend_m > 0.0 && …)`. And
`config/world.toml:645` is `class_blend_m = 0.0`.
**Therefore `surf_mix` is identically 0 in the shipped game and `blend_dials` (sled.cpp:200-213)
is never called.** MEASURED (code + shipped config).
*Killing mutation:* set `[snowpack] class_blend_m = 1.0`; `blend_dials` executes.

**Why it matters.** `docs/snowform_measurements.md` §M8.3 names the per-patch class step as
**"the live candidate for the Road bank-strike"** — Chad's own reported superspin — and §M9.1
MEASURED it on `seads_sled_probe bankgraze`: a **25 cm** lateral move (one ski crossing the
corridor edge), **straight, throttle open, NO steer input**, takes the machine from
*yaw 0.0° / roll 2.4°* to **89° of roll at 16 m/s** and **1.34 rotations at 24 m/s**; at
6.00 m offset, 24 m/s, **−597.8° of yaw and 171° of roll — fully inverted**.

`LANES.toml:939` records the ruling that keeps it off, and it is a *deliberate* ruling, not an
oversight: *"⚠ THE STICK RULING: [snowpack] class_blend_m STAYS 0.0 — it is live-tunable and
it is CHAD'S A/B, not the lane's call. The lane measured 1.0 as the value it would pick; it
ships 0.0 and he decides in the seat."*

**So the audit's finding is not "someone forgot": it is that the one measured fix for the one
rollover Chad named by mechanism is sitting behind a dial he has not yet been asked to rule on.**

---

## 6. TERMS THAT FIGHT THE CHARTER

The charter is `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0 and §0b. Every
quote below is verbatim with doc + section. **Nothing in this section is built.** Each
candidate cure names ONE dial and its identity value.

**The acceptance bar, §0 (drive 4, 2026-08-12):**
> *"Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** … Also I've seen lots of snowmobiles **drift around corners and then with
> throttle, straighten out. The feel is really good. Don't lose the feel**, except the
> constant rolling. … **Just allow the balance of body mechanism to ENHANCE ability, i.e.
> tighten a turn instead of having to be the necessary condition of not rolling over.** Just
> allow leaning a certain way in a particular condition to be the OPTIMAL weight distro for
> better traversing, **say up a hill** or around a corner."*

**§0b (superseding, 2026-08-12 night):**
> *"no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor.
> Make it less honest but dont ruin it. … Leaning shall enhance the ride an just make it more
> stable and slef righting by chance more."*
> — decoded at §0b: **"NO GOVERNOR. No automatic rider micro-balance, no autopilot moving the
> rider's mass. The player's lean stays the only thing that moves the rider."**

**The felt report, `docs/gi4_ride_handoff.md` §1, verbatim (his words, left column):**
> 1 *"tips over too easy"* · 2 *"should be able to launch in the air"* ·
> 3 *"turning too unstable, can't hold a carve"* · 4 ***"WOT should lift the skis to ~30°"*** ·
> 5 *"rider weight needs authority, not thrown into a spin"* · 6 *"flips happen too fast and easy"*

**Tape baseline for this section** — the six `kernel-v17-tremor-signed` tapes, which carry
today's shipped dials (`right_assist_nm` 2400, `roll_damp_nms` 800, `roll_stiff_vgain` 1.0,
`traction_mu` 0.0, `plane_lat_load_frac` 0.0, `plane_lift_split_frac` 1.0, `k_air_shift` 0.0).
MEASURED by `tools/sled_tape_audit.py`; definitions spot-verified at the lines named.

| quantity | value | tool definition, verified |
|---|---|---|
| total drive | **721.5 s (12.02 min)** over 6 tapes | — |
| rollovers past 90° | **52**, = **4.32/min** | `dot(body_up,up_cg) < 0` held ≥ 0.10 s (sled_tape_audit.py:581-593) |
| time past 90° | **41.4 s = 5.7 %** of the drive | :582, :900 |
| `R` autoright presses | **5** | override class `autoright_R` |
| assist applying torque | **80 – 94 %** of ticks | `assist_nm != 0.0` (:570-571) |
| STAND held hard (`in.stand > 0.5`) | **6.8 – 39.0 %** of ticks (mean ≈ 24 %) | :564-565 |
| catwalk (stand>0.5 ∧ lean_fwd<−0.30 ∧ throttle>0.90 ∧ grounded) | **2.9 – 22.8 %** of ticks | :575-577 |
| catwalk pitch p99 | **5.25° – 80.25°** across the six | :577 |

### 6.1 `roll_damp_nms = 800` vs **RC-3, the drift is canon** — "Don't lose the feel"

**Mechanism.** `sled.cpp:1876-1881`: `τ = (−stiff·tanh(φ/0.14) − **800·ω_z**) · release_eff ·
w_contact`. The damping is **not** shaped by φ — it acts on raw roll rate the whole time the
machine is in contact and inside the release band.

**Size, DERIVED.** Gravity's peak tipping torque about the rail pivot is
`m·g·hypot(0.19, 0.564) = 331 · 9.80665 · 0.59515 =` **1932.0 N·m** (the same number
`sled.h:244-246` derives). The damping term equals that at
**ω_z = 1932/800 = 2.415 rad/s = 138.4 °/s**. At the roll rate Chad's own tape peaked at —
**654 °/s = 11.414 rad/s** (MEASURED, gi4_ride_handoff §1 item 6) — the damping torque is
**9131 N·m**, i.e. `α = 9131/49.7 =` **183.7 rad/s²** on `I.z = 49.7`, and it is **10.1×**
the assist's own saturated ceiling of 900 N·m.
*Killing mutation:* `roll_damp_nms = 0.0` — the second half of the bracket at sled.cpp:1879
vanishes and the term is the pre-GI4 `−stiff·tanh(φ/0.14)` alone.

**The charter conflict is admitted in the tree.** `sled.h:362-365`: *"★ SHIPPED AT ZERO on the
consult's do-not list: damping is the dial that kills **the flick, the drift's body language**
and backflip initiation off a lip."* And `sled.h:373-377`: the air is safe by construction
(`w_contact` is 0 airborne) but *"What it DOES reach is the consult's other named cost: **the
drift's body language, on the ground**. That is the part that is Chad's call, not
measurement's."* It was raised to 800 on his 2026-08-13 ruling (`b971d14a9`) against item 6.

**Tape event.** The assist (stiffness **and** damping, same multiplier) is applying nonzero
torque for **80–94 %** of his v17 riding — it is not an edge-case rescue, it is the ambient
condition of the machine. And the rollover rate at that setting is still **4.32/min**.
**Candidate ONE-dial cure (NOT BUILT):** `roll_damp_nms`, identity value **0.0**. The honest
intermediate is that the damping buys the *arc* as well as safety (`sled.h:378-382`, MEASURED:
lean-in arc on Bush 323 m at damp 0, 143 m at 150, 42 m at 400, 30 m at 800) — so this is a
ruling, not a bug, and the number to put in front of him is the arc-vs-flick ladder, not a
single value.

### 6.2 ★ The R4a self-right vs **"NO GOVERNOR"** and **"MACHINE NEEDS TO BE TIPPING OVER"**

This is the strand's most serious mechanism finding. Three separate problems, one block
(`sled.cpp:1679-1800`).

**(a) It moves the rider's mass. The kernel, not the player.**
`right_shift_cmd` (written 1791) is consumed at 1401-1407 as `lat_target_sr`, which is what
`s.rider_lat_m` slews toward. `right_stand_shift_frac = 1.0 × lean_lat_stand_m 0.35 m` ⇒ the
kernel commands **up to 0.35 m of rider lateral displacement** with no player lean input.
`sim/sled.h:183-185` states the forbidden thing in its own words: *"What stays forbidden: a
governor (**nothing here reads or moves the rider's mass — the player's lean is the only thing
that moves the rider**)"*. §0b's decode says the same.
**This is authorised by a LATER Chad ruling**, verbatim (`sled.h:318-319`, and the same words
in `config/scenario.toml:2053-2056`): *"MACHINE NEEDS TO BE TIPPING OVER STANDING
AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED."* So the ruling supersedes §0b **for a machine that
is tipping over**. The audit's point is that the gate deciding "tipping over" is wrong — (b).

**(b) The tilt gate reads RADIAL UP, not the contact plane — the exact defect red-team
P1-1/2 fixed everywhere else.**
`sled.cpp:1680-1687`: `tilt = acos(up_body.y)` where `up_body = Rᵀ·up_cg`. That is tilt off
**gravity**, so a machine sitting perfectly conformal on a side-hill reads the slope angle as
"tipping over". Compare `sled.cpp:1367-1371`, which says why the hull terms do **not** do this:
*"a machine conformal to a slope is upright for hull purposes … (red-team P1-1/2: **the earlier
gravity-tilt gate armed rigid hull contacts during ordinary slope riding** and misread pitched
crashes as rollovers)"*. The hull was fixed to `phi_surf`; **the self-right was not**.

DERIVED: on a conformal **20.05°** slope, `w_tilt = smoothstep((0.34907−0.25)/0.10) =` **0.9997** —
full authority. `config/world.toml`'s own snowpack measurement block records terrain slope
**p95 = 21.1°** over 79,847 land samples (MEASURED, `offline_tool/measure_snowpack.py`), so
**~5 % of the world's land is at or past the full-authority edge of this gate.**
With `right_dir_eps = 0.04` the direction term is saturated outside ±4.5° (§4.3), so the
applied torque on that slope is essentially the full `2400 · charge · w_pump` N·m, rolling the
machine toward **gravity-vertical** — i.e. standing it up off the hill. The A assist already
carries a *deliberate, measured* uphill bias there (`roll_ref_blend = 0.5` ⇒ a settled 20°
side-hill reads φ ≈ 10°, sled.cpp:1804-1807), worth `450·tanh(10°/8.02°) ≈` **405 N·m**.
**The self-right adds a second, unmeasured bias in the same direction that is ~6× larger.**
*Killing mutation:* replace `acos(up_body.y)` at sled.cpp:1681 with `|phi_surf|` (already
computed at 1423, in scope). On flat ground nothing changes; on a conformal slope `w_tilt`
goes to 0 and the term is silent.

**Charter quote it fights**, §0: *"Just allow leaning a certain way in a particular condition
to be the OPTIMAL weight distro for better traversing, **say up a hill**."* And `sled.h:826-828`'s
own law: *"side-hilling is **HELD for many seconds**."*
**Tape event:** STAND is held hard (`in.stand > 0.5`) for **6.8–39.0 %** of the v17 drives
(MEASURED). STAND is a *routine riding control* on his tapes, not a rescue key — which is the
whole reason a mis-scoped gate on it matters.
**Candidate ONE-dial cure (NOT BUILT):** `right_stand_shift_frac`, identity **0.0** (kills (a),
leaves the torque) — or, better and still one dial, `right_assist_nm`, identity **0.0** (the
`sim/sled.h` default and what all 31 pre-R4a tapes replay at). The *correct* fix is not a dial
at all: it is the one-line gate swap above, and that needs its own rung, tests, and his ruling.

**(c) There is NO CONTACT GATE, so it is roll authority in the air.**
Between `sled.cpp:1679` and `sled.cpp:1800` there is not one reference to `normal_sum`,
`side_normal_sum`, `ground_contact`, `w_contact` or `patchN`. Every other comfort torque in the
kernel is contact-gated: A by `w_contact` (1842), C1 by penetration (1497), C2 by `w_load`
(1565), C3 by `hull_engage_lp` (1630). The speed gate is on **horizontal** ground speed
(`v_h_r = velocity − (velocity·up_cg)up_cg`, 1689-1691), so a fall with little horizontal
velocity reads ~0 and stays armed.

DERIVED authority: `2400 / I.z 49.7 =` **48.3 rad/s²**, and a single held press delivers
`≈ 2400 · τ_push 0.6 · w_pump_avg ≈` **1080 N·m·s** ⇒ **Δω_z ≈ 21.7 rad/s ≈ 1244 °/s** of free
roll with no ground under the machine. The kernel's airborne contract is stated four times over
(`sled.h:352` *"identically 0 airborne, so backflips and the airborne-lean anti-cheat leg stay
bit-exact"*; `sled.cpp:489-492` *"a cheat that made the machine rotate in the air by moving a
contact-patch mount remains impossible"*; `sled.h:614-618`; `sled.cpp:764-766`).
**There is no test covering it.** `test/unit/test_sled_selfright.cpp` has 11 TEST_CASEs; the
nearest, *"an upright machine is untouched"* (line 281), uses `t0 = 0.05 rad` (**2.9°**) on flat
ground and therefore covers neither the slope case nor the air case.
*Killing mutation:* add `&& ground_contact` (or `&& normal_sum + side_normal_sum > 0`) to the
condition at sled.cpp:1679. If the claim is wrong, that change is a no-op in every existing leg.
**Confidence: MEASURED for the code structure (grep), DERIVED for the impulse, UNVERIFIED on
tape** — no instrument in `tape_summary.json` joins `air_s > 0` with `stand > 0.5` and
`right_assist_nm_now != 0`, and `right_assist_nm_now` is not in the tape's positional pin roster
(`sled.h:1250-1258`), so it cannot be read back from the corpus at all.

### 6.3 The lean ENHANCER is delivered by the weak dial while the aimed one is OFF

**Charter, §0, verbatim:** *"**Just allow the balance of body mechanism to ENHANCE ability,
i.e. tighten a turn** instead of having to be the necessary condition of not rolling over."*
**Felt item 5, verbatim:** *"rider weight needs authority, not thrown into a spin."*

**Mechanism.** Two lean-reward paths exist. `lean_bite_gain = 0.18` multiplies the μ bite on
**every** patch including the track (sled.cpp:1067) — MEASURED self-defeating for an arc
(sled.h:1092-1097: 0.18 → 0.45 took the lean-in carve from 30 m to 295-1037 m, *"because the
track out-gains the skis"*). `plane_lat_lean_gain`, which can only ever add **ski** plate
(sled.cpp:1101-1102), **ships 0.0**.

**And the strongest lean reward was regressed on purpose.** `sled.h:1154-1157`, MEASURED:
shipping `plane_lift_split_frac = 1.0` re-pinned test 2336 — *"lean-in no longer tightens the
arc by 10 % (28.640 m vs 28.632 m free) — which is **Chad's own felt item 5, rider weight
authority**. It is carried as an open debt against §9.7 items 2+3, never as an absorbed cost."*
Those items 2 and 3 are `traction_mu` (ships **0.0**, §5) and a floor on the contact gate
(**never built** — no `w_contact` floor exists at sled.cpp:1842). **So the debt is still open
and both instruments that were supposed to pay it are off or absent.**

**Tape event.** `saturation.lean_gt_0p98_frac` on the v17 tapes: Chad holds **full lean** for
**14.8 – 43.6 %** of the drive (MEASURED, `tools/sled_tape_audit.py`). He is asking the reward
channel for everything it has, continuously.
**Candidate ONE-dial cure (NOT BUILT):** `plane_lat_lean_gain`, identity **0.0** (today). It is
the only dial in the kernel whose reward lands on the steered patches alone, it is inert
everywhere `plane_lat_gain` is (Road, LakeIce: `rho_eff = 0`), and it is the direction the GI4
measurement says actually tightens an arc. A value needs a sweep and his seat, not this document.

### 6.4 The wheelie is kept — and the dial that keeps it taxes felt item 5

**Charter, felt item 4, verbatim:** *"WOT should lift the skis to ~30°"*, MEASURED against
*"WOT + stand + lean-back: pitch **p50 3.6°, p95 10.8°**"* (gi4_ride_handoff §1).

**Mechanism.** `track_pitch_half_m = 0.20` (sled.h:619) splits the track's normal reaction
fore/aft, and the load shift **is** the pitch-restoring moment (sled.cpp:916-949). DERIVED
from the code: with both split points loaded, `fa − ff = 2·susp_k·dz/normal`, so the couple is
`2·susp_k·pitch_half²·sin(pitch)` ⇒ stiffness **2 · 46000 · 0.04 = 3680 N·m/rad = 64.2 N·m/deg**,
independent of load until one point unloads, after which it saturates at `normal · 0.20`.
*Killing mutation:* `track_pitch_half_m = 0.0` — a separate code path (the `else if` at
sled.cpp:950), bit-identical to the pre-S2b single-point track, and MEASURED to run the launch
wheelie away to **89.9° at t = 6 s, never coming back** (sled.h:576-581).

**The tax.** `sled.h:605-610`, MEASURED sweep: at 0.45 *"the fore-aft weight transfer the
rider's lean buys collapses to **0.32× of W/L**"*; 0.20 keeps `dN_ski/d(shift)` at **0.80×**.
So the dial that stops the runaway wheelie is, in the same motion, the dial that eats the
rider's fore-aft authority — felt item 5 again.

**Tape event.** The catwalk condition (stand ∧ hard lean-back ∧ WOT ∧ grounded) fires on
**2.9 – 22.8 %** of the v17 ticks, and the pitch it achieves there has p99 spanning
**5.25° – 80.25°** across the six tapes — i.e. on some drives the WOT-lean-back pose reaches his
30° and beyond, and on others it barely leaves 5°. MEASURED. That spread is the honest state of
felt item 4: it is **not** a flat "p50 3.6°" any more, it is **highly conditional**.
**Candidate ONE-dial cure (NOT BUILT):** `track_pitch_half_m`. Identity **0.0** = the old
single-point track (bit-identical branch, `sled_track_pitch_split_off_is_single_point`), which
is the 89.9° runaway. The measured green window is narrow — the sweep at sled.h:603-604 reads
**0.20 → 0 red**, 0.18 and 0.22 → 5 and 7 red — so this dial is **not** the right place to buy
wheelie back. Anything aimed at item 4 should go at `k_gyro_react` (§5), which is the term
riders actually use and which ships 0.0.

### 6.5 "Roll resistant" — where the machine actually stands, measured

**Charter, §0, verbatim:** *"**I rule that it should be roll resistant.** … **Make it possible
to roll but not the rule.**"*

MEASURED on the six v17 tapes: **52 rollovers past 90° in 12.02 minutes = one every 13.9 s**,
**5.7 % of the drive** spent past 90°, and **5** `R` autoright presses. For scale, the
pre-GI3/GI4 baseline his felt report was written against was *"56 rollovers past 90° in 8.5 min
= one per 9 s"* (gi4_ride_handoff §1 item 1, MEASURED).
DERIVED: **one per 9 s → one per 13.9 s is a 1.54× improvement** across everything that landed
between 2026-08-13 and 2026-09-17.
*Killing mutation:* the two rates come from the same instrument definition
(`dot(body_up,up_cg) < 0` held ≥ 0.10 s, `tools/sled_tape_audit.py:581-593`; gi4 §1 used the
same "past 90°" criterion). If gi4's count used a different dwell, the ratio moves.

⚠ **Both numbers are floors, not verdicts.** `rollovers_past90` counts *attitude*, not
*intent* — a deliberate backflip counts as a rollover, and §0's first sentence is *"Yep I can
do backflips."* No instrument in the corpus separates them. This is the single biggest
UNVERIFIED item this strand leaves behind (§8).

---

## 7. GIT VERIFICATION — WHICH DIALS ARE ON MAIN, AND WHEN EACH LANDED

Method: `git log -S"<symbol>" -- <file>` (pickaxe on the introducing commit) against this
worktree, whose kernel text is byte-identical to `origin/main` (§ header). All commits below
are ancestors of `origin/main`.

| rung | dial | shipped value | introduced | commit subject |
|---|---|---|---|---|
| **GI3** | `roll_stiff_vgain` | **1.0** | 2026-08-13 `bdcc9c7b3` | *GI3 ROLLFIX: the 360 roll + corkscrew fixed from the sink-measured mechanism — three stateless dials, judged by his own tape* |
| **GI3** | `assist_hull_frac` | **0.5** | 2026-08-13 `bdcc9c7b3` | (same commit) |
| **GI3** | `release_floor_frac` | **0.3** | 2026-08-13 `bdcc9c7b3` | (same commit) |
| **GI4** | `plane_lat_gain` | **0.3** | 2026-08-13 `dccc24e9f` | *GI4 RIDE (WIP, 1170/1175): Chad's 512 s tape measured, the carve root-caused, and two rulings owed* |
| **GI4** | `roll_damp_nms` (field) | — | 2026-08-12 `3b89a61cd` | *RC ROLL COMFORT: roll-resistant not roll-proof, under the SUPERSEDING 0b ruling* — **shipped 0.0** |
| **GI4** | `roll_damp_nms` (**= 800.0** in config) | **800.0** | 2026-08-13 `b971d14a9` | *GI4: Chad's two rulings applied, the geometry answer SIZED, and the tip-onset collapse root-caused* |
| **GI4** | `plane_lift_split_frac` | **1.0** | 2026-08-14 `8cf63ad88` | *GI4 §9.7 item 1 (Chad's ruling): give the planing lift the rail/pitch split* |
| **GI4 §9.7 it.2** | `traction_mu` | **0.0 (OFF)** | 2026-08-25 `27532eded` | *SK-1a: the bank-strike is a CLASS STEP — blend it (OFF), + GI4 item 2 measured and **declined*** |
| **K-WS1 / K1** | `kAftCeilC1` + `aft_ceiling_curve` | **1.0 (ON)** | 2026-08-21 `3f95dfc92` | *K-WS1: honest stand-dependent aft clamp (ON) + airborne momentum exchange (built, default OFF)* |
| **K-WS1 / K2** | `k_air_shift` | **0.0 (OFF)** | 2026-08-21 `3f95dfc92` | (same commit); touched again 2026-08-27 `b8e430415` *SNOW R1/R3 + tracks + gyro* |
| — | `side_yaw_mu` (C3) | **0.35 (ON, not config-loadable)** | 2026-08-12 `9e7e48a12` | *GI GROUND-INTERACTION RUNG: all five RC1 drive findings root-caused and fixed* |
| **R4a v3** | `right_assist_nm = 2400` **in config** | **2400.0** | 2026-08-26 `8e8c6f67d` | *R4a self-right v3: the PUMP — push with the motion, or it cannot accumulate* |
| **R4a v4** | `right_dir_eps = 0.04` **in config** | **0.04** | 2026-08-26 `c31e07395` | *R4a self-right v4: standing DOES the shift, and the leg extends to push it* |

**All three GI3 dials, all four GI4 dials and both K-WS1 dials are on main.** The two that
matter for §2 are on main **and OFF**: `traction_mu` and `plane_lat_load_frac`.

⚠ Note the commit subject for `traction_mu`: *"GI4 item 2 measured and **declined**"* — the
decline is recorded in `docs/snowform_measurements.md` §M8.3 and it is a *good* decline (it
does not fix the Road bank-strike Chad reported). But it leaves §9.6's loop-feeder unscaled,
and §10.5's open debt against "items 2+3" unpaid on both legs.

---

## 8. UNVERIFIED — what this strand could not close

1. **Backflip vs rollover.** `rollovers_past90` cannot tell a deliberate send from a crash.
   Every "roll resistant" number in §6.5 is contaminated by Chad's own backflips, which §0's
   first sentence says he does on purpose. Closing this needs a joint instrument
   (`air_s > 0` at entry ∧ pitch-dominant rotation) in `tools/sled_tape_audit.py` — **D-B's
   file, not mine.**
2. **The self-right's airborne authority (§6.2c) is DERIVED, not observed.**
   `right_assist_nm_now` is deliberately **not** in the tape's positional pin roster
   (`sled.h:1250-1258`), so no tape in the corpus can be asked whether it ever fired in the air.
   Closing it needs a probe run, which this strand did not build.
3. **The `rolled`/throttle chatter at 75.06° (§3.1a) is DERIVED.** No edge-count instrument
   exists.
4. **The side-hill self-right (§6.2b) has no tape event.** No field joins slope, stand and
   low speed.
5. **`plane_lift_split_frac`'s restoring couple (§2.3) is verified structurally, with the
   attractor magnitude quoted from `sim/sled.h:1124-1127`'s own measurement.** I did not
   re-measure it — no probe was built.
6. **Whether any of §6's conflicts are FELT is not in this document and must not be inferred
   from it.** Every candidate cure names a dial and an identity value and stops there, per the
   charter's own RC-5: *"THE FEEL IS SIGNED."*

---

*Written by strand D-A, 2026-09-18, against `ed0a43ce8` (== origin/main's kernel text).
No dial changed. No test run. No probe built. Nothing pushed.*
