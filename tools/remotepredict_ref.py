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

LAYER 25 — RECONCILE SMOOTHING (run_remote_client_smoothed): the coast SNAPS the display to each
reseed; across a maneuver that snap POPS (bank / heading / altitude jump the moment an update lands).
Layer 25 BLENDS the displayed remote a fraction toward each reseed instead — geometric error-decay
smoothing that spreads the correction over ~1/smooth ticks. The COAST (target) is byte-identical to
layer 24; smoothing touches only the rendered 7-tuple. smooth=1 is the exact hard snap (a degenerate
identity: smoothed(1) == the coast). Its honest trade-off: it hides the pop but LAGS the truth during
the transient (bounded, reproducible). Demonstrated on SMOOTH-SK-001 — a harsher bad-network regime
(5 Hz snaps, 200 ms lag, a violent break) where the coast actually drifts and the snap actually jerks.

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

# ---------------------------------------------------------------------------------------
# SMOOTH-SK-001 — the LAYER-25 smoothing demo scenario. REMOTE-SK-001's coast is so good (3e-6 rad)
# that there is nothing to hide; the pop only matters under a REALISTIC bad-network regime — SPARSE
# snapshots (5 Hz), a big lag (200 ms), and a VIOLENT break — where the coast holds the old kinematics
# through a long gap and each reseed pops the display hard. Same Ki-61, harsher conditions: this is
# where hard-snap jerks and smoothing earns its keep. (REMOTE-SK-001 is untouched — its layer-24
# digests stay pinned.)
# ---------------------------------------------------------------------------------------
SMOOTH_SK = {
    "id": "SMOOTH-SK-001",
    "ticks": 200,
    "snap_every": 20,       # 5 Hz snapshots — sparse; the coast drifts a full 20 ticks between reseeds
    "lag_ticks": 20,        # ~200 ms transport latency (a bad link)
    "render_delay": 40,     # layer-4a interp render delay (lag + one 20-tick frame)
    "envelope": "ki61",
    "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 90.0,
              "phi_deg": 0.0, "alt_m": 4000.0, "tas_mps": 220.0},
    "schedule": [
        {"start_tick": 0,   "bank_deg": 0.0,  "g_cmd": 1.0, "throttle": 0.72},  # cruise
        {"start_tick": 60,  "bank_deg": 75.0, "g_cmd": 3.0, "throttle": 1.0},   # VIOLENT break
        {"start_tick": 150, "bank_deg": 0.0,  "g_cmd": 1.0, "throttle": 0.72},  # roll out
    ],
    "steady_window": (40, 55),
}


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


# ---------------------------------------------------------------------------------------
# LAYER 25 — RECONCILE SMOOTHING: hide the maneuver-correction POP.
#
# run_remote_client SNAPS the displayed remote hard to each reseed. Over steady flight the coast is
# nearly exact, so the snap is invisible; but across a maneuver the coast holds the OLD kinematics
# until a fresher snapshot arrives, and the reseed then POPS the display (position + bank + heading)
# to the corrected state — a visible jerk each time an update lands. Layer 25 blends the DISPLAYED
# remote a fraction toward each reseed instead of snapping: geometric error-decay smoothing.
#
# The COAST (the target) is byte-identical to layer 24 — the kernel copy still steps / reseeds
# exactly as before. Smoothing touches ONLY the rendered 7-tuple `disp`, never the kernel (net code
# stays OUTSIDE the kernel, doctrine). Each tick disp moves `smooth * (target - disp)` componentwise
# toward the coast target; the correction is spread over ~1/smooth ticks instead of landing in one.
#
# Determinism: `_blend` is pure IEEE sub / mul / add (no transcendental, no FMA — the C++ mirror
# compiles under -ffp-contract=off), so the smoothed display's per-tick world_hash sequence is a
# cross-impl parity DIGEST, exactly like the coast digests. The corrections it operates on are small
# (milliradians — the coast never drifts more than lag+snap ticks before a reseed), so a straight
# componentwise blend is valid; wrap-around never engages in this near-equator, sub-degree regime.
#
# The honest TRADE-OFF (stated like every layer's bound): smoothing HIDES the pop but LAGS the truth
# during the transient — the displayed remote's "now" error is LARGER than the hard snap's while a
# correction decays, then converges. It buys smoothness with a bounded, reproducible transient lag.
# ---------------------------------------------------------------------------------------

# Smoothing factor in (0, 1]: 1.0 == run_remote_client (hard snap), smaller == smoother + laggier.
SMOOTH_FACTOR = 0.25


def _blend(disp, target, s):
    """Move the displayed 7-tuple `disp` a fraction `s` toward the coast `target`, componentwise.
    s == 1.0 is the exact hard snap (return `target` unchanged — a + 1*(b-a) is NOT bit-exactly b
    in IEEE, so the degenerate case is special-cased to keep smoothed(s=1) == the layer-24 coast).
    Otherwise: d + s*(t - d), pure sub/mul/add (no FMA under -ffp-contract=off ⇒ matches C++)."""
    if s == 1.0:
        return tuple(target)
    return tuple(d + s * (t - d) for d, t in zip(disp, target))


