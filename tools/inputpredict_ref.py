#!/usr/bin/env python3
"""
inputpredict_ref.py — SEADS predictive INPUT CLIENT REFERENCE (netcode layer 17).

Layers 5-15a are server->client (a client only reads). Layer 15b/16 opened the UPSTREAM path: a
client sends tick-stamped INPUT-001 Commands UP and the authoritative sealed kernel steps from them.
Layer 17 is the missing half — the CLIENT predicts its OWN aircraft LOCALLY from the same commands it
upstreams, and reconciles against the authoritative frames when they come back. This is the layer-4b
Predictor (predict_ref) driven against the layer-15b/16 authoritative input server, closing the loop:

  * SEAMLESS (canonical reconcile): a correctly-predicting client — one whose upstreamed commands the
    server applied in time — reconciles INVISIBLY. Snap to the authoritative full-precision own state,
    replay the buffered inputs since, and the result equals the authoritative trajectory bit-for-bit
    every tick. The reconcile is a zero-correction no-op: the client's local sim IS the server's,
    offset only by latency. This is the round-trip theorem, and its per-tick own-ship hash sequence is
    the cross-impl parity DIGEST the C++ bridge (seads_netpredict_test) must reproduce.

  * BOUNDED (wire reconcile): reconcile against the DECODED, lossy protocol-7 own state (what a real
    socket delivers). Not bit-exact — the reconstructed own ship stays within a few wire quanta of the
    authoritative trajectory after replay (the realistic remote/late-join path predict_ref pioneered).
    Reproducible (the decode is deterministic) ⇒ its own-ship hash sequence is ALSO a parity digest.

  * HEAL under misprediction: a command the client applied LOCALLY but which the authoritative server
    DROPPED as STALE (it arrived after its apply_tick was stepped — the canonical drop, cmdqueue.h)
    makes the client mispredict. It diverges immediately and is HEALED exactly at the first
    authoritative frame whose snap tick clears the bad input; a no-reconcile control stays broken
    forever (the reconcile, not luck, is load-bearing).

INPUT-SK-001 reuses the SESSION-SK-001 airframes + starts, but the OWN ship (id 0, the P-47D) is driven
by a DYADIC (grid-exact) radian command timeline so the INPUT-001 wire round-trips it losslessly (the
same scenario the layer-15b bridge upstreams). The own ship's kinematics are independent of the other
aircraft (it is never hit here), so the authoritative own(0) trajectory is a single-aircraft run, and
its lossy wire bytes are identical whether serialized alone or inside the full 3-ship frame — which is
why this reference can judge the socket round-trip without rebuilding the whole authoritative world.

Boundaries (doctrine, identical to predict_ref/session_ref): net code stays OUTSIDE the kernel; the
client DRIVES a kernel copy; decoded bits feed the reseed, never the canonical sim. No kernel / rails /
wire / golden change — composes the CURRENT protocol-7 wire + sealed layer-4b predictor, riding seal
v1.26r0 (a no-seal integration rung, like session/event were). The C++ mirror src/net/inputclient must
reproduce both digests + the heal behavior bit-for-bit.

Usage:  python tools/inputpredict_ref.py          # self-test (seamless + bounded + heal + control)
        python tools/inputpredict_ref.py --check   # assert the pinned digests (CI/gate)
"""
import argparse
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ref_kernel as rk
import envelopes as envmod
import snapshot_ref as snap
import predict_ref as pred   # reuse the sealed layer-4b Predictor + hashing (drives the sealed kernel)

# ---------------------------------------------------------------------------------------
# INPUT-SK-001 (layer-17 view) — the OWN ship only. The P-47D from SESSION-SK-001, driven by a DYADIC
# radian command timeline (the grid-exact schedule the layer-15b bridge sends upstream, so the wire is
# lossless), snapshots at 20 Hz, and ~100 ms of client-side latency (LAG = 10 ticks) so reconciliation
# genuinely REPLAYS buffered inputs (snap to a past tick, replay forward to now). Command values are
# RADIANS directly (not deg->rad): the sealed input wire carries commanded bank in radians, and these
# values (0.0, 0.5) are dyadic so quantize@1e6 is the identity.
# ---------------------------------------------------------------------------------------
INPUT_SK = {
    "id": "INPUT-SK-001",
    "ticks": 150,           # 1.5 s at 100 Hz
    "snap_every": 5,        # server snapshot cadence: 20 Hz over the 100 Hz sim
    "lag_ticks": 10,        # ~100 ms transport latency (server tick T reaches the client at T+lag)
    "envelope": "p47d",
    "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 90.0,
              "phi_deg": 0.0, "alt_m": 4000.0, "tas_mps": 200.0},
    # (start_tick, target_phi_RAD, target_g, throttle) — mirrors netinput_test_main.cpp IN_P0.
    "schedule": [
        (0,   0.0, 1.0, 0.75),
        (50,  0.5, 1.5, 1.0),
        (120, 0.0, 1.0, 0.75),
    ],
    # a stale-dropped-command misprediction: the client locally banks hard at tick 30, but that command
    # reached the authoritative server too late (apply_tick already stepped) and was DROPPED, so the
    # server holds-last. The client mispredicts from tick 31 until the reconcile clears the bad input.
    "stale_cmd": (30, 0.5, 1.5, 1.0),
}


