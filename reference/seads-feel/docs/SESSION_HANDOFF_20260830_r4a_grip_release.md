# R4a — THE LANDING RELEASE (launch doc, 2026-08-30)

**Work in `D:\seads_sandboxes\r4a-superman`, branch `sandbox/r4a-grip`.**
Base `f11a7e12a`, pushed. Gate **1731/1737**.

> ⚠⚠ **DO NOT WORK IN `D:\flight_sim2\seads-recon`.** That is Chad's FLY TREE — the
> tree he opens and drives. Sandbox branches run in `D:\seads_sandboxes\`, one dir
> per thread. The previous session built directly in the fly tree; Chad corrected
> it. This worktree already exists and is checked out for you.

---

## §1 WHAT IS SIGNED, AND THEREFORE FROZEN

Chad drove the arming memory and signed it, 2026-08-30:

> *"yea great job, he dosent let go of the bars yet but his legs fly sometimes, just
> the right amount of novelty suprise"*

**`buck_gain 0.02`, `decay_per_s 3.0` are SHIPPED DEFAULTS and are not yours to
move.** `SEADS_BUCK_GAIN=0` restores the pre-rung arm bit-identically — that is the
A/B and the kill switch, nothing else.

⚠ **THE PAIR IS SIGNED, NOT THE GAIN.** `decay 3.0` is set by the post-landing
recovery bound (p90 1.03 s over his 44 distinct drives), not by taste. The arm with
the BEST separation, `0.02/1.0`, leaves him limp for **20 s** after the worst landing
in the corpus. Move one without re-measuring the other and what he signed is gone.

---

## §2 THE RUNG — "HE DOSENT LET GO OF THE BARS **YET**"

His verdict names it. From the ruling that opened this thread:

> *"that second, unseated hit from the landing would then cause his grip to let go
> ... the hardness of the landing casue for letting go or the bars and falling off
> to the side ... maybe 10% of jumps given my normal driving"*

### The equation is measured and READY — do not re-derive it

    grip_load = landing hardness x extension from the pose at touchdown

**A PRODUCT, and the product is the whole mechanism.** A hard landing while he is
SEATED goes through his legs and the seat and he keeps hold — that is an ordinary
landing. The same landing while he is STRETCHED OUT has nowhere to go but his arms.
**A SUM would throw a seated man off, and a seated man is not who Chad described.**

**Capacity = `57.4`** at the shipped dial. That is the **p90 of his own measured
distribution** over the 44 distinct drives, so *"maybe 10% of jumps"* holds **by
construction**. It is not a number anybody picked.

Re-measure it any time with:

    ./build/seads_sled_probe.exe superman <tape>          # human-readable
    SEADS_PROBE_CSV=1 ./build/seads_sled_probe.exe superman <tape>   # all windows

⚠ The human-readable window table **stops at 12 rows**. Any corpus calibration must
use the CSV, or it is silently truncated.

### ⚠⚠ THIS ONE IS KERNEL WORK AND IT IS TAPED

LADDER §7.7 puts `grip_load_n`, `grip_capacity_n` and `rider_attached` in
`SledState`. Everything R0–R3 was render-side and structurally could not touch a
tape; **this rung can.** When he releases, `rider_mass_kg` (87.5) leaves the machine
— `cg_off` and `patch_geometry` both change, and a sled tumbling riderless is
physically a different machine.

**The shipping rule is explicit and is not optional:**

1. Ship with `grip_capacity_n` set so it **NEVER breaks.**
2. Prove the full existing golden set **BIT-IDENTICAL.**
3. **Only then** dial the capacity down, as a separate, re-taped change.

That is exactly how `assist_hull_frac`, `roll_stiff_vgain` and `release_floor_frac`
went in, and how the arming memory you are building on went in last night.

### ★ "FALLING OFF TO THE SIDE" IS LATERAL, AND NOTHING DOES THAT YET

Do not assume it falls out of the existing terms. **Measure it before designing it.**
The chain trails from the grips; a release has to produce a sideways departure, and
which side is presumably set by whatever asymmetry the buck and the machine's roll
already carry. That is a measurement, not a coin flip.

---

## §3 STANDING LAWS THAT BIT THIS THREAD (all in CLAUDE.md — read it first)

1. **THE SPEC IS READ AND FOLLOWED.** `docs/SUDBURIAN_LADDER.md` §7.3–§7.9 owns this
   rung. Conflicts get surfaced as ONE question, never re-derived past.
2. **THE GRAPH, NOT GREP.** `python tools/graph/graph_query.py …`; regenerate with
   `graphify.py` **in the same commit** as any structural change. If two branches
   both touch structure, `generated/graph/` WILL conflict on merge — the resolution
   is always *regenerate*, never a hand-merge.
3. **NO GUESSING.** Every number describing a real thing comes from a MEASUREMENT or
   from Chad's verbatim words. A gate turned green by tweaking constants is still a
   guess. A tape sweep BOUNDS candidates; **only his drive SELECTS one.**
4. **ONE DIAL PER DRIVE**, and he flies/drives every feel change.
5. **RED-TEAM IN A FRESH CONTEXT** before landing a mechanism; never self-review.
6. **PERSIST LESSONS TO `docs/lessons.md`** — a Process step the last session skipped
   and had to be reminded of. It is part of the loop, not paperwork.
7. **BUILD FOR HIM AND NAME THE ABSOLUTE PATH.** He opens the exe; you build it.
   `build/` = Debug, the gate. `build-play/` = RelWithDebInfo, what he drives —
   build the **`seads` target only** there, the test TUs `#error` outside Debug.

