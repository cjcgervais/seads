# R4 — LANDINGS: what happens between the lip and the next tick of grip

Strand: **R4-landings**. Audit worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. **READ-ONLY against the kernel** — nothing in `sim/`, `control/`,
`config/`, `test/golden/` was opened for write. No dial changed, no probe built, no test
run, no exe launched. Tapes were read from `D:/flight_sim2/seads-recon/build-play` only
through the JSON summary strand D-B already produced
(`docs/sled_audit/tape_summary.json`, 91 tapes, 84 with air events).

Confidence vocabulary: **MEASURED** (somebody instrumented it — a tape, a trade spec, a
shipped source line), **LITERATURE** (published/trade/developer statement, not re-derived
here), **DERIVED** (arithmetic done in this document from stated inputs), **GUESS**
(labelled, always).

Every numeric claim carries its **killing mutation**. A claim without one is decoration.

Companion strands: R1 (`R1-real-dynamics.md`) owns the real machine and the rollover
geometry; R2 (`R2-game-feel.md`) owns the genre and the assist taxonomy. This strand owns
the **air-to-ground transition** and deliberately does not re-derive their claims — it
cites them (R1-4, R1-24, R2-04, R2-09) and extends them.

---

## 0. The words this strand answers to

Chad, **verbatim**, `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0**
(drive 4, 2026-08-12):

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more
> often after a roll** (even though R works). But not every time — allow it to happen."

And **§0b** (2026-08-12 night, SUPERSEDING), verbatim:

> "Leaning shall enhance the ride an just make it more stable and slef righting by chance
> more. It should be arcadey to a degree so that it is fun., SLiding banging, punchy,
> **jumps**. Just make it more stable."

And the felt report that named the air, verbatim, `docs/gi4_ride_handoff.md` §1 item 2:

> "**should be able to launch in the air**"

— against which that document's own measurement read: "40 air events; the long ones are
**crashes, not jumps** — launch pitch 36–80°, landing roll ±100–180°. **Only 2 of 40 land
upright**", and whose §6 closed the item unfixed: "**Item 2, launching.** Untouched
pending the Road pathology — most of his 'air' was crash ballistics, not jumps."

And the world law that manufactures the air in the first place,
`Game_loop_idea/WINTER_LAW.md` §2.4c:

> "Roads are plowed. The snow the plow moves piles at the edges as banks. The banks are
> BROKEN, not continuous. **Hitting one at speed sends the snowmachine airborne.**"

**The whole of this strand's value is one sentence:** the launch was never the problem.
Between leaving the ground and the first tick of grip, this kernel gives the player
**exactly zero attitude authority** and then absorbs the arrival with a **linear damper**
that has no blow-off. Both halves are already written in the source — the air terms exist
and ship at zero; the damper is three lines — and the tapes show what that costs: **two
landings in three end past 45° of tilt.**

---

## 1. What his landings actually are — MEASURED off his own tapes

Method: strand D-B's `tools/sled_tape_audit.py` walks the tape's T records, opens an air
event when the kernel's own `air_s` leaves zero, closes it on the return to zero, keeps
the event if the run reached 0.15 s, scores `|g_eff|` (the kernel's own
`|(-g·up) − a_world|`, `sim/sled.cpp:2038`) through 0.4 s past touchdown, and scores the
landing **upright** if body tilt is under 45° **0.5 s after** touchdown. I re-aggregated
its per-tape output across all 84 tapes that contain air; I did not re-parse the tapes.

### R4-01 Air is a tenth of the game and two landings in three end on their side
**984 landings** over **206.1 minutes** of his driving = **4.8 landings per minute**.
Of those, **306 are upright at +0.5 s** = **31.1 %**. The other **678** are still past
45° half a second after the skis touch.
Confidence: **MEASURED** (91-tape corpus; `landings_scored` 984, `landings_upright` 306).
**Read against the record:** `gi4_ride_handoff.md` measured **2 of 40 = 5 %** upright on
the pre-GI3 kernel. The shipped kernel is **6.2× better** and still fails two landings in
three. Chad's "allow me to land on my skis **more often**" (§0) has been answered in
direction and not in degree.
**Killing mutation:** if `air_s` is a latch that does not clear promptly on touchdown
(R2's UNVERIFIED-2), event boundaries shift and the +0.5 s scoring window lands somewhere
else — the *count* of landings survives (it is edge-triggered), the *upright fraction*
does not. Reading the `air_s` writer settles it: `sim/sled.cpp:2131-2135` clears `air_s`
to 0 the moment `normal_sum + side_normal_sum > 1.0` **or** the CG drops within
`1.2 × cg_height_m` of the floor, and otherwise accumulates `h`. It is a true per-substep
airborne timer, not a cumulative counter — so this claim and R2-04's "% airborne" both
stand, and R2's UNVERIFIED-2 is **closed** by this reading.

### R4-02 Hang time: half his airs are hops, a quarter are real sends
Over the 984 events: **p50 = 0.473 s**, **p90 = 1.735 s**, **p99 = 2.884 s**,
**max = 3.944 s** (tape 29). **24.2 %** of airs reach 1 s; **78** reach 2 s.
A symmetric 1.74 s hang is a **3.7 m** apex and a **8.5 m/s** arrival; the 3.94 s event
is, ballistically, **19 m** and **19.3 m/s** — that one is a fall off something, not a
jump.
Confidence: **MEASURED** (hang), **DERIVED** (apex/arrival, via `h = g(t/2)²/2`, which
assumes the landing is at launch height).
**Killing mutation:** the symmetric assumption. Every one of his sends that lands *below*
the lip (a bank onto a road, a hill onto a flat) has a longer hang than its apex implies,
so the derived arrival speeds are **lower bounds**, not estimates.

### R4-03 Landing severity rises monotonically with hang — the physics is intact
Peak `|g_eff|` within 0.4 s of touchdown, bucketed by hang:

| hang | n | peak g p50 | p90 | max |
|---|---|---|---|---|
| 0.15–0.5 s | 513 | 6.6 g | 15.5 g | 66.9 g |
| 0.5–1.0 s | 233 | **13.2 g** | 26.5 g | 96.3 g |
| 1.0–1.5 s | 120 | 18.7 g | 28.4 g | 50.9 g |
| 1.5–2.0 s | 40 | 24.4 g | 34.4 g | 41.5 g |
| 2.0 s+ | 78 | **34.0 g** | 68.2 g | **261.6 g** |

Confidence: **MEASURED**.
**Why it matters:** nothing here is broken *in direction* — longer air, harder arrival, as
it must be. What is wrong is the *slope*: the median 2 s send arrives at **34 g**, and the
worst single event in the corpus (tape 16, 2.48 s hang) peaks at **262 g**. §2 shows that
slope is a modelling choice, not a consequence of the jump.
**Killing mutation:** `g_eff` is differenced between consecutive 120 Hz T records, so a
one-tick spike is one sample of a transient. The literature on exactly this measurement
problem: "A low sampling rate decreases the probability that the peak magnitude of a
transient signal spike **like jump landing** will occur at the same instant the signal is
sampled" (Sports-biomechanics accelerometer methods, ScienceDirect S0021929017302233).
That cuts *against* the numbers being overstated — under-sampling **misses** peaks, it
does not invent them — but if the tape's velocity field is post-solver rather than
post-integration, a single substep's clamp could appear as a whole-tick acceleration and
the p99/max column is then an artefact. The p50 column, which §2 predicts independently
from the source, is not.

### R4-04 He does not bail out of bad landings — he rides them
**146** `autoright_R` presses in 206.1 min = **0.71/min**, against 4.8 landings/min.
Every single press was at a tilt of **≥ 69.3°** (p10 99.6°, p50 **112.0°**, max 174.1°);
**zero** presses below 45°.
Confidence: **MEASURED** (override records, 146 of 146 carry `tilt_before_deg`).
**Why it matters:** R is reserved for a machine that is genuinely over. The 678 non-upright
landings are not 678 R presses — they are 678 moments he rides out, slides through, or
loses the line on. "even though R works" (§0) is exactly that: R is not the complaint, the
*distribution* is.
**Killing mutation:** an override class other than `autoright_R` that also rights the
machine (only `episode_start` and `autoright_R` appear in the corpus, so this is closed
unless a newer kernel adds one).

---

## 2. The kernel's landing budget — DERIVED from the shipped source

The suspension is per-patch, three patches (`Patch::SkiLeft`, `SkiRight`, `Track`,
`sim/sled.cpp:56-81`), with `susp_k = 46000` N/m, `susp_c = 3600` N·s/m,
`susp_travel_m = 0.26`, `susp_stop_k = 5.0`, `susp_rest_m = 0.21`
(`sim/sled.h`, echoed in every tape's `# param` header). Total weight
(331 + 87.5) × 9.80665 = **4104 N**.

