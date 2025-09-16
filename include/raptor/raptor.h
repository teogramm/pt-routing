#ifndef RAPTOR_H
#define RAPTOR_H

#include <ranges>
#include <unordered_map>

#include "reconstruction.h"
#include "schedule/Schedule.h"
#include "raptor/state.h"

namespace raptor {

    class Raptor {

        // Make the reference wrapper const so it can be used to directly extract pairs from the unordered_map
        using RouteWithStopIndex = std::pair<const std::reference_wrapper<const Route>, StopIndex>;

        std::unordered_map<std::reference_wrapper<const Stop>, std::vector<RouteWithStopIndex>>
        routes_serving_stop;
        /**
         * Calculates which routes serve every stop. Includes only stops which are served by some route.
         */
        void build_routes_serving_stop();

        using StopWithDuration = std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>;

        std::unordered_map<std::reference_wrapper<const Stop>,
                           std::vector<StopWithDuration>> transfers;
        void build_transfers();
        void build_same_station_transfers();

        /**
         * Builds on-foot transfers between stops that do not have an existing transfer
         */
        void build_on_foot_transfers(double max_radius_km = 1);

        const Schedule& schedule;

        /**
         * Find the earliest trip which departs from the given stop after the given departure time.
         * @param route_trips Range with Trip objects of a specific route, sorted by ascending departure time from the
         * route's origin.
         * @param departure_time Departure time from the given stop.
         * @param stop_index Index of the stop in the route.
         * @return Iterator to the route_trips range, pointing to the found trip. If no trip was found, iterator
         * pointing to route_trips.end().
         */
        template <std::ranges::random_access_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, const Trip&> &&
            requires(std::ranges::range_value_t<R> trip)
            {
                { trip.get_stop_times() } -> std::ranges::random_access_range;
                requires std::is_convertible_v<std::ranges::range_value_t<decltype(trip.get_stop_times())>,
                                               const StopTime&>;
            }
        static std::ranges::borrowed_iterator_t<R> find_earliest_trip(
                R&& route_trips,
                const std::chrono::zoned_seconds& departure_time,
                const StopIndex stop_index) {
            return std::ranges::find_if(route_trips, [&departure_time](const auto& trip_departure_time) {
                                            return trip_departure_time.get_sys_time() >= departure_time.get_sys_time();
                                        }, [&stop_index](const Trip& trip) {
                                            return std::ranges::next(trip.get_stop_times().begin(), stop_index)->
                                                    get_departure_time();
                                        });
        }

        std::vector<Movement> build_trip(const Stop& origin,
                                         const Stop& destination,
                                         const LabelManager& stop_labels);
        void process_transfers(RaptorState& status);

        void process_route(const Route& route, StopIndex hop_on_stop_idx, Time hop_on_time, RaptorState& status);


        template <std::ranges::input_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, const Stop>
        std::vector<RouteWithStopIndex>
        find_routes_to_examine(R&& improved_stops) {
            auto route_to_earliest_stop = std::unordered_map<
                std::remove_const_t<RouteWithStopIndex::first_type>, RouteWithStopIndex::second_type>();
            for (const Stop& stop : improved_stops) {
                // It is possible that a stop is not served by any route but can be accessed only on foot.
                auto& routes_for_stop = routes_serving_stop[stop];
                for (auto [route, stop_index] : routes_for_stop) {
                    auto current_stop_index = route_to_earliest_stop.find(route);
                    auto new_route = current_stop_index == route_to_earliest_stop.end();
                    auto earlier_stop = !new_route && current_stop_index->second > stop_index;
                    if (new_route || earlier_stop) {
                        route_to_earliest_stop.insert_or_assign(route, stop_index);
                    }
                }
            }
            return {std::make_move_iterator(route_to_earliest_stop.begin()),
                    std::make_move_iterator(route_to_earliest_stop.end())};
        }

    public:
        explicit Raptor(const Schedule& schedule);


        std::vector<Movement> route(const Stop& origin, const Stop& destination,
                                    const Time& departure_time);
    };
}

#endif //RAPTOR_H
