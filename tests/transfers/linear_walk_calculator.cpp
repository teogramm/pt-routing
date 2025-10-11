#include <gtest/gtest.h>

#include<transfers/linear_walk_calculator.h>

using namespace raptor;

auto point_1 = std::pair{59.15783963140067, 18.383180457016003};
// The distance between point 1 and point 2 is 5 kilometres
auto point_2 = std::pair{59.136848379900925, 18.305591982240376};

TEST(LinearWalkTimeCalculator, DefaultScalingIs1) {
    // Ensure that no scaling happens when we don't specify a factor
    auto walking_speed_kmh = 5.0;
    auto calculator = LinearWalkTimeCalculator{walking_speed_kmh};
    // 5 kilometres at 5 km/h take 1 hour
    EXPECT_EQ(calculator.calculate_walking_time(5).count(), 60*60);
    // When we are dealing with coordinates, we accept some inaccuracy
    EXPECT_NEAR(
            calculator.calculate_walking_time(point_1.first, point_1.second, point_2.first, point_2.second).count(),
            60*60,
            10);
    // 10 kilometres at 5 km/h take 2 hours
    EXPECT_DOUBLE_EQ(calculator.calculate_walking_time(10).count(), 2*60*60);
}

TEST(LinearWalkTimeCalculator, ScalingIsApplied) {
    auto walking_speed_kmh = 5.0;
    auto scaling_factor = 1.5;
    auto calculator = LinearWalkTimeCalculator{walking_speed_kmh, scaling_factor};
    // 5 kilometres at 5 km/h take 1 hour
    EXPECT_EQ(calculator.calculate_walking_time(5).count(), scaling_factor*60*60);
    // When we are dealing with coordinates, we accept some inaccuracy
    EXPECT_NEAR(
        calculator.calculate_walking_time(point_1.first, point_1.second, point_2.first, point_2.second).count(),
        scaling_factor*60*60,
        10);
    // 10 kilometres at 5 km/h take 2 hours
    EXPECT_EQ(calculator.calculate_walking_time(10).count(), scaling_factor*2*60*60);
}

TEST(LinearWalkTimeCalculator, SpeedMustBePositive) {
    auto walking_speed_kmh = 0.0;
    EXPECT_THROW(LinearWalkTimeCalculator{walking_speed_kmh}, std::invalid_argument);
    walking_speed_kmh = -5.0;
    EXPECT_THROW(LinearWalkTimeCalculator{walking_speed_kmh}, std::invalid_argument);
}

TEST(LinearWalkTimeCalculator, ScalingFactorMustBePositive) {
    auto walking_speed_kmh = 5.0;
    auto scaling_factor = 0.0;
    EXPECT_THROW(LinearWalkTimeCalculator(walking_speed_kmh, scaling_factor), std::invalid_argument);
    walking_speed_kmh = -5.0;
    EXPECT_THROW(LinearWalkTimeCalculator(walking_speed_kmh, scaling_factor), std::invalid_argument);
}
