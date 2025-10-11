/**
 * Tests for the KD Tree-based NearbyStopsFinder.
 */

#include <gtest/gtest.h>

#include <transfers/kd_tree.h>

using namespace raptor;

auto stop1 = Stop{"stop1", "stop1", 59.15225526334754, 18.246309647687365, "", {}};
// Stop1 - Stop2 real-world distance 882m
auto stop2 = Stop{"stop2", "stop2", 59.15627986037491, 18.259634253669688, "", {}};
// Stop1 - Stop3 real-world distance 1.5km
auto stop3 = Stop{"stop3", "stop3", 59.15969531957956, 18.268264633334773, "", {}};

TEST(KDTree, DoesNotReturnGivenStop) {
    auto stops = std::deque{stop1, stop2};
    auto kd_tree = StopKDTree{stops};
    // Actual distance is 882m, so give a generous radius
    auto nearby_stops = kd_tree.stops_in_radius(stop1, 2.0);
    ASSERT_EQ(nearby_stops.size(), 1);
    ASSERT_EQ(nearby_stops.at(0).stop, stop2);
}

TEST(KDTree, CalculateStopsInRadius) {
    auto stops = std::deque{stop1, stop2, stop3};
    auto kd_tree = StopKDTree{stops};
    // Although there is some approximation in the Stop KD tree, it should not be too much
    auto nearby_stops = kd_tree.stops_in_radius(stop1, 1.3);
    ASSERT_EQ(nearby_stops.size(), 1);
    ASSERT_EQ(nearby_stops.at(0).stop, stop2);
}

TEST(KDTree, ReturnsStopsOnSearchCoordinates) {
    // When searching using coords it should not exclude stops directly on top of the search point
    auto stops = std::deque{stop1, stop2, stop3};
    auto kd_tree = StopKDTree{stops};
    auto nearby_stops = kd_tree.stops_in_radius(stop1.get_coordinates().first, stop1.get_coordinates().second, 99);
    ASSERT_EQ(nearby_stops.size(), 3);
    EXPECT_EQ(nearby_stops.at(0).stop, stop1);
    EXPECT_DOUBLE_EQ(nearby_stops.at(0).distance_km, 0);
}

TEST(KDTree, CalculateAllDistancePairs) {
    auto stops = std::deque{stop1, stop2, stop3};
    auto kd_tree = StopKDTree{stops};
    auto all_nearby_stops = kd_tree.stops_in_radius(99);
    ASSERT_EQ(all_nearby_stops.size(), stops.size());
    for (const auto& stop : all_nearby_stops) {
        ASSERT_EQ(stop.nearby_stops.size(), all_nearby_stops.size() - 1);
    }
}
