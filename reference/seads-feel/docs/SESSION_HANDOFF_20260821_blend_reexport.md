# SESSION HANDOFF — THE .BLEND RE-EXPORT (retire the three post-passes)

**Launch line:** *"Read `docs/SESSION_HANDOFF_20260821_blend_reexport.md`; do §4 and §5."*

**Worktree:** `D:\seads_sandboxes\winter-gi`, branch `sandbox/gi4-ride`, HEAD `2e81c0aa1`.
**Status:** the weight-shift program is DRIVEN AND SIGNED by Chad 2026-08-21
("everything is good on the snowmachine drive test"). This rung is cleanup, not
feature work. **Chad is AT the Blender session for this one — it cannot be done
without him.**

---

## 1. WHY THIS RUNG EXISTS

Three defects were confirmed in the shipping rider and are now CORRECTED IN THE
GLB BYTES (`2e81c0aa1`) by running main's post-pass scripts:

| defect | was | now (in the GLB) |
|---|---|---|
| helmet family authored 0.34 m in front of the `head` joint | 340 mm off | 14 mm — encloses the head |
| `head_balaclava` — the 12-tri authoring BLOCK — still attached | attached | mesh detached |
| suit prim wound inward → backface-culled → see-through rider | −0.01176 m³ | +0.01176 m³ |

**But the .blend still has all three.** The scarf lane measured the live session
read-only and found the same numbers there (helmet gap 0.3405 m, suit volume
−0.01176), which proves **the splice and the exporter are FAITHFUL** — they were
never the problem, and "fix it in the splice" was explicitly retracted.

So today the correct bytes exist only because two post-passes are re-applied on
top. That is the debt this rung clears. **The goal is not to change how the
rider looks — he is already signed off. The goal is to make the SOURCE produce
what is already shipping, so the post-passes can be deleted.**

---

## 2. THE LAWS (read `docs/RIDER_AUTHORITY.md` before touching anything)

1. **Blender is NEVER headless on this file. ONE GUI session. The running
   session is the truth.** `export_live_v13.py:8` enforces `assert not
   bpy.app.background`. A save from your MCP call **IS** a save of Chad's
   session — do not save without saying so, and **do not kill his session**.
2. **The rider discriminator is NOT 19** (amended `64960b1ed`): the legacy rig
   is frozen at 19 forever; the Sudburian's count grows every rung (42 here, 44
   on main after the scarf rung). Corroborate with `scarf_s01`/`scarf_s02`.
   ⚠ `export_live_v13.py:10` and `apply_helmet_v13.py:29` both hard-assert
   `== 42`. In THIS worktree that is correct. If the scarf ever lands here,
   those two asserts must move to the NOT-19 form or the export dies.
3. **MEASURE, NEVER RETYPE.** Every number in §4 is a gate to hit, not a
   constant to paste. Derive the moves in the live session the way the scripts
   do — from the artefact.
4. **Never overwrite Chad's files.** Snapshot before you write (see §9).

---

## 3. WHAT THE PIPELINE ACTUALLY IS

**This is NOT a full re-export.** `indy650.blend` **can no longer reproduce**
`indy650.glb` — three machine nodes are gone from the .blend and L5/L6 shroud
work is unsigned in it (`splice_sudburian.py:18-20`). The machine's bytes are
carried across UNTOUCHED and verified. The real pipeline:

```
LIVE GUI SESSION (Chad's, open, never headless)
  └─ export_live_v13.py      selection-only export of rig + proxy + FAMILY
     │                        -> scratch/sudburian_seated.glb + sudburian_ref.json
     │                        :28 export_rest_position_armature=False
     │                        (the POSED rig IS the exported rest pose — this is
     │                         intentional and load-bearing; do not "fix" it)
     └─ splice_sudburian.py <seated.glb>
            swaps the rider into assets/sled/indy650.glb, removes LEGACY_ROOTS,
            verifies 132 machine nodes BYTE-IDENTICAL and rest==live 0.000000 m
```

`splice_sudburian.py` is **idempotent** — the six helmet nodes are in
`LEGACY_ROOTS` (`:41-48`) precisely because a second splice once silently
DUPLICATED them (138 vs 132 carried nodes was the tell). It has a `--check`
mode; use it.

---

## 4. THE THREE SOURCE FIXES (in the live session, Chad present)

### 4a. The suit winding — ★ FLIP IT, DO NOT REGENERATE ★

