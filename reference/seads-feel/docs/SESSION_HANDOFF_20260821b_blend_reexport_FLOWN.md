# SESSION HANDOFF — THE .BLEND RE-EXPORT RUNG IS **FLOWN AND CLOSED**

**Launch line for the next agent:** *"Read `docs/SESSION_HANDOFF_20260821b_blend_reexport_FLOWN.md`; §6 says what is open."*

**This rung is closed and pushed to main — run `git log` for where main actually
is.** ★ This line first read ``main = `e362c1123` `` and was stale before anyone
read it: main moved twice while the commit was being written. Naming a hash as
"main =" is the same rot §5 warns about, committed in the doc that warns about
it. Gate at close **1516/1520** — the ONLY four reds are the
known GI4 sled debts (`sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`).
**Any new failure NAME is a finding, not a number to accept.**

Supersedes `SESSION_HANDOFF_20260821_blend_reexport.md` — that doc is the SPEC
this rung executed. Keep it for the derivations, not for the plan.

---

## 1. WHAT THIS RUNG DID

**The `.blend` is now the source of truth. The source produces what ships.**
Three defects were corrected AT SOURCE in Chad's live session (SAVED on his
ruling), and both post-passes that had been re-applying them on every export are
gone.

| defect | fixed at source by |
|---|---|
| suit + mitt shells wound inward | reversing **only the 10 negative shells**, per shell, zero vertices moved |
| helmet family 0.34 m off the head | rigid move of `(0, +0.326080, +0.079143)` m (Blender frame), derived from the artefact |
| `head_balaclava` block shipping | object DELETED (Chad's ruling) and dropped from the export `FAMILY` |

**Root cause, fixed so it cannot recur:** the helmet authoring-frame constants
were the head bone's **POSED** frame while the comment said "rest". Geometry
authored in the posed frame and skinned to `head` is deformed by
`pose @ rest^-1`. Corrected to the measured REST frame — the whole **basis**,
not just the origin (`U_UP`/`V_BACK` were provably the pose axes, so fixing the
origin alone would have left a 10° error). The preflight that should have caught
this validated the POSED bone against the authoring origin, so it agreed with
the constant precisely when the constant was wrong.

**Also landed:** the §10 reconcile (winter-gi → main, Chad pulled it forward),
and the winding measurement promoted from a script into a real ctest leg.

---

## 2. THE LAW THIS RUNG PRODUCED — read before any mesh work

**Signed volume is a winding test ONLY on a CLOSED, ORIENTABLE surface, and
ONLY per shell.** Both halves were learned by getting them wrong:

1. **Per shell, never over an assembly.** The suit totalled `+0.01176` while
   nine limb shells sat inverted underneath. A blanket flip "fixed" the prim and
   shipped see-through arms, thighs and neck for a day.
2. **Only on a closed surface.** A glTF primitive is a **material subset, not a
   shell**. `helmet_trim_black` integrates negative because it is the INNER
   liner of a closed body whose outer skin is `helmet_silver`; split apart each
   is an open patch, and an open patch's signed volume measures the ORIGIN, not
   the winding.

Method: **weld by position across primitives, decompose into shells, verify
closed + orientable by DIRECTED edges, then take the sign per shell.** Directed
edges also catch a partially-reversed shell, which an undirected boundary count
cannot. Self-intersection is still not detected — stated, not assumed.

Enforced by `test/unit/test_rider_winding.cpp`, mutation-verified (inverting the
mitt prim turns it RED) with non-vacuity floors so it cannot quietly measure
nothing later.

---

## 3. ★ THE FAILURE MODE THAT RAN THROUGH EVERY DEFECT IN THIS RUNG

**Every wrong answer was a TRUE report of a question nobody asked.** Five
instances, all green at the time:

- `assert v_after > 0` truthfully reported a positive TOTAL over nine inverted
  shells.
- A test named `"every ingested GLB has outward winding"` truthfully passed — it
  iterates the **aircraft** node specs and never touches the rider GLB. Its NAME
  was broader than its REACH, which is how the suite reported coverage it never
  had, through four separate outbreaks.
- A preflight truthfully compared the POSED bone to the authoring origin — and so
  agreed with the constant exactly when the constant was wrong.
- ctest printed four FAILED lines while the runner reported **"exited with code 0"**.
- A **1515/1519 gate** passed without ever compiling the game: `sled_model.cpp`
  builds into the `seads` target, **not** `seads_tests`. A broken build reached
  main and Chad found it by trying to fly.

**Therefore, standing rules:**
- **Build the GAME, not only the gate.** `cmake --build build-play --config RelWithDebInfo --target seads`
  before any push touching `render/`. A green suite is not evidence that the
  shipped thing builds. (`seads_tests` refuses to build outside Debug by SPEC 6.1
  — that is not an error, just build the `seads` target.)
- **Read the output, never the exit code.** An exit code is a summary someone
  else computed; it is not the measurement.
- **Check that a check's REACH matches its NAME.**

---

## 4. TWO-LANE MERGES CUT BOTH WAYS — this rung produced both, in ONE file

`render/sled_model.cpp`, same merge:

- At one anchor the two lanes' additions **conflicted**, and keeping either side
  alone would have silently dropped a whole feature (the R3-WS `WsState` struct
  vs the scarf constants). **Kept both.**
- At another they did **not** conflict and were silently **duplicated**
  (`SledModel::n_neck`, added by each lane at a different line) — breaking the
  game build with no conflict to resolve.

**Non-overlapping edits from two lanes are dangerous in both directions:** the
merge that eats work, and the merge that doubles it.

---

## 5. THE PIPELINE NOW — no hash, ever

1. `exec(open(...export_live_v13.py).read())` in the LIVE session over MCP.
2. `python assets/character/sudburian_src/splice_sudburian.py scratch/sudburian_seated.glb --ref scratch/sudburian_ref.json`

That is the whole pipeline. **Do NOT `git checkout <hash>` a base** — naming a
hash rotted the recipe twice (v9, then v13).

⚠ **Regenerating off a pre-reconcile base is GONE, not degraded.** Both
corrective scripts are deleted; an old base now yields a broken rider —
see-through limbs, helmet off the neck, block attached — **with no warning at
generation time**. Recovery starts from `d73ca430f`. Do not reintroduce a
mutation that runs by default and reports success.

**Laws that still bind:** Blender NEVER headless, ONE GUI session, the running
session is the truth, never save it without saying so (`docs/RIDER_AUTHORITY.md`).
The rider discriminator is **NOT-19**, never a fixed count — **four** separate
`== 42` pins had to be moved this rung (export, apply, splice, and the v11 gen).
If you find a fifth, move it to NOT-19; do not re-pin it to a new literal.

---

## 6. WHAT IS OPEN

- **The four GI4 sled debts** — the only reds in the gate. Pre-existing, unowned.
- **`R4-SIDE`, the `P`-key side-hang** (`docs/SUDBURIAN_LADDER.md:2109`). The
  kneel is reserved for it and its runtime gain stays 0; whoever builds it
  inherits working, tested geometry.
- Deferred: `OPEN-R3WS-STANDKNEE`, `OPEN-R3WS-STEERREACH`, `OPEN-R3WS-STANDSLEW`
  (the stand key already spends 96 % of the pose-continuity budget),
  `OPEN-KWS1-SAG`.
- **Enemy-AI ladder:** `E1.1` closure and the `E7.4` pitch limit-cycle.

**RULED CLOSED — do not reopen:**

- **The face port.** The grey head box shows through the visor opening. Chad,
  2026-08-21: *"Grey box is fine, sudburian is camera shy anyways."* **No face
  piece, ever.** Recorded because **two lanes each independently reasoned their
  way toward authoring one off an inference** — first from an inverted-trim
  hypothesis, then from the port reading light — **and the real answer was that
  nothing needed fixing.**

---

## 7. ROLLBACK

- Pre-rung `.blend`:
  `assets/character/sudburian_src/blend_snapshots/indy650_pre_reexport_20260821.blend`
  (plus Blender's own rolling `.blend1`).
- Pre-reconcile branch tip: tag `pre-reconcile-20260821` in
  `D:\seads_sandboxes\winter-gi`.
- The shipping GLB is reproducible from the `.blend` by §5 — that is the point
  of the rung.
