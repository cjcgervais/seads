# The Maverick Squadron

*v5 world thread — 2026-07-24. The docile 10-drone patrol fleet, reforged into a
worthy adversary squadron with a penchant for **tunnel runs**.*

Ten named pilots hold a loose patrol over the world, and — on their own staggered
schedules — one peels off, dives into the **Errington pit**, threads the
underground bore down to **−2000 m**, climbs back toward **Murray**, and reverses
to do it again. They fly adeptly: faster, more committed, and more varied than the
old patrol. When you see one drop into the pit, you can chase it in.

Everything is pure / clock-free / RNG-free (the `drone/` seam), a sibling of
`drone.h`. Nothing touches the frozen flight kernel — the brain only reads the
tunnel geometry and emits bank/gamma/gain/speed setpoints for the *same*
bank-hold autopilot the patrol already used. `[maverick] enabled = false` makes
the fleet **bit-identical** to the old patrol.

## The roster

| Callsign | Speed | Aggr. | Run gap | First dir | Daredevil |
|----------|:-----:|:-----:|:-------:|:---------:|:---------:|
| **GULCH**     | 1.20× | 0.75 | 130 s | Errington→Murray | 0.55 | steady, dependable — the one you'll chase first |
| **SHAFT**     | 1.35× | 0.85 |  95 s | Errington→Murray | 0.80 | quick and hungry, short fuse between runs |
| **CANARY**    | 1.15× | 0.55 | 200 s | Murray→Errington | 0.30 | cautious, wide margins — flies the raid mouth |
| **NICKEL**    | 1.45× | 0.70 | 160 s | Errington→Murray | 0.65 | fast mover, hot into the bore |
| **SLAGHEAP**  | 1.25× | 0.60 | 240 s | Murray→Errington | 0.40 | rare and patient — a long-interval runner |
| **PITVIPER**  | 1.55× | 0.95 |  75 s | Errington→Murray | 0.95 | the ace: fastest, meanest, hugs the wall |
| **LAMPLIGHT** | 1.18× | 0.50 | 220 s | Murray→Errington | 0.25 | the tourist — gentle, wide, unhurried |
| **BOREAL**    | 1.30× | 0.65 | 150 s | Errington→Murray | 0.55 | a solid middle-of-the-pack fighter |
| **MURRAY**    | 1.40× | 0.80 | 110 s | Murray→Errington | 0.75 | named for the raid pit it dives — aggressive |
| **STOPE**     | 1.22× | 0.70 | 185 s | Errington→Murray | 0.50 | methodical, threads clean |

- **Speed** multiplies the base cruise (`[maverick] base_speed`, 130 m/s).
- **Aggression** ≥ `engage_aggression_min` (0.55) → the pilot still *hunts you*
  while loitering in patrol. A pilot committed to a run ignores you and dives —
  that's the invitation to give chase.
- **Run gap** is the nominal seconds between that pilot's runs (staggered by index
  so runs peel off one at a time, never a synchronized swarm).
- **Daredevil** shrinks the pilot's wall margin toward the hard floor and hots up
  its bore speed — PITVIPER threads tightest and fastest; LAMPLIGHT stays wide.

The table is deterministic constexpr flavour data in `drone/maverick.h`.

## The mode machine

Each pilot runs a five-state machine (`maverick::maverick_step`, pure):

1. **PATROL** — a gentle weave at cruise, counting down to the next run.
   Aggressive pilots still pursue you here (the old combat path).
2. **TRANSIT** — a pure-pursuit dash to the dive-in setup: above and behind the
   mouth on the approach side, lined up with the bore tangent.
3. **DIVE_IN** — a tangent glide into the pit throat (steep gamma allowed only
   here). Switches to RUN the instant it's inside the net.
4. **RUN** — wall-guarded centreline tracking with **path-slope feedforward**.
   The BANK channel is pure-pursuit with a **cross-track correction** (keeps the
   nose on centre through the dip curves); the GAMMA channel commands the path's
   OWN climb angle (from the spine tangent, sampled a lookahead ahead so the
   dip→climb knee is anticipated) and lets a gentle PD correct only the residual
   altitude error — so the steep exit ascent holds without the error-only PIO. A
   **hysteretic wall guard** (reading the tunnel's own `signed_distance` — the
   exact collision truth) tightens the lateral pull near a wall. A **throttle
   feedforward** (∝ sin γ) supplies the climb energy up front.
5. **CLIMB_OUT** — a **clean surface break-out**. It keeps flying the SAME
   wall-guarded slope-feedforward law, with the centreline **virtually extended**
   past the sunken mouth along the exit tangent (so the lead never drags back to
   the receding mouth point), then steepens to climb straight up out of the open
   bowl/pit; once above the exit **bowl rim** it hands back to PATROL and **flips
   run direction** so the next run threads the bore the other way.

## How to witness a run

- Loiter near the **Errington pit** (`kTunnelMouthErrington`, the Chelmsford home
  mouth) or the **Murray pit** (the raid mouth). Runs alternate direction, so both
  mouths see traffic.
- The stagger means a run peels off every ~1–2 minutes on average across the
  squadron. Watch a pilot break from the patrol weave and line up on a mouth —
  that's your cue to follow it down.
