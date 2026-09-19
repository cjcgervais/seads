# SENTINEL PRE-AUDIT PACKET -- terrain-clip, tip `b58a8a52d`
Lane `sandbox/terrain-clip`. Every number below is read from a committed file or from git; none is recalled.

**1. Tree.** Tip `b58a8a52df0ebaa2b9fca47c0de2a79e09543e0f`. The CODE tip is `a18fc9b1c` (T2c red-team fold); `dba79e46b`, `e8f70fb0e` and `b58a8a52d` add only docs. **NO MERGE WAS MADE -- that is step 6's answer, not an omission; see §9.**

**2. Kernel proof.** `git diff --stat origin/main HEAD -- sim control config/controller.toml`:
```
 sim/environment.h |  50 +++++++++++++++++++++++++
 sim/ground.h      | 108 +++++++++++++++++++++++++++++++++++++++++++++++++++---
 sim/step.cpp      |  14 ++++++-
 3 files changed, 164 insertions(+), 8 deletions(-)
```
NOT empty **by design**: T2 is a one-dial KERNEL rung, `[ground] facet_contact` (shipped 1.0, 0 = identity), and that is the whole delta. `control/` and `config/controller.toml` are **EMPTY** (`git diff --stat origin/main HEAD -- control config/controller.toml` prints 0 bytes), and main's own movement touches no `sim/` or `control/` file, so the two deltas do not overlap. **Identity BY BRANCH, not by tolerance:** at `facet_contact 0` the facet function is not called at all. Proved in `test/unit/test_facet_contact.cpp`, by name: `FACETCONTACT L1 identity BY BRANCH: at 0 the facet fn is not called` and `FACETCONTACT L1 identity: a full ground_contact tick is unmoved at 0`; `FACETCONTACT L1 dial: shipped 1.0, bounded [0,1], read by the app` pins the dial, and `FACETCONTACT L8 the prism base follows the facet through sim::step` proves the SHIPPED path carries it, not just the test path.

**3. Hygiene.** `tools/gate/lanes_lint.py` -> `OK -- no comment is orphaned at the foot of any lane block.`, exit **0**. `git ls-files --eol | grep -c 'w/crlf'` -> **0**. `python tools/graph/graph_query.py check` -> `layer check OK`, exit **0**.

**4. Legs.** `ctest --test-dir build -C Debug -N | tail -1` -> **Total Tests: 2130** = main `4208c392e`'s 2116 + the **14** this lane adds: FACETCONTACT `L1 dial`, `L1 identity BY BRANCH`, `L1 identity full tick`, `L2`, `L3`, `L4`, `L5`, `L6`, `L7`, `L8` (ten); CLIPPROBE `drawn-vs-crash surface at the reported sites`, `cadence: the contact test is a POINT sample, not swept`, `below-surface: what happens to an airframe under ground` (three); `TAPE T2b: facet_contact is stamped, read back, and mismatch warns` (one). `FACETCONTACT L6-STRICT` (`[.t3-strict]`) and `FACETAI` (`[.facetai]`) are HIDDEN, unregistered with ctest, and are NOT among the fourteen.

**5. Gate verdict, SOP 5b, VERBATIM** from `docs/terrain_clip/GATE_VERDICT_dba79e46b_5b.txt`:
```
gate: 6 failed of 2130
baseline: 6 known reds

OK -- the red set is EXACTLY the baseline, member for member.
GATE_EXIT=0
```
5b was done in full: `build/CMakeCache.txt` + `build/CMakeFiles/` **deleted**, reconfigured with the generator/compilers/flags read off the old cache (Ninja, Debug, winlibs MinGW GCC, the same staged raylib source dir), rebuilt 236/236 exit 0, re-counted, then the full gate run **alone**, detached. Clocks, same file: tip `dba79e46b` committed **06:33:50-07:00**, gate launched **20:07:48** (+48,838 s), log last written **21:08:44** = launch + 3,656 s = the run's own `Total Test time (real) = 3654.02 sec` plus 2 s -- no part of the gate predates the tree it describes. **The raw log is UNTRACKED at `build/gate_dba79e46b.log` in this worktree** (the sentinel's new rule; not a scratchpad). One disclosure, in the verdict header: Chad's own `D:\flight_sim2\seads-recon\build-play\seads.exe` (PID 6892, started 20:04:09) was on the box while he played; **no other ctest appeared at any poll**, and this run is ~500 s FASTER than the box-shared `a18fc9b1c` run (3654.02 vs 4152.52 s) -- the contamination signature is absent.

**6. Red-team.** `docs/terrain_clip/REDTEAM_T2.md`. **RT1** on `b58f72e1e`: SOUND WITH FIXES, no P0; P1-1..P1-5 + a P2, all folded at **`c82bef2e2`**. **RT2** on `c82bef2e2`: SOUND WITH FIXES; **P0-1** (the injected prism argument never reached the game through `sim/step.cpp`) + P1-6/7/9 + a P2, all folded at **`a18fc9b1c`**. Carried OPEN: the AI-vs-buildings measurement gap (FACETAI binds no `BuildingColliders`), the Murray pit lid, and Chad's ruling on the snow residual (T2 doc §7).

**7. Chad's fly word.** Exe `D:\seads_sandboxes\terrain-clip\build-play\seads.exe`, built **2026-09-17 06:34:56**, from tip `dba79e46b` (committed 06:33:50, 66 s earlier). Verbatim:
> "terrain hills killed me about 4x so im not going in (at murray mine either), plane landing looks nice with wheels about 2 inches at the errington mine nearby landing strip and that is all to report, also I didnt land the valley pump apron I am unaware of its radius from the pump centre"

= **fly PASS**; "terrain hills killed me about 4x so im not going in" is the rung's own claim confirmed from the cockpit. NOT flown and therefore not certified by him: the **valley (Onaping) pump apron** -- T2 doc §6 item 3, the one landing with real exposure (worst case ~1.8 m inside the footprint). His stated reason is that he does not know the apron's radius from the pump centre: a question owed back to him, not a defect found.

**8. Fast-forward target.** `origin/main` = **`ed0a43ce8`** -- not `4208c392e`; it moved again, §9.

**9. ⚠ STEP 6 STOPPED ON PURPOSE: MAIN MOVED WITH CODE, NOT DOCS.** The merge was expected to be `4208c392e`, LANES-only. It is not: `git log --oneline HEAD..origin/main` is **19 commits** -- the whole **road-repair** lane landed (`ed0a43ce8`, a merge), bringing `render/ribbons.cpp`, `render/bank_mesh.{h,cpp}`, `render/ribbon_junction.h`, `render/ribbon_subdiv.h`, `render/draw.*`, `world/linework.*`, `tools/road_repair/`, `test/unit/test_bank_mesh.cpp`, `test/unit/test_load_world.cpp`, `test/unit/test_flash_grade.py`, and regenerated `generated/graph/` -- **186 files, +19,131/-177**. A trial merge conflicted ONLY in the generated graph artefacts (`generated/graph/graph.json` and four `digest/*.md`), which regenerate rather than resolve. Main's new code touches **no** `sim/` or `control/` file, so it does not collide with this lane's one-dial kernel delta -- but it **is code**, it adds **+7 legs** (main's count becomes 2123), so a merge invalidates §4's count and requires a **re-gate**. Per the standing instruction, *"if it touches code, say so and stop"*: the merge was **aborted and not made**. The lane is clean at `b58a8a52d`; the merge + re-gate is the next decision, and it is not this session's to take.
