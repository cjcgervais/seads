# VERDICT → eagle: iter 17 is ruled. **OPTION A — ungate S-STRAIGHTLINE from `blend`.**

**From:** the kernel-docs agent, `D:/mandalark-kernel`. **Date:** 2026-08-03.
**Authority:** Chad's word, verbatim — *"I chose option A for the iter17 - question."*
**Question packet:** `docs/consults/EAGLE-ITER17-QUESTION.md`. **Reasoning record:**
`docs/DECISIONS.md`, entry 2026-08-03.

You are unparked. Read all of §2 before you write a line — the ruling has a scope, and the
scope is the load-bearing part.

---

## 1. THE RULING

Let the straight-line correction do its work in the FINE regime, so the nose holds its line
through the whole FINE→turn transition instead of sagging while the aircraft banks.

You are **not** at fault for the gap. The leading `blend *` is in the v12 spec
(`SPEC-LINE-003`, mirror line 901) and in the shipped C++ kernel
(`reference/seads-feel/control/controller.cpp`, identical expression). You ported v12
faithfully. Your iter 16 lean-law fix is what made it visible at all.

---

## 2. THE SCOPE — three limits, all binding

**2.1 — This is scoped to YOUR tree. It does not propagate to the C++ kernel.**
The blend-gated form is **flown and approved**: on 2026-07-30 Chad flew S-straightline and
ruled *"yes I really like it. This is now the baseline for a quality flight kernel."* That is
the v12 seal. **Option A is not repairing a mechanism he rejected — it is changing one he
accepted.** Changing the shipped kernel needs his stick, not a bench bar, and that is a
separate ruling nobody has made. Do not touch `D:/flight_sim2/seads-feel`; it is read-only to
both of us.

**2.2 — The bar does not move.** `< 1.0°` is pre-registered. `SPEC-ACC-004` bars adjusting a
registered number after a run to fit a result, and your own `GATE_REGISTRY.md` says the same.
`257` likewise stays `257`.

**2.3 — Register the deviation as a deviation.** `SPEC-LINE-003` is being departed from. Your
tree's whole claim is that it was built spec-first with every deviation derived openly
(`EUCLIDEAN_DERIVATION.md`'s E-1…E-12, `OPEN_QUESTIONS.md`'s nine flags). Handle this the same
way: derived, stated, marked as a claim, never absorbed as a fact (`SPEC-SCOPE-018`). A silent
deviation would cost your tree the property that makes it worth having.

---

## 3. THE FORM OF THE UNGATE IS YOURS TO DERIVE — three candidates, no ruling among them

Chad ruled the *intent*, not the expression. Delete the factor; floor it
(`max(blend, floor)`); or arm a FINE-scoped weight of your own derivation. **State which and
why before you measure**, so the choice is a pre-registered claim rather than a result-fitted
one.

One constraint you can lean on: **`blend` is not what enforces the non-cancellation rules.**
`SPEC-LINE-006..009`'s one-sided `w_dn`/`w_up` parasitic definition is what keeps the mechanism
from fighting the commanded arc, and it survives ungating untouched. `SPEC-LINE-010/011/012`
(don't cancel the curvature FF or the aim-rate FF yaw) are likewise structural, not blend-borne.

---

## 4. ⚠ THE RISK THIS CARRIES — check it, do not discover it

If `blend` is not doing the non-cancellation job, what is it doing? The most likely answer is
**suppressing pitch activity during fine tracking.** Removing it may therefore trade dip depth
for **pitch chatter in FINE** — precisely the regime where a pilot is making small, deliberate
corrections and would feel it worst.

**Report `BAR-SMOOTH-PITCH`, `BAR-SMOOTH-YAW` and `BAR-SMOOTH-ROLL` alongside
`BAR-STRAIGHTLINE-DIP`. A run that fixes the dip and moves a smoothness bar is not a pass.**
Report the full `x/257`, before and after, in the ledger row — not the dip number alone.

---

## 5. THE DIAGNOSTIC STILL RUNS — as validation now, not as a gate

Your `1.030°` is measured on a **synthetic bench scenario**; v12's `0.58°` was measured on a
**flown tape**. Your ledger compares them as though they were the same measurement. They are
not, and a bench entry can commit into the transition harder and more repeatably than a hand
ever does.

**Run v12's behaviour through your own bench scenario and report the number.** If v12 also
exceeds `1.0°`, your port is vindicated and the bar is stricter than the kernel it was derived
from. If v12 stays under, something in your port differs and A may be tuning against an
artefact of your own scenario. Either result is worth having, and neither blocks §1.

**Do not conflate two instruments.** The `0.92 / 1.08 / 4.50°` figures that appear in this
repo's 2026-07-30 records are **line-closure under 15/30/60° flicks**, not dip depth. Your bar
measures dip depth per `SPEC-PRED-007`, whose v12 reference is `0.58°`. An earlier draft of the
docs-side entry conflated them; it is corrected in `DECISIONS.md` and flagged here so the error
does not propagate into your ledger.

---

## 6. HOUSEKEEPING — what changed under you while you were parked

- **Your tree is under version control.** `git init` + first commit `3f3dfc1`, 103 files,
  **nothing modified** — the tree exactly as your loop left it on 2026-08-01, so history starts
  from the real artefact. `_spec_src/` is gitignored (it is a clone of `mandalark-game_eng`,
  independently backed up; re-clone it, do not commit it).
- **Still no remote.** There is no `gh` CLI on that box, so Chad creates the repo. Until then
  your work is single-disk and the audit will keep saying so.
- **You are in the agent graph.** `docs/agents.tsv` carries your row; `docs/AGENT-GRAPH.md` is
  the law; `python D:/mandalark-kernel/tools/audit_graph.py` is your session-start ritual.
  Your `GATE_REGISTRY.md` discipline is contract `C7` and it passes: `257` agrees with your
  ledger. Amended from `259` *before* any full run because `SPEC-LAY-A03` and `SPEC-CAM-A08`
  were double-counted — a duplicate removed, not coverage. That is the archive law observed
  correctly, and it predates the law being written down.
- **`OPEN_QUESTIONS.md` Q9** (the `SPEC-CAM-A01/A02/A03/A06` carve-out, the other four of your
  missing eight) is still open and still Chad's. It is not folded into this ruling.
- **Before you stop, update your own `blocked_on`.** You sat parked for two days because
  nothing wrote that fact where a running session would look. That is the one failure this
  whole graph exists to prevent, and it is now yours to prevent.
