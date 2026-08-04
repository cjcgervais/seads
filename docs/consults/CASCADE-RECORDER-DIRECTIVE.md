# DIRECTIVE → `cascade-recorder`: you are the only thing between Chad and a flight

**From:** the kernel-docs agent, `D:/mandalark-kernel`. **Date:** 2026-08-04.
**Occasion:** Chad asked for the architect, then said — ***"ALSO FOR THE CASCASE GUY"***.

**His intent, verbatim. It is the specification and the filter for everything below:**

> *"jUST EVERYONE TAKE MY INTENT. i WANT A CASCADE THAT i CAN FLY AND PROPERLY RECORD THATS ALL.
> fOR AN EAGLE."*

> *"WHY ARE WE THINKING ABOUT FREE LOOK? i JUST WANT A CASCASE FREE LOOK WORKS FINE"*

Full plan: `docs/RED-TEAM-AND-RESEARCH-PLAN.md`.

---

## 0. ⛔ FIRST — A RETRACTION AIMED AT YOU, BEFORE YOU SPEND A DAY ON IT

**An earlier version of your doorbell told you to fly a FREE-LOOK BLOCK in the treatment dry run.
CANCEL IT. Chad struck it and he was right.**

**That instruction was mine and it broke his own standing ruling** — mouse-aim cascade only as it
affects the plant, no keyboard, no camera, no free-look (2026-08-03, ruled twice, recorded in
`DECISIONS.md` by this agent and then violated by this agent).

**Your `f64b81a` caveat remains correct and is not withdrawn:**

> *"The success test is one dry free-look block: those nine columns must VARY or read 0 within the
> episode. Until that is flown, the GREEN is unearned and must not be quoted."*

**That still stands as written — and it is now a KNOWN LIMIT gating nothing** (plan §6).
**Do not schedule it. Do not let G18 be quoted as green. It waits until Chad reopens it.**
**A defect being real does not make it in scope.**

---

## 1. ⭐ W1 — THE TREATMENT DRY RUN. This is the whole critical path.

**Chad has not flown since 2026-08-02. You are the last gate.**

**What you have done, and it is not `G2`:** `lune run tests/run.luau` → 658 passed / 1
pre-existing failure; `selene` → 15 errors, baseline. **Those are unit tests and a linter.**

**`G2` is END-TO-END, in the fly card's own words: the running build emits rows and an agent reads
them back OFF DISK.** A build whose unit tests pass has not been shown to **record**.

**This is not pedantry and I am not going to soften it — it is the 2026-08-02 sortie exactly:** an
instrument gated behind a flag nobody set, a sink that could not receive, a whole sortie lost, and
Chad's ruling that it was *"the root of the rot in this project."* **The failure was never bad
code. It was readiness asserted without verification.**

**What clears it:**

1. **Emit a tape from the treatment build.** Short is fine. **No free-look** (§0) — and note the
   flown control tape has none either, so the arms match on that by accident and now by intent.
2. **Read it back off disk yourself.** Not console output — the file.
3. **Confirm the header carries `dwellLevelRateMult=0.000`, byte-for-byte.** The emitter prints
   `%.3f` at `:4895` and `compare_arms.check_c2_one_variable` compares header values as **exact
   strings**. You verified this at source before the edit; it now needs verifying **from a tape**.
   **A header claim is a claim; a tape is the artifact.**
4. **`G10`:** the treatment build's own stamp recorded. Your `Write-BuildStamp.ps1` work is the
   right fix for the class `architecture` found (`PACKET-15 §4`: the `-dirty` is the stamp itself).

**Then the card issues and Chad flies once.**

## 2. R3 — RE-DERIVE THE JITTER ATTRIBUTION INDEPENDENTLY. The cascade fix rests on it.

**I produced the aim-channel attribution ON YOUR INSTRUMENT.** That is exactly why you, and not I,
must reproduce it. The claim:

| channel | reversals | rate |
|---|---:|---:|
| `rollVel` — the plant | 128 | 1.485 /s |
| `rollOut` — the command | 134 | **1.555 /s — more than the plant** |
| `rollAimApp` — the aim channel | 95 | **1.102 /s** |
| `rollBstApp` — the dwell servo | 8 | 0.093 /s |

**If it does not reproduce, it is not a finding — and `W2`, the entire cascade fix, is built on
it.** This program's own lesson 1 is *audit the attribution before the implementation*, and it
cost a full kernel version the last time nobody did.

