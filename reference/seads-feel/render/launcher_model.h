#pragma once
// THE GUNSTOCK LAUNCHER, DRAWN -- app-side loader/drawer for
// assets/drone/launcher.glb.
//
// docs/PLAN_20260904_sting_st5_art.md §3 (phase C/D). A straight sibling of
// render/sting_model.{h,cpp}: rigid cgltf load, FULL node hierarchy
// (raylib's LoadModel flattens it), parents-before-children order, per-prim
// flat tint, lazy load, graceful miss. Lives in the app shell's source list
// (CMakeLists.txt, beside sting_model.cpp), NOT seads_render_core -- that
// target's layering bans raylib.
//
// THE ONE THING IT ADDS OVER sting_model: STATIONS. The launcher is not just
// drawn, it is POSED ONTO A MAN, so the pose needs to know where the grips,
// the buttplate, the sight and the muzzle are in the asset's own frame. Read
// off the five st_* empties exactly the way render::flak_stations() reads the
// gun's (render/flak_model.cpp), and FAIL SOFT: a missing station leaves the
// model "not ready" with a named TraceLog rather than posing a man onto a
// station that does not exist.
//
// AUTHORING FRAME (the contract the exporter fails loud on): fire direction
// is glTF -Z, up is +Y, the origin is the centre of the two grips.
//
// Kill switch: SEADS_STING_LAUNCHER=0 forces "not ready" on its own, and
// SEADS_STING_MODEL=0 kills the drone AND the launcher together -- the pair
// is one piece of kit and a session that wants the pre-ST-5 picture wants
// both gone. Either way "not ready" means nothing is drawn at all: there is
// no primitive fallback for the launcher (it had no primitive to fall back
// to), so a missing GLB is bit-identical to the frame before this rung.

#include <glm/glm.hpp>

#include "render/sting_deploy.h"

namespace render {

// True once the kill switches allow it AND assets/drone/launcher.glb has
// loaded AND all five st_* stations were found. A missing GLB is not an
// error -- callers draw nothing and the game is unchanged.
bool launcher_model_ready();

// The five station empties in the asset's own model space, rest pose. Valid
// (and `from_glb` true) only when launcher_model_ready(); otherwise the
// caller keeps the NOMINAL values render::sting::Stations ships with, which
// is what lets the deploy animation be built and flown before the art lands.
const sting::Stations& launcher_stations();

// Draw the launcher. `pos` is the model origin (the centre of the grips) in
// absolute world; `basis` columns are (right, up, +Z) with the fire direction
// at -basis[2] -- the same convention render::sting_model_draw takes, and the
// same one render::sting::Frame carries. `ambient` is the reserved flat
// brightness multiplier on the tint (1.0 = untouched), the flak_model
// precedent. Returns false and draws nothing when the model is unavailable.
bool launcher_model_draw(const glm::dvec3& pos, const glm::dmat3& basis,
                         const glm::dvec3& eye, float ambient = 1.0f);

}  // namespace render
