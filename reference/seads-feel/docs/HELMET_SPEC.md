# HELMET SPEC — the vintage Polaris jet helmet, in slag orange / complement blue

**Chad, 2026-08-19, verbatim:** *"[the photo] is what I want to build now,
graphify query the theme exact color and make it orange / blue. Please get
this exact shape, get it polished to a shine. Exact to this image, take
consults and red teams in order to get it right please."* — *"slag orange
you will find here and put some blue accents exact opposite on color wheel
and verify it externally that it is right."* — *"A black visor please, it's
not in the photo I believe."* — *"no lettering."*

Reference: `D:\flight_sim2\Game_loop_idea\Reference_pics\polaris_helmut.png`
(a 1970s Polaris open-face "jet" helmet: silver dome, thin black pinstripe,
wide red band, thin black pinstripe, black lower band with POLARIS lettering,
black rubber edge trim all round the opening and rim, three chrome snap studs
on the brow, chin strap). This spec maps: silver → **slag orange**, red →
**complement blue**, black stays black, lettering DELETED, a **black visor**
ADDED (a flip shield across the opening, not in the photo).

Obeys `docs/RIDER_AUTHORITY.md` (Sudburian only; live session only; never
headless) and `docs/SESSION_HANDOFF_20260818e.md` §1 (the proxy mesh is
HAND-TUNED — the helmet is APPENDED to the live mesh, never regenerated with
it; `apply_live.py` stays refused).

## 1. COLOURS — derived, not typed, and verified independently

| part | colour | source |
|---|---|---|
| dome (silver in the photo) | **slag orange (1.00, 0.55, 0.12) #FF8C1F** | `render/team_color.h: kSlagOrange` — parsed from the header, never retyped (the R2c-7 rule) |
| wide band (red in the photo) | **complement blue (0.12, 0.57, 1.00) #1F91FF** | `kComplementBlue` = the exact 180° HSV complement at equal S/V; `team_color.h` test-enforces it; independently re-derived 2026-08-19 with Python `colorsys`: orange hue 29.3182° → complement 209.3182° → (0.12, 0.57, 1.00) ✓ |
| two pinstripes, lower band | black (0.06) | photo |
| edge trim | `indy_rubber` (black matte) | existing material |
| snap studs ×3 | `indy_steel` | existing |
| visor | black gloss (0.02), the shiniest thing on him | new `helmet_visor` |

All colours are raylib BYTE space (this engine has no linear pipeline —
memory: the sRGB→linear step once shipped the mitts brown). Materials need a
node tree in Blender or the exporter writes 0.8 grey (handoff-c lesson 3).

## 2. SHAPE — the photo, measured  *(v2 after the Fable red-team, 2026-08-19 — its pixel measurements replace v1's guesses)*

Red-team measurements (photo 1152×973, ~0.30 mm/px): the dome is slightly
flatter than a sphere; widest ring ~55–60 % down; the band's LOWER edge is
level, its UPPER edge is NOT — it sweeps UP toward the face opening (~55 mm
higher at the front than the nape); the upper pinstripe is a DOUBLE stripe
(black 2 / gap 3 / black 2 mm); the lower stripe is in the DOME colour
(~3.5 mm); the lower band ~36 mm; the rim tucks ~18 mm and drops ~10 mm at the
nape; trim ~6 mm wide; the three snaps lie ALONG THE OPENING EDGE (brow, then
~55 mm lower, then at jaw level). v1's shell did not clear the head box's
corners (18 mm outside the inner surface at the widest ring) — fixed below.

The reference is a 3/4 side view; a jet helmet's canonical outer dimensions
at this era are ~290 mm deep × 250 mm wide × ~260 mm crown-to-rim. The
Sudburian's head box is 0.180 wide × 0.230 deep × 0.245 tall, centred at
`(0, 0, C7_Z + 0.1225)` in rest space (rest: +Y forward, +Z up, ±X sides).
Build in HEAD-LOCAL coordinates (origin = head box centre):

