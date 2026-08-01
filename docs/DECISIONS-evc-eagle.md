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