def run_remote_client_smoothed(states, scenario=REMOTE_SK, reconcile=True, source="canonical",
                               drop_emit_ticks=(), smooth=SMOOTH_FACTOR):
    """Layer 25: the layer-24 dead-reckoning coast, but the DISPLAYED remote is BLENDED toward each
    reseed instead of SNAPPED — error-decay smoothing that hides the maneuver-correction pop.

    The coast (`coaster`) is stepped / reseeded byte-identically to run_remote_client; only the
    rendered 7-tuple `disp` differs. Returns dict: per_tick (DISPLAYED remote world_hash), digest,
    max_pos_err (display vs true "now" — the transient lag we trade for smoothness), max_jump (worst
    single-tick displayed-position change — the pop we shrink), delivered."""
    ticks = int(scenario["ticks"])
    lag = int(scenario["lag_ticks"])
    frames = build_frames(states, scenario)
    drops = set(int(d) for d in drop_emit_ticks)

    coaster = _kernel_at(states[0])
    disp = tuple(states[0])            # displayed 7-tuple starts at the known spawn state
    per_tick, max_err, max_jump, delivered = [], 0.0, 0.0, 0
    for t in range(1, ticks + 1):
        st = t - lag
        reseeded = False
        if reconcile and st >= 0 and (st in frames) and (st not in drops):
            base = states[st] if source == "canonical" else _decode_remote7(frames[st])
            if base is not None:
                coaster = _coast_to_now(base, lag)   # extrapolate st -> now = t (target)
                delivered += 1
                reseeded = True
        if not reseeded:
            coaster.step()
        ac = coaster.aircraft[0]
        target = (ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma)
        prev = disp
        disp = _blend(disp, target, smooth)         # render-only blend toward the coast target
        per_tick.append(pred.tick_hash(_kernel_at(disp), t))
        tl = states[t]
        max_err = max(max_err, abs(disp[0] - tl[0]), abs(disp[1] - tl[1]))
        # the visible POP lives in attitude (bank/heading), not position — the coast holds the old
        # bank then the reseed snaps it. Track the worst single-tick change across the full 7-tuple.
        max_jump = max(max_jump, max(abs(d - p) for d, p in zip(disp, prev)))
    return {"per_tick": per_tick, "digest": pred.sequence_digest(per_tick),
            "max_pos_err": max_err, "max_jump": max_jump, "delivered": delivered}


def maneuver_jump(states, scenario=REMOTE_SK, source="canonical", smooth=SMOOTH_FACTOR,
                  drop_emit_ticks=()):
    """The worst single-tick displayed-position JUMP over the MANEUVER window (from the break at the
    2nd schedule phase onward) — the pop metric. Compared at smooth=1 (hard snap) vs smooth<1 to show
    smoothing shrinks the worst correction landed in any one tick."""
    lo = int(scenario["schedule"][1]["start_tick"])
    hi = int(scenario["ticks"])
    ticks = hi
    lag = int(scenario["lag_ticks"])
    frames = build_frames(states, scenario)
    drops = set(int(d) for d in drop_emit_ticks)
    coaster = _kernel_at(states[0])
    disp = tuple(states[0])
    worst = 0.0
    for t in range(1, ticks + 1):
        st = t - lag
        reseeded = False
        if st >= 0 and (st in frames) and (st not in drops):
            base = states[st] if source == "canonical" else _decode_remote7(frames[st])
            if base is not None:
                coaster = _coast_to_now(base, lag)
                reseeded = True
        if not reseeded:
            coaster.step()
        ac = coaster.aircraft[0]
        target = (ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma)
        prev = disp
        disp = _blend(disp, target, smooth)
        if lo <= t <= hi:
            worst = max(worst, max(abs(d - p) for d, p in zip(disp, prev)))
    return worst


