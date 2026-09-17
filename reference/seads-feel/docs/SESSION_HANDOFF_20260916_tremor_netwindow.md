# SESSION HANDOFF — kernel v17 CANDIDATE, S-tremor: `hand_net_window` (2026-09-16)

Written by the OVERNIGHT autonomous run (launch packet:
`docs/SESSION_HANDOFF_20260915_tremor_debt.md`; predecessor record:
`docs/SESSION_HANDOFF_20260915_kernel_v16.md` §3/§4/§6).

**THIS IS NOT A LANDING.** Main is untouched. No tag. The lane is pushed and
waiting on ONE thing: **Chad flies it and says his word.**

---

## §0 STATUS TABLE

**★★★ CHAD FLEW IT (2026-09-16 22:44, exe build-play 05:44:48 on 4a6248c80), verbatim:** "okay I flew the tremor tape, I think we are good I am satisfied, the pause at the top of a loop made it want to right it self and threw me off a bit, split s is good, and the belly up small movements kept me inverted for the most part"

Read: the pause-at-top righting is the dial doing its job on a resting hand (v16 held him
inverted there); the checklist's "nothing should change from v16" on item 2 was only true for a
perfectly still hand and was wrong for a resting one. "For the most part" = the ~1.3°/s
slow-drift wall (leg #2003), accepted by this word. LAND WORD under the v16 precedent.
Landing commit + tag `kernel-v17-tremor-signed` follow; main moves ONLY on the sentinel GO.

| item | value |
|---|---|
| lane | branch `feel/tremor-netwindow`, worktree `D:\seads_sandboxes\tremor` |
| tip | `4a6248c802bd258324e20729711fcb91348d67b7` (`4a6248c80`) + this handoff/LANES commit |
| base | `origin/main` `edfc60ee8` (kernel == v16 landed `354f6df3a`, tag `kernel-v16-yawbudget-signed`) |
| dial | **ONE**: `[auto_level] hand_net_window`, shipped **0.20 s** (`0` = off = the v16 tree) |
| full gate | `6 failed of 2116`, 3700 s, log `D:\seads_sandboxes\tremor\build\gate_fold.log` |
| gate verdict | `py -3 tools/gate/gate_baseline.py check build/gate_fold.log` → verbatim: **"OK -- the red set is EXACTLY the baseline, member for member."** `gate_baseline.py lint`: OK |
| red set BY NAME | probe P-F: the relentless raider keeps the pump and shoots back; E12.1: the raider backfill keeps a faction's pump offense alive; sled_slides_before_it_tips_on_flat_snow; sled_grip_ceiling_stays_below_the_tip_threshold; sled_assist_reference_plane_is_load_weighted; sled_debug_sink_is_write_only — **the baseline six, member for member. NO unexpected red.** |
| test count | 2116 = the v16 baseline 2101 + the 15 legs this lane adds. If questioned, diff `ctest -N` name lists, never arithmetic (v16 §4 trap). |
| lane battery | `ctest -R "righthand\|tremor\|loop_rollover\|controller_load\|params\|load_controller\|yawbudget\|TAPE"` = **52/52**, 17.4 s |
| hash pins | three green: `0xdbdf52980174305e` (rolling loop, at dial 0 AND at 0.20), `0x71176a88ee5d8a8f` (off_arm), `0x7102cf59b6460f33` (the v16 shipped tree) |
| red-team | two fresh-context lenses, both **LAND-WITH-FIX**, all P0/P1 folded + re-measured + re-gated (§4) |
| fly exe | `D:\seads_sandboxes\tremor\build-play\seads.exe`, mtime **2026-09-16 05:44:48**, built on `4a6248c80` |
| graph | regenerated in the same commit; `graphify.py --stale` = current; `graph_query.py check` = layer OK |
| kernel firewall | diff vs base confined to `config/controller.toml` + `control/{controller.cpp,controller.h,params.h}`; `sim/` untouched |
| pushed | lane branch pushed to `origin/feel/tremor-netwindow`. **main NOT touched, no tag.** |

---

## §1 THE DIAL AND WHY 0.20

`[auto_level] hand_net_window = 0.20  # [s] net-displacement window; 0 = off`

The debt (v15 P2): `hand_live` is ANY nonzero aim motion, so a ±1-count/frame
mouse tremor while belly-up resets the hand-rest clock every frame and the
aeroplane never rights. The cure is a **windowed NET** measure: a leaky integral
of the aim's signed angular displacement over `hand_net_window` seconds, divided
by an identically-leaked accumulator of **live** dt, giving a rate in deg/s that
is exact on the very first tick. A continuous liveness
`smoothstep(1 deg/s, 3 deg/s, net_rate)` **scales** the clock's reset
(`ns.hand_rest = min(rest+dt, cap) * (1 - live)`) — no new boolean anywhere, so
nothing can chatter. All new arithmetic sits inside `if (cp.hand_net_window > 0.0)`.

### Sweep table (measured on the shipped instrument)

```
window |  tremor: integ_right  gate_max  net@120  net@10 |  5 deg/s sweep: integ_right  gate_max | walk: integ_right
  0.05 |           3.78     0.017    2.14    1.58 |             0.00     0.000 |         0.20
  0.10 |         108.77     1.000    1.04    0.95 |             0.00     0.000 |         1.02
  0.15 |         109.03     1.000    0.72    0.66 |             0.00     0.000 |         3.15
  0.20 |         109.72     1.000    0.58    0.52 |             0.00     0.000 |         5.74   <- SHIPPED
  0.25 |         109.99     1.000    0.51    0.45 |             0.00     0.000 |        14.55
  0.30 |         110.13     1.000    0.48    0.40 |             0.00     0.000 |        17.27
  0.50 |         110.30     1.000    0.42    0.36 |             0.00     0.000 |        35.19
```

Units: `integ_right` = integrated |roll_right| over 2 s belly-up (theta 170)
through `app::step_frame`, deg. `net@120` / `net@10` = the measure's SETTLED
reading in deg/s for a ±1-count tremor at 120 and 10 fps (the 1 deg/s floor is
the wall it must sit under). Reference: hands off = **110.25 deg**; the tremor at
the dial OFF = **0.00 deg** — that is the debt. `walk` = the zero-mean
random-walk arm.

**Why 0.20:** 0.05 leaves the debt standing (3.78 deg). 0.10 works but its tremor
reading (1.04 / 0.95) sits ON the 1 deg/s floor with no headroom at any frame
rate. 0.20 carries ~45% headroom (0.58 / 0.52) while a genuinely wandering hand
is still 95% vetoed (5.74 against 109.72). 0.25+ buys little headroom and starts
handing righting back to a wandering hand (14.55 → 35.19), eroding the
gun-director veto. 0.20 is also **shorter than the `right_hand_rest = 0.25` ramp
it feeds**, so the window can never outlive its own consumer. The 5 deg/s
deliberate drift is vetoed at EVERY window in the sweep — the law never depended
on the tuning.

---

## §2 THE LAW, HONESTLY STATED (read before flying)

The claim "the gun-director law is preserved" was **struck** from `params.h` and
`controller.toml` by the red-team fold and replaced with a **bounded named
exception**:

> Net aim travel under ~0.2 deg inside the window is scored as no hand; under
> ~0.6 deg as a fractional hand — **deliberate or not.**

The felt wall, derived at the constants (multiplicative reset, gate halved at
live = 1/16) at `sim_dt = 1/120` and `right_hand_rest = 0.25`, is **~1.30 deg/s**
— about 2.2× the tremor, not 5.2×. It is coupled to `sim_dt` and
`right_hand_rest`; that coupling is named in the code. **A sustained deliberate
sweep slower than ~1.3 deg/s, started from a rested hand, is not vetoed.** That
is a hole, it is pinned in both directions by leg #2003, and it is **a ruling for
Chad, not a fold** (see §6, the fly-card question).

---

## §3 RED-TEAM VERDICTS

| record | lens | verdict |
|---|---|---|
| `docs/REDTEAM_20260916_tremor_netwindow_mechanism.md` | mechanism / mutation | **LAND-WITH-FIX** |
| `docs/REDTEAM_20260916_tremor_netwindow_law-feel.md` | law / feel | **LAND-WITH-FIX** |

Both found the SAME P0 by different routes:

- **P0-1** — the veto was **late**: `aim_net_w` was a constant, not a normaliser,
  so a deliberate sweep beginning from a **rested** hand (the only state where
  the gate is nonzero) leaked righting for ~0.10 s at 5 deg/s, and never vetoed
  at all at ≤1.5 deg/s.
- **P1-1 (law/feel)** — the gun-director leg could not red on the failure it was
  named for.
- **P1-1 (mechanism)** — the effective wall is ~1.25–1.6 deg/s, not the
  documented 1–3.
- **P1-2 (law/feel)** — "the gun-director law is preserved" was overstated.

Both records carry a FOLD addendum with every re-measured number and the argument
for the chosen fix.

---

## §4 FOLDS (all P0/P1, re-measured, re-gated)

1. **P0-1 fold** — `control/controller.cpp`: the normaliser now accrues dt **only
   on a live tick**: `ns.aim_net_w = internal.aim_net_w + (hand_live ? dt : 0.0)
   - leak*internal.aim_net_w`. Chosen over the mechanism lens's "clear both
   accumulators" because a gated increment makes both accumulators leak together,
   so their RATIO survives a pause (a hand that stops mid-sweep still reads its
   own rate; an intermittent tremor keeps its memory); clearing throws that away.
   **Re-measured** (belly-up, hand off 0.40 s to a gate of 1.000, then a sustained
   sweep; integral |roll_right| after the hand goes on): **1.50 deg, veto in
   0.008 s at 3/5/10/40 deg/s == v16 to the digit**, against the pre-fold tip's
   25.94 / 14.42 / 7.31 / 2.41 deg and 0.175 / 0.100 / 0.050 / 0.017 s. Tremor
   cure unchanged: 109.72 deg ON vs 0.00 OFF, hands off still 110.25.
2. **NEW LEG #2002** "S-tremor: a sweep from a FULL gate vetoes on the next tick"
   (the red team's own RT1 rig) with an explicit non-vacuity
   `REQUIRE(gate_at_mark > 0.99)`. **MUTATION RUN, not argued:** reverting the
   fold to `+ dt` reds it on 7 of 28 assertions and reprints the red team's table
   (14.42 deg at 5 deg/s).
3. **P1-1 (law/feel)** — leg #2001 renamed "S-tremor: a sweep never lets the clock
   start" with the reasoning at its head; #2002 is the gun-director leg proper.
4. **P1-1 (mechanism)** — the felt wall derived at the constants in
   `controller.cpp`, the `sim_dt` / `right_hand_rest` coupling named, and **NEW
   LEG #2003** "the felt wall of the veto, measured" pins the boundary from a full
   gate: 61.50 / 61.50 / 34.24 / 3.59 / 1.50 / 1.50 deg at
   0.5 / 1.0 / 1.5 / 2.0 / 3.0 / 5.0 deg/s (v16 at 1.0 deg/s: 1.50), asserting the
   hole in BOTH directions.
5. **P1-2** — the overstated law claim struck (§2). **NEW LEG #2004** "the
   tracking-wobble residual, measured not hidden" prints the table
   (±0.25 deg 1 Hz = 109.57 deg of righting; ±0.5 = 98.35; ±1.0 = 23.97;
   ±2.0 = 4.28; v16 0.00–0.04 on all twelve rows) and pins its shape.
6. **The fold's PRICE, measured and pinned** — **NEW LEG #2005** "an intermittent
   tremor, the fold's measured price": ON 109.72 / 108.49 / 97.45 / 43.27 / 86.01
   at k = 1/2/3/5/10 against v16's 0.00 / 1.44 / 5.56 / 21.02 / 86.58, pinned
   "never worse than v16" (3 deg slack at k = 10 where the arms converge).
7. **P3 (both lenses)** — the measure divides by `std::max(ns.aim_net_w, 1e-12)`
   (a `dt == 0` direct caller would have NaN'd the roll demand); a frame-rate-leg
   mutation note that could not fire is struck with the reason in place;
   `app/feel_tape_fields.h` says `SEADS_TAPE_OPT_FIELDS` (not `INT`);
   `controller.h`'s telemetry comment says the measure runs only while the hand is
   live, so a 0 reading means "netted to nothing OR hand off".

**Files carrying the fold:** `control/controller.cpp`, `control/controller.h`,
`control/params.h`, `config/controller.toml`, `app/feel_tape_fields.h`,
`test/unit/test_loop_rollover.cpp`, and both REDTEAM records (FOLD addenda).

---

## §5 NOTABLE DEPARTURES FROM THE PACKET §3 (all deliberate, all written down)

1. **Normalised, not raw, windowed net** (see §1) — the raw form was MEASURED
   failing the gun-director law.
2. **Continuous liveness scaling the reset**, not a boolean predicate (the packet
   offered "or better"). `live == 1` reproduces the v16 slam EXACTLY.
3. **The outer v16 `hand_live` gate is KEPT** — the measure only ever runs while
   the v16 predicate says the hand is live, so the window adds ZERO delay to the
   hands-off path (08-06 ruling pinned bit-identically).
4. **An "unmeasurable motion" clause** — `harness::ClosedLoop` can script
   `aim_moved` without an aim rate; that case stays fully live (fail to the veto,
   never to the righting). Structurally unreachable in the app.
5. **The random-walk tremor is a MEASURED RESIDUAL, not a pass** — at
   `aim_sensitivity 0.14 deg/count` a zero-mean walk genuinely moves the reticle
   ~0.4 deg over a window and reads as a 2–5 deg/s hand. By the gun-director law
   that IS a real hand movement. The leg PRINTS the numbers and pins only the
   ordering.
6. **Frame-rate leg extended** to 240/120/60/30/10 fps + a 0.4 s hitch. The rig
   flips the tremor's sign only on a frame that actually CONSUMED a tick.
7. **A NAMED EXCEPTION IN THE TAPE READER — look hardest at this.** Adding
   `aim_net` / `aim_net_w` to `control::Internal` staled every recorded tape, and
   the throwing reader correctly refused six S-yawbudget legs that replay **Chad's
   OWN 2026-09-13 dives** — fixtures that cannot be re-recorded because they are
   his hand. `SEADS_TAPE_OPT_FIELDS` is written and seeded like any replay field
   but not REQUIRED on read. Defensible ONLY because these are short-memory state
   the recorded input reconstructs (a 0.2 s leaky integral driven by the recorded
   `aim_dx`/`aim_dy` re-converges within one window; the only error is that a
   replay's first ~0.2 s scores the hand as stiller than it was).
   `app/feel_tape_fields.h` carries the rule and an **explicit ban** on ever
   putting long-memory (the rate-PI integral) or latched (`roll_latch`) state there.

**Telemetry / tape:** `Telemetry::hand_net_rate` and `::hand_live_frac` (pure
report), tape columns `hand_net_rate` + `hand_live_frac` and replay-state columns
`i_aim_netx/y/z` + `i_aim_net_w`. Feel-tape column count **160 → 166**, pinned as
a number in its own leg. `control::Internal` size assert 248 → 280. The v5 launch
banner now prints `hand_net_window %.3f s (0 = off)` beside `right_hand_rest`, so
a tape can never be replayed against the wrong dials.

---

## §6 FLY CHECKLIST (Chad) — packet §6.5

**Exe:** `D:\seads_sandboxes\tremor\build-play\seads.exe` (mtime 2026-09-16
05:44:48, tip `4a6248c80`).
Launch it directly (Explorer or the `build-play\fly_*.bat` shape). The startup
banner must read `hand_net_window 0.200 s (0 = off)` beside `right_hand_rest
0.250` — if it does not, you are flying the wrong exe.

Fly these five, in this order:

1. **Inverted rest with your hand RESTING ON the mouse.** Get belly-up, let go of
   the aim but keep your hand on the mouse the way you normally would. He should
   right himself after the usual short beat. *(This is the debt. Before this dial,
   a shaky hand or a wireless mouse left you inverted.)*
2. **A loop with a PAUSE at the top.** Hold it inverted at the apex a moment, then
   pull through. Nothing should change from v16.
3. **An Immelmann.** Nothing should change from v16.
4. **A slow ~5 deg/s lateral drift while belly-up — he must NOT right you.** Aim
   deliberately, slowly, sideways while inverted. The instructor must stay out of
   it. *(The gun-director law.)*
5. **THE OPEN QUESTION — a VERY slow deliberate drift while belly-up.** Slower
   than #4: creep the reticle about **one degree per second** while inverted.
   At that speed the dial currently scores your hand as "not moving" and will
   right you. **Is that acceptable, or should the wall be lower?** The alternative
   trade is `hand_net_window = 0.10` (a tighter wall, but the tremor reading sits
   ON the 1 deg/s floor with no headroom). This is your ruling, not ours.

Also worth one pass: normal fighting for a few minutes, watching for anything that
feels different from the v16 exe. Nothing else in the kernel changed.

**Tape it if you can:** set `SEADS_FEEL_TAPE=<path>` before launching → a
166-column feel tape (the v5 shape, 160 -> 166 columns) with the banner in its header.

---

## §7 WALK-BACK ORDER

**One line:** `config/controller.toml` → `[auto_level] hand_net_window = 0` = the
v16 tree, bit-identically (hash pins `0xdbdf52980174305e` and
`0x7102cf59b6460f33` prove it; all new arithmetic is inside
`if (cp.hand_net_window > 0.0)`).

No rebuild is needed to walk it back — it is a config value. If the whole lane
must go, it has never touched main: simply do not land the branch.

---

## §8 LANDING STEPS STILL OWED (none of them done, in order)

1. **Chad flies** `D:\seads_sandboxes\tremor\build-play\seads.exe` against §6, and
   **rules on §6 item 5** (the slow-drift wall).
2. **His word recorded VERBATIM with a timestamp** in the landing record (the v16
   §3.7 precedent). A fold that changes the artefact re-opens the fly.
3. If a fold follows his word: re-measure, re-run the full gate detached, quote
   `gate_baseline.py check` BY NAME again.
4. Merge `origin/main` ONCE into the lane (union, never `-X ours`; regenerate the
   graph rather than hand-merging it), verify the **kernel firewall** with
   `git diff` on `sim/ control/ config/controller.toml`, and run the full gate on
   the MERGED code tip in an isolated build dir, detached.
5. One landing commit: CLAUDE.md kernel line, `docs/flight-log.md` row, LANES
   status verbatim; annotated tag `kernel-v17-tremor-signed`; push lane + tag.
6. **Ping the sentinel** with SHA, gate verdict by name, file-by-file kernel diff,
   `ctest -N` predicted vs actual, and his words — then **WAIT. Silence is never a
   go.** (Memory law: announce EVERY main push, docs included.)
7. On GO: fast-forward main. Docs-only follow-ups after. The `seads-recon` resync
   + build-play rebuild is the SENTINEL's step, reported with its exe mtime.

---

## §9 OPEN, CARRIED (P2/P3 — skipped by the fold, with reasons)

- **P1-1(b) mechanism (HANDED TO CHAD, not debt):** moving `kNetFloorRate`, or
  making the reset additive so the smoothstep does real work, would lower the
  ~1.3 deg/s wall. NOT folded — it changes what he feels on a dial he has not
  flown once, and the slow-drift hole is **a ruling**. Documented, pinned by leg
  #2003, and put on the fly card (§6 item 5) with `0.10` as the alternative trade.
- **P2-1 (both lenses):** the cure's envelope is in counts × `aim_sensitivity` —
  the tremor cure dies by roughly 3× his current sensitivity. The better form is a
  floor in **counts of net travel per window** rather than deg/s. Same class of
  felt retune as P1-1(b). **Named debt.**
- **P2-2:** `SEADS_TAPE_OPT_FIELDS` is enforced only by a comment. It should pin
  the opt column list and narrow the safety wording to the six fixtures.
  **Named debt.**
- **P3-4:** the loader's lower wall admits a live-but-inert window (0.05). Walling
  it off would make the committed window-sweep leg unrunnable; the degenerate band
  is named in the toml block instead.
- **P3-3 (law/feel):** the Sting's cascade feeds the accumulator without the ZOH
  smear. Pre-existing v16-class seam, **no regression from this lane**; noted only.
- **Packet §7 knife-edge ringing: DIAGNOSIS DONE in parallel** (separate worktree
  `D:\seads_sandboxes	remor-knife`, branch `feel/knife-edge-diag`, tip `45adefe3f`,
  pushed). `docs/KNIFE_EDGE_RINGING_DIAG_20260916.md` + `docs/knife_edge_probes.patch`.
  The term is CONFIRMED: `target_body.y*sin(phi)` in the MB-lean `az_lat` numerator
  (a pure vertical aim reads 100% lateral at the knife edge; via `lean_gain 8` a 2 deg
  offset = 14-16 deg phantom bank; ring 31.6 deg/s, 0.54 s period, GROWING 37%/cycle
  through the `wings_level_gate` edge; `lean_gain 0` arm never rings). Proposed ONE
  dial `[auto_level] lean_vert_purge` (0 = v16, 1.0 removes the term; inert at wings
  level by construction). NOT built; needs Chad's ruling first. No kernel change on
  that branch.

No finding was judged wrong, so nothing was answered with a scar comment in place
of a fold.
