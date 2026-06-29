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

// B1 longitudinal-energy model constants (ATM-Sphere v1.5r0). Exact hex-float literals shared
// bit-for-bit with tools/ref_kernel.py (RHO0/V_MIN). RHO0: constant sea-level ISA air density
// (constant-atmosphere rail; ISA-vs-altitude is B5). V_MIN: hard speed floor (real stall = B3).
static constexpr double RHO0  = 0x1.399999999999ap+0;   // 1.225 kg/m^3
static constexpr double V_MIN = 0x1.e000000000000p+4;   // 30.0 m/s

// B3 limits & stall (ATM-Sphere v1.7r0): the B2 global placeholder clamp (N_MIN=-3, N_MAX=+9) is
// RETIRED. The achievable load factor n is now bounded per-airframe by BOTH a structural limit
// (Envelope::n_min_struct/n_max_struct) AND the C_Lmax aerodynamic ceiling
// n_aero = cl_max*qS/(m*g0) (the most lift the wing can make at the current dynamic pressure;
// below the corner speed this is binding and the turn collapses = accelerated stall). No new
// det_math (+,-,*,/ only). Mirrors tools/ref_kernel.py step_scenario. See ADR-Step8-FlightModel-B3.

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

// Clamped piecewise-linear interpolation over a 5-point LUT. det_math: only + - * / and exact
// IEEE comparisons (no FMA). Op order MUST match detmath_ref.lut_eval bit-for-bit.
static inline double lut_eval(const double* xs, const double* ys, double x) {
    if (x <= xs[0]) return ys[0];
    if (x >= xs[4]) return ys[4];
    int i = 0;
    while (x >= xs[i + 1]) ++i;
    double t = (x - xs[i]) / (xs[i + 1] - xs[i]);
    return ys[i] + (ys[i + 1] - ys[i]) * t;
}

std::size_t Kernel::add(double lat, double lon, double psi, double phi, double alt, double tas,
                        double gamma) {
    lat_.push_back(lat); lon_.push_back(lon); psi_.push_back(psi);
    phi_.push_back(phi); alt_.push_back(alt); tas_.push_back(tas); gamma_.push_back(gamma);
    return lat_.size() - 1;
}

void Kernel::advance_(std::size_t i, double req) {
    const double dt = rails_.dt, R = rails_.R, g0 = rails_.g0;
    double V = tas_[i];
    double psi_dot = g0 * det_tan(phi_[i]) / V;
    psi_[i] = wrap_2pi(psi_[i] + psi_dot * dt);
    double s = V * dt;
    double nlat, nlon;
    great_circle_step(lat_[i], lon_[i], psi_[i], s, R, &nlat, &nlon);
    lat_[i] = nlat;
    lon_[i] = nlon;
    double rate = ceiling_climb_rate(req, alt_[i], rails_.atm_top, rails_.soft);
    alt_[i] = clampd(alt_[i] + rate * dt, 0.0, rails_.atm_top);
}

void Kernel::step() {                       // straight golden: req=0, phi unchanged -> byte-identical
    for (std::size_t i = 0; i < lat_.size(); ++i) advance_(i, 0.0);
}

