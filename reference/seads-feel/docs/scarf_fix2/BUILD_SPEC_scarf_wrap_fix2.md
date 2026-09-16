# BUILD SPEC — scarf wrap FIX2 (approved design, 2026-09-06 overnight)

You are the builder. The design is decided; build it exactly. Read first:
1. `CONSULT_BRIEF_scarf_wrap.md` (context + measured recon) — same folder.
2. `CONSULT_REPORT_scarf_wrap.md` (the approved design §B.1, battery §C, risks) — same folder.

Worktree: `D:\seads_sandboxes\sudburian-head-lane` (branch sandbox/sudburian-head,
tip e1a087523). Work ONLY there and in this scratchpad. Do NOT run `git commit`
or `git push` — the mastermind commits after verification. Do NOT touch any
other worktree (esp. D:\seads_recon = Chad's fly tree) and never kill processes
outside this worktree's path.

House rules that bite here:
- Inline python heredocs get mangled in this shell — ALWAYS Write scripts to the
  scratchpad as files and run the file. A failed python does NOT stop a `;` chain —
  check exit codes.
- MSYS grep/cat lie about CRLF; byte-read when line endings matter.
- Reuse/extend the consult's scripts in this scratchpad (`poselbs.py`,
  `simfix2.py`, `simfix3.py`, `measure2.py`, `rest_vs_bind.py`) — they hold the
  calibrated rig/LBS core. The production patch + verify scripts must be clean
  standalone files: `patch_wrap_fix2.py` and `verify_wrap_fix2.py`.

## Step 1 — D0 (the compensation palette)

Read the pose pass in `render/sled_model.cpp` (search `nd.local = compose` /
ws channel application) and determine whether, PARKED at a mid seat station, the
additive channels (ws hinge, ws_neck_counter, arm IK, head look) are identity —
i.e. whether the engine's parked driven pose == the GLB node-rest pose.
- Write your finding with line numbers into the build report.
- Either way, D0 for the patch = the GLB node-rest worlds × IBMs (the consult's
  fallback). If you find parked channels are NOT identity, quantify the residual
  if you can and note it — do not block on it.
- ALSO add the env-gated one-shot palette dump to the CPU skinning loop
  (`SEADS_RIG_PALETTE_DUMP=path`): on first skinned frame, write all 44 joint
  palette matrices (world×IBM) as JSON to the given path, then never again.
  Follow the existing env-gate idioms in the file (static const lambda pattern).
  Instrumentation only — zero behavior change when unset. This is for Chad-drive
  validation (consult risks R1/R2) and future re-pinning.

## Step 2 — the patch (`patch_wrap_fix2.py`)

Inputs: the pre-patch GLB **extracted fresh from git** (`git show
cafe8482f:assets/sled/indy650.glb`) — never the working-tree file (law: never
re-patch an already-patched prim2). Assert the extraction matches
`scratchpad/glb/prepatch.glb` byte-for-byte before proceeding.

Per consult §B.1, on mesh `sudburian_proxy_R2cM.002` prim2 (908 verts):
1. Eligibility: bind Y < 1.29, zero chain-bone weight (scarf_01..06,
   scarf_s01/s02), nearest coat (prim3) vert within 90 mm in bind space.
   Eligibility/pairing ALWAYS from the cafe8482f authored positions. (~513 verts.)
2. Target blend: k-NN k=4 inverse-distance over coat verts, full joints+weights,
   arm terms included; restrict candidates to coat verts with
   dot(bind_normal_coat, bind_normal_wrap) > 0 (risk R3 fold guard).
3. Ramp: r = (1 − smoothstep(1.25, 1.29, bindY)) × (1 − smoothstep(0.045, 0.090, d_coat)).
   blend_new = normalize(r·coat_kNN + (1−r)·authored), truncated to top-4
   joints, renormalized (Σ=1 ±1e-3).
4. Position+normal compensation: p_bind' = M_new(D0)⁻¹ · M_old(D0) · p_bind,
   normal rotated by the same matrix pair (inverse-transpose for non-uniform
   parts; these are rigid-ish, but do it right), where M_old/M_new = the vert's
   blended palette matrices under D0.
