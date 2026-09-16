# HANDOFF — START HERE. R4a SUPERMAN: RULED, MEASURED, NOT YET BUILT.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws, then `docs/R4A_THROW_RULING_20260825.md`
IN FULL — it is Chad's ruling doc for this whole rung. Then §0–§2 of this file.
The next rung is PLUMBING THE FOUR PSEUDO-FORCE TERMS into `trail_chain` plus
the SEAT KEEP-OUT. Superman is the SCARF SOLVER re-anchored at the grips —
`SUDBURIAN_LADDER.md` §7.6 — and rigid-body ragdoll stays BANNED, by Chad's own
ruling this session. Do not re-open that question. Do not let the LEAN near the
self-right."*

This supersedes `docs/SESSION_HANDOFF_20260826_r4a_selfright.md`. Its §3 (the
self-right, driven and working) and §4 (the four traps) still stand and are not
repeated here. **Its §0 and §3 tape claims were WRONG and are corrected in that
file itself** — read the correction, not the original sentence.

---

## 0. THE STATE, IN THREE LINES

| branch | tip | state |
|---|---|---|
| `sandbox/r4a-phase0` | `fa94c351e` | everything below. **Committed, NOT PUSHED.** Tree clean. |
| `main` | `fa2cfe3b9` | R3-HANDS merged and driven. Unchanged this session. |

Two commits, split so only the first changes shipped behaviour:
- `a39566e71` — the tape corpus: the absent-dial trap + the 31-tape correction.
- `fa94c351e` — the superman measurement rig (probe-only).

**Gate: 1578 tests, 5 red, ALL PRE-EXISTING, verified BY NAME** (the count moves
as tests are added — ours added one — so a number comparison lies):

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

★ `sled_debug_sink_is_write_only` was **opened and checked**, not pattern-matched:
14402/14403, the single red is `REQUIRE(saw_release_under_one)` at
`test_sled.cpp:3456`, the fixture-coverage assert. Every bit-identity assert is
green. If a BIT-IDENTITY assert ever fails there, that IS a real regression —
read *which* assertion.

★★★ **THE CORPUS IS 31 REPLAYABLE TAPES, NOT 33.** See §6. Any sentence anywhere
saying "33" predates 2026-08-26 and is wrong.

---

## 1. WHAT CHAD RULED, AND WHAT LANDED

> **"the trailing chain tumble, don't override 7.6, what I meant was superman
> animations and getting thrown off all that, not ragdoll physics"**

That closes the open question the last handoff raised. There was never a real
conflict: `SUDBURIAN_LADDER.md` §7.6 is *titled* "superman IS the scarf" and
already specifies this solver with the anchor moved to the grips. **Do not ask
him again.**

> **"We have to be careful that the superman is an extreme case, and I dont
> understand why the physics dosent produce it… I disagree with the statement
> that it can produce such a force, but make it like the scarf"**

★★★ **HE WAS RIGHT AND THE LAST HANDOFF WAS SLOPPY.** The physics produces
superman fine. The INTEGRATOR is missing terms. `trail_chain_step` solves in sled
MODEL SPACE — a frame that is itself accelerating and rotating — while applying
only a uniform constant `in.gravity_dir * pr.gravity_mps2`
(`render/trail_chain.cpp`, the Verlet predict). The correct field is

```
g_eff = g − a_body − alpha × r − omega × (omega × r) − 2 omega × v
                     (Euler)     (centrifugal)         (Coriolis)
```

and **none of the four are present**. Never write "the physics can't"; write
*which term the integrator is missing*.

★ **The anchor-pin escape hatch is DEAD** (it was the one thing that could have
collapsed the whole plan): an accelerating Verlet pin genuinely does drag a
chain, but `st.p[0] = in.anchor` is a **model-space pose position** and does not
move a millimetre under a 3 g decel. So plumbing is the ONLY copy of the force —
no double-counting risk.

---

