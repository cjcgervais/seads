#!/usr/bin/env python3
"""
hash_sign_json.py — fill/verify header.hash for SEADS spec/tuning JSON cards.

Canonical hash = SHA-256 over the JSON with header.hash blanked, serialized with
sort_keys=True and compact separators. This is a fixpoint (signing then verifying is
stable), unlike hashing the raw file that already contains its own hash.

Usage:
  python tools/hash_sign_json.py --inplace data/tuning/envelopes/ki61.json   # sign
  python tools/hash_sign_json.py --verify  data/tuning/envelopes/ki61.json   # verify
Exit: 0 OK, 1 on verify mismatch.
"""
import argparse, json, hashlib, sys
from pathlib import Path


def canonical_hash(doc):
    d = json.loads(json.dumps(doc))  # deep copy
    d.setdefault("header", {})
    d["header"]["hash"] = ""
    blob = json.dumps(d, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(blob).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--inplace", action="store_true", help="compute and write header.hash")
    g.add_argument("--verify", action="store_true", help="verify header.hash")
    args = ap.parse_args()

    p = Path(args.path)
    doc = json.loads(p.read_text(encoding="utf-8"))
    h = canonical_hash(doc)

    if args.inplace:
        doc.setdefault("header", {})["hash"] = h
        p.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
        print(f"signed {p} header.hash={h}")
        return 0

    stored = doc.get("header", {}).get("hash", "")
    if stored == h:
        print(f"PASS: {p} header.hash valid ({h})")
        return 0
    print(f"FAIL: {p} header.hash mismatch stored={stored} canonical={h}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