The proxy mesh in the .blend is wound inward (signed volume −0.01176 m³).

**RULING FOR THIS RUNG: flip the existing mesh in place.** In the live session,
select `sudburian_proxy`, Edit Mode → select all → **Mesh ▸ Normals ▸ Recalculate
Outside**, then verify the signed volume is POSITIVE before leaving Edit Mode.

**Do NOT regenerate the proxy from `sudburian_proxy.py`.** Regeneration would
rebuild the character and put the SIGNED R2c art at risk — the leather mitts,
the SS4 costume, the hand/grip tuning Chad accepted. It would also move vertex
POSITIONS, which converts a zero-re-bake change into a full re-bake of the seat
/deck/boot profile and the rig rest tables. Not worth it, and not needed:

> The generator guard already landed (`8340b4d87`). It measures every piece and
> flips only what is actually inward, and it asserts the assembled proxy encloses
> positive volume. That stops the **next** inverted part. It does not need to be
> run over the existing one — the flip does that, without touching positions.
>
> Note from that work, worth keeping: `box_volume` and `oriented_box_volume`
> were inward (−8.000 on a 2 m cube) but **`prism_volume` was already correct
> (+5.657)**. A blanket "reverse the windings" would have inverted the prisms and
> shipped a NEW defect inside the fixing commit — and it would have gated green,
> because nothing measures prism orientation.

### 4b. The helmet family — move it onto the head

The five helmet meshes (`helmet_sudburian`, `helmet_dent_1..4`) are 100 %
skin-weighted to the `head` joint (48 780.0 weight on `head`, zero elsewhere),
so a rigid move is exact in every pose.

**Derive the delta in-session, do not type it.** The move that main's script
derives, and that this lane reproduced independently, is — in the glTF frame —
`(0.0000, +0.0791, −0.3261) m`. Two lanes, two derivations, identical number, so
treat it as the **gate**, not the input. The derivation itself: the BLOCK is the
stand-in for the head, so the delta that lands the block on the real head is the
delta that lands the helmet on the real head — centre-align x and z, **top**-align
y (the balaclava skirts lower than the head box, so aligning tops preserves the
crown clearance the helmet was authored with). The "real head" = the proxy
vertices weighted to the `head` joint.

⚠ Blender is Z-up and the glTF frame is Y-up — **convert by measuring, not by
assuming a sign.** Verify against §6's numbers after the splice.

### 4c. The block — stop exporting it

`head_balaclava` is the head stand-in the helmet was modelled around, not
shipping geometry. Two acceptable fixes:
- **preferred:** remove `"head_balaclava"` from `FAMILY` in
  `export_live_v13.py:11-12` (keeps the object in the .blend as the fitting aid
  it is), **or**
- delete the object in the session if Chad rules it has no further use.

Leave it in `splice_sudburian.py`'s `LEGACY_ROOTS` either way — that entry is
what keeps a RE-splice idempotent, and it costs nothing when the node is absent.

---

## 5. THE SOURCE FIXES THAT NEED NO BLENDER (do these too, same commit)

These are why the offset survived review, and they are pure Python:

1. **`helmet_v13/gen_helmet_v13.py:47`**
   `Z_ORIG = (0.0, 0.2802, 1.2152)   # head bone (rest), world m`
   **The comment says *rest*. The value is the *POSED* head position.** Same
   value in `helmet_spec_v11.py:36` as `ORIGIN_W`. Correct the value to the REST
   bone, or — better — stop hard-coding it and derive it at generation time.
2. **`helmet_v13/apply_helmet_v13.py:31`**
   ```python
   hw = rig.matrix_world @ rig.pose.bones["head"].head
   assert (hw - Vector(S.ORIGIN_W)).length < 0.001, ...
   ```
   The preflight validates the **posed** bone against the authoring origin — so
   it agrees with the constant precisely when the constant is wrong. **A check
   that reads the same wrong thing the code does will always pass.** Point it at
   the REST bone (`rig.data.bones["head"].head_local`) and it becomes a real gate.
   Same pattern at `spec_v11/gen_helmet_v11.py:27`.

---

## 6. VERIFICATION — the numbers to hit

After the export + splice, measure the shipping GLB read-only (crib the reader
from `measure_seat_profile.py`):

