# AGENT GRAPH — how the agents on this project stay out of each other's way

**Owner:** kernel-docs agent (`D:/mandalark-kernel`). **Established:** 2026-08-03, on Chad's word.
**Machine-readable half:** `docs/agents.tsv` (who), `docs/CONTRACTS.tsv` (what must agree),
`tools/audit_graph.py` (the check). This file is the law those three enforce.

This exists because Chad should not have to babysit or relay. Every rule below is written so
that an agent starting a fresh session can find out where it stands **without asking him**.

---

## 0. THE SESSION-START RITUAL — do this before anything else

```sh
python D:/mandalark-kernel/tools/audit_graph.py
```

Read-only. Touches no tree. Exit 0 = no RED, exit 1 = at least one RED. Then:

1. Read your own row in `docs/agents.tsv` — your write authority, your inbox, your `blocked_on`.
2. Poll your inbox path. Packets are dated and immutable; a reply you have not read is not a
   reply that does not exist.
3. If the audit reports a RED you own, that is your first work item, ahead of whatever you
   planned. If it reports a RED you do not own, write a packet — do not fix it.
4. Update your own `blocked_on` before you stop. **An agent that stops without recording what
   it is blocked on has made Chad the relay again.** The eagle sat parked at ledger iter 17 for
   two days because nothing wrote that fact anywhere a running session would look.

---

## 1. THE AUTHORITY RULE

> **Exactly one agent may write any given path. A change outside your authority is a packet,
> never an edit.**

Authority is declared in `agents.tsv` as `<tree>::<glob>` entries, so an agent can own paths in
a tree it is not rooted in, and two agents can root the same tree with disjoint lanes
(kernel-docs and harness both root `D:/mandalark-kernel`; `harness/**` and `captures/**` are the
harness agent's, everything else listed is kernel-docs').

**What the audit can and cannot see.** Git records *what* changed and never *which agent*
changed it. So the tool does not claim to detect authorship. It reports two things it can
actually know:

- **UNOWNED DIRTY FILE** — a modified path matching nobody's authority. Either an agent wrote
  outside its lane or `agents.tsv` is incomplete. Both are defects.
- **CONTESTED** — a modified path whose declared owner has named it in `blocked_on`. This is
  declared state, not inferred authorship, and it is the sound way to surface a cross-boundary
  edit.

That distinction is the point. A check that infers something it cannot measure is worse than no
check, because it reads as authoritative.

---

## 2. THE CONTRACT GRAPH — the edges that actually break

A **contract** is a fact that must be true in more than one file. Each has exactly ONE
**authority**; every other file holding it is a **derived copy**.

> **The authority is the file that physically does the thing.** For the EvCTAPE row contract
> that is the emitter's format string — not any schema describing it. A description that
> disagrees with the artefact is wrong by definition, however official it looks.

Every inversion of 2026-08-02/03 was one contract living in several places with no recorded
edge: the row contract across the emitter, two schema JSONs, a TSV, a fixture file and a gate
label; kernel identity across `KERNEL_SEAL`, the tape `tag=` header and hand-written verdict
stamps; gate anchors across literal row text.

`docs/CONTRACTS.tsv` names, for each one, the authority, every copy, the probe that reads each,
and the checklist a version bump must walk. A bump becomes a checklist instead of archaeology.

### 2a. The archive law (Chad-ratified, 2026-08-03)

> **Archive the outgoing version in the bump commit, reconstructed from the emitter as it
> stood — never back-filled from the current list.**

An archive is a claim about the past. Building one from present state produces a false contract
wearing a version number. This generalizes past schemas to **every past-tense record this
project keeps** — goldens, ledgers, verdicts, seals.

