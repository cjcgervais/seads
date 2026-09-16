# SEADS World-Art Technique Report — the "1940s Sudbury stereoscope" little planet

Research packet for the WORLD/art layer of a stylized "little planet" flight game.
Target: 15 km-radius sphere, raylib / OpenGL 3.3 (GLSL 330), retained-mode `DrawMesh` /
`DrawMeshInstanced`, Blender→glTF asset pipeline, offline GIS tool already ingesting
CDEM + OSM for Greater Sudbury / Chelmsford. Aesthetic: flying LOW = flying through
Chelmsford as if through an **old black-and-white stereoscope** — strong depth, 1940s,
silver-gelatin monochrome. Judged by feel/beauty, not fidelity. Single consumer GPU.

Every technique below notes GL 3.3 / raylib feasibility. Infeasible items are FLAGGED.

---

## Section 1 — The stylized B&W "newsreel / stereoscope" look

All passes are full-screen fragment passes over a raylib `RenderTexture` chain
(`BeginTextureMode` / `BeginShaderMode` ping-pong). Nothing here needs compute or
geometry shaders — GL 3.3 / GLSL 330 throughout. The ONE setup wrinkle is depth access
(flagged at the end).

**Grayscale, done right (not a naive average).**
- Use Rec.709 luminance weights: `L = dot(rgb, vec3(0.2126, 0.7152, 0.0722))`. Rec.601
  `(0.2990, 0.5870, 0.1140)` is the SD/newsreel-era alternative and is arguably MORE
  period-correct for a 1940s film look. A flat `(r+g+b)/3` reads muddy — blues too
  bright, greens too dark — because vision is far more green-sensitive.
  (johndcook.com/blog/2009/08/24/algorithms-convert-color-grayscale, mmuratarat.github.io/2020-05-13/rgb_to_grayscale_formulas)
- Subtle-but-real correctness: those weights are defined on LINEAR RGB. Linearize
  (`pow(c,2.2)`), apply weights, re-encode (`pow(g,1/2.2)`). raylib RenderTextures are
  NOT sRGB by default, so do this manually. Difference is "muddy vs clean" midtones for
  ~free. (30fps.net/pages/better-srgb-to-greyscale)

**Tone curve — filmic S-curve for deep blacks + highlight rolloff.**
- ACES filmic (crunchiest, best rolloff; hue-shift caveat is moot in mono):
  ```glsl
  vec3 ACESFilm(vec3 x){ float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0); }
  ```
  (knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve)
- Reinhard `v/(1+v)` is too FLAT for this look. Hable/Uncharted2 is the dialable
  alternative if you want manual toe/shoulder control. (64.github.io/tonemapping)
- Cheapest manual path (scene is likely already LDR): contrast-about-0.5
  `(L-0.5)*contrast+0.5` + lift (add to shadows) + gamma (`pow(L,1/g)`) + gain. This is
  the colorist triad and is enough for "deep blacks, bright rolloff."
  (therealmjp.github.io/posts/a-closer-look-at-tone-mapping)

**Silver-gelatin tonality — subtle split-tone (cool shadow / warm highlight).**
- Duotone = remap luma along a two-color gradient: `mix(shadowColor, highlightColor, L)`.
  Sepia is the brown/cream special case. (planetphotoshop.com/photographic-toning-effects)
- Split-tone (tint shadows and highlights INDEPENDENTLY) is what reads "silver," not
  "sepia." Keep tints within ±0.1 of neutral. The darkroom rule that maps exactly:
  MORE shadow toning, LESS highlight toning → clean bright highlights, subtly toned darks.
  ```glsl
  vec3 shadowTint    = vec3(0.90,0.95,1.05); // cool
  vec3 highlightTint = vec3(1.05,1.00,0.92); // warm silver
  vec3 tint = mix(shadowTint, highlightTint, smoothstep(0.0,1.0,L));
  vec3 outc = L * tint;
  ```
  (lifeafterphotoshop.com/split-toning-explained, vsco.co/features/split-tone)

**Film grain — animated, luminance-dependent (lives in the midtones).**
- mattdesl's `grain(uv, res, frame, q=2.5)` uses 3D noise so motion looks natural; feed
  time into the Z. Modulate DOWN in shadows/highlights: `amt = smoothstep(0.05,0.5,L)`,
  strength `pow(response,2.0)`. Cheap fallback = hash noise (worse motion).
  (github.com/mattdesl/glsl-film-grain, devlog-martinsh.blogspot.com/2013/05/image-imperfections-and-film-grain-post.html)

