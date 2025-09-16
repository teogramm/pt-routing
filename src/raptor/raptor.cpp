#include <ranges>
#include <utility>
#include <vector>

#include "raptor/raptor.h"

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

    Raptor::Raptor(const Schedule& schedule, TransferManager tm) :
        schedule(schedule), transfer_manager(std::move(tm)) {
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
                auto arrival_time = journey_to_here->arrival_time;
                journey.emplace(journey.begin(), WalkingMovement(boarding_stop, current_stop, {}, arrival_time));
            }

            current_stop = journey_to_here->boarding_stop.value();
            journey_to_here = stop_labels.get_latest_label(current_stop);
        }
        return journey;
    }

    void Raptor::process_transfers(RaptorState& status) {
        for (auto& origin_stop : status.get_improved_stops()) {
            auto arrival_time_to_origin = status.current_arrival_time_to_stop(origin_stop);
            for (auto [destination_stop, transfer_time] : transfer_manager.get_transfers_from_stop(origin_stop)) {
                auto arrival_time_with_transfer =
                        std::chrono::zoned_seconds(arrival_time_to_origin.get_time_zone(),
                                                   arrival_time_to_origin.get_sys_time() + transfer_time);
                status.try_improve_stop_arrival_time(destination_stop, arrival_time_with_transfer, origin_stop, std::nullopt);
            }
        }
    }

    void Raptor::process_route(const Route& route, const StopIndex hop_on_stop_idx, const Time hop_on_time,
                               RaptorState& status) {
        auto hop_on_stop = route.stop_sequence().at(hop_on_stop_idx);
        // Find the earliest trip of the route that we can hop on from this stop
        const auto& route_trips = route.get_trips();
        // TODO: Check with upper_bound
        if (auto trip = find_earliest_trip(route_trips, hop_on_time, hop_on_stop_idx);
            trip != route_trips.end()) {
            auto current_stop_idx = hop_on_stop_idx + 1;
            auto n_stops = trip->get_stop_times().size();
            auto trip_index = std::distance(route_trips.begin(), trip);
            // Iterate over all the next stops in the trip and update the arrival times
            while (current_stop_idx < n_stops) {
                const auto& current_stoptime = trip->get_stop_time(current_stop_idx);
                const auto& current_stop = current_stoptime.get_stop();
                const auto current_arrival_time = current_stoptime.get_arrival_time();
                const auto current_departure_time = current_stoptime.get_departure_time();

                // Try to improve the current journey
                auto improved = status.try_improve_stop_arrival_time(current_stop, current_arrival_time,
                                        hop_on_stop,
                                        std::make_pair(std::cref(route), trip_index));
                // If the optimal arrival time is before the current arrival time we might be able to catch
                // an earlier trip at that stop.
                // TODO: Check if this works
                if (!improved && status.might_catch_earlier_trip(current_stop, current_departure_time)) {
                    auto earlier_trip =
                            find_earliest_trip(route_trips,
                                               status.previous_arrival_time_to_stop(current_stop),
                                               current_stop_idx);
                    assert(earlier_trip != route_trips.end());
                    // From now on we are following a different trip
                    if (earlier_trip != trip) {
                        trip = earlier_trip;
                        trip_index = std::ranges::distance(route_trips.begin(), trip);
                        hop_on_stop = current_stop;
                    }
                }
                ++current_stop_idx;
            }
        }
    }


    std::vector<Movement> Raptor::route(const Stop& origin, const Stop& destination,
                                        const Time& departure_time) {
        auto status = RaptorState{origin, destination, departure_time};
        /* Since we don't consider a foot transfer to actually count as a transfer we must process all transfers from
         * the origin stop here, otherwise they will never be processed. */
        process_transfers(status);
        while (status.have_stops_to_improve()) {
            status.new_round();
            // Second stage: Traverse all routes
            auto current_round_routes = find_routes_to_examine(status.get_and_clear_improved_stops());
            for (auto& [route, stop_index] : current_round_routes) {
                auto hop_on_stop = route.get().stop_sequence().at(stop_index);
                const auto hop_on_time = status.previous_arrival_time_to_stop(hop_on_stop);
                process_route(route, stop_index, hop_on_time, status);
            }
            // Third stage: Process transfers
            process_transfers(status);
        }
        return build_trip(origin, destination, status.get_label_manager());
    }
} // namespace raptor