### R4-05 The spring alone bottoms at a 0.89 m drop — which 3 airs in 10 exceed
Static compression `x0 = W/3k = 0.0297 m`. Energy balance to the bump stop, all three
patches sharing equally:
`½mv² + mg(0.26 − x0) = (3/2)k(0.26² − x0²)` → stored **4603 J**, gravity adds **945 J**,
so the kinetic budget is **3658 J** and the machine bottoms at **vz = 4.18 m/s** — the
arrival from a **0.89 m** free drop, i.e. a **0.85 s** symmetric hang. Force at the stop:
`3k × 0.26 = 35 880 N = 8.74 g`. Past it the rate is **6×** (`k + 5k`).
Confidence: **DERIVED** from shipped parameters.
**Read with R4-02:** ~**30 %** of his airs hang 0.85 s or longer. **Bottoming is his normal
landing**, which is exactly what R1-24 concluded for the real 1990s machine
("bottoming is the normal case, not the failure case") — and the kernel's 0.26 m of travel
is *generous* against the era's measured ~0.20 m (R1-4). The kernel is not stingy with
travel. It is stingy with what happens **during** the travel.
**Killing mutation:** the equal-share assumption. The track patch carries most of the
static load and lands first or last depending on attitude, so a nose-high arrival puts the
whole budget through **one** patch — `x0` triples, the energy budget falls by roughly a
third, and the bottoming speed drops toward 3.4 m/s. That makes the claim *conservative*.
It would be falsified the other way only if `susp_k` were a whole-machine rate rather than
per patch — it is not: `normal = p.susp_k * x + p.susp_c * xdot` is inside the per-patch
loop (`sim/sled.cpp:705`).

