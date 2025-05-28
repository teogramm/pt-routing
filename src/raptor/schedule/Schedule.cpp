#include "raptor/schedule/Schedule.h"

#include <ranges>
#include <boost/container_hash/hash.hpp>

namespace raptor {

    size_t Route::hash(const std::vector<std::reference_wrapper<const Stop>>& stops, const std::string& gtfs_route_id) {
        size_t seed = 0;
        boost::hash_combine(seed, std::hash<std::vector<std::reference_wrapper<const Stop>>>{}(stops));
        boost::hash_combine(seed, std::hash<std::string>{}(gtfs_route_id));
        return seed;
    }
}



std::size_t std::hash<std::vector<std::reference_wrapper<const raptor::Stop>>>::operator()(
        const std::vector<std::reference_wrapper<const raptor::Stop>>& stop) const {
    auto hasher = std::hash<std::reference_wrapper<const raptor::Stop>>{};
    std::size_t seed = 0;
    for (const auto& stop : stop) {
        boost::hash_combine(seed, hasher(stop));
    }
    return seed;
}
