#include "render/road_census.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <thread>

namespace render {

const std::vector<double>& road_census_lateral_fracs() {
    // The corridor edge is at |lateral| == half_w; 1.5x puts a sample out in
    // the bank/feather, 0.5x inside the driven lane.
    static const std::vector<double> f = {-1.5, -1.0, -0.5, 0.0,
                                          0.5,  1.0,  1.5};
    return f;
}

double census_pct(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t k =
        static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(k, v.size() - 1)];
}

double bank_crest_offset_m(const world::SnowpackField& snow, glm::dvec3 up,
                           glm::dvec3 perp, double half_w_m,
                           const std::vector<double>& rings) {
    const double R = snow.hf != nullptr ? snow.hf->R : 6'371'000.0;
    double best_off = half_w_m;
    double best_depth = -1e30;
    for (double r : rings) {
        const double off = half_w_m + r;
        const glm::dvec3 d = glm::normalize(up + perp * (off / R));
        const double depth = snow.depth_geometry_at(d);
        if (depth > best_depth) {
            best_depth = depth;
            best_off = off;
        }
    }
    return best_off;
}

void road_census_run(const BankRun& run, const world::SnowpackField& snow,
                     const RoadCensusHooks& h,
                     std::vector<RoadCensusRow>& out) {
    const std::size_t n = run.ctr.size();
    if (n < 2 || snow.hf == nullptr) return;
    const double R = snow.hf->R;
    // ★ ROAD-REPAIR (bank strips continuous + lit): DELIBERATELY the UNREFINED
    // ring set. bank_ring_offsets now takes a chord tolerance and the mesh
    // builds on the refined list (12 knots, not 7), but this list is only used
    // to pick the CREST LATERAL -- the offset each station's crest row is
    // sampled at. Refining it would move that lateral by centimetres and every
    // registration percentile in docs/road_repair/census_after_sink.md would
    // stop being comparable to this run, for no gain: the crest lateral is a
    // place to stand, not a vertex position. The metric that DOES need the
    // built ring set -- the drawn-bank continuity in app/main.cpp -- is
    // measured on the strips themselves, at the ring count they were built
    // with.
    const std::vector<double> rings = bank_ring_offsets(snow.p);
    const std::vector<double>& fracs = road_census_lateral_fracs();

    bool have_prev = false;
    double prev_hw = 0.0, prev_drive = 0.0;
    // ★ ROAD-REPAIR C0 CORNER BLEND: per-LATERAL history. The corner step
    // lives out at the shoulder and the crest, never on the centreline (the
    // query's winner there is always this run's own segment, so the queried
    // width IS the baked width). A single centreline history could not see it.
    std::vector<double> prev_q(fracs.size() + 1, 0.0);
    std::vector<double> prev_dr(fracs.size() + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        // The station frame, built exactly as build_bank_run builds it:
        // tangent from the neighbours IN THIS RUN, outward = tangent x up.
        const glm::dvec3 up = run.ctr[i];
        const std::size_t i0 = i > 0 ? i - 1 : i;
        const std::size_t i1 = i + 1 < n ? i + 1 : i;
        glm::dvec3 t = run.ctr[i1] - run.ctr[i0];
        t -= up * glm::dot(t, up);
        const double tl = glm::length(t);
        if (tl < 1e-12) continue;  // degenerate station (the bank skips it too)
        t /= tl;
        const glm::dvec3 perp = glm::normalize(glm::cross(t, up));
        const double hw = run.hw[i];

        // The lateral set: the seven fractions, then the MEASURED crest.
        std::vector<double> lats;
        lats.reserve(fracs.size() + 1);
        for (double f : fracs) lats.push_back(f * hw);
        const double crest =
            bank_crest_offset_m(snow, up, perp, hw, rings);
        lats.push_back(crest);

        // The centreline drive radius drives the station-to-station delta, so
        // it is computed first and reused for lateral 0.
        double centre_drive = 0.0;
        for (std::size_t li = 0; li < lats.size(); ++li) {
            const double lat = lats[li];
            const glm::dvec3 d = glm::normalize(up + perp * (lat / R));
            const world::SnowpackField::GroundSample g = snow.sample_at(d);
            const double facet = h.facet_r ? h.facet_r(d) : 0.0;
            const double drawn = h.drawn_r ? h.drawn_r(d) : 0.0;
            const double ribbon = drawn + h.ribbon_lift_m;
            // ★ ROAD-REPAIR: the bank column measures the ring
            // render::build_bank_run actually emits, so it must go through the
            // SAME registration floor that function does
            // (SnowpackField::deck_floor_r). Composing it off the raw facet
            // here after the repair would report a divergence the built mesh
            // does not have -- an instrument fork, which is the one thing a
            // ruler may never be.
            const double bank = snow.deck_floor_r(
                d, facet + h.bank_lift_m + snow.depth_geometry_at(d));
            if (std::fabs(lat) < 1e-9) centre_drive = g.drive_r;

            RoadCensusRow r;
            r.path_id = run.path_id;
            r.kind = run.kind;
            r.station_s = static_cast<double>(run.s[i]);
            r.lateral_m = lat;
            r.drive_minus_facet = g.drive_r - facet;
            r.ribbon_minus_drive = ribbon - g.drive_r;
            r.bank_minus_drive = bank - g.drive_r;
            r.drawn_minus_drive = drawn - g.drive_r;
            r.surf = static_cast<int>(g.surf);
            r.half_w_m = hw;
            r.drive_r = g.drive_r;
            r.dir = d;
            r.is_crest = (li + 1 == lats.size());
            r.is_centre = (!r.is_crest && li < fracs.size() &&
                           fracs[li] == 0.0);
            if (snow.lines != nullptr) {
                // The ruler asks the corridor query the way the FIELD asks
                // it -- same blend dial -- or it is measuring a surface the
                // sled does not drive on, which is the one thing a ruler may
                // never be (census_after_sink.md 6).
                const world::LineHit hit = snow.lines->nearest(
                    d, h.search_m, snow.p.corner_blend_m);
                r.junction_m = hit.junction_m;
                r.seg_id = hit.seg_a;
                r.q_half_w_m = hit.found ? hit.half_w_m : 0.0;
            }
            // ★ THE LOCAL GRADIENT, crest row only (see RoadCensusHooks
            // ::fine_m). The step is taken along the run TANGENT at this
            // station -- the direction the sled travels -- so what it grades
            // is the surface under a ski, not a cross-section.
            if (r.is_crest && h.fine_m > 0.0) {
                const double half = 0.5 * h.fine_m / R;
                const glm::dvec3 dp = glm::normalize(d + t * half);
                const glm::dvec3 dm = glm::normalize(d - t * half);
                r.crest_grade =
                    std::fabs(snow.sample_at(dp).drive_r -
                              snow.sample_at(dm).drive_r) / h.fine_m;
            }
            r.has_prev = have_prev;
            if (have_prev) {
                r.d_q_half_w_m = r.q_half_w_m - prev_q[li];
                r.d_drive_r_lat_m = g.drive_r - prev_dr[li];
            }
            prev_q[li] = r.q_half_w_m;
            prev_dr[li] = g.drive_r;
            out.push_back(r);
        }
        // ★ ROAD-REPAIR ONAPING RUNG 1 -- the two edge measurements, once per
        // STATION (see RoadCensusRow::skirt_drop_m / float_2x_m). Per station
        // and not per lateral on purpose: they are cross-section properties,
        // and eight copies of them would be eight times the cost for the same
        // eight numbers. Neither one calls LineNetwork::nearest -- sample_at
        // does its own single corridor lookup and the facet/drawn hooks do
        // none at all, so the sink fix's cost rule (one corridor lookup per
        // sample) is kept.
        //
        // The bank FOOT is half_w + the outermost ring of the SAME ring list
        // the crest lateral was picked from, never a re-typed
        // bank_rise_m + bank_fall_m: one list, one foot.
        const double foot_off = hw + (rings.empty() ? 0.0 : rings.back());
        const double skirt_off = foot_off + snow.p.deck_skirt_m;
        double skirt_drop = 0.0, f2 = 0.0, f3 = 0.0, f4 = 0.0;
        int skirt_side = 0, float_side = 0;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            const double sg = static_cast<double>(sgn);
            const glm::dvec3 d_foot =
                glm::normalize(up + perp * (sg * foot_off / R));
            const glm::dvec3 d_skirt =
                glm::normalize(up + perp * (sg * skirt_off / R));
            // POSITIVE = the ground falls away outward.
            const double drop = (h.facet_r ? h.facet_r(d_foot) : 0.0) -
                                (h.facet_r ? h.facet_r(d_skirt) : 0.0);
            if (skirt_side == 0 || drop > skirt_drop) {
                skirt_drop = drop;
                skirt_side = sgn;
            }
            double fl[3] = {0.0, 0.0, 0.0};
            for (int k = 0; k < 3; ++k) {
                const glm::dvec3 d =
                    glm::normalize(up + perp * (sg * (k + 2) * hw / R));
                fl[k] = (h.drawn_r ? h.drawn_r(d) : 0.0) -
                        snow.sample_at(d).drive_r;
            }
            // ONE side for the triple, chosen by the outermost sample: the 4x
            // lateral is the one the ranking is done on, so a cross-section
            // that reads worst there is the cross-section to report.
            if (float_side == 0 || fl[2] > f4) {
                f2 = fl[0];
                f3 = fl[1];
                f4 = fl[2];
                float_side = sgn;
            }
        }

        // ★ ROAD-REPAIR ONAPING RUNG 2 -- THE APRON RULER. On the SKIRT SIDE
        // only: that is the side the fall is on and the side the mesh builds
        // an apron on, and measuring the other side would average a shelf
        // with the cut bank opposite it.
        double gap_a = 0.0, gap_b = 0.0, gap_c = 0.0, reach = 0.0;
        if (skirt_side != 0 && h.facet_r && h.drawn_r) {
            const double sg = static_cast<double>(skirt_side);
            const glm::dvec3 out_dir = perp * sg;
            const double probe =
                h.apron_probe_m > 0.0 ? h.apron_probe_m : h.apron_m;
            reach = bank_apron_reach_m(
                snow, h.facet_r, h.drawn_r, up, out_dir, foot_off,
                snow.p.deck_skirt_m, h.apron_tol_m, h.apron_min_drop_m,
                h.apron_rings, probe);
            const double skirt_ring_off = foot_off + snow.p.deck_skirt_m;
            const double samp[3] = {0.0, 0.5 * h.apron_sample_m,
                                    h.apron_sample_m};
            double* dst[3] = {&gap_a, &gap_b, &gap_c};
            for (int k = 0; k < 3; ++k) {
                const double off = skirt_ring_off + samp[k];
                const glm::dvec3 d = glm::normalize(up + out_dir * (off / R));
                const double drive = snow.sample_at(d).drive_r;
                double seen = h.drawn_r(d);  // the planet mesh
                // ...unless the drawn APRON covers this offset, in which case
                // the eye sees the apron -- the apron rides drive_radius_at,
                // so what is drawn there IS the driven surface. Strictly
                // GREATER than the skirt ring: sample A sits ON the apron's
                // buried lip, which is under the terrain and therefore not
                // what the eye is shown, before or after.
                if (h.apron_m > 0.0 && reach > 0.0 &&
                    samp[k] > 1e-9 && samp[k] <= reach + 1e-9)
                    seen = std::max(seen, snow.drive_radius_at(d));
                *dst[k] = seen - drive;
            }
        }

        // Run-local C0 metrics, stamped on EVERY row of the station so a row
        // can be read on its own.
        const double d_hw = have_prev ? hw - prev_hw : 0.0;
        const double d_dr = have_prev ? centre_drive - prev_drive : 0.0;
        for (std::size_t k = out.size() - lats.size(); k < out.size(); ++k) {
            out[k].d_half_w_m = d_hw;
            out[k].d_drive_r_m = d_dr;
            out[k].skirt_drop_m = skirt_drop;
            out[k].skirt_side = skirt_side;
            out[k].float_2x_m = f2;
            out[k].float_3x_m = f3;
            out[k].float_4x_m = f4;
            out[k].float_side = float_side;
            out[k].apron_gap_a_m = gap_a;
            out[k].apron_gap_b_m = gap_b;
            out[k].apron_gap_c_m = gap_c;
            out[k].apron_reach_m = reach;
        }
        prev_hw = hw;
        prev_drive = centre_drive;
        have_prev = true;
    }
}

