#ifndef RAPTOR_H
#define RAPTOR_H
#include <ranges>

#include "just_gtfs/just_gtfs.h"
#include "schedule/Schedule.h"

namespace raptor {


    struct JourneyInformation {
        std::chrono::zoned_seconds arrival_time;
        std::reference_wrapper<const Stop> boarding_stop;
        std::optional<std::reference_wrapper<const Route>> route;
    };

    /**
     * Represents a Raptor multilabel for a given stop, which contains information about how to reach that stop
     * with a certain number of transfers.
     *
     * To modify the object two actions can be performed:
     * * Extending the label by copying the current last value.
     * * Changing the content of the last value.
     * This way once the label is extended, all but the latest value become read-only.
     */
    class RaptorLabel {
        // TODO: Move responsibility for ensuring the labels are all the same size to the LabelManager.
        std::vector<std::optional<JourneyInformation>> labels;

        /**
         * Modify the last value of the label.
         * @param label Information about reaching the object with at most n_transfers.
         */
        void change_value(const std::optional<JourneyInformation>& label) {
            if (labels.empty()) {
                throw std::runtime_error("raptor::RaptorLabel: attempt to change the value of an empty label");
            }
            labels.back() = label;
        }

    public:
        RaptorLabel() = default;

        /**
         * Creates a label containing empty values up to and including the given number of transfers.
         */
        explicit RaptorLabel(const int n_transfers) {
            labels = std::vector<std::optional<JourneyInformation>>(n_transfers + 1, std::nullopt);
        }

        /**
         * Get the label associated with the given number of transfers.
         * @param n_transfers The maximum number of transfers required for reaching the stop.
         * @return Optional that contains a value if journey information exists for reaching the stop with the
         * given number of transfers.
         */
        [[nodiscard]] std::optional<JourneyInformation>
        get_label(const int n_transfers) const {
            if (labels.empty()) {
                return std::nullopt;
            }
            if (n_transfers < labels.size() && n_transfers >= 0) {
                return labels.at(n_transfers);
            }
            throw std::out_of_range("");
        }

        /**
         * Extends the label by one value by copying the content of the current latest value.
         * @return Number of transfers the new label corresponds to.
         */
        int extend() {
            auto previous_label = labels.back();
            labels.emplace_back(previous_label);
            return labels.size() - 1;
        }

        /**
         * Changes the latest value of the latest label.
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route Route used to travel between the stops.
         * @return Number of transfers the changed label corresponds to.
         */
        int change_value(const std::chrono::zoned_seconds& arrival_time,
                         const Stop& boarding_stop, const std::optional<std::reference_wrapper<const Route>> route) {
            auto label = std::make_optional<JourneyInformation>(arrival_time, boarding_stop, route);
            change_value(label);
            // Changing the value requires having at least one element, so no underflow.
            return labels.size() - 1;
        }
    };

    /**
     * LabelManager is responsible for controlling the labels associated with each Stop.
     * It also keeps track of the current round of the algorithm, ensuring that:
     *
     * * All labels have the same number of values.
     * * Only the latest value of each label can be edited.
     *
     * The round of the algorithm corresponds to the maximum number of transfers that can be used to reach a given stop.
     */
    class LabelManager {
        // Should ensure that labels for all stops are the same size
        using LabelContainer = std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>;

        LabelContainer stop_labels;
        int round = 0;


        /**
         * Returns the label associated with the given stop. If the stop does not have a label currently associated
         * with it, creates a new label with empty values, ready to be used at this round.
         */
        RaptorLabel& get_or_insert_label(const Stop& stop) {
            auto [label, inserted] = stop_labels.try_emplace(stop, RaptorLabel(round));
            return label->second;
        }

        /**
         * Extends all labels by one value, copying the content of their current last value.
         */
        void extend_labels() {
            for (auto& label : stop_labels | std::ranges::views::values) {
                label.extend();
            }
        }

    public:
        /**
         * LabelManager starts at round 0 (corresponding to 0 transfers).
         */
        LabelManager() = default;

        /**
         * Starts a new round.
         * @return Round number, corresponding to the maximum number of transfers required to reach a stop.
         */
        int new_round() {
            extend_labels();
            return ++round;
        }

