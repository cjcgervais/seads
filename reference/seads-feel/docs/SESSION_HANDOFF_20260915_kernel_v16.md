# SESSION HANDOFF — kernel v16 S-yawbudget LANDED (2026-09-15)

Lane: `feel/lateral-yawbudget` in `D:\flight_sim2\seads-feel` — **DORMANT, == main.**
**LAUNCH LINE: read §0 and §6 before touching anything. Nothing is queued for the
kernel; the next rung waits for Chad's word (§5).**

---

## §0 STATUS

**Kernel v16 is on main.** `main = 66036b98e` (landing commit `354f6df3a` + a
docs-only LANES fix). Annotated tag `kernel-v16-yawbudget-signed` at `354f6df3a`.
Landed on the sentinel's GO (`mandalark-kernel-69`) after its own read-only
pre-audit; main fast-forwarded, no force.

| item | value |
|---|---|
| the dial | `[coordination] yaw_vert_budget = 1.0`, gate `yaw_vert_gap_lo/hi = -5..10` deg |
| kernel constant | `kYawBudgetFloor` = 1 deg/s (`control/controller.cpp`, red-team P1-1) |
| walk-back | `yaw_vert_budget = 0` = the v15 tree bit-identically (S-righthand off-arm hash `dbdf52980174305e` intact) |
| kernel delta v15 → v16 | 5 files, +350/−0: `control/controller.cpp`, `control/controller.h`, `control/params.h`, `config/controller.toml`, `config/load_controller.cpp`; `sim/` and `aircraft.toml` untouched |
| gate | `build-gate/gate_v16_b4fbea3d4.log`: **6 failed of 2101 == the baseline six member for member** (`gate_baseline.py check` OK), 22:18–23:21 |
| Chad, landing tip | *"I didnt notice a difference, I can fly it fine"* (~23:20, 17-min natural tape on exe 22:18:37 = `b4fbea3d4`) |
| Chad, budget-only | *"I am actually pretty satisfied with the kernel now … only uncontrolled manouvering will crash you … I'm really good on this"* (~18:50) |
| the ruling that shaped v16 | *"no deck save unload keep the yaw budget"* |
| fly tree | `D:\flight_sim2\seads-recon` `sandbox/r4a-phase0` = `429f8b899` (carries main), pushed; `build-play\seads.exe` 2026-09-15 23:28:40 |

**Records (all committed):**
- `docs/REDTEAM_20260915_v16_yawbudget.md` — fresh-context red-team, LAND-WITH-FIX, all folded.
- `docs/TAPE_ANALYSIS_20260915_normal1.md` — independent analyst on the 17-min normal-fight tape.
- `control/params.h` — the six-actuator table + the red-team record + the accepted residual.
- `docs/flight-log.md` row 2026-09-15; `CLAUDE.md` kernel v16 line; `LANES.toml` `[lanes.kernel] status`.
- `docs/SESSION_HANDOFF_20260913_loop_lateral.md` — the full history of this lane (v15 + the lateral hunt).

---

## §1 WHAT v16 DOES (and does not)

**The fault (TARGET 2):** a large sustained lateral mouse deflection at speed rolls
the airframe past 90° bank; the lift vector is then below the horizon, so the max
pull *is* the dive — the nose slices 60–100° below an aim that is still above the
horizon, 400–700 m per event.

**The dial:** when the nose is below the aim and the airframe is banked, the
rudder's vertical component *digs* the nose down. The budget caps that dig at the
elevator's spare vertical authority (`pitch_ceil − pitch`, × cosΦθ). It runs
BEFORE S-straightline's axis correction (order is load-bearing; the equality
contract in `test_straightline` catches the wrong order). Measured on Chad's own
onset fixtures: ev6 −439 → −121 m, ev7 −719 → −615, ev1 −530 → −438, t5worst
−582 → −534 (9–72% of the descent). V250 lat-40 bit-identical; lat-90 turn
41.01 → 41.80 (not flattened; the 07-06 bank-cap rejection stands).

**What it does NOT do (recorded, pinned):**
- It is **inert past 90° bank** (`cos_phi_theta > 0` guard) — 0 of 120 slice ticks
  on tape 6. It is an ONSET-ONLY mitigation.
