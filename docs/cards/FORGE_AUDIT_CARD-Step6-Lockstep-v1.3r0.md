# Forge Audit Card — Step 6 layer 3: loopback lockstep harness (ATM-Sphere v1.3r0)

**Project:** SEADS 2026
**Seal:** ATM-Sphere v1.3r0 (rides current seal — kernel/snapshot/replay used to spec, no rail/golden change)
**Realm:** ATM-only
**Scope:** new lockstep desync-tripwire harness (own `seads_lockstep` lib) + cross-impl parity
gate. No kernel, no `det_math`, no rails values, no golden, no change to `seads_net`.

## Gates & Results
- [x] lint_determinism.py — PASS
- [x] spec_monotone_check.py — PASS (rails header v1.3r0, untouched)
- [x] det_math_oracle.py — PASS (unchanged)
- [x] tuning_probe.py — PASS (all 8 envelopes)
- [x] atm_top_probe.py — PASS
- [x] geo001_ref.py + snapshot_ref.py self-tests — PASS (layers 1–2 still green)
- [x] **lockstep_ref.py self-test — PASS (in-sync 600t×3ac + negative control trips at tick 1)**
- [x] gen_lockstep_vectors.py: regenerate → --check — **byte-reproducible** (hex-float + integer)
- [x] gen_*.py --check (all 8 headers incl. lockstep_vectors.h) — PASS
- [x] **seads_lockstep_test (GCC) — PASS: in-sync, digest == reference, tripwire trips**
- [x] **seads_lockstep_test (Clang) — PASS: in-sync, digest == reference, tripwire trips**
- [x] seads_detmath_test + seads_geo001_test + seads_snapshot_test (GCC + Clang) — PASS
- [x] ctest build-gcc + build-clang — **4/4** each (detmath, geo001, snapshot, lockstep)
- [x] validate_snapshot.py GOLDEN-SK-Sphere-001 — world_hash **unchanged** (`529c6a05…9218fe16`)
- [x] validate_scenarios GOLDEN-SK-{Turn,Climb,TurnClimb}-001 — match sealed hashes (GCC)
- [x] pytest tests/property — **36 passed** (32 → 36; +4 lockstep)
- [x] make_receipt.py — overall **PASS** (new `lockstep` gate green)
- [ ] CI guardian.yml cross-toolchain (MSVC + GCC/Clang × x64/AArch64) — PENDING push

## What this layer is
The desync tripwire: two in-process kernels stepped from ONE shared input timeline must produce
an identical per-tick `world_hash` (SHA-256 of `Kernel::snapshot(n)`) every tick. `run(a,b,…)`
stops at the first divergent tick and reports it (the binary-search handle). Foundation for
prediction → remote interpolation → correction / late-join.

## Determinism & Invariants
- Compares the **canonical** hashing snapshot (raw LE f64 — the sealed source of truth), NOT the
  lossy GEO-001 wire. Net code stays **outside** the kernel: the timeline carries sim `Command`s
  (bank/climb), never wire bits. Decoded wire is never fed back to advance the sim.
- Snapshot byte layout **unchanged** — we only hash it per tick (new `lockstep::tick_hash`).
- Cross-impl bit-identity is proven **exhaustively**: a SHA-256 digest over every per-tick hash
  must match the Python reference (+ readable per-tick checkpoints). Vectors are byte-reproducible
  (hex-float scenario, deterministic reference hashes) ⇒ CI `--check` green.
- The tripwire is **demonstrably real**: an asserted negative control (a 2⁻²⁰ m altitude desync)
  trips it — it can never silently pass as a no-op.

## Library boundary (documented deviation from the handoff suggestion)
NEXT_STEPS suggested `lockstep.{h,cpp}` "in `seads_net`". Because the harness DRIVES the kernel
(links `seads_kernel` → `det_math` + `seads_replay`), folding it into `seads_net` would make the
pure wire-codec lib transitively pull det_math + the kernel — eroding a documented invariant
(geo001/snapshot tests depend on `seads_net` staying pure). Resolution: lockstep is its **own**
lib `seads_lockstep`; it still lives under `src/net/`. Only linkage is separated. See ADR §4.

## Affected Files
- tools/lockstep_ref.py (new — canonical reference + self-test + negative control)
- src/net/lockstep.h, src/net/lockstep.cpp (new — `seads_lockstep` lib)
- tools/gen_lockstep_vectors.py, src/net/lockstep_vectors.h (new — generated parity vectors)
- src/net/lockstep_test_main.cpp (new — `seads_lockstep_test`)
- tests/property/test_lockstep.py (new — 4 Hypothesis/lockstep tests)
- CMakeLists.txt (seads_lockstep lib + seads_lockstep_test exe + ctest `lockstep_equal`)
- .github/workflows/guardian.yml (gen --check + reference self-test + lockstep test per leg)
- tools/make_receipt.py (`lockstep` gate)
- docs/adr/ADR-Step6-Lockstep-v1.3r0.md; docs/receipts/receipt-…

## Decision
**APPROVE**

**Sign-off:** Forge · net/tooling · 2026-06-28
