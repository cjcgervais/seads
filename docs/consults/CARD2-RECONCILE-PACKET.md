# CARD 2 RECONCILE PACKET — eagle → docs agent (S63, 2026-08-01)

**From:** the EvC engineering session (D:\evc2026, branch updraft @ a4478d4).
**Design under reconcile:** `D:\evc2026\docs\CARD2-STEEP-AIM-DESIGN.md` (full text; this
packet is the claims-with-provenance index).
**Ask:** check the design's requirement table + mechanism claims against (1) the plane's v12
instruments as they exist in `reference/seads-feel/` and (2) the eagle ledger's registrations
of record (R1/R4 originals, S62 tape findings as convicted, C2-1..C2-5 as committed at
`7ff13be`). Flag any drift BEFORE red-team. Verdict lands beside this file as
`CARD2-RECONCILE-VERDICT.md` per the banked convention (`424836f`).

Plane snapshot cited throughout: `reference/seads-feel/` = branch `feel/kernel-v5` @
`89447aba5` (snapshotted 2026-07-23, v12-current). Eagle ledger: `docs/DECISIONS-evc-eagle.md`
through `7ff13be` / `8110970`. Eagle tape: S62 flight on stamp `516899a`, 76 transitions/417 s.

---

## Claims and provenance

**CL-1 (foundational, §0 of the design): the plane's gate ADMITS the eagle's convicted case.**
Aim 6° off-nose, hair-below → `bank_error ≈ ±178°`, `sacred_middle_enter` TRUE ⇒ plane enters
push at 6° depth; its sacred arm is depth-blind BY RULING.
*Provenance:* gate conjuncts `controller.cpp:648–663`; sacred arm `:644–647`; the ruling
`controller.toml:697–714` (Chad 2026-07-24 "below the horizon at ANY depth"); `have_bank`/
`blend_lo` precondition `controller.cpp:566–574` + `:189–196`.
**Consequence claimed:** C2-1 is an eagle DELTA by necessity (the eagle's seam
`hCmd *= (1−pushW)`, BirdController:4086, MUTES the lateral cascade; the plane's push branch
keeps full ungated yaw, `controller.cpp:665–682`). Cited under the citation-honesty precedent
(eagle ledger `:2519–2524`).

**CL-2 (C2-1 D1a): sacred arm depth floor 10/6 (hysteretic), retiring the ±2° band.**
Convicted band: ENTERs at `elevDn 2–4°` (S62 tape; ledger `7ff13be` mechanism paragraph).
Plane analogue: NONE (depth-blind by ruling — this is the named delta). Depth quantity =
world elevation, the same quantity as the plane's real depth conjunct (`aim_elev`,
`controller.cpp:615–616`; eagle `elevDownDeg`, BirdController:3938).

