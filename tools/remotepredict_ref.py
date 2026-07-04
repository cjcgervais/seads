#!/usr/bin/env python3
"""
remotepredict_ref.py — SEADS REMOTE-AIRCRAFT PREDICTION REFERENCE (netcode layer 24).

The prediction story so far has two halves. Layer 4b/17 predict the OWN aircraft: the client
replays the LOCAL command timeline it upstreams, so control is instant and — over a lossless
loop — the correction is invisible. Layer 4a renders REMOTE aircraft by INTERPOLATION: it shows
them ~100 ms in the PAST, between the two freshest received snapshots, so motion stays smooth
under 20 Hz updates / jitter / loss. Smooth, but structurally LATE — a remote is always drawn
where it WAS, never where it IS.

Layer 24 closes that gap: predict the REMOTE aircraft too, to "now". The catch is a hard
constraint the own-ship predictor never faced — a client does NOT have a remote's input commands
(authorization means it only ever sees its OWN seat's commands; every bidirectional layer 15b-23
enforces this). All it has of a remote is that remote's KINEMATIC STATE on the wire
(lat/lon/psi/phi/alt/tas/gamma — the GEO-001 + KIN-002 sections, seal v1.4r0/v1.6r0). So the honest
model is DEAD-RECKONING COAST: seed a one-aircraft kernel from the remote's freshest authoritative
snapshot and advance it with the SEALED NO-ARG Kernel.step() — the "pure kinematic tail" that holds
bank / flight-path angle / speed and propagates the great circle (the same tail the Sphere golden
rides). When a fresher authoritative snapshot for that remote arrives (under integer lag + a
downstream loss set), RESEED to it and re-extrapolate forward to "now". No input replay — a remote
has no local inputs; the coast IS the extrapolation.

What this buys, and its honest bound:
  * COAST TRACKS "NOW". Interpolation renders the remote ~(lag + a frame) ticks in the past; the
    coast extrapolates the freshest snapshot forward to the current tick. When the remote flies
    quasi-steadily (established bank, trimmed speed) the coast's position error vs the true "now" is
    a small fraction of interpolation's structural render-lag error — prediction removes the lag.
  * BOUNDED, never bit-exact. Dead-reckoning assumes the last kinematics HOLD, so it is NOT the
    authoritative dynamics (which integrate the remote's real commands: rolling, pulling g,
    accelerating). Between reseeds the coast drifts; each reseed pulls it back. With reconcile the
    now-error stays bounded across a maneuver; a no-reconcile control drifts without bound — the
    reconcile, not luck, is load-bearing (the remote analogue of layer 17's heal).
  * REPRODUCIBLE. Every op is det_math (the no-arg kernel tail) or the deterministic lossy decode,
    so the coasted remote's per-tick world_hash sequence is a cross-impl parity DIGEST the C++
    mirror (src/net/remotepredict) must reproduce bit-for-bit — like session_ref / inputpredict_ref.

Two reconcile SOURCES, mirroring inputpredict_ref:
  * CANONICAL — reseed to the authoritative full-precision remote 7-tuple (lossless downstream).
  * WIRE      — reseed to the DECODED, lossy protocol-7 remote 7-tuple (what a real socket delivers).
Neither is "seamless" (there is no round-trip theorem for a remote — the client never had its
inputs); both are bounded, and each yields its own reproducible parity digest.

REMOTE-SK-001 is a single non-firing aircraft (the remote we watch): a Ki-61 that cruises wings-level
(coast tracks nearly exactly), then breaks into a hard banked turn at tick 90 (the maneuver the coast
mispredicts and the reconcile heals), then rolls out. Its kinematics never depend on another aircraft
(it is never hit), so the authoritative "now" trajectory is a single-aircraft run and its lossy wire
bytes are faithful whether serialized alone or inside a full frame — exactly the property that lets
the C++ LEG 3 ship these frames over a real socket (byte-identical to session::build_server_frames)
and reconstruct the same wire digest.

Boundaries (doctrine, identical to predict_ref/inputpredict_ref): net code stays OUTSIDE the kernel;
the client DRIVES a kernel copy through the public no-arg Kernel.step(); decoded bits feed the reseed,
never the canonical sim. No kernel / det_math / rails / wire / golden change — this composes the
CURRENT protocol-7 wire + the sealed no-arg kinematic tail, riding seal v1.26r0 (a no-seal
integration rung, like session / event / input-prediction were).

Usage:  python tools/remotepredict_ref.py          # self-test (coast tracks, beats interp, heals)
        python tools/remotepredict_ref.py --check   # assert the pinned digests (CI/gate)
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ref_kernel as rk
import envelopes as envmod
import snapshot_ref as snap
import interp_ref as interp     # layer 4a remote interpolation (the baseline the coast beats)
import predict_ref as pred      # reuse tick_hash / sequence_digest / _new_kernel / command_at / run_truth

# ---------------------------------------------------------------------------------------
# REMOTE-SK-001 — ONE non-firing remote aircraft we WATCH (we never drive it; we only receive its
# 20 Hz snapshots and predict it to "now"). A Ki-61: wings-level cruise (coast tracks), a hard
# banked break at tick 90 (the maneuver), roll-out at tick 150. Angles are DEGREES (deg->rad via
# ref_kernel.deg2rad, the single conversion path — shared with predict_ref).
# ---------------------------------------------------------------------------------------
REMOTE_SK = {
    "id": "REMOTE-SK-001",
    "ticks": 200,           # 2.0 s at 100 Hz
    "snap_every": 5,        # server snapshot cadence: 20 Hz over the 100 Hz sim
    "lag_ticks": 10,        # ~100 ms transport latency (server tick T reaches the client at T+lag)
    "render_delay": 15,     # layer-4a interp render delay (ticks in the past): lag + one frame
    "envelope": "ki61",
    "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 90.0,
              "phi_deg": 0.0, "alt_m": 4000.0, "tas_mps": 200.0},
    "schedule": [
        {"start_tick": 0,   "bank_deg": 0.0,  "g_cmd": 1.0, "throttle": 0.72},  # cruise (steady)
        {"start_tick": 90,  "bank_deg": 55.0, "g_cmd": 1.8, "throttle": 1.0},   # hard break (maneuver)
        {"start_tick": 150, "bank_deg": 0.0,  "g_cmd": 1.0, "throttle": 0.72},  # roll out
    ],
    # The steady window used to compare coast-vs-interp "now" error: after the interp buffer has two
    # frames (render_delay filled) and before the maneuver.
    "steady_window": (40, 85),
}

REMOTE_ID = 0  # the (only) aircraft in REMOTE-SK-001


def _emit_ticks(scenario):
    """Server emit ticks: the tick-0 pre-step frame + every snap_every (like build_server_frames)."""
    snap_every = int(scenario["snap_every"])
    ticks = int(scenario["ticks"])
    return [t for t in range(0, ticks + 1) if t % snap_every == 0]


def _frame_bytes(state7, server_tick):
    """Serialize one authoritative remote 7-tuple to a protocol-7 wire frame (a single-aircraft
    world; no projectiles). from_kernel/encode quantize each field independently, so these bytes
    equal the remote's bytes inside session::build_server_frames — the LEG-3 socket invariant."""
    lat, lon, psi, phi, alt, tas, gamma = state7
    e = snap.from_kernel(REMOTE_ID, lat, lon, psi, alt, phi, tas, gamma)
    return snap.encode_snapshot(snap.Snapshot(server_tick, [e]))


