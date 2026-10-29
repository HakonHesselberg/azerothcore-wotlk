-- Nightmare Northshire: make both diseased wolf types aggressive to players.
-- FactionTemplate 38 = hostile Beast - Wolf; 32 = neutral Beast - Wolf.
-- Applies to creature entries 69 (Diseased Timber Wolf) and 299 (Diseased Young Wolf).
UPDATE `creature_template` SET `faction` = 38 WHERE `entry` IN (69, 299);
