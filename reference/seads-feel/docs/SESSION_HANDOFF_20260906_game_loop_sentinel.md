# SESSION HANDOFF 2026-09-06 — game-loop lane + PROJECT SENTINEL

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260906_game_loop_sentinel.md; do §1."
Worktree `D:\seads_sandboxes\game-loop`, branch `sandbox/game-loop` (== main at every landing).
Never work in `D:\flight_sim2\seads-recon` (Chad's fly tree, branch `sandbox/r4a-phase0`) except to
merge `origin/main` into it and rebuild `build-play` (§4).

## §1 What to do first

1. `git fetch`; if `origin/main` has moved, audit it (§4 checklist) before anything else.
2. All five lanes of 09-04..09-07 are landed (§2, §3.1); nothing is pending a sync.
3. Ask Chad the two rulings owed (§5). Do NOT start a loop rung unprompted — the lane's standing
   order is sentinel + watch; Chad drives the rungs.

## §2 What landed on main this session (2026-09-04 → 09-06), all flown by Chad

| SHA | lane | what |
|---|---|---|
| `a34641444` | loop L8 | **X held 1.5 s = give up** (any mode but Pilot/OnGun; `DeathCause::kGiveUp`, wire `giveup`; spends a life, reborn in the aircraft, same spawn menu; under the R9 lock only the refusal note). **Spawn menu** dead Snowmachine row says how it comes alive. **M map** draws the flying Sting (filled ally diamond + heading tick). **P from the seat**: `PlayerModeState::drone_home`, `ModeContext::sled_upright`, `sting_stance` lambda in main.cpp (seat pos/heading/eye); the flight's end hands back the bars. |
| `c5ff368b1` | sudburian-head | sculpted head + mullet on the rider (surgical GLB patch, `render/sled_model.cpp` COLOR_0 + mullet pass, flak gunner allowlist) |
| `b80961064` | sting ST-5 | hero Sting + gunstock launcher GLBs, 3 s seat deploy (hand hook, sit-up, aim twist, seated ±90°), prop spin/buzz, freelook zoom, map follows the sting; **moved the loop's launch-height dial to measured 0.80 m seated / 1.85 m afoot** (theirs to keep now) |
| `09defb8fa` | loop L9 | **pump reach** (§6): pump HOUSE on the ground, prompt counts inside reach, reach 6→8 m, `SEADS_PUMPWALK_SMOKE` rig |
| `49161ee30` | sudburian-head (scarf) | FIX2 wrap re-patch on the GLB, parked-still latch, `render/trail_chain.cpp` rest_push contact law (red-teamed before landing, pogo gate test 7b) |
| `9c7124053` | gait | walk/run/jump/dash ladder, footsteps, posture, G2j pump work pose; `sim/gait.{h,cpp}` NEW in sim/ (§5.1) |

Fly tree `seads-recon` @ `38b15a474` carries all of it; exe rebuilt 2026-09-07 00:11.

## §3 Open, in order

### 3.1 Head lane (scarf fixes) — LANDED main `49161ee30` 2026-09-07 00:05 (audited PASS; recon
`38b15a474`, exe 00:11). Kept below as the record of how a three-stop landing was run.
Branch `sandbox/sudburian-head` @ `cd22abdd3`, worktree `D:/seads_sandboxes/sudburian-head-lane`,
bridge session "Seads-recon enemy AI improvement". Packet: `docs/PACKET_TO_SUDBURIAN_HEAD_merge_from_sentinel_20260906.md`.
Stops: (1) CRLF ×3 in `fix2_workpad/glb/*.json`; (2) `fix2_workpad/` scratch at the REPO ROOT —
Chad rules where it goes; (3) their own handoff says "NO LANDING without his drive" — status
needs his dated sign-off. They merge main ONCE at ≥ `9c7124053`, union-merge `render/sled_model.cpp`
(+102 beside gait's +1269 and sting's hand hook), `render/trail_chain.cpp` (scarf lane's file —
a contact-law change for every chain), `assets/sled/indy650.glb` re-patched. Full gate on the
merged tip, `--no-ff`, ping. Then sync seads-recon + rebuild.

### 3.2 Hornet animation lane — prompt queued, not started. Will need a packet like the sting one
(`docs/PACKET_TO_STING_ANIM_from_gameloop_20260904.md` is the template).

### 3.3 Head lane's GLB integration rung (`docs/SESSION_HANDOFF_20260904_head_glb.md` §6) — when it
reaches engine code/assets it gates + announces + pings the sentinel first (agreed in writing).

## §4 The sentinel protocol (Chad 2026-09-04: "no agents commit any changes that aren't their own")

