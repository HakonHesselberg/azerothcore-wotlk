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
  movement. Attack haste is applied and removed through AzerothCore's reversible
  attack-time modifier. Run speed is recalculated from the current creature
  stats, buffs, debuffs and health effects before applying the Frenzy multiplier,
  and is recalculated again on exit or reset. No old speed or attack-time
  snapshot is restored.
- The kobolds retain their original aggro text through `JustEngagedWith`.
- A nearby-fire cap prevents unlimited overlapping gameobjects in a small area.

Northshire Valley is area 9, but its parent zone is **Elwynn Forest (12)**.
The player fire effect checks zone 12 and requires proximity to the custom
gameobject, which also covers Northshire subareas such as the abbey and mine.

## Configuration

All keys have defaults in
`modules/mod-nightmare/conf/mod-nightmare.conf.dist`, which AzerothCore installs
as `etc/modules/mod-nightmare.conf.dist`. Copy it to
`etc/modules/mod-nightmare.conf` for an installation-specific configuration.
Settings are read on first use; **restart worldserver after changing them**.
Values outside the supported ranges are clamped.

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

Local verification (2026-10-29, Ubuntu 24.04, Clang 18, MySQL 8.0.46):

- A full Release/Ninja build with `MODULES=static` and `BUILD_TESTING=ON`
  compiled and linked `authserver`, `worldserver`, and `unit_tests`. Both
  server binaries passed `--help`. CMake installation put
  `mod-nightmare.conf.dist` in `etc/modules/`.
- All **14** new `Nightmare*` regression tests passed (fire damage/caps,
  exposure timing, radius and spawn limits, plus reversible Frenzy attack and
  movement behavior). The full native suite reported **5,972 passed, 5,479
  skipped, 0 failed** (11,451 cases run; one additional test disabled).
  `ctest --output-on-failure` passed its 1/1 native test target. The first
  sandboxed full-suite attempt had nine existing config tests fail because
  `/tmp` was inaccessible; rerunning locally with temporary-file access
  passed all nine, with no code changes.
- On a **disposable MySQL 8.0.46** instance, `authserver` populated the auth
  schema and listened on TCP 3724. `worldserver` connected to auth, characters,
  and world, loaded `mod-nightmare.conf`, populated and updated both game
  schemas, initialized scripts and game data, and reached a healthy listening
  state on TCP 8085. The server log reported `WORLD: World Initialized In 0
  Minutes 38 Seconds` followed by `ready...`; `Errors.log` was empty. Realm 1
  reported online (`flag=0`, game build 12340).
- The world schema recorded `nightmare_kobold_fire_gameobject.sql` and
  `nightmare_kobold_script_link.sql` with state `MODULE`. Creature entries 6
  and 257 both had `ScriptName=npc_nightmare_kobold`, HealthModifier 1.5 and
  DamageModifier 1.3. GameObject 900001 existed (type 5, display 5752,
  size 2.0). There were 108 character tables and 314 world tables.
- Server data used the [AC Data v20 enUS release](https://github.com/wowgaming/client-data/releases/tag/v20.0)
  (DBC/maps/vmaps/mmaps), verified against its published SHA-256 checksum.
  A nonfatal process-priority permission warning appeared at startup.
  **No WoW client and no GitHub Actions were used.**

This verifies headless initialization and listening state, not the live
player/creature interactions in the checklist below.

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
- Run-speed recomputation occurs every 500 ms while Frenzy is active. Very
  short-lived speed effects can have up to one check interval before Frenzy's
  extra speed is reapplied; the speed is always calculated from current effects.
- The native Frenzy interaction tests use a lightweight Unit-API stand-in,
  since constructing a live creature requires initialized world/map data.
  Live aura stacking and movement packets remain in-game checks.
- Fire is scoped to Elwynn Forest for player damage. Additional zones require
  an explicit scope change.

## Shared low-health behavior (Laborer implementation)

Vermin (6), Workers (257), and Laborers (80) now use the same AI. They attempt
ordinary core escape movement at or below `Nightmare.FleeHealthPercent` (default
20%, range 1–100) while engaged with a victim and outside Candle Frenzy.
Frenzy stops a Nightmare-owned escape; leaving fire at low health allows escape
again. Healing above the threshold, combat exit/reset, and death stop that escape.
Root/slow/haste effects remain handled by the core; the escape adds no speed
modifier. Fear/confusion or another controlled movement takes precedence, and
Nightmare cleanup never intentionally removes that replacement movement.
The flee generator chooses escape paths without seeking allies or calling an
assistance API; ordinary nearby-creature aggro/assistance rules still apply.
Fire exposure/Frenzy checks retain their existing 500 ms cadence.

Laborer death uses the existing Candle Fire lifetime and shared nearby-fire cap.
Laborers keep their native faction, stats, spawns, aggro dialogue (group 0), loot,
and quest 21 objectives. The migration explicitly clears `AIName` on the three
entries so SmartAI cannot override the shared script.

Deployment: the new migration is staged in
`data/sql/updates/pending_db_world/rev_1793304582886642446.sql` under repository
SQL-editing policy, rather than the module SQL directory. Apply it to a world
database only together with the enabled, updated Nightmare module; do not ship
it independently to a server without the module. Before distributing module-only
SQL, move/package this migration under explicit authorization. Do not replay the
old stat-multiplier migration. Removing the module later requires restoring the
upstream SmartAI binding for these entries.

The base world data identifies 25 Laborer spawns on map 0 (GUID 247,
80060, 80064–80071, 80074–80088), clustered around Northshire. Exact current
spawn coverage and indoor placement need effective database/client confirmation.

Verification for this change: seven native `NightmareCowardice` regression cases
were added for threshold boundaries/configuration, repeated updates, Frenzy
transitions, healing/death/combat exit, external movement precedence, fear handoff and large
health values. The existing 14 fire/Frenzy tests remain in place. These additions
have not been compiled or run: repository instructions prohibit builds without
an explicit request. Earlier verification results above predate this change.
No new game-client or worldserver verification is claimed.

Client checks for the shared behavior:

1. Aggro each of the three entries; check hostility and original aggro dialogue.
2. Reduce health to 20% and verify escape rather than melee, without an explicit
   run-to-allies action. Change the threshold and restart to check configuration.
3. Ignite Frenzy during escape; verify it stops escaping and resumes combat.
   Leave fire while still low and verify escaping becomes possible again.
4. Heal above threshold, evade/reset, kill and respawn; check no stale movement.
5. Apply root, slow, fear and confusion during escape. Verify external movement
   survives Frenzy activation and cleanup, and ordinary combat resumes afterward.
6. Kill Laborers beside existing fires; verify capped fire spawning and original
   fire lifetime/damage/Frenzy behavior for all three entries.

This change does not introduce mine supports or rockfall damage.
