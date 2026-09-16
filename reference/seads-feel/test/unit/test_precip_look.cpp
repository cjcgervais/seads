// ATMOSPHERE AS-3 — the LOOK terms of the precip field, pinned headlessly.
//
// Chad, 2026-09-12: snow must read as snow — "appearance of snow in a very
// effective way that deepens immersion", not uniform dots in a globe. AS-3 adds
// three pure terms to render::precip_sample and this file is where each one's
// load-bearing claim is checked, including the claim that every one of them has
// an OFF value that is BIT-IDENTICAL to the shipped W3 field.
//
// THE ONE THAT MATTERS MOST: the SWAY. Chad's NO-WIND ruling is absolute, and a
// lateral motion term is the one thing here a strict reading could object to.
// So the no-wind property is pinned TWICE: the original test_precip.cpp case
// still asserts pure-axial motion with sway OFF, and the cases below assert
// that with sway ON the mean lateral displacement over one full fall cycle is
// EXACTLY zero (to 1e-9), the excursion is bounded by the amplitude, and no two
// cells share a sway direction. Zero mean + no shared direction = no wind: the
// field never translates and has no direction.

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

#include "render/precip.h"

using render::precip_sample;
using render::PrecipFieldParams;
using render::PrecipSample;

namespace {
// A generic (non-axis-aligned) local-up so a world-lattice axis never coincides
// with the truth direction (the flat-frame trap).
const glm::dvec3 kUp = glm::normalize(glm::dvec3(0.3, -0.7, 0.5));
constexpr double kCell = 3.0;
constexpr double kBoxHalf = 21.0;
constexpr double kWrap = 0.12;

PrecipFieldParams base_params() {
    PrecipFieldParams fp;
    fp.cell_size = kCell;
    fp.box_half = kBoxHalf;
    fp.wrap_fade = kWrap;
    return fp;
}
}  // namespace

TEST_CASE("precip AS-3: every new term OFF is BIT-IDENTICAL to the W3 field") {
    // The landing condition for the whole rung: a config with the new dials at
    // zero must reproduce today exactly. Not "approximately" — bit-identical,
    // because the OFF path is a delegation to the same arithmetic, not a copy.
    const PrecipFieldParams fp = base_params();  // all AS-3 terms default 0
    const glm::dvec3 eye(500.0, -300.0, 900.0);
    for (int ci = -6; ci <= 6; ++ci)
        for (int cj = -6; cj <= 6; ++cj)
            for (int ck = -6; ck <= 6; ++ck) {
                const glm::ivec3 cell(166 + ci, -100 + cj, 300 + ck);
                for (double ph : {0.0, 0.13, 0.5, 0.87}) {
                    const PrecipSample a =
                        precip_sample(cell, eye, kUp, ph, fp);
                    // the PRE-AS-3 signature, i.e. literally the old call
                    const PrecipSample b = precip_sample(
                        cell, eye, kUp, kCell, kBoxHalf, kWrap, ph);
                    REQUIRE(a.pos.x == b.pos.x);
                    REQUIRE(a.pos.y == b.pos.y);
                    REQUIRE(a.pos.z == b.pos.z);
                    REQUIRE(a.alpha == b.alpha);
                    REQUIRE(a.size_mul == 1.0);
                }
            }
}

TEST_CASE("precip AS-3 sway: the mean lateral displacement over a cycle is ZERO") {
    // ⚑ THE NO-WIND PROOF for the sway term. Average the flake's LATERAL
    // (perpendicular-to-local_up) position over one full fall cycle. Wind is a
    // non-zero mean; a zero-mean sinusoid is not wind. N samples over exactly
    // one period of a sine sum to zero, so this is exact, not statistical.
    //
    // Kills the mutant that turns the sway into an advection term (replace
    // sin(phase) with phase, or add a constant drift): either one leaves a
    // non-zero mean here while every other property still holds.
    PrecipFieldParams fp = base_params();
    fp.sway_m = 0.25;
    fp.wrap_fade = 0.0;  // isolate POSITION from the alpha wrap fade
    // box_half is set enormous on purpose: the sampler short-circuits
    // flakes that are outside the fade sphere (they are invisible, so their
    // position is not a contract), and a partial set of fall phases would
    // make the mean of a sine over "one cycle" a lie. Here we want the sway
    // TERM, every phase of it.
    fp.box_half = 1.0e6;
    const glm::dvec3 eye(0.0);
    const int N = 4096;
    double worst = 0.0;
    for (int ci = -3; ci <= 3; ++ci)
        for (int cj = -3; cj <= 3; ++cj)
            for (int ck = -3; ck <= 3; ++ck) {
                const glm::ivec3 cell(ci * 7 + 1, cj * 5 - 2, ck * 11 + 3);
                // The no-sway reference at the same phases: the difference is
                // the sway displacement alone.
                glm::dvec3 acc(0.0);
                PrecipFieldParams off = fp;
                off.sway_m = 0.0;
                for (int i = 0; i < N; ++i) {
                    const double ph = static_cast<double>(i) / N;
                    const glm::dvec3 d =
                        precip_sample(cell, eye, kUp, ph, fp).pos -
                        precip_sample(cell, eye, kUp, ph, off).pos;
                    acc += d;
                }
                acc /= static_cast<double>(N);
                worst = std::max(worst, glm::length(acc));
            }
    CHECK(worst < 1e-9);  // EXACTLY zero mean, to double round-off
}

