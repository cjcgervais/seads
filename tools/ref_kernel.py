#!/usr/bin/env python3
"""
ref_kernel.py — SEADS deterministic kernel REFERENCE (ATM-Sphere v1.2r0)

Pure-Python reference implementation of the sealed kinematics, using detmath_ref only
(no libm transcendentals). It defines the canonical behavior the C++ kernel must
bit-match, and it generates the GOLDEN-SK-Sphere-001 world_hash.

Canonical snapshot format (little-endian), so C++ and Python produce identical bytes:
  offset 0  : u16  mode (1 = ATM)
  offset 2  : u16  pad (0)
  offset 4  : u32  tick_count
  offset 8  : f64  dt_s
  offset 16 : f64  R_m
  offset 24 : u32  n_aircraft
  offset 28 : u32  pad (0)
  offset 32 : per aircraft, 10 x f64 = [lat, lon, psi, phi, alt, tas, gamma, hp, fire_cd, ammo]
              (radians / meters / m s^-1 / radians / hitpoints / cooldown ticks / rounds)  -- gamma
              added in B2 (v1.6r0); hp added in G2 (v1.10r0, hp<=0 == dead); fire_cd added in G3
              (v1.11r0); ammo added in G4 (v1.13r0, firing gated on ammo>0, one round consumed/shot)
  then      : u32 n_projectiles, u32 pad            -- G1 (Step 7 guns, v1.9r0)
  then      : per projectile, 7 x f64 [lat, lon, psi, alt, tas, gamma, damage] + u32 ttl + u32 owner
              (damage added in G3 v1.11r0; the projectile block is ALWAYS present, n=0 for gun-less
               scenarios -> every prior golden's bytes grew, so all hashes moved, as B2's gamma did)

world_hash = SHA-256 of the snapshot bytes.

Usage:
  python tools/ref_kernel.py [--rails config/rails/atm.json] [--out run.bin]
                             [--seal]   # write sealed golden under tests/golden/<id>/
Exit: 0 on success.
"""
import argparse, json, struct, hashlib, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import detmath_ref as dm
import envelopes as envmod

ROOT = Path(__file__).resolve().parent.parent

# --- B1 longitudinal-energy model constants (ATM-Sphere v1.5r0) -------------------------
# Exact hex-float literals so Python and C++ (kernel.cpp) share identical bit patterns.
# RHO0: constant sea-level ISA air density (the "still air / constant atmosphere" rail; ISA
# density-vs-altitude is deferred to B5). V_MIN: hard speed floor keeping psi_dot = g0*tan(phi)/V
# and the drag solve well-defined (a real stall model is B3). See ADR-Step8-FlightModel-B1.
RHO0 = float.fromhex('0x1.399999999999ap+0')   # 1.225 kg/m^3
V_MIN = float.fromhex('0x1.e000000000000p+4')   # 30.0 m/s

# --- B3 limits & stall (ATM-Sphere v1.7r0) ----------------------------------------------
# The B2 global placeholder clamp on the load factor (N_MIN=-3, N_MAX=+9) is RETIRED. The
# achievable load factor n is now bounded per-airframe by BOTH a structural limit and an
# aerodynamic ceiling (accelerated stall):
#   * structural: n in [n_min_struct, n_max_struct]  (envelope scalars, the V-n "g limit")
#   * aerodynamic: |n| <= n_aero = cl_max * qS / (m*g0)  (the most lift the wing can make at the
#     current dynamic pressure; below the corner speed this is the binding limit -> the turn rate
#     collapses as speed bleeds = the accelerated stall / energy death-spiral, emergent).
# The corner speed V* = stall_tas * sqrt(n_max_struct) (where n_aero meets n_max_struct) falls
# out for free. No new det_math: n_aero is +,-,*,/ only. The post-stall lift is HELD at C_Lmax
# (AoA-limited cap, not a discontinuous lift drop / departure — that is optional, deferred).
# See ADR-Step8-FlightModel-B3.

