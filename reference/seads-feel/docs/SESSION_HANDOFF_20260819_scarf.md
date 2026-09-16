# HANDOFF — R2c-7s THE SCARF + the rider-winding rung: **CLOSED 2026-08-21**

**LAUNCH LINE:** *"Read `CLAUDE.md`'s three standing laws and
`docs/RIDER_AUTHORITY.md`, then `docs/SCARF_SPEC.md` §8h–§8m (the closing
sections) and §1 of THIS file. **This rung is CLOSED and SIGNED — do not reopen
it.** Chad's verdicts: 'that perfect ty all done… the mechanics are sound,
helmet is good', and the face port is 'grey box is fine, sudburian is camera
shy anyways'. If Chad names a NEW item, start from his words, not from here."*

## 0. STATE IN ONE LINE

**The source now produces what ships.** (The rung closed at `e362c1123`; that
is a HISTORICAL MARKER, not a claim about where `main` is — it had already moved
before this sentence was written. This document deliberately does not tell you
"main = <hash>": naming a hash is what rotted the v9 recipe, and a handoff that
does it goes stale faster than anything else in the tree. Run `git log`.) The `.blend` is the
source of truth, its export is the shipping GLB, both post-passes are retired,
and the winding measurement that found four separate defects now lives in ctest
where it runs on every build. Gate **1516/1520** — the same four pre-existing
GI4 `test_sled` debts that predate this rung and belong to the GI4 lane, no new
failure name. `seads-recon` is on `main`, clean, and its `build-play` binary is
REBUILT against the reconciled bytes (the GLB changed with the reconcile — a
stale fly binary after another lane pushes is this program's most repeated
trap; rebuild before judging anything).

## 0a. WHAT A FRESH AGENT MUST NOT UNDO

1. **The rider is SIGNED.** Scarf (wide loose band, flutter), v13 EGG helmet on
   the head, block gone, all 34 shells outward. Chad has judged all of it.
2. **No face piece, ever.** RULED. Two lanes each reasoned their way toward
   authoring one off an inference; the real answer was that nothing needed
   fixing. See §8m.
3. **`test/unit/test_rider_winding.cpp` is a GATE LEG, not scaffolding.** It is
   mutation-verified. If it goes red, an asset regressed — fix the asset, never
   the test.
4. **Regenerating off a pre-reconcile base is GONE, not degraded** (§8h recipe
   block). It silently yields see-through limbs, a helmet off the neck and the
   block attached. The supported path is `export_live_v13` → `splice_sudburian.py`.
5. **The bone-count discriminator is "19 = legacy, ANY OTHER COUNT = Sudburian"**
   (44 today). Do NOT re-pin it to a new literal; the Sudburian grows, the
   legacy 19 is frozen.

## 0b. THE FOUR LESSONS THIS RUNG PAID FOR

- **A report can be true and answer a question nobody asked.** Three instances
  here: `assert v_after > 0` reporting a positive TOTAL over nine inverted
  shells; a suite test named *"every ingested GLB has outward winding"* that
  reads only AIRCRAFT meshes and never the rider; and a runner printing
  `[exited with code 0]` while ctest printed four FAILED lines. **Check what a
  check touches, never its name; an exit code is a summary someone else
  computed.**
- **Signed volume is a winding test only on a CLOSED, ORIENTABLE surface, per
  shell, welded ACROSS primitives.** A glTF primitive is a MATERIAL SUBSET, not
  a shell. Both halves were learned by getting them wrong in opposite
  directions (§8k, §8l).
- **Measure the attachment, not the silhouette.** The helmet shipped 0.33 m off
  the neck past rear/side smoke shots that still "looked fine".
- **Every real finding came from re-measuring the artefact AFTER already having
  an answer we liked** — on both lanes, in both directions.

## 0c. THE BUILD-BY-BUILD LEDGER (historical, newest first)

**★★★★★★ 2026-08-20 latest — EIGHTH BUILD (spec §8h): the 'transparent Sudburian'
ROOT-CAUSED AND KILLED — the suit prim was wound INSIDE-OUT since before the
scarf existed (backface culling ate the back wall; winter-gi's GLB carries the
same defect, flagged for merge). Band now HEAD-WIDTH and loose (inner 0.106,
dia ~0.27), tails 0.132 wide, and the ruled FLUTTER is in: a traveling wave on
a copy of the solved chain — higher frequency / lower amplitude with speed,
never flat, never repeating; SEADS_SCARF_FLUTTER dials it. Gate 1365/1369
(same 4 GI4 debts). AWAITING CHAD'S EYE.**

