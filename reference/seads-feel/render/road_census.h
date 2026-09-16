#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

#include "render/bank_mesh.h"   // BankRun / recover_bank_runs / ring offsets
#include "world/snowpack.h"

// ★ ROAD-REPAIR CENSUS -- THE MISSING INSTRUMENT (Chad, 2026-09-08: "the
// Sudburian and the snowmachine sink into the road; snowbank road sections are
// jagged; sparkle drops at road edges").
//
// Nothing in this tree could answer, as a NUMBER, how far the DRAWN road deck
// sits from the DRIVEN surface underneath it. Four surfaces meet at a road
// edge and each is composed differently:
//
//   drive_r       SnowpackField::sample_at().drive_r -- what the sled and the
//                 walker stand on: facet + corridor geometry depth + lift +
//                 hill + tracks.
//   ribbon_r      the drawn road deck: drawn_radius_at (the FOLDED facet --
//                 the planet mesh's own snow fold) + [ribbons] lift_m.
//   bank_r        the drawn snowbank vertex: facet_radius_at (the UNFOLDED
//                 terrain facet) + the same lift + depth_geometry_at.
//   drawn_planet  the planet mesh itself -- drawn_radius_at, whose corridor
//                 fold mask is a 60 m ramp that a ~59 m mesh cell cannot
//                 resolve.
//
// This module measures all four at the SAME stations the banks are built on
// (render::recover_bank_runs) and reports the differences. It BUILDS NOTHING
// and CHANGES NOTHING: it is the instrument, and the tests are the gate.
//
// ★ LAYERING. The two facet functions live in render/sphere_param and need the
// shipped [planet] subdiv/tiles, which only the app layer holds -- exactly the
// situation SnowpackField::facet_radius_fn already answers. So they reach this
// census the same way: INJECTED as std::function, never called by name from
// here. That also makes the core testable against an analytic ground.
//
// PURE: glm + std + render/bank_mesh (pure) + world/. ZERO raylib.

