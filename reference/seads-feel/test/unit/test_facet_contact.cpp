// terrain-clip T2 — THE FIX, graded. [ground] facet_contact: the aircraft's
// crash surface is blended onto the surface the EYE IS SHOWN (the render mesh
// facet), which is the law the sled already lives under ([snowpack]
// hf_faceted_ground). T1 (docs/terrain_clip/CLIP_INSTRUMENT.md) measured the
// defect; this file grades the dial that closes it.
//
//   L1  THE DIAL — shipped value, loader bounds, and IDENTITY BY BRANCH: at
//       facet_contact 0 the injected facet function is NOT CALLED (counted) and
//       contact_radius returns bit-for-bit `hf.radius_at + contact_height_m`.
//       The mix is linear in between and lands exactly on the facet at 1.
//   L2  THE BEHAVIOUR — a stub facet 10 m above the field: an airframe at
//       field + 5 m is INSIDE the drawn ground. At dial 1.0 ground_contact
//       grades it (grounded or crashed); at 0.0 it is untouched airborne.
//       This is Chad's report, reduced to four lines.
//   L3  THE CENSUS — the T1 clip fraction re-run at dial 0 vs dial 1 over the
//       real DEM at Onaping / Valley / Sudbury, RAW and BLURRED (T1 caveat 1).
//   L4  THE LANDING GAP — |facet - field| where he actually touches down: the
//       Sudbury spawn (Murray mouth) and the Valley pump apron. p50/p90/max.
//       This is the feel exposure of arming the dial, in metres.
//   L5  THE COST — render::facet_radius_at per tick at 120 Hz.
//
// Real DEM decoded headlessly with stb_image; STB_IMAGE_IMPLEMENTATION lives in
// test_tunnel_mesh.cpp (the one owner) — declarations only here.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "config/load_world.h"
#include "render/sphere_param.h"
#include "sim/environment.h"
#include "sim/ground.h"
#include "sim/state.h"
#include "sim/step.h"
#include "world/faction_bubbles.h"
#include "world/buildings.h"
#include "world/heightfield.h"
#include "world/snowpack.h"
#include "world/tunnel_geo.h"

#define STBI_ONLY_PNG
#include "stb_image.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kAp =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

// The SHIPPED numbers (config/world.toml [planet], config/game.toml [ground],
// config/world.toml [snowpack]) — quoted so this grades the world Chad flies.
constexpr int kSubdiv = 200;
constexpr int kTiles = 2;
constexpr double kReliefScale = 350.0;
constexpr double kUOffset = 0.806;
constexpr double kContactHeightM = 2.45;
constexpr double kAmbientSnowM = 0.77;
constexpr int kDemBlurRadius = 3;  // config/world.toml [planet]

world::HeightField real_field() {
    world::HeightField hf;
    const std::string path = std::string(SEADS_ASSET_DIR) + "/sudbury_dem.png";
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(path.c_str(), &w, &h, &comp, 3);
    if (px == nullptr) return hf;  // w stays 0 — caller skips
    hf.w = w;
    hf.h = h;
    hf.R = kAp.R;
    hf.relief_scale = kReliefScale;
    hf.u_offset = kUOffset;
    hf.px.resize(static_cast<std::size_t>(w) * h);
    for (std::size_t i = 0; i < hf.px.size(); ++i)
        hf.px[i] = world::dem16_unpack(px[i * 3 + 0], px[i * 3 + 1]);
    stbi_image_free(px);
    return hf;
}

world::HeightField uniform_field(double elev_m, double relief = 350.0) {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kAp.R;
    hf.relief_scale = relief;
    hf.u_offset = 0.0;
    const double f = std::min(std::max(elev_m / relief, 0.0), 1.0);
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h,
                 static_cast<std::uint16_t>(f * 65535.0 + 0.5));
    return hf;
}

// raylib's ImageBlurGaussian(radius) is THREE successive BOX blurs of width
// 2*radius+1 (rlBlurBox x3) — not a true Gaussian. Reproduced here in uint16
// over a PADDED WINDOW about one site (the full 8192x4096 field would be six
// passes over 33 M texels for numbers that only ever get read inside a 6 km
// box; the padding is 3*radius wider than the box so every interior texel sees
// the same neighbourhood it would in a whole-image blur).
//
// ⚠ AND IT IS A SENSITIVITY ARM, NOT THE RUNTIME — see the L3 leg's finding.
void blur_window(world::HeightField& hf, glm::dvec3 center, double half_m,
                 int radius) {
    if (radius <= 0 || hf.w == 0) return;
    const glm::dvec2 uv = world::equirect_uv(center, hf.u_offset);
    const int ci = static_cast<int>(uv.x * hf.w), cj = static_cast<int>(uv.y * hf.h);
    // Metres per texel: the v axis is pi*R over h; use it for both (the u axis
    // is never coarser away from the poles), then pad generously.
    const double m_per_texel = 3.14159265358979323846 * hf.R / hf.h;
    const int span = static_cast<int>(half_m / m_per_texel) + 4 * radius + 8;
    const int i0 = std::max(0, ci - span), i1 = std::min(hf.w - 1, ci + span);
    const int j0 = std::max(0, cj - span), j1 = std::min(hf.h - 1, cj + span);
    const int bw = i1 - i0 + 1, bh = j1 - j0 + 1;
    std::vector<float> a(static_cast<std::size_t>(bw) * bh), b(a.size());
    for (int j = 0; j < bh; ++j)
        for (int i = 0; i < bw; ++i)
            a[static_cast<std::size_t>(j) * bw + i] = static_cast<float>(
                hf.px[static_cast<std::size_t>(j0 + j) * hf.w + (i0 + i)]);
    const int k = 2 * radius + 1;
    for (int pass = 0; pass < 3; ++pass) {
        // horizontal
        for (int j = 0; j < bh; ++j)
            for (int i = 0; i < bw; ++i) {
                double s = 0.0;
                for (int t = -radius; t <= radius; ++t) {
                    const int x = std::min(bw - 1, std::max(0, i + t));
                    s += a[static_cast<std::size_t>(j) * bw + x];
                }
                b[static_cast<std::size_t>(j) * bw + i] =
                    static_cast<float>(s / k);
            }
        // vertical
        for (int j = 0; j < bh; ++j)
            for (int i = 0; i < bw; ++i) {
                double s = 0.0;
                for (int t = -radius; t <= radius; ++t) {
                    const int y = std::min(bh - 1, std::max(0, j + t));
                    s += b[static_cast<std::size_t>(y) * bw + i];
                }
                a[static_cast<std::size_t>(j) * bw + i] =
                    static_cast<float>(s / k);
            }
    }
    for (int j = 0; j < bh; ++j)
        for (int i = 0; i < bw; ++i)
            hf.px[static_cast<std::size_t>(j0 + j) * hf.w + (i0 + i)] =
                static_cast<std::uint16_t>(std::min(
                    65535.0,
                    std::max(0.0, static_cast<double>(
                                      a[static_cast<std::size_t>(j) * bw + i]) +
                                      0.5)));
}

