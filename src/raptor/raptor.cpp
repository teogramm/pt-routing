#include "raptor/raptor.h"

#include <ranges>

#include "raptor/stop.h"

#include <vector>
#include <iostream>

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
                                               return std::make_pair(std::cref(to_stop), std::chrono::seconds{0});
                                           });
                    this->transfers.emplace(from_stop, std::move(transfers_with_times));
                }
            }
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
        while (current_stop != origin && i < 10) {
            auto journey_to_here = stop_labels.get_label(n_rounds, current_stop);
            auto boarding_stop = journey_to_here->boarding_stop;
            std::cout << boarding_stop.get().get_name() << "-" << current_stop.get().get_name() << std::endl;
            current_stop = boarding_stop;
            i++;
        }
    }

    void Raptor::process_transfers(LabelManager& stop_labels, const int n_round) {
        for (auto& [transfer_origin, transfer_destinations] : transfers) {
            auto journey_to_origin = stop_labels.get_label(n_round, transfer_origin);
            if (journey_to_origin.has_value()) {
                for (auto& [transfer_destination, transfer_time] : transfer_destinations) {
                    if (transfer_destination.get().get_name() == "Stadion") {
                        std::cout << 1 << std::endl;
                    }
                    auto journey_to_destination = stop_labels.get_label(n_round, transfer_destination);
                    auto arrival_time_with_transfer =
                            std::chrono::zoned_seconds(journey_to_origin->arrival_time.get_time_zone(),
                                                       journey_to_origin->arrival_time.get_sys_time() +
                                                       transfer_time);
                    if (!journey_to_destination.has_value() ||
                        arrival_time_with_transfer.get_sys_time() < journey_to_destination->arrival_time.get_sys_time()) {
                        stop_labels.add_label(n_round, arrival_time_with_transfer, transfer_destination,
                                              transfer_origin.get());
                    }
                }
            }
        }
    }

    void Raptor::route(const Stop& origin, const Stop& destination,
                       const std::chrono::zoned_seconds& departure_time) {
        int n_round = 0;
        auto stop_labels = LabelManager();
        stop_labels.add_label(n_round, departure_time, origin, origin);
        bool improved = true;
        for (int i = 0; i < 5; i++) {
            ++n_round;
            improved = false;
            // First stage: set t_k = t_k-1
            stop_labels.extend_labels(n_round);
            // Traverse all routes
            for (auto&& route : schedule.get_routes()) {
                // Find a stop from this route that we can hop on
                // TODO: What if we can hop on two stops from a route?
                auto route_stops = route.stop_sequence();
                auto hop_on_journey = stop_labels.find_hop_on_stop(
                        route_stops, n_round - 1);
                if (hop_on_journey.has_value()) {
                    auto [hop_on_stop, hop_on_time] = hop_on_journey.value();
                    // Find the earliest trip of the route that we can hop on from this stop
                    const auto& route_trips = route.get_trips();
                    // TODO: Check with upper_bound
                    if (auto earliest_trip = find_earliest_trip(route_trips, hop_on_time, hop_on_stop);
                        earliest_trip != route_trips.end()) {
                        auto& trip = *earliest_trip;
                        auto& trip_stop_times = trip.get_stop_times();
                        auto current_stoptime = std::ranges::find(trip_stop_times,
                            hop_on_stop, &StopTime::get_stop);
                        assert(current_stoptime != trip.get_stop_times().end());
                        auto end_guard = trip_stop_times.end();
                        // Iterate over all the next stops in the trip and update the arrival times
                        while (current_stoptime != end_guard) {
                            auto& current_stop = current_stoptime->get_stop();
                            if (current_stop.get_gtfs_id() == "9022001002601005") {
                                std::cout << 1 << std::endl;
                            }
                            const auto current_arrival_time = current_stoptime->get_arrival_time();
                            const auto existing_journey = stop_labels.get_label(n_round, current_stop);
                            const auto previous_journey = stop_labels.get_label(n_round - 1, current_stop);
                            if (!existing_journey.has_value() ||
                                current_arrival_time.get_sys_time() < existing_journey->arrival_time.get_sys_time()) {
                                stop_labels.add_label(n_round, current_arrival_time, current_stop, hop_on_stop);
                                improved = true;
                            }
                            // TODO: If the optimal arrival time is before the current arrival time we might be able to catch
                            // an earlier trip at that stop
                            else if (
                                previous_journey.has_value() &&
                                previous_journey->arrival_time.get_sys_time() < current_arrival_time.get_sys_time()) {
                                auto earlier_trip =
                                        find_earliest_trip(route_trips, existing_journey->arrival_time, current_stop);
                                // From now on we are following a different trip
                                if (earlier_trip != earliest_trip) {
                                    earliest_trip = earlier_trip;
                                    current_stoptime = std::ranges::find(earliest_trip->get_stop_times(), current_stop,
                                                                         &StopTime::get_stop);
                                    hop_on_stop = current_stop;
                                    assert(current_stoptime != earliest_trip->get_stop_times().end());
                                    end_guard = earliest_trip->get_stop_times().end();
                                    continue;
                                }
                            }
                            ++current_stoptime;
                        }
                    }
                }
            }
            // Process transfers
            process_transfers(stop_labels, n_round);
        }
        build_trip(origin, destination, stop_labels, n_round);
    }
} // namespace raptor
