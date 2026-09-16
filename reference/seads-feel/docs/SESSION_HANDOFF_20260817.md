# HANDOFF — 2026-08-17, winter-gi / `sandbox/gi4-ride`

Read this **and** `docs/SUDBURIAN_LADDER.md` §3 before touching anything.

---

## 0. THE ONE-LINE STATE

**R1 is closed. R2 is staged R2a/R2b/R2c. R2a is done, R2b's SEATING is SIGNED
by Chad's eye. NEXT = R2c (costume, textures, scarf).**

**★ STEERING, SHROUD AND DASH WORK IS DEFERRED BY CHAD. Do not pick it up.**
His words: *"I will defer the steering stuff the cutout and all that... We need
to get back on task."* Nothing in this handoff should send you there.

---

## 1. GIT STATE (nothing pushed)

`sandbox/gi4-ride`, in `D:\seads_sandboxes\winter-gi`:

```
363c885e0  R2b: the Sudburian is seated on the machine, reproducibly   <- HEAD
d8f8d3691  proxy: rebuild constants onto the corrected §3
2eaad13bf  ladder §3: surface vs joint-centre correction, defects 3+4 STRUCK
1b900db4b  graph: regenerate
e625f3de2  ladder: Chad's R2 rulings — costume closed, R2 staged
b3a59ce9f  export: export_apply is False
```

Gate baseline is unchanged and still the four known pre-existing GI4 reds:
`529 sled_slides_before_it_tips_on_flat_snow`, `530 sled_grip_ceiling…`,
`563 sled_assist_reference_plane…`, `566 sled_debug_sink_is_write_only`.
Any other red is yours.

---

## 2. WHAT LANDED

### The export bug — a live hazard, not a stale doc (`b3a59ce9f`)
`export_indy650.py:116` shipped `export_apply=True`. **It is `False`.** True adds
**+25,636 triangles across 17 nodes** of BEVEL/SOLIDIFY — all machine, no rider.
Verified without Blender: the shipped GLB carries RAW counts (`pan_skin` 8,804 =
4,402 polys ×2). Also pinned `export_rest_position_armature=True`, which is now
**load-bearing**: the `.blend` carries the IK-SOLVED pose, so without it you
export an IK pose against rest-derived inverse-bind matrices.

### §3 corrected — two defects were never real (`2eaad13bf`)
A **surface vs joint-centre** conflation, the third time this class of error has
cost this ladder a rung. Biacromial is acromion-to-acromion, ~25 mm/side outboard
of the glenohumeral centre. §3's own closure test decides it: read as surface the
outer shoulder computes to 0.643 against §3's stated 0.640; read as joints, 0.704.

- **Defect 3 (shoulders narrow) STRUCK at the joint level.** The rig's 0.380 GHJC
  span is correct for its body. The narrowness you can *see* is suit volume
  (~0.46 vs 0.640) — **R2c mesh work, not joints.**
- **Defect 4 (hips narrow) STRUCK, and backwards.** Correct inter-HJC is 0.190;
  0.200 is 6 % **wide**. **Never widen the hips.**
- `Pelvis → C7 0.533` was mislabelled — it is hip-JOINT → shoulder-JOINT (0.288 H).
- **The one real defect: hip→shoulder 0.394 vs 0.533 = 71 % of "reads short".**

**Stature RULED 1.850 m.** A "he's secretly a 2 m man" hypothesis was tested and
**refuted** — ten rows back-solve to mean 1.8487, spread 9.7 mm. On screen he is
1.905 with boots and helmet against 1.709 before: **+196 mm, mostly torso.**

### Costume RULED — R2c is unblocked
References win on **suit and boots** (grey one-piece Klim Ripsa, black Baffins);
the **helmet stays FULL-FACE** — reference (c) overruled explicitly. So no peak,
no goggles, no breath deflector, and **no balaclava** (the chin bar keeps the
no-face guarantee for free). That removes four meshes, the four riskiest.
**Cost R2c must pay:** the one-piece deletes the belt line, one of §3 rule 3's
four silhouette breaks. Win it back from the waist straps and chest pocket seam.
The Ripsa's full-length side leg zip **is** the `kComplementBlue` stripe line.

### R2b — the Sudburian is seated, and Chad signed it ("perfect")
`assets/character/sudburian_src/seat_sudburian.py` rebuilds the seating from
scratch and asserts every contact. Run it inside the `indy650.blend` GUI session
after `sudburian_proxy.py` has built the rig.

```
pelvis  0.68 along the seat      knees  ±0.265, ~48° V, clear of the tank
feet    forward on the runners   hands  both grips, 0.00 mm
        toes 43 mm below heel    lean   52°
```

---

## 3. ★★ THE FIVE TRAPS — all encoded in `seat_sudburian.py`, each cost real time

1. **The L/R crossover** (§2.3a). After the 180° yaw his anatomical LEFT is at
   +X, so `hand_l` welds to `grip_socket_**R**`. Pair `_l`→`_L` and it presents
   as a 0.6 m physics bug, not a naming bug.
2. **Mirrored poles.** The same `pole_angle` on both sides bends **one joint
   backwards**. Each side carries its own angle.
3. **Foot roll ≠ foot direction.** Aligning the bone axis alone leaves the soles
   facing inboard at the track. Needs axis forward **and** sole normal at −Z.
4. **IK targets are not parented to the sockets.** Move machine geometry and they
   go stale silently, welding the hand where the grip *used* to be — a 47 mm
   error immune to re-posing. `resync_targets()` exists for this.
5. **★ NEVER SEAT AGAINST A BOUNDING-BOX MAX.** The seat's bbox top is 0.663 but
   it is **0.5983 under him**; the board's is 0.277 but **0.2520 under his boot**.
   Seating to the maxima floated him ~2 inches. Chad saw it instantly while every
   joint measurement read correct — because every joint *was* correct.
   **`drop_onto_surfaces()` raycasts the real surface. Use it.**

**The lean is arithmetic, not taste.** Upright at this seat position the shoulder
is ~0.86 m from the grip against a 0.614 m arm; every cm rearward costs ~1.7°.
"Sits up straight" is unreachable at any rearward seat position, and Chad
accepted the lean: *"a lean is more natural looking… keeps the wind away from his
face, no need to have shoulders right over hips."* `solve_lean()` re-derives it.

---

## 4. NEXT — R2c

Costume, materials, textures and the scarf, on the signed blockout. §4 as ruled
above; §5 for the scarf. Census gate is already implemented in
`sudburian_proxy.py` (`census()` reports non-manifold / boundary / loose /
zero-area). **The ROM sweep does not exist yet** — shoulder 0–160°, elbow 0–140°,
knee 0–130°, hip 0–100°, no collapse, candy-wrap or pinch. That is real work.

---

## 5. ★ HOW TO WORK WITH CHAD ON THIS — read before your first Blender call

- He **watches the viewport live** and iterates in short imperative bursts.
  **Make ONE small decisive change and show it.** Do not write long analyses
  between his messages; it wastes his time and he will say so.
- When a placement is ambiguous, **ask him to point at the part once.** Guessing
  repeatedly is what burned most of this session.
- **A signed visual is evidence.** When a measurement disagrees with something
  Chad can see, the measurement is on trial — go find the *right* measurement.
  Twice today he was right and my numbers were answering the wrong question.
- **You open Blender. Never headless.** Don't wipe or save over his session
  without asking. `sudburian_proxy.py` starts with `wipe_scene()` — never run it
  blind in a live `indy650.blend`.
- Never boolean coarse hand-modelled geometry; an 8-vertex plate folds.
