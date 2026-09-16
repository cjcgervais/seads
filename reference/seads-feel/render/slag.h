#pragma once
// CC3 — slag pour (Living Copper Cliff, docs/copper_cliff_plan.md). The marquee: a
// dark procedural slag-heap MOUND at the Copper Cliff dump, a slag POT that tips at
// the rim, and a molten LAVA cascade flowing down the face as fiery orange/vermilion
// rivers — the iconic Sudbury night spectacle. The lava is the ONE sanctioned WARM-
// CHROMA exception in the B&W world (Chad's ruling), single-sourced heat() emissive
// that survives the S1 split-tone (high chroma passes the sat gate, like the planes).
// Every animation is a PURE function of a wrapped pour phase the app derives from
// t_cel (render/ never sees raw t_cel, reads no clock). The mound/pot are mono.
#include <glm/vec3.hpp>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField (radius_at — the shared drape)

namespace render {

// FELT dials from config/world.toml [slag] via draw.cpp. Mound/pot MONO; the lava is
// the warm-chroma exception (hot->cooling-red->crust, single-sourced in the FS).
struct SlagLook {
    float mound_radius_m = 150.0f;   // (legacy cone dial; the CC4 ridge ignores it)
    float mound_top_r_m = 62.0f;     // (legacy)
    float mound_height_m = 45.0f;    // CC4: RIDGE crest height above the surrounding ground
    float ridge_length_m = 700.0f;   // CC4: crest length along the dump edge ("proper length")
    float crest_width_m = 36.0f;     // CC6: flat mesa top width carrying the track (widened)
    float face_angle_deg = 35.0f;    // CC4: MEAN pour-face slope (slag angle of repose)
    int benches = 3;                 // CC6: terrace steps down the pour face (1 = planar, legacy)
    glm::vec3 mound_color{0.075f, 0.075f, 0.08f};  // dark matte slag (mono; CC6 darkened)
    float ambient = 0.30f, diffuse = 0.55f;      // mono mound/pot shading
    int rivers = 5;                  // molten streaks braiding the fan down the face
    float river_halfwidth_m = 5.0f;  // each river's half width at the source (widens down)
    float glow = 1.0f;               // emissive lava gain
    float night_boost = 0.7f;        // extra emissive when the sun is down
    float pour_rate_hz = 0.02f;      // pour cycles per s of t_cel (~one every 50 s)
    float lift_m = 1.5f;             // lava lift above the mound face (anti z-fight)
};

// CC4 — the slag RIDGE frame (Living Copper Cliff dump expansion). A LINEAR man-made
// range: a flat crest carrying the CC2 track + one steep pour face at the angle of
// repose. Built ONCE from the LOCKED aeqd dirs + the height field; SINGLE SOURCE for
// the ridge MESH (slag.cpp) AND the CC5 track crest-drape (train.cpp), so the rails sit
// exactly on the crest (Fable F3 — a constant lift would fork/float them). Local mesh
// coords are X=face (pour, +NW), Y=up (radial), Z=axis (crest line, along the spur).
struct SlagRidge {
    bool ok = false;
    const HeightField* hf = nullptr;
    glm::dvec3 anchor{}, up{}, axis{}, face{}, side{}, anchor_pos{};
    double Ra = 0.0;       // radius_at(anchor) — the ridge base radius
    float height = 45.0f;  // crest height above the base terrain
    float half_len = 350.0f, crest_hw = 12.0f;  // half crest length / half crest width
    float crest_flat_half = 110.0f;  // half-length of the FLAT crest; beyond it the height
                                     // ramps gradually to 0 at the ends (the climb ramp +
                                     // the shallow descent the train runs — Chad 2026-07-13)
    float face_run = 64.0f, back_run = 108.0f;  // horizontal run of the front/back faces
    // CC6 — the SINGLE-SOURCE benched pour-face profile (Fable): a normalized curve
    // [0,1]^2 walked by a parameter t at knots t_k = k/(nk-1). face_xf = cumulative
    // horizontal run fraction (STRICTLY increasing -> invertible for the drape),
    // face_yf = cumulative vertical drop fraction (non-decreasing). The MESH front
    // chain, the LAVA cx/cy, AND ridge_lift_at's front branch ALL sample these knots
    // (per-consumer vertical scale) so the terraced face can never fork/float.
    static constexpr int kMaxFaceKnots = 12;
    int face_nk = 2;
    float face_xf[kMaxFaceKnots]{0.0f, 1.0f};
    float face_yf[kMaxFaceKnots]{0.0f, 1.0f};
};

// Drop fraction [0,1] at horizontal run-fraction u=[0,1] down the pour face (inverts
// face_xf -> face_yf; the drape's single source, shared with the mesh/lava). Pure.
float face_drop_at_run(const SlagRidge& g, float u);
// (xf,yf) at profile parameter t=[0,1] (the mesh/lava sample this). Pure.
void face_profile_at(const SlagRidge& g, float t, float& xf, float& yf);

// Build the ridge frame from the locked Copper Cliff dirs + the height field. Pure
// (no raylib/GL) so train.cpp can share it for the crest drape.
SlagRidge make_slag_ridge(const HeightField& hf, const SlagLook& look);

// Crest height [m] above the base terrain at station z (metres along the crest axis
// from the anchor): endTaper (ramps to 0 at both ends) * gentle terrace noise. The
// SINGLE height function the mesh, the lava, and the track drape all sample.
float ridge_crest_h(const SlagRidge& g, float z);

// Extra height [m] the ridge adds above the BASE terrain at world unit dir `d` (0 off
// the ridge footprint, ramping down the faces). CC5 track crest-drape reads this and
// adds it to radius_at(dir)+lift_m so the rails ride the crest.
double ridge_lift_at(const SlagRidge& g, const glm::dvec3& d);

struct SlagRenderer {
    bool ok = false;
    Shader mono_sh{};   // dark mono mound + pot (facet-normal, like the buildings)
    Shader lava_sh{};   // warm-chroma emissive cascade
    Material mono_mat{}, lava_mat{};
    Mesh mound{}, lava{}, pot{};
    SlagLook look;
    SlagRidge ridge{};  // CC4 ridge frame (up=radial, X=face pour, Z=axis crest)
    // mono uniforms
    int m_sun = -1, m_color = -1, m_amb = -1, m_diff = -1;
    // lava uniforms
    int l_phase = -1, l_glow = -1, l_night = -1, l_sun = -1, l_eye = -1;
};

SlagRenderer build_slag_renderer(const HeightField& hf, const SlagLook& look);
void unload_slag_renderer(SlagRenderer& r);

// Draw the ridge + (optional) static tipping pot + lava cascade. `phase` = the pour
// phase in [0,1) (finished, DOUBLE by the app — CC5 derives it from the train head so
// the pots feed the pour). sun_dir = world light-travel dir (the night boost).
// `draw_static_pot` = false when the CC5 train supplies the tipping pot (avoid two).
// Opaque; drawn after the buildings. No-op if !ok.
void draw_slag_renderer(SlagRenderer& r, double phase, const glm::vec3& sun_dir,
                        const glm::dvec3& eye, bool draw_static_pot = true);

}  // namespace render
