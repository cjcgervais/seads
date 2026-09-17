# The hand tremor — a windowed NET measure of "is the hand live"

**Status (2026-09-16 22:47): KERNEL v17 LANDED** (tag `kernel-v17-tremor-signed` @ `3a95a4d08`,
on `main`, a pure fast-forward from `edfc60ee8`). ONE dial: `[auto_level] hand_net_window`
0.20 s; 0 = the v16 tree bit-identically (three hash pins). Chad's word on the exe built on the
code tip `4a6248c80`: *"okay I flew the tremor tape, I think we are good I am satisfied, the
pause at the top of a loop made it want to right it self and threw me off a bit, split s is
good, and the belly up small movements kept me inverted for the most part"*.
**Grounded in `reference/seads-feel/` at `3a95a4d08`.**

## Lineage

The v15 rung (S-righthand, `[auto_level] right_hand_rest` 0.25 s) put a **hand-motion veto**
on the belly-up righting: the aeroplane rights itself only after the hand has been at rest for
a short ramp. Its own red-team recorded a P2 debt: "hand is live" meant *any* nonzero aim
motion this tick, so a ±1-count-per-frame mouse tremor reset the rest clock every frame and
the aeroplane stayed inverted (integrated righting 1.76° with a tremor vs 117.75° with a still
hand). Nobody had felt it in flight; it was a trap for a wireless mouse or a shaky hand. It was
parked by Chad's word for the rung after v16, then built by the **overnight autonomous run**
(Chad, 2026-09-15: *"establish it as a workflow that runs automatically overnight and that is
cheaper on tokens BY USING OPUS"*), red-teamed by two fresh-context lenses, folded, and flown
by Chad the next evening. Launch packet: `docs/SESSION_HANDOFF_20260915_tremor_debt.md`;
record: `docs/SESSION_HANDOFF_20260916_tremor_netwindow.md` (both in the snapshot).

---

## 1. Feel

The governing law is still the gun-director law (Chad, 2026-09-12): *the aim IS the guns; any
auto-righting is vetoed while the hand moves.* And the 2026-08-06 ruling still holds: hands off
at the apex rights him after the short rest, with no added delay.

What the debt felt like, in the seat: you are belly-up, you take your hand off the stick to
let the instructor right you, but your hand is still *on the mouse*, and the mouse is not
perfectly still. Nothing happens. *"It will not right me."*

What v17 feels like, in Chad's words after the fly:

> "the belly up small movements kept me inverted for the most part"

> "the pause at the top of a loop made it want to right it self and threw me off a bit"

> "split s is good"

The first line is the fly card's belly-up case working: a resting hand with small movements
is read as resting, and the righting comes. The second is the same cure seen from the other
side: at the top of a loop, if the hand pauses, the instructor now treats it as a resting hand
and begins to right — which is the 08-06 ruling acting, not a fault. The named exception his
word accepts: a *very slow deliberate drift* (under about 1.3 °/s) now reads as a resting hand
too. The fly card's 5 °/s belly-up drift still does not right him.

## 2. Principle

The problem was the **predicate**, not a threshold. "Live" was a boolean on any motion at all.
A deadband on mouse counts was rejected twice before (S-aimclamp, S-retclamp): it just moves
the threshold and eats small deliberate inputs, and the aim is uncapped by ruling.

The cure is to ask a different question: not *did the hand move this tick* but *what does the
hand's motion over the last short window add up to*. A tremor alternates sign and **nets to
almost nothing**; a real sweep, of any size, nets to its own rate. So the kernel keeps a short
windowed integral of the aim's own rotation, turns it into a **net rate in degrees per
second**, and maps that rate through a continuous ramp into a **liveness** between 0 and 1.

Three rules shaped the mechanism:

- **No new boolean.** The liveness *scales* the rest clock's reset instead of slamming it to
  zero. A fully live hand reproduces v16's reset exactly; a fully resting hand accumulates rest
  exactly as before; in between, the clock decays smoothly. Nothing can chatter because there
  is no threshold to chatter across.
- **The window counts only LIVE time.** This is the red-team's P0, found independently by both
  lenses. The first build normalised by the window length, which is only right in steady state.
  From a rested hand (clock full, gate at 1, aeroplane rolling at the full inverted rate) a new
  sweep read as a fraction of its true rate for a window's worth of time, so the veto arrived
  ~0.10 s late at 5 °/s and never at all below ~1.5 °/s. Normalising by an identically-leaked
  accumulator of *live* dt makes the ratio exact from the very first live tick after any rest.
- **The outer v16 gate is kept.** The measure only runs while the hand is live at all. The tick
  the hand stops, liveness is 0 and the clock climbs, so the window adds no delay to the
  hands-off path (the 08-06 ruling).

The two walls of the ramp are deliberately **not dials**: 1 °/s is a floor under which nothing
is a hand (roundoff, ZOH residue, the leak's own tail — the v16 "lying instrument" lesson made
structural), and 3 °/s sits below the slowest deliberate input anyone has named, so a real
hand saturates. The *felt* decision point is lower than the saturation, because the reset is
multiplicative (see §3): about **1.3 °/s**, roughly 2.2× the measured tremor rate.

## 3. Math

All in the aim's world frame, per sim tick of length `dt` (frame-rate invariant through
`app::step_frame`; `aim_rate_world` is the ZOH-smeared, frame-rate-invariant rotation vector
`apply_mouse` produced). With `W = hand_net_window` (0.20 s):

