#ifndef PT_ROUTING_TRANSFERS_H
#define PT_ROUTING_TRANSFERS_H
#include <chrono>
#include <functional>

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

    /**
     * Interface for classes that support performing nearby stop searches.
     */
    class NearbyStopsFinder {
    public:
        virtual ~NearbyStopsFinder() = default;
        virtual std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) = 0;

        // Use a factory function, since a finder might require arguments be given in its constructor.
        using Factory = std::function<std::unique_ptr<NearbyStopsFinder>(const std::deque<Stop>&)>;
    };

    class WalkTimeCalculator {
    public:
        virtual ~WalkTimeCalculator() = default;
        virtual std::chrono::seconds calculate_walking_time(double latitude_1, double longitude_1,
                                                            double latitude_2, double longitude_2) = 0;
        virtual std::chrono::seconds calculate_walking_time(double distance) = 0;
    };

    class TransferManager {
        const std::deque<Stop>& stops;

        using StopWithDuration = std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>;
        std::unordered_map<std::reference_wrapper<const Stop>, std::vector<StopWithDuration>> transfers;
        std::vector<StopWithDuration> empty;

        std::unique_ptr<NearbyStopsFinder> nearby_stops_finder;

        void build_same_station_transfers();

        void build_on_foot_transfers(double max_radius_km);

        void build_transfers() {
            build_same_station_transfers();
            build_on_foot_transfers(1);
        }

    public:
        explicit TransferManager(const std::deque<Stop>& stops,
                                 const NearbyStopsFinder::Factory& nearby_stops_finder_factory) :
            stops(stops), nearby_stops_finder(nearby_stops_finder_factory(stops)) {
            build_transfers();
        }

        const std::vector<StopWithDuration>& get_transfers_from_stop(const Stop& stop) const;
    };

}

#endif //PT_ROUTING_TRANSFERS_H