struct Site {
    const char* name;
    glm::dvec3 dir;
};

void tangents(glm::dvec3 d, glm::dvec3& e1, glm::dvec3& e2) {
    e1 = glm::normalize(glm::cross(d, glm::dvec3{0.31, 0.77, 0.56}));
    e2 = glm::normalize(glm::cross(d, e1));
}

glm::dvec3 offset_dir(glm::dvec3 d, glm::dvec3 e1, glm::dvec3 e2, double x,
                      double y) {
    return glm::normalize(d + (e1 * x + e2 * y) / kAp.R);
}

double pct(std::vector<double>& v, double q) {
    if (v.empty()) return 0.0;
    const std::size_t i =
        static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(i),
                     v.end());
    return v[i];
}

// The shipped [ground] dials, so every leg grades the config Chad flies.
sim::GroundParams shipped_gp() {
    sim::GroundParams gp;
    gp.contact_height_m = kContactHeightM;
    gp.slope_limit_cos = std::cos(20.0 * 3.14159265358979323846 / 180.0);
    gp.normal_probe_m = 60.0;
    gp.max_sink_ms = 6.0;
    gp.deep_penetration_m = 50.0;
    gp.friction = 0.08;
    gp.brake_friction = 0.60;
    gp.ground_loop_lat_g = 0.0;  // off: this file grades the SURFACE, not the loop
    return gp;
}

}  // namespace

// ---------------------------------------------------------------------------
// L1 — the dial: shipped value, loader bounds, identity BY BRANCH, the mix.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L1 dial: shipped 1.0, bounded [0,1], read by the app") {
    const cfg::GameParams g =
        cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kAp);
    REQUIRE(g.ground.facet_contact >= 0.0);
    REQUIRE(g.ground.facet_contact <= 1.0);
    // THIS LANE SHIPS IT ARMED — Chad flies it. If this ever reads 0 the fix is
    // dormant and the doc's fly checklist is a lie.
    REQUIRE(g.ground.facet_contact == 1.0);
    std::printf("\nL1 [ground] facet_contact = %.3f (shipped)\n",
                g.ground.facet_contact);

    // THE BOUNDS, exercised through the real loader on a real file: the dial is
    // a FRACTION. Outside [0, 1] it would subtract the drawn surface from the
    // field or extrapolate past the mesh chord — neither is a surface anything
    // is drawn on, so the loader must refuse rather than hand the kernel one.
    const std::string src = SEADS_CONFIG_DIR "/game.toml";
    std::string text;
    {
        std::FILE* in = std::fopen(src.c_str(), "rb");
        REQUIRE(in != nullptr);
        char buf[4096];
        std::size_t got = 0;
        while ((got = std::fread(buf, 1, sizeof buf, in)) > 0)
            text.append(buf, got);
        std::fclose(in);
    }
    const std::string key = "facet_contact = 1.0";
    const std::size_t at = text.find(key);
    REQUIRE(at != std::string::npos);
    auto write_with = [&](const std::string& value, const char* fname) {
        std::string t = text;
        t.replace(at, key.size(), "facet_contact = " + value);
        const std::string path = std::string(SEADS_TEST_TMP_DIR) + "/" + fname;
        std::FILE* out = std::fopen(path.c_str(), "wb");
        REQUIRE(out != nullptr);
        std::fwrite(t.data(), 1, t.size(), out);
        std::fclose(out);
        return path;
    };
    REQUIRE_THROWS(
        cfg::load_game_toml(write_with("1.5", "t2_fc_hi.toml"), kAp));
    REQUIRE_THROWS(
        cfg::load_game_toml(write_with("-0.1", "t2_fc_lo.toml"), kAp));
    // And the OFF arm is a legal, loadable config — the A/B is real.
    const cfg::GameParams off =
        cfg::load_game_toml(write_with("0.0", "t2_fc_off.toml"), kAp);
    REQUIRE(off.ground.facet_contact == 0.0);
    std::printf("L1 bounds: 1.5 and -0.1 REJECTED by the loader; 0.0 loads "
                "(the OFF arm of the A/B)\n");
}

