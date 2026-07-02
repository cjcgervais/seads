# ADR-Step-Net-Layer7-Socket-v1.17r0 — Cross-process socket transport (stream framing + real TCP)

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Netcode layers 1–6 built the whole replication stack *in one process*: the GEO-001 codec (l1–2), the
20 Hz snapshot (l2 + KIN/WEAPON), the loopback lockstep tripwire (l3), remote interpolation (l4a),
client-side prediction (l4b), the server↔client SESSION loop (l5), and the reliable combat-EVENT
channel (l6). Layers 5–6 finally *used* a transport between two endpoints — but that transport was an
**in-process** object (`session_ref.Transport`, a Python dict / a C++ `frame_at` lookup): frames were
handed directly from the server half to the client half in the same address space, with integer lag
and a deterministic drop set standing in for a network.

The one thing never exercised: **a real byte stream between two OS processes.** A TCP stream has no
message boundaries — a single `recv()` may return half a frame, or two frames plus a sliver of a
third — so the self-delimiting protocol-6 snapshot, which is fine when handed over whole, needs an
outer framing to survive arbitrary chunking. Layer 7 adds exactly that (a length-prefixed envelope +
dependency-free BSD/Winsock sockets) and proves, with a determinism bridge, that moving the bytes
over a real socket changes **not one bit** of the reconstructed dogfight.

## 2) Decision

**(a) A strictly-OUTER length-prefixed framing (`src/net/framing`, `tools/framing_ref.py`).**

```
stream = concat over frames of ( LEB128(len(payload)) || payload )
```

where `payload` is a whole `encode_snapshot()` protocol-6 frame. Layer 7 **never looks inside the
payload** — it is opaque bytes. The length prefix is an **unsigned** LEB128 varint (lengths are
non-negative), reusing the **sealed** `geo001::leb128_encode_u64` / `leb128_decode_u64` verbatim, so
C++ ≡ Python parity is free and no new codec is introduced.

The reassembler (`StreamReassembler`) is a **pure function of the concatenated byte stream**:
`feed(chunk)` depends only on `(carry_buffer, chunk)`. Invariant — for any partition `C1..Cn` of a
stream `S`, `reassemble(C1..Cn)` yields the identical frame sequence as feeding `S` whole. The one
fragile spot is that **the length prefix itself can split across chunks** (1..10 bytes): the
reassembler buffers a partial *prefix*, distinguishing **truncated** (ran out mid-varint → wait for
more) from **overlong** (>10 bytes, no terminator → hard error, mirroring `leb128_decode_u64`'s bound
bit-for-bit). It never emits a frame until all `len` payload bytes are present, retaining the tail.

**(b) A minimal blocking-TCP socket wrapper (`src/net/socket`), dependency-free.** BSD sockets on
POSIX and Winsock2 on Windows behind one `#ifdef _WIN32` boundary — no asio/boost. Highlights: a
refcounted RAII `WsaGuard` (WSAStartup/WSACleanup on Windows, no-op on POSIX); `socket_t` = `SOCKET`
/ `int` with an `is_valid()` helper; `send_all()` loops until every byte is out (a single `send()`
may be short on a multi-KB frame); `recv_some()` returns whatever arrived (the reassembler tolerates
any chunk size, so no `recv_all` is needed); `SO_REUSEADDR` before `bind`; SIGPIPE avoided via
`MSG_NOSIGNAL` on POSIX. The wire is **endian-neutral by construction** (LEB128 varints), so payload
bytes are never byte-swapped; only `sin_port`/`sin_addr` use network order (`htons`,
`INADDR_LOOPBACK`).

**(c) The determinism BRIDGE (`seads_netloop_test`).** The reference is the sealed in-process session
digest — `session::run_session(rails, SESSION-SK-001, reconcile=true).digest` computed in-process
here, **not re-derived**. To reuse the exact reconstruction logic (zero drift), `session.{h,cpp}` was
split into `build_server_frames()` (server half) + `run_client()` (client half), with `run_session()`
now their composition. The bridge reproduces the sealed lossy scenario (**OPTION 2a**): the server
sends **every** emitted frame losslessly over a real `127.0.0.1` socket (including the tick-0 pre-step
frame); the client reassembles them, rebuilds the `ServerFrames` list **keyed on each frame's decoded
`server_tick`**, and hands it to the **same** `run_client()` — which applies the sealed integer lag +
drop set purely from `server_tick`/`t`, never from wall-clock arrival. So the socket path is
byte-identical to the in-process path: **timing, jitter, and coalescing cannot change a bit.** The
OS-assigned port (`bind :0` → `getsockname`) is handed to the client via a `mutex` +
`condition_variable` ready-signal handshake; a finite watchdog fails the leg rather than wedging if a
socket op ever hangs.

