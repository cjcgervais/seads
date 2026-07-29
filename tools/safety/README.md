# tools/safety/ — emergency off-site copies, NOT authoritative

Belt-and-braces copies of artifacts whose only other copy was at risk. Files here are
**unsigned safety duplicates**, not reference outputs — the authoritative, signed version
of anything in this directory lives (or will live) wherever its owning agent puts it.
Delete a file from here once its authoritative home exists and is pushed.

**Currently empty — no artifact is at risk.**

## felt_flight_2.seadsrec — SUPERSEDED and deleted 2026-07-29

Golden Felt Flight #2 has been sealed into its authoritative home,
`goldens/golden_2_v6_seal_flight.seadsrec`, with derived telemetry and a signing record
(`golden_2_TELEMETRY.md`, `golden_2_VERDICT.md`, `golden_2_telemetry.csv`). Before deletion,
per the rule above:

- Authoritative copy's SHA-256 verified byte-identical to the safety copy and to the
  original in `D:\flight_sim2\seads-recon\build-play\`:
  `BF1A6C18CC4C981E96CA8E0B62F94B91B7DFEBE57CE327AD52B4A3FEC1B6415B` (6,671,757 bytes).
- The recording's own fnv1a body signature was recomputed at sealing and matched
  (`13318769238472783059`), so the file is provably untampered since the flight.
- The authoritative copy is committed and pushed in the same change that removes this one —
  there is no window in which no pushed copy exists.

The wrong-`tag=` header the safety note flagged is **corrected in the signing metadata**, in
`goldens/golden_2_VERDICT.md`, exactly as the flag required — the header itself was not
edited, because the fnv1a signature covers it.
