#include "gtest/gtest.h"

#include "raptor/raptor.h"

using namespace raptor;

auto stop = Stop("test", "", 59.21, 16.2, "", {});

TEST(LabelManager, RetainValuesAfterNewRound) {
    auto time = std::chrono::floor<Time::duration>(std::chrono::system_clock::now());
    auto zoned_time = Time{"Europe/Stockholm", time};
    LabelManager manager(stop, zoned_time, std::nullopt, std::nullopt);
    EXPECT_TRUE(manager.get_latest_label(stop).has_value());
    EXPECT_FALSE(manager.get_previous_label(stop).has_value());
    manager.new_round();
    EXPECT_TRUE(manager.get_latest_label(stop).has_value());
    EXPECT_TRUE(manager.get_previous_label(stop).has_value());
    // EXPECT_EQ Throws an error
    EXPECT_TRUE(
            manager.get_latest_label(stop).value().arrival_time.get_sys_time() == manager.get_previous_label(stop).value
            ().arrival_time.get_sys_time());
}
