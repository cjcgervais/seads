# SENTINEL LEDGER — pushes into `flight_sim2` (`origin/main` of `seads_sandbox1`)

Chad, 2026-09-15: *"I need to have a sentinel for any pushes that are going into flight
sim2 ... SEADS."* This repo's agent holds that role from this date. The role, as the live
tree's own protocol defines it (`docs/SESSION_HANDOFF_20260908_game_loop_sentinel.md` §4 and
`docs/SESSION_HANDOFF_20260912_sentinel.md` in `D:\flight_sim2\seads-feel`): **traffic
control for `origin/main` — name the order, audit every tip, report OBSERVATIONS never a
diagnosis, never revert, never force, keep Chad's fly tree and exe current.** This agent
stays READ-ONLY in the live trees (its own charter); the lanes execute, the sentinel audits
and rules order.

## The push SOP every lane gets (the packet, in one place)

1. **Merge `origin/main` ONCE** at or past the last landed SHA. Union-merge every conflict.
   Never `-X ours`, never "take ours" — that deletes a landed, flown, signed rung. The
   expected conflicts are `generated/graph/*` (`checkout --theirs`, regenerate with
   `tools/graph/graphify.py`, add in the same commit), `LANES.toml` (both blocks survive),
   `CLAUDE.md`, `docs/flight-log.md`, `CMakeLists.txt`, `app/main.cpp`.
2. **Kernel proof.** Pre-merge: `git diff --stat <merge-base> HEAD -- control/
   config/controller.toml test/golden/controller_golden.h app/feel_tape.h
   app/feel_tape_columns.h test/harness/feel_tape.h test/unit/test_loop_rollover.cpp
   test/unit/test_tape_roundtrip.cpp test/unit/test_yawbank_balance.cpp` prints nothing.
   Post-merge: the same against `origin/main` prints nothing. (A world lane must never move
   the kernel; a kernel lane names exactly what it moved.)
3. **Hygiene on the merged tip:** `git ls-files --eol | grep -c i/crlf` = 0;
   `PYTHONIOENCODING=utf-8 python tools/gate/lanes_lint.py LANES.toml` exit 0; tomllib lane
   count == `grep -c '^\[lanes\.'`; `python tools/graph/graph_query.py check` = layer check OK;
   every touched file in the lane's `owns` or its raw-text SOP-5 announce.
4. **`ctest -N` on the merged tip**, predicted in advance, any delta explained by name.
5. **Full gate on the merged tip**, verdict quoted BY NAME from
   `python tools/gate/gate_baseline.py check build/.gate_ctest.log` ("the red set is EXACTLY
   the baseline, member for member"); gate-log mtime postdates the tip's commit time by about
   the gate length. A docs-only delta does not invalidate a verdict; a source delta does.
6. **Fresh-context red-team** with a WRITTEN record committed under the lane's docs; P0/P1
   folded before the gate. A LANES status line is not a record.
7. **Chad flies the merged tip's exe, his word verbatim** in the LANES block (kernel rungs:
   also the `CLAUDE.md` kernel line, a `docs/flight-log.md` row, and an annotated
   `kernel-vN-<name>-signed` tag at the pushed tip).
8. **Ping the sentinel with the merged SHA and the outputs of 2–5. Wait for the reply.**
   Silence is never a go.
9. **Push the lane, then `git push origin <tip>:main` as a fast-forward.** No force, ever.
10. **Fly tree:** in `D:\flight_sim2\seads-recon` (`sandbox/r4a-phase0`) `git fetch && git
    merge --no-edit origin/main` (only `generated/graph/*` ever conflicts), check `tasklist`
    for `seads.exe` IN ITS OWN STEP, then `cmake --build build-play --target seads -j 8`,
    report the exe mtime to Chad. Docs-only landings need no rebuild.
11. Stage paths explicitly; never `git add -A`.

## Ledger — dated observations, newest first

