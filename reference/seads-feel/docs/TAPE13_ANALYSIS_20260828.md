# CONSULT 1 REPORT — SEADS ENEMY AI BALANCE, TAPE 13. 2026-08-28
### (Independent context-free consult. Read-only. Every number below was measured off the tape or cited to file:line.)

Worktree `D:\seads_sandboxes\enemy-ai`, HEAD `3a34f0b8e`, read-only throughout. Tape `build-play/conquest_tape_13.jsonl` (13,085,974 B, signature **VERIFIED**, 14:04.57 of sim, ticks 24..101376, 42,240 drone rows). Screenshot `game_end_ 08_28.png` inspected and it matches the tape's final row exactly (score [240,100], rs [1.0, 0.0], PLANES 8, PUMPS 1/4, blue ellipse north only, orange cluster with one blue marker south-centre). All positions below are metres; "alt" is altitude above the 15,000 m sphere (the tape carries no DEM, so true AGL is not derivable offline — `af` air fraction and `ds` deck scope are the air-truth instruments, and I used them). One tooling caveat: my inside/outside calls use the single drawn ellipse per faction from the `bub` rows; the air field itself is what `af` reports.

Cast, verified from the recorder (`app/conquest_tape.h:265`, `fs` = `friendly_side`): **i=0..6 are the 7 SUDBURY enemies, i=7,8,9 the 3 VALLEY allies.** Match shape: enemy kills the VALLEY deep pump 05:15.74 via the bore; allies+player kill both SUDBURY pumps (deep 08:36.52, surface 10:41.07); enemy dome collapses at 10:41; VALLEY surface pump survives at 15,744/19,620 HP. **The player's side won 240–100.**

---

## A. WHAT TAPE 13 ACTUALLY SAYS

### A.1 The full death ledger — 16 deaths: 12 enemy, 4 ally; 0 AI-vs-AI kills (`da` count = 0)

| # | tag | drone | t | mav | eng/foe | net | alt | af | ds | ta | gc | reading |
|---|-----|-------|---|-----|---------|-----|-----|----|----|----|----|---------|
| 1 | dc | i=1 E | 03:40 | RUN | 1 / ally7 | 1 | −3748 | 1.00 | 1 | 0 | +3.7° | bore crash, **fighting in the bore** |
| 2 | dc | i=7 A | 04:26 | RUN | 1 / enemy2 | 1 | −3747 | 1.00 | 0 | 0 | +3.6° | bore crash, fighting |
| 3 | dk | i=5 E | 04:36 | PATROL | 1 / player | 0 | +656 | 1.00 | 0 | 1 | +26° | **player defends his pump** — killed at 40 m, at the pump |
| 4 | dc | i=2 E | 05:07 | RUN | 0 / − | 1 | −3747 | 1.00 | 1 | 0 | +3.6° | bore errand crash |
| 5 | dc | i=9 A | 05:28 | RUN | 1 / enemy4 | 1 | −120 | 1.00 | 1 | 0 | −10.6° | bore crash, fighting |
| 6 | dc | i=4 E | 06:51 | CLIMB_OUT | 0 / − | 1 | −3727 | 1.00 | 1 | 0 | +1.7° | bore errand crash |
| 7 | dc | i=0 E | 07:38 | PATROL | 1 / player | 1 | −1980 | 1.00 | 0 | 0 | +22.7° | bore crash while fighting the player (player was in the tunnel 07:03–09:33) |
| 8 | dc | i=1 E | 07:44 | RUN | 1 / player | 1 | −1590 | 1.00 | 0 | 0 | +9.9° | bore crash, fighting |
| 9 | dk | i=2 E | 09:06 | RUN | 1 / player | 1 | −2056 | 1.00 | 1 | 0 | −25° | player kill in the bore |
| 10 | dc | i=7 A | 09:53 | TRANSIT | 0 / − | 0 | +102 | 1.00 | 1 | 1 | +26° | errand deck crash |
| 11 | dc | **i=0 E** | **10:21.72** | PATROL | **1 / player** | 0 | +142→impact | 1.00* | 1 | 1 | +26° | **CHAD'S EVENT 1** — see A.3 |
| 12 | dc | i=9 A | 10:31 | TRANSIT | 1 / enemy3 | 0 | +97 | 1.00* | 1 | 1 | +26° | ally fell from a 1,941 m dome-top exit (see A.4) |
| 13 | dc | i=5 E | 10:53 | PATROL | 0 / − | 0 | +112 | 1.00 | 1 | 1 | +26° | errand deck crash at 238 m/s, ta latched, still hit |
| 14 | dk | i=0 E | 11:42 | PATROL | 1 / player | 0 | +211 | 1.00 | 1 | 0 | +0.5° | player kill |
| 15 | dc | **i=3 E** | **12:54.06** | **DIVE_IN** | **1 / player** | 0 | +122→impact | 1.00 | 0 | **0** | **−33.5°** | **CHAD'S EVENT 2** — see A.5 |
| 16 | dk | i=6 E | 13:46 | PATROL | 1 / player | 0 | +1281 | 1.00 | 0 | 0 | −10° | endgame dogfight **inside his dome**, killed at 298 m |

