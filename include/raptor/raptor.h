#ifndef RAPTOR_H
#define RAPTOR_H
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

    public:
        RaptorLabel() = default;

        [[nodiscard]] std::optional<JourneyInformation>
        get_label(const int n_transfers) const {
            assert(n_transfers >= 0);
            // size() - 1 overflows when size is 0
            //
            if (labels.empty() || n_transfers > (labels.size() - 1)) {
                return std::nullopt;
            }
            return labels[n_transfers];
        }

        [[nodiscard]] std::optional<JourneyInformation>
        get_label() const {
            if (labels.empty()) {
                return std::nullopt;
            }
            return labels.back();
        }

        void add_label(const int n_transfers, const std::chrono::zoned_seconds& arrival_time,
                       const Stop& stop) {
            // We might need to pad the vector, if we can only get to a stop for example with 3 changes
            auto information = std::make_optional<JourneyInformation>(arrival_time, stop);
            add_label(n_transfers, information);
        }

        void add_label(const int n_transfers, const std::optional<JourneyInformation>& label) {
            // We might need to pad the vector, if we can only get to a stop for example with 3 changes
            if (n_transfers > labels.size()) {
                labels.insert(std::end(labels), n_transfers - labels.size(), std::nullopt);
            }
            labels.emplace(std::begin(labels) + n_transfers, label);
        }
    };

    class Raptor {
        // TODO: These two are not needed
        std::unordered_map<std::reference_wrapper<const Stop>, std::unordered_set<std::reference_wrapper<const Route>>>
        routes_serving_stop;
        void find_routes_serving_stop();

        std::unordered_map<std::reference_wrapper<const Stop>,
                           std::vector<std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>>> transfers;
        void calculate_transfers();

        const Schedule& schedule;

    public:
        explicit Raptor(const Schedule& schedule);
        void build_trip(const Stop& origin,
                        const Stop& destination,
                        const std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>& stop_labels);

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
