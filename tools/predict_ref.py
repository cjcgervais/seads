#!/usr/bin/env python3
"""
predict_ref.py — SEADS client-side prediction REFERENCE (netcode layer 4b).

Predict your OWN aircraft forward from local input every tick, and reconcile against
authoritative snapshots: snap to the authoritative state at the snapshot's tick, then REPLAY
the buffered local inputs from that base back up to "now". Because both ends run the sealed
deterministic kernel from the same inputs, a correctly-predicted client reconciles SEAMLESSLY
(snap+replay reproduces the authoritative trajectory bit-for-bit); a client whose state has
drifted (loss / corruption) is HEALED exactly at the next snapshot.

This module is the *single source of truth* for the prediction harness; the C++ mirror
(`src/net/predict`) must reproduce the same per-tick predicted world_hash sequence bit-for-bit,
proven by the generated-vector gate (`tools/gen_predict_vectors.py` -> `src/net/predict_vectors.h`)
exactly as geo001 / snapshot / lockstep are.

What this is / is NOT
---------------------
- The client runs the REAL sealed kernel (ref_kernel) to predict — NOT an approximate
  dead-reckoner. Prediction is therefore bit-exact with the server when inputs match. Same
  doctrine as lockstep: net code DRIVES the kernel, so this lib links the kernel; it stays
  OUTSIDE the kernel boundary (it never edits kernel math / the snapshot byte layout).
- The bit-exact digest gate reconciles against the CANONICAL authoritative state (the raw
  full-precision 6-tuple), NOT the lossy GEO-001/KIN wire — consistent with layer 3 comparing
  the canonical snapshot, never the quantized wire. The lossy-wire reseed path (what a real
  remote / late-join uses) is exercised separately and is bounded by the quantum, not exact —
  see tests/property/test_predict.py. THAT lossy path is *why* phi/tas are on the wire
  (the KIN section, seal v1.4r0): a kernel needs the full state (lat,lon,psi,phi,alt,tas) to
  step, and GEO-001 carries only (lat,lon,psi->bearing,alt).
- NOT a new golden/seal of its own: the prediction scenario is defined inline here (not a
  config/scenarios/*.json), so it never enters the golden machinery. It rides seal v1.4r0
  (the seal that put phi/tas on the wire).

Per-tick predicted world_hash = SHA-256 of `Kernel.snapshot(t)` after the predictor has fully
processed tick t (predict, then reconcile if a snapshot landed this tick). The kernel is exactly
the sealed reference; prediction only DRIVES a client copy of it.

Usage:  python tools/predict_ref.py        # internal self-test (seamless + heal + control)
"""
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ref_kernel as rk
import envelopes as envmod
import snapshot_ref as snap

# ---------------------------------------------------------------------------------------
# The canonical prediction scenario. ONE own aircraft (prediction is about YOUR aircraft),
# a scripted command timeline that turns + climbs + descends, server snapshots at 20 Hz
# (every 5 ticks), and ~100 ms of latency (LAG = 10 ticks = two snapshots) so reconciliation
# genuinely REPLAYS buffered inputs each time (snap to a past tick, replay forward to now).
# Angles are DEGREES here (deg->rad via ref_kernel.deg2rad, the single conversion path).
# ---------------------------------------------------------------------------------------
SCENARIO = {
    "id": "PREDICT-SK-001",
    "ticks": 300,            # 3.0 s at 100 Hz
    "snap_every": 5,         # server snapshot cadence: 20 Hz over the 100 Hz sim
    "lag_ticks": 10,         # ~100 ms transport latency (matches the interp render delay)
    "envelope": "ki61",
    "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 45.0,
              "phi_deg": 0.0, "alt_m": 2000.0, "tas_mps": 150.0},
    "schedule": [
        {"start_tick": 0,   "bank_deg": 0.0,   "climb_mps": 0.0},
        {"start_tick": 60,  "bank_deg": 45.0,  "climb_mps": 5.0},
        {"start_tick": 180, "bank_deg": -30.0, "climb_mps": -4.0},
    ],
    # negative control: a 1-part-in-a-million altitude desync injected into the predictor's
    # INITIAL state (mirrors lockstep's negative control). Reconcile must heal it exactly.
    "perturb_alt": float.fromhex("0x1p-20"),   # ~9.5e-7 m
}


