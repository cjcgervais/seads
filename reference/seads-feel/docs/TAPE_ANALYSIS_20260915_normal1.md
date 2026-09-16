# Tape analysis — `feel_tape_normal1_budgetonly.csv` (2026-09-15)

Independent flight-tape analysis. Everything below is derived from the artefact
`build-play/feel_tape_normal1_budgetonly.csv` (122,727 rows, sim_dt 1/120 s) plus the
column contract in `app/feel_tape_columns.h` / `app/feel_tape.h`. Nothing was built or
replayed. Script: scratchpad `tape_analysis.py` (pandas 3.0.3).

Banner (dials live): `auto_level lean_lead 0.3 lateral 1`, `right_hand_rest 0.25`,
`coordination yaw_vert_budget 1.00 gap -5..10 deg`. Seed spawn alt 2500 m.

## 0. Sign / unit conventions verified against the source

| column | meaning (verified) |
|---|---|
| `alt` | `length(position) - R`, R = 15000 (`sim/world.h:19`). **Above the bare sphere, NOT AGL.** Terrain height is not in the tape. Two ground samples the tape does give: a landed episode 621.7–715.5 s (`s_on_ground` 93.8 s, speed 0) at alt **96–105 m**; kernel ground contacts at 978.2 s and 992.5 s at alt **256–290 m**. 219 s of the tape (t 44–388) are at alt < 0 (min −3974 m) = inside the bore/tunnel net (no crash predicate there). |
| `elev_gap` | `nose_world_elev - aim_world_elev` (checked: max abs err 1e-8 rad). Negative = nose BELOW aim. |
| `aim_dy` | `apply_mouse`: dy > 0 = aim moves DOWN in the carried frame (`input/aim_frame.h:57`). Mouse deltas land on the FIRST tick of each frame (`instructor_tick.h:2915`); 51% of ticks are 2-tick frames, so per-tick "nonzero mouse" is frame-quantized. |
| `bank_full` | `atan2(sin(phi), cos_pt)`; >90 deg = lift vector below the horizon. |
| `load_factor` | true n. `[g_limits] n_max = 32` (arcade table), so 20–35 G reads are the table, not an instrument fault. p50 1.17, p90 17.2, p99 27.5, max 35.4. |
| `yaw_budget_scale` | 1.0 = the dial did not act. Gate in `control/controller.cpp:944`: `cp.yaw_vert_budget > 0 && e.cos_phi_theta > 0` — **the dial is OFF by construction past 90 deg bank.** |
| `s_crashed` | kernel terrain-contact-outside-landing-limits, transient (`sim/state.h:63`). The CALLER respawns — or, under the L6 respawn lock, sets `respawn_refused` and does NOT re-place the body (`instructor_tick.h:1275–1283`). |

## 1. Flight summary

- Duration **1022.7 s** (17.0 min), 122,727 ticks.
- Altitude (above sphere): min −3974 / p5 −3602 / **median 194** / p95 1280 / max 2500 m. Excluding the bore excursion (t 44–388) and the landed 94 s, the fight is flown mostly 100–1300 m above the sphere.
- Speed: p5 0 (landed) / **median 246** / p95 313 / max 354 m/s. 71.4% of ticks > 200 m/s.
- Freelook engaged **13.5%** of ticks; override keys held **11.1%**; both 2.6%; neither **77.9%**.
- Crash/respawn: `a_grounded` rising edge ×1 at **t = 180.7** (alt −213 → 2500, `s_crashed` = 0: the bare-sphere / tunnel-exit predicate class). `s_crashed` rising edges ×2: **978.18 s (75 ticks, alt 289) and 992.47 s (49 ticks, alt 263)**, with `s_wing_strike` on 33 ticks (978.8, 992.9–993.4). **Neither produced a respawn** — position and speed continuous (206–214 m/s), the flight continued. The tape cannot say whether the respawn lock (`respawn_refused`) swallowed them; it can say the kernel declared two crashes in the last 45 s.
- Mouse: 25.6% of ticks carry a nonzero delta (= 25.6% of frames). Per-frame |dx| (nonzero frames): p50 2 / p90 13.3 / **p99 86** / max 345 counts. |dy|: p50 2 / p90 8.1 / p99 27.6 / max 130. dy sign 48.8% positive (down).
- Sustained lateral input: p90 of the per-tick lateral rate = 8.0 counts/tick (961 counts/s). Longest runs with the 0.25 s-mean rate above that: 0.97 s (t 289.5, 2819 counts/s, V 212, override held), 0.92 s (t 255.2, 1893 c/s, V 230), 0.52 s (t 922.1, 3482 c/s, V 256) — nothing above p90 is held longer than 1 s. Quarter-second windows with |Σdx| > 300 counts: **127** (93 at V > 200); > 500 counts: **59** (43 at V > 200). So he DOES flick hard and often; he does not HOLD max deflection.
- Body-frame aim azimuth |az| (from `tbx,tbz`): p50 0.6 / p90 17.5 / p95 45 / p99 166 deg. |az| > 90 on 3.3% of ticks (1.9% at V > 200). Sustained |az| > 90 for ≥ 1 s at V > 180: **9 episodes, 14.9 s total** (1.5% of the tape).

