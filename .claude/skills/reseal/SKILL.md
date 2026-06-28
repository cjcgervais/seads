---
name: reseal
description: Apply a rails VALUE-ONLY change (e.g. raise the ceiling ATM_TOP) as a clean reseal with no kernel/wire edits. Use for tuning a rail constant where the golden may or may not change.
---

# /reseal — rails value-only change kit

For changes that only move a rail's numeric value (the canonical example: ATM_TOP 6000→8000),
with no kernel/wire/schema edits.

1. Edit only the value in `config/rails/atm.json`; bump `header.seal`/`header.version`.
2. `python tools/gen_golden_params.py` to resync the C++ inputs.
3. Add `docs/annex/<RAIL>_vX.Y.md` documenting old→new value + invariants that stay green.
4. Add a Forge Audit Card; run `/probe` and `/golden`.
   - If the golden world_hash is unchanged, note "content unchanged; seal tag only".
   - If it changed (the value affects the golden trajectory), re-seal it (`ref_kernel.py --seal`)
     and explain.
5. Chronicler writes the receipt (`tools/make_receipt.py`).

This is the lightweight sibling of `/seal` for pure value moves — still fully ledgered.
