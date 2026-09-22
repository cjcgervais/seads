# R3 — PLAYER EXPERIENCE: ADVERSARIAL REFUTATION PASS

Target: `docs/sled_audit/R3-player-experience.md` (728 lines, written 2026-09-18T00:53).
Method: independent re-derivation of every numeric claim from the primary artefacts —
`docs/sled_audit/tape_summary.json` (strand D-B, 2026-09-18T00:38), strand R3's own saved
probe outputs (`marks.json`, `marks_all.json`, `spd.json`, `rec_A..D.json`, `r3_probe.py`
in the session scratchpad), and read-only source reads of `app/main.cpp` and
`app/player_mode.h` in this worktree. Two literature sources fetched and checked verbatim.
READ-ONLY: nothing built, no ctest, no exe, no dial, no config, no golden, nothing pushed.

**VERDICT: SOLID_WITH_FIXES.** The measurement half of R3 is unusually good — 7 of the 9
MEASURED findings reproduce to the digit from artefacts I re-derived myself, and every one
of Chad's quotes is verbatim-exact with the right document and section. Two findings are
refuted: **R3-M8** is flatly contradicted by the shipped source (and R3 names it "the single
most important framing in R3"), and **R3-M5** carries a decorative killing mutation plus an
interpretation the data in its own source file refutes.

---

## 1. WHAT SURVIVED — independently re-derived, exact

| claim | R3 says | I re-derived | source |
|---|---|---|---|
| R3-M1 v17 | 52 events, 16 episodes, 1.33/min, 3.25/crash, 5.7 % past-90 | 52 / 16 / 1.331 / 3.25 / 5.74 % | `marks.json` roll_marks, merged at 2.0 s |
| R3-M1 merge sweep | 1.0 s → 18; 3.0 s and 5.0 s → 16 | 18 / 16 / 16 | same |
| R3-M1 corpus table | 5 buckets, 355 episodes, 1032 events, 146 R, 206.1 min | every cell exact | `marks_all.json` + `tape_summary.json` |
| R3-M2 headline | p50 7.02 / p90 18.13 / max 38.41 at 80 % | 7.02 / 18.13 / 38.41 | `spd.json` 10 Hz trace, re-implemented from scratch |
| R3-M2 sweep | 8 variants | p50 and p90 exact in 6 of 8 rows; max exact in all | same |
| R3-M3 | 15 of 52; count 15 under all four predicates; p50 3.81→2.19, p90 6.25→3.53 | exact, all four | `rec_A_baseline/B_spd2/C_loose/D_nospd.json` |
| R3-M4 | 5 / 0.42 / 0.31; corpus 146 / 0.71 / 0.41; gaps n=113 p50 25.4 p10 7.6, 16 % <10 s, 54 % <30 s; tapes 8/14/16 = 23/21/18 | 25.35 / 7.58 / 15.9 % / 54.0 %, all counts exact | `override_detail` |
| R3-M5 numbers | 17/58, 306/984, 4.82/min | exact | `air` block |
| R3-M6 | six occupancy figures; steer p50 0.005 p90 0.995 | all six exact; 0.005 / 0.995 exact | pooled 0.01-bin histograms |
| R3-M7 source | 300 px, 2.5/6.0, 2.0/3.0, Q/E 2.0, no lean self-centre, `else if (!live.freelook_held)`, `lean_return_tau_s` 0.0 | all confirmed; tau 0.0 in **all 84** headers | `app/main.cpp`, `tape_summary.json` |
| R3-M9 | median 1.31 min, 27 % <30 s, 10 tapes / 56 %, 29 sittings | 1.307 / 27.4 % / 10 / 56.3 % / 29 | `duration_min`, `mtime` |
| corpus caveat | 84 of 91 parse; 41–47 zero-byte; build-play tape 4 = 1.73 min | exact | `parse` field |
| Chad's words | §0, §0b, gi4 §1, SK-1d, KEY_R | **verbatim-exact, correct sections** | primary docs + source |
| R3-L1 | Juul "the amount of *time* the player loses…"; permanent vs transient; no threshold in seconds | fetched; quote exact; self-limitation accurate | jesperjuul.net |
| R3-L3 | GRIP: "Small bits of scenery…"; angular momentum scaled against speed; assisted landing; map/arrows/light | fetched; all four confirmed | Game Developer |

**One thing R3 measured and under-claimed.** All five v17 R presses land on the *last past-90
exit tick* of a crash episode (tape 86 @5804, tape 88 @46589/48179, tape 91 @6372/15130). The
press is what ended those episodes. That is stronger support for R3-M4's "5 presses = 5
distinct crashes of 16" than R3 offered, and it means R3-M2's failure cost is *truncated* —
not inflated — on those five.

---

## 2. REFUTED — R3-M8, "Combat and riding are serialized, not concurrent"

**The source says the opposite.** `app/main.cpp:7907`:

    const auto sting_stance_ok = [&]() {
        return (player.mode == app::PlayerMode::Afoot && man_upright) ||
               (player.mode == app::PlayerMode::Sled && !sled.rolled);
    };

and `app/player_mode.h:283`, carrying **Chad's own word in the comment**:

> "★ … OR ON THE SEAT (Chad 2026-09-04: "press P on the snowmachine … deployment from the
> seat to the Sudburian's hands"). No J first: the machine is the stance, and the end of the
> flight hands him back the bars."

    if (ctx.drone_ready &&
        ((st.mode == PlayerMode::Afoot && ctx.man_upright) ||
         (st.mode == PlayerMode::Sled && ctx.sled_upright))) { … DeployDrone; }

Four independent confirmations:

1. **P shoulders the launcher from the saddle** (`main.cpp:7983-7995`), gated on
   `sting_stance_ok()`, which is true for `Sled && !sled.rolled`.
2. **`render::sting::clamp_seated_az`** exists for exactly one purpose — bounding the aim
   *while seated* (the 180° seat law). A serialized design would not need it.
3. **`sting_seated_now = player.mode == Sled || (Drone && drone_home == Sled)`**
   (`main.cpp:8063`). The launch is designed to originate on the seat and hand the bars back.
4. **`bars` is unaffected**: `const bool bars = player.driving() && walker.mode == Riding`
   (`main.cpp:8209`). Shouldering does not clear it, so W / A / D / S stay live while aiming.

R3 read `"RIGHT THE MACHINE FIRST"` as "dismount first". It means *un-roll the sled* — it is
the `else` branch reached only when `sled.rolled` is true. R3 inverted the law.

**Worse, it hides a third claimant on the mouse.** The branch chain is
`if (Drone) … else if (sting_shouldered) … else if (!live.freelook_held) { lean }`
(`main.cpp:6336 / 6363 / 6414`). While shouldered **on the seat**, the mouse drives
`sting_aim_az/el` and lean freezes — a mouse contest R3-M7's table does not list at all.

**Blast radius, by R3's own stated killing mutation** ("A build where the player can fire
from the bars inverts this finding and promotes the control-budget limit from background to
blocker"):

- R3-M8 is void as written.
- R3-M7's control-budget table is incomplete: throttle + steer + (lean **or** freelook **or**
  sting aim, three-way exclusive on one mouse) + stand + C/R/P, all live while mounted.
