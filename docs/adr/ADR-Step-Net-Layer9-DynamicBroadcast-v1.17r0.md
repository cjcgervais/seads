# ADR-Step-Net-Layer9-DynamicBroadcast-v1.17r0 — Single-thread select() broadcast with dynamic join/leave

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events / layers 7–8)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Layer 8 (`ADR-Step-Net-Layer8-MultiClient-v1.17r0`) proved that **one server broadcasts the frame
stream to N clients and all reconstruct the byte-identical digest** — but every client connected
**before** streaming began, and the server fanned out with a **blocking `send_all` per already-connected
client**. Its "What is NOT done" list named the next rung explicitly: *"a true async server (a
single-threaded `select`/`poll` broadcast loop … dynamic join/leave mid-session)."* The layer-8
`wait_readable` primitive was called out as the seed for that loop.

Layer 9 builds it: a **genuine single-threaded `select()` broadcast event loop** with **dynamic
membership**. One thread multiplexes accepting late joiners and reaping departed clients — no
thread-per-client — and proves the natural determinism statement for a live session with churn: a
client that joins mid-stream receives **exactly** the contiguous frame suffix from its join point, a
client that leaves receives a clean prefix, and neither perturbs a client that stays for the whole
fight (which still reconstructs the sealed SESSION-SK-001 digest).

## 2) Decision

**(a) One multi-fd readability primitive (`src/net/socket`).**

- `bool select_readable(const vector<socket_t>& fds, int timeout_ms, vector<socket_t>& ready)` — a
  portable `select()` over **many** fds at once (Winsock ignores `nfds`; POSIX passes `max(fd)+1`;
  `timeout_ms < 0` blocks). Fills `ready` with the readable subset. This is the layer-8
  `wait_readable` generalized to the whole fd set `{listener} ∪ {clients}`, which is what a
  single-threaded fan-out loop needs: a listener that is readable ⇒ a pending **JOIN**; a client that
  is readable (a receive-only broadcast socket) ⇒ `recv()==0` EOF ⇒ a **LEAVE**.

**(b) The single-thread select broadcast server (`src/net/broadcast`, new `seads_broadcast` lib).**
`netbcast::broadcast_select(listener, payloads, min_initial, accept_deadline_ms, on_frame)`:

- Blocks (bounded by `accept_deadline_ms`) until `≥ min_initial` clients are connected before frame 0
  (fail-not-wedge on an absent client).
- Streams each payload as **one atomically-sent length-prefixed frame** (`framing::encode_frame`), so a
  joiner is always **frame-aligned** — it never sees a partial frame, only whole frames from its join
  point onward.
- Each frame iteration runs **one `select_readable` over `{listener} ∪ clients`**: a readable listener
  drains the accept queue (**JOIN** — a client accepted before frame `fi` receives `fi…`); a readable
  client is probed with `recv_some` and dropped on `≤0` (**LEAVE** — clean EOF or peer error). A send
  that fails also drops that client. All with **no thread-per-client**.
- `on_frame(fi)` is an optional hook run at the top of each iteration — the determinism bridge uses it
  to rendezvous a mid-stream join at an **exact** frame with no sleeps.

`seads_broadcast` = `seads_framing` + `seads_socket`. TRANSPORT — outside the kernel + `world_hash`,
no `det_math`.

**(c) The dynamic-membership determinism BRIDGE (`seads_netdyn_test`).** The reference is again the
sealed in-process `session::run_session(...).digest` + the exact `build_server_frames` payload list
(**not re-derived**). One server thread runs `broadcast_select` over 41 real 127.0.0.1 frames; three
client threads with distinct lifetimes connect:

- **FULL** — connects before frame 0, reads to EOF: receives all 41 frames, rebuilds the `ServerFrames`
  list keyed on each frame's **decoded `server_tick`**, runs the **same** `run_client()`, and its digest
  **==** the in-process reference (`24f71845…c332`, GCC **and** Clang) — the layer-8 result, now driven
  by the single select loop.
- **LATE** — waits for the server to reach frame `J = N/2` (the `on_frame` hook pauses the server until
  the late client has connected, so the join is pinned with **no timing race**), then reads its suffix
  to EOF. The bridge computes its join index `K` from the `server_tick` of its **first delivered frame**
  and asserts its payloads are **exactly `frames[K:]`** (byte-exact), with `0 < K < N`.
