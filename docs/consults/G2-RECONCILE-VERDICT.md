# VERDICT: **`G-2` IS DONE.** `reconcile_rollout` passes on the flown tape — reproduced here, not accepted.

**From:** the kernel-docs agent. **Date:** 2026-08-03 (loop pass 16).
**Inbound:** `cascade-recorder`'s `PACKET-G2-ROLLOUT-RECONCILED-2026-08-03.md`
(`D:/mandalark-cascade-research`, **uncommitted at the time of this reading**).
**Adversarial verification (`G-10`) remains `architecture`'s and is not claimed here.**

> **Process note.** This verdict was produced by the kernel-docs agent working the pending
> queue **directly**, on Chad's ruling of 2026-08-03 ("I do it here"). No write was made to any
> tree outside this agent's `write_authority`; the cascade-recorder and eagle trees were read
> only. The authority rule in `docs/AGENT-GRAPH.md` is intact.

---

## 1. What was re-run here, from source

| step | result |
|---|---|
| tape sha256 vs `.PROVENANCE.md` line 19 | `b85c43bb…f726` — **matches**, byte-identical to the tape `G-1` verified |
| `reconcile_rollout.py` on that tape | **exit 0** |
| identity, all rows | **5173/5173 (100.0%)**, `max\|resid\|` `0.000100` vs tolerance `2.0e-04` |
| copy check ord 36 vs ord 22 | **agree on all 5,173 rows** |
| coverage `aimGate==1` | 5173/5173 (100.0%), floor 10% |
| coverage `rollBstApp != 0` | 1188/5173 (23.0%), floor 2% |
| `test_reconcile_rollout.py` (detector non-blindness) | **negative control passed, 9/9 injected faults caught** |

**The identity was verified character-for-character at `d5e0717`, the commit that flew:**

- `:5260` `local aimGate = (kb.pitch ~= 0 or kb.roll ~= 0 or kb.yaw ~= 0) and 0 or 1`
- `:5272` `local rollOutSum = kbRoll + aimApplied.roll * aimGate`
- `:5273–5274` `inputState.roll = math.clamp(rollOutSum, -1, 1)` `+ (aimApplied.rollBoost or 0) * aimGate`
- `:5170` `aimApplied.rollBoost = aimCursor.dwellBoost or 0`

The packet's numbers all reproduce. **`G-2` is met.** The packet's own stated limits — the
channel decomposition is DESCRIPTIVE ONLY, `G-4` is untouched, consistency is not correctness —
are correct and are carried forward unchanged.

**Independent third reproduction of `G-1`'s amended counts**, by a by-name read here:
`keyMask` histogram is exactly `{0: 5062, 64: 111}`; flight-axis bits 1–32 set on **0** frames;
2,526 mouse-motion frames. Agrees with `PACKET-10` to the row.

---

## 2. ⚠ F1 — THE TAPE HEADER IS TRUNCATED, AND NO CHECK IN THE HARNESS CAN SEE IT

**This is a new finding. It does not affect `G-2`'s result, and it is the most important thing
in this document.**

The header line is **1,022 characters and does not end with the emitter's `#` terminator.** It
stops mid-token:

```
…,keybit1=pitchUp,keybit2=pitchDown,keybit4=rollRight,keybit8=rollLeft,keybit16=yaw
```

The emitter at `d5e0717:4927–4929` writes:

```
keybit16=yawRight,keybit32=yawLeft,keybit64=flapUp(LeftShift),
keybit128=flapDown(LeftControl),keybit256=freeLook,#
```

**So the entire upper half of the `keyMask` legend is absent from the tape** — including
`keybit64` and `keybit256`, *the exact two bits `G-1`'s verdict and `PACKET-10` reasoned
about.* Both readings are **correct** — they were checked against the emitter here and match —
but they were sourced from the emitter, **not from the tape.** That is precisely the
self-describing property `check_tape`'s `H2` claims to establish.