\* af=1.00 at the last sample because both fell back into deck air before impact; af during the falls was 0.00 (A.3, A.4).

**Counts:** 12 crashes, 4 player kills, **0 AI-vs-AI shootdowns**. Of the 12 crashes, **8 were ENGAGED at the last sample** — the engaged population is the crashing population. In the bore: **7 crashes + 1 dk = 8 of 16 deaths (50%)**, and **5 of the 7 in-net crashes were ENGAGED** — the bore grinder in a real fly is substantially a fight-in-the-bore problem, which every existing errand-graded deck arm is structurally blind to. Died engaged AND outside any dome, not in the net: **2 enemy (i=0, i=3) + 1 ally (i=9)** — and only i=0 and i=9 are thin-air deaths; i=3 died on af ≥ 0.86 (terrain, not vacuum).

### A.2 The dome-exit population — and the exit nobody named

Every own-dome inside→outside crossing (alive, non-net samples):

- **Flown exits, ENGAGED: 6** (alt at crossing: +499, +496, +544, +600, +1,673, +1,803). Two died within 60 s (i=0 from 1,673 m; i=2 by player fire).
- **Flown exits, errand: 9** (med +917, max +1,941). Two died (i=9 twice, from 856 m and 1,941 m).
- ★ **Five of those "exits" — i=1, i=2, i=3, i=5, i=6 — happen at the SAME sample, 08:36.58**, the tick after pump 3 died and the SUDBURY dome halved (rs 1.25→0.75 at pk 08:36.52). **They did not fly out; the edge moved past them**, stranding them at 455–600 m, several at af=0.00 instantly. Nothing in the machine re-plans on a dome resize; the D2 regroup stage 0 that should catch exactly this fires **zero** times at HEAD (shadowed by ENGAGED). Same thing again, fleet-wide, at the 10:41 collapse.

### A.3 Chad's event 1, found: drone i=0, 09:37–10:21.72

Player context: he flew a 90 s approach to the SUDBURY surface pump, range 16 km → 733 m at 10:39, at alt ≈ +280.

1. 09:37.98 — i=0 is on a raid TRANSIT **inside its own dome at alt +1,762**, and becomes ENGAGED on the player at range **5,996 m**. Engagement is pure player-range: nearest `max_engaged` within `engage_range = 6000` (`drone/drone.h:1965-2002`, `config/scenario.toml:327`). No altitude, air, or geometry term.
2. 09:55.98 — player range 2,416 m: TRANSIT yields to the fight (`transit_fight_yield_m = 2500`, `config/scenario.toml:1182`), mav flips to PATROL, BFM takes the nose, it banks −64.7° and descends −7° after him.
3. 10:02.38 — it **crosses its own dome edge at alt +1,673, engaged** (af already 0.84 and falling).
4. 10:06.98 — `ds` latches (deck scope), af 0.25. **The deck track law is withheld because it is engaged** — `deck_errand = (deck_errand_order || maverick_committed || !d.engaged)` at `drone/drone.h:3335-3341`; with mav=PATROL and eng=1 every term is false. It holds gc = −12° while af hits 0.00 by 10:10.
5. 10:10–10:18 — ballistic mush: speed 157→209 m/s, alt 1,357→538, **while the range to the player it is "chasing" grows 3,434→6,005 m** — he is receding faster than it can fall after him.
6. 10:18.98 — `ta` latches, gc snaps to +26° (the crash net below the deck block is NOT gated on `deck_errand` — verified at `drone/drone.h:3343-3369`), and it impacts at 10:21.72 at 211 m/s, **2,450 m outside its own dome**.

