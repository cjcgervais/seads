# HANDOFF — 2026-08-18, winter-gi / `sandbox/gi4-ride`

> ★ **SUPERSEDED 2026-08-18 evening by `docs/SESSION_HANDOFF_20260818b.md` — launch from THAT.** This file is the morning record.

**LAUNCH LINE:** *"Read `docs/RIDER_AUTHORITY.md` FIRST, then
`docs/SESSION_HANDOFF_20260818.md`, then §7. Do the item Chad names."*

> ★★★ **2026-08-18 morning — READ THIS.** The overnight session after R2c-5
> built "R2c-6 leather mitts" and "R2c-7 costume" (`e0aaf725f`, `402440b75`)
> on the **WRONG RIDER** — the 19-bone LEGACY `rider_rig` inside `indy650.glb`,
> not the SUDBURIAN — with every gate green. Chad ruled: **REVERTED** (this
> branch's HEAD carries the revert; `indy650.glb` is byte-identical to R2c-5
> `9c3ae98f0` again). The mitt GENERATOR was salvaged, unwired, to
> `assets/character/sudburian_src/salvage_from_legacy_rider/` for re-targeting
> onto the Sudburian's hands. `docs/RIDER_AUTHORITY.md` is now the law:
> Sudburian only, legacy rider frozen, Blender never headless, the running GUI
> session is the truth. §7's option A still means: **the SUDBURIAN goes into
> the game** — not "improve what the game draws today".
>
> ★★ **AND THEN IT DID — R2c-S, same day.** The Sudburian is in `indy650.glb`
> and in the game (`SUDBURIAN_LADDER.md` §6 R2c-S: `splice_sudburian.py`,
> UE5 catalogue + crossover, root/pelvis frames derived, boot reseat 0;
> `[rider_pose],[rider_rig]` 50/50, gate 1237/1241 = the same 4 GI4 cases).
> The stale-bar finding was CLOSED the same day on Chad's word
> (`splice_bar.py`, the live handlebar assembly carried in; bellcrank/tie-rods
> held). §8–§9 below are the reverted wrong-rider record.

Read this **and** `docs/SUDBURIAN_LADDER.md` §3 before touching anything.
Query `tools/graph/graph_query.py` instead of grepping; regenerate the graph
(`python tools/graph/graphify.py`) in the SAME commit as any structural change.

---

## 0. THE ONE-LINE STATE

| | |
|---|---|
| worktree | `D:\seads_sandboxes\winter-gi` |
| branch | `sandbox/gi4-ride` — **NOT pushed** |
| HEAD | `9c3ae98f0` — *R2c-5: the throttle and brake reach the rider's arms in the game* |
| gate | **1237 / 1241.** NOT all-green, and was not before either — see §4's gate note before you panic |
| blend | **`D:light_sim2\Game_loop_ideaehicle_programlender\indy650.blend`** — ★ NOT inside this worktree, and there is no copy in it. SAVED 2026-08-18 01:17 with the rebuilt rig, posed neutral, seating green |
| blender scripts | `assets/character/sudburian_src/` — those DO live in the worktree (`sudburian_proxy.py`, `seat_sudburian.py`) |
| working tree | clean apart from pre-existing untracked `conquest_tape_*.jsonl` (the `*.sledtape` and `*.png` files in the root are gitignored, not untracked — do not sweep either, `build/sled_tape_2.sledtape` is a keeper) |

