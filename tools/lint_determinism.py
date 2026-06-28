#!/usr/bin/env python3
"""
lint_determinism.py — cheap static guard for the SEADS determinism bans.

Scans C++ kernel/math sources for banned libm transcendental CALLS, banned build flags,
and FMA-enabling pragmas. The det_sqrt hardware primitive in det_math.cpp is the one
sanctioned exception. This is a fast pre-build gate (and a Claude Code hook).

Usage:  python tools/lint_determinism.py
Exit:   0 clean, 1 violation.
"""
import re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = [ROOT / "src" / "kernel", ROOT / "src" / "det_math", ROOT / "src" / "replay"]

# banned libm transcendental calls (word-boundary so __builtin_sqrt / det_sqrt are fine)
BANNED_CALLS = re.compile(
    r'(?<![\w:])(std::)?(sin|cos|tan|atan2|atan|asin|acos|pow|exp|log|sqrt|hypot|cbrt)\s*\(')
# allow these det_math wrappers / kernels
ALLOW = re.compile(r'\b(det_sin|det_cos|det_tan|det_atan2|det_atan|det_asin|det_sqrt|'
                   r'kernel_sin|kernel_cos)\b')
BANNED_FLAGS = re.compile(r'(/fp:fast|-ffast-math|-Ofast|-mfma|-ffp-contract=fast)')
BANNED_PRAGMA = re.compile(r'#pragma\s+(STDC\s+FP_CONTRACT\s+ON|float_control.*push)', re.I)


def scan_cpp():
    violations = []
    for d in SCAN_DIRS:
        if not d.exists():
            continue
        for p in list(d.glob("*.cpp")) + list(d.glob("*.h")):
            for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                code = line.split("//", 1)[0]
                # strip det_* / kernel_* tokens so only raw libm calls remain
                stripped = ALLOW.sub("", code)
                if BANNED_CALLS.search(stripped):
                    violations.append(f"{p.relative_to(ROOT)}:{n}: banned libm call: {line.strip()}")
                if BANNED_PRAGMA.search(code):
                    violations.append(f"{p.relative_to(ROOT)}:{n}: banned pragma: {line.strip()}")
    return violations


def scan_flags():
    violations = []
    for name in ["CMakeLists.txt", "cmake/DeterminismFlags.cmake", ".github/workflows/guardian.yml"]:
        p = ROOT / name
        if not p.exists():
            continue
        for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
            code = line.split("#", 1)[0]  # strip cmake/yaml comments
            if BANNED_FLAGS.search(code):
                violations.append(f"{name}:{n}: banned flag: {line.strip()}")
    return violations


def main():
    v = scan_cpp() + scan_flags()
    if v:
        print("DETERMINISM LINT FAIL:")
        for x in v:
            print("  " + x)
        sys.exit(1)
    print("RESULT: DETERMINISM LINT PASS")
    sys.exit(0)


if __name__ == "__main__":
    main()
