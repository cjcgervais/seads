# SESSION HANDOFF 2026-09-08 — game-loop lane + PROJECT SENTINEL

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260908_game_loop_sentinel.md; do §1."

Worktree `D:\seads_sandboxes\game-loop`, branch `sandbox/game-loop` (== main at every landing).
Never work in `D:\flight_sim2\seads-recon` — that is Chad's fly tree, branch `sandbox/r4a-phase0`.
The only things you ever do there are merge `origin/main` in and rebuild `build-play` (§4.7).

## §1 What to do first

1. **You are the sentinel, not a rung-builder.** Chad is testing the HEAD lane and the GAIT lane
   right now. Your job is to watch `origin/main`, audit what lands, and keep his fly tree and exe
   current. **Do not start a loop rung unprompted** — that has been this lane's standing order since
   2026-09-03 and it still is.
2. Arm the watch before anything else (§4.0). Then `git fetch` and, if `origin/main` has moved past
   `3661e5639`, audit it with the §4 checklist.
3. **Read §2 before either of those two lanes pushes.** There is a live trap with their names on it.
4. Chad owes rulings (§5). Ask once, do not nag, and do not act on a guess.

## §2 ⚠ THE LIVE TRAP — both watched lanes are based on a tip that PREDATES the L10 landing

`sandbox/sudburian-head` (1 ahead) and `sandbox/gait` (3 ahead) both branched before
`8278b0638`. So **`git diff origin/main <their branch>` shows the L10 engine fix as DELETIONS** —
`test/unit/test_engine_repair.cpp` at −407 lines, `render/draw.h` at −15, and so on. Nothing is
wrong with their branches. The diff is an artefact of an old base, and it is the exact shape that
cost this project two bad merges on 2026-09-05.

**What to tell each lane, in writing, before they push:**

- Merge `origin/main` ONCE, at or past `3661e5639`, *before* pushing.
- **Never `-X ours` and never "take ours" on a conflicted file.** That silently deletes a landed,
  flown, signed rung.
- The head lane's pending commit `88e126972` ("the bored head: freelook follows for 2 s, then he
  faces front over 2 s") touches **`render/draw.cpp` and `render/draw.h` — the same two files L10b
  touched**. That one is a real union-merge, not a formality. L10 and L10b added exactly two
  defaulted `FrameInfo` fields, `repair_what` and `show_engine_out`, plus one changed condition on
  the ENGINE OUT plate and one changed caption. All four must survive.
- The gait lane's three commits touch `sim/gait.{h,cpp}`, `sim/walker.cpp`, `render/trail_chain.cpp`
  and `test/unit/test_gait.cpp`. No overlap with L10, but the same old-base artefact applies.

## §2b THE TRAFFIC PICTURE — who is queued, and the one cross-lane hazard

**Order agreed 2026-09-08/09: gait lands first, road-repair after it.**

- **gait** (`sandbox/gait`, `D:\seads_sandboxes\gait`) — flown and signed by Chad on the PRE-MERGE
  tip `178aeeae8`; the later merge added no gait source, only the head lane's bored head, so his
  signature stands on the pose he flew and NOT on the landed tree. Those are different trees and the
  difference should stay visible. Merged main cleanly, re-gating on `8932334cb` after the first
  verdict was correctly discarded (the delta had source in it).
- **road-repair** (`sandbox/road-repair`, `D:\seads_sandboxesoad-repair`) — NEW, based at
  `0513de5f8`. Declares `world/snowpack.{h,cpp}`, `world/linework.cpp`, `render/ribbons.cpp`,
  `render/bank_mesh.cpp`, `app/main.cpp` (probe wiring), `config/world.toml`, a new
  `tools/road_gap_probe`, new tests, `docs/road_repair/`. Lands AFTER gait and merges main
  afterwards. `app/main.cpp` is its one shared file with the loop and gait lanes.

⚠ **THE HAZARD, and road-repair is the one lane it can bite.** Gait's G2j-P2 DELETES `work_on` and
`work_blow_on` from `SledRig` and `render::FrameInfo`, replaced by `work_w` / `work_blow_w` /
`work_torque_frac`. State this precisely, because the loose form invites an auditor to skip the
check: **main before gait has SIX code readers** — `app/main.cpp:9022,9024`, `render/draw.h:641,643`,
`render/draw.cpp:4844,4846`, `render/sled_model.cpp:3038,3076` — and gait replaces every one of them
in the same commit, leaving only three comment lines that describe the retirement. So the deletion is
complete and safe to land. What it is NOT safe against is a branch that adds a NEW reader on an older
base: that merges clean and then fails to compile. road-repair's base has the fields; its declared
files do not include `render/draw.h` or `sled_model.*`, so the risk reads low — do not call it low
without looking.