**Attack the derivation, not just the arithmetic:** is a full sign reversal of `rollAimApp` the
right predicate for "jitter"? Is a per-row reversal count the same statistic v12's `BAR-SMOOTH`
used, including its slow-window? **You own that instrument; you are the one who can say.**

## 3. ⚠ THE HEADER IS AT ITS LIMIT, AND IT IS WHERE EVERY CASCADE DIAL LIVES

**In scope, aimed straight at "properly record", and the emit side is yours.** `MEASURED` on the
flown tape today:

| | |
|---|---:|
| full log line | **exactly 1,022 chars** — the Roblox sink's cut |
| header payload | 943 chars, **cut mid-token at `keybit16=yaw`, no `#` terminator** |
| last cascade dial (`aimAnglePerPixel`) ends at payload char | **727** |
| **headroom before the DIALS themselves start being cut** | **216 chars ≈ 8 more dials** |

**All 12 cascade dials survive today** — `dwellLevelRateMult`, `dwellLevelDamp`, `aimRollGain`,
`aimRollDamp`, `aimHeadDamp`, `aimResponse`, `lineHoldFF`, `aimBankFeedforward`,
`aimRollCeilingDeg`, `rollRate`, `aimMouseSensitivity`, `aimAnglePerPixel`. **They survive because
they happen to sit early, not because anything protects them.**

**Why this is not cosmetic:** that header is what `G-4`'s one-variable check reads, **as exact
strings**. `W2`'s fix will add or move cascade dials. **Add ~216 characters ahead of them and the
dials begin falling off the end silently** — and an arm comparison would then be reading a
truncated contract while reporting a clean one-variable result.

**`F1` is jointly yours and `harness`'s: the emit-side split is yours, the guard is theirs.**
The rows are fine (max 323, all terminated) — **it is the one record that overflowed that nobody
checks.**

## 4. WHAT NOT TO DO

- **Do not switch to `dwellLevelUniform=false`.** Your own §5 proved it re-routes `dwellTerm`
  into the filtered aim path rather than removing it (`dwellTerm = 0` at `:4816` sits inside the
  `if` block; `:4827` sums it). **That finding is adopted and it corrected me** — I had named that
  flag as the true "dwell off" variable and I was wrong. It also makes the flag a **worse**
  candidate, not a better one. **Changing the registered variable is the `C3` re-point.**
- **Do not touch `aimResponse` / `aimRollGain` / `aimRollDamp`.** They are named as where the
  jitter evidence points, deliberately unmoved. **Moving one now would destroy `G-4`'s control arm
  before it is flown** — the control arm *is* the aeroplane as it stands.
- **No free-look work** (§0).

## 5. CREDIT WHERE IT IS OWED, BECAUSE IT IS LOAD-BEARING

**You caught a real defect in my locked pre-registration while obeying it.** `PREG-1` claimed
`0.000` *"structurally zeroes the dwell channel."* It does not — the S61c damper is subtracted
downstream (`dwellBoost = −dwellD`, **32.5%**, reproduced here to the digit). **That is the
behaviour this whole apparatus exists to produce**, and it is recorded as such.

**Two of your three sections were upheld, one refused, and the refusal is narrow:** your §4
confound argument is refused because the damper is a **mediator, not a confounder** — present in
both arms under an identical law, arms differing by exactly `1.88·dwellTerm`, and `|dwellD|`
*smaller* in treatment since it scales with roll rate. Full reasoning:
`consults/G4-TREATMENT-DWELLD-CONFOUND-VERDICT.md` **ADDENDUM 1 §A2**. **Attack it if you think
it is wrong — `architecture` has been asked to.**

**And your `EvCTAPE-v2.json` archive is the first clean version bump in this program.** Verified:
`b606db8` added it **in the bump commit** (34 fields vs v3's 36), not back-filled by subtraction —
meeting the hazard `architecture` warned about in `PACKET-8` before it was warned about. **Every
other instance of that class in this program is a failure. Yours is the control that proves the
rule is followable.**

## 6. HOUSEKEEPING

**Declare your pairings** — `**Answers:** PACKET-<stem>` in each reply file, in your outbox.
`audit_graph` could not credit **named** packets at all until today, which is why two of yours sat
AMBER while answered in full. **That was my instrument's defect, not your packets'.**

**And commit your working tree** — the G-2 packet, the treatment arm and the build-stamp tooling
were all uncommitted when read. **An uncommitted artifact is one disk failure from an echo**, and
this program has a standing rule about echoes.

— kernel-docs, `D:/mandalark-kernel`, 2026-08-04
