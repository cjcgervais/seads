# RED-TEAM — LENS: MECHANISM

Target: `docs/sled_audit/ladder_synthesis.md` (2026-09-18, 862 lines).
Worktree `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
**READ-ONLY.** Nothing built, no dial changed, no config edited, no golden moved, `seads.exe`
never run, no ctest run, nothing pushed. Tapes 86–91 were opened **read-only** out of
`D:/flight_sim2/seads-recon/build-play/`. The only files this pass wrote are this one and a
throwaway measurement script in the session scratchpad.

**Confidence vocabulary: MEASURED / DERIVED / LITERATURE / GUESS.** Every numeric claim below
carries its own killing mutation.

**VERDICT: LAND-WITH-FIX.** Six of eight rungs survive mechanism attack with fixes. **Rung 4 and
Rung 8 are DO-NOT-LAND as written** — each names a dial that provably cannot move the quantity its
own invariant measures, and each is fixable without abandoning the rung. Nothing is built, so no
harm has been done; the cost of the two P0s is that both rungs would have been *driven*, *ruled on*
and *scored* against a change that could not have produced the effect they asked Chad to feel.

---

## 0. METHOD, AND WHAT I RAN

Three things were done, in this order:

1. **Re-derivation from source** — `sim/sled.h`, `sim/sled.cpp`, `world/snowpack.cpp`,
   `config/load_scenario.cpp`, `config/load_world.cpp`, `config/scenario.toml`,
   `config/world.toml`, `app/main.cpp`, `test/harness/sled_tape.h`. Every file:line in this
   document was opened this session; none is quoted from another strand's file.
2. **Independent arithmetic** — §1(a) and §1(c) of the ladder were recomputed from the shipped
   geometry without using the ladder's working.
3. **A new read-only joint measurement over the six v17 tapes** (86–91; 86 578 T-records,
   721.3 s = 12.02 min). The ladder's per-tape summary (`tape_summary.json`) carries **marginals
   only**, and three of my findings need **joint** conditions the marginals cannot express. Field
   offsets taken from `test/harness/sled_tape.h:98-107` (`SLEDTAPE_PIN_D`) and the `T_PIN = 11`
   convention; `v_side`, `wv`, `wo`, `tilt` and `pitch` were reconstructed with the kernel's own
   algebra (`sim/sled.cpp:1547-1560`, `:1680-1682`).
   **Epistemics that travel with every tape number below:** the tape pins at 120 Hz while the
   kernel runs the law at 1440 Hz (12 substeps, `sim/sled.cpp:219`), so these are **subsamples**.
   For the sustained states measured here (a machine lying still on its side, a held STAND at a
   crawl) subsampling under-detects transitions, not states — a positive count is real, a null
   would not have cleared anything.

**Corpus reconciliation (MEASURED, my own pass).** 52 past-90 events over 721.3 s = **4.33/min**,
matching D-B's 4.32; `assist_active_frac` 0.798 / 0.860 / 0.866 / 0.876 / 0.884 / 0.937 and
`stand_frac` 0.068 … 0.390 reproduce exactly. The ladder's corpus handling is sound; my
disagreements are about **mechanism**, not about D-B's counting.

---

## 1. WHAT SURVIVED THE ATTACK (stated first, so the P0s are not read as a verdict on the whole)

Each re-derived independently this session.

- **§1(a) geometry. EXACT.** Tipping line from downhill ski (±0.4635, +0.86) to downhill rail
  (±0.19, −0.52); perpendicular distance from the CG = **0.287465 m** (I get 0.404420 / 1.40684);
  SSF = 0.287465 / 0.564 = **0.509689**; static tip = **27.005°**. The rail-half mutation is right
  too: at 0.14 m, arm 0.254987, SSF **0.452105**, tip **24.32°**. `sim/sled.h:543-569`.
- **§1(c) roll-stiffness split. EXACT.** 2·46000·0.4635² = **19 764.6**, 2·46000·0.19² =
  **3 321.2**, φ = **0.85612**; inside-ski lift at 0.18841·0.927/(0.85612·0.564) = **0.36173 g**;
  at `track_rail_half_m = 0`, **0.30967 g**. `sim/sled.h:885, :546, :569, :550, :551`.
- **§1(b) the ski table.** Byte-verified against `sim/sled.cpp:162-193`; `track_lat_mu 0.70` at
  `sim/sled.h:1193`. Six of seven rows above 0.509689 — **confirmed** (LakeIce 0.22 the only one
  below). See P2-13 for the qualification the ladder omits.
- **§1(e) no friction circle.** Confirmed: longitudinal is Mohr-Coulomb (`sim/sled.cpp:1167-1174`),
  lateral is Coulomb-on-a-constant (`sim/sled.cpp:1056-1059`), and nothing couples them.
- **§1(f) the denominator.** `grep -n "p\.mass_kg" sim/sled.cpp` → **one line, 224**. Confirmed.
  (One qualification: P2-18.)
- **Rung 1 mechanism and identity.** `world/snowpack.cpp:722` is `if (p.class_blend_m > 0.0 && ...)`
  and `sim/sled.cpp:630-636` is a ternary on `gs.surf_mix > 0.0` — a branch both ends, identity 0.0
  bit-identical. And the "live-tunable today, no recompile" claim **holds**:
  `config/load_world.cpp:362` is `s.class_blend_m = require(root, "snowpack", "class_blend_m")`,
  `config/world.toml:645` = 0.0.
- **Rung 3 mechanism.** Both halves verified: `app/main.cpp:8263-8266` (C zeroes `sled_lean_lat`,
  `sled_lean_fwd` **and** `sled_steer_cmd`), `app/main.cpp:8356-8357` (R zeroes the two leans and
  **not** the bars), `sim/sled.cpp:292-293` (`(s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle)`).
  The "released the instant R clears it" half is right: `s.rolled = attitude_now && ...`, so a false
  `attitude_now` drops the flag on the same substep.
- **Rung 4's `wo` reading.** MEASURED and **stronger than the ladder claims**: of the 1570 pooled
  v17 ticks in the stall window (tilt 100–130°, |ω_z| < 0.5 rad/s), `wo > 0.8` on **1570 of 1570 —
  100 %**. The ladder is exactly right that `wo` hands that population full authority. It is the
  *other* gate that kills the rung (P0-1).
- **Rung 5 mechanism and identity.** `sim/sled.cpp:1219` is `if (p.traction_mu > 0.0)` — a branch;
  `sim/sled.h:1006` ships 0.0. Both engine ceilings above it confirmed (`max_thrust_n`,
  `engine_power_w/|v|`, `sim/sled.cpp:1203-1210`).
- **Rung 6's duty is not an artefact.** The obvious way that number could have been fake — a stale
  `assist_nm` never cleared when the block is skipped — **is closed**: `sim/sled.cpp:1819` is
  `s.assist_nm = 0.0;` *before* the `if` at 1820, every substep. The 79.8–93.7 % duty is real.
  p50/p95 also confirmed on tape 91 (`assist_nm_abs` p50 **75.0**, p95 **765.0**).
- **Rung 7's dead-cone.** `sim/sled.h:274` defaults `right_dir_eps` to **0.1736** (= sin 10°), and
  `config/scenario.toml:2102` **overrides it to 0.04** → asin 0.04 = **2.293°**. The ladder's claim
  survives, and it is a real config-vs-default drift. Also confirmed: the self-right is **live** —
  `config/scenario.toml:2085` `right_assist_nm = 2400.0`, so the block at `sim/sled.cpp:1679` is
  not dead code, and the ≈1244 °/s airborne figure has a real gain behind it.
- **§6 item 6.** `right_assist_nm_now` (`sim/sled.h:1260`), `right_shift_cmd` and `right_charge` are
  **not** in `SLEDTAPE_PIN_D` (`test/harness/sled_tape.h:98-107`). "No tape in the corpus can be
  asked" is correct **for that field**. (But see P1-9: the *gate's inputs* are all pinned.)

---

## 2. P0 — DO NOT LAND AS WRITTEN

### P0-1 — RUNG 4: THE ONE DIAL IS MULTIPLIED BY ZERO ON THE POPULATION THE RUNG EXISTS FOR
**ID: `RUNG-4-DIAL-CANNOT-REACH-THE-STALL`**

**The rung's own argument, quoted from `ladder_synthesis.md` Rung 4:** *"A machine stalled at
105–125° with near-zero roll rate is exactly the 71 %, and it is exactly the population `wo` hands
full authority to."* Its ONE DIAL is `[sled_comfort] side_right_gain_nm`, identity 700.0.

**The mechanism it did not read.** `sim/sled.cpp:1555-1559`:

```
const double wv = clamp01(
    (v_side - p.comfort.side_right_vmin_ms) /
    std::max(p.comfort.side_right_vref_ms - p.comfort.side_right_vmin_ms, kEps));
