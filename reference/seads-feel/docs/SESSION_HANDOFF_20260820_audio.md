# SESSION HANDOFF — THE AUDIO LANE: two pump animals, the snowmachine engine, and two rides

**Launch line for the next agent:** *"Read `docs/SESSION_HANDOFF_20260820_audio.md`; §7 says what is open."*

**Everything below is on `main` and pushed — run `git log --oneline --grep=audio`
for where it actually sits.** No hash is named here on purpose: two other lanes
were landing while this was written, and the enemy-AI and scarf lanes each shipped
a correction for putting a stale ``main = `hash` `` line in a handoff. Do not
reintroduce one.

**Gate: judge it by WHICH tests fail, never by the total.** The total has moved
twice since this work landed (1395/1399 here, 1516/1520 reported by the character
lane days later) and it will move again. The only reds that belong to anyone are
the four known GI4 sled debts:
`sled_slides_before_it_tips_on_flat_snow`,
`sled_grip_ceiling_stays_below_the_tip_threshold`,
`sled_assist_reference_plane_is_load_weighted`,
`sled_debug_sink_is_write_only`.
**Any new failure NAME is a finding, not a number to accept.** None of them are
audio, and stashing this work and rebuilding was used to prove that.

---

## 0. ⚠ WHERE TO WORK — read before you `cd` anywhere

**This lane's tree is `D:\seads_sandboxes\audio`** (branch `sandbox/audio`,
created off `main` at the end of this session, soundbank already installed).

**`D:\flight_sim2\seads-recon` IS NO LONGER THIS LANE'S TREE.** The audio work
was done there, but the character/scarf lane has since taken it over: it sits on
`main` with ~38 uncommitted files under `assets/character/sudburian_src/`, and
they gate against its `build/`. Building or committing there will collide with a
live agent. The enemy-AI lane shipped a correction for making exactly this
mistake in their own handoff — do not repeat it a third time.

Before touching any tree, check who else is in it:

```
git worktree list                 # who is where
git -C <tree> status --short      # someone else's uncommitted work?
```

**What a missed collision looks like:** not a merge conflict, but a build that
fails to link — after which the test run is silently meaningless.

