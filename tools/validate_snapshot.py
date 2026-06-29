#!/usr/bin/env python3
"""
validate_snapshot.py — ATM-Sphere v1.2r0
Compares a candidate world snapshot against the sealed golden.

The --golden argument may be either a snapshot .bin (compared byte-for-byte) or a
text file containing the expected hex world_hash.

Usage:
  python tools/validate_snapshot.py --golden tests/golden/GOLDEN-SK-Sphere-001/golden_snapshot.bin \
                                    --candidate run.bin
  python tools/validate_snapshot.py --golden tests/golden/GOLDEN-SK-Sphere-001/expected.world_hash \
                                    --candidate run.bin
Exit: 0 identical, 1 mismatch or error.
"""
import argparse, hashlib, sys, struct, os, re

HEXLINE = re.compile(r'^[0-9a-fA-F]{64}\s*$')


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def golden_hash(path):
    # If the golden file is a 64-hex-char text line, treat it as the expected hash.
    with open(path, 'rb') as f:
        head = f.read(80)
    try:
        text = head.decode('ascii')
        if HEXLINE.match(text.strip()):
            return text.strip().lower()
    except Exception:
        pass
    return sha256_file(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--golden", required=True)
    ap.add_argument("--candidate", required=True)
    args = ap.parse_args()
    for p in (args.golden, args.candidate):
        if not os.path.exists(p):
            print(f"FAIL: not found: {p}")
            sys.exit(1)
    g = golden_hash(args.golden)
    c = sha256_file(args.candidate)
    if g != c:
        print("FAIL: WORLD_HASH MISMATCH")
        print(f"  golden    sha256: {g}")
        print(f"  candidate sha256: {c}")
        sys.exit(1)
    print("PASS: WORLD_HASH IDENTICAL")
    print(f"  sha256: {g}")
    # Optional header sanity peek (best-effort).
    try:
        with open(args.candidate, 'rb') as f:
            hdr = f.read(32)
        if len(hdr) >= 24:
            mode = struct.unpack_from("<H", hdr, 0)[0]
            dt_s = struct.unpack_from("<d", hdr, 8)[0]
            R_m = struct.unpack_from("<d", hdr, 16)[0]
            warn = []
            if mode != 1: warn.append("Mode!=1(ATM)")
            if abs(dt_s - 0.01) > 1e-12: warn.append("dt!=0.01")
            if abs(R_m - 15000.0) > 1e-9: warn.append("R!=15000")
            # tick_count is scenario-specific (and covered by the hash); not a rail invariant.
            if warn:
                print("  WARN header: " + ", ".join(warn))
    except Exception:
        pass
    sys.exit(0)


if __name__ == "__main__":
    main()
