# SEADS — World Build Plan: executing the legible little world

*The **HOW** for the world/art layer. Companion to `docs/world_art_direction.md` (the **WHY** — the
ratified vision) and `SPEC.md` (the flight constitution, which wins on any seam conflict). Written
2026-07-07 after (a) the far-side disorientation was **solved on the camera side** (freelook re-orients
the view without touching the raw-mouse control basis — the `horizon_recovery_plan.md` thread landed and
flew clean), and (b) two fresh-context Fable 5 consults: an architecture pass, then a red-team of this
plan (its P0/P1 findings are folded in below). This slims the rejected "Gemini map build" package down to
its sound process spine and retargets it at Chad's actual vision.*

---

## 0. North star — what the world is FOR (re-justified)

The world is **no longer the fix for motion sickness** — the camera thread solved that. That subtracts
the one pressure that would have justified a heavy realistic planet. What remains is the vision on its own
terms, and it is enough:

1. **Tactical legibility** — read friend/foe, orientation, altitude, speed, and energy *instantly* from
   the frame. The one quantity the whole art direction exists to maximize: **can I see the other plane?**
2. **Identity** — one saturated color per plane; teams are **complementary duos** (orange+blue, green+red,
   purple+yellow). Picked **per match** (you choose which duo you fly in), always complementary; a large
   match is up to four duos (e.g. 8 orange+blue vs 8 green+red). You read *teams* at a glance, like painted
   squadron liveries.
3. **Charm & mood** — a 1940s black-&-white newsreel "little world": **high contrast, minimal grain**
   (Chad's call — crisp filmic, not a dust storm), familiar small-town-Canada silhouettes generalized at
   **~5× scale** so they read from combat altitude and speed.
4. **Reinforce the feel** — planes that "fly as intimidating as they look": shiny, glinting, fast, punchy.

**The place — Greater Sudbury, Ontario (arted up).** The whole little world IS the Greater Sudbury area,
stylized. This is a gift for the premise: Sudbury sits in a real **impact crater** (the Sudbury Basin), so
the **ring of hills with low valleys between** is the region's *actual* relief. And the arithmetic almost
fits by itself — the sphere's surface (4π·15² ≈ 2,830 km²) and Greater Sudbury (~3,600 km²) are within
~25% in area, so the whole region wraps the ball at **~0.9× (essentially 1:1, no enlargement)**. *Honest
deflation:* at a ~4–5 km visible ground cap the crater rim never reads as a *ring* (the ~60 km basin spans
~230° of arc on the 94 km-circumference world) — it reads as a **hill WALL you cross**, a binary
inside/outside-the-basin instrument. Still a genuinely useful two-zone global read; just don't promise a
visible ring silhouette. The region's **300+ lakes** (Ramsey in Sudbury,
Whitewater in Azilda, Wanapitei by the airport) become mirror-silver water (§2); its named towns —
Sudbury, New Sudbury, Garson (airport), Capreol, Hanmer, Val Caron, Chelmsford, Dowling, Levack, Onaping
(the mines), Azilda, Lively — each carry **visible 3D landmarks** (water towers, Chelmsford's French
church, the Inco **Superstack** / "Big Smoke", the **Big Nickel**, the tailings ponds). The mission's
"known-size familiar silhouettes = the orientation instrument" is satisfied by a *specific real place*
rather than a generic town — and Sudbury's mining-town iconography is a stronger, weirder read than
anywhere generic. **Whimsical, not a survey:** faithful to the region's *feel and landmarks*, not its GPS.

**Judge by feel, not fidelity. Do not gold-plate.** A realistic planet is still the wrong answer — not
because of sickness now, but because monochrome-world-plus-chroma-planes is the whole *tactical read*, and
recognizable Earth fights it. (`world_art_direction.md §1–3`.)

## 1. The seam — non-negotiable (SPEC §5/§6/§9; the donor package got this exactly right)

The world is the **disposable shell**; the flight kernel is sacred. Every gate ends by running the
kernel's own `ctest` — **a world change that moves a flight golden or reddens a flight AT is an automatic
regression halt.**

- **No write into the kernel.** Nothing in `render/`|`world/` assigns to, mutates, or holds a mutable
  handle into `sim/`|`control/` state. Render *reads* sim/control by const-ref only.
- **Camera-rotation basis stays RAW.** The camera may ease position / FOV / shake / **sun direction**;
  it may **never** feed a smoothed/slewed basis into the aim or camera-**rotation** path (SPEC §9.1/§9.2).
  A full-screen post pass touches pixels only and is seam-safe by construction — it never sees the basis.
