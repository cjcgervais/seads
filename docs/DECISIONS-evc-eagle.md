# Decisions — EvC2026 Eagle (vessel ledger)

The eagle kernel's ruling ledger inside the mandalark-kernel preservation system,
created 2026-07-30 on Chad's R7 ruling ("incorporate this project into the
mandalark-kernel documentation and preservation system"). EvC2026 is this repo's
**Generation 1** (see `docs/VERSIONS.md`); the eagle is registered as a governed
**vessel** under the vessel-ontology doctrine — its own character, its own constants,
its own rulings, sharing the kernel base's laws and lessons.

**Sync rule with the EvC CS registry** (`D:\EvC2026\docs\HANDOFF.md`): the CS registry
is the LIVE PROTECTION mechanism in-repo; this ledger is the HISTORY and REASONING.
Every CS-registry amendment gets a same-day entry here citing the row; every entry here
that changes a CS row names the row. Neither is edited silently.

**Standing doctrine that governs this vessel** (from the kernel base, applies verbatim):
the mouse-helm comfort doctrine (DECISIONS.md STANDING INTENT — including its eagle
clause: dip-and-rise "might be an actual feature in another aviary game such as an
eagle flying game"); the compensation-decay law (four flown instances); character is a
deliberate per-vessel choice, recorded as a ruling, never a kernel default.

---

## 2026-07-30 — REGISTRATION + Chad's R1–R7 ruling batch (the founding entries)

**Registered** on the v12-receive consult (packet
`D:\EvC2026\docs\CONSULT-KERNEL-V12-RECEIVE-PACKET.md` @ `166bd58`; reply on the
mandalark ledger). Live tree at registration: branch `updraft`, HEAD `45ea3c2`.
Constant table: packet §1 (to be re-verified against the live tree with
`tuning/evc2026-v5-rungE.md` at first adaptation flight).

**Chad's rulings, delivered 2026-07-30 with the consult request** (recorded by the
eagle engineer; verbatim fragments as given):

- **R1** — the dip/curl is now an ISSUE, not a protected feature: "a bit of a dip /
  curl in getting the eagle to follow the flight of the mouse aims inputs… issues I
  have just got rid of in seads." Nuance preserved: the involuntary parasitic dip is
  removed; the eagle's expressive curved pursuit is KEPT — the `line_hold_ff` port is
  a continuous character knob (0 = v11 curl, 1 = plane-straight), and the eagle's
  ruled value between them becomes a character-sheet entry.
- **R2** — the free-look release camera is an ISSUE: "there are some issues with the
  camera after releasing free look." (CS-2 EXIT-SNAP clause to be amended via Chad,
  never silently.)
- **R3** — chase-cam framing is an ISSUE: "I'm looking at the eagle, not in front of
  the eagle, in chase cam."
- **R4** — pole clamping in the mouse-aim cascade is an ISSUE: "a pole clamping that
  happens in the mouse aim cascade."
- **R5** — feel targets: make the eagle "feel lighter, a little faster in the bottom
  end, like acceleration from zero." (The mass=16 "punch comes from THRUST, not
  lightness" ruling STANDS; levers are thrust/gravity/speed-shaped thrust.)
- **R6** — the eagle's characteristic flight patterns are to be IMPROVED, not
  replaced: "an extremely careful but apt implementation towards the improvement of
  our characteristic eagles flight patterns."
- **R7** — governance: incorporate into the mandalark-kernel preservation system
  (this file is that ruling executed).

**Adopted at registration** (from the consult reply; full detail there): the seal
ladder (retro-seal **eagle-v1** as an ANNOTATED tag with Chad's verdict in the
message, recorded in VERSIONS.md); the four-file golden convention with F7/F6 harness
tapes accepted as interim goldens (BuildStamp hash + stated constants + stated
predicates required); the fly-card template (verbatim spec, table-flown-is-a-fact,
duration-stated conditions, pre-registered counts, sentinels, kill-switch,
revert-to-branch rejection); pre-registered decision rules before numbers exist.

