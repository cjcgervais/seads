// SEADS authoritative CERTIFICATE-CHAIN server (netcode LAYER 33) — path validation up to a trusted
// root: intermediate certificate authorities.
//
// Layer 30 (broadcast_authcert) trusted ONE self-signed root CA and each client presented a SINGLE
// certificate signed DIRECTLY by it — no delegation. Its honest-scope caveat named exactly this
// follow-up: "ONE self-signed root CA (no intermediate certificates / chain depth)". A real PKI
// delegates issuance: an INTERMEDIATE CA (itself certified by the root) signs the leaf, so the root
// key can stay offline and issuance scales. Layer 33 carries a CHAIN and validates the PATH.
//
// broadcast_authcertchain is a SIBLING of broadcast_authcert (authcertserver.h): the same single-
// thread select() loop, seat binding, BIND-001 reply, seat_authorizes authorization, and the
// CHALLENGE-001 nonce + derive_nonce freshness — all reused VERBATIM. broadcast_authcert /
// broadcast_authsig / broadcast_auth / broadcast_bound / broadcast_input and every other server are
// byte-for-byte UNTOUCHED. Only the credential grows from a single CA-signed certificate to a CHAIN:
//   1. CHALLENGE — on accept, send a fresh CHALLENGE-001 nonce DOWN (first downstream frame, before
//      BIND). Reused verbatim from layers 26/27/30.
//   2. HELLO-005 + PATH VERIFY — the client's response carries [chain (leaf..top), challenge_signature].
//      Each link is an ordinary CERT-001; each is signed by the NEXT link's key and the TOP link by the
//      trusted ROOT key (cert001::verify_chain). CaChainTable::authenticate returns the LEAF's
//      DESIGNATED seat only when the whole PATH verifies to the root, NO link's token is revoked, EVERY
//      link's epoch is current (>= that token's rotation floor), the leaf's seat is free, AND the
//      challenge signature verifies under the LEAF's public key; otherwise SPECTATOR — a broken chain /
//      self-signed leaf / wrong root / revoked (leaf OR intermediate) / stale-epoch / forged / over-deep
//      credential all land in the same class (the layer-21/26/27/30 OUT_OF_RANGE reject class).
//   3. BIND + AUTHORIZATION — reply with a one-time BIND-001 naming the seat, and authorize upstream
//      commands with seat_authorizes (own seat only). Both reused VERBATIM.
//
// THE HEADLINE over layer 30: the root CA delegates. New intermediates need no server change (the root
// certifies them offline); revoking or rotating an INTERMEDIATE invalidates every leaf beneath it (a
// single CRL entry / epoch bump kills a whole issuance branch) — real chain-of-trust semantics.
//
// DETERMINISM (unchanged, the whole point): any invalid chain rejects into the EXACT SAME SPECTATOR
// class — authentication stays an admission FILTER on the upstream that never touches the CommandQueue.
// So N certified clients each upstreaming ONLY their own seat's commands produce frames BYTE-IDENTICAL
// to session::build_server_frames, regardless of connect order, byte reorder/chunking, chain depth, or
// an attacker also connected and upstreaming forgeries (all dropped). The bridge proves it.
//
// Honest scope: a chain terminating at ONE trusted root (max depth bounded; no cross-signing / multiple
// trust anchors); the revocation set + epoch floors are in-memory server state (a production CRL/OCSP
// distributes them). The session key seeds the challenge nonce (CSPRNG in production, FIXED in the
// bridge for reproducibility). Downstream send is BLOCKING send_all (broadcast_authcert/30's base; the
// async/byte-cap/liveness or catch-up merge is the natural follow-up, as 30 preceded 31/32).
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

// Verifying CERTIFICATE-CHAIN roster: the server trusts ONE 32-byte Ed25519 ROOT public key and holds
// NO per-client keys. State: the root public key, a maximum chain depth, a set of REVOKED tokens
// (leaves OR intermediates), a per-token EPOCH FLOOR (rotation; default 0), and seat occupancy.
// authenticate(chain, nonce, challenge_sig) validates the PATH to the root + the leaf possession proof
// to a seat, occupying it; SPECTATOR on any failure. A revocation/rotation on an INTERMEDIATE token
// invalidates every leaf issued beneath it. Mirror of tools/cert_ref.CaChainTable.
class CaChainTable {
public:
    // max_depth bounds the presented chain length (leaf..top); a chain longer than this is rejected.
    CaChainTable(std::int64_t n_aircraft, const std::uint8_t root_pubkey[32],
                 std::size_t max_depth = 4);
    // Permanently reject a compromised identity (leaf OR intermediate token -> its subtree SPECTATOR).
    void revoke(std::int64_t token);
    void unrevoke(std::int64_t token);
    // Rotate `token`'s key: any chain containing a link for `token` with epoch < `epoch` -> SPECTATOR.
    void set_min_epoch(std::int64_t token, std::int64_t epoch);
    // Validate a presented certificate CHAIN + challenge signature to a seat, occupying it.
    std::int64_t authenticate(const std::vector<std::vector<std::uint8_t>>& chain, std::uint64_t nonce,
                              const std::uint8_t* challenge_sig, std::size_t siglen);
    void release(std::int64_t seat);
    std::int64_t size() const { return n_; }

private:
    std::int64_t n_;
    std::uint8_t root_pk_[32];
    std::size_t max_depth_;
    std::vector<bool> occupied_;
    std::unordered_set<std::int64_t> revoked_;
    std::unordered_map<std::int64_t, std::int64_t> min_epoch_;  // token -> minimum acceptable epoch
};

// Run the authoritative certificate-chain server: gather `min_initial` clients (bounded by
// accept_deadline_ms). On each accept, send a CHALLENGE-001 (nonce = derive_nonce(session key,
// accept_counter)); read the client's HELLO-005; CaChainTable::authenticate(chain, nonce,
// challenge_sig) to a seat (or spectator); reply with BIND-001. Then for each produced frame read
// upstream commands (submitting only a client's OWN-seat commands), step the producer, and broadcast
// the frame downstream (blocking send_all). `on_frame(fi)` fires at the TOP of iteration fi — the test
// rendezvous hook. Returns Stats; ok iff the whole stream was produced. cmds_unauth counts commands
// dropped for naming a foreign aircraft (or coming from a spectator). The caller owns (and must
// configure) `creds`, sized to the same n_aircraft as the producer/queue; the listener is NOT closed
// here (matching broadcast_authcert/broadcast_authsig/broadcast_input).
Stats broadcast_authcertchain(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                              CaChainTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                              std::size_t min_initial, int accept_deadline_ms,
                              const std::function<void(std::size_t)>& on_frame = {});

}  // namespace netinput
}  // namespace seads
