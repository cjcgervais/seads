# Ballistics ground-truth reference (Bf 109 gun battery)

The **cited, human-readable** companion to `ballistics_reference.json` (the machine
source the metric reads). The forge loop's RESEARCH step fills this in; every number
here must trace to a source and match the JSON exactly.

## What the metric needs, per gun
Under the JSON `test_condition` (flat fire, still air, ICAO sea level, muzzle
horizontal, shooter at rest), at ranges **100 / 300 / 500 m** (add 200/400/800 if
sourced):
- **muzzle_speed_mps** — cited MV for the *aircraft* mounting (barrel length matters).
- **mass_g**, **diameter_mm** — projectile, for the drag / BC derivation.
- **tof_s** — time of flight to range.
- **drop_m** — gravity drop at range (flat fire).
- **vret_mps** — retained velocity at range (this is the number the vacuum model
  gets catastrophically wrong — it is the loop's primary target).

## The four guns
| id | gun | round | on F-4/R1 | notes |
|---|---|---|---|---|
| `MG151_20` | 20 mm MG 151/20 | Minengeschoß (thin-wall HE) | **yes** (hub + gondola) | light/blunt → poor BC, sheds velocity fast |
| `MG17_792` | 7.92 mm MG 17 | sS ball | **yes** (cowl) | |
| `MG131_13` | 13 mm MG 131 | | no | later variants / completeness |
| `MGFF_M_20` | 20 mm MG-FF/M | Minengeschoß (20×80RB) | no | canonical low-velocity E/F cannon |

## Sourcing rules (research step)
- Prefer primary/period data: Waffen-Revue, Rheinmetall/Mauser tables, WWII Aircraft
  Performance, Tony Williams' *Flying Guns* series, published G1/G7 BCs or drag tables.
- Where only MV + mass + form are known, derive the retained-velocity curve from a G1
  drag model and **Fable cross-checks** the derivation (internal consistency: drop
  must follow from the same TOF; vret must fall monotonically; a light M-Geschoß must
  decay faster than a dense AP round of the same calibre).
- Record the source string in each JSON gun's `source` field; cite it here.
- Flag anything still recall-only as PROVISIONAL — the loop does not converge while a
  PROVISIONAL tag remains in a gun that is `on_airframe: true`.

## Current status
All four guns are **PROVISIONAL-RECALL** (seeded so the harness runs). Baseline vacuum
model scored **E ≈ 1.11** — see `ballistics_forge_log.md`. Replace these first.

## Sources
_(research step appends cited sources here)_