## 2. Slice signature search

Strict: `aim_world_elev > 0 AND nose_world_elev < -50 deg AND |bank_full| > 90`, grounded ticks excluded.

- **Strict ticks: 0. Events: 0.**
- Component counts: |bank| > 90 on 16,738 ticks (13.6%); nose < −50 on 2,124; |bank| > 90 AND aim above horizon on 4,397; |bank| > 90 AND gap < −20 deg on 656. The clause that kills every candidate is `aim_world_elev > 0`: in this pilot's fights the aim sits BELOW the horizon during hard manoeuvring (30 of 32 dives have aim below the horizon > 50% of the time; dive aim medians −13 to −63 deg). The strict signature excludes his style by construction.
- Gap distribution: p0.1 −63.5 / p1 −37.4 / p5 −9.9 / p50 0 / p95 +25 deg. Min gap −74.4 deg at t 187.45 (a one-frame transient inside a 1668-count flick, not a dive).

Weak: `nose < -30 AND |bank_full| > 80 AND (aim - nose) > 20 deg`. **291 ticks, 5 events:**

| t0 (s) | dur (s) | alt0 → min (m) | lost (m) | max sink (m/s) | bank med / max | aim elev med | nose min | gap min | G max | freelook/override 2 s before | budget fired in event | mean |dx| rate 2 s before (c/s) | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **197.84** | **2.12** | **806 → 33** (211 at nose-recapture) | **773** | **−243** | −118 / 180 | −25 (was +29 at onset) | −63 | −64 | 26.1 | fl 59% / ovr 29% | 4.3% (0 at onset) | 364 | **THE SLICE — see §2a** |
| 851.58 | 0.09 | 344 → 246 | 99 | −147 | 114 / 120 | −4 | −32 | −36 | 27.2 | 0 / 0 | 0 | 254 | inverted pull-through, marginal |
| 939.18 | 0.30 | 760 → 670 | 90 | −88 | −157 / 180 | +22 | −33 | −56 | 16.2 | 0 / 0 | 0 | 319 | inverted, aim above, 90 m — small |
| 1005.03 | 0.30 | 1029 → 581 | 448 | −158 | 136 / 162 | −45 | −71 | −26 | 11.9 | 0 / 0 | 0 | 191 | aim −45..−73 = deliberate inverted dive |
| 1009.13 | 0.22 | 479 → 262 | 217 | −154 | −82 / 82 | −16 | −55 | −39 | 12.9 | 0 / 0 | 100% | 219 | late pull from a deliberate dive, bank < 90 |

### 2a. The one real slice: t = 197.25–201.0 s (V 254–275 m/s)

0.25 s trace (alt / bank / nose / aim / gap / G / mouse Σdx,Σdy / fl / ovr):

```
197.00  916  bank   0  nose -29  aim -29  gap   0  G 0.4  dx -180 dy  +93  fl1 ovr0   <- freelook, aim==nose, descending at -29
197.25  885  bank -14  nose -33  aim -39  gap  +6  G 1.9  dx -251 dy +170  fl0 ovr1   <- freelook released, override pressed
197.50  852  bank -41  nose -33  aim -36  gap  +3  G 2.3  dx -754 dy +525  fl0 ovr1   <- THE FLICK (8.8x his p99 per-frame |dx|)
197.75  819  bank -70  nose -33  aim +29  gap -62  G 2.1                   fl0 ovr1   <- aim now 29 ABOVE horizon, nose 33 below
198.00  784  bank -98  nose -32  aim +31  gap -63  G 1.1                             <- rolled past 90, no pull yet (blend 1 = MANEUVER, roll first)
198.25  750  bank -127 nose -32  aim +25  gap -57  G 0.0
198.50  715  bank -160 nose -34  aim +21  gap -55  G 8.3
198.75  679  bank +165 nose -37  aim  +4  gap -41  G 9.1  dx +325 dy  -55            <- his hand moves the aim down (frame rolled: dy<0 read as DOWN in world)
199.00  640  bank -175 nose -43  aim -29  gap -13  G 14.8
199.25  597  bank -145 nose -49  aim -32  gap -16  G 23.4  AoA ceiling binding from here
199.50  548  bank -122 nose -56  aim -35  gap -21  G 25.0
199.75  493  bank -101 nose -63  aim -29  gap -33  G 25.0  budget 0.00 (fires now; gate was closed while cos_pt<0)
200.00  434  bank  -76 nose -63  aim -26  gap -37  G 25.3  budget 0.00
200.50  314  bank  -45 nose -48  aim -19  gap -29  G 26.0
201.00  211  bank  -31 nose -27  aim -21  gap  -6  G 26.1                             <- nose recaptures the aim at 211 m
201.25  174  ... he then continues down deliberately (aim -12..-37) into the bore, alt 5 at 203.25, -1730 by 216 s
```

