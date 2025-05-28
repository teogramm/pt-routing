#include "raptor/GTFSProvider.h"

#include <chrono>
#include <just_gtfs/just_gtfs.h>
#include <ranges>
#include <set>

namespace raptor {

bool is_available(const gtfs::CalendarItem &calendar,
                  const std::chrono::weekday &weekday) {
  gtfs::CalendarAvailability availability;
  if (weekday == std::chrono::Monday)
    availability = calendar.monday;
  else if (weekday == std::chrono::Tuesday)
    availability = calendar.tuesday;
  else if (weekday == std::chrono::Wednesday)
    availability = calendar.wednesday;
  else if (weekday == std::chrono::Thursday)
    availability = calendar.thursday;
  else if (weekday == std::chrono::Friday)
    availability = calendar.friday;
  else if (weekday == std::chrono::Saturday)
    availability = calendar.saturday;
  else if (weekday == std::chrono::Sunday)
    availability = calendar.sunday;
  else
    throw std::invalid_argument("Invalid weekday");
  return availability == gtfs::CalendarAvailability::Available;
}

std::chrono::year_month_day gtfs_date_to_year_month_day(gtfs::Date &date) {
  auto tuple = date.get_yyyy_mm_dd();
  auto year = std::get<0>(tuple);
  auto month = std::get<1>(tuple);
  auto day = std::get<2>(tuple);
  return std::chrono::year_month_day{std::chrono::year(year),
                                     std::chrono::month(month),
                                     std::chrono::day(day)};
}

template <typename Duration, typename  Tz>
std::chrono::zoned_time<Duration, Tz>
add_time_to_date(std::chrono::zoned_time<Duration, Tz> tp, gtfs::Time &time) {
  auto day =
    std::chrono::floor<std::chrono::days>(tp.get_local_time());
  auto [hours, minutes, seconds] = time.get_hh_mm_ss();
  auto new_time = day + std::chrono::seconds{seconds} + std::chrono::minutes{minutes} + std::chrono::hours{hours};
  return std::chrono::zoned_time{tp.get_time_zone(), new_time};
}

std::set<std::string>
GTFSProvider::active_services(const std::chrono::year_month_day &start_time) {
  auto service_ids = std::set<std::string>();
  auto normal_calendars = feed.get_calendar();
  auto weekday = std::chrono::weekday(start_time);
  for (auto &calendar : normal_calendars) {
    auto available = is_available(calendar, weekday);
    if (available) {
      auto calendar_start = gtfs_date_to_year_month_day(calendar.start_date);
      auto calendar_end = gtfs_date_to_year_month_day(calendar.end_date);

      if (start_time >= calendar_start && start_time <= calendar_end)
        service_ids.insert(calendar.service_id);
    }
  }
  // Parse exceptions
  auto exceptions = feed.get_calendar_dates();
  for (auto &exception : exceptions) {
    if (start_time == gtfs_date_to_year_month_day(exception.date)) {
      if (exception.exception_type == gtfs::CalendarDateException::Added) {
        service_ids.insert(exception.service_id);
      } else {
        // TODO: What if it doesn't exist
        service_ids.erase(exception.service_id);
      }
    }
  }
  return service_ids;
}

void GTFSProvider::routes_serving_stop(
    const std::string &stop_id,
    const std::chrono::zoned_time<std::chrono::seconds> &start_time) {
  auto trip_ids = std::unordered_map<std::string, gtfs::StopTime>();
  for (auto &st : feed.get_stop_times()) {
    if (stop_id == st.stop_id) {
      trip_ids[st.trip_id] = st;
    }
  }
  auto trips = std::vector<gtfs::Trip>();
  for (auto &trip : feed.get_trips()) {
    if (trip_ids.contains(trip.trip_id)) {
      trips.push_back(trip);
    }
  }
  // Find the trips that are active during the time
  auto services = active_services(std::chrono::year_month_day{
      std::chrono::floor<std::chrono::days>(start_time.get_local_time())});
  std::erase_if(trips, [services](auto &trip) {
    return !services.contains(trip.service_id);
  });
  // Convert running stop times to the next time point
  auto time_points =
      std::unordered_map<std::string,
                         std::chrono::zoned_time<std::chrono::seconds>>();
  for (auto &trip : trips) {
    auto &st = trip_ids[trip.trip_id];
    auto dt = add_time_to_date(start_time, st.departure_time);
    auto a = 1;
  }

}
} // namespace raptor
