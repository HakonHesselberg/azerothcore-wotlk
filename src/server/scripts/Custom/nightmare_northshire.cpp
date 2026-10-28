/*
 * Nightmare Northshire - Kobold Fire Mechanics
 *
 * When kobolds die, they leave burning fire zones.
 * - Players in fire take 10% max HP damage every 2 seconds
 * - Kobolds in fire gain Candle Frenzy: 2x damage, 2x attack speed, +50% move speed
 */

#include "Config.h"
#include "DataMap.h"
#include "GameObject.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <limits>
#include <type_traits>

namespace NightmareNorthshire
{
    constexpr uint32 GO_KOBOLD_FIRE = 900001;
    constexpr uint32 ZONE_ELWYNN_FOREST = 12;
    constexpr char FIRE_EXPOSURE_KEY[] = "NightmareNorthshire.FireExposure";

    enum Events : uint32
    {
        EVENT_CHECK_FIRE = 1
    };

    struct Settings
    {
        float fireRadius;
        float fireHeightTolerance;
        uint32 fireDurationSeconds;
        uint32 fireTickMs;
        float fireDamagePercent;
        float frenzyDamageMultiplier;
        float frenzyAttackTimeMultiplier;
        float frenzyMoveSpeedMultiplier;
        uint32 maxNearbyFires;
    };

    template<typename T>
    T GetSetting(char const* name, T fallback, T minimum, T maximum)
    {
        // Missing keys are expected because all Nightmare settings are optional.
        T value = sConfigMgr->GetOption<T>(name, fallback, false);
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value))
                return fallback;

        return std::clamp(value, minimum, maximum);
    }

    Settings const& GetSettings()
    {
        // Settings are loaded on first use; changes require a worldserver restart.
        static Settings const settings = {
            GetSetting("Nightmare.FireRadius", 8.0f, 1.0f, 40.0f),
            GetSetting("Nightmare.FireHeightTolerance", 3.0f, 0.5f, 10.0f),
            uint32(GetSetting("Nightmare.FireDurationSeconds", int32(600), int32(1), int32(3600))),
            uint32(GetSetting("Nightmare.FireTickMs", int32(2000), int32(100), int32(60000))),
            GetSetting("Nightmare.FireDamagePercent", 0.10f, 0.01f, 1.0f),
            GetSetting("Nightmare.FrenzyDamageMultiplier", 2.0f, 1.0f, 10.0f),
            GetSetting("Nightmare.FrenzyAttackTimeMultiplier", 0.5f, 0.1f, 1.0f),
            GetSetting("Nightmare.FrenzyMoveSpeedMultiplier", 1.5f, 1.0f, 3.0f),
            uint32(GetSetting("Nightmare.MaxNearbyFires", int32(5), int32(1), int32(50)))
        };
        return settings;
    }

    struct FireExposure : DataMap::Base
    {
        uint32 elapsedMs = 0;
    };

    bool IsNearFire(WorldObject const* obj, GameObject const* fire, float radius)
    {
        return obj->GetExactDist2dSq(fire) <= radius * radius &&
            std::abs(obj->GetPositionZ() - fire->GetPositionZ()) <= GetSettings().fireHeightTolerance;
    }

    bool IsInFireZone(WorldObject* obj)
    {
        if (!obj)
            return false;

        float radius = GetSettings().fireRadius;
        std::list<GameObject*> fires;
        obj->GetGameObjectListWithEntryInGrid(fires, GO_KOBOLD_FIRE, radius + 5.0f);

        for (GameObject* fire : fires)
            if (IsNearFire(obj, fire, radius))
                return true;

        return false;
    }

    bool CanSpawnAnotherFire(Creature* creature)
    {
        float radius = GetSettings().fireRadius * 2.0f;
        std::list<GameObject*> fires;
        creature->GetGameObjectListWithEntryInGrid(fires, GO_KOBOLD_FIRE, radius + 5.0f);

        uint32 nearbyFires = 0;
        for (GameObject* fire : fires)
            if (IsNearFire(creature, fire, radius) && ++nearbyFires >= GetSettings().maxNearbyFires)
                return false;

        return true;
    }
}

