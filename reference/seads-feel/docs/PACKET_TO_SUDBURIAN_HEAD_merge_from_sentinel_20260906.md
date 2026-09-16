# PACKET — to the sudburian-head lane (scarf fixes), from the SENTINEL, 2026-09-06 evening

Merge packet for `sandbox/sudburian-head` @ `cd22abdd3` (5 commits since your landed base
`cafe8482f`; behind `origin/main` by 24). Read against `D:/seads_sandboxes/sudburian-head-lane`.
Observations from the tree, not diagnoses. Chad's word: "done their work too … also did some
fixes on the scarf … send them a merge packet."

## 1. What you are landing (merge-base diff, the honest list)

| file | change |
|---|---|
| `assets/sled/indy650.glb` | FIX2 wrap re-patch (prim2 restored from cafe8482f, coat-kNN blend, bind-pose compensation), same byte size |
| `render/sled_model.cpp` | +102 −? (idle chatter latch / flutter onset; r4a's file) |
| `render/trail_chain.cpp` | +39 (rest_push inelastic contact in the chain solver; the SCARF lane's file) |
| `docs/SESSION_HANDOFF_20260905_scarf_triplefix.md`, `docs/SESSION_HANDOFF_20260906_scarf_fix2.md` | handoffs |
| `LANES.toml` | +4 (own section) |
| `fix2_workpad/**` (12 files, ~5.6k lines: consult brief/report, build spec/report, two .py, three .json) | session scratch, committed at the REPO ROOT |
| `generated/graph/*` | regenerated (current: `--stale` quiet) |

## 2. Three things that stop the landing as it stands

**2.1 CRLF tripwire — 3 files.** `git ls-files --eol | grep w/crlf`:
`fix2_workpad/glb/fix2_provenance.json`, `verify_fix2.json`, `verify_fix2_adj60.json` are
`i/lf w/crlf`. The gate hook fails on a non-zero count before ctest. Fix from the committed
tree: `git rm --cached -r . -q && git reset --hard` (the recorded recipe), then re-check = 0.
(If §2.2 removes the directory, this resolves itself.)

**2.2 `fix2_workpad/` at the repo root.** A lane's scratchpad copied into the tree root is the
flak lane's recorded trap (`fc7630c55` in the gait lane untracked exactly this class). The
consult/build reports are worth keeping; the JSON batteries and patch scripts are evidence, not
source. Sentinel's recommendation, Chad rules: move the four `.md` reports under
`docs/scarf_fix2/`, keep `patch_wrap_fix2.py` / `verify_wrap_fix2.py` under
`assets/character/sudburian_src/` beside the other asset tooling if they are the reproducible
patch path, and leave the three JSON batteries OUT of the repo (they live in your sandbox dir).
Whatever the ruling, nothing at the root.

**2.3 Your own handoff's gate.** `SESSION_HANDOFF_20260906_scarf_fix2.md` §1 records Chad's
ruling for this redo: "NO LANDING TO MAIN without his drive." The registry must carry his
sign-off on FIX2 + the parked-still fix in `status` (his words, dated) before the merge —
the sentinel cannot supply it. If he has flown `cd22abdd3` and signed it, write that line; if
he has not, this packet is the merge PLAN and the landing waits for the drive.

## 3. Announce (SOP §5) — `LANES.toml [lanes.sudburian-head]`

`as_of` is still 2026-09-05 and `status` predates the scarf work. Name, in `status`:
`render/sled_model.cpp` (r4a's), `render/trail_chain.cpp` (the scarf lane's — sandbox/scarf-drape,
LANED 2026-09-04; they are at main with nothing unpushed, so no merge collision, but the file is
theirs and the change to the chain solver's contact law is a BEHAVIOUR change for every chain
that solver runs: flak gunner scarf, rider scarf), `assets/sled/indy650.glb` (patched a second
time). Keep them OUT of `owns` (you did last time; keep it so).

## 4. Merge order — the gait lane goes first

Two lanes are landing `render/sled_model.cpp` tonight. The GAIT lane (`sandbox/gait`, 19+
commits, pose_pass reshaped, +1269 lines in sled_model.cpp) was cleared first and is mid-dance
on main `09defb8fa`. You go SECOND: wait for the gait landing SHA (I message it), then merge
main ONCE and union-merge your +102 beside theirs. Since your base you also need to absorb the
sting ST-5 landing (`b80961064`: sled_model.{h,cpp} hand hook / sit-up / twist, draw.{h,cpp},
map_screen.cpp, new render/sting_* + launcher_model, test_sting_pose) and the loop L9
(`09defb8fa`: draw.cpp pump block, draw.h ConquestPump::foot, main.cpp). Your `git diff
origin/main` shows those as deletions because your base predates them — union-merge, never
take-ours.

## 5. Landing sequence (the sentinel checks each)

1. §2.1–§2.3 resolved; `LANES.toml` §3 written; `tomllib` parse proven (each lane once).
2. Wait for the gait SHA. `git fetch`; merge `origin/main`; regenerate the graph in the merge
   commit (`--stale` quiet); `git ls-files --eol | grep -c w/crlf` == 0.
3. Full gate on the MERGED tip, detached, runner verdict: red set EXACTLY the baseline by name.
   The chain-solver change touches the scarf/flak subsets — a fresh red there is yours to
   attribute, never to record.
4. Confirm `origin/main` has not moved; merge `--no-ff`; push branch + main; ping me the SHA.
   I diff it against §1 + §3, sync `seads-recon` (sandbox/r4a-phase0) and REBUILD its
   build-play exe (code + asset).

## 6. Rules in force

Own work only — the merge commit carries your files plus the merge, nothing else. Never work in
`D:\flight_sim2\seads-recon`. The 42-bone Sudburian is the live rider; a full re-export of
indy650.blend crashes the mount — surgical GLB patch only (you know this; it is restated for
whoever reads this next).