def build_frames(states, scenario=REMOTE_SK):
    """The server's delivered frame table {emit_tick: wire_bytes}. states[t] = authoritative 7-tuple
    AFTER t ticks (states[0] = spawn). One frame per emit tick."""
    return {st: _frame_bytes(states[st], st) for st in _emit_ticks(scenario)}


def _decode_remote7(wire):
    """Decode a wire frame and return the remote's lossy 7-tuple (lat,lon,psi,phi,alt,tas,gamma) —
    exactly what a real client reseeds against."""
    dec, _ = snap.decode_snapshot(wire)
    for e in dec.entities:
        if e.id == REMOTE_ID:
            lat, lon, psi, alt, phi, tas, gamma = snap.to_kernel(e)   # note to_kernel order
            return (lat, lon, psi, phi, alt, tas, gamma)
    return None


def _kernel_at(state7):
    """A fresh single-aircraft kernel seeded at `state7` (the reseed base a coast extrapolates from)."""
    k = rk.Kernel({"rails": pred._RAILS})
    k.aircraft.append(rk.Aircraft(*state7))
    return k


def _coast_to_now(base7, steps):
    """Dead-reckon `base7` forward `steps` kinematic ticks with the SEALED no-arg Kernel.step()
    (holds bank/gamma/speed; coordinated-turn great-circle). Returns the extrapolated kernel."""
    k = _kernel_at(base7)
    for _ in range(steps):
        k.step()          # no-arg: the pure kinematic tail (constant phi/gamma/tas)
    return k


