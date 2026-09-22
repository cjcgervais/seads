# R1 — REAL DYNAMICS: how a 1990s 2-stroke trail sled actually behaves at the limit

Strand: **R1-real-dynamics**. Audit worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. **READ-ONLY against the kernel** — nothing in `sim/`, `control/`,
`config/`, `test/golden/` was opened for write by this strand. No dial changed, no
probe built, no test run.

Confidence vocabulary used throughout: **MEASURED** (a number somebody instrumented),
**LITERATURE** (published/trade/industry statement, not re-derived here), **DERIVED**
(arithmetic done in this document from stated inputs), **GUESS** (labelled, always).

Every numeric claim carries its **killing mutation**: the change to an input that would
make the claim false. A claim without one is decoration.

---

## 0. The two documents this strand answers to

Chad's ruling on the sled's handling, **verbatim**, from
`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0 (drive 4, 2026-08-12)**:

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more
> often after a roll** (even though R works). But not every time — allow it to happen.
> Also I've seen lots of snowmobiles **drift around corners and then with throttle,
> straighten out. The feel is really good. Don't lose the feel**, except the constant
> rolling. **Make it possible to roll but not the rule.** ... **Just allow the balance of
> body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to be the
> necessary condition of not rolling over.** Just allow leaning a certain way in a
> particular condition to be the OPTIMAL weight distro for better traversing, say up a
> hill or around a corner. **Work on the math to achieve a balance of fun and accuracy to
> real physics, just as our airplane ontological counterpart does.**"

And **§0b (2026-08-12 night, SUPERSEDING)**, verbatim:

> "Leaning shall enhance the ride an just make it more stable and slef righting by chance
> more. It should be arcadey to a degree so that it is fun., SLiding banging, punchy,
> jumps. Just make it more stable."

And the world-law ruling that names the trip, verbatim, from
`Game_loop_idea/WINTER_LAW.md` **§2.4c.1 (Chad, 2026-08-10)**:

> "approaching a high one at an off angle tilts the snowmachine, animating the ski
> suspension on the bank side first, then as it climbs on an angle it resolves to balance
> as both skis breach the top. **A too steep side approach at speed should also roll the
> snowmachine**, where the rider falls off and needs to run to the machine and get back
> on it."

**The whole of this strand's value is one convergence:** the real machine's physics and
Chad's ruling agree, and the agreement is quantitative, not rhetorical. On flat packed
snow a real 1990s trail sled **cannot generate enough lateral force to tip itself** — it
slides out first. It rolls when it is **tripped**: a bank taken at an angle, a berm, a
sidehill, a hooked carbide on ice, a trench. "Roll possible but not the rule" is not an
arcade concession. It is what the friction numbers say.

---

## 1. The machine — 1990s Polaris Indy 650 class

### R1-1 The kernel's signed stance is the base Indy 650's factory stance, exactly
`SledParams::stance_m = 0.927` m = **36.50 in**. Independent trade-press review of the
1991 Indy 650 RXL states its ski stance was **38 in, "1.5 inches greater than the
standard 650"** — so the standard 650 stance was **36.5 in = 0.9271 m**. This is an exact
independent corroboration of a number the charter (§2) lists as SEALED.
Confidence: **MEASURED** (trade-press spec; two independent statements agree to 1 mm).
Source: snowmobile.com, *1991 Polaris Indy 650 RXL EFI*.
**Killing mutation:** if the RXL's 38 in figure were the *base* 650's stance rather than
the RXL's, the base machine would be 0.965 m and the kernel would be 38 mm narrow.

### R1-2 The era's trail sleds were narrow by modern standards, and got wider on purpose
Snow Goer: trail-sled stances "increased in width from the old standard **38 and 40ish
inches** to now reaching about **43 inches** wide"; mountain stances fell from ~42 in to
"the **34- to 36-inch** range". The article's principle: "a wider sled is more laterally
stable, meaning it absorbs more force applied laterally, than narrower ones", and
"typically, the wider the stance, the higher the tipping point".
Confidence: **LITERATURE**. Source: Snow Goer, *The Math Behind Ski Stance*.
**Consequence for this audit:** a 36.5 in machine is at the **narrow end** of its own
era's trail class and 6.5 in narrower than a 2020s trail sled. A player who has ridden a
modern sled will find the modelled machine genuinely tippier — and that is *correct*, not
a bug. Chad's "she's too unsteady" is aimed at a machine that really was.
**Killing mutation:** a source showing 1990s trail stances clustered at 36 in rather than
38–40 in would remove the "narrow for its class" half of this claim.

### R1-3 Mass ledger cross-check
Contemporary spec for a **1994 Polaris Indy Trail** (488cc fan twin): **dry weight 438 lb
(198.7 kg)**, ski centre **40 in**, track **15 x 121 in**, **8 in travel front and 8 in
rear**, fuel 10.7 US gal. The kernel's ledger (`sim/sled.h` comment) cites a 1991 Polaris
factory chart at **486 lb (220.4 kg) dry** for the base Indy 650 — 48 lb heavier than the
Trail, which is the right order for a liquid-cooled 648cc **triple** versus a fan twin.
Confidence: **MEASURED** (Indy Trail figures); the 650 figure is corroborated only by
plausibility, not independently re-sourced (see §7).
Sources: goneoutdoors.com *1994 Polaris Indy Trail Specs*; snowmobile.com (RXL review).
**Killing mutation:** a factory 1991 Polaris spec chart printing anything other than
~486 lb for the base 650 would move the kernel's whole derived-inertia chain (which is
scaled at 331/340 off that mass).

### R1-4 Suspension travel of the era: ~8 in (0.20 m), both ends
1994 Indy Trail: "IFS front suspension with 8-inch travel", "ITS rear suspension with
8-inch travel". The 1991 650 RXL: "more than six inches" front, "nearly 8 inches" rear
("Dial-Adjust"). Free sag should be ~**20% of total travel** (Snow Goer / Dennis Kirk
setup guidance).
Confidence: **MEASURED** (Trail), **LITERATURE** (RXL, sag rule).
Sources: goneoutdoors.com; snowmobile.com; snowgoer.com suspension how-to.
**Killing mutation:** a spec sheet showing 10–12 in of travel on a 1990s trail Indy would
raise every landing-energy number in R1-24 by 25–50%.

---

## 2. The rollover geometry — why it is a trapezoid, not a rectangle

### R1-5 The support polygon is a TRAPEZOID and that is physically real
A snowmobile's ground contacts are **two skis, wide and forward** and **one track, narrow
and aft**. The tipping axis for a roll is therefore the line joining the downhill ski
contact to the downhill edge of the track's bearing surface — a **diagonal**, not a
lateral line. The kernel already names this (`sim/sled.h`: "the real tipping axis runs
ski-outer to RAIL-outer, not stance/2"; `track_rail_half_m` exists precisely to stop the
support polygon collapsing to a triangle).
Confidence: **DERIVED** from the machine's contact layout, which is universal to the
vehicle class.
**Killing mutation:** a machine with dual rear tracks or outboard skis at the rear would
make the polygon a rectangle and invalidate every number in §2.

### R1-6 Static rollover threshold of the modelled machine: **0.45–0.55 g**, tip angle **24–29°**

> **★ CORRECTED IN §9 — see R1-26.** Row 1/2 below use 0.14 m for the rail half, which is
> the value `sim/sled.h`'s *comment* gives for the REAL Indy. The **live parameter is
> `track_rail_half_m = 0.19`** (`sim/sled.h:569`), so the KERNEL's thresholds are
> **0.510–0.548 g / 27.0–28.7°**, not 0.452–0.548. Rows 1 and 2 describe the real machine;
> rows 3 (recomputed in R1-26) and 4 describe the sim.

Inputs, all quoted from the charter's SEALED list / `sim/sled.h` params (not re-derived):
total mass 331 kg (machine 243.5 + rider 87.5), **CG height 0.564 m**, stance 0.927 m
(half 0.4635), ski width 0.135 m (ski outer edge at 0.531 m), track width 0.38 m (half
0.19), `track_rail_half_m` 0.14, skis **0.86 m ahead** of the CG, track patch **0.52 m
behind** it.

Perpendicular distance from the CG's ground projection to the tipping line, three ways:

| tipping line | lateral arm a | SSF = a/h | static tip angle |
|---|---|---|---|
| ski centreline (0.4635) → rail outer (0.14) | 0.255 m | **0.452 g** | **24.3°** |
| ski outer (0.531) → rail outer (0.14) | 0.276 m | **0.490 g** | **26.1°** |
| ski outer (0.531) → track outer (0.19) | 0.309 m | **0.548 g** | **28.7°** |

(Arithmetic, first row: line through (0.86, 0.4635) and (−0.52, 0.14); |cross| / |d| =
0.36142 / 1.41741 = 0.25499 m; 0.25499 / 0.564 = 0.4521; atan = 24.34°.)

Confidence: **DERIVED** (exact arithmetic from stated, signed inputs).
**Killing mutation:** raising `cg_height_m` from 0.564 to 0.70 drops the first row to
0.364 g / 20.0°; lowering it to 0.45 raises it to 0.567 g / 29.5°. The claim dies on the
CG height, not on the stance.
**Note the naive number is nearly double:** a symmetric half-stance arm (0.4635/0.564)
would read **0.822 g / 39.4°**. Anyone reasoning about this machine's roll margin from
"stance over twice CG" is overstating the margin by **~80%**.

### R1-7 The 1970s tilt-table numbers, and why the sim's are lower
Snow Goer ran a **rollover tilt table** in the 1970s "to measure the angle at which each
snowmobile's upper ski began to lift". Measured: the "skinny and top-heavy" **Roll-O-Flex
GT 340 flopped at 36 degrees**; the "lower-slung" **Rupp Nitro II could be tilted 52
degrees** before going over.
Confidence: **MEASURED** (contemporaneous instrumented test). Sources: Snow Goer,
*Snow Goer 1973-74: The Shakeout Continues*; *The Math Behind Ski Stance*.

**Reconciliation (DERIVED):** those tests were **machine-only, no rider**. Re-running
R1-6's third row with the rider removed — machine 243.5 kg with a CG around 0.42–0.45 m —
gives atan(0.309 / 0.45) = **34.5°**, landing right on the 36° end of the measured 1973
band. **The entire gap between "52° tilt table" and "24–29° on the trail" is the rider's
own mass raising the CG.** The rider is 87.5 / 331 = **26.4%** of the system mass and sits
far above the machine's CG.
**Killing mutation:** a rider-aboard tilt-table result showing >36° would break the
reconciliation and indict the kernel's CG height as too high.

### R1-8 Rider lean is worth **+30 to +36% of roll margin** — an enhancer, quantified
Rider mass fraction 87.5/331 = 0.2644. Kernel reach box: **0.15 m seated, 0.35 m
standing**. A full standing lean moves the *system* CG laterally by 0.2644 × 0.35 =
**0.0925 m**; seated, 0.0397 m.

Against R1-6's arms: standing lean adds **+36.3%** (0.255 arm), **+33.5%** (0.276),
**+29.9%** (0.309). In cornering terms the first row goes **0.452 g → 0.616 g**; the
seated lean gives 0.452 → 0.522 g (**+15.6%**).
Confidence: **DERIVED** (arithmetic on the kernel's own signed reach box and mass split).
**Why it matters:** this is the numeric shape of Chad's §0 ruling — lean "**tighten a turn
instead of having to be the necessary condition of not rolling over**". A **+36%** roll
margin is a large, felt benefit, and it is **not** the difference between upright and
over, because (R1-10) the surface cannot deliver 0.452 g in the first place.
**Killing mutation:** if `rider_mass_kg` were 60 instead of 87.5 (fraction 0.18), standing
lean would buy only +24.7% and would read as cosmetic; if the reach box opened to 0.55 m,
lean would buy +57% and would start to *be* the condition Chad forbade.

---

## 3. What the snow can actually deliver

### R1-9 Measured snowmobile longitudinal performance on groomed/packed snow
SAE 2011-01-0287, ~**500 straight-line tests**, February 2010 rural Ontario, four sleds,
two- and four-stroke, **80–135 hp**, professional rider, IACS-monitored groomed snow,
target speeds **20–60 km/h**:
- acceleration **0.27 g** at quarter throttle, **0.70 g** maximum at full throttle;
- **full braking (locked track) deceleration 0.32 g to 0.42 g** across all test sleds;
- roll-down deceleration **decreased as speed decreased**, and showed **no difference
  between power-on and power-off**;
- "all four snowmobile speedometers consistently over-reported the actual sled speed,
  often by more than 10 km/h";
- "rider weight shifts were necessary for directional control during high-acceleration
  and braking events".
Confidence: **MEASURED**. Sources: SAE 2011-01-0287 (abstract page); Canadian Underwriter,
*Snowmobile Acceleration and Braking Performance* (summarising the same test programme —
the numbers above are quoted from that summary).
**Killing mutation:** a colder/harder or softer test surface. 0.32–0.42 g is *groomed
packed snow*; glare ice would roughly halve it and a slushy spring trail changes the shape
entirely.

### R1-10 THE CENTRAL FINDING: on flat packed snow the sled **slides out before it rolls**
Peak available **lateral** acceleration on winter surfaces:
- lateral friction coefficient for winter road conditions measured in the range
  **0.18–0.28** (*Lateral Coefficient of Friction for Characterizing Winter Road
  Conditions*, Can. J. Civ. Eng.);
- "available lateral acceleration on scraped ice could vary between **0.2 and 0.4 g**
  within a day" (same literature line);
- snowmobile-specific **longitudinal** capability caps at **0.42 g** braking (R1-9), and a
  snowmobile track's lug pattern is **transverse** — built for longitudinal bite and far
  weaker sideways.

Compare against R1-6: static rollover threshold **0.452–0.548 g**.

**0.2–0.42 g available < 0.45–0.55 g required.** A real 1990s trail sled on level packed
snow, hands-off, **cannot corner hard enough to tip itself**. The rear steps out, the
machine slides, and the run continues. This is precisely Chad's §0 bar — *"I rule that it
should be roll resistant"* and *"Make it possible to roll but not the rule"* — arrived at
from friction data with no feel input.
Confidence: **DERIVED** from two MEASURED inputs (winter-surface lateral friction; the
kernel's signed geometry).
**Killing mutation:** this claim dies the instant either input moves — if the modelled
surface can supply lateral force at μ ≥ 0.45 (studded ice, a hooked carbide, a berm wall,
a packed rut acting as a rail), **or** if the effective lateral arm in the kernel is
smaller than 0.255 m. Both of those are the *trip* conditions of §4, and they are exactly
where a roll SHOULD be available.

### R1-11 Corroboration from vehicle-dynamics literature: low μ means slide, not roll
A simulation study of snowy/icy conditions reports that "road adhesion coefficients
ranging from **0.10 to 0.20 lead first to sideslip**, while coefficients of **0.21 to 0.35
lead straight to rollover**" for high-CG vehicles, and that winter conditions drop
tire-road friction "from approximately 0.9 in dry conditions to as low as 0.28 on snowy
surfaces".
Confidence: **LITERATURE** (MDPI *Sustainability* 16(2) 888; abstract only — full text
returned HTTP 403, see §7).
**Read carefully:** that study's 0.21–0.35 rollover band belongs to *buses and trucks*
with SSF well under 0.45. A snowmobile at SSF 0.45–0.55 sits **above** the band, which is
the same conclusion as R1-10 reached by a different route.
**Killing mutation:** applying that paper's thresholds to a snowmobile directly, without
the SSF correction, would invert the reading. That is why this is corroboration and not
evidence.

### R1-12 Measured snowmobile cornering exists; the radii are small
SAE on-snow cornering tests (2014 test season) report maximum and average cornering speeds
and lateral accelerations for turns of radius **20, 35 and 65 ft (6.1, 10.7, 19.8 m)** on
level packed snow, with and without a passenger; the paper also discusses "**snow density,
trenching, and snow mass momentum exchange**".
Confidence: **MEASURED** that the tests exist and at what radii; the **lateral-acceleration
values themselves are paywalled and are NOT in hand** (see §7).
Source: *Snowmobile Cornering and Acceleration Data from On-Snow Testing* (2015).

**DERIVED sanity numbers using R1-10's band:** at 0.30 g, a 19.8 m radius corner is taken
at **7.6 m/s (27 km/h)**; at 0.45 g, **9.3 m/s (34 km/h)**. Conversely, at a trail-typical
**12 m/s** the minimum non-sliding radius at 0.30 g is **49 m**, and the radius that would
reach the 0.452 g tipping threshold is **32 m**. At **16 m/s** the tipping radius is
**58 m**.
**Use for the audit:** any kernel leg that holds a radius tighter than ~58 m at 16 m/s
hands-off is generating lateral force the snow does not have, and the roll it produces is
a kernel artefact, not physics.
**Killing mutation:** obtaining the paper's actual lateral-acceleration table. If measured
snowmobile cornering exceeds 0.45 g on packed snow, R1-10 and this row both fall.

---

## 4. Why it DOES roll — the trip

### R1-13 Rollover is a tripped event, and the real trips are named
No flat-ground friction path reaches the threshold (R1-10), so every real rollover of this
machine class needs an external lateral impulse or a gravity assist:
1. **Sidehill / bank taken at an angle.** A side slope θ adds tan(θ) directly to the
   demand. **DERIVED tipping criterion:** roll when `a_y/g + tan(θ) ≥ a/h`, i.e. at
   **0.452**. With zero lateral accel, static tip at **θ = 24.3°**; carrying 0.25 g of
   lateral, tip at **θ = 11.4°**. Chad's own world law names exactly this case:
   *"A too steep side approach at speed should also roll the snowmachine"*
   (WINTER_LAW.md §2.4c.1).
2. **A hooked carbide on ice.** Carbide runners are steel edges that "extend past the
   bottom of the ski to make direct contact with snow or ice"; "carbides with 60-degree
   angles bite deeply into the snow, allowing for sharper turns", and too much carbide
   causes **darting** — "your machine gets stuck in the tracks of other snowmobiles or
   jumps between tracks". A ski edge that suddenly bites while the machine is already
   sliding is a **ground-level lateral force uncoupled from the friction limit** — the
   classic trip.
3. **Trenching / a dug-in track in deep snow.** The track digs a trench whose wall then
   supplies lateral reaction well above surface friction. Rider guidance: "when in deep
   snow you want enough track spin to move but not so much that you trench yourself into a
   hole"; contributing causes named are approach angle, "not enough ski pressure if the
   rear shock/springs are too soft", and "too much throttle instantly".
4. **A berm, rut wall, or snowbank edge** — geometric, same mechanism as (2).
Confidence: (1) **DERIVED**; (2)–(4) **LITERATURE** (Ski-Doo runner guide, Dennis Kirk
*Choosing the Right Carbides*, Bergstrom Skegs selection guide, SnoWest trenching threads).
**Killing mutation:** a kernel in which the ski's lateral force saturates at the same
coefficient as the track's would make (2) impossible and would leave slope as the only
rollover path.

### R1-14 Injury epidemiology says rollover is the OFF-TRAIL signature
In snowmobile crash series: "being thrown, flipped, or roll-over accounted for **33%** of
injury causes" (n = 107); and among **non-traffic** crashes specifically, "**rollovers
accounted for 55%** of incidents", versus traffic crashes where "collisions predominated
(56%)".
Confidence: **LITERATURE** (Wilderness & Environmental Medicine 1999, *Risk factors and
patterns of injury in snowmobile crashes*, and related trauma-registry series — abstracts
only, see §7).
**Why it matters here:** rollovers are **common** — so "never rolls" is as wrong as
"always rolls" — but they cluster **off-trail**, where the trips of R1-13 live. That is
the distribution Chad asked for: *"Make it possible to roll but not the rule."*
**Killing mutation:** a series showing rollover as the dominant *on-trail* mechanism would
move the roll rate back toward the flat-ground case and contradict R1-10.

---

## 5. The cornering behaviour itself — push, drift, and the throttle

### R1-15 The track fights the turn; the skis do the turning
Patent background (US 3,974,890, snowmobile suspension): "**the long, flat track provides
excellent traction, but tends to drive the snowmobile in a straight line, even in corners,
therefore requiring that corners be negotiated at slow speeds**"; the ski steering system
alone cannot overcome the track's resistance to lateral movement.
Confidence: **LITERATURE** (patent background — the industry's own statement of the
problem).
**Killing mutation:** none available; this is a design statement, not a measurement. Treat
it as a mechanism hypothesis, not a number.

### R1-16 The yaw balance is SURFACE-DEPENDENT, and it flips
The most consequential mechanism finding of the strand. **DERIVED** from the friction
circle plus the LITERATURE on ski/track bite:
- **Hardpack and ice**: the front's lateral capacity is set by **steel carbide edges**
  biting a hard surface; the rear's is set by a **smooth, transversely-lugged track
  sliding sideways**. Front capacity ≫ rear capacity ⇒ the machine is **oversteering**:
  the rear breaks away first. **This is Chad's drift.**
- **Loose / deep snow**: the ski has nothing to bite. It **plows** — building a snow wedge
  whose reaction grows slowly with slip angle — while the track's long footprint keeps
  driving forward. Front capacity ≤ rear capacity ⇒ the machine **pushes / understeers**.
  This is why the rider guidance inverts (R1-17).
Corroborating tyre data on winter surfaces: "force versus slip angle curves for **ice**
reach a maximum value and then **decrease**, **packed snow** curves climb more gradually
and reach 90% or more of their maximum by **7 to 10 degrees**, while lateral forces for
**fresh snow** surfaces **continue to increase throughout the slip angle sweep**,
particularly for deeper snows" (SAE 2006-01-1627).
Confidence: **LITERATURE** for the slip-angle curve shapes (MEASURED in that paper);
**DERIVED** for the front/rear balance flip.
**Why it matters:** a kernel with one lateral-force law for all surfaces cannot produce
both behaviours, and the two behaviours demand **opposite rider technique** (R1-17). This
is the single richest available source of "feel" in the machine.
**Killing mutation:** if the kernel's ski lateral force and track lateral force scale by
the *same* snow-hardness term, the balance never flips and R1-16 is unmodellable — the
finding then becomes a change request, not a description.

### R1-17 Rider technique INVERTS between trail and powder — the lean law is not one law
- **Trail / hardpack (lean IN, load the inside ski):** official rider curriculum — "**Lean
  into turns to gain more control while turning**" and "**Placing more body weight forward
  and into the turn puts more loading on the inside ski and keeps it down on the snow,
  giving it a better bite**" (snowmobile-ed.com, *Turning*). Trade guidance: "Shift your
  body weight slightly to the inside of your snow machine to counter balance the outside
  pull of the centrifugal force"; "**Lean forward to place as much downward pressure on the
  inside ski as possible. As you come out of the corner, gradually shift your weight back
  to centre**"; cover the brake and "dip" it "in the case of under steer" (Intrepid
  Snowmobiler, *Thinking Like Pros*).
- **Deep snow / off-trail (weight OUTSIDE, counter-steer, throttle-pivot):** "In deep snow
  or off-trail riding, turns require more body movement. **Shift your weight to the outside
  running board, counter-steer with the handlebars, and use throttle to help pivot the
  sled.**"
- **Sidehilling:** "**counter-steer, apply a quick aggressive on/off throttle action (just
  enough to spin the track for a split second) and pull the handlebars toward you at the
  moment the track breaks free**"; "once the sled is on edge, you are in control"; the
  position is established **wrong foot forward**, the inside ski **anchored into the hill**.
Confidence: **LITERATURE** (rider curriculum + technique press; sidehill lines from the
KLIM/SnoWest Matt Entz piece and AMSOIL's how-to).
**Why it matters, against Chad's words:** his §0 ruling asks for "leaning a certain way in
a **particular condition** to be the OPTIMAL weight distro for better traversing, **say up
a hill or around a corner**". The real machine's technique is *already* condition-split
exactly that way. The ruling is asking for the real thing.
**Killing mutation:** a technique source that says to lean *in* in deep powder would
collapse the two laws back into one.

### R1-18 Counter-steering is real on a snowmobile, and it is not the motorcycle effect
"For a left turn, you should counter steer to the right. **Counter steering removes the
support of the left ski and carves out snow to let the machine fall left or inside the
turn.**" It is a **contact-patch unloading and snow-carving** action — nothing to do with
gyroscopic precession. Riders are also told to "be prepared to gently counter-steer when
cornering, especially as you exit and apply throttle".
Confidence: **LITERATURE** (technique press and rider forums, consistent).
**Killing mutation:** a kernel that models steering purely as a yaw-rate command, with no
per-ski normal-load consequence, cannot produce this at all — the finding would then be a
gap report rather than a description.

### R1-19 The drift-then-throttle-straightens sequence: the mechanism behind Chad's canon
Chad, §0: *"I've seen lots of snowmobiles **drift around corners and then with throttle,
straighten out. The feel is really good. Don't lose the feel**."*

**DERIVED mechanism** (friction circle + thrust geometry):
1. Corner entry on hardpack: the carbides hold the front, the track's lateral capacity is
   exceeded, the rear yaws out — sideslip angle β grows (R1-16).
2. **Throttle now does two things at once.** It spends the track's remaining friction
   budget on longitudinal force, which *reduces* the lateral force the track can still
   make — this **sustains** the slide. And the thrust vector points along the **chassis
   heading**, not along the velocity vector, so it accelerates the CG in the direction the
   sled is *pointed*.
3. As speed along the heading rises, the velocity vector rotates toward the heading:
   **β shrinks**. The machine "straightens out" — without the rider doing anything but
   holding throttle.
4. Lift the throttle instead and the track's full lateral budget returns at once, the rear
   hooks up, and the machine snaps straight (or, on ice, snaps past straight).
Confidence: **DERIVED**. The premises (friction circle; thrust along heading; the
front/rear capacity split of R1-16) are each LITERATURE or standard vehicle dynamics; the
sequence is assembled here.
**Killing mutation:** if the kernel's track force is not budget-limited — if longitudinal
thrust and lateral resistance are computed independently rather than sharing one friction
capacity — step 2 cannot happen, the drift will not sustain under throttle, and Chad's
signed feel is coming from something else. **This is the highest-value thing for another
strand to check in `sim/sled.cpp`.**

### R1-20 Inside-ski lift is a chassis-roll symptom, NOT the onset of a rollover
"Overdoing it can result in more bite from the outside ski, which can cause **inside ski
lift**" (Snow Goer, front-end handling). Rider diagnosis of the same: "Most people think
cranking up the front ski pressure will fix this problem. Makes it worse because instead
of the proper sag, **the outside is too stiff and that causes the body to roll = inside ski
lift**."
Confidence: **LITERATURE** (trade press + rider forum, consistent).
**Why it matters:** in reality a sled corners with the inside ski **in the air** routinely
and does **not** go over — it runs on one ski and the track, and slides. A kernel that
treats one-ski contact as a pre-roll state is modelling a rollover that real riders do not
experience. This is a strong candidate for Chad's "**constant rolling**" complaint (§0:
*"Don't lose the feel, except the constant rolling"*).
**Killing mutation:** if the kernel's per-ski patches already allow a stable one-ski carve
with the second patch unloaded, this is not the sim's problem and the finding is
descriptive only.

### R1-21 Ski pressure is set by weight transfer, and throttle unloads the skis
"**Shortening the limiter strap pulls down the front torque arm and weights the front end
more. Lengthening the limiter strap makes your sled into more of a wheelie monster while
unweighting the skis**" (Snow Goer). Setup guidance: "you can adjust the coupler blocks so
that the system stops sooner during the transfer, with **more load left on the skis to
keep them down as you apply the throttle**"; and on carbide sizing, "the higher the number
[4, 6 or 8 in], the more positive the front end will be on hard-pack trails". Riders set
limiter straps "so the skis come off the ground only at full throttle" to keep steering.
Confidence: **LITERATURE** (trade press; consistent across three independent how-tos).

**DERIVED consequence, with numbers:** the CG sits **0.86 m ahead of the track patch** and
**0.52 m behind the skis** (kernel params), so the static ski share of weight is
0.52 / 1.38 = **37.7%**. Thrust acts at the track, near running-surface height;
longitudinal acceleration `a` transfers `m·a·h / L` off the front, with h = 0.564 and
L = 1.38: at the **measured full-throttle 0.70 g** (R1-9), transfer = 0.70 × 0.564 / 1.38
= **28.6% of total weight** off the skis. Against a 37.7% static share, **full throttle
removes ~76% of the ski load** — the skis are nearly weightless and steering nearly
vanishes. At quarter throttle (0.27 g) the transfer is 11.0%, i.e. ~29% of the ski load:
the front still steers.
**Killing mutation:** the same arithmetic with a 0.45 m CG gives 22.8% at full throttle
(61% of ski load) — the qualitative conclusion survives but the "nearly weightless"
language does not. And if the kernel applies thrust at the CG rather than at the track
patch, no transfer occurs at all and the effect is simply absent.

---

## 6. Drivetrain, speeds, and airtime

### R1-22 CVT engine braking: near-zero, and the measurement proves it
- Two-strokes "normally lack the compression braking effect of four-stroke engines when
  the throttle is shut off"; premix two-stroke engine braking "can be extremely harmful ...
  because cylinder and piston lubricant is delivered to each cylinder mixed with fuel";
  "many old two-stroke cars ... had a **freewheel device** on the transmission to make
  engine braking optional" (Wikipedia, *Engine braking*, verbatim).
- The CVT primary engages by centrifugal weights at a threshold RPM — "for stock full-size
  snowmobiles, the engagement is usually around **3800–4000 RPM**"; "at idle, the belt is
  at the bottom of the sheaves on the primary clutch", i.e. **below engagement there is no
  driveline connection at all**.
- **The measurement that settles it:** in the SAE roll-down tests there was **"no
  performance variation between power-on and power-off conditions"** (R1-9).
Confidence: **MEASURED** (the roll-down equivalence), **LITERATURE** (the rest).
**Read:** coast-down deceleration is **track-and-snow drag, not engine braking**. A sled
that closes the throttle keeps going; the brake is the only real retarder, and it delivers
0.32–0.42 g.
**Killing mutation:** a roll-down dataset showing a power-on/power-off split would restore
engine braking as a real term.

### R1-23 Trail speeds: the honest operating band
- "Typical trail riders run **30–40 mph** [48–64 km/h] with occasional faster bursts when
  trails are wide and straight"; one tracked rider's "actual average for engine-on time was
  **45 mph**" while the speedometer "usually read around 60–80+".
- Regulatory: Ontario/Quebec trail limits **70 km/h**, Saskatchewan **80 km/h**; "80% of
  snowmobilers use their snowmobiles on trails with both skis firmly on the ground, not
  exceeding the regulatory speed limit of 70 km/h".
- And speedometers over-read "often by more than 10 km/h" (R1-9) — so rider *reports* of
  speed run high.
Confidence: **LITERATURE** (rider surveys, regulation); **MEASURED** (the speedometer bias).
**Killing mutation:** a source placing typical trail riding above 100 km/h would break the
band.

### R1-24 Landing energy against 8 in of travel — DERIVED, with the assumption stated
With **0.203 m (8 in)** of usable travel at each end (R1-4), modelling the landing as a
constant average deceleration of **n** g over the full stroke, the equivalent free-fall
drop absorbed before bottoming is `h = n × stroke`:

| average landing g | drop absorbed | impact vertical speed |
|---|---|---|
| 2 g | 0.41 m | 2.8 m/s |
| 3 g | 0.61 m | 3.5 m/s |
| 5 g | 1.02 m | 4.5 m/s |

Confidence: **DERIVED** (energy balance `m g h = n m g s`), with the g-level itself a
**GUESS** in the 2–5 range — no measured snowmobile landing g was found (see §7).
**Read:** a 1990s trail sled bottoms on the equivalent of a **0.4–1.0 m** drop. A bank
launch of the kind WINTER_LAW §2.4c describes ("Hitting one at speed sends the snowmachine
airborne") will routinely exceed that — **bottoming is the normal case, not the failure
case**, which is why rider guidance is "keep your knees flexed so you can absorb better"
and why compression damping exists to "keep the machine from bottoming out during the
travel of the shock".
**Killing mutation:** an instrumented snowmobile landing dataset. If real average landing
decelerations are 8–10 g, the absorbed drop doubles and the "bottoming is normal" reading
weakens.

### R1-25 Recovering a tip: leverage, not lifting — and the snow has to be prepared
Rider practice, consistently: "Use the leverage that you can get by **pulling on the
handlebars and standing on the running board**"; "**Grab the opposite ski tip and roll it
over — leverage is amazing**"; "If you're in deep snow you want to **clear the loose snow
from under the sled before trying to tip it upright. You don't want to pull the sled up
onto the snow, you want the sled to drop into place**"; and "roll it up & kick/tramp down
under the track, then roll it up the other way & do the same". Safety: kill switch or
tether first.
Confidence: **LITERATURE** (rider forums, consistent across several independent threads).
**Why it matters:** the real recovery is **slow, manual, and snow-dependent** — it needs a
dismounted rider and packed snow, and in deep powder it can take minutes. Chad's ask is
for the *machine* to land on its skis more often by chance (§0: *"allow me to land on my
skis more often after a roll ... But not every time"*; §0b: *"slef righting by chance
more"*), which is a different mechanism: **a sled that is still moving when it goes over
can slide back down onto its skis on a slope**. The manual recovery above is what happens
when it does not — and WINTER_LAW §2.4c.1 already rules that case ("the rider falls off
and needs to run to the machine and get back on it"), so the failed-recovery branch is
already specified and is not a dice roll.
**Killing mutation:** a source showing riders routinely right a sled from the seat without
dismounting would make recovery cheap and change the distribution Chad is asking for.

---

## 7. UNVERIFIED — named, not hidden

1. **The lateral-acceleration table from *Snowmobile Cornering and Acceleration Data from
   On-Snow Testing* (2015).** The radii (6.1 / 10.7 / 19.8 m) and the abstract are in hand;
   **the actual cornering g values are paywalled and were NOT obtained.** Every cornering-g
   number in this document is therefore DERIVED from winter-surface friction literature,
   not from snowmobile cornering measurement. This is the single biggest hole in the strand.
2. **The 1991 Polaris factory 486 lb figure for the base Indy 650.** Cited in `sim/sled.h`'s
   own comment; **not independently re-sourced here.** The 1994 Indy Trail 438 lb figure
   corroborates the order of magnitude only.
3. **MDPI sideslip/rollover friction bands (0.10–0.20 vs 0.21–0.35).** Abstract-level only —
   the full paper returned HTTP 403. The vehicle class is buses/trucks, not snowmobiles.
4. **Snowmobile injury rollover percentages (33%, 55%).** Abstract-level only; the two
   figures come from different series with different denominators. Do not add them.
5. **Measured snowmobile landing decelerations.** None found. R1-24's 2–5 g band is a
   labelled GUESS.
6. **Forum-sourced items** (inside-ski-lift diagnosis, carbide hooking causing tip-overs,
   tip-recovery technique, clutch engagement RPM) are **rider anecdote**, not measurement.
   Several sources (dootalk, snowmobile.com) now sit behind a paywall proxy (HTTP 402/403)
   and were read only through search-result excerpts.
7. **No kernel behaviour was measured by this strand.** Every comparison to `sim/sled.h`
   quotes numbers from that file's comments and from the charter's SEALED list. What the
   running kernel actually produces is another strand's job.
8. **Trail-speed figures** are rider-reported and regulatory, not instrumented — and the one
   instrumented fact nearby is that snowmobile speedometers over-read by >10 km/h.
9. **KLIM sidehilling article** returned HTTP 404 on direct fetch; its lines are quoted from
   the search-result excerpt of that page.

---

## 8. Sources

- Snow Goer, *The Math Behind Ski Stance* — https://snowgoer.com/latest-news/the-math-behind-ski-stance/33738/
- Snow Goer, *Snow Goer 1973-74: The Shakeout Continues* (tilt-table overturn angles) — https://snowgoer.com/snowmobile-features/snow-goer-1973-74-the-shakeout-continues/24247/
- Snow Goer, *How To Improve Snowmobile Front-End Handling* — https://snowgoer.com/news-info/how-to-improve-snowmobile-front-end-handling/32951/
- Snow Goer, *How To: Adjusting Snowmobile Suspension For Sharper Handling, Better Ride* — https://snowgoer.com/snowmobile-tech-tips/how-to/how-to-set-up-your-snowmobiles-suspension-for-sharper-handling-and-a-better-ride/2447/
- snowmobile.com, *1991 Polaris Indy 650 RXL EFI* (stance 38 in, "1.5 inches greater than the standard 650") — https://www.snowmobile.com/manufacturers/polaris/1991-polaris-indy-650-rxl-efi-678.html
- Gone Outdoors, *1994 Polaris Indy Trail Specs* (438 lb dry, 40 in ski centre, 15x121, 8 in / 8 in travel) — https://goneoutdoors.com/1994-polaris-indy-trail-specs-7645030.html
- SAE 2011-01-0287, *Acceleration and Braking Performance of Snowmobiles on Groomed/Packed Snow* — https://www.sae.org/publications/technical-papers/content/2011-01-0287/
- Canadian Underwriter, *Snowmobile Acceleration and Braking Performance* (0.27–0.70 g accel; 0.32–0.42 g braking; roll-down power on/off equivalence; speedometer over-read) — https://www.canadianunderwriter.ca/features/cc-snowmobile-acceleration-and-braking-performance/
- *Snowmobile Cornering and Acceleration Data from On-Snow Testing* (2015; radii 20/35/65 ft; trenching and snow-mass momentum exchange discussed) — https://www.researchgate.net/publication/283877747_Snowmobile_Cornering_and_Acceleration_Data_from_On-Snow_Testing
- SAE 2006-01-1627, *Tire Cornering Force Test Method for Winter Surfaces* (ice peaks then falls; packed snow 90% by 7–10° slip; fresh snow keeps climbing) — https://saemobilus.sae.org/papers/tire-cornering-force-test-method-winter-surfaces-2006-01-1627
- *Lateral Coefficient of Friction for Characterizing Winter Road Conditions*, Can. J. Civ. Eng. (μ 0.18–0.28; scraped ice 0.2–0.4 g lateral) — https://cdnsciencepub.com/doi/10.1139/cjce-2015-0222
- MDPI *Sustainability* 16(2) 888, *Influence of Snowy and Icy Weather on Vehicle Sideslip and Rollover* — https://www.mdpi.com/2071-1050/16/2/888
- US Patent 3,974,890, *Snowmobile suspension* (background: the flat track drives the sled straight in corners) — https://patents.google.com/patent/US3974890
- snowmobile-ed.com, *Turning* (official rider curriculum: lean into turns; weight forward loads the inside ski) — https://www.snowmobile-ed.com/indiana/studyGuide/Turning/501016_90577/
- Intrepid Snowmobiler, *Thinking Like Pros* — https://intrepidsnowmobiler.com/thinking-like-pros/
- NH Snowmobile Association, *Expert Snowmobile Tips and Techniques* (counter-steer; deep-snow outside-running-board weighting) — https://slednh.com/expert-snowmobile-tips-and-techniques-for-beginners/
- Finntrail, *Core Techniques Every Snowmobiler Should Master* — https://finntrail.com/blog/core-techniques-every-snowmobiler-should-master/
- KLIM / SnoWest (Matt Entz), *The Basics of Sidehilling* — https://www.klim.com/news/index.ssp?postID=473
- AMSOIL, *How to Sidehill a Snowmobile* — https://blog.amsoil.com/how-to-sidehill-a-snowmobile/
- Ski-Doo (BRP), *How to Choose the Right Snowmobile Ski Runners* — https://ski-doo.brp.com/us/en/owners/snowmobile-tips/snowmobile-ski-runners.html
- Dennis Kirk, *Choosing the Right Carbides* — https://www.denniskirk.com/blog/2021/10/20/choose-the-right-carbides/
- Bergstrom Skegs, *Choosing the Best Carbide / Wear Bar* — https://bergstromskegs.net/carbide-selection-guide/
- Wikipedia, *Engine braking* (two-stroke lacks compression braking; freewheel devices) — https://en.wikipedia.org/wiki/Engine_braking
- PowerSportsGuide, *How Does a Snowmobile Clutch Work?* (engagement ~3800–4000 rpm; belt at sheave bottom at idle) — https://powersportsguide.com/how-does-a-snowmobile-clutch-work/
- Sledder Mag, *The Art of Unstuck* — https://sleddermag.com/art-unstuck/ ; dootalk *Sled roll over — best way to upright?* ; snowmobileforum *Need help uprighting snowmobile after tipping over*
- *Risk factors and patterns of injury in snowmobile crashes*, Wilderness & Environmental Medicine 1999 — https://journals.sagepub.com/doi/pdf/10.1580/1080-6032(1999)010[0226:RFAPOI]2.3.CO;2
- Intrepid Snowmobiler, *Ontario Snowmobile Speed Limit Review* — https://intrepidsnowmobiler.com/ontario-snowmobile-speed-limit/

---

## 9. RESUMED-RUN ADDENDUM (2026-09-18) — verification log, one correction, six new claims

The first attempt at this strand was paused at 23:42 mid-run. This section is the second
pass: it (a) verifies the first pass's quotes and inputs against the actual files,
(b) corrects one number that the first pass got from a source comment rather than from the
live parameter, and (c) adds the claims the first pass left as gaps.

### 9.0 Verification log — what was re-checked, and against what

| checked | source of truth | result |
|---|---|---|
| Chad §0 ruling text | `D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0 | **VERBATIM MATCH** |
| Chad §0b superseding text | same file, §0b | **VERBATIM MATCH** (this doc quotes the second half of §0b; §0b opens with "no I dont like thge idea in the handoff at all! It will ruin the feel to have a governor. Make it less honest but dont ruin it. Get a fable consult and the measure of it shall be if my intent is heard." — see 9.1 below, it matters) |
| WINTER_LAW §2.4c.1 text | `D:/flight_sim2/Game_loop_idea/WINTER_LAW.md` §2.4c.1 (line 393) | **VERBATIM MATCH** (the law's copy is unbolded prose; the emphasis in §0 of this document is this author's, not Chad's) |
| `mass_kg`, `cg_height_m`, `stance_m`, `ski_fwd_m`, `track_aft_m`, `ski_width_m`, `track_width_m`, `rider_mass_kg`, `lean_lat_seated_m` | `sim/sled.h:543–786` (read-only) | **ALL MATCH** the values R1-6/R1-8/R1-21 used |
| `track_rail_half_m` | `sim/sled.h:569` | **MISMATCH — see R1-26** |
| SAE 2015 cornering lateral-g table | researchgate / SAE | **STILL NOT OBTAINED.** Second attempt: direct fetch returns **HTTP 403**; two further searches returned the same abstract text and no values. §7 item 1 stands, now twice-confirmed. |
| measured snowmobile landing decelerations | web | **STILL NONE FOUND.** A second targeted search returned only active-suspension patent and marketing material (Smart-Shox, DYNAMIX) with no g figures. §7 item 5 stands. |

**Location note for other strands:** `Game_loop_idea/` does **not** exist inside the audit
worktree `D:/seads_sandboxes/sled-audit`. The charter and the law live at
`D:/flight_sim2/Game_loop_idea/…`. Anything in this document that cites them was read
there.

### 9.1 A caution about §0b that the first pass did not state

§0b's opening line is *"no I dont like thge idea in the handoff at all! It will ruin the
feel to have a governor. **Make it less honest but dont ruin it.**"* — and the handoff's
own decode of it reads **"RC-6 IS DEAD AS WRITTEN. The honest-mechanism law is RELAXED for
the sled: 'less honest but don't ruin it' is an explicit licence for arcade-lawful terms
(e.g. a contact-gated roll-stability moment) that a strict sim would not add."**

**Therefore: nothing in this strand is an instruction.** Every finding below is *what the
real machine does*. Chad has explicitly ruled that the kernel is allowed to diverge from
it. A finding that the kernel departs from reality is, on its own, **not a defect** — it
is only a defect if it also costs one of Chad's named feels ("sliding banging, punchy,
jumps", "drift around corners and then with throttle, straighten out") or produces the one
thing he named as broken ("the constant rolling"). Any strand converging on this one must
carry that distinction or it will file reality as a bug.

### R1-26 CORRECTION to R1-6: the kernel's rail half is **0.19 m, not 0.14 m**

R1-6's table row 1 used **0.14 m** for the track's rail half-width. That number is from the
*prose comment* in `sim/sled.h:558–560` describing the **real machine** — "a real Indy's
slide rails sit ~11 in apart (~0.14 m half)". The **live parameter two lines later is
different**:

```
sim/sled.h:569:    double track_rail_half_m = 0.19;
```

and 0.19 m is exactly `track_width_m / 2` (0.38 / 2). So the kernel does **not** model the
rails at the real rail spacing; it applies the split normal reaction at the track's **full
outer width**. `grep` over `sim/` and `config/` finds the field used only at
`sim/sled.cpp:78` and no `[sled_comfort]`/`[snowpack]` key overrides it, so **0.19 is what
runs**.

Corrected table (same arithmetic, same CG height 0.564 m, all four rows recomputed):

| tipping line | lateral arm a | SSF = a/h | static tip angle | which machine |
|---|---|---|---|---|
| ski centreline 0.4635 → rail 0.14 | 0.2550 m | 0.452 g | 24.33° | **real Indy** |
| ski outer 0.531 → rail 0.14 | 0.2765 m | 0.490 g | 26.11° | **real Indy** |
| ski centreline 0.4635 → rail **0.19** | 0.2875 m | **0.510 g** | **27.01°** | **THE KERNEL** |
| ski outer 0.531 → rail **0.19** | 0.3092 m | **0.548 g** | **28.73°** | **THE KERNEL** |

Confidence: **MEASURED** (the parameter is read from the file) + **DERIVED** (the arithmetic).
**What it changes:** the kernel is **+12.7%** more roll-resistant than the real machine's
rail geometry (0.510 vs 0.452 g), and its tip angle is **2.7° later**. The header comment
predicted the sign of exactly this — "*it errs toward MORE stability than the real machine
has*" — but that comment is talking about a *different* open defect (the roll arm being
fixed at the mount). This is a second, independent stability surplus, and it is the larger
one.
**What it does NOT change:** R1-10's central finding gets *stronger*, not weaker. The
surface supplies 0.2–0.42 g; the kernel now demands 0.51–0.55 g to tip. The margin widens
from 0.03 g to 0.09 g.
**Killing mutation:** setting `track_rail_half_m = 0.14` to match the file's own stated
real-machine value. That single edit removes 12.7% of the machine's roll margin and moves
the static tip from 27.0° to 24.3° — which, against WINTER_LAW §2.4c.1's banked approach,
is the difference between rolling on a 25° bank and not. **It is also, in isolation, a
change in the direction Chad ruled AGAINST** ("Just make it more stable"), which is why
this is filed as a *finding about where the stability comes from*, not a repair.
**For the converging strand:** if the kernel currently feels roll-resistant and somebody
proposes to "fix" the rail width to be honest, this is the number they will be moving, and
it moves against the ruling.

