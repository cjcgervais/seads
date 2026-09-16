# The SEADS soundscape — how sound gets from a recording into the game

**Status 2026-08-18.** The soundscape rung is flown, signed and on `main`
(`bd8e043c3`, gate 1190/1190). This document is the standing reference for the
audio lane: where the pieces live, why the levels are what they are, and what to
do when a checkout is silent.

It exists because `tools/audio/soundbank.manifest.tsv` cited a
`README_SOUNDSCAPE.md §3` that had never been written.

---

## §1 The two lanes, and the one authority

Audio in this project has two homes, deliberately:

| | Where | Holds | Repo |
|---|---|---|---|
| **The sound lane** | `D:\audio_tracks` | source recordings, Chad's briefs, the build report | its own private repo, `cjcgervais/seads_audio_lane` — **media tracked** |
| **The game repo** | `tools/audio/` | the manifest + the two scripts | this repo |

The two repos make **opposite** calls on binaries, for one reason. Derived bytes
stay out of this repo's lean, widely-cloned history because they can be rebuilt
from the lane, byte-for-byte, by a pipeline verified deterministic. The lane is
where the chain of custody ends — the sources cannot be rebuilt from anything —
so that is where the bytes have to live, and they are pushed to a remote.

The **manifest is the single authority** on what ships. It is the input spec for
`render/music_director.h`, `render/intro_sequence.h`, `render/mix_levels.h`,
`render/stope_reverb.h` and the loaders in `app/main.cpp`, so it lives beside
that code and moves through the same review. The media stays in the lane because
it is large, and because the lane belongs to the human sound engineer.

## §2 The two kinds of sound, and the line between them

Not everything is a recording, and the split is a design ruling, not an accident.

**Procedural** — `wind_synth`, `bagpipe_throttle`, `gun_synth`, `sfx_synth`.
Anything whose job is to convey **speed** is synthesized in real time and driven
by a live parameter. A static loop there "kills the sense of speed instantly"
(`audio_ideas/wind_audio.md`).

**Recorded** — everything in the soundbank. Music, the scripted intro, ambience,
one-shots. None of it is speed-coupled.

The throttle voice sits on the seam and shows the correct pattern: a **recorded**
bagpipe drone, decoded once, then **resampled live against throttle** by
`render/bagpipe_throttle.h`. That is why `loops/sled_idle_loop.wav` and
`loops/sled_run.wav` are mastered but marked `wired=no` — a snowmachine engine is
a speed-coupled layer, so wiring them as flat loops would regress the doctrine.
They wait for a synth that drives them the way the drone is driven.

## §3 Loudness policy

Every shipped asset is normalised to an integrated **EBU R128** target chosen by
its **bus**, and true-peak limited. Sources arrived spanning **21.6 LU**
(−8.7 to −30.3 LUFS) with the sled idle clipping at **+1.4 dBFS**, so nothing
ships unnormalised.

| Bus | Target | Why |
|---|---|---|
| `VOICE` | **−16** | loudest; intelligibility wins, it is the narrative payload |
| `SFX` | **−18** | interactive, must cut through the beds without fatiguing |
| `MUSIC` | **−20** | a bed, sits under everything |
| `AMBIENCE` | **−24** | the furthest layer back; felt, not listened to |

True-peak ceiling is **−1.5 dBTP** by default, leaving headroom for Vorbis codec
overshoot and for raylib's float mixer summing buses.

**Rows depart from the table only with a written reason in the manifest**, and
there are four kinds of reason:

- *The crest is the content.* The blast measures −15.2 under a linear pass and
  cannot reach −14 without limiting the life out of the transient. It ships at
  −19.5/−1.0; the occlusion Chad asked for is delivered by the music duck in
  `render/music_director.h`, not by flattening the explosion.
- *A set must match itself.* The three footstep assets are all −22, not −18: a
  surface that jumps 2 LU when you walk from hardpack onto deep snow reads as a
  bug, not as tuning.
- *Headroom runs out.* See §4.
- *A signed mix is evidence.* The bagpipe drone ships ~8 LU hotter than every
  other `SFX` row, at −10.3, because that is the level Chad flew and approved on
  2026-08-17 when the raw file was in his tree. Mastering it to −18 would quietly
  drop the throttle voice 8 dB inside an approved mix. **Settled 2026-08-18: it
  stays.** The house target is a policy for assets nobody has judged yet; an
  approval outranks a policy. If the drone is ever called too loud, move
  `kEngineStreamTrim` in `render/mix_levels.h`, not the manifest row — that keeps
  the mastered asset comparable to the rest of the bank.