- R3-L9's "saving grace is R3-M8: you cannot shoot from the bars" is void. The control-budget
  heuristic is live, not background.
- §5 and R3-P3's lead framing — "the battle does not load the player's hands while riding, it
  loads the *clock*" — is half wrong. It loads both. R3-P3's conclusion still stands on
  R3-M1/M2 and Chad's §0 ruling, but its headline justification must be rewritten.

---

## 3. REFUTED — R3-M5, decorative killing mutation and a refuted reading

**The mutation names a behaviour the scorer does not have.** R3 writes: *"if 'landing' is
scored at first ground contact rather than after the bounce settles, a sled that touches
upright then tumbles counts as upright."* `tools/sled_tape_audit.py:620-631` already waits:

    pending_land.append(tick + int(0.5 / dt))
    …
    if pending_land and pending_land[0] <= tick:
        pending_land.pop(0)
        if tilt_deg < 45.0:
            land_upright += 1

The scorer delays 0.5 s and tests 45°. The stated mutation cannot fire. It is decoration.

**The live mutations R3 missed — and one of them kills the reading.** The real knobs are the
45° threshold, the 0.5 s delay, and above all `if air_run_max >= 0.15` (line 620): the
minimum air time that admits an "air event". Re-derived from the *same* `tape_summary.json`
R3 used (`air.hang_list_s`, v17, n=58):

