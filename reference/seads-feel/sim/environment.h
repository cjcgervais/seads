#pragma once

// Kernel-v4 — the world as a field argument (MASTER_PLAN §3.C). An OPTIONAL
// argument to the pure kernel: EVERY field null => bit-identical to the frozen
// v3 kernel (the all-null bit-identity proof — goldens must not move until a
// field is deliberately activated). The nullable-pointer pattern is this
// codebase's extension idiom (GunWorld); step() stays a pure function of data.
//
// Members are pointers, so this header only forward-declares the field types —
// the ground query dereferences world::HeightField in the .cpp (Phase 1/R4);
// the atmosphere/gravity/tunnel fields are Phase 2/3 and don't exist yet.
//
// ★ terrain-clip T2: the ONE exception to "members are pointers" is
// ground_facet_fn below — a std::function, because the thing it carries lives
// in render/ and the kernel may not include render/ (the same reason
// world::SnowpackField::facet_radius_fn is a std::function, world/snowpack.h).

#include <functional>

#include <glm/glm.hpp>

namespace world {
struct HeightField;  // Phase 1 — null = the bare sphere at R (today's crash
                     // rule)
struct BuildingColliders;  // R4f — null = buildings are ghosts (today)
struct TunnelNet;          // Phase 3 — null = no tunnels
}  // namespace world

namespace sim {

struct AtmosphereField;  // Phase 2 — null = today's altitude-Gaussian atm_frac
struct GravityField;     // Phase 2 — null = constant g (today)

// R4 touch-and-stick contact dials (config/game.toml [ground]; MASTER_PLAN
// §3.A + Chad's 2026-07-15 rulings: contact lives in the KERNEL, landing v1 is
// touch-and-stick). FELT values are Chad's stick-ruled keys — code reads them,
// never owns them (the tuning contract). These defaults are deliberately
// INERT-STRICT (nothing is landable, no friction): an unconfigured Environment
// cannot silently ship a feel; the strict game.toml loader always overwrites.
// Read iff Environment.ground != nullptr.
struct GroundParams {
    double slope_limit_cos = 1.0;  // min cos(angle), used for BOTH the local
                                   // terrain-slope-vs-up AND body-up-vs-normal
                                   // acceptance at contact [cos(deg at config)]
    double friction = 0.0;         // rolling decel as a fraction of g
    double max_sink_ms = 0.0;      // gentle-contact sink ceiling [m/s]
    double normal_probe_m = 0.0;   // finite-diff tangent step for the terrain
                                   // normal [m] (structural; > 0 when live)
    double contact_height_m = 0.0;  // CG height above the wheels/belly [m] —
                                    // the contact surface is terrain + this
                                    // (a CG on the terrain buries the hull;
                                    // Chad's first fly, 2026-07-15)
    // R4g wheel brakes (Chad's fly ask): EXTRA rolling decel, fraction of g,
    // scaled by Inputs.wheel_brake in the GROUNDED regime. Deliberately MAY
    // exceed T_max/(m·g) — holding brakes at full throttle holding the plane
    // IS the runup; the loader's accelerate check covers friction alone.
    double brake_friction = 0.0;
    // R4g grounded roll alignment — REVERTED BY CHAD (fly 2026-07-15: wings
    // must be free to bank and DAMAGE on strike, not be held level). The
    // mechanism stays dormant (0 = off, the shipped value); the consequence
    // dials below replace it. [rad/s]
    double roll_align_rate = 0.0;
    // R4-FLY-5 grounded CONSEQUENCE dials (Chad's rulings, 2026-07-15):
    // Tire/tailwheel rotational resistance [1/s]: with the q_att_floor
    // removed on the ground (controls need AIRSPEED — rudder dead at a
    // stop), residual rotation would otherwise never decay at rest.
    double ground_ang_damp = 0.0;
    // Wingtip strike geometry: half the wingspan [m] (structural, the
    // airframe's; a tip below the terrain surface = wing_strike event).
    double wing_halfspan_m = 0.0;
    // Ground loop ("turn too fast -> rotate and crash"): lateral acceleration
    // limit as a fraction of g while rolling; exceeded => crashed.
    double ground_loop_lat_g = 0.0;
    // Nose-over ("braking too fast -> pitch down, the prop breaks"): hard
    // braking pitches the nose down at brake_pitch_rate [rad/s per unit
    // brake decel fraction of g]; when the nose drops below
    // prop_strike_pitch_rad against the terrain plane => prop_strike event.
    double brake_pitch_rate = 0.0;
    double prop_strike_pitch_rad = 0.0;
    // R4-FLY-6 (Chad: "slow braking still tips forward"): the nose-dip is an
    // inertial moment, so it fades with the momentum the brakes are killing —
    // dip rate x min(1, ground_speed / this). Full dip at/above this speed,
    // ~0 at a crawl (a careful rolling stop cannot nose over). 0 = the
    // unscaled R4-FLY-5 dip, bit-identical. [m/s]
    double noseover_full_speed_ms = 0.0;
    // R4f building collision: prism-hit acceptance band. base_margin_m is the
    // LOWER bound slack (base - margin <= r): an equiv-area collider circle
    // can overhang a terrain dip its center doesn't see (~3% ramps x 127 m
    // max radius), and a grounded rollout in that dip must not false-crash.
    // inflate_m widens every footprint radius (the equiv-area radius under-
    // covers long/thin buildings + the wingspan is not modeled).
    double obstacle_inflate_m = 0.0;
    double obstacle_base_margin_m = 0.0;
    // T3 (spline-space collision, docs/tunnel_staging.md): the DEEP-PENETRATION
    // wall-strike floor [m]. Landing acceptance is a SURFACE law — it snaps the
    // CG radially to r_s = radius_at(up) + contact_height_m. A state whose
    // penetration (r_s - r) exceeds this can only be reached by emerging from a
    // tunnel VOLUME into solid rock (a plane grazing the tube/egg/chamber wall
    // deep underground with a level attitude and near-zero radial sink); the
    // acceptance would otherwise TELEPORT it ~2000 m up onto the surface, alive
    // and grounded (probed: r 13303 -> 15302 at a chamber-wall graze). It is a
    // wall strike: next.crashed = true before any acceptance. 0 = off, the T1
    // behavior bit-identically (the clause never fires). Must be UNREACHABLE by
    // a legitimate per-tick contact approach: max legit penetration is one tick
    // of sink (~max_sink_ms*dt, sub-metre) plus the sub-metre bilinear kink —
    // well under contact_height_m + a few metres. Loaded range-checked in
    // config/load_game.cpp against contact_height_m.
    double deep_penetration_m = 0.0;
    // ★ terrain-clip T2 — THE ONE DIAL: how much of the aircraft's crash
    // surface is the surface the EYE is shown.
    //
    //   r_s = mix(hf.radius_at(up), facet(up), facet_contact) + contact_height_m
    //
    // 0.0 = IDENTITY BY BRANCH: Environment::ground_facet_fn is not called and
    // the arithmetic is literally `hf.radius_at(up) + gp.contact_height_m` —
    // bit-identical to the pre-T2 kernel (the frozen-kernel law; every golden,
    // every headless test, every null-env path is on this arm by default).
    // 1.0 = THE SLED'S LAW: collision == what is drawn, the same pinning
    // [snowpack] hf_faceted_ground already gives the snowmachine.
    //
    // WHY (docs/terrain_clip/CLIP_INSTRUMENT.md, T1): the crash surface is the
    // bilinear DEM FIELD (~11.5 m texel) while the drawn planet is a ~59 m mesh
    // facet. Where terrain is steeper than one mesh cell the drawn chord rides
    // ABOVE the field — measured +66.7 m inside the Onaping pump area, 3.8 % of
    // the ground within 3 km of it — and the airframe is legally airborne
    // INSIDE visible rock (Chad: "I was able to fly into the small earth and
    // fly inside it"). The mirror sign (crash shell above the drawn ground =
    // the invisible wall) dies with the same dial.
    //
    // NOT a second height source: the facet is the SAME field read through the
    // mesh's own interpolation (the H1 anti-fork). Loader-bounded [0, 1];
    // SEADS_FACET_CONTACT replaces the config value (the kill, app/main.cpp).
    double facet_contact = 0.0;
};

struct Environment {
    const world::HeightField* ground = nullptr;
    const AtmosphereField* atm = nullptr;
    const GravityField* grav = nullptr;
    const world::TunnelNet* tunnels = nullptr;
    // R4f: building collision prisms (world/buildings.h). Read iff ground is
    // ALSO live (the base radius comes from the same height field — one crash
    // surface, never a fork). Null = buildings are ghosts (pre-R4f).
    const world::BuildingColliders* obstacles = nullptr;
    GroundParams ground_params{};  // consumed iff ground != nullptr

