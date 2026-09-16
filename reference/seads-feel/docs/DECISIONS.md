# Decisions (seads-feel copy)

**Provenance:** the four 2026-07-29 entries below are copied CHARACTER-FOR-CHARACTER from
the authoritative ledger `D:\mandalark-kernel\docs\DECISIONS.md` (commits `433038d`
pre-registration, `f2d8368` quote correction, `f20d655` weld ruling, `a44f1d3` + `b4c0751`
addenda; copied at `b4c0751`; the v9 MEASURED + FLOWN-APPROVED entries copied at `932c4e3`). That repo is the guard's ledger and stays authoritative —
edit THERE (via the guard), never here. Entries are newest-first; a later ruling supersedes
an earlier entry's resolution text (in particular, the v8 partial-reject entry's
"restore the anchor / sustain" resolution paragraph is superseded by the weld + instant-snap
rulings above it — no sustain mechanism exists in v9).

**Deferred candidate recorded per the guard's strike (v9 plan):** with the nose as
`orient_snap_dir`'s primary target, the ballistic/tail-slide guard inside it becomes moot —
its removal is a FUTURE candidate only; v9 does not touch it.

---
## 2026-07-29 (night) — v9 FLOWN AND APPROVED: all three conditions pass; CQ2 window KEPT by ruling

**Chad flew v9 (`a307a8a69` build) and approved all three fly-card conditions. Verbatim:**

> "All 3 conditions are settled on this session. Instant snap on release of space — check.
> Keys dont override the cam and snap back to oblique view — check. Nose was always welded
> to the aim and vice versa in this kernel — it was the camera only that needed
> modification; the aim wedded to nose was a default behavior in freelook. Keys dont affect
> camera — check. We're all good on this one."

**CQ2 ruled — the 0.30 s easeback window STAYS. Verbatim:**

> "The .3 seconds mouse dead time is not noticeable as I need that .4s to observe / orient
> myself and am able to respond in time without noticing."

The window's mechanism rationale may be dead, but Chad has given it a *new*, current
rationale from the cockpit: it covers the orientation beat after the snap. It is no longer
a contradiction of "instantly re-established" — authority returns before he reaches for
it. **Do not flip it; do not treat it as debt.** If it ever surfaces again, this entry is
the ruling of record.

**Card-1 deflection trade: accepted as flown.** No sustain, no further mechanism. Closed.

**Chad's framing of what v9 actually was, worth keeping:** the aim-to-nose weld was the
kernel's default freelook behavior all along — *the camera was the only thing that needed
modification.* The fourth camera round succeeded when the fix finally matched that shape:
camera-only, one instant snap, aim untouched.

**Now unblocked (standing decisions permitting):** the seads-feel agent seals v9 and pushes
(branch was 6 ahead, unpushed); graft to seads-recon proceeds per its open item. On this
side, once the v9 seal exists: `reference/seads-feel/` may be re-snapshotted at it (it is
now a FLOWN seal), and Golden Felt Flight #3 can be scheduled on the sealed build.

---

## 2026-07-29 (evening) — v9 MEASURED AND LANDED: the pre-registered prediction was RIGHT, both terms were real

**v9 "S-nosesnap" is implemented and green** on `feel/kernel-v5` @ `a307a8a69` (gate
388/388, zero moved goldens, red-team SOUND-WITH-FIXES, no P0). **NOT yet flown by Chad;
not yet pushed to origin (tip is 6 ahead) as of this entry.** Commit order verified from
the live tree: rulings registered alone → instrument + v8 baseline → mechanism → mutation
hardening → docs → red-team folds. The discipline held.

**The measurement, graded against the pre-registration below** (v8 unchanged baseline →
after v9), from the new `comfort_freelook_release_keys` instrument:

| metric | v8 baseline | after v9 |
|---|---|---|
| `nose_at_fire_deg` (term A) | **18.552** — at its `aoa_max` cap | 0.578 |
| `vel_at_fire_deg` | 0.272 — proves the old velocity-referenced metric was blind | 17.956 |
| `nose_after_1s_deg` | 23.098 | **40.861** ⚠ see below |
| `nose_after_3s_deg` | 18.824 | 16.783 |
| `updebt_after_release_deg` (term B) | **44.904** | 0.003 |

