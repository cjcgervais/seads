# Soundscape handoff — LIVE

**Branch `sandbox/audio` in `D:\flight_sim2\seads-recon`** (off main).
✅ COMMITTED AND PUSHED 2026-08-17 at `bd8e043c3`, on Chad's word ("okay it worked out
great committ and push") after he flew it.
✅ **FAST-FORWARDED TO `main` 2026-08-18 on his ruling** — the rung had been sitting on
a sandbox branch while every other active tree (winter-gi, winter-SF2, barrens) had no
audio at all. `main` is now at `26bac176c` and pushed.

⚠ THE AUDIO ASSETS ARE NOT IN THE COMMIT -- his call, asked and answered: "code and
docs only if its a one way decision" (54 MB is permanent in git history, and the audio
lane belongs to the human sound engineer). Every load is guarded and prints once on a
miss, so a fresh checkout is SILENT, not broken.

**→ Read `docs/README_SOUNDSCAPE.md` for the standing reference** (the two lanes, the
loudness policy, the gitignore trap, how to deploy). Deploy with:

```bash
bash tools/audio/install_soundbank.sh D:/flight_sim2/seads-recon
```

## 🔧 LANE CLEANUP, 2026-08-18

The rung shipped correct but its infrastructure was not, and Chad ruled all of it:

- **The pipeline moved into this repo** at `tools/audio/` — it had lived only as loose
  files in `D:\audio_tracks` with no version control at all, while being the input spec
  for the 5,652 lines this rung added. `D:\audio_tracks` is now its own git repo (text
  tracked; the ~127 MB of source media deliberately not — see its `.gitignore`). One
  authority, not two: the lane keeps the media and a pointer README.
- **`assets/audio/MANIFEST.lock` is now committed** — sha256 + size of all 16 assets.
  Nothing in git previously said *which* files a tree should have, so a silent checkout
  could not be told from a lost asset. `install_soundbank.sh --check` verifies it.
- **The `.gitignore` trap is fixed.** `assets/audio/*.wav` was a single-level glob that
  silently ate every loose WAV. Now scoped to `assets/audio/bench_*.wav`, with the bank
  ignored by three exact folder rules and the lock outside them. The installer asserts
  all three invariants; the check was verified by reintroducing the old rule and
  watching it fire.
- **The bagpipe drone is in the bank.** It had been loaded by `app/main.cpp` since
  2026-07-24, never committed (that glob), never in the manifest, and present in 2 of
  25 worktrees — the aircraft's throttle voice was silent for everyone else for three
  weeks. Now `loops/bagpipe_drone.wav`, mastered from the freesound original with its
  recording provenance intact, and the loader prints on a miss like every other channel.
- **The pipeline is now actually reproducible** — 16/16 byte-identical across a
  `--clean` rebuild. It had claimed determinism while the three `.ogg` assets got a
  random Ogg serial number every build (840 bytes differing, payload identical).
  `-fflags +bitexact` fixes it.
- **`docs/README_SOUNDSCAPE.md` written** — the manifest had cited a §3 that never
  existed.

**The bagpipe drone's level is SETTLED, not open.** It ships at **−10.3 LUFS**, ~8 LU
above every other SFX asset, because that is the level in the mix Chad flew and signed.
The −18 house target is a policy for assets nobody has judged yet; this one has an
approval, which outranks a policy. Moving it would be a blind 8 dB change inside an
approved mix and would cost him a re-fly to re-approve. **Nothing else in the mix
moved.** If he ever says the drone is too loud, move `kEngineStreamTrim` in
`render/mix_levels.h` rather than the manifest row, so the mastered asset stays
comparable to the rest of the bank.

Gate as of handoff: **1190/1190 green**, graphify current, layering OK.

## ⭐ WHAT LANDED THIS SESSION — both unbuilt rulings are now BUILT

1. **The intro is restructured TITLE FIRST** (ruling A below). The title card is now
   the LOADING PAGE, with the music loud and the world building behind it; then black
   (music fades out, wind rises); then the phone call; then gameplay with the bed
   swelling back in quieter. `render/intro_sequence.h` + `app/main.cpp`.
