#ifndef PT_ROUTING_ROUTE_H
#define PT_ROUTING_ROUTE_H
#include <string>
#include <vector>

#include <schedule/components/agency.h>
#include <schedule/components/trip.h>

namespace raptor {
    /**
     * A route is a collection of trips, which stop at exactly the same stops, in the same order, and have the same
     * GTFS route ID.
     */
    class Route {
        std::vector<Trip> trips;
        std::string short_name;
        std::string long_name;
        std::string gtfs_id;
        std::reference_wrapper<const Agency> agency;

    public:
        Route(std::vector<Trip>&& trips, std::string short_name,
              std::string long_name, std::string gtfs_id, const Agency& agency) :
            trips(std::move(trips)),
            short_name(std::move(short_name)),
            long_name(std::move(long_name)),
            gtfs_id(std::move(gtfs_id)),
            agency(std::cref(agency)) {
        }

        [[nodiscard]] const std::vector<Trip>& get_trips() const {
            return trips;
        }

        [[nodiscard]] const std::string& get_short_name() const {
            return short_name;
        }

        [[nodiscard]] const std::string& get_long_name() const {
            return long_name;
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        friend bool operator==(const Route& lhs, const Route& rhs) {
            return lhs.trips == rhs.trips && lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const Route& lhs, const Route& rhs) {
            return !(lhs == rhs);
        }

        /**
         * Return an ordered vector of the stops this route passes through.
         * All trips in a route have the same stop sequence.
         */
        [[nodiscard]] std::vector<std::reference_wrapper<const Stop>> stop_sequence() const;

        [[nodiscard]] size_t hash() const;

        static size_t hash(const std::vector<std::reference_wrapper<const Stop>>& stops,
                           const std::string& gtfs_route_id);
    };
}

template <>
struct std::hash<raptor::Route> {
    size_t operator()(const raptor::Route& route) const noexcept {
        return std::hash<std::string>{}(route.get_gtfs_id());
    }
};

template <>
struct std::hash<std::reference_wrapper<const raptor::Route>> {
    size_t operator()(const raptor::Route& route) const noexcept {
        return std::hash<raptor::Route>{}(route);
    }
};

#endif //PT_ROUTING_ROUTE_H
