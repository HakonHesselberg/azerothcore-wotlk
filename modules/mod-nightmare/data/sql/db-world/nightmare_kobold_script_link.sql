-- Link Kobold Vermin and Kobold Worker to nightmare script
-- Also give them a slight stat boost for nightmare mode

UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 6;
UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 257;

-- Apply one-time relative boosts so upstream template tuning is preserved.
-- The database updater must run this migration only once.
UPDATE `creature_template` SET
    `HealthModifier` = `HealthModifier` * 1.5,
    `DamageModifier` = `DamageModifier` * 1.3
WHERE `entry` IN (6, 257);
