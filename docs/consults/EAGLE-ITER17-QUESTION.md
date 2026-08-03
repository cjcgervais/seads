# THE EAGLE'S ITER-17 QUESTION — stated for Chad

**From:** the kernel-docs agent. **Date:** 2026-08-03. **For:** Chad's ruling.
**Occasion:** Chad said *"I dont know the question to rule on."* Fair — the ledger row states a
symptom, not a question. This states the question.

Everything below is **measured in the trees today** except where marked INFERRED.

---

## 1. FEEL — what this is, in the cockpit

You are tracking a target with small stick corrections — the fine, settled regime, wings
near level. You ask for a slightly bigger correction: enough that the aircraft decides this
is a real turn, not a nudge.

At that exact moment the aircraft **banks into it immediately** — which is what you asked
for, verbatim, and it is written into the spec as your words:

> **"nose should crab immediately and bank immediately."** — SPEC-BTT-017

But the mechanism that keeps the nose *on the straight line* while it banks is **switched off
in the fine regime and only fades in as the turn commits.** So for the first fraction of a
second of that transition, the plane banks with nothing holding the nose up, and the nose
**dips about 1.03°** before the correction catches up and recovers it.

It is a small, brief sag at the moment a fine correction becomes a turn. It self-recovers.

---

## 2. WHY THIS IS NOT AN EAGLE BUG — and this is the part that matters

**The eagle implemented the spec exactly.** The gating is in the v12 specification itself:

```
SPEC-LINE-003, TESGI-SEADS-KERNEL-SPEC-v12-MIRROR.md:901
    pitch += blend * fwd_gate_ff * knife_fade * line_hold_ff * w_axis
```

`blend = smoothstep(blend_lo, blend_hi, err)` — it is 0 in FINE and 1 in a committed turn.
That leading `blend *` is what makes the straight-line correction structurally zero in FINE.

**And the real C++ kernel you fly carries the identical line.** Measured in this repo's
snapshot at `reference/seads-feel/control/controller.cpp`:

```cpp
    pitch += blend * fwd_gate_ff * knife_fade *
             cp.line_hold_ff * w_axis;
```

So the eagle did not introduce this. It reproduced v12 faithfully, and in doing so it put a
light on a hole that **is in the kernel you are flying right now.** The eagle only saw it
because iter 16 fixed the lean law — before that, `held_bank` never left 0, the aircraft
tracked small offsets flat on rudder, and nothing banked in FINE to expose the gap.

That is the eagle earning its keep: a spec-first rebuild found a defect in the spec.

---

## 3. THE ONE THING THAT MUST BE MEASURED BEFORE YOU RULE

**I recommend you do not rule yet, because two different questions produce this same number,
and one measurement separates them.**

The bar's own predicate (`tests/gates/Bars.luau`, SPEC-PRED-007) measures the dip starting at
the tick where **`blend` leaves 0** — i.e. it deliberately measures the FINE→turn transition,
the exact window where the correction is weakest by construction.

- **v12's `0.58°`** was measured on **a flown tape** — your hand, your stick.
- **The eagle's `1.030°`** is measured on **a synthetic bench scenario.**

**These are not the same measurement, and the eagle's ledger compares them as if they were.**
A bench scenario can enter the transition harder and more repeatably than a human hand ever
does. The gap may be the mechanism, or it may be the entry aggressiveness.

**The question the eagle should answer first (INFERRED that it can, from its probe tooling):
does the C++ v12 kernel, run through the eagle's own bench scenario, also exceed 1.0°?**

- **If v12 also exceeds it** → the bar is being applied outside the regime it was derived for,
  the eagle's port is correct, and what you are ruling on is a **real feel defect in the
  shipped kernel** — see §4.
- **If v12 stays under** → the eagle differs from v12 somewhere real, and it is a port defect
  to hunt, not a design decision for you at all.

---

## 4. WHAT YOU WOULD BE RULING ON, IF IT IS THE FIRST CASE

| option | what it means | cost |
|---|---|---|
| **A. Ungate it** — let the straight-line correction work in FINE too | The nose holds its line through the whole transition. A deliberate, documented deviation from v12 spec, which would then propagate to the C++ kernel | Needs re-derivation; the `blend *` was presumably there for a reason nobody has written down. Touches the kernel you fly |
| **B. Scope the bar to the regime** — the `< 1.0°` bound applies only where the mechanism is armed; register the FINE-entry dip as a known limit | Honest and cheap | The feel defect stays in. You would be flying a documented sag |
| **C. Fade the lean law in instead** — don't bank until the correction is live | Removes the gap from the other side | Contradicts SPEC-BTT-017 — your own verbatim "bank immediately" |
| **D. Accept 1.030° and move the bound** | — | **Do not.** The bar is pre-registered; moving it after a run to fit a result is exactly what SPEC-ACC-004 forbids, and it is the discipline that makes every other number here worth reading |

**My read, not a ruling:** this is your "control is king" doctrine's own test case. `COMS-1`
promises *"your nose flies a straight line to where you aim"*, and you have already ruled once
this year — the 2026-07-30 turn-entry dip — that a measurable trajectory quirk **is a defect
because it taxes the pilot's attention.** That ruling is directly on point, and it points at A.
But A changes the shipped kernel, so it wants the §3 measurement under it first.

---

## 5. THE OTHER HALF OF THE ITER-17 STOP — and this one is not a ruling

The eagle stopped at **249/257**, not at 257. Its `GATE_REGISTRY.md` discipline holds: the
denominator is pre-registered, and a run with fewer gates passing is a FAIL, never a pass on a
smaller denominator. Four of the remaining gates are the `SPEC-CAM-A01/A02/A03/A06` camera
carve-out, already logged as `OPEN_QUESTIONS.md Q9` and awaiting your word separately.

**Status of the tree itself: fixed.** It was under no version control at all — 26 kernel
modules and a 257-gate ladder with no history. `git init` + first commit landed 2026-08-03 on
your word (`3f3dfc1`, 103 files, unmodified). **It still has no remote**, because there is no
`gh` CLI on this box: create `github.com/cjcgervais/mandalark-kernel-sandbox-eagle` (private)
and I will add the remote and push.
