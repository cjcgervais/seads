# SESSION HANDOFF 2026-09-10 — road-repair lane: LANDED, next = the Onaping road-edge pass

**Launch line for the next agent:** "Read docs/SESSION_HANDOFF_20260910_road_repair.md; do §1."

Worktree `D:\seads_sandboxes\road-repair`, branch `sandbox/road-repair` (== main `de523afd1` at this
handoff). Never work in `D:\flight_sim2\seads-recon` — that is Chad's fly tree; the sentinel session
(`flight-sim2-ae`) merges main into it and rebuilds `build-play`. You never touch it.

Read `docs/road_repair/CENSUS_FINAL.md` before touching a road. It is the lane's closing record:
the fly checklist (§1), one before/after table per rung (§2), the completeness critic (§3, eleven
items with file:line and a proposed rung each), and the gate verdict (§4). The four per-rung reports
under `docs/road_repair/` are the record of their own rounds; cite them, do not re-derive them.

## §1 What to do first

1. `git fetch origin`; confirm `origin/main` is at or past `de523afd1`. If it moved, read what
   landed (`git log --oneline de523afd1..origin/main`) before you branch anything.
2. Chad's verdict on this lane was **"flew it, all good, push to main and seads recon"** (this
   session, 2026-09-10). The lane is landed. **Do not reopen any of the five rungs unprompted.**
3. The next rung is **§3 — the Onaping road-edge pass**. It is his priority defect, in his words
   (2026-09-08): *"the roads are bad there and some drop offs coming off the road you dont see."*
   Build it on this lane branch, off main, never on main.
4. Before landing anything: §5 (the landing checklist) verbatim. It cost the previous lanes two
   bad merges and one killed gate to learn.

## §2 What landed (five rungs, one instrument)

| rung | commit | what it is | kill / arm |
|---|---|---|---|
| census probe | `a2292a72d` | `render/road_census.*`, `SEADS_ROAD_CENSUS=<tsv>`, stride `SEADS_ROAD_CENSUS_STRIDE`; the ruler before the cut | — |
| sink fix | `3297928a5` + cost fix `be8a30b5f` | drawn == driven on road decks: the driven radius is floored to the drawn deck (`apply_deck_floor`); the fold-corner memo keeps it at ONE corridor lookup per sample | `SEADS_DECK_FLOOR=0` |
| bank strips | `3b0dda7d9` | taper caps instead of junction walls, 3-ring smoothstep skirt, 12-knot section, per-vertex normals, sparkle shared with the planet via `render/snow_light_glsl.*` | `SEADS_BANK_LIT=0`, `SEADS_NO_BANKS=1`, `[bank_mesh] chord_tol_m=0`, `skirt_rings=1` |
| corner blend | `ebc47c1e3` | `LineNetwork::nearest` blends half_w / foot / junction_m over per-run candidates within a band; junction_m averaged as a DISTANCE, never from the averaged foot | `[snowpack] corner_blend_m` 2.0 → 0.0; `[bank_mesh] junction_station_m` 4.0 → 0.0 |
| kind-weight fold | `f0104a3ab` | red-team P1: `LineHit::plowed_w` blends the deck-lift extent, inside depth and bank amplitude, so a road↔trail tie no longer steps 0.45 m; `kind` stays the argmin for discrete read-outs only | same dials as the corner blend |

Then `848312f2d` (CENSUS_FINAL.md + `census_final_planet.tsv`), `de523afd1` (LANES stamp).
Merge of main `5ce9564b4` is `bf94bb8c2`; `13ad2ce8c` made the census writer binary (`"wb"`).

**Headline numbers** (full population, 166,053 drawn-ribbon stations; CENSUS_FINAL.md §2):
sink 54,611 (32.9 %) → 0; crest over-bound (local 0.5 m gradient vs `bank_max_grade` 1.20)
121 → 30, worst plowed grade 4.650 → 1.820; bank end-wall p50 1.138 → 0.040 m; within 2.5 km of
either pump, zero over-bound samples on plowed roads.

**Gate** (whole suite, detached, on `f0104a3ab`): `6 failed of 2012`, red set EXACTLY the baseline
six by name (79, 119, sled legs 1229/1230/1268/1271). `848312f2d` and `de523afd1` are docs/registry
only on top.

