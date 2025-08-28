#ifndef RAPTOR_H
#define RAPTOR_H
#include "just_gtfs/just_gtfs.h"
#include "schedule/Schedule.h"

namespace raptor {

    class RaptorLabel {
        std::vector<int> labels;

    public:
        RaptorLabel() = default;

        [[nodiscard]] std::optional<int> get_label(const int n_transfers) const {
            assert(n_transfers > 0);
            if (n_transfers > labels.size()) {
                return std::nullopt;
            }
            return labels[n_transfers];
        }

        void add_label(const int n_transfers, const int arrival_time) {
            assert(n_transfers == labels.size() + 1 || n_transfers == labels.size());
            labels.push_back(arrival_time);
        }
    };

    class Raptor {
        std::unordered_map<std::reference_wrapper<const Stop>, std::unordered_set<std::reference_wrapper<const Route>>>
        routes_serving_stop;
        void find_routes_serving_stop();

        std::unordered_map<std::reference_wrapper<const Stop>,
                           std::vector<std::pair<std::reference_wrapper<const Stop>, int>>> transfers;
        void calculate_transfers();

        const Schedule& schedule;

    public:
        explicit Raptor(const Schedule& schedule);

    };
}

#endif //RAPTOR_H
