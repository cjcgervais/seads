# RED-TEAM — LENS: LAW / FEEL

Target: `docs/sled_audit/ladder_synthesis.md` (MERGED LADDER — SYNTHESIS, 2026-09-18).
Worktree `D:/seads_sandboxes/sled-audit`, branch `audit/sled-ride`, HEAD `ed0a43ce8`.
READ-ONLY. Nothing built, no ctest run, no probe, no `seads.exe`, no dial moved, nothing pushed.
The only file this pass wrote is this one.

**VERDICT: LAND-WITH-FIX.** Nothing on the ladder is a build, so nothing can break tonight; but
three of its eight rungs carry a claim the shipped bytes contradict, and two of those three are
identity / keep-the-fun claims — the one class this repo's precedent (`kernel-v14-leanlead`) is
built on never being wrong. Fix the three P0s in text before the ladder is shown to Chad; the rung
ORDER survives every finding except Rung 3's and Rung 7's ranks.

Confidence vocabulary: MEASURED (read from the shipped bytes this session) / DERIVED / LITERATURE /
GUESS. Every numeric claim below carries its killing mutation.

---

## P0-1 — RUNG 8 CLAIMS THE BACKFLIP IS KEPT **BY CONSTRUCTION**, AND THE CONSTRUCTION DOES NOT DELIVER IT

**The claim.** Rung 8: *"The backflip must remain: the summit test's airborne tilt
(OPEN-SF1-FLIGHT-ROT, 165.5° off the St. Charles lip at ~21 m/s) must be **bit-identical**, which it
is by the `w_contact` gate."* Repeated in the law check: *"Backflip kept — by `w_contact`, identically
zero airborne."*

**Why it is wrong.** MEASURED, `sim/sled.cpp:1842-1844`:
`w_contact = clamp01((normal_sum + assist_hull_frac * side_normal_sum) / (0.5 * mass * 9.80665))`.
It is zero **airborne**. It is **not** zero in the contact ticks *before* takeoff — and a backflip off
a lip is *initiated* in contact: the rotation rate the machine carries over the lip is accumulated
while the track is still loaded on the crest. `pitch_arrest_nms` is
`τ_y = −gain · ω_y · w_contact · w_band` with `w_band` opening at 60°. If the machine passes 60° of
pitch with any track load left on the crest, the damper bites exactly the ticks that set the airborne
rotation rate, and the 165.5° is **not** bit-identical.

