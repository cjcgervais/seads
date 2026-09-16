# SESSION HANDOFF 2026-09-08 — CAM-SMOOTH (the jitter)

Lane: `sandbox/cam-smooth`, worktree `D:/seads_sandboxes/cam-smooth`, branched off main
`0e9636399`. Registry entry: `LANES.toml [lanes.cam-smooth]`. Play exe:
`D:\seads_sandboxes\cam-smooth\build-play\seads.exe`.

## 1. Chad's ask
"Find out why the camera motion feels jittery, what causes it, and how we fix it so flying,
driving and viewing all around is smooth as butter." Then: "lets sandbox and fix this."

## 2. What was measured before anything was touched
- A raylib probe built against this tree's own raylib (same window flags as seads: VSYNC +
  MSAA4X + RESIZABLE, 1920x1080 windowed, empty scene) on Chad's box:
  frame time mean 16.667 ms, stddev 0.083 ms, min 15.79, max 17.61; 100 % of frames within
  1 ms of the mean; a 120 Hz accumulator ticked 2/frame on 584 of 600 frames (8 frames at 1,
  8 at 3); zero frame-to-frame swings over 3 ms. GL context on the RTX 5060 Ti, not the AMD
  iGPU. **Timer noise and GPU selection are CLEARED as causes.**
- Rendering is eye-relative (`render/draw.cpp` sets `cam.position = 0` and subtracts the eye
  in double) so float precision at R = 15 km is not a cause either.
- The T25 profiler (`SEADS_PROF=1`) exists for load spikes; NOT measured this session — that
  half needs Chad's real fly (see §6).

## 3. The two causes, both in code
1. **The sled, the walker and the sting were drawn RAW off the 120 Hz tick.** The aeroplane
   has always been drawn from `render::interpolate(loop.prev, loop.curr, accum.alpha())`
   (SPEC §10). The sled camera anchored on `sled` / `walker.pos` directly (main.cpp: "sled_prev
   / sled interpolation arrives with the S4 ribbon" — it never did; `sled_prev` was written
   once per frame BEFORE the tick loop, N ticks behind, and never read). At 60 Hz every frame
   carrying 1 or 3 ticks instead of 2 hopped the body and the camera on it by half a tick of
   travel — ~1 frame in 40 at perfect pacing, and every frame once a heavy frame beats against
   the tick. That hop is the jitter.
2. **The sled camera was welded to the CG and heading** (eye/target computed straight from the
   body each frame, no lag), so every suspension tick, ski contact and yaw wobble off the facet
   terrain reached the eye 1:1. The aeroplane camera has a lagged forward
   (`ease_chase_forward`), which is why flying already felt smoother than driving.

## 4. What was built (app/main.cpp + render/interp.h + test)
- `render/interp.h`: `interpolate_sled(prev, curr, alpha)` (position, velocity, slerp
  orientation, susp_x/susp_v/sink_m, rider_lat/fwd/up, steer_actual mix; discrete fields
  newest-wins) and `interpolate_walker(prev, curr, alpha)` (pos, vel mix; heading nlerp
  renormalised, antiparallel falls back to newest). Tests in `test/unit/test_interp.cpp`
  (4 cases green).
