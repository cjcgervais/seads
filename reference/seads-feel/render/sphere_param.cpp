#include "render/sphere_param.h"

#include "world/snowpack.h"  // R1: the DRAWN fold provider

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace render {

namespace {
constexpr double kQuarterPi = 0.78539816339744830961;
}  // namespace

FaceBasis face_basis(int f) {
    // (normal, right, up) with right x up == normal (outward). Identical table
    // to the pre-extraction render/planet.cpp kFaces (the MESH convention).
    static const FaceBasis kFaces[6] = {
        {{1, 0, 0}, {0, 0, -1}, {0, 1, 0}},   // +X
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},   // -X
        {{0, 1, 0}, {0, 0, 1}, {1, 0, 0}},    // +Y
        {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},   // -Y
        {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},    // +Z
        {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}},  // -Z
    };
    if (f < 0) f = 0;
    if (f > 5) f = 5;
    return kFaces[f];
}

double warp(double s) { return std::tan(s * kQuarterPi); }
double unwarp(double s) { return std::atan(s) / kQuarterPi; }

glm::dvec3 face_dir(int f, double s, double t) {
    const FaceBasis b = face_basis(f);
    return glm::normalize(b.n + b.r * s + b.u * t);
}

FaceCoord dir_to_face(glm::dvec3 d) {
    // Major-axis select: the face whose outward normal the direction most
    // points along (largest positive dot). Strict '>' means the lowest face
    // index wins an exact edge tie -> deterministic, no double-counted edge
    // cells.
    FaceCoord fc;
    double best = -2.0;
    for (int f = 0; f < 6; ++f) {
        const FaceBasis b = face_basis(f);
        const double denom = glm::dot(d, b.n);
        if (denom > best) {
            best = denom;
            fc.face = f;
            // Gnomonic: project d onto the face plane at unit normal distance.
            fc.s = glm::dot(d, b.r) / denom;
            fc.t = glm::dot(d, b.u) / denom;
        }
    }
    return fc;
}

glm::dvec3 gl_cube_dir(int face, double sc, double tc) {
    // Inverse of the OpenGL cube-map major-axis selection (spec Table 8.19):
    // face,(sc,tc) at unit major axis magnitude -> direction, then normalize.
    // Distinct axes from face_basis() by design (header note).
    if (face < 0) face = 0;
    if (face > 5) face = 5;
    glm::dvec3 d(0.0);
    switch (face) {
        case 0:
            d = {1.0, -tc, -sc};
            break;  // +X
        case 1:
            d = {-1.0, -tc, sc};
            break;  // -X
        case 2:
            d = {sc, 1.0, tc};
            break;  // +Y
        case 3:
            d = {sc, -1.0, -tc};
            break;  // -Y
        case 4:
            d = {sc, -tc, 1.0};
            break;  // +Z
        case 5:
            d = {-sc, -tc, -1.0};
            break;  // -Z
    }
    return glm::normalize(d);
}

