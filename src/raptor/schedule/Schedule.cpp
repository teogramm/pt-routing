#include "raptor/schedule/Schedule.h"

#include <algorithm>
#include <ranges>
#include <boost/container_hash/hash.hpp>

namespace raptor {

    std::vector<std::reference_wrapper<const Stop>> Route::stop_sequence() const {
        // All trips have the same stops, so take the stops from the first trip
        auto stops = std::vector<std::reference_wrapper<const Stop>>{};
        auto& first_trip = trips.front();
        stops.reserve(first_trip.get_stop_times().size());
        std::ranges::transform(first_trip.get_stop_times(), std::back_inserter(stops), [](const StopTime& stop_time) {
            return std::cref(stop_time.get_stop());
        });
        return stops;
    }

    size_t Route::hash() const {
        auto stops = stop_sequence();
        auto gtfs_route_id = gtfs_id;
        return hash(stops, gtfs_route_id);
    }

    size_t Route::hash(const std::vector<std::reference_wrapper<const Stop>>& stops, const std::string& gtfs_route_id) {
        size_t seed = 0;
        boost::hash_combine(seed, std::hash<std::vector<std::reference_wrapper<const Stop>>>{}(stops));
        boost::hash_combine(seed, std::hash<std::string>{}(gtfs_route_id));
        return seed;
    }
}


std::size_t std::hash<std::vector<std::reference_wrapper<const raptor::Stop>>>::operator()(
        const std::vector<std::reference_wrapper<const raptor::Stop>>& stops) const {
    auto hasher = std::hash<std::reference_wrapper<const raptor::Stop>>{};
    std::size_t seed = 0;
    for (const auto& stop : stops) {
        boost::hash_combine(seed, hasher(stop));
    }
    return seed;
}