std::vector<RoadCensusRow> road_census(const std::vector<BankRun>& runs,
                                       const world::SnowpackField& snow,
                                       const RoadCensusHooks& h) {
    // The same static striping build_bank_strips uses: per-run buckets
    // concatenated in run order, so the output does not depend on scheduling.
    const unsigned hw_threads = std::thread::hardware_concurrency();
    const unsigned workers = std::max(
        1u, std::min<unsigned>(hw_threads > 2 ? hw_threads - 2 : 1u, 16u));
    std::vector<std::vector<RoadCensusRow>> bucket(runs.size());
    std::vector<std::thread> pool;
    for (unsigned w = 0; w < workers; ++w)
        pool.emplace_back([&, w]() {
            for (std::size_t ri = w; ri < runs.size(); ri += workers)
                road_census_run(runs[ri], snow, h, bucket[ri]);
        });
    for (std::thread& th : pool) th.join();
    std::vector<RoadCensusRow> out;
    std::size_t total = 0;
    for (const auto& b : bucket) total += b.size();
    out.reserve(total);
    for (auto& b : bucket)
        for (RoadCensusRow& r : b) out.push_back(r);
    return out;
}

RoadCensusStats road_census_stats(
    const std::vector<RoadCensusRow>& rows, const std::string& label,
    const std::function<bool(const RoadCensusRow&)>& keep,
    double snow_bank_max_grade) {
    RoadCensusStats s;
    s.label = label;
    std::vector<double> rib, bnk, dqw, grd;
    for (const RoadCensusRow& r : rows) {
        if (keep && !keep(r)) continue;
        ++s.rows;
        rib.push_back(std::fabs(r.ribbon_minus_drive));
        bnk.push_back(std::fabs(r.bank_minus_drive));
        if (r.is_centre) {
            ++s.centre_stations;
            // THE SINK: the drawn deck stands more than 10 cm ABOVE the
            // surface the body is placed on, so the body sits in the road.
            if (r.ribbon_minus_drive > 0.10) ++s.sink_stations;
            if (std::fabs(r.d_half_w_m) > 0.5) ++s.width_steps;
        }
        // ★ THE CORNER STEP, every lateral. `d_q_half_w_m` is exactly 0.0 on
        // the FIRST station of a run (no previous station), which is why the
        // eligibility count is carried beside the hit count instead of being
        // assumed equal to `rows`.
        if (r.is_crest) {
            grd.push_back(r.crest_grade);
            if (r.crest_grade > snow_bank_max_grade) ++s.grade_over_bound;
        }
        if (r.has_prev) {
            ++s.corner_rows;
            dqw.push_back(std::fabs(r.d_q_half_w_m));
            if (std::fabs(r.d_q_half_w_m) > 0.5 &&
                std::fabs(r.d_drive_r_lat_m) > kCornerDriveStepM)
                ++s.corner_steps;
        }
    }
    s.grade_p50 = census_pct(grd, 0.50);
    s.grade_p90 = census_pct(grd, 0.90);
    s.grade_p99 = census_pct(grd, 0.99);
    s.grade_max = grd.empty() ? 0.0 : *std::max_element(grd.begin(), grd.end());
    s.grade_rows = static_cast<long>(grd.size());
    s.d_q_half_w_p50 = census_pct(dqw, 0.50);
    s.d_q_half_w_p99 = census_pct(dqw, 0.99);
    s.d_q_half_w_max =
        dqw.empty() ? 0.0 : *std::max_element(dqw.begin(), dqw.end());
    s.stations = s.centre_stations;
    s.ribbon_p50 = census_pct(rib, 0.50);
    s.ribbon_p90 = census_pct(rib, 0.90);
    s.ribbon_p99 = census_pct(rib, 0.99);
    s.ribbon_max = rib.empty() ? 0.0 : *std::max_element(rib.begin(), rib.end());
    s.bank_p50 = census_pct(bnk, 0.50);
    s.bank_p90 = census_pct(bnk, 0.90);
    s.bank_p99 = census_pct(bnk, 0.99);
    s.bank_max = bnk.empty() ? 0.0 : *std::max_element(bnk.begin(), bnk.end());
    return s;
}

}  // namespace render
