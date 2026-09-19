# SENTINEL PRE-AUDIT PACKET -- terrain-clip, tip `15c15acae`

Lane `sandbox/terrain-clip`, worktree `D:\seads_sandboxes\terrain-clip`. Every number below is read
from a committed file, from `git`, or from a command run in this session; none is recalled. This
packet SUPERSEDES `PREAUDIT_PACKET_b58a8a52d.md`, whose §9 stopped step 6 because main had moved
with code. **That merge is now made, and the tree is re-gated from clean.**

---

## 1. Tree

Tip `15c15acae466ea850cc962f75d71032f464a3c92`. The lane, newest first:

| commit | what it is |
|---|---|
| `15c15acae` | the POST-MERGE gate verdict (docs only) |
| `fc02f8873` | his tag ruling + his recon word (docs only) |
| `29acc1182` | the kernel-class landing docs (docs only) -- **the tree the gate was built from** |
| `f7ccbfa30` | **the merge of `origin/main` `ed0a43ce8`** -- the CODE tip |
| `d9ee51b6a` | the previous pre-audit packet |
| `b58a8a52d` | the red-team record |
| `e8f70fb0e`, `dba79e46b` | the two PRE-MERGE gate verdicts (their 2130 count is now VOID) |
| `a18fc9b1c` | T2c, red-team fold 2 -- the previous CODE tip |

`git diff --stat 29acc1182 15c15acae -- sim control app render world test config CMakeLists.txt`
prints **nothing**: the three docs commits on top of the gated tree touch no compiled file, so this
packet's gate verdict describes the tip's code exactly, not approximately.

## 2. The merge, and how each conflict was resolved

`f7ccbfa30`, ONE merge of `origin/main` `ed0a43ce8` (the whole road-repair lane landing, 186 files).

**Conflicted, 5 files, ALL in `generated/graph/`** -- `graph.json` and `digest/{app,config,test,world}.md`.
**REGENERATED, never hand-resolved and never `--ours`:** `python tools/graph/graphify.py` ->
`graph.json: 412 files, 1434 include edges, 1704 symbols, 12 module digests`, in the same commit that
moved the code it describes (memory `no-guessing-graph-and-spec`).

**Auto-merged, both sides kept, verified by eye afterwards:**
* `app/main.cpp` -- road-repair's FLASH INSTRUMENT (the `app/flash_cam.h` include, `flash_smoke_armed`
  / `flash_env_d` / `apply_flash_smoke_env`, the `SEADS_FLASH_*` reads) **and** this lane's T2
  injection (`env.ground_params.facet_contact`, the `SEADS_FACET_CONTACT` strtod kill, the
  `ground_facet_fn` lambda over `render::facet_radius_at`, and the `[config] ground:` banner). Both
  blocks are present; neither lane's was dropped.
* `LANES.toml` -- both lane blocks, union. `CMakeLists.txt` -- both lanes' test translation units.

The two deltas do not collide: main's new code touches **no** `sim/` or `control/` file.

## 3. Kernel proof, post-merge, VERBATIM

`git diff --stat origin/main HEAD -- sim control config/controller.toml`:
```
 sim/environment.h |  50 +++++++++++++++++++++++++
 sim/ground.h      | 108 +++++++++++++++++++++++++++++++++++++++++++++++++++---
 sim/step.cpp      |  14 ++++++-
 3 files changed, 164 insertions(+), 8 deletions(-)
```
EXACTLY three files, and nothing in `control/` or `config/controller.toml`.
`git diff --stat origin/main HEAD -- control config/controller.toml` prints **0 bytes** -- the
controller firewall is proved, not asserted.

`git diff --stat origin/main HEAD -- test/harness` (the tape header fields):
```
 test/harness/feel_tape.h | 75 ++++++++++++++++++++++++++++++++++++++++++++++++
 1 file changed, 75 insertions(+)
```
Additive only: `has_facet_contact()`, `facet_contact_or(double)`, `facet_injected()` and
`warn_if_facet_mismatch(...)`, which read the `[config] ground: facet_contact %.2f (injected: %s)`
banner line the tape already carries and **warn, never throw**, when a tape was flown on a different
crash surface than the one replaying it.

