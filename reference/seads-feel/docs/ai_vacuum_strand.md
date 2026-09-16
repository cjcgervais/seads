# THE VACUUM STRAND — why the enemy fleet clumps outside the bubble and stops fighting

**Reported by Chad, 2026-07-26**, with `Game_loop_idea/ai_problem_.png` (the M-key
map at 1:16 left on the sudden-death clock).

> ★★★ **SUPERSEDED 2026-08-26 — RUNG S4. THE SCRAMBLE IS DELETED.** Chad:
> *"this games ai must not teleport but become skilled at deck flying. That is
> the answer"* / *"the dome being gone means they have to ride the deck they
> should have to respawn, but if they crash due to the moment of air loss and
> their context of orientation and speed, well then they respawn from their
> zone at the deck"*. Everything below is the ATTRIBUTION TRAIL for a mechanism
> that no longer ships. Its premise — "a plane placed in deleted air has no
> lift with which to execute an order, so only a state move can rescue it" —
> was true of a placement at 2000 m in vacuum and is NOT true of a placement at
> 100 m AGL: the global deck holds full air below 120 m AGL everywhere, and
> S1-DECK is the law that lets an aeroplane stay in it. Both teleport paths
> (the collapse hook and the crash-respawn cross-faction fallback) are gone;
> the replacement is `app::place_on_faction_deck`. See the S4 banner in
> `app/instructor_tick.h`.

**STATUS: ✅ SOLVED AND FLOWN (rung 5, SCRAMBLE ON COLLAPSE) — ★★★ AND REOPENED AND SOLVED AGAIN ONE LAYER DOWN, 2026-08-24: READ RUNG 6 AT THE BOTTOM FIRST, it supersedes the premise every rung here was built on.** Chad's verdict
2026-07-27: *"that was actually much better! destroyed two pumps and then made
it back to the valley and killed about 4 the last one I killed it read
victory!"* — the full loop now runs end to end: kill both enemy pumps -> their
dome collapses -> the surviving squadron scrambles into Valley air -> a real
final showdown inside the bubble -> VICTORY on the last kill. Rungs 1-4 are
kept below as the ATTRIBUTION TRAIL (three green-but-useless fixes and why they
failed); rungs 2-3 remain open as future work, not as bugs.

---

## WHAT THE SCREENSHOT SHOWS

| Readout | Value | Reading |
|---|---|---|
| Score | VALLEY 145 / SUDBURY 0 | Chad is winning outright |
| PUMPS | 2/4 | both SUDBURY pumps destroyed |
| Clock | `ENEMY ON THE CLOCK 1:16` | armed correctly on Sudbury |
| Bubbles drawn | **one** (the blue Valley oval) | Sudbury's dome is gone — correct |
| Enemy markers | a tight clump of 4 amber + 2 green dots **outside the Valley oval's east edge** | **the bug** |
| Player | white triangle, inside the oval at its east edge | — |

The whole surviving Sudbury fleet is bunched in open sky just outside the
Valley bubble, not closing, while a clock runs out that they lose by default.

---

## ROOT CAUSE — the win condition strands the loser in vacuum

This is **not** the leash bug fixed earlier the same day. It is a consequence of
three correct-in-isolation mechanisms composing badly:

1. **Chad's ruling:** both pumps lost ⇒ that faction's dome scale reaches
   **0** — `kFactionScaleFloor = 0.0`, and `build_faction_bubbles` zeroes the
   soft skirts too, so the bubble contributes *literally nothing*.
2. **`[atmosphere] enabled = true`** (activated the same day): air is now
   spatial. `sim::atm_frac_at` multiplies the bubble terms into density, and
   `sim::step` reads that density for **thrust, lift AND control authority
   together**.
3. Therefore, the instant Sudbury lost its second pump, **every cubic metre of
   Sudbury's sky above the global deck became unflyable** — for their own planes.

The global deck (`deck_agl_m = 120`, `deck_soft_m = 200`) keeps the planet
breathable **below ~120 m AGL** (the go-anywhere ruling), so air still exists —
but only down on the deck. Chad is at **ALT 1813 m**, and the fleet that came
for him is up there too, in near-vacuum, with no thrust, no lift and no
authority. They cannot turn, cannot climb, cannot close. They mush, drift
together into a clump, and die on the clock.

**The mechanic is self-defeating as specified:** the sudden-death rule says the
bubble-losing team must kill everyone to survive, but losing the bubble is
exactly what takes away their ability to fly and fight. It is unwinnable by
construction, and it *looks* like broken AI.

### Why the existing air guard doesn't save them

`drone/drone.h` already has the flyable-air ceiling (`avoid_air_frac_full 0.7` →
`avoid_air_frac_hard 0.25`), added for Chad's earlier "don't chase into vacuum
and mush" report. But it is **one-directional**:

