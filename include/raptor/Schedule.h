#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <chrono>
#include <iosfwd>
#include <set>
#include <unordered_map>
#include <utility>
#include <string>

#include "just_gtfs/just_gtfs.h"

namespace raptor {

    /**
     * Type which contains a ``gtfs_id`` field of type string.
     */
    template <typename T>
    concept GTFS_Entity = requires(T v)
    {
        { v.gtfs_id } -> std::same_as<std::string&>;
    };

    /**
     * Maps an entity's ID to a reference wrapper of the given type.
     */
    template <typename T>
    using reference_index = std::unordered_map<std::string, std::reference_wrapper<T>>;

    class Stop {
        friend bool operator==(const Stop& lhs, const Stop& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Stop& lhs, const Stop& rhs) { return !(lhs == rhs); }

    public:
        std::string name;
        std::string gtfs_id;

        struct {
            double longitude;
            double latitude;
        } coordinates;
    };

    /**
     * An instance of a specific vehicle arriving and departing from a stop.
     */
    class StopTime {
    public:
        std::chrono::zoned_time<std::chrono::seconds> arrival_time;
        std::chrono::zoned_time<std::chrono::seconds> departure_time;
        const Stop& stop;

        static StopTime from_gtfs(const gtfs::StopTime& stop_time, const std::chrono::year_month_day& service_day,
                                  const std::chrono::time_zone* time_zone,
                                  const reference_index<const Stop>& stop_index);
    };

    /**
     * A service corresponds to certain days for which a trip is active.
     * It can be defined either as a combination of a period and weekdays or by manually adding days to it.
     */
    class Service {
        friend bool operator==(const Service& lhs, const Service& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Service& lhs, const Service& rhs) { return !(lhs == rhs); }

    public:
        std::string gtfs_id;
        std::vector<std::chrono::year_month_day> active_days;

        static std::unordered_map<std::string, Service> from_gtfs(const gtfs::Calendar& calendars,
                                                                  const gtfs::CalendarDates& calendar_dates);
    };

    /**
     * A journey made by a specific vehicle at a specific date.
     */
    class Trip {
        friend bool operator==(const Trip& lhs, const Trip& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Trip& lhs, const Trip& rhs) { return !(lhs == rhs); }

    public:
        std::vector<StopTime> stop_times; /**< Stop times are completely owned by the trip */
        std::string gtfs_id;
        std::string shape_gtfs_id;
        std::string route_gtfs_id;

        static Trip from_gtfs(const gtfs::Trip& gtfs_trip,
                              const std::vector<std::reference_wrapper<const gtfs::StopTime>>& gtfs_stop_times,
                              const std::chrono::year_month_day& service_day, const std::chrono::time_zone* time_zone,
                              const reference_index<const Stop>&
                              stop_index);
        static std::vector<Trip> from_gtfs(const gtfs::Trips& gtfs_trips,
                                           const std::unordered_map<std::string, Service>& services,
                                           std::unordered_map<
                                               std::string, std::vector<std::reference_wrapper<const gtfs::StopTime>>>
                                           gtfs_stop_times, const std::chrono::time_zone* time_zone,
                                           const reference_index<const Stop>& stop_index);

        /**
         * @return Departure time for the specific instantiation of this trip.
         */
        [[nodiscard]] std::chrono::zoned_time<std::chrono::seconds> departure_time() const {
            return stop_times.front().departure_time;
        };
    };

    /**
     * A route is a collection of trips, which stop at exactly the same stops, in the same order, and have the same
     * GTFS route ID.
     */
    class Route {
    public:
        Route(std::vector<Trip>&& trips, std::string short_name,
                std::string long_name, std::string  gtfs_id) :
            trips(std::move(trips)),
            short_name(std::move(short_name)),
            long_name(std::move(long_name)),
            gtfs_id(std::move(gtfs_id)) {
        }

    private:
        friend bool operator==(const Route& lhs, const Route& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Route& lhs, const Route& rhs) { return !(lhs == rhs); }

    public:
        std::vector<Trip> trips;
        std::string short_name;
        std::string long_name;
        std::string gtfs_id;

        [[nodiscard]] std::vector<std::reference_wrapper<const Stop>> stop_sequence() const {
            // All trips have the same stops, so take the stops from the first trip
            auto stops = std::vector<std::reference_wrapper<const Stop>>{};
            for (const auto& stop_time : trips.front().stop_times) {
                stops.emplace_back(stop_time.stop);
            }
            return stops;
        }

        static size_t hash(const std::vector<std::reference_wrapper<const Stop>>& stops,
                           const std::string& gtfs_route_id);
    };

    class Agency {
        friend bool operator==(const Agency& lhs, const Agency& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Agency& lhs, const Agency& rhs) { return !(lhs == rhs); }

        const std::string gtfs_id;
        const std::string name;
        const std::string url;
        const std::chrono::time_zone* time_zone;

    public:
        Agency() = delete;

        Agency(std::string gtfs_id, std::string name,
               std::string url, const std::chrono::time_zone* time_zone):
            gtfs_id(std::move(gtfs_id)), name(std::move(name)), url(std::move(url)), time_zone(time_zone) {
        }
    };


    class Schedule {
    public:
        Schedule() = delete;

        Schedule(std::vector<Stop>&& stops, std::vector<Route>&& routes):
            stops(std::move(stops)), routes(std::move(routes)) {
        }

        static Schedule from_gtfs(gtfs::Feed& feed, std::optional<int> day_limit = std::nullopt);

    private:
        std::vector<Stop> stops;
        std::vector<Agency> agencies;
        std::vector<Route> routes;
    };

    std::vector<Stop> from_gtfs(const gtfs::Stops& gtfs_stops);
    Route from_gtfs(const gtfs::Route& route, std::vector<Trip>& trips);
}

template <>
struct std::hash<raptor::Stop> {
    size_t operator()(const raptor::Stop& stop) const {
        return std::hash<std::string>{}(stop.gtfs_id);
    }
};

template <>
struct std::hash<std::reference_wrapper<gtfs::Stop>> {
    size_t operator()(const std::reference_wrapper<const raptor::Stop>& stop) const {
        return std::hash<raptor::Stop>{}(stop);
    }
};

#endif //SCHEDULE_H