**★★★★★ 2026-08-20 night — THE CONTEXT-FREE CONSULT RAN (Chad's order) AND THE SEVENTH BUILD SHIPPED (spec §8g): banded drawn-back keep-out planes, band clear of the neck prism, roots embedded in the band arcing over the collar, bones moved with them, glTF byteLength bug fixed. The burial/'transparent' mechanism is CLOSED with measurements. AWAITING CHAD'S EYE.**

**★★★★ 2026-08-20 eve — SIXTH RULING BUILT (spec §8f): SIMPLE — one broad band ABOVE the collar (the 'transparent/inside the model' z-fight killed), NO knot, two flat UNTAPERED ends, short 0.136 = exactly 1/3 of long 0.408. AWAITING CHAD'S EYE.**

**★★★ 2026-08-20 pm — FOURTH+FIFTH RULINGS BUILT (R2c-7s(e), spec §8e): short tail ALIVE on two NEW bones scarf_s01/s02 (rig 42→44), tail rests ON the back (tube-half keep-out), 2 clean wraps, hang 0.408 m, and everything FLAT FABRIC not tubes (twisted strips). AWAITING CHAD'S EYE.**

**★★ 2026-08-20 — THIRD DRIVE: THE AUTHORED FLUFFY SCARF (R2c-7s(d)) — see `docs/SCARF_SPEC.md` §8d.** Wrap (2 loops, bunched back) + FRONT knot + short LEFT tail (0.305) + long RIGHT tail (one tube on the six bones, 0.288 hang) + kComplementBlue re-verified; ribbons retired; built in the OPEN Blender session (append-only, body proven untouched) + patched into the GLB (census + IBM-frame-proof + read-back). AWAITING CHAD'S EYE.

**★ 2026-08-19 pm — CHAD DROVE IT, RULED, ALL FOUR BUILT (see `docs/SCARF_SPEC.md` §8c):**
settle 0.960→0.930 (halved), face 0.11→0.14 m, long end 0.48→0.41 m, a SECOND
SHORT END (4 links, 0.27 m, own ribbon, no bones) beside it. **COLOUR VERIFIED
INDEPENDENTLY: `indy_red` is the machine's body red from the GLB, NOT a
`team_color.h` thematic hue — that is OPEN for Chad's pick** (keep, or
`kComplementBlue` / `kSlagOrange` wired through `team_color.h`). Gate 1249/1253
(same 4 GI4 debts). The old (a)/(b)/(c) below are superseded by §8c.

