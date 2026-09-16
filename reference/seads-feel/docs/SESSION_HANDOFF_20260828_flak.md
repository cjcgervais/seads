# HANDOFF — START HERE. THE FLAK GUN: ★★★F-POSE FLOWN THREE TIMES, ALL FIXES IN, AWAITING HIS NEXT FLY.

> Launch line for the next session:
> **"Read `docs/SESSION_HANDOFF_20260828_flak.md` in `D:\seads_sandboxes\flak-gun`; do §22."**
>
> ★**§22 IS THE CURRENT HANDOFF** — state, file map, fly checklist, dial sheet,
> ranked open list, traps. Everything above it is history, kept for the WHY
> behind a number: §§0–7 build; §8 free-look/boom; §9 the immersion ladder;
> §§10–14 the five verdict rounds; §15 round 5 signed; §16 F-POSE; **§§17–21
> the three flies that followed** — the tracer origin, the backward tracer +
> the smushed face, ★P1-10 the pads-are-handles ruling, the brace + the scarf,
> the hands + the neon ring.
>
> ✅**LANDED TO MAIN 2026-09-02** — see §23 for the merge, the CRLF trap, the gate, and the cross-lane incident this lane caused and fixed. ★Read `docs/CONTRIBUTION_SOP.md` before landing anything.

`docs/FLAK_GUN_SPEC.md` is the CONTRACT (dimensions, sources, rulings, the staged
rungs); this handoff is the SESSION STATE. Where they disagree, this file is newer.

---

## 0. THE STATE

| thing | where | state |
|---|---|---|
| worktree | **`D:\seads_sandboxes\flak-gun`**, branch `sandbox/flak-gun` off `sandbox/r4a-phase0` `2228182b3` | ⚠ NOT in `D:\flight_sim2\seads-recon` — another session was LIVE there (§6 trap 1) |
| committed | `ee6a55ad2` (model + contract + spec) → `d3b693a46` (F-PLACE + F-LOAD/DRAW) | NOT pushed |
| UNCOMMITTED | F-FIRE + F-SIGHT + chart marker + air signal + recoil/boom + Chad's fixes | tree green, flak 283/283, **full gate IN FLIGHT** (§4) |
| the .blend | `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\flak_gun.blend` | NEW file, saved; `indy650.blend` untouched |
| the GLB | `assets/flak/oerlikon_mk4.glb` (3 048 verts, 24 nodes, sha `39454654…54dd`) | committed; test-pinned |
| Chad's exe | **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`** | current with everything below |
| evidence | `assets/flak/flak_src/renders/` | hero/side/eye views, in-game rest + trained, sight view, chart markers, air signal beam (all *.png untracked — gitignored, deliberate) |

**CHAD HAS DRIVEN IT.** His verdicts so far: elevation axis GOOD; traverse was
REVERSED → ruled and flipped (mouse-right = muzzle-right is MINUS `md.x`,
`app/main.cpp`, comment marks the ruling); he asked for and got the map marker,
the air signal, recoil, and the boom. **No verdict yet on:** slew rates, the
sight picture, the pipper, drum/reload rhythm, the flank placement, the beam look.

## 1. WHAT THE GUN IS (one paragraph)

A US Navy **20 mm/70 Oerlikon on Mount Mk 4** — every dimension sourced (OP 909 /
OP 911 / NavWeaps) or derived from the Sudburian's ruled table and flagged in the
spec §4. One per SURFACE pump, **80 m toward the enemy pump** (Chad's flank
ruling; `render::flak::kFlankOffsetM`). Node hierarchy `flak_root › flak_train
(traverse, +Y) › flak_cradle (elevation, the ONE sign lives in
`render::flak::cradle_quat`)` with 13 station empties (grips, pads, sight axis,
muzzle, drum, feet, approach) for the R5+ walk-up. Facts (835 m/s, 7.5 Hz, 60-rd
drum, −5/+87°) live in `render/flak_gun.h`, single-sourced with the GLB extras
and pinned by `test/unit/test_flak_gun.cpp` against the SHIPPED file.

## 2. WHAT LANDED THIS SESSION (uncommitted half)

