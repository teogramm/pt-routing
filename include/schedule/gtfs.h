#ifndef GTFS_H
#define GTFS_H

#include <deque>
#include <unordered_map>

#include <just_gtfs/just_gtfs.h>

#include "Schedule.h"

namespace raptor::gtfs {
    /**
     * Maps an entity's ID to a reference wrapper of the given type.
     */
    template <typename K, typename V>
    using reference_index = std::unordered_map<K, std::reference_wrapper<V>>;

    /**
     * Converts just_gtfs stops into raptor Stops.
     * @param gtfs_stops Stops collection provided by the just_gtfs library
     * @return Vector of raptor Stop objects.
     */
    std::deque<Stop> from_gtfs(const ::gtfs::Stops& gtfs_stops);

    std::deque<Agency> from_gtfs(const ::gtfs::Agencies& gtfs_agencies);

    /**
     * Creates Service objects from GTFS calendar and calendar_dates.
     * @return Map of the GTFS service_id to the corresponding Service object. A map is returned for faster searching.
     */
    std::unordered_map<std::string, Service> from_gtfs(const ::gtfs::Calendar& calendars,
                                                       const ::gtfs::CalendarDates& calendar_dates,
                                                       const std::optional<std::chrono::year_month_day>& from_date =
                                                               std::nullopt,
                                                       const std::optional<std::chrono::year_month_day>& to_date =
                                                               std::nullopt);

    /**
     * Creates an instantiation of a stop time, from a generic GTFS stop_time.
     *
     * A GTFS StopTime contains generic time and might be referred to by many trips on different days.
     * This function creates a ``raptor::StopTime`` for a specific instance of a trip.
     * @param stop_time The ``gtfs::StopTime`` object, containing departure and arrival times, without any specified
     * date.
     * @param service_day Day for the created stop time object. GTFS service days are slightly different from normal
     * days so the resulting times might be on a different day.
     * @param time_zone Time zone of the values in the ``stop_time`` objects.
     * @param stop A reference to the stop in the given ``stop_time`` object.
     * @return A ``raptor::StopTime`` object, with a times corresponding to a specific date.
     */
    StopTime from_gtfs(const ::gtfs::StopTime& stop_time, const std::chrono::year_month_day& service_day,
                       const std::chrono::time_zone* time_zone,
                       const Stop& stop);

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
                   const reference_index<std::string, const Stop>& stop_index);

    /**
     * Converts the given GTFS trip objects to raptor Trips.
     * @param gtfs_trips
     * @param services Map of GTFS service ids to the corresponding Service objects.
     * @param gtfs_stop_times The GTFS stop times that will be assigned to the trips.
     * @param time_zone Time zone for all the stop times.
     * @param stop_index Map from a stop's GTFS ID to the stop object.
     * @return
     */
    std::pair<std::vector<Trip>, std::unordered_map<std::string, std::string>>
    from_gtfs(const ::gtfs::Trips& gtfs_trips,
              const std::unordered_map<std::string, Service>& services,
              const ::gtfs::StopTimes& gtfs_stop_times,
              const std::chrono::time_zone* time_zone,
              const reference_index<std::string, const Stop>& stop_index
            );

    /**
     * Creates Route objects using existing Trip objects and the GTFS route information.
     * @param trips Vector of Trip objects that will be assigned to the routes.
     * @param trip_id_to_route_id Map matching each trip's GTFS ID to the corresponding route's GTFS ID.
     * @param gtfs_routes GTFS Route objects, used for getting additional information about the routes.
     * @return
     */
    std::vector<Route> from_gtfs(std::vector<Trip>&& trips,
                                 const std::unordered_map<std::string, std::string>& trip_id_to_route_id,
                                 const std::deque<Agency>& agencies, const ::gtfs::Routes& gtfs_routes);

    /**
     * Construct a Schedule from a GTFS feed. The schedule instantiates and creates specific trips for all the dates
     * in the given date range.
     *
     * @param feed GTFS feed. The read_feed method must have been already called.
     * @param from_date Trips occurring on and after this date will be instantiated.
     * @param to_date Trips occurring on and before this date will be instantiated.
     * @return
     */
    Schedule from_gtfs(const ::gtfs::Feed& feed,
                       const std::optional<std::chrono::year_month_day>& from_date = std::nullopt,
                       const std::optional<std::chrono::year_month_day>& to_date = std::nullopt);

}

#endif //GTFS_H
