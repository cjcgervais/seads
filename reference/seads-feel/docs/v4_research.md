# V4 RESEARCH MEMO — flight + view controls on a spherical planet (2026-07-17)

Sonnet research round commissioned by Chad for the KERNEL V4 program ("maximally optimal
setup for flight and view controls on a spherical planet"; the basketball-rim overshoot ask;
freelook globe inertia; auto-level question; responsive elevator while aiming). This memo is
the program's bedrock alongside docs/comfort_research.md (last night's comfort round).

## 1. Spherical-world prior art

| Game | Up convention | Auto-level | Holonomy | Comfort |
|---|---|---|---|---|
| Outer Wilds | ship body frame | none (landing-mode auto-reorient = a discrete idle-gated verb) | accumulates | hard to learn; discrete assist verbs (match-velocity) praised |
| Mario Galaxy | surface normal, lerped ~0.5-1 s | continuous per-tick | resolved by surface contact | invisible — but only works at walking speed, zenith unreachable |
| KSP | north-pole-fixed navball frame | none | accumulates | requires training; atmosphere/orbit camera mode switch is disorienting |
| Astroneer | local-up always rebuilt | continuous | none | fine at low speed; vehicles were "wonky" until reworked |
| SEADS | parallel-transported aim frame | discrete verbs (shipped) | INTENTIONAL (AT-14) | Chad: "near perfect" |

**No surveyed game uses parallel transport for camera-up — SEADS's architecture is the most
sophisticated of the set and already validated on the stick.** The industry answer to
orientation on spheres is DISCRETE ASSIST VERBS, not continuous stabilization; continuous
local-up tracking only appears in slow, surface-bound games where the zenith is unreachable.
(Sources: Steam OW discussions 2789368718110950938; gamedeveloper.com Mario Galaxy demystified;
playtechs.blogspot.com 2007/11 Galaxy camera; ksp-kos.github.io ref_frame; Astroneer jet-powered
patch notes.)

## 2. War Thunder instructor (reverse-engineered)

- Player mouse = aim point in a circle; instructor = PID to the aim, pitch primary, roll
  presents the lift vector, yaw = coordination only (yaw pointing is "too sensitive" —
  community bug 1ePiB24ww1xK). Matches SEADS architecture.
- Error→rate is roughly LINEAR, clamped at full deflection; **no deliberate overshoot**; no
  documented small-error gain boost.
- **Continuous auto-roll to wings-level is the #1 community complaint** ("always fighting
  me"); players switch modes to disable it. AoA/stall protection is a hard clamp players
  feel as the plane "refusing."
- Mouse DPI matters: 1600-2200 native cited as smooth; steppy input makes the instructor
  "flap violently" — fix upstream, never smooth the aim.
- brihernandez/MouseFlight (the canonical open-source WT-style rig): raw aim transform +
  smoothed camera + flight AI steering at the aim — "the single best and most robust system."
  SEADS already has this decoupling.

## 3. The overshoot-and-settle feel (the basketball-rim ask)

t3ssel8r's second-order-dynamics parameterization (f, ζ, r) is the game-feel standard
(hackaday.com 2022/06/30). Percent overshoot PO = 100·exp(−ζπ/√(1−ζ²)):

