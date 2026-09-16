// S-airdome (docs/bubble_atmosphere_spec.md §1.1) — pins render::air_at
// bit-for-bit (1e-9 tolerance, double precision both sides) against the LIVE
// sim::atm_frac_at over a scatter of >=200 sample points spanning bubble
// cores, both soft edges (horizontal + ceiling), the vacuum gap between two
// ellipse bubbles, the deck, high vacuum, and the antipode. The taper is
// STRUCTURALLY disabled (atm_taper_sigma = 0) so sim::atm_frac_at already
// equals the spatial factor with no division needed (spec §1.1's "WITHOUT the
// altitude taper" — turning the taper off makes atm_frac(alt,p) == 1.0
// identically, so atm_frac_at(pos,&env,p) IS the spatial union factor).
//
// Plus a companion structural leg (the rig-A source-validator precedent):
// kAirFieldGLSL contains the ellipse r_eff denominator form and the
// complement-product union — the numeric cross-check that CAN run headlessly
// (render::air_at itself) is pinned above; the GLSL transliteration's
// side-by-side correctness is NOT independently verified by a GL-less test,
// stated honestly here and in the deliverable report.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "render/air_field.h"
#include "render/air_field_glsl.h"
#include "sim/aero.h"
#include "sim/environment.h"
#include "sim/fields.h"
#include "sim/params.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

// A small, arbitrary (not axis-aligned) two-bubble world: one circular, one
// true ellipse, well-separated so a real vacuum gap exists between them —
// plus the deck. Mirrors sim::AtmosphereField -> render::AirField by hand
// (never by a shared constructor — the whole point is two INDEPENDENT
// callers agreeing).
struct World {
    sim::AircraftParams p;
    sim::AtmosphereField af;
    sim::Environment env;
    render::AirField rf;
};

World make_world() {
    World w;
    w.p.R = 15000.0;
    w.p.atm_taper_sigma = 0.0;  // structurally off (spec §1.1's "without the
                                // altitude taper" — see file banner)
    w.p.atm_taper_alt = 8000.0;

    w.af.deck_agl_m = 120.0;
    w.af.deck_soft_m = 200.0;

    sim::AtmosphereField::Bubble circular;
    circular.center_dir = glm::normalize(glm::dvec3(0.3, 0.9, -0.2));
    circular.ground_radius_m = 6000.0;
    circular.ceiling_m = 4000.0;
    circular.edge_soft_m = 1200.0;
    circular.ceil_soft_m = 600.0;
    // major_axis/minor_radius_m default => the legacy circular branch.
    w.af.bubbles.push_back(circular);

    sim::AtmosphereField::Bubble ellipse;
    // Center well away from the circular bubble (>2x the sum of radii, arc
    // distance) so a real vacuum gap exists between them.
    ellipse.center_dir = glm::normalize(glm::dvec3(-0.7, -0.5, 0.4));
    ellipse.ground_radius_m = 8000.0;  // semi-major `a`
    ellipse.ceiling_m = 5000.0;
    ellipse.edge_soft_m = 1000.0;
    ellipse.ceil_soft_m = 500.0;
    // major_axis must be a unit tangent at center_dir (re-orthogonalized
    // defensively by both samplers anyway).
    const glm::dvec3 cn = glm::normalize(ellipse.center_dir);
    glm::dvec3 arbitrary(0.0, 0.0, 1.0);
    glm::dvec3 tangent = arbitrary - cn * glm::dot(arbitrary, cn);
    if (glm::length(tangent) < 1e-6) tangent = glm::dvec3(1.0, 0.0, 0.0);
    ellipse.major_axis = glm::normalize(tangent);
    ellipse.minor_radius_m = 5000.0;  // semi-minor `b` < `a`
    w.af.bubbles.push_back(ellipse);

    w.env.atm = &w.af;
    w.env.tunnels = nullptr;  // spec §1.1: WITHOUT the tunnel term

    w.rf.enabled = true;
    w.rf.deck_agl_m = w.af.deck_agl_m;
    w.rf.deck_soft_m = w.af.deck_soft_m;
    w.rf.planet_R = w.p.R;
    w.rf.bubble_count = 2;
    // S-domeround: the field-wide roundness dial rides with the geometry
    // copy (app/main.cpp's live-rebuild does the same).
    w.rf.dome_exponent = w.af.dome_exponent;
    auto copy_bubble = [](const sim::AtmosphereField::Bubble& src,
                          render::AirField::Bubble& dst) {
        dst.center_dir = src.center_dir;
        dst.major_axis = src.major_axis;
        dst.a = src.ground_radius_m;
        dst.b = src.minor_radius_m;
        // S-domeround: this slot CARRIES H (dome_h_m), with the same
        // fallback sim/aero.h::atm_frac_at applies -- mirrors app/main.cpp.
        dst.ceiling_m = (src.dome_h_m > 0.0) ? src.dome_h_m : src.ceiling_m;
        dst.edge_soft_m = src.edge_soft_m;
        dst.ceil_soft_m = src.ceil_soft_m;
    };
    copy_bubble(w.af.bubbles[0], w.rf.bubbles[0]);
    copy_bubble(w.af.bubbles[1], w.rf.bubbles[1]);
    return w;
}