| date | origin/main | subject | observation |
|---|---|---|---|
| 2026-09-15 22:30 | `f4c9aa2e6` | unchanged | **Atmosphere landing COMPLETE and audited by report:** recon `sandbox/r4a-phase0` `29e3b53de`→`5d38ed150` (graph.json only conflict), tasklist check in its own step, **Chad's exe `build-play/seads.exe` 2026-09-15 22:17:44** (supersedes 09-13 10:39). Landed commit carries the gate verdict, the count note, the hook-collision provenance, and both P1s shipping unfixed by ruling with AS-6 owning them. Count question CLOSED: v15 main = 2044 by ctest -N. AS-6 queue (spawn-clear phase_off, gauge re-derivation, P2s) not started. |
| 2026-09-15 22:20 | `f4c9aa2e6` | unchanged | Kernel lane merged main → `b4fbea3d4` (conflicts .gitignore + graph only; kernel firewall: main's delta touched no sim/control/controller.toml). **ctest -N reconciled:** v15 main registers **2044**, not the 2060 quoted from the gate line — atmosphere merged 2044+30 = 2074 ✓, kernel merged 2044+24+30+3 = 2101 ✓. Lane asked to verify with `ctest -N` on a b697d24a5 build. Gate on `b4fbea3d4` running (22:18); play exe 22:19; Chad's confirmation fly owed. |
| 2026-09-15 22:15 | **`f4c9aa2e6`** | LANES atmosphere: AS-5 LANDED 2e5675742 -- flown+signed, gate 6 of 2074 == baseline six by name | **MAIN MOVED: atmosphere-snow landed** (fast-forward through `2e5675742`). Recon merge + exe rebuild owed by the lane, mtime not yet reported. Kernel lane told to merge past `f4c9aa2e6`. Kernel lane state at `125328c05`: red-team LAND-WITH-FIX folded (P1-1 budget floor 1 deg/s changes the flown artefact, so Chad's 18:50 word — "I am actually pretty satisfied with the kernel now... I'm really good on this" — is NOT the landing word; one confirmation fly owed on the merged tip), pre-merge gate 6 of 2068 by name, v15 hash intact. Post-landing audit of `f4c9aa2e6` owed by the sentinel on resume. |
| 2026-09-15 (OK) | `b697d24a5` | unchanged | **atmosphere-snow `2e5675742` CLEARED TO LAND.** Gate by name: 6 of 2074 == baseline six member for member, log mtime 19:21 vs tip 17:48. ctest -N 2074 vs the sentinel's additive estimate 2090 **UNRECONCILED** (file-level checks clean: all 135 main test files present, every v15 instrument registered by name; the estimate was built from two hand-quoted numbers). **OWED BY THE SENTINEL:** ctest -N name-list diff between a clean `origin/main` build and the landed tip. Hook-vs-gate build collision noted; the lane's log carries no collision signature. |
| 2026-09-15 (v16 ack) | `b697d24a5` | unchanged | Kernel lane (`flight-sim2-57`) acknowledged the order and the SOP. Chad's v16 ruling verbatim (to that lane): **"no deck save unload keep the yaw budget"**. S-unload removed net-zero at `b2bc840c5`, scrap tag `scrapped/s-unload-20260915` = `40325f297`; v15 hash `dbdf52980174305e` intact; pre-merge gate running as a smoke; P1-3 pins to fold before the merge; Chad has a fly checklist for the yaw-budget-alone exe. Waits for the atmosphere landing SHA. |
| 2026-09-15 (v16 lane) | `b697d24a5` | unchanged | **Kernel lane pushed `feel/lateral-yawbudget` → `18cc020ce`:** S-unload REMOVED at Chad's ruling (`b2bc840c5`), **v16 = `[coordination] yaw_vert_budget` alone**. Session `flight-sim2-57` sent the kernel-lane packet: lands AFTER atmosphere-snow, merge main once past it, fold red-team P1-3 pins, fresh red-team with record, gate by name, Chad flies the yaw-budget-alone build and gives a new land word ("I like it now" was on both dials), CLAUDE/flight-log/LANES/tag in one landing. |
| 2026-09-15 (pre-audit) | `b697d24a5` | unchanged | **atmosphere-snow merged tip `2e5675742` PRE-AUDITED PASS** (read-only, reproduced): main is an ancestor; post-merge kernel proof empty; field bit-identical to the flown `5a314311d` (Chad: "Land what you flew, fix in AS-6", his words to the lane directly); 21 source files all declared; main.cpp 0 seam hits; `[weather]`/`[weather_cell]` parsed identical; hygiene clean; red-team record `docs/atmosphere_snow/AS5_RED_TEAM_20260915.md` committed at `a09868d73`. Gate running; ctest -N and the by-name verdict still owed before the OK. Lane also ruled with Chad: game lanes push to `seads_sandbox1` only, never `cjcgervais/seads`. |
| 2026-09-15 (ruling) | `b697d24a5` | unchanged | **Chad rules on the atmosphere hold, verbatim: "land it as flown was my ruling"** (said to the sentinel session, prefixed "If this is the atmosphere agent please only concern yourself with the atmosphere"). Relayed to the lane: land the tree he flew, P1-A/P1-B recorded and deferred to AS-6, red-team record committed, then merge main once, re-gate the merged tip, ping, fast-forward. |
| 2026-09-15 (later) | `b697d24a5` | unchanged | **atmosphere-snow HOLDS on its own red-team.** Fresh-context red-team on `5a314311d` returned SOUND-WITH-FIXES, no P0, **two P1s confirmed by execution**: (A) the AS-4 flurry-fill gauge (`kFlurryFillGauge` 0.85) is a measured no-op at the AS-5 squall geometry and its test bar was moved to `gauged <= chance + 0.1`, which cannot fail (mutation-killed: gauge set to 1.0, suite 70094/70094 green); (B) spawn-clear is broken for the snow: `squall_gate_lo` -0.65 sits below the -0.098 constraint stated in `render/weather.h`, so at t=0 63% of in-dome directions have snow and 36% heavy under a provably clear haze sky (R1 and R2 are one bug). Clean: sim/control untouched, `[weather]`/`[weather_cell]` zero-diff, OFF pin is a true bit-identity. Lane will not dial either fix silently (both move the field Chad signed); the ruling goes to Chad: fix both + re-fly (lane's recommendation) or land as signed and fix in AS-6. Nothing merged, nothing pushed. |
| 2026-09-15 | `b697d24a5` | kernel v15 S-righthand: landing record, flight-log row, CLAUDE.md line | Sentinel role taken. Queue ruled in writing: (1) **atmosphere-snow** (`sandbox/atmosphere-snow` @ `6df9b67a4`, Chad-signed: "I like the last changes made to the atmosphere for flight_sim2") lands first after merging main and re-gating; its LANES claim of a fresh red-team was retracted by the lane as unsupported and a red-team is running; pre-merge kernel proof PASSED empty. (2) **kernel v16** (`feel/lateral-yawbudget` @ `40325f297`, ON HOLD for Chad's ruling on the loop question) merges past the atmosphere landing. (3) **road-repair** (`sandbox/road-repair` @ `f64497f83`, F1 gated unflown, F2 WIP paused) after that; its audit session flagged `config/world.toml junction_cut_m = 12.0` ARMED under a DO-NOT-BUILD message. Fly tree `seads-recon` carries `b697d24a5`, exe 2026-09-13 10:39. |
