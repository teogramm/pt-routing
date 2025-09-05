#ifndef PT_ROUTING_KDTREESTOPVECTORADAPTOR_H
#define PT_ROUTING_KDTREESTOPVECTORADAPTOR_H

#include <deque>


/**
 * KD Tree for Stops.
 *
 * It transforms geographic coordinates to the cartesian coordinates to approximate the distance
 * (https://timvink.nl/blog/closest-coordinates/). As such, it should only be used for small distances.
 */
namespace raptor {
    class StopKDTree {
    public:
        struct StopWithDistance {
            const Stop& stop;
            const double distance_m;
        };

        struct StopsInRadius {
            const Stop& original_stop;
            const std::vector<StopWithDistance> nearby_stops;
        };

    private:
        using AdaptorType = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double, StopKDTree>,
                                                                StopKDTree, 3>;

        const std::deque<Stop>& stops;
        std::vector<std::array<double, 3>> stops_with_cartesian_coords;
        std::unique_ptr<AdaptorType> index;

        /**
         * Converts geographic to cartesian coordinates.
         * @param coordinates <Latitude, Longitude> pair
         * @return Array containing 3-dimensional cartesian coordinates
         */
        static std::array<double, 3> to_cartesian(const std::pair<double, double>& coordinates) {
            auto latitude = coordinates.first * (M_PI / 180.0);
            auto longitude = coordinates.second * (M_PI / 180.0);

            auto R = 6371.0;
            auto X = R * std::cos(latitude) * std::cos(longitude);
            auto Y = R * std::cos(latitude) * std::sin(longitude);
            auto Z = R * std::sin(latitude);

            return {X, Y, Z};
        }

        /**
         * Calculates all stops in radius of the given cartesian coordinates.
         *
         * @param cartesian_coords Stop coordinates, already transformed to the cartesian system.
         * @param radius_km Search radius in kilometres
         * @return Vector of nanoflann result items.
         */
        [[nodiscard]] std::vector<nanoflann::ResultItem<uint32_t>> stops_in_radius(
                const std::array<double, 3>& cartesian_coords, double radius_km) const {
            std::vector<nanoflann::ResultItem<uint32_t>> ret_matches;
            index->radiusSearch(cartesian_coords.data(), std::sqrt(radius_km), ret_matches);
            return ret_matches;
        }

        template <std::ranges::input_range R>
            requires std::is_same_v<std::ranges::range_value_t<R>, nanoflann::ResultItem<uint32_t>>
        std::vector<StopWithDistance> convert_flann_results(R&& flann_results) const{
            auto results = std::vector<StopWithDistance>();
            std::ranges::transform(flann_results, std::back_inserter(results),
                                   [this](const auto& match) {
                                       auto& stop = stops.at(match.first);
                                       return StopWithDistance{stop, match.second};
                                   });
            return results;
        }

    public:
        /**
         * Constructs a new index, including points for the given stops.
         * @param stops Reference to a deque container. Its lifetime must be longer than the object's.
         */
        explicit StopKDTree(const std::deque<Stop>& stops) :
            stops(stops) {
            stops_with_cartesian_coords = std::vector<std::array<double, 3>>();
            stops_with_cartesian_coords.reserve(stops.size());
            std::ranges::transform(stops, std::back_inserter(stops_with_cartesian_coords),
                                   [](const std::pair<double, double>& coords) {
                                       return to_cartesian(coords);
                                   }, &Stop::get_coordinates);
            index = std::make_unique<AdaptorType>(3, *this);
        }

        /**
         * Calculates all stops in radius of the given stop.
         *
         * If you need to calculate the stops in radius for every stop given when constructing the object,
         * use stops_in_radius(double)
         *
         * @param stop Stop object. Doesn't need to be included in the stops that were given when constructing the
         * object.
         * @return Vector with search results, containing each stop inside the radius, along with the distance from
         * the given stop.
         */
        [[nodiscard]] std::vector<StopWithDistance> stops_in_radius(const Stop& stop, double radius_km) const {
            auto stop_cartesian_coords = to_cartesian(stop.get_coordinates());

            auto ret_matches = stops_in_radius(stop_cartesian_coords, radius_km);

            auto is_not_search_stop = [this, stop](const nanoflann::ResultItem<uint32_t>& result_item) {
                return stop != stops.at(result_item.first);
            };

            auto results = convert_flann_results(ret_matches);

            return results;
        }

        /**
         * For all the stops given during construction of the object, calculates all other stops within the given
         * radius.
         *
         * Has better performance than stops_in_radius(const Stop&, double), as it avoids converting between coordinate
         * systems twice.
         * @param radius_km Search radius in kilometres.
         * @return
         */
        [[nodiscard]] std::vector<StopsInRadius> stops_in_radius(double radius_km) const {
            auto results = std::vector<StopsInRadius>();
            results.reserve(stops.size());

            for (int i = 0; i < stops.size(); i++) {
                auto coordinates = stops_with_cartesian_coords.at(i);
                auto nearby_stops = stops_in_radius(coordinates, radius_km);

                auto is_not_current_stop = [i](const nanoflann::ResultItem<uint32_t>& result_item) {
                    return result_item.first != i;
                };

                auto converted_stops = convert_flann_results(nearby_stops | std::ranges::views::filter(is_not_current_stop));
                results.emplace_back(stops.at(i), std::move(converted_stops));
            }
        }

        /*
         * Functions required by nanoflann
         */

        size_t kdtree_get_point_count() const {
            return stops_with_cartesian_coords.size();
        }

        double kdtree_get_pt(const size_t idx, int dim) const {
            return stops_with_cartesian_coords.at(idx).at(dim);
        }

        template <class BBOX>
        bool kdtree_get_bbox(BBOX&) const {
            return false;
        }
    };
}


#endif //PT_ROUTING_KDTREESTOPVECTORADAPTOR_H