| ζ | overshoot | character |
|---|---|---|
| 0.3 | ~37% | multi-bounce ring — BAD for aiming |
| 0.4 | ~25% | two bounces |
| **0.5** | **~16%** | **one strong bounce — the basketball rim** |
| **0.55-0.6** | **~10-13%** | **one clean modest bounce** |
| 0.7 | ~5% | barely perceptible |
| 1.0 | 0% | critically damped (today's kernel philosophy) |

Settling (±2%) T_s = 4/(ζ·ω_n); rise T_r ≈ 1.8/ω_n. At ζ=0.5, ω_n=20 rad/s: rise ~90 ms,
one 16% bounce, settled ~400 ms. **Chad's ask maps to ζ ≈ 0.5-0.55.** The r parameter adds
anticipatory punch independently (r>0 kicks before the target — but inverts briefly on fast
reversals; tune ≤0.5 and test a <100 ms 180° reversal). Semi-implicit solver stability:
dt_max ≈ √2/ω_n — at SEADS dt=1/120 the ω_n ceiling is ~170 rad/s, no constraint at feel
frequencies. Camera POSITION must stay critically/over-damped (underdamped position = vection);
only the nose/aim RESPONSE carries the bounce. Latency evidence (CHI 2015 2702123.2702432):
>41 ms added lag measurably hurts aiming — ζ 0.5-0.6 at ω_n 15-25 adds ~30-50 ms to capture,
acceptable; smooth TRACKING at constant aim rate has no overshoot (the bounce lives at
capture/settle events only). (Sources: torontomu pressbooks 2nd-order specs; ryanjuckett
damped springs; theorangeduck spring-roll-call; Swink Game Feel ADSR.)

## 4. Freelook globe inertia

Standard kinetic model: v(t)=v₀·exp(−t/τ). iOS reference τ=325 ms (ariya.io kinetic-scrolling);
gltumble (prideout) same shape. **Vection hazard is real and documented** (camera moving after
the hand stops — LiS/RiME Steam complaints; Nature s41598-024-80778-4): mitigations are
MANDATORY: (1) short τ — **150-250 ms**, felt as a "heavier globe"; (2) **instant grab-on-touch**
(any input kills momentum — the DCC-tool convention); (3) velocity cap ~90°/s. SEADS's freelook
is an ORBIT around the plane (not first-person head), which weakens the vection signal — the
safe pattern is τ≈200 ms + instant-grab + cap.

## 5. Auto-level horizon (Chad: "may be a bad idea — research it")

Taxonomy + verdicts:
- (a) continuous per-tick camera roll to local-up — **KNOWN BAD here** (zenith pole; S7-cam2
  corpse) and the WT evidence says players hate continuous level-pull anyway.
- (b) idle-gated hysteretic (hand at rest + wings <30° + not near zenith) — plausible but
  easy to mis-gate into the WT "fighting me" failure.
- (c) discrete event-triggered — **already shipped (S7-hrz + orient verb) and is the
  industry-validated shape** (Outer Wilds landing mode, IL-2 PadlockViewForward).
- (d) rate-limited "horizon gravity" drift (k≈0.03-0.05 rad/s, triple-gated: off >30-45°
  bank, off within ~20° of zenith, off during mouse input) — VR guidance warns slow drift
  is often MORE nauseating than a snap; ≤5°/s if ever tried.
**Recommendation: (c) is the answer; (d) only as a low-priority triple-gated experiment;
(a) stays banned; auto-level FIGHTS knife-edge/inverted/banked-tracking unless gated off
exactly there.** (Sources: WT wiki instructor; Meta Horizon locomotion comfort; BeamNG/iRacing
horizon-lock precedents are for GROUND vehicles where world-up is fixed.)

## 6. Elevator responsiveness while aiming

Felt pitch response layers: K_theta (outer gain, ZOH-ceiling-bounded), rate clamps, error
curve shape, G/AoA ceiling shape — and the big one:

**AIM-RATE FEEDFORWARD** (Betaflight FF 2.0: FF = gain·d(setpoint)/dt): feed the aim
vector's own angular velocity ω_aim as an additive rate demand. "Feedforward cannot cause
oscillation no matter how much is added" — it acts only while the setpoint MOVES, so
steady-state is untouched. This is the mechanism that makes an instructor feel CONNECTED —
the nose leads the mouse instead of chasing its error. Missile guidance is the same math:
proportional navigation a=N·λ̇·V with N≈3 (predatory flies N≈3 falling to 1.5 close-in;
hawks mixed PN+pursuit) — N=2-2.5 is the conservative game range. ω_aim from raw deltas is
SPIKY: low-pass the DERIVED rate (~5-10 ms) — filtering the derived demand is loop-shaping
downstream of the raw aim, NOT the banned aim smoothing. (Sources: betaflight.com FF-2.0;
handwiki proportional-navigation; PMC6228472; PMC10265027.)

## V4 CANDIDATE MECHANISMS (ranked, felt-promise : risk)

1. **Aim-rate feedforward** — ω_des += K_ff·ω_aim_filtered (K_ff 0.3-0.5 start, PN-style
   N=2 max). Promise: connected, leading nose = the responsive elevator ask. Risk: noise
   (filter the derived rate), overshoot on fast swings.
2. **Underdamped capture (the basketball rim)** — target ζ≈0.5-0.55 at CAPTURE events only,
   gated (fires on a genuine settle after >50 ms deadzone approach, never during smooth
   tracking — else "wobble", the top WT complaint). Promise: one clean bounce, character.
3. **r-parameter anticipatory punch** — conservative r≤0.5, fast-reversal tested.
4. **Nonlinear error gain** — 1.5-2× small-error gain, saturating; re-verify the ZOH ceiling
   + AT-18 (double-count) after any curve change.
5. **Freelook globe inertia** — τ=200 ms + instant-grab + 90°/s cap (cosmetic layer — can
   land early, independent of the kernel).
6. **Horizon-gravity drift (d)** — LOW priority, triple-gated, expect rejection.
7. **PN outer loop** — mechanism 1 with the guidance-law gain framing; constant N first.
8. **Smooth AoA-ceiling rolloff** — resist-not-refuse at the envelope edge (verify the
   current filtered-AoA protection isn't already this).
9. **Camera spring split** — position strictly ζ≥1; only aim-direction may carry ζ≈0.8.
10. **Discrete overshoot-on-snap** — event-injected rate pulse on >30° flicks; may be eaten
    by G-protection (verify before building).

## TRAPS (literature + repo constitution)

1. ANY smoothing on mouse→aim — absolute ban stands; jitter fixes are upstream (DPI) or on
   DERIVED signals only.
2. Continuous auto-roll fighting the pilot (the WT failure).
3. Underdamp in camera POSITION (vection) — direction only, and gently.
4. Freelook inertia without instant-grab — mandatory, not optional.
5. High small-error gain without the ZOH re-check (K_theta_eff·dt vs K_w/I).
6. r>0 without the fast-reversal test (anti-phase first 30-50 ms).
7. Horizon drift at bank >45° / near zenith / during input.
8. Aim-rate FF on unfiltered mouse deltas (chatter injection).
9. PN N>4 (nose swings away on target reversals; biology caps ~3).
10. The rim-bounce firing during smooth tracking = "wobble" — gate at genuine settle events.

Full source list in the research round transcript; key URLs inline above.
