#ifndef PT_ROUTING_STOP_H
#define PT_ROUTING_STOP_H

namespace raptor {
    class Stop {
        std::string name;
        std::string gtfs_id;
        std::string parent_stop_id;
        std::string platform_code;

        struct {
            double latitude;
            double longitude;
        } coordinates;

    public:
        Stop(std::string name, std::string gtfs_id, const double latitude, const double longitude,
             std::string parent_stop_id, std::string platform_code) :
            name(std::move(name)),
            gtfs_id(std::move(gtfs_id)),
            parent_stop_id(std::move(parent_stop_id)),
            platform_code(std::move(platform_code)),
            coordinates({latitude, longitude}) {
        }

        [[nodiscard]] const std::string& get_name() const {
            return name;
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] std::pair<double, double> get_coordinates() const {
            return std::make_pair(coordinates.latitude, coordinates.longitude);
        }

        [[nodiscard]] const std::string& get_parent_stop_id() const {
            return parent_stop_id;
        }

        [[nodiscard]] const std::string& get_platform_code() const {
            return platform_code;
        }

        friend bool operator==(const Stop& lhs, const Stop& rhs) {
            return lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const Stop& lhs, const Stop& rhs) {
            return !(lhs == rhs);
        }
    };

    using StationEntrance = Stop;

    /**
     * A station is a grouping of multiple stops and entrances.
     */
    class Station {
        std::vector<std::reference_wrapper<const Stop>> stops = {};
        std::vector<StationEntrance> entrances = {};
        std::string gtfs_id;
        std::string name;

    public:
        Station(std::string name, std::string gtfs_id,
                std::vector<std::reference_wrapper<const Stop>>&& stops, std::vector<StationEntrance>&& entrances) :
            stops(std::move(stops)), entrances(std::move(entrances)),
            gtfs_id(std::move(gtfs_id)), name(std::move(name)) {
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::string& get_name() const {
            return name;
        }
    };
}

#endif //PT_ROUTING_STOP_H