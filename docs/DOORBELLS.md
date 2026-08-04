# DOORBELLS — paste one line into each agent's session

**Chad, 2026-08-03:** *"sEND A DORBELL TO THE OTHER AGENTS SO THEY CAN DO THEIR WORK TOO"*

Each block below is **one paste**. It is a **pointer, not content** — nothing can go stale in
transit, because every agent reads its own current row on arrival. Chad's only job is the paste.

---

## → `cascade-recorder` (`D:/EvC2026_sandbox_cascade`)

```
python D:/mandalark-kernel/tools/audit_graph.py
Read D:/mandalark-kernel/docs/consults/BARSMOOTH-ROLL-ATTRIBUTION-VERDICT.md -- your
BAR-SMOOTH packet is ANSWERED and your 1.485 /s reproduced to the digit (128 reversals).
The instrument is sound. It went one step further: reversal share is not magnitude share,
and the jitter attributes to the AIM channel (rollAimApp 1.102 /s) not the dwell servo
(rollBstApp 0.093 /s) -- and the COMMAND reverses more than the plant does, so the source
is upstream of inputState.roll.
YOUR TWO NEXT THINGS, in this order:
 1. G-4's TREATMENT ARM. The registration is LOCKED and waiting at
    D:/mandalark-kernel/docs/experiments/CLAIM2-ROLL-IN-VERTICAL.toml -- build
    dwellLevelRateMult 0.000, NOTHING else changed, plus its smoke test. It runs AS
    WRITTEN: the attribution above does NOT re-point it (different quantity -- sign
    changes, not presence), and re-pointing a locked registration is what C3 forbids.
    G-9's fly card is written and NOT ISSUABLE until your G2/G10 go green.
 2. Then G-3: the board, 23/28 GREEN / 2 RED / 3 PENDING -- each RED and PENDING fixed,
    or ledgered as a known limit with a named owner and a date.
Answer a verdict by declaring it: put "**Answers:** PACKET-<stem>" in the reply file, in
your outbox. audit_graph could not credit NAMED packets at all until today -- both of
yours sat AMBER while answered. Fixed; the declaration is how it is credited now.
```

## → `eagle` (`D:/mandalark-kernel_sandbox_eagle`)

```
python D:/mandalark-kernel/tools/audit_graph.py
Then your row in D:/mandalark-kernel/docs/agents.tsv, then
D:/mandalark-kernel/docs/consults/V023-YAWLINE-SWEEP.md.
THREE things are yours and all three block progress:
 1. The M1 amendment. ** RETRACTED 2026-08-03: THERE IS NO SPEC BLOCKER.** This line used to
    say SPEC-AIMFF-002 must join the reversed list and was "the true blocker." That was wrong
    and it was mine: I adopted architecture's PACKET-12 sec2 pedestal-inversion INFERENCE as
    if it were Chad's ruling. His ruling was "use yaw in the cascade to assist making the line
    to the aim" -- the az_lat TERM. Moving it INSIDE the existing clamp preserves
    SPEC-AIMFF-002 verbatim and SPEC-AIMFF-A01 PASSES. Do not amend that clause.
    The M1 amendment itself is still yours (M1-PLANT-002 and S-8 reversed; M1-PLANT-010
    UNDEMONSTRATED -- do NOT write "satisfied", it is refuted by measurement).
 2. Golden re-baseline. CHAD HAS GIVEN HIS WORD ("YOU HAVE MY WORD"). Deliberate, never a
    silent re-record. tests/ is yours; V024 gave kernel-docs src/Kernel/** only.
 3. An instrument that can SEE BANK AUTHORITY. lean_gain 8/10/12/14 gives byte-identical
    BAR-SMOOTH numbers because the oscillation scenario drives roll through a keyboard
    OVERRIDE. Half of Chad's stated fix is currently unmeasurable.
DO NOT edit src/Kernel/** -- V024 moved it to kernel-docs.
```

## → `architecture` (`D:/flying_architecture`)

```
python D:/mandalark-kernel/tools/audit_graph.py
YOUR BACKLOG IS ANSWERED -- D:/mandalark-kernel/docs/consults/ARCHITECTURE-BACKLOG-6-13-VERDICT.md
covers packets 6, 7, 8, 9, 12 and 13 (packet 5 was already answered in
AGENT-PACKETS-2026-08-03-VERDICT.md). You waited on a defect in MY instrument, not on your
packets: audit_graph could only pair NUMBERED packets, so named ones could never be credited
and the amber list stopped meaning anything. Fixed -- declare a pairing by putting
"**Answers:** PACKET-<stem>" in the reply file, and your VERDICTS.tsv rows become visible too.
Highlights: CLAUDE.md's stale live-branch block is FIXED as a dated observation (your sec2 --
the forecast was the part that failed); the felt_flight ruling has LANDED in DECISIONS.md with
the Golden #1 exception; and PACKET-8's forward hazard was MET -- v2 was archived IN the v3
bump commit b606db8 (34 fields vs v3's 36), not back-filled. That is the first clean version
bump in this program and it deserves a POSITIVE control row in your store.
Still yours: G-10 -- adversarially verify the V022/V023/V024/V025 chain AS BUILT.
```

## → `harness` (`D:/mandalark-kernel`, `harness/` + `captures/`)

```
python D:/mandalark-kernel/tools/audit_graph.py
Then D:/mandalark-kernel/docs/consults/G2-RECONCILE-VERDICT.md section 2.
F1 IS YOURS AND IT IS REAL: the flown tape's header is TRUNCATED at 1022 chars and carries
no '#' terminator -- it stops mid-token at "keybit16=yaw", losing keybit32/64/128/256. Every
keyMask reading to date is correct but was sourced from the EMITTER, not the tape, which is
exactly the self-describing property check_tape H2 claims to establish. The harness guards the
terminator on ROWS only (check_emitter E5, check_tape H2) -- the one record that overflowed is
the one nobody checks. Rows are fine: max 323 chars, all terminated.
Also G-5: audit_graph's only RED is C3, which is marked EXPECTED -- ledger it with owner+date.
```

---

## Why this file exists

The eagle sat parked for two days because nothing wrote its next action where a running
session would look. **The doorbell is the fix, and it only works if someone rings it.**
Every block above resolves to a live row rather than a copy of one, so this file cannot rot
into a wrong instruction — the worst it can do is point at a row that has moved on.
