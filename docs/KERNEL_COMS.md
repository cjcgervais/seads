# KERNEL COMS — outward-facing intent

This is the **customer-facing intent ledger**: the design commitments Chad chooses to
communicate to a potential player — someone reading a Steam page, a devlog, or a store
description — as distinct from `DECISIONS.md`, which records the internal reasoning that
produced them.

Rules of this document:

- **Entries are added on Chad's word only.** This is his voice to his customers.
- **Chad's verbatim words are the source of every entry** — quoted, never paraphrased.
  Player-facing renderings below each source are **DRAFTS until Chad approves them**;
  approved renderings get marked as such.
- **Nothing goes here that the kernel doesn't actually do.** An outward-facing promise is
  a claim, and this project's whole discipline is that claims get reconciled against the
  artifact. If a coms entry ever stops being true of the shipped kernel, that is a STOP —
  the entry is corrected or the kernel is, never neither.
- Internal cross-reference: each entry names the `DECISIONS.md` entries it stands on.

---

## COMS-1 (2026-07-30) — CONTROL IS KING

### The philosophy, in Chad's words (source, verbatim)

> "My game philisophy is that controls is life. Or control and feel is evertyhing. I
> guess for sounbite stuff wee could say that control is king."

> "Feeling comfortable at the mouse helm will be something I want to offer my players so
> that they might better focus on situational awareness, strategy, tactics and their
> required manouvers in realtion to the flying adversary. I want to be undpedictable to
> them not myself. :)"

> "A smoothly controlled plane's nose will follow a straight line to its target. ...
> Curver are nice, but I can decide what and when that looks like via inputs rather that
> adjusting to quirks."

**Soundbite ladder** (Chad's own, shortest last):
1. Control and feel is everything.
2. Controls is life.
3. **Control is king.**

### Player-facing rendering — DRAFT, awaiting Chad's approval

> **Control is king.**
>
> Your plane is not your enemy — the other pilot is. Every hour of this game's
> development starts from one rule: the aircraft must never cost you a thought that
> belongs to the fight. Your nose flies a straight line to where you aim. Your tracers
> go where you pointed them. When the flight path curves, it curves because you asked
> it to.
>
> We build it this way so your mind is free for what actually wins dogfights:
> awareness, strategy, tactics, and the maneuver you're about to spring on someone.
>
> Be unpredictable to them — never to yourself.

### What it stands on (internal)

- `DECISIONS.md` — "STANDING INTENT: the mouse-helm comfort doctrine" (2026-07-30, the
  source ruling and its consequences).
- `DECISIONS.md` — "THE DIP IS A FLAW" ruling (2026-07-30): the doctrine applied — a
  measurable trajectory quirk was ruled a defect *because* it taxed the pilot's
  attention, and a mechanism thread was opened to remove it.
- The compensation-decay law (four flown instances): the ongoing audit that keeps the
  kernel honest to this promise as the plane's authority grows.
- Kernel lineage v5 → v10, each seal flown and approved on the stick before it shipped —
  the promise above is tested by a human hand every time, never only by a harness.
