# STRAND D-C — FELT-HISTORY VERIFICATION (independent rebuild)

**Scope.** Every verbatim Chad verdict on the SLED (ride, controls, rider, sled camera),
in date order, with doc + section; then three tables — SIGNED / OPEN / CONTRADICTIONS —
where every "on main" claim is checked against the git graph, not against a doc's own
assertion.

**Read-only.** No `sim/`, `control/`, `config/`, `test/golden/` file was touched. Nothing
built. No ctest run. No tape replayed (that is strand D-B). Tapes were only `ls`-ed.

---

## §0 METHOD, AND WHAT "ON MAIN TODAY" MEANS HERE

**⚠ THE AUDIT WORKTREE'S LOCAL `main` REF IS STALE.** In
`D:/seads_sandboxes/sled-audit`, `main` = `de523afd1` (road-repair LANDED, 2026-09-17
18:19), while `origin/main` = `533c86409` (terrain-clip sentinel pre-audit packet,
2026-09-17 22:44). **Every verification below was run against `origin/main`**, the tip the
repo actually carries. A reader who repeats this work against the local `main` ref gets
answers that are four hours and one merge stale.

- **On-main test** = `git log --reverse -S"<token>" origin/main -- <path>`; a commit that
  prints is reachable from `origin/main`, i.e. it is on main today regardless of which
  sandbox branch first carried it. This is the test that matters: several rungs below are
  recorded in their own memory/handoff as "NOT PUSHED", and are on main anyway.
- **Shipped value** = `git show origin/main:<path>`, read by hand.
- Confidence vocabulary: **MEASURED** (I read the bytes / ran the graph query),
  **DERIVED** (follows from two MEASURED facts), **LITERATURE**, **GUESS**.

**The single most load-bearing MEASURED fact in this strand:**

> `git log --no-merges origin/main -- sim/sled.cpp sim/sled.h` → newest is
> **`2a4ff3cba`, 2026-09-01**, "R4c-2a: measure how often he actually comes off".
> **The sled kernel has not changed in 17 days.** Tapes 86–91 (2026-09-17, 20:11–21:39)
> were therefore driven on the 2026-09-01 kernel.
>
> *Killing mutation:* any non-merge commit touching `sim/sled.cpp` or `sim/sled.h`
> reachable from `origin/main` dated after 2026-09-01. There is none. Same for
> `sim/rider_grip.*` (newest non-merge `773817e08`, 2026-09-01) and for the values in
> `[sled_comfort]` (last value change `8e8c6f67d`, 2026-08-26; `6e1e57ce6` on 2026-09-01
> touched `config/scenario.toml` elsewhere).

**Corpus fact (MEASURED).** `D:/flight_sim2/seads-recon/build-play/*.sledtape` = **91**
files; 86 (20:11), 87 (20:15), 88 (20:38), 89 (20:41), 90 (20:47), 91 (21:39), all
**2026-09-17**. *Killing mutation:* a 92nd tape, or an mtime on 86–91 that is not 09-17.

---

## §1 THE VERBATIM LEDGER, IN DATE ORDER

Quotes are byte-verbatim from the cited file, Chad's own spelling and typos preserved.
Where a quote survives only inside a commit message, the SHA **is** the citation.

### 2026-08-10 — the depth ruling (the fixed input the whole ride is tuned inside)
> *"Yes 0.77 m seems right to me for snow depth... Hopefully the snowmachine doesn't just
> plow it but that it can get on top and plane out, though still plowing some granted."*

`Game_loop_idea/WINTER_LAW.md` §"DEPTH SIGNED BY CHAD, 2026-08-10" (line 178).
The law's own consequence clause: *"The signed world is 0.77 m; the sled must be tuned to
plane in it. Ride feel at S3 is a `sim/sled.*` problem, not a snowpack problem."*

### 2026-08-11 — the vehicle roundtable
> *"approved as recommended, all seven — proceed to rung 1."*

memory `sled-system-registry`; rulings folded to `WINTER_LAW.md` §3.8 + `winter_plan.json`
RT-1..RT-7. Not a ride verdict; it is the authority the ride program runs under.

### 2026-08-12 — DRIVE 1: not driveable blind
> *"looks nothing like a sled"* · *"I'm a visual learner and need to see it to drive it"* ·
> *"going backwards it seemed"* · *"didn't see my mouse"*

memory `sled-system-registry` §DRIVE 1. **NOT SIGNED.** Physics existed; the machine was
not drawn.

### 2026-08-12 — DRIVE 2: the weight bug
> *"mouse is not moving with my mouse"* · *"rolls very easily"* · *"shroud in the asphalt"*

memory `sled-system-registry` §DRIVE 2 (fix `759718d48`). **★ THE ROLL COMPLAINT IS
CONFOUNDED BY RULING** — drive-mode weight consumed `pending_dx/dy`, so he could never
counter-lean. OPEN-D2-ROLL made drive 3 re-judge roll before any kernel roll-geometry work.

### 2026-08-12 — DRIVE 3: one bug, three findings
> *"skis don't turn when I turn my arms"* · *"head stays still at all times"* · standing
> *"doesn't look right"*