// Rotate `base` toward `tangent` (assumed already ⟂ base, unit) by `angle`
// radians — a point on the great circle through base in that tangent
// direction.
glm::dvec3 rotate_toward(const glm::dvec3& base, const glm::dvec3& tangent,
                         double angle) {
    return glm::normalize(base * std::cos(angle) + tangent * std::sin(angle));
}

void check_agreement(const World& w, const glm::dvec3& pos, int& count) {
    const double sim_val = sim::atm_frac_at(pos, &w.env, w.p);
    const double render_val = render::air_at(pos, w.rf);
    INFO("pos = (" << pos.x << ", " << pos.y << ", " << pos.z << ")");
    INFO("sim = " << sim_val << " render = " << render_val);
    REQUIRE(std::abs(sim_val - render_val) < 1e-9);
    ++count;
}

}  // namespace

TEST_CASE(
    "air_field: render::air_at agrees with sim::atm_frac_at over "
    ">=200 sample points") {
    const World w = make_world();
    int count = 0;

    // Per-bubble sweeps: 8 bearings x 8 radial multiples x 5 altitudes = 320
    // per bubble, 640 total — cores, both soft edges (horizontal via the
    // radial sweep, vertical via the altitude sweep), and well outside.
    for (const sim::AtmosphereField::Bubble& b : w.af.bubbles) {
        const glm::dvec3 cn = glm::normalize(b.center_dir);
        glm::dvec3 arbitrary(1.0, 0.0, 0.0);
        glm::dvec3 t1 = arbitrary - cn * glm::dot(arbitrary, cn);
        if (glm::length(t1) < 1e-6)
            t1 = glm::dvec3(0.0, 1.0, 0.0) -
                 cn * glm::dot(glm::dvec3(0, 1, 0), cn);
        t1 = glm::normalize(t1);
        const glm::dvec3 t2 = glm::cross(cn, t1);
        const double edge_r = std::max(
            b.ground_radius_m,
            b.minor_radius_m > 0.0 ? b.minor_radius_m : b.ground_radius_m);
        for (int bi = 0; bi < 8; ++bi) {
            const double bearing = bi * (2.0 * kPi / 8.0);
            const glm::dvec3 tangent =
                std::cos(bearing) * t1 + std::sin(bearing) * t2;
            for (double mult : {0.0, 0.3, 0.7, 0.9, 1.0, 1.1, 1.5, 3.0}) {
                const double arc = mult * edge_r;
                const double angle = arc / w.p.R;
                const glm::dvec3 dir = rotate_toward(cn, tangent, angle);
                for (double alt :
                     {-50.0, w.af.deck_agl_m * 0.5, b.ceiling_m * 0.5,
                      b.ceiling_m + b.ceil_soft_m * 0.5,
                      b.ceiling_m + b.ceil_soft_m * 3.0}) {
                    check_agreement(w, dir * (w.p.R + alt), count);
                }
            }
        }
    }

    // The vacuum gap: the midpoint direction between the two bubble centers
    // (well outside both hard edges + soft skirts by construction), at a few
    // altitudes.
    const glm::dvec3 mid =
        glm::normalize(glm::normalize(w.af.bubbles[0].center_dir) +
                       glm::normalize(w.af.bubbles[1].center_dir));
    for (double alt : {-50.0, 500.0, 3000.0, 9000.0}) {
        check_agreement(w, mid * (w.p.R + alt), count);
    }

    // The deck alone (far from every bubble): breathable near the surface,
    // thinning above deck_agl_m + deck_soft_m.
    const glm::dvec3 deck_dir = glm::normalize(glm::dvec3(1.0, -1.0, 1.0));
    for (double alt : {0.0, w.af.deck_agl_m, w.af.deck_agl_m + w.af.deck_soft_m,
                       w.af.deck_agl_m + w.af.deck_soft_m * 2.0, 5000.0}) {
        check_agreement(w, deck_dir * (w.p.R + alt), count);
    }

    // High vacuum, far above everything.
    check_agreement(w, deck_dir * (w.p.R + 20000.0), count);

    // The antipode of bubble 0's center: opposite side of the planet from
    // every bubble and far from the deck-only near-surface anywhere-breathable
    // read... still on the deck (the deck is global), so assert it separately
    // at an altitude ABOVE the deck to actually probe vacuum-far-from-bubbles.
    const glm::dvec3 antipode = -glm::normalize(w.af.bubbles[0].center_dir);
    check_agreement(w, antipode * (w.p.R + 50.0), count);
    check_agreement(w, antipode * (w.p.R + 5000.0), count);

    INFO("total sample points checked: " << count);
    REQUIRE(count >= 200);
}

TEST_CASE(
    "air_field: disabled AirField reads 1.0 everywhere (matches the "
    "null-env spatial factor)") {
    const World w = make_world();
    render::AirField disabled = w.rf;
    disabled.enabled = false;
    sim::Environment null_env;  // atm == nullptr
    for (double alt : {-50.0, 0.0, 500.0, 8000.0, 30000.0}) {
        const glm::dvec3 dir = glm::normalize(glm::dvec3(0.4, 0.4, 0.8));
        const glm::dvec3 pos = dir * (w.p.R + alt);
        const double sim_val = sim::atm_frac_at(pos, &null_env, w.p);
        REQUIRE(sim_val == 1.0);  // taper is off (atm_taper_sigma = 0)
        REQUIRE(render::air_at(pos, disabled) == 1.0);
    }
}

