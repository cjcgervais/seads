# Sudburian head — lane handoff (2026-09-04)

## What exists
A sculpted hero head for the Sudburian rider, built live in Blender against
Chad's turnaround reference sheet, iterated ~15 rounds on his live viewport
feedback and signed off feature-by-feature in session.

**Source of truth (NOT in the repo, by binary policy — no .blend is tracked;
same ruling as the 54 MB soundbank and indy650.blend):**

- `D:\seads_sandboxes\sudburian_head\sudburian_head.blend` (28.9 MB)
- Reference sheet: `D:\seads_sandboxes\sudburian_head\Gemini_Generated_Image_f7q1e9f7q1e9f7q1 (1).jpg`

## Contents of the .blend (scene `sudburian_head`)
- `head_skin` — one fused mesh (~8k verts), metres, nose faces −Y, head centre
  ~z 0. Sculpted: wide jaw carried down the jawline, square protruding chin,
  cheekbones, nose with flat bottom shelf + bridge ridge + nostril flares,
  smile with parted lips, occipital curve, trapezius neck. Vertex-color layers:
  `flush` (windburn cheeks/nose), `lipmask` (rose lips), `shade` (nostril +
  ear-concha shadow) — all mixed in the `skin` material.
- `scalp` — buzz-cut shell cut from the head surface (live shrinkwrap onto
  `head_skin`), nape hairline arched, stubble bump in the `hair` material.
- `goatee` — reduced chin patch (bottom front + under-chin shelf middle),
  normal-based cut that stops at the vertical neck wall; combed front-to-back
  strand texture (`goatee_combed` material, wave bump + strand striping).
- `mullet` — draped back curtain, full scalp width, curly/wispy (two displace
  layers), thickness fades from 0 at the top (helmet-safe) to full at the
  bottom via the `fade` vertex group driving solidify + both displaces.
- `mullet_rig` — 7-bone armature: pinned `mullet_root` + three 2-bone chains
  (L/M/R) down the lower curtain. Weights: pinned above z −0.055, free by
  −0.105; middle chain ~1.6× amplitude (the helmet's rear opening). Wind =
  two NOISE f-curve modifiers per rotation channel (scale 14 ripple + scale 55
  bow), phase-offset per chain/bone, plus a steady backward lean. 240-frame
  loop, plays in-viewport.
- Glasses: `frame_curve` (ONE continuous NURBS tube: temple→hinge→brow→bridge
  →mirror) + two rounded Oakley-style lenses. `teeth`, `verify_cam` (my render
  camera), lights.
- Helmet-fit invariant: every face-widening pass was banded below the brow —
  the cranium dome the helmet wraps is dimensionally untouched from the first
  fuse.

## Next steps (not started)
- GLB export/integration: per the two-riders rule the 42-bone Sudburian is the
  live rider and full re-exports of indy650.blend CRASH the mount — head
  geometry must land via the surgical GLB patch path the r4a lane documented,
  not a whole-file export. Needs its own session + ruling on how the head
  joins the rider.
- Engine-side mullet wind: the Blender noise rig is the motion spec (bone
  names/amplitudes above); wiring to the real wind like the scarf is
  engine-side asset-pass work.
- Eye height: this asset does not itself change the rider's seated eye height
  (rig/pose owns that). The Sting seat-launch placeholder (1.35 m,
  `main.cpp` sting_stance) is the animation lane's to measure.