**Looked, 2026-09-09. road-repair is CLEAR of it**, measured in its worktree rather than from its own
description of itself (that second-hand step is what bit gait): its diff against its base adds ZERO
references to `work_on`, `work_blow_on`, `sled_work_on` or `sled_work_blow_on`, in the modified files
and in its three new ones. Gait's deletion cannot reach it.

**And the three-way `app/main.cpp` overlap is disjoint by region.** road-repair inserts 237 lines in
three hunks near lines 78, 107 and 3159-3398. The loop lane's repair wiring sits at ~5400, ~5930,
~7000 and ~9020; gait's walk camera and work-pose wiring plus the head lane's bored-head clock sit in
the ~9020-9100 band. Nothing of one touches a region of another, so a clean union merge is the
expected outcome — and a conflict offered there means something moved and is worth READING rather
than resolving. Packet sent direct (session `flight-sim2-7e`), not relayed.

⚠ **THAT CLEARANCE IS A SNAPSHOT, NOT A STANDING PASS.** road-repair says more rungs land on its
branch tonight, and any later rung that adds a reader of those four names puts it straight back in
the hazard: clean merge, broken build. Re-run the check per rung and again at audit — it is one line:

    git diff <base> HEAD | grep -E '^\+' | grep -cE 'work_on|work_blow_on|sled_work_on|sled_work_blow_on'

Its first rung `a2292a72d` (census probe + baseline census, its gate 6/1997) is committed, the
worktree is clean, and it still reads 0. The earlier observation about uncommitted work is closed.

**The free instrument, worth reusing on every merge:** `ctest -N` before spending the hour. Main
carries 1993 legs, gait adds 3, road-repair adds 4. So gait's merged tip must say 1996 and
road-repair's must say 2000 — a wrong count names which side's legs the merge ate, and it costs
nothing. Gait used it to catch exactly that risk tonight. It holds only while nothing else lands in
between, which is the sentinel's job to say.

## §3 Where everything stands

| | |
|---|---|
| `origin/main` | `3661e5639` |
| the rung merge | `8278b0638` — L10 engine fix + L10b plate afoot |
| lane branch | `sandbox/game-loop` == main, pushed |
| fly tree `seads-recon` | `bd9550f43`, carries everything |
| **Chad's exe** | `D:\flight_sim2\seads-recon\build-play\seads.exe` — **2026-09-08 18:30** |

**What landed and was flown** (Chad, 2026-09-08: "flew it, all good, push to main and seads recon"):
the Sudburian repairs the parked aeroplane's ENGINE with **U**, under the pump rung's own laws —
off the machine, on his feet, a reach measured along the ground, work on the sim tick, the same
work pose, the same bar, walk away and it pauses where he left it. And the ENGINE OUT plate is now
visible on foot, which it never was before. Full detail and the design arguments live in
`docs/SESSION_HANDOFF_20260907_game_loop_engine_fix.md` — read that one if you touch the repair
seam.

Gate on the landing tip, quoted by name:

    gate: 6 failed of 1993
    baseline: 6 known reds
    OK -- the red set is EXACTLY the baseline, member for member.

Other branches ahead of main, none of them yours and none of them pending: `enemy-ai` +1,
`headlight` +1, `graphify` +5, `feel/kernel-v5` +64 (a held reconciliation, not a landing),
`audio/sled-level-pitch` +1, plus two branches whose names say they were rejected or retired.

## §4 The sentinel protocol

### 4.0 Arm the watch, and run exactly ONE

Use the Monitor tool, persistent, polling `git fetch` + `git rev-parse origin/main` every 60 s and
printing the log and file count on a move.

⚠ **One watcher, not two.** A stale bash poll from an earlier session ran alongside the new one this
session and every landing fired twice. Before arming, look for an existing sentinel and stop it:

    Get-CimInstance Win32_Process -Filter "Name='bash.exe'" |
      Where-Object { $_.CommandLine -like '*rev-parse origin/main*' }

### 4.1–4.7 On every new tip

