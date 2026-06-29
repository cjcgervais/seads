# ADR-Step6-Lockstep-v1.3r0 — Loopback lockstep harness (netcode layer 3, the desync tripwire)

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — kernel/tooling
**Seal:** ATM-Sphere v1.3r0 (rides current seal — no rail change, no golden change)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Netcode layers 1 (GEO-001 wire codec) and 2 (20 Hz snapshot framing) are in. Layer 3 is the
**desync tripwire** the multiplayer MVP is built on: two in-process kernels stepped from the
**same input timeline** must produce an **identical per-tick `world_hash`** every tick. A
server-authoritative loop relies on exactly this — if both ends run the sealed kernel from the
same inputs their canonical state stays bit-for-bit identical, and the world_hash diverges the
instant anything desyncs. The kernel is not touched; the canonical hashing snapshot stays the
source of truth. Immutable rails stay green.

## 2) Decision
Add the lockstep harness as a new isolated module + parity gate, same proven shape as
geo001/snapshot (Python reference ↔ generated vectors ↔ C++ test):

- `tools/lockstep_ref.py` — canonical reference: an inline multi-aircraft scripted scenario
  (`LOCKSTEP-SK-001`, 3 aircraft / 600 ticks), `run_lockstep` (two ref kernels, shared
  timeline, per-tick canonical hash, stop at first divergence), `sequence_digest`, and a
  self-test incl. a **negative control** (a perturbed kernel must trip the tripwire).
- `src/net/lockstep.{h,cpp}` — C++ mirror: `tick_hash`, `apply_inputs(a,b,cmd,env,tick)`,
  `run(...)`. Built into a **new `seads_lockstep` library** (see §4) layered on the kernel +
  replay.
- `tools/gen_lockstep_vectors.py` → `src/net/lockstep_vectors.h` — self-contained parity
  vectors: the scenario as exact hex-float envelopes / initial states / schedules, plus the
  expected `SEQUENCE_DIGEST` (SHA-256 over every per-tick hash) and per-tick `CHECKPOINTS`.
- `src/net/lockstep_test_main.cpp` (`seads_lockstep_test`) — drives two C++ kernels through
  the scripted timeline and asserts (1) in-sync every tick, (2) sequence digest == reference,
  (3) checkpoints match, (4) a 1-part-in-a-million altitude desync trips the tripwire.
- `tests/property/test_lockstep.py` — Hypothesis: random shared timelines stay in sync; any
  state/input desync trips; the sealed digest is reproducible.
- Gate wiring: `guardian.yml`, `make_receipt.py` (`lockstep` gate), `CMakeLists.txt`.

**Per-tick hash:** SHA-256 of `Kernel::snapshot(n)` after `n` ticks — the sealed-golden LE-f64
byte layout, **unchanged**. We only hash it per tick; we never alter the snapshot format.

## 3) Rationale
- **Canonical hash, not the wire.** The tripwire compares `Kernel::snapshot()` (raw LE f64,
  the world_hash source of truth), **not** the GEO-001 wire snapshot. The wire is lossy by
  quantization and lives downstream — decoded wire bits are for remotes/rendering and are never
  fed back to advance the sim. Net code stays strictly **outside** the kernel: the shared
  timeline carries sim `Command`s (target bank / climb), never wire bytes.
- **Cross-impl bit-identity, proven exhaustively.** The sequence digest covers **every** tick
  (not just the final snapshot), so the C++ kernel must reproduce the Python reference's whole
  per-tick trajectory bit-for-bit — across the MSVC/GCC/Clang × x64/AArch64 matrix in CI. The
  divergent-tick report is the binary-search handle if a toolchain ever drifts.
- **The tripwire is demonstrably real.** A negative control (perturbed kernel) is asserted to
  trip, so the comparator can never silently pass as a no-op.
