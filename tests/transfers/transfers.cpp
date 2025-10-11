#include <gtest/gtest.h>

#include <transfers/transfers.h>

using namespace raptor;

/**
 * Always returns a five-minute walking time between two stops
 */
class FiveMinCalculator final : public WalkTimeCalculator {
public:
    std::chrono::seconds calculate_walking_time(double latitude_1, double longitude_1, double latitude_2,
                                                double longitude_2) override {
        using namespace std::chrono_literals;
        return 5min;
    };

    std::chrono::seconds calculate_walking_time(double distance_km) override {
        using namespace std::chrono_literals;
        return 5min;
    }
};

auto nearby_stop = Stop("nearby stop 1", "nearby", 2.0, 4.0, "platform 1", {});

/**
 * Always returns one nearby stop 500m away.
 */
class SingleNearbyStopFinder final : public NearbyStopsFinder {
public:
    std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) override {
        if (radius_km >= 0.5)
            return {StopWithDistance{nearby_stop, 0.5}};
        return {};
    }

    static Factory create_factory() {
        return [](const std::deque<Stop>&) {
            return std::make_unique<SingleNearbyStopFinder>();
        };
    }
};

/**
 * Never finds any nearby stops
 */
class NoNearbyStopsFinder final : public NearbyStopsFinder {
public:
    std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) override {
        return {};
    }

    static Factory create_factory() {
        return [](const std::deque<Stop>&) {
            return std::make_unique<NoNearbyStopsFinder>();
        };
    }
};

TEST(TransferManager, CannotConstructWithRValueStops) {
    constexpr auto can_construct = std::is_constructible_v<TransferManager,
                                                           std::deque<Stop>&&, const NearbyStopsFinder::Factory&,
                                                           std::unique_ptr<WalkTimeCalculator>,
                                                           const TransferManagerParameters>;

    EXPECT_FALSE(can_construct);
}

TEST(TransferManager, ExitDurationAddedOnce) {
    using namespace std::chrono_literals;
    const auto stops = std::deque{nearby_stop};
    auto tm = TransferManager{stops, SingleNearbyStopFinder::create_factory(),
                              std::make_unique<FiveMinCalculator>(),
                              {.exit_station_duration = 2min}};
    const auto& transfers = tm.get_transfers_from_stop(nearby_stop);
    EXPECT_EQ(transfers.at(0).second, 5min + 2min);
}

TEST(TransferManager, UsesRadiusParameter) {
    using namespace std::chrono_literals;
    const auto stops = std::deque{nearby_stop};
    auto tm = TransferManager{stops, SingleNearbyStopFinder::create_factory(),
                              std::make_unique<FiveMinCalculator>(),
                              {.max_radius_km = 0.2}};
    const auto& transfers = tm.get_transfers_from_stop(nearby_stop);
    EXPECT_TRUE(transfers.empty());
}

TEST(TransferManager, ExitDurationIsNotAddedInSameStationTransfers) {
    using namespace std::literals;
    auto stop1 = Stop("test", "stop1", 1.1, 2.2, "hello", {});
    auto stop2 = Stop("test", "stop2", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "station1", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
            {"station1"s, std::vector{"stop1"s, "stop2"s}}
    };
    auto manager = StopManager({stop1, stop2}, {station1}, stops_per_station);
    auto& stops = manager.get_stops();
    auto tm = TransferManager{stops, NoNearbyStopsFinder::create_factory(),
        std::make_unique<FiveMinCalculator>(), {.in_station_transfer_duration = 60s}};

    const auto& transfers = tm.get_transfers_from_stop(stop1);
    EXPECT_EQ(transfers.size(), 1);
    EXPECT_EQ(transfers.at(0).second, 60s);
}

TEST(TransferManager, OnFootDoesNotOverrideSameStation) {
    using namespace std::literals;
    auto stop1 = Stop("test", "stop1", 1.1, 2.2, "hello", {});
    auto station1 = Station("station", "station1", {});
    const auto stops_per_station = StopManager::StationToChildStopsMap{
                {"station1"s, std::vector{"stop1"s, nearby_stop.get_gtfs_id()}}
    };
    auto manager = StopManager({stop1, nearby_stop}, {station1}, stops_per_station);
    auto& stops = manager.get_stops();

    auto tm = TransferManager{stops, SingleNearbyStopFinder::create_factory(),
        std::make_unique<FiveMinCalculator>(), {.in_station_transfer_duration = 60s}};

    const auto& transfers = tm.get_transfers_from_stop(stop1);
    EXPECT_EQ(transfers.size(), 1);
    EXPECT_EQ(transfers.at(0).second, 60s);
}