        /**
         * Adds or changes the value of the label for the current round.
         * @param stop
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route Route used to travel between the stops.
         */
        void add_label(const Stop& stop,
                       const std::chrono::zoned_seconds& arrival_time, const Stop& boarding_stop,
                       const std::optional<std::reference_wrapper<const Route>> route) {
            auto& stop_label = get_or_insert_label(stop);
            stop_label.change_value(arrival_time, boarding_stop, route);
        }

        std::optional<JourneyInformation> get_label(const int n_transfers, const Stop& stop) const {
            if (!stop_labels.contains(stop))
                return std::nullopt;
            return stop_labels.at(stop).get_label(n_transfers);
        }

        std::optional<JourneyInformation> get_latest_label(const Stop& stop) const {
            return get_label(round, stop);
        }

        /**
         * Get the first stop of the route that can be reached using at most the given number of transfers.
         * @param stops Range containing stop objects for this route in travel order.
         * @param n_transfers Maximum number of transfers to reach the given stop.
         * @return Pair containing a reference to the stop and the arrival time to the stop. No value if there is no
         * stop in the given route that can be reached with the given number of transfers.
         */
        template <std::ranges::input_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, Stop>
        std::optional<std::pair<std::ranges::range_difference_t<R>, std::chrono::zoned_seconds>> find_hop_on_stop(
                R&& stops,
                const int n_transfers) {
            auto has_journey_at_n_transfers = [n_transfers](const LabelContainer::value_type& label) {
                return label.second.get_label(n_transfers).has_value();
            };
            // TODO: Make this a set and construct it at the start of the round. Also, performance comparison.
            auto stops_with_journeys = stop_labels | std::views::filter(has_journey_at_n_transfers);
            auto hop_on_stop = std::ranges::find_first_of(stops, stops_with_journeys,
                                                          {}, {}, &LabelContainer::value_type::first);
            if (hop_on_stop == std::ranges::end(stops)) {
                return std::nullopt;
            }
            return std::make_pair(std::ranges::distance(std::ranges::begin(stops), hop_on_stop),
                                  stop_labels[*hop_on_stop].get_label(n_transfers)->arrival_time);
        }
    };

    class Raptor {
        // TODO: These two are not needed
        std::unordered_map<std::reference_wrapper<const Stop>, std::unordered_set<std::reference_wrapper<const Route>>>
        routes_serving_stop;
        void find_routes_serving_stop();

        // TODO: Make types clearer
        std::unordered_map<std::reference_wrapper<const Stop>,
                           std::vector<std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>>> transfers;
        void build_transfers();
        void build_same_station_transfers();

        /**
         * Builds on-foot transfers between stops that do not have an existing transfer
         */
        void build_on_foot_transfers(double max_radius_km = 1);

        const Schedule& schedule;

        /**
         * Find the earliest trip which departs from the given stop after the given departure time.
         * @param route_trips Range with trips of a route, sorted by ascending departure time from the route's origin.
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
                const std::ranges::range_difference_t<decltype(std::declval<Trip>().get_stop_times())> stop_index) {
            return std::ranges::find_if(route_trips, [&departure_time](const auto& trip_departure_time) {
                                            return trip_departure_time.get_sys_time() > departure_time.get_sys_time();
                                        }, [&stop_index](const Trip& trip) {
                                            return std::ranges::next(trip.get_stop_times().begin(), stop_index)->
                                                    get_departure_time();
                                        });
        }

    public:
        explicit Raptor(const Schedule& schedule);
        void build_trip(const Stop& origin,
                        const Stop& destination,
                        const LabelManager& stop_labels, int n_rounds);
        void process_transfers(LabelManager& stop_labels, int n_round);
        void process_route(const Route& route, size_t hop_on_stop_idx, std::chrono::zoned_seconds hop_on_time, LabelManager& stop_labels, int
                           n_rounds);


        void route(const Stop& origin, const Stop& destination,
                   const std::chrono::zoned_time<std::chrono::seconds>& departure_time);
    };
}

#endif //RAPTOR_H
