#pragma once

#include <glm/vec3.hpp>

#include "render/draw.h"  // FrameInfo (carries the T1 net pointer)

// T2 app-side upload/draw for the Errington-tunnel greybox (docs/tunnel_staging
// .md). Mirrors render/buildings.cpp: a file-scope lazily-built renderer
// (uploaded on the first frame the net is present), a mono depth-cued shader,
// drawn two-sided AFTER the opaque slag in draw_frame. The PURE geometry lives
// in render/tunnel_mesh.* (seads_render_core); THIS is the raylib seam (the
// seads target only).

namespace world {
struct HeightField;  // the collar rim + depth-cue surface source (may be null)
}  // namespace world

namespace render {

// Draw the tunnel interior greybox, eye-relative. No-op when info.tunnel_net is
// null (the lazy build never fires). sun_dir is the world light-travel
// direction (unused for shading beyond a faint local-up key; the interior is
// lit by the depth cue, not the sun). `ground` is the planet heightfield the
// collars drape on and the cue measures depth against (may be null => the bare
// sphere at the mouth radius). Passed from draw.cpp (g_planet.height) so the
// greybox mouths meet the real terrain hole.
// `errington_collar_outer_m` / `murray_collar_outer_m` are the per-mouth collar
// outer radii [m] derived by the caller via render::collar_reach() from the
// terrain grid knobs (Errington skirt / Murray open-pit bowl wall); 0 = legacy
// fallback. `terrain_cell_arc_m` (T5d) is the terrain grid cell size [m]
// (render::terrain_cell_arc()) — the radial ring spacing for the
// terrain-hugging collar/bowl-rim band (kills the portal strobe); 0 = legacy
// single-band fallback.
void draw_tunnel(const glm::dvec3& eye, const glm::vec3& sun_dir,
                 const FrameInfo& info, const world::HeightField* ground,
                 double errington_collar_outer_m = 0.0,
                 double murray_collar_outer_m = 0.0,
                 double terrain_cell_arc_m = 0.0);

// Draw ONLY the additive gaslamp glows for the tunnel (T4b). Must be called
// AFTER all opaque depth-writers (aircraft/trees/etc.) in the same pass
// discipline as the street-lamp draw_lamps() — additive, depth-test ON,
// depth-write OFF. No-op when info.tunnel_net is null or the mesh was not yet
// built (the lazy build fires on the first draw_tunnel() call). The `eye` and
// `sun_dir` are the same values passed to draw_tunnel().
void draw_tunnel_lamps(const glm::dvec3& eye, const glm::vec3& sun_dir,
                       const FrameInfo& info);

// Free the GPU meshes/shader (called at shutdown alongside the other
// renderers). Safe to call when nothing was built.
void unload_tunnel();

}  // namespace render