TEST_CASE("FACETCONTACT L1 identity BY BRANCH: at 0 the facet fn is not called") {
    const world::HeightField hf = uniform_field(120.0);
    sim::GroundParams gp = shipped_gp();
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});

    int calls = 0;
    std::function<double(glm::dvec3)> facet = [&](glm::dvec3 d) {
        ++calls;
        return hf.radius_at(d) + 10.0;
    };

    // The pre-T2 expression, verbatim.
    const double old_r_s = hf.radius_at(up) + gp.contact_height_m;

    gp.facet_contact = 0.0;
    const double r0 = sim::contact_radius(hf, gp, &facet, up);
    REQUIRE(calls == 0);           // BY BRANCH: never evaluated
    REQUIRE(r0 == old_r_s);        // bit-identical, not "within eps"

    // A NULL/EMPTY facet is ABSENT, whatever the dial says (every headless env).
    std::function<double(glm::dvec3)> empty;
    gp.facet_contact = 1.0;
    REQUIRE(sim::contact_radius(hf, gp, nullptr, up) == old_r_s);
    REQUIRE(sim::contact_radius(hf, gp, &empty, up) == old_r_s);
    REQUIRE(calls == 0);

    // ARMED: exactly the facet, no mix residue.
    const double r1 = sim::contact_radius(hf, gp, &facet, up);
    REQUIRE(calls == 1);
    REQUIRE(r1 == hf.radius_at(up) + 10.0 + gp.contact_height_m);

    // The mix is linear between them.
    gp.facet_contact = 0.5;
    const double rh = sim::contact_radius(hf, gp, &facet, up);
    REQUIRE(std::abs(rh - (old_r_s + 5.0)) < 1e-9);
    gp.facet_contact = 0.25;
    const double rq = sim::contact_radius(hf, gp, &facet, up);
    REQUIRE(std::abs(rq - (old_r_s + 2.5)) < 1e-9);
    std::printf("L1 identity: dial 0 -> %.6f (== pre-T2 %.6f, 0 facet calls); "
                "dial 1 -> %.6f; dial 0.5 -> %.6f\n",
                r0, old_r_s, r1, rh);
}

TEST_CASE("FACETCONTACT L1 identity: a full ground_contact tick is unmoved at 0") {
    // The whole contact law, not just the radius: a landing rollout tick with a
    // 10 m-high stub facet injected must produce the IDENTICAL next state at
    // dial 0 as with no facet at all.
    const world::HeightField hf = uniform_field(120.0);
    sim::GroundParams gp = shipped_gp();
    std::function<double(glm::dvec3)> facet = [&](glm::dvec3 d) {
        return hf.radius_at(d) + 10.0;
    };
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 head =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    const double dt = kAp.sim_dt;

    for (double sink : {0.0, 2.0, 9.0}) {
        sim::SimState st{};
        st.position = up * (hf.radius_at(up) + gp.contact_height_m + 0.3);
        st.velocity = head * 60.0 - up * sink;
        st.on_ground = false;
        sim::SimState base = st, armed = st;
        base.position = st.position + base.velocity * dt;
        armed.position = st.position + armed.velocity * dt;

        gp.facet_contact = 0.0;
        sim::ground_contact(st, base, hf, gp, kAp, 0.0, dt);        // no facet
        sim::ground_contact(st, armed, hf, gp, kAp, 0.0, dt, &facet);  // dial 0
        REQUIRE(armed.position == base.position);
        REQUIRE(armed.velocity == base.velocity);
        REQUIRE(armed.on_ground == base.on_ground);
        REQUIRE(armed.crashed == base.crashed);
    }
    std::printf("L1 tick identity: 3 sink rates, position/velocity/on_ground/"
                "crashed bit-equal with and without the injection at dial 0\n");
}

// ---------------------------------------------------------------------------
// L2 — Chad's report in four lines: airborne inside the drawn ground.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L2 an airframe inside the drawn ground is graded at 1.0") {
    const world::HeightField hf = uniform_field(120.0);
    sim::GroundParams gp = shipped_gp();
    std::function<double(glm::dvec3)> facet = [&](glm::dvec3 d) {
        return hf.radius_at(d) + 10.0;  // the drawn chord, 10 m above the field
    };
    const glm::dvec3 up = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const glm::dvec3 head =
        glm::normalize(glm::cross(up, glm::dvec3{0.2, 0.9, 0.4}));
    const double dt = kAp.sim_dt;

    // field + 5 m: five metres UNDER the drawn surface, five metres OVER the
    // crash field. Exactly the state Chad flew.
    auto run = [&](double dial, bool inject) {
        sim::GroundParams g = gp;
        g.facet_contact = dial;
        sim::SimState st{};
        st.position = up * (hf.radius_at(up) + 5.0);
        st.velocity = head * 100.0;  // LEVEL — no sink at all
        st.on_ground = false;
        sim::SimState next = st;
        next.position = st.position + next.velocity * dt;
        sim::ground_contact(st, next, hf, g, kAp, 0.0, dt,
                            inject ? &facet : nullptr);
        return next;
    };

    const sim::SimState off = run(0.0, true);
    REQUIRE(off.crashed == false);
    REQUIRE(off.on_ground == false);  // legally airborne INSIDE visible rock
    REQUIRE(glm::length(off.position) > hf.radius_at(up) + 4.9);

    const sim::SimState on = run(1.0, true);
    REQUIRE((on.crashed || on.on_ground));  // the ground is THERE now
    std::printf("L2 field+5 m under a facet at field+10 m: dial 0 -> crashed=%d "
                "on_ground=%d (flies through); dial 1 -> crashed=%d "
                "on_ground=%d\n",
                static_cast<int>(off.crashed), static_cast<int>(off.on_ground),
                static_cast<int>(on.crashed), static_cast<int>(on.on_ground));

    // And the airframe ABOVE the drawn surface is still airborne at 1.0 — the
    // dial raises the floor, it does not make everything a crash.
    {
        sim::GroundParams g = gp;
        g.facet_contact = 1.0;
        sim::SimState st{};
        st.position = up * (hf.radius_at(up) + 60.0);
        st.velocity = head * 100.0;
        st.on_ground = false;
        sim::SimState next = st;
        next.position = st.position + next.velocity * dt;
        sim::ground_contact(st, next, hf, g, kAp, 0.0, dt, &facet);
        REQUIRE(next.crashed == false);
        REQUIRE(next.on_ground == false);
    }
}

