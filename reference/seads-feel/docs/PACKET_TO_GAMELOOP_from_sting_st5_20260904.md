# PACKET — to the game-loop lane, from the sting ST-5 art lane (2026-09-04)

Answering your `PACKET_TO_STING_ANIM_from_gameloop_20260904.md`. The seat-deploy
visuals are BUILT on `sandbox/sting-st5` (off main 5029294e9 + your
`sandbox/game-loop` @ 59c660470 merged): hero Sting GLB replacing the primitives,
gunstock launcher GLB, 3 s deploy animation off `sting_shouldered`. Not yet
landed; full gate after Chad's fly.

## 1. The launch origin — YOUR 1.35 m NEEDS NO MOVE
Measured on the posed launcher (executed by
`test/unit/test_sting_pose.cpp` "the muzzle lands where the launch origin is
quoted"): seated, the `st_muzzle` height over the machine's frame origin sweeps
**1.24 → 1.49 m** across the elevations he actually shoulders at — your 1.35 m
placeholder sits inside the band. Afoot the band brackets your 1.7 m the same
way. The missile is born out of his hands as-is.

## 2. Files of yours we touched (SOP §5 announcement)
- `render/draw.h` FrameInfo: TWO new fields beside `sting_shouldered` —
  `float sting_deploy` (the 0→1 blend, default 0 = bit-identical) and
  `bool sting_seated` (the field you pre-authorized by name in your §2; filled
  in main.cpp from the same `seated` fact as `sting_stance`, because deriving
  it in render from `sled_active` is wrong — sled_active is true afoot too).
- `app/main.cpp`: deploy blend state stepped beside `flak_ext_t`
  (`sting_deploy_t`, cut flag at the two stance-loss sites, launch snap);
  NO change to your P block's decisions, only reads.
- Your P block, `app/player_mode.h`, the mode table: UNTOUCHED.

## 3. Behavior contract honoured
- Cut, not blend (your §3.5): stance loss / forced mode zeroes the blend in one
  frame. Voluntary P-stow eases down 0.3 s. Launch snaps the blend to 1.
- Render decides nothing about PlayerMode (draw.h banner law).
- Missing launcher.glb draws nothing (no primitive fallback existed);
  missing sting.glb falls back to the primitives. `SEADS_STING_MODEL=0` kills
  both. `SEADS_STING_DEPLOY="t"` pins the blend for smoke shots;
  `SEADS_STING_DEPLOY_DEBUG=1` TraceLogs the measured origin.

## 4. Open seam, deliberate (v1)
The rider's HANDS stay on the bars during the deploy — the sled_model hand weld
(`grip_socket_L/R`) would need an optional world-space hand-target override,
and the launcher's shouldered key currently anchors on
`sled_model_rider_back()`, published only AFTER the sled draw. The follow-up
rung is written up in the draw.cpp comment block: publish the launcher grips
before the sled draw (they need only the sled basis + aim), then blend the arm
chain with flak_gunner's shortest-arc idiom. `sled_model.cpp` is r4a's file —
we did not touch it.

## 5. ADDENDUM (same evening, Chad ruled after his fly: "onto the hand hook") —
## ⚠ r4a-owned file touched, announce to the r4a lane too
`render/sled_model.{h,cpp}` now carry the hand hook: `SledRig::hand_grip[2]`
(anatomical, [0] left / [1] right; world-space pos + lateral axis + weight,
default weight 0 = provably inert — the weld branch multiplies by 1 and skips
the override body) and, inside the existing hands-on-bars weld, a four-line
target substitution: shortest-arc roll of the rest hand wrap from the bar axis
onto the grip axis (pivoted on the hand), then mix(bar_socket, grip, weight)
fed to the SAME two-bone IK that has always run. Side resolved through the
file's own `arm_throttle_side` derivation (no second geometric ruling added).
The throttle/brake control pose fades with the same weight. R3-WS ladder,
legs, torso, walker branch: untouched. Choreography: right hand takes the
pistol grip t∈[0.25,0.50] of the deploy blend, left leaves the bars for the
forward grip t∈[0.55,0.80]; the one-frame cut snaps both back to the bars.
Reds after: exactly the four snow-lane baseline names.

## 6. CORRECTION to §1 — the 1.35 m dial DOES move (and we moved it)
§1 was measured on an invented 1.20 m seated shoulder. The rider's MEASURED
neck is 0.657 m over the kernel CG in the riding hunch (indy650 rest skeleton
via the mount law), so the posed muzzle band is **0.76-0.86 m seated** /
1.80-1.90 afoot. Chad hit the resulting defect in flight ("collapsed over ...
couldnt even see the rpas"); the full post-mortem is commit D3 on this branch.
Per your packet's "tell us the number, we will move the dial" — Chad was
present and iterating, so we moved `sting_stance`'s head_h in the P block
ourselves: **1.35 -> 0.80 seated, 1.7 -> 1.85 afoot** (your file; announced
here). The aim camera rides the same number and now sits at the seated eye
instead of a standing man's. test_sting_pose executes the bands.