- **LEAVER** — connects at frame 0, reads `≥ N/4` frames, then closes mid-stream. The bridge asserts it
  received a clean contiguous **prefix** `frames[:m]` (`0 < m < N`), and — crucially — that FULL and
  LATE are **unaffected** (FULL still reconstructs the sealed digest). The server observes the leave via
  `select` + `recv==0` (`stats.leaves ≥ 1`).

Server stats are asserted too: every frame sent, exactly **3 joins**, **≥1 leave**. A finite watchdog
fails rather than wedges on a socket hang.

**(d) The demo server uses the same loop (`seads_netserver [port] [num_clients]`).** It now calls
`broadcast_select` (shared with the bridge — no untested divergence), so the human demo also supports a
client joining mid-stream (it gets the remaining frames) and a client leaving.

**Why NO-SEAL.** Nothing sealed is touched: the framing envelope, the protocol-6 snapshot bytes, wire
scales, rails, `det_math`, the kernel, and every golden are unchanged. A `select`-multiplexed broadcast
with churn is pure transport over the existing byte stream — the same category as interp / predict /
session / events / layers 7–8.

## 3) Verification (gates)

- **Determinism bridge:** `seads_netdyn_test` (ctest `netdyn_bridge`, under the existing
  `option(SEADS_SOCKET_TESTS ON)` + `if(NOT CMAKE_CROSSCOMPILING)` guard alongside `netloop_bridge` /
  `multiclient_bridge`): FULL reconstructs `24f71845…c332`, LATE gets exactly `frames[20:]`, LEAVER gets
  a clean 10-frame prefix then leaves cleanly (`joins=3 leaves=1`) — GCC **and** Clang, 12/12 stress
  reruns clean. Local ctest **14/14** (was 13/13; + `netdyn_bridge`) both toolchains.
- **Property test** `tests/property/test_broadcast.py` (**+6 ⇒ 139**): the pure membership model
  (`present for [join, leave) ⇒ receives exactly those frames`) — window-exactness, full=whole,
  late=suffix, leaver=prefix, leave-doesn't-disturb-coresident, and composition with the layer-7 framing
  codec (a delivered suffix reassembles byte-exact under any TCP chunking). The timing-independent core
  of what `netdyn_bridge` proves over real sockets.
- **Gates wired:** `guardian.yml` — `seads_netdyn_test` is a default target, so the whole-project build
  step build-only-smokes the loop (incl. `select_readable`) on **all five** legs; the dynamic bridge runs
  on the **native x64** legs only (MSVC/GCC/Clang), exactly like the layer-7/8 bridges.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 9 never touched the kernel
  (Sphere re-validated `f2db95bd…`). No new golden; the guardian golden matrix is unchanged.
- **Full baseline green:** 15/15 receipt gates PASS, 139 property tests, ctest 14/14 GCC+Clang, plus a
  two-process `seads_netserver`/`seads_netclient` smoke (both clients reconstruct `24f71845…c332`).

## 4) Consequences / boundaries

- **What is NOT done (by design):** sends are still **blocking `send_all`** inside the single thread — a
  slow client back-pressures the whole broadcast (fine for loopback + a handful of clients). True async
  output would add per-client send buffers + `select` for writability; not blocked. Per-client
  **independent loss/lag** is still out of scope — `run_client` applies the sealed scripted loss from
  `server_tick`, so the point remains that the *transport* (now with churn) adds nothing.
- **Dynamic-join determinism is honest:** a late joiner **cannot** reconstruct the full session digest —
  it missed early ticks. The claim proved is that the transport delivered **precisely** `frames[K:]`
  (byte-exact suffix), losslessly and correctly offset. That is the strongest true statement, and the
  bridge checks the delivered bytes, not wall-clock.
- **Backward compatibility:** the layer-7/8 bridges (`netloop_bridge`, `multiclient_bridge`), the
  `seads_session_test` digest, and the goldens are all unchanged. `seads_netserver` keeps its
  `[port] [num_clients]` CLI; it now additionally tolerates dynamic join/leave.
- **Next (optional, none blocked):** async single-thread output (writability `select` + per-client send
  buffers); reconnection / late-join **catch-up** (replay the missed prefix to a joiner); per-round hit
  granularity (a kernel event QUEUE — its own ADR); renderer polish (guns + kill-feed in the live
  `--fly` path); or an optional new seal (component/region damage; **B5** ISA atmosphere).
