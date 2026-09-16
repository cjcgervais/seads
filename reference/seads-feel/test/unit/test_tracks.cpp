// SF3-B -- world/tracks.* gate legs. The TRACK FIELD deforms the DRIVEN
// surface (world/snowpack.cpp's drive_radius_at / depth_geometry_at / depth_at
// compose it beside snowhill_add), which is what lets the rider snow patch
// (render/snow_patch.h) draw a rut without a second representation of it.
//
// The legs that matter here are the ones a green build would otherwise hide:
//
//   1. UNTRACKED GROUND IS BIT-IDENTICAL. deform_at must return exactly 0.0
//      (not "small") off-track and on an empty field, because that is what
//      makes binding a TrackField a no-op everywhere the machine has not been.
//      An epsilon here would move every surface in the game.
//   2. The profile: full depression at the stamp, a POSITIVE berm outside the
//      groove, and exactly zero at and beyond the influence radius.
//   3. NO SCALLOPING along a path. The trench is built from per-stamp
//      distances; if the spacing were too coarse for the groove width the
//      floor would ripple between stamps.
//   4. The berms of neighbouring stamps must NOT fill in the groove -- the
//      reason deform_at runs ONE profile off the nearest-stamp distance
//      instead of summing per-stamp profiles. This is the leg that fails if
//      anyone "simplifies" it into a sum.
//   5. RING EVICTION removes a slot from its OLD GRID CELL. This is the
//      subtle one: forget it and the index keeps pointing at a stamp that has
//      been overwritten, so ancient track re-appears at the wrong place.
//   6. compaction_at reduces reported depth and is never negative.

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <glm/geometric.hpp>

#include "render/sphere_param.h"
#include "world/snowpack.h"
#include "world/tracks.h"

namespace {

constexpr double kR = 15000.0;

// A direction `metres` east of the anchor, on the sphere.
glm::dvec3 east_of(glm::dvec3 c, double metres) {
    const glm::dvec3 e = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), c));
    const double a = metres / kR;
    return glm::normalize(c * std::cos(a) + e * std::sin(a));
}

glm::dvec3 north_of(glm::dvec3 c, double metres) {
    const glm::dvec3 e = glm::normalize(glm::cross(glm::dvec3(0, 1, 0), c));
    const glm::dvec3 n = glm::normalize(glm::cross(c, e));
    const double a = metres / kR;
    return glm::normalize(c * std::cos(a) + n * std::sin(a));
}

const glm::dvec3 kC = glm::normalize(glm::dvec3(0.3, 0.5, 0.81));

}  // namespace

TEST_CASE("SF3-B: untracked ground is bit-identically untouched", "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    // An EMPTY field: every query is exactly zero, so binding one changes
    // nothing anywhere the machine has not driven.
    REQUIRE(tf.empty());
    REQUIRE(tf.deform_at(kC) == 0.0);
    REQUIRE(tf.compaction_at(kC) == 0.0);

    // One stamp laid; ground beyond the influence radius is STILL exactly zero.
    REQUIRE(tf.add(kC));
    const double reach = p.half_width_m + p.berm_width_m;
    REQUIRE(tf.deform_at(east_of(kC, reach + 0.01)) == 0.0);
    REQUIRE(tf.deform_at(east_of(kC, 50.0)) == 0.0);
    REQUIRE(tf.compaction_at(east_of(kC, reach + 0.01)) == 0.0);
}

TEST_CASE("SF3-B: the cross-section is groove, then berm, then nothing",
          "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    REQUIRE(tf.add(kC));

    // At the stamp: the full depression, downward.
    REQUIRE(tf.deform_at(kC) < 0.0);
    REQUIRE(std::abs(tf.deform_at(kC) + p.depress_m) < 1e-9);

    // Just outside the groove: the berm is UP. A track that only cut down and
    // never threw snow aside would read as a scratch, not a rut.
    const double berm_peak = p.half_width_m + 0.5 * p.berm_width_m;
    REQUIRE(tf.deform_at(east_of(kC, berm_peak)) > 0.0);

    // The berm meets untouched snow without a step. TWO separate claims,
    // because they are different guarantees:
    //   (a) BEYOND the influence radius the result is EXACTLY 0.0 -- the
    //       branch, not the profile, and what makes untracked ground
    //       bit-identical.
    //   (b) AT the radius the profile is CONTINUOUS, i.e. it has already
    //       decayed to nothing, so there is no cliff for the mesh to alias.
    // The sample cannot be placed exactly ON the radius to test (a): east_of
    // steps by ARC while the field measures CHORD, so an arc-`reach` sample
    // lands a hair inside and legitimately returns a sub-nanometre berm.
    const double reach = p.half_width_m + p.berm_width_m;
    REQUIRE(tf.deform_at(east_of(kC, reach * 1.001)) == 0.0);
    REQUIRE(std::abs(tf.deform_at(east_of(kC, reach))) < 1e-9);

    // Monotone descent into the groove from the edge inward.
    REQUIRE(tf.deform_at(east_of(kC, 0.0)) <=
            tf.deform_at(east_of(kC, 0.5 * p.half_width_m)));
}

