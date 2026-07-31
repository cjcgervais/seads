# HANDOFF → kernel-docs agent: S-keyprec / SEALED KERNEL v8

Source of truth: `seads-feel` @ `feel/kernel-v5`, **tag `flight-kernel-v8-2026-07-29` → `ae7ae8f23`**.
Gate 387/387, zero moved goldens.

⚠ **CORRECTED 2026-07-29** (docs agent's catch): an earlier revision of this file named `49e5f93da` as
the seal. That is the MECHANISM commit only. The tag resolves to `ae7ae8f23`, which adds the docs
commit (`39b80672a`) and the red-team review-bar commit on top. Cite `ae7ae8f23`. The three commits:
`49e5f93da` mechanism → `39b80672a` docs → `ae7ae8f23` review bar (= the seal).

⚠ **v8 IS THE FIRST KERNEL SEAL MADE BEFORE CHAD FLEW IT.** v5, v6 and v7 each carried his stick
verdict in the tag message; **this one does not** — the tag says AWAITING CHAD'S FLY, and that is
exactly what it means. Do not read v8 as carrying a flown approval. It matters more than usual here
because the walk-back is a REVERT, not a dial, so an unflown seal is harder to back out of than any
previous one. The convention permits it (tagged, pushed, gate-green), but the asymmetry is real and
should stay on the record until Chad rules.

The three files below live in Chad's kernel folder, not in this repo, so they are yours:
`docs/cascade/camera-anchor-mode-duality.md`, `DECISIONS.md`, `KERNEL_SEAL`.

## The ruling

Chad, flying v7: *"When I am flying in mouse aim the snap back to chase is occurring with every hard
key press. I usually fly with a combination of mouse aim with hard key inputs to maximize control for
the fight... if I input some aileron to cut into their path sooner, I get a disorienting snap to the
chase cam which throws off my aim and feels unnatural."*

Verbatim rule: **"Only the precedence of the freelook push shall do that."**

Resolved: **S-keychase is removed, not gated.** There is exactly ONE camera automation — the snap to
chase on freelook release (`release_orient` / `release_orient_with_keys`, fires with or without keys
held, untouched). After it the camera lags the AIM under mouse authority alone, permanently. Override
keys reach the TRAJECTORY and never the camera.

## §1 mode table — do NOT merely collapse it, SIMPLIFY it

The key-flying camera mode ceases to exist. The replacement is not a table with a row deleted; it is
Chad's own three lines, and this exact wording now appears in `SPEC.md` §9.2 so the two cannot fork:

> **freelook** — aim carried, camera free.
> **mouse-aim** — aim free, camera bound to the aim.
> **keys** — affect neither.

This is a cleaner statement than v7's duality, and it is the version that should stand.

## §3 chase_anchor math — delete it

`struct ChaseAnchor` and `render::chase_anchor(keys_flying, lead, lag_base, lag_gain, key_rate)` are
DELETED, along with `[camera] key_anchor_rate`, `ControllerParams::cam_key_anchor_rate`,
`MiniCamera::advance`'s `keys_flying` parameter, and the two selector `TEST_CASE`s. `app/main.cpp`
now calls `ease_chase_forward` with the `[camera]` dials directly — the sealed-v6 line. The only
camera law left to document is `ease_chase_forward` with `lead/lag_base/lag_gain`, rest target = the
aim, always.

A full RETIRED record (what it was, why it went, the fly-approval cautionary tale) is preserved in
`render/camera.h` above the deletion point, and in `SPEC.md` §0 under S-keyprec / S-keychase.

## DECISIONS.md entry

- **Decision:** override keys have no camera authority, ever. One camera automation only (freelook
  release). Camera is aim-anchored at all other times.
- **Status:** ruled by Chad 2026-07-29; implemented v8; **awaiting his fly** (`docs/v8_fly_cards.md`).
- **Walk-back:** a revert, not a dial — deliberately. `sandbox/s-keychase-retired`, tag
  `flight-kernel-v7-2026-07-29`. Rationale: the ruling is categorical, so a live knob at zero would be
  a loaded gun; precedent is S-aimclamp / S-retclamp (flown-rejected mechanisms get reverted, code
  preserved on a branch).
- **Supersedes:** S-keychase (v7) in full, including its GUNNERY reclassification and its
  "the handback must never be blended" corollary — both are moot with the mechanism gone.

## Two things worth carrying into the docs as lessons

1. **Attribution.** Chad's original oblique complaint was caused by the **D9 exception** (releasing
   freelook with keys held fired no snap at all). Retiring D9 fixed it. S-keychase was then stacked on
   top to also flatten the *standing* state — an over-correction, and the second mechanism is the one
   that fought his mouse. Two fixes for one defect; the second was the bug.

2. **Why a flown approval didn't hold.** S-keychase was approved "precisely perfect" against
   `comfort_turnsteady_keys`, which **parks the aim and flies on keys alone**. Chad flies mouse-aim AND
   keys simultaneously, and no scenario modeled that, so the defect was structurally unmeasurable. The
   new `comfort mouseaim_keys` scenario closes it and was built and shown FAILING on v7 *before*
   anything changed: camera-lag-behind-aim **0.000° → 67.781°** on the keypress. This is the third
   camera mechanism validated against a case he does not fly (cf. S-aimclamp, S-retclamp) — the
   generalizable rule is that a feel mechanism's instrument must model **both hands at once**.

## Numbers for the ledger

| metric | v7 | v8 |
|---|---|---|
| `comfort mouseaim_keys` lag-behind-aim, keys down | 67.781° | 10.492° |
| `comfort mouseaim_keys` max per-tick step at the edge | 3.222° | 0.378° |
| `comfort turnsteady_keys` standing oblique (keys only) | 0.000° | 16.341° (accepted, ruled) |
| gate | 388/388 | 387/387 (−2 retired cases, +1 new leg) |

⚠ `max_step` is bounded by construction — v7's swap was EASED at `key_anchor_rate·dt = 6.0/120 ≈
2.9°/tick`, so the peak saturates there however bad it feels. Cite the **before-vs-during divergence**,
not the peak, or the strongest evidence reads as trivial.

The 10.492° v8 residual is **not** an anchor effect: it is the ordinary chase lag responding to an aim
that swings faster once the keys bite — Chad's "my lag camera will be able to predictably catch up."

## Also fixed, worth a line

`comfort_detail::drive()` called `MiniCamera::advance` with no `keys_flying` argument, silently taking
the `false` default — the flag had to be threaded by hand through every call site and this convenience
wrapper dropped it, the very instrument fork `chase_anchor`'s purity was built to prevent. Measured
honestly when fixed: **no comfort number moved**, because no `drive()`-based scenario ever reached
keys-without-freelook (`turnsnap_ovr` holds its override only while freelook is *also* held, which the
selector excluded). So: a real latent defect with **zero measured consequence** — it would have bitten
the first scenario to model keys-in-mouse-aim. v8 removes the class outright, since the seam no longer
takes key state.
