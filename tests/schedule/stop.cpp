#include <gtest/gtest.h>

#include <schedule/components/stop.h>

using namespace raptor;

TEST(BaseStop, EqualsUsesOnlyGtfsId) {
    const auto stop1 = BaseStop("test", "123", 1.1, 2.2);
    const auto stop2 = BaseStop("test", "1234", 1.1, 2.2);
    const auto stop3 = BaseStop("hello", "123", 5.0, 6.0);
    EXPECT_EQ(stop1, stop3);
    EXPECT_NE(stop1, stop2);
}

TEST(Stop, GetNoParentStation) {
    const auto stop = Stop("test", "123", 0.0, 0.0, "", {});
    EXPECT_EQ(stop.get_parent_station(), std::nullopt);
}

TEST(Stop, EqualsUsesOnlyGtfsId) {
    const auto stop1 = Stop("test", "123", 1.1, 2.2, "hello", {});
    const auto stop2 = Stop("test", "1234", 1.1, 2.2, "hello", {});
    const auto stop3 = Stop("hello", "123", 5.0, 6.0, "",
                            {BoardingArea("test", "test", 0.0, 6.5)});

    EXPECT_EQ(stop1, stop3);
    EXPECT_NE(stop1, stop2);
}

TEST(StopManager, InitialiseWithoutRelationships) {
    auto stop1 = Stop("test", "123", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "789", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{};
    auto manager = StopManager({stop1}, {station1}, stops_per_station);
    auto& inserted_stop = manager.get_stops().front();
    EXPECT_EQ(inserted_stop.get_parent_station(), std::nullopt);
    auto& inserted_station = manager.get_stations().front();
    EXPECT_TRUE(inserted_station.get_stops().empty());
}

TEST(StopManager, InitialiseWithRelationships) {
    using namespace std::string_literals;
    auto stop1 = Stop("test", "stop1", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "station1", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
                {"station1"s, std::vector{"stop1"s}}
    };
    auto manager = StopManager({stop1}, {station1}, stops_per_station);
    auto& inserted_stop = manager.get_stops().front();
    auto& inserted_station = manager.get_stations().front();
    // TODO: Change this when implementing operator== on Station
    EXPECT_EQ(&inserted_stop.get_parent_station().value().get(), &inserted_station);
    EXPECT_EQ(inserted_station.get_stops(), std::vector{std::cref(inserted_stop)});
}

TEST(StopManager, InitialiseWithPartialRelationships) {
    using namespace std::string_literals;
    auto stop1 = Stop("test", "stop1", 1.1, 2.2, "hello", {});
    auto stop2 = Stop("test2", "stop2", 1.1, 2.2, "hello", {});
    auto station1 = Station("station1", "station1", {});
    auto station2 = Station("station2", "station2", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
                    {"station1"s, std::vector{"stop1"s}}
    };
    auto manager = StopManager({stop1}, {station1}, stops_per_station);
    for (const auto& inserted_stop: manager.get_stops()) {
        auto parent_station = inserted_stop.get_parent_station();
        if (inserted_stop.get_gtfs_id() == "stop1") {
            EXPECT_NE(parent_station, std::nullopt);
        }else {
            EXPECT_EQ(parent_station, std::nullopt);
        }
    }
    for (const auto& inserted_station: manager.get_stations()) {
        const auto& child_stops = inserted_station.get_stops();
        if (inserted_station.get_gtfs_id() == "station1") {
            EXPECT_FALSE(child_stops.empty());
        }else {
            EXPECT_TRUE(child_stops.empty());
        }
    }
}

TEST(StopManager, InitialiseInvalidStopID) {
    using namespace std::string_literals;
    auto stop1 = Stop("test", "123", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "789", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
        {"789"s, std::vector{"5612317"s}}
    };
    EXPECT_THROW(StopManager({stop1}, {station1}, stops_per_station), std::out_of_range);
}

TEST(StopManager, InitialiseInvalidStationID) {
    using namespace std::string_literals;
    auto stop1 = Stop("test", "123", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "789", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
            {"123112"s, std::vector{"123"s}}
    };
    EXPECT_THROW(StopManager({stop1}, {station1}, stops_per_station), std::out_of_range);
}