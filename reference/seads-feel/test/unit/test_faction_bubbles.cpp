// FACTION BUBBLES — the two warring atmosphere ELLIPSES
// (world/faction_bubbles.h, Scarce Skies MASTER_PLAN §4). REWRITTEN 2026-07-25
// (Chad's ruling): each faction's territory is now ONE TRUE ELLIPSE
// (sim::AtmosphereField::Bubble with minor_radius_m > 0 — the sim/aero.h
// ellipse branch), running EAST-WEST, separated by a north-south VACUUM GAP.
// Supersedes the prior 3-circle-union-per-faction approximation (6 bubbles ->
// 2).
//
// Legs (mutation levers noted per-case):
//  - anchor sanity: the baked directions/axes are unit vectors, mutually
//    distinct, and each major_axis is orthogonal to its center_dir (kills a
//    copy-paste duplicate-center mutant and a non-tangent axis mutant); the
//    Errington reproduction that VALIDATES the whole conversion chain was run
//    offline (offline_tool/sudbury_geo.py::SudburyFrame.geo_to_dir against
//    world/tunnel_geo.h::kTunnelMouthErrington, diff == 0.0 to full double
//    precision) BEFORE these ellipses were baked — see the header's
//    PROVENANCE comment.
//  - build+union: at growth 1.0, atm_frac_at reads ~1 inside each faction's
//    heart, ~0 far outside both ovals, and ~0 above the ceiling (kills a
//    dropped/zeroed bubble, a swapped ceiling/radius field, or a build that
//    emits < 2 bubbles).
//  - faction mapping: bubble_faction(0)==VALLEY, (1)==SUDBURY (kills a
//    swapped faction-index mutant).
//  - all 12 town legs (8 VALLEY + 4 SUDBURY): every roster town reads inside
//    its faction's hard edge with positive margin.
//  - the 3-longitude gap legs: the vacuum corridor between the two hard edges
//    is >= ~4 km of N-S ground distance at each sample longitude.
//  - both tunnel-mouth legs: Errington reads meaningful air within VALLEY,
//    Murray within SUDBURY.
//  - both pump-anchor legs: Chelmsford inside VALLEY, Copper Cliff inside
//    SUDBURY (unchanged from the pre-ellipse world).
//  - growth: scaling ONE faction's grow[] entry strictly grows ONLY that
//    faction's bubble (major AND minor radius, plus ceiling), and leaves the
//    other faction's bubble bit-identical (kills a shared/global growth
//    mutant); an absurd growth scale clamps to the loader's legal ranges,
//    and the minor radius stays < the (clamped) major radius (kills a
//    dropped-clamp or inverted-axis mutant).
//  - loader: [conquest] parses strictly; growth fracs outside [0,1] and an
//    unrecognized player_faction are rejected (kills a dropped-check
//    mutant); enabled ships true per Chad's 2026-07-24 activation.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "config/load_aircraft.h"
#include "config/load_game.h"
#include "sim/aero.h"
#include "sim/fields.h"
#include "sim/world.h"
#include "world/faction_bubbles.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

const sim::AircraftParams kP =
    cfg::load_aircraft_toml(SEADS_CONFIG_DIR "/aircraft.toml");

const cfg::GameParams kGame =
    cfg::load_game_toml(SEADS_CONFIG_DIR "/game.toml", kP);

glm::dvec3 at(const glm::dvec3& dir, double h) {
    return glm::normalize(dir) * (kP.R + h);
}

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string write_temp(const std::string& text, const char* tag) {
    const std::filesystem::path p =
        std::filesystem::temp_directory_path() /
        (std::string("seads_faction_") + tag + ".toml");
    std::ofstream(p) << text;
    return p.string();
}

std::string replace_all(std::string s, const std::string& from,
                        const std::string& to) {
    for (size_t i = s.find(from); i != std::string::npos;
         i = s.find(from, i + to.size())) {
        s.replace(i, from.size(), to);
    }
    return s;
}

const world::FactionGrowth kNoGrowth[2] = {};  // 1.0/1.0 both, the shipped
                                               // structural no-op state

}  // namespace