std::vector<std::uint8_t> bake_equirect_cubemap(const std::uint8_t* rgb, int w,
                                                int h, double u_offset,
                                                int face_size) {
    if (face_size < 1) face_size = 1;
    // Its own RGB bilinear (sample01 is single-channel): u wraps, v clamps,
    // -0.5 texel center — the exact convention the mesh height sampler uses.
    auto sample = [&](double u, double v, double* c) {
        const double fx = u * w - 0.5, fy = v * h - 0.5;
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const double tx = fx - x0, ty = fy - y0;
        auto at = [&](int x, int y, int ch) -> double {
            x = ((x % w) + w) % w;                 // wrap longitude
            y = y < 0 ? 0 : (y >= h ? h - 1 : y);  // clamp latitude
            return rgb[(static_cast<std::size_t>(y) * w + x) * 3 + ch];
        };
        for (int ch = 0; ch < 3; ++ch) {
            const double a = at(x0, y0, ch), b = at(x0 + 1, y0, ch);
            const double e = at(x0, y0 + 1, ch), g = at(x0 + 1, y0 + 1, ch);
            c[ch] = (a * (1 - tx) + b * tx) * (1 - ty) +
                    (e * (1 - tx) + g * tx) * ty;
        }
    };

    std::vector<std::uint8_t> out(static_cast<std::size_t>(6) * face_size *
                                  face_size * 3);
    for (int f = 0; f < 6; ++f) {
        for (int py = 0; py < face_size; ++py) {
            for (int px = 0; px < face_size; ++px) {
                const double s = (px + 0.5) / face_size;
                const double t = (py + 0.5) / face_size;
                const glm::dvec3 d =
                    gl_cube_dir(f, 2.0 * s - 1.0, 2.0 * t - 1.0);
                const glm::dvec2 uv = equirect_uv(d, u_offset);
                double c[3];
                sample(uv.x, uv.y, c);
                const std::size_t o =
                    ((static_cast<std::size_t>(f) * face_size + py) *
                         face_size +
                     px) *
                    3;
                out[o + 0] = static_cast<std::uint8_t>(c[0] + 0.5);
                out[o + 1] = static_cast<std::uint8_t>(c[1] + 0.5);
                out[o + 2] = static_cast<std::uint8_t>(c[2] + 0.5);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> bake_equirect_cubemap_rgba(const std::uint8_t* rgb,
                                                     const std::uint8_t* mask,
                                                     int w, int h,
                                                     double u_offset,
                                                     int face_size) {
    if (face_size < 1) face_size = 1;
    // Same bilinear convention as bake_equirect_cubemap (u wraps, v clamps,
    // -0.5 center); RGB from `rgb`, A from the single-channel `mask`.
    auto sample = [&](double u, double v, double* c) {
        const double fx = u * w - 0.5, fy = v * h - 0.5;
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const double tx = fx - x0, ty = fy - y0;
        auto at = [&](int x, int y, int ch, int stride,
                      const std::uint8_t* p) -> double {
            x = ((x % w) + w) % w;
            y = y < 0 ? 0 : (y >= h ? h - 1 : y);
            return p[(static_cast<std::size_t>(y) * w + x) * stride + ch];
        };
        for (int ch = 0; ch < 3; ++ch) {
            const double a = at(x0, y0, ch, 3, rgb),
                         b = at(x0 + 1, y0, ch, 3, rgb);
            const double e = at(x0, y0 + 1, ch, 3, rgb),
                         g = at(x0 + 1, y0 + 1, ch, 3, rgb);
            c[ch] = (a * (1 - tx) + b * tx) * (1 - ty) +
                    (e * (1 - tx) + g * tx) * ty;
        }
        const double a = at(x0, y0, 0, 1, mask), b = at(x0 + 1, y0, 0, 1, mask);
        const double e = at(x0, y0 + 1, 0, 1, mask),
                     g = at(x0 + 1, y0 + 1, 0, 1, mask);
        c[3] =
            (a * (1 - tx) + b * tx) * (1 - ty) + (e * (1 - tx) + g * tx) * ty;
    };

    std::vector<std::uint8_t> out(static_cast<std::size_t>(6) * face_size *
                                  face_size * 4);
    for (int f = 0; f < 6; ++f) {
        for (int py = 0; py < face_size; ++py) {
            for (int px = 0; px < face_size; ++px) {
                const double s = (px + 0.5) / face_size;
                const double t = (py + 0.5) / face_size;
                const glm::dvec3 d =
                    gl_cube_dir(f, 2.0 * s - 1.0, 2.0 * t - 1.0);
                const glm::dvec2 uv = equirect_uv(d, u_offset);
                double c[4];
                sample(uv.x, uv.y, c);
                const std::size_t o =
                    ((static_cast<std::size_t>(f) * face_size + py) *
                         face_size +
                     px) *
                    4;
                for (int ch = 0; ch < 4; ++ch)
                    out[o + ch] = static_cast<std::uint8_t>(c[ch] + 0.5);
            }
        }
    }
    return out;
}

FaceMesh fill_face(const HeightField& hf, int f, int N) {
    return fill_face(hf, f, N, 0, 0, 1);
}

// A vertex direction is INSIDE a cut disk when the great-circle angle to the
// disk's axis, times the base radius hf.R, is under the disk radius (T2 portal
// surgery). acos of the clamped dot is the angle; unit-length inputs. Empty cut
// list => always false (the bit-identical no-op). Exported (T24): the ribbon
// clip drops draped road/trail facets over the SAME cut disks the terrain
// drops triangles for — one definition or the two surfaces disagree at every
// excavation rim.
bool dir_in_any_cut(const glm::dvec3& d, double R,
                    const std::vector<CutDisk>& cuts) {
    for (const CutDisk& c : cuts) {
        const double dot = glm::clamp(glm::dot(d, c.dir), -1.0, 1.0);
        const double arc = std::acos(dot) * R;
        if (arc < c.radius_m) return true;
    }
    return false;
}

namespace {
// ★ ROAD-REPAIR P1 (red-team 2026-09-09) -- THE CORNER MEMO, and why the sink
// floor needs one.
//
// facet_radius_impl evaluates THREE cell corners per query. On the fold path
// each corner runs SnowpackField::draw_fold_at -> draw_sample_at ->
// ambient_depth_at + draw_corridor_mask_at (a lines->nearest at
// draw_mask_search_m) + draw_water_proximity_at. The sink floor
// (SnowpackField::apply_deck_floor) put that query on sample_at's per-substep
// path, so a corridor sample was paying THREE extra nearest() lookups and
// THREE ambient evaluations -- not "one extra radius query" -- and W1.1/INV-1
// (world/snowpack.cpp: "ONE corridor_eval() call ... exactly the double cost
// this refactor exists to remove") was reopened by the back door. The finding
// was RIGHT; this is the fix it asked for.
//
// THE OBSERVATION: the corners are GRID VERTICES, ~59 m apart at the shipped
// subdiv, and the sled's 3 patches x 12 substeps land centimetres from each
// other. They ask for the SAME three corners over and over. So cache the
// answer, direct-mapped, keyed by every input it depends on (fold provider +
// generation, height field, N, tiles, face, grid i/j). A hit returns the
// IDENTICAL double the miss would have computed -- the memo can only skip
// work, never change a value, which is what keeps the surface bit-identical.
//
// ★ WHAT MAKES IT SOUND: draw_fold_at is a pure function of LOAD-TIME state.
// ambient_depth_at reads hf / barren / landmask / the [snowpack] dials;
// draw_corridor_mask_at reads `lines`; draw_water_proximity_at reads landmask
// + hf. None of them reads `tracks`, the hill, or anything the sim mutates
// (world/snowpack.cpp) -- the fold channel does not move at run time.
//
// ⚠ IF A PROVIDER IS REBOUND OR RE-DIALLED, INVALIDATE. set_drawn_fold bumps
// the generation on every call, so the drape provider is covered; anyone who
// mutates a bound fold's params in place must call invalidate_facet_fold_memo()
// (the key includes the POINTER, and a fresh object at a recycled address
// would otherwise read a stale corner).
//
// thread_local: the mesh build and the census shard across workers, and a
// per-thread cache needs no lock and cannot tear.
struct FoldMemoEntry {
    const world::SnowpackField* fold = nullptr;
    const HeightField* hf = nullptr;
    unsigned gen = 0;
    int face = -1, gi = -1, gj = -1, N = 0, tiles = 0;
    glm::dvec3 p{0.0};
    bool valid = false;
};
constexpr std::size_t kFoldMemoSlots = 64;  // power of two
thread_local FoldMemoEntry t_fold_memo[kFoldMemoSlots];
unsigned g_fold_gen = 1;

// SEADS_FACET_MEMO=0 is the kill switch AND the honest OFF arm of the cost
// measurement (app/main.cpp, SEADS_DECK_COST): with it off the tree behaves
// exactly as the sink-fix commit shipped, three lookups per corridor sample.
bool& fold_memo_flag() {
    static bool on = [] {
        const char* e = std::getenv("SEADS_FACET_MEMO");
        return !(e != nullptr && std::atof(e) == 0.0);
    }();
    return on;
}

std::size_t fold_memo_slot(int face, int gi, int gj) {
    std::size_t h = static_cast<std::size_t>(face) * 0x9E3779B1u;
    h ^= static_cast<std::size_t>(gi + 1) * 0x85EBCA6Bu;
    h ^= static_cast<std::size_t>(gj + 1) * 0xC2B2AE35u;
    h ^= h >> 13;
    return h & (kFoldMemoSlots - 1);
}

// ★ R1: ONE body for both facets. `fold` null == the TERRAIN facet the sled
// drives; non-null == the DRAWN facet the screen shows. Sharing the body is the
// anti-fork: the two surfaces differ by exactly one term and cannot drift in
// face pick, grid recovery, triangle split, or ray-plane solve.
double facet_radius_impl(const HeightField& hf, glm::dvec3 d, int N, int tiles,
                         const world::SnowpackField* fold) {
    if (N < 2) N = 2;
    if (N > 256) N = 256;
    if (tiles < 1) tiles = 1;
    if (tiles > 4)
        tiles = 4;  // MIRROR load_planet's clamp: the drape must
                    // conform to the mesh actually on screen
                    // (adversarial review P3-2)
    d = glm::normalize(d);
    // Same face pick as the mesh (major-axis, lowest index wins a tie), then
    // the vertex-grid cell: gnomonic (s,t) ARE the warped tangent coords, so
    // unwarp() recovers the uniform grid param fill_face laid the verts on.
    const FaceCoord fc = dir_to_face(d);
    const FaceBasis b = face_basis(fc.face);
    const double ecells = static_cast<double>(tiles) * (N - 1);
    auto grid = [&](double warped) {
        const double x = (unwarp(warped) + 1.0) * 0.5 * ecells;
        // Clamp INSIDE the grid so an exact-edge dir stays in the last cell.
        return glm::clamp(x, 0.0, ecells - 1.0e-9);
    };
    const double gx = grid(fc.s), gy = grid(fc.t);
    const int i0 = static_cast<int>(gx), j0 = static_cast<int>(gy);
    const double fx = gx - i0, fy = gy - j0;
    // Corner vertices through the IDENTICAL expression fill_face uses (global
    // grid index -> warp -> normalized dir -> radius_at) — bit-faithful.
    auto corner_eval = [&](int gi, int gj) {
        const double s = warp(-1.0 + 2.0 * gi / ecells);
        const double t = warp(-1.0 + 2.0 * gj / ecells);
        const glm::dvec3 dir = glm::normalize(b.n + b.r * s + b.u * t);
        const double snow = fold != nullptr ? fold->draw_fold_at(dir) : 0.0;
        return dir * (hf.radius_at(dir) + snow);
    };
    // The memo sits ONLY on the fold path: the terrain facet's corner is one
    // radius_at, cheaper than the lookup that would guard it, and leaving it
    // alone keeps facet_radius_at byte-for-byte the function it always was.
    auto corner = [&](int gi, int gj) {
        if (fold == nullptr || !fold_memo_flag()) return corner_eval(gi, gj);
        FoldMemoEntry& e = t_fold_memo[fold_memo_slot(fc.face, gi, gj)];
        if (e.valid && e.fold == fold && e.hf == &hf && e.gen == g_fold_gen &&
            e.face == fc.face && e.gi == gi && e.gj == gj && e.N == N &&
            e.tiles == tiles)
            return e.p;
        const glm::dvec3 p = corner_eval(gi, gj);
        e.fold = fold;
        e.hf = &hf;
        e.gen = g_fold_gen;
        e.face = fc.face;
        e.gi = gi;
        e.gj = gj;
        e.N = N;
        e.tiles = tiles;
        e.p = p;
        e.valid = true;
        return p;
    };
    const glm::dvec3 p00 = corner(i0, j0);
    const glm::dvec3 p11 = corner(i0 + 1, j0 + 1);
    // The cell's two triangles split on the v00–v11 diagonal (the fill_face
    // index order): fx >= fy lies in (v00, v10, v11), else (v00, v11, v01).
    const glm::dvec3 pc = fx >= fy ? corner(i0 + 1, j0) : corner(i0, j0 + 1);
    // Ray from the origin along d against the triangle's plane: the rendered
    // surface radius at d.
    const glm::dvec3 n = glm::cross(p11 - p00, pc - p00);
    const double denom = glm::dot(n, d);
    // A degenerate/edge-on facet (should not occur on a displaced sphere):
    // fall back to the field itself rather than divide by ~0.
    if (std::abs(denom) < 1e-12)
        return hf.radius_at(d) +
               (fold != nullptr ? fold->draw_fold_at(d) : 0.0);
    return glm::dot(n, p00) / denom;
}
}  // namespace

double facet_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles) {
    return facet_radius_impl(hf, d, N, tiles, nullptr);
}

double drawn_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles,
                       const world::SnowpackField* fold) {
    return facet_radius_impl(hf, d, N, tiles, fold);
}

namespace {
const world::SnowpackField* g_drawn_fold = nullptr;
}  // namespace

void set_drawn_fold(const world::SnowpackField* fold) {
    // ★ ROAD-REPAIR P1: a rebind invalidates every cached corner, on every
    // thread, by key -- unconditionally, because the same pointer re-dialled
    // (draw.cpp rebuilds s_drawn_fold in place from SEADS_MASK_M) is a
    // DIFFERENT surface with an identical address.
    ++g_fold_gen;
    g_drawn_fold = fold;
}

void invalidate_facet_fold_memo() { ++g_fold_gen; }

void set_facet_fold_memo(bool on) {
    fold_memo_flag() = on;
    ++g_fold_gen;
}

bool facet_fold_memo_enabled() { return fold_memo_flag(); }

double drawn_radius_at(const HeightField& hf, glm::dvec3 d, int N, int tiles) {
    // Unbound (a test, a procedural planet, SEADS_NO_SNOWFOLD) falls through to
    // the terrain facet -- bit-identical to pre-R1, which is what makes the A/B
    // honest rather than approximate.
    return facet_radius_impl(hf, d, N, tiles, g_drawn_fold);
}

FaceMesh fill_face(const HeightField& hf, int f, int N, int tile_x, int tile_y,
                   int tiles, const std::vector<CutDisk>& cuts, int split_depth,
                   const world::SnowpackField* fold) {
    if (N < 2) N = 2;
    if (N > 256)
        N = 256;  // ushort index cap (256*256 <= 65536); pure-fn contract
    if (tiles < 1) tiles = 1;
    tile_x = tile_x < 0 ? 0 : (tile_x >= tiles ? tiles - 1 : tile_x);
    tile_y = tile_y < 0 ? 0 : (tile_y >= tiles ? tiles - 1 : tile_y);
    FaceMesh m;
    const int vcount = N * N;
    const int tcount = 2 * (N - 1) * (N - 1);
    m.positions.resize(static_cast<std::size_t>(vcount) * 3);
    m.normals.resize(static_cast<std::size_t>(vcount) * 3);
    m.indices.resize(static_cast<std::size_t>(tcount) * 3);
    // ★ R3: sized ONLY when a fold is bound. Left empty otherwise so a consumer
    // reads "no depth channel" rather than a plausible-looking field of zeros
    // that would shade the whole planet as bare ground (see FaceMesh::depths).
    if (fold != nullptr) m.depths.assign(static_cast<std::size_t>(vcount), 0.0f);
    // T2 portal surgery: per-vertex "inside a mouth cut" mask (all false when
    // cuts is empty — the bit-identical no-op). VERTICES are still all emitted;
    // only the index loop drops a triangle any of whose 3 verts is inside.
    std::vector<char> cut_v;
    // T25b: per-vertex direction, retained so the subdividing cut trim can test
    // a facet's bounding cap against the cut disks without re-normalizing floats.
    std::vector<glm::dvec3> vdir;
    if (!cuts.empty()) {
        cut_v.assign(static_cast<std::size_t>(vcount), 0);
        vdir.assign(static_cast<std::size_t>(vcount), glm::dvec3(0.0));
    }

    const FaceBasis b = face_basis(f);
    // R4d tiling: the face's EFFECTIVE grid is tiles*(N-1) cells per edge; this
    // tile covers global indices [tile*(N-1), tile*(N-1)+N-1]. Every vertex
    // param derives from its GLOBAL index through this ONE double expression,
    // so a shared tile-edge vertex (same global index) is bit-identical in
    // both tiles — watertight, no seam crack. eps (the normal probe) is the
    // EFFECTIVE grid spacing so normals agree across the shared edge too.
    const double ecells = static_cast<double>(tiles) * (N - 1);
    const double eps = 2.0 / ecells;
    // ★ R1: the ONE surface expression this mesh is laid on. The normal
    // probes below read it too, so the folded relief shades itself.
    auto surf = [&](glm::dvec3 dir) {
        const double snow = fold != nullptr ? fold->draw_fold_at(dir) : 0.0;
        return dir * (hf.radius_at(dir) + snow);
    };

    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            // Tangent-warp the grid so texel/area density is near-uniform.
            const int gi = tile_x * (N - 1) + i;
            const int gj = tile_y * (N - 1) + j;
            const double s = warp(-1.0 + 2.0 * gi / ecells);
            const double t = warp(-1.0 + 2.0 * gj / ecells);
            const glm::dvec3 d = glm::normalize(b.n + b.r * s + b.u * t);
            // ★ R3: ONE field call, BOTH drawn channels. `fold_m` displaces the
            // vertex (bit-identical to the pre-R3 draw_fold_at call it
            // replaces); `ambient_m` rides out as the per-vertex shading depth.
            // Taking them from one call is the anti-fork -- the surface the eye
            // is lit by and the surface it sits on cannot disagree about the
            // snowpack except through the corridor mask, deliberately.
            const world::SnowpackField::DrawSample ds =
                fold != nullptr ? fold->draw_sample_at(d)
                                : world::SnowpackField::DrawSample{};
            const double rad = hf.radius_at(d) + ds.fold_m;
            const int vi = j * N + i;
            if (fold != nullptr)
                m.depths[static_cast<std::size_t>(vi)] =
                    static_cast<float>(ds.ambient_m);
            if (!cuts.empty()) {
                cut_v[static_cast<std::size_t>(vi)] =
                    dir_in_any_cut(d, hf.R, cuts) ? 1 : 0;
                vdir[static_cast<std::size_t>(vi)] = d;
            }
            m.positions[vi * 3 + 0] = static_cast<float>(d.x * rad);
            m.positions[vi * 3 + 1] = static_cast<float>(d.y * rad);
            m.positions[vi * 3 + 2] = static_cast<float>(d.z * rad);

            // Analytic normal via a CENTRAL difference at ~mesh-quad angular
            // spacing (a one-sided or finer step amplifies sub-quad detail into
            // salt-and-pepper shading normals on steep terrain). Continuous
            // across the 12 cube-edge seams (depends only on d + the field).
            const glm::dvec3 helper = std::fabs(d.y) < 0.99
                                          ? glm::dvec3(0, 1, 0)
                                          : glm::dvec3(1, 0, 0);
            const glm::dvec3 t1 = glm::normalize(glm::cross(helper, d));
            const glm::dvec3 t2 = glm::cross(d, t1);
            const glm::dvec3 ta = surf(glm::normalize(d + t1 * eps)) -
                                  surf(glm::normalize(d - t1 * eps));
            const glm::dvec3 tb = surf(glm::normalize(d + t2 * eps)) -
                                  surf(glm::normalize(d - t2 * eps));
            glm::dvec3 nrm = glm::normalize(glm::cross(ta, tb));
            if (glm::dot(nrm, d) < 0) nrm = -nrm;
            m.normals[vi * 3 + 0] = static_cast<float>(nrm.x);
            m.normals[vi * 3 + 1] = static_cast<float>(nrm.y);
            m.normals[vi * 3 + 2] = static_cast<float>(nrm.z);
        }
    }

    // ---- index generation ------------------------------------------------
    // No cuts: the BIT-IDENTICAL no-op fast path — every triangle emitted in
    // grid order into the pre-sized index array (positions/normals untouched).
    // This is the pre-T2 fill byte-for-byte; all non-tunnel terrain hits it.
    if (cuts.empty()) {
        int k = 0;
        for (int j = 0; j < N - 1; ++j) {
            for (int i = 0; i < N - 1; ++i) {
                const unsigned short v00 =
                    static_cast<unsigned short>(j * N + i);
                const unsigned short v10 =
                    static_cast<unsigned short>(j * N + i + 1);
                const unsigned short v01 =
                    static_cast<unsigned short>((j + 1) * N + i);
                const unsigned short v11 =
                    static_cast<unsigned short>((j + 1) * N + i + 1);
                m.indices[k++] = v00;
                m.indices[k++] = v10;
                m.indices[k++] = v11;
                m.indices[k++] = v00;
                m.indices[k++] = v11;
                m.indices[k++] = v01;
            }
        }
        m.indices.resize(static_cast<std::size_t>(k));
        return m;
    }

    // Cut terrain: SUBDIVIDING trim (T25b) — the pre-T25b any-corner drop left a
    // facet whose 3 corners all sit OUTSIDE a cut disk but whose interior the
    // disk chords across HOVERING WHOLE over the pit (Chad's round-17 Errington
    // "terrain cover sheet"). Mirror drop_cut_swallowed_tris (tunnel_mesh.cpp):
    //  * a facet DISJOINT from every cut is kept whole via its original shared
    //    vertices (the cheap common case — no new verts, no cracked edge);
    //  * a facet fully inside a cut (all 3 corners in a disk) is dropped;
    //  * a facet a disk TOUCHES is recurse-subdivided at edge midpoints to
    //    `split_depth`, and only fully-inside LEAVES drop (a mixed leaf is kept
    //    — over-cover by up to a leaf, never a void).
    // dir_in_any_cut is the ONE in-cut predicate (shared, never forked); the
    // touch test below is a conservative bounding-cap SEPARATION (disjoint =>
    // safe to keep whole), not a second in-cut predicate.
    const int max_depth = split_depth < 0 ? 0 : split_depth;
    const double eR = hf.R;
    auto ang = [](const glm::dvec3& a, const glm::dvec3& bb) {
        return std::acos(glm::clamp(glm::dot(a, bb), -1.0, 1.0));
    };
    // The triangle's bounding cap is {centroid dc, rho = max corner angle}; it
    // strictly contains the 3 dirs. A `false` here => the whole facet lies
    // outside every cut cap {c.dir, c.radius_m/R} (safe to keep). This also
    // catches the interior-disk case the corner test misses: a disk centred in
    // the facet gives ang(dc, c.dir) ~ 0 < rho + radius => touch => subdivide.
    auto touches_cut = [&](const glm::dvec3& d0, const glm::dvec3& d1,
                           const glm::dvec3& d2) {
        const glm::dvec3 dc = glm::normalize(d0 + d1 + d2);
        const double rho =
            std::max({ang(dc, d0), ang(dc, d1), ang(dc, d2)});
        for (const CutDisk& c : cuts)
            if (ang(dc, c.dir) <= rho + c.radius_m / eR) return true;
        return false;
    };

    m.indices.clear();
    m.indices.reserve(static_cast<std::size_t>(3) * (N - 1) * (N - 1) * 2);

    struct Leaf {
        glm::dvec3 v[3];
        glm::dvec3 n[3];
        int depth;
    };
    std::vector<Leaf> stack, out;
    // ushort index budget: base grid verts fill [0, vcount); appended leaf
    // indices must stay <= 65535. If a facet's subdivided verts would overflow,
    // keep it WHOLE (over-cover) rather than overflow or void — defensive: the
    // clustered mouth cuts spend a few thousand of ~25k headroom at subdiv 200.
    const std::size_t kIdxCap = 65536;

    auto vpos = [&](unsigned short vi) {
        return glm::dvec3(m.positions[vi * 3 + 0], m.positions[vi * 3 + 1],
                          m.positions[vi * 3 + 2]);
    };
    auto vnrm = [&](unsigned short vi) {
        return glm::dvec3(m.normals[vi * 3 + 0], m.normals[vi * 3 + 1],
                          m.normals[vi * 3 + 2]);
    };
    // Subdivide one facet triangle into kept `out` leaves (flat linear midpoints
    // — leaves tile the parent's exact facet, so a T-junction with a kept
    // neighbour covers up to the shared straight edge with no gap; normals are
    // linearly interpolated so the kept region shades identically to the
    // parent's Gouraud fill).
    auto subdivide = [&](unsigned short ia, unsigned short ib,
                         unsigned short ic) {
        out.clear();
        stack.clear();
        stack.push_back({{vpos(ia), vpos(ib), vpos(ic)},
                         {vnrm(ia), vnrm(ib), vnrm(ic)},
                         0});
        while (!stack.empty()) {
            const Leaf L = stack.back();
            stack.pop_back();
            const glm::dvec3 d0 = glm::normalize(L.v[0]);
            const glm::dvec3 d1 = glm::normalize(L.v[1]);
            const glm::dvec3 d2 = glm::normalize(L.v[2]);
            const int lin = (dir_in_any_cut(d0, eR, cuts) ? 1 : 0) +
                            (dir_in_any_cut(d1, eR, cuts) ? 1 : 0) +
                            (dir_in_any_cut(d2, eR, cuts) ? 1 : 0);
            if (lin == 3) continue;  // swallowed leaf: drop
            if (!touches_cut(d0, d1, d2)) {
                out.push_back(L);  // fully outside every cut: keep
                continue;
            }
            if (L.depth >= max_depth) {
                out.push_back(L);  // mixed at the floor: keep (over-cover)
                continue;
            }
            const glm::dvec3 m01 = 0.5 * (L.v[0] + L.v[1]);
            const glm::dvec3 m12 = 0.5 * (L.v[1] + L.v[2]);
            const glm::dvec3 m20 = 0.5 * (L.v[2] + L.v[0]);
            const glm::dvec3 n01 = glm::normalize(L.n[0] + L.n[1]);
            const glm::dvec3 n12 = glm::normalize(L.n[1] + L.n[2]);
            const glm::dvec3 n20 = glm::normalize(L.n[2] + L.n[0]);
            const int d = L.depth + 1;
            stack.push_back({{L.v[0], m01, m20}, {L.n[0], n01, n20}, d});
            stack.push_back({{m01, L.v[1], m12}, {n01, L.n[1], n12}, d});
            stack.push_back({{m20, m12, L.v[2]}, {n20, n12, L.n[2]}, d});
            stack.push_back({{m01, m12, m20}, {n01, n12, n20}, d});
        }
    };

    for (int j = 0; j < N - 1; ++j) {
        for (int i = 0; i < N - 1; ++i) {
            const unsigned short v00 = static_cast<unsigned short>(j * N + i);
            const unsigned short v10 =
                static_cast<unsigned short>(j * N + i + 1);
            const unsigned short v01 =
                static_cast<unsigned short>((j + 1) * N + i);
            const unsigned short v11 =
                static_cast<unsigned short>((j + 1) * N + i + 1);
            // The cell's two triangles split on the v00-v11 diagonal (the
            // fill_face index order — kept identical so the kept-whole facets
            // reproduce the pre-cut mesh exactly).
            const std::array<std::array<unsigned short, 3>, 2> tris = {
                {{v00, v10, v11}, {v00, v11, v01}}};
            for (const std::array<unsigned short, 3>& t : tris) {
                const glm::dvec3 d0 = vdir[t[0]], d1 = vdir[t[1]],
                                 d2 = vdir[t[2]];
                if (!touches_cut(d0, d1, d2)) {  // disjoint: keep whole
                    m.indices.push_back(t[0]);
                    m.indices.push_back(t[1]);
                    m.indices.push_back(t[2]);
                    continue;
                }
                const int nin = cut_v[t[0]] + cut_v[t[1]] + cut_v[t[2]];
                if (nin == 3) continue;  // fully inside a cut: drop
                subdivide(t[0], t[1], t[2]);
                if (out.empty()) continue;  // every leaf swallowed: drop facet
                const std::size_t vcount_now = m.positions.size() / 3;
                if (vcount_now + out.size() * 3 > kIdxCap) {
                    // Budget guard: keep the whole facet rather than overflow
                    // the ushort index (over-cover, never a void).
                    m.indices.push_back(t[0]);
                    m.indices.push_back(t[1]);
                    m.indices.push_back(t[2]);
                    continue;
                }
                for (const Leaf& L : out) {
                    const unsigned int base =
                        static_cast<unsigned int>(m.positions.size() / 3);
                    for (int c = 0; c < 3; ++c) {
                        m.positions.push_back(static_cast<float>(L.v[c].x));
                        m.positions.push_back(static_cast<float>(L.v[c].y));
                        m.positions.push_back(static_cast<float>(L.v[c].z));
                        m.normals.push_back(static_cast<float>(L.n[c].x));
                        m.normals.push_back(static_cast<float>(L.n[c].y));
                        m.normals.push_back(static_cast<float>(L.n[c].z));
                        // ★ R3: the trim EMITS NEW VERTICES, so the depth
                        // channel has to grow with them or it desyncs from
                        // positions -- and a desynced channel is dropped whole
                        // by upload_face, which would render every cut face
                        // (the tunnel mouths) as BARE GROUND once the
                        // depth-keyed exposure is armed. Sampled at the leaf's
                        // own direction, the same field the corner verts used.
                        if (fold != nullptr)
                            m.depths.push_back(static_cast<float>(
                                fold->draw_sample_at(glm::normalize(L.v[c]))
                                    .ambient_m));
                        m.indices.push_back(
                            static_cast<unsigned short>(base + c));
                    }
                }
            }
        }
    }
    return m;
}

}  // namespace render