// The rig-A precedent: a headless SOURCE validator on the GLSL transliteration
// — this CANNOT verify numeric agreement with render::air_at (no GL context),
// only that the expected structural forms are present. State this coverage
// limit honestly: the GLSL side of the H1 fence is checked by EYE (this test
// asserts the exact substrings the implementer wrote) and by the shader
// actually compiling in the smoke run, never by an independent computation.
namespace {

// Quality fix pass item 7: air_optical_depth/air_sky_max_dist/air_eye_pos
// (the march that makes the dome VISIBLE, spec §1.2) had ZERO test coverage
// — this is a CPU twin, built on the ALREADY-pinned render::air_at (the
// numeric cross-check above), not a re-derivation of it. Deliberately
// UNJITTERED (the GLSL side's per-pixel phase jitter, quality fix pass item
// 6, is a display-only dither on TOP of this deterministic integral and
// does not change what the march itself computes at the sample points it
// does use) — stated honestly: this pins the MARCH ALGEBRA, not the GLSL
// text (the source-validator leg below does that, and only structurally).
double cpu_air_sky_max_dist(const glm::dvec3& eye_pos, const glm::dvec3& dir,
                            double planet_r) {
    const double b = glm::dot(eye_pos, dir);
    const double c = glm::dot(eye_pos, eye_pos) - planet_r * planet_r;
    const double disc = b * b - c;
    if (disc < 0.0) return -1.0;  // no hit; caller falls back to march_max_m
    const double t = -b - std::sqrt(disc);
    // Mirrors kAirFieldGLSL's air_sky_max_dist exactly: a NEGATIVE root means
    // the sphere is only crossed BEHIND the ray (e.g. straight-up from
    // altitude — the infinite LINE still intersects the sphere on its far
    // side, but the forward RAY never does) -> also "no hit".
    return (t > 0.0) ? t : -1.0;
}

double cpu_air_optical_depth(const glm::dvec3& eye_pos, const glm::dvec3& dir,
                             double max_dist, int steps,
                             const render::AirField& f, double tau_scale_m) {
    const int n = steps < 2 ? 2 : steps;
    const double ds = max_dist / n;
    double tau = 0.0;
    double f_prev = render::air_at(eye_pos, f);
    for (int i = 1; i <= n; ++i) {
        const glm::dvec3 p = eye_pos + dir * (ds * i);
        const double f_cur = render::air_at(p, f);
        tau += 0.5 * (f_prev + f_cur) * ds;
        f_prev = f_cur;
    }
    return tau / tau_scale_m;
}

}  // namespace

TEST_CASE(
    "air_field: cpu_air_optical_depth is monotonically "
    "non-decreasing in march distance inside a constant-air core") {
    const World w = make_world();
    // Deep inside bubble 0's core (well inside the edge radius, so air_at is
    // ~1.0 identically along the whole tested path — a true nonnegative
    // integrand, so the trapezoid integral of distance is monotonic by
    // construction, not by luck of the field's shape).
    const glm::dvec3 center = glm::normalize(w.af.bubbles[0].center_dir);
    glm::dvec3 tangent(0.0, 0.0, 1.0);
    tangent -= center * glm::dot(tangent, center);
    tangent = glm::normalize(tangent);
    const glm::dvec3 eye = center * w.p.R;
    double prev_tau = -1.0;
    for (double dist : {200.0, 800.0, 1500.0, 2500.0, 3500.0}) {
        const double tau =
            cpu_air_optical_depth(eye, tangent, dist, 24, w.rf, 5000.0);
        INFO("dist = " << dist << " tau = " << tau);
        REQUIRE(tau >= prev_tau);
        prev_tau = tau;
    }
    // And it should be close to the exact analytic value (constant field 1.0
    // over the whole path => tau == dist / tau_scale_m).
    const double tau_full =
        cpu_air_optical_depth(eye, tangent, 3500.0, 24, w.rf, 5000.0);
    REQUIRE(tau_full == Catch::Approx(3500.0 / 5000.0).epsilon(1e-9));
}

// ---------------------------------------------------------------------------
// S-marchbound (Chad's fly, 2026-08-09 — the world-anchored boundary banding).
// air_march_range brackets the segment of a ray that can contain air so the
// march's fixed step budget is spent INSIDE the air instead of across a blind
// 55 km. The whole mechanism rests on one property: the bracket must never
// CLIP real air, or the dome gets visibly cut. That is what the first leg
// pins, densely and by brute force, rather than by re-deriving the bound.

