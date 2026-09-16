#pragma once

// ★★★ L4 — THE M-KEY MAP, IN ITS OWN TRANSLATION UNIT
// (docs/PLAN_20260901_game_loop_millwright.md §4.4).
//
// WHY THIS FILE EXISTS. The map was 450 lines living inside `draw_frame`, in
// the middle of a 3,200-line function in a 6,000-line file that every render
// lane edits. It is not a HUD element -- it is a second, complete, top-down
// renderer with its own projection, its own basemap, its own symbology and
// (from this rung) its own view state. Growing zoom / pan / declutter /
// objectives INSIDE `draw_frame` would have put the whole millwright chart in
// the one place where four lanes collide.
//
// ⚠ THE FIRST COMMIT OF THIS FILE MOVED THE BLOCK AND CHANGED NOTHING. Not a
// renamed local, not a reordered draw call: the body below arrived verbatim
// from `render/draw.cpp`, so the extraction is pixel-identical BY
// CONSTRUCTION rather than by inspection. `draw_frame` calls it from exactly
// where the block used to sit -- last, over the 3D scene and every other HUD
// element, before the bezel. `info.map_open` false still draws NOTHING.
//
// The PURE half of the map -- the aeqd projection and its inverse, the view
// state, the zoom clamp, the follow threshold, the per-zoom declutter -- is
// deliberately NOT here: it lives in `render/bubble_map.h` and
// `render/map_style.h`, which are raylib-free and therefore linkable into
// `seads_tests`. This TU is only in the `seads` app target, so anything a
// gate must be able to execute cannot live in it (the .blend re-export
// lesson, CLAUDE.md).

#include "render/draw.h"  // FrameInfo (and, through it, sim::SimState/params)

namespace render {

// Draws the full-screen M-key chart over the current frame. Called by
// `draw_frame` only when `info.map_open` is true.
void draw_map_screen(const sim::SimState& state,
                     const sim::AircraftParams& params, const FrameInfo& info);

}  // namespace render
