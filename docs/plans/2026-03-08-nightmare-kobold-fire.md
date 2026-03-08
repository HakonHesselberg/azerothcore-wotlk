# Nightmare Kobold Fire Mechanic Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** When kobolds die, they leave a burning fire zone that damages players and empowers other kobolds standing in it.

**Architecture:** Custom CreatureScript hooks into kobold death events to spawn a fire GameObject. A periodic aura checks positions and applies buffs/damage. All server-side using existing client visuals.

**Tech Stack:** C++ (AzerothCore scripting API), SQL (spell/gameobject definitions), Display ID 5752 (fire visual)

---

## Constants Reference

```
Kobold Entries:
- Kobold Vermin: 6
- Kobold Worker: 257

Fire Mechanics:
- Radius: 8 yards
- Duration: 10 minutes (600,000 ms)
- Player damage: ~10% max HP per tick

Candle Frenzy Buff:
- Damage modifier: +100% (2x)
- Attack speed: +100%
- Movement speed: +50%

Display ID: 5752 (Consuming Flames visual)
```

---

## Task 1: Create Custom Spell - Candle Frenzy Buff

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_kobold_candle_frenzy_spell.sql`

**Step 1: Define the Candle Frenzy aura spell**

We need a buff that kobolds receive when inside fire. Using spell_dbc to create a custom spell.

```sql
-- Candle Frenzy buff for kobolds in fire
-- Custom spell ID: 99001 (high ID to avoid conflicts)

DELETE FROM `spell_dbc` WHERE `ID` = 99001;
INSERT INTO `spell_dbc` (`ID`, `Attributes`, `AttributesEx`, `AttributesEx2`, `AttributesEx3`, `AttributesEx4`, `AttributesEx5`, `AttributesEx6`, `AttributesEx7`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `Speed`, `SpellVisual1`, `SpellIconID`, `ActiveIconID`, `SpellName`, `DmgClass`, `EffectApplyAuraName1`, `EffectApplyAuraName2`, `EffectApplyAuraName3`, `Effect1`, `Effect2`, `Effect3`, `EffectBasePoints1`, `EffectBasePoints2`, `EffectBasePoints3`, `EffectMiscValue1`, `EffectMiscValue2`, `EffectMiscValue3`, `EffectImplicitTargetA1`, `EffectImplicitTargetA2`, `EffectImplicitTargetA3`) VALUES
(99001, 0, 0, 0, 0, 0, 0, 0, 0, 1, 21, 1, 0, 0, 1, 0, 'Candle Frenzy', 0,
79,   -- SPELL_AURA_MOD_DAMAGE_PERCENT_DONE (effect 1: +100% damage)
107,  -- SPELL_AURA_MOD_HASTE (effect 2: +100% attack speed)
31,   -- SPELL_AURA_MOD_SPEED_ALWAYS (effect 3: +50% move speed)
6, 6, 6,  -- SPELL_EFFECT_APPLY_AURA for all 3
100, 100, 50,  -- Base points: +100% dmg, +100% haste, +50% speed
127, 0, 0,    -- Misc values (127 = all damage schools for effect 1)
1, 1, 1);     -- Target self
```

**Step 2: Verify SQL syntax**

Run: `cat data/sql/updates/pending_db_world/nightmare_kobold_candle_frenzy_spell.sql`
Expected: SQL file contents displayed without syntax errors

**Step 3: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_kobold_candle_frenzy_spell.sql
git commit -m "feat(DB): add Candle Frenzy buff spell for nightmare kobolds"
```

---

## Task 2: Create Custom Spell - Fire Zone Damage Aura

**Files:**
- Modify: `data/sql/updates/pending_db_world/nightmare_kobold_candle_frenzy_spell.sql`

**Step 1: Add fire zone damage spell**

This spell damages players standing in the fire. Using percentage-based damage.

```sql
-- Fire Zone damage aura
-- Custom spell ID: 99002
-- Deals periodic damage to players in fire

DELETE FROM `spell_dbc` WHERE `ID` = 99002;
INSERT INTO `spell_dbc` (`ID`, `Attributes`, `AttributesEx`, `AttributesEx2`, `AttributesEx3`, `AttributesEx4`, `AttributesEx5`, `AttributesEx6`, `AttributesEx7`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `Speed`, `SpellVisual1`, `SpellIconID`, `ActiveIconID`, `SpellName`, `DmgClass`, `Effect1`, `EffectApplyAuraName1`, `EffectBasePoints1`, `EffectMiscValue1`, `EffectAmplitude1`, `EffectImplicitTargetA1`) VALUES
(99002, 0, 0, 0, 0, 0, 0, 0, 0, 1, 21, 1, 0, 5765, 1, 0, 'Candle Fire', 1,
6,    -- SPELL_EFFECT_APPLY_AURA
3,    -- SPELL_AURA_PERIODIC_DAMAGE
10,   -- Base damage (we'll scale this in script based on max HP)
2,    -- Fire damage school
2000, -- Amplitude: tick every 2 seconds
1);   -- Target self
```

