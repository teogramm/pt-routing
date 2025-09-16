#ifndef PT_ROUTING_TRANSFERS_H
#define PT_ROUTING_TRANSFERS_H
#include <chrono>

#include <nanoflann.hpp>
#include "schedule/Schedule.h"

namespace raptor {

    struct StopWithDistance {
        const Stop& stop;
        const double distance_m;
    };

    struct StopsInRadius {
        const Stop& original_stop;
        const std::vector<StopWithDistance> nearby_stops;
    };

    /**
     * KD Tree for Stops.
     *
     * It transforms geographic coordinates to the cartesian coordinates to approximate the distance
     * (https://timvink.nl/blog/closest-coordinates/). As such, it should only be used for small distances.
     */
    class StopKDTree {
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
                                       return StopWithDistance{stop, match.second};
                                   });
            return results;
        }

    public:
        StopKDTree() = delete;

        /**
         * Constructs a new index, including points for the given stops.
         * @param stops Reference to a deque container. Its lifetime must be longer than the object's.
         */
        explicit StopKDTree(const std::deque<Stop>& stops);

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

    class TransferManager {
        const std::deque<Stop>& stops;

        using StopWithDuration = std::pair<std::reference_wrapper<const Stop>, std::chrono::seconds>;
        std::unordered_map<std::reference_wrapper<const Stop>, std::vector<StopWithDuration>> transfers;

        std::vector<StopWithDuration> empty;

        void build_same_station_transfers() {
            std::unordered_map<std::string, std::vector<std::reference_wrapper<const Stop>>> stops_per_parent_station;
            for (const auto& stop : stops) {
                auto parent_station_id = std::string(stop.get_parent_stop_id());
                if (!parent_station_id.empty()) {
                    stops_per_parent_station[parent_station_id].emplace_back(std::cref(stop));
                }
            }
            // Create a transfer between all stops in the same parent station.
            for (auto& stops : stops_per_parent_station | std::views::values) {
                // Make sure to remove the stop itself from the list of stops that can be transferred
                if (stops.size() > 1) {
                    for (auto from_stop : stops) {
                        auto transfers_with_times = std::vector<std::pair<std::reference_wrapper<const Stop>,
                                                                          std::chrono::seconds>>{};
                        auto is_not_this_stop = [&from_stop](const Stop& other_stop) {
                            return from_stop != other_stop;
                        };
                        std::ranges::transform(stops | std::views::filter(is_not_this_stop),
                                               std::back_inserter(transfers_with_times),
                                               [](const Stop& to_stop) {
                                                   return std::make_pair(std::cref(to_stop), std::chrono::seconds{60});
                                               });
                        this->transfers.emplace(from_stop, std::move(transfers_with_times));
                    }
                }
            }
        }

        void build_on_foot_transfers(double max_radius_km) {
            auto kd = StopKDTree(stops);
            for (const auto& stop : stops) {
                auto nearby_stops = kd.stops_in_radius(stop, max_radius_km);

                auto& existing_transfers = transfers[stop];
                auto no_existing_transfer = [&existing_transfers](const StopWithDistance& to_stop) {
                    return std::ranges::all_of(existing_transfers,
                                               [&to_stop](const Stop& existing_stop) {
                                                   return existing_stop != to_stop.stop;
                                               }, &StopWithDuration::first);
                };

                std::ranges::transform(nearby_stops | std::views::filter(no_existing_transfer),
                                       std::back_inserter(existing_transfers),
                                       [](const StopWithDistance& to_stop) {
                                           // TODO: Customisable walking speed
                                           const int transfer_time =
                                                   std::round(3600 * to_stop.distance_m / 5 * 2 + 120);
                                           return std::make_pair(std::cref(to_stop.stop),
                                                                 std::chrono::seconds{transfer_time});
                                       });
            }
        }

        void build_transfers() {
            /*TODO: Process GTFS transfers. Maybe all this should be done in the Schedule class.*/
            build_same_station_transfers();
            build_on_foot_transfers(1);
        }

    public:
        explicit TransferManager(const std::deque<Stop>& stops) :
            stops(stops) {
            build_transfers();
        }

        const std::vector<StopWithDuration>& get_transfers_from_stop(const Stop& stop) const {
            const auto stop_transfers = transfers.find(stop);
            if (stop_transfers == transfers.end()) {
                // TODO: This seems a bit meh
                return empty;
            }
            return stop_transfers->second;
        }
    };

}

#endif //PT_ROUTING_TRANSFERS_H