- `app/main.cpp`:
  - `walker_prev` beside `walker`; `sled_prev` and `walker_prev` captured at the TOP of every
    tick of the sled loop (unconditionally). The old per-frame capture before the loop is
    removed (comment left in its place). The six respawn/teleport `sled_prev = sled` sites are
    untouched. ⚠ They are LOAD-BEARING on a 0-tick frame (fps > 120 with vsync off): the
    walker is placed by mount/dismount OUTSIDE the tick loop, so on such a frame the blend
    reads a stale prev for one frame. Not Chad's 60 Hz box; noted, not fixed.
  - `sled_draw`, `walker_draw`, `sting_draw_state` built beside `draw_state` from
    `accum.alpha()`. Every DRAW-side reader takes them: camera anchor + forward, DrawInfo
    sled_pos/basis/susp/sink, plume pin (pos/vel/up/head), shadow proxies, `info.sled_walker`
    pointer, `info.sting_draw`, the sting flight camera's `spos`. Gameplay readers (repair
    reach, listener, map dots) still read the raw kernel state on purpose.
  - The sled camera LAG: persistent `sled_cam_anchor` / `sled_cam_fwd` beside `sled_cam_az`.
    Anchor: first-order lag on the frame dt with velocity feed-forward `ff = dt*(1/kp - 1)`
    (`render::cam_lag_step`, the exact zero-lag fixed point of the discrete update as posed
    against THIS frame's body; pinned by test_interp.cpp at 30/60/120 Hz and the 0.25 s clamp).
    ⚠ The first cut shipped `ff = dt/kp`, the fixed point of the wrong recurrence: it LED the
    body by v*dt every frame (0.33 m at 60 Hz, 72 km/h) and lurched ~2 m on a 100 ms hitch;
    the 2026-09-09 red-team caught it by re-deriving the steady state. Forward: exponential rotate-toward, tangent-projected after. Re-seeded on first
    use, on a > 20 m jump (respawn/remount), and on every --smoke frame (inert there).
    Dial: `SEADS_SLED_CAM_TAU="pos,fwd"`, default `0.06,0.12` s; `"0,0"` = the welded camera
    exactly. Read once at first use.
- `tools/gate/lane_map.toml`: `test_interp.cpp` attributed to cam-smooth.
- `generated/graph/*` regenerated (`graphify.py`, layer check OK).

## 5. Evidence
- Play exe built (RelWithDebInfo, `--target seads`; the test targets refuse a non-Debug tree
  by design). `SEADS_PUMPWALK_SMOKE=12 --smoke 400` and plain `--smoke 120`: exit 0, no
  NaN/assert. The pump-walk shot A/B'd against `seads-recon/build-play/seads.exe` run from
  the SAME cwd: visually identical (the rig's own camera sits inside a dark facet mass in
  BOTH — pre-existing, not this lane's).
- In --smoke the blended bodies draw one tick (8.3 ms) behind the kernel, exactly as the
  aeroplane always has (alpha == 0 when frame_dt == sim_dt).
- Gate on the LANDED tip 42fe44404 (2026-09-10, full ctest on the Debug tree, DETACHED): 6 failed of
  1997 == the baseline six member for member (`gate_baseline.py check` exit 0). Chad flew the fixed
  exe 2026-09-09 and signed ("really good").
- Gate (2026-09-09, first cut, full ctest on the Debug tree, runner verdict via `gate_baseline.py check`):
  6 failed of 1995 == the baseline six member for member. The first run had ONE new red, this
  lane's own: the snow lane's row-9 source-text tripwire greps main.cpp for the shadow's
  `sled.steer_actual` read; pointed at `sled_draw`, announced in LANES.toml. Note: the harness
  memory guard killed the gate twice at test 102 (a 10 MB test — not real pressure); the
  verdict run was launched DETACHED via Start-Process.

## 6. Owed: Chad's drive (checklist inline)
Exe: `D:\seads_sandboxes\cam-smooth\build-play\seads.exe`
1. Ride the sled at speed on rough snow and on a groomed trail. Judge: is the once-a-second
   hop gone? Do the bumps read as a ride now instead of a shake?
2. Hold SPACE and look around while riding; release. Judge: the orbit and ease-back.
3. Come off the machine, walk, then remount. Judge: no camera pop on dismount/remount.
4. Launch the sting and fly it. Judge: the drone view.
5. Sweep the dial from the shell if the lag feels wrong: `set SEADS_SLED_CAM_TAU=0.10,0.20`
   (softer) or `0.03,0.06` (tighter); `0,0` is the old welded camera for A/B.
6. Fly the aeroplane ~2 min in a busy scene with `set SEADS_PROF=1`; paste any `[PROF] ...
   SPIKE` lines. That is the load-spike half of the question, unmeasured until he does.

## 7. Not done / next
- Red-team DONE 2026-09-09 (fresh context): verdict LAND-WITH-FIX; F1 (feed-forward fixed
  point) BLOCKER fixed, F2 (rider reads) fixed, F3 (test) added, F4 (dial clamp) fixed; F5-F7
  recorded above; F8 tripwire edit ruled honest; F9 blend ruled correct.
- `info.sled_gait` still points at the raw per-tick gait state (swing foot may sit a sub-tick
  off the blended body); feet pinned in stance are unaffected. (The rider pose and steer now
  read `sled_draw` -- red-team F2, fixed 2026-09-09.)
- The forward lag has NO feed-forward by design: in a steady turn at yaw rate w the camera
  looks w*tau_fwd outside the turn (~7 deg at 60 deg/s). A chase-cam feel choice for Chad.
- `walker_prev = walker` beside each mount/dismount would close the 0-tick-frame stale-prev
  edge (red-team F7); only reachable above 120 fps with vsync off.
- The aeroplane's `ease_chase_forward` is a constant-rate slew, not an exponential ease —
  a feel dial for Chad's ruling, not part of this fix.
- Landing: SOP §6 (gate baseline by name, graph in the same commit, LANES status line).
