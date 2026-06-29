#!/usr/bin/env python3
"""
lockstep_ref.py — SEADS loopback lockstep REFERENCE (netcode layer 3, the desync tripwire).

Two in-process kernels stepped from ONE shared input timeline must produce an IDENTICAL
per-tick world_hash every tick. That equality is the lockstep invariant a multiplayer server
relies on: if both ends run the sealed kernel from the same inputs, their canonical state stays
bit-for-bit identical and the world_hash diverges the instant anything desyncs.

This module is the *single source of truth* for that harness; the C++ mirror
(`src/net/lockstep`) must reproduce the same per-tick hash sequence bit-for-bit, proven by the
generated-vector gate (`tools/gen_lockstep_vectors.py` -> `src/net/lockstep_vectors.h`) exactly
as geo001/snapshot are.

What this is NOT
----------------
- NOT a new golden / seal. The lockstep scenario is defined inline here (NOT a
  config/scenarios/*.json), so it never enters the golden-seal machinery. It rides seal v1.3r0.
- NOT a use of the GEO-001 wire. The tripwire compares the CANONICAL hashing snapshot
  (`Kernel.snapshot()`, raw LE f64 — the sealed-golden byte layout). The GEO-001 wire snapshot
  is lossy by quantization; decoded wire bits are for remotes/rendering and are NEVER fed back
  to advance the sim. Net code stays strictly OUTSIDE the kernel: the shared timeline carries
  sim Commands (target bank / climb), never wire bytes.

Per-tick world_hash = SHA-256 of `Kernel.snapshot(n)` after n ticks (n = 1..ticks). The kernel
is exactly the sealed reference (ref_kernel.py); lockstep only *drives* two copies of it.

Usage:  python tools/lockstep_ref.py        # internal self-test (in-sync + negative control)
"""
import hashlib
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ref_kernel as rk
import envelopes as envmod

ROOT = Path(__file__).resolve().parent.parent
RAILS = json.loads((ROOT / "config" / "rails" / "atm.json").read_text(encoding="utf-8"))

# ---------------------------------------------------------------------------------------
# The canonical lockstep scenario (the desync-tripwire timeline). Multi-aircraft, multi-phase,
# chosen to exercise the full kinematic surface the netcode must stay in sync across:
#   - coordinated turns (bank), climbs AND descents (clamped to the envelope band)
#   - the ceiling soft-band predamp (AC2 starts at 7950 m climbing hard -> predamp engages)
#   - three different tuning envelopes (different phi_max/roll_rate/climb LUTs)
# Angles are in DEGREES here (deg->rad via ref_kernel.deg2rad, the single conversion path).
# ---------------------------------------------------------------------------------------
SCENARIO = {
    "id": "LOCKSTEP-SK-001",
    "ticks": 600,                       # 6.0 s at 100 Hz — rich trajectory, fast to hash
    "aircraft": [
        {
            "envelope": "ki61",
            "start": {"lat_deg": 0.0, "lon_deg": 0.0, "psi_deg": 45.0,
                      "phi_deg": 0.0, "alt_m": 2000.0, "tas_mps": 140.0},
            "schedule": [
                {"start_tick": 0,   "bank_deg": 0.0,   "climb_mps": 0.0},
                {"start_tick": 150, "bank_deg": 45.0,  "climb_mps": 5.0},
                {"start_tick": 400, "bank_deg": -30.0, "climb_mps": -3.0},
            ],
        },
        {
            "envelope": "bf109f4",
            "start": {"lat_deg": 10.0, "lon_deg": -20.0, "psi_deg": 90.0,
                      "phi_deg": 0.0, "alt_m": 3000.0, "tas_mps": 160.0},
            "schedule": [
                {"start_tick": 0,   "bank_deg": 0.0,  "climb_mps": 8.0},
                {"start_tick": 250, "bank_deg": 60.0, "climb_mps": 0.0},
            ],
        },
        {
            "envelope": "spitfire_mk5",
            "start": {"lat_deg": -15.0, "lon_deg": 30.0, "psi_deg": 270.0,
                      "phi_deg": 0.0, "alt_m": 7950.0, "tas_mps": 150.0},
            "schedule": [
                {"start_tick": 0,   "bank_deg": 0.0,   "climb_mps": 20.0},  # -> ceiling predamp
                {"start_tick": 300, "bank_deg": -40.0, "climb_mps": 0.0},
            ],
        },
    ],
}


def build_kernel(rails=RAILS, scenario=SCENARIO):
    """Build one ref kernel from the scenario, plus its per-aircraft (schedule, envelope)."""
    k = rk.Kernel(rails)
    schedules, envs = [], []
    for ac in scenario["aircraft"]:
        s = ac["start"]
        k.aircraft.append(rk.Aircraft(
            lat=rk.deg2rad(s["lat_deg"]), lon=rk.deg2rad(s["lon_deg"]),
            psi=rk.deg2rad(s["psi_deg"]), phi=rk.deg2rad(s["phi_deg"]),
            alt=float(s["alt_m"]), tas=float(s["tas_mps"])))
        schedules.append(ac["schedule"])
        envs.append(envmod.load_envelope(ac["envelope"]))
    return k, schedules, envs


