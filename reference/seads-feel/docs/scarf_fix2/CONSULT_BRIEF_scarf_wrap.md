# Fresh-context consult brief — scarf wrap regression (2026-09-05 overnight)

You are a fresh-context adversarial consultant. Re-derive from the artefacts; trust
nothing below until you've checked it. Deliverable = a written fix DESIGN (not code)
plus a verification-instrument design, written to
`C:\Users\Chad\AppData\Local\Temp\claude\D--flight-sim2\cc06baec-48f2-4d7a-ad43-d76782a5b65a\scratchpad\CONSULT_REPORT_scarf_wrap.md`.

## The user-visible defect (Chad, 2026-09-05, after driving lane tip e1a087523)

A previous agent patched the scarf WRAP (rigid authored geometry, not the solver
chain) in `assets/sled/indy650.glb` to stop it sinking into the coat at full-aft
seat position and at the flak gun. Chad drove it and reports it is now WORSE:

- the wrap hem now goes INTO the coat at pretty much ALL seat stations;
- worse with torso pitched forward;
- the scarf hem looks DETACHED (and the knots appear to have moved up — measured:
  knot verts Y>=1.29 are bit-identical, so that is perceptual, likely the hem
  dropping away from the knots);
- separately, required behaviour: scarf must hang still when parked (subtle wind
  at most), motion ramping up with speed of the riding wind.

## Ground truth already measured (verify yourself if suspicious)

Repo worktree: `D:\seads_sandboxes\sudburian-head-lane` (branch sandbox/sudburian-head,
tip e1a087523; the bad patch commit is 183da606a; pre-patch asset =
`cafe8482f:assets/sled/indy650.glb`).

Extracted GLBs + inspection script (python, runnable):
- `...\scratchpad\glb\prepatch.glb` (cafe8482f state)
- `...\scratchpad\glb\current.glb` (183da606a state = what Chad drove)
- `...\scratchpad\inspect_wrap.py` (usage: python inspect_wrap.py glb/prepatch.glb glb/current.glb)

Rider mesh = `sudburian_proxy_R2cM.002` (mesh idx 132), 6 prims:
prim0 = grey under-suit (6384 v), prim2 = scarf wrap/knots/short-tail authored geo
(908 v, material sudburian_scarf_blue, bind Y 0.821–1.308), prim3 = COAT
(hoodie_grey, 12630 v). Skin = skins[0], 44 joints.

Measured diff prepatch→current, prim2 only (all other prims bit-identical):
- 516 verts pos-changed (max 2.5 mm — the "proud offset"), 467 weights-changed,
  116 joints-changed; knots (Y>=1.29, 48 v) untouched.
- Y<=1.25 band weight dominance: OLD neck_01 .227 / spine_03 .189 / scarf bones ~.31
  → NEW spine_03 .416 / scarf bones unchanged / neck_01 ~0. I.e. the patch moved
  ALL neck weight onto spine_03. NO upperarm weight was added.
- 1.25<Y<1.29 transition band: OLD spine .529/neck .471 → NEW spine .807/neck .191,
  upperarm ~.001.
- The COAT's actual local blend at the same heights (prim3):
  Y1.2–1.25 spine_03 .877 + upperarm_l .061 + upperarm_r .061;
  Y1.25–1.29 spine_03 .833 + upperarm .084+.084.
  (prim0 under-suit at Y1.25-1.29 carries head .071/neck .047 — do NOT confuse it
  with the coat.)

So the patch did NOT reproduce the coat's blend (claimed "k-NN resample from local
coat verts" in docs/SESSION_HANDOFF_20260905_scarf_triplefix.md §2.2 — read it for
history): it dropped neck-follow and added no arm-follow.

## My working hypothesis — ATTACK THIS

