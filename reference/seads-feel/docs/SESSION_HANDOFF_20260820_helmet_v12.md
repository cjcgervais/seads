# HANDOFF — HELMET v12 BUILT + APPLIED (2026-08-20)

**STATE: the v12 helmet is IN THE LIVE BLENDER SESSION on the Sudburian,
AWAITING CHAD'S EYE. Nothing saved, nothing spliced, nothing pushed.**
Predecessor: docs/SESSION_HANDOFF_20260819_helmet_restart.md (v11 failure
record + Chad's verbatim quotes — still the requirements). Read that first,
then this. The NO-GUESSING law (CLAUDE.md) governs everything.

## 1. WHAT EXISTS RIGHT NOW

**Live session** (indy650.blend OPEN in the GUI — never headless, single
writer, the running session is the truth over the disk file):
- `helmet_sudburian` — 12110 verts / 24188 tris, NEW object, armature
  modifier → `sudburian_rig`, vertex group `head` weight 1.0 on all verts,
  8 materials with node trees (byte colours; viewport `diffuse_color` =
  sRGB→linear of the byte code = the signed viewport law), shade smooth.
- `head_balaclava` — black box (±92, ±117, h 0–247 in helmet-local mm),
  same rigging. It ENCLOSES the authored grey head blockout (±90, ±115,
  245) so no grey ever shows. Black visible through the opening and below
  the rim = the ruled no-faces stand-in + neck (v5 design, intended).
- `sudburian_proxy` UNTOUCHED — 3082 verts / 5734 polys, asserted before
  and after every apply. The legacy `rider_helmet` (19-bone rider_rig)
  was never touched.
- The .blend was NOT saved by me at any point. (Note: the session was
  found NOT dirty at start — the disk file may equal the stripped state,
  unlike the 0819 handoff's warning that disk held v9. Do not save without
  Chad's word either way.)

**Artifact of record**: `Game_loop_idea/vehicle_program/blender/helmet_v12/`
(in D:\flight_sim2):
- `helmet_v12_mesh.json` — THE mesh (verts in helmet-local mm, tris,
  per-tri material, piece ranges, palette, roughness). Deterministic
  output of the pipeline; the session content is byte-derived from it.
- `ARCHITECTURE_V12.md` — the plan, REV B (all 11 red-team findings), and
  FINAL STATE section with every recorded deviation/ruling.
- Pipeline (pure stdlib, NO Blender): `helmet_spec_v11.py` (in ../helmet_v11,
  the measured authority — NEVER re-measure), `surface_v12.py` (ring laws,
  C1 width, apex √-closure, polar crown smoothing, vF/vR overrides),
  `gamma_v12.py` (the single boundary curve Γ + bead axis laws),
  `gen_helmet_v12.py` (columns/knots/identity-anchored ladders/mirror),
  `pieces_v12.py` (bead torus, 5 snaps, 2 vents, balaclava),
  `palette_v12.py` (parses kSlagOrange from render/team_color.h:68 —
  asserts #FF8C1F, never retyped), `validate_v12.py` (census + ported
  photo gates), `assemble_v12.py` (build + ALL gates + SIZE_SCALE +
  writes the json), `render_v12.py` + `raster.py` (offline renders),
  `apply_helmet_v12.py` (the ONE transactional session apply: preflight
  asserts, full rollback on any failure, NO SAVE).
- Renders: `r_photomatch.png` etc (offline), plus true in-session OpenGL
  renders in the session scratchpad (session_rear/side/final.png).

**To rebuild from scratch**: `python assemble_v12.py` then
`python render_v12.py` (offline, safe). **To re-apply/replace in the
session**: delete `helmet_sudburian` + `head_balaclava` (+ purge 0-user
helmet/balaclava materials), then exec `apply_helmet_v12.py` via MCP.
Both steps are in this handoff because they are the ONLY sanctioned way
to touch the session helmet — never edit the mesh in place.

## 2. PROOF STATE (what "verified" means here)

- Every piece a CLOSED MANIFOLD: census 0 open / 0 over / 0 folded edges,
  Euler exact (shell+snaps+vents+balaclava genus 0, bead torus genus 1),
  positive signed volume, 0 degenerate tris. Budget 24200 ≤ 26000.
- The boundary Γ is ONE simple closed curve: exactly two interior φ
  extrema (jaw corner ~51.34°, lobe tip ~35.53°), gated by a 0.05°
  interval-structure scan of the final field (red-team R1, mandatory).
- EXACT mirror symmetry (vertex-multiset gate); brow snap s1 AT x=0
  (Chad's "two sides are identical" ruling).
- G1 photo-silhouette: 109/120 rows strict (±3px), residuals ≤2.2px;
  the tag rows (y115–188, paper tag in the photo) sit inside the spec's
  own [raw+10, raw+25] bound. v11's 12–35px failures all closed.
- H3 zone purity 0 (materials are identity-exact per construction);
  G10 crown in-band; bead tangency at the rim 0.005mm; fit-clearance
  min 2.7mm (see §3 size ruling); kSlagOrange asserted from the header.
- INDEPENDENT fresh-context visual judge: SHIP-TO-SESSION, A–F all PASS
  (shape / crown / colour zones / opening / proportions / defects).
- Crown confirmed glass-smooth by a TRUE in-session OpenGL render.

## 3. CHAD'S RULINGS FOLDED IN THIS SESSION (verbatim-derived)

1. "the reference is properly measured" — v11 spec = sole authority. ✓
2. "no chinstrap" — none. ✓
3. "two sides are identical" — exact mirror; s1 centred; vents mirrored. ✓
4. "use the slag orange queryable in the graphified codebase" — band +
   both band-colour gaps = kSlagOrange parsed live from team_color.h. ✓
5. "give the size an increase to account for the inner padding around the
   large sudburian head" — MEASURED CORRECT: at photo scale the head box
   poked THROUGH the shell 13mm at the rear-lower corners. **SIZE_SCALE
   1.15** (applied at export about the local origin = box-bottom/neck
   point; balaclava NOT scaled). Helmet now ~381 tall × ~353 wide mm,
   min box-to-inner-shell clearance 2.7mm. Shape gates still run at
   measured scale — the SHAPE is untouched, only size.
   Earlier quote-11 rulings (white thin→thick pinstripes, black bottom,
   shiny silver cap, no visor, 3/8" trim) all built as ruled.

## 4. OPEN FOR CHAD (one-word calls; DO NOT decide these yourself)

- **Q-TRIM**: with SIZE_SCALE the trim tube scales to ~10.9mm (ruled
  3/8"=9.5). Holding 9.5 absolute under the size-up = partial rebuild
  (regenerate bead at scaled boundary with absolute r). One word.
- **Q-CAP-SHAPE**: cap boundary built as the MEASURED swoop (reads as the
  photo's round cap). His words "make the top a circle" could also mean a
  literal planar loop (deviates ±20mm from the photo table). Built
  measured; one word flips it.
- **Q-BROW-FLAT**: the gentle front-crown transition is MEASURED (the
  spec's own tag bounds prove no smooth convex curve fits there; the
  photo's paper tag hides the region). Kept. If his eye rejects it, the
  options are documented in ARCHITECTURE_V12.md — do not silently smooth
  it away (that violates the measurement).
- **Q-VISOR**: still OFF (his quote 11 removed it to judge the opening).
  Re-adding is his call; the balaclava carries no-faces meanwhile.
- **Q-SIZE-EYE**: 1.15 was solved from box clearance; his eye may want
  more/less. The scale is ONE constant (`SIZE_SCALE` in assemble_v12.py),
  rebuild+replace takes ~3 minutes.

## 5. NEXT STEPS (only after his eye passes the session helmet)

1. Export/splice: `export_live.py` → `splice_sudburian.py` (the 0818c
   lane; 132 machine nodes must stay byte-identical). The GLB in winter-gi
   still carries the REJECTED v9 helmet — replace it at splice time.
2. H8 colour test leg in test_rider_pose.cpp: GLB helmet_orange
   baseColorFactor == kSlagOrange parsed from the header (1e-6); assert
   no helmet_blue material exists.
3. Smoke shots (H6 helmet-vs-cowl contact number — report, Chad rules;
   H7 gloss highlight). NOTE: at 1.15 scale the crown sits higher —
   H6 matters more now.
4. ~~recon merges 93e7cd820/95d0b2c38 still carry the v5/v9 helmets,
   UNPUSHED — reset or re-merge after the new splice. DO NOT PUSH.~~
   **RETIRED 2026-08-20 late:** both are now ancestors of `origin/main`
   (the audio lane pushed its merge), so there is nothing to reset. What
   governs is FILE CONTENT: the helmet-lane reconciliation (docs/SCARF_SPEC.md
   §8i) set the GLB to **v13 + the scarf** in main AND in seads-recon.

## 6. TRAPS PAID FOR THIS SESSION (do not re-buy)

- **Rasterizer z-buffer**: keeping the FARTHEST fragment showed the far
  side of the mirror-symmetric shell — looked plausible for a whole
  round. The independent judge caught it as fake holes. Fixed in
  raster.py (`z < zbuf`, init +inf).
- **MCP viewport screenshots go STALE** when Blender's window isn't
  redrawing (minimized/unfocused): two "verifications" returned the same
  frame. TRUTH PIXELS = `bpy.ops.render.opengl(write_still=True,
  view_context=True)` to a file, then Read it.
- **Crown ripple**: interpolating photo-noise rows ±1-2px makes visible
  shading rings on a specular dome. h-space filters fail both ways
  (cap-clamps ride the noise; uncapped SG erodes the apex's vertical
  tangent). THE fix: smooth r(θ) in POLAR about the crown centre — the
  apex becomes an ordinary point.
- **Vertex-cache identity**: fill knots need per-gap identities
  (("F",z,gap,j)) — a duplicate identity silently aliased verts 100mm
  apart (the corner-knot split duplicated ("F",4,j)).
- **Ladders must be identity-anchored**: a raw geometric two-pointer
  across a steep colour boundary emits cross-zone sliver spikes; anchor
  at matched identities (ignore the interval tag prefix — the "T"/"L"
  mismatch bug) and merge geometrically only inside one zone segment.
- **intervals/extrema truth**: derive φ_T/φ_J by bisecting the interval-
  count FIELD, never from polyline extrema (flat-topped extremum ≠ the
  transition; it built a bifurcation stitch between the wrong columns).
- **The measured rows are the trim-outer** on every silhouette-visible
  boundary segment (opening side, chin curl, rear curl): shell edge inset
  ~(TRIM_W−0.5)·n̂₂D, bead axis = M + r·n̂₂D ring-mapped (2D law — a 3D
  offset breaks the ring alignment and pokes the projection).
- **The brow blend zone** (h>235) closes C1-tangent onto the dome — no
  crease exists there to cover; excluded from the bead-cover check. The
  aperture's cut face is the trim-black ANNULUS and legitimately reads
  as trim where the tube can't reach (near-flat front, x-drift).
- Apply is TRANSACTIONAL and idempotent-by-refusal: it asserts no helmet
  present; replacement = delete objects+orphan materials first, then
  apply. Post-asserts pin the proxy at 3082/5734 every time.

Launch line for the next agent: **read this file, then
ARCHITECTURE_V12.md end-to-end, then wait for Chad's verdict on the
session helmet before touching anything.**