TEST_CASE("SF3-B: a driven path leaves a continuous trench, not scallops",
          "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    // Drive 20 m north, sampling far finer than stamp_spacing_m so add()'s own
    // rejection is what sets the spacing.
    for (double s = 0.0; s <= 20.0; s += 0.05) tf.add(north_of(kC, s));
    REQUIRE(tf.size() > 10);

    // Along the centreline the floor must stay at (or below) the full
    // depression everywhere -- no ripple between stamps.
    double worst = -1e9;
    for (double s = 2.0; s <= 18.0; s += 0.037) {
        const double d = tf.deform_at(north_of(kC, s));
        worst = std::max(worst, d);
    }
    // Allow a hair of numerical slack, but nothing like a visible scallop.
    REQUIRE(worst < -p.depress_m * 0.98);
}

TEST_CASE("SF3-B: neighbouring berms do not fill in the groove", "[tracks]") {
    // THE LEG THAT CATCHES A SUM. If deform_at summed a profile per stamp
    // instead of running one profile off the NEAREST stamp, then on a dense
    // path the positive berm lobes of the stamps ahead and behind would add
    // into the groove and cancel the depression -- the trench would vanish
    // exactly where the machine drove.
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    for (double s = 0.0; s <= 10.0; s += 0.05) tf.add(north_of(kC, s));

    const double mid = tf.deform_at(north_of(kC, 5.0));
    REQUIRE(mid < 0.0);
    REQUIRE(std::abs(mid + p.depress_m) < 1e-6);
}

TEST_CASE("SF3-B: ring eviction clears the old grid cell", "[tracks]") {
    // THE SUBTLE ONE. A slot being overwritten must be removed from the cell
    // it was indexed under. Skip that and the grid keeps a live-looking index
    // to a slot whose direction has been replaced, so old track re-appears
    // somewhere it was never driven.
    world::TrackField tf;
    world::TrackParams p;
    p.max_stamps = 64;
    tf.reset(kR, p);

    const glm::dvec3 first = kC;
    // Lay well past capacity, marching far enough that the early stamps end up
    // in entirely different grid cells from the late ones.
    for (int i = 0; i < 400; ++i)
        tf.add(north_of(kC, static_cast<double>(i) * p.stamp_spacing_m * 1.5));
    REQUIRE(tf.size() == static_cast<std::size_t>(p.max_stamps));

    // The very first stamp has long been evicted: its ground is untouched
    // again, EXACTLY.
    REQUIRE(tf.deform_at(first) == 0.0);
    // ...while the most recent track is present.
    const double last_s = 399.0 * p.stamp_spacing_m * 1.5;
    REQUIRE(tf.deform_at(north_of(kC, last_s)) < 0.0);
}

TEST_CASE("SF3-B: compaction is non-negative and confined to the groove",
          "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    REQUIRE(tf.add(kC));

    // A packed rut is not loose snow: reported sinkable depth drops by this.
    REQUIRE(tf.compaction_at(kC) > 0.0);
    REQUIRE(std::abs(tf.compaction_at(kC) - p.depress_m) < 1e-9);
    // Never negative anywhere -- depth_at subtracts it, so a negative value
    // would silently ADD sinkable depth beside the track.
    for (double s = 0.0; s <= 3.0; s += 0.021)
        REQUIRE(tf.compaction_at(east_of(kC, s)) >= 0.0);
    // Confined to the groove: the berm is raised ground, not packed ground.
    REQUIRE(tf.compaction_at(east_of(kC, p.half_width_m)) == 0.0);
}

// ---------------------------------------------------------------------------
// THE SEAM. The legs above prove TrackField is correct in isolation; these
// prove it is actually WIRED into the surfaces the game reads. This repo's own
// lessons call that the moved-consumer trap, and it has recurred five times:
// a mechanism that is right on its own and never reaches the shipped path.
// ---------------------------------------------------------------------------

namespace {

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

}  // namespace

TEST_CASE("SF3-B seam: an unbound track field is bit-identical", "[tracks]") {
    // The knob-off arm. Binding the mechanism must change NOTHING until the
    // machine has actually driven -- proven at the bit, not to a tolerance,
    // because every ground query in the game routes through these functions.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    world::TrackField tf;
    tf.reset(kR, world::TrackParams{});

    const double drive_unbound = f.drive_radius_at(kC);
    const double depth_unbound = f.depth_at(kC);
    const double geom_unbound = f.depth_geometry_at(kC);

    // Bound, but EMPTY: still bit-identical.
    f.tracks = &tf;
    REQUIRE(f.drive_radius_at(kC) == drive_unbound);
    REQUIRE(f.depth_at(kC) == depth_unbound);
    REQUIRE(f.depth_geometry_at(kC) == geom_unbound);

    // Bound and driven, but sampled far away: still bit-identical.
    REQUIRE(tf.add(kC));
    const glm::dvec3 far = east_of(kC, 500.0);
    f.tracks = nullptr;
    const double drive_far_unbound = f.drive_radius_at(far);
    f.tracks = &tf;
    REQUIRE(f.drive_radius_at(far) == drive_far_unbound);
}