That is exactly "flew out of the bubble from up high then fell and crashed", 20 seconds before he killed that pump.

### A.4 ★ The amendment: the deck track law as shipped CANNOT save a high exit anyway

Ally i=9 died the same way at 10:31 **with the track law available** (mav=TRANSIT → `maverick_committed` → deck_errand true; ds 0.84 through the fall): it exited its dome at +1,941 on an errand, and still hit at 225 m/s, ta latched. The reason is arithmetic: the track law's descent is capped at `deck_track_dive_cap_deg = 12.0` (`config/scenario.toml:609`). From a ~1,900 m dome-top exit, 12° at ~170 m/s sheds ~35 m/s of altitude — ~850 m in the 24 s the airframe stays alive in vacuum. It needed ~1,800. Meanwhile enemy i=4, engaged, exited at +1,803 in TRANSIT and survived — it entered the vacuum already descending on a long shallow profile. **Conclusion: fixing the engaged predicate alone is insufficient; the true invariant is "never arrive at a dome edge high" — the descent must begin BEFORE the boundary.**

### A.5 Chad's event 2, found — and the tape reverses its meaning: drone i=3, 12:41–12:54.06

i=3 held a live raid order on **pump 0 — the very "undefended pump to the north"** (rpi=0 window 11:42.17–14:04.77). It was not refusing to attack it; it was **executing the attack**, routed via the tunnel (`raid_route_via_tunnel = true`): TRANSIT inside the player's dome at ~860 m toward the **Errington portal**. At 12:41.97 it became engaged (player at 5,866 m). At 12:43.17 it armed **DIVE_IN** — gc pinned at −55°, the maverick's committed portal dive — crossed out of the player's dome at 12:48.37 (+22 m past the edge), and flew into terrain at 12:54.06 at 160 m/s, **1,152 m short of the Errington mouth** (crash pos [-8808.5, -1774.6, 12150.0] vs mouth [-8144.3, -997.3, 12680.9]), af never below 0.86. **A terrain death on breathable air, not a vacuum death**, and `ta` never latched because DIVE_IN is `is_underground_mode` (`drone/maverick.h:637-640`) and the entire terrain-avoid block is skipped for it (`drone/drone.h:3294`, exemption by design — a portal dive must be allowed to fly at the ground). Why the committed dive missed the mouth could not be settled read-only — the bank slewing −65°/+65°/−9°/−65° during a dive that is supposed to start "aligned by construction" (`drone/maverick.h:1095`) is suspicious. **HYPOTHESIS: the engagement perturbed the approach line before/at arming, or the arming window tolerance admits a fatal lateral error.** Settled by: a fixture replaying i=3's 12:42 state into `maverick` and grading portal-approach clearance, engaged vs not.

### A.6 Defence and attack awareness, measured

