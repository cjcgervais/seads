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

// G1 ballistic-projectile constants (Step 7 guns, ATM-Sphere v1.9r0). Exact hex-float literals
// shared bit-for-bit with tools/ref_kernel.py. A round is the n=0/thrust=0 specialization of the
// aircraft 3-DOF step (gravity along the path + lumped quadratic drag), so NO new det_math. Global
// for G1 (a generic gun); per-airframe weapon rosters are G3. See ADR-Step7-Guns-G1.
// G3 (v1.11r0): muzzle velocity and damage-per-round are PER-AIRFRAME (Envelope::muzzle_v_mps /
// damage_per_round); drag and ttl stay GLOBAL (a bullet is a bullet). Shared hex-floats with
// tools/ref_kernel.py.
static constexpr double        PROJ_DRAG_K    = 0x1.a36e2eb1c432dp-13;   // 2.0e-4 quadratic drag decel coeff
static constexpr std::uint32_t PROJ_TTL_TICKS = 250u;                    // 2.5 s lifetime, then despawn

// G2 hit detection + per-aircraft hitpoints (Step 7 guns, ATM-Sphere v1.10r0). Shared hex-floats
// with ref_kernel.py. Horizontal hit test is the spherical law of cosines vs COS_HIT_ANGLE (=
// det_cos(HIT_RADIUS/R), precomputed once) — acos is monotone so no det_acos is needed; with the
// |Δalt| gate it is a cylinder test using det_sin/det_cos only (NO new det_math). G3 (v1.11r0):
// hp_start / damage are per-airframe; START_HP remains the GLOBAL default for the no-arg/Sphere path.
static constexpr double START_HP         = 0x1.9000000000000p+6;   // 100.0 default hitpoints (no-arg/Sphere)
static constexpr double HIT_ALT_GATE_M   = 0x1.e000000000000p+5;   // 60.0 m vertical hit gate
static constexpr double COS_HIT_ANGLE    = 0x1.fffef3909d697p-1;   // cos(HIT_RADIUS/R); horizontal hit test

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
                        double gamma, double hp) {
    lat_.push_back(lat); lon_.push_back(lon); psi_.push_back(psi);
    phi_.push_back(phi); alt_.push_back(alt); tas_.push_back(tas); gamma_.push_back(gamma);
    hp_.push_back(hp);                           // G2 hitpoints (G3: per-airframe hp_start passed in)
    fire_cd_.push_back(0.0);                     // G3 (v1.11r0): fire-rate cooldown starts ready
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

// G2 (v1.10r0): index of the first ALIVE enemy aircraft the round p_idx hits this tick, else -1.
// Horizontal great-circle within HIT_RADIUS via the law of cosines (cosc > COS_HIT_ANGLE; acos is
// monotone so no det_acos) AND |Δalt| < HIT_ALT_GATE_M, excluding the firer. Array order. det_sin/
// det_cos + (+,-,*,/) only. MUST match ref_kernel._projectile_hit bit-for-bit.
std::ptrdiff_t Kernel::projectile_hit_(std::size_t i) const {
    double psin = det_sin(p_lat_[i]);
    double pcos = det_cos(p_lat_[i]);
    for (std::size_t j = 0; j < lat_.size(); ++j) {
        if (hp_[j] <= 0.0 || j == p_owner_[i]) continue;
        double cosc = psin * det_sin(lat_[j]) + pcos * det_cos(lat_[j]) * det_cos(lon_[j] - p_lon_[i]);
        if (cosc > COS_HIT_ANGLE) {
            double dalt = p_alt_[i] - alt_[j];
            if (dalt < 0.0) dalt = -dalt;
            if (dalt < HIT_ALT_GATE_M) return static_cast<std::ptrdiff_t>(j);
        }
    }
    return -1;
}

// G1 (v1.9r0): step every live round one tick (ballistic n=0/thrust=0 point mass), then despawn the
// expired/grounded ones via in-place forward compaction (array order = deterministic, no pointer
// dependence). G2 (v1.10r0): also resolve hits — a round that strikes an alive enemy deals damage
// and despawns. Op order MUST match ref_kernel._advance_projectiles bit-for-bit.
void Kernel::advance_projectiles_() {
    const double dt = rails_.dt, R = rails_.R, g0 = rails_.g0, atm_top = rails_.atm_top;
    std::size_t w = 0;                              // write cursor for the survivors (w <= i always)
    for (std::size_t i = 0; i < p_lat_.size(); ++i) {
        double V = p_tas_[i];
        double sg = det_sin(p_gamma_[i]);
        double cg = det_cos(p_gamma_[i]);
        double Vdot = -PROJ_DRAG_K * V * V - g0 * sg;   // lumped quadratic drag + gravity along path
        double Vnew = V + Vdot * dt;
        if (Vnew < V_MIN) Vnew = V_MIN;
        double gdot = (g0 / Vnew) * (-cg);              // n=0 -> gamma bends down under gravity
        double ngamma = p_gamma_[i] + gdot * dt;
        // psi unchanged (ballistic: no turn force)
        double cgN = det_cos(ngamma);
        double s = Vnew * cgN * dt;
        double nlat, nlon;
        great_circle_step(p_lat_[i], p_lon_[i], p_psi_[i], s, R, &nlat, &nlon);
        double sgN = det_sin(ngamma);
        double wv = Vnew * sgN;
        double nalt = p_alt_[i] + wv * dt;
        bool hit_ground = nalt <= 0.0;
        if (nalt < 0.0) nalt = 0.0;
        if (nalt > atm_top) nalt = atm_top;
        std::uint32_t nttl = p_ttl_[i] - 1u;            // ttl >= 1 on entry (despawned at 0)
        // commit the moved round into slot i, then resolve a hit against the NEW position
        p_lat_[i] = nlat; p_lon_[i] = nlon; p_alt_[i] = nalt;
        p_tas_[i] = Vnew; p_gamma_[i] = ngamma; p_ttl_[i] = nttl;
        std::ptrdiff_t hit_ac = projectile_hit_(i);     // G2: first alive enemy struck, else -1
        if (hit_ac >= 0) {
            double nhp = hp_[static_cast<std::size_t>(hit_ac)] - p_damage_[i];  // G3: carried damage
            if (nhp < 0.0) nhp = 0.0;
            hp_[static_cast<std::size_t>(hit_ac)] = nhp;
        }
        if (nttl > 0u && !hit_ground && hit_ac < 0) {   // survivor: write compacted into slot w
            p_lat_[w] = p_lat_[i]; p_lon_[w] = p_lon_[i]; p_psi_[w] = p_psi_[i];
            p_alt_[w] = p_alt_[i]; p_tas_[w] = p_tas_[i]; p_gamma_[w] = p_gamma_[i];
            p_damage_[w] = p_damage_[i]; p_ttl_[w] = p_ttl_[i]; p_owner_[w] = p_owner_[i];
            ++w;
        }
    }
    p_lat_.resize(w); p_lon_.resize(w); p_psi_.resize(w); p_alt_.resize(w);
    p_tas_.resize(w); p_gamma_.resize(w); p_damage_.resize(w); p_ttl_.resize(w); p_owner_.resize(w);
}

void Kernel::spawn_projectile_(std::size_t owner, const Envelope& e) {
    p_lat_.push_back(lat_[owner]);   p_lon_.push_back(lon_[owner]);   p_psi_.push_back(psi_[owner]);
    p_alt_.push_back(alt_[owner]);   p_tas_.push_back(tas_[owner] + e.muzzle_v_mps);  // G3: per-airframe muzzle
    p_gamma_.push_back(gamma_[owner]);
    p_damage_.push_back(e.damage_per_round);          // G3: carried per-round damage from the firer's gun
    p_ttl_.push_back(PROJ_TTL_TICKS); p_owner_.push_back(static_cast<std::uint32_t>(owner));
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
        if (hp_[i] <= 0.0) continue;            // G2 (v1.10r0): a DEAD aircraft freezes (no integration)
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
    // G1/G2/G3 guns: advance live rounds (resolving hits), then spawn newly-fired ones from the
    // post-step muzzle, gated by the per-airframe fire-rate. Order (step+hit+despawn, THEN appends)
    // and the decrement-then-fire cooldown mirror ref_kernel.step_scenario exactly.
    advance_projectiles_();
    for (std::size_t i = 0; i < lat_.size(); ++i) {
        if (fire_cd_[i] > 0.0) fire_cd_[i] = fire_cd_[i] - 1.0;
        if (cmd[i].fire && hp_[i] > 0.0 && fire_cd_[i] == 0.0) {   // ready, alive, trigger held
            spawn_projectile_(i, *env[i]);
            fire_cd_[i] = env[i]->rof_interval_ticks;             // G3: reset to the per-airframe interval
        }
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
    b.reserve(32 + 72 * lat_.size() + 8 + 64 * p_lat_.size());   // hdr + 9f64/ac + projblock(7f64+2u32)
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
        put_f64(b, hp_[i]);                          // G2 (v1.10r0): 8th per-aircraft f64
        put_f64(b, fire_cd_[i]);                     // G3 (v1.11r0): 9th per-aircraft f64 (fire cooldown)
    }
    // G1 (v1.9r0): projectile block — u32 n_projectiles, u32 pad, then per round 7 x f64
    // [lat, lon, psi, alt, tas, gamma, damage] + u32 ttl + u32 owner (damage added in G3 v1.11r0).
    // Always present (n=0 for gun-less scenarios). Mirrors ref_kernel.snapshot byte-for-byte.
    put_u32(b, static_cast<std::uint32_t>(p_lat_.size()));
    put_u32(b, 0);
    for (std::size_t i = 0; i < p_lat_.size(); ++i) {
        put_f64(b, p_lat_[i]); put_f64(b, p_lon_[i]); put_f64(b, p_psi_[i]);
        put_f64(b, p_alt_[i]); put_f64(b, p_tas_[i]); put_f64(b, p_gamma_[i]); put_f64(b, p_damage_[i]);
        put_u32(b, p_ttl_[i]); put_u32(b, p_owner_[i]);
    }
    return b;
}

}  // namespace seads
