# R2c-M2 — CUFF, THUMBS, THROTTLE PADDLE, BRAKE LEVER, FINGERS-ON-BRAKE

**Chad, 2026-08-18 (his eye on R2c-M attempt 1), verbatim items:**

1. "The gloves cuffs need to connect to the main part of the glove hand."
2. "The throttle for the snowmachine needs to be a curved wedge shaped like the
   right thumb currently is (likely misattribution)." … "the curved shape of the
   throttle also means that its curvature allows a wrapping half way around the
   bar (conforming) to half of the cylindrical steering bar shape and holding
   that curve in the wedge construction (see reference image on internet of
   your choice of a 1992 indy 650 throttle)."
3. "The right thumb needs to be consistent with the left or vice versa."
4. "The throttle needs to connect at the right location on the right side handle
   on the inside connected at its base and angling out at a 45 degree angle
   toward the rider and the thumb needs to be aligned and oriented so that the
   pad of the thumb rests on the throttle, the throttle at idle position.
   Reference 1992 indy 650 throttle placement and then get the right thumb
   correct."
5. "On the left side handle we need to attach a better shaped piece to the left
   handle bar on the outside front distal side of the handle, attached at the
   medial end of the brake lever to the medial side of the handle bar and placed
   correctly. Get a reference image and follow closely as you do for the
   throttle. The brake angle away from the rider."
6. "Then the rider's four fingers of the glove need to extend and rest on the
   brake."
7. **RULING on names/colours/sides:** "brake and throttle are currently named
   incorrect and placed on the wrong side, throttle_block supposed to be black
   and on the right side of the rider, and brake_lever is currently black and
   should be red and is supposed to be on the left side."

Process ruling: Opus codes; every tricky part gets a fresh-context Fable
consult / red-team and an independent verification; the deciding and the
arithmetic are the lead's own.

This spec is the R2c-M attempt-2 correction list (mitts) PLUS two machine art
nodes. It obeys `docs/RIDER_AUTHORITY.md` (Sudburian only, live session only,
never headless) and `SUDBURIAN_LADDER.md` §6 R2c.

---

## 0. MEASURED FRAME (live session, 2026-08-18, neutral seated pose)

Everything below is in the per-side **bar frame** of `mitt_geom._Bar`:
origin = the grip socket (= grip centre to 0.5 mm), `a` along the grip axis
**outboard** (+), `deg`: 0 = machine UP, 90 = machine FORWARD, 180 = down,
270 = REAR (toward the rider). `r` radial from the grip axis. Units metres.

| measured | value |
|---|---|
| grip radius `R_BAR` | 0.0210 (`cylinder_fit`, both sides) |
| grip axial extent | a ∈ [−0.0482, +0.0479] (96 mm) |
| bar tube radius | 0.0163 median; the bar's straight grip segment runs a ∈ [−0.10, +0.055]; the ring at a ≈ −0.10 is where the bend to the riser starts (centre already 7 mm off the grip axis) |
| grip axis, world, rider RIGHT (`grip_L` obj, world −X) | (−0.93684, 0.30433, −0.17238) — swept 19.6° aft, 10° down |
| grip axis, world, rider LEFT (`grip_R` obj, world +X) | (+0.93684, 0.30433, −0.17238) |
| wrist joint rel. socket | 20 out / 83 aft / 51 up (world), 100 mm |
| forearm blockout | 12-gon prism r 0.075, ends in a FLAT DISC exactly at the wrist joint (no taper) |
| mitt palm | a ∈ [−0.064, +0.058], deg 248→422, outer r 0.0685 (knuckle bulge 0.0795 at deg 14–46) |
| thumb root (both hands, unchanged) | `bar.point(−0.044, 288, r_in + 0.0295)` = (a −0.044, aft 0.0495, up 0.016) |
| finger knuckle line | deg 40, `fing_a0 = −0.0435`, pitch 0.029, r 0.018→0.0158 |
| current placeholders (bar coords) | rider RIGHT: `brake_lever` (steel) a[−0.095,0.033] f[−0.079,−0.045] u[−0.010,0.039] — AFT of the bar; rider LEFT: `throttle_block` (red) a[−0.117,−0.041] f[0.034,0.056] — FORWARD of the bar |

