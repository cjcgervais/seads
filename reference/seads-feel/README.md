# reference/seads-feel/ — PRIMARY reference: the active kernel

These files are **copies**, re-snapshotted **2026-09-15 (23:35)**, from `D:\flight_sim2\seads-feel`'s
shared repository (`D:\flight_sim2\seads`, remote `github.com/cjcgervais/seads_sandbox1`) at
**`354f6df3a`** = tag **`kernel-v16-yawbudget-signed`** (annotated, landed on `main`
2026-09-15 23:2x through this repo's sentinel; `origin/main` = `66036b98e`, a LANES-only follow-up). This is the kernel on `main`; at the
time of writing the `seads-recon` fly tree (`sandbox/r4a-phase0` @ `5d38ed150`, exe 22:17:44)
still carried v15 + AS-5 and the v16 recon merge + rebuild was in flight — check the exe mtime.

**Snapshot method:** `git archive 354f6df3a app config control docs input render sim test`.
**Purity exceptions, stated (all reproducible from the tag, none touch the kernel):**

- `render/*.gen.h` — the 26 MB generated Sudbury GIS header, omitted.
- `test/golden/sled/`, `test/golden/conquest/` — sled and AI tapes (~30 MB), not flight-kernel
  goldens, omitted. `test/golden/felt/` and `test/golden/controller_golden.h` are KEPT.
- `docs/road_repair/` — 28 MB of road census TSVs and PNGs from a world lane, omitted.
- PNGs over 200 KB anywhere in `docs/`, omitted.

Everything else is byte-for-byte the tagged tree. The previous snapshot (v12, `e362df289`)
was the complete eight-directory tree; the omissions above are the only difference in scope.

## What changed in the kernel, v12 → v15 (the chain of signed seals)

Naming changed on the live tree: seals are now `kernel-vN-<name>-signed` annotated tags at
the pushed `main` tip, landed through the SENTINEL protocol (full gate on the exact tip,
fresh red-team folded, Chad's verbatim in the `LANES.toml` kernel block, `CLAUDE.md` kernel
line + `docs/flight-log.md` row in the same landing). See this repo's `docs/DECISIONS.md`
2026-09-15 entry and `docs/VERSIONS.md`.

- **v13 (`kernel-v13`, `7ed1629e0`, 2026-08-06) — the automatic-comfort round.** Four pilot
  rulings, one theme: no cognitive load for orientation housekeeping. Freelook easeback
  RETIRED (`easeback_time` 0.30 → 0); double-tap orient RETIRED (`orient_double_tap_s` 0);
  inverted righting with NO added delay (`inverted_delay` 0.5 → 0.0); NEW rest-edge camera
  horizon recovery (`[horizon_recovery] rest_dwell` 0.15 s, 150 °/s roll about the aim's
  forward, any hand motion cancels). Gate 955/955.
- **v13g (`kernel-v13g-signed`, fly-7 2026-08-06, "that flew well").** `rest_dwell` 0.10 →
  0.05 s, `straight_max` 6 → 9 °/s, `path_band` 10 → 15°, NEW `ease_in` 500 °/s² (the
  rest-edge roll ramps from 0 instead of launching at 150 °/s; the freelook-RELEASE roll stays
  instant per the v9 ruling). Gate 958/958. (`kernel-v13f-signed` is the ai-primary-data
  handoff tag, not a kernel change.)
- **v14 (`kernel-v14-leanlead-signed`, `85896a73a`, 2026-09-10/11) — S-leanlead.** ONE dial
  `[auto_level] lean_lead` 0 → 0.3: the FINE wings-hold roll limb targets the live lean on the
  way IN, so the bank arrives WITH the rudder crab on a fine turn entry (Chad: "better than
  main, more intuitive"). Red-team P0 folded (continuous same-sign-excess clamp); controller
  golden re-recorded. Instrument `test/unit/test_yawbank_balance.cpp`. Gate 6/2015 == the
  baseline six by name.
- **v15 (`kernel-v15-righthand-signed`, `b697d24a5`, 2026-09-13) — S-righthand.** ONE dial
  `[auto_level] right_hand_rest` 0 → 0.25 s: a HAND-MOTION VETO on the inverted auto-righting
  limb (MB-right). It armed on `err < blend_lo` mid-loop and commanded the full
  `inverted_rate` 180 °/s into a hand still pulling, so a held loop rolled out at the apex
  where `unfold_bank` flips ±180° as `cosΦθ` crosses zero (the NOSE passing vertical, not the
  aeroplane being inverted). Measured at his apex: roll_right 180.0 → 0.0 °/s, gate 1.00 →
  0.00; on his tape the veto suppressed 2178 of 2344 MB-right ticks, wings level at all six
  apices (max |φ| 9.4°). Chad: "I can do the vertical loops and immelmans without a hitch",
  land word "yes land the loop fix on main". Frame-rate-invariant clock (red-team P1). Also
  carried, declared: `[auto_level] lean_lead_lateral` (`3be3e8282`) — v14's lead gated on
  the aim's horizon-lateral share (`lat_lo` 0.50 / `lat_hi` 0.85), bit-identical on a
  pure-pitch pull. Instrument `test/unit/test_loop_rollover.cpp` (drives the real
  `app::tick`). Gate on `4b59e9753`: 6 of 2060 == the baseline six by name. 0 = the v14 tree
  bit-identically, hash-pinned.

**Also in this snapshot, NOT kernel:** the feel-tape v5 instrument (`app/feel_tape.h`,
`app/feel_tape_columns.h`, `test/harness/feel_tape.h`, `test/unit/test_tape_roundtrip.cpp`)
— per-tick complete-state seeding so a recorded flight can be replayed through the real
controller; the writer and reader both derive from one column list after the "void battery"
defect (a reader that defaulted a missing column to 0.0 graded a dial against a non-dive for
a night). `app/rest_horizon.h` (extracted from `instructor_tick.h` by the cam-smooth lane).

## KERNEL v16 (`kernel-v16-yawbudget-signed`, `354f6df3a`, 2026-09-15) — S-yawbudget, IN THIS SNAPSHOT

ONE dial `[coordination] yaw_vert_budget` 1.0 (gap −5..10°, `kYawBudgetFloor` 1 °/s): the
rudder may spend at most the elevator's spare vertical authority, so a large lateral aim no
longer digs the nose at knife-edge. Chad's ruling removed the companion unload dial ("no deck
save unload keep the yaw budget"); his word on the landing exe: "I didnt notice a
difference, I can fly it fine". Kernel delta v15→v16 is five files, +350/−0:
`control/controller.cpp` (the guarded S-yawbudget block + the floor + the S-unload scar),
`control/controller.h` (Telemetry `yaw_budget_scale`), `control/params.h`,
`config/controller.toml`, `config/load_controller.cpp` (walls, `unload_*` refusal). Gate on
`b4fbea3d4` = 6 of 2101 == baseline six by name. Instrument `test/unit/test_yawbudget.cpp`
with the eight seeded `test/fixtures/*.csv` onset tapes. Cascade entry:
`docs/cascade/lateral-nose-down-unload.md`. The v15 line above still describes the tree
this one is built on.

## Lineage note

Previous snapshot headers (v10, v11, v12 content summaries) are retired from this file; that
history is in this repo's `docs/VERSIONS.md` seal table and in `git log -- reference/seads-feel`.
