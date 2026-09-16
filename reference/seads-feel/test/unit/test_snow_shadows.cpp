// ★ R5 ROW 9 — SHADOWS ON SNOW (docs/snow_R5_immersion_ledger.md ROW 9).
// The mechanism is receiver-side analytic occluder proxies (option (c)):
// capsules tested per-fragment in the planet shader, so the shadow lands at
// the rasterized pixel's own position on whatever surface drew it and the
// fence-3 under-the-world class is unrepresentable.
//
// THE ROW'S OWN FENCE, PINNED THE WAY ROW 3 PINNED ITS TINT: zero casters in
// range => shadow factor EXACTLY 1.0, by an EARLY-OUT ON CASTER COUNT — never
// a multiply by something that rounds to 1.0. If that is ever wrong, every
// pixel on the planet moves. The shader halves live in the exact FORM of the
// planet.cpp lines (the smoothstep/needle discipline of test_winter_reskin);
// the CPU halves (measured caster dims, the altitude gate) are exercised
// directly through render/shadow_casters.h, which is pure glm and raylib-free
// by construction.
//
// ⚠ HONEST COVERAGE NOTE: these legs pin shader SOURCE FORMS and CPU
// geometry. The GLSL is never compiled headlessly in this repo — a shader
// error surfaces only at launch.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "config/load_world.h"
#include "render/rider_pose.h"      // kSagDefaultM, sled_sag0_m (raylib-free)
#include "render/shadow_casters.h"  // PURE (glm + rig specs), no raylib
#include "sim/sled.h"               // SledParams: the measured dims chart