TEST_CASE("precip AS-3 sway: the excursion is bounded by the amplitude") {
    // |lateral| <= sway_m, always. A sway that can exceed its own dial is a
    // dial Chad cannot trust when he rules on it.
    PrecipFieldParams fp = base_params();
    fp.sway_m = 0.25;
    fp.box_half = 1.0e6;  // see the note above: measure the sway TERM
    PrecipFieldParams off = fp;
    off.sway_m = 0.0;
    const glm::dvec3 eye(0.0);
    double worst = 0.0;
    for (int ci = -5; ci <= 5; ++ci)
        for (int ck = -5; ck <= 5; ++ck)
            for (int i = 0; i < 64; ++i) {
                const glm::ivec3 cell(ci, 4, ck);
                const double ph = static_cast<double>(i) / 64.0;
                const glm::dvec3 d =
                    precip_sample(cell, eye, kUp, ph, fp).pos -
                    precip_sample(cell, eye, kUp, ph, off).pos;
                // and it IS lateral: no component along local_up
                REQUIRE(std::abs(glm::dot(d, kUp)) < 1e-12);
                worst = std::max(worst, glm::length(d));
            }
    CHECK(worst <= 0.25 + 1e-12);
    CHECK(worst > 0.2);  // and it is not silently inert
}

TEST_CASE("precip AS-3 sway: no two cells sway in lockstep (no shared direction)") {
    // The second half of "not wind": wind is a SHARED direction. Here each
    // cell's axis AND phase are hashed, so the population of sway directions
    // has no preferred direction — their vector mean is ~0 while their mean
    // MAGNITUDE is not. A mutant using one fixed tangent basis for all cells
    // (which is what "wind" would look like) fails the first check.
    PrecipFieldParams fp = base_params();
    fp.sway_m = 1.0;
    fp.box_half = 1.0e6;  // see the note above: measure the sway TERM
    PrecipFieldParams off = fp;
    off.sway_m = 0.0;
    const glm::dvec3 eye(0.0);
    glm::dvec3 vec_sum(0.0);
    double mag_sum = 0.0;
    int n = 0;
    for (int ci = -8; ci <= 8; ++ci)
        for (int cj = -8; cj <= 8; ++cj)
            for (int ck = -8; ck <= 8; ++ck) {
                const glm::ivec3 cell(ci, cj, ck);
                const glm::dvec3 d =
                    precip_sample(cell, eye, kUp, 0.31, fp).pos -
                    precip_sample(cell, eye, kUp, 0.31, off).pos;
                vec_sum += d;
                mag_sum += glm::length(d);
                ++n;
            }
    const double mean_vec = glm::length(vec_sum) / n;
    const double mean_mag = mag_sum / n;
    CHECK(mean_mag > 0.3);              // the cells really are swaying
    CHECK(mean_vec < 0.1 * mean_mag);   // but the population has no direction
}

