# SESSION HANDOFF — kernel-docs agent

Written 2026-07-29 for a fresh instance. Read this after `CLAUDE.md`. It is a **state of
play**, not history — the reasoning lives in `docs/DECISIONS.md`, which is authoritative.

---

## 1. Who you are and what you may touch

You are Chad's **kernel-docs agent** for `D:\mandalark-kernel`. A **separate agent** works in
`D:\flight_sim2\seads-feel` (the live kernel) and `D:\flight_sim2\seads-recon` (the game tree).
You two hand off through Chad, and through `docs/*_handoff.md` files in the live tree.

- **`goldens/` is YOURS** as of 2026-07-29 (Chad's ruling — see `CLAUDE.md`, which carries the
  full sealing checklist). `harness/` and `captures/` are **not**.
- **The live trees are READ-ONLY to you.** Read them freely (`git log`, `git status`, `cat`,
  `grep`) — that is expected and necessary — but never write, never commit there.
- **Check the live branch state at the start of every session.** It moves, sometimes mid-task.

## 2. Current state (verify before trusting — these are 2026-07-29 snapshots)

| repo | branch | tip | note |
|---|---|---|---|
| `mandalark-kernel` | `main` | `433038d` | pushed, clean |
| `seads-feel` | `feel/kernel-v5` | `e23653042` | clean. Seal `flight-kernel-v8-2026-07-29` = `ae7ae8f23`; tip is docs-only past it |
| `seads-recon` | `sandbox/kernel-v5-reconcile` | `cdf65753d` | clean, **flying v7** — v8 is NOT grafted |

Kernel seals: v5 → v6 (`cfe1bd7fe`) → v7 (`51eb5b9e3`) → **v8 (`ae7ae8f23`)**.
`reference/seads-feel/` is snapshotted at the **v7** seal, deliberately — see §5.

## 3. THE ACTIVE WORK — v9, the camera arc

**v8 is partially rejected on the stick and v9 is being built by the seads-feel agent right
now.** This is the fourth attempt at the same camera behaviour. Do not treat any of it as
settled.

### Chad's model — load-bearing, quote it, never paraphrase it

1. Release freelook → camera snaps to chase, **directly behind the plane**. Every time. Keys
   held or not; **keys are irrelevant to this**.
2. After that snap: mouse-aim with the ordinary lag camera.
3. **Keys never touch the camera. Ever.** (v8 achieved this — it must be preserved.)
4. The mouse activates nothing; it moves the aim, the camera lags it.

**One snap at release, then default lag.** No latch, no sustain, no key-triggered mode. A
previous instance repeatedly proposed a "sustain mechanism"; Chad rejected it twice. **It is
dropped. Do not reintroduce it without a new ruling.**

His sharpest formulation, which belongs in SPEC verbatim: the release *"doesn't change the aim,
just changes how the aim is controlled."*

### What v9 is fixing, and the two rival causes

At a freelook release with keys held, Chad sees *"an angled view from across the loop manoeuvre
at an oblique top down view"* instead of chase.

**The decision rule is PRE-REGISTERED in `docs/DECISIONS.md` (commit `433038d`) — written before
the numbers exist so the result cannot be rationalised.** Grade the v9 run against it. Summary:

- **A — forward term.** `orient_snap_dir` returns the **velocity**, so the release yanks the aim
  off the nose. Bounded ~20° by `[aoa] aoa_max = 20.0`.
- **B — up term.** `orient_fired` cuts `cam_fwd` but **never touches `cam_up`**, which carries
  loop holonomy while the eye is *lifted along up*. Up to 180°, righting only over ~1–1.6 s.
- **Rule: the dominant term is the term that gets fixed.** A threshold on A alone is explicitly
  rejected — ~18° reads as confirming A while B sits at 120°.
- A and B are **coupled**: S7-hrz captures its debt about the *post-snap* forward, so changing A
  changes B. Re-measure.

