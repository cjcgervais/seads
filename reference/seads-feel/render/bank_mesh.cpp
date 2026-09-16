#include "render/bank_mesh.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <thread>

#include "render/sudbury_gis.gen.h"

// SF2-BANKS geometry (WINTER_LAW §3.6c). See bank_mesh.h for the law; every
// design decision in here cites its red-team finding (F1..F9, the SF2 spec).
//
// ★ ROAD-REPAIR (2026-09-09, "the snowbank road sections are jagged and the
// sparkle drops out at every road edge"). Four changes, all of them geometry
// or the normals that light it; the FIELD the strip samples is untouched:
//
//   R1  A station culled by min_amp_m used to end the strip with an OPEN END
//       -- a vertical white wall up to bank_height_m tall at every junction
//       approach, because the last EMITTED station still carried a full bank.
//       The culled station is now EMITTED as a taper CAP (its own real rings,
//       whose amplitude is below min_amp_m by definition of the cull) and the
//       strip breaks after it; the strip that resumes on the far side of the
//       gap OPENS on the same cap. Drawn still == driven -- the cap's vertices
//       are composed by the identical anti-fork line every other vertex is --
//       so the junction hole is still a real hole, it just no longer has a
//       wall standing at its lip.
//   R2  The skirt is drawn as `skirt_rings` sub-rings and carries the strip's
//       EXCESS over the terrain facet (lift + drawn depth) down to
//       -skirt_bury_m on a smoothstep. One quad made that fall a CREASE; the
//       smoothstep's zero end-slope makes the join to the outer bank ring C1
//       by construction, and the OUTERMOST skirt ring still lands exactly on
//       `facet - skirt_bury_m` (F3 intact: it dives under the drawn terrain).
//   R3  Per-vertex NORMALS. The transverse slope comes from the ANALYTIC
//       derivative of the same field the sled drives
//       (SnowpackField::depth_geometry_slope_at, whose bank term is
//       bank_profile_slope) plus the facet's own chord; the longitudinal slope
//       is a central difference ALONG THE RUN, taken before the strip is cut
//       into chunks, so a chunk seam and a left/right pair cannot disagree.
//   R4  The ring set is refined until its chord error is under chord_tol_m.
//       The shipped 7 knots chorded the C1 section by 0.178 m.

