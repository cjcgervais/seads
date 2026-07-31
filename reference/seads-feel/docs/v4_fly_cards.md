# V4 FLY CARDS — one rung per fly, Chad's stick judges (program.md §loop step 6)

Build: `auto/kernel-v4` in `D:\flight_sim2\seads-v4` (`cmake --build build --config Debug`,
run `.\build\seads.exe`). Every dial lives in `config/controller.toml`; 0 = that mechanism
structurally OFF (bit-identical v3 kernel).

---

## CARD 1 — S-aimff: aim-rate feedforward ("responsive elevator while aiming")

**What changed:** while your mouse MOVES, its rate is fed straight into the nose's rate
demand (`ω_des += gain·ω_aim`, pitch + yaw). The nose LEADS the moving aim instead of
chasing its error. A still mouse is bit-identical to the sealed v3 kernel — this acts only
during motion, so trim, parking, the deadzone, and every capture are untouched at rest.

**Fly it:** get on a drone's six and TRACK — smooth continuous mouse pursuit, gentle S-curves,
barrel-roll lines. Then slow zoomed tracking (RMB) — the place the old kernel lagged behind
your hand. Instrument says: tracking lag −26% at V=140 (246→181 ms), sine phase 50°→33°,
zero new oscillation, reversal transients DOWN with no anti-phase kick.

**Ask yourself:** does the nose feel CONNECTED to the hand in motion — "responsive elevator"?
Any new buzz or jitter during smooth tracking (the trap the event gate protects against —
should be none; report immediately if felt)? Any weirdness at the instant you stop the hand?

**Dials (`[aim_ff]` in controller.toml):**
- `gain` — shipped **0.3**. More lead: 0.5. Full-lag-cancel feel: 0.8 (do not exceed 1.0
  without a session — PN discipline). 0 = off (bit-identical).
- `tau` — shipped **0.01 s** (rate de-spike). Leave alone unless gain>0.5 chattered.

**Honesty walls:** the FF is bounded by the same G/AoA envelope as everything else — at low V
a fast hand still outruns the wing (protection wins, by design). The yaw ceiling bounds
pointed+FF; coordination keeps its documented ride-above exemption.

**Verdict line (fill in):** `CARD-1: APPROVED / retune gain to __ / REJECTED because __`

---

## CARD 2 — S-rimshot: the full-traverse rebound capture (your 4-iteration spec)