using namespace NightmareNorthshire;

/*
 * Kobold AI - Handles death fire spawn and Candle Frenzy buff
 */
struct npc_nightmare_kobold : public ScriptedAI
{
    npc_nightmare_kobold(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        if (_hasCandleFrenzy)
        {
            me->SetSpeed(MOVE_RUN, _originalMoveSpeedRate);
            me->SetAttackTime(BASE_ATTACK, _originalAttackTime);
        }

        _hasCandleFrenzy = false;
        events.Reset();
        events.ScheduleEvent(EVENT_CHECK_FIRE, 500ms);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Preserve the original SmartAI aggro dialogue (creature_text group 0).
        Talk(0);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (!CanSpawnAnotherFire(me))
            return;

        // Spawn fire at death location
        me->SummonGameObject(
            GO_KOBOLD_FIRE,
            me->GetPositionX(),
            me->GetPositionY(),
            me->GetPositionZ(),
            me->GetOrientation(),
            0.0f, 0.0f, 0.0f, 0.0f,
            GetSettings().fireDurationSeconds,
            true,
            GO_SUMMON_TIMED_DESPAWN
        );
    }

    void DamageDealt(Unit* /*victim*/, uint32& damage,
        DamageEffectType /*damageType*/, SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (_hasCandleFrenzy)
        {
            double scaledDamage = double(damage) * GetSettings().frenzyDamageMultiplier;
            damage = uint32(std::min(scaledDamage, double(std::numeric_limits<uint32>::max())));
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);
        if (events.ExecuteEvent() == EVENT_CHECK_FIRE)
        {
            events.ScheduleEvent(EVENT_CHECK_FIRE, 500ms);

            bool inFire = IsInFireZone(me);

            if (inFire && !_hasCandleFrenzy)
            {
                _originalMoveSpeedRate = me->GetSpeedRate(MOVE_RUN);
                _originalAttackTime = me->GetAttackTime(BASE_ATTACK);
                _hasCandleFrenzy = true;

                me->SetSpeed(MOVE_RUN,
                    _originalMoveSpeedRate * GetSettings().frenzyMoveSpeedMultiplier);

                me->SetAttackTime(BASE_ATTACK,
                    uint32(_originalAttackTime * GetSettings().frenzyAttackTimeMultiplier));
            }
            else if (!inFire && _hasCandleFrenzy)
            {
                _hasCandleFrenzy = false;

                me->SetSpeed(MOVE_RUN, _originalMoveSpeedRate);
                me->SetAttackTime(BASE_ATTACK, _originalAttackTime);
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _hasCandleFrenzy = false;
    float _originalMoveSpeedRate = 1.0f;
    uint32 _originalAttackTime = 2000;
};

/*
 * Player Script - Handles fire damage to players in Northshire
 */
class nightmare_player_fire_damage : public PlayerScript
{
public:
    nightmare_player_fire_damage() : PlayerScript("nightmare_player_fire_damage") { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        // Northshire subareas belong to Elwynn Forest (zone 12, not area 9).
        if (player->GetZoneId() != ZONE_ELWYNN_FOREST || !player->IsAlive() ||
            !IsInFireZone(player))
        {
            player->CustomData.Erase(FIRE_EXPOSURE_KEY);
            return;
        }

        FireExposure* exposure = player->CustomData.GetDefault<FireExposure>(FIRE_EXPOSURE_KEY);
        uint32 interval = GetSettings().fireTickMs;
        uint64 elapsed = uint64(exposure->elapsedMs) + diff;

        if (elapsed >= interval)
        {
            // At most one tick per update after a long server pause.
            exposure->elapsedMs = uint32(elapsed % interval);

            double scaledDamage = double(player->GetMaxHealth()) * GetSettings().fireDamagePercent;
            uint32 damage = uint32(std::clamp(scaledDamage, 1.0, double(std::numeric_limits<uint32>::max())));

            player->EnvironmentalDamage(DAMAGE_FIRE, damage);
        }
        else
            exposure->elapsedMs = uint32(elapsed);
    }

    void OnPlayerLogout(Player* player) override
    {
        player->CustomData.Erase(FIRE_EXPOSURE_KEY);
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
