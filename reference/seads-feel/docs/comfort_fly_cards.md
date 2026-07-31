# Comfort program — FLY CARDS (Chad's stick is the only verdict)

Every mechanism below is landed on `auto/comfort-orient`, gated green, red-teamed
(fresh-context Fable, P0/P1 folded), and **ships OFF / behavior-preserving by default**.
Each card is one flip + one flight. Fly them ONE AT A TIME (the research memo's composition
caution: mitigations can interact badly — PubMed 39331562).

---

## CARD 1 — S-orient: the ORIENT verb (double-tap Space)

**What it does:** double-tap the freelook key (second press within the window) →
aim snaps to your guarded velocity (same guard as the existing freelook+override release),
the chase camera HARD-CUTS behind your flight path that same tick (no slew — the research
says angular snaps beat smooth slews for nausea), your freelook orbit re-centers, and the
second tap's release rolls the horizon level via the existing S7-hrz recovery.

**Why:** measured baselines — in a sustained turn the camera rides **96° off your flight
path and never converges**; a single space tap changes nothing; an immelmann/split-S exits
with camera-up **180° inverted**. The orient verb is the missing "put me back together"
command: the instrument shows the oblique collapse 95.8° → 0.23° in one tick.

**To fly it:** `config/controller.toml` → `[freelook]` → `orient_double_tap_s = 0.30`
(currently `0.0` = off).

**Honest flags (decide with your hands, not our numbers):**
- ⚠ **It moves the AIM** (to velocity), not just the camera — that's what makes "behind
  you" stable, but it means a double-tap mid-turn releases the turn. If you tap space
  rhythmically for quick glances, two taps within 0.3 s will fire it. If that collides
  with your scan habit, try 0.20, or ask for a dedicated-key binding instead (cheap flip).
- In a SUSTAINED turn the camera cut is instant but the oblique re-opens as the turn
  continues (physics of the lag law — the camera can't stay behind a rotating velocity
  without a per-tick weld, which is banned). Its main value is after VERTICAL maneuvers.
- Orient is suppressed while an override key is held (you're still maneuvering — D9).
- If you release the second tap while holding an override, the horizon roll is skipped
  (existing D9 semantics); the next clean tap recovers.
- Dials if the feel is close-but-not-right: `orient_double_tap_s` (the window);
  the roll speed is the existing `horizon_recovery_rate`/`settle` (yours — we did not
  touch them).

---

## CARD 2 — S-cues: ghost horizon + bank arc (peripheral orientation cues)

**What it does:** two always-on-while-enabled, world-stable HUD cues, both at the screen
periphery (the center reticle zone is exclusion-gapped — nothing new lives where you aim):
- **Ghost horizon** — a thin low-opacity line where the LOCAL LEVEL plane crosses your view
  (the attitude "which way is up" line, NOT the visible limb — on our little planet the limb
  dips way below level; this line is your ADI). Short **sky-side ticks** on its inner ends
  point at the sky: if the ticks point down-screen, you are inverted. Recomputed from
  local-up every frame — it cannot lie on the sphere.
- **Bank arc** — top-center Falcon-style arc, ticks at ±10/20/30/45/60°, pointer = your TRUE
  full-range bank (reads pegged-hard-over when past 60°, and correctly NOT level when
  inverted — the red-team killed a folded-angle bug that would have read wings-level upside
  down).

**Why:** the instrument showed full loops leave ~zero residual debt — your loop nausea is
the DURING: uncontrolled visual rotation with no stable reference (research memo §1.2, §3).
These are the two strongest-evidence passive cues (REC-3/REC-4), placed per the S-retclamp
lesson (periphery only).

**To fly it:** `config/controller.toml` → `[comfort]` →
`cue_horizon_alpha = 0.20`, `cue_bank_arc_alpha = 0.35` (both ship `0.0` = off;
`cue_horizon_gap_frac = 0.30` sets the center exclusion). Fly each cue alone first, then
together — and fly this card SEPARATELY from card 1.

**Honest flags:**
- The ghost line marks LEVEL, not the terrain limb — expect it well above the visible
  horizon at altitude. That's correct (it's the recovery reference, not scenery).
- Opacities are starting guesses from the literature, not tuned feel — dial to taste.
- If the mid-screen line bothers your eye in normal flight, try horizon alpha 0.10-0.15
  before giving up on it; the bank arc alone may be enough.

---

## CARD 3 — S-carets: screen-edge threat indicators (multi-bogey without the freelook tax)

**What it does:** every bandit that is NOT on screen gets a small triangle caret at the
screen edge pointing the way you'd turn to face it (nearer = bigger). The engaged bandit
with a live pipper is skipped; an engaged bandit parked on your six (no pipper) still gets
its caret. Carets are hysteretic (no strobing when a bandit dwells at the screen edge) and
continuous through the lens shift (both red-team catches, folded).

**Why:** your multi-bogey ask was "more freelook freedom, but it's also disorienting."
The research's strongest answer is to reduce the NEED to look: with edge carets you know
where everyone is without sweeping the camera (REC-2 — the Elite/Everspace/War Thunder
convention). Freelook stays exactly as it is; you just need it less.

**To fly it:** `config/controller.toml` → `[comfort]` → `cue_caret_alpha = 0.5`
(ships `0.0` = off).

**Honest flags:**
- Carets show DIRECTION + rough range (size), not elevation numbers — if you want a
  radar/mini-map instead (the full tactical picture), that's a different, bigger candidate;
  say the word.
- Fly this card separately from cards 1 and 2 first; then try card 1 + card 3 together
  (they compose mechanically — the research warns combinations can feel worse, so judge
  each addition on your own stomach).

---

## The composed result (why we believe this is the ultimate-solution SHAPE)

The instrument's chained scenario: an immelmann (you exit with the camera-up 180° inverted
— the measured "upside down inside out and backwards") followed by ONE double-tap orient:
camera cut to 0.04° behind the flight path at the fire tick, the horizon rolls level via
the existing recovery profile, fully converged in 113 ticks (~0.9 s). The during-maneuver
vection is covered by card 2's cues; the multi-bogey scan tax by card 3. Fly order
suggestion: card 1 first (it's the verb you asked for), then 2, then 3.
