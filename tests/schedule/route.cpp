#include <gtest/gtest.h>

#include <schedule/components/route.h>

using namespace raptor;

TEST(Route, HashUsesTrips) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};
    const auto agency = Agency{"agency1", "agency", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency};
    const auto route2 = Route{{trip2}, "route1", "route1", "route1", agency};
    ASSERT_NE(route1.hash(), route2.hash());
}

TEST(Route, HashUsesGtfsId) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};
    const auto agency = Agency{"agency1", "agency", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency};
    const auto route2 = Route{{trip1}, "route1", "route1", "route2", agency};
    ASSERT_NE(route1.hash(), route2.hash());
}

TEST(Route, HashUsesOnlyGtfsIdAndTrips) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};

    const auto agency1 = Agency{"agency1", "agency", "", time1.get_time_zone()};
    const auto agency2 = Agency{"agency2", "agency2", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency1};
    const auto route2 = Route{{trip1}, "route2", "route2", "route1", agency2};
    ASSERT_EQ(route1.hash(), route2.hash());
}

TEST(Route, EqualsUsesTrips) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};
    const auto agency = Agency{"agency1", "agency", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency};
    const auto route2 = Route{{trip2}, "route1", "route1", "route1", agency};
    ASSERT_NE(route1, route2);
}

TEST(Route, EqualsUsesGtfsId) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};
    const auto agency = Agency{"agency1", "agency", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency};
    const auto route2 = Route{{trip1}, "route1", "route1", "route2", agency};
    ASSERT_NE(route1, route2);
}

TEST(Route, EqualsUsesOnlyGtfsIdAndTrips) {
    using namespace std::literals::chrono_literals;
    auto time1 = Time{"Europe/Stockholm",
                     std::chrono::local_days{16d / std::chrono::September / 2025} + 9h + 24min};
    auto stop1 = Stop{"stop", "stop", 1.0, 2.0, "", {}};
    auto stop_time1 = StopTime{time1, time1, stop1};
    const auto trip1 = Trip{{stop_time1}, "trip1", "shape1"};

    const auto time2 = Time{"Europe/Stockholm",
                 std::chrono::local_days{17d / std::chrono::September / 2025} + 9h + 24min};
    auto stop2 = Stop{"stop", "stop2", 1.0, 2.0, "", {}};
    auto stop_time2 = StopTime{time2, time2, stop2};
    const auto trip2 = Trip{{stop_time2}, "trip1", "shape1"};

    const auto agency1 = Agency{"agency1", "agency", "", time1.get_time_zone()};
    const auto agency2 = Agency{"agency2", "agency2", "", time1.get_time_zone()};

    const auto route1 = Route{{trip1}, "route1", "route1", "route1", agency1};
    const auto route2 = Route{{trip1}, "route2", "route2", "route1", agency2};
    ASSERT_EQ(route1, route2);
}