| | |
|---|---|
| this rung | **R2c-7s THE SCARF = the proto-SUPERMAN trailing-chain solver** (ladder §5 + §7.6). Built overnight while Chad slept, on his instruction ("operate automatically … a complete committed and shipped feature … also for seads-recon build-play"). Opus built; Fable orchestrated, prototyped the numbers, and red-teamed twice (spec, then artefact) |
| the asset | `render/trail_chain.h/.cpp` — PURE (glm+std, in `seads_render_core`, headlessly tested), fixed segments / fixed iterations / fixed dt / no contact solver, the exact thing R4 re-anchors onto the body for superman |
| the wiring | `render/sled_model.cpp` drives the SIX EXISTING `scarf_01..06` bones of the shipped 42-joint skin every frame and draws a procedural two-sided ribbon off the same frames; `SledRig` carries `vel_body_mps / ticks / dt_s` from the kernel (render reads no clock — tick-stepped) |
| gate | `trail_chain` 12 cases / 24 042 assertions green; full suite 1249/1253 — the SAME four pre-existing GI4 `test_sled` debts as the base `b071770ce` (1237/1241), nothing else; `graph_query.py check` OK, graph regenerated in the commit |
| evidence | `--smoke 900` shots (rear quarter `SEADS_SLEDCAM="4.0,155,22"`, side `"3.0,90,8"`, daylight offset 120): hang 31.1° (lying ALONG the 33°-leaning back — the drape angle IS the torso tilt), 8 m/s 43.8°, 20 m/s 78.7°, left-slide → trails behind-RIGHT (az +154°); bind log `back surface 0.0747 m behind the torso plane, keep-out 0.0867, anchor stand-off 0.0190`; `SEADS_SCARF=0` byte-identical between two runs. Not committed (`scratch/`), regenerate with §3 |
| red-teams | TWO context-free Fable red-teams (spec v1→v2; built artefact→v3), both BUILD/SHIP-WITH-FIXES, every fix folded: `docs/SCARF_SPEC.md` §8 + §8b |
| open for Chad | (a) **the scarf COLOUR** — §4 never named one; shipped = the machine's own `indy_red` from the GLB; `SEADS_SCARF_COLOR="r,g,b"` A/Bs it live. (b) **the "+" cross-section** (a second 0.07 m strip along the bones' Z so the side view reads; `SEADS_SCARF_CROSS=0` for the single flat strip) — his eye. (c) width 0.11 m / length 0.48 m (the rig's 6×0.08 bones) — a felt call; an authored cloth on the same six bones retires the ribbon. (d) whether he wants a forced flutter (DROPPED: not in §5, sat on a resonance cliff; the whip from bumps/absorb/speed changes is the life). (e) the knot bone `scarf_01` is authored 7 mm INSIDE the suit's back — the code stands the anchor off by a MEASURED 19 mm; the art rung should move the bone onto the surface |

## 1. WHAT WAS MEASURED (not assumed)

- `scarf_01..06` exist in `assets/sled/indy650.glb` as skin joints 8..13 of `sudburian_rig`; `scarf_01` is a child of `neck_01` at local `(0, −0.0029, 0.0750)` rot `(0, 0.1452, 0.9894, 0)`; `scarf_02..06` each `+0.080 m` on the parent's +Y. **Zero vertices weight to any scarf joint** — bones yes, cloth no. So: drive the bones, draw a ribbon, and an authored cloth later skins on with no code change (spec §0, THE ONE DEPARTURE from "skinned", stated).
- Rest world of `scarf_01`: pos `(0, 1.1308, −0.3712)`, +Y `(0, −0.8934, −0.4493)`, +X `(1,0,0)`, +Z `(0, 0.4493, −0.8934)` — pinned by test 9b so a future cloth can never be X/Z-flipped.
- The posed torso leans **33° forward** (pelvis (0.684,−0.585) → neck (1.121,−0.297)): a vertical back plane let the idle scarf hang THROUGH the chest (red-team P1-1). The keep-out is the POSED torso plane (§3b), so the hang lies along the back at ~31° — the in-game v0 shot measures 30.2°.
- Lift law: a massless chain in uniform wind follows `tan θ = k v²/g` to 0.1° from 6 m/s up; k = 0.153 → 45° @ 8 m/s, 81° @ 20. Damping 0.990/step left the chain swinging ~20 s; shipped 0.960 settles in ~2.6 s. (Prototype numbers in `docs/SCARF_SPEC.md` §3.)
- `indy_red` baseColorFactor (0.450, 0.022, 0.022) → bytes (114,5,5), captured at load, never retyped.

## 2. DEVIATIONS THE CODE MADE FROM THE SPEC (all deliberate, all commented)

1. `trail_chain_frames` takes the state by non-const ref (`last_x` read+write) — the spec left it to the builder.
2. The effective-size rule is applied to the back plane too (`back_d = min(keepout, dot(anchor−origin, n))`) — needed for §5's own test rig; a no-op on the shipped rig (anchor 67.7 mm behind the plane vs 60 keep-out).
3. **A re-prime costs one sub-step** (`needs_solve`): the primed chain is constraint-solved then frozen (p_prev = p), not integrated. REAL BUG FOUND: a straight-down prime started 248 mm inside the torso plane, Verlet read the correction as 30 m/s and parked the chain straight UP the spine at 145° lift (the plane's inverted fixed point). Measured 145.0° → 31.4° after the fix.
4. Test 7's "lift < 25°" was arithmetically impossible (the drape angle IS the torso tilt, 33°) → gated 25°..35°, measured 31.4°.
5. Test 6 (head sphere) runs with the back plane off + a non-vacuity clause, because with both armed the plane stops the chain before the sphere.
6. `n_neck` comes from the declared catalogue (`rj[kRiderNeck]`), not a bare `find_node`.
7. (v3, after the artefact red-team) the back keep-out is MEASURED at bind from the rest-skinned proxy (surface + 12 mm) and the anchor stood off the knot bone by the difference; the ribbon is a "+" of two strips. Both in `docs/SCARF_SPEC.md` §8b.

## 3. HOW TO LOOK AT IT

From the repo root with a `build-play` binary:
```
SEADS_SLED_DEBUG_MODE=1 SEADS_SLEDCAM="4.0,155,22" SEADS_SCARF_DEBUG=1 \
  SEADS_SCARF_VEL="0,0,-8" ./build-play/seads.exe --smoke 900 scratch/scarf/q8.png 120
```
`SEADS_SCARF_VEL` = body-frame VELOCITY (forward 8 m/s = `0,0,-8`; a left slide
`-6,0,-12`). `SEADS_SCARF=0` off. `SEADS_SCARF_COLOR="r,g,b"`. The debug line
prints |v|, lift (deg), chord azimuth (body), tail, head/back clearance, steps.
Or just DRIVE: the scarf hangs at rest, lifts with speed, swings out on a slide.

## 4. CO-EXISTENCE WITH THE HELMET RUNG (winter-gi, `sandbox/gi4-ride`)

The helmet agent was editing `render/sled_model.cpp` (an `SPrim::gloss`, the
fragment shader, a material-name block, one `SetShaderValue(uGloss)` in the
prim loop) while this rung was built in a SEPARATE worktree. The scarf's hunks
were placed clear of those (fields next to `hinge{}`, colour capture before
`cgltf_free`, its own draw block after the prim loop). **One line is owed at
merge time:** the ribbon draw block must also set the helmet's `uGloss` to 0
(uniforms persist from the last prim, which may be the glossy helmet), i.e.
`SetShaderValue(sm.shader, sm.loc_gloss, &zero, SHADER_UNIFORM_FLOAT);` next
to the `loc_emit` line in the scarf block. Whoever merges second adds it.

## 5. NEXT

Chad's eye (colour, width, feel at speed) → costume/cloth art rung can author a
skinned scarf onto the six bones → R4 superman re-anchors `trail_chain` at the
grips with the body as the chain (§7.6; keep-outs disable with r = 0 /
back_normal = 0). Roost/spray reaction stays the §5 open detail.