**The stick ruling, do not touch it:** `[snowpack] class_blend_m` ships **0.0**. It is live-tunable
from `config/world.toml`, the lane measured 1.0 as better, and it is **Chad's A/B in the seat**, not
a lane's call. Ask him once whether he drove both; do not set it for him.

## §3 NEXT RUNG — the Onaping road-edge pass

**The complaint, verbatim (2026-09-08):** the Valley pump at Onaping has a long pond just north of
it, it is fun to drive up the hill, and *"the roads are bad there and some drop offs coming off the
road you dont see."*

**What the census already knows about that spot** (do not re-survey; query the TSV):

- `docs/road_repair/census_final_planet.tsv`, rows within 2500 m of `PUMP valley`. The over-bound
  crest samples on plowed roads there are **zero** after this lane, so the drop-offs are **not** a
  corner step and **not** the deck sink. The suspects, in order:
  1. **The skirt crease / the outer bank fall.** The strip dives to `facet − skirt_bury_m` across
     `skirt_m`; on a side-slope the drawn strip meets a terrain facet that is itself falling away, so
     the eye sees a smooth bank and the machine leaves it onto a facet 0.5–1.5 m lower.
     Instrument: extend the census's DRAWN-BANK CONTINUITY section with the **terrain drop across
     the skirt** (`facet(outer skirt ring) − facet(bank foot)`), per station, and rank Valley rows.
  2. **The drawn-vs-driven float 20–40 m off the deck.** Past `bank_rise+bank_fall` (9 m) plus the
     skirt (6 m) the floor fades and the driven surface returns to the unfolded facet while the eye
     still sees the folded planet mesh. The sink fix bounded its own change set at exactly 9.000 m
     (census_after_sink.md §4). The float beyond that is the fold residual the mask cannot
     remove at 59 m cells — **that is a drop you cannot see**, by construction. Instrument: sample
     `drawn_planet_r − drive_r` at laterals 2×, 3×, 4× `half_w` and rank by magnitude near Valley.
  3. **The 5 m TrailMain floats** (critic #6) — two of the worst ten are 1.87 km from the Valley
     pump. Diagnose the trail bake's `half_w` (15 stations sit near zero), not the floor.

**The smoke rig for him:** `SEADS_SLED_RIG_SMOKE` + `SEADS_SMOKE_SPAWN_DIR` (a `kind 0/1` row's
`dir_x,dir_y,dir_z` from the TSV) + `SEADS_SPAWN_ALT=25 SEADS_SLEDCAM="2.2,35,12"`; the Onaping
block is CENSUS_FINAL.md §1 rung 1. Add a named env (`SEADS_ONAPING_SMOKE` or a documented spawn
line in the doc) so his ride there is one paste.

**The speed lever he asked for** ("more places with a bit less snow makes the sled go faster around
there"): groomed TRAIL segments (0.12 m) between the pump and the pond, and baked wind-SCOUR patches
on the convex hill. **NEVER `base_m`** — 0.77 m is a signed fixed input (WINTER LAW). Pumps stay
**OFF-ROAD by design**; never connect one to the road net.

**Rung shape:** instrument first (one census section, numbers at Valley before any cut), one
bounded geometry fix, its own doc `docs/road_repair/onaping_edge.md`, the identity dial, the full
gate on the merged tip, a red-team before landing. Same ladder as the five above.

## §4 Debts and the critic (CENSUS_FINAL.md §3 has the full table)

Owned by this lane, in priority order after §3:

- **#10 the +72 % bank vertices (8.02 M)** are flown-and-signed but never TIMED on his box.
  `[bank_mesh] junction_station_m = 0.0` is the lever; ask him whether startup got slower.
- **#9 `world/props.cpp:141-142`** tree scatter still calls `nearest()` unblended. Pass the dial;
  test = no trunk in the bush moves.
- **#11 non-adjacent same-run ties** (hairpin, switchback, cul-de-sac) keep a hard argmin. Dedup
  by (run, station distance) only if a plowed hairpin tighter than the 2 m band is found.
- **#7 the 9.05 m off-corridor `|Δdrive_r|` step** — top 10 TSV rows by `d_q_half_w_m`; almost
  certainly terrain; say so and close it.

Not this lane's, named so nobody re-discovers them: **#1 the pump mast on bare DEM**
(`app/main.cpp:4181-4187`, `radius_at` not `drawn_radius_at`, one call site — the pump/game-loop
lane); **#2/#3 pump apron + trample disks** (gait / game-loop); **#4 bank mesh ignores `deform_at`**
(snow lane; measure vertex churn first); **#8 the four red sled legs** (next sled lane).

## §5 Landing checklist (the SOP as it stood on 2026-09-10; it worked)

1. Merge `origin/main` **ONCE**, at or past its current tip, as a real union merge. **Never
   take-ours / take-theirs.** `generated/graph` conflicts are resolved by regenerating
   (`python tools/graph/graphify.py`), never by splicing.
2. `LANES.toml`: the `[lanes.road-repair]` block exists; keep every non-owned file **announced in the
   `#` comment block ABOVE the keys** (SOP-5). The snow lane's `render/bank_mesh.*` and
   `world/snowpack.*` are announced, **never** added to `owns`. `python tools/gate/lanes_lint.py
   LANES.toml` exit 0. Announces are comments — audit them from the RAW text, not `tomllib`.
3. `git ls-files --eol | grep -c "w/crlf"` == 0. Any new file writer opens `"wb"`, not `"w"`
   (the census writer bit us; the `SNOWDUMP` writer in `app/main.cpp` still has the bug and is
   not ours).
4. `python tools/graph/graphify.py --check-only` → `layer check OK`; regenerate in the SAME commit
   as any include change.
5. Full gate **DETACHED** on the merged tip (the harness memory guard kills background ctest
   children; PowerShell `Start-Process ctest ... -RedirectStandardOutput build/.gate_ctest.log`,
   poll the log, ~56 min): verdict quoted VERBATIM from `python tools/gate/gate_baseline.py check
   build/.gate_ctest.log`. `ctest -N` on main is now **2012**. A pre-merge gate is a pre-check, not
   a verdict.
6. Message the sentinel (`flight-sim2-ae` on this machine, via `ListAgents`/`SendMessage`) with the
   tip SHA and the verdict; it confirms the runway, audits the push, and takes the seads-recon
   merge + rebuild. Nothing pushes without Chad's word.
7. Push = fast-forward `HEAD:main` if main is quiet; stamp `LANES.toml` LANDED with his words.

## §6 Traps found this lane (each cost real time)

- **`app/main.cpp` holds a NUL byte** (~line 6345, inside a char literal, pre-existing on main), so
  `grep` calls the file binary and answers `0` or `Binary file matches`. **Byte-read it in Python**
  or the check is not evidence. The merge was nearly mis-called by this.
- **Inline python heredocs get mangled on this box** (backslashes). Write the script to a file
  under `build/.scratch/` or the session scratchpad and run the file. A failed python does NOT
  stop a `;` chain — check exit codes.
- **A test that compares a function to itself is a tautology.** The corner blend's identity leg
  compared `nearest(d,90)` to `nearest(d,90,0.0)` — the same default argument. It now compares to a
  hand-built brute-force argmin with `==`. Grade code against an independent ruler, never a copy.
- **"The junction gap covers it" was an argument, not a measurement**, and it was false twice
  (the deck-lift extent is not faded by the gap at all; shallow merges tie 230 m from any junction
  node). When a builder names its own unmeasured claim, red-team that first.
- **A step COUNT at 16 m station spacing RISES when a step is resolved into a ramp.** The corner
  rung's stated success metric went the wrong way (43,391 → 50,568) while the magnitude fell; the
  0.5 m local-gradient ruler (`crest_grade` column) is the one that can see C0. Pick the ruler
  before the cut.
- **The usage limit kills a running gate silently.** The sink fix sat un-verdicted for a day. If
  the session is near its limit, do not start a full gate; write the handoff instead.

## §7 Rulings Chad has made on this lane (do not re-ask)

- Pumps are OFF-ROAD by design; the challenge is finding a lake, stream or road to land on.
- Onaping drop-offs are the priority defect.
- Less snow = groomed trail segments + wind-scour; never `base_m`.
- `class_blend_m` is his stick A/B; ships 0.0.
- The five rungs are flown and signed ("flew it, all good").

Owed by him: whether he drove `class_blend_m` at 1.0; whether startup got slower with the heavier
bank mesh.
