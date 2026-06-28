#include "kernel.h"
#include "../det_math/det_math.h"
#include <cstring>

namespace seads {
using namespace seads::detm;

static inline double clampd(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Closed-form intrinsic-S2 great-circle step. det_math only. Mirrors ref_kernel.py.
static void great_circle_step(double lat, double lon, double bearing, double s, double R,
                              double* lat2_out, double* lon2_out) {
    double alpha = s / R;
    double sinlat1 = det_sin(lat);
    double coslat1 = det_cos(lat);
    double ca = det_cos(alpha);
    double sa = det_sin(alpha);
    double cb = det_cos(bearing);
    double sb = det_sin(bearing);
    double sin_lat2 = sinlat1 * ca + coslat1 * sa * cb;
    sin_lat2 = clampd(sin_lat2, -1.0, 1.0);
    double lat2 = det_asin(sin_lat2);
    double y = sb * sa * coslat1;
    double x = ca - sinlat1 * sin_lat2;
    double lon2 = wrap_pi(lon + det_atan2(y, x));
    *lat2_out = lat2;
    *lon2_out = lon2;
}

static double ceiling_climb_rate(double req, double alt, double atm_top, double soft) {
    if (req <= 0.0) return req;
    double band_lo = atm_top - soft;
    if (alt >= band_lo) {
        double frac = (atm_top - alt) / soft;
        frac = clampd(frac, 0.0, 1.0);
        return req * frac;
    }
    return req;
}

std::size_t Kernel::add(double lat, double lon, double psi, double phi, double alt, double tas) {
    lat_.push_back(lat); lon_.push_back(lon); psi_.push_back(psi);
    phi_.push_back(phi); alt_.push_back(alt); tas_.push_back(tas);
    return lat_.size() - 1;
}

void Kernel::step() {
    const double dt = rails_.dt, R = rails_.R, g0 = rails_.g0;
    for (std::size_t i = 0; i < lat_.size(); ++i) {
        double V = tas_[i];
        double psi_dot = g0 * det_tan(phi_[i]) / V;
        psi_[i] = wrap_2pi(psi_[i] + psi_dot * dt);
        double s = V * dt;
        double nlat, nlon;
        great_circle_step(lat_[i], lon_[i], psi_[i], s, R, &nlat, &nlon);
        lat_[i] = nlat;
        lon_[i] = nlon;
        double req = 0.0;  // golden has no climb input
        double rate = ceiling_climb_rate(req, alt_[i], rails_.atm_top, rails_.soft);
        alt_[i] = clampd(alt_[i] + rate * dt, 0.0, rails_.atm_top);
    }
}

void Kernel::run(std::uint32_t ticks) {
    for (std::uint32_t t = 0; t < ticks; ++t) step();
}

static void put_u16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
static void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
static void put_f64(std::vector<std::uint8_t>& b, double d) {
    std::uint8_t tmp[8];
    std::memcpy(tmp, &d, 8);  // little-endian target (x64/AArch64)
    for (int i = 0; i < 8; ++i) b.push_back(tmp[i]);
}

std::vector<std::uint8_t> Kernel::snapshot(std::uint32_t tick_count) const {
    std::vector<std::uint8_t> b;
    b.reserve(32 + 48 * lat_.size());
    put_u16(b, 1);                 // mode = ATM
    put_u16(b, 0);                 // pad
    put_u32(b, tick_count);        // tick_count
    put_f64(b, rails_.dt);         // dt_s
    put_f64(b, rails_.R);          // R_m
    put_u32(b, static_cast<std::uint32_t>(lat_.size()));  // n_aircraft
    put_u32(b, 0);                 // pad
    for (std::size_t i = 0; i < lat_.size(); ++i) {
        put_f64(b, lat_[i]); put_f64(b, lon_[i]); put_f64(b, psi_[i]);
        put_f64(b, phi_[i]); put_f64(b, alt_[i]); put_f64(b, tas_[i]);
    }
    return b;
}

}  // namespace seads
