# T13 audit numbers — chamber centering (S2), chambers off (S1), entry trench (S3)

All measured by `test_tunnel_audit_probe.cpp` (run `ctest -R AUDIT
--output-on-failure`). The uniform-field rows use the 300 m test heightfield;
the real-DEM row decodes the shipped Sudbury DEM headlessly (the same path
`test_tunnel_mesh.cpp` T5d uses).

## S2 — chamber centering (arena_a_m 7350 → 4200)

The room (arena) is centered on the geodesic midpoint (slerp t=0.5). The two
bore legs run from each mouth to where the bore first breaks into the arena
ellipsoid; the crossing spans between the two breaches. S2 wants three
comparable thirds.

| arena_a_m | E-leg (m) | crossing (m) | M-leg (m) | leg-skew | crossing / mean-leg |
|-----------|-----------|--------------|-----------|----------|---------------------|
| 7350 (old, uniform) | 3176.2 | 5909.9 | 2957.5 | 7% | 1.86 (room swallows the tunnels) |
| **4200 (T13, uniform)** | **4034.5** | **4054.9** | **3954.2** | **2.0%** | **1.02 (three comparable thirds)** |
| 4200 (T13, real DEM) | 3984.1 | 3996.1 | 3926.8 | 1.4% | ~1.02 |

The gating leg `T13 chamber centering` (test_tunnel.cpp) pins: legs within 15%
of each other, crossing in [0.5×, 1.5×] the mean leg, arena center dir == slerp
t=0.5 to 1e-9. Mutation-verified: arena_a_m=7350 → crossing 5910 > 1.5× mean-leg
4600 → the crossing check FAILS.

## S1 — chambers gated off (`chambers_on = false`)

The two pump-room side chambers + their connectors are structurally inert
(Pockets built with b==0, Capsules with r==0 → SDF/mesh/lamps all skip them).
Off-arm piece count 4 (tube + 2 collars + arena) vs on-arm 8 (+2 chambers +2
connectors). The old chamber centers read solid rock (sd > 0) off-arm.

## S3 — entry approach trench (`trench_len_m = 450`, `trench_rim_m = 150`)

The Errington mouth is sunk 130 m behind the pit rim; a bore-tangent entry line
clips rock behind the pit. The T13 up-tangent open-cut trench (3 stepped open-cut
Bowls marching away from the underground route, depths ramping 60 m → ~pit depth)
excavates that approach, entirely over NO tunnel (the T12 crown-see-through class
cannot re-open).

Blocked-tick count along the bore-tangent entry chord (600 m out, into the mouth):

| measure | Errington ON | Errington OFF | Murray (ref) |
|---------|--------------|---------------|--------------|
| geometric proxy (below terrain & outside net, 100 samples) | 0 | 25 | 0 |
| LIVE plant crashes (`sim::step`, deep-penetration clause) | 0 | 5 | 0 |

The gating leg `T13 entry approach` pins: OFF ≥ 3 (real problem), ON ≤ 2, ON <
OFF. Mutation-verified: disabling the trench in the SDF → ON blocked 5 (= OFF) →
the ON≤2 check FAILS.

## Real-DEM sanity (A4)

Real terrain at the two mouths: Errington 15095.3 m (95.3 AMSL), Murray 15146.0 m
(146.0 AMSL), diff −50.7 m. Real leg skew **1.4%** — well under 10%, so the
uniform-field arena_a_m=4200 centering holds on the real terrain and no
pilot-ruling auto-centering deferral is needed.