**CL-3 (C2-1 D1b): bank-error floor gains a down-component MAGNITUDE conjunct 8/4,**
frozen with `pushBE` in the 5° hold zone (one-transverse-vector-one-policy = the S62
red-team BLOCK-1 rule; `beUp ≡ upc`, BirdController:4008).
Convicted quantity: `bankErrDeg = atan2(beRgt, beUp)` divides out `latSin`
(BirdController:4040–4043) — the S62 conviction verbatim (ledger `7ff13be`).
Plane analogue: none directly (the plane's floor is also direction-only, `controller.h:62–66`);
the plane's nearest magnitude precondition is `err > blend_lo 5°` on TOTAL off-nose angle
(`controller.cpp:189–196`) — which the convicted 6°-off-nose case PASSES, hence the delta.

**CL-4 (C2-2): chatter instrument.** The eagle's `[EvC push]` print rate-limit
(`st.pushLogT = 0.33`, BirdController:4079) HIDES >3/s chatter; counter goes OUTSIDE that
guard (:4073). No-exemptions rule honored (the "inert noise" pre-declaration class falsified
twice: `e417678`, then BLOCK-1 — eagle ledger).
*Ask:* confirm against the plane's chatter instrument what quantities its rows carry, so the
eagle's `__evcPushRows` schema ({t, elevDn, sideDeg, be, latSin}) is judged equivalent.

**CL-5 (C2-3 E2): the plane NEVER SHIPPED E2 — spec is ruling text only.**
*Provenance:* `DECISIONS.md:260–300` (the third-shape ruling, verbatim "pull grows WITH the
bank… phased to bank PROGRESS, not gated on bank-error-to-target"); the retained REJECTED
shape `controller.cpp:693–698` (`cos^p` error gate); the reactive floor `:764–776`;
"cannot express simultaneous arrival at any power" `straightline_thread.md:45–47`.
**Eagle algebra claimed as the third shape:**
`phase = (1−wH) + wH·|sin φ|`, `wH = |azErr|/(|azErr|+|vErr|+ε)`, multiplying the vCmd term
only (insertion BirdController:4161, pre-rail), knob `aimPitchPhase`, phase ≤ 1 always (the
July-6 pull-before-roll class structurally unreachable; bankFF term NOT phased — it is
already keyed on achieved bank, `sec φ − 1`).
Spec inputs from the eagle tape: rail-is-the-arc discriminator + P2 extreme (eagle ledger
`:388–397`, `:445–460`; tape `docs/tapes/2026-07-31-throws-b39d7e6.csv`).
*Ask:* does `|sin φ|` against demand-mix weighting satisfy the ruling's "both arriving at the
final turn angle together," and does it preserve the McRuer-continuity precedent
(`straightline_thread.md:61–62`)?

**CL-6 (C2-3 lineHoldFF): false-nulls modeled, dial made measurable, NO k landed this card.**
FN-1 (saturation rail) removed by E2; FN-2 (55° taper silence) removed by decoupling from
`aimBankFFTaperDeg` to the plane's own knife-band form (`knife_fade = smoothstep(0, 0.2,
cos_phi_theta)`, `controller.cpp:894–906`; eagle knob `aimLineHoldKnifeBand = 0.2`).
False-null provenance: eagle ledger `:322–332` (pre-registered), `:352–362` (where both
live), Stage-D design `STAGE-D-LINE-HOLD-DESIGN.md:89–123`.
Chad's sweep (0.3/0.5/0.7, felt character choice) remains HIS procedure, ordered after E2
(ledger ruling `345132b`).
*Ask:* confirm the plane's lineHoldFF defense set is exactly {knife_fade, fwd_gate_ff,
blend} so the eagle's replacement set {knife fade, upright fade, cosElev} is a faithful
translation and the 55° taper's removal from THIS term re-opens no pump path the plane
defends elsewhere. **MAJOR-3 watch re-registered** (sag creep at k>0 sub-saturation turns —
record, never auto-fix).

**CL-7 (C2-4): straight-line predicate as instrument; no algebra transplant.**
Pass bar < 1.0° parasitic, from Golden #5 (0.58° measured vs 2.23–8.31° baseline;
U-recovery window 0.035 — window stated per the plane's honesty rule;
`goldens/golden_5_TELEMETRY.md:44–72`, `golden_5_VERDICT.md:29–38`).
Mechanism candidates (not blind fixes): pole-region body-frame leaks (CL-8) + the rail (CL-5);
class registration eagle ledger `:2553` — transplant of the crab-cancellation algebra remains
RULED OUT (Stage-C supersession).

**CL-8 (C2-5): pole discipline port list.**
(1) `up_misalignment` total-atan2 primitive, ±π determinism, `|dot|>0.9999` cone
(`aim_frame.h:144–152`); (2) zenith-cone no-op at every reference-consuming site; mid-flight
body-up rebuild BANNED, reseed-time body-up fallback plane-faithful (`aim_frame.h:182–190`);
(3) eagle degenerate nose-vertical push branch (BirdController:3942–3946, `sideDeg=0` ⇒
sac/side trivially true) closed by state-hold — named eagle-local hardening (the plane also
zeroes `aim_side` there, `controller.cpp:626–631`, benign only because its branch doesn't
mute); (4) camera consumer VERIFY-first against `docs/CAMERA-LAWS.md` (the eagle has a live
`horizonLevelRate 2.0` relax toward projected frameUp, BirdController:821–831, vs the plane's
S7-cam3 "NO re-level rate," `controller.toml:1012–1015`; the law is the anchor, keychase
postmortem rule); (5) degenerate-branch bitfield instrument = the R4 registration's check
(eagle ledger `:2553–2572`, "z-clamping = R4 pole-clamp residual").
Also on record: the post-roll aim-inversion registration (`f608682`, ledger `:2391–2449`)
stays a SEPARATE finding — this card keeps the fast bound, ports its math (item 1), and does
NOT re-enable slow righting (`aimFrameRightRate` stays 0).

**CL-9 (regression rows): S62 card R1–R6 re-run on the same flight; death-latch class must
stay dead** (S62 PASS of record, ledger `7ff13be`). Freeze policy (state-hold vs the plane's
DENY) remains the ruled eagle delta (909c8ff item 2a) — not re-cited as plane-faithful.

---

## Specific reconcile questions (beyond the per-claim asks)

Q1. Does the committed C2-1 wording ("the floor/sacred family gains the plane's depth
semantics") tolerate the finding that the plane HAS no depth semantics on those two legs
(CL-1), with the depth floors as named eagle deltas — or does the scope text need a ledger
amendment before build?
Q2. C2-3's card-flight pass is defined WITHOUT landing a lineHoldFF value (E2 arcs + felt
verdict; sweep stays Chad's post-card procedure). Consistent with the C2-3 row as committed
("E2 residual + lineHoldFF port designed WHOLE")?
Q3. The pre-registered pass table (design §pass-table) — any drift vs the registrations of
record (original R1 dip 8–16° measurement, R4 z-clamp, S-straightline class filing)?

If no docs session is live, this packet parks here per the banked convention; nothing moves
to red-team until the verdict lands.
