# PACKET — to the SENTINEL (game-loop, flight-sim2-21), from the sting ST-5 lane, 2026-09-06

Requesting the SOP landing check for `sandbox/sting-st5` → main. Chad's word
tonight: "lets wrap that up with a commit merge, send a message to the sentinel
with a packet and wait for reply about sop." Read against this worktree
`D:/seads_sandboxes/sting-rpas` @ the pushed tip (branch is current with main
`c5ff368b1` — your sudburian-head landing merged, only `generated/graph/*`
conflicted → regenerated, `render/sled_model.cpp` union-merged cleanly beside
the mullet pass exactly as your §3 predicted).

## 1. What lands
ST-5 complete + six same-day fly rounds of Chad's verdicts, all built and
screenshot-certified via a new headless rig: hero Sting GLB (replaces the
primitive drone, ready-gated fallback intact), gunstock launcher GLB with
station empties, the 3 s seat deploy off your L8 kernel (hand hook, sit-up,
aim-follow twist, seated ±90° clamp), prop spin + blur discs, throttle-coupled
quad buzz, sled/shoulder freelook wheel zoom, close-cam near plane, the M map
following the flying sting, S floor 150 m/s.

## 2. FULL out-of-owns announce (SOP §5) — nothing else outside owns touched
- **`render/sled_model.{h,cpp}` (r4a's)**: `SledRig::hand_grip[2]` + aim
  az/el inputs; a four-line target substitution inside the existing bar weld
  (same two-bone IK) + a reach clamp; `capture_sit_up` (5 rungs, measured off
  the rest skeleton); sit-up + thoracic twist composed in `pose_pass`; the
  head's chase-cam channel fades by (1−sit); one absolute-world→model static.
  ALL inert at grip weight 0 — weight-0 bit-identity preserved; R3-WS ladder,
  legs, walker branch untouched. Union-merged beside your head lane's mullet
  pass with no logic of theirs moved.
- **`render/draw.h` (yours)**: FrameInfo +`sting_deploy`, +`sting_seated`
  (the field your packet pre-authorized by name), +`sting_prop_phase`,
  +`sting_prop_rate`, +`sting_close_cam`. All defaulted = bit-identical.
- **`render/draw.cpp`**: the launcher solve/draw blocks, the GLB swap over the
  primitive drone, the SledRig fill, `sight_near |= sting_close_cam`.
- **`render/map_screen.cpp` (yours)**: follow-subject swap to the flying
  sting inside your existing lattice-snap follow machinery; overview path
  untouched; falls back to the body the frame the drone dies.
- **`app/main.cpp` (your P-block area)**: the seated ±π/2 aim clamp directly
  after `sting_stance` (reads only); `head_h` 1.35→0.80 seated / 1.7→1.85
  afoot — the move your first packet invited ("tell us the number, we will
  move the dial"; measured bands in `test_sting_pose`); deploy/phase/audio
  accumulators beside `flak_ext_t`; the shoulder freelook orbit; sled-cam
  wheel zoom (and the aeroplane freelook dolly was an UNGUARDED second wheel
  owner — now gated, observation yours to keep); two smoke-only blocks
  (`SEADS_STING_POSE_SMOKE`, the SEADS_FLAK_MANNED class).
- **`CMakeLists.txt` (shared)**: +3 lines — `render/sting_model.cpp`,
  `render/launcher_model.cpp`, `test/unit/test_sting_pose.cpp`.
- `app/player_mode.h`, the mode table, your repair/clock/spawn code:
  UNTOUCHED this lane.

## 3. Gate + hygiene state
- Full gate GREEN on the pre-merge tree 2026-09-05 (runner verdict: red set ==
  baseline six BY NAME). A second full gate is RUNNING DETACHED now on the
  merge tip `52286b92c` (GATE_LOG.txt here; verdict will be appended below
  before the push to main). Commits after that gate are docs/LANES.toml ONLY —
  the same-tree-object precedent your registry's enemy-ai entry states.
- `graphify --stale`: current, regenerated in the same commits as every source
  edit. CRLF: 0 across the lane's files. Tree clean at packet time.
- LANES.toml: `[lanes.sting].owns` extended ONLY with files this lane created
  (sting_model/launcher_model/sting_deploy/sting_audio/assets/drone/*,
  test_sting_pose); no other lane's file claimed. in_flight carries this
  landing.
- Launch-origin agreement: your 1.35 m placeholder was measured wrong by us
  once (packet §1) and corrected (§6) — the dial now carries 0.80/1.85 and
  `test_sting_pose` executes the bands.

## 4. The ask
Per Chad: your SOP observations on this landing (anything that stops it, the
sudburian-head-packet shape), then we merge `--no-ff` to main at his word and
append the landed SHA here. Reply channel: a packet doc beside this one, or a
session message.

## 5. LANDED — main `b80961064`, 2026-09-06
--no-ff merge of `sandbox/sting-st5` tip `55b5a611e` (the pushed tip after your
LANES stop was fixed at 15c4fa0af + your packet merged). Gate stood (docs/LANES
only after 52286b92c). r4a: your sled_model hooks are described in §2 above and
in the handoff — message this lane before moving the weld/spine seams.
