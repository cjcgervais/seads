# SESSION HANDOFF — THE AUDIO LANE: the Chelmsford church bell

**Launch line for the next agent:** *"Read `docs/SESSION_HANDOFF_20260824b_audio.md`; §5 says what is open."*

**This file adds one channel. It supersedes nothing.** `docs/SESSION_HANDOFF_20260824_audio.md`
is still the current map for the train, the animals, the sled and the mix —
especially its §0 (where to work), §3 (the dial table) and §7 (what is open).
This file adds the bell's rows to that table and nothing else changes.

`docs/audio_handoff.md` **§ Session 7** is the deep record: every measurement,
every derivation, and the two places this channel deliberately parts company
with the train next door.

---

## 0. ⚠ WHERE TO WORK

**`D:\seads_sandboxes\audio`, branch `sandbox/audio`.** Unchanged, and the 08-24
handoff's §0 warning is unchanged with it: **`D:\flight_sim2\seads-recon` is not
this lane's tree and its branch moves under you.** Run `git branch --show-current`
immediately before every commit, never once at the start of a session.

---

## 1. WHAT LANDED

Chad: *"please make it play different amounts of plays at random, sometimes it
rings 3 times, sometimes 7 times, up to twelve to simulate the number of chimes
it plays based on the hour ... about two minutes after the train might, in and
over chelmsford"* / *"from one chime up to 12, 3 was an example, it should be
random though."*

| piece | where |
|---|---|
| the policy — count, gap, delay, gains, ring size | `render/bell_ambience.h` (new, pure) |
| the asset | `sfx/church_bell.wav`, 4.44 s mono, AMBIENCE −24 |
| the glue | `app/main.cpp`, inside the existing town block next to the train |
| the tests | `test/unit/test_bell_ambience.cpp` |
| the source | `D:\audio_tracks\sound_effects\church_bell.wav` (committed there) |

**How it behaves:** a train passes → ~120 s later the bell strikes a randomly
drawn 1–12 times at 2.4 s intervals, gated on the same town density and the same
per-context radius the train uses, and muted under rock.

---

## 2. THE TWO THINGS THAT ARE NOT LIKE ANY OTHER CHANNEL

**1. It is POLYPHONIC, and it has to be.** The bell rings 4.44 s and is struck
every 2.4 s, so strikes overlap. Every other one-shot in this bank is a single
raylib `Sound`, where `PlaySound` **restarts** a playing voice — here that would
truncate a ringing bell up to eleven times a peal. The strikes go round a ring of
four `LoadSoundAlias` voices (raylib 5.5; they share the sample data).

`kBellVoiceCount` is `static_assert`ed against `ceil(len/gap) + 1`. **A re-cut of
the manifest row, or a shortened gap, fails the build** rather than silently
bringing the truncation back. If you change either number, that assert is the
thing that will stop you, and it is right to.

**2. The count is DRAWN, not tabled** — the one place this parts company with
`render/town_ambience.h`. That header argues, correctly, that a fixed four-gap
table reads less mechanical than a uniform draw. **That argument does not
transfer**: a train gap is not counted by anyone, and a chime count is the entire
content of the sound. A table would ring the same twelve hours in the same order
forever, and a player hears that inside one session.

It uses `render::WSXor` from `audio_dsp.h` — the **fixed-seed** xorshift the gun
and wind synths already draw from. Same sequence every run, so the gate can pin
it; long enough that nobody hears it repeat. `<random>` is still banned and still
not needed: *unseeded* was the thing that was banned, not random.

---

## 3. THE DIALS — add these rows to the 08-24 table

| symptom | move THIS | where | now |
|---|---|---|---|
| bell too loud / quiet | `kBellGroundGain` / `kBellFlyingGain` — ⚠ read §4 before raising | `render/bell_ambience.h` | **2.35 / 2.00** |
| chimes too fast / too slow together | `kBellChimeGapSec` — ⚠ moves the alias-ring assert with it | `render/bell_ambience.h` | 2.4 s |
| bell too soon / too late after the train | `kBellDelaysSec[]` | `render/bell_ambience.h` | 105–135 s (mean 120) |
| peals too long | `kBellChimeMax` | `render/bell_ambience.h` | 12 |
| a single bong reads as a bug | `kBellChimeMin` — it is one o'clock, and Chad ruled it in | `render/bell_ambience.h` | 1 |
| bell never heard at all | it is keyed to the TRAIN — check `kTownRadiusFlyingM` first | `render/town_ambience.h` | 4000 m |

**`kBellChimeLen` (4.44) is NOT a taste dial.** It is the measured length of the
shipped asset and it sizes the voice ring. Move it only when the manifest row's
cut moves.

---

## 4. ⚠ THE LEVEL IS AGAINST A CEILING, NOT AGAINST TASTE

The bell's trim sits 3.4 dB under the signed train's, and that is **not** the
bell arriving quieter — the asset is 3 dB peakier to start with. Measured:

| | asset TP | trim | delivered peak |
|---|---|---|---|
| train pass (signed by ear) | −11.8 dBTP | 3.40 | **−5.2 dBTP** |
| one bell strike | −8.8 dBTP | 2.35 | **−5.4 dBTP** |
| two bell strikes overlapping | | | **−3.3 dBTP** |

