# ADR — Netcode layer 24: remote-aircraft prediction — dead-reckoning coast (`netremote::run_remote_client`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport/client-only, no reseal)

## Context

The prediction story had two halves that never met in the middle:

- **Own-ship prediction (layer 4b / 17)** predicts the LOCAL player's aircraft by replaying the very
  command timeline the client upstreams. Over a lossless loop the correction is invisible (the round-trip
  theorem) — control is instant because the client re-runs the sealed kernel from inputs it *has*.
- **Remote rendering (layer 4a)** shows the OTHER aircraft by INTERPOLATION: it holds them ~100 ms in the
  PAST, sampling between the two freshest received 20 Hz snapshots so motion stays smooth under jitter/loss.
  Smooth — but structurally LATE. A remote is always drawn where it *was*, never where it *is*.

The authenticated bidirectional arc (15b→23) is feature-complete, and its honest-scope notes repeatedly
named remote prediction as an open direction. The hard constraint that kept it open is exactly what the
own-ship predictor never faced: **a client does not have a remote's input commands.** Every bidirectional
layer 15b–23 enforces upstream authorization — a client only ever sees (and may only drive) its OWN seat's
commands. So the layer-17 trick (replay the local command timeline) is unavailable for a remote: there is
no local timeline to replay. All the client has of a remote is that remote's KINEMATIC STATE on the
protocol-7 wire (lat/lon/psi/phi/alt/tas/gamma — the GEO-001 + KIN-002 sections, seals v1.4r0 / v1.6r0).

## Decision

### Dead-reckoning coast on the sealed no-arg kinematic tail

`src/net/remotepredict.{h,cpp}` adds `netremote::run_remote_client` (bit-for-bit mirror of
`tools/remotepredict_ref.py`). The model is **dead-reckoning coast**: seed a one-aircraft kernel from the
remote's freshest authoritative snapshot and advance it each tick with the **sealed no-arg
`Kernel::step()`** — the *"pure kinematic tail, phi/gamma unchanged"* that holds bank / flight-path angle /
speed and propagates the coordinated-turn great circle (the same tail the Sphere golden rides). When a
fresher authoritative snapshot for that remote arrives (under an integer `lag` + a downstream loss set),
**reseed** to it and re-extrapolate forward `lag` kinematic ticks to "now". There is **no input replay** —
a remote has no local inputs; the coast *is* the extrapolation.

Concretely, each client tick `t`: if the frame for `server_tick st = t − lag` is delivered (an emit tick
present in `frames`, not in the loss set), rebuild the kernel at the authoritative state for `st` and step
it `lag` times (extrapolate `st → now`); otherwise advance the running estimate one more no-arg tick. So
the displayed remote always represents the CURRENT tick — the freshest snapshot extrapolated to now —
rather than interpolation's fixed render-delay in the past.

Two reconcile **sources**, mirroring `inputpredict`:
- **CANONICAL** — reseed to the authoritative full-precision 7-tuple (lossless downstream).
- **WIRE** — reseed to the DECODED, lossy protocol-7 remote 7-tuple (what a real socket delivers).

### Honest scope — bounded, never seamless

Unlike own-ship prediction there is **no round-trip theorem** for a remote: the client never had the
remote's inputs, so even a CANONICAL reseed does not make the coast equal the authoritative dynamics (which
integrate the remote's real commands — rolling, pulling g, accelerating). The coast assumes the last
kinematics HOLD. Therefore:

- **It TRACKS "now" far tighter than interpolation** when the remote flies quasi-steadily. Over the steady
  cruise window of REMOTE-SK-001 the coast's position error vs the true "now" is **1862× smaller** than the
  layer-4a interpolation baseline's structural render-lag error (1.07e-6 vs 1.99e-3 rad). Prediction
  removes the lag interpolation is stuck with.
- **It stays BOUNDED across a maneuver BECAUSE of the reconcile.** At the hard banked break (tick 90) the
  coast — holding the old wings-level attitude — mispredicts, and each reseed pulls it back; with reconcile
  the worst now-error is 3.05e-6 rad, while a no-reconcile control (spawn-seeded coast, never corrected)
  drifts **81× further** (2.47e-4 rad). The reconcile, not luck, keeps the remote tracked — the remote
  analogue of layer 17's heal.
