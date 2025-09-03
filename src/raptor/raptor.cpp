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
        calculate_transfers();
    }

    void Raptor::build_trip(const Stop& origin, const Stop& destination,
        const std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>& stop_labels) {
        auto current_stop = std::cref(destination);
        while (current_stop != origin) {
            auto& transfers = this->transfers[current_stop];
            auto label = stop_labels.at(std::cref(current_stop)).get_label();
            auto journey_to_here = stop_labels.at(std::cref(current_stop)).get_label().value();
            auto& boarding_stop = journey_to_here.boarding_stop;
            std::cout << boarding_stop.get().get_name() << "-" << current_stop.get().get_name() << std::endl;
            current_stop = boarding_stop;
        }
    }

    void Raptor::route(const Stop& origin, const Stop& destination,
                       const std::chrono::zoned_seconds& departure_time) {
        int n_round = 0;
        auto stop_labels = std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>();
        auto dummy_start_trip = Trip({}, "dummy", "dummy");
        stop_labels[origin].add_label(0, departure_time.get_sys_time().time_since_epoch().count(), origin,
                                      dummy_start_trip);
        n_round++;
        bool improved = true;
        for (int i = 0; i < 5; i++) {
            improved = false;
            // First stage: set t_k = t_k-1
            std::ranges::for_each(stop_labels | std::views::values, [n_round](RaptorLabel& stop_label) {
                                      auto prev_label = stop_label.get_label(n_round - 1);
                                      stop_label.add_label(n_round, prev_label);
                                  }
                    );
            // Traverse all routes
            for (auto&& route : schedule.get_routes()) {
                // Find a stop from this route that we can hop on
                // TODO: What if we can hop on two stops from a route?
                auto route_stops = route.stop_sequence();
                auto hop_on_stop = std::ranges::find_if(route_stops, [n_round, &stop_labels](const auto& stop) {
                    auto hop_on_journey = stop_labels[stop].get_label(n_round - 1);
                    return hop_on_journey.has_value();
                });
                if (hop_on_stop != route_stops.end()) {
                    auto hop_on_journey = stop_labels[*hop_on_stop].get_label(n_round).value();
                    // Find the earliest trip of the route that we can hop on from this stop
                    const auto& route_trips = route.get_trips();
                    // TODO: Check with upper_bound
                    auto hop_on_time = hop_on_journey.arrival_time;
                    auto earliest_trip = std::ranges::find_if(route_trips,
                                                              [hop_on_time, hop_on_stop](const auto& trip) {
                                                                  const auto& trip_departure_time = trip.
                                                                          get_stop_time(*hop_on_stop).get_departure_time();
                                                                  return trip_departure_time.
                                                                          get_sys_time().time_since_epoch().count() >
                                                                          hop_on_time;
                                                              });
                    if (earliest_trip != route_trips.end()) {
                        auto& trip = *earliest_trip;
                        auto current_stop_time = std::ranges::find(trip.get_stop_times(), *hop_on_stop, &StopTime::get_stop);
                        auto end_guard = trip.get_stop_times().end();
                        // Iterate over all the next stops in the trip and update the arrival times
                        while (current_stop_time != end_guard) {
                            auto& current_stop = current_stop_time->get_stop();
                            const auto new_arrival_time = current_stop_time->
                                                          get_arrival_time().get_sys_time().time_since_epoch().
                                                          count();
                            const auto existing_journey = stop_labels[current_stop].get_label(n_round);
                            if (!existing_journey.has_value() || new_arrival_time < existing_journey->arrival_time) {
                                stop_labels[current_stop].add_label(n_round, new_arrival_time, *hop_on_stop, trip);
                                improved = true;
                            }
                            // TODO: If the optimal arrival time is before the current arrival time we might be able to catch
                            // an earlier trip at that stop
                            else if (existing_journey->arrival_time < new_arrival_time) {
                                auto earlier_trip = std::ranges::find_if(route_trips,
                                                              [&existing_journey, current_stop_time](const auto& trip) {
                                                                  const auto& trip_departure_time = trip.
                                                                          get_stop_time(current_stop_time->get_stop()).get_departure_time();
                                                                  return trip_departure_time.
                                                                          get_sys_time().time_since_epoch().count() >
                                                                          existing_journey->arrival_time;
                                                              });
                                // From now on we are following a different trip
                                if (earlier_trip != earliest_trip) {
                                    earliest_trip = earlier_trip;
                                    current_stop_time = std::ranges::find(earliest_trip->get_stop_times(), current_stop,
                                                                 &StopTime::get_stop);
                                    end_guard = earliest_trip->get_stop_times().end();
                                }
                            }
                            ++current_stop_time;
                        }
                    }
                }
            }
            // Process transfers
            for (auto& [transfer_from_stop, transfer_to_stops] : transfers) {
                auto transfer_from_journey = stop_labels[transfer_from_stop].get_label(n_round);
                if (transfer_from_journey.has_value()) {
                    for (auto& [transfer_to_stop, transfer_time] : transfer_to_stops) {
                        auto transfer_to_journey = stop_labels[transfer_to_stop].get_label(n_round);
                        //TODO: Make the time unit clear
                        auto arrival_time_with_transfer = transfer_from_journey->arrival_time + 60 * transfer_time;
                        if (!transfer_to_journey.has_value() || arrival_time_with_transfer < transfer_to_journey->arrival_time) {
                            stop_labels[transfer_to_stop].add_label(n_round, arrival_time_with_transfer,
                                                        transfer_from_journey->boarding_stop, transfer_from_journey->trip);
                        }
                    }
                }
            }
            ++n_round;
        }
        build_trip(origin, destination, stop_labels);
    }
} // namespace raptor
