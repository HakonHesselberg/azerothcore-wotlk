# Nightmare

Nightmare mode for AzerothCore, starting with Kobold Candle Fire in Northshire.

Build AzerothCore with `MODULES=static`. The core discovers and registers this module
automatically. Worldserver's database updater installs the world SQL from
`data/sql/db-world/` when the module is enabled; existing installations retain
their migration history. Do not manually replay the one-time SQL updates.

Gameplay, settings, and checks: [Nightmare Northshire](../../docs/nightmare-northshire.md).

No GitHub Actions are needed or used for Nightmare verification.
