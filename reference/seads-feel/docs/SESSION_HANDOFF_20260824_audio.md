# SESSION HANDOFF — THE AUDIO LANE: the train that had no caller, and a country nobody could hear

**Launch line for the next agent:** *"Read `docs/SESSION_HANDOFF_20260824_audio.md`; §7 says what is open."*

**Everything below is on `main` and pushed — run `git log --oneline --grep=audio`
for where it actually sits.** No hash is named here on purpose; three lanes were
landing while this was written and a stale "main = <hash>" line has now been
corrected by three separate lanes. Do not reintroduce one.

**This file supersedes `docs/SESSION_HANDOFF_20260820_audio.md` for its §2 (the
dials) and closes its §4 (the `wired=yes` liars) by two thirds.** Everything else
in that file still stands — especially its §0, §5 and §9.

---

## 0. ⚠ WHERE TO WORK — read before you `cd` anywhere, and read it twice

**This lane's tree is `D:\seads_sandboxes\audio`** (branch `sandbox/audio`).

**`D:\flight_sim2\seads-recon` IS NOT THIS LANE'S TREE, AND ITS BRANCH MOVES
UNDER YOU.** The previous handoff already said the character lane had taken that
tree. What it could not say, because it had not happened yet, is the sharper
version:

> On 2026-08-24 `seads-recon` was on **`main`** when this session started and on
> **`sandbox/sw2-scarf`** a few hours later. Nobody announced the switch. Two
> audio commits were made in that window and landed on the character lane's
> branch instead of `main`.

It was caught, and the work was re-landed on `main` from this lane's own tree
(the scarf lane had touched **no** code files — only `assets/character/`,
`assets/sled/indy650.glb` and docs — so the two histories were cleanly
separable). The commits were **deliberately left in place** on
`sandbox/sw2-scarf` rather than reset off it: that lane holds uncommitted work in
that tree, and moving a branch pointer under a live agent to buy tidiness is a
bad trade. They are on `main` too, so their eventual merge is a no-op.

**THE RULE THIS BOUGHT:** run `git branch --show-current` **immediately before
every commit**, never once at the start of a session. A tree you checked an hour
ago is not a tree you know.

```
git worktree list                    # who is where, and on WHAT branch
git -C <tree> status --short         # someone else's uncommitted work?
git -C <tree> branch --show-current
```

**Other lanes' processes will also be holding your build outputs.** This session
could not link `build/seads_tests.exe` — another agent had two copies of it
running. Do **not** kill them. Link the suite under another name off the same
objects and run that:

```
cd build && c++ -g -static @CMakeFiles/seads_tests.rsp -o seads_tests_audio.exe
```

**A fresh tree has NO audio and is SILENT, by design** — see the 08-20 handoff §0
for `install_soundbank.sh` and the `.gitignore` trap. Unchanged.

---

## 1. WHAT LANDED

Chad, in one message: *"where is that train sound when flying over chelmsford? I
wanted a train sound in that vicinity, similar to the mechanism of the wildcat
hiss or the wolf howl, come to think I havent heard the wolf howl in seads-recon
play yet.... Can you check if those sounds are in there and the volume turned up
on those?"*

**Four faults sat behind that one question, and only the last was volume.** That
is the shape of the session and the reason this file is long.

| # | fault | fix |
|---|---|---|
| 1 | The train had **no caller** — for a week | app glue in `app/main.cpp` |
| 2 | …and wiring it as specified would still have been silent from the air | a per-context town RADIUS + `world::BuildingColliders::prisms_near_m` |
| 3 | The wolf was armed, but its bubble was a bullseye you had to hit | `kCryRangeFlyingM` 3200 → 5000 m |
| 4 | The volume was held down by **two assertions that encoded beliefs** | both corrected to measure what they claim |

Plus one carried debt: the `+3 dB` snowmachine raise from 2026-08-23 had never
reached `main` (it sat on `sandbox/audio` while `main` moved on) and so was not in
the binary Chad played. Merged.

### 1.1 The train had no caller

`render/town_ambience.h` was written 2026-08-17: density gate, four uneven gaps,
grace period, context-aware cadence — pure, and covered by nine unit tests.
`sfx/train_chelmsford_pass.wav` was built, mastered and installed the same day.
The manifest row said `wired=yes`.

