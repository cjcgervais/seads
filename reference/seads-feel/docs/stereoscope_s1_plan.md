# S1 — LOCK THE LOOK: B&W stereoscope post stack + depth FBO (design note)

Scratch design for S1 (stereoscope-sudbury). Live status stays in `docs/stereoscope_handoff.md`;
this is the pre-build design + the Fable-BEFORE question set. S0 is DONE (lock LOCKED 2026-07-10).

## Goal (plan §S1)
One new full-screen post pass + a real depth-texture FBO. The scene is authored MONO; the planes are
the ONLY chroma. Chad locks the tone ONCE over Chelmsford, then tunes `config/world.toml` dials.

## Codebase facts (verified this session)
- Frame draw = `render/draw.cpp draw_frame`: `BeginDrawing → ClearBackground → BeginMode3D`(planet/
  aircraft/props)`→ EndMode3D →` HUD/reticle/gunsight/readouts `→ EndDrawing`. No RenderTexture yet.
- GLSL idiom: pure source strings in `render/*_glsl.cpp` (raylib-free → the headless
  `test_asset_validator` gates them, asserting **no self-declared uniforms** = the clock/state backdoor
  guard). Gemini drafts, Fable vets, Opus reviews + single-sources.
- Config idiom: `cfg::WorldParams` (config layer, render-free) → the app maps to a `render::*Params`
  struct + calls a setter (e.g. `set_planet_build_params`). No bare look-constants in GLSL/`render/`.
- `[tone]` (contrast/lift/grain) is **loaded but UNUSED** — no shader consumes it. **No double-tone to
  un-wire**; S1 wires it fresh as the post pass's data (post = the ONE tone owner).
- Haze/scatter is ALREADY single-sourced in the scene (`sky_color()` feeds both sky + planet aerial,
  CLAUDE.md). ⇒ **a 2nd "aerial perspective" haze in POST would FORK it (H1 double-count).**

## Proposed architecture
- `render/post.{h,cpp}` — owns the scene FBO (color RT + a real depth-TEXTURE attachment via rlgl,
  GL 3.3 core, raylib shadowmap template) + loads/composes the post FS. `begin_scene()` /
  `resolve(PostParams)`. A11y flags: `SEADS_NO_POST=1` (A/B bypass, like `SEADS_NO_M2`).
- `render/post_glsl.{h,cpp}` — the pure fullscreen post FS source (Gemini draft, validator-gated).
- `cfg::WorldParams` — extend `[tone]` → the look dials (contrast, lift, grain, split-tone shadow/
  highlight tints, halation strength/threshold, vignette). App maps → `render::PostParams`.
- `draw.cpp draw_frame`: wrap `BeginMode3D..EndMode3D` in the scene FBO; after EndMode3D, resolve
  through the post FS to the backbuffer; **HUD drawn RAW after** (SPEC §9.2 — no grain on the reticle).
- Depth FBO built now (locks depth for S6 DoF/clarity, flag-gated OFF in S1).

## Post chain (plan default — pass ORDER is the Fable ★ decision)
`FXAA → filmic S-curve → silver split-tone (sat-gated) → warm halation → film grain → vignette`
**DROPPED from the plan's chain: "aerial perspective"** — the scene already single-sources haze;
a post copy double-counts (H1). Depth FBO stays for S6 DoF only. (Fable to confirm the drop.)
DoF + depth-gated clarity: coded stubs behind a flag, enabled in S6.

## The P0 invariants (skill house-law)
- NO full-frame desaturate. Chroma path = `mix(splitTone(lum), rgb, sat)`, `sat` = the pixel's OWN
  chroma (mono→silver, colored planes pass through). NEVER a global sat uniform.
- Post = the ONE tone owner. HUD/reticle draw RAW after post.
- No bare look-constants in GLSL. No clock read in `render/` — grain seed = an **app-owned frame
  counter** passed as a uniform (app owns the accumulator; render never reads a clock).

## Fable-BEFORE question set (the ★ = colorspace + pass-order + linearization)
Q1 Scene RT format: RGBA8 LDR (scene authored display-ish mono ~[0,1]) vs RGBA16F linear-HDR for a
   correct ACES S-curve. Is ACES even right for an LDR-authored mono scene, or is the "filmic S-curve"
   a display-space contrast sigmoid (the [tone] contrast/lift)? Banding risk of a steep curve in RGBA8.
Q2 The ONE correct space for each op — FXAA luma / S-curve / split-tone / grain / vignette — and any
   decode→linear→encode needed.
Q3 Pass order + FXAA placement (plan puts FXAA pass-0; it wants a perceptual signal). Rule the order.
Q4 CONFIRM dropping post aerial-perspective (double-haze H1). If Fable wants it in post, how to
   single-source vs the scene `sky_color()` haze.
Q5 Split-tone `sat` metric so mono terrain (chroma≈0) → full silver and chroma planes pass through,
   robustly; tints kept near-neutral (silver, not sepia).
Q6 Grain: static vs animated (no render clock — frame-counter uniform is the seam-legal seed). Given
   Chad's "minimal grain, crisp filmic," is static fine grain acceptable?
Q7 Depth linearization for the S6 DoF (GL 3.3 core depth): the correct `z_ndc→view distance` so the
   FBO we lock now is right for S6 (reconstruct radial distance, not just z).

