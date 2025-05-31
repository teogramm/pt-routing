#include "raptor/schedule/gtfs.h"

#include <list>
#include <ranges>


// TODO: Change references to shared ptr
namespace raptor::gtfs {

    using route_id = std::string;
    using calendar_id = std::string;
    using trip_id = std::string;
    using stop_id = std::string;

    /**
     * Create a map with values being const references.
     * Keys are selected using the given selector functions.
     * @param selector Function which converts an object to the value used as a key in the resulting map
     */
    template <std::totally_ordered Key, typename Value, typename Func>
    reference_index<Key, const Value> create_index(const std::vector<Value>& items, Func selector) {
        auto index = reference_index<Key, const Value>{};
        index.reserve(items.size());
        std::ranges::transform(items, std::inserter(index, index.begin()), [selector](const Value& item) {
            auto item_id = selector(item);
            return std::pair{item_id, std::cref(item)};
        });
        return index;
    }

    /**
     * Converts the given gtfs::Time to a duration object, corresponding to the hours after 00:00.
     * Consider that GTFS times can be longer than 24 hours.
     */
    std::chrono::seconds gtfs_time_to_duration(const ::gtfs::Time& gtfs_time) {
        auto [hours_gtfs, minutes_gtfs, seconds_gtfs] = gtfs_time.get_hh_mm_ss();
        auto hours = std::chrono::hours(hours_gtfs);
        auto minutes = std::chrono::minutes(minutes_gtfs);
        auto seconds = std::chrono::seconds(seconds_gtfs);
        auto total_duration = hours + minutes + seconds;
        // Total duration is in seconds
        return {total_duration};
    }

    std::chrono::year_month_day gtfs_date_to_ymd(const ::gtfs::Date& gtfs_date) {
        auto [year, month, day] = gtfs_date.get_yyyy_mm_dd();
        return std::chrono::year_month_day{std::chrono::year{year},
                                           std::chrono::month{month},
                                           std::chrono::day{day}};
    }


    /**
     * Combines a gtfs::Time with a date and time zone to create a zoned_time.
     */
    std::chrono::zoned_time<std::chrono::seconds> gtfs_time_to_local_time(const ::gtfs::Time& gtfs_time,
                                                                          const std::chrono::year_month_day&
                                                                          service_day,
                                                                          const std::chrono::time_zone* time_zone) {
        auto duration = gtfs_time_to_duration(gtfs_time);
        auto time = std::chrono::zoned_time<std::chrono::seconds>(time_zone,
                                                                  std::chrono::local_days(service_day));
        time = duration + time.get_local_time();
        return time;
    }