It is peak-matched to the train Chad flew and called right, and it only reaches
the −3.0 dBTP house line (the same one `kCryCeilingWolf` / `kCryCeilingWildcat`
are derived against) when two strikes coincide.

**So if he says it is too quiet, raising the trim is the wrong move** — it buys
loudness by clipping the attack, which is the part of a bell that carries. In
order of honesty:

1. Widen `kBellChimeGapSec` past `kBellChimeLen` so strikes stop overlapping.
   Buys the whole 2.3 dB back; costs the tower's ring, which is most of what
   makes it sound like a tower.
2. Re-master the row to a lower true peak and raise the trim against it.

---

## 4b. THE GATE — 1550/1556, and all five reds are inherited

`ctest --test-dir build -C Debug`, 1556 tests, **29 min**. Judge it by WHICH
tests fail, never by the total:

* the four known GI4 sled debts (`sled_slides_before_it_tips_on_flat_snow`,
  `sled_grip_ceiling_stays_below_the_tip_threshold`,
  `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`)
* `probe P-F: the relentless raider keeps the pump and shoots back` — the
  enemy-AI red the 08-24 handoff §4 already wrote down as unowned. Still unowned.

**Nothing audio is red.** Note the Debug build here had to be configured with the
staged deps or it stalls fetching raylib:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=D:/flight_sim2/seads-recon/build/_deps/raylib-src \
  -DFETCHCONTENT_SOURCE_DIR_CATCH2=D:/flight_sim2/seads-recon/build/_deps/catch2-src \
  -DFETCHCONTENT_SOURCE_DIR_GLM=D:/flight_sim2/seads-recon/build/_deps/glm-src \
  -DFETCHCONTENT_SOURCE_DIR_TOMLPLUSPLUS=D:/flight_sim2/seads-recon/build/_deps/tomlplusplus-src
```

### ⚠ One of MY tests was red first, and it was the test that was wrong

`a second train mid-countdown does not stack a second peal` failed `2 == 1`. The
first draft ran a flat 300 s window and counted peal starts — but **once a peal
COMPLETES the tower is free again and the next train is entitled to arm another
one**, which is the channel working, not stacking. The window is now bounded by
the first peal's last chime, which is exactly the interval the property is about.

Worth writing down because it is the *inverse* of the mistake this lane keeps
making. Usually the assertion encodes a belief and the code is right. Here the
assertion encoded a **sloppy window** and the failure was real information: it
said the scheduler recovers, which is behaviour nobody had asked for out loud.

---

## 5. WHAT IS OPEN

1. ⭐ **HEARD AND SIGNED, 2026-08-24, on the first listen** — *"ookay bell sounds
   good!"* and, when I hedged that only the peal had been approved and not the
   mix, *"I heard it, it passes my review so thats why I said to push it."*

   So `kBellGroundGain` / `kBellFlyingGain` are **ridden numbers**, not derived
   ones, and `render/mix_levels.h` ruling 3 now covers them: do not retune them
   to make a policy tidier. This channel went from written to signed in one pass,
   which is the first time that has happened in this lane — every other one took
   between two and four flies. **The difference is that its level was derived
   against a MEASURED CEILING rather than against an argument about what the
   sound deserves**, and the two assertions this lane retracted were both of the
   second kind. That is the thing to copy, not the luck.

   ⚠ What is still unproven is everything a single listen cannot judge: the
   **cadence** (a ~120 s delay against 105–225 s train gaps is not something one
   session hears), and whether the **count distribution** reads as hours over a
   long ride. Both are dials in §3, and neither is a fault if he raises it.
2. **The delay is only reachable via the train**, so anything that stops the
   train firing stops the bell. If he reports the bell missing, check the train
   first — that is the 08-24 handoff's §2 measurement, not a new fault.
3. **`kBellChimeMin = 1`.** A single bong is one o'clock and Chad ruled the floor
   explicitly, but it is the value most likely to read as a bug on first hearing.
   If he calls it one, raise the floor rather than explaining the clock.
4. Everything in the 08-24 handoff's §7 is still open and unchanged.

---

## 6. FILES THIS SESSION TOUCHED

| file | what |
|---|---|
| `render/bell_ambience.h` | **new** — the whole policy, pure |
| `app/main.cpp` | include, the alias ring + `BellTower`, the load block, the per-frame trigger, `abandon_peal()` on the cave branch, teardown before the source |
| `test/unit/test_bell_ambience.cpp` | **new** |
| `CMakeLists.txt` | registers the test |
| `tools/audio/soundbank.manifest.tsv` | the `sfx/church_bell.wav` row |
| `assets/audio/MANIFEST.lock` | 19 entries |
| `docs/audio_handoff.md` | § Session 7 — the running record |

And in the sound lane (`D:\audio_tracks`, a **separate repo**):
`sound_effects/church_bell.wav` + `build/soundbank_report.txt`. Assets do not
travel by git between trees — use `install_soundbank.sh`.