### R4-06 ★ THE FINDING: the landing spike is the DAMPER, it is LINEAR, and it is unbounded
At the instant of touchdown `x ≈ 0` and `xdot = vz`, so the first contact tick produces
`normal = 3 × c × vz = 10 800 · vz` newtons — **2.63 g per m/s of sink rate, with no
ceiling and no blow-off**:

| sink rate | damper-only force | as g |
|---|---|---|
| 2 m/s | 21.6 kN | 5.3 g |
| 4 m/s | 43.2 kN | 10.5 g |
| **5 m/s** | 54.0 kN | **13.2 g** |
| 8 m/s | 86.4 kN | 21.1 g |
| 16 m/s | 172.8 kN | 42.1 g |

Confidence: **DERIVED** (from `sim/sled.cpp:705`), and **independently corroborated by the
tapes**: a 1.0 s symmetric hang arrives at 4.9 m/s, and R4-03's measured p50 for the
0.5–1.0 s bucket is **13.2 g** against the derived **13.16 g at 5 m/s**. Two methods that
share no arithmetic agree to three significant figures.
**Why it matters:** the spring is not what the player feels on a send — the spring's whole
stroke is worth 8.74 g at the stop, and the damper passes that **before the suspension has
moved at all**. Every "banging, punchy" landing in this kernel is a damper spike whose
size is a straight line in sink rate.
**Killing mutation:** `xdot` is divided by `axis_dot = max(dot(body_up, up_i), 0.25)`
(`sim/sled.cpp:650-653`), so an off-angle arrival scales the rate by up to 4× — that makes
the spike **larger**, not smaller. The claim dies only if `v_patch` at touchdown is
systematically below the CG's sink rate (it is not: it is `velocity + ω × r`, which on a
nose-down arrival is *higher* at the skis).

### R4-07 ζ = 0.71 — the shipped damper is stiffer than any real shock at the speeds his jumps produce
Per patch: sprung share 139.5 kg, `ω_n = √(k/m) = 18.16 rad/s = 2.89 Hz`,
`c_crit = 2√(km) = 5066 N·s/m`, so **ζ = 3600/5066 = 0.711**.
Real dampers are deliberately **digressive** for exactly this reason: "A linear damper has
n = 1.0 in the F = c × vⁿ equation — double the velocity, double the force"; "A digressive
damper has **n between 0.5 and 0.7**, achieved by shim stacks that blow off above a
threshold velocity"; "Digressive valving lets you run high low-speed damping (good for
body control) **without punishing the chassis on sharp bumps**"; "nearly every modern race
damper above club level is digressive."
Confidence: **DERIVED** (ζ), **LITERATURE** (digressive practice; firgelliauto shock-
absorber mechanism article).
**Read:** the kernel runs a linear damper at a ζ a road car would consider extreme, and
then applies it to 5–16 m/s shaft speeds that a real shock answers with an open blow-off.
This is the single mechanism that turns Chad's "punchy" (§0b — which he **wants**) into the
262 g outlier of R4-03 (which nobody wants).
**Killing mutation:** a measurement showing the shipped `susp_c` was fitted to ride
*quality* on trail chop at 0.1–0.5 m/s shaft speed — in which case 3600 is correct
low-speed valving and the fault is purely the missing high-speed knee, which sharpens the
finding rather than killing it. There is no fit record for `susp_c` in `docs/` that I
found; see §6.

