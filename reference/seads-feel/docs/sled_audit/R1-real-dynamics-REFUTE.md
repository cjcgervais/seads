# REFUTATION PASS — strand R1-real-dynamics
Adversarial red-team, 2026-09-18. Read-only against `audit/sled-ride`.
Target: `docs/sled_audit/R1-real-dynamics.md` (66 KB, resumed run, §9 addendum present).
This file is the refuter's own; the target file was NOT edited.

VERDICT: **SOLID_WITH_FIXES**
REFUTED: R1-7, R1-10, R1-19, R1-28.

---

## 0. Method and what was independently re-derived

Every numeric claim in the findings JSON was recomputed from first principles, not
read back from the target. Kernel parameters were read from
`D:/seads_sandboxes/sled-audit/sim/sled.h` and `sim/sled.cpp`. Charter and law were
read at `D:/flight_sim2/Game_loop_idea/...` (the target's §9 location note is correct —
`Game_loop_idea/` does not exist in the audit worktree).

**Arithmetic reproduction: PASS.** Independent perpendicular-distance calculation on
the trapezoid (CG at origin; ski at (+0.86, ±0.4635), rail at (−0.52, ±0.19)) gives
lateral arms 0.25499 / 0.27645 / 0.28747 / 0.30919 m and tip angles 24.32° / 26.11° /
27.02° / 28.72°. The target's R1-26 table prints 0.2550 / 0.2765 / 0.2875 / 0.3092 and
24.33° / 26.11° / 27.01° / 28.73°. Match to the last printed digit.

Also reproduced exactly: R1-8's +32.2% / +13.8% headline; R1-29's `a_lift = 0.3097/phi`
and the phi=0.61 -> 0.508 g crossing and the 0.41/phi, phi~0.806 mutation; R1-21's 28.6% /
11.0% transfers and 76% / 29% / 61% ski-load fractions; R1-12's 87 / 62 / 51 / 49 / 29 m
radii; R1-24's 0.406 / 0.609 / 1.015 m and 2.82 / 3.46 / 4.46 m/s; R1-32's +41.4% and
+35.0%; R1-27's 0.822 g; R1-1's 36.5 in = 0.9271 m; R1-23's 13–18 m/s and 19 m/s.

**Verbatim quote audit: PASS with one defect.** All Chad quotes were checked against
`ROLL_COMFORT_HANDOFF.md` §0/§0b and `WINTER_LAW.md` §2.4c.1. All are verbatim.
The one defect is R1-16's truncation (see FIX-5).

**Signed-law audit: PASS.** No finding proposes moving `pack_ref_depth_m = 0.77`
(`sim/sled.h:900`, "Chad's signed median. NOT a free dial."); none proposes a governor
(R1-33 actively guards it); none proposes capping the wheelie; none proposes ragdoll —
R1-25's dismount branch is `WINTER_LAW` §2.4c.1's own ruled text. R1-26 correctly
identifies its own edit as moving AGAINST "Just make it more stable" and refuses to
file it as a repair. That is the right call and is the strongest single item in the
strand.

