# ADR-<slug>-<seal> — <Short Title>

**Status:** Proposed | Accepted | Superseded
**Date:** YYYY-MM-DD
**Author:** <agent/human> — <role>
**Seal:** ATM-Sphere vX.YrZ
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
What forces are at play? Which rails are touched (if any)? List immutable rails that must stay green.

## 2) Decision
The change, precisely. Exact files/paths and values.

## 3) Rationale
Why this is correct: realism, compliance, determinism, ops simplicity.

## 4) Consequences
Positive / negative. Any new gates introduced.

## 5) Alternatives Considered
Rejected options and why.

## 6) Acceptance & Probes
Concrete commands that must PASS under this seal. Always include:
- `python tools/spec_monotone_check.py config/rails/atm.json`
- `python tools/det_math_oracle.py`  (if det_math touched)
- `python tools/tuning_probe.py data/tuning/envelopes/*.json`  (if data touched)
- Golden: `GOLDEN-SK-Sphere-001` world_hash — state changed? (must bump seal) / unchanged?

## 7) Ledger Discipline
Chronicle receipt fields (seal, golden_sha256, commit_sha, toolchain, gates, signoff).

## 8) Implementation Notes
Build flags, kernel/wire impact, header.hash signing.
