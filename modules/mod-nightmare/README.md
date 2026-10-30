# Nightmare

Nightmare mode for AzerothCore, starting with Kobold Candle Fire and diseased wolf packs in Northshire.

Build AzerothCore with `MODULES=static`. The core discovers and registers this module
automatically. Worldserver's database updater installs the world SQL from
`data/sql/db-world/` when the module is enabled; existing installations retain
their migration history. Do not manually replay the one-time SQL updates.

Kobold gameplay, settings, and checks: [Nightmare Northshire](../../docs/nightmare-northshire.md).

## Diseased wolves

Diseased Young Wolves (299) and Diseased Timber Wolves (69) are hostile by default.
Every successful melee hit on a player adds one Infected Bite stack (maximum 10).
Each stack increases all incoming damage by 10% and slows movement by 10%.
At 10 stacks, the player is rooted. All stacks expire 10 seconds after the last bite.
The debuff uses the client's existing Infected Bite (7367) icon with a visible
stack count and countdown. Its stock tooltip does not describe Nightmare's
custom modifiers, so the first bite also explains the actual effects in system
chat. Dispelling the disease removes both the icon and Nightmare's modifiers.

Once per combat, a wolf at or below 50% health warns nearby players with a text
emote, then calls every unengaged Diseased Young/Timber Wolf within 30 yards after
two seconds. Wolves must be on the same floor (within five yards vertically), have
line of sight to the howler, and not already be in combat. Recruited wolves can
themselves howl when wounded. Killing the howler during the warning prevents
the call.

The seven optional wolf settings in `conf/mod-nightmare.conf.dist` are loaded on
first use and require a worldserver restart to change.

Configuration defaults are supplied in `conf/mod-nightmare.conf.dist`. Copy the
installed file to `etc/modules/mod-nightmare.conf` to customize the gameplay
settings. With `BUILD_TESTING=ON`, the module's regression cases join the native
`unit_tests` target.

No GitHub Actions are needed or used for Nightmare verification.

The Laborer/shared-fleeing migration is currently staged at
`data/sql/updates/pending_db_world/rev_1793304582886642446.sql` in the core checkout.
It binds creature entries 6, 80 and 257 to the same AI and must be applied only
when this module is enabled. See the gameplay document before deployment.
