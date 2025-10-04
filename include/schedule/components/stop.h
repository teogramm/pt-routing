#ifndef PT_ROUTING_STOP_H
#define PT_ROUTING_STOP_H

namespace raptor {

    class Station;

    class Stop {
        std::string name;
        std::string gtfs_id;
        const Station* parent_station = nullptr;
        std::string platform_code;

        struct {
            double latitude;
            double longitude;
        } coordinates;

        void set_parent_station(const Station* station) {
            parent_station = station;
        }

        friend class StopManager;

    public:
        Stop(std::string name, std::string gtfs_id, const double latitude, const double longitude,
             std::string platform_code) :
            name(std::move(name)),
            gtfs_id(std::move(gtfs_id)),
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

        [[nodiscard]] std::optional<std::reference_wrapper<const Station>> get_parent_station() const {
            if (parent_station) {
                return std::make_optional(std::cref(*parent_station));
            }
            return std::nullopt;
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
        std::vector<std::reference_wrapper<Stop>> stops = {};
        std::vector<StationEntrance> entrances = {};
        std::string gtfs_id;
        std::string name;

        std::vector<std::reference_wrapper<Stop>> get_stops() {
            return stops;
        }

        // Allow StopManager to modify the stops
        friend class StopManager;

    public:
        Station(std::string name, std::string gtfs_id,
                std::vector<std::reference_wrapper<Stop>>&& stops, std::vector<StationEntrance>&& entrances) :
            stops(std::move(stops)), entrances(std::move(entrances)),
            gtfs_id(std::move(gtfs_id)), name(std::move(name)) {
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::string& get_name() const {
            return name;
        }

        [[nodiscard]] const std::vector<std::reference_wrapper<Stop>>& get_stops() const {
            return stops;
        }
    };


    /**
     * Class responsible for enforcing relationships between Stops and Stations.
     */
    class StopManager {
        std::deque<Stop> stops;
        std::vector<Station> stations;

        void set_parent_stations() {
            for (auto& station : stations) {
                for (auto& stop : station.get_stops()) {
                    stop.get().set_parent_station(&station);
                }
            }
        }

    public:
        StopManager(std::deque<Stop>&& stops, std::vector<Station>&& stations) :
            stops(std::move(stops)), stations(std::move(stations)) {
            set_parent_stations();
        }

        StopManager(const StopManager& other) :
            stops(other.stops), stations(other.stations) {
            set_parent_stations();
        }

        StopManager(StopManager&& other) noexcept :
            stops(std::move(other.stops)), stations(std::move(other.stations)) {
            set_parent_stations();
        }

        StopManager& operator=(const StopManager& other) {
            auto copy = StopManager(other);
            std::swap(stations, copy.stations);
            std::swap(stops, copy.stops);
            return *this;
        }

        StopManager& operator=(StopManager&& other) noexcept {
            // TODO: Implement this in a better way
            stops = std::move(other.stops);
            stations = std::move(other.stations);
            set_parent_stations();
            return *this;
        }

        // We don't need to do anything since both stops and stations are destroyed together.
        ~StopManager() = default;

        [[nodiscard]] const std::deque<Stop>& get_stops() const {
            return stops;
        }

        [[nodiscard]] const std::vector<Station>& get_stations() const {
            return stations;
        }
    };
}

#endif //PT_ROUTING_STOP_H