namespace render {

namespace {

inline double ss01(double x) {
    const double t = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
    return t * t * (3.0 - 2.0 * t);
}
inline double dss01(double x) {
    if (x <= 0.0 || x >= 1.0) return 0.0;
    return 6.0 * x * (1.0 - x);
}

// One station of one side of one run, fully evaluated. `state`:
//   0 ABSENT  -- inside a cut, or degenerate: emits nothing and BREAKS.
//   1 CAP     -- amplitude below min_amp_m: emitted only as a taper cap.
//   2 FULL    -- a real bank station.
struct Station {
    int state = 0;
    float u = 0.0f;
    double s = 0.0;
    glm::dvec3 t{0.0};    // unit road tangent
    glm::dvec3 out{0.0};  // unit outward tangential (side-signed)
    std::vector<glm::dvec3> d;      // nr ring dirs
    std::vector<double> r;          // nr ring radials
    std::vector<double> dh_de;      // nr d(excess)/d(offset), ANALYTIC
    std::vector<double> facet;      // nr terrain facet radii
    std::vector<glm::vec3> n;       // nr unit normals (phase B)
    // ★ ROAD-REPAIR RUNG 2 -- the APRON, a strip of its own (see the header).
    // `areach` is 0 on a station that does not qualify, which also BREAKS the
    // apron strip there: an apron is a local piece of geometry over a local
    // shelf, not a second ribbon down the whole road.
    double areach = 0.0;
    std::vector<glm::dvec3> ad;
    std::vector<double> aoff;
    std::vector<double> ar;
    std::vector<double> afacet;
    std::vector<glm::vec3> an;
};

// A strip being accumulated: one side of one (possibly cut-, cap- or
// chunk-split) stretch of a run.
struct StripAccum {
    std::vector<glm::dvec3> dirs;
    std::vector<double> radial;
    std::vector<glm::vec3> nrm;
    std::vector<float> u;
    int stations = 0;
};

void flush(StripAccum& a, int nr, const std::vector<float>& ring_v,
           std::vector<BankStripCPU>& out, long* verts, bool apron = false) {
    if (a.stations >= 2) {
        BankStripCPU s;
        s.rings = nr;
        s.is_apron = apron;
        s.pos.reserve(static_cast<std::size_t>(a.stations) * nr * 3);
        s.nrm.reserve(static_cast<std::size_t>(a.stations) * nr * 3);
        s.uv.reserve(static_cast<std::size_t>(a.stations) * nr * 2);
        for (int i = 0; i < a.stations; ++i)
            for (int r = 0; r < nr; ++r) {
                const std::size_t k =
                    static_cast<std::size_t>(i) * nr + static_cast<std::size_t>(r);
                const glm::dvec3 p = a.dirs[k] * a.radial[k];
                s.pos.push_back(static_cast<float>(p.x));
                s.pos.push_back(static_cast<float>(p.y));
                s.pos.push_back(static_cast<float>(p.z));
                s.nrm.push_back(a.nrm[k].x);
                s.nrm.push_back(a.nrm[k].y);
                s.nrm.push_back(a.nrm[k].z);
                s.uv.push_back(a.u[static_cast<std::size_t>(i)]);
                // v: ring offset normalized to the bank span; the skirt rings
                // ride 1.0 -> 1.25 so the FS can fade the speckle out over
                // them (and so the gate can tell a skirt ring by its tag).
                s.uv.push_back(ring_v[static_cast<std::size_t>(r)]);
            }
        for (int i = 0; i + 1 < a.stations; ++i)
            for (int r = 0; r + 1 < nr; ++r) {
                const auto v00 = static_cast<std::uint16_t>(i * nr + r);
                const auto v01 = static_cast<std::uint16_t>(i * nr + r + 1);
                const auto v10 = static_cast<std::uint16_t>((i + 1) * nr + r);
                const auto v11 =
                    static_cast<std::uint16_t>((i + 1) * nr + r + 1);
                s.idx.insert(s.idx.end(), {v00, v01, v11, v00, v11, v10});
            }
        if (verts != nullptr)
            *verts += static_cast<long>(a.stations) * nr;
        out.push_back(std::move(s));
    }
    a.dirs.clear();
    a.radial.clear();
    a.nrm.clear();
    a.u.clear();
    a.stations = 0;
}

void push_station(StripAccum& a, const Station& st, int nr) {
    for (int r = 0; r < nr; ++r) {
        a.dirs.push_back(st.d[static_cast<std::size_t>(r)]);
        a.radial.push_back(st.r[static_cast<std::size_t>(r)]);
        a.nrm.push_back(st.n[static_cast<std::size_t>(r)]);
    }
    a.u.push_back(st.u);
    ++a.stations;
}

// ★ ROAD-REPAIR RUNG 2: the same, for the apron's own ring column.
void push_apron_station(StripAccum& a, const Station& st, int nra) {
    for (int r = 0; r < nra; ++r) {
        a.dirs.push_back(st.ad[static_cast<std::size_t>(r)]);
        a.radial.push_back(st.ar[static_cast<std::size_t>(r)]);
        a.nrm.push_back(st.an[static_cast<std::size_t>(r)]);
    }
    a.u.push_back(st.u);
    ++a.stations;
}

}  // namespace

std::vector<double> bank_ring_offsets(const world::SnowParams& sp,
                                      double chord_tol_m, int max_rings) {
    // Red-team F2: the drawn depth beyond the corridor edge is
    //   lerp(inside, ambient, ss(e/corridor_edge)) + bank_profile(e) * gap,
    // so the section needs BOTH families of knots -- the bank's (rise, fall)
    // and the feather's (corridor_edge) -- or the chords clip the true crest
    // (~0.16 m at the shipped dials, where the crest sits near e = 3.5, not
    // at rise = 3).
    const double rise = sp.bank_rise_m, fall = sp.bank_fall_m;
    const double ce = sp.corridor_edge_m, end = rise + fall;
    std::vector<double> e = {0.0,        rise * 0.5,      rise,
                             (rise + ce) * 0.5, ce,       (std::max(rise, ce) + end) * 0.5,
                             end};
    std::sort(e.begin(), e.end());
    e.erase(std::unique(e.begin(), e.end(),
                        [](double a, double b) { return std::abs(a - b) < 1e-6; }),
            e.end());
    e.erase(std::remove_if(e.begin(), e.end(),
                           [end](double v) { return v < 0.0 || v > end + 1e-9; }),
            e.end());
    if (chord_tol_m <= 0.0) return e;

    // ★ ROAD-REPAIR R4. The composed section, in METRES: the feather (its
    // amplitude taken from the shipped base_m, since the ring offsets are ONE
    // list for the whole map and the per-station ambient is not) plus the
    // bank. bank_profile is asked of a scratch field so the curve refined
    // against is literally the driven one, never a second copy of the formula.
    world::SnowpackField probe;
    probe.p = sp;
    const double feather_amp = std::max(0.0, sp.base_m - sp.road_bare_m);
    const double ce_safe = std::max(1e-6, ce);
    auto h = [&](double x) {
        return feather_amp * ss01(x / ce_safe) + probe.bank_profile(x);
    };
    auto chord_err = [&](double a, double b) {
        const double ha = h(a), hb = h(b);
        double worst = 0.0;
        for (int k = 1; k < 8; ++k) {
            const double x = a + (b - a) * k / 8.0;
            const double chord = ha + (hb - ha) * (x - a) / (b - a);
            worst = std::max(worst, std::abs(h(x) - chord));
        }
        return worst;
    };
    while (static_cast<int>(e.size()) < max_rings) {
        double worst = -1.0;
        std::size_t wi = 0;
        for (std::size_t i = 0; i + 1 < e.size(); ++i) {
            const double er = chord_err(e[i], e[i + 1]);
            if (er > worst) {
                worst = er;
                wi = i;
            }
        }
        if (worst <= chord_tol_m) break;
        e.insert(e.begin() + static_cast<long long>(wi) + 1,
                 0.5 * (e[wi] + e[wi + 1]));
    }
    return e;
}

int bank_apron_ring_count(const BankBuildParams& p) {
    // The apron rides the SKIRT'S OWN ring spacing (skirt_m / skirt_rings), so
    // an apron quad is the size of a skirt quad and the two cannot disagree
    // about how finely this strip resolves a fall. Derived, never a dial.
    if (p.apron_m <= 0.0) return 0;
    const double spacing =
        p.skirt_m / static_cast<double>(std::max(1, p.skirt_rings));
    const int n = static_cast<int>(
        std::ceil(p.apron_m / std::max(1e-6, spacing) - 1e-9));
    return std::max(1, std::min(std::max(1, p.max_apron_rings), n));
}

double bank_apron_reach_m(const world::SnowpackField& snow,
                          const std::function<double(glm::dvec3)>& facet_r,
                          const std::function<double(glm::dvec3)>& drawn_r,
                          glm::dvec3 up, glm::dvec3 out_dir, double foot_off,
                          double skirt_m, double apron_tol_m,
                          double apron_min_drop_m, int apron_rings,
                          double cap_m) {
    if (cap_m <= 0.0 || apron_rings <= 0 || !facet_r || !drawn_r) return 0.0;
    if (snow.hf == nullptr) return 0.0;
    const double R = snow.hf->R;
    if (R <= 0.0) return 0.0;
    // ★ THE QUALIFICATION, and it is rung 1's own measurement: the TERRAIN
    // FACET at the bank foot minus the terrain facet at the outer skirt ring,
    // POSITIVE == the ground falls away outward. Same expression, same sign
    // convention, same two offsets as RoadCensusRow::skirt_drop_m -- except
    // that this is ONE side (the census emits the worse of the two), because
    // the apron is a per-side piece of geometry and the far side of a road cut
    // into a hillside is a bank, not a shelf.
    const glm::dvec3 d_foot = glm::normalize(up + out_dir * (foot_off / R));
    const glm::dvec3 d_skirt =
        glm::normalize(up + out_dir * ((foot_off + skirt_m) / R));
    const double drop = facet_r(d_foot) - facet_r(d_skirt);
    if (drop < apron_min_drop_m) return 0.0;
    // Walk outward one ring at a time and stop at the FIRST ring where what
    // the eye is shown (the planet mesh) and what the machine stands on
    // (drive_radius_at) already agree within the tolerance -- past that point
    // the mesh is telling the truth and more drawn geometry would be bling.
    // The reach is quantized to the ring grid because that is the only reach
    // the mesh can actually express.
    for (int k = 1; k <= apron_rings; ++k) {
        const double reach = cap_m * k / apron_rings;
        const glm::dvec3 d =
            glm::normalize(up + out_dir * ((foot_off + skirt_m + reach) / R));
        if (std::abs(drawn_r(d) - snow.drive_radius_at(d)) <= apron_tol_m)
            return reach;
    }
    return cap_m;
}

long build_bank_run(const std::vector<glm::dvec3>& ctr,
                    const std::vector<float>& s_arc,
                    const std::vector<double>& half_w, const HeightField& hf,
                    int subdiv, int tiles, const world::SnowpackField& snow,
                    const BankBuildParams& p,
                    const std::vector<CutDisk>& cuts,
                    std::vector<BankStripCPU>& out) {
    long verts = 0;
    const std::size_t n = ctr.size();
    if (n < 2 || hf.R <= 0.0) return 0;
    const std::vector<double> rings =
        bank_ring_offsets(snow.p, p.chord_tol_m, p.max_rings);
    const double end = snow.p.bank_rise_m + snow.p.bank_fall_m;
    const int nb = static_cast<int>(rings.size());          // bank rings
    const int nsk = std::max(1, p.skirt_rings);             // skirt sub-rings
    const int nr = nb + nsk;
    // ★ ROAD-REPAIR: the u16 ceiling is derived from the LIVE ring count, not
    // assumed to be 8. At 12 knots + 3 skirt rings the shipped literal 8000
    // would have emitted 120,000 vertices into a uint16_t index.
    const int cap_stations =
        std::max(2, std::min(p.max_stations_per_mesh, 65535 / std::max(1, nr)));

    // The per-ring v tag, once: bank rings normalize to the bank span, the
    // skirt rings ride 1.0 -> 1.25 (the outermost keeps the shipped 1.25 tag).
    std::vector<float> ring_v(static_cast<std::size_t>(nr));
    std::vector<double> ring_off(static_cast<std::size_t>(nr));
    for (int r = 0; r < nb; ++r) {
        ring_v[static_cast<std::size_t>(r)] =
            static_cast<float>(rings[static_cast<std::size_t>(r)] / end);
        ring_off[static_cast<std::size_t>(r)] = rings[static_cast<std::size_t>(r)];
    }
    for (int k = 0; k < nsk; ++k) {
        const double f = static_cast<double>(k + 1) / nsk;
        ring_v[static_cast<std::size_t>(nb + k)] =
            static_cast<float>(1.0 + 0.25 * f);
        ring_off[static_cast<std::size_t>(nb + k)] = end + p.skirt_m * f;
    }

    // ★ ROAD-REPAIR RUNG 2 -- THE APRON'S RING COLUMN. nap rings that follow
    // the DRIVEN surface outward from the outer skirt ring, plus the lip ring
    // (shared, vertex-exact, with the main strip's outer skirt ring) and the
    // apron's own burial skirt. Zero when apron_m is 0: the whole feature is
    // one branch away from not existing.
    const int nap = bank_apron_ring_count(p);
    const int nra = nap > 0 ? 1 + nap + nsk : 0;
    std::vector<float> apron_v(static_cast<std::size_t>(std::max(0, nra)));
    for (int r = 0; r < nra; ++r)
        // v > 1.2 is already "plain snow, no gravel" in the bank FS (the
        // skirt's own smoothstep), which is what an apron is: the drawn
        // driven surface, not a plow windrow. It keeps the same normals and
        // the same shared sparkle lattice as every other ring, so it cannot
        // read as a bling seam.
        apron_v[static_cast<std::size_t>(r)] = static_cast<float>(
            1.25 + 0.25 * static_cast<double>(r) / std::max(1, nra - 1));

    for (int side = -1; side <= 1; side += 2) {
        // ---- phase A: evaluate every station of this side, in run order ----
        std::vector<Station> stn(n);
        for (std::size_t i = 0; i < n; ++i) {
            Station& st = stn[i];
            st.u = s_arc[i];
            st.s = static_cast<double>(s_arc[i]);
            // F4: a station inside an excavation cut emits nothing and SPLITS
            // the strip (the ribbon T24 mirror -- banks yield at the same rim).
            if (dir_in_any_cut(ctr[i], hf.R, cuts)) continue;  // state 0
            // Tangent from neighbours; outward normal = side * (tangent x up).
            const glm::dvec3 up = ctr[i];
            const std::size_t i0 = i > 0 ? i - 1 : i;
            const std::size_t i1 = i + 1 < n ? i + 1 : i;
            glm::dvec3 t = ctr[i1] - ctr[i0];
            t -= up * glm::dot(t, up);
            const double tl = glm::length(t);
            if (tl < 1e-12) continue;  // degenerate station -> state 0
            t /= tl;
            const glm::dvec3 nrm =
                static_cast<double>(side) * glm::normalize(glm::cross(t, up));
            st.t = t;
            st.out = nrm;
            st.d.resize(static_cast<std::size_t>(nr));
            st.r.resize(static_cast<std::size_t>(nr));
            st.dh_de.resize(static_cast<std::size_t>(nr));
            st.facet.resize(static_cast<std::size_t>(nr));
            st.n.assign(static_cast<std::size_t>(nr), glm::vec3(0.0f));

            double depth_max = 0.0;
            double h_out = 0.0;  // the outer bank ring's excess over the facet
            for (int r = 0; r < nb; ++r) {
                const double off = half_w[i] + rings[static_cast<std::size_t>(r)];
                const glm::dvec3 d = glm::normalize(up + nrm * (off / hf.R));
                // ★ THE ANTI-FORK LINE: the rendered facet + the REAL driven
                // depth. Never a re-derivation of the profile.
                //
                // ★ W2b: this is GEOMETRY (the drawn ring position), so it
                // must sample depth_geometry_at, not depth_at -- W2 capped
                // depth_at's bank contribution at bank_pack_skin_m (0.065 m,
                // a sinkage-only correction), which if used here would flatten
                // every drawn bank crest to a ~7 cm film. depth_geometry_at is
                // the SAME term drive_radius_at composes (corridor_eval's
                // geometry_depth_term + the raw hill), so the ring the mesh
                // draws stays registered to the surface the sled actually
                // drives (facet + lift_m + depth_geometry_at == drive_radius_at
                // minus the terrain-radius/lift bookkeeping, by construction).
                const double depth = snow.depth_geometry_at(d);
                depth_max = std::max(depth_max, depth);
                const double facet = facet_radius_at(hf, d, subdiv, tiles);
                // ★ ROAD-REPAIR (drawn == driven): and then through the
                // SAME registration floor the drive rides
                // (SnowpackField::deck_floor_r). Without it the inner ring --
                // which sits exactly ON the corridor edge (bank_ring_offsets
                // starts at 0.0) -- is composed off the UNFOLDED facet while
                // the ribbon deck it butts against is draped on the FOLDED
                // one, so the strip steps below the deck by the fold residual
                // (census_before.md §1: p50 6 cm, max 1.22 m). One function,
                // two consumers, zero fork: this is the identical body
                // drive_radius_at calls, faded by the identical weight, so
                // the ring can no more disagree with the driven surface than
                // it can with the deck.
                const double rad = snow.deck_floor_r(
                    d, facet + static_cast<double>(p.lift_m) + depth);
                st.d[static_cast<std::size_t>(r)] = d;
                st.r[static_cast<std::size_t>(r)] = rad;
                st.facet[static_cast<std::size_t>(r)] = facet;
                // ★ R3: the ANALYTIC transverse slope of the drawn snow --
                // the same closed form corridor_eval composes, differentiated
                // (world/snowpack.cpp). The facet's own transverse slope is
                // added below as the chord across this station's own ring
                // column: the terrain is a ~59 m facet, so a 9 m chord of it
                // is the facet's slope, while the SNOW is the term with the
                // curvature and the one that must never be chorded.
                st.dh_de[static_cast<std::size_t>(r)] =
                    snow.depth_geometry_slope_at(d);
                if (r == nb - 1) h_out = rad - facet;
            }
            // F3 (kept) + ★ ROAD-REPAIR R2: the skirt still ends BURIED at
            // `facet - skirt_bury_m` -- ending a strip at ground level draws a
            // floating terrace edge -- but the EXCESS over the facet now walks
            // there from the outer bank ring's excess on a smoothstep spread
            // over skirt_m. ss'(0) == 0 makes the join to the bank C1, which
            // is the crease this fixes; ss'(1) == 0 makes the buried end flat.
            const double h_end = -p.skirt_bury_m;
            for (int k = 0; k < nsk; ++k) {
                const double f = static_cast<double>(k + 1) / nsk;
                const double off = half_w[i] + end + p.skirt_m * f;
                const glm::dvec3 d = glm::normalize(up + nrm * (off / hf.R));
                const double facet = facet_radius_at(hf, d, subdiv, tiles);
                const std::size_t r = static_cast<std::size_t>(nb + k);
                st.d[r] = d;
                st.facet[r] = facet;
                st.r[r] = facet + h_out + (h_end - h_out) * ss01(f);
                st.dh_de[r] =
                    (h_end - h_out) * dss01(f) / std::max(1e-6, p.skirt_m);
            }
            // ambient_depth_at is upstream of corridor_eval's bank cap (it is
            // the pre-corridor term BOTH channels start from), so it never
            // diverges between reported and geometry -- no channel switch
            // needed here. It is sampled at the ring furthest from the
            // corridor (offset = end = rise+fall), where bank_profile() has
            // already fallen back to 0 on both channels, so this is also the
            // value depth_geometry_at would read; kept as ambient_depth_at
            // because that is the cheaper, more direct call for "what would
            // this station read with the bank profile absent" -- a min_amp_m
            // cull decision, not a vertex position.
            const double ambient_out =
                snow.ambient_depth_at(st.d[static_cast<std::size_t>(nb - 1)]);
            // ★ ROAD-REPAIR R1: below min_amp_m the station is a CAP, not a
            // deletion. Its vertices are the ones it always would have had --
            // the anti-fork line, unchanged -- and by the cull's own
            // definition its amplitude is under min_amp_m, so the wall the
            // strip ends on is bounded by that dial instead of by
            // bank_height_m.
            st.state = (depth_max - ambient_out < p.min_amp_m) ? 1 : 2;

            // ★ ROAD-REPAIR RUNG 2 -- THE APRON. Built on THIS side, from the
            // outer skirt ring outward, riding drive_radius_at: the anti-fork
            // line again, one surface further out. Not one call of any of it
            // happens when apron_m is 0 (nap == 0), which is the identity.
            if (nap <= 0) continue;
            const double foot_off = half_w[i] + end;
            const auto facet_fn = [&](glm::dvec3 dd) {
                return facet_radius_at(hf, dd, subdiv, tiles);
            };
            // The fold provider is passed EXPLICITLY (never the drape-facing
            // global): the strip is composed against the same SnowpackField
            // the planet mesh was folded with, by construction, so the
            // agreement test cannot be answered by some other world's fold.
            const auto drawn_fn = [&](glm::dvec3 dd) {
                return drawn_radius_at(hf, dd, subdiv, tiles, &snow);
            };
            st.areach = bank_apron_reach_m(
                snow, facet_fn, drawn_fn, up, nrm, foot_off, p.skirt_m,
                p.apron_tol_m, p.apron_min_drop_m, nap, p.apron_m);
            if (st.areach <= 0.0) continue;  // flat enough: no apron, no cost
            const std::size_t nras = static_cast<std::size_t>(nra);
            st.ad.resize(nras);
            st.aoff.resize(nras);
            st.ar.resize(nras);
            st.afacet.resize(nras);
            st.an.assign(nras, glm::vec3(0.0f));
            // Ring 0 is the LIP: the main strip's outermost skirt ring,
            // vertex-exact (same dir, same radial), so the apron opens on the
            // edge the skirt closes on and there is no crack between them.
            st.ad[0] = st.d[static_cast<std::size_t>(nr - 1)];
            st.ar[0] = st.r[static_cast<std::size_t>(nr - 1)];
            st.afacet[0] = st.facet[static_cast<std::size_t>(nr - 1)];
            st.aoff[0] = foot_off + p.skirt_m;
            for (int k = 1; k <= nap; ++k) {
                const double off =
                    foot_off + p.skirt_m + st.areach * k / nap;
                const glm::dvec3 d = glm::normalize(up + nrm * (off / hf.R));
                const std::size_t r = static_cast<std::size_t>(k);
                st.ad[r] = d;
                st.aoff[r] = off;
                st.afacet[r] = facet_radius_at(hf, d, subdiv, tiles);
                // ★ THE ANTI-FORK LINE, APRON EDITION: the drawn apron IS
                // SnowpackField::drive_radius_at -- literally the function the
                // sled's contact patch reads, sampled at the vertex. It is not
                // "facet + lift + depth" re-composed here, because out past
                // the corridor the deck lift has faded and re-composing it
                // would re-introduce the 0.45 m fork this rung exists to close.
                st.ar[r] = snow.drive_radius_at(d);
            }
            // ...and then the apron buries, on the SKIRT'S OWN LAW (F3: a
            // strip may never end in mid-air -- a free edge draws a floating
            // terrace). h_out is the apron's last ring's excess over the
            // terrain facet, exactly as the skirt's h_out is the bank's.
            {
                const double h_out_a = st.ar[static_cast<std::size_t>(nap)] -
                                       st.afacet[static_cast<std::size_t>(nap)];
                const double h_end_a = -p.skirt_bury_m;
                for (int k = 0; k < nsk; ++k) {
                    const double f = static_cast<double>(k + 1) / nsk;
                    const double off = foot_off + p.skirt_m + st.areach +
                                       p.skirt_m * f;
                    const glm::dvec3 d =
                        glm::normalize(up + nrm * (off / hf.R));
                    const std::size_t r =
                        static_cast<std::size_t>(1 + nap + k);
                    st.ad[r] = d;
                    st.aoff[r] = off;
                    st.afacet[r] = facet_radius_at(hf, d, subdiv, tiles);
                    st.ar[r] =
                        st.afacet[r] + h_out_a + (h_end_a - h_out_a) * ss01(f);
                }
            }
        }

        // ---- phase B: normals, over the WHOLE run ------------------------
        // The longitudinal derivative is a central difference between the
        // run's own neighbouring live stations -- taken HERE, before the walk
        // below cuts the run into chunks, so a chunk seam or the left/right
        // pair cannot shade differently. A hard break (state 0) is a real
        // discontinuity in the surface and stops the difference.
        for (std::size_t i = 0; i < n; ++i) {
            Station& st = stn[i];
            if (st.state == 0) continue;
            const bool has_prev = i > 0 && stn[i - 1].state != 0;
            const bool has_next = i + 1 < n && stn[i + 1].state != 0;
            for (int r = 0; r < nr; ++r) {
                const std::size_t k = static_cast<std::size_t>(r);
                // Transverse: the analytic snow slope + the facet's chord.
                const std::size_t lo =
                    r > 0 ? k - 1 : k;
                const std::size_t hi =
                    r + 1 < nr ? k + 1 : k;
                const double span = std::max(1e-6, ring_off[hi] - ring_off[lo]);
                const double dfacet = (st.facet[hi] - st.facet[lo]) / span;
                const double dr_de = dfacet + st.dh_de[k];

                // Longitudinal: central difference of the radial at the SAME
                // ring index, per metre of arc.
                double dr_ds = 0.0;
                if (has_prev || has_next) {
                    const Station& a = has_prev ? stn[i - 1] : st;
                    const Station& b = has_next ? stn[i + 1] : st;
                    const double ds = b.s - a.s;
                    if (std::abs(ds) > 1e-6) dr_ds = (b.r[k] - a.r[k]) / ds;
                }
                const glm::dvec3 d = st.d[k];
                const glm::dvec3 te = st.out + d * dr_de;
                const glm::dvec3 ts = st.t + d * dr_ds;
                glm::dvec3 nv = glm::cross(ts, te);
                const double nl = glm::length(nv);
                nv = nl > 1e-12 ? nv / nl : d;
                if (glm::dot(nv, d) < 0.0) nv = -nv;  // always outward
                st.n[k] = glm::vec3(nv);
            }
            // ★ ROAD-REPAIR RUNG 2: the apron's normals, the same construction
            // with ONE deliberate difference -- the transverse term is a pure
            // CHORD of the apron's own ring column, not the analytic snow
            // slope. The bank needed the analytic derivative because
            // bank_profile has real curvature over one ring; the apron is the
            // driven surface out past the corridor, where the shape is the
            // terrain facet plus a near-constant ambient depth, so the chord
            // IS the slope to well under the ring spacing.
            if (st.areach <= 0.0) continue;
            const bool ap_prev = i > 0 && stn[i - 1].areach > 0.0 &&
                                 stn[i - 1].state != 0;
            const bool ap_next = i + 1 < n && stn[i + 1].areach > 0.0 &&
                                 stn[i + 1].state != 0;
            for (int r = 0; r < nra; ++r) {
                const std::size_t k = static_cast<std::size_t>(r);
                const std::size_t lo = r > 0 ? k - 1 : k;
                const std::size_t hi = r + 1 < nra ? k + 1 : k;
                const double span = std::max(1e-6, st.aoff[hi] - st.aoff[lo]);
                const double dr_de = (st.ar[hi] - st.ar[lo]) / span;
                double dr_ds = 0.0;
                if (ap_prev || ap_next) {
                    const Station& a = ap_prev ? stn[i - 1] : st;
                    const Station& b = ap_next ? stn[i + 1] : st;
                    const double ds = b.s - a.s;
                    if (std::abs(ds) > 1e-6) dr_ds = (b.ar[k] - a.ar[k]) / ds;
                }
                const glm::dvec3 d = st.ad[k];
                const glm::dvec3 te = st.out + d * dr_de;
                const glm::dvec3 ts = st.t + d * dr_ds;
                glm::dvec3 nv = glm::cross(ts, te);
                const double nl = glm::length(nv);
                nv = nl > 1e-12 ? nv / nl : d;
                if (glm::dot(nv, d) < 0.0) nv = -nv;
                st.an[k] = glm::vec3(nv);
            }
        }

        // ---- phase C: walk, cap, chunk, flush ----------------------------
        StripAccum acc;
        long long pending_cap = -1;  // the cap that opens the NEXT strip
        long long last_pushed = -1;
        for (std::size_t i = 0; i < n; ++i) {
            const Station& st = stn[i];
            if (st.state == 0) {
                flush(acc, nr, ring_v, out, &verts);
                pending_cap = -1;
                continue;
            }
            if (st.state == 1) {
                // Close the open strip ON the cap, then break; the same cap
                // re-opens whatever resumes on the far side of the gap.
                if (acc.stations > 0) {
                    if (p.taper_caps) push_station(acc, st, nr);
                    flush(acc, nr, ring_v, out, &verts);
                }
                pending_cap = p.taper_caps ? static_cast<long long>(i) : -1;
                continue;
            }
            if (acc.stations == 0 && pending_cap >= 0)
                push_station(acc, stn[static_cast<std::size_t>(pending_cap)], nr);
            pending_cap = -1;
            // F6: chunk at the u16 ceiling, duplicating the boundary station
            // into the next chunk so there is no one-station crack.
            if (acc.stations >= cap_stations && last_pushed >= 0) {
                const Station& keep = stn[static_cast<std::size_t>(last_pushed)];
                flush(acc, nr, ring_v, out, &verts);
                push_station(acc, keep, nr);
            }
            push_station(acc, st, nr);
            last_pushed = static_cast<long long>(i);
        }
        flush(acc, nr, ring_v, out, &verts);

        // ---- phase D: the APRON strips -----------------------------------
        // A SEPARATE strip, not extra rings on the main one, for two reasons
        // that are the same reason: the ring count of a strip is uniform over
        // its stations, so folding the apron in would have forced every flat
        // station on the map to carry apron vertices it does not need (the
        // +72 % vertex debt this lane already owes says no), and an apron is
        // a LOCAL piece of geometry over a LOCAL shelf -- it should begin and
        // end where the shelf does. Contiguous qualifying stations make one
        // strip; a station that does not qualify breaks it, exactly as a cut
        // breaks the main strip.
        if (nra > 0) {
            const int ap_cap_stations = std::max(
                2, std::min(p.max_stations_per_mesh,
                            65535 / std::max(1, nra)));
            StripAccum aacc;
            long long ap_last = -1;
            for (std::size_t i = 0; i < n; ++i) {
                const Station& st = stn[i];
                if (st.state == 0 || st.areach <= 0.0) {
                    flush(aacc, nra, apron_v, out, &verts, true);
                    continue;
                }
                if (aacc.stations >= ap_cap_stations && ap_last >= 0) {
                    const Station& keep = stn[static_cast<std::size_t>(ap_last)];
                    flush(aacc, nra, apron_v, out, &verts, true);
                    push_apron_station(aacc, keep, nra);
                }
                push_apron_station(aacc, st, nra);
                ap_last = static_cast<long long>(i);
            }
            flush(aacc, nra, apron_v, out, &verts, true);
        }
    }
    return verts;
}

BankStripContinuity bank_strip_continuity(
    const std::vector<BankStripCPU>& strips, int nr, double crest_v) {
    BankStripContinuity c;
    if (nr <= 0) return c;
    std::vector<double> steps, ends, crest;
    for (const BankStripCPU& s : strips) {
        // ★ ROAD-REPAIR RUNG 2: the apron strips have their OWN ring count, so
        // reading them at `nr` would report a jag that is only a stride error.
        // They are excluded here on purpose: this metric is the BANK's
        // continuity, and the apron's continuity is the census's
        // `apron_gap_m`, measured against the driven surface rather than
        // against itself.
        if (s.is_apron) continue;
        const std::size_t nv = s.pos.size() / 3;
        if (s.uv.size() != nv * 2) continue;
        const std::size_t ns = nv / static_cast<std::size_t>(nr);
        if (ns < 2) continue;
        auto radial = [&](std::size_t i, int r) {
            const std::size_t k =
                i * static_cast<std::size_t>(nr) + static_cast<std::size_t>(r);
            const double x = s.pos[3 * k + 0], y = s.pos[3 * k + 1],
                         z = s.pos[3 * k + 2];
            return std::sqrt(x * x + y * y + z * z);
        };
        // The ring nearest the crest, by its own v tag -- the lateral the
        // census's crest row stands at.
        int crest_r = -1;
        if (crest_v >= 0.0) {
            double best = 1e30;
            for (int r = 0; r < nr; ++r) {
                const double d =
                    std::abs(static_cast<double>(
                                 s.uv[2 * static_cast<std::size_t>(r) + 1]) -
                             crest_v);
                if (d < best) {
                    best = d;
                    crest_r = r;
                }
            }
        }
        for (std::size_t i = 0; i + 1 < ns; ++i) {
            ++c.pairs;
            double worst = 0.0;
            for (int r = 0; r < nr; ++r)
                worst = std::max(worst, std::abs(radial(i + 1, r) - radial(i, r)));
            if (worst > 0.5) ++c.steps_over_50cm;
            steps.push_back(worst);
            if (crest_r >= 0) {
                crest.push_back(
                    std::abs(radial(i + 1, crest_r) - radial(i, crest_r)));
                ++c.crest_pairs;
            }
        }
        // The terminal walls: how far the bank still stands above its own
        // outermost BANK ring (the last ring whose v tag is <= 1).
        int outer_bank = 0;
        for (int r = 0; r < nr; ++r)
            if (s.uv[2 * static_cast<std::size_t>(r) + 1] <= 1.0f) outer_bank = r;
        for (std::size_t i : {static_cast<std::size_t>(0), ns - 1}) {
            double amp = 0.0;
            const double base = radial(i, outer_bank);
            for (int r = 0; r <= outer_bank; ++r)
                amp = std::max(amp, radial(i, r) - base);
            ends.push_back(amp);
            ++c.strip_ends;
        }
    }
    auto pct = [](std::vector<double> v, double q) {
        if (v.empty()) return 0.0;
        std::sort(v.begin(), v.end());
        const std::size_t k = static_cast<std::size_t>(
            q * static_cast<double>(v.size() - 1) + 0.5);
        return v[std::min(k, v.size() - 1)];
    };
    c.step_p50 = pct(steps, 0.50);
    c.step_p90 = pct(steps, 0.90);
    c.step_p99 = pct(steps, 0.99);
    c.step_max = steps.empty() ? 0.0 : *std::max_element(steps.begin(), steps.end());
    c.crest_p50 = pct(crest, 0.50);
    c.crest_p90 = pct(crest, 0.90);
    c.crest_p99 = pct(crest, 0.99);
    c.crest_max =
        crest.empty() ? 0.0 : *std::max_element(crest.begin(), crest.end());
    c.end_amp_p50 = pct(ends, 0.50);
    c.end_amp_p99 = pct(ends, 0.99);
    c.end_amp_max = ends.empty() ? 0.0 : *std::max_element(ends.begin(), ends.end());
    return c;
}

std::vector<BankRun> recover_bank_runs(const HeightField& hf,
                                       const BankBuildParams& p,
                                       const std::vector<int>& kinds) {
    std::vector<BankRun> runs;
    for (std::size_t pi = 0; pi < kSudburyRibbonPathCount; ++pi) {
        const GisRibbonPath& P = kSudburyRibbonPaths[pi];
        if (std::find(kinds.begin(), kinds.end(), P.kind) == kinds.end())
            continue;
        if (P.vtx_count < 4) continue;
        const int path_id = static_cast<int>(pi);
        const int kind = P.kind;

        // Walk the L/R pairs; a run boundary is arc s NON-INCREASING (F1 --
        // the linework add_path rule; these paths are concatenated batches of
        // thousands of streets and interpolating across a join slings a bank
        // ridge across town).
        std::vector<glm::dvec3> ctr;
        std::vector<float> s;
        std::vector<double> hw;
        auto emit_run = [&]() {
            if (ctr.size() >= 2)
                runs.push_back(BankRun{path_id, kind, ctr, s, hw});
            ctr.clear();
            s.clear();
            hw.clear();
        };
        const int pairs = P.vtx_count / 2;
        float prev_s = -1.0f;
        glm::dvec3 pc{0.0};
        float ps = 0.0f;
        double phw = 0.0;
        bool have_prev = false;
        for (int k = 0; k < pairs; ++k) {
            const GisRibbonVertex& A = kSudburyRibbonVerts[P.vtx_off + 2 * k];
            const GisRibbonVertex& B =
                kSudburyRibbonVerts[P.vtx_off + 2 * k + 1];
            const glm::dvec3 da(A.dir[0], A.dir[1], A.dir[2]);
            const glm::dvec3 db(B.dir[0], B.dir[1], B.dir[2]);
            const glm::dvec3 c = glm::normalize(da + db);
            // Per-STATION width recovery from the drawn pair (INV-6; F9 --
            // never the ribbons' per-path median shortcut).
            const double w =
                0.5 * std::acos(std::clamp(glm::dot(da, db), -1.0, 1.0)) *
                hf.R;
            if (have_prev && A.s <= prev_s) emit_run();  // run boundary (F1)
            // Resample the (~80 m densified) pair spacing down to station_m.
            if (have_prev && !ctr.empty()) {
                const double seg = static_cast<double>(A.s) - ps;
                // ★ ROAD-REPAIR: SHORT STATIONS WHERE THE GAP IS FADING. The
                // window is bank_gap_m, the fade's own length, and the test is
                // taken at BOTH ends of the pair -- a 80 m baked pair can have
                // one end inside the fade and the other well outside it, and
                // the end that matters is whichever one is closer to the
                // corner. Off (junction_station_m == 0) this is one compare
                // and the arithmetic below is the shipped arithmetic.
                double st_m = p.station_m;
                if (p.junction_station_m > 0.0 &&
                    p.junction_net != nullptr && p.junction_near_m > 0.0) {
                    const double reach = 2.0 * p.junction_near_m;
                    const double jd = std::min(
                        p.junction_net->junction_dist(pc, reach),
                        p.junction_net->junction_dist(c, reach));
                    if (jd < p.junction_near_m)
                        st_m = std::min(p.station_m, p.junction_station_m);
                }
                const int sub = std::max(
                    1, static_cast<int>(std::ceil(seg / st_m)));
                for (int q = 1; q < sub; ++q) {
                    const double f = static_cast<double>(q) / sub;
                    ctr.push_back(glm::normalize(pc * (1.0 - f) + c * f));
                    s.push_back(static_cast<float>(ps + f * seg));
                    hw.push_back(phw * (1.0 - f) + w * f);
                }
            }
            ctr.push_back(c);
            s.push_back(A.s);
            hw.push_back(w);
            pc = c;
            ps = A.s;
            phw = w;
            prev_s = A.s;
            have_prev = true;
        }
        emit_run();
    }
    return runs;
}

std::vector<BankStripCPU> build_bank_strips(const HeightField& hf, int subdiv,
                                            int tiles,
                                            const world::SnowpackField& snow,
                                            const BankBuildParams& p,
                                            const std::vector<CutDisk>& cuts,
                                            long* verts_out) {
    // Phase 1 (serial, cheap): recover + resample every RUN -- now through the
    // SHARED recover_bank_runs, so the road-gap census walks the same stations
    // this mesh is built on. Phase 2 (parallel): the depth_at-heavy strip
    // generation -- the core is pure, so runs shard across threads with no
    // shared state (SF2 red-team F5: the measured serial build was ~30 s; the
    // budget is startup, not correctness).
    // PLOWED roads only (§2.4c): trails are groomed, rivers are water.
    // ★ ROAD-REPAIR: the junction window is bank_gap_m, single-sourced from
    // the field's own fade law rather than retyped as a [bank_mesh] number.
    // The network comes from the SAME SnowpackField the strips are composed
    // against, so the stations cannot be denser than the field they sample.
    BankBuildParams rp = p;
    if (rp.junction_station_m > 0.0) {
        rp.junction_net = snow.lines;
        rp.junction_near_m = snow.p.bank_gap_m;
    }
    const std::vector<BankRun> runs = recover_bank_runs(hf, rp, {0, 1});
    long verts = 0;
    // Phase 2: shard the runs across workers; deterministic output order
    // (per-run buckets concatenated in run order, independent of scheduling).
    const unsigned hw_threads = std::thread::hardware_concurrency();
    const unsigned workers =
        std::max(1u, std::min<unsigned>(hw_threads > 2 ? hw_threads - 2 : 1u,
                                        16u));
    std::vector<std::vector<BankStripCPU>> bucket(runs.size());
    std::vector<long> bucket_verts(runs.size(), 0);
    std::vector<std::thread> pool;
    std::size_t next = 0;
    // Static striping: worker w takes runs w, w+W, w+2W... -- deterministic
    // and balanced enough (runs are many and small).
    for (unsigned w = 0; w < workers; ++w)
        pool.emplace_back([&, w]() {
            for (std::size_t ri = w; ri < runs.size(); ri += workers)
                bucket_verts[ri] =
                    build_bank_run(runs[ri].ctr, runs[ri].s, runs[ri].hw, hf,
                                   subdiv, tiles, snow, p, cuts, bucket[ri]);
        });
    (void)next;
    for (std::thread& th : pool) th.join();
    std::vector<BankStripCPU> out;
    for (std::size_t ri = 0; ri < runs.size(); ++ri) {
        verts += bucket_verts[ri];
        for (BankStripCPU& sc : bucket[ri]) out.push_back(std::move(sc));
    }
    if (verts_out != nullptr) *verts_out = verts;
    return out;
}

}  // namespace render
