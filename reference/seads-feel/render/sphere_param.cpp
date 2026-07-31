#include "render/sphere_param.h"

#include <cmath>
#include <glm/common.hpp>
#include <glm/geometric.hpp>

namespace render {

namespace {
constexpr double kPi = 3.14159265358979323846;
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

glm::dvec2 equirect_uv(glm::dvec3 d, double u_offset) {
    double u = 0.5 + std::atan2(d.z, d.x) / (2.0 * kPi) + u_offset;
    u -= std::floor(u);  // wrap longitude to [0,1)
    double v = 0.5 - std::asin(glm::clamp(d.y, -1.0, 1.0)) / kPi;
    v = glm::clamp(v, 0.0, 1.0);
    return {u, v};
}

double HeightField::sample01(double u, double v) const {
    const double fx = u * w - 0.5, fy = v * h - 0.5;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const double tx = fx - x0, ty = fy - y0;
    auto at = [&](int x, int y) -> double {
        x = ((x % w) + w) % w;                 // wrap longitude
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);  // clamp latitude
        return px[static_cast<std::size_t>(y) * w + x] / 255.0;
    };
    const double a = at(x0, y0), b = at(x0 + 1, y0);
    const double c = at(x0, y0 + 1), e = at(x0 + 1, y0 + 1);
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + e * tx) * ty;
}

double HeightField::radius_at(glm::dvec3 d) const {
    const glm::dvec2 uv = equirect_uv(d, u_offset);
    return R + sample01(uv.x, uv.y) * relief_scale;
}

FaceMesh fill_face(const HeightField& hf, int f, int N) {
    if (N < 2) N = 2;
    FaceMesh m;
    const int vcount = N * N;
    const int tcount = 2 * (N - 1) * (N - 1);
    m.positions.resize(static_cast<std::size_t>(vcount) * 3);
    m.normals.resize(static_cast<std::size_t>(vcount) * 3);
    m.indices.resize(static_cast<std::size_t>(tcount) * 3);

    const FaceBasis b = face_basis(f);
    const double eps = 2.0 / (N - 1);
    auto surf = [&](glm::dvec3 dir) { return dir * hf.radius_at(dir); };

    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            // Tangent-warp the grid so texel/area density is near-uniform.
            const double s = warp(-1.0 + 2.0 * i / (N - 1));
            const double t = warp(-1.0 + 2.0 * j / (N - 1));
            const glm::dvec3 d = glm::normalize(b.n + b.r * s + b.u * t);
            const double rad = hf.radius_at(d);
            const int vi = j * N + i;
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

    int k = 0;
    for (int j = 0; j < N - 1; ++j) {
        for (int i = 0; i < N - 1; ++i) {
            const unsigned short v00 = static_cast<unsigned short>(j * N + i);
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
    return m;
}

}  // namespace render
