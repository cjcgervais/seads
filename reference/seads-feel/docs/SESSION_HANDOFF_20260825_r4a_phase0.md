# HANDOFF — START HERE. R3-HANDS IS SIGNED AND PUSHED; R4a PHASE 0 IS MEASURED AND UNCOMMITTED.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws and `docs/RIDER_AUTHORITY.md`, then §0–§2
of this file. Two decisions are waiting on Chad (§2) — put them to him BEFORE you
build anything. R3-HANDS is CLOSED: do not reopen the finger squeeze, §4 proves it
is unreachable from the runtime. R4a Phase 1 opens at §3 item 1, the CASE-F fix,
and `docs/R4A_PHASE0_FINDINGS.md` §3.1 is the evidence for why."*

This file supersedes `docs/SESSION_HANDOFF_20260825_r3hands.md` as the launch
point. That file is still the record of the R3-HANDS rung and its §3/§5 are
referenced below; nothing in it is wrong.

---

## 0. THE STATE, IN THREE LINES

| branch | tip | state |
|---|---|---|
| `main` | `4be931ce7` | ★ does **NOT** contain R3-HANDS. Last change was the audio lane's church bell. |
| `sandbox/r3-hands` | `8852cf7d7` | ★ **SIGNED, COMMITTED, PUSHED** — verified on the remote with `git ls-remote`, not by the exit code. |
| `sandbox/r4a-phase0` | `8852cf7d7` + **uncommitted work** | R4a Phase 0: the instrument, built and measured. **Nothing committed, nothing pushed.** This is the checked-out branch. |

Gate on both: **1560 tests, 5 reds, ALL PRE-EXISTING**, proven by
revert-and-compare (stash, rebuild, re-run the same five **by name** — numbers
shift when tests are added, names do not):

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

★★ **`sled_debug_sink_is_write_only` is NOT a firewall failure and R4a leans on
it.** 14402 of its 14403 assertions pass, including every bit-identity check. The
one red is `REQUIRE(saw_release_under_one)` — a *non-vacuity* check that the
fixture still drives the release band. If a **bit-identity** assertion in it ever
fails, that IS a real regression. Check which assertion failed before reacting.

---

## 1. WHAT CHAD SIGNED, AND WHAT HE HAS NOT SEEN

**SIGNED 2026-08-25** — *"hey great job perfect ... This still looks good!"*:
the thumb on the throttle, the brake elbow rising +29.6 mm, the "handle twist"
deleted, and the brake fingers at the pelvis-aimed curl. Four other finger builds
were rejected on his eye and reverted; the one he pointed back to is what shipped.

**HE HAS NOT DRIVEN:** both **levers**. They were built after his sign-off on his
own instruction (*"lets reinstall their motion"*). Nothing in this repo had ever
driven either lever — squeezing the brake left the red blade dead in the air.
**Put them in front of him early.** The brake's throw is measured (19.9° to bar
contact, 48 mm of essentially pure aft travel); **the throttle's 12° is NOT
measured and is his to rule.**

---

## 2. ★★★ TWO DECISIONS WAITING ON CHAD — ASK BEFORE BUILDING

1. **Merge `sandbox/r3-hands` into `main`?** It is signed and pushed but was
   deliberately NOT merged: he was asleep, main is shared with the audio lane, and
   merging a shared branch unattended is not reversible the way pushing a branch
   is. Precedent (the scarf rung, R2c-M2) is that signed work merges to main.
2. **Does Phase 1 open with the CASE-F fix (§3 item 1)?** It is the highest-value
   item and it invalidates nothing else, but it means re-running all 27 tapes
   before any new capability is built.

---

## 3. R4a PHASE 1 — THE ORDERED LIST

**Read `docs/R4A_PHASE0_FINDINGS.md` first.** Its working is
`docs/R4A_PHASE0_MODEL.md` (the derivation) and `docs/R4A_PHASE0_DATA.md` (the
full 27-tape tables).

