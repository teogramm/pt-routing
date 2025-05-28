#include "raptor/Schedule.h"

#include <ranges>
#include <boost/container_hash/hash.hpp>

namespace raptor {

    /**
     * Creates an index mapping each item with a GTFS id to a const reference to it.
     * Since the result stores references, ownership of the items is the owner's.
     * @tparam T GTFS Entity type
     * @param items Vector of items of the given type
     * @return Index matching each item's GTFS id to a const reference to it.
     */
    template <GTFS_Entity T>
    reference_index<const T> create_index(const std::vector<T>& items) {
        auto index = reference_index<const T>{};
        index.reserve(items.size());
        std::ranges::transform(items, std::inserter(index, index.begin()), [](const T& item) {
            std::string item_id = item.gtfs_id;
            return std::pair{item_id, std::cref(item)};
        });
        return index;
    }

    /**
     * Converts the given gtfs::Time to a duration object, corresponding on the hours after 00:00.
     * Consider that GTFS times can be larger than 24 hours.
     * @param gtfs_time
     * @return
     */
    std::chrono::seconds gtfs_time_to_duration(const gtfs::Time& gtfs_time) {
        auto [hours_gtfs, minutes_gtfs, seconds_gtfs] = gtfs_time.get_hh_mm_ss();
        auto hours = std::chrono::hours(hours_gtfs);
        auto minutes = std::chrono::minutes(minutes_gtfs);
        auto seconds = std::chrono::seconds(seconds_gtfs);
        auto total_duration = hours + minutes + seconds;
        // Total duration is in seconds
        return {total_duration};
    }

    /**
     * Combines a gtfs::Time with a date and time zone to create a zoned_time.
     * @param gtfs_time
     * @param service_day
     * @param time_zone
     * @return
     */
    std::chrono::zoned_time<std::chrono::seconds> gtfs_time_to_local_time(const gtfs::Time& gtfs_time,
                                                                          const std::chrono::year_month_day&
                                                                          service_day,
                                                                          const std::chrono::time_zone* time_zone) {
        auto duration = gtfs_time_to_duration(gtfs_time);
        auto time = std::chrono::zoned_time<std::chrono::seconds>(time_zone,
                                                                  std::chrono::local_days(service_day));
        time = duration + time.get_local_time();
        return time;
    }

    std::chrono::year_month_day gtfs_date_to_ymd(const gtfs::Date& gtfs_date) {
        auto [year, month, day] = gtfs_date.get_yyyy_mm_dd();
        return std::chrono::year_month_day{std::chrono::year{year},
                                           std::chrono::month{month},
                                           std::chrono::day{day}};
    }

    // /**
    //  * Stop time that contains relative time, not bound to any service day.
    //  */
    // class GenericStopTime {
    // public:
    //     std::chrono::seconds departure_time = {};
    //     std::chrono::seconds arrival_time = {};
    //     const gtfs::StopTime& stop_time = {};
    //
    //
    //     GenericStopTime() = delete;
    //
    //     GenericStopTime(const std::chrono::seconds& departure_time, const std::chrono::seconds& arrival_time,
    //                     const gtfs::StopTime& stop_time):
    //         departure_time(departure_time), arrival_time(arrival_time), stop_time(stop_time) {}
    //
    //     static GenericStopTime from_gtfs(const gtfs::StopTime& stop_time) {
    //         auto departure_time = gtfs_time_to_duration(stop_time.departure_time);
    //         auto arrival_time = gtfs_time_to_duration(stop_time.arrival_time);
    //         return {departure_time, arrival_time, stop_time};
    //     }
    // };

