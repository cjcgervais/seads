# ADR — Netcode layer 15b: input upstreaming (INPUT-001 wire + canonical command queue, seal ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** **v1.26r0** (a wire reseal — a new sealed
rail block `wire.command`; all 15 goldens byte-identical)

## Context

Every netcode layer so far (5 → 15a) is **server → client**. The authoritative server DRIVES the
sealed kernel from a scripted `Phase` schedule and broadcasts snapshots; a client only ever reads. A
client's socket is watched for exactly one thing — a clean EOF (leave); `reap_leavers_async`
literally discards any `n > 0` upstream bytes as "unexpected client->server bytes … this is a one-way
broadcast" (`broadcast.cpp`).

Layer 15b is the **first bidirectional** layer: a client sends flight `Command`s back **up**, and the
authoritative kernel steps from them. This is the layer the roadmap flagged as "brushes the
determinism rail" — because a network **reorders and re-chunks** bytes, and the kernel's bit-for-bit
promise (CLAUDE.md §1) must survive that. The whole design is therefore an **ordering contract**, not
a transport trick.

The determinism rules (CLAUDE.md §5) are explicit that the wire is lossy and **never fed back as
canonical**. Layer 15b does not violate that: it does not decode DOWNSTREAM state and re-inject it. It
carries **inputs** (`Command`s) — which the kernel already accepts from any source — and makes the
*order* in which they reach the kernel canonical and independent of the transport.

## Decision

### 1. INPUT-001 — a new sealed upstream command wire

A new wire codec (`src/net/input001.{h,cpp}`, mirror `tools/input001_ref.py`) encodes one
tick-stamped `Command`. Field order is the contract:

    apply_tick, aircraft, seq, target_phi, target_g, throttle, fire

* `apply_tick` — the PRE-step tick this command governs (applied on the step advancing
  `apply_tick → apply_tick+1`, the same convention as `session::server_command_at(a, t-1)`).
* `aircraft` — the SoA index the command steers (the server binds a client to its aircraft).
* `seq` — a client-assigned **monotone tag**; the canonical tiebreak (below).
* `apply_tick / aircraft / seq / fire` are exact integers; `target_phi` (radians), `target_g`,
  `throttle` quantize at **1e6**.

It **reuses the sealed GEO-001 pipeline** (`geo001::{quantize, encode_i64, decode_i64}` — ZigZag +
LEB128) ⇒ **ZERO new det_math** (the fourteenth consecutive zero-transcendental seal) and C++ ≡ Python
byte-parity is inherited (pinned by a shared known-encoding vector in `input001_ref.py` +
`netinput_test_main.cpp`). New sealed rail block `config/rails/atm.json → rails.wire.command`
(`phi_scale`/`targetg_scale`/`throttle_scale = 1e6`). Upstream framing reuses the layer-7
`framing::StreamReassembler` (length-prefixed records over the byte stream).

**Why a seal.** INPUT-001 is a new sealed wire format that feeds the authoritative kernel — the same
class of change as the WEAPON-001 wire transport (v1.12r0), which was "a seal only because the wire is
a sealed rail" while all goldens stayed byte-identical. It is *not* pure transport like the layer-7
framing envelope: it introduces new sealed quantization constants on the input path.

### 2. The ordering contract — `CommandQueue` (drop + hold-last)

`src/net/cmdqueue.{h,cpp}` is where transport order is dissolved into a canonical command set:

* **Drop-at-ingest (STALE).** A command whose `apply_tick` is below the queue's **floor** — the next
  tick still to be stepped — is already in the past and is rejected the instant it arrives. Because
  the floor only advances as the authoritative producer *consumes* ticks, "past" is decided by the
  producer's progress, **never by wall-clock**, and a rejected command is byte-for-byte identical to
  one that never arrived. An out-of-range aircraft index is likewise rejected (OUT_OF_RANGE).
