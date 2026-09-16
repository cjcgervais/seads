# ENEMY-AI RUNG SESSION HANDOFF — 2026-08-20

LAUNCH LINE: "Read docs/SESSION_HANDOFF_20260820_enemy_ai.md in D:\seads_sandboxes\enemy-ai;
continue the E-ladder." The binding spec + ledger is docs/ENEMY_AI_E1_E2_SPEC.md (same dir).

## STATE
- Worktree D:\seads_sandboxes\enemy-ai, branch sandbox/enemy-ai @ 95d0b2c38 (fly-tree HEAD).
  ALL WORK UNCOMMITTED — Chad's hold stands until the mesh agents finish landing on
  sandbox/audio in seads-recon. NEVER commit until he releases it.
- E1 (kill chain vs the ace) + E2 (black stope offensive) BUILT + double-red-teamed
  (design + full diff, both SOUND-WITH-FIXES, all folds applied) + GATED 1390/1394
  (the 4 failures = pre-existing GI4 sled debt). Details/measurements in the spec ledger.
- Chad FLEW build-play (tape build-play/conquest_tape_1.jsonl, 13:41): the new AI was
  PROVEN live, but the felt verdict was "didn't engage" — attributed on tape: the on-order
  collapse put enemy strikers in committed TRANSIT 53-80% of their lives; he executed them
  as free prey (all dead by 3:34, permanent). Allies meanwhile destroyed BOTH enemy pumps
  (deep pump via the tunnel at 7:17) — the E2 machinery works.
- E3 (felt pass: TRANSIT fight-yield + concurrent-striker cap, spec section RUNG E3) —
  OPUS BUILDER IN FLIGHT right now (background agent). E4 (visibility: aircraft haze
  exemption, tag halo/size/red, engagement glint — render lane) and E5 (reinforcement
  waves — airborne respawn must be a SWAPPABLE seam per the millwright canon) are QUEUED
  behind it, specs in the same doc.

## PIPELINE REMAINING
1. E3 lands -> Fable adjudicates verbatim failures (never silently re-pin).
2. Opus builds E4, then E5 (sequential — ONE build dir, never two ctest gates on it).
3. ONE fresh-context Fable red-team over the combined E3+E4+E5 diff; fold P0/P1.
4. Full gate (ctest in build/), rebuild build-play --target seads, CHECK ITS MTIME.
5. Fly checklist INLINE in the reply to Chad (his standing rule). Carry the two repeals
   already flagged: "straight-line escape" repealed by intercept 265; strikes symmetric.
6. On Chad's commit release: merge into sandbox/audio in seads-recon, REGENERATE THE GRAPH
   in the same commit (generated/graph/ is tracked), rebuild seads-recon build-play
   (mtime check), smoke one verified tape through tools/ai_tape.py.

## PROCESS (Chad-ruled this session)
Opus builds / Fable specs, red-teams fresh-context, adjudicates every trip. Graph not grep.
Primary data rules: tapes are the verdict, probes use STEPPED players (never
stamped-velocity parked). Workflow tool for fan-outs.

## CANON LANDED THIS SESSION (already recorded, do not redo)
- Game_loop_idea/MASTER_PLAN.md §2·AI: pilots first (hunt us AND seek the pump),
  millwrights later with a lower-fidelity brain; AI quality outranks world/art expansion.
- Memory millwright-tier-canon: 2 classes pilot/millwright, per-respawn choice only while
  your pump is damaged, AI millwrights dispatched on pump damage (destroyable), pilot
  respawn = land at a map marker.

## TRAPS PAID FOR THIS SESSION
- An is_underground_mode-only arena-guard clause shoves plain bore runs off the spine —
  guard coverage is STRIKE-HISTORY based (strike_ticks survives RUN->CLIMB_OUT while
  in_arena_open).
- The E2.1 starvation was NOT a retry livelock (instrumentation refuted it): the 150 s
  TRANSIT budget spanned both approach stages — transit_fix_grace_s 90, earned once.
- ai_guns_on's 60/600 band was a stale copy of the fire dials; the cert mirrors the app's
  stamping now, with EXPLICIT strikes/air_war section isolation.
- A single 10-min engagement measures luck — use the 8-ensemble probes for any retune.