1. `git log --format='%h %an %s' OLD..NEW` and `git diff --stat OLD NEW`.
2. **Every touched file is in the pushing lane's `owns` in LANES.toml, or named in its SOP-5
   announce.** A file in ANOTHER lane's `owns` must never be added to theirs. Match the file list
   against every lane's `owns` patterns programmatically — `fnmatch` over `tomllib.load` does that
   in ten lines and catches what reading does not.

   ⚠ **BUT SEARCH THE ANNOUNCE IN THE RAW FILE TEXT, NEVER IN THE PARSED OBJECT.** By this repo's
   settled convention the SOP-5 announce is written as a `#` COMMENT BLOCK above the lane's keys —
   the loop, flak and head lanes all do it that way — and `tomllib` throws comments away. An audit
   that greps the parsed `status` string will report a missing announce that is sitting right there
   in the file. This bit me on 2026-09-08: the finding happened to be true that time (the announce
   really was absent), so the wrong method produced the right answer and nearly taught the wrong
   lesson. Do both:

       git show <TIP>:LANES.toml | awk '/^\[lanes.<LANE>\]/,/^\[lanes\./' | grep -n '<file>'

   Use the parser for `owns` and for the each-lane-once check; use the raw text for the announce.
3. `git ls-files --eol | grep -c "w/crlf"` must be 0, and `tomllib.load` on LANES.toml must parse
   with each lane appearing exactly once. A duplicated lane block stopped a landing once already.

   ⚠ **AND NO COMMENT MAY BE ORPHANED AT THE FOOT OF A LANE BLOCK.** An announce comment in this
   file belongs ABOVE the keys it describes, because that is how a person reads a comment: as a
   preamble to what FOLLOWS it. A comment sitting between a lane's last key and the next
   `[lanes.X]` header therefore reads as the NEXT lane's announce. That is worse than a missing
   announce — a missing line is silent, while a misplaced one is confidently wrong about somebody
   else's lane. Found on 2026-09-08 by the head lane re-reading its own fix, and neither of our
   methods caught it: an extraction that runs from a section header to the next header sweeps up
   trailing comments and attributes them to the block it started in, so the machine read it
   correctly and a human would not have. The check now ships as a tool:

       python tools/gate/lanes_lint.py LANES.toml     # exit 0 = clean, 1 = orphaned

   ⚠ **Only the FOOT of a block is a fault.** A comment between two keys — between `owns` and
   `status`, which is where the loop and ai lanes put theirs — is inside its own block, above the
   keys it belongs to, and is CORRECT. My first cut of this check flagged every comment following
   any key and cried wolf on two lanes including my own; the shipped tool finds the LAST key in
   each block and only complains about comments after it. Verified both ways before shipping:
   clean on `fd645a9f5`, and it reproduces the fault on `58d0abeb3`.

   On a cp1252 Windows console the finding lines render this file's stars and warning signs as `?`.
   That is the fallback doing its job, not a corrupted file — the tool prints the real text first
   and only degrades when the console raises, so `PYTHONIOENCODING=utf-8` gives you the glyphs back.
   What you act on is the lane name and the line number, and those are always ASCII.
4. `generated/graph` regenerated in the SAME commit if includes or structure changed; then
   `python tools/graph/graph_query.py check` must say `layer check OK`.
5. **The lane's own gate verdict, quoted BY NAME**, on the MERGED tip:
   `python tools/gate/gate_baseline.py check build/.gate_ctest.log`. A pre-merge gate is a
   pre-check, not a verdict. A log's "N failed" is not a verdict either — only the tool's "the red
   set is EXACTLY the baseline, member for member" is.
6. **No loop seam moved.** Grep the `app/main.cpp` hunks for
   `gave_up|spawn_menu_should_show|respawns_locked|player_mode_transition|drone_home|sled_upright|
   kSurfacePumpMastM|fix_line|repair_target|engine_in_reach|show_engine_out`. `app/player_mode.h`,
   `app/interact.h`, `app/spawn_policy.h` and `combat/{pump,engine}_repair.h` are untouched unless
   announced.
7. **Report OBSERVATIONS, never a diagnosis. Never revert and never force-push on main.** Then merge
   `origin/main` into `seads-recon`, rebuild, and tell Chad the exe time:

       cd /d/flight_sim2/seads-recon
       git fetch -q origin && git merge --no-edit origin/main
       # the ONLY conflict this merge ever has:
       git checkout --theirs generated/graph/graph.json && git add generated/graph/graph.json
       python tools/graph/graphify.py && git add generated/graph && git commit --no-edit
       cmake --build build-play --target seads -j 8
       stat -c '%y' build-play/seads.exe

   **Docs-only and registry-only landings need no rebuild** — say so rather than burning ten minutes.

## §5 Rulings owed by Chad

Ask once, together, and let him answer when he wants. None of these blocks anything.