## 2. ★★★ THE NEXT RUNG — PLUMB THE FOUR TERMS, AND BOUND THE FOLD

**The measurement is done and it is unambiguous. Build, do not re-measure.**

### 2.1 What the rig proved (31 tapes, `seads_sled_probe superman`)

- **ALL FOUR TERMS ARE REQUIRED.** The three per-particle terms have a C−A median
  of `+0.00…+0.14°` on nearly every tape and a max to `+164°`: they do NOTHING in
  ordinary riding and EVERYTHING at the tail. Tape 11 is the clincher —
  `occ60A 0.00%` vs `occ60C 11.82%`. The uniform term alone is not enough.
- **SUPERMAN IS EMERGENTLY EXTREME** — Chad's requirement, and it did NOT have to
  be imposed by a threshold: `>60°` occupancy is **0.00% on all ten cruise tapes**
  and 2.0–11.8% on the bush tapes.
- **FIDELITY 0.0e+00 on all 31 tapes**: the prototype with the terms OFF is
  bit-identical to render's solver, with arm C beside it as the liveness arm.

### 2.2 The structural rules the plumbing MUST obey

Read them before writing a line; they are what keeps the corpus alive.

1. The chain may read any function of ALREADY-SHIPPED sim state. It may **not**
   cause a new `SledState` field, a `step_sled` behaviour change, or a pin-roster
   extension (**41 doubles, read by index — a 42nd refuses every tape**).
2. `a_body`/`alpha_body`/`omega_body` exist TODAY only inside `SledDebugSubstep`,
   which is filled only under `if (dbg)` and `dbg` is null in the game. **Do not
   start passing a sink in the shipped game to get them.** Finite-difference
   `velocity`/`angular_vel` render-side instead — untapeable by construction.
3. They are BODY-AXIS; the chain wants model space — one `model_from_body`
   multiply (`render/sled_model.cpp`).
4. `r` is taken about the body origin, which **IS** the system CG. The grip
   anchor is `M.grip_body` from the SHIPPED GLB — anchoring at the origin
   silently zeroes the lever arm the Euler and centrifugal terms act on.
5. Constant work survives: all four are O(1) vector ops inside the existing
   predict loop. No loop bound becomes data-dependent, so §7.6's promise and the
   banner comment above `trail_chain_step` stay true verbatim.
6. `drag_k` for a BODY is ~0.005/m, not the scarf's 0.153 (a ribbon number).
   Derived two ways — body area/mass gives 0.0037–0.0062, a skydiver's terminal
   velocity gives 0.0032. Reuse 0.153 and an 87.5 kg man streams to 54° of lift
   **at 3 m/s**.
7. **Rate-convert any damping.** See §4.1. This is the one that cost the most.

### 2.3 The seat keep-out ships in the SAME pass

Several tapes peak at **178–180°** of lift — the body folded straight up over the
bars, legs through the machine. That is past superman into nonsense, and it is
exactly what a chain with nothing to hit does. Chad's ruling §2.3 already covers
it: *"his legs will hit the seat."* A seat projection is **spec-legal** — §7.6
bans a contact SOLVER, not the analytic projections the solver already runs for
the head sphere and the back planes (`render/trail_chain.cpp`, `apply_constraints`).
Build it as a third projection in that same loop. It is what BOUNDS the extreme,
so it is not a later polish item.

