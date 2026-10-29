# Nightmare Northshire (`mod-nightmare`)

This document describes the **current** Nightmare Kobold module. The
original [implementation plan](plans/2026-03-08-nightmare-kobold-fire.md) is
historical and contains superseded API examples.

## Gameplay

- Kobold Vermin (6) and Kobold Worker (257) spawn timed Candle Fire (gameobject
  900001) when they die.
- Players in fire take fire-school environmental damage after each full exposure
  interval. Leaving the fire resets the timer. Fire resistance and absorbs apply.
- Kobolds inside fire gain doubled outgoing damage, faster attacks and faster
  movement. Their original speed *rate* and attack time are restored on exit.
- The kobolds retain their original aggro text through `JustEngagedWith`.
- A nearby-fire cap prevents unlimited overlapping gameobjects in a small area.

Northshire Valley is area 9, but its parent zone is **Elwynn Forest (12)**.
The player fire effect checks zone 12 and requires proximity to the custom
gameobject, which also covers Northshire subareas such as the abbey and mine.

## Configuration

The following optional keys may be added to `worldserver.conf`. They are
read on first use; **restart worldserver after changing them**. Values outside
the supported ranges are clamped.

| Key | Default | Range | Description |
| --- | ---: | ---: | --- |
| `Nightmare.FireRadius` | 8.0 | 1–40 | Horizontal damage/buff radius in yards |
| `Nightmare.FireHeightTolerance` | 3.0 | 0.5–10 | Maximum vertical separation in yards |
| `Nightmare.FireDurationSeconds` | 600 | 1–3600 | Fire lifetime |
| `Nightmare.FireTickMs` | 2000 | 100–60000 | Player exposure interval |
| `Nightmare.FireDamagePercent` | 0.10 | 0.01–1.0 | Fraction of maximum health per tick |
| `Nightmare.FrenzyDamageMultiplier` | 2.0 | 1–10 | Damage multiplier |
| `Nightmare.FrenzyAttackTimeMultiplier` | 0.5 | 0.1–1.0 | Base-attack-time multiplier |
| `Nightmare.FrenzyMoveSpeedMultiplier` | 1.5 | 1–3 | Movement-rate multiplier |
| `Nightmare.MaxNearbyFires` | 5 | 1–50 | Maximum fires within twice the radius |

Example:

```ini
Nightmare.FireRadius = 8.0
Nightmare.FireDamagePercent = 0.10
Nightmare.MaxNearbyFires = 5
```

These keys affect only the Nightmare module. The fire visual's model scale is
set separately by SQL and need not exactly match the damage radius.

## Database and build

The two world-database updates in `modules/mod-nightmare/data/sql/db-world/` define
the fire template, assign the C++ script to both kobold templates, and set
their base health/damage modifiers. The module's updater installs them automatically.
The modifiers are relative boosts applied once by the updater; do not manually
re-run the migration against an already upgraded database.

The fire template uses a one-time `INSERT` that fails if entry `900001` already
exists. This deliberately avoids deleting or overwriting an existing template.
The generic SQL linter requests a preceding `DELETE` for inserts, but its
separate template-safety rule forbids that deletion; retain the safer insert.

Build with `MODULES=static`.
The module has its own script loader and needs no custom core script registration.
Its fire-damage handling includes fire resistance, absorbs and an environmental
combat log, without changing core `Player.cpp`.

Local verification (2026-10-29): A full `MODULES=static` Release build linked
`authserver`, `worldserver`, and the unit tests; both servers' `--help` passed.
The module C++ also passed `-Wall -Wextra -Werror`. Both SQL updates applied
successfully on a disposable MySQL 8.4.13 world schema. The native suite passed
5,958 tests, skipped 5,479, and failed none. No GitHub Actions were used. The
in-game checks below still require a WoW client and game data.

## Runtime verification checklist

1. Confirm the SQL updates applied and that `npc_nightmare_kobold` is assigned
   to creature entries 6 and 257.
2. Aggro a kobold and confirm its original dialogue still plays.
3. Kill a kobold and confirm fire spawns and remains for the configured time.
4. Stand within eight yards of fire, including Northshire subareas; verify a
   tick after two full seconds, then exit and reenter to verify timer reset.
5. Check fire resistance and absorbs reduce damage, and full immunity blocks it.
6. Lead a kobold into fire, check damage, attack time and movement, then lead
   it out and verify the original stats return.
7. Kill enough nearby kobolds to hit the nearby-fire cap.
8. Check the visual footprint against the configured gameplay radius; adjust
   the SQL model scale if visual feedback is misleading.

## Known constraints

- No Candle Frenzy buff icon is shown; the behavior is implemented in C++.
- Changes to unrelated movement effects while Candle Frenzy is active may
  need a more elaborate modifier-based implementation for perfect stacking.
- Fire is scoped to Elwynn Forest for player damage. Additional zones require
  an explicit scope change.