---

## §4 THE GATE — SIX REDS, AND ONE IS NOT OURS

Five long-standing: `probe P-F`, `sled_slides_before_it_tips`, `sled_grip_ceiling`,
`sled_assist_reference_plane`, `sled_debug_sink`.

The sixth is **`E12.1: the raider backfill keeps a faction's pump offense alive`**,
from the enemy-AI lane. **Attributed, not assumed**: their handoff says it was green
in their tree, so the whole source surface was reverted to `origin/main`'s inside the
merged tree, rebuilt, and re-run — still red. It arrives from main.

★ **MATCH REDS ON NAME, NEVER INDEX.** Indices shift whenever tests are added.

---

## §5 TRAPS THIS THREAD PAID FOR — the full set is in `docs/lessons.md`

- ★★★ **An instrument written before a mechanism keeps measuring the world without
  it, and stays green.** The probe had no pose spring for the entire life of the
  spring. Diff a rig against the shipped model TERM BY TERM.
- ★★★ **A column that is not printed reads as a ZERO, and a zero looks like a
  measurement.** Six `%.4f` for seven arguments nearly shipped the wrong dial.
- ★★★ **The constraint picks the dial, not the score.** The best-separation setting
  was unusable.
- ★★★ **A filter that protects a statistic buries the finding.**
- ★★ **Never quote a sweep before every arm has the same `n`.**
- ★★ **`core.autocrlf` converts on CHECKOUT, not on write** — a source-text test is
  green only in the tree that WROTE the file. MSYS `grep`/`cat -A`/`sed` lie about
  CRLF; byte-read or it is not evidence.
- ★ **Count your corpus.** 116 tape files are **44 distinct drives**.
- ★ **A locked exe silently fails the relink** — stop background runs before
  rebuilding, and `rm` the exe to prove freshness.

---

## §6 FIRST MOVES

1. Read `CLAUDE.md`, then `docs/SUDBURIAN_LADDER.md` §7.3–§7.9, then
   `docs/SESSION_HANDOFF_20260829_r4a_superman_instrument.md` §5 (the ruling and the
   calibration that produced the capacity).
2. `cmake --build build -j 12 && ctest --test-dir build -j 8` — confirm 1731/1737 and
   the six reds BY NAME before touching anything.
3. Measure the lateral question in §2 before designing the release.
4. Red-team the kernel design in a fresh context **before** it lands — `sim/` is the
   frozen kernel and this rung is authorized to touch it ONLY under §7.7's staging.
