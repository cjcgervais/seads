# THE FLAK GUN — a 20 mm Oerlikon the Sudburian walks up to and fires

Fable spec, 2026-08-27. Chad's ask (verbatim intent): *"an aa gun that the sudburian can
operate ... a really cool and fun to operate flak gun for the game, about 50ft away from the on
surface pumps for each side ... walk up to a flak gun and engage / operate it looking down
sight ... Simple look, but effective, good sight and smooth effective operation ... capable of
taking out enemy ai in the air who would be attacking the pump."*

Branch `sandbox/flak-gun` (off `sandbox/r4a-phase0` `2228182b3`). This rung ships **the model,
its measured contract, and the plan**. It does NOT ship the walk-up (there is no on-foot state
in the tree — §7.1), the loader, the fire control or the sight camera; §7 stages those.

Three fresh-context consults fed this (hardware research with sources, an adversarial design
red-team, a read-only codebase integration survey); their findings are folded in below and
marked **[RT]** / **[HW]** / **[CODE]**.

---

## 0. THE STATE

| artefact | where | state |
|---|---|---|
| the generator (every number sourced) | `assets/flak/flak_src/flak_gun_geom.py` | built LIVE in the GUI Blender over MCP |
| the exporter | `assets/flak/flak_src/export_flak.py` | selection-only GLB, sled export settings |
| the .blend | `Game_loop_idea/vehicle_program/blender/flak_gun.blend` | NEW file, saved; `indy650.blend` untouched |
| the shipped asset | `assets/flak/oerlikon_mk4.glb` | 176 552 B, 3 048 verts, 8 mono materials, sha256 `39454654…54dd` |
| the contract | `render/flak_gun.h` | pure, header-only, in `seads_render_core` |
| the pins | `test/unit/test_flak_gun.cpp` | 6 cases / 191 assertions, opens the shipped GLB |
| the graph | `generated/graph/` | regenerated, layer check OK |

---

## 1. ★ THE CONFLICT AND THE QUESTION — BOTH RULED (Chad, 2026-08-27, in session)

> **RULING 1 — placement:** *60–100 m on the threat flank* (option b). Built as
> `render::flak::kFlankOffsetM = 80` (the mid of the band, a placement dial).
> **RULING 2 — S9:** the flak gun is **ADDITIVE** to WINTER_LAW §1's FPV drone
> (drone = the sled's AA, flak = the pump's AA). WINTER_LAW carries a dated
> amendment note; S9 stands.
> **RULING 3 — keep going:** F-PLACE/F-LOAD/DRAW landed `d3b693a46`;
> F-FIRE/F-SIGHT landed the same session (see §7 status).

The original question, kept for the record:

