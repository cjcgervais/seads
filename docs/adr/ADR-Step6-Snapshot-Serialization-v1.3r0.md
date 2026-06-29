# ADR-Step6-Snapshot-Serialization-v1.3r0 — GEO-001 snapshot serialization (netcode layer 2)

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge — kernel/tooling
**Seal:** ATM-Sphere v1.3r0 (rides current seal — no rail change, no golden change)
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Netcode layer 1 (the GEO-001 wire codec) is in. Layer 2 frames a **multi-aircraft world
snapshot** over that codec at the 20 Hz snapshot cadence (physics stays 100 Hz) — the
transport a server emits and remotes interpolate. GEO-001 is a sealed rail; this *uses* it,
no reseal. The kernel is not touched; the canonical hashing snapshot stays the source of
truth. Immutable rails must all stay green.

## 2) Decision
Add the snapshot framing as a new isolated module + parity gate, same proven shape as
geo001/det_math (Python reference ↔ generated vectors ↔ C++ byte-exact test):

- `tools/snapshot_ref.py` — canonical reference: `EntityState` (degrees), `Snapshot`,
  `from_kernel`/`to_kernel` (rad↔deg via exact hex-float constants), `encode_snapshot`/
  `decode_snapshot`. Self-test.
- `src/net/snapshot.{h,cpp}` — C++ mirror, built into the existing `seads_net` lib, layered
  on the `geo001` codec.
- `tools/gen_snapshot_vectors.py` → `src/net/snapshot_vectors.h` — parity vectors: snapshots
  with hex-float degree fields → expected wire bytes (byte-reproducible), plus a conversion
  table (kernel radians → expected quantized i64).
- `src/net/snapshot_test_main.cpp` (`seads_snapshot_test`) — byte-identical wire on encode,
  round-trip on decode, bit-exact rad→quantize conversion (4 snapshots, 5 conv rows).
- `tests/property/test_snapshot.py` — Hypothesis: multi-entity round-trip within one quantum,
  self-delimiting framing, empty world, from/to_kernel inversion, protocol gating.
- Gate wiring: `guardian.yml`, `make_receipt.py` (`snapshot_codec` gate), `CMakeLists.txt`.

**Wire layout:** header `(protocol, server_tick, n)` then `n × (id, GeoPoint)`, every integer
ZigZag+LEB128, each GeoPoint `(lat, lon, bearing, alt)` per GEO-001. Self-delimiting (LEB128),
so a decoder reads the header then exactly `n` entities and stops.

## 3) Rationale
- **Separate quantized transport, not the hash snapshot.** `Kernel::snapshot()` (raw LE f64)
  stays the world_hash source of truth. The wire snapshot is lossy by quantization and lives
  downstream — never fed back as canonical. The two are kept distinct by construction.
- **Rail-faithful field scope.** GEO-001 defines scales for lat/lon/bearing/alt only. The
  kernel `psi` heading maps to GEO-001 `bearing`. We transmit position + heading + altitude —
  exactly what **remote interpolation** (the ~100 ms buffer) consumes.
- **Cross-impl parity proven** the same way as geo001: C++ encode == reference bytes
  (byte-for-byte) and both decoders recover the snapshot.
- **Units handled once, deterministically.** rad↔deg uses hex-float `RAD2DEG`/`DEG2RAD`; the
  only float ops are the conversion multiply and `value×scale`, identical in C++ and CPython.

## 4) Consequences
- **+** The transport every later netcode layer needs (loopback lockstep, prediction,
  correction, late-join all serialize snapshots).
- **+** New gates: `seads_snapshot_test` (×6 toolchain legs), `snapshot_codec` receipt gate,
  5 new property tests (27 → 32).
- **−** One more generated header to keep in sync (`gen_snapshot_vectors.py --check`).
- **Documented deferral (not a silent cap):** `phi` (bank) and `tas` are **not** on the wire —
  GEO-001 has no scale for them, so transmitting them is a rail reseal. Client-side
  *prediction* that needs bank/energy therefore awaits a later layer (reseal GEO-001 with new
  scales, or an auxiliary non-geographic block). Layer 2 supports interpolation, not yet full
  dead-reckoning.

## 5) Alternatives Considered
- **Put phi/tas on the wire now with ad-hoc scales.** Rejected: invents wire fields outside
  the GEO-001 rail → a reseal. Deferred deliberately and documented instead.
- **Serialize the raw f64 canonical snapshot over the network.** Rejected: that's the hashing
  format, not a quantized transport; wastes bandwidth and conflates the hash source of truth
  with the wire.
- **Normalize lon/heading into (−180,180]/[0,360) before quantizing.** Rejected for the wire:
  transmit state faithfully; normalization is a client presentation concern.

## 6) Acceptance & Probes
All PASS under seal v1.3r0:
- `python tools/spec_monotone_check.py config/rails/atm.json` — rails untouched.
- `python tools/snapshot_ref.py` — reference self-test PASS.
- `python tools/gen_snapshot_vectors.py --check` — header in sync (byte-reproducible).
- `seads_snapshot_test` (GCC + Clang local; full matrix in CI) — byte-exact.
- `python -m pytest tests/property` — 32 passed.
- Golden `GOLDEN-SK-Sphere-001` world_hash — **unchanged** (`529c6a05…9218fe16`).

## 7) Ledger Discipline
Receipt records seal v1.3r0, golden_sha256 (unchanged), commit_sha, toolchain matrix, gates
incl. `geo001_codec` + `snapshot_codec`, signoff Forge.

## 8) Implementation Notes
- `seads_net` still does **not** link `det_math` — the wire layer is off the sim-feeding path.
- Snapshot framing is self-delimiting; `decode_snapshot` rejects a negative entity count and
  any truncated field (propagated from the geo001 LEB128 decoder).
- No seal: layer uses the GEO-001 rail to spec; no kernel/wire-rail/golden edit.
