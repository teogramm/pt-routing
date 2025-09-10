#include <ranges>
#include <vector>
#include <iostream>

#include "raptor/raptor.h"
#include "raptor/stop.h"
#include "raptor/StopKDTree.h"

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

    void Raptor::build_transfers() {
        /*TODO: Process GTFS transfers. Maybe all this should be done in the Schedule class.*/
        build_same_station_transfers();
        build_on_foot_transfers();
    }

    void Raptor::build_same_station_transfers() {
        std::unordered_map<std::string, std::vector<std::reference_wrapper<const Stop>>> stops_per_parent_station;
        for (const auto& stop : schedule.get_stops()) {
            auto parent_station_id = std::string(stop.get_parent_stop_id());
            if (!parent_station_id.empty()) {
                stops_per_parent_station[parent_station_id].emplace_back(std::cref(stop));
            }
        }
        // Create a transfer between all stops in the same parent station without a time cost.
        for (auto& stops : stops_per_parent_station | std::views::values) {
            // Make sure to remove the stop itself from the list of stops that can be transferred
            if (stops.size() > 1) {
                for (auto& from_stop : stops) {
                    auto transfers_with_times = std::vector<std::pair<std::reference_wrapper<const Stop>,
                                                                      std::chrono::seconds>>{};
                    auto is_not_this_stop = [&from_stop](const Stop& other_stop) {
                        return from_stop != other_stop;
                    };
                    std::ranges::transform(stops | std::views::filter(is_not_this_stop),
                                           std::back_inserter(transfers_with_times),
                                           [](const Stop& to_stop) {
                                               return std::make_pair(std::cref(to_stop), std::chrono::seconds{30});
                                           });
                    this->transfers.emplace(from_stop, std::move(transfers_with_times));
                }
            }
        }
    }

    void Raptor::build_on_foot_transfers(double max_radius_km) {
        auto& stops = schedule.get_stops();
        auto kd = StopKDTree(stops);
        for (const auto& stop : stops) {
            auto nearby_stops = kd.stops_in_radius(stop, max_radius_km);

            auto& existing_transfers = transfers[stop];
            auto no_existing_transfer = [&existing_transfers](const StopKDTree::StopWithDistance& to_stop) {
                return std::ranges::all_of(existing_transfers,
                                           [&to_stop](const Stop& existing_stop) {
                                               return existing_stop != to_stop.stop;
                                           }, &std::pair<std::reference_wrapper<const Stop>,
                                                         std::chrono::seconds>::first);
            };

            auto new_transfers = std::vector<std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>>();
            std::ranges::transform(nearby_stops | std::views::filter(no_existing_transfer),
                                   std::back_inserter(new_transfers),
                                   [](const StopKDTree::StopWithDistance& to_stop) {
                                       // TODO: Customisable walking speed
                                       const int transfer_time = std::round(3600 * to_stop.distance_m / 5 * 2 + 120);
                                       return std::make_pair(std::cref(to_stop.stop),
                                                             std::chrono::seconds{transfer_time});
                                   });
            existing_transfers.insert(existing_transfers.end(), std::make_move_iterator(new_transfers.begin()),
                                      std::make_move_iterator(new_transfers.end()));
        }
    }

    Raptor::Raptor(const Schedule& schedule) :
        schedule(schedule) {
        build_transfers();
    }

    void Raptor::build_trip(const Stop& origin, const Stop& destination,
                            const LabelManager& stop_labels,
                            int n_rounds) {
        auto current_stop = std::cref(destination);
        int i = 0;
        while (current_stop != origin) {
            auto journey_to_here = stop_labels.get_label(n_rounds, current_stop);
            if (!journey_to_here.has_value()) {
                std::cout << "No way to here found" << std::endl;
            }
            auto boarding_stop = journey_to_here->boarding_stop;
            std::cout << boarding_stop.get().get_name() << "-" << current_stop.get().get_name();

            auto route = journey_to_here->route;
            if (route.has_value()) {
                std::cout << " using " << route->get().get_short_name();
            }
            std::cout << std::endl;
            current_stop = boarding_stop;
            i++;
        }
    }

    void Raptor::process_transfers(LabelManager& stop_labels) {
        for (auto& [transfer_origin, transfer_destinations] : transfers) {
            auto journey_to_origin = stop_labels.get_latest_label(transfer_origin);
            if (journey_to_origin.has_value()) {
                for (auto& [transfer_destination, transfer_time] : transfer_destinations) {
                    auto journey_to_destination = stop_labels.get_latest_label(transfer_destination);
                    auto arrival_time_with_transfer =
                            std::chrono::zoned_seconds(journey_to_origin->arrival_time.get_time_zone(),
                                                       journey_to_origin->arrival_time.get_sys_time() +
                                                       transfer_time);
                    if (!journey_to_destination.has_value() ||
                        arrival_time_with_transfer.get_sys_time() < journey_to_destination->arrival_time.
                        get_sys_time()) {
                        stop_labels.add_label(transfer_destination, arrival_time_with_transfer,
                                              transfer_origin.get(), journey_to_origin->route);
                    }
                }
            }
        }
    }

    void Raptor::process_route(const Route& route,
                               std::ranges::range_difference_t<decltype(std::declval<Route>().stop_sequence())>
                               hop_on_stop_idx, std::chrono::zoned_seconds hop_on_time,
                               LabelManager& stop_labels, int n_round) {
        auto route_stops = route.stop_sequence();
        auto hop_on_stop = route_stops.at(hop_on_stop_idx);
        // Find the earliest trip of the route that we can hop on from this stop
        const auto& route_trips = route.get_trips();
        // TODO: Check with upper_bound
        if (auto trip = find_earliest_trip(route_trips, hop_on_time, hop_on_stop_idx);
            trip != route_trips.end()) {
            auto current_stoptime = std::ranges::next(trip->get_stop_times().begin(), hop_on_stop_idx);
            assert(current_stoptime != trip->get_stop_times().end());
            auto end_guard = std::ranges::end(trip->get_stop_times());
            // Iterate over all the next stops in the trip and update the arrival times
            while (current_stoptime != end_guard) {
                auto& current_stop = current_stoptime->get_stop();
                const auto current_arrival_time = current_stoptime->get_arrival_time();
                const auto existing_journey = stop_labels.get_label(n_round, current_stop);
                const auto previous_journey = stop_labels.get_label(n_round - 1, current_stop);
                if (!existing_journey.has_value() ||
                    current_arrival_time.get_sys_time() < existing_journey->arrival_time.get_sys_time()) {
                    stop_labels.add_label(current_stop, current_arrival_time, hop_on_stop, route);
                }
                // If the optimal arrival time is before the current arrival time we might be able to catch
                // an earlier trip at that stop.
                // TODO: Check if this works
                else if (
                    previous_journey.has_value() &&
                    previous_journey->arrival_time.get_sys_time() < current_arrival_time.get_sys_time()) {
                    auto current_stop_index = std::ranges::distance(
                            std::ranges::begin(trip->get_stop_times()), current_stoptime);
                    auto earlier_trip =
                            find_earliest_trip(route_trips, previous_journey->arrival_time,
                                               current_stop_index);
                    // From now on we are following a different trip
                    if (earlier_trip != trip) {
                        trip = earlier_trip;
                        current_stoptime = std::ranges::next(
                                trip->get_stop_times().begin(), current_stop_index);
                        hop_on_stop = current_stop;
                        assert(current_stoptime != trip->get_stop_times().end());
                        end_guard = trip->get_stop_times().end();
                    }
                }
                ++current_stoptime;
            }
        }
    }


    void Raptor::route(const Stop& origin, const Stop& destination,
                       const std::chrono::zoned_seconds& departure_time) {
        int n_round = 0;
        auto stop_labels = LabelManager();
        // TODO: Remove the need for dummy route
        stop_labels.add_label(origin, departure_time, origin, std::nullopt);
        bool improved = true;
        for (int i = 0; i < 5; i++) {
            ++n_round;
            improved = false;
            // First stage: set t_k = t_k-1
            stop_labels.new_round();
            // Second stage: Traverse all routes
            for (auto&& route : schedule.get_routes()) {
                // Find the earliest stop from this route that we can hop on
                auto route_stops = route.stop_sequence();
                auto hop_on_journey = stop_labels.find_hop_on_stop(
                        route_stops, n_round - 1);
                if (hop_on_journey.has_value()) {
                    auto [hop_on_stop_index, hop_on_time] = hop_on_journey.value();
                    process_route(route, hop_on_stop_index, hop_on_time, stop_labels, n_round);
                }
            }
            // Third stage: Process transfers
            process_transfers(stop_labels);
        }
        build_trip(origin, destination, stop_labels, n_round);
    }
} // namespace raptor
