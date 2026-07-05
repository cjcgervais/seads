// SEADS authoritative CERTIFICATE-PKI + ASYNC + CATCH-UP server (netcode LAYER 32) — the LAST rung of the
// certificate-PKI arc (30 -> 31 -> 32), the 28->29 step re-run on the CERTIFICATE credential.
//
// Layer 31 (broadcast_authcert_async, authcertasyncserver.h) composed layer 30's CA-CERTIFICATE binding (a
// CHALLENGE-001 nonce + a verified HELLO-004 certificate + possession proof -> a DESIGNATED seat + BIND-001 +
// authorization) with the layer-16/19 async downstream hygiene. But a client joining after frame 0 still
// received only the SUFFIX from its accept point. Layer 20/23/29 already solved exactly this — retain the
// produced payloads (bounded to the last catchup_window, O(W) memory) and replay the missed prefix to a
// mid-stream joiner right after its BIND. Catch-up is a FOURTH orthogonal axis — replay DEPTH — beside
// admission (authentication + authorization) and delivery (the async hygiene). Layer 32 folds layer 20's
// catch-up onto layer 31.
//
// broadcast_authcert_catchup is broadcast_authcert_async's async loop (authcertasyncserver.cpp) with
// broadcast_live's history-retention + windowed catch-up replay folded in — a SIBLING of both
// broadcast_authcert_async AND broadcast_authsig_catchup, so the sealed broadcast.cpp AND every prior server
// (broadcast_bound / broadcast_bidi / broadcast_bound_async / broadcast_bound_catchup / broadcast_auth /
// broadcast_auth_async / broadcast_auth_catchup / broadcast_authmac / broadcast_authsig /
// broadcast_authsig_async / broadcast_authsig_catchup / broadcast_authcert / broadcast_authcert_async) are
// byte-for-byte UNTOUCHED (this file owns its own AuthCertCatchupClient + flush/enqueue/cap/reap/drop
// helpers; transport, outside the world_hash — no det_math, no seal, rides v1.26r0; CaTable +
// seat_authorizes + CHALLENGE-001 + HELLO-004/CERT-001 + BIND-001 reused VERBATIM, Stats.trimmed from layer
// 20 — NO shared-file or Stats change). It keeps ALL of layer 31's verified-certificate + hygiene properties
// and adds ONE, exactly as layer 29 added it to layer 28:
//   4. LATE-JOIN CATCH-UP (layer 20) — the server retains the produced payloads (the LAST `catchup_window`,
//      or ALL when window==0), and a client accepted at frame fi has that retained prefix
//      history[max(0,fi-W):fi] ENQUEUED right after its BIND-001 — so its whole downstream delivery is
//      [BIND | frames[max(0,fi-W):]]. `Stats.trimmed` counts window evictions; the byte-cap applies to each
//      replayed prefix frame (a joiner whose replay backlog trips the cap is shed during replay — counted
//      `capped`, its seat returned, never a live member).
//
// DETERMINISM: admission (the verified certificate + authorization) never touches the CommandQueue's
// canonical selection; hygiene is downstream delivery; catch-up is a joiner's replay depth. All four axes
// touch DISJOINT machinery. So N certified clients each upstreaming ONLY their own seat's commands produce
// frames BYTE-IDENTICAL to session::build_server_frames — regardless of connect order, upstream
// reorder/chunking, any downstream cap/liveness drop, OR any joiner's replay window — and every client's
// delivery is a byte-exact window of [BIND | the produced stream]. The bridge (seads_netauthcert_catchup_test)
// proves it.
//
// Honest scope: the CHALLENGE send + HELLO-004 read at accept are bounded-BLOCKING (the two non-async
// touches). Catch-up retention is O(catchup_window) frames (window==0 = retain-all = O(stream), bounded run
// only). ONE self-signed root CA (no intermediate certificate chains); the revocation set + epoch floors are
// in-memory server state.
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

// Run the authoritative certificate-PKI + async + catch-up server. Like broadcast_authcert_async, plus: a
// client accepted mid-stream is replayed the retained catch-up prefix (enqueued right after its BIND, before
// the live suffix). `catchup_window` (0 = retain ALL produced payloads) retains only the LAST
// `catchup_window`, evicting the oldest per new frame (Stats.trimmed); a joiner at frame fi is replayed
// exactly frames[max(0,fi-catchup_window):fi]. cap_bytes / liveness_frames as in broadcast_authcert_async.
// `on_frame(fi)` fires at the TOP of iteration fi. Returns Stats; ok iff the whole stream was produced. The
// caller owns (and must configure) `creds`; the listener is NOT closed here.
Stats broadcast_authcert_catchup(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                                 CaTable& creds, std::uint64_t session_k0, std::uint64_t session_k1,
                                 std::size_t min_initial, int accept_deadline_ms,
                                 const std::function<void(std::size_t)>& on_frame = {},
                                 std::size_t cap_bytes = 0, std::size_t liveness_frames = 0,
                                 std::size_t catchup_window = 0);

}  // namespace netinput
}  // namespace seads
