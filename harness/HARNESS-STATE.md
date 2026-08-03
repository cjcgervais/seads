# HARNESS-STATE — built / running / tested, honestly, separately

SOP-01's standing bar: *"Built, running and tested are three separate things
and each needs its own evidence: built = the code exists and passes static
checking; running = it produced real rows from the real build, read back off
disk this session; tested = fault injection proved every detector sees a
known event, and the analysis tooling was run end to end on that fixture."*

This document is that evidence, component by component, as of **2026-08-02**,
against `python D:/mandalark-cascade-research/tools/gate.py` run this session
(full output in that command's own transcript; summarised per-item below).
**Anything not demonstrated says NOT DEMONSTRATED — never "should work."**

---

## 1. The gate runner itself (`gate.py`)

| | Status |
|---|---|
| **Built** | YES — `D:/mandalark-cascade-research/tools/gate.py`, 25 items across 5 sections, run this session. |
| **Running** | YES — produced real GREEN/RED/PENDING output against the real repo state this session (see transcript below). |
| **Tested** | PARTIAL — its own sub-checks are fault-injection tested (A2 mutates the schema and confirms detection; A4 runs a 7-fault synthetic bad tape). `gate.py`'s top-level table logic itself (render/exit-code) has no separate unit test — it was exercised by observing it correctly go RED on a real run, not by a synthetic gate-of-the-gate. **NOT DEMONSTRATED**: an automated check that `gate.py` exits 1 whenever any item is non-GREEN (verified by inspection of `render()`, not by a test). |

## 2. Tape schema (`TAPE-SCHEMA.tsv` / `.md` + `check_schema.py`)

| | Status |
|---|---|
| **Built** | YES — 31-field schema, `EvCTAPE-1.0.0`, all `ord`/`precision`/`range`/`sentinel`/`source_ref` filled. |
| **Running** | YES — `check_schema.py` runs against the real TSV, real output, exit 0, this session (gate item A1). |
| **Tested** | YES — fault-injection: duplicate ord, deleted ord (gap), blanked source_ref, sentinel-inside-range all independently confirmed caught (gate item A2 re-runs the duplicate-ord case live every `gate.py` run; the other three fault classes were proven in the session that built the schema and are not re-run automatically — **NOT DEMONSTRATED THIS SESSION** for those three specific classes, only re-verified for the duplicate-ord class). |

## 3. Tape contract checker (`check_tape.py` + `evctape_schema.json`)

| | Status |
|---|---|
| **Built** | YES — H1/H2/H3/H4/H7 + CONST, stdlib-only. |
| **Running** | YES — ran against a real synthetic tape this session (gate items A3/A4), real exit codes. |
| **Tested** | YES — proven both directions this session: GREEN on a known-good synthetic tape (A3, 6/6 checks pass), RED on a known-bad synthetic tape with 7 independently-injected faults (A4, 6/6 checks correctly fail, every fault class named in the output). **NOT DEMONSTRATED**: `check_tape.py` has never yet been run against a *real* Studio log carrying real `[EvCTAPE]` rows — because none exists yet (see §6). Its correctness against synthetic fixtures does not by itself prove it parses the real emitter's real output; that is a distinct, still-open check. |

## 4. Pre-registration (`preregister.py`)

| | Status |
|---|---|
| **Built** | YES — `new` / `lock` / `verify`, PREG-1 format. |
| **Running** | YES — ran `new`/`lock`/`verify` this session in a scratch dir (gate item A6), real output. |
| **Tested** | YES — this session confirmed: `lock` refuses an unfilled template (exit nonzero, named missing fields); `verify` detects a one-line tamper after locking and reports `TAMPERED` with mismatched hashes. |

## 5. Arm comparison (`compare_arms.py`)

| | Status |
|---|---|
| **Built** | YES — enforces C2 (one variable), C3 (no data before lock), C4 (report the diff, not an absolute number), C6 (sample floor). |
| **Running** | YES — ran all 5 fixture cases this session (gate item A5), real output. |
| **Tested** | YES — 5 cases, 5 known ground truths, all matched exactly this session: above-floor real difference → ESTABLISHED; below-floor → `NOT ESTABLISHED n=5 floor=12`; two config values changed → `REFUSED: C2 VIOLATION`; data predates registration → `REFUSED: DATA PREDATES PREREGISTRATION`; above-floor null → `NO DIFFERENCE`. |

## 6. The actual flight instrument — `[EvCTAPE]` in `BirdController.client.luau` (M1-M7)

| | Status |
|---|---|
| **Built** | **NOT DEMONSTRATED.** This session did not open `BirdController.client.luau` (read or write) — a concurrent process owns it, and file scope forbade touching it. Whether M1 (the ungated per-frame emitter), M2 (roll-term decomposition), M3 (pole-safe roll), M4 (ladder sequencer), M5 (pole-bitfield fix), M6 (A/B flag + delete the lying print) have landed is **unknown to this session**, not merely unverified. |
| **Running** | **NO, confirmed.** Gate item G7: the latest Studio log (`0.732.0.7321043_20260802T184344Z_Studio_40303_last.log`, 455018 bytes, actively growing — G4 GREEN) contains **zero** `[EvCTAPE] r,` rows. This is a real, current, negative result, not an absence of a check. |
| **Tested** | **NO** — cannot be, until Running is YES. G8-G12, G14, G15 are all RED-PENDING-HUMAN for exactly this reason and print the exact fix (fly the G7 smoke test once M1 lands). |

## 7. Live pre-flight items requiring a human / a Studio session (SOP-01 G1-G10, PREFLIGHT-GATE G1-G17)

All **RED-PENDING-HUMAN**, honestly, because they require either a real
sortie or scope this session did not have:

- **G1 build identity, G3 headless floor, G13 HTTP-free source scan** — require reading/running inside `D:/EvC2026_sandbox_cascade`, which this session's file scope forbids (a concurrent process owns `BirdController.client.luau`). `gate.py --allow-evc-touch` implements these fully for a session that has that scope; not run here by design, not by oversight.
- **G7 end-to-end smoke, G8 no-drops, G9 no-truncation, G10 well-formed rows, G11 heartbeat, G6 header, G14 pole fault injection, G15 ladder dry-run** — all require `[EvCTAPE]` rows to exist first (§6). Zero today.
- **C1-C8 production pre-registration** — the *mechanism* is built and tested (§4-§5), but no production `EXP-*` registration is locked for an actual upcoming flight; only the `EXP-TEST01` self-test fixture is locked. **NOT DEMONSTRATED** that a real experiment is ready to compare.
- **C5 blind arm order actually kept blind** — inherently a human-conduct fact about a specific sortie; not machine-checkable ever, only auditable after the fact from the locked registration + tape headers.

---

## The honest one-line summary

**The enforcement machinery (schema, tape contract, pre-registration, arm
comparison, and the gate runner that ties them together) is built, running,
and tested against synthetic fixtures with proven fault injection in both
directions.** **The thing it exists to gate — a real `[EvCTAPE]` row from the
real running EvC2026 build — does not exist yet.** `gate.py` says this
itself, correctly, as RED. That is not a partial pass. Per SOP-01 CCP-1, no
flight may be requested until it says otherwise.

Re-run to get the current state at any time:
```
python D:/mandalark-cascade-research/tools/gate.py
```
