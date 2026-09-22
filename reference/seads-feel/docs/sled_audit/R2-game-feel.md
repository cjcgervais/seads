# R2 — GAME FEEL: how shipped vehicle games handle the arcade/sim trade

Strand R2 of the sled ride audit. Worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. READ-ONLY against main; nothing built, no dial touched, no test run.

**What this strand is for.** The charter (`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md`
§0b) licences arcade-lawful terms — "less honest but dont ruin it". This strand asks
the industry what "arcade-lawful" actually means in machines that shipped, so the
next rung picks a *named, precedented* mechanism rather than inventing one.

**Confidence vocabulary.** MEASURED (I ran it, here, this session) / DERIVED
(arithmetic on a measured number) / LITERATURE (a cited published source; the source
is evidence that someone *said* it, not that it is true) / GUESS (labelled, every time).

**Rule obeyed throughout.** No feel recommendation rests on a harness number alone.
Every recommendation in §6 cites (a) a tape event from Chad's own drives and (b) one
of Chad's words, quoted verbatim with doc and section.

---

> ## ★ RESUMED-RUN NOTICE — READ §10–§14 BEFORE ACTING ON §1–§9
>
> Sections §1–§9 are the first attempt's work (paused 23:42, 2026-09-17). The resumed
> run **independently re-measured them and then went further**. Result:
> * the tape arithmetic in §4 **reproduced exactly** — it is sound (§10.1);
> * two of its *framings* are **overturned** (§10.3, §10.4);
> * six of the ten UNVERIFIED items are now **resolved** (§14);
> * and the strand's headline finding was **not in the first attempt at all** (§11).
>
> **The single thing to carry forward:** the acceptance-bar quotes in §1 are from
> **drive 4, 2026-08-12**. The kernel that ships *today* already contains a
> Chad-specified righting assist at `right_assist_nm = 2400.0` that did not exist
> when he said those words, and measured against his own drives of **2026-09-17** it
> has already delivered the *recovery* half of his ask and left the *frequency* half
> untouched. §1's quotes describe a machine that no longer exists. See §11.

---

## 1. Chad's words — verbatim, with provenance

All from `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` (main tree copy at
`D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md`).

**§0 "Chad's ruling (drive 4, 2026-08-12 — verbatim, this is the acceptance bar)":**

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more
> often after a roll** (even though R works). But not every time — allow it to happen.
> Also I've seen lots of snowmobiles **drift around corners and then with throttle,
> straighten out. The feel is really good. Don't lose the feel**, except the constant
> rolling. **Make it possible to roll but not the rule.** However the game feels like a
> nicely made sim as there is dynamic room and **the balance mechanism works really
> good. Just allow the balance of body mechanism to ENHANCE ability, i.e. tighten a
> turn instead of having to be the necessary condition of not rolling over.** Just
> allow leaning a certain way in a particular condition to be the OPTIMAL weight distro
> for better traversing, say up a hill or around a corner. **Work on the math to achieve
> a balance of fun and accuracy to real physics, just as our airplane ontological
> counterpart does.**"

**§0b "SUPERSEDING RULING (2026-08-12 night — verbatim)":**

> "no I dont like thge idea in the handoff at all! It will ruin the feel to have a
> governor. Make it less honest but dont ruin it. Get a fable consult and the measure
> of it shall be if my intent is heard. Leaning shall enhance the ride an just make it
> more stable and slef righting by chance more. It should be arcadey to a degree so
> that it is fun., SLiding banging, punchy, jumps. Just make it more stable."

> "please build autmatically and make this kernel way better ... I am afraid it might
> ruin something that is reallt good right now but just dont ruin it. Allow for bad
> driving too but keep the benefit there for good riding. DOnt make it impossibly hard,
> there is a batttle going on as well. And the point is this should be fun as well, not
> a true sim. But I do want the finess and all the mechanisms available for tuning, so
> lets stop messing around abnd build something good for real. ... please knock this out
> of the park and make it awesome, keeping the good but making it easier / more fun?"

**§3 "Chad's process ruling — BE PROGRAMMATIC":**

> "We should be more programmatic in the next run."

**One more, from session memory (`sting-rpas-plan`, 2026-09-12), governing the
speed-communication half of this strand:** the R1 motion-blur / tunnel-vision speed FX
was **REVERTED at Chad's word** — recorded reason "looked bad"; lane reset to main
`28e741247`, scrap tag `scrapped/speedfx-r1-20260912`. I did not find the raw quote in
this worktree (`grep -rniE "tunnel.?vision|motion blur|speedfx"` over `*.md`/`*.toml`
returns only `docs/world_build_plan.md:432`, an unrelated line about motion blur
degrading target tracking). **Treated as a paraphrase, not a verbatim quote** — see
§7 UNVERIFIED-3.

---

## 2. Method, and what "MEASURED" means here

Web research: 14 searches and 9 page fetches, all cited inline in §3–§5 and listed in §8.
One primary source downloaded and text-extracted locally
(`Harris_Matthew_VehicleFeelMasterclass.pdf`, GDC 2018, 93 pages, `pdftotext -layout`).

Tape grounding: read-only `awk` passes over Chad's own `.sledtape` files in
`D:/flight_sim2/seads-recon/build-play`. No binary was run, no ctest, no seads.exe.

**Column derivation (so the numbers are auditable, not magic).**
`test/harness/sled_tape.h` defines a T record as: `T`, tick, then the six `SledInputs`
floats (`sim/sled.h:42-58`: throttle, brake, steer, lean_lat, lean_fwd, stand), then the
three cold writes (air_temp_c, snow_hardness, cold_t_ref_c), then the 41 doubles of
`SLEDTAPE_PIN_D` in order, then `surface` and `rolled` as ints. That is 2+6+3+41+2 = **54
fields**; `awk '/^T/{print NF; exit}' sled_tape_56.sledtape` prints **54**, confirming the
layout. Therefore `$47 = ground_speed_ms` (pin item 36), `$49 = rolled_hold_s`,
`$50 = air_s`, `$54 = rolled`. Header: `# dt 0.0083333333333333332 substeps 12` → one T
record = 1/120 s.

> **Killing mutation for every tape number below:** add or remove one field from
> `SLEDTAPE_PIN_D`, or reorder it, and the column indices shift — the speed column
> becomes `thrust_n` or `depth_under_m` and every figure in §4 is wrong. The NF=54 check
> is what pins it; re-run that check before trusting any of this after a kernel change.

---

## 3. The two poles, and what each one costs — the snow machines that shipped

### R2-01 — Sledders is the full-sim pole, and it costs it exactly Chad's complaint

Sledders (Hanki Games, EA Dec 2023, 1.0 Mar 2025) markets itself as a
"Physics-driven snowmobile simulator built for backcountry riding" with "Physics-based
deep snow riding", where "Every tap to throttle, lean, and countersteer matters"
(Steam store page). Reception is strong — 94% of 2,945 English reviews Very Positive at
time of fetch — but the *handling* complaints that reviewers file are Chad's complaints
verbatim in a different accent: a player with real snowmachine experience reported that
"sleds tipped over even on level snow going straight, which never happened in real life",
and that "counter steering in powder isn't always necessary in reality but the game makes
it required"; and, tellingly, that some players "wanted physics-based gameplay combined
with arcade-like fun, which this game doesn't provide" (PC Gamer round-up of player
reaction). Top-rated Steam reviews accept the cost knowingly: "The learning curve is a
steep as some of the mountains but that's a good thing", and "If you ride in real life and
you're looking for a perfect simulation of real backcountry riding, you'll find a million
things to complain about."