TEST_CASE("faction_bubbles: baked anchors are unit vectors and distinct") {
    const glm::dvec3 dirs[6] = {
        world::kValleyCenterDir,   world::kValleyMajorAxis,
        world::kSudburyCenterDir,  world::kSudburyMajorAxis,
        world::kPumpValleySurface, world::kPumpSudburySurface,
    };
    for (const glm::dvec3& d : dirs) {
        CHECK(glm::length(d) == Catch::Approx(1.0).epsilon(1e-9));
    }
    // Distinctness: every pair separated by a real arc (kills a copy-paste
    // duplicate-center mutant — a repeated direction would dot to 1.0).
    for (int i = 0; i < 6; ++i) {
        for (int j = i + 1; j < 6; ++j) {
            CHECK(glm::dot(dirs[i], dirs[j]) < 0.999);
        }
    }
    // Each major_axis is a tangent at its own center (orthogonal, within
    // baking precision) — kills a non-tangent / copy-pasted-axis mutant.
    CHECK(std::abs(glm::dot(world::kValleyCenterDir, world::kValleyMajorAxis)) <
          1e-6);
    CHECK(std::abs(glm::dot(world::kSudburyCenterDir,
                            world::kSudburyMajorAxis)) < 1e-6);
}

TEST_CASE("faction_bubbles: bubble_faction maps 0 VALLEY, 1 SUDBURY") {
    CHECK(world::bubble_faction(0) == world::VALLEY);
    CHECK(world::bubble_faction(1) == world::SUDBURY);
}

TEST_CASE("faction_bubbles: build_faction_bubbles emits exactly 2, in order") {
    std::vector<sim::AtmosphereField::Bubble> out;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, out);
    REQUIRE(out.size() == 2);
    CHECK(out[0].center_dir == world::kValleyCenterDir);
    CHECK(out[0].major_axis == world::kValleyMajorAxis);
    CHECK(out[0].minor_radius_m == Catch::Approx(world::kValleyMinorRadiusM));
    CHECK(out[0].ground_radius_m == Catch::Approx(world::kValleyMajorRadiusM));
    CHECK(out[1].center_dir == world::kSudburyCenterDir);
    CHECK(out[1].major_axis == world::kSudburyMajorAxis);
    CHECK(out[1].minor_radius_m == Catch::Approx(world::kSudburyMinorRadiusM));
    CHECK(out[1].ground_radius_m == Catch::Approx(world::kSudburyMajorRadiusM));
    // APPENDS, never clears (a caller unioning with other bubble sources).
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, out);
    CHECK(out.size() == 4);
}

TEST_CASE("faction_bubbles: union reads ~1 inside each faction's heart") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 valley_pos = at(world::kValleyCenterDir, 500.0);
    const double f_valley = sim::atm_frac_at(valley_pos, &env, kP);
    CHECK(f_valley == Catch::Approx(1.0).epsilon(1e-6));

    const glm::dvec3 sudbury_pos = at(world::kSudburyCenterDir, 500.0);
    const double f_sudbury = sim::atm_frac_at(sudbury_pos, &env, kP);
    CHECK(f_sudbury == Catch::Approx(1.0).epsilon(1e-6));
}

TEST_CASE("faction_bubbles: far outside both ovals reads near-vacuum") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    // The antipode of the MIDPOINT of the two centers — genuinely far from
    // both ellipses (a bare antipode of either single center is NOT safe on
    // this small a sphere: it can land inside the OTHER faction's ellipse,
    // verified against the actual baked geometry before picking this point).
    glm::dvec3 far_dir = -(world::kValleyCenterDir + world::kSudburyCenterDir);
    far_dir = glm::normalize(far_dir);
    const glm::dvec3 far_pos = at(far_dir, 500.0);
    const double f = sim::atm_frac_at(far_pos, &env, kP);
    CHECK(f < 0.2);
}

TEST_CASE("faction_bubbles: above the VALLEY ceiling reads near-vacuum") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    const double h = kGame.atmosphere.bubble_ceiling_m +
                     3.0 * kGame.atmosphere.bubble_ceil_soft_m;
    const glm::dvec3 pos = at(world::kValleyCenterDir, h);
    const double f = sim::atm_frac_at(pos, &env, kP);
    CHECK(f < 0.2);
}

