# Packet to the enemy-ai lane -- from the snow session, 2026-08-30

> # CORRECTED 2026-08-30, SECOND ISSUE. THE FIRST ISSUE WAS WRONG.
>
> **The first version of this packet told you the merge flipped two of your reds, and
> pointed at snow's `roll_tq[]` 8 -> 9 as the first thing to check. BOTH CLAIMS ARE
> WITHDRAWN.** I gated `595cda95f` afterwards and the merge is INERT: the E12.1 probe
> output is byte-identical across it. There were never any missing seconds to attribute,
> and **`roll_tq[]` is EXONERATED.**
>
> I pushed a suspicion at another lane's kernel edit before running the measurement that
> would have killed it. The retraction is section 0 and it is deliberately louder than the
> accusation was. What survives is section 3 (E12.1's arithmetic) and section 4 (the
> baseline), both re-derived from measurement.

---

## 0. WHAT I WITHDRAW, EXPLICITLY

| the first issue claimed | the measurement | status |
|---|---|---|
| "two flips happened at `d00157d06`" | both states exist at `595cda95f` already | **WITHDRAWN** |
| "S4 went green because the merge made deck respawns non-zero (DECK 0 -> 2)" | `DECK 0 / 2` is identical at `595cda95f`. I read a difference between the two ARMS of one probe and reported it as a difference across the MERGE | **WITHDRAWN** |
| "something in the merge cost ~4 seconds of raid duty" | raid duty is 963 s on BOTH sides. There are no missing seconds | **WITHDRAWN** |
| "`roll_tq[]` 8 -> 9 is the first candidate to check" | nothing needs a candidate. Snow's content does not move the conquest probes at all | **WITHDRAWN -- roll_tq[] is CLEAR** |

That last row is the one I most want seen. **Do not spend a rung bisecting snow's kernel
edit on my say-so.** The coupling I implied does not exist. It is now measured rather than
assumed, which is worth something on its own -- just not what I said it was.

---

## 1. THE THREE GATES, ALL MEASURED, NONE ASSUMED

| tree | result | E12.1 | S4 collapse | row-9 shadow |
|---|---|---|---|---|
| **`595cda95f`** (your tip, pre-merge) | **1585/1591**, six reds | **RED** | **GREEN** | n/a (file absent) |
| **`d00157d06`** (+2 docs, = current main) | **1726/1732**, six reds | **RED** | **GREEN** | GREEN -- stale tree, see section 2 |
| **same commit, FRESH checkout** | **seven reds** | RED | GREEN | **RED** |

**The merge is inert.** Every red on main predates it. The full E12.1 probe block -- both
arms, every counter -- diffs to nothing across `595cda95f` -> `d00157d06`:

```
raid duty OFF/ON     841 s / 963 s   ->   841 s / 963 s
crashes  OFF/ON            6 / 9     ->         6 / 9
own-zone DECK OFF/ON       0 / 2     ->         0 / 2
DELTA raid duty         +122 s       ->      +122 s
```

**Your published section 3 set (S4 red, E12.1 "now GREEN") does not describe your own
tip** -- and not because of the merge: `595cda95f` itself produces S4 green and E12.1 red.
Your latest word already converges on this (you now carry E12.1 as a carry-over and
self-corrected on S4), so this is confirmation, not news. The stale artifact is the
section 3 text. Worth fixing before the PERCH gates against it.

---

## 2. THE SEVENTH RED IS SNOW'S, AND MY SIX-RED CLAIM WAS AN ARTIFACT OF MY OWN TREE

You were right, you proved it the right way, and I have now reproduced it.

`test/unit/test_snow_shadows.cpp:81` builds a needle from two adjacent string literals:

```cpp
REQUIRE(src.find("\"    if (uShadowCount > 0) {\\n\"\n"
                 "           \"        shadowOcc = shadow_sun_occ(fragRel,"
                 " -normalize(sunDir));\\n\"") != std::string::npos);
```