NOT empty **by design**: T2 is a one-dial rung, `[ground] facet_contact` (shipped 1.0, 0 = identity),
and that is the whole delta. **Identity BY BRANCH, not by tolerance:** at 0 the facet function is not
called at all -- `FACETCONTACT L1 identity BY BRANCH: at 0 the facet fn is not called` proves it with
a counting stub; `L1 identity: a full ground_contact tick is unmoved at 0` proves the tick; `L8 the
prism base follows the facet through sim::step` proves the SHIPPED path carries it, not the test path.

## 4. Hygiene, all post-merge

| check | result |
|---|---|
| `python tools/gate/lanes_lint.py` | `OK -- no comment is orphaned at the foot of any lane block.`, exit **0** |
| `git ls-files --eol \| grep -c 'w/crlf'` | **0** |
| `python tools/graph/graph_query.py check` | `layer check OK`, exit **0** |
| `tomllib.load(open('LANES.toml','rb'))` | parses, **15** lanes |
| `git status --short` | **empty** -- nothing uncommitted, nothing stray |

## 5. Legs

`ctest --test-dir build -C Debug -N | tail -1` -> **Total Tests: 2137** = road-repair main
`ed0a43ce8`'s **2123** + this lane's **14**. The fourteen, by name and by ctest number:

`#244` FACETCONTACT L1 dial: shipped 1.0, bounded [0,1], read by the app ·
`#245` L1 identity BY BRANCH: at 0 the facet fn is not called ·
`#246` L1 identity: a full ground_contact tick is unmoved at 0 ·
`#247` L2 an airframe inside the drawn ground is graded at 1.0 ·
`#248` L3 the clip census closes at dial 1 (raw and blurred) ·
`#249` L4 the landing gap at the Sudbury spawn and the pump apron ·
`#250` L5 cost: facet_radius_at per tick at 120 Hz ·
`#251` L6 the drawn-above-facet residual, measured ·
`#252` L7 building prisms are based on the contact surface ·
`#253` L8 the prism base follows the facet through sim::step ·
`#254` CLIPPROBE drawn-vs-crash surface at the reported sites ·
`#255` CLIPPROBE cadence: the contact test is a POINT sample, not swept ·
`#256` CLIPPROBE below-surface: what happens to an airframe under ground ·
`#2013` TAPE T2b: facet_contact is stamped, read back, and mismatch warns.

`FACETCONTACT L6-STRICT` (`[.t3-strict]`) and `FACETAI` (`[.facetai]`) are HIDDEN, unregistered with
ctest, and are NOT among the fourteen.

## 6. Gate verdict, SOP 5b FROM CLEAN, GATE ALONE, VERBATIM

From `docs/terrain_clip/GATE_VERDICT_29acc1182_5b.txt` (commit `15c15acae`):
```
gate: 6 failed of 2137
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
GATE_EXIT=0
```
The runner wrote that, not a hand grade: `python tools/gate/gate_baseline.py check
build/gate_29acc1182.log`.

5b in full: `build/CMakeCache.txt` + `build/CMakeFiles/` **deleted**, reconfigured with the
generator/compilers/flags read off the old cache (Ninja, Debug, winlibs MinGW GCC, the same staged
raylib source dir), rebuilt **236/236 exit 0**, re-counted, then the full gate run **ALONE**, detached.

**⚠ THE MERGE MOVED THE SLED REDS' NUMBERS WITHOUT MOVING THEIR NAMES** -- `1287/1288/1326/1329`
became `1290/1291/1329/1332`. That is exactly the count-matched-while-the-members-moved failure
`gate_baseline.py` was written for, and the reason the set was graded BY NAME by the runner and never
by number by a human.

**CLOCK, same file:** merge `f7ccbfa30` committed **21:23:06-0700**, tip `29acc1182` committed
**21:25:43-0700** (epoch 1789705543), gate launched **21:28:59-0700** (epoch 1789705739, **+196 s**),
log last written **22:34:04-0700** (epoch 1789709644) = launch + **3,905 s** == the run's own
`Total Test time (real) = 3904.41 sec` + ~1 s of ctest startup/teardown. No part of the gate predates
the tree it describes.

**LOG PATH:** `build/gate_29acc1182.log` in this worktree, **UNTRACKED** (`.gitignore:1 "build/"`),
605,706 bytes, mtime 2026-09-17 22:34:04 -- the sentinel's rule; not a scratchpad, so an auditor on
this box can read the raw run.