TEST_CASE("SF3-B seam: the DRIVEN surface carries the rut", "[tracks]") {
    // The whole design rests on this: the patch draws drive_radius_at, so if
    // the rut is here it is drawn, and it is also what the sled's contact
    // patch rides. One surface, not two.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    const double before = f.drive_radius_at(kC);
    f.tracks = &tf;
    for (double s = -5.0; s <= 5.0; s += 0.05) tf.add(north_of(kC, s));
    const double after = f.drive_radius_at(kC);

    // Driving over it lowered the ground by the depression, to the millimetre.
    REQUIRE(after < before);
    REQUIRE(std::abs((before - after) - p.depress_m) < 1e-6);

    // And beside the groove the ground is RAISED -- the berm is on the driven
    // surface too, so it can be felt and not merely seen.
    const double berm_peak = p.half_width_m + 0.5 * p.berm_width_m;
    REQUIRE(f.drive_radius_at(east_of(kC, berm_peak)) > before);
}

TEST_CASE("SF3-B seam: a packed rut reports less sinkable depth", "[tracks]") {
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    f.tracks = &tf;

    const double loose = f.depth_at(kC);
    for (double s = -5.0; s <= 5.0; s += 0.05) tf.add(north_of(kC, s));
    const double packed = f.depth_at(kC);

    REQUIRE(packed < loose);
    // Never negative: depth_at feeds sinkage, and a negative sinkable depth
    // would be a hole in the physics, not a rut.
    REQUIRE(packed >= 0.0);
}

// ---------------------------------------------------------------------------
// R1 -- THE DRAWN FOLD (BLOCK-VP1). world/snowpack.cpp draw_fold_at is what the
// render mesh will carry per-vertex so the drawn world stops sitting ~0.77 m
// below the driven one. The legs that matter are the ones that keep it from
// becoming a SECOND SURFACE, which is the failure mode this whole thread keeps
// hitting.
// ---------------------------------------------------------------------------

TEST_CASE("R1 fold: with no linework the fold IS the ambient field", "[fold]") {
    // The fold must be a TERM OF the driven field, never a lookalike. With
    // nothing plowed, it is required to be bit-identical to ambient_depth_at --
    // if these two can ever disagree by an epsilon, the drawn and driven
    // surfaces have forked and the rung has recreated the defect it exists to
    // close.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    REQUIRE(f.draw_corridor_mask_at(kC) == 1.0);
    REQUIRE(f.draw_fold_at(kC) == f.ambient_depth_at(kC));

    for (double s = 0.0; s < 900.0; s += 137.0) {
        const glm::dvec3 d = east_of(kC, s);
        REQUIRE(f.draw_fold_at(d) == f.ambient_depth_at(d));
    }
}

TEST_CASE("R1 fold: the fold never touches the DRIVEN surface", "[fold]") {
    // THE LEG THAT PROVES R1 CANNOT CHANGE FEEL. [snowpack] hf_faceted_ground
    // is TRUE in the shipped config, so the sled reads the facet; the driven
    // surface already contains ambient. Adding the fold there as well would
    // double-count it into feel by ~0.7 m. Nothing in this rung may move
    // drive_radius_at, depth_at, or depth_geometry_at -- pinned here at the
    // bit.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    const double drive = f.drive_radius_at(kC);
    const double depth = f.depth_at(kC);
    const double geom = f.depth_geometry_at(kC);

    const double fold = f.draw_fold_at(kC);
    REQUIRE(fold > 0.0);  // non-vacuous: there IS snow here to fold

    REQUIRE(f.drive_radius_at(kC) == drive);
    REQUIRE(f.depth_at(kC) == depth);
    REQUIRE(f.depth_geometry_at(kC) == geom);
}

TEST_CASE("R1 fold: the corridor mask is a smooth ramp, not a vertex lottery",
          "[fold]") {
    // A 6 m corridor cannot be represented on a ~59 m mesh cell
    // (world/snowpack.h:343-346 says exactly this). A mask sharp enough to be
    // correct would be sampled by whichever vertices happened to land on the
    // road -- so roads would come out DASHED. The mask is therefore widened to
    // mesh scale and must be monotone and continuous across that run.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    world::LineNetwork net;
    // One straight run through the anchor, densified the way the baked linework
    // is, with a road-scale half width.
    std::vector<glm::dvec3> verts;
    std::vector<float> arcs;
    for (int i = 0; i <= 60; ++i) {
        const double s = -600.0 + 20.0 * i;
        const glm::dvec3 c = north_of(kC, s);
        // add_path takes the drawn ribbon's (L,R) vertex PAIRS; a 6 m road is
        // +/- 3 m about the centreline.
        verts.push_back(east_of(c, -3.0));
        verts.push_back(east_of(c, 3.0));
        arcs.push_back(static_cast<float>(s + 600.0));
        arcs.push_back(static_cast<float>(s + 600.0));
    }
    net.add_path(verts.data(), arcs.data(), verts.size(),
                 world::LineKind::RoadMinor, kR);
    net.build_index();
    REQUIRE_FALSE(net.empty());
    f.lines = &net;

    // On the deck: no fold at all. A plowed road is plowed.
    REQUIRE(f.draw_corridor_mask_at(kC) == 0.0);
    REQUIRE(f.draw_fold_at(kC) == 0.0);

    // Far out in open country: the full ambient field, exactly.
    const glm::dvec3 far = east_of(kC, 2.0 * f.p.draw_mask_m + 50.0);
    REQUIRE(f.draw_corridor_mask_at(far) == 1.0);
    REQUIRE(f.draw_fold_at(far) == f.ambient_depth_at(far));

    // Across the ramp: monotone non-decreasing, and it actually MOVES (a mask
    // stuck at 0 or 1 would pass a bounds check while representing nothing).
    double prev = -1.0;
    int rises = 0;
    for (double s = 0.0; s <= f.p.draw_mask_m + 20.0; s += 3.0) {
        const double m = f.draw_corridor_mask_at(east_of(kC, s));
        REQUIRE(m >= prev - 1e-12);
        if (m > prev + 1e-9) ++rises;
        prev = m;
    }
    REQUIRE(rises > 10);
}

