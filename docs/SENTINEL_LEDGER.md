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
| 2026-09-15 | `b697d24a5` | kernel v15 S-righthand: landing record, flight-log row, CLAUDE.md line | Sentinel role taken. Queue ruled in writing: (1) **atmosphere-snow** (`sandbox/atmosphere-snow` @ `6df9b67a4`, Chad-signed: "I like the last changes made to the atmosphere for flight_sim2") lands first after merging main and re-gating; its LANES claim of a fresh red-team was retracted by the lane as unsupported and a red-team is running; pre-merge kernel proof PASSED empty. (2) **kernel v16** (`feel/lateral-yawbudget` @ `40325f297`, ON HOLD for Chad's ruling on the loop question) merges past the atmosphere landing. (3) **road-repair** (`sandbox/road-repair` @ `f64497f83`, F1 gated unflown, F2 WIP paused) after that; its audit session flagged `config/world.toml junction_cut_m = 12.0` ARMED under a DO-NOT-BUILD message. Fly tree `seads-recon` carries `b697d24a5`, exe 2026-09-13 10:39. |