**Step 2: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_kobold_candle_frenzy_spell.sql
git commit -m "feat(DB): add Candle Fire damage spell for fire zones"
```

---

## Task 3: Create Fire Zone GameObject Template

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_kobold_fire_gameobject.sql`

**Step 1: Define custom fire trap GameObject**

Based on existing "Roaring Flame" (179065) but customized for our needs.

```sql
-- Nightmare Kobold Fire Zone
-- Custom entry: 900001 (high to avoid conflicts)
-- Type 6 = TRAP, Display 5752 = Consuming Flames visual

DELETE FROM `gameobject_template` WHERE `entry` = 900001;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`, `Data8`, `Data9`, `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`, `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`, `Data22`, `Data23`, `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(900001, 5, 5752, 'Kobold Candle Fire', '', '', '', 2.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', '', 0);

-- Note: Using type 5 (GENERIC) for visual only
-- Damage will be handled by the C++ script checking proximity
-- Size 2.0 scales the visual to approximately 8 yards
```

**Step 2: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_kobold_fire_gameobject.sql
git commit -m "feat(DB): add Kobold Candle Fire gameobject template"
```

---

## Task 4: Create Nightmare Kobold Script Header

**Files:**
- Create: `src/server/scripts/Custom/nightmare_northshire.h`

**Step 1: Create header with constants and declarations**

```cpp
/*
 * Nightmare Northshire - Kobold Fire Mechanics
 * When kobolds die, they leave burning fire zones
 * Kobolds in fire gain Candle Frenzy buff
 */

#ifndef NIGHTMARE_NORTHSHIRE_H
#define NIGHTMARE_NORTHSHIRE_H

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "GameObject.h"
#include "Player.h"

namespace NightmareNorthshire
{
    // Kobold creature entries
    constexpr uint32 NPC_KOBOLD_VERMIN = 6;
    constexpr uint32 NPC_KOBOLD_WORKER = 257;

    // Custom spell IDs
    constexpr uint32 SPELL_CANDLE_FRENZY = 99001;
    constexpr uint32 SPELL_CANDLE_FIRE_DAMAGE = 99002;

    // Custom GameObject entry
    constexpr uint32 GO_KOBOLD_FIRE = 900001;

    // Fire zone properties
    constexpr float FIRE_RADIUS = 8.0f;
    constexpr uint32 FIRE_DURATION_MS = 600000; // 10 minutes
    constexpr uint32 FIRE_DAMAGE_TICK_MS = 2000; // 2 seconds
    constexpr float FIRE_DAMAGE_PCT = 0.10f; // 10% max HP per tick

    // Candle Frenzy check interval
    constexpr uint32 BUFF_CHECK_INTERVAL_MS = 1000; // 1 second

    // Northshire Valley zone ID
    constexpr uint32 ZONE_NORTHSHIRE = 9;

    // Helper to check if a position is near any fire
    bool IsPositionInFire(Creature* creature);

    // Helper to get all fire GameObjects near a position
    std::list<GameObject*> GetNearbyFires(WorldObject* obj, float range);
}

#endif // NIGHTMARE_NORTHSHIRE_H
```

**Step 2: Commit**

```bash
git add src/server/scripts/Custom/nightmare_northshire.h
git commit -m "feat(Scripts): add nightmare northshire header with constants"
```

---

## Task 5: Create Nightmare Kobold AI Script

**Files:**
- Create: `src/server/scripts/Custom/nightmare_northshire.cpp`

**Step 1: Create the main script file with kobold AI**

```cpp
/*
 * Nightmare Northshire - Kobold Fire Mechanics
 */

#include "nightmare_northshire.h"
#include "ObjectAccessor.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"

namespace NightmareNorthshire
{
    std::list<GameObject*> GetNearbyFires(WorldObject* obj, float range)
    {
        std::list<GameObject*> fires;
        if (!obj)
            return fires;

        // Find all fire GameObjects in range
        obj->GetGameObjectListWithEntryInGrid(fires, GO_KOBOLD_FIRE, range);
        return fires;
    }

