# Audio handoff — flight sound (LIVE)

> **NEXT AGENT: start at `docs/SESSION_HANDOFF_20260824_audio.md`.** It carries
> the launch line, the tree this lane works in (⚠ **not** `seads-recon`, whose
> BRANCH also moves under you mid-session — see its §0), the current dial table,
> and what is open. It supersedes `SESSION_HANDOFF_20260820_audio.md` for the
> dials and for the `wired=yes` liars; the rest of that file still stands.
> THIS file is the running log — every measurement and every derivation — and it
> is the territory, not the map.

**Goal (Chad, 2026-07-10):** near-professional but free-to-use flight audio.
Four layers, each driven by physics so nothing feels static:
1. **wind** — more DYNAMIC (gusts, buffet on G/AoA), not a desk fan.
2. **engine** — sound REAL and NOT ANNOYING (the DB 601/605 V12 growl).
3. **prop** — the disc "cutting the air" in G maneuvers, SUBTLY.
4. **creak** — occasional panel/glass stress at high speed.

**Rules:** cosmetic, read-only off `SimState`, firewalled from
`sim/`/`control/` (can't move a golden). Procedural + code-only through raylib
`AudioStream` — no middleware. Tunables are code constants (S9-zoom / wind-audio
precedent). Drive the whole thing with the **`flight-audio` skill**.

## Decisions locked
- **Engine = hybrid** — wind/prop/creak procedural; engine is one CC0 real V12
  loop, RPM-pitched + lo/mid/hi crossfade + detune (synthesized starter is the
  fallback until a CC0 sample is sourced).
- **Editor = Audacity** (installed via winget). Chad shapes starters to feel; I
  bake the direction into the headers.

## State — what's DONE vs PENDING
- ✅ Bench built: `tools/audio_synth.py` renders all four layers + a 20s flight
  demo to `assets/audio/*.wav` (all synthesized → license-free).
- ✅ Audacity installed + opened on `seads_engine_starter.wav` for Chad to shape.
- ✅ `render/wind_audio.h` — the one existing pure mapping (speed→vol/pitch),
  wired in `app/main.cpp` (`wind_stream`). This is the TEMPLATE for the rest.
- ✅ Wind CALM PASS (Chad, 2026-07-11: "calm soft and beautiful — no flapping,
  wavering, or staccato"). `render/wind_synth.h` stripped to a smooth steady
  slipstream: pink-noise bed (lo<->hi crossfade by speed) + soft G-swell slip.
  REMOVED the flute phrases (staccato notes), the loose-tag flitter (flapping),
  and the gust LFO (amplitude wavering). PENDING Chad's fly-audition.
- ✅ Engine HUM baked + wired (Chad, 2026-07-11: "a low quiet soft hum that
  increases in frequency by throttle"). `render/engine_synth.h` is NO LONGER the
  firing-slap train (that model is staccato by construction) — it is now a
  smooth harmonic drone (fundamental + soft octave/fifth, dark top roll-off,
  whisper of breath noise), pitch = kEngineIdleHz..kEngineFullHz linear in
  throttle, quiet vol that opens slightly with throttle. Wired in `app/main.cpp`
  as `engine_stream`/`engine_synth` next to `wind_stream`, fed off
  `draw_state.throttle`, stream vol 0.55 (sits under the wind). PENDING Chad's
  fly-audition. (Hybrid CC0-loop plan deferred — the synth hum is what Chad asked for.)
- ⏳ `render/prop_audio.h` + glue — NOT started. Chop depth off `load_factor`.
- ⏳ `render/creak_audio.h` + glue — NOT started. Sparse, hysteretic, fast-gated.
- ✅ **COMBAT AUDIO (gun-fire + explosion/hit) — LANDED 2026-07-13, gate 421/421.**
  Two EVENT-driven layers (not continuous like wind/engine): `render/gun_audio.h`
  (pure envelope/const mappings, unit-tested) + `render/gun_synth.h`
  (`GunSynth` machine-gun stutter — two shot-schedule phase accumulators at the
  composite battery rates [3 cannon ×12 Hz, 2 MG ×19 Hz], each shot a band-passed
  noise crack in a 24-voice ring; `CombatSfxSynth` — polyphonic explosion + hit
  one-shot voice pool). Glue in `app/main.cpp` next to `wind_stream`/`engine_stream`:
  `gun_stream` driven by `fire_held && !raw_mode`; `sfx_stream` edge-detects the
  monotone `cw.kills`/`cw.hits` counters (≤8 voices/frame). Firewall-clean
  (read-only, no clock, fixed-seed PRNG), skipped in smoke, streams unloaded on
  exit. **Fable red-team: SOUND-WITH-FIXES** — folded the P1 (phase-jitter clamp
  was killing the "late" half of the jitter → metronomic/fast brrrt; clamp
  removed) + added a silence-after-fire regression test. Deferred P2s (for Chad's
  ear pass): tanh soft-clip on stacked booms (match `wind_synth`), per-voice tau
  wiring, precompute the decay coefficients, first-crack phase reset on trigger.
  Volumes: gun 0.55 / sfx 0.85 (code trims). **AUDITION:** `gun_preview.wav` in the
  worktree root (offline render of the exact synths — 7 s: burst, taps, booms;
  regenerate via `g++ -O2 -std=c++17 -I. tools/gun_audio_preview.cpp -o
  gun_preview.exe && ./gun_preview.exe`). PENDING Chad's ear (in-game or the wav):
  is the brrrt punchy? booms/tinks read? mix balance vs wind/engine?

## Next action
Chad auditions `assets/audio/seads_flight_demo.wav` + the four starters, reports
DIRECTION per layer. Then, one layer at a time: bake constants into a pure
`render/*_audio.h`, add the `AudioStream`/`LoadSound` glue in `app/main.cpp`
next to `wind_stream`, unit-test the pure mapping (monotone/clamped/continuous),
run the exe and confirm by EAR (the green gate is blind to the app binary).

---

# Soundscape rung — recorded assets (2026-08-17, branch `sandbox/audio`)

Chad brought a sound repo (`D:\audio_tracks`) and asked for a game soundscape:
an ambient music bed, a placeholder splash screen with a timed intro sequence,
and effects placed per his direction.

**This rung adds; it replaces nothing.** The four procedural channels
(`wind_synth`, `bagpipe_throttle`, `gun_synth`, `sfx_synth`) are untouched, and
the doctrine holds: every layer whose job is to convey SPEED stays synthesized,
because a static loop there "kills the sense of speed instantly". Nothing added
here is speed-coupled. This is also consistent with the **Engine = hybrid**
decision above — a real loop was always sanctioned where it is physics-driven.

## The pipeline (in `D:\audio_tracks`, not in this repo)
- `tools/soundbank.manifest.tsv` — one row per shipped asset: cut, loop-fold,
  bus, loudness target, true-peak ceiling, format, normalisation mode, wired.
- `tools/build_soundbank.sh` — cut → seamless loop-fold → two-pass EBU R128
  loudnorm → true-peak limit → encode → **re-measure and gate**. Deterministic.
- `tools/install_soundbank.sh` — deploys into a game tree's `assets/audio/` and
  verifies byte-identical, then asks git whether the result is committable.

Sources arrived spanning **21.6 LU** (-8.7 to -30.3 LUFS) with the sled idle
clipping at **+1.4 dBFS**, so nothing ships unnormalised. Bus targets: VOICE -16,
SFX -18, MUSIC -20, AMBIENCE -24. Current build: **15/15 PASS**.

### Three defects an independent verification pass caught (all fixed)
The first build reported a clean PASS and was wrong, because it measured only
the two quantities that could not reveal the fault. Worth remembering: **a green
report is evidence about what you measured, not about what you did.**

1. **`linear=true` was silently ignored on 9 of 15 assets.** ffmpeg's loudnorm
   engages linear mode only if the gain fits under the true-peak ceiling AND
   `measured_lra <= LRA`. Otherwise it falls back to dynamic with no warning and
   no non-zero exit — and it hits the integrated target *better* that way, so a
   LUFS+peak check can never see it. At the hardcoded `LRA=11` this rode ~10 dB
   of level across a music bed and 15.3 dB across an ambience bed: exactly the
   pumping the manifest forbids. Fixed by raising the LRA target and, crucially,
   by **asserting pass 2's `normalization_type` against the manifest** — a
   fallback is now a build FAILURE. Seven assets then failed honestly and were
   given targets they can physically reach, or a deliberate `dynamic`.
2. **The crossfade curve was linear (`tri`), not equal-power.** Tail and head of
   a loop are uncorrelated, so a linear pair digs a **-3.7 dB hole** through
   every crossfade — measured; on the near-constant sled idle it was a -4.0 dB
   pulse once per loop. Now `qsin`. (`esin` measures -8.5 dB — worse, not the fix.)
3. **Both music beds looped through their own fade-in/fade-out.** Profiling in
   5 s windows: *Above the Iron Clouds* runs -29.8 LUFS at 0 s and -43.2 at 140 s;
   *Volatus Aeternus* -47.2 at 0 s. Every loop fell into near-silence for ~25 s.
   Now cut to level-matched bodies — 20:130 (-13.1 head vs -12.7 tail) and
   20:120 (-16.2 vs -15.6) — before folding.

Also fixed: a greedy JSON parse that returned the wrong field on single-line
output, an encode stage with no error check, failed rows vanishing from the
report instead of appearing as FAIL, orphaned outputs surviving a manifest edit,
a missing trailing-newline check (which silently drops the last row), an
undeclared 0.3 dB peak slack now printed as `ok-slack`, and `install --check`
testing directories rather than the files the ignore rule actually matches.

## What is wired
| Asset | Where it plays |
|---|---|
| `music/music_ambient_main.ogg` (*Above the Iron Clouds*) | surface ambient bed |
| `music/music_deep_tunnel.ogg` (*Volatus Aeternus*) | tunnels **and** the black stope |
| `sfx/black_stope_explosion.wav` | ambient blast in the net; drives the music duck |
| `loops/intro_wind_bed.ogg`, `sfx/intro_{machine,ring_cycle,operator}.wav` | the splash sequence |

## The two new pure headers
- **`render/music_director.h`** — the MUSIC bus. Equal-power cross-fade between
  the two beds on `app::inside_tunnel()` (the SAME single-source predicate
  CAVECAM uses, so music and camera cannot fork about where the tunnel is; the
  black stope is inside that net, so one predicate covers both). Enter tau 1.6 s,
  exit tau 3.2 s — Chad asked for the deep bed to *fade* on the way out.
  `StopeRumble` schedules the blast on a fixed non-metronomic gap table (no
  `<random>`: it is banned here and the gate needs reproducibility).
- **`render/intro_sequence.h`** — the splash timeline. Cue times are derived
  from the MEASURED length of the mastered asset each one follows, so there is
  no dead air and no overlap. Order is Chad's ruling, chosen over the
  conventional reading: **wind alone → answering machine → ringing → operator**.

Both are raylib-free (the `seads_render_core` / `seads_tests` firewall), so
`graphify --check-only` passes and both are unit-tested headlessly:
`test/unit/test_soundscape.cpp`, 29 cases.

## Four defects a red-team pass caught in the integration (all fixed)
1. **The explosion was 33.4 s but three of the four rumble gaps were shorter**
   (23/16/30 s), and it is one non-polyphonic `Sound`, so a re-trigger restarts
   it — the blast would have cut itself off mid-decay on 3 of every 4 cycles.
   Root cause turned out to be the asset: measured in 3 s windows it is **21 s
   of digital silence** (-70 LUFS from 15 s on). Cut to its real 13 s body,
   which fixes the truncation at source. `kMusicDuckLife` was 3.82 s against
   that body — it released the music to full while the rumble was still going,
   the opposite of occlusion — now 10.1 s. Both facts are pinned by
   `static_assert` against `kStopeBoomLen`, so a re-cut cannot silently break it.
2. **The duck fired even when the explosion had failed to load** — the `*_ok`
   guard was on the wrong side. The bed would dip 14 dB every 16-37 s with
   nothing audible causing it, which reads as a broken music player.
3. **A key or click during the world load skipped the whole intro.** Nothing
   polls events between `InitWindow` and the splash's first `EndDrawing`, and
   raylib delivers the entire backlog on the first poll — so anyone who clicked
   the black window to focus it lost the intro on frame 2. Now drained, plus a
   0.5 s skip guard.
4. **The music beds inherited a sub-buffer sized for the 22 kHz synths.**
   `SetAudioStreamBufferSizeDefault(1024)` is 46 ms at 22050 Hz but only 23 ms
   at 44.1 kHz, and `UpdateMusicStream` runs once per render frame — so any
   frame spike (this repo prints a profiler line at 20 ms, and the post FBO is
   built lazily on the first flight frame) risked a dropout. Raised to 4096
   before the music loads.

Also: the per-frame block recomputed `app::inside_tunnel` when `cave` — the
identical call — was already in scope, doubling an ~80-primitive SDF scan
exactly while inside the net and booking it to the `audio` prof lap; and
`intro_t` clamped its dt while the audio it schedules plays at wall-clock rate,
so a window drag or minimize desynced the sequence permanently.

**Test quality** was the more valuable half of that review. Several cases were
vacuous — most seriously, **the cue table's `.cue` values were never asserted**,
so swapping `kMachine` and `kOperator` (inverting the order Chad explicitly
ruled) passed all 29 tests while `main.cpp` dispatches on exactly that field.
Now pinned, along with strict monotonicity in the duck (`<=` let a constant
function pass), a `StopeRumble` rearm test that actually fails when the rearm is
deleted, wind-duck continuity, and a non-vacuity clause on the bounds check.
33 cases.

## Occlusion — why the duck, not a louder blast
Chad: "make the expolsion come in and occulude the tunnel and balck stope
music". The blast is mastered to **-15 LUFS at a -1.0 dBTP ceiling**, which is
as loud as it physically goes without limiting; its source LRA is 19.3 and that
crest **is** the content. Winning the same contrast by compressing it flat would
cost the thing that makes it read as a blast. So occlusion is delivered by
pulling the music to `kMusicDuckFloor` (-14 dB) under it and releasing over
2.6 s. The bed stays audible underneath — occluded, not dropped out.

## The `.gitignore` trap (verified, not inherited)
`.gitignore:37` is `assets/audio/*.wav`. A gitignore `*` does not cross `/`, so
that pattern is **single-level**: a loose WAV in `assets/audio/` is silently
refused by `git add` (exit 0, nothing staged) while the same file one folder
deeper commits normally — confirmed with `git check-ignore -v`. Combined with
the loader's deliberate missing-file-is-silence policy, this is how
`assets/audio/bagpipe_drone.wav` came to be loaded at startup by `app/main.cpp`
and **never once committed** — the throttle voice is silent in every fresh
checkout and nothing ever said so. The soundbank routes around it by living in
`music/ sfx/ loops/` subfolders; no `.gitignore` change was needed. Each new
loader now also **prints once** when an asset is missing.

## Session 2 — the splash card, the sled engine, the town train (2026-08-17)

### The splash card
Chad's interim name: **"Scarce Skies - Last Call"** (`kGameTitle`/`kGameSubtitle`
in `render/intro_sequence.h`, one constant so a rename is one edit).

He asked for "our thematic orange / blue colors that are opposite on the color
wheel ... Aleady chosen and predefined in this game." The **code graph found
them in two queries** (`graph_query.py symbol Complement` →
`render/team_color.h:126`): `kSlagOrange` (1.00, 0.55, 0.12) and
`kComplementBlue` (0.12, 0.57, 1.00), the faction palette.

The splash READS both from that header. It must never retype them — that header
exists precisely because the slag orange used to live only inside a GLSL string
(H1 single-source-or-fork). And the header carries a trap worth repeating: the
eye-plausible channel reversal (0.12, 0.55, 1.00) is **not** the complement, it
lands 181.36 deg away — invisible on screen, and it would make "precisely
opposite" a lie. `test_soundscape.cpp` now pins the 180 deg relation AND that
the near-miss fails it.

### The snowmachine engine — built, tested, NOT wired here
Chad: "Idle sngine for snowmachine is [the idle] sample loop and the riding is
the other engine snowmachine brap brap sounds that can ramp up en key pressed.
Tapping keys gives the brap brap ramp up... then chose a tone for the sustained
machine sound... Make sure to balance the sounds." Assets confirmed by him:
`idle_engine_indy_650.wav` = idle, `Indy650_engine_sound.m4a` = riding. He also
ruled the bagpipes stay the AIRCRAFT's — only the resampling *technique* is
shared, never the sound.

`render/sled_audio.h` (pure mappings) + `render/sled_drive.h` (the state
machine). Three layers, and the split is the design:
- **IDLE** — owns the voice at rest, retreats squared as revs come up.
- **BRAP** — the riding sample, re-triggered on throttle **onset**. One per tap.
- **TONE** — gated on SUSTAIN, not on revs, so a tap cannot produce it. That
  gate is what keeps a tap a brap.

"Brap brap ramp up" comes from an asymmetric rev follower (up 0.22 s, down
0.55 s): each tap starts from a higher floor than the last instead of four
identical barks. Pinned by a test asserting revs strictly increase over 4 taps.

**The tone, derived not picked.** The Indy 650 is a 650 cc three-cylinder
two-stroke, which fires once per revolution per cylinder, so
`f = rpm/60 * 3`: idle 1200 rpm → **60 Hz**, pinned 7000 rpm → **350 Hz**.
Saw-like harmonic stack (a sine reads as an electric motor), lowpass opening
420 → 4200 Hz with load because a loaded two-stroke gets brighter as well as
higher, plus a few cents of detune so it beats instead of sounding synthetic.
That range sits below the aircraft drone so the two vehicles can share a mix —
pinned under 400 Hz so an rpm retune cannot walk the sled into the plane's band.

⚠ **`sim/sled.cpp` and `render/sled_model.cpp` do not exist in this tree** —
confirmed via `graph_query.py symbol sled` → no match. The sled lives only in
`winter-gi`. The DSP and its 17 tests are here and transplant cleanly; the
wiring and the ear-check wait for that tree.

### The town train
Chad: "The train sounds every once in a couple of minutes when on the
snowmachine near a town or in a town and can be heard also when flying over a
town sometimes." And, when I asked whether to attach it to the train already in
the world: **"that train is different it is for the slag dump, I dont have a
chelmsford train model created yet but still want the sound of it for now."**

So `render/town_ambience.h` is deliberately decoupled from `render/train.cpp`
(an electric trolley hauling slag pots — the wrong sound for it anyway). No
object, no model: a mainline train somewhere past the treeline.

**What counts as a town** is not a curated list — there isn't one, and a town is
exactly where the buildings are. `world::BuildingColliders` already indexes
every building into an equirect grid, so the prism count in the player's own
cell IS the local density, readable in O(1) from the prefix spans
(`cell_start[c+1] - cell_start[c]`). The app does the lookup; the header owns
only the policy, so it stays pure and testable.

The clock advances ONLY while near a town, and leaving **pauses** it rather than
resetting — otherwise a player crossing the town edge repeatedly would never
hear the train at all. Ground cadence 105/165/132/198 s; from the air ~2.4x
rarer, and mixed lower rather than pushed up (a train you hear clearly at
altitude stops sounding like distance).

The asset changed shape for this: it was a loop, it is now a **one-shot pass**.
The 84 s source is four separate passes split by near-silence
(0-5s -16.4 | 5-10s **-50.5** | 10-25s -16.3..-20.0 | 25-30s **-50.7** |
30-60s -15.6..-18.3 | 60-65s -39.4 | 65-80s -20.9..-17.6 | 80s+ **-61.2**),
so it is cut to the sustained 30:57 block with 1.5 s fades — a new `fade`
manifest column, because a cut landing mid-sustain starts on a nonzero sample,
which is a step, which is a click.

## Session 3 — the two animals at the surface pumps (2026-08-20)

Chad: *"I want a wolf cry to sound when you get to the enemy surface pump in
sudbury. And I will go find a wildcat [sound] for near levak and onaping falls
allied pump ... okay found one .. cave_hiss in the sound effects ... please add
those sounds when you fly over the area or snomobile close to it (those
respective pumps)."*

WIRED and audible. Two new mastered assets, one new pure header, one glue block.

| Piece | Where |
|---|---|
| Policy (ranges, cadence, balance, fall-off) | `render/pump_ambience.h` — pure, `<algorithm>` only |
| Assets | `sfx/wolf_cry.wav` (5.0 s, stereo), `sfx/wildcat_cry.wav` (7.0 s, mono), both AMBIENCE −24 |
| Glue | `app/main.cpp`, inside the existing MUSIC-bus block next to `StopeRumble` |
| Tests | `test/unit/test_pump_ambience.cpp`, 29 cases |

**Keyed to the PLACE, not the allegiance.** Chad named the pumps by faction, but
what he described is two geographies — a wolf on the Coniston side, a cat in the
bush behind Levack and Onaping Falls. `[conquest] player_faction` is a config
switch, so flying as Sudbury would invert "enemy" and "allied" and swap the two
animals across 30 km of map. The wolf is bound to `world::kPumpSudburySurface`
and the cat to `world::kPumpValleySurface` as places, and stays there whoever
you fly for.

**Ground distance, not 3D range.** The distance comes from the baked unit
directions in `world/faction_bubbles.h` via a great circle against `params.R` —
not from the lifted positions `combat::make_pumps` builds. Three reasons: it
works with `[conquest]` off (those positions only exist inside the conquest
branch, and the map still has an Onaping either way); it is altitude-blind,
which is what "fly OVER the area" means (a 3D range holds the wolf silent
directly above the pump and fires it on the descent); and it needs no terrain
sampling, so it costs two dot products a frame and cannot disagree with the
ground under it.

**Ranges**: 1200 m on the machine, 3200 m in the air — "close to it" and "fly
over the area" are different acts at different speeds. The header
`static_assert`s that twice the air radius clears the **42.2 km** between the
two anchors (measured on the R = 15 km sphere — not the ~30.8 km in
`faction_bubbles.h`, which is each pump's distance to the ENEMY CENTRE and is
the wrong number for this), and the test re-derives that separation from the
real baked directions so a future pump move (they moved once already,
2026-07-25) cannot leave the assertion checking a map that no longer exists.

**An arrival, not a cadence.** This is where it deliberately differs from
`render/town_ambience.h`. The train runs on its own schedule whether or not you
are there, so its clock banks time while you are away. These animals are
reacting to YOU: the cry is an arrival event 2.5 s after you cross the radius
(not on the tick — that reads as a trigger volume, which is what it would be),
then a ~41–78 s loiter table on the ground / ~66–125 s in the air while you
stay, and a **30 s re-arm hold** before a departure can earn a fresh greeting.
That hold is the whole defence against the boundary-dither case: circling the
pump, or sitting where the distance dithers by a metre, otherwise re-fires the
greeting on every crossing and the wolf machine-guns.

**Muted in the workings.** Gated on the same `cave` predicate the music bed and
the camera read — these are surface animals, and hearing the wolf from inside
the black stope would say the sound is stuck to the camera. Held rather than
merely muted: the scheduler is not ticked underground, so a visit cannot serve
out its gap under rock and cry the instant you surface.

**The wildcat's mastering note.** `sfx/wildcat_cry.wav` is cut `0:7` and that
number is the pipeline's, not taste. The 8.3 s source decays −12 → −43 dBFS, an
LRA of 22.8 — above `LRA_TARGET`, so loudnorm silently fell back to DYNAMIC and
the build's `normalization_type` assertion refused the asset, working exactly as
designed. Dynamic is the wrong answer here: gated leveling would ride that decay
back up, and the decay IS the sound. Cutting to 7 s brings LRA to 13.0 and
discards only the last 1.3 s (−37 → −43 dBFS, about −65 dBFS once mastered:
inaudible under the wind bed by a wide margin). It then verifies 1.0 LU hot
against the −24 target — a meter disagreement between loudnorm and ebur128 on a
short mono file, which re-aiming does not escape (measured at both −24 and −25
the file lands 1.0 above whatever it was asked for; output peak is −6.6 dBTP, so
nothing is clipped). The target stays at the house number and the 1 dB is
corrected in `kWildcatCryGain`, where balance belongs.

⚠ **UNHEARD.** Both radii, both cadences and the two mix trims are code
constants; the gate cannot judge any of it. `kCryRangeFlyingM` is the number
most likely to want moving after a fly, and it moves alone.

## Session 4 — the snowmachine engine, finally connected (2026-08-20)

Chad: *"find the engine idle sounds idle_engine_indy_650 for engine idle and
indy 650 engine sounds to be sampled for the throttle press like the brap brap
sound while driving — the last agent failed to build those sounds in."*

**Nothing needed building.** `render/sled_audio.h`, `sled_drive.h` and
`sled_synth.h` were written, unit-tested and red-teamed on 2026-08-17 and were
simply never called: `app/main.cpp` had no reference to any of them, and the
manifest's two rows shipped `wired=no`. The synth was finished code with no
caller. This session connected it — the manifest hold is lifted, both rows are
`wired=yes`.

| Piece | Where |
|---|---|
| Pure layer mappings (revs, gate, tone, balance) | `render/sled_audio.h` |
| Driver state (rev follower, onsets, sustain) | `render/sled_drive.h` |
| Real-time render | `render/sled_synth.h` |
| **Glue (new)** | `app/main.cpp` — decode, stream, per-frame drivers, teardown |
| **Mix level (new)** | `render::kSledStreamLevel` in `render/mix_levels.h` |

**Three layers, as Chad assigned them.** `loops/sled_idle_loop.wav` owns the
voice at rest and retreats as revs come up. `loops/sled_run.wav` is the riding
bed — a LOOP whose level rides revs, re-attacked on every throttle onset, and
that re-attack is the brap. A synthesized two-stroke tone sings underneath a
sustained hold, its pitch derived from the real engine (650 cc three-cylinder
two-stroke fires once per revolution per cylinder, so `f = rpm/60 × 3`: 60 Hz at
a 1200 rpm idle, 350 Hz pinned — an octave and a half below the aircraft's
bagpipe drone, so the two never fight for the same band).

**The doctrine hold is satisfied, not waived.** Both recordings are replayed at
a REV-DRIVEN resample ratio off a rev follower with engine inertia (spin-up
0.22 s, spin-down 0.55 s — the asymmetry is what makes repeated taps stack into
a rising blat instead of dropping back to idle between each one). Nothing plays
at a fixed pitch, which was the condition the manifest's hold named.

**The driver is `sled_thumb`, pushed at FRAME rate.** Both halves matter. The
thumb throttle is the key press, and the brap is a property of how the throttle
is *used*, not of how fast the machine is going — driving this off a speed or an
rpm gives a machine that swells but never barks. And per frame, never per
buffer: a buffer is 46 ms, and the red-team on this synth showed that feeding it
there drops real taps outright (a 40 ms press that opens and closes between two
buffers is never seen). The synth keeps an onset latch that `render()` drains.

**Three things the wiring itself had to get right**, none of which the gate can
see:

1. **The stream is created before the music loads.** raylib hands
   `LoadAudioStream` whatever the sub-buffer default is at the moment of the
   call and zero-fills any unwritten remainder, so a 22050 Hz stream created
   after the beds raise the default to 4096 and then fed 1024 frames plays 25%
   signal and 75% silence. Same trap the blast send documents at length.
2. **Off the machine the throttle is forced to zero.** `sled_thumb` is only
   *advanced* inside the drive-mode block, but nothing zeroes it on dismount —
   the mount branch resets it, the dismount branch does not. Stepping off at
   full throttle freezes it at 1.0 for the whole flight, and the mount gate
   would then fade in a screaming engine at the moment you got back on a
   stationary machine.
3. **The mount gate is a slewed 0..1, not a boolean**, ~0.25 s each way — a
   full-scale engine channel switched on at a buffer boundary is a click at
   exactly the moment the player is listening for feedback. Below audibility the
   synth is skipped and zeros are fed, but they are fed *through* the stope
   room, so its delay lines cannot freeze holding the last ride's audio.

**Mix.** `kSledFlownLevel = 0.62` on the gameplay bus — between the guns (0.55)
and the sfx pool (0.85), and asserted to sit under the music bed, which is the
one complaint Chad has made twice. Deliberately NOT on `kWindEngineTrim`: "trim
wind / engine further" was said about the aircraft while flying, about a mix in
which this channel was silent. Note the synth also carries its own internal
master (`kSledMaster` 0.78) which balances its four layers against each other —
move `kSledFlownLevel` for "the sled is too loud", `kSledMaster` for "the layers
are wrong against each other".

⚠ **UNHEARD, and this one has more unheard dials than anything else in the
lane** — four layer gains, two rev time constants, two gate thresholds, the tone
harmonic stack and its cutoff sweep. The gate proves the maths, not the machine.

### Known next step, not done here
The sled kernel publishes `sim::SledState::engine_rpm`, commented in `sim/sled.h`
as "Readout for dash/audio: idle 1700 .. 8000 at full track speed **while the
clutch is engaged**". This wiring does not use it — the synth models revs from
throttle with its own follower, which is what it was written and tested against.
Feeding the real rpm would add CVT load coupling: the engine bogging in deep
snow, free-revving on a slipping track. That is a genuine improvement and a
contract change to `SledDrive`, so it wants its own pass and its own fly.

## Session 5 — Chad's first RIDE, and the octave it cost (2026-08-20)

> "the engine sound is good but it need to come down one or two octaves it
> really high pitched" ... "and decibels"

Two dials moved, both derived rather than guessed, because the offline preview
tool written for this made them measurable.

### The pitch — and it was the RECORDINGS, not only the ratios

Measured off the shipped assets (direct spectrum, 0.75 s window, 2.5 Hz bins):

| asset | dominant partials | implied rpm (fires = rpm/60 × 3) |
|---|---|---|
| `sled_idle_loop.wav` | 135–150 Hz, nothing below | ~2800 — kernel idles at **1700** |
| `sled_run.wav` | 460–520 Hz (peak 517), lesser 247–260 | ~10 000 — engine tops out at **8000** |

So neither recording is what its filename says, and the playback ratios were
then pitching them **up** again: the riding bed reached 517 × 1.5 = **776 Hz**
at full throttle. That is the whine.

**`kSledPitchPedestal = 0.50`** (`render/sled_audio.h`) — one octave, applied to
both ratios in `sled_drive.h`. One, not the two Chad also offered, and the
reason is that the synthesized TONE was never wrong: it is derived from the real
firing rate (60 Hz idle, 350 Hz pinned). One octave lands the recordings **on**
it, so all three layers become one engine instead of three at different pitches.
Two octaves would put the idle bed at 35 Hz — an octave *below* the firing rate
it is supposed to be the sound of, and under most speakers.

Verified by rendering the real synth offline, not by arithmetic:

```
throttle 0.0  ->  dominant  72.5 Hz   vs derived  60 Hz   (+0.27 oct)
throttle 1.0  ->  dominant 350.0 Hz   vs derived 350 Hz   (+0.00 oct)
peak -3.2 dBFS, ffmpeg flat factor 0.0  =>  the soft clip is NOT being driven
```

That last line mattered: if the synth had been clipping, the harshness would
have been generated *before* the mix trim and no amount of level would have
fixed it. It is not.

It is a **pedestal, not a re-range** — it scales each ratio whole, so the sweep
across the rev range is untouched (idle still ×1.35 end to end, run ×1.875) and
only the floor moves. Downward resampling cannot alias, so there is no quality
cost. **If it is still high, halve this one number for his second octave.**

### The decibels

`kSledFlownLevel` 0.62 → **0.31**, exactly −6 dB — the standard "audibly
quieter" step rather than a nudge nobody can hear.

⚠ **This drops the sled below the gun channel, and a `static_assert` I wrote
forbade exactly that.** The assertion said a vehicle engine must sit above the
guns. That was an argument about what an engine deserves, welded into the build,
and Chad's ride went straight past it. It is retracted — left visible in
`mix_levels.h` with the reasoning, and replaced by a floor that is a real
property: the machine you are riding must not end up quieter than the music
behind it. **An assertion is for what must hold for the code to be correct, not
for a balance nobody has heard.** The matching `REQUIRE` in `test_soundscape.cpp`
went the same way.

⚠ Note the octave drop *by itself* reduces perceived loudness (brightness reads
as loud). If it is now too quiet, `kSledFlownLevel = 0.44` is −3 dB instead.

**2026-08-23 — it was, and 0.44 is what shipped.** Chad: *“please raise
snowmachine sounds up”*. The paragraph above had already called this shot, so
the move was the one it named rather than a fresh guess: `kSledFlownLevel`
0.31 → **0.44**, +3 dB, half the distance back toward the 0.62 he rejected by
ear three days earlier. Not the whole distance — 0.62 is an experiment already
run and answered, and the pitch that made it harsh has since come down two
octaves, so the smallest question his ear can resolve is the right one to ask.

Nothing else moved. `kSledMaster` (the balance BETWEEN the four synth layers)
and `kSledVoiceTranspose` (the pitch) are untouched, and the mastering targets
in `soundbank.manifest.tsv` are untouched — the bank was not rebuilt, because
this is balance and balance lives in the trim, which is the same rule the
bagpipe row settled on 2026-08-18. Derived: `kSledStreamLevel` 0.195 → 0.277.
Every surviving invariant still holds with room — under the music bed
(0.277 vs 0.599), over the floor (0.150), `stream × master` = 0.216, well
under unity. It is still *below* the gun channel (0.347), which is where his
first ride put it and where the retracted assertion no longer objects.

### New: `tools/sled_synth_preview.cpp`

Standalone, one `g++` line, no raylib, no CMake target — the convention
`wind_synth_preview.cpp` and `engine_synth_preview.cpp` already set. It renders
the real `render::SledSynth` to a WAV and prints the drive state, and it is why
this session could answer "how high, exactly" with numbers in seconds instead of
reading constants. Re-run it after any move of the pedestal or the layer gains.

Two new cases in `test_sled_audio.cpp` pin the *reasoning*, not just the value:
that each sampled layer lands within a third of an octave of the derived firing
rate, and that the pedestal scales the ratios whole rather than flattening the
rev sweep. The old ratio bounds were flat literals (`> 0.5`) that the first
octave broke immediately; they now derive from the pedestal and follow it.

## Session 6 — the second octave, and splitting taste from measurement (2026-08-20)

> "okay it still reads like it needs to come down an octave"

He said "one or two" the first time; this is the second, and it is granted.

### The reasoning that lost

Session 5 landed one octave and argued that one was correct because it put every
layer exactly on the derived firing rate (rpm/60 × 3) — verified at +0.27 and
+0.00 octaves. That argument was *fidelity*, and `CLAUDE.md` rules this project
by **feel, not fidelity**. A derivation that keeps producing a sound Chad does
not want is a derivation being used as an argument — the same mistake as the
`static_assert` retracted an hour earlier, and this time it was a unit test
doing it.

### Two constants, two jobs

The single `kSledPitchPedestal` couldn't answer "which half of this is the
taste?", so it is now the product of two named things:

| constant | value | what it is |
|---|---|---|
| `kSledSourceCorrection` | 0.50 | **A measurement.** The recordings are not at the rpm their filenames claim (140 Hz ⇒ ~2800 for an "idle" that runs at 1700; 517 Hz ⇒ ~10 000 on an engine that stops at 8000). |
| `kSledVoiceTranspose` | 0.50 | **Chad's ear.** How far the sounded voice sits below the physics. The dial to move. |

`kSledPitchPedestal = 0.25` is their product, applied to both sample ratios.

### The transposition moves TIMBRE, not just pitch

`kSledVoiceTranspose` also carries the tone's **fundamental** (new
`sled_voice_hz()`, which the synth now uses instead of `sled_fire_hz()`) and its
**lowpass sweep** (420–4200 Hz → 210–2100 Hz).

That last one is not an extra. Resampling a recording moves its whole spectrum
for free; the synth tone does not get that for free. Dropping f0 while leaving
the brightness envelope where it was gives a thin bright harmonic stack over a
low note — which reads high however low f0 goes. Transposing an instrument moves
all of it. `sled_fire_hz()` stays exact and unscaled beside it, so the physics
remains readable next to the taste applied on top.

### Measured, not assumed

```
throttle 0.0  ->  dominant  32.5 Hz   (was 72.5)
throttle 1.0  ->  dominant 175.0 Hz   (was 350.0)   exactly one octave
peak -5.6 dBFS, flat factor 0.0  =>  still not driving the soft clip
```

⚠ **One risk fell out of the band split, and it is real:**

| | below 50 Hz | above 50 Hz | above 100 Hz |
|---|---|---|---|
| riding (throttle 1) | −39.4 dB | **−18.3 dB** | −18.6 dB |
| idle (throttle 0) | **−25.0 dB** | −29.0 dB | −36.4 dB |

The ride is healthy — essentially all its energy is in the audible band. The
**idle bed now has more energy under 50 Hz than over it**, so a speaker that
rolls off below ~150 Hz will keep the ride and lose most of the idle. On
monitors or headphones it is a deep burble, which is right.

If the idle goes *missing* rather than deep, the fix is **not** to undo the
transpose (the ride would go back up with it) but to lift the idle bed alone —
its own honest source correction is 1700/2800 = **0.61**, not the 0.50 the pair
shares.

### Tests

The coherence case was rewritten, not relaxed. It no longer asserts against the
physical firing rate; it asserts that **all layers sing at the same transposed
pitch**, against `sled_voice_hz()`. That property is structural, survives any
future octave, and is the thing that actually matters — a sled coming apart into
two engines a fifth apart is the failure worth catching. Two new cases pin that
the timbre transposes with the pitch, and that the two constants keep their
separate jobs.

## PENDING / not wired
- ~~**Sled engine**~~ — ✅ WIRED 2026-08-20, see session 4 above. The hold was
  "not as flat loops"; `render/sled_synth.h` replays both at a rev-driven
  resample ratio, so the condition is met rather than dropped.
- **Footsteps** (`sfx/step_*.wav`) — Chad: "my sudburian cannot walk yet so
  defer for now the walking sounds". Mastered, matched at -22 LUFS, unwired.
- **Breathing / train ambience** — mastered, not yet placed. (The wolf and
  wildcat cries WERE placed, 2026-08-20 — see session 3 above.)
- **Title card** — the splash draws `[ TITLE CARD PLACEHOLDER ]`; Chad has not
  picked from `Game_name_candidates.txt`.
- **winter-gi mirror** — held. That tree's `app/main.cpp`, `render/draw.*` and
  `render/post*` are dirty under the animation agent. The audio init/feed/
  teardown blocks there are byte-identical to this tree (offset +15 / +687), so
  the transplant is mechanical once they land. Assets do NOT travel by git
  between trees — use `install_soundbank.sh`.

## Verify by EAR (the gate cannot)
`ctest` never runs `seads.exe`, so this ships green even if every stream is
silent. Listen on `build-play\seads.exe` (RelWithDebInfo) — never judge audio
cost from a `build/` Debug run.

## Feel-decisions still open (Chad drives)
- Master mix balance engine vs wind (engine should sit UNDER wind at redline).
- Idle vs cruise vs redline RPM anchors for the engine pitch map.
- Creak frequency + the speed threshold that gates it on.
- Whether prop chop is a separate channel or folded into the engine layer.

---

## Session 5 — 2026-08-24: the train that had no caller, and two animals nobody could hear

Chad, in one message: *"where is that train sound when flying over chelmsford? I
wanted a train sound in that vicinity, similar to the mechanism of the wildcat
hiss or the wolf howl, come to think I havent heard the wolf howl in seads-recon
play yet.... Can you check if those sounds are in there and the volume turned up
on those?"*

Three separate faults, and only one of them was volume.

### 1. The train had no caller — for a week

`render/town_ambience.h` was written 2026-08-17: the density gate, the four
uneven gaps, the grace period, the context-aware cadence, all of it pure and
covered by nine unit tests. `sfx/train_chelmsford_pass.wav` was built, mastered
and installed the same day. The manifest row said `wired=yes`.

**Nothing in `app/main.cpp` had ever included the header.** The asset was loaded
by nobody and played by nothing. The gate could not see it: `ctest` never runs
`seads.exe`, so a channel with no caller is indistinguishable from a channel
that works.

This is the SECOND time — `render/sled_synth.h` sat finished and uncalled from
08-17 to 08-20. The lesson is now in the manifest's `wired` column definition
rather than in the comments of the rows that got caught: **`wired=yes` means a
caller exists, and the way to know is to grep `app/` for the header.**

### 2. …and wiring it as specified would still have been silent from the air

The more interesting half. `town_ambience.h` asked the app for "how many
buildings nearby", and the obvious source is `world::BuildingColliders`, whose
index cells are ~92 m of ground. Ask that from an aircraft and `near_town` is
true for **under a second per cell** — you cross 92 m at 110 m/s almost
instantly. The scheduler's clock only advances while you are near a town, so an
overflight of Chelmsford banked perhaps 5 s against a 40 s grace and gaps of
four to eight minutes. Tens of passes per train. Indistinguishable from broken,
and that is precisely the report we got.

So "nearby" became a **radius**, per context: 300 m on the sled, **4000 m** in
the air (a 65–90 s pass, one real gap's worth of clock per overflight). The
flying gaps came down with it — `{255, 390, 310, 470}` → `{130, 190, 155, 225}`.
Those old numbers multiplied a rarity the geometry already supplies: the air is
rare *by construction*, because the clock stops the moment you leave town.

New: `world::BuildingColliders::prisms_near_m(pos, radius_m)` — O(cells) distinct
count with a stamp buffer, so a building span-inserted into six cells counts
once. Brute-force parity test at the u seam and both poles in
`test_buildings.cpp`.

### 3. The wolf was armed, but the bubble was a bullseye you had to hit

The animals *were* wired (08-20) and Chad's binary had them. They never fired
because `kCryRangeFlyingM` was 3200 m around a single point: the wolf sits over
Coniston, the cat over Onaping/Dowling, and a route that misses by 4 km hears
nothing and gives no hint anything was there. → **5000 m** (~2.4× the area for
the same flight path, still 10 km of arming against 42 km of pump separation).

### 4. And then the volume — which was being held down by a wrong assertion

`test_pump_ambience.cpp` asserted `cry_gain(...) <= 1.0`, justified in its own
comment as *"SetSoundVolume, which has no headroom above unity"*. **That is
false about raylib.** `SetSoundVolume` forwards to `SetAudioBufferVolume`, which
is `buffer->volume = volume;` with no bound (`raudio.c`) — above 1 amplifies, it
does not saturate. The real ceiling is the asset's own true peak, and these
assets are swimming in it: `build/soundbank_report.txt` measures wolf_cry at
**−16.6 dBTP**, wildcat_cry at −6.6, the train at −11.8, and everything is
multiplied by `kGameplayBusGain` (0.63) before the mixer sees it.

A second assertion compared the cry's linear gain against the music's — but the
cries master to −24 LUFS and the bed to −20, so at equal gain the animal is
already 4 LU down. The check was 4 dB stricter than the property it claimed to
enforce, and that margin is exactly what Chad could not hear. Both are now
measured properly: delivered peak in dBTP against clipping, delivered loudness
in LUFS against the bed.

| dial | was | now |
|---|---|---|
| `kCryGroundGain` | 0.92 | 1.30 (+3.0 dB) |
| `kCryFlyingGain` | 0.60 | 1.05 (+4.9 dB) |
| `kCryEdgeFloor` | 0.42 | 0.55 |
| `kTrainGroundGain` | 0.90 | 1.20 (+2.5 dB) |
| `kTrainFlyingGain` | 0.62 | 0.95 (+3.7 dB) |

Loudest path in the system is the cat at zero distance on the ground:
`1.30 × 0.89 × 0.63 = 0.73`, landing its −6.6 dBTP peak near −9.4. Nothing is
close to clipping.

### The lesson worth keeping from this one

Two of the four faults were **assertions that encoded a belief rather than a
measurement** — "no headroom above unity" (wrong about the library) and a
loudness comparison done in linear gain (wrong about the units). Both were
green, both were confidently commented, and between them they held the animals
4–5 dB below where the ear wanted them. `mix_levels.h` already carries this
lesson from the retracted sled/gun assertion. It keeps coming back: **a test
that pins a number you have not measured is a guess with a build behind it.**

---

## Session 6 — 2026-08-24, the same evening: three flies, and a signature

Session 5 wired the train and guessed at levels. Chad then flew it three times in
one evening, and the mix went from "did I hear that?" to signed.

### Fly 2 — it works, and now it is a balance problem

*"okay yes I heard the train, other sounds like the cat hiss, turns the music
down a bit and make the sound loud enough."*

**The wiring was confirmed by ear** — the train, and the **cat**, which was the
first report of either animal since they shipped on 08-20. Everything after this
point is balance, not plumbing.

Both halves moved, ~3 dB each, closing the gap by ~6 while the bed moved one
honest step: `kMusicFlownSurface` 0.95 → 0.67, `kSledFlownLevel` 0.44 → 0.62,
train 1.20/0.95 → 1.70/1.45, cries 1.30/1.05 → 1.75/1.55.

This partly walked back **Fly 1's music number**, deliberately. 0.95 was the fix
for *"the wind and the engine are too loud to hear the music"* — ruled in a mix
with no train, no animals and a silent snowmachine. Three channels had arrived
since. The pinned value in `test_soundscape.cpp` was **re-pinned, not unpinned**:
that case exists so a *retune* cannot walk the balance back silently, and a
ruling is not a retune.

It also retracted **"the animals sit under the music, not over it"** — an
argument about what an ambience deserves, which his ear had just contradicted.

### Fly 3 — the ruling that reversed the doctrine

*"they barely make it over the music and its an environmental sound effect, not
very immersive if not quite a bit louder than the music … right now I have to
question if I heard them over the music."*

**"I have to question if I heard them" is a harder spec than "louder."** A sound
you have to *ask* about has failed even when it was technically audible. That is
what bought +6 dB — a doubling of perceived loudness — rather than another +3,
which would have moved the sounds and left the question standing.

| | fly 2 | fly 3 | |
|---|---|---|---|
| `kTrainGroundGain` / `kTrainFlyingGain` | 1.70 / 1.45 | **3.40 / 2.90** | +6.0 dB |
| `kCryGroundGain` / `kCryFlyingGain` | 1.75 / 1.55 | **3.50 / 3.10** | +6.0 dB |
| `kSledFlownLevel` | 0.62 | **0.98** | +4.0 dB |

Delivered against the bed: train **+10.1 dB**, wolf **+10.4**, cat **+7.0**,
sled **+3.3**.

The sled's smaller step is his sentence, not a rounding: *"more lasting so it can
raise … a little bit more but not too much"*. A sustained channel held at a
one-shot's level is the one that fatigues.

#### The cat could not have the full +6, and that is an asset fact

Both animals master to −24 LUFS, so they are **equally loud**. They are not
equally **peaky**: wolf_cry −16.6 dBTP, wildcat_cry −6.6 — crest 7.4 dB against
17.4, because the cat is a shriek and its transient *is* the sound. At a matched
trim the cat would deliver −0.7 dBTP.

So `kWildcatCryGain` stopped being a loudness correction and became a **peak
limit** (0.89 → 0.68): the cat gets +3.7 dB where the wolf gets +6.0, and the two
animals are **no longer loudness-matched**. That is affordable only because the
pumps are 42 km apart and nobody can hear both — unlike the footstep set, which
must agree with itself. The remaining 2.3 dB needs a **new recording**; limiting
the shriek is what the cat's own manifest row forbids, on the same grounds the
black-stope blast refuses it.

The cry ceiling went **per-animal** at the same time. A ceiling is an asset's
true peak and nothing else, and these two are 10 dB apart on exactly that; one
shared number could only ever guard one of them.

#### And it retracted the sled's "under the music" assert

`static_assert(kSledStreamLevel < kMusicSurfaceGain)` was **true to his rulings
when it was written** — and that is the point. Both of those rulings were made
about a mix in which the engine was the only thing competing with the bed. Fly 3
is the same ear on a fuller mix, and *"an environmental sound effect … quite a
bit louder than the music"* is not a retune of one channel. **It is a ruling
about what a bed is:** the music is the layer you hear past, the world is the
layer you hear.

The floor survived untouched — the machine you are riding must not be quieter
than the music behind it — because that one only gets *more* true as this moves.

### ⭐ Fly 4 — SIGNED

*"good the sounds are right now"*

**The mix is now an approval, not a derivation.** Every level is one he has
heard. `render/mix_levels.h` carries this as ruling 3, with the rule it implies:
do not retune these to make a policy tidier — move one only when he says to, or
when a new channel arrives and changes what the mix is. That last clause is not a
loophole; it is exactly how ruling 1's `0.95` was legitimately undone.

⚠ **What the signature does NOT cover:** the **wolf** (never tested — it was
moved with the cat on his *"I'd imagine it suffers a similar problem"*), the
second engine **octave**, and the train's **cadence**, which one evening cannot
judge against a 130–225 s gap.

⚠ **The next thing to bite is the master sum.** Every gameplay channel rose while
the bed fell, so a train pass over a running sled now adds much closer to full
scale. If a loud *moment* distorts rather than a particular sound, the dial is
`kGameplayBusGain` — it lowers everything together and so leaves every approved
**ratio** intact. Never fix a sum by trimming whichever channel was playing.

### The lesson, and it is the fourth time

Fly 2 and fly 3 each ended by **deleting an assertion that encoded a belief**:
"an ambience sits under the bed", "the engine sits under the bed". Both were
green. Both were reasonable. Both were written before the mix they described
existed, and between them they were the reason Chad had to ask whether he had
heard anything at all.

> **A test that pins a balance nobody has heard is a guess with a build behind
> it.** Pin the physics — clipping, headroom, format. Report the balance. Leave
> the taste in a named dial where one ruling moves it.

## Session 7 — 2026-08-24: the church bell, and the first polyphonic channel

Chad, in one message: *"Okay Ive added church_bell to sound effects, please make
it play different amounts of plays at random, sometimes it rings 3 times,
sometimes 7 times, up to twelve to simulate the number of chimes it plays based
on the hour. It should play about two minutes after the train might, in and over
chelmsford"* — and, when asked where the floor was, *"from one chime up to 12, 3
was an example, it should be random though."*

WIRED and audible. One new mastered asset, one new pure header, one glue block,
one alias ring.

| Piece | Where |
|---|---|
| Policy (count, cadence, delay, gains, ring size) | `render/bell_ambience.h` — pure; `<algorithm>` + the lane's own `audio_dsp.h` |
| Asset | `sfx/church_bell.wav` (4.44 s, mono, AMBIENCE −24) |
| Glue | `app/main.cpp`, in the same town block as the train |
| Tests | `test/unit/test_bell_ambience.cpp`, 18 cases |

### The peal is triggered, not recorded

The source is **one strike** — attack inside the first 85 ms, decaying to digital
silence by 4.35 s. That is the right shape, and it is not an accident of what
Chad happened to find: **a canned peal asset can only ever ring one number of
times**, which is the opposite of what he asked for. The peal is built by
striking the one sample N times, so the count is a code decision and 1..12 costs
nothing.

### This is the first channel in the bank that needs POLYPHONY

Every other one-shot here — the train, the blast, both animals — is a single
raylib `Sound`, where `PlaySound` on a playing voice **restarts** it. That has
always been safe because every scheduler's gap clears its asset's length; the
blast's manifest row was re-cut to 13 s precisely to keep it so.

It cannot be true here. The bell rings **4.44 s** and is struck every **2.4 s**,
so a strike would truncate the one still ringing — up to eleven times a peal, and
a bell cut off mid-decay is a step, which is a click. So the strikes go round a
ring of `LoadSoundAlias` voices (raylib 5.5): separate voices over one copy of
the sample data, four handles and no extra memory.

`kBellVoiceCount` is `static_assert`ed against `ceil(kBellChimeLen /
kBellChimeGapSec) + 1`, so a re-cut of the row or a shortened gap **fails the
build** instead of quietly bringing the truncation back. That is the kind of
assertion this lane has settled on keeping: a property of the code, not a balance
nobody has heard.

**Why 2.4 s.** Measured off the asset, not chosen: the strike is −9.4 dBFS at the
attack and −19.7 by 2.4 s, so a new strike lands *into* the ring-out of the last
rather than on top of its attack. It also sits inside the 2–3 s a real tower bell
of this size takes between strikes, and it makes a twelve-count 28.8 s — about
the length of the train pass it follows, which is a useful check on "is this too
long an event".

### The count is DRAWN — a deliberate break with the train next door

`render/town_ambience.h` uses a fixed four-gap table and argues for it: four
uneven gaps read less mechanical than a uniform draw, `<random>` is banned, and
the gate needs reproducibility. **That argument does not transfer**, and Chad's
correction is what makes the difference explicit.

A train gap is not counted by anyone. **A chime count is counted** — it is the
entire content of the sound, it is what "what time is it" means — and a fixed
table would be a bell ringing the same twelve hours in the same order forever.
A player would hear that inside one session.

So the count comes from `render::WSXor`, the **fixed-seed xorshift this layer
already owns** (`audio_dsp.h`; the gun and wind synths draw from it). Same
sequence every run, so a test can assert the exact peal it produces, while the
sequence is long enough that nobody hears it repeat. *Deterministic and random
are not opposites here; **unseeded** is what was banned.*

One constraint on the draw: **never the same count twice running**. At 1-in-12 a
plain uniform draw hits that about every twelfth peal, and a bell that rings
seven and then seven again does not read as two hours — it reads as a loop that
got stuck.

The **delay** stayed a table (105/126/114/135 s, mean exactly 120), and the split
is the point: the ear counts chimes, not the seconds since a train.

### "Two minutes after the train" — keyed, not coincident

Armed off `TrainAmbience`'s firing tick, so the two are one event with a gap in
it rather than two schedules that collide at random. Armed off the **scheduler**,
not off the `PlaySound`: if the train asset failed to decode, the town still has
a church. Making one missing file silence two channels is the coupling the
blast's music-duck already got wrong once.

Stacking is refused. Train gaps run 105–225 s against a ~120 s delay, so a second
train arriving mid-peal is the *normal* case — two overlapping peals would give a
count nobody can read, which is the one thing this channel must not do.

### The countdown PAUSES, the peal ABANDONS — and they are different questions

The train's clock pauses when you leave town, because the schedule is a fact
about the world: the train is out there whether or not you are, and being near
town is only when you can hear it. **The bell's countdown inherits that.**

A peal *in progress* is a fact about **your ears**. Pausing it would resume at
chime four of seven when you came back minutes later — a bell that waited for
you, which is the "stuck to the camera" failure this lane keeps designing
against. So leaving town **ends** the peal, and so does descending into the
workings: the `cave` branch calls `abandon_peal()` while leaving the countdown
intact, which is the one thing the underground hold must not do to it.

### The mix — derived from a ceiling, not from taste

The natural anchor is the signed train (3.40 / 2.90). The bell cannot have that
level, and the reason is measured: the asset delivers **−8.8 dBTP**, ×
`kGameplayBusGain` 0.63, and **two strikes are audible at once** — worst-case
coherent sum ×1.30. At 3.40 the overlap goes over full scale.

    0.363 (asset TP) × 0.63 (bus) × 1.30 (overlap) × gain ≤ 0.708 (−3.0 dBTP)
    ⇒ gain ≤ 2.38    →  kBellGroundGain 2.35, kBellFlyingGain 2.00

−3.0 dBTP is the same house rule `kCryCeilingWolf` / `kCryCeilingWildcat` are
already derived against. The air keeps the train's ratio under the ground
(0.853), for the train's reason.

**Where that actually lands is the check that matters.** One strike delivers
0.537, a peak near **−5.4 dBTP**; the signed train pass delivers 0.551, **−5.2**.
The bell is peak-matched to the train Chad flew and called right, and only
touches the −3 line when two strikes coincide. The 3.4 dB its *trim* sits under
the train's is not the bell arriving quieter — it is the asset being 3 dB peakier
to begin with.

The row also verifies **1.0 LU hot** (−23.0 against −24), the same short-mono-file
meter disagreement the wildcat row documents at length. Left uncorrected: the
derivation above is against the *measured* peak, which already contains it, and
fly 3 ruled that this class of sound errs loud.

⚠ **If it is too quiet, do not simply raise the trim.** It is already near the
overlapped ceiling. The next dB has to come from widening `kBellChimeGapSec` past
`kBellChimeLen` (buys the whole 2.3 dB, costs the tower's ring) or from a
re-master. Trimming past the ceiling buys loudness by clipping the attack, which
is the part of a bell that carries.

### Tests — three of them shaped against a specific wrong answer

- **The count is drawn, not cycled** — fails any fixed table of period ≤ 12,
  which is the mistake a next agent copying `town_ambience.h` would make.
- **Leaving town abandons rather than pauses** — a scheduler that paused passes
  every other case in the file.
- **The same count never twice running** — fails a plain uniform draw.

Plus the non-vacuity habit this lane learned the hard way: the delay table is
checked for *spread* as well as mean (four identical 120s would pass a mean check
and be a metronome); the abandon case *draws until it gets a peal long enough to
interrupt* rather than bailing out with a `SUCCEED` on a short one; and the
headroom case asserts the product is near the ceiling as well as under it — *a
bell 10 dB below the bound would pass and fail the ear, which is the failure this
lane keeps making.*

### ⭐ SIGNED on the first listen — which has not happened here before

*"ookay bell sounds good!"* and, when I hedged that only the peal had been
approved and not the mix, *"I heard it, it passes my review so thats why I said
to push it."*

So `kBellGroundGain` / `kBellFlyingGain` are **heard** numbers and
`render/mix_levels.h` ruling 3 now covers them. Every other channel in this lane
took between two and four flies to get here; this one took none. **The difference
is what its level was derived against.** The sled's first level was an argument
about what a vehicle engine deserves, and the animals' were held down by two
assertions that encoded beliefs — all three were retracted by an ear. The bell's
was derived against a measured ceiling and then checked against a level Chad had
already signed (one strike at −5.4 dBTP against the train pass's −5.2). **Deriving
against a measurement instead of against a conviction is the transferable part;
the one-pass approval is just what it looks like when that works.**

⚠ A single listen still cannot judge the **cadence** — a ~120 s delay behind
105–225 s train gaps is a multi-session question — nor whether the **count
distribution** reads as hours over a long ride. Both are dials, and neither is a
fault if he raises it. The gate remains blind to all of it.

### The gate found one thing, and it was my test

`a second train mid-countdown does not stack a second peal` came back `2 == 1`.
The draft ran a flat 300 s window and counted peal starts — but once a peal
**completes**, the tower is free and the next train is entitled to arm another
one. That is the channel working. The window is now bounded by the first peal's
last chime, which is the interval the property was ever about.

It is worth keeping because it is the **inverse** of the mistake this lane has
now made four times. Usually the assertion encodes a belief and the code is
right. Here the assertion encoded a sloppy *window*, and the red was real
information: it said the scheduler recovers cleanly, which nobody had asked for
out loud.

Gate: 1550/1556. The five reds are the four known GI4 sled debts plus the
enemy-AI `probe P-F`, which the 08-24 handoff already recorded as unowned.
Nothing audio is red.

---

## Session 9 — 2026-08-28: the first ruling that takes the snowmachine BACK DOWN

*"turn down the volume level of the snowmachine a few notches like 10db, maybe
lower frequency so not so high pitched a bit lower too, though close to where it
should be it is a bit of a pain on the ears."*

Two dials, one message, and **the first time this channel has ever been asked to
get quieter since his first ride.**

| constant | was | now | |
|---|---|---|---|
| `kSledFlownLevel` (`render/mix_levels.h`) | 0.98 | **0.31** | −10.0 dB |
| `kSledVoiceTranspose` (`render/sled_audio.h`) | 0.50 | **0.40** | −3.9 semitones |

`kSledPitchPedestal` follows to **0.20** as their product. Nothing else moved.

### He named the size, so the lane's 3 dB step does not apply

Every previous move on this dial was ±3 or ±6 dB, chosen by me as "the smallest
honest step". Here he said **"like 10db"**. Splitting that into halves would ask
him to re-fly a complaint he had already quantified, so it went in one step.
*"Maybe lower"* is the tail he left open — deliberately **not spent** here, so
there is somewhere to go if 0.31 is still too much.

### 0.31 is exactly where his FIRST ride put it, and that is worth saying out loud

The road back: 0.31 → 0.44 → 0.62 → 0.98 → **0.31**. Three raises, each
answering *"I cannot hear it over the music"*, each correct about **audibility**.
None of them tested whether the channel had become **painful** rather than merely
present — which is a different complaint, and the one he is making now.

So the raises were not wrong. They were climbing past the point where the answer
stopped being *louder* and started being *the bed is in the wrong place*. His
first ear found this number under a different question and landed on the same
one.

The retracted *"under the music"* assert **stays retracted**. 0.31 does sit under
the bed again — but it got there by his volume ruling, not by the argument that
assertion encoded, and re-welding it would re-arm the trap that fired on fly 4.

### The pitch: the pre-authorized octave was NOT taken

`sled_audio.h` has carried a note since session 6 saying *"0.25 for a third
octave"* — pre-computed, sitting there, ready. It was not used, and that is the
decision in this session I would most want a future agent to understand.

Twice before he said *"come down an octave"*, flat, and got an octave. This time
he said **"a bit lower"** and, in the same breath, **"close to where it should
be"**. A pre-computed step is a convenience, not a standing order; taking 0.25
because it was written down would have overshot a man who had just said the pitch
was nearly right.

0.40 is 0.80×, about a major third — comfortably above the "is it my imagination"
floor for pitch, while leaving most of the octave unspent. If he says it a fourth
time: **0.31** is the rest of that octave, **0.25** is the octave itself.

### Measured before and after, and the measurement corrected me

Both pitches rendered through `tools/sled_synth_preview.cpp`, measured with
ffmpeg (2-stage biquad splits at 50/100 Hz; 0.75 s Hann window, 2.5 Hz bins):

| | old (pedestal 0.25) | new (0.20) |
|---|---|---|
| riding, dominant | 175.0 Hz | **140.0 Hz** (0.80×) |
| riding, fire-rate partial | 352.5 Hz | **282.5 Hz** (0.80×) |
| riding, RMS >50 Hz | −18.3 dB | −18.4 dB |
| riding, RMS >100 Hz | −18.8 dB | −19.5 dB |
| idle, dominant | 27.5 Hz | 27.5 Hz |
| idle, RMS <50 Hz | −26.0 dB | −24.7 dB |
| idle, RMS >50 Hz | −32.4 dB | −34.9 dB |

Two things fall out of that table.

**The instrument moved as ONE.** Both riding partials scaled by exactly 0.80 —
the fundamental and the fire-rate partial together. That is the claim the session-6
"transposition moves TIMBRE" note made in prose, now checked and true.

**And the idle-bed risk note was WRONG about causation.** I predicted this move
would *spend* the idle margin. The measurement says the margin was **already
spent at 0.50**: the idle's dominant partial is 27.5 Hz at *both* pitches, under
where any laptop or phone speaker reproduces anything. This change widens the
sub-50/over-50 split from 6.4 dB to 10.2 dB — real, but a **worsening of a
condition that predates it**, not the cause of it.

That distinction is the whole value of measuring. If the idle burble is ever
reported missing, do **not** read it as this ruling's fault and do **not** undo
the transpose — the ride would climb back with it, and the ride is the only layer
he has ever complained about. The fix is to lift the idle take alone, whose own
honest source number is 1700/2800 = **0.61** rather than the 0.50 measured across
the pair. That needs `kSledSourceCorrection` split per sample (`sled_drive.h`
applies one pedestal to both ratios today) — a real change, not a dial, so it is
**named and not done on speculation**.

### The two dials compound, and which one to reach for

Lower voice **plus** lower gain is more than either alone — the "a lower-seated
two-stroke carries less perceived loudness" argument already written into the
0.44 note runs the same way in this direction. So the machine will read as *more*
than 10 dB quieter.

- Too quiet now → **`kSledFlownLevel`** (0.44 is +3 dB, 0.55 is +5). Not the pitch.
- Still too high-pitched → **`kSledVoiceTranspose`** (0.31, then 0.25). Not the level.

Keeping those separate is the same discipline as splitting `kSledSourceCorrection`
from `kSledVoiceTranspose` in session 6: one dial per complaint, so a ruling never
has to be guessed at afterwards.

### A stale comment, found on the way

`tools/sled_synth_preview.cpp` carried a measured block labelled *"at
`kSledPitchPedestal = 0.50`"* — stale through **two** pedestal moves, printing
ratios (`idle_ratio 0.500`) the tool itself no longer produces. It is refreshed
to today's numbers, keeps the old entry as history, and now says in the file that
**if the printout disagrees with the comment, the printout is right.**

That is how a comment stops being a measurement and quietly becomes a belief —
the same failure mode as the three static_asserts this lane has already
retracted, just in a slower-moving place.

### Verify by EAR, as always

`ctest` cannot hear this. Both changes are pure constants and the gate only
proves the arithmetic around them. `build-play\seads.exe` is rebuilt with them.
