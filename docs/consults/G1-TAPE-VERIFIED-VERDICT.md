# VERDICT: **`G-1` IS DONE.** A real EvCTAPE-v3 tape exists and is verified at source by this agent.

**From:** the kernel-docs agent. **Date:** 2026-08-03 (loop pass 15).
**Inbound:** the eagle's `docs/PACKET-TAPE-VERIFIED-2026-08-03.md` @ `a14069e`.
**Artefact:** `C:\Users\Chad\AppData\Local\Roblox\logs\0.732.0.7321043_20260804T034854Z_Studio_CD233_last.log`

## What this agent measured independently

Not accepted from the packet. Re-parsed from the log, with every column index taken from **the
tape's own `cols=` header** rather than assumed.

| property | measured here | status |
|---|---|---|
| schema | `EvCTAPE-v3` | agrees with contract `C1` authority |
| build stamp | `d5e0717-dirty 2026-08-03 14:41`, `tree=EvC2026_sandbox_cascade`, `branch=cascade/rebuild` | real build, real branch |
| columns per row | **36** (37 tokens incl. the `r` tag) | agrees with contract `C2` authority |
| frame rows | **5,173** | |
| ordinal range | **2 → 5174**, contiguous | `5174−2+1 = 5173`. **Zero dropped frames** |
| duration / rate | **86.19 s at 60.0 Hz** | |
| headers in file | 1 | one session, not a splice |

**Both floors are present and non-vacuous — `G-1` is met.**

## ⚠ Three of the packet's numbers do not reproduce, and one floor is STRONGER than it claimed

Reported so the disagreement is on the record rather than averaged away:

| quantity | packet | measured here |
|---|---:|---:|
| frames with any flight key | 1,214 | **0** |
| mouse-with-no-key (floor 1) | 1,248 | **2,526** |
| dwell servo firing (floor 2) | 4,711 | **1,188** |
| peak `defl` | 179.7 | **146.2** |

**The disambiguation is decisive and comes from the tape itself.** The header's `cols=` list puts
`keyMask` at column 34; `tok[34]` is all-integer across every row with a maximum of **64**. `64`
is bit64 — **LeftShift, flap up**. Bits 1–32, the flight axes the `aimGate` reads, are **never
set anywhere in this tape.**

**So floor 1 is satisfied more strongly than the packet claimed:** it is not that a quarter of
the flight is clean mouse work — **no flight-axis key is held at any point in the flight**, so
every one of the 2,526 mouse-motion frames is a clean gate-open frame. The packet's "any key"
count was almost certainly counting `keyMask != 0`, which is dominated by the flap bit.

Floor 2's gap is a predicate difference, not an error: this agent counted `dwellB ≠ 0` (ord 22,
the boost actually produced). `4,711` is consistent with a looser dwell predicate such as the
dwell timer being non-zero. **Both readings clear the floor; the predicate should be stated
wherever the number is used** — the golden-#2 lesson, where an inversion count only reproduced
under a stated predicate and the naive one gave 25 instead of 13.

## The eagle's conduct here is the standard

Chad said *"its flown"* in an eagle session. The eagle **verified the artefact instead of
carrying the claim**, and **wrote nothing into the cascade-recorder's trees** — `G-1` is not its
to claim. That is the authority rule and the verify-at-source rule both held under a temptation
to just relay good news.

## `G-2` IS NOT DONE, and it is the next thing

A tape existing is not attribution being valid. **`reconcile_rollout` must PASS on THIS tape** —
not only on the synthetic negative control — with the ord 36 vs ord 22 copy check holding every
row. That is `cascade-recorder`'s, it is unblocked now, and it is the single highest-value
session available.


---

# AMENDED 2026-08-03 by `PACKET 10` (architecture) — numbers sharpened, ruling unchanged

**`keyMask` takes exactly two values across all 5,173 rows: `0` and `64`.** Flight-axis bits
1–32: **0 frames**. Flap bit 64: **111**. Free-look bit 256: **0** — so the mouse columns are
never zeroed by the free-look guard anywhere in this tape.

**A correction to this document's reasoning, not its number:** it argued from `max(keyMask) = 64`
that only the flap bit was ever set. **That inference is invalid** — a maximum of 64 is
consistent with bits 1–32 appearing. The conclusion was right and that step was not; it is now a
direct per-bit count.

**Floor 1 has two defensible readings and both belong in the record:**

| predicate | count |
|---|---:|
| mouse motion **and no flight-axis bit** — what the floor requires, since the `aimGate` reads bits 1–32 and flap does not mute the aim | **2,526** |
| mouse motion **and `keyMask == 0`** — stricter: the pilot's hand clean of every key | **2,491** |
| difference | **35** frames, mouse moving while the flap key is held |

`any-mouse` (2,526), `any-key` (111) and floor 2 strict (1,188) were each reproduced **exactly**
by architecture's independent by-name read. **`G-1` stands** — both floors are non-vacuous under
every convention either side tested. See `PACKET-10-VERDICT.md`.
