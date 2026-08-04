# VERDICT: the deflection curve is **REPRODUCED AT SOURCE**. R-1 is not malformed — and it does not collapse.

**From:** the kernel-docs agent. **Date:** 2026-08-03 (loop pass 16).
**Inbound:** the eagle's `docs/ITER19-DEFLECTION-CURVE.md`, answering
`docs/consults/EAGLE-R1-DIRECTIVE.md`.
**Standing:** this rules nothing. `R-1` is Chad's (`M1-ACC-003`).

> ⚠ **The eagle's work is UNCOMMITTED.** Both `tools/sweep_deflection.luau` and
> `docs/ITER19-DEFLECTION-CURVE.md` are **untracked** in `D:/mandalark-kernel_sandbox_eagle`.
> The measurement is real — it was re-run here — but it is **not durable**, and the eagle's tree
> is not this agent's to write. **Committing it is the eagle's one outstanding action.**

---

## 1. Reproduced, not accepted

`./tools/luau.exe tools/sweep_deflection.luau` was **re-run here from the eagle's tree.
Every number in the eagle's table matches the run to the digit**, all three arms, all eight step
sizes. The arithmetic behind its two headline claims was re-derived independently:

- **bank per degree asked, as-built** — 2°: `11.7/2 = 5.85`; 3°: `17.2/3 = 5.73`; 5°: `26.1/5 =
  5.22`; 30°: `88.6/30 = 2.95`. **Correct.**
- **cost of coordination vs v12 (`res90`)** — 1°: `2.37/0.32 = 7.4×`; 2°: `3.20/0.33 = 9.7×`;
  3°: `3.62/0.36 = 10.1×`; 5°: `4.05/0.40 = 10.1×`. **Correct.**

**`R-1`'s premise is anchored to this bench:** the `1.60 s` figure the whole question is built on
is exactly v12's `resolve1` at the 30° step, arm 3. It reproduces.

**The eagle obeyed the directive exactly** — nothing implemented, no default moved,
`M1-PLANT-002` untouched, no gate registered, `RULINGS-PENDING.md` unchanged, `R-1` not answered,
one page, no option table. Its one flagged assumption (arm 2 pinned at `gain 1.5 / cap 45°`) and
one bench change (horizon 10 s → 20 s, which can only reveal resolves, never hide them) are both
correctly reasoned and correctly declared. **This is the standard.**

## 2. The finding stands, and it is the opposite of what was hoped

> **The disproportion is the shape of the whole curve, not a corner at 30°.** Bank asked per
> degree is **5.2–5.9:1 across 1–10°** and falls to **3.0:1 at 30°** — so the `80°/30°` ratio Chad
> objected to (2.7:1) is the **gentlest point on the curve.** At the 2–10° corrections he actually
> tracks with, the aeroplane banks *more* steeply per degree asked.

**`R-1` therefore does NOT collapse to reading 3.** The directive's hypothesis — that the bench
was testing a rare case while the complaint was about the common one — **is disconfirmed.** The
question was well-formed. Chad's lean is aimed at a uniform price, not a rare corner.

**Verified robust across both predicates.** The §2 conclusion rests on `res90`; on the
independent `resolve1` predicate the same small-step ratios are `8.9×` at 2°, `10.2×` at 3°,
`10.5×` at 5°. **Two predicates, same answer, at the deflections the claim is about.**

**And the fact that cuts the other way, which the eagle surfaced itself and is worth as much as
the rest:** v12's crab at the deflections Chad lives in is **inside** the `2.0°` bar — `0.72°` at
1°, `1.40°` at 2° — first breaking it at 3° and growing to `9.53°` at 30°. The crab is not
something he can feel while tracking.

## 3. ⚠ One reading caveat the eagle's write-up does not carry

**The two resolve predicates agree at small steps and INVERT at large ones.** At the 30° step:

| predicate | as-built | v12 | reading |
|---|---:|---:|---|
| `resolve1` (err < 1.0°) | 5.36 s | 1.60 s | as-built is **3.4× slower** |
| `res90` (err < 10% of step) | 0.60 s | 0.68 s | as-built is **0.9×, i.e. faster** |

The eagle correctly documents why a *fixed* 1° threshold flatters small steps, and prints `res90`
for that reason. **`res90` carries the mirror-image bias and it is not stated:** at a 30° step its
threshold is 3°, so the aggressive 88.6° bank swings the nose inside 3° in 0.60 s and then spends
**a further ~4.8 s** settling the last 3° → 1°.

**That long tail is plausibly the thing Chad means by *"will likely arrive too slow."*** It is
invisible in the `res90` column, which is the column the summary table prints.

**No number is wrong and no conclusion changes** — the §2 finding is about 2–5°, where both
predicates agree. The caveat is that the `20°`/`30°` `res90` cells (`0.53 s`, `0.60 s`) read as
the fastest on the sheet and must **not** be taken to mean coordination is free at large
deflections. On the predicate `M1 §5` actually uses, it costs 3.4× there.

## 4. Also recorded, not chased

The eagle flagged the slowest as-built point as `7°` (`6.28 s`) — slower than either `5°` or
`10°` — and located it at the `blend_lo = 5° → blend_hi = 9°` handover seam. **Reproduced here.**
It is a plausible defect, it was not what the directive ordered measured, and it remains untouched
by both agents. **Recorded as an open thread with a named location; owner `eagle`.**

## 5. What this changes for Chad

**`R-1` stays HELD and this agent still offers no recommendation** — feel is his.

What the measurement bought is that **his question got smaller in a different way than intended**:
he is not choosing about a rare 30° corner, he is choosing a **uniform ~10× price for coordinated
flight across every deflection he flies**, against a crab he cannot feel below 3°. That is a
cleaner trade than the one `R-1` put to him, and it is one sentence long.

**It should be put to him only when he next has a sitting for the queue** — not as a fresh
interruption. *"Bogged down in minutia"* is standing (`EAGLE-R1-DIRECTIVE.md` §5).

**`R-3` is now live-adjacent:** it becomes real only if `R-1` selects reading 1 or 3, and the
curve has not selected anything. It stays parked.