// ---------------------------------------------------------------------------
// L3 — the census over the real DEM: clip fraction at dial 0 vs dial 1,
//      RAW and BLURRED.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L3 the clip census closes at dial 1 (raw and blurred)") {
    world::HeightField raw = real_field();
    if (raw.w == 0) {
        WARN("assets/sudbury_dem.png missing — L3 skipped");
        return;
    }
    REQUIRE(raw.w == 8192);

    const Site sites[] = {
        {"ONAPING_VALLEY_PUMP", world::kPumpValleySurface},
        {"CTRL_VALLEY_CENTER", world::kValleyCenterDir},
        {"CTRL_SUDBURY_PUMP", world::kPumpSudburySurface},
        {"MURRAY_MOUTH", world::kTunnelMouthMurray},
    };

    std::printf("\n=========== T2 CLIP CENSUS (6 km box, 20 m step) ===========\n");
    std::printf("CLIP = the DRAWN surface (facet + %.2f m ambient snow) rides "
                "above the lowest legal airborne CG (r_s = mix(field, facet, "
                "dial) + %.2f m)\n",
                kAmbientSnowM, kContactHeightM);
    for (const Site& s : sites) {
        glm::dvec3 e1, e2;
        tangents(s.dir, e1, e2);
        // A BLURRED copy of this site's window (raylib's 3-box ImageBlurGaussian).
        world::HeightField blur = raw;
        blur_window(blur, s.dir, 3200.0, kDemBlurRadius);

        long n = 0, clip0_raw = 0, clip1_raw = 0, wall0_raw = 0, wall1_raw = 0;
        long clip0_bl = 0, clip1_bl = 0, wall0_bl = 0, wall1_bl = 0;
        double worst_raw = -1e9, worst_bl = -1e9;
        for (double x = -3000.0; x <= 3000.0 + 1e-9; x += 20.0) {
            for (double y = -3000.0; y <= 3000.0 + 1e-9; y += 20.0) {
                const glm::dvec3 d = offset_dir(s.dir, e1, e2, x, y);
                ++n;
                for (int arm = 0; arm < 2; ++arm) {
                    const world::HeightField& hf = arm == 0 ? raw : blur;
                    const double fi = hf.radius_at(d);
                    const double fa =
                        render::facet_radius_at(hf, d, kSubdiv, kTiles);
                    const double drawn = fa + kAmbientSnowM;
                    // dial 0: crash surface is the FIELD. dial 1: the FACET.
                    const double rs0 = fi + kContactHeightM;
                    const double rs1 = fa + kContactHeightM;
                    long& c0 = arm == 0 ? clip0_raw : clip0_bl;
                    long& c1 = arm == 0 ? clip1_raw : clip1_bl;
                    long& w0 = arm == 0 ? wall0_raw : wall0_bl;
                    long& w1 = arm == 0 ? wall1_raw : wall1_bl;
                    if (drawn > rs0) ++c0;
                    if (drawn > rs1) ++c1;
                    if (rs0 > drawn + 2.0) ++w0;  // the invisible wall, mirror sign
                    if (rs1 > drawn + 2.0) ++w1;
                    double& w = arm == 0 ? worst_raw : worst_bl;
                    w = std::max(w, fa - fi);
                }
            }
        }
        auto p = [&](long v) {
            return 100.0 * static_cast<double>(v) / static_cast<double>(n);
        };
        std::printf(
            "%-20s n=%ld\n"
            "    RAW     CLIP %.3f%% -> %.3f%%   WALL(>2m) %.3f%% -> %.3f%%   "
            "max(facet-field) %+.2f m\n"
            "    BLURRED CLIP %.3f%% -> %.3f%%   WALL(>2m) %.3f%% -> %.3f%%   "
            "max(facet-field) %+.2f m\n",
            s.name, n, p(clip0_raw), p(clip1_raw), p(wall0_raw), p(wall1_raw),
            worst_raw, p(clip0_bl), p(clip1_bl), p(wall0_bl), p(wall1_bl),
            worst_bl);
        // ⚠ T2b (red-team P1-3): THESE FOUR ARE TAUTOLOGIES, AND THEY ARE
        // LABELLED AS SUCH RATHER THAN SOLD AS A RESULT. This leg models the
        // drawn surface as `facet + kAmbientSnowM`, a CONSTANT; at dial 1 the
        // crash surface IS the facet, so CLIP asks facet + 0.77 > facet + 2.45
        // and WALL asks facet + 2.45 > facet + 0.77 + 2.0 -- both decided by
        // 0.77 < 2.45 before a single DEM texel is read. They are kept ONLY as
        // a tripwire on those two constants (a contact_height_m cut below
        // 2.77 m would fire WALL, and below 0.77 m would fire CLIP).
        //
        // THE REAL RESIDUAL -- the MEASURED drawn stack over the MEASURED
        // facet, render::drawn_radius_at - render::facet_radius_at, which is
        // the thing that decides whether dial 1 actually closes the clip -- is
        // L6 below. Read that one for the answer; this one only re-states the
        // arithmetic.
        REQUIRE(clip1_raw == 0);
        REQUIRE(clip1_bl == 0);
        REQUIRE(wall1_raw == 0);
        REQUIRE(wall1_bl == 0);
    }

    // ★ THE FINDING THAT RETIRES T1 CAVEAT 1. render/planet.cpp load_planet:
    //     if (procedural) dem_blur_radius = 0;
    // config/world.toml [ground] use_procedural = 1 for Sudbury, so the runtime
    // blur NEVER RUNS on the shipped planet — the DEM ships pre-blurred and
    // lake-flattened from the offline bake, and the retained HeightField is the
    // baked PNG verbatim. The RAW arm above IS the runtime field; the BLURRED
    // arm is a sensitivity bound, not the world.
    std::printf("\nL3 NOTE: load_planet forces dem_blur_radius = 0 on the "
                "procedural (Sudbury) path — the RAW arm IS the runtime field. "
                "T1 caveat 1 is retired.\n");
}

