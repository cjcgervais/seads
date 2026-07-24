#!/usr/bin/env python3
"""replay_diff.py -- feel-drift diff for the SEADS / EvC flight-feel harness.

Dependency-free (Python 3 stdlib only). Handles BOTH capture sources on one
scale:

  * SEADS C++ CSV   -- the harness telemetry (telemetry.h). Two schemas are
                       auto-detected by header:
                         raw   : t,px,py,pz,vx,vy,vz,speed,altitude,qw..wz,
                                 throttle,alpha,beta        (fly / step / lathold)
                         ctrl  : ...e,wx..wdz,in_pitch..,alpha,aoa_filtered,beta,
                                 phi,cos_phi_theta,regime,push,ballistic,...,
                                 load_factor,n_proxy         (ctrl_fly / replay)
  * Roblox EvC JSONL -- FlightRecorder batches (one session line + frame lines).

Column NAMES/ORDER for CSV output match the SEADS probes exactly, so recorded
live flights and synthetic probes read on one scale.

Metrics align with docs/v5_kernel_handoff.md grids where the columns exist:
  - nose-vs-aim pointing error   -> `e` (ctrl CSV) [the headline aim error]
  - bank                         -> `phi`
  - speed / energy curve         -> `speed`, derived specific energy
  - push_gate commitment timing  -> first/edge transitions of `push`
  - lathold-style summary        -> t_capture, min_nose_elev, alt_loss,
                                    alpha_p95, windmill (when the columns exist)

Usage:
  python replay_diff.py diff GOLDEN CANDIDATE [--json OUT.json] [--align tick|time]
  python replay_diff.py summary CAPTURE
  python replay_diff.py promote CAPTURE NAME [--notes "feel verdict"]
                                             [--goldens DIR]
"""

import argparse
import json
import math
import os
import shutil
import sys
from datetime import datetime, timezone

# --------------------------------------------------------------------------- #
# Loading
# --------------------------------------------------------------------------- #


class Capture:
    """A normalized capture: list of per-tick/-frame dict rows + metadata."""

    def __init__(self, path):
        self.path = path
        self.source = None      # 'seads-raw' | 'seads-ctrl' | 'evc-jsonl'
        self.meta = {}
        self.rows = []          # list of dict, keys are the native column names
        self._load()

    def _load(self):
        ext = os.path.splitext(self.path)[1].lower()
        if ext in (".jsonl", ".ndjson"):
            self._load_jsonl()
        else:
            self._load_csv()

    def _load_csv(self):
        with open(self.path, "r", encoding="utf-8-sig") as fh:
            header = fh.readline().strip()
            cols = header.split(",")
            self.cols = cols
            # ctrl schema is discriminated by the 'e' + 'push' columns.
            if "push" in cols and "cos_phi_theta" in cols:
                self.source = "seads-ctrl"
            else:
                self.source = "seads-raw"
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                vals = line.split(",")
                row = {}
                for c, v in zip(cols, vals):
                    try:
                        row[c] = float(v)
                    except ValueError:
                        row[c] = v
                self.rows.append(row)
        self.meta = {"source": self.source, "columns": self.cols,
                     "rows": len(self.rows)}

    def _load_jsonl(self):
        self.source = "evc-jsonl"
        with open(self.path, "r", encoding="utf-8-sig") as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                obj = json.loads(line)
                if "session" in obj and "t" not in obj:
                    self.meta = obj["session"]
                    continue
                self.rows.append(self._normalize_evc(obj))
        self.meta.setdefault("source", self.source)

    @staticmethod
    def _normalize_evc(f):
        """Map an EvC frame onto SEADS-ish column names so one diff path works."""
        vel = f.get("vel", {})
        pos = f.get("pos", {})
        speed = f.get("speed", 0.0)
        row = {
            "t": f.get("t", 0.0),
            "px": pos.get("x", 0.0), "py": pos.get("y", 0.0),
            "pz": pos.get("z", 0.0),
            "vx": vel.get("x", 0.0), "vy": vel.get("y", 0.0),
            "vz": vel.get("z", 0.0),
            "speed": speed,
            "altitude": pos.get("y", 0.0),      # Roblox world-up is +Y
            "phi": f.get("bankDeg", 0.0),        # bank proxy -> phi slot
            "alpha": f.get("aoaProxyDeg", 0.0),  # AoA proxy -> alpha slot
        }
        # Aim error if the (optional) kernel feed supplied a world aim vector.
        if "noseAimErrDeg" in f:
            row["e"] = math.radians(f["noseAimErrDeg"])  # store like SEADS e (rad)
        else:
            row["e"] = math.radians(f.get("noseVelAngleDeg", 0.0))
        k = f.get("kernel") or {}
        if "pushGateCommitted" in k:
            row["push"] = 1.0 if k.get("pushGateCommitted") else 0.0
        return row

    # -- derived accessors (unit-normalized so both sources compare) --
    def series(self, key):
        return [r[key] for r in self.rows if key in r and isinstance(r[key], float)]

    def times(self):
        return [r.get("t", i) for i, r in enumerate(self.rows)]

    def has(self, key):
        return any(key in r and isinstance(r[key], float) for r in self.rows)


