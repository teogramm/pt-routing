#ifndef PT_ROUTING_KD_TREE_H
#define PT_ROUTING_KD_TREE_H
#include <cmath>

#include "transfers.h"
#include "raptor/reconstruction.h"

namespace raptor {
    /**
      * Finds nearby stops by using a KD tree for Stops.
      *
      * It transforms geographic coordinates to the cartesian coordinates to approximate the distance
      * (https://timvink.nl/blog/closest-coordinates/). As such, it should only be used for small distances.
      * In addition, the accuracy is limited.
      */
    class StopKDTree final : public NearbyStopsFinder {
        using AdaptorType =
        nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double, StopKDTree>, StopKDTree, 3>;

        const std::deque<Stop>& stops;
        std::vector<std::array<double, 3>> stops_with_cartesian_coords;
        std::unique_ptr<AdaptorType> index;

        /**
         * Converts geographic to cartesian coordinates.
         * @param coordinates <Latitude, Longitude> pair
         * @return Array containing 3-dimensional cartesian coordinates
         */
        static std::array<double, 3> to_cartesian(const std::pair<double, double>& coordinates);


        /**
         * Calculates all stops in radius of the given cartesian coordinates.
         *
         * @param cartesian_coords Stop coordinates, already transformed to the cartesian system.
         * @param radius_km Search radius in kilometres
         * @return Vector of nanoflann result items.
         */
        [[nodiscard]] std::vector<nanoflann::ResultItem<uint32_t>> stops_in_radius(
                const std::array<double, 3>& cartesian_coords, double radius_km) const;

        /**
         * Converts the results returned by the nanoflann library to a vector of StopWithDistance objects.
         *
         * @param flann_results Range containing the nanoflann:ResultItem objects.
         * @return
         */
        template <std::ranges::input_range R>
            requires std::is_same_v<std::ranges::range_value_t<R>, nanoflann::ResultItem<uint32_t>>
        std::vector<StopWithDistance> convert_flann_results(R&& flann_results) const {
            auto results = std::vector<StopWithDistance>();
            std::ranges::transform(flann_results, std::back_inserter(results),
                                   [this](const auto& match) {
                                       auto& stop = stops.at(match.first);
                                       // TODO: This seems correct but verify formally
                                       auto distance = std::sqrt(match.second);
                                       return StopWithDistance{stop, distance};
                                   });
            return results;
        }

    public:

        /**
         * The class stores references to the given stops, so passing a temporary will result in those references
         * becoming invalid.
         */
        explicit StopKDTree(std::deque<Stop>&&) = delete;

        /**
         * Constructs a new index, including points for the given stops.
         * @param stops Reference to a deque container. Its lifetime must be longer than the object's.
         */
        explicit StopKDTree(const std::deque<Stop>& stops);

        /**
         * Return a factory function which creates a StopKDTree.
         */
        static Factory create_factory();

        /**
         * Find all stops near the given geographic coordinates.
         * @param latitude
         * @param longitude
         * @param radius_km Search radius in kilometres.
         * @return Vector with search results, containing each stop inside the radius, along with the distance from
         * the given stop.
         */
        std::vector<StopWithDistance> stops_in_radius(double latitude, double longitude, double radius_km) override;

        /**
         * Calculates all stops in radius of the given stop.
         *
         * If you need to calculate the stops in radius for every stop given when constructing the object,
         * use stops_in_radius(double)
         *
         * @param stop Stop object. Doesn't need to be included in the stops that were given when constructing the
         * object.
         * @param radius_km Search radius in kilometres.
         * @return Vector with search results, containing each stop inside the radius, along with the distance from
         * the given stop. Does not include the given stop.
         */
        [[nodiscard]] std::vector<StopWithDistance> stops_in_radius(const Stop& stop, double radius_km) const;

        /**
         * For all the stops given during construction of the object, calculates all other stops within the given
         * radius.
         *
         * Has better performance than stops_in_radius(const Stop&, double), as it avoids converting between coordinate
         * systems twice.
         * @param radius_km Search radius in kilometres.
         * @return Vector containing each Stop and all other Stops inside the given radius.
         */
        [[nodiscard]] std::vector<StopsInRadius> stops_in_radius(double radius_km) const;

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

#endif //PT_ROUTING_KD_TREE_H
