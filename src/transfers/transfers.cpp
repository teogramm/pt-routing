#include <ranges>
#include <transfers/transfers.h>

namespace raptor {
    void TransferManager::build_same_station_transfers() {
        // Create a transfer between all stops in the same parent station.
        for (const auto& from_stop: stops) {
            if (auto parent_station = from_stop.get_parent_station()) {
                auto stops_in_station = parent_station->get().get_stops();
                auto is_not_this_stop = [&from_stop](const Stop& other_stop) {
                    return from_stop != other_stop;
                };

                auto transfers_with_times = std::vector<std::pair<std::reference_wrapper<const Stop>,
                                                  std::chrono::seconds>>{};
                std::ranges::transform(stops_in_station | std::views::filter(is_not_this_stop),
                                       std::back_inserter(transfers_with_times),
                                       [](const Stop& to_stop) {
                                           return std::make_pair(std::cref(to_stop), std::chrono::seconds{60});
                                       });
                this->transfers.emplace(from_stop, std::move(transfers_with_times));
            }
        }
    }

    void TransferManager::build_on_foot_transfers() {
        for (const auto& origin_stop : stops) {
            auto [latitude, longitude] = origin_stop.get_coordinates();
            auto nearby_stops = nearby_stops_finder->stops_in_radius(latitude, longitude, max_radius_km);

            auto& existing_transfers = transfers[origin_stop];
            // Given a destination stop, it checks that there is no existing transfer defined between it and the
            // origin stop.
            auto no_existing_transfer = [&existing_transfers](const StopWithDistance& to_stop) {
                return std::ranges::all_of(existing_transfers,
                                           [&to_stop](const Stop& existing_stop) {
                                               return existing_stop != to_stop.stop;
                                           }, &StopWithDuration::first);
            };

            std::ranges::transform(nearby_stops | std::views::filter(no_existing_transfer),
                                   std::back_inserter(existing_transfers),
                                   [this](const StopWithDistance& to_stop) {
                                       auto walk_time =
                                               walk_time_calculator->calculate_walking_time(to_stop.distance_km);
                                       auto transfer_time = walk_time + exit_station_duration;
                                       return std::make_pair(std::cref(to_stop.stop), transfer_time);
                                   });
        }
    }

    const std::vector<TransferManager::StopWithDuration>&
    TransferManager::get_transfers_from_stop(const Stop& stop) const {
        const auto stop_transfers = transfers.find(stop);
        if (stop_transfers == transfers.end()) {
            // TODO: This seems a bit meh
            return empty;
        }
        return stop_transfers->second;
    }
}
