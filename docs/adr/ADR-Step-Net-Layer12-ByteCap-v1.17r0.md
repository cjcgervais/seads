# ADR-Step-Net-Layer12-ByteCap-v1.17r0 — Send-buffer byte-cap + drop-slowest over the async broadcast

**Status:** Accepted
**Date:** 2026-07-01
**Author:** Forge (Claude) — Forge/Auditor
**Seal:** none — **NO-SEAL net layer, rides ATM-Sphere v1.17r0** (like interp / session / events / layers 7–11)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context

Layer 11 (`ADR-Step-Net-Layer11-AsyncOutput-v1.17r0`) removed all output back-pressure: every
client owns a userspace send buffer, so a slow client accumulates buffer instead of stalling the
broadcast. Its "boundaries" section named the cost plainly: *"the per-client buffer is unbounded
(in practice bounded by the total encoded stream, since frames are precomputed) — an explicit
byte-cap / drop-slowest policy for open-ended live streams is the honest boundary this layer
leaves,"* and the handoff was explicit: *"add the byte-cap policy BEFORE pointing this at an
open-ended live stream."*

Layer 12 is that rung — the live-stream hygiene policy. A permanently-slow client can no longer
grow server memory without bound: the moment its pending backlog exceeds an opt-in per-client
**byte-cap**, it is **shed** (drop-slowest). The cap decides only **WHO is dropped, never WHICH
bytes flow**: every surviving client's delivered bytes are untouched, and a shed client's delivered
bytes are always a clean **byte-prefix** of the encoded stream.

## 2) Decision

**(a) The policy (`src/net/broadcast.{h,cpp}`).** `netbcast::broadcast_async` gains a final
`std::size_t cap_bytes = 0` (default **0 = unbounded = layer-11 behavior EXACTLY**, the same
opt-in-default pattern `catchup` used at layer 10). `broadcast_select` is untouched (layers 9/10
verbatim; their bridges keep gating it). Semantics:

- **Drop threshold:** whenever a frame enqueue (opportunistic flush included) leaves a client's
  **pending userspace backlog** above `cap_bytes`, that client is dropped — closed, counted in a
  new `Stats.capped`, and (being a live member) also counted as a `leave`, exactly like a send
  failure. The check mirrors the enqueue order bit-for-bit in the reference model (below).
- **Uniform policy:** the cap applies to the **catch-up prefix replay** too — a joiner it trips
  during `accept_pending_async`'s prefix enqueue is closed before ever becoming live (counted in
  `capped` only; it was never a member, so it is not a `leave` — the same accounting as a joiner
  that dies mid-replay).
- **Prefix shape of a shed client:** delivery stops at the bytes the kernel had accepted — a
  contiguous prefix of the encoded stream; the pending tail is discarded whole. The cap never
  reorders, corrupts, or skips.
- **Memory now tracks the backlog:** `flush_client` compacts the consumed prefix of the buffer on
  every flush (layer 11 compacted only on full drain), so per-client memory is O(pending backlog) —
  the quantity the cap bounds — not O(total bytes ever flushed). Delivered bytes are unaffected
  (compaction is internal layout only).
- The **drain phase needs no cap check**: nothing is enqueued there, so a backlog only shrinks; the
  layer-11 progress-bound + consecutive-idle-cap drain applies unchanged to survivors.

**(b) The byte-cap determinism BRIDGE (`seads_netcap_test`).** Two legs, all rendezvous
cv+`notify_all` (no `std::future` — MinGW `call_once` caveat), **no sleeps**:

- **LEG 1 (drop-slowest under volume):** a ~7.5 MiB synthetic stream (512 × 15 KiB — one encoded
  frame stays strictly under the pinned 16-KiB kernel send buffer, which makes the pacing
  rendezvous provably deadlock-free) to **two** clients through `cap_bytes = 1 MiB`. **FAST** reads
  continuously, *rate-coupled to the frame loop via the `on_frame` hook* (the loop doesn't enqueue
  frame `fi` until FAST has consumed everything up to frame `fi−32`), so FAST's backlog is provably
  bounded ≈ 33 encoded frames ≈ 507 KiB < cap/2 — FAST can never be the one capped,
  deterministically, with no sleeps. **SLOW** reads nothing. Asserted: the server finishes **every**
  frame un-wedged with `joins=2, capped=1, leaves=1` (exactly SLOW shed); FAST's reassembled stream
  is **byte-identical** to the frame list; SLOW's delivered bytes are a **strict byte-prefix** of
  the encoded stream. WHICH frame index SLOW is shed at is OS-timing (how many bytes its kernel
  buffers absorbed) — deliberately unasserted. A capless layer-11 server would instead buffer ~7 MiB
  for SLOW and deliver it all — `seads_netasync_test` still gates exactly that, so the two bridges
  pin both sides of the policy boundary.