namespace render {

// The four injected surfaces + the two lift dials, single-sourced by the
// caller from the SAME [ribbons] lift_m the drape and the banks use (F8).
struct RoadCensusHooks {
    std::function<double(glm::dvec3)> facet_r;  // render::facet_radius_at
    std::function<double(glm::dvec3)> drawn_r;  // render::drawn_radius_at
    double ribbon_lift_m = 0.45;
    double bank_lift_m = 0.45;
    double search_m = 400.0;  // corridor lookup radius for the row's own hit
    // ★ ROAD-REPAIR C0 CORNER BLEND -- THE FINE BASELINE. A 16 m station
    // spacing CANNOT see C0: a continuous surface sampled 16 m apart still
    // reports a metre of change, and a discontinuous one reports the same
    // metre. So the crest row also measures the LOCAL gradient, by a central
    // difference ALONG the run over this distance -- the direction the machine
    // travels, at a spacing a ski actually spans. 0.5 m is ~the contact patch.
    double fine_m = 0.5;
    // ★ ROAD-REPAIR ONAPING RUNG 2 -- THE APRON RULER. Picked BEFORE the cut
    // (the corner rung's lesson: a step COUNT rose while the magnitude fell,
    // because the ruler could not see what was being fixed).
    //
    //   apron_m          the SHIPPED [bank_mesh] apron_m -- the COVERAGE cap.
    //                    0 means "no apron is drawn", so the drawn surface out
    //                    there is the planet mesh and `apron_gap_m` is the
    //                    BEFORE measurement. SEADS_NO_APRON=1 zeroes it, which
    //                    is exactly how the before/after pair is taken with
    //                    one exe.
    //   apron_probe_m    the cap the REACH COLUMN is measured with. Normally
    //                    == apron_m; when apron_m is 0 it is still the shipped
    //                    number, so the before run reports the reach the after
    //                    run will build on and the two tables line up.
    //   apron_sample_m   the lateral distance the three gap samples span. It
    //                    is NEVER zeroed by the kill -- the before and the
    //                    after have to be measured at the SAME three offsets
    //                    or the table is two different rulers.
    double apron_m = 0.0;
    double apron_probe_m = 0.0;
    double apron_sample_m = 0.0;
    double apron_tol_m = 0.25;
    double apron_min_drop_m = 0.5;
    int apron_rings = 0;
    double skirt_bury_m = 0.5;
};

// One sample: one station, one lateral offset.
struct RoadCensusRow {
    int path_id = -1;
    int kind = 0;
    double station_s = 0.0;  // arc length along the run (m)
    double lateral_m = 0.0;  // signed offset across the corridor (m)
    double drive_minus_facet = 0.0;
    double ribbon_minus_drive = 0.0;
    double bank_minus_drive = 0.0;
    double drawn_minus_drive = 0.0;
    int surf = 0;             // world::Surface
    double half_w_m = 0.0;    // the station's own recovered half-width
    double junction_m = 0.0;  // from the row's own LineNetwork::nearest
    std::int64_t seg_id = -1;
    double d_half_w_m = 0.0;  // station-to-station delta, run-local (C0 jag)
    double d_drive_r_m = 0.0;  // ditto, at the CENTRELINE
    // ★ ROAD-REPAIR C0 CORNER BLEND: the half-width the CORRIDOR QUERY
    // returns at THIS row's own direction -- LineNetwork::nearest's
    // half_w_m, i.e. what corridor_eval actually feeds bank_profile as
    // `e = dist - half_w`. It is NOT `half_w_m` above: that one is the
    // station's own baked width, recovered off the drawn ribbon pair, and
    // it is a property of the BAKE along one run. The queried width is a
    // property of the FIELD at a point, and at a corner or a crossing it
    // steps when the nearest segment flips to the other road. On the
    // centreline the two agree by construction (the query's winner is this
    // run); out at the shoulder and the crest -- where the bank lives, and
    // where the eye sees the jag -- they part company. That difference is
    // the defect this round closes, so the census had to be able to name it.
    double q_half_w_m = 0.0;
    // Station-to-station deltas AT THIS LATERAL (not stamped from the
    // centreline): the queried width step and the driven-radius step that
    // together define a CORNER STEP.
    double d_q_half_w_m = 0.0;
    double d_drive_r_lat_m = 0.0;
    double drive_r = 0.0;
    // ★ The LOCAL longitudinal gradient |d(drive_r)/ds| at this row, by a
    // central difference along the run over RoadCensusHooks::fine_m. Measured
    // on the CREST row only (0 elsewhere): the crest is the lateral the jag is
    // seen at, and two extra sample_at per station is the cost of seeing it.
    double crest_grade = 0.0;
    // ★ ROAD-REPAIR ONAPING RUNG 1 -- THE TWO EDGE MEASUREMENTS. Both are
    // per-STATION (not per-lateral): they are stamped on every row of the
    // station exactly as d_half_w_m is, so a row can be ranked on its own.
    //
    // skirt_drop_m -- THE SKIRT CREASE / OUTER BANK FALL (handoff §3 suspect
    // 1). The TERRAIN FACET at the bank FOOT (half_w + the outermost bank ring
    // offset, i.e. where the drawn strip stops being bank and starts being
    // burial skirt) MINUS the terrain facet at the OUTER SKIRT ring (that foot
    // plus [snowpack] deck_skirt_m, which mirrors [bank_mesh] skirt_m at the
    // config boundary). Signed so POSITIVE == THE GROUND FALLS AWAY OUTWARD
    // -- the case the complaint is about: the eye reads a finished bank, the
    // machine leaves it onto a facet that is already lower. (That is the
    // NEGATION of the literal expression in the handoff prose; the sign rule
    // is the operative one and the column is named for the fall.) Measured on
    // BOTH sides; the WORSE (larger positive) side is emitted and named.
    //
    // It is FACET, never drive_r, on purpose: this is the terrain the fold and
    // the corridor are draped on, so it cannot be confused with a corridor
    // effect the earlier rungs already closed.
    double skirt_drop_m = 0.0;
    int skirt_side = 0;  // -1 / +1: which side skirt_drop_m was measured on
    // float_{2,3,4}x_m -- THE DRAWN-VS-DRIVEN FLOAT BEYOND THE FLOOR (handoff
    // §3 suspect 2). `drawn_planet_r - drive_r` at 2x / 3x / 4x half_w off the
    // centreline: past bank_rise+bank_fall (9 m) plus the skirt (6 m) the deck
    // floor has faded and the driven surface returns to the UNFOLDED facet
    // while the eye still sees the FOLDED planet mesh. Positive == the drawn
    // ground stands ABOVE what the machine is placed on -- a drop you cannot
    // see, by construction. All three are reported from ONE side, chosen by
    // the worse (larger positive) float_4x_m, so the triple reads as one
    // cross-section and not as three unrelated maxima.
    double float_2x_m = 0.0;
    double float_3x_m = 0.0;
    double float_4x_m = 0.0;
    int float_side = 0;  // -1 / +1
    // ★ ROAD-REPAIR ONAPING RUNG 2 -- THE APRON RULER, per STATION, measured
    // on the SKIRT_SIDE (the side the fall is on, i.e. the side that gets the
    // apron). Three samples, at the outer skirt ring and at skirt +
    // apron_sample_m/2 and skirt + apron_sample_m:
    //
    //   apron_gap_*_m = WHAT THE EYE IS SHOWN  minus  WHAT THE MACHINE STANDS
    //                   ON, i.e. max(drawn planet mesh, the drawn strip/apron
    //                   where one covers this offset) - drive_r.
    //
    // NEGATIVE == the drawn ground is BELOW the surface you ride on: you are
    // told the hill is lower than it is, which is the Onaping complaint's
    // shape. POSITIVE == the drawn ground stands above you (the sink shape,
    // closed on the deck by the sink-fix rung). The headline is |gap|.
    //
    // Sample A is at the outer skirt ring, where the strip is BURIED by
    // construction both before and after -- it is the CONTROL: the apron must
    // not move it.
    double apron_gap_a_m = 0.0;
    double apron_gap_b_m = 0.0;
    double apron_gap_c_m = 0.0;
    // The apron's reach at this station on the skirt side (0 == the station
    // does not qualify: its terrain drop across the skirt is under
    // apron_min_drop_m). Measured through render::bank_apron_reach_m -- the
    // SAME function the mesh builds on, so the ruler cannot describe an apron
    // the mesh does not build.
    double apron_reach_m = 0.0;
    // This station has a predecessor in its run, so its deltas are real.
    bool has_prev = false;
    glm::dvec3 dir{0.0, 0.0, 1.0};
    bool is_crest = false;   // this lateral is the measured bank crest
    // ★ The centreline row, flagged rather than inferred from
    // lateral_m == 0: a station whose recovered half-width is ~0 puts
    // EVERY fractional lateral on the centreline, and a station count
    // that reads `lateral_m == 0` would then count that station seven
    // times. (It does: the first run of this census reported 269,203
    // centre rows over 265,515 stations.)
    bool is_centre = false;
};

// The lateral fractions of half_w every station is sampled at, in order. The
// bank crest is appended after these (its offset is measured, not a fraction).
const std::vector<double>& road_census_lateral_fracs();

// Percentile of an UNSORTED copy (nearest-rank, q in [0,1]). Empty -> 0.
double census_pct(std::vector<double> v, double q);

// The bank crest offset at one station: half_w plus whichever bank ring offset
// carries the greatest drawn geometry depth. Measured off the field exactly as
// build_bank_run measures its own amplitude, never re-derived from the dials.
double bank_crest_offset_m(const world::SnowpackField& snow, glm::dvec3 up,
                           glm::dvec3 perp, double half_w_m,
                           const std::vector<double>& rings);

// Walk one run. Appended to `out`. Pure; safe to shard across threads.
void road_census_run(const BankRun& run, const world::SnowpackField& snow,
                     const RoadCensusHooks& h, std::vector<RoadCensusRow>& out);

// Walk every run, sharded across workers, deterministic in run order.
std::vector<RoadCensusRow> road_census(const std::vector<BankRun>& runs,
                                       const world::SnowpackField& snow,
                                       const RoadCensusHooks& h);

// --- the summary -------------------------------------------------------
struct RoadCensusStats {
    std::string label;
    long rows = 0;
    long stations = 0;
    double ribbon_p50 = 0, ribbon_p90 = 0, ribbon_p99 = 0, ribbon_max = 0;
    double bank_p50 = 0, bank_p90 = 0, bank_p99 = 0, bank_max = 0;
    // THE SINK: centreline stations where the driven surface sits more than
    // 0.10 m BELOW the drawn deck -- i.e. the sled/walker stands inside the
    // road you can see.
    long sink_stations = 0;
    long centre_stations = 0;
    // CORNER STEPS: |half_w[i] - half_w[i-1]| > 0.5 m between consecutive
    // stations -- the C0 width jump the bank strip turns into a jag.
    // ⚠ This one is the BAKED width along a run. The corridor blend cannot
    // move it and does not claim to: it is the bake's own width sequence.
    long width_steps = 0;
    // ★ ROAD-REPAIR C0 CORNER BLEND -- THE CORNER STEP. A row (any lateral,
    // not just the centreline) where BOTH |d_q_half_w_m| > 0.5 m and
    // |d_drive_r_lat_m| > kCornerDriveStepM: the corridor query handed this
    // lateral a different road's width than it handed the station before it,
    // AND the surface under the machine moved when it did. Width alone is a
    // bookkeeping change; width AND radius together is the jag.
    long corner_steps = 0;
    long corner_rows = 0;  // rows eligible (have a previous station)
    double d_q_half_w_p50 = 0, d_q_half_w_p99 = 0, d_q_half_w_max = 0;
    // ★ THE C0 METRIC THAT CAN ACTUALLY SEE C0 (crest rows only): the local
    // |d(drive_r)/ds| along the road, and how many of those exceed the law's
    // own ramp bound (§2.4c bank_max_grade) -- i.e. how many crest samples the
    // machine would read as a STEP rather than as a ramp.
    double grade_p50 = 0, grade_p90 = 0, grade_p99 = 0, grade_max = 0;
    long grade_rows = 0;
    long grade_over_bound = 0;
};

// The driven-radius move that makes a width flip a STEP the machine feels.
// 0.10 m is the same 10 cm the SINK metric is cut at -- one threshold for
// "the surface moved", used twice, rather than a second number.
inline constexpr double kCornerDriveStepM = 0.10;

// Summarize a row subset selected by `keep` (null == all).
// `snow_bank_max_grade` is §2.4c's ruled ramp bound, passed in rather than
// re-typed: it is what separates "a ramp the machine launches off" from "a
// step that stops it dead", and the count of crest rows above it is the
// headline C0 number.
RoadCensusStats road_census_stats(
    const std::vector<RoadCensusRow>& rows, const std::string& label,
    const std::function<bool(const RoadCensusRow&)>& keep,
    double snow_bank_max_grade = 1.20);

}  // namespace render