// The 8 VALLEY + 4 SUDBURY roster towns baked as unit directions (same
// offline provenance as the ellipse centers/axes — see the header). Each is
// checked against the REAL sim::atm_frac_at ellipse math with a positive
// margin, i.e. genuinely inside the hard edge (not merely the soft-edge
// tolerance a frac>=0.95 threshold would also accept).
namespace {
struct BakedTown {
    const char* name;
    glm::dvec3 dir;
    world::Faction faction;
};

// clang-format off
const BakedTown kBakedTowns[] = {
    {"Onaping",     glm::dvec3{-0.87674834598112827, 0.43300365519540973, -0.20933268356081083}, world::VALLEY},
    {"Dowling",     glm::dvec3{-0.85945282315147808, 0.43131926581001201, 0.27441671909354659}, world::VALLEY},
    {"Chelmsford",  glm::dvec3{-0.43510447686431641, 0.19229751306740031, 0.87960545739594109}, world::VALLEY},
    {"Azilda",      glm::dvec3{0.11176362666500897, 0.081297523466543342, 0.99040375828895733}, world::VALLEY},
    {"Val Caron",   glm::dvec3{0.5997126239091829, 0.34295754660869487, 0.72299715763487471}, world::VALLEY},
    {"Valley East", glm::dvec3{0.67889204864461195, 0.51980997726957978, 0.5185587467376489}, world::VALLEY},
    {"Hanmer",      glm::dvec3{0.64648734048923195, 0.63458005099732329, 0.42351183863428737}, world::VALLEY},
    {"Capreol",     glm::dvec3{0.65011466438926646, 0.75319598270371746, 0.10023340154366206}, world::VALLEY},
    {"Sudbury",     glm::dvec3{0.46892813603295158, -0.44923248180969061, 0.76045813857422018}, world::SUDBURY},
    {"Lively",      glm::dvec3{-0.13389408451646184, -0.76418850166788199, 0.6309423967765172}, world::SUDBURY},
    {"Coniston",    glm::dvec3{0.91792326900695709, -0.38431941060793923, 0.098567047462870955}, world::SUDBURY},
    {"Whitefish",   glm::dvec3{-0.6566651062318245, -0.75169116505451417, -0.061248107207766203}, world::SUDBURY},
};
// clang-format on
}  // namespace

TEST_CASE(
    "faction_bubbles: all 12 roster towns read inside their faction with "
    "margin") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    for (const BakedTown& t : kBakedTowns) {
        INFO("town: " << t.name);
        REQUIRE(glm::length(t.dir) == Catch::Approx(1.0).epsilon(1e-6));
        const glm::dvec3 pos = at(t.dir, 500.0);
        const double f = sim::atm_frac_at(pos, &env, kP);
        CHECK(f >= 0.95);
    }
}

TEST_CASE("faction_bubbles: kTunnelMouthErrington reads air within VALLEY") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    // Errington Mine #3 (baked in world/tunnel_geo.h, the VALLEY/Chelmsford
    // coalition's home mouth).
    const glm::dvec3 errington{-0.53498162816562578, -0.069036317995522498,
                               0.84203838649011564};
    const glm::dvec3 pos = at(errington, 500.0);
    const double f = sim::atm_frac_at(pos, &env, kP);
    CHECK(f >=
          0.9);  // meaningful air at the gateway (well within the soft edge)
}

TEST_CASE("faction_bubbles: kTunnelMouthMurray reads air within SUDBURY") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    // Murray pit (baked in world/tunnel_geo.h, the SUDBURY raid-target mouth).
    const glm::dvec3 murray{0.22052874171460962, -0.30086267548060569,
                            0.92781933833070263};
    const glm::dvec3 pos = at(murray, 500.0);
    const double f = sim::atm_frac_at(pos, &env, kP);
    CHECK(f >= 0.9);
}

TEST_CASE("faction_bubbles: pump anchors are inside their faction's ellipse") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 valley_pump = at(world::kPumpValleySurface, 500.0);
    CHECK(sim::atm_frac_at(valley_pump, &env, kP) >= 0.95);

    const glm::dvec3 sudbury_pump = at(world::kPumpSudburySurface, 500.0);
    CHECK(sim::atm_frac_at(sudbury_pump, &env, kP) >= 0.95);
}