// ---------------------------------------------------------------------------
// R3 -- THE PER-VERTEX SHADING DEPTH. R1 folded the field into the GEOMETRY;
// R3 makes the SAME field decide where snow SHOWS, retiring the planet FS's
// self-described "PROVISIONAL SNOW STUB" (a private slope mask plus a private
// barren shed, both older than the field and disagreeing with it -- exactly
// what WINTER_LAW 3.2/6c.1 forbids, "or the rock shows where the sled is still
// sinking").
//
// The legs here guard the two ways this rung can silently go wrong.
// ---------------------------------------------------------------------------

TEST_CASE("R3 sample: the fold channel is bit-identical to draw_fold_at",
          "[fold]") {
    // draw_fold_at is now a thin call onto draw_sample_at. If those two can
    // ever disagree by an epsilon then the GEOMETRY the mesh is built on and
    // the value the anti-fork rationale claims it shares have separated -- the
    // same failure mode R1 exists to close, one level up. Bit equality, not
    // approximate, because the mesh vertex radius is built from it.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    for (double s = 0.0; s < 900.0; s += 137.0) {
        const glm::dvec3 d = east_of(kC, s);
        REQUIRE(f.draw_sample_at(d).fold_m == f.draw_fold_at(d));
        REQUIRE(f.draw_sample_at(d).ambient_m == f.ambient_depth_at(d));
    }
}

TEST_CASE("R3 sample: the shading channel survives the corridor mask",
          "[fold]") {
    // ★ THE LEG THAT MATTERS. The corridor mask is ~60 m wide against a 6 m
    // road, and it is that wide for a MESH-RESOLUTION reason (a ~59 m cell
    // cannot dip into the deck) -- NOT because the world is bare out there.
    // The obvious implementation reuses draw_fold_at's early exits and hands
    // the shader a zero across that whole band, which paints a 2,213 km
    // bare-ground stripe down every road and trail on the map.
    //
    // So: ON THE DECK, where the fold is exactly 0, the ambient channel must
    // still report real snow. This fails the moment anyone "simplifies"
    // draw_sample_at by short-circuiting ambient behind the mask.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    world::LineNetwork net;
    std::vector<glm::dvec3> verts;
    std::vector<float> arcs;
    for (int i = 0; i <= 60; ++i) {
        const double s = -600.0 + 20.0 * i;
        const glm::dvec3 c = north_of(kC, s);
        verts.push_back(east_of(c, -3.0));
        verts.push_back(east_of(c, 3.0));
        arcs.push_back(static_cast<float>(s + 600.0));
        arcs.push_back(static_cast<float>(s + 600.0));
    }
    net.add_path(verts.data(), arcs.data(), verts.size(),
                 world::LineKind::RoadMinor, kR);
    net.build_index();
    f.lines = &net;

    // Non-vacuous: this really is a fully plowed deck.
    REQUIRE(f.draw_corridor_mask_at(kC) == 0.0);
    REQUIRE(f.draw_sample_at(kC).fold_m == 0.0);

    // ...and the shading channel is NOT zero there.
    const world::SnowpackField::DrawSample on_deck = f.draw_sample_at(kC);
    REQUIRE(on_deck.ambient_m > 0.0);
    REQUIRE(on_deck.ambient_m == f.ambient_depth_at(kC));

    // Across the whole masked band the shading channel must stay live, while
    // the fold ramps. Half the band is inside the mask by construction.
    int fold_zeroed = 0;
    for (double s = 0.0; s <= f.p.draw_mask_m; s += 5.0) {
        const glm::dvec3 d = east_of(kC, s);
        const world::SnowpackField::DrawSample ds = f.draw_sample_at(d);
        REQUIRE(ds.ambient_m > 0.0);
        if (ds.fold_m == 0.0) ++fold_zeroed;
    }
    REQUIRE(fold_zeroed > 0);  // the mask really did suppress the fold
}