**ONE DISCLOSURE, in the verdict header:** Chad was playing **another lane's** build-play across part
of the window -- `D:\seads_sandboxes\road-repair-f2\build-rel\seads.exe`, PID 14332, started 21:26:18.
Not started by this session, **not killed by it** (process kills are scoped to a worktree). No other
`ctest.exe` and no other lane's gate appeared at any 30 s poll; at 22:34 the sweep returned nothing
but this gate's own. Wall clock 3904.41 s vs the solo `dba79e46b` run's 3654.02 s: +250 s on +7 legs
plus a GPU game for part of it, nowhere near the +498 s the genuinely box-shared `a18fc9b1c` run cost.

## 7. Red-team record

`docs/terrain_clip/REDTEAM_T2.md`. **RT1** on `b58f72e1e`: SOUND WITH FIXES, no P0; P1-1..P1-5 + a P2,
all folded at `c82bef2e2`. **RT2** on `c82bef2e2`: SOUND WITH FIXES; **P0-1** -- the injected prism
argument never reached the game through `sim/step.cpp`, so L7's repair was true in the test and FALSE
in the shipped exe -- plus P1-6/7/9 and a P2, all folded at `a18fc9b1c` and pinned by the new L8.

**Carried OPEN, not closed:** (1) the AI-vs-buildings measurement gap (`FACETAI` binds no
`BuildingColliders`); (2) the Murray pit lid; (3) Chad's ruling on the snow residual (T2 doc §7).

## 8. Chad's fly word

Exe `D:\seads_sandboxes\terrain-clip\build-play\seads.exe`, built **2026-09-17 06:34:56** from tip
`dba79e46b`, zero `SEADS_*` env. Verbatim:

> "terrain hills killed me about 4x so im not going in (at murray mine either), plane landing looks
> nice with wheels about 2 inches at the errington mine nearby landing strip and that is all to
> report, also I didnt land the valley pump apron I am unaware of its radius from the pump centre"

= **fly PASS.** "terrain hills killed me about 4x so im not going in" is the rung's own claim
confirmed from the cockpit.

**NOT FLOWN AND THEREFORE NOT CERTIFIED BY HIM: the Onaping/Valley pump apron** -- T2 doc §6 item 3,
the one landing with real exposure (|facet − field| p50 0.216, p90 0.647, **max 1.779 m**). His stated
reason is that he does not know the apron's radius from the pump centre: **a question owed back to
him, not a defect found.**

**HIS TAG RULING**, verbatim: *"its own lane tag is fine"*. The landing tag is therefore the LANE form
**`terrain-clip-t2-facet-signed`** -- annotated, created **at landing time** at the pushed tip, **not
created by this session**, and **NOT a kernel v18**.

**SEPARATELY, and another lane's:** he flew recon at main `ed0a43ce8` on 2026-09-17 21:19 and said
*"looks fine except those flashing white lines at intersections"* -- that is road-repair F2
(`junction_cut`, still DISARMED at 0). Recorded so the word is not lost; **no leg here grades it.**

## 9. The exe an auditor should launch

`D:\seads_sandboxes\terrain-clip\build-play\seads.exe`, **mtime 2026-09-17 22:39:26**, 109,326,401
bytes, built with `cmake --build build-play --target seads -j 8` (exit 0, 29/30 relinked) **from the
final tip `15c15acae`, after the gate**. RelWithDebInfo, Ninja.

120 s smoke, zero `SEADS_*` env (verified 0 such variables in the environment), launched only after
the box was confirmed free of `ctest.exe`/`seads_tests`; killed by PID **scoped to this worktree**.
The banner it printed:
```
[config] ground: facet_contact 1.00 (injected: yes)
```
Beside it on the same run, unchanged by this lane: `lean_lead 0.300` (v14), `right_hand_rest 0.250 s
| hand_net_window 0.200 s` (v15/v17), `yaw_vert_budget 1.00 gap -5.0..10.0 deg` (v16).

## 10. Fast-forward target

`origin/main` = **`ed0a43ce8b356751d14c550827e3b8add52f73f1`** at packet time, and `f7ccbfa30` merges
exactly that, so `sandbox/terrain-clip` is a **clean fast-forward** of main unless main moves again.
`git ls-remote origin 'refs/heads/tmp/*'` -> **0 lines**: the 107 staging refs are gone.

**THE FAST-FORWARD IS THE SENTINEL'S CALL, NOT THIS SESSION'S.** Nothing here was pushed to main and
no tag was created. What is still owed is HIS, and it is named in §7 and §8.