**Halation, NOT plain bloom, on highlights.**
- Bloom = neutral glow: bright-pass `smoothstep(0.6,0.8,L)` → separable Gaussian
  (H then V; chain small blurs at texel steps 1,2,4 rather than one huge kernel; blur at
  DOWNSAMPLED res) → additive `scene + bloom*intensity`.
  (mini.gmshaders.com/p/gm-shaders-mini-bloom, learnopengl.com/Advanced-Lighting/Bloom)
- Halation = the SAME passes, but tint the blurred glow WARM (`vec3(1.0,0.55,0.30)`,
  dialable magenta↔yellow) before adding. This is the one place a B&W pipe reintroduces
  warm color; it pairs with the warm-highlight split-tone. Most pass-heavy item but well
  inside GL 3.3. (vsco.co/learn/halation-vs-bloom-vs-lens-flare, blog.dehancer.com/articles/halation)

**Vignette + ordered dither (the "printed card" cues).**
- Vignette: `v = smoothstep(outer,inner, length(uv-0.5)*2.0); col *= mix(0.4,1.0,v);`
  Multiply last. A strong vignette is core to the stereoview card look.
- Ordered/Bayer dither for the halftone/printed feel: 8×8 matrix, threshold
  `(dither[x][y]+1)/64`, `x=int(mod(gl_FragCoord.x,8))`. Scale `gl_FragCoord` by
  `1/gridSize` for bigger printed dots. The 8×8 cluster-dot "mimics newspaper halftoning."
  (devlog-martinsh.blogspot.com/2011/03/glsl-8x8-bayer-matrix-dithering.html,
  en.wikipedia.org/wiki/Ordered_dithering)
  - **Caveat:** hard 1-bit Bayer FIGHTS smooth silver tonality. For "newsreel," use a
    SUBTLE multi-level dither or grain; reserve aggressive 1-bit halftone for an explicit
    "printed card" mode (see Obra Dinn, §6).

**What specifically evokes an OLD STEREOSCOPE (the depth cues — the highest-value items).**
The stereograph feel = HYPERREAL depth + micro-contrast + sharp near / hazy far +
"cardboard cutout layers." Three depth-driven levers, in priority order:
1. **Aerial perspective (fog-by-depth) — #1 lever.** Distant = lower-contrast, lighter,
   hazy. Systematically EXAGGERATE it (the landscape-painter trick for depth on a flat
   plane). `fog = smoothstep(near,far,depth); col = mix(col, hazeColor, fog)` where
   hazeColor is a light warm-gray (not blue, in mono). Also multiply the clarity term
   (below) by `(1-fog)`. (en.wikipedia.org/wiki/Aerial_perspective)
2. **Depth-of-field / focus blur — the "cardboard layers."** Stereo content reads FLAT
   when the focus-blur gradient is missing; a depth-driven blur restores dimensionality
   and the layered-plane feel. Simple gather/box blur lerped by `abs(depth-focusDepth)`
   — no compute needed. (arxiv.org/pdf/1703.04574, arxiv.org/pdf/1506.06001)
3. **Micro-contrast / clarity (unsharp mask) on the near field.**
   `sharp = orig + (orig - blurred)*amount` (amount ~0.3–1.0; large blur radius = local
   contrast/"clarity", small = sharpening). GATE by depth so only the near field gets the
   tactile crispness — near-sharp/far-soft IS the core stereograph cue. Watch halos at
   high amount. (martysmods.com/clarity, github.com/garamond13/unsharp_masking.glsl)

**Combined stereograph recipe:** near = high clarity + deep blacks + full grain;
far = aerial haze + focus blur + reduced local contrast; strong vignette; subtle
sepia-silver split. The DEPTH-DRIVEN modulation is what separates "stereograph" from a
generic "B&W filter."

**Recommended single pass order:**
scene → [depth pre-pass] → linearize+grayscale → tone/S-curve → depth-gated
clarity+aerial-haze+DoF → bright-pass→blur→additive **warm halation** → split-tone →
film grain → vignette → optional subtle dither → gamma-encode.

**FEASIBILITY FLAG (the one wrinkle):** all pure color-space passes drop straight into a
standard raylib RenderTexture chain. But the three DEPTH cues (aerial perspective, DoF,
depth-gated clarity) — the strongest for this look — need scene DEPTH in the post chain.
raylib's `LoadRenderTexture` gives a depth RENDERBUFFER, NOT a sampleable texture. You
must either (a) build a custom FBO with a depth-TEXTURE attachment via rlgl
(`rlLoadTextureDepth` + `rlFramebufferAttach` as `RL_ATTACHMENT_DEPTH`), or (b) run a
depth pre-pass writing linear depth to a color target. Everything downstream is normal
GLSL 330. Nothing here is infeasible in GL 3.3.

