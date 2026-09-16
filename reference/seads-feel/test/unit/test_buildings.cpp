// R4f — world::BuildingColliders index correctness (the Fable-named killer
// test): the span-inserted equirect-uv cell index must return EXACTLY the
// verdict of a brute-force O(N) scan over every prism, for query directions
// that include the u wrap seam and high |lat| rows (the real bake's colliders
// reach |lat| 88° — the aeqd hero axis maps to the equirect POLE — and the
// max footprint radius ~127 m exceeds a cell, which is why the index
// span-inserts instead of neighborhood-searching; a wrap slip, an
// under-inserted span, or a pole row bug shows up here as an indexed miss
// the scan catches).

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "render/town_ambience.h"  // the town radii these counts feed
#include "world/buildings.h"
#include "world/heightfield.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 15000.0;

world::HeightField flat_field() {
    world::HeightField hf;
    hf.w = 8;
    hf.h = 4;
    hf.R = kR;
    hf.relief_scale = 350.0;
    hf.u_offset = 0.0;
    hf.px.assign(static_cast<std::size_t>(hf.w) * hf.h, 20000);
    return hf;
}

// Deterministic LCG (never a wall clock — house law).
struct Lcg {
    std::uint64_t s = 0x5EAD5BEEFCAFE123ull;
    double next() {  // [0,1)
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 11) * (1.0 / 9007199254740992.0);
    }
};

glm::dvec3 dir_at(double lat, double lon) {
    return {std::cos(lat) * std::cos(lon), std::sin(lat),
            std::cos(lat) * std::sin(lon)};
}

