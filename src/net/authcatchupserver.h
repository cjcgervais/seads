// SEADS authoritative AUTHENTICATED + ASYNC + CATCH-UP server (netcode LAYER 23) — the LAST rung of the
// authenticated arc. Layer 22's identity-authenticated async server grows late-join catch-up: an
// authenticated client that connects mid-fight gets its designated seat + BIND, is REPLAYED the frames it
// missed, then joins the live suffix. It is the 19->20 step (bound+async -> bound+async+catch-up) re-run on
// the AUTHENTICATED server (22->23), exactly as layer 22 was the 18->19 step re-run on it.
//
// Layer 22 (broadcast_auth_async, authasyncserver.h) composed layer 21's IDENTITY binding (a HELLO-001
// credential looked up in a pre-shared CredentialTable to a DESIGNATED seat + a BIND-001 reply + upstream
// authorization) with the layer-16/19 async downstream hygiene (non-blocking per-client send buffers +
// byte-cap drop-slowest + liveness reap). But a client joining after frame 0 still received only the SUFFIX
// from its accept point — it could never reconstruct the whole stream. Layer 20 (broadcast_bound_catchup)
// already solved exactly this on the JOIN-ORDER bound + async server: retain the produced payloads (bounded
// to the last catchup_window, O(W) memory) and replay the missed prefix to a mid-stream joiner right after
// its BIND. Catch-up is a FOURTH orthogonal axis — replay DEPTH — beside admission (authentication +
// authorization) and delivery (the async hygiene). Layer 23 folds layer 20's catch-up onto layer 22.
//
// broadcast_auth_catchup is broadcast_auth_async's async loop (authasyncserver.cpp) with broadcast_live's
// history-retention + windowed catch-up replay (broadcast.cpp, via broadcast_bound_catchup) folded in — a
// SIBLING of both broadcast_auth_async AND broadcast_bound_catchup, so the sealed broadcast.cpp AND
// broadcast_bound / broadcast_bidi / broadcast_bound_async / broadcast_bound_catchup / broadcast_auth /
// broadcast_auth_async are ALL byte-for-byte UNTOUCHED (this file owns its own AuthCatchupClient +
// flush/enqueue/cap/reap/drop helpers; it is transport, outside the world_hash — no det_math, no seal,
// rides v1.26r0; CredentialTable + seat_authorizes + HELLO-001 + BIND-001 are reused VERBATIM from layers
// 21/18, and Stats.trimmed from layer 20 — NO shared-file or Stats change). It keeps ALL of layer 22's
// identity + hygiene properties and adds ONE, exactly as layer 20 added it to layer 19:
//   1. IDENTITY SEAT (layer 21) — on accept the server reads the client's FIRST upstream framing frame, a
//      HELLO-001 credential; CredentialTable::authenticate(token) resolves it to its DESIGNATED seat (free)
//      or bind001::SPECTATOR (unknown token or double-login), invariant to join order. A leaver — clean
//      EOF, fatal flush, byte-cap shed, OR liveness reap — release()s its seat, so a reconnecting identity
//      reclaims ITS OWN seat.
//   2. BIND + AUTHORIZATION (layers 18/21) — the resolved seat's one-time BIND-001 record is ENQUEUED as
//      the FIRST bytes of the client's downstream send buffer; a decoded command is submitted to the
//      CommandQueue ONLY when it names the client's own seat (seat_authorizes), else DROPPED
//      (Stats.cmds_unauth).
//   3. ASYNC HYGIENE (layers 16/19) — non-blocking per-client send buffers + opt-in byte-cap drop-slowest
//      (cap_bytes) + liveness reap of a stalled peer (liveness_frames).
//   4. LATE-JOIN CATCH-UP (layer 20) — the server retains the produced payloads (the LAST `catchup_window`
//      of them, or ALL when window==0), and a client accepted at frame fi has that retained prefix
//      history[max(0,fi-W):fi] ENQUEUED right after its BIND-001 record — before it enters the live stream
//      at frame fi. So its whole downstream delivery is [BIND | frames[max(0,fi-W):fi] | live frames fi,
//      fi+1, ...] = [BIND | frames[max(0,fi-W):]]. A client present from the initial gather (history empty)
//      receives [BIND | the whole stream]. `Stats.trimmed` counts window evictions. The replay rides the
//      SAME non-blocking send buffer (enqueued after the BIND), so a slow joiner's catch-up cannot
//      back-pressure the loop, and the byte-cap applies to each replayed prefix frame too (a joiner whose
//      replay backlog trips the cap is shed during replay — counted `capped`, its seat returned, never a
//      live member ⇒ not a join/leave).
//
// DETERMINISM (the whole point): authentication is an admission FILTER on the upstream (which identity
// holds which seat, and per-seat authorization); the hygiene is DOWNSTREAM delivery (WHEN each client's
// bytes move, WHICH slow/dead clients are shed); catch-up is a joiner's REPLAY DEPTH. All three axes touch
// DISJOINT machinery and NONE touches the CommandQueue's canonical selection. So the authoritative kernel's
// frames stay a pure function of the AUTHENTICATED, AUTHORIZED, canonically-ordered command SET: when N
// authenticated clients each upstream ONLY their own seat's commands and their union is the whole scenario
// (delivered before each apply_tick), the produced frames are BYTE-IDENTICAL to session::build_server_frames
// — regardless of which identity connected in which order, the upstream reorder/chunking, any downstream
// cap/liveness drop, OR any joiner's replay window — and every client's delivery is a byte-exact window of
// [BIND | the produced stream]. The bridge (seads_netauthcatchup_test) proves it.
//
// Honest scope: the HELLO-001 handshake read at accept is bounded-BLOCKING (accept_deadline_ms — the one
// non-async touch, matching layers 21/22). Catch-up retention is O(catchup_window) frames (window==0 =
// retain-all = O(stream), only for a bounded run — layer 14's boundary, inherited); per-frame join service
// (accept + replay enqueue happen in the frame loop, one select() slice). The "credential" is an opaque
// i64 the server looks up in a roster (a production system would carry a MAC/signature — orthogonal crypto).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#include "authserver.h"   // seads::netinput::CredentialTable (the layer-21 identity->seat roster)
#include "boundserver.h"  // seads::netinput::seat_authorizes (the layer-18 authorization predicate)
#include "inputserver.h"  // seads::netinput::{InputProducer, CommandQueue, Stats}
#include "socket.h"       // seads::netsock::socket_t