```
if (target_gamma > 0.0 && env->atm) { ... target_gamma *= t; }   // climb only
```

It fades *commanded climb* to zero and explicitly never restricts descent. A
drone **already** in thin air therefore commands **level flight in vacuum,
forever**. It stops the fleet flying *into* the problem; it has no notion of
flying *out* of it. There is no air-seeking behaviour anywhere in the fleet AI.

---

## RUNG 1 — AIR-SEEK DIVE (LANDED 2026-07-26)

Below the hard floor a drone now actively **dives for breathable air** instead
of coasting level in vacuum. Because the global deck guarantees air below ~120 m
AGL *everywhere on the planet*, "down" is always a valid answer — no pathing to
a specific bubble required, so this is robust to any bubble state.

- New dial `DroneParams::avoid_air_dive_gamma` (default `0.35` rad ≈ 20° nose
  down), ramping in as `frac` falls below `avoid_air_frac_hard`.
- Applied as `target_gamma = min(target_gamma, seek)` so it can only ever add
  descent, never fight a steeper dive the pursuit already wants.
- Still gated on a live `env->atm`, so the legacy / frozen-kernel fleet path
  stays **bit-identical** (the superset firewall).
- The terrain-avoidance block still runs **after** and overrides everything, so
  air-seek can never fly anyone into the ground.

Net effect: a fleet whose dome collapses sinks to the deck, regains authority,
and can fly and fight again — down low, which is a legible and dramatic place to
lose a war from.

---

## OPEN — RUNG 2: give the loser somewhere to go

Air-seek returns their *authority*. It does not give them a *plan*. Once on the
deck they still have to reach Chad, who is at altitude inside a dome they cannot
enter without starving again.

Chad's own spec already contains the answer: **"the tunnel and black stope
always have 100% air."** The tunnel is the loser's lifeline — a guaranteed
full-air corridor running from Murray to Errington, straight under the war. The
intended shape is that a faction which loses its sky **goes underground** and
strikes from the tunnel mouths. That is the same machinery rung 1 of
`docs/strike_mission_plan.md` needs, so build them together.

## OPEN — RUNG 3: is the sudden-death rule what Chad actually wants?

Worth a ruling before more code. As specified, losing your bubble costs you your
sky, which costs you the fight you must now win. Options:

- **(a) Keep it brutal.** Losing the dome *is* the death spiral; the tunnel
  (rung 2) is the only comeback path. Most dramatic, hardest.
- **(b) A vestigial dome.** The loser keeps a small breathable core (a floor of
  ~0.15 rather than 0) — a last stand around their own field. Softest change,
  costs the clean "the bubble is GONE" reading Chad asked for.
- **(c) Thicken the global deck** while a clock is live, so the whole planet has
  a fightable low lane. Preserves the visual, changes the felt war.

**Recommend (a) + rung 2** — it is the only one that uses the tunnel Chad
already built and already ruled must always hold air.

---

## LESSON (for `docs/lessons.md`)

A win condition that removes a faction's **air** removes its **authority**, and
authority is what the losing side needs to execute the comeback the same rule
demands of it. When a mechanic's penalty and its escape clause draw on the same
resource, the rule is unwinnable by construction — and it presents as an AI bug
(planes clumping, not engaging), not as a rules bug. Trace a "the AI stopped
fighting" report to the *plant* before the *brain*: here the brain was issuing
sane commands into air too thin to execute them.

Second, narrower: a guard written to stop entry into a bad state
(`avoid_air_frac_*`, climb-only) is **not** a guard that can leave it. Any
"don't go there" limiter should be paired with a "get out if you're already
there" behaviour, or the first time something else puts an agent in that state
it is stuck permanently.

---

## SECOND REPORT (2026-07-26 21:00, `ai_problem2.png`) — rung 1 was NOT ENOUGH

Chad flew the air-seek build (`build-play/seads.exe` 20:38) and reported: **"I
got a victory but there was no final showdown, they loitered outside the
bubble."** Map at 5:45 on the clock, 5 planes, VALLEY 120 / SUDBURY 0 — the same
clump of amber+green markers parked off the Valley oval's east edge, in the
Sudbury home region.

**Ruled OUT this round: the brain.** `drone::assign_engagements` was read in
full. Under `all_vs_player` the app hands it `engage_range = disengage_range =
kFurballRangeM (1e9)` and `max_engaged = fleet size`, so every non-inert drone
qualifies and every one gets a slot. They ARE engaged and targeting the player.
The defect is downstream of target selection — in what the plant can execute.

