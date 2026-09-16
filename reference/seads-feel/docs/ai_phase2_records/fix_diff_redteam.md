# RED-TEAM VERDICT — AI Phase-2 fix set (F1/F2/F3/F4)

Worktree reviewed: `D:\seads_sandboxes\ai-fix-forge` @ uncommitted working tree
(branch `sandbox/ai-fixes`), 2026-08-07. Nothing edited. All probes built
standalone with g++ in `D:\seads_sandboxes\ai-probes\redteam\` (the worktree's
`build/` was never written to).

**VERDICT: SOUND-WITH-FIXES.** The firewall holds, F3 is clean, F4 reproduces
its claimed number exactly, and F1's headline claim is now PROBE-CONFIRMED
(`wants_fire` 0 -> 90 on the regression case the F1 attribution named). Three
things must not ship as they stand: a config comment that describes a change
that was reverted, an Intercept catch-up law that provably cannot close, and a
respawn-placement predicate that can plant a drone in vacuum.

---

## Probes run (all reproducible from this directory)

| probe | source | log | what it answers |
|---|---|---|---|
| F4 fleet | `fleet_fix.cpp` (f4's `fleet.cpp`, paths repointed at THIS worktree) | `fleet60_fix.txt` | in-net death rate under the shipped dials |
| F4 offset matrix | `offset_matrix.cpp` | `offset_matrix.txt` | is the TU's anti-vacuity mutation arm real? |
| F1 rerun | `probe_f1.cpp` (f1's, `-DSEADS_CONFIG_DIR` = this worktree) | `f1_fix.txt` | does the fire chain actually open now? |
| F2 ceiling | `f2_ceiling.cpp` | (stdout, inline below) | is the relocation point breathable as the dome shrinks? |

### F4 re-measurement — REPRODUCED (21%)

`./fleet_fix.exe --min=60`, real DEM (`assets/sudbury_dem.png`), real
`[tunnel]` dials, this worktree's `scenario.toml` (`lookahead_m = 500`,
`centerline_gain = 1.0`):

```
entries via Errington : 17   in-net deaths: 5   clean exits (either end): 11
entries via Murray    : 12   in-net deaths: 1   clean exits: 12
TOTAL entries 29, in-net deaths 6 (21%), surface crashes 31
```

The probe author's 69% -> 21% claim is **confirmed, not refuted** (their
pre-fix arm was 25/36 = 69% on the identical harness). The lateral bore-wall
mode is gone: of the 6 residual deaths, 4 are the SAME `BOREAL` pit-crown clip
(`n_up = -1.00`, 1.45 s / 256 m in, byte-identical coords each time —
`[-8000.4 -1125.2 12489.3]`), 1 is the Errington lateral ramp (`n_lat = 1.00`,
3.48 s), 1 is the Murray mouth. Matches the attribution's "what survives is the
pit-crown clip, a separate defect". The 31 surface crashes (outside the net)
are untouched by F4 and are what FIX-F2 now relocates.

### F1 re-run — the fire chain OPENS (the claim is honest)

`probe_f1_fix.exe`, this worktree's config (`extend_energy_m = 3000`,
`attack_range_m = 1200`, the catch_up law):

| case | before (f1/run3.txt) | now (f1_fix.txt) |
|---|---|---|
| head-on 3 km, player 250 m/s (**the named regression case**) | `wants_fire=0`, rounds 0, guns_hot∧in_band 76 | **`wants_fire=90`, rounds live 1361 ticks, guns_hot∧in_band 702** |
| head-on 3 km, player 85 m/s | 147 | 147 (unchanged, healthy) |
| tail 600 m / 400 m | 1040 / 513 | 1040 / 513 (unchanged) |
| ATM-2 bare deck | 0 | 0 (thin-air case, unchanged) |

Also answers the brief's Extend question with evidence: Extend is NOT dead
where it was healthy — the AI-matched cases still fire it via frustration
(`head-on 85 m/s`: Extend duty 1920, `->Extend(frust)=2`; `tail 600 m`: 9
frustration exits). Only the structurally-unwinnable player-speed case now
skips it, which is the intent. **No finding.** Goldens/controller surfaces
cannot move: `git diff --stat` touches no `sim/`, no `control/`, no
`test/harness/`, no golden file; `drone/bfm.h` gained no include.

---

## P0 — would ship a lie

### P0-1 `config/scenario.toml:374-378` — the config states a dial change that was reverted

```toml
# FIX-F1 (...): 1200 -> 750 — the merge range must land the guns-hot dwell
# INSIDE the fire band (fire_range_max = 600), else Offensive spends its whole
# spell outside gun range and wants_fire never opens. ...
bfm_attack_range_m      = 1200.0   # [m] inside this, the merge is on
```

The value is 1200. The comment asserts 750 and gives a rationale for a change
that is not in the file, ending "Dial — flagged for Chad's stick." Chad is
being asked to rule on this dial FROM THIS COMMENT. Same lie is repeated in
`test/unit/test_bfm_fastplayer.cpp:15-17` ("THE FIX ... `bfm_attack_range_m`
1200 -> 750") and in the F1 section of `BUILD_NOTES_FIXES.md` (corrected only
by a footnote 140 lines later). Cost to fix: rewrite the comment to record
that 750 was TRIED and reverted, and why (the game-loop certificate's C kill
chain). Note the probe rerun above shows 1200 is now defensible on its own
merits — `guns_hot AND in_band` went 76 -> 702 with the energy bail alone — so
the honest comment is also the favourable one.

---

## P1

### P1-1 `drone/bfm.h:541-545` — the catch_up law can never close a stern chase (mechanism ships non-functional)

```cpp
const double catch_up = std::min(
    dl.pursue_speed_bump,
    std::max(0.0, glm::length(player.velocity) - dl.speed));
