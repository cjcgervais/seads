# SESSION HANDOFF 2026-09-05 — scarf triple-fix (idle chatter + wrap sink ×2)

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260905_scarf_triplefix.md; do §5."

## §1 Mission and state

Chad's ask (2026-09-05, immediately after the head landing `c5ff368b1`), three
defects taken together:

1. "The scarf was blowing as if there was wind when I was stopped" — it must
   only move from the riding wind (and real bumps).
2. "The scarf still vanishes into the coat just below the knots when moved all
   the way back on the seat" — the parked coat-sink debt, un-parked by him.
3. "The same spot on the scarf is vanished when the Sudburian stands at the
   flak gun."

**STATE: ALL THREE BUILT AND GATED GREEN at `183da606a`** (red set == baseline
six BY NAME, 1920 tests), pushed on `sandbox/sudburian-head`, **NOT LANDED to
main, AWAITING CHAD'S DRIVE.** Exe built for him:
`D:\seads_sandboxes\sudburian-head-lane\build-play\seads.exe`.
Drive checklist: park and watch the scarf hang dead still; slide full-aft and
look at the collar; man the flak gun and sweep the aim.

## §2 What was found (recon first, fix second — none of it guessed)

### 2.1 Idle motion — the anchor stand-off LATCH (fixed in render/sled_model.cpp)
The flutter was EXONERATED before anything was touched: its gate
(`kScarfFlutterVMinMps` 1.5, `sled_model.cpp` ~:5562) reads `|wind| = |vel|`
and nothing adds to the wind — it cannot fire at a standstill.

The mover: the scarf root's anchor stand-off did
measure-on-the-already-offset-anchor (pre-offset along `back_normal`), correct
along a DIFFERENT axis (`bp_normal[ba]`), and HARD-RESET the stored value to
zero the frame it succeeded. Two-frame self-sustaining oscillation of the
chain root, amplitude up to the whole 42–79 mm stand-off, at ANY speed — the
Verlet solver then reads every root jump as velocity (constraint pushes land
in `p_prev` deltas), so the whole scarf "blows around."

Fix (the `d23ad731f` pod-0 deadband precedent, one call-site over): measure on
the RAW anchor, converge the stored stand-off — instant growth on a real
violation, 3 mm deadband, 50 mm/s decay on release — applied ONCE along the
band's own normal. The short tail inherits the converged value (:5568). The
flak gunner's equivalent (flak_gunner.cpp:935-949) is STATELESS (no latch) and
was left alone.

Residual idle movers, known and deliberately untouched (they are signed
behaviour): the frame field has no idle gate (`SEADS_SCARF_FIELD=0` is the
A/B arm) and the banded planes re-skin off slot-0 joints each frame.

### 2.2 The wrap sink — one root cause for BOTH sled-aft and flak (fixed in the GLB)
The wrap/knot/bridges are rigid AUTHORED geometry (not chain): wrap rode
spine_03 60 / neck_01 40 while the coat in the same band rides spine_03 84 /
upperarm_l+r 8+8. Two surfaces in one place on different bones cannot stay in
order. The NECK term is dominant (wrap 40%, coat 0%):
- full-aft = `ws_neck_counter` rotates neck_01 (up to its 30° cap) exactly
  when aft — the old "aft is the cure" finding held only unsaturated;
- the flak pose aims neck_01 at the sight EVERY frame and IKs both arms to
  the gun — the same divergence, amplified.
No runtime keep-out can move authored verts (three render-side wrap metrics
were previously built and ALL were artefacts — the banded planes do not
describe a wrap around a neck; see the superman-rung history).

Fix, surgical same-count transplant in `assets/sled/indy650.glb` (prim2,
sudburian_scarf_blue, 908 verts): the wrap's HEM BAND — 516 non-scarf-bone
verts, ramp FULL at bind Y ≤ 1.25, OFF by Y ≥ 1.29 (the knots stay
neck-driven) — resamples its skin weights from the k-NN LOCAL coat verts,
plus a 2.5 mm tapered radial proud offset. LBS linearity then keeps
hem-vs-coat relative motion ~0 in every pose. Script pattern:
scratchpad `patch_wrap_weights.py` (this session; rewrite from this doc if
lost — the parameters above are the complete spec).