TEST_CASE(
    "air_march_range never clips air: every point with air_at > 0 along a "
    "ray lies inside the returned bracket") {
    const World w = make_world();
    const double max_dist = 55000.0;
    // Eyes spanning the interesting regimes: inside each dome, just outside a
    // soft edge, high above, and out in the vacuum gap.
    const std::vector<glm::dvec3> eyes = {
        glm::normalize(glm::dvec3(0.3, 0.9, -0.2)) * (w.p.R + 200.0),
        glm::normalize(glm::dvec3(0.3, 0.9, -0.2)) * (w.p.R + 3800.0),
        glm::normalize(glm::dvec3(-0.7, -0.5, 0.4)) * (w.p.R + 1500.0),
        glm::normalize(glm::dvec3(1.0, 0.1, 0.1)) * (w.p.R + 9000.0),
        glm::normalize(glm::dvec3(0.1, 0.2, 1.0)) * (w.p.R + 25000.0),
    };
    int rays_with_air = 0;
    for (const glm::dvec3& eye : eyes) {
        // A deterministic spread of directions (no RNG): a coarse lat/lon fan
        // in an arbitrary, non-axis-aligned basis so nothing lines up with a
        // world axis or with a bubble centre.
        const glm::dvec3 f = glm::normalize(eye);
        glm::dvec3 e1 = glm::cross(f, glm::normalize(glm::dvec3(0.2, -0.9, 0.35)));
        e1 = glm::normalize(e1);
        const glm::dvec3 e2 = glm::cross(f, e1);
        for (int ia = 0; ia < 12; ++ia) {
            for (int ib = 0; ib < 12; ++ib) {
                const double th = kPi * (ia + 0.5) / 12.0;   // from up
                const double ph = 2.0 * kPi * (ib + 0.5) / 12.0;
                const glm::dvec3 dir = glm::normalize(
                    f * std::cos(th) +
                    (e1 * std::cos(ph) + e2 * std::sin(ph)) * std::sin(th));
                const glm::dvec2 range =
                    render::air_march_range(eye, dir, max_dist, w.rf);
                // Brute-force scan at a stride far finer than any soft width.
                const int kSamples = 1100;
                for (int s = 0; s <= kSamples; ++s) {
                    const double t = max_dist * s / kSamples;
                    const double air = render::air_at(eye + dir * t, w.rf);
                    if (air <= 0.0) continue;
                    ++rays_with_air;
                    INFO("t = " << t << " air = " << air << " range = ["
                                << range.x << ", " << range.y << "]");
                    REQUIRE(range.y > range.x);   // must not report empty
                    REQUIRE(t >= range.x - 1e-6);
                    REQUIRE(t <= range.y + 1e-6);
                }
            }
        }
    }
    // Guard against the fixture making this vacuous (the fixture-no-op class):
    // if no sampled point had air, every REQUIRE above was skipped.
    REQUIRE(rays_with_air > 1000);
}

TEST_CASE(
    "air_march_range returns an EMPTY bracket for a ray that meets no air, "
    "and brackets the whole ray when the field is disabled") {
    const World w = make_world();
    // A ray in the vacuum gap, aimed away from both domes and away from the
    // planet: it can never enter air.
    const glm::dvec3 mid = glm::normalize(
        glm::normalize(glm::dvec3(0.3, 0.9, -0.2)) +
        glm::normalize(glm::dvec3(-0.7, -0.5, 0.4)));
    const glm::dvec3 eye = mid * (w.p.R + 6000.0);
    const glm::dvec3 out = glm::normalize(mid);  // straight up, away from all
    const glm::dvec2 empty =
        render::air_march_range(eye, out, 55000.0, w.rf);
    REQUIRE(empty.y <= empty.x);
    // Cross-check the premise: there really is no air along that ray (so this
    // is an honest emptiness, not a bracket bug that happens to agree).
    for (int s = 0; s <= 500; ++s) {
        const double t = 55000.0 * s / 500;
        REQUIRE(render::air_at(eye + out * t, w.rf) == 0.0);
    }
    // A DISABLED field is full air everywhere (air_at == 1.0), so the bracket
    // must span the whole ray — never a phantom vacuum.
    render::AirField off = w.rf;
    off.enabled = false;
    const glm::dvec2 all = render::air_march_range(eye, out, 55000.0, off);
    REQUIRE(all.x == 0.0);
    REQUIRE(all.y == 55000.0);
}

TEST_CASE(
    "air_march_range tightens the march stride enough to resolve the soft "
    "edge (the defect Chad flew)") {
    const World w = make_world();
    // Inside dome 0 at a typical fighting altitude, looking up: the old march
    // spent all 20 steps across march_max_m (55 km => ds = 2750 m) while the
    // edge_soft it has to resolve is 1200 m. The bracket must bring the stride
    // BELOW the soft width, which is the whole point of the mechanism.
    const glm::dvec3 up = glm::normalize(glm::dvec3(0.3, 0.9, -0.2));
    const glm::dvec3 eye = up * (w.p.R + 200.0);
    const glm::dvec2 range = render::air_march_range(eye, up, 55000.0, w.rf);
    REQUIRE(range.y > range.x);
    const double ds_bounded = (range.y - range.x) / 20.0;
    const double ds_blind = 55000.0 / 20.0;
    INFO("bounded ds = " << ds_bounded << " blind ds = " << ds_blind);
    REQUIRE(ds_bounded < w.rf.bubbles[0].edge_soft_m);
    REQUIRE(ds_bounded < 0.25 * ds_blind);
}

TEST_CASE(
    "air_field: cpu_air_optical_depth is exactly zero in the "
    "vacuum gap") {
    const World w = make_world();
    const glm::dvec3 mid =
        glm::normalize(glm::normalize(w.af.bubbles[0].center_dir) +
                       glm::normalize(w.af.bubbles[1].center_dir));
    glm::dvec3 tangent(1.0, 0.0, 0.0);
    tangent -= mid * glm::dot(tangent, mid);
    tangent = glm::normalize(tangent);
    const glm::dvec3 eye = mid * (w.p.R + 3000.0);  // well above the deck too
    const double tau =
        cpu_air_optical_depth(eye, tangent, 6000.0, 16, w.rf, 5000.0);
    REQUIRE(tau == 0.0);
}

