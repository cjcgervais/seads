# SCARF SPEC — the Sudburian's trailing scarf = the proto-SUPERMAN solver (R2c-7s)

**Authority chain:** `CLAUDE.md` three standing laws → `docs/RIDER_AUTHORITY.md`
→ `docs/SUDBURIAN_LADDER.md` §5 (THE SCARF) + §7.6 (superman IS the scarf) →
this file. Chad, 2026-08-19 (verbatim, going to bed): *"build the scarf which
will be the proto superman off the handlebars wiring that can co-exist [with the
helmet agent]. See the spec. Let's do a thorough and correct job on the scarf …
make this mechanism a valuable asset in the game … a complete committed and
shipped feature … also make it for seads-recon build-play."*

## 0. What the ladder says, and the one place this file departs from it

§5, verbatim: *"Six-segment bone chain `scarf_01..06`, skinned, **driven
entirely render-side**, carrying no authored keyframes. Trail direction from the
sled's velocity vector in body frame; lift angle rises monotonically with speed
as Chad specified; a light damped-spring lag per segment for the whip. It never
writes back to sim (§0.1), so it cannot touch a tape."*

§7.6: *"A damped trailing chain anchored at a fixed point is exactly the scarf.
Superman is that same solver with the anchor moved to the grips and the body as
the chain. Build it once in R2 for the scarf, re-use it in R4 for the body. Same
code, same fixed iteration count, same fixed dt. It is a trailing-chain solver,
NOT a ragdoll. Fixed segment count, fixed iterations, fixed dt, no contact
solver."*

**MEASURED out of `assets/sled/indy650.glb` (2026-08-19):** the six bones
`scarf_01..scarf_06` ARE in the shipped 42-joint `sudburian_rig` skin (skin
joint indices 8..13). `scarf_01` is a child of `neck_01` at local translation
`(0, −0.0029, 0.0750)` with rest rotation `(x,y,z,w) = (0, 0.1452, 0.9894, 0)`
(180° about an axis tipped 8.3° off the neck's +Z, i.e. the chain's +Y points
DOWN-and-BACK the neck); `scarf_02..06` are each `+0.080 m` along their
parent's +Y, identity rotation. **Zero vertices of `sudburian_proxy` weight to
any scarf joint** — the bones exist, the cloth does not.

**THE ONE DEPARTURE, stated as the spec demands:** §5 says "skinned". Authoring
a skinned scarf mesh requires the live Blender session, which tonight has ONE
writer — the helmet agent (`docs/HELMET_SPEC.md`, `winter-gi`) — and
`RIDER_AUTHORITY.md` forbids a second session on the file. So this rung:

1. **DRIVES THE SIX EXISTING BONES render-side from the solver** (their
   `world` transforms are overridden every frame, exactly as the IK overrides
   the hand/foot bones), so that the day a skinned scarf cloth is authored in
   Blender and spliced in, it skins onto these bones with **no code change**;
2. **DRAWS A PROCEDURAL RIBBON off those same bone frames** in C++ (a
   two-sided strip, 7 stations × 2 verts per side), so the scarf is VISIBLE
   and judged tonight.

