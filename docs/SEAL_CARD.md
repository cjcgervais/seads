# Master Seal Card

**Seal:** ATM-Sphere v1.7r0
**Realm:** ATM-only
**Geometry:** R=15000 m, flattening=0
**Tick:** Δt=0.01 s (100 Hz)
**Gravity:** g₀=9.80665 m/s² (constant)
**Ceiling:** ATM_TOP=8000 m, SOFT=100 m
**Atmosphere:** still air; constant density ρ₀=1.225 kg/m³ (B1/B2 energy model; ISA-vs-altitude deferred to B5)
**Determinism:** det_math only; ban libm/fast-math/FMA/x87
**Wire/Hash:** GEO-001 (lat/lon×1e7, bearing×1e6, h×1e3; ZigZag+LEB128) + **KIN-002** aux block (phi×1e6, tas×1e3, **gamma×1e6**) — snapshot **protocol 3** (B2 added gamma to the wire; throttle/target_g are inputs, not state)
**Flight model:** B3 limits & stall (B2 lift & pitch) — full **3-DOF point mass**. Flight-path angle **γ is a stored state** (per-aircraft state is now the 7-tuple `lat,lon,psi,phi,alt,tas,gamma`), driven by commanded load factor `n` (`Command.target_g`). Altitude is **earned** (`alṫ = V·sin γ`); pulling g raises induced drag → bleeds speed. Generalizes B1 (`n=1/cosφ, γ=0` ⇒ the old `ψ̇=g₀tanφ/V` level turn). Builds on B1 energy (TAS from thrust−drag). V_MIN=30 m/s. **B3 (v1.7r0):** the achievable `n` is bounded per-airframe by BOTH the structural g limits (`n_min_struct`/`n_max_struct`) AND the C_Lmax aerodynamic ceiling `n_aero = cl_max*qS/(m*g0)` — `n = clamp(n_cmd, max(n_min_struct,-n_aero), min(n_max_struct,n_aero))`. Below the corner speed `V* = stall*sqrt(n_max_struct)` the aero ceiling binds and the turn collapses (**accelerated stall**); the 1 g stall speed falls out for free. Retires the B2 global placeholder [-3,9]; post-stall lift held at C_Lmax (no departure). No new det_math (det_sin/det_cos only).
**Roster (8):** P-47D · Bf 109 F-4 · Ki-61 · A6M2 · Yak-3 · La-7 · Spitfire Mk V · P-51
**Goldens (B3 added GOLDEN-SK-Stall-001 ONLY; the six below are byte-identical to v1.6r0 — the B3 limits are a conservative extension that does not bind in any prior scenario):**
- GOLDEN-SK-Sphere-001 — 10,000 ticks; (0°,0°), ψ=45°, TAS=250 m/s — `db777327…d13ac394` (kinematic anchor; trajectory unchanged, hash moved only because γ=0 appended)
- GOLDEN-SK-Turn-001 — ki61, level coordinated 45° turn @ g=1/cos45, 6,000 ticks — `3faca110…9fc8e57f`
- GOLDEN-SK-Climb-001 — bf109f4, pull-up coasting into the ceiling soft band, 3,000 ticks — `9d0eb912…4a0c3026`
- GOLDEN-SK-TurnClimb-001 — spitfire_mk5, banked climbing pull, 4,000 ticks — `cd705c4a…4bda5c9c`
- GOLDEN-SK-Accel-001 — p51, wings-level 1 g throttle program (idle→full→idle), 5,000 ticks — `9fb59805…de8c3aaf`
- GOLDEN-SK-Pitch-001 — p51, wings-level pull (g=1.8 zoom climb) then push (g=0.3 dive), 3,000 ticks — `c0332e9e…6fd379ea` (new in B2)
- GOLDEN-SK-Stall-001 — 2-ship V-n limit: ki61 banked g=2 turn bled into **accelerated stall** (aero ceiling binds, then recovers) + p47d wings-level g-limit zooms (**structural** cap), 3,000 ticks — `1a57b6d1…a0526fc6` (**new in B3**)

## Seal history
| Seal | Date | Change |
|------|------|--------|
| v1.1r1 | 2025-10 | Roster-8 sealed; ATM_TOP=6000 m |
| v1.2r0 | 2025-10-12 | ATM_TOP 6000→8000 m (+100 m soft band); kernel/wire unchanged |
| v1.2r0 (repo) | (Pass 1) | Deterministic core + harness stood up; golden sealed |
| v1.3r0 | 2026-06-28 | Envelope-driven flight inputs (bank/climb) + scripted-timeline goldens (Turn/Climb/TurnClimb). Sphere hash unchanged; no rail change. |
| v1.4r0 | 2026-06-28 | **Wire reseal:** auxiliary KIN-001 block (phi×1e6, tas×1e3) carries bank/speed for client-side prediction (netcode layer 4b); snapshot protocol 1→2 (KIN as a 2nd self-delimiting section). GEO-001 codec byte-unchanged; kernel/det_math/snapshot-layout untouched; **all 4 goldens unchanged**. |
| v1.5r0 | 2026-06-29 | **Flight model B1 (longitudinal energy):** TAS is now an integrated state (thrust − parasitic/induced drag − climb cost); `Command` gains throttle; per-airframe aero params added (constant ρ₀, V_MIN=30). No new det_math (uses +−×÷ and det_cos). Energy lives in `step(cmd,env)` only ⇒ **GOLDEN-SK-Sphere-001 unchanged**; Turn/Climb/TurnClimb **regenerated** + new **GOLDEN-SK-Accel-001**. Wire/snapshot layout unchanged (throttle is an input). See ADR-Step8-FlightModel-B1. |
| v1.6r0 | 2026-06-29 | **Flight model B2 (lift & pitch):** flight-path angle **γ promoted to stored state** (state 6-tuple → 7-tuple); full 3-DOF point mass driven by commanded load factor (`Command.target_climb`→`target_g`); altitude earned (`alṫ=V·sinγ`); pulling g bleeds speed. No new det_math (det_sin/det_cos). **Wire reseal KIN-001→KIN-002** (gamma×1e6; snapshot protocol 2→3). **ALL goldens regenerated** (incl. Sphere — γ joined the canonical snapshot; trajectory unchanged) + new **GOLDEN-SK-Pitch-001**. Generalizes B1's level turn. Vertical (cosγ→0) is a documented singularity; per-airframe C_Lmax/stall = B3. See ADR-Step8-FlightModel-B2. |
| v1.7r0 | 2026-06-29 | **Flight model B3 (limits & stall):** achievable load factor `n` now bounded per-airframe by the **structural g limits** (`n_max_struct`/`n_min_struct`) AND the **C_Lmax aerodynamic ceiling** (`n_aero=cl_max·qS/(m·g₀)`) — retires the B2 global [−3,9]. **Accelerated stall + corner speed emergent**; 1 g stall speed coherent with each envelope's `stall_tas_mps`. New envelope scalars `cl_max,n_max_struct,n_min_struct` (8 airframes). **No new det_math, no state-vector/wire change** (n is derived). Conservative extension ⇒ **the 6 prior goldens are byte-identical** (limits never bind in those scenarios); only **GOLDEN-SK-Stall-001** added. See ADR-Step8-FlightModel-B3. |