void Kernel::step(const std::vector<Command>& cmd, const std::vector<const Envelope*>& env) {
    // B3 (v1.7r0): the commanded load factor n is bounded per-airframe by the structural g limits
    // AND the C_Lmax aerodynamic ceiling (n_aero); below the corner speed the turn collapses as
    // speed bleeds (accelerated stall). Retires the B2 global [-3,9] clamp. No new det_math.
    // B2 (v1.6r0): full 3-DOF point-mass step. Pitch is real — flight-path angle gamma is a stored
    // state, driven by the commanded load factor n (g-command) through the lift vector; altitude is
    // earned (alt = V*sin gamma). Op order MUST match tools/ref_kernel.step_scenario bit-for-bit.
    // Strictly generalizes B1: wings level n=1 gamma=0 -> level; n=1/cos(phi) gamma=0 -> the old
    // coordinated-turn law psi_dot=g0*tan(phi)/V. cos(gamma)->0 (vertical) is a documented
    // singularity in psi_dot (scenarios/viewer stay well inside +/-90 deg). See ADR-Step8-B2.
    const double dt = rails_.dt, g0 = rails_.g0;
    const double R = rails_.R, atm_top = rails_.atm_top, soft = rails_.soft;
    for (std::size_t i = 0; i < lat_.size(); ++i) {
        double V = tas_[i];
        const Envelope& e = *env[i];
        // --- bank dynamics (unchanged from B1): slew toward commanded bank at roll_rate(V) ---
        double phimax = lut_eval(e.phi_max.x, e.phi_max.y, V);
        double rollrate = lut_eval(e.roll_rate.x, e.roll_rate.y, V);
        double cmdphi = clampd(cmd[i].target_phi, -phimax, phimax);
        double step_max = rollrate * dt;
        double delta = cmdphi - phi_[i];
        delta = clampd(delta, -step_max, step_max);
        phi_[i] = phi_[i] + delta;
        phi_[i] = clampd(phi_[i], -phimax, phimax);
        // --- dynamic pressure (depends only on V; needed for the aero stall ceiling) ---
        double q = 0.5 * RHO0 * V * V;                  // dynamic pressure
        double qS = q * e.wing_area_m2;
        // --- commanded load factor n, bounded by structural g AND C_Lmax (B3, v1.7r0) ---
        // n_aero = most |n| the wing can lift at this q; below the corner speed it is the binding
        // limit and the turn collapses (accelerated stall). Retires the B2 [-3,9] placeholder.
        double n_aero = e.cl_max * qS / (e.mass_kg * g0);
        double n_hi = e.n_max_struct;
        if (n_aero < n_hi) n_hi = n_aero;
        double n_lo = e.n_min_struct;
        double neg_aero = -n_aero;
        if (neg_aero > n_lo) n_lo = neg_aero;
        double n = clampd(cmd[i].target_g, n_lo, n_hi);
        // --- trig of NEW phi and OLD gamma (single eval, fixed order) ---
        double cphi = det_cos(phi_[i]);
        double sphi = det_sin(phi_[i]);
        double cg = det_cos(gamma_[i]);
        double sg = det_sin(gamma_[i]);
        // --- drag/thrust with current V and load factor n (B1 algebra; reuses q, qS above) ---
        double L = n * e.mass_kg * g0;                  // lift = n * weight
        double CL = L / qS;
        double Dp = qS * e.cd0;                         // parasitic drag
        double Di = e.induced_k * CL * CL * qS;         // induced drag (rises with n -> g bleeds speed)
        double D = Dp + Di;
        double thr = clampd(cmd[i].throttle, 0.0, 1.0);
        double T = thr * e.thrust_static_n * (1.0 - V / e.v_max_mps);
        if (T < 0.0) T = 0.0;
        // --- speed: gravity now acts along the flight path (uses OLD gamma) ---
        double Vdot = (T - D) / e.mass_kg - g0 * sg;
        double Vnew = V + Vdot * dt;
        if (Vnew < V_MIN) Vnew = V_MIN;
        tas_[i] = Vnew;
        // --- flight-path angle integrates (uses Vnew, OLD gamma), then track heading turns ---
        double gdot = (g0 / Vnew) * (n * cphi - cg);
        gamma_[i] = gamma_[i] + gdot * dt;
        double psidot = (g0 / Vnew) * (n * sphi / cg);
        psi_[i] = wrap_2pi(psi_[i] + psidot * dt);
        // --- horizontal great-circle advance: ground speed = Vnew*cos(NEW gamma) ---
        double cgN = det_cos(gamma_[i]);
        double s = Vnew * cgN * dt;
        double nlat, nlon;
        great_circle_step(lat_[i], lon_[i], psi_[i], s, R, &nlat, &nlon);
        lat_[i] = nlat;
        lon_[i] = nlon;
        // --- altitude EARNED: vertical rate Vnew*sin(NEW gamma), ceiling predamp + clamp ---
        double sgN = det_sin(gamma_[i]);
        double w = Vnew * sgN;
        double w_eff = ceiling_climb_rate(w, alt_[i], atm_top, soft);
        alt_[i] = clampd(alt_[i] + w_eff * dt, 0.0, atm_top);
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
    b.reserve(32 + 56 * lat_.size());
    put_u16(b, 1);                 // mode = ATM
    put_u16(b, 0);                 // pad
    put_u32(b, tick_count);        // tick_count
    put_f64(b, rails_.dt);         // dt_s
    put_f64(b, rails_.R);          // R_m
    put_u32(b, static_cast<std::uint32_t>(lat_.size()));  // n_aircraft
    put_u32(b, 0);                 // pad
    for (std::size_t i = 0; i < lat_.size(); ++i) {
        put_f64(b, lat_[i]); put_f64(b, lon_[i]); put_f64(b, psi_[i]);
        put_f64(b, phi_[i]); put_f64(b, alt_[i]); put_f64(b, tas_[i]); put_f64(b, gamma_[i]);
    }
    return b;
}

}  // namespace seads
