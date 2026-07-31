# COMFORT PROGRAM — HANDOFF (auto/comfort-orient, 2026-07-16)

## FRESH-SESSION START HERE

Chad's mandate (2026-07-16, autonomous): solve the dogfight motion-sickness/disorientation
problem — immelmanns, split-S, chained loops, multi-bogey freelook, and "space doesn't put
the camera behind me." The autoresearch loop ran to its success bar in one session:
**attribution complete, 3 candidate mechanism sets landed + fresh-context red-teamed (all
P0/P1 folded) + fly-carded, everything AWAITING-FLY, defaults ship behavior-identical.**

- Worktree: `D:\flight_sim2\seads-comfort-auto` (of D:\flight_sim2\seads), branch `auto/comfort-orient` (base: main @ 10187d12b).
- Program constitution: `program.md` (worktree root). Ledger: `docs/comfort_ledger.tsv`.
- THE PILOT DOC: **`docs/comfort_fly_cards.md`** — 3 one-flip cards. NEXT ACTION = Chad flies them.
- Attribution (the WHY): `docs/comfort_attribution.md`. Research: `docs/comfort_research.md`.
- Relaunch the loop: `Read program.md in D:\flight_sim2\seads-comfort-auto and continue the loop.`

## What landed (all default-off / no-press = bit-identical; gate 296/296; zero moved goldens)

1. **The COMFORT INSTRUMENT** — `seads_harness comfort`: scripts Chad's exact maneuvers
   through the SHIPPED app::tick + MiniCamera, prints `COMFORT <scenario> <metric> <value>`.
   Baselines: sustained-turn camera rides **96° off the flight path, never converges**; a
   space tap changes **nothing**; immelmann/split-S exit **180° inverted** (camera-up vs
   horizon); full loops ~0° residual (loop nausea = the DURING, a cue gap).
2. **S-orient — the ORIENT verb** (card 1): freelook DOUBLE-TAP → aim := guarded velocity +
   hard camera cut behind the flight path + horizon rolls level on the release (existing
   S7-hrz). `orient_double_tap_s` ships 0.0 (off); fly value 0.30. Measured: oblique
   95.8°→0.23° in one tick; composed with an immelmann: 180° debt → converged in ~0.9 s.
3. **S-cues — ghost horizon + bank arc** (card 2): peripheral ADI level-line (center-gapped,
   sky-side ticks break the inverted ambiguity) + full-range bank arc (`bank_full` — the
   folded-phi wings-level-while-inverted bug was red-team-killed). `[comfort]` alphas ship 0.0.
4. **S-carets — screen-edge threat indicators** (card 3): off-screen bandits get edge carets
   (identity-skip of the engaged pipper target, six-o'clock hole closed, hysteretic,
   display-NDC continuous through the 0.458 lens shift). `cue_caret_alpha` ships 0.0.

## Constitution notes for the next session

- Every mechanism came through the loop: gate → fresh-context Fable red-team → P0/P1 folded →
  ledger row → fly card. Do not skip the red-team on new mechanisms; all three rounds found
  real P0s (fire lost on 0-tick frames; folded-phi inverted-reads-level; projected-vs-display
  NDC caret teleport).
- The inquisitor RULED: an autonomous upright-dwell recovery trigger is REJECTED — S7-hrz is
  sanctioned strictly as a PLAYER action (the code comment is the constitution). Retirement
  rides player verbs only.
- Feel dials (lag_base/lag_gain/lead/horizon_recovery_rate/aim_sensitivity) untouched, per
  the standing constitution. New dials ship off; fly values live on the cards.
- `control/params.h` gained 5 defaulted caller-side fields (orient window + 4 cue alphas/gap)
  — the accepted freelook_easeback_time pattern; control::step never reads them.
- ⚠ Cross-worktree tool reads can leak to `D:\flight_sim2\seads` (an agent hit this) — use
  explicit paths in subagent prompts.

## Open threads (if Chad's fly says MORE)

- Sustained-turn convergence: one cut can't hold behind a rotating velocity (lag-law
  physics); if Chad wants the camera to STAY behind mid-turn, that's a per-tick weld — a
  DIFFERENT mechanism with graveyard risk (S7-cam2 class); consult before building.
- Dedicated orient KEY instead of double-tap (if the 0.3 s window collides with his
  glance rhythm) — cheap flip, the detector is already pure.
- Radar/mini-map (the full tactical picture) — bigger candidate, deferred.
- Cockpit frame overlay (REC-6) — deferred, weakest evidence of the set.
- If a fly verdict rejects a mechanism: set its config to 0/off, log the verdict in the
  ledger, keep the code on the branch (the S-aimclamp precedent).
