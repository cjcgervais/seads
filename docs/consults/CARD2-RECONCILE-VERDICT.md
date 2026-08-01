# CARD 2 RECONCILE VERDICT — docs agent → eagle engineer (S63, 2026-08-01)

**Packet:** `CARD2-RECONCILE-PACKET.md` (this directory). **Design:** EvC
`docs/CARD2-STEEP-AIM-DESIGN.md`. **Convention:** `424836f`.

**Verification basis.** Every claim was checked against the actual sources by four
independent verification passes: (1) the plane gate code, (2) the plane E2/lineHoldFF
material, (3) goldens + pole discipline, (4) the eagle ledger, BirdController, and the
design's pass table. Nothing below is taken from the packet's own text.

**Provenance correction first (packet header):** the packet cites the plane snapshot as
`89447aba5`, snapshotted 2026-07-23. Wrong on both. `reference/seads-feel/` is the
COMPLETE tracked tree at seal commit **`e362df289`** (`flight-kernel-v12-2026-07-30`),
re-snapshotted 2026-07-30 late night, and that commit is still the live branch tip as of
this verdict (read-only check run per the watch-item). The "v12-current" claim is right;
the hash and date are not. All line citations below are against `e362df289`.

---

## GATE VERDICT: **PASS WITH THREE REQUIRED FOLDS.**

The design's load-bearing findings are TRUE — including the headline. Nothing moves to
red-team until the three folds below are in the design text; none of them requires
re-architecting, but F1 is substantive, not editorial.

---

## The headline finding (CL-1): CONFIRMED, adversarially

The reconcile did not take CL-1 on faith — the verifier traced all six push-gate
conjuncts in the plane's v12 code (`control/controller.cpp:656–663`) with the S62
geometry (6° off-nose, aim a hair below, be ≈ ±178°): **all six pass, push enters, and
the exit (`:652`) stays latched while the aim sits below.** The geometric key: an aim 6°
off the nose is at most 6° out of the vertical plane, so `aim_side ≤ sin 6° = 0.105` —
the sacred arm's 18° side gate (`side_pure_enter = 18.0`, `controller.toml:713`) can
NEVER block any 6°-off-nose aim, and its only depth condition is `aim_elev < 0.0`
(`controller.cpp:644–645`) — any depth, per the Rung F ruling verified verbatim at
`controller.toml:697–714` ("below the horizon at ANY depth", Chad 2026-07-24). CL-2
(`aim_elev` world elevation, `:615–616`) and CL-3 (direction-only `bank_error`,
`controller.h:62–66`, atan2 normalizes magnitude away; the 1e-12/1e-18 guards are
explicitly "NOT a control regime gate") both VERIFIED.

**Consequences, of record:**
- The plane is depth-blind on these legs BY RULING; it is benign there only because the
  push branch keeps full pointing yaw (`:665–682`, verified — no lateral muting).
  C2-1's two depth floors are therefore **legitimately named eagle deltas** under the
  citation-honesty precedent (ledger `:2521–2525`, verified). The design's framing is
  honest and stands.
- **Precision fold (part of F3):** the sacred arm itself contains NO bank_error
  condition — the `|bank_eff| > 120°` guard (`:656`, `push_gate_bank`) is a separate
  conjunct of the same entry. The design should not describe the arm as carrying it.
- **Test-configuration requirement (red-team handoff):** be ≈ ±178° at trivial depth
  REQUIRES substantial bank — wings-level the same aim reads be ≈ ±90°. The D1a/D1b
  card-flight legs must exercise the banked mid-sweep configuration that actually
  produces the convicted case, or the depth floors pass vacuously.

## F1 (BLOCKING): CL-6's defense set is incomplete — the plane's line_hold_ff carries defenses the port does not model

The packet claims the plane's defense set is exactly {knife_fade, fwd_gate_ff, blend}.
**REFUTED.** The shipped v12 implementation (`controller.cpp:882–907`, emission `:905–906`)
carries, beyond those three (knife_fade actual lines `:890–891`, band constant `:884`):

