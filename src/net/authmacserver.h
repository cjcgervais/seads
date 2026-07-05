// SEADS authoritative STRONG-CREDENTIAL server (netcode LAYER 26) — a verifying keyed-MAC identity.
//
// Layer 21 (broadcast_auth) made the seat a function of the client's IDENTITY, but the "credential"
// was an abstracted i64 `token` the server merely LOOKED UP in a roster — a public username with no
// proof of possession. Anyone who observed or guessed a token could claim that identity and take its
// seat. Every auth ADR flagged the same honest-scope gap: "a production system would carry a
// MAC/signature the server verifies against a secret." Layer 26 closes it.
//
// broadcast_authmac is a SIBLING of broadcast_auth (authserver.h): the same single-thread select()
// loop, seat binding, BIND-001 reply, and seat_authorizes authorization — with the token LOOKUP
// replaced by a verifying keyed-MAC CHALLENGE-RESPONSE, all pure TRANSPORT (outside the world_hash;
// no det_math, no seal, rides v1.26r0). broadcast_auth / broadcast_bound / broadcast_input and every
// other server are byte-for-byte UNTOUCHED.
//   1. CHALLENGE — the instant a client connects, the server derives a fresh per-connection nonce
//      (SipHash(session_key, accept_counter)) and sends it DOWN as a CHALLENGE-001 record, the FIRST
//      downstream framing frame (before BIND). A distinct nonce per accept defeats replay of a
//      captured HELLO.
//   2. HELLO-002 + VERIFY — the client's response carries [token, mac], mac =
//      SipHash(secret[token], nonce||token). SecretTable.authenticate(token, nonce, mac) returns the
//      token's DESIGNATED seat only when the token is enrolled, its seat is free, AND the mac verifies
//      (proof of possession, bound to the fresh nonce + the claimed identity); otherwise SPECTATOR —
//      unknown token, double-login, OR a BAD MAC (forgery / wrong secret / stale nonce).
//   3. BIND + AUTHORIZATION — as in layers 18/21: reply with a one-time BIND-001 naming the seat, and
//      authorize upstream commands with seat_authorizes (own seat only). BIND-001 + seat_authorizes
//      are reused VERBATIM.
//
// DETERMINISM (unchanged from layers 18/21, the whole point): a forged / stale / unknown credential
// is rejected into the EXACT SAME SPECTATOR class as layer 21's unknown token — authentication stays
// an admission FILTER on the upstream that never touches the CommandQueue's canonical ordering. So N
// authenticated clients each upstreaming ONLY their own seat's commands (union = the whole scenario
// set, delivered before each apply_tick) produce frames BYTE-IDENTICAL to session::build_server_frames,
// regardless of connect order, byte reorder/chunking, or an attacker also connected and upstreaming
// forgeries (all dropped). The bridge (seads_netauthmac_test) proves it.
//
// Honest scope (deliberate, matching the layer discipline): the downstream send is BLOCKING send_all
// (broadcast_auth's base; merging with the async/byte-cap/liveness hygiene or catch-up is the natural
// follow-up, as layer 21 preceded 22/23). This is a SYMMETRIC MAC (a pre-shared 128-bit secret the
// server holds) with a server-contributed nonce for freshness — strictly stronger than a bare token;
// an ASYMMETRIC signature (public-key identity, no shared secret on the server) is the named next
// hardening. The session key seeds the challenge nonce; a production server draws it from a CSPRNG,
// the deterministic bridge passes a fixed seed so the run is reproducible.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "boundserver.h"  // seads::netinput::seat_authorizes (the authorization predicate)
#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Verifying identity->seat binding: a pre-shared roster maps each known token to a DESIGNATED seat in
// [0, n_aircraft) AND a 128-bit secret. authenticate(token, nonce, mac) hands back the seat only when
// the token is enrolled, its seat is free, AND the presented mac == SipHash(secret, nonce||token);
// otherwise bind001::SPECTATOR (unknown token, double-login, or a bad MAC — a forgery is rejected into
// the same class as an unknown token). release() frees a seat on leave. Mirror of
// tools/authmac_ref.SecretTable.
class SecretTable {
public:
    explicit SecretTable(std::int64_t n_aircraft) : n_(n_aircraft), occupied_(n_aircraft, false) {}
    // Register a credential: `token` is designated `seat` (must be in [0, n_aircraft)) with the
    // 128-bit secret (k0, k1).
    void enroll(std::int64_t token, std::int64_t seat, std::uint64_t k0, std::uint64_t k1);
    // Resolve + VERIFY a presented (token, nonce, mac) to a seat, occupying it; SPECTATOR if unknown,
    // already held, or the mac does not verify.
    std::int64_t authenticate(std::int64_t token, std::uint64_t nonce, std::uint64_t mac);
    void release(std::int64_t seat);
    std::int64_t size() const { return n_; }

private:
    struct Entry { std::int64_t seat; std::uint64_t k0, k1; };
    std::int64_t n_;
    std::unordered_map<std::int64_t, Entry> roster_;  // token -> (designated seat, secret)
    std::vector<bool> occupied_;
};

// Run the authoritative strong-credential server: gather `min_initial` clients (bounded by
// accept_deadline_ms). On each accept, send a CHALLENGE-001 (nonce = derive_nonce(session key,
// accept_counter)); read the client's HELLO-002; SecretTable::authenticate(token, nonce, mac) to a
// seat (or spectator); reply with BIND-001. Then for each produced frame read upstream commands
// (submitting only a client's OWN-seat commands), step the producer, and broadcast the frame
// downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi — the test
// rendezvous hook. Returns Stats; ok iff the whole stream was produced. cmds_unauth counts commands
// dropped for naming a foreign aircraft (or coming from a spectator). The caller owns (and must
// enroll) `creds`, sized to the same n_aircraft as the producer/queue; the listener is NOT closed
// here (the caller owns it, matching broadcast_input/broadcast_auth).
Stats broadcast_authmac(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                        SecretTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                        std::size_t min_initial, int accept_deadline_ms,
                        const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