### R1-27 The tripped/untripped split and SSF are the **industry's own** categories

The first pass built §2 (SSF) and §4 (the trip) from first principles. Both are the
standard regulatory framing, verbatim from NHTSA/TRB:
- "A rollover that occurs as a result of forces on the tire created by a **mechanical
  obstacle, such as a curb or other surface irregularity (e.g., a furrow plowed during an
  off-road maneuver)**, is described as **tripped**. In contrast, a rollover is described
  as **untripped** if the vehicle rolled solely as a result of the lateral forces created
  at a smooth tire–road interface."
- "**Static Stability Factor (SSF) is the ratio of one half the track width to the center
  of gravity height**" — and NHTSA "chose SSF as the basis of NCAP ratings because it
  represents the first order factors that determine vehicle rollover resistance **in the
  vast majority of rollovers which are tripped** by impacts with curbs, soft soil, pot
  holes, guard rails, etc."
Confidence: **LITERATURE** (regulatory). Sources: NHTSA rollover-resistance NPRM; TRB
Special Report 265.
**Why it matters:** note the parenthetical — "**a furrow plowed during an off-road
manoeuvre**" is the regulator's own example of a trip, and it is *literally* R1-13's
trenching case. The framing this strand used is not an analogy borrowed from cars; it is
the same physics the same way, and a snowmobile's own trip list (bank, berm, hooked
carbide, trench) is that list.
**Killing mutation:** NHTSA's SSF uses **half track width**, which for a symmetric
four-contact vehicle is the correct arm. Applying that formula naively to a snowmobile
gives 0.4635/0.564 = **0.822 g**, which is wrong by ~80% (R1-6's note). Anyone who quotes
"SSF" for this machine without the trapezoid correction of R1-5 has imported the metric and
dropped the geometry.

### R1-28 The front/rear lateral balance is a **deliberately tuned ratio** in the real sport — and both failure modes are named

R1-16 derived that the yaw balance flips with surface. The trade practice goes further: on
a real trail sled the balance is a **ratio between two purchasable quantities** — carbide
length at the front, stud count at the rear — and riders tune it explicitly.
- "If a bunch of traction has been added to the rear end of a snowmobile via studs, it
  affects the balance of that snowmobile's traction package and **you'll likely need to use
  more aggressive runners up front to match**."
- Too much rear: "You will need more carbide on the skis to balance the additional traction
  the studs give the track **or you will understeer or 'push' in the corners**";
  "installation of an **excessive number of studs will cause the snowmobile to exceed the
  steering capabilities so that it will proceed straight when the operator intends a turn,
  a condition known as understeer**."
- Too much front: "running **8 inches of carbide on a snowmobile with an unstudded track**
  will make the front end more positive, but it'll steer heavily and will likely have you
  **tugging around the rear end in icy turns**"; and "when the length of the carbide is too
  long, the snowmobile skis will tend to dig in more than it should while cornering, which
  can cause **the back end of the sled to break free and come around too quickly**."
- The calibration points, for scale: "for the average trail rider on a modern 121″–129″
  track sled with **up to 96 studs and 1.0″ lugs, a sharp set of 4[″] carbides** offers the
  best balance"; "a **6″** carbide is a solid match for … 121″ chassis equipped with **more
  than 96 studs**"; and the coarser rule "4 inch is for non studded track; 6 inch for
  lightly studded track; and 8 inch for a heavily studded track."
- The darting failure is separate and also length-driven: "**too long carbides can cause the
  skis to dart on ice** (when the skis try to turn by themselves)".
Confidence: **LITERATURE** (four independent trade/retail sources agreeing on the direction
and roughly on the numbers). Sources: Snow Goer front-end handling; Dennis Kirk *Choosing
the Right Carbides*; Bergstrom Skegs selection guide; Woody's stud installation instructions.
**Why it matters against Chad's words:** his signed feel is *"drift around corners and then
with throttle, straighten out"* — that is the **front-heavy** end of this ratio
(rear breaks free first). The opposite end of the same ratio is "push", which he has never
asked for. So the machine Chad has described is one with **deliberately excess front
bite relative to rear**, and the real-world recipe for it is a long carbide and few or no
studs — which is also, exactly, an unstudded 1990s trail sled.
**The audit-usable shape:** the sim does not need a stud model. It needs **one ratio** —
front lateral capacity ÷ rear lateral capacity — and that ratio should (i) exceed 1 on
hardpack to give the drift, (ii) fall below 1 in deep snow to give the push (R1-16), and
(iii) be a named dial, which is the standing RC-8 requirement in the handoff ("EVERY
MECHANISM IS A DIAL").
**Killing mutation:** if a kernel strand finds the ski and track lateral forces computed
from a single shared coefficient, this ratio is structurally pinned at one value and
neither Chad's drift nor the powder push can be tuned — the finding becomes a change
request. That is the same test R1-16 named, arrived at from the parts catalogue instead of
from tyre data.

### R1-29 Inside-ski lift happens at ~**0.31 g / φ**, well below the tip — and φ (roll-stiffness share) is the parameter that decides whether the sim ever shows it

R1-20 claimed qualitatively that inside-ski lift is a chassis-roll symptom, not pre-roll.
Here is the arithmetic.

Static ski share of weight = `track_aft_m / (ski_fwd_m + track_aft_m)` = 0.52 / 1.38 =
**37.7%**, so each ski carries **18.85%** of W. Let **φ** be the fraction of the total roll
moment `W·a·h` reacted at the **front** (the rest going to the rails). Lateral transfer
across the front is `φ·W·a·h / stance`. The inside ski lifts when that equals its static
share:

`a_lift = 0.1885 × 0.927 / (0.564 × φ) = 0.3098 / φ  [g]`

| front roll-stiffness share φ | inside ski lifts at | is that reachable on packed snow (≤0.42 g)? | vs kernel tip 0.510 g |
|---|---|---|---|
| 1.00 (all roll at the front) | **0.31 g** | **yes, routinely** | far below |
| 0.80 | 0.39 g | **yes, near the limit** | below |
| 0.74 | 0.42 g | **exactly at the slide limit** | below |
| 0.61 | 0.51 g | no | **equals the tip** |
| 0.50 | 0.62 g | no | above the tip |

Confidence: **DERIVED** (rigid-body load transfer on the kernel's own signed geometry;
assumes the two skis share the front's load equally when level and that the rails react the
remainder).
**Read it:** there is a **threshold at φ ≈ 0.61**. Above it, the inside ski lifts *before*
the machine either slides or tips, and one-ski cornering is a normal, frequent, harmless
trail event — which is what real riders report (R1-20, and the rider fix "get your butt off
the seat and really lean in", below). Below it, the inside ski never leaves the snow in
any manoeuvre the surface can support, and the sim will look flat and dead in corners.
**And the real machine's own diagnosis is a φ argument.** Riders: "suspension settings can
be **too stiff (which will pick the inside ski) or too soft (which will roll right over and
pick the inside ski)**"; "Removing preload off the **IFS (ski shocks)** can help prevent
inside ski lift by widening the stance and softening the ride"; and the Snow Goer line
R1-20 already quoted, "the outside is too stiff and that causes the body to roll = inside
ski lift". Stiffening the **front** raises φ and lifts the ski earlier — exactly the sign
this arithmetic predicts.
**Why it matters against Chad's words:** *"she's too unsteady … except the constant
rolling."* If the kernel treats a lifted inside ski as a roll-onset state rather than as a
normal carve, a φ anywhere above 0.61 will manufacture "constant rolling" out of an event
that in reality costs the rider nothing. **This is the most testable bridge in the strand
between the real machine and his complaint.**
**Killing mutation:** three, any one of which kills it — (1) if the kernel's front and rear
patches have no independent roll stiffness (φ undefined / structurally 0.5), lift never
occurs and R1-29 is unmodellable; (2) if the skis are modelled as one axle-centre patch
rather than two independent patches, there is no "inside ski" to lift at all (WINTER_LAW
§2.4c.1 requires two, so this should fail, but it must be checked, not assumed); (3) a
`track_aft_m` of 0.86 instead of 0.52 would put the static ski share at 50% and move
`a_lift` to 0.41/φ, shifting the threshold to φ ≈ 0.80 and making lift much rarer.

### R1-30 Powder counter-steer is a **rudder** effect, and the mechanism is specific

R1-18 established that counter-steering on a snowmobile is not gyroscopic. The deep-snow
mechanism, from rider sources, is more precise than "unloading":
- "When the snow is deep, **turn your skis opposite of the direction you're turning**. …
  the ski that is in the snow needs to counter-steer, as **the part behind the spindle is
  acting as a rudder, pulling in the opposite direction the bars are pointing**."
- And the hardpack version stays the unloading one: "For a left turn, you should counter
  steer to the right. Counter steering **removes the support of the left ski and carves out
  snow** to let the machine fall left or inside the turn."
Confidence: **LITERATURE** (rider forums and technique press, two distinct mechanisms
consistently described for two distinct surfaces).
**Why it matters:** a **buried** ski is a submerged hydrofoil with area both ahead of and
behind its steering pivot; the after-body dominates and the yaw moment **reverses sign**
relative to the shallow case. That is a genuine sign flip in the ski's yaw contribution as
a function of **burial depth**, not of steering angle — and burial depth is something the
kernel already has (`drive_surface = terrain + depth`, per WINTER_LAW §2.4c.1's per-ski
sampling requirement).
**Killing mutation:** if ski yaw moment in the kernel is a monotone function of steer angle
with no depth term, this behaviour is absent and the finding is a gap report. If it *is*
depth-dependent but monotone in the same sign, the powder handling will feel like trail
handling with more drag — which is the commonest way a snow sim gets powder wrong.

### R1-31 Trenching gives the machine a **positive attack angle**, and the real fix trades travel for float

R1-13 named trenching as a trip. The mechanism riders describe is a steady-state attitude,
not just a wall to catch on:
- "**When power is on, the weight of the machine is transferred to the track and it is
  digging a trench pushing the rest up so the sled is at a positive angle in relation to
  the terrain.**"
- The setup remedy is explicitly a trade: "raising the suspension to the top hole will
  **decrease suspension travel slightly but will allow it not to trench nearly as bad**
  because it lets your suspension get on top of the snow."
- And the throttle law inverts against the trail case: "The main key is **using the power so
  you float and not dig in**" — where on hardpack more throttle is what *sustains* a drift
  (R1-19), in powder more throttle is what *buries* you.
Confidence: **LITERATURE** (rider forums, consistent; no instrumented measurement found).
**Why it matters against WINTER_LAW:** §3.5 of the law already rules the stuck end-state
("drag rises superlinearly *and* the packed track can no longer develop slip, so the thrust
that would save you is gone precisely when you need it … it is a state you have to get off
the machine and dig out of"). R1-31 supplies the **intermediate** state the law does not
name: before stuck, the machine sits nose-up in its own trench, and that attitude is itself
the thing that unloads the skis and takes the steering away. It is the same
weight-transfer arithmetic as R1-21, driven by trench geometry instead of by thrust.
**Killing mutation:** if the kernel's depth field is static under the machine (WINTER_LAW
§3.6 rules **no persistence** — "tracks, breakage and snow disturbance do NOT persist for
now"), the sled cannot dig its own trench and this state is unreachable *by construction*.
That is a ruled limitation, not a defect — but it means the trenching trip of R1-13 can
only arrive from pre-existing terrain, never from the rider's own throttle.

### R1-32 Real "body english" reaches **further than the kernel's standing reach box**

R1-8 valued the kernel's reach box (0.15 m seated / 0.35 m standing). Riders describe a
third, larger position — hang-off:
- "you want to **lean out and over the inside ski** while keeping the outside boot under the
  toe hold and **'pulling up' on the outside toe hold** to help with weight transfer to the
  inside";
- "You need to **get your butt off the seat and really lean in** to the corners, **hooking
  your boots in the foot wells** and getting over as far as you can."
Confidence: **LITERATURE** for the technique; the *distance* it reaches is a **GUESS**.
**GUESS, labelled:** a rider hanging off with the outside foot hooked reaches a lateral CG
offset of roughly **0.45 m** (torso mass moved outboard of the running board, which sits
~0.30 m from centreline). At the kernel's 26.44% rider mass fraction that is 0.119 m of
system CG shift, giving, against the kernel's 0.2875 m arm: **SSF 0.510 → 0.721 g, +41%**,
versus **+32%** for the standing 0.35 m box and **+14%** for the seated 0.15 m box.
**Why it matters against Chad's words:** §0 — *"allow the balance of body mechanism to
ENHANCE ability, i.e. tighten a turn"*, and §0b — *"Leaning shall enhance the ride."* The
enhancement ladder in the real sport has **three rungs** (seated, standing, hung off),
and the kernel models two. The third is the one riders actually use in a hard corner.
**Killing mutation:** this claim is a GUESS on 0.45 m and dies to any measurement. At
0.38 m the benefit falls to +35% and is barely distinguishable from the standing rung, at
which point a third rung is not worth building. It also dies if the kernel's lean is a
*rate-limited* reach (`sim/sled.h:780` calls the lean params "reach limits + rates") whose
rate cannot traverse the extra 0.10 m inside a corner's entry — an extra rung you cannot
reach in time is not an enhancement.

### 9.2 What this addendum changes in the strand's bottom line

Nothing in §1–§6 is retracted except R1-6's row 1 attribution (see R1-26). The central
finding **strengthens**: with the kernel's real parameter, the machine demands
**0.510–0.548 g** to tip, the winter surface supplies **0.2–0.42 g**, and so on flat ground
the sim's sled — like the real one, only more so — **slides out before it rolls**. Every
roll it can produce on the flat is either a trip (§4, and the regulator agrees, R1-27) or a
kernel artefact.

The one new mechanism worth another strand's time is **R1-29**: the roll-stiffness share φ
decides whether a lifted inside ski is a routine carve or a rollover cue, the threshold is
sharp (φ ≈ 0.61), and a kernel sitting on the wrong side of it would produce exactly the
symptom Chad named and nothing else in this document explains.

### 9.3 Additional sources (this addendum only)

- NHTSA, *Notice of Proposed Rulemaking — FMVSS Rollover Resistance* (tripped vs untripped definition) — https://www.nhtsa.gov/document/notice-proposed-rulemaking-federal-motor-vehicle-safety-standards-rollover-resistance
- TRB Special Report 265, *An Assessment of NHTSA's Rating System for Rollover Resistance* (SSF = half track width / CG height; SSF chosen because most rollovers are tripped) — https://onlinepubs.trb.org/onlinepubs/sr/sr265.pdf ; https://nap.nationalacademies.org/read/10308/chapter/3
- Snow Goer, *How To Improve Snowmobile Front-End Handling* (stud/carbide balance; push from excess rear traction) — https://snowgoer.com/snowmobile-tech-tips/how-to-improve-snowmobile-front-end-handling/32951/
- Woody's Traction, *Stud Installation Instructions* (excess studs exceed steering capability = understeer) — https://www.woodystraction.com/app/uploads/2019/08/stud-singleply-installation-instructions.pdf
- Dennis Kirk, *Choosing the Right Carbides* (4/6/8 in vs stud count; 96-stud calibration point) — https://www.denniskirk.com/blog/2021/10/20/choose-the-right-carbides/
- Bergstrom Skegs, *Choosing the Best Carbide / Wear Bar* (too-long carbide darts on ice; rear breaks free) — https://bergstromskegs.net/carbide-selection-guide/
- Hardcore Sledder, *Inside Ski Lift Solution* (IFS preload; too stiff picks the ski, too soft rolls over and picks it) — https://www.hardcoresledder.com/threads/inside-ski-lift-solution.1664202/
- dootalk, *cornering technique…confused* (hang-off: butt off the seat, boots hooked in the foot wells, pull up on the outside toe hold) — https://www.dootalk.com/threads/cornering-technique-confused.977721
- dootalk, *Powder riding* (deep-snow counter-steer: the ski behind the spindle acts as a rudder) — https://www.dootalk.com/threads/powder-riding.216285/
- SnoWest, *We Got Trencher — skid set up* (power-on trench gives a positive attack angle; raise the skid to trade travel for float) — https://www.snowest.com/forum/threads/we-got-trencher-skid-set-up.333506/
- Finntrail, *How to Ride a Snowmobile in Deep Snow* (use power so you float and do not dig in) — https://finntrail.com/blog/how-to-ride-a-snowmobile-in-deep-snow-skills-setup-pro-tips/
- Sled Magazine, *To Stud, Or Not to Stud Your Snowmobile Track?* — https://sledmagazine.com/snowmobile-track-choosing-installing-right-studs/
- `sim/sled.h` lines 530–580, 780–802 (read-only, audit worktree `D:/seads_sandboxes/sled-audit`) — the live parameter values
- `D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0, §0b — Chad verbatim
- `D:/flight_sim2/Game_loop_idea/WINTER_LAW.md` §2.4c.1 (line 393), §3.5, §3.6 — Chad verbatim + rulings
