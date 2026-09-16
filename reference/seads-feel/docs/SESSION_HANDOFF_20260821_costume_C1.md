> ## ⚠ SUPERSEDED 2026-08-21 by `docs/SESSION_HANDOFF_20260821_costume_C2.md`.
> C1 (the body) and C2 (the garment, T2) are both BUILT and applied in the live
> session. Launch from the C2 file. §3 (measurements) and §9 (traps) below are
> still true and still worth reading; §6's plan is executed.

# HANDOFF — costume rung, ATTEMPT 2 (T1) REJECTED MID-BUILD BY CHAD.
# The rung is now **C1: SHAPE THE BODY.** Not the cloth. The body.

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws, `docs/RIDER_AUTHORITY.md`,
then §1–§4 of this file. Two costume attempts have been rejected on QUALITY,
both for the same root cause, and Chad has now named it himself: the grey
box-and-prism body is the ceiling and it has to be re-authored. Do NOT start by
writing garment geometry. Start at §6, rung C1."*

**This supersedes the plan in `docs/SESSION_HANDOFF_20260821_costume.md`.** That
file's §2 (measurements) and §5 (traps) are still true and still worth reading;
its §4 plan ordering is wrong (see §2 below).

---

## 0. STATE IN ONE LINE

A second costume (T1) was built, applied, measured and **stopped by Chad
part-way through verification** with the ruling that decides the whole rung:
*"A square block body just isnt going to cut it… THe underneath does matter."*
Two attempts have now died of the same disease, and the disease has a measured
name: **every garment surface in this character is forced to be the shape of a
box, because it has to CONTAIN a box.**

---

## 1. CHAD'S RULINGS (verbatim, in order — all three are live)

**RULING A — 2026-08-21, the topology ruling.** (Built at T0, measured green,
rejected on quality. Still binding.)

> "The scarf needs to be wider vertically around the neck the wall of the ring
> needs to go up to the chin and the hoodie fits under it, but the hood part is
> at the back and covering the back of the scarf ring and going over the knot of
> the scarf. But the hood goes within the scarf at the front (covered by it) and
> scarf is covered by hood at the back the rest just covers the rest of the body
> arnms and legs snowpants, and the rest of the hoodie. Cover him up and make
> the scarf free ends rest atop rather than going through"

**RULING B — 2026-08-21, the garment ruling.** (Built at T1. Three of its four
clauses were satisfied; see §4.)

> "The tails of the scarf should come out under the bottom of the hood, the
> scarf ring goes around the front of the neck opening of the hoodie, nice
> baggy hoodie, shaped and covered limbs."

**RULING C — 2026-08-21, the ruling that stopped the build. THE GOVERNING ONE.**

> "A square block body just isnt going to cut it, nor are the arms need some
> shapingt, right now to I can see the spaces between limb parts clothing
> should cover the void. THe underneath does matter. Shape th eparts better,"

Ruling C explicitly overturns a claim the T1 agent made in writing and built on.
It is quoted here so the next agent does not re-derive the same wrong idea:

| the T1 agent claimed | Chad ruled |
|---|---|
| "baggy means the garment is its own silhouette, so the prisms underneath stop mattering; the body does not need re-authoring" | **"THe underneath does matter."** |
| the ceiling in the T0 handoff was overstated | **"A square block body just isnt going to cut it"** |