| element | spec |
|---|---|
| head fit | **v4: head blockout SHRUNK to 0.140 × 0.160 × 0.160 (was 0.180 × 0.230 × 0.245), same centre (0,0,C7_Z+0.1225) — invisible behind the opaque visor; the one-way scale lives in `apply_helmet_live.py` §0.** Helmet = a TRUE ellipsoid: plan half-axes (0.125, 0.145) at the widest ring z +0.020, vertical semi-axis 0.140 to the crown +0.160 with only a mild top flattening (chord ≤ 1.25× a sphere 10 mm below the crown), rim −0.085 (nape −0.095). Old text: +Y is FACE-FORWARD in rest space (verified `sudburian_proxy.py`), ±X the sides, Z up. Inner surface: plan ellipse half-axes ≥ (0.108, 0.133) held near-vertical from the rim z −0.085 up to z +0.1225 (box top, 10 mm clearance at the CORNERS), dome only above that; crown (outer) at z +0.180 (helmet height 0.265 ≈ the real 260 mm). Shell 8 mm → outer half-width 0.116, half-depth 0.141 at the flank, widest ring at z ≈ +0.020, rim tuck to (0.104, 0.125) at the rim, the rim 10 mm lower at the nape than at the jaw corner. Profile = C¹ (cosine/smoothstep stations), the dome slightly flattened on top. ONE closed solid (outer + inner bridged at every free edge). |
| face opening | no opening above the brow line z_brow = +0.055; below it the half-angle (azimuth from +Y) ramps with smoothstep from 0 at z_brow to 68° at z_brow − 0.045, then holds 68° to the rim; corners radiused by the smoothstep. |
| edge trim | torus tube r 0.0035 (6–7 mm visible), black `indy_rubber`, along the WHOLE free edge (opening + rim), one loop, half-proud. |
| bands | faces by centroid: DOME orange above the band top; the band top is SWEPT: z_top(az) = +0.010 at the nape (az 180°) rising smoothly (smoothstep in az) to +0.060 at the opening edge (az 68°); the double pinstripe straddles that swept line: black 2 mm / gap 3 mm (dome colour) / black 2 mm, following z_top(az); BAND blue from below the pinstripe down to z −0.045 (LEVEL); lower stripe 3.5 mm in DOME colour (orange) at z ∈ [−0.0485, −0.045]; LOWER BAND black from −0.0485 to the rim. The lathe rows follow the swept curve in the band-top region (rows offset along z(az)) and are level elsewhere, so every material boundary is a clean line. |
| snaps | three chrome hemispheres r 0.005 ON THE OPENING EDGE line (just aft of the trim): brow centre (az 0°, z_brow + 0.010) and two side snaps at az ≈ ±80°, z ≈ −0.050. |
| visor | black-gloss spherical-section shell 3 mm thick, 6 mm proud of the shell, azimuth ±75°, from z_brow + 0.006 (under the brow snap) DOWN TO z −0.095 (below the rim, covering the blockout chin — the visor now carries the "no faces" guarantee the chin bar used to: OPAQUE, not smoked). Its own closed solid. |
| chin strap | OMITTED (Q1). |

Tessellation: 48 azimuth segments, ~40 profile rows (the five material
rows included), trim 12×(boundary samples); target ≤ 5 k tris for the whole
helmet (dome 3.8 k, trim 0.8 k, visor 0.4 k).

## 3. WHERE IT LIVES

- Generator `assets/character/sudburian_src/helmet_geom.py` — stdlib, pure,
  `build_helmet(params) -> (verts, tris, pieces, material_of_tri)` in
  head-local coordinates, self-test = census 0/0/0/0 per piece, directed
  winding 0, folded edges 0, signed volume > 0 per piece, head-box clearance.
- `apply_helmet_live.py` — exec'd in the OPEN session: APPENDS the helmet to
  `sudburian_proxy`'s mesh (new verts/faces, vertex group `head` weight 1.0,
  new material slots `helmet_orange / helmet_blue / helmet_black /
  helmet_visor` with node trees + existing `indy_rubber` / `indy_steel`),
  placed at the head box centre in REST space. It must NOT touch any existing
  vertex (assert the first N verts are byte-identical before/after) and must
  refuse if a helmet is already present (re-run = remove the previous helmet
  faces by material first — idempotent). NO SAVE (lead saves).
- Export/splice as before (`export_live.py` → `splice_sudburian.py`).
- `render/sled_model.cpp`: ONLY for primitives whose material name starts with `helmet_` (red-team R4: 24 of the 34 sled materials carry roughness < 0.5 — gating on roughness would re-light the whole hood), read `pbr_metallic_roughness.roughness_factor` into `sp.gloss = clamp((0.5 − rough)/0.5, 0, 1)`; every other primitive gloss 0 and add to
  `kFS` a Blinn-Phong term `spec = gloss · 0.55 · pow(max(dot(N,H),0), mix(16,96,gloss))`
  with `H = normalize(-uSunDir + V)`, added to `lit`. Materials without a
  roughness (cgltf default 1.0) and every existing material with rough ≥ 0.5
  are bit-identical (gloss 0 → no term). `indy_black_gloss` (if its roughness
  is < 0.5) gains a highlight — that IS its name. Golden/test impact: none
  (no ctest runs the shader); smoke shot required.

