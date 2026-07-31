# SEADS knowledge base — index (the game-engine beginnings)

2026-07-30, built overnight by the vessel-ontology autoresearch loop (docs-only; the
standing guardrail held: no feel dial was tuned against a harness number). Every
document below was drafted by a subagent, then adversarially verified by a
fresh-context re-derivation pass (all five verdicts SOUND-WITH-FIXES; every P0/P1
folded, the fix list lives in the git log). Chad is the audience: each opens with a
**Feel** section in cockpit language for his ruling.

## The vessel layer (relational cascades)

- **[vessel_ontology.md](vessel_ontology.md)** — vessel #1 (the flown fighter): its
  coordinates cited from the live TOMLs, the relations (power↔sustained turn,
  W/S↔corner, authority↔snap, the PIO/ZOH walls), the 11 flown-REJECTED walls with
  Chad's verbatim words (where feel-space ENDS), an 18-row retrodiction self-check
  (the ontology re-derives the flown plane before it may predict a new one), and the
  ordered new-vessel derivation procedure.
- **[vessel_brief_p47.md](vessel_brief_p47.md)** — the energy fighter: dives away
  from everything, must never turn-fight. Hypothesis delta tables + harness-verifiable
  envelope targets.
- **[vessel_brief_a6m2.md](vessel_brief_a6m2.md)** — the turn fighter: the
  know-your-plane-vs-theirs competitive dynamic from day one; low-speed roll
  celebrated, dive lock-up encoded in the compression knee.
- **[vessel_brief_bushplane.md](vessel_brief_bushplane.md)** — the DHC-2 Beaver
  float/ski archetype (Whitewater Lake): slow, heavy, honest.
- OPEN RULING (named in all three briefs): the inter-vessel power convention —
  vessel #1 measures arcade ×1.5 on the static-thrust baseline while the briefs
  hypothesize ×2; Chad rules the convention before any vessel sandbox is cut.

## The world layer

- **[world_scale_memo.md](world_scale_memo.md)** — plane scale vs the 15 km sphere,
  the horizon table (100 m AGL → ~1.7 km horizon), bubble sizing from turn-circle
  physics (~3–8 km), the 100 m atmosphere-FLOOR design note (inverts the current
  ceiling; requires the already-scoped `sim::Environment` phase), and the tiered
  tree-collision recommendation. Ends with the rulings Chad must make.

## Canon (already ruled — never rediscover)

- `SPEC.md` — the constitution; §0 the supersession ledger. `docs/HARNESS.md` — the
  verification SOP. `docs/TEACHING.md` — the mental models. `docs/flight-log.md` —
  every flown verdict, verbatim; the REJECTED rows are the walls.
- `D:\flight_sim2\Game_loop_idea\MASTER_PLAN.md` — Scarce Skies game canon.
- `D:\mandalark-kernel\docs\DECISIONS.md` — the guard ledger (read-only from here;
  changes hand off through Chad).
- The kernel seal line: v9 = tag `flight-kernel-v9-2026-07-29` (approval logged at
  `29787debc`); buttery rung 1 (`lean_gain` 8) = `e22807a04`, AWAITING FLY.

## Proposed next /goal

1. **Chad flies the buttery rungs** (`docs/buttery_fly_cards.md`) — rung 1 verdict,
   then rung 2 (side cone), conditional rung 3 (the real B/C rudder ladder). If the
   end state is his "final tuning of the cascade qua mouse aim": the v10 seal +
   Golden Felt Flight #4.
2. **The power-convention ruling** (above), then the first vessel sandbox — the
   A6M2 is the highest-value first opponent (the competitive dynamic needs two
   planes, and the Zero is the sharpest contrast to vessel #1).
3. **Envelope instruments** (the briefs' named gaps): harness modes for stall-V,
   top-V, and a sustained-turn-rate sweep — instruments, not tuning.
4. **PARKED CANON — do not rediscover as new**:
   - The deferred **v10 candidate (manual-stays horizon)**, recorded at `3ab831968` —
     an autolevel-adjacent idea deferred to a future sandbox with the S7-mouselevel
     caution attached. Parked until Chad re-opens it.
   - **The POOL BALL / cue-ball capture (S-rimshot v3), RETIRED 2026-07-30 — parked,
     not deleted** (machinery + dials stay in code/table; `carry = 0` is the structural
     off). Chad's rationale verbatim, which IS the re-entry condition stated in reverse:
     "I think the plant and flight control authority as it is now gives me sufficient
     closing ability to intercept an opponent diverging away from my gun solution,
     especially with the powerful elevator at full bank in this kind of situation. So
     park the cue ball behavior as retired for now." ⇒ **if that closing ability ever
     degrades — a plant retune, or a LOW-AUTHORITY vessel — the park unwinds.** The
     cue-ball is a compensation that scales inversely with plant authority (ontology
     R6): retired on vessel #1, plausibly live on an A6M2-class vessel.