    bool IsPositionInFire(Creature* creature)
    {
        if (!creature)
            return false;

        auto fires = GetNearbyFires(creature, FIRE_RADIUS * 2);
        for (GameObject* fire : fires)
        {
            if (creature->GetDistance(fire) <= FIRE_RADIUS)
                return true;
        }
        return false;
    }
}

using namespace NightmareNorthshire;

// Kobold AI with fire mechanics
struct npc_nightmare_kobold : public ScriptedAI
{
    npc_nightmare_kobold(Creature* creature) : ScriptedAI(creature)
    {
        _buffCheckTimer = BUFF_CHECK_INTERVAL_MS;
    }

    void JustDied(Unit* /*killer*/) override
    {
        // Spawn fire at death location
        float x = me->GetPositionX();
        float y = me->GetPositionY();
        float z = me->GetPositionZ();
        float o = me->GetOrientation();

        // Spawn fire GameObject that lasts 10 minutes
        if (GameObject* fire = me->SummonGameObject(
            GO_KOBOLD_FIRE,
            x, y, z, o,
            0.0f, 0.0f, 0.0f, 0.0f,
            FIRE_DURATION_MS / 1000)) // respawnTime in seconds
        {
            // Fire spawned successfully
        }
    }

    void UpdateAI(uint32 diff) override
    {
        // Check for Candle Frenzy buff
        _buffCheckTimer -= diff;
        if (_buffCheckTimer <= 0)
        {
            _buffCheckTimer = BUFF_CHECK_INTERVAL_MS;

            bool inFire = IsPositionInFire(me);
            bool hasBuff = me->HasAura(SPELL_CANDLE_FRENZY);

            if (inFire && !hasBuff)
            {
                // Apply Candle Frenzy
                me->CastSpell(me, SPELL_CANDLE_FRENZY, true);
            }
            else if (!inFire && hasBuff)
            {
                // Remove Candle Frenzy
                me->RemoveAura(SPELL_CANDLE_FRENZY);
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    int32 _buffCheckTimer;
};

// Player script to handle fire damage
class npc_nightmare_fire_damage : public PlayerScript
{
public:
    npc_nightmare_fire_damage() : PlayerScript("npc_nightmare_fire_damage") { }

    void OnUpdate(Player* player, uint32 diff) override
    {
        // Only in Northshire
        if (player->GetZoneId() != ZONE_NORTHSHIRE)
            return;

        // Track timer per player using a simple approach
        uint32 timer = player->GetCustomDataUInt32("fire_damage_timer");
        timer += diff;

        if (timer >= FIRE_DAMAGE_TICK_MS)
        {
            timer = 0;

            // Check if player is in any fire
            auto fires = GetNearbyFires(player, FIRE_RADIUS * 2);
            bool inFire = false;

            for (GameObject* fire : fires)
            {
                if (player->GetDistance(fire) <= FIRE_RADIUS)
                {
                    inFire = true;
                    break;
                }
            }

            if (inFire)
            {
                // Deal 10% max HP as fire damage
                uint32 damage = player->GetMaxHealth() * FIRE_DAMAGE_PCT;
                player->EnvironmentalDamage(DAMAGE_FIRE, damage);
            }
        }

        player->SetCustomDataUInt32("fire_damage_timer", timer);
    }
};

void AddSC_nightmare_northshire()
{
    RegisterCreatureAI(npc_nightmare_kobold);
    new npc_nightmare_fire_damage();
}
```

**Step 2: Commit**

```bash
git add src/server/scripts/Custom/nightmare_northshire.cpp
git commit -m "feat(Scripts): add nightmare kobold AI with fire mechanics"
```

---

## Task 6: Register Script in CMake and Loader

**Files:**
- Create: `src/server/scripts/Custom/CMakeLists.txt` (if not exists)
- Modify: `src/server/scripts/Custom/custom_script_loader.cpp`
- Modify: `src/server/scripts/CMakeLists.txt`

**Step 1: Create or update Custom CMakeLists.txt**

```cmake
# Custom scripts CMakeLists.txt

set(scripts_STAT_SRCS
  ${scripts_STAT_SRCS}
  Custom/nightmare_northshire.cpp
  Custom/nightmare_northshire.h
)

message(STATUS "SCRIPT: Custom scripts enabled")
```

**Step 2: Create custom_script_loader.cpp if it doesn't exist**

```cpp
/*
 * Custom Script Loader
 */

// Nightmare Northshire
void AddSC_nightmare_northshire();

void AddCustomScripts()
{
    AddSC_nightmare_northshire();
}
```

**Step 3: Verify CMake structure**

Check if Custom directory is included in main CMakeLists.txt:
```bash
grep -r "Custom" src/server/scripts/CMakeLists.txt
```

If not present, add to the scripts CMakeLists.txt.

**Step 4: Commit**

```bash
git add src/server/scripts/Custom/
git commit -m "feat(Scripts): register nightmare northshire in build system"
```

---

## Task 7: Link Kobold Entries to Custom Script

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_kobold_script_link.sql`

**Step 1: Update creature_template to use our script**

```sql
-- Link Kobold Vermin and Kobold Worker to nightmare script
UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 6;
UPDATE `creature_template` SET `ScriptName` = 'npc_nightmare_kobold' WHERE `entry` = 257;

-- Also increase their stats slightly for nightmare mode
-- +50% health, +30% damage to make them feel dangerous
UPDATE `creature_template` SET
    `HealthModifier` = `HealthModifier` * 1.5,
    `DamageModifier` = `DamageModifier` * 1.3
WHERE `entry` IN (6, 257);
```

**Step 2: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_kobold_script_link.sql
git commit -m "feat(DB): link kobolds to nightmare script and buff stats"
```

---

## Task 8: Build and Verify Compilation

**Files:**
- None (build verification)

**Step 1: Create build directory if needed**

```bash
mkdir -p build && cd build
```

**Step 2: Configure CMake**

```bash
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/azeroth-server -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSCRIPTS=static -DMODULES=static
```

Expected: CMake completes without errors, shows "Custom scripts enabled"

**Step 3: Build**

```bash
make -j$(nproc)
```

Expected: Build completes without errors

**Step 4: Commit any build fixes if needed**

```bash
git add -A
git commit -m "fix(Scripts): resolve build issues"
```

---

## Task 9: Create Test SQL for Manual Verification

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_test_commands.sql`

**Step 1: Create GM test commands**

```sql
-- Test commands for nightmare kobold verification
-- Use in-game with .gob add, .cast, etc.

-- To test fire visual:
-- .gob add 900001

-- To test Candle Frenzy buff on target:
-- .cast 99001

-- To spawn a kobold for testing:
-- .npc add 6

-- To check if script is loaded:
-- .reload creature_template
```

**Step 2: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_test_commands.sql
git commit -m "docs(DB): add test commands for nightmare kobold verification"
```

---

## Task 10: Review and Final Integration

**Files:**
- All created files

**Step 1: Review all SQL files**

```bash
cat data/sql/updates/pending_db_world/nightmare_kobold_*.sql
cat data/sql/updates/pending_db_world/nightmare_test_commands.sql
```

**Step 2: Review all C++ files**

```bash
cat src/server/scripts/Custom/nightmare_northshire.h
cat src/server/scripts/Custom/nightmare_northshire.cpp
```

**Step 3: Final commit with all files**

```bash
git status
git add -A
git commit -m "feat: complete nightmare kobold fire mechanic implementation

- Kobolds spawn fire zones on death (8 yard radius, 10 min duration)
- Fire deals 10% max HP damage per 2 seconds to players
- Kobolds in fire gain Candle Frenzy: +100% damage, +100% attack speed, +50% move speed
- Kobolds have +50% HP and +30% damage baseline

Custom spell IDs: 99001 (Candle Frenzy), 99002 (Fire Damage)
Custom GameObject: 900001 (Kobold Candle Fire)"
```

---

## Testing Checklist (In-Game)

After building and running the server:

1. [ ] Create human character, go to Northshire
2. [ ] Find Kobold Vermin, kill it
3. [ ] Verify fire spawns at death location
4. [ ] Verify fire visual is visible (~8 yard radius)
5. [ ] Stand in fire, verify damage (~10% HP per 2 sec)
6. [ ] Aggro another kobold, kite it into fire
7. [ ] Verify kobold gains Candle Frenzy buff (visual/tooltip)
8. [ ] Verify kobold hits harder in fire
9. [ ] Kite kobold out of fire, verify buff drops
10. [ ] Wait 10 minutes, verify fire despawns
11. [ ] Kill multiple kobolds, verify fires stack/overlap correctly

---

## Known Limitations & Future Work

1. **Player damage timer** uses a simple approach; could be optimized with proper event system
2. **Fire visual size** may need tuning (size 2.0 is estimate for 8 yards)
3. **No fire-on-fire stacking logic** - fires can overlap, which is intended
4. **Zone restriction** - only works in Northshire (zone 9); expand for other areas later