```
leak      = clamp(dt / W, 0, 1)
aim_net   <- aim_net   + aim_rate_world * dt   - leak * aim_net          [rad, vector]
aim_net_w <- aim_net_w + (hand_live ? dt : 0)  - leak * aim_net_w        [s, LIVE time only]
net_rate  = |aim_net| / max(aim_net_w, 1e-12)                            [rad/s]
live      = smoothstep(kNetFloorRate, kNetLiveRate, net_rate)            [1 deg/s .. 3 deg/s]
```

Under a constant sweep rate `r` both accumulators share the same leak, so `net_rate = r`
exactly on every live tick, including the first after a rest. A ±1-count alternating tremor at
`aim_sensitivity 0.14` settles at **0.52–0.59 °/s** across 240/120/60/30/10 fps and through a
0.4 s hitch — under the 1 °/s floor at every frame rate the game runs.

Fail-safe arm: a hand that reports `aim_moved` but zero `aim_rate_world` is unmeasurable and is
scored fully live (`live = 1`). Unreachable in the app (both come from the same rotation), kept
for harness callers.

The clock reset, replacing v16's boolean:

```
v16:  hand_rest <- hand_live ? 0 : min(hand_rest + dt, right_hand_rest)
v17:  if hand_live:  hand_rest <- min(hand_rest + dt, right_hand_rest) * (1 - live)
      else:          hand_rest <- min(hand_rest + dt, right_hand_rest)          (unchanged)
hand_gate = smoothstep(0, right_hand_rest, hand_rest)                          (v15, unchanged)
```

`live = 1` gives a 0.0 clock, v16 exactly. The multiplicative form has an equilibrium
`hand_rest* = dt · (1 − live) / live`, so at `sim_dt = 1/120` and `right_hand_rest = 0.25`
the gate is already halved at `live = 1/16`: `hand_rest* = 0.125 s`, i.e. `net_rate ≈ 1 +
2·0.152 ≈ 1.30 °/s`. That is the felt wall, and it is **coupled to `sim_dt` and
`right_hand_rest`** — retuning either silently moves it. A sustained deliberate drift under
~1.2 °/s is therefore not vetoed (v16 vetoed it 100 %); this is the named, accepted exception.

Structural OFF arm: every line above sits inside `if (cp.hand_net_window > 0.0)`; at 0 the
v16 expression runs verbatim. Pinned by three hashes in `test/unit/test_loop_rollover.cpp`:
`0xdbdf52980174305e` (rolling loop, both arms), `0x71176a88ee5d8a8f` (off-arm apex),
`0x7102cf59b6460f33` (v16 shipped apex).