Standing rulings from earlier that are still in force: TWO-PIECE hoodie +
snowpants (supersedes ladder §4's one-piece Klim Ripsa); GREY hoodie, BLACK
snowpants, kComplementBlue stripe on the outer arm and outer leg; quality target
**T2** = authored mesh + UVs + 2048² textures.

---

## 2. THE FINDING THAT DECIDES THE RUNG — and it is provable in one line

The T1 agent tried to get a shaped figure out of the cloth alone, leaving the
body untouched. It cannot be done, and the reason is not aesthetic:

> **A garment shell must CONTAIN the body shell it covers. The body's torso is
> three sharp rectangular boxes. Therefore every torso garment section must be
> a rounded RECTANGLE — a rounded rectangle is the smallest well-behaved
> section that contains a rectangle. The box corners are what you are seeing.**

Rounding the garment's corners harder does not help: a superellipse at
(box + 8 mm) pulls in to 0.841 of its half extent at 45° and leaves all eight
corners of every torso box standing OUTSIDE the garment (measured at T0, and
that is why `_rrect_ring` exists at all). The block is structural.

**THE BODY, MEASURED IN FULL — this is the whole man, and it explains everything:**

```
grey body: 264 vertices, 162 faces, TOTAL.
  pelvis      8-vert BOX   z 0.932..1.122  half (0.220, 0.150)
  spine_02    8-vert BOX   z 1.075..1.367  half (0.220, 0.150)
  spine_03    8-vert BOX   z 1.367..1.513  half (0.215, 0.155)
  head        8-vert BOX   z 1.609..1.854  half (0.090, 0.115)
  foot_l/r    8-vert BOX   each
  neck_01     12-gon PRISM, 24 verts, CONSTANT radius
  upperarm, lowerarm, thigh, calf (x2) : 12-gon PRISMS, 24 verts,
                                         CONSTANT radius end to end
```

Eleven of the fourteen volumes are a box or a constant-radius tube. There is no
deltoid, no ribcage taper, no elbow, no knee, no ankle, no waist. `sudburian_proxy.py`
says so itself in its own docstring: *"THIS IS A PIPELINE PROOF, NOT ART… Appeal
is explicitly not the gate (§9)."* That was true and correct when it was
written. Chad has now moved the gate.

**Corollary the next agent must internalise:** shape may currently be added
OUTWARD (swells, gathers, bags) but never by TAPERING, because a garment that
tapers below the body's constant-radius prism gets pierced by it. That is why
T1 has no slim wrist and no slim ankle. Re-authoring the body removes this
constraint — it is the *point* of C1.

---

## 3. THE MEASUREMENTS — ALL OFF THE LIVE MESH, NONE INFERRED

These cost this session and the last one. **A C1 re-author needs every one of
them and must not re-derive them.** Re-runnable scripts are named where they exist.

### 3.1 The void Chad can see (`"I can see the spaces between limb parts"`)

Solid-coverage scan outboard along +x at y = 0, by ray-parity containment,
1 mm steps. A gap in a row is a hole you can see through:

```
  z      GARMENT covered x-run              BODY covered x-run
  1.30   0.000-0.250  0.318-0.594           0.000-0.219  0.335-0.579
  1.36   0.000-0.533                        0.000-0.219  0.264-0.520
  1.40   0.000-0.489                        0.000-0.214  0.216-0.473
  1.44   0.000-0.445                        0.000-0.425
  1.48   0.000-0.403                        0.000-0.377
  1.51   0.000-0.373                        0.000-0.342
```

Read it this way: **the BODY has a 116 mm hole at the armpit at z 1.30 and a
45 mm hole at z 1.36** — the chest box stops at |x| 0.219 and the arm prism does
not begin until 0.264/0.335. The garment closes the z 1.36–1.51 holes but still
carries a **68 mm hole at z 1.30** (torso ends 0.250, sleeve starts 0.318).
Below the armpit some air is anatomically correct; the holes at 1.36 and above
are not, and the shoulder has no deltoid mass bridging box to tube at all.

### 3.2 The scarf, as any garment has to see it

```
scarf total          908 verts, z 1.0579..1.6038
band (the ring)      inner radius 0.10600 EXACTLY   outer 0.1305..0.1486
                     floor z 1.5167
                     top per azimuth: LOWEST 1.5715 (th 190)
                                      HIGHEST 1.6038 (th 80)
knot / bridges       202 verts with r > 0.150 in z 1.50..1.62,
                     spanning r 0.1509..0.2648, z 1.5003..1.5775
tails (free ends)    257 verts below z 1.45; z 1.0579..1.4487; th 228..289;
                     y -0.2374..-0.1894 ; r out to 0.2773
```

`th = atan2(y, x)` in the BIND frame: **90 = front, 180 = character's LEFT,
270 = back.**

**THE COLLAR'S TWO HARD CEILINGS.** "The hoodie fits under it" has to hold at
the azimuth where the ring is *shortest*, not on average: a hoodie collar must
be inside **r 0.1060** and below **z 1.5715**.

**THE TORSO'S DEPTH CEILING.** The tails never come inside **|y| 0.1894**. A
torso half-depth above that EATS THEM — the exact "going through" defect Ruling
A forbids. T1 used `HY_MAX 0.172` (17.4 mm margin). The torso's WIDTH has no
such cap, which is why T1's hoodie is wide and shallow. **A re-authored body
must respect this too.**

**MEASURED SCARF RADIAL ENVELOPE, per 10° bin (index 0 = th 0), in two z bands.**
This is what a hood has to clear. The spec's stated 0.215 was a true statement
about the tail CENTRELINE and the wrong answer to this question — the tails are
132 mm-wide FLAT STRIPS and a strip's *width* sweeps radially:

```
ENV_LO (z 1.24..1.48) = 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                        0.2773, 0.2771, 0.2437, 0.2286, 0.2282, 0.2286,
                        0.2280, 0,0,0,0,0,0,0
ENV_HI (z 1.48..1.70) = 0.1381,0.1358,0.1340,0.1333,0.1325,0.1339,0.1320,
                        0.1360,0.1359,0.1333,0.1315,0.1322,0.1305,0.1340,
                        0.1360,0.1368,0.1367,0.1348, 0.0000, 0.1334,0.1351,
                        0.1486,0.2669,0.2663,0.2470,0.2406,0.2250,0.2168,
                        0.2169,0.1874,0.1399,0.1402,0.1374,0.1346,0.1344,
                        0.0000
```
(Bins 180 and 350 hold no scarf vertex; carry a neighbour's value. Always take
a ROLLING max — a coarse ceiling is only honest as a lower envelope.)

### 3.3 Carried forward from the T0 session, still true

- **THE CHIN** = the Sudburian's own head box bottom, **bind z 1.6087**. NOT a
  helmet chin bar: the v13 helmet's front centre is OPEN below z 1.8927 (the
  face port). Reading "chin" as the helmet builds the ring 250 mm too tall.