namespace {

double luma(double r, double g, double b) {
    return 0.299 * r + 0.587 * g + 0.114 * b;
}

std::string read_file(const std::string& path, bool* ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        *ok = false;
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    *ok = true;
    std::string s = ss.str();
    // ★★★ EOL-INSENSITIVE, AND THE FOURTH INSTANCE OF THIS FAMILY IN THIS REPO.
    // The patterns below span line boundaries (a bare "\n" between concatenated
    // fragments), and this stream is BINARY on purpose -- so on a checkout under
    // `core.autocrlf=true` the file carries "\r\n" and no spanning pattern can
    // ever match. That makes the leg green ONLY in the tree that WROTE
    // planet.cpp (autocrlf converts on CHECKOUT, not on write) and red in every
    // fresh checkout -- verbatim the failure .gitattributes already documents
    // for the bake manifest, projection.lock, MANIFEST.lock and the soundbank
    // TSV. Fixed here, at the READ, rather than by pinning one more path to LF:
    // the pin is per-instance and cannot even repair an already-checked-out
    // tree, while this covers every assertion in the file, in both trees, now.
    //
    // ★ IT CANNOT MASK A DEFECT. These legs pin C++/GLSL TOKEN TEXT, and a '\r'
    // occurs only at a physical line end. The shader the GPU receives is built
    // from the "\n" ESCAPES INSIDE the literals, never from the file's own line
    // endings -- so the file's EOL is provably not load-bearing for anything
    // shipped, and stripping it moves the read toward the LF form these legs
    // were authored and signed against.
    //
    // ⚠ DO NOT copy this into a reader of genuine BINARY data (the sled-tape
    // and GIS readers): there a '\r' is payload, and erasing it is corruption.
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

const std::string kRoot = std::string(SEADS_ASSET_DIR) + "/..";

}  // namespace

// --------------------------------------------------------------------------
// ★ THE FENCE — zero casters => the shipped pixel, by early-out.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 9: zero casters is an EARLY-OUT on uShadowCount -- the "
          "shipped lit expression survives verbatim") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);

    // The uniforms reach the shader and their locations are fetched — the
    // array locs by the GLSL-spec "[0]" spelling.
    REQUIRE(src.find("uniform int uShadowCount;") != std::string::npos);
    REQUIRE(src.find("uniform vec4 uShadowA[16];") != std::string::npos);
    REQUIRE(src.find("uniform vec4 uShadowB[16];") != std::string::npos);
    REQUIRE(src.find("uniform vec3 uShadowTint;") != std::string::npos);
    REQUIRE(src.find("uniform float uShadowStrength;") != std::string::npos);
    REQUIRE(src.find("uniform float uShadowTanSun;") != std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uShadowCount\")") !=
            std::string::npos);
    REQUIRE(src.find("GetShaderLocation(p.shader, \"uShadowA[0]\")") !=
            std::string::npos);

    // ★ THE ZERO-CASTER FORM. shadowOcc is born the literal 0.0 and is only
    // ever written INSIDE the count guard; the shipped lit line is present
    // UNMODIFIED; the shadow's only write to lit is a guarded subtraction.
    // With count 0 neither guard is entered, so the pixel's dataflow is the
    // pre-row-9 one to the bit — an early-out, not a multiply by ~1.0.
    REQUIRE(src.find("\"    float shadowOcc = 0.0;\\n\"") != std::string::npos);
    REQUIRE(src.find("\"    if (uShadowCount > 0) {\\n\"\n"
                     "           \"        shadowOcc = shadow_sun_occ(fragRel,"
                     " -normalize(sunDir));\\n\"") != std::string::npos);
    REQUIRE(src.find("\"    vec3 lit = albedo * (uNightFill + uGroundDayGain "
                     "* ndl);\\n\"") != std::string::npos);
    REQUIRE(src.find("\"    if (uShadowCount > 0) {\\n\"\n"
                     "           \"        lit -= albedo * (uGroundDayGain * "
                     "ndl * shadowOcc) *\\n\"") != std::string::npos);

    // ★ THE AMBIENT SURVIVES. The subtraction reaches only the
    // uGroundDayGain*ndl product — uNightFill must not appear inside it (a
    // snow shadow is sky-lit, never black).
    const std::size_t sub =
        src.find("lit -= albedo * (uGroundDayGain * ndl * shadowOcc)");
    REQUIRE(sub != std::string::npos);
    CHECK(src.substr(sub, 140).find("uNightFill") == std::string::npos);

    // ★ ROW 8 STAYS SIGNED. The sparkle dies inside a shadow through the SAME
    // early-out — a guarded compound multiply, so at zero casters row 8's
    // shipped gate expression is untouched.
    REQUIRE(src.find("if (uShadowCount > 0) { ssGate *= 1.0 - shadowOcc; }") !=
            std::string::npos);

    // Fence 2 (arm discipline): the count is set EVERY frame in
    // draw_planet_mesh, never trusted to a previous frame's uniform.
    REQUIRE(src.find("SetShaderValue(p.shader, p.loc_shadow_count, "
                     "&shadows.count,") != std::string::npos);
}

// --------------------------------------------------------------------------
// Penumbra: distance-proportional from the SHIPPED 1.40-deg sun — the
// descent-sharpening IS the landing cue, so no fixed blur may replace it.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 9: penumbra scales with occluder distance off the shipped "
          "sun diameter") {
    bool ok = false;
    const std::string src = read_file(kRoot + "/render/planet.cpp", &ok);
    REQUIRE(ok);
    // The soft edge is tan(sun radius) * t — t is the per-fragment occluder
    // distance, so the shadow crisps as the caster descends (probe: 7.3 m at
    // 300 m, 0.37 m at flare). The epsilon floor only guards t -> 0.
    REQUIRE(src.find("float pen = max(uShadowTanSun * t, 1e-2);") !=
            std::string::npos);

    bool dok = false;
    const std::string dsrc = read_file(kRoot + "/render/draw.cpp", &dok);
    REQUIRE(dok);
    // tan_sun single-sources from the SAME SunParams the drawn disc uses.
    REQUIRE(dsrc.find("std::tan(static_cast<float>(info.sun.ang_radius_rad))") !=
            std::string::npos);

    // The shipped sun is the 4x one (1.40 deg) — the consult's 0.35 stays
    // dead. tan(0.70 deg) = 0.012217 -> penumbra ~7.33 m at 300 m slant.
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    CHECK(w.celestial.sun_angular_diameter_deg == Catch::Approx(1.40));
}

