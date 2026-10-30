/*
 * Nightmare Northshire - Diseased Wolf Packs
 *
 * Successful wolf melee hits stack Infected Bite on players. Each stack slows
 * movement and increases damage taken from every source. Wounded wolves howl
 * once per combat, recruiting all eligible wolves within thirty yards.
 */

#include "Chat.h"
#include "Config.h"
#include "DataMap.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "nightmare_wolves_logic.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <list>
#include <type_traits>

namespace NightmareWolves
{
    constexpr uint32 NPC_DISEASED_TIMBER_WOLF = 69;
    constexpr uint32 NPC_DISEASED_YOUNG_WOLF = 299;
    constexpr char INFECTION_KEY[] = "NightmareWolves.InfectedBite";
    constexpr float HOWL_HEIGHT_TOLERANCE = 5.0f;

    enum Events : uint32
    {
        EVENT_CALL_PACK = 1
    };

    struct Settings
    {
        uint8 maxStacks;
        uint32 infectionDurationMs;
        float damageBonusPerStack;
        float slowPerStack;
        float howlHealthPercent;
        uint32 howlWarningMs;
        float howlRadius;
    };

    template<typename T>
    T GetSetting(char const* name, T fallback, T minimum, T maximum)
    {
        T value = sConfigMgr->GetOption<T>(name, fallback, false);
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value))
                return fallback;

        return std::clamp(value, minimum, maximum);
    }

    Settings const& GetSettings()
    {
        static Settings const settings = {
            uint8(GetSetting("Nightmare.WolfMaxStacks", int32(10), int32(1), int32(10))),
            uint32(GetSetting("Nightmare.WolfInfectionDurationMs", int32(10000), int32(1000), int32(60000))),
            GetSetting("Nightmare.WolfDamageBonusPerStack", 0.10f, 0.01f, 1.0f),
            GetSetting("Nightmare.WolfSlowPerStack", 0.10f, 0.01f, 0.10f),
            GetSetting("Nightmare.WolfHowlHealthPercent", 50.0f, 1.0f, 99.0f),
            uint32(GetSetting("Nightmare.WolfHowlWarningMs", int32(2000), int32(100), int32(10000))),
            GetSetting("Nightmare.WolfHowlRadius", 30.0f, 1.0f, 60.0f)
        };
        return settings;
    }

    constexpr std::array<UnitMoveType, 7> MOVE_TYPES = {
        MOVE_WALK, MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM, MOVE_SWIM_BACK, MOVE_FLIGHT, MOVE_FLIGHT_BACK
    };

    struct Infection : DataMap::Base
    {
        uint8 stacks = 0;
        uint32 elapsedMs = 0;
        uint32 movementCheckMs = 0;
        bool ownsRoot = false;
        std::array<float, MOVE_TYPES.size()> appliedRates{};
    };

    void RefreshMovement(Player* player, Infection& infection, bool force)
    {
        float multiplier = Rules::MovementMultiplier(infection.stacks, GetSettings().slowPerStack);

        if (multiplier <= 0.0f && !player->HasUnitState(UNIT_STATE_ROOT))
        {
            player->SetControlled(true, UNIT_STATE_ROOT);
            infection.ownsRoot = true;
        }
        else if (multiplier > 0.0f && infection.ownsRoot)
        {
            player->SetControlled(false, UNIT_STATE_ROOT);
            infection.ownsRoot = false;
        }

        for (std::size_t i = 0; i < MOVE_TYPES.size(); ++i)
        {
            UnitMoveType type = MOVE_TYPES[i];
            if (!force && std::abs(player->GetSpeedRate(type) - infection.appliedRates[i]) < 0.0001f)
                continue;

            // Recalculate from current auras before applying Nightmare's slow.
            // Other speed buffs and debuffs must not be overwritten on expiry.
            player->UpdateSpeed(type, true);
            if (multiplier > 0.0f)
                player->SetSpeed(type, player->GetSpeedRate(type) * multiplier, true);

            infection.appliedRates[i] = player->GetSpeedRate(type);
        }
    }

    void ClearInfection(Player* player, bool notifyClient = true)
    {
        Infection* infection = player->CustomData.Get<Infection>(INFECTION_KEY);
        if (!infection)
            return;

        if (infection->ownsRoot)
            player->SetControlled(false, UNIT_STATE_ROOT);

        for (UnitMoveType type : MOVE_TYPES)
            player->UpdateSpeed(type, notifyClient);

        player->CustomData.Erase(INFECTION_KEY);
    }

    void ApplyInfectedBite(Player* player)
    {
        Infection* infection = player->CustomData.GetDefault<Infection>(INFECTION_KEY);
        infection->stacks = Rules::AddInfectionStack(infection->stacks, GetSettings().maxStacks);
        infection->elapsedMs = 0;
        RefreshMovement(player, *infection, true);

        uint32 damagePercent = uint32(infection->stacks * GetSettings().damageBonusPerStack * 100.0f);
        uint32 slowPercent = uint32(infection->stacks * GetSettings().slowPerStack * 100.0f);
        ChatHandler(player->GetSession()).PSendSysMessage(
            "Infected Bite: {}/{} stacks (+{}% damage taken, {}% movement slow).",
            infection->stacks, GetSettings().maxStacks, damagePercent, slowPercent);
    }

    void RecruitWolves(Creature* caller, Unit* target)
    {
        Settings const& settings = GetSettings();
        for (uint32 entry : {NPC_DISEASED_YOUNG_WOLF, NPC_DISEASED_TIMBER_WOLF})
        {
            std::list<Creature*> nearby;
            caller->GetCreatureListWithEntryInGrid(nearby, entry, settings.howlRadius);

            for (Creature* wolf : nearby)
            {
                if (wolf == caller || !wolf->IsAlive() || wolf->IsInCombat() || wolf->IsInEvadeMode() ||
                    !wolf->IsAIEnabled || !wolf->IsWithinLOSInMap(caller) || !wolf->IsWithinLOSInMap(target) ||
                    !Rules::InHowlRange(caller->GetExactDist2dSq(wolf),
                        caller->GetPositionZ() - wolf->GetPositionZ(),
                        settings.howlRadius, HOWL_HEIGHT_TOLERANCE))
                    continue;

                wolf->AI()->AttackStart(target);
            }
        }
    }
}

