# AI PROGRAM HANDOFF — the primary-data loop is the standing machine

You are the next agent on the SEADS game-AI thread. This doc is your launch.
Read it fully, then do §1 FIRST — before touching any file.

## 1. YOUR FIRST ACTION: ASK CHAD WHAT TO WORK ON NEXT

Do not pick a target yourself. Present him the menu in §5 (with the one-line
tape evidence behind each item), ask which he wants — or whether he has
something else entirely — and wait. When you later hand him a fly, put the
fly checklist INLINE IN YOUR REPLY (his standing rule: never only in a doc).

## 2. THE CONSTITUTION (Chad's 2026-08-06 ruling — binding)

"Use sonnet to code, opus to verify, and fable to do effective thinking and
delegation, using recorded flights' primary data as assessment. No guessing."

- **FABLE** (you): think, attribute, spec, delegate, review every diff, run
  the gate, commit (explicit paths only). You do not hand-write feature code
  when a spec can be delegated; you DO adjudicate every certificate trip.
- **OPUS**: instruments + verification. Headless probes that reproduce the
  real composition under the REAL configs; fresh-context red-teams of every
  landed fix set. Opus verifies; it does not decide.
- **SONNET**: implements explicit specs in ISOLATED WORKTREES
  (D:\seads_sandboxes\<name>, branch off the fly branch HEAD). Never
  commits, never touches sim/, control/, goldens, test/harness/. Tell it to
  run builds/tests SYNCHRONOUSLY (it stalls itself waiting on its own
  background tasks otherwise).
- **PRIMARY DATA RULES EVERYTHING**: the assessment authority is a recorded
  real flight. Certificates are regression locks, never the source of a
  "works" claim. No hypothesis becomes a fix without tape evidence naming
  the mechanism. If the tape cannot answer a question, EXTEND THE RECORDER
  FIRST. The re-fly tape is the acceptance, not the certificate.

## 3. THE LOOP (proven end-to-end, 2026-08-06/07 — copy it)

1. Chad flies build-play normally; the conquest tape records itself
   (auto-on, `build-play/conquest_tape_<n>.jsonl`, crash-safe, fnv1a sig).
2. `python tools/ai_tape.py <tape>` = the standard assessment report
   (threat timeline / duty vs STANDING ORDERS vs on-station / dwell
   clusters with bubble-edge distances / raid order-windows / tunnel / outcome).
3. Fable attributes: raw-tape greps + code reading; where static reading
   runs out, spec an Opus probe that reproduces the REAL chain (real
   loaders, real assign_foes/drone::tick order, a STEPPED player — never a
   stamped-velocity parked state; that fixture trap cost two aiming
   rewrites once).