In the nominal riding pose the arms are always forward on the handlebars, so the
coat collar/chest verts (8–17% upperarm weight) are dragged forward/down relative
to a pure-spine_03 surface. The old hem's neck_01 weight (~.23–.47) rotated it up
and back with ws_neck_counter, which HAPPENED to keep it clear of the coat at most
stations (failing only when neck saturated aft / flak aim). The new hem is
pure-spine: it lost the accidental neck lift AND never gained the arm-follow, so
the coat now moves through it at essentially every posed station → sinking
everywhere + a hem that no longer follows the neck-driven knots above it →
"detached" look.

## Constraints and laws (from lane memory + repo docs — respect unless you can refute)

1. The wrap is AUTHORED rigid skinned geometry; the solver chain (scarf_01..06,
   scarf_s01/s02 bones) is separate and working. Fix the asset, not the chain.
2. Three render-side wrap-vs-coat metrics were built previously and were ALL
   artefacts; do not design a runtime metric off the banded planes. The honest
   instrument is per-vertex LBS: weights x rigid joint transforms on the drawn
   verts (a verify script existed: flak-style pose OLD mean 31.5 mm / worst 101.5).
   NOTE the failed patch PASSED that instrument (NEW 2.9/34.3) — the instrument
   under-tested: no upperarm-forward nominal pose, no hem-vs-KNOT gap metric, no
   sweep of seat stations. Your verification design must close those holes.
3. Same-count surgical patch only: full GLB re-export CRASHES the mount. Patch =
   rewrite JOINTS_0/WEIGHTS_0/POSITION bytes of existing accessors, counts
   unchanged. Base node count 218 / after head patch 226 — sanity-check.
4. Never re-run a weight-blend patch on an already-patched prim2 (double-blend);
   always restore prim2 from cafe8482f first.
5. Chad ruling: scarf stays ATOP the coat, attached-looking; knots stay wrapping
   the neck (they must follow neck). Also: NO new bones (skin stays 44 joints).
6. The engine skins the rider on the CPU (there is a mullet wind vertex pass in
   the CPU skinning loop, render/sled_model.cpp), so a small deterministic
   runtime vertex post-pass on prim2 IS technically available — but prefer the
   asset fix; runtime passes on the wrap have a bad track record here.

## Questions for you

A. Confirm or refute the hypothesis above by computing posed LBS positions
   yourself (rotate spine_03 / neck_01 / upperarms through realistic riding,
   full-aft, and flak poses) on both GLBs. Quantify hem-vs-coat penetration AND
   hem-vs-knot separation in each pose, old vs current.
B. Design the correct asset fix. Candidate to attack: restore prim2 from
   cafe8482f, then hem-band verts copy the FULL joints+weights of the nearest
   coat (prim3) vert (true resample, arm terms included), Y-ramped toward the
   authored neck-driven blend at the knots; keep/resize the proud offset; widen
   or reshape the transition band so hem and knots stay visually connected
   (consider geometric overlap: knots' lower rim overlapping the hem's top).
   Say what ramp bounds, offsets, and blend shape are right and WHY. If a
   different design is better (e.g. blending coat-copy weights with a retained
   fraction of neck for the upper hem), argue it with numbers.
C. Design the verification battery: which poses (include nominal
   arms-on-bars ~40-60 deg forward + spine flex, seat-station sweep of
   ws_neck_counter 0..30 deg, flak aim sweep), which metrics (coat-vs-hem signed
   clearance along coat normal — penetration; hem-top vs knot-bottom gap), and
   the pass thresholds you would gate on.
D. Sanity-check the idle-motion side: read render/sled_model.cpp ~5306-5648
   (stand-off latch + flutter). Chad's spec: still when parked, subtle at low
   speed, ramping with speed. Does the current code meet it; if not, what is the
   minimal change? (Flutter gate at 1.5 m/s, smoothstep 1 m/s above; the latch
   converges with 3 mm deadband / 50 mm/s decay.)

Be adversarial: the LAST fix also gated green and measured 10x better on its own
instrument, and it made the game worse. Name every way the proposed design could
do that again, and how the battery catches it.
