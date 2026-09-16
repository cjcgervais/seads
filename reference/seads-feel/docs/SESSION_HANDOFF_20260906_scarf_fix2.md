# SESSION HANDOFF 2026-09-06 — scarf wrap FIX2 (paused near session limit)

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260906_scarf_fix2.md; do §4."

## §1 What happened

Chad drove the 183da606a "triple-fix" and it REGRESSED: the wrap hem sinks into
the coat at pretty much ALL seat stations, worse pitched forward, hem looks
detached (knots did NOT move — measured bit-identical; perceptual). His ruling
for the redo: fresh-context Fable consult designs, Opus builds, mastermind
verifies; overnight auto-run; scarf stays ATOP the coat, attached; still when
parked, motion ramps with riding wind. NO LANDING TO MAIN without his drive.

Root cause (consult, measured, reframes the lane): **the GLB node-rest skeleton
is a seated riding snapshot diverged ~30 cm from the bind pose the IBMs encode**
(|D_spine03 − D_neck01| = 62 mm at the collar). Under that divergence weight
edits re-sculpt drawn geometry at EVERY station — the failed patch (a blunt
neck_01→spine_03 swap, its "k-NN coat resample" claim false, no arm terms)
moved the hem mean 34 / max 145 mm at driven neutral. All prior differential
instruments were blind to it. **Lane law: any instrument for this asset must
include a driven-neutral placement gate (posed pos vs pre-patch ≤ 2 mm).**
A faithful coat-resample weights-only fix measures near-identical to the failed
patch — never ship weights-only on this asset.

Idle-motion side: consult read the latch + flutter code and ruled it MEETS
Chad's spec as coded (parked still, smoothstep onset >1.5 m/s, ramps with v);
dial is kScarfFlutterVMinMps if he wants life below 1.5 m/s. After a big
transient the root settles over ~1–1.6 s — name it in the drive checklist.

## §2 What is built (in this worktree, UNCOMMITTED)

All evidence in `fix2_workpad/` (preserved copy of the session scratchpad):
CONSULT_BRIEF / CONSULT_REPORT (the approved design §B.1 + battery §C + risks),
BUILD_SPEC, **BUILD_REPORT_scarf_wrap_fix2.md** (read §3.4 + §6 first),
patch_wrap_fix2.py, verify_wrap_fix2.py, glb/ (bases from git, fix2.glb,
provenance, battery JSONs), gate.log (INCOMPLETE — killed mid-run at pause).

Working-tree changes:
- `assets/sled/indy650.glb` = FIX2: prim2 restored from cafe8482f then patched —
  coat-kNN(k=4, fold-guarded) full blend, ramp r=(1−smoothstep(1.25,1.29,Y))
  ×(1−smoothstep(45,90 mm,d_coat)), **per-vert bind position+normal
  compensation** p' = M_new(D0)⁻¹·M_old(D0)·p pinned at D0 = node-rest palette
  (builder proved node rest == the engine's TRUE zero-demand parked palette;
  translations (absorb/lean) provably cancel; "mid seat station" phrasing in
  the spec was wrong — mid station is an aft demand). No proud offset. Knots +
  all other prims bit-identical; node count 226.
- `render/sled_model.cpp`: SEADS_RIG_PALETTE_DUMP=path env-gated one-shot
  palette dump (instrumentation only, for re-pinning/validation).
- `generated/graph/*`: regenerated after the code edit.
- `build-play/seads.exe` REBUILT 2026-09-06 00:34 with all of the above.

## §3 Verification state — the open mastermind question

- **P0 driven-neutral gate: PASSES EXACTLY (0.000 mm; failed patch scores
  19.4 mean / 145.5 max on the same instrument).** Full-blend band worst
  clearance drift −76.3 → −11.1 mm.
- **Five numeric drift/penetration gates FAIL as thresholded — but the accepted
  pre-patch asset fails every one of them HARDER in every pose.** Builder's
  §3.4 argues the consult's thresholds as written are unreachable by the
  approved design per the consult's own predicted numbers (thresholds were set
  tighter than its own table). UNRESOLVED: mastermind must re-derive §3.4,
  decide relative-to-prepatch gates vs absolute, and re-rule pass/fail. Do not
  take either agent's word — check the JSONs.
- Repo gate (gate_baseline.py check) was mid-run when paused → verdict UNKNOWN.

