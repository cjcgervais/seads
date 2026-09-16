# R4a STAGE 2 — THE RELEASE ITSELF (launch doc, 2026-08-31)

**Work in `D:\seads_sandboxes\r4a-superman`, branch `sandbox/r4a-grip`.**
Base `f1388b5f1`, NOT pushed. Gate **1741/1747**.

> ⚠⚠ **DO NOT WORK IN `D:\flight_sim2\seads-recon`.** That is Chad's FLY TREE — the
> tree he opens and drives. Sandbox branches run in `D:\seads_sandboxes\`, one dir per
> thread. You may READ the tapes there (`sled_tape_*.sledtape`); never write, never run
> git in it.

---

## §1 CHAD DROVE STAGE 1, AND "I DID NOT FALL OFF" IS THE PASS

His words, 2026-08-31: *"I rode this last night and did not fall off."*

**That is the correct outcome and it is the certification stage 1 needed**, on both
counts:

1. **The release cannot fire.** `grip.capacity` ships at 1e30. A drive in which he came
   off would have meant something was wired that should not be.
2. **The arming memory he signed on 2026-08-30 still feels like itself** after being
   moved out of `render/body_drive.*` down into `sim/rider_grip.*`. That move was the
   one thing in stage 1 that could have changed the feel, the diff was checked
   float-for-float, and now his stick agrees. **The move is closed.**

★ So `buck_gain 0.02` / `decay_per_s 3.0` remain SIGNED AND FROZEN, and are now signed
in their new home. `SEADS_BUCK_GAIN=0` is still the bit-identical kill switch.

**What he has NOT yet been given, and what this rung owes him: coming off.**

---

## §2 WHAT IS BUILT (do not rebuild it, do not re-derive it)

`sim/rider_grip.{h,cpp}` — the grip law, pure, testable without a machine:

- the arming memory (`BuckMemory`/`buck_step`), **moved verbatim**, re-exported from
  `render/body_drive.h` under its old names. ONE law; three callers (drawn body,
  kernel, probe). ⚠ Shared function is **not** shared history — render steps it once
  per FRAME, the kernel every SUBSTEP. Do not build anything that needs them to agree
  sample-for-sample.
- `GripState` on `SledState`: memory, scalar `extension_m`, `load`, one-way `attached`.
- `GripParams` / `BuckParams` on `SledParams`, in the tape roster, OFF-by-absence.

**Measured, not picked — the two numbers stage 2 spends:**

| | value | how it was got |
|---|---|---|
| `unseat_gain` | **0.0669** (shipped) | calibrated so the kernel's scalar extension matches the DRAWN CHAIN's p90 over 45 distinct drives / 84 airborne windows: 0.8177 vs 0.8222, ratio 0.995, rank corr ~+0.96 |
| `capacity` | **78.34** ← *dial this in* | p90 of the kernel's own load over the same windows, read at SUBSTEP resolution. His "maybe 10 % of jumps" holds by construction on the distribution it was actually measured on |

Reproduce either with:

    SEADS_PROBE_CSV=1 SEADS_GRIP_GAIN=0.0669 ./build/seads_sled_probe.exe superman <tape>

17 CSV fields after `path`. The human-readable table stops at 12 rows — corpus claims
use the CSV. The buck pair defaults to the shipped 0.02/3.0 in the probe now.

---

## §3 THE RULINGS THIS RUNG IS BUILT ON (spec, LADDER §7.7a)

**Q1 — WHICH SIDE (Chad, 2026-08-30): "whichever way he's already leaning out."**
The release takes the lateral the body already has at touchdown and amplifies it. It
does not invent a side and does not read the machine's roll.

**Q2 — WHERE IT LIVES (Chad, 2026-08-30): "the kernel grows its own extension."**
Done — that is §2. `sim/` cannot see `render/`, and a taped field driven by a render
solve could never replay.

### ★★★ AND THE DEPARTURE IS FORWARD, NOT AFT — measured on the KERNEL's own windows

The first pass reported "up and back". That was a median of ABSOLUTE values and it was
**wrong**; it was also graded on the windows the render CHAIN selects, which are not the
set the shipped kernel latches on (6 of 10 overlap). Re-measured on the 10 windows above
the kernel's own p90 load:

| | chain-selected (n 8) | **kernel-selected (n 10)** |
|---|---|---|
| lateral share of the HORIZONTAL departure | 0.204 | **0.220** |
| goes FORWARD, not aft | 6 of 8 | **8 of 10** |
| side predicted by the buck's own lateral push | 4 of 8 | **8 of 10** |

**Read it so:** at the moment the grip breaks he is going UP, **forward over the bars**
8 times in 10, with about a fifth of his horizontal departure sideways — and the BUMP
that threw him picks which side, 8 times in 10. Corpus-wide the sign is aft, because
drag streams him back in flight; it FLIPS at the landing, which is what a landing is —
the machine stops and he does not.

★ So the side exists and can be inherited (the first pass's "it cannot" was too strong),
but it is WEAK: ~0.22 of horizontal. **The gain that turns it into a visible departure
is doing most of the work, and only Chad's eye can set it.**

---

## §4 WHAT STAGE 2 IS

In order. **Each step is separately gated; the last one is his drive.**

1. **Dial `capacity` from 1e30 to 78.34.** The moment this lands the mechanism can fire.
2. **Wire `attached` into the machine.** §7.7: `rider_mass_kg` (87.5) leaves — `cg_off`
   (`sim/sled.cpp:313`) and `patch_geometry` (`:317`) both change. A sled tumbling
   riderless is physically a different machine.
3. **Give the departure its direction**: amplify the lateral he already has (Q1), and
   carry him FORWARD as well as sideways (§3). Render-side pose/chain work.
4. **RE-TAPE.** Steps 1–2 move tapes by construction — that is the whole reason §7.7
   stages it this way. Old tapes replay OFF-by-absence and stay valid; new goldens are
   cut after.
5. **Hand him a drive**, ONE DIAL AT A TIME.

### ⚠⚠ THE MOMENT THIS LANDS, TWO SLEEPING THINGS WAKE UP

Both are already fixed in the tree, both were found by red-team, and both are inert
today — do not "clean them up":

- `app/main.cpp` KEY_R autoright zeroes `sled.grip` beside `ws_exch_l`. `attached` is a
  ONE-WAY LATCH and structurally cannot re-converge; without that line a live drive
  containing an autoright stops replaying the day the release is wired.
- The grip dials are in `SLEDTAPE_PARAMS_D` **and** OFF-by-absence in the loader. Both
  halves are required. A dial that is off-by-absence but absent from the roster can
  never be turned back on by any tape.

### ★ WHAT §7.7 ASKS FOR THAT IS STILL NOT BUILT — say so, do not discover it

- **`seat_load_frac` / `board_load_frac`** — no such symbols exist. They drive §7.3
  stages 1 and 2 (unweighted, boards free).
- **`rider_up_m` gaining a physical driver** (machine vertical accel). `sim/sled.cpp:370`
  is still a pure slew off the stand input, so §7.3 stage 1 — "he floats off the seat;
  `rider_up_m` rises" — cannot happen yet.

Neither is a defect in stage 1; both are unbuilt spec, and a stage-2 agent should decide
deliberately whether the release needs them.

---

## §5 FIRST MOVES

1. Read `CLAUDE.md`, then `docs/SUDBURIAN_LADDER.md` §7.3–§7.9 **and §7.7a**, then
   `docs/SESSION_HANDOFF_20260830_r4a_lateral_MEASURED.md` (the measurement and the
   red-team ledger).
2. **Run the gate — the hook, not the pieces**: `.claude/hooks/gate.sh`. Confirm
   **1741/1747** and the six reds BY NAME before touching anything: `probe P-F`, `E12.1`
   (inherited from main), `sled_slides_before_it_tips_on_flat_snow`,
   `sled_grip_ceiling_stays_below_the_tip_threshold`,
   `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`.
3. Red-team the stage-2 design in a fresh context BEFORE it lands. Stage 1's red-team
   found nine real defects in work that was already gated and mutation-verified.
4. Build him `build-play` and give him the ABSOLUTE path. He opens the exe; you build it.

---

## §6 TRAPS THIS THREAD PAID FOR — the full set is `docs/lessons.md`

- ★★★ **A ctest count certifies the TESTS built, never the GAME.** This branch arrived
  with a published 1731/1737 and a `seads` target that had never compiled. Run
  `.claude/hooks/gate.sh`; by hand, read the BUILD's own `$?` — `cmake --build … | tail`
  hands you the tail's zero over the compiler's failure.
- ★★★ **An A/B that proves equality must prove its arms DIFFERED first.** The
  inertness leg's first draft never actually released and passed its own mutation.
- ★★★ **Half a tripwire is worse than none, because it reads as covered.**
- ★★★ **A threshold read off a subsample is right only by luck** — and when an
  instrument warns about itself, that warning is a TODO, not a disclaimer.
- ★★★ **Grade the mechanism on the windows IT selects**, not the ones the prototype did.
- ★★ **A median of ABSOLUTE values cannot tell you a direction** — report the sign
  distribution beside the magnitude. And never divide a directional "share" by an axis
  that is rectified by construction.
- ★★ **A test that inherits a default is a test about today's default.** A leg whose
  subject is "the mechanism is off" must SET it off.
- ★★ **A sweep over a dial the statistic is invariant to is not N measurements.**
- ★ **Count the corpus again — it grows.** 157 tape files today, 45 distinct by md5.
- ⚠ `origin/main` is **9 commits ahead** (repo-wide LF `.gitattributes` pin,
  `LANES.toml`, the nightly-gate red-SET baseline). Merge it as its OWN commit, build the
  `seads` target explicitly afterwards, and expect `generated/graph/` to conflict —
  the resolution is always REGENERATE, never a hand-merge.