## FABLE-BEFORE VERDICT (2026-07-10) — SOUND-WITH-FIXES; all P0/P1 folded below
Fable read the repo (caught the off-center frustum + verified `rlSetClipPlanes(2,60000)` draw.cpp:319).
- **P0 · NO ACES / no scene-linear operator.** Scene is display-referred → ACES would double-tonemap
  AND its highlight desat would DRAIN plane chroma (breaks the one law). Use a display-space per-channel
  contrast sigmoid: `s(x)=x^c/(x^c+(1-x)^c)`, c=contrast; lift on input `x'=lift+(1-lift)·x`.
- **P0 · post aerial-perspective STAYS DROPPED.** Double-counts `sky_color()` haze AND would sit
  outside the `weather_haze` gate (breaks haze-is-rare). Legal future S6 shape: scene writes remaining
  transmittance T into the RT **ALPHA** (reserve it in the FBO contract NOW); post may only modulate T.
- **P0 · radial depth via `invProj`.** Off-center frustum (lens_shift, draw.cpp:593-602) breaks the
  symmetric tan-reconstruction. Lock: `v=invProj·vec4(ndc,1); pos=v.xyz/v.w; radial=length(pos)`.
- **P1 · scene RT = RGBA16F** (smooth mono sky/haze gradients band badly under a >1-slope sigmoid in
  RGBA8; grain can't fix INPUT quantization). Post runs in float, quantizes ONCE at the backbuffer.
- **P1 · no-sRGB guardrail:** never `GL_SRGB8_ALPHA8` / `GL_FRAMEBUFFER_SRGB` (silent linearize). Repo
  is clean; keep it. **Everything works in ONE space: display-referred.** No decode→linear→encode.
- **P1 · split-tone gate:** `mx=max(r,g,b); C=mx-min(r,g,b); sat=smoothstep(C0,C1, C/max(mx,m_dark))`
  with C0≈0.06 C1≈0.25 m_dark≈0.04 (config). Measure `sat` on PRE-split-tone rgb (post-S-curve ok).
  Relative chroma → dark saturated planes still pass; m_dark floor kills the near-black halo.
- **P1 · grain ANIMATED, TERMINAL:** frame-counter uniform seed (seam-legal); luminance-only equal-
  channel add; mid-weighted `w=0.15+0.85·4Y(1-Y)`, `c+=k_g·g·w`, k_g≈0.02-0.04, `g=hash(fragcoord,
  frame)`. Static grain = "dirty monitor," rejected.
- **P1 · depth = real DEPTH_COMPONENT24 TEXTURE** (raylib default is a renderbuffer — custom FBO must
  attach a texture). n/f/invProj single-sourced as uniforms from the rlSetClipPlanes values (2, 60000).
- **FINAL ORDER: FXAA → S-curve → split-tone → halation → vignette → grain.** (FXAA first: scene RT is
  already perceptual; vignette before grain so grain is the terminal single-quantize dither.) Structure:
  pass1 = FXAA (scene RT→16F RT); pass2 = S-curve..grain fused per-pixel. Halation = display-space screen
  blend `c+tint·h·(1-c)`. HUD raw after post (outside the print — accepted).
- P2 (noted, optional): grain frame-hold at 120+fps, res-normalized grain size, S6 sky-depth gate d≥0.9999.

## FABLE-AFTER VERDICT (2026-07-10) — SOUND-WITH-FIXES, no P0; core look math verified correct
Fable read the LANDED files. Confirmed correct: the sigmoid (clamped, no blowup), the split-tone gate
(mono C=0→silver, relative-chroma passes planes, m_dark floor), FXAA (true passthrough at 0, no div-by-
zero), halation screen-blend (can't run away), luminance-only equal-channel grain (animates from the
app ordinal), the depth linearization (n/f single-sourced), and the seam (no clock; exact-match uniform
allowlist). Folded:
- **P1-1 (FBO-failure lifecycle):** the failure path retried + logged EVERY frame and leaked partial GL
  objects; `post_enabled()` was dead + wrong. → Added a per-size `fbo_failed` latch (one attempt per
  size, reset on resize), freed every partial resource on both `build_fbo` failure paths (rlUnload the
  color tex explicitly — rlUnloadFramebuffer frees only depth+fbo), DELETED `post_enabled()`.
- **P2-1:** `silver = clamp(Y*tint,0,1)` so a >1 highlight tint can't flip halation's `(1-col)` negative.
- **P2-2:** `tone contrast` upper cap 8 (past ~30 the 1e-6 eps collapses mid-grey; typo guard).
- **P2-3:** grain seed `uFrameCount % 1024` (no float-ulp drift on multi-hour sessions).
- **P2-4:** hoisted the depth-viz branch above FXAA (no wasted taps in debug mode).
- **P2-6:** `unload_post()` wired at app shutdown.
- **P2-5 DEFERRED (noted):** halation `radius=4.0`px is a bare FS constant — Fable ruled it borderline/
  structural (resolution-coupled). Left as-is; promote to a `[tone]` dial if Chad wants a bigger bloom.
- Re-gate after folds: build PASS, ctest 349/349, validator 16/16, re-smoke (lit + depth) unchanged.

## SCENE-MONO GAP (honest finding, NOT a post bug) — for Chad + the celestial thread
The post pass correctly silvers MONO pixels and passes CHROMA through (planes-only-color). But the scene
is not yet fully authored mono: the AURORA (green/teal/magenta) has chroma, so it passes through as
color (visible in `shots/s1_poststack.png`). To complete the B&W vision every colored SCENE element
(aurora first) must be authored mono in its own shader — the post pass cannot distinguish "aurora
chroma" from "plane chroma." This is scene-authoring (aurora is the parallel celestial thread), tracked
here as the natural next step; it does NOT block S1's tone-lock.