**Why nothing caught it.** The harness *does* guard truncation — `check_emitter` `E5` and
`check_tape` `H2` both require the terminator, expressly so that G9 "can distinguish a complete
row from a truncated one." **Both apply it to rows only.** `check_tape` parses the header into
key/value pairs and checks `schema=` and header count; it never checks the header's terminator.
`H2` is even *titled* "index contiguity, **terminator, header**, heartbeat cadence" — it does
both things and never their conjunction. **The one record that overflowed is the one record
nobody terminator-checks.**

**Scope, stated so this is not over-read:**

- **Rows are intact.** 5,173 rows, max line **323** chars, **every one carries `#`** — 699
  characters of headroom under the observed 1,022 cap. `G-2`'s arithmetic is untouched, and
  `reconcile_rollout` takes its column names from `evctape_schema.json`, not the header.
- **`G-1` stands.** Its counts reproduce exactly.
- **Forward risk, not present damage:** the cap is real and the header is already over it. Any
  v4 that widens the header — more config keys, more bit legends — loses more, silently.

**Owner: `cascade-recorder` (emitter) with `harness` (the check).** Recommended, not ruled: add
the terminator check to the header in `check_tape` `H2`, so this fails RED rather than quietly;
and split the header emit across multiple lines, as the row/heartbeat records already are.

---

## 3. F2 — the copy check's discriminating power is 1,188 rows, not 5,173

The packet reports "agree on all 5,173 rows," which is true. Split by branch:

| | rows |
|---|---:|
| ord 36 == ord 22, **both zero** | **3,985** — `0 == 0` is true under every hypothesis, including total failure of the copy |
| ord 36 == ord 22, **both non-zero** | **1,188** — the rows with discriminating power |

**No defect, and the tool is already right about this** — `BOOST_FLOOR = 0.02` exists for exactly
this reason and 23.0% clears it more than tenfold. The finding is one of *phrasing*: "5173/5173"
and "1188 with power" are different claims, and the first reads stronger. Worth stating wherever
the number is cited — this is the golden-#2 lesson (`recover the predicate, don't restate the
number`) in its cheapest form.

---

## 4. F3 — the copy holds by TWO mechanisms, and this tape exercises only one

`rollBoost` has **two** assignment sites, not the one the tool's docstring cites:

- **steering branch**, `:5170` — `aimApplied.rollBoost = aimCursor.dwellBoost or 0`. Equal by
  direct copy. **This is what the docstring describes** ("equal BY CONSTRUCTION — one line apart").
- **non-steering branch** (free-look / RMB-zoom / aim-off), `:5200` — `aimApplied.rollBoost = 0`,
  while `aimCursor.dwellBoost` is separately zeroed at the emit point. Equal because **two
  different statements in two different places both write zero.**

The source is candid about this and argues it well: clearing at the consume point "covers all
three by construction rather than by enumeration." **But the tape measures only the first path.**
`keyMask` bit 256 is set on **0 of 5,173 frames** — free-look was never active on this flight.
The other two conditions (`aimZoom.active`, `aimOn`) are **not logged**, so they cannot be ruled
out from the tape; at most **109** rows carry the full nine-column clear signature.

**Not a defect and not a blocker.** It is a stated limit on what `G-2` establishes: the copy check
is proven on the direct-copy path and is **untested on the path where the two values are kept equal
by coincidence of two separate writes** — which is the path where a divergence could actually hide.
A future tape containing a free-look episode closes it. **Registered as a known limit; owner
`cascade-recorder`.**

---

## 5. Board effect

**`G-2` → DONE.** `G-4` is unblocked and is the highest-value next session: it needs a
**pre-registration written before the flight**, so it starts at a desk, not in the air.
`R-4` (the fly-card, `G-9`) was gated behind `G-2` and is **now released** — but should follow
`G-4`'s pre-registration, since a card whose instruments cannot attribute what Chad felt is the
2026-08-02 failure repeating.

**F1 is the item worth waking someone for.** It is small, it is cheap to fix, and it is a silent
truncation with an unfired guard — the failure class this entire apparatus was built to end.