- The per-azimuth ceiling over the ring: `measure_neck_ceiling.py` (committed,
  re-runnable). Helmet rim as low as **1.5822** at the sides (x ±0.137).
- The mitt cuff meets the forearm at **t/L 0.91**; a sleeve ending at 0.93 hides
  the join.
- Bind-frame bone anchors: the full dump is in `costume_geom.py`'s banner.
- Helmet family = `helmet_sudburian` + `helmet_dent_1..4`. Measure against ALL
  of them, never one prim.

---

## 4. WHAT T1 BUILT, AND EXACTLY WHAT IT GOT RIGHT AND WRONG

T1 = `costume_geom.py` as it stands on disk now. **5724 verts / 11376 tris /
20 pieces.** Per-shell census all green (20/20 closed, balanced, volume > 0) and
its own generator-side ruling gate green. Then the live containment
measurement (`verify_costume_live.py`) said:

| clause | result |
|---|---|
| ring wall up to the chin | **PASS** — worst margin +4.9 mm at th 75, band top 1.6038 vs chin 1.6087 |
| hood inside the scarf at the FRONT | **PASS** — +4.5 mm at every front azimuth |
| hood outside the scarf at the BACK | **FAIL** — gap −98.4 mm at th 225 |
| free ends rest atop, not through | **FAIL — 331 of 908 scarf verts inside a garment shell** (T0 had 0) |
| nothing inside a BODY shell | PASS — 0 of 908 |
| still bare | head 8 (helmet), neck 12 (ring), lowerarm 18+18 (mitt zone), **upperarm 4+5** |

**THE HOOD DEFECT, AND ITS FIX — worked out, not yet built.** T1 modelled the
down hood as a swept **solid** rounded-rectangle section 86 mm thick
(r_in 0.172 → r_out 0.258). A hood is not solid. The tails, which should lie in
the *cavity* between the fabric and the back, ended up embedded in the fabric —
which is why 331 scarf verts read as "inside a garment shell". The fix is to
build the hood as a **thin shell with a real inner face, outer face and rim**
(this is also what C2 asks for anyway):

- per azimuth, an OUTER profile `R(z)` over `z ∈ [z_hem, z_top]`, with the inner
  face at `R(z) − T`, `T ≈ 0.013` at the back and `≈ 0.0075` at the front collar;
- drive `R(z) ≥ env(th, z) + T + clear` off ENV_LO / ENV_HI in §3.2, so the
  tails sit in the cavity, never in the fabric;
- **the hem must be a flat edge, not a fold.** A swept ellipse pinches to a
  point at its lowest z, and the tails straddle that radius (they span
  0.196..0.227 at z 1.30 while the pinch sits at 0.213) — the hood's own hem
  would run through a tail. A rounded-RECTANGLE section spans r_in..r_out right
  down to z_bot, so the hem is an edge of cloth.

**WHAT IS WORTH KEEPING OUT OF T1** (do not re-derive):
- the measured tables and ceilings in §3, which live in `costume_geom.py`'s banner;
- `_rrect_ring(hx, hy, rc, n_arc, n_side)` — generalised, corner-first, samples
  0/45/90 exactly, so the only containment loss is the inherent `rc·0.293`;
- `_ring_shell()` — one swept-section primitive, proven outward by census, used
  for both the collar and the hood;
- `rulings()` — the four clauses of Ruling B as a generator-side gate;
- the collar: a real neck opening (inner wall, outer wall, rim) inside r 0.1060
  and under z 1.5715. **Ruling B clause 2 is satisfied by it and it is correct.**