TEST_CASE(
    "R3 mesh: the depth attribute is present with a fold and ABSENT "
    "without one",
    "[fold]") {
    // FaceMesh::depths empty must mean "no depth channel", never "zero snow
    // everywhere" -- a consumer that reads empty as zeros paints the whole
    // planet bare rock, which is a far worse artifact than the stub R3
    // replaces. planet.cpp's upload skips the buffer on empty and the draw
    // clamps the mix to 0; this pins the producer half of that contract.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    const render::FaceMesh with =
        render::fill_face(hf, 0, 9, 0, 0, 1, {}, 0, &f);
    const render::FaceMesh without =
        render::fill_face(hf, 0, 9, 0, 0, 1, {}, 0, nullptr);

    REQUIRE(without.depths.empty());
    REQUIRE(with.depths.size() == with.positions.size() / 3);

    // Non-vacuous, and it carries the FIELD -- not the fold, not a constant.
    int nonzero = 0;
    for (float d : with.depths)
        if (d > 0.0f) ++nonzero;
    REQUIRE(nonzero == static_cast<int>(with.depths.size()));
}

TEST_CASE("R3 mesh: the depth channel survives the CUT trim", "[fold]") {
    // The T25b subdividing cut trim EMITS NEW VERTICES for partially-swallowed
    // facets. If the depth channel does not grow with them it desyncs from
    // positions -- and upload_face drops a desynced channel WHOLE, so every
    // face carrying a tunnel mouth would render as bare ground the moment the
    // depth-keyed exposure is armed. A green build hides this completely: the
    // geometry is still correct and the default-off dial makes it invisible.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    // A cut disk centred on the face so the trim actually subdivides.
    std::vector<render::CutDisk> cuts;
    cuts.push_back({glm::normalize(render::face_basis(0).n), 400.0});

    const render::FaceMesh cut =
        render::fill_face(hf, 0, 17, 0, 0, 1, cuts, 2, &f);

    // Non-vacuous: the trim really did add vertices past the plain grid.
    REQUIRE(cut.positions.size() / 3 > 17u * 17u);
    // ...and the channel tracked every one of them.
    REQUIRE(cut.depths.size() == cut.positions.size() / 3);
    for (float d : cut.depths) REQUIRE(d > 0.0f);
}

// ---------------------------------------------------------------------------
// ★ R4 -- THE DEPTH-CARRYING TRACK. Chad ruled the floored law on 2026-08-27
// against a measured ladder (0.12 m invisible / 0.30 faint / 0.60 a legible
// trail / 1.50 a trench): the cut follows the snow it was laid in, but never
// falls below a readability floor, and never cuts deeper than the snowpack it
// is cut into. The legs a green build would otherwise hide:
//
//   R4-1. The law itself, at all three of its regimes (floored, proportional,
//         depth-bounded) AND at the crossover between them.
//   R4-2. The depression travels with the NEAREST STAMP, not blended across
//         neighbours -- a deep-bush rut beside a thin-snow one must keep two
//         different depths, or the seam invents a step nobody drove.
//   R4-3. A SECOND PASS MAY DEEPEN A RUT, NEVER FILL IT IN. This is the one
//         that reads as a bug from the seat: your own track healing under you.
//   R4-4. The berm rides on the stamp's own depression, so a deep cut gets a
//         proportionate shoulder instead of the SF3-B scratch.
//   R4-5. The depth-less overload is BIT-IDENTICAL to pre-R4. Every fixture
//         and the bench rig lay through it.
// ---------------------------------------------------------------------------

TEST_CASE("R4: the floored depth law, in all three regimes", "[tracks]") {
    world::TrackParams p;
    // The dials this leg is written against; if a retune moves them the
    // REQUIREs below re-derive from the params, never from these literals.
    REQUIRE(p.min_depress_m > 0.0);
    REQUIRE(p.depth_k > 0.0);
    REQUIRE(p.max_depth_frac < 1.0);

    // Bare ground cuts nothing. Not "small" -- exactly zero, so a plowed deck
    // is bit-identically untracked.
    REQUIRE(p.depress_for(0.0) == 0.0);
    REQUIRE(p.depress_for(-1.0) == 0.0);

    // THIN SNOW: the floor loses to the snowpack. The cut cannot be deeper
    // than the snow that is there -- this is the half of the ruling that
    // physics keeps, and the reason thin-snow readability has to come from
    // shading rather than from geometry.
    const double thin = 0.20;
    REQUIRE(std::abs(p.depress_for(thin) - thin * p.max_depth_frac) < 1e-12);
    REQUIRE(p.depress_for(thin) < p.min_depress_m);

    // THE BAND CHAD BOUGHT: deep enough to hold the floor, not deep enough for
    // the proportional law to have reached it. This is the bush he drives --
    // the whole point of the rung is that a track READS here.
    const double bush = 0.85;
    REQUIRE(bush * p.max_depth_frac > p.min_depress_m);  // floor is reachable
    REQUIRE(p.depth_k * bush < p.min_depress_m);         // ...and it binds
    REQUIRE(std::abs(p.depress_for(bush) - p.min_depress_m) < 1e-12);

    // DEEP: the proportional law overtakes the floor and the cut keeps growing
    // with the snow. Picked from the params so a retune cannot make this leg
    // silently vacuous.
    const double deep = 2.0 * p.min_depress_m / p.depth_k;
    REQUIRE(p.depth_k * deep > p.min_depress_m);
    REQUIRE(std::abs(p.depress_for(deep) - p.depth_k * deep) < 1e-12);

    // MONOTONE across the whole range: no retune may make more snow give a
    // shallower rut.
    double prev = -1.0;
    for (double d = 0.0; d <= 3.0; d += 0.01) {
        const double cur = p.depress_for(d);
        REQUIRE(cur >= prev - 1e-12);
        REQUIRE(cur <= d + 1e-12);  // never deeper than the snowpack
        prev = cur;
    }
}

