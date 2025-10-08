#include "schedule/Schedule.h"

#include <algorithm>
#include <ranges>
#include <boost/container_hash/hash.hpp>

namespace raptor {

    void StopManager::initialise_relationships(
            const StationToChildStopsMap& stops_per_station) {
        auto stop_index = std::unordered_map<std::string, std::reference_wrapper<Stop>>{};
        for (auto& stop: stops) {
            stop_index.insert({stop.get_gtfs_id(), std::ref(stop)});
        }
        auto station_index = std::unordered_map<std::string, std::reference_wrapper<Station>>{};
        for (auto& station: stations) {
            station_index.insert({station.get_gtfs_id(), std::ref(station)});
        }

        for (auto& [station_id, child_stop_ids] : stops_per_station) {
            auto& parent_station = station_index.at(station_id).get();
            for (auto& child_stop_id : child_stop_ids) {
                auto& child_stop = stop_index.at(child_stop_id).get();
                set_parent_station(parent_station, child_stop);
            }
        }
    }

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
        const std::vector<std::reference_wrapper<const raptor::Stop>>& stops) const noexcept {
    auto hasher = std::hash<std::reference_wrapper<const raptor::Stop>>{};
    std::size_t seed = 0;
    for (const auto& stop : stops) {
        boost::hash_combine(seed, hasher(stop));
    }
    return seed;
}
