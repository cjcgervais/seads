// SEADS authoritative ASYMMETRIC-SIGNATURE server (netcode LAYER 27) — a verifying PUBLIC-KEY
// identity: the server holds ONLY public keys.
//
// Layer 26 (broadcast_authmac) made the seat a function of a VERIFIED credential, but the
// credential was a SYMMETRIC keyed MAC: the server held a shared 128-bit secret per token and
// recomputed the MAC. That is strictly stronger than layer 21's bare-token lookup — yet the server
// still holds a secret capable of FORGING any client's proof, so a roster leak (or a malicious
// server) can impersonate. Layer 27 closes that last gap with an ASYMMETRIC signature.
//
// broadcast_authsig is a SIBLING of broadcast_authmac (authmacserver.h): the same single-thread
// select() loop, seat binding, BIND-001 reply, and seat_authorizes authorization — with the
// symmetric-MAC verify replaced by an Ed25519 SIGNATURE verify, all pure TRANSPORT (outside the
// world_hash; no det_math, no seal, rides v1.26r0). broadcast_authmac / broadcast_auth /
// broadcast_bound / broadcast_input and every other server are byte-for-byte UNTOUCHED. The
// CHALLENGE mechanism is reused VERBATIM from layer 26 (CHALLENGE-001 + derive_nonce): only the
// PROOF becomes asymmetric.
//   1. CHALLENGE — on accept, derive a fresh per-connection nonce (SipHash(session_key,
//      accept_counter)) and send it DOWN as a CHALLENGE-001 record, the FIRST downstream frame
//      (before BIND). A distinct nonce per accept defeats replay of a captured HELLO.
//   2. HELLO-003 + VERIFY — the client's response carries [token, signature], sig =
//      Ed25519_sign(private_seed[token], nonce||token). PubkeyTable.authenticate(token, nonce, sig)
//      returns the token's DESIGNATED seat only when the token is enrolled, its seat is free, AND
//      the signature verifies under the enrolled PUBLIC key; otherwise SPECTATOR — unknown token,
//      double-login, OR a bad signature (forgery / wrong key / stale nonce).
//   3. BIND + AUTHORIZATION — reply with a one-time BIND-001 naming the seat, and authorize
//      upstream commands with seat_authorizes (own seat only). Both reused VERBATIM.
//
// THE HEADLINE over layer 26: the PubkeyTable holds no secret capable of signing — only public
// keys. A full roster leak (every enrolled public key) still cannot impersonate any client, because
// a valid HELLO-003 requires the private seed the server never sees. Real public-key identity.
//
// DETERMINISM (unchanged, the whole point): a forged / stale / unknown credential is rejected into
// the EXACT SAME SPECTATOR class as layer 21/26 — authentication stays an admission FILTER on the
// upstream that never touches the CommandQueue. So N authenticated clients each upstreaming ONLY
// their own seat's commands produce frames BYTE-IDENTICAL to session::build_server_frames,
// regardless of connect order, byte reorder/chunking, or an attacker also connected and upstreaming
// forgeries (all dropped). The bridge (seads_netauthsig_test) proves it.
//
// Honest scope: keys are enrolled from a fixed roster (a production PKI carries certificates / key
// rotation); the session key seeds the challenge nonce (a CSPRNG seed in production, a FIXED seed in
// the bridge for reproducibility). The downstream send is BLOCKING send_all (broadcast_auth/26's
// base; the async/byte-cap/liveness or catch-up merge is the natural follow-up, as layer 21
// preceded 22/23).
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

// Verifying PUBLIC-KEY identity->seat binding: a pre-shared roster maps each known token to a
// DESIGNATED seat in [0, n_aircraft) AND a 32-byte Ed25519 PUBLIC key (NO secret). authenticate(
// token, nonce, sig) hands back the seat only when the token is enrolled, its seat is free, AND the
// presented signature verifies under the public key over (nonce||token); otherwise
// bind001::SPECTATOR (unknown token, double-login, or a bad signature — a forgery is rejected into
// the same class). release() frees a seat on leave. Mirror of tools/authsig_ref.PubkeyTable.
class PubkeyTable {
public:
    explicit PubkeyTable(std::int64_t n_aircraft) : n_(n_aircraft), occupied_(n_aircraft, false) {}
    // Register a credential: `token` is designated `seat` (must be in [0, n_aircraft)) with the
    // 32-byte Ed25519 public key `pubkey`.
    void enroll(std::int64_t token, std::int64_t seat, const std::uint8_t pubkey[32]);
    // Resolve + VERIFY a presented (token, nonce, signature) to a seat, occupying it; SPECTATOR if
    // unknown, already held, or the signature does not verify.
    std::int64_t authenticate(std::int64_t token, std::uint64_t nonce, const std::uint8_t* sig,
                              std::size_t siglen);
    void release(std::int64_t seat);
    std::int64_t size() const { return n_; }

private:
    struct Entry { std::int64_t seat; std::uint8_t pk[32]; };
    std::int64_t n_;
    std::unordered_map<std::int64_t, Entry> roster_;  // token -> (designated seat, public key)
    std::vector<bool> occupied_;
};

// Run the authoritative asymmetric-signature server: gather `min_initial` clients (bounded by
// accept_deadline_ms). On each accept, send a CHALLENGE-001 (nonce = derive_nonce(session key,
// accept_counter)); read the client's HELLO-003; PubkeyTable::authenticate(token, nonce, sig) to a
// seat (or spectator); reply with BIND-001. Then for each produced frame read upstream commands
// (submitting only a client's OWN-seat commands), step the producer, and broadcast the frame
// downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi — the test
// rendezvous hook. Returns Stats; ok iff the whole stream was produced. cmds_unauth counts commands
// dropped for naming a foreign aircraft (or coming from a spectator). The caller owns (and must
// enroll) `creds`, sized to the same n_aircraft as the producer/queue; the listener is NOT closed
// here (matching broadcast_authmac/broadcast_auth/broadcast_input).
Stats broadcast_authsig(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                        PubkeyTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                        std::size_t min_initial, int accept_deadline_ms,
                        const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
