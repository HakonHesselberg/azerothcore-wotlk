/*
 * Nightmare Northshire - pure rules for diseased wolf packs.
 */

#ifndef NIGHTMARE_WOLVES_LOGIC_H
#define NIGHTMARE_WOLVES_LOGIC_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace NightmareWolves::Rules
{
    inline std::uint8_t AddInfectionStack(std::uint8_t current, std::uint8_t maximum)
    {
        return std::min<std::uint8_t>(maximum, current + (current < maximum ? 1 : 0));
    }

    inline float MovementMultiplier(std::uint8_t stacks, float slowPerStack)
    {
        return std::max(0.0f, 1.0f - stacks * slowPerStack);
    }

    inline std::uint32_t IncreasedDamage(std::uint32_t damage, std::uint8_t stacks, float bonusPerStack)
    {
        double scaled = double(damage) * (1.0 + double(stacks) * bonusPerStack);
        return std::uint32_t(std::min(scaled, double(std::numeric_limits<std::uint32_t>::max())));
    }

    inline bool InfectionExpired(std::uint32_t elapsedMs, std::uint32_t diffMs, std::uint32_t durationMs)
    {
        return std::uint64_t(elapsedMs) + diffMs >= durationMs;
    }

    inline bool ShouldHowl(std::uint32_t health, std::uint32_t maxHealth, float thresholdPercent,
        bool alreadyHowled, bool engaged)
    {
        return engaged && !alreadyHowled && health && maxHealth &&
            double(health) * 100.0 <= double(maxHealth) * thresholdPercent;
    }

    inline bool InHowlRange(float horizontalDistanceSquared, float heightDifference,
        float radius, float heightTolerance)
    {
        return horizontalDistanceSquared <= radius * radius &&
            std::abs(heightDifference) <= heightTolerance;
    }
}

#endif
