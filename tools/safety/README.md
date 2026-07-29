# tools/safety/ — emergency off-site copies, NOT authoritative

Belt-and-braces copies of artifacts whose only other copy was at risk. Files here are
**unsigned safety duplicates**, not reference outputs — the authoritative, signed version
of anything in this directory lives (or will live) wherever its owning agent puts it.
Delete a file from here once its authoritative home exists and is pushed.

## felt_flight_2.seadsrec

- **What:** Golden Felt Flight #2, flown 2026-07-28 by Chad on the sealed v6 kernel
  (recon build, grafted tip `5e27f237c`). See `docs/VERSIONS.md` for coverage stats and
  the provenance flag that must ride with the eventual signing.
- **Why here:** at copy time the only copy on Earth was
  `D:\flight_sim2\seads-recon\build-play\felt_flight_2.seadsrec` — gitignored, untracked,
  unpushed, in a build directory that clean scripts can delete.
- **Copied:** 2026-07-28, byte-verified. SHA-256:
  `BF1A6C18CC4C981E96CA8E0B62F94B91B7DFEBE57CE327AD52B4A3FEC1B6415B` (6,671,757 bytes).
- **Supersession:** the harness agent's pending pass — sign it and mirror it into the
  recon repo's `test/golden/felt/` (and/or this repo's `goldens/`, which is theirs) —
  produces the authoritative copy. Once that is committed and pushed, this file should be
  deleted from `tools/safety/`. Verify the authoritative copy's SHA-256 matches the hash
  above before deleting.
- **Do not** edit the file, and do not treat this copy as the signed golden.
