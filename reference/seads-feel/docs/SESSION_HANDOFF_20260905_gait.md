# SESSION HANDOFF 2026-09-05 — THE GAIT LADDER (G1–G2g BUILT+FLOWN; launch here for G3+)

Lane: `D:\seads_sandboxes\gait`, branch `sandbox/gait`, base main `5029294e9`.
HEAD at handoff: `8de6db6ce` ("gait G2g: he stands ON the ground, STRAIGHT, and leaves FOOTSTEPS").
Contract: `docs/PLAN_20260904_gait_ladder.md` — §3 LAWS are hard fails, §4 the rung ladder, §6 the
defect ledger. READ IT FIRST. Memory: `gait-ladder-lane` (the session memory index).
Fly exe: `D:\seads_sandboxes\gait\build-play\seads.exe` (`cmake --build build-play --target seads`;
the tests DON'T compile there by design — assert-dead — that is normal, build `--target seads`).

## §1 What is BUILT and what Chad has SIGNED (fly verdicts, 2026-09-05)

SIGNED on his drives: the 5.5 m/s road run · the snow slow-down · "walk looks better, walking on
the asphalt now" · "other than these complaints it looks good".
AWAITING his verdict (fixed since, not yet re-flown): ground contact beside a capsized sled ·
the straight back · the footstep trail.

The commit chain (each message carries its full story — `git log --oneline` and read them):
- G1 `fcb125127` feet meet the ground (sim::GaitState, world pins, slope pitch)
- G1b `8ebaa9bb1` red-team fixes (guard converges, pitch axis derived, stop pins under the foot)
- G1c `1b26b4810` verify round (excursion scale removed — it WAS skate; pitch axis frame)
- G1d `317db4360` foot targets get the root's anchor
- G1e `3d44177c6` ★THE BUG: R4c-3's walking-ARM `continue` skipped the LEG block on main
  (gated-never-flown) + targets raised to the ANKLE. Legs stride from here on.
- G2 `20d8e2e53` the pelvis lives (drop/bob/sway/Trendelenburg) + the RULED 5.5 cap
- G2b `d11a98448` boot composed from the SOLVED calf (feet were detached out front)
- G2c `3b3b371f4` the ankle is a HINGE (absolute orientation; tippy-toes) + measured
  `flak::GunnerAnthro::ankle_up_m` 0.10 (one number, one owner)
- G2d `6c4b3fdb2` walking camera frames the whole man (on top of presets; SLED_CAM pin wins)
- G2e `caf71ddc3` the run stands up (4 stacked drop-law bugs, see message) + capsize float rd 1
- G2f `eada40091` exact ground placement BY MODE off the one-way ladder (capsize float rd 2)
- G2g `8de6db6ce` ★root anchored to the GROUND under him (walker.pos = drive_r + lie_clearance,
  a convention not ground — the SAME 0.564 m was the 90.7%→9.8% clamp duty) + measured
  seated-hunch removal per spine joint + FOOTSTEPS (stance-entry pins → 256-ring → heel/toe discs).

Gate state: full gate GREEN thru G2e (red set == baseline six member-for-member); G2f/G2g are
render/app-only (`sled_model.cpp`/`draw.cpp`/`main.cpp` are NOT in seads_tests — verified against
CMakeLists) so the verdict carries; the 39-leg gait/walker/g2 subset is green at HEAD.
Run a fresh full gate before any LANDING regardless.

## §2 THE INSTRUMENT THAT FOUND EVERY REAL BUG — use it before believing anything

```
SEADS_SLED_DEBUG_MODE=1 SEADS_GAIT_DEBUG=1 SEADS_SMOKE_WALK=1 \
  ./build-play/seads.exe --smoke 1830 shot.png            # natural walk camera
SEADS_SLEDCAM="9,80,40"  ... --smoke 1860 top.png         # orbit the machine instead
```
Cuts the grip at tick 40 exactly where the kernel cuts it, runs the REAL throw/land/get-up
(Afoot ~tick 1740 in 0.71 m snow), circles him (forward+turn baked), screenshots the frame,
and prints per second: `GAIT clamp duty: X% ..., target sweep fwd [a, b] m` plus `SMOKE_WALK`
mode telemetry. ~4–5 min a run, deterministic frame-for-frame. READ THE PNG — five of this
lane's defects were invisible to every test and every code-reading round and OBVIOUS in one
screenshot. Healthy now: duty ~10% wading, sweep ~[-0.2, +1.0].
⚠ It exercises deep-snow BUSH only — road/run and capsize scenarios are Chad's stick.

## §3 Laws learned the hard way THIS lane (beyond the plan's §3 — all ledgered in plan §6)

- ★A `continue` in pose_pass's per-side chain loop skips everything below it for that side.
  Check the loop before adding blocks. (G1e — the "walking with hands" bug, shipped on main.)
- ★Any world→model conversion for the walker must reproduce the ROOT's exact composition
  (`rest_root + flight_off + M·(P − anchor)`), and every consumer must share ONE anchor.
  The anchor is the GROUND under walker.pos, never walker.pos itself (lie_clearance 0.564).
- ★A world-pinned target's offset from a MOVING bone cannot be scaled/re-derived render-side —
  that is skate by construction. (G1b's mitigation, killed in G1c.)
- ★rest-frame vs current-frame: `flight_q` maps REST vectors to current. Applying it to an
  already-current vector rotates by his heading twice. Contrast pole vector (rest, needs it)
  vs rt_m (current, must not). (G1c/G2c.)
- ★The foot inherits nothing: position from the solved calf, orientation ABSOLUTE (the ankle
  is a hinge). (G2b/G2c.)
- ★Only STANCE feet may demand pelvis drop; a vertical drop cannot fix a HORIZONTAL over-reach
  (demand 0, the clamp owns it); the crouch fades with duty — a runner resolves by flight +
  toe-off, which is G4's job. (G2e.)
- ★An ease that preserves old behaviour "near X" must ask WHO LIVES near X. (G2f — the man
  who tipped his sled walks BESIDE it.)
- ★Never touch `build/` while a gate's ctest runs there (three near-misses this lane); before
  killing ANY ctest, attribute it: `Get-CimInstance Win32_Process` → seads_tests.exe
  ExecutablePath names the worktree. The head-mesh lane (`sudburian-head-lane`) gates on this
  box too.
- Smoke-only rigs and env dials this lane added: `SEADS_SMOKE_WALK`, `SEADS_GAIT_DEBUG`,
  `SEADS_GAIT_STRAIGHT`, `SEADS_GAIT_BOB`, `SEADS_GAIT_SWAY`, `SEADS_WALK_FOOTDROP` (additive
  trim, default 0). `SEADS_SLED_CAM` pins the camera and the on-foot framing defers to it.

## §4 OPEN ITEMS, in order

1. **Chad's re-fly of G2f/G2g** (the drive checklist): walk beside the TIPPED sled — feet ON
   snow; road walk — straight back, boots on asphalt, NO prints; bush walk — footstep trail
   behind him. His word signs G2.
2. **RULING OWED (kernel, one line): `walker_stride` derives speed from the DEPTH CAP, not
   actual `|vel|`** — measured desync in lumpy snow (plan §6). Fix at `sim/walker.cpp:70-82`
   with a test, ONLY on his word (frozen-surface territory).
3. **G3 — counter-rotation + lean** (plan §4): pelvis vs shoulder counter-yaw up spine_01..03
   (walk 4°/6°, run 8°/14°), speed lean, slope lean. Note G2g's `gait_straight_q` already
   owns the spine channel seam — G3 composes WITH it, one owner per joint per term.
4. **G4 — ankles/toes**: heel-strike→toe-off through `gait_foot_pitch_rad` (the hinge is
   ready); drive `ball_l/r` (never yet written); toe-off plantarflexion buys back the
   effective leg length the run's clamp currently stands in for.
5. **G5 turns/starts/stops · G6 deep-snow effort · G7 head stabilization · G8 asymmetry +
   the NPC test crowd** (NPC-READY is RULED — everything is value-typed already).
6. **Fresh-context RED-TEAM ROUND before any main landing** (standing rule; G1's round found
   3 real P1/P2s, its verify round found 2 more). Then the CONTRIBUTION_SOP landing dance +
   warn the game-loop STAND-BY lane (pose_pass changed shape) to re-run its loop subsets.
7. For the WORLD/GAME-LOOP lanes, not this one: Chad reports the SNOWMACHINE sometimes sinks
   into the plowed road.

## §5 Fly checklist template (inline in the reply, ABSOLUTE exe path, build-play built FOR him)

EXE: `D:\seads_sandboxes\gait\build-play\seads.exe`
1. Tip the sled, dismount, circle it on foot — boots ON the snow the whole lap.
2. Road: walk + hold W to the 5.5 run — upright back, boots on the surface, no prints.
3. Bush: wade, look back — alternating heel/toe trail; `SEADS_GAIT_DEBUG=1` if anything reads
   wrong (clamp duty + target sweep name the guilty layer from the log alone).