**A fresh tree has NO audio and is SILENT, by design.** The 54 MB soundbank is
deliberately outside git history (Chad: "code and docs only if its a one way
decision"), so every loader is guarded and prints on a miss. Deploy with:

```
bash tools/audio/install_soundbank.sh <tree>
bash tools/audio/install_soundbank.sh --check <tree>    # verify, write nothing
```

⚠ Only install into a tree whose `.gitignore` has the three folder rules
(`assets/audio/{music,sfx,loops}/`). This session deployed into
`D:\flight_sim2\seads` — on the older `sandbox/fields-forge` — and the
installer's own check caught that those rules were absent there, leaving 38 MB
**unignored** and one `git add -A` from being permanent. It was backed out. The
check is not decoration; read its output.

---

## 1. WHAT LANDED

Three rungs, all flown by Chad except where §7 says otherwise.

| rung | what | where |
|---|---|---|
| **The two pump animals** | a wolf at the Sudbury surface pump, a wildcat at the Onaping/Levack one, on approach from air or sled | `render/pump_ambience.h` (new, pure), glue in the MUSIC-bus block of `app/main.cpp`, `test/unit/test_pump_ambience.cpp` (29 cases) |
| **The snowmachine engine** | idle bed + riding bed + brap accent + two-stroke tone | `render/sled_audio.h` / `sled_drive.h` / `sled_synth.h` — **all pre-existing**; this session wrote the app glue that was missing |
| **Two pitch/level rides** | Chad rode it twice; the engine came down two octaves and −6 dB | `kSledVoiceTranspose`, `kSledFlownLevel` |

New assets: `sfx/wolf_cry.wav`, `sfx/wildcat_cry.wav`. Sources tracked in the
audio lane repo (`D:\audio_tracks`, remote `seads_audio_lane`).

### The animals are bound to PLACES, not factions
Chad named the pumps by allegiance ("the enemy surface pump in sudbury"), but he
described two geographies. `[conquest] player_faction` is a config switch — keyed
to allegiance, the wolf and cat would swap ends of a 42 km map when one line in
`game.toml` changed. They are keyed to `world::kPumpSudburySurface` /
`kPumpValleySurface`.

Distance is **great-circle along the ground** off the baked unit directions, not
3D range and not the lifted `combat::make_pumps` positions: it works with
`[conquest]` off, it is altitude-blind (which is what "fly over the area" means —
a 3D range holds the wolf silent directly *above* the pump), and it costs two dot
products.

⚠ The "~30.8 km" in `faction_bubbles.h` is each pump's distance to the **enemy
centre**. Pump-to-pump is **42.2 km**. The first draft used the wrong one.

### The sled synth was finished code with no caller
`render/sled_synth.h` was written, unit-tested and red-teamed on 2026-08-17 and
`app/main.cpp` contained **zero references to it** for three days, while the
manifest rows said `wired=no`. Chad's report was "the last agent failed to build
those sounds in" — nothing needed building. **If a manifest row says `no`, check
whether it is waiting on a DECISION or just on a caller.** (§4 is the inverse of
this bug, still open.)

---

## 2. THE DIALS — every unheard knob, and which one to move

The gate never runs `seads.exe`, so **none of this is proven by green**. All of
it is code constants with comments. One dial at a time; Chad flies each.

| symptom | move THIS | where | now |
|---|---|---|---|
| engine still too high | `kSledVoiceTranspose` (0.25 = a third octave) | `render/sled_audio.h` | 0.50 |
| engine too loud/quiet | `kSledFlownLevel` (0.31 = −3 dB back down; 0.62 = +3 dB up, his first guess, once ruled too loud) | `render/mix_levels.h` | **0.44** (raised from 0.31 on 2026-08-23, “raise snowmachine sounds up” — the +3 dB this row pre-computed) |
| engine LAYERS wrong against each other | `kSledMaster` | `render/sled_audio.h` | 0.78 |
| idle bed missing (not just deep) | lift the idle alone — see §3 | | |
| animals fire too early/late from the air | `kCryRangeFlyingM` | `render/pump_ambience.h` | 3200 m |
| animals fire too often while parked | `kCryGapsGround[]` | `render/pump_ambience.h` | 41–78 s |
| animals too loud/quiet | `kCryGroundGain` / `kCryFlyingGain` | `render/pump_ambience.h` | **1.30 / 1.05** (raised 2026-08-24 from 0.92/0.60) |
| animals never heard AT ALL | `kCryRangeFlyingM` — the bubble you have to fly into | `render/pump_ambience.h` | **5000 m** (was 3200) |
| train never heard from the air | `kTownRadiusFlyingM` — one 92 m index cell is a sub-second event at 110 m/s | `render/town_ambience.h` | **4000 m** (new 2026-08-24) |
| train too loud/quiet | `kTrainGroundGain` / `kTrainFlyingGain` | `render/town_ambience.h` | **1.20 / 0.95** (raised 2026-08-24 from 0.90/0.62) |
| train too often/rare | `kTrainGapsFlying[]` | `render/town_ambience.h` | **130–225 s** (was 255–470) |

**`kSledSourceCorrection` (0.50) is NOT a taste dial** — it is a measurement of
how wrong the two recordings are, and it is deliberately separate from
`kSledVoiceTranspose` so that "which half of this is the taste?" has an answer.
Do not move it for an ear ruling.

**To measure instead of guess**, use the tool this session added:

```
g++ -std=c++17 -O2 -I. -DSEADS_ASSET_DIR='"<tree>/assets"' tools/sled_synth_preview.cpp -o build/sled_preview.exe
./build/sled_preview.exe 0.0 build/sled_idle.wav     # held closed
./build/sled_preview.exe 1.0 build/sled_full.wav     # held pinned
```

It renders the real `render::SledSynth` and prints the drive state. Follow the
same convention as `wind_synth_preview.cpp` — standalone, one `g++` line, no
raylib, no CMake target. Measuring the output turned "how high, exactly" from an
argument into a number in seconds.

---

## 3. ⚠ THE ONE MEASURED RISK, UNCONFIRMED BY EAR

Band-splitting the rendered engine after the second octave:

| | below 50 Hz | above 50 Hz | above 100 Hz |
|---|---|---|---|
| riding (throttle 1) | −39.4 dB | **−18.3 dB** | −18.6 dB |
| idle (throttle 0) | **−25.0 dB** | −29.0 dB | −36.4 dB |

The ride is healthy. **The idle bed now has more energy below 50 Hz than above
it**, so a speaker rolling off under ~150 Hz keeps the ride and loses most of the
idle. On monitors it is a deep burble, which is right.

If Chad says the idle is **missing** rather than deep: do **not** undo
`kSledVoiceTranspose` — the ride would go back up with it. Lift the idle bed
alone. Its own honest source correction is 1700/2800 = **0.61**, not the 0.50 it
currently shares with the run bed, which means splitting `kSledSourceCorrection`
per asset.

---

## 4. 🔎 OPEN FINDING — three assets claim `wired=yes` and have NO caller

Verified against `origin/main` at the time of writing:

| asset | references in source |
|---|---|
| `loops/player_breathing_loop.wav` | **none anywhere** |
| `sfx/train_chelmsford_pass.wav` | only a **comment** in `render/town_ambience.h` |
| `sfx/intro_signal_rumble.wav` | **none anywhere** |

All three are mastered, levelled, in the bank and in `MANIFEST.lock`, and the
manifest says they are in the game. They are silent.

This is the **same class of bug as the sled loops, inverted** — that column is
unverified prose in both directions. The train is the ripest: `town_ambience.h`
is a complete, tested policy header (`TrainAmbience`, cadence, gains) waiting on
one app-side building-density lookup, described in
`docs/soundscape_handoff.md` §5.

**Worth doing first:** make the column honest, ideally with a test that fails
when a row says `yes` and no source file names the asset. That would have caught
both directions of this bug automatically.

---

## 5. THE PIPELINE — two repos, and which one owns what

| | repo | holds |
|---|---|---|
| **sound lane** | `D:\audio_tracks` → `seads_audio_lane` | the SOURCE recordings + the built bank + `soundbank_report.txt` |
| **game** | this repo → `seads_sandbox1` | `tools/audio/soundbank.manifest.tsv` (the single authority) + the engine code |

Adding a sound: drop the source in `D:\audio_tracks\sound_effects\`, add a
manifest row **in the game repo**, `bash tools/audio/build_soundbank.sh`, then
`install_soundbank.sh <tree>`. **Commit the source in the audio lane** or the
build fails for everyone but the box that has it.

**The build gate is real and it caught a genuine defect this session.**
`cave_hiss.wav`'s full 8.3 s decay measures LRA 22.8, over `LRA_TARGET` — which
makes `loudnorm` fall back from LINEAR to DYNAMIC *silently*, and the pipeline's
`normalization_type` assertion refused the asset. Dynamic was the wrong answer
(gated leveling would ride that decay back up, and the decay IS the sound), so
the row is cut `0:7` → LRA 13.0, discarding only material that lands near
−65 dBFS in game. **When the build refuses an asset, read why before changing the
target** — its own "max target reachable LINEARLY" hint assumes a POSITIVE gain
and does not apply to a row normalising downward.

---

## 6. THE LESSONS THIS SESSION PRODUCED — both are about assertions

**1. A `static_assert` encoding a guess outranks nothing.** I wrote
`static_assert(kSledStreamLevel > kGunStreamLevel)` — "a vehicle engine must sit
above the guns". That was an *argument*, welded into the build. Chad's first ride
took the engine straight past it, and the assertion's only effect was that the
ear which disagreed had to fight the compiler. It is retracted, **left visible**
in `mix_levels.h` rather than deleted, and replaced by a floor that is a real
property (the machine you ride must not be quieter than the music behind it).

**2. The same mistake in a unit test is worse.** The first octave landed every
layer exactly on the derived firing rate (`rpm/60 × 3`) and I wrote a test
asserting that. When Chad asked for the second octave, **my own test failed the
change he asked for.** `CLAUDE.md` rules this project by FEEL, NOT FIDELITY —
the derivation was fidelity. Rewritten (not relaxed) to assert the property that
actually matters: all layers sing at the *same* transposed pitch, whatever it is.

**Three more tests were caught pinning a constant that a correct change moved** —
`sign_changes > 4` (silently coupled to playback pitch), the ratio bounds
(`> 0.5` flat), and `sled_tone_cutoff`'s endpoints. All now derive from
`kSledVoiceTranspose` / `kSledPitchPedestal`, so a third octave moves one number
and the tests follow instead of failing on correct work.

**Rule of thumb for this lane:** assert what must hold for the code to be
correct. A balance nobody has heard yet belongs in a comment.

**One more, about transposition:** resampling a recording moves its whole
spectrum for free; a synth layer does not. When `kSledVoiceTranspose` moves, it
must carry the tone's fundamental **and** its lowpass sweep — otherwise you get a
thin bright harmonic stack over a low note, which reads high however low the
fundamental goes.

---

## 7. WHAT IS OPEN

**Ranked. The first item is the whole reason to read this file.**

1. **NOTHING SINCE THE SECOND OCTAVE HAS BEEN HEARD.** Chad rode the engine
   twice (→ two octaves down, −6 dB) and then asked for this handoff. He has
   **not** reported on the second octave, and — separately — **he has never said
   anything about the wolf or the wildcat at all.** Do not treat either as
   accepted. Start by asking him to fly and ride.
2. **The idle-audibility risk in §3**, if he reports the idle missing.
3. **The three `wired=yes` liars in §4**, and a test to keep that column honest.
4. **The train**, which is 90% built (`render/town_ambience.h`) and blocked only
   on the app-side building-density lookup — `docs/soundscape_handoff.md` §5.
5. **`sim::SledState::engine_rpm`**, deliberately not used. It is commented
   "Readout for dash/audio ... while the clutch is engaged". Feeding it would add
   real CVT load coupling — the engine bogging in deep snow, free-revving on a
   slipping track — instead of the synth's throttle-driven rev follower. It is a
   contract change to `SledDrive` and wants its own pass and its own fly.
6. **Footsteps** (`sfx/step_*.wav`), `wired=no` and correctly so: Chad, "my
   sudburian cannot walk yet so defer for now the walking sounds".
7. **The explosion while riding** — his spec asks for it outside tunnels too.
   The old blocker ("no snowmachine in this tree") is stale; the sled is here.
8. **`docs/soundscape_handoff.md` has stale rows** — it still claims the sled
   audio headers are unwired and that there is no snowmachine in this tree. §2/§3
   were corrected for the parts this session touched; the rest was not audited.

---

## 8. FILES THIS LANE OWNS

```
render/pump_ambience.h        the two animals          (pure, <algorithm> only)
render/sled_audio.h           engine constants + the two pitch dials
render/sled_drive.h           rev follower, onsets, sustain
render/sled_synth.h           the real-time render
render/mix_levels.h           the whole gameplay balance on one screen
render/town_ambience.h        the train — built, NOT wired
render/music_director.h       music bus, bed select, blast duck
render/stope_reverb.h         the stope echo
app/main.cpp                  ALL the glue (load block, per-frame, teardown)
tools/audio/                  build_soundbank.sh, install_soundbank.sh, the manifest
tools/sled_synth_preview.cpp  offline render — the only way to hear anything headlessly
test/unit/test_pump_ambience.cpp, test_sled_audio.cpp, test_soundscape.cpp
docs/audio_handoff.md         the LIVE running log — sessions 1-6 are in it
```

`docs/audio_handoff.md` is the deep record: every measurement, every retracted
argument, every number's derivation. This file is the map; that one is the
territory.

---

## 9. THE FIREWALL — unchanged, and it is why audio can move fast

Everything in this lane is **cosmetic and read-only off the render snapshot**.
No audio symbol is included by `sim/` or `control/`; nothing here writes state.
That is why an audio rung cannot move a golden, and it is the property to
preserve. Audio channels are also skipped in smoke (`smoke_frames > 0`) and
whenever the device fails to open — headless CI has no audio device.
