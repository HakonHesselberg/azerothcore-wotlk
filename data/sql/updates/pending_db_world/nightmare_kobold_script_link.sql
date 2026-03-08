-- Link Kobold Vermin and Kobold Worker to nightmare script
-- Also give them a slight stat boost for nightmare mode

UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 6;
UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 257;

-- Increase base stats: +50% health, +30% damage
-- These make kobolds dangerous even without Candle Frenzy
UPDATE `creature_template` SET
    `HealthModifier` = 1.5,
    `DamageModifier` = 1.3
WHERE `entry` IN (6, 257);
