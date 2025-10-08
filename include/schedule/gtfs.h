#ifndef GTFS_H
#define GTFS_H

#include <unordered_map>

#include <just_gtfs/just_gtfs.h>

#include "Schedule.h"

namespace raptor::gtfs {
    using stop_id = std::string;

    /**
     * Maps an entity's ID to a reference wrapper of the given type.
     */
    template <typename K, typename V>
    using reference_index = std::unordered_map<K, std::reference_wrapper<V>>;

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
    */
    StopManager from_gtfs(::gtfs::Stops&& gtfs_stops);

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