// ---------------------------------------------------------------------------
// L4 — the feel exposure: |facet - field| where he lands.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L4 the landing gap at the Sudbury spawn and the pump apron") {
    const world::HeightField hf = real_field();
    if (hf.w == 0) {
        WARN("assets/sudbury_dem.png missing — L4 skipped");
        return;
    }
    // app/spawn_policy.h: the Sudbury side is born over the MURRAY mouth, the
    // Valley side over ERRINGTON. The Valley PUMP apron is the marker landing.
    const Site sites[] = {
        {"SUDBURY_SPAWN(Murray)", world::kTunnelMouthMurray},
        {"VALLEY_SPAWN(Errington)", world::kTunnelMouthErrington},
        {"VALLEY_PUMP_APRON", world::kPumpValleySurface},
    };
    std::printf("\n=========== T2 LANDING GAP  |facet - field| "
                "(the metres arming the dial moves the touchdown) ===========\n");
    for (const Site& s : sites) {
        glm::dvec3 e1, e2;
        tangents(s.dir, e1, e2);
        std::vector<double> mag, signed_gap;
        for (double x = -250.0; x <= 250.0 + 1e-9; x += 2.0)
            for (double y = -250.0; y <= 250.0 + 1e-9; y += 2.0) {
                const glm::dvec3 d = offset_dir(s.dir, e1, e2, x, y);
                const double g =
                    render::facet_radius_at(hf, d, kSubdiv, kTiles) -
                    hf.radius_at(d);
                signed_gap.push_back(g);
                mag.push_back(std::abs(g));
            }
        const double mx = *std::max_element(mag.begin(), mag.end());
        const double p50 = pct(mag, 0.50), p90 = pct(mag, 0.90),
                     p99 = pct(mag, 0.99);
        const double s50 = pct(signed_gap, 0.50);
        std::printf("%-24s n=%zu  |gap| p50=%.3f  p90=%.3f  p99=%.3f  "
                    "MAX=%.3f m   (signed p50 %+.3f)\n",
                    s.name, mag.size(), p50, p90, p99, mx, s50);
        // The claim this fix was sold on: a normal touchdown moves CENTIMETRES.
        // Graded, not asserted tight — the number is reported either way.
        REQUIRE(p50 < 0.5);
    }
}

// ---------------------------------------------------------------------------
// L5 — the cost of one extra facet query per tick.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L5 cost: facet_radius_at per tick at 120 Hz") {
    const world::HeightField hf = real_field();
    if (hf.w == 0) {
        WARN("assets/sudbury_dem.png missing — L5 skipped");
        return;
    }
    glm::dvec3 e1, e2;
    tangents(world::kPumpValleySurface, e1, e2);
    std::vector<glm::dvec3> dirs;
    dirs.reserve(20000);
    // 20 000 samples along a 120 m/s track: 1 m apart, i.e. the actual cadence.
    for (int i = 0; i < 20000; ++i)
        dirs.push_back(offset_dir(world::kPumpValleySurface, e1, e2,
                                  static_cast<double>(i), 0.0));

    volatile double sink = 0.0;
    auto t0 = std::chrono::steady_clock::now();
    for (const glm::dvec3& d : dirs) sink = sink + hf.radius_at(d);
    auto t1 = std::chrono::steady_clock::now();
    for (const glm::dvec3& d : dirs)
        sink = sink + render::facet_radius_at(hf, d, kSubdiv, kTiles);
    auto t2 = std::chrono::steady_clock::now();

    const double field_ns =
        std::chrono::duration<double, std::nano>(t1 - t0).count() / dirs.size();
    const double facet_ns =
        std::chrono::duration<double, std::nano>(t2 - t1).count() / dirs.size();
    const double tick_us = 1e6 / 120.0;
    std::printf("\nL5 cost: radius_at %.0f ns/call, facet_radius_at %.0f "
                "ns/call (3 corner radius_at + the cell solve, O(1) — NOT a "
                "mesh walk). One extra per tick = %.4f%% of a %.0f us tick "
                "(assert-live build; RelWithDebInfo is faster).\n",
                field_ns, facet_ns, 100.0 * facet_ns * 1e-3 / tick_us, tick_us);
    // The guard that matters: it is O(1) in the mesh, so it cannot grow with
    // subdiv/tiles. A regression into a walk would blow this by orders.
    REQUIRE(facet_ns < 20000.0);
}