- **Pump 3 (SUDBURY deep) died 08:36.52** with the nearest enemies at 5,102 m and 5,637 m — both engaged **on the player**. Structural finding: `assign_defense` only ever examines `pumps[f]`, f∈{0,1} — **the surface pumps. Deep pumps have no defence trigger at all** (`combat/raid.h:245-249`).
- **Pump 1 (SUDBURY surface) died 10:41.07** with the nearest enemy at **9,070 m**. Its HP was taken 15,184→dead over 08:12–10:41 mostly by **ally i=8's raid** (on-station at 352 m closest), with the player arriving inside 733 m only at 10:39. The 2,500 m threat radius (`app/instructor_tick.h:60`, `kDefendThreatRadiusM`) did arm defend orders (def duty 4–7% across the fleet), but no defender ever arrived: candidates are filtered to non-tunnel-committed while the fleet spends 42–64% of its life in tunnel modes, and those who did engage the attacker could not close.
- ★ **The AI-vs-AI air war is completely non-lethal.** All three allies ended 14 minutes at **min hp = 100.0, zero damage ticks**, `da` = 0 — despite enemies spending 4,402 engaged-on-a-drone-foe samples. The damage seam exists and is armed (`app/instructor_tick.h:2250-2281`, `kAiVsAiDpsFrac = 0.8` at `:73`); it is gated on `ai_guns_on`'s 60–900 m band (`config/scenario.toml:444,452`). Measured: **enemy-on-drone-foe range was p10/med/p90 = 2,183 / 4,137 / 6,111 m; in-band ticks 13 of 4,402 = 0.3%** (all 13 from one pair). The post-collapse cluster Chad watched "fighting one of my allies" is an **unresolvable furball**: i=1/i=2/i=4 engaged on ally i=8 for 83/67/57% of 3.4 minutes at ~4 km and could never hurt it. Foes ARE assigned (nearest-opposing, sticky, `app/instructor_tick.h:1414-1423`) — what is missing is **closure**.
- Against the player: **8 real rounds all match** (i=4×1, i=6×7), wf=1 on 2 samples fleet-wide — but the one fight that converged nearly won: i=6 chased him inside his own dome for 6+ minutes (leash=1), closed to **19 m**, and two hits at 13:34 took him **100→53.4→7.3 hp**. The machinery can fight when geometry lets it converge; it converged once in 14 minutes.
- **The entire enemy surface-pump offense of the match was one 4.6-second strafe**: pump 0 lost 3,875 HP in 02:32.59–02:37.19 with i=5 (the single `raid_deck_run_slots=1` deck runner) at 998→545 m; i=5 was then killed at the pump by the player, respawned at 05:51, and **never again came closer than 12,662 m to any pump; its ballistic run is one-shot and never re-armed** (0 vt ticks after respawn). Everyone else's raid path was the bore, where they died.

### A.7 Post-collapse — grading the S4 probe against the real case