// --------------------------------------------------------------------------
// The look: [shadows] is darker AND bluer, inside the real-snow 30-50% band.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 9: the shadow tint obeys the ribbons.h ruling and lands a "
          "30-50% shadow, not black and not grey") {
    const auto w = cfg::load_world_toml(kRoot + "/config/world.toml");
    REQUIRE(w.shadows.enabled);
    CHECK(w.shadows.strength >= 0.0);
    CHECK(w.shadows.strength <= 1.0);
    // Darker AND bluer (ribbons.h:37-47 / S1_SPEC RT-1) — the same ordering
    // rows 3 and 8 obeyed, loader-enforced too; this leg keeps the ship
    // values honest even if the loader check is ever loosened.
    CHECK(w.shadows.tint.b > w.shadows.tint.r);
    CHECK(w.shadows.tint.g > w.shadows.tint.r);
    // Full-umbra brightness relative to sunlit snow, with the SHIPPED
    // lighting: (fill + gain*ndl*lum(tint)) / (fill + gain*ndl) at a high sun
    // (ndl 0.8). Real snow shadows sit around 30-50% of sunlit; raw contrast
    // at this insertion point is near-black, which is exactly what the tint
    // dial exists to prevent.
    const double fill = w.atmosphere.night_fill_min;
    const double gain = w.atmosphere.ground_day_gain;
    const double ndl = 0.8;
    const double tl =
        luma(w.shadows.tint.r, w.shadows.tint.g, w.shadows.tint.b);
    const double frac =
        (fill + gain * ndl * tl) / (fill + gain * ndl);
    CHECK(frac > 0.30);
    CHECK(frac < 0.50);
}

// --------------------------------------------------------------------------
// Caster 1 — the aircraft proxies are MEASURED, not typed.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 9: aircraft proxy dims equal the probe's measured airframe") {
    const render::AircraftShadowDims d = render::aircraft_shadow_dims();
    // The row-9 probe's published numbers, from the same accumulation over
    // aircraft_node_specs box_dims: span 10.10 m, length 8.90 m, chord
    // 2.40 m (docs/snow_row9_shadow_probe_results.md).
    CHECK(2.0 * d.half_span == Catch::Approx(10.10).margin(0.01));
    CHECK(d.fus_z_max - d.fus_z_min == Catch::Approx(8.90).margin(0.02));
    CHECK(2.0 * d.wing_radius == Catch::Approx(2.40).margin(0.001));
    // Wing group centre straight off the spec chain (fuselage {0,0,0.9} +
    // wing {±2.55,-0.25,0.2}).
    CHECK(d.wing_y == Catch::Approx(-0.25));
    CHECK(d.wing_z == Catch::Approx(1.1));
    // Tailplane group: hstab at ±1.15 with a 2.0 box -> half-span 2.15.
    CHECK(d.tail_half_span == Catch::Approx(2.15));
    CHECK(d.tail_radius > 0.0);

    // The capsules preserve the measured OVERALL extents (endpoints are inset
    // by the radius; a capsule adds its radius past each endpoint).
    std::vector<render::ShadowCaster> out;
    render::aircraft_shadow_proxies(out, glm::dvec3{0.0},
                                    glm::dquat{1.0, 0.0, 0.0, 0.0});
    REQUIRE(out.size() == 3);
    const auto& fus = out[0];
    CHECK((fus.b.z + fus.radius) - (fus.a.z - fus.radius) ==
          Catch::Approx(8.90).margin(0.02));
    const auto& wing = out[1];
    CHECK((wing.b.x + wing.radius) - (wing.a.x - wing.radius) ==
          Catch::Approx(10.10).margin(0.01));

    // Pose: capsules follow the body->world transform (nose -Z convention) —
    // a 90-deg yaw about +Y maps the wing span axis onto world Z.
    out.clear();
    const glm::dquat yaw90 =
        glm::angleAxis(glm::radians(90.0), glm::dvec3{0.0, 1.0, 0.0});
    render::aircraft_shadow_proxies(out, glm::dvec3{100.0, 0.0, 0.0}, yaw90);
    const glm::dvec3 span_axis = glm::normalize(out[1].b - out[1].a);
    CHECK(std::abs(span_axis.x) < 1e-9);
    CHECK(std::abs(std::abs(span_axis.z) - 1.0) < 1e-9);
}