★ Note `TrailChainParams`'s own comment: the head/back keep-outs "**DISABLE
cleanly for R4 superman, where the chain IS the body**" — with `head_keepout_r_m
= 0` and a zero back normal, `apply_constraints` reduces to exactly the distance
pass. Verified by reading it, and the rig relies on it.

---

## 3. WHAT IS OPEN, ALL NEEDING CHAD

- **`drag_k = 0.005/m` and the 5 × 0.35 m body** are MY derivation, not measured
  off him. They set how hard he streams. His dials.
- **The return-to-pose spring.** The chain has NO angular stiffness of any kind —
  it is a perfect flail, so inside superman it is limp. If his superman should
  FIGHT back toward the seat, that is a new term. Flagged now rather than
  discovered on his drive.
- **Do tapes 32/33 get re-driven** on today's kernel, so the self-right has
  replayable coverage? (§6.)
- **`grip_capacity_n`** — the events ladder makes ruling it a table lookup, but
  the §1.3 band omits the LATERAL load he found. Re-derive before he rules.
- **Does `grip_n` become 3D?** `grip3_n` is reported alongside, deliberately not
  folded in — folding moves every published percentile in one step.
- **`right_assist_nm = 2400`** — measured, not flown.
- **The marker's look**, from the viewpoint it is actually used from (on foot).
- **The leg-seat strike**: visual only, or does it feed the throw? Open in §2.3
  of the ruling doc.

**Nothing in this rung needs a drive yet** — the rig is measurement-only and the
game is unchanged. The first thing worth his time is the plumbing + seat keep-out
landing, and then a drive.

---

## 4. ★★★ THE TRAPS THIS SESSION PAID FOR

### 4.1 A PER-STEP DAMPING IS MEANINGLESS WITHOUT THE STEP IT WAS MEASURED AT

`damping = 0.930` is documented in `trail_chain.h` as measured at **dt = 1/120 s**.
The rig stepped the chain at the KERNEL SUBSTEP `h = 0.694 ms` — 12× faster —
while keeping the same per-step multiplier. Velocity tau ~10 ms instead of
~115 ms. It crushed the chain and produced a **confident, entirely false "no
superman"** that was one message away from being reported as a finding. Convert:
`damping^(h / (1/120))`. **Any dial expressed per-step carries its step with it.**

### 4.2 POOLED PERCENTILES DROWNED THE MECHANISM

On the ground a supported rider feels 1 g, so `a_body ≈ 0` and the arms are
**IDENTICAL BY PHYSICS**; the mechanism lives only in transients. The first run
"proved" the control arm supermans just as hard — it was measuring ROLLS
(rotating `gravity_dir` whips the chain, a path already live in shipped code) and
FREE FALL (no field ⇒ 176° of meaningless chord angle). Fix: condition on a
RIDING REGIME (upright AND field > 2 m/s²) and use a **paired per-substep
difference**. Two pooled distributions can agree while every sample differs.

### 4.3 A CONCLUSION HARD-CODED INTO A `printf`

The footer asserted that same-order omitted terms meant the uniform rig
"UNDERSTATES the shape". The four-term arm then measured the opposite on the
first tape run. **A printf that pre-judges its own result is the same trap as a
test that cannot fail.** Report what was measured; never pre-write the verdict.

### 4.4 A BIT-IDENTICAL ARM IS ONLY EVIDENCE WITH A LIVE ARM BESIDE IT

Fidelity `0.0e+00` means the prototype is faithful ONLY because arm C differs
live in the same run. Getting to zero exposed three real forks, each worth
knowing: the arms must SHARE an anchor (Verlet is chaotic — differing float
rounding grew to 1e-3 m); `needs_solve` must be mirrored (**a prime costs one
sub-step**, else the copy runs one step AHEAD); and render's `reset` LAYS OUT
STRAIGHT AND DEFERS the solve, so constraining in `reset` too ran 8 iterations
against render's 4.

### 4.5 A NUMBER QUOTED IN THE WRONG REGIME

The omitted-term maxima were first pooled over crashes — a 299 g reset spike
wearing a modelling label. A figure used to justify a modelling choice must be
measured in the regime the choice is made for.

---

## 5. HOW TO RUN THINGS

```
# THE GAME. Chad opens the EXE; build it FOR him and give the ABSOLUTE path.
cmake --build build-play --target seads
# -> D:\flight_sim2\seads-recon\build-play\seads.exe        (never build/, -O0)