**Measured** (the attribution method: per-vert LBS displacement difference,
wrap vs nearest coat, under test rotations — `verify_wrap_divergence.py`):

| pose | mean | worst |
|---|---|---|
| flak-style (arms 45° + neck 25°), OLD | 31.5 mm | 101.5 mm |
| flak-style, NEW | **2.9 mm** | **34.3 mm** |
| arms-only 35°, OLD → NEW worst | 16.7 mm | 12.1 mm |

The residual worst lives in the DELIBERATE transition band (Y 1.25–1.29)
where the hem hands off to the neck-following knots — a real scarf slides
there. If Chad still sees a sliver at the knot line, the dials are the ramp
bounds (Y_FULL/Y_NONE) and PROUD_M, one re-run of the patch script each.

## §3 The laws paid for here (do not re-learn)

1. **The wrap is not the chain.** scarf_01..06 + scarf_s01/s02 are
   solver-driven and clamped; the wrap/knot/bridges are authored geometry on
   neck_01/spine_03 that only an ASSET edit can move. "Just below the knots" =
   the hem band, NOT the short tail.
2. **No render-side wrap-vs-coat metric.** Three were tried, all artefacts,
   none in the tree. The honest instrument is weights × rigid transforms on
   the DRAWN vertices (the LBS-divergence script), or Blender ray parity with
   Chad awake.
3. **A stored offset updated by add-or-reset is an oscillator.** Converge with
   a deadband (repo precedent `d23ad731f`); measure on the raw quantity, apply
   along one axis.
4. **`git checkout -- <asset>` restores the COMMITTED (already-patched)
   binary.** Re-patch from the pre-patch commit and CHECK NODE COUNTS
   (base 218 / one head patch 226).
5. `graphify --stale` trips on any later source edit — regen in the LAST
   commit before gating.

## §4 Shared-file announce (SOP 5)

`render/sled_model.cpp` (r4a's) and `assets/sled/indy650.glb` — both already
carried in `[lanes.sudburian-head]`'s announce comment from the head rung;
this rung's edits are the stand-off block (~:5307/:5454-5500) and prim2's
vertex data. `render/flak_gunner.cpp` untouched this rung. Merge-order note
from the sentinel stands: gait / sting-st5 / headlight carry unpushed
`sled_model.cpp` edits and merge main first.

## §5 Next steps, in order

1. **Chad drives** (checklist in §1). Tune dials on his verdict: idle —
   deadband/decay in the stand-off block; wrap — ramp bounds + PROUD_M
   (re-run the patch script against the CURRENT GLB? NO — against
   `183da606a`'s parent asset state; the script composes weight blends, so
   re-running on an already-patched prim2 double-blends. Restore prim2 from
   `cafe8482f:assets/sled/indy650.glb` first, or re-run the whole chain from
   the head-patch base).
2. **Land on his word**, sentinel protocol exactly as the head landing:
   fetch + confirm main; graphify quiet; full gate on the landing tip; merge
   --no-ff; push; append SHA; ping the sentinel session (game-loop lane) —
   they diff, sync seads-recon, REBUILD its build-play (code + asset again).
3. Open debts this rung did NOT touch: the collar transition residual (§2.2);
   the frame field idle gate + slot-0 plane skinning (recon M3/M4, only worth
   it if Chad still sees idle motion after the latch fix); the scarf-lane's
   named wrap/knot ASSET re-author vs the coat collar (this rung's reweight
   may retire it — his eye decides); mullet flutter rate dial (0.30/m/s).

## §6 Sources of truth

- This lane's memory + `docs/SESSION_HANDOFF_20260904_head_glb.md` §5b (the
  head rung this extends).
- The recon report (mechanisms with line numbers) is condensed in §2; the
  full scarf history lives in the superman-rung and scarf-drape-lane memories
  and `docs/SESSION_HANDOFF_20260903_scarf_drape.md`.
- Instruments: `SEADS_SCARF_DEBUG[=2]`, `SEADS_SCARF_FIELD=0`,
  `SEADS_SCARF_FLUTTER`, `SEADS_FLAK_SCARF_DEBUG`, `SEADS_MULLET=0`,
  `drive_scarf_log.bat`.