**Verdict under the pre-registered rule: COMPARABLE — both terms real.** A capped and
persistent (~18.6°), B larger but transient (44.9°). The pre-registration's named
repeat-risk happened in the data exactly as written: A read ~18° and looked like solid
confirmation while B sat 2.4× larger. A threshold test on A alone would have bought a
fifth round. Both halves were implemented unconditionally per Chad's ruling anyway — the
rule ended up grading the diagnosis, which is what survives of it by design.

**Honest attribution, recorded as reported:** term B **predates v8** — S-keychase had been
masking a long-standing up-debt by re-anchoring forward. The up half of the felt oblique
was never a v8 regression.

**The one number that got worse — expected, and why:** `nose_after_1s_deg` rose 23.1° →
40.9°. This is the pre-named Card-1 trade, not a defect: post-snap, hard key-only turning
walks the nose away from the parked aim, and the lag camera follows the aim, re-opening a
~17°+ deflection view until the mouse takes over. The pre-registration's "honest reading
2" called this in advance: ordinary lag against a parked aim cannot stay behind a plane
turning on keys. **Chad's call after flying; the sustain stays dead unless he rules it
back.**

**Also retired, per the weld ruling:** no-keys mid-turn freelook drift 75.5° → 1.46°
(the carve is gone); out-of-a-loop orient rolled-world 178.7° → 0.000° (instant upright).
`comfort_mouseaim_keys` — the v8 win — bit-identical.

**Open on Chad (from the fly cards):**
1. **CQ2 easeback window** — for 0.30 s after any release, mouse deltas are dropped
   (`[freelook] easeback_time`, ruled 2026-07-03). This contradicts "mouse aim authority
   is instantly re-established," and its original rationale is reported dead twice over.
   If the first third-second of mouse feels dead after release, it's this window, not the
   snap. One-line flip, **waits on Chad's ruling**.
2. **Card-1 deflection trade** (above) — genuine physical tension, Chad's call.
3. Walk-back scope: `release_orient_with_keys = false` disables the with-keys snap ONLY;
   the true walk-back for v9 is the v8 seal tag `flight-kernel-v8-2026-07-29`.

---

---

## 2026-07-29 (later still) — RULING: in freelook the aim is WELDED to the nose, ALWAYS — the no-keys "parked carve" is retired

