# HANDOFF — START HERE. R4a: CASE-F FIXED, EVENTS BUILT, SELF-RIGHT DRIVEN. RAGDOLL IS NEXT.

**LAUNCH LINE:**
*"Read `CLAUDE.md`'s three standing laws, then `docs/R4A_THROW_RULING_20260825.md`
IN FULL — it is Chad's ruling doc for this whole rung and §6 is the one that
governs everything you build next. Then §0–§2 of this file. The next rung is the
RAGDOLL, which he named himself. Do not reopen the self-right; do not let the
LEAN anywhere near it."*

This supersedes `docs/SESSION_HANDOFF_20260825_r4a_phase0.md`. Nothing in that
file is wrong; its §4 (the finger squeeze is unreachable) and §6 (traps) still
stand and are not repeated here.

---

## 0. THE STATE, IN THREE LINES

| branch | tip | state |
|---|---|---|
| `main` | `fa2cfe3b9` | R3-HANDS **merged and driven** — thumb, brake elbow, both levers. Chad's 12° throttle throw is SIGNED. |
| `sandbox/r4a-phase0` | `31e0dd96a` | everything below. **Committed and pushed**, verified with `git ls-remote`, working tree CLEAN. |
| `sandbox/r3-hands` | `8852cf7d7` | unchanged, fully contained in main |

**Gate: 1560-odd tests, 5 reds, ALL PRE-EXISTING**, verified BY NAME (the numbers
shift as tests are added — ours added several — so a number comparison lies):

```
probe P-F: the relentless raider keeps the pump and shoots back
sled_slides_before_it_tips_on_flat_snow
sled_grip_ceiling_stays_below_the_tip_threshold
sled_assist_reference_plane_is_load_weighted
sled_debug_sink_is_write_only
```

★ `sled_debug_sink_is_write_only` was **opened and checked**, not pattern-matched:
14402/14403 assertions pass, the single red is `REQUIRE(saw_release_under_one)`,
the fixture-coverage assert. Every bit-identity check is green. If a BIT-IDENTITY
assert ever fails there, that IS a real regression — read *which* assertion.

★★★ **CORRECTED 2026-08-26 — THE REPLAYABLE CORPUS IS 31 TAPES, NOT 33.**
Tapes **32 and 33 do not replay bit-exact on this kernel**, and the claim in §3
below that they do was wrong. Attribution PROVEN by differential, not inferred
(each tape was replayed against three kernels):

