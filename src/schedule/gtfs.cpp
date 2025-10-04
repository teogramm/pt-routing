#include <deque>
#include <list>
#include <ranges>

#include "schedule/gtfs.h"
#include "schedule/construction.h"

// TODO: Change references to shared ptr
namespace raptor::gtfs {

    using agency_id = std::string;
    using route_id = std::string;
    using calendar_id = std::string;
    using trip_id = std::string;
    using stop_id = std::string;

    /**
     * Create a map with values being const references.
     * Keys are selected using the given selector functions.
     * @param selector Function which converts an object to the value used as a key in the resulting map
     */
    template <
        std::ranges::input_range R,
        typename Value = std::ranges::range_value_t<R>,
        typename Selector,
        std::totally_ordered Key = std::invoke_result_t<Selector, Value>>
    reference_index<Key, const Value> create_index(R&& range, Selector selector) {
        auto index = reference_index<Key, const Value>{};
        index.reserve(std::ranges::size(range));
        std::ranges::transform(range, std::inserter(index, index.begin()), [selector](const Value& item) {
            auto item_id = selector(item);
            return std::pair{item_id, std::cref(item)};
        });
        return index;
    }

    /**
     * Converts just_gtfs stops into raptor Stops.
     * @param gtfs_stops Stops collection provided by the just_gtfs library
     * @return Vector of raptor Stop objects.
     */
    std::pair<std::deque<Stop>,std::vector<Station>> from_gtfs(const ::gtfs::Stops& gtfs_stops) {
        //TODO: Handle different location types
        auto stops = std::deque<Stop>{};
        auto station_id_to_builder = std::unordered_map<stop_id, StationBuilder>{};

        auto process_gtfs_stop = [&stops, &station_id_to_builder](const ::gtfs::Stop& gtfs_stop) {
            auto& station_id = gtfs_stop.location_type != ::gtfs::StopLocationType::Station
                    ? gtfs_stop.parent_station
                    : gtfs_stop.stop_id;
            auto station_builder = station_id_to_builder.find(station_id);

            // We haven't encountered this parent station before
            if (!station_id.empty() && station_builder == station_id_to_builder.end()) {
                station_builder = station_id_to_builder.emplace(station_id,
                                                                StationBuilder(station_id)).first;
            }

            if (gtfs_stop.location_type == ::gtfs::StopLocationType::StopOrPlatform) {
                auto stop = Stop{gtfs_stop.stop_name, gtfs_stop.stop_id,
                                 gtfs_stop.stop_lat, gtfs_stop.stop_lon,
                                 gtfs_stop.platform_code};
                auto& inserted_stop = stops.emplace_back(std::move(stop));
                if (station_builder != station_id_to_builder.end()) {
                    station_builder->second.add_stop(inserted_stop);
                }
            } else if (gtfs_stop.location_type == ::gtfs::StopLocationType::Station) {
                // We have encountered its definition so set its name
                station_builder->second.set_name(gtfs_stop.stop_name);
            } else if (gtfs_stop.location_type == ::gtfs::StopLocationType::EntranceExit) {
                auto entrance = Stop{gtfs_stop.stop_name, gtfs_stop.stop_id,
                                     gtfs_stop.stop_lat, gtfs_stop.stop_lon,
                                     gtfs_stop.platform_code};
                station_builder->second.add_entrance(std::move(entrance));
            }
        };

        std::ranges::for_each(gtfs_stops, process_gtfs_stop);

        auto stations = std::vector<Station>{};
        stations.reserve(station_id_to_builder.size());
        std::ranges::transform(station_id_to_builder | std::views::values, std::back_inserter(stations),
                               &StationBuilder::build);

        return {std::move(stops), std::move(stations)};
    }

    std::deque<Agency> from_gtfs(const ::gtfs::Agencies& gtfs_agencies) {
        std::deque<Agency> agencies;
        std::ranges::transform(gtfs_agencies, std::back_inserter(agencies), [](const ::gtfs::Agency& gtfs_agency) {
            // TODO: Handle wrong timezone. But is this likely to happen?
            auto time_zone = std::chrono::locate_zone(gtfs_agency.agency_timezone);
            return Agency(gtfs_agency.agency_id, gtfs_agency.agency_name, gtfs_agency.agency_url, time_zone);
        });
        return agencies;
    }

