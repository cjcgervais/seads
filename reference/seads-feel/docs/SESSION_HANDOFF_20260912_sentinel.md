# SESSION HANDOFF 2026-09-12 — PROJECT SENTINEL (the two-night landing run)

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260912_sentinel.md; do §1."

This supersedes `docs/SESSION_HANDOFF_20260908_game_loop_sentinel.md` for the sentinel role. That
doc's §4 protocol and §7 traps are still law; this one adds two nights of landings, the new
worktree, the queue as it stands, and the defects found by re-reading the method.

Chad, 2026-09-09: "please be the acting sentinel for this session ... stand by to help them merge
when they are all done." Later: "I have a flight kernel change incoming ... I want you to know if
this is sentinel." The role is traffic control for `origin/main`: name the order, audit every tip,
report OBSERVATIONS never a diagnosis, never revert or force, and keep his fly tree + exe current.

## §1 What to do first

1. **Work in `D:\seads_sandboxes\_sentinel`** (a DETACHED worktree created 2026-09-10 with
   `git worktree add --detach`). ⚠ **NEVER `git checkout` anything in `D:\seads_sandboxes\game-loop`
   any more** — the cam-smooth session (`flight-sim2-f3`) builds loop rungs there (L11, L12) with
   live uncommitted edits. On 2026-09-10 a chained sentinel command `cd`'d into another lane's
   worktree and ran `git checkout` there, flipping it onto the wrong branch for two minutes. Tree
   was clean so nothing was lost, but it is exactly the class of damage the sentinel exists to
   prevent. Every audit command starts `cd /d/seads_sandboxes/_sentinel && git fetch -q origin &&
   git checkout -q --detach origin/main`.
2. **Arm ONE watch** (§4.0 of the 0908 doc). Kill any stale one first:
   `Get-CimInstance Win32_Process -Filter "Name='bash.exe'" | ? { $_.CommandLine -like '*rev-parse origin/main*' }`.
   The watch this session ran also tracked lane pushes (`origin/sandbox/{cam-smooth,road-repair,gait}`);
   add `sandbox/game-loop` and `feel/*`.
3. `ListAgents` and message every live `flight-sim2-*` session ONE packet: who you are, main's SHA,
   the queue, the checklist (§4). Ask each to name its lane. Silence is never a go — say so.
4. **Read §2 (the queue) before anything lands.** As of this writing L12 is gating and lands next.

## §2 THE QUEUE, as of 2026-09-12 ~00:30

