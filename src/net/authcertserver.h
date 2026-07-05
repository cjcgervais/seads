// SEADS authoritative CERTIFICATE-PKI server (netcode LAYER 30) — the server trusts ONE certificate
// authority public key, with revocation and key rotation.
//
// Layer 27 (broadcast_authsig) bound each seat to a per-token PUBLIC key ENROLLED on the server
// (PubkeyTable). Real public-key identity — but the roster is FIXED: enrollment needs a server
// change, and there is no way to REVOKE a compromised key or ROTATE one. Layer 30 closes that
// (layer 27's named honest-scope caveat) by moving trust to a CERTIFICATE AUTHORITY.
//
// broadcast_authcert is a SIBLING of broadcast_authsig (authsigserver.h): the same single-thread
// select() loop, seat binding, BIND-001 reply, seat_authorizes authorization, and the CHALLENGE-001
// nonce + derive_nonce freshness — all reused VERBATIM. broadcast_authsig / broadcast_authmac /
// broadcast_auth / broadcast_bound / broadcast_input and every other server are byte-for-byte
// UNTOUCHED. Only the credential changes: an ENROLLED public key becomes a CA-signed CERTIFICATE.
//   1. CHALLENGE — on accept, send a fresh per-connection nonce DOWN as a CHALLENGE-001 record (the
//      first downstream frame, before BIND). Reused verbatim from layers 26/27.
//   2. HELLO-004 + VERIFY — the client's response carries [certificate, challenge_signature]. The
//      certificate = CA's Ed25519 signature over (token, seat, epoch, client_pubkey);
//      challenge_signature = Ed25519_sign(client_seed, nonce||token). CaTable.authenticate returns
//      the certificate's DESIGNATED seat only when the certificate verifies under the CA public key,
//      the token is not revoked, the epoch is current (>= the token's rotation floor), the seat is
//      free, AND the challenge signature verifies under the certified client public key; otherwise
//      SPECTATOR — a non-CA / tampered / revoked / stale / forged / double-login credential all land
//      in the same class (the layer-21/26/27 OUT_OF_RANGE reject class).
//   3. BIND + AUTHORIZATION — reply with a one-time BIND-001 naming the seat, and authorize upstream
//      commands with seat_authorizes (own seat only). Both reused VERBATIM.
//
// THE HEADLINE over layer 27: the server holds a SINGLE CA public key, not a per-client roster. New
// identities need no server change (the CA issues a certificate); a compromised identity is REVOKED
// (revoke(token)); a client ROTATES its key by re-certifying at a higher epoch and the server raising
// the floor (set_min_epoch). A real certificate PKI.
//
// DETERMINISM (unchanged, the whole point): a forged / revoked / stale / unknown credential is
// rejected into the EXACT SAME SPECTATOR class — authentication stays an admission FILTER on the
// upstream that never touches the CommandQueue. So N certified clients each upstreaming ONLY their
// own seat's commands produce frames BYTE-IDENTICAL to session::build_server_frames, regardless of
// connect order, byte reorder/chunking, or an attacker also connected and upstreaming forgeries (all
// dropped). The bridge (seads_netauthcert_test) proves it.
//
// Honest scope: ONE self-signed root CA (no intermediate certificates / chain depth); the revocation
// set + epoch floors are in-memory server state (a production CRL/OCSP distributes them). The session
// key seeds the challenge nonce (CSPRNG in production, FIXED in the bridge for reproducibility). The
// downstream send is BLOCKING send_all (broadcast_authsig/27's base; the async/byte-cap/liveness or
// catch-up merge is the natural follow-up, as layer 27 preceded 28/29).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "boundserver.h"  // seads::netinput::seat_authorizes (the authorization predicate)
#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Verifying CERTIFICATE-AUTHORITY roster: the server trusts ONE 32-byte Ed25519 CA public key and
// holds NO per-client keys. State: the CA public key, a set of REVOKED tokens, a per-token EPOCH
// FLOOR (rotation; default 0), and seat occupancy. authenticate(cert, nonce, challenge_sig) verifies
// a presented certificate + possession proof to a seat, occupying it; SPECTATOR on any failure
// (non-CA / tampered / revoked / stale-epoch / out-of-range or held seat / bad possession proof).
// Mirror of tools/cert_ref.CaTable.
class CaTable {
public:
    CaTable(std::int64_t n_aircraft, const std::uint8_t ca_pubkey[32]);
    // Permanently reject a compromised identity (any certificate for `token` -> SPECTATOR).
    void revoke(std::int64_t token);
    void unrevoke(std::int64_t token);
    // Rotate `token`'s key: certificates with epoch < `epoch` are superseded (-> SPECTATOR).
    void set_min_epoch(std::int64_t token, std::int64_t epoch);
    // Resolve + VERIFY a presented certificate + challenge signature to a seat, occupying it.
    std::int64_t authenticate(const std::uint8_t* cert, std::size_t certlen, std::uint64_t nonce,
                              const std::uint8_t* challenge_sig, std::size_t siglen);
    void release(std::int64_t seat);
    std::int64_t size() const { return n_; }

private:
    std::int64_t n_;
    std::uint8_t ca_pk_[32];
    std::vector<bool> occupied_;
    std::unordered_set<std::int64_t> revoked_;
    std::unordered_map<std::int64_t, std::int64_t> min_epoch_;  // token -> minimum acceptable epoch
};

// Run the authoritative certificate-PKI server: gather `min_initial` clients (bounded by
// accept_deadline_ms). On each accept, send a CHALLENGE-001 (nonce = derive_nonce(session key,
// accept_counter)); read the client's HELLO-004; CaTable::authenticate(cert, nonce, challenge_sig)
// to a seat (or spectator); reply with BIND-001. Then for each produced frame read upstream commands
// (submitting only a client's OWN-seat commands), step the producer, and broadcast the frame
// downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi — the test
// rendezvous hook. Returns Stats; ok iff the whole stream was produced. cmds_unauth counts commands
// dropped for naming a foreign aircraft (or coming from a spectator). The caller owns (and must
// configure) `creds`, sized to the same n_aircraft as the producer/queue; the listener is NOT closed
// here (matching broadcast_authsig/broadcast_auth/broadcast_input).
Stats broadcast_authcert(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                         CaTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                         std::size_t min_initial, int accept_deadline_ms,
                         const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