| kernel | tape 31 | tape 32 | tape 33 |
|---|---|---|---|
| `2842a3350` (tape 32's recording commit) | bit-exact | **bit-exact** | won't load |
| `8e8c6f67d` (tape 33's recording commit) | bit-exact | NOT | **bit-exact** |
| `HEAD 2631ff2f4` | bit-exact | NOT | NOT |

Each tape reproduces on the kernel it was recorded on and NOWHERE LATER. Nothing
is corrupt. 32 and 33 are the SELF-RIGHT DEVELOPMENT DRIVES, recorded with the
mechanic LIVE (their own headers carry `right_assist_nm` 900 and 2400), and two
commits landed after them that changed exactly that path: `c31e07395` (standing
DOES the shift) and `31e0dd96a` (LEAN IS THE THROW, STAND IS THE RIGHTING). A
tape replays bit-exact only if the kernel is unchanged in every path it
exercises. Tape 31 is immune because it predates the mechanic and names no
`right_*` dial at all, so the mechanic is OFF for it on every kernel.

**Use 31 tapes.** 32 and 33 are historical: valid evidence only against their own
commits. If the self-right needs replayable coverage, Chad must re-drive it.

★★ **THE CORPUS IS 33 TAPES, NOT 27.** Every "27" in `R4A_PHASE0_FINDINGS.md`,
the old handoff and all five consult reports predates six later recordings (two
are Chad's self-right drives). §2 of the findings is still the 27-tape table and
has NOT been regenerated. Say which count you mean, every time.

---

## 1. WHAT LANDED, AND WHAT CHAD RULED

Fourteen commits, `ad0bd24c8` → `31e0dd96a`. The four that matter:

1. **`8c306e610` — the CASE-F fix.** `V <= 0` was a proxy for "nothing below
   carries"; the real test is the sign of `W = -(K + dz_h*V)`, for either sign of
   V. The re-run **scored a prediction written before it ran** and all five held
   (§2.5 of the findings). Headline honesty: the published percentiles barely
   move — the fix buys integrity at the `V ≈ 0` crossings, where the old rule
   stepped 14.04 N between adjacent samples against a 0.35 N budget.
2. **`d67f7f796` — the exceedance EVENTS pass.** Crossings and durations per
   candidate capacity, cut at a floor below any rulable dial with a 9-rung
   ladder, so ruling `grip_capacity_n` is a **table lookup, not a re-run**.
   Reconciles in-run against an independent arm (`Σ dur_above(c) == h·|{lp > c}|`)
   and **exits 3 rather than print a table it cannot vouch for**. JSONL sidecars
   carry location + snow-as-ridden for the terrain use Chad ruled.
3. **`2d70c1a32` — THE LEAN DIRECTION** (Chad found this). The lateral hand load
   was computed every substep since Phase 0 and read only by a `printf`. The
   planar figure omits **up to 52.7 %** on bush tapes and **13 % on a road
   cruise**, where the peak lateral load equals his entire body weight.
4. **`c31e07395` / `31e0dd96a` — the seated self-right**, driven and working.

**His rulings this session** (all verbatim in `docs/R4A_THROW_RULING_20260825.md`,
which is THE authority — read it, do not work from this summary):

- Superman happens **while he is still holding on** — the last ATTACHED state.
- Stage 4 stays **one-way**; there is no re-grab.
- The jolt is a **VECTOR**, one direction per throw, and the force **PERSISTS**.
- His **legs hit the seat** during superman.
- A **MARKER** marks the machine; his rest position and the machine's differ, and
  the run back is the felt cost.
- **P is DROPPED.** **R survives** as the escape hatch, deliberately.
- ★★★ **§6: LEAN IS THE THROW. STAND IS THE RIGHTING.** They are different
  systems and must never touch.

---

## 2. ★★★ THE RAGDOLL IS NEXT — AND READ THIS FIRST

Chad: *"so we can move on to the ragdoll."*

⚠ **"RAGDOLL" IS A BANNED WORD IN THE SPEC, AND YOU MUST SURFACE THAT AS ONE
QUESTION BEFORE BUILDING.** `docs/SUDBURIAN_LADDER.md` §7.6 says, in as many
words: *"It is a trailing-chain solver, NOT a ragdoll. Full rigid-body ragdoll is
banned here: it is a determinism hazard (iteration-count and contact-order
dependent) and a well-known quality trap that produces exactly the floppy,
weightless motion §3 and §0.4 exist to prevent."*

That is a real conflict between his word and the written spec, and Standing Law 1
says surface it, never improvise past it. The likely reading — **do not assume
it** — is that he means the TUMBLE (stage 5), which the spec wants built as the
scarf's trailing-chain solver with fixed segment count, fixed iterations and
fixed dt. Ask him whether he means that, or genuinely wants rigid-body ragdoll
and is overriding §7.6.

**The other thing the ragdoll needs, which does not exist:** `trail_chain` runs
in **sled model space** (`render/trail_chain.h:126`) where the machine's linear
and angular acceleration are identically zero — so **as shipped it cannot
superman at all**, and Chad's ruling made superman the COMMON case (CASE F, i.e.
hands-only contact, is 8–34 % of his bush riding; seven of 33 tapes are above
24 %). It needs `a_body`/`alpha_body` plumbed through `SledRig`, and the scarf's
`drag_k = 0.153/m` is a **ribbon** number, ~29× too strong for an 87.5 kg man.
Reuse it unchanged and he will stream like a flag at jogging speed.

---

## 3. THE SELF-RIGHT — DRIVEN, WORKING, DO NOT REOPEN CASUALLY

STAND (Left Shift) rights the machine: gated on tipped (ramp 0.25→0.35 rad) AND
slow (his 5 km/h, hysteretic, on a **low-passed** speed) AND standing. Ships
behind `right_assist_nm` (0.0 in the struct, 2400 in `config/scenario.toml`), in
the **name-keyed** param roster with a tape-absent preset of 0.0 — so the **31
tapes recorded BEFORE the mechanic existed** replay **bit-exact** with it live in
the game.

⚠ **CORRECTED 2026-08-26.** This originally read "all 33 tapes"; that was FALSE
and over-reached from the 31 tapes the preset actually protects. The preset works
by giving a tape that never named `right_assist_nm` the value 0.0 (= off), which
is what those drives were recorded with. It cannot protect a tape that DID name
it — tapes 32 and 33 carry the mechanic live and do not replay here. See the
corrected §0 table for the proven attribution.

★★★ **AND THE PRESET IS NOT A DESIGN, IT IS A COINCIDENCE OF ONE DEFAULT.**
`seads_sledtape::load` starts from a default-constructed `SledParams` and
overwrites ONLY the dials a tape names, so any absent dial silently takes
TODAY'S struct default rather than the value in force when Chad drove — and the
replay still says "bit-exact", because the pins were re-derived under the same
wrong assumption. That is safe right now only because `right_assist_nm` happens
to default to 0.0. **The first dial that ships a non-inert struct default
silently rewrites the history of all 31 tapes.** THE LAW again: a constant that
describes the shipped table stops describing it the moment the table moves.

**FIXED 2026-08-26** (`test/harness/sled_tape.h`): `Tape::dial_gap` now records
every comfort dial this build declares that the tape does not name — and since
the writer emits the whole `SLEDTAPE_COMFORT_D` list, "absent" means "did not
exist at record time", exactly. `probe tape` prints the gap either way and
qualifies its verdict: a clean replay against a non-empty gap now reports
**"bit-exact ON A KERNEL THIS TAPE NEVER SAW"** instead of a bare "bit-exact".
Tape 31 reports a gap of 13. Covered by
`sled_tape_dial_gap_names_exactly_the_dials_the_tape_lacks`, TWO arms
(freshly-written tape ⇒ empty gap; one dial line deleted ⇒ exactly that name),
each MUTATION-VERIFIED to kill a different stub — neither arm pins it alone.

The leg animation is `SledRig::right_push` → the foot on the far side from his
brace drops 0.16 m and reaches outboard 0.10 m. **Additive and exactly zero when
not righting**, so the signed R3 foot pose is untouched (rider suite 193/193).

**Open / honest limits, all stated to him:**
- **It rights in ONE press, not several.** Swept 1400–2400: below 2400 nothing
  rights from inversion, at 2400 one press does. There is no value in between.
  **This kernel's damping removes energy faster than a press adds it, so presses
  do not accumulate — confirmed three separate ways.** "A few sustained pushes"
  is one committed push in practice. Giving him the multi-push feel needs
  something that genuinely stores energy between presses.
- **The response is NOT monotonic in the dial**: 2400 and 3000 right from deep
  inversion, 2600 and 2800 do not. Re-run the hidden `[.probe]` leg in
  `test/unit/test_sled_selfright.cpp` before moving `right_assist_nm`.
- A continuous **hold** rights it too, and at full lean **mashing** does. With no
  lean, a committed press still beats mashing, and that is what the leg pins.
- `right_assist_nm = 2400` is **measured, not flown**. He drove it and said "it
  works now"; he has not ruled the number.

---

## 4. ★★★ THE TRAPS THIS SESSION PAID FOR — ALL FOUR ARE THE SAME TRAP

Every one is *a test that could not fail*, and Chad paid for two of them with
drives.

1. ★★★ **THE TESTS PROVED 4000 N·m WHILE THE CONFIG SHIPPED 900.** The suite was
   green and the mechanic did nothing on his machine. THE LAW again — a constant
   that describes the shipped table stops describing it the moment the table
   moves. **Fixed structurally:** `shipped()` in the self-right tests reads the
   SAME `scenario.toml` the game reads, so an outcome leg cannot be written
   against a value nobody ships. **Copy that pattern into any new feel test.**
2. ★★★ **EVERY OUTCOME LEG HELD THE LEAN AT FULL — and that was the one variable
   making it work.** From inversion it righted at lean 1.0 and never at 0.0 /
   0.25 / 0.5. The fixture set the thing under test. His drive found it; no test
   of mine could.
3. ★★ **A VACUITY GUARD THAT GUARDED THE WRONG THING.** My first CASE-F leg
   checked that a pinned solution existed but never that `V <= 0`, so the fixture
   sat entirely in the other half-plane and the mutant passed bit-identically.
   Locate a boundary by bisection; never assume where it sits.
4. ★★ **A RECONCILIATION BETWEEN TWO ZEROS.** The events identity is "EXACT" on a
   cruise tape because both sides are 0. It was verified on tape 3 where both are
   5.5292 s. **A differential is only evidence when both arms are non-zero.**

And two smaller ones worth keeping:
- **`in.stand` also moves the rider's pose, CG and drag.** A seated-vs-standing
  comparison measures the POSE, not your mechanism. Vary ONE dial, hold inputs
  identical.
- **Deep snow does not stop the self-right — it rights BETTER** (0.030 vs 0.052
  rad). My "deep snow is the failure case" was a story; the data refuted it. The
  honest emergent failure is AUTHORITY.

---

## 5. HOW TO RUN THINGS

```
# THE GAME. Chad opens the EXE; build it FOR him and give the ABSOLUTE path.
cmake --build build-play --target seads
# -> D:\flight_sim2\seads-recon\build-play\seads.exe        (never build/, -O0)

# the rider-load instrument + the events pass (subcommand `griphold`, NOT `grip`)
cmake --build build --target seads_sled_probe
./build/seads_sled_probe.exe griphold build-play/sled_tape_3.sledtape
#   exit 3 = the model is inadmissible or the events do not reconcile

# tape replay verdict (0 bit-exact, 1 not, 2 cannot open)
./build/seads_sled_probe.exe tape build-play/sled_tape_16.sledtape

# the self-right dial sweep (hidden; how the dials were MEASURED, not guessed)
./build/seads_tests.exe "selfright probe sweep"

# the gate -- ONE ctest per build dir at a time, never two
cmake --build build --target seads_tests && ctest --test-dir build
```

⚠ **Never rebuild while a gate is running** — the exe locks, the relink silently
fails, and you will mutation-test a stale binary. It happened here.
⚠ **Catch2 filters glob against the CWD.** `'marker*'` matched a stray `.png` and
reported "No tests ran", which reads like a pass. Quote filters, and treat "no
tests ran" as a failure.

---

## 6. WHAT IS OPEN, ALL NEEDING CHAD

- **`grip_capacity_n`** — the ladder makes ruling it a lookup. ⚠ But the band in
  §1.3 of the findings is derived from figures that **omit the lateral load**, so
  re-derive before he rules.
- **Does `grip_n` become 3D?** `grip3_n` is reported alongside, deliberately NOT
  folded in — folding moves every published percentile in one step. His call,
  with the difference in front of him.
- **`right_assist_nm = 2400`** — measured, not flown.
- **The marker's look** — neither of us has seen it from the viewpoint it is
  actually used from (on foot, walking back). It draws only under
  `SEADS_SLED_MARKER=1`; its real trigger `!rider_attached` does not exist.
- **The leg-seat strike**: visual only, or does it feed the throw? Open in §2.3.
- **The kernel fields** (`grip_load_n`, `rider_attached`, …) are still NOT built.
  §3 item 3 of the old handoff still governs: ship OFF, bit-identical, sentinel
  **1e9 never infinity**, latch on the **low-passed** value, and **do not extend
  the positional pin roster** (41 doubles, read by index — a 42nd refuses every
  tape).
