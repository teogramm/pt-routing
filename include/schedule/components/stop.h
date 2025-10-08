#ifndef PT_ROUTING_STOP_H
#define PT_ROUTING_STOP_H

#include <deque>
#include <unordered_map>

namespace raptor {

    class Station;
    class StopManager;

    /**
     * Class containing common attributes of stop-like objects.
     *
     * In the GTFS specification the stops.txt contains other types of points, such as nodes and entrances, which
     * have many fields in common with regular stops (also referred to as platforms).
     */
    class BaseStop {

        std::string name;
        std::string gtfs_id;

        struct {
            double latitude;
            double longitude;
        } coordinates;

    public:
        BaseStop(std::string name, std::string gtfs_id, const double latitude, const double longitude) :
            name(std::move(name)),
            gtfs_id(std::move(gtfs_id)),
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

        friend bool operator==(const BaseStop& lhs, const BaseStop& rhs) {
            return lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const BaseStop& lhs, const BaseStop& rhs) {
            return !(lhs == rhs);
        }
    };

    using StationEntrance = BaseStop;
    using BoardingArea = BaseStop;
    using Node = BaseStop;

    class Stop : public BaseStop {
        const Station* parent_station = nullptr;
        std::string platform_code;
        std::vector<BoardingArea> boarding_areas;

        void set_parent_station(const Station* station) {
            parent_station = station;
        }

        friend class StopManager;

    public:
        Stop(std::string name, std::string gtfs_id, const double latitude, const double longitude,
             std::string platform_code, std::vector<BoardingArea>&& boarding_areas) :
            BaseStop(std::move(name), std::move(gtfs_id), latitude, longitude),
            platform_code(std::move(platform_code)),
            boarding_areas(std::move(boarding_areas)) {
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
    };

    /**
     * A station is a grouping of multiple stops and entrances.
     */
    class Station {
        std::vector<std::reference_wrapper<const Stop>> stops;
        std::vector<StationEntrance> entrances = {};
        std::string gtfs_id;
        std::string name;

        void add_child_stop(const Stop& stop) {
            stops.emplace_back(std::cref(stop));
        }

        // Allow StopManager to modify the stops
        friend class StopManager;

    public:
        Station(std::string name, std::string gtfs_id,
                std::vector<StationEntrance>&& entrances,
                std::vector<std::reference_wrapper<const Stop>>&& stops = {}) :
            stops(std::move(stops)), entrances(std::move(entrances)),
            gtfs_id(std::move(gtfs_id)), name(std::move(name)) {
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::string& get_name() const {
            return name;
        }

        [[nodiscard]] const std::vector<std::reference_wrapper<const Stop>>& get_stops() const {
            return stops;
        }
    };

    /**
     * Responsible for managing stops and stations.
     */
    class StopManager {
    public:
        using StationToChildStopsMap = std::unordered_map<std::string, std::vector<std::string>>;

    private:
        // Use a deque to ensure references stored in StopTimes are not invalidated
        // A vector could be used instead and since it's stored as const the references should not be invalidated.
        // In addition, the number of elements is known beforehand, so reserve calls can be made, ensuring that no
        // reallocation happens.
        // Nevertheless, using a deque is better, since it guarantees references will remain.
        std::deque<Stop> stops;
        std::vector<Station> stations;

        /**
         * Sets the parent station pointer of the given stop and adds it to the station's children.
         *
         * The references to both the station and the stop are non-owing and thus their lifetime must be managed
         * by the caller.
         *
         * @param station Reference to the parent station
         * @param stop Reference to the child stop
         */
        static void set_parent_station(Station& station, Stop& stop) {
            station.add_child_stop(stop);
            stop.set_parent_station(&station);
        }

        void initialise_relationships(const StationToChildStopsMap& stops_per_station);

    public:
        /**
         * Initialise the stop manager with the given stops and stations. The manager takes ownership of the objects
         * and initialises the parent/child relationship according to the given map.
         * @param stops Stops to be used by the manager
         * @param stations
         * @param stops_per_station Map matching a parent station GTFS ID to multiple child station GTFS IDs
         * @throws std::out_of_range If the GTFS ID of a stop or station in stops_per_station does not correspond
         * to any stop or station.
         */
        StopManager(std::deque<Stop>&& stops, std::vector<Station>&& stations,
                    const StationToChildStopsMap& stops_per_station) :
            stops(std::move(stops)), stations(std::move(stations)) {
            initialise_relationships(stops_per_station);
        }

        // Copying the StopManager is a bit tricky, as the parent-child relationships need to be recreated.
        // For now, delete the functions and they can be created if they are necessary.
        StopManager(const StopManager& other) = delete;
        StopManager& operator=(const StopManager& other) = delete;

        StopManager(StopManager&& other) noexcept :
            stops(std::move(other.stops)), stations(std::move(other.stations)) {
        }

        StopManager& operator=(StopManager&& other) noexcept {
            stops = std::move(other.stops);
            stations = std::move(other.stations);
            return *this;
        }

        [[nodiscard]] const auto& get_stops() const noexcept {
            return stops;
        }

        [[nodiscard]] const auto& get_stations() const noexcept {
            return stations;
        }

    };
}

template <>
// requires std::is_convertible_v<T, const raptor::Station&>
struct std::hash<std::reference_wrapper<raptor::Station>> {
    size_t operator()(const raptor::Station& station) const noexcept {
        return std::hash<std::string>{}(station.get_gtfs_id());
    }
};

#endif //PT_ROUTING_STOP_H