def run_remote_client(states, scenario=REMOTE_SK, reconcile=True, source="canonical",
                      drop_emit_ticks=()):
    """Predict the remote to "now" every tick by dead-reckoning the freshest authoritative snapshot.

    Each tick t the client's estimate represents the remote AT tick t (now). When the frame for
    server_tick st = t - lag has been delivered (an emit tick, not in the loss set), reseed to that
    authoritative state and extrapolate forward `lag` kinematic ticks to now; otherwise advance the
    running estimate one more kinematic tick. `source` selects the reseed truth (canonical 7-tuple
    or the decoded lossy wire 7-tuple).

    Returns dict: per_tick (coasted remote world_hash), digest, max_pos_err (worst |dlat|,|dlon| in
    radians vs the authoritative "now"), delivered (frames reseeded from)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    frames = build_frames(states, scenario)
    drops = set(int(d) for d in drop_emit_ticks)

    coaster = _kernel_at(states[0])   # spawn state is known (a client learns it on join/BIND)
    per_tick, max_err, delivered = [], 0.0, 0
    for t in range(1, ticks + 1):
        st = t - lag
        reseeded = False
        if reconcile and st >= 0 and (st in frames) and (st not in drops):
            base = states[st] if source == "canonical" else _decode_remote7(frames[st])
            if base is not None:
                coaster = _coast_to_now(base, lag)   # extrapolate st -> now = t
                delivered += 1
                reseeded = True
        if not reseeded:
            coaster.step()                           # advance the running estimate one tick
        per_tick.append(pred.tick_hash(coaster, t))
        ac = coaster.aircraft[0]
        tl = states[t]
        max_err = max(max_err, abs(ac.lat - tl[0]), abs(ac.lon - tl[1]))
    return {"per_tick": per_tick, "digest": pred.sequence_digest(per_tick),
            "max_pos_err": max_err, "delivered": delivered}


def interp_now_error(states, scenario=REMOTE_SK, drop_emit_ticks=()):
    """The layer-4a INTERPOLATION baseline's position error vs the true "now". A SnapshotBuffer is
    fed each delivered frame; each tick t it is sampled at render_tick = t - render_delay (server_tick
    units), and the interpolated remote position is compared to the authoritative state AT tick t.
    Interpolation renders the PAST, so this error is the structural render-lag the coast removes.
    Measured over the steady window. Returns the worst |dlat|,|dlon| (radians)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    render_delay = int(scenario["render_delay"])
    lo, hi = scenario["steady_window"]
    frames = build_frames(states, scenario)
    drops = set(int(d) for d in drop_emit_ticks)

    buf = interp.SnapshotBuffer()
    worst = 0.0
    for t in range(1, ticks + 1):
        st = t - lag
        if st >= 0 and (st in frames) and (st not in drops):
            dec, _ = snap.decode_snapshot(frames[st])
            buf.add(dec)
        ents = buf.sample(float(t - render_delay))
        tl = states[t]
        for e in ents:
            if e.id == REMOTE_ID and lo <= t <= hi:
                lat_r = e.lat_deg * snap.DEG2RAD
                lon_r = e.lon_deg * snap.DEG2RAD
                worst = max(worst, abs(lat_r - tl[0]), abs(lon_r - tl[1]))
    return worst