    // ★ terrain-clip T2: the DRAWN mesh-facet radius sampler —
    // render::facet_radius_at(*ground, dir, planet.subdiv, planet.tiles).
    // INJECTED by the app layer (app/main.cpp), exactly the seam and exactly
    // the function world::SnowpackField::facet_radius_fn already carries for
    // the sled: sim/ is render-free by law, and app/ is the one place holding
    // both env.ground and the shipped subdiv/tiles the mesh was actually built
    // at. NOT a second height source — the same HeightField, the mesh's own
    // interpolation (the H1 anti-fork).
    //
    // EMPTY (every headless test, every golden, every env built without a
    // render layer) = ABSENT: sim/ground.h falls back to hf.radius_at, i.e.
    // the pre-T2 field crash surface, whatever ground_params.facet_contact
    // says. So a test that never injects can never accidentally be on the
    // armed arm.
    std::function<double(glm::dvec3)> ground_facet_fn;

    // R5 (Fable-BEFORE P2-1): the app gates its env pointer on THIS, never
    // on any single field — the R4 gate on ground alone would have silently
    // no-opped a gravity-only activation, and R6 (atm) / R11 (tunnels) would
    // have re-armed the same trap.
    bool any_live() const {
        return ground != nullptr || atm != nullptr || grav != nullptr ||
               tunnels != nullptr || obstacles != nullptr;
    }
};

}  // namespace sim