- It **does not save the deck event** (`onset_t6_deck`: AGL ≈ 0 on both arms; pinned
  in `test_yawbudget.cpp` "what the ruling gave up"). The actuator that did —
  S-unload — was built, flown ("I like it now"), and **REMOVED at Chad's ruling**
  because it armed on the back half of a pure loop (25.9%). Scrap tag
  `scrapped/s-unload-20260915` = `40325f297`. The loader REFUSES any stale
  `[coordination] unload_*` key.
- Duty cycle: armed on 17.2% of ticks (tape 6), 4.45% (normal tape 1), 1.96%
  (normal tape 2). Not rare.

**The floor (red-team P1-1):** with the elevator clipped, `avail == 0`, so the scale
was `1 − gate` for ANY dig — 475 wings-level ticks on tape 6 had the whole rudder
removed for a 0.2 deg/s dig, and bank sign-crossings stepped the yaw. 1 deg/s of dig
is always allowed; continuous at dig == floor; cost 3–18 m on the dives. Also
covers the pure-pitch roundoff case (yv ~ 1e-17).

**Loader walls:** `line_hold_ff > 0` when the budget is on; `gap_hi > sin 8°` (a
saturated band is the ungated form: lat-90 collapses 41 → 31, yaw flips); `gap_lo
≥ sin −20°` (at −40 the lat-90 yaw demand reads −0.52 — the plane stops following
the mouse); both edges within −90..90° (sin wraps: 170 loaded as 10).

---

## §2 THE ACCEPTED RESIDUAL — read this before proposing a fix

Chad ruled acceptance on 2026-09-15 after seeing the numbers. Evidence:

| tape | style | strict slice events | outcome |
|---|---|---|---|
| normal1 (17 min, pre-floor exe) | his fight: freelook 13.5%, override 11.1% | 0 strict; analyst found **1** fault event (freelook-exit flick 8.8× his p99, 607 m lost / 459 excess, dial inert at onset) | 2 wing-strikes from **aim-down** dives, no respawn |
| confirm1 (2 min, deliberate max-hold, low, 245 m/s) | mouse-only 96.6% | **4** strict (aim +5..+8°, nose −57..−84°, bank 118–134°, dial 0% inside) | **2 crashes** from 394 m and 592 m |
| normal2 (17 min, landing exe) | his fight: freelook 17.3%, override 10.2% | **0** strict, 0 weak-with-aim-above | 0 crashes |

Written in `params.h` as a statement about *this pilot's hand*: he flicks hard
(1-s |dx| p99 ~900 counts) but does not PARK a far-lateral aim; the fault needs
~1 s of sustained far-lateral aim at speed. A second pilot who parks it will find
it. Precedent: flight-log 2026-07-12 ("deflect full left … long dive") — accepted
then too.

**Why freelook avoids it (the red-team's correction of this session's claim):**
freelook WELDS `aim := nose` every tick (`app/instructor_tick.h`,
`snap_forward_to_nose`), so a flick-then-freelook is a flick then ZERO lateral
demand. Override keys set `pursuit = false`. Armed & freelook = 0, armed &
override = 0 on every tape.

**The six actuators, all measured on his dives (table in `params.h`):** bank cap
(rejected 07-06, re-measured: no effect / halves turn), far-aim level-turn
preference (worse), top rudder (inert — the lifting rudder opposes the aim-chasing
rudder on 50% of ticks), K_aoa 10→12.8 (5–13 m, moves the hash), path_above_aim
roll fade (worse — roll cannot raise a nose, confirmed 3×), UNLOAD (works, removed
by ruling). **Do not re-walk these.** If Chad ever re-opens the slice, the only
measured lever is the unload with a narrowed arm (aim body-azimuth discriminator —
in a loop the aim is dead ahead, in a slice it is 90–137° around) — and he said
"never on the unload" on 2026-09-15.

---

## §3 PROCESS RECORD (how this landed — the precedent for v17)

1. Chad's ruling in his words, recorded verbatim before building.
2. Removal net-zero with scar comments + scrap tag; loader refuses the dead keys.
3. Fresh-context red-team on the exact tip, WRITTEN record under `docs/`, every
   P0/P1 folded and re-measured; its second opinion checked against the code.
4. Independent analyst on the pilot's tape (a second fresh agent; the two were told
   not to coordinate). Both records committed.