TEST_CASE("R4: the cut depth travels with the nearest stamp", "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    // Two stamps, far enough apart that neither is inside the other's reach,
    // laid in very different snow.
    const double sep = 4.0 * (p.half_width_m + p.berm_width_m);
    const glm::dvec3 deep_at = kC;
    const glm::dvec3 thin_at = north_of(kC, sep);
    const double deep_snow = 3.0 * p.min_depress_m / p.depth_k;  // proportional
    const double thin_snow = 0.20;                               // depth-bound
    REQUIRE(tf.add(deep_at, deep_snow));
    REQUIRE(tf.add(thin_at, thin_snow));

    // Out of each other's reach, each stamp keeps ITS OWN depth exactly.
    REQUIRE(std::abs(tf.deform_at(deep_at) + p.depress_for(deep_snow)) < 1e-9);
    REQUIRE(std::abs(tf.deform_at(thin_at) + p.depress_for(thin_snow)) < 1e-9);
    // Non-vacuous: they really are different depths.
    REQUIRE(p.depress_for(deep_snow) > 2.0 * p.depress_for(thin_snow));
}

TEST_CASE("R4b: two passes of different depth meet WITHOUT a cliff",
          "[tracks]") {
    // ★ THE LEG THAT WOULD HAVE CAUGHT THE WINNER-TAKE-ALL STEP. R4 shipped
    // the amplitude on the same nearest-stamp winner as the shape, so where a
    // deep pass runs beside a thin one the depth flips at the Voronoi boundary
    // with both grooves still at full weight -- a step of half a metre in the
    // DRIVEN surface, which world/snowpack.h says stops a machine dead.
    //
    // The old fixture could not see it: it put its two stamps FOUR reaches
    // apart and sampled only at their centres, so any cross-stamp combination
    // law -- winner, mean, anything -- was a no-op there. This one puts them
    // inside each other's reach and walks the seam.
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    const double reach = p.half_width_m + p.berm_width_m;
    const double sep = 1.0;  // inside reach: the two profiles overlap
    REQUIRE(sep < reach);
    const double deep_snow = 3.0;
    const double thin_snow = 0.25;
    REQUIRE(p.depress_for(deep_snow) > 4.0 * p.depress_for(thin_snow));
    REQUIRE(tf.add(kC, deep_snow));
    REQUIRE(tf.add(east_of(kC, sep), thin_snow));

    // NO CLIFF -- and measured where ONLY the amplitude can move. The band
    // between the two stamps is inside 0.6*half_width of BOTH, so the groove
    // shape is saturated at 1.0 across all of it and contributes exactly
    // nothing to the difference between samples. (Walking the whole profile
    // instead would measure the groove WALL of a deep cut -- which is steep by
    // construction and has nothing to do with the seam. The first draft of
    // this leg made that mistake and failed honestly.)
    const double band_lo = 0.6 * p.half_width_m + 0.01;
    const double band_hi = sep - band_lo;
    REQUIRE(band_hi > band_lo);
    // ★ THE DISCRIMINATOR IS CONTINUITY, NOT A SLOPE BOUND. The transition
    // between two passes of very different depth is legitimately STEEP -- what
    // must not exist is a JUMP. So sample the band at two resolutions: a
    // continuous ramp's worst step shrinks with the sample spacing, and a
    // Voronoi step does NOT (refine forever, the jump stays the full depth
    // difference). A fixed slope bound would have been calibrated to today's
    // blend kernel and would false-fail an honest retune of it.
    auto worst_step = [&](double dx) {
        double worst = 0.0;
        double prev = tf.deform_at(east_of(kC, band_lo));
        for (double x = band_lo; x <= band_hi; x += dx) {
            const double cur = tf.deform_at(east_of(kC, x));
            worst = std::max(worst, std::abs(cur - prev));
            prev = cur;
        }
        return worst;
    };
    const double gap = p.depress_for(deep_snow) - p.depress_for(thin_snow);
    REQUIRE(gap > 0.5);
    const double coarse = worst_step(0.01);
    const double fine = worst_step(0.0025);
    // Non-vacuous: the transition really is happening in this band.
    REQUIRE(coarse > 0.01);
    // Continuous: 4x finer sampling gives a proportionally smaller step.
    REQUIRE(fine < 0.4 * coarse);
    // And nowhere near a cliff at either resolution.
    REQUIRE(coarse < 0.15 * gap);

    // ...and the blend is still NEAREST-DOMINANT, so a lone pass is not
    // diluted by a distant neighbour: 0.3 m from the deep stamp the cut is
    // still most of the deep stamp's own.
    const double near_deep = -tf.deform_at(east_of(kC, 0.3));
    REQUIRE(near_deep > 0.85 * p.depress_for(deep_snow));
}

