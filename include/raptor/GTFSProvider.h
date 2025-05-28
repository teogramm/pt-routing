
#ifndef GTFSPROVIDER_H
#define GTFSPROVIDER_H
#include "just_gtfs/just_gtfs.h"

#include <chrono>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

namespace raptor {
class GTFSProvider {
public:
  explicit GTFSProvider(gtfs::Feed feed): feed(std::move(feed)) {};

  std::set<std::string>
  active_services(const std::chrono::year_month_day &start_time);
  void routes_serving_stop(const std::string& stop_id,
                           const std::chrono::zoned_time<std::chrono::seconds> & start_time);

private:
  gtfs::Feed feed;
};
} // namespace raptor

#endif // GTFSPROVIDER_H