1. **`sim/` frozen-kernel wording vs `sim/gait.{h,cpp}`.** The contribution rules say no lane touches
   `sim/`; the gait lane added two files there on the walker's precedent and owns them in the
   registry. Bless the precedent, carve an exception, or relocate. Owed since 2026-09-06.
2. **Where `fix2_workpad/` lives.** It sits at the repository root. Owed since 2026-09-06.
3. The retired legacy rider in the export contract, and the walker_stride speed source (gait's).
4. **Three readings from the L10 rung he has not contradicted** — he flew it and said "all good",
   which is acceptance, not a ruling. Each is a one-line change if he ever differs:
   - the engine offer appears whenever the engine is short of whole, not only at ENGINE OUT;
   - `engine_full_s = 45 s` is a dial, not his 60 s pump ruling;
   - with a pump and an engine both in reach, **U** takes the pump.

## §6 Instruments — run them before believing a report

- `SEADS_ENGINEWALK_SMOKE="<start_m>[,bearing]" build/seads.exe --smoke <ticks>` — breaks the engine
  the way a prop strike does, parks the aeroplane, seeds the machine, steps the man off, walks him
  in, presses U through the real transition, logs every 30 ticks.
- `SEADS_PUMPWALK_SMOKE="<start_m>[,bearing]"` — the same for the pump. The LAW behind both: run the
  rig before believing any "I cannot reach it" report. It has paid for itself twice.
- `SEADS_PUMPWALK_SMOKE`'s sibling `SEADS_SMOKE_REPAIR=1|2` drives the work pose alone.
- ⚠ `SEADS_SMOKE_WALK` BURIES the man and does not draw him — use `SEADS_PUMPWALK_SMOKE` instead.

## §7 Traps, all of them paid for

- **The walker cannot take a step without a seeded machine.** The whole sled+walker+gait tick is
  gated on `player.off_aircraft() && player.sled_seeded`. Correct for the shipped game — `Afoot` is
  only reachable by stepping off a machine — but it silently stalls any rig that forgets to seed one.
- **A `--smoke` run keeps flying the aeroplane.** Parking it once is not enough; it rolled away at
  ~11 m/s while the man closed at 0.73. A bench rig must re-pin its fixture every frame.
- **Scope every process kill by worktree path.** Match `seads_tests.exe` whose CommandLine contains
  your worktree, then its `ctest` PARENT. A kill by image name has cost three lanes before. When you
  do kill your own run, say so afterwards and confirm the other lanes' gates survived.
- **A killed ctest leaves no summary line** and `gate_baseline.py check` refuses it. That is the tool
  working. Discard the run, do not report it.
- **Run gates DETACHED**: `Start-Process bash.exe -ArgumentList "-lc","<worktree>/gate_run.sh"`.
  `setsid nohup` from the Bash tool does not survive.
- **A running exe blocks its own relink** — "cannot open output file: Permission denied". Stop your
  own background run first.
- **Inline `python - <<'EOF'` in the Bash tool mangles backslashes** (`\\n` arrives as `\n`) and a
  failed python does NOT stop a `;`-chained commit. Write the script to the scratchpad, run the file.
- **`app/main.cpp` contains a literal NUL byte** in a character literal near the interact hint, which
  is why `grep` calls it binary. Use `grep -a`, and match `'\x00'` when patching that line.
- **`raylib`'s `TakeScreenshot` ignores an absolute path** and writes to the working directory. Move
  the file out of the repo afterwards — scratch at the repo root is a landing stop.
- **`git merge -F -` cannot read stdin.** Write the message to a file.
- `ls -la` dates are unreliable here; use `stat -c %y`.
- Never name a local `near` or `far` in a test — ancient Windows macros.
- The Bash tool's cwd sometimes persists and sometimes resets. `cd` explicitly in every command that
  touches a repo.

## §8 Packets — how you reach the other lanes

A repo doc, `docs/PACKET_TO_<LANE>_from_sentinel_<date>.md`, pushed to main docs-only, PLUS a session
message. `ListAgents` does not name lanes, so the doc is the reliable channel; the message is the
nudge. Templates: `docs/PACKET_TO_GAIT_from_sentinel_20260906.md` and
`docs/PACKET_TO_SUDBURIAN_HEAD_merge_from_sentinel_20260906.md`.

**Merge-order law:** when two lanes hold unpushed edits to the same file, name the order in BOTH
packets. The second merges main once, after the first's SHA, and union-merges. Say "never take-ours"
explicitly — see §2, it is live right now for the head lane and `render/draw.{h,cpp}`.
