# Nightmare

Nightmare mode for AzerothCore, starting with Kobold Candle Fire in Northshire.

Build AzerothCore with `MODULES=static`. The core discovers and registers this module
automatically. Worldserver's database updater installs the world SQL from
`data/sql/db-world/` when the module is enabled; existing installations retain
their migration history. Do not manually replay the one-time SQL updates.

Gameplay, settings, and checks: [Nightmare Northshire](../../docs/nightmare-northshire.md).

Configuration defaults are supplied in `conf/mod-nightmare.conf.dist`. Copy the
installed file to `etc/modules/mod-nightmare.conf` to customize the ten gameplay
settings. With `BUILD_TESTING=ON`, 21 module regression cases join the native
`unit_tests` target.

No GitHub Actions are needed or used for Nightmare verification.

The Laborer/shared-fleeing migration is currently staged at
`data/sql/updates/pending_db_world/rev_1793304582886642446.sql` in the core checkout.
It binds creature entries 6, 80 and 257 to the same AI and must be applied only
when this module is enabled. See the gameplay document before deployment.