* **Canonical selection.** At most one command applies per `(apply_tick, aircraft)`. When several
  share that key the winner is **maximal under a total order on the wire-relevant fields**: `seq`
  first (the client's intent tag), then the quantized `phi / g / throttle / fire`. Because the order
  is over the commands' OWN bytes, the winner is a **pure function of the SET** — submit them in any
  order / any chunking and the same command wins. (Clients SHOULD keep `seq` unique per key; the
  field tiebreak only makes a duplicate-`seq` collision deterministic rather than arrival-ordered.)
* **Hold-last.** A tick with no command for an aircraft reuses that aircraft's last applied command —
  exactly `session::phase_at` semantics. Lives in the producer, not the queue.

**The policy chosen (drop + hold-last)** was picked over clamp-forward because it gives the cleanest
determinism story: the authoritative kernel's output is a pure function of the *applied* canonical
set; buffering lead-time is the client's responsibility. A late command is dropped, not silently
re-timed, so the canonical set never depends on *how* late a packet was.

### 3. `InputProducer` + `broadcast_input` — the authoritative bidirectional server

`src/net/inputserver.{h,cpp}`:

* **`InputProducer`** is `session::FrameProducer`'s twin — the identical op sequence, but each tick's
  per-aircraft `Command` comes from the `CommandQueue` (peek `(pre-step tick, aircraft)`, else
  hold-last) instead of `server_command_at`. Aircraft init (envelope, start state, cadence) comes from
  a `session::Scenario`; the `Phase` schedule is ignored — commands drive it. It reuses the now-exposed
  `session::serialize_world` so its frames are **byte-identical** to `build_server_frames`.
* **`broadcast_input`** is a single-thread `select()` server that, each iteration, (a) reads upstream
  bytes from every client (reassemble → `input001::decode_command` → `CommandQueue::submit`), (b)
  steps the producer one frame, (c) sends that frame downstream. Accepted client sockets stay
  **blocking** with a blocking `send_all` downstream (a cooperative client reads concurrently); the
  layer 11–15a async / cap / liveness machinery is deliberately **out of scope** for this first
  bidirectional cut — `broadcast_input` is a sibling of `broadcast_live`, so `broadcast_live` (and the
  sealed layer-13/14/15a bridges) are **byte-for-byte untouched**.

`session::serialize_world` was moved out of `session.cpp`'s anonymous namespace to `seads::session`
scope (declared in `session.h`) so the input producer emits identical frames. Pure exposure — no
behavior change; `session_reconstruct` / `event_reliable` / all sealed net bridges still reconstruct
`966aca05…`.

### 4. The determinism claim (and its honest scope)

> Given commands that arrive **before their `apply_tick` is stepped** (adequate lead), the
> authoritative kernel's produced frame stream is **invariant to upstream ORDER and CHUNKING** — the
> input-direction analogue of the downstream layers' "lossy ≠ nondeterministic". Late arrival is a
> separate, deterministic-given-progress **drop**.

This is authoritative-server netcode: the server applies what arrived in time; late = dropped. The
digest is not independent of arrival *timing* in general (nor should it be) — it is independent of
arrival *order and framing* for a command set delivered with lead. That is exactly what the bridge
proves.

## Verification

**`seads_netinput_test`** (ctest `netinput_bridge`; native-x64 CI legs like layers 7–15a):

* **codec parity** — `encode_command(5,1,2, 0.5,1.5,0.75,true)` == the 14-byte vector pinned in
  `input001_ref.py`; round-trip exact.
* **LEG 1 (socket order + chunk invariance)** — a grid-exact **INPUT-SK-001** scenario (the three
  SESSION-SK-001 airframes; **dyadic** command values so the lossy wire round-trips bit-for-bit) is
  driven entirely by upstream commands. Two sub-legs — fully **reversed** order at **1 byte/send**
  (maximal reassembler fragmentation), and **rotate-3** order at 7 bytes/send — both produce a frame
  stream **byte-identical to `session::build_server_frames`** of the same scenario. The kernel's
  output is a pure function of the command set, blind to arrival order.
* **LEG 2 (drop + hold-last, in-process)** — the same commands reversed, plus injected STALE
  (past-tick), OUT_OF_RANGE (bad aircraft), and lower-`seq` duplicate commands that must lose, still
  reproduce the canonical frames; queue-level checks pin the STALE / OUT_OF_RANGE / max-seq-winner
  semantics and floor advance.

**Gates (local, gcc + clang):** ctest **20 → 21** (`netinput_bridge`); property tests **205 → 211**
(+6 `test_input001.py`: codec round-trip, dyadic-lossless, queue order-invariance, max-seq winner,
stale drop, oob drop). Rails/roster + det_math oracle + tuning + ceiling all PASS. **All 15 goldens
byte-identical** (Sphere `6914a994…`; no `src/kernel/**`, `src/det_math/**`, or tuning touched); the
sealed session/event digests are unmoved (`966aca05…`). No new golden, no protocol-7 change,
`guardian.yml` unchanged.

## Consequences

* SEADS can now be *flown* by a remote client into the authoritative kernel, deterministically. The
  server-driven `Phase` scenarios are unaffected (they never route through the upstream path), so the
  whole sealed corpus is preserved.
* **Honest boundaries (deferred):** `broadcast_input` is a first cut — blocking downstream, one command
  read per readable event per iteration (fine for the small command volume; a large upstream burst
  spanning many `recv`s would ingest across iterations). Back-pressure / byte-cap / liveness on the
  bidirectional path, and folding input into `broadcast_live`, are future work. Client→aircraft
  binding is positional (the server trusts the `aircraft` field); an authentication/authorization
  layer is out of scope. There is no anti-cheat validation of command values beyond the range/stale
  drop — the kernel clamps `throttle`/`target_phi`/`target_g` as it always has.
* **Next:** a real bidirectional server merging the layer 11–15a output hygiene with the upstream
  path; or input prediction/reconciliation against this authoritative input server.