// --------------------------------------------------------------------------
// Caster 2 — the sled's six proxies: chart-read dims, GLB-measured
// silhouette, and the drawn machine's own articulation law. Chad's ruling
// (2026-08-28): the shadow "ACTUALLY FOLLOWS AND PROJECTS THE MEASURED
// OUTLINE".
// --------------------------------------------------------------------------

namespace {

// The one dims builder every sled leg uses — chart values READ from
// sim::SledParams (a re-derived copy is an H1 fork), articulation zeroed.
render::SledShadowDims sled_dims() {
    const sim::SledParams sp{};  // the MEASURED chart (sim/sled.h:388)
    render::SledShadowDims d;
    d.cg_h = sp.cg_height_m;
    d.half_stance = 0.5 * sp.stance_m;
    d.ski_half_w = 0.5 * sp.ski_width_m;
    d.track_half_w = 0.5 * sp.track_width_m;
    d.sag0 = render::kSagDefaultM;  // the named default (env not read here)
    // â MEASURED seated helmet CROWN (measure_helmet_crown.py, CPU-skinned,
    // asserts 44 skin joints = Sudburian). Was 1.25, which rider_pose.h never
    // measured -- it was the cowl top plus an allowance, and it left the
    // shadow's head 0.32 m short.
    d.helmet_top = 1.574660;
    d.rider_radius = 0.25;  // documented approximation (main.cpp)
    return d;
}

// Proxy emission order (shadow_casters.h): ski +X (kernel L), ski -X,
// track belt, tunnel+seat, hood, rider.
enum { kSkiL = 0, kSkiR = 1, kBelt = 2, kTunnel = 3, kHood = 4, kRider = 5 };

std::vector<render::ShadowCaster> sled_at_identity(
    const render::SledShadowDims& d) {
    std::vector<render::ShadowCaster> out;
    render::sled_shadow_proxies(out, glm::dvec3{0.0}, glm::dmat3{1.0}, d);
    return out;
}

}  // namespace