- **`app/flak_tick.h`** — F-FIRE, PURE and headless-tested: slew toward the raw
  mouse demand (the gun's rate cap is the mass, nothing smooths the hand), the
  synthetic zero-velocity shooter whose body −Z **is** the bore (fire_tick is
  read-only on its shooter — the consult's recipe), drum → 4 s reload → refill,
  1.6 s self-destruct (the flak-curtain read). Threaded `main → step_frame →
  tick` behind a defaulted-nullptr `FlakWorld*`.
- **KEY_O man/unman** — the KEY_J-class labelled scaffold (30 m reach, stopped
  sled or parked aircraft; the walk-up is R5+ and nothing pretends otherwise).
- **F-SIGHT** — camera ON the sight axis, eye pulled **2.9 m back** because the
  render near plane is 2 m (S3 lesson — an eye at `st_eye` clips the whole sight);
  no roll; sticky lead target (acquire ≤1.8 km, drop >1.15×) solved by the SAME
  `render::lead_solution` as the aircraft pipper; white double-circle pipper;
  `FLAK nn RDS / RELOADING` HUD.
- **The M-key chart marker** (Chad's ask) — green AA glyph (mount wedge + the
  chart's only DIAGONAL barrel stroke + muzzle bead), thin owner ring, displaced
  just clear of the pump rings **along the true bearing** (80 m ≈ 2 px — the
  displacement is chart licence, the bearing is honest).
- **The air signal** (Chad: "couldn't find either of the guns from the air") —
  a steady **searchlight UP-cone**, 120 m, owner-coloured; the third distinct
  machine signal (pump = column+lamp, sled = down-pool, flak = up-beam);
  SUPPRESSED for the manned gun (it would glare through the high-elevation
  sight). Plus a breech lamp for the walk-up.
- **Recoil + boom** (Chad's ask) — 6 cm aft kick of the sliding assembly
  (gun+drum+grips; sight/rest stay on the cradle), spring tau 55 ms; two
  `gun_synth.trigger_cannon()` voices per real spawned round at the true
  cadence; **and the plane's gun-roar is now gated off while manned** (the held
  LMB was double-driving it — found, fixed).
- Config: `[guns] flak_damage / flak_reload_s / flak_selfdestruct_s` (dials);
  facts deliberately NOT in config.
- Smoke rigs (all env-gated, smoke-only): `SEADS_FLAKCAM="dist,az,el[,gun]"`,
  `SEADS_FLAK_POSE="train,elev"`, `SEADS_FLAK_MANNED=1`, plus `SEADS_MAP=1`.

## 3. THE FEEL DIALS (what Chad's next words will move)

| dial | where | today |
|---|---|---|
| traverse / elevation slew | `render::flak::SlewRates` | 90 / 60 °/s |
| mouse rad-per-px | `kFlakAimRadPerPx`, app/main.cpp | 0.0025 |
| drum / reload / self-destruct | flak_gun.h fact / `[guns]` | 60 rd / 4 s / 1.6 s |
| recoil kick / return | app/main.cpp + flak_model | 6 cm / 55 ms |
| flank offset | `render::flak::kFlankOffsetM` | 80 m |
| signal beam | draw.cpp kSignal* | 120 m, 0.35→5.5 r |
| boom weight | trigger count in main | 2 voices/round |

## 4. THE GATE — honest ledger

- `test_flak_gun.cpp`: **283/283** (GLB structure + stations vs sources,
  kinematics at a generic point, F-FIRE cadence/drum/reload/self-destruct/
  shooter frame — the spawn-dir bound deliberately admits the ~7.1 mrad
  harmonisation rise, comment explains).
- Targeted suites (`*instructor*,*tick*,*gun*`): 86 945 assertions green.
- **Full ctest:** the pre-F-FIRE tree ran **1602/1607** — the 5 reds are the
  documented pre-existing debts (4 `sled_*` + `probe P-F`), untouched here. Two
  later full runs were killed by CPU contention with Chad's live drive (one
  died SILENTLY mid-test — check `tasklist` for a live ctest before trusting a
  quiet log). **gate4 was relaunched and is/was in flight** — read
  `gate4.log`'s tail; expect the same 5 reds. COMMIT the uncommitted work with
  the real number once it lands (the session may have ended first — if so,
  that commit is §7 item 0).
- The gate is BLIND to the app binary — every visual claim above was certified
  by screenshot (renders/), the R2.3 rule.

## 5. OPEN — ranked

1. **COMMIT the uncommitted half** (explicit adds + `graphify.py` in the same
   commit; gate4 number in the message).
2. **Chad's feel verdicts** (§0 list) — one dial at a time.
3. **Proximity burst** (red-team P0-1's second half): R≈4 m on aircraft —
   needs a defaulted radius-add on `combat_tick` for the flak pool only.
4. **Self-destruct + prox PUFF FX** — the flak read is currently a retire with
   no flash; wire `combat::Fx` (the kill-loop pool) at the retire point.
5. **The Sudburian ON the gun** (F-POSE): stations are measured and waiting;
   arms IK to `st_grip_l/r`, pads, feet; the high-elevation crouch question
   (red-team P1-9) is unruled.
6. **Mil-true ring overlay** in the sight view (the 3D rings are decorative
   from 2.9 m back — the angular truth needs a 2D overlay if Chad wants to
   shoot mils instead of the pipper).
7. **instructor_tick differential leg** for the FlakWorld arm (the "additive
   gated on defaulted nullptr is only HALF-pinned" lesson — every flak write
   lands on fk-owned state by construction, but the leg should exist).
8. **Guns are indestructible + unmanned-AI-proof** — no HP, raiders ignore
   them; both are design questions for Chad, not bugs.
9. The gun can shoot its OWN pump if aimed at it (rounds are live ballistics;
   conquest_tick never sees the flak pool so no DAMAGE lands — verify that
   stays true if anyone ever threads it).

## 6. TRAPS PAID FOR HERE — do not re-pay

1. **`seads-recon` had a SECOND LIVE SESSION** editing files while this one
   worked; a truncated `git status | head` hid it and a `checkout -b` moved
   their HEAD for ~10 min (restored byte-for-byte). Check mtimes of modified
   files before ANY branch op there; this work lives in a worktree for exactly
   that reason.
2. **Never build while a detached ctest holds `seads_tests.exe`** — the link
   dies `Permission denied`; and a detached gate can die SILENTLY (a quiet log
   is not a running gate — `tasklist`).
3. **The 2 m near plane eats a true eye-point sight camera** — pull back along
   the axis, never off it (parallax).
4. **The drum blocked the sight line in v1** — the Mk 4 bracket is outboard
   LEFT; caught by the eye-station screenshot, not by looking at the model.
5. **The foot stations shipped 0.943 m off the ground** — caught by the
   independent GLB parse, not by any viewport; the test now pins world y == 0.
6. **The spawn dir is NOT the bore** — harmonisation (toe-in + gravity rise,
   ~7 mrad) is the mechanism working; bound it, don't "fix" it.
7. **The held LMB drives BOTH triggers** unless gated — the plane's audio
   (and, symmetrically, check any future input the gun borrows).
8. **`DrawBillboard` renders nothing; DrawTriangle culls CW** — real geometry
   + screenshot verification for every chart/world glyph.
9. **Blender**: GUI only, over MCP (`tools/blender/bmcp.py`), the generator is
   idempotent — rerun `flak_gun_geom.py` then `export_flak.py`; the GLB test
   goes red on any station drift.

## 7. NEXT SESSION

0. If §5 item 1 is still open: commit (explicit paths — `app/flak_tick.h` is
   UNTRACKED, don't lose it), graphify same commit, gate number in message.
1. Then take Chad's verdicts at the gun and turn ONE dial at a time.
2. Then §5 items 3–5 in order (prox burst → puff FX → F-POSE), each with its
   own red-team round per the house process.

## 8. 2026-08-28 FOLLOW-UP SESSION (Chad's two asks, both landed)

- §7 item 0 PAID: the uncommitted half went in as `722957f83` (gate4 was
  still in flight at commit time; its number belongs to that tree).
- **"I cant hear the gun"** — FOUND + FIXED: `GunSynth::render` multiplied
  the whole mix by `firing_gain_`, and while manned we deliberately hold
  `set_firing(false)` (the plane-roar gate) — so the flak's
  `trigger_cannon()` voices were synthesized straight into a closed master
  gate. Fix: `ShotVoice.free` — external one-shots bypass `firing_gain_`
  (master + soft-clip still apply); the internal scheduler passes
  free=false, so the aircraft path is bit-identical. Pinned by a new
  Catch2 test in `test_gun_audio.cpp` ("external cannon one-shot audible
  while NOT firing") + verified standalone (peak 14300/32767 through the
  closed gate, dies to pin silence in <1 s).
- **"there should be free look for flak"** — SPACE + mouse turns the
  gunner's HEAD (the sled DRIVE-2 precedent: deltas lent to the camera,
  gun demand holds); release eases back onto the sight at 6/s. The head
  yaws about the MOUNT's local vertical, NOT the cradle's carried +Y —
  yawing about the tilted cradle up rolls the horizon (caught by the
  az=−120 certification screenshot; at 87° elevation it would be
  near-total). Camera up cross-fades sight-up→vertical over the first
  ~17° of head travel so the at-rest sight picture is untouched.
  Rotations are Rodrigues (a linear f/right blend degenerates at az=90°).
  Smoke rig: `SEADS_FLAK_LOOK="az_deg,el_deg"` (smoke-only) — certified
  by `renders/freelook_*.png`.
- ⚠ smoke rig gotcha: `SEADS_FLAK_MANNED` needs `tick_count > 30` — an
  8-frame smoke run silently never mans the gun (three identical
  screenshots before it was caught by md5).
- Immersion candidates offered to Chad (beyond §5 items 3–5): tracer
  glow/streaks, muzzle flash, spent-brass ejection, drum-swap animation +
  reload clank, gun-crew chatter/ring bell on acquire, distant-gun report
  from the OTHER pump's flak, camera shake per shot, snow kicked by the
  muzzle blast.

## 9. THE IMMERSION LADDER — Chad said YES TO ALL (2026-08-28), ALL BUILT

Chad: "yes to literally all of what you recommended … I want all parts."
Built as four orchestrated stages (Opus builders, Fable red-team/verify/
commit — the house pattern), each committed with its own gate:

| stage | commit | what |
|---|---|---|
| A | `ea51f8ce5` | 4 m prox burst (`[guns] flak_prox_radius_m`, defaulted arms on `combat_tick`) + FlakPuff curtain (night-certified moonlit grey — soot black VANISHED on the night sky) |
| B | `74c66f00c` | muzzle flash (true bell), per-shot sight shake, flak tracers split from aircraft look (6 `flak_tracer_*` keys, STRICT schema), pad-anchored snow blast |
| C | `7849bbdff` | drum-swap animation (closed-form off `reload_left_s`, GLB says drum is RIGHT not the handoff's "left"), clank/latch foley (CombatSfxSynth — no firing_gain_ gate), 150-case deterministic brass pile (on the DRAWN snow +0.55 m) |
| D | `0a7b7cdd2` | AI gunners man unmanned guns vs OPPOSING-faction raiders ONLY (player-isolation BY CONSTRUCTION — signatures admit only const drones; E-ladder canon intact), kill_sink → on_ai_kill (zero score), distant report (gain 120/d, silent >3 km), `SEADS_FLAK_AI_TARGET` smoke hook |

Also this session: free look (SPACE head-turn about MOUNT vertical) +
the boom-through-closed-gate fix (`0217a4f69`), and TWO regressions
found+fixed on the way: (1) the audio-gated `spawned_accum` drain jammed
all envelopes at ceiling in audio-less runs (now an unconditional
frame-end zero; discrete spawners difference `spawned_total`); (2) ★the
manned flak's `combat_tick` cleared `killed_spawn_indices` AFTER the
cannon's sweep — aircraft-cannon kills paid NO conquest score from
F-FIRE `722957f83` until the `clear_kills=false` arm (test-pinned).

OPEN after the ladder (ranked): Chad's feel verdicts on ALL the new
dials (one at a time); promote FlakAiParams to `[guns]` if he wants
AI-gun dials; the air-signal beacon ball occludes the gun close-up
(pre-existing); F-POSE (the Sudburian ON the gun) is now the biggest
unbuilt rung; spec §7 has no Stage-D section (stages live here).

## 10. VERDICT ROUND 1 (2026-08-29, Chad: "its great" + three asks) — ALL PAID

**STATE: everything below is COMMITTED + PUSHED thru `e7e38bbb0`
(`sandbox/flak-gun` == origin). `build-play\seads.exe` is current with all
of it. Full gate last ran 1627/1632 at `93e11ca21` (= the 5 documented
pre-existing reds: 4 `sled_*` + probe P-F); after `e7e38bbb0` the targeted
suites `[flak]+[flak_ai]+[render]+[app]` ran 230,859 assertions / 62 cases
green (a full gate has NOT rerun since — cheap insurance for the next
session to fire one off first).**

Chad's three asks off his second drive, each built + certified:

1. **RMB zoom goes down the AIM** — the FOV-zoom magnified the BORE
   (screen centre) while the gunner's eye lives on the PIPPER. While
   zoomed, the look direction eases onto the solved lead by `zoom_t`
   (fully zoomed = pipper dead centre; release = the parallax-true axis;
   free look wins via the head factor). The lead point is LAST frame's
   solve by construction (the camera runs before the solve) — do not
   "fix" the one-frame lag.
2. **Hilltop resite + tree clearings** — guns leave the 80 m flank for a
   searched NORTHWARD site (90–300 m band): flat pad, firing line ≥25°
   clear of the own pump, open horizon in 8 azimuths, and the THREAT fan
   (±30° toward the enemy pump) HARD-gated at 8°. ★★FOUR WRONG CUTS
   PAID, in order: (a) raw height-max parked the gun against a rock
   face; (b) an open-scored north bonus lost to a 276 m mountain SOUTH
   (north is a RULING, not a tie-break — hard-constrain the fan);
   (c) a MEAN horizon penalty let ONE walled azimuth slide — and it slid
   onto the threat bearing; (d) every site stood inside the boreal
   scatter — **the trees were the wall, not the DEM**. Fix for (d) =
   `render::add_tree_cuts` (NEW, draw.h/draw.cpp): TREE-ONLY 60 m pad
   clearings appended after the site search (legal: the scatter builds
   lazily on frame 1 — the set_tree_snowhill late-bind precedent).
   ⚠NEVER put gun pads in `planet_cuts` — that list also punches TERRAIN
   holes (portal surgery). Certified `renders/site_hilltop_sight.png`
   (open sky, pump out of frame). No ground => the exact old flank site.
3. **Hit indication** — four-tick X at the pipper (screen centre when
   the solution died WITH its target — a kill drops the sticky target
   the same frame): white 0.25 s = damaging hit, red larger 0.6 s =
   kill. Envelopes in main off the PLAYER-ONLY counters. ★Found on the
   way: `cw.hits` was booked UNGATED in `combat_tick`, so AI-gun sweeps
   inflated the player's hit count AND played full-volume hit cracks for
   fights 3 km away since Stage D — hits are now sink-gated like kills
   (`combat/kill.h`, both arms test-pinned in `test_flak_ai.cpp`).

### §10.1 OPEN — ranked for the next session

1. **Fire a full ctest gate first** (build/ is Debug; expect the same 5
   reds) — `e7e38bbb0` shipped on targeted suites only.
2. **Chad's verdicts on round 1** (zoom feel, the chosen hilltops, the
   X) — if a hill is wrong, `kSiteMinM/kSiteMaxM` + the 0.3 north-fan
   dot + the 60 m clearing radius are the dials (main.cpp site search).
3. **Promote `FlakAiParams` to `[guns]`** if Chad wants AI-gun dials
   (engage range, burst rhythm, the 3 km report reach) — v0 code
   constants today, the RaidParams precedent.
4. **F-POSE** (the Sudburian ON the gun) — the biggest unbuilt rung:
   stations measured and waiting (grips/pads/feet/eye), the
   high-elevation crouch question (red-team P1-9) is UNRULED.
5. The air-signal beacon ball occludes the gun close-up (pre-existing,
   noted by the Stage D builder).
6. Spec §7 still has no Stage-D/immersion section — the ladder lives in
   §9/§10 here; fold into FLAK_GUN_SPEC.md when it stops moving.
7. Standing design questions: guns indestructible + raiders ignore them
   (ruled, unchanged); mil-true 2D ring overlay (§5 item 6, still open);
   `instructor_tick` differential leg for the FlakWorld arm (§5 item 7).

### §10.2 TRAPS PAID THIS ROUND — do not re-pay

1. ⚠⚠**A `str.find`-spliced edit matched the WRONG `fprintf` and gutted
   a span of main.cpp** (the file has MANY fprintfs) — caught only by
   the build; restored from HEAD + re-applied. Match on a UNIQUE anchor
   or use the Edit tool for surgical removals.
2. A `git checkout config/world.toml` mid-verify WIPED a builder's
   uncommitted config key, after which the app **died AT LOAD with one
   stderr line** and every smoke screenshot silently came out stale/
   black. Read stderr before calling a black screenshot an FX bug; diff
   config/ before any checkout.
3. The certification loop for a placement change is SLOW (build + 250-
   frame smoke per iteration) — put a one-line DBG site print in EARLY,
   iterate on numbers, screenshot only the finalist (and remove the
   print with a unique anchor, see trap 1).
4. Screenshot instruments beat green suites again: the forest occluder,
   the pump signal column on the firing line, and the invisible night
   smoke were ALL invisible to every unit test.

### §10.3 The current dial sheet (one place)

| dial | where | today |
|---|---|---|
| site band / north fan / clearing | main.cpp site search | 90–300 m / dot≥0.3 / 60 m |
| prox fuze radius | `[guns] flak_prox_radius_m` | 4.0 m |
| flash/shake/blast | `render::flak` kFlash*/kShake*/kBlast* | see flak_gun.h |
| flak tracers | `[guns] flak_tracer_*` (6 keys, STRICT) | red-orange, 2.1x len |
| drum/reload/self-destruct | flak_gun.h fact / `[guns]` | 60 rd / 4 s / 1.6 s |
| AI gunner | `app::FlakAiParams` (code, v0) | engage 1.5 km, 8–15 rd bursts |
| distant report | main.cpp audio | gain 120/d, mute >3 km |
| hit/kill X | main.cpp envelopes + draw.cpp | 0.25 s white / 0.6 s red |
| zoom centering | main.cpp camera (`zoom_t` blend) | full at max zoom |

## 11. VERDICT ROUND 2 (2026-08-29, Chad: zoom NOT fixed, no X at all, bigger prox blast) — ALL PAID

Run as an orchestrated workflow (2 diagnosticians → 3 sequential builders →
3 adversarial red-teams, 0 blocking findings; ~1.09 M agent tokens).

1. **Zoom-on-pipper was real and had TWO stacked causes.** (a) The Stage-B
   shake block rebuilt the view from the UNBLENDED `look` and re-wrote
   `pose.target`, discarding the zoom blend — and it runs essentially always
   in combat because `fk_env_step`'s floorless exponential keeps `flak_shake`
   > 0.0 for ~60 s after any shot (saturated 1.6 while firing). Fix: the jolt
   now rides the blended `lookz` (bit-identical unzoomed by construction).
   (b) The flak sight INHERITED the chase-cam `lens_shift_ndc`, recomputed at
   the zoom FOV — measured 1.057 NDC at full zoom, which put the camera-forward
   point at pixel y≈1110 on a 1080 screen: even a perfectly centred axis could
   never show the pipper. Fix: while flak-manned, `info.lens_shift_ndc *=
   (1 - zoom_t)` — at-rest picture pixel-diff-identical, full zoom = symmetric
   frustum, pipper dead centre at px (956,540) (renders/zoom_after_fix.png,
   FZDBG3-verified; before/after set in renders/). Plus a red-team NaN guard:
   near-overhead lead can pull `lookz` ~parallel to `pose.up`; the cosmetic
   shake jolt now skips on a degenerate basis instead of poisoning the camera.
2. **The hit-X chain has NO code defect — proven LIVE, not just statically.**
   New smoke arm SEADS_FLAK_AIM_LEAD (smoke-only, slews the manned demand onto
   the solved lead) landed 19 hits + 1 kill in a 600-frame run: white X and
   red kill X both screenshot-certified at the pipper
   (renders/hitmark_hit_226*.png, hitmark_kill_386*.png), [FHIT] counters
   climbing, all manned=1. **Chad's "no X at all" + "zoom to centre screen"
   together are the exact fingerprint of a PRE-round-1 binary** — a stale
   `build\seads.exe` (Debug tree, has the ladder, LACKS round 1: binary-grep
   add_tree_cuts = 0) sat beside the current `build-play\seads.exe`.
   ★ONE-QUESTION TELL for Chad: were his guns on HILLTOP pads in tree
   clearings (current exe) or at the old 80 m flank in the trees (stale exe)?
   The decoy is renamed `build\seads_DEBUG_DO_NOT_PLAY.exe` (a Debug build
   regenerates it — re-rename after any build/ full build).
   ⚠SECOND LIVE MECHANISM, VERIFIED IN-CODE, NEEDS A RULING: the pipper reads
   in_range out to engage_m=1800 m while the shell self-destructs at 1.6 s
   ≈ 909 m — in the 909–1800 m band the sight invites shots that can never
   hit. Options: cap pipper/zoom-lead range at true reach / raise
   flak_selfdestruct_s / accept as ranging cue. NOT built, awaiting ruling.
   Insurance built: SEADS_FLAK_HIT_DBG=1 works in a REAL drive — [FHIT]
   stderr lines on every counter move; if Chad still sees no X, one drive
   with it settles reach-band vs anything else.
3. **Bigger prox blast (Chad's ask), FX-only** — damage/fuze/ballistics
   untouched. A DAMAGING burst's FlakPuff now carries
   `Fx::variant = kFlakPuffVariantHit`: flash life ×1.8, flash radius ×2.0,
   ×1.3 brighter, smoke ball ×1.6, denser, and an energy floor (a 9 HP graze
   used to draw a runt puff — the reward burst now draws full-size). The
   curtain/self-destruct puff is bit-identical by construction (retire path
   never sets the tag). All dials in the `kFlakHit*` block, combat/fx_curves.h.
   Night-legible: growth concentrated in the additive flash; smoke keeps the
   §9-A moonlit-grey ramp. Certified renders/hitblast_night_326b.png (+ day,
   + pre-hit attribution control). If Chad wants MORE from the gunner's seat:
   kFlakHitFlashRGain / kFlakHitSmokeRGain first.

New env rigs (SEADS_FLAK_* convention): SEADS_FLAK_ZOOM="0..1" (smoke-only,
pins zoom_t — RMB is hard-false headless), SEADS_FLAK_ZOOM_DBG (env-only,
FZDBG/FZDBG2/FZDBG3 probes), SEADS_FLAK_AIM_LEAD (smoke-only), 
SEADS_FLAK_HIT_DBG (env-only, real drives).

New tests: player prox-burst damaging hit books cw.hits + puff variant tag
(test_flak_ai.cpp, mutation-verified against the kill.h hits gate); 5-leg
blast-variant curve pin (test_flak_gun.cpp). Targeted flak|gun|render|app:
122/122 after every edit including the red-team fixes.

Red-team minors NOT paid (deliberate): the white X sits on the cream reticle
for 0.25 s — legibility is a THIRD live "no X" mechanism; colour/size/tau are
undocumented dials in draw.cpp/main.cpp if Chad still can't see it.
blend_toward has no snap-to-zero floor (zoom_t and flak_shake tails are the
same floorless-decay class) — sub-pixel, cosmetic-only, noted not fixed.

### §11.1 OPEN — ranked
1. Chad flies round 2 (checklist in the session report; the ONE-QUESTION tell
   above settles the stale-exe theory first).
2. The 909–1800 m pipper-beyond-reach band — needs Chad's ruling (item 2).
3. Hit-X legibility dial if he still misses it (red-team minor above).
4. F-POSE remains the biggest unbuilt rung; then §10.1 items 3/5/6/7.

## 12. VERDICT ROUND 3 (2026-08-30, Chad's drive report) — PAID

Chad: "heard sounds of pumps getting hit from the flak gun", "enemies were
not attacking me but our tunnel pump", "not sure if I saw hit markers",
"take the number of hits to kill down — I hit him a couple of times and he
did not die".

**Diagnosis first (no code assumed):**
- There is NO pump-hit sound in the game — `damage_pump` (abstracted raid
  DPS) has no audio path; the only combat one-shots are the PLAYER's own
  hit crack (`trigger_hit` off `cw.hits`, sink-gated) and kill explosion.
  What Chad heard at the gun = HIS OWN HITS LANDING. Consistent with
  "he did not die": hits booked, kills slow.
- Raiders on the tunnel pump = the designed R3 raid/deep-strike behaviour
  (instructor_tick (4)); an enemy-AI ruling, not a flak bug.
- "Not sure if I saw hit markers" = the round-2 red-team legibility minor
  come true (white X on the cream reticle, 0.25 s).

**Built:**
1. `[guns] flak_damage 30 → 200`. At 30 the drag-slowed KE law delivered
   ~5 HP/hit at 683 m — the certified rig needed 19 hits/kill. At 200 the
   SAME rig kills in **exactly 2 hits** ([FHIT]-measured: kill every 2nd
   hit across 5 straight kills, 600-frame smoke). One-shot only
   point-blank square-on. Flak-only dial; the aircraft cannon is untouched.
2. Hit-X legibility: WHITE → warm YELLOW (255,216,64), ticks 9–17 →
   10–20 px, width 2 → 2.5, tau 0.25 → 0.35 s (kill X unchanged red).
   Certified renders/hitmark_yellow_226.png (+ _crop): yellow diagonals
   clearly distinct from the cream reticle at the pipper.

Targeted flak|gun|render|app|config|toml: 133/133. Full gate: see commit
message (fired after these edits).

### §12.1 OPEN
1. Chad flies round 3: does ~2-hits-to-kill feel right, and does he SEE the
   yellow X now? (Each hit crack he hears IS a hit — same event as the X.)
2. Still unruled from §11.1: the 909–1800 m pipper-beyond-reach band; the
   hilltop-vs-flank one-question tell was never answered (moot if round 3
   verdicts come back clean — the current exe demonstrably books hits).
3. If raiders ignoring the player needs changing, that is an enemy-AI
   ladder ruling (raid/deep-strike target priorities), not a flak dial.

## 13. VERDICT ROUND 4 (2026-08-30, Chad live) — RULING: NO AUTO-ANYTHING

Chad: "sounds are too quiet for the gun"; "Take off the automatic ai
operation of the gun against enemies. It should be for player use only";
"Zoom at first went to a dot in the sky for about 5 tries, then it finally
would zoom into the pipper"; "I still didnt see hit markers". Mid-build he
ruled: **"zoom should not anchor to bandit, only pipper, no auto aim."**

★★★THE ROUND'S LESSON: "the middle of my pipper" always meant the SIGHT
RETICLE. Round 2 fixed the real bug (the lens_shift fade — reticle dead
centre at zoom) but ALSO shipped an ease-onto-the-solved-lead view blend
that STEERED his view onto whatever bandit the sight had latched — and the
sticky selector picked the NEAREST drone by distance regardless of his aim,
so zoom yanked to "a dot in the sky" and the hit X flashed at a pipper he
was not looking at. Built:

1. **Zoom = pure magnification.** The lead blend is DELETED (state
   `flak_zoom_lead*` removed); the lens_shift fade stays — full zoom puts
   the reticle dead centre, the view axis never moves. FZDBG2 now proves
   the camera does NOT track the lead.
2. **The sight targets where the GUNNER AIMS**: sticky selector acquires
   the smallest-angle drone off the BORE inside a 25 deg cone, drops on
   death / 1.15x range / past 40 deg (hysteresis; both cones are dials in
   the selector block, main.cpp). No drone in the cone = no pipper. This
   re-anchors the lead circle AND the hit X to the bandit he is engaging.
3. **AI gunners RULED OFF** — `app::kFlakAiOperate = false` (flak_ai.h)
   gates the fk_ai pointer at step_frame; gunners are still constructed
   (the SEADS_FLAK_AI_TARGET rig reads their mount frames) but never tick,
   fire, or sweep. Code + tests kept per the ruling-attached-to-design law.
4. **Boom louder**: `kFlakBoomGain = 2.2` (main.cpp boom block) on both
   per-round voices via trigger_cannon's gain arm; soft-clip bounds the sum.

Calibration rig re-run after all of it: identical 2-hits-per-kill, first
hit tick 224 — the cone does not break acquisition. Targeted 133/133.

### §13.1 OPEN
1. Chad's round-4 fly: boom weight (kFlakBoomGain), the quiet guns
   (unmanned guns now NEVER fire — is the valley too quiet?), zoom feel,
   and FINALLY the yellow X — with the selector fixed it flashes at the
   pipper of HIS bandit (it fires with every hit crack he hears).
2. "Still didn't see hit markers" is now double-covered (yellow round 3 +
   anchor round 4); if STILL invisible, one SEADS_FLAK_HIT_DBG=1 drive
   settles whether hits are being booked at all (the 909-1800 m reach band,
   §11.1, remains unruled).
3. Distant-report audio is dead with the AI gunners off (it read their
   spawned_total) — expected, not a regression.

## 14. THE LIVE STATE (2026-08-30, handoff prep) — START HERE

### §14.0 State

| thing | where | state |
|---|---|---|
| worktree | `D:\seads_sandboxes\flak-gun`, branch `sandbox/flak-gun` | == origin, PUSHED thru `d6ea6f759` |
| commits this arc | `e7e38bbb0` (round 1) → `af92b72ba` (round 2) → `d950b39da` (round 3) → `d6ea6f759` (round 4) | all pushed |
| full gate | `gate_round4.log` (worktree root, untracked) | **1629/1634** = the 5 documented pre-existing reds (4 `sled_*` + probe P-F), unchanged all four rounds |
| Chad's exe | **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`** | current with everything; AWAITING HIS ROUND-4 FLY |
| decoy | `build\seads_DEBUG_DO_NOT_PLAY.exe` | ⚠ any Debug build of target `seads` REGENERATES `build\seads.exe` — re-rename after building build/ |
| gate logs | `gate*.log/.err` in worktree root | untracked on purpose (multiple sessions' logs), never commit |

### §14.1 What the gun IS now (post-rulings, one paragraph)

PLAYER-ONLY manned 20 mm Oerlikon, one per surface pump on searched hilltop
pads in 60 m tree clearings. O mans (30 m, stopped). Slew is the hand, sight
camera 2.9 m back on the axis, SPACE free-look, RMB = PURE magnification
(reticle dead centre at full zoom via the lens_shift fade — the view axis
NEVER moves itself; Chad's ruling "no auto aim"). The lead pipper goes to the
bandit HE POINTS AT (25° acquire / 40° drop off the bore; none in cone = no
pipper). Hits: ~2 kill (flak_damage 200, MEASURED on the rig), yellow X at
the pipper per hit + red X per kill, prox bursts draw the big kFlakPuffVariantHit
blast, boom at kFlakBoomGain 2.2. AI gunners exist in code but are RULED OFF
(`app::kFlakAiOperate=false`, flak_ai.h) — with them the distant report and
any unmanned defence. Facts (835 m/s, 7.5 Hz, 60-rd, −5/+87°) stay pinned in
render/flak_gun.h + the GLB tests.

### §14.2 OPEN — ranked

1. **Chad's round-4 fly** (checklist relayed in-session): boom weight, pure
   zoom feel, aim-anchored pipper, the yellow X finally visible, 2-hit
   kills, guns silent unless manned. One dial at a time off his words —
   §14.4 is the dial sheet.
2. **UNRULED: the 909–1800 m pipper-beyond-reach band** — the sight solves
   to engage_m=1800 m but the shell self-destructs at 1.6 s ≈ 909 m; in
   that band a valid pipper can never produce a hit. Options for Chad: cap
   the pipper (and cone acquisition) at true reach / raise
   flak_selfdestruct_s / accept as ranging cue. If he STILL reports no X,
   suspect this band first — one drive with `SEADS_FLAK_HIT_DBG=1` settles
   it ([FHIT] stderr lines on every booked hit).
3. **F-POSE — the Sudburian ON the gun** — the biggest unbuilt rung.
   Stations measured and waiting in the GLB (grips/pads/feet/eye); the
   high-elevation crouch question (red-team P1-9) is unruled. Own rung,
   own red-team round per the house process.
4. Design questions parked: guns indestructible + raiders ignore them
   (ruled, unchanged); mil-true 2D ring overlay (§5.6); instructor_tick
   differential leg for the FlakWorld arm (§5.7); the air-signal beacon
   ball occludes the gun close-up (pre-existing); spec §7 still lacks the
   Stage-D/immersion/verdict sections (fold §§9–14 into FLAK_GUN_SPEC.md
   when it stops moving).
5. If Chad ever wants the valley defended again: flip
   `app::kFlakAiOperate` — everything else (Stage D isolation, sinks,
   tests) is intact and gated.

### §14.3 THE ARC'S TRAPS — the ones the next session must not re-pay

1. ★★★**"My pipper" = the SIGHT RETICLE.** Pin a player-facing word to the
   OBJECT he means before building on it; the wrong pin cost two rounds.
2. ★★★When a fix ships TWO mechanisms and the verdict stays red, suspect
   the EXTRA one (the lens fade was right; the lead blend was the bug).
3. ★★★A floorless exponential envelope is effectively ALWAYS-ON in combat
   (flak_shake >0 for ~60 s after a shot silently owned the camera write).
4. ★★A later pose WRITER beats a correct earlier blend — audit every
   downstream rebuild of the view (the shake block).
5. ★Calibrate damage by MEASURING hits-per-kill on the instrumented rig
   (`SEADS_FLAK_AIM_LEAD` + `SEADS_FLAK_HIT_DBG`), never by the reference
   number — the v² drag law ate 83% of it.
6. ★The stale-exe fingerprint: `build\` vs `build-play\` — give Chad the
   ABSOLUTE build-play path every time; re-rename the decoy after Debug
   builds.
7. Plus everything in §6 and §10.2 (unique-anchor edits in main.cpp, no
   builds while ctest holds the exe, never checkout config/, 250+-frame
   smokes for SEADS_FLAK_MANNED, screenshots for every visual claim).

### §14.4 The dial sheet (one place, post-round-4)

| dial | where | today |
|---|---|---|
| hits-to-kill | `[guns] flak_damage` | 200 (= 2 hits at 683 m, measured) |
| boom weight | `kFlakBoomGain`, main.cpp boom block | 2.2 |
| pipper cones | selector block, main.cpp (`kAcquireConeCos`/`kDropConeCos`) | 25° / 40° |
| hit/kill X | draw.cpp ticks + main.cpp taus | yellow 10–20 px τ0.35 / red 12–26 px τ0.60 |
| prox blast | `kFlakHit*`, combat/fx_curves.h | flash ×2.0 r ×1.8 life, smoke ×1.6 |
| prox fuze | `[guns] flak_prox_radius_m` | 4.0 m |
| reload / self-destruct | `[guns]` | 4 s / 1.6 s (≈909 m reach) |
| AI gunners | `app::kFlakAiOperate`, flak_ai.h | **false (RULED OFF)** |
| site band / clearing | main.cpp site search | 90–300 m / 60 m |
| zoom | lens_shift fade off `zoom_t` | pure magnification (no dial) |

### §14.5 Smoke rigs (all SEADS_FLAK_*)

`MANNED=1` (needs 250+ frames), `POSE="train,elev"`, `FLAKCAM="dist,az,el[,gun]"`,
`LOOK="az,el"`, `AI_TARGET=<gun>` (parks an orbit target), `AIM_LEAD=1`
(smoke-only: slews the manned demand onto the solve — the hit rig),
`HIT_DBG=1` (REAL drives too: [FHIT] stderr per booked hit/kill),
`ZOOM="0..1"` (smoke-only, pins zoom_t), `ZOOM_DBG` (FZDBG2/3 probes).
Calibration run: `SEADS_FLAK_MANNED=1 SEADS_FLAK_AI_TARGET=0
SEADS_FLAK_AIM_LEAD=1 SEADS_FLAK_HIT_DBG=1 build-play\seads.exe --smoke 600`
→ first hit tick 224, kill every 2nd hit.


---

## 15. ★★★ROUND 5 SIGNED (2026-08-30) — THE SIGNED STATE + WHAT'S NEXT

### §15.0 THE VERDICT

Chad, 2026-08-30, after the round-5c fly: **"okay I killed x2 of them saw a
nice red x on my last kill so it seems all is well I would like to move on
from here now."** Two kills, red X seen live, arc CLOSED. That signs the
whole round-5 arc (three sessions, all pushed):

| commit | what it paid |
|---|---|
| `280e4c297` (5) | the burst got a VOICE: boom + 2 mountain echoes (negative-age predelay on the explosion voice); curtain crumps off new monotone `FlakWorld::burst_total`; reload foley cut ~5 dB under the gun; exp voice pool 8→24 (an 8-ring recycled in ~1.1 s and stole every parked echo) |
| `23d94ddae` (5b) | ★RULING "raise the self destruct time": `flak_selfdestruct_s` 1.6→4.8 s — drag law x(t)=ln(1+k·v0·t)/k, k=0.0008, v0=835 ⇒ ~1796 m = the sight's engage_m. **The 909–1800 m dead band is CLOSED**: pipper shown ⇒ killable. Prox fuze independent + unchanged. Distant crumps hollowed (kFlakCrumpFc, kFlakCrumpCrackMix — "earthy not poppy"); hit-boom echoes hollow too, the close boom keeps full crack |
| `258dba62b` (5c) | four dials: curtain gain 0.5→1.0, hit boom 1.5→2.2, crump LP 55→40 Hz (deeper), kFlakBoomGain 2.2→3.2 |

Round-5 diagnosis worth keeping: his "one white X total" in round 4 was the
dead band's predicted symptom — the ruling above, not a marker bug.

### §15.1 State

| thing | where | state |
|---|---|---|
| worktree | `D:\seads_sandboxes\flak-gun`, branch `sandbox/flak-gun` | == origin, PUSHED (thru `258dba62b` + this handoff) |
| full gate | 1629/1634 all three rounds | the 5 documented pre-existing reds (4 `sled_*` + probe P-F), unchanged |
| Chad's exe | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` | current with everything; FLOWN + SIGNED |
| decoy | `build\seads_DEBUG_DO_NOT_PLAY.exe` | intact; ⚠ any Debug build of target `seads` regenerates `build\seads.exe` — re-rename |
| calibration | `SEADS_FLAK_MANNED=1 SEADS_FLAK_AI_TARGET=0 SEADS_FLAK_AIM_LEAD=1 SEADS_FLAK_HIT_DBG=1 --smoke 600` | first hit tick 224, kill every 2nd hit — unchanged all three rounds |

### §15.2 The dial sheet (supersedes §14.4 where they differ)

| dial | where | today |
|---|---|---|
| hits-to-kill | `[guns] flak_damage` | 200 |
| self-destruct / reach | `[guns] flak_selfdestruct_s` | **4.8 s ≈ 1796 m = engage_m (RULED)** |
| gun report | `kFlakBoomGain`, main.cpp boom block | **3.2** |
| burst booms | `kFlakBoomHitGain` / `kFlakBoomCurtainGain`, gun_audio.h | **2.2 / 1.0** |
| crump timbre | `kFlakCrumpFc` / `kFlakCrumpCrackMix` | **40 Hz / 0.12** |
| mountain echo | `kFlakEchoDelayS` {0.9, 2.1} / `kFlakEchoGain` {0.35, 0.16} | as built |
| reload foley | `kClankMaster` / `kLatchMaster` | 0.30 / 0.45 |
| everything else | §14.4 | unchanged (cones 25°/40°, prox 4 m, AI gunners OFF) |

### §15.3 OPEN — ranked next moves

1. ~~**F-POSE — the Sudburian ON the gun.**~~ **BUILT 2026-08-30/31 — see
   §16.** P1-9 ruled (crouch curve), P1-9b ruled (feet step in), and the
   VIEW ruled (clean sight + free-look pullout). Awaiting Chad's fly.
2. **Fold the handoff into the spec.** The gun has STOPPED MOVING: fold
   §§9–15 (immersion ladder, five verdict rounds, rulings, dial sheet) into
   `docs/FLAK_GUN_SPEC.md` §7 so the contract is one document again.
3. **Watch-item, unjudged: kill pace at long range.** At ~1800 m a
   drag-slowed round delivers ~11 HP (v² KE law) ⇒ many hits/kill out
   there, by physics. Chad's two kills felt right; if a future drive says
   "far kills too slow", this is the note.
4. Parked design questions (§14.2 item 4, all still parked): mil-true 2D
   ring overlay; instructor_tick differential leg for the FlakWorld arm;
   the air-signal beacon ball occluding the gun close-up.
5. If the valley should ever defend itself again: flip
   `app::kFlakAiOperate` (flak_ai.h) — Stage D is intact and gated.

### §15.4 Traps for the next session

All of §14.3 stands. Round 5 added two:

8. ★A floorless exponential was round 4's trap; round 5's twin is the
   VOICE-POOL RING: a parked (predelayed) voice occupies its slot for
   delay+life, and a ring sized for instant one-shots silently steals
   parked voices under sustained fire. Size the ring for the LONGEST
   occupancy at full cadence.
9. ★A "no sound" report can be a MISSING EVENT, not a missing gain — the
   burst had no voice at all (only the 90 ms hit ping); no dial could have
   fixed it. Find the event's owner before touching levels.

---

## 16. ★F-POSE — THE SUDBURIAN ON THE GUN (2026-08-31, AWAITING CHAD'S FLY)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §16." The rung is BUILT and gated through
`09adf97f2` on `sandbox/flak-gun`, **NOT pushed — Chad flies first**. If he
has already flown, his verdict decides between §16.6 (dial it) and §16.5
(the ranked open list).

### §16.0 What the rung is

Spec §7 rung 6. The Sudburian is posed ON the manned gun: shoulders in the
pads, hands IK-welded to `st_grip_l/r`, feet on the deck. Built over four
builder rounds plus two fresh-context red-team rounds.

| ruling | date | what it says |
|---|---|---|
| **P1-9 crouch curve** | 08-30 | As elevation rises the legs bend and the torso rocks back; feet planted; NOT a seat, NOT a stiff-legged lean. |
| **P1-9b FEET STEP IN** | 08-30 | The pads swing ~1.04 m forward of the 0° foot stations at +87°, so a crouch over planted feet is geometrically impossible past ~60°. The feet shuffle forward along the deck toward the pedestal — the real gunner's move. |
| **★THE VIEW RULING** | 08-31 | **The sight picture is CLEAN — no body at all down the sight — and holding FREE-LOOK (SPACE) eases the camera OUT behind his shoulder, where the whole man is drawn.** That is what §7.6's "third person only" always meant: he is authored to be SEEN, not worn. |

### §16.1 The commits (`sandbox/flak-gun`, NOT pushed)

| commit | what it paid |
|---|---|
| `db839a661` | F-POSE round 1: the pose solve (`render/flak_pose.h`), the drawer (`render/flak_gunner.cpp`, 44-joint Sudburian, CPU-skinned, flat tint), sled-rider hide while manned, kill switch, 4 gate legs. Red-teamed: at +87° he SAT on the snow. |
| `449389a38` | P1-9b feet step in (0 → 0.55 m). The sit is dead (pelvis 0.147 → 0.45 m at 87°); helmet off the breech at 45°. |
| `5959842a8` | Drop-aware pelvis seed: the 87° torso tuck 74.1° → **50.8°**, which the red-team computed as the feasible minimum for this mount. |
| `09adf97f2` | **The view ruling**: all first-person arm/mitt drawing DELETED (arm cut, `GunnerView`, the fade-by-head-angle curve); the free-look **pullout** added — `flak_ext_t` eased off `freelook_held`, camera LERP applied last, `gunner_fade` carrying the man's alpha. Gate **1637/1642**. |

### §16.2 Traps paid — do not re-pay

1. ★★★**A RUNG CAN BE INVISIBLE IN PLAY AND STILL PASS EVERY GATE.**
   `flak_cam_external` was set ONLY inside the `SEADS_FLAKCAM` smoke block,
   so with a clean sight picture the Sudburian would never have been seen
   in an actual game — green, screenshotted, and unseen. **Check that a
   LIVE view path exists, not just the screenshot path.**
2. ★★★**FIRST-PERSON ARMS CANNOT WORK ON THIS MOUNT** (measured, four
   pull-backs 0.85/1.2/1.5/1.8/2.4 + a forearm-only cut): his hands sit
   ~22° BELOW the sight axis (`st_eye` y=+0.24 vs `st_grip` y=−0.02, and
   the eye is 0.2 m AHEAD of the grips), so a close camera puts them at or
   past the bottom of frame and a far one shows severed shoulder stumps or
   the back of his own head. The flat tint makes any of it read as a blob.
3. ★★**A DIAL TUNED AFTER THE LAST SCREENSHOT IS AN UNVERIFIED DIAL.** The
   round-3 builder set `kSightEyeBackM = 0.85` at 22:53 — after its 22:50
   exe build and 22:52 last shot — and stopped before re-rendering. That
   value put the hands entirely out of frame and nothing caught it.
   **Compare SOURCE mtimes against the exe and the screenshots.**
4. ★A gate leg can encode the defect: round 2's crouch-depth assert
   (`y0 − y87 > 0.25`) pinned the SIT, and would have gone red against the
   fix. Deleted and re-derived.
5. ★The sight view's downward lens shift INVERTS framing intuition:
   raising the camera's aim point moves the subject DOWN in frame.

### §16.3 The dials (all FEEL, `render/flak_gun.h`)

| dial | today | what it does |
|---|---|---|
| `kSightEyeBackM` | 0.85 | sight camera behind `st_eye` (was 2.9 — Chad: "closer in") |
| `kExtDistM` | 4.5 | pullout eye distance |
| `kExtTargetUpM` | 0.7 | pullout aim height above the pad |
| `kExtAzBiasRad` / `kExtElBiasRad` | 0.44 / 0.22 | ~25° off his shoulder, ~13° above |
| `kExtEaseHz` | 6.0 | pullout ease in/out |
| fade band | 0.20 → 0.55 | where the man fades in across the blend |
| near-plane handback | t ≥ 0.9 | LATE on purpose: at t = 0.5 the eye is only ~2 m out and the 2 m plane would slice the gun |

### §16.4 State — everything below was MEASURED this session, not assumed

| thing | where | state |
|---|---|---|
| worktree | `D:\seads_sandboxes\flak-gun`, `sandbox/flak-gun` | 4 commits ahead of origin, **NOT pushed**, tree clean |
| full gate | `1637/1642` | the 5 reds are the documented pre-existing ones **by name**: probe P-F + `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only` |
| Chad's exe | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` | REBUILT after the last source edit — current with everything |
| decoy | `build\seads_DEBUG_DO_NOT_PLAY.exe` | intact; no stray `build\seads.exe` |
| graph | `generated/graph/` | regenerated in the commit, `graph_query.py check` = layer check OK |
| mutation | camera placed in FRONT of the gun | **4 assertions RED**, reverted, green — the new legs bind |
- Calibration smoke unchanged through every round: **first hit tick 224,
  kill every 2nd hit** — the round-5 fire behaviour Chad signed is untouched.
- Kill switch `SEADS_FLAK_GUNNER=0`: no load, no draw, no rider hide, and
  **no pullout** (free-look falls back to the plain head turn).
- Smoke rigs: `SEADS_FLAK_EXT="t"` pins the pullout blend; `SEADS_FLAK_LOOK`
  orbits it; `SEADS_FLAKCAM` is the external rig as before.

### §16.6 THE FLY CHECKLIST (give him this, with the absolute path)

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**.

1. Man a gun — the sight picture must be CLEAN: gun, ring, pipper, no body.
2. **Hold SPACE** — the camera swings out behind his shoulder (~0.2 s) and
   the whole Sudburian is there on the gun.
3. Mouse while holding SPACE — it ORBITS him at a fixed radius.
4. Release — eases back onto the clean sight.
5. Slew + elevate while pulled out — he steps in and crouches; at +87° he is
   tucked under the gun.
6. Fire a burst — the aim must feel exactly as signed in round 5.
7. If anything is wrong: `SEADS_FLAK_GUNNER=0` restores the pre-rung frame
   (no man, free-look back to a plain head turn).

If the pullout FRAMING is what is off (too close/far, too high/low, wrong
shoulder), it is one number each in §16.3 — not a rebuild of the rung.

### §16.5 OPEN after the fly

1. **The flat tint.** He is unlit beside the flat gun. It reads fine in the
   pullout, but lighting him is the one change that would lift every view.
2. **No body-vs-gun keep-out.** At +87° the helmet crown grazes the
   descended pads; clearance is graded only against the bore capsule.
3. The trunnion is FIXED at 1.543 m, so the ~50° forward tuck at full
   elevation is geometric. The real Mk 4's jack-screw column
   (1.181–1.581 m) is the rung-shaped fix if Chad wants him taller.
4. Mitts grip without a finger wrap; no reload/recoil animation of the man;
   mount/dismount is instant (no walk-up — F-WALK is blocked on R5+).

## 17. ★TWO DEFECTS OFF CHAD'S F-POSE FLY (2026-08-31)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §17." Built on top of §16, **NOT pushed —
Chad flies first.**

His two words after the F-POSE fly:

> "the tracer appears to come out of the sudburians view, rather than the end
> of the barrel and that needs to be fixed. Also the sudburian should be
> looking through the pipper, currently he looks at the ground"

Both were real, both were code, and they turned out to be **the same class of
mistake made twice: a quantity carried through the WRONG PIVOT.**

### §17.1 THE TRACER — the muzzle is a CRADLE STATION, not a fixed offset

`weapon::fire_tick` spawns at `shooter.position + orientation * muzzle_body`.
`muzzle_body` was measured ONCE off the GLB at zero elevation
(`app/main.cpp`, at MAN time) and then carried by the shooter's orientation —
which rotates `st_muzzle` **about the mount ORIGIN**. The gun does not
elevate about its base: it elevates **about the trunnion, 1.543 m up the
pedestal**. The two agree at 0° and nowhere else. Measured, spawn point vs
the drawn bell:

| elevation | how far the round spawned from the barrel end |
|---|---|
| 0° | 0.000 m (why nothing looked wrong on the flat) |
| 45° | ~1.18 m aft + low |
| 70° | **1.770 m** |
| 87° | **2.124 m** |

At the angles you actually shoot aircraft at, that puts the spawn a metre or
two BEHIND and BELOW the bell — beside the breech, right where the sight
camera is. Hence "out of the sudburians view".

**The fix** (`app/flak_tick.h`): `FlakWorld` now carries the `Stations`
table, and `flak_rebuild_shooter` rebuilds `muzzle_body` **every tick** by
running `st_muzzle` through `cradle_station_world()` — the exact function
`render/draw.cpp` already draws the muzzle FLASH through — and transposing
back into body coords. ★ONE kinematic source for the metal, the flash and the
round, so they cannot drift apart again. `main.cpp` now hands over the
station table (player at MAN time, AI guns per frame) instead of a computed
offset; a `FlakWorld` with no stations (the headless fixtures that type a
`muzzle_body`) is left exactly as the caller set it.

### §17.2 THE GAZE — the head's aim is the FACE, not the skull axis

Every bone in `flak_gunner_draw`'s table is aimed at the next joint down the
chain. The head is a LEAF, and it was aimed along **neck → head** — the
SKULL axis — at the crane target `st_eye`. The crane runs forward of and
barely above the shoulders, so aligning the top of his skull with it laid the
whole head over **face-down**. He stared at the deck at every elevation.
`GunnerJoints::look_dir` — the sight axis, solved since round 1 — was
computed and **never used by anything**.

**The fix** (`render/flak_gunner.cpp`): "head" comes out of the table and is
posed explicitly by the shortest arc carrying its **rest FACE direction**
(the model frame's own +Z, orthogonalised against the rig's rest skull axis —
measured off the rig, not declared) onto **`gj.look_dir`**. Pure pitch, so no
roll is introduced. His head POSITION is untouched: the crane onto `st_eye`
is P1-9's cheek-to-the-rest and the helmet-clearance leg still grades that
joint.

Verified BY EYE, side-on at az 90 (the §16 trap: a rung can be green and
invisible):

| elevation | before | after |
|---|---|---|
| 0° | goggles level, roughly right | unchanged, head level down the bore |
| 45° | **goggles pointed at the snow**, helmet crown up at the barrel | goggles up the bore, head tipped back with the gun |
| 87° | — | fully tipped back, looking straight up the near-vertical sight |

### §17.3 The two new gate legs (both MUTATION-VERIFIED)

1. `test_flak_gun.cpp` — **"flak F-FIRE: the round leaves the BELL at every
   elevation"**: the spawn equals `cradle_station_world(st_muzzle)` to 1e-9
   at 0/20/45/70/87°, is never within 1.0 m of the sight eye, and — ★so the
   leg cannot pass by accident — asserts the old about-the-origin placement
   is a **genuinely different point** (>0.3 m) off the horizontal. Disabling
   the fix turns it RED with the table above as the failure text.
2. `test_flak_pose.cpp` — **"flak pose: the gaze is the sight axis, never the
   head crane"**: `look_dir` is the rear-peep→front-bead line, its rise angle
   IS the elevation, it points downrange — and the head CRANE is more than
   30° away from it, which is the angle the face was wrong by.

⚠**HONEST LIMIT:** leg 2 pins the pose header, not the drawer. `flak_gunner_
draw` needs a GL context, so **no ctest can see the head's orientation** —
the gaze fix is certified by the screenshots above and by Chad's eye, nothing
else.

### §17.4 Also built: the manned smoke rig takes an ANGLE

`SEADS_FLAK_MANNED` was a boolean pinned at 18° elevation, and 18° is the one
band where this defect nearly hides. It now reads its value as the **demanded
elevation in degrees** (`SEADS_FLAK_MANNED=55`; anything ≤0 or absent keeps
18). Unlike `SEADS_FLAK_POSE`, which moves only the DRAWN gun, this drives
the real demand — gun, flash and round agree — so the tracer river is
certifiable at the angles where the muzzle actually swings.

### §17.5 State — measured, not assumed

| thing | state |
|---|---|
| full gate | **1639/1644** -- the red set is the documented five, matched **BY NAME** not by count: probe P-F, `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only` -- identical to §16's; the two new legs are the +2 on the total |
| worktree | `D:\seads_sandboxes\flak-gun`, `sandbox/flak-gun` — **NOT pushed** |
| files | `app/flak_tick.h`, `app/main.cpp`, `render/flak_gunner.cpp`, 2 test files, `generated/graph/` |
| graph | regenerated (`graphify.py`), `graph_query.py check` = layer check OK |
| line endings | all five edited files are pure LF, matching the tree (no CRLF mixing) |
| Chad's exe | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` — REBUILT after the last source edit (`--target seads`; the test target cannot compile in a Release tree, and a failed build piped to /dev/null is how a stale binary reports a pass) |
| mutation | fix disabled ⇒ 5 assertions RED at 20–87°, restored ⇒ green |

### §17.6 THE FLY CHECKLIST

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**.

1. Man a gun (**O**), elevate to ~45–60°, hold LMB. **The tracers must leave
   the END OF THE BARREL** — out there, past the ring — not out of your face.
2. Watch a burst at a HIGH angle (70°+): same thing, the stream starts at the
   bell.
3. Hold **SPACE** to pull out, elevate through 0 → 45 → 87°. **His goggles
   must follow the barrel up.** At full elevation he is looking straight up.
4. Fire while pulled out — flash and tracer start at the same point on the
   metal.
5. The aim itself must still feel exactly as signed in round 5.
6. Kill switches unchanged: `SEADS_FLAK_GUNNER=0` removes the man;
   nothing here changes the sight picture.

### §17.7 OPEN — unchanged from §16.5

The flat tint, the body-vs-gun keep-out, the fixed trunnion, the mitt finger
wrap. Add: the muzzle spawn ignores the 6 cm RECOIL slide (the flash
subtracts it, the round does not) — a centimetre-scale debt, named so it is
not re-found as a mystery.

## 18. ★HIS SECOND FLY — THE BACKWARD TRACER + THE SMUSHED FACE (2026-09-01)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §18." Built on §17, **NOT pushed.**

> "in first person the tracer is going through the sudburians face, from free
> look I can see the tracer coming from the barrel end, but also an opposite
> direction trace is blowing out backwards, its not the casing being expelled
> … There seems to be an equal and opposite tracer in both directions, lets
> make sure it is only one. Also the placement of the sudburian is incorrect.
> he is smushing his face into the back of the flak gum, pressed right up
> against it."

★**HIS FIRST TWO REPORTS WERE ONE DEFECT, NOT TWO.** §17's spawn fix was real
and it held — he confirms the tracer now leaves the bell. What is left going
through his face is the SAME round's STREAK, drawn backwards past the muzzle.

### §18.1 THE BACKWARD TRACER — the streak outran the round

`draw_pool` draws each comet as a FIXED-LENGTH streak back along −velocity:
core `tracer_len_m × flak_tracer_len_mult` = 8.0 × 2.1 = **16.8 m**, glow
× `tracer_glow_len_mult` 1.7 = **28.6 m**. A round leaves at 835 m/s, so one
120 Hz tick after the muzzle it has flown **6.96 m** — and the other **21.6 m
of glow was drawn BEHIND THE BELL**: back through the breech, through the
gunner, and out the back of the mount. Equal, opposite, and anchored to the
muzzle, exactly as he describes.

★★★**WHY NOBODY EVER SAW IT ON THE AEROPLANE:** the same overhang has always
been there for the plane's guns — but the plane's muzzle is 2.87 m AHEAD of
the camera, so the overhang lands BEHIND the eye and is clipped away unseen.
The flak gun is the first weapon in this game whose camera sits at the
BREECH. *A bug can be years old and only become visible when the camera moves.*

**The fix:** `combat::tracer_tail_m(want_m, speed, age)` — pure, in
`combat/fx_curves.h` beside the other FX curves — clamps every streak to the
distance the round has actually flown. `render/draw.cpp` uses it for the core
and the glow tail. **The bound is physical, not a dial:** a tracer cannot
start before the gun fired it. `spd·age` is drag-free and therefore a slight
UNDER-estimate (drag has already bled `spd`), which errs short — the safe
side. Applies to EVERY pool on the one shared path; the aeroplane's own look
changes only in the clipped region, which is the honest thing to do.

### §18.2 THE SMUSHED FACE — the crane aimed the JOINT, and the head is 0.209 m LONG

Measured, at every elevation (the whole man rotates rigidly with the cradle,
so one elevation determines all):

| quantity | value |
|---|---|
| `st_eye` (where the EYE belongs) | (−0.140, **1.783**, −0.604) |
| his neck / shoulder-line mid | (0, **1.503**, −0.952) |
| old head joint | (−0.028, 1.560, −0.881) |
| **drawn head's forward reach** | **0.2087 m** (measured off the rig) |
| ⇒ old helmet front | z = **−0.672** |
| `flak_gun` mesh box aft face | z = **−0.762** |

His helmet front sat **90 mm inside the gun's own mesh box.** The crane put
the head JOINT on the line to `st_eye` and then the drawn head reached
another 0.209 m forward again, straight into the breech.

**The fix** (`render/flak_pose.h`): the crane now aims the point that becomes
his FACE, at a target pulled back down the sight axis by
`face_fwd_m + kEyeReliefM` (0.2087 + 0.35). The helmet front now clears the
box by ~34 mm. Nothing below the neck moved.

★★★**AND THE MEASUREMENT THAT NEARLY LIED:** `face_fwd_m` is measured by the
loader over the vertices the rig binds to the head bone — but read straight
off `base_pos` it comes out as **0.000**, because this rig does NOT author the
head geometry in the head's rest frame: the helmet's raw vertices sit ~0.10 m
*behind* the joint and the INVERSE BIND MATRIX carries them onto the skull.
The honest read is through `rest × ibm`, the same product the skin pass
computes. *A raw vertex read measures a shape that is never drawn.*

### §18.3 ⚠★★★ THE FINDING CHAD HAS TO RULE ON: THE MAN IS TOO SHORT FOR THIS MOUNT

This is not a dial and it is not a bug — it is the geometry, and it is why
his head has to sit behind the breech instead of at the sight:

- His measured chain: ankle 0.10 + thigh 0.4526 + calf 0.455 + torso 0.523 =
  **1.531 m** of shoulder height, DEAD STRAIGHT. Nobody stands dead straight.
- The gun wants his shoulders in pads at **1.643 m** and his eye at
  **1.783 m**. `kShoulderPadOff`'s −0.14 already exists only to absorb this
  (§7.6 note: "the measured chain cannot reach a 1.543 shoulder line").
- Neck 0.095 + head reach 0.209 cannot bridge 1.503 → 1.783. **His eye can
  never reach the ring sight on this mount.** He is posed as a man at the gun,
  not a man looking through it.
- The gun body is only 0.19 m wide and its box is y 1.498–1.703 — the same
  band as his head — so BEHIND is the only free direction; above and outboard
  are both out of reach of a neck.

★**THE HARDWARE'S OWN ANSWER IS THE JACK-SCREW.** The real Mk 4's column runs
**1.181–1.581 m** and the GLB is built at **1.543**. Dropping the trunnion
~0.14 m puts the pads at his actual shoulder line and the eye station within
his neck's reach — everything else falls out. It re-bakes the GLB (the
pedestal and carriage shorten with it) and moves a signed number, so it is
**a rung and it needs Chad's ruling** — it is not something to do quietly.

### §18.4 The gate legs (both MUTATION-VERIFIED)

1. `test_flak_gun.cpp` — **"tracer streak never outruns the round that made
   it"**: zero on the spawn frame, exactly `spd·dt` one tick out (the case
   that bit), full length once downrange, monotone, never past either bound,
   and four degenerate inputs that must not produce a backwards streak.
2. `test_flak_pose.cpp` — **"the drawn head clears the gun body at every
   elevation"**: the face point, carried back OUT of the elevation into the
   frame the gun's box lives in, is outside `flak_gun`'s **shipped mesh AABB**
   (read from the GLB — not a typed radius), and the head joint is behind the
   breech. Graded as a PROPERTY over face reaches 0.10 → 0.24, because the
   anthro arrives. It also pins the **CEILING**: the head is tethered
   `neck_m` from the neck, so a helmet reaching past ~0.285 m could not be
   cleared by ANY aim — that would need §18.3's jack-screw, not a dial.

Mutating each fix out turns 2 test cases and 35 assertions red.

### §18.5 State

| thing | state |
|---|---|
| full gate | **1641/1646** -- the red set is the documented five, matched **BY NAME**: probe P-F, `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`. The two new legs are the +2 on the total |
| files | `combat/fx_curves.h`, `render/draw.cpp`, `render/flak_pose.h`, `render/flak_gunner.cpp`, `app/flak_tick.h`, `app/main.cpp`, 2 test files, `generated/graph/` |
| new dials | `kEyeReliefM` 0.35 (`flak_pose.h`) — the ONLY feel number added |
| measured, not typed | `face_fwd_m` = 0.2087 m, off the rig through its bind matrices |
| Chad's exe | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` |

### §18.6 THE FLY CHECKLIST

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**.

1. Man a gun (**O**), hold LMB at ~25–60°. **In the sight there must be ONE
   tracer, going away.** Nothing through your face, nothing backwards.
2. Hold **SPACE** and fire — one stream, leaving the bell, no second stream
   out the back.
3. Look at him on the gun at 0°, 45°, 87° — **his head is behind the breech,
   not in it**, goggles following the barrel up.
4. Then read §18.3 and tell me whether to lower the trunnion. That is the
   difference between a man AT the gun and a man LOOKING THROUGH it.

## 19. ★★★ P1-10 — THE PADS ARE HANDLES (Chad's ruling, 2026-09-01)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §19." BUILT + gated, **NOT pushed.**

> "OKAY JUST HAVE TO LET THE MAN STEP BACK AND HOLD THE SHOULER PADS LIKE
> THEY ARE HANDLES."

### §19.0 ★★★ THE RULING THAT REPLACED A RUNG

§18.3 put a finding to Chad: the Sudburian is too short for the mount, and
the hardware's answer is the Mk 4 jack-screw — re-bake the GLB, lower the
trunnion, move a signed number. **He ruled the other way, and it is better.**
The man doesn't get taller and the gun doesn't get shorter: **he stands back
and holds the pads like handlebars.** The stature mismatch stops being a
defect to engineer around and becomes the posture.

★★★**THE LESSON WORTH THE WHOLE SESSION: FOUR ROUNDS OF SYMPTOMS HAD ONE
CAUSE, AND THE FIX WAS A RULING, NOT A RUNG.** Every awkward number in
rounds 1–4 descended from welding his shoulders into pads he cannot reach:
`kShoulderPadOff`'s −0.14 m fudge, the head pinned at bore height behind the
breech, the helmet 90 mm inside the receiver, the ~74° forward fold at +87,
the eye that could never see the sight. Not one of them was addressable at
its own level. *When a rung keeps producing symptoms at different places, the
thing to re-examine is the RULING it is built on.*

### §19.1 What changed in the solve (`render/flak_pose.h`)

| | before (rounds 1–4) | after (P1-10) |
|---|---|---|
| weld | shoulders → `st_pad_l/r`, hands → `st_grip_l/r` | **hands → `st_pad_l/r`**, nothing else |
| shoulders | welded to the gun | **solved**: `kArmWorkFrac` 0.88 of a full arm behind/below the handle mid |
| shoulder height | whatever the pads gave (1.503 → 0.508) | his own standing height (1.494), crouching only when the handles descend |
| feet | `st_foot_l/r` + a forward step-in cap | width off the stations, **fore-aft derived** under the shoulder line |
| stance at 0° | in the pads | **0.38 m aft of the foot stations** |
| torso pitch at +87° | ~51° folded (74° before the seed fix) | **−8.9°** — upright, leaning slightly back against the pull |
| helmet vs `flak_gun` box | 90 mm INSIDE | outside by > 50 mm at every elevation, every body point |

The arm length is **constant at 0.881 of a full arm at every elevation** by
construction — the elbows can neither lock nor overreach, and the elevation
curve is a pure crouch. `kShoulderPadOff`, `kHandGripOff` and `kStepInMaxM`
are gone; `kHandPadOff`, `kArmWorkFrac`, `kStandoffMinM`, `kStandLegFrac`,
`kFootAheadOfShoulderZ` replace them. `shoulder_half_m` is now MEASURED off
the rig's upperarm joints (the shoulders are no longer positioned by the gun,
so the rig has to say how wide he is).

### §19.2 The gate — four legs re-derived to the new ruling

★A gate leg can encode a dead ruling (§16.2 trap 4, paid again here). These
were rewritten, not relaxed:

1. **"the hands hold the pads, the arms never lock"** (was "welds hold"):
   the mitt grips the rotated pad; the arm length is one value ±0.06 at every
   elevation and always short of a straight arm; the shoulders are **> 0.35 m
   OFF the pads** (the assertion that is mutually exclusive with the old
   build); the stance width is exact and the fore-aft is under the shoulders;
   and at 0° his feet are **aft of the stations by > 0.20 m** — he stepped
   back.
2. **P1-9b crouch** — the step-in clauses are gone and the torso-pitch bound
   went from `< 60°` to **`|pitch| < 15°`**, five times tighter, measured
   −2.7° at the stand and −8.9° at +87. Plus: the knee folds below 0.62 of a
   straight leg at +87, so the LEGS pay for the crouch, not the spine.
3. **the drawn head clears the gun body** — now grades SEVEN body points
   (face, head, neck, shoulder mid, both shoulders, pelvis) against the
   shipped `flak_gun` mesh AABB with a 50 mm margin, over face reaches
   0.10 → **0.30** (the old ceiling at 0.285 is gone: standing off the gun,
   even an oversized helmet clears). Plus an explicit "at the stand his face
   is > 0.30 m BEHIND the breech" — the shape of the ruling itself.
4. **the gaze discriminator** was RE-MEASURED, not kept: standing back, the
   crane runs closer to the sight line, so the separation bound moved 30° →
   12° (measured worst case 19.3° at +10°, nearly opposed at +87).

★★★**THE MUTATION IS THE BEST ONE THIS RUNG HAS HAD:** restoring the OLD
ruling (shoulders welded into the pads, hands on the grips) turns **4 test
cases and 109 assertions RED**. The two rulings are mutually exclusive, which
is exactly what a gate leg that encodes a ruling should be.

### §19.3 State

| thing | state |
|---|---|
| full gate | **1641/1646** -- the red set is the documented five, matched **BY NAME**: probe P-F, `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only` |
| files | `render/flak_pose.h`, `render/flak_gunner.cpp`, `test/unit/test_flak_pose.cpp`, `docs/FLAK_GUN_SPEC.md` §7.6 (marked SUPERSEDED), `generated/graph/` |
| measured, not typed | `shoulder_half_m` (rig upperarm span), `face_fwd_m` 0.2087 |
| graph | regenerated, `graph_query.py check` = layer check OK |
| Chad's exe | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` — rebuilt after the last source edit |

### §19.4 THE FLY CHECKLIST

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**.

1. Man a gun (**O**), hold **SPACE**. He is **standing back** from the gun,
   arms out, mitts on the pads.
2. Elevate 0 → 45 → 87°. He follows the handles down into a crouch —
   **without folding over the gun** and without his head going near it.
3. Fire — one tracer, from the bell (§18 still holds).
4. Anything about the STANCE is one number in `flak_pose.h`:
   `kArmWorkFrac` (how far back he stands), `kStandoffMinM` (how close he
   gets at full elevation), `kFootAheadOfShoulderZ` (foot placement),
   `kStandLegFrac` (how straight his legs are at the stand).

### §19.5 OPEN

Unchanged from §16.5 minus the trunnion: the flat tint, the mitt finger wrap,
no reload/recoil animation, instant mount/dismount. ★**The jack-screw is
CLOSED — not built, ruled around.** New and small: `st_grip_l/r` are now
drawn geometry nobody holds; if that reads wrong, the spade grips could be
dropped from the model or the hands split between pads and grips.

## 20. ★P1-10b — THE BRACE, AND ONE UNBOUNDED AIM (2026-09-01)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §20." BUILT + gated, **NOT pushed.**

> "okay he needs to bend his knees and bring his shoulders down too. His
> shoulders are too high, his head appears low compared to shoulders. Also
> the scarf is sticking straight up out of the sudburains helmet."

★★★**THE SECOND AND THIRD COMPLAINTS WERE ONE DEFECT, IN A PART OF THE BODY
NEITHER OF THEM NAMES.** The head crane aims at `st_eye` pulled back down the
sight axis. Welded at the gun that target was always up and forward. **Once
P1-10 stood him BACK, it can fall BELOW his own shoulder line** — measured,
it does so from about +35°, and at +87 the aim points almost straight DOWN
from the neck. Two consequences, in two different places:

1. the head hung UNDER his shoulders (his "head appears low"), and
2. the drawer aims `neck_01` AT the head — and the rig's scarf chains
   (`scarf_01..06`, `scarf_s01..02`) are **children of `neck_01`** — so an
   inverted neck bone threw the scarf straight up over his helmet.

★*One unbounded aim, three symptoms, none of them in the aim.* The fix is a
bound, not a dial: **the head LEANS off the torso's own up, never past
`kHeadLeanMaxRad` (25°), and never replaces it.** The scarf hangs again
because `neck_01` can no longer invert.

### §20.1 The brace

`kStandLegFrac` 0.96 → **0.85**. He is not standing to attention holding a
bar, he is braced on it: the shoulder line drops 0.10 m and the knee closes
to about 120° **before** the elevation asks for anything. Measured leg
extension at the stand went 0.968 → **0.859** of a straight leg.

### §20.2 The gate

- ★NEW leg **"the head rides on top of the neck, never under it"**: the neck
  bone keeps its length exactly, its direction never leaves the 25° cap off
  the torso up, and `head.y > shoulder_mid.y + neck_m/2` at every elevation
  on a 31-point sweep. Unclamped this last one reads NEGATIVE past ~35°.
  **This is also the scarf leg** — there is no scarf in the pure solve, and
  the guarantee for it is exactly "`neck_01` never inverts".
- The stand leg was re-derived, not relaxed: it asserted "knees near full
  extension" (0.94 of straight), which is the thing Chad just ruled out. It
  now asserts the knee IS bent — `leg0 < 0.92 × straight` — a bound that
  **goes red against every earlier build** — and still `> 0.78` so he is not
  collapsed. The +87 crouch depth bound moved 0.35 → 0.25 m (measured 0.326)
  because he now STARTS lower, which is the point.
- Mutating both fixes out (lean cap removed, `kStandLegFrac` back to 0.96)
  turns **2 test cases and 60 assertions RED**.

**Full gate 1642/1647** -- the red set is the documented five, matched BY
NAME: probe P-F, `sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.

### §20.3 Dials, if the brace needs taste

`kStandLegFrac` 0.85 (how deep the braced stand is) · `kHeadLeanMaxRad` 25°
(how far he cranes toward the sight before the head stays put) ·
`kArmWorkFrac` 0.88 (how far back he stands) · `kStandoffMinM` 0.25 ·
`kFootAheadOfShoulderZ` 0.05.

### §20.4 FLY

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**, man a gun, hold
**SPACE**:
1. At 0° — knees bent, shoulders down, **head clearly above the shoulders**,
   scarf hanging down his chest.
2. Elevate through 45° to 87° — the scarf **stays down** the whole way.
3. Fire — one tracer, from the bell.

## 21. ★HIS THIRD FLY — HANDS, SCARF, AND THE NEON RING (2026-09-01)

**LAUNCH LINE:** "Read `docs/SESSION_HANDOFF_20260828_flak.md` in
`D:\seads_sandboxes\flak-gun`; do §21." BUILT + gated, **NOT pushed.**

> "need to drop the scarf so that it runs along his body its still offset a
> bit, and need to swap his hands for the flak gun only not the snowmachine
> hands ever (do not touch or move those). Just swap the hands on the gun a
> reorient as needed ... Oh maybe a neon color coded in query for the correct
> blue as neon and just thiner blue metal ring for the pipper, not the shape
> but the thickness of the pipper occludes the view of the enemy a little bit."

### §21.1 ★★★ THE HANDS — THE TWO FILES DISAGREE ABOUT WHICH SIDE IS LEFT

Measured, and it is not a bug in either file:

| | left limb sits at |
|---|---|
| the RIG (`indy650.glb`) | `upperarm_l` **x = +0.21** |
| the GUN (`oerlikon_mk4.glb`) | `st_pad_l` **x = −0.235** |

The gun names its stations off the MOUNT's convention, the rig names its bones
off the MAN's, and **the two are opposite in sign**. `hand_l → st_pad_l` was
therefore drawing his left arm, sleeve and thumbed mitt across on his right.
★**A SUFFIX IS NOT A SIDE.** The pairing is now decided by GEOMETRY: the
loader measures `left_sign` off the rig's own upperarm joints, and the solve
gives each hand the pad on ITS side. The elbow splay hint moved from the FEET
(`sxl`) to the HANDS (`sxh`) with it — hinted off the old side it would have
folded the arm across his chest ("reorient as needed").

⚠**FLAK ONLY.** Nothing here is on the sled rider's path: the drive pose welds
its hands elsewhere and was not read, written or rebuilt. Chad: "not the
snowmachine hands ever (do not touch or move those)."

⚠**THE FEET ARE STILL PAIRED BY SUFFIX** — the same mismatch, so his left boot
is on his right foot. He did not report it (a low-poly boot is near
symmetric), and he said the rest was good, so it is NAMED HERE and not
changed. One line in `gunner_solve` when he wants it.

### §21.2 The scarf hangs off his chest, not off his gaze

The rig parents BOTH scarf chains (`scarf_01..06`, `scarf_s01..02`) to
`neck_01` — and this drawer AIMS `neck_01` at the head. §20 stopped the neck
INVERTING; it could still swing up to the head-lean cap, which is the offset
he could still see. Now the chain root is carried by **`spine_03`** (his
chest, where a scarf actually hangs from) and rotated so the AUTHORED TAIL
DIRECTION points DOWN — gravity in the gun model frame. The children rigid-
follow as before, so **every curve the art gave it is preserved**; only the
hang changed.

### §21.3 ★ THE RING — QUERIED, NOT PICKED, AND HALVED

"the correct blue as neon" is **`[teams] ally = (0.12, 0.57, 1.00)`** in
`config/world.toml`: the exact 180° HSV complement of the slag orange,
enforced by `render::is_exact_complement` and `test_team_color`. There is
deliberately no second colour table (CLAUDE.md H1).

- **The HUD pipper** now reads that colour from `render::team_colors().ally`
  — not a bare RGB — and its centre pip lost 40% of its radius (1.8 → 1.1 px).
- **The metal ring** is GEOMETRY, so it needed the model rebuilt:
  `RING_TUBE_R` **0.004 → 0.0022** and `SPOKE_R` 0.003 → 0.0018 in
  `flak_gun_geom.py`. The SHAPE is untouched — three Mk 5 mil rings, four
  spokes, the bead, all exactly where OP 911 puts them. On screen the ring
  lines went ~5.9 px → ~3.2 px at the sight distance.
- The generator **READS the blue out of `config/world.toml`** rather than
  retyping it, and the gate now pins the shipped material to
  `render::team_colors().ally` to 1e-6. A retyped blue would look right and
  fail — which is the point.

**GLB rebuilt** (GUI Blender + `bmcp`, the documented idempotent path):
verts **6096 → 6096**, nodes 24 → 24, **ZERO stations moved**, one material
changed colour. The old file is in the scratchpad as `oerlikon_mk4.glb.bak`.

### §21.4 ⚠★★★ A LANDMINE FOUND AND DEFUSED IN `export_flak.py`

The export script ended with a **hard-coded absolute path**:

```
OUT = "D:/flight_sim2/seads-recon/assets/flak/oerlikon_mk4.glb"
```

`seads-recon` is **CHAD'S FLY TREE**, not this sandbox. Run from here it would
have written this sandbox's model into the tree he flies — with **no diff in
this sandbox to show for it**, and (since `seads-recon` has no `assets/flak/`)
possibly as a stray new file. ★*An absolute path in a build script is a
cross-worktree write waiting for the first person who runs it somewhere else.*
`OUT` is now FOUND by walking up from the session's cwd to the tree that
contains `config/world.toml`. Verified after the export: the GLB changed here,
`seads-recon` untouched.

### §21.5 The gate

- The weld leg is re-derived: each mitt grips ONE pad, they grip DIFFERENT
  pads, and **his left hand/elbow/shoulder are all on his left** (`* left_sign
  > 0`). Plus a leg that hands a **MIRRORED rig** (`left_sign = -1`) to the
  same stations and requires the hands to swap with it — a build that
  hard-codes the pairing passes the sweep and fails that.
- The station-independence leg no longer names WHICH hand a nudged pad moves:
  it asserts exactly one moves and the other is bit-identical.
- The mono-material leg is re-derived: every flak material is mono **except
  exactly one**, which must equal the palette's ally blue to 1e-6.
- Mutation: pairing back to suffix ⇒ **28 assertions RED**.
- ⚠**The scarf change is NOT gateable** — it lives in the drawer, which needs
  a GL context. Certified by the renders (it now runs straight down his back
  at 0° and 45°) and by Chad's eye.

**Full gate 1642/1647** -- the red set is the documented five, matched BY
NAME: probe P-F, `sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.

### §21.6 FLY

**`D:\seads_sandboxes\flak-gun\build-play\seads.exe`** — man a gun, hold SPACE:
1. His mitts: **left hand on his left pad**, no arm across his chest.
2. The scarf runs down his body, at every elevation.
3. Sight picture: the ring is **neon ally blue and thinner**; the pipper is
   the same blue with a smaller centre pip. If it wants to be thinner still,
   `RING_TUBE_R` in `assets/flak/flak_src/flak_gun_geom.py` is the one number
   (then rerun the generator + export in a GUI Blender).

---

# 22. ★★★ HANDOFF — START HERE (2026-09-01)

**LAUNCH LINE:**
> **"Read `docs/SESSION_HANDOFF_20260828_flak.md` in
> `D:\seads_sandboxes\flak-gun`; do §22."**

§§0–15 are the gun's build history and the five verdict rounds. §16 is F-POSE.
**§§17–21 are one continuous arc off Chad's F-POSE fly, and §22 is where it
stands.** Read §22 first; the earlier sections are the WHY behind any number
you are about to move.

## §22.1 What happened, in one paragraph

Chad flew F-POSE and reported, over three flies: the tracer came out of his
face; then an equal-and-opposite tracer and a smushed face; then shoulders too
high, head low, scarf over the helmet; then swapped hands and a fat sight
ring. **Every one was real and every one is fixed.** Two of them were older
than the flak gun — the tracer overhang has always been on the aeroplane's
guns, clipped behind the camera and therefore never seen. One of them replaced
a rung: the Mk 4 jack-screw was put to him as the fix for a man too short for
his mount, and ★**he ruled the other way — "let the man step back and hold the
shoulder pads like they are handles" — which is better, and which retired four
rounds' worth of symptoms at once.**

## §22.2 STATE — every row measured, not assumed

| thing | state |
|---|---|
| worktree | **`D:\seads_sandboxes\flak-gun`**, branch `sandbox/flak-gun` |
| HEAD | **`14c366645`** — "Flak F-POSE fly arc: the tracer, the smush, and P1-10 — the pads are handles" (18 files, +1777/-180). 6 commits ahead of origin |
| the work | **COMMITTED 2026-09-01, tree clean. NOT PUSHED** — he flies first, and the DO-NOT-PUSH law is not repealed. Landed as ONE commit because `flak_pose.h` and `flak_gunner.cpp` carry changes from every section (§§17–21) and cannot be split without rewriting them apart; the gate below covers the tree as a whole |
| full gate | **1642/1647**; the 5 reds are the documented pre-existing ones **BY NAME**: probe P-F, `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only` |
| Chad's exe | **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`** — REBUILT after the last source edit and after the GLB rebuild (mtimes checked) |
| the GLB | `assets/flak/oerlikon_mk4.glb` REBUILT: verts 6096 → 6096, nodes 24 → 24, **zero stations moved**, one material changed colour. Pre-rebuild copy in the scratchpad as `oerlikon_mk4.glb.bak` |
| graph | regenerated, `graph_query.py check` = layer check OK |
| spec | `docs/FLAK_GUN_SPEC.md` §7.6 marked **SUPERSEDED** by P1-10, so the dead shape cannot be rebuilt from it |
| Blender | launched for the GLB rebuild and **closed**; it held the default scene, never `indy650.blend` |

## §22.3 The file map — what changed and why

| file | change |
|---|---|
| `app/flak_tick.h` | `FlakWorld::stations`; `muzzle_body` rebuilt every tick through `cradle_station_world` (§17.1) |
| `app/main.cpp` | hands the station table over; `SEADS_FLAK_MANNED=<elev deg>` (§18.4) |
| `combat/fx_curves.h` | **NEW** `tracer_tail_m` — a streak may not outrun its round (§18.1) |
| `render/draw.cpp` | tracer tails clamped; the pipper is the palette's ally blue, pip 1.8 → 1.1 px (§18.1, §21.3) |
| `render/flak_pose.h` | **P1-10 + P1-10b**: hands on the pads by measured SIDE, shoulders solved, braced stand, head-lean cap, eye relief (§19, §20, §21.1) |
| `render/flak_gunner.cpp` | the gaze aims the FACE; `face_fwd_m` / `shoulder_half_m` / `left_sign` measured off the rig; the scarf hangs off `spine_03` (§17.2, §18.2, §21.2) |
| `assets/flak/flak_src/flak_gun_geom.py` | `RING_TUBE_R` 0.004 → 0.0022, `SPOKE_R` 0.003 → 0.0018, sight colour READ from `config/world.toml` (§21.3) |
| `assets/flak/flak_src/export_flak.py` | ⚠ hard-coded cross-worktree `OUT` removed (§21.4) |
| `docs/FLAK_GUN_SPEC.md` | §7.6 superseded |
| `test/unit/test_flak_gun.cpp` | the bell leg, the tracer-streak leg, the mono-materials leg re-derived |
| `test/unit/test_flak_pose.cpp` | the weld / stand / keep-out / gaze legs re-derived to P1-10, plus the head-on-neck leg |
| `generated/graph/` | regenerated in the same change |

## §22.4 THE FLY CHECKLIST — give him this

Open **`D:\seads_sandboxes\flak-gun\build-play\seads.exe`**. Man a gun with **O**.

1. **Fire at 25–60°.** ONE tracer, leaving the **end of the barrel**. Nothing
   through your face, nothing going backwards.
2. **Hold SPACE** (pull-out). He is **standing back**, knees bent, shoulders
   down, **head above his shoulders**, **left mitt on his left pad**, scarf
   running down his body.
3. **Elevate 0 → 45 → 87°** while pulled out. He follows the handles down into
   a crouch, never folds over the gun, the scarf stays down, and his goggles
   track the barrel up.
4. **The sight picture.** Clean — gun, ring, pipper, no body. The ring is
   **neon ally blue and thin**; the pipper is the same blue with a small pip.
   Can you see a target sitting ON the 200-mil ring?
5. **The aim must still feel exactly as signed in round 5.**
6. Kill switch unchanged: `SEADS_FLAK_GUNNER=0` removes the man entirely.

## §22.5 THE DIAL SHEET — one edit each, no rebuilds

**The man** (`render/flak_pose.h`) — all FEEL:

| dial | today | what it does |
|---|---|---|
| `kArmWorkFrac` | 0.88 | how far back he stands (fraction of a full arm) |
| `kStandLegFrac` | 0.85 | how deep the braced stand is (knee ~120°) |
| `kStandoffMinM` | 0.25 | how close he gets at full elevation |
| `kFootAheadOfShoulderZ` | 0.05 | foot placement under the shoulder line |
| `kHeadLeanMaxRad` | 25° | how far he cranes toward the sight |
| `kEyeReliefM` | 0.35 | face standoff behind `st_eye` |
| `kHandPadOff` | (0, −0.02, −0.06) | where the palm takes the pad |
| `kPelvisDeckMinM` / `kHipOverFootM` / `kPelvisAftLimitZ` | 0.26 / 0.12 / −0.30 | the never-a-sit constraint set |
| `kLegReachFrac` / `kToeRiseMaxM` | 0.985 / 0.09 | IK guards |

**MEASURED, NEVER TYPED** — the loader reads these off the rig; do not turn
them into constants: `face_fwd_m` 0.2087 · `shoulder_half_m` · `left_sign` ·
every bone length in `GunnerAnthro`.

**The camera** (`render/flak_gun.h`): `kSightEyeBackM` 0.85 · `kExtDistM` 4.5 ·
`kExtTargetUpM` 0.7 · `kExtAzBiasRad` 0.44 · `kExtElBiasRad` 0.22 ·
`kExtEaseHz` 6.0 · fade band 0.20 → 0.55.

**The sight ring** (`assets/flak/flak_src/flak_gun_geom.py`): `RING_TUBE_R`
0.0022 · `SPOKE_R` 0.0018. ⚠These need a GUI Blender: rerun `flak_gun_geom.py`
then `export_flak.py` through `tools/blender/bmcp.py exec`, then re-gate — the
GLB is test-pinned.

**Smoke rigs** (all env-gated, smoke-only): `SEADS_FLAK_MANNED=<elev deg>`
mans gun 0 and drives the REAL demand — use this, not `SEADS_FLAK_POSE`, for
anything the tick computes · `SEADS_FLAKCAM="dist,az,el[,gun]"` ·
`SEADS_FLAK_POSE="train,elev"` moves only the DRAWN gun and therefore **lies
about the sim** · `SEADS_FLAK_EXT="t"` · `SEADS_FLAK_LOOK="az,el"` ·
`SEADS_MAP=1`.

## §22.6 OPEN, ranked

1. ⚠**The FEET are still paired by suffix** — the same rig/gun sign mismatch
   §21.1 fixed for the hands, so his left boot is on his right foot. Named and
   deliberately not changed (he said the rest was good). **One line in
   `gunner_solve` when he wants it.**
2. **The flat tint.** He is unlit beside a flat gun. Lighting him is the one
   change that would lift every view.
3. **`st_grip_l/r` are drawn geometry nobody holds** now that the pads are the
   handles. Drop the spade grips from the model, or split the hands between
   pads and grips — Chad's call.
4. **No body-vs-gun keep-out beyond the head/face points.** The gate grades
   seven body points against `flak_gun`'s box; the arms and the drum are not
   graded against each other.
5. Mitts grip without a finger wrap; no reload/recoil animation of the man;
   mount/dismount is instant. ★**THE WALK-UP IS NO LONGER OURS TO BUILD** — it
   was blocked on R5+ (no on-foot state) and the GAME-LOOP lane is now building
   it on `sandbox/game-loop` (`84b18a3ca`, `b7524e2b3`): O requires
   `PlayerMode::Afoot`, upright, within 3 m of `st_approach`. Not on main yet.
   Two things verified WITH that lane, in the tree rather than from memory:
   (a) the gate sits at the KEYPRESS, and the smoke branch
   (`SEADS_FLAK_MANNED`, `smoke_frames > 0`) still calls `flak_man(0)`
   DIRECTLY — ⚠if that gate ever moves INSIDE `flak_man`, every headless flak
   render and the whole tracer/pose certification dies **and no ctest sees it,
   because nothing runs `seads.exe`**; (b) `st_approach` is TRAIN-parented, so
   it must never be carried through the cradle/elevation path — it would swing
   ~1 m with the gun up and stay green in every flat-gun test.
6. The muzzle spawn ignores the 6 cm recoil slide (the flash subtracts it, the
   round does not) — centimetre-scale, named so it is not re-found as a
   mystery.
7. ★**CLOSED, do not reopen:** the Mk 4 jack-screw. P1-10 ruled around it.

## §22.7 ★★★ TRAPS PAID — the transferable ones

1. ★★★**WHEN A RUNG KEEPS PRODUCING SYMPTOMS IN DIFFERENT PLACES, RE-EXAMINE
   THE RULING IT IS BUILT ON.** Four rounds of awkward numbers all descended
   from welding his shoulders into pads he cannot reach. The fix was a ruling,
   not a rung.
2. ★★★**A ZERO-ELEVATION MEASUREMENT AGREES WITH THE TRUTH AT EXACTLY ONE
   POSE** — and 0° is the pose every screenshot gets taken at. The muzzle
   offset was 1.77 m wrong at +70° and perfect on the flat.
3. ★★★**A BUG CAN BE YEARS OLD AND ONLY APPEAR WHEN THE CAMERA MOVES.** The
   tracer overhang has always been on the aeroplane's guns; its muzzle is
   ahead of the eye, so the overhang was clipped away unseen. The flak gun is
   the first weapon whose camera sits at the breech.
4. ★★★**A RAW VERTEX READ MEASURES A SHAPE THAT IS NEVER DRAWN.** `face_fwd_m`
   read off `base_pos` is 0.000 — this rig does not author head geometry in
   the head's rest frame. Read through `rest × ibm`, the product the skin pass
   computes.
5. ★★★**ONE UNBOUNDED AIM, THREE SYMPTOMS, NONE OF THEM IN THE AIM.** The head
   crane fell below the shoulder line once he stood back → his head hung under
   his own shoulders AND the scarf went over his helmet (the scarf chains ride
   `neck_01`, which this drawer aims at the head). Fixed with a bound, not a
   dial.
6. ★★★**A SUFFIX IS NOT A SIDE.** The rig puts `upperarm_l` at +x, the gun
   puts `st_pad_l` at −x. Pair by measured geometry, never by name.
7. ⚠★★★**AN ABSOLUTE PATH IN A BUILD SCRIPT IS A CROSS-WORKTREE WRITE WAITING
   FOR THE FIRST PERSON WHO RUNS IT SOMEWHERE ELSE.** `export_flak.py` wrote
   into `seads-recon` — Chad's fly tree — with no diff in this sandbox to show
   for it. **Read a build script's output path before running it in a worktree
   it was not written in.**
8. ★**A GATE LEG CAN ENCODE A DEAD RULING.** Six legs were REWRITTEN this
   session, never relaxed — including one that asserted "knees near full
   extension", the exact posture Chad had just ruled out, and one whose 30°
   bound had to be re-measured to 12° because the new stance moved the
   geometry it was measuring.
9. ⚠**ORPHANED `ctest` / `seads_tests` PROCESSES THRASH THE MACHINE** — 20
   tests in 10 minutes instead of ~700. The full gate is ~31 min: kill strays
   first, then run it **detached** via PowerShell `Start-Process`. A
   backgrounded shell gets killed and takes ctest with it, and the exit code
   you read back is the wrapper's, not the gate's.
10. ★**THE MUTATION IS THE PROOF.** Every fix was reverted and re-run: the old
    P1-10 ruling turns 4 cases / 109 assertions red; the hand pairing 28; the
    lean cap + brace 60; the muzzle fix 5; the tracer clamp its whole leg.
    ⚠Two changes are **NOT gateable** — the gaze and the scarf both live in
    the drawer, which needs a GL context. Those are certified by screenshots
    and by Chad's eye, and this handoff says so rather than implying coverage
    that does not exist.

---

# 23. ★★★ THE LANDING — flak → main (2026-09-02)

Chad: **"push to main and seads-recon."** What that turned out to involve, and
what a future session must know.

## §23.1 Main had moved 150 commits, and it grew rules while we were away

`sandbox/flak-gun` was cut from `sandbox/r4a-phase0` at `2228182b3`. By the
time we landed, `origin/main` was `96cd0bf19` — **150 commits ahead** — and
three of them govern how landing works now:

| commit | what it changed |
|---|---|
| `83e8ac64b` | **`docs/CONTRIBUTION_SOP.md` + `LANES.toml` + `tools/gate/lane_map.toml`** — the lane registry and the landing rules |
| `a2f718da1` | the gate compares the red **SET** against a machine-written baseline, not a count |
| `8b599e91d` | a **CRLF tripwire** in the gate — one second instead of a 57-minute false red |

★**READ `docs/CONTRIBUTION_SOP.md` BEFORE LANDING ANYTHING.** It is short and
every rule in it was bought with a dated failure.

## §23.2 The merge

- 10 conflicts. **8 were `generated/graph/`** — regenerated with
  `graphify.py`, never hand-merged, confirmed with `--stale`. That is the
  standing rule and it is now paid twice on this branch.
- **`render/draw.cpp` was a REAL semantic conflict**, not a union: main
  refactored `draw_pool(pool, bool enemy, bool flak_pool)` into
  `draw_pool(pool, int tint_mode)` for S3-GUNS' cosmetic pool in the same
  window this lane added `flak_pool`. ★They are ORTHOGONAL — `tint_mode` picks
  WHOSE round it is, `flak_pool` picks WHICH LOOK the pool draws with — so
  both survive as `draw_pool(pool, int tint_mode, bool flak_pool = false)`,
  and the flak pools pass `tint_mode 0`. A "take theirs" would have silently
  deleted the flak tracer look; a "take mine" would have deleted the cosmetic
  pool's per-round tint.
- **`render/sled_model.cpp`** was addition-vs-addition at one point (this
  lane's rider-hide flag against r4a's `RiderBack`). Union, r4a's first. No
  logic of r4a's touched.

## §23.3 ⚠ The CRLF trap, paid exactly as the memory said it would be

After the merge commit, **700 files in the working tree were CRLF** — merging
the LF pin does not rewrite files the merge did not touch. The fix is
non-destructive and must be run **from a COMMITTED tree**:

```
git rm --cached -r . -q  &&  git reset --hard
```

700 → 0, tree clean, HEAD unchanged, **zero code change**. Left alone it would
have produced a false red in an EOL-sensitive test and cost a full gate.

## §23.4 The gate, and what baseline to trust

**1780/1786 — the red set is EXACTLY main's committed baseline, member for
member** (`gate_baseline.py check`): probe P-F, E12.1, and the four `sled_*`.
Zero new reds from the whole flak arc.

★**On which baseline to consume.** `_nightly_main/STATUS` read
`main: 83e8ac64b` while `origin/main` was `96cd0bf19` — by SOP §1 that is
history, not a baseline, so this lane fell back to main's **committed**
`generated/gate/known_reds.txt`. The nightly's fresh re-run for the real tip
has since published **seven** reds: our six plus an unassigned
`ballistic truth harness`, which the ai lane documents as **green in a
lived-in worktree and red only in a fresh checkout**. It is green here, and it
is nobody's regression — do not "fix" it and do not record it.

## §23.5 ★ The lane is registered now — it never was

There was no `[lanes.flak]` in `LANES.toml` and no `flak` in
`lane_map.toml`; this lane's three test files reported as **unassigned** for
its entire life. Both are added, and the LANES entry **announces the three
files this lane touched that it does not own** — `render/draw.cpp`,
`render/sled_model.cpp`, `combat/fx_curves.h`.

⚠★★★**THE TRACER CLAMP IS A CHANGE FOR EVERY GUN IN THE GAME**, not just the
flak: a streak may no longer be drawn longer than the distance its round has
actually flown. The aeroplane always had the identical overhang and always
clipped it behind the camera, so nothing visibly changes there — but it is a
shared path on `render/draw.cpp` and every lane should know it moved.

## §23.6 ⚠⚠ A CROSS-LANE HARM THIS LANE CAUSED, AND THE FIX

This session ran, five times, as a "clear strays before gating" step:

```
Get-Process -Name ctest,seads_tests -ErrorAction SilentlyContinue | Stop-Process -Force
```

That is **machine-wide**. It killed the game-loop lane's gate twice (00:39 and
00:41) and is a candidate cause of the nightly's truncated run. It was scoped
to "my strays" in intent and to every worktree in fact.

★**THE LESSON: A CLEANUP STEP'S BLAST RADIUS IS A PROPERTY OF THE FILTER, NOT
OF YOUR INTENT.** It was introduced for a real reason — orphaned processes from
harness-killed shells were making this lane's own gate crawl (20 tests in 10
minutes) — and then never re-examined once the reason had passed.

**Kill by the PID you started, or filter on the command line, never the name:**

```
Get-CimInstance Win32_Process -Filter "Name='ctest.exe' OR Name='seads_tests.exe'"
  | Where-Object { $_.CommandLine -like '*flak-gun*' }
```

Verified before being trusted: it cleanly separates three concurrent gates
(this lane's, `_nightly_main`, game-loop's). `docs/lessons.md:673` already
carried this rule; this lane had not read it.

★And a second one worth keeping: when the enemy-ai lane told game-loop that
another lane was the sole cause and that this lane was a victim, **that
exoneration was as unevidenced as an accusation would have been**. A wrong
clearing sends the next lane looking in the wrong place. Both were corrected
at the source.

## §23.7 Waiting on a gate, honestly

★**An empty output is not a pass.** The first attempt to run
`.claude/hooks/gate.sh` detached produced no log, no stdout and no stderr —
the launch itself had failed. Checking for `build/.gate_ctest.log` is what
caught it; a session that read "no errors" would have landed on nothing.

★With three lanes gating at once this run took **3950 s** against ~1900 s
solo. Contention is the correct trade — serialising costs the other lanes more
than it saves — and it is not a reason to kill anything.

## §23.8 Where the code is

| what | where |
|---|---|
| the branch | `sandbox/flak-gun`, merged with `origin/main` |
| main | the arc landed as a `--no-ff` merge (see §23.9) |
| the fly tree | `D:/flight_sim2/seads-recon`, `sandbox/r4a-phase0` — main merged IN, never checked out (that branch carries commits on no other branch) |
| the exe he opens | `D:\seads_sandboxes\flak-gun\build-play\seads.exe` and the fly tree's own `build-play\seads.exe` |

★**`git rev-list --count <branch> --not --remotes`** is the number that answers
"what is lost if this disk dies". `origin/main..HEAD` and
`origin/<branch>..<branch>` both measure something else and both mislead — one
counts merged-in history that is already safe, the other counts nothing that is
at risk. (Owed to the enemy-ai lane, which caught itself doing it.)
