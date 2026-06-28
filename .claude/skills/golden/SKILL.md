---
name: golden
description: Run the SEADS GOLDEN-SK-Sphere-001 replay and compare the world_hash to the seal. Use to confirm the deterministic core is unchanged after any edit, or to inspect the current hash.
---

# /golden — run the golden replay + hash compare

1. Regenerate the reference snapshot:
   `python tools/ref_kernel.py --out /tmp/seads_run.bin`
2. Compare to the sealed hash:
   `python tools/validate_snapshot.py --golden tests/golden/GOLDEN-SK-Sphere-001/expected.world_hash --candidate /tmp/seads_run.bin`
3. If a C++ toolchain exists, also build and run `seads_golden --out run.bin` and validate it the
   same way — both must equal the seal.

Report PASS/FAIL and the hash. **A changed hash is a seal event** — if intended, invoke `/seal`;
if not, it is a determinism regression to fix, not to re-baseline.