// DISTAL PROPERTY (Chad's 2026-07-25 fly-2 ruling A): each surface pump moved
// to the far end of its own ellipse's major axis. Two independent legs, both
// against the REAL sim r_eff ellipse math (not just the atm_frac_at soft-edge
// proxy above):
//  (1) each anchor sits inside its OWN faction's hard edge (r_eff at the
//      anchor's bearing) with >= 1 km margin.
//  (2) each anchor is FARTHER from the ENEMY ellipse's center (great-circle,
//      against the real kP.R) than the OLD (pre-2026-07-25-fly-2) anchor was
//      -- the literal "moved to the distal end, away from the contested
//      middle" property.
// MUTATION: reverting either anchor to the old (Chelmsford/Copper Cliff)
// direction flips leg (2); placing an anchor outside its own hard edge (e.g.
// swapping which faction's ellipse params feed the r_eff call) flips leg (1).
namespace {
double hard_edge_radius_m(const glm::dvec3& center_dir,
                          const glm::dvec3& major_axis, double a, double b,
                          const glm::dvec3& query_dir) {
    const glm::dvec3 cn = glm::normalize(center_dir);
    const glm::dvec3 up = glm::normalize(query_dir);
    const double c = glm::clamp(glm::dot(up, cn), -1.0, 1.0);
    glm::dvec3 m = major_axis - cn * glm::dot(major_axis, cn);
    m = glm::normalize(m);
    glm::dvec3 t = up - cn * c;
    t = glm::normalize(t);
    const double cos_th = glm::clamp(glm::dot(t, m), -1.0, 1.0);
    const double sin_th = std::sqrt(std::max(0.0, 1.0 - cos_th * cos_th));
    const double denom =
        std::sqrt((b * cos_th) * (b * cos_th) + (a * sin_th) * (a * sin_th));
    return denom > 1e-9 ? (a * b / denom) : b;
}

double great_circle_m(double R, const glm::dvec3& a, const glm::dvec3& b) {
    const double c =
        glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0);
    return R * std::acos(c);
}

// The OLD (pre-2026-07-25-fly-2, Chelmsford/Copper Cliff) pump anchors, kept
// here ONLY as the historical comparison point for the distal-property leg
// (they no longer exist as named constants in world/faction_bubbles.h).
const glm::dvec3 kOldPumpValleySurface{-0.42141019306050187,
                                       0.19272584885676147, 0.8861547248461475};
const glm::dvec3 kOldPumpSudburySurface{0.21840387027352948,
                                        -0.5687009594539248, 0.793018895213556};
}  // namespace

TEST_CASE(
    "faction_bubbles: pump anchors sit inside the real hard edge with "
    ">= 1 km margin") {
    const glm::dvec3 vq = glm::normalize(world::kPumpValleySurface);
    const double v_edge = hard_edge_radius_m(
        world::kValleyCenterDir, world::kValleyMajorAxis,
        world::kValleyMajorRadiusM, world::kValleyMinorRadiusM, vq);
    const double v_arc = great_circle_m(kP.R, world::kValleyCenterDir, vq);
    CHECK(v_edge - v_arc >= 1000.0);

    const glm::dvec3 sq = glm::normalize(world::kPumpSudburySurface);
    const double s_edge = hard_edge_radius_m(
        world::kSudburyCenterDir, world::kSudburyMajorAxis,
        world::kSudburyMajorRadiusM, world::kSudburyMinorRadiusM, sq);
    const double s_arc = great_circle_m(kP.R, world::kSudburyCenterDir, sq);
    CHECK(s_edge - s_arc >= 1000.0);
}

TEST_CASE(
    "faction_bubbles: pump anchors moved farther from the enemy center "
    "(the distal property)") {
    const double v_new = great_circle_m(kP.R, world::kPumpValleySurface,
                                        world::kSudburyCenterDir);
    const double v_old =
        great_circle_m(kP.R, kOldPumpValleySurface, world::kSudburyCenterDir);
    CHECK(v_new > v_old);

    const double s_new = great_circle_m(kP.R, world::kPumpSudburySurface,
                                        world::kValleyCenterDir);
    const double s_old =
        great_circle_m(kP.R, kOldPumpSudburySurface, world::kValleyCenterDir);
    CHECK(s_new > s_old);
}