| lane | session | branch / tip | state |
|---|---|---|---|
| **game-loop L12** (player-centric roster, 3 vs 7 always the player's side) | `flight-sim2-f3` | `sandbox/game-loop` `41ffe557d` (merge of `ca73158fe`) | **Chad-SIGNED, FULL GATE RUNNING**, pre-audited clean (§5.6). Lands next; fast-forward if main stays at `ca73158fe`. Expect 2046 legs. After it lands, f3 resyncs recon itself. |
| **this handoff** | sentinel | `sentinel/handoff-20260912` in `_sentinel` | docs-only; lands AFTER L12 so L12 keeps its fast-forward. |
| gait | offline | `sandbox/gait` `acb3651ed`, gated 6/1996 on 2026-09-08 | **PARKED.** 6 ahead, ~60 behind. Its verdict is stale (six landings of source since, including cam-smooth's edits in the same `app/main.cpp` ~9020-9100 band it touches). Needs a fresh main merge + re-gate + Chad's word. Its G2j-P2 field deletion (`work_on`/`work_blow_on`) is still the live hazard — see 0908 §2b. |
| cam-smooth, road-repair, kernel (feel), enemy-ai, sudburian-head | — | == main or docs-behind | dormant |

**Merge-order law, restated:** first lane DONE lands first; the second merges `origin/main` ONCE at
or past the first's SHA, union-merges (never take-ours), re-gates on the MERGED tip if the delta
carries source (docs/LANES-only deltas do not invalidate a verdict, but they do turn a fast-forward
into a merge commit — say which). Name the order in writing to BOTH lanes.

## §3 What landed these two nights (all audited PASS, all fly-tree-synced)

| main tip | what | lane / session | gate (by name) |
|---|---|---|---|
| `5ce9564b4` | cam-smooth: sled/walker/sting drawn from the sub-tick blend; lagged sled camera (`SEADS_SLED_CAM_TAU`); red-team fix `render::cam_lag_step` | cam-smooth / f3 | 6/1997 on `42fe44404` |
| `de523afd1` | road-repair: drawn==driven deck floor (`SEADS_DECK_FLOOR=0`), lit continuous banks (`SEADS_BANK_LIT=0`, `SEADS_NO_BANKS=1`), C0 corner + road/trail seam blend (`corner_blend_m`, `junction_station_m`) | road-repair / df | 6/2012 on `f0104a3ab` |
| `92f111e81` | road-repair handoff (docs) | df | — |
| `ec2e1a93a` | cam-smooth rung 2: chase cam ×0.75, `app/rest_horizon.h` extracted from `instructor_tick.h`, sting cam horizon, `[horizon_recovery] straight_max` 9→12, v13d test margin 3×→2× | cam-smooth / f3 | 6/2012 on `ae64218b5` |
| `85896a73a` | **KERNEL v14 S-leanlead**, tag `kernel-v14-leanlead-signed`: ONE dial `[auto_level] lean_lead 0.3`; red-team P0 folded (continuous same-sign-excess clamp); controller golden re-recorded | feel/yaw-bank-balance / `flight-sim2-07` in `D:\flight_sim2\seads-feel` | 6/2015 on `d9f6dc6ea` (`seads-feel/build/gate_d9f6dc6ea.log`) |
| `b74d35e44` | game-loop L11/b/c: CHOOSE YOUR SIDE, `render/team_kit.h` absolute colours (Sudbury always orange), faction spawn homes | game-loop / f3 | 6/2035 on `80c68a0f1` |
| `ed6a6ba7f` | kernel handoff §0 (docs/LANES) | 07 | — |
| `ca73158fe` | road-repair Onaping pass: drawn APRON past the bank skirt (`apron_m` 12, `SEADS_NO_APRON=1`); ribbon SUBDIVISION at drape (`max_seg_m` 8 / `max_tr_m` 5, `SEADS_RIBBON_MAXSEG=0`) — the mid-road sink on hills was flat chords between ~53 m GIS rungs floating up to +7.2 m above `drive_r` | road-repair / df | 6/2045 on `c5572c83e` |

**Fly tree** `D:\flight_sim2\seads-recon`, branch `sandbox/r4a-phase0`: `266a4e651` carries
`ca73158fe`, pushed, clean. **Chad's exe: `build-play/seads.exe` 2026-09-12 00:04:42.**
The only conflict that merge ever has is `generated/graph/*` (graph.json, sometimes a digest):
`git checkout --theirs` each, `python tools/graph/graphify.py`, `git add generated/graph`, commit.
Build: `cmake --build build-play --target seads -j 8` (~10 min, run it in the background). One
known warning: `on_road` set but not used in `app/main.cpp:2703` (road-repair's, harmless).

## §4 The audit checklist as actually run (one command, from `_sentinel`)

```
cd /d/seads_sandboxes/_sentinel && git fetch -q origin && git checkout -q --detach origin/main
OLD=<previous tip> NEW=$(git rev-parse HEAD)
git merge-base --is-ancestor $OLD $NEW && echo ff            # linear? if not, who merged what
git log --format='%h %s' $OLD..$NEW; git diff --stat $OLD $NEW
git diff --name-only <lane's last CODE commit> $NEW            # must be docs/LANES only
git ls-files --eol | grep -c 'i/crlf'                         # 0 (INDEX endings; see §5.4)
python -c "import tomllib;print(len(tomllib.load(open('LANES.toml','rb'))['lanes']))"; grep -c '^\[lanes\.' LANES.toml   # equal
PYTHONIOENCODING=utf-8 python tools/gate/lanes_lint.py LANES.toml
python tools/graph/graph_query.py check; python tools/graph/graphify.py --check-only
python <scratch>/owns_check.py D:/seads_sandboxes/_sentinel $OLD <lane>   # §4.1
awk '/^\[lanes\.<lane>\]/{f=1} f&&/^\[lanes\./&&!/<lane>/{exit} f' LANES.toml | grep -c <file>   # announce, RAW text
git diff -U0 $OLD $NEW -- app/main.cpp | python -c "<byte-read seam+hazard count>"            # §5.2
git diff --name-only $OLD $NEW | grep -E '^(sim|control)/|app/player_mode.h|app/interact.h|app/spawn_policy.h|combat/(pump|engine)_repair'
python tools/gate/gate_baseline.py check <lane worktree>/build/.gate_ctest.log   # BY NAME, on the CODE tip
stat -c '%y' <that log>; git log -1 --format='%ci' <code tip>   # log must POSTDATE the code tip by ~the gate length
```

### 4.1 `owns_check.py` (rewrite it; ten lines)
`tomllib.load` → for every file in `git diff --name-only OLD NEW` (skip `generated/`, `docs/`,
`LANES.toml`) print which lanes' `owns` fnmatch it, and whether the literal path appears in the raw
LANES text. Two known false negatives to read past: announces written in brace form
(`render/bank_mesh.{h,cpp}`) and short names (`load_controller`, `golden`, `yawbank`). Confirm those
with the `awk … | grep` line, never by eye alone.

### 4.2 Pre-audit BEFORE the push, audit AFTER
Every landing this run was pre-audited read-only in the lane's worktree while its gate ran, so the
post-push audit was a diff against a known tip (`git diff --name-only <pre-audited tip> $NEW` should
be `LANES.toml` only). It made every landing a two-minute check instead of a twenty-minute one, and
it caught road-repair's three SOP-5 stops (no registry block, snow-lane files unannounced, a dozen
undeclared files) an hour before they would have been a landing failure.

## §5 Defects and lessons from this run — the METHOD, not just the verdicts

1. **The "flew it" misquote I got wrong.** I flagged road-repair's LANDED line as misquoting Chad
   because the sentence he sent ME was different from the one in the registry. The lane had his word
   DIRECTLY in its own session. Two sessions can each hold a genuine, different verbatim. Before
   calling a quote false, ask whether the other session had its own channel. I retracted in writing.
2. **`app/main.cpp` has a literal NUL byte** (~line 6345), so `grep` calls it binary and reports 0
   or "Binary file matches". Every seam/hazard count on that file is a python byte-read or it is not
   evidence. (Found by road-repair; confirmed.)
3. **Gate log timing is part of the verdict.** Read the log's mtime against the code tip's commit
   time. A log OLDER than the code tip is a pre-check, whatever the message says. This run's logs
   were 57–63 min after their tips, matching the reported 3,400–3,800 s.
4. **CRLF: index vs checkout.** `seads-feel` (the kernel worktree) shows 319 `w/crlf` files with
   `core.autocrlf=true` while the INDEX is clean (`i/crlf` = 0) and a fresh checkout shows 0. What
   lands is the index. But a gate run on a CRLF checkout can read a source-text test differently
   than main will (the "CRLF checkout test class"). Tripwire = `i/crlf`; if a new red is a
   source-text test, blame the checkout before the code.
5. **Docs-only deltas do not invalidate a verdict; source deltas do.** Precedent set by cam-smooth
   on 2026-09-08 and applied four times since. Say which case applies in the packet.
6. **The free instrument still pays:** predict `ctest -N` on the merged tip (main + lane legs) and
   ask for it before the hour. Twice the number came back higher than my prediction (2035 not 2032;
   2011 not 2004) because another landing had added legs in between — that is the instrument
   working, and the lane explaining the delta by name is the check.
7. **A test margin loosened to admit a ruled dial** (cam-smooth rung 2: v13d arm-gate 3× → 2× after
   `straight_max` 12 made 3× physically unreachable). Honest, announced, the blocking assertion
   untouched — but name it to Chad every time; a margin relaxed to fit a ruling is the pattern that
   wants a human eye once.
8. **A frozen kernel landed by the book** (§3, `85896a73a`). The path: a feel-loop session, ONE
   dial, Chad flies, fresh-context red-team, gate on the exact tip, Chad's verbatim in the LANES
   kernel block, CLAUDE.md kernel line + flight-log row in the same landing, annotated tag at the
   pushed tip. That is now the precedent; hold the next one to it.
9. **Repo weight is an observation, not a stop:** road-repair committed 3.9 MB screenshots and a
   580 KB ctest log under `docs/`; on observation it halved the PNGs (0.74 MB) and trimmed the log
   to its 21-line verdict. Six 22k–35k-line census TSVs ride in `docs/road_repair/` as declared DATA.
10. **The sentinel's own held docs** (three commits from 2026-09-09) rode to main inside L11's
    landing, which is fine for docs but means a lane's `git log OLD..NEW` can show commits that are
    not the lane's. Read the author line and the subject; it was announced.

## §6 Rulings still owed by Chad (ask once, together)

Unchanged from the 0908 doc §5: `sim/` wording vs `sim/gait.{h,cpp}`; where `fix2_workpad/` lives
(it is still untracked at the head lane's root, with `docs/SESSION_HANDOFF_20260907_scarf_landed.md`
also untracked there); the retired legacy rider in the export contract. New: **gait** — land it
after a fresh merge + re-gate, or retire it? It has been parked since 2026-09-09.

## §7 Open items nobody owns yet

- **No snowmachine sound in seads-recon** while the same source + soundbank sounds in the cam-smooth
  lane exe (f3 was capturing recon's `[AUDIO]` load prints). Both trees carry the same 19 files
  under `assets/audio`; the 54 MB soundbank is deliberately uncommitted (`tools/install_soundbank.sh`).
- The SNOWDUMP writer in `app/main.cpp` has the same `fopen("w")` CRLF bug the census writer had
  (road-repair fixed theirs; left this one for its owner).
- `docs/SESSION_HANDOFF_20260907_scarf_landed.md` — written, never committed, sits untracked in
  `D:\seads_sandboxes\sudburian-head-lane`.
