# Wild Hornets "STING" FPV Interceptor — Hero Modeling Reference Sheet

Prior-established facts (do not re-derive): real Sting tops out ~87 m/s (280–315+ km/h),
ram interceptor with contact charge, bullet-shaped fuselage, X-frame prop arms
perpendicular to the fuselage, stubby wings starting at the last 1/3 of the fuselage,
radar-cue -> FPV two-phase employment.

This sheet adds the fine geometry a hero modeler needs, from photo evidence + published
specs. All dimensions are BEST-ESTIMATE from proportional analysis of reference photos
cross-checked against the one hard number available (17-inch/0.432 m prop class);
confidence noted per item. No official dimensioned drawing of STING is public.

---

## 0. Downloaded reference images

All in `D:\flight_sim2\Game_loop_idea\Reference_pics\sting\` — verified real image data
(checked via `file` magic bytes, not HTML error pages).

| File | What it shows |
|---|---|
| `sting_launch_114tobr.jpg` | **BEST reference.** Soldier holding drone up at arm's length against sky, 3/4 front-low angle. Shows: rounded ogive nose with reflective foil/tape wrap band, boxy slab-sided rear fuselage, single-station X-arms with round/bulb tan motor pods, 2-blade props, a flat foil-taped wing/fin panel at the nose-body joint, thin wire antenna whip curving up off the top rear, small pitot-like stub protruding from the tail knuckle. |
| `sting_silhouette_missile.jpg` | Clean side-profile silhouette at dusk. Confirms: tall ogive nose dome, straight taper into a slab body, a flat rectangular wing spanning the fuselage width positioned right where the nose meets the body (~35% of body length back from tip), 45°-angled X-arms below with bulbous motor pods, one visible 2-blade prop, small block (battery/tail unit) hanging below/behind the arm root. |
| `sting_prep_launch.png` | Soldier kneeling, drone on ground on a flat under-plate/skid, being armed — shows a flat ventral mounting plate / skid and rear cable routing; also shows the "Wild Hornets" unit crest overlay (not part of the drone, ignore). |
| `sting_defenceua_1.jpg` | Macro of ~a dozen finished nose cones standing in a rack (red safety-light workshop shot). Confirms: smooth rounded ogive tip, a **vertical seam/rib line running down the center front** (print seam or bond line), a **horizontal band of fine vertical ribbing/fluting** roughly 35–40% back from the tip (looks like a corrugated collar or shrink-wrap ring where the nose cone mates to the body — good hero surface detail), one nose in raw tan/undyed 3D-print plastic vs rest painted/taped black (confirms 3D-printed shells, hand-finished). |
| `launcher_nlaw_shoulder.jpg` | NLAW anti-tank weapon fired shoulder-mounted — for the in-game gunstock launcher: compact tube on a P-shaped shoulder stock/sling with a sight housing on top, held tight to shoulder pocket, one hand on a pistol-grip trigger, other steadying the front of the tube. Good stock/tube proportion reference. |
| `launcher_stinger_fire.jpg` | FIM-92 Stinger MANPADS firing from the shoulder — reference for a longer launch tube with a separate rear grip/sight unit clipped on, plume/blast at the rear that a game gunstock could stylize as a soft "release puff" instead of exhaust. |

---

## 1. Overall dimensions (meters, best-estimate)

Anchor number: real STING is built on a **17-inch prop class** (prop diameter **0.432 m**,
0.216 m radius) — this is the one hard figure in circulation and is what all other
estimates below are scaled against. Confidence: MEDIUM (widely repeated in coverage,
not an official spec sheet).

| Measurement | Estimate | Confidence |
|---|---|---|
| Fuselage length (nose tip to tail cone) | **0.95 m** | Medium |
| Fuselage max diameter (at the boxy mid/rear section) | **0.16 m** | Medium-low |
| Fuselage min diameter (tail cone) | **0.09 m** | Low |
| Prop diameter | **0.432 m** (17") | Medium-high (published) |
| Arm span, motor-to-motor diagonal (X, opposite corners) | **0.70 m** | Medium-low |
| Wing span (tip to tip, across fuselage) | **0.55 m** | Low |
| Wing chord | **0.10 m** | Low |
| Nose-cone length (tip to nose/body joint) | **0.38 m** (~40% of fuselage) | Medium |

Sanity check: arm span 0.70 m gives ~0.27 m of radial clearance beyond the fuselage on
each side for a 0.432 m prop to spin freely without fuselage strike — plausible for an
X-frame quad of this size.

---

## 2. Fuselage shape

- **Nose:** rounded **ogive** (bullet) profile, not a hemisphere and not a sharp cone —
  the curve is convex and tightens gradually to a blunt-ish tip, like a rifle-bullet nose
  or an artillery-shell nose. Confirmed directly in `sting_defenceua_1.jpg` (rack of nose
  cones) and `sting_silhouette_missile.jpg`.
- **Nose seam:** a visible vertical parting-line/seam runs down the center of the nose
  cone front face — model this as a fine raised or scribed line, not flush.
  A horizontal fluted/ribbed collar band sits ~35–40% back from the nose tip, where the
  cone probably mates to the main body tube — good hero-detail opportunity (small ridge
  ring, 8–12 vertical corrugations).
- **Body:** immediately aft of the nose cone the cross-section changes from round to a
  **flattened, slab-sided box** (visible in `sting_launch_114tobr.jpg` — the fuselage
  reads as rectangular/slab in the mid-rear section where the arms and wing root,
  not a continuation of the round nose). Think: round bullet nose grafted onto a flat
  avionics box, a common scratch-built FPV airframe pattern.
- **Taper/tail:** the body tapers down again aft of the arm-root station into a small
  tail knuckle/stub, with a small protruding pin or antenna stub visible poking
  straight back off the tail in the 3/4 photo.
- **Ventral face:** a flat skid/mounting plate on the underside (seen in
  `sting_prep_launch.png` — drone rests flat on the ground on this plate during arming).
- **FPV camera:** not clearly resolved in these photos (all shots show fully-shrouded
  black/tape-wrapped units). Best-estimate placement per typical bullet-fuselage FPV
  interceptor convention: a small round lens window let into the nose cone itself, just
  behind the very tip, canted very slightly down (5–10°) for a forward-and-down FPV view
  — NOT a separate canted turret. Confidence: LOW — model it as a subtle circular lens
  insert flush with the nose skin, camera cover = a small dark glass/acrylic disc, not a
  full dome.

---

## 3. Prop arms, motors, props

- **Count:** 4 arms, X-quad configuration (confirmed, matches prior research).
- **Root station:** **single X station** — all four arms root from one point/cluster on
  the fuselage, not two separate front/rear pairs. Visible clearly in both
  `sting_launch_114tobr.jpg` and the silhouette shot: the arms form a clean X radiating
  from one knuckle just aft of the wing.
- **Cross-section:** arms read as **flat plate/slab**, not round tube — in the 3/4 photo
  the near-side arm shows a flat rectangular face with visible edge thickness, consistent
  with a laser-cut or 3D-printed flat carbon/composite arm rather than a round boom.
  Confidence: medium.
- **Motor pods:** bulbous, rounded cylindrical pods at each arm tip, tan/orange colored
  in the 3/4 photo (raw 3D-print plastic or a heat-shrink motor bell cover) — distinctly
  lighter than the black arms and body. Model as a rounded bell/cup shape around the
  motor, slightly wider than the arm it sits on.
- **Props:** **2-blade**, dark (carbon-look), pusher-tractor mix typical of an X-quad
  (front-left/rear-right spin one way, front-right/rear-left the other — no visual
  distinction needed, both pairs look identical). Blade shape: narrow, slightly curved,
  moderate pitch — standard FPV racing/freestyle prop silhouette, not a wide agri-drone
  blade.
- **Orientation:** arms angle sharply down-and-out from the root (roughly 30–40° below
  the fuselage centerline in the silhouette shot), typical of an X-frame with the body
  riding above the prop plane.

---

## 4. Wings / fins

- **Placement:** a single flat wing/fin element spans the fuselage width at the
  **nose-cone/body joint**, i.e. right at the point prior research already places it —
  "start of the last 1/3 of the fuselage" measuring from the tail forward, which lines up
  with ~35–40% back from the nose tip measuring from the front. Both descriptions point
  at the same station.
- **Planform:** flat, roughly rectangular-to-slightly-tapered plank, foil/tape-covered in
  the 3/4 photo (visible reflective panel). Reads as symmetric left/right stub wings
  rather than a full flying-wing shape — short chord, moderate span relative to the
  fuselage (wingtip roughly level with or just outside the prop arc).
- **No separate vertical fin observed** in any photo — yaw/pitch stability likely comes
  from the flat body cross-section and FC/gyro authority rather than aero surfaces,
  consistent with a hand-built FPV airframe. Do not add a dorsal/ventral fin unless the
  builder wants one for silhouette read — if added, keep it small and treat as
  speculative.

---

## 5. Surface details for hero quality

- **Nose seam line** — vertical center seam, tip to the ribbed collar (Section 2).
- **Ribbed/fluted collar band** at nose/body transition — 8–12 fine vertical ridges,
  reads like a corrugated coupling ring.
- **Foil/reflective tape wrap** — visible as a shiny silver/chrome band and patches on
  the nose and wing in the 3/4 photo; likely real-world thermal/RF signature tape or just
  reinforcement tape over a 3D-print seam. Good hero detail: irregular, hand-applied
  edges, slightly wrinkled, NOT a clean factory decal.
- **Antenna whip** — one thin flexible wire antenna curving up and back from the top of
  the tail/body area (seen arcing above the drone in the 3/4 photo). Single antenna,
  no visible second whip.
- **Tail stub/pin** — small rigid pin or rod protruding straight aft from the tail
  knuckle, thinner than the antenna whip — possibly a VTX antenna or an arming pin;
  model as a short (~4–6 cm) rigid stub, distinct from the flexible wire antenna.
- **Motor pod color break** — tan/undyed plastic motor pods against black arms/body is a
  real, photographed material split — use it as a deliberate two-tone material call
  (see Section 6), it reads well and is cheap to fake in-game.
- **Ventral skid plate** — flat mounting plate on the belly, visible screw/standoff points
  where the drone rests during ground handling.
- **Fasteners/standoffs** — not clearly resolved at this photo resolution; for hero
  quality, add small screw heads at the arm-root cluster and nose/body seam at a
  plausible ~15–20 mm spacing, since a bare 3D-print/carbon build like this is virtually
  always screwed together at those joints.
- **No visible LEDs, unit markings, or roundels** in any reference photo — keep the hull
  clean of insignia; the in-game livery is a separate ally-blue tint pass, not a decal
  callout.
- **Warhead demarcation:** not visually separable from the nose cone in these photos —
  treat the entire nose-cone volume as the warhead/contact-charge section (it is
  described in sourcing as housing the payload), with no visible external seam beyond
  the nose/body collar already noted. Confidence: low — no cutaway reference found.

---

## 6. Color / finish

- **Real-world:** predominantly **matte black** body/arms, with an **undyed
  tan/natural-plastic** motor-pod and (sometimes) nose-cone material visible where parts
  are unpainted 3D-print, plus **silver/chrome reflective tape** patches on nose and wing.
  Overall impression: hand-finished dark composite/print airframe, not a painted
  factory scheme — expect asymmetric tape coverage, no two units identical.
- **Material split for the model (carbon/dark vs light):**
  - **Dark/near-black (carbon-look):** main body box, arms, nose-cone base material.
  - **Light/tan (raw plastic):** motor pods, and occasionally a whole nose cone if
    unfinished — use sparingly, as an accent, not a large area.
  - **Reflective/metallic silver:** tape wrap patches on nose tip and wing — small
    accent areas only, irregular shapes, not full panels.
- **In-game:** per the task brief this will be tinted ally-blue overall; keep the above
  as the underlying *material* split (matte-dark vs raw-light vs metallic-accent) so the
  blue tint reads as a paint/tint over real material variation rather than a flat single
  color model.

---

## 7. Modeling order (suggested)

1. **Nose cone** — ogive primitive, add center seam line, ribbed collar detail at the
   35–40% station. This sets the whole model's scale reference (nose length ≈ 0.38 m).
2. **Main body/box** — slab-sided section aft of the nose, tapering to the tail knuckle;
   add ventral skid plate.
3. **Arm-root cluster** — single knuckle at ~35–40% station (same station as the wing),
   block out the X arm roots before extruding arms, so all four arms share one clean hub.
4. **Wings** — flat plank spanning the fuselage width at the same station as the arm
   root/nose-body joint; keep thin, slight taper is optional.
5. **Arms** — flat-plate cross-section, 30–40° down-angle from body, extend to motor pod
   radius (~0.35 m from centerline for a 0.70 m span).
6. **Motor pods + props** — bulbous pod, tan material; 2-blade prop, dark carbon-look,
   4x.
7. **Tail details** — antenna whip (flexible wire), tail stub pin, any small fasteners.
8. **Surface pass** — seam lines, tape patches, screw heads, material ID (dark/tan/metal)
   ready for the ally-blue tint shader.

---

## 8. ASCII proportion diagram

Stations given as fraction of fuselage length, measured from nose tip (0.0) to tail (1.0).

```
SIDE VIEW  (nose tip -> tail)

 0.00                0.38          0.55                       1.00
   |--------------------|------------|--------------------------|
  tip              nose/body      arm-root+           tail knuckle
  (ogive           joint,         wing station         + antenna stub
   apex)           ribbed         (X-arms angle
                   collar          down-and-out
                   band            from here)

      ___
     /   \
    /     \___________________
   (        |    ____====____  \.__
    \_______|___/         \___\____\___
            |  /  ARM        ARM \
            | /     \       /     \
   nose     |/       O     O       (motor pods, props)
   cone     wing    (X arms recede
   (0-38%)  (38-48%) into/out of page,
                     shown schematically)


FRONT VIEW  (looking aft from ahead of the nose)

              ______
            /        \          <- nose cone (round, ogive)
           |    o     |            camera lens ~centered, low
            \________/
         ____|      |____
        /    (body slab)  \      <- body flattens to a slab just
       |                   |        behind the nose
        \_________________/
       \                   /
        \                 /      <- wing spans out here (0.38 station)
   ______\               /______
  (arm)   \             /   (arm)
     \     \           /     /
      O     \_________/     O    <- motor pods, ~0.70 m apart tip-to-tip
   (motor)                (motor)
```

---

## 9. Confidence summary

- **High:** X-quad, single arm-root station, ogive nose, wing at ~1/3-from-tail station,
  2-blade props, matte-black-plus-tape-plus-tan-plastic material story.
- **Medium:** absolute dimensions (scaled off the 17" prop class), flat-plate arm
  cross-section, arm-span figure, nose seam/collar detail.
- **Low:** exact camera placement/cover geometry, warhead section demarcation, fastener
  layout, wing planform detail, presence/absence of a vertical fin. Flagged inline above
  — the builder should treat these as reasonable hero-modeling choices, not verified fact.
