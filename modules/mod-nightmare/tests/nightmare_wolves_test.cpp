#include "nightmare_wolves_logic.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace NightmareWolves::Rules;

TEST(NightmareWolfInfection, StacksReachTenButCannotExceedIt)
{
    std::uint8_t stacks = 0;
    for (int i = 0; i < 20; ++i)
        stacks = AddInfectionStack(stacks, 10);

    EXPECT_EQ(stacks, 10u);
}

TEST(NightmareWolfInfection, DamageTakenIncreasesTenPercentPerStack)
{
    EXPECT_EQ(IncreasedDamage(100, 0, 0.10f), 100u);
    EXPECT_EQ(IncreasedDamage(100, 1, 0.10f), 110u);
    EXPECT_EQ(IncreasedDamage(100, 5, 0.10f), 150u);
    EXPECT_EQ(IncreasedDamage(100, 10, 0.10f), 200u);
    EXPECT_EQ(IncreasedDamage(std::numeric_limits<std::uint32_t>::max(), 10, 0.10f),
        std::numeric_limits<std::uint32_t>::max());
}

TEST(NightmareWolfInfection, SlowBecomesRootAtTenStacks)
{
    EXPECT_FLOAT_EQ(MovementMultiplier(0, 0.10f), 1.0f);
    EXPECT_FLOAT_EQ(MovementMultiplier(1, 0.10f), 0.9f);
    EXPECT_FLOAT_EQ(MovementMultiplier(5, 0.10f), 0.5f);
    EXPECT_FLOAT_EQ(MovementMultiplier(10, 0.10f), 0.0f);
}

TEST(NightmareWolfInfection, ExpiryIsTenSecondsSinceLastBite)
{
    EXPECT_FALSE(InfectionExpired(9000, 999, 10000));
    EXPECT_TRUE(InfectionExpired(9000, 1000, 10000));
    EXPECT_TRUE(InfectionExpired(std::numeric_limits<std::uint32_t>::max(), 1, 10000));
    EXPECT_FALSE(InfectionExpired(0, 9999, 10000)); // new bite resets elapsed time
}

TEST(NightmareWolfHowl, FirstHowlAtFiftyPercentOnlyWhenEngaged)
{
    EXPECT_FALSE(ShouldHowl(51, 100, 50.0f, false, true));
    EXPECT_TRUE(ShouldHowl(50, 100, 50.0f, false, true));
    EXPECT_TRUE(ShouldHowl(49, 100, 50.0f, false, true));
    EXPECT_FALSE(ShouldHowl(49, 100, 50.0f, true, true));
    EXPECT_FALSE(ShouldHowl(49, 100, 50.0f, false, false));
    EXPECT_FALSE(ShouldHowl(0, 100, 50.0f, false, true));
}

TEST(NightmareWolfHowl, RangeChecksDistanceAndFloorSeparation)
{
    EXPECT_TRUE(InHowlRange(900.0f, 5.0f, 30.0f, 5.0f));
    EXPECT_FALSE(InHowlRange(900.1f, 0.0f, 30.0f, 5.0f));
    EXPECT_FALSE(InHowlRange(100.0f, 5.1f, 30.0f, 5.0f));
}
