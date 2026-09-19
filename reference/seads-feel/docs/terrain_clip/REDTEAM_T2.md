# RED-TEAM RECORD -- terrain-clip T2 `[ground] facet_contact`

Two adversarial fresh-context reviews were run on this lane (standing rule, memory
`red-team-major-work`). Neither transcript lives in the repo; this file is the record of what
each returned and WHERE each finding was folded, so an auditor checks the fold against the
commit instead of taking a summary on faith.

## RT1 -- on T2 (`b58f72e1e`), the rung as first built
**Verdict: SOUND WITH FIXES. No P0.** Dial, identity-by-branch proof and clip census accepted;
five P1s + one P2 raised. All folded as ONE commit, **`c82bef2e2`** (T2b):

| # | finding | fold |
|---|---|---|
| P1-1 | an AI reader could be handed a DIFFERENT ground surface than the kernel | `sim::contact_radius` split into `sim::air_ground_radius` (ground radius, no gear height) + the gear addition; an `Environment&` overload pulls field, dials and injected facet out of the one `Environment` `sim::step` ticks, so the surface cannot fork |
| P1-2 | building prisms based on the OLD surface -- a building could float or sink once contact moved | prism bases moved onto the contact surface; **L7** added |
| P1-3 | the residual was asserted from a sampled guess, not measured | **L6** rebuilt to MEASURE it on the real DEM, four sites, 90 601 points each |
| P1-4 | the exe banner did not say which surface was live, so an A/B could not be told apart after the fact | banner line added and parsed |
| P1-5 | doc overclaimed the fix's reach | invisible-wall / inside-the-hill claims re-scoped to what L2/L3 close |
| P2 | tolerance + naming nits | folded with the above |

## RT2 -- on T2b (`c82bef2e2`), the fold re-reviewed
**Verdict: SOUND WITH FIXES.** One **P0**, plus P1-6, P1-7, P1-9 and a P2. All folded as ONE
commit, **`a18fc9b1c`** (T2c):

| # | finding | fold |
|---|---|---|
| **P0-1** | **the injected prism argument never reached the game** -- `sim/step.cpp` passed the un-faceted surface, so L7's repair was true in the test and FALSE in the shipped exe | the prism argument threaded through `sim/step.cpp`; **L8** added to prove the base follows the facet THROUGH `sim::step`, the path the game takes |
| P1-6 | `deck_look`'s `Environment*` defaulted -- a caller could silently get no environment | un-defaulted; `air_ground_radius` asserts `env.ground` |
| P1-7 | L6's p50 band was an ABSOLUTE pair, so it graded a world nobody ships | band derived from the shipped `[snowpack] base_m` (0.6--1.4x), so a retune moves the band with it |
| P1-9 | a ruling that is Chad's was holding the gate hostage | L6 became a MEASUREMENT leg (prints the table, WARNs over the bar, REQUIREs only ruling-independent invariants); the strict bar moved to the hidden `FACETCONTACT L6-STRICT` `[.t3-strict]`, unregistered with ctest |
| P2 | tape tolerance 5e-3; banner parse anchored; the `app/main.cpp` banner block moved below `render::set_tree_snowhill` | folded with the above |

## OPEN after both reviews -- carried, NOT closed
1. **AI-vs-buildings measurement gap.** `FACETAI` (`[.facetai]`, manual, two 22-min matches)
   binds NO `BuildingColliders`, so it never reaches the `sim/step.cpp` block P0-1 repaired.
   Its numbers are UNMOVED from T2b, honestly so -- not evidence either way. A harness that
   binds buildings is not built.
2. **The Murray pit lid** (T2 doc 5.4). Diving into the Murray pit mouth at ~15 m AGL still
   finds no ground (the tunnel-net lid). NOT in this rung, explicitly not graded by it;
   Chad's ruling owed.
3. **The snow residual ruling** (T2 doc 7). Options A/B/C measured and tabled; recommendation
   is **B** (`aircraft_snow_contact` -- its OWN T3 rung, its own dial, its own fly).
   `L6-STRICT` is red by measurement and by design until he rules; on **C** it is deleted and
   section 7 becomes the record of why.