- hang p50 **0.39 s**, p90 0.94 s, max 2.43 s
- only **4 of 58 (7 %)** exceed 1.0 s
- 14 of 58 are under 0.2 s; the shortest admitted is 0.15 s

So "4.82 air events/min" counts **bumps**, and "29 % land upright" is a bump-landing
statistic. R3's reading — *"an air event is the one failure the player chose"*, *"a 29 %
success rate on the thing he asked for by name makes the jump a gamble rather than a skill"* —
does not survive: 93 % of the counted events are sub-second hops the player did not choose as
jumps. `docs/gi4_ride_handoff.md` §1 item 2 says the same thing in the other direction ("the
long ones are **crashes, not jumps**").

The superlative "**the least forgiving event in the game**" is also unsupported: R3 measures
no other event's success rate to compare against.

The three numbers (17/58, 306/984, 4.82/min) are correct and reproduce exactly. It is the
mutation and the interpretation that fail.

---

## 4. FIXES REQUIRED (findings that stand, but as written they mislead)

**F1 — Publish the probe.** R3's three headline numbers (16 episodes, 7.02 s, the 15/52
invariance) exist only in an ephemeral session scratchpad
(`…/Temp/claude/D--flight-sim2/4bc841c5-…/scratchpad/r3_probe.py`, `marks*.json`, `spd.json`,
`rec_A..D.json`). I could verify them only because that directory happened to survive. The
hard rules permit adding files under `docs/` and `tools/`. Copy `r3_probe.py` and the small
JSONs to `docs/sled_audit/`, or the strand is unverifiable in a week.

**F2 — "median pre-crash speed 22.2 m/s (80 km/h)" is the upper middle value, not the
median.** The 16 episode pre-speeds are
[2.83, 3.26, 10.02, 10.84, 11.49, 11.53, 14.46, 19.40, 22.25, 29.31, 29.50, 34.71, 35.63,
37.33, 40.87, 42.64]. True median **20.82 m/s = 75 km/h**; 22.25 is index 8 under R3's
`int(q*n)` convention. Two crashes occur under 3.3 m/s — near-stationary tip-overs. §5 C1 and
R3-P3 then reuse "entered at 22 m/s" as though it characterised the typical crash. Restate as
"p50 20.8 m/s, range 2.8–42.6".

**F3 — The censored count is one low in every variant.** A straight re-derivation from R3's
own `spd.json` gives 16 scorable episodes at gap 2.0 s (5 censored), not 15 (4 censored) — and
the offset is exactly +1 in all eight sweep rows. R3 silently excludes one episode from the
scorable set. Name it, or the "4 of 15" in the findings JSON is unreconcilable with the
published trace.

**F4 — R3-P1 cites a precedent that contradicts its own shape and brushes the signed law.**
P1's shape is "act **after** the first past-90 crossing … nothing that makes the first roll
less likely", and it names GRIP's speed-scaled angular momentum as "the shipped precedent".
The GRIP text (fetched and confirmed) is: *"we have scaled the angular momentum in relation to
speed so the faster you go the less likely you are to be violently thrown off course by
hitting something"* — that acts at the **first** impact, before the first roll, and it is a
speed-dependent authority limiter, i.e. the thing Chad rejected by name: *"It will ruin the
feel to have a governor"* (§0b). R3 asserts it is "arcade-lawful under Chad's §0b licence";
that is R3's interpretation, not Chad's word, and the audit's rule forbids paraphrasing into a
stronger claim. **P1's core survives** (attack the chain; acceptance = tumbles-per-crash with
the episode rate held at ≥ ~1/min; grounded in M1/M3 + "self righting by chance more"). Strike
the GRIP precedent sentence, or P1 should be treated as refuted too.