**R1-26 is confirmed and is the strand's best finding.** `git log -S` shows
`track_rail_half_m` went 0.14 -> 0.19 in commit `9e7e48a12` ("GI GROUND-INTERACTION
RUNG") with the surrounding comment left untouched, so the file has said 0.14 and run
0.19 since 2026-08-12. The charter itself still prints the stale value
(`ROLL_COMFORT_HANDOFF.md` §1: "the rail split (`track_rail_half_m 0.14`)"). MEASURED,
reproduced, and materially under-documented in two places. Carry it.

---

## 1. REFUTED

### REFUTE R1-10 — the strand's self-declared CENTRAL FINDING is false of the kernel

**Claim:** "on flat packed snow the sled SLIDES OUT BEFORE IT ROLLS. Available lateral
0.2–0.42 g < required 0.510–0.548 g (kernel)... Any flat-ground roll the kernel produces
is a trip or an artefact."

**Refutation.** The claim names the KERNEL's threshold, so it is a claim about the
kernel. The kernel's own shipped lateral table contradicts it. `sim/sled.cpp:1056`:

```
const double mu_l = g.is_track ? p.track_lat_mu : d.mu_lat;
double bite = -normal * mu_l * std::tanh(slip_ang / p.slip_ref_rad);
```

and the surface table at `sim/sled.cpp:162–193` ships ski `mu_lat` = **0.55 Bush,
0.70 TrailMain, 0.72 TrailTributary**, with `p.track_lat_mu = 0.70` (`sim/sled.h:1193`).
Every snow surface in the game supplies a lateral coefficient **above** the 0.510–0.548 g
the same finding says is required to tip. At low speed, where `normal` is the full
machine weight, a plain friction-driven flat-ground roll is available BY CONSTRUCTION.
It is the shipped table, not a trip and not an artefact.

The finding's own killing mutation is the thing that fires: "if the modelled surface
supplies mu >= 0.51". It does, on four of the seven surfaces. The strand wrote the
correct test and did not run it against a file in its own read-only worktree.

**This matters for Chad, not just for the audit.** mu_lat 0.55–0.72 against an SSF of
0.510 is a live, cheap, first-order candidate for the exact thing he named as broken —
"the constant rolling" (§0) — and R1-10 as written steers a converging strand away from
it toward a phantom artefact hunt.

**Second defect, independent of the above:** the 0.2–0.42 g band is a splice. 0.18–0.28
is car-tyre LATERAL friction on winter roads (Can. J. Civ. Eng.); 0.32–0.42 g is
snowmobile LONGITUDINAL locked-track braking (SAE 2011-01-0287, R1-9). Neither is a
measured snowmobile lateral number. The strand's §7 already concedes no snowmobile
cornering-g data is in hand; the band should carry that label at the point of use.

**Third defect:** the kernel's available lateral is strongly speed-dependent and the
finding treats it as a constant. `sim/sled.h:1028–1040` records the MEASURED consequence
of the planing-lift hole — at WOT on Bush at 12 m/s the steered patches retain 4.5% of
weight and the machine holds a **370 m** steady radius at full lock, i.e. ~0.04 g lateral.
So the kernel spans roughly 0.04 g (planing, WOT) to 0.70 g (low speed, full normal).
"0.2–0.42 g available" is not the kernel's number in either regime.

**What survives:** the real-world half — that a real machine on a smooth packed surface
runs out of grip before it runs out of roll margin — is sound and well-sourced. Restate
R1-10 as a real-machine finding and DELETE the kernel-facing sentence.

### REFUTE R1-19 — the named mechanism is absent from this kernel; the killing mutation fires

**Claim:** "MECHANISM behind Chad's signed feel: throttle sustains a drift by spending
the track's friction budget longitudinally..."

**Its own killing mutation:** "If the kernel's track force is NOT budget-limited — if
longitudinal thrust and lateral resistance are computed independently rather than
sharing one friction capacity — step 2 cannot happen... THIS IS THE HIGHEST-VALUE THING
FOR ANOTHER STRAND TO CHECK IN `sim/sled.cpp`."

**It fires.** There is no friction circle. The lateral bite at `sim/sled.cpp:1057–1059`
is `-normal * mu_l * tanh(slip/slip_ref)` — a function of normal load and slip angle
only, with no thrust term and no combined-slip coupling. A grep for
`friction circle | combined slip | traction budget` over `sim/sled.cpp` and `sim/sled.h`
returns only the per-surface **brake** budget (`mu_brake`, `sled.cpp:1232–1246`), which
caps longitudinal braking and never charges lateral capacity. `sim/sled.h:1028` names
the lateral law explicitly as a HOLE that "reads the BEKKER NORMAL REACTION only."

So step 2 of the four-step sequence cannot happen, and whatever produces the drift Chad
signed is not this. The strand was one grep from the answer in its own worktree and
instead shipped the conclusion at DERIVED confidence under the heading "MECHANISM behind
Chad's signed feel."

**What survives — and it is worth keeping:** step 3/4 (thrust acts along the CHASSIS
heading, not the velocity vector, so rising speed-along-heading rotates the velocity
vector toward it and beta shrinks) requires no friction circle at all. That half is very
likely the real source of the straighten-out and should be re-filed on its own as a
separate, checkable claim.

### REFUTE R1-7 — arm conventions swapped mid-comparison, and "the entire gap" is false

**Claim:** "1970s tilt-table results (36° to 52°) are MACHINE-ONLY; **the entire gap** to
the sim's 24–29° is the rider's own mass raising the CG."

Three defects:

1. **Arm swap.** `atan(0.309/0.45) = 34.5°` uses the ski-**OUTER** arm (0.3092 m); the
   "27.0° with rider" it is compared against uses the ski-**CENTRELINE** arm (0.2875 m).
   Two different tipping lines. Held consistent, centreline gives 32.6° machine-only vs
   27.0° with rider (a 5.6° gap), and outer gives 34.5° vs 28.7° (a 5.8° gap).
2. **The reconciliation does not reach the band it claims.** 34.5° is BELOW the 36°
   floor, and nothing in the argument addresses the Rupp's 52°. The sentence "the entire
   gap between '52° tilt table' and '24–29° on the trail' is the rider's own mass" is
   literally false: the rider accounts for about 6°, and roughly 17° of the cited span
   is left unexplained (machine-to-machine variation across two 1970s sleds, which the
   sources themselves attribute to "skinny and top-heavy" vs "lower-slung").
3. **Unstated inconsistency.** Removing an 87.5 kg rider changes the LONGITUDINAL CG
   position too, which moves `ski_fwd_m`/`track_aft_m` and therefore the trapezoid arm.
   The reconciliation changes the mass and the CG height while holding the with-rider
   arm fixed.

Its killing mutation ("a rider-aboard tilt-table result above 36°") is also decorative —
no search for one is logged anywhere in the file or in §7.

**What survives:** the 36°/52° tilt-table numbers themselves (MEASURED, cited) and the
directional point that the rider raises the CG. Restate as "the rider accounts for
roughly 6° of the gap; the remainder is machine-to-machine variation" and drop "entire".

### REFUTE R1-28 — "Chad's signed feel is the FRONT-HEAVY end of that ratio" is contradicted by the kernel table

The real-sport half (carbide-vs-stud as a deliberately tuned ratio, both failure modes
named, four agreeing sources) is fine LITERATURE. The sim-facing sentence is not, and it
is uncited.

The kernel's front/rear lateral capacity ratio is `d.mu_lat / p.track_lat_mu` with
`track_lat_mu` a **constant 0.70**. From the shipped table:

| surface | ski `mu_lat` | ratio vs track 0.70 | balance |
|---|---|---|---|
| Bush | 0.55 | 0.79 | push / understeer |
| TrailMain | 0.70 | 1.00 | neutral |
| TrailTributary | 0.72 | 1.03 | ~neutral |
| Road | 0.60 | 0.86 | push |
| LakeIce | 0.22 | **0.31** | strong push |

The ratio never meaningfully exceeds 1. The machine is **rear-heavy or neutral on every
surface in the game**, and most rear-heavy exactly where R1-28 predicts front-heavy
(ice, 0.31). Chad's signed feel is therefore NOT "the front-heavy end of that ratio";
that end is not reachable in the shipped kernel.

Secondary defect: "the sim does not need a stud model — it needs ONE named dial: front
lateral capacity / rear lateral capacity." The dial pair already exists
(`p.track_lat_mu` vs per-surface `d.mu_lat`, both config-reachable). The finding
under-reads the kernel and files an existing mechanism as a gap.

**What survives, upgraded:** this is a real and useful finding once inverted — the
kernel's front/rear ratio is pinned at <=1.03 and inverted on ice relative to the trade's
description. File it that way.

---

## 2. FIXES (finding survives; the stated item is wrong or under-labelled)

**FIX-1 — R1-8's killing mutation is computed on a RETRACTED arm.** The headline
(+32.2% / +13.8%) uses the corrected 0.2875 m arm; the killing-mutation figures
(+24.7%, +57%) reproduce only on R1-6's superseded **0.2550 m** arm, which R1-26
retracted. Verified: 0.18 x 0.35 = 0.063 against 0.255 -> +24.7%; 0.55 x 0.2644 = 0.14542
against 0.255 -> +57.0%. On the corrected arm they are **+22.1%** and **+50.6%**. Two
arms in one JSON row with no flag. (R1-32 uses the corrected arm throughout and is
clean — the defect is local to R1-8.)

**FIX-2 — R1-13's sidehill criterion is the small-angle form and overstates the angle.**
`a_y/g + tan(theta) >= SSF` drops the coupling term. Exact:
`tan(theta) = (SSF - a)/(1 + SSF*a)`. At 0.25 g lateral against SSF 0.510 that is
tan(theta) = 0.2306 -> **13.0°**, not the stated 14.6°. The static 27.0° is exact and
unaffected. 1.6° optimistic in the "carrying lateral" row.

**FIX-3 — R1-27 / R1-5's "~80%" needs its baseline named.** 0.822 g overstates by **+82%**
against the real machine's 0.452 g but only **+61%** against the kernel's corrected
0.510 g. Post-R1-26 a bare "~80%" reads as the kernel number and is not.

**FIX-4 — R1-1's "independent corroboration" is not established.** `sim/sled.h:530–536`
records the stance as coming from the 1991 factory brochure spec chart. The magazine
inference (38 in RXL − 1.5 in) landing on the same 36.5 in may be the same source chain,
not a second one. A 1 mm match is as consistent with shared provenance as with
independent confirmation. Downgrade "independent" to "consistent with". Note also the
strand's own R1-3 records a 1994 Indy Trail at a **40 in** ski centre — same family,
3.5 in wider — which is worth carrying beside R1-2.

**FIX-5 — R1-16's Chad quote is truncated without an ellipsis, dropping material
context.** JSON prints: "...The feel is really good. Don't lose the feel." The source
§0 reads "...Don't lose the feel, **except the constant rolling**." The dropped clause is
the acceptance bar. R1-19 and R1-20 quote it in full; R1-16 must too.

**FIX-6 — R1-30 over-reads the kernel's depth readiness.** Per-patch `gs.surf` and
`gs.depth_m` do exist (`sim/sled.cpp:627–690`), but the ski's LATERAL law
(`sim/sled.cpp:1057`) takes `d.mu_lat` — a per-SURFACE constant with **no depth term** —
so the ski's yaw contribution is monotone in steer angle and the sign flip R1-30
describes is absent. That is R1-30's own killing mutation, and it fires. Also,
`WINTER_LAW` §2.4c.1 requires independent per-ski contact PATCHES; it does not require
a per-ski depth term, so the "required by §2.4c.1" attribution is a stretch.

**FIX-7 — R1-12's proposed test is aimed at the wrong sign and will never fire.** "Any
kernel leg holding a radius tighter than ~51 m at 16 m/s hands-off is generating lateral
force the snow does not have." The kernel's documented MEASURED behaviour is a **370 m**
steady radius at 12 m/s at full lock (`sim/sled.h:1036–1039`), 7x wider than the slide
limit. The kernel's cornering defect is the opposite of the one this test hunts.

**FIX-8 — the findings JSON silently drops R1-3, R1-11 and R1-15.** All three are present
in the .md (lines 84, 239, 324) and none are marked retracted; the .md's own closing says
"Nothing in §1–§6 is retracted except R1-6's row 1 attribution." R1-6's omission is
correctly superseded by R1-26, but the other three vanish without a note. A converger
reading only the JSON loses them.

**FIX-9 — R1-29's phi model uses two different tipping widths in one comparison.** The
lift condition transfers load across `stance_m` (0.927 m, correct for the front axle) but
the result is compared against the 0.510 g trapezoid threshold, which is built on the
diagonal arm. Defensible, but it should be stated — the two thresholds are not measured
on the same line, and R1-29 is flagged as "THE most testable bridge in the strand."

---

## 3. Items checked and found CLEAN

- **R1-26** — parameter, arithmetic, `git log -S` provenance (`9e7e48a12`), grep coverage
  (`sim/sled.cpp:78` sole use, no TOML override) all confirmed. Its refusal to file the
  0.14 edit as a repair is correct under §0b.
- **R1-33** — every quote verbatim; the RC-6 decode is quoted exactly from the charter.
  This is the guardrail that keeps the rest of the strand from being read as instructions
  and it should travel with any excerpt.
- **R1-21** — transfers, fractions and the 0.45 m CG mutation all reproduce. Its second
  mutation ("if the kernel applies thrust at the CG... NO transfer occurs") is a genuine
  test, and `WINTER_LAW` line 586 is quoted verbatim.
- **R1-24** — `h = n*s` energy balance reproduces; the 2–5 g band is honestly labelled
  GUESS and the two failed search passes are logged in §7.
- **R1-29** — all five phi crossings and the `track_aft_m = 0.86` mutation reproduce. Note
  for the converger: its mutation (2) does NOT fire — `kPatches` is 3
  (`sim/sled.cpp:546`), so the skis ARE independent patches, as the finding hoped.
- **R1-32** — arithmetic clean on the corrected arm; the 0.45 m hang-off is labelled
  GUESS at every occurrence and the rate-limit mutation is a real one.
- **R1-9, R1-14, R1-22, R1-23, R1-25, R1-31** — sourcing and labelling appropriate; §7
  carries the abstract-only and forum-anecdote caveats honestly, including the
  instruction not to add the 33% and 55% figures.
- **§7 as a whole** is unusually honest, in particular "NO KERNEL BEHAVIOUR WAS MEASURED
  BY THIS STRAND". All four refutations above are instances of that declared gap being
  crossed anyway in a claim or conclusion.

---

## 4. Why SOLID_WITH_FIXES and not UNSAFE

Nothing here is fabricated. The arithmetic reproduces to the last printed digit
everywhere it was checked, the Chad quotes are verbatim, the kernel parameters are read
correctly, and no finding contradicts a signed law or proposes a change against a ruling
without saying so. R1-26 is a genuine, material discovery.

All four refutations share a single failure mode: a real-world conclusion extended into
a kernel claim without opening `sim/sled.cpp` — which the strand itself declared it had
not done, and which is one grep away in its own read-only worktree. That is a scoping
failure, correctable by restating four claims as real-machine findings and re-filing
their kernel halves as open questions. It is not a trustworthiness failure.

**One escalation for whoever converges this.** The refutation of R1-10 surfaces the
cheapest live candidate for Chad's actual complaint that this audit has produced: ski
`mu_lat` of 0.55–0.72 on snow against a static tip threshold of 0.510 g means a
low-speed flat-ground friction roll is available by construction, with no trip required.
That is a measurement to take, not a change to make — and any change there moves the
number R1-26 correctly identifies as running against "Just make it more stable."