- **Altitude: 197.75 → 201.0 actual ΔAlt −607 m; aim-commanded ΔAlt (∫V·sin(aim_elev)) −148 m; excess −459 m.** Mean nose − aim over the window −34.8 deg. Max sink −243 m/s.
- Mechanism as recorded: max-deflection lateral flick at 255 m/s out of a freelook release → bank runs −41 → −98 → −175 (past 90) with the nose parked 33 deg below an aim that is +29 above the horizon, G ≈ 1 for the first second (roll-first MANEUVER regime); then the pull arrives (G 9 → 26, AoA ceiling binding 100% of ticks 199.5–200.75) with the lift vector still past 90 and the nose is dumped from −37 to −63 while the aim is −26..−35. Textbook slice; the only difference from the ruling's description is that his hand had moved the aim below the horizon by the time the pull arrived, so the STRICT `aim > 0` clause misses it.
- Dial: `yaw_budget_scale` = 1.0 on every tick from 197.25 to 199.5 (gate `cos_phi_theta > 0` closed while inverted); fired (scale 0.0) on 11 ticks at 199.75–200.0 after the roll came back through 90. It could not act on the onset.
- Freelook/override: freelook held until 197.0; override held 197.25–198.0 and 200.25–201.5. This was his hand (mean |dx| rate 364 counts/s in the 2 s before, 31% of frames moving), not a parked aim.
- Fatal 500 m lower? The nose recaptured the aim at **211 m** above the sphere. Started at 306 m instead of 806 m, recapture lands at **−290 m** — inside the terrain anywhere outside the bore. **Yes.**

## 3. Dive events (0.25 s-smoothed sink < −80 m/s for > 0.5 s)

**32 dives.** Classes: slice (strict) 0 / weak-slice 2 / **deliberate 30** (aim below the horizon or below the nose > 50% of the event) / other 0. The 197.8 s slice is inside dive #8 (t 188–201, 2060 → −209 m, 2270 m lost, −243 m/s), whose first 9 s (188–197) are a deliberate −60..−80 aim dive into the bore after a 1668/517-count flick at 187.25 — the slice is the last 3.25 s of it.

Largest / most relevant:

| t0 | dur (s) | alt0 → min | lost | sink | bank med / |max| | aim med (min..max) | nose min | gap min | G max | fl/ovr in | class |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 17.5 | 9.4 | 1650 → 123 | 1527 | −242 | 0 / 42 | −45 (−81..+14) | −73 | −18 | 28.5 | 16% / 6% | deliberate |
| 44.2 | 23.5 | −15 → −3680 | 3665 | −304 | 0 / 17 | −24 | −63 | −8 | 23.7 | 13% / 4% | deliberate (bore descent) |
| 188 | 13.4 | 2060 → −209 | 2270 | −243 | −9 / 180 | −29 (−89..+31) | −78 | −64 | 26.1 | 23% / 16% | deliberate + **the slice** |
| 345 | 14.8 | 175 → −1780 | 1950 | −210 | 0 / 130 | −22 | −48 | −5 | 28.1 | 0 / 6% | deliberate (bore) |
| 973 | 2.65 | 762 → 269 | 492 | −203 | −64 / 176 | −36 (−82..+56) | −69 | −71 | 20.6 | 0 / 0 | deliberate, **kernel crash 978.2 within 5 s** |
| 988 | 3.79 | 812 → 256 | 556 | −170 | 10 / 38 | −58 (−61..+46) | −61 | −54 | 14.1 | 0 / 0 | deliberate, **kernel crash 992.5 within 5 s** |
| 1005 | 6.2 | 1100 → 262 | 841 | −158 | −27 / 179 | −37 (−76..+38) | −71 | −51 | 12.9 | 0 / 0 | deliberate (weak 8%) |

Near-deck (min alt < 300 m above sphere): **20 of 32**, all class "deliberate" — he fights low, aim below the horizon. Terrain height unknown; the 978/992 contacts show ground at ~260–290 m in that area.

## 4. The last 90 s (932.7–1022.7 s)

