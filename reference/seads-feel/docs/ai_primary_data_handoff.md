# AI PRIMARY-DATA HANDOFF — the recorded-flight assessment program

**Chad's ruling (2026-08-06, binding): "use sonnet to code, opus to verify and
for fable to do effective thinking and delegation and using recorded flights
primary data as assessment and no guessing." We should always work from
primary and complete data.**

This document is the launch prompt for the next session(s). Read it fully
before touching anything.

---

## 1. THE ROLES (the seal-pass binding, extended)

- **FABLE** — thinking, attribution, and delegation ONLY: reads tapes/evidence,
  forms the mechanism story, writes explicit specs, reviews every diff,
  runs the gate, commits (explicit paths). Fable does not hand-write feature
  code when a spec can be delegated.
- **OPUS** — verification: builds/runs instruments, replays tapes, red-teams
  each landed mechanism fresh-context, checks the math, hunts fixture
  artifacts. Opus verifies; it does not decide.
- **SONNET** — the workhorse: implements Fable's explicit specs in ISOLATED
  WORKTREES. Sonnet never commits, never touches `sim/`, `control/`, or any
  golden, never invents scope. Escalate Sonnet -> Opus -> Fable on failure.

## 2. THE PRIMARY-DATA DISCIPLINE (no guessing — the constitution of this program)

- **The assessment authority is a RECORDED REAL FLIGHT** (the shipped game,
  the shipped config, Chad flying). Headless certificates on synthetic
  fixtures are secondary: regression locks AFTER tape-driven attribution,
  never the source of a "works" claim.
