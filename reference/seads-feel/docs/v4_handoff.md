# KERNEL V4 HANDOFF — session 2026-07-17 (the loop ran to completion)

**Branch `auto/kernel-v4`, worktree `D:\flight_sim2\seads-v4`, pushed to origin.**
**Status: ALL FOUR ASKS LANDED, red-teamed, fly-carded — AWAITING CHAD'S STICK.**
**Gate 342/342, ZERO moved goldens across the whole session (every still-hand path is
bit-identical sealed-kernel v3; all golden scenarios structurally outside every new
mechanism).**

## ✅ THE RIMSHOT MISCONCEPTION IS RESOLVED — S-rimshot v2 UNIVERSAL landed 2026-07-17

Chad caught the flick misconception (the rimshot was built as a big-FLICK-gated event; he
ruled the rebound UNIVERSAL — "any deflection at all... Always, even mid-track") and the
same-day rebuild landed on this branch: ARM/snap_on/park-edge DELETED, ENGAGE is the
self-calibrating FINE-endgame arrival trigger on the net closing rate, re-flicks
abandon-and-chase with a fresh bounce, one bounce per arrival (completion refractory).
Evidence trail: ledger row **2.5** (two plan-stage consult rounds, two diff red-team rounds
— round 1 found two real P0s including a MEASURED blend-band multi-engage — 13 mutants
killed, golden deliberately re-recorded twice with signature verification). CARD 2 in
`docs/v4_fly_cards.md` is REWRITTEN for the universal build; the misconception handoff
(`docs/v4_rimshot_MISCONCEPTION_handoff.md`) is historical. Named trades on the card:
lateral/banked arrivals creep (nothing to carry — one attempt then regroup), the ~6°
post-failed-attempt regroup fence (pre-agreed fallback: aim-motion-gated clear), the
~0.15° post-arrest micro-drift. Gate 356/356.

## START HERE (fresh session)

1. Chad flies `docs/v4_fly_cards.md` — cards 1-4, one verdict line each. That is the ONLY
   next action until verdicts exist.
2. On verdicts: fold retunes as one-dial rows in `docs/v4_ledger.tsv`; a REJECTED mechanism
   gets its dial set 0 (bit-identical off) and the code stays on this branch.
3. Nothing merges to main without Chad's explicit ruling (program constitution).
4. The program constitution is `program.md`; the full evidence trail is
   `docs/v4_ledger.tsv` (rows 1.0-3.1); specs in `docs/v4_rung{1,2,3}_spec.md`;
   research bedrock `docs/v4_research.md`.

## What landed (each: mechanism + instrument + red-team folded + fly card)

- **RUNG 1 S-aimff** (ask 1, "responsive elevator while aiming"): aim-rate feedforward
  ω_des += 0.3·ω_aim (pitch/yaw), frame-rate-independent by the app-side smear seam.
  Track instrument: lag −26% at V140, sine phase 50°→33°, zero new oscillation. Red-team
  P1 folded: the yaw ceiling bounds POINTED+FF only (coordination exemption preserved).
  Commits 02b1b9bdd + 4aef5e935.
- **RUNG 2 S-rimshot** (ask 2, Chad's 4-iteration capture spec): ATTRIBUTION FIRST — the
  slow-in is the K_theta taper (braking sqrt never binds). Hysteretic snap-capture event
  (>30° swept + park edge): CARRY the full rate through the aim to the far rim, then the
  full-authority sqrt arrest with the τ-surface release landing DEAD at center (drift
  0.014-0.058°, exactly ONE reversal). Smooth tracking structurally cannot arm. THE
  SURPRISE: the naive unshaped return blew 0.5-1° past center on the servo coast — the
  τ-surface release (τ = I/(K_w+damp·q_eff)) is the completion of the plant inversion,
  the S-dampff pattern a third time. Physics walls named on card 2 (rim overrun 1.3-1.8×;
  AoA bleed at V140; arrest tail ~100-400 ms at 0.03° precision — the named next lever if
  Chad reads it soft = an active counter-brake phase, NOT built). Commit 713a0ee59.
- **RUNG 3 S-globelook** (ask 3, freelook globe inertia): pure OrbitInertia helper —
  instant grab / exact-integral coast τ=0.2 / 90°/s cap / wall absorb / all resets paired.
  Red-team P1 folded: 0.075 s stillness dwell kills the integer-mouse staircase
  amplification (was up to 3.7×, now exactly 0, ctest-pinned). Cosmetic; constitutional
  sweep clean. main.cpp glue is ctest-uncovered (honest ledger). Commit 5c5b1c6b4.
- **ASK 4 composition** = card 4 (all dials ship ON; every instrument number was measured
  with the composition live). **ASK 5 auto-level** = deliberately NOT built (research says
  discrete verbs won; expect-rejection variant available on request — card 4 footer).

## The three off-switches (attribution discipline)

`[aim_ff] gain = 0` · `[capture] carry = 0` · `[freelook] inertia_tau = 0` — each is a
STRUCTURAL off (bit-identical expression tree, executably pinned). One dial at a time.

## Process notes for the next session

- Build: the worktree's deps are LOCAL (`deps_src/`, gitignored) because the FetchContent
  network populate hung — configure with the four `-DFETCHCONTENT_SOURCE_DIR_*` flags if
  the build dir is ever wiped (see the session transcript or just copy from a sibling).
- Windows App Control intermittently blocks a freshly-linked exe (BAD_COMMAND on smoke
  tests) — delete + relink clears it.
- The comfort style round (ed7fc0224) red-team debt was paid this session (SOUND-WITH-FIXES,
  P1 ladder-reach vacuity folded, commit a2660c3db).
- AT-9's shipped-gain leg carries a 15 m cross-fps bound with its event-fires premise
  pinned — the widening is honest amplification of the accepted smear divergence, NOT a
  frame-rate law leak (rung-2 red-team, judged + pinned).
