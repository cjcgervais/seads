# Session handoff 2026-09-10/11 -- KERNEL v14 = S-leanlead (LANDED)

## 0. STATUS FOR THE NEXT AGENT (read this first)

**LANDED ON MAIN `85896a73a` 2026-09-11, tag `kernel-v14-leanlead-signed`, Chad-flown
and signed, sentinel audit PASS.** Branch `feel/yaw-bank-balance` in
`D:\flight_sim2\seads-feel` == main. Working tree clean, everything pushed.
The kernel lane is DORMANT == main. Chad's fly tree `seads-recon` carries it as
`6bea78cb3` (merged by the sentinel, flight-sim2-ae; its build-play exe time was
reported to Chad by the sentinel).

Chad's verdicts, verbatim:
- 2026-09-10 on build-play from the pre-fold tree: "yea this is good, better than
  what is currently on main and feels more intuitive and would also extend that to a
  potential beginner as more friendly. Good commit this change do all the necessary
  SOP to invoke a flight kernel version. Attach that to main".
- 2026-09-11 (relayed by the sentinel): "land the kernel change when its gate
  passes, I have flown and it is good and an improvement."

Landing record: code tip `d9f6dc6ea`; three docs/LANES-only commits above it; linear
fast-forward from cam-smooth's `ec2e1a93a` (sentinel sequenced cam-smooth FIRST; the
union merge kept their `straight_max 12` beside `lean_lead 0.3` in
`config/controller.toml` and both pins in `test_load_controller.cpp`). Full gate on
`d9f6dc6ea`: 6 failed of 2015 == the baseline six MEMBER FOR MEMBER
(`gate_baseline.py check`; log `seads-feel/build/gate_d9f6dc6ea.log`, untracked).
Red-team LAND-WITH-FIX, P0 folded and mutation-verified (section 5).