- **Why this is now law — the R3-R5 lesson:** three rungs of certificates
  (uniform terrain, test tunnel dials, no real bubbles, a hand-built parked
  player, a hand-mirrored copy of the app's order glue) all passed while
  Chad's real flight contradicted them on every count (see §4). Two
  specific recorded traps: the certificate glue was a FORK of app::tick
  (red-team flagged it), and the "parked" player carried level_state_at's
  stamped cruise velocity in a never-stepped state — the correct ballistic
  solver led that phantom 60-90 m and two aiming rewrites were measured
  "no improvement" against a fixture artifact.
- **If the tape cannot answer a question, EXTEND THE RECORDER FIRST.** No
  hypothesis graduates to a fix without tape evidence naming the mechanism.
- Practical traps already paid for: parked test targets must carry ZERO
  velocity; Git Bash heredocs on this box eat backslashes (emit code via the
  Write/Edit tools, never `cat << EOF` with `\n` in it); non-ASCII
  TEST_CASE/SECTION names silently break ctest filters.

## 3. THE DATA AUDIT (2026-08-06 — why phase 0 exists)

- The only recorder is the F9 FELT tape (`test/harness/recorder.h`,
  `felt_flight_<n>.seadsrec` beside the exe). Six tapes exist in `build/`,
  all dated 2026-07-24/30 — **nothing from the 2026-08-05 R5 fly**.
- The felt format records ONLY the player: resolved input + player SimState
  pin (+ 2 controller telemetry fields). **Zero AI content** — no drone
  states, modes, foes, orders, fire events, or conquest events. It exists
  for bit-exact kernel replay, not AI assessment. Leave it untouched (its
  goldens pin the format).

## 4. GROUND TRUTH — Chad's fly report of game-AI-R5 (2026-08-05, verbatim findings)

1. "I did not get hit by an round or shot at by the enemy. Only merged with
   and I sought them out. Pretty easy to kill same as before."
2. "Most of the enemy and friendlies loitered about 2km to the east of the
   sudbury bubble by the end."
3. "I saw two your pump is being attacked but the one I saw attacking I only
   saw on the map and they flew away from it prior and I had to chase them,
   they did not try to complete the pump attack."
4. "I did see an enemy enter the murray tunnel but they died (crashed)
   becasue I followed them in and they were gone."

Code state when he flew: branch `sandbox/kernel-v5-reconcile` @ `afd44a010`
(tags game-AI-R2..R5), gate 947/947. All four findings contradict passing
certificates — treat every certificate claim about AI behavior as UNPROVEN
in the real world until a tape confirms it.

## 5. THE PHASES

> **PHASE 0 + 0b: DONE (2026-08-06).** Built to `docs/conquest_tape_spec.md`
> (the binding spec incl. the §9 Opus round-1 folds) exactly per the roles:
> Fable spec/review/gate, two Sonnet builds in the isolated worktree
> `sandbox/ai-tape`, Opus fresh-context red-team x2 (verdicts in the round
> notes; ROUND2 = CONFIRMED-FIXED). Recorder = `app/conquest_tape.h` +
> `app::ConquestTapeHook` in step_frame + main.cpp auto-on wiring; analyzer =
> `tools/ai_tape.py` (`--selftest` green; run it on any tape for the standard
> report). Proven END-TO-END on the live binary: a real `--smoke` conquest run
> writes `build/conquest_tape_<n>.jsonl`, signature VERIFIED, analyzer report
> correct (this smoke check caught the Windows text-mode CRLF sig corruption
> the green gate is structurally blind to). Gate green, layer check green.
>
> **ATTRIBUTION CANDIDATES ALREADY SURFACED BY THE BUILD (test before
> believing, per §2):**
> - **F4 candidate:** in conquest, only GUNFIRE kills go through the
>   no-respawn path — a terrain/tunnel crash calls `respawn_in_place`
>   unconditionally (drone/drone.h ~1481), reviving the drone at the STOCK
>   scatter with mav/raid/strike/foe reset. Chad's "entered the tunnel and
>   was gone" is consistent with crash + silent relocation. The `dc` event
>   (age_ticks reset) now records exactly this.
> - **F1 instrument lesson:** the first ef detector (pool-slot edge) missed
>   90% of real shots — the tape itself must be adversarially verified before
>   its silence is treated as evidence. It now is (Opus oracle: 0/24000
>   missed).
> - **F2/F3 layer discipline:** raid/strike/def flags are STANDING ORDERS
>   armed on every pilot every tick; rpi/spi are the ORDER WINDOW; actual
>   on-station duty is derived (raider_on_station thresholds). Never read an
>   armed order as behavior.
>
> **NEXT = PHASE 1: Chad flies one normal conquest session.** Nothing to
> press; tapes land beside the exe. Then Phase 2 attribution strictly from
> `python tools/ai_tape.py <tape>`.

### Phase 0 — THE CONQUEST FLIGHT RECORDER (Sonnet codes, Opus verifies, Fable specs/reviews)

A NEW tape, separate from the felt tape (do not touch recorder.h's format):
- **Auto-on whenever conquest is enabled** — no keypress dependence (the F9
  dependence is why we have no data). Auto-flush on exit AND periodically
  (a crash must not lose the session). File: `conquest_tape_<n>.jsonl` (or
  versioned CSV) beside the exe, with a version tag + fnv1a signature line.
- Sample ~5 Hz (every 24th sim tick; tick-driven, AT-9 discipline):
  - per drone: spawn_index, pos (double), vel, hp, inert, engaged, foe,
    wants_fire, bfm mode, maverick mode, raid/strike/defend active flags,
    leash blend, friendly_side;
  - player: pos, vel, hp summary, alive;
  - conquest: pump hp x4, radius_scale, score, outcome, pump_under_attack /
    raided_pump;
  - events (every occurrence, not sampled): enemy round spawned AT PLAYER
    (shooter index, range), player hit (damage), AI-vs-AI kill, pump death,
    drone crash/respawn (with position + maverick mode at death), tunnel
    entry/exit (net.contains transitions).
- Purity: the recorder READS app state only (a render-side consumer like the
  HUD — never feeds sim/control); it may live in app/ glue. Budget: ~10
  drones x 5 Hz x a short row = a few MB per session — no perf excuse.
- **Phase 0b — THE ANALYZER** (`tools/ai_tape.py`, Sonnet, Opus-verified
  against a synthetic hand-built tape): loads a tape, emits the standard
  assessment: shots-at-player + hits timeline; per-drone mode/duty
  time-in-state; position dwell clusters (the loiter detector: report any
  >60 s cluster of >3 drones with centroid + nearest bubble-edge distance);
  raid/strike timelines (order on -> on-station -> damage -> abandoned, with
  player range at each transition); tunnel entries and in-net deaths with
  positions; foe-assignment churn. Plain text + optional PNG.

### Phase 1 — ONE FLY (Chad)

Chad flies a normal conquest session. Nothing to remember — the tape records
itself. Deliverable: the tape file(s).

### Phase 2 — ATTRIBUTION FROM TAPE (Fable), then fix (Sonnet), verify (Opus)

Each §4 finding becomes a tape query BEFORE any code is touched. Starting
hypothesis sets (to be tested, not believed):

- **F1 never shot at**: did ANY drone ever hold foe=player? ever wants_fire?
  ever spawn a round at him? If foes were assigned but no fire: gate
  component timelines (cone/range/coordination/guns_hot) at closest
  approaches. If no foes: the real-config assign_foes path (engage_range vs
  actual geometry, max_engaged, the furball flag state in his game.toml).
- **F2 the 2 km loiter cluster east of the Sudbury bubble**: the dwell
  detector + per-drone duty at cluster times. Candidates: leash standoff at
  the REAL ellipse edge (the old vacuum-strand class — pursuit/defend duty
  pointing inside, leash blending outside), air-seek at the real atm field,
  defend orders parking pilots at a pump that sits outside the shrunk dome,
  foe hysteresis pinning everyone to one unreachable target. NOTE: both
  factions in one cluster is the strongest clue — find what BOTH duties
  point at there.
- **F3 raid abandoned before completion**: correlate the abandon tick with
  the player's range crossing engage/disengange (the pause is DESIGN —
  defend-by-showing-up); if it abandoned while Chad was far, inspect the
  raid gate inputs at that tick (raid_range_ok latch, defend preemption,
  fight-yield). Also judge the DESIGN: if the pause reads as "they don't
  try", maybe raiders should press unless actually attacked — Chad rules.
- **F4 the Murray tunnel death**: death position + maverick mode from the
  crash event. Rim auger at entry (the known R2 ~12% class) vs in-bore wall
  strike under the REAL game.toml tunnel dials (all bore certificates ran
  the test_tp dials — the real net was never flown headlessly; if in-bore,
  run the R2 money legs against the REAL dials first).

### Phase 3 — regression + re-fly

Tape-derived fixes get (a) a targeted certificate leg on the REAL config
where feasible, (b) a fresh-context red-team, (c) the gate, (d) ONE re-fly
with the recorder on. The tape from the re-fly is the acceptance, not the
certificate.

## 6. LAUNCH LINE FOR A FRESH SESSION

"Read docs/ai_primary_data_handoff.md and execute it: Fable thinks and
delegates, Sonnet codes to spec in isolated worktrees (never commits, never
touches sim/control/goldens), Opus verifies. Phase 0 first. No conclusions
without tape evidence."