---

## Section 2 — Large-scale boreal forest in GL 3.3

**Primitive choice — cross-quads are the SEADS default.**
- Single camera-facing billboard (4–6 verts): cheapest, but TUMBLES under camera roll —
  and SEADS rolls constantly on the sphere, so a billboard silhouette reads wrong. Also,
  at 4–6 verts, instancing may give little speedup (per-draw overhead swamps the vertex
  win) — profile it. Reserve for the far tier.
- **Cross-quads / X-tree (2–3 fixed intersecting quads): the workhorse.** Real
  view-independent silhouette, survives roll, cheap. More overdraw than a billboard, far
  cheaper than a mesh. (alanzucconi.com/2018/08/25/shader-showcase-saturday-7)
- Low-poly mesh: best silhouette/shading, highest cost. Shipped budgets: ~2,000 tris/tree
  at LOD0 was effective vs 12k+ for "full geometry." Use only for the very nearest trees.
  (polycount.com/discussion/204716)
- The central tension is fill-rate (cards → overdraw) vs vertices (meshes). Horizon Zero
  Dawn / GDC guidance: ACCEPT moderate overdraw rather than chase silhouette with polys;
  trim cutout cards tight; profile in an overdraw visualizer — no universal threshold.
  Prefer alpha-TEST (`discard`, no sort) over alpha-BLEND for dense foliage.

**LOD & impostors (all GL-3.3-safe; bake offline in the existing tool).**
- Octahedral/hemi-octahedral impostors: bake N×N pre-rendered views into one atlas; FS
  picks matching frame(s) by view direction, blends. Shipping params: **16×16 angles,
  2048² atlas, HEMI-octahedral** for foliage (more res on side views you actually see),
  depth-based frame blend + a baked depth map for parallax. Pure texture-atlas math in the
  FS — no compute/geometry shader. (github.com/wojtekpil/Godot-Octahedral-Impostors,
  shaderbits.com/blog/octahedral-impostors)
- LOD ladder: near mesh → reduced mesh → far billboard/impostor (the SpeedTree ladder).
  CROSSFADE the swap — the GL-3.3-friendly way is DITHERED alpha (`discard` keyed to a
  screen-space dither pattern with a distance-driven threshold); no MSAA-blend dependency.
  Bake impostor lighting to match the prior LOD so the pop is invisible.

**Scale — 100k trees is comfortable.**
- raylib `DrawMeshInstanced`: 100,000 instances × 576 verts ran at a constant 60 FPS
  (asteroids example); bunnymark hit 500k+ instanced quads at 60. 100k low-poly trees in
  a single instanced draw is well within reach. (github.com/raysan5/raylib/issues/1127)
- Per-instance transform = a mat4 vertex attribute (4 consecutive vec4 slots,
  `glVertexAttribDivisor(loc,1)`), standard LearnOpenGL layout. GL 3.3 supports instancing
  (3.1+).
- **CULL PER-CHUNK, NOT per-instance.** Chunk the world; frustum+distance cull whole
  chunks on the CPU; keep a per-chunk instance buffer. If you must cull single pieces,
  expand the frustum by max tree radius. Godot grass reference: 5×5 m chunks, ~10k
  blades/chunk, whole LOD'd system <2 ms (~12% of a 60 FPS budget) — trees are far
  sparser, so huge headroom. (hexaquo.at grass series, discussions.unity.com)
- **SEADS bonus:** free horizon/back-hemisphere cull — drop trees whose position dotted
  against the camera/planet normal is over the limb, before frustum test.

**Stylized monochrome foliage (silhouette-first).**
- Alpha-test cutout + flat or 2–3-step toon ramp on `N·L`; for pure mono you can flat-fill
  the cutout as a silhouette. Optional INK OUTLINE: render an enlarged back-faced black
  copy behind the tree (verts pushed along normals in the VS) — a clean GL-3.3 hard rim,
  ideal in mono. (en.wikipedia.org/wiki/Cel_shading, torchinsky.me/cel-shading)
- Wind (Crysis / GPU Gems 3 Ch.16, ports straight to GLSL 330): main bending displaces
  XY by wind × normalized height (`fBF*fBF - fBF` to limit artifacts, constrained to a
  sphere); detail bending sums 4 triangle waves at freq **1.975, 0.793, 0.375, 0.193**;
  vertex-color channels encode **R=edge stiffness, G=phase offset, B=branch stiffness,
  A=AO**. (developer.nvidia.com/gpugems/gpugems3 Ch.16)
