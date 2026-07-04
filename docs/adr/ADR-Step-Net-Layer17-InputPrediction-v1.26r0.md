# ADR — Netcode layer 17: predictive input client — round-trip prediction/reconciliation against the authoritative input server (`inputclient`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport/client-only, no reseal)

## Context

The netcode arc finally became bidirectional at layer 15b/16: a client sends tick-stamped INPUT-001
`Command`s UP into the authoritative sealed kernel (`broadcast_input`/`broadcast_bidi`), which steps
from the canonically-ordered command SET and streams the resulting protocol-7 snapshots back DOWN.
But the client half of that loop was still a pure READER — it had to wait a full round-trip to see the
result of its own input. Every real action game hides that latency the same way: the client PREDICTS
its own aircraft locally the instant it issues a command, and reconciles against the authoritative
frames when they arrive. SEADS already had the ingredient — the layer-4b `predict::Predictor` (snap to
an authoritative past state, replay buffered inputs to "now") — but it had only ever been driven
against a SCRIPTED server (session/predict). The handoff since layer 15b named this as the first free
pick: *input prediction/reconciliation against the authoritative input server — the client applies its
own commands locally and rewinds/replays on the authoritative frame.*

The determinism stake is the whole arc's: the authoritative kernel's output is a pure function of the
command SET, and every client reconstruction op is det_math (the predictor's kernel), the reproducible
lossy decode, or integer transport. So the round-trip's client side is bit-exactly gateable too.

## Decision

### `run_predictive_client` — the client counterpart to `broadcast_input`

`src/net/inputclient.{h,cpp}` (folded into the existing `seads_netinput` lib) adds
`netpredict::run_predictive_client`: predict the OWN aircraft forward every tick from the LOCAL command
timeline (the same commands the client upstreams; the fire bit is dropped — firing never moves the
shooter and the own ship's hp/ammo are wire-sourced), and reconcile against the authoritative input-
server frames delivered under an integer client `lag` and a downstream loss set. It DRIVES a kernel
copy through the sealed `predict::Predictor` — net code stays OUTSIDE the kernel; decoded bits feed the
RESEED, never the canonical sim. Two reconcile SOURCES, mirroring `predict_ref`:

* **`Source::CANONICAL`** — snap to the authoritative full-precision own state. A correctly-predicting
  client is **SEAMLESS**: predicted == authoritative EVERY tick, the reconcile a zero-correction no-op
  (the client's local sim IS the server's, offset only by latency — the round-trip theorem).
* **`Source::WIRE`** — snap to the DECODED, lossy protocol-7 own state (what a real socket delivers).
  Bounded, not bit-exact: after replay the own ship stays within a few wire quanta of the authoritative
  trajectory (the realistic remote/late-join path). Reproducible (the decode is deterministic) ⇒ its
  per-tick own-ship hash sequence is a cross-impl parity DIGEST, exactly like `session_ref`'s.

Both sources reconcile on the SAME cadence — the ticks a frame actually lands on (emit ticks, delivered
under lag, not dropped) — so `CANONICAL` and `WIRE` differ only in the value snapped to, never in when.
`authoritative_own_states` computes the canonical own(0) trajectory from the own aircraft's own
kinematic phase schedule on a fresh single-aircraft kernel: the own ship's kinematics are independent
of the other aircraft absent a hit, so this is the reference the wire path is judged bounded against
and the canonical path tracks seamlessly. (INPUT-SK-001's own P-47D is never hit, so its lossy wire
bytes are identical whether serialized alone or inside the full 3-ship frame — which is why the Python
reference can judge the socket round-trip without rebuilding the whole authoritative world.)

### INPUT-SK-001 (layer-17 view)

The layer-15b bridge's grid-exact scenario, viewed from the OWN ship (id 0, the P-47D): a DYADIC radian
command timeline (so the INPUT-001 wire round-trips it losslessly and the frames match
`build_server_frames`), snapshots at 20 Hz, ~100 ms client latency (`lag = 10`, a multiple of
`snap_every` so a reconcile always lands on an emit). The three continuous fields are radians as-is
(the input wire carries commanded bank in radians), and the values (0.0, 0.5) are dyadic.

### Honest scope

* The determinism claim is `predict_ref`'s, now closed around the input server: SEAMLESS is proven
  against the CANONICAL state (not the wire — the wire path is bounded, by design). This is the
  authoritative-server guarantee, not "the wire is lossless".
* The bridge drives the whole authoritative sim from one client's upstream (all three aircraft's
  commands) — the layer-15b simplification; a real multi-client binding (each client its own aircraft)
  is layer 15b's still-open positional/unauthenticated binding, out of this cut.
* No client→server input prediction of OTHER aircraft (remotes stay interpolated, layer 4a); this rung
  is about the OWN ship's round trip.

## Verification

* **Bridge `seads_netpredict_test`** (ctest `netpredict_bridge`, native-x64 legs like layers 7–16),
  mirroring `tools/inputpredict_ref.py` bit-for-bit (both digests pinned in both files):
  * **LEG 1 — SEAMLESS round-trip (in-process):** predict own(0) from its own upstreamed timeline,
    reconcile against the CANONICAL authoritative own state each frame ⇒ **in sync every tick**,
    `first_divergent == -1`, `max_pos_err == 0.0`, and the per-tick own-ship hash sequence ==
    `PIN_CANONICAL` (`abecf117…`).
  * **LEG 2 — BOUNDED round-trip over a real 127.0.0.1 socket:** the whole scenario's commands sent UP
    SCRAMBLED (reversed, 1 byte/send) through `broadcast_input`; the downstream frames stream back
    **byte-identical to `build_server_frames`** (the layer-15b invariant), and the client reconciles
    own(0) against the DECODED lossy wire ⇒ `max_pos_err = 4.6e-9 rad` (bounded) and the own-ship hash
    sequence == `PIN_WIRE` (`007c4b9d…`). C++ ≡ Python bit-for-bit on both digests.
  * **LEG 3 — HEAL under a stale-dropped command (in-process):** the client banks locally at tick 30,
    but that command was DROPPED STALE by the server (hold-last), so it mispredicts the 20-tick window
    31..59 and is HEALED exactly at tick 60 — the first authoritative frame whose snapshot clears the
    whole divergent (hold-last) window; a no-reconcile control stays broken forever (`heal == -1`).
* **Property tests +8 ⇒ 224** (`test_inputpredict.py`): seamless in-sync + pinned digest; predicted ==
  truth hashes; digest reproducibility; wire reconcile bounded + pinned + distinct from canonical;
  stale-drop heal at the computed window-clear tick; no-reconcile control never heals; and (Hypothesis)
  ANY finite initial-state desync is snapped back by the first canonical reconcile while the control
  never recovers.
* **Gates:** full **ctest 22→23** GCC + 22→23 Clang (all sealed net bridges still reconstruct
  `966aca05…`); property tests **216→224**; **all 15 goldens byte-identical** (Sphere `6914a994…`,
  C++ ≡ seal on GCC + Clang) — no `src/kernel/**`, `src/det_math/**`, or tuning touched.

**TRANSPORT/CLIENT-ONLY: no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, wire bytes,
protocol-7, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event digests unmoved,
no seal.** Diff: NEW `src/net/inputclient.{h,cpp}`, `src/net/netpredict_test_main.cpp`,
`tools/inputpredict_ref.py`, `tests/property/test_inputpredict.py`; MODIFIED `CMakeLists.txt`
(`inputclient.cpp` into `seads_netinput`, `seads_netpredict_test` target, `netpredict_bridge` ctest).
guardian.yml UNCHANGED (ctest-only bridge, like layers 13–16).

## Alternatives rejected

* **Reuse `session::run_client` for the own ship.** `run_client` reconstructs the FULL dogfight view
  (own predicted + remotes interpolated + weapon/kill state from the wire) against a SCRIPTED server; it
  is not driven by, nor judged against, the authoritative INPUT server, and it bundles remotes/weapons
  this rung does not need. A lean own-ship-only client makes the round-trip theorem the thing under
  test and gives it its own INPUT-SK-001 cross-impl digest.
* **Edit `predict::Predictor` / `predict_ref`.** The sealed layer-4b predictor is exactly right as-is
  (snap + replay); layer 17 only supplies a new authoritative source (the input server) and a new
  scenario. Editing the sealed module for zero behavioral gain would risk its own vectors.
* **Reconcile SEAMLESS against the wire.** The wire is lossy (GEO-001 quantization), so a wire-only
  seamless claim is false; the bit-exact theorem must reconcile against canonical state, and the wire
  path is reported bounded — the same honest split `predict_ref` drew.
* **A brand-new production server.** The authoritative side is `broadcast_input` (layer 15b) verbatim;
  this rung adds only the client. Reusing the sealed server keeps the layer-15b proof intact and makes
  layer 17 a pure client-side closure of the loop.
