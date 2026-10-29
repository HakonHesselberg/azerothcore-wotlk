-- Nightmare Northshire: share Candle Fire, Candle Frenzy and low-health fleeing with Laborers.
-- Keep native faction, stats, spawns, creature_text and quest 21 unchanged.
-- Explicitly clear SmartAI selection when assigning the shared C++ script.
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` IN (6, 80, 257);