**Confidence:** LITERATURE.
**Why it matters:** the closest thing to a direct competitor already occupies the honest-sim
pole and is *praised for the snow, forgiven for the tipping*. SEADS does not win by going
further in that direction, and Chad has already ruled it out ("this should be fun as well,
not a true sim", §0b).
**Killing mutation:** if a re-read of Sledders' top-rated and top negative reviews shows
the tipping complaint is a minority artefact of one reviewer rather than a recurring
theme, this claim collapses to anecdote. I read a fetched summary of the review pages,
not the full corpus — see §7 UNVERIFIED-1.

### R2-02 — the arcade pole fails on a *different* axis: "never fully in control"

Snow Moto Racing Freedom (the only other shipped snowmobile title with reviews) is the
arcade pole, and reviewers describe the failure precisely: controls "feel extremely loose,
with players quickly losing control after the slightest movement, making snowmobiles feel
like lightweight toys that fling themselves around the course"; "sloppy and unpredictable,
especially in corners"; and the summary judgement — "even accounting for snowmobile
difficulty, the overall feel is that players are never fully in control, which isn't ideal
for a racing game" (Nintendo Life / Cubed3 / ZTGD reviews, via search).

**Confidence:** LITERATURE.
**Why it matters:** this is the failure mode a naive "just make it more stable" patch
produces if stability is bought by *decoupling the machine from the ground* (raising mu,
flattening response). Loose ≠ stable. Chad's ask is the opposite: "keeping the good but
making it easier / more fun" (§0b) — the good being contact feel he can read.
**Killing mutation:** a reading of the same reviews showing the "loose" complaint refers to
a controller deadzone / input bug rather than the handling model.

### R2-03 — the shipped snowmobile audience explicitly wants the middle, and no one serves it

Two independent data points converge: the Sledders player quoted above who "wanted
physics-based gameplay combined with arcade-like fun, which this game doesn't provide",
and the top-rated Sledders reviewer's frame of reference — "It is way more realistic than
something like 'Sledstorm'" and "Best snowmobile game since Sledstorm on the PS1" — i.e.
the audience's only two reference points are a 1999 arcade game and a 2025 sim, with
nothing between.

**Confidence:** LITERATURE (two review quotes; a market claim would be GUESS).
**Why it matters:** it is the same middle Chad named — "a balance of fun and accuracy to
real physics" (§0). This is corroboration that the target is coherent and unoccupied, not
a private preference.
**Killing mutation:** finding a shipped, well-reviewed simcade snowmobile title
(2020-2026) that already occupies this middle.

---

## 4. What Chad actually drives — MEASURED from his own tapes

Every figure in this section is from read-only `awk` over `.sledtape` files in
`D:/flight_sim2/seads-recon/build-play`. Nothing was executed.

### R2-04 — his real drive envelope: ~11-16 m/s mean, ~45 m/s peak, 6-10% of time airborne

Six longest tapes (9-22 minutes each), `$47 = ground_speed_ms`, `$50 = air_s`:

| tape | ticks | drive_s | mean V (m/s) | max V (m/s) | %ticks >10 | %>15 | %>20 | % airborne | longest air (s) |
|---|---|---|---|---|---|---|---|---|---|
| 16 | 156074 | 1300.6 | 12.99 | 44.78 | 45.2 | 29.7 | 22.3 | 6.5 | 3.35 |
| 14 | 129566 | 1079.7 | 13.58 | 45.49 | 54.2 | 33.8 | 23.4 | 7.6 | 2.88 |
| 8  | 113662 |  947.2 | 12.81 | 45.67 | 54.8 | 31.8 | 18.8 | 9.5 | 3.57 |
| 1  |  97184 |  809.9 | 11.08 | 44.16 | 42.6 | 22.1 | 14.5 | 6.7 | 2.68 |
| 15 |  76820 |  640.2 | 12.20 | 44.13 | 51.2 | 36.1 | 18.6 | 7.6 | 2.29 |
| 34 |  66134 |  551.1 | 16.06 | 44.82 | 66.6 | 42.2 | 24.5 | 6.7 | 2.68 |

**Confidence:** MEASURED.
**Why it matters:** every "communicate mass and speed" recommendation (§5) has to work in
the 10-25 m/s band (36-90 km/h) where he spends a third to two thirds of his time, not at
the 45 m/s peak. And **6.5-9.5% of his drive is off the ground, with hangs up to 3.6 s** —
air is not an edge case in this game, it is a tenth of it. That directly promotes
"rotation-in-air control" from nice-to-have to a first-class surface.
**Killing mutation:** the NF=54 column check failing (see §2), or `air_s` turning out to be
a *cumulative* counter rather than a per-tick airborne timer — in which case "% airborne"
is "% of ticks after the first jump" and is meaningless. I did not read the `air_s` writer
in `sim/sled.cpp`; see §7 UNVERIFIED-2.

### R2-05 — `rolled` is overwhelmingly a GROUND state, not a mid-air artefact

The charter (§1, "`rolled` readout semantics") flags that `rolled` = `dot(body_up, up) <
cos(75°)` is instantaneous and "fires MID-AIR on legitimate sends and cuts throttle those
ticks", and proposes gating on contact. I measured how much of the problem that actually is,
splitting rolled ticks by `air_s > 0`:

| tape | ticks | rolled ticks | rolled % | rolled **in air** | as % of rolled | rolled **on ground** | as % of all ticks |
|---|---|---|---|---|---|---|---|
| 1  |  97184 | 16304 | 16.78 | 206 | 1.3 | 16098 | 16.56 |
| 8  | 113662 |  7853 |  6.91 | 684 | 8.7 |  7169 |  6.31 |
| 34 |  66134 |  3564 |  5.39 | 310 | 8.7 |  3254 |  4.92 |
| 16 | 156074 |  5384 |  3.45 | 972 | 18.1 |  4412 |  2.83 |

**Confidence:** MEASURED.
**Why it matters:** **81.9% to 98.7% of `rolled` time is spent past 75° while in contact
with the ground.** Contact-gating the readout is still correct housekeeping, but it is a
1-18% fix, not the fix. On tape 1 Chad spent **135.9 s of an 809.9 s drive — 16.8% —
lying past 75°**. That is the arithmetic behind "she's too unsteady" (§0) and behind
"except the constant rolling" (§0).
**Killing mutation:** if `air_s` is a latch that stays >0 after touchdown, the "in air"
column is overcounted and the ground share is even *higher* — the claim survives in
direction but the exact split is wrong. If instead `air_s` is only set on long airs and is
0 during short hops, the in-air share is undercounted; the claim could weaken. Reading the
`air_s` update in `sim/sled.cpp` kills or confirms it.

### R2-06 — a roll episode every 15-22 seconds, and one 53.6 s lie-down

Episode = a maximal run of consecutive ticks with `rolled == 1`:

| tape | drive_s | episodes | mean episode (s) | longest (s) | episodes ≥1 s | ≥5 s | total rolled_s |
|---|---|---|---|---|---|---|---|
| 1  |  809.9 | 37 | 3.67 | **53.6** | 13 | 5 | 135.9 |
| 8  |  947.2 | 76 | 0.86 | 8.3 | 19 | 2 |  65.4 |
| 16 | 1300.6 | 65 | 0.69 | 2.6 | 13 | 0 |  44.9 |
| 14 | 1079.7 | 71 | 0.70 | 7.3 | 15 | 1 |  49.7 |

**Confidence:** MEASURED. **DERIVED:** one episode per 12.5 s (tape 8), 16.6 s (tape 14),
20.0 s (tape 16), 21.9 s (tape 1).
**Why it matters:** this is the single most decision-relevant number in the audit for the
game-feel question, because it separates two very different complaints that both sound
like "too unsteady":
  * **frequency** — an event every ~12-22 s. Compare: this is faster than the between-fault
    cadence any of the reviewed titles tolerate outside a deliberately punishing mode.
  * **recovery cost** — mostly cheap (mean 0.7-0.9 s on tapes 8/14/16) but with a fat tail
    (13-19 episodes per drive last ≥1 s; tape 1 has five ≥5 s and one **53.6 s**).
Chad's two asks map cleanly onto the two: "Make it possible to roll but not the rule" (§0)
is the **frequency** ask; "allow me to land on my skis more often after a roll (even though
R works)" (§0) is the **recovery-cost** ask. §5 shows the industry treats these as two
separate systems with two separate solutions, and the recovery one is far better precedented.
**Killing mutation:** if `rolled` chatters (crosses the 75° threshold repeatedly within one
physical event), the episode count is inflated and the mean duration deflated. The
`rolled_persist_s` / `rolled_grace_s` dials already on `SledComfort` (see
`SLEDTAPE_COMFORT_D` in `test/harness/sled_tape.h`) exist precisely because someone
suspected this. Re-running the episode pass with a 0.25 s debounce would kill or confirm it;
I did not, to stay out of strand D-B's lane.

---

## 5. What shipped games actually do — the mechanisms, named

### R2-07 — assists are TWO layers, not one dial: input-layer vs simulation-layer

Criterion's Matthew Harris (GDC 2018, *Vehicle Feel Masterclass: Balancing Arcade
Accessibility with Simulation Depth*, 16 years of Burnout / Need for Speed / Star Wars
Battlefront II) frames the entire arcade-sim trade as a layered architecture. The slide
text (extracted verbatim from the deck) reads:

> `Input Layer  Simulation`
> `   Assists     Assists`

with the simulation column enumerating honest terms — `Anti Roll Bar`, `Load
Distribution`, `Ackermann Steer`, `Tyre Friction`, `Camber Effects`, `Suspension`,
`Body Aero Drag` — and the vehicle-archetype slides showing the *same* two-layer treatment
applied to wildly different craft (`Fighter Plane: Airfoils / Banked Turns / Prop-Jet
Engine / Body Aero Drag` vs `Starfighter: Simple Anti-Grav / Horizon-Steering / Banked
Turns / Simple Engine Thrust / 'Air' Brake`). The steering-assist slides describe an
input-layer construction: `Drag Cursor Along` → `Calculate Player-Space Angular Velocity`
→ `Feed into Input Layer`.

