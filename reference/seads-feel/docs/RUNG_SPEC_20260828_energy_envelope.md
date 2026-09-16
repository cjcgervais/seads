# RUNG SPEC — THE ENERGY ENVELOPE. **FINAL, v2, revised under fire.** 2026-08-28

Worktree `D:\seads_sandboxes\enemy-ai`, branch `sandbox/enemy-ai`, HEAD `3a34f0b8e`. Supersedes SPEC v1. Chad's 2026-08-28 ruling (§1 below) is the binding text; "chase you anywhere" (2026-08-26) stands untouched. Builder: read §1–2 of `docs/CONSULT_PACKET_20260825_ai_audit.md` in full before the first edit, then `docs/SESSION_HANDOFF_20260829_enemy_ai_BALLISTIC.md` §2 (quote that headline table, no earlier one), then this document end to end.

---

## §0. ADJUDICATION OF THE RED-TEAM FINDINGS

Every finding below was re-verified at the artefact by this pass, not inherited. RT1 = the loop red team (findings L1–L10 + parts C/D), RT2 = the buildability red team (F1–F13). Where the two teams found the same defect, one row carries both.

| # | finding | verdict | disposition, with the verifying number |
|---|---|---|---|
| L1 / F1 | A(d) has no outside floor → governor porpoises every deep-outside transit, fights the terrain latch, mutes guns during crossings, breaks the signed D3 RUN crossing and the S1-DECK lane (`deck_track_agl_m = 100.0`, scenario.toml:607 — RT1's ":606" was off by one, value right) | **ACCEPTED** | E2 formula amended: `A_eff = max(A(d), deck_track_agl_m)` **plus a structural skip** — when `max_i A(d_i) ≤ deck_track_agl_m` over all six samples the governor writes nothing (bit-identical deep outside). New deep-outside transit arm E2-A7 with a remove-the-skip required-red mutation. `deck survival ≥ 0.700` and `crashes ≤ 0.36/min` added to E2's regression clause (the two reproduced BALLISTIC §2 numbers). |
| L2 | The knee is **constant**: `d_knee = 0.5·r_eff_base(θ)` (verified: shrink is subtractive, conquest.h:646–649; `s` baked into a/b, faction_bubbles.h:249–252; SUDBURY major 17,400 → knee 8,700 m, minor 13,050 → 6,525 m). Free core = 36% of disc area at s=1.25, **11% at s=0.75, 0 at s=0.5**; enemy median depth 6,852 m is *inside* the ring; 5,114 samples above 1,800 m today would cap at 1,650 | **ACCEPTED — ESCALATED** | One loud headline table in §6 (DECISIONS) + **new DECISION #4**: constant knee vs live-fraction knee. Nothing pre-picked. E2 arm 5 also runs at s=0.75. |
| L3 / F3 | "The tunnel offense cannot regress: the governor never executes on those ticks" is **FALSE** — TRANSIT is a tunnel mode (`is_tunnel_mode`, maverick.h:623–627) the governor deliberately DOES govern (it is the i=9 fix), and TRANSIT flies the portal approach (`approach_alt_m` 500 / `approach_back_m` 850, maverick.h:269–271; `approach_reach_m` 450 :1143–1145; `transit_line_tol_m` **300**, maverick.h:298, arm gate :1237). RT2's arithmetic verified: at the i=3 post-shrink geometry, pair (a) leaves 0–100 m cross error; pair (b) leaves **253–317 m against the 300 m tolerance** | **ACCEPTED** | The false sentence is deleted. New portal-approach arm E2-A8 (governor ON vs OFF, shrunk dome, mouth outside; grades DIVE_IN armings + go-arounds + mouth clearance). DECISION #1 now carries the tunnel-life numbers, not just fringe feel. FIX-point elevation (~1,200 m by the chord construction) stays HYPOTHESIS — settled by the arm. |
| L4 / F4 | E4's deep-pump leg is hollow: `assign_defense` stamps `defend.target_pos = pump.pos` verbatim (raid.h:318–320), the defend branch flies `aim_at` at it as surface flight (drone.h:2907–2909) gated `if (!tunnel_mode)` (:2896–2897), tunnel-committed pilots are excluded from candidacy (raid.h:291–307, E14) — so the only eligible pilots are structurally incapable of flying the order; the arm grades a flag. raid.h:281–286's own comment names the disease | **ACCEPTED — ESCALATED** | Deep-pump defence is **cut from E4's build scope** and goes to Chad as **DECISION #5** with three options and numbers. E4 ships surface pumps + signed HP-delta only, fully gradeable. HP-delta signed on **losses only** (millwright repair ticks raise HP — L9 carried). |
| L5 | Both fringe dial pairs damage the tape's best fight (i=6 endgame, depth med 741 m / alt med 1,110; A(741) = 458 conservative, 870 loose); arm 1 grades "still shoots" but not "still converges" | **ACCEPTED** | i=6 window replay under BOTH pairs added to the DECISION #1 evidence set; min-range-achieved convergence stat added to E2 arm 1. |
| L6 | "AND attack enemyplane and attack awareness too" deferred to R-CLOSE without telling Chad | **ACCEPTED** | One line in §6 preamble + the fly note: R-CLOSE is the next rung, baseline 0.3% in-band carried by the T-4 instrument. |
| L7 | "in a fight they might climb" ships as permission, not a tactic (BFM is do-not-touch) | **ACCEPTED** | Stated in the fly note verbatim so the fly verdict grades what was built. |
| L8 | An E2-only fly reads as a pure nerf | **ACCEPTED** | Fly checklist is held until E3 relinks; if Chad flies early, the pre-brief in §8 goes in the reply. |
| L9 | Millwright canon carry-items: signed HP-delta; R6 archive correct | **ACCEPTED** | Both in. |
| L10 | (a) −30° close-in dive is a magic number; (b) `defend_sprint_speed` 245 = `v_redline` (aircraft.toml:114) needs the authority-recovery check; (c) pre-gate disciplines kept | **ACCEPTED** | (a) two new keys `defend_dive_close_gamma_deg` = 30 / `defend_dive_close_range_m` = 4,000; flyable because the phase mirror writes `target_gamma` directly (D3 pattern) — see F11a. (b) T-1 fixture asserts full deflection authority recovered before the 2,500 m release merge. (c) kept. |
| RT1-C | Pursuit-leash check: clean once L1 is fixed; leash-mute (drone.h:3178) × governor interaction unchecked | **ACCEPTED** | One cheap leg added to E2 arm 6's fixture: leashed fringe tick, both writers active, assert no oscillation between `wants_fire` mutes. |
| RT1-D | `deck_look` fleet-wide frame cost unmeasured | **ACCEPTED** | Measure, don't assume: E2 lands with a frame-time delta printed by the sweep; the `d − v·lookahead > d_knee` skip is the mitigation. |
| F2 | E2's bit-identity table is unsatisfiable: `[.bdeck]` arm C (`kHeadEnemyHash = 2ec5baddc8346cfe`, test_conquest_match.cpp:2969) is the shipped-table full match containing the errand deck population that MUST move; keys are `require()`-loaded (verified load_scenario.cpp:291–314) | **ACCEPTED** | Table rewritten (E2 arm 9): arm C **expected MOVED, with a declared re-pin exactly on the D3 precedent** (test_conquest_match.cpp:2960–2968: the OFF arm carries the old literal as a gate leg, so the re-pin is declared, not discovered). A governor-OFF arm (all env_* keys 0) must reproduce `2ec5baddc8346cfe` bit-for-bit. E12/E12.1/P-F/pressure floor share the fixture — movers declared before the first gate run; **E12.1's red is attributed to backfill churn and must be re-attributed at the new operating point, never silently re-baselined.** |
| F5 | E1 arm (i) compares radial depth against ai_tape.py's min-over-boundary-samples great-circle metric (verified `dist_to_bubble_edge`, ai_tape.py:199–233: `unsigned = min(...great_circle_angle...)`) under an imported 49.8 m bound; aspect 17,400/13,050 = 1.33 makes the metrics differ O(100 m) at 3 km | **ACCEPTED** | The radial-depth formula goes into the Python side too; compare like with like; measure a fresh bound at HEAD before the leg gates. Sign-agreement clause kept (metric-independent). |
| F6 | "the second ellipse is already computed there and discarded" is FALSE — verified: `faction_ellipse(1 − own, …)` runs only inside the crush branch (instructor_tick.h:1599–1600); the both-alive case computes only the own ellipse | **ACCEPTED** | E1 text corrected: the rung **adds** the enemy-ellipse call at the leash-stamp site (one call beside :1580). The stale ":1559-1563" in drone.h:1253–1257's comment is noted for the builder, not fixed this rung. |
| F7 | d_knee as typed double-counts rs (verified: `a = base_a * s`, faction_bubbles.h:250–252) — verbatim it yields 10,875, not 8,700 | **ACCEPTED** | Formula corrected everywhere in this spec: `d_knee(θ) = shrink_radius_frac · r_eff_base(θ)` ≡ `(shrink_radius_frac / s_live) · r_eff_live(θ)`, with the subtractive-shrink note (conquest.h:646–649) that makes the identity hold. |
| F8 | Governor behaviour with no live dome stamped is unspecified; both readings have teeth | **ACCEPTED** | Specified: **stamps empty ⇒ governor structurally off** (the same superset-firewall honesty as `env == nullptr`). Consequences declared: legacy ground+atm fixtures without conquest stamps are bit-identical by construction; the both-domes-dead endgame keeps only the existing climb-fade + air-seek (un-deleted this rung) — declared residual in §7. |
| F9 | E2 arm 1's "the fight still shoots" may be unreachable as seeded (gun band 60/900, scenario.toml:444/:452; the i=0 seed is a receding player) | **ACCEPTED** | Player script specified: recede through the crossing, then converge; the OFF arm must produce `rounds > 0` at HEAD **before** the clause is trusted. |
| F10 | "replay" arms could be built open-loop (the `[.e17track]` shape), which cannot fail under a steering change | **ACCEPTED** | Stated in every replay arm: **closed-loop re-simulation**, seeded from the tape row's pos/vel/mode/orders, graded on survival + crossing altitude; countdown/latch state approximated and said so. |
| F11 | (a) −30° exceeds DefendOrder `gamma_cap` 0.52 rad = 29.8° (verified drone.h:1375); (b) the perch has no elevation seam — verified: the non-engaged order-free branch writes bank only (drone.h:3172–3179) | **ACCEPTED** | (a) The E3 dive is flown by the app-side phase mirror writing `target_gamma` directly (the D3 pattern, instructor_tick.h:1909+), which makes "the 0.52 cap untouched" true-and-decorative — said so. (b) The perch gets a **named seam**: a new app-stamped `GuardOrder` + a drone-side branch in the errand chain — E3 §change 4. |
| F12 | E4's constexpr→TOML hoist must edit the `fly_match` hand-mirror in the same commit (verified: test_conquest_match.cpp:1334–1336 passes `app::kDefendThreatRadiusM` / `app::kDefendersPerFaction` directly) | **ACCEPTED** | In E4's change list, same commit, by the hand-mirror law. |
| F13 | (a) `raid_fight_yield_m` = 2500 is a header default (drone.h:144), never `require`d; (b) the desc-cap fade has no shape or key; (c) s→0 guard order on the `shrink_jump` stamp; (d) the 34–41% violation figure is unverified here | **ACCEPTED** | (a) hoisted in E3, value unchanged. (b) new key `env_desc_fade_band_m` = 400 with a linear shape (below). (c) live-dome exclusion (`radius_scale < 1e-9`) evaluated **before** any division. (d) re-measured at HEAD before the first gate run, per the noise-floor law. |

**Nothing was REJECTED.** Both red teams were correct at every point this pass re-verified (12 independent re-checks: instructor_tick.h:1599–1600, faction_bubbles.h:249–252, conquest.h:646–649 + :377, ai_tape.py:199–233, test_conquest_match.cpp:1334–1336 / :2969–2970 / :574, maverick.h:298 / :623–639 / :269–271 / :1237, raid.h:232–320, drone.h:1375 / :144 / :410 / :1046–1056 / :2896–2925 / :3172–3255 / :3294–3386 / :3492–3493, scenario.toml:607/:609, load_scenario.cpp:291–314). SPEC v1's §0 contradiction-settling survives both attacks unchanged; its E2 formula, its bit-identity table, its "cannot regress" sentence, its E1 seam description, its d_knee notation, and its E4 deep-pump leg do not.

---

## §1. WHAT CHAD RULED, VERBATIM (2026-08-28)

> "I want the enemy ai to go low around the edges of the bubble in case theirs shrinks, I want them to have combat tactics, such that when they are in a fight they might climb if they have the availability of atmosphere, or when triggered to defend may climb and then use the dive (parabolic to get to enemy me faster if I am attacking their pump or they mine. SO two behaviors, one they prefer to stay in a bubble, but if they chase me out of one they should already be low. THey need an awareness of proximity ot the edge, if they are deep in bubble they can climb and get an energy advantage to be more effective defenders and attackers, make sure that ability persists. Safe flying on deck, energy fighting in the safe atmospheric zone, with an applied buffer for them to fight lower on the fringes. That is for survival as well last match I watched them plummet upon leaving the bubble from up high. YOu can maybe watch how I fly on the tape and give them some instruction from how I play?"

And earlier the same night:

> "okay see the photo, I watched enemy ai fly out of their own bubble to cross over, when I was close to them going to their pump, they flew out of the bubble from up high then fell and crashed. Watched an enemy ai near the end, leave my bubble for the no air zone and crash, there was our pump to the north undefended and they went to fly out of the bubble and crash rather than attack. Enemy now makes tunnel runs effectively, need more defense awareness too AND attack enemyplane and attack awareness too."

Standing rulings that bind this rung: **"chase you anywhere"** (2026-08-26 — no leash, no break-off, no give-up law) and **"stop nerfing them in secret"** (capability changes ship loudly, with their tables).

---

## §2. THE MEASURED PROBLEM

All numbers from tape 13 (`build-play/conquest_tape_13.jsonl`, 14:04.57 of sim, 42,240 drone rows) and the consult reports, verified against the code at HEAD.

* **The two watched deaths are flown high exits.** i=0: engaged at 5,996 m while at +1,762 inside its own dome (09:37.98), crossed the edge at **+1,673 engaged**, mushed in vacuum while the receding player opened 3,434→6,005 m, impacted at 211 m/s (10:21.72). i=9 (ally): errand/TRANSIT exit at **+1,941**, dead at 10:31 at 225 m/s **with the deck track law active** — the 12° cap (`deck_track_dive_cap_deg`, scenario.toml:609) sheds ~850 m in the ~24 s the airframe survives; it needed ~1,800. **Fixing the engaged predicate alone cannot fix i=9; the invariant is "never arrive at a dome edge high."**
* **Strandings mostly don't kill — yet.** 13 one-tick strandings across the three resizes, 12 clean recoveries by the ungated air-seek (drone.h:3221–3249), 1 recovery-dive terrain crash (i=5, 236 m/s arrival). But nothing bounds stranding altitude, and one resize moves the edge **8,700 m in one tick** (subtractive shrink: 0.5 × 17,400, conquest.h:646–649, faction_bubbles.h:250–252) — five drones were stranded simultaneously at 08:36.58 with zero re-plan.
* The measured survivable-crossing band is **+430…+920**; certain death flown level starts ≈ **+1,670**. The player's own stranding recovery: 2,292 m → deck in 24 s at γ −18° sustained, −25° peaks.
* **Defence is too slow to matter and half-blind.** Time-to-first-defender-within-900-m of the dying surface pump at HEAD: **∞** (closest any SUDBURY drone came while pump 1 died: 3,679 m). The scramble inherits `raid_speed_target = 123` (scenario.toml:1342, consumed at drone.h:2918–2919) — an orbit dial, not a sprint. A plain 245 m/s sprint answers a 9 km scramble in **42 s vs 73 s today**; a reactive climb-then-dive is 55 s (T's point-mass integration, T/W 0.61); a perch-dive from already-held altitude ties the sprint within 1 s and keeps the ruled parabola. `assign_defense` triggers only on proximity (2,500 m — the player crossed it ~8 s before the pump died) and reads only `pumps[0..1]` (raid.h:244–246; deep-defence deliberately out of scope, :237–238). Pump 3 died with the nearest enemy 5,102 m away.
* **The air-block scoping at HEAD** (settled at the braces): the climb-fade + air-seek (drone.h:3181–3251) sits inside `if (!tunnel_mode)` (:2896) and runs for defend/regroup/raid/patrol AND engaged non-tunnel ticks; a maverick **TRANSIT never gets it** — deliberate (maverick.h:631–635) and exactly what killed i=9. On deck-scope errand ticks the track law's unconditional REPLACE (:3338–3339) runs after the seek's `min()` and discards it.

---

## §3. THE DESIGN, WHOLE

One continuous field: signed horizontal depth `d` to the nearest **live** dome's hard edge — `d = max over live domes of (rs_eff(θ) − arc)` where the ellipse math is the existing shared law (`ellipse_frac` shape, drone.h:2009–2018) and a dome is live iff `radius_scale ≥ 1e-9` (floor is 0.0, conquest.h:377). The field maps to an altitude allowance `A(d)`, AGL, terrain-following:

```
A_raw(d) = min( env_edge_alt_m + env_slope·d ,
                env_plateau_m + env_slope·max(0, d − d_knee(θ)) )
A(d)     = max( A_raw(d), deck_track_agl_m )                      // the L1/F1 floor
d_knee(θ) = shrink_radius_frac · r_eff_base(θ)                    // ≡ (shrink_frac/s_live)·r_eff_live(θ);
                                                                  // subtractive shrink is what makes the identity hold
```

Deck lane outside (the floor) → `env_edge_alt_m` at the edge → a ramp through the fringe → a shrink-survivability **plateau** whose reach is the shrink law's own one-tick edge jump (8,700 m on the SUDBURY major axis, 6,525 m minor — **θ-dependent, s-independent; see DECISION #4**) → the existing climb-fade ceiling in the free core. It attaches as a **clamp-only constraint stage** on `target_gamma` immediately after the deck track law (between drone.h:3339 and :3340), evaluated over the six `deck_look` forward samples (drone.h:1046–1056) so the descent begins *before* the edge. **No behavioural predicate** — no `d.engaged`, no order flags, no mode reads. Its only exemptions are the three physical ones already at that seam: underground modes + arena (structurally outside the `!underground_mode && !arena_exempt` gate at :3294), and ballistic CLIMB/DIVE (`ballistic_owns_gamma`, :3331–3334). **TRANSIT is deliberately governed** — that is the i=9 fix, and it also touches the portal approach, which is why arm E2-A8 exists. The terrain latch still runs after and wins (:3346–3365, "pulling up always wins"). When no live dome is stamped, or when `max_i A(d_i) ≤ deck_track_agl_m`, the governor writes nothing — structurally inert, bit-identical.

On top of it: a standing energy **perch** deep inside (a new GuardOrder — the pre-positioned altitude the ruled parabola dives from), and a **defence scramble** that sprints at 245 and dives if already high, reusing the D3 phase-mirror pattern. Bank, direction, target, and pursuit are untouched everywhere: "chase you anywhere" governs where the nose points; the envelope governs only where flight exists. Awareness (E4) makes the trigger see HP loss, not just proximity.

**Ship order is a hard constraint: E1 → E2 → E3 (→ E4).** The perch without the envelope raises the stranded-altitude distribution ~1,500 m in exactly the population that died — an energy fighter without the envelope is a taller corpse. **The fly checklist is held until E3 relinks** (adjudication L8); if Chad flies between E2 and E3, the pre-brief in §8 goes in the reply verbatim.

---

## §4. THE RUNGS, IN BUILD ORDER

### RUNG E1 — THE DEPTH INSTRUMENT (the instrument before the feature)

**Change.** At the leash-stamp site (app/instructor_tick.h:1568–1609), **add** a second `world::faction_ellipse(1 − own, …)` call in the both-alive case (it exists today only in the crush branch, :1599–1600 — corrected per F6) and stamp both live ellipses on the drone, each tagged with its `radius_scale`. Live-dome exclusion (`radius_scale < 1e-9`) is evaluated **before** any derived scalar that divides by s (F13c). Drone-side helper computes `d` as §3. ⚠ Never read own-edge facts off `d.leash` — the stamp swaps to the ENEMY ellipse when the own dome dies (instructor_tick.h:1599–1600; trap documented at drone.h:1250–1257, whose own ":1559-1563" cite is stale — the code moved). Add one tape field (`ed`, signed depth) to the `d` row (app/conquest_tape.h) so later arms grade off the recorder.

**Seam.** instructor_tick.h:1568–1609; drone.h beside `ellipse_frac` (:2009–2018); conquest_tape.h `d`-row writer. **Hand-mirror law:** the new stamp goes into `fly_match`'s arm mirrors (test_conquest_match.cpp:1332–1366) AND test_stope_probe's in the same commit. **Graph regenerated in this commit** (it is stale on LOC at HEAD: instructor_tick.h 2790→2825, test_conquest_match.cpp 3816→3830, attributed to `28581e90c`), then `check`.

**Dials.** None. `shrink_radius_frac` = 0.5 (game.toml:388) is read, not moved.

**Arms, each with its required-red mutation.**
* **E1-A1, pure-law depth leg:** drone-side `d` vs the **same radial formula implemented in Python** (F5 — not `dist_to_bubble_edge`, which is a min-over-boundary great-circle metric, ai_tape.py:199–233; on a 1.33-aspect ellipse the two differ O(100 m) at 3 km). Fresh error bound measured at HEAD before the leg gates; sign-agreement clause kept. **Required-red mutation:** stamp only the own dome — `ed` diverges from the two-dome model on every tick where the enemy dome is nearer → red.
* **E1-A2, live-dome exclusion leg:** a fixture with one dome at rs=0 — depth MUST change when the dead dome is excluded. **Required-red mutation:** remove the `< 1e-9` exclusion → red.
* **E1-A3, bit-identity leg:** behaviour hashes bit-identical (instrument-only rung; here, and only here, bit-identity IS the pass). Its liveness proof is E1-A1/A2 above — the instrument demonstrably varies; a constant `ed` column across a full match is a red in A1.

**Regression clause.** Zero behaviour change; `[.bdeck]` arm C hash `2ec5baddc8346cfe` (test_conquest_match.cpp:2969) reproduced; recorder size delta noted.

### RUNG E2 — THE ENVELOPE GOVERNOR (the core)

**Change.** Insert between drone.h:3339 and :3340 — inside `!underground_mode && !arena_exempt` (:3294) and `env->ground` (:3295), upstream of the terrain latch (:3346–3365) and the B2 witness (:3492–3493):

```
// A(d), A_raw, d_knee as §3 — WITH the deck floor and the subtractive-shrink identity.
// Structural skips (each bit-identical, each with an exemption arm):
//   no live dome stamped  → skip (F8)
//   max_i A(d_i) ≤ deck_track_agl_m over the six samples → skip (L1/F1)
//   d − v·lookahead > d_knee → skip (perf; RT1-D: measure the frame cost anyway)
gamma_env = min over the 6 deck_look samples i of
            atan2( (r_terrain[i] + A(d_i)) − r_now , d_horiz[i] )
gamma_desc_cap = deck_track_dive_cap_deg
               + (env_desc_cap_deg − deck_track_dive_cap_deg)
                 · clamp((alt_agl − A(d_now)) / env_desc_fade_band_m, 0, 1)   // F13b: shape + key
target_gamma = min(target_gamma, max(gamma_env, −gamma_desc_cap))
// anti-porpoise: scale POSITIVE gamma by (A − alt)/env_fade_band_m on approach from below
```

`deck_look` is computed whenever the governor is armed even with `deck_on` false (today gated at :3302–3303). Exempt: underground modes + arena (structural), ballistic CLIMB/DIVE (reuse `ballistic_owns_gamma` — an unexempted governor stillbirths the 2,018 m apex exactly as the −12° cap nearly stillbirthed the D3 dive). **TRANSIT is governed on purpose** — the ⚠ from adjudication L3/F3: this touches the portal approach that Chad praised tonight ("Enemy now makes tunnel runs effectively"), which is why arm E2-A8 below is mandatory and why the "cannot regress" claim from SPEC v1 is retracted. Terrain latch still wins. Do **not** delete the climb-fade/air-seek this rung (walk-back discipline).

**Dials (all NEW keys, `require()`-loaded in load_scenario.cpp per the house style verified at :291–314, each with a 0 = OFF bit-identical guard; the two laws they supersede in effect — `avoid_air_dive_gamma` 0.35 header-only (drone.h:410) and `deck_track_dive_cap_deg` 12 — keep their shipped values):**

| key | proposed | alternative | note |
|---|---|---|---|
| `env_edge_alt_m` | 600 | 300 | ceiling AGL at d=0 — **DECISION #1** |
| `env_slope` | 0.364 (tan 20°) | 0.213 (tan 12°) | fringe ramp — **DECISION #1** (⚠ also a tunnel-approach dial — see the arm E2-A8 numbers in DECISION #1) |
| `env_plateau_m` | 1,650 | sweep 1,200–2,400 | settled by arm E2-A4 (arrival ≤ 210 m/s), not by taste |
| `env_desc_cap_deg` | 20 | — | the measured 13-recovery descent authority; the player's own drill is 18–25° |
| `env_desc_fade_band_m` | 400 | — | F13b: the cap opens linearly from 12°→20° over this band above the ceiling |
| `env_fade_band_m` | 150 | — | anti-porpoise approach fade |
| `d_knee` | **derived, NO key** | — | `shrink_radius_frac · r_eff_base(θ)` (F7-corrected); reproduces the measured 8,700 m one-tick jump; **its constancy under shrink is DECISION #4, not silently shipped** |

**Arms, each with its required-red mutation. Foes-present mandatory. Pre-gate steps first:** (a) read `deck_fight_ticks` (test_conquest_match.cpp:574) in the shipped `[.deckx]` printout — a small population makes a near-identical `[.deckx]` prove nothing; (b) re-measure the "34–41% of flight samples violate the field" figure at HEAD (F13d) — it is the floor under every must-move expectation below.

1. **E2-A1, foes-present envelope arm** — two-faction fixture at the i=0 09:37 geometry (engaged at 5,996 m, alt +1,762). Player script (F9): **recede through the crossing, then converge** — and the OFF arm must produce `rounds > 0` at HEAD before the shoot clause is trusted. Grades: p90 crossing-entry altitude ≤ env_edge_alt_m + 150; zero thin-air deaths; `wants_fire` ticks > 0 and rounds > 0; **min range achieved** (L5's convergence stat — vertical disengagement must be measurable). **Required-red mutation:** all env_* keys → 0 (governor OFF) — crossing altitude reverts to the +1,670 class → red.
2. **E2-A2, the i=9 replay** — **closed-loop re-simulation** (F10), seeded from the tape row's pos/vel/mode/orders (TRANSIT, +1,941 exit); countdown/latch state approximated and declared. **Red at HEAD by construction** (it dies with the track law active — bit-identical here = fake fix). **Required-red mutation:** gate the governor on `!is_tunnel_mode` (re-create the TRANSIT exclusion) → red again.
3. **E2-A3, the i=0 replay** — closed-loop, engaged gradual-skirt exit from +1,673, receding target. Red at HEAD. **Required-red mutation:** add `!d.engaged` to the governor's gate → red again.
4. **E2-A4, shrink replay** — scripted rs 1.25→0.75 with the fleet at the measured 08:36 states plus one aircraft at the worst legal point (d = d_knee, alt = plateau; plateau 1,650 vs certain-death ≈ 1,670 — the margin is the *drill*, and this seed stresses it). Grades: all recover af ≥ 0.7; deck arrival ≤ 210 m/s (236 crashed); 0 deaths in 60 s. Repeat for the 10:41 collapse states. **Required-red mutation:** `env_plateau_m` → 3,500 → arrival > 210 → red (proves the plateau value is load-bearing).
5. **E2-A5, energy persistence** (the ruling's "make sure that ability persists") — deep-core fixture: an ordered climb reaches ≥ 2,000 m and holds, un-clamped. **Runs at s=1.25 AND s=0.75** (L2 — at 0.75 the free core is the inner 4,350 m; seed inside it). **Required-red mutation:** flatten A to deck (`env_plateau_m`→0, `env_slope`→0) → red — the tripwire against the design quietly becoming deck-everywhere, which the ruling itself bans.
6. **E2-A6, fringe dwell + mute-chatter leg** — fringe ridge fixture; p95 ceiling-violation dwell bounded (bound set from the OFF measurement); guns not muted by latch chatter; plus one leashed-tick sub-case asserting no `wants_fire` oscillation between the leash mute (drone.h:3178) and the governor's descent (RT1-C). **Required-red mutation:** remove the anti-porpoise fade → dwell/chatter exceeds bound → red.
7. **E2-A7, deep-outside transit leg** (NEW — L1/F1) — errand transit seeded 10 km outside any dome (the measured transits run 33.7/39.2 km at med 243 m). With the floor + structural skip, governor ON must be **bit-identical to OFF** on this leg — the S1-DECK lane untouched to the bit. **Required-red mutation:** remove the `max_i A ≤ deck_track_agl_m` skip → the −20° command vs the 60/110 latch produces the limit cycle → gun-mute fraction spikes → red.
8. **E2-A8, portal-approach arm** (NEW — L3/F3) — shrunk dome, mouth ~1.1 km outside the edge (the i=3 12:42 geometry), governor ON vs OFF, **under both DECISION-#1 dial pairs**. Grades: DIVE_IN armings, go-around count (the `along_raw > leg + dive_arm_reach_m` reset, maverick.h:1241–1243), mouth clearance. Pass: shipped-pair armings within the measured floor of OFF. **Required-red condition:** pair (b) at this geometry (cross error 253–317 m vs `transit_line_tol_m` = 300, maverick.h:298) is expected red — that red is DECISION #1 evidence, and the pair that ships must be green here.
9. **E2-A9, the bit-identity table, rewritten (F2), declared before the first gate run:**
   * **Expected bit-identical, each proven by an exemption-removal mutation that must move/red:** bore arms, arena arms, env-null closed-loop legs (test_drone.cpp:1354/:1413 — bit-identical *by construction*; new closed-loop legs must build atm+ground), stamps-empty legacy fixtures (F8), governor-OFF arm ↔ `2ec5baddc8346cfe`.
   * **Expected MOVED:** `[.bdeck]` arm C — **declared re-pin on the D3 precedent** (test_conquest_match.cpp:2960–2968: the feature ships ON, the literal moves with it, and the OFF arm asserting the old literal is the gate leg that makes it honest). Errand deck arms (`[.deckx]` family, `[.e17track]` – noting `[.e17track]` is open-loop and moves only via inputs it replays, so it may NOT move: declare which), full sweeps, and the signed rows already broken by BALLISTIC ⚠D (`[.floor2x2]`, S1-DECK's in-bubble proofs).
   * **E12 / E12.1 / P-F / the pressure floor share arm C's fixture** (the fixture-wide-change law). E12.1 is red at HEAD, attributed to backfill churn — it must be **re-attributed at the new operating point, never silently re-baselined**, and P-F is not bent a fourth time.

**Regression clause.** Raid pressure ≥ 0.0448; **deck survival ≥ 0.700; crashes ≤ 0.36/min** (L1 — the two exactly-reproduced BALLISTIC §2 rows); E12 stays green; no new red among the four sled debts; tunnel offense graded by E2-A8 (the "cannot regress" *claim* is retracted; the *arm* replaces it); no leash/break-off anywhere (chase-anywhere = the `!d.engaged` term at :3337 plus no non-Extend leash); P-A rounds/hits quoted only with the noise arm (the statistic spans 4–39 for identical arms at n=8); frame-time delta of the fleet-wide `deck_look` printed (RT1-D). This rung is **not** graded on bore deaths — the 5-of-7 engaged in-net crashes are C2's rung.

### RUNG E3 — THE DEFENCE SCRAMBLE + THE STANDING PERCH

**Change** (defend branch drone.h:2896–2925 + an app-side phase mirror on the D3 pattern, instructor_tick.h:1909+ — NOT a BFM mode; the branch stands down at `fight_hot`, whose `raid_fight_yield_m` = 2500 is hoisted to TOML this rung, value unchanged — F13a):
1. **Sprint:** scramble legs fly `defend_sprint_speed` = 245 instead of inheriting `raid_speed_target` = 123 (scenario.toml:1342, consumed drone.h:2918–2919 — an orbit dial, never a scramble); release to 123 inside 2,500 m of `defend.target_pos` for the C1 turn radius, and the fixture asserts full deflection authority is recovered before the merge (L10b — 245 = `v_redline`, aircraft.toml:114, where `min_frac` deflection begins).
2. **Dive-if-high:** if the standing posture supplied altitude, the phase mirror pushes over at `defend_dive_gamma_deg` (−18°), steepening to `defend_dive_close_gamma_deg` (−30°) inside `defend_dive_close_range_m` (4,000 m) — **flyable because the mirror writes `target_gamma` directly** (the D3 pattern); DefendOrder's 0.52 rad `gamma_cap` (drone.h:1375, no TOML key) is bypassed by that construction, which makes "untouched" true-and-decorative, said here so nobody claims it as protection (F11a). Speed command 245; recovery = the D3 RUN handoff verbatim.
3. **Abort:** any tick where `A()` at the current/look position drops below current altitude — fringe drift or a resize — the dive re-aims at the deck lane and the phase goes SPENT (the i=4 survival shape: cross already descending).
4. **The perch** (the ruled "might climb if they have the availability of atmosphere", standing-posture half): a new **GuardOrder** — app-stamped at the order site beside defend (instructor_tick.h leash/defend stamp region), drone-side branch inserted **last in the errand chain** (after defend/regroup/raid/strike, before the plain-leash arm at drone.h:3172) so it never starves an order: non-engaged, order-free drones deep inside (`d > d_knee + 500`) fly `aim_at` toward a guard point above the faction's standing station at target alt = min(`guard_alt_hi_m`, A − 300). This is F11b's named seam — the elevation channel exists because the branch, like defend, owns `target_gamma` via `aim_at`. The fleet already loiters at 1,807–2,013 m; the posture formalizes it and couples it to the edge, which is the part that kills them today.

**Dials (NEW, `require()`-loaded, 0 = OFF):** `defend_sprint_speed` = 245; `defend_dive_gamma_deg` = 18; `defend_dive_close_gamma_deg` = 30; `defend_dive_close_range_m` = 4,000; `guard_alt_lo_m`/`guard_alt_hi_m` = 1,500/2,500 (envelope-clamped to ~1,200–1,500 near pumps, which sit ~4.9 km inside the edge); `raid_fight_yield_m` = 2,500 hoisted (unchanged).

**Arms, each with its required-red mutation.**
* **T-1, defence arrival** (foes present): first defender within 900 m in < 50 s at 9 km; zero wrecks on-profile; authority-recovery assert at the release. **Required-red mutation:** `defend_sprint_speed` → 0 → 73 s → red.
* **T-2, arrival energy:** ≥ 220 m/s at the merge (baseline 123–156). **Required-red mutation:** same OFF switch → red.
* **T-3, the perch pays:** scripted 270 m/s attacker strafing the pump; perch ON vs OFF; grades in-band-on-attacker fraction + time-to-first-solution. This is the arm that can refute the whole player-facing value claim — if it does not move, the perch is spectacle and is reported as such. **Required-red mutation:** GuardOrder never stamped (dial 0) must change the arm's trajectory hashes — bit-identical = dead branch = red on the liveness leg.
* **T-5, fringe/shrink abort:** replay the 08:36.58 −8,700 m jump with a perched defender and a mid-dive intercept; fails at HEAD by construction. **Required-red mutation:** remove the abort clause (3) → red again.
* **T-4 is an INSTRUMENT, not an arm** (re-classified — an arm without a required-red does not ship in this spec): the AI-vs-AI in-band fraction vs the 0.30% baseline is printed in the gate output, expected near-null, carried so R-CLOSE's claim stays falsifiable in both directions. It gates nothing.

**Regression clause.** Raid pressure ≥ 0.0448 (defenders drawn from non-raiders first, raid.h:301–307 — the E14 cannibalization lesson); D3's own hash arms still move; tunnel/deck mix untouched; crashes/min not up; **declared blast radius: the sprint moves the defend branch for BOTH factions — the player will see his own allies scramble away at 245 m/s** (RT2 part C; goes in the fly note).

### RUNG E4 — DEFENCE AWARENESS (the trigger; separable, buildable after E3 — and therefore not independently gradeable standalone; declared)

**Change.** `assign_defense` (raid.h:232–320) additionally triggers on **surface-pump HP lost** — `defend_hp_delta_trigger`, **signed on losses only** (the millwright canon adds repair ticks that raise HP; an unsigned delta would scramble defence on the repair itself — L9). Hoist `kDefendThreatRadiusM` = 2,500 / `kDefendersPerFaction` = 2 (instructor_tick.h:60–61) to TOML, values unchanged, **editing the `fly_match` hand-mirror (test_conquest_match.cpp:1334–1336) in the same commit** (F12). **Deep pumps are NOT in this rung's build scope** — adjudication L4/F4: the order is unflyable as the code stands (defend branch gated `!tunnel_mode` at drone.h:2896–2897; tunnel-committed pilots excluded from candidacy at raid.h:291–307; the target is ~1,800 m underground), and an arm grading `defend.active == true` is the E14 disease raid.h:281–286 names in its own comment. Deep-pump defence goes to Chad as **DECISION #5**.

**Dials.** `defend_threat_radius_m` = 2,500 (was constexpr); `defenders_per_faction` = 2 (was constexpr); `defend_hp_delta_trigger` NEW (X HP lost in Y s; swept in-fixture).

**Arm.** **E4-A1, the pump-1 window replay:** closed-loop, the 08:12–10:41 HP trace; at HEAD no defender ever entered 900 m; the fix must produce a first-defender arrival inside the window. **Required-red mutation:** `defend_hp_delta_trigger` → 0 (proximity-only) → the 8-second window returns → red.

**Regression clause.** Raid pressure floor again; tunnel-committed pilots stay exempt from defender designation (do not widen the candidate pool this rung); no defence armed by a repair tick (a fixture leg with rising HP asserts zero triggers).

---

## §5. THE PLAYER-DERIVED INSTRUCTIONS — first-class content (Chad: "watch how I fly on the tape and give them some instruction from how I play")

Mined from his 4,224 rows in tape 13, with disposition in THIS spec:

| # | instruction, with his numbers | disposition |
|---|---|---|
| R1 | **Altitude allowance vs edge distance**: deck outside (med 243, p90 573); ≤ ~300 within 1 km of the edge (fringe-fight med 279); ~1,000–1,300 within 2 km (med 1,025 — his one exception at 1,301 m cost him 92.7 hp); free deep (above 800 m 96% of the time, zooms to 4,368) | **IS Rung E2.** His fringe numbers are the conservative pair in DECISION #1 |
| R2 | **Never cross an edge outbound above the deck; if high, be descending 18–25° through it** (his stranding recovery: 2,292 → deck in 24 s at γ −18° sustained, −25° peaks; the invariant: never be outside air with more altitude than your descent authority sheds in ~25 s) | E2's forward sampling + `env_desc_cap_deg` = 20, faded per F13b. The raise-the-track-cap variant stays cut — the fade subsumes it without moving the signed near-deck law |
| R3 | **The stranding drill**: immediate committed dive, full authority, deck in ~24 s, no plan continuation at altitude | The ungated seek already does this for non-TRANSIT ticks (12/13 survived); E2 extends it to TRANSIT and bounds the starting altitude to the plateau. Pre-shrink anticipation stays cut — re-propose only if E2-A4 fails at the plateau |
| R4 | **The defence parabola**: dive from held altitude at γ −30/−35°, ~0.57 m alt per m of closure, arrival ≈ v₀²+2g·Δh·0.9, average closure 268 vs the fleet's 143 m/s | **IS Rung E3**, climb moved BEFORE the trigger (the perch), arrival capped at 245 (`v_redline`, aircraft.toml:114 — past it the compression ramp strips authority; D3's 243 m/s arrivals proven) |
| R5 | **The energy climb, deep inside only**: +1,600–2,700 m climbs, 5 of 6 at edge < −2,500 m; the sixth nearly killed him | The perch (E3.4) + E2's deep allowance; the deep band is plateau-capped inside d_knee — said honestly, and the extent of that cap is DECISION #4 |
| R6 | **The pump-attack profile**: deck penetration at pump-AGL 220–240, pop-up at 6,294 m, push-over at 2,166 m, strafe 295 m/s at 50 m AGL, egress on the deck | **DEFERRED** to the raid-attack-pattern rung (`raid_attack_alt_m`/`raid_reattack_m`, Chad's open ruling). Constants archived here for that rung — which is also where AA-vs-attack-run shape gets decided under the millwright canon |
| R7 | **Bore fighting: protect the line, don't copy the speed** (his survival = 47.7 °/s in-net turn authority vs their 13.3; he *slows* to 199–221 where tight) | **C2's rung.** This spec's rungs are not graded on bore deaths |
| R8 | **Long dead-air transits are deck transits** (33.7/39.2 km at med 243 m) | E2's floored field outside — and E2-A7 now proves the governor leaves that lane untouched to the bit |
| — | Not transferable, named: gun conversion at 16–100 m (mechanism debt); the 5.1 km ballistic hop (n=1; E16 measured and rejected the hop, drone.h:3257–3267); bait-luring a chaser home (collides with chase-anywhere; its harmless kernel IS R1) | recorded, not built |

---

## §6. DECISIONS FOR CHAD — numbered, nothing pre-picked

*(Delivery note, per adjudication L6: your same-night "AND attack enemyplane and attack awareness too" is NOT this rung — it is R-CLOSE, the next rung; 85% of engaged samples never enter pursuit and the chaser is commanded slower than its target, 131 vs 143 m/s. This rung carries its baseline (0.3% in-band) as an instrument so the claim stays falsifiable. If you fly E1–E4 and still watch allies end matches untouched, that is the queued rung, not this one failing.)*

1. **The fringe dial pair (`env_edge_alt_m`, `env_slope`) — this is now TWO dials in one: fringe feel AND tunnel-approach life.** (a) Conservative 300 / tan 12°: matches your own fringe flying (med 279 in fights within 1 km), strands nothing — but would have clipped 100% of the samples of tape 13's best fight (the i=6 endgame that took you to 7.3 hp: depth med 741, alt med 1,110; A(741)=458), **and at the measured post-shrink portal geometry it leaves 253–317 m of cross error against the 300 m arming tolerance — the tunnel raid arming goes marginal-to-dead** (arm E2-A8 measures it). (b) Loose 600 / tan 20°: A(741)=870 — trims the top third of that same fight; portal cross error 0–100 m, arming survives. Both pairs are inside the survival margin (survivable crossings +430…+920; certain death ~+1,670). The build carries both as sweep arms plus the i=6-window replay under each; your fly verdict picks.
2. **The triggered-defence shape from LOW.** Already-high defenders dive and tie the sprint (42 s at 9 km) with the parabola you described. From low: full climb-then-dive = 55 s; straight sprint = 42 s; today = 73 s. Ship dive-if-high / sprint-if-low, or the literal climb-then-dive always, paying ~13 s for the shape?
3. **The engaged-outside altitude law — confirming our reading.** We read "if they chase me out of one they should already be low … Safe flying on deck" as (b)-generalized: the continuous ceiling, under which BFM keeps nose and bank and only the climb is clamped. (a) = full deck track law on engaged outside ticks (your own outside flying matches it, med 243 engaged or not) takes more BFM freedom; (c) leave-it keeps both watched deaths. Confirm (b), or rule (a).
4. **★ NEW — how far in does the buffer reach, and does it shrink with the dome?** The buffer's reach is your shrink insurance, and the arithmetic makes it **constant**: half the base radius (~8,700 m on the long axis), whatever the dome's current size. Consequences, in one table (SUDBURY, long axis; "free core" = where full energy climb is allowed):

   | dome state | radius | free core | share of dome area | fleet exposure today |
   |---|---|---|---|---|
   | s = 1.25 (now) | 21,750 | inner 13,050 | 36% | enemy median depth 6,852 m is INSIDE the capped ring; 5,114 samples above 1,800 m would cap at 1,650 |
   | s = 0.75 (one pump lost) | 13,050 | inner 4,350 | 11% | nearly the whole dome fights ≤ 1,650 |
   | s = 0.5 | 10,875 | none | 0% | whole faction ≤ plateau |

   **(a) Constant knee** (ships the full insurance): any single shrink strands nobody above the plateau, ever — but after one pump loss the energy game you ordered barely exists, exactly when "killers to contend with" matters most, and inside the ring you can vertically disengage any pursuer by out-climbing A(d). **(b) Live-fraction knee** (`d_knee = 0.5·r_eff_live`): the free core stays 36% of the dome at every size — but the insurance under-covers the next shrink: at s=0.75 the knee is 6,525 m against a real 8,700 m jump, a 2,175 m band where a drone at the plateau can still be stranded above it (the drill then has to save it, as it saves 12/13 today). Numbers on both sides; your call.
5. **★ NEW — deep-pump defence.** Pump 3 died with the nearest enemy 5,102 m away, and today deep pumps have **no defence trigger at all** (raid.h:244–246, deliberate per :237–238). But a defend order at a deep pump is unflyable as the code stands: the defend branch cannot run in tunnel modes, tunnel-committed pilots cannot hold the order, and the target is ~1,800 m under the terrain — a defender would circle the dirt above it with guns muted. Options: **(a)** leave deep pumps undefended this rung (E4 as built — surface + HP-delta only), the stope fight stays carried by strike/run traffic; **(b)** a portal-mouth picket — station a defender at the own portal mouth (a reachable surface point that interdicts the bore route), graded on time-to-station; it does nothing against an attacker already in the stope; **(c)** real deep defence, which needs the tunnel brain to route a defender down the bore — a later rung with its own spec. Nothing built until you pick.
6. **A consequence to feel on the fly, not a dial:** near the fringe you keep an energy sanctuary the defenders cannot climb into (your med 1,269 m in the 0–1 km band vs their ~460–870 clamp). The design answer is that intercepts route inward and arrive diving; a foe-altitude exception is refused by mechanism (it is precisely how i=0 died). If it plays wrong, say so and we bring numbers.

---

## §7. WHAT WE REFUSE TO DO, AND WHY

1. **Any leash, break-off range, or foe-altitude exception** — banned by "chase you anywhere" and by mechanism (i=0's death IS a foe-altitude story).
2. **R-RESIZE event/re-plan machinery** — the field is stamped from live `radius_scale` every app tick and remaps the fleet on the resize tick with zero event code.
3. **Raising `deck_track_dive_cap_deg`** — the desc-cap fade (12°→20° over `env_desc_fade_band_m`) subsumes it without moving a signed near-deck law.
4. **Pre-shrink anticipation predicates, time-to-crossing latches** — a latch is a predicate, and a scope predicate is a silent shadow (the tape-13 law); the predictive element lives in sampling the field along `deck_look`.
5. **Reactive climb-then-dive as the default** — measured 10–14 s slower than the sprint; survives only as DECISION #2.
6. **Deleting the climb-fade/air-seek this rung** — walk-back discipline; the governor supersedes them at its seam but they stay (and they are all that governs the both-domes-dead endgame, a declared residual).
7. **R-CLOSE** — not this spec (told to Chad this time, §6 preamble); its baseline is carried as an instrument.
8. **The deep-pump defend order as spec'd in v1** — an order the code structurally refuses to fly, graded on a flag: the E14 disease verbatim. Escalated instead (DECISION #5).
9. **Building on the "second ellipse already computed" fiction, the double-counted knee, or the floorless A(d)** — all three v1 defects are corrected above, not worked around.
10. **Grading this rung on bore deaths (C2's), on `[.deckx]`'s banned p10, or on P-A rounds/hits without the noise arm** (spans 4–39 for identical arms at n=8).

**Known residuals, declared (so the next "they fell from the sky" is a number, not a surprise):** collapse (s→0) exposure remains for the deep core, up to ~3.5 km of stranding altitude beyond d_knee — collapse-proofing everyone is deck-everywhere, which the ruling rejects; the both-domes-dead endgame has no governor (stamps empty ⇒ off, F8) and keeps only the air-seek; a synchronized shrink noses every violator over in the same tick — reads scripted, is also literally the ruled behaviour; routine transit inside the plateau ring drops ~200–500 m; the fleet may flat-top at exactly A(d) near the fringe — partly the ruled "progressively lower", partly a look to check.

---

## §8. THE LAWS AND TRAPS THIS RUNG MUST RESPECT (carried from the handoffs)

* **Re-measure every headline at the final head, with the noise arm, before writing it down** — four of twelve D3 headlines died in re-measurement; noise floors are per-statistic and move when the feature lands.
* **A scope predicate is a silent shadow**: enumerate the population a gate EXCLUDES and measure its outcomes (`d.engaged` shadowed two rungs' work).
* **A bit-identical arm means blind fixture OR dead branch; a moved hash proves the order exists, not that an aeroplane turned; a threshold clause grades a floor, not a delta** — every arm above therefore carries a required-red mutation, and T-4 was demoted to an instrument for lacking one.
* **The instrument before the feature** (E1 before E2); **a derived bound beats a dial** (d_knee has no key — but its LAW is DECISION #4, because a derived bound that changes the game's shape still ships loudly).
* **A constant or hand-maintained list describing the shipped table stops describing it when the table moves — READ THE LOADER** (`require()`, no defaults; ⚠ RaidOrder/DefendOrder gains have no TOML key); **a hand-mirror is not a caller** (fly_match edits land in the same commit, twice in this spec: E1's stamp, E4's constants).
* **A fixture-wide change moves every arm that shares the fixture** — the arm-C re-pin is declared with its OFF-arm gate leg, the D3 precedent verbatim; **do not bend P-F a fourth time; do not silently re-baseline E12.1.**
* **A location predicate is not an altitude predicate** (the D3 porpoise); **guard every `normalize()`**; **a Catch2 name with a comma selects nothing**; **non-ASCII names silently never run.**
* **Graph, not grep**; regenerate `generated/graph/` **last, in the same commit** as the structural change (it is stale on LOC at HEAD, attributed to `28581e90c`), then `check`.
* **Do-not-touch:** `drone/bfm.h` (zero edits — the governor is upstream of the B2 witness at drone.h:3492–3493, downstream of BFM; note for C2: bfm.h has **six** `aim_at` callers at HEAD — :1064, :1109, :1124, :1180, :1197, :1226 — the hand-maintained "four call sites" count is stale), `maverick::aim_at` itself, `sim/`, `control/`, `assets/`. Do not push; commit freely on `sandbox/enemy-ai`. Never relink during a sweep; relink the fly build last and quote the stamp.
* **The fly:** checklist goes IN the reply with the absolute exe path (`D:\seads_sandboxes\enemy-ai\build-play\seads.exe`), held until E3 relinks. Pre-brief if he flies early: *"E2 alone makes them fly lower everywhere near the fringe — the perch and the 245 m/s scramble are the next relink. Your own allies will also scramble at 245 once E3 lands. In-fight climbing deep inside is permission this rung, not a new tactic — BFM is untouched by design."*
---

## §9. CHAD'S RULINGS ON §6 — ALL FIVE CLOSED, 2026-08-29

He answered §6 with **"I agree with all recommendations made"**, plus two explicit
picks on the two decisions that carried no recommendation. Resolved:

1. **Fringe pair — LOOSE ships: `env_edge_alt_m` = 600, `env_slope` = tan 20°
   (0.364).** Not a taste call: the spec's own constraint is that the pair which
   ships must be GREEN on arm E2-A8, and the tight pair (300 / tan 12°) is
   expected red there — at the measured post-shrink portal geometry it leaves
   253–317 m of cross error against `transit_line_tol_m` = 300 (maverick.h:298),
   i.e. it makes the tunnel raid arming marginal-to-dead. ⚠ The tight pair is
   still carried as a sweep arm plus the i=6-window replay, because his fly
   verdict may still prefer its fringe feel — but it does not ship unless E2-A8
   comes back green on it, which the arithmetic says it will not.
2. **Defence from LOW — dive-if-high / sprint-if-low.** The literal
   climb-then-dive-always costs ~13 s (55 s vs 42 s at 9 km) and is refused as a
   default by §7.5; the ruled parabola is preserved for defenders that already
   hold altitude, which is what the perch (E3.4) exists to guarantee.
3. **Engaged-outside — reading (b)-generalized CONFIRMED.** The continuous
   ceiling; BFM keeps nose and bank; only the climb is clamped. The full deck
   track law on engaged outside ticks is NOT taken.
4. **★ The knee is CONSTANT** — `d_knee(θ) = shrink_radius_frac · r_eff_base(θ)`,
   half the BASE radius whatever the dome's current size. Full shrink insurance:
   no single shrink can strand anyone above the plateau. Declared cost, loudly:
   the free core is 36% of the dome at s=1.25, **11% at s=0.75, 0 at s=0.5** —
   but the plateau is still 1,650 m of energy band, and the trim on the samples
   actually affected is ~150 m (5,114 samples above 1,800 m cap at 1,650), with
   the enemy median depth 6,852 m already inside the capped ring today.
   ⚠ **The residual in §7 stands and is now RULED, not merely declared:** at
   collapse (s→0) the deep core keeps up to ~3.5 km of stranding exposure beyond
   d_knee. Collapse-proofing everyone is deck-everywhere, which the ruling bans.
5. **★ Deep pumps stay UNDEFENDED this rung.** E4 ships surface pumps + the
   signed HP-delta trigger only — fully gradeable, no arm that grades a flag.
   Neither the portal-mouth picket nor real bore-routed deep defence is built.
   Revisit AFTER the fly, because R-CLOSE (the 0.3% in-band figure) is the larger
   lever and may change what deep defence needs to be.

**Nothing else in this spec is open. Build order E1 → E2 → E3 → E4 stands as a
hard constraint; the fly checklist is held until E3 relinks.**
