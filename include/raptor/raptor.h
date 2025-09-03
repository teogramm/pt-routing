#ifndef RAPTOR_H
#define RAPTOR_H
#include "just_gtfs/just_gtfs.h"
#include "schedule/Schedule.h"

namespace raptor {


    struct JourneyInformation {
        int arrival_time;
        std::reference_wrapper<const Stop> boarding_stop;
        std::reference_wrapper<const Trip> trip;
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

        void add_label(const int n_transfers, const int arrival_time,
                       const Stop& stop, const Trip& trip) {
            // We might need to pad the vector, if we can only get to a stop for example with 3 changes
            if (n_transfers > labels.size()) {
                labels.insert(std::end(labels), n_transfers - labels.size(), std::nullopt);
            }
            labels.emplace(std::begin(labels) + n_transfers, JourneyInformation{arrival_time, stop, trip});
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
                           std::vector<std::pair<std::reference_wrapper<const Stop>, int>>> transfers;
        void calculate_transfers();

        const Schedule& schedule;

    public:
        explicit Raptor(const Schedule& schedule);
        void build_trip(const Stop& origin,
                        const Stop& destination, const std::unordered_map<std::reference_wrapper<const Stop>, RaptorLabel>& stop_labels);

        void route(const Stop& origin, const Stop& destination,
                   const std::chrono::zoned_time<std::chrono::seconds>& departure_time);
    };
}

#endif //RAPTOR_H