### R4-08 The snowpack is already a bigger shock absorber than the suspension — and nothing tells the player
Suspension and snow are **in series** by construction ("The honest structure is one
deflection `d` shared between the spring and the pack", `sim/sled.cpp:663-670`). Track
patch, Bekker modulus `K = kc/b + kφ = 3800/0.38 + 88000 = 98 000`, area 0.433 m²,
`n = 1.55`; energy to penetrate `z` is `A·K·z^(n+1)/(n+1)`:

| sink z | pack energy | pack reaction |
|---|---|---|
| 0.10 m | 47 J | 0.29 W |
| 0.30 m | 773 J | 1.60 W |
| 0.50 m | 2 843 J | 3.53 W |
| 0.77 m | **8 549 J** | 6.90 W |

Against the suspension's **4 603 J** total spring capacity (R4-05), deep pack at the signed
`pack_ref_depth_m = 0.77` (WINTER_LAW's signed depth) absorbs **nearly twice** what the
shocks can — and it does so with a **rising** `z^1.55` reaction, i.e. digressively in the
displacement sense, the opposite of the damper's linear velocity law.
Confidence: **DERIVED** (Bekker integral from shipped constants), upper bound not a
per-landing measure.
**Why it matters:** "land in the deep stuff" is already a real, modelled, physically
correct survival strategy in this kernel — Bush is 49.3 % of his driving (`gi4` §1) — and
the game never says so, never shows it, and never rewards it. That is a free depth of play
already paid for.
**Killing mutation:** `z` is clamped to `min(d_total, z_cap)` with `d_total ≤ susp_rest_m
= 0.21 m` (`sim/sled.cpp:673, 690-694`), so the pack can never take more than 0.21 m of
the shared deflection in a substep — the 0.5 m and 0.77 m rows are **unreachable in one
substep** and describe the energy an accumulating `sink_m` would absorb over many. If
`sink_m` cannot accumulate that far during a 0.1 s impact, the realised pack absorption is
closer to the 0.30 m row (773 J) and the claim weakens from "twice the shocks" to "a sixth
of them". **This is the one number in this strand most worth a probe leg** (§7 D-1).

### R4-09 In the air the machine has no roll assist, no roll damping, and no pitch authority at all
The comfort assist is multiplied by
`w_contact = clamp01((normal_sum + assist_hull_frac·side_normal_sum)/(0.5·m·g))`
(`sim/sled.cpp:1842-1844`), and the source states the consequence in its own comment:
"**airborne stays exactly 0 (both sums are)**". It also acts only on `torque_body.z` — roll
— never pitch. Airborne, the only forces are gravity and `-½ρ·cda·|v|v` at the CG
(`sim/sled.cpp:2030-2033`); there is **no aerodynamic moment of any kind**.
So between the lip and touchdown, the machine is a free rigid body with **zero** damping
on all three axes and the player's inputs reach nothing.
Confidence: **MEASURED** (shipped source).
**Why it matters:** this is the mechanism under R4-01. A landing attitude is decided
entirely at the lip; 0.5 s or 3.9 s later it arrives unchanged. Two in three are past 45°
because two in three *launches* were, and nothing in between could have helped.
**Killing mutation:** a term I missed that applies torque while `ground_contact == false`.
I grepped every `torque_body` write and every `angular_vel` write in `step_sled`; the only
two that are not contact-gated are the gyro pair and the airborne exchange, and all three
of their dials ship at 0 (R4-12).

### R4-10 The righting moment arrives as a STEP at touchdown, not a ramp
`w_contact` reaches 1.0 at `normal_sum = 0.5·m·g = 2052 N`. By R4-06 the damper alone
passes 2052 N at a sink rate of **0.19 m/s**. So on any landing worth the name the assist
goes **0 → full authority inside one substep** (8.33 ms), and what switches on is
`(−450·tanh(φ/0.14) − 800·ω_z)`: at a landing roll rate of 3 rad/s that is a **2400 N·m**
damping torque appearing in one tick against `I.z = 49.7 kg·m²` — **48 rad/s² of roll
deceleration, from nothing.**
Confidence: **DERIVED** from shipped constants (`roll_stiff_nm 450`, `roll_damp_nms 800`,
`roll_ref_rad 0.14`, `inertia.z 49.7`).
**Why it matters:** the "catch" the player feels on touchdown is real and large — it is
just **discontinuous**, and it is the same one-shot, condition-gated shape R2-09 found the
industry ships deliberately as a *landing assist*. SEADS already owns the mechanism; it
owns it by accident, at the wrong crossing (0.19 m/s of sink), with no window and no
attitude condition.
**Killing mutation:** if `release` (the roll-release band, `roll_release_lo/hi_rad`
0.6/1.2) is near zero at the tilt a bad landing arrives at, the product is small and the
step is cosmetic. That is exactly what happens past ~69°, so this claim is about landings
in the **0–35°** band, not about the ones that are already over.

---

## 3. The air itself — what real riders do, and what this kernel could

### R4-11 The real in-air control is the TRACK, and it works both ways
Trade instruction, verbatim:

- "**The throttle controls the pitch of the sled. If you push on the throttle, it will
  bring the nose of the machine up; the brake will bring the nose down.**" — Jeremy Hanke,
  SnoRiders, *The art of jumping*.
- "**Tap the brake in the air** longer depending on how tail heavy you are"; "**Give it
  throttle in the air to bring it back up.**" — Ashley Chaffin, Backcountry Access,
  *Backcountry Sled Tricks 101*.
- "A hand full of the brake will most certainly send you flying over the bars, and a hand
  full of throttle will send the machine into a tail stand, then catapult you over the
  bars." — Hanke (the authority is large enough to be dangerous).
- The mechanism riders name is the track as a flywheel: when you brake mid-flight "you
  don't actually stop this inertia — you simply **transfer this stored energy into the
  chassis**, causing it to pitch" (snowmobilefanatics/dootalk rider threads, as summarised
  in search; the thread bodies are behind a tollbit redirect I did not follow — see §6).

Confidence: **LITERATURE** (instructional trade press, two independent authors agreeing).
**Killing mutation:** an instrumented measurement showing the pitch change comes
predominantly from the *rider's* body throw rather than the track's angular momentum. Both
are real; the split is unmeasured.

### R4-12 All three air terms are already in the kernel — and all three ship at ZERO, unreachable from TOML
| term | source | shipped value | what it would do |
|---|---|---|---|
| `k_gyro` | `sim/sled.h:755`, used `sled.cpp:1969` | **0.0** | gyroscopic precession: a yaw rate in the air couples into roll |
| `k_gyro_react` | `sim/sled.h:768`, used `sled.cpp:2068-2070` | **0.0** | the reaction wheel: spinning the track up/down pitches the chassis |
| `k_air_shift` | `sim/sled.h:859`, used `sled.cpp:2096-2099` | **0.0** | rider body throw ↔ chassis rotation, momentum-conserving |

And per the audit charter, **only `[sled_comfort]` and `[snowpack]` reach the kernel from
TOML** — none of these three is in either block. They are compile-time zeros.
Confidence: **MEASURED** (shipped source + the charter's TOML reach statement).
**Why it matters:** the honest reading of "should be able to launch in the air"
(`gi4` §1 item 2) is not that the launch is too weak — R4-02 shows he gets 3.9 s of it.
It is that **the air is dead**. The machinery to fix it is already written, already
commented, already bit-identical-at-zero, and already dialled. Nobody has ever turned it
on.
**Killing mutation:** a `[sled_comfort]` or `[snowpack]` key in `config/scenario.toml` /
`config/world.toml` that aliases one of these three (I did not open the TOML files — the
charter's statement of TOML reach is what I relied on; see §6).

### R4-13 Armed, the reaction wheel is worth ~17–22 °/s — about half the authority a real rider uses
`rotor_inertia_track_kgm2 = 0.193` at `drive_radius_m = 0.0815`, `I.x = 158.7 kg·m²`:

| belt change in the air | ΔL | Δω at `k_gyro_react = 1` |
|---|---|---|
| 20 → 0 m/s (full spin-down) | 47.4 kg·m²/s | 0.298 rad/s = **17.1 °/s** |
| 30 → 0 m/s | 71.0 | 0.448 rad/s = **25.6 °/s** |
| 20 → 46 m/s (full throttle in the air) | 61.6 | 0.388 rad/s = **22.2 °/s** |

Against the demand: levelling a 40° nose-high attitude needs **80 °/s** over a 0.5 s hop,
**40 °/s** over a 1 s air, **24 °/s** over a 1.7 s send (his p90).
Confidence: **DERIVED** (shipped constants; sign deliberately not asserted — `sled.h` rules
that a sign is measured by test, never typed).
**Read:** `k_gyro_react = 1` is real but modest authority that only bites on the *long*
airs — which is arguably exactly right, and is a defensible default rather than a tuned
one. Full rider-grade authority is around **k ≈ 2**.
**Killing mutation:** `rotor_inertia_track_kgm2 = 0.193` is documented as a
"momentum-equivalent, at the axle" figure for a 36 lb track. If the real effective inertia
including driveshaft, jackshaft and the clutch side is 2–3× that, every number in the
table scales with it and `k = 1` is already rider-grade.

### R4-14 ★ The kernel can give NOSE-UP authority but structurally cannot give NOSE-DOWN
`rep_belt = max(dv · throttle · track_speed_max_ms, max(v_bf, 0))` (`sim/sled.cpp:1354`).
The belt is driven to at least the body's forward speed **whatever the brake is doing** —
`brake` does not appear in the expression. So:

- **Throttle in the air raises the belt** from `v_bf` to up to 46 m/s → a real ΔL → a real
  pitch impulse if `k_gyro_react` is armed. ✔
- **Brake in the air cannot lower the belt below `v_bf`** → ΔL is zero → no pitch impulse,
  at any dial value. ✘

The real rider's *primary* corrective — "the brake will bring the nose down" (R4-11) — has
**no channel in this kernel**, while the *dangerous* one (throttle, nose up) is fully
wired and waiting on a dial.
Confidence: **MEASURED** (one shipped expression).
**Why it matters:** if `k_gyro_react` were armed tomorrow, the player would get exactly the
half of in-air control that makes a landing *worse* (nose-high → tail-heavy arrival →
"send you flying over the bars", Chaffin), and none of the half that saves it. Arming the
dial without touching the belt expression is the wrong order of operations.
**Killing mutation:** engine braking already appears as a force (`engine_brake_n = 420`);
if the belt is *meant* to be a kinematic ground-speed proxy rather than a driveshaft state,
then the fix is not in this expression at all but in a separate braked-driveshaft state —
which the source's own comment anticipates: "this kernel's belt is **KINEMATIC**, so it
spins up for free. The reaction charges the chassis for momentum whose energy was never
charged to the engine. **Acceptable at this fidelity, and recorded so nobody discovers it
as a surprise.**"

### R4-15 The industry ships this exact term, named, toggleable and tunable
Rainbow Studios (the *MX vs ATV* developer, and the studio R2-10 already credits with the
dual-stick rider body), posting as **Lenore** on the *MX vs ATV Legends* forum, verbatim:

> "**Nose Up/Down (Bike Pitch) is primarily controlled by the wheel gyro and rider lean
> (forward/back)**"
>
> "With **Reflex Gyro** enabled, the bike will pitch up/down **based on the rate of change
> in the speed of the rear tire**. With reflex gyro, the bike will **pitch up as the wheel
> accelerates (gas) and pitch down as the wheel decelerates (brake)**."
>
> "**Helpers are completely disabled with Pro Physics On** and any stick inputs while
> in-air with Pro Physics Off will override the assists entirely."
>
> "'**Airborne Throttle**' toggle in Gameplay Settings is set to '**Auto Disable**' by
> default, which will cut the throttle automatically… You can disable this assist by
> selecting 'User Input'."
>
> "Having +3 on brakes and +3 on acceleration will make the **gyro effect stronger**.
> Likewise, having +3 (Flex) on the chassis will make the bike **easier to rotate in air**."

Confidence: **LITERATURE** (developer statement on the game's own forum).
**Why it matters, three ways:** (1) `k_gyro_react` is not an exotic SEADS invention — it is
a shipped, named, *default-on* mechanic in the biggest off-road franchise, derived from the
same `dω ∝ d(wheel speed)` law. (2) They ship it as **rate-of-change**, exactly the
telescoping-delta form `sim/sled.cpp:2069` already uses — so a spin-up/spin-down cycle
returns every borrowed rad/s and only an attitude change survives. (3) They pair it with an
**airborne-throttle cut, on by default**, precisely because unrestricted throttle in the air
is the failure mode R4-14 warns about.
**Killing mutation:** Lenore being a community moderator rather than a developer. The post
reads as authoritative studio communication (it describes tuning internals and defaults),
but I did not verify the account's role.

### R4-16 Landing with the track already spinning is a real technique — and this kernel half-models it
"**Right before you hit the ground, you will want to give it throttle. You need to get your
track spinning, so the impact isn't so abrupt.**" — Chaffin, BCA. Also "Be at around 3/4
throttle when you land to pull the sled out" (SnoRiders / forum consensus).
In this kernel the belt **does** respond to throttle in the air (R4-14), and track thrust
at touchdown is a function of belt-vs-ground slip — so blipping the throttle before the
skis touch already changes the landing's longitudinal shear. Whether it changes it in the
helpful direction is **unmeasured**.
Confidence: **LITERATURE** (technique), **DERIVED** (that the kernel has a channel for it).
**Killing mutation:** if the track patch's shear law saturates at the slip a landing
produces, the pre-landing blip is inert and the technique is unmodelled. A probe leg
settles it (§7 D-2).

---

## 4. Cross-slope and downslope — the two landings that decide everything

### R4-17 Cross-slope landings roll the machine, and this repo has already MEASURED it
`docs/snowform_terrain_jump_consult.md` §2, quoting `config/world.toml:507-509` and
reproducing it: *"The steep axis is N-S so the straight jump run lands ON the fall line
(probe shape v2, rot -45, **rolled on a cross-slope landing**)"*, with a `rot` sweep
0/20/45/70/90 giving landing vertical **−1.19 / −10.74 / −5.25 / −15.09 / −17.08 m/s** and
lip pitch rate 87 → 171 °/s. Its conclusion: "**The biggest air (3.20 s, 44.9 m, rot 70)
was also the worst landing.** Rotating the takeoff axis off the run line is not a lever, it
is a self-inflicted wound."
Confidence: **MEASURED** (in-repo probe, prior session).
**Read with R4-06:** a −15 m/s arrival is **39 g of damper-only force** before the spring
does anything, arriving asymmetrically on the downhill ski, whose `axis_dot` divisor makes
its own rate larger still (R4-06's killing mutation). The cross-slope landing is not
"unlucky", it is the damper law multiplied by a geometry that loads one corner.
**Killing mutation:** those numbers are from a probe shape (h6/sx4/sy8), not from the
shipped `[snowhill]`; the shipped kicker is gentler and the magnitudes will be smaller.
The *ordering* (rot ↑ → arrival speed ↑) is what transfers.

### R4-18 Land level with the slope, not level with the world — and flat landings are the injury case
- "**The ultimate goal is to land your machine level with the slope you are landing on to
  create a smooth, soft transition.**" — Hanke, SnoRiders.
- "**Landing onto a completely flat landscape can lead to injury. Many a broken back has
  resulted simply from riders landing flat.**" — Hanke.
- "**You want your skis and track to touch down at the same time.**" — Chaffin, BCA;
  done right on a steep landing "**you will hardly notice you left the ground!**"
- MX practice on why the downslope is soft: "If you are going fast, you will continue down
  the slope further as your suspension is compressing… you are **increasing the amount of
  time it takes to dissipate the vertical energy**. In contrast, **speed does nothing for
  you if you land on the flat bottom**." (Vital MX, *Flat landing/casing tips*.)

Confidence: **LITERATURE** (unanimous across snowmobile and MX instruction).
**Why it matters for this kernel:** the geometry of "land on the downslope" is already free
— terrain slope lowers the *closing* speed with the surface, which by R4-06 lowers the
damper spike **linearly**. The kernel already rewards landing downslope. What it does not
have is any way for the player to *achieve* the attitude that cashes that in (R4-09,
R4-12), and no feedback that tells him it mattered.
**Killing mutation:** none on the physics; the transfer to SEADS dies if the kernel's
ground query resolves the landing surface as the *pre-impact* flat rather than the local
downslope facet — which the terrain-clip lane's `facet_contact` work makes unlikely but
which I did not verify.

### R4-19 The absorbed-distance law, quantified: 5× force for the same fall
Lee Likes Bikes, *Drops: calculating landing force*: impact velocity from height, kinetic
energy from velocity and mass, then "to figure out the average impact force (IF), you
divide the kinetic energy (KE) by **the distance travelled after impact (d)**". His worked
case — a 200 lb rider dropping five feet — gives **3000 lb** on a stiff landing against
**600 lb** when the energy is taken through arms, legs and suspension: **5× for the same
drop**, bought entirely with stroke.
Confidence: **LITERATURE** (trade, with the physics stated).
**Read against R4-05:** the kernel's stroke is fixed at 0.26 m and the rider contributes
**nothing** — there is no rider-absorption channel on landing at all (`stand_rise_m`,
`tuck_drop_m` move the CG, not a damper). Real riders buy a factor of five here. The kernel
buys zero.
**Killing mutation:** if the rider-grip/`rider_grip.h` extension already dissipates
landing energy through the arms, this is wrong. I did not read `sim/rider_grip.h` (§6).

### R4-20 The named game shape for all of this is one line, and a developer states it plainly
Livio De La Cruz, *Implementing Racing Games* (Game Developer), verbatim: "it can be
helpful to **artificially apply torque to keep the car's 'up' vector aligned with the
ground's normal vector**"; "The goal is to build something that **feels good** for a racing
game, not necessarily to build something that's faithful to how cars actually work"; and on
the cost of doing it: "you'll need to **cover up many of these kinds of 'hacks' with
animations or visual effects** that make the game look like it's more sophisticated."
Confidence: **LITERATURE** (developer, general practice).
**Why it matters:** this is the same mechanism R2-09 found priced in *Riders Republic*'s
landing modes and the same shape Chad licensed in §0b — "less honest but don't ruin it".
The SEADS-specific observation is R4-10: **the kernel already applies exactly this torque,
on the roll axis, the instant contact begins.** The question is not whether to add an
alien mechanism. It is whether that torque should have a *window* (an approach phase) and a
*condition* (attitude near the surface normal) instead of a 0.19 m/s threshold.
**Killing mutation:** none — this is a design precedent, not a physical claim. It fails as
*guidance* only if Chad rules the air must stay unassisted, which §0b's "arcadey to a
degree so that it is fun… jumps" argues against but does not settle.

---

## 5. Candidate directions — each cites a tape event AND Chad's words

No dial is proposed and no default is named. These are **candidates for a ruling**,
ordered by evidence, not by preference.

**D-A. Give the air a control surface before making landings softer.**
Tape: 984 landings, **31.1 % upright** (R4-01), and in the air the machine has zero
authority on any axis (R4-09) with all three air dials at zero (R4-12). Chad: "should be
able to launch in the air" (`gi4` §1 item 2) and "SLiding banging, punchy, **jumps**"
(§0b). Shape: arm the term the industry already ships under a name — Rainbow's Reflex Gyro
(R4-15) is `k_gyro_react` (R4-13) — **but only after R4-14**, because today the kernel can
only give the half of it that hurts.

**D-B. The belt expression is the blocker, not the dial.**
Tape: the 678 non-upright landings (R4-01) arrive at whatever attitude the lip set, and
R4-02's p90 1.74 s gives a full 1.7 s in which a real rider would be on the brake. Chad:
"allow me to **land on my skis more often** after a roll" (§0). Shape: `rep_belt` cannot
fall below body forward speed (R4-14), so nose-down authority does not exist at any dial
value. Any in-air work starts here.

**D-C. The landing spike is a valve problem, not a spring problem.**
Tape: peak-g rises linearly with hang (R4-03) and the 0.5–1.0 s median (13.2 g) matches the
damper-only prediction at 5 m/s (13.16 g) to three figures (R4-06). Chad: "punchy" is
**wanted** (§0b) but "Just make it more stable" is the same sentence. Shape: every real
race damper is digressive above a knee (R4-07); the shipped one is linear to 16 m/s. A
high-speed knee changes only the outliers and leaves trail chop — the ride he signed —
untouched. **This is the only candidate that makes hard landings softer without touching
the air.**

**D-D. Make the snow the landing zone, out loud.**
Tape: Bush is 49.3 % of his driving (`gi4` §1) and the pack can absorb more than the shocks
(R4-08). Chad: "**Work on the math to achieve a balance of fun and accuracy to real
physics**" (§0). Shape: nothing to add to the physics — it is already there and already
honest. What is missing is that the player is never told, never shown, and never rewarded
for choosing the deep line. Cheapest possible depth of play.

**D-E. The contact step is a landing assist in disguise — give it a window, not a
threshold.**
Tape: 146 R presses, all at ≥69° (R4-04) — meaning the machine that is *nearly* saved is
far more common than the one that is over. Chad: "**slef righting by chance more**" (§0b),
"But not every time — allow it to happen" (§0). Shape: the assist already snaps 0→full at
0.19 m/s of sink (R4-10); the industry shape is a *conditioned window* (R4-20, R2-09). This
changes a distribution, which is exactly the thing Chad asked to be changed, and it
respects RC-1 because abuse still lands outside the window.

---

## 6. UNVERIFIED — named, not hidden

1. **The tollbit-gated rider threads.** The track-as-gyroscope rider explanations
   (snowmobilefanatics, dootalk) are quoted here **from search-result summaries only** —
   both hosts 307-redirect to `tollbit.*` and I did not follow the redirect. The same
   mechanism is independently attested by Hanke, Chaffin and Rainbow Studios, so R4-11
   survives without them; the specific wording does not.
2. **`susp_c = 3600` has no fit record I could find.** R4-07 assumes it was chosen for ride
   quality, not for landings. If a `docs/` note fits it to landing behaviour, the reading
   changes (the claim does not).
3. **I did not open `config/scenario.toml` or `config/world.toml`.** R4-12's "unreachable
   from TOML" rests on the audit charter's statement of TOML reach, not on my own read.
4. **I did not read `sim/rider_grip.h`.** R4-19's "the kernel buys zero rider absorption"
   is therefore a claim about the *suspension* path only.
5. **The 262 g outlier (tape 16) is uninvestigated.** I have no tick index for it — the
   summary exports hang and peak-g lists but not event ticks — so I cannot say whether it
   is a landing, a wall, or a solver artefact. R4-03's p50 column does not depend on it.
6. **No instrumented snowmobile landing dataset was found.** R1 §7 flagged this gap; I
   searched SAE (snowmobile technical papers exist for crash tests 2021-01-0876 and
   acceleration/braking 2011-01-0287, but not for jump landings) and the sports-biomechanics
   literature (water-ski jump landings measured at **26–104 g**, which brackets this
   kernel's p50–p90 but is a different vehicle and a different sensor mount). **The gap
   stands.**
7. **Sign conventions.** R4-13 states magnitudes only. Which way `k_gyro_react` pitches the
   chassis is pinned by test in this codebase and I did not run one.

---

## 7. Probe legs this strand would ask for (none run)

- **D-1** — pack absorption during a real impact: instrument `sink_m[Track]` and the
  spring/pack force split over a 1.7 s send onto Bush vs Road. Settles R4-08's killing
  mutation, which is the largest single uncertainty in this document.
- **D-2** — the pre-landing throttle blip (R4-16): same send, blip vs no blip, measure
  landing `g_eff` and post-landing tilt at +0.5 s.
- **D-3** — the damper knee (R4-07/D-C): sweep an added high-speed blow-off against the
  same recorded sends; the acceptance metric is that trail-chop ride is bit-unchanged.

Every one of these is `tools/sled_probe.cpp` work in
`D:/seads_sandboxes/sled-audit/build` only, per the charter.

---

## 8. Sources

**Chad and the repo (verbatim, cited in place):**
`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0, §0b ·
`Game_loop_idea/WINTER_LAW.md` §2.4c · `docs/gi4_ride_handoff.md` §1 item 2, §6 ·
`docs/snowform_terrain_jump_consult.md` §2 (quoting `config/world.toml:507-509`).

**Shipped source (read-only):** `sim/sled.cpp:56-81, 640-712, 1337-1361, 1836-1890,
1952-1999, 2030-2033, 2064-2099, 2131-2135` · `sim/sled.h:194-202, 383, 739-770, 859,
1237-1244` · tape `# param` headers via `docs/sled_audit/tape_summary.json`.

**Tapes:** 91 tapes / 84 with air / 206.1 min, `D:/flight_sim2/seads-recon/build-play`,
read through strand D-B's `tools/sled_tape_audit.py` summary. No tape was re-parsed or
written by this strand.

**Web (all fetched or searched 2026-09-18):**
- Jeremy Hanke, "The art of jumping", SnoRiders — https://snoriderswest.com/article/journeys/the_art_of_jumping
- Ashley Chaffin, "Backcountry Sled Tricks 101: How to Throw a Snowmobile Jump", Backcountry Access — https://backcountryaccess.com/en-us/blog/p/backcountry-sled-tricks-101-how-to-snowmobile-jump
- Lenore (Rainbow Studios), MX vs ATV Legends physics options — https://steamcommunity.com/app/1205970/discussions/0/6689600878685789672/
- Livio De La Cruz, "Implementing Racing Games", Game Developer — https://www.gamedeveloper.com/design/implementing-racing-games-an-intro-to-different-approaches-and-their-game-design-trade-offs
- Lee McCormack, "Drops: Calculating landing force", Lee Likes Bikes — https://leelikesbikes.com/drops-calculating-landing-force.html
- "Flat landing/casing tips", Vital MX — https://www.vitalmx.com/forums/Moto-Related,20/Flat-landing-casing-tips,1340907
- "Shock Absorber Mechanism Explained", Firgelli — https://www.firgelliauto.com/blogs/mechanisms/shock-absorber
- "Measurement of peak impact loads differ between accelerometers", ScienceDirect — https://www.sciencedirect.com/science/article/abs/pii/S0021929017302233
- "Acceleration and Braking Performance of Snowmobiles on Groomed/Packed Snow", SAE 2011-01-0287 — https://www.sae.org/publications/technical-papers/content/2011-01-0287/
- Riders Republic landing modes (context for R2-09) — https://www.gamespot.com/articles/riders-republic-stunts-guide-what-to-know-about-the-various-control-options/1100-6497519/
- "Don't Diss the Ditch", On Snow Magazine (modern trail travel 9.3 in front / 16.2 in rear) — https://osmmag.com/dont-diss-the-ditch-best-big-bump-trail-sleds-of-2022/
- Sledders (physics-based snowmobile sim; nose-first landings) — https://store.steampowered.com/app/2486740/Sledders/