**Nothing in `app/main.cpp` had ever included the header.** The asset was loaded
by nobody and played by nothing.

The gate cannot see this. `ctest` never runs `seads.exe`, so a channel with no
caller is indistinguishable from a channel that works. **This is the second
time** — `render/sled_synth.h` sat finished and uncalled from 08-17 to 08-20 — so
the lesson has been moved out of the comments of the rows that got caught and
into the `wired` column's own DEFINITION at the top of
`tools/audio/soundbank.manifest.tsv`:

> `wired   yes = A CALLER EXISTS` … *Before writing yes, grep `app/` for the header.*

### 1.2 …and it would STILL have been silent from the air

This is the more interesting half, and it is the part a reviewer should check
hardest.

`town_ambience.h` asked the app for "how many buildings nearby". The obvious
source is `world::BuildingColliders`, whose index cells are **~92 m** of ground
(1024×512 equirect on R = 15 km). Ask that question from an aircraft and the
answer is *yes, for about a second*: you cross 92 m at 110 m/s almost instantly,
so `near_town` flickers and the scheduler's clock — which only advances while you
are near a town — banks perhaps **5 s per overflight**, against a 40 s grace and
gaps of four to eight minutes. **Tens of passes over Chelmsford per train.**
Indistinguishable from broken, and that is exactly the report we got.

So "nearby" is now a **radius, per context**, and the radius is what makes the
clock run at the rate the gaps were written for:

* **GROUND 300 m** — you are in the streets; the buildings around you *are* the town.
* **AIR 4000 m** — a 65–90 s pass at 90–120 m/s: one real gap's worth of clock
  per overflight rather than a rounding error.

The flying gaps came down with it: `{255, 390, 310, 470}` → `{130, 190, 155,
225}`. Those old numbers multiplied a rarity the geometry already supplies —
**the air is rare by construction**, because the clock stops the moment you leave
town. They are still longer than the ground gaps, so riding in remains the
reliable way to hear it.

**New: `world::BuildingColliders::prisms_near_m(pos, radius_m)`.** O(cells
touched) local density, altitude-blind (direction only), with a **stamp buffer**
so a collider span-inserted into six cells counts **once** — without that, a wide
building reads as several and a farmstead is promoted to a town. Killer test in
`test_buildings.cpp`: brute-force parity at the u seam and both poles, plus a
one-building-is-one-building case and a degenerate-input case.

⚠ It is `mutable`-backed and therefore **not thread-safe**. It is a
single-threaded per-frame audio query and the header says so out loud.

### 1.3 The wolf was armed — you just had to nearly aim at it

The animals **were** wired on 08-20 and were in Chad's binary. They never fired
because `kCryRangeFlyingM` was 3200 m around a single point: the wolf sits over
Coniston, the cat over Onaping/Dowling, ~15 km and ~30 km from the Chelmsford
side he flies. A route that misses by 4 km hears nothing **and gives no hint that
anything was ever there.** → **5000 m** (~2.4× the area for the same flight path;
still a PLACE — 10 km of arming diameter against 42 km of pump separation — and
the overlap `static_assert` still holds with room).

### 1.4 The volume — and the two assertions holding it down

`test_pump_ambience.cpp` pinned `cry_gain(...) <= 1.0`, justified in its own
comment as *"SetSoundVolume, which has no headroom above unity"*. **That is false
about raylib.** `SetSoundVolume` forwards to `SetAudioBufferVolume`, which is
`buffer->volume = volume;` with no bound (`raudio.c`) — above 1 amplifies, it does
not saturate. The real ceiling is the asset's own true peak, and these assets are
swimming in it (`build/soundbank_report.txt`): wolf **−16.6 dBTP**, wildcat −6.6,
train −11.8 — all before `kGameplayBusGain` (0.63, −4 dB).

A second case compared the cry's **linear gain** against the music's. But the
cries master to −24 LUFS and the bed to −20, so at equal gain the animal is
already 4 LU down: the check was **4 dB stricter than the property it claimed to
enforce**, and that margin is precisely what Chad could not hear.

Both now measure what they assert — delivered peak in dBTP against clipping,
delivered loudness in LUFS against the bed.