memory `sled-system-registry` §DRIVE 3 (fix `7083d3c12`). Ruling attached: roll forgiveness
+ track fishtail **wanted but DEFERRED**, *"don't ruin any tuning"*.

### 2026-08-12 — ★★★ DRIVE 4: THE FEEL SIGN AND THE ROLL RULING (the acceptance bar)
`Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` **§0**:
> *"Yep I can do backflips, but she's too unsteady. I rule that it should be roll
> resistant. Let me slide around a bit arcade but allow me to land on my skis more often
> after a roll (even though R works). But not every time — allow it to happen. Also I've
> seen lots of snowmobiles drift around corners and then with throttle, straighten out.
> The feel is really good. Don't lose the feel, except the constant rolling. Make it
> possible to roll but not the rule. However the game feels like a nicely made sim as
> there is dynamic room and the balance mechanism works really good. Just allow the
> balance of body mechanism to ENHANCE ability, i.e. tighten a turn instead of having to
> be the necessary condition of not rolling over. Just allow leaning a certain way in a
> particular condition to be the OPTIMAL weight distro for better traversing, say up a
> hill or around a corner. Work on the math to achieve a balance of fun and accuracy to
> real physics, just as our airplane ontological counterpart does."*

### 2026-08-12 night — ★★★ THE SUPERSEDING RULING (no governor)
`ROLL_COMFORT_HANDOFF.md` **§0b**:
> *"no I dont like thge idea in the handoff at all! It will ruin the feel to have a
> governor. Make it less honest but dont ruin it. Get a fable consult and the measure of
> it shall be if my intent is heard. Leaning shall enhance the ride an just make it more
> stable and slef righting by chance more. It should be arcadey to a degree so that it is
> fun., SLiding banging, punchy, jumps. Just make it more stable."*

and, same section:
> *"please build autmatically and make this kernel way better ... I am afraid it might
> ruin something that is reallt good right now but just dont ruin it. Allow for bad
> driving too but keep the benefit there for good riding. DOnt make it impossibly hard,
> there is a batttle going on as well. And the point is this should be fun as well, not a
> true sim. But I do want the finess and all the mechanisms available for tuning, so lets
> stop messing around abnd build something good for real. ... please knock this out of the
> park and make it awesome, keeping the good but making it easier / more fun?"*

### 2026-08-12 — RC1 DRIVEN AND REJECTED
> weak brakes · still tips/corkscrews (his bar: **360-spin a road WITHOUT rolling**) ·
> banks too sinky · *"sunken to its engine on the road"* · post-roll **spins like a top
> ~10×** · *"get the physics right"* · rebuild programmatically (Sonnet codes / Opus
> checks / Fable masterminds)

memory `roll-comfort-rc1` ¶1; root causes in
`Game_loop_idea/vehicle_program/RESEARCH_PACKET_C_GROUND_INTERACTION.md`.
★ The post-mortem line worth carrying forward: **RC1 "felt no different" because it tuned
the assist on the same wrong 0.354 m mount arm + the same C2 over-authority.**

### 2026-08-13 — GI DRIVE 1: four verbatim findings + a ruling
`Game_loop_idea/vehicle_program/GI_DRIVE1_HANDOFF.md` **§0**:
1. *"The machine now sits on the deck."* — **SIGNED BY DRIVE** (deck registration).
2. *"Brakes work on the road pretty good now."* — **SIGNED BY DRIVE** (brake authority).
3. *"I tried a 360, still rolls over way too easily."* — **OPEN. And the gate says green.**
4. *"Corkscrew is alive and well."* — **OPEN.**
5. *"If I lean back and hit a snowbank, I flip multiple times end over end... like 5x. Not
   good. I DO want a wheelie when standing and leaning back — that should be even MORE
   so."* — **OPEN, WITH A RULING: the wheelie is the feature, the unarrested tumble is the
   bug. Never fix it by weakening lean authority or softening the bank.**

### 2026-08-13 — GI4: the 512 s tape, six felt items
`docs/gi4_ride_handoff.md` **§1** (the table), each quote his:

| # | his words | measured on `sled_tape_4` |
|---|---|---|
| 1 | *"tips over too easy"* | 56 rollovers past 90° in 8.5 min = one per 9 s; 19.0 % of the drive rolled |
| 2 | *"should be able to launch in the air"* | 40 air events; **2 of 40 land upright**; launch pitch 36–80° |
| 3 | *"turning too unstable, can't hold a carve"* | true full lock: Bush 373 m radius, Bush 20+ 904 m, Road 20+ 240 m, vs kinematic 3.10 m |
| 4 | *"WOT should lift the skis to ~30°"* | WOT+stand+lean-back pitch p50 **3.6°**, p95 10.8° |
| 5 | *"rider weight needs authority, not thrown into a spin"* | lean→yaw is noise; TrailMain 20+ median **204 °/s** snap-spin |
| 6 | *"flips happen too fast and easy"* | 25°→90° in median **0.20 s**, peak 654 °/s, `roll_damp_nms` was **0** |