- **LEG 2 (the cap does not perturb the healthy path):** the layer-11 sealed-session shape — EARLY
  (from frame 0) + CATCHUP (joins at frame J=20, prefix enqueued) — run through
  `broadcast_async(catchup=true, cap_bytes=8 MiB)`. Both receive the whole 41 frames
  byte-identically, both reconstruct the sealed SESSION-SK-001 digest (`24f71845…c332`), and
  `capped == 0`: enabling the policy on clients that keep up changes nothing.

**(c) The demo server exposes it (`seads_netserver [port] [num_clients] [catchup] [async]
[cap_bytes]`).** A fifth optional CLI arg (default `0`; applies to the async path) — the human demo
shares the same loop the CI bridge gates (no untested divergence).

**Why NO-SEAL.** Nothing sealed is touched: the framing envelope, the protocol-6 snapshot bytes,
wire scales, rails, `det_math`, the kernel, and every golden are unchanged. Deciding to stop
serving a client that cannot keep up is pure transport policy over the existing byte stream — the
same category as layers 7–11.

## 3) Verification (gates)

- **Determinism bridge:** `seads_netcap_test` (ctest `netcap_bridge`, under the existing
  `option(SEADS_SOCKET_TESTS ON)` + `if(NOT CMAKE_CROSSCOMPILING)` guard): both legs PASS — GCC
  **and** Clang, **12/12 (gcc) + 8/8 (clang)** stress reruns clean. Local ctest **17/17** (was
  16/16; + `netcap_bridge`) both toolchains.
- **Layer-9/10/11 regression:** `broadcast_select` untouched; `cap_bytes=0` (every existing caller)
  is the layer-11 code path exactly — `netdyn_bridge`, `netcatchup_bridge`, `netasync_bridge` all
  still pass.
- **Property tests** `tests/property/test_broadcast.py` (**+2 ⇒ 145**): the capped send-buffer
  model `sendbuffer_deliver_capped` (mirrors the enqueue → flush → `over_cap` order bit-for-bit)
  proves (i) for ANY kernel-acceptance pattern and ANY cap, a shed client's delivered bytes are a
  clean **byte-prefix** of `encode_stream(frames)`, a surviving client's delivery is the **whole**
  stream == the layer-11 model, and `cap=0` disables the policy bit-for-bit; (ii) **healthy-client
  immunity** — a client whose kernel always accepts everything offered is never dropped by any cap.
  The timing-independent core of the bridge.
- **Gates wired:** `guardian.yml` — `seads_netcap_test` is a default target, so the whole-project
  build step build-only-smokes it on **all five** legs; the byte-cap bridge runs on the **native
  x64** legs only (MSVC/GCC/Clang), exactly like the layer-7/8/9/10/11 bridges.
- **Determinism invariant held:** **all 10 goldens byte-identical** — layer 12 never touched the
  kernel. No new golden; the guardian golden matrix is unchanged.
- **Full baseline green:** 15/15 receipt gates PASS, 145 property tests, ctest 17/17 GCC+Clang.

## 4) Consequences / boundaries

- **What is NOT done (by design):** the policy is **binary** (shed at the cap) — no
  priority/downgrade tiers (e.g. skipping frames for a laggard while keeping it connected), no
  per-client cap overrides, and no reconnect-with-catch-up flow for a shed client (a shed client
  that reconnects is simply a new joiner; with `catchup=true` it gets the prefix replay and a fresh
  cap). Frame-skipping would break the "delivered bytes are a prefix of the same stream" statement
  and is a different, lossier contract — its own rung if ever wanted.
- **The layer-11 statement is now conditional, honestly:** *delivery* of the whole stream to every
  client holds when `cap_bytes=0` (or for every client under the cap); with a cap, the guarantee
  becomes *whole stream to every client that keeps up, clean prefix + disconnect to those that
  don't* — which is the correct contract for an open-ended live stream.
- **Backward compatibility:** every existing call site compiles and behaves identically
  (`cap_bytes` defaults to 0); `seads_netserver` keeps its `[port] [num_clients] [catchup] [async]`
  CLI and gains an optional `[cap_bytes]`.
- **Next (optional, none blocked):** per-round hit granularity (a kernel event QUEUE — its own
  ADR); renderer polish (guns + kill-feed in the live `--fly` path); an optional new seal
  (component/region damage; **B5** ISA atmosphere); or further live-stream rungs (an actual
  open-ended frame source feeding `broadcast_async` incrementally instead of a precomputed list).