# --- G1 ballistic projectiles (Step 7 guns, ATM-Sphere v1.9r0) ---------------------------
# A fired round is a deterministic point mass on the SAME sphere, stepped by the SAME closed-form
# great-circle integrator as an aircraft — it is exactly the 3-DOF point-mass step specialized to
# n=0 (no lift, ballistic) and thrust=0 (no engine): only gravity along the flight path and a
# lumped quadratic drag act. So there is NO new det_math (the projectile uses det_sin/det_cos +
# great_circle_step + (+,-,*,/) already proven vs the MPFR oracle). The round carries an integer
# time-to-live (ticks) and an owner index (which aircraft fired it; carried for G2 hit attribution).
# G3 (v1.11r0): muzzle velocity and damage-per-round are now PER-AIRFRAME (envelope scalars
# muzzle_v_mps / damage_per_round); drag and ttl stay GLOBAL (a bullet is a bullet). Exact hex-float
# literals so Python and C++ (kernel.cpp) share identical bit patterns. See ADR-Step7-Guns-G1/G3.
PROJ_DRAG_K = float.fromhex('0x1.a36e2eb1c432dp-13')  # 2.0e-4: lumped quadratic drag decel coeff (Vdot -= k*V^2)
PROJ_TTL_TICKS = 250                                 # 2.5 s lifetime, then the round despawns

# --- G2 hit detection + per-aircraft hitpoints (Step 7 guns, ATM-Sphere v1.10r0) ---------
# A round that passes within HIT_RADIUS_M (horizontal great-circle) AND HIT_ALT_GATE_M (vertical)
# of an ALIVE enemy aircraft deals DAMAGE_PER_ROUND and despawns. hp reaches 0 -> the aircraft is
# DEAD (its flight integration is skipped — it freezes — and it can no longer fire or be hit).
# The horizontal test uses the spherical law of cosines: cos(central angle) = sinφ1·sinφ2 +
# cosφ1·cosφ2·cos(Δλ), compared to the precomputed COS_HIT_ANGLE = cos(HIT_RADIUS/R) — acos is
# monotone, so "distance < HIT_RADIUS" <=> "cosc > COS_HIT_ANGLE". So NO new det_math (det_sin/det_cos
# + +,-,*,/). G1/G2 constants are GLOBAL (per-airframe HP / armament are G3). Exact hex-floats shared
# Python<->C++. COS_HIT_ANGLE was computed once via det_cos(HIT_RADIUS/R). See ADR-Step7-Guns-G2.
# G3 (v1.11r0): hp_start and damage_per_round are now PER-AIRFRAME (envelope). START_HP remains the
# GLOBAL default for the no-arg path (the Sphere golden has no envelope). HIT_RADIUS/ALT_GATE stay
# global (they reflect target size, not the weapon). The round CARRIES its damage (set at spawn from
# the firer's gun) so its lethality is fixed at fire time and the advance/hit stay self-contained.
START_HP = float.fromhex('0x1.9000000000000p+6')     # 100.0 default hitpoints (no-arg/Sphere; per-airframe via env)
# --- G4 finite ammunition (Step 7 guns, ATM-Sphere v1.13r0) ------------------------------
# START_AMMO is the GLOBAL default magazine for the no-arg/Sphere path (per-airframe ammo_start
# comes from the envelope). Firing is gated on ammo > 0 (one round consumed per shot); at 0 the
# gun falls silent ("Winchester"). A pure integer-valued counter (like fire_cd) — NO new det_math.
# Exact hex-float shared bit-for-bit with kernel.cpp. See ADR-Step7-Guns-G4.
START_AMMO = float.fromhex('0x1.f400000000000p+8')   # 500.0 default magazine (no-arg/Sphere; per-airframe via env)
HIT_RADIUS_M = float.fromhex('0x1.e000000000000p+5')  # 60.0 m horizontal hit radius (validation/doc)
HIT_ALT_GATE_M = float.fromhex('0x1.e000000000000p+5')  # 60.0 m vertical hit gate
COS_HIT_ANGLE = float.fromhex('0x1.fffef3909d697p-1')  # cos(HIT_RADIUS_M/R) via det_cos; the binding test