**⚠ THE PROVENANCE DEFECT, HIS SIDE OF IT (`gi4_ride_handoff.md` §0):** the tape carries 19
`cparam` lines against a 25-line roster — **he drove a stale pre-`bdcc9c7b3` binary.** His
six words are evidence against the PRE-GI3 kernel, not against GI3.

### 2026-08-13 — the two rulings taken off that report
`b971d14a9` ("Chad's two rulings applied"): `roll_damp_nms` **0.0 → 800.0**, against the RC
consult's own do-not list, earned from item 6. The "raise the tip threshold" ask was
**SIZED at 1.75× against the 3.3× needed and REFUSED** — no measured geometry moved.

### 2026-08-15 — item 1 ruling
> *"ship at 1.0."*

`docs/gi4_ride_handoff.md` §10.5 ("ANSWERED 2026-08-15"). `plane_lift_split_frac`
0.0 → **1.0** (`cbf4882b4`). The agent recommended holding; Chad ruled ship. Price accepted
and carried: `sled_lean_into_the_carve_tightens_the_radius` ratio 0.9003.

### 2026-08-21 — ★★★ THE WEIGHT-SHIFT DRIVE SIGN
> *"EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST"*

memory `weight-shift-kernel-deferred` ¶1. This is the **only unqualified sled drive sign in
the whole record after drive 4.**