| dial | was | now |
|---|---|---|
| `kCryGroundGain` | 0.92 | **1.30** (+3.0 dB) |
| `kCryFlyingGain` | 0.60 | **1.05** (+4.9 dB) |
| `kCryEdgeFloor` | 0.42 | **0.55** |
| `kTrainGroundGain` | 0.90 | **1.20** (+2.5 dB) |
| `kTrainFlyingGain` | 0.62 | **0.95** (+3.7 dB) |

Loudest path in the whole system is the cat at zero distance on the ground:
`1.30 × 0.89 × 0.63 = 0.73`, landing its −6.6 dBTP peak near **−9.4 dBTP**.
Nothing is near clipping, and that is checked rather than remembered.
`kCryGainCeiling` (1.35) exists as a **typo guard**, not a design limit.

---

## 2. THE MEASUREMENT THE GATE CANNOT MAKE — and it says the train will arm

`ctest` never loads the real bake, so "the train is wired" and "the train will
ever fire over Chelmsford" are different claims. The second was measured, against
`assets/sudbury_buildings.bin` (86 503 colliders), with a standalone probe:

```
place                              <=300m    <=4000m   GROUND    AIR
Chelmsford (St-Joseph church)         204       3799   TOWN      TOWN
St Charles snow mountain              141       3864   TOWN      TOWN
Coniston pump (wolf)                    0       2517   -         TOWN
Onaping/Dowling pump (cat)              0       1207   -         TOWN
 8 km off Chelmsford                    -        145   -         TOWN
15 km off Chelmsford                    -         17   -         -
25 km off Chelmsford                    -         18   -         -
```

Thresholds are 6 (ground) and 30 (air). **The air threshold sits in a wide gap** —
145 in the sprawl versus 17 out in the country — so it is not balanced on a knife
edge. Note the pumps also read as TOWN from the air: Coniston and Onaping *are*
settlements, so the train can sound there too. That is the design ("near a town"),
not a leak.

**Rebuild the probe when a dial moves.** It is deliberately not a CMake target,
same convention as `wind_synth_preview.cpp` / `sled_synth_preview.cpp`:

```
g++ -std=c++17 -O2 -I. -Ibuild/_deps/glm-src -include string \
    <probe>.cpp render/building_asset.cpp world/buildings.cpp world/heightfield.cpp -o probe.exe
probe.exe assets/sudbury_buildings.bin
```

It needs `render/building_asset.h`, `render/town_ambience.h`,
`render/sudbury_hero.gen.h` (`kSudburyHeroes[0]` is the Chelmsford church — the
only usable Chelmsford anchor in the tree; **there is no town list**) and
`world/faction_bubbles.h`.

---

## 3. THE DIALS — every unheard knob, and which one to move

The gate never runs `seads.exe`, so **none of this is proven by green**. One dial
at a time; Chad flies each.

| symptom | move THIS | where | now |
|---|---|---|---|
| **train** never heard from the air | `kTownRadiusFlyingM` — the clock only runs while you are over town | `render/town_ambience.h` | 4000 m |
| train arms over farmsteads | `kTownPrismCountFlying` / `kTownPrismCount` | `render/town_ambience.h` | 30 / 6 |
| train too often / too rare | `kTrainGapsFlying[]` / `kTrainGapsGround[]` | `render/town_ambience.h` | 130–225 s / 105–198 s |
| train too loud / quiet | `kTrainGroundGain` / `kTrainFlyingGain` | `render/town_ambience.h` | **3.40 / 2.90** |
| **animals** never heard at all | `kCryRangeFlyingM` — the bubble you must fly into | `render/pump_ambience.h` | 5000 m |
| **the music buries everything** | `kMusicFlownSurface` — the bed itself | `render/mix_levels.h` | **0.67** (was 0.95) |
| the music is now buried by WIND | `kWindEngineTrim` — see §9 | `render/mix_levels.h` | 0.70 |
| **a loud MOMENT distorts** (not one sound) | `kGameplayBusGain` — lowers everything together, preserving every ruled ratio | `render/mix_levels.h` | 0.63 |
| animals too loud / quiet | `kCryGroundGain` / `kCryFlyingGain` | `render/pump_ambience.h` | **3.50 / 3.10** |
| animals too faint on arrival | `kCryEdgeFloor` — the rim is where you MEET them | `render/pump_ambience.h` | 0.55 |
| animals too often while parked | `kCryGapsGround[]` | `render/pump_ambience.h` | 41–78 s |
| the cat sits wrong against the wolf | `kWildcatCryGain` — ⚠ now a PEAK limit, not taste; raising it clips | `render/pump_ambience.h` | **0.68** |
| **sled** too loud / quiet | `kSledFlownLevel` | `render/mix_levels.h` | **0.98** |
| sled still too high in pitch | `kSledVoiceTranspose` | `render/sled_audio.h` | 0.50 |
| sled LAYERS wrong against each other | `kSledMaster` | `render/sled_audio.h` | 0.78 |

