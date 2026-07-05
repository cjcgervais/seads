// SEADS presentation-side remote coast (renderer polish) — see remote_coaster.h.
#include "remote_coaster.h"

namespace seads {
namespace client {

Coast7 coast_to_now(const Rails& rails, const Coast7& base, unsigned steps) {
    // Seed a one-aircraft kernel at the reseed base and advance the sealed no-arg kinematic tail —
    // exactly netremote::coast_to_now (layer 24), presentation-side (net code stays outside the
    // kernel: this drives a copy). No envelope needed — the no-arg step holds bank/gamma/speed.
    Kernel k(rails);
    k.add(base.lat, base.lon, base.psi, base.phi, base.alt, base.tas, base.gamma);
    for (unsigned i = 0; i < steps; ++i) k.step();
    return Coast7{k.lat(0), k.lon(0), k.psi(0), k.phi(0), k.alt(0), k.tas(0), k.gamma(0)};
}

namespace {
// Blend `disp` a fraction `s` toward `target`, componentwise — netremote::blend (layer 25). s >= 1.0
// hard-snaps (a + 1*(b-a) is NOT bit-exactly b in IEEE, so the snap is special-cased). Otherwise
// d + s*(t-d): pure IEEE sub/mul/add (no transcendental, no FMA under -ffp-contract=off).
Coast7 blend(const Coast7& d, const Coast7& t, double s) {
    if (s >= 1.0) return t;
    return Coast7{d.lat + s * (t.lat - d.lat), d.lon + s * (t.lon - d.lon),
                  d.psi + s * (t.psi - d.psi), d.phi + s * (t.phi - d.phi),
                  d.alt + s * (t.alt - d.alt), d.tas + s * (t.tas - d.tas),
                  d.gamma + s * (t.gamma - d.gamma)};
}
}  // namespace

Coast7 RemoteCoasterSet::update(std::int64_t id, const Coast7& target, double smooth) {
    for (auto& sl : slots_) {
        if (sl.id == id) {
            sl.disp = blend(sl.disp, target, smooth);
            return sl.disp;
        }
    }
    slots_.push_back(Slot{id, target});  // first sighting seeds to the target (no spawn pop)
    return target;
}

}  // namespace client
}  // namespace seads
