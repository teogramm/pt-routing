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
        calculate_transfers();
    }

    void Raptor::route(const Stop& origin, const Stop& destination,
                       const std::chrono::zoned_time<std::chrono::seconds>& departure_time) {
        int n_round = 0;
        auto stop_labels = std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>();
        stop_labels[origin].add_label(0, departure_time.get_sys_time().time_since_epoch().count(), origin);
        n_round++;
        bool improved = true;
        for (int i = 0; i < 5; i++) {
            improved = false;
            // First stage: set t_k = t_k-1
            std::ranges::for_each(stop_labels | std::views::values, [n_round](RaptorLabel& stop_label) {
                                      auto prev_label = stop_label.get_label(n_round - 1);
                                      if (prev_label.has_value())
                                          stop_label.add_label(n_round,
                                                               prev_label.value().first, prev_label.value().second);
                                  }
                    );
            // Traverse all routes
            for (const auto& route : schedule.get_routes()) {
                // Find a stop from this route that we can hop on
                // TODO: What if we can hop on two stops from a route?
                auto route_stops = route.stop_sequence();
                auto hop_on_stop = std::ranges::find_if(route_stops, [n_round, &stop_labels](const auto& stop) {
                    auto hop_on_time = stop_labels[stop].get_label(n_round - 1);
                    return hop_on_time.has_value();
                });
                if (hop_on_stop != route_stops.end()) {
                    auto hop_on_time = stop_labels[*hop_on_stop].get_label(n_round - 1).value();
                    // Find the earliest trip we can hop on
                    const auto& route_trips = route.get_trips();
                    // TODO: Check with upper_bound
                    // Trips must be sorted by departure time. Assumes that a trip departing later from the first stop
                    // cannot arrive earlier at another stop.
                    auto earliest_trip = std::ranges::find_if(route_trips,
                                                              [hop_on_time, hop_on_stop](const auto& trip) {
                                                                  const auto& stop_times = trip.get_stop_times();
                                                                  auto departure_time = std::ranges::find_if(
                                                                          stop_times, [hop_on_time](
                                                                          const auto& dep_time) {
                                                                              return dep_time.get_sys_time().
                                                                                      time_since_epoch().count() >
                                                                                      hop_on_time.first;
                                                                          }, &StopTime::get_departure_time);
                                                                  return departure_time != stop_times.end();
                                                              });
                    if (earliest_trip != route_trips.end()) {
                        auto& trip = *earliest_trip;
                        auto stop_pos = std::ranges::find(trip.get_stop_times(), *hop_on_stop, &StopTime::get_stop);
                        auto end_guard = trip.get_stop_times().end();
                        // Iterate over all the next stops in the trip and update the arrival times
                        while (stop_pos != end_guard) {
                            auto& stop = stop_pos->get_stop();
                            const auto new_arrival_time = stop_pos->
                                                          get_arrival_time().get_sys_time().time_since_epoch().
                                                          count();
                            const auto old_arrival_time = stop_labels[stop].get_label(n_round - 1);
                            if (!old_arrival_time.has_value() || new_arrival_time < old_arrival_time->first) {
                                stop_labels[stop].add_label(n_round, new_arrival_time, stop);
                                improved = true;
                            }
                            // TODO: If the optimal arrival time is before the current arrival time we might be able to catch
                            // an earlier trip at that stop
                            else if (old_arrival_time->first < new_arrival_time) {
                                auto earlier_trip = std::ranges::find_if(route_trips,
                                                                         [old_arrival_time, &stop](const auto& trip) {
                                                                             const auto& stop_times = trip.
                                                                                     get_stop_times();
                                                                             auto departure_time = std::ranges::find_if(
                                                                                     stop_times, [old_arrival_time](
                                                                                     const auto& dep_time) {
                                                                                         return dep_time.get_sys_time().
                                                                                                 time_since_epoch().
                                                                                                 count()
                                                                                                 > old_arrival_time->
                                                                                                 first;
                                                                                     }, &StopTime::get_departure_time);
                                                                             return departure_time != stop_times.end();
                                                                         });
                                // From now on we are following a different trip
                                if (earlier_trip != earliest_trip) {
                                    earliest_trip = earlier_trip;
                                    stop_pos = std::ranges::find(earliest_trip->get_stop_times(), *hop_on_stop,
                                                                 &StopTime::get_stop);
                                    end_guard = earliest_trip->get_stop_times().end();
                                }
                            }
                            ++stop_pos;
                        }
                    }
                }
            }
            // Process transfers
            for (auto& [from, destinations] : transfers) {
                auto from_arrival_time = stop_labels[from].get_label(n_round);
                if (from_arrival_time.has_value()) {
                    for (auto& destination : destinations) {
                        auto& stop = destination.first;
                        auto transfer_time = destination.second;
                        auto existing_time = stop_labels[stop].get_label(n_round);
                        //TODO: Make the time unit clear
                        auto arrival_time_with_transfer = from_arrival_time->first + 60 * transfer_time;
                        if (!existing_time.has_value() || arrival_time_with_transfer < existing_time->first) {
                            stop_labels[stop].add_label(n_round, arrival_time_with_transfer, from_arrival_time->second);
                        }
                    }
                }
            }
            ++n_round;
        }
        return;
    }
} // namespace raptor
