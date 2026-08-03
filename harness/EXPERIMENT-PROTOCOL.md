# EXPERIMENT-PROTOCOL — mechanical form of SOP-01's SCIENTIFIC CONTROL (C1–C8)

**Status:** BINDING, subordinate only to `SOP-01-PRIMARY-DATA.md`. Implements C1–C8 as code, not
prose. Every clause below is enforced by a command in
`D:/mandalark-cascade-research/tools/preregister.py` or
`D:/mandalark-cascade-research/tools/compare_arms.py` — not by an agent's care.

---

## 1. THE PRE-REGISTRATION FILE FORMAT — "PREG-1"

A pre-registration is a TOML file. One file, one experiment, one variable. The template is
produced by `preregister.py new` and is only a *draft* until `preregister.py lock` freezes it.

```toml
schema = "prereg-v1"

[meta]
experiment_id = "EXP-TEST01"
author = "harness-test"
hypothesis = "Enabling aimPushMode reduces max unwanted roll (phiInt) during a straight-down knife-edge deflection."

[variable]                      # the ONE thing this experiment changes (C2)
name = "aimPushMode"
control_value = "false"
treatment_value = "true"

[arms.control]
config = { aimPushMode = "false" }

[arms.treatment]
config = { aimPushMode = "true" }

[statistic]                     # the statistic and the threshold (C3)
field = "phiInt"                # tape column name
metric = "max_abs"              # mean | mean_abs | median | median_abs | max_abs
threshold = 5.0                 # |treatment - control| >= threshold => "a difference"
unit = "deg"

[sample]                        # the sample-size floor (C6)
floor_n = 12
population = "episode"          # what one unit of n is: one row = one episode

[order]                         # C5 — blind arm order, recorded, not announced
sequence = ["control", "treatment"]
blind = true
```

Every field named in C1–C8's requirements has exactly one home in this file:

| Requirement | Field |
|---|---|
| hypothesis | `meta.hypothesis` |
| the ONE variable changed (C2) | `variable.name` / `.control_value` / `.treatment_value` |
| control arm config | `arms.control.config` |
| treatment arm config | `arms.treatment.config` |
| the statistic (C3) | `statistic.field` + `statistic.metric` |
| the threshold that counts as a difference (C3) | `statistic.threshold` |
| the sample-size floor (C6) | `sample.floor_n` |
| the arm order, blind (C5) | `order.sequence`, `order.blind` |

---

## 2. THE LIFECYCLE — `preregister.py`

```
python preregister.py new <experiment_id> [--dir DIR]
```
Writes an unfilled PREG-1 template to `DIR/<experiment_id>.toml` (default
`tools/preregistry/`). A fresh template has **no lock file** and is not yet usable by anything —
this is deliberate: it forces the hypothesis/statistic/threshold to be written down as a distinct
act from filling in the numbers later.

