# ADR — Renderer: remote prediction + smoothing on the viewer HUD (`seads_coaster`, no-seal, rides ATM-Sphere v1.26r0)

**Status:** accepted · **Date:** 2026-07-05 · **Seal:** rides **v1.26r0** (presentation/downstream-only, no reseal)

## Context

The globe viewer (`src/client/viewer_main.cpp`) renders remote aircraft by **layer-4a interpolation**: it
samples the decoded 20 Hz wire ~100 ms in the PAST between the two freshest received frames. Smooth under
jitter/loss, but structurally LATE — a remote is always drawn where it *was*.

Netcode **layer 24** (`netremote::run_remote_client`) closed that gap for the network client by
DEAD-RECKONING a remote to "now": seed a one-aircraft kernel from the freshest authoritative snapshot and
advance it with the SEALED no-arg `Kernel::step()` (the pure kinematic tail). **Layer 25**
(`run_remote_client_smoothed`) then BLENDS the drawn remote toward each reseed instead of snapping, hiding
the maneuver pop. Both were proven only in the socket/parity test harness — never drawn on screen. The
v1.26r0 handoff named "surface the predicted-vs-interpolated-vs-smoothed remote / correction magnitude on
the HUD" as a free-pick renderer follow-up. This ADR records that follow-up.

## Decision

Bring layers 24/25 onto the viewer as a live, toggleable **remote render mode**, backed by a headless-
testable presentation-side coaster.

1. **`seads_coaster` lib (`src/client/remote_coaster.{h,cpp}`).** A faithful presentation mirror of the
   layer-24/25 core: `coast_to_now(rails, base7, steps)` seeds a one-aircraft `Kernel` and dead-reckons the
   no-arg tail (== `netremote::coast_to_now`); `RemoteCoasterSet::update(id, target, smooth)` holds each
   aircraft's DISPLAYED (blended) tuple and nudges it `disp += smooth·(target − disp)` (== `netremote::blend`;
   `smooth ≥ 1` hard-snaps, first sighting seeds). It is its own tiny lib because, unlike the deliberately
   kernel-free `seads_client`, it DRIVES a kernel copy — so it links `seads_kernel`, while staying outside
   the world_hash (a render-only coast, never fed back to the sim).

2. **`Playback::nearest_state(render_tick, id, EntityState&, server_tick&)`.** One small public accessor
   returning the authoritative (NON-interpolated) nearest-frame 7-tuple + its server_tick — the coast must
   start from a real received snapshot, not the interpolated in-between of `Playback::sample`.

3. **Viewer wiring (both replay `run_gui` and fly `run_fly`).** `M` cycles `RemoteMode`
   INTERP → PREDICT → SMOOTH (default INTERP = original behaviour). `remote_draw()` builds each remote's
   display state under the mode — INTERP passes the layer-4a sample through; PREDICT/SMOOTH seed the coast
   from the freshest snapshot at/behind `now − lag` and dead-reckon forward to now (SMOOTH then blends).
   `draw_remote_mode_hud` shows the active mode + the worst coast-vs-interp "now" error live. Trails, damage
   bars, and the combat-feed's `screen_of` all follow the DRAWN (coasted) position so the overlay stays
   pinned to the aircraft.

## Consequences

- **Downstream-only ⇒ no seal.** Diff is `src/client/*` + CMake additions only — no `src/kernel/**`,
  `src/det_math/**`, `config/rails/**`, wire, protocol-7, or tuning. All 15 goldens byte-identical
  (Sphere `6914a994…`); `guardian.yml` unchanged.
- **Headless proof** (`test_remote_coaster` in `seads_client_test`, ctest `client_presentation`): (a) the
  coast is COMPOSABLE — `coast(a+b) == coast(a) then coast(b)` bit-for-bit — i.e. it is exactly the pure
  no-arg kernel tail; (b) the layer-25 blend seeds on first sight, lands `disp + s·(target − disp)`, and
  hard-snaps at `s ≥ 1`; (c) on a steady coordinated turn driven through the REAL kernel and recorded at
  20 Hz, the WIRE coast tracks "now" far tighter than the interpolation baseline. `--selfcheck` echoes
  per-tick interp-err vs coast-err on any recording.
- **Honest bound (layer 24's, surfaced not hidden).** The coast assumes the last kinematics HOLD, so it
  wins big on quasi-steady flight (the client-test turn is ~exact) but is only BOUNDED through hard
  maneuvers — on `demo_dogfight` (continuous hard turns) coast ≈ interp (~21 m vs ~19–21 m). The HUD shows
  both numbers live, so the viewer never over-promises: it lets the operator see when prediction helps.
- **Gotcha for future edits:** raylib `#defines DEG2RAD/RAD2DEG/PI` as macros, so in `viewer_main.cpp` the
  coast seed uses the viewer's local `DEG2RAD_V`, never `netsnap::DEG2RAD` (macro-clobber → compile error).
  `RemoteMode` defaults to INTERP so the change is inert unless the operator presses `M`.

## Alternatives considered

- **Reuse `netremote::run_remote_client` directly.** Its batch API (precomputed frames + 100 Hz states →
  a digest) does not fit a per-frame, looping render loop that only holds 20 Hz snapshots. The presentation
  coaster re-expresses the same core per-frame instead.
- **Apply to fly mode only.** Rejected — replay (`run_gui`) draws every aircraft as an interpolated remote,
  so the shared `remote_draw` helper covers both paths for free.