- **Byte-reproducible vectors.** Envelopes/states/schedules are exact hex-floats and integers;
  expected hashes come from the deterministic pure-Python reference ⇒ the header is identical
  on every platform ⇒ CI `--check` stays green.

## 4) Consequences
- **+** The foundation every later netcode layer needs (prediction, remote interpolation,
  correction, late-join all assume two ends that provably stay in lockstep from shared inputs).
- **+** New gates: `seads_lockstep_test` (×6 toolchain legs), `lockstep` receipt gate, 4 new
  property tests (32 → 36).
- **+** A reusable per-tick `world_hash` accessor (`lockstep::tick_hash`) for future binary
  searches without changing the sealed snapshot layout.
- **−** One more generated header to keep in sync (`gen_lockstep_vectors.py --check`).
- **Deliberate library split (deviation from the literal handoff suggestion, documented):**
  NEXT_STEPS suggested putting `lockstep.{h,cpp}` "in `seads_net`". The harness **drives** the
  kernel, so it must link `seads_kernel` (→ `det_math`) + `seads_replay`. Folding it into
  `seads_net` would make the **pure wire-codec** lib (which deliberately links *neither*
  det_math nor the kernel, and is what `seads_geo001_test`/`seads_snapshot_test` depend on)
  transitively pull in det_math + the kernel — eroding a documented invariant. So lockstep is
  its **own** lib `seads_lockstep`; `seads_net` stays pure. Placement under `src/net/` is kept
  (it is a net concern); only the linkage is separated.

## 5) Alternatives Considered
- **Put `lockstep.cpp` in `seads_net`.** Rejected — see §4; it would pollute the pure wire-codec
  lib with det_math + kernel. Own lib instead.
- **Hash the GEO-001 wire snapshot per tick.** Rejected: the wire is lossy/quantized; two ends
  could agree on lossy wire bytes while their canonical state differs. The tripwire must compare
  the canonical hashing snapshot.
- **Add a new `config/scenarios/*.json` golden for the lockstep timeline.** Rejected: that would
  enter the golden-seal machinery (and the receipt/guardian scenario loops). Lockstep is a
  harness, not a sealed trajectory — its scenario is defined inline in `lockstep_ref.py` and
  rides seal v1.3r0.
- **Store the full 600-hash sequence in the header.** Rejected as bulky; a single all-ticks
  digest + a handful of checkpoints gives exhaustive coverage with readable localization.

## 6) Acceptance & Probes
All PASS under seal v1.3r0:
- `python tools/spec_monotone_check.py config/rails/atm.json` — rails untouched.
- `python tools/lockstep_ref.py` — reference self-test PASS (in-sync + negative control).
- `python tools/gen_lockstep_vectors.py --check` — header in sync (byte-reproducible).
- `seads_lockstep_test` (GCC + Clang local; full matrix in CI) — in-sync, digest matches
  reference, tripwire trips.
- `python -m pytest tests/property` — 36 passed.
- All 4 goldens (`Sphere`/`Turn`/`Climb`/`TurnClimb`) world_hash — **unchanged**
  (`529c6a05…9218fe16` et al.).

## 7) Ledger Discipline
Receipt records seal v1.3r0, golden_sha256 (unchanged), commit_sha, toolchain matrix, gates
incl. `geo001_codec` + `snapshot_codec` + `lockstep`, signoff Forge.

## 8) Implementation Notes
- `seads_lockstep` links `seads_kernel` + `seads_replay`; `seads_net` (geo001/snapshot) is
  untouched and still links neither det_math nor the kernel.
- The C++ harness sources rails from `golden_params.h` (the sealed scalars) so its kernels are
  bit-identical to the reference; per-tick hash uses tick index `n` = ticks elapsed, matching
  `Kernel::snapshot(n)` on both sides.
- No seal: layer uses the existing kernel/snapshot/replay to spec; no kernel/wire-rail/golden
  edit. The next layers (prediction, remote interpolation, correction, late-join) each get their
  own gated layer + ledger entry.