TEST_CASE("R4b: a re-drive cannot fill in its own rut, and cannot chain",
          "[tracks]") {
    // ★ THE REGRESSION PIN FOR THE SESSION-WIDE RUNNING MAX. R4 shipped a
    // `d = max(d, nearest.depress)` guard so a re-drive could not heal a rut.
    // Consecutive stamps of ONE pass are always closer than half_width_m, so
    // the max chained stamp to stamp and never released: one transit of deep
    // bush and every later stamp -- on a road, a plowed deck, bare rock --
    // still cut 0.55 m, metres of track carved below the ground it was cut in.
    // The old leg for this ASSERTED the chaining, so it blessed the bug.
    //
    // The guard is gone. What replaces it is the INPUT: the caller passes the
    // UNDISTURBED depth (SnowpackField::track_lay_depth_at), so both halves
    // fall out for free and are pinned here.
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    const double bush = 0.85;  // the floor binds
    const double deck = 0.02;  // a plowed road: essentially nothing to cut
    REQUIRE(p.depress_for(bush) > 20.0 * p.depress_for(deck));

    // (a) A RE-DRIVE HOLDS THE RUT. The undisturbed depth is unchanged by the
    // pass that already went through, so the second pass re-lays the same cut.
    REQUIRE(tf.add(kC, bush));
    const double cut = tf.deform_at(kC);
    REQUIRE(std::abs(cut + p.depress_for(bush)) < 1e-9);
    REQUIRE(tf.add(north_of(kC, 1.2 * p.stamp_spacing_m), bush));
    REQUIRE(tf.deform_at(kC) <= cut + 1e-9);  // never filled in

    // (b) AND IT CANNOT CHAIN. Drive 30 m of deep bush, then out onto a deck.
    // Every deck stamp is within half_width_m of the one behind it, which is
    // exactly what let the old guard carry the bush depth out onto the road.
    world::TrackField tf2;
    tf2.reset(kR, p);
    double s = 0.0;
    for (; s <= 30.0; s += 0.05) tf2.add(north_of(kC, s), bush);
    const double first_deck = s;
    for (; s <= 60.0; s += 0.05) tf2.add(north_of(kC, s), deck);

    // On the deck the cut is the DECK's, not the bush's. Sampled well clear of
    // the transition so the blend from the last bush stamps has died away.
    const double on_deck = -tf2.deform_at(north_of(kC, first_deck + 10.0));
    REQUIRE(on_deck < 2.0 * p.depress_for(deck));
    REQUIRE(on_deck < 0.1 * p.depress_for(bush));
    // ...and the bush behind it still carries the full cut, so this is not a
    // vacuous "everything went shallow".
    REQUIRE(-tf2.deform_at(north_of(kC, 15.0)) > 0.95 * p.depress_for(bush));

    // (c) AN ISOLATED THIN STAMP KEEPS ITS OWN SHALLOW CUT -- it must not
    // inherit a neighbour's depth, nor the depth-less fallback dial.
    world::TrackField tf3;
    tf3.reset(kR, p);
    REQUIRE(tf3.add(kC, deck));
    REQUIRE(std::abs(tf3.deform_at(kC) + p.depress_for(deck)) < 1e-9);
    REQUIRE(p.depress_for(deck) != p.depress_m);
}

TEST_CASE("R4: the berm rides on the stamp's own depression", "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);

    const double deep_snow = 4.0 * p.min_depress_m / p.depth_k;
    REQUIRE(tf.add(kC, deep_snow));
    const double berm_peak = p.half_width_m + 0.5 * p.berm_width_m;
    const double berm = tf.deform_at(east_of(kC, berm_peak));
    REQUIRE(berm > 0.0);
    // The shoulder is the ruled FRACTION of this stamp's cut -- not the
    // SF3-B absolute, which on a 0.55 m rut would be a scratch beside a
    // trench. (The lobe is exactly 1.0 at the peak by construction.)
    REQUIRE(std::abs(berm - p.depress_for(deep_snow) * p.berm_frac) < 1e-9);
}

TEST_CASE("R4: the depth-less overload is bit-identical to pre-R4",
          "[tracks]") {
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    REQUIRE(tf.add(kC));
    // Every fixture and the bench rig lay through this overload; it must keep
    // using the fallback dial, not the new law, or the whole SF3-B suite would
    // be silently re-recorded against R4's floor.
    REQUIRE(tf.deform_at(kC) == -p.depress_m);
    REQUIRE(tf.compaction_at(kC) == p.depress_m);
    // Non-vacuous: the two paths really do disagree at bush depth.
    REQUIRE(p.depress_for(0.85) > 2.0 * p.depress_m);
}

// ---------------------------------------------------------------------------
// R5 ROW 3 -- THE TRACK READS. compaction_at existed, was unit-tested above,
// and NOTHING in the renderer consumed it (the ledger's finding). The patch
// now carries it, normalized, in texcoord.y -- these legs pin the seam and,
// above all, fence 5: zero compaction must be BIT-IDENTICAL, because the
// shader's mix() passes an exact 0 through exactly and this buffer is where
// that exact 0 is born.
// ---------------------------------------------------------------------------