    std::deque<Stop> from_gtfs(const ::gtfs::Stops& gtfs_stops) {
        //TODO: Handle different location types
        std::deque<Stop> stops;

        auto is_platform = [](const ::gtfs::Stop& stop) {
            return stop.location_type == ::gtfs::StopLocationType::StopOrPlatform;
        };
        std::ranges::transform(gtfs_stops | std::views::filter(is_platform),
                               std::back_inserter(stops), [](const ::gtfs::Stop& gtfs_stop) {
                                   return Stop{gtfs_stop.stop_name, gtfs_stop.stop_id,
                                               gtfs_stop.stop_lat,
                                               gtfs_stop.stop_lon};
                               });
        return stops;
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

    StopTime from_gtfs(const ::gtfs::StopTime& stop_time, const std::chrono::year_month_day& service_day,
                       const std::chrono::time_zone* time_zone,
                       const Stop& stop) {
        auto departure_time = gtfs_time_to_local_time(stop_time.departure_time, service_day, time_zone);
        auto arrival_time = gtfs_time_to_local_time(stop_time.arrival_time, service_day, time_zone);;
        return {arrival_time, departure_time, std::cref(stop)};
    }

    /**
     * Gets all occurrences of the given weekday in the given period. Limits are inclusive.
     * @param start Start of the period. Can be included in the range.
     * @param end End of the period. Can be included in the range.
     */
    std::vector<std::chrono::year_month_day> all_weekdays_in_period(const std::chrono::year_month_day& start,
                                                                    const std::chrono::year_month_day& end,
                                                                    const std::chrono::weekday& weekday) {
        auto dates = std::vector<std::chrono::year_month_day>{};
        // Find how many days until occurrence of the next weekday
        auto days_until_next_weekday = std::chrono::weekday(std::chrono::sys_days(start)) - weekday;
        // Go to that date
        auto current_date = std::chrono::sys_days{start} + days_until_next_weekday;
        while (current_date <= end) {
            dates.emplace_back(current_date);
            current_date += std::chrono::days{7};
        }
        return dates;
    }

    /**
     * Get the active weekdays for the given GTFS calendar.
     * @return Vector of weekdays when the given calendar is active.
     */
    std::vector<std::chrono::weekday> calendar_active_weekdays(const ::gtfs::CalendarItem& calendar) {
        auto calendar_active = [](const ::gtfs::CalendarAvailability& availability) {
            return availability == ::gtfs::CalendarAvailability::Available;
        };
        auto weekdays = std::vector<std::chrono::weekday>{};
        if (calendar_active(calendar.monday)) {
            weekdays.emplace_back(std::chrono::Monday);
        }
        if (calendar_active(calendar.tuesday)) {
            weekdays.emplace_back(std::chrono::Tuesday);
        }
        if (calendar_active(calendar.wednesday)) {
            weekdays.emplace_back(std::chrono::Wednesday);
        }
        if (calendar_active(calendar.thursday)) {
            weekdays.emplace_back(std::chrono::Thursday);
        }
        if (calendar_active(calendar.friday)) {
            weekdays.emplace_back(std::chrono::Friday);
        }
        if (calendar_active(calendar.saturday)) {
            weekdays.emplace_back(std::chrono::Saturday);
        }
        if (calendar_active(calendar.sunday)) {
            weekdays.emplace_back(std::chrono::Sunday);
        }
        return weekdays;
    }

    std::unordered_map<calendar_id, Service> from_gtfs(const ::gtfs::Calendar& calendars,
                                                       const ::gtfs::CalendarDates& calendar_dates) {
        using days_vector = std::vector<std::chrono::year_month_day>;

        auto service_dates_map = std::unordered_map<calendar_id, days_vector>{};
        service_dates_map.reserve(calendars.size());
        // Parse regular calendars
        for (auto& calendar : calendars) {
            auto start_date = gtfs_date_to_ymd(calendar.start_date);
            auto end_date = gtfs_date_to_ymd(calendar.end_date);
            auto active_weekdays = calendar_active_weekdays(calendar);

            auto this_service_dates = days_vector{};
            for (auto& weekday : active_weekdays) {
                auto new_dates = all_weekdays_in_period(start_date, end_date, weekday);
                // Avoid copying the dates
                this_service_dates.insert(this_service_dates.end(), std::make_move_iterator(new_dates.begin()),
                                          std::make_move_iterator(new_dates.end()));
            }
            // TODO: Can a service appear twice?
            if (service_dates_map.contains(calendar.service_id)) {
                throw std::runtime_error("Duplicate service ID found in calendars.txt");
            }
            service_dates_map[calendar.service_id] = this_service_dates;
        }
        // Parse exceptions
        for (auto& calendar_date : calendar_dates) {
            auto exception_date = gtfs_date_to_ymd(calendar_date.date);
            if (!service_dates_map.contains(calendar_date.service_id))
                throw std::runtime_error("Unknown service ID in calendar_dates");
            auto& active_days = service_dates_map.at(calendar_date.service_id);
            if (calendar_date.exception_type == ::gtfs::CalendarDateException::Added) {
                active_days.emplace_back(exception_date);
            }
            else {
                // Find the given date and remove it
                auto date_pos = std::ranges::find(active_days, exception_date);
                if (date_pos != active_days.end()) {
                    active_days.erase(date_pos);
                }
                else {
                    throw std::runtime_error("Can't find specified date to remove from calendar_dates.txt");
                }
            }
        }
        // Return a map for faster searches, since services are only used for building the schedule and are not
        // retained. It is also convenient since the map is useful when creating the service objects.
        auto services = std::unordered_map<std::string, Service>{};
        services.reserve(service_dates_map.size());
        std::ranges::transform(service_dates_map, std::inserter(services, services.begin()), [](
                               std::pair<std::string, days_vector> service_dates) {
                                   return std::make_pair(service_dates.first,
                                                         Service(service_dates.first,
                                                                 std::move(service_dates.second)));
                               });
        return services;
    }

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

    std::unordered_map<trip_id, std::vector<std::reference_wrapper<const ::gtfs::StopTime>>>
    group_stop_times_by_trip(const ::gtfs::StopTimes& gtfs_stop_times, const size_t n_trips) {
        auto stop_times_by_trip = std::unordered_map<
            trip_id, std::vector<std::reference_wrapper<const ::gtfs::StopTime>>>{};
        stop_times_by_trip.reserve(n_trips);
        for (const auto& stop_time : gtfs_stop_times) {
            auto& map_value = stop_times_by_trip[stop_time.trip_id];
            map_value.emplace_back(stop_time);
        }
        // First sort each stop time by its sequence
        for (auto& st_list : stop_times_by_trip | std::views::values) {
            std::ranges::sort(st_list, std::ranges::less{}, &::gtfs::StopTime::stop_sequence);
        }
        return stop_times_by_trip;
    }

    std::pair<std::vector<Trip>, std::unordered_map<trip_id, route_id>>
    from_gtfs(
            const ::gtfs::Trips& gtfs_trips,
            const std::unordered_map<std::string, Service>& services,
            const ::gtfs::StopTimes& gtfs_stop_times,
            const std::chrono::time_zone* time_zone,
            const std::deque<Stop>& stops) {
        // Group stop times by the corresponding trip id
        auto stop_times_by_trip = group_stop_times_by_trip(gtfs_stop_times, gtfs_trips.size());
        // Create index mapping stop gtfs ids to the stop object
        auto stop_index = reference_index<stop_id, const Stop>{};
        stop_index.reserve(stops.size());
        std::ranges::transform(stops, std::inserter(stop_index, stop_index.begin()), [](const Stop& stop) {
            auto gtfs_id = std::string(stop.get_gtfs_id());
            return std::make_pair(gtfs_id, std::cref(stop));
        });
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
            auto route_id = trip_id_to_route_id.at(std::string(trip.get_trip_gtfs_id()));
            auto hash = Route::hash(stops, route_id);
            route_map[hash].emplace_back(std::move(trip));
        }
        return route_map;
    }

