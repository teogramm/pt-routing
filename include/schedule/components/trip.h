#ifndef PT_ROUTING_TRIP_H
#define PT_ROUTING_TRIP_H

#include <schedule/components/stop.h>

namespace raptor {
    using Time = std::chrono::zoned_seconds;
    /**
     * An instance of a specific vehicle arriving and departing from a stop.
     */
    class StopTime {
        Time arrival_time;
        Time departure_time;
        const Stop& stop;

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
    };

    /**
     * A service corresponds to certain days for which a trip is active.
     * It can be defined either as a combination of a period and weekdays or by manually adding days to it.
     */
    class Service {
        std::string gtfs_id;
        std::vector<std::chrono::year_month_day> active_days;

    public:
        Service(std::string gtfs_id, std::vector<std::chrono::year_month_day>&& active_days) :
            gtfs_id(std::move(gtfs_id)),
            active_days(std::move(active_days)) {
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::vector<std::chrono::year_month_day>& get_active_days() const {
            return active_days;
        }

        friend bool operator==(const Service& lhs, const Service& rhs) {
            return lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const Service& lhs, const Service& rhs) {
            return !(lhs == rhs);
        }
    };

    /**
     * A journey made by a specific vehicle at a specific date.
     */
    class Trip {
        std::vector<StopTime> stop_times; /**< Stop times are completely owned by the trip */
        std::string trip_gtfs_id;
        std::string shape_gtfs_id;

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

        [[nodiscard]] const std::string& get_trip_gtfs_id() const {
            return trip_gtfs_id;
        }

        [[nodiscard]] const std::string& get_shape_gtfs_id() const {
            return shape_gtfs_id;
        }

        [[nodiscard]] const StopTime& get_stop_time(const size_t index) const {
            auto& stop_time = stop_times.at(index);
            return stop_time;
        }

        // TODO: Check this, since we instantiate GTFS trips, comparing the GTFS ID is not enough, we also need
        // to care about 2he service day.
        friend bool operator==(const Trip& lhs, const Trip& rhs) {
            return lhs.trip_gtfs_id == rhs.trip_gtfs_id;
        }

        friend bool operator!=(const Trip& lhs, const Trip& rhs) {
            return !(lhs == rhs);
        }

        /**
         * @return Departure time for the specific instantiation of this trip.
         */
        [[nodiscard]] Time departure_time() const {
            return stop_times.front().get_departure_time();
        }
    };
}

#endif //PT_ROUTING_TRIP_H