| check | required |
|---|---|
| suit prim signed volume | **POSITIVE**, ≈ +0.01176 m³ |
| helmet family bbox centre z vs `head` joint bind z | within **~15 mm** (was 340 mm). Reference: centre ≈ −0.5704 vs head −0.5848 |
| `head_balaclava` | absent, or present with **no mesh** |
| splice self-verify | **132 machine nodes byte-identical**, rest == live **0.000000 m** |
| rig | bone count **NOT 19**; the 21 driven joints resolve by NAME |
| rider suite | `ctest --test-dir build -C Debug -R rider` → **11/11** |
| full gate | **1269/1273**, and the ONLY failures are the four known GI4 sled debts: `sled_slides_before_it_tips_on_flat_snow`, `sled_grip_ceiling_stays_below_the_tip_threshold`, `sled_assist_reference_plane_is_load_weighted`, `sled_debug_sink_is_write_only`. **Any new failure NAME is a finding, not a number to accept.** |

Run ctest **serially** (`-j1`) and never two gates on one build dir — a
concurrent `build-play` compile has contaminated a run in this repo before.

**Then judge it with your eyes**, not only the numbers — this is the lane where
a displaced helmet plus a grey block read as "a head" through many screenshots.
Zoom in on the neck.

---

## 7. THE RETIREMENT (the actual point of the rung)

Both post-passes **self-disarm by construction**:
- `patch_scarf.fix_suit_winding()` no-ops once the volume is positive;
- `fix_helmet_placement.py` no-ops when the block is absent and the helmet
  family is present (the scarf lane shipped that softening as `0d6d69340`
  specifically so a clean source export would not fail the pipeline — it still
  fails loudly when block AND helmet are both missing, which means "wrong GLB").

**Confirm both are inert by running them and seeing them do nothing**, then
**DELETE them** — in ONE lane's commit, not two. Leaving them is worse than
deleting them: a disarmed post-pass with no live purpose is a mystery patch that
the next agent will be afraid to touch.

Files to remove at that point: `assets/character/sudburian_src/fix_helmet_placement.py`
(and the `fix_suit_winding` path in main's `patch_scarf.py` — coordinate with the
scarf lane; they offered to retire theirs).

---

## 8. TRAPS

- **The colour-space trap.** glTF says `baseColorFactor` is LINEAR; this engine
  has NO linear pipeline and all materials are authored in raylib byte space. An
  sRGB→linear step once shipped the mitts brown and the suit near-black. Do not
  add one.
- **Verify the ARTEFACT, not the process.** The mitts once shipped with the left
  hand carrying the right hand's winding while every generator number was
  perfect. The suit shipped inside-out the same way. Measure the bytes.
- **Names, never indices.** A splice verify once caught a stale-index-after-
  compaction bug. `head_balaclava` is detached rather than removed for exactly
  this reason — every downstream node index stays valid.
- **`export_rest_position_armature=False` is deliberate** — the posed rig IS the
  exported rest pose (`skinned rest == live posed, 0.000000 m`). Do not "correct"
  it; the helmet bug is NOT caused by it.
- **Both hard `== 42` asserts** (§2.2) will fire if the bone count ever changes.

---

## 9. ROLLBACK

Everything is committed and nothing is pushed. Before the session:
- snapshot the .blend (there is a `blend_snapshots/` convention in
  `assets/character/sudburian_src/`), and
- note the current GLB blob hash: `git rev-parse HEAD:assets/sled/indy650.glb`.

If the export goes wrong: `git checkout -- assets/sled/indy650.glb` restores the
signed, driven bytes exactly. The .blend is restored from the snapshot. **The
shipping asset today is good and signed — a failed re-export must cost nothing.**

---

## 10. NOT IN THIS RUNG

- **The winter-gi → main reconcile.** main has moved a long way (E1–E7 enemy AI,
  the scarf rung; gate 1484/1488 there), winter-gi is 9 commits ahead and
  unpushed, and **both lanes have edited `assets/sled/indy650.glb`** — so that
  merge will conflict on exactly the contested asset. Fresh session, deliberate
  pass, and FETCH FIRST. Do not fold it into this rung.
- **R4-SIDE, the `P`-key side-hang** (`docs/SUDBURIAN_LADDER.md`) — kneel is
  reserved for it and its runtime gain stays 0.
- The deferred `OPEN-R3WS-STANDKNEE`, `OPEN-R3WS-STEERREACH`,
  `OPEN-R3WS-STANDSLEW`, `OPEN-KWS1-SAG`.