**(d) A two-process human demo (`seads_netserver` / `seads_netclient`).** The server streams the
SESSION-SK-001 frames; the client reconstructs the dogfight and prints the whole-session digest,
which over a lossless loopback equals the sealed in-process digest.

**Why this is a NO-SEAL change.** The framing is a *strictly outer* envelope around the existing
protocol-6 frame — the snapshot bytes, the wire scales, the rails, `det_math`, the kernel, and every
golden are untouched. Layer 7 is pure transport (integer/byte framing + OS sockets + a bridge),
exactly the category that interp / predict / session / events ride the current seal as. The outer
length prefix is **not** part of the sealed snapshot rail — a decoder that strips it recovers the
identical protocol-6 bytes.

## 3) Verification (gates)

- **Mirror-first.** `tools/framing_ref.py` (encode_frame + `StreamReassembler` + self-test) written
  and green first; `src/net/framing.{h,cpp}` mirrors it, reusing the sealed LEB128.
- **Cross-impl parity gate** (mirrors det_math/geo001/snapshot exactly): `tools/gen_framing_vectors.py`
  → `src/net/framing_vectors.h` (byte literals + integer spans ⇒ byte-reproducible, CI `--check`
  stable) → `src/net/framing_test_main.cpp` (`seads_framing_test`, ctest `framing_byteexact`). Batches:
  empty, one small frame, several small, **one large frame forcing a multi-byte LEB128 length**, and a
  mixed stream. For each, encode byte-parity + **every** chunking (whole / 1-byte / every-3-bytes /
  on-each-prefix-boundary / one-byte-into-each-prefix / giant-chunk-plus-sliver) yields byte-identical
  frames, plus a partial-frame-buffered assertion and an overlong-prefix reject.
- **Determinism bridge:** `seads_netloop_test` (ctest `netloop_bridge`, guarded
  `if(NOT CMAKE_CROSSCOMPILING)` / `option(SEADS_SOCKET_TESTS ON)`) asserts the socket-path client
  digest **equals** the in-process `run_session` digest over a real 127.0.0.1 socket
  (`24f71845…c332` locally, GCC **and** Clang).
- **Property tests** `tests/property/test_framing.py` (**+4 ⇒ 132**): reassembly round-trip,
  chunk-boundary invariance under any fixed chunk size, partial-frame-buffered at an arbitrary cut
  (emitted frames are always a prefix of the full list; feeding the rest completes it exactly), and
  overlong-prefix reject.
- **Gates wired:** `guardian.yml` — `gen_framing_vectors.py --check` in the sync block +
  `framing_ref.py` self-test + `seads_framing_test` on **all five** legs (MSVC/GCC/Clang × x64/AArch64)
  + `seads_netserver/netclient/netloop` build-only-smoked on every leg (default targets, so the build
  step catches Winsock/BSD `#ifdef` breakage) + `seads_netloop_test` run on the **native x64** legs
  only (arm64 legs are emulated/foreign-run for hashing). Local: ctest **12/12** (was 10/10; +
  `framing_byteexact` + `netloop_bridge`) under GCC and Clang.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 7 never touched the
  kernel. No new golden; the guardian golden matrix is unchanged (it gains only the framing test leg +
  the x64 bridge leg).

## 4) Consequences / boundaries

- **What is NOT done (by design):** the sockets are **blocking** and the demo is loopback / one-client
  — no async I/O, no multi-client fan-out, no reconnection, no real-network loss injection over the
  socket (the sealed drop set is applied by the client from `server_tick`, exactly as in layers 5–6).
  Non-blocking I/O, a select/poll loop, and multi-client sessions are future work; none is blocked.
- **`session.cpp` refactor is behavior-preserving:** `run_session` is now
  `run_client(build_server_frames(...))` — the existing `seads_session_test` (which calls
  `run_session`) is byte-identical (still `24f71845…`); the split only exposes the two halves so the
  socket bridge and the demo binaries reuse the exact reconstruction (no reimplementation → no drift).
- **Frame boundaries vs. `server_tick`:** the framing carries no tick tag of its own — `server_tick`
  already rides inside the payload (snapshot header), so the client keys delivery on the decoded tick.
  The outer prefix carries only the payload length; nothing about lag/drop lives in the transport.
- **Portability caveat noted:** `std::future`/`std::promise` was avoided in the bridge (it pulls
  `call_once` and trips some MinGW libstdc++ links) in favour of a plain `condition_variable`
  handshake; `Threads::Threads` is already linked globally and Winsock needs `ws2_32` (added for
  `seads_socket` under `if(WIN32)`).
- **Next (optional):** non-blocking / multi-client sockets; per-round hit granularity (a kernel event
  QUEUE — its own ADR); renderer polish (guns + kill-feed in the live `--fly` path); or an optional new
  seal (component/region damage; **B5** ISA atmosphere). None blocked by this layer.