// Brute force: the ONE reference the index must reproduce.
bool brute_hit(const std::vector<world::BuildingColliders::Prism>& ps,
               const glm::dvec3& pos, const world::HeightField& hf,
               double inflate, double margin) {
    const double r = glm::length(pos);
    const glm::dvec3 d = pos / r;
    for (const auto& p : ps) {
        const double dist = glm::length(d - p.dir) * kR;
        if (dist > p.radius_m + inflate) continue;
        const double base = hf.radius_at(p.dir);
        if (r >= base - margin && r <= base + p.height_m) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("buildings R4f: indexed hit() == brute-force scan (wrap + poles)") {
    const world::HeightField hf = flat_field();
    Lcg rng;

    // Prisms deliberately covering the trap rows: near both poles, ON the u
    // seam (lon ~ 0 == 2pi with u_offset 0: u = 0.5 + atan2(z,x)/2pi — the
    // seam is atan2's +/-pi cut at lon = pi), mid-lat random, and radii up to
    // 130 m (over a cell).
    std::vector<world::BuildingColliders::Prism> prisms;
    prisms.push_back({dir_at(88.0 * kPi / 180.0, 1.3), 90.0, 30.0});
    // The dense-ring prism sits POLEWARD OF 89°: the round-2 reviewer proved
    // empirically that at 87.5-88° the row-edge lat_max conservatism happens
    // to cover the old planar u-span bound (a revert mutant PASSED a ring
    // there); genuine planar-bound misses start at |lat| >= 89.0° (69 misses
    // at 89.4°/r 130 under the old bound, 0 under the exact asin span).
    prisms.push_back({dir_at(-89.4 * kPi / 180.0, 4.0), 130.0, 15.0});
    prisms.push_back({dir_at(0.1, kPi), 60.0, 20.0});          // on the seam
    prisms.push_back({dir_at(-0.4, kPi - 1e-4), 25.0, 40.0});  // beside it
    for (int i = 0; i < 60; ++i) {
        const double lat = (rng.next() - 0.5) * kPi * 0.98;
        const double lon = rng.next() * 2.0 * kPi;
        prisms.push_back({dir_at(lat, lon), 5.0 + 125.0 * rng.next(),
                          3.0 + 40.0 * rng.next()});
    }

    // PRODUCTION shape: the shipped grid (1024x512) and reserve == inflate
    // (main.cpp passes the same [buildings] inflate_m for both — a test with
    // slack reserve is structurally blind to a span-bound under-search, the
    // adversarial-review P2 that reproduced misses at lat 88°/r 127 m).
    const double inflate = 4.0, margin = 5.0;
    world::BuildingColliders bc;
    bc.build(prisms, 1024, 512, 0.0, inflate, kR);
    int hits = 0, checked = 0;
    // Targeted probes: rings around every prism at radii straddling the
    // boundary (0.5x, 0.98x, 1.02x, 2x of radius+inflate) and altitudes
    // straddling the band; plus uniform random probes.
    for (const auto& p : prisms) {
        const glm::dvec3 e1 = glm::normalize(
            glm::cross(std::abs(p.dir.y) < 0.9 ? glm::dvec3{0, 1, 0}
                                               : glm::dvec3{1, 0, 0},
                       p.dir));
        const glm::dvec3 e2 = glm::cross(p.dir, e1);
        const double base = hf.radius_at(p.dir);
        for (double f : {0.5, 0.98, 1.02, 2.0}) {
            for (int k = 0; k < 8; ++k) {
                const double ang = k * kPi / 4.0;
                const double off = f * (p.radius_m + inflate) / kR;
                const glm::dvec3 d = glm::normalize(
                    p.dir + off * (std::cos(ang) * e1 + std::sin(ang) * e2));
                for (double dr : {-margin - 3.0, -1.0, p.height_m * 0.5,
                                  p.height_m + 3.0}) {
                    const glm::dvec3 pos = d * (base + dr);
                    const bool want =
                        brute_hit(prisms, pos, hf, inflate, margin);
                    const bool got = bc.hit(pos, hf, inflate, margin);
                    if (want != got) {
                        INFO("prism r=" << p.radius_m << " f=" << f << " k="
                                        << k << " dr=" << dr);
                        REQUIRE(got == want);
                    }
                    hits += want ? 1 : 0;
                    ++checked;
                }
            }
        }
    }
    // Dense boundary ring around the worst-case prism (poleward of 89° + max
    // radius): 360 bearings exactly AT and a hair inside the cap edge — the
    // sliver the planar u-span bound missed (fails under cap/(2π·cos) at this
    // latitude — reviewer-verified; passes under the exact asin span).
    {
        const auto& p = prisms[1];  // lat -89.4°, r 130 — the worst case
        const glm::dvec3 e1 = glm::normalize(
            glm::cross(std::abs(p.dir.y) < 0.9 ? glm::dvec3{0, 1, 0}
                                               : glm::dvec3{1, 0, 0},
                       p.dir));
        const glm::dvec3 e2 = glm::cross(p.dir, e1);
        const double base = hf.radius_at(p.dir);
        for (int k = 0; k < 360; ++k) {
            const double ang = k * (2.0 * kPi / 360.0);
            for (double f : {0.999, 1.0}) {
                const double off = f * (p.radius_m + inflate) / kR;
                const glm::dvec3 d = glm::normalize(
                    p.dir + off * (std::cos(ang) * e1 + std::sin(ang) * e2));
                const glm::dvec3 pos = d * (base + p.height_m * 0.5);
                const bool want = brute_hit(prisms, pos, hf, inflate, margin);
                const bool got = bc.hit(pos, hf, inflate, margin);
                if (want != got) {
                    INFO("dense ring k=" << k << " f=" << f);
                    REQUIRE(got == want);
                }
                hits += want ? 1 : 0;
                ++checked;
            }
        }
    }
    for (int i = 0; i < 20000; ++i) {
        const double lat = (rng.next() - 0.5) * kPi * 0.999;
        const double lon = rng.next() * 2.0 * kPi;
        const glm::dvec3 d = dir_at(lat, lon);
        const glm::dvec3 pos = d * (hf.radius_at(d) + 60.0 * rng.next() - 8.0);
        const bool want = brute_hit(prisms, pos, hf, inflate, margin);
        const bool got = bc.hit(pos, hf, inflate, margin);
        if (want != got) {
            INFO("random probe " << i << " lat=" << lat << " lon=" << lon);
            REQUIRE(got == want);
        }
        hits += want ? 1 : 0;
        ++checked;
    }
    // Fixture-no-op guards: the sweep genuinely exercised BOTH verdicts (an
    // index that never fires, or prisms nothing ever hits, would pass a
    // mutation vacuously).
    REQUIRE(hits > 100);
    REQUIRE(hits < checked);
}

// ---------------------------------------------------------------------------
// prisms_near_m — the town-density query the train ambience runs on
// (render/town_ambience.h). Same standard as hit() above: it must reproduce a
// brute-force O(N) count EXACTLY, at the u wrap seam and at high |lat|, and it
// must count each collider ONCE however many index cells it was span-inserted
// into. The de-duplication is the part worth a killer test — without it a wide
// building reads as several and a farmstead is promoted to a town.
// ---------------------------------------------------------------------------
namespace {

int brute_near(const std::vector<world::BuildingColliders::Prism>& ps,
               const glm::dvec3& pos, double radius_m) {
    const glm::dvec3 d = glm::normalize(pos);
    const double cos_cap = std::cos(std::min(radius_m / kR, kPi * 0.5));
    int n = 0;
    for (const auto& p : ps)
        if (glm::dot(d, p.dir) >= cos_cap) ++n;
    return n;
}

}  // namespace

TEST_CASE("prisms_near_m: exactly the brute-force count, seam and poles too") {
    Lcg rng;
    const world::HeightField hf = flat_field();
    std::vector<world::BuildingColliders::Prism> prisms;
    // Clusters, so the counts span "empty", "farmstead" and "town" instead of
    // hovering at one density the thresholds could not tell apart.
    for (int c = 0; c < 24; ++c) {
        const double clat = (rng.next() - 0.5) * kPi * 0.98;
        const double clon = rng.next() * 2.0 * kPi;
        const int n = 1 + static_cast<int>(rng.next() * 40.0);
        for (int i = 0; i < n; ++i) {
            const double dlat = (rng.next() - 0.5) * 0.06;
            const double dlon = (rng.next() - 0.5) * 0.06;
            prisms.push_back({glm::normalize(dir_at(clat + dlat, clon + dlon)),
                              6.0 + 120.0 * rng.next(), 4.0 + 20.0 * rng.next()});
        }
    }
    // The seam and both poles explicitly, not left to the random draw.
    for (double lon : {0.0, 2.0 * kPi - 1e-9, kPi})
        for (double lat : {0.0, 1.55, -1.55})
            prisms.push_back({glm::normalize(dir_at(lat, lon)), 40.0, 8.0});

    world::BuildingColliders bc;
    bc.build(prisms, 1024, 512, 0.0, 4.0, kR);

    int nonzero = 0, saw_town = 0, checked = 0;
    const double radii[] = {render::kTownRadiusGroundM,
                            render::kTownRadiusFlyingM, 90.0, 12000.0};
    auto probe = [&](const glm::dvec3& d) {
        for (double rad : radii) {
            const glm::dvec3 pos = d * (hf.radius_at(d) + 500.0);
            const int want = brute_near(prisms, pos, rad);
            const int got = bc.prisms_near_m(pos, rad);
            if (want != got) {
                INFO("radius " << rad << " want " << want << " got " << got);
                REQUIRE(got == want);
            }
            nonzero += want > 0 ? 1 : 0;
            saw_town += want >= render::kTownPrismCountFlying ? 1 : 0;
            ++checked;
        }
    };
    for (const auto& p : prisms) probe(p.dir);
    for (double lon : {0.0, 1e-9, 2.0 * kPi - 1e-9, kPi, kPi * 0.5})
        for (double lat : {0.0, 0.8, -0.8, 1.5533, -1.5533}) probe(dir_at(lat, lon));
    for (int i = 0; i < 3000; ++i)
        probe(dir_at((rng.next() - 0.5) * kPi * 0.999, rng.next() * 2.0 * kPi));

    // Fixture-no-op guards: the sweep must have seen real density, not just
    // empty country, or the de-duplication is untested.
    REQUIRE(nonzero > 100);
    REQUIRE(saw_town > 20);
    REQUIRE(nonzero < checked);
}

TEST_CASE("prisms_near_m: one building is one building, not one per cell") {
    // The point of the stamp buffer. A collider whose footprint is wider than
    // an index cell (~92 m at the shipped 1024x512 on R=15 km) is span-inserted
    // into several, and a query that walks those cells sees it several times.
    std::vector<world::BuildingColliders::Prism> prisms;
    prisms.push_back({glm::normalize(dir_at(0.81, 1.2)), 127.0, 9.0});
    world::BuildingColliders bc;
    bc.build(prisms, 1024, 512, 0.0, 4.0, kR);
    const glm::dvec3 pos = prisms[0].dir * (kR + 300.0);
    CHECK(bc.prisms_near_m(pos, 2000.0) == 1);
    CHECK(bc.prisms_near_m(pos, render::kTownRadiusFlyingM) == 1);
    // ...and it is measured to the building's CENTRE, not to the far edge of
    // its footprint: a 127 m disc does not make you "within 50 m" of it.
    CHECK(bc.prisms_near_m(pos, 50.0) == 1);   // you are standing on it
    const glm::dvec3 off = glm::normalize(prisms[0].dir +
                                          glm::dvec3{0.0, 300.0 / kR, 0.0});
    CHECK(bc.prisms_near_m(off * (kR + 300.0), 100.0) == 0);
    CHECK(bc.prisms_near_m(off * (kR + 300.0), 600.0) == 1);
}

TEST_CASE("prisms_near_m: degenerate inputs answer 0 rather than reading off "
          "the end of the index") {
    world::BuildingColliders empty;
    CHECK(empty.prisms_near_m(glm::dvec3{kR, 0.0, 0.0}, 500.0) == 0);
    std::vector<world::BuildingColliders::Prism> prisms;
    prisms.push_back({glm::normalize(dir_at(0.0, 0.0)), 20.0, 6.0});
    world::BuildingColliders bc;
    bc.build(prisms, 1024, 512, 0.0, 4.0, kR);
    CHECK(bc.prisms_near_m(glm::dvec3{0.0, 0.0, 0.0}, 500.0) == 0);  // r == 0
    CHECK(bc.prisms_near_m(glm::dvec3{kR, 0.0, 0.0}, 0.0) == 0);
    CHECK(bc.prisms_near_m(glm::dvec3{kR, 0.0, 0.0}, -5.0) == 0);
    // Repeated queries must not accumulate stamps into a growing count.
    const glm::dvec3 pos = prisms[0].dir * (kR + 10.0);
    const int first = bc.prisms_near_m(pos, 500.0);
    for (int i = 0; i < 2000; ++i) CHECK(bc.prisms_near_m(pos, 500.0) == first);
    CHECK(first == 1);
}