**RESOLVED 2026-09-06 (mastermind session):** repo gate re-run in full — red
set == baseline six, member for member (runner-written). Battery re-ruled from
the JSONs: the five threshold failures are transcription errors (the consult's
own predicted table — worst −27 mm / 74 penetrators, called "success" in its
prose — is tighter than the gates it wrote; FIX2 reproduces the prediction to
0.1 mm on the consult's calibration set). Tear = 0.098 mm absolute, no tear.
One §6 overstatement corrected (flak n-10 a-60 is 0.9 mm worse than prepatch on
one metric; everywhere Chad complained FIX2 is 2–4× better). FIX2 ruled good,
committed to the lane branch. See BUILD_REPORT §8. Awaiting Chad's drive (§4.4).
- Revert-in-one-command if needed: `git checkout cafe8482f -- assets/sled/indy650.glb`.

## §4 Next steps, in order

1. Re-run the full repo gate: `python tools/gate/gate_baseline.py check` (let
   the runner write the verdict; red set must == baseline six BY NAME).
2. Mastermind verification of the battery: read BUILD_REPORT §3.4 + the two
   verify JSONs; re-rule the five threshold failures (pre-patch-relative is the
   defensible frame given P0 passes exactly). Spot-check with your own numbers.
3. If ruled good: commit the rung on sandbox/sudburian-head (asset + code +
   graph + docs; fix2_workpad/ stays untracked or add only the two production
   scripts + reports — your call, follow repo binary policy), push lane branch
   ONLY.
4. Drive checklist for Chad (exe `D:\seads_sandboxes\sudburian-head-lane\build-play\seads.exe`):
   park & look at collar (still scarf; one slow ≤1.6 s settle after a transient
   is normal); slide every seat station incl. full-aft; crouch/pitch forward;
   flak gun sweep; head look-around; one fast run (motion ramps with speed).
   Residual by design: up to ~−25 mm transient sliver in the hem↔knot sliding
   band at aft-sat/flak — his eye rules.
5. If his eye dislikes the parked look: re-pin to a live palette —
   `SEADS_RIG_PALETTE_DUMP=D:/tmp/pal.json build-play/seads.exe` (park, quit),
   `python fix2_workpad/patch_wrap_fix2.py --palette D:/tmp/pal.json`, re-verify.
   ⚠ patch always restores from cafe8482f internally — never hand-feed it an
   already-patched GLB.
6. Land to main ONLY on Chad's word, sentinel protocol as the head landing.

## §5 SCARF-STILL rung (2026-09-06, same session, after Chad's "nope the
## scarf still bouncing when parked")

The idle-motion "MEETS spec as coded" ruling was WRONG — overturned by
measurement, not code-reading. Reproduced headlessly (SEADS_SLED_DEBUG_MODE=1
smoke, tail-tip per-frame series): at |v|=0.00 the tail swung 11 mm/frame mean
/ 48 max, FOREVER. Not flutter (gated), not the latch (stand-off constant),
not breath (a plume). Root cause: **every keep-out projection in
trail_chain.cpp was a bare `p += d`, which Verlet reads as velocity d/dt next
step — a pogo kick with restitution ~1.** At a standstill the loop
gravity→push→fly→fall→push never dies; the pod-0 steer's deliberate /0.6
overshoot (gain 1.67) was its loudest voice (66 mm bursts in the class
accounting). The R4a frame field's never-exactly-zero jitter merely seeds it
(the banner's "exactly (0,0,0)" bit-freeze is the only reason FIELD0 parks).

Fix (render/trail_chain.cpp, `rest_push`): after each back-plane-class
projection, cancel the OUTWARD implied normal velocity only — the standard
inelastic contact. Approach absorption stays (a fully velocity-neutral push
was tried and MEASURED WORSE: it un-floors the contact, gravity creep 2.6
mm/frame). Positions per frame are bit-identical; only the phantom kinetic
injection is gone. Head/seat pushes untouched (measured 0.00 injectors); body
chain unaffected (no back planes); flak-gunner scarf inherits via the shared
solver. Also kept: SCARF-FIELD line under SEADS_SCARF_DEBUG (field
accel/alpha/omega per frame, the instrument that exonerated the field's
magnitude). A speed-gate on the field was built and REVERTED — treats the
symptom, and the root fix makes it unnecessary.

Verified (900-frame smokes, tail step mean mm at frames 700-860):
park 12.58→**0.000**; crouch-hold kick 13.77→**0.019**; drive@11 m/s
1.15→0.56 solver motion (the removed half WAS the pogo chatter; the drawn
flutter wave rides on top unchanged). Repo gate: baseline six, member for
member. OWED before main: fresh-context red-team of the solver change
(red-team law; it touches every trail-chain consumer).