- **It is REPRODUCIBLE.** Every op is det_math (the no-arg kinematic tail) or the deterministic lossy
  decode, so the coasted remote's per-tick world_hash sequence is a cross-impl parity DIGEST.

### Boundaries (doctrine, identical to predict / inputclient)

Net code stays OUTSIDE the kernel. The client DRIVES a kernel copy through the public no-arg
`Kernel::step()`; decoded wire bits feed the RESEED, never the canonical sim. No kernel / det_math / rails /
wire / golden change — this composes the EXISTING protocol-7 wire + the sealed no-arg kinematic tail,
riding seal **v1.26r0** (a no-seal integration rung, like session / event / input-prediction were). ZERO
new det_math (16th consecutive zero-transcendental integration rung).

## Verification

**REMOTE-SK-001** — one non-firing Ki-61 we WATCH (never drive): wings-level cruise (coast tracks),
a hard banked break at tick 90 (the maneuver), roll-out at tick 150; 200 ticks, snap/5, lag 10,
render_delay 15. Its kinematics never depend on another aircraft, so the authoritative "now" trajectory is
a single-aircraft run and its lossy wire bytes are faithful whether serialized alone or inside a full frame
— the property that lets LEG 3 ship these frames over a real socket byte-identical to
`session::build_server_frames`.

- **Python reference** `tools/remotepredict_ref.py` (`--check` asserts two pinned digests):
  - canonical `d28979c30e3c694fae0792d697cc7d3be79d9b965e342eb9e52030fefb6ab5e2`
  - wire      `7468a4edacacddbfd13990646fb734271ca60c3a1c5a7ebf259b2b4d84fb2879`
- **BRIDGE `seads_netremotepredict_test`** (ctest **29→30** `netremotepredict_bridge`, native-x64 gcc+clang):
  - **LEG 1** — coast tracks NOW 1862.3× tighter than interpolation over the steady window; canonical
    digest == pin, reproducible.
  - **LEG 2** — reconcile keeps the coast bounded (3.05e-6 rad) across the tick-90 break; no-reconcile
    control drifts 81.2×.
  - **LEG 3** — the remote's commands upstreamed SCRAMBLED (reversed / 1 B per send) through
    `broadcast_input` → 41 frames byte-identical to `build_server_frames` → the observing coast reseeds
    against the decoded lossy wire within 3.05e-6 rad; wire digest == pin. (Upstreaming here merely DRIVES
    the authoritative server on a real socket; the remote-prediction logic under test is strictly downstream.)
- **Gates:** full ctest **30/30** GCC + Clang; **all 15 goldens byte-identical** (Sphere `6914a994…`);
  property tests **260→268** (+8 `test_remotepredict.py`: coast beats interpolation, canonical/wire digests
  pinned + reproducible, reconcile load-bearing, deterministic loss set, Hypothesis loss-set sweep stays
  bounded + reproducible); determinism lint + rails monotone + det_math oracle PASS.

**TRANSPORT / CLIENT-ONLY:** no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, snapshot wire bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved. **No seal.**

## Consequences

- The prediction story is now symmetric: the own ship is predicted from local inputs (layer 17), and every
  remote is predicted to "now" by dead-reckoning coast (layer 24) — interpolation (layer 4a) remains as the
  baseline it improves on (and a smoothing fallback for a fully-steady display).
- **Files:** NEW `src/net/remotepredict.{h,cpp}`, `src/net/netremotepredict_test_main.cpp`,
  `tools/remotepredict_ref.py`, `tests/property/test_remotepredict.py`, this ADR; MODIFIED `CMakeLists.txt`
  (`remotepredict.cpp` into `seads_netinput`; `seads_netremotepredict_test` target; `netremotepredict_bridge`
  ctest). No shared-file/`Stats` change. `guardian.yml` UNCHANGED (ctest-only bridge, like layers 13–23).
- **Next (free pick, none blocking):** predict-others reconciliation SMOOTHING (blend the coast toward each
  reseed instead of snapping, to hide the maneuver correction — a pure presentation choice on top of this);
  **stronger credentials** (a real MAC/signature vs the abstracted i64 token); or **renderer polish** (draw
  the predicted-vs-interpolated remote / the correction magnitude / the assigned seat on the HUD).
