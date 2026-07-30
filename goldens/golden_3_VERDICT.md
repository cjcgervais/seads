# GOLDEN FELT FLIGHT #3 — the v9-seal flight
- File: golden_3_v9_seal_flight.seadsrec (17,512 ticks / 145.9 s, fnv1a-signed,
  signature VERIFIED at sealing 2026-07-29; recorded 2026-07-29 by Chad on the
  grafted seads-recon build-play build, the same day v9 sealed and he flew it).
- Chad's verdict, verbatim, the same session: "v9 is golden and has a golden
  flight." Full report: "I shot down a couple of bandits and crash landed too,
  did maybe 10 free look presses or more, didnt notice any harmful wait on the
  mouse aim to engage after the snap. The snap is working as intended and the
  keyboard override works as intended as well."
- PURPOSE: the drift alarm for the v9 generation — the FIRST recording that
  exercises the S-nosesnap release law in the wild (20 releases, all instant
  auto-orients, two of them while inverted), plus the first golden with combat,
  a crash landing, and a respawn discontinuity. #1 = slow/dirty/ground,
  #2 = fast/clean/air, #3 = mixed/combat. None supersedes another.

## Provenance — read this before trusting the header
The file's own header says
`tag=sandbox/kernel-v5-reconcile@game-kernel-v5-34-g966654e0f kernel=flight-kernel-v7-2026-07-29`.
The describe half is CORRECT; the `kernel=` half is WRONG and must NOT be edited
in place — the fnv1a signature covers the header. The correction lives here:

- **kernel surface: sealed v9 content, exactly** — `flight-kernel-v9-2026-07-29`
  = `29787debc` in seads-feel, grafted whole into seads-recon (nine commits,
  v8 S-keyprec + all seven v9 commits; recon gate 901/901, comfort numbers
  bit-identical to the seal).
- **binary:** build-play, built at recon commit `966654e0f` (the last grafted
  ledger commit — every v9 code commit is its ancestor; the graft-marker commit
  `6058329d3` that followed is docs-only).
- **cause of the wrong string:** `KERNEL_SEAL` (the hand-maintained file that
  build_info stamps into the header — introduced by the `400f133ec` seam fix
  after golden #2's header was wrong for the *previous* reason) still said v7
  at build time: the graft brought the kernel but not the stated seal. Fixed in
  recon `4d3e848bc` immediately after; this recording predates the fix by
  minutes. Lesson recorded there: the seal file must be updated IN the graft
  commit, and the kernel identity must be recorded by hand in this ledger
  beside each recording regardless.
- **The tree was CLEAN at build** — `build_info` runs `git describe --dirty`
  and the header carries no `-dirty` suffix, so this golden is reconstructible
  from recon `966654e0f` exactly. This discharges the VERSIONS.md warning
  ("commit the recon WIP before flying #3"): the `cdf65753d` tattoo commit had
  already absorbed the in-flight work.
- Two recordings were made this session: felt_flight_3.seadsrec (54.5 s, the
  keyboard-override false-alarm test, sig-verified, NOT promoted) and
  felt_flight_4 (this one). Identification was derived from the data (20
  releases, crash-landing profile), not assumed from timestamps.
- **⚠ The seads-feel agent's same-night echo misidentified the capture as
  felt_flight_3** (2,204,646 bytes, sig 9257030133408061227). Its own stated
  countables refute that file: felt_flight_3 has 5 release edges (not ≥10) and
  never descends below 916 m (no ground contact). felt_flight_4 matches every
  countable — 20 releases, terminal ground contact, crash profile. The seal
  stands on felt_flight_4; recorded here so a future reader who finds that echo
  does not conclude the wrong file was promoted.

## Sealing record
- Mirrored into `goldens/` 2026-07-29 from
  `D:\flight_sim2\seads-recon\build-play\felt_flight_4.seadsrec`.
- SHA-256 `3948B0F8B30B3CE88053F18D26D6490F54A45567D86A495A8E90B5CFE1C2F4D9`
  (5,874,030 bytes) — source and mirrored copy verified identical at sealing.
- fnv1a body signature recomputed at sealing and matched:
  `12230860415588719877`.
- The `.gitignore` excludes `*.seadsrec` by default; this one is force-added
  BECAUSE it is a golden. Promote future flights deliberately, never by default.

## Rulings adjacent to this flight (ledger: docs/DECISIONS.md, 2026-07-29)
- CQ2 0.30 s easeback window: KEPT by ruling, new cockpit rationale ("I need
  that .4s to observe / orient myself") — this flight is the felt evidence.
- Chad's deferred v10 thought, recorded the same session: a possible autolevel
  after mouse-only aerobatics ("the earth can take on a disorienting aspect…
  but the press of the spacebar cures all and keeps the reorientation manual
  and under control. It might be worth a future sandbox but I will defer for
  now"). Deferred by his word — not an open work item.
