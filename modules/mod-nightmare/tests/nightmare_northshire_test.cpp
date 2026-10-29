#include "nightmare_northshire_logic.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace NightmareNorthshire::Rules;

namespace
{
    // The controller uses the same public Unit methods and enum types as the
    // production script. This lightweight stand-in lets tests change unrelated
    // movement and haste modifiers without requiring a live world/map.
    struct SimulatedUnit
    {
        float baseRunRate = 1.0f;
        float otherSpeedFactor = 1.0f;
        float runRate = 1.0f;
        float otherHasteFactor = 1.0f;
        float frenzyHasteFactor = 1.0f;
        uint32 recalculations = 0;
        uint32 attackModifications = 0;

        void UpdateSpeed(UnitMoveType type, bool)
        {
            EXPECT_EQ(type, MOVE_RUN);
            ++recalculations;
            runRate = baseRunRate * otherSpeedFactor;
        }

        float GetSpeedRate(UnitMoveType type) const
        {
            EXPECT_EQ(type, MOVE_RUN);
            return runRate;
        }

        void SetSpeed(UnitMoveType type, float rate)
        {
            EXPECT_EQ(type, MOVE_RUN);
            runRate = rate;
        }

        void ApplyAttackTimePercentMod(WeaponAttackType type, float hastePercent, bool apply)
        {
            EXPECT_EQ(type, BASE_ATTACK);
            ++attackModifications;
            float factor = 1.0f + hastePercent / 100.0f;
            frenzyHasteFactor *= apply ? factor : 1.0f / factor;
        }

        float AttackPeriod() const
        {
            return 2000.0f / (otherHasteFactor * frenzyHasteFactor);
        }
    };
}

TEST(NightmareFire, DefaultDamageTenPercent)
{
    EXPECT_EQ(FireDamage(5000, 0.10f), 500u);
    EXPECT_EQ(FireDamage(1001, 0.10f), 100u);
}

TEST(NightmareFire, DamageHasMinimumOfOne)
{
    EXPECT_EQ(FireDamage(1, 0.01f), 1u);
    EXPECT_EQ(FireDamage(0, 0.10f), 1u);
}

TEST(NightmareFire, DamageClampedToUInt32)
{
    EXPECT_EQ(FireDamage(std::numeric_limits<uint32>::max(), 1.0f),
        std::numeric_limits<uint32>::max());
}

TEST(NightmareFire, FrenzyDamageMultiplierAndOverflow)
{
    EXPECT_EQ(FrenzyDamage(100, 2.0f), 200u);
    EXPECT_EQ(FrenzyDamage(std::numeric_limits<uint32>::max(), 10.0f),
        std::numeric_limits<uint32>::max());
}

TEST(NightmareExposure, AccumulatesUntilFullInterval)
{
    uint32 elapsed = 0;
    EXPECT_FALSE(AdvanceExposure(elapsed, 999, 2000));
    EXPECT_FALSE(AdvanceExposure(elapsed, 1000, 2000));
    EXPECT_TRUE(AdvanceExposure(elapsed, 1, 2000));
    EXPECT_EQ(elapsed, 0u);
}

TEST(NightmareExposure, LargeUpdateTicksOnlyOnceAndKeepsRemainder)
{
    uint32 elapsed = 1500;
    EXPECT_TRUE(AdvanceExposure(elapsed, 5300, 2000));
    EXPECT_EQ(elapsed, 800u);
    EXPECT_FALSE(AdvanceExposure(elapsed, 1199, 2000));
    EXPECT_TRUE(AdvanceExposure(elapsed, 1, 2000));
}

TEST(NightmareExposure, ExitAndReentryResetsExposure)
{
    uint32 elapsed = 1500;
    elapsed = 0; // player leaves fire; production erases custom data
    EXPECT_FALSE(AdvanceExposure(elapsed, 1500, 2000));
    EXPECT_EQ(elapsed, 1500u);
    EXPECT_FALSE(AdvanceExposure(elapsed, 10, 0)); // defensive invalid interval
}

