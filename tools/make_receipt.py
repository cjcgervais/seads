#!/usr/bin/env python3
"""
make_receipt.py — generate a Chronicle Receipt (Ledger Discipline) for SEADS.

Runs every local gate, regenerates the reference golden, compares to the sealed
world_hash, captures the git commit, and writes a YAML receipt under docs/receipts/.

Usage:  python tools/make_receipt.py [--signoff "Forge"] [--notes "..."]
Exit:   0 if all gates PASS, 1 otherwise (receipt is still written, marked FAIL).
"""
import argparse, json, subprocess, sys, tempfile, os, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
PY = sys.executable


def run(cmd):
    r = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)
    return r.returncode == 0, (r.stdout + r.stderr).strip()


def git(*args):
    ok, out = run(["git", *args])
    return out if ok else "<unknown>"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--signoff", default="Forge")
    ap.add_argument("--notes", default="Automated Chronicle receipt.")
    args = ap.parse_args()

    rails = json.loads((ROOT / "config/rails/atm.json").read_text(encoding="utf-8"))
    seal = rails["header"]["seal"]
    gid = rails["golden"]["id"]
    golden_dir = ROOT / "tests" / "golden" / gid
    expected_hash = ""
    eh = golden_dir / "expected.world_hash"
    if eh.exists():
        expected_hash = eh.read_text(encoding="utf-8").strip()

    gates = {}
    gates["spec_monotone_check"], _ = run([PY, str(TOOLS / "spec_monotone_check.py"),
                                           str(ROOT / "config/rails/atm.json")])
    gates["det_math_oracle"], _ = run([PY, str(TOOLS / "det_math_oracle.py"), "--samples", "2000"])
    envelopes = sorted(str(p) for p in (ROOT / "data/tuning/envelopes").glob("*.json"))
    gates["tuning_probe"], _ = run([PY, str(TOOLS / "tuning_probe.py"), *envelopes])
    gates["atm_top_probe"], _ = run([PY, str(TOOLS / "atm_top_probe.py"),
                                     "--ceil", "8000", "--soft", "100"])
    # GEO-001 wire codec: reference self-test + generated parity header in sync.
    geo_ref_ok, _ = run([PY, str(TOOLS / "geo001_ref.py")])
    geo_vec_ok, _ = run([PY, str(TOOLS / "gen_geo001_vectors.py"), "--check"])
    gates["geo001_codec"] = geo_ref_ok and geo_vec_ok
    # GEO-001 snapshot framing (netcode layer 2): reference self-test + parity header in sync.
    snap_ref_ok, _ = run([PY, str(TOOLS / "snapshot_ref.py")])
    snap_vec_ok, _ = run([PY, str(TOOLS / "gen_snapshot_vectors.py"), "--check"])
    gates["snapshot_codec"] = snap_ref_ok and snap_vec_ok
    # Loopback lockstep (netcode layer 3): reference self-test + parity header in sync.
    lock_ref_ok, _ = run([PY, str(TOOLS / "lockstep_ref.py")])
    lock_vec_ok, _ = run([PY, str(TOOLS / "gen_lockstep_vectors.py"), "--check"])
    gates["lockstep"] = lock_ref_ok and lock_vec_ok
    # Remote interpolation (netcode layer 4a): reference self-test + parity header in sync.
    interp_ref_ok, _ = run([PY, str(TOOLS / "interp_ref.py")])
    interp_vec_ok, _ = run([PY, str(TOOLS / "gen_interp_vectors.py"), "--check"])
    gates["interp"] = interp_ref_ok and interp_vec_ok

    # regenerate golden candidate and validate against the seal
    cand = Path(tempfile.gettempdir()) / "seads_golden_candidate.bin"
    gen_ok, _ = run([PY, str(TOOLS / "ref_kernel.py"), "--out", str(cand)])
    if gen_ok and eh.exists():
        gates["validate_snapshot"], _ = run([PY, str(TOOLS / "validate_snapshot.py"),
                                             "--golden", str(eh), "--candidate", str(cand)])
    else:
        gates["validate_snapshot"] = False

    # scenario goldens (step 4): regenerate each via the reference and validate its seal
    scen_dir = ROOT / "config" / "scenarios"
    scen_ok = True
    for sp in sorted(scen_dir.glob("*.json")):
        sid = json.loads(sp.read_text(encoding="utf-8"))["header"]["id"]
        seh = ROOT / "tests" / "golden" / sid / "expected.world_hash"
        sc = Path(tempfile.gettempdir()) / f"seads_{sid}.bin"
        g_ok, _ = run([PY, str(TOOLS / "ref_kernel.py"), "--scenario", str(sp), "--out", str(sc)])
        v_ok = False
        if g_ok and seh.exists():
            v_ok, _ = run([PY, str(TOOLS / "validate_snapshot.py"),
                           "--golden", str(seh), "--candidate", str(sc)])
        scen_ok = scen_ok and v_ok
    if list(scen_dir.glob("*.json")):
        gates["validate_scenarios"] = scen_ok

    gates["property_tests"], _ = run([PY, "-m", "pytest", "-q", str(ROOT / "tests/property")])

    all_pass = all(gates.values())
    commit = git("rev-parse", "HEAD")
    short = git("rev-parse", "--short", "HEAD")
    # filesystem-safe short tag (no commits yet -> 'uncommitted')
    safe_short = "".join(ch for ch in short if ch.isalnum()) or "uncommitted"
    # date without Date.now-style nondeterminism concerns in this context: real wall clock is fine here
    date = datetime.date.today().isoformat()

    def yn(b):
        return "PASS" if b else "FAIL"

    lines = [
        f"seal: {seal}",
        "realm: ATM-only",
        "geometry:",
        "  radius_m: 15000",
        "  flattening: 0",
        "tick_dt_s: 0.01",
        "gravity_mps2: 9.80665",
        "ceiling:",
        "  atm_top_m: 8000",
        "  soft_band_m: 100",
        "wire_hash: GEO-001",
        f"change_summary: \"{args.notes}\"",
        "rail_change: false",
        "gates:",
    ] + [f"  {k}: {yn(v)}" for k, v in gates.items()] + [
        f"golden_id: {gid}",
        f"golden_sha256: \"{expected_hash}\"",
        f"commit_sha: \"{commit}\"",
        "toolchain_matrix: [python-ref, MSVC2022, Clang18, GCC13, x64, AArch64]",
        f"date: \"{date}\"",
        f"overall: {yn(all_pass)}",
        f"signoff: \"{args.signoff}\"",
    ]
    receipt = "\n".join(lines) + "\n"

    rdir = ROOT / "docs" / "receipts"
    rdir.mkdir(parents=True, exist_ok=True)
    fname = f"receipt-{seal.replace(' ', '_').replace('/', '-')}-{safe_short}.yml"
    (rdir / fname).write_text(receipt, encoding="utf-8")

    print(receipt)
    print(f"wrote docs/receipts/{fname}")
    sys.exit(0 if all_pass else 1)


if __name__ == "__main__":
    main()
