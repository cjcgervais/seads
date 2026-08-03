# TAPE-SCHEMA — the deterministic source of truth for [EvCTAPE]

**The file that governs:** `D:/mandalark-kernel/harness/TAPE-SCHEMA.tsv`
**Validated by:** `D:/mandalark-cascade-research/tools/check_schema.py`
**Subordinate to:** `D:/mandalark-kernel/harness/SOP-01-PRIMARY-DATA.md` (H1–H8, C1–C8). Where
this document and SOP-01 disagree, SOP-01 wins.
**Derived from:** `D:/mandalark-cascade-research/impl/field-map.md` (per-field source
expressions) and `D:/mandalark-cascade-research/PREFLIGHT-GATE.md` §5 (the 31-field spec, lines
370-409). Neither of those two files was edited to produce this one; this file is read-derived
from them and from nothing else. `BirdController.client.luau` was **not opened** while writing
this (a concurrent process owns that file) — every `file:line` citation below is copied from the
field-map, not independently re-verified against the source this session.

---

## What this file is for

SOP-01's Harness Contract (H1) says: *"Fixed field order, fixed precision, a versioned schema
string in the header. No field may be conditional, reordered, or omitted between rows. A row is
a fixed-width contract, not a message."*

`TAPE-SCHEMA.tsv` is that contract, written down once, in one place, as data rather than as
prose scattered across a Luau file's comments. Every consumer — the print statement in
`BirdController.client.luau`, the harvest script (`Harvest-Tape.ps1` / `tools/*`), the analysis
tooling in §6 of `PREFLIGHT-GATE.md`, and any human reading a tape six months from now — reads
the same 34-row table instead of re-deriving field order and precision from memory or from a
comment that has drifted.

`check_schema.py` exists because a schema file that nobody checks is exactly the kind of
"an agent could forget it" defect SOP-01 exists to prevent. It is run in this session, on this
file, and its real output is pasted below — not asserted.

---

## Column definitions

| column | meaning |
|---|---|
| `ord` | Fixed column position, 1-indexed, matching the field's position in the printed CSV row. **Permanent.** Per H1, an `ord` may never be reordered or reused. If a field is ever retired, its row stays in the table with `type` set to `RETIRED`; the `ord` is never reassigned to a new field, and the schema_version's major component increments. |
| `name` | The field's short identifier, matching the column name used in the `[EvCTAPE] h,cols=...` header line (G6). |
| `units` | Physical unit or dimensionless category (`deg`, `deg/s`, `s`, `studs/s`, `unitless`, `bool`, `bitfield`). |
| `type` | `int` or `float` — the text-encoding family. (`RETIRED` for a struck field; see above.) There is no third numeric type: everything is printed as decimal text via `print`, never binary, so `int`/`float` fully describes the parse rule together with `precision`. |
| `precision` | Number of digits after the decimal point for `float` fields; always `0` for `int` fields. Combined with `type`, this fixes the exact byte pattern of every field on every row — the literal meaning of H1's "byte-predictable." A checker (or a human) can compute the expected string for any legal value and compare it exactly. |
| `range` | The value's expected envelope under normal flight — either a bracketed numeric interval `[lo,hi]`/`(lo,hi)`, a discrete set `{v1,v2,...}`, or a short prose note where a hard bound isn't physically meaningful (e.g. `t`'s monotonic session clock). This is an **engineering bound for sentinel-collision checking**, not a hard clamp enforced on the flight quantity itself — H5 forbids the harness from altering any flight quantity, so `range` documents what a healthy row looks like, it does not gate what gets written. |
| `sentinel` | The literal value emitted when the field cannot be sourced this frame. See "Sentinel convention" below. |
| `question_it_answers` | One sentence: what pilot claim or gate item this field exists to adjudicate. Copied/condensed from `PREFLIGHT-GATE.md` §5. |
| `source_ref` | Where the value comes from: a citation into `field-map.md`'s per-field row (which itself cites `BirdController.client.luau`/`FlightPhysics.luau` file:line) and, where relevant, the `PREFLIGHT-GATE.md` clause (M1–M6) that specifies a field requiring genuinely new state. Never blank; a field with no source is a field that was invented, which H4 forbids as surely as a fabricated value would be. |

---

## Sentinel convention — the literal string `NA`

**Chosen convention: every field, on any row where it cannot be sourced this frame, emits the
literal text `NA`** in that CSV position — not `-1`, not `-999999`, not `0`.

**Why this and not an out-of-range numeric magic value:**

1. **Type-level distinguishability, not just range-level.** An out-of-range numeric sentinel
   (e.g. `-999999` for a field ranged `[-90,90]`) is safe only as long as every downstream
   consumer remembers to check the range before trusting the number. `NA` fails to parse as a
   float or int at all — any consumer doing `tonumber(field)` or `float(field)` gets an
   immediate, structural failure instead of a silently-wrong-but-plausible read. This is the
   stronger form of H4's requirement: the sentinel isn't merely "distinguishable from a real
   zero," it is distinguishable from *every* real value, mechanically, without the consumer
   having to know the field's range.
2. **No collision surface as ranges evolve.** If a field's envelope is later widened (e.g.
   `rollVel` reasonably observed at 750 deg/s instead of the current 720 bound), a numeric
   sentinel chosen against today's range can silently become a plausible value tomorrow. `NA`
   never collides with any numeric range, by construction, forever.