2. **The stope echo exists** (§4 item 2, the largest gap). `render/stope_reverb.h` — a
   Schroeder/Moorer network sized for a blasted chamber — on the gun, sfx and engine
   streams, gated on the same `app::inside_tunnel` predicate the camera and the music
   bed read. Chad: "Make all sounds like guns echo when in the stope as well."

**Verified by RUNNING the binary, not only by the gate** (`SEADS_LOADPROF=1
.\build-play\seads.exe`): title lit 1.6 s BEFORE the build starts, page up ~24 s
total, then cues at t = 8.00 / 37.40 / 41.90 / 46.40 / 50.90 — the designed timeline,
end to end.

**Measured, and the reason the loading page has music at all:** the world build is one
~22 s uninterruptible call (`render::planet_heightfield` → `load_planet`) out of a ~23 s
total load. ⚠ An earlier version of this doc blamed `building_colliders` — that was the
off-by-one instrument bug described below, and it is a good reminder that a measurement
can be precise and still point at the wrong thing.
Nothing calls `UpdateMusicStream` inside that call, so a gameplay-sized sub-buffer
would be silence for the whole page. The loading bed is therefore its OWN Music handle
with a 32 s sub-buffer (64 s of runway, ~3x the worst stage), freed the moment the
intro ends; the gameplay bus is untouched. `SEADS_LOADPROF=1` prints the per-stage gaps
— **re-measure with it before changing that number.**

Still unheard by Chad: this build, the balance fix, and the echo.

### ⭐ BOTH OPEN RULINGS ANSWERED AND BUILT — "trim wind / engine further , yes echos
### the stope explosion itself" (Chad, 2026-08-17)

**1. The mix.** `render/mix_levels.h` is NEW and now holds every level on one screen,
with TWO dials, because his ruling contains two separate moves:

| | flown (fly 1) | now | |
|---|---|---|---|
| loading page | 1.00 | **1.00** | the ceiling; no headroom above it |
| gameplay music | 0.95 | **0.599** | −4.0 dB under the page, per his readme |
| wind | 0.55 | **0.243** | bus **+ the extra trim he named** |
| throttle drone | 0.30 | **0.132** | bus + the extra trim |
| guns | 0.55 | **0.347** | bus |
| combat sfx | 0.85 | **0.536** | bus |
| dry blast | *unset (1.0!)* | **0.630** | bus — it had no volume set at all |
| blast echo | — | **0.536** | the new send |

- `kGameplayBusGain` = 0.63 drops the WHOLE gameplay mix under the loading page.
- `kWindEngineTrim` = 0.70 drops wind and the drone **a further −3.1 dB on top**, which
  is the ruling as written: *further* means further than the music, not alongside it.
- Guns/sfx/blast were pulled onto the bus too. Left alone they would have come out
  **+4 dB relative to the bed** — the opposite of everything he has said about this mix.
  The dry blast is the sharp one: it had no `SetSoundVolume` anywhere, so raylib played
  it at unity and it was about to become the loudest thing in the game.

⚠ The FIRST draft of this got it wrong and shipped a static_assert *guaranteeing* the
wind:music ratio could not move — i.e. it asserted that his ruling had not happened. The
red-team caught it. The assert is now inverted: the build fails if wind/engine are not
quieter RELATIVE to the music than fly 1 left them.

**2. The blast echoes.** The recorded blast still plays DRY through raylib's Sound path
(full stereo, its own 44.1 kHz, byte-for-byte as before) and a mono copy runs through
`render/sample_voice.h` into a `wet_only` `StopeReverb` — a reverb SEND, not an insert.
That shape is deliberate: it keeps Chad's "I prefer stereo sound quality" intact, since
only the diffuse tail (where mono is inaudible as a limitation) goes through the
22050/16/1 pipe. Measured offline against the real asset: the echo sits **~10 dB under
the dry blast**, peaks −23 dBFS, and empties ~1.5 s after the 13 s blast ends.

The WIND bed is still not reverbed (diffuse noise gains nothing from a room and it would
smear the speed cue) and neither is the music (score, not something in the room). Those
two exclusions remain unsigned — one line each to undo if he wants them.

### Red-team findings folded (fresh context, per Chad's standing rule)