**The law it breaks.** `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §1, verbatim:
*"OPEN-SF1-FLIGHT-ROT (the backflip): launch ~21 m/s off the St. Charles lip → 165.5° airborne tilt.
Chad ENJOYS backflips ("yep I can do backflips") — do NOT delete them; the summit test prints the
tilt as declared debt. … Raising honest pitch inertia will calm rotation everywhere at once —
**measure the backflip stays POSSIBLE**."* The charter demands the backflip be **measured**, and §0's
very first sentence is *"Yep I can do backflips."* The ladder substituted a construction proof for
the measurement the charter names.

**Killing mutation for this finding.** Trace the contact-phase pitch angle and `normal_sum` over the
last 0.5 s before takeoff on the summit send. If `w_contact` is already 0 at every tick where pitch
exceeds 60°, the ladder's claim survives and this P0 falls to a P2 wording note. Nobody has run that
trace; it is not in §5's owed legs.

**FIX.** (1) Strike *"which it is by the `w_contact` gate"* — a construction proof is not admissible
for a charter item that says *measure*. (2) Add to §5 a probe leg **L6 — the lip trace**: pitch angle,
`ω_y`, `normal_sum` and `w_contact` over the 0.5 s of contact before the St. Charles takeoff, and make
it a **precondition** of Rung 8, not an invariant of it. (3) Add to the invariant: *the maximum pitch
reached while `w_contact > 0` on a send must sit below the band opening, or the band moves above it
and the rung is re-argued.* (4) Say in the rung that if that max is above 60°, the rung is **dead as
written** and becomes an airborne-only term — which `w_contact` forbids — i.e. dead.

---

## P0-2 — RUNG 6 SHOWS HIM THE DAMPER AND CALLS IT THE HAND THAT HELD HER UP

**The claim.** Rung 6 grades a slag-orange tell on `|assist_nm|` and asks Chad, verbatim in its drive
line: *"does the orange ball light up at moments you agree the machine was saving you"*. Its cited
magnitudes are p50 5–75 N·m, p95 215–765 N·m.

**What `assist_nm` actually is.** MEASURED, `sim/sled.cpp:1876-1882`:

```
tq = (-stiff * tanh(phi / roll_ref_rad) - roll_damp_nms * angular_vel.z) * release_eff * w_contact;
torque_body.z += tq;   s.assist_nm = tq;
```

Shipped: `roll_stiff_nm = 450.0`, `roll_ref_rad = 0.14` (`config/scenario.toml:2016-2017`),
`roll_damp_nms = 800.0` (`:2027`).

`tanh ≤ 1`, so the **spring** half is bounded by `450 · release_eff · w_contact` — and
`release_eff · w_contact` scales BOTH halves identically, so the ratio bound is independent of them.
Therefore:

- **Every `|assist_nm|` above 450 N·m is necessarily damper-majority.** DERIVED, exact.
- At the ladder's own p95 of **765 N·m**, at least **315 N·m — 41 %** — is `800 · ω_z`, which requires
  `|ω_z| ≥ 0.394 rad/s`. DERIVED.

So the tell's bright end is a **roll-RATE meter**. And the rate term is the one the same ladder
refuses to touch and describes, quoting `sim/sled.h:362-365`: *"damping is the dial that kills **the
flick, the drift's body language** and backflip initiation off a lip."* Rung 6 would light the ball
brightest when Chad flicks her — i.e. when he is riding **well** — and ask him whether the machine was
**saving** him. The rung's own stated crux (*"the BAND, not the duty, is the whole rung"*) cannot
separate spring from damper, because `assist_nm` is the only scalar published.

**Second-order cost, and this is the law-feel part.** Rung 6 is billed as *"the precondition for Chad
grading Rungs 2 and 4 from the seat at all."* A mislabelled precondition corrupts two other rungs'
acceptance. Worse, it invites him to rule on `roll_damp_nms` — which he **already ruled**
(2026-08-13, `b971d14a9`) and which §3 refuses on that ground — from a tell that never names it.

**Killing mutation for this finding.** If `roll_stiff_nm` were not 450 (raise
`config/scenario.toml:2016` to 900 and every value up to 900 N·m becomes spring-explicable), or if the
p95 band in PJ read A were measured on a different field than `s.assist_nm`, the 41 % floor goes. The
≤ 450 spring bound itself dies only if `tanh` is replaced or `roll_stiff_nm` moves.

**FIX.** (1) The tell must gate on the **spring component alone**
(`-stiff · tanh(phi/roll_ref) · release_eff · w_contact`). That is a new published scalar, i.e. a
field in `sim::SledState` — so Rung 6's law-check line *"`render/` and `app/` only; `sim/`, `config/`,
`test/golden/` unchanged"* is **false as written** and must be corrected before the rung is ranked.
(2) If the `sim/` touch is unacceptable, the honest move is to **drop Rung 6** and say so, not to ship
the conflated scalar. (3) Either way, restate the magnitudes as *"spring ≤ 450 N·m by construction;
everything above that is roll rate"* so the 51 %-of-roll-budget headline is not read as comfort.

---

## P0-3 — RUNG 2'S ACCEPTANCE INVARIANT BELONGS TO TWO OTHER DIALS, AND ITS THRESHOLD IS NOT THE PINNED ONE

**The claim.** Rung 2 invariant: *"Plus the pinned gate leg
`sled_lean_into_the_carve_tightens_the_radius` must return to ≤ 0.90 (today 0.9003, missing its own
10 % pin by 0.03 pp)."*

**Error (a) — 0.90 is not the pin.** MEASURED, `test/unit/test_sled.cpp:2647`:
`REQUIRE(r_lean < 0.93 * r_free);` and the block's own comment: *"The threshold is held at **0.93** —
loose enough not to be a coin-flip on the third decimal."* The leg **passes today** at 0.9003. Writing
*"must return to ≤ 0.90"* as a rung invariant silently requires **tightening a pinned test threshold**
— a `test/` edit, which this audit's own rules forbid and which the ladder nowhere declares.

**Error (b) — the tree assigns that ratio to different dials.** The same test's printf, MEASURED,
`test/unit/test_sled.cpp:2643-2645`:

```
"[GI4 lean-carve] r_free=%.3f m  r_lean=%.3f m  ratio=%.4f (debt: back under 0.9000 via 9.7 items 2+3)\n"
```

and the comment above it: *"§9.7 items 2 (traction-limit the thrust) and 3 (floor the assist's
`w_contact` gate) are the other two legs of the same loop, and **this ratio is what they have to buy
back under 0.90**."* Item 2 is `traction_mu` — the ladder's **Rung 5**. Item 3 is a `w_contact` floor
— a dial the ladder **does not propose at all**. Neither is `plane_lat_lean_gain`.

So the ladder's self-declared best rung (*"the best rung in all 24"*) carries an acceptance test that
**two other dials, one of them unproposed, are on the hook for.** Under the "really two dials" lens
this is the clearest instance on the board: land Rung 2, watch the ratio not reach 0.90, and the rung
reads as a failure for a reason its dial cannot touch.

**Killing mutation for this finding.** If the `REQUIRE` line read `0.90` rather than `0.93`, error (a)
goes; I read the assertion itself, not only the comment. If `plane_lat_lean_gain` moved that ratio on
a sweep, error (b) would soften — but nobody has swept it, and the tree's own text names the other two.

**FIX.** (1) Strike the gate-leg clause from Rung 2's invariant. Keep only the four-speed steady-radius
probe (33.2 / 33.5 / 33.7 / 33.9 m at v0 = 8/12/16/20) and the *"the non-leaned radius must not move
at all"* mutation — those are `plane_lat_lean_gain`'s own. (2) Move the 0.90 ratio to **Rung 5**, where
the bytes assign it. (3) Add the **`w_contact` floor (§9.7 item 3)** to §3's named-omissions list, as
the missing third leg of the loop, so nobody rediscovers it as an oversight.

---

## P1-1 — RUNG 7'S TAPE EVIDENCE CANNOT REACH ITS MECHANISM; "ARMED DURING ORDINARY RIDING" IS FALSE

**The claim.** Rung 7, under a heading that reads **Tape evidence**: *"STAND is held hard … for
6.8–39.0 % of the v17 drives, mean ≈ 24 % … The catwalk pose … fires on 11.9 % of tonight's ticks. So
**the gate below is armed during ordinary riding, not only during a crash**."*

**Why it is wrong.** MEASURED, `sim/sled.cpp:1679-1700`. The block is entered only when
`right_assist_nm > 0.0 && hands_on`, and then the authority runs through a hysteretic gate on the
**low-passed horizontal speed**, whose own variable is literally `right_assist_armed`:

```
gate_hi = right_assist_max_ms;   gate_lo = gate_hi * right_assist_rearm_frac;
if (armed && right_gs_lp > gate_hi) armed = false;
else if (!armed && right_gs_lp < gate_lo) armed = true;
p_eff = (armed ? 1.0 : 0.0) * push * w_tilt;
```

Shipped: `right_assist_max_ms = 1.3888888888888888` m/s = **5 km/h** (`config/scenario.toml:2088`),
`right_assist_rearm_frac = 0.8` (`:2089`), `right_speed_lp_s = 0.5` (`:2108`). The tree even ships the
leg by name: `test/unit/test_sled_selfright.cpp:301` — **"selfright: above 5 km/h it cannot happen"**.

Every tick in the rung's cited evidence is above 5 km/h: the catwalk is defined as **WOT**, and
STAND-at-24 %-duty is riding, not crawling. **None of those ticks can arm the gate.** The rung's own
killing mutation states the honest version — *"Unverified on tape: no field in the corpus joins slope,
stand and low speed (D-A §8 item 4). Probe-only."* — but it sits three paragraphs under a heading
asserting the opposite, and the assertion is what a reader carries away.

**Killing mutation for this finding.** If `config/scenario.toml:2088` read a large value, or if the
0.5 s low-pass could hold `right_gs_lp` above the gate through a crawl, the finding softens. It reads
1.3889 and the low-pass settles in ~0.5 s.

**FIX.** Replace Rung 7's *"Tape evidence"* block with **"NO TAPE EVIDENCE — probe-only (D-A §8
item 4)"**, and restate the scope as the crawl case its own drive line already describes (*"Sit her
across a steep side-hill at a crawl and hold STAND"*). Then **re-rank it**: it is the only rung on a
tape-led ladder with zero tape support. Note also, in its favour and understated by the rung:
`right_assist_nm` defaults to 0.0 in `sim/sled.h:224` but `config/scenario.toml:2085` ships **2400.0**,
so the block does execute — the rung is not vacuous, only unevidenced.

---

## P1-2 — RUNG 1 IS NOT A SLED DIAL. `surf_mix` HAS A SECOND CONSUMER AND IT IS THE MAN ON FOOT

**The claim.** Rung 1's law check: *"One surface — **it does not add a surface, it removes a
discontinuity between two that already exist**, exactly as `sim/sled.cpp:200-213` was written to do."*

**Why it is incomplete.** MEASURED, `grep -rn surf_mix` over the tree returns two physics consumers,
not one. Besides `sim/sled.cpp:632`, there is **`sim/walker.cpp:98-107`**:

```
const double a = mu_of(p, g.surf);
const double b = mu_of(p, g.surf_b);
const double t = std::clamp(g.surf_mix, 0.0, 1.0);
return a + (b - a) * t;
```

with its own comment: *"A body crossing from trail to bush has exactly the same problem, and the fix
already exists — so it is read, not re-invented."* Flipping `[snowpack] class_blend_m` 0.0 → 1.0
therefore changes **the walker's ground friction at every corridor edge** as well as the machine's. No
sled tape records the walker; the ladder's invariant cannot see it, and the drive checklist never puts
him on foot.

**Standing, which the rung cites but does not finish.** `LANES.toml:939`'s STICK RULING is quoted
verbatim and correctly. But the same lane's `in_flight` field carries, as **its own** owed items:
*"`[snowpack] class_blend_m` 0.0 vs 1.0 (his stick)"* and *"`world/props.cpp` unblended `nearest()`"* —
and its `owns` block records that its pass touched *"nothing in `world/snowpack.{h,cpp}`"*. So the
road-repair lane is holding this dial for Chad, `props.cpp` still classifies by unblended nearest, and
flipping the blend re-opens exactly the drawn-vs-driven registration fork that lane closed.

**Killing mutation for this finding.** If `sim/walker.cpp:106`'s blended `mu_of` were dead (return
value unused), the walker half goes — it is the function's return value. If `props.cpp` has since been
blended, the second half goes; LANES.toml lists it open as of 2026-09-16.

**FIX.** (1) Name the walker consumer in Rung 1's law check; *"one surface"* is true of the class
system and false of the blast radius. (2) Add a walker line to the drive checklist (*"walk the same
corridor edge at 0.0 and 1.0 — do your feet still catch?"*), given that *"feet in asphalt Hwy 144 W"*
is a live open defect on the same corridor geometry. (3) State the `props.cpp` unblended-`nearest()`
divergence as a named risk. (4) Route the A/B **through the road-repair lane that owns
`world/snowpack.*`** rather than presenting a dial another lane is holding as this ladder's Rung 1.

---

## P1-3 — RUNG 3 SAYS ONLY HIS DRIVE CAN SETTLE IT. THE TAPE SETTLES IT, FREE, TONIGHT

**The claim.** Rung 3's third killing mutation: *"the tape cannot tell a held A/D key from a stale
accumulator. Tape 91 @15130 … is equally consistent with him simply holding a steer key through the
press, in which case zeroing the command buys one frame and **the rung is decoration.** Only his drive
settles that."*

**Why it is wrong.** MEASURED, `app/main.cpp:8244-8256`:

```
const double steer_key_rate = 2.0 * frame_dt;
if (steer_l) sled_steer_cmd = std::min(1.0, sled_steer_cmd + steer_key_rate);
if (steer_r) sled_steer_cmd = std::max(-1.0, sled_steer_cmd - steer_key_rate);
if (steer_l == steer_r) {                       // neither held, or both fighting
    const double back = 3.0 * frame_dt;         // release decay, unconditional
    sled_steer_cmd = sled_steer_cmd > 0.0 ? std::max(0.0, sled_steer_cmd - back)
                                          : std::min(0.0, sled_steer_cmd + back);
}
```

The release decay runs **unconditionally** whenever the key is not held. And the tape carries the
COMMAND, not only the bar: `test/harness/sled_tape.h:198-201` writes `in.throttle … in.steer …
in.lean_lat`, and `:105` pins `steer_actual` — the ladder's own D-E-02 metric
`cmd_minus_bar_abs_max = 0.3500` is computed from both. So:

- **held key** ⇒ `in.steer` flat at ±1.0 (or rising) across the ticks after the `O`;
- **released key** ⇒ `in.steer` falls on a **3.0/s ramp**, reaching 0 from full lock in **0.334 s**.

That signature is decisive, read-only, and costs one pass over the same tapes PJ read D already opened
byte-by-byte. DERIVED from the shipped rates; MEASURED source.

**Killing mutation for this finding.** If `bars` is false through the window, `steer_l == steer_r` and
the command decays regardless — which is *still* diagnostic (a decaying command with `steer_actual`
pinned past 0.5 s means the **slew**, not the command, is the cause, and Rung 3 is decoration for a
different reason). The finding dies only if `in.steer` is written to the tape *after* the override
rather than as fed to `step_sled`.

**FIX.** Add **leg X3** to §5's free read-only list — *"the `in.steer` decay signature over the 1.0 s
after each of the six `O` records"* — and make it a **precondition of Rung 3** exactly as X1 is of
Rung 1. Until X3 runs, Rung 3 does not hold rank 3 on a ladder whose ordering principle is measured
tape support: its own text concedes it may be decoration, and the test that would settle it is free.

---

## P1-4 — THE LADDER'S PRECONDITION LINE IS INCONSISTENT WITH THE LADDER'S OWN RULINGS

§4 opens: *"Ordered. Nothing on the ladder should be built before R1–R3 are answered."* But by the
ladder's own text:

- **Rung 1 IS R2** — and the rung's own Standing paragraph calls it *"a ruling request, not a build
  (ruling R1)"* while §4 numbers that ruling R2. A second, smaller inconsistency to fix in the same pass.
- **Rung 4 has no denominator until R4.** The ladder says so: *"Without your definition, Rung 4's
  two-sided invariant has no denominator — the 'must not fall below ~2/min' floor is counting your
  backflips as failures."* That floor is the rung's only fence against buying roll-resistance by a
  route §0 forbids, and it is admittedly undefined.
- **Rung 8 is gated on R7** (the rung says so outright).
- **Rung 2 sits under R5's grip trade** — R5 attaches `lat_mu_scale` as *"the one move that can cost
  you the sliding you signed"*, and Rung 2 is the other half of the same lean-reward argument.

**FIX.** Replace the blanket sentence with a per-rung precondition column: **R1 → all eight;
R2 → Rung 1; R4 → Rung 4; R5 → Rung 2; R7 → Rung 8; X1 → Rung 1; X3 → Rung 3; L6 → Rung 8.** And
renumber the Rung 1 / §4 ruling-id collision.

---

## P1-5 — RUNG 4 SITS ON A COLLISION BETWEEN TWO OF HIS RULINGS AND QUOTES ONLY THE PERMISSIVE ONE

**Verified in his favour first.** §0b's *"Leaning shall enhance the ride an just make it more stable
and **slef righting by chance more**"* is verbatim-correct
(`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0b, checked this session). §0's *"allow me
to land on my skis more often after a roll … But not every time — allow it to happen"* likewise. C2 is
genuinely arcade-lawful under RC-6-as-relaxed, and it genuinely never touches rider mass
(`sim/sled.cpp:1536-1608`: `torque_body.z += dbg_tq_c2` and nothing else — verified).