def _new_kernel(start, perturb_alt=0.0):
    k = rk.Kernel({"rails": _RAILS})
    s = start
    k.aircraft.append(rk.Aircraft(
        lat=rk.deg2rad(s["lat_deg"]), lon=rk.deg2rad(s["lon_deg"]),
        psi=rk.deg2rad(s["psi_deg"]), phi=rk.deg2rad(s["phi_deg"]),
        alt=float(s["alt_m"]) + perturb_alt, tas=float(s["tas_mps"])))
    return k


import json
_RAILS = json.loads(
    (Path(__file__).resolve().parent.parent / "config" / "rails" / "atm.json")
    .read_text(encoding="utf-8"))["rails"]


def command_at(schedule, t):
    """(target_phi_rad, target_climb_mps) at tick t. Integer phase select: largest
    start_tick <= t (mirrors ref_kernel.run_scenario / scenario_main.cpp)."""
    idx = 0
    for j, ph in enumerate(schedule):
        if int(ph["start_tick"]) <= t:
            idx = j
        else:
            break
    ph = schedule[idx]
    return (rk.deg2rad(ph["bank_deg"]), float(ph["climb_mps"]))


def state6(ac):
    """The canonical full-precision own-aircraft state a reconcile re-seeds from."""
    return (ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas)


def tick_hash(kernel, tick):
    """Canonical per-tick world_hash: SHA-256 of the raw LE-f64 snapshot after `tick` ticks."""
    return hashlib.sha256(kernel.snapshot(tick)).hexdigest()


def run_truth(scenario=SCENARIO):
    """Server/authoritative kernel: step the own aircraft over the whole command timeline.
    Returns (per_tick_hash[1..ticks], states[0..ticks]) — states[t] is the canonical 6-tuple
    AFTER t ticks (states[0] = initial), the source a reconcile re-seeds from."""
    ticks = int(scenario["ticks"])
    env = envmod.load_envelope(scenario["envelope"])
    k = _new_kernel(scenario["start"])
    states = [state6(k.aircraft[0])]
    hashes = []
    for t in range(ticks):
        cmd = command_at(scenario["schedule"], t)
        k.step_scenario([cmd], [env])
        states.append(state6(k.aircraft[0]))
        hashes.append(tick_hash(k, t + 1))
    return hashes, states


class Predictor:
    """A client predicting its OWN aircraft. Holds a kernel copy + a ring buffer of the local
    (tick, command) inputs it has applied, so a reconcile can snap to an authoritative past
    state and replay the inputs since."""

    def __init__(self, kernel, env):
        self.k = kernel
        self.env = env
        self.buffer = []   # list of (tick, command), oldest first

    def predict(self, tick, cmd):
        """Advance the local kernel one tick from local input, and remember the input."""
        self.k.step_scenario([cmd], [self.env])
        self.buffer.append((tick, cmd))

    def reconcile(self, server_tick, auth6):
        """Authoritative snapshot for `server_tick` arrived: snap the own aircraft to it, drop
        inputs at/older than server_tick, and REPLAY the rest forward to re-derive 'now'."""
        ac = self.k.aircraft[0]
        ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas = auth6
        remaining = [(t, c) for (t, c) in self.buffer if t > server_tick]
        for (_t, c) in remaining:
            self.k.step_scenario([c], [self.env])
        self.buffer = remaining