**F5 — R3-M3 is a corroboration of R3-M1, not an independent finding.** "71 % of past-90
events are re-rolls" is arithmetically the complement of "3.25 crossings per episode"
(1 − 15/52 = 71 %). The genuinely new content is the four-predicate invariance, which I
verified and which does have real headroom (the analyzer's `pending_recovery` allows more than
one recovery per chain). Label it as corroboration.

**F6 — "resume-riding time … per past-90 event" is a mislabel.** `sled_tape_audit.py:586-604`
keeps a single `pending_recovery` slot and overwrites `roll_enter_tick` on a re-roll, so the
15 values are **per chain**, measured from the chain's *first* entry — not 15 of 52 events.

**F7 — Stale line citations.** In the 00:37 analyzer the past-90 filter is at **lines
581-590** (R3 says "near line 529") and the `autoright_R` classifier at **line 769** (R3 says
"near line 711"). The described predicates are otherwise exact: `dot_up < 0` for ≥ 0.10 s;
`prev_tilt_deg > 60.0 && after < 30.0`; recovery `tilt_deg < 20.0 && gspeed > 5.0` held 0.50 s.

**F8 — Percentile convention drift.** R3 mixes `int(q*n)` and `int(q*(n-1))` across the file:
"time upside-down" v17 p50 2.49 / p90 6.24 re-derives as 2.58 / 6.98 (max 11.78 exact); corpus
p50 2.52 / p90 7.18 re-derives as 2.57 / 7.33 (max 57.05 exact); two sweep rows' p50 move on
n=8 and n=14. Immaterial to every conclusion, but state the convention once.

---

## 5. SIGNED-LAW CHECK — clean

Checked R3 against the frozen law (depth 0.77 m fixed / no governor / one surface / wheelie
kept / ragdoll banned). R3 makes **no** claim about snow depth, surface count, the wheelie or
ragdoll, and proposes **no dial value** — it says so itself and repeats it in UNVERIFIED-1
("no number in this file licenses a dial move"). The only law-adjacent problem is F4's GRIP
precedent. R3's advisories are correctly hedged, correctly sourced to a tape event **and** one
of Chad's words, and correctly refuse to propose a number. UNVERIFIED-1 through -10 are
honest and, where I could check them (-3, -6, -9), accurate.

R3-P3's declared absence of a killing mutation ("the acceptance test for legibility is Chad's
word after a drive, not a harness number") is honesty, not decoration, and is correct.

---

## 6. ARTEFACTS I RE-DERIVED FROM (all read-only)

- `D:/seads_sandboxes/sled-audit/docs/sled_audit/tape_summary.json`
- `C:/Users/Chad/AppData/Local/Temp/claude/D--flight-sim2/4bc841c5-50c2-4751-b0b0-f257568b4f37/scratchpad/` — `marks.json`, `marks_all.json`, `spd.json`, `rec_A_baseline.json`, `rec_B_spd2.json`, `rec_C_loose.json`, `rec_D_nospd.json`, `r3_probe.py`, `r3_agg2.py`
- `D:/seads_sandboxes/sled-audit/tools/sled_tape_audit.py` (lines 145, 581-604, 608-631, 755-780)
- `D:/seads_sandboxes/sled-audit/app/main.cpp` (6336-6432, 7907-7910, 7975-8070, 8209-8270, 8404-8432)
- `D:/seads_sandboxes/sled-audit/app/player_mode.h` (283-302)
- `D:/flight_sim2/Game_loop_idea/vehicle_program/ROLL_COMFORT_HANDOFF.md` §0, §0b
- `D:/seads_sandboxes/sled-audit/docs/gi4_ride_handoff.md` §1
- https://jesperjuul.net/text/losttime/ (fetched this session)
- https://www.gamedeveloper.com/business/game-design-deep-dive-situational-awareness-and-player-frustration-in-i-grip-i- (fetched this session)
