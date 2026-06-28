---
name: seal
description: Cut a new SEADS seal (ATM-Sphere vMAJ.MINrREV) when a rail changes. Use when R, Δt, roster, ceiling, geometry, gravity, determinism bans, or wire format change, or when the golden world_hash legitimately changes.
---

# /seal — cut a new seal

A seal is mandatory whenever a **rail** changes or the golden world_hash legitimately changes.

1. Pick the new seal id `ATM-Sphere vX.YrZ` (bump rev for value-only rail changes, minor for
   behavior changes, major for doctrine changes).
2. Update `config/rails/atm.json` `header.seal` + `header.version`, and the changed rail value.
3. Regenerate affected headers: `python tools/gen_golden_params.py` (and `gen_coeffs.py` /
   `gen_detmath_vectors.py` if det_math changed).
4. Write an **ADR** (`docs/adr/`, from `ADR-template.md`) and a **Forge Audit Card**
   (`docs/cards/`, from the template). Add an annex in `docs/annex/` for rail-value changes.
5. If the golden changed, re-seal it: `python tools/ref_kernel.py --seal` and explain the diff.
6. Update the `docs/SEAL_CARD.md` seal-history table.
7. Run `/probe`, then `/golden`, then have **Chronicler** run `tools/make_receipt.py`.

Never edit rails without completing this ritual.
