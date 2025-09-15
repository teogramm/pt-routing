#ifndef RAPTOR_H
#define RAPTOR_H

#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include "reconstruction.h"
#include "schedule/Schedule.h"

namespace raptor {

    using StopIndex = std::ranges::range_difference_t<decltype(std::declval<Route>().stop_sequence())>;
    using TripIndex = std::ranges::range_difference_t<decltype(std::declval<Route>().get_trips())>;
    using StopTimeIndex = std::ranges::range_difference_t<decltype(std::declval<Trip>().get_stop_times())>;

    /**
     * Contains information about reaching a stop. The algorithm supports two types of reaching a stop: either on
     * foot or using public transport. When travelling on foot the route_and_trip_index member is set to std::nullopt.
     *
     * The boarding_stop member is set to std::nullopt for the starting point of the journey.
     */
    struct JourneyInformation {
        Time arrival_time;
        std::optional<std::reference_wrapper<const Stop>> boarding_stop;
        std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>> route_and_trip_index;
    };

    struct JourneyInformationPTExtension {
        const Route& route;
        TripIndex trip_index;
        StopIndex stop_index;
    };

    /**
     * LabelManager is responsible for controlling the labels associated with each Stop.
     * It keeps track of two sets of the current and previous sets of labels.
     *
     * All modifications to the labels apply to the current set.
     */
    class LabelManager {
        // Should ensure that labels for all stops are the same size
        using LabelContainer = std::unordered_map<std::reference_wrapper<const Stop>, JourneyInformation>;

        LabelContainer current_round_labels;
        LabelContainer previous_round_labels;

        static std::optional<JourneyInformation> get_label(const Stop& stop, const LabelContainer& labels) {
            const auto label = labels.find(stop);
            return label == labels.end() ? std::nullopt : std::make_optional(label->second);
        }

    public:
        LabelManager() = default;

        /**
         * Initialises the object and adds a label to the current set.
         */
        LabelManager(const Stop& stop,
                     const Time& arrival_time,
                     const std::optional<std::reference_wrapper<const Stop>> boarding_stop,
                     const std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>>
                     route_with_trip_index) {
            add_label(stop, arrival_time, boarding_stop, route_with_trip_index);
        }

        /**
         * Copies the labels of the current round to the previous round
         */
        void new_round() {
            previous_round_labels = current_round_labels;
        }

        /**
         * Adds or changes the value of the label for the latest set.
         * @param stop
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route_with_trip_index Route used to travel between the stops along with the index of the trip used.
         */
        void add_label(const Stop& stop,
                       const Time& arrival_time,
                       const std::optional<std::reference_wrapper<const Stop>> boarding_stop,
                       const std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>>
                       route_with_trip_index) {
            current_round_labels.insert_or_assign(stop, JourneyInformation(arrival_time, boarding_stop, route_with_trip_index));
        }

        std::optional<JourneyInformation> get_latest_label(const Stop& stop) const {
            return get_label(stop, current_round_labels);
        }

        std::optional<JourneyInformation> get_previous_label(const Stop& stop) const {
            return get_label(stop, previous_round_labels);
        }

        using IndexWithTime = std::pair<StopIndex, Time>;

        /**
         * Get the first stop of the route that was reached in the previous round.
         * @param stops Range containing stop objects for this route in travel order.
         * @return Pair containing a reference to the stop and the arrival time to the stop. No value if there is no
         * stop in the given route that can be reached with the given number of transfers.
         */
        template <std::ranges::input_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, Stop>
        std::optional<IndexWithTime> find_hop_on_stop(
                R&& stops) {
            auto stops_with_journeys = previous_round_labels | std::views::keys;
            auto hop_on_stop = std::ranges::find_first_of(stops, stops_with_journeys,
                                                          {}, {}, &LabelContainer::value_type::first);
            if (hop_on_stop == std::ranges::end(stops)) {
                return std::nullopt;
            }
            return std::make_pair(std::ranges::distance(std::ranges::begin(stops), hop_on_stop),
                                  previous_round_labels.at(*hop_on_stop).arrival_time);
        }
    };

    class Raptor {

        // struct RouteWithStopIndex {
        //     std::reference_wrapper<const Route> route;
        //     StopIndex stop_idx;
        // };

        // Make the reference wrapper const so it can be used to directly etract pairs from the unordered_map
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

        struct RaptorStatus {
            LabelManager label_manager;
            std::unordered_map<std::reference_wrapper<const Stop>, Time> earliest_arrival_time;
            std::unordered_set<std::reference_wrapper<const Stop>> improved_stops;
            int n_round;
        };

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
        static std::ranges::borrowed_iterator_t<R&> find_earliest_trip(
                R&& route_trips,
                const std::chrono::zoned_seconds& departure_time,
                const StopIndex stop_index) {
            return std::ranges::find_if(route_trips, [&departure_time](const auto& trip_departure_time) {
                                            return trip_departure_time.get_sys_time() > departure_time.get_sys_time();
                                        }, [&stop_index](const Trip& trip) {
                                            return std::ranges::next(trip.get_stop_times().begin(), stop_index)->
                                                    get_departure_time();
                                        });
        }

        std::vector<Movement> build_trip(const Stop& origin,
                                         const Stop& destination,
                                         const LabelManager& stop_labels);
        void process_transfers(RaptorStatus& status);

        void process_route(const Route& route, StopIndex hop_on_stop_idx, Time hop_on_time, RaptorStatus& status,
                           const Stop& destination);


        template <std::ranges::input_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, const Stop>
        std::vector<RouteWithStopIndex>
        find_routes_to_examine(R&& improved_stops) {
            auto route_to_earliest_stop = std::unordered_map<
                std::remove_const_t<RouteWithStopIndex::first_type>, RouteWithStopIndex::second_type>();
            for (const Stop& stop : improved_stops) {
                // It is possible that a stop is not served by any route but can be accessed only on foot.
                // As such, use
                auto& routes_for_stop = routes_serving_stop[stop];
                for (auto& [route, stop_index] : routes_for_stop) {
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

        static bool can_improve_current_journey_to_stop(const Time& new_arrival_time, const Stop& current_stop,
                                                        const Stop& destination, const RaptorStatus& status);

    public:
        explicit Raptor(const Schedule& schedule);


        std::vector<Movement> route(const Stop& origin, const Stop& destination,
                                    const Time& departure_time);
    };
}

#endif //RAPTOR_H
