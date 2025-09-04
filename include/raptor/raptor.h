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
     * * Extending the label, either by copying the last value or by setting it to an empty optional.
     * * Changing the content of the last value.
     * This way once the label is extended, all but the latest value become read-only.
     */
    class RaptorLabel {
        // TODO: Move responsibility for ensuring the labels are all the same size to the LabelManager.
        std::vector<std::optional<JourneyInformation>> labels;

        /**
         * Extends the label by adding the given value to the end of the label.
         * @param n_transfers The number of transfers that the given value refers to. Used for verification that
         * the object is being used correctly.
         * @param value Information about reaching the object with at most n_transfers.
         */
        void add_value(const int n_transfers, const std::optional<JourneyInformation>& value) {
            if(n_transfers != labels.size()) {
                throw std::invalid_argument(
                    // TODO: Maybe these checks should be carried out by the LabelManager
                    "A new label can only be added for a larger n_transfers than the label already contains.");
            }
            labels.emplace_back(value);
        }

        /**
         * Modify an existing value of the label. Only the latest value added to the label (with the highest
         * n_transfers) can be modified.
         * @param n_transfers The number of transfers that the given value refers to. Used for verification that
         * the object is being used correctly.
         * @param label Information about reaching the object with at most n_transfers.
         */
        void change_value(const int n_transfers, const std::optional<JourneyInformation>& label) {
            if (n_transfers + 1 != labels.size()) {
                throw std::invalid_argument("Only the last label can be modified.");
            }
            labels.at(n_transfers) = label;
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
            if (n_transfers < labels.size() && n_transfers > 0) {
                return labels.at(n_transfers);
            }
            throw std::out_of_range("");
        }

        /**
         * Extends the label by adding a new value for the given number of transfers.
         * @param n_transfers The number of transfers that the given value refers to. Used for verification that
         * the object is being used correctly.
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route Route used to travel between the stops.
         */
        void add_value(const int n_transfers, const std::chrono::zoned_seconds& arrival_time,
                       const Stop& boarding_stop, const std::optional<std::reference_wrapper<const Route>> route) {
            auto label = std::make_optional<JourneyInformation>(arrival_time, boarding_stop, route);
            add_value(n_transfers, label);
        }

        /**
         * Changes the latest value of the label.
         * @param n_transfers The number of transfers that the given value refers to. Used for verification that
         * the object is being used correctly.
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route Route used to travel between the stops.
         */
        void change_value(const int n_transfers, const std::chrono::zoned_seconds& arrival_time,
                       const Stop& boarding_stop, const std::optional<std::reference_wrapper<const Route>> route) {
            auto label = std::make_optional<JourneyInformation>(arrival_time, boarding_stop, route);
            change_value(n_transfers, label);
        }

        /**
         * Extends the label by copying the latest value.
         * @param n_transfers Used for verification that the object is being used correctly.
         */
        void copy_latest_value(const int n_transfers) {
            assert(!labels.empty() && n_transfers == labels.size());
            auto previous_label = labels.back();
            add_value(n_transfers, previous_label);
        }
    };

    /**
     * LabelManager is responsible for controlling the labels associated with each Stop.
     */
    class LabelManager {
        // Should ensure that labels for all stops are the same size
        using LabelContainer = std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>;

        LabelContainer stop_labels;

        // TODO: Better function names
        RaptorLabel& get_or_insert_label(const int n_transfers, const Stop& stop) {
            auto [label, inserted] = stop_labels.try_emplace(stop, RaptorLabel(n_transfers));
            return label->second;
        }

    public:
        LabelManager() = default;

        void extend_labels(const int n_transfers) {
            for (auto& label : stop_labels | std::ranges::views::values) {
                label.copy_latest_value(n_transfers);
            }
        }

        void add_label(const Stop& stop, const int n_transfers,
                       const std::chrono::zoned_seconds& arrival_time, const Stop& boarding_stop,
                       const std::optional<std::reference_wrapper<const Route>> route) {
            auto& stop_label = get_or_insert_label(n_transfers, stop);
            stop_label.add_value(n_transfers, arrival_time, boarding_stop, route);
        }

        std::optional<JourneyInformation> get_label(const int n_transfers, const Stop& stop) const {
            if (!stop_labels.contains(stop))
                return std::nullopt;
            return stop_labels.at(stop).get_label(n_transfers);
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
        std::optional<std::pair<std::reference_wrapper<const Stop>, std::chrono::zoned_seconds>> find_hop_on_stop(
                R&& stops,
                const int n_transfers) {
            auto has_journey_at_n_transfers = [n_transfers](const LabelContainer::value_type& label) {
                return label.second.get_label(n_transfers).has_value();
            };
            auto stops_with_journeys = stop_labels | std::views::filter(has_journey_at_n_transfers);
            auto hop_on_stop = std::ranges::find_first_of(stops_with_journeys, stops,
                                                {}, [](const auto& pair) {
                                                    return pair.first;
                                                });
            if (hop_on_stop == stops_with_journeys.end()) {
                return std::nullopt;
            }
            return std::make_pair(hop_on_stop->first, hop_on_stop->second.get_label(n_transfers)->arrival_time);
        }
    };

    class Raptor {
        // TODO: These two are not needed
        std::unordered_map<std::reference_wrapper<const Stop>, std::unordered_set<std::reference_wrapper<const Route>>>
        routes_serving_stop;
        void find_routes_serving_stop();

        std::unordered_map<std::reference_wrapper<const Stop>,
                           std::vector<std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>>> transfers;
        void build_transfers();

        const Schedule& schedule;

    public:
        explicit Raptor(const Schedule& schedule);
        void build_trip(const Stop& origin,
                        const Stop& destination,
                        const LabelManager& stop_labels, int n_rounds);
        void process_transfers(LabelManager& stop_labels, int n_round);

        template <std::ranges::random_access_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, Trip>
        std::ranges::borrowed_iterator_t<R&> find_earliest_trip(R&& route_trips,
                                                                const std::chrono::zoned_seconds& departure_time,
                                                                const Stop& origin) {
            return std::ranges::find_if(route_trips,
                                        [&departure_time](const auto& trip_departure_time) {
                                            return trip_departure_time > departure_time.get_sys_time();
                                        }, [&origin](const Trip& trip) {
                                            return trip.get_stop_time(origin).get_departure_time().get_sys_time();
                                        });
        }

        void route(const Stop& origin, const Stop& destination,
                   const std::chrono::zoned_time<std::chrono::seconds>& departure_time);
    };
}

#endif //RAPTOR_H
