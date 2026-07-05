// SEADS authoritative CERTIFICATE-PKI + ASYNC server (netcode LAYER 31) — the layer-30 CA-CERTIFICATE
// binding merged with the layer-16/19 downstream OUTPUT HYGIENE. The 27->28 step re-run on the
// CERTIFICATE credential (30->31).
//
// Layer 30 (broadcast_authcert, authcertserver.h) made the client->aircraft binding a function of a
// CA-signed CERTIFICATE the server verifies under ONE trusted certificate-authority public key: on accept
// the server sends a fresh CHALLENGE-001 nonce; the client answers HELLO-004 = [certificate,
// challenge_signature]; CaTable::authenticate verifies the certificate under the CA key (plus revocation +
// a per-token epoch floor) and the possession proof under the CERTIFIED client key before binding the
// certificate's DESIGNATED seat (a self-signed / revoked / stale / forged / unknown credential ->
// spectator). But — like layers 18/21/26/27 before their async siblings — it kept broadcast_input's
// BLOCKING send_all downstream, so one slow client back-pressures the broadcast and a silently-dead peer is
// never shed. Layer 28 (broadcast_authsig_async) already brought the layer-16/19 async hygiene (non-blocking
// per-client send buffers + byte-cap drop-slowest + liveness reap) to the ASYMMETRIC-SIGNATURE server;
// layer 31 does the same for the CERTIFICATE-PKI server. Admission (which certified identity PROVES which
// seat) and hygiene (delivery) are ORTHOGONAL axes.
//
// broadcast_authcert_async is broadcast_authsig_async's async loop (authsigasyncserver.cpp) with the
// ENROLLED-public-key verify replaced by the layer-30 CA-certificate verify — a SIBLING of both
// broadcast_authcert AND broadcast_authsig_async, so the sealed broadcast.cpp AND broadcast_bound /
// broadcast_bidi / broadcast_bound_async / broadcast_auth / broadcast_auth_async / broadcast_authmac /
// broadcast_authsig / broadcast_authsig_async / broadcast_authcert are byte-for-byte UNTOUCHED (this file
// owns its own AuthCertAsyncClient + flush/enqueue/cap/reap/drop helpers; transport, outside the
// world_hash — no det_math, no seal, rides v1.26r0; NO shared-file or Stats change — layers 16/18 already
// added capped/reaped/cmds_unauth, and CaTable + seat_authorizes + CHALLENGE-001 + HELLO-004/CERT-001 +
// BIND-001 are reused VERBATIM from layers 30/18). The three composed properties, all pure TRANSPORT:
//   1. VERIFIED CERTIFICATE SEAT (layer 30) — on accept the server sends a fresh CHALLENGE-001 nonce DOWN
//      (blocking, the socket still blocking), reads the client's HELLO-004 (bounded-blocking), and
//      CaTable::authenticate(cert, nonce, sig) VERIFIES the certificate under the trusted CA public key
//      (+ revocation + epoch floor) and the possession proof under the CERTIFIED client key, resolving the
//      certificate's DESIGNATED seat (free) or bind001::SPECTATOR (self-signed / revoked / stale / forged /
//      double-login). A leaver — clean EOF, fatal flush, byte-cap shed, OR liveness reap — release()s its
//      seat, so a reconnecting identity reclaims ITS OWN seat.
//   2. BIND + AUTHORIZATION (layers 18/30) — the resolved seat's one-time BIND-001 record is ENQUEUED as the
//      FIRST bytes of the client's downstream send buffer (right after the CHALLENGE that was already sent
//      synchronously), and a decoded command is submitted to the CommandQueue ONLY when it names the
//      client's own seat (seat_authorizes); a foreign/spectator command is DROPPED (Stats.cmds_unauth).
//   3. ASYNC HYGIENE (layers 16/19) — non-blocking per-client send buffers + opt-in byte-cap drop-slowest
//      (cap_bytes) + liveness reap of a stalled peer (liveness_frames).
//
// DETERMINISM (the whole point): the certificate verify is an admission FILTER on the upstream (which
// certified identity holds which seat, plus per-seat authorization); the hygiene is purely DOWNSTREAM
// delivery. Neither touches the CommandQueue's canonical selection. So N certified clients each upstreaming
// ONLY their own seat's commands (union = the whole scenario, delivered before each apply_tick) produce
// frames BYTE-IDENTICAL to session::build_server_frames — regardless of connect order, upstream
// reorder/chunking, any downstream cap/liveness drop, or a forger (self-signed / revoked / stale) also
// connected and upstreaming (all dropped). The bridge (seads_netauthcert_async_test) proves it.
//
// Honest scope: the CHALLENGE send + HELLO-004 read at accept are bounded-BLOCKING (the two non-async
// touches, matching layer 30's blocking base + layer 28's HELLO read). No late-join CATCH-UP (that is layer
// 32's axis on this server — orthogonal, replay-depth only). ONE self-signed root CA (no intermediate
// certificate chains); the revocation set + epoch floors are in-memory server state.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "authcertserver.h"  // seads::netinput::CaTable (the layer-30 CA-trust roster)
#include "boundserver.h"     // seads::netinput::seat_authorizes (the layer-18 authorization predicate)
#include "inputserver.h"     // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"          // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative certificate-PKI + async server. Gather `min_initial` clients (bounded by
// accept_deadline_ms): on each accept send a CHALLENGE-001 nonce (derive_nonce(session key,
// accept_counter)), read the client's HELLO-004, verify the certificate + possession proof against `creds`
// to a DESIGNATED seat, go non-blocking, and ENQUEUE its BIND-001 handshake as the first downstream bytes;
// then for each produced frame read upstream commands (submitting only a client's OWN-seat commands), step
// the producer, and ENQUEUE the frame to every client's non-blocking userspace send buffer (async output).
//   * cap_bytes (0 = unbounded): a client whose pending backlog an enqueue leaves above the cap is shed
//     (drop-slowest; Stats.capped, also a leave — its seat freed).
//   * liveness_frames (0 = no reaping): a client that makes NO receive progress for more than
//     liveness_frames consecutive produced frames is reaped (Stats.reaped, also a leave — its seat freed).
// `on_frame(fi)` fires at the TOP of iteration fi — the test rendezvous hook. Returns Stats; ok iff the
// whole stream was produced. The caller owns (and must configure) `creds`, sized to the same n_aircraft as
// the producer/queue; the listener is NOT closed here (matching broadcast_authcert / broadcast_authsig_async).
Stats broadcast_authcert_async(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                               CaTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                               std::size_t min_initial, int accept_deadline_ms,
                               const std::function<void(std::size_t)>& on_frame = {},
                               std::size_t cap_bytes = 0, std::size_t liveness_frames = 0);

}  // namespace netinput
}  // namespace seads