TEST_CASE(
    "air_field: cpu_air_optical_depth saturates (airAmt -> 1) on a "
    "long march through a dome core") {
    const World w = make_world();
    // Bubble 0's ground_radius_m is 6000 (make_world) -- stay well inside
    // that so the march never exits into vacuum (a march that runs PAST the
    // edge is a different, correctly-lower-tau scenario, not a saturation
    // test); 5000 m at tau_scale_m=800 gives an exact-field tau of 6.25,
    // airAmt ~0.998.
    const glm::dvec3 center = glm::normalize(w.af.bubbles[0].center_dir);
    glm::dvec3 tangent(0.0, 0.0, 1.0);
    tangent -= center * glm::dot(tangent, center);
    tangent = glm::normalize(tangent);
    const glm::dvec3 eye = center * w.p.R;
    const double tau =
        cpu_air_optical_depth(eye, tangent, 5000.0, 32, w.rf, 800.0);
    const double air_amt = 1.0 - std::exp(-tau);
    REQUIRE(air_amt > 0.99);
}

TEST_CASE(
    "air_field: cpu_air_sky_max_dist clamps to the planet-hit "
    "distance on a straight-down ray, and reports no-hit above the "
    "horizon") {
    const World w = make_world();
    const glm::dvec3 up = glm::normalize(glm::dvec3(0.2, 0.9, 0.1));
    const double alt = 2000.0;
    const glm::dvec3 eye = up * (w.p.R + alt);
    // Straight down: t == alt exactly (b = -(R+alt), disc = R^2, t =
    // (R+alt)-R = alt) — an exact closed form, not an eyeballed bound.
    const double t_down = cpu_air_sky_max_dist(eye, -up, w.p.R);
    REQUIRE(t_down == Catch::Approx(alt).epsilon(1e-9));
    // Straight up (away from the planet): never hits => negative discriminant
    // signalled by the -1.0 sentinel (the caller falls back to march_max_m).
    const double t_up = cpu_air_sky_max_dist(eye, up, w.p.R);
    REQUIRE(t_up == -1.0);
}

// ===========================================================================
// S-domeround (docs/airdome_round_spec.md §5) — the six mandated legs, all
// against the LIVE sim::atm_frac_at (the H1 site; render::air_at is already
// pinned bit-for-bit against it above, so a sim-side proof covers both).
// ===========================================================================

namespace {

// A single, arbitrary-direction (not axis-aligned) circular test bubble at
// the spec's own worked example (n=3, ceiling_m=4000 => H ~= 4961.96, the
// exact std::lgamma-derived value, not the spec's rounded ~4959.7 prose
// approximation).
sim::AtmosphereField one_dome_bubble(double n) {
    sim::AtmosphereField af;
    af.dome_exponent = n;
    // The DECK is a SEPARATE union term (sim/aero.h's "full air below
    // deck_agl_m EVERYWHERE" floor) that ALSO saturates at exactly alt=0
    // (atm_falloff's d<=0 plateau fires regardless of deck_agl_m/soft) --
    // disabled here so alt_p=0 probes isolate the BUBBLE term only, per the
    // "REQUIRE the baseline term > eps"/anti-no-op discipline (a probe that
    // silently rides the deck's own floor proves nothing about the dome).
    af.deck_agl_m = -1.0e9;
    af.deck_soft_m = 0.0;
    sim::AtmosphereField::Bubble b;
    b.center_dir = glm::normalize(glm::dvec3(0.3, 1.0, 0.2));
    b.ground_radius_m = 6000.0;
    b.ceiling_m = 4000.0;
    b.edge_soft_m = 1200.0;
    b.ceil_soft_m = 600.0;
    const double I_n =
        std::exp(std::lgamma(1.0 + 1.0 / n) + std::lgamma(1.0 + 2.0 / n) -
                 std::lgamma(1.0 + 3.0 / n));
    b.dome_h_m = b.ceiling_m / I_n;
    af.bubbles.push_back(b);
    return af;
}

sim::AircraftParams dome_test_params() {
    sim::AircraftParams p;
    p.R = 15000.0;
    p.atm_taper_sigma = 0.0;  // structurally off -- isolate the spatial term
    return p;
}

glm::dvec3 dome_dir_at_arc(const glm::dvec3& center, double arc_m, double R) {
    const double theta = arc_m / R;
    glm::dvec3 arbitrary(0.0, 0.0, 1.0);
    glm::dvec3 t = arbitrary - center * glm::dot(arbitrary, center);
    if (glm::length(t) < 1e-6) t = glm::dvec3(1.0, 0.0, 0.0);
    t = glm::normalize(t);
    return glm::normalize(std::cos(theta) * center + std::sin(theta) * t);
}

// The OLD (pre-domeround) cylinder expression, duplicated here deliberately
// (the fork-detector discipline, e.g. AT-12/MB-rud's oracle arms) -- NOT
// calling sim::atm_frac_at's own code, so a regression in the new law that
// happens to still equal itself can't hide from this leg.
double old_cylinder_u(double arc, double alt, double r, double ceiling,
                      double edge_soft, double ceil_soft) {
    return sim::atm_falloff(arc - r, edge_soft) *
           sim::atm_falloff(alt - ceiling, ceil_soft);
}

}  // namespace

