#include "render/post.h"

#include <cstdlib>

#include "raylib.h"
#include "render/post_glsl.h"
#include "rlgl.h"

namespace render {

namespace {

// The scene FBO + the resolved post shader. Built lazily on the first post_begin
// at the live screen size; rebuilt if the window resizes.
struct PostState {
    bool tried = false;       // shader-load attempted (logged once)
    bool ok = false;          // FBO + shader are live
    bool fbo_failed = false;  // this size's FBO build failed — don't retry/log per frame
    int attempt_w = -1, attempt_h = -1;  // last size the FBO build was attempted at
    int w = 0, h = 0;         // current live FBO size

    RenderTexture2D rt{};  // .id=fbo, .texture=RGBA16F color, .depth=DEPTH24 tex
    Shader shader{};
    // cached uniform locations
    int locDepth = -1, locRes = -1, locContrast = -1, locLift = -1,
        locGrain = -1, locSplitShadow = -1, locSplitHi = -1, locSatC0 = -1,
        locSatC1 = -1, locSatDark = -1, locHalation = -1, locHalThresh = -1,
        locHalTint = -1, locVignette = -1, locFxaa = -1, locFrame = -1,
        locDebug = -1, locNear = -1, locFar = -1, locFocusStart = -1,
        locFocusEnd = -1, locDofRadius = -1, locSkyCoc = -1, locHtMode = -1,
        locHtScale = -1, locHtAngle = -1, locHtSoft = -1, locHtInk = -1,
        locHtGrainMul = -1;
};

PostState g_state;
PostParams g_params;
// S1b-3: the per-frame night amount driving split_hi -> split_hi_night.
// 0 by default, so anything that never calls set_post_night keeps the exact
// configured daylight look.
float g_night_amt = 0.0f;

// SEADS_NO_POST=1 bypasses the whole pass (A/B toggle + the degraded fallback).
// Read once; the env is a launch-time choice.
bool no_post_env() {
    static const bool v = [] {
        const char* e = std::getenv("SEADS_NO_POST");
        return e != nullptr && e[0] != '\0' && e[0] != '0';
    }();
    return v;
}

// ★★ THE SC1 HELMET FOG IS DELETED (Chad's ruling, 2026-08-17: "no more frost
// just turn it off"). It saturated in ~7.7 s below 8 m/s and cleared ONLY above
// 8 m/s, adding up to +0.495 luma at the frame corners -- so standing still or
// crawling, which is exactly what you do to LOOK at something, was the case that
// maximised it, and the only way out was to drive fast. It cost Chad two drives'
// worth of judgement before he named it ("its the frost!"), and it hid a rider
// who already spans only 28 of 255 luma values.
//
// Deleted, not defaulted off. It HAD been defaulted off (a3e669601) and that was
// not enough: the toggle was seeded from the bypass, so the first 'H' press
// turned frost back ON, and the readout that would have said so was itself
// hidden until a tune key armed it. A blinder reachable by one keystroke, with
// no visible state, is not off.
//
// ★ THE COLD MECHANIC IS UNTOUCHED AND THAT IS THE POINT OF THE RULING: cold
// still hardens the pack and still makes the machine faster (world::h,
// world::air_temp_c), world::frost still exists and still drives the dash rime.
// Only the thing between Chad and the screen is gone.

void cache_locations(Shader s) {
    auto& st = g_state;
    st.locDepth = GetShaderLocation(s, "uDepth");
    st.locRes = GetShaderLocation(s, "uResolution");
    st.locContrast = GetShaderLocation(s, "uContrast");
    st.locLift = GetShaderLocation(s, "uLift");
    st.locGrain = GetShaderLocation(s, "uGrain");
    st.locSplitShadow = GetShaderLocation(s, "uSplitShadow");
    st.locSplitHi = GetShaderLocation(s, "uSplitHi");
    st.locSatC0 = GetShaderLocation(s, "uSatC0");
    st.locSatC1 = GetShaderLocation(s, "uSatC1");
    st.locSatDark = GetShaderLocation(s, "uSatDark");
    st.locHalation = GetShaderLocation(s, "uHalation");
    st.locHalThresh = GetShaderLocation(s, "uHalThresh");
    st.locHalTint = GetShaderLocation(s, "uHalTint");
    st.locVignette = GetShaderLocation(s, "uVignette");
    st.locFxaa = GetShaderLocation(s, "uFxaa");
    st.locFrame = GetShaderLocation(s, "uFrameCount");
    st.locDebug = GetShaderLocation(s, "uDebugMode");
    st.locNear = GetShaderLocation(s, "uNear");
    st.locFar = GetShaderLocation(s, "uFar");
    st.locFocusStart = GetShaderLocation(s, "uFocusStart");
    st.locFocusEnd = GetShaderLocation(s, "uFocusEnd");
    st.locDofRadius = GetShaderLocation(s, "uDofRadius");
    st.locSkyCoc = GetShaderLocation(s, "uSkyCoc");
    st.locHtMode = GetShaderLocation(s, "uHalftoneMode");
    st.locHtScale = GetShaderLocation(s, "uHalftoneScale");
    st.locHtAngle = GetShaderLocation(s, "uHalftoneAngle");
    st.locHtSoft = GetShaderLocation(s, "uHalftoneSoft");
    st.locHtInk = GetShaderLocation(s, "uHalftoneInk");
    st.locHtGrainMul = GetShaderLocation(s, "uHalftoneGrainMul");
}

// Build the custom FBO: RGBA16F color texture (Fable P1) + a real DEPTH24
// TEXTURE (not a renderbuffer — raylib's LoadRenderTexture default; Fable P1).
bool build_fbo(int w, int h) {
    RenderTexture2D rt{};
    rt.id = rlLoadFramebuffer();
    if (rt.id == 0) return false;
    rlEnableFramebuffer(rt.id);

    const unsigned int colorId =
        rlLoadTexture(nullptr, w, h, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    // Real depth TEXTURE (useRenderBuffer = false) so S6 DoF can sample it.
    const unsigned int depthId = rlLoadTextureDepth(w, h, false);
    if (colorId == 0 || depthId == 0) {
        // Free whichever attachment succeeded (nothing is attached yet, so
        // rlUnloadFramebuffer only reclaims the empty FBO).
        if (colorId != 0) rlUnloadTexture(colorId);
        if (depthId != 0) rlUnloadTexture(depthId);
        rlDisableFramebuffer();
        rlUnloadFramebuffer(rt.id);
        return false;
    }
    rt.texture.id = colorId;
    rt.texture.width = w;
    rt.texture.height = h;
    rt.texture.mipmaps = 1;
    rt.texture.format = RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rt.depth.id = depthId;
    rt.depth.width = w;
    rt.depth.height = h;
    rt.depth.mipmaps = 1;
    rt.depth.format = 19;  // DEPTH_COMPONENT24 (raylib depth-tex convention)

    rlFramebufferAttach(rt.id, colorId, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(rt.id, depthId, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_TEXTURE2D, 0);

    const bool complete = rlFramebufferComplete(rt.id);
    rlDisableFramebuffer();
    if (!complete) {
        // rlUnloadFramebuffer reclaims the depth attachment + the FBO but NOT
        // the color texture — free it explicitly (no double-free).
        rlUnloadTexture(colorId);
        rlUnloadFramebuffer(rt.id);
        return false;
    }
    // Smooth taps for FXAA/halation; clamp so the edge doesn't bleed.
    rlTextureParameters(colorId, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(colorId, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
    rlTextureParameters(colorId, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(colorId, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
    // Depth sampled NEAREST + clamped: at a silhouette a filtered depth would
    // linearize to a near-biased in-between value (benign — errs toward SHARP —
    // but make it explicit so the S6 DoF CoC never depends on the driver default
    // (Fable-BEFORE P2-e); raylib sets NEAREST already, this pins it).
    rlTextureParameters(depthId, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_POINT);
    rlTextureParameters(depthId, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_POINT);
    rlTextureParameters(depthId, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
    rlTextureParameters(depthId, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);

    g_state.rt = rt;
    g_state.w = w;
    g_state.h = h;
    return true;
}

// Build (or rebuild on resize) the FBO + shader. Logs once on failure so the
// degraded fallback (post disabled) is never silent (Fable: a logged decision).
void ensure_built(int w, int h) {
    auto& st = g_state;
    if (st.ok && st.w == w && st.h == h) return;

    // A size change (or the first build) drops any old FBO and clears the
    // per-size failure latch so the new size gets ONE fresh attempt. Without
    // this latch a persistent build failure (e.g. RGBA16F unsupported) would
    // retry + log every frame (Fable-AFTER P1-1).
    if (w != st.attempt_w || h != st.attempt_h) {
        if (st.ok) {
            UnloadRenderTexture(st.rt);
            st.rt = RenderTexture2D{};
            st.ok = false;
        }
        st.fbo_failed = false;
        st.attempt_w = w;
        st.attempt_h = h;
    }
    if (st.fbo_failed) return;  // this size already failed — no per-frame retry

    if (!st.tried) {
        st.tried = true;
        st.shader = LoadShaderFromMemory(nullptr, kPostFS);
        if (st.shader.id == 0 || st.shader.id == rlGetShaderIdDefault()) {
            TraceLog(LOG_WARNING,
                     "SEADS post: kPostFS failed to compile — post pass "
                     "DISABLED (scene renders raw to the backbuffer).");
            st.ok = false;
            return;
        }
        cache_locations(st.shader);
    }
    if (st.shader.id == 0 || st.shader.id == rlGetShaderIdDefault()) return;

    if (!build_fbo(w, h)) {
        TraceLog(LOG_WARNING,
                 "SEADS post: scene FBO (RGBA16F + depth-tex) incomplete — post "
                 "pass DISABLED (scene renders raw to the backbuffer).");
        st.ok = false;
        st.fbo_failed = true;  // latch: don't retry/log this size every frame
        return;
    }
    st.ok = true;
}

}  // namespace

void set_post_params(const PostParams& p) { g_params = p; }

void set_post_night(float night_amt) {
    g_night_amt = night_amt < 0.0f ? 0.0f : (night_amt > 1.0f ? 1.0f : night_amt);
}

bool post_begin(int sw, int sh) {
    if (no_post_env() || sw <= 0 || sh <= 0) return false;
    ensure_built(sw, sh);
    if (!g_state.ok) return false;
    BeginTextureMode(g_state.rt);
    ClearBackground(Color{0, 0, 0, 255});
    return true;
}

void post_end_and_resolve(int sw, int sh, int frame_count, int debug_mode) {
    if (!g_state.ok) return;
    EndTextureMode();  // back to the backbuffer

    Shader s = g_state.shader;
    const PostParams& p = g_params;
    const float res[2] = {static_cast<float>(sw), static_cast<float>(sh)};
    SetShaderValue(s, g_state.locRes, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(s, g_state.locContrast, &p.contrast, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locLift, &p.lift, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locGrain, &p.grain, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locSplitShadow, &p.split_shadow[0],
                   SHADER_UNIFORM_VEC3);
    // ★ S1b-3: the highlight tint blends toward split_hi_night after dark.
    // The split-tone REPLACES hue for low-saturation pixels (col ~ Y*tint), so
    // a warm split_hi repaints bright winter-night snow CREAM no matter what
    // colour the light was -- measured, and the reason this had to move here
    // rather than into the night light. At g_night_amt = 0 this is exactly
    // p.split_hi, so DAYLIGHT IS BIT-IDENTICAL to the flown look.
    const glm::vec3 split_hi_now =
        p.split_hi + (p.split_hi_night - p.split_hi) * g_night_amt;
    SetShaderValue(s, g_state.locSplitHi, &split_hi_now[0], SHADER_UNIFORM_VEC3);
    SetShaderValue(s, g_state.locSatC0, &p.sat_c0, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locSatC1, &p.sat_c1, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locSatDark, &p.sat_dark, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHalation, &p.halation, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHalThresh, &p.halation_threshold,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHalTint, &p.halation_tint[0],
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(s, g_state.locVignette, &p.vignette, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locFxaa, &p.fxaa, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locFrame, &frame_count, SHADER_UNIFORM_INT);
    SetShaderValue(s, g_state.locDebug, &debug_mode, SHADER_UNIFORM_INT);
    // Near/far single-sourced from the SAME clip planes the scene projection uses
    // (Fable Q7); the debug depth viz linearizes with them.
    const float nearZ = static_cast<float>(rlGetCullDistanceNear());
    const float farZ = static_cast<float>(rlGetCullDistanceFar());
    SetShaderValue(s, g_state.locNear, &nearZ, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locFar, &farZ, SHADER_UNIFORM_FLOAT);
    // S6 far-field-only DoF dials (view-space m / screen px; the DoF CoC now also
    // consumes uNear/uFar, so those are no longer debug-only).
    SetShaderValue(s, g_state.locFocusStart, &p.focus_start, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locFocusEnd, &p.focus_end, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locDofRadius, &p.dof_radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locSkyCoc, &p.sky_coc, SHADER_UNIFORM_FLOAT);
    // S6 "printed card" halftone/dither MODE (halftone_mode=0 -> shader skips it).
    SetShaderValue(s, g_state.locHtMode, &p.halftone_mode, SHADER_UNIFORM_INT);
    SetShaderValue(s, g_state.locHtScale, &p.halftone_scale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHtAngle, &p.halftone_angle, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHtSoft, &p.halftone_soft, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHtInk, &p.halftone_ink, SHADER_UNIFORM_FLOAT);
    SetShaderValue(s, g_state.locHtGrainMul, &p.halftone_grain_mul,
                   SHADER_UNIFORM_FLOAT);

    // Depth texture bound as a plain Texture2D to the uDepth sampler (slot 1;
    // texture0 = the color, bound by DrawTexturePro to slot 0).
    Texture2D depthTex{};
    depthTex.id = g_state.rt.depth.id;
    depthTex.width = g_state.w;
    depthTex.height = g_state.h;
    depthTex.mipmaps = 1;
    depthTex.format = RL_PIXELFORMAT_UNCOMPRESSED_R32;  // sampled as .r

    BeginShaderMode(s);
    SetShaderValueTexture(s, g_state.locDepth, depthTex);
    // Flip Y (FBO is bottom-up): negative source height.
    const Rectangle src{0.0f, 0.0f, static_cast<float>(g_state.w),
                        -static_cast<float>(g_state.h)};
    const Rectangle dst{0.0f, 0.0f, static_cast<float>(sw),
                        static_cast<float>(sh)};
    DrawTexturePro(g_state.rt.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f,
                   WHITE);
    EndShaderMode();
}

void unload_post() {
    if (g_state.ok) {
        UnloadRenderTexture(g_state.rt);
        g_state.rt = RenderTexture2D{};
        g_state.ok = false;
    }
    if (g_state.tried && g_state.shader.id != 0 &&
        g_state.shader.id != rlGetShaderIdDefault()) {
        UnloadShader(g_state.shader);
    }
    g_state.shader = Shader{};
    g_state.tried = false;
}

}  // namespace render
