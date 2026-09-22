# R3 — PLAYER EXPERIENCE: difficulty, failure cost, recovery, and the fact that there is a war on

Strand R3 of the sled ride audit. Worktree `D:/seads_sandboxes/sled-audit`, branch
`audit/sled-ride`. READ-ONLY against main: no dial moved, no config touched, no goldens
moved, no ctest run, `seads.exe` never launched, `seads-recon` read only (its
`build-play/*.sledtape` files opened for reading, nothing written there).

**What this strand is for.** R1 asks what a real snowmobile does; R2 asks what shipped
games do. R3 asks the third question, the one Chad put in the charter himself:
*"DOnt make it impossibly hard, there is a batttle going on as well"*
(`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0b). That is a
player-experience constraint, not a physics one. It has a literature, it has measurable
proxies, and — this is the point of the strand — **the proxies are already in his tapes.**
Every failure-cost number below was measured off drives Chad actually did, not off a
synthetic cell.

**Confidence vocabulary.** MEASURED (I computed it, this session, from his tapes or from
the shipped source) / DERIVED (arithmetic on a measured number) / LITERATURE (a cited
published source — evidence that someone *said* it, not that it is true) / GUESS (labelled
every time).

**Rule obeyed throughout.** No feel recommendation in §6 rests on a harness number alone.
Each one cites (a) an event measured in Chad's own tapes and (b) one of Chad's words,
quoted verbatim with document and section.

---

## 1. Chad's words — verbatim, with provenance

### 1.1 The charter

From `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md`, read this session out of
the main tree (`D:/flight_sim2/...`), §0 *"Chad's ruling (drive 4, 2026-08-12 — verbatim,
this is the acceptance bar)"*:

> "Yep I can do backflips, but she's too unsteady. **I rule that it should be roll
> resistant.** Let me slide around a bit arcade but **allow me to land on my skis more
> often after a roll** (even though R works). But not every time — allow it to happen.
> Also I've seen lots of snowmobiles **drift around corners and then with throttle,
> straighten out. The feel is really good. Don't lose the feel**, except the constant
> rolling. **Make it possible to roll but not the rule.** However the game feels like a
> nicely made sim as there is dynamic room and **the balance mechanism works really good.
> Just allow the balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead
> of having to be the necessary condition of not rolling over.** Just allow leaning a
> certain way in a particular condition to be the OPTIMAL weight distro for better
> traversing, say up a hill or around a corner. **Work on the math to achieve a balance of
> fun and accuracy to real physics, just as our airplane ontological counterpart does.**"

§0b *"SUPERSEDING RULING (2026-08-12 night — verbatim, OVERRIDES §0's decode and §1's
design)"*:

> "no I dont like thge idea in the handoff at all! It will ruin the feel to have a
> governor. Make it less honest but dont ruin it. Get a fable consult and the measure of it
> shall be if my intent is heard. Leaning shall enhance the ride an just make it more
> stable and slef righting by chance more. It should be arcadey to a degree so that it is
> fun., SLiding banging, punchy, jumps. Just make it more stable."

> "please build autmatically and make this kernel way better ... I am afraid it might ruin
> something that is reallt good right now but just dont ruin it. Allow for bad driving too
> but keep the benefit there for good riding. DOnt make it impossibly hard, there is a
> batttle going on as well. And the point is this should be fun as well, not a true sim.
> But I do want the finess and all the mechanisms available for tuning, so lets stop
> messing around abnd build something good for real. ... please knock this out of the park
> and make it awesome, keeping the good but making it easier / more fun?"

### 1.2 The felt report the GI4 rung was built against

From `docs/gi4_ride_handoff.md` §1 *"THE FELT REPORT, EACH ITEM MEASURED"* (this worktree;
the doc's provenance line names `build/sled_tape_4.sledtape`, 512.3 s, replayed bit-exact):

> 1 — "tips over too easy"
> 2 — "should be able to launch in the air"
> 3 — "turning too unstable, can't hold a carve"
> 4 — "WOT should lift the skis to ~30°"
> 5 — "rider weight needs authority, not thrown into a spin"
> 6 — "flips happen too fast and easy"

and, in the same doc's turning section, the property he named: *"I should be able to hold
a turn"*.

### 1.3 Two of his words that live in the shipped source, not in a handoff

`app/main.cpp` (this worktree, read-only), at the KEY_R block, quoting the DRIVE-2 ask:

> "I need a key for now that lets me autoright until we get the guy running back to the
> snowmachine"

`app/main.cpp` at the mouse-lean block, SK-1d, dated 2026-08-25:

> "I dont like the steering via mouse coupled with lean they have to be two independent
> things, part of what is so fun on the road is driving by lean."

and at the A/D steering block, dated 2026-08-25:

> "steering is too slow"

These three matter to R3 specifically: the first defines the reset key as *scaffolding*
(which is what makes its press rate a legitimate frustration proxy rather than a game
mechanic), and the second and third are his only recorded words about the *control
transfer function*, which is the onboarding half of this strand.

### 1.4 What is NOT quoted here

Chad has given **no felt report on tonight's tapes 86–91** (kernel v17). The audit plan
asks him for one line each and he has not answered
(`Game_loop_idea/vehicle_program/SLED_RIDE_AUDIT_PLAN_20260917.md` §6 item 2). Every
number in §3 about those six tapes is therefore *unattributed measurement*: it says what
the machine and his hands did, never how it felt. See §7 UNVERIFIED-1.

---

## 2. Method

### 2.1 Where the numbers come from

Two sources, both read-only:

1. `docs/sled_audit/tape_summary.json` — produced by strand D-B's analyzer
   `tools/sled_tape_audit.py` at 2026-09-17T23:43 over the 91 `.sledtape` files in
   `D:/flight_sim2/seads-recon/build-play` (84 parse OK; tapes 41–47 are zero-byte and are
   skipped). I did not edit that file or that tool.
2. A **scratchpad copy** of that analyzer (outside the worktree, nothing published),
   patched in three places so R3 can measure what D-B's schema does not expose: the tick of
   every past-90 entry and exit, a 10 Hz ground-speed trace, and the recovery predicate
   made settable from the environment. The patched copy was run over tapes 86–91 and over
   the full corpus. **No kernel code, no test, no ctest, no exe.**

The per-tape schema and its column derivation (T record = 54 fields, `$47` ground speed,
etc.) are D-B's and are documented in that strand's file; R3 inherits them.

### 2.2 Definitions R3 uses (each one is a knob, so each one is a killing mutation)

| Term | Definition in code | Why R3 uses it |
|---|---|---|
| **past-90 event** | `dot_up < 0` held for at least 0.10 s (D-B's analyzer, `sled_tape_audit.py` near line 529) | the machine is past ninety degrees: a rollover crossing |
| **crash episode** | past-90 events separated by **less than 2.0 s** of not-past-90 are ONE episode (R3's merge) | a player counts *crashes*, not tumble revolutions |
| **resume-riding time** | from past-90 entry until `tilt < 20°` AND `speed > 5 m/s` held 0.5 s (D-B's predicate) | "I am riding again" |
| **failure cost** | from past-90 entry until ground speed is back to **80 % of the mean speed over the 2 s before entry** (R3's metric) | Juul's measure: the *time the player loses* |
| **R press** | a tape `O` record classed `autoright_R` (tilt before > 60°, after < 30°) | the reset key; a behavioural frustration proxy |

Everything below names, for each number, the mutation that would falsify it.

### 2.3 The corpus caveat, up front

The 91 tapes in `seads-recon/build-play` are **not** the same corpus as the historical
docs. `docs/gi4_ride_handoff.md` measures `build/sled_tape_4.sledtape` — 512 s, 592 MB —
while `build-play/sled_tape_4.sledtape` in this corpus is 1.73 min. **Tape numbers are
per-directory and do not correspond.** Any cross-reference between a GI4-era number and a
number in this file compares two different drives. Stated once here; assumed everywhere
after.

---

## 3. What Chad's drives actually cost him — MEASURED

### R3-M1 — On kernel v17 he crashes about **once every 45 seconds**, and each crash tumbles about three times

Tonight's six tapes (86–91, `kernel-v17-tremor-signed`, 12.02 min of ride, written
2026-09-17 20:11–21:39):

- **52 past-90 events** = 4.32/min
- merged into **16 crash episodes** = **1.33 episodes/min = one crash every 45 s**
- therefore **3.25 past-90 crossings per crash** (DERIVED)
- **5.7 %** of ride time spent past ninety degrees (41 s of 721 s)

The whole corpus, for shape (206.1 min, 84 tapes, bucketed by the kernel tag in each tape
header):

| kernel bucket | ride min | crash episodes | episodes/min | past-90 events | R presses | R/min |
|---|---|---|---|---|---|---|
| `pre-reconcile-20260821` | 132.9 | 236 | 1.78 | 689 | 96 | 0.72 |
| `kernel-v13g-signed` | 39.9 | 89 | **2.23** | 251 | 43 | **1.08** |
| `kernel-v14-leanlead-signed` | 4.6 | 1 | 0.22 | 4 | 0 | 0.00 |
| `kernel-v15-righthand-signed` | 16.7 | 13 | **0.78** | 36 | 2 | 0.12 |
| `kernel-v17-tremor-signed` | 12.0 | 16 | 1.33 | 52 | 5 | 0.42 |
| **ALL** | **206.1** | **355** | **1.72** | **1032** | **146** | **0.71** |

**Confidence:** MEASURED.
**Killing mutation:** the 2.0 s merge gap. At a 1.0 s gap tonight's count rises to 18
episodes (1.50/min); at 3.0 s and 5.0 s it stays at 16 — the number is stable across a 5×
change in the gap above 2 s and only moves at 1 s. Also: the bucket rows depend on the
`describe` tag in each tape header; a tape written from a dirty tree, or a rebuilt exe that
did not re-stamp, mis-buckets and the per-kernel column is wrong. The per-bucket rows are
**not a controlled comparison** anyway — different terrain, different intent, wildly
different sample sizes (v14 is 4.6 min and one episode; it means nothing).

### R3-M2 — A crash costs him a **median 7.0 s**, p90 18 s, worst 38 s — and that is a floor, not a ceiling

Failure cost as Juul defines it (time lost), measured on v17: from past-90 entry until
ground speed returns to 80 % of what he carried into the crash (median pre-crash speed
**22.2 m/s = 80 km/h**):

- **p50 7.02 s, p90 18.13 s, max 38.41 s, mean 11.04 s** over 11 scored episodes
- **4 of the 15 scorable episodes never regained 80 % of pre-crash speed before the tape
  ended** — right-censored, so the true median is **higher** than 7.0 s

Sensitivity (same data, metric parameters varied):

| variant | n scored | censored | p50 | p90 |
|---|---|---|---|---|
| threshold 50 % of pre-speed | 15 | 0 | 6.22 s | 35.21 s |
| threshold 60 % | 14 | 1 | 7.45 s | 36.21 s |
| **threshold 80 % (headline)** | 11 | 4 | **7.02 s** | 18.13 s |
| threshold 100 % | 8 | 7 | 10.21 s | 13.42 s |
| 80 %, episode gap 1.0 s | 12 | 4 | 6.82 s | 14.61 s |
| 80 %, episode gap 5.0 s | 11 | 4 | 7.02 s | 18.13 s |
| 80 %, pre-window 1 s | 12 | 3 | 6.72 s | 14.41 s |
| 80 %, pre-window 5 s | 11 | 4 | 10.84 s | 37.11 s |

**Confidence:** MEASURED (the sweep is the evidence that the headline is not an artefact of
one threshold: every variant lands between 6.2 s and 10.8 s).
**Killing mutation:** censoring. If the four unscored episodes are ones where he gave up and
idled, the median is an underestimate; if they are ones where he *chose* to stop (tape
ended, he quit), they are not failure cost at all. Telling those apart needs his word, which
R3 does not have (§7 UNVERIFIED-1). Second mutation: regaining 80 % of speed is not the
same as being back in the fight — upright at speed but pointed the wrong way scores as
recovered.

Two component numbers, for a rung that wants to attack a specific part of that 7 s:

- **time upside-down per crash** (past-90 entry to the episode's last past-90 exit):
  v17 p50 **2.49 s**, p90 6.24 s, max 11.78 s; whole corpus p50 2.52 s, p90 7.18 s,
  max **57.05 s**.
- **resume-riding time** (D-B's predicate, per past-90 event): v17 p50 **3.81 s**,
  p90 6.25 s, max 31.40 s. Loosen the predicate to `tilt < 30°, speed > 2 m/s, hold 0.25 s`
  and it becomes p50 2.19 s / p90 3.53 s / max 5.92 s. **So roughly 1.5 s of the recovery
  is not getting upright — it is getting back up to speed.** (MEASURED; killing mutation:
  the predicate itself, which is what the four-variant sweep exercises.)

### R3-M3 — **71 % of past-90 events are re-rolls**, and that is invariant to how you define recovery

Of the 52 past-90 events on v17, only **15 (29 %) end in a resumed ride**; the other 37 are
followed by another past-90 event before the machine is upright for even half a second.

The important part: I re-ran the recovery predicate four ways —
`(20°, 5 m/s, 0.5 s)` baseline, `(20°, 2 m/s, 0.5 s)`, `(30°, 2 m/s, 0.25 s)`,
`(20°, no speed gate, 0.5 s)` — and the count of recoveries is **15 in every one of them**.
The recovery *times* move (p50 3.81 → 2.19 s); the recovery *count* does not move at all.

**Confidence:** MEASURED.
**Why it matters:** the 71 % is not an artefact of a strict "riding again" test. The machine
genuinely does not get upright between crossings. What the tape calls 52 rollovers is
16 crashes that each tumble about three times.
**Killing mutation:** the past-90 entry filter (0.10 s of `dot_up < 0`). If the sled
oscillates across ninety degrees faster than that — a shudder, not a tumble — crossings go
uncounted and the ratio changes. A probe logging roll angle at 1440 Hz through one episode
would settle it; R3 did not build one (§7 UNVERIFIED-3).

### R3-M4 — He reaches for the reset key in about **one crash in three**, and the tape already records it

- v17: **5 R presses / 12.02 min = 0.42/min** = **0.31 presses per crash episode**
- whole corpus: 146 presses / 206.1 min = 0.71/min = **0.41 per crash episode**
- v13g era: **1.08/min**; v15: 0.12/min
- inter-press gaps within a tape (n = 113): p50 **25.4 s**, p10 7.6 s, **16 % under 10 s**,
  54 % under 30 s. The long runs are in the long sessions: tape 8 has 23 presses in
  15.8 min, tape 14 has 21 in 18.0 min, tape 16 has 18 in 21.7 min.

**Confidence:** MEASURED.
**Reading it:** he rides out roughly two crashes in three. The reset key is not his default
answer to a roll — which makes **R-press rate a conservative proxy**: it undercounts
crashes and fires only when riding it out was not worth it.
**Killing mutation:** the `autoright_R` classifier (tilt before > 60° AND tilt after < 30°,
`sled_tape_audit.py` near line 711). An R press while the machine is only half-tipped is
classed `mount_or_other` and vanishes from this rate. And per `app/player_mode.h`,
`autoright_legal` gates the key — presses the guard rejects never reach the tape at all, so
a legality change silently changes what this metric means.

### R3-M5 — Landings are the least forgiving event in the game: **29 % land upright**

- v17: **17 of 58 landings upright (29.3 %)**, 4.82 air events/min
- whole corpus: **306 of 984 (31.1 %)**

**Confidence:** MEASURED (D-B's landing scorer).
**Why R3 cares:** an air event is the one failure the player *chose*. Chad asked for
"SLiding banging, punchy, jumps" (§0b) and "should be able to launch in the air"
(gi4 §1 item 2). A 29 % success rate on the thing he asked for by name makes the jump a
gamble rather than a skill.
**Killing mutation:** the landing-upright test and the air-event segmentation. If a landing
is scored at first ground contact rather than after the bounce settles, a sled that touches
upright and then tumbles counts as upright — and 29 % is optimistic, not pessimistic.

### R3-M6 — The steering command is used as a **switch**; the lean command is used as an **analog axis**

Command-occupancy histograms (fraction of ticks, weighted by tick count):

| corpus | abs(steer) < 0.05 | 0.05–0.95 | > 0.95 | abs(lean_lat) < 0.05 | 0.05–0.95 | > 0.95 |
|---|---|---|---|---|---|---|
| v17 tonight (86–91) | 61.7 % | **18.9 %** | 19.4 % | 18.6 % | **54.0 %** | 27.4 % |
| all 84 tapes | 72.4 % | **4.8 %** | 22.7 % | 16.4 % | **57.6 %** | 26.0 % |

**Confidence:** MEASURED.
**Reading it:** lean is genuinely modulated — more than half of all riding ticks sit at a
partial lean. Steering, even after SK-1c made A/D an analog accumulator, spends only 19 % of
ticks between the stops on v17 and under 5 % across the whole corpus. The whole-corpus
figure is **confounded**: most of those tapes predate the accumulator (the code comment in
`app/main.cpp` calls the old scheme "the old binary +/-1 instant full lock"), so the 4.8 %
is partly the old input map, not his hands. The v17 number is the honest one.
**Killing mutation:** the histogram bin edges (0.01 wide, so "< 0.05" is bins 0–4). Move the
dead-band to 0.02 and the "at zero" share falls; the conclusion survives any edge in
0.02–0.10 because the distribution is bimodal — p50 of abs(steer) is **0.005**, p90 is
**0.995**.

### R3-M7 — The control budget on the bars, counted from the shipped source

Read from `app/main.cpp` (this worktree, read-only). While `bars` is true the player holds,
simultaneously:

| channel | binding | law |
|---|---|---|
| throttle | `W` | ramp up 2.5/s, release 6.0/s |
| brake | `S` | direct |
| steer | `A` / `D` | accumulator 2.0/s, self-centres at 3.0/s when neither is held |
| lean lateral | mouse X | 300 px = full lean, **integrating, no self-centre** |
| lean fore/aft | mouse Y | 300 px = full, **integrating, no self-centre** |
| lean lateral (alt) | `Q` / `E` | accumulator 2.0/s, no self-centre |
| stand / tuck | `LShift` / `LCtrl` | state |
| recentre | `C` | zeroes lean and steer |
| autoright | `R` | scaffolding, gated by `app::autoright_legal` |

Two structural facts follow, both MEASURED from source:

1. **Three continuous primary axes** (throttle, steer, lean) are live at once, across two
   hands, plus a state change (stand) and a contextual key (C or R).
2. **Lean and look share the mouse.** The lean integration sits inside
   `else if (!live.freelook_held)`: while freelook is held the mouse looks and **lean stops
   being commanded**, frozen at its last value (there is no command-side return, and the
   kernel's own `lean_return_tau_s` is 0.0 in every tape header). Looking around and
   shifting your weight are mutually exclusive.

**Killing mutation:** any rebind. This is the input map in this worktree's `app/main.cpp` at
the audit commit. Also, if a gamepad path exists that I did not find, the "two hands"
framing is wrong — I searched `main.cpp` for the sled input assignment sites only.

### R3-M8 — Combat and riding are **serialized**, not concurrent

The sting/RPAS launch path refuses while mounted: `app/main.cpp` sets the note
`"RIGHT THE MACHINE FIRST"` when `player.mode == Sled` and `"GET UP FIRST"` otherwise, and
the launch click is gated on `sting_shouldered`, an afoot stance. No weapon fire is bound
while `bars` is true.

**Confidence:** MEASURED (source read).
**Why it matters, and this is the single most important framing in R3:** the battle does
**not** load the player's hands while riding. It loads the *clock*. Chad's "there is a
batttle going on as well" is therefore not a control-budget problem (§4 R3-L9) — it is a
**failure-cost problem** (§4 R3-L1): every second lost to a tumble is a second the war moves
without him.
**Killing mutation:** a build where the player can fire from the bars. Any such feature
inverts this finding and promotes the control-budget literature from background to blocker.

### R3-M9 — Session length is NOT a usable frustration proxy in this corpus

84 tapes, 206.1 min total. Median tape **1.31 min**; 27 % of tapes are under 30 s; 10 tapes
are at least 5 min and carry 56 % of all ride time. Clustered by file mtime into sittings
(gap > 60 min): **29 sittings**, from single 13–22 min drives (18–23 Aug) to tonight's
90.8 min wall / 12.0 min ride across six tapes.

**Confidence:** MEASURED — and **deliberately not used** as a frustration signal. These are
a developer's fly-tests, launched by his own build loop against whatever rung was in the
tree that hour; a short tape is usually a rebuild, not a quit. Using session length as
engagement here would be exactly the "harness number alone" the audit forbids. It is
recorded so a later strand does not rediscover it and draw the wrong conclusion. See §7
UNVERIFIED-2.

---

## 4. What the player-experience literature says (and how good the source is)

Sources I **fetched and read this session** are marked [read]; sources I have only through a
search engine's summary of the page are marked [second-hand] and are weighted accordingly.
Full URLs in §8.

### R3-L1 — The measure of difficulty is the TIME the failure costs, not how often you fail [read]

Juul, *In Search of Lost Time: On Game Goals and Failure Costs* (FDG 2010): "the amount of
*time* the player loses when failing is now the better measure of how difficulty impacts the
player". Punishment is enumerated as "draining the player's energy resources, subtracting a
life, forcing the player to start over" — all of which the paper reduces to lost play time.
He splits goals into **permanent** (progress survives; lost time is recoverable — BioShock)
and **transient** (the goal expires with the instance — losing a multiplayer match). Cost
also has a psychological component set by how failure is communicated and by how repetitive
the replay is.

**Confidence:** LITERATURE.
**Why it matters here:** SEADS's war objectives are *transient* goals in Juul's sense — a
pump under attack does not wait. That makes the 7 s median failure cost (R3-M2) more
expensive than the same seven seconds in a time-trial, and it is the formal version of
Chad's "there is a batttle going on as well".

### R3-L2 — Frustration does not make players try harder; it makes them switch tasks [second-hand]

Bowman, Keene & Jimenez Najera, *Flow Encourages Task Focus, but Frustration Drives Task
Switching* (CHI 2021). Cognitive and physical demands were rated lowest in boredom (high
reward / low effort), moderate in flow (balanced), highest in frustration (low reward / high
effort). Response times to a **secondary** task improved most in the frustrating condition:
players "initially tr[ied] to master the game's over-challenging primary task before giving
up and, instead, divert[ed] attention toward a secondary task" that "required less effort
and thus gave greater attentional rewards".

**Confidence:** LITERATURE, second-hand (the ACM full text returned HTTP 403 to me; this is
the abstract-level summary and I did not read the method section).
**Why it matters here:** in a game where the sled is transport and the war is the objective,
a frustrating sled does not produce a player who practises riding. It produces a player who
stops using the sled — walks, or flies, or plays the war from the gun. That is a *silent*
failure mode: engagement stays up while the vehicle you spent a year on goes unused.

### R3-L3 — A racing team that shipped fixed exactly these two complaints: disorientation and unfair crashing [read]

*Game Design Deep Dive: situational awareness and player frustration in GRIP*
(Game Developer). The two named sources of frustration inherited from Rollcage were
**disorientation** after a crash and **unfair crashing**: "Small bits of scenery can send you
spinning or flying. This is the physics element of the game, and it's what would happen if
this were real. But it's not fun." Better players had built a mental map and recovered;
newer players had not. Their fixes: a minimal map, near-subliminal HUD direction arrows,
screen lighting so you "head towards the light"; **angular momentum scaled against speed** so
higher speed produces less violent direction change; and an **assisted landing** system that
uses physics prediction to help the player land favourably "while maintaining player control
through air movement options".

**Confidence:** LITERATURE.
**Why it matters here:** this is a shipped precedent for two mechanisms that are
arcade-lawful under Chad's §0b licence ("Make it less honest but dont ruin it") and that
attack R3-M3 (chained tumbles) and R3-M5 (29 % landings) directly — without a governor and
without taking the player's hands off the machine.

### R3-L4 — The retry has to be instant or the frustration compounds [second-hand]

Trials HD review (Gamecritics): instant restart after a wreck is "an absolute game-saver";
"If there was even a single second of pause, the frustration level would quickly spike to
intolerable levels." The design is explicitly "letting players try again and again as
painlessly as possible".

**Confidence:** LITERATURE, second-hand (review, not research; I have it through a search
summary).
**Why it matters here:** SEADS's equivalent of the restart is the R key, and it is not
instant in the relevant sense — the player first has to *decide* the tumble is over, and the
tumble is a median 2.49 s long (R3-M2) with a 71 % chance of another crossing (R3-M3).

### R3-L5 — The industry's answer to failure cost in driving games is a bounded rewind [second-hand]

GRID's Flashback (2008) let players "rewind the clock by ten seconds to cancel out mistakes",
rewarded not using it, and reduced the number available as difficulty rose; the mechanic
spread to F1, DiRT and Forza Motorsport 3 onward.

**Confidence:** LITERATURE, second-hand.
**Why it matters here:** it calibrates the budget. The genre decided that **about ten seconds**
is the size of a mistake worth erasing. SEADS currently *spends* a median 7 s (p90 18 s) per
crash, 1.33 times a minute, with no rewind. This is not a recommendation to add a rewind —
it is a yardstick for R3-P2.

### R3-L6 — Players want failure to be their fault; they lose that feeling when failure accumulates instead of happening once [second-hand]

From the failure-design literature (Foch & Rice, *"The game doesn't judge you"*, and the TCD
dissertation on "valid" failure, plus Game Developer's *Figuring out Failure in Game
Design*): players in difficult games show a strong internal locus of control and blame
themselves rather than the system — **but** when failure comes from an accumulation of small
mistakes rather than one identifiable mistake, they are *less* likely to feel responsible.
The more abstract the system, the more informative the feedback has to be, or the player
cannot learn and becomes frustrated. Notably, players **prefer** games where they feel
responsible for failing.

**Confidence:** LITERATURE, second-hand.
**Why it matters here:** this is the theoretical name for R3-M3. A crash made of 3.25 tumbles
in 2.5 s, entered at 22 m/s, is an accumulation, not an identifiable mistake. The design
question is not only "how often does he roll" but "**can he name what he did**".

### R3-L7 — Immediate retry is what lets players reframe failure as learning [second-hand]

*Fail, fail again, fail better* (IJHCS 2023; ten participants played Celeste for a week, then
a 30-minute semi-structured interview): the design that allows immediate retry supports
persistence; most participants felt failure was barely possible because progress and eventual
completion were protected — "There really isn't any failing, you just have to keep on trying
over and over again."

**Confidence:** LITERATURE, second-hand; N = 10 qualitative, so it is a mechanism story, not
an effect size.

### R3-L8 — Skill floor and skill ceiling are separate dials, and assists belong to the floor [second-hand]

The skill ceiling is "the upper limit of skill expression in a game or game mechanic"; depth
is asked as "how much can a player theoretically improve before there is no way to get
better". The enduring vehicle games "combine instant accessibility with long-term mastery",
and predictable physics, readable speed and consistent feedback are what "establish trust".

**Confidence:** LITERATURE, second-hand (industry explainer, not research).
**Why it matters here:** this is Chad's ruling in the industry's vocabulary. "Just allow the
balance of body mechanism to **ENHANCE** ability, i.e. tighten a turn instead of having to be
the necessary condition of not rolling over" (§0) is precisely *lean belongs to the ceiling,
not the floor*. A change that makes lean **required** raises the floor; a change that makes
lean **rewarding** raises the ceiling. Every candidate rung can be sorted by that test alone.

### R3-L9 — There is a hard limit on simultaneous continuous actions, and vehicle-combat games breach it [read]

*Designing Game Controls* (Game Developer): "the maximum limit of simultaneous actions (for
each hand) is: One primary action, One state change, One contextual action". Its worked
example is GTA 5 vehicle combat — "car steering (primary action), accelerate/brake (primary
action), shooting mode (state change), aiming and shooting (primary action)" — where "the
player has to use three primary actions simultaneously" and the verdict is: "Technically,
it's possible to do it with the gamepad, but the attention limit doesn't allow to do it
effectively." The general rule: "There's always human attention limit for a number of
simultaneous actions, even if they're physically possible."

**Confidence:** LITERATURE (practitioner heuristic, not measured psychology — it is a rule of
thumb with a plausible mechanism, not a published limit).
**Why it matters here:** SEADS riding is already at three continuous primary axes (R3-M7) —
throttle, steer, lean — the same count the article calls over budget, **and** lean shares the
mouse with looking. The saving grace is R3-M8: you cannot shoot from the bars, so the
breach is not compounded by combat. If a mounted weapon is ever added, this becomes the
dominant constraint.

### R3-L10 — Analog body-weight controls cost the player one to two hours of unlearning, and shipped games stage that cost [second-hand]

Skate put "all meaningful controls on the controller's two analog sticks and triggers";
Skater XL maps left and right sticks to left and right feet, which reviewers describe as
"initially overwhelming because games have ingrained the idea that the left analog stick is
for general movement", taking "about an hour or two to override that muscle memory".
Descenders is described as easy to pick up with a tutorial for the basics and **advanced
controls introduced only after a region is completed** — staged onboarding — while still
being "hard to master".

**Confidence:** LITERATURE, second-hand (reviews).
**Why it matters here:** SEADS asks for the same class of unlearning (the mouse is lean, not
look) and currently stages nothing. Chad himself flagged the coupling problem —
"I dont like the steering via mouse coupled with lean they have to be two independent things"
(SK-1d, quoted in `app/main.cpp`) — and the fix he got was a decoupling, not an onboarding.

---

## 5. Where the literature meets the tape — four claims that matter

**C1 — The problem is not the crash rate, it is the crash SHAPE.** One crash per 45 s
(R3-M1) is high but not disqualifying for an arcade machine at 80 km/h in trees; Chad
explicitly asked to "Allow for bad driving too" and to "Make it possible to roll but not the
rule" (§0/§0b). What breaks the deal is that each crash is 3.25 tumbles (R3-M1), 71 % of
crossings are re-rolls (R3-M3), and the literature says an accumulation of small failures is
exactly what stops a player feeling responsible (R3-L6) and what a shipped racer chose to fix
by scaling angular momentum against speed (R3-L3). **A rung that cut tumbles-per-crash from
3.25 to ~1.3 without changing the crash rate would satisfy his ruling more precisely than a
rung that made the machine harder to roll.**

**C2 — The failure-cost budget is about one GRID flashback, and SEADS is already spending
it.** Median 7.0 s, p90 18.1 s, censored low (R3-M2), against a genre yardstick of ten
seconds worth *erasing* (R3-L5) and a fast-retry genre that will not tolerate one second of
pause (R3-L4). Under Juul (R3-L1) the cost is worse than it looks because the war's goals are
transient.

**C3 — The reset key is the cheapest honest instrument this project owns.** It is already in
the tape as an `O` record, it is already classified, and it is *scaffolding by Chad's own
definition* ("a key for now that lets me autoright…"). At 0.31–0.41 presses per crash
(R3-M4) it is conservative: it only fires when riding it out was not worth it. It should be
a standing acceptance number on every comfort rung, reported beside his word — never instead
of it.

**C4 — Lean is already the expressive channel; protect it and reward it, never require it.**
Lean sits between the stops for 54 % of riding ticks while steer is bimodal (R3-M6); Chad
calls lean "part of what is so fun on the road" and rules that it must ENHANCE rather than be
"the necessary condition of not rolling over" (§0). In R3-L8's vocabulary: lean is the
ceiling, and the audit's candidate rungs should be sorted by whether they raise the ceiling
or the floor.

---

## 6. R3's advisory items (each cites a tape event AND one of Chad's words)

These are **advice for the ladder, not rungs**, and no dial value is proposed — R3 has no
standing to pick one. Each names the acceptance metric and its killing mutation so a builder
can tell whether the rung worked.

### R3-P1 — Attack the chain, not the threshold

**Tape event:** 37 of 52 past-90 events on v17 are re-rolls; only 15 end in a resumed ride,
invariant across four recovery predicates (R3-M3).
**Chad's words:** "allow me to land on my skis more often after a roll (even though R works).
But not every time — allow it to happen. Make it possible to roll but not the rule" (§0);
"Leaning shall enhance the ride an just make it more stable and slef righting by chance more"
(§0b).
**Shape:** whatever the mechanism, it should act **after** the first past-90 crossing, not
before it — nothing that makes the first roll less likely, everything that makes the second
one less likely. That is literally "self righting by chance more" and it leaves "possible to
roll" untouched. GRIP's speed-scaled angular momentum is the shipped precedent (R3-L3).
**Acceptance metric:** tumbles-per-crash (past-90 events ÷ merged episodes), target ≈ 1.3
against tonight's 3.25, measured on his next tape, with the crash-episode rate NOT falling
below about 1/min (if crashes stop happening, the rung has broken his ruling).
**Killing mutation:** the 2.0 s episode merge; re-run at 1/3/5 s and the ratio must move
less than the effect.

### R3-P2 — Give the failure cost a written budget

**Tape event:** median 7.0 s, p90 18.1 s, 4 of 15 censored, at a pre-crash speed of 22 m/s
(R3-M2).
**Chad's words:** "DOnt make it impossibly hard, there is a batttle going on as well" (§0b).
**Shape:** adopt a number in the packet so future rungs can be judged: e.g. **p50 ≤ 4 s and
p90 ≤ 8 s** from past-90 entry to 80 % of pre-crash speed. (GUESS on the specific figures —
they are half of today's, which is a reachable step, not a derived optimum; the genre
yardstick in R3-L5 is 10 s and Chad's own tolerance is unknown until he rules.)
**Killing mutation:** the 80 % threshold and the censoring. Both are swept in R3-M2; a rung
that improves p50 while increasing censoring has not improved anything.

### R3-P3 — Make the crash legible (this is feedback work, not kernel work)

**Tape event:** 2.49 s upside-down per crash with 3.25 crossings (R3-M1/M2) means the player
cannot attribute a crash to one input; 29 % of landings are upright (R3-M5).
**Chad's words:** "the balance mechanism works really good" (§0) — the thing that works is the
one he can feel; the rolls he described as "flips happen too fast and easy" (gi4 §1 item 6)
are the ones he cannot.
**Shape:** GRIP's answer was informational, not physical (R3-L3); the failure literature says
abstract systems need more informative feedback or the player cannot learn (R3-L6). This is
strand D-D's territory (camera, HUD, audio, roost) and R3's contribution is only the priority
claim: **legibility of the crash is worth more than a further reduction of the crash rate**,
because his ruling forbids removing the crash.
**Acceptance metric:** not a harness number. This one has to be his word after a drive.
**Killing mutation:** none available — flagged deliberately as the one item R3 cannot measure.

### R3-P4 — Adopt the R key as a standing instrument, and do not remove the scaffolding yet

**Tape event:** 0.31 presses per crash on v17, 0.41 corpus-wide, p50 25 s between presses,
16 % under 10 s (R3-M4).
**Chad's words:** "I need a key for now that lets me autoright until we get the guy running
back to the snowmachine" (`app/main.cpp`); "allow me to land on my skis more often after a
roll (even though R works)" (§0).
**Shape:** report R/min and R-per-episode on every future comfort rung's gate line, from his
tape, beside his verbatim word. Never tune *to* it.
**Killing mutation:** the `autoright_R` classifier thresholds and `autoright_legal`'s gate —
both change what a press means without changing the count's name.

### R3-P5 — Stage the onboarding of the lean control before widening it

**Tape event:** lean occupies the intermediate band 54 % of riding ticks while steer is
bimodal at 19 %/62 % (R3-M6); lean and look share the mouse and lean freezes under freelook
(R3-M7).
**Chad's words:** "I dont like the steering via mouse coupled with lean they have to be two
independent things, part of what is so fun on the road is driving by lean" (SK-1d, in
`app/main.cpp`); "I do want the finess and all the mechanisms available for tuning" (§0b).
**Shape:** analog body controls cost about an hour or two of unlearning and shipped games
stage the advanced half (R3-L10). Nothing here asks for a change to the transfer function —
it asks that any future change be judged as an *onboarding* question as well as a feel
question, and that the freelook/lean exclusion be named as a known cost rather than
discovered later.
**Killing mutation:** a rebind falsifies R3-M7 entirely.

---

## 7. UNVERIFIED — what R3 could not establish

- **UNVERIFIED-1 (the big one).** Chad has given **no felt report on tapes 86–91**. Every
  v17 number in §3 is behaviour without an attribution. In particular I cannot tell a crash
  he shrugged off from a crash that made him quit the tape, which is exactly the
  distinction the censored failure-cost episodes turn on (R3-M2). The audit plan asks him
  for one line per tape (§6 item 2); until he answers, **no number in this file licenses a
  dial move**.
- **UNVERIFIED-2.** Session length, tape count and sitting length are measured (R3-M9) but
  are **not** engagement evidence for a developer's own fly-tests. Any later strand that
  wants a session-length proxy needs a player who is not Chad.
- **UNVERIFIED-3.** The past-90 entry filter (0.10 s) is inherited from D-B. I did not
  build the 1440 Hz roll-angle probe that would show whether fast oscillations are being
  missed, so "3.25 tumbles per crash" is a count of *sustained* crossings only.
- **UNVERIFIED-4.** The CHI 2021 flow/frustration paper (R3-L2) is abstract-level only —
  ACM returned HTTP 403 — so I have not seen its N, its effect sizes, or its secondary-task
  design. The claim is used as a mechanism hypothesis, never as a measured effect.
- **UNVERIFIED-5.** The GDC 2018 *Vehicle Feel Masterclass* deck (Harris, Criterion) is the
  obvious primary source for assists and is cited in §8, but WebFetch refused it (over the
  10 MB content limit). Strand R2 reports reading a local text extraction; **I did not read
  it**, and nothing in this file rests on it.
- **UNVERIFIED-6.** The per-kernel table in R3-M1 is observational. Terrain, intent and
  sample size all differ between buckets; the v15 row (0.78 episodes/min) and the v17 row
  (1.33) are not an A/B of v15 against v17.
- **UNVERIFIED-7.** Two sources for the failure-attribution claim (R3-L6) are a dissertation
  and a trade article that I have only in summary. The claim "players prefer to feel
  responsible" is repeated here because it is consistent across three independent summaries,
  not because I verified a study.
- **UNVERIFIED-8.** I did not check whether a gamepad or any alternate input path exists for
  the sled; R3-M7 describes the keyboard/mouse path in `app/main.cpp` only.

---

## 8. Sources

Fetched and read this session:

1. Jesper Juul, *In Search of Lost Time: On Game Goals and Failure Costs*, FDG 2010 —
   https://jesperjuul.net/text/losttime/
2. *Game Design Deep Dive: Situational awareness and player frustration in GRIP*, Game
   Developer —
   https://www.gamedeveloper.com/business/game-design-deep-dive-situational-awareness-and-player-frustration-in-i-grip-i-
3. *Designing Game Controls*, Game Developer —
   https://www.gamedeveloper.com/design/designing-game-controls

Used at abstract / search-summary level only (second-hand, weighted down in §4):

4. Bowman, Keene & Jimenez Najera, *Flow Encourages Task Focus, but Frustration Drives Task
   Switching*, CHI 2021 — https://dl.acm.org/doi/10.1145/3411764.3445678 (full text HTTP 403)
5. *Fail, fail again, fail better: How players who enjoy challenging games persist after
   failure in "Celeste"*, IJHCS 2023 —
   https://www.sciencedirect.com/science/article/pii/S1071581923002082 (HTTP 403)
6. Foch & Rice, *"The game doesn't judge you": game designers' perspectives on implementing
   failure in video games* — https://dl.acm.org/doi/fullHtml/10.1145/3555858.3555868
7. *"Valid" Failure in the Design of Digital Games*, TCD dissertation 2018 —
   https://publications.scss.tcd.ie/theses/diss/2018/TCD-SCSS-DISSERTATION-2018-064.pdf
8. *Figuring out Failure in Game Design*, Game Developer —
   https://www.gamedeveloper.com/design/figuring-out-failure-in-game-design
9. Brad Gallaway, *Trials HD* review, Gamecritics —
   https://gamecritics.com/brad-gallaway/trials-hd-review/
10. *How Race Driver: Grid's rewinding time … made it a beloved racer*, GamesRadar —
    https://www.gamesradar.com/race-driver-grid-retrospective/ ; Flashback mechanic —
    https://racedriver.fandom.com/wiki/Flashback
11. *Skill Ceiling & Skill Floor: How to Make Your Game Easy to Learn but Hard to Master* —
    https://gamedesignskills.com/gaming/skill-ceiling-skill-floor/
12. *Skater XL* review, Engadget —
    https://www.engadget.com/skater-xl-ps4-easy-day-studios-review-133011739.html ;
    *Skate* review, GameSpot — https://www.gamespot.com/reviews/skate-review/1900-6180061/
13. *Descenders* reviews — https://godisageek.com/reviews/descenders-review/ and
    https://www.psu.com/reviews/descenders-ps4-review/
14. Jesper Juul, *Fear of Failing? The Many Meanings of Difficulty in Video Games* —
    https://jesperjuul.net/text/fearoffailing/ (listed for the ladder; not read this session)

Cited but NOT read by me (see §7 UNVERIFIED-5):

15. Matthew Harris (Criterion), *Vehicle Feel Masterclass: Balancing Arcade Accessibility
    with Simulation Depth*, GDC 2018 —
    https://media.gdcvault.com/gdc2018/presentations/Harris_Matthew_VehicleFeelMasterclass.pdf

Project sources (read-only, this session):

16. `Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0, §0b (main tree)
17. `Game_loop_idea/vehicle_program/SLED_RIDE_AUDIT_PLAN_20260917.md`
18. `docs/gi4_ride_handoff.md` §0, §1 (audit worktree)
19. `docs/sled_audit/tape_summary.json` (strand D-B, 2026-09-17T23:43)
20. `tools/sled_tape_audit.py` (strand D-B) — read, copied to scratchpad, **not edited here**
21. `app/main.cpp`, `app/player_mode.h` (audit worktree, read-only)
