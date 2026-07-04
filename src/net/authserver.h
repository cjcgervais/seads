// SEADS authoritative AUTHENTICATED-BINDING server (netcode LAYER 21) — identity-based seats.
//
// Layer 18 (broadcast_bound) gave each client its own aircraft, but by JOIN-ORDER POSITION: the first
// socket to connect got seat 0, the next seat 1, and so on. Every bidirectional ADR since flagged the
// same honest-scope caveat — the binding was POSITIONAL and UNAUTHENTICATED. A reconnecting client got
// a DIFFERENT seat depending on who else was connected, and nothing tied a socket to an identity.
// Layer 21 settles it: the binding becomes a function of the client's IDENTITY.
//
// broadcast_auth is a SIBLING of broadcast_bound (boundserver.h): the same single-thread select()
// loop that reads upstream INPUT-001 commands, steps the InputProducer, and streams snapshots back —
// with the join-order SeatPolicy replaced by an identity handshake, all pure TRANSPORT (outside the
// world_hash; no det_math, no seal, rides v1.26r0):
//   1. HELLO HANDSHAKE — a joining client's FIRST upstream framing frame is a HELLO-001 record
//      (hello001.h) carrying its opaque i64 credential `token`. The server reads it at accept, BEFORE
//      assigning a seat (the mirror of layer 18, where the seat came first and the client only read
//      its BIND). A client that sends no valid HELLO is dropped (never a member).
//   2. CREDENTIAL LOOKUP — a pre-shared CredentialTable maps each enrolled token to a DESIGNATED seat.
//      authenticate(token) returns that seat when the token is known AND its seat is free; SPECTATOR
//      (-1) when the token is unknown (no credential) or its seat is already held (a double-login).
//      The seat for a given token is a function of the ROSTER, invariant to join order.
//   3. BIND + AUTHORIZATION — as in layer 18: the server replies with a one-time BIND-001 record
//      naming the resolved seat, and authorizes upstream commands with seat_authorizes (own seat
//      only; a spectator commands nothing). BIND-001 and seat_authorizes are reused VERBATIM.
//
// DETERMINISM (unchanged from layer 18, and the whole point): authentication only decides WHICH seat
// (if any) each identity holds — it is an admission FILTER on the upstream, exactly like the join-order
// SeatPolicy was — it never touches the CommandQueue's canonical ordering. So when N authenticated
// clients each upstream ONLY their own seat's commands and their union is the whole scenario command
// set (delivered before each apply_tick), the produced frames are BYTE-IDENTICAL to
// session::build_server_frames, regardless of which identity connected in which order, how the bytes
// were reordered/chunked, or that an unknown-token spectator was also connected and upstreaming
// (its commands are all dropped). The bridge (seads_netauth_test) proves it.
//
// Honest scope (deliberate, matching layer 18's first-cut discipline): the downstream send is BLOCKING
// send_all (broadcast_input's base — a cooperative client reads concurrently); merging identity binding
// with the layer-16/19 async/byte-cap/liveness hygiene, or with layer-20 catch-up, is the natural
// follow-up (orthogonal axes — the same way layer 18 preceded layers 19/20). The "credential" is an
// opaque i64 the server looks up in a roster; a production system would carry a MAC/signature verified
// against a secret (orthogonal crypto). This layer is the BINDING mechanics: identity -> seat, and
// "you cannot command a seat you were not bound to."
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "boundserver.h"  // seads::netinput::seat_authorizes (the layer-18 authorization predicate)
#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Identity->seat binding: a pre-shared roster maps each known token to a DESIGNATED seat in
// [0, n_aircraft). authenticate() hands back that seat if it is currently free, else bind001::SPECTATOR
// (unknown token or a double-login). release() frees a seat on leave, so a reconnecting identity
// reclaims ITS seat. A deterministic function of the roster + the join/leave order (only double-login
// contention depends on order — the seat for a given token is fixed by the roster). Mirror of
// tools/auth_ref.CredentialTable.
class CredentialTable {
public:
    explicit CredentialTable(std::int64_t n_aircraft) : n_(n_aircraft), occupied_(n_aircraft, false) {}
    // Register a credential: `token` is designated `seat` (must be in [0, n_aircraft)).
    void enroll(std::int64_t token, std::int64_t seat);
    // Resolve a presented token to a seat, occupying it; SPECTATOR if unknown or already held.
    std::int64_t authenticate(std::int64_t token);
    void release(std::int64_t seat);
    std::int64_t size() const { return n_; }

private:
    std::int64_t n_;
    std::unordered_map<std::int64_t, std::int64_t> roster_;  // token -> designated seat
    std::vector<bool> occupied_;
};

// Run the authoritative authenticated-binding server: gather `min_initial` clients (bounded by
// accept_deadline_ms), reading each client's HELLO-001, authenticating its token against `creds` to a
// seat, and replying with a BIND-001 handshake; then for each produced frame read upstream commands
// (submitting only a client's OWN-seat commands), step the producer, and broadcast the frame
// downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi — the test
// rendezvous hook. Returns Stats; ok iff the whole stream was produced. cmds_unauth counts commands
// dropped for naming a foreign aircraft (or coming from a spectator). The caller owns (and must enroll)
// `creds`, sized to the same n_aircraft as the producer/queue; the listener is NOT closed here (the
// caller owns it, matching broadcast_input/broadcast_bound).
Stats broadcast_auth(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                     CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                     const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