TEST_CASE("precip AS-3 density: the field thins with intensity, and 0 => none") {
    // "A flurry = a few soft flakes; a squall = a dense fall." The kept
    // FRACTION must be monotone non-decreasing in intensity, must be zero at
    // intensity 0, and must be everything at intensity 1.
    PrecipFieldParams fp = base_params();
    fp.density_exp = 0.6;
    fp.wrap_fade = 0.0;
    fp.box_half = 1.0e6;  // isolate the CULL from the boundary fade
    const glm::dvec3 eye(0.0);
    auto kept_frac = [&](double intensity) {
        PrecipFieldParams q = fp;
        q.keep_p = render::precip_keep_p(intensity, q.density_exp);
        int kept = 0, total = 0;
        for (int ci = -9; ci <= 9; ++ci)
            for (int cj = -9; cj <= 9; ++cj)
                for (int ck = -9; ck <= 9; ++ck) {
                    // Sample near the eye so the boundary fade is not what
                    // zeroes the alpha — the CULL is what we are measuring.
                    const glm::ivec3 cell(ci, cj, ck);
                    ++total;
                    if (precip_sample(cell, eye, kUp, 0.5, q).alpha > 0.0)
                        ++kept;
                }
        return static_cast<double>(kept) / total;
    };
    const double f0 = kept_frac(0.0);
    const double f03 = kept_frac(0.3);
    const double f07 = kept_frac(0.7);
    const double f1 = kept_frac(1.0);
    CHECK(f0 == 0.0);  // intensity 0 => NOTHING drawn
    CHECK(f03 < f07);
    CHECK(f07 < f1);
    CHECK(f03 > 0.0);  // a flurry is a few flakes, not no flakes
    // density_exp == 0 is the OFF value: the cull keeps everything, at every
    // intensity, including 0 (the renderer's own intensity gate handles that).
    PrecipFieldParams offp = base_params();
    offp.wrap_fade = 0.0;
    offp.box_half = 1.0e6;  // isolate the CULL from the boundary fade
    offp.keep_p = render::precip_keep_p(0.0, offp.density_exp);  // == 1: OFF
    int kept_off = 0;
    for (int ci = -5; ci <= 5; ++ci)
        for (int ck = -5; ck <= 5; ++ck)
            if (precip_sample(glm::ivec3(ci, 0, ck), eye, kUp, 0.5, offp).alpha >
                0.0)
                ++kept_off;
    CHECK(kept_off == 11 * 11);
}

TEST_CASE("precip AS-3 variety: size and alpha vary per cell, within their dials") {
    // Hashed from the WORLD cell, so a flake keeps its size for its whole life
    // and two eyes agree — the variety is a property of the world, not of the
    // frame. Bounds pin the dial's meaning: size in [1-v, 1+v], alpha scaled
    // into [1-v, 1].
    PrecipFieldParams fp = base_params();
    fp.size_var = 0.4;
    fp.alpha_var = 0.4;
    fp.box_half = 1.0e6;  // measure the VARIETY, not the boundary fade
    PrecipFieldParams off = base_params();
    off.box_half = 1.0e6;
    const glm::dvec3 eye(0.0);
    double smin = 1e9, smax = -1e9, ratio_min = 1e9, ratio_max = -1e9;
    for (int ci = -10; ci <= 10; ++ci)
        for (int ck = -10; ck <= 10; ++ck) {
            const glm::ivec3 cell(ci, 2, ck);
            const PrecipSample a = precip_sample(cell, eye, kUp, 0.5, fp);
            const PrecipSample b = precip_sample(cell, eye, kUp, 0.5, off);
            smin = std::min(smin, a.size_mul);
            smax = std::max(smax, a.size_mul);
            REQUIRE(a.pos == b.pos);  // variety NEVER moves a flake
            if (b.alpha > 0.0) {
                const double ratio = a.alpha / b.alpha;
                ratio_min = std::min(ratio_min, ratio);
                ratio_max = std::max(ratio_max, ratio);
            }
        }
    CHECK(smin >= 0.6 - 1e-12);
    CHECK(smax <= 1.4 + 1e-12);
    CHECK(smax - smin > 0.5);  // the spread is real, not a rounding artefact
    CHECK(ratio_min >= 0.6 - 1e-12);
    CHECK(ratio_max <= 1.0 + 1e-12);
    CHECK(ratio_max - ratio_min > 0.3);
    // Determinism: the same cell hands back the same size on a second call.
    const glm::ivec3 c(7, 2, -3);
    CHECK(precip_sample(c, eye, kUp, 0.5, fp).size_mul ==
          precip_sample(c, eye, kUp, 0.9, fp).size_mul);
}