- **No `(lat, lon, heading)`, no fixed up/down axis, no tangent-plane-as-world.** Every "up" is its own
  `normalize(pos)`, recomputed, never cached, never global. **The trap the token-grep can't see:** the
  existing planet fragment shader parameterizes by equirect `(u = atan2(z,x), v = asin(y))`
  (`render/planet.cpp` FS). *Any procedural pattern derived from that `(u,v)` is lat/lon in disguise* —
  latitude-scaled, pole-pinched, invisible to a `lat|lon` name grep. **All procedural ground/scatter must
  be a function of the 3D direction `fragDir` (3D hash / cube-face param), never `(u,v)`.** The P1 red-team
  explicitly checks the field grid *at a pole*.
- **Config, not code.** No bare numbers in `render/`|`world/` or a shader — they live in `config/world.toml`.
  (First cleanup: the live `constexpr kReliefScale/kSubdiv/kUOffset` and the fixed `sun` in
  `render/draw.cpp:34-47` move into it in P1.)
- **R is single-source, not mirrored.** Render already receives `params.R` by argument
  (`render/planet.h`, `draw.cpp:41`) — keep passing it through; put only world-native values in
  `world.toml`. (An asserted `world.radius_m == params.R` mirror is *safe* but re-creates the duplication
  the repo's H1 lesson exists to discourage — avoid it.)
- **Chad flies every felt change**, one knob per flight, logged in `docs/flight-log.md` (§5). Autonomy is
  for the objective half only (§4).

**Landing-ready — a PLANNED capability; build the rails now, not the logic (Chad, 2026-07-07).** Chad
intends to **land on airstrips** later. Landing *interaction* (gear/flaps/ground-contact/flare) is a future
**kernel** change — today the frozen sim rules "no landing in v1; `altitude ≤ 0` = crash → reset" — so the
world build does NOT implement it. But it must **not preclude it, and must lay correct rails**, so that when
the kernel grows landing it needs **zero world/render rework**:
- The **persistent height sampler** (§2) is exactly the ground-elevation-at-direction query a future
  ground-contact test consumes — build it authoritative and single-source (it is the same anti-fork the
  props need). This is *why* we build it robustly now.
- Each authored runway is **flattened to a genuine flat plateau** (a real constant-height mesa, §P5) and
  registered as a **`LandingSurface`** data region: world-Cartesian `anchor`, surface `normal =
  normalize(anchor)`, a **heading-free tangent basis** (two orthonormal tangents, NOT a heading angle),
  extent, an approach-corridor descriptor, and an inert `state`. Tolerances live in `world.toml [landing]`.
  Pure data the future kernel queries ("is the aircraft inside this box, gear down, below approach speed?")
  — defining it does **not** breach the seam (it is an input, not a sim mutation). Heading-free + Cartesian
  so it survives the §6 lat/lon ban. This is FWD-1 from the donor package, kept as a real deliverable.

## 2. Architecture — how the layer sits (Fable 5, grounded in and corrected against the actual code)

The cubesphere renderer already exists and is good (pole-free, uniform texel density, DEM displacement):
`render/planet.*`, drawn eye-relative in `render/draw.cpp`. **Evolve it, don't restart.**

- **"Drop Earth" is TWO swaps, not one — the DEM carries the coastlines too.** Replacing only the albedo
  leaves Earth's continent/coastline *silhouettes* in the displaced mesh (`earth_dem_2048.png`,
  `planet.cpp` DEM load; displaced 600 m at `draw.cpp:34`) — and P1's slope-darkening would make them read
  *harder*. Recognition beats any lighting trick (`world_art_direction.md §1`), and landform shape is
  recognition. So **P1 replaces both the heightfield and the albedo** — but with **Greater Sudbury**, not
  generic procedure (§0 "The place"). Two source layers, both *authored art baked to the cubesphere at load*:
  (1) a **Sudbury-region heightfield** (the crater-rim hills + valleys) → the DEM; (2) a **landmask** (lake
  vs land) → drives the water material (below). Between the lakes, land gets a **procedural grayscale
  farmland/bush patchwork** (~200 m cells, 3D-hash on `fragDir`) so B&W ground keeps the high-frequency
  contrast optical-flow needs for speed/altitude cues. Keep the cubesphere mesh + normals + tangent warp.