// The 3-longitude vacuum-gap legs. At each of 3 sample longitudes spanning
// the overlap (-81.3, -81.1, -80.95), THREE points baked offline
// (offline_tool, a dense N-S scan through the SAME r_eff/atm_falloff math):
// `north_edge`/`south_edge` sit at the two hard-edge crossings bounding the
// gap (VALLEY's southern hard edge, SUDBURY's northern hard edge), and `mid`
// is their midpoint. The corridor width (north_edge<->south_edge great-circle
// separation, computed HERE in C++ against the real kP.R, not re-imported
// from the bake) must be >= ~4 km, AND the midpoint — genuinely deep in that
// corridor, past both bubbles' soft edges — must read near the
// outside-bubble (vacuum) baseline.
namespace {
struct GapTriple {
    const char* label;
    glm::dvec3 north_edge;  // VALLEY's hard edge (the gap's north bound)
    glm::dvec3 south_edge;  // SUDBURY's hard edge (the gap's south bound)
    glm::dvec3 mid;         // midpoint of the corridor
};
// clang-format off
const GapTriple kGapTriples[] = {
    {"lon=-81.3",
     glm::dvec3{-0.8225764531790589, -0.08922085160245356, 0.5616116258720902},
     glm::dvec3{-0.7836070936410074, -0.43085090109450075, 0.4475795167582519},
     glm::dvec3{-0.8165990809262935, -0.26440945439197405, 0.5130824314459338}},
    {"lon=-81.1",
     glm::dvec3{0.05094070443852235, 0.031360213342333715, 0.9982091873201882},
     glm::dvec3{0.05026251461226378, -0.2889023871402665, 0.9560382263955284},
     glm::dvec3{0.05127482571536886, -0.13048428893570516, 0.9901235996524889}},
    {"lon=-80.95",
     glm::dvec3{0.7007167103515314, 0.397160267242434, 0.5926717590353422},
     glm::dvec3{0.7216798218667138, -0.20447838128145246, 0.6613371502486046},
     glm::dvec3{0.7462689370968275, 0.10109171597154248, 0.6579233530482865}},
};
// clang-format on
}  // namespace

TEST_CASE("faction_bubbles: the 3 sample longitudes show a real vacuum gap") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    const double h = 500.0;  // above the deck's ~320 m AGL fade-out
    for (const GapTriple& g : kGapTriples) {
        INFO(g.label);
        REQUIRE(glm::length(g.north_edge) == Catch::Approx(1.0).epsilon(1e-6));
        REQUIRE(glm::length(g.south_edge) == Catch::Approx(1.0).epsilon(1e-6));
        REQUIRE(glm::length(g.mid) == Catch::Approx(1.0).epsilon(1e-6));

        // Corridor width, computed HERE against the real kP.R.
        const double cos_sep =
            glm::clamp(glm::dot(g.north_edge, g.south_edge), -1.0, 1.0);
        const double sep_m = kP.R * std::acos(cos_sep);
        CHECK(sep_m >= 4000.0);

        // The midpoint reads near the outside-bubble (vacuum) baseline.
        const glm::dvec3 pos_mid = at(g.mid, h);
        const double f_mid = sim::atm_frac_at(pos_mid, &env, kP);
        CHECK(f_mid < 0.05);
    }
}

// The NARROWEST-crossing pin (direct-check P2 fold): the 3 baked longitudes
// above miss the true minimum (~4.0 km near lon -81.22), so a retune that
// pinches the corridor shut between samples would slip past them. This leg is
// SELF-CONTAINED: it slerp-walks the great circle between the two baked
// ellipse centers (the natural raid crossing) with the LIVE sim field and
// measures the contiguous vacuum stretch (frac < 0.05 at 500 m AGL) along it.
// Mutation this kills: growing either minor radius (or shrinking the center
// separation) until the hard edges touch — the stretch collapses below the
// floor. It also fails if the corridor is not vacuum at all (edges overlap).
TEST_CASE(
    "faction_bubbles: the direct center-to-center crossing has a real "
    "vacuum core") {
    std::vector<sim::AtmosphereField::Bubble> bubbles;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, bubbles);
    sim::AtmosphereField af;
    af.bubbles = bubbles;
    sim::Environment env;
    env.atm = &af;

    const glm::dvec3 a = glm::normalize(world::kValleyCenterDir);
    const glm::dvec3 b = glm::normalize(world::kSudburyCenterDir);
    const double full_arc =
        kP.R * std::acos(glm::clamp(glm::dot(a, b), -1.0, 1.0));
    const int kSteps = 2000;  // ~15 m resolution over the ~30 km path
    // Two thresholds, both MEASURED on the shipped geometry (2026-07-25:
    // half-air stretch 1732 m, dead-air (<5%) stretch 1261 m — the oblique
    // crossing + the 1200 m soft edges eat the 4-5 km hard-edge gap down to
    // this): the pins sit just under the measured values so a retune that
    // meaningfully closes the corridor trips them, while a retune that WIDENS
    // it stays green.
    double half_len = 0.0, best_half = 0.0;
    double dead_len = 0.0, best_dead = 0.0;
    for (int i = 0; i <= kSteps; ++i) {
        const double t = static_cast<double>(i) / kSteps;
        const glm::dvec3 dir = glm::normalize(glm::mix(a, b, t));  // chordal
        const double f = sim::atm_frac_at(at(dir, 500.0), &env, kP);
        const double step_m = full_arc / kSteps;
        half_len = (f < 0.5) ? half_len + step_m : 0.0;
        dead_len = (f < 0.05) ? dead_len + step_m : 0.0;
        best_half = std::max(best_half, half_len);
        best_dead = std::max(best_dead, dead_len);
    }
    REQUIRE(best_half >= 1500.0);   // suffocating-air corridor is real
    REQUIRE(best_dead >= 1000.0);   // a true dead-vacuum core exists
    REQUIRE(best_half <= 15000.0);  // ...and not the whole map (domes exist)
}