**Do not move `kSledSourceCorrection` (0.50)** for an ear ruling — it is a
measurement of how wrong the two recordings are, kept separate from
`kSledVoiceTranspose` so "which half of this is taste?" has an answer.

**Above unity is now legal in this lane, and it is measured, not slipped.** Any
new trim that goes above 1.0 must carry its asset's true peak from
`build/soundbank_report.txt` in the comment, the way these do. `kCryGainCeiling`
is the guard.

---

## 4. THE GATE — judge it by WHICH tests fail, never by the total

Total at time of writing: **1535 cases, 1530 passed, 5 failed** (24 063 229
assertions). **All 5 are pre-existing and none are audio.** That was *proved*, not
assumed: both failing files were rebuilt from a clean detached worktree at the
commit before this session's work, with no lane's dirty files present, and they
fail identically there.

Four are exactly the known GI4 sled debts the 08-20 handoff already named:

* `sled_slides_before_it_tips_on_flat_snow`
* `sled_grip_ceiling_stays_below_the_tip_threshold`
* `sled_assist_reference_plane_is_load_weighted`
* `sled_debug_sink_is_write_only`

### 🔎 The fifth is NEW and belongs to the enemy-AI lane

* **`probe P-F: the relentless raider keeps the pump and shoots back`**
  (`test/unit/test_enemy_ai_e6.cpp:798`)

It is **not** on the known-debt list, it is not audio, and it pre-dates this
session's work. Per this lane's own rule — *any new failure NAME is a finding, not
a number to accept* — it is written down here rather than absorbed into a count.
Someone should own it.

**Audio-side green**, run individually against these exact sources:
`test_pump_ambience` (29 cases), `test_sled_audio` (44), `test_buildings` (4,
including the new parity sweep). `seads.exe` builds and links in both `build`
(Debug) and `build-play` (RelWithDebInfo).

---

## 5. WHAT CHAD CAN ACTUALLY HEAR RIGHT NOW

`D:\flight_sim2\seads-recon\build-play\seads.exe` was rebuilt 2026-08-24 with all
of it. ⚠ **That tree is the character lane's and was on `sandbox/sw2-scarf`**, so
that binary also contains their uncommitted `indy650.glb` and
`render/sled_model.cpp`. That is fine for listening; it is not a reference build.

For a clean audio-only fly, build from this lane's tree (`D:\seads_sandboxes\audio`,
which now contains everything) — and remember a fresh tree needs
`install_soundbank.sh` first or every channel is silently absent.

**Never judge audio from a `build/` Debug run.**

---

## 6. THE LESSON THIS SESSION PRODUCED — it is the same one, a third time

Two of the four faults were **assertions that encoded a belief rather than a
measurement**:

* *"no headroom above unity"* — wrong about the library, never checked against
  `raudio.c`.
* a loudness comparison performed in **linear gain** — wrong about the units,
  ignoring a 4 LU mastering difference that was written down one file away.

Both were green. Both were confidently commented. Between them they held the
animals 4–5 dB below where the ear wanted them, and no amount of running the
suite would ever have said so.