def run_predictor(truth_states, scenario=SCENARIO, reconcile=True, perturb_alt=0.0):
    """Drive a Predictor over the same timeline as the truth run. Each tick: predict; then, on
    a snapshot tick whose authoritative snapshot has had time to arrive (tick > lag), reconcile
    against truth_states[tick - lag] (the CANONICAL authoritative state — the bit-exact path).

    Returns dict: per_tick (predicted hashes), in_sync (predicted == truth EVERY tick),
    heal_tick (first tick from which predicted == truth onward; -1 if never), first_divergent
    (first tick predicted != truth; -1 if never)."""
    ticks = int(scenario["ticks"])
    snap_every = int(scenario["snap_every"])
    lag = int(scenario["lag_ticks"])
    env = envmod.load_envelope(scenario["envelope"])
    p = Predictor(_new_kernel(scenario["start"], perturb_alt), env)

    # truth hashes to compare against (recompute from states so we don't depend on call order)
    per_tick = []
    first_divergent = -1
    heal_tick = -1
    in_sync = True
    for t in range(1, ticks + 1):
        cmd = command_at(scenario["schedule"], t - 1)
        p.predict(t, cmd)
        if reconcile and t % snap_every == 0 and t > lag:
            server_tick = t - lag
            p.reconcile(server_tick, truth_states[server_tick])
        h = tick_hash(p.k, t)
        per_tick.append(h)
        truth_h = hashlib.sha256(_snapshot_from_state(truth_states[t], t)).hexdigest()
        if h != truth_h:
            in_sync = False
            if first_divergent < 0:
                first_divergent = t
            heal_tick = -1
        else:
            if heal_tick < 0:
                heal_tick = t
    return {"per_tick": per_tick, "in_sync": in_sync,
            "heal_tick": heal_tick, "first_divergent": first_divergent}


def _snapshot_from_state(s6, tick):
    """Rebuild the canonical snapshot bytes for a single-aircraft world in state `s6` after
    `tick` ticks — lets us hash a recorded truth state without re-running the kernel."""
    k = rk.Kernel({"rails": _RAILS})
    k.aircraft.append(rk.Aircraft(*s6))
    return k.snapshot(tick)


def sequence_digest(per_tick):
    """SHA-256 over the concatenated ASCII hex per-tick hashes — one cross-impl equality check
    covering every tick (identical construction to lockstep_ref.sequence_digest)."""
    h = hashlib.sha256()
    for x in per_tick:
        h.update(x.encode("ascii"))
    return h.hexdigest()


def checkpoint_ticks(ticks):
    """Readable spot-check ticks: 1, every 50, and the final tick."""
    pts = [1] + [t for t in range(50, ticks, 50)] + [ticks]
    return sorted(set(p for p in pts if 1 <= p <= ticks))