def _phase_at(schedule, t):
    idx = 0
    for j, ph in enumerate(schedule):
        if int(ph[0]) <= t:
            idx = j
        else:
            break
    return schedule[idx]


def own_command_at(schedule, t):
    """(target_phi_rad, target_g, throttle) at tick t — integer phase select, radians as-is."""
    _st, phi, g, thr = _phase_at(schedule, t)
    return (float(phi), float(g), float(thr))


def run_truth(scenario=INPUT_SK, schedule=None):
    """Authoritative own(0): a single-aircraft P-47D stepped over the command timeline. Returns
    (hashes[1..ticks], states[0..ticks]); states[t] = the canonical 7-tuple AFTER t ticks."""
    sched = schedule if schedule is not None else scenario["schedule"]
    ticks = int(scenario["ticks"])
    env = envmod.load_envelope(scenario["envelope"])
    k = pred._new_kernel(scenario["start"])
    states = [pred.own_state(k.aircraft[0])]
    hashes = []
    for t in range(ticks):
        k.step_scenario([own_command_at(sched, t)], [env])
        states.append(pred.own_state(k.aircraft[0]))
        hashes.append(pred.tick_hash(k, t + 1))
    return hashes, states


def _wire_own_state(state7, server_tick):
    """Round-trip an authoritative own(0) 7-tuple through the protocol-7 wire (quantize->dequantize)
    and return the decoded 7-tuple — exactly the lossy own state a real client reconciles against.
    Own(0)'s bytes are identical whether serialized alone or inside the full frame (the wire quantizes
    each aircraft independently), so this is faithful to the socket round-trip."""
    lat, lon, psi, phi, alt, tas, gamma = state7
    e = snap.from_kernel(0, lat, lon, psi, alt, phi, tas, gamma)
    wire = snap.encode_snapshot(snap.Snapshot(server_tick, [e]))
    dec, _ = snap.decode_snapshot(wire)
    lat_r, lon_r, psi_r, alt_r, phi_r, tas_r, gamma_r = snap.to_kernel(dec.entities[0])
    return (lat_r, lon_r, psi_r, phi_r, alt_r, tas_r, gamma_r)


def run_client(truth_states, scenario=INPUT_SK, local_schedule=None, reconcile=True,
               source="canonical", perturb_alt=0.0):
    """Predict the own ship over `local_schedule` (defaults to the scenario schedule) and reconcile
    against the authoritative frames. `source` selects the reconcile truth:
      'canonical' — snap to truth_states[st] (bit-exact / seamless)
      'wire'      — snap to the DECODED lossy own state (realistic; bounded)
    Returns dict: per_tick (predicted own hashes), digest, in_sync/heal_tick/first_divergent (bit-exact
    vs the canonical truth hashes), max_pos_err (worst |dlat|,|dlon| radians vs canonical truth)."""
    sched = local_schedule if local_schedule is not None else scenario["schedule"]
    ticks = int(scenario["ticks"])
    snap_every = int(scenario["snap_every"])
    lag = int(scenario["lag_ticks"])
    env = envmod.load_envelope(scenario["envelope"])
    p = pred.Predictor(pred._new_kernel(scenario["start"], perturb_alt), env)

    per_tick, first_divergent, heal_tick, in_sync, max_err = [], -1, -1, True, 0.0
    for t in range(1, ticks + 1):
        p.predict(t, own_command_at(sched, t - 1))
        st = t - lag
        if reconcile and st >= 0 and t % snap_every == 0:
            if source == "canonical":
                p.reconcile(st, truth_states[st])
            else:
                p.reconcile(st, _wire_own_state(truth_states[st], st))
        h = pred.tick_hash(p.k, t)
        per_tick.append(h)
        ac = p.k.aircraft[0]
        tl = truth_states[t]
        max_err = max(max_err, abs(ac.lat - tl[0]), abs(ac.lon - tl[1]))
        truth_h = hashlib.sha256(pred._snapshot_from_state(tl, t)).hexdigest()
        if h != truth_h:
            in_sync = False
            if first_divergent < 0:
                first_divergent = t
            heal_tick = -1
        elif heal_tick < 0:
            heal_tick = t
    return {"per_tick": per_tick, "digest": pred.sequence_digest(per_tick),
            "in_sync": in_sync, "heal_tick": heal_tick, "first_divergent": first_divergent,
            "max_pos_err": max_err}