    /**
     * Gets all occurrences of the given weekday in the given period. Limits are inclusive.
     * @param start Start of the period. Can be included in the range.
     * @param end End of the period. Can be included in the range.
     * @param weekday
     * @return
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
     * Get the active weekdays for the given calendar item.
     * @return Vector of weekdays when the given calendar is active.
     */
    std::vector<std::chrono::weekday> calendar_active_weekdays(const gtfs::CalendarItem& calendar) {
        auto calendar_active = [](const gtfs::CalendarAvailability& availability) {
            return availability == gtfs::CalendarAvailability::Available;
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

    StopTime StopTime::from_gtfs(const gtfs::StopTime& stop_time,
                                 const std::chrono::year_month_day& service_day,
                                 const std::chrono::time_zone* time_zone,
                                 const reference_index<const Stop>& stop_index) {
        auto departure_time = gtfs_time_to_local_time(stop_time.departure_time, service_day, time_zone);
        auto arrival_time = gtfs_time_to_local_time(stop_time.arrival_time, service_day, time_zone);
        auto& stop = stop_index.at(stop_time.stop_id);
        return {arrival_time, departure_time, stop};
    }

    std::unordered_map<std::string, Service> Service::from_gtfs(const gtfs::Calendar& calendars,
                                                                const gtfs::CalendarDates& calendar_dates) {
        auto services = std::unordered_map<std::string, Service>{};
        services.reserve(calendars.size());
        // Parse regular calendars
        for (auto& calendar : calendars) {
            auto start_date = gtfs_date_to_ymd(calendar.start_date);
            auto end_date = gtfs_date_to_ymd(calendar.end_date);
            auto active_weekdays = calendar_active_weekdays(calendar);
            auto service_dates = std::vector<std::chrono::year_month_day>{};
            for (auto& weekday : active_weekdays) {
                auto new_dates = all_weekdays_in_period(start_date, end_date, weekday);
                // Avoid copying the dates
                service_dates.insert(service_dates.end(), std::make_move_iterator(new_dates.begin()),
                                     std::make_move_iterator(new_dates.end()));
            }
            // TODO: Can a service appear twice?
            if (services.contains(calendar.service_id)) {
                throw std::runtime_error("Duplicate service ID found in calendars.txt");
            }
            services[calendar.service_id] = Service{calendar.service_id, service_dates};
        }
        // Parse exceptions
        for (auto& calendar_date : calendar_dates) {
            auto exception_date = gtfs_date_to_ymd(calendar_date.date);
            if (!services.contains(calendar_date.service_id))
                throw std::runtime_error("Unknown service ID in calendar_dates");
            auto& service = services[calendar_date.service_id];
            if (calendar_date.exception_type == gtfs::CalendarDateException::Added) {
                service.active_days.emplace_back(exception_date);
            }
            else {
                // Find the given date and remove it
                auto date_pos = std::ranges::find(service.active_days, exception_date);
                if (date_pos != service.active_days.end()) {
                    service.active_days.erase(date_pos);
                }
                else {
                    throw std::runtime_error("Can't find specified date to remove from calendar_dates.txt");
                }
            }
        }
        return services;
    }

    /**
     * Instantiates a specific instance of a trip with the given stop times, at the given dates.
     * @param gtfs_trip
     * @param service_day
     * @param time_zone
     * @return
     */
    Trip Trip::from_gtfs(const gtfs::Trip& gtfs_trip,
                         const std::vector<std::reference_wrapper<const gtfs::StopTime>>& gtfs_stop_times,
                         const std::chrono::year_month_day& service_day,
                         const std::chrono::time_zone* time_zone,
                         const reference_index<const Stop>& stop_index) {
        auto stop_times = std::vector<StopTime>{};
        stop_times.reserve(gtfs_stop_times.size());
        // Convert all the gtfs stop times to raptor stop times
        std::ranges::transform(gtfs_stop_times, std::back_inserter(stop_times),
                               [service_day, time_zone, &stop_index](const gtfs::StopTime& stop_time) {
                                   return StopTime::from_gtfs(stop_time, service_day, time_zone, stop_index);
                               });
        return {stop_times, gtfs_trip.trip_id, gtfs_trip.shape_id, gtfs_trip.route_id};
    }

    std::vector<Trip> Trip::from_gtfs(const gtfs::Trips& gtfs_trips,
                                      const std::unordered_map<std::string, Service>& services,
                                      std::unordered_map<
                                          std::string, std::vector<std::reference_wrapper<const gtfs::StopTime>>>
                                      gtfs_stop_times,
                                      const std::chrono::time_zone* time_zone,
                                      const reference_index<const Stop>& stop_index) {
        // First sort each stop time by its sequence
        for (auto& st_list : gtfs_stop_times | std::views::values) {
            std::ranges::sort(st_list, std::ranges::less{}, &gtfs::StopTime::stop_sequence);
            StopTime::from_gtfs(st_list[0].get(), services.begin()->second.active_days[0], time_zone, stop_index);
        }
        // Create a corresponding trip for each day of the service
        auto trips = std::vector<Trip>{};
        trips.reserve(gtfs_trips.size());
        std::ranges::for_each(
                gtfs_trips, [&services, &trips, &time_zone, &gtfs_stop_times, &stop_index](const gtfs::Trip& trip) {
                    auto& service = services.at(trip.service_id);
                    auto& stop_times = gtfs_stop_times.at(trip.trip_id);
                    std::ranges::transform(service.active_days, std::back_inserter(trips),
                                           [&trip, &time_zone, &stop_times, &stop_index](
                                           const std::chrono::year_month_day& service_day) {
                                               return from_gtfs(trip, stop_times, service_day, time_zone, stop_index);
                                           });
                });
        return trips;
    }

    std::size_t hash_value(const std::reference_wrapper<const Stop>& stop) {
        boost::hash<std::string> hasher;
        return hasher(stop.get().gtfs_id);
    }

    size_t Route::hash(const std::vector<std::reference_wrapper<const Stop>>& stops, const std::string& gtfs_route_id) {
        auto stop_hash = boost::hash<std::vector<std::reference_wrapper<const Stop>>>{}(stops);
        auto route_hash = std::hash<std::string>{}(gtfs_route_id);
        std::size_t seed = 0;
        boost::hash_combine(seed, stop_hash);
        boost::hash_combine(seed, route_hash);
        return seed;
    }

    /**
     *
     * @param trips Vector with all the trips. The items are moved into the routes, so it is invalid afterwards.
     * @return Vector with all the routes, containing the given trips.
     */
    std::vector<Route> create_routes(std::vector<Trip>&& trips,
                                     const reference_index<const gtfs::Route>& gtfs_routes) {
        // Each raptor route is considered unique if it contains the same stops and corresponds to the same gtfs id
        auto route_map = std::unordered_map<size_t, std::vector<Trip>>{};
        for (auto& trip : trips) {
            // Calculate the hash of the trip
            // Create a vector of stops
            auto stops = std::vector<std::reference_wrapper<const Stop>>{};
            stops.reserve(trip.stop_times.size());
            std::ranges::transform(trip.stop_times, std::back_inserter(stops),
                                   [](const StopTime& st) {
                                       return std::cref(st.stop);
                                   });
            auto hash = Route::hash(stops, trip.route_gtfs_id);
            // TODO: Check std move performance difference
            route_map[hash].emplace_back(std::move(trip));
        }
        // Create the actual route objects
        auto routes = std::vector<Route>{};
        routes.reserve(route_map.size());
        for (auto& route_trips : route_map | std::views::values) {
            // TODO: All trips for the same route should have the same gtfs id. Hash collisions?
            auto& route_gtfs_id = route_trips[0].route_gtfs_id;
            auto& gtfs_route = gtfs_routes.at(route_gtfs_id);

            auto& short_name = gtfs_route.get().route_short_name;
            auto& long_name = gtfs_route.get().route_long_name;
            routes.emplace_back(std::move(route_trips), short_name, long_name, route_gtfs_id);
        }
        return routes;
    }

    Schedule Schedule::from_gtfs(gtfs::Feed& feed, std::optional<int> day_limit) {
        // TODO: Get the timezone from each agency
        auto timezone_gtfs = feed.get_agencies().front().agency_timezone;
        auto timezone = std::chrono::locate_zone(timezone_gtfs);

        auto stops = raptor::from_gtfs(feed.get_stops());
        auto stop_index = create_index(stops);

        auto services = Service::from_gtfs(feed.get_calendar(), feed.get_calendar_dates());
        // Group stop times by trip
        auto& gtfs_times = feed.get_stop_times();
        auto stop_times = std::unordered_map<
            std::string, std::vector<std::reference_wrapper<const gtfs::StopTime>>>();
        stop_times.reserve(gtfs_times.size());
        for (auto& stop_time : gtfs_times) {
            auto& map_entry = stop_times[stop_time.trip_id];
            map_entry.emplace_back(stop_time);
        }
        // Assemble trips from the services
        auto trips = Trip::from_gtfs(feed.get_trips(), services, stop_times, timezone, stop_index);
        auto gtfs_route_index = reference_index<const gtfs::Route>{};
        gtfs_route_index.reserve(feed.get_routes().size());
        std::ranges::transform(feed.get_routes(), std::inserter(gtfs_route_index, gtfs_route_index.begin()), [](const gtfs::Route& route) {
            return std::make_pair(route.route_id, std::cref(route));
        });
        auto routes = create_routes(std::move(trips), gtfs_route_index);
        return {std::move(stops), std::move(routes)};
    }

    std::vector<Stop> from_gtfs(const gtfs::Stops& gtfs_stops) {
        //TODO: Handle different location types
        std::vector<Stop> stops;

        auto is_platform = [](const gtfs::Stop& stop) {
            return stop.location_type == gtfs::StopLocationType::StopOrPlatform;
        };
        std::ranges::transform(gtfs_stops | std::views::filter(is_platform),
                               std::back_inserter(stops), [](const gtfs::Stop& gtfs_stop) {
                                   return Stop{gtfs_stop.stop_name, gtfs_stop.stop_id,
                                               {gtfs_stop.stop_lat,
                                                gtfs_stop.stop_lon}};
                               });
        return stops;
    }

}