### 2026-08-24 — the traction ruling (GI4 item 2)
> *"at high speeds, getting pulled in and flipping out like 15x is not desirable so yes
> take care of that"* — and planing lift itself STAYS: *"it is important for traversing
> atop the snow"*, *"at slower speeds I think it is good too because [I'd] like to jump the
> snowbanks"*

`sim/sled.h` at `traction_mu` (origin/main, ~lines 961–1005), verbatim in the comment.

### 2026-08-25 — SK-1d: the controls rulings
`c2b7bfeeb` commit body, four rulings verbatim:
> *"I dont like the steering via mouse coupled with lean they have to be two independent
> things, part of what is so fun on the road is driving by lean."*
>
> *"steering is too slow."*
>
> *"make it pasted to his white/grey sweater. Currently it is on his head. make it slightly
> bigger to fit the whole back."*
>
> *"the handlebars arent necessary anymore"* + *"make the grid blue and the ball slag
> orange, glowing as the themed color code graphified into this codebase."*

### 2026-08-25/26 — the throw ruling
> *"If he holds on he stays on, if he lets go he will likely be thrown"* ·
> *"the force persists throwing the sudburian in the DIRECTION of the jolt"* ·
> *"thrown off in ANY direction if he supermans"* · *"what about the lean direction?"*

memory `r4a-throw-ruling`; "ragdoll" BANNED by SUDBURIAN_LADDER §7.6.

### 2026-08-26 — the seated self-right, and three drives of it
His spec, quoted verbatim into `config/scenario.toml` `[sled_comfort]`:
> *"IF ON THE SEAT AFTER A ROLLOVER PRESSING THE STAND BUTTON AS IN REGULAR SEATED
> OPERATION WILL RIGHT THE SLEIGHT ONTO ITS SKIS ... MACHINE NEEDS TO BE TIPPING OVER ...
> NOT ABOVE 5KM/H ... IT CAN FAIL TO RIGHT GIVEN THE SITUATION, PROGRESSIVE HOLD"*

plus *"STANDING AUTOMATICALLY HELPS PUSH YOU OVER RIGHTED"* (quoted in `c31e07395`).

- **drove v1:** *"no it didnt work at all, never got the standing function to [right it] ...
  pushes."* (`8e8c6f67d`)
- **drove v3:** *"it works now, but a multiple press from full inversion should not be able
  to right it and it looks terrible."* (`c31e07395`)
- **drove v4, and it FAILED:** *"I dont understand why yo are clonlating the lean mechanism
  with the righting after the roll over, im not going to ask a player to lean with the
  mouse when inverted... leaning is for the flying off the handlebar direction."*
  (`31e0dd96a`, which separates THE THROW ← lean from THE RIGHTING ← stand, permanently.)

### 2026-08-27 — the gyro ruling
> *"is there mention of the gyroscopic effect on stability in this kernel?"*
> → on being shown it is absent and knowingly deferred:
> *"I think we should build it. Because then it has the foundation it needs."*

`docs/sled_gyro_spec.md` line 94, marked "Owner ruling, 2026-08-27 (Chad, verbatim)".

### 2026-08-29 — ★★★ THE BUCK RULING (the superman is not a defect)
Recorded verbatim in `tools/sled_probe.cpp` above `struct Air`, and in memory
`r4a-superman-instrument-rung`:
> *"i mean the buck, big bumps buck him up not off, yes the initiate of the superman is the
> bump the delta v, the acceleration, he may freefall but during that he would pull himself
> back to the seat, If it is too big a bump he may land in superman pose, that second,
> unseated hit from the landing would then cause his grip to let go. Not every time but if
> the bump is hard enough, then the superman should sustain until the landing and the
> hardness of the landing casue for letting go or the bars and falling off to the side."*

### 2026-08-30 — ★★★ THE BUCK SIGN
> *"yea great job, he dosent let go of the bars yet but his legs fly sometimes, just the
> right amount of novelty suprise."*

memory `r4a-superman-instrument-rung` §SIGNED. Shipped ON at `buck_gain 0.02` /
`decay_per_s 3.0` (`f11a7e12a`). **⚠ THE PAIR IS SIGNED, NOT THE GAIN.**

### 2026-08-31 / 2026-09-01 — the release, driven
> *"Falls off pretty easy"* · *"a little less"* · (earlier spec) *"maybe 10 % of jumps"* ·
> then the number: *"set it to 70"*

`docs/PLAN_20260901_r4c2_realism.md` §"One thing only you can rule" (line ~222) and
`773817e08` ("set it to 70"). Also from the same drive:
> *"he walks like a robot but good ... he needs to maintain [his speed] ... poof, im buried
> in the deep snow."* (`c88314b1e`)

### 2026-09-03 — the P key retired
No side-hang / PULL. **STAND alone rights the tipped sled**; any pull deferred
indefinitely. (memory `p-key-side-hang-rung`.)

### 2026-09-04 — respawn
> *"a way I can respawn in a snowmachine / plane"* (X held 1.5 s = give up)

`docs/SESSION_HANDOFF_20260903_game_loop_NEXT.md` line 38.

### 2026-09-05 — the sled camera ruling + the sled-sink report
- **sled cam:** default **1.45×** out of the near-plane (the hollow-model view) + wheel zoom
  3–30 m in chase AND freelook (`ad7266c59`, "Chad's four morning verdicts" #2).
- **on foot beside a capsized machine:** *"upon turning the sled over, he float walks 4 feet
  above the snow"* (`eada40091`); *"walking underneath the road up to my waist"* +
  *"floating 5 feet above the snow"* (`caf71ddc3`).
- **routed away and never answered by a sled lane:** *"Chad reports the SNOWMACHINE
  sometimes sinks into the plowed road"* — `docs/SESSION_HANDOFF_20260905_gait.md` §4
  item 7, re-routed in `docs/INFO_PACKET_20260906_pump_reach.md` line 52.

### 2026-09-09 — the camera sign
> *"really good"*

`docs/SESSION_HANDOFF_20260908_cam_smooth.md` line 78 ("exe 2026-09-09 and signed").
Covers the sub-tick blend and the **lagged sled camera**.

### 2026-09-10 — the speed lever
> *"more places with a bit less snow makes the sled go faster around there"*

`docs/SESSION_HANDOFF_20260910_road_repair.md` line 82.

### 2026-09-17, 20:11–21:39 — SIX DRIVES, NO WORDS
Tapes 86–91 exist (MEASURED, §0). **No felt report of any kind is on record for them** —
not in `docs/`, not in `LANES.toml`, not in a commit message, not in memory. The audit plan
itself asks for one (`SLED_RIDE_AUDIT_PLAN_20260917.md` §6 item 2, still unanswered).

---

## §2 TABLE A — SIGNED

"Signed" = his own words accepted a built thing. "On main" is verified by graph
reachability from `origin/main`, never by the doc's claim about itself.

| # | His word (verbatim, abbreviated) | Date | The dial / build it signs | On main today? SHA + when it landed | Confidence |
|---|---|---|---|---|---|
| S1 | *"Yes 0.77 m seems right to me for snow depth"* | 2026-08-10 | `[snowpack]` field → **p50 0.77 m** bush median (`base_m 0.85`) | **YES** — `bd0a0c597` 2026-08-10 set `base_m = 0.85`, unchanged since (`config/world.toml:554`) | MEASURED |
| S2 | *"The machine now sits on the deck."* | 2026-08-13 | deck registration (`deck_lift_m` into the radius sum) | **YES** — landed with the GI rung (`9e7e48a12` lineage); no revert on `origin/main` | MEASURED (presence) / DERIVED (no-revert) |
| S3 | *"Brakes work on the road pretty good now."* | 2026-08-13 | `brake_force_n 2600` + per-surface `mu_brake` budget | **YES** — `git show origin/main:sim/sled.h` → `brake_force_n = 2600.0` | MEASURED |
| S4 | the two rulings off the 512 s tape | 2026-08-13 | `roll_damp_nms` **0 → 800** (a ruling taken, not a sign of feel) | **YES** — `b971d14a9` 2026-08-13; 800.0 in both `sim/sled.h:383` and `scenario.toml` | MEASURED |
| S5 | *"ship at 1.0."* | 2026-08-15 | `plane_lift_split_frac` 0.0 → **1.0** | **YES** — `cbf4882b4` 2026-08-15; `sim/sled.h:1165` = 1.0 | MEASURED |
| S6 | *"EVERYTHING IS GOOD ON THE SNOWMACHINE DRIVE TEST"* | 2026-08-21 | K-WS1 **K1** honest stand-dependent aft clamp (ON) + the R3-WS pose ladder; `lean_aft_max_m 0.25 → 0.2183` | **YES** — `3f95dfc92` 2026-08-21 01:37 and `15c05cebb` both reachable; his signed tip `99622d956` (16:45) is a descendant. `sim/sled.h:802` = 0.2183 | MEASURED |
| S7 | the four SK-1d controls rulings | 2026-08-25 | mouse = lean @ **300 px**; A/D = steer @ **2.0/s push, 3.0/s self-centre**; `kSteerLeanFollow` DELETED; HUD on the sweater | **YES** — `c2b7bfeeb` 2026-08-25; `app/main.cpp:6494` (`px_full = 300.0`), `:8323` (`2.0 * frame_dt`), `:8331` (`3.0 * frame_dt`) | MEASURED |
| S8 | *"it works now"* (v3) + the separation ruling | 2026-08-26 | LEAN = THE THROW / STAND = THE RIGHTING; the righting reads the latched brace, never `in.lean_lat` | **YES** — `31e0dd96a` 2026-08-26 | MEASURED |
| S9 | *"I think we should build it."* | 2026-08-27 | rotor gyroscopics G1/G2 **built**, `k_gyro` master dial | **YES, BUILT** — `b8e430415` 2026-08-27; `sim/sled.h:755` `k_gyro = 0.0`, `:768` `k_gyro_react = 0.0`. ⚠ *the BUILD is signed, the VALUE is not* — see O6 | MEASURED |
| S10 | *"yea great job... just the right amount of novelty suprise."* | 2026-08-30 | **the buck PAIR** `buck_gain 0.02` / `decay_per_s 3.0`, shipped ON | **YES** — `f11a7e12a` 2026-08-30; `render/sled_model.cpp:6129-6138` defaults 0.02f / 3.0f, `SEADS_BUCK_GAIN=0` kills | MEASURED |
| S11 | *"set it to 70"* | 2026-09-01 | `rider_grip` `capacity = 70.0` | **YES** — `773817e08` 2026-09-01; `sim/rider_grip.h:244` = 70.0 | MEASURED |
| S12 | *"really good"* | 2026-09-09 | cam-smooth: sub-tick blend for sled/walker/sting + **lagged sled camera** (`tau_pos 0.06` / `tau_fwd 0.12`) | **YES** — rung 1 `5ce9564b4`, rung 2 `ec2e1a93a`, both 2026-09-10; `app/main.cpp:9670` | MEASURED |
| S13 | sled cam **1.45×** out of the near-plane + 3–30 m wheel zoom | 2026-09-05 | sled/sting camera defaults | **YES** — `ad7266c59` 2026-09-05 | MEASURED (commit body) |

**Killing mutation for the whole table:** for any row, `git log --reverse -S"<token>"
origin/main -- <path>` printing nothing (the thing never reached main), or
`git show origin/main:<path>` printing a different value than the cell states. Each row's
token is the dial name or literal quoted in it.

---

## §3 TABLE B — OPEN

"Open" = a word of his that a built thing has not answered, or a built thing his eye has
never been on. "What is still unpaid" states the next decision, not a recommendation — this
strand proposes no dials.

| # | His word (verbatim) | Date | What was measured against it | What is still unpaid today | Confidence |
|---|---|---|---|---|---|
| O1 | *"I tried a 360, still rolls over way too easily."* | 2026-08-13 | GI3 rollfix built + pushed (`bdcc9c7b3`) from the R3 sink trace of **his own taped roll** | **HE HAS NEVER DRIVEN IT.** He drove a pre-`bdcc9c7b3` binary an hour after it committed (`gi4_ride_handoff.md` §0). GI3 is on main and **unjudged five weeks later** | MEASURED (tape `cparam` count 19 vs roster 25) |
| O2 | *"Corkscrew is alive and well."* | 2026-08-13 | S4 trace: release band abandons mid-carve, sustained −6.5 rad/s; `release_floor_frac 0.3` shipped as the fix | Same as O1 — the fix is on main, unflown | MEASURED |
| O3 | *"If I lean back and hit a snowbank, I flip multiple times end over end... like 5x"* + *"I DO want a wheelie ... even MORE so"* | 2026-08-13 | `gi2tumble`: aft+stand **2.8 turns @18 m/s, 4.4 @20**; neutral seated 1.8 and RECOVERS | **THE TUMBLE RUNG WAS NEVER BUILT.** Deliberately fenced out of GI3 (§2 constraint set). The three numbers are still the only measurement | MEASURED (numbers) / DERIVED (never-built: no commit on `origin/main` names a tumble arrest) |
| O4 | *"WOT should lift the skis to ~30°"* | 2026-08-13 | probe: **6.0° Bush / 2.3° Road** (`gi4_ride_handoff.md` lines 273, 383); tape p50 3.6° | Unpaid. Thrust is pegged at `max_thrust_n 2272` and rpm at 8000 through the whole window — the ask is not a thrust shortage | MEASURED |
| O5 | *"turning too unstable, can't hold a carve"* | 2026-08-13 | M12 (2026-08-25, `c2b7bfeeb`): **"there is no carveable steer angle on Road at ANY speed tested"** — every steer 0.02–0.26 rolls past 140° in 12 s; zero steer holds 1.4° to 44.9 m/s | Unpaid. `plane_lat_gain 0.3` bought Bush 370 m → 33 m; Road still rolls in 8 of 8 powered-turn cells with GI3 on. **Road was 37.7 % of the 512 s tape** | MEASURED |
| O6 | *"I think we should build it. Because then it has the foundation it needs."* | 2026-08-27 | G1 armed: 0.6 rad/s yaw → **1.02 rad/s roll in 1 s (58 °/s)**; G2 blip → +34.1 °/s pitch, momentum ledger exact | **`k_gyro = 0.0` and `k_gyro_react = 0.0` on main.** The spec's own words: *"Chad has not flown it. Until he does, it is built, not approved."* | MEASURED |
| O7 | (no word — a dial awaiting one) | 2026-08-25 → today | `class_blend_m`: blend 0.0 → yaw −482.7°, roll 166.6°, **ROLLED**; blend 1.0 → yaw −11.6°, roll 52.7°, **no rollover** | **Ships at 0.0** (`config/world.toml:645`) with the comment *"AWAITING CHAD'S DRIVE -- not ruled."* A measured anti-rollover lever, off | MEASURED |
| O8 | the traction ruling *"take care of that"* | 2026-08-24 | GI4 item 2 built as `traction_mu`, a CONTACT ceiling on thrust; the attractor trace measured **62 N of 4106 N on the running surfaces at −28.7°** while the drivetrain is still pegged | **`traction_mu = 0.0` on main**, *"SHIPS AT 0.0 PENDING MEASUREMENT"*. His ruling is taken and the mechanism is off | MEASURED |
| O9 | GI4 §9.7 item 3 (floor the assist's `w_contact` gate) | 2026-08-13 | Named as the third leg of the three-legged loop (`gi4_ride_handoff.md` §10.5a) | **NEVER BUILT.** No `w_contact` floor dial exists in `origin/main:sim/sled.h` | MEASURED (absence by grep) |
| O10 | *"rider weight needs authority, not thrown into a spin"* | 2026-08-13 | `sled_lean_into_the_carve_tightens_the_radius` ratio **0.9003** at the shipped `plane_lift_split_frac 1.0` — misses the pinned 10 % by 0.03 pp; threshold widened to 0.93 so the leg is not a coin flip | Carried as a DEBT, on the board. Also: fore-aft `dN_ski/d(shift)` = **0.80×** W/L, the FLOOR of the ruled [0.8, 1.2] band (was 1.144× pre-pitch-split) | MEASURED |
| O11 | *"should be able to launch in the air"* | 2026-08-13 | **2 of 40 air events landed upright** on the 512 s tape | Unpaid; nothing on main since addresses landings. **⚠ STALE by five weeks — D-B must re-measure on 86–91** | MEASURED (historic) |
| O12 | *"Chad reports the SNOWMACHINE sometimes sinks into the plowed road"* | 2026-09-05 | Routed to world/game-loop, never to a sled lane. The Onaping SINK rung (`312643a45`…`51702109e`, 2026-09-11) fixed a **drawn-ribbon chord sag** across the road | **Unconfirmed either way.** The 09-11 rung is a DRAW fix for *"went into the road"*; whether it is the same symptom is not established and he has not re-reported. Precedent: 2026-08-12's road "sinking" was also a render-registration bug (machine drawn 0.43 m below the deck), not physics | DERIVED (same symptom class), **flagged** |
| O13 | *"more places with a bit less snow makes the sled go faster around there"* | 2026-09-10 | Scoped as groomed TRAIL (0.12 m) + wind-scour patches; **NEVER `base_m`** (WINTER LAW) | Not built as of `origin/main`; no commit names a scour patch or a new groomed segment near the pump/pond | DERIVED (absence by `git log --grep`) |
| O14 | (no word yet) | 2026-09-17 | Six drives, 20:11–21:39, on the **2026-09-01 kernel** | **NO FELT REPORT EXISTS FOR THE MOST RECENT SIX DRIVES.** Every felt attribution older than five weeks rests on the 512 s tape of a binary that lacked the rollfix. Largest single gap in the record | MEASURED |
| O15 | *"a multiple press from full inversion should not be able to right it and it looks terrible"* (v3) → v4 driven and FAILED | 2026-08-26 | `31e0dd96a` separated throw from righting; measured one-press from 178° at lean 0.0 (3.6°) and lean 1.0 (4.8°) — lean-independent | **No drive verdict exists on the post-`31e0dd96a` self-right**, and the value it runs at is unruled — see C2 | MEASURED |
| O16 | the four GI4 sled debts | 2026-08-13 → today | `generated/gate/known_reds.txt` on `origin/main` | **4 of main's 6 baseline reds are sled debts, by name:** `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_slides_before_it_tips_on_flat_snow`. The other two are AI. Red for five weeks | MEASURED |

---

## §4 TABLE C — CONTRADICTIONS

Where two documents, or a document and the shipped bytes, disagree. Each row states which
side the bytes are on.

| # | The disagreement | Side A | Side B | What the bytes say (MEASURED) | Killing mutation |
|---|---|---|---|---|---|
| C1 | **The trip band's shipped value** | `SLED_RIDE_AUDIT_PLAN_20260917.md` §2: *"Trip band drifted 50/60 (RC1 signed) to **70/80** shipped"*; `docs/gi_measurements.md` lines 663 and 781 also say 70/80 | `docs/gi_measurements.md` line 1630 (Phase S3): *"The trip leg's gate is updated to the MEASURED **60/70** boundary"* | **60/70.** `origin/main:test/unit/test_sled.cpp` ~2517: `REQUIRE_FALSE(rc_trip_rolls(p, f, 60.0)); REQUIRE(rc_trip_rolls(p, f, 70.0));` — **the plan's 70/80 is STALE by one phase.** The drift away from his signed 50/60 is real but one notch smaller than the plan states | those two `REQUIRE` lines reading 70.0/80.0 instead of 60.0/70.0 |
| C2 | **`right_assist_nm`: the comment derives 1500, the file ships 2400** | the comment immediately above it in `config/scenario.toml`: *"1500 gives a one-press ceiling of 18.6 + asin(1500/1932) = 70 deg... Chad rules the final value on his drive."* | the value on the next line | **`right_assist_nm = 2400.0`.** Comment and value entered in the SAME commit `8e8c6f67d` (2026-08-26): the prose argues 1500, the file ships 2400, 1.6× it. Twenty lines above, still true: *"⚠ THIS IS A STARTING POINT FOR CHAD'S DRIVE, NOT A FLOWN VALUE. Nobody has ruled it."* Kernel default is `right_assist_nm = 0.0` (`sim/sled.h:224`) | `git show 8e8c6f67d -- config/scenario.toml` not containing both `+#   1500 gives` and `+right_assist_nm          = 2400.0` |
| C3 | **"0.77 m is the depth" vs `base_m = 0.85`** | memory + several docs: *"depth 0.77 m SIGNED FIXED input, NEVER lower base_m"* — reads as if `base_m` were 0.77 | `origin/main:config/world.toml:554` `base_m = 0.85` | **NOT A REAL CONTRADICTION, BUT A STANDING TRAP.** 0.77 m is the **p50 of the depth DISTRIBUTION** the field produces over 79,847 DEM samples (p5 0.37 / p50 0.77 / p95 1.14 / p99 1.75); `base_m 0.85` is the flat-ground parameter that yields it. WINTER_LAW is explicit. Anyone who "restores base_m to the signed 0.77" would silently *shallow* the parameter while claiming to honour the ruling | `base_m` reading 0.77 on `origin/main`, or WINTER_LAW not printing the p5/p50/p95/p99 row |
| C4 | **"the four GI4 sled reds are the only gate reds"** | memory `gi4-ride-rung`: *"its FOUR sled debts = the only gate reds"*; several 2026-09-05 commits say *"sled reds == baseline four"* | every lane since 2026-08-30 reports *"the baseline **six** by name"* | Both true at their own dates. `origin/main:generated/gate/known_reds.txt` = **6 rows: 4 `snow` (the GI4 four) + 2 `ai`**. The sled four never changed; two AI reds joined | `known_reds.txt` on `origin/main` not containing exactly those four `snow` rows |
| C5 | **The GI4 gate denominator** | `gi4_ride_handoff.md` §§5a–10 quote **/1175** | its own ⚠ Doc correction at the tail: *"this tree discovers **1178** ctest cases, not the 1175 this handoff has been quoting"* | 1178 at that date; today's lanes report /2012–/2137. A reader taking /1175 from §5 is reading a number the same file retracts 600 lines later | the ⚠ correction paragraph missing from the tail of `docs/gi4_ride_handoff.md` |
| C6 | **"sled cam lag taus unswept"** | `SLED_RIDE_AUDIT_PLAN_20260917.md` §2 lists the taus as an open feedback debt | `docs/SESSION_HANDOFF_20260908_cam_smooth.md` line 78: exe 2026-09-09 **signed** *"really good"* | Both partly right. The pair **0.06 / 0.12 s** are compiled literals in `app/main.cpp:9670`, in **no TOML**, overridable only by `SEADS_SLED_CAM_TAU` — so "unswept and unconfigurable" holds; but the pair **did** take a felt PASS on 09-09. The plan overstates it as an untouched debt | `app/main.cpp` not declaring `static double tau_pos = 0.06, tau_fwd = 0.12`, or the cam-smooth handoff not carrying "really good" |
| C7 | **"K-WS1 NOT PUSHED"** | memory `weight-shift-kernel-deferred`: *"9 commits on `sandbox/gi4-ride` in winter-gi, **NOT PUSHED**"*, and `3f95dfc92`'s own note "(NOT pushed)" | the graph | **It is on main.** `3f95dfc92` is reachable from `origin/main`. The memory is a 2026-08-21 snapshot never updated. Same staleness class: `GI3_ROLLFIX_HANDOFF.md`'s "NOT pushed" header vs `bdcc9c7b3` being on main | `git log -S"k_air_shift" origin/main -- sim/sled.h` not printing `3f95dfc92` |
| C8 | **"GI3 was judged on a stale binary" vs "GI3 is the fix for his roll"** | `GI3_ROLLFIX_HANDOFF.md` header: fix shipped, "NEXT AUTHORITY: Chad's next drive" | `gi4_ride_handoff.md` §0: that next drive ran a binary predating it | The felt record contains **no drive of GI3 at all.** Every roll complaint on file is pre-GI3; every post-GI3 drive (2026-08-21 onward) produced words about the rider, the walk, the grip and the camera — **never about rolling**. Whether that silence is a pass or an unasked question is a ruling only he can make | a `cparam` count of 25 in `sled_tape_4`, or any post-2026-08-13 doc quoting him on rollover |

---

## §5 THE PRIOR SURVEY, CLAIM BY CLAIM

Independent corroboration verdict on `SLED_RIDE_AUDIT_PLAN_20260917.md` §2
("Measured-and-unmet asks on record").

| Prior-survey claim | Verdict | Evidence |
|---|---|---|
| "no carveable steer angle on Road at any speed" (M12); Road 37.7 % of the tape | **CORROBORATED** | `c2b7bfeeb` commit body (12 s hold, every steer 0.02–0.26 rolls past 140°); 37.7 % from `gi4_ride_handoff.md` §1 |
| Catwalk 6.0° Bush / 2.3° Road vs the 30° ask | **CORROBORATED** | `gi4_ride_handoff.md` lines 273, 383 |
| Tumble rung never built; gi2tumble 2.8 turns @18 m/s aft+stand | **CORROBORATED** | `GI_DRIVE1_HANDOFF.md` §0 S5; GI3 §2 fences it out; no arrest commit on `origin/main` |
| Trip band 50/60 → **70/80** shipped | **STALE — the shipped band is 60/70** | C1; `test_sled.cpp` asserts 60/70 |
| Fore-aft lean authority at the floor of its band (0.80× vs 1.14×) | **CORROBORATED** | `gi_measurements.md` OPEN FINDINGS item 1 (line ~789) |
| Three comfort dials contradict the kernel defaults (`right_assist_nm` 2400 vs 0) | **CORROBORATED, AND ALL THREE NAMED** | `right_assist_nm` 2400 vs `sim/sled.h` **0.0**; `right_charge_push_s` **0.6** vs **1.0**; `right_dir_eps` **0.04** vs **0.1736**. Every other `[sled_comfort]` key matches its `sled.h` default exactly |
| GI3 judged on a stale binary; K-WS1 K2 ships OFF; gyro G1/G2 built, OFF, never flown | **CORROBORATED** | §0 provenance; `sim/sled.h:859` `k_air_shift = 0.0`; `:755`/`:768` `k_gyro`/`k_gyro_react` = 0.0 |
| 2 of 40 air events landed upright | **CORROBORATED AS HISTORY, STALE AS FACT** | 2026-08-13 measurement on a pre-GI3 binary; D-B owns the re-measure |
| Roost is a consult packet, not built | **NOT VERIFIED BY THIS STRAND** — D-D's scope | — |
| Real rpm not fed to the engine synth | **NOT VERIFIED BY THIS STRAND** — D-D's scope | — |
| Sled cam lag taus unswept | **PARTLY — see C6** | Compiled literals, no TOML; but a felt pass 2026-09-09 |
| SK-1d input map never got a felt verdict | **CONSISTENT WITH THE RECORD, NOT PROVABLE** | The map IS his 2026-08-25 ruling executed the same day; no later word about steering or lean exists in `docs/`, `LANES.toml`, commit messages or memory. Absence of evidence — labelled **DERIVED** |
| No snowmachine sound in seads-recon is an open runtime bug | **NOT VERIFIED BY THIS STRAND** — D-D's scope. Adjacent memory: *"sled audio silent in RECON only (ask which exe path he launches)"* (`cam-smooth-lane`) | — |

**Nothing in the prior survey was found fabricated.** One claim is stale (the trip band),
one is overstated (the cam taus), three are outside this strand.

---

## §6 WHAT I COULD NOT CORROBORATE

1. **The 2026-09-17 drives have no words** (O14). Anything an audit says about "how it
   feels today" is inference from a five-week-old tape of a binary that did not contain the
   rollfix. Stated plainly so no downstream strand launders it into a felt claim.
2. **Whether the 2026-09-11 Onaping SINK rung answered his 2026-09-05 sled-sink report**
   (O12). Same symptom class, different lane, no re-report.
3. **Whether `right_assist_nm 2400` is a value he drove and liked.** He drove v3 at 2400
   (`8e8c6f67d` set it) and said *"it works now, but..."*; v4 then changed the mechanism
   under it and his v4 drive FAILED for the lean-coupling reason; `31e0dd96a` fixed that
   and **no drive of the fixed form is on record.** So 2400 carries a partial, superseded
   felt pass and no current one.
4. **Any felt verdict on GI3 itself** (C8). The silence is real; its meaning is his.