**Attributed to the MUSH ZONE.** Rung 1 keyed the seek to
`avoid_air_frac_hard` (0.25), but the climb fade runs from `avoid_air_frac_full`
(0.7) down to `_hard`. That left the whole **0.25 – 0.7** band with climb
already faded to ~zero and *no seek yet*: a drone there can't climb, isn't
trying to descend, and wallows at low speed and low authority. From a top-down
map that is indistinguishable from loitering — and the ~0.3-ish band is exactly
where the space just outside a dome edge sits, thanks to the 1200 m soft skirt.

**Rung 1b (landed):** the seek is now keyed to `avoid_air_frac_full` — anything
short of full air heads down toward it, strength `(1 - t)` reusing the existing
ramp so the dive fades to exactly zero at full air (no boundary snap).

### ⚠ UNVERIFIED — this has not been flown

Gate is green (879/879) and the reasoning is sound, but the previous fix was
also sound-and-green and did not change the felt behaviour. **Do not treat rung
1b as done until Chad flies it.** If they still loiter, stop guessing and
instrument: the honest next step is per-drone telemetry (position, `atm_frac_at`
at that position, `engaged`, commanded vs achieved gamma, speed) for the stranded
fleet. Two speculative fixes in a row is the point at which measurement is
cheaper than another guess.

### Free dials to try first, no code

- `[atmosphere] deck_agl_m` 120 → a few thousand: gives the whole planet a
  fightable low lane, so a domeless fleet can still bring a final showdown. One
  number, instantly reversible, and it tests the whole diagnosis in one fly — if
  raising the deck produces a showdown, the vacuum-strand attribution is
  CONFIRMED and the only remaining question is how to do it elegantly.
- `avoid_air_dive_gamma` 0.35 → steeper, if they descend but too slowly.

---

## RUNG 4 — CONFINEMENT (Chad's ruling, 2026-07-26): "confine their programming to bubble"

Supersedes the air-threshold approach as the PRIMARY mechanism. Rather than
detecting thin air and reacting to it, bandits are simply never allowed out of
breathable sky in the first place.

The leash is now **always on** — the furball exemption and the engaged exemption
added earlier the same day are both **gone**. What changed instead is WHICH
ellipse confines a drone:

| own dome | leashed to |
|---|---|
| alive | its own dome (unchanged behaviour) |
| **extinct** | **the OTHER faction's dome** |

That second row is the whole fix. A faction with both pumps gone owns no air,
and the only breathable sky on the planet is the enemy's — which is where the
player is. **Confinement now drives the invasion instead of preventing it.**
Leash and pursuit point the same direction, so the standoff that produced the
bubble-edge clump is structurally unrepresentable, and the endgame is forced to
happen inside the one surviving dome. It also makes the vacuum strand
impossible by construction: a bandit is never asked to hold station somewhere it
cannot fly.

Both domes gone => `a` stays 0 => leash off by its own documented contract
(`BubbleLeash`: "enabled=false (a<=0/b<=0) => no leash").

