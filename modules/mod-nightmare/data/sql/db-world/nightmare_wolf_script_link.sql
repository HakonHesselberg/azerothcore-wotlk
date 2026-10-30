-- Give both diseased wolf templates their Nightmare C++ behavior.
-- This is a separate one-time module migration from the hostile faction update.
UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_wolf' WHERE `entry` IN (69, 299);