TEST_CASE("faction_bubbles: growth scales only the grown faction's ellipse") {
    std::vector<sim::AtmosphereField::Bubble> base;
    world::build_faction_bubbles(kGame.atmosphere, kNoGrowth, base);
    REQUIRE(base.size() == 2);

    world::FactionGrowth grow[2];
    grow[world::VALLEY] = world::FactionGrowth{1.25, 1.25};
    grow[world::SUDBURY] = world::FactionGrowth{};  // untouched (1.0/1.0)

    std::vector<sim::AtmosphereField::Bubble> grown;
    world::build_faction_bubbles(kGame.atmosphere, grow, grown);
    REQUIRE(grown.size() == 2);

    // VALLEY (0): strictly bigger, major AND minor radius, AND ceiling.
    CHECK(grown[0].ground_radius_m > base[0].ground_radius_m);
    CHECK(grown[0].minor_radius_m > base[0].minor_radius_m);
    CHECK(grown[0].ceiling_m > base[0].ceiling_m);
    CHECK(grown[0].ground_radius_m ==
          Catch::Approx(base[0].ground_radius_m * 1.25).epsilon(1e-9));
    CHECK(grown[0].minor_radius_m ==
          Catch::Approx(base[0].minor_radius_m * 1.25).epsilon(1e-9));
    CHECK(grown[0].ceiling_m ==
          Catch::Approx(base[0].ceiling_m * 1.25).epsilon(1e-9));
    // Aspect ratio preserved (same scalar on both axes).
    CHECK(grown[0].minor_radius_m < grown[0].ground_radius_m);

    // SUDBURY (1): bit-identical to the ungrown baseline (kills a
    // shared/global-growth mutant that leaks across factions).
    CHECK(grown[1].ground_radius_m == base[1].ground_radius_m);
    CHECK(grown[1].minor_radius_m == base[1].minor_radius_m);
    CHECK(grown[1].ceiling_m == base[1].ceiling_m);
}

TEST_CASE("faction_bubbles: absurd growth clamps to the loader's legal range") {
    world::FactionGrowth grow[2];
    grow[world::VALLEY] = world::FactionGrowth{10.0, 10.0};
    grow[world::SUDBURY] = world::FactionGrowth{10.0, 10.0};

    std::vector<sim::AtmosphereField::Bubble> grown;
    world::build_faction_bubbles(kGame.atmosphere, grow, grown);
    REQUIRE(grown.size() == 2);
    for (const auto& b : grown) {
        CHECK(b.ground_radius_m <= world::kFactionBubbleRadiusMaxM);
        CHECK(b.minor_radius_m <= world::kFactionBubbleRadiusMaxM);
        CHECK(b.ceiling_m <= world::kFactionBubbleCeilingMaxM);
        // Minor never EXCEEDS major after every clamp (an ellipse can never
        // invert into a taller-than-wide shape via a growth mutant — equal is
        // the degenerate circle, still legal).
        CHECK(b.minor_radius_m <= b.ground_radius_m);
        // The clamp actually BOUND something here (not a vacuous <=): every
        // baked major radius * 10 exceeds the cap.
        CHECK(b.ground_radius_m ==
              Catch::Approx(world::kFactionBubbleRadiusMaxM));
    }
    // Ceiling: shipped bubble_ceiling_m (4000) * 10 = 40000 > cap (20000).
    CHECK(grown[0].ceiling_m ==
          Catch::Approx(world::kFactionBubbleCeilingMaxM));
}