- `hood_profile()`'s build → smooth → **RE-APPLY THE HARD CONSTRAINTS** order.
  A clearance that gets blended is not a clearance.

---

## 5. THE VISUAL RECORD — what the viewport actually shows

Looked at from 3/4 front and from dead behind, seated on the machine:

1. **The torso is a slab.** Flat front, flat back, hard vertical edges, a hard
   horizontal hem. It reads as a rectangular box with a coat of paint. This is
   §2's corollary made visible.
2. **The shoulder is a tube jammed into a slab.** There is a visible step and a
   dark notch where the sleeve meets the torso. No deltoid bridges them.
3. **The elbow is a crease between two cylinders**, and the knee is the same.
   Two capped tubes meeting on a joint plane cannot read as a limb.
4. **The hood reads as a lumpy box with a crease**, not as a hood — the T1
   smoothing left a pinched ridge across its top.
5. **The scarf tails are not visible at all in the seated pose** — consistent
   with the standing open finding (§8 below).

---

## 6. THE PLAN — C1 FIRST, and C1 is the body

### C1 — RE-AUTHOR THE BODY. **This is the rung. Everything else waits.**
Replace the boxes and constant-radius prisms in `sudburian_proxy.py`'s
`volume_specs()` with a shaped figure:
- **torso** as one lofted shell with an anatomical section — rounded chest,
  flatter back, real waist narrowing, shoulders wider than the waist — instead
  of three stacked rectangles. It still has to split its vertex groups across
  `pelvis` / `spine_02` / `spine_03` by z, exactly as the T1 garment torso does.
- **deltoid mass at the shoulder that closes the armpit hole in §3.1.**
- **shaped limbs:** elbow and knee volume, forearm and calf taper, an ankle
  break, a wrist. These are only possible once the body stops being a cylinder.
- **KEEP EXACTLY:** all 44 bones, every head/tail, the bind frame, the seat fit,
  the grip weld, the scarf anchors, the helmet fit. Bones must not move — only
  the skin around them.
- **RESPECT `|y| ≤ 0.1894`** at the back over z 1.06..1.45, or the body eats the
  scarf tails (§3.2).
- Chad judges the grey silhouette BEFORE a seam is authored (ladder §9: "agents
  build the blockout, Chad judges the form").

⚠ **C1 touches a signed, gated file.** `sudburian_proxy.py` is governed by
`docs/RIDER_AUTHORITY.md` and its output is what `measure_seat_profile.py`,
the grip weld and the scarf anchors were all measured against. Re-measure the
seat fit after the re-author, and **red-team the change before it lands**
(standing rule `red-team-major-work`). Also note `apply_costume_live.py`'s
docstring asserts the live body "carries hand tuning that no generator
reproduces" — **verify whether that is still true before regenerating**, because
if it is, the body has to be reshaped in place rather than rebuilt.

### C2 — THE GARMENTS, re-cut onto the new body
Panels, seams, hem drawcord, cuff ribbing, side zip as geometry, and the **thin
two-faced hood of §4**. Keep the collar as built. `verify_costume_live.py` still
measures Rulings A and B by containment and does not care what geometry
satisfies them.

### C0 — THE ENGINE PREREQUISITE (blocking for textures ONLY, not for form)
**MEASURED: this engine cannot show a texture today.** The shipping GLB has
`images: 0, textures: 0`, no primitive carries `TEXCOORD_0`, and
`render/sled_model.cpp` never samples one. Needs UV ingestion, embedded-image
load, and shader sampling — check what the fragment shader actually reads before
promising a normal map. **The T0 handoff put this first; that ordering is wrong
now.** Chad judges form in grey, so C1 and C2 come first and C0 lands before C4.

### C3 — SKINNING (weighted, multi-bone at elbow/knee/waist/shoulder; everything
today is rigid one-bone-per-vertex, which is why a stripe cannot cross an elbow).
### C4 — UVs + 2048² textures.  ### C5 — LOD + draw cost.

**BUDGET WARNING for C5:** T1's costume alone is 11376 tris on top of the
existing 11470 — about 23k against ladder §R2's ~14k LOD0 target. The re-author
should be built with that budget in mind rather than trimmed to it afterwards.

---

## 7. STATE ON DISK AND IN THE SESSION

**Nothing is committed. The tree is dirty.** Branch `main` in `D:\flight_sim2\seads-recon`.