def _stale_schedule(scenario=INPUT_SK):
    """The client's LOCAL schedule with the stale-dropped command spliced in (the server never saw it,
    so the truth run uses the plain schedule)."""
    st, phi, g, thr = scenario["stale_cmd"]
    return sorted(list(scenario["schedule"]) + [(st, phi, g, thr)], key=lambda ph: ph[0])


# When the stale-dropped command heals. Because a command HOLDS until the next phase (hold-last), the
# client's inputs differ from the authoritative truth over the whole window [stale_start, window_end),
# where window_end is the next truth phase start (the client rejoins the truth schedule there). A
# reconcile at snap tick t snaps to server_tick st=t-lag and REPLAYS the client's buffered inputs > st;
# it only heals once st has advanced past the entire divergent window (st >= window_end), i.e. the first
# snap-aligned tick with t-lag >= window_end. (This models a persistent input-level misprediction, not a
# one-tick blip: the client believed it banked for 20 ticks the server never applied.)
def _first_heal_tick(scenario=INPUT_SK):
    snap_every = int(scenario["snap_every"])
    lag = int(scenario["lag_ticks"])
    stale_start = int(scenario["stale_cmd"][0])
    laters = [int(ph[0]) for ph in scenario["schedule"] if int(ph[0]) > stale_start]
    window_end = min(laters) if laters else int(scenario["ticks"])
    need = window_end + lag
    return need if need % snap_every == 0 else ((need // snap_every) + 1) * snap_every


# Pinned digests (regenerate by running this file; the C++ bridge asserts the SAME two values).
PIN_CANONICAL_DIGEST = "abecf117812d1a9037c774c3746c80f92ccfd542ca2d934651672a092cec1b72"
PIN_WIRE_DIGEST = "007c4b9d6cbde8be87a25c6e12d950e6221b13cc3b0c7bc8b4d76bc729645f19"


def _selftest(check=False):
    fails = 0
    truth_hashes, truth_states = run_truth()

    # 1) SEAMLESS: a correctly-predicting client reconciles invisibly against canonical state.
    seamless = run_client(truth_states, reconcile=True, source="canonical")
    if not seamless["in_sync"] or seamless["per_tick"] != truth_hashes:
        print(f"FAIL seamless client not in sync (first divergent {seamless['first_divergent']})")
        fails += 1
    canon_digest = seamless["digest"]
    # determinism: a second run yields the identical digest
    if run_client(truth_states, source="canonical")["digest"] != canon_digest:
        print("FAIL canonical digest not reproducible"); fails += 1

    # 2) BOUNDED: reconcile against the lossy wire — reconstructed own ship within a few quanta.
    wire = run_client(truth_states, reconcile=True, source="wire")
    wire_digest = wire["digest"]
    if wire["max_pos_err"] > 1e-3:   # lat/lon quantum ~1.7e-9 rad; replayed <= lag ticks
        print(f"FAIL wire reconcile error too large: {wire['max_pos_err']}"); fails += 1
    if run_client(truth_states, source="wire")["digest"] != wire_digest:
        print("FAIL wire digest not reproducible"); fails += 1

    # 3) HEAL: a stale-dropped command makes the client mispredict; reconcile heals it at the next
    #    frame clearing the bad input; a no-reconcile control stays broken forever.
    local = _stale_schedule()
    healed = run_client(truth_states, local_schedule=local, reconcile=True, source="canonical")
    fh = _first_heal_tick()
    exp_div = int(INPUT_SK["stale_cmd"][0]) + 1
    if healed["first_divergent"] != exp_div:
        print(f"FAIL stale-drop should diverge at tick {exp_div}, got {healed['first_divergent']}")
        fails += 1
    if healed["heal_tick"] != fh:
        print(f"FAIL stale-drop should heal at tick {fh}, got {healed['heal_tick']}")
        fails += 1
    broken = run_client(truth_states, local_schedule=local, reconcile=False, source="canonical")
    if broken["in_sync"]:
        print("FAIL no-reconcile control stayed in sync (reconcile not load-bearing)"); fails += 1

    if check:
        if canon_digest != PIN_CANONICAL_DIGEST:
            print(f"FAIL canonical digest pin: {canon_digest} != {PIN_CANONICAL_DIGEST}"); fails += 1
        if wire_digest != PIN_WIRE_DIGEST:
            print(f"FAIL wire digest pin: {wire_digest} != {PIN_WIRE_DIGEST}"); fails += 1

    if fails == 0:
        print(f"RESULT: INPUT-PREDICT REFERENCE SELFTEST PASS "
              f"({INPUT_SK['ticks']} ticks, snap/{INPUT_SK['snap_every']}, lag {INPUT_SK['lag_ticks']})")
        print(f"  canonical_digest={canon_digest}")
        print(f"  wire_digest     ={wire_digest}")
        print(f"  heal_tick={fh}  wire_max_pos_err={wire['max_pos_err']:.3e}")
        return 0
    print(f"RESULT: INPUT-PREDICT REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="assert the pinned digests")
    args = ap.parse_args()
    sys.exit(_selftest(check=args.check))
