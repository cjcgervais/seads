# ADR-roster-envelopes-v1.2r0 — Remaining 7 Roster Tuning Envelopes

**Status:** Accepted
**Date:** 2026-06-28
**Author:** Forge
**Seal:** ATM-Sphere v1.2r0
**Realm:** ATM-only • **Tick:** 100 Hz • **R:** 15 km

---

## 1) Context
Pass 1 sealed the deterministic core and shipped one tuning envelope (`ki61.json`, see
`ADR-Ki-61-v1.2r0.md`). The sealed roster has 8 airframes; the other 7 had no envelope, so the
kernel cannot yet fly them with airframe-specific limits. This ADR adds those 7 as a single
**data-only** batch. Immutable rails that stay green: sphere R=15000/flattening=0; ATM-only;
ATM_TOP=8000, SOFT=100; g₀=9.80665; Δt=0.01; S² coordinated turn `ψ̇=g₀·tan(φ)/V`; det_math only;
GEO-001 wire. No kernel/wire/det_math change → golden `world_hash` is unchanged.

## 2) Decision
Add 7 envelopes under `data/tuning/envelopes/` (each: `v_ne_mps`, `stall_tas_mps`, and the four
TAS-indexed LUTs `phi_max_deg_vs_tas`, `climb_rate_max/min_mps_vs_tas`, `roll_rate_degps_vs_tas`):

| File | Aircraft | v_ne | stall | character |
|------|----------|-----:|------:|-----------|
| `p47d.json` | P-47D Thunderbolt | 205 | 46 | heavy; fastest dive (climb_min to −36); big turn (φ≤52); strong high-speed roll |
| `bf109f4.json` | Bf 109 F-4 | 185 | 42 | strong climber (19 m/s); good turn; roll falls off at speed (75→62) |
| `a6m2.json` | A6M2 Zero | 165 | 38 | tightest turn (φ≤65); low stall; structurally limited v_ne; roll freezes at speed (70→38) |
| `yak3.json` | Yak-3 | 185 | 40 | very light; agile; excellent roll (85→100) and climb |
| `la7.json` | La-7 | 195 | 43 | powerful radial; fast; strong climb (19) and roll |
| `spitfire_mk5.json` | Spitfire Mk V | 190 | 40 | excellent turn (φ≤63); good climb; moderate roll |
| `p51.json` | P-51 Mustang | 210 | 44 | highest v_ne; energy fighter; good high-speed roll (→100) |

## 3) Rationale
Values encode each airframe's well-documented relative strengths within a single comparable scale
(0–8000 m, R=15 km), not literal sea-level figures. Cross-roster balance is intentional: the Zero
out-turns everyone at low speed but loses roll authority as TAS rises; the P-47 and P-51 own the
high-speed/dive regime; the 109/Yak/Spit/La climb hard. Every LUT has a strictly increasing TAS
domain ending at `v_ne`, stays inside the probe bounds (φ∈[0,80]°, climb_max∈[−40,30],
climb_min∈[−60,0], roll∈[30,240]°/s), and respects the Lipschitz guards (≤15°/10 m/s bank,
≤10 m/s per 10 m/s TAS for both climb rates). No libm fast-paths; parsed once into tables.

## 4) Consequences
Positive: completes the roster so step 4 (wiring envelopes + climb/bank inputs into the kernel) is
unblocked; gives a balanced 8-aircraft baseline. Negative: none identified — data-only, within probe
bounds and rails; golden unchanged. Envelopes are an initial balance pass and may be retuned later
(still data-only, still rides this seal).

## 5) Alternatives Considered
Seven separate ADRs (rejected — near-identical boilerplate for one logical decision; a single batch
ADR with per-airframe rows is clearer and still satisfies Ledger Discipline). Literal sea-level
performance figures (rejected — incomparable across the tiny-sphere model and unnecessary for
gameplay balance). Wider LUT bounds for more dramatic spread (rejected — would breach probe Lipschitz
guards and reduce determinism-friendly smoothness).

## 6) Acceptance & Probes
- `python tools/tuning_probe.py data/tuning/envelopes/*.json` → PASS (all 8)
- `python tools/spec_monotone_check.py config/rails/atm.json` → PASS
- `python tools/atm_top_probe.py --ceil 8000 --soft 100` → PASS
- `python -m pytest tests/property` → PASS
- Golden `GOLDEN-SK-Sphere-001` world_hash unchanged = `529c6a05…9218fe16` (data-only).
- CI `guardian.yml` cross-toolchain remains green (no kernel/wire change).

## 7) Ledger Discipline
Receipt fields: seal=ATM-Sphere v1.2r0, tuning_added={P-47D, Bf-109-F-4, A6M2, Yak-3, La-7,
Spitfire-Mk-V, P-51}, golden_sha256=<unchanged>, probes={tuning_probe, spec_monotone, atm_top,
property}, signoff=Forge. Forge Audit Card: `docs/cards/FORGE_AUDIT_CARD-roster-envelopes-v1.2r0.md`.

## 8) Implementation Notes
Optionally sign each with `python tools/hash_sign_json.py --inplace data/tuning/envelopes/<f>.json`.
No build-flag/kernel/wire change; determinism bans remain enforced; tuning is parsed once into tables
before the sim starts (no live-reload across the lockstep boundary).

**Appendix — Roster (sealed 8):** P-47D, Bf 109 F-4, Ki-61, A6M2, Yak-3, La-7, Spitfire Mk V, P-51.
