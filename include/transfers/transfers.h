#ifndef PT_ROUTING_TRANSFERS_H
#define PT_ROUTING_TRANSFERS_H
#include <chrono>

#include <nanoflann.hpp>
#include "schedule/Schedule.h"

namespace raptor {

    struct StopWithDistance {
        const Stop& stop;
        const double distance_m;
    };

    struct StopsInRadius {
        const Stop& original_stop;
        const std::vector<StopWithDistance> nearby_stops;
    };

    class INearbyStopsFinder {
    public:
        virtual ~INearbyStopsFinder() = default;
        virtual std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) = 0;
    };

    class IWalkingTimeCalculator {
    public:
        virtual ~IWalkingTimeCalculator() = default;
        virtual std::chrono::seconds calculate_walking_time(double latitude_1, double longitude_1,
                                                            double longitude_2, double latitude_2) = 0;
    };

    class TransferManager {
        const std::deque<Stop>& stops;

        using StopWithDuration = std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>;
        std::unordered_map<std::reference_wrapper<const Stop>, std::vector<StopWithDuration>> transfers;
        std::vector<StopWithDuration> empty;

        std::unique_ptr<INearbyStopsFinder> nearby_stops_finder;

        void build_same_station_transfers();

        void build_on_foot_transfers(double max_radius_km);

        void build_transfers() {
            build_same_station_transfers();
            build_on_foot_transfers(1);
        }

    public:
        explicit TransferManager(const std::deque<Stop>& stops,
                                 std::unique_ptr<INearbyStopsFinder> nearby_stops_finder) :
            stops(stops), nearby_stops_finder(std::move(nearby_stops_finder)) {
            build_transfers();
        }

        const std::vector<StopWithDuration>& get_transfers_from_stop(const Stop& stop) const;
    };

}

#endif //PT_ROUTING_TRANSFERS_H