TEST_CASE("air_field domeround leg 1: on-axis exactness (alt=0 and arc=0)") {
    const sim::AircraftParams p = dome_test_params();
    const sim::AtmosphereField af = one_dome_bubble(3.0);
    const sim::AtmosphereField::Bubble& b = af.bubbles[0];
    sim::Environment env;
    env.atm = &af;
    const glm::dvec3 center = glm::normalize(b.center_dir);

    // alt_p = 0 across many bearings: the new law must equal the OLD
    // horizontal expression bit-for-bit (spec §1.2's on-axis proof).
    for (double bearing_deg : {0.0, 40.0, 90.0, 137.0, 200.0, 271.0, 333.0}) {
        for (double arc :
             {0.0, 2000.0, 5000.0, 6000.0, 6600.0, 7200.0, 9000.0, 20000.0}) {
            const double theta = arc / p.R;
            glm::dvec3 t0(0.0, 0.0, 1.0);
            t0 -= center * glm::dot(t0, center);
            t0 = glm::normalize(t0);
            const glm::dvec3 t1 = glm::cross(center, t0);
            const double rad = bearing_deg * kPi / 180.0;
            const glm::dvec3 tangent = std::cos(rad) * t0 + std::sin(rad) * t1;
            const glm::dvec3 dir = glm::normalize(std::cos(theta) * center +
                                                  std::sin(theta) * tangent);
            const glm::dvec3 pos = dir * p.R;  // alt == 0 exactly
            const double u_new = sim::atm_frac_at(pos, &env, p);  // taper off
            const double u_old =
                sim::atm_falloff(arc - b.ground_radius_m, b.edge_soft_m);
            INFO("bearing=" << bearing_deg << " arc=" << arc);
            // Approx, not literal ==: geometric reconstruction of `arc` from
            // `dir` (via cos/sin then acos) and the pow/pow(1/n) round-trip
            // at w=0 both carry a few ULP of floating noise even though the
            // MATH is an exact identity (spec §1.2) -- 1e-9 is far tighter
            // than any real algorithmic mutant (perturbing d or the soft
            // blend) while tolerating that noise floor.
            REQUIRE(u_new == Catch::Approx(u_old).margin(1e-9));
        }
    }

    // arc = 0 across many altitudes: the new law must equal the OLD vertical
    // expression WITH H substituted for the literal ceiling_m (spec's own
    // "modulo H" caveat).
    for (double alt :
         {-50.0, 0.0, 500.0, 2000.0, 4000.0, 4961.96, 5200.0, 6000.0, 9000.0}) {
        const glm::dvec3 pos = center * (p.R + alt);
        const double u_new = sim::atm_frac_at(pos, &env, p);
        const double alt_p = std::max(alt, 0.0);
        const double u_old =
            sim::atm_falloff(alt_p - b.dome_h_m, b.ceil_soft_m);
        INFO("alt=" << alt);
        REQUIRE(u_new == Catch::Approx(u_old).margin(1e-9));
    }
}

TEST_CASE(
    "air_field domeround leg 2: n=32 matches the old cylinder within a "
    "tight bound near each axis") {
    // H = ceiling_m LITERALLY here (no volume-preserve confound -- this leg
    // isolates the pure SHAPE/exponent convergence, not the H-raise), i.e.
    // bubble_ceiling_volume_preserve=false semantics: dome_h_m == ceiling_m.
    const sim::AircraftParams p = dome_test_params();
    sim::AtmosphereField af;
    af.dome_exponent = 32.0;
    af.deck_agl_m = -1.0e9;
    af.deck_soft_m = 0.0;
    sim::AtmosphereField::Bubble b;
    b.center_dir = glm::normalize(glm::dvec3(0.3, 1.0, 0.2));
    b.ground_radius_m = 6000.0;
    b.ceiling_m = 4000.0;
    b.edge_soft_m = 1200.0;
    b.ceil_soft_m = 600.0;
    b.dome_h_m = b.ceiling_m;  // literal H
    af.bubbles.push_back(b);
    sim::Environment env;
    env.atm = &af;
    const glm::dvec3 center = glm::normalize(b.center_dir);

    // The meridional distance rho = sqrt(arc^2 + alt^2) mixes BOTH axes'
    // ABSOLUTE distances (not just their ratios to r/H), so the n->inf
    // convergence to the old per-axis-separable product only holds where
    // the OFF-axis coordinate is small in ABSOLUTE metres (a genuinely
    // near-axis probe), not merely a small FRACTION of its own radius/
    // ceiling -- a point at, say, arc=0.5r/alt=1.1*ceiling has arc=3000m,
    // large enough in absolute terms to visibly perturb rho even though
    // 0.5^32 is astronomically negligible inside the norm `s` itself. This
    // is a genuine, expected structural difference between the dome's
    // Euclidean corner-rounding and the old code's separable axis product
    // -- NOT a cylinder-limit approximation error -- and is exactly what
    // leg 3 exists to demonstrate at n=3; this leg instead verifies the
    // TRUE near-axis limit converges tightly.
    struct Case {
        double arc, alt, bound;
    };
    const Case cases[] = {
        {1.1 * b.ground_radius_m, 10.0, 1e-3},  // near-pure-horizontal
        {1.1 * b.ground_radius_m, 50.0, 1e-3},
        {10.0, 1.1 * b.ceiling_m, 1e-3},  // near-pure-vertical
        {50.0, 1.1 * b.ceiling_m, 1e-3},
    };
    for (const Case& c : cases) {
        const glm::dvec3 dir = dome_dir_at_arc(center, c.arc, p.R);
        const glm::dvec3 pos = dir * (p.R + c.alt);
        const double u_new = sim::atm_frac_at(pos, &env, p);
        const double u_old =
            old_cylinder_u(c.arc, c.alt, b.ground_radius_m, b.ceiling_m,
                           b.edge_soft_m, b.ceil_soft_m);
        INFO("arc=" << c.arc << " alt=" << c.alt << " new=" << u_new
                    << " old=" << u_old);
        REQUIRE(std::abs(u_new - u_old) < c.bound);
    }
}

