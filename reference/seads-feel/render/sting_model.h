#pragma once
// THE STING, DRAWN -- app-side loader/drawer for assets/drone/sting.glb.
//
// docs/PLAN_20260904_sting_st5_art.md §2 (phase B). Cloned from
// render/flak_model.h/.cpp: cgltf load, FULL node hierarchy (raylib LoadModel
// flattens it), parents-before-children order, per-prim flat tint. Lives in
// the app shell (CMakeLists.txt source list, beside flak_model.cpp), NOT
// seads_render_core -- that target's layering bans raylib (see
// render/flak_model.h's own header for the precedent).
//
// The Sting is RIGID except for ONE driven channel: the four props. Every
// other node is drawn at its rest transform, so it is still one mount-relative
// unit dropped in at (pos, orient) exactly where the primitive block used to
// draw the bullet-body/dome/arms/stubs by hand (render/draw.cpp:3225-3269, the
// fallback arm below).
//
// ★★★ THE PROPS (Chad 2026-09-05: "the sting propellors need to spin when
// flying"). The GLB's four `sting_prop_*` nodes have their origins exactly on
// their motor hubs and no rest rotation (verified in the asset), so the spin is
// a clean local rotation about the model's glTF Z -- the flak_model driven-node
// idiom, recomposing local = T * R(spin) * S for those four nodes only.
// Direction alternates by the quad convention: ul/lr one way, ur/ll the other.
//
// ★★★ AND THE BLUR DISC (Chad, polish fly-2: the blades must "still be
// apparent but a circular blur"). Each prop also draws a thin translucent disc
// at its hub, radius = that prop's own MEASURED tip radius, whose opacity
// ramps in with the TRUE rate -- the Bf 109's own prop-disc idiom
// (render/draw.cpp's draw_prop / g_fleet.prop_mat: default-shader material,
// per-draw alpha on the diffuse colour, BLEND_ALPHA with depth-WRITE off so
// overlapping discs and the body behind them composite). The blades stay
// drawn and merely tint toward the disc as it fills in, so the two together
// read as blades dissolving into a disc rather than as either one alone.
//
// ⚠ THE PHASE IS APP-OWNED. render/draw.h's law forbids wall-clock in the
// renderer, so `spin` is an accumulated angle shipped through FrameInfo
// (sting_prop_phase, stepped in main.cpp on clamped_dt off the shared rate law
// in render/sting_audio.h and wrapped mod 2pi app-side so float precision
// survives a long flight). Passing 0 draws the rest pose, bit-identical to the
// pre-spin frame.
//
// Tint: ONLY prims whose material name contains "body" (the asset's declared
// tint target, sting_mat_body) take the quantized render::team_colors().ally
// -- the same authority the primitive block already reads (one livery rule,
// ally blue, never re-derived). Every other material (tan pods, silver tape,
// carbon, glass) keeps its OWN GLB colour so the hero model's material story
// survives the livery pass: accents should read as material, not paint.
//
// Kill switch: SEADS_STING_MODEL=0 forces "not ready" (the flak_gunner_enabled
// idiom, render/flak_gunner.cpp) -- the primitive fallback then draws exactly
// as it did before this rung, bit-identical, with no GLB even opened.

#include <glm/glm.hpp>

namespace render {

// True once SEADS_STING_MODEL is not "0" AND assets/drone/sting.glb has been
// found and loaded (lazy: the first ready()/draw() call does the load). A
// missing GLB is not an error -- callers fall back to the primitive block.
bool sting_model_ready();

// Draw the Sting eye-relative. `orient` is body->world (columns = right, up,
// forward, matching glm::mat3_cast(sting_draw.orientation) at the primitive
// call site). `ambient` is a reserved flat-brightness multiplier on the tint
// (default 1.0 = today's untouched flat-tint brightness; no sun term yet,
// the flak_model precedent -- a lit shader is private sled state and a fork
// of it would be a second opinion about the light). Returns false (draws
// nothing) if the model is unavailable; callers must fall back, never treat
// false as a frame-killing error.
// `spin` is the APPARENT blade angle [rad] (already soft-knee compressed
// app-side by render::sting_apparent_spin_rate -- see draw.h's
// sting_prop_phase). `rate_rad_s` is the TRUE rate, and it is a separate
// argument rather than something derived from the phase because the two say
// different things: the phase says where the blades are, the rate says how
// hard the machine is working, and only the second can drive the blur disc.
// Deriving a rate by differencing the phase here would recover the CLAMPED
// rate -- i.e. the disc would stop thickening at exactly the speed the clamp
// starts mattering, which is the speed the disc exists for.
bool sting_model_draw(const glm::dvec3& pos, const glm::dmat3& orient,
                      const glm::dvec3& eye, float ambient = 1.0f,
                      float spin = 0.0f, float rate_rad_s = 0.0f);

}  // namespace render
