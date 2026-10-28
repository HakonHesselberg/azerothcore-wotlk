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
        _hasCandleFrenzy = false;
        _originalMoveSpeed = creature->GetSpeed(MOVE_RUN);
        _originalAttackTime = creature->GetAttackTime(BASE_ATTACK);
    }

    void Reset() override
    {
        _hasCandleFrenzy = false;
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
        if (_hasCandleFrenzy)
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

            if (inFire && !_hasCandleFrenzy)
            {
                // Apply Candle Frenzy
                _hasCandleFrenzy = true;

                // Increase movement speed by 50%
                me->SetSpeed(MOVE_RUN, _originalMoveSpeed * FRENZY_MOVE_SPEED_MULT);

                // Reduce attack time by 50% (2x attack speed)
                me->SetAttackTime(BASE_ATTACK, static_cast<uint32>(_originalAttackTime * FRENZY_ATTACK_SPEED_MULT));
            }
            else if (!inFire && _hasCandleFrenzy)
            {
                // Remove Candle Frenzy
                _hasCandleFrenzy = false;

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
    bool _hasCandleFrenzy;
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
