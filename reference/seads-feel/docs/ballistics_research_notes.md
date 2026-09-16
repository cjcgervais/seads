# Ballistics research notes (raw sub-agent findings)

Durable capture of the forge loop's RESEARCH step. Raw per-gun findings land here;
the consolidation pass turns them into internally-consistent (tof, drop, vret) tables
in `ballistics_reference.json`, with Fable cross-checking the drag derivation. Key
recurring finding: **open-web primary firing tables (Schusstafel) are login/archive
locked**, so ground truth is largely DERIVED from cited MV + mass + a G1 BC estimate,
calibrated to known harmonization anchors (e.g. Fw 190 Visierschuss 550 m). This is
exactly where Fable's consistency check earns its keep.

## MG151_20 — 20 mm MG 151/20, Minengeschoß  (DONE, adversarially verified)
- **MV = 805 m/s** (M-Geschoß specifically; the common "700–720 m/s" figure is the
  heavier HEF-T/AP family — do not conflate). Mass **92 g** (sub-variants 86–97 g),
  20 mm. 18.7 g PETN filler.
- **No published BC or firing table** in any open source. Derive **G1 BC ≈ 0.20**
  (SD 0.327 lb/in², form factor i≈1.5–1.7 for a blunt thin-wall shell — decays faster
  per metre than a dense AP round despite higher MV). Use G1, not G7 (blunt cannon
  shell goes transonic→subsonic in the combat envelope).
- Derived retained-velocity (G1≈0.20, v0=805, flat, ISA SL) — COMPUTED ±5–10%:
  100→787, 200→768, 300→748, 400→727, 500→705 m/s. Derived drop:
  100→0.08, 200→0.32, 300→0.74, 400→1.37, 500→2.22 m. (NB: this is much LESS drag than
  the provisional seed assumed — seed had 500 m vret ≈ 400; real ≈ 705. Correct on
  consolidation.)
- **Calibration anchor:** Fw 190 Revi Visierschuss (trajectory re-crosses aim line) at
  **550 m** (D.(Luft)T.2190); Fw 190 A-8 inner-gun convergence 600 m, outer 400 m.
- No ballistic difference between hub Motorkanone and gondola (same barrel/cartridge).
- **Locked primaries to chase later:** Handbuch Bordwaffenmunition Teil 5 (michaelhiske.de,
  login), L.Dv.4000/10 Serie H, Schiessfibel "Horrido" (OKL 1944, archive.org login),
  Bundesarchiv-Militärarchiv Freiburg, NARA RG319/407.
- Confidence: **medium** (BC + table derived, not measured).

## MG17_792 — 7.92 mm MG 17, sS ball  (DONE, high confidence)
- **MV = 760 m/s** for the **sS ball** (600 mm barrel; anchored on a transcribed German
  sS Schußtafel, whq-forum, energy column arithmetic-verified). Mass **12.8 g**, 7.92 mm.
  **G1 BC ≈ 0.575** (published, Reibert 1940 / "Du und dein Heer" 1943; G7 ≈ 0.295).
- Table (200 m table anchors, 100/300/500 interpolated; 800 is a direct table value):
  | range m | 100 | 200 | 300 | 400 | 500 | 800 |
  |---|---|---|---|---|---|---|
  | vret m/s | 708 | 660 | 616 | 574 | 533 | 426 |
  | tof s | 0.14 | 0.28 | 0.44 | 0.61 | 0.79 | 1.43 |
  | drop m | 0.025 | 0.10 | 0.22 | 0.40 | 0.66 | 2.30 |
- **⚠ CHAD DECISION — which round the cowl guns fire:** the Bf 109 cowl MG 17 in service
  fired **-v (verbessert / high-pressure) rounds — SmK-v at 855 m/s** (L.Dv.4/10), NOT
  standard sS ball. Config today uses `mg_speed_mps = 760` (= sS ball). For combat-accurate
  cowl gunnery use ~855. Both defensible; flag for Chad. (905 m/s is the SmK L'Spur
  AP-tracer, NOT sS — adversarially refuted across 5 angles.)
- **Locked primary to OCR next:** D 126/1 MG34 sS Schußtafel scan (downloadable image, not
  web-text-extractable) for exact original digits.

