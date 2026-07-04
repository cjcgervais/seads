# ADR — Netcode layer 25: remote-prediction reconcile smoothing — hide the maneuver pop (`netremote::run_remote_client_smoothed`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-04 · **Seal:** rides **v1.26r0** (transport/client-only, no reseal)

## Context

Layer 24 predicts a remote to "now" by dead-reckoning coast: seed a one-aircraft kernel from the remote's
freshest authoritative snapshot, advance it with the sealed no-arg `Kernel::step()`, and **reseed** (snap)
to each fresher snapshot. Over steady flight the coast is nearly exact, so the snap is invisible. But
across a maneuver the coast holds the OLD kinematics (bank / flight-path angle / speed) until a fresher
snapshot arrives, then the reseed **pops** the displayed remote to the corrected state — a visible jerk
every time an update lands. The worse the link (sparse snapshots, high latency) and the more violent the
maneuver, the bigger the pop. Layer 24's own honest-scope note named this: the coast is *bounded, not
seamless*, so the correction is real and, on a bad link, visible.

## Decision

### Geometric error-decay smoothing on the rendered 7-tuple

`src/net/remotepredict.{h,cpp}` adds `netremote::run_remote_client_smoothed` (bit-for-bit mirror of
`tools/remotepredict_ref.run_remote_client_smoothed`). The **coast** (the target) is stepped / reseeded
**byte-identically to layer 24** — the kernel copy is untouched. What changes is presentation: instead of
displaying the coast target directly, the client keeps a separate rendered 7-tuple `disp` and each tick
blends it a fraction `smooth ∈ (0,1]` toward the target, componentwise:

```
disp = disp + smooth * (target − disp)        // pure IEEE sub / mul / add
```

so a reseed's correction is spread over ~`1/smooth` ticks instead of landing in a single tick. `smooth = 1`
is the exact hard snap (special-cased to `disp = target`, since `a + 1·(b−a)` is not bit-exactly `b` in
IEEE) — a **degenerate identity**: `smoothed(1)` hashes the same coast state every tick, so its digest
equals the layer-24 coast digest bit-for-bit. Smaller `smooth` = smoother + laggier.

The blend operates on small corrections (the coast never drifts more than `lag + snap` ticks before a
reseed; the pop is milliradians of attitude / a few metres of altitude), so a straight componentwise blend
is valid — angular wrap never engages in this near-equator, sub-degree regime. Two reconcile **sources**
carry over from layer 24 (CANONICAL full-precision reseed, WIRE decoded-lossy reseed), each yielding its
own reproducible smoothed digest.

### Honest trade-off — smoothness bought with transient lag

Smoothing **hides the pop but lags the truth** during the transient: while a correction decays, the
displayed remote trails the true "now" by more than the hard snap did, then converges. This is the
deliberate accuracy↔smoothness dial, and it is stated as the layer's bound (like every layer's honest
scope). It remains **bounded** (reseeds keep pulling `disp` toward truth) and **reproducible** (the blend
is pure IEEE arithmetic — no transcendental, no FMA under `-ffp-contract=off`, so the displayed remote's
per-tick world_hash sequence is a cross-impl parity DIGEST, exactly like the coast digests).

### Boundaries (doctrine, identical to layer 24)

Net code stays OUTSIDE the kernel. The client drives a kernel copy through the public no-arg
`Kernel::step()`; smoothing touches ONLY the rendered 7-tuple, never the kernel or the reseed. No kernel /
det_math / rails / wire / golden change — composes the EXISTING protocol-7 wire + the sealed no-arg
kinematic tail, riding seal **v1.26r0** (a no-seal integration rung). **ZERO new det_math** (the blend is
IEEE ±×÷, not a transcendental) — the 16th-consecutive zero-transcendental posture holds.

## Verification

**SMOOTH-SK-001** — the same Ki-61 as REMOTE-SK-001 under a harsher **bad-network** regime where the coast
actually drifts and the snap actually jerks: SPARSE 5 Hz snapshots (snap/20), a 200 ms lag (lag 20), and a
VIOLENT break (bank 75°, g 3.0) at tick 60; 200 ticks. (REMOTE-SK-001 is untouched — its layer-24 digests
stay pinned.) The dominant pop here is **altitude** — the violent pull changes flight-path angle and the
coast's altitude drifts across the sparse gaps.

- **Python reference** `tools/remotepredict_ref.py` (`--check` now asserts four pinned digests — the two
  layer-24 coast digests plus):
  - smoothed canonical `8fa8148481bf91e090c34f827c5c12d12a564c582771cf48c4cf22c8a1075e54`
  - smoothed wire      `67db5c51a378e956de706a169b501d174ce07d990da3c60e9fd9bc774f0c1af3`
- **BRIDGE `seads_netremotepredict_test`** gains **LEG 4** (in-process on SMOOTH-SK-001; the ctest target
  and count are UNCHANGED — a new leg, not a new target, `netremotepredict_bridge` stays test 30/30):
  - smoothing shrinks the worst single-tick state jump **4.0×** (2.61e+00 → 6.55e-01 over the break) with
    `smooth = 0.25` — exactly `1/smooth`, the geometric-decay result;
  - the degenerate identity — `smooth = 1` reproduces the layer-24 coast digest bit-for-bit;
  - the smoothed canonical + wire digests == the pinned reference values, reproducibly;
  - the honest trade-off — the smoothed now-error (4.36e-4 rad) is ≥ the hard snap's, but bounded.
- **Gates:** full ctest **30/30** GCC + Clang; **all 15 goldens byte-identical** (Sphere `6914a994…`);
  property tests **268→274** (+6 `test_remotepredict.py`: pop shrinks, `smooth=1` == the coast, smoothed
  digests pinned + reproducible, accuracy-for-smoothness trade, monotone in the factor, Hypothesis
  factor×loss sweep stays bounded + reproducible); determinism lint + rails monotone + det_math oracle PASS.

**TRANSPORT / CLIENT-ONLY:** no `src/kernel/**`, `src/det_math/**`, `config/rails/**`, snapshot wire bytes,
protocol-7, session/event codec, or tuning touched ⇒ all 15 goldens byte-identical, sealed session/event
digests unmoved. **No seal.**

## Consequences

- The remote-prediction display is now tunable end-to-end: interpolation (layer 4a, smooth-but-late) →
  dead-reckoning coast (layer 24, tracks-now-but-pops) → smoothed coast (layer 25, tracks-now-without-the-pop
  at the cost of a bounded transient lag). The `smooth` factor dials the accuracy↔smoothness point.
- **Files:** NEW `docs/adr/ADR-Step-Net-Layer25-RemoteSmoothing-v1.26r0.md`; MODIFIED
  `src/net/remotepredict.{h,cpp}` (`SmoothResult` + `run_remote_client_smoothed` + `blend`/`coaster_state`
  helpers), `src/net/netremotepredict_test_main.cpp` (SMOOTH-SK-001 + LEG 4 + smoothed pins),
  `tools/remotepredict_ref.py` (SMOOTH-SK-001 + `run_remote_client_smoothed` + `_blend`/`maneuver_jump` +
  smoothed pins), `tests/property/test_remotepredict.py` (+6). No `CMakeLists.txt` change (no new target),
  no shared-file/`Stats` change. `guardian.yml` UNCHANGED (ctest-only bridge, like layers 13–24).
- **Next (free pick, none blocking):** **stronger credentials** (a real MAC/signature the server verifies
  vs the abstracted i64 token); or **renderer polish** (surface the assigned seat / auth state / the
  predicted-vs-interpolated-vs-smoothed remote / the correction magnitude on the HUD).
