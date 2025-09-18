#include "schedule/gtfs.h"

namespace raptor::gtfs {
    using calendar_id = std::string;

    std::chrono::year_month_day gtfs_date_to_ymd(const ::gtfs::Date& gtfs_date) {
        auto [year, month, day] = gtfs_date.get_yyyy_mm_dd();
        return std::chrono::year_month_day{std::chrono::year{year},
                                           std::chrono::month{month},
                                           std::chrono::day{day}};
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
                                                       const ::gtfs::CalendarDates& calendar_dates,
                                                       const std::optional<std::chrono::year_month_day>& from_date,
                                                       const std::optional<std::chrono::year_month_day>& to_date) {
        using date_vector = std::vector<std::chrono::year_month_day>;

        auto service_dates_map = std::unordered_map<calendar_id, date_vector>{};
        service_dates_map.reserve(calendars.size());

        // For some reason using chrono::year::max gives a limit_end in 1969
        auto limit_start = std::chrono::year_month_day(std::chrono::year::min(), std::chrono::month{1},
                                                       std::chrono::day{1});
        if (from_date.has_value())
            limit_start = from_date.value();

        auto limit_end = std::chrono::year_month_day(std::chrono::year::max(), std::chrono::month{12},
                                                     std::chrono::day{31});
        if (to_date.has_value())
            limit_end = to_date.value();

        assert(limit_end >= limit_start);

        auto parse_calendar = [&limit_start, &limit_end, &service_dates_map](const ::gtfs::CalendarItem& calendar) {
            // Return the most limiting period as defined by the limit and the calendar dates
            auto start_date = std::max(gtfs_date_to_ymd(calendar.start_date), limit_start);
            auto end_date = std::min(gtfs_date_to_ymd(calendar.end_date), limit_end);
            auto active_weekdays = calendar_active_weekdays(calendar);

            auto this_service_dates = date_vector{};
            for (auto& weekday : active_weekdays) {
                auto new_dates = all_weekdays_in_period(start_date, end_date, weekday);
                // Avoid copying the dates
                this_service_dates.insert(this_service_dates.end(),
                                          std::make_move_iterator(new_dates.begin()),
                                          std::make_move_iterator(new_dates.end()));
            }
            // TODO: Can a service appear twice?
            if (service_dates_map.contains(calendar.service_id)) {
                throw std::runtime_error("Duplicate service ID found in calendars.txt");
            }
            service_dates_map.emplace(calendar.service_id, std::move(this_service_dates));
        };

        auto parse_exception = [&limit_start, &limit_end, &service_dates_map
                ](const ::gtfs::CalendarDate& calendar_date) {
            auto exception_date = gtfs_date_to_ymd(calendar_date.date);
            if (exception_date >= limit_start && exception_date <= limit_end) {
                if (!service_dates_map.contains(calendar_date.service_id))
                    throw std::runtime_error("Unknown service ID in calendar_dates");
                auto& active_days = service_dates_map.at(calendar_date.service_id);
                if (calendar_date.exception_type == ::gtfs::CalendarDateException::Added) {
                    active_days.emplace_back(exception_date);
                } else {
                    // Find the given date and remove it
                    auto date_pos =
                            std::ranges::find(active_days, exception_date);
                    if (date_pos != active_days.end()) {
                        active_days.erase(date_pos);
                    } else {
                        throw std::runtime_error("Can't find specified date to remove from calendar_dates.txt");
                    }
                }
            }
        };

        std::ranges::for_each(calendars, parse_calendar);
        std::ranges::for_each(calendar_dates, parse_exception);

        // Return a map for faster searches, since services are only used for building the schedule and are not
        // retained. It is also convenient since the map is useful when creating the service objects.
        auto services = std::unordered_map<std::string, Service>{};
        services.reserve(service_dates_map.size());
        std::ranges::transform(service_dates_map, std::inserter(services, services.begin()), [](
                               std::pair<std::string, date_vector> service_dates) {
                                   return std::make_pair(service_dates.first,
                                                         Service(service_dates.first,
                                                                 std::move(service_dates.second)));
                               });
        return services;
    }
}