// ---------------------------------------------------------------------------
// L6 — THE REAL RESIDUAL (T2b, red-team P1-3).
//
// L3's "zero by construction" was a tautology over a constant. This one
// MEASURES the thing dial 1 does not cover: how far the DRAWN world rides above
// the terrain facet the aeroplane now crashes on.
//
//   residual(d) = render::drawn_radius_at(hf, d, N, tiles, &snow)   // drawn
//               - render::facet_radius_at(hf, d, N, tiles)          // crash
//
// drawn_radius_at IS facet_radius_at plus the folded snow, through the identical
// corner expression (render/sphere_param.h) — the same surface fill_face lays
// its vertices on. So this is exactly the stack of drawn material standing on
// the crash surface, measured over the real DEM, with the SHIPPED [snowpack]
// table (loaded, never hand-copied).
//
// THE BAR is contact_height_m (2.45 m): an airframe's CG rides that far above
// the contact point, so as long as the drawn stack is thinner than it, a plane
// sitting legally on the facet is never inside visible material.
//
// ★★★ THE FINDING (measured 2026-09-17). T2c: THIS LEG MEASURES IT, IT DOES
// NOT ASSERT THE BAR -- the strict check moved to FACETCONTACT L6-STRICT,
// hidden behind [.t3-strict], because the fix is a RULING OF CHAD'S and a
// ruling must not hold the gate hostage. What L6 still REQUIREs is only what
// must hold whatever he rules: the ambient fold is present (p50 within
// 0.6..1.4 x the shipped [snowpack] base_m), nothing is wild (max < 10 m), and
// the over-bar exposure is a tail (< 1% of points). The ANALYTIC road-drape
// clause is PRINTED and WARNed, never REQUIREd -- it is arithmetic on the
// table, not a measurement. The three options are in the doc, section 7.
// T2's doc believed the residual was empty on the arithmetic "ambient 0.77 +
// bank 1.30 < 2.45". BOTH HALVES OF THAT PREMISE ARE WRONG:
//   * [snowpack] base_m is 0.85, not 0.77, and 0.77 was never the ceiling
//     anyway -- ambient_depth_at MODULATES it by elevation, aspect (lee/
//     windward) and curvature, so the fold is a FIELD, not a constant.
//   * measured over the real DEM at the shipped mesh resolution:
//       ONAPING_VALLEY_PUMP  p50 0.757  p90 1.101  p99 1.748  MAX 3.945  (0.20% over 2.45)
//       CTRL_VALLEY_CENTER   p50 0.761  p90 1.054  p99 1.485  MAX 3.754  (0.11%)
//       CTRL_SUDBURY_PUMP    p50 0.769  p90 0.998  p99 1.308  MAX 3.665  (0.03%)
//       MURRAY_MOUTH         p50 0.777  p90 0.998  p99 1.160  MAX 2.475  (0.001%)
// So on the order of one point in a thousand -- the deep lee/hollow snow --
// the DRAWN ground stands up to 1.5 m higher than an aeroplane's whole CG
// height above the surface it now crashes on. dial 1 does NOT close the clip
// there; it closes it everywhere else.
//
// ⚠ THIS FIXTURE BINDS NO landmask AND NO barren RASTER (render/draw.h is the
// raylib side; seads_tests cannot reach it), and both of those only ever TAKE
// SNOW AWAY (water flattens, black rock thins to bare_rock_depth_m). So this
// is an UPPER BOUND on the shipped world, not the shipped world -- but the
// lee/curvature term that makes the tail is real either way.
//
// NOT A BOUND TO LOOSEN, and not fixable inside this rung: the choices are
// Chad's (raise contact_height_m, fold the ambient depth into the aircraft's
// contact surface the way drive_radius_at does for the sled, or accept that a
// plane parked in 4 m of snow is buried). Ruling owed.
//
// ★ AND THE ROAD DRAPE, WHICH THIS FIELD SAMPLE CANNOT SEE. Road decks and
// banks are SEPARATE MESHES draped on drawn_radius_at, and no LineNetwork is
// bound here, so the measured residual is the ambient snow alone. The drape
// terms are therefore added ANALYTICALLY from the shipped table and REPORTED
// too (asserted only in the hidden strict leg) — they are ALTERNATIVES at a station (a deck OR a bank crest), never a
// stack, so the worst drawn material at any point is
//     ambient + max([ribbons] lift_m, [snowpack] bank_height_m).
// If that ever exceeds contact_height_m, the residual is NOT empty -- which is
// the finding, not a bound to loosen.
// ---------------------------------------------------------------------------
namespace {

// One site's residual distribution. Measured once, read by BOTH the measurement
// leg (L6) and the hidden strict leg (L6-STRICT) so the two can never grade
// different numbers.
struct Residual {
    double p50 = 0.0, p90 = 0.0, p99 = 0.0, mx = 0.0, mn = 0.0;
    long over = 0;
    std::size_t n = 0;
    double over_frac() const { return n == 0 ? 0.0 : double(over) / double(n); }
};

Residual measure_residual(const world::HeightField& hf,
                          const world::SnowpackField& snow, glm::dvec3 dir) {
    glm::dvec3 e1, e2;
    tangents(dir, e1, e2);
    std::vector<double> res;
    for (double x = -3000.0; x <= 3000.0 + 1e-9; x += 20.0)
        for (double y = -3000.0; y <= 3000.0 + 1e-9; y += 20.0) {
            const glm::dvec3 d = offset_dir(dir, e1, e2, x, y);
            res.push_back(render::drawn_radius_at(hf, d, kSubdiv, kTiles, &snow) -
                          render::facet_radius_at(hf, d, kSubdiv, kTiles));
        }
    Residual r;
    r.n = res.size();
    r.mx = *std::max_element(res.begin(), res.end());
    r.mn = *std::min_element(res.begin(), res.end());
    r.p50 = pct(res, 0.50);
    r.p90 = pct(res, 0.90);
    r.p99 = pct(res, 0.99);
    for (double v : res)
        if (v > kContactHeightM) ++r.over;
    return r;
}

const Site kResidualSites[] = {
    {"ONAPING_VALLEY_PUMP", world::kPumpValleySurface},
    {"CTRL_VALLEY_CENTER", world::kValleyCenterDir},
    {"CTRL_SUDBURY_PUMP", world::kPumpSudburySurface},
    {"MURRAY_MOUTH", world::kTunnelMouthMurray},
};

// THE LOOSE INVARIANTS — the ones that must hold WHATEVER Chad rules, because
// each is a statement about the ambient fold being present and bounded, not
// about where the bar sits:
//   the ambient fold IS THERE, and the median sits where [snowpack] base_m
//   says it should. T2c red-team P1-7: DERIVED FROM THE SHIPPED TABLE, not an
//   absolute pair -- a base_m retune must move this band with it, or the leg
//   would quietly start grading a world nobody ships.
constexpr double kAmbientP50LoFrac = 0.6;
constexpr double kAmbientP50HiFrac = 1.4;
//   no wild residual — a chord bridging 10 m of drawn snow would be a bug in
//   the fold, not a ruling question
constexpr double kResidualSaneMaxM = 10.0;
//   the exposure that needs the ruling is a TAIL: under one point in a hundred
constexpr double kOverBarMaxFrac = 0.01;

}  // namespace