1. ★★★ **FIX THE CASE-F ENTRY TEST AND RE-RUN ALL 27.** `tools/sled_probe.cpp`
   uses `if (V <= 0.0) -> CASE F`, but §4.4 of the model justifies CASE F as
   *"nothing below carries"* — and those are **not the same statement**, because
   the seat is unilateral (`N_s ≥ 0`). Where a pinned-wrist solution with
   `N_s ≥ 0` still exists the model discards it and **under**-reports, measured at
   up to **572 N**, with a **jump discontinuity at `V = 0`** — which is every
   crest and every fall-away, i.e. exactly the regime this rung is about.
   ★ **Nobody re-ran the tapes with a fix, so it is UNKNOWN whether any published
   percentile in §2 of the findings moves.** Replace the test with a real
   admissibility check, re-run all 27, diff the tables. Until that diff exists,
   read those numbers as *at least* this large in the F regime.
2. ★ **Exceedance EVENTS, not percentiles.** Per tape, with hysteresis: how many
   separate crossings of a candidate capacity, and how long each lasts. A
   percentile cannot answer *"how many times per ride does it throw me"*, and that
   is the number the felt call actually needs.
3. **Then the kernel fields**, per the plan: `grip_load_n`, `grip_load_lp`,
   `seat_load_frac`, `board_load_frac`, `rider_attached`, all shipped OFF and
   bit-identical, `grip_capacity_n` at a **large finite sentinel (1e9, never
   infinity** — the pin writer refuses non-finite values and silently drops the
   whole record, and any `load/capacity` blend would go NaN). Latch on the
   **low-passed** value: `substeps` is a taped param, so a peak threshold silently
   moves when it is retuned. House pattern is `hull_engage_lp`.

**Structural findings that must survive into the build — both still true:**

4. ★★★ **Superman is impossible in the frame the solver runs in.** `trail_chain`
   is Verlet in **sled model space** (`trail_chain.h:126`), where the machine's
   linear and angular acceleration are identically zero. Only aero drag can lift
   the chain, and the flown `drag_k = 0.153/m` is a *scarf ribbon* number
   (≈0.0053 for an 87.5 kg man, **29× smaller**). It needs frame pseudo-forces
   plus `a_body`/`alpha_body` plumbed to render through `SledRig`. ★ Phase 0
   already put both on the debug record (`sim/sled.h`), which is half that
   plumbing. **Do not** run the chain in world space (float at R = 15 km) and
   **do not** re-difference velocity in render.
5. ★★ **§7.4's FIRST link does not exist.** The drivetrain needs no work — idle
   rpm, clutch decouple below 3.25 m/s, free-coast, engine brake all exist and
   no-creep falls out by arithmetic — **but nothing releases the throttle when his
   hands do.** `sim/sled.cpp:241` gates on `rolled`, not on rider attachment, and
   the spring-return is the W key.
6. ★ **"All taped" would break the corpus.** §7.7 says the new fields are taped;
   the tape's PIN roster is **positional** (41 doubles) and a 42nd makes every
   tape refuse to load. Chad ruled: **do not pin the derived fields**. Precedent
   is `ws_exch_l` (`sim/sled.h:1005`), un-pinned for this exact reason. ★ And the
   corpus is now **27 tapes, not 19**.

---

## 4. ★★★ DO NOT REOPEN: THE FINGER SQUEEZE IS UNREACHABLE FROM CODE

Four builds were rejected on Chad's eye before this was measured. `mittfront_01_l`
is a **rigid block** — 610 verts, ALL at weight exactly 1.0, zero blending —
sitting **34–104 mm FORWARD of the bar**. Mass forward of a pivot swings DOWN when
it rotates, so the complete menu of directions its tip can reach, **for any pivot
axis**, is:

```
medial  down   aft
 0.99   0.00  0.15   <- no sink, but 99% medial   (this is the SIGNED build)
 0.64   0.60  0.48
 0.08   0.85  0.53   <- most aft available, and 85% of it is down
```

There is **no direction that is aft-dominant with no sink and no medial**. A real
squeeze needs the mitt front reshaped to wrap UNDER the bar — that reopens the
R2c-M2 hands Chad hand-tuned and signed 2026-08-18, so it is an **art rung with
his eye on it**, never a fix slipped into a code change.

---

## 5. HOW TO RUN THINGS (all verified working from the repo root)