# THE SUPERMAN RIG (this session's instrument)
cmake --build build --target seads_sled_probe
./build/seads_sled_probe.exe superman build-play/sled_tape_31.sledtape
#   optional: [drag_k seg_len segments]   exit 3 = refused, read WHY
#   prints: riding regime, three arms, paired A-B and C-A, occupancy, fidelity,
#           and a one-line SUMMARY for sweeping

# tape replay verdict (0 bit-exact, 1 not, 2 cannot open) -- now prints the DIAL GAP
./build/seads_sled_probe.exe tape build-play/sled_tape_31.sledtape

# the rider-load instrument + events pass (subcommand `griphold`, NOT `grip`)
./build/seads_sled_probe.exe griphold build-play/sled_tape_3.sledtape

# the gate -- ONE ctest per build dir at a time, never two (~30 min)
cmake --build build --target seads_tests && ctest --test-dir build
```

⚠ **Never rebuild while a gate is running** — the exe locks, the relink silently
fails, and you will mutation-test a stale binary.
⚠ **Catch2 filters glob against the CWD.** Quote them, and treat "no tests ran"
as a FAILURE, not a pass.
⚠ **Regenerate the graph IN THE SAME COMMIT** as any structural change
(`python tools/graph/graphify.py`; `graph_query.py check` must be green). When
splitting a change across commits, re-graphify against EACH tree — stash the
other half first, or one commit ships a graph describing code it does not carry.

---

## 6. ★★★ THE TAPE CORPUS — READ BEFORE QUOTING ANY TAPE NUMBER

**31 replayable tapes, not 33.** Proven by differential, replaying each tape
against three kernels:

| kernel | tape 31 | tape 32 | tape 33 |
|---|---|---|---|
| `2842a3350` (tape 32's recording commit) | bit-exact | **bit-exact** | won't load |
| `8e8c6f67d` (tape 33's recording commit) | bit-exact | NOT | **bit-exact** |
| `HEAD` | bit-exact | NOT | NOT |

Each tape reproduces on the kernel it was recorded on and **nowhere later**.
Nothing is corrupt. 32 and 33 are the SELF-RIGHT DEVELOPMENT DRIVES, recorded
with the mechanic LIVE (`right_assist_nm` 900 and 2400 in their own headers), and
`c31e07395` + `31e0dd96a` then changed exactly that path. Tape 31 is immune: it
names no `right_*` dial, so the mechanic is off for it on every kernel.

★ My first guess — terrain/snow cache, from the `GROUND KEY MISMATCH` wording —
was **WRONG**. The ground key desyncs as a CONSEQUENCE of the trajectory
diverging. It attributes the TICK, not the CAUSE.

★★★ **THE ABSENT-DIAL TRAP, and why it mattered more than the divergence.**
`load()` starts from a default-constructed `SledParams` and overwrites ONLY the
dials a tape NAMES, so an absent dial silently takes **today's struct default**
rather than the value in force when Chad drove — and the replay still reports
"bit-exact", because the pins were re-derived under the same wrong assumption.
Safe today ONLY by the coincidence that `right_assist_nm` defaults to 0.0. **The
first dial that ships a non-inert struct default silently rewrites the history of
all 31 tapes.**

FIXED: `Tape::dial_gap` records every comfort dial this build declares that the
tape does not name. The signal is EXACT — the writer emits the whole
`SLEDTAPE_COMFORT_D` list, so absent means did-not-exist-at-record-time. `probe
tape` prints it either way, and a clean replay with a non-empty gap now reports
**"bit-exact ON A KERNEL THIS TAPE NEVER SAW"**. Tape 31's gap is 13. Covered by
`sled_tape_dial_gap_names_exactly_the_dials_the_tape_lacks`, two arms, each
mutation-verified to kill a different stub.

**When adding a comfort dial, check its struct default is INERT** (the value that
means "off"), or every older tape silently replays a mechanic it never had.
