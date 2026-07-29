# The rudder / coordination ladder — how much rudder is in the equation

How the kernel decides how hard the rudder chases the mouse, how hard coordination pulls
the nose back into clean air, and why those two are in permanent, deliberate tension. This
is the mechanism behind Chad's 2026-07-28 "a little too much rudder bias in the equation"
ask, and behind the whole 2026-07-08→11 rudder-flick / yaw-magnet session ladder.

---

## 1. Feel — pilot voice

There are three hands on the nose, left-and-right:

- **The rudder chasing your mouse.** Small lateral flicks are rudder-first — the nose
  skids sideways onto the aim without waiting for a bank. Chad, tuning it up (2026-07-10):
  "rudder strong at the bottom end... lil flicks that do go far are just rudder," and
  "more rudder authority... crabbing? yes!"
- **Coordination pulling the nose straight.** The classic "step on the ball" force — it
  wants the fuselage pointed into the airflow, which *fights* the skid the rudder just
  made. When it was too strong near center, Chad felt it as the nose parking just outside
  the circle: "the magnet for yaw isn't strong enough to pull it into the middle."
  His ruling (Rung M1, 2026-07-11): "forget the crabbing — I just want that nose to snap
  and stay right in the middle."
- **The airframe itself straightening out.** Since S-wvane (2026-07-11, "I don't want the
  cocking — snap to the centre every time"), a crabbed fuselage weathervanes flat on its
  own in about a second — the air pushes the tail back in line without the controller
  doing anything.

When the balance is right, little flicks feel like flat rudder snaps, big turns feel
banked and clean, and the nose sits dead center at rest. When the rudder hand is too
strong, you get what Chad reported 2026-07-28: "a little too much rudder bias" — the nose
sits crabbed with the rudder always working, and at speed the same gain turns violent.
And the fix that worked revealed the deeper truth (Chad's approval verdict): "Now that the
flight kernel was given a more sufficient engine per weight ratio... it feels much better
now to not have to chase the mouse with so much rudder but now the plant is able to
respond" — a stronger airframe (rung D) needs less rudder, because lift can carry the nose
to the aim instead of yaw skidding it there.

## 2. Principle — instructor voice

Three lateral influences sum into the yaw command each tick:

1. **Yaw pointing** — a gain on lateral aim error, shaped so small errors get
   proportionally strong response and large errors are braking-capped. Its strength is the
   product of the master pointing gain and a yaw-specific scale (`yaw_scale`) — the single
   "how much rudder in the equation" dial. Because the demand ultimately acts through
   aerodynamic force, it scales with dynamic pressure: **the same dial bites harder the
   faster you fly.** That is why v5's energy model (rung D) changed the *felt* rudder
   without any yaw commit — higher typical speeds exercised the old gain harder.
2. **Coordination** — a gain on sideslip (β) that commands yaw to null it. Full strength
   would kill the rudder-flick character entirely, so it *fades near center*
   (S-yaw-magnet): at zero pointing error coordination runs at a floor fraction
   (`center_frac`, currently 0 — fully silent), rising smoothly to full strength by
   `center_band` (1.5°). The rudder finishes the last fraction of a degree unopposed; a
   sustained banked turn (error well past the band) keeps full coordination, so the skid
   limit in real turns is untouched by construction.
3. **The plant's own weathervane** (S-wvane) — a fuselage side-force in the *simulation*,
   not the controller, that bleeds sideslip to zero in ~1 s. This is what made
   `center_frac = 0` safe: coordination is no longer the only channel that removes crab.

**The wall:** AT-16, the sideslip sentinel — peak β must stay ≤ 5.0° in the sustained
coordinated turn. Both global dials breach it if pushed (`yaw_scale` 2.4 breached at
5.02°; `K_coord` 0.8–0.9 breached pre-fly). The near-center relief exists precisely
because the global dials are walled; past the wall, the honest path is a Chad ruling on
the coordination trade, never a bigger gain.

## 3. Math — engineer voice

Per tick, the lateral demand `e_y` (aim error, yaw axis) and measured sideslip `β` feed:

```
yaw  = sqrt_law(e_y, K_theta * yaw_scale, aB_yaw, yaw_max)     // pointing
     + coord_scale * K_coord * (−β)                             // coordination
```

- `sqrt_law(e, K, a_brake, w_max)`: sign-preserving, √-shaped in the linear region,
  braking-capped, saturating at `w_max`. Yaw uses the plain form (no step floor, no expo).
  Effective linear pointing gain: `K_theta · yaw_scale` = 3.2 · 2.0 = **6.4** (was 7.04 at
  2.2). `yaw_max = 55 deg/s` (the ruled top-end nerf — `yaw_scale` raises linear-region
  gain only, never past the cap).
- `coord_scale = center_frac + (1 − center_frac) · smoothstep(|e| / center_band)` —
  continuous, no latch, no chatter. `center_frac = 0.0`, `center_band = 1.5°`. At
  `center_frac = 1.0` the expression short-circuits to legacy full coordination (the A/B).
- Park standoff equilibrium (why the relief exists):
  `e_ss ≈ K_coord · β / (K_theta · yaw_scale)` — with full coordination and residual crab
  the nose parks *just outside* the 0.05° aim circle (measured 0.0718° at V=80 for a big
  jut) instead of latching center.
- S-wvane plant side-force: `F = −q · S · Cy_beta · sin β · cos β` (lateral, ⊥ velocity),
  giving crab decay `τ_β = 2m / (ρ · V · S · Cy_beta)` ≈ 1.1 s at V=140 with the shipped
  `Cy_beta = 2.5`. `Cy_beta = 0` is structurally OFF (bit-identical pre-S-wvane).
- Yaw damping pair: ζ ≈ 0.77 (`K_w_yaw` 200k against ~338k critical). Constitutional
  order if buzz appears: raise `K_w_yaw` first, then walk `yaw_scale` back toward 2.0.
- AT-16 wall: sustained-coordinated-turn peak β ≤ 5.0°.

### The dial ladder (history, all Chad-ruled)

| Date | Dial | Move | Why |
|---|---|---|---|
| 2026-07-06 | `K_coord` | 2.0→3.0→1.0 | less coordination = the banked turn slips; the skid Chad wants |
| 2026-07-08 | `yaw_scale` | 1.5 → 2.0 (MB-4, one step) | stronger linear-region pointing |
| 2026-07-10 | `yaw_scale` | 2.0 → 2.2 (rudder-flick fly 1) | "rudder strong at the bottom end"; 2.4 drafted and **breached AT-16 pre-fly** |
| 2026-07-10 | `center_band` | 0.5° → 1.5° (fly 13) | "more rudder authority... crabbing? yes!" |
| 2026-07-10/11 | `center_frac` | 0.25 → 0.15 → 0.05 (Y1) → 0.02 (Y2) → **0.0 (M1)** | park-standoff ladder, ended by "forget the crabbing — nose in the MIDDLE" |
| 2026-07-11 | `Cy_beta` | 0 → 2.5 (S-wvane) | plant bleeds crab itself; makes frac 0 free |
| 2026-07-28 | `yaw_scale` | 2.2 → **2.0** (rudder-trim Fly A) | "too much rudder bias"; v5 speeds exercised the old gain harder. APPROVED |

Pre-agreed, untaken fallbacks from the 2026-07-28 session (kept for the record):
`Cy_beta 2.5 → 1.5` (if speed snap-back survived), `center_frac 0.0 → 0.3` (if
crab-at-rest survived — ⚠ walks back Rung M1).

## 4. Code — grounded in `reference/seads-feel/` (snapshot @ `89447aba5`)

- `control/controller.cpp` — `sqrt_law` (the shaped gain law; comment block names the
  yaw call convention), the yaw pointing call sites (`cp.K_theta * cp.yaw_scale`), and the
  coordination block: `coordination_yaw_demand(e.beta)` scaled by the `coord_scale`
  smoothstep over `cp.coord_center_frac` / `cp.coord_center_band`, with the
  `e_ss ~ K_coord*beta/(K_theta*yaw_scale)` equilibrium comment.
- `control/params.h` / `config/load_controller.cpp` — `yaw_scale`, `K_coord`,
  `coord_center_frac`, `coord_center_band`, `yaw_max`, `K_w_yaw` plumbing and loader
  walls (`frac ∈ [0,1]`, band ∈ (0, blend_lo/2]).
- `sim/params.h` — `Cy_beta` (S-wvane side-force slope; 0 = structurally off);
  `sim/step.cpp` — the S-wvane fuselage side-force block (`F = -q*S*Cy_beta*sin(b)cos(b)`)
  and the crab-pays-drag `Cd_beta` companion.
- `config/controller.toml` `[coordination]` — the dials with Chad's rulings quoted inline;
  the richest single narrative of this ladder lives in those comments.

**Snapshot drift flag:** the snapshot's `controller.toml` carries `yaw_scale = 2.2` — the
pre-trim value. The live tree (`feel/kernel-v5` @ `385a43dbd`, 2026-07-28) carries **2.0**
with the trim ruling quoted inline. Re-snapshot pending (see the watch-item in
`docs/DECISIONS.md`). Everything else in this entry is present in the snapshot.

## Lineage

The rudder-first flick character originates in the EvC2026 Luau testbed's flat-turn
behavior, but every number and mechanism above is the seads-feel C++ kernel — the
current authority. The EvC2026 yaw path never had the S-yaw-magnet relief or S-wvane;
do not back-port readings between them.