TEST_CASE("R5 row 9: sled silhouette -- two runners at the stance, a belt on "
          "the centreline, tunnel+seat, hood, rider, all measured") {
    const sim::SledParams sp{};
    const render::SledShadowDims d = sled_dims();
    const auto out = sled_at_identity(d);
    REQUIRE(out.size() == 6);

    // ★ THE NO-FORK CROSS-CHECK: the GLB kingpin |x| equals the chart's
    // stance/2 EXACTLY (both 0.4635) — if the chart ever moves, this leg
    // breaks loudly instead of the silhouette silently forking.
    CHECK(render::sled_glb::kSkiPivotZ == Catch::Approx(0.955));
    CHECK(0.5 * sp.stance_m == Catch::Approx(0.4635));
    // GLB ski span = the chart's ski_len_m; GLB belt width ~ track_width_m.
    CHECK(render::sled_glb::kSkiZMax - render::sled_glb::kSkiZMin ==
          Catch::Approx(sp.ski_len_m));

    // SKIS: thin runners, radius = half the chart ski width, centred at
    // +-stance/2, spanning the measured mesh z (body z = -model z).
    for (int i : {kSkiL, kSkiR}) {
        const auto& s = out[i];
        CHECK(s.radius == Catch::Approx(0.5 * sp.ski_width_m));
        const double want_x = (i == kSkiL ? -1.0 : 1.0) * 0.5 * sp.stance_m;
        CHECK(s.a.x == Catch::Approx(want_x));  // model +X (kernel L) ->
        CHECK(s.b.x == Catch::Approx(want_x));  // body -X
        CHECK(std::min(s.a.z, s.b.z) - s.radius ==
              Catch::Approx(-render::sled_glb::kSkiZMax));
        CHECK(std::max(s.a.z, s.b.z) + s.radius ==
              Catch::Approx(-render::sled_glb::kSkiZMin));
        // At susp_x = 0 the runner bottom lands at the kernel contact height
        // body -cg_h + susp = -cg_h (pose_pass's own law, sag0 cancelling).
        CHECK(s.a.y - s.radius ==
              Catch::Approx(render::sled_glb::kSkiBotY - sp.cg_height_m));
    }

    // BELT: half the chart track width, on the centreline, the measured loop
    // footprint (NOT the 1.14 contact patch — the plan view shows the loop).
    const auto& b = out[kBelt];
    CHECK(b.radius == Catch::Approx(0.5 * sp.track_width_m));
    CHECK(b.a.x == Catch::Approx(0.0));
    CHECK(std::max(b.a.z, b.b.z) + b.radius ==
          Catch::Approx(-render::sled_glb::kBeltZMin));  // tail, body +z

    // TUNNEL+SEAT: measured tunnel half-width; capsule top 3 mm shy of the
    // measured seat crown (0.420 diameter vs the 0.423 stack — documented).
    const auto& t = out[kTunnel];
    CHECK(t.radius == Catch::Approx(render::sled_glb::kTunnelHalfW));
    const double drop = sp.cg_height_m - render::kSagDefaultM;
    CHECK(std::max(t.a.y, t.b.y) + t.radius ==
          Catch::Approx(render::sled_glb::kTunnelBotY +
                        2.0 * render::sled_glb::kTunnelHalfW - drop));

    // HOOD: the widest plan footprint (pan half-width 0.449), spanning the
    // measured pan z; its top reaches within 1.3 cm of the measured cowl top
    // (0.892), so no separate cowl proxy is needed.
    const auto& h = out[kHood];
    CHECK(h.radius == Catch::Approx(render::sled_glb::kPanHalfW));
    CHECK(std::min(h.a.z, h.b.z) - h.radius ==
          Catch::Approx(-render::sled_glb::kPanZMax));  // nose, body -z
    const double hood_top_model =
        0.5 * (render::sled_glb::kPanBotY + render::sled_glb::kShroudTopY) +
        render::sled_glb::kPanHalfW;
    CHECK(hood_top_model == Catch::Approx(0.892).margin(0.014));

    // RIDER: top reaches EXACTLY the measured helmet CROWN above the model
    // ground line; bottom touches the measured seat crown; the head end sits
    // at the measured helmet z-extent midpoint.
    //
    // â 2026-08-29: BOTH of this block's expectations were CORRECTED, and both
    // old values were composed rather than measured. The height was 1.25 (the
    // cowl top plus a head-group allowance, labelled MEASURED) against a real
    // crown of 1.574660 -- the head was 0.32 m short. The head z was -0.10,
    // which was the row-6 breath anchor's offset OUT FROM THE NECK, a RELATIVE
    // number used as an ABSOLUTE coordinate; it put the head outside the
    // helmet's own measured z extent (-0.440089..-0.034555) and over-leaned
    // the rider by 0.34 m. Both now come from
    // assets/character/sudburian_src/measure_helmet_crown.py (R4a's, re-run in
    // this tree against the byte-identical GLB).
    //
    // The pelvis anchor is what proves the frames are the same one:
    // kPelvisZ -0.5848 equals that table's measured pelvis z EXACTLY. Note the
    // emitted body z is the NEGATED constant (the constants are model frame,
    // +z forward; body is -z forward), so the head lands at +0.237322 and sits
    // 0.347 m FORWARD of the pelvis -- a rider leaning to the bars.
    const auto& r = out[kRider];
    const double top = std::max(r.a.y, r.b.y) + r.radius;
    CHECK(top == Catch::Approx(1.574660 - drop));
    CHECK(std::min(r.a.y, r.b.y) - r.radius ==
          Catch::Approx(render::sled_glb::kSeatTopY - drop));
    CHECK((r.a.z < r.b.z ? r.a.z : r.b.z) == Catch::Approx(0.237322));
    // The head must lie INSIDE the measured helmet's z extent -- the leg that
    // would have caught the relative-used-as-absolute error.
    const double head_model = -(r.a.z < r.b.z ? r.a.z : r.b.z);
    CHECK(head_model >= -0.440089);
    CHECK(head_model <= -0.034555);
}

