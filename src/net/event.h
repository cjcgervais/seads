// SEADS reliable combat-EVENT channel (netcode layer 6). Mirrors tools/event_ref.py.
//
// Layer 5 (session) replicates combat STATE (HP/positions/rounds) via nearest-frame semantics — HP
// is IDEMPOTENT, so a dropped frame is healed by the next. But a HIT or a KILL is a transient EVENT
// (a discrete moment), not idempotent: reading it off the nearest STATE frame smears the exact tick,
// lumps aggregate hp, and loses the moment if that frame drops. Layer 6 adds a RELIABLE EVENT
// CHANNEL: the server DERIVES events by OBSERVING the authoritative kernel's hp deltas (the kernel
// is NOT modified — pure observation, so all goldens stay byte-identical), and ships them with
// REDUNDANCY (each 20 Hz frame re-sends the last EVENT_WINDOW_K events). The client applies them by
// monotonic seq (a de-duped, append-only journal) and so reconstructs the EXACT event sequence —
// every hit + the kill, each at its precise tick — even though the transport drops frames.
//
// Failure bound: an event rides up to K frames of redundancy (fewer in a dense burst), so it is lost
// only if EVERY frame carrying it drops — a blackout of at most K consecutive frames; under isolated
// single drops the reconstruction is COMPLETE. The journal resyncs past a permanently-lost event
// without head-of-line blocking. See event_ref.py for the full rationale + the bound proof.
//
// Determinism: event derivation observes det_math hp (bit-exact cross-toolchain by the golden
// promise) quantized to integers (sealed 1e3); windowing, the transport's integer lag/drop, and the
// seq dedup are pure integer logic. So the reconstructed event log serializes to identical bytes and
// its whole-run SHA-256 EVENT_DIGEST is the cross-impl parity artifact this C++ mirror reproduces
// BIT-FOR-BIT vs the Python reference. This composes the EXISTING protocol-4 session (the event
// window is a SESSION-LAYER message, NOT a new snapshot section) — no rail/golden/wire change, rides
// seal v1.12r0 (a Tier-2 net layer like interp/session).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "kernel.h"    // seads::Kernel, Rails, Command, Envelope
#include "session.h"   // reuse session::{Phase, AircraftSpec, Scenario} (SESSION-SK-001)

namespace seads {
namespace event {

// How many most-recent events every frame re-sends (the redundancy depth; mirrors
// event_ref.EVENT_WINDOW_K). An event rides up to K frames before newer events push it out.
constexpr int EVENT_WINDOW_K = 4;

// One derived combat event. All fields integer (hp/damage in milli-hp via the sealed HP_SCALE=1e3;
// hp is integer-valued so 1e3 carries it losslessly). seq is the monotonic reliable-delivery key.
struct Event {
    std::int64_t seq;
    std::int64_t tick;
    std::int64_t target;
    std::int64_t damage_milli;
    std::int64_t hp_after_milli;
    std::int64_t killed;
};

struct EventResult {
    std::vector<Event> events;    // server-derived authoritative log (seq = index)
    std::vector<Event> applied;   // client reconstruction (append-only, de-duped by seq)
    std::string digest;           // SHA-256 over encode_event_log(applied) — the parity artifact
    unsigned delivered = 0;       // event windows the client received
    unsigned n_windows = 0;       // event windows the server emitted
};

// Run the layer-6 event channel over the session scenario `sc`. When `drops != nullptr` it overrides
// the scenario's packet-loss set (used by the synthetic bound test); otherwise sc.drop_emit_ticks.
EventResult run_events(const Rails& rails, const session::Scenario& sc,
                       const std::int64_t* drops = nullptr, unsigned n_drops = 0);

// Canonical serialization of a reconstructed event log (count + each event's six integer fields,
// through the SAME GEO-001 varint the wire uses) — exposed so tests can re-hash independently.
std::vector<std::uint8_t> encode_event_log(const std::vector<Event>& applied);

}  // namespace event
}  // namespace seads
