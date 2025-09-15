#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <algorithm>
#include <chrono>
#include <deque>
#include <utility>
#include <string>


namespace raptor {
    using Time = std::chrono::zoned_seconds;

    class Stop {
    public:
        Stop(std::string name, std::string gtfs_id, const double latitude, const double longitude,
             std::string parent_stop_id, std::string platform_code) :
            name(std::move(name)),
            gtfs_id(std::move(gtfs_id)),
            parent_stop_id(std::move(parent_stop_id)),
            platform_code(std::move(platform_code)),
            coordinates({latitude, longitude}) {
        }

        [[nodiscard]] std::string_view get_name() const {
            return name;
        }

        [[nodiscard]] std::string_view get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] std::pair<double, double> get_coordinates() const {
            return std::make_pair(coordinates.latitude, coordinates.longitude);
        }

        [[nodiscard]] std::string_view get_parent_stop_id() const {
            return parent_stop_id;
        }

        [[nodiscard]] std::string_view get_platform_code() const {
            return platform_code;
        }

        friend bool operator==(const Stop& lhs, const Stop& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Stop& lhs, const Stop& rhs) { return !(lhs == rhs); }

    private:
        std::string name;
        std::string gtfs_id;
        std::string parent_stop_id;
        std::string platform_code;

        struct {
            double latitude;
            double longitude;
        } coordinates;
    };

    /**
     * An instance of a specific vehicle arriving and departing from a stop.
     */
    class StopTime {
    public:
        StopTime(const Time& arrival_time, const Time& departure_time, const Stop& stop) :
            arrival_time(arrival_time),
            departure_time(departure_time),
            stop(stop) {
        }

        [[nodiscard]] Time get_arrival_time() const {
            return arrival_time;
        }

        [[nodiscard]] Time get_departure_time() const {
            return departure_time;
        }

        [[nodiscard]] const Stop& get_stop() const {
            return stop;
        }

    private:
        Time arrival_time;
        Time departure_time;
        const Stop& stop;
    };

    /**
     * A service corresponds to certain days for which a trip is active.
     * It can be defined either as a combination of a period and weekdays or by manually adding days to it.
     */
    class Service {
    public:
        Service(std::string gtfs_id, std::vector<std::chrono::year_month_day>&& active_days) :
            gtfs_id(std::move(gtfs_id)),
            active_days(std::move(active_days)) {
        }

        [[nodiscard]] std::string_view get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::vector<std::chrono::year_month_day>& get_active_days() const {
            return active_days;
        }

        friend bool operator==(const Service& lhs, const Service& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Service& lhs, const Service& rhs) { return !(lhs == rhs); }

    private:
        std::string gtfs_id;
        std::vector<std::chrono::year_month_day> active_days;
    };

    /**
     * A journey made by a specific vehicle at a specific date.
     */
    class Trip {
    public:
        Trip(std::vector<StopTime>&& stop_times, std::string trip_gtfs_id,
             std::string shape_gtfs_id) :
            stop_times(std::move(stop_times)),
            trip_gtfs_id(std::move(trip_gtfs_id)),
            shape_gtfs_id(std::move(shape_gtfs_id)) {
        }

        [[nodiscard]] const std::vector<StopTime>& get_stop_times() const {
            return stop_times;
        }

        [[nodiscard]] std::string_view get_trip_gtfs_id() const {
            return trip_gtfs_id;
        }

        [[nodiscard]] std::string_view get_shape_gtfs_id() const {
            return shape_gtfs_id;
        }

        [[nodiscard]] const StopTime& get_stop_time(const size_t index) const {
            auto& stop_time = stop_times.at(index);
            return stop_time;
        }

        // TODO: Check this, since we instantiate GTFS trips, comparing the GTFS ID is not enough, we also need
        // to care about he service day.
        friend bool operator==(const Trip& lhs, const Trip& rhs) { return lhs.trip_gtfs_id == rhs.trip_gtfs_id; }
        friend bool operator!=(const Trip& lhs, const Trip& rhs) { return !(lhs == rhs); }

        /**
         * @return Departure time for the specific instantiation of this trip.
         */
        [[nodiscard]] Time departure_time() const {
            return stop_times.front().get_departure_time();
        };

    private:
        std::vector<StopTime> stop_times; /**< Stop times are completely owned by the trip */
        std::string trip_gtfs_id;
        std::string shape_gtfs_id;
    };

    class Agency {
    public:
        Agency(std::string gtfs_id, std::string name,
               std::string url, const std::chrono::time_zone* time_zone) :
            gtfs_id(std::move(gtfs_id)), name(std::move(name)), url(std::move(url)), time_zone(time_zone) {
        }

        [[nodiscard]] const std::chrono::time_zone* get_time_zone() const {
            return time_zone;
        }

        [[nodiscard]] std::string_view get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] std::string_view get_name() const {
            return name;
        }

        [[nodiscard]] std::string_view get_url() const {
            return url;
        }

        friend bool operator==(const Agency& lhs, const Agency& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Agency& lhs, const Agency& rhs) { return !(lhs == rhs); }

    private:
        std::string gtfs_id;
        std::string name;
        std::string url;
        const std::chrono::time_zone* time_zone;
    };

    /**
     * A route is a collection of trips, which stop at exactly the same stops, in the same order, and have the same
     * GTFS route ID.
     */
    class Route {
    public:
        Route(std::vector<Trip>&& trips, std::string short_name,
              std::string long_name, std::string gtfs_id, const Agency& agency) :
            trips(std::move(trips)),
            short_name(std::move(short_name)),
            long_name(std::move(long_name)),
            gtfs_id(std::move(gtfs_id)),
            agency(std::cref(agency)) {
        }

        [[nodiscard]] const std::vector<Trip>& get_trips() const {
            return trips;
        }

        [[nodiscard]] std::string_view get_short_name() const {
            return short_name;
        }

        [[nodiscard]] std::string_view get_long_name() const {
            return long_name;
        }

        [[nodiscard]] std::string_view get_gtfs_id() const {
            return gtfs_id;
        }

        friend bool operator==(const Route& lhs, const Route& rhs) {
            return lhs.trips == rhs.trips && lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const Route& lhs, const Route& rhs) {
            return !(lhs == rhs);
        }

        /**
         * Return an ordered vector of the stops this route passes through.
         * All trips in a route have the same stop sequence.
         */
        [[nodiscard]] std::vector<std::reference_wrapper<const Stop>> stop_sequence() const;

        [[nodiscard]] size_t hash() const;

        static size_t hash(const std::vector<std::reference_wrapper<const Stop>>& stops,
                           const std::string& gtfs_route_id);

    private:
        std::vector<Trip> trips;
        std::string short_name;
        std::string long_name;
        std::string gtfs_id;
        std::reference_wrapper<const Agency> agency;
    };


    class Schedule {
    public:
        Schedule() = delete;

        Schedule(std::deque<Agency>&& agencies, std::deque<Stop>&& stops, std::vector<Route>&& routes) :
            stops(std::move(stops)), agencies(std::move(agencies)), routes(std::move(routes)) {
        }

        [[nodiscard]] const std::vector<Route>& get_routes() const {
            return routes;
        }

        [[nodiscard]] const std::deque<Stop>& get_stops() const {
            return stops;
        }

    private:
        // Use a deque to ensure references stored in StopTimes are not invalidated
        // A vector could be used instead and since it's stored as const the references should not be invalidated.
        // In addition, the number of elements is known beforehand, so reserve calls can be made, ensuring that no
        // reallocation happens.
        // Nevertheless, using a deque is better, since it guarantees references will remain.
        const std::deque<Stop> stops;
        const std::deque<Agency> agencies;
        const std::vector<Route> routes;
    };

}