TEST_CASE(
    "air_field domeround leg 3: the corner actually rounds (n=3 gives "
    "strictly less air than the old cylinder)") {
    // The point proving the feature is not a no-op: 90% of the horizontal
    // radius AND 90% of the (config) ceiling -- deep enough inside EACH
    // individual axis's OLD hard core that a naive per-axis reading calls it
    // a full-density no-op (old_u == 1.0 exactly, both factors saturated),
    // yet the new law is a REAL 2D distance from the rounded surface and
    // reads meaningfully thinner. (Literal "70%/70%" sits fully inside the
    // dome's own hard core too at n=3 -- both terms still solve s<1 there,
    // so old==new==1.0 identically; 90%/90% is the smallest round-number
    // fraction that actually separates the two laws. Reported as a spec
    // deviation in docs/airdome_report.md.)
    const sim::AircraftParams p = dome_test_params();
    const sim::AtmosphereField af = one_dome_bubble(3.0);
    const sim::AtmosphereField::Bubble& b = af.bubbles[0];
    sim::Environment env;
    env.atm = &af;
    const glm::dvec3 center = glm::normalize(b.center_dir);

    const double arc = 0.9 * b.ground_radius_m;
    const double alt = 0.9 * b.ceiling_m;
    const glm::dvec3 dir = dome_dir_at_arc(center, arc, p.R);
    const glm::dvec3 pos = dir * (p.R + alt);

    const double u_old = old_cylinder_u(
        arc, alt, b.ground_radius_m, b.ceiling_m, b.edge_soft_m, b.ceil_soft_m);
    REQUIRE(u_old == 1.0);  // the fixture-no-op guard: the OLD law is
                            // genuinely saturated here, not just close
    const double u_new = sim::atm_frac_at(pos, &env, p);
    REQUIRE(u_new < u_old);  // STRICTLY less air -- the corner rounds
    REQUIRE(u_new < 0.95);   // a real, non-trivial gap (not fp noise)
}

TEST_CASE(
    "air_field domeround leg 4: volume preservation (H = ceiling_m / "
    "I(n) holds the SAME air as a cylinder of height ceiling_m)") {
    // I(2) == 2/3 EXACTLY -- the free analytic self-check (true half-
    // ellipsoid volume; spec §1.3).
    auto I = [](double n) {
        return std::exp(std::lgamma(1.0 + 1.0 / n) +
                        std::lgamma(1.0 + 2.0 / n) -
                        std::lgamma(1.0 + 3.0 / n));
    };
    REQUIRE(I(2.0) == Catch::Approx(2.0 / 3.0).epsilon(1e-12));

    // Numerically integrate the dome's cross-sectional area over altitude
    // (a simple many-step quadrature of the HARD superellipse surface --
    // a(t) = a*(1-(t/H)^n)^(1/n), area(t) = pi*a(t)*b(t) -- i.e. the pure
    // geometric shape, independent of atm_falloff's soft skirt) and compare
    // against pi*a*b*ceiling_m -- the volume of the CYLINDER this dome is
    // meant to replace air-for-air.
    const double a = 8000.0, b_axis = 5000.0, ceiling_m = 4000.0;
    for (double n : {2.0, 3.0, 8.0}) {
        const double H = ceiling_m / I(n);
        const int kSteps = 20000;
        double vol = 0.0;
        double prev_area = a * b_axis * kPi;  // t=0: full footprint
        for (int i = 1; i <= kSteps; ++i) {
            const double t = H * static_cast<double>(i) / kSteps;
            const double shrink =
                std::pow(std::max(0.0, 1.0 - std::pow(t / H, n)), 1.0 / n);
            const double area = kPi * a * shrink * b_axis * shrink;
            vol += 0.5 * (prev_area + area) * (H / kSteps);
            prev_area = area;
        }
        const double cylinder_vol = kPi * a * b_axis * ceiling_m;
        INFO("n=" << n << " dome_vol=" << vol
                  << " cylinder_vol=" << cylinder_vol);
        REQUIRE(std::abs(vol - cylinder_vol) / cylinder_vol < 0.01);  // ~1%
    }
}

