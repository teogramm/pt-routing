#include "schedule/gtfs.h"

namespace raptor::gtfs {

    using GtfsLocationTypeToStops = std::unordered_map<::gtfs::StopLocationType, std::vector<::gtfs::Stop>>;

    using stop_gtfs_id = std::string;
    using station_gtfs_id = std::string;

    /**
     * Groups the stops by their location type.
     * @param gtfs_stops All imported GTFS stop objects. Consider using std::move to avoid copying, if the given vector
     * is not needed afterwards.
     * @return Map containing the relevant gtfs::Stop objects for each location type.
     */
    GtfsLocationTypeToStops group_stops_by_location_type(::gtfs::Stops gtfs_stops) {
        // Instead of storing references to the GTFS stop objects, store copies to benefit from continuous storage
        // when processing them afterwards.
        auto groups = GtfsLocationTypeToStops();
        groups.reserve(5);
        for (auto& stop : gtfs_stops) {
            groups[stop.location_type].emplace_back(std::move(stop));
        }
        return groups;
    }

    /**
     * Creates platform boarding area objects.
     * @param gtfs_boarding_areas Vector of gtfs::Stop objects with location type of boarding area.
     * @return Map grouping boarding areas based on their parent stop GTFS ID.
     */
    std::unordered_map<stop_gtfs_id, std::vector<BoardingArea>> create_boarding_areas(
            std::vector<::gtfs::Stop>&& gtfs_boarding_areas) {
        auto boarding_area_idx = std::unordered_map<stop_gtfs_id, std::vector<BoardingArea>>();
        for (const auto& boarding_area : gtfs_boarding_areas) {
            auto new_area = BoardingArea(boarding_area.stop_name, boarding_area.stop_id,
                                         boarding_area.stop_lat, boarding_area.stop_lon);
            boarding_area_idx[boarding_area.stop_id].push_back(std::move(new_area));
        }
        return boarding_area_idx;
    }

    /**
     * Creates station entrance objects.
     * @param gtfs_entrances Vector of gtfs::Stop objects with location type of station entrance.
     * @return Map grouping boarding areas based on their parent station GTFS ID.
     */
    std::unordered_map<station_gtfs_id, std::vector<StationEntrance>> create_entrances(
            std::vector<::gtfs::Stop>&& gtfs_entrances) {
        auto entrances_idx = std::unordered_map<station_gtfs_id, std::vector<StationEntrance>>();
        for (const auto& gtfs_entrance : gtfs_entrances) {
            auto new_entrance = StationEntrance(gtfs_entrance.stop_name, gtfs_entrance.stop_id,
                                                gtfs_entrance.stop_lat, gtfs_entrance.stop_lon);
            entrances_idx[gtfs_entrance.stop_id].push_back(std::move(new_entrance));
        }
        return entrances_idx;
    }

    /**
     * Create station objects with the given entrances.
     * @param gtfs_stations Vector of gtfs::Stop objects with location type of station.
     * @param gtfs_entrances Vector of gtfs::Stop objects with location type of station entrance.
     * @return Vector of Station objects.
     */
    std::vector<Station>
    assemble_stations(std::vector<::gtfs::Stop>&& gtfs_stations, std::vector<::gtfs::Stop>&& gtfs_entrances) {
        auto entrance_idx = create_entrances(std::move(gtfs_entrances));
        auto stations = std::vector<Station>();
        stations.reserve(gtfs_stations.size());
        for (const auto& gtfs_station : gtfs_stations) {
            auto& station_entrances = entrance_idx[gtfs_station.stop_id];
            stations.emplace_back(gtfs_station.stop_name, gtfs_station.stop_id,
                                  std::move(station_entrances));
        }
        return stations;
    }


    /**
     * Creates Stop objects with the given boarding areas.
     * @param gtfs_stops Vector of gtfs::Stop objects with location type of stop (also referred as platform).
     * @param gtfs_boarding_areas Vector of gtfs::Stop objects with location type of boarding area.
     * @param n_stations Number of station objects, used to allocate memory more efficiently.
     * @return Deque with Stop objects, and a mapping of parent station GTFS ID to a vector of the IDs of its
     * child stops.
     */
    std::pair<std::deque<Stop>,
              StopManager::StationToChildStopsMap>
    assemble_stops(std::vector<::gtfs::Stop>&& gtfs_stops,
                   std::vector<::gtfs::Stop>&& gtfs_boarding_areas,
                   const size_t n_stations) {
        auto boarding_area_idx =
                create_boarding_areas(std::move(gtfs_boarding_areas));

        auto station_to_child_stops = StopManager::StationToChildStopsMap{};
        station_to_child_stops.reserve(n_stations);

        auto stops = std::deque<Stop>();
        for (const auto& gtfs_stop : gtfs_stops) {
            auto& stop_boarding_areas = boarding_area_idx[gtfs_stop.stop_id];
            auto stop = Stop(gtfs_stop.stop_name, gtfs_stop.stop_id,
                             gtfs_stop.stop_lat, gtfs_stop.stop_lon,
                             gtfs_stop.platform_code, std::move(stop_boarding_areas));
            auto& inserted_stop = stops.emplace_back(std::move(stop));
            auto& parent_station_id = gtfs_stop.parent_station;
            station_to_child_stops[parent_station_id].emplace_back(inserted_stop.get_gtfs_id());
        }
        return {std::move(stops), std::move(station_to_child_stops)};
    }

    StopManager from_gtfs(::gtfs::Stops&& gtfs_stops) {

        auto stops_by_location_type = group_stops_by_location_type(gtfs_stops);

        auto [stops, station_to_stop_ids] =
                assemble_stops(std::move(stops_by_location_type[::gtfs::StopLocationType::StopOrPlatform]),
                               std::move(stops_by_location_type[::gtfs::StopLocationType::BoardingArea]),
                               std::size(stops_by_location_type[::gtfs::StopLocationType::Station]));

        auto stations = assemble_stations(std::move(stops_by_location_type[::gtfs::StopLocationType::Station]),
                                          std::move(stops_by_location_type[::gtfs::StopLocationType::EntranceExit]));

        return {std::move(stops), std::move(stations), station_to_stop_ids};
    }
}