**Status: RULING. Chad's answer to the seads-feel agent's direct question ("nested always /
keep the carve / other"). He wrote option 3 himself. Verbatim, typo-fixes bracketed:**

> "In freelook, keys are the only means of aiming as the nose aim becomes welded to the nose
> directionality since mouse inputs now control camera during the freelook phase. Upon
> release the camera snaps back to chase and the mouse aim authority is instantly
> re[e]stablished. The camera is at that instant in chase view and is now subject to and
> dependent on mouse aim inputs, the nose following the cascade."

> "If I were to be mid turn and press freelook, then I am in freelook and my nose holds the
> last directionality it had before the spacebar press and hold (freelook). Then my mouse has
> no control over the plane and snaps around the nose. The nose maintains its heading and
> stops whatever input i[t] was giving it via mouse as the mouse can no longer influence the
> nose because we are in freelook. I have only the keyboard over[r]ide gross inputs to
> control the plane as my mouse is now controlling my camera for situational awareness. My
> nose is controlled by my careful qweasd inputs."

> "Releasing the space (freelook) allows the camera to snap back into alignment camera →
> tail → nose → nose indicator dot nested inside center of mouse aim... I now having released
> the space bar given back mouse aim authority and the nose and camera follow
> deterministically."

**What this rules:**

1. **Freelook ENTRY welds aim := nose — every freelook, keys or not.** The pre-freelook
   mouse-aim command stops driving the plane at the spacebar press. The nose *holds its
   heading*; it does not keep carving the old commanded turn. This retires today's no-keys
   "parked aim keeps the carve" behavior — a deliberate behavior change, ruled by Chad in
   answer to a direct either/or, not an incidental side effect of v9.
2. During freelook: mouse → camera only; qweasd keys are the sole control of the plane.
3. Release: mouse-aim authority is instantly re-established; the camera snaps to the chase
   alignment **camera → tail → nose → nose-indicator dot nested inside the mouse-aim
   center** (and upright, per the ruling below). The aim does not move — it was welded to
   the nose the whole time. Release must be a no-op on the aim **in all cases**, by
   construction, because entry did the welding.
4. This closes the code-vs-model gap flagged in the previous entry (§5b nesting observed
   only in the keys-held path): the nesting is now spec for ALL freelook, implemented at
   entry as a weld, not per-tick only when keys are held.

**Addendum (same day):** the guard stated the banked-release composition back to Chad —
camera on the tail line, rolled so the horizon is level — and Chad confirmed verbatim:
*"yes upright relative to the horizon and behind the plane is correct."* Together with
"Instantaneous. No need for anything else," the Step 2B question ("does snap-to-chase mean
upright too, and same-instant?") is **fully answered: upright, at the snap instant, behind
the plane.** The 2026-07-07 "eased" ruling is superseded for the freelook-release case. If
B dominates or is comparable, the seads-feel agent implements upright-in-the-snap — **it
does not re-ask.** Chad has now stated this three times.

**Second addendum (same day) — Chad removes the measurement GATE on the up fix entirely.
Verbatim:**

> "No, for 2B nothing to measure!! Use my words here not a previous ruling. Snap to view
> upon release of freelook, no eased anything as I need to immediately view the back of my
> plane, the aim, the nose, everything — making it lag there is going to disorient."

**What this rules:** the instant full snap — camera behind the plane, upright to the
horizon, aim and nose in view, in one un-eased step — is **spec unconditionally**. It does
not wait on the A-vs-B comparison; there is no "if B dominates" branch for it. This
partially supersedes the pre-registered decision rule below **by direct ruling**, which is
the one legitimate way to supersede a pre-registration: the rule existed to stop an agent
rationalising numbers into a preferred fix, not to stop Chad specifying the behavior.

**What survives of the pre-registration:** the instrument is still built first and the
baseline numbers still recorded on unchanged v8 — as *evidence* (before/after, and proof of
which term carried the felt oblique), not as a *gate*. The "both small → STOP and
re-attribute" arm survives for the forward term's diagnosis. The golden tripwire survives
untouched. v9 therefore implements BOTH halves: aim law (nose, unconditional weld) and
camera law (one instant snap — forward AND up together).

---

## 2026-07-29 (later) — CHAD CORRECTS THE v9 "LAW" QUOTE: the aim NEVER snaps, and the release snap IS upright

**Status: RULING. Amends the model statement inside the pre-registered v9 entry below. The
measurement rule itself (comparative A vs B, coupling, golden tripwire) is unchanged.**

The seads-feel v9 plan opened with a quote of Chad's ("Wherever the nose is pointing when I
release the freelook, I snap the camera and the aim…"). **Chad retracts that phrasing as wrong
on two counts, in his own words:**

> "It is w[r]ong precisely because I dont snap the camera and the aim... The aim is waiting
> for me nesting around the nose indicator circle. Freelook ensures that the aim and nose are
> nested together as one. Saying I snap the camera is incorrect. The camera is dependent on
> the aim so releasing freelook just snaps my camera to the aim deterministically, I am not
> controlling the camera to that directionality."

> "In freelook I am operating the camera with my mouse movement. My aim is in that freelook
> mode attached to the nose direction and are inseparable. The aim is waiting for me when my
> camera snaps based on the release mechanism from free look. Instantaneous. No need for
> anything else."

> "I should after releasing space (freelook) be looking directly at the rudder of my plane
> given my camera is now chasing the tail of my plane and looking at the currently aligned
> mouse aim and nose pointing indicator."

> "Only the release of freelook sets me directly looking at my plane from behind, **orients my
> view as upright relative to the earth** and whatever velocity or turn rate it is currently
> happening I have control with the mouse aim and the camera is completely dependent on mouse
> aim never keyboard over[r]ide."

(Bracketed letters are typo fixes only; wording untouched. Emphasis on "upright" is this
agent's, flagged as such.)

**Corrected model — what changed vs the retracted quote:**

1. **The aim never snaps. Nothing "changes the aim" at release — not even nominally.** During
   freelook the aim is nested to the nose, inseparably; the mouse is operating the *camera*.
   At release the aim is simply *waiting there*. The release moves ONLY the camera, and
   deterministically — Chad is not steering it there. Consequence for the code: setting
   `ci.target_dir_world := nose` at release must be a **no-op in every freelook release,
   keys or no keys** — the aim is already on the nose because freelook nests it there. Any
   measured aim jump at release is a defect by definition.
2. **The release snap includes UPRIGHT relative to the earth.** This is new, from Chad
   directly, and it **pre-answers the Step 2B question** ("does 'snap to chase' mean upright
   too?") — **yes**. "Instantaneous. No need for anything else." The 2026-07-07 ruling
   ("eased, not a snap") is **superseded for the freelook-release case specifically**;
   it was made for ordinary releases, and Chad has now ruled the freelook release upright.
   The pre-registered measurement still runs first and the dominant term still gets fixed —
   but if B dominates or is comparable, **no question to Chad is needed; the ruling is here.**
3. Everything else stands: one snap at release, then ordinary mouse-aim lag; keys never touch
   the camera; the mouse activates nothing.

**Docs correction:** the SPEC quote the v9 plan carries must be replaced with the corrected
wording above — the retracted sentence must not land in SPEC.md as law.

---

## PRE-REGISTERED (2026-07-29) — v9 decision rule, written BEFORE the measurement

**Recorded in advance deliberately.** The last three camera rounds each interpreted numbers
after the fact and each picked a cause that turned out to be partial. This entry fixes the
decision rule, the predictions, and the falsification condition **before** the v9 scenario is
run, so the result cannot be rationalised into agreeing with a preferred fix.

**Chad's model — the whole of it. Nothing may be added.**
1. Release freelook → camera snaps to chase, **directly behind the plane**. Every time. Keys
   held or not; keys are irrelevant to it.
2. After that snap: mouse-aim with the ordinary lag camera.
3. **Keys never touch the camera. Ever.** v8 got this right and it stays.
4. The mouse activates nothing — it moves the aim, the camera lags it.

No latch, no sustain, no key-triggered mode. The "sustain mechanism" framing was this agent's
and was wrong; Chad rejected it twice. **Dropped, and not to be reintroduced without a new
ruling.**

**Symptom being diagnosed** (Chad, flying v8, localised to the *instant*, not the drift):
*"If I release freelook with keys override still getting input it does not give chase but it is
reverted to the old behavior of an angled view from across the loop manoeuvre at an oblique top
down view."*

### The two candidate causes and their predicted magnitudes

| # | candidate | mechanism | predicted magnitude |
|---|---|---|---|
| **A** | **forward term** | `orient_snap_dir` returns the **velocity**, so the cut lands behind the flight path, not behind the aircraft | **≤ ~20°** — bounded by `[aoa] aoa_max = 20.0` plus modest sideslip |
| **B** | **up term** | `orient_fired` cuts `cam_fwd` but **never touches `cam_up`**; `cam_up = loop.aim.up()` carries loop holonomy, and the eye is *lifted along up* | **up to 180°**, righting only over ~1–1.6 s (`horizon_recovery rate = 150 deg/s` + eased tail) |

### The decision rule — comparative, not a threshold

**The dominant term is the term that gets fixed.** Compare `nose_at_fire_deg` against
`updebt_after_release_deg`.

- **A dominates** → change `orient_snap_dir` to return the nose.
- **B dominates** → bring the camera-up upright **as part of the one snap**, instead of leaving
  it to S7-hrz's open-loop roll. ⚠ This contradicts Chad's 2026-07-07 ruling (*"not too much of
  a snap, just a quick uniform movement that is eased at the end"*) — made for ordinary
  releases, not a 180° debt out of a loop mid-fight. **It therefore becomes one question for
  Chad: does "snap to chase" mean upright too?** Ask it with the numbers attached.
- **Comparable** → both need fixing; the plan needs a second half.
- **Both small** → **both hypotheses are wrong. STOP and re-attribute.** Do not proceed to a fix.

⚠ **A threshold test on A alone is not acceptable**, and this is the specific repeat-risk: a
reading of ~18° looks like a solid confirmation of A while B sits at 120°. Fixing A then
delivers 20° of a 120° problem and buys a fourth round.

### Known coupling — the two causes are NOT independent

The S7-hrz debt is captured about the **post-snap forward on the same tick** (load-bearing
ordering, established v6). Changing the snap target from velocity to nose therefore changes the
captured angle and the resulting roll. **`updebt` must be re-measured after any change to A**,
never assumed to have held still.

### Standing guardrail for this change specifically

`orient_snap_dir` sets the **aim** — `ci.target_dir_world`, a **control input**, not a camera
value. This is the first change in the whole camera arc that can legitimately move a controller
golden. **If a golden moves: STOP and confirm intent. Never re-record — a re-record blesses the
bug.**

---

## 2026-07-29 — v8 FLOWN, PARTIAL REJECT: the release snap now EVAPORATES (v9 needed)

**Chad, flying v8:** *"Release of freelook is now giving me oblique view rather than chase…
the most important part of the change is now reverted. The freelook then release should snap
back to chase view even if I am holding the key… we threw out the baby with the bathwater and
reverted to whack-a-mole."* Mouse-aim + keys (the v8 target) is **confirmed fixed**; the
freelook-release case is **regressed**.

**Nothing is mechanically reverted — verified in-tree.** `release_orient = true`,
`release_orient_with_keys = true`, the `ovr_ok` predicate fires, and `app/main.cpp` still
hard-cuts `cam_fwd = loop.aim.forward()` on `orient_fired`. The D9 retirement is fully intact.

**The defect is that the snap no longer persists.** At the release tick: aim := guarded
velocity, camera cut to it — correct, chase-behind. But the aim is then **parked**, and the
pilot keeps turning on the keys. `ease_chase_forward` pulls `cam_fwd` toward a *static* target
at `lag_base + lag_gain·defl` ≈ 0.2 /s, so the camera effectively holds a fixed world direction
while the aircraft rotates out from under it. In a hard turn the view is oblique within a
fraction of a second. **S-keychase had been supplying the *sustain*** by re-anchoring to the
live flight path; excising it removed the sustain, not the snap. **The snap fires and is
erased.**

### Why this was missed — three failures, recorded because each is reusable

1. **The v8 plan's attribution was false, and this agent endorsed it across three audit
   passes.** The plan stated: *"Chad's original oblique complaint was caused by the D9
   exception. Retiring D9 fixed that."* **This file says the opposite**, in the heading of the
   2026-07-28 entry below: *"retire the D9 exception — ⚠ did NOT fix the reported symptom."*
   The measurement was explicit (0.375° at the fire, then re-opening) and is the entire reason
   S-keychase was built. The audits went deep on *implementation* — a vacuous test pin, gate-count
   accounting, the `drive()` instrument gap, the bounded `max_step` — and **never checked the
   premise against the ledger.** The section labelled "Attribution, stated honestly" was the one
   part taken on trust. **Lesson: audit the attribution before the implementation. A plan's
   stated cause is a claim, not context, and the ledger is the place to check it.**
2. **Fly card 2 tested firing, not persistence.** Its criterion was "the one snap *still
   fires*." Firing is a tick-level fact; the felt question was whether the resulting view
   *holds* for the second after. A kernel with exactly this defect passes that card — and did.
   **Lesson: a fly card for a discrete event must state how long the result must survive.**
3. **The sequence Chad actually flies has still never been instrumented.**
   `comfort_turnsteady_keys` starts on keys with no freelook; `comfort_mouseaim_keys` is mouse +
   keys with no freelook. **Neither models freelook → release → continue on keys**, which is the
   complaint in v7 *and* in v8. This is the fourth camera mechanism measured against something
   adjacent to how he flies. **The v9 scenario must be that exact three-phase sequence.**

**Also: the 16.34° "accepted drift" figure understated the real case.** In
`turnsteady_keys` the instructor keeps pursuing the parked aim, which drags the aircraft back
and bounds the divergence. Under real hard key input after a release the aircraft leaves much
further, which is why this reads as "very distracting" rather than as a mild 16°. A
scenario-limited number was quoted as if it were the general one.

### The resolution — the two rulings collide, and only one shape satisfies both

Chad's rules **"keys affect neither"** and **"release should put me in chase even while holding
keys"** conflict in this one case. A one-shot snap can only persist if either the **aim** tracks
the aircraft (ruled out — keys must never carry the aim) or the **camera** sustains. Therefore
the camera must sustain, **armed by the freelook release, not by the keys.** That is not key
authority: it is the freelook release having a *duration* rather than an *instant* — which is
exactly *"only the precedence of the freelook push shall do that."*

This is the original S-keyprec brief (*gate the anchor on freelook precedence*). When the
arm-rule question came back and Chad rejected the three options as framed, the plan swung to
**full excision**, and the excision took the sustain with it. **v8 was right about mouse-aim and
wrong to discard the release sustain.** v9 restores the anchor gated on freelook precedence:
arm on the freelook release edge, disarm when the pilot takes the mouse again (his "then I am in
mouse aim mode"). Open sub-question for Chad, unresolved: whether releasing the *keys* should
also disarm, and whether the disarm-on-mouse transition needs blending.

**Status:** v8 stands sealed (`ae7ae8f23`) and is **partially rejected on the stick**. Do not
graft v8 to seads-recon. Fly card 3 (the deflection-shot question) is still outstanding and is
independent of this.
---
