---
name: probe
description: Run the full SEADS verification probe suite — determinism lint, det_math MPFR oracle, rails/roster check, tuning + flight probes, ceiling probe, and Hypothesis metamorphic tests. Use before any commit or seal.
---

# /probe — run all SEADS gates

Run, in order, and report each result:

```
python tools/lint_determinism.py
python tools/gen_coeffs.py --check
python tools/gen_golden_params.py --check
python tools/gen_detmath_vectors.py --check
python tools/spec_monotone_check.py config/rails/atm.json
python tools/det_math_oracle.py --samples 8000
python tools/tuning_probe.py data/tuning/envelopes/*.json
python tools/atm_top_probe.py --ceil 8000 --soft 100
python -m pytest tests/property -q
```

All must pass. If a generated-header `--check` fails, run the matching `gen_*.py` (no `--check`)
to resync, and explain why the constants changed (it may be a seal event).