`C3` and `C4` in `CONTRACTS.tsv` are the same task done both ways, kept side by side on purpose:
v1 was back-filled by subtraction and over-claims by exactly two keys (19 where the true count
is 17, witnessed by `gate.py`'s own literal); v2 was archived from the emitter at the bump
commit and is correct and already load-bearing. **C3 is expected RED and must not be silenced.**

### 2c. The ruling-identity law (added 2026-08-03, earned by `V021`)

> **A ruling record must name the question document and quote the selected option's text. A bare
> option label is not an identifier.**

On 2026-08-03 Chad ruled twice on one question, saying *"I chose option A"* and then *"I went
with 2."* `A/B/C/D` came from `EAGLE-ITER17-QUESTION.md` §4; `1/2/3/4` came from the eagle's own
question built on `ITER17-MEASUREMENT.md`. **Two enumerations, two documents, one topic.** A
record carrying only the letter cannot be resolved later, and a supersession pointing at a bare
label retires whatever the reader assumes it meant.

Two corollaries, both drawn from the same incident:

- **Supersede in parts, not wholesale.** `V020` lost its option choice and its propagation
  clause but kept its control-arm warning. A bare pointer would have killed the warning by
  implication — the opposite of what the new ruling intended.
- **Record the disclosure, not only the decision.** When an agent advises a change to an
  option's *content* before the principal selects it, the record must say so. An informed
  selection whose record omits the disclosure is **indistinguishable from an agent substituting
  its own judgement after the fact**, and an auditor reading only the tree will correctly flag
  it as the latter. This is not paperwork: it is the difference between Chad ruling and an agent
  ruling in his name.

### 2b. The inert-check law

> **A check that stops matching is not a passing check.**

On the v3 bump, `G21`'s five emitter-mutation anchors and `check_tape`'s constant-column check
both went inert. One went RED because a `count(old) != 1` guard caught it; the other stayed
GREEN, because a skipped check is not a failed one — `0/6` gating checks failed became `0/5`.
Only the guard is acceptable. Every probe in `CONTRACTS.tsv` reports `PROBE-FAILED` as **RED**
when its anchor goes stale, and `audit_graph.py` is held to the same standard it enforces.

---

## 3. THE CHANNEL — no relaying

`docs/consults/` in this repo is the channel. Convention: `<TOPIC>-PACKET.md` inbound,
`<TOPIC>-VERDICT.md` outbound.

- **Write packets in your own tree, then land them here.** Do not switch to a rolling status
  doc: a packet is dated, immutable and citable; a rolling doc silently loses its own history,
  which is the same defect class as a mutable pinned tape.
- **Poll, don't wait.** Every agent has read access to every other tree. Chad's only job is to
  start sessions. He never carries content.
- **`docs/KERNEL_COMS.md` does not accept agent traffic.** It is Chad's customer-facing intent
  ledger, his voice, entries on his word only.

---

## 4. WHAT THE GRAPH IS NOT

- It is not a permission system. Nothing enforces authority at write time; the audit reports
  after the fact. The rule is honored by agents reading it, which is why it is short.
- It is not a substitute for Chad's rulings. `blocked_on` entries naming him are **his queue**,
  surfaced automatically so he can see it without being asked.
- It does not make an agent's claim true. The graph tracks whether copies agree — never whether
  the authority itself is right. That is still flying, still reconcile, still Chad's stick.

---

## 5. CURRENT STANDING ITEMS (2026-08-03, from the first audit run)

| item | owner | state |
|---|---|---|
| `D:/mandalark-kernel_sandbox_eagle` has **no version control at all** — 26 kernel modules, a 257-gate ladder, no history, no remote | Chad | RED, largest single risk on the board |
| Eagle **parked since 2026-08-01** at ledger iter 17 (249/257): S-STRAIGHTLINE is gated on `blend`, so it is OFF in FINE where the lean law banks; dip 0.645 → 1.030° against a 1.0° bound | Chad | needs a ruling |
| `D:/mandalark-cascade-research` has **no remote** and holds the `gate.py` that actually executes | Chad / cascade-recorder | RED |
| `cascade/rebuild` branch has **no upstream** — the whole EvCTAPE-v3 build is local only | cascade-recorder | RED |
| `harness/TAPE-SCHEMA.tsv` bumped to v3 by cascade-recorder inside the harness agent's lane | Chad | CONTESTED — awaiting an ownership ruling |
| `EvCTAPE-v1.json` archive over-claims 19 keys vs a true 17 | harness | RED, C3, deliberately visible |
| 17 unpushed commits in `D:/mandalark-kernel` | Chad | AMBER |