4. **Parasitic-only split band** (`kLineParaBand = 0.087`, `:889`, `:897–902`):
   `w_dn`/`w_up` smoothstep weights by sag sign — only vertical motion AWAY from the
   aim's elevation line is cancelled. This is the v2-all-cancel fix; without it the
   all-cancel form bunted elevated aims into the −G floor (README `:15–16`).
5. **Emitted-yaw keying** (`:841–844`, `:894`): `yv = yaw·sin(φ)` reads the yaw AFTER
   the gate composition — an uncommanded yaw is never phantom-cancelled.
6. Entry gate `line_hold_ff > 0 && cos_phi_theta > 0` (`:882`) + division clamp
   `max(cos_phi_theta, 1e-6)` (`:904`).
7. Structural self-bound (term ≤ |emitted yaw|·sin φ/cosφθ) with the AoA pushback as
   the downstream hard wall (`:857–865`); deliberately NOT capped by w_push (`:867–869`).
8. Loader envelope `0 ≤ line_hold_ff ≤ 2` (`load_controller.cpp:413–414`).

Items 4 and 5 are precisely the defenses that closed the −G-floor bunt and the
phantom-cancel failure paths. The eagle's replacement set {knife fade, upright fade,
cosElev} accounts for **none** of 4–7. Porting without them is exactly the
incremental-port pattern the 909c8ff LEDGER LAW forbids ("never port a converged
mechanism incrementally — six conjuncts = six re-lived bugs, in order"), and C2-3's
committed bar is "port designed WHOLE." **Required fold: the design must enumerate the
plane's full defense set and, for each item 4–8, either adopt it or explicitly
adjudicate why the eagle's structure makes it unnecessary — silence on any one is a
red-team block.** The two false-nulls themselves are correctly modeled (FN provenance
spans corrected in F3), and the 55°-taper decoupling claim is confirmed: the plane's
form has no bank-angle taper coupling anywhere in the snapshot.

## F2 (REQUIRED): C2-4's <1.0° bar must state its envelope

Golden #5's numbers VERIFIED (0.58° measured max vs 2.23–8.31° v11 baseline;
`golden_5_TELEMETRY.md:44–56`) — but the predicate is scoped to **committed near-level
U-recovered entries** (|nose_elev| < 0.1, blend ≥ 1 within 1.5 s, recovery within a 2 s
window to a 0.035 tolerance — note 0.035 is the recovery TOLERANCE in nose_elev units
≈2°, not a time window; the packet's "U-recovery window 0.035" conflates them). On 60°
flick legs the plane's own shipped v12 leaves a **4.50° residual**, declared "the honest
G envelope" (README `:16–17`), and the ledgered endgame droop is ~1.3°. A flat per-leg
<1.0° bar across a flick sweep is unpassable by the plane itself and would read as a
false regression. **Required fold: state C2-4's predicate as "<1.0° parasitic dip on
committed near-level U-recovered entries (the Golden #5 instrument)," never as a blanket
deviation cap.** The design's pass-table row ("straight-up episodes") is close but must
carry the scoping explicitly. Citation drift: the verdict text is at
`golden_5_VERDICT.md:23–25`, not 29–38.

## F3 (REQUIRED): citation corrections — fold before red-team, none change a mechanism

1. **CL-5(a):** the third-shape ruling is NOT at `DECISIONS.md:260–300` (that span is
   camera content) and the quoted phrase "phased to bank PROGRESS, not gated on
   bank-error-to-target" exists nowhere in the snapshot. The real sources:
   `straightline_thread.md:32–34` ("back-pressure grows with bank… Chad's 'both align to
   the final turn angle at the same time'") and README `:48` ("RULED A FLAW: pull arrives
   WITH the bank, simultaneous arrival" — the ruled-missing behavior). Recite from these.
   The substantive claim HOLDS: no bank-progress-phased pull shipped anywhere in v12
   (searched; the only achieved-bank pitch term is the line_hold_ff crab cancellation,
   which is a cancellation, not a phased pull; the shipped pull gate is the
   `cos(bank_eff)^p` error form at `:693–698` — the ruled flaw itself — plus the reactive
   floor at `:764–776`, both verified exact). E2 as eagle-authored: stands.
2. **CL-6 false-null provenance:** the ledger spans are `:205–207`, `:257–264`, `:271`,
   `:303–306` — not `:322–332`/`:352–362` (those hold the CS-5 quote and the P0 rider).
3. **CL-8(4):** `docs/CAMERA-LAWS.md` is an EAGLE-repo file, not a mandalark file. This
   repo's anchor is `docs/cascade/camera-anchor-mode-duality.md`, whose law ("exactly one
   camera automation in the whole kernel") independently supports the port-list item — a
   continuous relax-toward-frameUp is a second automation, hence a violation. Verified
   caveat on the eagle side: the `horizonLevelRate 2.0` branch (BirdController `:821–831`)
   runs only when `CAM.worldLevelHorizon` is false — the design's VERIFY-first stance is
   correct; keep it.
4. **CL-8(5):** the degenerate-branch bitfield has NO ledger antecedent — ledger
   `:2553–2572` registers the z-clamp residual but no branch check ("degenerate" appears
   nowhere in that entry). Cite the bitfield as a NEW instrument introduced by this
   design, not as "the R4 registration's check."
5. **R1 figure of record:** the founding R1 registration carries no number; the measured
   figures are the 2026-07-31 attribution entry, ledger `:103` — sweeps 13.6°/8.4°,
   **session range 8.6–16.4°**. "8–16°" is a rounded echo (and `:2559`'s "8.4–16.4°"
   already conflates the sweep value with the range floor). Use 8.6–16.4°.
6. **Packet header provenance** (snapshot hash/date), per the correction at top.

## The per-claim asks, answered

**CL-4 (chatter schema equivalence):** the plane has NO per-transition row schema —
chatter is measured by edge counts alone (AT-16 `switches ≤ 2`, AT-15 `switches ≤ 1`,
`test_acceptance.cpp`), with gate quantities scattered across per-tick tables (latflick
carries be/tb/push; mouseloop carries aimElev; NO plane instrument logs aim_side at
all). The eagle's `{t, elevDn, sideDeg, be, latSin}` is strictly FINER than anything the
plane carries. Verdict: not equivalent — better. Acceptable as the C2-2 instrument
provided the pass bar remains the edge-count rule (<3/s, no exemptions) with the plane's
AT-15/AT-16 switch-count checks named as the analog. The `:4073`/`:4079` rate-limit
blind spot is real (verified in code) — the counter outside that guard is right.

**CL-5 ask (does `phase = (1−wH) + wH·|sin φ|` satisfy the ruling + McRuer):**
Structurally yes, on both named properties: pull develops WITH achieved bank (|sin φ|
monotone through the roll-in — kills the lull), and it is keyed on bank progress, not
bank-error-to-target — the exact distinction the ruled-flaw `cos(bank_eff)^p` form fails.
Pure-vertical demand gives wH = 0 → phase = 1, so straight pulls are untouched. And the
composition `(1−w) + w·x` is literally the McRuer-continuity precedent's own form
(`straightline_thread.md:61–62`, verified). **One open point, handed to red-team, not
blocking:** at a final bank φ_t < 90°, phase arrives at (1−wH) + wH·|sin φ_t| < 1 — a
steady-state attenuation of the sustained-turn pull that the ruling's "both arriving
together" language doesn't obviously license. Red-team should adjudicate whether that
residual is the intended physics (lift-vector geometry) or a new lull in disguise.

**CL-6 ask:** answered by F1 — the premise (defense set = three items) was false, so
"faithful translation" cannot yet be judged; re-submit the mapping against the full set.
MAJOR-3 watch re-registration noted and kept (sag creep at k>0: record, never auto-fix).

## The three packet questions

**Q1 — NO ledger amendment needed on this ledger.** The committed C2-1 row of record
(`7ff13be`, ledger `:2589–2590`) reads "depth semantics for the floor/sacred family
(magnitude, not direction alone; zero level-sweep ENTERs)" — it does NOT say "the
plane's depth semantics." That phrase is EvC-side plan text; correct it there. The
requirement of record is mechanism-agnostic and the named eagle deltas satisfy it as
written. The table preamble "the plane's proven instruments" attributes the measurement
bar, not the mechanisms — and with CL-1 confirmed, the honest citation is: the plane is
depth-blind on these legs by the Rung F ruling; the depth floors are eagle deltas.

**Q2 — CONSISTENT, with one tightening.** C2-3 as committed requires the port "designed
whole with its two false-nulls modeled" — designed, not flown at a value; the row itself
records the gate-hold at 0, and the sweep-after-E2 ordering is the ruled procedure
(`345132b` — a mandalark commit, not ledger text; entry at `:365–436`, sweep-resumes
language at `:429–431`). A card-flight pass on E2 arcs + felt verdict with no k landed
matches the row. **But after F1, "designed whole" means the FULL plane defense set
modeled, not just the two false-nulls.** The gate-hold at 0 stays deliberate and
toggle-bisectable.

**Q3 — pass table verified against the registrations; two flags, one fold each.**
The table is consistent with the S62 conviction (C2-1's zero-ENTER band at elevDn < 10°
covers the convicted 2–4° with margin), the chatter rule (<3/s, no exemptions, ledger
`:2543–2545`), and the straight-line bar (ledger `:2561–2562`; F2 scoping applies).
Flags: (i) **R1's residual has no numeric predicate in the table** — C2-3's ≤3° bar is
on throw arcs at healthy bank_frac (the structural-floor population, ledger
`:458–462`), a different instrument and population than the basis-live sweep dip R1 was
measured on (8.6–16.4°). Not a contradiction — Stage C closed the camera-basis share —
but the substitution must be STATED in the design: R1's residual rides on E2 + Chad's
felt verdict, with the basis-live sweep numbers named as the original baseline, so a
later reader doesn't mistake the throw-arc bar for the R1 instrument. (ii) the bitfield
citation fix (F3.4).

**CL-9:** consistent as claimed — freeze-policy delta verified at `:2521–2525` +
909c8ff item 2(a) at `:2482–2485` (mirrored in BirdController `:4060–4064`); not
re-cited as plane-faithful. Correct.

## Eagle-side citations: all verified

Every BirdController citation in the packet checked exact: the seam `:4086`, the
magnitude-blind atan2 `:4037–4043`, elevDownDeg `:3938`, the 0.33 s log guard
`:4073`/`:4079`, the nose-vertical degenerate branch `:3942–3946`, beUp ≡ upc `:4008`
(comment `:4008–4010`), the E2 insertion point `:4161` with the rail at `:4195`, and
the camera relax `:821–831` (with the worldLevelHorizon flag caveat above). The design's
own D1a/D1b anchors (`:3969–3972`, `:4044–4051`) also verified.

---

**Standing after this verdict:** fold F1–F3 into the design text, then the pipeline
proceeds — one red-team → one build → verify ladder → stamp → Chad's one flight. The
red-team inherits three named handoffs from here: the banked-configuration test
requirement (CL-1), the steady-state phase < 1 question (CL-5), and the F1 adjudication
table (each unadopted plane defense argued, not skipped). This verdict is banked on the
eagle ledger; the reconcile gate is CLEARED conditional on the folds — the folds are
design-text amendments, not re-design, and do not re-open the gate once landed.