The consult found a **P0 that would have shipped the feature broken**: `pump_loading`
read `GetFrameTime()` before `BeginDrawing()`. raylib sets `CORE.Time.frame` inside
`EndDrawing` as `update + draw`, where `update` was measured at the PREVIOUS
`BeginDrawing` (`rcore.c:876/941/944`) — so that read reports the frame BEFORE last.
The page clock lagged a full stage, `load_title_alpha` was still ~0 when the long stage
began, and **the title was invisible for the entire load**, snapping up only at the end.
Chad would have got a black screen with loud music: ruling A, undelivered, gate green.

Fixed by driving both the page clock and the intro clock off `GetTime()` (absolute, no
phase) AND by holding the page until the title is fully lit BEFORE the world build
starts. Also folded: the comb bank could rail a sustained drone (measured peak/input
1.79 — the throttle voice sweeps straight through the comb resonances), now fixed with
an input trim plus a soft knee; the intro's key-skip guard DEFERRED presses instead of
consuming them (raylib queues them, so a tap at t=0.1 skipped at t=0.51); and a skip
mid-intro dropped a loud bed into 4 s of near-silence, now level-continuous.

**Four reverb mutants survived the first test suite** (dry duck deleted / both allpass
diffusers deleted / damping deleted / the wet ramp deleted from the audio). The last was
the sharp one — the room would snap to full at the portal and hang around in open sky,
with the gate green, because `wet_` was pinned only through its accessor and never
through the audio. All four now have named killing legs, plus a fifth for the input
trim; **re-run them if you touch `stope_reverb.h`** (mutate, `cmake --build build`,
`.\build\seads_tests.exe "stope reverb*"`).

---

## 0. READ THIS FIRST — the spec is Chad's, not mine

The authority is **`D:\audio_tracks\readme_audio.txt`** and
**`D:\audio_tracks\sound_effects\readme.txt`**. Read both before touching anything.

⚠ Early in this session both files read as **0 bytes** through `ls`, `cat` and `wc -c`.
I reported that, asked blocking questions, built from his chat answers — and then
**wrote my own reconstructed spec into his two files.** That was wrong: never `Write`
over a file Chad already had. He later supplied the real text and I restored both
files verbatim. My reconstruction is gone from there; the equivalent content lives in
`docs/audio_handoff.md` in this repo.

**Standing rule from that mistake:** copy any pre-existing file to the scratchpad
before writing over it, and prefer appending to replacing. A zero-byte read is not
proof a file is empty (a cloud-sync placeholder reads as zero while holding content).

A second lesson worth carrying: when his source-of-truth doc is missing or unreadable,
that is a BLOCKING question, not a gap to fill by inference. Several placements below
were built from chat and turned out to disagree with the written spec.

---

## 1. Chad's rulings (chronological, all from 2026-08-17)

- **Music** = `Above_the_Iron_Clouds.mp3` (surface/general), `Volatus_Aeternus.mp3`
  (tunnels AND the black stope, "on the quieter side", "as you exit that music fades").
- **Intro order** = wind alone → answering machine → ringing → operator. He chose this
  over the conventional ring-then-machine reading, and his readme independently confirms
  it ("Resigned to fate ... then a telephone ring ... then the phone_operator sound").
- **`resigned_to_fate.wav` IS the answering machine.** `phone_operator.wav` is the
  "sorry, call cannot be completed as dialed" operator.
- **Stereo preferred.** "I prefer stereo sound quality, you are free to make the changes
  you need to excell in this sound build.. You are the commander."
- **Sled**: `idle_engine_indy_650.wav` = idle, `Indy650_engine_sound.m4a` = riding brap.
  "merlin engine spitfire" in his readme is his DESCRIPTION of how the idle file sounds,
  not a missing asset — confirmed explicitly. **Bagpipes are the AIRPLANE's**; only the
  resampling technique is shared, never the sound.
- **Train**: the Copper Cliff slag train in `render/train.cpp` "is different it is for
  the slag dump". No Chelmsford model exists; he wants the sound anyway.
- **Interim game name**: "Scarce Skies - Last Call".
- **Splash colours**: the game's predefined pair, `render::kSlagOrange` +
  `render::kComplementBlue` from `render/team_color.h`.
- **Breathing + footsteps**: deferred until the Sudburian can walk/run.
- **Process**: "please remember to redteam check all you work never only check you own
  work." Standing — every batch gets a fresh-context red-team BEFORE reporting done.

