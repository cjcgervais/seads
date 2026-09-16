#pragma once
// ★ F-POSE -- THE SUDBURIAN ON THE GUN, DRAWN (app-side).
//
// docs/FLAK_GUN_SPEC.md §7.6 + the P1-9 crouch-curve ruling (Chad,
// 2026-08-30). render/flak_pose.h (pure, render_core) owns the skeleton
// solve; THIS file owns cgltf + GPU meshes + CPU skinning, so it lives ONLY
// in the app target (the flak_model.cpp split precedent).
//
// THE MAN IS THE SHIPPED SUDBURIAN. Loaded from assets/sled/indy650.glb --
// its OWN cgltf instance keeping ONLY the skinned prims (suit, mitts, scarf,
// hoodie, pants, stripe, pristine helmet) + the skin's 44 joints (the
// 42-joint sudburian_rig plus the two scarf strap joints scarf_s01/s02 the
// skin also binds). Nothing here touches render/sled_model.cpp's private
// state, and the signed sled picture cannot move. Anthropometry
// (GunnerAnthro) is MEASURED off the loaded rig's rest joints -- the
// one-number rule, never typed.
//
// Skinning: joint world matrix = T(solved pos) * shortest-arc(rest bone dir
// -> posed bone dir) * rest rotation; leaf/twist/scarf joints follow their
// parent rigidly (rest-relative). CPU-skinned in gun model frame, then one
// mount * train transform at DrawMesh -- the same world map
// train_station_world() takes, so no world axis is assumed.
//
// Shading is FLAT TINT, the flak_model.cpp named trade: the sled's lit
// shader is private state and a fork of it would be a second opinion about
// the light. Chad judges whether the flat man reads beside the flat gun.
//
// VIEWS (Chad's 2026-08-31 ruling, after his fly "freelook shows floating
// mits"): the man is drawn ONLY from a camera that is OUTSIDE him -- the
// free-look pullout in play, and the SEADS_FLAKCAM smokes. Down the sight
// NOTHING of him is drawn: no head, no torso, no arms, no mitts. Flat-
// tinted limbs seen from inside his own head read as blobs, and his hands
// sit ~22 deg below the sight axis, so there is no framing that saves
// them (measured over four pull-back distances, 2026-08-31). `alpha`
// carries the pullout's fade-in so he cannot pop into existence in the
// lens.
//
// KILL SWITCH: SEADS_FLAK_GUNNER=0 disables the gunner draw, the sled
// rider hide AND the free-look pullout (which exists only to show him, so
// free-look falls back to the plain head turn) -- the frame is
// bit-identical to the pre-rung build.

#include <glm/glm.hpp>

#include "render/flak_model.h"

namespace render {

// Read once (static init): false only under SEADS_FLAK_GUNNER=0.
bool flak_gunner_enabled();

// Enabled AND the model actually loaded (lazy-loads on first call). The
// sled-rider hide must key off THIS, not off enabled -- a missing GLB must
// leave the rider visible on his sled, never vanish the man entirely.
bool flak_gunner_ready();

// Draw the posed Sudburian on gun `g` (uses g.pos/up/fwd0/train_rad/
// elev_rad), eye-relative. `alpha` is the pullout fade-in; at 0 nothing is
// drawn at all. Lazy-loads on first call; returns false when the model is
// unavailable (a missing optional prop must never kill the frame).
bool flak_gunner_draw(const FlakDraw& g, const glm::dvec3& eye, float alpha);

}  // namespace render