4. Spec fixes (explicit file:line specs in the worktree's docs/), Sonnet
   implements, honesty rule: any existing-test failure is REPORTED verbatim,
   never adjusted by Sonnet — Fable adjudicates each (welded-literal fixture
   vs real regression; re-derive config-relative, never slacken blindly).
5. Opus red-teams the whole diff fresh-context (+ re-runs the probes
   against the fixed tree). Fold P0/P1s. Gate (.claude/hooks/gate.sh).
6. Fable commits (explicit paths, records into docs/), ff-merges to the fly
   branch, **REBUILDS build-play** (`cmake --build build-play --target
   seads` — release config, game target only; and CHECK ITS MTIME vs your
   commit: the first Phase-1 fly was lost to a stale play binary), smoke
   run to prove the recorder still writes a VERIFIED tape.
7. Chad re-flies ONCE; the new tape is the verdict.

## 4. STATE (2026-08-07 late — R7/FIX-F4b closed)

- Branch: `sandbox/kernel-v5-reconcile` (Chad's fly tree = seads-recon;
  build-play is his play build). Commits: recorder+analyzer `ec75f90af`
  (merge `9615a7731`), Phase-2 fixes `5b05cc731`, handoff `4ff2b33f7`,
  **R7/FIX-F4b `2763ca623`**. Gate 981/981. **PUSHED to origin.**
- Records: docs/ai_primary_data_handoff.md (the origin ruling + Phase list),
  docs/conquest_tape_spec.md (+§9), docs/conquest_tape_redteam.md,
  docs/ai_phase2_fix_spec.md, docs/ai_phase2_records/ (probe attributions,
  the fix-diff red-team, **f4b_pitcrown_attribution.md +
  f4b_offset_witness_adjudication.md**).
- Tapes in build-play/: conquest_tape_1.jsonl (the four-findings baseline),
  conquest_tape_2.jsonl (Phase-2 acceptance, 140-0),
  conquest_tape_4.jsonl (**R7 acceptance**, 18:21, 230-0). (tape 3 = a
  600-tick smoke, ignore.)
- **R7 verdict (menu item c, DONE):** the "pit-crown" was mislabeled — it is
  the Errington PIT FLOOR (BOREAL crosses the lip 132 m off-axis at 55°,
  contains() fires via the trench, RUN's altitude ref = the mouth node 220 m
  below, track_gain commands a further full-cap dive; 19/25 in-net deaths
  over 150 min, one byte-identical event). Fix = `[maverick]
  dive_lookahead_m 150→250`, config-only: pit class 19→0, in-net 32%→8% on
  the probe; ON TAPE 4: 9 entries, ZERO pit-floor deaths. Measured
  dead-ends recorded so nobody rebuys them: projected-contact guard PROVEN
  impossible (inside the arrest envelope before net entry; guard actuator
  lateral, fatal axis vertical), aim-sinking + crater-widening byte-null.
  The 120 m/SHAFT offset certificate was adjudicated a welded-fixture
  calibration and re-derived: 80 m survives ALL traits (fixture envelope
  117→82 m — a measured trade ruled on fleet primary data).
- Phase-2 acceptance (still true): first-ever enemy hits on Chad, crash
  events 95→3 (vacuum pen closed), raid pause exact at the 1500 m dial.

## 5. THE SUGGESTED IMPROVEMENT MENU (present to Chad; each line = tape fact)

a. **Burst lethality / difficulty**: TWICE documented now — tape 2 (i=2,
   4 hits, 100→0) and tape 4 (i=3 at 14:56, 6 hits in ~1 s, engine out in
   a mutual head-on kill). ~30 dmg/hit at the current difficulty. Pure
   dial ([combat] difficulty / bandit damage). Is one-burst engine-out the
   intended threat level?
b. **More fights should reach guns**: Offensive duty was 1-2% across the
   fleet on tape 2 and ~0-3% on tape 4; the only bursts that reach him are
   head-ons where his 259 m/s doesn't matter. Levers: bail energy, merge
   range (bfm_attack_range_m — the flagged 1200-vs-750 question),
   corner-speed / closing geometry. Needs a fresh attribution pass on the
   closest-approach geometries before dialing.
c. **[DONE — R7/FIX-F4b, see §4] → the NEW smallest tunnel fish**: tape 4's
   two residual in-net deaths are a far-ascent LATERAL (enemy i=4, the
   known class the fleet probe measured at ~8%) and a mid-chamber clip
   (ally i=7, right after killing the deep pump). Deferred rungs already
   named in commit `2763ca623`: bore_track gamma floored at the path slope
   near the mouth node; a DIVE_IN→RUN handover certificate at the sunken
   Errington mouth; port the f4b sd_parts() forensics into the fleet
   harness.
d. **Contest the Black Stope**: deep-pump strikes almost never go
   on-station (order windows 550 s, on_station ~0-3 s). If Chad wants
   dogfights IN the chamber, the strike pipeline needs a dedicated round.
e. **Recorder extensions**, if a chosen question needs them: nose/orientation
   on the tape (the on-station proxy uses velocity), player-crash events,
   per-gate fire-chain flags. Extend-the-recorder-first is the law.
f. **Not-AI threads**: the master plan's R7 escape-energy telemetry, the
   07-21 mine-entrance art rulings, the seal-pass census — different
   memories/handoffs own those; mention they exist if Chad asks for the map.

## 6. TRAPS ALREADY PAID FOR (do not re-buy)

- Stale play binary = a lost fly (check build-play mtime EVERY time).
- Windows text-mode CRLF corrupted the tape sig once (ofstream must stay
  binary); the green gate is structurally blind to seads.exe — always one
  live smoke tape through the analyzer after a rebuild.
- Pool-slot reuse hid 90% of shot events from the first detector — never
  trust a new instrument's silence until Opus has attacked it.
- raid/strike flags are STANDING ORDERS; rpi/spi are ORDER WINDOWS;
  on-station is DERIVED. Never read an armed order as behavior.
- Non-ASCII Catch2 names, heredoc backslash-eating, welded-literal loader
  fixtures, `min(bump, deficit)` speed-matching that can't close — all in
  docs/lessons.md + the phase-2 records.
- Sonnet backgrounds its own builds and stalls: tell it foreground, always.

Launch line for you: "Read docs/ai_next_agent_handoff.md; do §1."
