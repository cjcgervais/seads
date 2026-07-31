# Golden Felt Flight #5 — VERDICT (Chad) and signing record

## Chad's words (verbatim — the golden's commissioning and identification)

> "the second flight that I just recorded is the new golden flight. I was in thin air
> for my first attempt."

And the seal verdict this golden flies under (the S-straightline fly, same night,
sealed as v12):

> "yes I really like it. This is now the baseline for a quality flight kernel. This v11
> is the standard. Lets seal it and fly the golden."

(His "v11" = the build in his hands = sealed v11 + S-straightline = **v12**; the
numbering note is ledgered in DECISIONS — the felt_flight_18 class, resolved the same
way.)

## Purpose — what #5 pins

**The v12 straight line.** First golden flown on a kernel where the turn-entry tracer
dip is closed (S-rollmix + S-straightline), and the first recorded under recorder v2 —
the regime-boundary state (`telem_blend`, `telem_held_bank`) is native on the tape. Its
regression instrument is the **straight-line predicate** (TELEMETRY: parasitic-class
dip < 1.0° on committed near-level entries; measured max 0.58° vs the v11 baseline
2.23–8.31°). It also lays the first blend/dwell baselines. **It does NOT supersede any
earlier golden**: #1 slow/dirty/ground, #2 fast/clean/air-combat, #3 mixed/combat,
#4 fast/clean/slow-tracking butter, **#5 = fast/low/maneuver-dense straight-line**.
#4 remains the butter golden (this tape has no deliberate butter segment — stated in
TELEMETRY known limits).

## Identification — from the tape's own data, before trusting any echo

Two tapes were recorded this session (22:22 and 22:23, 2026-07-30):

- `felt_flight_19.seadsrec` (37.7 s): alt 650–3122 m, mean 2257 m — the HIGH thin-air
  take, matching Chad's "I was in thin air for my first attempt." **Set aside, NOT
  promoted.** Sig-verified (fnv1a `14421491564803575365` matches), SHA-256
  `F54BDC044E9C06294AA8069861B6D1F428869283FC8B8B3276BC3E0429DA4FFE`. **THIRD occurrence
  of the thin-air/spawn-altitude class at a golden session** (v9's felt_flight_3, v10's
  felt_flight_17, now this) — the checklist's spawn-altitude check exists because of it;
  it fired correctly this time.
- `felt_flight_20.seadsrec` (55.8 s): alt 298–881 m — low, dense air; the longer, fuller
  session with freelook use and the deepest FINE share. **THE GOLDEN.** Identification
  agrees with Chad's word AND stands on the data independently.

## Signing record

- **fnv1a body signature: VERIFIED** — stored `6046690173471929225`, recomputed match.
  Implementation validated against sealed Golden #4 (`7215269534658975596` reproduced)
  BEFORE trusting any result, per the documented procedure. This session's first
  recompute mismatched on raw bytes — the documented LF-normalization clause (CRLF on
  disk, LF in the hashed body); diagnosed as implementation error against the sealed
  golden, exactly as the Golden-#4 note prescribes. The procedure caught its second
  distinct implementation-error cause (v10 session: wrong constants; this session:
  line endings). Also caught pre-derivation: a column-index error (pin_throttle read as
  telem_blend) — found because "blend==1 at 100% of ticks" failed the smell test, fixed
  before any number was derived.
- **SHA-256 source↔mirror: VERIFIED** —
  `2A9A01A1F4BC327B7DBF7E3781724BBBCBC28B51305E1C06E2E659BC9667DCE9`, byte-identical,
  source left in place in `build-play/` (copy, not move).
- **Telemetry derived from the recording's own per-tick pins only** — constants and
  instrument definitions stated in full in TELEMETRY; no re-simulation.
- **Header correction (never-edit-a-header rule):** the header's
  `kernel=flight-kernel-v10-2026-07-30` is STALE — a stale seal string at build time,
  the same class as Golden #3's wrong header (that one said v7). The truth is the build
  stamp `game-kernel-v5-46-g2116f6ea3` = the **v12** recon graft (`2116f6ea3`, gate
  914/914), and the content agrees (parasitic dips at v12 closures). The recon
  `KERNEL_SEAL` string should be refreshed at the next graft — noted for the seads-feel
  agent; the header stands unedited, covered by its signature.
- Promoted with `git add -f` (deliberate, per the `.seadsrec` ignore rule).
- Sealed 2026-07-30 (late night) by the kernel-docs agent on Chad's word.