    /**
     * Creates a specific instantiation of a GTFS trip.
     * While GTFS objects are
     * @param gtfs_trip GTFS trip.
     * @param gtfs_stop_times GTFS stop times for the given trip.
     * @param service_day Day for the created stop time object. GTFS service days are slightly different from normal
     * days so the resulting stop times might be on a different day.
     * @param time_zone Time zone of the values in the stop_times.
     * @param stop_index Map of GTFS stop IDs to the corresponding Stop objects.
     * @return The resulting stop times in the trips contain references to the stops in the index. Ensure proper
     * ownership.
     */
    Trip from_gtfs(const ::gtfs::Trip& gtfs_trip,
                   const std::vector<std::reference_wrapper<const ::gtfs::StopTime>>& gtfs_stop_times,
                   const std::chrono::year_month_day& service_day, const std::chrono::time_zone* time_zone,
                   const reference_index<stop_id, const Stop>& stop_index) {
        auto stop_times = std::vector<StopTime>{};
        stop_times.reserve(gtfs_stop_times.size());
        // Convert all the gtfs stop times to raptor stop times
        std::ranges::transform(gtfs_stop_times, std::back_inserter(stop_times),
                               [service_day, time_zone, &stop_index](const ::gtfs::StopTime& stop_time) {
                                   auto& stop = stop_index.at(stop_time.stop_id);
                                   return from_gtfs(stop_time, service_day, time_zone, stop);
                               });
        return {std::move(stop_times), gtfs_trip.trip_id, gtfs_trip.shape_id};
    }

    /**
     * Group the given StopTimes by trip and sort them by their sequence.
     */
    std::unordered_map<trip_id, std::vector<std::reference_wrapper<const ::gtfs::StopTime>>>
    group_stop_times_by_trip(const ::gtfs::StopTimes& gtfs_stop_times, const size_t n_trips) {
        auto stop_times_by_trip = std::unordered_map<
            trip_id, std::vector<std::reference_wrapper<const ::gtfs::StopTime>>>{};
        stop_times_by_trip.reserve(n_trips);
        for (const auto& stop_time : gtfs_stop_times) {
            auto& map_value = stop_times_by_trip[stop_time.trip_id];
            map_value.emplace_back(std::cref(stop_time));
        }
        // First sort each stop time by its sequence
        for (auto& st_vec : stop_times_by_trip | std::views::values) {
            std::ranges::sort(st_vec, std::ranges::less{}, &::gtfs::StopTime::stop_sequence);
        }
        return stop_times_by_trip;
    }

    /**
     * Converts the given GTFS trip objects to raptor Trips.
     * @param gtfs_trips
     * @param services Map of GTFS service ids to the corresponding Service objects.
     * @param gtfs_stop_times The GTFS stop times that will be assigned to the trips.
     * @param time_zone Time zone for all the stop times.
     * @param stop_index Map from a stop's GTFS ID to the stop object.
     * @return
     */
    std::pair<std::vector<Trip>, std::unordered_map<trip_id, route_id>>
    from_gtfs(
            const ::gtfs::Trips& gtfs_trips,
            const std::unordered_map<std::string, Service>& services,
            const ::gtfs::StopTimes& gtfs_stop_times,
            const std::chrono::time_zone* time_zone,
            const reference_index<std::string, const Stop>& stop_index) {
        // Group stop times by the corresponding trip id
        auto stop_times_by_trip = group_stop_times_by_trip(gtfs_stop_times, gtfs_trips.size());
        // Create a corresponding trip object for each day of the service
        // In addition, maintain a map for the route each trip belongs to
        auto trips = std::vector<Trip>{};
        auto trip_id_to_route_id = std::unordered_map<trip_id, route_id>{};
        trips.reserve(gtfs_trips.size());
        for (const auto& trip : gtfs_trips) {
            auto& service = services.at(trip.service_id);
            auto& stop_times = stop_times_by_trip.at(trip.trip_id);
            std::ranges::transform(service.get_active_days(), std::back_inserter(trips),
                                   [&trip, &time_zone, &stop_times, &stop_index, &trip_id_to_route_id](
                                   const std::chrono::year_month_day& service_day) {
                                       auto trip_id = trip.trip_id;
                                       auto route_id = trip.route_id;
                                       trip_id_to_route_id[trip_id] = route_id;
                                       return from_gtfs(trip, stop_times, service_day, time_zone, stop_index);
                                   });
        }
        // If move is not specified a copy happens here
        return {std::move(trips), std::move(trip_id_to_route_id)};
    }