## ⚠ CROSS-GUN CONSISTENCY FLAG (for Fable consolidation)
The MG 17 sS table (published BC 0.575) loses **~30 % velocity by 500 m** (760→533).
The MG 151/20 M-Geschoß DERIVED table (G1≈0.20, low conf) loses only **~12 %** (805→705).
That ordering is **physically backwards**: the M-Geschoß is blunt/low-BC (form factor
1.5–1.7) and its sectional density (~230 kg/m²) is barely above the streamlined 7.92
(~204 kg/m²), so it should decay AT LEAST as fast as the 7.92, not slower. The derived
MG 151/20 table is almost certainly **too optimistic** — real 500 m vret likely ~600–650,
not 705. **Fable must reconcile the two BCs on a common drag model before either table is
frozen.** This is the load-bearing correctness check of the research step.

## MG131_13 — 13 mm MG 131  (DONE, medium confidence, OFF-airframe)
- **MV:** AP-T **710 m/s / 38.5 g**; HE-T **750 m/s / 34 g** (I-T 770/32 g). 13 mm.
- **Derived G1 BC ≈ 0.24 (AP) / 0.20 (HE)** — from an OCR'd Handbuch table (single
  source, LOW conf). Retained-velocity column (AP-T, arithmetic-self-consistent):
  0→710, 100→619, 200→539, 300→472, 500→368 m/s (tof 0/0.151/0.325/0.523/1.004).
  **⚠ the drop column is OCR-misaligned (off by ~10×) — discard it, recompute drop from
  the drag model + tof.** ~48 % velocity loss by 500 m.
- Locked primaries: L.Dv.4000/3 (michaelhiske.de, 401), Handbuch PDF at archive.org
  (OCR the 13 mm Schusstafel directly), BArch RL 3/7207.

## ⚠ CROSS-GUN PICTURE (sharpened — three of four in)
| gun | round | BC(G1) | v0 | v@500 | loss@500 |
|---|---|---|---|---|---|
| MG17 7.92 | sS ball | 0.575 (pub) | 760 | 533 | 30 % |
| MG131 13 | AP-T | ~0.24 (der) | 710 | 368 | 48 % |
| MG151/20 | M-Geschoß | ~0.20 (der, low) | 805 | **705?** | **12 %?** |
The 20 mm M-Geschoß row is the clear outlier: with a LOWER BC than the 13 mm it must lose
**at least as much** velocity, so real v@500 is plausibly ~**500–560 m/s**, not 705. The
derived table is too optimistic. **Fable resolves this on ONE common drag integration**
(same G1 model, same air density) before any table is frozen — do NOT freeze MG151/20 on
the 0.20-BC number alone.

