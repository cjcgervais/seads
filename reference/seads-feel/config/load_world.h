#pragma once

#include <string>

// TOML -> WorldParams (docs/world_build_plan.md §1, §5): the single source for
// every WORLD/ART threshold. Lives in config/ (with the data), never in world/
// or render/ (no bare numbers there). Strict like load_aircraft /
// load_scenario: every key present, every value sane, or it throws. Angles
// convert from degrees (the config boundary) to radians here, once.
//
// SEPARATE from aircraft.toml / controller.toml — the map never reads those. R
// is NOT here: the planet radius is the sim's (passed as params.R),
// single-source, never mirrored (the H1 lesson; §1 "R is single-source, not
// mirrored").

namespace cfg {

struct WorldParams {
    struct {
        double silver = 0.0;  // base sky luminance [0,1]
        double gradient =
            0.0;  // zenith->horizon 2-stop gradient strength [0,1]
    } sky;
    struct {
        double reflectivity = 0.0;       // mirror strength of the sky [0,1]
        double sparkle_sharpness = 0.0;  // specular pow() exponent (>0)
        double min_lake_m = 0.0;         // authored lake floor (m, >0)
    } water;
    struct {
        double relief_scale_m =
            0.0;  // radial displacement at full-white height (m)
        double field_scale_m = 0.0;  // farmland/bush patchwork cell (m)
        bool procedural = false;     // A/B: false = Earth DEM, true = Sudbury
    } ground;
    struct {
        int subdiv = 0;           // cubesphere verts per face edge (2..256)
        double u_offset = 0.0;    // longitude alignment of the map (fraction)
        int dem_blur_radius = 0;  // DEM softening (>=0; coupled to relief)
    } planet;
    struct {
        double contrast = 0.0;  // tone-curve steepness (>0)
        double lift = 0.0;      // black lift [0,1)
        double grain = 0.0;     // grain amount [0,1) (minimal, Chad)
    } tone;
    struct {
        double margin_rad = 0.0;  // horizon over-cull pad (radians, >=0)
    } horizon_cull;
    struct {
        int seed = 0;                  // determinism anchor (>=0)
        double furniture_scale = 0.0;  // landmark up-scale (>=1)
    } scatter;
    struct {
        double frame_budget_ms = 0.0;  // (>0)
        int max_draw_calls = 0;        // instanced ceiling (>=1)
    } perf;
};

WorldParams load_world_toml(const std::string& path);

}  // namespace cfg