NEXT (only on Chad's ask -- the kernel is frozen again): (a) if he wants MORE
bank-with-crab, walk the ladder in section 3 (0.5, then 1.0 = bank-first) -- one
line in `config/controller.toml`, re-key the loader pin, re-record the golden, one
fly each; (b) if it reads yaw-first only while the mouse is MOVING (not held), the
next rung is section 6's first item (the aim-rate feedforward has no roll term);
(c) the P2 telemetry note in section 5 if any HUD ever reads `telem.held_bank` as
the roll target. Do NOT re-open the blend-band P1 without his fly: he signed that
behaviour.

---

Original session record follows (written before the landing; the tense is the
session's, the facts still hold).

Branch `feel/yaw-bank-balance` in `D:\flight_sim2\seads-feel`, off main `92f111e81`.
A feel-loop maintenance rung: ONE dial, Chad flies it.

## 1. Chad's ask (verbatim intent)

"Revisit the seads-feel sandbox to make a test on what the eagle has in
EvC2026 because I think it is a near even balance of yaw and bank in the
confluence of the turn dynamic to the cascade from mouse aim deflection.
Currently, and correct me if wrong, there is more yaw early on than banking.
... All other things stay the same. Get a higher rating on that buttery feel."

## 2. Was he right? YES -- measured, not argued

`test/unit/test_yawbank_balance.cpp` (kernel-owned) applies a lateral aim STEP
to the level-trim closed loop and reads two clocks: `t50_nose` (the nose's
horizontal swing has closed half the step) and `t50_bank` (the bank has reached
half its eventual peak), plus the bank's share at the moment the nose is
half-way. Main's tree (`lean_lead = 0`):

| step | t50_nose | t50_bank | bank clock / nose clock | bank share at nose-half |
|------|----------|----------|-------------------------|-------------------------|
| 2 deg | 0.133 s | 0.158 s | 1.19 | 0.41 |
| 4 deg | 0.133 s | 0.158 s | 1.19 | 0.40 |
| 7 deg | 0.142 s | 0.192 s | 1.35 | 0.41 |
| 12 deg (committed) | 0.167 s | 0.108 s | 0.65 | 0.90 |

Inside FINE (under `blend_lo` 5 deg) the bank is ~20% LATE against the nose and
has less than half arrived when the nose is half-way. Past `blend_hi` the roll
leads (bank-to-turn at 260 deg/s) -- his read is about the FINE/blend window,
and there it is true.

WHY (read off `control/controller.cpp`, not guessed): the rudder pointing is
ONE lag (`K_theta * yaw_scale` = 6.4/s, tau ~0.16 s) and the S-aimff aim-rate
feedforward leads it while the mouse moves. The FINE bank is TWO series lags
with no feedforward: `held_bank` chases the lean target at `[auto_level] rate`
5.5/s (tau 0.18 s), THEN the wings-hold limb chases `held_bank` at `K_phi` 5/s
(tau 0.2 s). The lean TARGET is large (8 deg bank per deg of lateral error) but
by the time it is reached the nose has already closed most of the error and the
target has collapsed with it -- the bank is a trailing transient.

## 3. The dial -- `[auto_level] lean_lead` (new key)

The wings-hold roll limb now chases `held_bank + lean_lead * d`, where `d` is
the live lean's SAME-SIGN EXCESS over the held bank:
`d = lean_target - clamp(held_bank, min(0, lean_target), max(0, lean_target))`
-- only the part of the lean that is MORE bank in its own direction than the
hold already carries (continuous everywhere, no threshold; the red-team P0
fold, see section 5). The first lag is skipped on the way IN only. Every
other case -- rest (lean target 0: the AT-13 banked-spawn capture), the lean
RELEASE, an aim shrinking toward the nose, an inverted rest, the MB-right
righting -- leaves the target as the SAME double, so the flown decay tree is
bit-identical there (pinned tick-for-tick in the test). `lean_lead = 0` never
enters the branch: the structural OFF arm = main's kernel.

Untouched, by construction: `yaw_scale`, `K_theta`, `K_phi`, `rate`,
`lean_gain`, `lean_max`, the blend band, the aim feedforward, the push branch.

Instrument sweep (same probes, only the dial moves):

| lean_lead | 2 deg: clock ratio / share / peak bank | 4 deg: ratio / share / peak |
|-----------|----------------------------------------|-----------------------------|
| 0 (main)  | 1.19 / 0.41 / 4.6 deg | 1.19 / 0.40 / 9.3 deg |
| **0.3 (this build)** | **0.88 / 0.60 / 5.1 deg** | **0.88 / 0.59 / 10.1 deg** |
| 0.5       | 0.75 / 0.71 / 5.4 deg | 0.75 / 0.70 / 10.8 deg |
| 0.7       | 0.62 / 0.79 / 5.9 deg | 0.69 / 0.77 / 11.5 deg |
| 1.0       | 0.56 / 0.87 / 6.6 deg | 0.62 / 0.82 / 12.5 deg |

0.3 is where the two clocks are within ~12% of each other and the bank's share
at nose-half is ~0.6 -- the instrument's "near even" point, which is his stated
target. Above it the bank LEADS the nose (bank-first, not even). 0.3 ships in
this build's `config/controller.toml`; the ladder is his to walk.

## 4. Fly card (build-play, the feel binary) -- FLOWN 2026-09-10, verdict in section 0

EXE: `D:\flight_sim2\seads-feel\build-play\seads.exe`
Dial: `D:\flight_sim2\seads-feel\config\controller.toml` line `lean_lead = 0.3`
(read at launch -- edit, relaunch, no rebuild).

1. Level at ~140 m/s. Small lateral aims, 2-4 deg off the nose, held. Does
   the bank now come on WITH the crab instead of after it? Does the entry read
   as one motion (the "buttery" ask) or as two?
2. Same, moderate 6-8 deg. The band: no slam, no double-step at the blend edge.
3. Near-center centering: the last half degree must still read as RUDDER
   (Fly-10 rejection vector). If it reads as wing-wag, walk back.
4. Slow flight < ~80 m/s: wallow sentinel (the lean loop's low-V phase margin).
5. Freelook CARVE (held lateral aim in freelook): banks sooner now; acceptable?
6. Release: let go of a moderate aim -- the roll-out must be the flown one
   (untouched by construction; confirm by feel).
7. Banked spawn / a rest after a hard turn: no lurch (AT-13 arm, bit-identical).

Ladder: 0 (main, walk-back) -> 0.3 (this) -> 0.5 -> 1.0 (bank-first).
Verdict line: `LEANLEAD-0.3: EVEN / still yaw-first (-> 0.5) / bank-eager (-> 0)`.

## 5. Gate + evidence

- Loader pins `lean_lead == 0.3`, walls 1.5 and -0.5 (`test_load_controller.cpp`).
- `test_yawbank_balance.cpp`: 3 cases / 806 assertions green -- mechanism pin
  (FINE steps: bank earlier, peak not eroded, share up), blend == 1 first-tick
  bit-identity, AT-13 rest + lean-release bit-identity, positive-control arm.
- Controller golden RE-RECORDED (knob-off arm verified bit-identical BEFORE
  the re-record; the move is the roll channel + carry). Note in the header.
- RED-TEAM (fresh-context Opus, on the flown tree): LAND-WITH-FIX.
  P0 FOLDED: the first-cut predicate `d = lean_target - held_bank, fire if
  d*lean_target > 0` fired for ANY opposite-sign carry (a MANEUVER->FINE capture
  at -40 deg with a small right aim), stepping the hold target by
  lean_lead*|held_bank| across the aim's zero crossing (~30 deg/s of aileron
  at a 20 deg carry; a bare-threshold chatter edge). Now `d` is the lean's
  SAME-SIGN EXCESS over held_bank (a clamp -- continuous, no threshold); the
  continuity leg pins it (0.08 rad/s vs the mutant's 0.58, bound 0.19), and
  the first probe at 0.02 deg was itself VACUOUS (inside the deadzone latch) --
  moved to 0.3 deg. Golden did NOT move under the fix; instrument numbers at
  0.3 identical (the step probes start from level, same-sign carry).
  P1 (walk-back reds the instrument): folded -- the instrument arm is now
  config-relative. The LOADER pin `lean_lead == 0.3` stays: every flown dial
  is pinned that way here (lean_gain == 8.0), the walk-back re-keys the pin.
  P1 (lead runs in the blend band, held_bank frozen there) NOT folded, on
  purpose: the band's maneuver limb already targets the live lean
  (roll_target_mix), so the hold limb agreeing with it is the band's own
  design, the target is the same capped lean (not a lean_gain escalation), and
  a bare `err < blend_lo` gate would be a NEW step at the FINE edge (the
  McRuer transition trap). Chad flew this band behaviour and signed it.
  P2 noted: `telem.held_bank` mirrors the setpoint, not the led target the
  limb chases this tick (instrument fork if a HUD ever reads it as the target).
- Full gate on the merged + folded code tip `d9f6dc6ea`: 6 failed of 2015 ==
  baseline by name (ai pair + four GI4 sled debts). Sentinel audit of main
  `85896a73a`: PASS, observations only (verdict re-read from the log by the
  runner; tag resolves to the tip; every non-owned file announced; nothing in
  sim/ app/ render/ world/).
- Graph regenerated; layer check OK; lanes_lint OK.

## 6. Open / not done

- The aim-rate feedforward (`[aim_ff] gain 0.3`) feeds pitch+yaw only, no roll
  -- a second structural yaw-early bias, deliberately NOT touched (one dial).
  If 1.0 still reads yaw-first while the mouse is MOVING, that is the next rung.
- `K_phi`'s 0.2 s wings-chase is now the binding stage (the TOML's own note);
  K_phi 7 needs the AT-8/AT-13 gates re-checked (previously saturated the roll
  override).