- Alpha-to-coverage softens cutout edges but needs an MSAA framebuffer +
  `GL_SAMPLE_ALPHA_TO_COVERAGE` — **FLAG:** if not on MSAA, use hard alpha-test + screen-
  space dither instead. For a hard mono silhouette, plain alpha-test is the on-style choice.

**Northern-Ontario species (for silhouette design).**
- **Black spruce** — most common; narrow spire, club-tuft top; the iconic thin boreal spike.
- **Jack pine** — sandy/ROCKY sites (the Shield); scrubby, crooked, open crown; "pioneer
  on bare rock."
- **White/red pine** — taller, fuller, layered/rounded crown.
- **White (paper) birch** — slender white trunk, round open crown; first-in after fire;
  the pale vertical accent.
- **Trembling aspen** — pioneer, clonal stands, slender rounded crown.
- **Tamarack/larch** — deciduous conifer (bare grey spikes in winter, gold in fall),
  wet/boggy ground.
- Readable mono trio: **narrow spruce spikes + scrubby jack pine on rock + pale slender
  birch.** (ontario.ca common-trees, lotnoutfitters.com/pages/boreal-forest-trees)

**The Sudbury "moonscape" (drives terrain palette + placement).**
- Historically ~20,000 ha fully barren + ~80,000 ha semi-barren; hillside soil eroded to
  **exposed bedrock knobs blackened by SO₂**; NASA used it as a LUNAR-analog training site.
- Regreening formula (Laurentian): crushed limestone ~10 t/ha → grass+legume+fertilizer
  sod → tree seedlings (pine first, later ~75 species) the next year. 10M+ trees over 40 yr.
- **How it looks NOW: NOT a dense forest — sparse young conifers/birch dotted on black
  Shield rock, grass swards between, denser green in recovered valley pockets.** A DENSITY
  GRADIENT, not uniform cover. Monochrome suits it: dark rock, mid sod, light birch
  trunks, dark spruce spikes. (oala.ca/ground-56-regreening-the-moonscape, activehistory.ca)

**Placement from land-cover data.**
- Density-map-weighted sampling is standard: land-cover / canopy raster → per-cell density
  → place proportionally. (arxiv.org/pdf/2511.02417, arxiv.org/pdf/2008.05567)
- Scatter method: **variable-radius Poisson-disk** is best (min spacing shrinks in dense
  cells, grows in sparse ones → one pass gives dense stands AND sparse rock-dotting from
  the same field — exactly the Sudbury gradient). Jittered-grid is the cheap good-enough
  fallback; pure random clumps, pure grid too regular. (devmag.org.za/2009/05/03/poisson-disk-sampling)
- **SEADS fit:** offline raster → density → variable-radius Poisson → baked chunked
  instance buffers — the same offline-bake + instanced-draw shape the project already uses.

---

## Section 3 — Small-town buildings

**Extruding OSM footprints.**
- Height rule: `height = building:levels × 3 m` (literature range 2.8–3.5 m/storey; 3.0
  residential, 3.2 mixed, 3.5–4 industrial ground floors). (wiki.osm.org/Key:building:levels,
  gmd.copernicus.org/articles/15/7505/2022)
- **KEY PLANNING FACT:** globally <3% of buildings have `height`, ~4% have
  `building:levels`. **Assume ~97% of Sudbury/Chelmsford/Copper Cliff buildings are
  untagged** — you WILL default nearly all. Defaulting hierarchy: explicit `height` →
  `levels×3` → **type-keyed default by `building=*`** (house 3–6 m, industrial taller box,
  church tall+spire) → global fallback 6–10 m. A type lookup table beats an ML estimator
  for a stylized game.
- Roof: `roof:shape` = flat/gabled/hipped/pyramidal/skillion/dome/onion/etc. Collapse to
  ~3 archetypes (flat box, gabled, pyramidal/spire) keyed off `building=*`.
  (wiki.osm.org/Simple_3D_buildings)
- Mechanics: ear-clip the polygon cap (mapbox `earcut`/`earcut.hpp` handles non-convex +
  holes from OSM multipolygons), wall quads per edge `min_height`→`height`, roof on top.
- Tools to bake offline OR reference: OSM2World (roof handling, glTF export), blender-osm
  /blosm (imports w/ height + floors, default-height for untagged), BlenderGIS. Realistic
  SEADS path: write a small extruder in the existing `offline_tool/` reading the same GIS
  data as the terrain. (fosdem.org osm2world slides, github.com/vvoovv/blosm)