```
# the rider-load instrument, per tape  (subcommand is `griphold`, NOT `grip` --
# `grip` already exists in that file and means TYRE grip)
cmake --build build --target seads_sled_probe
./build/seads_sled_probe.exe griphold build-play/sled_tape_23.sledtape

# tape replay verdict (exit 0 = bit-exact, 1 = not, 2 = cannot open/parse)
./build/seads_sled_probe.exe tape build-play/sled_tape_16.sledtape

# the game, and the control-pose instrument
cmake --build build-play --target seads          # ★ --target seads
SEADS_SLED_DEBUG_MODE=1 SEADS_SLED_CTL_DEBUG=1 \
SEADS_SLED_RIG_SMOKE="steer,sL,sR,sT,lat,up,fwd,absorb,thr,brk" \
  ./build-play/seads.exe --smoke 110 shot.png    # fields 9/10 = throttle/brake

# the gate -- ONE ctest per build dir at a time, never two
cmake --build build --target seads_tests && ctest --test-dir build
```

★ `SEADS_SLED_CTL_DEBUG` prints a **DIFFERENTIAL** — this frame with the control
minus this frame without it. The rider rides the suspension, so a raw joint height
moves ~40 mm frame to frame at **zero** input; an absolute position cannot see
this rung's defects, and that is why one survived five prints.

---

## 6. THE TRAPS THIS WORK PAID FOR

1. ★★★ **A DELTA IS ONLY A DIRECTION IF YOU SAY WHICH FRAME IT IS IN.** A debug
   print differenced a bone's `local` alone — the *parent's* frame — and reported
   "+34 mm forward" for fingers Chad could see going skyward.
2. ★★★ **A SKINNED MESH'S NODE TRANSFORM MUST BE IGNORED** (glTF spec). Reading
   through it put the mitt's verts 1.2 m from their own bone and produced a
   26.7 mm shelf; through the inverse-bind matrices it is 8.1 mm. A "fix" was
   built on the wrong number. **Two measurements of one thing that disagree mean
   one is lying — find out which before building on either.**
3. ★★ **MIXING A REST-SPACE INPUT WITH LIVE PER-FRAME STATE.** A load-time probe
   fed `rest_world` into `solve_chain`, which reads `sm.nodes[].world` — not posed
   at load. It returned −0.6612 m, a guard refused it, and the whole branch was
   silently DEAD while appearing to work.
4. ★★ **NAMING A VECTOR "AFT" DOES NOT MAKE IT AFT.** `pelvis − grip_socket` was
   called aft; the pelvis is ON the centreline and the grip 315 mm outboard, so it
   was 315 mm of *lateral*. Measure a machine axis **same-side** and the lateral
   terms cancel by construction.
5. ★★ **A NAME THAT OUTRUNS ITS REACH.** `sled_debug_sink_is_write_only` fails on
   a fixture-coverage assert, not on the firewall. Read *which assertion* failed.
6. ★ **`indy_red` IS NOT ALWAYS THE LEVER.** On the brake it is the blade; on the
   throttle it is the **kill cap** (Chad: *"throttle is not red"*). The throttle's
   real lever shares a primitive with its housing and is separable only as a
   connected **shell**.
7. ★ **VERIFY A PUSH WITH `git ls-remote`, never the exit code** — GitHub silently
   drops an oversized POST.

---

## 7. WHAT IS UNCOMMITTED RIGHT NOW

On `sandbox/r4a-phase0`, off `8852cf7d7`:

```
M  CMakeLists.txt          seads_sled_probe links seads_render_core (+ include dir,
                           SEADS_ASSET_DIR) so the instrument re-derives the segment
                           table from the shipped GLB instead of hard-coding a CG
M  sim/sled.h              +19/-0  two fields on SledDebugSubstep only
M  sim/sled.cpp            +17/-0  one write block, entirely inside `if (dbg)`
M  tools/sled_probe.cpp    +765    the `griphold` instrument
M  generated/graph/*       regenerated in step (the standing law)
M  docs/SESSION_HANDOFF_20260825_r3hands.md   append-only §7
?? docs/R4A_PHASE0_{MODEL,DATA,FINDINGS}.md
```

Independently re-verified before this handoff was written: the sink write is
`dbg->substeps.back()` and the record for that substep is pushed at
`sim/sled.cpp:1626` while the write is at `:1656` — **same substep, no
off-by-one**; the red team's temporary audit is **not** in the tree
(`grep REDTEAM tools/sled_probe.cpp` → 0); and `sled_tape_23` reproduces to the
digit (`lp` p99.9 458.475, max 460.634, `rider_cg (+0.00033, +0.81774, −0.35734)`,
at rest **grip 0.00 N**).

`indy650.blend` and `assets/sled/indy650.glb` have **not been touched** since the
scarf rung. Everything above is runtime, tool and doc only.