TEST_CASE("FACETCONTACT L6 the drawn-above-facet residual, measured") {
    const world::HeightField hf = real_field();
    if (hf.w == 0) {
        WARN("assets/sudbury_dem.png missing - L6 skipped");
        return;
    }
    const cfg::WorldParams kWorld =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    // The shipped [planet] resolution, pinned against this file's constants so
    // a world.toml retune cannot leave L1..L5 measuring a mesh nobody builds.
    REQUIRE(kWorld.planet.subdiv == kSubdiv);
    REQUIRE(kWorld.planet.tiles == kTiles);

    world::SnowpackField snow;
    snow.hf = &hf;
    snow.p = kWorld.snowpack;  // world::SnowParams IS the [snowpack] block

    std::printf("\n=========== T2b DRAWN-ABOVE-FACET RESIDUAL "
                "(drawn_radius_at - facet_radius_at, 6 km box, 20 m step) "
                "===========\n");
    std::printf("bar = contact_height_m %.2f m   ambient snow base_m %.2f\n",
                kContactHeightM, snow.p.base_m);
    double worst_all = -1e9;
    double worst_over_frac = 0.0;
    for (const Site& s : kResidualSites) {
        const Residual r = measure_residual(hf, snow, s.dir);
        worst_all = std::max(worst_all, r.mx);
        worst_over_frac = std::max(worst_over_frac, r.over_frac());
        std::printf("%-20s n=%zu  residual p50=%.3f  p90=%.3f  p99=%.3f  "
                    "MAX=%.3f  (min %+.3f)  over %.2f m: %ld (%.4f%%)\n",
                    s.name, r.n, r.p50, r.p90, r.p99, r.mx, r.mn,
                    kContactHeightM, r.over, 100.0 * r.over_frac());
        INFO(s.name << " p50 " << r.p50 << " max " << r.mx);
        if (r.mx >= kContactHeightM)
            WARN(s.name << ": drawn material stands " << r.mx
                        << " m over the crash facet at the worst point, vs "
                           "contact_height_m "
                        << kContactHeightM
                        << " m -- a plane parked legally on the facet is "
                           "BURIED there. Share over the bar: "
                        << 100.0 * r.over_frac()
                        << " %. RULING OWED: docs/terrain_clip/"
                           "T2_facet_contact.md section 7.");

        // LOOSE INVARIANTS ONLY. The strict bar (mx < contact_height_m) lives
        // in the hidden [.t3-strict] leg below: it is Chad's ruling, not this
        // leg's, and T3 flips it green once he has ruled.
        // The band is base_m-relative (P1-7): shipped base_m 0.85 -> (0.51, 1.19).
        REQUIRE(r.p50 > kAmbientP50LoFrac * snow.p.base_m);  // the fold is there
        REQUIRE(r.p50 < kAmbientP50HiFrac * snow.p.base_m);  // and it is ambient
        REQUIRE(r.mx < kResidualSaneMaxM);  // no wild residual
        REQUIRE(r.over_frac() < kOverBarMaxFrac);  // the exposure is a TAIL
    }

    // The road drape, analytically, from the shipped table (see the banner).
    const double lift_m = kWorld.ribbons.lift_m;
    const double bank_m = snow.p.bank_height_m;
    const double worst_drape = worst_all + std::max(lift_m, bank_m);
    std::printf("\nL6 WORST measured residual %.3f m over all four sites "
                "(worst over-bar share %.4f%%).\n"
                "   road drape (ALTERNATIVES at a station, not a stack): "
                "[ribbons] lift_m %.2f | [snowpack] bank_height_m %.2f\n"
                "   worst drawn material over the crash facet = %.3f + %.2f = "
                "%.3f m   vs contact_height_m %.2f m -> %s\n"
                "   THIS LEG MEASURES, IT DOES NOT ASSERT THE BAR. The ruling "
                "is Chad's: docs/terrain_clip/T2_facet_contact.md section 7.\n",
                worst_all, 100.0 * worst_over_frac, lift_m, bank_m, worst_all,
                std::max(lift_m, bank_m), worst_drape, kContactHeightM,
                worst_drape < kContactHeightM ? "RESIDUAL EMPTY"
                                              : "RESIDUAL NON-EMPTY (finding)");
    if (worst_drape >= kContactHeightM)
        WARN("worst drawn material (ambient + road drape) "
             << worst_drape << " m exceeds contact_height_m " << kContactHeightM
             << " m -- RULING OWED.");
}

// ---------------------------------------------------------------------------
// L6-STRICT — THE BAR, hidden behind [.t3-strict] until Chad rules.
//
// This is the assert L6 used to carry: the drawn stack must not out-reach the
// aeroplane's CG height, or a plane parked legally on the facet is inside
// visible material. It is RED on this tree, by design and by measurement, and
// it is HIDDEN so a ruling that is not this lane's to make cannot hold the gate
// hostage. T3 flips it green by whichever of the three options in the doc Chad
// picks. Nothing here is a bound to loosen.
//
//   Run it by tag (a hidden test is not selected by -R):
//       build/seads_tests.exe "[.t3-strict]"
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L6-STRICT the residual fits under the CG height",
          "[.t3-strict]") {
    const world::HeightField hf = real_field();
    if (hf.w == 0) {
        WARN("assets/sudbury_dem.png missing - L6-STRICT skipped");
        return;
    }
    const cfg::WorldParams kWorld =
        cfg::load_world_toml(SEADS_CONFIG_DIR "/world.toml");
    world::SnowpackField snow;
    snow.hf = &hf;
    snow.p = kWorld.snowpack;

    double worst_all = -1e9;
    for (const Site& s : kResidualSites) {
        const Residual r = measure_residual(hf, snow, s.dir);
        worst_all = std::max(worst_all, r.mx);
        INFO(s.name << " max residual " << r.mx);
        REQUIRE(r.mx < kContactHeightM);
    }
    const double worst_drape =
        worst_all + std::max(kWorld.ribbons.lift_m, snow.p.bank_height_m);
    REQUIRE(worst_drape < kContactHeightM);
}