`render/mix_levels.h` already carries this lesson from the retracted sled-vs-gun
`static_assert` ("an assertion is for a property that must hold for the code to be
correct, not for a balance nobody has heard yet"). It has now cost this lane three
times.

> **A test that pins a number you have not measured is a guess with a build behind
> it.** In this lane, that guess is usually holding something inaudible.

And its twin, from §1.1: **a green gate cannot tell you a sound is playing.** It
can only tell you the arithmetic around a sound that may not be.

---

## 7. WHAT IS OPEN

**Ranked. Item 1 is closed as of the last fly — read it anyway, because what it
does NOT cover is the top of the real list.**

1. ~~**NOTHING HERE HAS BEEN HEARD.**~~ ⭐ **CLOSED — SIGNED 2026-08-24.**
   Chad flew it three times in one evening and signed the third:
   *"good the sounds are right now"*.

   * Fly 2 — he heard the **train** for the first time, and the **cat**: the
     first report of either animal since they shipped. Ruled: bed down, events up.
   * Fly 3 — *"they barely make it over the music … not very immersive if not
     quite a bit louder than the music"*. Ruled: **the world belongs OVER the
     bed**, which retracted two assertions (§6).
   * Fly 4 — **SIGNED.**

   **THE MIX IS NOW AN APPROVAL, NOT A DERIVATION.** Every level in §3 is one he
   has heard. Do not retune them to make a policy tidier — move one only when he
   says to, or when a NEW channel arrives and changes what the mix is (which is
   how ruling 1's `0.95` was legitimately undone). The rulings list at the top of
   `render/mix_levels.h` carries the long form.

   ⚠ **Still unheard, and NOT covered by the signature:** the **wolf** (he never
   tested it — *"I'd imagine it suffers a similar problem"*, so it was moved with
   the cat on inference), the second engine **octave**, and the train's
   **cadence** (how often, as opposed to how loud — one evening is not long
   enough to judge a 130–225 s gap).
2. **Two `wired=yes` liars remain**, re-verified today — neither has a caller
   anywhere in `app/` or `render/`:
   * `loops/player_breathing_loop.wav` — no reference anywhere
   * `sfx/intro_signal_rumble.wav` — no reference anywhere
3. **A test that keeps the `wired` column honest.** The 08-20 handoff proposed it
   and it was not built; had it existed, fault §1.1 *and* the sled-synth
   equivalent would both have been caught automatically. It is a small test: for
   each row with `wired=yes`, assert that some source file under `app/` or
   `render/` names the asset. **This is the highest-leverage item in this list
   after the fly**, because it converts a recurring class of silent bug into a
   red.
4. **The train's cadence is unflown.** 130–225 s of accumulated over-town time is a
   *reasoned* number, not a heard one. If Chad says "too often" the gaps move; if
   he says "never" the radius moves. They are separate dials on purpose.
5. **`sim::SledState::engine_rpm`**, still deliberately unused — feeding it adds
   real CVT load coupling and is a contract change to `SledDrive`. Its own pass,
   its own fly.
6. **Footsteps** (`sfx/step_*.wav`), `wired=no` and correctly so: *"my sudburian
   cannot walk yet so defer for now the walking sounds"*.
7. **The explosion while riding** — his spec asks for it outside tunnels too; the
   old "no snowmachine in this tree" blocker is stale.
8. **`docs/soundscape_handoff.md` has stale rows** — its §5 describes the
   building-density lookup as still needed. It is built. The rest was not audited.

---

## 8. FILES THIS LANE TOUCHED THIS SESSION

| file | what |
|---|---|
| `app/main.cpp` | the train's glue: load, per-frame trigger under the cave gate, unload; `town_colliders` fetched outside the `[buildings] collide` toggle |
| `render/town_ambience.h` | per-context radius + threshold, re-cut flying gaps, raised gains |
| `render/pump_ambience.h` | wider air range, raised gains, raised edge floor, `kCryGainCeiling` |
| `render/mix_levels.h` | `kSledFlownLevel` 0.44 (carried from 08-23) |
| `world/buildings.h` / `.cpp` | `prisms_near_m` + its stamp buffer |
| `test/unit/test_buildings.cpp` | brute-force parity sweep, de-dup case, degenerate case |
| `test/unit/test_pump_ambience.cpp` | the two corrected assertions; relaxed the fly-over count, with the anti-chorus property checked directly instead |
| `test/unit/test_sled_audio.cpp` | corrected train ceiling; new radius/threshold cases |
| `tools/audio/soundbank.manifest.tsv` | the `wired` column definition, and the train row's confession |
| `docs/audio_handoff.md` | session 5 log |
| `docs/SESSION_HANDOFF_20260820_audio.md` | dial table updated in place |

**The firewall is unchanged** — `town_ambience.h` and `pump_ambience.h` remain
pure (`<algorithm>` / `<cmath>` only; no raylib, no `<random>`, no wall clock),
and all policy stayed in them. `world/buildings.h` gained one query and no
dependency. See the 08-20 handoff §9 for why that matters.