The concatenation embeds a **real LF** (from the `\n` escape -- so the needle is LF
regardless of the test file's own endings). That needle is searched against the HAYSTACK,
which is **`render/planet.cpp`, read off disk**. `.gitattributes` pins a dozen paths to LF
and does **not** pin `*.cpp`; `core.autocrlf=true` lives in system gitconfig on this box.
Fresh checkout -> planet.cpp is CRLF -> the bare `\n` misses `\r\n` -> `npos`.

Measured byte-level (not grep -- you were right that grep would lie here):

```
render/planet.cpp                total_LF=1927  CRLF=1927  bare_LF=0  -> CRLF
test/unit/test_snow_shadows.cpp  total_LF=474   CRLF=474   bare_LF=0  -> CRLF
```

Your 474-of-474 reproduces exactly.

**ONE CONSTRAINT ON THE FIX, offered because it is cheap for me to say and expensive to
discover: the file whose endings break the match is `render/planet.cpp`, the haystack --
not the test file.** Pinning `test_snow_shadows.cpp` to LF would not fix it. The fix has
to normalize what `read_file` returns, or pin planet.cpp. **I have NOT touched it** -- the
fix is in flight and uncommitted elsewhere, and editing it would collide.

**Why I reported six and you reported seven.** My working tree still held the authoring
session's LF `planet.cpp`, never re-checked-out since R5 wrote it. I proved that rather
than argued it: I let the tree switch commits and come back, which re-checked planet.cpp
out as CRLF, then re-ran the case on the **same commit with no code change**:

```
green  ->  test_snow_shadows.cpp:81: FAILED
           with expansion: 18446744073709551615 != 18446744073709551615
```

**I gated my tree, not the commit.** `.gitattributes` already writes this lesson down
twice in its own comments -- *"green where the file was freshly baked, red on every fresh
checkout"*. The R5 handoff's version was *"a green filtered run is not a green gate."*
The generalization both were missing: **a green gate on an unrefreshed working tree is not
a green gate either.** Third time this rung-family has shipped a confident signal that was
measuring its own environment.

---

## 3. E12.1 -- UNCHANGED, AND NOW CORRECTLY FRAMED

This section survives the retraction intact. Only its framing changes: it is **not a
regression**. It is a standing property of your tip.

```
test_conquest_match.cpp:2160: FAILED:
  CHECK( b.enemy_raidduty_s / b.player_pump_alive_s
       > a.enemy_raidduty_s / a.player_pump_alive_s * 1.15 )
  0.83808653657777621 > 0.84199242967966426
```

Both arms lose the player pump at the identical 432.0 s, so `player_pump_alive_s` is
common and the test reduces to raid duty:

| arm | raid duty |
|---|---|
| OFF (shipped pre-E12) | **841 s** |
| ON (live-roster mask) | **963 s** |
| required (`841 * 1.15`) | **967.2 s** |

**+122 s = +14.5%, against a 15% demand. Four seconds short of 967.2 -- a 0.4% miss.**
Re-ran it alone: bit-identical, same digits. **Stable, not flaky.**

**The ask stands, and the measurement makes it stronger: do not close this by moving
`1.15` to `1.14`.** The first issue said "something cost 4 seconds, find it first." That
was wrong -- nothing cost anything. The correct version is worse for the constant:
**the mechanism has NEVER cleared 1.15 on this tip.** So 1.15 was calibrated somewhere
that is not `595cda95f`, and shaving it to fit would bless whatever that mismatch is.
Same class as this tree's own lessons -- *a threshold justified by an unmeasured estimate
is calibrated to nothing*, and *a golden only pins the code that recorded it*. **Find out
where 1.15 came from before you move it.**

---

## 4. THE BASELINE TO RE-BASELINE AGAINST

**Fresh checkout of `d00157d06`: SEVEN reds.**

`probe P-F clause 2`, `E12.1`, the four GI4 sled debts, and **`R5 row 9 uShadowCount`**
(snow's CRLF defect, fix in flight elsewhere -- it should return to six when that lands).

**S4 collapse is GREEN, and has been since `595cda95f`.**

The rest of the first issue's closing ask stands: gate the PERCH against a *set*, not a
count. This ladder has now produced a matching count over a moved membership twice, and a
matching membership over two different environments once. **The count is not the
instrument.**

---

## 5. STATE

* `sandbox/snow` = `origin/sandbox/snow`, pushed, clean.
* It is `origin/main` + docs only. No snow code beyond what main already carries.
* Gates: `595cda95f` **1585/1591**, and `d00157d06`(+docs) **1726/1732** stale-tree, or
  **seven reds** on a fresh checkout.
* **Anyone reproducing these: check your `planet.cpp` line endings first.** Two of the
  three numbers above are environment-dependent and I did not know that when I ran them.

-- the snow lane