#include <algorithm>

#include "render/snow_patch.h"

namespace {

// Build a complete patch anchored at kC over `f` and emit its buffers.
void build_patch(const render::SnowPatchParams& pp,
                 const world::SnowpackField& f, std::vector<float>& pos,
                 std::vector<float>& nrm, std::vector<float>& uv) {
    render::SnowPatchBuild b;
    render::snow_patch_begin(b, pp, kC, kR, &f);
    while (!b.complete())
        REQUIRE(render::snow_patch_step(b, pp, *f.hf, 200, 1, f) > 0);
    render::snow_patch_emit(b, pp, pos, nrm, uv);
}

}  // namespace

TEST_CASE("R5 row 3: zero compaction emits a bit-identical texcoord.y",
          "[tracks][snow_patch]") {
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;

    render::SnowPatchParams pp;
    pp.n_side = 32;

    // No TrackField bound at all: the pre-R5 patch.
    std::vector<float> pos0, nrm0, uv0;
    build_patch(pp, f, pos0, nrm0, uv0);

    // Bound but EMPTY field: every buffer must be IDENTICAL -- binding the
    // mechanism changes nothing until the machine has driven (fence 5).
    world::TrackField tf;
    tf.reset(kR, world::TrackParams{});
    f.tracks = &tf;
    std::vector<float> pos1, nrm1, uv1;
    build_patch(pp, f, pos1, nrm1, uv1);

    REQUIRE(uv0 == uv1);
    REQUIRE(pos0 == pos1);
    REQUIRE(nrm0 == nrm1);
    // And the channel itself is EXACTLY 0.0f everywhere -- not small. An
    // epsilon here would tint every square metre under the rider.
    for (std::size_t k = 0; k + 1 < uv0.size(); k += 2)
        REQUIRE(uv0[k + 1] == 0.0f);

    // Driven, but sampled OFF-track: still exactly 0.0f away from the cut.
    world::TrackParams p;
    for (double s = -6.0; s <= 6.0; s += 0.05) tf.add(north_of(kC, s));
    std::vector<float> pos2, nrm2, uv2;
    build_patch(pp, f, pos2, nrm2, uv2);
    // Node (2, 2) sits ~9.5 m from the centreline -- far outside the groove.
    const int n = pp.n_side;
    REQUIRE(uv2[(static_cast<std::size_t>(2) * n + 2) * 2 + 1] == 0.0f);
    // ...and the rut itself now CARRIES the signal.
    const std::size_t kc = (static_cast<std::size_t>(n / 2) * n + n / 2) * 2;
    REQUIRE(uv2[kc + 1] > 0.0f);
}

TEST_CASE("R5 row 3: texcoord.y is compaction over the readability floor",
          "[tracks][snow_patch]") {
    // The normalizer, pinned: compaction_at returns METRES (0 up to the
    // stamp's cut depth), and the channel is that over min_depress_m (the
    // Chad-signed 0.55 floor), clamped, times the rim weight -- computed here
    // through the SAME functions, never a re-derived constant.
    const world::HeightField hf = flat_field();
    world::SnowpackField f;
    f.hf = &hf;
    world::TrackField tf;
    world::TrackParams p;
    tf.reset(kR, p);
    f.tracks = &tf;
    for (double s = -6.0; s <= 6.0; s += 0.05) tf.add(north_of(kC, s));

    render::SnowPatchParams pp;
    pp.n_side = 32;
    std::vector<float> pos, nrm, uv;
    build_patch(pp, f, pos, nrm, uv);

    render::SnowPatchFrame frame = render::snow_patch_frame(kC);
    const int n = pp.n_side;
    const int ic = n / 2;
    const glm::dvec3 d = render::snow_patch_dir(frame, pp, ic, ic, kR);
    const double expect =
        std::min(1.0, std::max(0.0, tf.compaction_at(d) / p.min_depress_m)) *
        render::snow_patch_rim_weight(pp, ic, ic);
    const float got = uv[(static_cast<std::size_t>(ic) * n + ic) * 2 + 1];
    REQUIRE(got == static_cast<float>(expect));
    // The fallback-depth cut (0.12 m) reads WELL below saturation: the dial
    // has range to express a shallow pass against a floor-depth one.
    REQUIRE(got > 0.0f);
    REQUIRE(got < 0.5f);

    // A deep-bush cut (depress > the floor) CLAMPS to exactly 1.0 -- packed
    // is packed; the tint must not keep climbing off the top of the dial.
    world::TrackField tf2;
    tf2.reset(kR, p);
    f.tracks = &tf2;
    const double bush = 4.0 * p.min_depress_m / p.depth_k;
    for (double s = -6.0; s <= 6.0; s += 0.05) tf2.add(north_of(kC, s), bush);
    std::vector<float> pos2, nrm2, uv2;
    build_patch(pp, f, pos2, nrm2, uv2);
    REQUIRE(uv2[(static_cast<std::size_t>(ic) * n + ic) * 2 + 1] == 1.0f);
}
