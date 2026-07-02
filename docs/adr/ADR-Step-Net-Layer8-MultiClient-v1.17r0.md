# ADR-Step-Net-Layer8-MultiClient-v1.17r0 — Multi-client fan-out over the socket transport

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events / layer 7)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Layer 7 (`ADR-Step-Net-Layer7-Socket-v1.17r0`) moved the SESSION-SK-001 frame stream over a **real
TCP socket** and proved, with a determinism bridge, that a **single** client reconstructs the dogfight
to the identical in-process `run_session` digest — sockets + framing add zero information and zero
nondeterminism. Its own "What is NOT done" list named the next rung explicitly: the sockets were
**blocking** and the demo was **one client** — no non-blocking accept, no fan-out to many clients.

Layer 8 closes that: **one server broadcasts the frame stream to N independent clients, each over its
own TCP connection with its own `StreamReassembler`, and every client reconstructs the byte-identical
digest.** This is the natural determinism statement for a real multiplayer session — a shared
authoritative stream fanned out to many spectators/players must reconstruct the same fight for
everyone, regardless of connect order or how each client's `recv()` happens to chunk the stream.

## 2) Decision

**(a) Two portable non-blocking socket primitives (`src/net/socket`).**

- `bool set_nonblocking(socket_t)` — `ioctlsocket(FIONBIO)` on Windows, `fcntl(O_NONBLOCK)` on POSIX.
- `bool wait_readable(socket_t, int timeout_ms)` — a portable `select()` readability wait (Winsock
  ignores `nfds`; POSIX passes `s+1`; `timeout_ms < 0` blocks forever). Returns true iff the socket is
  readable within the timeout; false on timeout or error.

Together they let a fan-out server accept clients **without wedging** on an absent one: it waits for
the listener to become readable (a pending connection) and only then calls `accept_one()`, bounded by
a finite deadline. No busy-spin, no indefinite block. Both are pure OS-socket calls — TRANSPORT,
outside the kernel + `world_hash`, no `det_math`.

**(b) The multi-client determinism BRIDGE (`seads_multiclient_test`).** Mirrors the layer-7 bridge but
fans out. The reference is again the sealed in-process `session::run_session(...).digest` (**not
re-derived**). The server binds `:0`, publishes the OS-assigned port over a `mutex` +
`condition_variable` (`notify_all`, since N client threads wait on it — **not** `std::future`, per the
MinGW `call_once` caveat), sets the listener **non-blocking**, and accepts `N` connections with a
`wait_readable` + `accept_one` loop (finite deadline ⇒ fail-not-wedge). It then **broadcasts the
identical lossless frame stream** (`build_server_frames` — every frame incl. tick 0, `encode_frame`'d)
to each connection via `send_all`. N client threads each connect, reassemble their **own** byte stream,
rebuild the `ServerFrames` list keyed on each frame's **decoded `server_tick`**, and run the **same**
`run_client()` (reusing the layer-7 `session.cpp` split — zero reimplementation). The leg asserts
**every** client digest equals the in-process reference (and hence each other). A finite watchdog fails
rather than wedges on any socket hang. `N = 3` — a small fan-out, enough to prove the per-client
invariant.

**(c) The demo server gains fan-out (`seads_netserver [port] [num_clients]`).** `num_clients` defaults
to 1 (backward-compatible with the layer-7 one-client demo); with `>1` the server accepts that many
connections, then broadcasts the identical stream to each. `listen_loopback` now takes the backlog =
`num_clients` so all pending connects queue.

**Why this is a NO-SEAL change.** Nothing on the sealed path is touched: the framing envelope, the
protocol-6 snapshot bytes, the wire scales, the rails, `det_math`, the kernel, and every golden are
unchanged. Fan-out is pure transport (N sockets + a broadcast loop over the existing byte stream + a
bridge), the same category interp / predict / session / events / layer-7 ride the current seal as.

## 3) Verification (gates)

- **Determinism bridge:** `seads_multiclient_test` (ctest `multiclient_bridge`, under the existing
  `option(SEADS_SOCKET_TESTS ON)` + `if(NOT CMAKE_CROSSCOMPILING)` guard alongside `netloop_bridge`)
  asserts all **3** clients reconstruct the in-process `run_session` digest (`24f71845…c332` locally,
  GCC **and** Clang) over real 127.0.0.1 sockets — 41 frames broadcast to each.
- **Property test** `tests/property/test_framing.py` (**+1 ⇒ 133**): `test_fanout_all_clients_identical`
  — one server stream, N clients each reading it in a different (Hypothesis-drawn) chunk size, every
  client's independent `StreamReassembler` emits the byte-identical frame list. The pure-codec form of
  the end-to-end socket claim.
- **Gates wired:** `guardian.yml` — `seads_multiclient_test` is a default target, so the whole-project
  build step build-only-smokes the Winsock/BSD `#ifdef` (incl. the new `set_nonblocking`/`wait_readable`
  primitives) on **all five** legs; the fan-out bridge is run on the **native x64** legs only (MSVC/GCC/
  Clang), exactly like the layer-7 bridge (arm64 legs are emulated/foreign-run for hashing). Local:
  ctest **13/13** (was 12/12; + `multiclient_bridge`) under GCC and Clang.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 8 never touched the kernel
  (Sphere re-validated locally, `f2db95bd…`). No new golden; the guardian golden matrix is unchanged.
- **Full baseline re-run green:** 15/15 receipt gates PASS, 133 property tests, ctest 13/13 GCC+Clang.

## 4) Consequences / boundaries

- **What is NOT done (by design):** the client-facing sends are still **blocking** `send_all` — fine for
  loopback and a handful of clients, and correct because a broadcast to a slow client back-pressures
  only that send, not the reconstruction (each client is an independent thread here). A true async
  server (a single-threaded `select`/`poll` broadcast loop with per-client send buffers), dynamic
  join/leave mid-session, and reconnection are future work; none is blocked. The `wait_readable`
  primitive is the seed for that loop.
- **Fan-out is lossless in the bridge:** the sealed per-client lag + drop set is applied by
  `run_client` from `server_tick` (as in layers 5–7), so every client applies the **same** scripted
  loss — the point of this layer is that the *transport* fan-out adds nothing, not that each client
  sees a different network. Per-client independent loss injection would be a separate change.
- **Backward compatibility:** `listen_loopback` gained a defaulted `backlog` (was already a defaulted
  param); `seads_netserver` gained a defaulted `num_clients` arg. The layer-7 `seads_netloop_test`,
  `seads_netserver`/`netclient` one-client path, and `seads_session_test` digest are all unchanged.
- **Next (optional):** async/select-based single-thread broadcast + dynamic join/leave; per-round hit
  granularity (a kernel event QUEUE — its own ADR); renderer polish (guns + kill-feed in the live
  `--fly` path); or an optional new seal (component/region damage; **B5** ISA atmosphere). None blocked
  by this layer.