// ---------------------------------------------------------------------------
// L7 — THE BUILDING PRISMS SIT ON THE SAME SURFACE (T2b, red-team P1-2).
//
// Before T2b the prism base was hf.radius_at(center_dir) while the terrain
// under it had moved to the facet. On a hillside that puts a house's FOOT a
// facet-gap below the rock you can see, and the prism's lower band
// (base - base_margin) misses an airframe that is visibly inside the building.
// obstacle_contact now hands BuildingColliders::hit the same
// sim::air_ground_radius the terrain contact uses.
//
//   identity  — dial 0 / no injection: the hit verdict is the pre-T2 one, and
//               the facet function is NEVER CALLED (counted).
//   behaviour — a prism whose field base is 40 m below its facet base: an
//               airframe at facet height is INSIDE the drawn building at dial
//               1 and legally in clear air at dial 0.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L7 building prisms are based on the contact surface") {
    const world::HeightField hf = uniform_field(120.0);
    const glm::dvec3 dir = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const double base_field = hf.radius_at(dir);

    // ONE building, 20 m tall, wide enough that the query direction is inside.
    std::vector<world::BuildingColliders::Prism> raw;
    world::BuildingColliders::Prism P;
    P.dir = dir;
    P.radius_m = 60.0;
    P.height_m = 20.0;
    raw.push_back(P);
    world::BuildingColliders obs;
    obs.build(raw, 256, 128, 0.0, 10.0, kAp.R);

    sim::GroundParams gp = shipped_gp();
    gp.obstacle_inflate_m = 0.0;
    gp.obstacle_base_margin_m = 5.0;

    int calls = 0;
    std::function<double(glm::dvec3)> facet = [&](glm::dvec3 d) {
        ++calls;
        return hf.radius_at(d) + 40.0;  // the drawn chord, 40 m above the field
    };

    auto graded = [&](double dial, double alt_above_field, bool inject) {
        sim::GroundParams g = gp;
        g.facet_contact = dial;
        sim::SimState next{};
        next.position = dir * (base_field + alt_above_field);
        next.crashed = false;
        sim::obstacle_contact(next, obs, hf, g, inject ? &facet : nullptr);
        return next.crashed;
    };

    // IDENTITY BY BRANCH. At dial 0 the injected sampler is not called, and the
    // verdict is the pre-T2 one at every altitude probed.
    calls = 0;
    for (double a : {2.0, 10.0, 30.0, 45.0, 55.0}) {
        REQUIRE(graded(0.0, a, true) == graded(0.0, a, false));
        REQUIRE(graded(1.0, a, false) == graded(0.0, a, false));  // absent==off
    }
    REQUIRE(calls == 0);

    // BEHAVIOUR. 45 m above the FIELD is 5 m above the facet base: inside the
    // 20 m prism once the base follows the drawn ground, clear air before.
    REQUIRE(graded(0.0, 45.0, true) == false);
    REQUIRE(graded(1.0, 45.0, true) == true);
    // And 2 m above the field — inside the prism on the OLD base — is now
    // 38 m BELOW the building's foot, past the 5 m margin: no longer a hit.
    REQUIRE(graded(0.0, 2.0, true) == true);
    REQUIRE(graded(1.0, 2.0, true) == false);
    std::printf("L7 prism base follows the contact surface: at field+45 m "
                "(facet+5 m) dial 0 -> clear, dial 1 -> CRASH; the facet fn is "
                "never called at dial 0 (calls=%d over 10 probes)\n",
                calls);
}

// ---------------------------------------------------------------------------
// L8 — THE ARGUMENT REACHES THE GAME (T2c, red-team P0-1).
//
// L7 grades sim::obstacle_contact DIRECTLY. That is not the game: the game
// reaches it through sim::step, which owns the call and therefore owns whether
// the facet pointer is passed at all. T2b fixed the callee and left the ONE
// call site in sim/step.cpp passing four arguments -- so the prism-base fix was
// correct, tested, and INERT in Chad's build. A leg that only ever calls the
// callee can never see that class of defect again, so this one drives the whole
// thing from sim::step at ENV level, exactly as app/main.cpp does:
//
//   ARMED     — env.ground_facet_fn injected, [ground] facet_contact 1: an
//               airframe 45 m above the DEM field (5 m above the drawn facet)
//               is INSIDE the drawn building and sim::step must crash it.
//   IDENTITY  — the same env at dial 0: not crashed, and the injected sampler
//               is NEVER CALLED (counted across the whole tick, terrain
//               contact included).
//
// If the step.cpp call site ever drops the argument again, the ARMED half goes
// red here and nowhere else.
// ---------------------------------------------------------------------------
TEST_CASE("FACETCONTACT L8 the prism base follows the facet through sim::step") {
    const world::HeightField hf = uniform_field(120.0);
    const glm::dvec3 dir = glm::normalize(glm::dvec3{0.55, 0.15, 0.82});
    const double base_field = hf.radius_at(dir);

    std::vector<world::BuildingColliders::Prism> raw;
    world::BuildingColliders::Prism P;
    P.dir = dir;
    P.radius_m = 60.0;
    P.height_m = 20.0;
    raw.push_back(P);
    world::BuildingColliders obs;
    obs.build(raw, 256, 128, 0.0, 10.0, kAp.R);

    int calls = 0;
    // The drawn chord, 40 m above the field. Terrain contact cannot reach the
    // airframe in EITHER arm at field+45 m (facet + contact_height_m = +42.45),
    // so the only thing that can crash it is the BUILDING -- which is the
    // whole point of the leg.
    auto facet_fn = [&](glm::dvec3 d) {
        ++calls;
        return hf.radius_at(d) + 40.0;
    };

    auto flown = [&](double dial, bool inject) {
        sim::Environment env{};
        env.ground = &hf;
        env.obstacles = &obs;
        env.ground_params = shipped_gp();
        env.ground_params.obstacle_inflate_m = 0.0;
        env.ground_params.obstacle_base_margin_m = 5.0;
        env.ground_params.facet_contact = dial;
        if (inject) env.ground_facet_fn = facet_fn;  // else EMPTY == absent

        sim::SimState st{};
        st.position = dir * (base_field + 45.0);
        st.orientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
        st.on_ground = false;
        st.crashed = false;
        const sim::Inputs in{};
        return sim::step(st, in, kAp, &env, kAp.sim_dt).crashed;
    };

    // ARMED: the prism's foot rides the drawn facet, and the aeroplane visibly
    // inside the building is killed by it.
    calls = 0;
    REQUIRE(flown(1.0, true) == true);
    REQUIRE(calls > 0);  // the injection really did reach the kernel

    // IDENTITY BY BRANCH, through the same door: dial 0 never calls it and the
    // verdict is the pre-T2 one (clear air, 43 m under the field-based foot).
    calls = 0;
    REQUIRE(flown(0.0, true) == false);
    REQUIRE(calls == 0);
    REQUIRE(flown(1.0, false) == false);  // absent function == off at any dial
    REQUIRE(calls == 0);

    std::printf("L8 sim::step -> obstacle_contact: at field+45 m (facet+5 m) "
                "dial 1 CRASHES on the building, dial 0 flies clear, and the "
                "facet fn is never called at dial 0 (calls=%d)\n",
                calls);
}