**Sides.** Rider RIGHT = world −X = `hand_r` = `grip_L`/`grip_socket_L`
objects (the L/R crossover of §2.3a is real: object `_L` is world −X = the
rider's right). Rider LEFT = world +X = `hand_l` = `grip_R`. `BRAKE_SIDE = "l"`
in `sudburian_proxy.py` is correct and stays.

**Ruling 7 applied:** object `throttle_block` becomes the THROTTLE (block +
paddle), BLACK, on the rider's RIGHT (world −X). Object `brake_lever` becomes
the BRAKE (perch + blade), RED (`indy_red`), on the rider's LEFT (world +X).
Names then match function; the runtime never references either name (graph:
no symbol; only `splice_bar.py`/`indy650_fitout.py` list them). Both are
children of `CH_steer_pivot` and reach the GLB via `splice_bar.py`.

## 1. THE THROTTLE — rider RIGHT, black  *(v4 — CHAD'S RULING 2026-08-18, supersedes v1–v3)*

**Chad:** *"the lever is not curved [along its length] … it is connected by
mechanism to top and bottom and does not curl under the handle but sticks out
at an angle toward the rider (straight out) … connected to the handle on the
medial side [toward] the centre of the steering bars and protrudes toward the
rider and distal toward the outer handle, on level with the plane of the bar,
pointing wedge (straight not curled) toward the rider and rider aligns thumb.
To increase throttle rider pushes it to the bar against the force of the cable
… there is a curve to the wedge: it does not curl down its length but across
the width of the wedge such that its shape conforms down the length of the bar
it aligns to lengthwise, and the rider can therefore squeeze the throttle lever
tight to the bar at full throttle."*

History for the record: v1 (mine) had the horizontal swing but the paddle
inboard; the red team (C1/C2) argued pin ∥ bar / aft-down from generic
thumb-throttle photos; v2/v3 followed that. Chad's XLT video frame and his
words settle it: **pin VERTICAL, paddle STRAIGHT in the bar plane, aft-and-
outboard at 45°, arc cross-section conforming to the bar.** Photos of other
machines are not the reference; his machine is.

**v5 (Chad, 2026-08-18, after seeing v4):** *"the box of the throttle body
assembly needs to be offset toward the centre of the steering bar so that the
lever of the throttle points to the outside from its connection box, and the
box has a little square cap on top which is a press-down red kill switch, to
the left of the right hand … the shape of the throttle thumb lever should
actually match the shape of a thumb (smooth wedge WIDER where it connects to
the box and NARROWS down its length to the rounded vertex) with rounded end.
So the thumb aligns in front and presses it in to the handle from there."*
→ block a ∈ [−0.120, −0.088] (f ±0.026, u −0.024..+0.026, must contain the
bending bar — asserted in the session), red 16×16×6 mm kill-switch cap on top
(`indy_red`, 2nd slot), vertical pin at the block's outboard-aft corner
(a −0.090), paddle wedge REVERSED (half-span 45° at the box → 14° at a
rounded tip), idle 45°, and the throttle-hand palm inboard edge trimmed
−0.064 → −0.052 (a 45° lever from a −0.090 meets the palm's aft surface at
a −0.0515; the trim is what lets Chad's 45° hold without a solver fudge).
v4 had landed at 50° with the wedge the wrong way round.

**v6 (Chad, 2026-08-18, viewport on v5):** *"further medially offset the
throttle housing to the left, and get the thumb on it by moving the thumb
parallel to the handle, aligned, oriented and resting on the throttle … Right
now the Sudburian's right thumb is pointed backwards and the throttle lever is
too close."* → thumb STRAIGHT and PARALLEL to the bar (along −A from the same
root, 58 mm to the tip centre, no Bézier); the pin position is SOLVED so the
45° lever's aft face meets the thumb's tip cap from behind (0.5–1 mm, zero
penetration anywhere) — arithmetic puts it near a −0.153, on the bar's main
run past the swept grip section, so the block is built on the LOCAL bar axis
there (measured ring centres), kill cap on its top; the paddle stays defined
in the grip frame from the pin.

**v7 (Chad, 2026-08-18, same viewing):** *"relax the right hand a bit like
the brake hand is, except the fingers hold the bar and the hand is opened up a
bit to extend the thumb's position back … the new hand shape allows the thumb
to be placed back toward the rider an inch or two and allows the thumb to
align the throttle lever."* → throttle hand: palm heel lifts 32 mm AFT off the
bar (`palm_open_aft`, smoothstep deg 340→250, fingers still wrap), thumb root
moves aft to (a −0.044, aft 0.094, up 0.012) (+44 mm), thumb straight along −A
58 mm; lever shortened to L 0.060 with the pin SOLVED so the lever lies
between thumb pad and bar, touching the tip pad from the forward side
(≈ a −0.144); block follows the pin on the local bar axis. Supersedes v6.

**v8 (Chad, 2026-08-18, viewport on v7 — two messages):** *"the throttle
connection is just jammed into the box on a slant … sloppy … place the box
appropriately first, then resize the throttle lever to meet the thumb, don't
make hardware crooked to align to the thumb … make the shape of the hand the
same as on the left, except set back on the handle … throttle has too short a
lever and too sharp an angle, losing its width down its length at too high a
ratio. Throttle and connector bar need to drop about an inch on the z axis.
The big gauntlet-like collars of the gloves are not very nice, delete those and
that weird piece that occludes the wrist — just keep the gloved hand, the coat
sleeve will end at the hand, one piece for the hand. The bottom of the palms
should not hold a J-shape curve — the palm face should round off."*
→ hardware first: box dropped 25 mm on the local bar, pin SQUARE on the box's
aft face (paddle section radius derived from the pin, never a crooked lug),
lever 30° (not 45°), longer (L solved to reach the thumb), taper 40°→24°;
mitt = ONE piece (palm+knuckles+fingers+thumb; cuff/strap/handback flare
deleted; slim dorsum ends inside the sleeve ≥ 8 mm), palm arc ends at deg 275
(no J-hook), both hands the same construction (`finger_mode="brake"` on both),
the throttle hand SET BACK ~30 mm on the bar so its fingers drape the grip and
its bar-parallel thumb meets the lever. Supersedes §2, §5 and the v5–v7 lines.

**v8b (Chad, same sitting):** *"left hand fingers are too far outstretched,
the brake lever isn't even connected to the box, that's why fingers have to be
nearly straight … first attach the brake lever to the master cylinder and then
make the hand settle on it so that fingers are not fully extended but half
closed resting on the brake lever, then match that with the other hand … the
left and right hand should match level of extension."* → §3's ORIGINAL blade
centreline (Q0 f +0.042 on the pivot boss → tip f +0.075) is restored; gate G9
(≥ 25 mm free finger, red-team C6) is DELETED — it is what pushed the blade
38 mm off the box; fingers half-closed (90–110° curl) with pads on the blade;
right hand same curl ±5°, set back on the bar.

| part | spec |
|---|---|
| block | chamfered box CONCENTRIC WITH THE GRIP AXIS: a ∈ [−0.098, −0.066], f ∈ [−0.024, +0.024], u ∈ [−0.020, +0.026]; the bar tube (r 16.3, ≤ 6 mm off the grip axis at −0.098) is inside it; inboard of the palm's inboard edge (−0.064) because the palm's bore (r_in 0.0225) is smaller than the block corner. `indy_rubber`. |
| pin | **VERTICAL = axis U** ("mechanism to top and bottom"): a lug on the block's aft face with the fork's two prongs above and below; pin line through (a −0.094, deg 270, r 0.030), u −0.026..+0.030. |
| paddle, CLOSED (WOT) | **STRAIGHT along +A** from the pin, a −0.094 → −0.019 (L 0.075), tight along the AFT face of the bar; cross-section = an ARC concentric with the bar, inner r 0.024 / outer 0.029, centred deg 270, half-span 20° at the pin → 55° at the free end (the wedge, narrow at the fork, wide at the tip); rounded edges; NO curl along its length. |
| paddle, IDLE | the closed paddle rotated about the vertical pin so the free end swings AFT and OUTBOARD, level with the bar plane; sign MEASURED. Angle 45° (Chad); if the paddle enters the palm/knuckle solid at 45°, raise the angle in 2.5° steps to ≤ 55°, else trim the throttle-hand palm inboard edge (≤ 12 mm) — whichever, the number is REPORTED. |
| thumb contact | on the paddle's AFT (outer) face at idle: the centreline point whose distance from the thumb root is closest to 0.055 (else the free end); `T`, `n_T` returned. |
| material | `indy_rubber` (Chad: black). |
| R3 hook | throttle input = rotation about the pin toward closed; the closed pose is the sheet ON the bar. |

## 2. THE THUMBS — one construction, two poses

`_thumb_volume` becomes a spine loft: `thumb_loft(spine_points, radii)`, the
SAME radii (0.0295 → 0.0215), the SAME root `bar.point(−0.044, 288,
r_in + 0.0295)`, the same station count, on both hands.

- **brake hand (l):** spine = the present arc (deg 288 → 196, a −0.044 →
  −0.074), unchanged look — the left thumb wraps the grip.
- **throttle hand (r):** spine = a quadratic Bézier root → tip with the tip
  centre `T + n_T · 0.0215` (the pad ON the paddle's convex face, thumb
  pointing down-inboard along the paddle) and the control point pushed
  0.02 aft so the thumb arcs, not stabs. Thumb points aft/inboard/down onto the paddle ("rider aligns thumb"); root→pad length reported.
- WHY NOT one pose (red-team C10): a wrap-under right thumb occupies aft 0–75 /
  down 0–66 mm of the bar — everything a paddle "at the right location" needs
  — so its pad cannot rest on the paddle unless the paddle moves 55 mm inboard
  of the grip end. Same construction, real posture. **Q3 for Chad:** if he
  wants the same POSE both sides, say which.

## 3. THE BRAKE — rider LEFT, red

Reference: a perch / master-cylinder body clamped on the bar just inboard of
the left grip; the lever pivots at the perch's forward corner and its blade
runs OUTBOARD in front of the grip, diverging forward from it and tilted
slightly DOWN toward the tip ("angled away from the rider", red-team C5).

| part | spec |
|---|---|
| perch (master cylinder) | **BLACK (`indy_rubber`) — Chad ruled 2026-08-18: "the brake cylinder box is supposed to be black but yes the lever is red".** Boxy body per the eBay '95 Indy 500 Classic photo: a ∈ [−0.098, −0.066], f ∈ [−0.020, +0.040], u ∈ [−0.018, +0.028], flat top, pivot boss at the outboard-forward corner. |
| pivot | `Q0 = (a −0.066, f +0.042, u +0.004)`; pin axis ≈ U. |
| blade | loft along the centreline from Q0 to the tip `(a +0.060, f +0.085, u −0.010)` — 19° forward of the bar, 14 mm drop; OVAL section (f × u) 0.012 × 0.020 at the base → 0.009 × 0.014 at the tip (the photo's round red lever), rounded tip, slight aft hook over the last 15 mm (tip f −0.006). Resting gap grip-surface→blade 21 mm at the base, 64 at the tip (red-team C6: the blade must sit at f ≥ 0.06–0.085 or the extended fingers never leave the knuckle bulge, deg 36–68 r ≤ 0.0795). |
| finger contacts | for finger k (a_k = −0.0435 + 0.029 k) the blade's centreline point at a_k and its top face — returned by `brake_geom()`. |
| material | blade `indy_red`; body `indy_rubber` — TWO slots on `brake_lever` (splice_bar verify now reads all primitives, two-way). |

## 4. THE FINGERS ON THE BRAKE — brake hand only

`finger_mode = "brake"` on the brake hand: each finger is a spine loft (same
radii/pitch/count/root as the grip fingers) from the knuckle
`bar.point(a_k, 40, r_in + r_k)` forward and DOWN so its underside rests on the
blade's top face at a_k (spine passes at blade_top + r_tip) and continues
0.015 past the blade's front face curling 25° down (fingertips draped over the
lever). Gate G9: ≥ 25 mm of each finger's spine lies OUTSIDE the palm+knuckle
solid before the contact point (else the fingers do not read as extended).
Throttle hand fingers stay `"grip"` (unchanged).

## 5. THE CUFF CONNECTS

- `handback` first station moves TO the wrist with radii (0.080, 0.078) — it
  swallows the forearm's r 0.075 end disc — tapering to (0.052, 0.036) at
  30 mm toward the fist, then as now into the palm.
- `cuff_start` 0.015 → 0.000 (the head rim sits ON the wrist), head cap 0.15
  kept (rim, not dome). The forearm disc is then inside leather from both
  sides.

## 6. GATES — measured in the LIVE session on the POSED mesh, both hands

Existing (kept): census 0/0/0/0 per hand, directed winding 0, dihedral fold 0,
every wrapping piece ≥ r_bar + 2 mm from the real grip axis, fingers leave the
top (brake hand exempt from the wrap test only for the four fingers — they no
longer wrap; they must still be ≥ r_bar + 2 mm from the axis).

New:
| gate | test |
|---|---|
| G1 cuff connected | piece-overlap graph (vertex-in-closed-solid parity, both directions) over {palm, knuckles, handback, cuff, strap, finger0-3, thumb} is ONE connected component, both hands. |
| G2 forearm end hidden | all 12 forearm-end vertices at the wrist are INSIDE the mitt solid (parity), both hands. |
| G3 thumb on paddle | throttle hand: min distance from the thumb's TIP CAP RING + pole (the pad) to the paddle surface ≤ 3 mm; max penetration ≤ 1.5 mm (measured as thumb verts inside the paddle solid, depth via nearest paddle face). |
| G4 fingers on blade | brake hand: each of the 4 fingers min distance to the blade ≤ 3 mm, penetration ≤ 1.5 mm. |
| G5 nothing in the grip | paddle/block/perch/blade verts all ≥ r_bar + 1 mm from the grip axis over the grip's extent, and the block/perch inboard of a = −0.064. |
| G6 paddle vs fist | paddle + block verts outside EVERY mitt piece except the thumb (parity), else the tip flare/centre moves inboard — measured, not assumed. |
| G9 fingers read | brake hand: ≥ 25 mm of each finger spine outside the palm+knuckle solid before contact. |
| G10 seen | close-up shots (both hands, outboard + rear-inboard) in the sheet; the paddle and blade must be visible under the mitts. |
| G7 machine untouched | `splice_bar.py` verify: **123** other mesh nodes byte-identical (the two lever nodes change by design); splice_sudburian.py 132 machine nodes byte-identical, skinned rest == live posed 0.000000 both ways. |
| G8 tests | `[rider_pose],[rider_rig]` 50/50; full suite = 1237/1241 (same 4 GI4 test_sled). |

## 7. PIPELINE (unchanged shape, two new scripts)

```
edit sudburian_src/control_geom.py (new), mitt_geom.py, sudburian_proxy.py
python sudburian_src/mitt_geom.py                      # self-test incl. G1/G3/G4 in rest space
live:  exec sudburian_src/apply_levers_live.py         # NEW: replace throttle_block / brake_lever
                                                       # mesh data + material + side; NO save
live:  exec sudburian_src/apply_live.py                # mitts in place; NO save
live:  exec sudburian_src/verify_controls_live.py      # NEW: G1-G6 on the posed mesh, prints numbers
       -> viewport look; back up .blend; save; SAY SO
selection export rig+proxy (posed)  -> scratch/sudburian_seated.glb + ref
selection export CH_steer_pivot + children -> scratch/bar_live.glb + ref
python splice_bar.py scratch/bar_live.glb --ref ...
python splice_sudburian.py scratch/sudburian_seated.glb --ref ...
cmake --build build-play --target seads ; shots (SEADS_SLEDCAM="2.6,-40,14" + close-ups)
ctest
```

## 8. OPEN QUESTIONS (one each, for Chad; not blocking)

- Q1 (closed by the in-situ photos): the paddle hangs aft-down, pin ∥ bar.
- Q3 thumbs: same construction, two poses (right lies on the paddle, left wraps). If Chad wants ONE pose, which?
- Q2 the block/perch sit 20 mm inboard of the grip end because the mitt palm is
  16 mm wider than the grip on that side; a real block butts the grip. Shrinking
  the palm is a mitt change he did not ask for — left alone.
