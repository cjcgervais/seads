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
| `mandalark-kernel` | `main` | see git | pushed, clean |
| `seads-feel` | `feel/kernel-v5` | `39aa52a48` | **UNPUSHED (ahead 6)**. Seal `flight-kernel-v10-2026-07-30` @ `f86ee7b9f` stands; tip carries **S-ROLLMIX (mechanism code, AWAITING CHAD'S FLY)** — `[regime] roll_target_mix` (0.0 = bit-identical v10), telem.blend/held_bank + recorder v2, controller golden deliberately re-recorded, gate 395/395. **Reference snapshot is now BEHIND the live tree's code** — live tree is ground truth until the next flown seal + re-snapshot |
| `seads-recon` | `sandbox/kernel-v5-reconcile` | see git | **graft of the sealed v10 state was in flight 2026-07-30** — verify before treating recon as flying the committed seal (it flew the byte-matched logged flip before that) |

Kernel seals: v5 → v6 (`cfe1bd7fe`) → v7 (`51eb5b9e3`) → v8 (`ae7ae8f23`, pre-fly,
partially rejected) → v9 (`29787debc`, camera arc closed) → **v10 (`f86ee7b9f`,
buttery cascade — pool-ball capture retired-parked)**. `reference/seads-feel/` is
snapshotted at the **v10** seal.

## 3. THE CAMERA ARC — CLOSED 2026-07-29 night (v9 flown-approved)

**v9 "S-nosesnap" is sealed, flown, approved, and grafted — the fourth attempt succeeded.**
The record of what was ruled, measured, and why is in `docs/DECISIONS.md` (read the five
2026-07-29 entries newest-first). The section below is kept as it stood during the work,
because the rulings in it remain load-bearing — but note the handoff quote was CORRECTED
(the aim never snaps; freelook welds aim to nose; the release snap is instant and upright).

### Chad's model — load-bearing, quote it, never paraphrase it

> **2026-07-29 (later) CORRECTION — read the "CHAD CORRECTS THE v9 LAW QUOTE" entry in
> `docs/DECISIONS.md` before quoting anything here.** Chad retracted the "I snap the camera
> and the aim" phrasing: **the aim never snaps** (freelook nests aim to nose; release moves
> only the camera, deterministically), and **the release snap includes upright relative to
> the earth** — which pre-answers the v9 Step 2B question with a ruling. Points 1–4 below
> still stand.

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

**2026-07-30 — v10 SEALED; pipeline ran; GOLDEN #4 IS THE OPEN ITEM.** Sealed
`flight-kernel-v10-2026-07-30` @ `f86ee7b9f` (docs tip `2be93007c`, gate 391/391, tag
verified by this agent). This agent's seal pipeline is DONE: `reference/seads-feel/`
re-snapshotted at the seal (README states the one deliberate exception — the v10 fly card
from the docs tip); DECISIONS.md carries the v10 entry (cue-ball park verbatim + re-entry
condition, Fly-13 supersession, the compensation-decay law with the A6M2 predictive note,
lesson 8 read-the-flown-table); VERSIONS.md carries the seal row. **GOLDEN FELT FLIGHT #4: FLOWN AND SEALED 2026-07-30** (`golden_4_*`, the v10-seal
flight — see VERSIONS.md). Headline findings: the knife edge HELD at both steep-down
entries (Chad: "I couldn't produce a roll over"), the smoothness predicate is pinned
(body-rate reversal < 1.1/s per axis in the stated slow-window instrument), and the
horizon-gate before-state was captured incidentally at t=55.7 s in its exact documented
geometry (~22° above horizon → partial bank-over, min cpt −0.554). **Sealing lessons paid
for:** recorder.h's fnv1a uses VARIANT constants (offset `1469598103934665603`, prime
`1099511628257`) — validate any signature reimplementation against a sealed golden before
trusting a STOP (this session's first recompute mismatched everything and was correctly
diagnosed as implementation error, not tampering). File counter ≠ golden number
(felt_flight_18 = Golden #4; felt_flight_17 set aside "no air", sig-verified, unpromoted,
SHA in the VERDICT). **ONE active mechanism thread**, now LANDED AS CODE AND AWAITING CHAD'S FLY: the
5–10° blend-boundary roll slam → **S-rollmix** (`docs/rollmix_fly_card.md` in the live
tree; consult 45f9737, audit e606070, both on this ledger; fly card cleared by this agent
2026-07-30 — all three audit corrections verified in the live tree, recverify sig_ok=1 on
all four canonical goldens recorded in commit 063b4d27d). Rejection disposition:
revert-to-branch, never parked at zero. After approval: recon graft incl. recorder-v2
port, then build-play re-stamp. The horizon-gate thread is **PARKED-PENDING-RECURRENCE**
(live ledger `cdfa7b0b4`, mirrored in DECISIONS.md: the symptom did not reproduce on the
sealed tape — do not design against it; #4's clean segment B is the ready-made A/B, and
re-opened fix work should request a dedicated recording set).

| item | owner | state |
|---|---|---|
| **v9** | — | **DONE**: flown-approved, sealed (`flight-kernel-v9-2026-07-29` @ `29787debc`), pushed, grafted to recon (901/901). CQ2 window KEPT by ruling; Card-1 trade accepted. See DECISIONS.md |
| **Re-snapshot `reference/seads-feel/`** | — | **DONE** at the v9 seal, 2026-07-29 night |
| **Golden Felt Flight #3** | — | **FLOWN AND SEALED 2026-07-29** (`golden_3_*`, the v9-seal flight — 20 releases, 2 while inverted, combat + crash + respawn). See VERSIONS.md |
| **v10 idea (DEFERRED by Chad)** — autolevel after mouse-only aerobatics; "spacebar cures all… I will defer for now" | Chad | parked, his word; recorded in golden_3_VERDICT.md and DECISIONS ledger context |
| **Recon `KERNEL_SEAL` doc comments** — uncommitted WIP in the recon tree (comments only) | seads-feel agent | should be committed |
| **Fly card 3 (v8)** — does the aim-bound camera still let Chad read a deflection shot? | Chad | open — Card-1 v9 acceptance is adjacent but this gunnery question was left open at the v8 seal |
| **Golden Felt Flight #3** | you (goldens are yours) | not flown. Needs a clean tree; `KERNEL_SEAL` + build-info now track the build |
| **Re-snapshot `reference/seads-feel/`** | you | held until v9 flies — see §5 |
| **Mouse-aim `turnsteady` 95.82°** | Chad | never asked; probably a *feature* (deflection view) |

## 5. Standing decisions that are easy to get wrong

- **Never re-snapshot `reference/` at an unflown seal.** (This is why v8 was never
  snapshotted. v9 is a FLOWN seal — snapshotted 2026-07-29 night.)
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
7. **Identify artifacts from their own data, never from another agent's echo.** The
   seads-feel agent's Golden #3 echo named `felt_flight_3` as the capture; its own stated
   countables (≥10 releases, terminal ground contact) failed against that file and passed
   against `felt_flight_4`. The guard had already sealed the right file because
   identification was derived from release edges and the crash profile before the echo
   arrived. A cross-agent handoff is a claim with the same standing as a plan's attribution
   — reconcile it against the artifact before acting on it. (Same night, same principle:
   the recording header's `kernel=` string was wrong too — stale `KERNEL_SEAL` at build
   time. Headers are claims; signatures and pins are facts.)

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
