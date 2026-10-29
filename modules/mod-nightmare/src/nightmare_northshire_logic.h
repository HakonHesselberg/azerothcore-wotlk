/*
 * Nightmare Northshire gameplay calculations and reversible Candle Frenzy.
 * Kept separate from the scripts so they can be regression tested.
 */

#ifndef NIGHTMARE_NORTHSHIRE_LOGIC_H
#define NIGHTMARE_NORTHSHIRE_LOGIC_H

#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace NightmareNorthshire::Rules
{
    enum class FleeAction
    {
        None,
        Start,
        Stop
    };

    // The movement adapter supplies ownership separately from permission to start:
    // an external fear/confusion must never be removed by Nightmare cleanup.
    inline FleeAction DecideFlee(uint32 health, uint32 maxHealth, float thresholdPercent,
        bool frenzy, bool engaged, bool ownsMovement, bool canStart, bool feared)
    {
        bool shouldFlee = health && maxHealth && engaged && !frenzy && !feared &&
            double(health) * 100.0 <= double(maxHealth) * thresholdPercent;
        if (ownsMovement)
            return shouldFlee ? FleeAction::None : FleeAction::Stop;

        return shouldFlee && canStart ? FleeAction::Start : FleeAction::None;
    }

    inline bool IsWithinFire(float horizontalDistanceSquared, float heightDifference,
        float radius, float heightTolerance)
    {
        return horizontalDistanceSquared <= radius * radius &&
            std::abs(heightDifference) <= heightTolerance;
    }

    inline bool HasFireCapacity(uint32 nearbyCount, uint32 maxNearbyFires)
    {
        return nearbyCount < maxNearbyFires;
    }

    inline uint32 FireDamage(uint32 maxHealth, float fraction)
    {
        double scaled = double(maxHealth) * fraction;
        return uint32(std::clamp(scaled, 1.0, double(std::numeric_limits<uint32>::max())));
    }

    inline uint32 FrenzyDamage(uint32 damage, float multiplier)
    {
        double scaled = double(damage) * multiplier;
        return uint32(std::min(scaled, double(std::numeric_limits<uint32>::max())));
    }

    // One damage tick at most per server update, even after a long pause.
    // The remainder is preserved, and the caller resets exposure on exit.
    inline bool AdvanceExposure(uint32& elapsedMs, uint32 diffMs, uint32 intervalMs)
    {
        if (!intervalMs)
            return false;

        uint64 total = uint64(elapsedMs) + diffMs;
        elapsedMs = uint32(total % intervalMs);
        return total >= intervalMs;
    }

    // ApplyAttackTimePercentMod expects a haste percentage: +100% haste halves
    // the attack period. Its inverse removal composes with other haste effects.
    inline float FrenzyHastePercent(float attackTimeMultiplier)
    {
        return (1.0f / attackTimeMultiplier - 1.0f) * 100.0f;
    }

    class CandleFrenzy
    {
    public:
        [[nodiscard]] bool Active() const { return _active; }

        template<class UnitType>
        void SetActive(UnitType& unit, bool active, float attackTimeMultiplier, float moveSpeedMultiplier)
        {
            if (_active == active)
                return;

            unit.ApplyAttackTimePercentMod(BASE_ATTACK, FrenzyHastePercent(attackTimeMultiplier), active);
            _active = active;
            RefreshMovement(unit, moveSpeedMultiplier);
        }

        template<class UnitType>
        void RefreshMovement(UnitType& unit, float moveSpeedMultiplier) const
        {
            // Rebuild the baseline from the creature template and *current* auras,
            // slows and health effects. Never restore a saved run-speed snapshot.
            unit.UpdateSpeed(MOVE_RUN, false);
            if (_active)
                unit.SetSpeed(MOVE_RUN, unit.GetSpeedRate(MOVE_RUN) * moveSpeedMultiplier);
        }

    private:
        bool _active = false;
    };
}

#endif
