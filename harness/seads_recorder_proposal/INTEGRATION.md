# SEADS felt-flight recorder — integration proposal

**Status:** drop-in proposal. Nothing here is grafted. Hand it to the seads-feel
session to review as a kernel-firewall diff and wire in.

**What it does:** captures one of Chad's hand-flown test flights as a tick-indexed
input stream (`.seadsrec`), so the same flight can be replayed bit-for-bit through
the kernel after a future change and diffed for feel drift.

Files in this proposal:
- `recorder.h` — header-only recorder + replayer + `.seadsrec` (de)serializer with
  an fnv1a content signature. Written to drop in at `test/harness/recorder.h`.

---

## 1. Kernel-firewall compliance (read this first — it makes the review trivial)

The recorder is **app/render-side only**. It never enters `sim/` or `control/`.

| Concern | Compliance |
|---|---|
| Where the file lives | `test/harness/` (the app/harness side, next to `telemetry.h`, `injector.h`) |
| Where the hook sits | inside `app::step_frame` (declared in `app/instructor_tick.h`) — the **fixed-dt accumulator seam**, app namespace |
| What it reads | `app::TickInput` (input going *in*) and `app::LoopState`/`sim::SimState` (telemetry coming *out*) — as **plain data** |
| What it writes | a text file. It mutates **no** engine state |
| Directories touched | `app/` (one call-site line), `test/harness/` (the new header), `app/main.cpp` (owning the recorder object + a flush) |
| Directories NOT touched | **`sim/` and `control/` are untouched** — no instrumentation crosses the firewall |

The hook taps inputs at the accumulator boundary and state at the tick output. It
is the app tapping its own seam, exactly the "render/app-side only" rule. The
recorder `#include`s `sim/state.h` and `sim/params.h` **only to name the types it
reads** — it calls no sim/control function that could perturb a trajectory.

---

## 2. The seam (why tick-level, and exactly where)

`app::step_frame(LoopState& st, Accumulator& accum, frame_dt, FrameInput, pending_dx,
pending_dy, ap, cp)` in `app/instructor_tick.h` is the per-frame loop. It calls
`accum.advance(frame_dt)` to get a whole-tick count, then runs that many
`app::tick(st, in, ap, cp, dw)` calls — the plant never sees `frame_dt`. That
frame-rate independence is the whole point of AT-9 (`test/unit/test_at9.cpp`):
**identical tick-indexed input ⇒ bit-identical trajectory at any fps.**

So the recorder captures the resolved `TickInput in` **per sim-tick**, at the
`tick(...)` call site inside the `for (t < res.ticks)` loop. Recording mouse
deltas per *render frame* instead (what the Roblox recorder is forced to do,
lacking a fixed-dt seam) would replay differently across frame rates — the exact
divergence AT-9 forbids.

**Post-curve note:** `aim_dx/aim_dy` on the consuming tick are already
POST-curve — the rate-keyed sensitivity from `input/aim_curve.h` is applied
upstream at the device accrual in `main.cpp`, before `pending_dx/dy`
(`test_at9.cpp` header, "MB-aim NOTE"). Recording the consumed-tick value makes
the stream **curve-independent**: a replay reproduces the flight without
re-deriving the curve. This mirrors how `test_at9.cpp` injects post-curve deltas.

---

## 3. Record hook (three lines, app-side)

In `app/main.cpp` own a recorder and arm it (e.g. behind a `--record out.seadsrec`
flag or a keybind), then flush on quit:

```cpp
#include "test/harness/recorder.h"
seads_replay::Recorder rec;              // in the app object / main scope
// ... on quit / when recording stops:
rec.flush("flight_v5-rungE.seadsrec", "v5-rungE@89447aba5");
```

The per-tick capture belongs **inside** `step_frame`'s tick loop so it sees every
tick (a mid-frame respawn neutralizes `cur` in place — the recorder captures the
*neutralized* input, which is what actually flew). Two options:

**(a) minimal, inside `step_frame`** — pass an optional recorder pointer and, right
after `const TickResult r = tick(st, in, ap, cp, dw);`, add:

```cpp
if (recorder) recorder->on_tick(in, st);   // taps the resolved input + post-tick state
```

`on_tick` stores a `TickRecord` (all `TickInput` fields + a `SimState` pin). This
is the single line that crosses into `app/instructor_tick.h`; it reads only, and
is null-guarded so the gate/goldens/mirror (which pass no recorder) are
bit-identical — a strict superset, the same discipline the codebase uses for
`flap_cmd`/`aim_gain_scale` defaults.

**(b) zero-touch to `instructor_tick.h`** — if the firewall review prefers NOT to
add even a read-only line inside `step_frame`, drive the recording loop from
`main.cpp` by calling `accum.advance()` + `app::tick()` directly (the same
composition `test_at9.cpp::run_at` and the AT-9 mirror already use) and calling
`rec.on_tick(in, st)` there. Then `step_frame` is untouched entirely and the tap
lives 100% in `main.cpp`. Recommended for the cleanest firewall diff.

---

## 4. Replay hook (verification + felt-flight golden)

Replay feeds the recorded `TickInput`s straight into `app::tick`, one per record,
bypassing the accumulator (the ticks are already the fixed-dt sequence):

```cpp
std::vector<seads_replay::TickRecord> recs;
std::string tag; bool sig_ok = false;
seads_replay::read_records("flight_v5-rungE.seadsrec", recs, &tag, &sig_ok);
REQUIRE(sig_ok);                                  // tamper/alias guard

app::LoopState st = /* the recorded start state (pin it in the file header or a
                       sibling golden_start-style fixture) */;
for (const auto& rec : recs) {
    app::TickInput in = seads_replay::to_tick_input(rec);
    app::tick(st, in, ap, cp);
    // DETERMINISM PROOF: bit-compare against the pin recorded that tick
    CHECK(st.curr.position    == rec.pin_position);
    CHECK(st.curr.orientation == rec.pin_orientation);
    // ... velocity, angular_vel, throttle
}
```

- On the **same** kernel build, every pin matches bit-for-bit (an AT-9-style
  ctest leg: `test_replay.cpp`, a natural companion to `test_at9.cpp`).
- On a **changed** kernel, the pins diverge — that divergence *is* the felt-flight
  drift. Emit it as CSV using the harness's existing `CtrlCsvTelemetry` columns
  (`telemetry.h`) so the replayed flight reads on the SAME scale as `ctrl_fly` and
  the synthetic probes, then feed both CSVs to `../replay_diff.py`.

---

## 5. Third golden layer (say this to the reviewer)

These recordings are a **third** golden layer, NOT a replacement for the two
bit-level pins:

1. `test/golden/golden_flight.h` — plant trajectory pin (scripted `golden_input`)
2. `test/golden/controller_golden.h` — closed-loop controller pin
3. **felt-flight recordings** (`*.seadsrec`) — Chad's real hand-flown runs

Layers 1–2 are synthetic scripts that gate the math and MUST move deliberately.
Layer 3 pins how the kernel answers a REAL human's stick, carries an fnv1a content
signature (the same tamper-loud discipline the goldens use), and — unlike 1–2 — is
EXPECTED to be re-recorded whenever Chad re-flies a new kernel to bless a new feel
reference. A moved layer-3 golden is a feel decision (Chad's stick), not a math
regression.

---

## 6. Baseline note

Pin recordings/baselines to `feel/kernel-v5 @ 89447aba5` (pushed to origin). That
branch is a full generation **ahead of `main` (v4)** and diverges from it until
reconciliation — anything diffed against `main` is a generation behind. See
`../README.md`.