**R2c is under way. The HANDS are built and Chad has driven the shape of them
through four corrections in one session, and R2c-5 has now carried the
control-input pose INTO THE GAME (`render/sled_model.cpp` — the throttle and
brake move the rider's arms in the running build, measured, not eyeballed).
NEXT is a RULING, not a task: see §7.**

★ STEERING, SHROUD AND DASH ARE STILL DEFERRED BY CHAD. Do not pick them up.
The one machine-side item this session raises is logged in §5 and is NOT yours
to fix without asking.

★ **ONE BLENDER SESSION, ONE WRITER.** Chad may have `indy650.blend` open. Agents
open Blender themselves and NEVER headless (`tools/blender/mcp_startup.py`) — he
has to see the viewport. Do not kill his session; re-read the inventory after
every reconnect.

---

## 1. WHAT LANDED

| item | what |
|---|---|
| **R2c-1** | the mitt folds at the knuckle; bar contact wrist → knuckle; elbows allowed to bend |
| **R2c-2** | thumb gets geometry and goes on the bar; the mitt axis correction |
| **R2c-3** | thumb re-rooted on the medial circumference; wrist articulation; contact solver rewritten |
| **R2c-4** | the two hands stop being mirror images: fingers on the brake side, thumb on the throttle side |
| **R2c-5** | **THE RUNTIME.** The control-input pose now exists in the game, not just in Blender |

`indy650.blend` is SAVED with the rebuilt rig, posed neutral, seating green.
`assets/character/sudburian_proxy.glb` is re-exported and committed.

### The hands, as they now stand

Right hand (throttle, world −X, proxy `_r`): fist prism wrist→knuckle 100 mm,
mitt tube 140 mm across the bar at the knuckle, thumb extended off the medial
circumference. Left hand (brake, world +X, proxy `_l`): same fist, but
`mittfront_01_l` is a **four-finger slab** (oriented box 95 × 115 × 38 mm,
dropped 55 mm off the knuckle) and `thumb_01_l` is a **tucked 35 mm stub** — it
grips, it does not operate anything. Asymmetry keys off `BRAKE_SIDE` in
`sudburian_proxy.py`, one constant, ruling written beside it.

### The control-input pose

`apply_controls(arm, throttle, brake)` in `seat_sudburian.py`, 0..1 each.
All four articulations are 20°, per Chad.

| input | elbow | hand |
|---|---|---|
| throttle | right drops −35 mm | angles **up**, thumb presses in on the proximal side |
| brake | left rises +46 mm | angles **down and over** the bar, fingers reach the lever |

Contacts hold at 0.00–0.09 mm through the whole travel.

---

## 2. ★ THE LESSONS. These are the expensive ones.

1. **MEASURE IN THE POSED RIDER, NOT IN REST.** Three separate bugs, one root.
   A rest-space `+X` "bar axis" lands 58° off the bar once the arm is IK-solved.
   A rotation axis expressed in `bone.matrix_local` gave 3° of pitch where 20
   was asked for — it belongs in `parent_pose × rest_local`, which is `pb.matrix`
   with the bone's own rotation cleared. If a direction has to mean something on
   the machine, take it FROM the machine (`grip_socket_R − grip_socket_L`).

2. **NEVER ASSUME THE TWO SIDES MIRROR. MEASURE EACH.** The elbow poles need
   opposite signs (`lowerarm_l` −20° drops, `lowerarm_r` +20° drops); the wrists
   need the *same* sign. The thumb `up` vector silently flipped with the side and
   put one thumb 51 mm above the grip and the other 51 mm below it, inside the
   bar. This is the mirrored-poles trap, and it appeared **three times today**.

3. **A CONTACT SOLVER MUST CLOSE ON THE MEASURED ERROR, NOT ON A FORMULA.**
   `resync_targets` derived the wrist target as `grip − forearm_dir × FIST_M`,
   which silently assumes the hand is colinear with the forearm. True until the
   wrist articulates, then 7 mm of drift. It now drives the target by the
   measured knuckle-to-grip error: geometry-free, holds at any wrist angle.

4. **SOLVING FOR A MINIMUM SILENTLY PINS EVERYTHING ELSE.** `solve_lean()`
   returned the smallest lean that closed the hand contact, so it stopped at the
   first angle the arm could reach *at all* — the elbows came out dead straight
   and no one noticed for a rung. If a solver returns an extremum, ask what it
   has quietly locked.

5. **DO NOT RE-DERIVE A RULING FROM THE COORDINATES.** Chad ruled the throttle
   is the right thumb. I re-argued it from the axes twice and burned his patience
   for it — *and the object names were the thing misleading me*: the red part on
   the brake side is called `throttle_block`. Take the ruling; log the naming
   conflict as a separate finding.

6. **`drop_onto_surfaces()` DIVERGES IF YOU RE-RUN IT AFTER A LEAN CHANGE.**
   Its foot raycast finds the SEAT above the runners: −78, −82, −87, −94 mm,
   never converging, and it puts his feet on the seat. `seat()` alone is correct;
   the signed `PELVIS_Z` / `ANKLE_Z` are right and do not want re-deriving.

7. **REBUILDING THE RIG IN CHAD'S LIVE SESSION IS A BIG DEAL.** The live rig was
   20 mm stale against his own `SHOULDER_Y 0.020 → 0.000` ruling, on all 18 arm
   bones. Rebuilding was correct but I did it without showing him the mismatch
   first, and it cost him confidence. Show the evidence, then ask.

8. **A RULING YOU CANNOT WRITE DOWN CANNOT BE RETYPED WRONG.** R2c-5's whole
   defence is that it stores NO sign anywhere: the throttle side is derived from
   the grip socket's x, and both per-side signs are measured off the rest pose at
   load. That is what made the runtime re-derive the Blender session's hand
   measurements independently and agree (elbow +1/−1, wrists −1/−1). When a
   ruling has a geometric consequence, prefer deriving it to declaring it — and
   then LOG what was derived (`SLED: control pose -- throttle side 0 …`) so a
   bad asset shows up in the log instead of on Chad's screen.

9. **CHOOSE THE PIVOT AND THE CONSTRAINT DISAPPEARS.** Blender iterates 24 times
   to keep the knuckle on the bar because its IK tip is the wrist. The runtime
   rotates the hand ABOUT THE BAR, so the contact is a fixed point of the
   rotation and holds exactly, for free, at any angle. Before writing a solver,
   check whether a change of pivot makes the thing you are solving for
   structural.

10. **SAY WHAT THE GATE ACTUALLY IS.** This branch is 1237/1241 and has been red
    in the same four `test_sled.cpp` cases since GI4. The correct move is to
    prove the failures are not yours (stash, rebuild, re-run — 5 minutes) and
    then state the number, not to write "green" and not to adopt someone else's
    debt mid-rung.

---

## 3. THE MEASUREMENTS THAT SETTLED THINGS

- Arm sections **were never wrong**: 344 / 270 / 200 mm back-solve to
  H 1.849 / 1.849 / 1.852 against the ruled 1.850 (LADDER:1245). The hand read
  long because it was **drawn** as a 240 mm tube down the forearm axis at 93 % of
  the forearm's diameter. Folded at the knuckle it projects **170 mm**.
- Legacy shipping rider (`indy650.glb`, being replaced): `rider_mitt_L/R` is a
  106-vert blob whose cuff sits 21–45 mm off the forearm axis, so the forearm
  stands **outside the glove by up to 33 mm at rest**. Not fixed — that asset is
  going away. Recorded so nobody re-measures it.

---

## 4. R2c-5 — THE RUNTIME. DONE, AND HOW IT WAS PROVED.

The game now poses the arms off the two control inputs. It is the SAME pose
`seat_sudburian.py::apply_controls` holds — same 20°, same directions — but the
runtime reaches it a different and better way, and that difference is the
interesting part of this rung.

### What was built

| file | what |
|---|---|
| `render/rider_pose.h/.cpp` | 4 new PURE functions + the two 20° constants: `swing_bend_normal` (the pole/shoulder swing), `roll_about_axis` (the wrist on the bar), `swing_down_sign`, `wrist_up_sign` |
| `render/sled_model.cpp` | `SledRig::throttle/brake` consumed in `pose_pass`; the throttle SIDE and the two per-side SIGNS derived at load in `capture_rest_ik`; `SEADS_SLED_CTL_DEBUG=1` prints the posed elbow/wrist |
| `render/sled_model.h`, `render/draw.h`, `render/draw.cpp`, `app/main.cpp` | the two channels plumbed from `sim::SledInputs` — the numbers the kernel stepped with, not a second animation state |
| `test/unit/test_rider_pose.cpp` | 7 new cases, including one that pins the ruled directions **against the shipped GLB bytes** |

### The three things that are DERIVED, not retyped

1. **Which side is the throttle.** By the grip socket's x, exactly the way the
   suspension pair is ordered by position. A rename cannot swap it and a
   mirrored re-export cannot either. Measured on the shipped file: throttle
   side 0, grip x −0.315 (model −X = the rider's anatomical RIGHT ✔).
2. **The elbow sign, per side.** The runtime measured **+1 / −1** — the two
   arms do NOT share it.
3. **The wrist sign, per side.** The runtime measured **−1 / −1** — the two
   wrists DO share it.

★ That is an INDEPENDENT re-derivation of what the Blender session measured by
hand, off different code and a different file, and it agrees. The mirrored-pole
trap is now caught by construction rather than by remembering it: nothing in
this rung writes a sign down.

### The one place the runtime is BETTER than the Blender rig

`resync_targets()` closes the knuckle contact with up to 24 solver iterations
because Blender's IK tip is the wrist. The runtime rotates the hand **about the
bar, through the grip socket** — so the point on the bar is a fixed point of the
rotation and the contact cannot drift at all, at any wrist angle, with no
iteration. `the wrist roll leaves the CONTACT POINT exactly where it was` pins
it over a ±40° sweep.

⚠ ONE HONEST NOTE ON THAT PIVOT. The runtime rolls about the GRIP SOCKET, i.e.
about the bar. On the shipped legacy rider the hand node sits 130 mm forward of
the socket (`rest_tip_minus_socket_m`, and §3 records why), so the socket is
not the knuckle *yet* — it is the bar, which is the physically right pivot
either way. When the sudburian proxy replaces the legacy rider its knuckle IS
on the socket and the two constructions become literally the same one. Nothing
to change then; recorded so nobody "fixes" it.

### What it measures in the running game

`SEADS_SLED_CTL_DEBUG=1`, hero GLB, all other channels zero:

| input | right (throttle) elbow | right wrist | left (brake) elbow | left wrist |
|---|---|---|---|---|
| neutral | y +0.7590 | y +0.7543 | y +0.7590 | y +0.7543 |
| throttle 1.0 | **−36.9 mm (DOWN)** | **+46.4 mm (UP)** | unchanged | unchanged |
| brake 1.0 | unchanged | unchanged | **+45.8 mm (UP)** | **−42.5 mm (DOWN)** |

Against the Blender figures in §1 (right elbow −35 mm, left elbow +46 mm): the
two implementations agree to about 2 mm without a single number being copied
between them. The brake wrist also comes back 19 mm in z — down and OVER the
bar, which is the motion Chad described.

Screenshots: `ctl_neutral.png` / `ctl_throttle.png` / `ctl_brake.png` in the
repo root (`SEADS_SLED_DEBUG_MODE=1 SEADS_SLEDCAM="2.2,35,12"
SEADS_SLED_RIG_SMOKE="0,0,0,0,0,0,0,0,<thr>,<brk>" seads.exe --smoke 40 x.png`).
★ The smoke string is now TEN fields; an old eight-field string still parses and
leaves both control inputs at 0, which is the signed rest visual.

★ AND ZERO INPUT IS BIT-UNCHANGED. Every added term multiplies by the input, so
at throttle 0 / brake 0 the arms take exactly the path they took before this
rung. The signed rest visual is untouched.

★ AND THE GATE, STATED HONESTLY. It is NOT all-green on this branch and it was
not all-green before this rung either. Four cases fail:
`sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted` and
`sled_debug_sink_is_write_only` -- all four in `test/unit/test_sled.cpp`, all
four the GI4 open debt, none of them touched by anything here (this diff
contains zero `sim/` changes). VERIFIED, not assumed: the four were re-run with
this rung's changes STASHED and they fail identically on HEAD. Everything else
passes, including the 7 new cases: **1237 / 1241**, and 1230 / 1234 on HEAD
before the rung -- the same four, and only the four.

### What R2c-5 did NOT do

- **The mitts on screen are still the legacy blob** (`rider_mitt_L/R`, §3). The
  new hands live in `sudburian_proxy.glb`; the game loads `indy650.glb`. So the
  runtime now moves the RIGHT arms in the RIGHT directions, wearing the OLD
  hands. That swap is R2c's remaining mesh work, not this rung.
- **No ROM sweep.** Still does not exist. Still the honest gap.

After that: costume, materials, textures, scarf (§4 as ruled, §5 for the scarf).

---

## 5. OPEN, NOT MINE TO CLOSE

- **THE LEVERS ARE NOT PLACED YET.** Chad, 2026-08-18: *"brake lever is the left
  handle bar's red cylinder, they werent placed yet."* So the red cylinder on the
  **left (world +X)** bar is the **BRAKE** lever — and the object carrying it is
  named **`throttle_block`** (x +0.217…+0.293, material `indy_red`), while the
  object named **`brake_lever`** (dark `indy_steel`, x −0.336…−0.204) is on the
  **throttle** side. **The two names are swapped relative to function. Trust the
  side and the colour, not the name** — this is what cost most of one session.
  Because neither lever is placed, the four fingers landing 119 mm from the red
  cylinder at full brake is EXPECTED, not a defect. **Do not tune the finger slab
  to the current lever position.** The fingers finalise after the levers are
  placed, and placing them is machine-side and deferred.
- **THE THROTTLE'S SHAPE IS SPEC'D AND UNBUILT.** Chad, 2026-08-18: *"throttle
  should be an elongated triangle panel connected by a wire going into the
  handle."* So the throttle is a thumb paddle — an elongated triangular panel,
  not the block that is there now — with a wire running from it into the handle.
  It sits on the **proximal** side of the **right** (world −X) handle, where the
  extended right thumb already presses. Unbuilt. Build it WITH the lever
  placement, in one pass, not before.
- `thumb_01_*` radius 0.070 / R_THUMB 0.035 / FINGERS_SIZE are all UNSOURCED and
  marked as such. Chad has accepted the blocks as **placeholders**: "a hand is
  not a block or cylinder".
- Right elbow flex moves 42.6° → 40.6° at full throttle (the wrist rotation
  shifts the target). Returns at neutral. Not chased.

---

## 6. ★ HOW TO WORK WITH CHAD — read before your first Blender call

Everything in last session's §5 still holds. Add these:

- **He watches the viewport and corrects in short bursts.** Four corrections in
  one session is NORMAL and is the process working. Build the small change, show
  it, take the next correction. Do not write him an essay between changes — he
  said so directly: *"this is so monotonous"*.
- **When he states a fact about his own machine, take it.** If your measurement
  disagrees, build what he said and log the disagreement as a finding.
- **He will tell you when you were right.** *"im sorry I didnt see it you likely
  had it the first time"*. That is not licence to re-litigate; check the current
  state against the evidence, say plainly where it stands, move on.

---

## 7. ★ YOUR FIRST HOUR — AND THE RULING THAT COMES FIRST

### 7.1 The ruling. Ask it, do not pick it.

Three things are ready to go and they are NOT interchangeable. Chad picks.

| option | what it is | why it might be first | why it might not |
|---|---|---|---|
| **A — the hands reach the game** | the game loads `assets/sled/indy650.glb`, whose rider still wears the LEGACY blob mitts. The hands Chad drove through four corrections are in `assets/character/sudburian_proxy.glb`. Getting them into the shipping asset is what makes R2c-1..5 *visible* | everything built this session is invisible to him until this happens. It is also the cheapest thing to judge: he looks once and says yes or no | it is pipeline/mesh work with a real chance of a rebuild-identity fight, and R2a owns the pipeline rules. Read `SUDBURIAN_LADDER.md` R2a before starting |
| **B — the ROM sweep** | the R2c gate calls for census 0/0/0/0 + **ROM sweep** + contact sheet. The sweep does not exist. It would drive the rig across its range and report contacts/penetrations per cell | it is the gate for the rung that is running, it is yours to build without waiting on Chad, and it catches the next mirrored-pole bug before he sees it | it produces no visual, and Chad judges visuals. Needs a Blender session (see §0's single-writer rule) |
| **C — costume/materials/textures/scarf** | §4 of the ladder as ruled, §5 for the scarf | it is the literal next line of R2c | the costume refs CONTRADICT §4 and that conflict is already logged in the ladder; starting here without re-reading it re-opens a closed question |

★ **My recommendation, stated once: A, then B, then C.** A is the only one that
puts this session's work in front of him, and §9 of the ladder says he judges
form — he cannot judge what does not draw.

### 7.2 If he picks A, the first four things to check

1. Does the shipping `indy650.glb` rider skin even carry the new hand joints, or
   does the swap mean re-exporting the rider from `indy650.blend` rather than
   copying the proxy in? Check before planning — `test/unit/test_rider_rig.cpp`
   parses the shipped GLB and is the fastest way to enumerate what is in it.
2. `bind_rider_rig` REFUSES a GLB missing any declared node, loudly. That is a
   feature: a half-done swap fails at load with a named list, not silently.
3. The runtime pivots the wrist on the GRIP SOCKET. On the legacy rider the hand
   sits 130 mm forward of it; on the proxy the knuckle is ON it. When the swap
   lands, re-read §4's ⚠ note — nothing needs changing, and that is worth
   confirming rather than assuming.
4. Re-take the three screenshots (§7.4) and put them next to the old ones. That
   is the artefact Chad signs.

### 7.3 What is DONE and must not be re-derived

- The control-pose ruling and its four directions. §4. Chad ruled it, the
  runtime and Blender now agree to ~2 mm. **Do not re-argue it from the
  coordinates — that is lesson 5, and the object names will mislead you again.**
- `PELVIS_Z` / `ANKLE_Z` and the seat. Lesson 6: `drop_onto_surfaces()` diverges
  if re-run; `seat()` alone is correct.
- The arm section lengths (§3). They were never wrong.

### 7.4 The commands you will want

```sh
# gate (Git Bash). NEVER run two ctest gates against one build dir.
cmake --build build && ctest --test-dir build -C Debug --output-on-failure

# just this rung's tests
./build/seads_tests.exe "[rider_pose]"

# the app, for anything judged by eye (RelWithDebInfo -- Debug is 45x slower
# on the rider skinning and will lie to you about feel)
cmake --build build-play --target seads      # target seads ONLY: the tests do
                                             # not compile in RelWithDebInfo by
                                             # design (assert-live gate)

# the R2c-5 evidence: posed elbow/wrist in millimetres, per side
SEADS_SLED_CTL_DEBUG=1 SEADS_SLED_DEBUG_MODE=1 \
  SEADS_SLED_RIG_SMOKE="0,0,0,0,0,0,0,0,1,0" \
  ./build-play/seads.exe --smoke 40 x.png     # fields 9,10 = throttle,brake

# the three comparison shots (written to the REPO ROOT, and *.png is gitignored)
SEADS_SLED_DEBUG_MODE=1 SEADS_SLEDCAM="2.2,35,12" \
  SEADS_SLED_RIG_SMOKE="0,0,0,0,0,0,0,0,<thr>,<brk>" \
  ./build-play/seads.exe --smoke 40 ctl_<name>.png
```

⚠ The sled needs ~40 smoke frames to seed; at `--smoke 3` the hero GLB has not
loaded and you will see neither the log line nor the machine. And
`SEADS_SLEDCAM` framing is fragile — `"2.2,35,12"` is the one known to frame the
rider from behind-left; other values put the camera on the aircraft instead.

### 7.5 The map of what R2c-5 touched

| file | what to know |
|---|---|
| `render/rider_pose.h/.cpp` | the 4 pure functions + the two 20° constants. PURE, in `seads_render_core`, unit-testable. Anything new that can be pure belongs HERE, not in `sled_model.cpp` |
| `render/sled_model.cpp` | `capture_rest_ik` derives the side + both signs at load; `pose_pass` applies them. raylib-only TU — **no ctest reaches it**, which is why the env-var log exists |
| `render/sled_model.h`, `render/draw.h/.cpp`, `app/main.cpp` | the two channels, plumbed from `sim::SledInputs` (the numbers the kernel stepped with, never a second animation state) |
| `test/unit/test_rider_pose.cpp` | 7 new cases; the last one pins the ruled directions against the shipped GLB bytes and will fail on a mirrored re-export |
| `assets/character/sudburian_src/seat_sudburian.py` | the Blender reference. If you change one side's pose law, change BOTH files or they drift |
