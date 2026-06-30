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

    // returns index of the new aircraft. gamma (flight-path angle, rad) defaults to 0 (level);
    // hp (G2 hitpoints) defaults to 100.0 == START_HP (scenarios pass the per-airframe hp_start).
    std::size_t add(double lat, double lon, double psi, double phi, double alt, double tas,
                    double gamma = 0.0, double hp = 100.0);

    void step();                  // straight golden: pure kinematic tail, phi/gamma unchanged
    // envelope-driven step (B2): cmd[i] = per-aircraft (target_phi, target_g, throttle); env[i] =
    // its envelope. Full 3-DOF point mass: gamma is a real state, altitude is earned.
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
    double gamma(std::size_t i) const { return gamma_[i]; }
    double hp(std::size_t i) const { return hp_[i]; }   // G2 (v1.10r0): hitpoints; hp<=0 == dead
    double fire_cd(std::size_t i) const { return fire_cd_[i]; }  // G3 (v1.11r0): fire-rate cooldown

    // G1 (v1.9r0) ballistic projectiles — read-only accessors (the renderer/tests inspect them; the
    // sim spawns them internally on a fire Command). SoA, deterministic array iteration order.
    std::size_t proj_count() const { return p_lat_.size(); }
    double proj_lat(std::size_t i) const { return p_lat_[i]; }
    double proj_lon(std::size_t i) const { return p_lon_[i]; }
    double proj_psi(std::size_t i) const { return p_psi_[i]; }
    double proj_alt(std::size_t i) const { return p_alt_[i]; }
    double proj_tas(std::size_t i) const { return p_tas_[i]; }
    double proj_gamma(std::size_t i) const { return p_gamma_[i]; }
    double proj_damage(std::size_t i) const { return p_damage_[i]; }   // G3 (v1.11r0): carried damage
    std::uint32_t proj_ttl(std::size_t i) const { return p_ttl_[i]; }
    std::uint32_t proj_owner(std::size_t i) const { return p_owner_[i]; }

private:
    // Kinematic tail for aircraft i (coordinated-turn + great-circle + ceiling-clamped vertical).
    // phi_[i] must already be final for this tick. Verbatim ops shared by both step() overloads.
    void advance_(std::size_t i, double req);
    // G1 (v1.9r0): step every live round one tick (ballistic n=0/thrust=0 point mass) then despawn
    // expired/grounded rounds; spawn one round from aircraft i's post-step muzzle. Mirror ref_kernel.
    // G2 (v1.10r0): advance_projectiles_ also resolves hits (damage + despawn on hit).
    void advance_projectiles_();
    // G3 (v1.11r0): spawn carries the firer's per-airframe muzzle velocity + per-round damage.
    void spawn_projectile_(std::size_t owner, const Envelope& e);
    // G2: index of the first ALIVE enemy aircraft the projectile p_idx hits this tick (-1 = miss).
    std::ptrdiff_t projectile_hit_(std::size_t p_idx) const;

    Rails rails_;
    // aircraft SoA: 7-tuple kinematics + hp (G2) + fire_cd (G3 fire-rate cooldown)
    std::vector<double> lat_, lon_, psi_, phi_, alt_, tas_, gamma_, hp_, fire_cd_;
    // projectile SoA: kinematic 6-tuple + carried damage (G3) + integer ttl + owner aircraft index
    std::vector<double> p_lat_, p_lon_, p_psi_, p_alt_, p_tas_, p_gamma_, p_damage_;
    std::vector<std::uint32_t> p_ttl_, p_owner_;
};

}  // namespace seads
