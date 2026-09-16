# SESSION HANDOFF 2026-09-06 — STING ST-5 ART: COMPLETE, AWAITING SENTINEL + LAND

Lane `sting`, worktree `D:\seads_sandboxes\sting-rpas`, branch
`sandbox/sting-st5` (pushed; base = main `c5ff368b1` merged). Plan
`docs/PLAN_20260904_sting_st5_art.md`; the sentinel landing packet
`docs/PACKET_TO_LOOP_from_sting_st5_20260906.md`; the game-loop coordination
trail `docs/PACKET_TO_GAMELOOP_from_sting_st5_20260904.md` (§1 superseded by
its own §6). Exe `build-play\seads.exe`.

## 1. What this lane shipped (all Chad-flown across six rounds, 2026-09-04..06)

| Piece | Where |
|---|---|
| Hero Sting GLB (Chad live-approved in Blender; perpendicular X-frame his mid-build ruling) | `assets/drone/sting.glb` ← `D:\flight_sim2\Game_loop_idea\vehicle_program\blender\sting.blend` (own file, NEVER indy650) via `assets/drone/sting_src/export_sting.py` |
| Gunstock launcher GLB (open rail — the X-frame can't ride a tube; offset sight; 5 st_* stations fail-loud) | `assets/drone/launcher.glb` + `export_launcher.py` |
| Rigid GLB drawers (flak_model clones; body-slot-only ally tint; `SEADS_STING_MODEL=0` kill; primitive fallback intact) | `render/sting_model.{h,cpp}`, `render/launcher_model.{h,cpp}` |
| The 3 s seat deploy (P from the seat — the loop lane's L8 kernel): eased rise, 0.3 s voluntary stow, ONE-FRAME cut on stance loss, launch snap; sweep around the silhouette; the Sting riding the rail | `render/sting_deploy.h` (pure laws) + draw.cpp blocks + `sting_deploy_t` in main.cpp |
| Hand hook (target substitution into the existing bar weld, reach clamp), sit-up (72% of the MEASURED 33.4° hunch), aim-follow twist (thoracic-heavy, head leads + 40% of elevation) — all keyed on grip weights, inert at 0 | `render/sled_model.{h,cpp}` (r4a's, announced) |
| Seated aim clamped ±90° of the machine's nose, ON the accumulator, every frame | main.cpp after `sting_stance` |
| Prop spin (apparent-rate soft knee, cap 34 rad/s, alias static_assert) + blur discs (measured radius, x^0.6 alpha, 0.55 ceiling) | `render/sting_audio.h` laws + `render/sting_model.cpp` |
| Throttle-coupled quad buzz (4 detuned motors × 5 harmonics, 94→354 Hz + 15.9 dB over the band; `SEADS_STING_AUDIO=0`) | `render/sting_audio.h` + main.cpp audio block |
| S floor 150 m/s (bottom only; the fly-2 top stands); `sting_thr01` re-based to the BAND fraction (the old |v|/max pinned S at 0.56 up every voice) | `app/sting.h`, main.cpp |
| Cameras: sled default 1.45× out + wheel 3–30 m; shouldered SPACE-freelook 3rd person 1.5 m default, wheel 1.2–8 m; close-cam 0.15 m near plane (`sting_close_cam`) — the "hollow model" root cause | main.cpp + draw.cpp |
| M map follows the FLYING sting (follow-subject swap in the loop lane's lattice machinery) | `render/map_screen.cpp` (announced) |
| Launch origin corrected: `head_h` 1.35→0.80 seated / 1.7→1.85 afoot (measured; the loop packet's invited move) | main.cpp |

## 2. THE DEBUG RIG this lane leaves behind (use it, it paid for itself)
`SEADS_STING_POSE_SMOKE="yaw,cam_az,cam_el,aim_az_deg"` seeds man-on-machine +
shouldered from tick 40 headlessly; with `--smoke N shot.png` any deploy frame
is screenshot-certifiable. Companion pins: `SEADS_STING_DEPLOY` (blend),
`SEADS_STING_SPIN` (true prop rate), `SEADS_STING_LOOK="az,el,dist"` (the
3rd-person orbit), `SEADS_STING_DEPLOY_DEBUG=1` (measured muzzle TraceLog).
It convicted the D4 aim-frame bug after two static-analysis rounds could not.

## 3. Lessons carried (the collapse saga, D3/D4 commits)
- TWO AIMS EXIST: `st.aim` is the flying drone's; the shoulder's is
  `sting_aim_az/el`. Which ships in FrameInfo is the state's call.
- A green test on INVENTED geometry certifies nothing: nominals now mirror
  the GLB digit-for-digit; tests run measured shoulders + the real 0.613 m arm.
- The weld may never be asked past the chain (reach clamp), and a hand-target
  cache must be BODY-frame (a world point smears 0.33 m at 20 m/s).

## 4. State + next actions
1. Full gate DETACHED on merge tip `52286b92c` — verdict lands in GATE_LOG.txt;
   commits after it are docs/LANES only (same-tree-object precedent).
2. Sentinel packet pushed (`b20bf4566`); session flight-sim2-21 was offline —
   the reply is owed by packet or message. DO NOT push to main before it.
3. At Chad's word after the reply: `git checkout main && git merge --no-ff
   sandbox/sting-st5`, push, append the landed SHA to the sentinel packet,
   relay to r4a (the sled_model hooks) per SOP.
4. Deferred, named: afoot the hands don't grip the launcher (walker arms are
   the gait branch — own rung); launch/detonation one-shot SFX (the buzz
   shipped; the manifest one-shots didn't); config-file dials; lead pip;
   per-match reload reset; sting not enemy-hittable.
5. seads-recon (Chad's fly tree) untouched, as always — whoever merges main
   there does it on his ask.
