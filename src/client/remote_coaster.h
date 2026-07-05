// SEADS presentation-side REMOTE COAST (renderer polish, rides seal v1.26r0) — bring the netcode
// layer-24 remote prediction + layer-25 reconcile smoothing onto the viewer HUD.
//
// The globe viewer renders remote aircraft by layer-4a INTERPOLATION: it samples the decoded 20 Hz
// wire ~100 ms in the PAST between the two freshest received frames (smooth, but structurally LATE —
// a remote is drawn where it WAS). Netcode layer 24 closes that gap for the network client by
// DEAD-RECKONING a remote to "now" — seed a one-aircraft kernel from the freshest authoritative
// snapshot and advance it with the SEALED no-arg Kernel::step() (the pure kinematic tail that holds
// bank / flight-path angle / speed and propagates the great circle). Layer 25 then BLENDS the drawn
// remote toward each reseed instead of snapping, hiding the maneuver pop.
//
// This header is the PRESENTATION mirror of that core, so the viewer can offer INTERP / PREDICT /
// SMOOTH remote modes and show the coast-vs-interpolation "now" error live. It is a faithful
// re-expression of netremote::coast_to_now + netremote::blend (src/net/remotepredict.cpp), kept in
// the client lib so the headless viewer test can prove the data path without a socket harness.
//
// Boundaries (identical to netremote / the whole client lib): DOWNSTREAM-ONLY. It drives a KERNEL
// COPY through the public no-arg Kernel::step() to extrapolate a display position; it never advances
// the authoritative sim, never hashes, never writes a wire bit. No kernel/det_math/rails/wire/golden
// change — it composes the EXISTING protocol-7 wire + the sealed no-arg kinematic tail (no seal).
// The coast itself is det_math (the no-arg tail); the blend is pure IEEE sub/mul/add. The viewer's
// wall-clock -> tick mapping stays in the caller, so every function here is pure given its inputs.
#pragma once
#include <cstdint>
#include <vector>

#include "kernel.h"  // seads::Kernel, Rails (from the kernel include dir)

namespace seads {
namespace client {

// A remote's kinematic 7-tuple in KERNEL units (radians / metres) — the coast seed and target.
// Mirrors predict::OwnState / the no-arg kernel state without pulling the seads_predict lib.
struct Coast7 {
    double lat = 0, lon = 0, psi = 0, phi = 0, alt = 0, tas = 0, gamma = 0;
};

// Dead-reckon `base` forward `steps` no-arg kernel ticks and return the coasted 7-tuple.
// Presentation mirror of netremote::coast_to_now (layer 24): seed a one-aircraft kernel and advance
// it with the SEALED no-arg Kernel::step(). Pure / headless; `steps == 0` returns `base` unchanged.
Coast7 coast_to_now(const Rails& rails, const Coast7& base, unsigned steps);

// Per-remote display coaster for the layer-25 smoothing blend. Holds each aircraft's DISPLAYED
// (blended) 7-tuple keyed by id; the coast TARGET is stepped/reseeded by the caller (byte-identically
// to layer 24) and passed in each frame. On the first sighting of an id the display seeds to the
// target (no pop on spawn); thereafter it is nudged `disp += smooth * (target - disp)` componentwise
// (pure IEEE sub/mul/add, the netremote::blend op-shape). `smooth >= 1` hard-snaps to the target
// (== layer 24). Reset on a replay loop / restart so the blend re-seeds cleanly.
class RemoteCoasterSet {
public:
    // Update aircraft `id`'s displayed tuple toward `target` by `smooth` and return it.
    Coast7 update(std::int64_t id, const Coast7& target, double smooth);
    void reset() { slots_.clear(); }

private:
    struct Slot { std::int64_t id; Coast7 disp; };
    std::vector<Slot> slots_;
};

}  // namespace client
}  // namespace seads