def coast_now_error(states, scenario=REMOTE_SK, source="canonical"):
    """The coast's "now" position error measured over the SAME steady window as interp_now_error,
    so the two are directly comparable (coast removes the render lag)."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    lo, hi = scenario["steady_window"]
    frames = build_frames(states, scenario)
    coaster = _kernel_at(states[0])
    worst = 0.0
    for t in range(1, ticks + 1):
        st = t - lag
        if st >= 0 and (st in frames):
            base = states[st] if source == "canonical" else _decode_remote7(frames[st])
            coaster = _coast_to_now(base, lag)
        else:
            coaster.step()
        ac = coaster.aircraft[0]
        tl = states[t]
        if lo <= t <= hi:
            worst = max(worst, abs(ac.lat - tl[0]), abs(ac.lon - tl[1]))
    return worst


# Pinned digests (regenerate by running this file; the C++ bridge asserts the SAME two values).
PIN_CANONICAL_DIGEST = "d28979c30e3c694fae0792d697cc7d3be79d9b965e342eb9e52030fefb6ab5e2"
PIN_WIRE_DIGEST = "7468a4edacacddbfd13990646fb734271ca60c3a1c5a7ebf259b2b4d84fb2879"


def _selftest(check=False):
    fails = 0
    _hashes, states = pred.run_truth(REMOTE_SK)

    # 1) COAST TRACKS "NOW" and BEATS INTERPOLATION over the steady window.
    canon = run_remote_client(states, reconcile=True, source="canonical")
    canon_digest = canon["digest"]
    coast_err = coast_now_error(states, source="canonical")
    interp_err = interp_now_error(states)
    if coast_err >= interp_err:
        print(f"FAIL coast now-error {coast_err:.3e} not < interp now-error {interp_err:.3e}")
        fails += 1
    # determinism: a second run yields the identical digest
    if run_remote_client(states, source="canonical")["digest"] != canon_digest:
        print("FAIL canonical digest not reproducible"); fails += 1

    # 2) BOUNDED over the lossy wire — the coast stays within a small bound of the true now.
    wire = run_remote_client(states, reconcile=True, source="wire")
    wire_digest = wire["digest"]
    if wire["max_pos_err"] > 1e-2:   # dead-reckoning drift over <= lag+snap ticks of maneuver
        print(f"FAIL wire coast error too large: {wire['max_pos_err']}"); fails += 1
    if run_remote_client(states, source="wire")["digest"] != wire_digest:
        print("FAIL wire digest not reproducible"); fails += 1

    # 3) RECONCILE IS LOAD-BEARING: with reconcile the now-error stays bounded across the maneuver;
    #    a no-reconcile control (spawn-seeded coast, never corrected) drifts without bound.
    bounded = run_remote_client(states, reconcile=True, source="canonical")["max_pos_err"]
    drifting = run_remote_client(states, reconcile=False, source="canonical")["max_pos_err"]
    if not (drifting > 10.0 * bounded):
        print(f"FAIL no-reconcile control did not drift (bounded={bounded:.3e} drift={drifting:.3e})")
        fails += 1

    # 4) LOSS SET is deterministic: dropping some emit frames changes the digest reproducibly.
    lossy = run_remote_client(states, reconcile=True, source="wire", drop_emit_ticks=(20, 40, 60))
    if run_remote_client(states, source="wire", drop_emit_ticks=(20, 40, 60))["digest"] != lossy["digest"]:
        print("FAIL lossy-set digest not reproducible"); fails += 1

    if check:
        if canon_digest != PIN_CANONICAL_DIGEST:
            print(f"FAIL canonical digest pin: {canon_digest} != {PIN_CANONICAL_DIGEST}"); fails += 1
        if wire_digest != PIN_WIRE_DIGEST:
            print(f"FAIL wire digest pin: {wire_digest} != {PIN_WIRE_DIGEST}"); fails += 1

    if fails == 0:
        print(f"RESULT: REMOTE-PREDICT REFERENCE SELFTEST PASS "
              f"({REMOTE_SK['ticks']} ticks, snap/{REMOTE_SK['snap_every']}, lag {REMOTE_SK['lag_ticks']})")
        print(f"  canonical_digest={canon_digest}")
        print(f"  wire_digest     ={wire_digest}")
        print(f"  coast_now_err={coast_err:.3e}  interp_now_err={interp_err:.3e}  "
              f"(coast {interp_err / coast_err:.1f}x tighter)")
        print(f"  reconcile bounded={bounded:.3e}  no-reconcile drift={drifting:.3e}  "
              f"({drifting / bounded:.1f}x)")
        return 0
    print(f"RESULT: REMOTE-PREDICT REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="assert the pinned digests")
    args = ap.parse_args()
    sys.exit(_selftest(check=args.check))