def clamp(v, lo, hi):
    if v < lo:
        return lo
    if v > hi:
        return hi
    return v


def great_circle_step(lat, lon, bearing, s, R):
    """Closed-form intrinsic-S2 great-circle step. det_math only."""
    alpha = s / R
    sinlat1 = dm.det_sin(lat)
    coslat1 = dm.det_cos(lat)
    ca = dm.det_cos(alpha)
    sa = dm.det_sin(alpha)
    cb = dm.det_cos(bearing)
    sb = dm.det_sin(bearing)
    sin_lat2 = sinlat1 * ca + coslat1 * sa * cb
    sin_lat2 = clamp(sin_lat2, -1.0, 1.0)
    lat2 = dm.det_asin(sin_lat2)
    y = sb * sa * coslat1
    x = ca - sinlat1 * sin_lat2
    lon2 = dm.wrap_pi(lon + dm.det_atan2(y, x))
    return lat2, lon2


def ceiling_climb_rate(requested_mps, alt, atm_top, soft):
    """Soft-band predamp -> hard clamp. Monotone reduction in the soft band, no overshoot."""
    if requested_mps <= 0.0:
        return requested_mps
    band_lo = atm_top - soft
    if alt >= band_lo:
        frac = (atm_top - alt) / soft
        frac = clamp(frac, 0.0, 1.0)
        return requested_mps * frac
    return requested_mps


class Aircraft:
    # B2 (v1.6r0): gamma (flight-path angle, rad) is a real stored state. Defaults to 0.0 so the
    # no-arg straight golden and any 6-tuple caller still build a level-flying aircraft.
    # G2 (v1.10r0): hp (hitpoints) is a stored state; hp<=0 == DEAD. Defaults to START_HP so every
    # prior caller builds a healthy aircraft (and the no-damage paths leave it at full HP).
    # G3 (v1.11r0): fire_cd (fire-rate cooldown, ticks) is a stored state; firing is gated by it
    # (decrement-then-fire). Defaults to 0.0 (ready to fire).
    # G4 (v1.13r0): ammo (magazine, rounds) is a stored state; firing is gated on ammo > 0 and one
    # round is consumed per shot. Defaults to START_AMMO (the no-arg/Sphere path; per-airframe via env).
    __slots__ = ("lat", "lon", "psi", "phi", "alt", "tas", "gamma", "hp", "fire_cd", "ammo")

    def __init__(self, lat, lon, psi, phi, alt, tas, gamma=0.0, hp=START_HP, fire_cd=0.0,
                 ammo=START_AMMO):
        self.lat, self.lon, self.psi, self.phi, self.alt, self.tas = lat, lon, psi, phi, alt, tas
        self.gamma = gamma
        self.hp = hp
        self.fire_cd = fire_cd
        self.ammo = ammo


class Projectile:
    # G1 (v1.9r0): a ballistic round on the sphere. No phi (no bank) and no thrust/lift — its motion
    # is the n=0/thrust=0 specialization of the aircraft 3-DOF step. ttl is an integer tick countdown;
    # owner is the firing aircraft index (G2 hit attribution). G3 (v1.11r0): damage is carried (set at
    # spawn from the firer's gun) so lethality is fixed at fire time and the hit stays self-contained.
    __slots__ = ("lat", "lon", "psi", "alt", "tas", "gamma", "damage", "ttl", "owner")

    def __init__(self, lat, lon, psi, alt, tas, gamma, damage, ttl, owner):
        self.lat, self.lon, self.psi, self.alt, self.tas, self.gamma = lat, lon, psi, alt, tas, gamma
        self.damage, self.ttl, self.owner = damage, ttl, owner