- Min alt **214 m** at t 953.5 (bank −0.6, nose −11, aim +20, G 19.6, V 230). Max G **20.6** (t 978.4). Max |bank| 180. Min nose **−70.8 deg** (t 1005.05). Max sink −203 m/s (t 974.3). Speed 152–230. Freelook 1.8%, override **0%**. Strict slice ticks **0**; weak ticks 98 (0.9%). Budget fired 12.5% of ticks (min 0.0).
- Three low points, none a slice:
  1. **953.5 s, 214 m** — from a self-commanded dive (aim −67 at 948.7, nose −54, sink −165). He moved the aim up (dy −163/−227/−231 per second = UP), aim +4 → +66; the nose followed at ~37 deg/s wings-level, G 19.4–19.6, AoA ceiling binding 37% of the window. Bottomed with the aim 31 deg above the nose. Saved by: **the aim moved up early + 214 m of margin**; budget fired 7.6% of the window ticks (wings level, nothing to trim). Not a slice.
  2. **978.2 s, 265 m — kernel `crashed` 75 ticks + wing strike.** Preceded by the 973 dive (inverted aim −81 at 972.7, nose −67..−69, bank −175 → −65 → −27, gap −71; then aim +52 at 974.7, nose −44; bank 96 at 977.7, nose −17, aim +48, G 20.6). A rolling pull-out of a deliberate inverted dive; |bank| > 90 for only ~0.5 s near the bottom with nose > −30, so no signature.
  3. **992.5 s, 256 m — kernel `crashed` 49 ticks + wing strikes.** Preceded by a wings-level (bank 5–10) push-over to a −59/−60 aim=nose dive from 832 m (G −7 → −0.3), pull begun at 990.5 (aim −53 → +50 in 1.75 s), nose lagging 40–52 deg behind the aim, **G 13.7 flat = the AoA ceiling (16–17 deg) binding 100%**, bank 28–43. Late pull from a deliberate dive, AoA-limited, not a slice.
- His "didn't crash": no respawn occurred, but the kernel recorded two terrain contacts outside landing limits in that window. Whether a death was booked (`book_player_death kCrash` + `respawn_refused`) is outside the tape.

## 5. The dial (`yaw_vert_budget` 1.0)

- Fired (`yaw_budget_scale < 1`) on **5,461 ticks = 4.45%**, in **82 episodes** (gap > 1 s). Scale when fired: p1 0.0 / p10 0.0 / **p50 0.36** / p90 0.88; min 0.0.
- **Co-occurrence: freelook 0.0%, override 0.0%** (5461/5461 with neither engaged).
- Bank when fired: p5 9 / p50 72 / **p95 88 deg**; fraction of the 16,738 |bank| > 90 ticks on which it fired: **0.0%** — the `cos_phi_theta > 0` gate (`controller.cpp:944`). Gap when fired: p5 −35 / p50 −4.7 / p95 +2.2 deg.
- Where |bank| > 90 in this tape: freelook 11.9%, override 25.3% of those ticks.

## 6. Verdict

The slice **does** appear in this normal fight — once. The strict signature returns 0 ticks only because its `aim_world_elev > 0` clause excludes this pilot's style (aim below the horizon in 30/32 dives); under the weaker geometry the event at **t = 197.25–201.0 s** is the ruling's mechanism exactly: a freelook-exit lateral flick of −754/+525 counts (8.8× his p99 per-frame |dx|) at 255–275 m/s rolled the airframe past 90 (bank −98 → −175) with the nose 33 deg below an aim +29 above the horizon and no pull for 1 s, then a 25–26 G AoA-limited pull dumped the nose to −63 while the aim was −26..−35 deg; **607 m lost in 3.25 s of which 459 m was excess over what the aim commanded**, recapture at 211 m above the sphere; the dial could not act on it (gate closed past 90 deg bank, 1.0 on every onset tick). Started 500 m lower it ends 290 m below the sphere = fatal anywhere outside the bore. The other four weak events lost 76–217 m and are inverted deliberate dives, not this class. Rate: 127 quarter-second flick windows > 300 counts (93 at V > 200) and 9 sustained |az| > 90 episodes at speed (14.9 s) produced **1 slice in 17 min (0.8% of hard flicks)**. The two kernel-declared terrain contacts at 978/992 s in his "pulled hard near the deck" ending were late pulls from deliberate aim-down dives under the AoA ceiling (bank < 45 at the bottom), not slices, and the 214 m low point at 953 s was a wings-level pull with the aim moved up early. What the tape cannot show: AGL (alt is above the bare sphere; terrain samples 96–105 m and 256–290 m), whether the two crash flags were booked as deaths, and any held max-deflection lateral input longer than 1 s (his longest sustained above-p90 lateral run is 0.97 s) — the 400–600 m slice at a HELD deflection is not exercised by this tape; the flick-triggered one is, once, at 459 m excess.
