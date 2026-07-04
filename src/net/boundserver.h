// SEADS authoritative BOUND server (netcode LAYER 18) — a real multi-client client->aircraft binding.
//
// Layers 15b/16/17 opened the bidirectional axis but left ONE deferral flagged in every ADR: the
// client->aircraft binding was POSITIONAL and UNAUTHENTICATED. Any connected socket could upstream a
// command for ANY in-range aircraft (the CommandQueue rejects only an OUT-OF-RANGE index), and a
// client had no way to learn which aircraft was "its own" — it GUESSED. That is fine for a single
// scripted test driver; it is not a multiplayer server. Layer 18 settles it.
//
// broadcast_bound is a SIBLING of broadcast_input (inputserver.h): the same single-thread select()
// loop that reads upstream INPUT-001 commands, steps the InputProducer, and streams snapshots back —
// with three additions, all pure TRANSPORT (outside the world_hash; no det_math, no seal, rides
// v1.26r0):
//   1. SEAT ASSIGNMENT — a join-order SeatPolicy hands each joining client the LOWEST free aircraft
//      index (a SEAT) in [0, n_aircraft); when every seat is taken further joiners are SPECTATORS
//      (seat -1, receive-only). A leaver frees its seat for reuse. Deterministic in the join/leave
//      order (a transport fact).
//   2. BIND HANDSHAKE — right after accept, the server sends the client a one-time BIND-001 record
//      (bind001.h) naming its seat + the world size, as the FIRST framing frame on its downstream,
//      before any snapshot. The client no longer guesses; it is TOLD its aircraft.
//   3. UPSTREAM AUTHORIZATION — a client's decoded command is submitted to the CommandQueue ONLY when
//      it names the client's own seat (seat_authorizes: seat >= 0 && aircraft == seat). A command for
//      any other aircraft — or any command from a spectator — is DROPPED (Stats.cmds_unauth), which is
//      byte-identical to it never arriving (the same class as the queue's OUT_OF_RANGE reject).
//
// DETERMINISM (the whole point): the authoritative kernel's frames stay a pure function of the
// AUTHORIZED, canonically-ordered command SET. Authorization is an admission FILTER on the upstream —
// it cannot make the output depend on transport order/chunking any more than OUT_OF_RANGE did — and
// the seat assignment / BIND record touch only bookkeeping and the downstream. So: when N clients each
// upstream ONLY their own seat's commands and their union is the whole scenario command set (delivered
// before each apply_tick), the produced frames are BYTE-IDENTICAL to session::build_server_frames,
// regardless of which client got which seat or how the bytes were reordered/chunked; a foreign-aircraft
// command changes nothing. The bridge (seads_netbound_test) proves both.
//
// Honest scope (this first binding cut, deferred exactly as layer 15b deferred hygiene): the downstream
// send is BLOCKING send_all (broadcast_input's base — a cooperative client reads concurrently); merging
// the binding with the layer-16 async/byte-cap/liveness hygiene (broadcast_bidi) is the natural
// follow-up (the two are orthogonal axes). No late-join catch-up. The seat binding is UNAUTHENTICATED
// (join-order, no identity) — it enforces "one client, one aircraft", not "who you are".
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Join-order seat assignment: assign() hands out the LOWEST free aircraft index in [0, n_aircraft),
// or bind001::SPECTATOR (-1) when all are taken; release() frees a seat for reuse. Deterministic in
// the join/leave order. Pure integer bookkeeping (mirror of tools/bound_ref.SeatPolicy).
class SeatPolicy {
public:
    explicit SeatPolicy(std::int64_t n_aircraft) : occupied_(n_aircraft, false) {}
    std::int64_t assign();
    void release(std::int64_t seat);
    std::int64_t size() const { return static_cast<std::int64_t>(occupied_.size()); }

private:
    std::vector<bool> occupied_;
};

// The upstream authorization predicate: a seated client may command ONLY its own aircraft; a spectator
// (seat -1) may command nothing. Mirrors bound_ref.seat_authorizes.
inline bool seat_authorizes(std::int64_t seat, std::int64_t aircraft) {
    return seat >= 0 && aircraft == seat;
}

// Run the authoritative bound server: gather `min_initial` clients (bounded by accept_deadline_ms),
// assigning each a join-order seat and sending it a BIND-001 handshake; then for each produced frame
// read upstream commands (submitting only a client's OWN-seat commands), step the producer, and
// broadcast the frame downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi
// (before the upstream read + the step) — the test rendezvous hook. Returns Stats; ok iff the whole
// stream was produced. cmds_unauth counts commands dropped for naming a foreign aircraft (or coming
// from a spectator). Closes every client + does NOT close the listener (the caller owns it, matching
// broadcast_input).
Stats broadcast_bound(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                      std::size_t min_initial, int accept_deadline_ms,
                      const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