TEST(NightmareFire, RadiusIncludesBoundaryButNotOutside)
{
    EXPECT_TRUE(IsWithinFire(64.0f, 0.0f, 8.0f, 3.0f));
    EXPECT_FALSE(IsWithinFire(64.01f, 0.0f, 8.0f, 3.0f));
}

TEST(NightmareFire, HeightToleranceIncludesBothDirections)
{
    EXPECT_TRUE(IsWithinFire(4.0f, -3.0f, 8.0f, 3.0f));
    EXPECT_TRUE(IsWithinFire(4.0f, 3.0f, 8.0f, 3.0f));
    EXPECT_FALSE(IsWithinFire(4.0f, -3.1f, 8.0f, 3.0f));
}

TEST(NightmareFire, SpawnCapIsExclusive)
{
    EXPECT_TRUE(HasFireCapacity(0, 5));
    EXPECT_TRUE(HasFireCapacity(4, 5));
    EXPECT_FALSE(HasFireCapacity(5, 5));
    EXPECT_FALSE(HasFireCapacity(6, 5));
}

TEST(NightmareFrenzy, ActivationIsIdempotent)
{
    SimulatedUnit unit;
    CandleFrenzy frenzy;
    frenzy.SetActive(unit, true, 0.5f, 1.5f);
    frenzy.SetActive(unit, true, 0.5f, 1.5f);
    EXPECT_TRUE(frenzy.Active());
    EXPECT_EQ(unit.attackModifications, 1u);
    EXPECT_FLOAT_EQ(unit.AttackPeriod(), 1000.0f);
    EXPECT_FLOAT_EQ(unit.runRate, 1.5f);
}

TEST(NightmareFrenzy, RemovalAndRepeatedCleanupRestoreCurrentBaseline)
{
    SimulatedUnit unit;
    CandleFrenzy frenzy;
    frenzy.SetActive(unit, true, 0.5f, 1.5f);
    unit.baseRunRate = 1.2f;
    frenzy.SetActive(unit, false, 0.5f, 1.5f);
    frenzy.SetActive(unit, false, 0.5f, 1.5f);
    EXPECT_FALSE(frenzy.Active());
    EXPECT_EQ(unit.attackModifications, 2u);
    EXPECT_FLOAT_EQ(unit.runRate, 1.2f);
    EXPECT_FLOAT_EQ(unit.AttackPeriod(), 2000.0f);
}

TEST(NightmareFrenzy, AttackSpeedComposesWithOtherHasteAndSlow)
{
    SimulatedUnit unit;
    CandleFrenzy frenzy;
    unit.otherHasteFactor = 1.25f;
    frenzy.SetActive(unit, true, 0.5f, 1.5f);
    EXPECT_FLOAT_EQ(unit.AttackPeriod(), 800.0f);
    unit.otherHasteFactor = 0.8f;
    EXPECT_FLOAT_EQ(unit.AttackPeriod(), 1250.0f);
    frenzy.SetActive(unit, false, 0.5f, 1.5f);
    EXPECT_FLOAT_EQ(unit.AttackPeriod(), 2500.0f);
}

TEST(NightmareFrenzy, MovementRecalculatesChangingBuffsAndSlows)
{
    SimulatedUnit unit;
    CandleFrenzy frenzy;
    frenzy.SetActive(unit, true, 0.5f, 1.5f);
    unit.otherSpeedFactor = 1.3f;
    frenzy.RefreshMovement(unit, 1.5f);
    EXPECT_FLOAT_EQ(unit.runRate, 1.95f);
    unit.otherSpeedFactor = 0.5f;
    frenzy.RefreshMovement(unit, 1.5f);
    EXPECT_FLOAT_EQ(unit.runRate, 0.75f);
    frenzy.SetActive(unit, false, 0.5f, 1.5f);
    EXPECT_FLOAT_EQ(unit.runRate, 0.5f);
    EXPECT_EQ(unit.recalculations, 4u);
}
