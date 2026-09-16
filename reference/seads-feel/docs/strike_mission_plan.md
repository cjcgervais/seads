# STRIKE MISSION — the AI's standing sortie + the zero-sum air war

**Status:** PLAN. Rungs 0a–0d LANDED (gate 875/875). Rungs 1–4 NOT BUILT.
**Ruled by Chad, 2026-07-26.** Written with 2% budget left, so a fresh session
executes rungs 1–4 from here without re-discovering the codebase.

---

## 0. CHAD'S SPEC (verbatim intent, do not re-interpret)

> "make them all my enemy. They start in sudbury, all behave the same way they
> get through the murray tunnel to the black stope, destroy the valleys
> underground pump, then they fly through the errington tunnel (due nw) and
> then head to the blue (valley's surface pump) my job is to stop them and
> destroy their pumps, by the way the opposite team of the destroyed pumps team
> gets the air volume on their side and grow their home bubble (zero sum for
> every unit of air). Air leaves on a timer but it is fairly quick. The tunnel
> and black stope always have 100% air."

Decoded into contracts:

| # | Contract |
|---|---|
| C1 | Player flies **VALLEY**. All 10 mavericks are **SUDBURY** = all hostile. |
| C2 | Every maverick flies the **SAME** sortie — no per-pilot role split. |
| C3 | Sortie: spawn Sudbury → **Murray tunnel** → **black stope** → kill the **VALLEY DEEP pump** → **Errington tunnel** (due NW) → kill the **VALLEY SURFACE pump** (blue). |
| C4 | Player's job = intercept, and kill the two **SUDBURY** pumps. |
| C5 | Air is **ZERO SUM**: a destroyed pump's air volume TRANSFERS to the opposite faction. Total conserved. |
| C6 | The transfer is **animated on a timer**, "fairly quick" — not a snap. |
| C7 | Tunnel + black stope are **always 100% air**, regardless of the bubble war. |

---

## ⚠ THE ONE BLOCKER — `[atmosphere] enabled = false`

`config/game.toml:135` ships **false**. That means `env.atm` is null, which
means:

- `app::rebuild_conquest_bubbles` returns immediately (`atm_field == nullptr`)
- `sim::atm_frac_at` takes the frozen-kernel path and **ignores every bubble**
- the entire air war — grow, shrink, zero-sum, timer — has **no physical effect**

C5/C6/C7 are all unreachable until this is `true`. It is deliberately flagged
**"⚑ ACTIVATION IS CHAD'S (the R6 fly HALT)"**, so this plan does **not** flip
it unilaterally. **Chad: this is a one-word change and everything below depends
on it.** Flip it, fly it, confirm the thin-air feel outside the bubble is what
you want, then rungs 1–4 have something to act on.

(C7 is already IMPLEMENTED and correct — see rung 0d.)

---

## RUNG 0 — LANDED 2026-07-26 (gate 875/875)

### 0a. The drawn bubble never moved — **the actual reported bug**
`render/draw.cpp` built the M-key map ovals from the **baked** constants
`world::kValleyMajorRadiusM` / `kSudburyMajorRadiusM`, never from conquest
state. The oval Chad was staring at was hardcoded at base size and could not
move no matter what happened to the pumps.

Fixed: new `FrameInfo::conquest_radius_scale[2]` (`render/draw.h`), populated
from `cq.state.radius_scale` (`app/main.cpp`), consumed through
**`world::faction_ellipse`** — the *same* growth-scaled + clamped geometry
`build_faction_bubbles` feeds the plant, so the drawn outline can never fork
from the real air edge. At `{1,1}` it reproduces the old curve exactly.

### 0b. The shrink was floored at 40%
`combat::kFactionScaleFloor` was `0.4` ("never a vanishing bubble", fly-2
ruling C) and `shrink_*_frac` was `0.25`. Two pumps took a dome 1.0 → 0.75 →
0.5, then the floor held it up forever. Now floor `0.0`, frac `0.5`: **1 pump
lost = 50%, both lost = gone.** `world/faction_bubbles.h` also zeroes the soft
skirts on an extinct faction (a 0-radius bubble still carried a ~1.2 km
`edge_soft` blob of air).

### 0c. Faction + mode flips
- `player_faction = "sudbury"` → **`"valley"`** (C1). The shipped-value test and
  its mutation leg in `test/unit/test_faction_bubbles.cpp` were updated.
- `all_vs_player = true` → **`false`**. This flag exists to glue the fleet to
  the player and **explicitly suppresses the tunnel-run diversions** — it is
  structurally incompatible with C3. Do **not** flip it back on to "make them
  aggressive"; that silently cancels the whole strike mission. Aggression
  belongs in rung 1's state machine.

### 0d. C7 was already done
`sim::atm_frac_at` (`sim/aero.h`, the "T1 — the tunnel union term") already
multiplies in `env->tunnels->signed_distance()`, and `TunnelNet::signed_distance`
mins over the **arena** (black stope) as well as the bores. Inside the net,
`u_t == 1.0` exactly → full air restored regardless of the bubble war.
`[tunnel] enabled = true` ships. **No work needed — but it is invisible until
the `[atmosphere]` blocker above is lifted**, because with `env.atm` null there
is no bubble war for it to override.

---

## RUNG 1 — THE SORTIE STATE MACHINE (the headline; biggest)

**Goal:** C2 + C3. All 10 mavericks fly one shared objective chain.

### What already exists (reuse, do not rewrite)
`drone/maverick.h` has a working bore autopilot:
`Mode::{PATROL, TRANSIT, DIVE_IN, RUN, CLIMB_OUT}` with the curvature-ff
`bore_track` law shared by `RUN` and the in-net `CLIMB_OUT` continuation.
Memory records **full runs with clean break-outs both ways**. `in_tunnel_mode()`
(line ~335) already exempts these modes from the bubble leash.

### What is missing
1. **A mission sequencer above `Mode`.** Today a run is a *diversion* that hands
   back to `PATROL`. C3 needs an ordered chain with memory of which leg is done:
   `TO_MURRAY → RUN_MURRAY → STRIKE_DEEP → RUN_ERRINGTON → STRIKE_SURFACE → (done//patrol)`.
   Add `MaverickState::Objective` alongside `Mode`; `Mode` stays the *how*,
   `Objective` becomes the *why*. Keep it a pure function of state — **no clock,
   no rng** (the `age_ticks` phase seam is the precedent).
2. **Which mouth.** `RUN` currently picks a direction; C3 pins entry at
   **Murray** and exit at **Errington (due NW)**. Anchors:
   `world::kTunnelMouthErrington`, and the Murray anchor near
   `46.5144,-81.0657` (see `docs/tunnel_handoff.md`).
3. **AI-vs-DEEP-pump damage — this does not exist.** `combat/raid.h:19` says
   plainly: *"Deep-pump raiding via the tunnel is OUT of scope this rung … a
   raider only ever targets the player-faction SURFACE pump."* `RaidOrder` +
   `raider_on_station` are hardcoded to the surface pump index (`pumps[pf]`,
   `app/instructor_tick.h` block 4).

### The cheap way to do #3
Generalize, don't duplicate. `combat::raider_on_station(s, pump_pos, rp)` is
already position-generic — only the **call site** is hardcoded. In
`app/instructor_tick.h` block (4):
- replace the single `cq->state.pumps[pf]` target with a loop over **both**
  Valley pumps (surface index `pf`, deep index `pf + 2` — see `make_pumps`
  ordering: `[0]` Valley surface, `[1]` Sudbury surface, `[2]` Valley deep,
  `[3]` Sudbury deep);
- let `RaidOrder::target_pos` come from the **Objective** (deep pump during
  `STRIKE_DEEP`, surface during `STRIKE_SURFACE`) instead of always the surface;
- keep routing damage through the **shared `combat::damage_pump`** path so score
  / grow / shrink / events / bubble rebuild can never fork.

`RaidParams::raid_range_m` (800 m) is a surface-strike envelope; the stope is
tighter — give the deep strike its own smaller range or it triggers from inside
the rock.

### Test legs (mutation-verify each)
- Objective chain advances only on a real kill, never on a timer.
- A maverick that dies mid-chain and respawns re-enters at `TO_MURRAY` (respawn
  resets — the `d.leash_engaged = false` precedent, `drone/maverick.h:842`).
- Deep strike cannot fire from outside the arena (range leg).
- Every gate **hysteretic** — the house hard rule.

---

## RUNG 2 — ZERO-SUM AIR (C5)

Today grow and shrink are **independent** fracs: destroyer `+= 0.15`, victim
`-= 0.5`. That is not conserved — it is two unrelated dials.

Make it a **transfer**. In `combat::damage_pump`, replace the two independent
updates with one conserved move:

```
const double moved = params.transfer_frac;          // e.g. 0.5 == half a dome
const double take  = std::min(moved, cs.radius_scale[victim]);
cs.radius_scale[victim]    -= take;
cs.radius_scale[destroyer] += take;                 // exactly what was lost
```

Invariant to pin: `radius_scale[0] + radius_scale[1]` is **constant across every
pump death** (sum starts at 2.0). That single test leg is the whole contract and
it kills any future re-forking of the two dials.

Caveats to decide at build time:
- "Air **volume**" is not "air **scale**". An ellipse's area goes as `a·b ∝ s²`,
  so conserving `s` is *not* conserving volume. Chad said "zero sum for every
  unit of air" — if he means literal volume, transfer `s²` and take the sqrt
  back. **Ask him; do not guess.** Conserving `s` is simpler and reads the same
  on the map at these sizes.
- The `kFactionBubbleRadiusMaxM` clamp in `build_faction_bubbles` can *silently
  eat* transferred air at the top end (the destroyer hits the cap and the
  victim's loss vanishes). Either raise the cap or account the clipped
  remainder — otherwise "zero sum" is a lie the first time someone caps out.

---

## RUNG 3 — THE TIMER (C6)

"Air leaves on a timer but it is fairly quick." So `radius_scale` becomes a
**target**, with a displayed/actual value chasing it.

- Add `radius_scale_target[2]` to `ConquestState`; `damage_pump` writes the
  **target**, never the live value.
- Advance the live value toward the target in the **sim tick** (`app::tick`),
  **never on a frame clock** — AT-9 frame-rate invariance is a hard rule here,
  and `rebuild_conquest_bubbles` is already documented as tick-driven for
  exactly this reason.
- Rate as a config dial in `[conquest]`, e.g. `air_transfer_rate_per_s = 0.35`
  (≈3 s for a half-dome move — "fairly quick"). **Linear rate, not exponential
  decay**: a decay never arrives, and the zero-sum sum invariant stays exact
  under a symmetric linear move (give and take the *same* clamped delta each
  tick, or conservation drifts).
- Rebuild the bubbles every tick the value is still moving, not just on death.

---

## RUNG 4 — POLISH / FLY

- HUD: which objective the fleet is on; "DEEP PUMP UNDER ATTACK" alongside the
  existing surface warning (`conquest_pump_under_attack` / `conquest_raided_pump`
  already exist in `FrameInfo`).
- Map: draw the sortie path so Chad can read where to intercept.
- Fly card in `docs/` per house process; **Chad flies it**, one dial at a time.

---

## HOUSE RULES THAT BIND THIS WORK

- **Never touch `sim/` or `control/`.** Frozen kernel v4/v5. The air field is
  read *by* the plant through `sim::atm_frac_at`, which is already built — rungs
  1–4 are all `combat/`, `drone/`, `app/`, `render/`, `config/`.
- Every gate/latch **hysteretic**.
- No bare numeric gains in `combat/`/`drone/` — tune data in `config/game.toml`.
- Red-team each landed mechanism in a **fresh context**, never self-review.
- Gate = `cmake --build build --config Debug` + `ctest --test-dir build -C Debug`.
  A moved golden means STOP and confirm intent.
- **Fly the optimized build**: `build-play\seads.exe`
  (`cmake --build build-play --target seads`). `build/` is Debug — for the gate
  only. Chad was flying Debug, which was the framerate problem.

---

## ⭐ NEXT ACTION (2026-07-27, from Chad's victory fly) — HALF THE FLEET IS HIS OWN TEAM

Chad flew the working loop and reported:

> "some still stayed outside zone and didnt yet engage me while pump destroying
> ... However most enemies were outside the zone, it was friendlies I engaged on
> my own side I believe. They dont shoot back at me."

**Root cause, one function.** `combat::maverick_faction()` splits the roster
0-4 SUDBURY / 5-9 VALLEY — a 5v5 leftover. With the player now flying VALLEY,
**five of the ten mavericks are Chad's own squadron**, parked at the Valley home
dome. That single fact explains all three of his observations at once:

1. "most enemies were outside the zone" — the 5 VALLEY planes are at the
   *Valley* anchor, nowhere near the Sudbury pumps he was attacking;
2. "it was friendlies I engaged on my own side" — `all_vs_player = true`
   deliberately SUPPRESSES the friendly-faction exclusion, so his own team is
   engaged against him and drawn with the red BANDIT tag, indistinguishable
   from a real enemy;
3. "they dont shoot back" — the return-fire path is gated differently from the
   pursuit/label path, so a friendly reads as a bandit but never fires: the
   exact tell that the plane was never a legitimate target.

**The fix is Chad's original spec, still unimplemented:** *"make them all my
enemy. They start in sudbury, all behave the same way."* Make
`maverick_faction()` return `CQ_SUDBURY` for every index. Then:

- all 10 are hostile, no friendly-fire confusion, no BANDIT tag on a teammate;
- `all_vs_player` can go back to **false** (no exclusion left to suppress), which
  re-enables the tunnel-run diversions rung 1 needs — the two changes unblock
  each other;
- VICTORY becomes "kill all 10", which is the intended match length.

**Touches:** `combat/conquest.h` (`maverick_faction`), the roster comment above
it, `config/game.toml` (`all_vs_player` -> false), and the tests that pin the
5/5 split (`test_conquest.cpp` — grep `maverick_faction`). Small, but it moves
the win condition, so gate and re-fly.

Do this BEFORE rung 1: the sortie AI is pointless while half the fleet is on the
wrong side.