```

and the torque at `sim/sled.cpp:1606-1608` is
`dbg_tq_c2 = -sgn_c2 * side_right_gain_nm * wv * wt * w_side * w_load * wo`.

`side_right_vmin_ms = 1.5`, `side_right_vref_ms = 6.0` — **and they agree in both places**
(`sim/sled.h:518-519` and `config/scenario.toml:2141-2142`, so there is no config drift to hide
behind). `wv` is a **multiplicative** gate that is **exactly 0.0 below 1.5 m/s of lateral speed**.
A machine that has *stalled* has, by the meaning of the word, no lateral speed. `wo` full × `wv`
zero = **zero**, and `700 × 0 = 2400 × 0 = 0`.

The ladder even quotes the fact and does not apply it — its own law check cites the block's header:
*"may only AMPLIFY side-slide momentum that already exists"* (`sim/sled.cpp:1536`). That sentence
**is** this finding.

**MEASURED, my own pass over the six v17 tapes** (`v_side` reconstructed with the kernel's own
algebra at `sim/sled.cpp:1547-1554`):

| population | ticks | `wv == 0` | share |
|---|---|---|---|
| all past-90 (tilt > 90°) | 4 973 | 2 277 | **45.8 %** |
| stall window (tilt 100–130°, \|ω_z\| < 0.5) | 1 570 | 1 178 | **75.0 %** |
| stall window, tape 91 alone (**ends upside down at 131.4°**) | 886 | 795 | **89.7 %** |

`v_side` over all past-90 ticks: mean **4.541**, p50 **1.881**, p90 **12.080**, p95 **18.648** m/s —
i.e. the past-90 population is bimodal (a fast slide, then a dead stop), and the dead-stop half is
the half that never recovers.

**So: raising `side_right_gain_nm` cannot move three quarters of the population Rung 4 argues from,
and cannot move nine tenths of it on the tape whose last frame is the complaint.** The rung's
invariant (*"`never_recovered` must fall from 71.2 %"*) is not reachable through this dial except
via machines that are **still sliding** — a different population from the one its mechanism
paragraph describes.

**Killing mutation for THIS finding** (what would make me wrong): `side_right_vmin_ms = 0.0` — then
`wv` is open at rest and the gain reaches the stall. It ships 1.5 in header **and** TOML. Second:
if my `v_side` reconstruction is wrong — it is the kernel's own three lines, off pinned
`position`/`velocity`/`orientation`, and it reproduces D-B's rollover count (52) and duration
(721.3 s) exactly. Third: if the 120 Hz subsample systematically misses sub-8 ms lateral-speed
spikes above 1.5 m/s — possible in principle, but a machine at 0.4 m/s for seconds (tape 91's last
tick) is not hiding a 1.5 m/s side-slide between frames.

**FIX (keeps the rung, changes the dial).** Make Rung 4's ONE DIAL **`[sled_comfort]
side_right_vmin_ms`** (identity **1.5**, already TOML-reachable, and a pure ramp bound so any value
is as bit-identical as 700.0 is). Lowering it toward ~0.3 m/s opens C2 on precisely the stalled
machines, and the rung keeps its two-sided invariant unchanged. Report `side_right_gain_nm` as the
**second** dial, for the *sliding* half, with its own sweep. Both invariants then have a population:
the gain move is graded on the 2 696 sliding past-90 ticks, the vmin move on the 2 277 stalled ones.
**The non-oscillation fence in the rung stays — it is now MORE load-bearing**, because opening `wv`
at rest is exactly the "machine flopping like a fish" risk the drive-checklist line already asks
Chad to watch for.

---

### P0-2 — RUNG 8: THE SPECIFIED TORQUE IS A YAW DAMPER, NOT A PITCH DAMPER
**ID: `RUNG-8-WRONG-AXIS`**

**The rung specifies**, verbatim: *"`τ_y = −gain · ω_y · w_contact · w_band`"*, and in its law
check *"a damping term on ω_y, not a righting torque"*.

**The kernel's body axes are labelled in the source**, `sim/sled.h:772`:

```
// Principal inertia, body axes (X pitch, Y yaw, Z roll) [kg m^2].
glm::dvec3 inertia{158.7, 186.9, 49.7};
```

Corroborated three independent ways: the comfort assist damps roll with
`p.comfort.roll_damp_nms * s.angular_vel.z` into `torque_body.z` (`sim/sled.cpp:1878-1881`); the
C2 righting bias gates on `|s.angular_vel.z|` for roll rate (`:1602-1605`); and the gyroscopic
reaction writes the **pitch** rate as `s.angular_vel.x -= (p.k_gyro_react * dL) / I.x;`
(`sim/sled.cpp:2070`).

**Therefore pitch is X and yaw is Y.** Built literally as written, Rung 8 adds a **yaw damper**:

1. It damps the drift. `roll_damp_nms` was **refused by this very ladder** because Chad ruled it and
   because *"damping is the dial that kills the flick, the drift's body language"* — a yaw damper is
   the same class of harm aimed straight at *"SLiding banging, punchy"* (§0b) and
   *"drift around corners and then with throttle, straighten out … Don't lose the feel"* (§0).
   This is the one rung on the ladder that adds a new kernel term, and as specified it is a new
   term of the exact type the ladder spends a paragraph refusing.
2. Its invariant becomes **decoration**. *"The catwalk pitch p99 falls from 66.75° toward the p95 of
   14.25° while p50, p90 and p95 do not move"* — a torque on ω_y cannot move a pitch statistic
   except through third-order coupling, so the invariant would read "no movement" and be scored as
   "the gate is conservative" rather than "the term is on the wrong axis."
3. Its safety argument evaporates. The 60° `w_band` is argued from **pitch** bimodality; gating a
   yaw torque on a pitch band is arbitrary, and the wheelie-preservation claim (*"Wheelie kept — by
   the 60° gate, and the invariant above is what proves it"*) proves nothing about a yaw term.

**Killing mutation for THIS finding:** if `inertia` were ordered (X roll, Y pitch, Z yaw) the rung
would be right. It is not — `sim/sled.h:772` says so in a comment, and `sled.cpp:1878` and `:2070`
independently confirm z = roll and x = pitch. Second: if "τ_y" were meant as aerospace-convention
pitch (nose-up about the lateral axis, conventionally *y* in an x-forward frame) rather than as this
kernel's `torque_body.y` — that is almost certainly what was meant, which is exactly why it must be
fixed on the page: **the ladder is the document a builder implements from**, and it names `ω_y`
twice, in the spec line and in the law check.

**FIX.** Rewrite the shape as **`torque_body.x += −pitch_arrest_nms · s.angular_vel.x · w_contact ·
w_band`**, and state in the rung that in this kernel **X is pitch, Y is yaw, Z is roll
(`sim/sled.h:772`)**. Add to the invariant: **`|ω_y|`'s distribution must be bit-identical** — that
is the test that the term did not land on yaw, and it is the fence that protects the drift Chad
signed. And make it a branch (`if (p.comfort.pitch_arrest_nms > 0.0)`) rather than an add-of-zero,
per the ladder's own §2 identity law.

**Second, independent defect in the same rung's mechanism paragraph.** *"`grep -nE
"torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns exactly five sites … **Every comfort term
in the kernel is a roll or yaw term; pitch has none.**"* The grep is right (602, 1608, 1655, 1792,
1881) but the conclusion is not:
 - `:602` (`torque_body += tq`) and `:1655` (`torque_body += t_c3`) are **whole-vector** adds, and
   both have pitch components. `:1655` is C3's world-up torque rotated into body axes; the source
   itself records the spill on the *other* off-axis (`dbg_c3_roll = t_c3.z; // the yaw term's roll
   spill`, `sim/sled.cpp:1654`). So pitch is not untouched — it is untargeted.
 - A `torque_body` grep **cannot** find pitch authority that bypasses the torque accumulator, and
   there is some: `sim/sled.cpp:2070` writes `s.angular_vel.x` directly (`k_gyro_react`, shipping
   0.0, `sim/sled.h:768`). The ladder names that dial in ruling R6 as *Reflex Gyro* without noticing
   it is a **pitch-rate** channel that already exists, written and commented, on the one axis Rung 8
   says has nothing. It is not a damper and is not a drop-in substitute — but "pitch has none" is
   the sentence that justifies adding the ladder's only new kernel term, and it was established by a
   grep that structurally could not see the counter-example.