**The collision the rung does not confront.** Chad's 2026-09-03 P-key retirement rules **STAND alone
rights the tipped sled**; the ladder's own Rung 7 quotes his self-right spec, *"IF ON THE SEAT AFTER A
ROLLOVER **PRESSING THE STAND BUTTON**"* (`config/scenario.toml:2053-2056`). C2 rights the machine with
**no player input at all**. And MEASURED, `sim/sled.cpp:1603-1606`,
`wo = clamp01(1 − |ω_z| / side_right_wref_rads)` makes the term **strongest at near-zero roll rate** —
strongest, that is, on a machine that has **stopped**, which is the population furthest from *"by
chance."* Rung 4's law check asserts *"STAND rights / LEAN throws untouched; R remains the backstop"*
and never puts the two sentences side by side.

The tree already fenced this and the rung should name the fence: `test/unit/test_sled.cpp:2649`,
**`sled_onside_recovery_is_momentum_not_magnetism`** — *"parked, it must STAY DOWN — a stationary
machine that self-rights is the cheat that kills the illusion … KILLED BY: `side_right_vmin_ms` 0 with
the gain high (parked machines right themselves)."* Raising `side_right_gain_nm` alone does not trip it
(`side_right_vmin_ms` still gates), but it walks toward the thing the test is named against.