3. **Consistent with the pre-existing machine-readable schema.** `tools/evctape_schema.json`
   (already in this repo, predating this file) already used `"sentinel": "NA"` for the same
   reason. This file adopts that convention rather than inventing a second one, so the two
   schema artifacts do not disagree about the one thing that most needs to agree.
4. **`check_schema.py`'s collision check still runs regardless.** The checker parses `sentinel`
   as a number if it can, and if it can, verifies it falls outside `range`. If a future editor
   ever changes a sentinel to a bare number, the checker catches the regression immediately — see
   the fault-injection run below, where a numeric sentinel placed inside a field's range was
   flagged as a finding.

**`int` fields** (`n`, `phiOk`, `rollSat`) use the same `NA` string, not a numeric int sentinel
like `-1` — `phiOk` and `rollSat` are small discrete domains (`{0,1}` and `[0,3]`) where `-1`
would already be an obviously-invalid value to a careful reader, but "obviously invalid to a
careful reader" is exactly the standard SOP-01 rejects (H4: *"anything an agent could forget, the
code must refuse"* — a human being careful is not code refusing). `NA` needs no field-specific
knowledge to recognize.

**Practical note on when `NA` is actually expected to appear:** for most of the 34 fields
(`n`, `t`, `dt`, and anything read from `flightEngine`/`rootPart.CFrame`/`aimTargetDir`, all of
which are established, always-populated locals inside `onFlightStep`), `NA` should essentially
never appear on a healthy tape — its purpose is defensive: if a future refactor breaks one of
these reads (e.g. `flightEngine` becomes `nil` mid-flight), the row must say `NA` in that column,
never silently substitute `0`, because a `0` in a units-typed column reads as a measurement (H4).
A tape with `NA` values present is itself diagnostic and must be reported, not hidden.

---

## Precision — why it is fixed per field, not uniform

Fields fall into three precision bands, matching `tools/evctape_schema.json`'s pre-existing
decision (this file adopts, not re-derives, those decimal counts, since a second, disagreeing
decimal convention across the two schema artifacts would itself be an H1 violation):

- **0 decimals** — `n` (row index), `phiOk` (boolean), `rollSat` (bitfield 0–3): all are already
  exact integers; a decimal point would be a lie about precision that does not exist.
- **2 decimals** — the physical/attitude channel: `t`, `spd`, `aoa`, `noseElev`, `aimElev`,
  `aimAz`, `defl`, `phiH`, `phiInt`, `bankRaw`, `rollVel`, `pitchVel`, `yawVel`. Two decimal
  places on a degree or studs/s quantity is well below any claim's discrimination threshold
  (claim 1's regression test looks for shifts on the order of tenths of a second and multiple
  degrees, per `PREFLIGHT-GATE.md` §6.3).
- **4 decimals** — `dt` and every `-1..1`-scaled control-law contributor (`rollP` through
  `kEff`). `dt` needs sub-millisecond resolution to be a meaningful frame-rate witness (a 60 Hz
  frame is 0.0167 s; 2 decimals would round every frame to the same `0.02`). The roll-term
  contributors are summed and compared against each other for attribution (`PREFLIGHT-GATE.md`
  §6.4 step 4/5) — small terms like `rollDmpH` can plausibly be under 0.01 in magnitude, and 2
  decimals would zero them out, defeating the entire point of M2's decomposition.

---

## `range` — where the bound came from, and its limits

Most `range` values are taken directly from the mathematical form of the source expression named
in `field-map.md` (e.g. any `asin(...)` result is bounded `[-90,90]` in degrees; any `acos(...)`
result is bounded `[0,180]`). Where the source is a raw physical rate or a control-law term with
no closed-form bound, `range` states an engineering envelope wide enough that no plausible flight
value collides with it, documented in-row rather than asserted from nothing (e.g. `rollVel`
`[-720,720]` deg/s is a generous multiple of the file's own documented "~183 deg/s top end"
comment cited in `field-map.md` row 12/14).

**`range` is explicitly not a clamp.** H5 forbids the harness from altering a flight quantity —
if a real value falls outside the stated `range` (e.g. a spin produces `rollVel` > 720 deg/s),
the harness must still print the true value, unclamped. `range`'s only job in this schema is to
give `check_schema.py` (and a human) something to check a `sentinel` against for collision. A
flight value that exceeds `range` is a fact about the flight, to be reported, not a schema
violation.

---

## Version history