5. Order of play by the sentinel: the other lane landed first; merge `origin/main`
   ONCE, union, never `-X ours`; kernel firewall verified by `git diff` on
   `sim/ control/ config/controller.toml`.
6. Full gate on the MERGED code tip in the isolated `build-gate/`, detached
   (`scratchpad/run_gate.ps1` shape: rm the test exe, build ALL targets, ctest to a
   log + .status file), verdict quoted BY NAME from `gate_baseline.py check`.
7. Chad flies the merged tip's exe; his word verbatim with timestamp. A fold that
   changes the artefact (the floor) re-opens this step — he flew again.
8. One landing commit: CLAUDE.md line, flight-log row, LANES status verbatim;
   annotated tag; push lane + tag; ping the sentinel with SHA, gate verdict, file-
   by-file kernel diff, ctest -N predicted vs actual, his words; **WAIT** — silence
   is never a go.
9. On GO: fast-forward main; docs-only follow-ups after; recon merge + build-play
   rebuild as its own step, exe mtime reported; recon branch pushed (precedent).

---

## §4 TRAPS THIS SESSION PAID FOR

- **A python heredoc in Bash mangles `\n` and backslashes** — twice broke a printf
  and a macro line. Write scripts to the scratchpad and run the file.
- **`python` on PATH is a venv without numpy/pip.** Use `py -3` or
  `C:\Users\Chad\AppData\Local\Programs\Python\Python311\python.exe` (numpy, pandas).
- **A tape's `alt` column is |p| − R, NOT AGL** — the bore/stope reaches −4 km.
  Use `s_crashed` / `s_wing_strike` / `a_grounded` for ground truth.
- **Claims about freelook/override must be read from `instructor_tick.h`**, not
  inferred. The "flick-then-freelook = sustained aim" argument was wrong.
- **Measure both populations** (good turns AND dives) before proposing a
  discriminator; three earlier candidates were calibrated on failures only.
- **A "bit-identical on pure pitch" claim needs a closed-loop leg through
  `app::tick`.** The first one showed the dial arming on roundoff (yv ~1e-17) — a
  lying instrument, fixed by the floor.
- **The gate's count differs across lanes for a reason:** the atmosphere build had
  16 of its own tests unregistered (stale configure). Diff `ctest -N` name lists,
  don't argue arithmetic.
- **Landing gate in `build-gate/`, nothing else touches it; run it detached**
  (the harness memory guard kills background ctest). `rm` the test exe before every
  mutation rebuild (locked-exe stale relink).
- **`git add -A` once swallowed 349 build files on this lane.** Stage paths.

---

## §5 NEXT RUNG (parked by Chad's word: one signed change per kernel version)

Both are OPEN debts recorded in the v15/v16 records, neither is started:

1. **The tremor debt** — S-righthand's "hand is live" is ANY nonzero aim motion, so
   a ±1-count/frame tremor while belly-up caps the veto (integrated righting 1.76°
   vs 117.75° with a still hand). Cure shape: a windowed NET aim-motion measure.
   Instrument: `test/unit/test_loop_rollover.cpp`.
2. **Knife-edge ringing** — nose-down knife-edge `az_lat = eps·sin(phi)` contamination
   via `lean_gain 8`; predates v14. Instrument: `test_yawbank_balance.cpp`.

How to start (v14/v15/v16 precedent): new lane off main (`git worktree add`, never in
seads-recon), tape the pilot FIRST (`SEADS_FEEL_TAPE=<path>` env var → 160-column
v5 tape with the banner in its header; `build-play/fly_*.bat` are the launch
shape), reproduce offline through `app::tick` with a seeded fixture, ONE dial with
0 = bit-identical and hash-pinned, red-team, Chad flies, §3.

---

## §6 LAWS FOR THIS LANE (unchanged, restated)

- **Gun-director law (Chad 2026-09-12):** the aim IS the guns; the instructor never
  flies the nose where the aim is not; no bank cap (rejected 07-06); fix lateral
  pitch in coordination/skid space; any auto-righting vetoed while the hand moves.
- **Kernel is frozen-near-perfect:** one dial per version, 0 = bit-identical,
  Chad flies each; never tune against harness numbers.
- **Execute the ask exactly; take the ruling; surface conflicts as ONE question.**
- **Publish the method, not just the verdict.** Every number in a record names its
  tape, seed, and exe.