# Pinned digests (regenerate by running this file; the C++ bridge asserts the SAME values).
PIN_CANONICAL_DIGEST = "d28979c30e3c694fae0792d697cc7d3be79d9b965e342eb9e52030fefb6ab5e2"
PIN_WIRE_DIGEST = "7468a4edacacddbfd13990646fb734271ca60c3a1c5a7ebf259b2b4d84fb2879"
# Layer 25 — the SMOOTHED display digests over SMOOTH-SK-001 (smooth=SMOOTH_FACTOR), canonical + wire.
PIN_SMOOTH_CANON_DIGEST = "8fa8148481bf91e090c34f827c5c12d12a564c582771cf48c4cf22c8a1075e54"
PIN_SMOOTH_WIRE_DIGEST = "67db5c51a378e956de706a169b501d174ce07d990da3c60e9fd9bc774f0c1af3"


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

    # 5) LAYER 25 — SMOOTHING HIDES THE POP (on SMOOTH-SK-001: sparse 5 Hz snaps, 200 ms lag, a
    #    violent break — where the coast genuinely drifts between reseeds and the hard snap jerks).
    #    The smoothed display's worst single-tick maneuver jump is a fraction of the hard-snap jump;
    #    smoothing is deterministic; and smooth=1 reproduces the hard-snap coast EXACTLY (the
    #    degenerate identity — the coast target is byte-identical, only the render blend differs).
    _sh, sm_states = pred.run_truth(SMOOTH_SK)
    smooth_canon = run_remote_client_smoothed(sm_states, SMOOTH_SK, source="canonical")
    smooth_wire = run_remote_client_smoothed(sm_states, SMOOTH_SK, source="wire")
    smooth_canon_digest = smooth_canon["digest"]
    smooth_wire_digest = smooth_wire["digest"]
    snap_coast = run_remote_client_smoothed(sm_states, SMOOTH_SK, source="canonical", smooth=1.0)
    snap_jump = maneuver_jump(sm_states, SMOOTH_SK, source="canonical", smooth=1.0)
    smooth_jump = maneuver_jump(sm_states, SMOOTH_SK, source="canonical", smooth=SMOOTH_FACTOR)
    if not (smooth_jump < snap_jump):
        print(f"FAIL smoothing did not shrink the pop (snap={snap_jump:.3e} smooth={smooth_jump:.3e})")
        fails += 1
    if run_remote_client_smoothed(sm_states, SMOOTH_SK, source="canonical")["digest"] != smooth_canon_digest:
        print("FAIL smoothed canonical digest not reproducible"); fails += 1
    if run_remote_client_smoothed(sm_states, SMOOTH_SK, source="wire")["digest"] != smooth_wire_digest:
        print("FAIL smoothed wire digest not reproducible"); fails += 1
    # degenerate: smooth=1 hard snap == the layer-24 coast on SMOOTH-SK, bit-for-bit
    coast_ref = run_remote_client(sm_states, SMOOTH_SK, source="canonical")
    if snap_coast["digest"] != coast_ref["digest"]:
        print(f"FAIL smooth=1 not identical to the coast ({snap_coast['digest']} != {coast_ref['digest']})")
        fails += 1
    # honest trade-off: the smoothed display LAGS the truth more than the hard snap during transients
    if not (smooth_canon["max_pos_err"] >= coast_ref["max_pos_err"]):
        print("FAIL smoothing unexpectedly tighter than hard snap (should trade accuracy)"); fails += 1
    if smooth_canon["max_pos_err"] > 1e-1:   # ... but still bounded (harsh scenario, generous margin)
        print(f"FAIL smoothed now-error unbounded: {smooth_canon['max_pos_err']}"); fails += 1

    if check:
        if canon_digest != PIN_CANONICAL_DIGEST:
            print(f"FAIL canonical digest pin: {canon_digest} != {PIN_CANONICAL_DIGEST}"); fails += 1
        if wire_digest != PIN_WIRE_DIGEST:
            print(f"FAIL wire digest pin: {wire_digest} != {PIN_WIRE_DIGEST}"); fails += 1
        if smooth_canon_digest != PIN_SMOOTH_CANON_DIGEST:
            print(f"FAIL smoothed canonical pin: {smooth_canon_digest} != {PIN_SMOOTH_CANON_DIGEST}"); fails += 1
        if smooth_wire_digest != PIN_SMOOTH_WIRE_DIGEST:
            print(f"FAIL smoothed wire pin: {smooth_wire_digest} != {PIN_SMOOTH_WIRE_DIGEST}"); fails += 1

    if fails == 0:
        print(f"RESULT: REMOTE-PREDICT REFERENCE SELFTEST PASS "
              f"({REMOTE_SK['ticks']} ticks, snap/{REMOTE_SK['snap_every']}, lag {REMOTE_SK['lag_ticks']})")
        print(f"  canonical_digest    ={canon_digest}")
        print(f"  wire_digest         ={wire_digest}")
        print(f"  smooth_canon_digest ={smooth_canon_digest}")
        print(f"  smooth_wire_digest  ={smooth_wire_digest}")
        print(f"  coast_now_err={coast_err:.3e}  interp_now_err={interp_err:.3e}  "
              f"(coast {interp_err / coast_err:.1f}x tighter)")
        print(f"  reconcile bounded={bounded:.3e}  no-reconcile drift={drifting:.3e}  "
              f"({drifting / bounded:.1f}x)")
        print(f"  SMOOTH-SK maneuver pop (worst single-tick state jump, alt-dominated): "
              f"snap={snap_jump:.3e}  smooth={smooth_jump:.3e}  "
              f"(smoothing {snap_jump / smooth_jump:.1f}x smaller)  smooth={SMOOTH_FACTOR}")
        print(f"  SMOOTH-SK trade-off: snap now-err={coast_ref['max_pos_err']:.3e}  "
              f"smooth now-err={smooth_canon['max_pos_err']:.3e} (smoother = laggier, still bounded)")
        return 0
    print(f"RESULT: REMOTE-PREDICT REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="assert the pinned digests")
    args = ap.parse_args()
    sys.exit(_selftest(check=args.check))
