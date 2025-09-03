#ifndef RAPTOR_H
#define RAPTOR_H
#include <ranges>

#include "just_gtfs/just_gtfs.h"
#include "schedule/Schedule.h"

namespace raptor {


    struct JourneyInformation {
        std::chrono::zoned_seconds arrival_time;
        std::reference_wrapper<const Stop> boarding_stop;
        // std::reference_wrapper<const Route> route;
    };

    class RaptorLabel {
        std::vector<std::optional<JourneyInformation>> labels;

        void add_label(const int n_transfers, const std::optional<JourneyInformation>& label) {
            // We can only add a new label to the end or update the latest one
            assert(n_transfers == labels.size() || (!labels.empty() && n_transfers == (labels.size() - 1)));
            // Check whether we are extending or changing an existing element
            // TODO: Split functions
            if (n_transfers == labels.size()) {
                labels.emplace_back(label);
            }else if (n_transfers < labels.size()) {
                labels.at(n_transfers) = label;
            }
        }

    public:
        RaptorLabel() = default;

        explicit RaptorLabel(const int n_transfers) {
            labels = std::vector<std::optional<JourneyInformation>>(n_transfers + 1, std::nullopt);
        }

        [[nodiscard]] std::optional<JourneyInformation>
        get_label(const int n_transfers) const {
            assert(n_transfers >= 0);
            // size() - 1 overflows when size is 0
            //
            if (labels.empty()) {
                return std::nullopt;
            }
            return labels[n_transfers];
        }

        // [[nodiscard]] std::optional<JourneyInformation>
        // get_label() const {
        //     if (labels.empty()) {
        //         return std::nullopt;
        //     }
        //     return labels.back();
        // }

        void add_label(const int n_transfers, const std::chrono::zoned_seconds& arrival_time,
                       const Stop& boarding_stop) {
            auto label = std::make_optional<JourneyInformation>(arrival_time, boarding_stop);
            add_label(n_transfers, label);
        }

        /**
         * Copy the value from n_transfers-1
         */
        void add_label(const int n_transfers) {
            assert(!labels.empty() && n_transfers == labels.size());
            auto previous_label = labels.back();
            add_label(n_transfers, previous_label);
        }
    };

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
                label.add_label(n_transfers);
            }
        }

        void add_label(const int n_transfers, const std::chrono::zoned_seconds& arrival_time,
                       const Stop& stop, const Stop& boarding_stop) {
            auto& stop_label = get_or_insert_label(n_transfers, stop);
            stop_label.add_label(n_transfers, arrival_time, boarding_stop);
        }

        std::optional<JourneyInformation> get_label(const int n_transfers, const Stop& stop) const {
            if (!stop_labels.contains(stop))
                return std::nullopt;
            return stop_labels.at(stop).get_label(n_transfers);
        }

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
