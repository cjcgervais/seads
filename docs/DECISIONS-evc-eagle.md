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
