#pragma once
// Street-lamp night point lights (stereoscope-sudbury). Glowing points lining
// the town streets, baked as unit dirs + a height in render/sudbury_gis.gen.h
// (kSudburyLamps). Drawn as ADDITIVE camera-facing billboard glows at a
// ~constant screen size (so they stay visible from altitude — the "town from
// above at night" read), night-gated by sun elevation. render/ reads state,
// writes nothing, reads no clock — house law.
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField (radius_at — the shared drape)

namespace render {

// FELT dials, from config/world.toml [streetlamps] via draw.cpp (no bare look
// constants in GLSL). The color is near-white (low-sat) so the S1 post keeps it
// silver; a warm tint is a 2nd chroma (Chad's call).
struct LampLook {
    glm::vec3 color{1.0f, 0.98f,
                    0.95f};    // lamp glow tint (near-white; low-sat -> silver)
    float brightness = 0.55f;  // soft-halo (aura) additive strength
    // Two-part glow: the soft halo above + a tight hot CORE (the bulb) so lamps
    // read as ACTUAL lit points up close, not vague hazes — WITHOUT the
    // dense-street blowout a bright halo would cause (the core is
    // tight-footprint). See lights.cpp kLampFS.
    float core_bright = 1.0f;  // hot-core additive strength (the bulb)
    float core_sharp = 34.0f;  // core falloff sharpness (higher = tighter bulb)
    // The glow has a WORLD radius (grows on screen as you approach, so a lamp
    // reads as a real glow up close) with a MIN pixel floor (so distant lamps
    // stay a visible dot that aggregates into lit streets). screen = max(world,
    // min_px).
    float size_m = 3.5f;  // world glow radius (m) — the up-close size
    float min_px = 2.5f;  // far floor: never smaller than this on screen (px)
    // T10.1: bypass the sun-elevation night gate (always lit). The tunnel
    // ceiling ember field spans a FULL sphere, so no single sun dir reads night
    // for every lamp; a contained mine is lit day and night regardless. 0 =
    // the normal gate (street lamps); 1 = always lit.
    float night_bypass = 0.0f;
};

struct LampRenderer {
    bool ok = false;
    Shader shader{};
    Material mat{};
    std::vector<Mesh> meshes;  // batched (<=16k lamps/mesh for ushort indices)
    LampLook look;
    int loc_sun = -1, loc_sizem = -1, loc_minpx = -1, loc_color = -1,
        loc_bright = -1, loc_core_bright = -1, loc_core_sharp = -1,
        loc_night_bypass = -1;  // T10.1
};

LampRenderer build_lamp_renderer(const HeightField& hf, const LampLook& look);

// Parallel entry point (T4b tunnel gaslamps): build a lamp renderer from an
// EXPLICIT world-absolute point set (not the baked street-lamp table), reusing
// the IDENTICAL additive-glow shader. `positions` are metres; the night gate
// (lamp radial vs sun) still applies — underground lamps read `vUp` from their
// own position, which is well below the surface but keeps the same silver tone,
// and the tunnel set is drawn with a bypassed night gate at the app seam.
LampRenderer build_lamp_renderer_from_points(
    const std::vector<glm::dvec3>& positions, const LampLook& look);

void unload_lamp_renderer(LampRenderer& r);

// Draw the lamp glows. Additive, depth-test ON (terrain occludes) but
// depth-write OFF; run after the opaque planet/buildings. sun_dir = world
// light-travel dir (night gate); fovy_deg + viewport_h size the constant-pixel
// glow.
void draw_lamp_renderer(LampRenderer& r, const glm::vec3& sun_dir,
                        const glm::dvec3& eye, float fovy_deg, int viewport_h);

}  // namespace render