# --------------------------------------------------------------------------- #
# Alignment
# --------------------------------------------------------------------------- #


def align_rows(golden, cand, mode="tick"):
    """Return list of (g_row, c_row) pairs. 'tick' = index-aligned (both are
    fixed-dt tick streams -> the AT-9 correct alignment); 'time' = nearest-t."""
    if mode == "tick":
        n = min(len(golden.rows), len(cand.rows))
        return [(golden.rows[i], cand.rows[i]) for i in range(n)]
    # time alignment: for each golden row, pick the candidate row with nearest t
    ct = cand.times()
    pairs = []
    j = 0
    for g in golden.rows:
        gt = g.get("t", 0.0)
        while j + 1 < len(ct) and abs(ct[j + 1] - gt) <= abs(ct[j] - gt):
            j += 1
        pairs.append((g, cand.rows[j]))
    return pairs


# --------------------------------------------------------------------------- #
# Metrics
# --------------------------------------------------------------------------- #


def _dev(pairs, key, scale=1.0):
    """max/mean absolute deviation of `key` between aligned rows, scaled."""
    diffs = []
    for g, c in pairs:
        if key in g and key in c and isinstance(g[key], float) \
                and isinstance(c[key], float):
            diffs.append(abs(g[key] - c[key]) * scale)
    if not diffs:
        return None
    return {"max": max(diffs), "mean": sum(diffs) / len(diffs), "n": len(diffs)}


def _first_edge(cap, key="push", rising=True):
    """Tick index + time of the first rising (or falling) edge of a 0/1 flag."""
    prev = None
    for i, r in enumerate(cap.rows):
        v = r.get(key)
        if not isinstance(v, float):
            continue
        b = v >= 0.5
        if prev is not None:
            if rising and (not prev) and b:
                return {"tick": i, "t": r.get("t", i)}
            if (not rising) and prev and (not b):
                return {"tick": i, "t": r.get("t", i)}
        prev = b
    return None


def _percentile(vals, p):
    if not vals:
        return None
    s = sorted(vals)
    idx = int(math.floor(p * (len(s) - 1)))
    return s[idx]