### ⭐ THE TWO RULINGS MADE LAST — NOW BUILT (kept here as the record of the shape)

**A. The intro is restructured. TITLE FIRST.** He confirmed this shape:

```
TITLE   "SCARCE SKIES"   music LOUD      <- the loading page; world loads behind it
BLACK   music fades out, wind rises
0:00    wind alone
0:08    resigned_to_fate      (29.37 s)
0:37    phone ringing         (3 x 4.50 s)
0:50    phone_operator        (11.63 s)
  ->    gameplay, music returns QUIETER
```

This reconciles his readme ("louder in the loading page and quieter during the
gameplay"; "Resigned to fate for AFTER title screen and black screen") with his chat
("keep it off or turned right down for the splash screen" — which applies to the phone
sequence specifically, where a bed would fight the voices).

✅ BUILT. The title card is the loading page (`pump_loading()` in `app/main.cpp`,
`load_title_alpha`/`load_page_done`/`kLoadMusicGain` in `render/intro_sequence.h`); it
dissolves over the first 1.5 s of the intro (`intro_title_alpha`) while the bed fades
out over 3 s (`intro_music_gain`) under the rising wind; the gameplay bed swells back
over 4 s (`intro_music_return`, applied in the flight loop). The old shape — title at
1:02, `kIntroMusicGain = 0.0` throughout — is gone, and there are named tests pinning
that it cannot come back.

One honest note on "louder": the loading page is at 1.00 and gameplay at
`kMusicSurfaceGain` 0.95. Those are a hair apart as numbers — the felt contrast is that
the page plays the bed ALONE into an empty mix while gameplay puts it under a
full-scale wind synth, the drone, the guns and the world. If Chad wants the gap wider,
**gameplay is the end to move**; the loading page is already at the ceiling.

**B. The train stays TOWN-GATED** (his chat version), not the readme's "looped
ambience". Already built that way in `render/town_ambience.h` — no change needed, but
the readme line is superseded and that is now decided.

---

## 2. What is BUILT and WIRED (audible today)

| System | Where |
|---|---|
| Music beds + tunnel/stope crossfade + blast duck | `render/music_director.h`, glue in `app/main.cpp` |
| Loading page (title card, music loud, world builds behind it) | `render::load_title_alpha` + `pump_loading()` in `app/main.cpp` |
| Intro sequence (black → machine → ringing → operator → gameplay) | `render/intro_sequence.h`, intro loop in `app/main.cpp` |
| Stope explosion, cadence ~125 s | `render::StopeRumble` |
| ⭐ The stope echo, on guns / sfx / engine | `render/stope_reverb.h`, applied in the stream refills |
| ⭐ The wolf + wildcat cries at the two surface pumps | `render/pump_ambience.h`, glue in the MUSIC-bus block of `app/main.cpp` |
| ⭐ The snowmachine engine (idle bed + riding bed + brap + two-stroke tone) | `render/sled_audio.h` + `sled_drive.h` + `sled_synth.h`, glue in `app/main.cpp` |

Music hangs off `app::inside_tunnel(env_ptr, draw_state.position)` — the SAME
single-source predicate CAVECAM uses (`main.cpp:2625`, variable `cave`), so music and
camera can never disagree about where the tunnel is. The black stope is inside that net.

## 3. What is BUILT but NOT WIRED

| System | Why not |
|---|---|
| ~~`render/sled_audio.h` + `sled_drive.h` + `sled_synth.h`~~ | ✅ WIRED 2026-08-20. This row was also STALE: it said "no snowmachine in this tree", but `sim/sled.cpp`, `drive_mode` and the J-key mount have all since landed here. The synth was finished, tested code with no caller for three days. |
| `render/town_ambience.h` (the train) | The app-side building-density lookup is not written. See §5. |

## 4. OUTSTANDING against Chad's written spec

Audited line by line. These are real gaps, ranked:

1. ~~Rebuild the intro to ruling A~~ — ✅ DONE this session.
2. ~~"Make all sounds like guns echo when in the stope as well."~~ — ✅ DONE this
   session: `render/stope_reverb.h`, on the gun, sfx and engine streams.
   - The room: 25 ms predelay, four damped feedback combs (43.0/47.3/51.9/57.1 ms,
     deliberately incommensurate), two allpass diffusers, RT60 2.6 s, 2.2 kHz damping.
     The comb feedbacks are DERIVED from RT60 and each line's own delay — re-sizing the
     room is one number, not four hand-tuned gains.
   - NOT applied to the wind bed (reverberating broadband noise is inaudible as reverb
     and would smear the speed cue), NOT to the music (score, not something happening
     in the room), NOT to the `black_stope_explosion.wav` one-shot (a recorded cave
     blast that already carries its own rock). Each exclusion is one line to undo.
   - Once faded out it is a bit-identical dry bypass, so open sky is untouched.
   - ⚠ UNHEARD. Every dial (`kStopeRT60`, `kStopeWetMax`, `kStopeDampHz`) is a code
     constant with a comment; the ear is the only judge of whether it sounds like rock.
3. **The explosion should ALSO fire "riding around on the snowmachine"**, not only in
   tunnels. The sled IS in this tree now (see the corrected row in §3), so this is no
   longer blocked on the winter-gi wiring — it is just not done.
4. **`splash_screen_ambient_winds` is also "general ambience when on the snowmachine or
   flying or walking"** — currently splash-only. Note this must coexist with the
   PROCEDURAL wind synth (`render/wind_synth.h`), which owns the speed cue; the recorded
   bed is a separate atmospheric layer, not a replacement.
5. **Wire the train**: read the building density at the player and feed `TrainAmbience`.
6. **Footsteps**: combine the three takes by terrain, "Higher frequency for running".
   Deferred until locomotion exists.
7. **winter-gi mirror**: sled synth + everything else. Held — that tree's `app/main.cpp`,
   `render/draw.*`, `render/post*` are dirty under the animation agent. Audio blocks
   there are byte-identical to this tree (offset +15 / +687), so the transplant is
   mechanical. Assets do NOT travel by git — use `install_soundbank.sh`.

## 5. How "a town" is derived (for item 5 above)

There is no settlement list and none is needed — a town is where the buildings are.
`world::BuildingColliders` (`world/buildings.h:35`) already indexes every building into
an equirect grid. The prism count in the player's own cell is the local density, O(1)
from the prefix spans: `cell_start[c+1] - cell_start[c]`, with the cell index computed
exactly as `BuildingColliders::hit()` does (`world/buildings.cpp:99-104`). Feed that
count to `render::is_town()`. Policy is already written and tested; only the lookup
is missing.

---

## 6. The audio pipeline (lives OUTSIDE this repo)

`D:\audio_tracks\tools\` — `soundbank.manifest.tsv` (declarative), `build_soundbank.sh`
(cut → loop-fold → two-pass loudnorm → encode → re-measure → gate), `install_soundbank.sh`
(deploy + byte-verify + ask git if committable). Currently **15/15 PASS**.

```
bash tools/build_soundbank.sh
bash tools/install_soundbank.sh D:/flight_sim2/seads-recon
```

### Three traps this pipeline exists to defend against

1. **loudnorm silently falls back linear → dynamic** when the source LRA exceeds the
   LRA target or the gain would breach the true-peak ceiling — no warning, no non-zero
   exit, and it hits the loudness target BETTER that way. A LUFS+peak report can never
   see it. It had dynamically level-ridden 9 of 15 assets (~10 dB across a music bed)
   while reporting a clean PASS. Pass 2 now asserts `normalization_type`; a mismatch is
   a build FAILURE. **Do not remove that assertion.**
2. **`.gitignore:37` is `assets/audio/*.wav`, a SINGLE-LEVEL pattern.** A loose WAV in
   `assets/audio/` is silently refused by `git add` (exit 0, nothing staged) while the
   same file one folder deeper commits fine. Everything ships in `music/ sfx/ loops/`.
   This is why `assets/audio/bagpipe_drone.wav` has never been committed and the
   throttle voice is silent in every fresh checkout.
3. **Measure the asset before sizing any constant against it.** The explosion was 21 s
   of digital silence (−70 LUFS from 15 s on) and truncated itself; both music beds
   looped through their own fade-in/out. `static_assert`s now pin the rumble gaps
   against `kStopeBoomLen`.

---

## 7. Verification discipline (this repo's rules, learned the hard way here)

- **`ctest` never runs `seads.exe`.** Audio ships green while silent. Verify by EAR on
  `build-play\seads.exe` (RelWithDebInfo). `build\` is the Debug gate build and is
  unplayably slow BY DESIGN — Chad tried it and reported jitter.
- **Never build while a ctest gate is live on the same build dir.** I hit it: the link
  failed with `cannot open output file seads_tests.exe: Permission denied`.
- **Mutation-verify, don't trust green.** The sled tests passed while a synth emitting
  one DC sample forever would also have passed. Three mutations (freeze the idle cursor,
  delete the run bed, delete the tone) are now each caught by a named test — re-run them
  if you touch `sled_synth.h`.
- **A green gate is evidence about what you measured.** Chad's first fly found the intro
  never played at all, with every test green: the first-frame `GetFrameTime()` reported
  the entire world load, so `intro_t` leapt past the end before a pixel was drawn. My own
  red-team had flagged it as P3 and I deprioritised it. Fixed and verified by RUNNING the
  binary (`intro channels still loaded at 55 s`), not by a test.

## 8. Balance — Chad's first fly

"The wind and the engine are too loud to hear the music is all I could tell."

| | was | now |
|---|---|---|
| `kMusicSurfaceGain` | 0.55 | **0.95** |
| `kMusicDeepGain` | 0.42 | **0.78** |
| wind stream trim (`main.cpp`) | 0.85 | **0.55** |
| bagpipe drone trim (`main.cpp`) | 0.42 | **0.30** |

Root cause of the error: I set levels against each asset's MASTERED LOUDNESS, which is
the wrong reference. The music does not compete with a −20 LUFS number; it competes with
a full-scale noise synth at its own 0.85 trim. **Balance is decided against the other
channels, by ear.** Both synth trims are one-word reverts, marked in the code.

He has NOT yet heard the post-fix build.

---

## 9. Commands

```
cd D:\flight_sim2\seads-recon
cmake --build build --config Debug                        # gate build
ctest --test-dir build -C Debug --output-on-failure       # 1190/1190 (~11 min)
python tools/graph/graphify.py                            # SAME COMMIT as any new file
cmake --build build-play --target seads                   # the build Chad flies
.\build-play\seads.exe                                    # run from the REPO ROOT

SEADS_LOADPROF=1 .\build-play\seads.exe                   # trace the WHOLE opening
```

`SEADS_LOADPROF` prints every loading-page stage gap and every intro cue as it fires.
It is the only instrument that can see the opening at all — no ctest runs this binary,
and the last intro bug (the first-frame dt eating the sequence) shipped fully green.
Expected on this machine:

Each line names the stage that JUST RAN — it used to name the NEXT one, which is exactly
how 22 s of `planet_heightfield` got written up as `building_colliders`. Expected here:

```
[LOADPAGE]   0.001 s  render build params
[LOADPAGE]  21.962 s  planet heightfield      <- the one that needs the runway
[LOADPAGE]   0.045 s  building colliders
[LOADPAGE] title up for 23.69 s total; handing off to black
[INTRO] t=  8.00  cue 0 fires     (answering machine)
[INTRO] t= 37.40  cue 1 fires     (ring 1 of 3)
[INTRO] t= 50.90  cue 2 fires     (operator)
```

Query the graph before grepping: `python tools/graph/graph_query.py symbol|file|impact|tests-for <arg>`.
It found the team palette in two queries and proved the sled absent in one.

## 10. Test files

`test/unit/test_soundscape.cpp` (music director, stope rumble, intro timeline, splash
palette, **the loading page / title-first shape**, **the stope reverb**) and
`test/unit/test_sled_audio.cpp` (sled mappings, drive, synth, town train). Both
registered in the root `CMakeLists.txt` next to `test_gun_audio.cpp`.

The reverb pins are deliberately mechanism-level, not level-level: energy arrives AFTER
the input stops (a reverb with no tail is just a volume trim), the first reflection
lands exactly at `pre_len + min(comb_len)` (so the room has a size), the tail decays
past RT60 (so no feedback path is >= 1), the comb delays are mutually incommensurate
(so it is a room and not a metal pipe), and outside the net the buffer comes back
byte-for-byte unchanged.