class Kernel:
    def __init__(self, rails):
        r = rails["rails"]
        self.R = float(r["geometry"]["radius_m"])
        self.dt = float(r["dt_s"])
        self.g0 = float(r["gravity_mps2"])
        self.atm_top = float(r["atm_top_m"])
        self.soft = float(r["atm_soft_band_m"])
        self.aircraft = []
        self.projectiles = []

    def _advance(self, ac, req):
        """Kinematic tail for one aircraft: coordinated-turn + great-circle + ceiling-clamped
        vertical. `ac.phi` is already final for this tick. This is the VERBATIM body of the
        original step() inner loop; both the straight golden (req=0, phi=0) and the scenario
        path go through it, so the sealed golden stays byte-identical. C++ mirror: Kernel::advance_."""
        dt, R, g0 = self.dt, self.R, self.g0
        V = ac.tas
        # coordinated-turn law
        psi_dot = g0 * dm.det_tan(ac.phi) / V
        ac.psi = dm.wrap_2pi(ac.psi + psi_dot * dt)
        # great-circle horizontal advance
        s = V * dt
        ac.lat, ac.lon = great_circle_step(ac.lat, ac.lon, ac.psi, s, R)
        # vertical (ceiling-clamped)
        rate = ceiling_climb_rate(req, ac.alt, self.atm_top, self.soft)
        ac.alt = clamp(ac.alt + rate * dt, 0.0, self.atm_top)

    def _advance_projectiles(self):
        """G1 (v1.9r0): step every live round one tick, then despawn the expired/grounded ones.
        Ballistic point mass = the aircraft 3-DOF step with n=0 (no lift) and thrust=0: gravity
        acts along the flight path, a lumped quadratic drag bleeds speed, the heading psi is fixed
        (no turn force; the great-circle step still carries the geodesic), altitude is V*sin(gamma).
        G2 (v1.10r0): after moving a round, test it against every ALIVE aircraft (array order); on
        the first qualifying hit (within HIT_RADIUS_M horizontally via the law-of-cosines test AND
        HIT_ALT_GATE_M vertically, not the firer) deal DAMAGE_PER_ROUND and despawn the round.
        Op order is the sealed byte spec and MUST match Kernel::advance_projectiles_ in kernel.cpp.
        Iteration + compaction are in array order (deterministic; no pointer/address dependence)."""
        dt, R, g0, atm_top = self.dt, self.R, self.g0, self.atm_top
        survivors = []
        for p in self.projectiles:
            V = p.tas
            sg = dm.det_sin(p.gamma)
            cg = dm.det_cos(p.gamma)
            # speed: lumped quadratic drag + gravity along the path (uses OLD gamma)
            Vdot = -PROJ_DRAG_K * V * V - g0 * sg
            Vnew = V + Vdot * dt
            if Vnew < V_MIN:
                Vnew = V_MIN
            p.tas = Vnew
            # flight-path angle bends under gravity (n=0 -> gdot = -(g0/Vnew)*cos(gamma))
            gdot = (g0 / Vnew) * (-cg)
            p.gamma = p.gamma + gdot * dt
            # psi unchanged (ballistic: no turn force)
            cgN = dm.det_cos(p.gamma)
            s = Vnew * cgN * dt
            p.lat, p.lon = great_circle_step(p.lat, p.lon, p.psi, s, R)
            sgN = dm.det_sin(p.gamma)
            w = Vnew * sgN
            new_alt = p.alt + w * dt
            hit_ground = new_alt <= 0.0
            if new_alt < 0.0:
                new_alt = 0.0
            if new_alt > atm_top:
                new_alt = atm_top
            p.alt = new_alt
            p.ttl -= 1
            # --- G2 hit detection: round (NEW position) vs every alive enemy aircraft, array order ---
            hit_ac = self._projectile_hit(p)
            if hit_ac >= 0:
                ac = self.aircraft[hit_ac]
                ac.hp = ac.hp - p.damage              # G3: per-round damage carried from the firer's gun
                if ac.hp < 0.0:
                    ac.hp = 0.0
            if p.ttl > 0 and not hit_ground and hit_ac < 0:
                survivors.append(p)
        self.projectiles = survivors

    def _projectile_hit(self, p):
        """Return the index of the first ALIVE aircraft the round `p` hits this tick (horizontal
        great-circle within HIT_RADIUS_M via the law-of-cosines test cosc > COS_HIT_ANGLE AND
        |Δalt| < HIT_ALT_GATE_M, excluding the firer), or -1 for a miss. Array order. det_sin/det_cos
        only — no det_acos: acos is monotone so the distance test reduces to a cos comparison.
        MUST match Kernel::projectile_hit_ in kernel.cpp bit-for-bit."""
        psin = dm.det_sin(p.lat)
        pcos = dm.det_cos(p.lat)
        for j, ac in enumerate(self.aircraft):
            if ac.hp <= 0.0 or j == p.owner:
                continue
            cosc = psin * dm.det_sin(ac.lat) + pcos * dm.det_cos(ac.lat) * dm.det_cos(ac.lon - p.lon)
            if cosc > COS_HIT_ANGLE:
                dalt = p.alt - ac.alt
                if dalt < 0.0:
                    dalt = -dalt
                if dalt < HIT_ALT_GATE_M:
                    return j
        return -1

    def _spawn_projectile(self, ac, owner_idx, env):
        """Spawn a round from aircraft `ac` (its POST-step state this tick): muzzle along the
        velocity vector (psi, gamma), speed = firer TAS + the firer's per-airframe muzzle velocity,
        carrying the firer's per-round damage (G3). The round does NOT advance on its spawn tick
        (it is appended after _advance_projectiles), so it appears at the muzzle."""
        self.projectiles.append(Projectile(
            lat=ac.lat, lon=ac.lon, psi=ac.psi, alt=ac.alt,
            tas=ac.tas + env["muzzle_v_mps"], gamma=ac.gamma,
            damage=env["damage_per_round"], ttl=PROJ_TTL_TICKS, owner=owner_idx))

    def step(self, climb_inputs=None):
        for i, ac in enumerate(self.aircraft):
            req = 0.0 if climb_inputs is None else climb_inputs[i]
            self._advance(ac, req)

    def step_scenario(self, commands, envelopes):
        """Envelope-driven step. commands[i] = (target_phi_rad, target_g[, throttle]);
        throttle defaults to 0.0 if absent. envelopes[i] = dict from envelopes.load_envelope.
        Op order is the sealed byte spec and MUST match Kernel::step(cmd,env) in kernel.cpp exactly.

        B3 (v1.7r0): the commanded load factor n is now bounded per-airframe by the structural g
        limits AND the C_Lmax aerodynamic ceiling (n_aero = cl_max*qS/(m*g0)); below the corner
        speed n_aero is the binding limit and the turn rate collapses as speed bleeds (accelerated
        stall). Retires the B2 global [-3, 9] placeholder. No new det_math. See ADR-Step8-B3.

        B2 (v1.6r0): full 3-DOF point-mass step. Pitch is now REAL: flight-path angle gamma is a
        stored state, driven by the commanded load factor n (g-command) through the lift vector.
        Altitude is EARNED (alt_dot = V*sin gamma); pulling g raises induced drag and bleeds speed
        (energy-vs-turn in the pitch plane). It strictly generalizes the B1 kinematics:
          - wings level (phi=0), n=1, gamma=0 -> level flight;
          - level coordinated turn n=1/cos(phi), gamma=0 -> psi_dot = g0*tan(phi)/V (the old law).
        Rates use the UPDATED speed Vnew (mirrors B1's advance_); the velocity-vector trig uses the
        OLD gamma, position uses the NEW gamma. cos(gamma)->0 (vertical) is a documented singularity
        in psi_dot; scenarios/viewer stay well inside +/-90 deg (real loop/vertical handling is post-B3).
        See ADR-Step8-FlightModel-B2."""
        dt, g0 = self.dt, self.g0
        R, atm_top, soft = self.R, self.atm_top, self.soft
        for i, ac in enumerate(self.aircraft):
            if ac.hp <= 0.0:                      # G2: a DEAD aircraft freezes (no flight integration)
                continue
            V = ac.tas
            e = envelopes[i]
            # --- bank dynamics (unchanged from B1): slew toward clamped target at roll_rate(V) ---
            phimax = dm.lut_eval(e["phi_max"][0], e["phi_max"][1], V)
            rollrate = dm.lut_eval(e["roll_rate"][0], e["roll_rate"][1], V)
            cmdphi = clamp(commands[i][0], -phimax, phimax)
            step_max = rollrate * dt
            delta = cmdphi - ac.phi
            delta = clamp(delta, -step_max, step_max)
            ac.phi = ac.phi + delta
            ac.phi = clamp(ac.phi, -phimax, phimax)
            # --- dynamic pressure (depends only on V; needed for the aero stall ceiling) ---
            q = 0.5 * RHO0 * V * V                        # dynamic pressure
            qS = q * e["wing_area_m2"]
            # --- commanded load factor n, bounded by structural g AND C_Lmax (B3, v1.7r0) ---
            # n_aero = most |n| the wing can lift at this q; below the corner speed it is the
            # binding limit and the turn collapses (accelerated stall). Retires the B2 [-3,9].
            n_aero = e["cl_max"] * qS / (e["mass_kg"] * g0)
            n_hi = e["n_max_struct"]
            if n_aero < n_hi:
                n_hi = n_aero
            n_lo = e["n_min_struct"]
            neg_aero = -n_aero
            if neg_aero > n_lo:
                n_lo = neg_aero
            n = clamp(commands[i][1], n_lo, n_hi)
            # --- trig of NEW phi and OLD gamma (single eval, fixed order) ---
            cphi = dm.det_cos(ac.phi)
            sphi = dm.det_sin(ac.phi)
            cg = dm.det_cos(ac.gamma)
            sg = dm.det_sin(ac.gamma)
            # --- drag/thrust with current V and load factor n (B1 algebra; reuses q, qS above) ---
            L = n * e["mass_kg"] * g0                     # lift = n * weight
            CL = L / qS
            Dp = qS * e["cd0"]                            # parasitic drag
            Di = e["induced_k"] * CL * CL * qS            # induced drag (rises with n -> g bleeds speed)
            D = Dp + Di
            thr = clamp(commands[i][2] if len(commands[i]) > 2 else 0.0, 0.0, 1.0)
            T = thr * e["thrust_static_n"] * (1.0 - V / e["v_max_mps"])
            if T < 0.0:
                T = 0.0
            # --- speed: gravity now acts along the flight path (uses OLD gamma) ---
            Vdot = (T - D) / e["mass_kg"] - g0 * sg
            Vnew = V + Vdot * dt
            if Vnew < V_MIN:
                Vnew = V_MIN
            ac.tas = Vnew
            # --- flight-path angle integrates (uses Vnew, OLD gamma), then track heading turns ---
            gdot = (g0 / Vnew) * (n * cphi - cg)
            ac.gamma = ac.gamma + gdot * dt
            psidot = (g0 / Vnew) * (n * sphi / cg)
            ac.psi = dm.wrap_2pi(ac.psi + psidot * dt)
            # --- horizontal great-circle advance: ground speed = Vnew*cos(NEW gamma) ---
            cgN = dm.det_cos(ac.gamma)
            s = Vnew * cgN * dt
            ac.lat, ac.lon = great_circle_step(ac.lat, ac.lon, ac.psi, s, R)
            # --- altitude EARNED: vertical rate Vnew*sin(NEW gamma), ceiling soft-band predamp + clamp ---
            sgN = dm.det_sin(ac.gamma)
            w = Vnew * sgN
            w_eff = ceiling_climb_rate(w, ac.alt, atm_top, soft)
            ac.alt = clamp(ac.alt + w_eff * dt, 0.0, atm_top)
        # --- G1 (v1.9r0) guns: advance live rounds, then spawn newly-fired ones (post-step muzzle).
        # Order: existing projectiles step+despawn FIRST, then appends -> a fresh round waits at the
        # muzzle this tick. commands[i][3] (fire bool) is OPTIONAL (defaults False) so every prior
        # 3-tuple caller — lockstep_ref/predict_ref/test_energy — keeps working unchanged. ---
        self._advance_projectiles()
        for i, ac in enumerate(self.aircraft):
            # G3 fire-rate: per-aircraft cooldown, decrement-then-fire (shots exactly rof_interval
            # ticks apart while held). A dead aircraft (hp<=0) never fires.
            if ac.fire_cd > 0.0:
                ac.fire_cd = ac.fire_cd - 1.0
            want_fire = len(commands[i]) > 3 and commands[i][3]
            # G4 (v1.13r0): also gate on ammo > 0 — an empty magazine ("Winchester") falls silent
            # (no spawn, no cooldown reset). One round is consumed per shot. Mirror Kernel::step.
            if want_fire and ac.hp > 0.0 and ac.fire_cd == 0.0 and ac.ammo > 0.0:
                self._spawn_projectile(ac, i, envelopes[i])
                ac.ammo = ac.ammo - 1.0                       # G4: one round consumed
                ac.fire_cd = envelopes[i]["rof_interval_ticks"]

    def run(self, ticks):
        for _ in range(ticks):
            self.step()

    def snapshot(self, tick_count):
        buf = bytearray()
        buf += struct.pack("<HHIddII", 1, 0, tick_count & 0xFFFFFFFF,
                           self.dt, self.R, len(self.aircraft), 0)
        for ac in self.aircraft:
            # G2 (v1.10r0): hp is the 8th per-aircraft f64 (after gamma). G3 (v1.11r0): fire_cd (the
            # fire-rate cooldown) is the 9th. G4 (v1.13r0): ammo (the magazine) is the 10th. Appending
            # each grows every snapshot -> all prior goldens move (trajectory/hp/fire_cd/ammo identical
            # for the unchanged scenarios), as gamma did in B2.
            buf += struct.pack("<10d", ac.lat, ac.lon, ac.psi, ac.phi, ac.alt, ac.tas, ac.gamma,
                               ac.hp, ac.fire_cd, ac.ammo)
        # G1 (v1.9r0): projectile block appended after the aircraft block. n_projectiles (u32) + pad,
        # then per round 7 x f64 [lat, lon, psi, alt, tas, gamma, damage] + ttl (u32) + owner (u32);
        # damage was added in G3 (v1.11r0). The block is always present (n=0 for gun-less scenarios),
        # so EVERY prior golden's bytes grow -> all hashes move. See docstring / ADR-Step7-Guns-G1/G3.
        buf += struct.pack("<II", len(self.projectiles) & 0xFFFFFFFF, 0)
        for p in self.projectiles:
            buf += struct.pack("<7dII", p.lat, p.lon, p.psi, p.alt, p.tas, p.gamma, p.damage,
                               p.ttl & 0xFFFFFFFF, p.owner & 0xFFFFFFFF)
        return bytes(buf)