namespace seads {
namespace netinput {

// Run the authoritative authenticated + async + catch-up server. Gather `min_initial` clients (bounded by
// accept_deadline_ms), reading each client's HELLO-001, authenticating its token against `creds` to a
// DESIGNATED seat, going non-blocking, and ENQUEUEING its BIND-001 handshake as the first downstream bytes;
// then for each produced frame read upstream commands (submitting only a client's OWN-seat commands), step
// the producer, and ENQUEUE the frame to every client's non-blocking userspace send buffer. A client
// accepted mid-stream is additionally replayed the retained catch-up prefix (enqueued right after its BIND,
// before the live suffix).
//   * cap_bytes (0 = unbounded): a client whose pending backlog an enqueue leaves above the cap is shed
//     (drop-slowest; Stats.capped). A live member shed this way is also a leave (its seat freed); a
//     mid-replay joiner shed before becoming live is capped but NOT a join/leave (its seat is returned).
//   * liveness_frames (0 = no reaping): a client that makes NO receive progress (drains zero bytes AND
//     stays pending) for more than liveness_frames consecutive produced frames is reaped (Stats.reaped,
//     also a leave — its seat is freed); a slow-but-alive client is never reaped.
//   * catchup_window (0 = retain ALL produced payloads): retain only the LAST `catchup_window`, evicting
//     the oldest per new frame (Stats.trimmed). A joiner at frame fi is replayed exactly
//     frames[max(0,fi-catchup_window):fi]; window==0 replays frames[0:fi] (whole prefix).
// `on_frame(fi)` fires at the TOP of iteration fi (before the upstream read + the step) — the test
// rendezvous hook, same contract as broadcast_auth_async / broadcast_bound_catchup. Returns Stats; ok iff
// the whole stream was produced. cmds_unauth counts commands dropped for naming a foreign aircraft (or
// coming from a spectator). The caller owns (and must enroll) `creds`, sized to the same n_aircraft as the
// producer/queue; the listener is NOT closed here (the caller owns it, matching broadcast_auth_async).
Stats broadcast_auth_catchup(netsock::socket_t listener, InputProducer& producer, CommandQueue& queue,
                             CredentialTable& creds, std::size_t min_initial, int accept_deadline_ms,
                             const std::function<void(std::size_t)>& on_frame = {},
                             std::size_t cap_bytes = 0, std::size_t liveness_frames = 0,
                             std::size_t catchup_window = 0);

}  // namespace netinput
}  // namespace seads
