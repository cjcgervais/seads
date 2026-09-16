# SESSION HANDOFF 2026-09-04 — Sudburian head GLB integration rung

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260904_head_glb.md; do §4."

## §1 Mission
Get the new Sudburian head — sculpted, approved feature-by-feature by Chad on
2026-09-04 — onto the LIVE 42-bone Sudburian rider in the game, replacing the
rider's current head, with the mullet's lower half wind-animated like the
scarf (scaled down; the middle of the curtain freest, matching the helmet's
rear opening).

## §2 Source of truth
- Head sculpt: `D:\seads_sandboxes\sudburian_head\sudburian_head.blend`
  (28.9 MB, scene `sudburian_head`). Full contents inventory:
  `docs/SUDBURIAN_HEAD_HANDOFF.md`. Reference sheet jpg sits beside it.
- Rider/machine source: `Game_loop_idea/vehicle_program/blender/indy650.blend`
  — Chad's RUNNING Blender session on it is the truth; connect over MCP
  (`tools/blender/mcp_startup.py` socket) and READ THE INVENTORY first.
- Shipped asset: `assets/sled/indy650.glb` (1 skin, 44 joints, Sudburian only).

## §3 The laws (each bought with a dated failure — violate none)
1. **TWO RIDERS** (`docs/RIDER_AUTHORITY.md`): the 42-bone `sudburian_rig` is
   the ONLY rider under development; the 19-bone legacy `rider_*` set is
   FROZEN (hide_render'd in the blend). Tell them apart by BONE COUNT, never
   name. Two agents have already built on the wrong rider, all tests green.
2. **NO FULL RE-EXPORT of indy650.blend** — the blend's rest skeleton has
   DIVERGED from the shipped GLB (15/44 joints rotated, CH_cam off 1.28 m);
   a full export FATALs the mount-time R3-WS pose gates = exe closes on J.
   Ship geometry via the **surgical GLB patch** path only: base = committed
   GLB, transplant same-count mesh buffers, append-and-repoint accessors for
   re-meshed parts, node TRS edits explicit. Working script precedent:
   `patch_glb.py` (headlight lane, scratchpad of session 3e7151c6; needs
   numpy → run inside Blender). The loader reads `skins[0]` ONLY — a second
   skin renders as a mangled ghost.
3. **`export_indy650.py` verify() is STALE** — it demands the legacy rider and
   fails 22 checks against the real asset. Do not "fix" the export to satisfy
   it; the contract rewrite needs Chad's ruling.
4. **Blender in the GUI, never headless; ONE writer per session; never a
   second Blender on indy650.blend; never kill Chad's session.** The head
   .blend is a SEPARATE file — open it in a separate step or append/link its
   objects; never overwrite Chad's files (copy aside first).
5. **Helmet fit:** the head's cranium dome was kept dimensionally untouched
   through every widening pass precisely so the shipped helmet still fits.
   Do not scale the dome to "fit better"; if the helmet clips, measure and
   report, don't resize.
6. Kernel firewall as always: nothing in `sim/`/`control/`.

## §4 The rung, in order (each step gated before the next)
1. **Recon (read-only):** connect to the running Blender (or open the
   committed GLB read-only). Measure the CURRENT rider head: which mesh(es),
   which bones drive it (head/neck joint names in the 44-joint skin), vert
   counts, materials, and how the helmet + balaclava + scarf sit around it.
   Also measure the head sculpt's scale vs the rider (the sculpt is ~0.26 m
   head height, real scale — verify the rider's head is the same scale).
2. **Ruling questions for Chad (ask BEFORE building):**
   a. Replace the rider's head geometry outright, or keep the game head and
      swap at the neck seam? Where is the seam (balaclava line?)?
   b. Does the bare-head version ship (glasses+goatee visible under helmet
      visor?) or is this the no-helmet/menu/close-up head?
   c. Mullet bones: add 6 new bones to the skin (grows the joint set 44→50 —
      touches the skin, HIGH risk vs law 2) or drive the mullet procedurally
      engine-side like the scarf (NO new bones — scarf precedent, lower
      risk)? Recommend the scarf path; the Blender rig then serves as the
      motion spec only (amplitudes/phases in SUDBURIAN_HEAD_HANDOFF.md).
3. **Fit pass in Blender:** append the head objects into a WORKING COPY,
   snap to the rider's neck/head joint, weight the head to the existing
   head/neck bones (posed-not-rest measurement — sides never mirror), bake
   the shrinkwrap shells (scalp/goatee/mullet modifiers) to meshes for
   export.
4. **Surgical patch:** extend the patch_glb.py pattern — new head meshes are
   NEW primitives (append-and-repoint accessors), skinned to EXISTING joints
   (law 2c above decides the mullet). Verify node set + TRS identical to
   committed except what you explicitly changed.
5. **Mullet wind (if scarf path ruled):** engine-side pass like the scarf
   drape (scarf lane precedent, landed 5029294e9) — bottom half only, middle
   column ~1.6× amplitude, two frequencies (fast ripple + slow bow), steady
   backward lean. ⚠ `world_override` composition-order lesson from the
   tie-rod solve applies to anything composed after pose_pass.
6. **Gate + fly:** build-play exe in YOUR lane worktree (never seads-recon),
   J-mount must NOT fatal (the R3-WS gates are the tripwire the divergence
   trips), `gate_baseline.py check` — red set == baseline BY NAME (extract
   names; "None failed of None" = the parser failed, not all-fixed). Then
   Chad flies; the fly checklist goes IN the reply with the absolute exe
   path, built for him.

## §5 Precedent correction (vs SUDBURIAN_HEAD_HANDOFF.md)
That doc says "no .blend is tracked" — WRONG on current main:
`assets/character/sudburian_src/blend_snapshots/` tracks .blend snapshots.
The head .blend MAY be snapshotted there if Chad wants it in-repo (~29 MB;
ask, don't assume). The live working .blend stays where it is either way.

## §5b RUNG EXECUTION LOG (2026-09-04 session, this rung BUILT)
Chad's §4.2 rulings, all taken in-session: (a) stub head REPLACED OUTRIGHT,
seam at the neck_01/head weight boundary; (b) bare head SHIPS VISIBLE under
the visor; (c) mullet = ENGINE WIND scarf-style, NO new bones (skin stays 44);
(d) head .blend SNAPSHOTTED to blend_snapshots/sudburian_head_20260904.blend.

What was built:
- **Fit pass** in a working copy (`sudburian_head_glbfit_WORKING.blend`,
  Blender on port 9877 — 9876 belonged to the Sting lane's live session):
  committed GLB imported as the reference skeleton (the diverged .blend rig
  never consulted), head placed on the POSED head bone (10° X tilt), raised
  0.045 m along the bone; helmet clearance ≥ 5.9 cm cranium-wide; shells
  (scalp/goatee/mullet/glasses) baked to meshes, mullet's ARMATURE modifier
  dropped per ruling (c).
- **Surgical patch** (scratchpad `patch_glb_head.py`, pure python): 8 new
  nodes on skins[0] (`sudburian_head_skin/_teeth/_scalp/_goatee/_mullet/
  _glasses/_lens_L/_lens_R`), bind positions via inverse LBS of the FILE's own
  posed joint matrices (round-trip re-import error 1e-5 m); the 24 legacy stub
  verts collapsed same-count inside prim0; joints/IBMs/TRS untouched; face
  grade (flush/lipmask/shade × the skin material's mix chain) baked to COLOR_0.
- **Engine** (render/ only): COLOR_0 → Mesh.colors + `vertexColor` in the sled
  shader (prims without colours get the driver's constant white — exact
  multiply, bit-identical); mullet wind vertex pass riding the CPU skinning
  loop (fade + 1.6× middle column baked from the bind curtain at load, ripple
  ~3.2 Hz + bow ~0.7 Hz + steady lean, wind = -velocity like the scarf,
  `SEADS_MULLET=0` kill); flak_gunner.cpp allowlist extended with the 8 names.
- **Verified safe by recon**: R3-WS mount FATALs check joint kinematics only;
  scarf back-surface scans band at t ≤ 0.98/1.05 on the torso axis and every
  head vert sits at t > 1.1; rider-hide behaviour inherited via `skinned`.

Open after this session: Chad's fly (J-mount, face under visor, mullet
flutter at speed, helmet dent cycle Y), then land to main per §6.

## §6 Process
Lane SOP is `docs/CONTRIBUTION_SOP.md`; register/refresh the
`[lanes.sudburian-head]` section in LANES.toml, land after coordinating with
whatever sentinel lane is active (2026-09-04 protocol: message the game-loop
lane, land after theirs, ping the SHA for the sentinel check, they sync
seads-recon). Blender 5.1 note: action f-curves live at
`act.layers[0].strips[0].channelbag(slot)`, not `act.fcurves`; bevel modifier
width is in LOCAL units.