**Open at registration:** the C/D attribution split (camera-basis arc vs crab
parasitic vertical) — to be MEASURED by the STAGE-0 phase-resolved instrument before
any ranking (the kernel base's freshest scar: an approved, audited attribution was
falsified by its own instrument at 2–4% measured closure); the eagle's `line_hold_ff`
character value (Chad's stick, suggested sweep 0.3/0.5/0.7 one per flight); eagle-v1
retro-seal and Golden Felt Flight #1 (eagle) on Chad's next accept flight.

---

## 2026-07-30 (late night) — STAGE-0 instruments LANDED (`d7a42b8`), inert; baseline capture awaits Chad's stick

Four instruments, one add-zero-inert hook in `computeMouseAim`, behavior byte-identical
unarmed: the parasitic-dip column on the F7/F6 step tracker (basis-free by construction
— world-fixed aim isolates the crab term); the F4 synthetic mouse sweep (basis-live,
with a direct `aim_drift_deg` basis-arc read); the duration-stated free-look release
scorer (true horizon roll); the keys-held oblique/slam scorer. Sonnet built, Opus
verified SHIP-WITH-FIXES all applied — headline catch: a sign-blind overshoot metric
that would have FABRICATED 20–40° readings on every sweep, killed before any baseline
existed. Tier-4 659/659, rojo PASS, no CS contact. Commit-window discipline held
against the concurrent flight-13 session (waited out `9e33187`, committed alone).

**The measured C/D split = the dip difference between F7 rows (crab alone) and F4 rows
(crab + camera basis)** — same number family as Golden #5's 0.58° predicate. Capture
protocol (twice, one stamped build): serve → confirm `[BuildStamp]` → F8 on → F7 ×2 →
F4 ×2 → two free-look releases → one keys-held turn + release → F9 flush; ONE injector
at a time. The two numbers return to the kernel base for the ordering consult; nothing
is ranked until they exist.

---

## 2026-07-31 (small hours) — THE EAGLE'S FIRST MEASURED ATTRIBUTION: the dip is THE CAMERA, not the crab. C leads; the crab fix does NOT port

**The split came back TOTAL** (baseline flown by Chad on stamped build
`ead1cc0-dirty 23:35`, twice-reproduced; eagle repo `963195b`, packet §7.2):

- **Basis-free steps (crab term alone): dip 0.00° / 0.01°.** The crab's turn-sag is
  ALREADY fully cancelled against a stationary target by `aimBankFeedforward` (the
  S27-approved term) — the eagle has carried its own S-straightline equivalent all
  along.
- **Basis-live sweeps (crab + camera): dip 13.6° / 8.4°** (session range 8.6–16.4°).
- **Independent confirmation:** the `aim_drift_deg` sign-flip, five-for-five by sweep
  direction — the bank-rolled swing basis read directly, a signature the crab term
  cannot produce.

**Ordering, automatic under the pre-registered rule:** the **aim-carries-its-own-frame
port (stage C) LEADS** — one stage that is simultaneously the pole fix (R4) and, by
measurement, most-to-all of the dip fix (R1). **`line_hold_ff` DEMOTES to a residual
character dial**, swept only if any dip survives the basis port. The consult reply's
"C and D land together or not at all" resolves to: C lands; D is contingent on C's
residual.

**The mirror, for both ledgers:** the plane assumed gravity and measured the crab; the
eagle assumed the crab and measured the camera. Two kernels, two wrong priors, two
instrument-corrected attributions — measure-before-rank is now two-for-two across the
federation.

**Baselines on the record as diff targets:** releases — 30–39° horizon-tilt debt
persisting ~1 s (one run −42.8°), standing oblique 22°→72° behind by 3 s (ask A / ask
B targets respectively, seven runs each); keys — 42–86° oblique, 85–147°/s release
slam (3–5× the plane's accepted 16.3°). **Must-not-regress rows:** zero-overshoot /
zero-reversal arrival, 0.35 s vertical settle, 0° basis-free dip. **Honesty flags:**
the overshoot=30.00 rolling-frame artifact is never to be cited; F7 was unreachable on
Chad's keyboard — the lateral step is now F3 (instrument keymap is part of the card).

**Next:** stage C, built inert behind a flag, one knob, these baselines as the diff.
The HttpService recorder (BuildStamp in the tape header) is the specified tape format,
queued behind live feel work.

---

## 2026-07-31 — STAGE C FLOWN AND KEPT (Chad's word: "keep") — `aimOwnFrame = true`. Attribution #1 partially SUPERSEDED: the camera owned the DRIFT and the POLE; the pointing law owns the arc

**Chad ruled KEEP** on the aim-carries-its-own-frame port, flown on his stick. What the
flight measured:

- **Drift: KILLED.** After a Space-tap reseed, ±1.89° (was ±16° carried-tilt state;
  old basis −12/+3). A horizontal hand sweep now draws a level world line — the old
  basis could not do this at any tilt. The carried tilt was flying history, not a
  defect; it was invisible to the hand (screen-right stayed hand-right — the seads
  self-consistency property), and every Space tap re-levels it free — a player action
  righting the frame, the same shape as the plane's release verb.
- **Pole (R4): FIXED, on the stick.**
- **Zero regressions:** release clean, no wobble, arrival still 0-overshoot — every
  must-not-regress row held.
- **The dip column DID NOT MOVE (9.99/12.03 vs 8.4–13.6 baseline).**

**SUPERSESSION (stated plainly, no smooth phrasing):** attribution #1's "the camera
owns most-to-all of the dip" was WRONG. The camera owned the drift and the pole — both
now fixed. The curl/arc (nose banks, rises over or dips under, then pulls straight)
lives in the POINTING LAW: pitch and roll commanded simultaneously from the same
error, the eagle over-rotating through the vertical as the bank comes on. Stage C was
the discriminating experiment that sharpened this. **The federation's instruments are
now three-for-three at overturning confident attributions** (plane: gravity→crab;
eagle: crab→camera; eagle: camera→pointing-law). Nobody is embarrassed; this is what
the discipline is for — each landed fix is also the experiment that sharpens the next
attribution.

**Consequences:** CS-8 amended via Chad's word with same-day entry (eagle repo);
`line_hold_ff` (stage D) PROMOTED back to next — adopting the CONCEPT (continuous,
envelope-bounded character dial, structurally off at 0, Chad's 0.3/0.5/0.7 sweep, his
choice of where the eagle sits between bird and plane) with the eagle's OWN algebra
for its own arc mechanism, never the plane's crab-cancellation transplanted. **The
two-eyed instrument patch precedes any stage-D knob** (`arc_over_deg`/`arc_under_deg`,
target-referenced — a dip_deg blind to the rise half is a one-eyed instrument;
`dip_deg` retained for continuity).

---

## 2026-07-31 (evening) — STAGE D LANDED INERT (`1b68fe8`), verified; SWEEP DECISION RULE PRE-REGISTERED before any number exists

**Verified read-only by this agent against the EvC2026 tree** (branch `updraft`, tip
`1b68fe8`; serve stamp `1b68fe8-dirty 2026-07-31 17:51`, dirty = BuildStamp only):

- **Instrument-first order HELD:** `05ffb45` (two-eyed `arc_over_deg`/`arc_under_deg`,
  target-referenced running max each side, on both step and sweep rows) landed BEFORE
  the dial commit. The columns read the nose vs the aim's CURRENT elevation, so the
  rise half of the arc is visible — the one-eyed-instrument scar is answered.
- **The dial is the eagle's own algebra, as ruled:** elevator target blends body-frame
  `upc` → world-elevation `upcLine = clamp(sin(eElev)/max(cosB, 0.5), -1, 1)`, weight
  `kEff = lineHoldFF · cosElev · uprightFade · bankTaperFade` — three continuous fades
  returning today's law at zenith (loops commit — red-team BLOCKER-1), below the
  horizon (CS-5 no-auto-level stays unconditional — BLOCKER-2), and past the
  `aimBankFFTaperDeg=55°` shoulder (S30 knife-edge pump stays dead — MAJOR-1, taper
  MIRRORED not hoisted; if the 15° shoulder ever moves, move BOTH copies).
- **k=0 is today's path:** `vTarget` aliases `upc` into the unchanged `shapeAxis`
  call; both terms are exactly zero at a level throw's initiation, and `upcLine ≡ upc`
  at wings level by construction (the 0.35 s vertical channel untouched).
- **Declared watch (MAJOR-3, on the record pre-flight):** at k>0 in settled banked
  turns below saturation, altitude-hold shifts onto `aimBankFeedforward` alone —
  expect possible mild sag creep in medium sustained turns. Recorded if seen, never
  auto-fixed; any bankFF retune is its OWN knob, its own flight.

**PRE-REGISTERED SWEEP DECISION RULE** (written now, before the k=0 baseline or any
sweep row exists, so no result can be rationalised):

1. **Baseline first:** k=0, F3 ×2 + F4 ×2 — the first-ever `arc_over`/`arc_under`
   rows. These are the diff target for every sweep value. No sweep flight before the
   baseline rows exist.
2. **Expected instrument signature:** arc magnitudes on banked pulls fall
   monotonically as k rises through 0.3/0.5/0.7. **If the arc columns do NOT move
   with k, that is a candidate FOURTH attribution overturn — STOP the sweep and
   re-attribute; do not tune through a null.** Known false-null to exclude first: the
   dial is silent past 55° of bank BY DESIGN (kEff full only below ~40°) — judge on
   normal-to-hard banked pulls, not rim-pinned or knife-edge rows.
3. **Must-not-regress rows, per sweep value:** rim-pinned full circle still
   full-rate; powered loop with cursor high still commits; release clean; arrival
   zero-overshoot/zero-reversal; basis-free dip stays 0°. Any regression on these
   rows fails that k value regardless of arc numbers.
4. **The ruling criterion is Chad's felt character choice, NOT maximal arc
   reduction.** k=1 is a plane; the sweep is looking for his number — how much bird
   stays in the bird. The instrument says whether the dial works; only the stick says
   where it sits. One value per flight (the S28 rule), his verdict verbatim.

---

## 2026-07-31 (night) — SWEEP FLIGHTS 1–2 FLOWN (k=0.3 @ 18:03, k=0.5 @ 18:20): the FELT curl barely moved. Pre-registered rule 2 is LIVE — sweep HELD at the gate; Chad's bank-timing hypothesis REGISTERED as candidate attribution #4

**Chad's verdicts, verbatim (his words are the specification):**

- **k=0.3** (stamp `1b68fe8-dirty 2026-07-31 18:03`): "Throws to mid screen the nose
  indicator is going like an inch over the mouse aim, quite a large curl still at
  0.3 …. full pinned turn works, full loop still works… 0.3 did little to fix the
  curl lets try 0.5 next."
- **k=0.5** (stamp `1b68fe8-dirty 2026-07-31 18:20`): "full turn is good, loop is
  good, Quick halfway deflection still have the curl. It didnt change too much…
  The curl behavior is still there it shooting a little straight but its going
  straighter to a point still the one inch above the mouse aim then settleing down
  into it."

**Must-not-regress rows HELD on both flights** (rim-pinned circle full-rate, powered
loop commits). The felt signal: a small straightening at 0.5 ("shooting a little
straight… going straighter to a point") but the one-inch-over arc persists.

**CHAD'S HYPOTHESIS — registered verbatim, the candidate for attribution #4:**

> "I am hypothesizing that bank rate may be an issue as the eagle works to reconcile
> the nose to the mouse aim the banking is not full yet before the full pitch is
> going to try to reconcile the nose to the mouse aim by pitching and the eagle
> hasnt full banked to that angle and as it continue during the pitch, its forming
> the loop. So it may not be this straightline but a lack of banking propensity,
> initial proptness or turing up banking rate might cure this (hypothesis only and
> to be taken up with my doc manager… SO I think its the bank angle not being there
> enough on time with the mouse input relative to pitch and yaw. The eagle curl
> behavior is beautiful but I want the players to command the eagle… I think this
> is a matter of control surface balancing.. But let us continue programmatically
> as per instruments and SOPS…."

In mechanism terms (this agent's translation, clearly marked as such): the curl is a
TIMING/ordering claim — full pitch authority arrives while the bank is still
developing, so the nose pitches through a partially-established lift vector; the
defect would live in the bank/pitch PHASE relationship during the throw transient,
not in the elevator's steady-state target (which is all stage D reshapes).

**Why this hypothesis is credible against the stage-D design itself:** two mechanisms
already on the record predict exactly a weak dial during fast hard throws — (a) the
engineer's saturation rail (during a hard throw both `upc` and `upcLine` can
saturate `shapeAxis`, making the blend weightless right when the curl forms); (b)
the bank-taper fade (kEff fading above 40° of bank — a hard throw's bank transient
passes through/above that band at the very moment of the arc). Both put the felt
curl OUTSIDE the dial's active domain. Chad's timing hypothesis is compatible with,
and sharper than, both.

**RULING UNDER THE PRE-REGISTERED RULE — the sweep is HELD at the gate. NO k=0.7
flight until:**

1. The engineer reports the `arc_over`/`arc_under` diffs for the 0.3 and 0.5 rows
   (F3 ×2 + F4 ×2 each) against the k=0 baseline (~8–10 over / 0–7 under), with the
   two false-null exclusions checked (>55° silence; saturation rail).
2. The branch is then decided by the numbers:
   - **Columns did not move** → rule 2 fires clean: attribution overturn #4, sweep
     closed, re-attribute.
   - **Columns moved but the hand didn't** → lesson-2 territory (the instrument is
     not modeling the maneuver Chad flies): F3 steps / F4 synthetic sweeps are not a
     fast throw to mid-screen. Either way the naive sweep is over — the felt curl
     lives outside what the current instrument+dial pair address.
3. In BOTH branches the next step is the same, per the measure-before-rank law: a
   **phase-resolved throw instrument BEFORE any bank-rate knob is touched** —
   measure, during Chad's actual fast-throw reproduction, bank angle attained vs
   pitch application over time (e.g. bank fraction of steady value at the moment of
   peak pitch command / peak arc_over). If the bank is well short of its steady
   value at peak pitch, Chad's hypothesis is confirmed by its own instrument;
   only then does a banking-propensity/promptness lever get designed. His
   hypothesis is a claim with the same standing as any plan's attribution — it gets
   the instrument treatment, not a tuning pass. (Three overturned attributions say
   this protects HIS hypothesis too: if it's right, the instrument makes it
   unassailable.)

**Standing intent restated for the eventual fix:** "The eagle curl behavior is
beautiful but I want the players to command the eagle" — the goal is not curl
removal; it is putting the curl under command. A bank-timing fix and a character
dial may yet compose.

---

## 2026-07-31 (later night) — GATE RESOLVED: the COLUMNS MOVED, THE HAND DIDN'T. CS-5 amendment-pending on CHAD'S OWN WORD (dwell auto-level). Three levers on the table; ONE flight adjudicates — predictions pre-registered, plus this agent's third-outcome rider

**The sweep gate resolved to the SHARPER branch** (engineer's diff, packet §7.4;
verified read-only, commits `ecd0d36` + `b39d7e6` on `updraft`): the F3 arc columns
DID move — arc_over 7.82 → 7.23 → 5.4/6.05, monotone with k — while Chad's hand felt
"didn't change too much." The dial works as designed, sub-saturation; the FELT curl
lives in the fast throw, exactly where both pre-identified false-nulls live (the
saturation rail + the >40° bank-transient taper) and exactly where the scripted
F3/F4 scenarios don't reach. Lesson 2 fires again: the instrument must model the
maneuver Chad actually flies. `lineHoldFF` is HELD at the gate (proven lever,
unranked) and reset to 0 so the throw capture reads the pure curl.

**Honesty ledger:** flight-2's F4 sweep rows are CONTAMINATED (aim_drift +17.8/−41.6
— armed on a heavily tilted carried frame) and are never to be cited. **Protocol
amendment on the record: Space-tap re-level before every F4 set.**

**CS-5 AMENDMENT-PENDING — OWNER-INITIATED. Chad's word, verbatim:**

> "per my request when this was build I left autolevel wings to horizon out. So the
> eagle will fly inverted. Now, I dont really need this anymore. Sometimes a
> midscreen deflection throw of the mouse aim will casue the bird to hear to the
> mouse with the inverted bank and is pitching down (sideways along horizon due to
> the bank) to meet the mouse aim. Also the eagle is not always in a roll angle to
> meete the mouse input ideally. Ideally an auto level wings after say 0.5s without
> mouse aim input (holdingstill to settle) will set the eagle on the right pre
> orientation for specific deflection calls."

This is the vessel doctrine working as designed: inverted flight was Chad's OWN
session-9 character ask, and it is revised only by his own word — never silently,
never by an agent's inference. The engineer's tombstone check is endorsed: a
dwell-gated settle-to-level is an attitude VERB (player-shaped, discrete trigger),
not the banned continuous-basis class stage C already severed, and not MB-right
(whose exclusion derived from the very lock now being revised). Design questions
PARKED until its turn: dwell time (~0.5 s starting point, Chad's number), ease rate,
and inverted recovery — which per Chad's report is a MUST, since the inverted-bank
pitch-down IS the complaint.

**Three levers now on the table, NONE ranked** (measure-before-rank, four
attributions of scar tissue say so): dwell auto-level (pre-orientation), bank
promptness (timing), residual `lineHoldFF` (arc shape — proven sub-saturation).

**THE ADJUDICATION FLIGHT — predictions pre-registered before any row exists**
(stamp of record `b39d7e6-dirty 2026-07-31 19:01`; tracker auto-arms on real throws:
near-centre → ≥15° in ≤0.30 s, keys idle, never injected; per-throw columns:
`bank0_deg`/`up0_y` starting attitude, `bank_frac` at first pitch-rail
(`t_peak_pitch` = the hypothesis's exact instant), the two-eyed arc):

- **P1 (pre-orientation):** ugly-arc rows cluster on cross-banked/inverted `bank0`
  → the dwell-level lever leads.
- **P2 (timing):** `bank_frac` is small at the pitch-rail even from clean level
  starts → bank promptness leads.
- **P1∧P2:** both true — the levers compose (they are not rivals).
- **P0 — THIS AGENT'S RIDER, added at the gate:** if NEITHER signature appears —
  arcs forming from clean level starts WITH healthy `bank_frac` — then Chad's
  hypothesis is itself overturned (attribution overturn #4 fires in full) and we
  re-attribute again rather than rank a lever. Written now so a third outcome
  cannot be rationalised into P1 or P2 after the rows exist.

**Flight protocol (variety IS the data):** a dozen-plus fast mid-screen throws,
both directions, from varied situations — level starts, banked starts, right after
maneuvers. F8 on; every qualifying throw prints its own row; no other F-keys.

**Standing intent governs whichever lever wins:** "The eagle curl behavior is
beautiful but I want the players to command the eagle."

---

## 2026-07-31 (adjudication) — THE 29-THROW CAPTURE: three signatures, all pre-registered predictions bounded or confirmed, plus ONE UNPREDICTED FINDING (the structural floor). RULING: design order E1 dwell-level → re-capture → E2 phased pull; promptness RE-CLASSIFIES to R5; lineHoldFF trims last

**Provenance caveat, stated first:** this ruling proceeds on the engineer's reported
numbers (29 throws on stamp `b39d7e6-dirty 19:01`). The row table was flushed to the
engineer's session log only — NO metrics file exists on disk in the EvC2026 tree
(verified read-only; nothing written since 19:00 but BuildStamp/rbxlx). The numbers
are internally coherent and match the pre-registered signatures, but this is an ECHO,
not an artifact. **Condition of this ruling: the 29-row table is archived verbatim
into the eagle repo (a committed file) before any lever's fly card cites it.** The
HttpService tape recorder already queued is the systemic fix.

**Population predicate on the record** (recover the predicate, don't restate the
number): real single throws = settle < ~1.7 s; five 2–4 s rows are compound
maneuvers, SET ASIDE (not deleted — set aside, named).

**The three signatures, against the pre-registration:**

- **P1 CONFIRMED — bad starts own the chaos.** Cross-banked/inverted starts
  (|bank0| > 30° or up0 < 0.9): arc_under median ~9° vs ~0 for level starts, sign
  chaos, worst row bank0 67° → 32° over / 50° under. The dwell-level's signature.
- **P2 CONFIRMED AT THE EXTREME, BOUNDED.** Worst clean-level-start arc: pitch
  railed at t = 0.13 s with 25% of eventual bank → 28° arc. Low bank_frac produces
  the biggest clean-start curls. But above frac ≈ 0.4, NO correlation between bank
  timing and arc size.
- **UNPREDICTED — THE STRUCTURAL FLOOR (the sharpest finding).** Level starts with
  healthy bank at the rail (frac 0.88–1.0) still arc 8–15°. Cause visible in the
  data: peak_pitch ≈ 1.0 on nearly every throw — the elevator is RAILED BY DESIGN
  on any fast throw (gains saturate at small error), so full pull rides a
  still-developing bank regardless of promptness. This is the consult ask-D third
  shape, verbatim concept: "the pull grows WITH the bank, both arriving together" —
  the phased pull, promoted from deferred speculation to measured need. It also
  closes the lineHoldFF null: a railed command doesn't care about its target's fine
  shape.
- **P0 outcome: HALF-FIRED, honestly.** Chad's hypothesis was not overturned — it
  was BOUNDED: true at the extreme, insufficient for the floor. Attribution #4 is
  not an overturn but a PARTITION: three co-owners, each with a measured share.
  (The pre-registration did its job: without P0 in writing, the floor could have
  been rationalised into P2.)

**THE RULING (the base's seat, reasoning stated):**

1. **E1 — dwell settle-to-level FIRST** (the CS-5 amendment executed). Owner-
   initiated, smallest structural risk (a dwell-gated attitude verb), deletes the
   measured chaos population — and, decisive for the ordering: it CLEANS THE
   MEASUREMENT BED. Bad starts contaminate every arc statistic; every later lever
   gets measured on a clean population. Design constraints from the record: dwell
   ~0.5 s (Chad's number, tunable on his stick); eased, never snapped; MUST recover
   from inverted (the inverted-bank pitch-down IS the complaint); never fires
   during active mouse input or held keys; inert behind a flag, red-teamed, one
   flight — the standing cadence.
2. **E1 RE-CAPTURE, pre-registered now:** same throw instrument, same protocol.
   Expected: the bad-start population disappears from real throws; the floor
   (8–15°) and the low-frac extremes PERSIST — they are clean-start phenomena. If
   the floor MOVES with E1, that is a surprise finding; record it, re-attribute.
3. **E2 — the PHASED PULL second** (the structural fix). Ranked above promptness
   because by construction it addresses BOTH remaining signatures: it removes the
   floor (the pull is no longer railed through a developing bank) AND it
   neutralises the low-bank_frac extremes (if the pull grows with the bank, a slow
   bank means the pull WAITS — the arc becomes timing-independent). Promptness as a
   curl fix is structurally superseded by E2.
4. **Promptness RE-CLASSIFIES to the R5 feel thread** ("lighter, a little faster in
   the bottom end") — it governs how fast the eagle answers the hand, not whether
   the answer curls. It is not dead; it changed departments. Revisit only if E2's
   re-capture contradicts this.
5. **lineHoldFF LAST — the sub-saturation character trim.** Chad's sweep resumes on
   the E2-landed kernel (the dial finally has an unsaturated command to shape), his
   number, one value per flight.

**Standing intent governs all of it:** "The eagle curl behavior is beautiful but I
want the players to command the eagle." E1 gives the eagle the right posture for
the call; E2 makes the answer arrive as one motion instead of a pull through a
half-set wing; the dial then decides how much bird is in the answer.

---

## 2026-07-31 (archive verification) — CONDITION SATISFIED (`456dbad`, 34 rows); ARTIFACT RE-VERIFICATION CORRECTS THE EVIDENCE MAP: P1's flagship rows are COMPOUND, the FLOOR gains its own discriminator. RULED ORDER STANDS on corrected grounds. E1 design catch ENDORSED + falsifier registered

**The archive condition is satisfied:** `docs/tapes/2026-07-31-throws-b39d7e6.csv`
committed (`456dbad`), provenance in-file, 34 rows vs 29 read live — honestly
flagged. `docs/tapes/` accepted as the recorder's permanent home.

**This agent re-derived the three signatures from the artifact itself** (the whole
point of the condition). Stated plainly, no smooth phrasing — the echo and the
artifact disagree in places:

- **P1 (bad-start chaos) DOWNGRADED from "confirmed" to "supported, chiefly by
  compound rows."** The flagship "monster" (bank0 67° → 32°/50°) is a COMPOUND row
  (settle 3.54 s) — outside the adjudication's own single-throw predicate. Among
  true singles, bad starts show only a MILD penalty: arc_under median 3.2° (not
  ~9°) vs level-start mean 2.7°. The dramatic bad-attitude arcs live in the 13
  compound rows (not five — recount from the artifact, settle ≥ 2 s), where
  interpretation is genuinely ambiguous (sequential throws from maneuver exits —
  which is still a pre-orientation story, but an unmeasured one).
- **The FLOOR is UPGRADED — the artifact contains a discriminator the echo
  missed.** High-frac singles WITH a railed elevator (pp ≈ 0.93–1.0) arc
  12.8–15.5°; high-frac singles WITHOUT the rail (pp 0.01–0.10) arc 0–2.8°. The
  rail IS the arc, visible directly in the data, no inference needed. **E2's case
  is now the strongest of the three levers.**
- **P2 (timing extreme): confirmed EXACTLY but n=1.** The 0.24-frac/0.13-s/28.3°
  row reproduces verbatim; the only other low-frac single (0.38) arced just
  3.4° — the extreme is real but rarer than the echo implied.

**RULING AMENDED IN GROUNDS, NOT IN ORDER.** E1 remains first — but explicitly on
its true supports: (1) Chad's OWNER RULING (he does not want inverted flight
anymore; a character ruling needs no statistics), (2) bed-cleaning for every later
measurement, (3) smallest structural risk. It no longer claims a measured
chaos-deletion in single throws. **E1 re-capture pre-registration CORRECTED
accordingly:** expect the bank0/up0 columns to cluster level (the mechanism working)
and the compound-row chaos to shrink; the floor and the rail discriminator PERSIST.
A large single-throw arc improvement from E1 alone would now be a SURPRISE, not a
confirmation.

**E1 design catch — ENDORSED, and its falsifier is REGISTERED as a must-not-regress
row on E1's fly card:** stillness alone is not idleness in a world-anchored-cursor
architecture — a rim-pinned sustained turn has still hands and a commanded hard
bank. The dwell gate therefore requires aim-RESOLVED (nose on cursor) as well as
still hands. Falsifier, duration-stated: pin the cursor at the rim, freeze hands
two full seconds — the bank MUST hold. Verb scope endorsed: roll-input term in the
controller (the plant stays stability-free), full-range through inverted,
proportional inside 45°, one-directional ease-in, instant collapse on any input.

**The tally:** the federation's instruments are now four-for-four at correcting
confident readings — and this one corrected an ECHO against its own ARTIFACT within
the hour of the archive landing. The condition was not paperwork.

---

## 2026-07-31 (E1 at the gate) — E1 LANDED INERT (`ee20bc5`, verified read-only; Opus SHIP zero fixes); NEW LAW OF RECORD: dwellLevelRate ceiling ~0.55; verdict-flight card ENDORSED with one rider

**Verified read-only:** `ee20bc5` on `updraft`, `dwellLevelRate = 0` (THE flag —
tracker and term both inside `if rate > 0`; today's byte path). The verb is a
controller-scoped roll-input term, plant untouched; CS-5 registry amendment WAITS on
Chad's keep word (correct — never lands before the verdict, same-day ledger sync
when it does).

**Design facts now of record** (a ceiling unstated reads as arbitrary later):

- **`dwellLevelRate` DESIGN CEILING ≈ 0.55** (clamp saturation above); 0.5 is both
  the first flown value AND the boundary. Any future tuner raising it past 0.55 is
  buying nothing but saturation — written down so the knob's real range is known.
- **Gate stack:** still hands (post-synth delta) + rotational keys idle + aim
  RESOLVED with hysteresis (open 5°/15°, hold 10°/20° — the sustained-turn
  protection) + only while the aim path runs (gap-reset carriers in the else-branch
  and resetInput). Direction latch past 90° (IEEE ±π chatter), one-directional
  0.3 s ramp, instant collapse on any input, crossfaded into levelAssist's
  complement (peak summed authority 0.723 vs today's accepted 0.505 — the mid-bank
  band carries MORE leveling authority than today's law; that is why the
  upright-45° ease-never-snap row is load-bearing).
- **Opus proved, not sampled:** zero frames of leveling can leak into a throw; the
  parked-cursor chase is dead by construction.

**Verdict-flight card ENDORSED, one rider:** the rim-pin falsifier must be flown in
its REGISTERED duration-stated form — cursor pinned at the rim, hands frozen TWO
FULL SECONDS, the bank must hold — not only the moving-chase variant (the chase row
tests the resolved gate; the two-second pin tests the dwell gate; both are needed).
Ratification calls reserved to Chad on the card: Shift/Ctrl not resetting the dwell
(a hands-free powered climb levels itself — bless or veto) and the accepted
deep-stall corner.

**On keep:** CS-5 amendment lands in the EvC registry with same-day sync entry here,
closing the loop Chad opened in session 9. Then the re-capture on the SAME stamp —
pre-registered read stands as corrected: bad-start rows migrate to level starts at
unchanged throws-per-minute; the floor and the rail discriminator persist; E2 gets
designed against a clean bed.

---

## 2026-07-31 (alliance audit) — IS EvC2026 FULLY ON BOARD? Audit verdict: SUBSTANTIALLY YES, five linkage asks (L1–L5) to close the gap; the automation Chad asked for rides their EXISTING SessionStart hook

Chad's ask before the E1 verdict flight: is EvC2026 fully aligned with the engine's
SOPs, and can the constitution linkage be automated as cultural alliance. Audited
read-only against the manifesto's eight clauses.

**Already on board (better than assumed — credit where due):** the CS-1..9 locked
registry with grep-the-registry discipline; the `/evc-loop` SOP harness (orient →
one-change → verify → red-team → memory → handoff → approved commit) reminded by a
SessionStart hook every session; the red-team → build-to-spec → Opus-verify chain
(three stages ran it flawlessly tonight); design-doc-per-stage; `docs/tapes/` with
provenance-headed verbatim archives; mandalark conventions already cited in the
consult packet (symbol-not-line citations, lesson 1, artifact identification); and —
notable — their THREE existing eagle tags are all ANNOTATED (the convention the
plane's v12 deviated from; the student outdoes the reference here).

**THE FIVE LINKAGE ASKS (all changes in the EvC tree are the engineer's to make —
this ledger authors the ask; Chad relays):**

- **L1 — Constitution pointer in EvC's CLAUDE.md.** The orientation block names
  /evc-loop and HANDOFF.md but never mandalark. Add: mandalark-kernel is the
  governing preservation system; before any kernel-feel work read (read-only)
  `docs/MANDALARK_MANIFESTO.md` + `docs/DECISIONS-evc-eagle.md` there; the CS-sync
  rule named. A fresh EvC session today would not know its own vessel ledger exists.
- **L2 — THE AUTOMATION: a constitution step inside `/evc-loop`'s orient phase.**
  The SessionStart hook already fires every session and points at /evc-loop; add to
  the skill's orient checklist: "kernel-feel work? → read the eagle ledger's latest
  entry first; mandalark holds the base's seat; rulings land there." Cultural
  linkage that runs itself — no new infrastructure, rides what exists.
- **L3 — Execute the retro-seal: `eagle-v1` ANNOTATED tag, overdue.** Adopted at
  registration "on Chad's next accept flight" — stage C's KEEP came and went
  untagged. Natural moment: the E1 keep. Tag message carries Chad's verdict
  verbatim; recorded in mandalark VERSIONS.md same day.
- **L4 — `docs/tapes/README.md`.** The convention is currently only in one CSV's
  header: provenance header mandatory (stamp, instrument commit, flag values),
  verbatim always, append-only, supersede-never-overwrite; the four-file golden
  convention for when Golden Eagle Felt Flight #1 arrives.
- **L5 — Manifesto mirror: a short `docs/CONSTITUTION.md` in EvC** quoting the
  eight clauses, marked MIRROR (mandalark authoritative, update on amendment only)
  — so the culture is legible in-tree, not only by cross-repo reference.

**On this side, already standing:** the vessel ledger, the manifesto, the memory
system, and this repo's CLAUDE.md/SESSION_HANDOFF eagle sections. No new mechanism
needed here; the sync rule covers the rest.

**Sequencing:** none of L1–L5 blocks the E1 verdict flight (L3 explicitly WAITS for
it). Fly first; land the linkage in the same session's cleanup.

---

## 2026-07-31 (E1 verdict flight) — FLOWN. Verdict: FIX-AND-REFLY, not keep (two defects: too-slow ease, inverted flip-no-level). CS-5 does NOT land; retro-seal WAITS. And Chad's in-flight natural experiment CONFIRMS E2's thesis by feel

**Chad's verdict, verbatim (stamp `ee20bc5-dirty 2026-07-31 20:15`):**

> "the autolevel of wings kicked in when expected, but the time to level is way too
> slow. Also when auto level occurs the horizon flips, and does not autolevel. The
> curl is still happenning form a wings level to horizon orientation off a medium
> throw… When it goes straight (presiding condition being wings not level and
> closer to the max bank needed already by sheer chance of past manouver, the
> condition of a straight flight line profile from operator pov is achieved in this
> environmental circumstance and impetus directive relation."

**Rulings from the oversight seat:**

1. **E1 is FIX-AND-REFLY.** The verb fires when expected (gate stack works) but
   fails two card rows: ease rate way too slow, and the inverted case — which was
   a registered MUST — flips the horizon and does not level. **CS-5 amendment does
   NOT land; `eagle-v1` retro-seal WAITS; the CS registry stays as-is** until a
   kept refly.
2. **The too-slow finding collides with the ceiling law of record.** `0.5` is both
   the flown value AND the ~0.55 design ceiling (clamp saturation above). "Way too
   slow" therefore CANNOT be fixed by turning the knob — the authority envelope
   itself needs redesign, which is a red-team-again change, not a tune. The
   ceiling law did its job: it converts "just raise it" into a design question
   before a wasted flight.
3. **Inverted flip-no-level: defect candidates named for the engineer's
   diagnosis** (from the design as ledgered, not from the code): the direction
   latch past 90° (IEEE ±π chatter zone), and/or the crossfade with levelAssist's
   complement at extended bank. The inverted-recovery MUST came from Chad's
   original complaint (the inverted-bank pitch-down) — it is not optional.
4. **The persisting wings-level curl off a medium throw is the PRE-REGISTERED
   floor, confirmed on the stick** — expected, not a surprise; E2's territory.
5. **THE GOLD: Chad ran E2's discriminating experiment by accident and it
   confirmed the thesis by feel.** When past maneuver happens to leave the wings
   already near the needed bank, the same throw flies STRAIGHT from the operator's
   POV. Bank-present-at-pull = no arc; bank-arriving-during-pull = arc. That is
   the phased pull's entire mechanism, observed in the wild before E2 exists —
   the felt twin of the tape's rail discriminator (pp≈1.0 arcs 13–15°, pp≈0.05
   arcs ~0°). E2's design now starts with BOTH an instrumented and a felt
   confirmation on record.

**Next:** engineer's row-read of the E1 flight (in progress at entry time), then
the E1 revision through the standing chain (red-team the authority envelope +
inverted path), refly the SAME card with the two-second rim-pin rider intact.

---

## 2026-07-31 (E1 read-back + scoping ruling) — Both defects have NAMED OWNERS (crossfade caution-tax; stale-frame horizon). RULED: E1.2 as ONE revision flight with per-fix card rows; ceiling law to be RECOMPUTED-not-bypassed; F8 becomes a standing card line

**Engineer's read-back accepted** (each finding mapped to mechanism; stamp
confirmed as the real E1 build):

- **The verb's core is RATIFIED** — the triple gate (still hands + keys idle + aim
  resolved w/ hysteresis) survived first contact with Chad's real flying.
- **"Way too slow" owner: the crossfade** (red-team Medium-2's caution tax) —
  0.146 effective at 45° of bank against the 0.5 flown; the fade against
  levelAssist's authority cut the verb to ~15–30% at ordinary banks.
- **Horizon flip owner: the stale aim frame** — the window Opus flagged at the
  stage-C verify, arriving on schedule: during dwell the hands are still, so the
  frame never moves while the bird rolls level beneath it; the stage-C2 camera
  levels toward a stale, sometimes-inverted frame. Bird rights itself; horizon
  lies.
- **Honesty note:** no instrument rows — the card omitted F8 (tracker prints are
  F8-gated, flush on F9). Chad's stick report is the verdict material regardless.

**RULINGS (the base's seat):**

1. **E1.2 is APPROVED as ONE revision flight** — the frameUp carry (E1.1) + the
   speed retune together — on these conditions: (a) the two fixes have DISJOINT
   symptoms (ease speed vs horizon truth), so attribution survives a combined
   flight; (b) the refly card carries a SEPARATE duration-stated row per fix (the
   inverted park must both LEVEL and show a TRUE horizon — two observations, one
   maneuver; the upright-45° ease-never-snap row re-flown for the retuned speed);
   (c) the two-second rim-pin rider stays.
2. **Retune legality order ENDORSED** (soft-zone steepening → ramp → crossfade
   reshape) — but since the crossfade is the named OWNER, the reshape is
   pre-authorized rather than a last resort, on the stated discipline: **reopening
   the D3 arithmetic means the ~0.55 ceiling law is RECOMPUTED WITH it, never
   worked around** — the ledger's ceiling entry gets a supersession note citing
   the new arithmetic, supersede-never-overwrite.
3. **E1.1 frameUp carry ENDORSED, with one red-team question carried into its
   review:** the carry must be proven inert during freelook and across the
   release contract (same gates as the dwell verb — but state it, don't assume
   it). The generalization is real and welcome: every dwell becomes a frame
   re-level, retiring the Space-tap maintenance chore permanently — the carried
   tilt that stage C ruled "flying history" stops accumulating at all.
4. **PROTOCOL AMENDMENT, standing: F8 leads every instrumented card** — first
   line, before any maneuver. A card without its instrument step produced a
   verdict-only flight tonight; verdict-only was acceptable for E1's verb but
   would have voided the re-capture.
5. **E2's design opens with Chad's ablation quoted at the top** (previous entry) —
   the lever arrives with an instrumented confirmation (the rail discriminator)
   AND a felt one (bank-present-at-pull = straight) before a line of it exists.

**Unchanged:** CS-5 amendment and `eagle-v1` retro-seal wait for the clean keep on
the E1.2 refly.

---

## 2026-07-31 (E1.2 design review) — ENDORSED pre-build. CEILING LAW FORMALLY SUPERSEDED: the ~0.55 rate ceiling was an artifact of the multiplicative crossfade; new law = `dwellLevelTotalCap 0.8`, exact by construction. One caveat on the 0.8 license

**SUPERSESSION, stated plainly:** the `dwellLevelRate ≤ ~0.55` ceiling (this ledger,
"E1 at the gate" entry) is RETIRED WITH ITS MECHANISM — it was a property of the
multiplicative crossfade, not of the verb. E1.2 replaces the crossfade with a
structural total cap: dwell takes exactly the headroom levelAssist leaves under
`dwellLevelTotalCap = 0.8` — the summed-authority guarantee is now exact by
construction, not measured after the fact. The old entry stands as history; this
entry is its supersession note, per the recompute-not-workaround discipline. New
flown values: rate 0.65, proportional zone 30°.

**Design ENDORSED, with one caveat on the record about the 0.8 license:** the
engineer licenses the cap by "you flew the 0.723 band tonight and called it slow,
not snappy." Accepted as plausible but noted as INFERENTIAL — Chad's "too slow" was
a global verdict on a flight whose effective mid-bank authority was 0.146; how much
dwell time he actually spent at the 0.723 peak band is unmeasured. **The real
license is the re-flown upright-45° ease-never-snap row on the E1.2 card** — that
row, at the new speed, is what actually proves 0.8 safe. If it snaps, the cap (or
rate/zone) comes down; the flown row outranks the inference, as always.

**frameUp carry:** endorsed as designed; the attached freelook/release-contract
inertness question is confirmed as a MANDATORY Opus verification item, first in the
verify, not a prose claim. Card confirmed as ruled: F8 first; inverted park with
two observations in one maneuver, duration-stated (wings level AND horizon true
within ~2 s, both holding a further second); upright-45° ease re-flown; two-second
rim-pin; Chad's speed verdict in his words.

**On a clean keep, the closing sequence stands:** CS-5 lands with same-day sync;
`eagle-v1` annotated seal with Chad's verdict verbatim; alliance asks L1/L2/L4/L5
in the same session (L3 IS the seal).

---

## 2026-07-31 (E1.2 verify gate) — Opus SHIP-WITH-FIXES (`7dcdb95`); THE LATCH-ZONE IDENTITY IS PROVEN — the 0.8 cap is airtight BY PROOF; both mandatory items closed; Chad's E1.2 refly is next

**Verify verdict accepted and ledgered** (commit `7dcdb95` @ 21:15, verified in the
tree):

- **The identity HOLDS, exactly:** |φ| > 90° ⟺ UpVector.Y < 0, by the definition
  of atan2 — precisely the region where levelAssist is gated to zero. The dwell
  term and levelAssist are PROVABLY DISJOINT in the only zone where their signs
  could oppose; the total-cap's summed-authority guarantee is airtight by proof,
  not by testing. The superseded-ceiling arithmetic is now closed mathematics.
- **M2 strengthened beyond the design's own argument:** the frameUp carry is
  structurally locked out of the snapToChase window — the 0.4 s failsafe expires
  before the 0.5 s dwell can open. Disjoint by arithmetic, not by luck.
- **The caveat's letter enforced in code:** the 0.8 comment corrected to
  "licensed, unflown, the 45° row is the court"; the builder's wrong
  single-anchor release claim corrected to the real two-mechanism contract.
  Comment-only fixes, both.
- **Tier-4 659/659 exact, zero new findings, register floor untouched.**

**Standing at:** E1.2 flies on Chad's stick next — F8 first, the inverted park's
two-observations-in-one (level AND true horizon within ~2 s, holding a further
second), the 45° ease row sitting in judgment over the 0.8 cap, the two-second
rim-pin, his speed verdict in his words. **On his clean keep, the closing sequence
executes:** CS-5 lands with same-day sync entry here; `eagle-v1` annotated seal
with his verdict verbatim in the tag message (recorded in mandalark VERSIONS.md);
alliance asks L1/L2/L4/L5 in the same session.

---

## 2026-07-31 (STOP — owner's word) — E1.2 VERDICT FLIGHT BLOCKED on a camera regression: freelook release with override keys held gives an OBLIQUE view. The release law restated; attribution is a CLAIM to bisect, not context

**Chad's word, verbatim:**

> "I wont fly it and remark until the camera is fixed. It should only ever go to
> chase cam, right behind the eagle upon release of freelook. I am getting a
> oblique view when I maintain override key press this was a v12 migratory fix
> that was dropped in the last build."

**The law this violates — the kernel base's camera model, Chad's load-bearing
ruling (SESSION_HANDOFF §3, quoted never paraphrased):** release freelook → camera
snaps to chase, **directly behind** — every time, **keys held or not; keys are
irrelevant to this**. This was the plane's entire v9 arc (four attempts to close),
migrated to the eagle with v12-receive (R2 was a registered issue from day one).
An oblique view at keys-held release is the EXACT symptom the plane's v9 killed.

**Rulings:**

1. **E1.2's verdict flight is BLOCKED until the camera is fixed and the fix is
   flown.** The E1.2 card is unchanged behind the block; nothing else advances —
   no CS-5, no seal, no paperwork.
2. **"Dropped in the last build" is a CLAIM with the same standing as any
   attribution — bisect before fixing.** The suspect set is every eagle commit
   since the v12 release contract last demonstrably worked, not just `7dcdb95`.
   Note for the diagnosis, stated honestly: Opus PROVED the E1.2 carry disjoint
   from the snapToChase window — but a proof covers what it models; the keys-held
   release path is a DIFFERENT lane than the one proven, and the E1.2 verify also
   corrected "the builder's wrong single-anchor release claim" to a two-mechanism
   contract — that correction is itself a marker that the release path was
   misunderstood at build time at least once tonight. Bisect by stamp, reproduce
   with a stated maneuver (enter freelook, hold an override key, release freelook,
   keys still held — camera must arrive chase-behind), find the dropping commit,
   THEN fix.
3. **PERMANENT must-not-regress row added, effective immediately and on every
   future eagle card:** freelook release with keys held → chase directly behind,
   every time, duration-stated (arrival within the contract's snap time, no
   oblique dwell). The plane paid four kernel versions for this law; the eagle
   does not get to re-learn it by regression.
4. The fix restores the v12-migrated contract as specified — no redesign, no
   "improvement," restoration first; any redesign is a separate consult.

---

## 2026-07-31 — PROTOCOL AMENDMENT (Chad's word): every fly card begins with a FREE WARM-UP — fly first, feel first, THEN F8 and the card

**Chad's word, verbatim:**

> "add to flight cards to always fly a bit first to practice regular feel and the
> manouvers you will do before pressing f8 and going through the card becasue you
> can feel if something is off before you make judgements on what is being
> measured."

**Standing, federation-wide** (the fly-card template is the kernel base's; this
amends it at the source — mirrored in DECISIONS.md): every card's first line is now
a free warm-up — ordinary flying plus dry runs of the card's own maneuvers, BEFORE
F8, before any judged row. Purpose: the pilot's feel is the first instrument, and
it runs before the measured ones — if something is off (a regression, a lying
camera, a wrong build), it is caught before it can contaminate judged rows.

**Founding evidence:** tonight's camera regression was caught EXACTLY this way —
Chad felt the oblique release before flying the E1.2 card, and the STOP protected
every row on it. The card order is now: warm-up → F8 → judged rows → verdict.

---

## 2026-07-31 (late) — FIX FLIGHT FAILED on Chad's word ("nope its still not fixed"). STOP HOLDS. Attribution correction recorded honestly BOTH ways; two data reads + one feel question REQUIRED before attempt #2

**Chad's word:** "nope its still not fixed." The STOP holds; nothing advances.

**Attribution corrections, both directions, no smooth phrasing:**

1. **Chad's "this was a v12 migratory fix that was dropped in the last build" is
   SUPERSEDED by the bisect** (commit `a747da5`, packet §7.6, verified in the
   tree): NOT a regression — the v12 release contract (ask A) was NEVER ported to
   the eagle; the keys-held lane is provably untouched by all program commits; the
   oblique exists in the pre-stage-C baseline tapes. The bisect-before-fix ruling
   was FOLLOWED (this agent's earlier concern is withdrawn — the relay omitted
   the bisect report; the commit carries it).
2. **The port (`releaseSnapV12`, built inert, Opus SHIP-W-FIXES) FAILED ITS
   FLIGHT anyway.** A correct bisect and a correct-looking port still did not
   satisfy the stick. The plane took FOUR attempts at this same symptom; that
   scar governs now.

**REQUIRED BEFORE ATTEMPT #2 (no code until all three exist):**

- **Data read 1 — was the flag actually ON in the flown build?** The flip was an
  uncommitted edit (the `-dirty`); confirm from the flown log (flag echo /
  BuildStamp) that Chad flew `releaseSnapV12=true`. A flag-off flight would make
  "still not fixed" a NULL, not a failure — rule this out first.
- **Data read 2 — the release-scorer rows from the failed flight** (t0_behind /
  t0_tilt / keys_at_release, the frame-1 columns the port fix added): does the
  snap FIRE clean at the release instant (behind + upright at frame 1) or not?
  This splits the world: snap-fires-clean → the oblique lives in the ONGOING
  keys-held lane AFTER the snap (ask B territory — the plane's law is "keys never
  touch the camera, EVER"; audit whether the eagle's camera carries ANY key-driven
  term); snap-doesn't-fire → the port itself is wrong or gated out.
- **The feel question, Chad's alone to answer, in cockpit terms:** *at the moment
  you let go of free-look with the key still held — does the camera arrive behind
  the eagle and THEN drift oblique while you keep holding the key, or is it
  oblique from the very first instant?* First answer = the snap works and the
  ongoing-keys law is the defect (a DIFFERENT mechanism than the port). Second
  answer = the snap itself fails. The two fixes share nothing; flying attempt #2
  at the wrong one is the plane's v8 mistake replayed.

**Pre-registration rule imported from the plane's v9 arc, binding on attempt #2:**
rival causes stated in advance, the decision made by the scorer rows + Chad's
answer BEFORE the fix is designed, and the dominant term is the term that gets
fixed. No threshold patches on the minority term.

---

## 2026-07-31 (Chad's directive: mine the history) — THE HISTORY NAMES THE DEFECT AND THE FIX. The oblique is the plane's Card-1/keychase phenomenon: the camera follows a PARKED AIM while held keys walk the nose away. And the superseded-golden precedent (carry=0, v10) is the discipline the fix needs

**Chad's directive, verbatim:** "look at the history of this fix. There is a golden
that needed to be superceded, find that is the key to this look at the history of
this problem will be the right fix."

**Mined, from DECISIONS.md's v8→v10 arc — the mapping is one-to-one:**

1. **The eagle's port likely works in its own lane, and the oblique lives in the
   NEXT lane — exactly as on the plane.** v8's scar: "the snap still fires" was a
   tick-level fact that a kernel with the exact defect passed. v9's measured
   Card-1 trade, verbatim from the ledger: *"post-snap, hard key-only turning
   walks the nose away from the parked aim, and the lag camera follows the aim,
   re-opening a ~17°+ deflection view until the mouse takes over"*
   (`nose_after_1s` 23.1° → 40.9°). The eagle: release reseeds aim ON the nose
   (verified in `a747da5`'s code — `aimTargetDir = nil`, reseed-on-nose under the
   guard), the camera lag-follows the CURSOR (the S30 law) — then Chad MAINTAINS
   the override key, the nose walks away from the parked aim, and the camera
   stares oblique at the bird. His complaint is the Card-1 phenomenon, felt.
2. **The plane's history already contains the answer's shape: keychase.** The v9
   attribution entry records that S-keychase "had been masking a long-standing
   up-debt by RE-ANCHORING FORWARD" during key flight — the mechanism family that
   keeps the camera behind the plane while keys steer. The eagle received the
   snap port tonight but has NEVER had the keychase half: nothing re-anchors its
   aim/camera to the nose during sustained key-only flight. That absent half is
   the defect's owner.
3. **THE GOLDEN THAT NEEDED SUPERSEDING — the carry=0 precedent (v10):** the
   right fix changed arrival dynamics that controller goldens protected, and the
   discipline was a DELIBERATE golden move — pre-stated bar, pre-registered gate
   count — never a workaround to keep goldens green, never a silent re-record.
   That is Chad's key: **the eagle's right fix will touch behavior its own locked
   expectations protect** (the S30 "camera lag-follows the cursor" law; CS-2's
   exit-snap clause; the S21 ease already superseded under flag), **and the fix
   is to supersede those expectations DELIBERATELY, via Chad's word on the CS
   rows, with the bar stated before the numbers** — not to keep patching the
   release tick while the protected law re-creates the oblique every frame after.

**THE RULING SHAPE (Chad's own words tonight are the specification):** "It should
only ever go to chase cam, right behind the eagle upon release of freelook" +
maintained keys = **during ongoing key-override flight, the eagle's aim re-anchors
to the nose (the eagle's keychase), so the lag camera stays behind the bird.** The
S30 camera-follows-cursor law gains a KEY-FLIGHT clause by supersession — Chad's
word, CS-row amendment, same-day sync — the cursor law remains untouched for
mouse flight. Note the vessel divergence honestly: the plane ACCEPTED the Card-1
deflection view as a feature (deflection gunnery); the eagle REJECTS it — two
vessels, two rulings, both on the owner's stick, exactly what the vessel doctrine
is for.

**Gate for the fix build (supplements, does not replace, the prior entry's
gates):** scorer read confirming t0 clean (predicted by this diagnosis: the snap
fires; the oblique develops AFTER, while keys are held); rival-cause
pre-registration collapses to confirming that prediction; the fix designs the
keychase half, red-teamed, with the CS supersessions stated in advance.

---

## 2026-07-31 (keychase design endorsed pre-red-team) — The mechanism made EXACT: aimHeadingLag τ≈0.45 s parks the camera ω/2.2 ≈ 50° off the nose in a hard keys turn — the §7.2 baseline's 42–86° family, matched. Tombstone adjudication ACCEPTED

**The engineer's code-read completes the diagnosis with numbers:** with keys held
the camera already targets the flight path — but through the MOUSE lag law
(`aimHeadingLag = 2.2`, τ ≈ 0.45 s, tuned for cursor hang in mouse flight). At a
~2 rad/s keys turn that lag parks the camera a STANDING ~50° off the nose — the
§7.2 keys-baseline's 42–86° oblique family, matched from first principles. At
human granularity (25°+ by a quarter second) it reads as "oblique the instant I
release." Frame 1 snaps clean; every frame after obeys a law never written for
key flight. **The snap port was necessary and insufficient — both attribution
claims survive.** The 42–86° baseline row, captured three days before the fix
existed, just became its diff target: the baselines pay rent again.

**Tombstone adjudication ACCEPTED as sound:** the plane DELETED S-keychase because
the plane's aim stays live under keys — keychase fought the mouse mid-combo. The
eagle's CS-1 makes that fight structurally impossible: keys-held = the aim exerts
zero pull, so a nose-anchored camera during key flight cannot contradict a mouse
that isn't flying. The mechanism the plane buried is safe in the eagle BECAUSE of
a registry difference already locked — the correct form of cross-vessel reuse
(adopt the concept where the tombstone's reason doesn't apply; cite the tombstone).

**Design ENDORSED as scoped:** fast `keysChaseLag` heading reference during
keys-held aim mode (behind the eagle, period); on key release, Chad's law verbatim
— mouse instantly at full authority, camera EASING back into lag pursuit, no snap
at the seam, existing exponential machinery carrying both transitions. CS-8
Stage-1 camera-follow clause gains a key-flight exception ON CHAD'S WORD, stated
before the numbers; S30 cursor law untouched for mouse flight; same-day sync;
Chad's post-fix flight ratifies. **Pre-registered discriminator endorsed,
including its honest arm:** fix-build keys-held release rows must show t0 clean
AND 0.25/1/3 s all small; a dirty t0 means the snap claim was wrong too →
re-attribute, not patch. Red-team focus confirmed: the key-press/key-release
seams (S26d), E1-dwell and C2 interactions under the new reference, flag-off
byte-identity.

**Chain unchanged:** red-team → build → Opus → inert commit → Chad's flight. The
STOP holds until his sentence is honored on his stick.

---

## 2026-07-31 (keychase red-team gate) — REVISE, no Critical/High; mechanism survives whole. CORRECTION TO THIS LEDGER'S OWN WORDING accepted (Low-6); the pure-B instrument and pre-registered floors are the findings that matter

**Red-team verdict:** REVISE → PROCEED, no Critical/High — every finding a
card/design-text amendment, eight revisions of record binding on the build
(`docs/STAGE-B-KEYCHASE-DESIGN.md` in the eagle tree).

**Correction accepted against THIS ledger's previous entry, stated plainly:** the
endorsement's "structurally impossible" was overclaimed. The precise adjudication:
CS-1 deletes the CONTROL fight (no live aim law under keys) — but the cursor
remains mouse-movable under keys, and the tightened camera WILL send a pre-placed
cursor off-screen where today's oblique incidentally kept it near view. That is an
INTENT-VISIBILITY trade, not a control fight, and it is RULED by Chad's sentence
(chase behind the eagle, period). The tombstone adjudication is sustained on the
corrected wording.

**The findings that will matter later, on the record:**

- **The pure-B instrument (revision 3):** the §7.2 keys standing-oblique rows
  (42–86°, no release involved) must collapse to the no-free-look control
  ballpark — B's acceptance line independent of any release. Frame-1 = pure A;
  keys-oblique = pure B; release 0.25/1/3 s = the composition. The key-PRESS seam
  has no instrument — attributed by carded feel rows only, stated honestly.
- **Pre-registered floors (revision 4)** so a CORRECT build reads clean: t0
  tolerance absorbs one same-frame ease step; the 0.25 s keys-row floor is the
  lookAheadFactor settle toward the flight path — correct behavior, not drift.
- **Velocity-step honesty (revision 7):** the key-press hurry-behind is a
  cap-limited ~206°/s swing — that IS the sentence, carded so pass/fail is
  judgeable; `chaseTurnRate` OFF-LIMITS as a tuning response to the re-find row.
- **A+B attribution for the combined flight** pre-stated: stage A is technically
  unflown-clean; the scorer's t0-vs-later split carries the attribution; dirty t0
  indicts A too → re-attribute.

**Chain position:** Sonnet building; Opus next; inert commit; both flags flipped
for Chad's flight; the STOP holds until his sentence is honored on his stick.

---

## 2026-07-31 (MEANING STOP — Chad's word) — "i did not fly you guys are just wrong on meaning" — the KEYCHASE INTERPRETATION IS HALTED pre-flight; his words re-specify before anything else moves

Chad's "noppe" was NOT a flight verdict — no flight occurred. His correction,
verbatim: **"i did not fly you guys are just wwrong on meaning."** The agents'
interpretation of his camera sentence — extended by the keychase design into an
ongoing-keys camera law (camera hugs the nose at chase tightness during held
keys, hurry-behind at the key-press seam) — is REJECTED as a reading of his
intent, before any flight tested it.

**Ruling to both agents:** the engineer HOLDS the keychase build (no further build
steps, no flag flips, no flight card on the current design); the interpretation
chain re-opens at its source — Chad states the meaning in his own words, and
those words re-specify the design from the top. This is SESSION_HANDOFF §7
operating exactly as written: his words are the specification; when an
interpretation is wrong, the failure is in the agents' translation, not in the
words; capture the restatement verbatim and rebuild the mechanism reading from
it, never from the previous translation.

The A-stage port (`releaseSnapV12`) and its flight status are UNAFFECTED by this
entry — it too awaits his re-specification before any further verdict is drawn.

---

## 2026-07-31 — CHAD'S ONE STATEMENT: THE EAGLE CAMERA STATE LAW (verbatim, then organized). KEYCHASE IS WITHDRAWN — a misreading. Two oversight errors corrected plainly

**Chad's statement, verbatim — this IS the specification:**

> "one statemnet… Free look release goes to mouse suthority. you dont chase the
> keypress. Free look release is a change of camera mechanism. During free look
> camera is controlled by me. During mouse aim state, activated instantly when I
> release the camera is in a lag state with the mouse. The moment I release free
> look my view snaps to chase. Then camera is going to obey the free and
> independent mouse….. not thing to do with keys. I can make my eagle turn
> oblique after it dosent matter, the cam only lags my mouse aim, what the eagle
> does has nothing to do with what the cameera follows including hard key press.
> This is a suble understanding that needs to be made clear and explict. The
> organization of elements in the relationship need to be made more specific,
> organized and explicit as laws and the notes better worded to reflect what I
> want. I thought you knew the state of the relationship but ,,, this is not
> clear yet to any new agent coming in here.. that or we are over injected by
> the gravity of this superceded bug."

**THE LAW, ORGANIZED (translation checked against the verbatim above; if they ever
disagree, the verbatim wins):**

- **Two camera states, one transition.**
  - **State 1 — FREELOOK:** the mouse controls the CAMERA (Chad's situational
    awareness). Keys control the bird. The aim is inert.
  - **Transition — RELEASE (a change of camera MECHANISM, instant):** the view
    SNAPS to chase — behind the eagle, upright. Mouse authority over the aim is
    instantly re-established. One snap; nothing eased.
  - **State 2 — MOUSE-AIM:** the camera is in a LAG state with the MOUSE. The
    camera follows the aim and ONLY the aim. **Keys have nothing to do with what
    the camera follows — ever.** The eagle may turn oblique in frame under hard
    keys; that does not matter; the camera does not chase the bird, it lags the
    free and independent mouse.
- **Corollary (the killed misreading):** there is NO key-flight camera clause. A
  camera that chases the NOSE during held keys is WRONG in state 2 — it would
  make the camera follow the bird, violating "the cam only lags my mouse aim."

**KEYCHASE IS WITHDRAWN — tombstoned as a MISREADING, second reason on the same
stone as the plane's deletion.** The design, its red-team, and its card do not
fly and are not built. (The design doc stays in the eagle tree as history with a
withdrawal banner — never deleted, per house rule.)

**Two oversight-seat errors, owned plainly:**

1. **The "vessel divergence" I recorded was FALSE.** The eagle does NOT reject
   the Card-1 deflection view — Chad just ruled the opposite ("I can make my
   eagle turn oblique after it dosent matter"). Both vessels accept it. The
   divergence entry (`aca4e00`) is superseded on this point.
2. **The history-mined diagnosis over-reached.** The archive correctly named the
   Card-1 PHENOMENON as what the camera does under keys-after-release — but I
   wrongly promoted the phenomenon to being Chad's COMPLAINT. His complaint is
   the TRANSITION (the release mechanism-change misbehaving with keys held), not
   the state-2 law. Chad's own closing line names the failure mode: "over
   injected by the gravity of this superceded bug" — we imported the plane's
   whole battle instead of only its law.

**What remains genuinely unfixed, re-opened under the correct law:** at
release-with-keys-held, Chad's flown verdict on stage A stands ("nope its still
not fixed") — the transition does not deliver what state law demands on his
stick. Diagnosis re-opens THERE: the engineer reads the stage-A flight's scorer
rows under the corrected law (is the snap-to-chase itself wrong — placement,
uprightness, timing — or is state 2 misbehaving in its OWN terms, e.g. the
camera failing to lag the aim and instead tracking something key-coupled?). No
design until that read exists and Chad's words confirm the residual symptom in
state-law terms.

**Standing directive from Chad executed by this entry:** the relationship is now
stated as explicit organized law in this ledger. The engineer mirrors it into
the CS registry wording / code notes (their tree, their motion, same-day sync
rule applies) so "any new agent coming in" meets the law before the code.

---

## 2026-07-31 — CONFIRMED FROM THE SEALED RECORD: the eagle's camera state law and the plane's v12 law are THE SAME LAW (shared-kernel law, not per-vessel character)

The engineer verified against the v12 snapshot: freelook = aim carried/camera
free; mouse-aim = camera bound to the aim; **keys affect NEITHER** — the plane
DELETED S-keychase and its `key_anchor_rate` dial rather than defaulting them to
zero. The data-flow statement of the law: the camera's REFERENCE is the mouse in
freelook and the AIM in mouse-aim state; the aim's only author is the mouse;
**there is no path from keys to camera, ever**; the bird is IN frame, not the
frame's TARGET.

**Standing consequence:** this is a SHARED-KERNEL law (both vessels, one law),
not a character choice — it joins the release-snap contract in the
must-not-regress family at the federation level. The "vessel divergence" entry is
fully collapsed. The engineer is repairing three contaminated E-series learnings
by supersession-in-place, per the discipline — nobody embarrassed, the record
corrected where it lives.

**Unchanged:** diagnosis waits on Chad's state-law answer (does the SNAP arrive
wrong, or does the camera DISOBEY THE MOUSE after a correct snap?) + the stage-A
scorer re-read under the corrected law.

---

## 2026-07-31 — CROSS-REPO POINTER: the camera state law is mirrored in TESGI. Three repos, one law, loop closed in both directions

**The TESGI mirror is landed and verified at the artifact** (this agent read the
commits and diffs in `D:\mandalark-game_eng` directly, per lesson 7 — never from
the third seat's echo):

- **`e88c7a4`** — law introduced as shared-kernel federation law at
  `30_method/03_FEDERATION_MODEL.md` §9.1.2 (under what-ports-by-default), with
  this ledger's `ac2371f` / `d3dc8c2` / `5655bbb` cited as evidence.
- **`5997a22`** — authority + status fields completed: the filing states that
  §9.1.2 is TRANSLATION subordinate to Chad's verbatim block in this file's
  "CHAD'S ONE STATEMENT" entry (`d3dc8c2`), verbatim wins on any conflict, per
  that entry's own rule. Status filed exactly: law RATIFIED in both ledgers; the
  release-transition defect OPEN against it (flown "nope" stands); eagle
  CS-registry mirror cited as PENDING; explicit sentence that the filing must
  never read as the fix having landed. Arc record + the "over injected by the
  gravity of this superceded bug" lesson (mine the archive for the law, not the
  war) at `40_learnings/GOLD_ADDENDUM_2026-07-31_eagle-keychase.md`, including
  the two-reason tombstone fact and the excerpt-vs-canonical note.

**Traceability chain, now closed both directions:** Chad's verbatim
(`d3dc8c2`, this file — the specification) → sealed-record confirmation
(`5655bbb`, this file) → TESGI filing (`e88c7a4` + `5997a22`,
`cjcgervais/mandalark-game_eng` main) → this pointer entry. The eagle-side CS
registry mirror remains the one open leg (engineer's tree, same-day sync rule) —
when it lands, its entry here cites both this pointer and the CS row.

**Board unchanged by this entry:** diagnosis still waits on Chad's state-law
answer + the stage-A scorer re-read; the STOP holds.

---

## 2026-07-31 (late night) — S60 THE LAW-4 EXCISION (`2229160`): the diagnosis answered itself in the code — the S58 `not keysHeld` fallback WAS the forbidden keys→camera path. The fix is a DELETION. Flight pending; STOP lifts on Chad's word alone

**Verified at the artifact** (read-only `git show` in `D:\EvC2026`, per lesson 7
— the deletion confirmed in the diff, not the engineer's echo): commit
`2229160` on `updraft`, pushed. The hoisted `keysHeld` local is deleted; both
predicates that consumed it (`easeTarget` and the C2 `aimDriving`) lost their
`not keysHeld` term; `updateCamera` reads zero keyboard state. The forbidden
path does not exist as code — deleted, not flag-zeroed, per the convergent
shared-kernel law ("S-keychase and its dial were DELETED rather than defaulted
to zero").

**The pre-registered question is ANSWERED — by code-read, second branch:** the
snap was not the residual defect; **state 2 disobeyed the mouse.** The S58
Stage-1 fallback made the camera abandon the aim and lag-chase the flight path
(`followDir`) whenever any flight key went down — a key-coupled camera
reference, the exact structure Chad's law forbids ("keys have nothing to do
with what the camera follows — ever"). The keys-held oblique was that fallback
operating every frame after a clean frame-1 snap.

**Also in the commit:** `docs/CAMERA-LAWS.md` (the law organized in the eagle
tree — states, transition, Law 4's absolute exclusion, the
presentation-decoration clarification protecting bank tilt, the "over injected
by the gravity of this superceded bug" postmortem); the keychase build was
built, verified, and **rejected by Chad pre-commit** — reverted, tombstoned in
`GameConfig`, design doc kept as history; `releaseSnapV12` committed **TRUE**
(under the ratified law the instant snap IS Law 2 — the eased return was the
violation). Tier-4 659/659, rojo PASS.

**Two observations from the artifact, flagged for the engineer (echo-rule
finds, not blockers):**

1. **The serve stamp honestly reads `2229160-dirty`** — working tree carries
   uncommitted `CLAUDE.md` and `src/shared/BuildStamp.luau`. The stamp
   self-declares, which is the discipline working; but the claimed "memory
   updated so no future agent can arrive at tonight's confusion" lives in the
   **uncommitted** `CLAUDE.md` — it should be committed or it dies with the
   session.
2. **CS-8 rewrite + registry amendment still pend Chad's flown word** (stated
   in the commit itself) — the CS-registry mirror leg of the same-day sync rule
   remains open, now explicitly gated on the flight.

**The flight card as issued (warm-up first, per the federation template):**
release-with-keys-held ×3 (snap instant, keys irrelevant) → sustained key turn
(camera holds the aim, eagle banks oblique in frame — the law operating) →
mouse mid-keys (camera obeys it) → clean releases ×3 + normal mouse flight
(loved lag pursuit, byte-untouched).

**The STOP lifts on Chad's word alone.** His flown sentence ratifies or we go
again; CS amendments and this ledger's closure entry land on that word.

---

## 2026-07-31 (late night) — TESGI pointers current through the S60 beat (`c6e0ba1`, `274490c`, verified at the artifact)

Two TESGI commits verified in `D:\mandalark-game_eng` (read-only, tree clean,
pushed): **`c6e0ba1`** records the return pointer for this ledger's `ac5e930`
and the fix's BUILT-NOT-FLOWN status; **`274490c`** pins the S60 arc into the
gold addendum against this ledger's `d20f8e9` (verified there first) — the
completed attribution with its filed lesson (*a well-formed question is itself
an instrument*: the two-branch question was answerable by code-read because it
was posed in state-law terms), the dirty-stamp self-declaration as a
discipline nugget, and the echo-rule-run-by-every-seat observation. Sync
current in both directions through this beat. Board unchanged: one thing left
in the world — Chad's flight.

---

## 2026-07-31 (late night) — CORRECTION, SUPERSEDING TWO OF THIS LEDGER'S OWN CLAIMS: the dirty stamp is a CONSTANT, not a signal; the memory update was never at risk

The engineer examined the dirty state; this agent verified each corrected fact
at the artifact. Three claims in this ledger's S60 entry and the TESGI-pointer
entry are superseded:

1. **"The claimed memory update lives in the uncommitted `CLAUDE.md`" — FALSE.**
   The memory update was written to the engineer's memory directory
   (`project-kernel-v12-consult.md`, verified present), independent of any
   tracked file. It was never at risk. What sat uncommitted in `CLAUDE.md` was
   ~51 lines of accidentally pasted conversation transcript — noise, since
   backed up to scratchpad and restored; `CLAUDE.md` is clean (verified:
   `git status` now shows only `BuildStamp.luau` modified).
2. **The "dirty stamp self-declaring" instrument framing — WRONG CLASS.**
   Verified structurally in `tools/Write-BuildStamp.ps1`: the tool writes the
   TRACKED `BuildStamp.luau` and then computes the dirty flag from git state —
   so every serve after a commit dirties the tree with the stamp's own
   regeneration, and the suffix reads `-dirty` on any tree, forever. **The
   dirty bit here has zero discriminating power.** A session trusting it as an
   alarm chases nothing; a session trained to ignore it misses a real
   uncommitted change.
3. **The TRUE nugget from this beat, replacing the filed one:** *"no stamp, no
   verdict" must compare the HASH, not the dirty suffix.* The commit hash in
   the stamp discriminates; the suffix does not.

Items 1 and 3 of the TESGI arc pin (attribution-by-code-read;
echo-rule-as-practice) stand as filed. This correction flows to the TESGI
record through the third seat; its commit gets a pointer here when it lands.
Note the shape of tonight's second imported-gravity error, same family as the
keychase misreading: an instrument framing filed from a plausible reading of
the state instead of an examination of it — the lesson-7 discipline applies to
DIAGNOSES of artifacts, not just their identification.

Board unchanged: `2229160` pushed and verified, CS-8 gated on Chad's word, the
flight the one thing left.

**Pointer (added when the leg closed):** the TESGI correction landed as
**`a080b49`** (verified at the artifact — supersession in the gold addendum:
retraction, the replacement discriminating-power nugget, the error family
named against this ledger's `bd6b515`; items 1 and 3 standing). Sync current
in both directions through the correction beat.

---

## 2026-08-01 — S60 FIX FLIGHT: FAILED. Chad's flown word, verbatim: "no camera chase on release." The STOP holds; the failing clause is now the TRANSITION ITSELF — the pre-registered re-attribute arm FIRES

**Chad flew the S60 build (`2229160`, `releaseSnapV12 = TRUE` live). His flown
verdict, verbatim — this is the specification of the failure:**

> "no camera chase on release"

**What this means against the ratified law:** the violated clause is **Law 2,
the transition** — release of freelook is NOT delivering the one instant snap
to chase behind the eagle. This is the FIRST branch of the two-branch
question, the branch the code-read attribution ruled out ("the snap fires
clean at frame 1; state 2 disobeyed"). The flown word contradicts that
prediction on the stick.

**The pre-registered discriminator's honest arm FIRES — by its own words,
binding:** *"a dirty t0 means the snap claim was wrong too → re-attribute,
not patch."* The S58-fallback deletion may still be correct law-enforcement
(Law 4 stands regardless), but it was NOT the owner of what Chad feels at
release — the attribution is re-opened, not amended. No patch on the current
attribution; attempt #3 starts at re-attribution.

**Gates standing for the next attempt (all previously registered, none new):**

1. **Archive-first (standing discipline):** before any design, mine the ledger
   history for THIS symptom class — note the snap-does-not-arrive family is
   new on the eagle (prior symptom was oblique-AFTER-snap); the plane's v9 arc
   distinguished A (forward term) from B (up term) with the rule *the dominant
   term is the term that gets fixed*.
2. **Scorer rows before design:** the engineer reads the release rows from
   THIS flight's build (frame-1 placement, uprightness, timing) — the
   fix-build discriminator rows exist for exactly this question.
3. **Flag-state read first (imported at attempt #2, still binding):** verify
   what `releaseSnapV12 = TRUE` actually executes on this build before
   attributing — a flag believed live that isn't, or a path behind it that
   doesn't run, is the cheapest hypothesis and must be excluded by read, not
   assumption.
4. **Rival causes pre-registered, decision by rows + Chad's words BEFORE the
   fix is designed** (the v9 rule, imported and binding).

**The STOP holds.** Diagnosis is the engineer's seat; this entry is the flown
record and the gate. Sync legs (engineer's read, TESGI pin) get pointers here
as they land.

---

## 2026-08-01 — FLAG-STATE READ COMPLETE (gate 3, run first as cheapest): by read, the snap MUST arrive on `2229160`. The LEADING RIVAL is a STALE-BUILD FLIGHT — and the stamp-hash nugget goes to the front of the queue one hour after it was filed

**The engineer ran the registered flag-state gate before any hypothesis; the
checkable facts verified at the artifact by this agent** (file timestamps
read directly; snap-chain symbols confirmed at their cited sites in
`BirdController.client.luau`).

**The code read — `releaseSnapV12 = TRUE` executes a real snap on `2229160`:**
arm at Space `InputEnded` → `setFreeLook(false)` → `releaseSnapNow = true`
(HOLD-Space confirmed live, so release is the correct trigger); consume sets
`chaseDir = -nose`, level `camUp`, `aimHeading = nose`; write is a direct
same-frame `camera.CFrame = targetCF`, no lerp. The staleness guard can only
eat the snap on a dead `flightEngine`/`rootPart` — impossible mid-flight.
**Branch 1 failing on this code has no mechanism the read can find.**

**But the flown build is UNVERIFIED, and the pipe evidence points the other
way** (timestamps verified by this agent):

- **No capture from the fix flight exists on disk.** All eight log/logpic
  PNGs are 2026-07-30 (23:32–23:48) — the night BEFORE the S60 commit.
- **`EaglesVsCrows.rbxlx` was baked 2026-07-31 23:21:18 — one minute BEFORE
  the commit (23:22:26).** A place file from that moment carries a pre-commit
  tree (stamp would read `a747da5-dirty`, not `2229160`) — and the rejected
  keychase build lived and died uncommitted in exactly that window. Opened in
  Studio without the Rojo serve connected, that file flies a tree the
  excision never reached, possibly with the keychase in it.

**The corrected nugget pays rent immediately — front of the queue, per
no-stamp-no-verdict:** the discriminator is the HASH from the flown session.
**Open question to Chad:** what did the HUD corner / `[BuildStamp]` boot line
read on the fix flight? `2229160` → the verdict indicts the snap design and
re-attribution proceeds for real. Anything else → the verdict was rendered on
a build the fix never reached — the pipe, not the code — and the flight
re-runs on a confirmed serve. Second discriminator, on a confirmed-`2229160`
build only: (a) does the view fail to REACH chase (stays where freelook left
it) or snap-then-misbehave; (b) clean release, keys-held release, or both.

**Engineer's position, filed under the honest arm and endorsed by this
ledger:** the code read cannot confirm the defect on `2229160`; the leading
rival cause is a stale-build flight; **no fix gets designed until the hash is
on the record.** This is gate 3 doing exactly what it was registered to do.

**Two records corrected in passing:** (1) the eagle tree's `CLAUDE.md`
controls section still says Space is a TOGGLE — stale since the 2026-07-13
revert to HOLD (engineer's tree to fix). (2) The engineer's report attributes
a "looks like your flight evidence" flag on `log3.png` to the docs agent — no
such flag exists in this ledger or this seat's record (this ledger only ever
listed the PNGs as untracked files); noted for accuracy, source unknown,
nothing turns on it.

**Addendum (pre-flight, re-run pending):** the CLAUDE.md fix landed as
**`c36f1c3`** on `updraft` (verified at the artifact: doc-only, 1 line,
CLAUDE.md alone). **The valid-hash set for the re-run is therefore
{`2229160`, `c36f1c3`} — identical camera code, both count as the fix
build.** The still-running serve (started 23:23) stamps `2229160-dirty`; a
restarted serve stamps `c36f1c3`. Any other hash — especially `a747da5` —
means the pipe. Chad is flying the re-run now; the verdict entry follows his
word and his hash.

---

## 2026-08-01 — S60 RE-RUN FLOWN: **IT'S A GO.** Hash confirmed `2229160` from the flight's own logs. The earlier FAIL is SUPERSEDED as Chad's own misattribution, owned in his words. THE STOP LIFTS. The camera arc on the eagle CLOSES

**The hash, first — no-stamp-no-verdict satisfied on both counts.** This
agent read the flown sessions' Roblox Studio logs directly: both of tonight's
sessions (boot lines 07:00:37Z and 07:25:06Z / 07:33:19Z) print
`[BuildStamp] 2229160-dirty 2026-07-31 23:23`. **Chad flew the fix build.**
(The `45ea3c2-dirty` line in the same logs is the FarmLevel baked-artifact
stamp, not the camera code — the `a747da5` family never appears.) The verdict
below is rendered on confirmed-fix code.

**Chad's flown verdict, verbatim — this is the ruling:**

> "Yea I think you got it all this time and last time too. I may have
> missattributed one or two of these rounds to the oblique camera because in
> a sustained turn, as said my own oblique law obeyed and my camera
> independent of the keys, my eagle zooms a circle and I watch it from
> wahtever angle I want as soon at my velocity stops changing with keyboard
> override, the snap occurs as I was always looking at my mousee aim, but on
> releasing keys the eagle goes back to follow the mouse aim too = alignment.
> This is an improvement of my own understanding of my kernel laws and making
> them explicit is warranted and right by the standards of this codebase. I
> attribue my error to the tight turn circle of the eagle and to my rather
> far back chase angle. With such a fast circle and my mouse going where i
> want the camera is ideal. It looks different from the way the plane might
> behave in the seads game because of the turn radius and manouver aspect
> ratio to chase distance. It is the same thing but looks different. So we
> are open I will record now .... check the logs for my flight but my its a
> go stands.. Other proof that I was wrong and that this is actually correct
> as asked is that I was able to follow my eagle through the movement of my
> mouse with key override press performing tight loops. THe camera will chase
> but I have to manual follow. But with taps of the keys, I stay in chase
> view, when in full deflection looping I can choose to chase with mouse aim
> and cam will follow otherwise release and aim my flightline rather than
> sustain press full deflection keys. ---> I will come out of it knowing
> where me eagle will point and my cam will point upon releasing the keys.
> This one on me :)"

**What this ruling does, organized (verbatim wins on any conflict):**

1. **The 2026-08-01 FAIL entry ("no camera chase on release") is SUPERSEDED
   as a misattribution — Chad's own, owned in his words** ("This one on me").
   The snap fires; the law operates; what he saw in a sustained key turn was
   Law 4 obeyed — camera on the mouse aim, eagle zooming its circle in frame,
   watchable from any angle. The re-attribute gate closes with no code defect
   found: the flag-state read's "branch 1 has no mechanism" was correct.
2. **The vessel-presentation insight, cascade-worthy:** the eagle's tight
   turn circle plus its far-back chase angle make the IDENTICAL shared-kernel
   law LOOK different from the plane — "turn radius and manouver aspect
   ratio to chase distance… It is the same thing but looks different." One
   law, two presentations; presentation divergence is geometry, not law
   divergence. (Candidate for a cascade entry when the law is extracted.)
3. **The technique record, in the ruling:** taps of the keys keep chase view;
   full-deflection looping is a CHOICE — chase with the mouse aim (camera
   follows) or release and aim the flightline; and the predictability
   outcome is the comfort doctrine satisfied on the eagle: *"I will come out
   of it knowing where me eagle will point and my cam will point upon
   releasing the keys."*
4. **Explicitness ratified:** "making them explicit is warranted and right by
   the standards of this codebase" — `docs/CAMERA-LAWS.md`'s
   ratification-pending banner may flip to RATIFIED (engineer's tree, their
   motion).

**Consequences now unblocked (engineer's motions, same-day sync):** the CS-8
rewrite + registry amendment on the flown word; `releaseSnapV12 = TRUE`
stands ratified; the S60 fly card records hash `2229160` + this verdict.

**THE STOP LIFTS. The eagle camera arc — R2, the oblique complaint, the
keychase misreading, the state-law restatement, the Law-4 excision, the
stale-build scare — CLOSES here, on Chad's word, on his hash, on his own
improved understanding of his own law.** Next by the standing ruling: the
E-series resumes (E1 dwell settle-to-level → E1 re-capture → E2 phased pull →
lineHoldFF sweep last).

---

## 2026-08-01 — CS-REGISTRY SYNC (same-day rule): CS-2 and CS-8 amended by `bb99d82` on the flown word. The row diffs verified at the artifact

The engineer's S60-close commit **`bb99d82`** (`updraft`, pushed, verified —
the amendment is in `docs/HANDOFF.md`, where the CS registry lives) amends two
rows, both carrying Chad's verdict and the stamp hash in the row per
discipline. This entry names them, per the sync rule:

- **CS-2 — EXIT-SNAP SUPERSEDED (S60, flown KEEP, stamp `2229160-dirty`
  confirmed in the Studio log):** under `releaseSnapV12=true` (committed TRUE
  in `2229160`), freelook release is a ONE-FRAME direct snap to the chase
  pose (Law 2) with a same-frame level `camUp` — the tilted-horizon flash
  that motivated the S21 ease is deleted by the snap's own level-up write.
  The S21 fast eased return remains byte-identical as the flag-off path.
- **CS-8 — S60 CAMERA-LAWS AMENDMENT (flown KEEP, same stamp):** the camera's
  follow reference in mouse-aim is THE AIM and nothing else; **key state is
  NEVER a camera input (Law 4)**; the S58 `not keysHeld` fallback in both
  `updateCamera` predicates was the violation, DELETED not flag-zeroed;
  oblique-in-frame under held keys is the law operating. Chad's misattribution
  note preserved in the row ("same thing, looks different" — turn circle vs
  chase distance geometry); the keychase build tombstoned in `GameConfig` —
  do not rebuild.

Also in the engineer's close: `docs/CAMERA-LAWS.md` is now the RATIFIED law;
the stale-build rival formally dead on the log evidence; the geometry-ratio
lesson written to the engineer's memory (`project-kernel-v12-consult`). The
one earlier-registered item this closes out: the CS-registry mirror leg that
the TESGI filing cited as PENDING is now LANDED — the TESGI seat can flip
that citation on its next pass.

**This closes every leg of the S60 arc on this ledger.** Eagle board next:
the E1.2 card, queued and unflown (inverted park — wings level + honest
horizon within ~2 s holding a further second; upright-45° ease judging the
0.8 cap; two-second rim-pin; Chad's speed verdict) — on Chad's stick,
whenever he's ready.

---

## 2026-08-01 — RED-TEAM CONSULT (docs agent, two-lens Fable panel): the wobble night reviewed. Pitch gain was NEVER touched; the autolevel is a chattering crossfade, not a separation; the camera constants were never in the compensation set

**What was reviewed:** the eagle tree's UNCOMMITTED working state on `11aee38`
(stamp `11aee38-dirty 2026-08-01 02:30`) — the S60 "compensated package"
(rollRate 8.4 kept; aimRollGain 7.5→2.86, aimLevelGain 0.9→0.34,
aimHeadDamp 0.45→0.17, bankKeyScale 0.40 added; aimRollDamp 0.94 applied and
WITHDRAWN same night) plus the uniform level-off v2
(`dwellLevelUniform=true`, rate 1.0, time 0.5→0.15, ramp 0.3→0.12,
stopDeg 10) and the [EvC shake] tracker. Two independent read-only Fable
red-team agents: one attacking the diff's engineering claims against the
plant code, one tracing Chad's three felt symptoms. They converge.

**Chad's words, the spec for this arc (verbatim):** *"I just didnt want to be
upside down long as soon as inpu settles it should autorotate to wings
level."* And the complaints: *"autolevel shakes everything"*, *"Eagle cascade
is too pitchy"*, *"The camera feels laggy for the horizonal levelling"*, and
his suspicion *"I think they turned up my pitch gain."*

**⚠ Tree moved mid-review (M1):** a live engineer session landed an S61 block
in `BirdController` (dwellBoost / `aimApplied.rollBoost`; comments declaring
"rollRate restored to 3.2" and "bankKeyScale retired") while `GameConfig`
still carries `rollRate=8.4`, `bankKeyScale=0.40`, and no `dwellLevelRateMult`
— the boost is inert via `or 1` and the tree is HALF-LANDED and internally
inconsistent. Everything below reviews the S60 state, which is still the live
config. (M2: the S61 comment itself records the S60 package "flown REJECTED:
pitchy + wobble" — the rejection is already on the engineer's record.)

### Findings (both lenses, converged)

- **F1 — PITCH WAS NEVER TOUCHED.** `aimPitchGain 8.2`, `aimPitchDamp 0.64`,
  the pitch law, and profile pitchRate are byte-identical through the whole
  night. Chad's suspicion is factually cleared — and his felt report is
  STILL RIGHT: the pitchiness is real, manufactured by roll-side coupling.
  Three paths: (1) `aimBankFeedforward 0.35` (sec-bank pull) now arrives
  ~2.6× faster on big throws because the roll linear zone widened 13%→35%
  and real bank onset reaches 480 deg/s; (2) roll saturation moved to ~35%
  cursor offset while pitch still saturates at ~12% — a diagonal throw is
  now NOSE-FIRST where it was bank-first; (3) every 305 deg/s level-off
  removes the bankFF term (~0.35 of pitch command at 60° bank) in ~0.15 s —
  a nose-down step synchronized with every autolevel. This is the E2
  structural floor ("pull grows WITH the bank") amplified by the roll
  speedup — the fix belongs to E2, not to any pitch constant.
- **F2 — THE AUTOLEVEL IS NOT SEPARATED.** Every levelling author
  (`levelAssist`, `dwellTerm`, both damps) sums into the ONE clamped roll
  channel inside `computeMouseAim`; the dwell path's delivered rate depends
  on the cascade's `rollRate` and `aimRollDamp` by construction
  (`rate·rollRate/(1+aimRollDamp)`), which is exactly why raising the
  autolevel speed dragged the whole cascade with it. Chad's model (a
  separate assist that engages when input settles) is INVERTED in the code:
  with time=0.15 s the dwell path is the de-facto permanent levelling
  author in cruise, through a chattering crossfade.
- **F3 — THE WOBBLE, leading mechanism: GATE-CHATTER RELAY.** Gate 1 is a
  per-frame `|dx|+|dy| ≤ 1.5 px` stillness test; resting-hand tremor sits
  astride it. One over-threshold frame collapses `dwellRamp` to 0
  INSTANTLY; re-arm now takes only 0.15 s + 0.12 s ramp. At any bank >10°
  that is a 2–4 Hz near-full-stick roll square wave (tens of degrees of
  bank amplitude), which `bankTiltFactor 0.6` transmits straight to the
  camera and bankFF pumps into pitch — "shakes everything." At the old
  0.5 s this tremor cadence simply never armed. **0.15 s is what let the
  gate cycle.**
- **F4 — THE WOBBLE, second mechanism: LOOP MARGIN.** `dwellLevelRate 1.0`
  + `stopDeg 10` raise the level-off loop bandwidth ~12× past the
  UNMODELED `aimResponse = 7`/s ease (a ~143 ms lag inside the loop, 5×
  slower than the 28 ms plant pole) — phase margin ~5–10°, ringing at
  ~2.1 Hz with the damps live. Every ζ/overshoot number in the package was
  computed without this pole (and the package carries two different ζ
  claims, 0.68 and 0.54). The pre-S60 legacy dwell had ~45°+ margin.
- **F5 — THE CAP IS BYPASSED.** The v2 block recomputes `dwellTerm` AFTER
  the E1.2a headroom clamp: at full ramp |dwellTerm| = 1.0 >
  `dwellLevelTotalCap 0.8`. The "enforced by construction, EXACTLY, every
  frame" invariant is FALSE on the uniform path (i.e. always), and the S61
  comment inherits the same false claim. Comment-vs-code divergence,
  in-tree.
- **F6 — THE CAMERA CANNOT FOLLOW THE NEW LEVEL-OFF.** No camera code was
  touched (camera laws respected). But the horizon is served by two
  cascaded ~0.5 s lags — `horizonLevelRate 2.0` on `camUp`, and the E1.2b
  frameUp carry's hardcoded `2.0·dt·dwellRamp` on `levelRef` — sized for
  the 183 deg/s era. A 305 deg/s level-off finishes in ~0.15–0.2 s; the
  horizon trails it by ~1 s+. Aggravator: gate chatter freezes `frameUp`
  mid-carry, parking the horizon reference off-level between dwells. The
  camera constants are consumers of plant speed and were never in the
  compensation set. Any change here is a camera-laws question → Chad's
  word, CS-8 territory, never silent.
- **F7 — "exactly four consumers" is FALSE (minor).** Missed:
  `MEGA.updateDeflectionFeathers` (rollN normalizes by rollRate — feathers
  now ~never fire; cosmetic) and the CROW: `Controls` aim gains are GLOBAL,
  so the eagle-specific ×3.2/8.4 cut the crow's cascade authority to ~38%
  with zero compensation. Harmless while combat is shelved; must be
  re-derived before any 1-v-4 work (same class as the standing roll-
  asymmetry note).

### Governance (kernel-protector findings)

- **G1 — The dirty-build discipline broke.** Three-plus tunings were flown
  tonight under stamps differing only in TIMESTAMP (`11aee38-dirty …`) —
  the `-dirty` suffix has zero discriminating power (the S60 corrected
  nugget, already on this ledger), so no flight tonight can be paired with
  certainty to the config it flew. No-stamp-no-verdict is degraded from a
  hash check to a wall-clock check. Advisement: commit (or stamp a config
  hash) per iteration before the next verdict-bearing flight.
- **G2 — The E-series ruled order was jumped.** Standing order: E1 → E1
  re-capture (pre-registered: chaos population vanishes, floor persists) →
  E2 phased pull → lineHoldFF sweep. The re-capture was never run;
  instead the session did rollRate 4x + cascade recompensation + a
  structural level-off rewrite in one night. The pitchiness complaint is
  the E2 floor arriving on schedule, amplified — predicted by the board.
- **G3 — Nothing is on the CS registry** because nothing is committed; the
  same-day sync rule has nothing to bite on. The dwell family is CS-5
  amendment-pending context; the cascade constants are governed rows. The
  registry protection is OFFLINE while the work rides the working tree.
- **G4 — S61 direction note (no ruling implied):** the half-landed S61
  shape — rollRate back to 3.2, dwell path gets its OWN rate authority —
  is structurally the separation Chad asked for ("autorotate speed without
  touching the cascade"). Recorded here as an observation for the
  engineer's red team, not a design instruction from this ledger.

### Pre-registered discriminating probes (before any further tuning)

Written before the numbers exist, per doctrine:

- **P1 — one `[EvC shake]` line captured DURING the wobble.** If `ramp`
  prints strictly between 0 and 1 across rows → gate chatter (F3) is
  confirmed. If `ramp=1.00` steady → the servo-margin mechanism (F4)
  leads. (Caveat on old tapes: the tracker records the WINDOW MAX of ramp,
  so `ramp=1.00` rows never excluded chatter as co-cause.)
- **P2 — the hand-off probe:** bank ~40°, then take the hand FULLY OFF the
  mouse. Zero tremor → gates hold → if the shake vanishes hand-off but
  returns hand-resting, chatter is proven on the stick, no instrument
  needed.
- **P3 — the camera probe:** hand-off level-off from ~60°; time from
  bird-level to horizon-level. ~1 s+ → the cascaded camera lags (F6).
  ~0.1 s → F6 is wrong.
- **P4 — the pitch probe:** a pure-vertical cursor throw. Feels identical
  to pre-S60 → pitch untouched confirmed on the stick; the pitchy feel is
  diagonal-throw rebalance + bankFF timing (F1).

**Consult standing:** advisement only — no code was written, no live tree
touched. The engineer's own red team owns the fix design; this ledger owns
the record. Full agent reports retained in this session's transcript;
symbols cited in-entry are the durable pointers.

---

## 2026-08-01 — ADDENDUM: S61 (`85c93d2`) scored against the consult. The wobble SURVIVING S61 is the F4 prediction landing — the super-unity channel rides the SAME unmodeled ease

Chad reports the autolevel still wobbles on the S61 build (clean stamp
`85c93d2 2026-08-01 02:55` — G1 satisfied). The engineer's independent
red-team consult (recorded in `85c93d2`'s message) had not seen this
ledger's entry; the two consults converged on most findings independently.
Scorecard of S61 against F1–F7/G1–G4:

**Fixed by S61:** F2 (true decouple — `dwellLevelRateMult 2.25` super-unity
channel, cascade knobs byte-restored to flown 3.2/7.5/0.9/0.45; the rollRate
tombstone is written); F1's root (pitch harmony restored with the cascade);
F7 (crow regression undone; deflection feathers re-normalized implicitly;
`bankKeyScale` retired); G1 (committed, clean stamp); G2 partially (the
detour is over); F3 partially (`dwellLevelTime 0.25` kills the
decel-tail arming — the engineer's own diagnosis — but the per-frame 1.5 px
gate + instant one-directional ramp collapse remain, so resting-tremor
chatter is reduced, not excluded).

**NOT fixed — and now the leading suspect for the surviving wobble:**

- **F4 — THE UNMODELED EASE POLE, verbatim in the S61 diff:** the rollBoost
  channel deliberately rides *"the same resp ease as the axes it shares a
  frame with"* — `aimResponse = 7`/s, a ~143 ms first-order lag INSIDE the
  level-off loop, 5× slower than the 28 ms plant pole. S61 loop gain in the
  landing band: `mult·rollRate/(rad(stopDeg)·(1+aimRollDamp))` =
  2.25·3.2/(0.209·1.58) ≈ **22/s** — same marginal structure as S60's
  ~30/s, crossover still ~2× past the ease pole, phase margin still thin,
  predicted ring ~2 Hz. The S61 stability claim ("coast 8.6° < 12° →
  single crossing, no limit cycle") repeats the S60 analytical omission:
  computed with the plant pole only, ease pole absent. Every generation of
  this loop has now been sized without its slowest pole.
- **F5 — the cap bypass stands:** uniform-path `dwellTerm` (rate 1.0) still
  overwrites the E1.2a headroom clamp; the S61 comment's claim that the
  boost "inherits … headroom cap" inherits the falsehood.
- **F6 — camera untouched:** both ~0.5 s horizon lags (`horizonLevelRate`,
  the frameUp-carry literal) still face a ~306 deg/s level-off — the laggy
  horizontal levelling will persist on S61 as-is.

**Discriminating read, pre-registered (P1/P2 now sharpened for S61):** if
the wobble occurs with the hand FULLY OFF the mouse (P2), chatter is
excluded and F4 is confirmed on the stick alone — tremor is required for
chatter, not for a marginal servo. On the [EvC shake] line: steady
`ramp=1.00` + `keys=0%` + f~2 Hz = F4; `ramp` strictly inside (0,1) across
rows = residual chatter (F3).

**Advisement (mechanism named, design left to the engineer):** the lever
families that restore margin are (a) take the ~143 ms ease OUT of the dwell
loop — the boost channel bypasses `resp` (its own faster ease or none; the
28 ms plant then dominates and 22/s gain is comfortable), or (b) lower the
loop gain into the existing lag — wider `stopDeg` / lower `mult` (costs
Chad's ruled speed). Family (a) preserves the ~306 deg/s ask. Whichever is
chosen: size it WITH the ease pole in the model this time, and state the
predicted crossover and margin in the card before it flies.

---

## 2026-08-01 — ENGINEER ACCEPTS F4; S62 motion: the ENTIRE dwell command leaves the aimResponse ease (not just the excess)

The engineer's reply, on reading the consult + addendum (recorded here per
the consult pattern; design his, verbatim substance): the S61 seam is ruled
a structural flaw by its author — the boost channel rode the 143 ms
`aimResponse` ease, so the fast servo commanded through a slow filter.
Motion, to land BEFORE the probe flight so the probe tests the corrected
structure: the WHOLE dwell command bypasses the ease — not just the
super-unity excess, since the inner ±1 slice would still ring through the
lag. The dwell path keeps its own smoothing (the 0.15 s ramp-in; the
one-directional instant collapse stays, by design) so engagement never
arrives as a snap.

**Docs-agent watch-items attached to the motion (advisement, pre-flight):**

- **W1 — chatter is now unfiltered.** The 143 ms ease was incidentally
  masking gate chatter; outside it, a surviving 1.5 px gate flicker reaches
  the plant with only the 28 ms lag — sharper per event. This makes P1/P2
  discrimination cleaner, and makes any residual F3 chatter feel HARDER,
  not softer. P2 (hand-off probe) remains the decisive read on the new
  build.
- **W2 — state the worst-frame authority in the card.** With all of
  `dwellTerm` outside the aim ±1 mix, the worst-frame total roll command is
  `rollP(±1) + mult·dwellTerm`, gated but no longer clamp-bounded; F5 (the
  E1.2a totalCap comment claiming "enforced exactly, every frame") is
  still-open comment-vs-code divergence. The fly card should state the
  intended ceiling explicitly rather than inherit the stale claim.

Probe protocol unchanged: P2 hand-off level-off from ~40°, then P1 shake
line if anything survives. If the corrected structure flies clean hand-off
AND hand-resting, F4 closes as the wobble's cause on this ledger.

---

## 2026-08-01 — S61b (`4814e61`) FLOWN, SHAKE SURVIVES. F8 opened: the defiltered dwell loop is UNDAMPED BY DESIGN. Verdict held for the probe reads

**Chad's report (verbatim): "It still shakes after this."** Build verified:
`4814e61` committed + pushed, stamp `4814e61 2026-08-01 03:08`, the defilter
is real and complete on inspection (dwellTerm zeroed out of the filtered
channel; rollBoost applied direct; degenerate-viewport abort zeroes the
boost; no screenshots from this flight are on disk — the log*.png set is
all 2026-07-30). **The probe discrimination is NOT yet on record** — no
hand-off result, no [EvC shake] line, no stamp word from the cockpit. Per
no-stamp-no-verdict and the re-instrument SOP, no dial moves until those
exist. But the S61b diff itself yields a new structural finding:

**F8 — the dwell loop now has NO effective rate damping.** Pre-S61b the
dwell command and the damps shared the filtered channel (consistently
laggy — F4's ring). S61b moved the command OUT and left `aimRollDamp` /
`aimHeadDamp` IN: the loop's only damping is now the 28 ms plant lag plus a
rate term that arrives through the 143 ms filter — at the new loop's
natural frequency (ωn ≈ 35 rad/s ≈ 5.6 Hz from K = mult·rollRate/stopRad =
34.4/s over the plant pole) that filtered term is attenuated ~5× AND
phase-lagged ~79°, i.e. nearly in quadrature — it is not damping there,
and the commit's "only adds margin" claim is wrong in general (small
magnitude ≈ 0.11 keeps it secondary, not stabilizing). The engineer's own
math concedes the point: ζ = 0.51 is the PLANT-LAG-ONLY figure — an
underdamped 5–6 Hz mode with ~15% overshoot per crossing. And because the
dwell gates open in ordinary aimed cruise (the block's own comment), this
underdamped 2.25-authority wing-leveler is live nearly ALL the time — a
stiff spring with no damper, holding wings level, waiting to be excited.

**For a SUSTAINED shake, ζ=0.51 needs an energy source. Two candidates,
both already on this ledger:** (1) **residual gate chatter (F3), now
unfiltered (W1 landing)** — each 1.5 px tremor frame now delivers an
instant 2.25-authority step with only 28 ms smoothing; (2) **camera-coupled
re-excitation** — `rollP` stays live during dwell; the bird's roll moves
the camera (bankTiltFactor through ~93 ms followSmoothing), which moves the
cursor's world ray, which changes hCmd — an outer feedback loop through the
camera that the stiffened dwell loop can now hunt against.

**The three reads that discriminate (before ANY dial):**
- **R-a — stamp word:** `4814e61 2026-08-01 03:08` seen in the corner label.
- **R-b — P2 hand-off:** bank ~40°, hand FULLY off. Shakes hand-off too →
  F8 family (arrival ring / camera-coupled hunt); clean hand-off, shakes
  hand-resting → F3 chatter, and the dial is the quantum/gate decay, not
  any gain.
- **R-c — the [EvC shake] line + WHEN:** does the shake live at the
  level-off ARRIVAL (brief ring, ~5–6 Hz = F8 arrival mode) or run
  CONTINUOUSLY while flying level (chatter or camera-coupled hunt)? A
  SILENT console with felt shake = the oscillation is not in rollVel —
  look at camAmp/the camera loop.

**Advisement for the engineer (mechanism named, design his):** if hand-off
still rings, the lever family is giving the dwell channel its OWN direct
(unfiltered) rate-feedback term — e.g. a dwell-scoped rollVel damp applied
on the rollBoost channel itself — restoring ζ without re-introducing the
lag. That is the half of the S61b move that didn't ship: the command left
the filter; its damping never followed. Size it with BOTH poles in the
model, per the standing rule he just wrote to memory.

---

## 2026-08-01 — R-b/R-c READ IN: F9, THE SELF-BREAKING GATES. The verb's own motion kills it mid-roll; each kill is now an unfiltered step. Chad's original words were the spec

**Chad's probe report (verbatim): "I shakes a couple of times right away and
half way through and a bit as it settles."** Stamp confirmed on his word
(R-a: `4814e61 2026-08-01 03:08`). The cadence — DISCRETE shakes at onset,
mid-motion, and arrival, not a continuous buzz — is the fingerprint of a
relay, and the committed code confirms the relay structurally:

**F9 — the dwell verb's arming gates keep judging it while it executes, and
its own motion breaks them.** `computeMouseAim`: the gate-3 hysteresis
(`absAz > 10 or absElev > 20` → `dwellGateOpen = false`) evaluates every
frame; ANY gate break zeroes `dwellT` AND `dwellRamp` the same frame
(instant one-directional collapse); re-arm = 0.25 s + 0.15 s ramp. But a
level-off from a COORDINATED bank swings the nose (rolling out of a turn
changes heading), and the E1.2b frameUp carry simultaneously rotates the
camera reference the cursor ray hangs on — so the verb's own action drives
`absAz` past the 10° break. Cycle: fire → self-break → instant cut →
re-settle → re-arm ~0.4 s → fire again. A ~40° level-off yields 2–3 cycles:
exactly Chad's felt count and placement. This was flagged as a chatter
source in the original consult (gate-3 clause of F3); S61b's defilter (W1
landing) turned each cut from a softened dip into a sharp unfiltered step,
which is why S60→S61→S61b all shook DIFFERENTLY but none cleanly — three
mechanisms (F4 filter ring, F8 undamped margin, F9 gate relay) stacked, and
each fix peeled one layer and sharpened the next. The residual "a bit as it
settles" is consistent with the F8 arrival ring (ζ≈0.51, plant-lag-only).

**Found in the committed tree:** the engineer already shipped
`dwellLevelDamp` — a dwell-scoped DIRECT rate damp on the rollBoost channel
(`dwellBoost -= dd·rollRateN·dwellRamp`), currently **0 = inert**. That is
the F8 lever, built and waiting; it does not address F9.

**The spec, which was on this ledger from the first entry — Chad verbatim:
"as soon as INPUT settles it should autorotate to wings level."** The gates
judge AIM GEOMETRY, which the verb's own roll perturbs; the spec says
HANDS. Advisement (mechanism named, design the engineer's): once armed and
executing, the verb's KILL conditions should be actual pilot input —
mouse delta past the quantum, any key (CS-1 preserved), free-look — never
the aim-resolution geometry; gate-3 belongs at ARMING only (or frozen /
latched while the verb runs, releasing at level or on real input).
Pre-registered check for the fix flight: a hand-off ~40° level-off must
show ZERO mid-motion collapses ([EvC shake] silent or ramp monotone 0→1→
level), and the arrival ring, if felt, is F9-independent — the inert
`dwellLevelDamp` is its dial, one change, its own flight.

---

## 2026-08-01 — S61c (`f136f83`) FLOWN: the bird is damped, the front moves to THE CAMERA (F6 unparked on Chad's word). Two Chad rulings captured verbatim

**Build:** stamp confirmed on Chad's word (`f136f83 2026-08-01 03:19`) —
S61c armed the pre-registered damper pair (`dwellLevelDamp 0.75` +
`dwellLevelRateMult 3.50`, steady ~306 deg/s unchanged, in-band ζ≈0.71,
damper unfiltered AND unclamped — the S61c commit correctly found the
cascade's damp signal was both behind the filter and PINNED at ±1 through
the boosted roll, i.e. zero rate feedback in the ringing regime).

**Chad's report (verbatim):** *"camera shake seems like the horizon is
lagging it. Also the roll of the autolevel is took hard. still the duration
on the shake is the duration of the bank to level, seems the wings are
forcing the camera. I think somewhere the orientation autolevel camera got
loose and dependent on the birds wind level to elvel itself , I noticed the
problem earlier come on where the orientation would be off a moment until
the autolevel horizon caught up. Maybe not its just hard for it to catch up
but the horizon should always stay level."*

**Reading:** the discrete jolt cadence is GONE — the character changed from
relay cuts to continuous, roll-duration, camera-flavored shake. The
bird-side stack (F4 filter ring → F8 missing damper) is quiet; S61c's own
pre-registration names the survivor and Chad's felt read agrees:
**F6, the camera.** His diagnosis is structurally accurate: while
aim-driving, the camera's level reference is NOT the world — `levelRef` is
`aimCursor.frameUp`, a carried frame the E1.2b dwell carry re-levels at a
hardcoded `2.0·dt·dwellRamp`, consumed by `camUp` relaxing at
`horizonLevelRate 2.0`/s — two cascaded ~0.5 s lags chained to the bird's
frame, while `bankTiltFactor 0.6` rolls the camera with the wings through
the fast follow path. The camera's horizon is literally "dependent on the
bird's wing level to level itself." At 306 deg/s the wings outrun it for
the whole motion: "the wings are forcing the camera."

**RULING 1 (Chad, verbatim, the word the parked item waited for): "the
horizon should always stay level."** F6 UNPARKS. This is camera-laws
territory (CS-2/CS-8 process; the shared-kernel camera-state law is
ratified — amendments on Chad's word, never silent). Note for the
engineer's design: the ruling as spoken is stronger than a rate bump — it
says the horizon reference should be WORLD-anchored (never a carried
bird-coupled frame), which reaches the frameUp-as-levelRef chain and
touches `bankTiltFactor` (any deliberate camera roll-with-bank must now be
justified against "always level" or ruled a character exception by Chad).
The previously identified first dial (frameUp carry rate, Controls-scope,
before anything in the camera module) is the conservative rung; the ruling
may retire the reference outright — engineer's design, Chad's law,
CS-amendment on record either way. **Numeric pass pre-registered:** on a
hand-off level-off the [EvC shake] signature of a working fix is `bankAmp`
LARGE while `camAmp` ~0 — the tracker's own camera-artifact discriminator,
run in reverse.

**RULING 2 (Chad, verbatim): "the roll of the autolevel is took hard."**
The ~306 deg/s + 0.15 s ramp is ruled too violent as felt. The speed dial
is `dwellLevelRateMult` — ⚠ now one half of the S61c COUPLED PAIR: any
speed change re-solves the pair arithmetic (steady rate =
(mult − aimRollDamp − dwellLevelDamp·x̂)·rollRate), never moves one knob
alone. Chad's number pending in cockpit terms (time-to-level), asked of
him directly. `dwellLevelRampS` is the harshness-of-onset lever if the
complaint is the GRAB rather than the speed.

**Still open, latent:** F9 (self-breaking gates) was NOT addressed by
S61c — the engineer read the earlier cadence as pure servo and the damper
evidently quieted what was felt, but the gate-3 mid-verb kill structure is
unchanged in code. If discrete mid-roll cuts ever return on a tape
(`ramp` flicker 0↔1), F9 is the standing suspect; it should not be
re-diagnosed from scratch.

---

## 2026-08-01 — CONSULT REPLY (to `docs/CONSULT-S61-SHAKE-PACKET.md` @ `a139c6b`): the open question is ANSWERED — shake SURVIVES `f136f83`, and Chad's felt read lands on the packet's own last suspect. Two Chad rulings delivered verbatim, one is THE WORD the camera work was parked on

**Answer to the packet's open question:** Chad flew the `f136f83` build
(stamp confirmed on his word: `f136f83 2026-08-01 03:19`) and reported to
this agent BEFORE the packet landed. The shake survives. His words,
verbatim: *"camera shake seems like the horizon is lagging it. Also the
roll of the autolevel is took hard. still the duration on the shake is the
duration of the bank to level, seems the wings are forcing the camera. I
think somewhere the orientation autolevel camera got loose and dependent on
the birds wind level to elvel itself , I noticed the problem earlier come
on where the orientation would be off a moment until the autolevel horizon
caught up. Maybe not its just hard for it to catch up but the horizon
should always stay level."*

**Attribution (checked against all six tombstones — re-proposes none):
AGREES with the packet's remaining pre-registered suspect — the camera —
with one refinement to red-team.** The discrete jolt cadence is gone
(S61c's damper did its job on the bird); what survives is continuous,
roll-duration, camera-flavored. Within the camera family there are two
distinct mechanisms and Chad's report doesn't yet discriminate:

- **(i) DISPLAY artifact — the split horizon.** The camera renders TWO
  horizon cues at different speeds: `bankTiltFactor 0.6` rolls the view
  with the wings through the fast follow path (~0.1 s), while the true
  horizon reference crawls behind two chained ~0.5 s smoothers
  (`camUp` ← `horizonLevelRate 2.0` ← `frameUp` carry literal 2.0) — and
  the reference itself is the CARRIED, bird-coupled `frameUp`, not the
  world. At 306 deg/s the wings outrun the reference for the whole
  motion: "the wings are forcing the camera," shake felt, bird clean.
  Chad's structural diagnosis is accurate as read from code: while
  aim-driving, the camera's level is literally "dependent on the bird's
  wing level to level itself."
- **(ii) FEEDBACK loop — camera→cursor→cascade** (the packet's framing).
  Note hands-off does NOT exclude it: the hand is still but the camera
  tilting moves the world point under the stationary screen cursor, so
  the aim error changes with zero hand input.

**The packet's own discriminator is the next read, still outstanding from
the cockpit:** during the camera shake, was the `[EvC shake]` console line
printing? SILENT + felt shake = (i), camera-only, bird clean. Printed with
`camAmp` hot / `bankAmp` cold = camera confirmed twice over; `bankAmp` hot
too = (ii), the loop is moving the bird. One screenshot settles it.

**RULING 1 (Chad, verbatim — THE WORD the camera work was parked on): "the
horizon should always stay level."** Delivered here per the consult
pattern; the CS-2/CS-8 amendment process is the engineer's motion. Design
note for the red team: as spoken, the ruling is stronger than a rate bump —
it says the horizon reference is WORLD-anchored, never a carried
bird-coupled frame — which reaches the `frameUp`-as-`levelRef` chain and
puts `bankTiltFactor` itself in question (any deliberate roll-with-bank
must now be justified against "always level" or ruled a character
exception by Chad). The conservative first rung previously identified
(frameUp carry rate, Controls-scope) may under-shoot the ruling as given.
**Numeric pass pre-registered for the eventual fix flight:** hand-off
level-off shows `bankAmp` LARGE while `camAmp` ~0 — the tracker's
camera-artifact discriminator run in reverse.

**RULING 2 (Chad, verbatim): "the roll of the autolevel is took hard."**
The ~306 deg/s + 0.15 s ramp is too violent as felt. ⚠ The speed dial
(`dwellLevelRateMult`) is now HALF of the S61c coupled pair — any speed
change re-solves the pair (steady x from mult − aimRollDamp −
dwellLevelDamp·x), never one knob alone. Chad's number pending in
time-to-level terms (being asked directly); `dwellLevelRampS` is the
onset-grab lever if the complaint is the catch, not the speed.

**Latent, for the record (not the current attribution):** F9 — the gate-3
aim-geometry hysteresis still kill-judges the executing verb in code
(`dwellGateOpen` break → same-frame ramp collapse). Not indicated on this
flight's felt read; if discrete mid-roll cuts ever return on a tape (ramp
flicker 0↔1), it is the standing suspect, pre-diagnosed — never re-derive.
Also still open, minor: F5 (the E1.2a totalCap comment's "enforced
exactly" claim is stale on the uniform path — a docs fix, not a behavior
one).

---

## S61 HORIZON LAW — engineer motion on RULING 1 (eagle repo, red-team CLEAR w/ riders)

**Engineer entry (2026-08-01, follows 788a429).** RULING 1 executed as a camera-laws
amendment: `Camera.worldLevelHorizon=true` — the chase camera's self-level REFERENCE is
world-level ALWAYS; the Stage-C C2 camera-consumer swap (levelRef = the carried frameUp
while aim drives) is superseded. C1 (aimOwnFrame steering basis) and the E1.2b dwell
carry untouched. CS-8 amended in docs/HANDOFF.md; ratified as LAW 5 in
docs/CAMERA-LAWS.md. Revert = flag false (byte-identical C2 path).

**Red-team correction adopted, on the record (F1):** under shipped flags there is NO
camera-to-cursor feedback loop — Stage C C1 cut every camera read out of the aim law
(swing basis 3731-3790 reads no camera axis; the camCF clamp at 3837 is unreachable
with aimFreeCursor=true; aimHeadingLag/chaseDir are camera-reads-aim, one-way;
WorldToViewportPoint sites are HUD-only). The docs-agent split therefore resolves to:
display problem = REAL (two mechanisms: the C2 levelRef chain, now fixed by LAW 5; and
bankTiltFactor 0.6 rolling the camera with the bird — at the 306 deg/s level-off that is
~184 deg/s of camera roll, faithfully displaying every servo wiggle); feedback loop =
STRUCTURALLY ABSENT. The hands-off caution stands acknowledged but moot for this build.

**Pre-registered next rung if shake survives LAW 5 flown:** bankTiltFactor — noting it
is a RATIFIED LOVED decoration (CAMERA-LAWS clarifications: "Do not delete bank tilt
under Law 4"), so it moves only on Chad's explicit ruling that "always level" covers
bank tilt too. The [EvC shake] numeric pass from 788a429 stands: hands-off level-off
with bankAmp LARGE / camAmp ~0 = the camera fix working.

**RULING 2 staged:** pre-solved mult table filed in GameConfig (0.25 s -> 2.30,
0.33 s -> 1.88, 0.40 s -> 1.65 for a 45-deg bank; damp 0.75 fixed; ramp is the lever if
the complaint is onset grab). Flip on Chad's number; the pair discipline holds.

**F5 closed:** the E1.2a totalCap "enforced exactly" comment was corrected in
GameConfig during S61 (legacy-path-only caveat added). F9 stands latent as filed.

**RULINGS EXECUTED (engineer, eagle commit 64cb275, same night):** Chad,
verbatim: "0.33s. remove bank tilt! ty". (1) `dwellLevelRateMult` 3.50 -> 1.88
from the pre-registered table (45 deg in ~0.33 s = 136 deg/s; damp 0.75
untouched, pair discipline held). (2) `bankTiltFactor` 0.6 -> 0 -- the owner
ruled "always level" covers the decoration; the CAMERA-LAWS "loved, do not
delete" clarification is superseded on his word and rewritten (zeroed not
deleted, revert = 0.6). With LAW 5 + tilt 0, the camera rolls with the bird by
NO mechanism -- the camera-shake suspect list is exhausted by construction.
Flight verdict on the full S61 stack pends on stamp 64cb275.

---

## 2026-08-01 — WOBBLE SAGA TAIL + NEW THREAD: THE BROAD KNIFE (Chad ask, verbatim) — LINEAGE ANSWER DELIVERED: the plane solved this exact complaint three times; port the flown mechanism, do not invent

**Saga tail:** the engineer's build `64cb275 2026-08-01 03:45` landed both
rulings (autolevel at Chad's pace — 45° levels in ~0.33 s / 136 deg/s from
the pre-registered table, damper kept; `bankTiltFactor` ZEROED not deleted
under the new Law 5 — horizon world-level "by no mechanism at all";
engineer's ledger entry `0ad86ef` — ON THIS LEDGER; correction, this entry
first said "in the eagle repo": the engineer now writes
`DECISIONS-evc-eagle.md` directly, see also `3960d55`). Chad's response
opening the next ask: **"ookay perfect"** — reads as acceptance; the formal
KEEP log on the one-flight verdict is the engineer's motion on Chad's
confirmed word.

**NEW THREAD — Chad's ask (verbatim):** *"Th knife edge cone is too sharp
sending me into a roll when I only want to pitch down for a dive. My seads
codebase had this problem. We altered the deflection zone below something
like 34 - 40 degrees, to the side without the wings rolling over when its a
mostly straight down mouse aim movement. So to allow me to dive straight
down given me choice control of the inversion if I chose to. So a broader
knife is required."*

**LINEAGE ANSWER (archive-first, per standing doctrine — sources:
`docs/cascade/push-gate-knife-edge.md`, `reference/seads-feel/config/
controller.toml` [push] block, snapshot current at the v12 seal — live
branch verified unchanged at `e362df289`):**

Chad's memory is accurate to the half-degree: the plane's shipped
**side-cone is 37.5° enter / 42.5° exit** — his "34–40." And the plane's
ledger carries his SAME complaint, three flown rulings deep:

1. **2026-07-07** (side cone 22.5→27.5): *"the knife edge on nose down
   maneuvers needs a little widening... I got thrown over once when nosing
   down."*
2. **2026-07-24 FLY-1** (27.5→37.5): *"when I go nose down that knife edge
   is strong and I roll over to one side or the other. Narrow the rollover
   bands to give way to the sacred middle... broaden that knife edge."*
3. **Rung F, THE SACRED MIDDLE** (side_pure 18/23): *"when I nose straight
   down I need a wider knife edge... pitch straight down without tipping
   over... maintain my horizon because I will slowly pitch up from the dive
   in that same direction."*

**The mechanism (the eagle has NONE of it — `push-gate-knife-edge.md`'s
own last line: no equivalent gate exists in the EvC kernel; its knife-edge
behavior is emergent from `aimRollGain` on any lateral component of a
steep-down aim):** a hysteretic push-mode state machine confining PURE
PITCH-DOWN (roll suppressed) to a cone around the vessel's own vertical
plane, with four legs, plane-shipped values:

- **World-horizon leg** (rung E): push only when the aim is ≥
  `horizon_enter 45°` BELOW the WORLD horizon (exit 40°, 5° band). The
  elevation MUST be world-frame (`dot(aim_world, local_up)`),
  bank-independent — ⚠ eagle-specific hazard: the eagle's measured dip
  attribution was camera-basis contamination; a camera- or body-frame
  elevation here recreates the plane's original "mystery dive" (side cone
  DEGENERATES near-astern — documented in the toml comment).
- **Side cone**: pure-pitch only within `side_cone_enter 37.5°` of the
  vertical plane (exit 42.5°); outside it, roll to track — Chad's "choice
  control of the inversion": a down-AND-side aim still rolls through.
- **Sacred middle** (rung F): within `side_pure_enter 18°` in-plane (exit
  23°), below the horizon at ANY depth → pure-pitch — the shallow
  straight-ahead dive that rung E's 45° blanket alone would deny. Strict
  subset: a shallow LATERAL aim still never pushes.
- **Turnover leg**: push while the below-nose angle ≤ `down_enter 88°`,
  roll into the loop past `down_exit 96°` (turnover ~92°, just past
  straight down) — "THE knob for nose down without banking over," and the
  literal implementation of inversion-by-choice.

**Port lessons attached (paid for, both kernels):** every leg HYSTERETIC
with ~5° bands — the eagle has just paid twice for non-hysteretic
self-judging gates (F3/F9); the companion display (off-screen red aim
arrow) made the 45° gate legible on the plane — the eagle equivalent is
the engineer's call; the reticle is never clamped (S-retclamp lesson).
**Governance:** this IS a Chad-initiated cascade change (his word above,
scope: steep-aim region only) — the S61 packet's "zero change to the flown
cascade feel" stands everywhere else. **Pre-registered pass, from the
plane's own measured check:** a pure lateral hold produces zero push on
every row; a straight-down aim pure-pitches with no roll; down-and-side
still rolls through past inverted. Constants are the plane's as-shipped —
the eagle's own values are Chad's stick after the port flies, recorded as
character-sheet entries per vessel doctrine.

**BROAD KNIFE PORTED (engineer, eagle commit e9114ac, stamp 04:22):** the
plane four-leg push-gate lands on the eagle, constants as-shipped (horizon
45/40 WORLD-frame, side 37.5/42.5, sacred 18/23, turnover 88/96, bank guard
100/120), one seam (hCmd multiply, pushW ramp in-24/out-6). Architect design
+ red-team BLOCK-then-CLEAR; fixes folded: F1 dwell verticality fade (the
vertical-stoop phi noise snap-roll -- NOT deferred, the stoop is the home
state), F2 fresh-latch-vs-exit + asymmetric ramp (no dive-entry roll kick),
F3 [EvC push] transition log + push column on the shake line (chatter
pre-instrumented; side cone is a self-judging gate, exit action can re-cause
entry -- pre-diagnosed fix on file), F4 sacred = side-angle AND
below-horizon, F5 honest: levelAssist self-fades steep, push HOLDS bank
there, dwell levels the shallow regime, F6 gap-reset clears. CS-8
amendment-pending row filed in HANDOFF; crow shared-Controls collateral
noted latent. Red-team confirmed hCmd has exactly three consumers -- the
one-seam claim is exhaustive, not asserted. Ships aimPushMode=true on
Chad ask; pre-registered pass = the plane measured check mapped (6 rows in
the commit). Cone widths become Chad character-sheet values after the fly.

---

## 2026-08-01 — BROAD-KNIFE BUG (Chad, flown on `e9114ac 04:22`): push fires on a hard LATERAL REVERSAL while banked. Attribution: the port dropped the plane's ASTERN EXCLUSION — the near-astern degeneracy reaches push through the sacred-middle arm + a signed turnover leg

**Chad's report (verbatim):** *"hard sideways drag one lateral direction
then the other treats down as relative and it maintains its banked
orientation pitching down to go the other lateral direction horizontall
across my screen with no banking, but at teh wrong time / direction."*

**Attribution (from the `e9114ac` code + the plane's cascade doc):** the
port's geometry IS world-frame (the pre-flagged hazard was avoided) — the
hole is different and also documented on the plane. During a hard lateral
reversal the aim sweeps NEAR-ASTERN of the nose, where the side-cone
measurement DEGENERATES (the vertical plane contains the astern direction:
at azimuth-off-nose 180±18°, `sideDeg = asin(|aim·n|)` reads ≤ 18° — the
toml comment's own degeneracy, the one `horizon_enter=45` was built to
blanket). Three port details then line up:

1. **Sacred-middle arm needs only ANY depth below horizon**
   (`pushSac = sideDeg ≤ 18/23 and elevDownDeg > 0`) — correct per rung F,
   but it bypasses the 45° horizon leg, so the astern degeneracy reaches
   push with a one-degree dip.
2. **The turnover leg is SIGNED:** `pitchDownDeg = atan2(aim·dvec,
   aim·birdLook)` — a near-astern aim slightly HIGH of the nose-plane
   (easy with the nose a little low in the banked turn) reads ≈ −177°,
   which passes `≤ 96` through the sign. The plane's `down_enter/exit`
   is a below-nose MAGNITUDE; astern reads ~180 and is denied.
3. **The plane's astern exclusion was dropped in port:** the plane's enter
   conjuncts include `target_body.z <= push_down_z_enter` (cascade doc
   `push-gate-knife-edge.md`, Math; `params.h push_down_z_enter/exit`) — a
   BODY-frame forward bound that denies astern aims outright, independent
   of the degenerate side read. The eagle port has hz/sac/side/turnover/
   bank-guard legs and no forward bound.

Result: push engages mid-reversal while banked (bank guard passes below
100–120°), `hCmd → 0` kills rollP AND coordinated yaw ("no banking"), and
the elevator chases the reversed aim in the BIRD'S banked frame — pitching
"down"-in-body = horizontally across the screen: Chad's report, mechanism
for mechanism, including "treats down as relative."

**Pre-registered confirmation (the port ships its own instrument):** the
`[EvC push]` transition log. Prediction for this bug's ENTER line:
`hz=false sac=true side=true(<18) turn=true(NEGATIVE large, ~ -150..-179)
bank=true(large) elevDn=small(0-10)`. A line matching that shape confirms;
`turn` printing a large negative number is the signed-leg smoking gun.

**Advisement (fix families named, design the engineer's; both deny astern
independently, plane-precedented):** (a) restore the forward bound — the
plane's `push_down_z` conjunct, e.g. require `aim·birdLook` above a
threshold (aim in the nose's forward hemisphere with margin), hysteretic
like every other leg; (b) take the turnover leg on MAGNITUDE
(`|pitchDownDeg| ≤ enter/exit`), matching the plane's below-nose-angle
semantics. (a) is the missing conjunct proper; (b) closes the signed back
door even alone. Neither touches the flown dive/sacred-middle behavior —
astern was never push-eligible on the plane.

---

## 2026-08-01 — BROAD-KNIFE BUG: ATTRIBUTION CONFIRMED BY THE AUTHOR, both plane-shipped fixes going in

The engineer confirmed from the code before touching it — attribution
exact: the ported turnover check takes the SIGNED angle (astern ≈ −170°
passes ≤ 96) and astern is in-plane by construction (side cone blind
there). Both fixes land together, both hysteretic: **(a) the forward
bound** — the plane's fifth conjunct that never made the trip — and
**(b) turnover as a magnitude.**

**Pre-registered pass for the fix flight (three moves, one flight):**
1. The bug maneuver — hard lateral drag one way, then hard the other,
   banked — produces ZERO `[EvC push]` ENTER lines and the bird ROLLS to
   the reversed aim (no sideways pitch, no held bank).
2. The flown-approved dive set is UNCHANGED: straight-down aim
   pure-pitches; shallow in-plane dive (sacred middle) pure-pitches; a
   down-AND-side aim rolls through — inversion by choice.
3. Lateral protection holds: a hard sideways drag at the horizon never
   noses down (rung E's original pass, re-run on the eagle).
Commit hash + stamp word before verdict, per standing discipline.

**ASTERN FIX SHIPPED (engineer, eagle commit 8610740, stamp 04:34):** both
scoped fixes landed exactly as filed (1788652): turnover now a MAGNITUDE
(signed -170 could pass "<=96"; astern now ~180, denied by the leg itself)
AND the plane fifth conjunct restored as a hysteretic forward bound
(aimPushFwdDeg 87 / exit 96, full 3D off-nose angle -- the side cone is
blind astern by construction, so the in-plane legs can never police that
region alone). [EvC push] log gains fwd=(off-nose deg). No felt change to
the dive set. The three-move fix-flight pass stands as pre-registered; on
all-pass the broad-knife thread closes flown-approved. Noted for both
kernels: the astern exclusion has now earned load-bearing-conjunct status
twice -- once designed on the plane, once by its absence on the eagle.

---

## 2026-08-01 — ARCHIVE CONSULT (inline, token-lean): astern fix flight — core passes; the surviving oscillation/curl is ONE of two KNOWN things, and the shipped [EvC push] log discriminates in one flight

**Chad's report on `8610740` (verbatim):** *"stamp correct , straight down
dosent roll, straight sideways wont pitch down but there is osscilations
and pulling toward the middle z direction like it wants to stop turning go
up then a big curl. smae for the down direction."* Moves 2–3 of the
pre-registered pass: PASSING. The oscillation/pull/curl on sustained hard
holds is the open item. Archive says it is one of exactly two things:

**Hypothesis A — push-gate leg chatter (NEW, would be `8610740`'s own).**
A hard lateral/down hold parks the aim near leg boundaries (the forward
bound 87/96 and turnover 88/96 both sit astride the ~90° off-nose geometry
of exactly these holds), and the port's red-team F3 pre-registered that the
side cone is a self-judging gate. If pushW cycles: `hCmd` cuts (turn
stops), the port's known F5 seam UN-gates levelAssist (roll toward level +
bankFF pull = "go up"), then the leg exits and roll slams back = "a big
curl." **Signature: `[EvC push]` ENTER/EXIT lines spamming (≥3/s) during
the hold.**

**Hypothesis B — the E2 STRUCTURAL FLOOR, already measured, fix already
ruled (NOT new).** If the console is SILENT, push never engaged and the
felt package is the eagle's documented cascade floor arriving at the front
of the stage now that everything above it is fixed: the S59 throw capture's
"pull grows WITH the bank" (elevator rails on hard throws — the pull toward
the middle and up), plus the three-co-owner curl partition (stage D
capture). The ruled E-series order (E1 → E1 re-capture → E2 phased pull)
was interrupted by the wobble arc; E1 is now settled and flown — **B means
the board simply resumes: E1 re-capture on the standing throw instrument
(pre-registered: chaos population vanishes, floor persists), then E2, the
designed fix for exactly this feel.** Nothing new to design; do not open a
new thread for a pre-measured floor.

**One-flight read:** repeat the hard lateral hold, watch the console.
Spamming push lines = A (engineer damps/widens the offending leg's band —
the printed line names WHICH leg flips). Silent = B (log it, resume the
E-series per the standing ruling). Both paths are fully pre-registered;
no further consult needed to proceed on either.

---

## 2026-08-01 — ARCHIVE ANSWER (sacred-middle elevation hysteresis): the plane's term is BARE TOO — its protection is a SIXTH conjunct the port also dropped: the BANK-ERROR FLOOR

Read from `reference/seads-feel/control/controller.cpp` (sacred-middle
block, ~line 644, snapshot current at v12):

1. **The plane ships NO elevation band on the sacred arm.**
   `sacred_middle_enter = aim_side < side_pure_enter && aim_elev < 0.0`;
   the exit term uses the SAME bare `aim_elev < 0.0`. The hysteresis is on
   the side-angle half only (18/23) — identical to the eagle port. A ±2°
   band is therefore an eagle-local hardening, not a lineage restoration.

2. **What actually protected the plane from horizon-riding chatter is
   compositional:** push ENTRY requires
   `have_bank && |bank_eff| > push_gate_bank` — a **bank-ERROR floor**
   (plus `elev < 0.0` and the `target_body.z` forward bound). In any
   ESTABLISHED hold the bank error is near zero, so push cannot enter or
   re-enter at all — the bare elevation term never gets to chatter. **This
   conjunct did not make the port either** (the port's bank leg is an
   angle CEILING, 100/120 — correctly kept for near-inverted denial, but
   it is a different guard; the plane has both ideas). The astern fix
   restored the fifth dropped conjunct; this is the sixth.

3. **Geometry note for the tape read:** in a pure LATERAL hold, sideDeg ≈
   90° — the sacred SIDE half already blocks push, so `sac=` flipping
   there would itself be a finding (check the printed sideDeg). The
   engineer's bare-elevation chatter is geometrically live in the
   DOWN-hold / dive-RECOVERY case (aim in-plane, elevDn crossing 0 on the
   pull-up — Chad's flown "maintain my horizon [as] I slowly pitch up").
   If the LATERAL-hold oscillation shows a silent push console, it stays
   Hypothesis B (the E2 floor), per the standing consult.

**Advisement:** lineage-faithful fix = port the bank-error floor as the
entry conjunct (eagle equivalent of `|bank_eff| > push_gate_bank`,
hysteretic like every leg); the ±2° elevation band is compatible as
belt-and-suspenders under the generalized F3 lesson (every sub-condition
of a latch hysteretic). Console verdict still rules which hypothesis the
felt oscillation belongs to before anything lands.

---

## 2026-08-01 — CONSOLE VERDICT FROM THE FLIGHT'S OWN LOGS: HYPOTHESIS A CONFIRMED — sacred-middle latch chatter, 73 + 11 transitions on the tape. The scoped fix is GO. New SOP: read the Studio logs, never ask for screenshots

**Method (now standing SOP, Chad's ask "there must be a better way"):** the
docs agent reads `%LOCALAPPDATA%\Roblox\logs\*Studio*.log` directly — the
same source that confirmed hash `2229160` in the camera arc. No screenshots
from the cockpit, ever again; the log IS the tape.

**The read (two sessions, stamp `8610740 2026-08-01 04:34` confirmed
inside each log):** 73 `[EvC push]` transitions in one session, 11 in the
other. Every ENTER: `hz=false sac=true` with **elevDn 0–15 (mostly 0–6)**
and sideDeg 8–18; EXITs flip on BOTH sacred halves — `sac=false` via
sideDeg crossing ~23–29 OR elevDn dipping ≤ 0. The engineer's predicted
signature, line for line. Bank angle at ENTER ranges 5–102° — push
entering established holds freely, which the plane's bank-error floor
forbids. One `[EvC shake]` line: `f~2.0Hz pkRoll=70 pkPit=115
bankAmp=10.8 camAmp=2.5` — PITCH-dominant, consistent with push pulsing
the elevator, not a roll-servo relapse.

**Geometry note resolved:** sac fired at genuinely small sideDeg — during
the felt oscillation the aim IS near-in-plane (the curl's own swing brings
it there: the gate's action creates the geometry that re-triggers it, the
F3 self-judging class, third instance across the federation).

**GO for the scoped fix, both halves as already designed:** the bank-error
floor ported as a hysteretic entry conjunct (the sixth conjunct; the
engineer's mapping question — the eagle rate-law has no commanded bank, so
bank_eff's equivalent is designed against the archive's definition, e.g.
lift-plane vs required-turn-plane angle — goes to the architect as
planned) + the ±2° elevation band belt-and-suspenders. Pre-registered
pass: the same two holds on the fixed build show near-zero push
transitions in the logs (read from disk), oscillation gone, dive set
unchanged.

---

## 2026-08-01 — ARCHIVE INPUT TO THE ARCHITECT VET: `bank_eff` needs NO mapping — the plane never computes a commanded bank either. It is one atan2 on the body-frame aim

Exact, from `reference/seads-feel/control/controller.h` (`bank_error`) and
`controller.cpp` (~line 577):

- **`bank_error = atan2(target_body.x, target_body.y)`** — the roll angle
  that would put the AIM above the nose (aim's body-frame x=right, y=up).
  Pure geometry; no commanded-bank state exists on the plane either. The
  eagle computes it identically from its body-frame aim direction — one
  atan2, no rate-law impedance mismatch. The architect's "lift plane vs
  required turn plane" formulation is an approximation of exactly this;
  use the archive's form.
- `bank_eff` = bank_error with the roll-direction latch applied
  (`roll_latch` on 135°/off 120° keeps the sign committed near-astern);
  `have_bank` requires lateral magnitude > 1e-12 AND blend > 0 —
  singularity guard, deliberately NOT hysteretic (the snapshot's own
  comment: hysteresis targets regime chatter, not singularity guards).
- **Floor values confirmed:** enter push only when `|bank_eff| > 120°`
  (bank_hi), exit below 100° (bank_lo) — i.e. push requires the aim
  DOWN-DOMINANT relative to body-up. An established tracking hold keeps
  the aim near body-up (|bank_eff| small) → push structurally unreachable
  → the bare sacred elevation term never chatters. This is the whole
  protection, in two constants the port already has names for.

Vet checklist, then: (1) body-frame aim, not camera-frame (the paid
lesson); (2) latch the sign like the plane (135/120) if the eagle keeps
near-astern rolls committed; (3) the 120/100 floor legs hysteretic as
planned; (4) singularity guard non-hysteretic, per the snapshot comment.

**BANK-ERROR FLOOR SHIPPED (engineer, eagle commit f9f0e5c, stamp 05:04):**
the sixth conjunct lands per the GO (7fd0a74) and the archive form
(91c6173) exactly: pushBE hysteretic entry conjunct, bankErr =
|atan2(aim.Right, aim.Up)|, enter >=120 / exit <100; singularity -> 0 ->
deny, non-hysteretic per the plane's own comment; sign latch ruled NOT
needed this pass (magnitude-only consumer, astern pushFwd-excluded) --
revisit only if a future pass consumes the sign. Sacred elevation half
gains the +-2 deg band. Architect vet vs ground truth: homology exact;
inverted + the 96-100 bank window double-covered; knobs
aimPushBankErrDeg/ExitDeg (renamed in-vet to avoid the aimPushBankDeg
vehicle-phi collision -- same two numbers, opposite sense, flagged in
config). ON THE RECORD, the felt hypothesis of this dial: the eagle
converges aim ONTO the nose (not above it, unlike the plane), so at dive
RESOLUTION the floor exits push -- fine lateral corrections near the dive
bottom ROLL again. Tell if wrong: "knife got sharp again near the dive
bottom" -> the scoped answer is holding the floor with the sacred latch,
not widening. LOG PRE-DECLARATION (F3 false-alarm guard): ENTER/EXIT
clusters at dive resolution / on-nose convergence are the floor at its
singularity, behaviorally inert while |hCmd|~=0. Fix-flight pass stands
as pre-registered: the same two holds, telemetry read from disk.

---

## 2026-08-01 — `f9f0e5c` TAPE READ (from disk, per SOP): the floor formula is right, its OPERATING POINT is wrong — the eagle LIVES at atan2's singularity the plane only visits. The "behaviorally inert" pre-declaration is falsified on the tape

**Chad (verbatim):** *"now my aim got inverted!!! What is wrong? I flew but
there are weird behaviors, like the double roll."* Log
`20260801T120614Z`, stamp `f9f0e5c 2026-08-01 05:04` confirmed in-log. 17
push transitions (down from 73 — the hold chatter IS fixed), but the
pattern is a NEW defect:

- ENTERs at `fwd=2..9, side=1..10, be=109..174` — aim nearly ON the nose,
  be huge; EXITs MID-DIVE at `elevDn=21..38` with `fwd=1..3, be=90..100`.
- Shake lines: `f~2.0Hz pkPit=65 push=1.00` (pitch oscillating WHILE push
  held) and `f~2.5Hz pkRoll=59 bankAmp=13 camAmp=9.2 push=0.00` (roll
  burst when push drops).

**Attribution:** `bank_error = atan2(aim_body.x, aim_body.y)` ignores the
forward component by construction — when the aim converges ONTO the nose
(the eagle's design; the plane holds it ABOVE the nose, the engineer's own
pre-flagged "honest thing"), the lateral projection → 0 and be is the
atan2 of numerical noise: swings 90↔174 on consecutive prints. The floor
(enter >120 / exit <100) then toggles on noise mid-dive; each drop
returns roll-to-track for a beat with a NOISE-CHOSEN direction —
"double roll," the inverted-feeling aim, the 2 Hz pitch cycle under held
push. The plane's `lat_sq > 1e-12` guard is a singularity EXCLUSION sized
for a kernel that never parks there; the eagle parks there whenever a
dive is tracking. The pre-declaration that singularity-adjacent lines are
"behaviorally inert" is falsified: the toggles carry authority changes.

**Fix family (engineer's design, both plane-consistent):** (a) a REAL
lateral dead-zone on the floor's evaluation — when the aim's lateral
projection is inside a felt-size cone of the nose (degrees, not 1e-12),
the floor HOLDS its last state instead of re-evaluating (state-hold, the
same move the fresh-latch rule already uses); (b) equivalently, while the
pure-dive corridor is engaged (sac/turnover in their bands, aim-on-nose),
the floor is not consulted — it exists to deny ESTABLISHED HOLDS entry,
and an engaged tracking dive is neither. Hysteresis on the dead-zone edge
like every leg. Pre-registered pass: the dive set flown with ZERO push
transitions between entry and pull-up on the log, no roll burst at
convergence, hold-chatter fix retained (still no transitions in lateral
holds).

---

## 2026-08-01 — CHAD'S CORRECTION STANDS: the post-roll AIM INVERSION is a SEPARATE finding — the carried aim frame has NO world-righting outside dwell. Law 5 fixed the camera's reference; the MOUSE mapping still rides the carried frame

**Chad (verbatim):** *"after the roll near the gorund my aim got inverted I
dont see that explained anywhere."* Correct — the singularity entry covered
the uncommanded rolls, not the inversion that OUTLIVES them. From the code:

- STAGE C's `(aimDir, frameUp)` pair "rotates about ITS OWN axes. No
  external" righting. The only things that world-right `frameUp`: a
  free-look release reseed (nil → reseeded level) and the E1.2b dwell
  carry (`2.0·dt·dwellRamp` — only while dwell is ARMED).
- Law 5 (`worldLevelHorizon`) explicitly kept it: "frameUp still governs
  the MOUSE mapping; the camera just no longer" follows it.
- So an UNCOMMANDED roll (the singularity's noise rolls; a ground
  graze/tumble) leaves the carried frame rolled or inverted, and the mouse
  then swings the aim about an inverted 'up' — **aim controls inverted** —
  persisting exactly when hands are busy (near the ground, dwell never
  arms, no carry ever runs). Chad's earlier observation was the same
  family: "the orientation would be off a moment until the autolevel
  horizon caught up." ⚠ Additional hazard at full inversion: the carry is
  a LERP toward level — from an antiparallel frameUp it passes near zero
  magnitude (degenerate direction) mid-recovery.

**Two-part shape (engineer's design):** the singularity fix removes this
instance's CAUSE (no more noise rolls), but the frame needs a righting
guard regardless — candidates: a slow ALWAYS-ON world-righting of frameUp
(world-level target, so it cannot reintroduce the camera-basis dip STAGE C
was built against; dwell carry stays the fast path), and/or an inverted-
frame bound (frameUp·worldUp < 0 while the bird is upright is never a
valid state — reseed or fast-right, slerp not lerp through the
antiparallel case). Whether "the horizon should always stay level" extends
to the aim frame's up is a Chad ruling if the engineer wants it: the
conservative reading says yes.

**Pre-registered pass:** after any uncommanded roll or ground graze, mouse
up is world-up within ~1 s without requiring still hands; no aim-axis
inversion reproducible from the dive-roll-graze family.

**S61d SHIPPED (engineer, eagle commit e32c497, stamp 05:19) -- both scoped
fixes:** (1) FLOOR ENTRY-ONLY, the second scoped form chosen: an engaged
tracking dive never consults the floor (it exists to protect holds); entry
requires pushBE, the other five legs govern staying-on, any exit re-arms
it. The vet "singularity harmless" claim is corrected of record in the
code comment (tape falsified it). (2) FRAME RIGHTING GUARD: always-on slow
world-righting of the Stage-C carried aim frame (aimFrameRightRate 0.5/s;
world target -- cannot reintroduce the camera-basis dip; Law 5 already
un-hooked the camera), dwell 2.0/s fast path kept, hard bound
inverted-frame-while-upright -> 4.0/s fast-right. The Lerp carry replaced
with a GEODESIC rotation about the aim axis: the Lerp antiparallel
collapse + keep-the-old guard is what froze inversions; collapse now
resolves to the LEVEL target. E1.2b ramp=0 inertness proofs marked stale
in place. CS-8 Stage-C "re-levels only on player actions" further amended
on the owner flown word (conservative Law-5 reading extends to the aim
up) -- amendment-pending his keep. Pre-registered pass stands: dive
entry->pull-up zero push transitions on the disk log + hold fix retained +
post-roll mouse-up rights within ~1 s busy-handed; inversion
unreproducible from the dive-roll-graze family.

---

## 2026-08-01 — CHAD'S PROCESS RULING: THE WHACK-A-MOLE STOPS. One consolidated, mathematically planned gate — designed whole against the archive, red-teamed once, flown once. The night's findings become the card's requirements, not more dials

**Chad (verbatim, ruling-grade):** *"were whack a moling instead of
planning the cascade solution arent we... This need a well planned once
time mathematical fix for the cascade the knife edge, everything! All im
doing is catching errors."* And the `e32c497` regression (verbatim): *"the
pitching down to go sideways is happenning again.. If I throw the mouse
aim to the left, my eagle banks sideways now after the fix when I go back
to the right on the lateral plane the eagle uses pitch and did not level
out."*

**Regression attribution (input to the plan, NOT another dial):** the
entry-only floor still ASKS the noise question at entry — a lateral
reversal sweeps the aim across the nose (fwd small = the singularity), so
push can now LATCH during the transit and, entry-only, never re-checks:
pitch-to-track mid-reversal returns, worse than before mid-engagement.
Each serial fix has moved the dice-roll, not removed it.

**THE STRUCTURAL LESSON (both kernels, ledger law):** the plane's
push-vs-roll gate is a CONVERGED mechanism — six conjuncts, two latches,
an operating-point guard, each one a paid-for bug (v5→v12, Chad's own
grind). **Never port a converged mechanism incrementally: each missing
conjunct is a bug you re-live in the order it was originally found.**
Tonight re-found, in sequence: the astern hole, the hold chatter, the
singularity, the frame righting — all solved problems in the archive.

**THE RULING EXECUTED — thread STOP on serial dials. Next motion is ONE
consolidation card:**
1. **Spec:** the plane's full push-vs-roll state machine (SPEC §9.3 +
   the [push] block, ALL conjuncts/latches/hysteresis as one unit) as the
   base — it is the mathematical solution, already converged, already
   flown v12.
2. **The three eagle deltas, now known and measured:** (a) aim-on-nose
   operating point → the singularity POLICY is part of the design (where
   be is undefined, the STATE holds — not entry-only, not per-leg
   patches); (b) no commanded bank → same atan2, settled; (c) the carried
   frame → righting shipped in `e32c497`, keep.
3. **Requirements = tonight's ledger findings verbatim** (astern denial,
   hold protection, dive-engagement stability, reversal-transit rolls not
   pitches, frame never inverted) — each with its pre-registered check,
   counted in advance, verified from the LOGS in one flight.
4. Architect designs against the archive; red-team once against the full
   requirement set; ONE build; ONE fly card. No intermediate dials land
   on this thread.

Tonight's real wins stand and carry into the card: the wobble arc closed
flown-approved; hold chatter dead at zero; frame world-righting live.
The grind was real work — it was pointed at the wrong granularity.

---

## 2026-08-01 — S62: THE CONSOLIDATION CARD EXECUTED (EvC `516899a`). The 909c8ff ruling built: the plane's gate completed WHOLE, red-teamed once, one build, one fly card

**What landed (EvC commit `516899a`, stamp `516899a 2026-08-01 05:51`, card
= `docs/UNIFIED-STEEP-AIM-PLAN.md`):** (1) the bank-error floor returned to
a STANDING hysteretic conjunct (120/100) under the ruled SINGULARITY POLICY
— state-hold: within 5° of the nose axis (sized from the plane's blend_lo)
the floor is never asked; st.pushBE freezes at its last defined truth,
fresh-in-zone = false. Engaged dive: frozen TRUE (R3). Reversal transit:
frozen FALSE from the pre-transit hold — the e32c497 latch-from-noise
regression is structurally unreachable (R4). S61d entry-only retired.
(2) The plane's elev<0 demand-sign conjunct, frozen with the floor
(red-team BLOCK-1: upc IS beUp — one transverse vector, one policy; noted
of record: outside the zone a live floor ≥100° already implies it — the
conjunct is the plane's leg made a no-op by the eagle's on-nose operating
point, kept against knob drift). (3) Astern elev-sign latch 170/160,
gate-only, decorative on today's numbers (pushFwd already denies ≥96°) —
on the record, not discovered by a bug. (4) pushFwd 87→88 (numeric align;
NOT parity — the plane's one acos(−z) leg is two eagle conjuncts,
strictly tighter).

**Citation honesty (red-team MINOR-3), binding for future ports:** the
plane's own policy at !have_bank is DENY (push forced off, no re-entry).
The freeze is the RULED EAGLE DELTA (909c8ff item 2a), required because
the eagle parks the converged aim ON the nose — a plane-faithful DENY
would exit every engaged dive.

**Amendment to 909c8ff item 2(c), evidence post-dating the ruling:** the
e32c497 slow always-on frame righting (0.5/s) is OFF (aimFrameRightRate=0,
S61e — Chad's flight attributed the "back right uses pitch, did not level"
half to it rotating the mouse basis while banked; 0 moves TOWARD locked
CS-8 "re-levels ONLY on player actions"). The fast inverted-frame bound
STAYS (that is what 2(c) protected). Attribution caveat accepted of
record: floor policy + righting change share the symptom; both are
independently config-revertible (aimPushMode=false / aimFrameRightRate=0.5)
so a dirty flight bisects by toggle, no code revert.

**Fly card (ONE flight, verdicts from the [EvC push] tape; stamp must read
`516899a`, clean):** R1 astern denial · R2 hold protection (zero ENTER in
level/lateral holds) · R3 dive stability (one ENTER, zero mid-dive pairs)
· R4 level reversal (zero ENTER during transit, rolls + levels) · R5
frame never inverted (~1 s fast bound) · R6 dive-recovery-into-reversal
(EXIT within ~0.1 s of the throw, no re-ENTER — the maneuver in Chad's
complaint, red-team MINOR-4a). Chatter rule ≥3 transitions/s with NO
exemptions (the "convergence elv noise inert" pre-declaration was
falsified by BLOCK-1 and the noise source removed).

Verify: Tier-4 659/659 green, rojo build green, red-team (opus) run ONCE
against the full requirement set per the ruling — BLOCK-1/MAJOR-2/
MINOR-3/MINOR-4 all folded before commit. No intermediate dials landed.

---

## 2026-08-01 — CHAD'S 05:51 FELT SWEEP CONFIRMED PRE-EXISTING: R1 residual (side-to-side rises/dips), S-straightline class (loop veer), R4 residual (z/pole clamping) — ALL go on the consolidation card as requirements with the plane's v12 instruments

Chad (verbatim): *"alot of effects in the cascade, like rises and dips when
going from dise to side, straight up mouse aim loops would veer off the
straight line, some z clamping"* — asked whether these predate tonight.
**They do, all three, on this ledger:** R1 dip/curl (registration ruling;
measured 8.4–16.4° on basis-live sweeps 2026-07-31; Stage C fixed the
camera-basis share; residual queued behind E2 + the never-swept
line_hold_ff); loop veer = the plane's S-straightline class (v12's
straight-line predicate, max parasitic dip < 1.0°, is the ready-made
instrument); z-clamping = R4 pole-clamp residual (Stage C was "the pole
fix" for the camera-basis share only). **Card requirement add:** these
three join the requirement table with the plane's flown instruments
(straight-line predicate; F7/F6 step tracker; S59 throw instrument) so the
consolidated gate is judged on the ORIGINAL asks, not only tonight's
regressions. The E-series ruled order (E1 re-capture → E2 → lineHoldFF
sweep) remains the standing path for the cascade half; the card covers the
gate half — the two meet at the same requirement table.

---

## 2026-08-01 — S62 FLY VERDICT (stamp 516899a verified): the death-latch is DEAD; the tape convicts the floor's magnitude-blindness; card 2 scoped to the shared v12 bar

**Chad flew ONCE on the pre-registered stamp.** PASS: R4/R5 — no
pitch-instead-of-roll latch on tape or stick; the e32c497 regression class
did not recur. FAIL, tape-read honestly (76 logged transitions/417 s):
R2 + chatter — dozens of push ENTERs in near-level flight at elevDn 2–4°
via the sacred arm, two clusters ≥4 transitions/2 s. Mechanism of record:
the bank-error floor is MAGNITUDE-BLIND — ~6° off-nose, a hair below,
reads be 125–179° ("down-dominant" by direction at trivial depth), so
sacred+floor admit push mid-sweep and the seam mutes roll for the transit.
Chad's felt "rises and dips side-to-side" = the pre-registered R1 dip
(docs agent 8110970) PLUS this tape-found contributor. Per the law: a
COUNTED requirement for card 2, no dial landed tonight.

**Card 2 scope committed (EvC docs/UNIFIED-STEEP-AIM-PLAN.md):** one
shared requirement table — C2-1 depth semantics for the floor/sacred
family (magnitude, not direction alone; zero level-sweep ENTERs), C2-2
chatter <3/s no exemptions, C2-3 the dip (E2 residual + lineHoldFF port
designed whole with its two false-nulls modeled; plane v12 flew 1.0,
eagle still gate-held at 0), C2-4 straight-line predicate <1°, C2-5 pole
clamp residual. The plane's proven instruments; one design → docs-agent
reconcile → one red-team → one build → one flight.

---

## 2026-08-01 — CARD 2 RECONCILE VERDICT (docs agent, per the 424836f gate): PASS WITH THREE REQUIRED FOLDS — headline confirmed adversarially; lineHoldFF defense set incomplete (blocking fold); C2-4 bar envelope-scoped

**The packet parked at the gate; the reconcile ran; the verdict is
`docs/consults/CARD2-RECONCILE-VERDICT.md`.** Four independent verification
passes over the v12 snapshot (`e362df289` — packet's `89447aba5`/07-23
provenance corrected of record), the goldens, this ledger, and BirdController.

**CONFIRMED:** CL-1, the design's foundation — all six plane push-gate
conjuncts traced with the S62 geometry: a 6°-off-nose hair-below aim ENTERS
push on the plane too (aim_side ≤ sin 6° can never reach the 18° sacred
gate; depth condition is sign-only per the Rung F ruling, verbatim at
controller.toml:697–714). The plane is depth-blind BY RULING; benign only
because its push branch keeps full pointing yaw. C2-1's depth floors stand
as honestly-cited eagle deltas. Q1 answered: NO amendment to this ledger —
the committed C2-1 row never said "the plane's" depth semantics; the
EvC-side plan wording corrects. Q2: consistent (designed-whole ≠ flown-at-k;
sweep stays Chad's post-E2 procedure). Q3: pass table consistent with the
registrations; R1's residual carries NO numeric predicate (throw-arc bar is
a different instrument than the basis-live 8.6–16.4° dip of record — the
substitution must be stated); the C2-5 bitfield is a NEW instrument, not
"the R4 registration's check" (no ledger antecedent).

**THE FOLDS (design-text amendments before red-team; gate cleared
conditional on them):** F1 BLOCKING — the plane's shipped line_hold_ff
carries defenses beyond the packet's {knife_fade, fwd_gate_ff, blend}:
the parasitic-only split band (the −G-bunt fix) and emitted-yaw keying
(the phantom-cancel fix), plus entry gate/clamp, structural self-bound with
the AoA hard wall, loader envelope. The eagle's {knife fade, upright fade,
cosElev} models none of these — the exact incremental-port pattern the
909c8ff LAW forbids; each must be adopted or explicitly adjudicated.
F2 — Golden #5's <1.0° bar is scoped to committed near-level U-recovered
entries; the plane's own v12 leaves 4.50° at 60° flicks ("the honest G
envelope") — a blanket bar would false-fail the plane itself. F3 —
citation corrections (third-shape ruling recited from
straightline_thread.md:32–34, not DECISIONS.md:260–300; false-null spans
:205–207/:257–264/:303–306; CAMERA-LAWS.md is eagle-repo, this repo's
anchor is the camera-anchor-mode-duality cascade; R1 = 8.6–16.4°).

**Red-team inherits three named handoffs:** the banked-configuration test
requirement (be ≈ ±178° at trivial depth REQUIRES bank — wings-level reads
±90°; D1a/D1b legs must fly the configuration or pass vacuously); the E2
steady-state question (phase < 1 at final banks under 90° — intended
physics or a new lull); the F1 adjudication table. CL-5's substance holds
(no bank-progress pull shipped anywhere in v12 — E2 is eagle-authored,
confirmed by search); the phase algebra satisfies both ruled properties and
is literally the McRuer composition form. CL-4 answered: the plane has no
per-transition schema at all; the eagle's row schema is strictly finer —
acceptable, bar stays the edge-count rule, AT-15/AT-16 named as analogs.

---

## 2026-08-01 — CARD 2 BUILT AND STAMPED (EvC 9ac7c7a, pushed): full pipeline ran per the ruled process while Chad slept; C2-4 translation APPROVED on spot-check; serve verified live and stamp verified clean by the docs agent

**Pipeline of record (close-out packet `docs/consults/CARD2-BUILD-REPORT.md`):**
reconcile folds F1–F3 landed → ONE red-team (BLOCK, 4 MAJORs, all folded —
best catch: E2's original weighting would have starved the sacred dive to
half authority) → ONE Sonnet build → Opus verify SHIP-WITH-FIXES (three
instrument bugs that would have burned the flight, incl. a chatter window
printing false-fails) → fix round → Opus delta-verify SHIP (659/659, zero
new findings, register floor held, reverts byte-traced). The reconcile's
three red-team handoffs adjudicated: H1 → C2-1 pass is now witness>0 AND
zero shallow ENTERs via a denial witness on the convicted geometry
(elevDn ∈ [2,10) ∧ side ≤ 18 ∧ be ≥ 120); H2 → CLEARED as lift-vector
physics, φ_need normalize REJECTED as the ruled-flaw shape; H3 → split
band pinned as the two-sided corrEff selector.

**A real pole bug found and fixed in the build:** Stage-C's mid-flight
upPerp fallback rebuilt the carried frame from cf.UpVector — body-roll
contamination of the mouse basis near the pole; the concrete candidate for
the R4 z-clamp residual and part of the straight-up veer class. Now an
explicit zenith cone-hold (rotations skipped, frame untouched) — the
plane's §9.2 ban, honored.

**C2-4 SPOT-CHECK VERDICT (asked in the packet, answered here): the eagle
translation is FAITHFUL to the F2 fold.** F2's requirement was the envelope
discipline, not the plane's quantity: scoped committed-entry population,
stated window, parasitic never conflated with commanded, never a blanket
cap. The eagle form (maneuver-plane departure |asin(look·planeN)| < 1.0°,
hysteresis-scoped free episodes + deterministic F6 leg, command-following a
SEPARATE column) carries all four; the quantity swap is geometry-forced
(the plane's near-level dip form cannot measure a straight-up hold), and
the G-envelope false-fail class F2 guarded against cannot fire on a lateral
departure quantity. APPROVED for judging the flight.

**Docs-agent verification of the parked state (this machine, direct):**
Rojo serve LIVE on 127.0.0.1:34872 (the transcript's exit-code-1 was the
stale-port first attempt — the stale serve from the earlier session was
the exact stale-build class of flights 8–12, killed); stamp file reads
`9ac7c7a 2026-08-01 12:32`, NO -dirty suffix; the on-disk BuildStamp.luau
modification is the generated stamp overwriting the committed S62 value,
documented normal in the file's own header. Fly-card pre-registration
("stamp must read 9ac7c7a clean") satisfiable as parked.

**Also banked from the packet:** (N4) degenerate nose-vertical push hold
has NO per-knob revert — aimPushMode=false or whole-card only, deliberate;
(N1) F1 row-#6 corrected — eagle cosPhiTheta unsigned by construction,
inverted region defended by uprightFade; (N5) inverted-lateral-throw
corner (asin saturates ±90°) pre-registered on the fly card, lever named;
D1b's revert is the PAIR to 0/0 (enter-only diverges, MAJOR-2,
boolean-traced). lineHoldFF k-sweep = Chad's post-card procedure, now
non-null (FN-1 killed by E2, FN-2 by the knife decouple); kEff fades by
(1−pushW), the recorded push-interaction decision.

**PENDING: Chad's one flight** on the S63 fly card (EvC docs/HANDOFF.md
top box). The tape judges C2-1..C2-5 + the S62 regression set (death-latch
stays dead) + the felt dip verdict. Every kill-flag is one config line.

---

## 2026-08-01 — S63 FLY VERDICT: FAIL, on the verified stamp (9ac7c7a clean, from the flight's own Studio log). The tape agrees with Chad's hands. Card 2 does NOT ship.

**Chad's verbatim report (sacred, of record):** "completely destroy this
flight kernel… mouse aim is flipping on me, no more straight down dive…
This is fully broken… All kinds of old bugs are reintroduced with this
build. The vertical knife edge is not [guarded] anymore allowing me to
dive down, I can loop around in the vertical and the curl is still
there… very disappointing for this cascade. Is my intent not clear[?]"
Also: "I can do a straight up loop manoeuvre without it rolling off to
the side" — read as the one PASS (the veer class), pending his word.

**Tape (log 20260801T194827Z, stamp verified):** C2-2 FAIL — [EvC
pushchatter] n=3 fired at elevDn=71.4°, side=1.6, be=144.5 — a NEW
chatter class AT DEPTH (S62's was shallow 2–4°); bar was zero lines.
C2-4 FAIL — [EvC line] episode max_parasitic_deg=4.19 (bar <1.0; other
episodes 0.57/0.28). C2-5 PASS on tape — [EvC pole] mask=0 sticky=0,
no clamps, matching the felt straight-up-loop-no-veer. pushwit n=94
(witness live; shallow-ENTER split not yet read). Felt symptoms not yet
attributed on tape: aim flipping (inversion class, f608682 registration);
straight-down dive GONE (suspects: D1a depth floor / N4 nose-vertical
state-hold — which has NO per-knob revert, only aimPushMode=false);
knife-edge guard loss (suspect: the FN-2 knife decouple).

**The curl is NOT a bug of this build:** lineHoldFF k is still 0 by the
ruled ordering (sweep = Chad's post-card procedure). The card failed to
say this in Feel voice — Chad flew expecting the dip fixed and met it
unchanged. HIS INTENT WAS CLEAR; THE CARD WAS NOT.

**LESSON, counted:** a pipeline can honor every gate — reconcile,
red-team, dual verify, 659/659, byte-traced reverts — and still ship a
kernel that fails in the hand inside one flight. The gates measured what
they pre-registered; three of tonight's four felt failures were outside
the pre-registered instrument set. And the fly card was written in
engineer voice; the four-level convention exists because Chad reads
LEVEL 1. Proposed standing rule (for Chad's word): every fly card
carries a Feel-voice front page — what will feel different, what will
feel THE SAME ON PURPOSE, and the one-line revert.

**Escape hatch, tonight:** every kill-flag is one config line;
aimPushMode=false backs out the push family incl. the nose-vertical
hold; D1b reverts as the PAIR to 0/0; whole-card revert documented in
the build report. Attempt #2 waits on the archive-first law and Chad's
token budget — no whack-a-mole resumes.

---

## 2026-08-01 — CORRECTION OF RECORD (Chad's word): the straight-up veer is NOT fixed — the banked "one PASS" is WITHDRAWN. And Chad's standing question on the curl is answered with its provenance chain, and his intent is re-ruled of record.

**Correction:** Chad: "the straight up loop is rolling off → that was a
typo.. not fixed. the whole thing is broken." The S63 entry's C2-5/veer
PASS is withdrawn. The [EvC pole] mask=0 sticky=0 tape line now reads
the other way: THE VEER FLEW WHILE THE POLE INSTRUMENT SAW NOTHING —
the veer is not the clamp class that instrument watches, so the Stage-C
body-up-rebuild fix (real bug, kept) was NOT the veer's mechanism. The
veer's cause remains UNATTRIBUTED. S63 is a full-card FAIL: C2-1
unread, C2-2 FAIL, C2-3 curl present (k=0), C2-4 FAIL, C2-5 FAIL
(instrument blind to the symptom).

**Chad's question: "Why did you deliberately leave the curl there? Why
didn't we fix it like the seads archive with my remaining tokens as I
had asked?"** The provenance chain, honestly: the k=0 hold traces to
the 29-throw adjudication ruling (345132b, "lineHoldFF trims last") and
the S62 card's ordering — every agent downstream, INCLUDING THIS DOCS
AGENT AT THE RECONCILE, read "trims last" as "ship at 0, Chad sweeps
later," and the committed C2-3 row froze that reading ("eagle still
gate-held at 0"). Nobody asked the archive question: THE PLANE'S OWN
FLOWN ANSWER IS 1.0 — v12 shipped line_hold_ff = 1.0, flown-approved,
sealed. Porting the mechanism whole per Chad's own 909c8ff law arguably
included porting its CONVERGED VALUE as the default, with the sweep as
trim-from-there. The reconcile checked the design against the ledger as
committed and passed it — correct against the letter, blind to the
drift from Chad's intent. That intent-drift catch was the docs agent's
to make. Counted against this agent.

**CHAD'S RULING, now of record:** fix it like the seads archive. Next
build lands the plane's flown value (line_hold_ff ≡ k = 1.0 equivalent)
as the DEFAULT, sweep is trim, not prerequisite. "Trims last" is
re-read per Chad's word: last means last in the BUILD order, not
absent from it.

**Standing for attempt #2 (fresh tokens, archive-first law):** the
symptom list is UNATTRIBUTED except the curl (k=0, now ruled to land at
archive value). Aim flipping, straight-down dive gone, knife-edge guard
gone, deep-dive chatter, straight-up veer — five open, all concentrated
in the vertical where the card's changes concentrated. Revert state:
aimPushMode=false live on stamp 3a0c159.