    std::vector<Route> from_gtfs(std::vector<Trip>&& trips,
                                 const std::unordered_map<trip_id, route_id>& trip_id_to_route_id,
                                 const std::vector<::gtfs::Route>& gtfs_routes) {
        auto gtfs_route_index = std::unordered_map<std::string, std::reference_wrapper<const ::gtfs::Route>>{};
        gtfs_route_index.reserve(gtfs_routes.size());
        std::ranges::transform(gtfs_routes, std::inserter(gtfs_route_index, gtfs_route_index.begin()),
                               [](const ::gtfs::Route& route) {
                                   return std::make_pair(route.route_id, std::cref(route));
                               });
        auto route_map = group_trips_by_route(std::move(trips), trip_id_to_route_id);
        // Create the actual route objects
        auto routes = std::vector<Route>{};
        routes.reserve(route_map.size());
        for (auto& route_trips : route_map | std::views::values) {
            // TODO: All trips for the same route should have the same gtfs id. Hash collisions?
            auto route_gtfs_id = trip_id_to_route_id.at(std::string(route_trips[0].get_trip_gtfs_id()));
            auto& gtfs_route = gtfs_route_index.at(route_gtfs_id);

            auto& short_name = gtfs_route.get().route_short_name;
            auto& long_name = gtfs_route.get().route_long_name;
            routes.emplace_back(std::move(route_trips), short_name, long_name, route_gtfs_id);
        }
        return routes;
    }

    Schedule from_gtfs(const ::gtfs::Feed& feed, std::optional<int> day_limit) {
        // TODO: Add day limit
        // TODO: Get the timezone from each agency
        auto agencies = from_gtfs(feed.get_agencies());
        auto timezone = agencies.front().get_time_zone();

        auto stops = from_gtfs(feed.get_stops());

        auto services = from_gtfs(feed.get_calendar(), feed.get_calendar_dates());
        // Group stop times by trip
        auto& gtfs_times = feed.get_stop_times();
        // Assemble trips from the services
        auto [trips, trip_id_to_route_id] =
                from_gtfs(feed.get_trips(), services, gtfs_times, timezone, stops);
        auto routes = from_gtfs(std::move(trips), trip_id_to_route_id, feed.get_routes());
        return {std::move(agencies), std::move(stops), std::move(routes)};
    }
}
