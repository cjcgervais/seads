# ADR — Netcode layer 16: bidirectional server — upstream input + downstream output hygiene (`broadcast_bidi`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport-only, no reseal)

## Context

Two arcs met but did not join. The **downstream** arc (layers 5→15a, `broadcast.cpp`) grew a full
suite of output hygiene for a server→client-only stream: per-client userspace send buffers so no
slow client can back-pressure the loop (async, layer 11), an opt-in byte-cap that sheds a client
whose backlog grows without bound (drop-slowest, layer 12), and a liveness timeout that reaps a
silently-dead peer (layer 15a). The **upstream** arc opened with layer 15b (`inputserver.cpp`,
`broadcast_input`): a client sends tick-stamped `Command`s UP into the authoritative sealed kernel,
and the server streams the resulting snapshots back down. But that first bidirectional cut kept the
two halves deliberately minimal — its downstream send is a **blocking `send_all`**, so one slow
client stalls the whole broadcast (every other client's frames wait behind its full kernel buffer),
and a silently-dead peer that never sends EOF is never shed. The handoff since layer 15b named this
merge as the first free pick: *bring the layer 11/12/15a downstream hygiene to the bidirectional
server.*

The determinism stake is the same one that governs the whole netcode arc: the authoritative kernel's
output must stay a pure function of the canonically-ordered command SET. The merge touches only the
**downstream transport** — WHEN each client's bytes move and WHICH misbehaving clients are shed — so
it must leave that guarantee intact.

## Decision

### `broadcast_bidi` — a sibling, not an edit

`src/net/bidiserver.{h,cpp}` adds `broadcast_bidi`: `broadcast_input`'s loop with `broadcast_live`'s
downstream machinery folded in. It is a **SIBLING** of both — it owns its own `BidiClient` struct and
its own `flush_client` / `over_cap` / `enqueue_bytes` / `drop_client` / `reap_dead` helpers (each a
line-for-line mirror of `broadcast.cpp`'s `static` helpers). So the sealed layer-11..15a code in
`broadcast.cpp` and the layer-15b code in `inputserver.cpp` are **byte-for-byte untouched** — the
same "sibling" discipline layer 15b used to keep `broadcast_live` intact. `netinput::Stats` gains two
additive fields, `capped` and `reaped` (0 for `broadcast_input`, which never checks them).

`BidiClient` is the one genuinely new shape: it carries an **upstream** `framing::StreamReassembler`
(reassembling INPUT-001 command records from arbitrary TCP chunks) **beside** a **downstream**
userspace send buffer `buf[off:]` and the three layer-15a liveness fields. Each frame iteration:

1. `on_frame(fi)` — the rendezvous hook (block until a client has sent its commands), exactly
   `broadcast_input`'s contract.
2. one `select_rw()` over `{listener} ∪ {clients readable} ∪ {pending clients writable}`:
   * **listener readable** → ACCEPT (each joiner goes non-blocking; no catch-up prefix — see scope);
   * **client writable** → `flush_client` (push the pending downstream tail; layer-11 async output);
   * **client readable** → **drain all available upstream bytes** (`recv_some` in a
     `do…while(wait_readable(s,0))` loop → `StreamReassembler::feed` → `input001::decode_command` →
     `CommandQueue::submit`), or LEAVE on `recv ≤ 0`.
3. `producer.next()` — step the sealed kernel one frame, its per-tick `Command`s taken from the queue
   with hold-last; `false` ends the stream.
4. ENQUEUE that frame to every client (non-blocking; a fatal send drops), then apply the **byte-cap**
   (a client left above `cap_bytes` is shed: `capped` + `leave`).
5. `reap_dead()` — the **liveness** reap (no receive progress for > `liveness_frames` produced
   frames → `reaped` + `leave`).

After the stream ends, a bounded drain flushes stragglers (still-pending at the deadline → dropped),
exactly `broadcast_async`/`broadcast_live`'s tail.

### The one place the merge differs from `broadcast_live`

In a server→client-only broadcast, a *readable* client means EOF/error (a LEAVE) — `reap_leavers_async`
recvs once and drops on `n ≤ 0`, ignoring any `n > 0` bytes as unexpected. In `broadcast_bidi` a
readable client is **usually sending COMMANDS** (`n > 0`, fed to the `CommandQueue`); only `recv ≤ 0`
is a leave. So the readable-client handler *drains a burst upstream* rather than probing for EOF —
`broadcast_input`'s upstream logic, now on non-blocking clients. Because every `recv_some` is guarded
by a positive readability check (the `select_rw` result, then `wait_readable(s,0)`), it never hits a
spurious `EWOULDBLOCK` (`recv_some` returns `<0` on EWOULDBLOCK — which a non-blocking socket would
otherwise trip), so the same code that reaped by EOF now ingests commands safely.

### Non-blocking clients + upstream ordering

Clients are set non-blocking at accept (for async downstream), but the upstream drain and the tick
ingest keep `broadcast_input`'s ordering exactly: commands are drained BEFORE `producer.next()` each
iteration, and `InputProducer` still consumes `apply_tick == t_` at the step `t_ → t` with hold-last,
so a command delivered before its `apply_tick` is applied on the right tick regardless of the async
send interleaving.

### Honest scope (first bidirectional-hygiene cut)

* **No late-join CATCH-UP** (layers 10/13/14): a spectator joining mid-fight gets only the suffix from
  its join point. A bidirectional catch-up must first settle the positional **client→aircraft
  binding** (layer 15b's other deferral) — who a replayed joiner is allowed to command — so it is
  deliberately out of this cut.
* **Client→aircraft binding stays positional and unauthenticated** — inherited from layer 15b.
* **WHICH** frame a shed/reaped client crosses its threshold is OS-timing (how many frames its buffers
  absorb before `send_some` stalls), exactly like layers 12/15a; deliberately unasserted.

## Verification

* **Bridge `seads_netbidi_test`** (ctest `netbidi_bridge`, native-x64 legs like layers 7–15b), over
  real 127.0.0.1 sockets, no sleeps (cv+notify rendezvous), finite watchdog:
  * **LEG A — the async path preserves upstream order/chunk invariance (the merge anchor):** a single
    cooperative client sends a grid-exact scenario's commands UP SCRAMBLED + adversarially chunked
    (reversed@1 B/send; rotate-3@7 B/send) while the server runs the FULL `broadcast_bidi` async path
    (`cap_bytes = 0`, `liveness = 0`). The downstream frames are **byte-identical to
    `session::build_server_frames`** — the layer-15b claim, now through the userspace send buffers
    (which change WHEN bytes move, never WHICH). `capped == reaped == 0`.
  * **LEG B — liveness reaps a dead client at `cap_bytes == 0` (orthogonal to the byte-cap):** a long
    scenario (~1 MB downstream, ~4000 frames) to TWO clients through a pinned tiny kernel send buffer,
    `cap_bytes = 0` so ONLY liveness can drop anyone. FAST sends the driving commands once and is
    drained every frame by the `on_frame` hook (the netheartbeat pattern) ⇒ never reaped, downstream
    **byte-identical** to `build_server_frames`, and its commands drove the sim (`cmds_ok == n_cmds`,
    `stale == oob == 0`). DEAD reads nothing ⇒ `reaped == 1`, `leaves == 1`, `capped == 0`, its
    delivered bytes a strict byte-**prefix** of the encoded stream; the stream finishes un-wedged.
  * **LEG C — byte-cap sheds a slow client (drop-slowest):** the same long scenario, `liveness = 0`
    and a finite `cap_bytes` so ONLY the cap can drop. FAST survives byte-identical; DEAD's backlog
    exceeds the cap ⇒ `capped == 1`, `leaves == 1`, `reaped == 0`, a strict byte-prefix. The cap
    decides only WHO is dropped, never WHICH bytes flow.
* **Property tests +5 ⇒ 216** (`test_bidi.py`): composes the upstream canonicalisation
  (`input001_ref.CommandQueue`, modelling `InputProducer` tick-exact — peek at pre-step tick,
  consume, hold-last) with the downstream cap/liveness delivery models (mirroring `test_broadcast.py`)
  and proves the two directions are **orthogonal**: the produced frame stream is a pure function of
  the command SET (blind to upstream order), and a downstream cap/liveness drop changes neither the
  produced frames nor any surviving client's delivered bytes; `cap = 0`/`liveness = 0` delivers the
  whole stream under any kernel-acceptance pattern.
* **Gates:** full **ctest 21→22** (all sealed net bridges still reconstruct `966aca05…`); property
  tests **211→216**; **all 15 goldens byte-identical** (Sphere `6914a994…` — no `src/kernel/**`,
  `src/det_math/**`, or tuning touched).

**TRANSPORT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes, protocol-7,
or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event digests unmoved, no seal.**
Diff: NEW `src/net/bidiserver.{h,cpp}`, `src/net/netbidi_test_main.cpp`, `tests/property/test_bidi.py`;
MODIFIED `src/net/inputserver.h` (`Stats` +`capped`/`reaped`), `CMakeLists.txt` (`bidiserver.cpp` into
`seads_netinput`, `seads_netbidi_test` target, `netbidi_bridge` ctest). guardian.yml UNCHANGED
(ctest-only bridge, like layers 13–15a).

## Alternatives rejected

* **Edit `broadcast_live` to take an upstream sink.** Would fold the input path into the sealed,
  bridge-tested downstream loop and force it to distinguish readable-is-command from readable-is-EOF
  everywhere — changing functions layers 13/14/15a depend on for zero benefit. The sibling keeps the
  sealed loop byte-for-byte.
* **Expose `broadcast.cpp`'s `BufClient` + helpers (de-`static`) and reuse them.** Behavior-preserving
  but couples the bidirectional server to the sealed file's linkage; the ~120 lines of well-understood
  buffer/cap/liveness helpers are cheap to mirror, and mirroring keeps each file self-contained (the
  layer-15b precedent).
* **Include catch-up/window in this cut.** Late-join replay for a bidirectional server needs the
  client→aircraft binding settled first (who a replayed joiner may command); bundling it would drag an
  unresolved policy question into a transport merge. Deferred, named as scope.
* **Keep `broadcast_input` and add async as a flag.** A boolean that swaps blocking `send_all` for the
  whole userspace-buffer/cap/liveness pipeline is a second code path inside one function; a distinct
  `broadcast_bidi` (with `broadcast_input` retained as the layer-15b baseline whose bridge still
  passes) is clearer and keeps the 15b proof intact.
