# Mandalark harness

> ## ⛔ READ FIRST — [SOP-01: PRIMARY DATA](SOP-01-PRIMARY-DATA.md) — CRITICAL CONTROL POINT
>
> **Chad's ruling, 2026-08-02. The number one imperative of this project. It outranks
> every other document in this repo, including this one.**
>
> **No agent may ask the Pilot to fly until [`gate.py`](../../mandalark-cascade-research/tools/gate.py)
> prints GREEN.** Not "should be fine." Not "the code has a print statement." Run it,
> paste the real output, every item GREEN, or the flight request is void.
>
> ```
> python D:/mandalark-cascade-research/tools/gate.py
> ```
>
> If you take away nothing else from this file: **that command, that file, that
> exit code — is the entire answer to "is it ready?"** Everything below explains
> what it's checking and why.

> **Fresh session? Read [`D:/mandalark-cascade-research/FOUNDATION.md`](../../mandalark-cascade-research/FOUNDATION.md)
> then [`HANDOFF.md`](../../mandalark-cascade-research/HANDOFF.md).** Handoffs on this project are
> written to [SOP-02](SOP-02-HANDOFF-STANDARDS.md): every claim is either reproducible by a
> command or labelled unverified. Both files carry their own reading list — anything not on it is
> archived, deliberately.

---

## The order of things in this directory, and why

This layout is not incidental — it *is* SOP-01's primacy, expressed as file
structure, not just as the first paragraph of a README. Read top to bottom:

| # | What | Where | Answers |
|---|---|---|---|
| 1 | **The law** | [`SOP-01-PRIMARY-DATA.md`](SOP-01-PRIMARY-DATA.md) | Why any of this exists. H1-H8 (harness contract), C1-C8 (scientific control), G1-G10 (the gate). Binding on every agent, every session. |
| 2 | **The gate** | [`../../mandalark-cascade-research/tools/gate.py`](../../mandalark-cascade-research/tools/gate.py) | *Is it ready?* Mechanically evaluates every item in #1 and in `PREFLIGHT-GATE.md`, prints GREEN/RED/RED-PENDING-HUMAN with evidence, exits 0 only if all GREEN. This is what an agent runs instead of judging. |
| 3 | **The schema** | [`TAPE-SCHEMA.tsv`](TAPE-SCHEMA.tsv) / [`TAPE-SCHEMA.md`](TAPE-SCHEMA.md) | *What does a row mean?* The 31-field deterministic `[EvCTAPE]` contract (H1). Checked by `check_schema.py`. |
| 4 | **The protocol** | [`EXPERIMENT-PROTOCOL.md`](EXPERIMENT-PROTOCOL.md) | *How is a comparison made honest?* Pre-registration (PREG-1), the tape format (TAPE-1), lifecycle (`new`/`lock`/`verify`), and how `compare_arms.py` enforces C1-C8. |
| 5 | **The tools** | `D:/mandalark-cascade-research/tools/` (`gate.py`, `check_schema.py`, `check_tape.py`, `compare_arms.py`, `preregister.py`) | The code that makes 1-4 mechanical instead of hoped-for. Lives in the sibling `mandalark-cascade-research` repo by prior file-scope convention; this directory is its documentation home. |
| 6 | **The state** | [`HARNESS-STATE.md`](HARNESS-STATE.md) | *What's actually true right now* — built / running / tested, as three separate, honestly-scored evidences, not one blended "done." |
| 7 | **The legacy material** | [`legacy-replay/`](legacy-replay/) | Pre-SOP-01 replay tooling. Some of it (SEADS side) is still live; the Roblox/EvC capture path is DEAD and marked as such at the top of every dead file. Read `legacy-replay/README.md` before touching anything in there. |

If you are a fresh agent and have ten seconds: **item 2 is the only one you
strictly need to act on.** Run `gate.py`. If it's not all-GREEN, no flight
happens, full stop — go fix whatever it says to fix, in the order it says.

---

## ⚠️ BASELINE WARNING — read before you trust any diff

*(Preserved from the pre-SOP-01 README; still true, still binding for anything
that touches the SEADS C++ kernel via `legacy-replay/replay_diff.py`.)*

- The kernel Chad is flight-testing is **`feel/kernel-v5`** in
  `D:\flight_sim2\seads-feel` — pushed to origin at commit **`89447aba5`**
  (rungs A–E: knife-edge / red-arrow / arcade-energy / hold-the-line).
- **`main` is still v4.** `feel/kernel-v5` is a **full generation ahead** of it
  and **diverges** from it until someone reconciles the two.
- So: **golden references must be recorded on `feel/kernel-v5 @ 89447aba5`.**
  Anything you baseline against `main` is a generation behind and will show
  "drift" that is really just v4-vs-v5. When you promote a golden, the notes
  sidecar stamps the baseline for you.

---

## Two projects, two data paths — don't cross them

**EvC2026 (Roblox, "Eagles vs Crows")** — governed by SOP-01 and
`PREFLIGHT-GATE.md`. Current, real sink: `print` → Studio's CreatorOutput log
→ read directly with `Select-String`/`gate.py`. No server, no port, no
`HttpEnabled`. The old HTTP-relay path (`capture_server.ps1` +
`FlightRecorder`) is **DEAD** — see `legacy-replay/README.md`.

**SEADS (C++, `flight_sim2/seads-feel`)** — a different codebase, its own
golden/replay layers, unaffected by any of the above. `legacy-replay/replay_diff.py`
Workflow A and `legacy-replay/seads_recorder_proposal/` are **live** for this side.
See `legacy-replay/README.md` for the file-by-file status.

---

## The three golden layers (SEADS side, don't confuse them)

1. **Plant golden** (`test/golden/golden_flight.h`) — a bit-exact trajectory from a
   *scripted* input. Gates the physics math. Moves only on purpose.
2. **Controller golden** (`test/golden/controller_golden.h`) — same, closed-loop.
3. **Felt-flight goldens** (`../goldens/*.seadsrec` / `*.csv`) —
   **Chad's real hand-flown runs.** Expected to be re-recorded when Chad
   blesses a new feel on a new kernel.

Layers 1–2 are owned by the seads-feel gate. This harness owns layer 3, via
`legacy-replay/replay_diff.py`.

---

## Quick reference — the commands that matter today

```powershell
# Is it ready to fly? (SOP-01's single mechanical answer)
python D:/mandalark-cascade-research/tools/gate.py

# Validate the tape schema on its own
python D:/mandalark-cascade-research/tools/check_schema.py D:/mandalark-kernel/harness/TAPE-SCHEMA.tsv

# Check a captured tape (Studio log) against the harness contract (H1-H4,H7)
python D:/mandalark-cascade-research/tools/check_tape.py <path-to-studio-log>

# Pre-register an experiment (C3), then lock it
python D:/mandalark-cascade-research/tools/preregister.py new <EXPERIMENT_ID>
python D:/mandalark-cascade-research/tools/preregister.py lock <path-to-toml>

# Compare two arms against a locked pre-registration (C2, C3, C4, C6)
python D:/mandalark-cascade-research/tools/compare_arms.py --prereg <toml> --control <tape.csv> --treatment <tape.csv>
```

SEADS-side commands (unaffected by SOP-01, unchanged from before) are in
`legacy-replay/README.md`.