def deg2rad(d):
    return d * (dm.PI / 180.0)


def build_golden(rails):
    g = rails["golden"]
    k = Kernel(rails)
    k.aircraft.append(Aircraft(
        lat=deg2rad(g["start_lat_deg"]),
        lon=deg2rad(g["start_lon_deg"]),
        psi=deg2rad(g["start_psi_deg"]),
        phi=deg2rad(g["start_phi_deg"]),
        alt=float(g["start_alt_m"]),
        tas=float(g["start_tas_mps"]),
    ))
    return k, int(g["ticks"]), g["id"]


def build_scenario(rails, scenario):
    """Build a Kernel + per-aircraft (schedule, envelope) from a scenario JSON doc.
    Returns (kernel, ticks, gid, schedules, envelopes)."""
    k = Kernel(rails)
    schedules = []
    envelopes = []
    for ac in scenario["aircraft"]:
        s = ac["start"]
        env = envmod.load_envelope(ac["envelope"])
        a = Aircraft(
            lat=deg2rad(s["lat_deg"]), lon=deg2rad(s["lon_deg"]),
            psi=deg2rad(s["psi_deg"]), phi=deg2rad(s["phi_deg"]),
            alt=float(s["alt_m"]), tas=float(s["tas_mps"]),
        )
        a.hp = env["hp_start"]                 # G3 (v1.11r0): per-airframe starting hitpoints
        a.ammo = env["ammo_start"]             # G4 (v1.13r0): per-airframe magazine size
        k.aircraft.append(a)
        schedules.append(ac["schedule"])
        envelopes.append(env)
    ticks = int(scenario["ticks"])
    gid = scenario["header"]["id"]
    return k, ticks, gid, schedules, envelopes


