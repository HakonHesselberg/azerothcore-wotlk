# Nightmare Kobold Fire Mechanic Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** When kobolds die, they leave a burning fire zone that damages players and empowers other kobolds standing in it.

**Architecture:** Custom CreatureScript hooks into kobold death events to spawn a visual-only fire GameObject. C++ code handles all damage/buff logic by checking positions periodically. No custom spells needed.

**Tech Stack:** C++ (AzerothCore scripting API), SQL (gameobject template only), Display ID 5752 (fire visual)

---

## Constants Reference

```
Kobold Entries:
- Kobold Vermin: 6
- Kobold Worker: 257

Fire Mechanics:
- Radius: 8 yards
- Duration: 10 minutes (600 seconds)
- Player damage: ~10% max HP per 2 seconds

Candle Frenzy (applied via C++):
- Damage multiplier: 2x (100% increase)
- Attack speed: 2x faster (50% reduction in attack time)
- Movement speed: +50%

Display ID: 5752 (Consuming Flames visual)
GameObject Entry: 900001
Zone ID Northshire: 9
```

---

## Task 1: Create Fire Zone GameObject Template

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_kobold_fire_gameobject.sql`

**Step 1: Define custom fire visual GameObject**

Type 5 is GENERIC (visual only). All damage logic handled in C++.

```sql
-- Nightmare Kobold Fire Zone
-- Custom entry: 900001 (high to avoid conflicts)
-- Type 5 = GENERIC (visual only), Display 5752 = Consuming Flames visual
-- Size 2.0 scales the visual to approximately 8 yards

