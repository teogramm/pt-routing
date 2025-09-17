#include <ranges>

#include "schedule/Schedule.h"
#include "transfers/kd_tree.h"

namespace raptor {

    StopKDTree::StopKDTree(const std::deque<Stop>& stops) :
        stops(stops) {
        stops_with_cartesian_coords = std::vector<std::array<double, 3>>();
        stops_with_cartesian_coords.reserve(stops.size());
        std::ranges::transform(stops, std::back_inserter(stops_with_cartesian_coords),
                               [](const std::pair<double, double>& coords) {
                                   return to_cartesian(coords);
                               }, &Stop::get_coordinates);
        index = std::make_unique<AdaptorType>(3, *this);
    }

    NearbyStopsFinder::Factory StopKDTree::create_factory() {
        // TODO: Avoid creating a new lamba at every invocation
        return [](const std::deque<Stop>& stops) {
            return std::make_unique<StopKDTree>(stops);
        };
    }

    std::array<double, 3> StopKDTree::to_cartesian(const std::pair<double, double>& coordinates) {
        auto latitude = coordinates.first * (M_PI / 180.0);
        auto longitude = coordinates.second * (M_PI / 180.0);

        auto R = 6371.0;
        auto X = R * std::cos(latitude) * std::cos(longitude);
        auto Y = R * std::cos(latitude) * std::sin(longitude);
        auto Z = R * std::sin(latitude);

        return {X, Y, Z};
    }

    std::vector<nanoflann::ResultItem<uint32_t>> StopKDTree::stops_in_radius(
            const std::array<double, 3>& cartesian_coords, double radius_km) const {
        std::vector<nanoflann::ResultItem<uint32_t>> ret_matches;
        index->radiusSearch(cartesian_coords.data(), std::sqrt(radius_km), ret_matches);
        return ret_matches;
    }

    std::vector<StopWithDistance> StopKDTree::stops_in_radius(double latitude, double longitude, double radius_km) {
        auto stop_cartesian_coords = to_cartesian({latitude, longitude});
        auto ret_matches = stops_in_radius(stop_cartesian_coords, radius_km);
        auto results = convert_flann_results(ret_matches);
        return results;
    }

    std::vector<StopWithDistance>
    StopKDTree::stops_in_radius(const Stop& stop, double radius_km) const {
        auto stop_cartesian_coords = to_cartesian(stop.get_coordinates());
        auto ret_matches = stops_in_radius(stop_cartesian_coords, radius_km);

        auto is_not_search_stop = [this, stop](const nanoflann::ResultItem<uint32_t>& result_item) {
            return stop != stops.at(result_item.first);
        };

        auto results = convert_flann_results(ret_matches | std::views::filter(is_not_search_stop));

        return results;
    }

    std::vector<StopsInRadius> StopKDTree::stops_in_radius(const double radius_km) const {
        auto results = std::vector<StopsInRadius>();
        results.reserve(stops.size());

        for (int i = 0; i < stops.size(); i++) {
            auto coordinates = stops_with_cartesian_coords.at(i);
            auto nearby_stops = stops_in_radius(coordinates, radius_km);

            auto is_not_current_stop = [i](const nanoflann::ResultItem<uint32_t>& result_item) {
                return result_item.first != i;
            };

            auto converted_stops = convert_flann_results(
                    nearby_stops | std::ranges::views::filter(is_not_current_stop));
            results.emplace_back(stops.at(i), std::move(converted_stops));
        }
        return results;
    }
}