The air-seek dive (rungs 1/1b) stays in as a **backstop**, not the primary
mechanism — it still catches a drone that ends up thin-aired some other way
(above a dome's ceiling, mid-transition as a dome collapses under it).

`drone::tick` already exempts the tunnel dispositions from the leash
(`in_tunnel_mode`), so `docs/strike_mission_plan.md` rung 1's Murray/Errington
sortie is unaffected and still needs no leash special-casing.

### Still unflown

Gate green (879/879). This one is structurally different from rungs 1/1b — it
removes the failure mode rather than reacting to it — but it is still unflown,
and two sound-and-green fixes have already failed to change the felt behaviour.
If bandits now sit on the INSIDE of the player's dome edge rather than closing,
the suspect is the leash's own `steer_frac` / hysteresis band, not the target
selection.

---

## RUNG 5 — SCRAMBLE ON COLLAPSE (Chad's ruling, 2026-07-26). THE ACTUAL FIX.

Rung 4 (always-on confinement) **also failed** — "they still got stranded out
there". That is three sound-and-green fixes in a row with no felt change, and
the third failure is what finally named the wall:

> **Every one of those fixes issues an ORDER. A plane in vacuum cannot execute
> an order.** The leash only edits `target_bank`; air-seek only edits
> `target_gamma`. With `rho ~ 0` there is no lift and no thrust to bank or climb
> WITH — only the `q_att_floor` attitude authority, enough to point the nose and
> nothing else.

And the specific reason confinement could not work, however correct the rule:
**the dome is not flown out of, it is DELETED around aircraft that were legally
inside it.** A rule that prevents leaving cannot rescue a plane that was never
let out.

So the fix has to change STATE, not commands. Chad ruled: **scramble.**

`app::scramble_to_surviving_air` (`app/instructor_tick.h`), fired one-shot per
faction from the conquest tick block (3c) the tick that faction's
`radius_scale` reaches 0:

- every living plane of the collapsed faction is relocated into the SURVIVING
  faction's dome — the squadron scrambling for the last breathable air;
- placed at **0.6 of the surviving dome's semi-MINOR axis** from its centre, so
  they are inside the ellipse in every bearing whatever its orientation;
- **deterministic, no rng, no clock** (the seam forbids both) — golden-angle
  bearings by drone slot, the same Fibonacci discipline as
  `drone::spawn_state`;
- nose pointed at the dome centre, and — critically —
  `drone::level_state_at` reseeds them to the fleet cruise `dp.speed`. They will
  have mushed to near-zero airspeed while stranded, so **the speed reseed is
  what actually makes them able to fight again**;
- `prev = curr` so the render interpolator draws no streak across the jump, and
  `leash_engaged = false` so the containment latch re-arms clean.

It composes with rung 4 rather than replacing it: scramble puts them INSIDE the
surviving dome, and always-on confinement then keeps them there. Rungs 1/1b
(air-seek) remain a backstop for thin air reached some other way.

**Known cost, accepted by Chad:** this is a visible teleport. Watching the map
at the moment a dome collapses will show that faction's markers jump.

### Unflown

Gate 879/879. Structurally different from rungs 1-4 (it changes state rather
than issuing an order into air that cannot carry it), so it cannot fail in the
same way — but it is still unflown, and this thread's record is three for three
the wrong way. Watch for: markers jumping into the surviving oval on collapse,
then actually closing.

### LESSON (add to docs/lessons.md)

An AI order is only as good as the PLANT'S ability to execute it. Three
successive fixes to a "the AI stopped fighting" report were all command-layer
(target selection, bank, gamma) while the real defect was that the aircraft had
no lift or thrust to act on any command — the brain was issuing sane orders into
air too thin to carry them. When a behaviour fix lands green and changes
nothing FELT, stop refining the command and ask whether the plant can execute
the command at all. Corollary: a containment rule ("stay inside X") is
unfalsifiable protection if X can be DELETED around the agent — guard the
transition, not just the boundary.

---

## ★★★ RUNG 6 — THE DECK WAS NEVER THERE (2026-08-24, enemy-AI rung E16)

**The whole strand above is correct and was aimed at the wrong layer.** Rungs
1/1b built an air-seek dive on one premise, stated in their own comment:

> "'Down' is always a valid answer because the global deck keeps the planet
> breathable below ~deck_agl_m AGL EVERYWHERE (the go-anywhere ruling)."

**That premise was false by the time it was written, and nothing ever asked.**
`sim::atm_frac_at` measures the deck from the SPHERE — `atm_falloff(alt -
deck_agl_m, ...)`, `alt` = altitude above R — while `game.toml` describes the
same dial as "full air below this height **above the surface**". On the bare
sphere R6 shipped on, those are the same sentence. The real DEM then landed
underneath (`relief_scale_m = 350`) and, measured on the shipped
`assets/sudbury_dem.png`: **69% of the surface stands above the full-air deck
and 26% above its fade.** Over most of the planet the go-anywhere breathable
floor is *underground* — you cannot land outside a bubble there, and the
air-seek dive has been aiming the fleet at air that does not exist.

### THE MEASUREMENT (probe P-H, docs/ENEMY_AI_E1_E2_SPEC.md §E16)
The corridor between the two domes, sampled every 50 m, dead length (air<0.25):
**0.00 km at 200 m altitude, 3.10 km at 350 m, 4.25 km at 2500 m, 9.35 km at
4000 m.** The curve is monotone — a dome is widest at its base — which also
killed the plausible fix (a ballistic HOP: climb high, coast across). Higher is
a WIDER gap, and the hop measured worse.

### THE FIX
`[atmosphere] deck_terrain_relative` — the deck measured from
`env->ground->radius_at`, which is what the dial always claimed. With it on and
**the AI not touched at all**: enemy terrain crashes 37 → 18 in a 22-minute
match (1.65 → 0.80/min, under Chad's signed 0.94), deaths in thin air **34 →
0**, raid-sortie crashes 19 → 2 — and the enemy simultaneously got *better*
(on-station 42.0 → 46.2 s, both player pumps down). Rungs 1/1b now do what they
were designed to do, because there is finally something down there to find.

### THE LESSON (add to docs/lessons.md)
Rung 5's lesson was "an AI order is only as good as the plant's ability to
execute it". **Rung 6's is one layer under that: a plant law is only as good as
the WORLD CONTRACT it was written against, and a world contract can be repealed
by an asset bake with nothing going red.** Five successive AI fixes — three
here, two more in the E10/E11 ladder — were all sound, all green, and all
aimed at a fleet that was obeying orders correctly into a world that had
silently stopped honouring its own documented floor. When a behaviour keeps
resisting sound fixes, stop fixing the behaviour and go and MEASURE the
environment it is being asked to survive; and when code and config describe the
same dial in different frames, that is not a wording difference, it is a bug
waiting for a bake.