---

## 3. P1 — MUST BE FIXED ON THE PAGE BEFORE CHAD IS ASKED TO RULE

### P1-3 — RUNG 8: "there is no populated band" is REFUTED on the rung's own condition
**ID: `RUNG-8-EMPTY-BAND-REFUTED`**

The rung: *"There is no populated band between 'nose down on the snow' and 'past 60° and going
over' — a spike at zero and a tail straight to the flip. … a gate that opens above 60° **cannot
touch the 30° wheelie he asked for**, because nothing lives between."*

**MEASURED, my own pass, under the rung's exact condition** (`stand > 0.5 ∧ lean_fwd < −0.30 ∧
throttle > 0.90 ∧ air_s == 0`), pooled over tapes 86–91, **10 316 catwalk ticks**:

| pitch band | ticks | share of catwalk |
|---|---|---|
| ≥ +60° | 209 | 2.03 % |
| **+20° … +60°** | **156** | **1.51 %** |
| −20° … +20° | 6 924 | 67.1 % |
| **−60° … −20°** | **3 010** | **29.2 %** |
| ≤ −60° | 17 | 0.16 % |

The "empty" band holds **156 ticks — 75 % as many as the flip band it is supposed to be empty
relative to**. It is *thin*; it is not *nothing*, and "nothing lives between" is the load-bearing
sentence.

**The safety claim survives anyway, for a different reason, and the rung should say so:** a
smoothstep that **opens at 60°** is identically zero below 60° by construction, so the 30° wheelie
is untouched whether the band is populated or not. The emptiness argument is therefore
**decorative** — it is not what protects the wheelie. Delete it and keep the construction argument.

**Killing mutation:** widen the catwalk predicate (drop `throttle > 0.90`, or `air_s == 0`) and
these counts move; the ladder's own third killer already names the `air_s` clause. Under the exact
shipped predicate, they are what they are. Per-tape (20–60° / ≥60°): 86 → 23 / 132, 87 → 0 / 0,
88 → 9 / 10, 89 → 6 / 0, 90 → 11 / 15, 91 → **107 / 52**. On tape 91 the "empty" band is **twice**
the flip band.

### P1-4 — RUNG 8: the band's SIGN is unspecified, and the nose-DOWN half is 19× larger
**ID: `RUNG-8-BAND-SIGN-UNSPECIFIED`**

`w_band` is described only as *"a smoothstep opening from 60° of pitch"*. Signed or absolute is
never stated, and the difference is enormous: **3 010 catwalk ticks sit between −20° and −60°
against 156 between +20° and +60°** (MEASURED, above). Tape 91's own catwalk `pitch_deg` runs
**min −76.09° to max +79.18°** (`tape_summary.json`, D-B's own field). On `|pitch|` the term fires
on ~29 % of catwalk time and damps *nose-down* rotation — a machine trying to pick its nose up out
of a dive, held down by a comfort term. On signed pitch it fires on 2 %. **Specify it as signed
(nose-up only), and put the measured nose-down population in the rung so Chad knows the pose he
calls a catwalk is, in this corpus, mostly a nose-down pose.**

### P1-5 — RUNG 2: the dial is gated on the dial the rung calls "the wrong one"
**ID: `RUNG-2-ALIGN-M-COUPLING`**

`sim/sled.cpp:585-592`:

```
double align_m = 0.0;
if (p.comfort.lean_bite_gain > 0.0) {
    ...
    align_m = clamp01(lean_frac * (wy >= 0.0 ? 1.0 : -1.0)) * mag * mag * (3.0 - 2.0 * mag);
}
```

Rung 2's dial is applied as `lat *= 1.0 + p.plane_lat_lean_gain * align_m` (`sim/sled.cpp:1102`).
**`align_m` does not exist unless `lean_bite_gain > 0.0`.** The two lean-reward paths the rung
presents as *alternatives* — *"Two lean-reward paths exist and the shipped one is the wrong one"* —
are in fact **in series**: the "wrong" one is the enable gate for the "right" one. Anyone who acts
on the rung's own argument by taking `lean_bite_gain` (0.18) down to 0 silently zeroes Rung 2's dial
as well. Say it in the rung, and add it to the invariant: **`lean_bite_gain` must stay > 0 for this
dial to exist at all.**

### P1-6 — RUNG 2: the killing mutation cannot fail
**ID: `RUNG-2-KILLER-DECORATIVE`**

The rung: *"Sweep the gain and watch the **non-leaned** radius: if the flat 33 m ladder moves at all,
the term is leaking off `align_m`."* With no lean, `lean_frac = s.rider_lat_m / lat_reach_m = 0`
(`sim/sled.cpp:576-577`), so `align_m = clamp01(0 · ±1) · … = 0` **exactly**, and the multiplier is
`1.0 + gain·0 = 1.0` **for every gain**. The non-leaned radius is bit-identical by construction.
The test cannot fail, so it discriminates nothing.

**FIX — three tests that CAN fail, all cheap:**
(a) **wrong-way lean must be bit-identical** — `clamp01` of a sign-mismatched product is 0, which is
the house rule (`sim/sled.cpp:1100`, and the existing pin
`sled_wrong_way_lean_is_never_a_penalty`); sweep the gain and confirm the wrong-way radius does not
move by a bit.
(b) **the track's contribution must not move** — the whole argument for this dial over
`lean_bite_gain` is *steered-patch only* (`if (g.steered)`, `sim/sled.cpp:1101`); sweep the gain and
confirm `track_slip` and the track's lateral force are unchanged. That is the test that this term is
the aimed reward and not the un-aimed one.
(c) **the yaw-rate threshold** — `align_m` ramps over |ω about local up| 0.02→0.07 rad/s
(`sim/sled.cpp:588-591`), so a straight-line held lean gets nothing; confirm the straight-running
case is bit-identical, which is a real risk if anyone widens `mag`.

### P1-7 — RUNGS 2, 3 AND 5: "one loader line" is false — there is **no** TOML→`SledParams` path, and **no `[sled_input]` table exists**
**ID: `RUNGS-2-3-5-PLUMBING-UNDERSCOPED`**

- `grep -n "sled_params\|SledParams" config/load_scenario.cpp` → **nothing**.
- `config/load_scenario.h:46` carries exactly one sled struct: `sim::SledComfort sled_comfort{};`.
- `app/main.cpp:2481` is the **only** bridge: `sled_params.comfort = scen.sled_comfort;`.
- `grep -n "sled_input" config/scenario.toml config/load_scenario.cpp app/main.cpp` → **nothing**.

Consequences:
- **Rung 2** (*"the rung's first act is **one loader line** promoting it into `[sled_comfort]`"*):
  `plane_lat_lean_gain` is a top-level `SledParams` field (`sim/sled.h:1061`), not a `SledComfort`
  one. Making it a `[sled_comfort]` key needs a **new field in `struct SledComfort` in `sim/sled.h`**
  plus a change at `sim/sled.cpp:1102` to read `p.comfort.…` — i.e. **`sim/` edits**, on a rung
  billed as config-cheap.
- **Rung 5** (*"like Rung 2 it needs one RC-8 loader line"*): identical, for `traction_mu`
  (`sim/sled.h:1006`).
- **Rung 3** (*"`[sled_input] autoright_recentre`"*, and the dropped `[sled_input]
  steer_centre_per_s`): names a TOML table that **does not exist anywhere in the tree**. It needs a
  new table, a new loader block, a new struct, an `app/main.cpp` plumbing line and a
  `test_load_scenario.cpp` CHECK.

None of this changes the *numerics* — done correctly all three stay bit-identical — but it changes
what Chad is being told a rung costs, and it moves Rungs 2 and 5 from "config" into `sim/`, which is
the boundary the audit's own hard rules police. **FIX: state the real file list per rung**
(`sim/sled.h` struct field + `sim/sled.cpp` read site + `config/load_scenario.cpp` require + check +
`config/scenario.toml` key + `test/unit/test_load_scenario.cpp` CHECK), and for Rung 3 either create
`[sled_input]` deliberately or put the key in `[sled_comfort]` where the plumbing already exists.

### P1-8 — RUNG 7: the tape evidence attributes the wrong gate; the measured armed duty is **2.32 %**, not 6.8–39.0 %
**ID: `RUNG-7-ARMED-DUTY-OVERSTATED`**

The rung: *"**STAND is held hard (`in.stand > 0.5`) for 6.8–39.0 % of the v17 drives, mean ≈ 24 %**
… So the gate below is armed during ordinary riding, not only during a crash."*

The self-right is **not** gated on STAND alone. `sim/sled.cpp:1690-1699` adds a hysteretic
low-passed **speed** gate at `right_assist_max_ms = 1.3888…` m/s (5 km/h,
`config/scenario.toml:2088`), and `p_eff = armed · push · w_tilt` (`:1704-1705`) needs all three.

**MEASURED, pooled over tapes 86–91 (86 578 ticks):**

| condition | ticks | share |
|---|---|---|
| `stand > 0.5` | 22 142 | 25.6 % |
| `… ∧ ground_speed_ms < 1.3889` | 2 265 | **2.62 %** |
| `… ∧ tilt > 0.25 rad` (the ramp's lower edge) | 2 011 | **2.32 %** |

So the ladder overstates the armed duty by **≈ 11×**. The rung's own drive-checklist line
(*"Sit her across a steep side-hill **at a crawl**"*) is correct; its evidence paragraph is not, and
the catwalk pose it cites in the next sentence is by definition WOT — **never** armed.

### P1-9 — RUNG 7: declared "Probe-only / Unverified on tape" — the tape pins every input the gate reads but one
**ID: `RUNG-7-DECLARED-UNMEASURABLE-BUT-MEASURABLE`**

The rung: *"**Unverified on tape: no field in the corpus joins slope, stand and low speed** (D-A §8
item 4). Probe-only."* and §5 lists no free leg for it.

`SLEDTAPE_PIN_D` (`test/harness/sled_tape.h:98-107`) pins `position`, `orientation`,
`ground_speed_ms`, `hull_engage_lp`, and `TickRec` pins `in.stand`. That is **the gate's own
arithmetic**: `up_cg = normalize(position)`, `body_up = R·(0,1,0)`,
`tilt = acos(dot(body_up, up_cg))` — literally `sim/sled.cpp:1680-1682` — plus the speed gate and
the stand term. The only unpinned input is `hands_on` (`s.grip.attached`), which can only make the
count an **upper bound**. And `hull_engage_lp` is a ready-made discriminator for the rung's whole
question: hull engagement blends in above **25° of SURFACE-relative roll**
(`sim/sled.cpp:1428-1430`), so **tilted off gravity while `hull_engage_lp ≈ 0` is "conformal on a
slope, not on its side."**

**MEASURED (I ran it):** of the 2 011 armed ticks, **1 022 (50.8 %) have `hull_engage_lp < 0.05`**.
Per tape: 86 → 0/77, 87 → 183/237, 88 → 0/124, **89 → 839/839**, 90 → 0/0, 91 → 0/734.
**Tape 89 has ZERO rollovers** (`rollovers_past90 = 0`, `tape_summary.json`) — so all 839 of its
armed, hull-free ticks (≈ 7.0 s at 120 Hz) are the rung's target case with a crash excluded by
construction. Tapes 86/88/90/91 contribute **zero** hull-free armed ticks: all of their arming is
genuine on-its-side.

**This does not kill Rung 7 — it is the first real evidence FOR it**, and it replaces a wrong
paragraph with a right one. But it does refute §5's framing, and it means the ladder shipped a rung
to a ruling queue while declaring unmeasurable a thing that costs one read-only pass.

**FIX: add free read-only leg X3 to §5** — "armed-on-a-hill": count
`stand > 0.5 ∧ ground_speed_ms < 1.3889 ∧ tilt > 0.25 rad`, split by `hull_engage_lp < 0.05`, per
tape. **Epistemics that must travel with it:** `hands_on` is unpinned (upper bound);
`hull_engage_lp` is a low-passed proxy for `|phi_surf| > 25°`, not `phi_surf` itself; 120 Hz vs
1440 Hz, so a null under-detects.

### P1-10 — RUNG 7: `|phi_surf|` is a surface-relative **ROLL**, not a surface-relative **TILT** — the substitution breaks the pitched case and falsifies the rung's own identity claim
**ID: `RUNG-7-PHI-SURF-IS-ROLL-NOT-TILT`**

The proposed dial: `tilt_ref = mix(acos(up_body.y), |phi_surf|, right_tilt_surf_frac)`, with
*"At 1.0 flat ground is unchanged (φ_surf ≡ tilt there)"*.

`sim/sled.cpp:1423-1425`:
```
const double phi_surf = std::atan2(glm::dot(R * glm::dvec3(1, 0, 0), n_surf),
                                   glm::dot(body_up, n_surf));