## 4. GATES

| gate | test |
|---|---|
| H1 census | per piece 0/0/0/0, winding 0, folds 0, volume > 0 (shell, trim, 3 studs, visor) |
| H2 head inside | all head-box CORNERS aft of the brow are ≥ 3 mm inside the shell's inner surface (v3/v4 ruling: the blockout box was SHRUNK to 0.140 × 0.160 × 0.160 — it is invisible behind the opaque visor — so the shell can be the photo's true ovoid; the spec's original 10 mm was the box-size conflict the red-team found; corners, not face centres); NO shell/inner vertex inside the head box; the face (front of the box at z < z_brow) is OUTSIDE the shell (visible through the opening) |
| H3 boundaries | every material boundary is a clean row: level rings for the level edges, rows that follow z_top(az) for the swept band top; no face straddles a boundary (assert per face) |
| H4 visor | ≥ 4 mm clear of the shell everywhere, ≥ 8 mm clear of the head box front |
| H5 append-only | proxy verts [0, N) byte-identical before/after; only the `head` group grew; all other groups unchanged; `[rider_pose],[rider_rig]` 50/50 after splice (the head bone's rest is unchanged — the helmet rides it) |
| H6 contact | in the game, `SEADS_SLED_RIG_SMOKE` fwd 0.25 / 0.5 shots: report the helmet's lowest point vs the cowl top (the R1a helmet-contact class) — number only, Chad rules |
| H7 shine | smoke shot shows a highlight on the dome and the visor; a shot with gloss forced 0 is pixel-identical on the machine (the bit-identical claim, measured) |
| H9 cowl | at fwd 0 no helmet vertex inside any machine node; tri budget ≤ 5 k asserted in the generator |
| H8 colours | the GLB's `helmet_orange` baseColorFactor == kSlagOrange parsed from the header, `helmet_blue` == kComplementBlue, to 1e-6 (a test leg in `test_rider_pose.cpp` alongside the existing kComplementBlue derivation test; the header path via the test's source-dir define) |

