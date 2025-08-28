#include "raptor/raptor.h"

#include <ranges>

#include "raptor/stop.h"

#include <vector>

namespace raptor {

    void Raptor::find_routes_serving_stop() {
        // TODO: See if this can be done with ranges
        for (const auto& route : schedule.get_routes()) {
            // Every trip of a route shares the same stops, so just take the first trip
            auto& first_trip = route.get_trips().front();
            for (const auto& stop_time : first_trip.get_stop_times()) {
                const auto& stop = stop_time.get_stop();
                routes_serving_stop[stop].emplace(route);
            }
        }
    }

    void Raptor::calculate_transfers() {
        /*TODO: Calculate on-foot transfers between different stops. For now calculates transfers between stops with
        the same parent station. Also process GTFS transfers. Maybe all this should be done in the Schedule class.*/\
        std::unordered_map<std::string, std::vector<std::reference_wrapper<const Stop>>> stops_per_parent_station;
        for (const auto& stop : schedule.get_stops()) {
            auto parent_station_id = std::string(stop.get_parent_stop_id());
            if (!parent_station_id.empty()) {
                stops_per_parent_station[parent_station_id].emplace_back(std::cref(stop));
            }
        }
        // Create a transfer between all stops in the same parent station without a time cost.
        for (auto& stops : stops_per_parent_station | std::views::values) {
            // Make sure to remove the stop itself from the list of stops that can be transfered
            if (stops.size() > 1) {
                for (auto& stop : stops) {
                    auto transfers_with_times = std::vector<std::pair<std::reference_wrapper<const Stop>, int>>{};
                    auto is_not_this_stop = [&stop](const Stop& other_stop) {
                        return stop != other_stop;
                    };
                    std::ranges::transform(stops | std::views::filter(is_not_this_stop),
                                           std::back_inserter(transfers_with_times),
                                           [](const Stop& stop) {
                                               return std::make_pair(std::cref(stop), 0);
                                           });
                    this->transfers[stop] = std::move(transfers_with_times);
                }
            }
        }
    }

    Raptor::Raptor(const Schedule& schedule) :
        schedule(schedule) {
        find_routes_serving_stop();
        calculate_transfers();
    }

} // namespace raptor
