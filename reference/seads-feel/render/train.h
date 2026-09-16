#pragma once
// CC2 — slag-pot trains (Living Copper Cliff, docs/copper_cliff_plan.md). An electric
// trolley loco hauling a string of open slag pots along a hand-authored CLOSED spur
// (smelter -> dump -> return) draped on the planet's own height field. Each car's pose
// is a PURE function of a wrapped phase the app derives from t_cel in DOUBLE (render/
// never sees raw t_cel). Mono steel (the S1 post silvers it — the CC3 hot slag is the
// one chroma exception). render/ reads state, writes nothing, reads no clock.
#include <glm/vec3.hpp>
#include <vector>

#include "raylib.h"
#include "render/sphere_param.h"  // HeightField (radius_at — the shared drape)

namespace render {

struct SlagRidge;  // CC4 slag ridge frame (render/slag.h) — the rails ride its crest

// FELT dials from config/world.toml [train] via draw.cpp. Mono steel (low-sat).
struct TrainLook {
    int pots = 8;                       // slag-pot CARS trailing the loco (2 pots each)
    float car_gap_m = 24.0f;            // spacing between car centers (CC5 F8: pour cadence)
    float lift_m = 1.4f;                // drape above the terrain (rail head)
    float tip_span_m = 14.0f;           // CC5: arc-length each pot tips over the dump edge
    glm::vec3 color{0.46f, 0.46f, 0.48f};  // mono steel (low-sat -> silver)
    float ambient = 0.34f, diffuse = 0.72f;
};

struct TrainRenderer {
    bool ok = false;
    Shader shader{};
    Material mat{};
    Mesh loco{}, potcar{}, pot{};  // loco / static pot-car (deck+cradle) / the tipping pot
    Mesh rails{};                  // CC6 static draped rails + ties (a track to move on)
    glm::dvec3 rails_origin{};     // local anchor the rails mesh is baked relative to
    // CC6: the loco + pot are reference-true glTF (assets/coppercliff/*.glb, blender-
    // hero-forge); loaded models are kept alive so unload frees them (else procedural).
    Model loco_model{}, pot_model{};
    bool loco_loaded = false, pot_loaded = false;
    TrainLook look;
    const HeightField* hf = nullptr;     // for per-frame drape (cars move along s)
    std::vector<glm::dvec3> pts;         // Chaikin-smoothed CLOSED spur dirs (unit)
    std::vector<double> cum;             // cumulative draped arc-length; cum[M] = L
    double L = 0.0;                      // total loop length [m]
    // CC5 dump coupling: the crest dump point + the pour-face tip direction (set only
    // when a ridge routes the crest; drives the pot tip + the lava's single clock).
    bool coupled = false;
    double s_dump = 0.0;                 // arc-length of the dump point on the spur (F7)
    glm::dvec3 dump_face{0.0, 0.0, 0.0}; // ridge pour-face dir (the pots tip toward it)
    int loc_sun = -1, loc_color = -1, loc_amb = -1, loc_diff = -1;
};

// Build the train. `ridge` (nullable) routes the dump leg along the CC4 crest (F2) and
// arms the CC5 pot-tip + lava coupling; nullptr = the plain CC2 closed spur.
TrainRenderer build_train_renderer(const HeightField& hf, const TrainLook& look,
                                   const SlagRidge* ridge = nullptr);
void unload_train_renderer(TrainRenderer& r);

// Draw the train. `phase` = frac(t_cel*rate) in [0,1), computed in DOUBLE by the app
// (finished phase, never raw t_cel); the loco head is at s = phase*L. Opaque, drawn
// after the buildings. `ridge` (nullable) lifts the dump-leg rails onto the CC4 slag
// ridge crest (F3 single-source; nullptr = plain terrain drape). No-op if !ok.
void draw_train_renderer(TrainRenderer& r, double phase, const glm::vec3& sun_dir,
                         const glm::dvec3& eye, const SlagRidge* ridge = nullptr);

// CC5 lava coupling (Fable F1): the pour phase [0,1) DERIVED FROM THE TRAIN HEAD so the
// pots FEED the pour (single clock = s_head). Returns 0 (FS-dark) when no pot is at the
// dump. `train_phase` = frac(t_cel*rate). If the train isn't coupled to a ridge, returns
// a negative sentinel so the caller falls back to the standalone CC3 pour cycle.
double train_dump_phase(const TrainRenderer& r, double train_phase);

}  // namespace render
