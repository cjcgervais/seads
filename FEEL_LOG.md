# Feel Log

Chad's flight-test journal. One dated entry per test session — what was flown, what it felt
like, what changed as a result. This is the *feel* record; tuning-constant changes belong in
`tuning/`, mechanism decisions belong in `docs/DECISIONS.md`, and durable conceptual
knowledge belongs in `docs/cascade/`.

---

## 2026-07-28 — rudder trim + release-orient fly ("okay we have a winner")

**What changed since last test:** `yaw_scale 2.2 → 2.0` (the rudder-bias trim) and
S-relorient (every freelook release now fires the orient verb — instant chase-behind on
release). Both on `feel/kernel-v5`, tip `b2019cf43`. See `docs/DECISIONS.md` 2026-07-28.

**Result — APPROVED, both.** Chad, verbatim:

> "The banking and rudder combo is better balanced. Now that the flight kernel was given a
> more sufficient engine per weight ratio, the mouse aim and nose is responding better
> without the need of so much rudder. I believe this is because it feels much better now to
> not have to chase the mouse with so much rudder but now the plant is able to respond."

The load-bearing feel insight: **rung D's power (T/W 0.61) is upstream of the rudder
feel.** Chad had "been intuiting all along that the airframe is being underserved" — with
the plant now able to respond, the yaw pointing no longer has to drag the nose onto the
aim, and 2.0 reads balanced where it once read weak. Neither pre-agreed fallback rung
(`Cy_beta`, `center_frac`) was needed.

**Follow-up, same day — auto-right quickening, also APPROVED:** `inverted_delay`
1.0 → 0.5 s (the belly-up rest timer before the wings slow-roll upright; the 180°/s roll
itself unchanged). Chad: "yes perfect as expected 3/3!" — three asks, three approvals in
one session. Standing fly sentinel: a loop apex or slow roll where the hand rests a full
half-second now auto-rights sooner; if it starts stealing inverted maneuvers, walk-back
is 1.0 (or 0.75 splits the difference). See `docs/DECISIONS.md` 2026-07-28 entries.

---

## 2026-07-23 — rung-E flight test (45° knife-edge gate)

**What changed since last test:** the push/split-S commitment gate moved from
`horizon_enter = 1.0°` to `45.0°` (exit 40°, hysteresis); a red off-screen arrow now shows
the true aim direction when it swings off-frame. See `docs/DECISIONS.md` and
`docs/cascade/push-gate-knife-edge.md`.

**Checklist to fly:**

- [ ] **Dive test with hard lateral hold, watching the red arrow.** Hold the cursor hard
      left (or right) for a sustained period, at various speeds. Confirm: no push/dive
      commits during the hold, no matter how long it's held or how the arrow drifts. Watch
      the red arrow specifically — does it ever cross below the 45° line during a *pure*
      lateral hold? It shouldn't, but if it grazes near the line without committing, that's
      the gate working as intended.
- [ ] **Split-S via big deflection + mouse well down.** Flick hard left (or right) AND push
      the mouse well down (aim clearly past the 45° line). Confirm: commits cleanly, rolls
      all the way through past inverted, comes out the bottom on demand — this is the
      behavior the gate is supposed to *preserve*, not just the mystery-dive it's supposed
      to *prevent*.
- [ ] **Energy retention in turns.** Sustained turns at various bank angles — does the bird
      hold speed/altitude the way the energy-fighter (eagle) identity promises, or does it
      bleed unexpectedly? (Cross-check against `tuning/evc2026-v5-rungE.md`'s
      `energyRetention` / `glideBleed` values if something feels off.)
- [ ] **Small-end soft adjustments without bounce.** Fine, small cursor nudges near centre —
      confirm they land smoothly (no overshoot/bounce/porpoise) at the current deadzone/expo/
      damping settings.

**Result:** _(fill in after flying — feel notes here, then log any resulting tuning changes
in `tuning/evc2026-v5-rungE.md` and any resulting mechanism decisions in
`docs/DECISIONS.md`)_

---

<!-- Template for future entries:

## YYYY-MM-DD — <what was tested>

**What changed since last test:** ...

**Checklist to fly:**
- [ ] ...

**Result:** ...

-->
