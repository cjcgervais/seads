# Fable-5 HARD-MATH consult packet — rig-D guns FIRING mechanism

**Role:** isolated math sniper. Do NOT trust the author's rationale below — treat every
CLAIM as a hypothesis to **refute**. For each, either (a) confirm with the derivation, or
(b) break it with a concrete numeric counterexample + the failure mode. Rate findings
**P0** (wrong result / NaN / firewall breach), **P1** (systematic error a pilot would feel),
**P2** (cosmetic / bounded). Per repo policy the author iterates only on P0/P1.

Scope: the PURE firing driver only (`weapon/ballistics.*`) + its one call site
(`app/instructor_tick.h`). NO render/sound this pass. The module is READ-ONLY on the
player and nothing here feeds `control/`/`sim/` (the world-thread firewall — verify the
call site honors it).

## Fixed numeric context (single-sourced)
- Crash sphere radius **R = 15000 m** (`ap.R`; the physics ground is the perfect sphere —
  terrain relief ≤ 350 m rides ABOVE R and is NOT modelled here).
- Fixed sim tick **dt = sim_dt = 1/120 s = 0.00833333 s** (`aircraft.toml [world].sim_dt`).
- Gravity **g ≈ 9.81 m/s²** (`ap.g`), applied as `grav = g · gravity_dir(pos)`,
  `gravity_dir(pos) = -normalize(pos)` (points at planet center; `sim/world.h`, single source).
- Flight-time cap **kMaxFlightTime = 10 s** (single-sourced from the gunsight solver
  `render::kBallisticTMax`).
- Battery (`world.toml [guns]`): hub 20 mm 800 m/s @ 12 Hz; 2× cowl 7.92 mm 760 m/s @ 19 Hz;
  2× wing 20 mm 800 m/s @ 12 Hz. Convergence 300 m. Muzzle speeds are RELATIVE to the shooter.
- Player speed envelope ~ up to ~250 m/s (redline); so max round world speed ≈ 800 + 250 ≈
  1050 m/s → max step ≈ 1050 · dt ≈ **8.75 m/tick**.

## The code under test

```cpp
// advance one round (semi-implicit / symplectic Euler)
void advance(Projectile& p, double dt, const glm::dvec3& grav) {
    p.vel += grav * dt;   // velocity first...
    p.pos += p.vel * dt;  // ...then position with the UPDATED velocity
    p.age += dt;
}
inline bool retired(const Projectile& p) { return p.age > kMaxFlightTime; }

// world unit fire dir: toe-in to (0,0,-conv) in body coords, then rotate to world
glm::dvec3 muzzle_world_dir(shooter, muzzle_body, conv) {
    aim_body = dvec3(0,0,-conv) - muzzle_body;
    dir_body = |aim_body|>1e-9 ? aim_body/|aim_body| : dvec3(0,0,-1);
    return normalize(shooter.orientation * dir_body);
}
// spawn: pos = shooter.pos + orient*muzzle_body; vel = shooter.vel + speed*dir (inherit)

void fire_tick(GunWorld& gw, const sim::SimState& shooter, bool firing,
               double dt, double g, double ground_radius) {
    const std::size_t n = gw.battery.guns.size();
    if (gw.cooldown.size() != n) gw.cooldown.assign(n, 0.0);  // ready by default

    // (1) advance + retire every ACTIVE round; grav recomputed AT THE ROUND each tick
    for (Projectile& p : gw.pool) {
        if (!p.active) continue;
        advance(p, dt, g * sim::gravity_dir(p.pos));
        if (retired(p) || glm::length(p.pos) <= ground_radius) p.active = false;
    }

    // (2) fire: each gun off cooldown emits one round; released trigger => ready (0)
    for (std::size_t i = 0; i < n; ++i) {
        if (!firing) { gw.cooldown[i] = 0.0; continue; }
        gw.cooldown[i] -= dt;
        if (gw.cooldown[i] > 0.0) continue;
        const GunSpec& gun = gw.battery.guns[i];
        const Projectile r = spawn(gw.battery, gun, shooter);
        bool placed = false;
        for (Projectile& slot : gw.pool)
            if (!slot.active) { slot = r; placed = true; break; }
        if (!placed) gw.pool.push_back(r);
        gw.cooldown[i] += (gun.rof_hz > 0.0) ? 1.0 / gun.rof_hz : dt;
    }
}
```

Call site (once per player tick, after the drone/meter advance):
```cpp
if (gw != nullptr) {
    const bool firing = in.fire_held && !in.raw_mode && !st.grounded;
    weapon::fire_tick(*gw, st.curr, firing, ap.sim_dt, ap.g, ap.R);
}
```
`gw`/`fire_held` default to nullptr/false, so the pre-guns path is bit-identical (the
mirror-equivalence tests `test_at9`/`test_instructor_tick` pass unchanged).