5. NO proud offset.
6. Surgical same-count byte patch: only prim2's POSITION / NORMAL / JOINTS_0 /
   WEIGHTS_0 accessor bytes change. Knot verts (bind Y ≥ 1.29) bit-identical.
   Node count must remain 226. All other prims and meshes bit-identical.
7. Write the patched GLB to the worktree `assets/sled/indy650.glb` AND keep a
   copy at `scratchpad/glb/fix2.glb`. Emit a JSON sidecar
   `scratchpad/glb/fix2_provenance.json`: base commit, vert lists, ramp params,
   D0 source, per-band counts.

## Step 3 — verification (`verify_wrap_fix2.py`), gates from consult §C

Run the FULL battery on prepatch vs failed(current at 183da606a, kept at
`scratchpad/glb/current.glb`) vs fix2. Report all three columns so the gates
are meaningful. Poses/deltas composed on D0:
- P0 driven-neutral: every prim2 vert of fix2 within 2 mm of prepatch posed
  position (mean ≤ 1 mm). (Failed patch must score ~26/125 mm here — if it
  doesn't, your instrument is wrong, stop and re-derive.)
- Seat-station sweep θ ∈ {5,10,15,20,25,30,38}°: pelvis +θ about model X,
  neck_01 −min(θ,30°), upperarms −θ toward grips (use poselbs.py calibration).
- Flak grid: neck −10/−20/−30° × arms −30/−45/−60°.
- Head-cam: head yaw ±45°, pitch ±30° — must be a no-op on prim2 (catches
  head-weight pickup from coat kNN near the hood).
- Arms-only ±35°.
Metrics/gates (changed + transition verts; the 11 chain-carrying coat-adjacent
verts excluded from gates, reported as observations):
- clearance drift vs P0 along POSED coat normal: full-blend |drift| ≤ 8 mm all
  poses; partial-r ≥ −20 mm floor, mean ≥ −5 mm.
- absolute penetration: no changed vert < −20 mm any pose; count < −5 mm ≤
  prepatch neutral count.
- hem-top↔knot-rim gap growth vs P0: mean ≤ +3 mm, max ≤ +8 mm every pose.
- tear detector: posed stretch of bind-neighbour pairs (<12 mm) ≤ 1.5× the
  prepatch value in the same pose.
- byte gates as in step 2.6, plus weights normalized, ≤4 joints.
- R6: confirm the mullet wind vertex pass does not touch prim2 (read the CPU
  skinning loop; state line numbers).

## Step 4 — repo hygiene + build

1. `python tools/graph/graph_query.py` — regen the code graph per repo SOP
   (graphify) AFTER the sled_model.cpp edit, so the graph matches source
   (read tools/graph usage; the lane law: regen in the last commit before
   gating — leave the regenerated files in the tree for the mastermind's
   commit).
2. Full gate: `python tools/gate/gate_baseline.py check` from the worktree —
   red set must equal the baseline six BY NAME. Let the runner write the
   verdict; do not summarize it yourself from partial output.
3. Rebuild the play exe: the build dir is `build-play` (ninja). Build the
   `seads` target (and tests if the gate needs them). Confirm
   `build-play/seads.exe` timestamp updates and it links clean.

## Step 5 — build report

Write `scratchpad/BUILD_REPORT_scarf_wrap_fix2.md`: step-1 pose-pass finding,
patch stats (verts touched per band, weight moved), the full three-column
battery table with pass/fail per gate, gate_baseline verdict verbatim, exe
build result, and anything that deviated from this spec and why. Your final
message = a short summary + pointer to the report.

If ANY battery gate fails and one re-derivation doesn't fix it: fall back per
consult §B.2 — restore the plain cafe8482f asset into the worktree (the honest
revert), still do steps 4–5 (code change ships regardless: dump is
instrumentation; report the failure honestly). Do not iterate past 2 attempts.