**Chad's own insight, verified and important:** during freelook+keys the §5b nesting already
does `aim := nose` every tick, so at the release instant the aim is **already on the nose**. The
fix therefore *removes* a spurious ~20° aim jump at the control handover rather than adding one.
**Verified: no golden exercises freelook** (`controller_golden.h` / `golden_flight.h` don't
mention it), so this cannot move a golden — despite touching `ci.target_dir_world`, a control
input.

## 4. Open items

| item | owner | state |
|---|---|---|
| **v9 implementation** | seads-feel agent | in progress; plan approved with 2 amendments |
| **Fly card 3 (v8)** — does the aim-bound camera still let Chad read a deflection shot? | Chad | **open, independent of v9** |
| **Graft v8/v9 to seads-recon** | seads-feel agent | blocked until v9 flies |
| **Golden Felt Flight #3** | you (goldens are yours) | not flown. Needs a clean tree; `KERNEL_SEAL` + build-info now track the build |
| **Re-snapshot `reference/seads-feel/`** | you | held until v9 flies — see §5 |
| **Mouse-aim `turnsteady` 95.82°** | Chad | never asked; probably a *feature* (deflection view) |

## 5. Standing decisions that are easy to get wrong

- **Never re-snapshot `reference/` at an unflown seal.** v8's walk-back is a *revert*, not a
  dial, so a snapshot could enshrine reverted code. It stays at v7 until Chad flies.
- **Goldens are append-only in practice.** Never re-derive, re-record or tidy one. Supersede,
  never overwrite. **No golden supersedes another** (#1 = slow/dirty/ground; #2 = fast/clean/air).
- **Never edit a recording's header**, even a wrong one — the fnv1a signature covers it.
  Corrections go in the `_VERDICT.md` signing metadata.
- **If a controller golden moves: STOP.** Never re-record — a re-record blesses the bug.
- **`main.cpp`'s `ease_chase_forward` call must stay branch-free on key state.** A conditional
  there is a ruling violation on sight. No test can catch it (no ctest runs `seads.exe`), which
  is why it is a review bar.

## 6. Lessons this project paid for — apply them

1. **Audit the ATTRIBUTION before the implementation.** The v8 plan claimed "retiring D9 fixed
   the original complaint"; the ledger said the opposite in a heading. Three audit rounds found
   real implementation defects and none checked the premise the whole plan rested on. That cost a
   full kernel version. *A plan's stated cause is a claim, not context.*
2. **A feel mechanism's instrument must model BOTH HANDS at once.** Four camera mechanisms in a
   row were validated against sequences Chad doesn't fly (`turnsteady_keys` omits the mouse;
   `mouseaim_keys` omits freelook; neither models a release).
3. **Fly cards must state DURATION, not firing.** v8's card said "the snap still fires" — a
   tick-level fact that a kernel with the exact defect passes, and did.
4. **State expected gate counts in advance.** "Gate green" with an unstated count hides a
   silently lost test.
5. **Recover the predicate, don't restate the number.** Golden #2's inversion count only
   reproduced at `dot(body_up, local_up) < -0.5` with a ≥0.25 s dwell. An unstated definition
   reads as a regression later.
6. **Never bulk-edit `docs/flight-log.md`** in the live tree — a blind replace once falsely
   marked 15 pending rows as flown-approved.

## 7. How Chad works, and what he needs from you

He is a non-coder designer with excellent flight-feel intuition. He steers by flying and
reporting sensation. **His words are the specification** — capture them verbatim; do not
translate them into mechanism language and then reason from your translation. That error caused
the "gun-line" confusion (a term *this agent* invented and then had to unpick).

**When he says something is clear, it usually is — the failure is normally in the questions.**
He has pushed back on planning questions twice. Ask only what is genuinely a feel choice he
alone can make, present it in cockpit language with numbers attached, and never ask him to
choose between mechanism designs.

**Own errors plainly and once**, correct the record where it lives, and move on. This repo's
docs carry several such corrections; that is the intended standard, not a failure mode.
