# THE KILL CHAIN — RESOLVED (game-AI-R5, 2026-08-05)

**Chad's order (verbatim): "I ordered for ai that can kill me." DELIVERED and
GATED**: the game-loop certificate's "C kill chain" section (test_maverick.cpp)
runs shipped-difficulty attackers against a physically-parked player through
the REAL fire pipeline and they SHOOT IT DOWN inside three minutes. This doc is
the evidence trail: every link was measured before it was touched.

## The chain, link by link (all shipped; each measured)

1. **Fire cone 5 -> 14 deg** (scenario.toml, landed with R4): the sniper cone was unholdable
   against any maneuvering target — bandits merged without ever firing.
2. **Merges no longer stolen**: the tunnel-run countdown fired MID-MERGE and
   the committed TRANSIT marched the fighter 7 km off (the literal "they just
   merge and don't know how to fight"); the leash hauled cross-dome fighters
   home mid-fight; raid orders muted guns. hold_runs |= close-merge; leash
   skips a live fight; raid yields inside raid_fight_yield_m.
3. **Corner-speed law**: turn rate is g·tan(bank)/V under the RULED 55-deg
   bank cap, so speed is the only honest turn lever — the pursuit speed bump
   now scales by nose-on alignment^2 (chase fast, turn slow).
4. **Tactical spacing**: bfm_frustration_s 20 -> 8 (the unresolved Offensive
   case IS the orbit spiral — break off early), bfm_extend 4-10 s -> 8-16 s
   (re-attacks start 2.5-3 km out so alignment completes on the run-in).
   Measured: turn radius at fight speeds ~= fight range, so pure pursuit
   orbits forever without this.
5. **THE R5 CORE — the ballistic spawn solve** (combat/kill.h, fed by
   PursueCmd::tgt_pos/tgt_vel -> DroneState::gun_tgt_*): within the (strict)
   fire gate the ROUND leaves on the true firing solution — a 4-iteration
   fixed point over gravity + the round's own drag, where the drag DECAYS THE
   INHERITED velocity component too (gamma = ln(1+kvt)/(kvt) applied to both
   inheritance and muzzle displacement). bfm::lead_point aiming was measured
   15 m wide on a full-crossing shot at 400 m (drag_k 8e-4): first-order
   intercept, no gravity, no drag-decay. Isolated probe: 0.02 m astern /
   0.19 m half-cross / 0.24 m full-cross. The solved direction is slewed at
   most 10 deg off the harmonized boresight (still reads as a boresight
   burst) + a 0.4 deg deterministic circular spray (age-phased, per-pilot
   offset — the pure no-clock/no-rng seam) for tracer texture.

## The two great false trails (kept so nobody re-walks them)

- **"Achieved turn ~2 deg/s vs 9.4 available"** — half true: the step harness
  showed the autopilot itself turns 5-8.5 deg/s at a sustained command; the
  fight-observed 2-3 deg/s was speed (V dominates 1/V) + the merge thefts
  above, not a broken execution layer.
- **THE FIXTURE PHANTOM (the big one)**: the certificate's "parked" player was
  built with drone::level_state_at, which stamps CRUISE VELOCITY into a state
  the fixture never steps — a mover that never moves. The (correct) solver led
  that phantom 60-90 m ahead of the eternal position; every crossing round
  missed by exactly the phantom lead while astern shots (lead along the line)
  still hit. Two full aiming-law rewrites measured "no improvement" against
  this artifact before the state capture exposed it. Class: the INVERSE of the
  fixture-no-op lesson — a fixture-manufactured defect. Rule: a parked test
  target must carry zero velocity (or be honestly stepped).

## Residual polish (not blockers, noted)

- The vs-player gamma envelope: a target far ABOVE is un-gunnable
  (pursue_max_gamma 30 deg cap); DECK targets are protected by the
  terrain-avoid guard. Real envelope limits; acceptable.
- AI-vs-AI attrition stays ABSTRACTED (combat::ai_guns_on window x
  difficulty-DPS x kAiVsAiDpsFrac 0.8 — my pacing call, not a Chad ruling);
  with the ballistic solve now real, a future rung could give drone-vs-drone
  rounds real sweeps and retire the abstraction. Section B's
  REQUIRE(ai_kills >= 1) stays absent until then.
