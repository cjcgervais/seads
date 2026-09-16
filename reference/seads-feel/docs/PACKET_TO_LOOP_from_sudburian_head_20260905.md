# PACKET → game-loop lane (SENTINEL) — sudburian-head landing notice, 2026-09-05

Per the 2026-09-04 sentinel protocol (your LANES.toml in_flight: audit that
every lane's push to main is its own work only).

## What lands
Branch `sandbox/sudburian-head` tip `a5ed28837` (merge --no-ff to main after
the full gate stamps this exact tree green; the landed SHA will be appended
below). **Chad SIGNED the rung 2026-09-05** ("good job we can call this done")
after three fly rounds (size ×2 refit, −10% cheek fix, flutter-rate fix).

## Files touched — all announced, all this lane's own
- `assets/sled/indy650.glb` — the surgical head patch (8 new skinned nodes on
  skins[0]; 44 joints/IBMs/TRS untouched; legacy stub shrunk in place).
- `render/sled_model.cpp` — COLOR_0 vertex-colour support (constant-white
  default = bit-identical for every existing prim) + the mullet wind vertex
  pass (SEADS_MULLET=0 kill; phase rate rides airspeed).
- `render/flak_gunner.cpp` — the 8 head-part names added to the gunner's
  draw allowlist (it is an explicit allowlist; rider drawer is automatic).
- `assets/character/sudburian_src/blend_snapshots/sudburian_head_20260904.blend`
  — Chad-ruled snapshot.
- `docs/SESSION_HANDOFF_20260904_head_glb.md`, this packet, LANES.toml (own
  section), `generated/graph/*` (regenerated same-commit per gate law).

⚠ `render/sled_model.*` is listed under lanes.r4a's owns — the head rung's
edits there were required by the rung (loader + shader + skinning loop are the
only place a GLB attribute or a vertex pass can land) and are additive:
COLOR_0 is dormant without the attribute, the mullet pass is keyed to node
name `sudburian_mullet` + env kill. Flagging it here rather than leaving it
for your diff to find. r4a lane: observation, not a diagnosis — if your
worktree carries uncommitted sled_model.cpp edits, merge order matters.

## Loop-breakage watch (your standing checklist)
J-mount exercised in three fly rounds — no fatal. Flak F-pose covered (gunner
allowlist). Y dent cycle untouched (dent table keys on helmet names only).
Scarf drape untouched — head verts sit above the bind scans' torso band
(t > 1.1 vs cutoff 1.05, measured). Fly tree seads-recon NOT synced by this
lane (your protocol step).

## Landed SHA
- **LANDED: main `c5ff368b1` 2026-09-05** (merge --no-ff of lane tip `7d0d92d70`; gate on that tip: red set == baseline six BY NAME, 1920 tests; base main `62047e67c` confirmed unmoved at merge time).