- **Seam ruling — authored source art is equirect INPUT; the RUNTIME must be made 3D-direction (needs a
  code change the plan mis-stated).** *Verified against the code:* the DEM genuinely IS baked at load
  (`radius_at` → `build_face`, CPU, pixels freed at `planet.cpp:239`). BUT the **albedo is still sampled
  per-fragment at runtime** via equirect `u=atan2(z,x)/2π`, `v=asin(y)/π` (`planet.cpp:46-47`, sampled
  :54; the per-vertex texcoords at :153-157 are dead), AND the shader **draws a global lat/lon graticule**
  (`planet.cpp:64-71`) — a rendered fixed-frame anchor, exactly the memorized-global-up channel the vision
  exists to kill. If the landmask rides that live `(u,v)` path, lakes near the caps pinch into stars and
  their mirror boundary smears. **P1 fix (cheap, total):** at load, CPU-resample the authored
  heightfield + landmask + albedo into a **GL CUBEMAP** (or six face textures); the FS samples
  `texture(cube, fragDir)` — no runtime `(u,v)` anywhere, hardware-filtered, pole-free by construction —
  and **delete the graticule**. Then the ruling ("no runtime lat/lon; detail keys on `fragDir`") is
  literally true, and the pole red-team checks authored *content* at the caps, not sampler math.
- **An authored real place does NOT reintroduce Earth's memorized-up problem.** Earth failed on *inherited*
  hemisphere recognition (continent silhouettes + a lifetime of north-up maps). Sudbury on a ball carries
  no inherited orientation; its landmarks are LOCAL radial beacons and the town-adjacency graph is the
  orientation instrument. The rules that keep it safe: **no compass, no north arrow, no rendered graticule,
  and towns spread over the WHOLE sphere so no hemisphere is "the front"** (§Q3b). Off-pole placement
  handles the pinch; the recognition fear doesn't transfer.
