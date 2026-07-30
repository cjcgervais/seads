# v9 S-nosesnap — fly cards (2026-07-29)

Kernel: `feel/kernel-v5` post-`9cf264664` (gate 388/388, zero moved goldens).
Ruling ledger: `docs/DECISIONS.md` (verbatim copy of mandalark-kernel @ b4c0751).
The law under test, your words: freelook welds the aim to the nose; release =
one instant snap — behind the plane, upright to the horizon, aim and nose in
view; then ordinary mouse-aim lag; keys never touch the camera.

**Every card states a DURATION, not a firing** — the v8 lesson: "the snap
still fires" is a tick-level fact a broken kernel passed.

---

## CARD 1 — the release with keys held (the v9 complaint, the main event)

Hard turn on mouse → hold Space, keep flying on qweasd → release Space with
the keys STILL down → **keep turning on the keys for three seconds**.

- PASS: the instant of release puts you directly behind the aircraft — rudder,
  nose, aim dot all centered, **horizon level immediately** (even coming out
  of a loop; no roll-in over a second). Measured: camera 0.578° off the nose
  at the fire tick (was 18.6°), up-debt 0.003° (was 44.9°).
- ⚠ KNOWN SEAM, pre-attributed (red-team P1 — YOUR call, nothing changed):
  for **0.30 s after any release** the mouse deltas are DROPPED (the CQ2
  ease-back suspension, `[freelook] easeback_time`, ruled 2026-07-03). Your
  new words — "mouse aim authority is instantly re-established" — contradict
  it, and its original rationale is dead twice over (the mouse basis has been
  raw since S7-raw; the release camera move is now a hard cut, no ease left
  to bridge). If the first third-second of mouse after release feels dead,
  THAT is this window, not the snap — the flip is one line
  (`easeback_time = 0`) and waits on your ruling.
- KNOWN TRADE, expected: over the next seconds of HARD key-only turning the
  plane turns out from under the parked aim and the view goes oblique again
  (~17° standing, up to ~40° transient at 1 s in the harness's full-deflection
  turn). That is the deflection view you ruled for in v8 — the aim is your
  parked plan, and the cure is to move the mouse. If THIS still reads as "not
  chase," it is a genuine physical tension (lag against a parked aim cannot
  stay behind a turning plane) and it is your call — say so and we design with
  you; nothing gets smuggled in.

## CARD 2 — the mid-turn look-around with NO keys (the retired carve)

Mid mouse-aim turn → hold Space, hands OFF the keys → look around for two
seconds → release.

- PASS: **the plane stops turning and holds its nose while you look; it no
  longer keeps carving the old turn.** (Your ruling: "the nose maintains its
  heading and stops whatever input it was giving it via mouse.") On release:
  nothing moves but the camera — the aim is already waiting on the nose
  (measured: aim jump at release 0.002°, was 25.5°).
- If the plane easing out of your turn the moment you press Space surprises
  you in a way you don't like, that is the weld itself — say so; it was your
  direct either/or ruling and walks back only by ruling.

## CARD 3 — the inverted release (the "oblique top down view" case)

Fly a loop or split-S → hold Space mid-maneuver (keys optional) → release
while the world is still rolled/inverted → **watch the first half second**.

- PASS: ONE instant step — behind the plane AND horizon level in the same
  frame. No 1-second roll-in (the old 150°/s recovery is retired). Measured on
  the immelmann-exit harness leg: up-debt at the cut 178.7° → 0.000°.
- Watch for: whether the instant 180°-class reorientation reads as crisp
  ("Instantaneous. No need for anything else") or as a jarring teleport. Your
  words chose instant; the fly is the test.
- Also expected, pre-attributed: if you keep HARD-turning on the mouse after
  the release, the horizon slowly rolls off again over the sustained turn
  (~73° after six more seconds of max-rate turning in the harness) — righting
  is edge-only by design (no continuous auto-level, the S7-mouselevel ban);
  the next release rights it again. The CQ2 0.30 s dead-mouse window (CARD 1)
  applies here too.

## CARD 4 — the v8 win must still hold (do-not-regress)

Mouse-aim turn, then add hard aileron+elevator keys mid-track, mouse still
live — **for the whole hold**.

- PASS: no camera snap of any kind at the keypress; the camera stays bound to
  your aim with the ordinary lag, continuously. (Harness: `mouseaim_keys`
  bit-identical to v8.)

## CARD 5 — the double-tap (same verb, redundant trigger)

Anytime: double-tap Space.

- PASS: identical to a release — behind the nose, upright, instant. (It now
  lands on the NOSE like everything else; it used to land on the flight path,
  up to ~AoA away.)

---

## Walk-backs (scopes stated honestly — the guard's correction)

- `[freelook] release_orient_with_keys = false` disables the WITH-KEYS release
  snap ONLY. It does NOT walk back the weld, the nose target, or the
  instant-up — none are behind that flag; flipping it alone yields an unflown
  hybrid.
- The true walk-back is the v8 seal: tag `flight-kernel-v8-2026-07-29`
  (`ae7ae8f23`); pre-arc, the v6 seal.
- `[horizon_recovery] rate = 0` structurally disables the upright-at-release
  (releases then land behind the nose but leave the world rolled — diagnostic
  only, not a flyable state).
