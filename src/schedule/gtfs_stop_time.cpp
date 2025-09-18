#include "schedule/gtfs.h"

namespace raptor::gtfs {
    /**
     * Converts the given gtfs::Time to a duration object, corresponding to the hours after 00:00.
     * Consider that GTFS times can be longer than 24 hours.
     */
    Time::duration gtfs_time_to_duration(const ::gtfs::Time& gtfs_time) {
        auto [hours_gtfs, minutes_gtfs, seconds_gtfs] = gtfs_time.get_hh_mm_ss();
        // Total duration is in seconds
        return Time::duration(60 * 60 * hours_gtfs + 60 * minutes_gtfs + seconds_gtfs);
    }


    /**
     * Combines a gtfs::Time with a date and time zone to create a zoned_time.
     */
    Time gtfs_time_to_local_time(const ::gtfs::Time& gtfs_time,
                                 const std::chrono::year_month_day& service_day,
                                 const std::chrono::time_zone* time_zone) {
        auto duration = gtfs_time_to_duration(gtfs_time);
        // TODO: Check if earliest is the correct option for resolution. Off the top of my head it should be since
        // the GTFS service day refers to the previous day, but must look into how earliest works.
        auto time = Time(time_zone, std::chrono::local_days(service_day) + duration,
                         std::chrono::choose::earliest);
        return time;
    }

    StopTime from_gtfs(const ::gtfs::StopTime& stop_time, const std::chrono::year_month_day& service_day,
                       const std::chrono::time_zone* time_zone, const Stop& stop) {
        auto time_equal = [](const ::gtfs::Time& time_a, const ::gtfs::Time& time_b) {
            auto [a_hours, a_minutes, a_seconds] = time_a.get_hh_mm_ss();
            auto [b_hours, b_minutes, b_seconds] = time_b.get_hh_mm_ss();
            return (a_hours == b_hours) && (a_minutes == b_minutes) && (a_seconds == b_seconds);
        };

        // Often the departure time is the same as the arrival time. In this case we can skip the creation of an
        // extra object and create just a copy instead.
        // This is probably feed dependent, but if this happens, this optimization increases performance.
        auto departure_time = gtfs_time_to_local_time(stop_time.departure_time, service_day, time_zone);
        if (time_equal(stop_time.departure_time, stop_time.arrival_time)) {
            auto arrival_time = departure_time;
            return {arrival_time, departure_time, std::cref(stop)};
        }
        auto arrival_time = gtfs_time_to_local_time(stop_time.arrival_time, service_day, time_zone);
        return {arrival_time, departure_time, std::cref(stop)};

    }
}