def lathold_summary(cap):
    """Reproduce the SEADS lathold summary line from a raw/ctrl trace when the
    columns exist. Uses local-up = radial for SEADS (position through origin),
    world-up=+Y for EvC (already folded into altitude/alpha at load)."""
    out = {}
    if not cap.rows:
        return out
    # t_capture: first time |e| < 5 deg (ctrl e is radians)
    if cap.has("e"):
        for r in cap.rows:
            if math.degrees(abs(r["e"])) < 5.0:
                out["t_capture_s"] = round(r.get("t", 0.0), 3)
                break
        out.setdefault("t_capture_s", -1.0)
    # altitude loss
    alts = cap.series("altitude")
    if alts:
        out["alt_loss_m"] = round(alts[0] - min(alts), 2)
    # alpha p95 over the positive lobe
    al = [a for a in cap.series("alpha") if a > 0.0]
    if al:
        out["alpha_p95_deg"] = round(_percentile(al, 0.95), 2)
    # windmill: longest run of |in_roll| > 0.95
    if cap.has("in_roll"):
        cur = mx = 0
        for r in cap.rows:
            if abs(r.get("in_roll", 0.0)) > 0.95:
                cur += 1
                mx = max(mx, cur)
            else:
                cur = 0
        out["windmill_ticks"] = mx
    # min nose elevation is only meaningful with orientation; skip unless phi
    # trace present as a coarse proxy is misleading -> omit rather than fake.
    return out


def diff(golden, cand, mode):
    pairs = align_rows(golden, cand, mode)
    report = {
        "golden": golden.path, "candidate": cand.path,
        "golden_source": golden.source, "candidate_source": cand.source,
        "align": mode, "aligned_rows": len(pairs),
        "generated": datetime.now(timezone.utc).isoformat(),
        "drift": {}, "commitment": {}, "summary_golden": {}, "summary_cand": {},
    }
    # Feel-drift metrics (deg where an angle, native units otherwise).
    report["drift"]["aim_error_deg"] = _dev(pairs, "e", scale=180.0 / math.pi)
    report["drift"]["bank_phi_deg"] = _dev(pairs, "phi")
    report["drift"]["speed"] = _dev(pairs, "speed")
    report["drift"]["altitude"] = _dev(pairs, "altitude")
    report["drift"]["alpha_deg"] = _dev(pairs, "alpha")
    report["drift"]["in_pitch"] = _dev(pairs, "in_pitch")
    report["drift"]["in_roll"] = _dev(pairs, "in_roll")
    report["drift"]["load_factor"] = _dev(pairs, "load_factor")
    # Specific energy drift: 0.5 v^2 + g*h  (g folded via altitude column units;
    # both captures use their own consistent g, so compare the CURVES not values)
    e_g = [0.5 * g.get("speed", 0.0) ** 2 for g, _ in pairs]
    e_c = [0.5 * c.get("speed", 0.0) ** 2 for _, c in pairs]
    if e_g and e_c:
        d = [abs(a - b) for a, b in zip(e_g, e_c)]
        report["drift"]["kinetic_energy"] = {
            "max": max(d), "mean": sum(d) / len(d), "n": len(d)}

    # push_gate commitment timing (the RUNG E surface).
    if golden.has("push") or cand.has("push"):
        gp = _first_edge(golden, "push", rising=True)
        cp = _first_edge(cand, "push", rising=True)
        report["commitment"]["golden_push_engage"] = gp
        report["commitment"]["candidate_push_engage"] = cp
        if gp and cp:
            report["commitment"]["engage_shift_ticks"] = cp["tick"] - gp["tick"]
            report["commitment"]["engage_shift_s"] = round(
                cp.get("t", 0) - gp.get("t", 0), 4)

    report["summary_golden"] = lathold_summary(golden)
    report["summary_cand"] = lathold_summary(cand)
    return report


# --------------------------------------------------------------------------- #
# Rendering
# --------------------------------------------------------------------------- #


def _fmt_dev(d):
    if not d:
        return "        (n/a)"
    return "max %9.4f  mean %9.4f  (n=%d)" % (d["max"], d["mean"], d["n"])