- **Mirror-silver water — the signature look and a free orientation cue (feasible; four traps).** In a B&W
  photo a calm lake is near-black except where it mirrors the bright sky, then it flashes **silver**. *Shape*
  (verified against the shader): one FS branch on the landmask — no second mesh, no render-to-texture. Add
  an `eyePos` uniform + a planet-local `fragPos` varying (`vertexPosition` is already planet-local, drawn
  eye-relative at `planet.cpp:288-291`); then `V=normalize(fragPos−eyePos)`, water normal `n=normalize(fragDir)`
  (**not** `fragNormal`), `refl=reflect(V,n)`, sky sampled on `dot(refl, normalize(fragPos))` (reflected-ray
  elevation over the LOCAL horizon — pure 3D-direction, seam-clean), sparkle `pow(max(dot(refl,−sunDir),0),k)`
  reusing the existing `sunDir` uniform. **The four traps that break it:**
  (1) **A flat sky reflects a constant → no beacon.** The "300 lakes flash toward up" promise is FALSE until
  the sky has a gradient. Pull a minimal 2-stop zenith→horizon gradient into **P1 step 1** (~5 shader lines,
  the same function P4's glow formalizes) and **single-source it** so water and background sky can't disagree
  at the horizon.
  (2) **Flatten each lake at BAKE, or the mirror sits on bumpy geometry.** Lakes are the same displaced +
  Gaussian-blurred mesh (`ImageBlurGaussian`, `planet.cpp:229`), so shores bleed and the surface dishes.
  Force each lake interior to one constant height under the mask *before* the blur. Bonus: this removes the
  z-fight worry (no separate water plane) and the "sea level at R" mistake (`planet.h:36-38`: DEM 0 = R;
  Sudbury lakes sit at ~250 m relief — per-lake height comes free from the flattened DEM).
  (3) **Landmask resolution floor.** At 2048-wide source one texel ≈ 46 m; a real 100 m pond is a gray
  smudge. Set a **minimum authored lake ≈ 300–400 m** (the whimsy license) and threshold the mask with an
  `fwidth` smoothstep for crisp shores.
  (4) **Sparkle aliasing** on a distant 4-pixel lake strobes — clamp specular sharpness by `fwidth`/distance
  (one line; the difference between "beacon" and "shimmer noise").
  Net: bold move #0 (§6), and it makes the water carry as much legibility as the planes.
- **Monochrome = per-material, plus one thin cosmetic post pass — NOT full-scene-desaturate-then-mask.**
  Desaturate *inside* the world/prop shaders (luminance + a config tone curve → gray). Planes get their
  **own full-color shader**. Grain + vignette are one full-screen `RenderTexture` pass on the 3D scene —
  **then the HUD / reticle / nose-marker draw RAW on top** (load-bearing: the reticle-to-nose gap *is* the
  controller error, SPEC §9.2 — grain on it injects noise into the pilot's control loop). Two raylib traps
  to name in P2: (i) a `RenderTexture` is **not multisampled**, so it silently trades away the
  `FLAG_MSAA_4X_HINT` 4× AA (`app/main.cpp`) — worst on the thin high-contrast prop silhouettes that carry
  the read. **Chad wants minimal grain, so grain will NOT mask the lost AA** — keep an FXAA-in-post or
  supersample knob as a real reserve, not a fallback afterthought;
  (ii) the vignette center must **follow `lens_shift_ndc`** (`draw.cpp` lens-shift) or it de-centers when
  the lens shift is active; (iii) the flipped-Y source-rect blit.
- **The plane "glint" is a prerequisite, not polish — it is 0% built.** Planes today are flat `DrawCube`
  calls (`render/draw.cpp:84-88`). "Saturated glinting planes" needs ONE plane shader: Blinn-Phong spec +
  fresnel rim, team color + pair-role from config, geometry moved to a mesh. Build it in P2 — brightness
  (glint), not chroma, is the spotting mechanism at combat range (a 10 m plane at 2 km ≈ 5 mrad, a few
  pixels; color barely helps, a glint flash carries).
- **Sun → "noon everywhere," eased.** Replace the fixed world sun (`draw.cpp:46-47`) with
  `sun_dir = -normalize(player_pos)`, eased by a **time-constant** (not a per-frame lerp alpha — keep the
  frame-rate independence deliberate, per the S7-mouselevel lesson). Seam-safe: a lighting uniform never
  touches the basis. At 200 m/s it rotates ~0.76°/s, imperceptible. *Precisely:* this is zenith light **at
  the player**; terrain θ away is lit θ off its local-up — fine only because the visible cap is ~15–20° at
  combat altitude (don't let anyone "fix" distant lighting). Zenith light flattens relief — recover it with
  in-shader slope-darkening from the existing analytic normals, **not** by tilting the sun (any world-fixed
  tilt azimuth re-imports a weak global anchor).
- **Extract the sphere machinery FIRST (P1), or P3 forks it** (shape fixed by the Fable pre-impl audit).
  The prop position `dir · radius_at(dir)` and the face/tangent-warp param the scatter reuses are **private**
  (anon namespace in `planet.cpp`), and the DEM pixels are **freed** right after mesh build
  (`UnloadImageColors`, `planet.cpp:239`) — P3 would re-derive a height sampler (an H1 fork: props float/sink).
  Extract a **`render/sphere_param.{h,cpp}` — glm, ZERO raylib — into `seads_render_core`** (the raylib-free
  lib ctest links; `planet.cpp` is app-target-only, so a raylib-typed extraction would be **untestable**,
  the single highest risk here). Expose: `face_basis(f)` (the mesh face table), **separate** `warp(x)=tan(xπ/4)`/
  `unwarp(x)`, `face_dir(f,s,t)`↔`dir_to_face(dir)` (major-axis select, deterministic edge tie-break),
  `equirect_uv(dir,u_offset)` (today forked 3×: `radius_at`, the DEAD per-vertex texcoords `:153-157` (delete
  them), and the GLSL copy — the GLSL dup stays until the cubemap bake deletes it), and a
  `struct HeightField{ vector<uint8_t> px; int w,h; double R,relief_scale,u_offset; radius_at(dir); }` with
  `Dem::sample01` semantics EXACT (`planet.cpp:95-109`: −0.5 texel center, u wraps mod, v clamps).
  **Split `build_face` into a pure `fill_face(HeightField,f,N)->{verts,normals,indices}` (std::vectors) + a
  thin raylib `UploadMesh` wrapper** — the pure half is what the tests pin. **HeightField ownership:** built
  from the **post-blur / (later) post-flatten** pixels (the same buffer the mesh consumed, red channel only),
  stored in `Planet` (a process-lifetime static — no leak); NEVER a file re-read (that samples the un-blurred
  field → props float/sink by the blur delta). **R is captured** (with relief_scale + u_offset) as HeightField
  members from the `params.R` `load_planet` already receives — so world.toml stays R-free and mesh/props/cull
  read one captured value. **Two conventions, named in the header, never merged:** the MESH param is
  warped `face_basis`; the coming **GL cubemap** texel param is *unwarped gnomonic in GL's own face axes*
  (NOT `face_basis` — reusing it bakes faces flipped/rotated). *Honest limit:* a prop sits on the sampler
  surface, not the rendered triangle (mesh linear between ~118 m verts vs bilinear ~46 m texels → metre-scale
  mid-quad divergence on steep blurred relief) — plan a small `prop_sink_m` embed at P3, don't add a 2nd sampler.
- **Radial props = new `world/props.*` (pure data) + `render/props.*` (draw).** A prop is
  `{unit dvec3 dir, type, scale, spin}`; position `= dir · height_sampler(dir)`; up `= dir` (its own
  `normalize(pos)`). The tangent frame is built from `dir` + a stable helper axis (as `planet.cpp` already
  does; mind the `|d.y|<0.99` helper flip so a prop straddling it doesn't reorient across a rebuild) and a
  hash-seeded **spin about `dir`** (a cosmetic angle on a static prop — *not* a heading; say so plainly).
  **Placement without lat/lon:** tan-warped cube-face cells with per-cell jittered samples and hash-density
  on the 3D direction. **Hash INTEGER cell indices `(face, i, j)` + the `world.toml` seed** (never the
  float direction — its ulps drift across toolchains and would make the determinism gate re-baseline-bait).
  Use tan-warped cells (or Jacobian-weighted per-cell counts) so density is uniform — raw cube cells are
  ~5.1× denser at face centers, a visible artifact on a "known-size ruler" world.
- **Per-prop horizon cull, prop-height-aware.** A prop stands on relief up to `H_max` (max relief + max
  prop height) above R, so it's visible *past* the ground horizon (the steeple poking over the rim is the
  charm shot). Threshold `= cos(acos(R/(R+h_eye)) + acos(R/(R+H_max)))` with `h_eye = |eye|−R` (the camera
  eye, not the plane), `H_max` from `world.toml`; cull if `dot(dir_prop, normalize(eye)) < threshold`.
  Mutation-verify with a prop placed just past the geometric horizon (the S4a "pin the predicate" lesson).
  Translations rebuilt eye-relative in double each frame (the `rel()` seam), `DrawMeshInstanced`. Budget:
  one prop / 500 m ≈ 11k, / 200 m ≈ 70k — instancing eats this trivially.

## 3. Build order (each phase: plan → build → objective gate → red-team separate-context → Chad flies → tag → next)

**P0 — Integration. DONE.** `sandbox/planet-art` is a full ancestor of `sandbox/flight-tuning-2`;
`render/planet.*` is already on this branch. Nothing to merge.

**Entry gate for P1 (do before tuning any value):** (1) ratify a **Gemini style bible** — 1940s B&W
Northern-Ontario mining-town aerials — as the fixed reference every later contrast sortie is judged against;
(2) ratify a one-page **REGION GRAPH** — towns as nodes + which-neighbors-which, the hero lakes (Ramsey,
Wanapitei by the airport, Whitewater), the rim wall. **This graph IS the entire fidelity contract**: it
pins the load-bearing adjacency that makes it Sudbury, and everything NOT on it (the antipodal seam, the
polar caps, every shoreline vertex) is explicitly free invention. This is the single cheapest guard against
the fidelity ratchet (§Q4) — it turns "is it faithful?" from an unbounded survey into a checkbox. Also land
the **target-visibility probe** (§4) and the `render/sphere_param.{h,cpp}` + persistent height sampler
extraction (§2).

*Fable pre-impl audit amendments to P1 ordering:* (i) the **sphere_param increment also wires `load_world`
into the app and deletes the `draw.cpp:34-39` constexprs** (`kReliefScale/kSubdiv/kUOffset`) — leaving the
`relief_scale_m` config mirror dead across even one more commit is the worst state; fold it in here. Add the
config keys P1 will consume: `subdiv`, `u_offset`, the **DEM blur radius** (`planet.cpp:229` — coupled to
relief, so it moves with it), the **sun** vector + `sun_ease_tau_s`, and an asset **`source = "earth" |
"sudbury"`** string + path keys (ages better than the bare 0/1). (ii) **Bake the cubemap against the
EXISTING Earth albedo first** — same planet, new sampler path, graticule deleted — so the bake mechanics get
a pixel-level A/B with zero new-art confound *before* any Sudbury source exists. (iii) The **sky gradient
must precede water** (it is water's reflected source; §2 water trap 1). (iv) fovy is now **dynamic** (S9-zoom,
`draw.cpp:144`), so any sparkle/LOD anti-alias clamp keys on `fwidth` (zoom-aware for free), never distance,
and the target-visibility probe measures at **both 60° and 30°**.

**P1 — Drop Earth, get the value ladder right.** This is the plan's cliff — do it as *single-knob sorties
behind a `world.toml ground = earth | procedural` toggle* (the `Planet{ok=false}` fallback pattern already
exists), so every step has an A/B and nothing lands four coupled changes before Chad flies:
  1. **Sky value + minimal gradient** — flip the dark-navy space clear color (`draw.cpp` `ClearBackground`)
     to a config **Mercury-silver** with a **minimal 2-stop zenith→horizon gradient** (not a flat value —
     the water mirrors this, and a flat sky reflects a constant = no lake beacon; the full per-pixel glow is
     P4). **Single-source the gradient function** so sky and water can't disagree. Do this FIRST: the ground
     tone curve can't be tuned against a background that will later change, or every contrast flight re-flies.
  2. **Noon sun** as its own sortie (before the albedo swap).
  3. **Sudbury sources → GL cubemap + delete the graticule** (§2 seam fix). Bake the authored heightfield
     (crater-rim hills + valleys, replacing the Earth DEM), the **landmask** (drives the water), and the
     albedo into a cubemap sampled by `fragDir`; **flatten each lake to a constant height under the mask
     before the blur** (§2 water trap 2). Land this FIRST with a **placeholder hand-blobbed source** (rim
     wall + ~20 hero lakes, an afternoon) behind the `ground = earth|procedural` toggle, so the value ladder,
     water, and pole check tune against real pixels while the source-art question (§7) resolves in parallel.
  4. **Procedural grayscale farmland/bush** on the land between lakes + slope-darkening; the ratified grays
     are the fixed value ladder (silver sky / silver water-flash / field-light / field-dark / shadow)
     everything else tunes against.
  Also: move the `draw.cpp` constexprs → `world.toml`.
  *Objective gate:* builds; shaders compile GLSL 330; **field grid + landmask checked at a pole** (no
  `(u,v)` procedural leak; Sudbury source placed off-pole); flight `ctest` green. *Chad flies:* the ground
  reads as a place with legible speed/altitude and the lakes flash silver toward the sky; one knob per
  flight (sky/water silver, sun-ease, relief scale, tone curve, ground contrast, field scale, water
  reflectivity/sparkle).

**P2 — The B&W look + saturated, glinting planes.** Per-material tone curve (world/props gray); the plane
spec+rim+**glint** shader with team-pair colors; repaint own plane + the **S8 drone fleet** into
complementary pairs; the `RenderTexture` grain/vignette post with the HUD drawn raw over it.
*Objective gate:* shaders compile; **no `smoothstep(a,b,x)` with `a≥b`** (GLSL UB); the **target-visibility
probe** shows the drone still pops against the P1 ground at 1–2 km; flight `ctest` green. *Chad flies:*
planes POP against the world; knobs — contrast, grain amount, rim/glint strength, spec tightness, palette.

**P3 — Radial props + ONE hero town (Sudbury proper).** `world/props.*` + `render/props.*` on the P1
sphere_param + height sampler; tan-warped cube-face scatter, integer-index hash-density, radial up,
instanced, prop-height-aware cull; primitive-composed silhouettes (northern bush/redwoods, water tower,
church steeple, headframe/smokestack). Prove the pipeline on **ONE hero town = Sudbury**: a hand-authored
`{dir,type,scale,spin}` list of its signature landmarks — the **Inco Superstack** ("Big Smoke", a tall thin
radial spike = a superb long-range orientation beacon), the **Big Nickel**, a **water tower**, the
**Ramsey Lake** shoreline read against the crater hills — placed by flying there and noting directions,
never coordinates. Build the deterministic **`--dump-manifest`** here (first phase with generated data).
*Objective gate:* determinism-dump hash stable across two seeded runs (+ per-cell counts so density is a
diff); zero bounding-box overlap probe; every prop up `== normalize(pos)` within tol; the cull mutation
(prop just past horizon) verified; flight `ctest` green. *Chad flies:* legibility + charm — can he name the
place from the air.

**P4 — Bold moves + polish.** Per-pixel horizon **glow** (view-ray vs `local_up`, recomputed per frame);
white contrails; wing sun-glint's full form (prototype it under the noon sun in P2 first — under a zenith
sun the glint is bank-driven, likely a feature but an untested coupling).
*Objective gate:* frame budget; flight `ctest` green. *Chad flies:* the whole thing, for keeps.

**P5 — Fill the region (expansion, not gold-plating — it IS the world).** Only after P3 proves the pipeline:
scatter the rest of Greater Sudbury, each town a small hand-authored landmark list — **Chelmsford** (the big
French church), **Garson** (the airport: a runway flattened to a genuine constant-height plateau + a real
`LandingSurface` data rail — heading-free, queryable, built to spec because **airstrip landing is planned**;
the interaction itself is a future kernel change, §1 "Landing-ready"), **Wanapitei Lake** by the airport,
**Whitewater Lake** in
Azilda, the **tailings ponds**, and **every town's water tower** as a large 3D structure (Capreol, Hanmer,
Val Caron, Dowling, Levack/Onaping mines, Lively, New Sudbury). The 300+ lakes already exist from the P1
landmask; P5 adds the built landmarks. Stage town-by-town, each a Chad fly-by; global density/LOD + horizon
culling keep it cheap. *Objective gate:* determinism + overlap + cull hold at region scale; frame budget;
flight `ctest` green.

**Autonomy boundary (draw it explicitly).** *Autonomous-safe* = everything with a deterministic oracle:
mesh/heightfield gen, scatter determinism, horizon-cull correctness (incl. the mutation), instancing perf,
the target-visibility probe, and every seam check (§4). *Human-only feel-loop* = every value Chad sees or
feels: sky value, tone curve, grain, prop scale/density, sun-ease, team palette. Structure it like
`docs/flight-log.md` — one knob per flight, hypothesis → observed — with a new **"Legibility" star** beside
Tracking, and put the probe's contrast number next to the feel call.

## 4. The process spine (kept from the donor package — this part was sound)

- **Target-visibility probe (the one instrument the art direction most needs, and nobody had).** A
  deterministic screenshot rig (built on the existing `SEADS_SPAWN_ALT`) places a drone at 500 m / 1 / 2 /
  3 km against the procedural ground and measures its pixel luminance/chroma contrast vs the local
  background patch. It catches the *ground-busyness-vs-plane-pop collision* (§P1 adds ground contrast; §P2
  needs the plane to beat it) the moment P1 tunes field contrast — not at P4 — and gives every Legibility
  sortie a number. Phase-gated from P1 on.
- **Deterministic headless worldgen dump** (`--dump-manifest`, built in P3): chunk/prop table + per-cell
  counts + hashes, so scatter regression is a diff. Seed from `world.toml`, **hash integer cell indices**,
  never wall-clock, never a float direction.
- **Mechanical seam gate** (grep, cheap, every change): no `world/`|`render/` write into `sim/`|`control/`;
  no `lat|lon|heading`/fixed-axis token; no `lerp|slew|filter|smooth|ease` on an aim/rotation-basis path;
  no `aircraft.toml`|`controller.toml` read. (Note its blind spot: it can't see `(u,v)`-as-lat/lon *math* —
  §1 mandates 3D-direction patterns and the pole check covers that by red-team, not grep.)
- **Flight `ctest` is the LAST gate step, always.** The compatibility spine — the map may not cost the
  kernel anything.
- **Mutation-verify** each objective test (inject the paired break, confirm red, revert — a test that
  can't fail is décor).
- **Separate-context red-team at each phase boundary** (never self-review). Attack the seam + render
  correctness + the shape of any generated data.
- **A moved golden/manifest halts the loop** — confirm intent, never re-baseline to pass.

## 5. Asset pipeline — how the art actually gets made (Fable 5)

- **Geometry: procedural / primitive-composed, in code.** A steeple is a box + pyramid; a water tower a
  cylinder on legs; a stadium a low ring. Coherent by construction, deterministic, config-tunable. A
  model-import pipeline is a trap (every asset re-opens scale/orientation/style). At most a dozen hand-built
  low-poly OBJs.
- **Textures: mostly procedural** (grain = shader hash; farmland = shader 3D-hash/Voronoi on `fragDir`).
  One authored candidate: a grayscale ground-detail tile set — grayscale means even AI output can't break
  palette.
- **Gemini's real role: concept + calibration, not an asset factory.** (1) The **style bible** (P1 entry
  gate above). (2) Later, **nose-art / livery decals** — small 2D insignia are the one place image-gen
  drops straight in. *Non-role:* anything 3D (image→mesh), global ground megatextures (re-imports the
  lat/lon temptation), in-loop gen.
- **Coherence enforcement:** one `world.toml` style block — one tone curve, one grain value, a few named
  gray levels, and the team chroma pairs as the **only** saturated entries in all of config. A color literal
  outside that block is a review flag.

## 6. Visionary moves — and the explicit do-NOT list

**Three bold moves that punch above their cost:**
- **Mirror-silver lakes** (§2). Sudbury's 300+ lakes rendered as smooth mirrors of the silver sky — near-
  black, flashing silver where they catch the sky/sun. The signature image of the whole world, AND 300
  orientation beacons (a lake flashes toward *up*), AND unmistakably "1940s B&W photograph." One water
  material, analytic reflection, no render-to-texture. This is the move that makes the world *this* place.
- **Bright newsreel SKY** (flat value in P1, per-pixel horizon glow in P4). Today the world reads as *a
  globe floating in space* (the "which way is the planet up" framing, and it costs horizon legibility at the
  screen edge). Film-white/silver + a horizon glow makes everywhere *air over a place* instead of *orbit
  over a ball* — the cheapest mood-and-orientation lever after dropping Earth, and it was on nobody's list.
- **White contrails + wing glint.** Monochrome contrail ribbons turn the dogfight's energy and history into
  geometry you read at a glance (where everyone is, was, how hard they turn) — free legibility against B&W
  ground. The specular sun-glint flash is the era-authentic "flash of a wing" spotting cue, and it fixes
  chroma-invisible-at-2 km with *brightness* instead of saturation.

**Do NOT** (each is a live temptation): **film flicker** (a full-screen luminance strobe is a
motion-sickness *amplifier* — the one newsreel affectation that fights the mission; default OFF), heavy
vignette (darkens the peripheral horizon reference), shadow mapping (zenith sun makes fake blob shadows
read fine), PBR / bloom / DoF / **motion blur** (actively degrades target tracking), the donor's day/night
terminator + naval ships (rejected realism), accurate street layouts (recognition ≈ 0 at 300 m/s —
silhouettes carry everything), general terrain collision/consequence (deferred — but NOT airstrip landing,
which is planned: build the `LandingSurface` rails per §1, just not the interaction logic here), and **any camera change
smuggled in as world work** (horizon recovery is its own gated thread).

## 7. Decisions & remaining opens

**RULED by Chad (2026-07-07) — folded into the plan above:**
- **The place:** Greater Sudbury, Ontario, arted up (§0 "The place"). The whole sphere is the region: the
  crater-rim hills as relief, 300+ lakes, named towns each with visible 3D landmarks (Superstack/"Big Smoke",
  Big Nickel, Chelmsford's French church, tailings ponds, Garson airport, every town's water tower). Ramsey
  Lake (Sudbury), Whitewater Lake (Azilda), Wanapitei Lake (by the airport). Whimsical, faithful to feel +
  landmarks, not GPS-accurate.
- **Sky:** Mercury silver (P1 step 1). **Grain:** high contrast, minimal grain — crisp, not gritty (§0.3).
- **Water:** mirror of the silver sky — near-black, silver where it catches the sky, smooth (§2, §6 bold #0).
- **Teams:** complementary duos, picked per match; large matches = up to four duos (8v8 split into
  orange+blue vs green+red) (§0.2).

**RULED by Chad (2026-07-07, on Fable's recommendation):**
1. **Heightfield + lake source → HAND-PAINT, traced over a GIS underlay** (not real CDEM baked). Real
   relief (~150–300 m) gets exaggerated to the ~600 m theatrical displacement anyway; real hydrography is
   illegible at the texel budget (sub-46 m ponds = speckle); and **no GIS export wraps a rectangular UTM
   region onto a closed sphere** — that projection warp is the most expensive, least-whimsical line in the
   whole pipeline. Hand-paint in the world's own space; keep GIS as a *tracing underlay* only, to preserve
   the real adjacency (Ramsey by downtown, the basin ring, Wanapitei by the airport).
2. **Region-to-sphere → WHOLE REGION at 1:1** (§0 arithmetic: ~0.9× linear, no enlargement). Scale the
   **furniture at ~5×** — the Superstack, water towers, church, headframes as bold, *generalized*
   silhouettes (read-from-altitude landmark shapes, not literal replicas) — floor the lake size, and do
   **NOT** scale the map.
3. **Ratify the Gemini style bible + the region graph** — folded into the P1 entry gate (§3).