That is the whole departure: mesh = procedural ribbon instead of authored
cloth, until the art rung. The solver, the bone driving and the wiring are
exactly §5/§7.6. **Open question for Chad (not blocking): the scarf's
COLOUR.** §4 never names one. Default shipped = the machine's own `indy_red`
base colour, read from the GLB material at load (the §4 "matched to the Indy
650, no retyped literal" rule) — it is the proto-Superman cape and it ties the
man to his machine against the grey suit and the orange/blue helmet.
`SEADS_SCARF_COLOR="r,g,b"` (bytes) A/Bs it without a rebuild.

## 1. Co-existence with the helmet agent (HARD)

- Work happens in a NEW worktree `D:\seads_sandboxes\scarf` on branch
  `sandbox/scarf` off `b071770ce` (R2c-M2 SIGNED). **Nothing in
  `D:\seads_sandboxes\winter-gi` is touched. Blender is never opened by this
  rung.** The GLB is not modified.
- Footprint in shared files is minimal and hunk-separable from the helmet
  agent's (their diff touches `SPrim::gloss`, the fragment shader, the
  material-name gate in `load_model`, the draw loop's uniform set):
  `render/sled_model.cpp` gets (a) a `TrailChain` member on `SledModel`,
  (b) a scarf bind/capture block at the END of `load_model()`, (c) one block
  in `sled_model_draw()` between `pose_and_solve_lean()` and the skinning
  loop, (d) the ribbon prim drawn in the draw loop via the existing prim path.
  `render/sled_model.h` adds three fields to `SledRig`. `render/draw.h` +
  `render/draw.cpp` + `app/main.cpp` add the three channel reads. Everything
  else is NEW files.
- Ship = commit on `sandbox/scarf`; merge into `main` and into `seads-recon`'s
  `sandbox/audio` (Chad's fly tree); `cmake --build build-play` there. The
  helmet agent merges `gi4-ride` on its own schedule; git will merge the two
  hunk sets. If a conflict appears in `sled_model.cpp` it is resolved by
  keeping BOTH.

## 2. Architecture (§0.5 layering, house law)

| file | layer | role |
|---|---|---|
| `render/trail_chain.h/.cpp` | `seads_render_core` (PURE: glm+std, ZERO raylib, no clock, no env) | the solver. The thing R4 reuses for superman. |
| `render/sled_model.cpp` | `seads` exe (raylib) | anchor capture, per-frame step, bone override, ribbon build + draw, env dials |
| `render/sled_model.h` | | `SledRig` gains `vel_body_mps`, `ticks`, `dt_s` |
| `render/draw.h/.cpp`, `app/main.cpp` | | `info.sled_vel_ms` (world, from `SledState::velocity`), `info.sled_ticks = fr.ticks`, `info.sled_dt = params.sim_dt`; `draw.cpp` turns them into the body-frame velocity via `info.sled_basis` and fills the rig |
| `test/unit/test_trail_chain.cpp` | `seads_tests` | the gate (§5) |
| `docs/SCARF_SPEC.md` (this), ladder §5 amendment, handoff | | |

**Render reads NO clock (house law).** The scarf advances `rig.ticks`
sub-steps of `rig.dt_s` per frame — TICK-derived like `player_wheel_roll_rad`
and `t_cel`. In `--smoke` the tick count per frame is fixed, so smoke shots
are reproducible. Sub-steps per frame are capped at `kScarfMaxSubsteps = 16`
(a stall never buys a 600-step frame; the cap drops time, it never stretches
dt). **It never writes to `sim/`** — the tape firewall is untouched by
construction: `trail_chain.*` takes `const` inputs and owns its own state
inside `SledModel` (render static), nothing in `sim/` or `app/` reads it.

## 3. The solver — `render/trail_chain.h`

```cpp
namespace render {
inline constexpr int kTrailChainMaxSegments = 16;   // superman (legs+torso) fits
inline constexpr int kScarfSegments = 6;            // §5

struct TrailChainParams {
    int   segments     = kScarfSegments;
    float seg_len_m    = 0.080f;    // MEASURED (scarf_02..06 local +Y); overwritten at bind
    float dt_s         = 1.0f/120;  // FIXED sub-step; caller supplies the same value every call
    int   iterations   = 4;         // FIXED constraint passes per sub-step (§7.6). MEASURED: with a
                                    // pinned root and child-only projection ONE root->tail pass already
                                    // satisfies every distance constraint exactly (1 vs 30 passes:
                                    // bit-identical trajectories); the extra passes only arbitrate the
                                    // keep-outs against the lengths. 4 is plenty and still constant work.
    float gravity_mps2 = 9.81f;
    // aerodynamics: dv = w_rel * min(1, drag_k_per_m * |w_rel| * dt)  -- quadratic
    // drag on a light ribbon, written as a velocity relaxation toward the
    // relative wind so it is UNCONDITIONALLY STABLE for any |w| and any dt.
    // Steady hang angle from the down direction: tan(theta) = drag_k * v^2 / g.
    float drag_k_per_m = 0.153f;    // 45 deg at 8.0 m/s; 80 deg at 20; 8 deg at 3.
                                    // MEASURED on the 6-link prototype: 0/1.0/10.8/29.4/45.0/
                                    // 66.0/75.9/80.9/84.1 deg at 0/2/4/6/8/12/16/20/25 m/s -- the chain
                                    // follows the single-particle law tan(theta)=k v^2/g to 0.1 deg from
                                    // 6 m/s up (a massless chain in uniform wind is straight); below
                                    // 6 m/s the back keep-out plane holds it a few degrees under the law.
    float damping      = 0.960f;    // Verlet velocity retention per sub-step (the "light damped-spring lag").
                                    // MEASURED on the prototype (speed step from hang, settle = last time
                                    // outside +-2 deg of final): 0.990 left a released chain swinging ~20 s;
                                    // 0.975 -> 4.2/3.8/1.9 s at 4/8/20 m/s with peaks 36/100/162 deg;
                                    // 0.960 -> ~2.7/2.6/1.6 s, peaks ~24/79/154 deg (SHIPPED: a scarf, not a
                                    // rope, and the whip on a gust still reads); 0.950 -> 1.6/2.1/1.4 s.
                                    // Steady-state lift is unaffected (v = 0 there).
    // (v1 carried a forced 'flutter' wave here. DROPPED at the red-team: it is not in §5,
    //  it sat on a resonance cliff, and the anchor-driven whip (bumps, absorb, speed
    //  changes) supplies the life. The prototype numbers are kept in git history.)
    // keep-outs (no contact solver: two analytic projections, applied inside the
    // constraint loop after the distance pass, every iteration)
    float head_keepout_r_m   = 0.20f;   // helmet sphere; 0 disables (R4 superman). Per step the
                                        // EFFECTIVE radius is min(r, |anchor - centre| - 0.02) so the
                                        // pinned root can never be inside its own keep-out (red-team P2-1)
    float back_keepout_m     = 0.06f;   // half-space behind the TORSO plane (see §3b); back_normal = 0 disables
};

struct TrailChainState {
    int n = 0;                                   // segments in use
    glm::vec3 p[kTrailChainMaxSegments + 1];     // p[0] = anchor
    glm::vec3 p_prev[kTrailChainMaxSegments + 1];
    glm::vec3 last_x{1,0,0};                     // last good ribbon width axis (frames fallback)
    bool primed = false;
};

// per-step inputs, all in ONE frame (the caller's: model space for the sled)
struct TrailChainInput {
    glm::vec3 anchor;          // p[0] this step
    glm::vec3 wind_mps;        // air velocity relative to the frame (= -velocity)
    glm::vec3 gravity_dir;     // unit, points DOWN in this frame
    glm::vec3 width_axis;      // unit; the ribbon's rest width direction at the anchor
    glm::vec3 head_center;     // keep-out sphere centre
    glm::vec3 back_origin;     // a point on the torso line (the neck joint)
    glm::vec3 back_normal;     // unit, points BACKWARD, PERPENDICULAR TO THE POSED TORSO AXIS (§3b)
};

// Straight chain from anchor along `dir` (unit), at rest. Used at first sight
// and after a teleport (anchor jump > 1 m in one step).
void trail_chain_reset(TrailChainState&, const TrailChainParams&, const glm::vec3& anchor, const glm::vec3& dir);

// ONE fixed sub-step: Verlet predict (gravity + drag relaxation +
// damping), then `iterations` passes of {distance constraints root->tail,
// head-sphere projection, back half-space projection}, p[0] pinned to anchor.
// Constant work. No tolerance loop. No NaN for any finite input (zero wind,
// zero dt, 1e3 m/s wind, anchor teleport).
void trail_chain_step(TrailChainState&, const TrailChainParams&, const TrailChainInput&);

// Bone frames for the chain: frame i (i in [0,n)) has origin p[i], +Y along
// normalize(p[i+1]-p[i]), and its +X/+Z from the input width axis parallel-
// transported down the chain (Gram-Schmidt against +Y, each step seeded by the
// previous frame's X). Orthonormal, right-handed, continuous.
void trail_chain_frames(const TrailChainState&, const glm::vec3& width_axis, glm::mat4* out /*n*/);

// The "lift angle" measurement the tests gate: angle (rad) between the chord
// (p[n]-p[0]) and the DOWN direction. 0 = hanging, pi/2 = streaming level.
float trail_chain_lift_rad(const TrailChainState&, const glm::vec3& gravity_dir);
}
```

Algorithm of `trail_chain_step` (write it exactly; it is the reusable core):

```
if (!primed || |anchor - p[0]| > 1.0) reset(anchor, gravity_dir)   // first sight / teleport
p[0] = anchor                                                       // pin
for i in 1..n:
    v   = (p[i] - p_prev[i]) / dt                                   // dt>0 guaranteed by caller; if dt<=0 return after pin
    w   = wind - v                                                  // relative wind at the particle
    v  += w * min(1, drag_k*|w|*dt)                                 // quadratic drag, stable
    v  += gravity_dir * g * dt
    v  *= damping
    p_prev[i] = p[i];  p[i] += v*dt
for it in 0..iterations:
    for i in 1..n:                                                  // root -> tail, anchor infinitely heavy
        d = p[i]-p[i-1]; L=|d|; if L>1e-6: p[i] = p[i-1] + d*(seg_len/L)
    r_eff = min(r, |anchor - c| - 0.02); if r_eff > 0: for i in 1..n: if |p-c|<r_eff: p = c + normalize(p-c)*r_eff  (|p-c|<1e-6 guard: use back_normal)
    if |back_normal| > 0: for i in 1..n: s = dot(p - back_origin, back_normal); if s < back_keepout: p += back_normal*(back_keepout - s)
```
`trail_chain_frames`: if `!primed` the caller must have reset first (sled_model
primes at BIND along the rest chain direction, so a 0-tick first frame never
sees an unprimed chain). Width axis: X_i = input width axis for i=0, then the
previous frame's X, Gram-Schmidt'd against Y_i; if |X_perp| < 1e-3 (the chain
streaming along the width axis -- a hard side slide) fall back to
`state.last_x`, then to cross(Y, (0,1,0)), then (1,0,0); store the result in
`state.last_x` (frames() therefore takes the state by non-const ref, or the
caller passes last_x in/out -- builder's call, documented). A segment shorter
than 1e-6 reuses the previous frame's Y. Frames are ALWAYS finite and
orthonormal (tested).

### 3b. THE TORSO PLANE (red-team P1-1 -- the posed torso leans 33 deg FORWARD)

MEASURED in the shipped rest: pelvis (y 0.684, z -0.585) -> neck (1.121,
-0.297): the spine climbs forward at 33 deg from vertical. A scarf hanging
vertically from the anchor (1.131, -0.371) is therefore 3 cm in FRONT of
spine_03 and 19 cm in front of spine_01 -- through the chest. A plane that is
merely "vertical through the neck" cannot stop that. So the back half-space
is built from the POSED torso every frame, in model space:

```
axis   = normalize(neck_pos - pelvis_pos)                    // posed, this frame
b      = anchor_pos - neck_pos;  b -= axis*dot(b,axis)        // the anchor's offset perpendicular to the torso
back_normal = normalize(b)                                    // derived: "back" is where the rig put the scarf knot
back_origin = neck_pos
```
With the torso tilted forward, a vertical hang from the anchor falls further
BEHIND this plane the lower it goes, so the idle scarf lies along the back and
clears the chest by construction. Test 7 runs with the torso axis tilted 33 deg
forward and asserts every particle stays >= back_keepout - 1e-4 behind the
tilted plane.
Every loop bound is a compile-time or params constant. **No early exit on
convergence.**

## 4. The wiring — `render/sled_model.cpp` (the seads exe)

**4.1 Bind/capture at the end of `load_model()`** (after the catalogue bind):
- `sm.scarf_node[6]` = `find_node("scarf_01".."scarf_06")`; ALL six must be
  present or the scarf is disabled with ONE `TraceLog(LOG_WARNING)` naming the
  missing bone (the machine still draws — a missing scarf is not a crippled
  rider, unlike a missing arm). `sm.n_neck = find_node("neck_01")`.
- `seg_len_m` := measured `|nodes[scarf_0k].t|` mean for k=2..6 (0.080).
- `scarf_anchor_local` := `nodes[scarf_01].local` (the rest TRS as a mat4 in
  the neck's frame) — anchor position AND the rest chain direction/width axis
  come from it: `dir_rest = R * (0,1,0)`, `width_rest = R * (1,0,0)` (bone X).
- `back_n_local` := normalize(translation of scarf_01 local) — "backward" is
  DERIVED as the direction from the neck joint to the scarf root, never a
  hard-coded axis (the rig's own authoring says where the back is).
- head keep-out centre = `head` joint world + 0.10 m along the head bone's +Y
  (derived per frame from `nodes[n_head].world`); r = `head_keepout_r_m`.
- Colour: `indy_red` (EXACT name -- `indy_red.001` also exists) is captured
  in its OWN small block placed between the prim loop's closing brace and
  `cgltf_free(data)` (>= 20 lines clear of the helmet agent's material hunk;
  `data->materials` is gone after `cgltf_free`, so it cannot live "at the
  end"). Shipped bytes are (114, 5, 5); fallback if absent `{114, 5, 5}`
  logged once. `SEADS_SCARF_COLOR="r,g,b"` overrides.
- PRIME the chain at bind: `trail_chain_reset(anchor_rest, dir_rest)` so a
  0-tick first frame draws a resting scarf, never an unprimed one.
- Build the ribbon prim: a DYNAMIC `Mesh` (`UploadMesh(&m, true)`),
  `(kScarfSegments+1)` stations × 2 verts × 2 sides = 28 verts, 24 triangles
  (both windings so it draws two-sided without touching global cull state),
  half-width `kScarfHalfWidthM = 0.055`. **NOT an `SPrim` in `sm.prims`** (the
  prim loop indexes `sm.nodes[sp.node]` and the skinning loop indexes `jw` --
  a node=-1 prim is UB, and patching that line collides with the helmet
  agent's hunk): it is its own `Mesh sm.scarf_mesh` + `bool sm.scarf_ok` +
  `Color sm.scarf_col`, drawn in its OWN block AFTER the prim loop and BEFORE
  the beam block -- set `uColor`, `uEmitColor`, `uEmit = 0` (and ANY other
  per-prim uniform the shader has at merge time, e.g. the helmet's `uGloss` =
  0, since uniforms persist from the last prim) then `DrawMesh(scarf_mesh,
  sm.mat, to_ray_m(mount))`. `SledModel` fields go next to `hinge{}`, not
  next to `loc_*` (the helmet hunk).

**4.2 Per frame in `sled_model_draw()`, AFTER `pose_and_solve_lean()` and
BEFORE the skinning loop:**
```
if (sm.scarf_ok && !scarf_off) {
    // frames: model space. body->model is R_y(180) (the mount's last factor):
    // body (x,y,z) -> model (-x, y, -z).
    wind_model  = -(R_y180^T * rig.vel_body_mps)
    g_dir_model = R_y180^T * basis^T * (-normalize(pos))     // local down, sphere law: from normalize(position), this frame
    neck_w = nodes[n_neck].world;  anchor_w = neck_w * scarf_anchor_local
    in.anchor = pos(anchor_w); in.width_axis = normalize(mat3(anchor_w)*X); ...
    in.back_origin = pos(neck_w); in.back_normal = section 3b (posed pelvis->neck axis, anchor offset perpendicular to it)
    in.head_center = pos(nodes[n_head].world) + 0.10*normalize(mat3(head.world)*Y)
    steps = min(rig.ticks, kScarfMaxSubsteps); params.dt_s = rig.dt_s
    for s in 0..steps: trail_chain_step(sm.scarf, params, in)
    trail_chain_frames(sm.scarf, in.width_axis, frames)
    for k in 0..6: nodes[scarf_node[k]].world = frames[k]; world_override = true   // a future skinned cloth rides these
    // ribbon: station k at p[k], width along frame k's X (station 6 uses frame 5's X); normals = cross(X, tangent); second side flipped
    rebuild vertices+normals; UpdateMeshBuffer(0 and 2)
}
```
The ribbon is drawn in its own block after the prim loop with `xf = mount`
(its verts are model-space like the skinned prims), `emit = 0`, colour
`scarf_col` (section 4.1).
`world_override` on the scarf bones is set AFTER the pose passes, so the
settle pass inside `pose_pass` never touches them; the next frame's pose_pass
recomputes them from the neck first (harmless) and the block above overrides
again.

**4.3 Env dials (one `getenv` each, read once, the house pattern):**
`SEADS_SCARF=0` off (the machine draws bit-identically to pre-scarf);
`SEADS_SCARF_DEBUG=1` → `TraceLog` per frame: `|v|`, lift angle (deg), chord
azimuth in body frame (deg), tail position, min head-sphere clearance, min
back-plane clearance, the sub-step count;
`SEADS_SCARF_VEL="vx,vy,vz"` (a VELOCITY, body frame, m/s) → overrides `rig.vel_body_mps` in `draw.cpp`'s
smoke block (mirrors `SEADS_SLED_RIG_SMOKE`), so a `--smoke` shot can certify
the scarf at 0 / 8 / 20 m/s and in a sideslip without driving there;
`SEADS_SCARF_COLOR` (above).

**4.4 Body frame / sign law (SPEC §7):** body +X right, +Y up, −Z forward.
Forward travel at V ⇒ `vel_body = (0,0,−V)` ⇒ relative wind `(0,0,+V)` in body
⇒ model `(0,0,−V)`… NO — compute it, never write it down: `wind_model =
−R_y180^T·vel_body`. The TEST for the sign is physical: at forward travel the
tail must end up BEHIND the rider (model −Z of the neck, the side the anchor is
on), at a left slide it must trail to the rider's right. Both are in §5 tests
and in the smoke shots.

## 5. The gate — `test/unit/test_trail_chain.cpp` (Catch2, ASCII names only)

Each test feeds the pure solver directly (no GLB). `dt = 1/120`, settle = 2400
sub-steps (20 s) unless stated (the chain takes ~3 s to settle; 20 s is margin, not a claim). `g_dir = (0,−1,0)`, anchor at origin, width
axis `(1,0,0)`, back_normal `(0,0,−1)` with back_origin `(0,0,0.05)` (anchor 50
mm behind the "neck"), head sphere at `(0,0.17,0.10)` r 0.18 (the ANCHOR is 17 mm OUTSIDE it -- the
real rig's margin is 12 mm; the effective-radius rule makes both safe).

1. **determinism** — two states stepped through the SAME 300-step input
   sequence (wind ramps 0→20 m/s with a 30° yaw sweep) are BIT-IDENTICAL
   (compare p[0..n] and p_prev[0..n] component-wise with `==`; the state
   arrays are value-initialised `{}` so the unused tail is not garbage).
2. **invariants** — after every step of that sequence: `p[0] == anchor`
   exactly; every `|p[i]-p[i-1]|` within 1e-4 of seg_len; no NaN/Inf.
3. **hang** — zero wind, settle: lift angle < 1°, chord length within 1 mm of
   6·seg_len, tail directly below the anchor (|x|,|z| < 2 mm).
4. **lift is monotone in speed** — forward wind at v ∈
   {0,2,4,6,8,12,16,20,25} m/s, settle each from hang (1200 steps): the lift
   angle is STRICTLY increasing; θ(8) ∈ [40°,50°]; θ(20) > 75°; θ(2) < 10°.
   (The drag/g arithmetic of §3 is the prediction; the prototype measured
   45.0 / 80.9 / 1.0; the test measures the solver.)
5. **direction follows the wind** — wind from the rider's left (body +X
   relative wind) at 12 m/s: the chord's horizontal direction is within 10° of
   the wind direction. Then swap to the right: it follows (within 10°) inside
   1.5 s.
6. **keep-out: head** — wind blowing the chain TOWARD the head sphere (+Z
   model) at 15 m/s, 600 steps: no particle ever inside the sphere by more than
   1e-4 after a step; the chain still has full chord length ≥ 4·seg_len (it
   wraps, it does not collapse).
7. **keep-out: back (the tilted torso)** (threshold corrected in v3, see §8b P2-3: the hang lies ALONG the 33° back, so the gate is 25°..35°) -- torso axis tilted 33 deg forward:
   neck at the origin, pelvis at (0, -0.52, +0.34) (the spine climbs forward,
   so the axis UP the spine is normalize((0, 0.52, -0.34))); anchor 75 mm
   behind the neck perpendicular to the axis; back_normal per section 3b;
   keep-out 0.06. Zero-wind settle, then 6 m/s forward wind, then 6 m/s wind
   TOWARD the chest: for all i, all steps `dot(p_i - back_origin,
   back_normal) >= back_keepout - 1e-4`; and at zero wind the chain still
   hangs (lift < 25 deg: it lies on the back, it is not pushed into the air).
8. **stability** — wind 1000 m/s for 300 steps: finite, lengths hold; then
   wind 0: returns to hang (lift < 2°) within 20 s (prototype: < 3 s at
   damping 0.96). dt = 0 call: no change, no NaN. Anchor teleport by 3 m:
   re-primed straight along gravity, no NaN. `head_keepout_r_m = 0` and
   `back_normal = 0`: both projections are no-ops (the R4 superman
   configuration), still finite.
9. **frames** — after settle at 12 m/s: each frame orthonormal (|det−1|<1e-4,
    columns unit, dot < 1e-4), +Y along the segment, successive X axes continue
    (dot > 0.9), right-handed.
9b. **frames reproduce the GLB rest bone frames** (the ONLY thing that
    protects a future skinned cloth from a flipped X/Z): feed p[k] = the
    measured rest chain -- scarf_01 at model (0, 1.1308, -0.3712), each next
    station +0.08 along Y = (0, -0.893, -0.449), width axis (1,0,0): every
    frame's columns are X=(1,0,0), Y as given, Z=(0, 0.449, -0.893) within
    1e-3 (Z = cross(X,Y), right-handed, Y = the glTF bone axis). THE BUILDER
    VERIFIES these three vectors against the live GLB's scarf_01 rest WORLD
    rotation (python, stdlib) before writing the test, and quotes them.
9c. **frames are finite when the chain streams ALONG the width axis** -- wind
    (25,0,0) settle: every frame finite and orthonormal; width falls back per
    section 3 (no NaN, no zero column).
10. **whip lag** — step the wind 0→15 m/s: the TAIL's lift
    angle lags the ROOT segment's by ≥ 30 ms and ≤ 600 ms to reach 63 % of
    final (prototype: root 67 ms, tail 142 ms, lag 75 ms — the "light
    damped-spring lag per segment" is measured, not asserted).
11. **cost** -- NO timing test (nondeterministic under a loaded box, red-team
    P1-6). Constant work is true by construction -- no loop bound depends on
    data, no early exit -- and is enforced by code review + a comment block at
    the top of `trail_chain_step` that says so.

Also: `[rider_rig]`, `[rider_pose]`, `test_sled_tape`, the whole suite: the
pass bar is the SAME pass/fail set as the base commit `b071770ce` (1237/1241 —
the 4 `test_sled` GI4 debts are PRE-EXISTING and must be identical by name).
`tools/graph/graphify.py` regenerated in the same commit;
`graph_query.py check` green; layer gate green (the solver has NO raylib).

## 6. Evidence (no ctest runs the draw TU — the house pattern is measured smoke)

`build-play` shots, `SEADS_SLED_DEBUG_MODE=1 SEADS_SLEDCAM="2.2,35,12"`,
`SEADS_SCARF_DEBUG=1`, **>= 900 frames** (smoke = 1 tick/frame, drive arms at
tick 30, the chain settles in ~3 s = 360 ticks; 60 frames would photograph
the transient -- red-team P1-3). Quote the frame count on every shot:
- `SEADS_SCARF_VEL="0,0,0"` → hang down the back, off the body;
- `"0,0,-8"` (8 m/s forward) → lifted ~45°, behind;
- `"0,0,-20"` → streaming level behind;
- `"-6,0,-12"` (sliding with the left side leading: velocity has a −X
  component… CHECK THE SIGN IN THE LOG, not the doc) → trails to the side;
- `SEADS_SCARF=0` → the pre-scarf picture (byte-compare against a baseline shot
  from the base commit: identical).
The debug lines' lift angles are quoted in the commit message and the handoff.
A contact sheet of the five shots is the artefact Chad judges.

## 7. Out of scope tonight (named so nobody mistakes silence for a claim)

Authored cloth mesh + skin weights (art rung; the bones are ready). Roost/spray
reaction (§5 "open detail", deferred by the ladder itself). The superman
re-anchoring (R4 -- this is the solver it will call; keep-outs disable with
r = 0 / back_normal = 0). Wind from world weather. Self-collision. A forced
flutter (dropped, see section 3). omega x r wind (no extra swing during a
roll/360 -- named, accepted). Colour ruling (default `indy_red`, question
logged). **Determinism claim, precisely:** the solver is bit-deterministic for
a given input sequence; in the game the anchor/velocity are FRAME-sampled while
sub-steps are TICK-counted, so two runs with different frame partitions
diverge visually -- true in `--smoke` (1 tick/frame), never claimed for a tape
replay (the scarf is not on any tape and never will be).

## 8b. Red-team fold of the BUILT ARTEFACT (v3, 2026-08-19, after the build)

A second context-free Fable red-team verified the built diff, re-measured the
GLB, simulated the 3-way merge against the helmet agent's tree and shot 13
extra frames. Verdict SHIP-WITH-FIXES; every fix is in:

- **P1-1 the knot bone is INSIDE the suit** (7 mm: the skinned-rest proxy's
  back lies 0.0747 m behind the §3b plane, the knot 0.0677) and the typed
  0.06 keep-out let the ribbon z-fight along the back and emerge from the
  shoulder blades. FIX (built): at bind the proxy is rest-skinned with
  `rest_world * ibm` (the raw mesh space carries the R2b yaw and reads 0.44 m
  off), the farthest torso-band vertex behind the plane is MEASURED, the
  keep-out becomes surface + 12 mm (0.0867), and the solver anchor is stood
  off the bone by the difference (19 mm) so the root sits ON the back. Logged
  at bind. Supersedes §3's typed `back_keepout_m = 0.06` (now the fallback
  when no skinned vertices exist).
- **P1-2 a zero-thickness strip is a hairline from the side.** FIX (built):
  a second perpendicular strip along the frames' Z (half-width 0.035, the
  scarf's fold/edge) — a "+" cross-section, 56 verts / 48 tris; the side view
  now reads; `SEADS_SCARF_CROSS=0` collapses it for an A/B. An authored cloth
  retires both strips. Chad's call on the look.
- **P2-1 merge with the helmet hunk is clean** (`git merge-file` exit 0) but
  the ribbon draw block must ALSO set the helmet's `uGloss = 0` at merge time
  (uniforms persist from the last prim). One line; in the handoff.
- **P2-2 two solver additions not in §3** (now in §3 by this note): (a) a
  re-prime is SOLVED then FROZEN (`needs_solve`: constraints applied, `p_prev
  = p`, no integration) — a straight-down prime started 248 mm inside the
  torso plane and Verlet read that as 30 m/s, parking the chain straight UP
  the spine at 145° (the plane's inverted fixed point, unstable, reachable
  only by exact symmetry); (b) the effective-size clamp applies to the plane
  too (`back_d = min(keepout, s_anchor)`), a no-op on the shipped rig.
- **P2-3 test 7's "lift < 25°" is arithmetically impossible** — the drape
  angle IS the torso tilt (33°); the test gates 25°..35° (measured 31.4°; in
  game 31.1°). §5.7 above reads accordingly. Also §1(d) "drawn via the
  existing prim path" is SUPERSEDED by §4.1 (own draw block); §4.1's
  `back_n_local` is superseded by §3b; `info.sled_dt` was built as
  `sled_dt_s`; `trail_chain_frames` takes the state by non-const ref.
- **P2-4** a 0-tick first frame draws the rest-primed chain one frame before
  the posed neck — invisible, accepted.

## 8. Red-team fold (v2, 2026-08-19)

A context-free Fable red-team re-derived v1 from the artefacts and returned
BUILD-WITH-FIXES. Every finding is folded above: P0-1 ribbon out of `sm.prims`
(UB + helmet-hunk collision); P0-2 damping re-measured (0.990 -> 0.960, settle
windows re-derived, tests settle 2400 steps); P1-1 the torso plane (3b);
P1-2 frames fallback + tests 9b/9c; P1-3 smoke >= 900 frames; P1-4 material
capture before `cgltf_free`; P1-5 value-initialised state; P1-6 timing test
dropped; P2-1 effective head radius; P2-2 flutter dropped; P2-3
`SEADS_SCARF_VEL`; P2-4 prime at bind, keep-out disables; P2-5 determinism
stated precisely; P2-6 the missing tests added.

## 8c. Chad's first drive (2026-08-19) — rulings, all BUILT

Chad drove the shipped R2c-7s and ruled: *"scarf takes too long to settle;
needs a SECOND END that hangs back, not as long as the longer one; the long
one is a bit too long and a bit too narrow; the colour must be verified
INDEPENDENTLY — is it our thematic code?"*

| ruling | built as | measured |
|---|---|---|
| settle faster | `damping` 0.960 → **0.930** (trail_chain.h) | 4/8/20 m/s step settle 2.45/2.62/1.69 s → 0.93/1.40/1.23 s; 8 m/s whip peak 78° → 56° (still a whip) |
| wider | half-width 0.055 → **0.070** (0.14 m face), cross 0.035 → **0.045** | — |
| shorter | `kScarfLengthScale` **0.85** on the GLB's measured 0.080 m spacing: 0.48 → **0.41 m**, still six bones | lift law unchanged (massless chain): 31.1/43.8/78.7° at 0/8/20 m/s, identical to R2c-7s |
| second end | a SECOND `TrailChainState` (`scarf_short`, **4 links = 0.27 m**) off the same knot, same params, its own ribbon, **no bones** (the rig has one scarf chain — a skinned short end is art-rung work); ends split ±0.030 m along the knot's width axis, the short end 0.010 m further off the back (no z-fight at rest) | both ends read in the rear-quarter and side shots at 0/8/20 m/s |
| colour | **INDEPENDENTLY VERIFIED: the shipped `indy_red` (114,5,5) is NOT a thematic code.** `render/team_color.h` holds exactly three sanctioned hues — `kSlagOrange` (enemy / lava / gauges), `kComplementBlue` (ally / the suit's stripes), medical-oxygen green (objectives). `indy_red` is the machine's own body-panel material read from the GLB — it ties the man to his machine, but it is NOT "our thematic code". **OPEN: Chad picks** — keep `indy_red`, or go thematic (`kComplementBlue`, the suit's own stripe blue; or `kSlagOrange`). Either thematic pick must be wired through `team_color.h`, never retyped. `SEADS_SCARF_COLOR` still A/Bs live: blue ≈ `"31,145,255"`, slag ≈ `"255,140,31"` (byte-rounded; the real wiring reads the constants) |

Gate after: `trail_chain` green with the new damping (the settle windows were
already 2400-step margins); full suite 1249/1253, the same four GI4 debts.

**§8c colour — RULED 2026-08-19 (second drive): `SEADS_SCARF_COLOR="255,140,31"`
= the SLAG ORANGE.** Shipped default is now `slag_color()` = `kSlagOrange` from
`render/team_color.h` (never retyped); the `indy_red` GLB capture is gone. The
env A/B stays. Colour question CLOSED.

## 8d. Chad's third drive (2026-08-20) -- THE AUTHORED SCARF, all rulings BUILT

His words: *"there are 3 tails; you should not work headlessly; use graphify
for everything; don't ever do mesh work without Blender open; the scarf looks
like it's choking the Sudburian -- I don't see the part around the neck under
the helmet. It should look wrapped covering the neck, bunching a bit at the
back and coming to a knot: 2 loops thick, knot at the FRONT, two tails, one
over each shoulder -- a little shorty over the left, about 12 inches, the
other over the right running halfway down the back. Fluffy and soft and
thicker. Make it look nice. Go with our thematic queryable blue, the exact
opposite of the slag orange -- independently verify it."*

| ruling | built as |
|---|---|
| 3 tails = bug | root cause: the "+" cross strips read as extra tails from the rear quarter. The PROCEDURAL RIBBONS ARE RETIRED entirely |
| authored, in Blender, visible | `scarf_geom.py` (stdlib, ONE source) + `apply_scarf_live.py` (APPEND-ONLY to the live GUI session, helmet-rung pattern: body verts proven bit-untouched, idempotent by material, never saves) + `patch_scarf.py` (writes the SAME geometry into indy650.glb as a skinned primitive; frame map PROVEN against the file's own IBMs; per-piece census: closed, 0 unbalanced edges, volume > 0; read-back worst 5.9e-8 m). A fresh SPLICE was rejected: the GLB carries the helmet prims the live proxy mesh does not -- a splice ships a helmetless rider |
| the wrap | two lumpy torus loops about the measured neck (r 0.080, z 1.513..1.609 bind), minor 0.034/0.031, BUNCHED at the back (+45% swell), under the shrunk head box (top clears it by 12 mm); rigid to neck_01 |
| the knot | front (+y bind), lumpy two-lobe ball r 0.047, under the chin |
| short tail | over the LEFT shoulder, 0.305 m ("about 12 inches"), fluffy flattened tube, rigid to neck_01 (STATIC -- flagged: bones for it are a future rung if Chad wants it alive) |
| long tail | knot -> over the RIGHT shoulder (rigid bridge) -> ONE continuous tube down the chain, each RING rigid to its arc-station bone scarf_01..06. Authored SEG_RUN=0.048 m per bone against the 0.080 m rest spacing: the GLB rest shows pod gaps, the RUN-TIME chain steps exactly 0.048 (kScarfRunSegFrac 0.60 = scarf_geom SEG_FRAC, one ruling two readers) and every boundary ring lands ON its chain point -- watertight bent or straight. 6 x 0.048 = 0.288 m = halfway down the back. First cut used six capped pods and read as BEADS; the one-tube rebuild killed that |
| fluffy/soft/thicker | deterministic sine fluff on every surface, smooth shading, tail cross 0.108 x 0.054 m, taper to 0.30, wave 5.5% -- roughness 0.92 wool |
| colour | **kComplementBlue, INDEPENDENTLY RE-VERIFIED 2026-08-20**: fresh HSV arithmetic (slag (1.00,0.55,0.12) -> H 29.3181818 +180 -> (0.12,0.57,1.00) EXACT) AND the executable invariant (tests 678-682 green, incl. the channel-reversal rejection). PARSED from team_color.h by scarf_geom.py into the GLB material `sudburian_scarf_blue` -- never retyped. Engine reads it back like any material; `SEADS_SCARF_COLOR` A/B now overrides BY MATERIAL NAME at load |
| code | sled_model.cpp: ribbons/second-chain/colour-capture DELETED; the whole render job is driving the six bones (world_override) -- the skinned path draws the scarf. `SEADS_SCARF=0` now freezes the chain at the posed rest (the authored scarf stays visible, undriven) |

Gate 1249/1253 (same four GI4 debts). Live session: reopened by the agent
after the prior session died mid-run (helmet object confirmed intact on
disk); .blend NOT saved -- the GLB is the shipping artefact; viewport hide
flags in the open session are not authoritative.

## 8e. Chad's fourth drive (2026-08-20 pm) -- all five rulings BUILT

His words: *"only the longer tail is moving -- the shorter one can move a
little bit but more simply; the scarf doesn't rest ON the back but INSIDE it;
there are like 3 wraps; the longest tail is too short -- but great work"* and
*"the wrap should be a FLAT WRAPPED look, not a tube; the tails a FLAT LENGTH
OF FABRIC -- fluffy meant thicker and soft textured, NOT tubular."*

| ruling | built as |
|---|---|
| short tail moves, simply | TWO new deform joints `scarf_s01/s02` under neck_01 (patch_scarf.py appends nodes + extends the skin + a fresh 44-IBM accessor; apply_scarf_live.py adds the same edit bones to the live rig 42 -> 44). A 2-link chain drives them: same drag/keep-outs, damping 0.90 (dies fast = "a little"). Missing bones (an older GLB) = rigid + a WARN. The rig-count pins moved 42 -> 44 (test_rider_rig x2, RIDER_AUTHORITY amended); LEGACY is still 19 |
| rests INSIDE the back | the chain is the tube CENTERLINE and the keep-out held it at surface+12 mm with 27 mm of half-thickness -- the inner half sat in the suit. Keep-out (and the anchor stand-off with it; the effective-plane rule would neuter a deeper plane alone) now = surface + margin + kScarfTubeHalfM; the bridges overshoot the anchor to cover the root gap |
| "like 3 wraps" | the fat drape starts read as a third wrap: bridges now leave the knot SLIM (52% width, swelling over the shoulder) and lower; the two loops separated further apart |
| long tail too short | kScarfRunSegFrac 0.60 -> 0.85: hang 0.288 -> 0.408 m (between his "too long" 0.48 and "too short" 0.288) |
| FLAT, not tubular | every cross-section is a flat band: wrap loops = tall shallow bands hugging the neck (0.031 thick x 0.082 tall, gathered at the back), tails = fabric strips 0.112 x 0.030 with a soft authored TWIST (a dead-flat strip is a hairline edge-on -- the twist keeps the face reading from every angle); knot flattened; the surface fluff carries "soft", not roundness |

Gate 1365/1369 (same four GI4 debts). GLB patched from the pristine helmet-v9
file each iteration (the s-joint guard refuses a double-patch).

## 8f. Chad's fifth drive (2026-08-20 evening) -- SIMPLE, all rulings BUILT

His words: *"it seems like you made the Sudburian transparent and the scarf is
still inside the model. Too much scarf noise -- redesign a SIMPLE look: just a
broad slightly compressed wrap around the neck and two tails, no knot, would
that be easier. No taper -- it's not a real tail, I mean the long flat free
ends of the scarf. The shorter end resting and hanging off the left shoulder,
1/3 the length of the longer flat length; on the right the full length."*

| ruling | built as |
|---|---|
| "transparent / inside the model" | ROOT CAUSE: the fifth build's wrap spanned z 1.48..1.60 -- its lower half sat BELOW the collar line (chest box top 1.5129) inside the torso, and the z-fighting patches read as transparency. The band now spans 1.521..1.605, entirely above the collar |
| simple, no knot, less noise | the whole scarf is now THREE pieces: ONE broad slightly-compressed band (0.024 thick x 0.084 tall, a hint of gather at the back) + two flat ends. Knot and both shoulder bridges DELETED; fluff halved, waves 3%, twist gentled. 704 verts (was 1626) |
| no taper | both free ends are CONSTANT-width fabric strips, 0.112 x 0.030 m end to end |
| short = 1/3 of long | both chains run the SAME seg (0.080 x kScarfRunSegFrac = 0.068): long 6 x 0.068 = 0.408, short 2 x 0.068 = 0.136 = exactly 1/3, off the LEFT shoulder (anchor tucked under the band edge at (-0.090,-0.070,1.518)); the long end full length on the right |

Gate 1365/1369 (same four GI4 debts).

## 8g. THE CONTEXT-FREE CONSULT + the seventh build (2026-08-20 night)

Chad: *"Everything is inside and not resting on top of the Sudburian. The two
lengths are not attached to the wrap -- they need to attach at the top of the
back of the neck wrap, then come up and out over and rest on the coat. It
drops down and hangs like it's his spine. Why is he transparent? Get a
context-free Fable consult and let's solve this -- this is a crucial part of
the game mechanic development of the Sudburian."*

A fresh-context Fable consult re-derived everything from the artefact (25
smoke shots, 16 stdlib audit scripts, point-in-mesh tests). Its verdicts, all
MEASURED:

**P0-1 (the burial):** the S3b chord plane sat 25-105 mm UNDER the drawn back
(the seated back bulges s 0.126 -> 0.207 -> 0.150 behind the neck-pelvis
chord); 216/252 long-tail verts were inside the suit. TWO bugs beneath it:
the bind scan's 0.15 lateral filter rejected every real back vertex (the
coarse suit's central back has NONE -- they all sit at |lateral| 0.215-0.221;
the "0.0747 back surface" was one shoulder vert), and no single plane can fit
a bent torso. **Fix:** the DRAWN BACK is now the surface -- at bind the suit's
back-quad verts are captured (rings along the torso), per frame they are
re-skinned (rigid, one matrix each) into banded keep-out planes
(`TrailChainInput.n_back_planes`, kTrailChainMaxBackPlanes=6; n=0 keeps the
old single-plane path bit-identical -- every trail_chain test and R4's
disable rule untouched). Keep-out off each plane = margin 12 mm + fabric
half-thickness 15 mm.

**P0-2 (tight / "transparent"):** the band's inner surface was authored at
EXACTLY the neck prism's radius (12-gon faces 0.0795..0.0805) and shared both
prism ring heights (1.5130/1.6090) -- coincident-surface z-fight. **Fix:**
inner 0.088, thickness 0.022 (outer 0.110 < the 0.1136 corner posts), span
1.528..1.602.

**P0-2b (glTF-invalid file):** patch_scarf.py appended 42 KB past the declared
`buffers[0].byteLength` -- only cgltf's unchecked reads tolerated it. Fixed in
save_glb + a bounds validator run after every patch.

**P1-3 (not attached):** roots sat 7 mm below and radially inside the band,
21.6 mm from the nearest wrap vert, heading 73 deg straight down. **Fix:**
root rings EMBEDDED in the band's top-back shell, rigid bridges arc UP AND
OUT OVER the collar to the chain anchors, and THE BONES MOVED WITH THEM:
scarf_01..06 re-seated (node TRS + rewritten IBMs) so the chain begins at the
bridge end 30 mm off the back face -- anchor stand-off is now 0. Twist
dropped (P2-5: it dug the strip edge 13.5 mm through the margin).

**Refuted by the consult:** no depth/render-state bug; the solver -> bone ->
skin -> GPU chain works end-to-end; the frame map is exact; the suit is
closed and occludes correctly -- NO stand-in suit needed (Chad had offered
one): the drawn back is a usable resting surface once the keep-out uses it.

Verified after the rebuild: the consult's own revealing side view
(3.0,90,8 at rest) now shows the full flat length attached at the wrap,
over the collar, resting on the coat to the seat. Gate 1365/1369 (same four
GI4 debts). GLB bounds-valid.

## 8h. Chad's eighth drive (2026-08-20 late) -- transparency ROOT-CAUSED, wide loose band, FLUTTER

His words: *"Still makes the main body of the Sudburian transparent. I can see
the whole scarf now in game but in the live Blender session you lose sight of
the long portion into the torso, likely why it makes the torso transparent in
game. The scarf's width needs to be a little wider and it should fit on the
neck loose more and be about the width of the head and it shouldn't look like
it's so tight it chokes the Sudburian. Also at the top speed the scarf raises
itself in the wind -- it should not go flat like a board but it should still
flap just with a higher frequency / less amplitude but the flapping should
also vary some."*

| finding / ruling | measured / built |
|---|---|
| **THE TRANSPARENCY (P0, root-caused at last)** | NOT the scarf, NOT z-fighting, NOT a depth bug: the suit prim `sudburian_proxy_grey.006` is wound INSIDE-OUT -- signed volume NEGATIVE (-0.0058), all 468 tris consistently inverted (0 inconsistent directed edges, closed, manifold), vertex normals AGREE with the inverted winding (468/0). The GLB marks every material doubleSided but the engine backface-culls unconditionally, so from astern the whole back wall culls away and the rider reads as a GLASS CASE with the scarf hanging inside it (drive-cam smoke, `scratch` nb0/drive0). Present in the PRISTINE helmet-v9 GLB **and in winter-gi's GLB (vol -0.01176)**: it predates every scarf build -- the scarf just gave the eye a solid object to expose it. The 8f/8g "z-fight" attributions were partial: real coincidences existed, but THIS was the transparency. **Fix:** `fix_suit_winding()` in patch_scarf.py (a POST-pass so the byte-identity proof stays byte-identical): reverse each suit tri's index order AND negate its vertex normals, idempotent by the volume sign, refuses shared accessors (the mitt lesson). vol -0.00580 -> +0.00580. Drive-cam smoke now shows a SOLID grey back with the tail resting ON it. ⚠ OPEN AT MERGE: winter-gi's GLB carries the same inversion -- whoever reconciles the helmet lane runs the same fix |
| "lose sight of the long portion into the torso" (Blender) | the live session was showing the same 908-vert (g) scarf as the GLB; from the session's FRONT viewport the tails are simply OCCLUDED by the (solid, in Blender) torso, and the old dia-0.22 band hid entirely behind the helmet chin. Nothing is buried: the suit's back wall is MEASURED near-vertical in bind frame (backmost y -0.155 from z 0.93..1.51) and the authored rest tail hangs at y -0.200..-0.220, 30+ mm clear with 15 mm fabric half-thickness. The in-game solver holds it clear too (back_clr 0.160 at rest) |
| wider / looser / head-width / not choking | head = helmet, MEASURED 0.270 wide. Band inner 0.088 -> 0.106 (26 mm visible slack off the 0.080 neck, was 8 = the choke), thick 0.022 -> 0.028, outer ~0.134..0.143 -> diameter ~0.27 = the head; z 1.518..1.570 (bottom rests 5 mm above the chest-box top, top 7 mm below the helmet family's lowest vert 1.5770; the 0.1136 trapezius corner posts now sit INSIDE the shell wall, no coincident surface). Tails 0.112 -> 0.132 wide (TAIL_W 0.056 -> 0.066). Roots/bridges/anchors all moved out with the shell; bones re-seated by patch_scarf + apply_scarf_live (live rig re-seated, 44 bones, body verts proven untouched, NOT saved) |
| flutter at speed (REVERSES ruling 8-open-(d) -- his call) | TWO layers, both tick-driven (render reads no clock), both dead behind `SEADS_SCARF=0` (byte-identical re-proven) and `SEADS_SCARF_FLUTTER=0`, dials live via `SEADS_SCARF_FLUTTER="amp,freq"`: (1) a wind-DIRECTION wobble into the solver input -- measured nearly invisible (+/-0.35 deg at 8 m/s: the chain is a hard low-pass, settle 2.6 s), kept for the slow sway; (2) **the visible flap = a traveling wave on a COPY of the solved chain** just before the bone frames are built -- zero feedback into the solver (no energy injection, the resonance-cliff lesson stands), displacement grows to the free end (s^1.5), f = 0.2 v / 0.408 hang (8 m/s -> 3.9 Hz, 20 -> 9.8 Hz capped 12), tip A = 0.14 * 12/(v+12) (8 -> ~84 mm, 20 -> ~52 mm: HIGHER frequency, LESS amplitude, never flat), two golden-ratio sines + a 0.31f amplitude envelope = never-repeating variation. Short tail: the same wave at half amplitude ("move a little, more simply"). MEASURED: ~1700 px of tail motion between half-period frames at 20 m/s; first A/B pinned an aliasing trap (frames 0.1 s apart at 9.8 Hz sample ~one period -- compare at half-period) |

⚠ **THE PIPELINE RECIPE BELOW WAS STALE AND IS CORRECTED (2026-08-21).** It
said `git checkout 8df63a101` — the **v9** helmet base. That was right when
written, and became WRONG the moment §8i merged the v13 helmet: following it
today regenerates the rider with the helmet Chad explicitly rejected. A recipe
that names a commit hash rots the instant the asset lineage moves.

★★★ **SUPERSEDED 2026-08-21 — THE RECONCILE LANDED.** The condition this
section named as the one thing that would retire steps 1–3 has happened: Chad
ruled the winter-gi→main reconcile PULLED FORWARD, and `assets/sled/indy650.glb`
in this tree is now a **clean export from the fixed `.blend`** — suit and mitt
shells outward at source, helmet on the head at source, balaclava block deleted
at source, scarf included. There is no longer a defective base to correct.

⚠⚠ **THE OLD-BASE PATH IS GONE, NOT DEGRADED — 2026-08-21, after the scarf
lane executed Chad's "retire fix suit" ruling.** The paragraph that stood here
said the pre-reconcile recipe "still works with `--repair`". **It no longer
does, and pretending otherwise is the exact rot this rung kept paying for**, so
it is corrected rather than left standing:

- `--repair` **was retired with `fix_suit_winding`**. Nothing remains that will
  correct the inverted suit and mitt shells an old base still contains.
- `fix_helmet_placement.py` was **deleted** in the reconcile, so the helmet
  offset and the balaclava block are not corrected either.

**Regenerating off a pre-reconcile base therefore produces a BROKEN rider —
see-through limbs, helmet off the neck, block attached — and nothing in the tree
will fix it or warn you at generation time.** The ctest leg
`test/unit/test_rider_winding.cpp` will fail afterwards, which is the safety
net, not a workflow. If you genuinely need that path for archaeology, recover
`fix_suit_winding` from git history at **d73ca430f**; do not rewrite it from
memory, and do not reintroduce a mutation that runs by default and reports
success — that shape is what put see-through arms in Chad's build for a day.

Pipeline for this build, CURRENT (post-reconcile):
1. `exec(open(...export_live_v13.py).read())` in the LIVE session over MCP —
   rig + proxy + helmet family, selection-only, to `scratch/`.
2. `python assets/character/sudburian_src/splice_sudburian.py
   scratch/sudburian_seated.glb --ref scratch/sudburian_ref.json`.

That is the whole pipeline. **Do not `git checkout` an old blob as a base** —
naming a hash is what rotted this recipe twice (v9, then v13).

RETIRED, and why:
* `fix_helmet_placement.py` — **DELETED** in the reconcile commit. It had
  already reduced to `"a clean source export, nothing to do."`
* `patch_scarf.py`'s scarf-append — the scarf is now IN the exported source, so
  appending it again would double-patch (the script already refuses that).
* `patch_scarf.py`'s `fix_suit_winding` — **RETIRED AND DELETED** (scarf lane,
  2026-08-21), Chad: *"retire fix suit"*. It went through way 2 (validator by
  default, `--repair` opt-in) for exactly as long as a defective base existed;
  once the reconcile landed it validated the shipping GLB with zero repairs and
  `--repair` unreached, which was the criterion for it to stop existing.
  **THE MEASUREMENT SURVIVES, THE MUTATION DID NOT:** it is now
  `test/unit/test_rider_winding.cpp`, a ctest leg run against the shipping asset
  on every build, mutation-verified (inverting the mitt prim turns it red).
  It lives in ctest rather than in a script because the winter-gi lane made the
  decisive objection: a validator inside `patch_scarf.py` only runs when someone
  runs `patch_scarf`, and with the repair gone nobody would — it would exist
  without ever executing, which reads as coverage while measuring nothing.

**Red-team round (context-free, from the artefact):** VERIFIED the winding fix
(exact per-triplet reversal + bitwise normal negation; 148/149 shared prims
byte-identical to the pristine, only the suit changed; GLB structurally valid)
and the flutter (deterministic, SEADS_SCARF=0 clean, zero solver feedback,
no NaN paths, trail_chain.* untouched). **REFUTED the first band cut:** top
z 1.596 sat 15 mm INSIDE the helmet's chin skirt (helmet_black reaches DOWN
to bind z 1.5814; 0.0 mm measured contact, 154 verts in the overlap annulus,
band skins to neck vs helmet to head = pose grind) behind an
arithmetic-inverted comment -> band top moved to 1.570 (the indy_rubber trim
hangs to 1.5770, 4.4 mm under helmet_black -- the 1.576 re-cut still grazed it
at 2.2 mm) and the bridges/roots lowered until the whole scarf measures
>= 8.0 mm off every helmet-family vert. Verify against the WHOLE helmet
family (helmet_* + indy_rubber + indy_steel), not one prim. P1: flutter amplitude popped at the 1.5 m/s
gate (A is maximal there by design) -> smoothstepped over the first 1 m/s.
P2 cards: helmet_black is an OPEN shell with net-negative volume (helmet
rung's business); several machine prims are also negative-volume (silently
one-sided under the cull -- consistent with the old zero-up-facing-deck
finding); the shared-accessor guard checks indices, not byte ranges.

## 8i. THE HELMET-LANE RECONCILIATION (2026-08-20 late, Chad's order)

Chad: *"Please reconcile the helmet lane, there is a new helmet made, it should
NOT be the one you have in this sandbox, there is a brand new one. It still goes
through the top of the coat a little bit at the top corner but it's good. Commit
and push to main game only with the new helmet I most recently made, not the one
I just played here. Put that to the main game and seads-recon."*

**Which helmet is "the brand new one" — PROVEN, not assumed.** `origin/sandbox/gi4-ride`
= `20d4cc107` "R3 HELMET: v13 EGG accepted by Chad's eye + MULTIDENT". Its GLB
blob hash `0aaf6e8610…` is **bit-identical at v13, at winter-gi's current HEAD
(`e6a2c9cf5`), and in winter-gi's working tree** — no commit since v13 touched
`assets/sled/indy650.glb` or `helmet_v13/`. So v13 IS the newest helmet asset,
and the sandbox the scarf was judged in carried the OLD v9 (`8df63a101`) — the
one Chad says not to ship.

**What was deliberately NOT brought along:** winter-gi carries newer UNPUSHED
commits (`4ffdad1a2`/`c89edd099`/`e6a2c9cf5`, the R3-WS fore-aft weight-shift
ladder) plus a dirty `rider_pose.cpp` working tree. That is a DIFFERENT rung,
unjudged in this tree; Chad's order was "only with the new helmet". The merge
takes `origin/sandbox/gi4-ride` (= v13 exactly), so none of it comes.

| merge item | resolution |
|---|---|
| `render/sled_model.cpp` conflict | ONE hunk, both sides adding a field to `SPrim` at the same line: the scarf's `bool is_scarf` and the helmet's MULTIDENT `int dent`. **Kept BOTH** (the co-existence contract, spec §1). Every other helmet hunk (`helmet_sudburian`/`helmet_dent_N` material gate, `sled_helmet_dent_set/get`, the `U` key cycle, the two draw-loop `sp.dent` skips) auto-merged |
| `assets/sled/indy650.glb` conflict | binary, no textual merge exists. Resolved by TAKING THE v13 FILE (`--theirs`) and RE-RUNNING THE SCARF PIPELINE on it: `patch_scarf.py` appends the scarf primitive, re-seats the bones, and re-runs the winding post-pass. The scarf is regenerated FROM ITS SOURCE onto the new helmet file, never hand-merged |
| **the owed `uGloss` line (handoff §4)** | **MOOT, verified.** That debt existed because the scarf drew a PROCEDURAL RIBBON in its own block, which would inherit the last prim's (glossy helmet's) uniform. The ribbons were retired in §8d; the scarf is now authored geometry drawn through the normal prim loop, which sets `uGloss` per prim from that prim's own material (`sled_model.cpp` `SetShaderValue(sm.shader, sm.loc_gloss, &sp.gloss, …)`). Nothing owed |
| **the suit winding, on the OTHER lane's file** | winter-gi's GLB carried the SAME inside-out suit the §8h fix caught (`vol -0.01176`, a deeper negative than main's `-0.00580` because the v13 splice re-authored the proxy). The post-pass flipped it on the merged file: **-0.01176 -> +0.01176**. The transparency does NOT come back with the helmet |
| scarf-vs-v13 clearance | MEASURED on the merged file: min scarf-vert to helmet-family-vert distance **0.0545 m** (was 0.0080 against v9's lower trim). The v13 egg sits well clear of the head-width band — no re-tune needed |
| `render/sled_model.h`, `test/unit/test_rider_pose.cpp`, `splice_sudburian.py`, all of `helmet_v13/`, both `.blend` archives | auto-merged clean |

**Known and ACCEPTED by Chad, not a defect to fix here:** the v13 helmet still
clips the top corner of the coat slightly ("it still goes through the top of
the coat a little bit at the top corner but it's good"). Recorded as his ruling,
carried forward for whoever opens the helmet rung next.

**Stale warning retired:** the v12 handoff §5.4 says the recon merges
`93e7cd820`/`95d0b2c38` carry rejected helmets and must not be pushed. Both are
now ancestors of `origin/main` (already pushed by the audio lane's merge), so
there is nothing to reset — what governs is FILE CONTENT, and this merge sets
the GLB to v13 + scarf in both trees.

## 8j. THE HELMET PLACEMENT FIX (2026-08-21, Chad's report on the shipped merge)

Chad, driving the shipped build: *"helmet is just on the right shoulder and
facing the right, its supposed to be on his head and facing forward"* then
*"it was made with it offset to the front, it has a block in it, that block
needs to be removed and helmut placed on head"*.

**This was a REAL regression I shipped in §8i** — and the §8i merge was not the
cause: the helmet's mesh bytes, its skin binding and the head IBM are all
BIT-IDENTICAL to winter-gi's accepted v13 file (verified by SHA1 on POSITION /
JOINTS_0 / WEIGHTS_0 and a per-joint IBM diff: only the six scarf joints moved,
by design). The defect was already in v13 and had simply never been seen IN
GAME — v13 was accepted on Chad's eye in Blender, and the tree he last drove
carried v9. My §8i verification looked at rear/side smoke shots where an
offset helmet still reads plausibly; I never checked it against the head.

**MEASURED root cause:** the whole helmet family (`helmet_sudburian` +
`helmet_dent_1..4`) is 100 % skin-weighted to the `head` joint (total weight
48780.0 on `head`, zero elsewhere) but was AUTHORED ~0.33 m FORWARD of it —
family bind-bbox centre z −0.2443 vs the head joint's bind world z −0.5848.
Because that offset lives in BIND space it is then ROTATED by the posed head
transform every frame, which is why a purely forward authoring offset reads in
game as *"on the right shoulder, facing right"*. Riding inside it at the same
offset: `head_balaclava` / `balaclava_black`, a **12-triangle box** — Chad's
"block" — the head stand-in the helmet was fitted around.

**The fix** — historically `assets/character/sudburian_src/fix_helmet_placement.py`,
run after `patch_scarf.py`. ★ That script is **DELETED as of 2026-08-21**: the
same move is now baked at SOURCE in the `.blend`, so the table below is the
RECORD of how the delta was derived, not a step anyone still runs. The
derivation is preserved because the number it produces is a gate:

| step | how |
|---|---|
| find the real head | the proxy vertices weighted to the `head` joint: bbox `(-0.09, 1.3129, -0.6998)..(0.09, 1.5579, -0.4698)`, i.e. the head box sits ON the head joint (joint y == box min y, joint x/z == box centre) |
| derive the move | the block is the stand-in the helmet was fitted around, so the delta that lands the BLOCK on the real head lands the HELMET on it. x/z centre-align; y **TOP**-align (the block is a balaclava — taller than the head box because it skirts down over the neck, so aligning tops preserves the authored crown clearance). Computed from the file every run, never a typed constant: **(0.0000, +0.0791, −0.3261) m** |
| move the family | translate the POSITION data of all 5 helmet meshes (48 780 verts) and update every accessor min/max. Baking into vertices is exact because the family is rigid to one joint |
| delete the block | drop the `mesh`/`skin` reference from the `head_balaclava` node. The NODE stays so every node index in the file remains valid — skins, IBMs and the MULTIDENT name gate all index by position |
| guards | refuses if a helmet POSITION accessor is shared with any other mesh (the mitt lesson); asserts afterwards that the moved family ENCLOSES the head on all three axes; re-reads the file and bounds-validates every bufferView (the P0-2b lesson); IDEMPOTENT — a second run sees the detached block and exits |

Verified: helmet bbox `(-0.1757, 1.2864, −0.7646)..(0.1757, 1.674, −0.3761)`
encloses the head on x, y and z. Smoke shots front / side / drive-cam show the
helmet squarely on the head, facing forward, block gone.

**⚠ OPEN FOR CHAD'S EYE (a look call, not a defect I should decide):** with the
block gone the helmet's face opening now reads LIGHT — the helmet's own
`helmet_white` liner (front face z −0.464) sits just ahead of the grey head box
(−0.4698) and shows through the port. The black balaclava box used to fill it.
The "no faces" guarantee still holds (a blank is not a face), but if Chad wants
the port DARK the honest options are (a) a dark face piece authored properly in
the helmet lane, or (b) tinting the head box. Not guessed either way.

**⚠ OWED TO THE HELMET LANE:** winter-gi's own GLB carries the same forward
offset and the same block — the lane should fix it AT THE SOURCE (the v13
splice) rather than inherit this GLB post-pass, and it also still carries the
inside-out suit (§8h).

## 8k. THE BLANKET FLIP WAS WRONG — PER SHELL, NEVER THE GRAND TOTAL (2026-08-21)

**The winter-gi lane caught a defect I shipped in §8h.** Their measurement,
independently reproduced here on the shipping GLB: the suit prim is **15
separate closed shells and they were never uniformly inverted**.

| shells | what they are | before §8h | after §8h's blanket flip |
|---|---|---|---|
| 6 × 12-tri BOXES | chest, upper chest, pelvis, head, both feet | inward (the real defect) | **outward — correctly fixed** |
| 9 × 44-tri PRISMS | neck, both upperarms, lowerarms, thighs, calves | **already outward** | **INSIDE-OUT — broken by the fix** |

`fix_suit_winding` reversed the whole primitive on its TOTAL signed volume, so
it fixed the six boxes and broke the nine prisms. Measured on the shipped file:
shells at −0.00184 … −0.01936, i.e. the rider's **arms, thighs, calves and neck
were backface-culled and see-through in game** — the remainder of the very
"transparent Sudburian" §8h was written to kill. A smoke shot at
`SEADS_SLEDCAM="3.2,60,10"` shows the near arm as a translucent outline.

**Why my own guard missed it:** `assert v_after > 0.0` read the GRAND TOTAL,
which went to **+0.01176** while nine shells sat negative underneath. That is
exactly the R2c-6 census law this repo already carries — *per piece, never one
grand total* — and this function violated it. It is the second time a total has
hidden a per-piece defect in this rung (the first: the four R2c numbers reading
0/0/0/0 over two real bugs). **A signed-volume assert on an assembly is not a
winding check.**

**It is also the same trap the winter-gi lane hit in the generator** and avoided
by measuring: `prism_volume` was ALREADY +5.657 while both box emitters sat at
−8.000, so a blanket "reverse the windings" there would have shipped a NEW
inverted part inside the fixing commit — and gated green, because nothing
measures prism orientation. Blind reversal breaks whatever it touches that was
already right. Measure each piece; flip only what is inward.

**THE FIX (this section):** `fix_suit_winding` now welds by position,
decomposes the primitive into connected shells, flips **only the shells whose
own signed volume is negative**, and asserts **every shell** positive
afterwards. `--winding-only` applies just this pass to an already-patched GLB
(the full patch refuses a double-patch). Idempotent: a second run reports
"already outward, all 15 shells".

**Result: 9 of 15 shells flipped, total +0.01176 → +0.19198** — the identical
figure the winter-gi lane derived independently from their per-shell `.blend`
fix. Two independent paths, one number.

⚠ **The §6 gate number is superseded: +0.01176 was recorded believing the prim
was uniformly inverted. The corrected figure is +0.19198.** Anyone citing the
old number is citing a file with nine inside-out limbs.

## 8l. THE MITTS, AND THE HELMET FAMILY'S OWN SHELLS (2026-08-21)

Chad: *"fix the mitts too and update the bone count law, MAKE SURE NOT TO MOVE
THE PIECES OF THE MITTS, I checked them and they look good, but if they are
transparent yea lets fix that."*

**His condition is structurally guaranteed, not merely respected.** A winding
pass writes ONLY triangle index order and vertex normals; it never writes
POSITION. That is now EXECUTABLE: `fix_suit_winding` hashes every POSITION
accessor in the file before the pass and asserts all of them bit-identical
after — *"PROVEN no vertex moved -- all 178 POSITION accessors bit-identical"*.
No mitt piece can move, by construction and by assertion.

**Mitts: 4 of 16 shells were inside-out** (−4.3e−05 … −7.2e−05) under a green
total of +0.00206 — the **third** instance in this rung of a positive grand
total hiding per-piece inversion. Flipped per shell: **+0.00206 → +0.00254**,
16/16 outward. The scarf prim was measured in the same pass and is 3/3 outward,
untouched.

### ⚠ A HELMET "FINDING" OF MINE — RAISED, THEN REFUTED, AND THE LAW'S SECOND HALF

My first per-shell census reported the helmet family inverted too
(`helmet_trim_black` at −0.0730 in all five variants). **That was WRONG.** The
winter-gi lane refuted it on the same bytes and I reproduced their refutation
here before accepting it:

**A glTF primitive is a MATERIAL SUBSET, not a shell.** `helmet_silver`
(+0.0708) is the OUTER skin of the egg and `helmet_trim_black` (−0.0730) is the
INNER liner of that same closed body. Taken alone each is an OPEN patch —
measured: 132 and 128 boundary edges — and **the signed volume of an open patch
is origin-dependent and says nothing about winding**; an inward-facing liner
*must* integrate negative by design. Welded across primitives the helmet is **7
shells, every one CLOSED (0 boundary edges) and POSITIVE**, and they sum to the
body shell's +0.0021267. Nothing in the helmet is inverted.

**So the law has two halves, and each of us was bitten by one:**
1. Signed volume must be taken **per shell, never over an assembly** — my §8k
   blanket flip failed this way (a positive grand total hid nine inverted
   shells).
2. Signed volume is a winding test **only on a CLOSED surface** — my helmet
   claim failed this way (splitting one closed body into material patches that
   were never closed). **Weld by position across primitives, check
   manifoldness, THEN take the sign. Anything else measures the origin.**

`fix_suit_winding` now enforces both halves: it refuses to flip any shell that
is not closed, and only asserts positivity on closed shells. Re-verified for
the prims it does touch: all 34 shells of the welded proxy mesh are closed and
positive, so every flip it made was legitimate.

**Consequence for §8j: the light face port is NOT an inverted trim shell — that
hypothesis is dead.** It still needs a real measurement of what renders there.
Nobody should author a face piece on the strength of a volume sign.

**A counting note for the record**, so two commits don't look contradictory:
the suit was *"9 of 15 flipped"* in the GLB because the §8h blanket flip had
already inverted those nine; **at source it was 6 boxes inward out of 15**, plus
4 mitt shells. Same endpoint, different starting state.

## 8m. §8j ANSWERED — the light face port, measured (winter-gi lane, 2026-08-21)

The §8j open question ("why does the face port read LIGHT now the balaclava box
is gone?") is **closed with a measurement, and my §8l hypothesis was wrong twice
over** — first blaming an inverted trim shell (refuted, §8l), and the real cause
is simpler than either guess.

The winter-gi lane ray-cast the grey head box's front face against the helmet
shell along the camera direction:

- **138 of 169 samples on the head-box front face SEE DAYLIGHT** — no helmet
  geometry between them and the camera.
- exposed band z 1.3129 … 1.5375 of a box spanning 1.3129 … 1.5579 — nearly its
  full height.
- a centre-line ray forward from the box **never hits the helmet at all.**

**The port reads light because you are looking straight at the proxy's grey head
box through an open port.** Nothing is inverted; nothing is mis-shaded.

And what `head_balaclava` actually was matters here: material `balaclava_black`,
colour (15,16,17), and `helmet_spec_v11` declares
`BALACLAVA = dict(x=92, v=117, h=247, color=(15,16,17))` — **dimensioned as a
balaclava, not merely a fitting cube.** So it was plausibly doing DOUBLE duty:
the head stand-in the helmet was fitted around, AND the dark thing behind the
port. §8j's wording — "the port reads light now that the black box is gone" —
turns out to be the literal mechanism rather than a coincidence of phrasing, and
§8l's "not shipping geometry" framing was at best half true.

✅ **RULED CLOSED BY CHAD, 2026-08-21: _"Grey box is fine, sudburian is camera
shy anyways."_ NO FACE PIECE, EVER — both lanes stand down permanently.**
The cautionary entry is better than either hypothesis that preceded it: **two
lanes each reasoned their way toward authoring a face piece off an inference,
and the actual answer was that the thing did not need fixing at all.** Neither
"inverted liner" nor "missing dark piece" was true; the port is open, the grey
box behind it is fine, and the fix was to stop looking for one.

Superseded framing kept for the record — Removing the box
was his explicit ruling and it stands; the "no faces" guarantee is now carried
by a grey slab. Whether the port gets a balaclava piece, a tinted visor, or
stays grey is his. Two lanes have now separately talked themselves toward
inventing a face piece off an inference — don't.

**Also fixed there:** Chad saw the helmet surface FLASHING. Cause: the balaclava
box had been moved by the same delta as the helmet so it would sit on the head,
which made it COINCIDENT with the grey head box — z-fighting. His ruling:
*"remove the black head inside the helmut and leave the grey one."*

### The winding law, final form

The winter-gi lane's correction to §8l's guard, folded in: **`boundary_edges == 0`
is necessary but NOT sufficient** — a non-orientable or self-intersecting shell
can be closed and still make signed volume meaningless. `fix_suit_winding` now
tests **DIRECTED edges** (this repo's own R2c-6 census pattern): on a closed,
consistently-wound surface every directed edge appears exactly once and its
opposite supplies the second use of that undirected edge. Self-intersection is
NOT detected — stated here rather than silently assumed. The honest law:

> **Signed volume is a winding test only on a surface that is CLOSED,
> ORIENTABLE and NON-SELF-INTERSECTING. Weld by position ACROSS primitives,
> verify that, and only then take the sign — per shell, never over an assembly.**