def print_report(rep):
    print("=" * 72)
    print("FLIGHT-FEEL DIFF")
    print("  golden    :", rep["golden"], "(%s)" % rep["golden_source"])
    print("  candidate :", rep["candidate"], "(%s)" % rep["candidate_source"])
    print("  aligned   : %d rows  (align=%s)" %
          (rep["aligned_rows"], rep["align"]))
    print("-" * 72)
    print("PER-MANEUVER TELEMETRY DRIFT (candidate vs golden)")
    labels = [
        ("aim_error_deg", "nose-vs-aim error (deg)"),
        ("bank_phi_deg", "bank phi (deg)"),
        ("alpha_deg", "angle of attack (deg)"),
        ("speed", "speed (m/s)"),
        ("altitude", "altitude (m)"),
        ("kinetic_energy", "kinetic energy 1/2 v^2"),
        ("load_factor", "load factor n"),
        ("in_pitch", "elevator input"),
        ("in_roll", "aileron input"),
    ]
    for key, lab in labels:
        d = rep["drift"].get(key)
        print("  %-26s %s" % (lab, _fmt_dev(d)))
    if rep["commitment"]:
        print("-" * 72)
        print("PUSH_GATE COMMITMENT (rung E surface)")
        gp = rep["commitment"].get("golden_push_engage")
        cp = rep["commitment"].get("candidate_push_engage")
        print("  golden engage    :", gp if gp else "(never)")
        print("  candidate engage :", cp if cp else "(never)")
        if "engage_shift_ticks" in rep["commitment"]:
            print("  engage shift     : %+d ticks (%+.4f s)" % (
                rep["commitment"]["engage_shift_ticks"],
                rep["commitment"]["engage_shift_s"]))
    if rep["summary_golden"] or rep["summary_cand"]:
        print("-" * 72)
        print("LATHOLD-STYLE SUMMARY        golden        candidate")
        keys = set(rep["summary_golden"]) | set(rep["summary_cand"])
        for k in sorted(keys):
            print("  %-22s %12s   %12s" % (
                k, rep["summary_golden"].get(k, "-"),
                rep["summary_cand"].get(k, "-")))
    print("=" * 72)


# --------------------------------------------------------------------------- #
# Commands
# --------------------------------------------------------------------------- #


def cmd_diff(args):
    g = Capture(args.golden)
    c = Capture(args.candidate)
    rep = diff(g, c, args.align)
    print_report(rep)
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(rep, fh, indent=2)
        print("wrote JSON summary ->", args.json)
    return 0


def cmd_summary(args):
    c = Capture(args.capture)
    print("source :", c.source)
    print("rows   :", len(c.rows))
    print("meta   :", json.dumps(c.meta)[:400])
    s = lathold_summary(c)
    print("summary:", json.dumps(s, indent=2))
    return 0


def cmd_promote(args):
    os.makedirs(args.goldens, exist_ok=True)
    base = args.name
    if not os.path.splitext(base)[1]:
        base += os.path.splitext(args.capture)[1]
    dst = os.path.join(args.goldens, base)
    shutil.copy2(args.capture, dst)
    # Sidecar NOTES / provenance (the feel verdict).
    sidecar = dst + ".notes.json"
    note = {
        "golden_name": base,
        "source_capture": os.path.abspath(args.capture),
        "promoted": datetime.now(timezone.utc).isoformat(),
        "baseline": "feel/kernel-v5 @ 89447aba5 (pushed; diverges from main/v4)",
        "notes": args.notes or "",
    }
    with open(sidecar, "w", encoding="utf-8") as fh:
        json.dump(note, fh, indent=2)
    print("promoted golden ->", dst)
    print("notes           ->", sidecar)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Flight-feel replay diff (SEADS + EvC)")
    sub = ap.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("diff", help="diff golden vs candidate")
    d.add_argument("golden")
    d.add_argument("candidate")
    d.add_argument("--json", default=None)
    d.add_argument("--align", choices=["tick", "time"], default="tick")
    d.set_defaults(func=cmd_diff)

    s = sub.add_parser("summary", help="print a single capture's summary")
    s.add_argument("capture")
    s.set_defaults(func=cmd_summary)

    p = sub.add_parser("promote", help="copy a capture into the goldens dir")
    p.add_argument("capture")
    p.add_argument("name")
    p.add_argument("--notes", default="")
    p.add_argument("--goldens",
                   default=os.path.join(os.path.dirname(__file__), "..", "goldens"))
    p.set_defaults(func=cmd_promote)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