## 5. OPEN FOR CHAD (one line each)
- Q1 chin strap: omitted (clutter) — add if wanted.
- Q2 CLOSED by the red-team: the visor is OPAQUE black — it now carries the "no faces" guarantee (§4's full-face chin bar is superseded by this jet helmet).
- Q3 colour mapping: default (a) dome orange / wide band blue (literal silver→orange, red→blue, "make it orange / blue"); alternative (b) orange dome + black band with blue only on the stripes ("some blue accents"). Built as (a); one word flips it.

## 6. SUPERSESSION
`SUDBURIAN_LADDER.md` §4 / §4.1(c) (CKX Mission free-face, black + red, "chin bar delivers the no-face guarantee") is SUPERSEDED by this spec on Chad's photo: a vintage Polaris jet helmet, slag orange / complement blue, opaque black visor, no lettering. The colour-parsing rule (parse `kSlagOrange`/`kComplementBlue` from `render/team_color.h`, never retype) is re-implemented here — the old parser in `patch_costume.py` was deleted with the R2c revert.

## 7. v5 — CHAD'S EYE, 2026-08-19 (verbatim: *"make sure the size of the helmet is correct to the proportion of the body size, the helmet visor not so flat and also increase the size of the head so that it sits on top of the neck, the helmet is floating 3""* — *"I don't think that the orange is the right color make sure, it looks the same as it does in Blender as in the game, check the color with independent check, it has to be the exact right code."*)

Measured in the live session before touching anything: `neck_01` tops out at
z 1.609 (= the head bone's head); the v4 blockout (0.160 tall, centred on the
bone MIDPOINT 1.7315) bottomed at 1.651 — **42 mm of air** between neck and
head — and the rim (−0.085) hung 24 mm above the neck top; the visor was a
constant 6 mm offset off a near-vertical shell wall = a flat plate; 250 × 290
read small against 0.43 m shoulders. A lollipop on a stick.

| change | was (v4) | now (v5) | why |
|---|---|---|---|
| head blockout | 0.140 × 0.160 × 0.160, centred on the bone | **0.160 × 0.190 × 0.195**, BOTTOM pinned to the neck top (`HEAD_BOX_DZ`) | "increase the size of the head so that it sits on top of the neck" |
| head sink | — | head + helmet sink **30 mm** into the neck post (`HEAD_SINK`) | with the box exactly on the neck the rim still hung 12 mm over a 96 mm bare neck (no collar on the suit); the skirt now wraps the top of the neck, ~55 mm of neck shows |
| shell plan | 250 × 290 | **270 × 310** (ax 0.135 / ay 0.155) | XL jet proportions for this body; also clears the bigger box |
| rim / crown | −0.085 / +0.160 | **−0.110 / +0.165** (height 275) | skirt comes down round the neck |
| tuck | 0.115 | 0.060 | solved down to ≥ 3 mm at the aft jaw corners |
| visor | constant 6 mm off the shell | **bubble**: +25 mm at centre, (1−u²)(1−w²) falloff, bottom −0.120 | "not so flat" |
| H2 gate | box at the origin | box at `HEAD_C` | the box is no longer centred on the helmet origin |
| colour, Blender | `diffuse_color` = the code, shown LINEAR → #FFC263 peach on screen | `diffuse_color` = sRGB→linear of the code, so the Solid viewport (Standard view transform) DISPLAYS #FF8C1F; the Principled node keeps (1, 0.55, 0.12) — the exporter reads the node, H8 pins the GLB | "looks the same in Blender as in the game" |
| colour, game | `static_cast<unsigned char>(c·255)` TRUNCATED → #FF8C1E | `std::lround` → **#FF8C1F** | "the exact right code" — the lava shader's GPU rounding of the same kSlagOrange gives #FF8C1F; every sled material moves ≤ 1 LSB toward its true byte |

Independent colour check (this session): stdlib GLB parse → `helmet_orange`
baseColorFactor (1, 0.55, 0.12) = #FF8C1F, `helmet_blue` (0.12, 0.57, 1) =
#1F91FF; `render/team_color.h` kSlagOrange {1.00, 0.55, 0.12}; slag.cpp
injects the same constant as SLAG_HOT. Generator gates H1–H4, H9 PASS;
splice 132 machine nodes byte-identical, rest == posed 0.000001 m.

## 8. v9 — MEASURED FROM THE PHOTO (2026-08-19, after Chad's ruling: *"can this just all be measured properly to get the design exact like I asked? close enough for who?"*)

Method: pixel classification of `polaris_helmut.png` (1152×973). Helmet crown
y=42, bottom y=916 → height 874 px; height mapped to the build's 275 mm →
**0.3146 mm/px**. Scanlines x ∈ {300,330,360,420,520,640,740,800}.

**Profile (Chad: "more round, not half a pill, no flared bottom"):** widest
row y=460 → **47.8 % down** the height (z ≈ +0.033). Above it the dome reads
as a near-sphere. Below the widest ring the REAR edge pulls in continuously —
x0: 213@y460 → 294@y786, ~8 %/side and still curling at the rim; nowhere
straight, nowhere flared. Build: pure ellipsoid above z_wide=+0.033
(dome_p=2.0), and below it scale(z)=sqrt(1−((z_wide−z)/0.264)²) — a sphere-
like curl that is C1 at the widest ring, reaches 0.87 at the rim, and KEEPS
curling through the skirt. tuck params retired.

**Colour zones (top→bottom), measured at every scanline (px→mm):**
| zone | measurement | build |
|---|---|---|
| SILVER cap ("the silver circle on top") | boundary y: 339@x300(back) → 539@x740(front) — the cap boundary DESCENDS toward the face: f 0.340(back)→0.569(front) | z_top(az): **nape +0.0715 → face +0.0085** (the OLD sweep was inverted — that was "the first color goes too low") |
| THICK black stripe | 10–12 px | **3.3 mm** |
| band-colour gap | 10–15 px | **3.8 mm** |
| THIN black stripe | 4–5 px | **1.4 mm** |
| main band (photo red → SLAG ORANGE) | bottom f 0.80(back)→0.84(front) | z_bot: **nape −0.050 → face −0.066** |
| WHITE stripe (thin) | 7 px | **2.2 mm** |
| black gap | 6 px | **1.9 mm** |
| WHITE stripe (thick) | 10–11 px | **3.5 mm** |
| BLACK section | 40–60 px (front) | to the rim |
| edge trim | Chad ruled **3/8" = 9.5 mm** wide, opening AND bottom | tube r 4.75 mm |

**Colours:** silver sampled p25–median (231,232,224) → `helmet_silver`
(0.90, 0.91, 0.88); white stripes `helmet_white` (0.95, 0.95, 0.95); band =
`kSlagOrange` parsed from the header; black 0.06; visor 0.02. **helmet_blue
LEAVES the helmet** on the 2026-08-19 ruling ("silver circle on top, then
proper striping, then orange section, then the white pinstriping, then the
black section"); the H8 test drops its blue leg and pins orange + asserts no
helmet_blue material remains.