def first_heal_tick(scenario=SCENARIO):
    """The first snapshot tick at which a perturbed predictor reconciles (and thus heals):
    smallest multiple of snap_every strictly greater than lag_ticks."""
    snap_every = int(scenario["snap_every"])
    lag = int(scenario["lag_ticks"])
    t = ((lag // snap_every) + 1) * snap_every
    return t


# ---- self-test -------------------------------------------------------------------------
def _selftest():
    fails = 0
    truth_hashes, truth_states = run_truth()

    # 1) SEAMLESS: a correctly-predicting client reconciles invisibly — predicted == truth
    #    every tick (snap+replay reproduces the authoritative trajectory bit-for-bit).
    nominal = run_predictor(truth_states, reconcile=True, perturb_alt=0.0)
    if not nominal["in_sync"]:
        print(f"FAIL nominal not in sync (first divergent tick {nominal['first_divergent']})")
        fails += 1
    if nominal["per_tick"] != truth_hashes:
        print("FAIL nominal predicted hashes != truth hashes"); fails += 1
    digest = sequence_digest(nominal["per_tick"])

    # determinism: a second run yields the identical digest
    if sequence_digest(run_predictor(truth_states)["per_tick"]) != digest:
        print("FAIL prediction sequence not reproducible"); fails += 1

    # 2) HEAL: a state-desynced client (perturbed initial alt) diverges, then reconcile snaps
    #    it back EXACTLY at the first snapshot tick > lag and it stays in sync thereafter.
    healed = run_predictor(truth_states, reconcile=True, perturb_alt=SCENARIO["perturb_alt"])
    fh = first_heal_tick()
    if healed["first_divergent"] != 1:
        print(f"FAIL perturbed predictor should diverge at tick 1, got {healed['first_divergent']}")
        fails += 1
    if healed["heal_tick"] != fh:
        print(f"FAIL perturbed predictor should heal at tick {fh}, got {healed['heal_tick']}")
        fails += 1

    # 3) NEGATIVE CONTROL: same perturbation WITHOUT reconcile stays broken forever (proves the
    #    reconcile, not luck, is what heals).
    broken = run_predictor(truth_states, reconcile=False, perturb_alt=SCENARIO["perturb_alt"])
    if broken["in_sync"]:
        print("FAIL no-reconcile control stayed in sync (reconcile not load-bearing)"); fails += 1

    # 4) LOSSY-WIRE reseed is BOUNDED (not exact): reconcile against the dequantized GEO-001+KIN
    #    snapshot of the authoritative state. Error vs truth stays within a small multiple of the
    #    coarsest quantum after replay — this is the realistic remote/late-join path, and it is
    #    why phi/tas are on the wire.
    lossy_err = _lossy_reseed_error(truth_states)
    if lossy_err > 1e-3:   # generous: lat/lon quantum ~1e-7 deg, alt ~1e-3 m, replayed <=lag ticks
        print(f"FAIL lossy-wire reseed error too large: {lossy_err}"); fails += 1

    if fails == 0:
        print(f"RESULT: PREDICTION REFERENCE SELFTEST PASS "
              f"({SCENARIO['ticks']} ticks, snap/{SCENARIO['snap_every']}, lag {SCENARIO['lag_ticks']})")
        print(f"  sequence_digest={digest}")
        print(f"  heal_tick={fh}  lossy_reseed_max_err={lossy_err:.3e}")
        return 0
    print(f"RESULT: PREDICTION REFERENCE SELFTEST FAIL ({fails})")
    return 1


def _lossy_reseed_error(truth_states, scenario=SCENARIO):
    """Reconcile against the LOSSY GEO-001+KIN wire snapshot instead of canonical state, and
    measure the worst-case position error (radians) vs truth at the end. Bounded, not exact."""
    ticks = int(scenario["ticks"])
    snap_every = int(scenario["snap_every"])
    lag = int(scenario["lag_ticks"])
    env = envmod.load_envelope(scenario["envelope"])
    p = Predictor(_new_kernel(scenario["start"], scenario["perturb_alt"]), env)
    worst = 0.0
    for t in range(1, ticks + 1):
        cmd = command_at(scenario["schedule"], t - 1)
        p.predict(t, cmd)
        if t % snap_every == 0 and t > lag:
            st = t - lag
            lat, lon, psi, phi, alt, tas = truth_states[st]
            # round-trip the authoritative state through the wire (quantize -> dequantize)
            e = snap.from_kernel(1, lat, lon, psi, alt, phi, tas)
            wire = snap.encode_snapshot(snap.Snapshot(st, [e]))
            dec, _ = snap.decode_snapshot(wire)
            lat_r, lon_r, psi_r, alt_r, phi_r, tas_r = snap.to_kernel(dec.entities[0])
            p.reconcile(st, (lat_r, lon_r, psi_r, phi_r, alt_r, tas_r))
        ac = p.k.aircraft[0]
        tl = truth_states[t]
        worst = max(worst, abs(ac.lat - tl[0]), abs(ac.lon - tl[1]))
    return worst


if __name__ == "__main__":
    sys.exit(_selftest())
