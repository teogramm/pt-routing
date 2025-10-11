#ifndef PT_ROUTING_TRANSFERS_H
#define PT_ROUTING_TRANSFERS_H
#include <chrono>
#include <functional>

#include <nanoflann.hpp>
#include "schedule/Schedule.h"

namespace raptor {

    struct StopWithDistance {
        const Stop& stop;
        const double distance_km;
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

        /**
         * Calculates all stops in radius of the given geographical coordinates.
        * @param latitude Longitude of the centre point in decimal degrees.
         * @param longitude Latitude of the centre point in decimal degrees.
         * @param radius_km Search radius in kilometres.
         * @return Vector of stops along with the distance from the search point
         */
        virtual std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) = 0;

        // Use a factory function, since a finder might require arguments be given in its constructor.
        using Factory = std::function<std::unique_ptr<NearbyStopsFinder>(const std::deque<Stop>&)>;
    };

    class WalkTimeCalculator {
    public:
        virtual ~WalkTimeCalculator() = default;

        /**
         * Calculates the walking time between two geographical coordinate pairs.
        * @param latitude_1 Latitude of the first point in decimal degrees.
         * @param longitude_1 Longitude of the first point in decimal degrees.
         * @param latitude_2 Latitude of the second point in decimal degrees.
         * @param longitude_2 Longitude of the second point in decimal degrees.
         * @return Walking time in seconds
         */
        virtual std::chrono::seconds calculate_walking_time(double latitude_1, double longitude_1,
                                                            double latitude_2, double longitude_2) = 0;

        /**
         * Calculates the time required to walk the given distance
         * @param distance_km Distance in kilometres
         * @return Walking time in seconds
         */
        virtual std::chrono::seconds calculate_walking_time(double distance_km) = 0;
    };

    struct TransferManagerParameters {
        // Can't be inside TransferManager because https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165

        double max_radius_km = 1.0; /**<  Maximum distance for creating transfers between stops. */
        /**
         * Duration added to the walking time when calculating the transfer time between two stops.
         * It is intended to account for the time taken to exit and enter the stops.
         * It is added only once for each pair of stops.
         * It is not added for transfers between stops with the same parent station.
         */
        std::chrono::seconds exit_station_duration = std::chrono::seconds{120};

        std::chrono::seconds in_station_transfer_duration = std::chrono::seconds{60};
    };

    /**
     * Class responsible for handling all operations regarding transfers between stops.
     */
    class TransferManager {
        const std::deque<Stop>& stops;
        TransferManagerParameters parameters;

        using StopWithDuration = std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>;
        std::unordered_map<std::reference_wrapper<const Stop>, std::vector<StopWithDuration>> transfers;
        std::vector<StopWithDuration> empty;

        std::unique_ptr<NearbyStopsFinder> nearby_stops_finder;
        std::unique_ptr<WalkTimeCalculator> walk_time_calculator;

        /**
         * Creates transfers for stops inside the same station.
         */
        void build_same_station_transfers();

        /**
         * Builds on-foot transfers between stops in the given range. Only builds transfers between stops for which a
         * transfer has not been previously defined.
         */
        void build_on_foot_transfers();

        /**
         * Calculates transfers and transfer times for all stops.
         */
        void build_transfers() {
            build_same_station_transfers();
            build_on_foot_transfers();
        }

    public:
        // Since we store a reference to the stops, prevent accidental creation with rvalue reference
        template <typename... Args>
        explicit TransferManager(std::deque<Stop>&&, Args...) = delete;

        /**
         * @param stops Collection of stops. A reference to it is stored inside the class, so the caller must ensure
         * it outlives the TransferManager.
         * @param nearby_stops_finder_factory Factory for creating a NearbyStopsFinder object.
         * @param walk_time_calculator Object used for calculating walking times between stops.
         * @param parameters Options affecting
         */
        explicit TransferManager(const std::deque<Stop>& stops,
                                 const NearbyStopsFinder::Factory& nearby_stops_finder_factory,
                                 std::unique_ptr<WalkTimeCalculator> walk_time_calculator,
                                 const TransferManagerParameters parameters = TransferManagerParameters()) :
            parameters(parameters), stops(stops),
            nearby_stops_finder(nearby_stops_finder_factory(stops)),
            walk_time_calculator(std::move(walk_time_calculator)) {
            build_transfers();
        }

        /**
         * Returns all transfers from the given stop, along with the time required to make the transfer.
         * @param stop Transfer origin stop.
         * @return Pairs of destination stop and transfer duration.
         */
        const std::vector<StopWithDuration>& get_transfers_from_stop(const Stop& stop) const;
    };

}

#endif //PT_ROUTING_TRANSFERS_H
