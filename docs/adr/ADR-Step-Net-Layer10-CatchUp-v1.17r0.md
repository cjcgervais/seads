# ADR-Step-Net-Layer10-CatchUp-v1.17r0 — Late-join catch-up (prefix replay) over the select() broadcast

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events / layers 7–9)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Layer 9 (`ADR-Step-Net-Layer9-DynamicBroadcast-v1.17r0`) made the fan-out a genuine single-threaded
`select()` broadcast loop with dynamic join/leave, and proved the honest determinism statement for a
late joiner: it receives **exactly** the contiguous frame suffix `frames[K:]` from its join point. But
its "boundaries" section named the gap plainly: *"a late joiner **cannot** reconstruct the full session
digest — it missed early ticks,"* and listed the follow-up: *"reconnection / late-join **catch-up**
(replay the missed prefix to a joiner)."*

Layer 10 closes that gap. When a client joins mid-stream at frame `K`, the server first **replays the
missed prefix `frames[0:K]`** and only then feeds it the live suffix `frames[K:]`. The joiner therefore
receives the **whole stream `frames[0:]`** — byte-identical to a client present from frame 0 — and can
reconstruct the **same** sealed SESSION-SK-001 digest (`24f71845…c332`). The late joiner now sees the
whole fight.

## 2) Decision

**(a) Opt-in catch-up on the broadcast server (`src/net/broadcast.{h,cpp}`).**
`netbcast::broadcast_select(...)` gains a final parameter `bool catchup = false` (default preserves
layer-9 behavior exactly). The accept path (`accept_pending`) now takes the payload list, the count
`upto` of already-sent frames, and the flag:

- On accepting a client mid-stream at frame `fi`, when `catchup && upto>0`, the server calls a new
  `catch_up_client(c, payloads, /*upto=*/fi)` which sends `frames[0:fi]` each as **one atomically
  length-prefixed frame** (`framing::encode_frame`, the same envelope the live stream uses) — so the
  joiner's byte stream is `encode_stream(frames[0:fi]) ++ encode_stream(frames[fi:])`, which is exactly
  `encode_stream(frames[0:])`. It then enters the live broadcast set and receives `frames[fi:]`.
- **No off-by-one:** the joiner accepted at the top of frame `fi`'s iteration is replayed `frames[0:fi]`
  (the `[0, fi)` half-open prefix) and then receives the live `frames[fi], frames[fi+1], …` in the same
  and subsequent iterations ⇒ every frame exactly once.
- **Initial gather (before frame 0):** `accept_pending` is called with `upto=0`, so the min_initial
  clients are **never** replayed (they are present from the start and get everything live). Catch-up is
  strictly a mid-stream affordance.
- A joiner that **dies during the replay** is closed and **not** added to the broadcast set (no phantom
  join counted): `catch_up_client` returns false ⇒ `continue`.

The replay is a **synchronous burst** on the accepting `select` iteration (bounded by the prefix
length). A slow catch-up joiner therefore back-pressures the broadcast for that iteration; per-client
async send buffers (writability `select`) remain a separate deferred layer. Still TRANSPORT — outside
the kernel + `world_hash`, no `det_math`; reuses `seads_broadcast` = `seads_framing` + `seads_socket`.

**(b) The late-join catch-up determinism BRIDGE (`seads_netcatchup_test`).** Reference is again the
sealed in-process `session::run_session(...).digest` + the exact `build_server_frames` payload list
(**not re-derived**). One server thread runs `broadcast_select(..., catchup=true)` over 41 real
127.0.0.1 frames; two client threads:

- **EARLY** — connects before frame 0 (`min_initial=1`), reads to EOF: receives all 41 frames, rebuilds
  the `ServerFrames` list keyed on each frame's **decoded `server_tick`**, runs the **same**
  `run_client()`, and its digest **==** the in-process reference (`24f71845…c332`, GCC **and** Clang) —
  proving catch-up mode doesn't perturb an already-present client.