- Chase it in. A committed maverick ignores you and flies the bore; you fly the
  same −2000 m tunnel to keep on its tail.

## The dials (`config/scenario.toml [maverick]`)

All live-tunable (no recompile), validated against `aircraft.toml` at load:

| Dial | Ship value | What it does |
|------|:----------:|--------------|
| `enabled` | true | master gate (false = the old docile patrol, bit-identical) |
| `base_speed` | 130 | reference cruise the trait speed multiplies |
| `tunnel_speed_max` | 135 | the honest bore-speed ceiling (see below) |
| `lookahead_m` | 220 | pure-pursuit centreline lookahead |
| `centerline_gain` | 2.0 | cross-track pull toward centre (tight dip tracking) |
| `gamma_lookahead_m` | 300 | how far ahead the path slope is sampled (knee anticipation) |
| `slope_ff_gain` | 1.0 | path-slope feedforward weight (1 = full; 0 = error-only PIO) |
| `track_gain` | 0.0045 | residual altitude-error → gamma gain [rad/m] |
| `climb_throttle_gain` | 1.3 | throttle feedforward weight on sin(γ) |
| `climb_speed_min` | 118 | sustainable climb speed the ramp bleeds toward |
| `wall_margin_m` / `_release_m` / `_floor_m` | 34 / 60 / 14 | hysteretic wall guard band + daredevil floor |
| `dive_/run_/transit_gamma_cap_deg` | 55 / 44 / 30 | attitude caps per mode |
| `bank_cap_deg` | 65 | roll ceiling |
| `bank_p` / `pitch_p` / `k_az` / `k_el` | 0.5 / 2.2 / 3.5 / 3.0 | autopilot + steering gains |
| `exit_frac` | 0.95 | fraction of the bore the RUN threads before CLIMB_OUT |
| `period_scale` | 1.0 | global stretch on every pilot's run gap |
| `engage_aggression_min` | 0.55 | patrol-pursuit aggression gate |

## The break-out (curvature-feedforward bore autopilot, v5 2026-07-24)

The maverick now threads the whole **descent → −1600 m dip → the full steep
ascent** of the ~12 km bore and **breaks CLEAN out to the surface** in **both
directions**, then hands back to PATROL and flies free. The old "honest ceiling"
(a final steep-ascent clip at ~72%) is **gone**.

The prior failure was diagnosed and fixed, not tuned around. It was **closed-loop
control**, not power — the plant *itself* holds 25–27° open-loop at full throttle
(speed bleeding to ~110–127 m/s), above the 21° bore. The error-only pursuit gamma
was a violent **pitch PIO** on the ascent (commanded γ swung ±45° while the path
slope was only ~22°, the cross-track term over-driving elevation), so the plane
wove hundreds of metres off centre and clipped the narrowing exit. The fix is the
same lesson as the player kernel's `S-dampff`: **complete the inversion — feed
forward what you KNOW.**

- **Path-slope feedforward** (the load-bearing term). Command the path's OWN climb
  angle `γ_path = asin(tan·local_up)` (from the spine tangent, sampled
  `gamma_lookahead_m` ahead so the dip→climb knee is anticipated — the curvature
  term for free) and let a gentle PD (`track_gain`) correct only the residual
  altitude error. Standing tracking error on the steep segment drops from a
  hundreds-of-metres PIO to **~5 m**; γ tracks the path slope within ~1°.
- **Throttle feedforward** (`climb_throttle_gain·sin γ`). Supplies the climb
  energy up front so the autothrottle no longer has to discover it through speed
  sag. Improves steep-segment wall clearance (~37 → ~52 m) — **helpful, not
  strictly load-bearing** at the shipped dials (the speed-error autothrottle alone
  also breaks out).
- **Virtual centreline extension** past the sunken mouth (along the exit tangent)
  so CLIMB_OUT's lead never drags back to the receding mouth point, then a steep
  climb straight up out of the open bowl/pit, with a **hysteretic** in-net/broken-
  out latch and a rim-clearance hand-off.

Measured, closed-loop against the real `sim::step` plant + `TunnelNet::signed_distance`,
both directions (Errington→Murray and Murray→Errington):

| | ship (ff on) | ff off (`slope_ff_gain = 0`) |
|---|:---:|:---:|
| break out to PATROL | **yes, both** | no — augers (respawns) |
| bore threaded | **100%** | ~4% (stalls on the descent) |
| deep-bore wall clearance | **82–84 m** (floor 14) | grazes / crashes |
| steep-segment tracking error | **~5 m** | PIO / crash |

The closed-loop worthiness test (`test_maverick.cpp`) grades exactly this for BOTH
directions: enter, fly the steep ascent (γ_path > 15° observed — a non-vacuous
premise), go deep past −1400 m, thread >95% of the bore with margin above the
floor, monotone, no crash, **break clean out to PATROL and fly free above the
surface**. The **feedforward tripwire** (`slope_ff_gain = 0`) proves the ff is
load-bearing — the same run then augers and respawns. Both pins mutation-verified.

*Files: `drone/maverick.h` (the brain + route + traits), `drone/drone.h` (the
minimal tick hook), `config/scenario.toml` `[maverick]` + `config/load_scenario.cpp`
(dials + envelope validation), `test/unit/test_maverick.cpp` (the pins).*