TEST_CASE(
    "air_field domeround leg 5: an extinct faction (radius<=0) reads "
    "exactly zero air everywhere, dome or not") {
    const sim::AircraftParams p = dome_test_params();
    sim::AtmosphereField af;
    af.dome_exponent = 3.0;
    // The DECK is a SEPARATE union term (the go-anywhere floor, sim/aero.h)
    // that provides its own near-ground air INDEPENDENT of bubble state --
    // disabled here so this leg isolates the extinct BUBBLE term (an
    // extinct faction's dome reads zero; the planet's deck floor is a
    // different, correct mechanism entirely and is not what this leg is
    // about).
    af.deck_agl_m = -1.0e9;
    af.deck_soft_m = 0.0;
    sim::AtmosphereField::Bubble b;
    b.center_dir = glm::normalize(glm::dvec3(0.3, 1.0, 0.2));
    b.ground_radius_m = 0.0;  // extinct: combat::kFactionScaleFloor reaches 0
    b.ceiling_m = 0.0;
    b.edge_soft_m = 0.0;
    b.ceil_soft_m = 0.0;
    b.dome_h_m = 0.0;
    af.bubbles.push_back(b);
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 center = glm::normalize(b.center_dir);
    // Dead center at ground (the s<=0/rho==0 guard would read u==1 for a
    // LIVE bubble -- an extinct one must NOT).
    CHECK(sim::atm_frac_at(center * p.R, &env, p) == 0.0);
    CHECK(sim::atm_frac_at(center * (p.R + 500.0), &env, p) == 0.0);
    CHECK(sim::atm_frac_at(dome_dir_at_arc(center, 3000.0, p.R) * (p.R + 200.0),
                           &env, p) == 0.0);
}

TEST_CASE(
    "air_field domeround leg 6: the weather scalar is gated by air -- "
    "zero in the vacuum gap, unchanged at a dome centre") {
    render::AirField f;
    f.enabled = true;
    f.planet_R = 15000.0;
    f.deck_agl_m = 120.0;
    f.deck_soft_m = 200.0;
    f.dome_exponent = 3.0;
    f.bubble_count = 1;
    render::AirField::Bubble& rb = f.bubbles[0];
    rb.center_dir = glm::normalize(glm::dvec3(0.3, 1.0, 0.2));
    rb.a = 6000.0;
    rb.ceiling_m =
        4961.96;  // H (folded into this slot -- see the struct comment)
    rb.edge_soft_m = 1200.0;
    rb.ceil_soft_m = 600.0;

    const glm::dvec3 center = glm::normalize(rb.center_dir);
    const glm::dvec3 dome_pos = center * (f.planet_R + 500.0);
    const double gated_dome = render::gate_weather_by_air(0.8, dome_pos, f);
    CHECK(gated_dome == Catch::Approx(0.8).epsilon(1e-9));  // unchanged in-core

    // Far from the bubble, above the deck: pure vacuum -- the weather
    // scalar must read exactly zero regardless of the raw weather value.
    const glm::dvec3 vac_dir = dome_dir_at_arc(center, 60000.0, f.planet_R);
    const glm::dvec3 vac_pos = vac_dir * (f.planet_R + 3000.0);
    const double gated_vac = render::gate_weather_by_air(1.0, vac_pos, f);
    CHECK(gated_vac == 0.0);
}

TEST_CASE(
    "air_field: kAirFieldGLSL contains the ellipse r_eff denominator "
    "and the complement-product union") {
    const std::string src = render::kAirFieldGLSL;
    // The ellipse polar-form radius a*b/sqrt((b*cos)^2+(a*sin)^2).
    REQUIRE(src.find("a * b / denom") != std::string::npos);
    REQUIRE(src.find("(b * cosTh) * (b * cosTh)") != std::string::npos);
    REQUIRE(src.find("(a * sinTh) * (a * sinTh)") != std::string::npos);
    // The complement-product union: oneMinus *= (1.0 - u), returned as
    // 1.0 - oneMinus.
    REQUIRE(src.find("oneMinus *= (1.0 - u)") != std::string::npos);
    REQUIRE(src.find("return 1.0 - oneMinus;") != std::string::npos);
    // S-domeround (docs/airdome_round_spec.md §1/§4): the superellipse-of-
    // revolution law -- the meridional norm `s` mixing arc/radiusH and
    // altP/H (uAirDomeExp itself is declared in kAirFieldUniformsGLSL, the
    // separate uniform-block string, checked below).
    REQUIRE(src.find("pow(arc / radiusH, n)") != std::string::npos);
    REQUIRE(src.find("pow(altP / H, n)") != std::string::npos);
    REQUIRE(src.find("uAirDomeExp") != std::string::npos);
    const std::string uniforms_src = render::kAirFieldUniformsGLSL;
    REQUIRE(uniforms_src.find("uniform float uAirDomeExp;") !=
            std::string::npos);
    // No stray preprocessor directive, no smuggled uniform declared in the
    // shared function body's own text beyond the dedicated uniform block
    // (this file references, it does not declare, uUp/uEyeAlt — the H1
    // convention documented in air_field_glsl.h).
    REQUIRE(src.find("#define") == std::string::npos);
    REQUIRE(src.find("#include") == std::string::npos);
    // S-marchbound: the GLSL must carry the SAME bracket the C++ twin above is
    // numerically pinned against — the bounding radius form, the empty-range
    // short circuit, and a march that starts at range.x rather than at the eye.
    // (Structural only: no GL context here, same honest limit as the rest of
    // this leg.)
    REQUIRE(src.find("vec2 air_march_range(") != std::string::npos);
    REQUIRE(src.find("aMax * aMax * (1.0 + hMax / uAirPlanetR)") !=
            std::string::npos);
    REQUIRE(src.find("if (range.y <= range.x) return 0.0;") !=
            std::string::npos);
    REQUIRE(src.find("(range.y - range.x) / float(n)") != std::string::npos);
    // And that the degenerate large-argument sin hash is GONE (it degenerated
    // into structured streaks at 1080p fragcoords — see air_ign's comment).
    REQUIRE(src.find("43758.5453") == std::string::npos);
    REQUIRE(src.find("float air_ign(vec2 p)") != std::string::npos);
}