**Confidence:** LITERATURE (slide text MEASURED from the PDF; the surrounding argument is
the talk's, which I did not hear).
**Why it matters:** this is the precedent that answers Chad's §0b objection. A *governor*
is a simulation-layer intervention that overwrites the player's authority — which is what
he rejected ("It will ruin the feel to have a governor"). An **input-layer** assist shapes
what the player's stick *means* before it reaches an untouched kernel, and is therefore
out-ridable by construction. Criterion shipped both layers, separately labelled, for 16
years across cars and spacecraft. That is the architectural licence for "less honest but
dont ruin it" (§0b) without a governor.
**Killing mutation:** obtaining the talk audio/transcript and finding the two columns mean
something else (e.g. "assists we tried" vs "assists we shipped"). The slide gives the two
labels but not their definitions; I am inferring the definitions from the surrounding
slides. Treat the *architecture* as LITERATURE and the *interpretation* as DERIVED.

### R2-08 — the named arcade stability tools, and the one that is a governor

Livio De La Cruz (Game Developer, "Implementing Racing Games: An intro to different
approaches and their game design trade-offs", 2016-08-12) enumerates the stability toolkit
actually used in shipped racing games, split by approach:

*Physics-based implementations:* anti-roll bars ("artificially compressing suspension on
opposite axles"), lowering centre of gravity, reducing suspension distance, **dynamic
steering angle limits based on speed**, varying acceleration torque by current speed.

*Arcade implementations:* single box collider, circle-based turning, optional drifting,
**"artificial torque application to prevent tipping"**, visual effects faking suspension.

He adds: "the line between the two is definitely a blurry one."

**Confidence:** LITERATURE.
**Why it matters, and the warning:** of these, "artificial torque to prevent tipping" is
**exactly the anti-roll torque from nowhere** that the charter's RC-6 originally banned and
§0b only conditionally relaxed. It is also the one that, applied unconditionally, produces
Snow Moto Racing Freedom's "never fully in control" (R2-02) — because a constant righting
torque decouples attitude from terrain. The other four are *conditioning* terms: they
change how much authority the player has as a function of state, and they are the honest
half of the list. **Speed-scaled steering authority** is the most transferable to a sled
and is not a governor: it does not move the machine, it bounds the command.
**Killing mutation:** finding the same article recommending the artificial-torque approach
as a general solution rather than an arcade-only shortcut, which would weaken the
distinction I am drawing.

### R2-09 — the landing/rotation assist is the standard air-control solution, and it is PRICED

Riders Republic (Ubisoft, 2021) ships landing assist as an explicit difficulty setting.
Per the guides: "Auto is the most forgiving landing mode, meant for beginners or racers,
and essentially prevents you from messing up a spin, although you can still crash if you
try to jump too low to the ground"; "In Manual, you are responsible for landing a trick
successfully, while Auto makes it so you can't get the rotation wrong on landing." The
price: "the downside is that you do not get a points bonus when using Auto, and Riders
Republic events treat it as a handicap of sorts, meaning that on higher difficulties
you'll have to do even more tricks to hit the top of the rankings."

Forza uses the same pricing structure on the ground: turning off traction and stability
control "makes your car naturally less stable, but it gives you greater control, making
drifting easier and **increasing your CR rewards**".

**Confidence:** LITERATURE.
**Why it matters, three ways:**
1. **Shape.** The assist acts *on rotation, at the landing moment* — not as a continuous
   attitude torque. It is a one-shot, condition-gated correction. That is structurally
   identical to what Chad asked for: "allow me to land on my skis more often after a roll
   ... But not every time — allow it to happen" (§0) and "slef righting by chance more"
   (§0b). He did not ask for less rolling *in the air*; he asked for a better *landing
   distribution*.
2. **Limits.** Even on Auto you "can still crash if you try to jump too low to the ground" —
   the assist has a competence envelope and abuse still fails. That is RC-1.
3. **Price.** Both Ubisoft and Turn 10 charge for forgiveness in the scoring currency
   rather than hiding it. SEADS has a battle economy to charge in — see §6-C.
**Killing mutation:** Ubisoft's own help page contradicting the third-party guides on the
points handicap. I could not fetch the official Ubisoft help article (returned empty body);
the handicap claim rests on Twinfinite/Gamepur/GameSpot. See §7 UNVERIFIED-4.

### R2-10 — the rider is a separate body with its own stick: the shipped lean-as-enhancer

MX vs ATV Reflex (Rainbow Studios, 2009) shipped the dual-analog "Reflex" scheme where
"the left analog controls the handlebars while the right analog stick controls the rider's
body movement", letting players "'ride' the motorcycle and other vehicles as they actually
would". Reception credited "the game's successful separation of the rider from the machine
with the rider reflex dual-analog control". The series' later entry MX vs ATV Legends keeps
rider lean as an *enhancer with a cost*: "players can push and hold the right stick forward
to lean the rider's weight over the bars for distance in the air, though leaning too long
results in a nose dive." Trials Fusion works the same way — left stick shifts balance and
bike position, right stick moves the rider; "players need to find the sweet spot in the
analogue range."

**Confidence:** LITERATURE.
**Why it matters:** this is 17 years of shipped precedent for exactly Chad's model —
"the balance mechanism works really good. Just allow the balance of body mechanism to
ENHANCE ability, i.e. tighten a turn instead of having to be the necessary condition of not
rolling over" (§0). SEADS already has the separated rider (`rider_lat_m`, `lean_lat_seated_m
0.15` / `lean_lat_stand_m 0.35` in the tape header). The gap is not the mechanism, it is the
*baseline*: in Reflex and Legends, the rider-neutral machine is stable and lean buys
distance/radius; in SEADS the measured baseline (R2-05, R2-06) is not stable at rider-neutral.
**Killing mutation:** a source showing Reflex's machine is *also* unstable at rider-neutral,
i.e. that the dual-stick scheme was a tax rather than an enhancer there too.

### R2-11 — the law that governs all of it: the player must never feel cheated

Trials HD's reviews isolate the principle better than any design doc: "it is simply hugely
addictive and any faults you make are down to human error with you never feeling cheated."
The series' counterpart to an assist is *friction removal on failure*, not failure
prevention: "The game makes restarting as painless as possible with a simple push of the
'back' button that puts you back at the track's beginning with no load screen", and "this
unobtrusive reloading system is essential to the game's success and your enjoyment of it,
without it the inevitable rate at which you need to reset back to the previous checkpoint
would quickly become tedious."

Riders Republic and Forza reach the same destination from the other side: "Screwing up
outside of a multiplayer event is nearly without consequence thanks to the rewind mechanic";
"controls are pretty forgiving and even the worst crash can be shaken off"; "crashes are
hilarious, not punishing". Forza's rewind "can be used as much or as little as you desire",
and accessibility writers praise Playground for being "completely okay with players breaking
their game".

**Confidence:** LITERATURE.
**Why it matters:** it reframes the whole rung. **The industry's dominant answer to "too
many failures" is not "fewer failures" — it is "cheaper failures, and failures the player
can attribute to themselves."** Chad is already living this: "allow me to land on my skis
more often after a roll (**even though R works**)" (§0) — R works, and he is still
unsatisfied, which means the cost he objects to is not *recoverability* but *interruption*
and *attribution*. Cross-reference R2-06: his recoveries are mostly short (0.7-0.9 s mean)
but there are 13-19 long ones per drive and one 53.6 s. The fat tail is where "cheated"
lives.
**Killing mutation:** a re-read of the Trials reviews showing the "never cheated" line
refers to level design rather than the physics model.

### R2-12 — the arcade licence, stated by a developer in Chad's exact words

Lonely Mountains: Downhill's developers state they "deliberately didn't want to build a
downhill simulation but an arcade game which also downhill/mountain bikers can enjoy", and
that the bike physics and controls "were created to be simple, fun and — although not
necessarily physically correct — feel 'real'."

**Confidence:** LITERATURE.
**Why it matters:** independent developer articulation of §0b's "Make it less honest but
dont ruin it", shipped and well reviewed ("easily the best mountain biking game available",
Hardcore Gamer 4/5). It is the existence proof Chad's ruling needs, and it targets the same
dual audience — enthusiasts *and* players.
**Killing mutation:** the quote being the publisher's marketing rather than the developers'
statement of method.

---

## 6. Mass, speed and feedback — the communication channels

### R2-13 — camera does the mass work, and the shipped techniques are named

Four techniques recur across sources:
* **Spring-arm lag.** "A dynamic spring arm responds to vehicle movements, adding a sense
  of weight and acceleration to the vehicle." Implementation note from the same corpus:
  "the camera lags behind the farther the target, the faster it catches up, that is its
  speed of approach varies with the distance to the object."
* **Speed-coupled FOV.** "A dynamic camera also adjusts the field of view in correlation to
  speed/acceleration/deceleration and gives a much greater sense of speed to driving."
* **Lateral shift in turns.** "When the car enters a turn the camera shifts sideways, making
  it look/feel like the car is actually turning instead of how some games have the car
  basically planted center screen pointing straight."
* **Two shake families.** Criterion's deck lists them as separate systems:
  `Conventional Shake` and `Orbit Shake` — i.e. translational jitter and an orbital
  (rotational, around the subject) shake, tuned independently.

**Confidence:** LITERATURE (shake taxonomy MEASURED from the PDF text).
**Why it matters:** SEADS already landed a chase-camera rung (session memory
`cam-smooth-lane`: sub-tick blend + lagged sled cam, chase cam x0.75, landed main
`ec2e1a93a`), so the spring-arm channel is live and tuned. The two *unused* channels here
are speed-coupled FOV and lateral shift in turns — and the latter is the one that would
sell Chad's canonical drift ("drift around corners and then with throttle, straighten out.
The feel is really good", §0) without touching a single kernel term.
**Killing mutation:** finding that the landed cam rung already implements a turn-side shift,
which would make this a duplicate proposal. I did not read `app/rest_horizon.h` or the cam
code; see §7 UNVERIFIED-5.

### R2-14 — motion blur REDUCES perceived speed; the literature backs Chad's revert

Berényi & Lidestam, "Speed perception affected by field of view: Energy-based versus
rhythm-based processing", *Transportation Research Part F* 65 (2019) 227-241, DOI
10.1016/j.trf.2019.07.016. Reported findings, via the DiVA open-access record and the
ScienceDirect abstract listing: increasing geometric field of view **increases** perceived
velocity; **"A strong setting of motion blur decreases the perceived velocity"** — which the
authors flag as contrasting with earlier subjective studies that found no effect; and narrow
FOV optic flow is perceived as slower than it is, so "when the FoV reduces, visual speed
underestimation tends to increase."

**Confidence:** LITERATURE. The verbatim sentence came through a search-result extract; the
ScienceDirect page returned HTTP 403 and the DiVA PDF text did not extract cleanly, so I
could not confirm it against the paper body. See §7 UNVERIFIED-6.

**Why it matters — this is the strongest single research finding in the strand:** the
reverted R1 speed FX combined *motion blur* (which the literature says **lowers** perceived
speed) with *tunnel vision* (a **FOV reduction**, which the literature says **also lowers**
perceived speed). Both halves of that effect push the wrong way. Chad's rejection was not
taste — it was correct perception, and the literature predicts it. The lesson generalises:
**to sell speed, widen FOV with speed and sharpen the image; do not narrow and blur it.**
**Killing mutation:** obtaining the paper body and finding the motion-blur result is
non-significant, or that the FOV effect reverses at the geometric FOVs a game actually uses
(one result in the same corpus reports that "larger FoV, both horizontally and
peripherally-vertically, significantly reduced participants' speed in a car simulator
study" — i.e. perceived-speed and driven-speed effects can point opposite ways). That
counter-result is real and is why I label this LITERATURE, not DERIVED.

### R2-15 — engine audio is a load channel, not a speed channel, and the two must not be confused

The shipped practice: "Engine audio should match the game physics engine RPM and react to
load (throttle) in a believable way"; recordings are "looped, pitch-shifted, and
cross-faded according to information from the game, such as revolutions per minute (RPM),
throttle load, and gear"; racing games "rely heavily on engine sound not only for atmosphere
but also as a form of gameplay feedback — communicating information about acceleration,
traction, and mechanical performance to the player in real time"; and wind is a *separate*
layer — "Engine and wind sounds do a very good job of giving a feeling of speed", with
audio systems modelling "collision forces, and aerodynamic effects such as wind rush."
The governing principle stated in the same corpus: "the key point of designing sound for
games is to make the sound emphasize the feeling the gameplay tries to create, rather than
to sound exactly like something in real life."

**Confidence:** LITERATURE.
**Why it matters:** SEADS pins both channels already — `engine_rpm` and `belt_speed_ms` are
distinct fields in `SLEDTAPE_PIN_D`, as are `track_slip` and `ground_speed_ms`. A
snowmobile's most legible state — *track slipping under load vs hooking up* — is exactly the
one an RPM-vs-ground-speed divergence encodes, and it is exactly the state that makes Chad's
canonical drift readable ("drift around corners and then with throttle, straighten out",
§0). Pitching engine audio off `ground_speed_ms` instead of `engine_rpm` would destroy that
signal; pitching a wind layer off `ground_speed_ms` restores the speed channel separately.
**Killing mutation:** reading the audio wiring and finding engine pitch is already driven by
`engine_rpm` with a separate wind bus, making this a no-op observation. I did not read the
audio code.

### R2-16 — roost is the reviewed feature in this genre, and it is the cheapest honest win

The snow-interaction layer is what reviewers and endorsers actually name in Sledders:
SnoWest — "This is our kind of game! Mountain sleds, deep snow, hitting trees and getting
stuck... sign us up!"; a top-rated review — "the amount of times where ive hit a jump and it
makes my heart skip a beat", "me leaning in my chair trying be a sidehill demon", and "the
'illusion of actually riding a sled'". The praise is for the *snow*, not the handling model.

**Confidence:** LITERATURE.
**Why it matters:** SEADS already computes the driver — `roost_flux` is a pinned state field
and `roost_ref_depth_m` / `roost_gain` are existing params (`SLEDTAPE_PARAMS_D`). The
finding is that this is a *disproportionately* high-return channel in this specific genre:
the market's positive reviews are dominated by deep-snow feedback, and Chad's own ask is
partly perceptual — "SLiding banging, punchy" (§0b) is a *feedback* adjective list, not a
dynamics one. Three of those four words are things a particle system and a shake bus deliver.
**Killing mutation:** finding that `roost_flux` is not consumed by any renderer, which would
make this a build task rather than a tuning one; or finding Sledders' praise corpus is
dominated by handling after all (same risk as R2-01).

---

## 7. Candidate directions — each cites a tape event AND Chad's words

These are **candidates for the converge stage, not recommendations to build.** Each is
stated as a hypothesis with the evidence pair the hard rules demand. None of them may be
acted on without strand D-A/D-B confirming the kernel term and a red-team pass.

**C-1 — Treat frequency and recovery as two systems, and attack recovery first.**
*Tape:* R2-06 — 65-76 roll episodes per drive, mean 0.69-0.86 s but with 13-19 episodes ≥1 s
and, on tape 1, five ≥5 s and one of **53.6 s**.
*Chad:* "allow me to land on my skis more often after a roll (even though R works). But not
every time — allow it to happen" (§0); "slef righting by chance more" (§0b).
*Industry shape:* R2-09 (Riders Republic auto-landing acts on rotation at the landing
moment, is condition-gated, and still lets you crash) and R2-11 (the genre's answer to
failure is cheaper failure, not less).
*Reading:* the 53.6 s tail — not the 0.7 s median — is what reads as "constant rolling",
because a 0.7 s tip you ride out is *feel*, and a 53.6 s lie-down is a *stop*. A distribution
shift on the long tail satisfies §0 literally and touches neither the frequency nor the
drift.

**C-2 — If a stability term is added, add it at the input layer or as a conditioning term,
never as a constant righting torque.**
*Tape:* R2-05 — 81.9-98.7% of rolled time is in ground contact, so a term gated on contact
reaches essentially all of the problem without touching air behaviour (and Chad's air is
6.5-9.5% of the drive and includes his backflips — R2-04).
*Chad:* "It will ruin the feel to have a governor" (§0b); "Yep I can do backflips" (§0).
*Industry shape:* R2-07 (Criterion's `Input Layer Assists` / `Simulation Assists` split) and
R2-08 (the honest four — anti-roll bar, load distribution, speed-scaled steering authority,
speed-varied torque — versus the one governor-shaped tool, "artificial torque application to
prevent tipping", which is what produces R2-02's "never fully in control").

**C-3 — If forgiveness is added, price it in the battle economy rather than hiding it.**
*Tape:* R2-04 — he is airborne 6.5-9.5% of the time and above 20 m/s for 14-25% of it; these
are the moments an assist would act on, and they are also his showpiece moments.
*Chad:* "there is a batttle going on as well" and "DOnt make it impossibly hard" (§0b), held
against "I do want the finess and all the mechanisms available for tuning" (§0b).
*Industry shape:* R2-09 — Ubisoft handicaps score for auto-landing; Turn 10 pays CR for
assists off. Pricing preserves the sim identity Chad likes ("the game feels like a nicely
made sim as there is dynamic room", §0) while shipping the forgiveness he asked for.

**C-4 — Spend the next perception rung on turn-side camera shift and speed-coupled FOV, not
on blur or tunnel effects.**
*Tape:* R2-04 — the 10-25 m/s band is where 42-67% of his ticks live; that is where a
speed-coupled FOV curve must be calibrated, not at the 45 m/s peak.
*Chad:* "drift around corners and then with throttle, straighten out. The feel is really
good. Don't lose the feel" (§0); and the R1 motion-blur/tunnel-vision revert (paraphrased
from session record, see §7 UNVERIFIED-3).
*Industry + literature:* R2-13 (lateral camera shift is how shipped games make a turn read
as a turn) and R2-14 (blur and narrowed FOV both *reduce* perceived speed — the reverted
effect was pushing against its own goal).

**C-5 — Make the roll legible before making it rarer.**
*Tape:* R2-05/R2-06 — 37-76 episodes per drive means Chad experiences a roll onset every
12-22 s; whatever he is or is not doing at those moments, the kernel is not currently
telling him which it was.
*Chad:* "she's too unsteady" (§0) — an attribution complaint, not a frequency one; and
"Allow for bad driving too but keep the benefit there for good riding" (§0b), which is only
meaningful if the player can tell which one they just did.
*Industry law:* R2-11 — "any faults you make are down to human error with you never feeling
cheated."
*Note:* this is the cheapest candidate and the one most likely to be mistaken for doing
nothing. It is a readout/feedback rung, not a kernel rung.

---

## 8. UNVERIFIED — what I could not establish

1. **Sledders review corpus not read in full.** R2-01 and R2-16 rest on fetched *summaries*
   of the Steam top-rated and top-negative review pages plus a PC Gamer round-up whose body
   would not extract (the fetch returned only the page chrome). The individual quotes are
   real; their *representativeness* is not established.
2. **`air_s` semantics not read.** Every "% airborne" figure (R2-04, R2-05) assumes `air_s`
   is a per-tick airborne timer that is 0 in contact. I read the tape schema, not
   `sim/sled.cpp`. If it is a latch or a cumulative counter, R2-04's air share and R2-05's
   in-air/on-ground split both move.
3. **The R1 speed-FX revert quote is a paraphrase.** "looked bad" is the session record's
   summary, not Chad's typed words. `grep -rniE "tunnel.?vision|motion blur|speedfx"` over
   `*.md`/`*.toml` in this worktree finds nothing relevant. The scrap tag
   `scrapped/speedfx-r1-20260912` and lane reset to `28e741247` are the hard artefacts; the
   words are not. **C-4 must not be presented to Chad as quoting him.**
4. **Riders Republic points handicap not confirmed at source.** The official Ubisoft help
   article fetched empty. Twinfinite, Gamepur and GameSpot all state the no-bonus/handicap
   rule; Ubisoft's own wording is unverified.
5. **SEADS camera code not read.** R2-13's claim that turn-side lateral shift is *unused* is
   an assumption from the session record of the cam-smooth lane, not from reading
   `app/rest_horizon.h` or the cam implementation.
6. **The FOV/motion-blur paper body not obtained.** ScienceDirect returned 403; the DiVA PDF
   did not yield clean body text. Title, authors, year, journal, volume, pages and DOI are
   confirmed; the quoted results sentences come from search-result extracts of the
   abstract/results. A counter-result in the same literature (larger FOV *reducing* driven
   speed in a simulator) is noted in R2-14 and not reconciled.
7. **The Criterion talk was not heard.** Only the slide deck text was extracted. The
   `Input Layer Assists` / `Simulation Assists` labels are verbatim; their definitions are
   my reading of the surrounding slides (DERIVED, flagged in R2-07).
8. **No competitive-scan of 2020-2026 simcade snowmobile titles.** R2-03's "unoccupied
   middle" is supported by two review quotes, not by a market survey. It would be a GUESS to
   call it a gap.
9. **Roll-episode debounce not run.** R2-06's episode counts assume `rolled` does not chatter
   across the 75° threshold. `rolled_persist_s` and `rolled_grace_s` exist on `SledComfort`,
   which suggests someone already suspected chatter. Left to strand D-B.
10. **Nothing here measures the drift Chad loves.** RC-3 in the charter demands the drift be
    measured before and after any change. This strand did not measure it; no candidate in §7
    may land without that leg.

---

## 9. Sources

Primary (downloaded and text-extracted locally):
- Matthew Harris, *Vehicle Feel Masterclass: Balancing Arcade Accessibility with Simulation
  Depth*, GDC 2018, Criterion Games —
  https://media.gdcvault.com/gdc2018/presentations/Harris_Matthew_VehicleFeelMasterclass.pdf
  (93 pp; session page https://www.gdcvault.com/play/1025383/Vehicle-Feel-Masterclass-Balancing-Arcade ;
  video https://www.youtube.com/watch?v=n_A0RqeGado )

Design / development:
- Livio De La Cruz, "Implementing Racing Games: An intro to different approaches and their
  game design trade-offs", Game Developer, 2016-08-12 —
  https://www.gamedeveloper.com/design/implementing-racing-games-an-intro-to-different-approaches-and-their-game-design-trade-offs
- "Introducing Lonely Mountains: Downhill", ModDB (developer statement; fetch returned 403,
  content via search extract) — https://www.moddb.com/news/introducing-lonely-mountains-downhill
- Lonely Mountains: Downhill, Steam store page — https://store.steampowered.com/app/711540/

Snow machines:
- Sledders, Steam store page — https://store.steampowered.com/app/2486740/Sledders/
- Sledders, Steam top-rated reviews — https://steamcommunity.com/app/2486740/reviews/?browsefilter=toprated
- Sledders, Steam top negative reviews — https://steamcommunity.com/app/2486740/negativereviews/?browsefilter=toprated
- "Powder enthusiasts seem pretty pleased with new physics-based realistic snowmobile sim
  Sledders", PC Gamer (body would not extract; content via search extract) —
  https://www.pcgamer.com/games/sim/powder-enthusiasts-seem-pretty-pleased-with-new-physics-based-realistic-snowmobile-sim-sledders/
- Snow Moto Racing Freedom reviews — Nintendo Life
  https://www.nintendolife.com/reviews/nintendo-switch/snow_moto_racing_freedom ; Cubed3
  https://www.cubed3.com/games/reviews/pc/snow-moto-racing-freedom ; ZTGD
  https://ztgd.com/snow-moto-racing-freedom-pc-review/

Assists, landing, forgiveness:
- "Riders Republic Auto or Manual Landing Assist: Which Should You Choose & How to Change
  It", Twinfinite — https://twinfinite.net/guides/riders-republic-auto-manual-landing-assist/
- "Riders Republic landing mode options explained", Gamepur —
  https://www.gamepur.com/guides/riders-republic-landing-mode-options-explained
- "Riders Republic Stunts Guide — What To Know About The Various Control Options", GameSpot —
  https://www.gamespot.com/articles/riders-republic-stunts-guide-what-to-know-about-the-various-control-options/1100-6497519/
- "Landing Modes in Riders Republic", Ubisoft Help (fetch returned empty body) —
  https://www.ubisoft.com/en-us/help/riders-republic/gameplay/article/landing-modes-in-riders-republic/000098980
- Riders Republic reviews — CogConnected https://cogconnected.com/review/riders-republic-review/ ;
  GameSkinny https://www.gameskinny.com/reviews/riders-republic-review-steep-goes-deep/
- Forza driver assists — Forza Wiki https://forza.fandom.com/wiki/Driver_Assists (fetch
  returned HTTP 402; content via search extract); Forza Motorsport assists guide
  https://simracingsetup.com/forza/forza-motorsport-assists-settings/ ; ForzaTune
  https://forzatune.com/guide/recommended-assists-and-settings/
- "Accessibility is a core pillar of Forza Horizon 5, with its new 'Tourist' difficulty",
  PCGamesN — https://www.pcgamesn.com/forza-horizon-5/accessibility-tourist-difficulty
- Forza Horizon 5 Accessibility Support, Forza Support —
  https://support.forza.net/hc/en-us/articles/46523995129747-Forza-Horizon-5-Accessibility-Support

Rider-as-separate-body, forgiveness curves:
- MX vs. ATV Reflex, Wikipedia — https://en.wikipedia.org/wiki/MX_vs._ATV_Reflex
- "The arcade physics ruin it for me...", MX vs ATV Legends Steam discussion —
  https://steamcommunity.com/app/1205970/discussions/0/690868226470453065/
- Trials Fusion, Wikipedia — https://en.wikipedia.org/wiki/Trials_Fusion
- Trials HD reviews — TheSixthAxis https://www.thesixthaxis.com/2009/08/13/review-trials-hd/ ;
  GameSpot https://www.gamespot.com/reviews/trials-hd-review/1900-6215184/ ; Destructoid
  https://www.destructoid.com/review-trials-hd/
- Descenders reviews — Gamereactor https://www.gamereactor.eu/descenders-review/ ; Metacritic
  https://www.metacritic.com/game/descenders/

Perception and audio:
- Béla Berényi & Björn Lidestam, "Speed perception affected by field of view: Energy-based
  versus rhythm-based processing", *Transportation Research Part F: Psychology and Behaviour*
  65 (2019) 227-241, DOI 10.1016/j.trf.2019.07.016 —
  https://www.sciencedirect.com/science/article/pii/S1369847819301548 (403; open-access copy
  https://www.diva-portal.org/smash/get/diva2:1369670/FULLTEXT01.pdf )
- "Impact of the geometric field of view on drivers' speed perception and lateral position in
  driving simulators", Procedia Computer Science —
  https://www.sciencedirect.com/science/article/pii/S1877050920304270
- "Integrating Interactive Car Engine Sounds in Games", BOOM Library —
  https://www.boomlibrary.com/blog/the-car-engine-sound-primer-mike-caviezel/
- Harald af Malmborg, "Evaluation of Car Engine Sound Design Methods in Video Games", DiVA —
  https://www.diva-portal.org/smash/get/diva2:1557027/FULLTEXT01.pdf
- "Racing Game Design (Principles, Mechanics, Template)", GameDesignSkills —
  https://gamedesignskills.com/game-design/racing/
- Jan Willem Nijman (Vlambeer), "The Art of Screenshake", GDC/INDIGO 2013 (secondary
  summaries only; the deck's technique list was not obtained) —
  https://pepwuper.com/jan-willem-nijman-co-founder-of-vlambeer-on-the-art-of-screenshake/

Repo sources read (read-only):
- `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0, §0b, §1, §2, §3 (main tree)
- `test/harness/sled_tape.h` (record schema, `SLEDTAPE_PARAMS_D` / `SLEDTAPE_COMFORT_D` /
  `SLEDTAPE_PIN_D`)
- `sim/sled.h:42-58` (`SledInputs`)
- `D:/flight_sim2/seads-recon/build-play/sled_tape_{1,8,11,13,14,15,16,18,22,23,25,34,39,51,55,56,57,71,73}.sledtape`
  (read-only `awk`; no binary run)

---

*Strand R2, sled ride audit, 2026-09-17. Read-only. No dial changed, no config edited, no
test run, nothing pushed.*

---

# §10. VERIFICATION PASS — what re-measurement confirmed, and what it overturned

Method: an independent `awk` pass written from scratch against the schema (`r2_roll.awk`,
`r2_roll2.awk`, `r2_gate.awk` in this session's scratchpad), run read-only over Chad's
`.sledtape` files in `D:/flight_sim2/seads-recon/build-play`. No binary was run, no ctest,
no `seads.exe`, no dial touched, no config edited.

## 10.1 — CONFIRMED: the first attempt's tape arithmetic is exact

Re-derived independently, tape 34: `ticks 66134  drive_s 551.1  meanV 16.06  maxV 44.82
pct>10 66.6  pct>15 42.2  pct>20 24.5  pct_air 6.7  rolled 3564 (5.39%)  rolled_in_air 310
(8.7%)`. Every one of those matches §4's table to the digit. Tape 1 likewise: `97184 /
809.9 s / meanV 11.08 / maxV 44.16 / rolled 16304 (16.78%) / 37 episodes / longest 53.60 s
/ total 135.9 s`. **`NFbad 0` on every tape** — the NF=54 layout check holds, so
`$47 = ground_speed_ms`, `$49 = rolled_hold_s`, `$50 = air_s`, `$54 = rolled` are correct.

Independently re-confirmed from `test/harness/sled_tape.h`: `SLEDTAPE_PIN_D` item 36 *is*
`ground_speed_ms`, 38 `rolled_hold_s`, 39 `air_s`.

**Confidence:** MEASURED. **Killing mutation:** adding or reordering a `SLEDTAPE_PIN_D`
entry; the `NFbad 0` counter is the tripwire and must be re-run after any kernel change.

## 10.2 — RESOLVED (UNVERIFIED-2): `air_s` is a true per-tick airborne timer, and it is CONSERVATIVE

`sim/sled.cpp:2131-2135` (read-only):

```
if (normal_sum + side_normal_sum > 1.0 ||
    (r_after - floor_r) < 1.2 * p.cg_height_m)
    s.air_s = 0.0;
else
    s.air_s += h;
```

It is reset on contact **or CG proximity**, and accumulates otherwise. So it is not a latch
and not a cumulative counter — §4's "% airborne" stands. Better: because the CG-proximity
clause also zeroes it, a low hop reads as *ground*, so **the measured 3–9% air share is a
floor, not a ceiling.** R2-04's "air is a tenth of this game" survives and strengthens.

**Confidence:** MEASURED (the source text) + DERIVED (the conservatism).
**Killing mutation:** `cg_height_m` being large enough that 1.2x it exceeds a real jump
apex, swallowing genuine air. `cg_height` is ~0.564 m per the kernel's own
`d_rail = hypot(0.19, 0.564)` comment, so 1.2x is ~0.68 m — far below a send.

## 10.3 — OVERTURNED: `rolled` was already contact-gated and debounced when these tapes were cut

§4/R2-05 calls contact-gating the readout "still correct housekeeping". **It is already
shipped.** `sim/sled.cpp:255-263` requires the >75 deg attitude to *hold* for
`rolled_persist_s` while `air_s <= rolled_grace_s`, and decays the hold at 2x otherwise.
Every tape header reads `rolled_persist_s 0.3  rolled_grace_s 0.2`.

Three consequences the first attempt missed:

1. **Every measured episode is a floor.** `rolled` cannot fire until 0.3 s of continuous
   past-75 deg attitude has already accumulated, so a "0.69 s episode" is ~1.0 s of real
   attitude excursion, and the episode *counts* cannot be chatter-inflated.
   **This resolves UNVERIFIED-9 without a debounce re-run.**
2. **The small in-air share is a decay tail, not an artefact.** With grace 0.2 s and the
   hold decaying at 2x from its 0.8 s cap, a rolled machine that launches can stay flagged
   for at most ~0.45 s of air. The measured 1.3–18.1% in-air share is exactly that tail.
   R2-05's "81.9–98.7% of rolled time is in ground contact" is therefore **confirmed by
   construction as well as by measurement.**
3. The charter's complaint that `rolled` "fires MID-AIR on legitimate sends" was fixed
   before these drives. **Do not spend a rung on it.**

**Confidence:** MEASURED. **Killing mutation:** a tape whose header carries
`rolled_persist_s 0.0`, which the kernel comment says "makes this exactly the old
instantaneous readout". I checked tapes 1, 16 and 34: all 0.3.

## 10.4 — OVERTURNED then RE-DERIVED: the long rolls are decelerations, not tip-overs

A first cut measured inputs over the 0.5 s before the flag and found onset speeds of
0.5–4 m/s with the bar pinned, which reads as "he tips over at walking pace". **That read
is wrong and I killed it myself**, because §10.3 means the 0.5 s window overlaps 0.3 s of
already-tipped machine. Widening the window to 3.3 s (`r2_roll2.awk`) inverts the picture.
Tape 1, every episode >=1 s, instantaneous speed (m/s) at the stated lead time before the
flag:

| ep | dur (s) | V@-0.5 | V@-1.0 | V@-2.0 | V@-3.0 | Vmax over prior 3.3 s |
|---|---|---|---|---|---|---|
| 3 | 4.16 | 25.2 | 27.6 | 34.3 | 39.7 | 42.4 |
| 4 | 14.66 | 0.1 | 0.6 | 4.5 | 10.0 | 11.8 |
| 7 | 2.34 | 3.6 | 6.7 | 14.0 | 14.4 | 14.5 |
| 8 | 3.73 | 14.6 | 14.8 | 21.0 | 20.9 | 22.6 |
| 17 | 4.16 | 3.7 | 5.1 | 8.5 | 13.4 | 13.9 |
| 20 | 9.02 | 3.3 | 5.5 | 14.0 | 16.1 | 16.2 |
| 23 | 15.32 | 0.8 | 3.6 | 10.4 | 12.2 | 12.3 |
| 29 | 1.48 | 9.1 | 11.6 | 18.9 | 19.2 | 19.7 |
| 32 | 10.22 | 4.0 | 4.7 | 10.0 | 12.3 | 12.5 |
| 34 | 4.91 | 10.5 | 10.6 | 10.8 | 11.1 | 11.5 |
| **37** | **53.60** | 5.3 | 8.1 | 9.1 | 9.2 | 9.2 |

**Mean V at -2.0 s across the 13 long episodes = 13.7 m/s. Zero of the 13 had a prior-3.3 s
max below 8 m/s.** Tape 34 agrees: mean V@-2.0 s = 12.5 m/s over its 9 long episodes, one
below 8 m/s. These are **not** standing tip-overs. The machine carries 10–16 m/s, sheds
almost all of it inside 1–2 s, and *then* lies over — a dig-in / impact, not a wobble.

**And the law that falls out of it:** *roll duration is inversely related to the speed
carried into the roll.* On tape 1 every episode with a prior max above 19 m/s lasted under
4.2 s (42.4 -> 4.16 s; 22.6 -> 3.73 s; 19.7 -> 1.48 s), and every episode over 9 s had a
prior max below 16.3 m/s (9.2 -> 53.60 s; 12.3 -> 15.32 s; 11.8 -> 14.66 s; 12.5 ->
10.22 s; 16.2 -> 9.02 s). A fast roll **tumbles and releases you**; a moderate-speed roll
**lies down and stays down**.

**Why it matters:** the thing Chad called "the constant rolling" is not the spectacular
wipeout, and it is not a low-speed balance failure either. It is the *lie-down after a
mid-speed dig-in*, where the machine has no kinetic energy left to carry it through and —
before §11 — no mechanism to get up. That is exactly what "**allow me to land on my skis
more often after a roll (even though R works)**" (§0) describes.

**Confidence:** MEASURED (the table) / DERIVED (the inverse law; n = 22 long episodes over
two tapes, no regression fitted).
**Killing mutation:** the inverse law is a two-tape eyeball, not a fit — pooling all 15+
tapes and finding the duration/entry-speed rank correlation not significant would kill it.
It would also be killed if `ground_speed_ms` is body-frame rather than world-frame, so that
a machine sliding on its side reads ~0 while genuinely moving; I did not read its writer.

---

# §11. THE HEADLINE — the mechanism this strand's research converged on is ALREADY BUILT, and Chad's own tapes measure it working

## 11.1 — The kernel contains a Chad-specified, condition-gated righting assist

`sim/sled.h` (read-only) carries, on `SledComfort`, a complete righting system whose design
comments quote Chad directly. Verbatim from the source:

> `// "IT CANNOT HAPPEN ABOVE 5KM/H" -- his number, exactly, not a derivation.`
> `double right_assist_max_ms = 5.0 / 3.6;`

> `// "MACHINE NEEDS TO BE TIPPING OVER" -- the assist does nothing to an`
> `// upright machine, so it can never be a free roll-stiffness term in normal riding.`

> `// ★ v2 (2026-08-26, after Chad drove v1): "no it didnt work at all, never`
> `// got the standing function to right me, also should work from a full`
> `// inversion, just takes a few sustained pushes."`

The shipped shape: a torque gated on **low-passed speed < 1.389 m/s** (hysteretic, re-arms
at 0.8x), on **tilt** (a ramp: zero below 14.3 deg, full at 20 deg), on **ground contact**,
and driven by the player's own `stand` input through a **charge/rest muscle model**
(`right_charge_push_s`, `right_charge_rest_s`) tuned to the machine's measured ~0.93 s rock
half-period. It is **rocking, not righting**: the player pumps it out.

This is, term for term, what §5's research says the industry does and what §0b licenses: it
is **not a governor** (it cannot act on an upright machine, at speed, or without the player
pressing), it is **condition-gated** like Riders Republic's landing assist (R2-09), it is
**player-driven** like the Trials/MX rider stick (R2-10, R2-21), and the kernel comment even
refuses the dishonest version — *"Do NOT 'fix' that with a hidden nudge -- a hidden nudge is
the RNG-shaped sin this kernel forbids."*

**Confidence:** MEASURED (source text quoted verbatim).
**Killing mutation:** `config/scenario.toml` shipping `right_assist_nm = 0.0`, which would
make the whole block inert. It ships **2400.0** (line 2085, read-only).

## 11.2 — Chad's tapes are a natural A/B across it, because it did not exist for the early ones

Header grep, read-only:

| tape | date | `right_assist_nm` in header |
|---|---|---|
| 1, 8, 16 | 2026-08-19 to 08-23 | *absent* -> tape-absent preset pins **0.0** -> **assist OFF** |
| 34 | 2026-08-27 | **2400** |
| 86–91 | **2026-09-17** | **2400** |

The kernel comment explains why absence means off: *"the tape-absent preset pins it to 0.0
-- so all 31 existing tapes replay bit-exactly while the game's config turns it on."*

## 11.3 — MEASURED: severity collapsed; frequency did not move

| | assist | drive_s | rolled % | episodes | one per (s) | >=1 s | >=5 s | **longest** | total rolled_s | overrides |
|---|---|---|---|---|---|---|---|---|---|---|
| tape 1 (08-19) | **OFF** | 809.9 | **16.78** | 37 | 21.9 | 13 | 5 | **53.60 s** | 135.9 | 13 |
| tape 34 (08-27) | ON 2400 | 551.1 | 5.39 | 29 | 19.0 | 9 | 1 | 7.44 s | 29.7 | 3 |
| tape 87 (09-17) | ON 2400 | 118.9 | 1.86 | 4 | 29.7 | 0 | 0 | 0.79 s | 2.2 | 0 |
| tape 89 (09-17) | ON 2400 | 127.1 | **0.00** | 0 | — | 0 | 0 | **0.00 s** | 0.0 | 0 |
| tape 90 (09-17) | ON 2400 | 108.2 | 2.28 | 5 | 21.6 | 1 | 0 | 1.06 s | 2.5 | 0 |
| tape 91 (09-17) | ON 2400 | 172.8 | 8.48 | 13 | 13.3 | 3 | 1 | 5.09 s | 14.7 | 2 |
| **09-17 pooled** | ON 2400 | **527.0** | **3.67** | **22** | **24.0** | **4** | **1** | **5.09 s** | **19.4** | **2** |

Read the last two rows against the first:

* **Recovery cost — SOLVED.** Longest lie-down **53.60 s -> 5.09 s** (10.5x). Episodes
  >=5 s: **5 -> 1**. Share of drive spent past 75 deg: **16.78% -> 3.67%** (4.6x).
* **The R key — nearly retired.** Overrides per minute of drive **0.96 -> 0.23** (4.2x
  fewer). On tapes 87, 89 and 90 he pressed it **zero times in 354 s**. (Every override in
  tapes 1 and 34 landed within 0.01 s of a roll-episode end, so in these drives the O record
  *is* the roll escape and nothing else.)
* **Frequency — UNMOVED.** One roll onset per **21.9 s** then, per **24.0 s** now. Within
  noise. The machine tips over just as often; it simply no longer *stays* tipped.

**A further measurement pins why the assist reaches exactly this population and nothing
else** (`r2_gate.awk`, tape 1, the assist-OFF drive): **88.3% of all rolled time, and 92.2%
of long-episode time, was spent below the shipped 1.389 m/s gate.** Per episode: the 53.60 s
lie-down was **99.0%** below the gate; the 14.66 s, 9.02 s and 2.63 s episodes **100%**; and
the one fast tumble (EP 3, entry 42.4 m/s) only **8.0%**. **Chad's own 5 km/h number selects
the lie-downs and excludes the wipeouts and the backflips**, which is precisely "**Make it
possible to roll but not the rule**" (§0) and precisely why it does not read as a governor.

**Confidence:** MEASURED for every cell in the table and every percentage.
**Killing mutation — and it is a serious one:** *this is a before/after across a moving
kernel, not a controlled A/B.* Tape 1 (08-19) and the 09-17 tapes are different drives, on
different terrain, separated by five weeks in which the R4a ladder, the rollover fix, the
hull work and several kernel versions all landed. **The claim "the assist caused this" is
DERIVED and confounded.** The clean experiment exists and is cheap: replay tape 1 through
`seads_sled_probe` twice, with `right_assist_nm` at 0.0 and 2400.0, and compare episode
duration histograms. That is strand D-B's lane and this strand did not run it. A second
confound: the 09-17 tapes are 108–173 s, the August ones 551–810 s; short tapes may be
targeted test drives rather than free riding, which would bias the pooled row.

## 11.4 — What this does to the charter

The acceptance-bar quotes in §1 are dated **drive 4, 2026-08-12**. Tape 1 (08-19) is the
closest tape to that machine, and it is the 16.78% / 53.60 s / 13-R-presses drive. **The
machine Chad complained about is not the machine that ships.** Of his two distinguishable
asks:

* "**allow me to land on my skis more often after a roll (even though R works)**" (§0) and
  "**slef righting by chance more**" (§0b) — **DELIVERED, measurably** (§11.3).
* "**Make it possible to roll but not the rule**" (§0) and "**she's too unsteady**" (§0) —
  **STILL OPEN**: one onset per 24 s, unchanged.

§5's research says these are two different systems with two different solutions — and the
better-precedented one (cheap, fast recovery: R2-09 Riders Republic, R2-11 Trials, R2-18
Steep's "very forgiving restart system") **has already been spent.** The remaining half is
the frequency half, which is where the governor risk lives and where §0b's veto bites
hardest. **The next rung is therefore harder than the last one, not easier, and it should
not be opened by re-reading the §0 quotes as if nothing had happened. It should be opened by
asking Chad to fly the current build and re-rule.**

---

# §12. NEW RESEARCH — the titles the first attempt did not cover

### R2-17 — Sledders, the honest-sim competitor, ships an INPUT-LAYER assist

Sledders' own community support thread tells struggling players the game has an **"auto
steer"** option in settings that "makes the beginning a lot easier", alongside the underlined
recommendation to use a controller, because in Sledders "players turn by shifting their
weight around rather than using traditional steering methods".
**Confidence:** LITERATURE.
**Why it matters:** the most sim-committed snowmobile title on the market did not answer its
difficulty complaints with a stability torque. It answered with an **input-layer** assist —
exactly Criterion's left-hand column (R2-07) — that reshapes what the stick means and leaves
the machine honest. It is the closest available precedent for a non-governor intervention in
*this exact genre*, and it points at steering/lean shaping, not at attitude.
**Killing mutation:** finding that Sledders' "auto steer" actually applies a corrective yaw
torque to the machine (simulation layer), which would collapse the distinction.
Source: https://steamcommunity.com/app/2486740/discussions/0/4035851449847545127/

### R2-18 — Steep is the shipped proof that "forgiving" and "great sense of speed" coexist

Reviews of Steep (Ubisoft, 2016) praise both halves at once: the sense of speed is
"impressive", reviewers "don't remember another skiing or snowboarding video game in recent
memory that nailed that sense of speed as much as Steep does", and "the sense of speed and
relative friction" is "exquisite" — while simultaneously "tricks and jumps feel very
intuitive but also forgiving" and "Ubisoft nailed the balance between Sim and Arcade
controls for skiing and snowboarding". Its failure handling is friction removal, not failure
prevention: "a very forgiving restart system that draws a dotted line down the path you take
and then lets you restart on any of those dots."
**Confidence:** LITERATURE.
**Why it matters:** it is the existence proof for Chad's exact target — "**a balance of fun
and accuracy to real physics**" (§0) — in a snow game, and it reaches it by the R2-11 route
(cheap re-entry), which §11.3 shows SEADS has now also taken. Note the dissent: some
reviewers found "timing jumps remains more of an art than a science" even after hours, which
is the cost of the arcade pole.
**Killing mutation:** Steep's speed praise being attributable to its camera/FOV rather than
its handling, which would move this finding from §5 into §6. Both readings support C-4.
Sources: https://www.gamespot.com/reviews/steep-review/1900-6416591/ ;
https://www.pushsquare.com/reviews/ps4/steep ;
https://www.metacritic.com/game/steep/user-reviews/

### R2-19 — SnowRunner: mass is sold by *consequence*, and the failure mode is "weightless"

SnowRunner's physics are the reviewed feature — "outstanding mud physics and vehicle
simulation", trucks "often sluggish, especially with loaded trailers attached, requiring
wider turning circles and almost tactical thinking", and a model that "simulates how mud
clings to tires, how water displaces when different weights are put into it, and when snow
spurts from the back of spinning tires". The complaint, when it comes, is the exact inverse:
"**It really feels like nearly weightless RC cars when you see your 15 ton APC jumping wildly
like kangaroo on rocks.**"
**Confidence:** LITERATURE.
**Why it matters:** the mass channel players actually read is **behaviour under load and in
contact** — turning circle, sluggishness, material displacement — not camera shake. And the
named failure is *airborne behaviour betraying the mass the ground behaviour promised*.
SEADS is airborne 3–9% of the time (§10.2) with hangs to 3.6 s, so this is a live risk here,
and it argues that any air-rotation authority added for §6/C-4 reasons must be small enough
not to read as "weightless".
**Killing mutation:** the "weightless RC cars" line being one forum post rather than a
recurring review theme; it is a community comment and I did not establish its frequency.
Sources: https://www.shacknews.com/article/117907/snowrunner-review-dumping-the-biggest-loads ;
https://godisageek.com/reviews/snowrunner-review/ ;
https://community.focus-entmt.com/focus-entertainment/snowrunner/ideas/3883-tweak-physics-simulation

### R2-20 — MX Bikes is the cautionary tale, and its own defenders name the remedy

MX Bikes "has a steep learning curve, with physics that feel deep and unforgiving" and
"needs a lot of time to learn the physics and controls". Critics say it "takes too long to
learn", "describing it as having closed itself off from thousands of possible players to a
limited group." Its defenders' counter-argument is the load-bearing part: "**there are
assists to make it more forgiving**", and the game ships "an arcade mode aimed at players who
turn on all the assists in the options."
**Confidence:** LITERATURE.
**Why it matters:** the sim-pole title in the adjacent genre resolves the same fight by
**shipping the assists as an explicit, named, optional layer** rather than by retuning the
kernel. That is the architecture Chad's contradictory pair asks for — "**DOnt make it
impossibly hard**" *and* "**I do want the finess and all the mechanisms available for
tuning**" (§0b) — and it is exactly how SEADS' `right_assist_nm` is already built: a named
dial, 0.0-off, bit-identical when off.
**Killing mutation:** MX Bikes' arcade mode being a separate physics model rather than an
assist layer over the same kernel.
Sources: https://steamcommunity.com/app/655500/discussions/0/1812044473307860692/ ;
https://steamcommunity.com/app/1205970/discussions/0/690868226470453065/

### R2-21 — Trials' air control is a *physics-based weight shift*, not a rotation override

In Trials Fusion the player has "control over your virtual motorcycle's throttle and brakes
as well as the lean of the rider", and uses that lean to "rotate in the air and land hard on
squishy suspension" — "the player controls how the rider shifts their weight forward and
backward in order to perform wheelies and stoppies as well as flips while in the air and
controlling how the bike lands." Crucially, "the trick system is entirely physics-based,
instead of relying on canned animations or set button inputs", and the left stick controls
rider lean "and therefore the rotation of the bike".
**Confidence:** LITERATURE.
**Why it matters:** the genre's benchmark for in-air control gets it **for free from the
rider's mass**, not from an added angular-velocity term. SEADS already has the separated
rider mass (`rider_lat_m`, the `lean_lat_*` dials) and already has a documented,
shipped-at-0 air term — `k_air_shift`, which `sim/sled.h:859` and `sim/sled.cpp:2096-2098`
show applies an angular impulse only when `!ground_contact`. The Trials precedent says: if
air authority is wanted, route it through the rider's weight shift (the honest path Chad
already praised — "**the balance mechanism works really good**", §0), not through a free
torque.
**Killing mutation:** reading `k_air_shift`'s implementation in full and finding it is
already a rider-mass term rather than a free impulse, which would make this a no-op
observation. I read its two call sites, not its derivation.
Sources: https://en.wikipedia.org/wiki/Trials_Fusion ;
https://venturebeat.com/games/trials-fusion-is-a-beautiful-poem-of-physics-and-motion/ ;
https://www.pcgamer.com/trials-fusion-review/

### R2-22 — Forza's assists are *optional and priced*, and the design line is explicit

Forza's stability control "will try to prevent your car from spinning during erratic inputs"
and traction control "will prevent your wheels from spinning and breaking traction"; the
strongest setting "attempts to keep wheels from losing traction by applying correcting
braking to the spinning wheel and cutting power when it detects extreme wheel spin",
eliminating "virtually all wheelspin". The stated design line: "**Every assist trades control
for safety**", "the ones that change how the car behaves are the ones worth understanding",
and "The Horizon games are about freedom. So forcing things isn't how they get down" — the
assists "remain optional rather than always-on".
**Confidence:** LITERATURE.
**Why it matters:** it names the tax honestly (*control for safety*) and refuses to levy it
without consent. That is the precise content of Chad's veto — "**It will ruin the feel to
have a governor**" (§0b) — and the reason SEADS' existing assist escapes it: it is armed only
while the player holds `stand`, so the player **opts in every single time**.
**Killing mutation:** Forza Horizon shipping any of these assists force-enabled at the
default difficulty, which would weaken "optional rather than always-on".
Sources: https://simracingsetup.com/forza/forza-motorsport-assists-settings/ ;
https://forzahorizonhub.com/guides/forza-horizon-6-assists-and-difficulty ;
https://www.ludo.guide/guide/forza-horizon-6/traction-and-stability-control-assist

---

# §13. REVISED CANDIDATES — superseding §7

Still candidates for the converge stage, not build orders. Each cites a tape event and one
of Chad's words, per the hard rule.

**C-0 (NEW, and it outranks everything) — Re-ask before re-tuning.**
*Tape:* §11.3 — the drive behind the §0 acceptance bar measures 16.78% rolled / 53.60 s
longest / 13 R presses; his 2026-09-17 drives measure 3.67% / 5.09 s / 2.
*Chad:* "**I am afraid it might ruin something that is reallt good right now but just dont
ruin it**" (§0b).
*Reading:* the largest single risk in this whole audit is tuning against a five-week-old
complaint about a machine that has since been changed by the very mechanism the complaint
asked for. Every remaining candidate should be gated behind one flight of the current build
and one fresh ruling. This is the cheapest and highest-value action available.

**C-1 (REVISED) — The recovery half is done; do not spend the next rung there.**
*Tape:* §11.3 — episodes >=5 s went 5 -> 1; R presses per minute 0.96 -> 0.23; zero R
presses in 354 s across tapes 87/89/90.
*Chad:* "**allow me to land on my skis more often after a roll (even though R works)**" (§0).
*Industry:* R2-09, R2-11, R2-18 — the genre's dominant answer to failure is cheaper failure,
and SEADS has now taken it.
*Supersedes* the first attempt's C-1, which proposed attacking the fat tail. The fat tail was
already attacked, by a mechanism Chad himself specified, and it worked.

**C-2 (REVISED, and the live one) — The open ask is FREQUENCY, and it is the hard half.**
*Tape:* §11.3 — one roll onset per 21.9 s (assist off, 08-19) vs per 24.0 s (assist on,
09-17): unmoved. §10.4 — onsets follow a 10–16 m/s -> near-zero deceleration inside 1–2 s,
i.e. a dig-in, not a balance failure.
*Chad:* "**Make it possible to roll but not the rule**" and "**she's too unsteady**" (§0).
*Industry:* R2-08's honest four are *conditioning* terms, not righting torques; R2-17 says
the one shipped snowmobile competitor reached for an **input-layer** assist. R2-02 is the
warning: buy stability by decoupling the machine from the ground and you ship "never fully in
control".
*Reading:* because the onset is a deceleration event, the lever is more likely to be in **how
the machine sheds speed into terrain** (the dig-in) than in roll stiffness. A roll term would
treat the symptom. **Strand D-A/D-B must identify the decelerating term before anyone
proposes a dial.**

**C-3 (UNCHANGED) — Price forgiveness in the battle economy rather than hiding it.**
*Tape:* §10.2 — 3–9% of the drive is airborne with hangs to 3.6 s; §4/R2-04 — 14–25% of ticks
above 20 m/s. These are the showpiece moments an assist would touch.
*Chad:* "**there is a batttle going on as well**" and "**DOnt make it impossibly hard**"
(§0b).
*Industry:* R2-09 (Ubisoft handicaps score for auto-landing), R2-22 (Forza pays CR for
assists off), R2-20 (MX Bikes ships assists as a named optional layer).

**C-4 (STRENGTHENED) — Spend the perception rung on turn-side camera shift and speed-coupled
FOV; not on blur or tunnel effects.**
*Tape:* §4/R2-04 — 42–67% of ticks in the 10–25 m/s band, which is where a FOV curve must be
calibrated. §11.3 — the 09-17 drives still show maxV 38–46 m/s, so the band is current.
*Chad:* "**drift around corners and then with throttle, straighten out. The feel is really
good. Don't lose the feel**" (§0). WARNING: the R1 blur/tunnel revert is a **paraphrase**, not
a quote — UNVERIFIED-3 stands. Do not present it to him as his words.
*Industry + literature:* R2-13, R2-14, and now R2-18 — Steep is reviewed as having the best
sense of speed in its genre *while being forgiving*, so the two are not in tension.

**C-5 (REVISED) — Make the roll legible; and note the readout is already honest.**
*Tape:* §10.3 — `rolled` is already debounced (0.3 s) and contact-gated (0.2 s grace), so the
instrument is not lying. §10.4 — but the *cause* is a dig-in the player has no cue for.
*Chad:* "**Allow for bad driving too but keep the benefit there for good riding**" (§0b) —
meaningless unless the player can tell which one they just did.
*Industry law:* R2-11 — "any faults you make are down to human error with you never feeling
cheated"; R2-19 — SnowRunner sells mass through *consequence in contact*.
*Reading:* the cue that is missing is **the dig-in itself** — a real rider would feel the skis
knife and the track hook. `track_slip`, `depth_under_m`, `plane_frac` and `roost_flux` are all
pinned already (R2-15, R2-16). This is a feedback rung, not a kernel rung, and it is the
cheapest thing on this list after C-0.

---

# §14. UNVERIFIED — revised status (supersedes §8)

**RESOLVED this run:**
* ~~2. `air_s` semantics~~ -> §10.2. Per-tick timer, reset on contact or CG proximity; the air
  share is a floor, not a ceiling.
* ~~9. roll-episode chatter~~ -> §10.3. The kernel debounces at `rolled_persist_s 0.3` and
  every tape header carries it; episode counts cannot be chatter-inflated.
* **New:** the §4 tape arithmetic, independently reproduced -> §10.1.
* **New:** whether the tapes were cut with the righting assist -> §11.2. Tapes 1/8/16 OFF
  (param absent, preset pins 0.0); tape 34 and all 09-17 tapes ON at 2400.
* **New:** whether the shipped assist's gate covers the complained-of population -> §11.3.
  92.2% of long-episode time is below it; the one fast tumble only 8.0%.
* **Partially:** item 5 (SEADS camera code) is still not read, but R2-18 makes C-4 robust
  either way.

**STILL OPEN — §8 items 1, 3, 4, 5, 6, 7, 8 and 10 stand as written, plus:**

11. **The A/B is confounded.** §11.3 compares different drives on different terrain five weeks
    and several kernel versions apart. "The assist caused the improvement" is DERIVED, not
    MEASURED. The clean experiment — replay tape 1 through `seads_sled_probe` at
    `right_assist_nm` 0.0 vs 2400.0 — was **not run** (strand D-B's lane; the hard rules forbid
    the full suite). **No dial may move on §11.3 alone.**
12. **Short-tape bias.** The 09-17 tapes are 108–173 s against 551–810 s for the August ones.
    If they are targeted test drives rather than free riding, the pooled 3.67% is optimistic.
    I did not establish what Chad was doing in them.
13. **`ground_speed_ms` frame not read.** §10.4 and §11.3's gate analysis both assume it is
    world-frame ground speed. If it is body-frame or track-derived, a machine sliding on its
    side could read ~0 while moving, inflating "below the gate".
14. **The inverse duration/entry-speed law (§10.4) is not fitted.** n = 22 long episodes over
    two tapes, eyeballed. No rank correlation computed, no pooling over the other 15+ tapes.
15. **O records are not proven to be R presses.** `test/harness/sled_tape.h:17` defines O as
    "any write to the state outside `step_sled`", which includes respawn and teleport. I showed
    every O in tapes 1 and 34 lands within 0.01 s of a roll-episode end, which is strong
    circumstantial evidence, but I did not read the input binding.
16. **Nothing here measures the drift Chad loves.** §8 item 10 stands and is the most important
    of these: RC-3 demands the drift be measured before and after any change, and this strand
    still has not measured it. **No candidate may land without that leg.**

---

# §15. Additional sources (this run)

- Sledders community support thread (auto-steer assist) —
  https://steamcommunity.com/app/2486740/discussions/0/4035851449847545127/
- Steep — GameSpot https://www.gamespot.com/reviews/steep-review/1900-6416591/ ;
  Push Square https://www.pushsquare.com/reviews/ps4/steep ;
  Metacritic user reviews https://www.metacritic.com/game/steep/user-reviews/ ;
  Kotaku https://kotaku.com/what-we-like-and-dont-like-about-steep-1789762995
- SnowRunner — Shacknews
  https://www.shacknews.com/article/117907/snowrunner-review-dumping-the-biggest-loads ;
  GodisaGeek https://godisageek.com/reviews/snowrunner-review/ ;
  Focus community "Tweak physics simulation"
  https://community.focus-entmt.com/focus-entertainment/snowrunner/ideas/3883-tweak-physics-simulation ;
  Wikipedia https://en.wikipedia.org/wiki/SnowRunner
- MX Bikes — Steam discussions
  https://steamcommunity.com/app/655500/discussions/0/1812044473307860692/ ;
  MX vs ATV Legends "arcade physics" thread
  https://steamcommunity.com/app/1205970/discussions/0/690868226470453065/
- Trials Fusion — Wikipedia https://en.wikipedia.org/wiki/Trials_Fusion ;
  GamesBeat https://venturebeat.com/games/trials-fusion-is-a-beautiful-poem-of-physics-and-motion/ ;
  PC Gamer https://www.pcgamer.com/trials-fusion-review/
- Forza assists — https://simracingsetup.com/forza/forza-motorsport-assists-settings/ ;
  https://forzahorizonhub.com/guides/forza-horizon-6-assists-and-difficulty ;
  https://www.ludo.guide/guide/forza-horizon-6/traction-and-stability-control-assist

Repo sources read this run (read-only, nothing edited):
- `sim/sled.cpp:245-263` (the `rolled` gate), `sim/sled.cpp:2124-2135` (`air_s`),
  `sim/sled.cpp:2096-2098` (`k_air_shift`)
- `sim/sled.h:215-300` (the `right_assist_*` block and its verbatim Chad quotes),
  `sim/sled.h:859` (`k_air_shift`), `sim/sled.h` `struct SledComfort`
- `config/scenario.toml:2005-2116` (shipped comfort values; **read only, not edited**)
- `test/harness/sled_tape.h` (`SLEDTAPE_PIN_D` ordering re-confirmed; O-record definition)
- tape headers and bodies for `sled_tape_{1,8,16,34,86,87,88,89,90,91}.sledtape` in
  `D:/flight_sim2/seads-recon/build-play`

*Strand R2 resumed run, sled ride audit, 2026-09-18. Read-only. No dial changed, no config
edited, no test run, no binary executed, nothing pushed.*
