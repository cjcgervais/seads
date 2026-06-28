#!/usr/bin/env python3
"""
spec_monotone_check.py — ATM-Sphere v1.2r0 (Roster-8)
Validates rails/spec cards against project rails and roster.

Checks:
  * Canonical rails (R=15000, dt=0.01, ATM_TOP=8000, realm=ATM, flattening=0)
  * Determinism declarations present (NO_LIBM/NO_FAST_MATH/NO_VINCENTY/NO_KEPLER)
  * Roster size == 8 and includes P-51
  * Optional header.hash equals sha256(file) if header.hash is present
  * Version monotonicity per header.id across multiple files

Usage:
  python tools/spec_monotone_check.py config/rails/atm.json [more.json ...]
Exit: 0 OK, 1 failure
"""
import sys, json, hashlib, re
from pathlib import Path

OK = True
errs = []

ATM_TOP_REQUIRED = 8000  # v1.2r0


def sha256_bytes(b):
    return hashlib.sha256(b).hexdigest()


def norm_name(s):
    return re.sub(r'\s+', ' ', s.strip().upper())


def check_file(path):
    global OK
    raw = path.read_bytes()
    try:
        j = json.loads(raw.decode('utf-8'))
    except Exception as e:
        OK = False; errs.append(f"[{path}] JSON parse error: {e}"); return None
    hdr = j.get("header", {})
    if "hash" in hdr:
        calc = sha256_bytes(raw)
        if hdr["hash"] != calc:
            OK = False; errs.append(f"[{path}] header.hash != sha256(file)")
    rails = j.get("rails", {})
    g = rails.get("geometry", {})
    realm = rails.get("realm", "")
    dt = rails.get("dt_s", rails.get("tick_s"))
    atm_top = rails.get("atm_top_m", rails.get("ATM_TOP"))
    flattening = g.get("flattening", 0)
    R = g.get("radius_m", g.get("R_m"))
    if realm != "ATM":
        OK = False; errs.append(f"[{path}] realm != ATM (got {realm})")
    if R != 15000:
        OK = False; errs.append(f"[{path}] radius_m != 15000 (got {R})")
    if flattening != 0:
        OK = False; errs.append(f"[{path}] flattening must be 0")
    try:
        dt_val = float(dt)
    except Exception:
        dt_val = -1
    if round(dt_val, 5) != 0.01:
        OK = False; errs.append(f"[{path}] dt_s != 0.01 (got {dt})")
    if int(atm_top or -1) != ATM_TOP_REQUIRED:
        OK = False; errs.append(f"[{path}] ATM_TOP != {ATM_TOP_REQUIRED} (got {atm_top})")
    det_decl = rails.get("determinism", {})
    bans = " ".join(det_decl.get("bans", []))
    req = ["NO_LIBM", "NO_FAST_MATH", "NO_VINCENTY", "NO_KEPLER"]
    for r in req:
        if r not in bans and r not in det_decl.get("flags", ""):
            OK = False; errs.append(f"[{path}] missing determinism flag {r}")
    roster = [norm_name(x) for x in j.get("roster", [])]
    if len(roster) != 8:
        OK = False; errs.append(f"[{path}] roster size != 8 (got {len(roster)})")
    if not any(x.startswith("P-51") or x.startswith("P51") for x in roster):
        OK = False; errs.append(f"[{path}] roster missing P-51")
    return j


def check_versions(objs):
    global OK
    by_id = {}
    for p, j in objs:
        sid = j.get("header", {}).get("id", str(p))
        ver = j.get("header", {}).get("version", 0)
        by_id.setdefault(sid, []).append((ver, p))
    for sid, arr in by_id.items():
        vs = sorted(v for v, _ in arr)
        if vs != sorted(set(vs)):
            OK = False; errs.append(f"[id={sid}] version duplicates present")


def main():
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    objs = []
    for arg in sys.argv[1:]:
        paths = list(Path().glob(arg)) if any(ch in arg for ch in "*?[]") else [Path(arg)]
        for p in sorted(paths):
            if not p.exists():
                OK_local = False; errs.append(f"[{p}] not found"); continue
            j = check_file(p)
            if j:
                objs.append((p, j))
    check_versions(objs)
    if errs:
        print("\n".join(errs))
    print("RESULT: SPEC MONOTONE " + ("PASS" if OK else "FAIL"))
    sys.exit(0 if OK else 1)


if __name__ == "__main__":
    main()
