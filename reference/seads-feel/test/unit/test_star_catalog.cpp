// Star catalog + star-pass conventions (docs/little_planet_plan.md Stage 4).
// The catalog is REAL data (render/star_catalog.gen.h, HYG-derived; Chad ruled
// real, 2026-07-09), so the tripwires are: the data is sane (unit dirs, mag
// range), Polaris actually sits on the spin axis (the sky wheels about it — the
// diegetic compass), and the named hero anchors resolve. The GLSL uniform sets
// are gated in test_asset_validator.cpp (the clock/state backdoor guard). No GL
// context here — pure catalog + render::radec_to_dir math.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/glm.hpp>
#include <string>

#include "render/celestial.h"
#include "render/star_catalog.gen.h"

#ifdef NDEBUG
#error "SEADS gate requires an assert-live build (SPEC 6.1)"
#endif

using Catch::Approx;
using render::CelestialConfig;
using render::CelestialParams;
using render::make_celestial;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 15000.0;
constexpr double kD2R = kPi / 180.0;

CelestialParams cel() {
    CelestialConfig c;
    c.orbit_normal = {0.0, 1.0, 0.0};
    c.tilt_deg = 23.5;
    c.tilt_lean = {1.0, 0.0, 0.0};
    c.day_period_s = 3600.0;
    c.year_period_s = 43200.0;
    return make_celestial(c, kR);
}

glm::dvec3 star_dir(const CelestialParams& c, const render::CatalogStar& s) {
    return render::radec_to_dir(c, s.ra_deg * kD2R, s.dec_deg * kD2R);
}
}  // namespace

// The catalog is non-empty and every entry is physically sane: a unit direction
// (radec_to_dir on an authored RA/Dec), a magnitude in the naked-eye range, and
// a declination on the sphere. A parse slip (RA left in HOURS not degrees, a
// bad column) would blow one of these.
TEST_CASE("star catalog: every entry is a sane unit-dir star") {
    const CelestialParams c = cel();
    REQUIRE(render::kStarCatalogCount > 100);  // a real bright-star sky
    REQUIRE(render::kStarCatalogCount ==
            sizeof(render::kStarCatalog) / sizeof(render::kStarCatalog[0]));
    for (std::size_t i = 0; i < render::kStarCatalogCount; ++i) {
        const render::CatalogStar& s = render::kStarCatalog[i];
        INFO("star " << i << " ra=" << s.ra_deg << " dec=" << s.dec_deg
                     << " mag=" << s.mag);
        REQUIRE(s.ra_deg >= 0.0f);
        REQUIRE(s.ra_deg <= 360.0f);
        REQUIRE(s.dec_deg >= -90.0f);
        REQUIRE(s.dec_deg <= 90.0f);
        REQUIRE(s.mag >= -2.0f);  // Sirius -1.46 is the brightest
        REQUIRE(s.mag <= 8.0f);   // bake limit ~5.5 + margin
        REQUIRE(glm::length(star_dir(c, s)) == Approx(1.0).margin(1e-9));
    }
}

// Brightest-first ordering (the generator sorts by magnitude): star 0 is the
// brightest in the sky. A mangled sort or a unit slip in mag would break this.
TEST_CASE("star catalog: sorted brightest-first, star 0 is Sirius-bright") {
    REQUIRE(render::kStarCatalog[0].mag <= -1.0f);  // Sirius (-1.46)
    for (std::size_t i = 1; i < render::kStarCatalogCount; ++i)
        REQUIRE(render::kStarCatalog[i].mag >=
                render::kStarCatalog[i - 1].mag - 1e-4f);
}

// THE compass invariant: Polaris sits on the spin axis â, so the sky wheels
// about it and "find home by the pointer stars -> Polaris" works. Polaris is
// dec ~ +89.26 (0.74 deg off the true pole), so its world dir hugs â. A wrong
// equatorial basis in radec_to_dir, or Polaris parsed wrong, drops the dot.
TEST_CASE("star catalog: Polaris sits on the spin axis (the diegetic compass)") {
    const CelestialParams c = cel();
    bool found = false;
    for (std::size_t i = 0; i < render::kNamedStarCount; ++i) {
        if (std::string(render::kNamedStars[i].name) != "Polaris") continue;
        found = true;
        const int idx = render::kNamedStars[i].index;
        REQUIRE(idx >= 0);
        REQUIRE(idx < static_cast<int>(render::kStarCatalogCount));
        const render::CatalogStar& p = render::kStarCatalog[idx];
        REQUIRE(p.dec_deg > 89.0f);  // ~+89.26 in real data
        // Its world dir is within ~0.74 deg of â (cos 0.74 deg ~ 0.99992).
        REQUIRE(glm::dot(star_dir(c, p), c.spin_axis) > 0.9999);
    }
    REQUIRE(found);
}

// The dec=+90 pole maps EXACTLY to â (the header's contract), so Polaris-near-â
// is the wheel eigenvector story, not a coincidence of tilt.
TEST_CASE("star catalog: the celestial pole (dec=+90) is exactly the spin axis") {
    const CelestialParams c = cel();
    const glm::dvec3 pole = render::radec_to_dir(c, 0.0, 0.5 * kPi);
    REQUIRE(glm::dot(pole, c.spin_axis) == Approx(1.0).margin(1e-12));
}

// The named hero anchors all resolve into the catalog and carry real
// brightness (a dropped/renamed hero would leave a dangling index or a faint
// impostor). Sirius/Betelgeuse/Rigel etc. anchor Chad's HUD-off navigation.
TEST_CASE("star catalog: named hero anchors resolve to real bright stars") {
    REQUIRE(render::kNamedStarCount >= 15);  // the Dippers/Orion/Cassiopeia/Crux
    for (std::size_t i = 0; i < render::kNamedStarCount; ++i) {
        const render::NamedStar& n = render::kNamedStars[i];
        INFO("named " << n.name << " (" << n.constellation << ")");
        REQUIRE(n.index >= 0);
        REQUIRE(n.index < static_cast<int>(render::kStarCatalogCount));
        // Hero navigators are naked-eye conspicuous (all < mag 4).
        REQUIRE(render::kStarCatalog[n.index].mag < 4.0f);
    }
}
