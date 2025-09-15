#include <ranges>
#include <vector>

#include "raptor/raptor.h"
#include "raptor/stop.h"
#include "raptor/StopKDTree.h"

namespace raptor {
    void Raptor::build_routes_serving_stop() {
        // TODO: See if this can be done with ranges
        for (const auto& route : schedule.get_routes()) {
            // Every trip of a route shares the same stops, so just take the first trip
            auto& first_trip = route.get_trips().front();
            auto index = 0;
            for (const auto& stop_time : first_trip.get_stop_times()) {
                const auto& stop = stop_time.get_stop();
                routes_serving_stop[stop].emplace_back(route, index);
                index++;
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
                                               return std::make_pair(std::cref(to_stop), std::chrono::seconds{60});
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
        build_routes_serving_stop();
    }

    std::vector<Movement> Raptor::build_trip(const Stop& origin, const Stop& destination,
                                             const LabelManager& stop_labels) {
        auto current_stop = std::cref(destination);
        auto journey_to_here = stop_labels.get_latest_label(current_stop);
        if (!journey_to_here.has_value()) {
            return {};
        }
        auto journey = std::vector<Movement>{};
        while (journey_to_here.has_value() && journey_to_here->boarding_stop.has_value()) {
            auto boarding_stop = journey_to_here->boarding_stop.value();

            if (journey_to_here->route_and_trip_index.has_value()) {
                // PT Movement
                auto route = journey_to_here->route_and_trip_index->first;
                auto route_stops = route.get().stop_sequence();
                auto& trip = route.get().get_trips().at(journey_to_here->route_and_trip_index->second);
                // TODO: This can produce wrong results when a route travels through the same stop twice
                auto from_stop = std::ranges::find(route_stops, boarding_stop);
                auto to_stop = std::ranges::find(std::next(route_stops.begin(), 0), std::end(route_stops),
                                                 current_stop.get());
                auto from_stop_index = std::distance(std::begin(route_stops), from_stop);
                auto to_stop_index = std::distance(std::begin(route_stops), to_stop);
                journey.emplace(journey.begin(), PTMovement(trip, from_stop_index, to_stop_index, route));
            }
            else {
                journey.emplace(journey.begin(), WalkingMovement(boarding_stop, current_stop, {}));
            }

            current_stop = journey_to_here->boarding_stop.value();
            journey_to_here = stop_labels.get_latest_label(current_stop);
        }
        return journey;
    }

    void Raptor::process_transfers(RaptorStatus& status) {
        auto& stop_labels = status.label_manager;
        for (auto& origin_stop : status.improved_stops) {
            auto journey_to_origin = stop_labels.get_latest_label(origin_stop);
            for (auto& [destination_stop, transfer_time] : transfers[origin_stop]) {
                auto journey_to_destination = stop_labels.get_latest_label(destination_stop);
                auto arrival_time_with_transfer =
                        std::chrono::zoned_seconds(journey_to_origin->arrival_time.get_time_zone(),
                                                   journey_to_origin->arrival_time.get_sys_time() +
                                                   transfer_time);
                if (!journey_to_destination.has_value() ||
                    arrival_time_with_transfer.get_sys_time() < journey_to_destination->arrival_time.
                    get_sys_time()) {
                    stop_labels.add_label(destination_stop, arrival_time_with_transfer,
                                          origin_stop.get(), std::nullopt);
                    status.improved_stops.insert(destination_stop);
                    status.earliest_arrival_time[destination_stop] = arrival_time_with_transfer;
                }
            }
        }
    }

    bool Raptor::can_improve_current_journey_to_stop(const Time& new_arrival_time, const Stop& current_stop,
                                                     const Stop& destination, const RaptorStatus& status) {
        auto arrival_time_to_current_stop = status.earliest_arrival_time.find(current_stop);
        if (arrival_time_to_current_stop == status.earliest_arrival_time.end())
            return true;
        auto arrival_time_to_destination = status.earliest_arrival_time.find(destination);
        if (arrival_time_to_destination == status.earliest_arrival_time.end()) {
            return new_arrival_time.get_sys_time() < arrival_time_to_current_stop->second.get_sys_time();
        }
        return new_arrival_time.get_sys_time() < std::min(arrival_time_to_current_stop->second.get_sys_time(),
                                                          arrival_time_to_destination->second.get_sys_time());

    }

    void Raptor::process_route(const Route& route, StopIndex hop_on_stop_idx, Time hop_on_time, RaptorStatus& status,
                               const Stop& destination) {
        auto& stop_labels = status.label_manager;
        auto hop_on_stop = route.stop_sequence().at(hop_on_stop_idx);
        // Find the earliest trip of the route that we can hop on from this stop
        const auto& route_trips = route.get_trips();
        // TODO: Check with upper_bound
        if (auto trip = find_earliest_trip(route_trips, hop_on_time, hop_on_stop_idx);
            trip != route_trips.end()) {
            auto current_stoptime = std::ranges::next(trip->get_stop_times().begin(), hop_on_stop_idx);
            assert(current_stoptime != trip->get_stop_times().end());
            auto sentinel = std::ranges::end(trip->get_stop_times());
            // Iterate over all the next stops in the trip and update the arrival times
            while (current_stoptime != sentinel) {
                const auto& current_stop = current_stoptime->get_stop();
                const auto current_arrival_time = current_stoptime->get_arrival_time();

                const auto previous_journey = stop_labels.get_previous_label(current_stop);
                const auto might_catch_earlier_trip = previous_journey.has_value() &&
                        previous_journey->arrival_time.get_sys_time() < current_stoptime->get_departure_time().
                        get_sys_time();

                if (can_improve_current_journey_to_stop(current_arrival_time, current_stop, destination, status)) {
                    auto trip_index = std::distance(route_trips.begin(), trip);
                    stop_labels.add_label(current_stop, current_arrival_time, hop_on_stop,
                                          std::make_pair(std::cref(route), trip_index));
                    status.earliest_arrival_time[current_stop] = current_arrival_time;
                    status.improved_stops.insert(std::cref(current_stop));
                }
                // If the optimal arrival time is before the current arrival time we might be able to catch
                // an earlier trip at that stop.
                // TODO: Check if this works
                else if (might_catch_earlier_trip) {
                    const auto current_stop_index =
                            std::ranges::distance(std::ranges::begin(trip->get_stop_times()), current_stoptime);
                    auto earlier_trip =
                            find_earliest_trip(route_trips, previous_journey->arrival_time, current_stop_index);
                    // From now on we are following a different trip
                    if (earlier_trip != trip) {
                        trip = earlier_trip;
                        current_stoptime = std::ranges::next(trip->get_stop_times().begin(), current_stop_index);
                        hop_on_stop = current_stop;
                        assert(current_stoptime != trip->get_stop_times().end());
                        sentinel = trip->get_stop_times().end();
                    }
                }
                ++current_stoptime;
            }
        }
    }


    std::vector<Movement> Raptor::route(const Stop& origin, const Stop& destination,
                                        const Time& departure_time) {
        auto status = RaptorStatus{};
        auto& stop_labels = status.label_manager;
        status.n_round = 0;
        // TODO: Remove the need for dummy route
        stop_labels.add_label(origin, departure_time, std::nullopt, std::nullopt);
        status.earliest_arrival_time[origin] = departure_time;
        status.improved_stops.insert(origin);
        while (!status.improved_stops.empty()) {
            ++status.n_round;
            // First stage: set t_k = t_k-1
            stop_labels.new_round();
            // Second stage: Traverse all routes
            auto current_round_routes = find_routes_to_examine(status.improved_stops);
            status.improved_stops.clear();
            for (auto& [route, stop_index] : current_round_routes) {
                auto hop_on_stop = route.get().stop_sequence().at(stop_index);
                auto arrival_time = status.earliest_arrival_time.at(hop_on_stop);
                process_route(route, stop_index, arrival_time, status, destination);
            }
            // Third stage: Process transfers
            process_transfers(status);
        }
        return build_trip(origin, destination, stop_labels);
    }
} // namespace raptor