- **CATCHUP** — waits for the server to reach frame `J = N/2` (the `on_frame` hook pauses the server
  until the catch-up client's `connect()` has returned, so the join is pinned with **no timing race**),
  then reads prefix-replay + live suffix to EOF. The bridge asserts its payloads are **byte-identical to
  EARLY's** (i.e. exactly `frames[0:]`, `pending==0`) **and** that its reconstructed digest **==** the
  same sealed reference. The late joiner reconstructs the whole fight.

Server stats are asserted: every frame sent, exactly **2 joins**. A finite watchdog fails rather than
wedges on a socket hang.

**(c) The demo server exposes catch-up (`seads_netserver [port] [num_clients] [catchup]`).** A third
optional CLI arg (`0`/`1`, default `0`) flips `broadcast_select`'s `catchup` flag — the human demo
shares the same loop (no untested divergence), so a client joining mid-stream with `catchup=1` gets the
whole dogfight.

**Why NO-SEAL.** Nothing sealed is touched: the framing envelope, the protocol-6 snapshot bytes, wire
scales, rails, `det_math`, the kernel, and every golden are unchanged. Replaying already-emitted frames
through the existing length-prefix envelope is pure transport over the existing byte stream — the same
category as interp / predict / session / events / layers 7–9.

## 3) Verification (gates)

- **Determinism bridge:** `seads_netcatchup_test` (ctest `netcatchup_bridge`, under the existing
  `option(SEADS_SOCKET_TESTS ON)` + `if(NOT CMAKE_CROSSCOMPILING)` guard alongside `netloop_bridge` /
  `multiclient_bridge` / `netdyn_bridge`): EARLY and CATCHUP both reconstruct `24f71845…c332`, their
  delivered streams byte-identical — GCC **and** Clang, 12/12 (gcc) + 8/8 (clang) stress reruns clean.
  Local ctest **15/15** (was 14/14; + `netcatchup_bridge`) both toolchains.
- **Layer-9 regression:** `broadcast_select`'s `catchup` defaults to `false`, so `netdyn_bridge`
  (full/late/leaver) is behaviorally unchanged and still passes (4/4 stress). The layer-7/8 bridges are
  unaffected.
- **Property test** `tests/property/test_broadcast.py` (**+2 ⇒ 141**): a catch-up delivery model
  (`broadcast_delivery_catchup`: a joiner at `j` receives prefix `[0, j)` ++ live `[j, leave)`) proved
  to equal `range(0, leave)` for every join point (so a to-EOF catch-up joiner gets the whole stream,
  identical to a `join=0` client under the layer-9 model), plus composition with the layer-7 framing
  codec (`encode_stream(frames[0:j]) ++ encode_stream(frames[j:]) == encode_stream(frames[0:])`,
  reassembling byte-exact under any TCP chunking). The timing-independent core of `netcatchup_bridge`.
- **Gates wired:** `guardian.yml` — `seads_netcatchup_test` is a default target, so the whole-project
  build step build-only-smokes it on **all five** legs; the catch-up bridge runs on the **native x64**
  legs only (MSVC/GCC/Clang), exactly like the layer-7/8/9 bridges.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 10 never touched the kernel
  (Sphere re-validated `f2db95bd…`). No new golden; the guardian golden matrix is unchanged.
- **Full baseline green:** 15/15 receipt gates PASS, 141 property tests, ctest 15/15 GCC+Clang.

## 4) Consequences / boundaries

- **What is NOT done (by design):** the replay is **synchronous** — a slow catch-up joiner
  back-pressures the broadcast for its accepting iteration (fine for loopback + a handful of clients).
  True async output (per-client send buffers + writability `select`) is the next rung; not blocked.
- **Catch-up determinism is honest:** because the server replays already-emitted, unchanged frames
  through the same envelope, the catch-up joiner's stream is *provably* the whole `frames[0:]` (the
  bridge checks delivered bytes, not wall-clock), so it reconstructs the identical sealed digest. There
  is no interpolation or state fast-forward — it is a literal replay of the frame history the server
  already holds (`build_server_frames`).
- **Backward compatibility:** default `catchup=false` reproduces layer 9 exactly; the layer-7/8/9
  bridges, the `seads_session_test` digest, and all goldens are unchanged. `seads_netserver` keeps its
  `[port] [num_clients]` CLI and gains an optional `[catchup]` arg.
- **Next (optional, none blocked):** async single-thread output (writability `select` + per-client send
  buffers, so a slow client can't back-pressure the broadcast); per-round hit granularity (a kernel
  event QUEUE — its own ADR); renderer polish (guns + kill-feed in the live `--fly` path); or an
  optional new seal (component/region damage; **B5** ISA atmosphere).