def commands_at(schedules, t):
    """Per-aircraft (target_phi_rad, target_climb_mps) at tick t. Integer phase select:
    largest start_tick <= t (mirrors ref_kernel.run_scenario / scenario_main.cpp)."""
    cmds = []
    for sched in schedules:
        idx = 0
        for j, ph in enumerate(sched):
            if int(ph["start_tick"]) <= t:
                idx = j
            else:
                break
        ph = sched[idx]
        cmds.append((rk.deg2rad(ph["bank_deg"]), float(ph["climb_mps"])))
    return cmds


def tick_hash(kernel, tick):
    """Canonical per-tick world_hash: SHA-256 of the raw LE-f64 snapshot after `tick` ticks."""
    return hashlib.sha256(kernel.snapshot(tick)).hexdigest()


def run_lockstep(rails=RAILS, scenario=SCENARIO, ticks=None, perturb=None):
    """Drive TWO independently-built kernels from the same input timeline. After each tick,
    hash both canonical snapshots and compare. Stop at the first divergent tick.

    perturb = (aircraft_index, field, delta) injects a desync into kernel B's initial state
    (the negative control: proves the tripwire actually trips). None => the kernels are
    identical and must stay in sync for every tick.

    Returns a dict: in_sync, ticks, divergent_tick (1-based, -1 if none), per_tick (list of
    kernel-A per-tick hashes up to the stop point)."""
    if ticks is None:
        ticks = int(scenario["ticks"])
    a, sched, envs = build_kernel(rails, scenario)
    b, _, _ = build_kernel(rails, scenario)
    if perturb is not None:
        i, field, delta = perturb
        ac = b.aircraft[i]
        setattr(ac, field, getattr(ac, field) + delta)

    per_tick = []
    divergent = -1
    for t in range(ticks):
        cmds = commands_at(sched, t)
        a.step_scenario(cmds, envs)
        b.step_scenario(cmds, envs)
        ha = tick_hash(a, t + 1)
        hb = tick_hash(b, t + 1)
        per_tick.append(ha)
        if ha != hb:
            divergent = t + 1
            break
    return {"in_sync": divergent < 0, "ticks": ticks,
            "divergent_tick": divergent, "per_tick": per_tick}


def sequence_digest(per_tick):
    """One value summarizing the WHOLE per-tick hash sequence: SHA-256 over the concatenated
    ASCII hex hashes. C++ recomputes this identically => a single cross-impl equality check
    covering every tick (not just the final snapshot)."""
    h = hashlib.sha256()
    for x in per_tick:
        h.update(x.encode("ascii"))
    return h.hexdigest()


def checkpoint_ticks(ticks):
    """Human-readable spot-check ticks: 1, every 100, and the final tick. Used for readable
    divergence localization in the C++ test (the digest is the exhaustive check)."""
    pts = [1]
    pts += [t for t in range(100, ticks, 100)]
    pts.append(ticks)
    # unique + sorted, all within [1, ticks]
    return sorted(set(p for p in pts if 1 <= p <= ticks))


def _selftest():
    fails = 0

    # in-sync: identical kernels agree on every tick
    r = run_lockstep()
    if not r["in_sync"]:
        print(f"FAIL: kernels diverged at tick {r['divergent_tick']}"); fails += 1
    if len(r["per_tick"]) != int(SCENARIO["ticks"]):
        print(f"FAIL: expected {SCENARIO['ticks']} hashes, got {len(r['per_tick'])}"); fails += 1
    digest = sequence_digest(r["per_tick"])

    # determinism: a second run yields the identical sequence digest
    r2 = run_lockstep()
    if sequence_digest(r2["per_tick"]) != digest:
        print("FAIL: lockstep sequence not reproducible across runs"); fails += 1

    # negative control: a 1-part-in-a-million altitude desync MUST trip the tripwire
    rp = run_lockstep(perturb=(2, "alt", float.fromhex("0x1p-20")))
    if rp["in_sync"]:
        print("FAIL: negative control did not trip (perturbed kernel stayed 'in sync')"); fails += 1
    elif rp["divergent_tick"] != 1:
        print(f"FAIL: expected divergence at tick 1, got {rp['divergent_tick']}"); fails += 1

    if fails == 0:
        print(f"RESULT: LOCKSTEP REFERENCE SELFTEST PASS "
              f"({SCENARIO['ticks']} ticks, {len(SCENARIO['aircraft'])} aircraft)")
        print(f"  sequence_digest={digest}")
        return 0
    print(f"RESULT: LOCKSTEP REFERENCE SELFTEST FAIL ({fails})")
    return 1


if __name__ == "__main__":
    sys.exit(_selftest())
