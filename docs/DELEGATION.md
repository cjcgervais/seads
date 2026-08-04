# DELEGATION REGISTER — what agents may do without asking Chad

**Established 2026-08-03 on Chad's word** (*"yes"*, to building it). **In force.**
**Every line below is separately strikeable.** Strike any one and it stops applying; add any
line and it starts. This document is Chad's, not the agents'. A delegation register that agents
widened for themselves would be the exact defect it exists to prevent.

**Owner:** kernel-docs. **Read by:** every agent, at session start, with `docs/agents.tsv`.

---

## 0. WHY THIS EXISTS

Chad asked whether he can stop ruling and let the build run. The honest answer was: **mostly,
but not on feel** — and that the danger of removing the principal is not that agents stall, it
is that **they start deciding in his name.** `V021` caught the kernel-docs agent doing a soft
version of exactly that on 2026-08-03, substituting content into an option after Chad had
selected it.

So the boundary gets written down instead of improvised, and the list of things that still need
him gets short, named, and **batched** into `docs/RULINGS-PENDING.md` rather than interrupting
him four times a day.

---

## 1. ✅ DELEGATED — do these without asking

1. **Build behind a default-off knob.** Any new mechanism ships with its dial defaulting to the
   existing behaviour, with a bit-identical knob-off arm demonstrated. Building is not shipping.
2. **Measure anything.** Sweeps, probes, traces, benches, fault injections. Cost is not a reason
   to ask.
3. **Refuse.** Declining to run a comparison that would be an echo failure, or to invent a number
   you cannot derive, is delegated and expected. *"I will not until you set it"* needs no
   permission.
4. **Correct yourself and each other**, including correcting the docs agent and including
   correcting a conclusion Chad has already been told. A correction is never blocked on him.
5. **Commit and push your own tree.** Durability is not a decision.
6. **Register and fix contracts** in `docs/CONTRACTS.tsv` (kernel-docs) or raise them by packet.
7. **Write packets and verdicts.** Reply, challenge, escalate, audit.
8. **Proceed under a stated reversible assumption** — see §3.
9. **Record pin/gate debt** without incurring it.

## 2. ⛔ RESERVED TO CHAD — never do these, however obvious the answer looks

1. **Answer a feel question.** How the aeroplane should feel, what trade is worth it, what
   "good" is. `M1-ACC-003`: feel is the first instrument. **This is the product, not a
   bottleneck.**
2. **Set or move a pre-registered bar value or denominator.** Setting a bar after seeing which
   arms pass is what `SPEC-ACC-004` forbids; an agent doing it is the failure this whole
   discipline exists to prevent. Propose, never apply.
3. **Change `MANDALARK1`** or any governing specification, including its supersession register.
4. **Re-record, move, or retire a golden.** `SPEC-ACC-005/006`. Supersede, never overwrite.
5. **Ship a dial as a new default** — i.e. change what Chad flies.
6. **Declare a mechanism done on feel grounds.** Gates can pass; only Chad can approve.
7. **Push to a remote that is not your own tree**, or create/delete a remote.
8. **Anything outward-facing** — `docs/KERNEL_COMS.md` entries, anything a player would read.

## 3. THE DEFAULT-CARRY RULE — the one that actually reduces the interruptions

> **If a decision is reversible, do not block on Chad. Proceed under a clearly stated
> assumption, flag it, and keep going. Block only when proceeding would be irreversible, would
> change what Chad flies, or is a feel question.**

The eagle already modelled this correctly and it is the template: it adopted `≤ 1.60 s` as a
**working target**, explicitly refused to **register** it as a gate, and said why — *"a working
target that is not a pass condition cannot be gamed for a pass, so it can be used to steer
engineering without touching `SPEC-ACC-004`."* Steering: delegated. Registering: reserved.

**Every carried assumption must be written where the next session will read it** — your ledger
and your `blocked_on`. An assumption that lives only in a session transcript is the defect
`V021` found: a decision nobody can later prove was authorised.

## 4. WHAT TO DO WHEN YOU HIT A RESERVED ITEM

**Do not stall silently. Append to `docs/RULINGS-PENDING.md`** — the question, the options, your
recommendation, and what it blocks — then **carry on with everything that does not depend on
it.** The eagle sat parked for two days because a `stop` was written somewhere no running
session would look. That must not recur.

`tools/audit_graph.py` reports the open count every run, so the queue cannot go quiet.

## 5. THE STANDING LIMIT, STATED SO IT IS NOT MISTAKEN FOR A GAP

**No agent can start another agent's session.** The loop observes, verifies, records and
sequences; it cannot press anyone's start button. Chad opens the directories. Everything above
reduces how *often* he is needed and how much is asked of him at once — it does not make the
build unattended, and no document can.
