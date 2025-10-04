#ifndef PT_ROUTING_CONSTRUCTION_H
#define PT_ROUTING_CONSTRUCTION_H

#include <schedule/Schedule.h>

namespace raptor {
    /**
     * Helper class for building stations by incrementally adding stops and entrances.
     * A station does not own its stops, but contains only references to them.
     * A station owns its entrances.
     */
    class StationBuilder {
        // TODO: Move to separate header
        std::vector<std::reference_wrapper<const Stop>> stops;
        std::vector<StationEntrance> entrances;
        std::string gtfs_id;
        std::string name;

    public:
        StationBuilder() = delete;

        explicit StationBuilder(std::string gtfs_id) :
            gtfs_id(std::move(gtfs_id)) {
        }

        StationBuilder(std::string name, std::string gtfs_id) :
            gtfs_id(std::move(gtfs_id)), name(std::move(name)) {
        }

        void set_name(std::string new_name) {
            this->name = std::move(new_name);
        }

        void add_stop(const Stop& stop) {
            stops.emplace_back(std::cref(stop));
        }

        void add_entrance(StationEntrance entrance) {
            entrances.emplace_back(std::move(entrance));
        }

        Station build() {
            return {std::move(name), std::move(gtfs_id), std::move(stops), std::move(entrances)};
        }
    };
}
#endif //PT_ROUTING_CONSTRUCTION_H
