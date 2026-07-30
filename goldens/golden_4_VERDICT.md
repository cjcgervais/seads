# Golden Felt Flight #4 — VERDICT (Chad, 2026-07-30)

## Chad's verdict, verbatim

> "okay flew golden #4 and made the recording, ready to report ... I captured 2
> recordings, the first was one where there was no air, so I made the second one. Use
> recording two fow the golden. The feeling was good, buttery smooth a little less snappy
> but low on jitter. Good news is that I couldnt produce a roll over. The knife edge held
> on pitching up down manouvers. Ready to close this and move into the next phase."

## Purpose — what #4 pins

**The v10 buttery small-deflection state** — the feel bought by the pool-ball capture
retirement (`[capture] carry` 1.0→0.0, Chad's ruling) plus lean_gain 8. None of goldens
#1–3 covers slow smooth tracking: #1 = slow/dirty/ground, #2 = fast/clean/air-combat,
#3 = mixed/combat (the v9 camera flight). #4 = **fast/clean/slow-tracking butter**, plus
the **horizon-gate before-state** (one partial bank-over at the documented ~22°
above-horizon geometry, t=55.7 s — see TELEMETRY). None supersedes another.

The accepted trade is part of the pin, in Chad's words: "a little less snappy but low on
jitter" — with his standing conditional from the carry=0 A/B: "If that can be adjusted
without a significant trade off, I am quite happy to trade a little less snappiness to
centre for the buttery feel… It would be nice if it could recoile a little bit faster."
Smoothness is the constraint; recoil speed is the open free variable.

## Provenance / signing record

- Source: `D:\flight_sim2\seads-recon\build-play\felt_flight_18.seadsrec`, mtime
  2026-07-30 14:41. Header: `tag=sandbox/kernel-v5-reconcile@game-kernel-v5-38-gefeb7020a
  kernel=flight-kernel-v10-2026-07-30` — consistent with the green-word build (recon graft
  `efeb7020a`, gate 901/901) and the v10 seal (`flight-kernel-v10-2026-07-30` @
  `f86ee7b9f`, tag verified by this agent at the seal pipeline).
- **Identification derived from data, not from the echo (lesson 7):** two candidate files
  existed (felt_flight_17, 14.8 s, sig-verified — no steep-down entries, no freelook, none
  of the card's content; felt_flight_18, 114.0 s — both steep-down entries, the slow-
  tracking segment, 3 freelook releases). The card's countables identify 18; Chad's "use
  recording two" reconciles with it. felt_flight_17 (Chad: "no air") is set aside,
  unpromoted, SHA-256 `60ef85dc…569908b` recorded here for the ledger.
- fnv1a body signature recomputed = stored = `7215269534658975596` — **MATCH** (scheme:
  recorder.h's variant constants; see TELEMETRY's fnv1a note. First recompute used
  standard FNV constants, mismatched on sealed goldens #2/#3 too, and was diagnosed as
  implementation error — the STOP was withdrawn on evidence, not overridden).
- SHA-256 source ↔ mirror verified identical at promotion:
  `095c5692e73f0624c01f8f5869e4945a21f91dcf433dc537e750bd3c895bc6de`.
- Header untouched (never edit a recording's header). `.seadsrec` force-added
  deliberately per the promotion rule.
- Sealed by the mandalark-kernel docs agent, 2026-07-30, per the CLAUDE.md checklist:
  sig → SHA → pin-derived telemetry with stated constants → predicates recovered and
  reconciled (the t=55.7 episode reconciled against Chad's "couldn't produce a roll
  over": no FULL roll-over; one partial at the horizon-gate geometry, matching his
  Card-1 "only a partial bank" description) → four files → VERSIONS.md.