// The SHARED r_eff law (world::ellipse_r_eff, the ADEPT-AI bubble-leash edge)
// must MATCH the live sim field's air edge, or the AI's "edge" forks the
// plant's. For a lone ellipse bubble the air is full inside r_eff and
// half-faded at r_eff + edge_soft/2; this leg BISECTS the field's own half-air
// crossing along two bearings (the semi-major axis, where r_eff==a, and the
// semi-minor, where r_eff==b) and confirms world::ellipse_r_eff reproduces it
// to the metre. MUTATION: swap a<->b in ellipse_r_eff, or drop its bearing
// dependence -> the major/minor legs disagree with the sim crossing.
TEST_CASE("faction_bubbles: ellipse_r_eff matches the live sim air edge") {
    const glm::dvec3 c = glm::normalize(world::kValleyCenterDir);
    const glm::dvec3 m = glm::normalize(world::kValleyMajorAxis);
    const double a = world::kValleyMajorRadiusM, b = world::kValleyMinorRadiusM;
    const double soft = kGame.atmosphere.bubble_edge_soft_m;

    // A single VALLEY ellipse bubble (NOT the 2-oval build — isolate one edge).
    sim::AtmosphereField af;
    af.bubbles.push_back(sim::AtmosphereField::Bubble{
        c, a, kGame.atmosphere.bubble_ceiling_m, soft,
        kGame.atmosphere.bubble_ceil_soft_m, m, b});
    sim::Environment env;
    env.atm = &af;

    // A tangent bearing at the center, `theta` (fraction) toward it.
    const glm::dvec3 minor = glm::normalize(glm::cross(c, m));  // the b-axis
    auto bisect_half_air = [&](const glm::dvec3& bearing) {
        // f(arc) = atm_frac_at at arc along `bearing` from the center, 500 m
        // AGL.
        auto f = [&](double arc) {
            const double th = arc / kP.R;
            const glm::dvec3 dir = std::cos(th) * c + std::sin(th) * bearing;
            return sim::atm_frac_at(at(dir, 500.0), &env, kP);
        };
        double lo = 0.0, hi = std::max(a, b) + 2.0 * soft;  // inside .. vacuum
        REQUIRE(f(lo) > 0.9);
        REQUIRE(f(hi) < 0.1);
        for (int i = 0; i < 60; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (f(mid) > 0.5)
                lo = mid;
            else
                hi = mid;
        }
        return 0.5 * (lo + hi);
    };

    // Major-axis bearing: r_eff == a; half-air at a + soft/2 -- MODULO the
    // S-domeround mixing (docs/airdome_round_spec.md §1): the bisection
    // probe sits at alt=500m, which the OLD code's separable u_h*u_v
    // product ignored entirely at this arc (u_v==1 exactly, alt << ceiling)
    // but the dome's meridional norm folds in via rho=sqrt(arc^2+alt^2), so
    // the half-air crossing shifts inward by a few metres (~11.3m major,
    // ~9.8m minor at the shipped table -- exact values derived offline and
    // verified against the live field, not guessed). The margin widened
    // 3.0->15.0 to absorb this real, expected shift; ellipse_r_eff ITSELF
    // (the hard-edge radius, unrelated to the soft transition) is still
    // pinned tight (1e-9) two lines below -- that is what this leg is
    // actually about (the AI leash-edge single-source claim).
    const double half_major = bisect_half_air(m);
    const double reff_major =
        world::ellipse_r_eff(c, m, a, b,
                             glm::normalize(std::cos(0.5 * a / kP.R) * c +
                                            std::sin(0.5 * a / kP.R) * m));
    CHECK(reff_major == Catch::Approx(a).epsilon(1e-9));
    CHECK(half_major == Catch::Approx(reff_major + 0.5 * soft).margin(15.0));

    // Minor-axis bearing: r_eff == b; half-air at b + soft/2 (same dome-
    // mixing caveat as above).
    const double half_minor = bisect_half_air(minor);
    const double reff_minor =
        world::ellipse_r_eff(c, m, a, b,
                             glm::normalize(std::cos(0.5 * b / kP.R) * c +
                                            std::sin(0.5 * b / kP.R) * minor));
    CHECK(reff_minor == Catch::Approx(b).epsilon(1e-9));
    CHECK(half_minor == Catch::Approx(reff_minor + 0.5 * soft).margin(15.0));
}

