#pragma once

#include <vector>

#include "render/camera.h"
#include "render/vortex.h"
#include "sim/params.h"
#include "sim/state.h"

// raylib drawing. Reads state, NEVER writes (SPEC §5) — everything comes in
// by const ref and nothing flows back. This is the double->float seam: the
// world is re-based to the camera eye in DOUBLE first (positions become
// O(chase distance..horizon), not O(R)), then cast — a float cast of raw
// 15 km coordinates would quantize at ~2 mm and shimmer.

namespace render {

// One-time GPU-independent setup (clip planes for a 15 km planet — raylib's
// default far plane is 1000 m). Call once after InitWindow.
void init_draw();

// Cubesphere build parameters, sourced from config/world.toml (no bare geometry
// numbers in render/ — docs/world_build_plan.md §1). Set ONCE at startup via
// set_planet_build_params() BEFORE the first frame builds the planet. The
// defaults reproduce the pre-config planet, so an unset caller (tests/harness,
// which never draw) is unchanged.
struct PlanetBuildParams {
    double relief_scale = 600.0;  // metres of radial displacement at full-white
    int subdiv = 200;             // cubesphere verts per face edge
    double u_offset = 0.806;      // longitude alignment of the map (fraction)
    int dem_blur_radius = 3;      // DEM softening (coupled to relief_scale)
};
void set_planet_build_params(const PlanetBuildParams& p);

// Per-frame HUD context the draw layer can't derive from state alone. The
// physics readouts (V/ALT/G/AoA/bank) are NOT here — draw_frame computes them
// through the shared render::flight_readout so there is one correct-frame
// source (SPEC §12; HARNESS §1 row 5). The nose marker is derived from state.
struct FrameInfo {
    int fps = 0;
    bool raw_mode = false;  // debug raw-stick mode (no instructor, no reticle)
    bool freelook = false;  // reticle nests in the cursor ("aim locked" cue)
    // S-reticle: the DISPLAY-EASED reticle direction (render::reticle_smooth
    // — the true aim plus a hard-capped <= quant_px*sensitivity display lag
    // that hides the integer-mouse staircase). NOT the raw aim: any future
    // consumer needing the TRUE aim must read loop.aim.forward(), never this
    // (this field's only legitimate reader is the reticle projection).
    glm::dvec3 reticle_dir{0.0, 0.0, -1.0};  // world unit; reticle draw only
    // Current vertical FOV [deg] (RMB-zoom eases this from kChaseFovyDeg toward
    // kZoomFovyDeg). draw_frame feeds it to the Camera3D, the off-center lens
    // frustum, AND the reticle/nose/pipper projection — ONE source, so the
    // reticle can never drift from the scene it centers against. The caller
    // computes the matching lens_shift_ndc at this SAME fov.
    double fovy_deg = kChaseFovyDeg;
    // Vertical lens shift (NDC, SPEC §9.2 framing): slides the 3D scene AND the
    // reticle/nose overlay down together so the resting reticle rests at screen
    // center. Computed by the caller via render::lens_shift_ndc; 0 = no shift.
    double lens_shift_ndc = 0.0;
    // Target drones (SPEC §0 S8-drone): the interpolated bandit states, each
    // drawn as an enemy-tinted aircraft (at drone_scale size) in the player's
    // view. The drones never touch the camera (it always frames the player).
    // Empty by default => no drones drawn (the player draw is unchanged).
    std::vector<sim::SimState> drones_draw{};
    double drone_scale = 1.0;
    // Lead-angle gunsight (SPEC §0 S8-drone): the pipper + time-on-target HUD.
    // draw_frame READS the meter snapshot (computed once per sim tick in
    // app::tick, single source) and only PROJECTS + draws it — no render-time
    // solve. gunsight_lead is the world lead direction; on_target => green.
    bool gunsight_active = false;      // draw the gunsight (instructor mode)
    bool gunsight_has_target = false;  // a bandit is engaged (draw the pipper)
    glm::dvec3 gunsight_lead{0.0, 0.0, -1.0};  // world lead dir (where to aim)
    // Index into drones_draw of the ENGAGED bandit (the one the lead pipper is
    // solving), or -1 = none. 1:1 with dw.meter.engaged_index (drones_draw is
    // built 1:1 from dw.drones). The S-caret skip is BY THIS INDEX (P1-2),
    // never by a lead-cone angle match — a high-crossing engaged bandit's
    // direction can diverge from the lead line, and an unengaged bandit can lie
    // near it.
    int gunsight_target_index = -1;
    bool gunsight_on_target = false;  // nose on the solution + in range (green)
    double tot_frac = 0.0;            // time-on-target fraction [0,1] (HUD %)
    double tot_range = 0.0;           // range to the engaged bandit [m]
    // MB-7c energy legibility (read-only, S8 gunsight firewall). All
    // defaulted so an unset caller (smoke/tests) draws the pre-7c frame.
    // AoA dial limits (from controller/aircraft config, set once by the
    // caller): the protection clamp's aoa_max [rad] each side, and the
    // plant's stall alpha Cl_max/Cl_alpha [rad]. aoa_max <= 0 hides the dial.
    double aoa_max = 0.0;      // [rad] protection limit, positive side
    double aoa_max_neg = 0.0;  // [rad] protection limit, negative side (>0)
    double stall_alpha = 0.0;  // [rad] plant stall (red arc start)
    // Speed trend [m/s^2], display-smoothed by the caller (cosmetic — off
    // every control path): the HUD's chevron cue for "gaining/bleeding".
    double speed_trend = 0.0;
    // MB HUD status stack: the COMMANDED devices (main.cpp F-cycle / G-toggle
    // latches), display-only — positions come from state.flap/state.gear (the
    // slewed plant truth). Same display-state precedent as raw_mode/freelook.
    int flap_mode = 0;           // commanded detent: 0 CLEAN/1 COMBAT/2 LANDING
    bool gear_down_cmd = false;  // commanded gear latch
    // Wingtip vortex trails (MB-7c iii), owned/updated by the caller;
    // nullptr = none drawn.
    const VortexTrails* vortices = nullptr;
    // S-cues (comfort program, [comfort] in controller.toml): two peripheral
    // orientation cues, pure HUD, DEFAULT OFF. alpha 0 => the draw is SKIPPED
    // entirely (strict superset — the shipped default frame is unchanged).
    // draw_frame reads local_up = normalize(state.position) and the bank from
    // the shared flight_readout, both fresh, never cached (SPEC §6.1).
    double cue_horizon_alpha = 0.0;     // [0..1] ghost-horizon opacity; 0 = OFF
    double cue_horizon_gap_frac = 0.3;  // [0..1) center exclusion disk radius
    double cue_bank_arc_alpha = 0.0;    // [0..1] bank-arc opacity; 0 = OFF
    // S-carets (comfort program, REC-2): screen-edge threat indicators for
    // OFF-SCREEN drones. alpha 0 => the caret pass is SKIPPED entirely (strict
    // superset — shipped default frame unchanged). Drawn for drones in
    // drones_draw not visible in the frustum; the engaged pipper target (which
    // has its own diamond) is skipped. Pure HUD read of drones_draw + pose.
    double cue_caret_alpha = 0.0;  // [0..1] edge-caret opacity; 0 = OFF
    // REC-6: the STYLIZED COCKPIT FRAME — a screen-anchored peripheral canopy
    // interior (the steady-state REST FRAME, docs/comfort_research.md §3.3).
    // alpha 0 => the draw is SKIPPED entirely (strict superset). Cosmetic HUD,
    // off every aim/camera/control path. Ships 0.35 (Chad wants to SEE it).
    double cue_cockpit_alpha = 0.35;  // [0..1] cockpit-frame opacity; 0 = OFF
};

void draw_frame(const sim::SimState& state, const sim::AircraftParams& params,
                const CameraPose& pose, const FrameInfo& info);

}  // namespace render