Why 0.20 s (sweep table in the handoff §1): 0.05 leaves the debt standing (3.78° righting);
0.10 works but the tremor reading sits *on* the 1 °/s floor with no headroom; 0.20 carries
~45 % headroom while a zero-mean random-walk hand is still 95 % vetoed (5.74° vs 109.72°);
0.25+ hands righting back to a wandering hand (14.55° → 35.19° at 0.5). 0.20 is also shorter
than the 0.25 s `right_hand_rest` ramp it feeds, so the window can never outlive its consumer.
The 5 °/s deliberate drift is vetoed at every window in the sweep — the law never depended on
the tuning.

## 4. Code

Grounded in `reference/seads-feel/` at `3a95a4d08` (`kernel-v17-tremor-signed`).

- `config/controller.toml` `[auto_level] hand_net_window = 0.20` — the dial and its walk-back
  (*"THE WALK-BACK IS THIS LINE -> 0"*). Loader range check in `config/load_controller.cpp`
  beside `right_hand_rest` (refuses a negative value).
- `control/params.h` — `ControllerParams::hand_net_window`, the `SEADS_FEEL_DIALS` X-macro entry
  `X(hand_net_window, 0.0)`, and the long comment that states the law honestly (the "bounded
  named exception" wording that replaced "the gun-director law is preserved" at the red-team
  fold).
- `control/controller.cpp` — file-scope `kNetFloorRate` / `kNetLiveRate` (1 and 3 °/s, in
  rad/s, with the derivation of the ~1.3 °/s felt wall in the comment above them); the S-tremor
  block inside the controller step computing `ns.aim_net`, `ns.aim_net_w`, `hand_net_rate`,
  `hand_live_frac` and the scaled `ns.hand_rest` reset; `out.telem.hand_net_rate` and
  `out.telem.hand_live_frac`.
- `control/controller.h` — `ControllerInternal::aim_net` (dvec3) and `aim_net_w` (with the scar
  comment on why the un-normalised `|aim_net|/window` was wrong), `ControllerTelem::hand_net_rate`
  and `hand_live_frac`.
- `app/feel_tape_fields.h` — the new internal fields on the feel tape (columns 160 → 166),
  carried under `SEADS_TAPE_OPT_FIELDS` so older tapes still replay; `app/feel_tape_columns.h`,
  `test/harness/feel_tape.h` (reader treats OPT fields as optional). Red-team P2-2 records that
  this opt list is enforced by comment only — recorded, not folded.
- `app/main.cpp` — the config banner line names `hand_net_window`, so a flight can never be
  attributed to a dial it did not fly.
- `test/unit/test_loop_rollover.cpp` — the S-tremor block: the tremor leg (dial OFF reproduces
  the debt, ON restores righting), the real-sweep vetoes leg, the slow 5 °/s drift leg, the
  frame-rate invariance leg (240/60/30/10 fps + hitch), the three hash pins, and the fold legs
  #2002 (*"a sweep from a FULL gate vetoes on the next tick"*, mutation-run: reverting the fold
  reds 7/28) and #2004 (the honest-law leg). `test/unit/test_load_controller.cpp` for the loader
  wall; `test/unit/test_yawbudget.cpp` adjusted for the tape column count.
- Red-team records: `docs/REDTEAM_20260916_tremor_netwindow_mechanism.md` and
  `docs/REDTEAM_20260916_tremor_netwindow_law-feel.md` (both LAND-WITH-FIX; the same P0 found by
  both).

Not in this snapshot's `main`: the knife-edge ringing diagnosis
(`docs/KNIFE_EDGE_RINGING_DIAG_20260916.md`, lane `feel/knife-edge-diag` @ `45adefe3f`), whose
proposed `lean_vert_purge` dial is the other half of this rung. Chad opened its build the same
evening (*"yes on the knife edge for after the push"*) as the v18 candidate. See
`docs/cascade/push-gate-knife-edge.md` for the knife-edge background.