`linear` vs `dynamic` in the `dyn` column is a **hard** distinction. Linear is one
static gain and preserves the crest factor; it is mandatory for every bed, since a
dynamic pass pumps audibly on sustained material. `dynamic` is gated per-window
leveling and is for **voice** — it is broadcast speech leveling, and it buys
intelligibility under the wind bed.

## §4 The bug this pipeline is built around

ffmpeg's `loudnorm` engages linear mode only when **both** the gain fits under the
true-peak ceiling **and** `measured_lra <= LRA_TARGET`. Otherwise it falls back to
**dynamic silently**: no warning, no non-zero exit — and it hits the integrated
target *better* than linear would. A report that checks only LUFS and true peak
can therefore never detect it. At an earlier `LRA_TARGET` of 11 this
dynamic-processed **9 of 15 assets**, riding ~10 dB across a music bed.

The defence is not the number. It is the build **asserting that pass 2 actually
used the mode the manifest asked for**, and failing if it did not. That assertion
earned its keep on 2026-08-18: the bagpipe drone was first specified at its
measured −9.9, the build refused to produce it, printed that −10.3 was the
loudest target reachable linearly, and the row was corrected. A 0.4 dB difference
nobody can hear, instead of gain-riding a sustained drone.

## §5 Reproducibility

Same sources + same manifest ⇒ **byte-identical outputs**, verified across a
`--clean` rebuild, 16/16.

This was only made true on 2026-08-18. The Ogg muxer stamps each stream with a
**random** bitstream serial number, so the three `.ogg` assets differed by 840
bytes across page headers and CRCs on every rebuild while the payload and the
file size stayed identical. `-fflags +bitexact` fixes it, and also drops the
ffmpeg version string out of the Vorbis comment header so a toolchain upgrade
does not churn the lock either.

## §6 Why a fresh checkout is silent, and how to fix it

**The assets are not in git.** Chad's ruling, 2026-08-17: *"code and docs only if
its a one way decision."* 54 MB is permanent in history, and the audio lane
belongs to the human sound engineer.

Every load in `app/main.cpp` is guarded, so a checkout without assets is
**silent, not broken** — and each guard **prints one line** naming the missing
file. Silence you were never told about is the trap, not the silence.

To deploy:

```bash
bash tools/audio/build_soundbank.sh --sources D:/audio_tracks   # master
bash tools/audio/install_soundbank.sh D:/flight_sim2/seads-recon
bash tools/audio/install_soundbank.sh --check <tree>            # verify only
```

`assets/audio/MANIFEST.lock` **is** committed — sha256 and size of all 16 assets.
It is the only thing in git that says what a tree is *supposed* to have, and it
turns "silent" from a mystery into a diagnosis.

## §7 The gitignore trap

`.gitignore` used to read `assets/audio/*.wav`. A gitignore `*` **does not cross a
`/`**, so that rule was single-level and ate every loose `.wav` in
`assets/audio/`. `git add` on one exits **0 and stages nothing** — no error, no
warning. Combined with the guarded loaders, a file that never committed sounded
perfect on the machine that made it and was silent everywhere else.

That is exactly how `assets/audio/bagpipe_drone.wav` came to be loaded at startup
on 2026-07-24 and never once committed: the aircraft's throttle voice was missing
from every other worktree for three weeks, with nothing ever reported.

Fixed 2026-08-18. The bench rule is scoped to `assets/audio/bench_*.wav`; the bank
is ignored by three **exact folder** rules; the lock sits beside them where no
rule can reach it. `install_soundbank.sh` asserts all three invariants and fails
on the trap's return — verified by reintroducing the old rule and watching it
fire.

## §8 Where to look

| | |
|---|---|
| `tools/audio/soundbank.manifest.tsv` | what ships, and every per-asset ruling |
| `tools/audio/build_soundbank.sh` | the mastering pipeline |
| `tools/audio/install_soundbank.sh` | deploy + verify + write the lock |
| `assets/audio/MANIFEST.lock` | committed record of the 16 assets |
| `docs/soundscape_handoff.md` | the rung: what was built, flown and signed |
| `docs/audio_handoff.md` | the procedural-audio work that preceded it |
| `audio_ideas/wind_audio.md` | the speed-coupling doctrine |
| `D:\audio_tracks\build\soundbank_report.txt` | measured loudness of every asset |