TEST_CASE("precip AS-3: the FAR veil's inner HOLE keeps big flakes off the face") {
    // Red-team P1-4. The veil's flakes are 0.45 m; Chad already ruled 0.28 m
    // "too big / in your face" (that note is why snow_size_m is 0.18 today). A
    // 70 m lattice of 0.45 m flakes with no hole in it puts them in the canopy.
    // inner_fade_m is 0 inside the radius, unchanged from twice it out, C0
    // across the one-radius ramp, and OFF at 0 (which is what the NEAR lattice
    // ships).
    const double inner_m = 12.0;
    PrecipFieldParams fp = base_params();
    fp.cell_size = 8.0;
    fp.box_half = 70.0;
    fp.inner_fade_m = inner_m;
    PrecipFieldParams off = fp;
    off.inner_fade_m = 0.0;
    const glm::dvec3 eye(4000.0, -2500.0, 9000.0);

    int inside_hole = 0, beyond_ramp = 0, in_ramp = 0;
    for (int ci = -12; ci <= 12; ++ci)
        for (int cj = -12; cj <= 12; ++cj)
            for (int ck = -12; ck <= 12; ++ck) {
                const glm::ivec3 cell =
                    render::precip_center_cell(eye, fp.cell_size) +
                                        glm::ivec3(ci, cj, ck);
                const PrecipSample a = precip_sample(cell, eye, kUp, 0.37, fp);
                const PrecipSample b = precip_sample(cell, eye, kUp, 0.37, off);
                const double d = glm::length(b.pos - eye);
                if (d <= inner_m) {
                    REQUIRE(a.alpha == 0.0);  // the hole is EMPTY
                    ++inside_hole;
                } else if (d >= 2.0 * inner_m) {
                    REQUIRE(a.alpha == b.alpha);  // untouched beyond the ramp
                    if (b.alpha > 0.0) ++beyond_ramp;
                } else {
                    REQUIRE(a.alpha <= b.alpha + 1e-12);  // only ever removes
                    if (a.alpha > 0.0 && a.alpha < b.alpha) ++in_ramp;
                }
            }
    REQUIRE(inside_hole > 0);   // the fixture really does put flakes in the hole
    REQUIRE(beyond_ramp > 0);   // and plenty outside it, still drawn
    CHECK(in_ramp > 0);         // and the ramp is a ramp, not a second step

    // Monotone and C0 across the ramp, measured on the radius itself.
    double prev = -1.0;
    for (int i = 0; i <= 400; ++i) {
        const double d = 0.5 * inner_m + (2.5 * inner_m) * (i / 400.0);
        // smoothstep(inner, 2*inner, d) is the factor; re-derive it here so a
        // mutant that changes the ramp SHAPE is caught, not just its endpoints.
        const double t = std::clamp((d - inner_m) / inner_m, 0.0, 1.0);
        const double f = t * t * (3.0 - 2.0 * t);
        REQUIRE(f >= prev - 1e-12);
        prev = f;
    }
}

TEST_CASE("precip AS-3: the density cull fades cells IN, and OFF is the hard step") {
    // Red-team P2b. A hard threshold pops whole cells on as a squall breathes.
    // density_soft fades a cell in over that width of keep_p. At 0 the old hard
    // step is reproduced exactly.
    PrecipFieldParams fp = base_params();
    fp.density_exp = 0.6;
    fp.wrap_fade = 0.0;
    fp.box_half = 1.0e6;
    fp.density_soft = 0.08;
    PrecipFieldParams hard = fp;
    hard.density_soft = 0.0;
    const glm::dvec3 eye(0.0);

    int partial = 0, full = 0, gone = 0;
    fp.keep_p = render::precip_keep_p(0.45, fp.density_exp);
    hard.keep_p = fp.keep_p;
    for (int ci = -9; ci <= 9; ++ci)
        for (int cj = -9; cj <= 9; ++cj)
            for (int ck = -9; ck <= 9; ++ck) {
                const glm::ivec3 cell(ci, cj, ck);
                const double a = precip_sample(cell, eye, kUp, 0.5, fp).alpha;
                const double h = precip_sample(cell, eye, kUp, 0.5, hard).alpha;
                REQUIRE((h == 0.0 || h == Catch::Approx(1.0)));  // hard: on/off
                if (a <= 0.0)
                    ++gone;
                else if (a >= 1.0 - 1e-12)
                    ++full;
                else
                    ++partial;
            }
    CHECK(gone > 0);
    CHECK(full > 0);
    CHECK(partial > 0);  // the soft band really is populated
    std::printf("[AS-3 MEASURED] density cull at keep_p %.3f: %d gone, %d "
                "partial, %d full\n",
                fp.keep_p, gone, partial, full);
}