**REBUILT 2026-07-17 — S-rimshot v2 UNIVERSAL (your flick-misconception correction).**
Your ruling landed verbatim: the carry-through-rebound is the pointing law's ARRIVAL
behavior — **any deflection at all, always, even mid-track**. There is no flick gate, no
size threshold, no park gate; the machine never reads whether the mouse is moving. Your two
follow-up rulings are in too: a re-flick mid-bounce **abandons and chases instantly** (and
the new aim's arrival takes its own fresh bounce), and every catch-up gets exactly ONE
bounce (glued lockstep pacing is the only quiet state — no catching-up means nothing to
carry).

**What changed:** whenever the nose is genuinely ARRIVING at the crosshair — the error
inside the final ~5° endgame where the pointing law owns the approach, and the law asking
for less rate than the nose actually carries — the demand is HELD at the incoming rate
through the crossing, out toward the far rim of the reticle ring, then the full-authority
arrest drops it DEAD at center (the τ-surface release you already approved conceptually).
The entry is rate-continuous by construction (the engage-tick demand exactly reproduces the
nose's real rate — no step, pinned to machine precision), and the rebound plays out relative
to a MOVING crosshair (frame-carried), so mid-track catch-ups bounce around the live aim.

**Fly it:**
1. **The headline — a small nudge:** cruise, park the aim ~2° off the nose. The nose must
   carry through, tap the far side of the ring, and snap to dead center — the same basketball
   drop a 45° flick gets (instrument: 2°@V220 rim=0.85, one reversal, 75 ms drop).
2. **Big flicks** 45°/90°, then park: through the aim, out, one beat back to center.
3. **Mid-track:** track a drone, let the nose catch the moving crosshair — it should bounce
   once around the LIVE crosshair each catch-up, then glue back into the track (sine
   instrument: tracking actually TIGHTENS — phase 33°→25°, mean err 3.7°→3.0°).
4. **Re-flick mid-bounce:** yank the aim somewhere new while the nose is mid-swing — it must
   abandon instantly, chase, and bounce fresh at the new arrival.
5. **Smooth-track wobble sentinel:** long steady sweeps must feel EXACTLY like before
   (instrument: bit-identical, zero engages — but your hand is the judge; report ANY new
   weave immediately).

**Honesty walls (named, measured):**
- **Lateral/banked arrivals don't get the crisp bounce.** A sideways flick resolves through
  BANK; its endgame creeps in at 1–3°/s via the bank/lean standoff — there is physically
  nothing to carry (a "bounce" at that rate would be a half-second crawl). The event takes
  ONE attempt, then regroups and lets the legacy law converge. The bounce is a
  pitch/direct-aim phenomenon at real closing rates.
- **After such a failed banked attempt, small re-arrivals (up to ~6°, both axes) don't
  re-bounce** until the aim moves away past that scale — the fence that killed a measured
  4× churn. If this ever FEELS like dead bounces mid-fight, the pre-agreed one-dial fix is
  an aim-motion-gated clear (say the word, it's a one-session fold).
- The far swing overruns the ring at full carry (45°@V140: 1.5× the ring — stopping distance
  is physics; `carry` is the trade dial). Post-arrest there is a ~0.1–0.15° past-center
  micro-drift that decays (~2 px for a beat — the honest carried momentum; the old build
  under-carried by exactly the curvature term).

**Dials (`[capture]`):** `carry` (**1.0** shipped; **0 = the kill-switch**, bit-identical
v3), `engage_frac` (0.95 — lower = later/gentler engages; 0.7 is STILLBORN, loader-blocked),
`return_w` (60°/s drop cap), `handback_frac` (1.1 — re-flick sensitivity), `break_frac`
(1.25 — yank sensitivity), `w_eps` (0.4°/s engage floor), `rim_frac`/`circle_deg`
(0.85/0.55 — the ring geometry).

**v3 POOL BALL UPDATE (2026-07-17, same day — supersedes the walls above):** your exact
spec is now the measured behavior. Full speed through the center (zero pre-braking,
instrument-verified); the glance lands DEAD ON the inside wall — rim 0.95–1.08 across
20°–120° deflection, V100–220, pitch AND yaw (was 1.3–1.8× and variable: the brake now
fires predictively so the stop lands on the rim every time); slow/gentle arrivals seek
center directly and briskly (0.13–0.16 s, no creep, no manufactured dart-out); the snap
home is the physical maximum with the dead-blow stop (drift ≤ 0.04°). Lateral flicks get
the identical pool-ball beat (the "banked creep" behavior is dead — a 20° yaw flick
crosses, kisses the wall at 0.99, snaps home in 50 ms). Smooth tracking untouched
(bit-identical sweeps), and sine tracking measurably TIGHTER. Two things only your stick
can judge: coordination is briefly compensated inside the sub-second bounce window (the
crab-on transient), and the 2° nudge direct-seeks rather than painting a rim glance (your
Q3 ruling — say the word if you want the flourish there too).

**Verdict (Chad, 2026-07-17, on the stick):** `CARD-2: APPROVED` — **"you did it, its perfect way more responsive, exactly what I wanted."** The pool-ball capture is the v4 kernel's mouse-aim dynamic.

---

## CARD 3 — S-globelook: freelook globe inertia ("move it like a globe with the hand")

**What changed:** while freelook is HELD, the orbit carries momentum. While your mouse moves,
the hand owns the globe 1:1 exactly as before (that path is bit-identical). When the hand
stops, the view keeps gliding at your last hand rate and decays out over ~0.2 s — flick the
globe and it coasts. Release still eases home exactly as today (the coast dies with release);
respawn/orient/focus-loss all wipe it. Cosmetic only — nothing here ever feeds mouse→aim.

**Red-team fold (the dwell):** the coast now ENGAGES only after **75 ms of true stillness**
(~4–5 frames — masked by the flick itself). Without it, an integer mouse during a SLOW drag
faked "stillness" every 0-count gap frame and the globe self-amplified the drag (measured up
to 3.7×, and a lone 1-px nudge glid 2.4° instead of 0.2°). With the dwell that contribution
is exactly zero (ctest-pinned); slow deliberate freelook sweeps are 1:1 with the hand.

**Fly it:** hold freelook and throw the view sideways, then FREEZE the hand — the globe
should glide on and settle in about a fifth of a second, like spinning a desk globe with
drag on it. Then a slow deliberate look-around — it must feel EXACTLY like the old 1:1
freelook (no float, no gain change, no per-pixel glide). Coast into the pitch floor — the
wall should absorb the spin dead (no winding behind it). Release mid-coast — the ease-home
must be today's, no extra spin.

**Ask yourself:** does the flick-and-coast feel like a globe with the hand? Is slow tracking
still perfectly 1:1 (the dwell's whole job — report ANY float on a slow drag immediately)?
Is the ~5-frame coast onset after a flick invisible (it should hide inside the flick)?

**Dials (`[freelook]` in controller.toml):**
- `inertia_tau` — shipped **0.2 s** (research band 150–250 ms). Heavier globe: 0.25.
  Lighter: 0.15. **0 = whole mechanism structurally OFF** (bit-identical legacy freelook).
- `inertia_cap` — shipped **90°/s** coast/seed ceiling (a violent flick can't spin the view
  into vection).
- `inertia_dwell` — shipped **0.075 s**. Do not set to 0 (re-opens the integer-mouse
  staircase amplification); raise toward 0.1 only if a slow drag ever still floats.

**Verdict line (fill in):** `CARD-3: APPROVED / tau to __ / cap to __ / REJECTED because __`

---

## CARD 4 — COMPOSITION: the whole v4 candidate set together ("buttery smooth, something I can settle in to")

**What this card is:** all three mechanisms ship ON in this build's `controller.toml`
(`aim_ff.gain 0.3` + `capture.carry 1.0` + `freelook.inertia_tau 0.2`) — every instrument
number on cards 1–3 was in fact measured with the full composition live, and the whole
342-test gate runs against it. This card is the SETTLE-IN fly: not one mechanism, the sum.

**Fly it:** a full sortie. Take off from a fight-ready spawn, chase a drone through smooth
tracking (card 1's regime), break off with a hard 90° flick and let it rimshot (card 2),
check six with a freelook flick-and-coast (card 3), come back, repeat — then just FLY for
five minutes without testing anything.

**Ask yourself (the program's acceptance frame):** does it feel SETTLED? Do the three
mechanisms compose — the FF lead never fights the rimshot event (the aim is parked during
the event, so the FF is quiet there by construction — pinned), the globe coast never touches
the plane? Is there ANY regime where the kernel feels different from v3 when your hand is
still (there must not be — every still-hand path is bit-identical)?

**If one mechanism sours the mix:** each has its own 0 = structurally-off dial — kill one,
re-fly, attribute. One dial at a time.

**Verdict line (fill in):** `CARD-4: SETTLED / __ mechanism needs __ / composition issue: __`

---

## Not built, on purpose — the auto-level horizon answer (ask 5)

Research verdict (docs/v4_research.md §5): the DISCRETE verbs you already fly (orient
double-tap, S7-hrz release-roll) are the industry-validated shape; continuous auto-level is
the known failure (War Thunder's #1 instructor complaint — "always fighting me"; the
S7-cam2/S7-mouselevel graveyard here). The rate-limited "horizon gravity" variant (≤5°/s,
triple-gated) is buildable on request but the research expects rejection. Say the word and
it becomes a rung; the recommendation is to spend the next session's feel budget elsewhere.