A persistent monitor polled `origin/main` every 60 s this session (`git fetch` + `git log`
old..new); re-arm it (Monitor tool, or a background `until` loop — the Monitor once timed out
without firing; the `until grep GATE_EXIT` background bash was reliable).

On every new main tip:
1. `git log --format='%h %an %s' OLD..NEW`; `git diff --stat OLD NEW`.
2. Every touched file is in the pushing lane's `[lanes.X].owns` OR named in its `status`
   announce (SOP §5). A file in ANOTHER lane's `owns` must not be added to theirs.
3. `git ls-files --eol NEW | grep -c w/crlf` == 0; `tomllib.load(LANES.toml)` parses with each
   lane once (the sting landing was stopped on a duplicated block run).
4. `generated/graph` regenerated in the same commit if includes/structure changed.
5. The lane's own gate verdict quoted BY NAME (`gate_baseline.py check`) on the MERGED tip —
   a pre-merge gate is a pre-check, not the verdict.
6. No loop seam moved: grep the main.cpp hunks for `gave_up|spawn_menu_should_show|
   respawns_locked|player_mode_transition|drone_home|sled_upright|kSurfacePumpMastM|fix_line`;
   `app/player_mode.h`, `app/interact.h`, `app/spawn_policy.h` untouched unless announced.
7. Report OBSERVATIONS (never a diagnosis), never revert or force on main; then merge main into
   `seads-recon` (`git checkout --theirs generated/graph/graph.json` + graphify on the graph
   conflict — the only conflict it ever has), push, rebuild `build-play --target seads`,
   tell Chad the exe time. Docs-only landings need no rebuild.

Merge-order law learned tonight: when two lanes hold unpushed edits to the same file, name the
order in both packets; the second merges main ONCE after the first's SHA and union-merges. A
branch whose base predates a landing shows that landing as DELETIONS in `git diff origin/main` —
tell them "never take-ours" explicitly; it saved two merges tonight.

Packets: repo docs (`docs/PACKET_TO_<LANE>_from_sentinel_<date>.md`, pushed to main docs-only)
PLUS a session message. Addresses used: gait = `uds:\\.\pipe\LOCAL\cc-msg-cd9e7eb0...` (session
flight-sim2-27), head = `bridge:session_01TrKKGYzcuEp235bYd9RxYo`; sting ST-5 was reached via the
doc only. `ListAgents` does not name lanes — the doc is the reliable channel.

## §5 Rulings owed by Chad

1. **`sim/` frozen-kernel wording vs `sim/gait.{h,cpp}`.** SOP says no lane touches sim/; the gait
   lane added two files there by the walker precedent and owns them in LANES. Bless by precedent,
   carve an exception, or relocate — the gait lane acts on his word (logged as owed on their side).
2. **Where `fix2_workpad/` goes** (§3.1 stop 2).
3. Standing from earlier: the retired legacy rider in the export contract; the walker_stride
   speed-source kernel ruling (gait lane's).

## §6 The pump-reach instrument (L9) — use it before believing "can't reach the pump"

    SEADS_PUMPWALK_SMOKE="<start_m>[,bearing_deg]" build/seads.exe --smoke 4000

Damages the own surface pump at tick 40, seeds the machine `start_m` from the pump's FOOT on the
drive surface, puts the man off on his feet, walks him straight in, logs every 30 ticks:
`gd` (ground distance to the axis), `h_dem`/`h_drv` (height over DEM / drive), terrain rise to
the pump, walker/player mode, the live prompt. Deep snow walk is ~1.1 m/s: 12 m ≈ 1200 ticks.
Finding tonight: no floor in either tree from four bearings; Chad's "6 m" was the out-of-reach
hint's last number before it became a static "U FIX PUMP", under a cube floating 5 m up. The
mast is `app::kSurfacePumpMastM` (10 m) — the ONE number; `FrameInfo::ConquestPump::foot` is the
terrain point the house stands on and the reach measures to.

## §7 Traps recorded this session

- Inline `python - <<'EOF'` in the Bash tool mangles backslashes (`\\n` → `\n`) and a failed
  python does NOT stop a `;`-chained commit. Write scripts to the scratchpad, run the file.
- `ls -la` dates are unreliable here; `stat -c %y` for exe times.
- The Bash tool's cwd sometimes persists across calls and sometimes resets — `cd` explicitly in
  every command that touches a repo (one stray merge went into the wrong worktree; harmless).
- Grep for `w/crlf` on the TREE (`git ls-files --eol`), not on the diff.
- A gate log's "6 failed" is not the verdict; only `gate_baseline.py check`'s "EXACTLY the
  baseline, member for member" is.
