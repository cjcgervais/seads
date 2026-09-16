# Barrens asset manifest — what a consumer branch must carry, and together

**For:** the winter/S3 program (asked in `snow_info_packet_winter_to_barrens.md` §3).
**From:** the barrens session, `sandbox/barrens-layer`. **Date:** 2026-08-11.

## The set

The authoritative manifest is **`assets/bake_manifest.txt`** — it lists the SHA-256 of
every artifact of the one certified bake run (`bake_id=20260811T014543`, remap
`207.000000,447.592346`). A consumer branch carries the layer coherently by copying
**all of these, atomically, from the same commit**:

| file | why it moves together |
|---|---|
| `assets/sudbury_cones.png` | the layer itself — G = `barren`, R = shock fabric |
| `assets/sudbury_barrens.png` | the barrens field intermediate the accept gates read |
| `assets/sudbury_color.png` | B2 baked barren albedo into it — stale copy = grey rock |
| `assets/sudbury_treedensity.png` | B2 multiplied tree kill into it — **INV-9: your vegetation collision reads this same file**, so this copy changes where the sled drives through trees |
| `assets/sudbury_dem.png`, `sudbury_landmask.png`, `sudbury_normal.png`, `sudbury_buildings.bin` | same bake run; the manifest gate (`test_bake_manifest.cpp`) fails on a MIXED set |
| `render/sudbury_gis.gen.h` | bakes the manifest hashes + remap into the build |
| `assets/bake_manifest.txt` | the gate's ground truth |
| `assets/projection.lock` | must arrive LOCKED `0x0D1DCE234E86034EULL`; carry it byte-for-byte |
| `assets/remap_range.lock` | pins the remap the gen header asserts |

Overnight agent B's refusal of a lone-file copy was correct — this table is why.

## ★ The code that must ride with the assets

Assets alone will render the **pre-fix** layer ("too light", Chad's rejected read).
Commit `021103d3e` moved the shed law to
`depth *= (1 - k_barren * smoothstep(0.10, 0.60, b))` with `k_barren = 1.0`,
single-sourced through `render::set_barren_shed_law` from `world::SnowParams` —
in BOTH `render/planet.cpp` and `world/snowpack.cpp`. Copying assets without that
commit ships the version where `RockOutcrop` is dead code and the barrens read grey.

## Recommendation (our call, as asked)

**Wait for the merge.** The set above plus the shed-law code plus the tests that pin
them is ~the whole branch; a partial copy has two known failure modes (stale albedo,
forked shed law) and one gate designed to catch it (manifest MIXED). If sled-drive
visibility is needed before Chad merges, copy the full table above **plus** cherry-pick
`021103d3e` — and re-run your gate, since the treedensity hash change moves your
vegetation collision. Either way we sign off on the copy only as the full set.