def run_scenario(k, ticks, schedules, envelopes):
    """Drive the scripted-timeline. Phase select is integer-only (largest start_tick <= t)."""
    for t in range(ticks):
        cmds = []
        for sched in schedules:
            idx = 0
            for j, ph in enumerate(sched):
                if int(ph["start_tick"]) <= t:
                    idx = j
                else:
                    break
            ph = sched[idx]
            thr = float(ph.get("throttle", 0.0))
            fire = bool(ph.get("fire", False))   # G1 (v1.9r0): per-phase trigger; default off
            cmds.append((deg2rad(ph["bank_deg"]), float(ph["g_cmd"]), thr, fire))
        k.step_scenario(cmds, envelopes)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rails", default=str(ROOT / "config" / "rails" / "atm.json"))
    ap.add_argument("--scenario", default=None,
                    help="run a config/scenarios/<id>.json scenario instead of the straight golden")
    ap.add_argument("--out", default=None)
    ap.add_argument("--seal", action="store_true",
                    help="write sealed golden snapshot + expected.world_hash")
    args = ap.parse_args()

    rails = json.loads(Path(args.rails).read_text(encoding="utf-8"))
    if args.scenario:
        scenario = json.loads(Path(args.scenario).read_text(encoding="utf-8"))
        k, ticks, gid, schedules, envelopes = build_scenario(rails, scenario)
        run_scenario(k, ticks, schedules, envelopes)
    else:
        k, ticks, gid = build_golden(rails)
        k.run(ticks)
    snap = k.snapshot(ticks)
    world_hash = hashlib.sha256(snap).hexdigest()

    print(f"golden={gid} ticks={ticks}")
    ac = k.aircraft[0]
    print(f"final lat={ac.lat!r} lon={ac.lon!r} psi={ac.psi!r} alt={ac.alt!r}")
    print(f"world_hash={world_hash}")

    out = args.out
    if out:
        Path(out).write_bytes(snap)
        print(f"wrote snapshot -> {out}")

    if args.seal:
        gdir = ROOT / "tests" / "golden" / gid
        gdir.mkdir(parents=True, exist_ok=True)
        (gdir / "golden_snapshot.bin").write_bytes(snap)
        (gdir / "expected.world_hash").write_text(world_hash + "\n", encoding="utf-8")
        meta = {
            "id": gid, "seal": rails["header"]["seal"], "ticks": ticks,
            "world_hash": world_hash,
            "snapshot_format": "see ref_kernel.py docstring",
            "generated_by": "tools/ref_kernel.py (Python det_math reference)",
        }
        (gdir / "golden.json").write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
        print(f"SEALED golden under {gdir}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
