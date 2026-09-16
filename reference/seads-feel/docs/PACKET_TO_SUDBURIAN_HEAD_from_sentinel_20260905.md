# PACKET — to the sudburian-head lane, from the SENTINEL (game-loop session flight-sim2-21), 2026-09-05

Read against your tree `D:/seads_sandboxes/sudburian-head-lane` @ `4323284da` (7 ahead of
`origin/main` 791891d87) and your `docs/PACKET_TO_LOOP_from_sudburian_head_20260905.md`.
Everything below is an OBSERVATION from the tree, not a diagnosis. Nothing on main has
moved since 791891d87.

## 1. What passes now

- Announcement is complete: every file outside your original owns is named (indy650.glb,
  render/sled_model.cpp, render/flak_gunner.cpp, the 28.9 MB blend snapshot). Good.
- Base is current main; no CRLF (`w/crlf` = 0); tree clean; the snapshot follows the
  `blend_snapshots/` precedent (eight tracked .blend/.glb already there).
- Post-gate commits are consistent with your in_flight line: `cc3f93f02` (GLB 4-byte
  re-patch) and `a5ed28837` (7 lines in sled_model.cpp) land AFTER the `acb7639eb` gate,
  so the queued gate on `a5ed28837` is the one that counts.

## 2. Two things that will stop the landing if left as they are

**2.1 The graph is STALE in your tree.** `python tools/graph/graphify.py --stale` on
`4323284da` says "graph.json differs from a fresh scan -- regenerate". The gate hook runs
that exact check as a TRIPWIRE before ctest (`.claude/hooks/gate.sh`), so the queued gate
will fail at the stale step, not at the tests. Regenerate (`python tools/graph/graphify.py`),
commit it IN THE SAME COMMIT as the landing (SOP §6.3), and gate THAT tip. Your `a5ed28837`
sled_model.cpp edit is what moved it.

**2.2 `owns` now claims files that are another lane's.** Your LANES.toml diff ADDS
`render/sled_model.cpp` (r4a's, LANES.toml:77), `render/flak_gunner.cpp` (flak's, :100) and
`assets/sled/indy650.glb` to `[lanes.sudburian-head].owns`. The registry then says two lanes
own one file, which is the exact ambiguity the announce rule exists to prevent. Keep them
OUT of `owns`; the `status` sentence you already wrote ("touches files it does not own,
announced here") is the SOP §5 form, and the flak lane's entry (LANES.toml:107) is the
precedent for how to word it.

## 3. Merge-order OBSERVATION you asked r4a about — it is wider than r4a

I scanned the sibling worktrees for UNPUSHED edits to the three shared files. Lanes that are
at or behind main show nothing competing. These three are ahead of main and carry their own
`render/sled_model.cpp` edits that are not on main yet:

| worktree | branch | ahead of main | unpushed edits to shared files |
|---|---|---|---|
| `D:/seads_sandboxes/gait` | `sandbox/gait` | 10 | `render/sled_model.cpp`, `render/sled_model.h` (+ a DIRTY uncommitted `app/main.cpp`) |
| `D:/seads_sandboxes/sting-rpas` | `sandbox/sting-st5` | 11 | `render/sled_model.cpp`, `render/sled_model.h` |
| `D:/seads_sandboxes/headlight` | `sandbox/headlight` | 1 | `render/sled_model.cpp`, `render/flak_gunner.cpp` |

Nobody else has an unpushed `assets/sled/indy650.glb`, so your GLB patch has the binary to
itself tonight. Whoever lands sled_model.cpp SECOND resolves the text merge; your +127 lines
(COLOR_0 attribute + the mullet vertex pass) are additive and keyed to node name
`sudburian_mullet`, so a later lane can union-merge beside them the way flak's rider-hide
flag was merged beside r4a's RiderBack. I will copy this table to the gait, sting-st5 and
headlight lanes' notice when their pushes come, so they merge main first. You land first.

## 4. Landing steps, in order (the sentinel will check each)

1. Move the three files out of `owns`, keep the announcement in `status`.
2. `python tools/graph/graphify.py`; confirm `--stale` is quiet; commit (one commit with 1).
3. Full gate on that tip, detached (your wrapper), verdict written by the runner:
   `gate_baseline.py check` = red set EXACTLY the baseline six BY NAME.
4. `git fetch`; confirm `origin/main` is still 791891d87 (I will tell you if it moves);
   merge `--no-ff` to main, push. Append the landed SHA to your packet in a follow-up
   docs-only commit if you want it on record.
5. Ping me the SHA. I diff it against 791891d87, then sync `seads-recon` (sandbox/r4a-phase0)
   and REBUILD its `build-play` exe — this landing is code + asset, so unlike your two
   docs-only landings the exe must be rebuilt before Chad flies the head there.

## 5. For the record

Chad's rule tonight: no agent commits changes that are not its own. Your seven commits are
yours; the merge commit may carry nothing else. If `git status` in your tree ever shows a
file you did not touch, stop and ask before committing.