```
Write `c = body_up·n_surf`, `r = (R·x̂)·n_surf`. Then `|phi_surf| = atan2(|r|, c)` while
`tilt = acos(c)`. Because `body_up`, `R·x̂`, `R·ẑ` are orthonormal, the two are equal **iff
`(R·ẑ)·n_surf = 0` — i.e. iff the pitch is zero.** DERIVED, exact.

Consequences, all on **flat** ground (so "they are flat-ground legs" does not save it):
- At **exactly 90° of pure pitch**: `c = 0`, `r = 0` → `atan2(0, 0) = 0`, while `tilt = 90°`.
- Just **past** 90° of pitch: `c < 0`, `r = 0` → `atan2(0, negative) = π`. So `|phi_surf|` **jumps
  0 → 180°** across the pitch-over. At `right_tilt_surf_frac = 1.0` the self-right's gate becomes a
  **step function in pitch**.
- A machine nose-down at 85° — *precisely the case the kernel's own red-team note was written about*
  (`sim/sled.cpp:1367-1371`: *"misread pitched crashes as rollovers"*) — reads `|phi_surf| ≈ 0`, so
  at 1.0 the STAND self-right **refuses to work** for it. That is a behaviour **regression**, not
  the no-op the rung's killing mutation anticipates.
- Therefore the invariant *"Every leg in `test/unit/test_sled_selfright.cpp` (11 TEST_CASEs) passes
  unmoved at 1.0 — they are flat-ground legs"* rests on a false premise. Flat-ground ≠ pure-roll.
  (Note also: one of the 11 is `TEST_CASE("selfright probe sweep", "[.probe]")` — tagged out of the
  default run, so the invariant really covers 10.)

**FIX, and it is one token.** The quantity the rung wants is the **surface-relative tilt**, not the
surface-relative roll: **`acos(clamp(dot(body_up, n_surf), -1, 1))`**. `n_surf` is declared at
`sim/sled.cpp:1372` at substep scope — the **same** scope as the self-right `if` at `:1679` — so
there is no plumbing to add, and it degenerates to `acos(up_body.y)` exactly when
`n_surf → up_cg`, which is the rung's own stated fallback. It is continuous everywhere, it reads ~0
on a conformal hill at any pitch, and it keeps full authority for a genuine inversion.

**Second, smaller, and worth one line in the rung:** the dial changes only `w_tilt`'s reference.
The **direction** term is still gravity-referenced — `e = -up_body.x - right_seed_frac·braced`,
`dir = tanh(e/eps)` (`sim/sled.cpp:1762-1765`) — so on a *partially* tipped machine on a side-hill
the shove still aims at gravity-up rather than hill-normal-up. Not a defect in the rung; an
unstated limit of it.

**Third, the ladder's own identity law, violated by its own rung.** §2 preamble: *"Every rung is a
branch or a multiply-by-zero, **never a 0-weight lerp** — the `kernel-v14-leanlead` precedent."*
`mix(a, b, 0.0)` **is** a 0-weight lerp: it evaluates `a·1.0 + b·0.0`, which is bit-identical only
while `b` is finite. Write it as a branch: `frac > 0.0 ? mix(...) : acos(up_body.y)`.

### P1-11 — §1(d) AND RUNG 6: three different "gravity roll budget" denominators, one of them underivable
**ID: `SPINE-1D-DENOMINATOR-UNDERIVABLE`**

§1(f) exists to stop exactly this — *"The denominator, once, so no rung repeats the error"* — and
the document then carries three:
- **933.2 N·m** — what §1(a)'s own geometry gives: `W · arm = 3246.0 × 0.287465`. Never stated.
- **1 505 N·m** — Rung 6: *"the machine's own tipping scale (331 × 9.80665 × the 0.4635 m
  half-stance = 1505 N·m)"*, i.e. `W · half-stance`, an arm §1(a) explicitly says is **not** the
  rollover's (`sim/sled.h:546`: *"ski centre-to-centre. **NOT the rollover's**"*).
- **1 931.8 N·m** — §1(d): *"**2.24× gravity's own peak tipping torque (1931.8 N·m)**"*. It implies
  an arm of 1931.8/3246.0 = **0.5951 m**, which is none of the machine's dimensions, and
  `grep -rn "1931" docs/ Game_loop_idea/` finds no source for it. **No derivation is given and I
  could not reconstruct one.**

The consequence is that Rung 6's headline is a choice: p95 765 N·m is **51 %** of 1 505, **40 %** of
1 931.8, or **82 %** of 933.2. **FIX: pick `W · arm = 933.2 N·m` (the only one §1(a) derives), state
it once in §1(f), and restate §1(d) and Rung 6 against it.** Rung 6's own second killing mutation
(*"Any figure quoted to him must name the mass it used"*) should be widened to *"…the mass **and the
arm** it used."*

### P1-12 — §1(d): the "2.24×" rests on an unstated slip-angle assumption, unlabelled
**ID: `SPINE-1D-TANH-ASSUMPTION-UNSTATED`**

*"A **4 m/s** arrival ⇒ 14 400 N damper normal ⇒ **7 677 N** lateral at μ 0.70 ⇒ 4 330 N·m."*
14 400 × 0.70 = **10 080**, not 7 677. The gap is the `tanh` the lateral law carries
(`sim/sled.cpp:1057-1059`): 7 677 / 10 080 = **0.76159 = tanh(1)** — i.e. the arithmetic silently
assumes `slip_ang = slip_ref_rad = 0.300 rad` **exactly** (`sim/sled.h:1187`). The 14 400 N and
`susp_c = 3600` check out (`sim/sled.h:886`), and the `axis_dot` 0.25 floor (`sim/sled.cpp:650-653`)
is real, so the mechanism is right — but the headline multiplier is **linear in
tanh(slip/0.30)**, and the assumed slip is a **GUESS** that is never stated or labelled. At a
touchdown slip of 0.10 rad the same chain gives **0.95×** — i.e. below gravity, and §1(d)'s claim
**inverts**. The rung's killing mutation names only the 0.05 rad extreme, which reads as a remote
corner rather than as the live sensitivity it is. **FIX: state the assumed slip angle, label it
GUESS, and print the multiplier as a function of it (0.05 → 0.37×, 0.10 → 0.95×, 0.20 → 1.75×,
0.30 → 2.24×, 0.60 → 2.85×). It is the same question as probe leg D-1 and it should be attached
there.**

---

## 4. P2 — FIX ON THE PAGE, NOTHING BLOCKS

- **P2-13 `SPINE-1B-OMITS-TANH-AND-LEAN`.** *"an untripped, flat-ground friction rollover is
  **available by construction**"* compares a **table** μ against SSF 0.5097, but the kernel never
  delivers the table μ except at large slip: `tanh(slip/0.30)`. At a carve's slip (≲0.10 rad) Bush's
  effective lateral coefficient is 0.55 × 0.321 = **0.177 g** — a third of the tip threshold. The
  honest statement is *"available in a big slide (slip ≳ 0.3 rad), not in a carve"* — which
  **strengthens** the ladder's refusal of `lat_mu_scale` (that refusal is now about spending the
  drift he signed, not about spending a carve that does not exist) and weakens §1(b)'s framing.
  Conversely, the comparison omits a term pointing the other way: `bite *= 1.0 +
  lean_bite_gain·align_m` (`sim/sled.cpp:1067`, gain 0.18) raises the effective lateral coefficient
  on **every** patch by up to 18 % — Bush 0.55 → **0.649 g**. So **the one shipped lean reward moves
  the machine toward its own tip threshold when you lean into a turn**, which is a §0-relevant fact
  (*"Leaning shall enhance the ride an just make it more stable"*) that no rung on the ladder states.
- **P2-14 `RUNG-6-IDENTITY-AMBIGUOUS`.** *"identity 0.0 = the x-ray panel is pixel-identical. Above
  0 it **grades** the slag-orange ball's glow"* — if the tell **scales** the existing glow, 0.0 puts
  the ball **dark** and the identity claim is false; only the **additive** reading is
  pixel-identical. Specify: `glow = base + kAssistTell · band(|assist_nm|)`, behind
  `if (kAssistTell > 0.0f)`.
- **P2-15 `RUNG-1-SINKABLE-STILL-STEPS`.** *"it removes a discontinuity between two [surfaces] that
  already exist"* is true of the **continuous** dials only. `blend_dials` (`sim/sled.cpp:200-213`)
  says so itself: *"`sinkable` is a bool and cannot be lerped -- it takes the DOMINANT class"*, and
  `surf_mix` ramps only to 0.5 either side of the edge (`world/snowpack.cpp:731, :743`), so
  `sinkable` — and with it the whole pack/Bekker branch — **still steps at exactly the same place**
  at any `class_blend_m`. One line in the rung. It does not change the direction of the fix
  (`rho_eff` 0 → 260 under one ski is the couple, and that one **is** ramped).
- **P2-16 `SELFRIGHT-MIN-TILT-IS-DECORATIVE`.** `right_assist_min_tilt_rad` is a **required** TOML
  key (`config/load_scenario.cpp:514-515`, `config/scenario.toml:2094` = 0.35) that
  `sim/sled.cpp` **never reads** — the TOML comment admits it: *"kept for the loader; the RAMP below
  owns the gate"*. In a ladder whose §2 preamble invokes RC-8 (*"EVERY MECHANISM IS A DIAL"*), a
  dial that is not a mechanism belongs in the record. It also means anyone reasoning about the
  self-right's arming from `scenario.toml` alone reads the wrong threshold (0.35 rad, not the
  ramp's 0.25).
- **P2-17 `RUNG-3-HANDS-ON-UNNAMED`.** Ruling R3 (*"5.74 % of your v17 ride time"*) attributes the
  throttle cut to `rolled` alone; `sim/sled.cpp:292-293` is `(s.rolled || !hands_on)`, and
  `hands_on = s.grip.attached` (`:291`). `grip.attached` is **not** in `SLEDTAPE_PIN_D`, so the
  split between the two reasons is not measurable from the corpus — which should be stated, because
  it is the difference between "the machine cuts your throttle because it thinks you rolled" and
  "the machine cuts your throttle because you were thrown off it."
- **P2-18 `SPINE-1F-RIDER-MASS-OVERSTATED`.** *"`rider_mass_kg` is split OUT of `mass_kg` and
  **never enters a force**"* — it enters at `sim/sled.cpp:459` and `:464` (the grip reduced-mass
  exchange, `87.5·243.5/331 = 64.4 kg`), which the ladder's own `k_air_shift` refusal describes in
  §3. The intended claim — *never enters the machine's weight* — is true and should be the wording.
- **P2-19 `RUNG-8-IDENTITY-SHOULD-BE-A-BRANCH`.** `τ = −0.0 · ω · …` added to `torque_body` is
  numerically identical for finite ω but is an add-of-zero, not a branch, and it evaluates `w_band`
  and `w_contact` for nothing every substep. Per the ladder's own §2 law, gate it:
  `if (p.comfort.pitch_arrest_nms > 0.0)`.

---

## 5. WHAT I DID NOT TEST

1. **X1, the exit-surface attribution** (Rung 1's own decisive killer). Still unrun. I did not run
   it; it remains the single cheapest thing on the board and my pass does not substitute for it.
2. **The 12/12 sign test and the per-surface rollover rates.** I reconciled the totals (52 events,
   721.3 s, 4.33/min) and tape 91's per-surface split (TrailMain 7 / 15.594 per min, Bush 11 /
   6.248 per min — both reproduce D-B exactly) but did not re-run the sign test.
3. **Anything in a probe.** No probe was built, `seads_sled_probe` was not compiled, no ctest was
   run, `seads.exe` was never launched. Every "would fail / would not fail" above is DERIVED from
   source or MEASURED off a tape Chad recorded.
4. **`phi_surf` in the corpus.** It is not pinned; `hull_engage_lp` is the only proxy and I used it
   as one, with the proxy stated every time.
5. **The felt half.** Ruling R1 stands untouched: **there is still no word from Chad on tapes
   86–91**, which are the six drives every number in this red-team is measured on. None of my
   findings license a dial move; they license a correction to the page.

---

*Red-team pass, lens MECHANISM, 2026-09-18. Read-only against main. No dial changed, no config
edited, no golden moved, nothing built, nothing run, nothing pushed.*
