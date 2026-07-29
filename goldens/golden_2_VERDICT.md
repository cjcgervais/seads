# GOLDEN FELT FLIGHT #2 — the v6-seal flight
- File: golden_2_v6_seal_flight.seadsrec (19,844 ticks / 165.4 s, fnv1a-signed,
  signature VERIFIED at sealing 2026-07-29; recorded 2026-07-28 by Chad on the
  grafted seads-recon build-play build).
- Chad's verdict on this kernel, flown the same day it sealed: "yes perfect as
  expected 3/3!" — the three-for-three approval session that produced the v6
  seal (rudder trim yaw_scale 2.0, S-relorient, auto-right inverted_delay 0.5 s).
- PURPOSE: the drift alarm for the v6 generation, and the FIRST recording that
  exercises S-relorient in the wild — 20 freelook releases, every one of them
  auto-oriented, zero double-taps. If a future kernel changes the release
  behaviour, this stream is where it shows.

## Provenance — read this before trusting the header
The file's own header says `tag=v5-reconcile@46051ca23`. **That string is wrong**
and must NOT be edited in place: the fnv1a signature covers the header, so
correcting it would break the signature that proves the file is untampered. The
correction belongs here, in the signing metadata:

- **kernel surface:** `5e27f237c` exactly — the sealed v6 content
  (`flight-kernel-v6-2026-07-28` = `cfe1bd7fe` in seads-feel, cherry-picked into
  the seads-recon conquest tree).
- **app layer:** seads-recon tip + uncommitted conquest WIP at flight time.
- **binary:** `build-play` RelWithDebInfo, rebuilt the night of the flight.
- **cause:** `kFeltFlightVersionTag` was a hardcoded constant in `main.cpp` that
  did not track the build. **Fixed 2026-07-29** (`400f133ec`) — recordings after
  that date self-describe via `git describe`. This golden predates the fix.

## Sealing record
- Mirrored into `goldens/` 2026-07-29 from `tools/safety/felt_flight_2.seadsrec`.
- SHA-256 `BF1A6C18CC4C981E96CA8E0B62F94B91B7DFEBE57CE327AD52B4A3FEC1B6415B`
  (6,671,757 bytes) — matches the byte-verified safety copy and the original in
  `D:\flight_sim2\seads-recon\build-play\`.
- fnv1a body signature recomputed at sealing and matched:
  `13318769238472783059`.
- The `.gitignore` excludes `*.seadsrec` by default; this one is force-added
  BECAUSE it is a golden. Promote future flights deliberately, never by default.
