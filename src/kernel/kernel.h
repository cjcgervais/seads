// SEADS deterministic kernel (ATM-Sphere v1.2r0). Mirrors tools/ref_kernel.py.
// Struct-of-arrays state; fixed 100 Hz step; det_math only; no RNG / wall-clock / threads.
#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "flight_types.h"

namespace seads {

struct Rails {
    double R = 15000.0;
    double dt = 0.01;
    double g0 = 9.80665;
    double atm_top = 8000.0;
    double soft = 100.0;
};

class Kernel {
public:
    explicit Kernel(const Rails& r) : rails_(r) {}

    // returns index of the new aircraft
    std::size_t add(double lat, double lon, double psi, double phi, double alt, double tas);

    void step();                  // straight golden: climb input = 0, phi unchanged
    // envelope-driven step: cmd[i] = per-aircraft (target_phi, target_climb); env[i] = its envelope.
    void step(const std::vector<Command>& cmd, const std::vector<const Envelope*>& env);
    void run(std::uint32_t ticks);

    // canonical little-endian snapshot (identical bytes to ref_kernel.py)
    std::vector<std::uint8_t> snapshot(std::uint32_t tick_count) const;

    std::size_t count() const { return lat_.size(); }
    double lat(std::size_t i) const { return lat_[i]; }
    double lon(std::size_t i) const { return lon_[i]; }
    double psi(std::size_t i) const { return psi_[i]; }
    double phi(std::size_t i) const { return phi_[i]; }
    double alt(std::size_t i) const { return alt_[i]; }
    double tas(std::size_t i) const { return tas_[i]; }

private:
    // Kinematic tail for aircraft i (coordinated-turn + great-circle + ceiling-clamped vertical).
    // phi_[i] must already be final for this tick. Verbatim ops shared by both step() overloads.
    void advance_(std::size_t i, double req);

    Rails rails_;
    std::vector<double> lat_, lon_, psi_, phi_, alt_, tas_;
};

}  // namespace seads
