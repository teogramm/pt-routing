#include <gtest/gtest.h>

#include <schedule/components/trip.h>

using namespace raptor;

TEST(Trip, InvalidStopTimeIndex) {
    using namespace std::literals::chrono_literals;
    auto time = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time = StopTime{time, time, stop};
    const auto trip = Trip{{stop_time}, "trip1", "shape1"};
    ASSERT_THROW(auto t = trip.get_stop_time(1), std::out_of_range);
}

TEST(Trip, CannotConstructWithoutStopTimes) {
    ASSERT_ANY_THROW(Trip({}, "aa", ""));
}

TEST(Trip, EqualUsesDepartureTime) {
    // Test that equals takes into account the instantiation date of the trip
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                     std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop_time2 = StopTime{time2, time2, stop1};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};
    // Differ only in StopTime
    ASSERT_NE(trip1, trip2);

    const auto time3 = Time{time1};
    const auto stop_time3 = StopTime{time3, time3, stop1};
    const auto trip3 = Trip{{stop_time3}, "trip1", "shape2"};
    // Differ only in shape ID
    ASSERT_EQ(trip1, trip3);
}

TEST(Trip, EqualsUsesGtfsId) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop_time2 = StopTime{time2, time2, stop1};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape2"};
    // Differ only in shape ID
    ASSERT_EQ(trip1, trip2);

    const auto time3 = Time{time1};
    const auto stop_time3 = StopTime{time3, time3, stop1};
    const auto trip3 = Trip{{stop_time3}, "trip2", "shape1"};
    // Differ only in GTFS ID
    ASSERT_NE(trip1, trip3);
}