## CLAIMS to refute

**C1 — discrete drop.** After n uniform steps under a constant `grav` of magnitude g,
`advance` drops exactly `g·dt²·n(n+1)/2` (NOT the continuous `½g t²`). Verify the closed
form for symplectic Euler and that the code produces it. (Test pins n=100, dt=0.01 → 4.954 m.)

**C2 — cadence & the ≤1-shot/tick invariant.** Holding the trigger for T = N·dt spawns
`⌊T/period⌋ + 1` rounds (first shot instant, then one per `period = 1/rof`); the
accumulating `cooldown += period` (not `= period`) makes the long-run rate → rof with
bounded phase error and no dt-quantization rate loss. The `if (cooldown>0) continue;` fires
**at most one** round per gun per tick — CORRECT here **iff period > dt**. Show the margin:
min period = 1/19 = 0.0526 s vs dt = 0.00833 s (6.3×). **Refute:** construct any reachable
(rof, dt) where a tick genuinely owes ≥2 shots and one is silently dropped. Is `+=` ever a
bug (unbounded negative cooldown while firing)? Bound |cooldown| over a long hold.

**C3 — sphere-coherent gravity, per-tick.** grav is recomputed at each round's position
every tick (piecewise-constant over dt). (a) Bound the error of a spawn-FROZEN grav over the
10 s cap vs the per-tick recompute — is the recompute necessary? (b) Is per-TICK (not
sub-step) resolution sufficient? Per step the round turns ≈ step/|pos| ≈ 8.75/15000 ≈
5.8e-4 rad through the g-field; argue whether this piecewise-constant sampling introduces
error a pilot feels at gun range (range ≤ 500 m, TTI ≲ 0.6 s). Refute if it matters.

**C4 — ground retire soundness (tunneling).** A round is retired when `|pos| ≤ ground_radius
(=R)`. (a) On a near-radial descent |pos| is monotone → caught on the first sub-R tick;
confirm. (b) **Tunneling:** on a shallow chord whose true closest approach dips just below R
BUT both sampled endpoints stay above R, the round crosses the R-shell undetected for one
step and re-emerges (a bullet skimming through a hilltop). Quantify the max undetected
penetration as a function of step (≤8.75 m) and R geometry — is it a real defect or bounded
cosmetic? (c) We ignore terrain relief (≤350 m above R): rounds fly up to 350 m INTO hills
before retiring. Rate the severity / recommend (retire at R+h vs accept for this pass).

**C5 — pool boundedness (no immortal rounds / no leak).** Every active round has `age +=
dt` monotone, so it retires by age > 10 s at worst → max simultaneously-active ≤
Σ_g(rof_g·10) + guns = (12+19+19+12+12)·10 + 5 = **745**. Confirm no code path keeps a round
active past that (e.g., a round that never advances, or an inactive slot never reused →
fragmentation growth). Is first-fit reuse leak-free?

**C6 — toe-in + inheritance consistency with the pipper.** `orient·normalize(a) ==
normalize(orient·a)` (rotation preserves norm) so muzzle_world_dir is the true world toe-in.
`spawn.vel = shooter.vel + speed·dir` matches the gunsight's relative-velocity + gravity
intercept model, so a round fired down `LeadSolution.lead_dir` hits (the existing absolute
cross-check test). Confirm the two models cannot fork (same g, same inheritance, same speed
— but note the pipper solves at 850 m/s while the 20 mm is 800 m/s; that reconciliation is
DEFERRED to the render pass — flag if 800 vs 850 breaks C6's consistency claim in a way that
matters before then).

**C7 — numerics.** dvec3 at |pos| ~15–17 km (double: ~2e-12 relative → sub-µm). The cooldown
`double` accumulation over a 10-min hold (~11k shots). Any catastrophic cancellation, NaN
(e.g. `gravity_dir` of a round that reaches the origin — reachable?), or precision loss that
changes a hit/miss? `glm::length(p.pos) ≤ ground_radius` at |pos| ≈ R — is the comparison
robust to rounding (a round parked exactly on R chattering active/inactive)?

**C8 — firing-gate composition & ordering.** `firing = fire_held && !raw_mode && !grounded`,
and the block runs AFTER crash/respawn so on a respawn tick `st.curr` is the fresh spawn
state but `grounded==true` gates firing off (no shot from a reborn stick); rounds in flight
still advance (a mode toggle / respawn never freezes tracers). A round spawned this tick is
NOT advanced this tick (age 0 at muzzle, first move next tick) — a 1-tick (8 ms) muzzle
dwell. Confirm the ordering has no double-count, no dropped advance, no firewall leak
(nothing reads/writes player or kernel state beyond the const shooter).

Return: per-claim CONFIRM/REFUTE + derivation or counterexample, severity, and a one-line
fix for each P0/P1.