**Also: Rung 4 is one dial only until its own killing mutation fires.** *"the rung is dead as a gain
move and must become a `side_right_wref_rads` / window move instead"* — a second dial, held in reserve.
Honest, but it belongs in the rung's headline, not its footnotes.

**Killing mutation for this finding.** If Chad rules that §0b's *"self righting by chance more"* governs
over the P-key STAND ruling, the collision dissolves and Rung 4 stands as written.

**FIX.** (1) Add a ruling that quotes both sentences side by side and asks which governs an **unbidden**
stand-up. (2) Name `sled_onside_recovery_is_momentum_not_magnetism` in Rung 4's invariant as the guard
that must stay green. (3) Move the `wo`-shape / `side_right_wref_rads` fallback into that ruling rather
than leaving a second dial in the killing mutation. (4) Hard-gate Rung 4 on R4 (see P1-4) — without a
backflip definition its floor is not a fence.

---

## P2-1 — RUNG 4'S LOAD-BEARING QUOTE IS MIS-CITED

The ladder attributes *"(700, 2.5) is the measured best-of-9 … smooth, non-oscillatory settling
(**stalls ~105-125 deg rather than swinging**) … **the 15 m/s full recovery is not alive at this
pair**"* to **`sim/sled.h:1035-1049`**. MEASURED: that range is the `plane_lat_gain` planing-lateral
comment (it runs *"…0.3 is the measured choice: on Bush at WOT and full lock it takes the steady radius
from 370 m to a dead-flat 33.2/33.5/33.7/33.9 m…"*). The quoted text lives at **`sim/sled.h:497-512`**.
An auditor checking the single strongest sentence in Rung 4 lands on the wrong dial's comment.
*Killing mutation:* none — it is a line-number check; `grep -n "best-of-9" sim/sled.h` returns 497 and 511.

## P2-2 — §2'S "REPORT, NEVER RE-DERIVE" IS SATISFIED IN THE BODY AND DROPPED FROM THE RULINGS

Charter §2, verbatim: *"The signed numbers: mass 331, CG 0.564, stance 0.927, depth 0.77, reach box,
stand_rise. **A finding against one = report to Chad, never re-derive.**"* The ladder's §1(f) finds that
`docs/gi4_ride_handoff.md` §2's *"(331 + 87.5) × 9.81 = 4106 N"* is wrong against the bytes
(`sim/sled.h:543` reads `mass_kg = 331.0 // machine + rider, TOTAL`), and propagates the corrected
3246.0 N into §1(d)'s 2.24× and Rung 6's 51 %-of-roll-budget headline. **The correction is right** — I
checked the mass ledger and the ladder is defending the sealed number against a doc error, not
re-deriving it. But it is a finding against a sealed number's documentation and it appears **nowhere in
R1–R9**. *Killing mutation:* if `rider_mass_kg` entered a force anywhere, §1(f) would be the
re-derivation §2 forbids; `grep -n "p.mass_kg" sim/sled.cpp` returning one line is the check the ladder
already ran.
**FIX:** add a one-line ruling — *"the gi4 handoff's 4106 N is wrong; the bytes are on 3246 N"* — so no
number in this audit is quoted to him on a denominator he has not been shown.

## P2-3 — RC-5'S ENVELOPE CLAUSE IS MET FOR L2 ONLY; **L3 IS MISSING AND IT IS THE HILL LEG**

RC-5, verbatim: *"Any change must show the before/after of the §5 envelope, not just 'tests green.'"*
The §5 envelope is **L1–L5**. The ladder makes **L2** a landing condition on every rung (correct, and
the best process decision on the board), names L1 and L4 in passing, and implies L5 in Rung 5's
invariant. **L3 — "hill traverse: the SF1 side-slope at fixed line; lean uphill vs none →
track-hold/slip difference (RC-2's 'optimal weight distro')" — appears nowhere.** It is the charter's own
instrument for the exact sentence Rungs 2 and 7 both cite: §0's *"say up a hill."* Rung 7 in particular
**removes** hill-time righting authority and has no leg that can tell whether the hill got worse — which
matters on a tree where his most recent terrain word is *"hills killed me 4x."* *Killing mutation:* if
L3 exists under another name in `tools/sled_probe.cpp`, this is a naming fix; the ladder's §5 lists no
hill leg.
**FIX:** add L3 to §5 and make it a precondition of Rungs 2 and 7; add the §5 before/after table to every
rung's landing condition, per RC-5's literal words.

## P2-4 — TWO RUNGS ARE A GAIN PLUS A BAND, WHICH IS TWO DIALS

- **Rung 8** is `pitch_arrest_nms` **plus** a 60° `w_band` opening whose own killing mutation sweeps it
  (*"Set `w_band` to open at 0° instead of 60°"*). A band that opens at 180° is **also** bit-identical,
  so the band is a dial with its own identity and is not named as one.
- **Rung 6** is `render::kAssistTell` **plus** a band, and the rung says outright *"the **BAND**, not
  the duty, is the whole rung"* — i.e. the named dial is not the dial that matters.

**FIX:** for Rung 8, name the band as a second `[sled_comfort]` key with its own identity value and its
own RC-8 loader line + `test_load_scenario.cpp` CHECK; for Rung 6, promote the band to THE dial and
demote `kAssistTell` to a display gain.

## P2-5 — RUNG 7 NAMES AN AGGRAVATOR IT DOES NOT PROPOSE TO MOVE

Rung 7's mechanism paragraph flags `right_dir_eps` shipping **0.04** (*"a 2.29° dead-cone, not the
documented 10°"* — `config/scenario.toml:2102` confirms 0.04) as an aggravator, inside a rung whose dial
is `right_tilt_surf_frac`. A real observation in the right neighbourhood, but a reader grading "one dial
per rung" will read it as scope. **FIX:** move it to §3's named-omissions list or to R6, with one line
saying this rung does not touch it.

---

## WHAT I ATTACKED AND COULD NOT BREAK

Stated because a red-team that only lists hits is not a measurement.

1. **Chad's §0 and §0b quotations are verbatim-exact.** Checked against
   `D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md:16-50`. The §0b elision is
   marked with an ellipsis and drops only *"I am afraid it might ruin something that is reallt good right
   now but just dont ruin it"* — which, if anything, strengthens the ladder's own caution. **No
   paraphrase-into-a-stronger-claim found in the charter quotes.** Every paraphrase finding above is
   against the ladder's reading of *code*, not of his words.
2. **`LANES.toml:939`'s STICK RULING is quoted verbatim.** Verified.
3. **§1(b)'s ski μ_lat table is exact.** MEASURED, `sim/sled.cpp:162-193`: Bush 0.55, TrailMain 0.70,
   TrailTributary 0.72, Road 0.60, LakeIce 0.22; `track_lat_mu = 0.70` at `sim/sled.h:1193`. Six of seven
   above the 0.5097 g tip threshold, as claimed. The spine holds.
4. **Rung 5's "a value exists that pays his ruling and costs his traverse nothing" is TRUE, and I tried
   hard to break it.** I expected the two windows to be disjoint (a looser ceiling should mean more
   runaway). They are not: `docs/snowform_measurements.md` §M8.1 shows the runaway killed at
   `traction_mu ∈ {0.6, 1.0, 2.0, 3.0, 4.0}` (`v_end` 24.8 m/s *rising* → 0.1 m/s) **and** the Bush
   `stand+back` WOT traverse byte-identical to baseline at 3.0 and 5.0 (14.1 / 14.5 / 15.3). **3.0 and
   4.0 sit in both.** And the traverse leg runs in the *stand + lean-back* pose, so the wheelie-pose
   concern the rung raises against itself is already partly measured. Rung 5 is the soundest rung on the
   ladder, and its honest-limit paragraph (inert on Road; **not** the bank-strike fix) is exactly right,
   confirmed at §M8.2–M8.3.
5. **Rung 2's dial is NOT inert on the shipped build.** I suspected a P0: `plane_lat_lean_gain` is applied
   inside `if (p.plane_lat_gain > 0.0 && d.rho_eff > 0.0)` (`sim/sled.cpp:1083`), so a `plane_lat_gain` of
   0 would make the whole rung cosmetic. It ships at **0.3** (`sim/sled.h:1055`). The dial is live on Bush
   and both Trail classes, and the pinned carve test's own field (`flat_field` + `field_at_depth`, no
   `lines`) classifies Bush with `rho_eff = 260`, so the dial reaches that test. The rung's stated honest
   limit (inert on Road and LakeIce, so it cannot answer SK-1d's *"part of what is so fun on the road is
   driving by lean"*) is correct and correctly refuses to be sold as that fix.
6. **Rung 8's "there is no pitch damping anywhere in this kernel" is exact.**
   `grep -nE "torque_body(\.[xyz])?\s*(\+=|-=|=)" sim/sled.cpp` returns exactly five sites — 602 (per-patch
   contact), 1608 (C2, z), 1655 (C3, yaw), 1792 (STAND self-right, z), 1881 (comfort assist, z). Every
   comfort term is roll or yaw. Confirmed.
7. **Rung 3's mechanism, both halves, is exact.** The `app/main.cpp` R block sets `sled_lean_lat = 0.0` and
   `sled_lean_fwd = 0.0` and never touches `sled_steer_cmd`; `app/main.cpp:8263-8266` C zeroes all three
   with the comment *"SK-1c: C recentres the bars too"*; `sim/sled.cpp:292-293` is
   `(s.rolled || !hands_on) ? 0.0 : clamp01(in.throttle)`. The two recovery keys do disagree.
8. **Rung 7 is not vacuous.** `sim/sled.h:224` defaults `right_assist_nm` to 0.0, but
   `config/scenario.toml:2085` ships **2400.0**, so the block executes on the shipped scenario. For the
   record, `config/scenario.toml:2075-2084` says of that number *"⚠ THIS IS A STARTING POINT FOR CHAD'S
   DRIVE, NOT A FLOWN VALUE. Nobody has ruled it."* — so Rung 7 modifies the gate of an **unruled** term,
   a point in its favour.
9. **No rung moves depth 0.77, and every law check's depth line is honest.** `base_m` and `g.depth_m` are
   untouched by every dial named on the ladder.
10. **No rung is a governor in the §0b sense.** Checked each for kernel-commanded rider-mass movement:
    Rungs 1, 2, 3, 5, 6, 8 move no rider mass; Rung 4's C2 writes `torque_body.z` only; Rung 7 **removes**
    commanded rider shift (`right_shift_cmd`, `sim/sled.cpp:1791`) during conformal slope riding. The
    ladder's governor claims all hold. **The one governor-shaped risk on the board is Rung 8's new torque,
    and the ladder itself flags it and gates it on R7.**

---

## VERDICT

**LAND-WITH-FIX.** The ladder's method is sound, its charter quotations are exact, its spine (§1) survives
scrutiny, and its most-attacked rung (Rung 5) is its strongest. But three rungs carry a claim the shipped
bytes contradict — Rung 8's backflip identity, Rung 6's assist scalar, Rung 2's acceptance invariant — and
two of those three are *keep-the-fun* / *identity* claims, the class this repo's landing precedent depends
on never being wrong.

Apply P0-1, P0-2 and P0-3 in text before this document is put in front of Chad. Apply P1-1 and P1-3 before
the rung ORDER is presented as measured: Rung 7's rank rests on a paragraph that does not hold, and Rung 3's
rank rests on a question the tapes can answer tonight for free. P1-2, P1-4 and P1-5 are scope, precondition
and ruling-collision fixes; the P2s are citations and process.

Nothing here requires a dial to move, a test to change, or a line of kernel to be written.

*Red-team pass, lens LAW-FEEL, 2026-09-18. Read-only against main. No dial changed, no config edited, no
goldens moved, nothing built, nothing pushed. The only file this pass wrote is this one.*