template <>
struct std::hash<raptor::Stop> {
    size_t operator()(const raptor::Stop& stop) const {
        return std::hash<std::string_view>{}(stop.get_gtfs_id());
    }
};

//TODO: Make this work for both const and non-const types
template <>
struct std::hash<std::reference_wrapper<const raptor::Stop>> {
    size_t operator()(const std::reference_wrapper<const raptor::Stop>& stop) const noexcept {
        return std::hash<raptor::Stop>{}(stop);
    }
};

template <>
struct std::hash<raptor::Route> {
    size_t operator()(const raptor::Route& route) const {
        return std::hash<std::string_view>{}(route.get_gtfs_id());
    }
};

template <>
struct std::hash<std::reference_wrapper<const raptor::Route>> {
    size_t operator()(const raptor::Route& route) const {
        return std::hash<raptor::Route>{}(route);
    }
};

// template <typename T> requires std::is_convertible_v<T, const raptor::Stop>
// struct std::hash<std::reference_wrapper<T>> {
//     size_t operator()(const std::reference_wrapper<T>& stop) const {
//         return std::hash<raptor::Stop>{}(stop);
//     }
// };

template <>
struct std::hash<std::vector<std::reference_wrapper<const raptor::Stop>>> {
    size_t operator()(const std::vector<std::reference_wrapper<const raptor::Stop>>& stop) const;
};

#endif //SCHEDULE_H
