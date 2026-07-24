# Flight-feel replay harness

This is Chad's toolkit for **recording a test flight and diffing it later**, so a
kernel change can be checked against how the flying actually felt — not just
whether the math tests pass.

There are TWO flying games this harness serves. Read the one you're testing.

---

## ⚠️ BASELINE WARNING — read before you trust any diff

- The kernel Chad is flight-testing is **`feel/kernel-v5`** in
  `D:\flight_sim2\seads-feel` — pushed to origin at commit **`89447aba5`**
  (rungs A–E: knife-edge / red-arrow / arcade-energy / hold-the-line).
- **`main` is still v4.** `feel/kernel-v5` is a **full generation ahead** of it and
  **diverges** from it until someone reconciles the two.
- So: **golden references must be recorded on `feel/kernel-v5 @ 89447aba5`.**
  Anything you baseline against `main` is a generation behind and will show
  "drift" that is really just v4-vs-v5. When you promote a golden, the notes
  sidecar stamps the baseline for you.

---

## What's in here

| File | What it is |
|---|---|
| `replay_diff.py` | The diff/report tool. Compares two captures, prints a feel-drift report + JSON. Reads BOTH the C++ (SEADS) CSVs and the Roblox (EvC) JSONL. Has a `promote` command to bless a golden. |
| `capture_server.ps1` | A tiny local web server that catches flight data from the Roblox game and writes it to disk. (SEADS writes its own CSVs directly — no server needed there.) |
| `seads_recorder_proposal/` | A drop-in C++ recorder (`recorder.h`) + integration note + a test, for the seads-feel session to graft in so Chad's hand-flown SEADS flights can be recorded and replayed. |
| `INSTALL.md` | The 2–4 lines to wire the Roblox recorder in (EvC side). |
| `REPLAY_ROADMAP.md` | What's needed to do true input-replay on the Roblox side (its kernel is more entangled than SEADS'). |
| `../captures/` | Where raw recordings land. |
| `../goldens/` | Where blessed reference flights live (with a `.notes.json` feel verdict each). |

---

## The three golden layers (don't confuse them)

For the SEADS kernel there are now **three** kinds of reference data:

1. **Plant golden** (`test/golden/golden_flight.h`) — a bit-exact trajectory from a
   *scripted* input. Gates the physics math. Moves only on purpose.
2. **Controller golden** (`test/golden/controller_golden.h`) — same, closed-loop.
3. **Felt-flight goldens** (this harness, `../goldens/*.seadsrec` / `*.csv`) —
   **Chad's real hand-flown runs.** These are the new layer. They're *expected* to
   be re-recorded when Chad blesses a new feel on a new kernel.

Layers 1–2 already exist and are owned by the seads-feel gate. This harness owns
layer 3.

---

## Workflow A — SEADS C++ kernel (the primary one tonight)

The SEADS harness already writes CSVs directly (no server). Its probe fleet
(`track`, `lathold`, `latflick`, `step`, `nudge`, `ctrl_fly`) and this tool all
use the **same column names**, so live flights and synthetic probes read on one
scale.

1. **Bless a golden from the current kernel.** Fly (or run `ctrl_fly` / `lathold`),
   which writes a CSV, then:
   ```
   python replay_diff.py promote path\to\flight.csv v5-rungE-baseline --notes "fly-4 verdict: knife-edge fixed, snappy to centre"
   ```
   (records it into `..\goldens\` with a baseline-stamped notes file.)

2. **After a future kernel change**, re-run the same probe/flight to a new CSV, then
   diff it against the golden:
   ```
   python replay_diff.py diff ..\goldens\v5-rungE-baseline.csv path\to\new_flight.csv --json drift.json
   ```
   The report shows per-maneuver drift in nose-vs-aim error, bank, AoA, speed,
   energy, load factor, and **push_gate commitment timing** (the rung-E surface),
   plus a lathold-style summary (t_capture, alt_loss, alpha_p95, windmill).

3. **True input-replay** (record a hand flight, replay it bit-for-bit through the
   kernel): that needs the `seads_recorder_proposal/` grafted in — hand it to the
   seads-feel session. See that folder's `INTEGRATION.md`.

---

## Workflow B — Roblox "Eagles vs Crows" (secondary)

1. **Start the capture server** (leave this window open the whole session):
   ```
   powershell -ExecutionPolicy Bypass -File D:\mandalark-kernel\harness\capture_server.ps1
   ```
   It prints a live line per batch: frames received, duration, last speed.

2. **Wire the recorder** into the game once — see `INSTALL.md` (2–4 pasted lines) —
   and make sure **HttpService is enabled** in Studio (Game Settings → Security →
   Allow HTTP Requests). *Note: Roblox only lets the SERVER send HTTP, so the
   recorder is started from a server Script; it records the flown bird's replicated
   trajectory. Raw mouse input needs the optional client feed — see INSTALL.md.*

3. **Fly your test flight.** Watch the server window count frames. Stop with
   **Ctrl+C** when done. Your flight is now a `.jsonl` file in `..\captures\`.

4. **Bless it as golden:**
   ```
   python replay_diff.py promote ..\captures\<the-file>.jsonl evc-v5-baseline --notes "your feel verdict"
   ```

5. **After a kernel change**, capture again and diff:
   ```
   python replay_diff.py diff ..\goldens\evc-v5-baseline.jsonl ..\captures\<new>.jsonl
   ```

---

## Quick reference — the commands

```
# see what a capture contains
python replay_diff.py summary <capture>

# compare two captures (golden first)
python replay_diff.py diff <golden> <candidate> [--json out.json] [--align tick|time]

# bless a capture as a golden reference
python replay_diff.py promote <capture> <name> --notes "feel verdict"

# start the Roblox capture server (EvC only)
powershell -ExecutionPolicy Bypass -File capture_server.ps1 [-Port 8790]
```

`--align tick` (default) pairs row-by-row — correct for two fixed-dt tick streams
from the same start. `--align time` pairs by nearest timestamp — use it when the
two captures have different frame rates or lengths.