    /**
     * Groups the given trips by route.
     * Two trips belong in the same route if they have the same stop order and the same route ID.
     * @param trips Collection of Raptor trip objects. Ownership of the trips in the vector is transferred
     * to the returned map.
     * @param trip_id_to_route_id Map matching the GTFS trip ID to the GTFS route ID for each route.
     * @return Map matching the Raptor route hash to a vector of trips belonging to it.
     */
    std::unordered_map<size_t, std::vector<Trip>>
    group_trips_by_route(std::vector<Trip>&& trips, const std::unordered_map<trip_id, route_id>& trip_id_to_route_id) {
        // Each raptor route is considered unique if it contains the same stops and corresponds to the same gtfs id
        auto route_map = std::unordered_map<size_t, std::vector<Trip>>{};
        for (auto& trip : trips) {
            // Calculate the hash of the trip
            // Create a vector of stops
            auto stops = std::vector<std::reference_wrapper<const Stop>>{};
            stops.reserve(trip.get_stop_times().size());
            std::ranges::transform(trip.get_stop_times(), std::back_inserter(stops),
                                   [](const StopTime& st) {
                                       return std::cref(st.get_stop());
                                   });
            auto& route_id = trip_id_to_route_id.at(trip.get_trip_gtfs_id());
            auto hash = Route::hash(stops, route_id);
            route_map[hash].emplace_back(std::move(trip));
        }
        return route_map;
    }

    /**
     * Creates Route objects using existing Trip objects and the GTFS route information.
     * @param trips Vector of Trip objects that will be assigned to the routes.
     * @param trip_id_to_route_id Map matching each trip's GTFS ID to the corresponding route's GTFS ID.
     * @param gtfs_routes GTFS Route objects, used for getting additional information about the routes.
     * @return
     */
    std::vector<Route> from_gtfs(std::vector<Trip>&& trips,
                                 const std::unordered_map<trip_id, route_id>& trip_id_to_route_id,
                                 const std::deque<Agency>& agencies,
                                 const ::gtfs::Routes& gtfs_routes) {
        auto gtfs_route_index = create_index(gtfs_routes, [](const ::gtfs::Route& route) {
            return route.route_id;
        });
        auto agencies_index = create_index(agencies, [](const Agency& agency) {
            return agency.get_gtfs_id();
        });
        auto route_map = group_trips_by_route(std::move(trips), trip_id_to_route_id);
        // Create the actual route objects
        auto routes = std::vector<Route>{};
        routes.reserve(route_map.size());
        for (auto& route_trips : route_map | std::views::values) {
            // TODO: All trips for the same route should have the same gtfs id. Hash collisions?
            auto& route_gtfs_id = trip_id_to_route_id.at(route_trips[0].get_trip_gtfs_id());
            auto& gtfs_route = gtfs_route_index.at(route_gtfs_id);
            auto& agency = agencies_index.at(gtfs_route.get().agency_id);

            // TODO: Check performance.
            std::ranges::sort(route_trips, std::less{}, [](const Trip& trip) {
                return trip.get_stop_times()[0].get_departure_time().get_sys_time();
            });
            route_trips.shrink_to_fit();

            auto& short_name = gtfs_route.get().route_short_name;
            auto& long_name = gtfs_route.get().route_long_name;
            routes.emplace_back(std::move(route_trips), short_name, long_name, route_gtfs_id, agency);
        }
        return routes;
    }

    Schedule from_gtfs(const ::gtfs::Feed& feed,
                       const std::optional<std::chrono::year_month_day>& from_date,
                       const std::optional<std::chrono::year_month_day>& to_date) {
        // TODO: Add day limit
        // TODO: Get the timezone from each agency
        auto agencies = from_gtfs(feed.get_agencies());
        auto timezone = agencies.front().get_time_zone();

        auto [stops, stations] = from_gtfs(feed.get_stops());

        auto services = from_gtfs(feed.get_calendar(), feed.get_calendar_dates(),
                                  from_date, to_date);
        // Group stop times by trip
        auto& gtfs_times = feed.get_stop_times();
        // Assemble trips from the services
        auto stop_index = create_index(stops, [](const Stop& stop) {
            return stop.get_gtfs_id();
        });
        auto [trips, trip_id_to_route_id] =
                from_gtfs(feed.get_trips(), services, gtfs_times, timezone, stop_index);
        auto routes = from_gtfs(std::move(trips), trip_id_to_route_id, agencies, feed.get_routes());
        return {std::move(agencies), std::move(stops), std::move(stations), std::move(routes)};
    }
}