DELETE FROM `gameobject_template` WHERE `entry` = 900001;
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `size`, `Data0`, `Data1`, `Data2`, `Data3`, `Data4`, `Data5`, `Data6`, `Data7`, `Data8`, `Data9`, `Data10`, `Data11`, `Data12`, `Data13`, `Data14`, `Data15`, `Data16`, `Data17`, `Data18`, `Data19`, `Data20`, `Data21`, `Data22`, `Data23`, `AIName`, `ScriptName`, `VerifiedBuild`) VALUES
(900001, 5, 5752, 'Kobold Candle Fire', '', '', '', 2.0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', '', 0);
```

**Step 2: Commit**

```bash
cd /home/hhess/dev/azerothcore
git add data/sql/updates/pending_db_world/nightmare_kobold_fire_gameobject.sql
git commit -m "feat(DB): add Kobold Candle Fire visual gameobject template"
```

---

## Task 2: Create Nightmare Kobold Script File

**Files:**
- Create: `src/server/scripts/Custom/nightmare_northshire.cpp`

**Step 1: Create the complete script file**

This single file contains:
- Constants
- Fire position tracking
- Kobold AI with death fire spawn and Candle Frenzy buff logic
- Player damage script with static timer map

```cpp
/*
 * Nightmare Northshire - Kobold Fire Mechanics
 *
 * When kobolds die, they leave burning fire zones.
 * - Players in fire take 10% max HP damage every 2 seconds
 * - Kobolds in fire gain Candle Frenzy: 2x damage, 2x attack speed, +50% move speed
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "GameObject.h"
#include "Player.h"

namespace NightmareNorthshire
{
    // Kobold creature entries
    constexpr uint32 NPC_KOBOLD_VERMIN = 6;
    constexpr uint32 NPC_KOBOLD_WORKER = 257;

    // Custom GameObject entry
    constexpr uint32 GO_KOBOLD_FIRE = 900001;

    // Fire zone properties
    constexpr float FIRE_RADIUS = 8.0f;
    constexpr uint32 FIRE_DURATION_SEC = 600; // 10 minutes

    // Damage properties
    constexpr uint32 FIRE_DAMAGE_INTERVAL_MS = 2000; // 2 seconds
    constexpr float FIRE_DAMAGE_PCT = 0.10f; // 10% max HP

    // Candle Frenzy properties
    constexpr float FRENZY_DAMAGE_MULT = 2.0f;      // 2x damage
    constexpr float FRENZY_ATTACK_SPEED_MULT = 0.5f; // 50% attack time = 2x speed
    constexpr float FRENZY_MOVE_SPEED_MULT = 1.5f;   // +50% move speed

    // Check intervals
    constexpr uint32 BUFF_CHECK_INTERVAL_MS = 500; // 500ms for responsive buff updates

    // Northshire Valley zone ID
    constexpr uint32 ZONE_NORTHSHIRE = 9;

    // Static map to track player damage timers (GUID -> accumulated time)
    static std::unordered_map<ObjectGuid, uint32> playerFireTimers;

    // Helper: Check if position is inside any fire zone
    bool IsInFireZone(WorldObject* obj)
    {
        if (!obj)
            return false;

        std::list<GameObject*> fires;
        obj->GetGameObjectListWithEntryInGrid(fires, GO_KOBOLD_FIRE, FIRE_RADIUS + 5.0f);

        for (GameObject* fire : fires)
        {
            if (obj->GetDistance(fire) <= FIRE_RADIUS)
                return true;
        }
        return false;
    }
}

using namespace NightmareNorthshire;

/*
 * Kobold AI - Handles death fire spawn and Candle Frenzy buff
 */
struct npc_nightmare_kobold : public ScriptedAI
{
    npc_nightmare_kobold(Creature* creature) : ScriptedAI(creature)
    {
        _buffCheckTimer = BUFF_CHECK_INTERVAL_MS;
        _hasCandieFrenzy = false;
        _originalMoveSpeed = creature->GetSpeed(MOVE_RUN);
        _originalAttackTime = creature->GetAttackTime(BASE_ATTACK);
    }

    void Reset() override
    {
        _hasCandieFrenzy = false;
        _buffCheckTimer = BUFF_CHECK_INTERVAL_MS;

        // Restore original stats
        me->SetSpeed(MOVE_RUN, _originalMoveSpeed);
        me->SetAttackTime(BASE_ATTACK, _originalAttackTime);
    }

    void JustDied(Unit* /*killer*/) override
    {
        // Spawn fire at death location
        me->SummonGameObject(
            GO_KOBOLD_FIRE,
            me->GetPositionX(),
            me->GetPositionY(),
            me->GetPositionZ(),
            me->GetOrientation(),
            0.0f, 0.0f, 0.0f, 0.0f,
            FIRE_DURATION_SEC
        );
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damagetype*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        // Candle Frenzy doesn't affect damage taken, only damage dealt
    }

    uint32 GetModifiedDamage(uint32 baseDamage)
    {
        if (_hasCandieFrenzy)
            return static_cast<uint32>(baseDamage * FRENZY_DAMAGE_MULT);
        return baseDamage;
    }

    void UpdateAI(uint32 diff) override
    {
        // Periodically check for Candle Frenzy
        if (_buffCheckTimer <= diff)
        {
            _buffCheckTimer = BUFF_CHECK_INTERVAL_MS;

            bool inFire = IsInFireZone(me);

            if (inFire && !_hasCandieFrenzy)
            {
                // Apply Candle Frenzy
                _hasCandieFrenzy = true;

                // Increase movement speed by 50%
                me->SetSpeed(MOVE_RUN, _originalMoveSpeed * FRENZY_MOVE_SPEED_MULT);

                // Reduce attack time by 50% (2x attack speed)
                me->SetAttackTime(BASE_ATTACK, static_cast<uint32>(_originalAttackTime * FRENZY_ATTACK_SPEED_MULT));
            }
            else if (!inFire && _hasCandieFrenzy)
            {
                // Remove Candle Frenzy
                _hasCandieFrenzy = false;

                // Restore original stats
                me->SetSpeed(MOVE_RUN, _originalMoveSpeed);
                me->SetAttackTime(BASE_ATTACK, _originalAttackTime);
            }
        }
        else
        {
            _buffCheckTimer -= diff;
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    uint32 _buffCheckTimer;
    bool _hasCandieFrenzy;
    float _originalMoveSpeed;
    uint32 _originalAttackTime;
};

/*
 * Player Script - Handles fire damage to players in Northshire
 */
class nightmare_player_fire_damage : public PlayerScript
{
public:
    nightmare_player_fire_damage() : PlayerScript("nightmare_player_fire_damage") { }

    void OnUpdate(Player* player, uint32 diff) override
    {
        // Only process in Northshire
        if (player->GetZoneId() != ZONE_NORTHSHIRE)
            return;

        // Don't damage dead players
        if (!player->IsAlive())
            return;

        // Get or create timer for this player
        ObjectGuid guid = player->GetGUID();
        uint32& timer = playerFireTimers[guid];
        timer += diff;

        if (timer >= FIRE_DAMAGE_INTERVAL_MS)
        {
            timer = 0;

            // Check if player is in any fire zone
            if (IsInFireZone(player))
            {
                // Deal 10% max HP as fire damage
                uint32 damage = static_cast<uint32>(player->GetMaxHealth() * FIRE_DAMAGE_PCT);
                if (damage < 1)
                    damage = 1;

                player->EnvironmentalDamage(DAMAGE_FIRE, damage);
            }
        }
    }

    void OnLogout(Player* player) override
    {
        // Clean up timer when player logs out
        playerFireTimers.erase(player->GetGUID());
    }
};

/*
 * Script registration
 */
void AddSC_nightmare_northshire()
{
    RegisterCreatureAI(npc_nightmare_kobold);
    new nightmare_player_fire_damage();
}
```

**Step 2: Verify file syntax**

```bash
head -50 src/server/scripts/Custom/nightmare_northshire.cpp
```

**Step 3: Commit**

```bash
git add src/server/scripts/Custom/nightmare_northshire.cpp
git commit -m "feat(Scripts): add nightmare kobold fire mechanics script"
```

---

## Task 3: Create CMakeLists.txt for Custom Scripts

**Files:**
- Create: `src/server/scripts/Custom/CMakeLists.txt`

**Step 1: Create CMakeLists.txt following AzerothCore conventions**

```cmake
# Nightmare Northshire custom scripts

set(scripts_STAT_SRCS
  ${scripts_STAT_SRCS}
  ${AC_SCRIPTS_DIR}/Custom/nightmare_northshire.cpp
)

AC_ADD_SCRIPT_LOADER("Custom" "ScriptLoader.h")

message("  -> Prepared: Nightmare Northshire scripts")
```

**Step 2: Commit**

```bash
git add src/server/scripts/Custom/CMakeLists.txt
git commit -m "build(Scripts): add CMakeLists.txt for nightmare northshire"
```

---

## Task 4: Update Custom Script Loader

**Files:**
- Modify: `src/server/scripts/Custom/custom_script_loader.cpp`

**Step 1: Add script registration function**

The file currently has an empty `AddCustomScripts()` function. Add our script.

```cpp
/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Nightmare Northshire
void AddSC_nightmare_northshire();

// The name of this function should match:
// void Add${NameOfDirectory}Scripts()
void AddCustomScripts()
{
    AddSC_nightmare_northshire();
}
```

**Step 2: Commit**

```bash
git add src/server/scripts/Custom/custom_script_loader.cpp
git commit -m "feat(Scripts): register nightmare northshire in custom script loader"
```

---

## Task 5: Link Kobold Entries to Custom Script

**Files:**
- Create: `data/sql/updates/pending_db_world/nightmare_kobold_script_link.sql`

**Step 1: Update creature_template to use nightmare script**

```sql
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
```

**Step 2: Commit**

```bash
git add data/sql/updates/pending_db_world/nightmare_kobold_script_link.sql
git commit -m "feat(DB): link kobolds to nightmare script and increase base stats"
```

---

## Task 6: Build and Verify Compilation

**Files:**
- None (build verification)

**Step 1: Create build directory and configure**

```bash
cd /home/hhess/dev/azerothcore
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/azeroth-server \
         -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DSCRIPTS=static \
         -DMODULES=static
```

Expected output should include:
```
  -> Prepared: Nightmare Northshire scripts
```

**Step 2: Build the project**

```bash
make -j$(nproc)
```

Expected: Build completes without errors.

**Step 3: If build fails, check for issues**

Common issues:
- Missing includes: Add required headers
- Undefined functions: Check AzerothCore API
- Linker errors: Check CMakeLists.txt

**Step 4: Commit any fixes**

```bash
git add -A
git commit -m "fix(Scripts): resolve compilation issues"
```

---

## Task 7: Create Final Integration Commit

**Files:**
- All files from previous tasks

**Step 1: Verify all files are present**

```bash
cd /home/hhess/dev/azerothcore
ls -la data/sql/updates/pending_db_world/nightmare_*.sql
ls -la src/server/scripts/Custom/nightmare_northshire.cpp
ls -la src/server/scripts/Custom/CMakeLists.txt
cat src/server/scripts/Custom/custom_script_loader.cpp
```

**Step 2: Create summary commit if not already done**

```bash
git log --oneline -5
```

If changes are spread across multiple commits, optionally squash or just verify they're all present.

---

## Testing Checklist (In-Game)

After building, installing, and running the server with a fresh database import:

**Fire Spawn:**
1. [ ] Create human character, travel to Northshire
2. [ ] Kill a Kobold Vermin
3. [ ] Verify fire visual spawns at death location
4. [ ] Verify fire appears to be roughly 8 yards in radius (visual check)

**Fire Damage:**
5. [ ] Stand in the fire zone
6. [ ] Verify damage messages appear every ~2 seconds
7. [ ] Verify damage is approximately 10% of max HP
8. [ ] Step out of fire, verify damage stops

**Candle Frenzy:**
9. [ ] Aggro a kobold and kite it into a fire zone
10. [ ] Verify kobold moves faster (visually noticeable)
11. [ ] Verify kobold attacks faster (watch attack animations)
12. [ ] Verify kobold hits harder (check combat log)
13. [ ] Kite kobold out of fire
14. [ ] Verify speed/attack speed return to normal

**Duration:**
15. [ ] Note the time when fire spawns
16. [ ] Verify fire despawns after 10 minutes

**Edge Cases:**
17. [ ] Kill multiple kobolds in same area, verify multiple fires
18. [ ] Die in fire, resurrect, verify timer resets properly
19. [ ] Log out in fire zone, log back in, verify no crashes

---

## GM Test Commands

```
-- Spawn fire manually to test visual:
.gob add 900001

-- Spawn kobold for testing:
.npc add 6

-- Teleport to Northshire Abbey:
.tele northshire

-- Check script is loaded (look for nightmare_northshire):
.lookup creature 6
```

---

## Architecture Notes

**Why no custom spells?**
- `spell_dbc` has 200+ columns with complex relationships
- Easier and safer to handle buffs directly in C++
- More control over buff application/removal timing

**Why static timer map?**
- AzerothCore doesn't have per-player custom data storage API
- Static map is simple and efficient
- Cleaned up on logout to prevent memory leaks

**Why visual-only GameObject?**
- Type 6 (TRAP) requires proper spell setup
- Type 5 (GENERIC) just shows the visual
- All damage logic in C++ gives full control

**Damage calculation:**
- Uses `EnvironmentalDamage(DAMAGE_FIRE, amount)` - native WoW mechanic
- Shows proper "Fire" damage in combat log
- Respects immunities and absorbs

**Candle Frenzy implementation:**
- Tracks original stats on creature spawn
- Modifies `SetSpeed()` and `SetAttackTime()` directly
- Damage multiplier would need combat hook (simplified: base stat increase via SQL)

---

## Known Limitations

1. **Candle Frenzy damage bonus**: The 2x damage multiplier isn't implemented in this version. Would require hooking into damage calculation. The SQL-based +30% damage increase provides baseline difficulty.

2. **Fire visual size**: Size 2.0 is an estimate. May need adjustment after in-game testing.

3. **Zone restriction**: Only works in Northshire (zone 9). Expand `ZONE_NORTHSHIRE` check for other zones.

4. **No buff icon**: Candle Frenzy doesn't show a buff icon on the kobold. Would require applying an actual aura spell.
