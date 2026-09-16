#pragma once
// THE FLAK GUN, DRAWN -- app-side loader/drawer for assets/flak/oerlikon_mk4.glb.
//
// docs/FLAK_GUN_SPEC.md §7.3 (F-LOAD/DRAW). The raylib half of the flak rung:
// render/flak_gun.h (pure, render_core) owns the stations and kinematics;
// THIS file owns cgltf + GPU meshes + DrawMesh, so it lives ONLY in the app
// target (the sled_model.cpp split precedent -- CMakeLists "app shell" list,
// never seads_render_core, whose layering bans raylib).
//
// Loader shape copied from render/sled_model.cpp load_model: cgltf, the FULL
// node hierarchy kept (raylib LoadModel flattens it -- unaffordable for a
// DRIVEN prop), parents-before-children order, flat base_color_factor
// materials via std::lround. The two driven nodes are written per draw from
// the Pose: flak_train gets train_quat, flak_cradle gets cradle_quat -- the
// ONE cradle sign lives in flak_gun.h, never re-derived here.
//
// Shading is deliberately FLAT TINT (placeholder-grade, the same class as
// the pump-marker cubes in draw.cpp): no sun term yet. Named trade for the
// fly; the sled's lit shader is private state and a fork of it would be a
// second opinion about the light.

#include <glm/glm.hpp>

#include "render/flak_gun.h"

namespace render {

// One gun's placement + live pose, app-computed (main.cpp), render-consumed.
struct FlakDraw {
    glm::dvec3 pos{0.0};   // pedestal base centre, world (on the ground)
    glm::dvec3 up{0.0};    // local up at the site
    glm::dvec3 fwd0{0.0};  // threat bearing (tangent; re-projected on use)
    double train_rad = 0.0;
    double elev_rad = 0.0;
    double recoil_m = 0.0;  // cosmetic aft kick of the gun body [m]
    // STAGE C: reload_left_s / reload_s, in [0,1]. 0 = no reload running, and
    // at EXACTLY 0 the drum node's transform is untouched (bit-identical rest
    // -- flak_model.cpp guards on `> 0.0`, it does not add a zero). Drives
    // flak::drum_swap; see render/flak_gun.h "STAGE C".
    double reload_frac = 0.0;
    // Which faction's pump this gun defends (combat::CqFaction values,
    // 0 = VALLEY / 1 = SUDBURY) -- the M-key chart's owner-ring cue.
    int faction = 0;
    // STAGE D (the AI gunner): this gun's OWN muzzle-flash / snow-blast
    // envelopes and FX phase, so an AI-manned gun two kilometres away flashes
    // when it fires. The PLAYER's gun keeps reading FrameInfo::flak_flash /
    // flak_blast / flak_shot_seq (untouched), so with no AI guns every field
    // here stays 0 and not one primitive is drawn -- the off-arm.
    double flash = 0.0;
    double blast = 0.0;
    int shot_seq = 0;
};

// Draw one gun eye-relative. Lazy-loads the GLB on first call; returns false
// when the model is unavailable (caller may fall back or skip -- a missing
// optional prop must never kill the frame).
bool flak_model_draw(const FlakDraw& g, const glm::dvec3& eye);

// The station table measured off the LOADED GLB's st_* node world transforms
// (rest pose, model frame) -- the ONE source; nothing app-side retypes a
// station number. Lazy-loads the model on first call. False = GLB unavailable.
bool flak_stations(flak::Stations& out);

}  // namespace render
