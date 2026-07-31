# v8 FLY CARDS — S-keyprec (the hard key never moves the camera)

Branch `feel/kernel-v5` @ `49e5f93da`. Gate **387/387**, zero moved goldens.
Walk-back for everything below: **a revert**, not a dial — `git revert 49e5f93da`, or
`sandbox/s-keychase-retired` / tag `flight-kernel-v7-2026-07-29` for the v7 code.

## What changed, in one line

S-keychase is **gone**. Your ruling: *"Only the precedence of the freelook push shall do that."*
One camera automation — the snap to chase on freelook release — then the camera lags your **aim**
under mouse authority alone, permanently. Hard keys move the **plane**, never the camera.

- **freelook** — aim carried, camera free.
- **mouse-aim** — aim free, camera bound to the aim.
- **keys** — affect neither.

## Measured before you fly (the scenario that was missing)

A new harness scenario finally models **mouse-aim AND keys at once** — how you actually fly. It was
built and shown FAILING on v7 first, so this is a fix against evidence, not an argument:

| | v7 (S-keychase) | v8 |
|---|---|---|
| camera's lag behind your AIM, keys down | **67.781°** | **10.492°** |
| worst single-tick jump at the keypress | 3.222° | 0.378° |
| keys-only standing oblique (`turnsteady_keys`) | 0.000° | **16.341°** ← comes back, see CARD 3 |

The 10.5° residual is not an anchor change — it is the ordinary chase lag catching an aim that
swings faster once the keys bite. Exactly your "my lag camera will be able to predictably catch up."

---

## CARD 1 — the repro. Mouse-aim + hard keys.

Fly mouse-aim, track something, then **punch aileron to cut inside** — the exact thing that was
snapping you.

- **Expect:** the camera does **nothing of its own**. It stays behind your aim with the lag you
  approved. The plane cuts; the view does not jump.
- **Watch for:** any camera motion at the *instant* the key goes down or comes up. There should be
  none. Also try tapping keys repeatedly mid-track — no accumulating drift, no pumping.
- **Fails if:** you still feel a pull toward the flight path while keys are held.

## CARD 2 — the one automation still fires. Freelook release, keys held.

Press freelook, look around, **keep a hard key down**, release freelook, keep flying on the key.

- **Expect:** unchanged from v7 — instant chase-behind, horizon rights itself while you keep flying
  on the key. This is the S-relorient addendum you approved as "precisely perfect"; nothing here
  touched it.
- **Also check:** a clean mouse-only release, and a double-tap. Both should be identical to v7.
- **Fails if:** the release snap is weaker, late, or missing in any of those three.

## ⭐ CARD 3 — THE ONE THAT MATTERS. Fly on keys alone and rule on the drift.

Hold the mouse **still** and turn the plane on keys only, sustained.

- **Expect:** the plane turns out from under your parked aim, so you end up looking at it ~16°
  obliquely. The camera is showing where you are **pointing**, not where you are **going**.
- **This is the thing v8 gives back.** S-keychase existed to flatten exactly this to 0°, and you
  approved it as "precisely perfect" — but it was tested against a scenario that parks the aim and
  never touches the mouse, which is not how you fly. Removing it restores the drift.
- **The ask:** do not just confirm the snap is gone. Rule *affirmatively* on whether this drift is
  the view you want. Moving the mouse cures it instantly — the question is whether that is the right
  bargain.

- ⚠ **AND THE QUESTION THIS CARD REALLY ANSWERS — CAN YOU STILL READ A DEFLECTION SHOT?** (flagged by
  the docs agent; genuinely open, not assumed either way). Your gunnery reasoning for v7 was: *"I am
  looking through the nose line of the plane from behind, or see its angle from behind its velocity —
  and it may be obliquely aligned at an enemy, but I see where I am shooting."* You gave that while
  describing the **behind-velocity** view — which is precisely the view v8 removes. Whether the
  aim-bound camera serves that same shooting need is **unresolved**, and nobody should assume it does.
  So while flying this card, take an actual **gunnery** read: set up a deflection shot on a hard key
  turn and see whether you can still judge the nose-vs-flight-path angle well enough to shoot. If you
  cannot, that is a real finding and NOT a contradiction of your v8 ruling — it would mean the
  shooting need is separate from the camera-authority question, and wants its own mechanism (e.g. a
  HUD/instrument read of the gun line) rather than giving the keys camera authority back.
- **If it reads wrong:** say so and stop. Do **not** expect a dial — the knob was deliberately
  deleted, so restoring any part of the key-flown anchor is a revert plus a fresh design round. That
  is the honest cost of the categorical ruling, and it is why this card exists.

---

## Honest residual (stated, not hidden)

No ctest runs `seads.exe`, so the app's own camera line is verified by derivation and by the harness
mirror, not by an executable pin — the standing structural blindness (CLAUDE.md). What *is* pinned:
the camera must sit nearer the AIM than the FLIGHT PATH (`test_relorient.cpp` "S-keyprec…",
mutation-verified), and the harness camera seam takes no key state at all, so a re-introduction
cannot reach it without changing that signature in plain view.