TEST_CASE("R5 row 9: the sled shadow FOLLOWS -- steer yaws the skis about "
          "their kingpins, susp moves each corner, the rider lean carries "
          "the rider capsule") {
    const render::SledShadowDims d0 = sled_dims();
    const auto base = sled_at_identity(d0);

    SECTION("steer: skis yaw about the kingpin, chassis stays put") {
        render::SledShadowDims d = d0;
        d.steer_rad = sim::SledParams{}.steer_max_rad;  // full LEFT
        const auto out = sled_at_identity(d);
        for (int i : {kSkiL, kSkiR}) {
            // The ski FRONT endpoint (smaller body z = further forward)
            // swings toward the rider's LEFT = body -X... model +X maps to
            // body -X, so +steer (left) moves the forward tip to body -X.
            const auto& s = out[i];
            const auto& s0 = base[i];
            const glm::dvec3 tip = s.a.z < s.b.z ? s.a : s.b;
            const glm::dvec3 tip0 = s0.a.z < s0.b.z ? s0.a : s0.b;
            CHECK(tip.x < tip0.x - 0.05);  // fwd tip swings left (body -X)
            // Rigid about the kingpin: the endpoint's distance to its own
            // kingpin is preserved.
            const double kx = (i == kSkiL ? -1.0 : 1.0) * d.half_stance;
            const glm::dvec3 kp{kx, tip0.y, -render::sled_glb::kSkiPivotZ};
            CHECK(glm::length(tip - kp) ==
                  Catch::Approx(glm::length(tip0 - kp)));
        }
        // Belt, tunnel, hood, rider: bit-identical (steering moves skis only).
        for (int i : {kBelt, kTunnel, kHood, kRider}) {
            CHECK(glm::length(out[i].a - base[i].a) == 0.0);
            CHECK(glm::length(out[i].b - base[i].b) == 0.0);
        }
    }

    SECTION("susp: each corner rides its own channel, sprung mass does not") {
        render::SledShadowDims d = d0;
        d.susp_m[0] = 0.08;  // kernel L (model +X -> body -X ski)
        const auto out = sled_at_identity(d);
        // Only the kernel-L ski moved, straight up by the compression.
        CHECK(out[kSkiL].a.y - base[kSkiL].a.y == Catch::Approx(0.08));
        CHECK(glm::length(out[kSkiR].a - base[kSkiR].a) == 0.0);
        CHECK(glm::length(out[kBelt].a - base[kBelt].a) == 0.0);
        CHECK(glm::length(out[kTunnel].a - base[kTunnel].a) == 0.0);
        CHECK(glm::length(out[kHood].a - base[kHood].a) == 0.0);
        // The track skid rides channel 2.
        render::SledShadowDims dt = d0;
        dt.susp_m[2] = 0.05;
        const auto out2 = sled_at_identity(dt);
        CHECK(out2[kBelt].a.y - base[kBelt].a.y == Catch::Approx(0.05));
        CHECK(glm::length(out2[kSkiL].a - base[kSkiL].a) == 0.0);
    }

    SECTION("rider lean: the rider capsule translates by the kernel slew, "
            "the machine does not") {
        render::SledShadowDims d = d0;
        d.rider_lat_m = 0.20;  // + LEFT (sim/sled.h:1096) -> body -X
        d.rider_fwd_m = 0.10;  // + forward -> body -Z
        d.rider_up_m = 0.15;
        const auto out = sled_at_identity(d);
        const glm::dvec3 delta = out[kRider].a - base[kRider].a;
        CHECK(delta.x == Catch::Approx(-0.20));  // left = body -X
        CHECK(delta.y == Catch::Approx(0.15));
        CHECK(delta.z == Catch::Approx(-0.10));  // fwd = body -Z
        CHECK(glm::length(out[kRider].b - base[kRider].b -
                          delta) == Catch::Approx(0.0).margin(1e-12));
        for (int i : {kSkiL, kSkiR, kBelt, kTunnel, kHood}) {
            CHECK(glm::length(out[i].a - base[i].a) == 0.0);
        }
    }

    SECTION("sag0 single source: the app reads render::sled_sag0_m — the "
            "same accessor sled_model.cpp mounts the mesh with") {
        bool ok = false;
        std::ifstream in(kRoot + "/app/main.cpp", std::ios::binary);
        REQUIRE(in.good());
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string msrc = ss.str();
        REQUIRE(msrc.find("sd.sag0 = render::sled_sag0_m();") !=
                std::string::npos);
        // CAM-SMOOTH (2026-09-08): the app reads the DRAW-side blend of the
        // kernel state (sled_draw, render/interp.h) -- same steer, sub-tick.
        REQUIRE(msrc.find(
                    "sled_draw.steer_actual * render::ski_steer_max_rad()") !=
                std::string::npos);
        std::ifstream in2(kRoot + "/render/sled_model.cpp", std::ios::binary);
        REQUIRE(in2.good());
        std::ostringstream ss2;
        ss2 << in2.rdbuf();
        REQUIRE(ss2.str().find("sag0 = sled_sag0_m();") != std::string::npos);
        (void)ok;
    }
}