cmd.speed_target = dl.speed + std::max(bp.intercept_speed_bump, catch_up);
```

Whenever `|v_target| - dl.speed <= pursue_speed_bump`, this evaluates to
`speed_target == |v_target|` **exactly** — the interceptor is commanded to
MATCH the target's speed, so closing rate is zero by construction; and when the
deficit exceeds the bump it is capped at `speed + 90 = 175` against a 250 m/s
player. Either way the range asymptotes. Measured (`f1_fix.txt`), the exact
scenario FIX-F1's second-order item exists to fix:

| stern chase 2 km | pre-fix (run3.txt) | now |
|---|---|---|
| player 250 m/s | Offensive entries 0, min_range 1989 m | **0, 1989 m** |
| player 120 m/s | Offensive entries 0, min_range 1988 m | **0, 1834 m** |

The spec's plain `max(intercept_speed_bump, pursue_speed_bump)` would have
commanded 175 m/s and closed the 120 m/s case outright. The amendment was made
to stop 175 m/s intercepts overshooting SLOW certificate targets — that is a
real constraint, but `min(bump, deficit)` overcorrects into "never faster than
the target". A law that satisfies both: `catch_up = min(pursue_speed_bump,
max(0, |v_target| - dl.speed) + closing_margin)` with a small margin (e.g. 20-30
m/s), or gate the full pursue bump on `|v_target| > dl.speed` rather than
interpolating to it. Whatever is chosen, `test_bfm_fastplayer.cpp` leg 9 must
gain a leg that asserts `speed_target > |v_target|` for a target faster than
own cruise — today's leg 9 only checks arithmetic and passes happily on the
non-closing law. Fly consequence: Chad flying away from a bandit still never
gets chased down, i.e. the felt report the round was opened to fix.

### P1-2 `app/instructor_tick.h:415` (+ callers at 1269-1277) — "this dome has air" is decided on radius only; the relocation can plant a drone in vacuum

`place_in_faction_air` calls `world::faction_ellipse`, which reads
`grow[f].radius_scale` and **never `ceiling_scale`**
(`world/faction_bubbles.h:235-246`), then places at `ap.R + dw.dparams.spawn_alt`
= 2000 m — with no reference to the dome's ceiling at all. Measured
(`f2_ceiling.exe`, real `game.toml`: `bubble_ceiling_m = 4000`,
`ceil_soft = 600`, `spawn_alt = 2000`):

```
rs=1.00 cs=1.00 : ceiling=4000 m -> atm_frac 1.000  (AIR)
rs=0.75 cs=0.75 : ceiling=3000 m -> atm_frac 1.000  (AIR)
rs=0.50 cs=0.50 : ceiling=2000 m -> atm_frac 1.000  (AIR)
rs=0.25 cs=0.25 : ceiling=1000 m -> atm_frac 0.000  <-- helper says AIR, it is VACUUM
rs=1.00 cs=0.00 : ceiling=   0 m -> atm_frac 0.000  <-- helper says AIR, it is VACUUM
```

`radius_scale = ceiling_scale = 0.25` is REACHABLE under the shipped dials
(`growth_*_frac = 0.25`, `shrink_*_frac = 0.5`, 2 pumps per faction): lose a
pump (0.5) -> destroy an enemy pump (0.75) -> lose the second pump (0.25). At
that state every crash-respawn of that faction is relocated to `atm_frac = 0`
— the vacuum respawn pen FIX-F2 exists to end, now reproduced deterministically
at one fixed point per pilot. The last row shows the structural version: the
two scales are independent fields, so any future asymmetric tuning divorces
"dome alive" from "dome breathable" entirely. Fix: have the helper consult the
ceiling (`atm_cfg.bubble_ceiling_m * grow[f].ceiling_scale`) — either return
false when the ceiling cannot hold `spawn_alt`, or clamp the placement altitude
to a fraction of the live ceiling. Test gap to close in the same pass:
`test_conquest_respawn.cpp` only ever runs `FactionGrowth{}` (1.0/1.0) — add a
shrunken-ceiling leg asserting `atm_frac_at(pos) >= 0.9`, which is the assertion
the existing leg 1 already makes and which would fail today at cs = 0.25.

### P1-3 `test/unit/test_combat.cpp:855-869` — a behavioral leg's window was widened 4.4x with no recorded re-measurement

The chase window went from a welded `120*20` (20 s) to
`2*t_rev + 10 s`. With shipped values (`speed 85 + pursue_speed_bump 90 = 175`,
`pursue_max_bank = 55 deg`) `t_rev = pi*175/(9.81*tan 55) = 39.2 s`, so the
window is **88.4 s = 10 613 ticks** of full `app::tick` with 10 drones (4.4x the
old runtime, and 4.4x the slack on `best_nose_on > 0.3`). Two problems:

1. The comment's own arithmetic is wrong — it claims "~26 s at pursuit speed";
   its formula with its own cited inputs gives 39.2 s. A reader checking the
   window against the comment gets a different number than the code computes.
2. `BUILD_NOTES_FIXES.md` records the 0.228 failure under `attack_range_m =
   750` + the unconditional `max()` bump. Both of those were subsequently
   reverted/replaced. Nothing in the notes records `best_nose_on` re-measured
   under the FINAL dial set, so there is no evidence the widening is needed at
   all — and if it IS needed, then the shipped set makes the fleet's nose swing
   SLOWER than pre-fix, which is a finding about the fly, not a test-fixture
   detail. Required before commit: print the tick at which `best_nose_on`
   first crosses 0.3 under the final dials. If it crosses inside 20 s, restore
   the tight window (keeping it config-relative is fine — `t_rev/2 + margin`).

---

## P2

- **P2-1 `test_maverick_offset.cpp:239-256` — the 120 m SECTION is trait-cherry-picked and the case NAME overclaims.** My matrix (`offset_matrix.txt`, all 3 traits x {0,40,120,200} m x both dial sets x both run directions) confirms the TU's own disclosure: at 120 m Errington->Murray, idx 0 and idx 2 CRASH under the shipped dials; only idx 1 survives. The TEST_CASE is named "off-axis Errington entries survive under the FIX-F4 dials". Honest in the comment, misleading in the name a `ctest` line prints. Rename to something like "...survive at 0/40 m; 120 m survives for the SHAFT trait".
- **P2-2 (GOOD NEWS, no action) the anti-vacuity mutation arm is REAL.** The TU compares idx 1 (new dials) against idx 0 (old dials), which looked like a confound. It is not: at 40 m / dir +1 all three traits crash under 220/2.0 and all three survive under 500/1.0. The mutation lever is the dials, not the trait.
- **P2-3 nothing pins the shipped `bfm_extend_energy_m = 3000`.** `test_bfm_fastplayer.cpp:82` sets `bp.extend_energy_m = 3000.0` on a `BfmParams` whose struct default is still 600, and no loader test checks the value. A silent revert of `scenario.toml` leaves the suite green. (By contrast F4 IS pinned — `test_maverick_offset.cpp:227-229` REQUIREs 500.0/1.0 off the loaded scenario, and `test_load_scenario.cpp:201` now pins `centerline_gain = 1.0`.) Add one `CHECK` in the load_scenario `[combat]` leg.
- **P2-4 golden-angle slot collision between the two callers.** Crash-respawn passes `slot = d.spawn_index`; `scramble_to_surviving_air` passes its own `0,1,2,…` counter. `combat::maverick_faction` maps 0-4 to SUDBURY, so for a SUDBURY drone the two series overlap exactly: a scramble and a crash-respawn on the same tick can place two different aircraft at the bit-identical point (same ellipse, same `0.6*b`, same bearing). Harmless today (no drone-drone collision) but it is a coincidence, not a contract. Offset one series (e.g. `slot = spawn_index + kNumMavericks` for the respawn path).
- **P2-5 a repeat-crasher now loops on a fixed point.** Placement is a pure function of `spawn_index`, so a drone whose crash cause survives the relocation re-crashes and returns to the SAME coordinates forever. The fleet probe shows the analogous signature already (`BOREAL DIE-ERR` at byte-identical coords and `t_in = 1.45 s`, four times in 60 min). Consequence for the tape analyzer: `dc` sites dedup to one row and under-report a hot loop. Consider mixing `age`/`crash count` into the slot, or at least be aware when reading the next tape.
- **P2-6 no tape event marks the teleport.** Crash-site attribution itself is SAFE — `app/conquest_tape.h:357-365` emits `dc` from the tape's OWN previous-tick snapshot (`prev.pos`), which is the pre-crash position and is written before any relocation, so the analyzer stays honest (verified, no finding). But any metric derived from consecutive drone positions sees a multi-km one-tick jump with no marker. If the next tape is analyzed for speeds/tracks, add a `reloc` field or reuse `dc`.
- **P2-7 fly-observable: bandits now pop into existence inside the dome.** Relocation is instantaneous, at `0.6*b` from the dome centre (kilometres, but well inside the bubble the player is fighting in) with `prev = curr` so there is no interpolation streak to soften it. Chad may read "planes appearing out of nowhere" as a new bug. Worth one line on the fly card.
- **P2-8 `test_conquest_respawn.cpp:239-240` is a tautological assertion.** `CHECK_FALSE(inside_faction_ellipse(pos, SUDBURY, grow))` cannot fail: with `radius_scale[SUDBURY] = 0` the helper returns false on the `a > 0 && b > 0` guard before looking at `pos`.
- **P2-9 `place_in_faction_air` recomputes the ellipse/tangent basis per drone** (documented in BUILD_NOTES as a deliberate trade). Confirmed bit-identical for the scramble path (the pinned test is real) and negligible cost. No action.

---

## Cross-cutting checks — all clean

- **Kernel firewall:** `git diff --stat` = `CMakeLists.txt`, `app/instructor_tick.h`, `app/main.cpp`, `combat/conquest.h`, `config/{game.toml,load_game.cpp,load_game.h,scenario.toml}`, `drone/bfm.h`, `test/unit/{test_combat,test_load_scenario}.cpp` + 3 new TUs + 2 new docs. **No `sim/`, no `control/`, no goldens, no `test/harness/`.** `drone/bfm.h` added no include (`<algorithm>`/`<cmath>`/glm already present).
- **AT-9 / determinism:** the relocation is inside the fixed-dt drone loop, keyed to `spawn_index` and `grow[]`; no clock, no rng, no iteration-order dependence (foes already read the pre-loop `foe_snap`). Two same-tick crashes in the same faction get different bearings (distinct `spawn_index`). Ordering vs. the combat sweeps is safe: `enemy_fire_tick`/`combat_tick` run later (instructor_tick.h:1332+) but the relocated drone carries `prev == curr` and `respawn_in_place` already cleared `wants_fire`/`hp`/`bfm`/`mav`, so no round is resolved against a stale segment.
- **F3 hysteresis direction:** `d.raid_range_ok = ok ? (r > engage) : (r > disengage)` — armed while beyond 1500 once armed, re-arms only past 2500, releases inside 1500. Matches both the code comment and the `game.toml`/`conquest.h` prose. Loader check `disengage > engage > 0` is correct and strict; the TU exercises `<`, `==`, `0`, negative, and a missing key.
- **F3 other consumers of `engage_range`/`disengage_range`:** `drone/drone.h:844-845` (`select_engaged_target`), `combat/raid.h:128/145/169/181` (foe assignment), `instructor_tick.h:1081` (furball override). All are foe assignment — intentionally NOT split, per the brief. Nothing else read the pair.
- **F4 other consumers of `lookahead_m`/`centerline_gain`:** `lookahead_m` only at `drone/maverick.h:510` (`bore_track`, RUN); `centerline_gain` at `maverick.h:534` (RUN) and `maverick.h:819` (DIVE_IN, inside `net.contains()` only). TRANSIT/patrol use `transit_lookahead_m`, untouched. **No open-terrain behavior changes.** Note `maverick.h:196/206` still carry the OLD struct defaults (220.0 / 2.0) — every test that builds a bare `MaverickParams` still gets pre-fix dials; that is pre-existing repo style, flagged only so nobody reads a struct default as the shipped value.
- **Config diffs contain nothing beyond the spec'd dials:** `game.toml` adds only the two `raid_pause_*` keys; `scenario.toml` changes only `lookahead_m`, `centerline_gain`, `bfm_extend_energy_m` (+ the P0-1 comment and one whitespace column on the `bfm_attack_range_m` line, which still matches `test_load_scenario.cpp:351`'s literal search).
