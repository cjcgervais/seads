# DOORBELLS — paste one line into each agent's session

**Chad, 2026-08-03:** *"sEND A DORBELL TO THE OTHER AGENTS SO THEY CAN DO THEIR WORK TOO"*

Each block below is **one paste**. It is a **pointer, not content** — nothing can go stale in
transit, because every agent reads its own current row on arrival. Chad's only job is the paste.

---

## → `cascade-recorder` (`D:/EvC2026_sandbox_cascade`)

```
python D:/mandalark-kernel/tools/audit_graph.py
Read D:/mandalark-kernel/docs/GOAL-2026-08-03.md section 0 -- the MAIN DRIVE changed today.
Your job: analyse the buttery question the way v12's success was analysed. Chad's words:
"MAKE THE RECOREDER TO THE WORK PROERLY TO ANAYSE IT JUST LIKE THE SUCCESS WE HAD FOR V12".
Concretely: EvCTAPE has no smoothness instrument. v12's is BAR-SMOOTH-* -- body-rate
FULL-REVERSAL RATE per axis over a manoeuvring run, bound < 1.1 /s, v12 measured
PITCH 0.80 / YAW 0.91 / ROLL 1.03. Build that statistic over a real tape. Then your row.
```

## → `eagle` (`D:/mandalark-kernel_sandbox_eagle`)

```
python D:/mandalark-kernel/tools/audit_graph.py
Then your row in D:/mandalark-kernel/docs/agents.tsv, then
D:/mandalark-kernel/docs/consults/V023-YAWLINE-SWEEP.md.
THREE things are yours and all three block progress:
 1. The M1 amendment -- and SPEC-AIMFF-002 must join the reversed list. It encodes
    coordination-over-pointing, which is exactly what Chad reversed. SPEC-AIMFF-A01 now
    fails at ANY nonzero yaw_line_gain. This is the true blocker.
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
Then D:/mandalark-kernel/docs/consults/V023-YAWLINE-SWEEP.md, both addenda.
Two things:
 1. G-10: adversarially verify the V022/V023/V024/V025 chain AS BUILT. One of your own
    conclusions is already refuted by measurement -- PACKET-12 sec6 and PACKET-13 sec5 both
    said M1-PLANT-010 was SATISFIED. It is not: the dip is DEEPER (-1.066 -> -1.377 deg) and
    the bar went blind. kernel-docs propagated that error too and has retracted it.
 2. audit_graph now scans the packet channel and reports YOUR backlog: PACKET-3,4,5,6,7,8,9
    unanswered, and 12/13 answered only inside a sweep doc, which the pairing cannot credit.
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