## MGFF_M_20 — 20 mm MG-FF/M, Minengeschoß  (DONE, medium confidence, OFF-airframe)
- **MV = 700 m/s** for the **M-Geschoß** (NOT 585 — that's the heavy 115–117 g HE/AP).
  Mass **92 g**, 20 mm — the SAME M-Geschoß projectile as the MG 151/20. Derived
  **G1 BC ≈ 0.14** (band 0.10–0.19), fitted to primary velocity anchors. Very loopy,
  short-range. vret 100→598, 200→512, 300→438, 400→374, 500→320 (54 % loss); tof to
  500 m = 1.084 s.

## ✅ CONSOLIDATION MODEL (all four in — quadratic drag, fitted to real anchors)
A constant-`k` quadratic drag law `a_drag = −k·|v|·v` gives horizontal `v(x)=v0·e^(−k·x)`.
Fitting `k` to each gun's cited (v0 → v@500) anchor **reproduces the real MG 17 firing
table to <1 %** at every 100 m step (708/660/616/574/533 vs model 708/660/614/572/533) —
strong validation. Per-gun `k` (1/m), which maps to the config drag knob per round kind:

| gun | round | v0 | k (1/m) | v@500 | note |
|---|---|---|---|---|---|
| MG17_792 | sS ball | 760 | 7.10e-4 | 533 | pinned to real Schußtafel |
| MG151_20 | M-Geschoß | 805 | **1.565e-3** | **368** | k SHARED with MG-FF (same projectile) |
| MG131_13 | AP-T | 710 | 1.315e-3 | 368 | OCR anchor |
| MGFF_M_20 | M-Geschoß | 700 | 1.565e-3 | 320 | pins the M-Geschoß k |

**The big correction:** MG 151/20 M-Geschoß at 500 m ≈ **368 m/s**, not 705 (my seed) — same
projectile as MG-FF forces the same k; the higher MV just shifts the absolute up. 54 % loss.

**DROP columns in the source tables are NOT usable as-is:** they are sight/LOS-referenced
(Pfeilhöhe / Fallhöhe relative to a sighted trajectory), not bore-referenced gravity drop —
that's why MG 17 lists 0.66 m at 500 m while a horizontally-fired round actually drops
≈½·g·tof² ≈ 3 m, and why the MG 131 drop column looked "10× off". The sim fires a round
along the bore, so ground-truth **drop must be computed as bore-referenced ½·g·tof² with the
drag-drop coupling**, from the same integrator — NOT copied from the table drop columns.
(Fable: verify this datum call — it's the subtle one.)

## ✅ FABLE-5 PRE-CONSULT VERDICT (fresh isolated context — the derivation is vetted)
Model + tables CONFIRMED; the drop datum and the pipper-fork caught. Verdicts:
- **C1 CONFIRM** — constant-k reproduces the real MG 17 firing table to **<0.4 %** at all
  five points, tof(500)=0.79 s. Strong validation over 0–500 m (M2.2→M1.57).
- **C2 CONFIRM (P2)** — shared-k for the 92 g M-Geschoß is sound to first order (implied
  Cd≈0.75, plausible for a blunt shell); second-order Mach bias means MG 151/20 true
  vret(500) ≈ **386, not 368** (+5 %, within the 7 % vret tolerance — the shared-k slightly
  over-drags the faster gun). Noted; kept shared-k (honest band covers it).
- **C3 CONFIRM** — 705 m/s @500 was arithmetically broken (needs Cd≈0.13, artillery
  territory); ~368–390 is the right order.
- **C4 CONFIRM** — bore-referenced drop is the only self-consistent datum; the table's
  0.66 m is a sight-line/max-ordinate quantity (Fable's integrator: max ordinate 0.767 m
  @265 m — same family). Using it would make gravity look **4.1×** too strong. Vertical-drag
  reduction (2.73 vs vacuum 3.06 m, −11 %) confirmed correct sign.
- **C5 CONFIRM** — on-airframe guns stay supersonic through 500 m (constant-k fine);
  MG-FF/M crosses M1 at **461 m** — don't extrapolate the reference guns past 500 m.
- **C6 CONFIRM — P0 (the one that matters): PIPPER FORK.** Add drag to rounds but leave the
  pipper solving vacuum → MG 151/20 on a 150 m/s crosser at 400 m: lead 74.5 m (vacuum) vs
  103.6 m (dragged) = **29 m / 73 mrad / 4.2° aim-point shift** — a pure miss-behind on every
  deflection shot. **Mandatory: re-derive the pipper with the SAME drag law; both the round
  integrator and the pipper consume ONE shared per-gun k from a single source.** tof has a
  closed form t(x)=(e^{kx}−1)/(k·v0), monotone → the fixed-point solve stays cheap.

### Implementation spec (shaped by Fable — hand this to the Sonnet implement step)
1. **One drag constant per round kind, config-tabled** (`[ballistics] drag_k_per_m` or a Cd
   the loader converts), consumed by BOTH `weapon/advance` AND the pipper — single source, no
   hand-copied second constant (that's how they re-fork).
2. **`advance`: add `a_drag = −k·|v_air|·v_air`** to the per-tick accel (v_air = round vel −
   local air, = round vel for still air). Drag acts on AIR-RELATIVE velocity.
3. **P1 — inheritance/air-relative:** the round launches at v0 + shooter TAS and drag acts on
   the air-relative speed; the pipper must solve with the SAME effective launch speed + drag,
   or they fork again even after C6. (Fable: 16 % lead difference if the pipper uses table v0
   while the round uses the inherited speed.)
4. **P2 — integrator:** semi-implicit Euler at dt=1/120 overshoots drop **3–4 %** vs the fine
   reference (within the 8 % tol but eats half the budget). Prefer the **exact drag update
   `v ← v/(1 + k·|v|·dt)`** (cheap, removes the overshoot) or substep; else loosen drop_rel.
5. **P2:** k bakes in sea-level ρ — over-drags at altitude (~10 %/km). Fine for the near-SL
   arcade sphere; flag if combat goes high.
6. **C++ truth harness** dumps measured (tof,drop,vret) to `build/ballistics_measured.json`
   so `ballistic_score.py --measure file:...` scores the REAL shipped model (SIE at 1/120),
   not the Python reference — that's what catches P2 #4.