| version | ords | what changed | why |
|---|---|---|---|
| `EvCTAPE-v1` | 1–31 | the original comprehensive record | PREFLIGHT-GATE §5 |
| `EvCTAPE-v2` | 1–34 | **appended** `mouseDx`, `mouseDy`, `keyMask` (ords 32–34); added `aimMouseSensitivity` and `aimAnglePerPixel` to the required header keys | v1 was an **observation** record, not a **replay** record: 31 fields answering *what did the aircraft do*, none answering *what did the pilot ask*. See `D:/mandalark-cascade-research/findings/S65-REPLAY-FEASIBILITY.md` §2. |

**v1 is archived at `D:/mandalark-cascade-research/tools/schemas/EvCTAPE-v1.json` and every tool
reads a tape against the version the tape's own header declares** (RECORDER-CONTRACT R5). Bumping
without archiving is what orphans a corpus from its own checker; the archive was written *before*
the bump, and `check_tape.py`, `reconcile_roll.py`, `parse_tape.py` and `gate.py` all resolve
through the one shared `schema_resolve.py` so they cannot disagree about what a row is.

**Why the new columns are appended and never inserted:** R5. A trailing column leaves every
left-indexed reader of a v1 tape working unchanged. An inserted column silently changes the
meaning of every ord after it — the same prohibition as never reusing an ord.

**Where ords 32/33 are captured, and why it is load-bearing:** at
`BirdController.client.luau:3770`, **before** `sens = aimMouseSensitivity * aimAnglePerPixel`.
The value on the tape is therefore RAW SCREEN PIXELS — the pilot's hand, not the hand times a
knob. Recording the post-`sens` value would have meant a sensitivity change silently made old and
new tapes incomparable with nothing on either tape saying so, which defeats the exact question
the input columns exist to answer. Both knobs are required v2 header keys, so the commanded angle
stays reconstructible and a knob change is a header difference `compare_arms.py` can see.

**The `keyMask` bit map is minted into the emitted header**, alongside the pole bits
(`keybit1=pitchUp` … `keybit256=freeLook`). A bitfield whose legend lives only in this file is
readable only by someone holding the matching revision of this file; a tape must carry its own
dictionary or it stops being self-identifying the moment the two drift. **Bits are permanent at
mint, exactly as ords are:** a future key gets a new bit, never a reused one.

---

## Retirement rule (H1)

No field is retired as of this schema version. When one is:

1. Its row stays in `TAPE-SCHEMA.tsv` at its original `ord`.
2. `type` becomes `RETIRED`.
3. `precision`, `range`, and `sentinel` may be left as historical documentation (the checker
   skips byte-predictability/sentinel checks on `RETIRED` rows — there is nothing live to
   validate) but `source_ref` and `question_it_answers` are updated to say what replaced it and
   why, so the row remains an audit trail rather than a hole.
4. `schema_version`'s major version increments, because the printed row's column count or
   meaning at that `ord` has changed for any code that isn't aware of the retirement.

`check_schema.py` enforces the mechanical half of this: an `ord` sequence with a hole in it
(a field removed outright rather than marked `RETIRED`) is flagged as a finding, not silently
accepted.

---

## Checker output (this session, real file)

```
$ python check_schema.py D:/mandalark-kernel/harness/TAPE-SCHEMA.tsv
checked 34 data row(s)
GREEN: TAPE-SCHEMA.tsv passes all checks (no duplicate ords, no gaps, every field has a sentinel
distinct from its range, every field cites a source).
exit=0
```

**Fault-injection proof the checker is not a rubber stamp** (H8's standard applied to the checker
itself — a clean run on an unmodified file proves nothing; this run demonstrates the checker
detects known-bad input): a scratch copy of the TSV was mutated with four independent injected
defects — `ord 5` duplicated onto `ord 4`; `ord 31` (`kEff`) deleted outright, opening a gap;
`defl`'s `source_ref` blanked; `rollP`'s `sentinel` changed from `NA` to the numeric `5`, which
falls inside its declared `[-20,20]` range.

```
$ python check_schema.py <mutated copy>
checked 30 data row(s)
RED: 4 finding(s)
 - line 7: duplicate ord 4 (already used at line 6)
 - ord sequence has gaps (retired fields must keep their ord, marked RETIRED, not be removed): missing [5]
 - line 11 (defl): source_ref is missing or a placeholder ('')
 - line 19 (rollP): sentinel '5' falls inside declared range '[-20,20] hCmd in [-1,1] times aimRollGain (flown value 7.5)' -- not distinguishable from a real value
exit=1
```

All four injected defects were caught, and re-running the checker against the real,
unmodified `TAPE-SCHEMA.tsv` immediately after still returns GREEN / exit 0.

---

## What this file does not do

It does not touch, open for writing, or verify against
`D:/EvC2026_sandbox_cascade/src/client/BirdController.client.luau` — that file is owned by a
concurrent process this session and is out of scope. This schema is therefore a **specification**,
not yet a proof that the running build emits rows matching it. That proof is a separate,
later obligation under SOP-01 G6/G8/G9/G10 (header present, no gaps, no truncation, every row
well-formed) once M1 lands in the Luau source and a real tape exists to check the schema against.