| artefact | state |
|---|---|
| `assets/character/sudburian_src/costume_geom.py` | **REWRITTEN this session = T1.** Carries all the §3 measured tables in its banner, the generalised `_rrect_ring`, `_ring_shell`, `hood_profile()` and `rulings()`. Census green, ruling gate green, hood defect per §4. `python costume_geom.py` is the census + gate, exit 1 on any defect |
| `assets/character/sudburian_src/costume_geom_T0_rejected.py` | NEW. The T0 generator, preserved verbatim. It is NOT referenced by anything; it is there so the first attempt is not lost |
| `assets/character/sudburian_src/apply_costume_live.py` | unchanged from T0. Appends the generator's output to the live proxy, idempotent by material slot, with a body-untouched proof. Never saves |
| `assets/character/sudburian_src/verify_costume_live.py` | unchanged. **The valuable one** — measures the rulings on the APPLIED mesh by ray-parity containment. ⚠ its test 2/3 verdict text assumes the T0 hood; the measurement itself is still right |
| `assets/character/sudburian_src/measure_neck_ceiling.py` | unchanged from T0 |
| `assets/character/sudburian_src/scarf_geom.py` | EDITED at T0: flat `LOOP_Z_HI 1.570` → measured `loop_z_hi(th)` profile. **KEEP — it is Chad's ruling, measured, and independent of the costume** |
| `assets/character/sudburian_src/export_live_v13.py` | EDITED at T0: `REPO` is caller-overridable (it was hardcoded to `D:\seads_sandboxes\winter-gi` and would have exported into the wrong tree) |
| `assets/sled/indy650.glb` | **STILL OVERWRITTEN with the T0 splice** from the previous session. `git checkout assets/sled/indy650.glb` reverts it |
| **the live Blender session** (`indy650.blend`) | **HOLDS THE T1 COSTUME, NOT SAVED.** Proxy is 9714 verts / 6 material slots. Rig 44 bones, correct rider. Viewport view was moved to BACK and **restored** to its original transform. Nothing is lost if it closes: the three scripts rebuild it deterministically |
| the gate | **NOT RUN, either session.** `test_rider_winding` has never seen these shells |

To remove T1 from the session: a plain reload of `indy650.blend` (it was never
saved), or re-run `apply_costume_live.py` with an emptied piece list.

---

## 8. THE OPEN FINDING THAT OUTLIVES THE RUNG

**In the SEATED POSE, 497 of 908 scarf verts are inside the BODY.** The authored
rest tails hang straight down while the posed torso leans 33° forward, so in the
`.blend` the tails are *buried*, not merely occluded. Pre-existing and unrelated
to either costume attempt (both left the body and the tails untouched), and the
GAME never draws that pose — `render/trail_chain` drives those bones every frame
off the measured back keep-out. But the §8h answer on record ("nothing is buried,
the tails are occluded by the torso") is **wrong for the seated pose**, and now
that the `.blend` is the source of truth that discrepancy wants a ruling from
Chad. It is also why §5.5 above sees no tails in the back view.

---

## 9. TRAPS — every one of these has now been paid for at least twice

1. **A superellipse does not contain the box it rounds.** Power-4 at (box+8 mm)
   leaves all eight corners of every torso box outside the garment.
2. **Sampling that misses the feature it exists to reproduce.** A
   perimeter-parameterised rounded rect put every sample on the straight runs
   and cut the corners off with one chord.
3. **A measurement with a filter in it is a measurement OF the filter.** The
   ceiling script's first cut had a z floor at 1.60 and reported 1.6003 where
   the real helmet rim is 1.5822.
4. **Interpolating a ceiling upward between bins invents headroom.** A ceiling
   read off a coarse grid is only trustworthy as a *lower envelope*.
5. **A clearance that gets blended is not a clearance.** Build → smooth →
   RE-APPLY the hard constraint, in that order, always.
6. **The per-shell census earns its keep every single time.** Per shell, signed
   volume > 0, never one grand total (SCARF_SPEC §8k).
7. **A combined BVH over overlapping shells makes parity lie** — inside two
   shells is an even crossing count, i.e. "outside". Test containment PER SHELL.
8. **NEW, T1: a green gate can be a gate that asks the wrong question.**
   `rulings()` checked the hood's inner wall against the tails' *maximum* radius
   and passed, while the hood was slicing through the knot at its *minimum*
   radius. The generator gate went green and the containment measurement on the
   applied mesh found 331 buried vertices. The same shape as every other defect
   on this ladder: **a true report of a question nobody asked.** Trust
   `verify_costume_live.py`, not the generator's own opinion of itself.
9. **NEW, T1: do not reason your way out of a stated ceiling.** The T0 handoff
   said the body was the ceiling. T1 argued it away on the theory that a baggy
   garment is its own silhouette, built on that, and Chad overturned it in one
   line. The containment argument in §2 was available before a triangle was
   written and would have settled it.