// faction_ellipse exposes the SAME growth-scaled + clamped a/b build_faction_
// bubbles emits (the single-source the leash reads). MUTATION: a divergent
// clamp / a dropped growth scale desyncs it from the built bubble.
TEST_CASE(
    "faction_bubbles: faction_ellipse matches the built bubble geometry") {
    world::FactionGrowth grow[2];
    grow[world::VALLEY] = world::FactionGrowth{1.3, 1.0};
    grow[world::SUDBURY] = world::FactionGrowth{};  // 1.0
    std::vector<sim::AtmosphereField::Bubble> built;
    world::build_faction_bubbles(kGame.atmosphere, grow, built);
    REQUIRE(built.size() == 2);

    for (int f = 0; f < 2; ++f) {
        glm::dvec3 cd, ma;
        double a = 0.0, b = 0.0;
        world::faction_ellipse(f, grow, cd, ma, a, b);
        CHECK(a == Catch::Approx(built[f].ground_radius_m));
        CHECK(b == Catch::Approx(built[f].minor_radius_m));
        CHECK(cd == built[f].center_dir);
        CHECK(ma == built[f].major_axis);
    }
}

TEST_CASE("load_game: [conquest] loads strictly; ships enabled=true") {
    // Chad's activation (2026-07-24): the conquest sandbox ships LIVE for the
    // certification fly.
    CHECK(kGame.conquest.enabled == true);
    CHECK(kGame.conquest.growth_radius_frac == Catch::Approx(0.25));
    CHECK(kGame.conquest.growth_ceiling_frac == Catch::Approx(0.25));
    // 2026-07-26 STRIKE-MISSION spec: the pilot flies VALLEY (the whole
    // maverick fleet is SUDBURY and hostile, striking the Valley pumps).
    CHECK(kGame.conquest.player_faction == "valley");

    const std::string src = slurp(SEADS_CONFIG_DIR "/game.toml");

    // Fraction bands: [0, 1].
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "growth_radius_frac = 0.25",
                               "growth_radius_frac = 1.5"),
                   "radhigh"),
        kP));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "growth_radius_frac = 0.25",
                               "growth_radius_frac = -0.1"),
                   "radlow"),
        kP));
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "growth_ceiling_frac = 0.25",
                               "growth_ceiling_frac = 1.5"),
                   "ceilhigh"),
        kP));
    // player_faction: exactly "valley" | "sudbury", never a silent 3rd state.
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "player_faction = \"valley\"",
                               "player_faction = \"neutral\""),
                   "badfaction"),
        kP));
    // raid_dps_frac: [0, 1]; ships 0.65 (RUNG E12.2, Chad 2026-08-23 "you can
    // actually lose" -- swept on probe P-H; 0.80 is a cliff that takes his
    // first pump at six minutes, see config/game.toml).
    CHECK(kGame.conquest.raid_dps_frac == Catch::Approx(0.65));
    // ★ THE MUTATION IS BUILT FROM THE LIVE VALUE, never from a literal copy
    // of it. This leg previously spelled the shipped "raid_dps_frac = 0.5"
    // into replace_all, so when E12.2 moved the dial the substitution matched
    // NOTHING, the file loaded unmutated, and the out-of-band REJECTION leg
    // silently stopped testing anything -- the ladder's own recurring trap
    // ("an arm defined by reading the shipped table stops being an arm the
    // moment the table moves, and nothing goes red"). The REQUIRE below is
    // what makes that impossible: if the key is ever renamed or reformatted,
    // this test fails LOUDLY instead of passing vacuously.
    {
        // Take the WHOLE assignment line as it actually stands in the file,
        // so no number is ever spelled twice.
        const std::size_t k = src.find("\nraid_dps_frac = ");
        REQUIRE(k != std::string::npos);
        const std::size_t e = src.find('\n', k + 1);
        REQUIRE(e != std::string::npos);
        const std::string live = src.substr(k + 1, e - k - 1);
        CHECK_THROWS(cfg::load_game_toml(
            write_temp(replace_all(src, live, "raid_dps_frac = 1.5"),
                       "raidhigh"),
            kP));
    }
    // Strict: a missing key throws (never a silent default).
    CHECK_THROWS(cfg::load_game_toml(
        write_temp(replace_all(src, "growth_radius_frac = 0.25",
                               "growth_radius_frac_typo = 0.25"),
                   "conqgone"),
        kP));
}