```
python preregister.py lock <path>
```
Validates every required field is non-empty (hypothesis, variable name/values, statistic
field/metric, floor_n ≥ 1). Refuses (exit 2) if anything is blank — an agent cannot lock a
half-filled template and come back to finish it after seeing data. On success, computes
`sha256(file bytes)`, stamps the current UTC time, and writes `<path>.lock`:
```
sha256=<hex>
locked_utc=<ISO8601Z>
file=<basename>
```
**A registration can be locked exactly once.** Re-locking an already-locked file is refused
unless `--force` is passed explicitly — because re-locking is exactly the mechanism by which a
threshold could be quietly moved after seeing data (C3's core prohibition).

```
python preregister.py verify <path>
```
Recomputes the file's hash and compares it to the `.lock` sidecar. Prints `OK sha256=... 
locked_utc=...` or refuses with `TAMPERED` / `NOT LOCKED`. `compare_arms.py` calls this before
trusting any registration — no separate "trust me" path exists.

**Why hash + timestamp, and not a database or a signature:** the hash makes any edit to the
registration after locking detectable without needing a server, an account, or trust in the
editing agent. The timestamp makes "was this written before or after the data" a plain
comparison, not a memory. Both are checked by code, every time, in `compare_arms.py` — never by
asking whoever ran the experiment.

---

## 3. THE TAPE FORMAT — "TAPE-1"

`compare_arms.py` reads two tapes (control, treatment). Format:

```
# HEADER build=<hash> collected_utc=<ISO8601Z> arm=<control|treatment> <cfgkey>=<val> ...
<csv column header row>
<csv data rows, one row per unit of analysis>
```

- Line 1 is the self-identifying header (SOP-01 H7): build hash, collection timestamp, which arm
  this tape is, and every config key in force. `collected_utc` and `arm` are reserved; every other
  `key=value` on the header line is a config value subject to the C2 check below.
- `collected_utc` is the C3 anchor: it must be at or after the registration's `locked_utc`, or the
  tape is refused outright.
- The remaining lines are an ordinary CSV. One row = one unit of `sample.population` (an episode,
  by default) — not one frame. Per-frame tape rows (the `[EvCTAPE]` 31-field record) are reduced
  to one row per episode by whatever upstream reduction step the experiment card names, before
  they reach `compare_arms.py`. `compare_arms.py` does not do that reduction itself; it consumes
  its output and is not a place to hide an assumption.

---

## 4. `compare_arms.py` — the comparison IS the result (C4)

```
python compare_arms.py --prereg <path.toml> --control <control.csv> --treatment <treatment.csv>
```

Order of checks, each a hard refusal (exit 2, no statistic printed) on failure:

1. **C3 — registration must be locked and unmodified.** Calls `verify_registration()`. Tampered
   or unlocked → refused before either tape is even opened.
2. **C3 — data must postdate the lock.** Both tapes' `collected_utc` must be ≥ the registration's
   `locked_utc`. Earlier → `REFUSED: DATA PREDATES PREREGISTRATION`.
3. **C2 — exactly one config value may differ.** Every header key (excluding `build`,
   `collected_utc`, `arm`) is diffed between the two tapes. Zero differences → refused (the
   registered variable didn't move). Two or more → `REFUSED: C2 VIOLATION` naming every key that
   differs. Exactly one, but it isn't the registered variable → refused as an unregistered
   variable change. This is the mechanical form of the project's S28 SOP: **the tool reads the
   headers, it does not take an agent's word that only one thing changed.**
4. **C6 — the sample-size floor.** Only past this point are the two tapes reduced to a statistic.
   If either arm's n is below `sample.floor_n`, the tool prints exactly:
   ```
   NOT ESTABLISHED n=<min(n_control, n_treatment)> floor=<floor_n>
   ```
   and nothing else is offered as a finding. This is not a warning attached to a number — no
   difference, no direction, no trend is printed alongside it.
5. **C4 — the comparison is the result.** Above the floor, the tool prints **both** arm values
   with their n and the signed difference in one line — it has no code path that prints an
   absolute number without its paired control value beside it. It then states
   `DIFFERENCE ESTABLISHED` or `NO DIFFERENCE` against the pre-registered threshold — never a
   qualitative read of the raw numbers.

---

## 5. RUN LOG — refusal tests against synthetic tapes with known ground truth

All commands run for real with `/c/Users/Chad/AppData/Local/hermes/hermes-agent/venv/Scripts/python`
against `D:/mandalark-cascade-research/tools/`. Fixtures generated by
`tools/testdata/make_tapes.py` (seeded RNG, so ground truth is known):

| Case | control center | treatment center | n/arm | config diff | expected |
|---|---|---|---|---|---|
| A | 30° | 10° | 15/15 (≥ floor 12) | 1 (aimPushMode) | DIFFERENCE ESTABLISHED |
| B | 30° | 10° | 5/5 (< floor 12) | 1 (aimPushMode) | NOT ESTABLISHED n=5 floor=12 |
| C | 30° | 10° | 15/15 | **2** (aimPushMode + aimRollGain) | REFUSED, C2 VIOLATION |
| D | 30° | 10° | 15/15 | 1 | REFUSED, control tape predates registration lock |
| E | 12° | 12.5° | 15/15 (≥ floor) | 1 | NO DIFFERENCE (below threshold) |

### 5.0 — refuse to lock an unfilled template

```
$ python preregister.py new EXP-TEST01 --force
template written: D:\mandalark-cascade-research\tools\preregistry\EXP-TEST01.toml

$ python preregister.py lock preregistry/EXP-TEST01.toml
REFUSED: registration is not filled in:
  - empty required field: meta.hypothesis
  - empty required field: variable.control_value
  - empty required field: variable.treatment_value
  - empty required field: statistic.metric
```
(exit 2)

### 5.1 — lock the filled-in registration, verify, then prove tamper detection

```
$ python preregister.py lock preregistry/EXP-TEST01.toml
LOCKED: preregistry\EXP-TEST01.toml
  sha256=6a7262d87daec7cf002f69cd1cb0ef67c0177b41bb19f8d1003631ca668eff95
  locked_utc=2026-08-02T19:51:28Z
  lock file: preregistry\EXP-TEST01.toml.lock

$ python preregister.py verify preregistry/EXP-TEST01.toml
OK sha256=6a7262d87daec7cf002f69cd1cb0ef67c0177b41bb19f8d1003631ca668eff95 locked_utc=2026-08-02T19:51:28Z

$ echo "# tampered" >> preregistry/EXP-TEST01.toml
$ python preregister.py verify preregistry/EXP-TEST01.toml
REFUSED: TAMPERED: file hash does not match lock record (recorded=6a7262d8...eff95 actual=3e8dd194...4d4)
```
(restored the file afterwards; `verify` returns OK again)

### 5.2 — analysis refuses against a registration that was never locked

```
$ python preregister.py new EXP-UNLOCKED --force
$ python compare_arms.py --prereg preregistry/EXP-UNLOCKED.toml \
    --control testdata/caseA_control.csv --treatment testdata/caseA_treatment.csv
REFUSED: NOT LOCKED: no lock file preregistry\EXP-UNLOCKED.toml.lock
```
(exit 2)

### 5.3 — CASE A: above floor, real difference → ESTABLISHED

```
$ python compare_arms.py --prereg preregistry/EXP-TEST01.toml \
    --control testdata/caseA_control.csv --treatment testdata/caseA_treatment.csv
experiment: EXP-TEST01
hypothesis: Enabling aimPushMode reduces max unwanted roll (phiInt) during a straight-down knife-edge deflection.
variable:   aimPushMode (control=false treatment=true)
statistic:  max_abs(phiInt) [deg]  threshold=5.0
n:          control=15 treatment=15 floor=12
control=32.3530 (n=15)  treatment=12.7430 (n=15)  diff=-19.6100
DIFFERENCE ESTABLISHED: |diff|=19.6100 >= threshold=5.0
```
(exit 0)

### 5.4 — CASE B: below floor → refuses to conclude, exactly per C6

```
$ python compare_arms.py --prereg preregistry/EXP-TEST01.toml \
    --control testdata/caseB_control.csv --treatment testdata/caseB_treatment.csv
n:          control=5 treatment=5 floor=12
NOT ESTABLISHED n=5 floor=12
```
(exit 0 — "not established" is a valid, honestly reported result, not an error)

### 5.5 — CASE C: two config values changed → C2 refusal

```
$ python compare_arms.py --prereg preregistry/EXP-TEST01.toml \
    --control testdata/caseC_control.csv --treatment testdata/caseC_treatment.csv
REFUSED: C2 VIOLATION — 2 config values differ between arms (aimPushMode, aimRollGain); one change per flight only (SOP-01 C2 / project S28).
```
(exit 2 — no statistic printed at all)

### 5.6 — CASE D: data collected before the registration lock → C3 refusal

```
$ python compare_arms.py --prereg preregistry/EXP-TEST01.toml \
    --control testdata/caseD_control.csv --treatment testdata/caseD_treatment.csv
REFUSED: DATA PREDATES PREREGISTRATION — control tape collected_utc=2026-08-02T10:00:00Z is before registration locked_utc=2026-08-02T19:51:28Z. A threshold chosen after seeing data is not a result (C3).
```
(exit 2)

### 5.7 — CASE E: above floor, true difference below threshold → NO DIFFERENCE

```
$ python compare_arms.py --prereg preregistry/EXP-TEST01.toml \
    --control testdata/caseE_control.csv --treatment testdata/caseE_treatment.csv
control=14.7230 (n=15)  treatment=15.0750 (n=15)  diff=+0.3520
NO DIFFERENCE: |diff|=0.3520 < threshold=5.0
```
(exit 0 — a null result, reported as one, never softened and never omitted)

All five cases match their known ground truth exactly. The two REFUSED cases (C, D) never reach
the point of computing a statistic — the tool does not compute-then-hide a number, it refuses
before the computation happens.

---

## 6. HOW THIS MAPS BACK TO C1–C8

| Clause | Enforced by |
|---|---|
| C1 — every experiment has a control arm | PREG-1 requires `arms.control` and `arms.treatment` both present; `compare_arms.py` requires two tape arguments, always |
| C2 — one change per flight | `compare_arms.py` header diff, hard refusal on >1 differing key (§4.3) |
| C3 — pre-registration before the flight | `preregister.py lock` hash+timestamp; `compare_arms.py` refuses tapes with `collected_utc` before `locked_utc` (§4.1–4.2) |
| C4 — the comparison is the result | `compare_arms.py` has no output path that prints one arm's number without the other (§4.5) |
| C5 — blind where possible | `order.sequence` / `order.blind` recorded in the registration before the flight, not derived from or announced to the pilot |
| C6 — below the floor is a result | `NOT ESTABLISHED n=<x> floor=<y>`, exact wording, printed instead of any statistic (§4.4) |
| C7 — analysis may not share a bug with the code it measures | `compare_arms.py` recomputes `phiH`/`phiInt`-class statistics from the tape's own columns using stdlib `statistics`, independent of the flight code's internal computation — it does not import or call anything from `BirdController.client.luau` |
| C8 — pilot report vs instrument, instrument loses | outside this tool's scope by design — recorded as a standing rule in `SOP-01-PRIMARY-DATA.md`; this protocol only guarantees the instrument's own number is never asserted without n and a control |

---

## 7. FILES

- `D:/mandalark-kernel/harness/EXPERIMENT-PROTOCOL.md` — this document
- `D:/mandalark-cascade-research/tools/preregister.py` — lifecycle: `new`, `lock`, `verify`
- `D:/mandalark-cascade-research/tools/compare_arms.py` — the comparison tool
- `D:/mandalark-cascade-research/tools/preregistry/` — registrations live here, `<id>.toml` +
  `<id>.toml.lock`
- `D:/mandalark-cascade-research/tools/testdata/make_tapes.py` — synthetic fixture generator used
  for §5's proof run; re-runnable, seeded, ground truth stated in-file