From 10:41 to end, the six surviving enemies: weighted **engaged fraction 0.61**, weighted **deck-scope duty 0.87** (five pilots at 1.00; i=3 0.43, i=6 0.74 because both flew high inside the player's dome), alt p10/med ≈ +190/+210–230, **af median 1.00**. Two crashed in those 3.4 min, two were killed by the player. **Verdict on the prior S4 probe: NOT structurally blind on its own statistics** — "collapsed faction flies the deck, ~60% engaged" reproduces almost exactly (61% here). What the probe could not see, because both its domes were dead: the ally inside the dead zone makes the furball permanent and unresolvable, and the deck-crash and portal-dive killers keep running. Chad's "respawns staying in a zone that has no air" is visually right and functionally wrong: **they are breathing** (af med 1.00 on the deck) — the S4 respawn-on-own-deck law is working; it looks like a death circle because the fight there can never end.

---

## B. THE DIAGNOSIS — four defects, ranked by how much of Chad's report each explains

1. **CLOSURE — the fleet cannot convert engagement into effect** (measured). Explains "attack enemyplane / attack awareness" (median 4.1 km to drone foes, 0.3% in the gun band, 0 AI-vs-AI damage in 14 min), the ineffective defence even when orders armed, the endless dead-zone furball, and most of the quiet air war. The prior "last metre" finding (intercept ceiling faded to ~144 m/s, `drone/bfm.h:1041`; P-A: naive un-fade made player-facing results worse) covers the player-facing regime — but the AI-vs-AI regime never even reaches the merge where P-A's overshoot lived. **Biggest single lever.**
2. **THE ENGAGED DECK HOLE + NO PRE-EDGE DESCENT** (confirmed: code `drone/drone.h:3335-3341`, tape i=0; amendment A.4 measured). Explains claim 1 in full — but only 2 of 12 crashes. Fixing the predicate alone is insufficient for exits above roughly 1 km.
3. **OBJECTIVE-APPROACH GEOMETRY: the bore grinder and the portal dive** (8 of 16 deaths in-net, 5 of 7 in-net crashes engaged; i=3 1,152 m short of the mouth with the net exempt). Explains claim 2 and half the death ledger.
4. **DOME-RESIZE STRANDING** (5 simultaneous "exits" at 08:36.58). Explains part of "they leave the bubble and crash" that no flown-exit story covers.

Not a defect: the tunnel offense works (deep pump died at 05:15 to a bore run), and the one converged fight nearly killed him. Both must survive whatever ships.

## C. ARCHITECTURE RECOMMENDATION (consult 1's view)

The current structure has four laws negotiating one gamma channel by predicate exclusion (`deck_errand`, `terrain_avoid`, arena clamp, ballistic phases), and engagement is a range-only fact that silently re-scopes all of them. Principle: **"where flight exists" must be a constraint stage, not a competing behaviour** — as the arena clamp already does at the final seam (`drone/drone.h:3379-3383`).

- **Air-envelope governor at the final commanded-gamma seam**, every non-underground tick, engaged or not: (i) outside a dome, clamp commanded gamma so the flight path cannot leave the breathable band; (ii) **inside** a dome, when the forward look (the existing 12 s `deck_look` shape, along track) predicts a dome-edge crossing, blend in a descent target so the drone arrives at the edge already low. Direction, bank, target and pursuit untouched.
- Against the three options previously tabled: **(a) engaged-gets-track-law alone is insufficient** — i=9 died with the law active; **(b) clamp-only alone is insufficient** — a clamp after the edge cannot un-climb 1,700 m; **(c) leave it** keeps both watched deaths.
- **Attack awareness:** target selection is already deliberate; the defect is downstream closure. Instrument closure for the **AI-vs-AI regime first** (P-A's refutation does not apply there); success = in-band tick fraction 0.3% → double digits, first `da` event ever, ally min hp < 100. Do not re-attempt the player-facing speed fix.
- **Defence awareness:** (1) trigger on **pump-HP-delta, not radius**; (2) **extend to deep pumps** (today surface-only; pump 3 is structurally undefendable); (3) defenders fly the errand shape to a station, graded on time-to-first-defender-within-900-m (today ∞). `kDefendersPerFaction = 2` is a reasonable start; keep raiders exempt in pass 0 (the E14 cannibalization lesson).
- **C2 standoff** remains the largest queued debt; its probe must carry a **foes-present arm** (5 of 7 in-net crashes were mid-fight) plus a new graded case: portal-approach terrain clearance under engagement.

## D. ORDER OF WORK
1. **R-CLOSE** (AI-vs-AI closure; instrument then fix). Arm: foes-present two-faction fixture; grade = in-band tick fraction (baseline 0.3%), first `da`, ally min-hp.
2. **R-AIR** (envelope governor / engaged deck hole). Arm: replay i=0's 09:55 state; also re-grade i=9's 1,900 m errand exit — that one fails at HEAD *with* the law, so a bit-identical result there means the fix is fake.
3. **R-DEF** (HP-delta defence + deep pumps + defender errand shape). Regression clause: raid pressure must not fall.
4. **R-C2** (standoff + portal approach), with the foes-present arm and the i=3 replay.
5. **R-PERSIST** (surface-pump pressure): total enemy surface-pump offense was 4.6 s / 3,875 HP, one run, never repeated.
6. **R-RESIZE** (dome-shrink re-plan): the 08:36.58 five-drone stranding is the concrete case.

## E. WHAT CONSULT 1 REFUSED TO DO
- Any pursuit leash, break-off range, or "return to bubble when losing" law — the fix is where-flight-exists, not whether-to-chase; a break-off also deletes i=6's 19 m gun pass, the best content in the tape.
- Making DIVE_IN/RUN abortable on engagement, or re-arming the terrain net during them — that deletes the tunnel offense Chad just praised. Protect the approach line instead.
- Prioritizing objectives over fights (or the reverse) globally — the loop is content-rich because they interleave.
- Raising `raid_dps_frac`, `kAiVsAiDpsFrac`, or difficulty dials to fake effectiveness — the failing gate is geometric (0.3% in-band); dial-turning is a secret buff, the mirror of the banned secret nerf.
- Re-attempting the player-facing intercept speed fix (`bfm_intercept_chase_unfaded` ON) — P-A measured rounds at player 54→23, hits 16→2.
- Teleporting the dead-dome fleet or adding vacuum/hypoxia damage — they measurably breathe on their own deck (af median 1.00); the spectacle resolves when the furball can end.
- Grading any of this with errand-only arms — a bit-identical foes-present arm means blind fixture or dead branch, not success.