// --------------------------------------------------------------------------
// Fence 5 — the altitude gate: an underground caster casts nothing.
// --------------------------------------------------------------------------
TEST_CASE("R5 row 9: the altitude gate drops casters below the drive surface") {
    const double R = 15000.0;
    const double surf = R + 320.0;  // some terrain elevation
    const glm::dvec3 up{0.0, 0.0, 1.0};
    // On / above the surface: casts.
    CHECK(render::shadow_caster_above_surface(up * (surf + 1.0), surf, 2.0));
    CHECK(render::shadow_caster_above_surface(up * surf, surf, 2.0));
    // Within the suspension-sink margin: still casts (no contact flicker).
    CHECK(render::shadow_caster_above_surface(up * (surf - 1.5), surf, 2.0));
    // In the tunnel network, tens of metres down: casts NOTHING.
    CHECK_FALSE(
        render::shadow_caster_above_surface(up * (surf - 30.0), surf, 2.0));

    // And the gate + the kill dial are actually wired at the app seam.
    bool ok = false;
    const std::string msrc = read_file(kRoot + "/app/main.cpp", &ok);
    REQUIRE(ok);
    REQUIRE(msrc.find("SEADS_SHADOWS") != std::string::npos);
    // The kill is AT THE SOURCE: dial 0 (or [shadows] enabled=false) leaves
    // the caster list empty, which IS the shader's early-out.
    REQUIRE(msrc.find("world.shadows.enabled && shadow_dial > 0.0") !=
            std::string::npos);
    REQUIRE(msrc.find("shadow_caster_above_surface") != std::string::npos);

    // FrameInfo defaults are the OFF values (unset callers — tests, probe,
    // harness — draw the shipped frame). Source-pinned: draw.h pulls raylib,
    // so this test must not include it.
    bool hok = false;
    const std::string hdr = read_file(kRoot + "/render/draw.h", &hok);
    REQUIRE(hok);
    REQUIRE(hdr.find("std::vector<render::ShadowCaster> shadow_casters{};") !=
            std::string::npos);
    REQUIRE(hdr.find("float shadow_strength = 0.0f;") != std::string::npos);
}