using namespace NightmareWolves;

struct npc_nightmare_wolf : public ScriptedAI
{
    npc_nightmare_wolf(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        _hasHowled = false;
        events.Reset();
    }

    void DamageDealt(Unit* victim, uint32& damage, DamageEffectType damageType,
        SpellSchoolMask /*damageSchoolMask*/) override
    {
        if (damageType == DIRECT_DAMAGE && damage && victim && victim->IsPlayer())
            ApplyInfectedBite(victim->ToPlayer());
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (Rules::ShouldHowl(me->GetHealth(), me->GetMaxHealth(), GetSettings().howlHealthPercent,
            _hasHowled, me->IsInCombat()))
        {
            _hasHowled = true;
            me->TextEmote("The diseased wolf howls for its pack!");
            events.ScheduleEvent(EVENT_CALL_PACK, std::chrono::milliseconds(GetSettings().howlWarningMs));
        }

        events.Update(diff);
        if (events.ExecuteEvent() == EVENT_CALL_PACK && me->IsAlive() && me->GetVictim())
            RecruitWolves(me, me->GetVictim());

        DoMeleeAttackIfReady();
    }

private:
    bool _hasHowled = false;
};

class nightmare_wolf_damage : public UnitScript
{
public:
    nightmare_wolf_damage() : UnitScript("nightmare_wolf_damage") { }

    uint32 DealDamage(Unit* /*attacker*/, Unit* victim, uint32 damage,
        DamageEffectType /*damageType*/) override
    {
        if (!damage || !victim || !victim->IsPlayer())
            return damage;

        if (Infection* infection = victim->ToPlayer()->CustomData.Get<Infection>(INFECTION_KEY))
            return Rules::IncreasedDamage(damage, infection->stacks, GetSettings().damageBonusPerStack);

        return damage;
    }
};

class nightmare_wolf_player : public PlayerScript
{
public:
    nightmare_wolf_player() : PlayerScript("nightmare_wolf_player") { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        Infection* infection = player->CustomData.Get<Infection>(INFECTION_KEY);
        if (!infection)
            return;

        if (!player->IsAlive() ||
            Rules::InfectionExpired(infection->elapsedMs, diff, GetSettings().infectionDurationMs))
        {
            ClearInfection(player);
            return;
        }

        infection->elapsedMs += diff;
        infection->movementCheckMs += diff;
        if (infection->movementCheckMs >= 500)
        {
            infection->movementCheckMs = 0;
            RefreshMovement(player, *infection, false);
        }
    }

    void OnPlayerJustDied(Player* player) override
    {
        ClearInfection(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        ClearInfection(player, false);
    }
};

void AddSC_nightmare_wolves()
{
    RegisterCreatureAI(npc_nightmare_wolf);
    new nightmare_wolf_damage();
    new nightmare_wolf_player();
}