**Conflict (surfaced, not resolved by me):** `WINTER_LAW.md` §1 rules *"Anti-aircraft is
manually flown, not fire-and-forget"* — the S9 FPV drone. This ask adds a **second**, pump-local
AA weapon. I have treated the flak gun as ADDITIVE to S9 (the drone is the sled's weapon; the gun
is the pump's), not a replacement. If you meant it to replace S9, say so and §1 of WINTER_LAW
gets a dated amendment.

**Question — the 50 ft:** the surface pump today is a **24 × 30 × 24 m placeholder cube** with a
25 m hit sphere (`render/draw.cpp:2941`, `combat/conquest.h:57`). 50 ft (15.24 m) from its
*face* is 27 m from centre. The red-team measured what that costs **[RT P0-3]**: from 15 m off
a 24 × 30 m face the cube masks the sky to **63° elevation over ±39° of azimuth**, and the gun
sits 2 m outside the hit sphere, dead on the overshoot line of any strafe within ±30° of its
bearing. Real light-AA doctrine dispersed guns 100–300 m off the vital point on the threat flank.
Two honest readings of your words:

- **(a) 50 ft as stated** — the gun is *at* the pump, part of its silhouette; the mask is the
  price, and the raider that comes over the cube gets the tail shot.
- **(b) 60–100 m on the threat flank** (toward the enemy pump — Onaping ↔ Coniston is a known
  bearing) — full sky, the strafer crosses the gun's front, the gunner is out of the beaten zone.

I built nothing that depends on this; it is one number in the placement rung (§7.2). **Your
word wins.** Note also the real pump art is unbuilt (S-pumpcube is a placeholder) — if the
final pump is smaller than 24 m, (a) masks less.

---

## 2. THE DESIGN

### 2.1 The pick — US Navy 20 mm/70 Oerlikon on Mount Mark 4 (Mod. 2/3) **[HW]**
The one gun in the class that ONE man trains, elevates and fires with no crew: he stands behind
it, both shoulders in two rubber pads, strapped in, hands on the two spade grips, eye at the peep,
and **swings the whole gun with his body**. No handwheels — the red-team's arithmetic **[RT P0-2]**:
a 109 crossing at 265 m/s needs **38°/s at 400 m, 76°/s at 200 m**; a hand-cranked traverse peaks
30–40°/s, so the Flak 38 (seat + handwheels + computing sight, the runner-up) would ship *slow and
dull*. Silhouette: pedestal → yoke → long tube with a flared bell → the 60-round drum on top →
the cartwheel ring sight → two pads. Sources: OP 909 (mount), OP 911 (gun), NavWeaps.

### 2.2 The operating loop (what the fun IS) **[RT] [HW]**
1. **Walk up** to `st_approach` (1.7 m behind the axis, inside the 10 ft working circle), step
   to `st_foot_l/r`, hands to `st_grip_l/r`, shoulders to `st_pad_l/r`. Facing must match the
   approach so mounting never spins the gun 180° **[RT P2-12]**.
2. **Free-gun slew.** The aim DEMAND is unsmoothed (the mouse, §9.1 discipline); the GUN carries
   a rate cap — the gun's mass, not the hand. `render::flak::slew_toward`, short way round.
   Defaults **90°/s train, 60°/s elevation** are FEEL DIALS, not facts; Chad judges them.
   Elevation **−5° … +87°** [OP 909] — the 3° dead cone overhead is real and wanted: the plane
   that pulls out over the gun is taken on the tail, not chased through a gimbal spin.
3. **The sight.** Physical Mk 5 ring foresight — bead + rings at **100 / 200 / 300 mils** [OP 911]
   subtending at the rear peep — is the character of the object. But a ring alone cannot solve a
   25° crossing lead to the ±2.5 % a 0.64° target needs **[RT P0-1]**; so the existing pure
   `render::lead_solution` draws a faint pipper INSIDE the ring (what the real Flakvisier 38 did
   mechanically). Ring = the read; pipper = the answer; the gunner does the smoothing.
4. **Fire.** 450 rpm cyclic [NavWeaps] — **7.5 Hz**, distinct from the fighter's 12 Hz cannon
   **[RT P2-10]**. 835 m/s. One **60-round drum ≈ one 5–11 s strafing pass** **[RT P1-4]**: the
   reload is the breath between passes, by design (not the Flak 38's 20-round box = 2.7 s).
5. **The flak read.** 20 mm makes no puffs by itself **[RT P1-5]**; so (i) rounds **self-destruct
   at ≈1.6 s (~870 m)**, the range where they drop below aircraft speed anyway — a curtain of
   small flashes that is ALSO a ranging cue (puffs behind the plane = it is inside your envelope);
   (ii) a **proximity burst, R ≈ 4 m** on aircraft, HE damage — the game licence BF1942's Flak 38
   took, and why it was loved. Both are `weapon::` dials, knob-off = the bare 20 mm.
6. **Look down the sight.** The sight camera is AT `st_eye`, exactly on the peep→bead axis
   (parallax breaks the ring otherwise **[RT P1-7]**), no roll ever, head mesh hidden, the mitts
   on the grips and the pads at the bottom of frame ARE the shot. `render::flak::sight_camera`.

### 2.3 The envelope it must fit **[CODE]**
Raiders steer straight at `pump.pos`, orbit on-station within **1000 m slant** (`combat/raid.h:33`
`raid_range_m`), and are floored by terrain-avoid at **250–400 m AGL** (`config/scenario.toml:434-435`).
The barrel's effective reach is ~900 m **[RT]** (round speed 424 m/s at 800 m, drag 0.0008/m).
That is a match, not a coincidence: real Oerlikon doctrine opened at 1,200–1,300 yd and was
effective inside 1,000 yd [NavWeaps]. Time of flight 0.6 s at 400 m, 1.4 s at 800 m; lead 9–25°.

### 2.4 The sphere **[RT P1-6]**
Horizon from a 1.78 m eye on R = 15 km is **231 m**; a raider at 50 m appears at ~1.45 km, 5 s
before overhead at 265 m/s. The mount frame is LOCAL (`make_mount_frame`: train axis = local up,
`fwd0` re-projected into the tangent plane so a caller cannot tilt the pedestal), the pipper's
gravity is local-up, and the ring is the gunner's horizon cue at 80° elevation.

---

## 3. THE MODEL — every number and where it came from

Frame: glTF +Y up, **+Z = muzzle forward**, +X = gunner's right, origin = pedestal base centre on
the ground. Built in Blender with the muzzle on −Y so the exporter's Y-up swap lands it on +Z.

| part | value | source |
|---|---|---|
| trunnion height (jack-screw range) | 1.181 – 1.581 m | OP 909 (46½ / 62¼ in) |
| **built trunnion height** | **1.543 m** = his shoulder joint in boots 0.980+0.533+0.030 | SUDBURIAN_LADDER §3 (inside the range) |
| stand bearing OD / ID | 0.749 / 0.610 m | OP 909 |
| base bolt circle, 5 × 1⅛ in bolts | 0.670 m | OP 909 |
| pad face behind the train axis | 0.902 m | OP 909 (35½ in, Mk 4 rest) |
| working circle (the snow pad) | 3.048 m dia | OP 909 (10 ft) |
| elevation / train | −5° … +87° / 360° | OP 909 |
| gun overall | 2.210 m | OP 911 (87 in) |
| barrel forging / flared bell | 1.452 m / first 2 in | OP 911 |
| drum | 15 in dia × 8 in thick, transverse axis, on top, right-offset | dealer listing (8×15×12 in) + photos |
| pad spread (the adjustable knob, set to HIM) | 0.470 m c-c | SUDBURIAN_LADDER §3 biacromial |
| ring sight | 100 / 200 / 300 mils at the peep | OP 911 (Mk 5) |
| muzzle speed / ROF / drum | 835 m/s / 450 rpm / 60 rd | NavWeaps, OP 911 |
| vertex budget | 3 048 (< 65 535) | buildings.cpp u16 rule |

Stations as exported (glTF, metres): trunnion (0, 1.543, 0) · muzzle (0, 1.623, +1.448) ·
pads (±0.235, 1.643, −0.902) · grips (±0.150, 1.523, −0.802) · eye (−0.140, 1.783, −0.604) ·
peep (−0.140, 1.783, −0.504) · rings (−0.140, 1.783, −0.004) · drum (0.100, 1.873, −0.150) ·
eject (0.115, 1.623, −0.350) · feet (±0.200, **0.000**, −1.052) · approach (0, 0, −1.700).
His eye in boots is 1.762 (0.936 H + 0.030); the sight line is 1.783 — he leans 2 cm in, he does
not stoop.

**Two defects the loop caught before Chad's eye, both by MEASUREMENT not by looking:**
- the gunner's-eye screenshot (camera placed AT `st_eye`) showed the **drum square in the sight
  line** — the real Mk 4 sight bracket is outboard LEFT; moved to x = −0.14, drum to x = +0.10.
- the independent GLB parse (plain Python, no Blender) found the **feet and approach point at
  y = 0.943** — authored relative to the train head. Fixed at source; the test pins y == 0.

---

## 4. ★ OPEN DIMENSIONS — flagged [DERIVED], not measured (Chad's word wins)
receiver box 0.19 × 0.16 m · bore 0.08 m above the trunnion · barrel OD 0.080 → 0.045 m ·
barrel-spring casing Ø0.110 × 0.55 m · grip spread 0.30 m, grip length 0.14 m (> the 0.135 m
mitt) · peep→rings 0.50 m, sight line 0.16 m above the bore · eye 0.10 m behind the peep ·
carriage arm inner spread 0.36 m · column radius 0.19 m · feet 0.15 m behind the pads, 0.40 m
apart · approach 1.70 m · the trunnion taken ON the train axis (offset undocumented) · the 24.5
arc-min sight droop (750 yd zero) is NOT modelled · no back strap (it would have to open for
mounting; the pads read without it **[RT]**). Each is one constant in the generator.

---

## 5. WHAT THE HEADER GIVES THE NEXT RUNGS (`render/flak_gun.h`)
`Stations` (the st_* table, model frame) · `MountFrame` (pos, local up, tangent fwd0/right0) ·
`Pose{train, elev}` · `aim_to_pose` (world aim → pose, clamped) · `slew_toward` (rate-capped,
short way) · `train_quat` / `cradle_quat` (**the ONE sign** — +X rotation drops the muzzle, so
the cradle node gets `angleAxis(−elev, +X)`) · `cradle_station_world` / `train_station_world`
(where a hand target / foot target IS under a pose) · `bore_dir_world` · `sight_camera`.
Pure glm; raylib-free; the loader is app-side (§7.3).

---

## 6. THE GATE
`test/unit/test_flak_gun.cpp`: (A) the shipped GLB — hierarchy (`flak_root › flak_train ›
flak_cradle`), every station an EMPTY under the right parent, the driven nodes mesh-less, verts
in budget, trunnion == 1.543 and inside OP 909's range, pads at −0.902, gun 2.210 through the
bracketing stations, sight axis collinear + outboard of the drum, feet on the ground, eye within
30 mm of his, mono materials; (B) kinematics at `up = normalize(1,1,1)` — no world axis is up:
+train = right, +elev = up, the −5/+87 clamp, bore round-trip (kills the cradle sign flip),
short-way rate-capped slew, station transforms (muzzle rises ABOUT the trunnion, feet never
elevate), sight camera on the bore with no roll. **191/191.** Full gate: see the handoff line
in the commit.

---

## 7. THE RUNGS AFTER THIS ONE (in order; each Chad-flown)
1. **F-WALK — DONE (lane `loop`, L5, 2026-09-03).** ~~Blocked on R5+.~~ The on-foot state, the
   walker and the outer mode manager all exist now (`app/player_mode.h`, `app/interact.h`,
   `sim/walker.*`), so the mount scaffold this item described — "the gun can be reached the way
   the sled is", i.e. **30 m from a stopped sled or a landed aeroplane** — is **DELETED, not kept
   as a fallback**. O now needs all three of: `PlayerMode::Afoot` (the keys are the man's), the
   walker upright (`sim::WalkerMode::Afoot`, read BY ENUMERATOR NAME — you do not man a gun
   face-down in the snow), and the man standing inside `[interact] gun_reach_m = 3.0` of the gun's
   own **`st_approach`** station — baked into the GLB since F-PLACE and, until this rung, read by
   nothing. It is a TRAIN station, so the mark rides the platform's train angle and "walk up to
   it" means the same thing at every bearing. O again puts him down ON THAT MARK (facing away),
   not back where he mounted. The geometry is `app/flak_walkup.h`; the legs are
   `test/unit/test_flak_walkup.cpp`.
2. **F-PLACE — DONE `d3b693a46`.** One gun per SURFACE pump (`pumps[0]`, `pumps[1]`), at Chad's answer to §1, on a
   packed-snow pad (the 0.77 m snowpack floats or sinks a bare pedestal **[RT P1-8]**), facing
   the threat bearing, carried to draw via a `FrameInfo::flak_guns` vector beside
   `conquest_pumps` (`render/draw.h:740`, filled `app/main.cpp:4706`).
3. **F-LOAD/DRAW — DONE `d3b693a46`** (flat tint, named trade; SEADS_FLAKCAM / SEADS_FLAK_POSE smoke rigs certify visually). A `render/flak_model.cpp` copying `sled_model.cpp:902` `load_model` (cgltf,
   full hierarchy, parents-before-children order, `lround` on `base_color_factor`), eye-relative
   mount matrix (`sled_model.cpp:4623`), the two driven nodes written from `Pose` each frame.
   NOT raylib `LoadModel` — it flattens the hierarchy.
4. **F-FIRE — BUILT this session** (`app/flak_tick.h`, pure + headless-tested: cadence, drum, reload, self-destruct, the synthetic shooter; prox burst + burst FX still OPEN). Its own `weapon::GunWorld`; `fire_tick` with a synthetic zero-velocity
   `sim::SimState` built from `bore_dir_world` (the API reads shooter pos/orient/vel, nothing
   else); `combat_tick` is shooter-agnostic and reuses `ke_damage` unchanged; do NOT pass the
   flak pool to `conquest_tick` (it would shoot its own pump). Dials in `config/world.toml
   [guns]` (`flak_muzzle_speed`, `flak_rof_hz`, `flak_selfdestruct_s`, `flak_prox_m`,
   `flak_prox_damage`); drag single-sourced from `cannon_drag_k_per_m`. Knob-off bit-identical.
   `combat_player_tick` makes the man hittable only if fed his state — a design choice for the
   rung (the red-team's "beaten zone" point).
5. **F-SIGHT — BUILT this session** (sight-axis camera pulled 2.9 m back past the 2 m near plane — parallax-true; lead pipper double-circle + FLAK drum HUD; mil-ring overlay + beacon dimming OPEN). A third camera block beside the sled chase override (`app/main.cpp:3836`), pose
   from `sight_camera`, FOV ~60°, near clip past the helmet, `KEY_V` is Chad's (do not take it);
   the pipper inside the ring via `lead_solution` (`render/gunsight.h:173`); the beacon column
   dimmed from the sight view **[RT P2-11]**.
6. **F-POSE.** The Sudburian on the gun. ★★★**SUPERSEDED BY P1-10 (Chad, 2026-09-01): "JUST
   HAVE TO LET THE MAN STEP BACK AND HOLD THE SHOULER PADS LIKE THEY ARE HANDLES."** The
   original shape below (hands to `st_grip_l/r`, pads to shoulders, feet to `st_foot_l/r`)
   was built through round 4 and is **dead**: it requires a 1.643 m shoulder line and a
   1.783 m eye, and the measured Sudburian tops out at **1.531 m dead straight**. Everything
   that mismatch caused — the −0.14 m pad fudge, the head pinned at bore height, the helmet
   90 mm inside the receiver — was a symptom. ★The weld is now **HANDS to `st_pad_l/r`**, the
   shoulders are SOLVED an arm's working reach behind and below the handles, and the feet are
   derived under him: he **steps back** and holds the pads like handlebars. `st_grip_l/r`
   remain drawn geometry. The P1-9 crouch survives — the handles descend and swing forward
   with elevation and he follows them down — but he never folds over the gun again (measured
   torso pitch stays inside ±9° over the whole curve, against ~74° in round 1). Third person
   only (R5+ ruling: no FP arms rig). See `render/flak_pose.h` and
   `docs/SESSION_HANDOFF_20260828_flak.md` §19.
7. **F-SOUND.** A second `gun_synth` instance at 7.5 Hz with a heavier report + case clatter
   (`app/main.cpp:4908`).
8. **Liveness gate for the whole thing** **[RT]**: a scripted 265 m/s head-on run must be
   killable within two passes; a 400 m beam crossing WITHOUT the pipper must be hard, not
   impossible — and both arms measured before any dial is ruled.
