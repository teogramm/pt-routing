#ifndef SCHEDULE_H
#define SCHEDULE_H
#include <chrono>
#include <deque>
#include <iosfwd>
#include <set>
#include <unordered_map>
#include <utility>
#include <string>


namespace raptor {


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
              std::string long_name, std::string gtfs_id) :
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
    public:
        Agency(std::string gtfs_id, std::string name,
               std::string url, const std::chrono::time_zone* time_zone):
            gtfs_id(std::move(gtfs_id)), name(std::move(name)), url(std::move(url)), time_zone(time_zone) {
        }

        [[nodiscard]] const std::chrono::time_zone* get_time_zone() const {
            return time_zone;
        }

    private:
        friend bool operator==(const Agency& lhs, const Agency& rhs) { return lhs.gtfs_id == rhs.gtfs_id; }
        friend bool operator!=(const Agency& lhs, const Agency& rhs) { return !(lhs == rhs); }

        const std::string gtfs_id;
        const std::string name;
        const std::string url;
        const std::chrono::time_zone* time_zone;
    };


    class Schedule {
    public:
        Schedule() = delete;

        Schedule(std::deque<Agency>&& agencies, std::deque<Stop>&& stops, std::vector<Route>&& routes):
            agencies(std::move(agencies)), stops(std::move(stops)), routes(std::move(routes)) {
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
        return std::hash<std::string>{}(stop.gtfs_id);
    }
};

//TODO: Make this work for both const and non-const types
template <>
struct std::hash<std::reference_wrapper<const raptor::Stop>> {
    size_t operator()(const std::reference_wrapper<const raptor::Stop>& stop) const {
        return std::hash<raptor::Stop>{}(stop);
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