**Draw-call budget.**
- Instancing is GL 3.1+ (3.3 fine); raylib `DrawMeshInstanced` demoed 500,000 meshes at
  60 FPS → thousands of buildings is ~0.1% of headroom, one draw call per kit piece.
- **raylib GOTCHA (matches your rig-A "green gate is blind to the binary" trap):**
  `DrawMeshInstanced` does NOT work with the default shader — you MUST author a VS with an
  instanced mat4 bound to `SHADER_LOC_MATRIX_MODEL`; and `count==1` silently falls back to
  the non-instanced path (a shader that only works instanced breaks at 1). Add a
  source-level validator. Ref: `shaders_mesh_instancing.c`.
  (github.com/raysan5/raylib/discussions/1950, issue #1958)
- **Hybrid batching is strongest:** INSTANCE a small authored kit (houses, commercial
  block, church, headframe, water tower) for repeated stock; MERGE the long tail of unique
  OSM-extruded prisms into per-tile material-batched static meshes. Even a naive "instance
  one generic box per building + per-instance scale/color" renders the whole town in a
  handful of calls. (docs.unity3d.com DrawCallBatching, catlikecoding.com rendering-19)

**Authored kit workflow (Blender → glTF).**
- Small modular low-poly library (wall segments, roof caps, storefronts + hero props).
  Shipped low-poly city budgets: ~1.5K tris/building, 1024 px, ONE shared atlas for the
  whole kit → every piece shares one material → maximally instanceable. glTF/GLB is the
  clean raylib path; keep one material/atlas so instancing stays one-call-per-mesh.
  (ithappystudios.com, blenderkit.com, cgtrader.com low-poly city)

**1940s Sudbury / Chelmsford / Copper Cliff / Coniston built environment (makes it read as
THIS place).**
- **Mine headframes** (shaft towers) — the defining vertical steel skyline icon; a basin
  town is legible by its headframes.
- **Roast yards / roast beds** — open heaps of ore on cordwood, burning for weeks in
  sulphur smoke (Copper Cliff 1888; 3 yards within a mile by 1912; Coniston 40 heaps of
  60k–100k tons). ORIGIN of the blackened moonscape ground — ties to terrain palette.
- **Smelter** with tall stacks + night slag pours (glowing molten slag down banks — iconic).
- **Inco Superstack** — 381 m/1,250 ft, tallest chimney until 1987. **FLAG: built 1972,
  an anachronism for a strict-1940s town** — but it IS the modern recognizable Sudbury
  icon; include only if going for "timeless Sudbury" over strict period.
- **Company-town housing:** Inco/Mond rental houses, boarding houses; planned
  engineer/admin housing set APART from the mine vs a rough worker "Shantytown" + ethnic
  enclaves ("Little Italy" w/ Italian church). Read = tidy company blocks vs dense rough
  shacks. Vernacular = **wood-frame clapboard** miner houses.
- **French-Canadian Catholic parish churches** (the other hero silhouette): pattern is
  early wooden chapel → later monumental STONE/BRICK church with a TALL PROMINENT
  STEEPLE/SPIRE. Paroisse Ste-Anne-des-Pins (est. 1883, bell tower) is the Sudbury
  archetype — depict the earlier steepled church, NOT the modern 1995 rebuild.
- **FLAG (artistic license, NOT sourced):** the "silver/tin-clad spire" detail (common on
  Québec/Franco-Ontarian churches) and the presence of water towers / grain elevators
  could not be confirmed in Sudbury-specific sources. Treat as plausible stylization.
- Archives to mine for period photos: Inco Triangle Digital Archive (1936–1998, 612
  issues, free PDFs), City of Greater Sudbury Archives via Archeion (1930s–40s Sudbury Star
  photos), Republic of Mining (roast-yard history), Ontario Heritage Trust "Churches of
  New Ontario." (en.wikipedia.org/wiki/Copper_Cliff, heritagetrust.on.ca, republicofmining.com)

**Photogrammetry / photo-to-3D (long-term, hero-only).**
- Multi-view photogrammetry (RealityCapture paid, AliceVision Meshroom free) needs ~20–60
  overlapping views — works for a SURVIVING building you can walk around, FAILS on the
  handful of non-overlapping archival shots of a demolished 1940s building.
- Single-image AI (TripoSR/LRM class): minutes per asset, but no geometric constraints —
  invents wrong back/sides, blurry occluded textures. OK as a rough starting mesh for a
  hero prop an artist then cleans up; NOT for accurate reconstruction.
- **Verdict:** authored low-poly kit + OSM extrusion for the bulk town; photogrammetry a
  later hero-only enhancement. (knowledgeone.ca photogrammetry case study, arxiv.org LRM)

---

## Section 4 — Roads, trails, rivers as stylized linework

**Three approaches on terrain, compared.**
- (a) **Decals / projected textures:** render road spline to a target, project back per-
  pixel using scene depth; kill z-fighting with a severe depth bias. No draped geometry,
  follows terrain, cheap — but needs depth read-back, resolution-limited, edge bleed at
  grazing angles. (gamedev.net topic/691182, mtnphil.wordpress.com/2014/05/24/decals-deferred-rendering)
- (b) **Baked into terrain texture/splatmap:** preprocess road strip into colour/height/
  blend feature textures, blend terrain↔road while rendering. Zero z-fighting (it IS the
  surface), auto lighting/fog — but static, resolution-bound (bad for thin crisp lines
  unless you add an SDF channel), hard to animate dashes. (kosmonautblog.wordpress.com)
- (c) **Geometry ribbons draped on the heightfield — BEST for stylized crisp linework +
  the ONLY approach giving per-vertex arc-length UVs for animated dashes.** Triangle strip
  along the polyline, sample terrain height per vertex to drape. Coplanar → z-fighting;
  mitigate with `glPolygonOffset(1.0,1.0)` (`GL_POLYGON_OFFSET_FILL`) — **FLAG: polygon
  offset applies to FILLED tris, NOT `GL_LINES`, and offsets the terrain, not the ribbon;
  keep ribbons as TRIANGLES, or lift a tiny constant height above terrain.**
  (opengl.org polygonoffset FAQ)

**Ribbon mesh from a polyline.**
- Extrude ±half-width along the vertex normal (2 verts/point → strip); Mapbox does it in
  the VS as `delta = a_normal * u_linewidth`.
- **Miter joins:** extrude along the angle-BISECTOR of adjacent segment normals, scale by
  `1/sin(α)` (`width /= sinAngle`). **Miter limit → BEVEL fallback** past a threshold or
  sharp corners spike to infinity.
- UVs: cumulative arc-length along the polyline = U (drives dashes); ±1 across width = the
  AA/edge coord.
- World-space extrusion = constant physical width (right for a draped map ribbon).
  (mattdesl.svbtle.com/drawing-lines-is-hard, cesium.com/blog/2013/04/22/robust-polyline-rendering,
  medium.com/mapbox drawing-antialiased-lines, jvernay.fr/blog/polyline-triangulation)

**Dashed centerlines / trails.**
- Core FS dash: `if (fract(dist/(dash+gap)) > dash/(dash+gap)) discard;` with `dist` =
  interpolated CUMULATIVE arc length so dashes stay UNIFORM across variable-length segments
  (pass cumulative distance as a per-vertex attribute for `GL_LINE_STRIP`, or reconstruct
  from a `flat` start varying for `GL_LINES`). Cartographic dash arrays: `line-dasharray`
  e.g. `10,4`; snowmobile/ski trails = thin `~6,4` dash, optional round cap.
  (rabbid76.github.io dashed_line_shader, tilemill styling-lines, jcgt.org/published/0002/02/08)

**Map / blueprint aesthetic for linework.**
- Do NOT use `GL_LINES` (no joins/caps, no varying/wide width) — tessellate to triangles.
- AA = feather the perpendicular distance-from-center: `alpha = smoothstep(0.5-f,0.5+f,|v|)`,
  f≈0.15–0.5 (0.5 ≈ Agg-quality); this is a 1-D SDF edge → resolution-independent crisp.
  Needs width ≥ 2 for AA room.
- **Casing/outline hierarchy (the blueprint signature):** draw the line TWICE — a wider
  DARK case underneath, a narrower LIGHT fill on top; drive both widths by road class.
  Mapbox's Blueprint style conveys the ENTIRE road hierarchy through casing width alone.
  (medium.com/mapbox drawing-antialiased-lines + whats-in-a-mapbox-studio-style, tilemill casing)
- SDF glyphs (Mapbox packs fonts as SDF ranges) for crisp resolution-independent labels/icons.
- Hand-drawn/ink look: outline + hatch/cross-hatch, per-vertex "scribbly" overdrawn
  outlines, Tonal Art Maps for hatching. (alastaira.wordpress.com hand-drawn-shaders)

**Rivers & trails by scale.**
- Small rivers = thin TAPERED casing-less lines (width encodes flow); wide rivers/lakes =
  FILLED polygons with a stroke (SEADS already has mirror-water lakes — matches). Scale-
  adaptive stroke: <25 px no stroke, 25px–40k px 1 px @ 50%, large 2 px @ 50%. Intermittent
  streams = dashed. (tilemill styling-lines, saylordotorg cartographic-design, esri storymaps lines)

---

## Section 5 — The staged buildout ("good now" → "exquisite")

**The hard-gated order: blockout → LIGHTING/LOOK → hero asset → set-dress → polish.**
- Blockout FIRST from primitives (boxes/cylinders/planes) to validate proportion, camera
  read, focal hierarchy; treat blockout sign-off as a GATE before any art. Cheap to delete;
  finalized art is expensive to throw away — "keep it cheap until it is ready to become
  expensive." Detailing before the look is locked is the canonical waste.
  (rmcad.edu env-artist-playbook, book.leveldesignbook.com/process/blockout)
- **LIGHTING/TONE comes EARLY, not last:** "start with blockout lighting early to confirm
  value read and focal points... lock mood and post effects in the final pass." For SEADS
  this means: **lock the B&W stereoscope post stack (§1) FIRST, on graybox geometry** —
  before trees/buildings/roads. The look carries cheap geometry (see INSIDE, §6). This is
  the single highest beauty-per-effort layer and the CURRENT thread's "feel not fidelity."
  (rmcad.edu env-artist-playbook)
- **Vertical slice as look-proof:** prove the full look/tone stack on ONE small slice
  (e.g. a few blocks of Chelmsford + a stand of trees + one road) — "reveals shader
  pitfalls, texel/readability problems long before they spread." (rmcad.edu)
- **Silhouette/read-first + focal points:** hero landmarks (headframe, steepled church,
  smelter stack) must CONTRAST with surroundings; the majority of assets are deliberately
  SUBORDINATE. Composition judged by palette + silhouette + LOD as a set.
  (book.leveldesignbook.com/composition, 80.lv stages-of-environment-art)
- **Polish = break uniformity + add imperfection.** Named traps map directly:
  over-detailing, UNIFORM density (no focal points), and FIGHTING the established tone.

**Recommended SEADS layer sequence (beauty-per-effort):**
1. Lock the §1 post stack on graybox terrain — the whole look rides here. (Depth-driven
   aerial haze + DoF + clarity = the biggest single jump toward "stereoscope.")
2. Terrain palette to the moonscape read (dark blackened rock, mid sod) — already partly
   landed (Sudbury terrain thread).
3. Sparse boreal trees via density-gradient Poisson (§2) — silhouette-first, the biggest
   "populated world" jump for the effort.
4. Building MASSING (extruded OSM boxes + a few hero silhouettes: headframe, church,
   smelter) (§3) — read before detail.
5. Road/trail/river casing linework (§4).
6. Set-dress + polish: kit-detail the hero buildings, break density uniformity, tune post.
- **Trap for THIS project specifically:** the green gate is blind to the app binary
  (per CLAUDE.md) — every new instanced draw (trees, buildings) ships shader-linkage/
  count==1 bugs green; pair each with visual smoke + a source-level validator (rig-A trap).

---

## Section 6 — Visual references

**Games with a strong stylized B&W / vintage look (and the technique that carries it):**
- **INSIDE (Playdead) — the key SEADS reference.** GDC "Low Complexity, High Fidelity":
  cheap geometry looks exquisite because the LOOK is the lighting/post stack — lighting as
  separate diffuse/specular/bounce entities, local shadowed VOLUMETRICS for atmospheric
  depth, analytic primitive-based AO, SSR, and **dithering to kill banding** in the
  near-mono palette. This is the "post + lighting make simple geometry beautiful" thesis.
  (gdcvault.com/play/1023002)
- **LIMBO (Playdead)** — hand-drawn B&W silhouettes against a soft glowing backlight
  (rim-light simple geometry to pure edges/negative space) + volumetric fog/light rays +
  film grain + shallow DoF + minimal palette. A lighting-and-post look on simple shapes.
  (en.wikipedia.org/wiki/Limbo_(video_game), joshli1997.medium.com film-grain)
- **Return of the Obra Dinn (Lucas Pope) — 1-bit "dither-punk."** Render normal lighting,
  then a post shader computes luminance (`0.299R+0.587G+0.114B`) and THRESHOLDS each pixel
  against a dither NOISE TEXTURE → pure 1-bit. Bayer ordered matrix (neat, for detail) vs
  BLUE-NOISE (organic, for environments). Motion sickness solved by ANCHORING the dither to
  the scene (scroll the pattern by camera rotation/FOV so it moves WITH the world). Renders
  2× then downsamples. This is the most directly portable "printed card" mode.
  (danielilett.com/2020-02-26-tut3-9-obra-dithering, gamedeveloper.com dither-punk)
- **Trek to Yomi** — Kurosawa look: full B&W + film-grain celluloid overlay + letterbox +
  bloom (toggleable), plus 3-plane cinematic staging (playable/fg/bg) for depth. Directly
  relevant to composing depth in a mono frame. (unrealengine.com tech-blog inspired-by-kurosawa)
- **Betrayer** — desaturated near-mono world with selective SPOT-COLOR (red) + player
  contrast/desaturation sliders + full-color toggle. (vgblogger.com betrayer-black-and-white)
- **Genesis Noir** — film-noir mono, brilliant whites, selective yellow/gold accents
  (same spot-color family). White Night is the additional B&W-noir cousin.
  (en.wikipedia.org/wiki/Genesis_Noir)
- **Cross-cutting:** every strong example rides the same stack — desaturate/threshold +
  grain + vignette/letterbox + fog/bloom + optional spot-color — as POST over simple
  geometry. Obra Dinn's scene-anchored dither is the most portable to SEADS's fragDir world.

**Old stereoscope / stereograph aesthetics (the look to emulate):**
- Format: two eye-spaced photos on cardstock (Holmes ~3¼"×7"), brain fuses to hyperreal 3D.
- **Curved/warped mount** (concave toward the print, late-1870s+) enhanced the depth
  illusion — a period-authentic signature to fake with a subtle screen curve/vignette.
- Tone: **albumen SILVER prints** → glossy, sharp, WARM sepia-silver (the defining cast).
- **Oval VIGNETTE** + thin colored framing lines around the print.
- **Hyperstereo:** lengthening the camera baseline yields EXAGGERATED ("hyper") depth — the
  uncanny miniature-diorama feel, directly analogous to the little-planet exaggerated depth.
- Biggest public-domain trove: **Library of Congress "Stereograph Cards"**
  (loc.gov/pictures/collection/stereo/ — blocks automated fetch, BROWSE manually); also
  Brown University "Stereoscopy Digitized." (en.wikipedia.org/wiki/Stereoscope, cycleback.com,
  shortcourses.com/stereo)

**1940s Sudbury / Northern Ontario mining photography (archives, with the look):**
- **Inco Triangle Digital Archive** — Inco employee magazine 1936–1998, 612 issues, FREE
  PDFs by year/month; richest 1936-on Sudbury mining/community visual record.
  (museums.greatersudbury.ca/explore-local-history/the-inco-triangle-digital-archive)
- **City of Greater Sudbury Archives via Archeion** — 1930s–40s Sudbury Star photos
  (Creighton Mine, headframes, streetcars, Austin Airways floatplanes); SILVER-GELATIN
  newspaper prints = high contrast + grain. Curated: cosmicray.ca/writing/sudburyarchivephotos.html
- **Laurentian University Archives** (J.N. Desmarais Library) — regional mining incl.
  Falconbridge (by appointment). Also Library and Archives Canada (broader national search).
- **The look to emulate:** high-contrast fine-grained mono; deep blacks in headframe/slag
  shadows to bright industrial highlights; slight warm-neutral (silver→sepia) cast — pair
  with a stereoview vignette + curved frame + hyperstereo depth for a "1940s Sudbury mining
  stereograph."

---

## Gaps / caveats flagged honestly
- **Depth in raylib** is the one real engineering wrinkle for the strongest look (§1 depth
  cues) — needs a custom depth-texture FBO or a depth pre-pass. Everything else is a plain
  RenderTexture chain.
- **1-bit Bayer/dither FIGHTS smooth silver tonality** — use subtle multi-level dither/grain
  for "newsreel"; reserve hard dither for an explicit "printed card" mode.
- **Silver/tin church spire + water towers/grain elevators** for Sudbury = artistic license,
  NOT confirmed in Sudbury-specific sources.
- **Inco Superstack is 1972** — anachronistic for a strict-1940s town.
- LoC stereo collection + the Copper Cliff geotour PDF block automated fetch — browse
  manually for the image troves.
- The JCGT "Antialiased Dashed Stroked Polylines" (Rougier 2013) PDF is the most rigorous
  primary source for exact dash/cap/join distance formulas — fetch directly if needed.
