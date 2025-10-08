#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <algorithm>
#include <chrono>
#include <deque>
#include <string>
#include <utility>

#include <schedule/components/route.h>
#include <schedule/components/stop.h>

namespace raptor {

    class Schedule {
    public:
        Schedule() = delete;

        Schedule(std::deque<Agency>&& agencies, StopManager&& stop_manager, std::vector<Route>&& routes) :
            agencies(std::move(agencies)), stop_manager(std::move(stop_manager)),
            routes(std::move(routes)) {
        }

        [[nodiscard]] const std::vector<Route>& get_routes() const {
            return routes;
        }

        [[nodiscard]] const std::deque<Stop>& get_stops() const {
            return stop_manager.get_stops();
        }

    private:
        const std::deque<Agency> agencies;
        StopManager stop_manager;
        const std::vector<Route> routes;
    };
}

//TODO: Move hashes to respective files
template <>
struct std::hash<raptor::Stop> {
    size_t operator()(const raptor::Stop& stop) const noexcept {
        return std::hash<std::string>{}(stop.get_gtfs_id());
    }
};

//TODO: Make this work for both const and non-const types
template <>
struct std::hash<std::reference_wrapper<const raptor::Stop>> {
    size_t operator()(const std::reference_wrapper<const raptor::Stop>& stop) const noexcept {
        return std::hash<raptor::Stop>{}(stop);
    }
};

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

// template <typename T> requires std::is_convertible_v<T, const raptor::Stop>
// struct std::hash<std::reference_wrapper<T>> {
//     size_t operator()(const std::reference_wrapper<T>& stop) const {
//         return std::hash<raptor::Stop>{}(stop);
//     }
// };

template <>
struct std::hash<std::vector<std::reference_wrapper<const raptor::Stop>>> {
    size_t operator()(const std::vector<std::reference_wrapper<const raptor::Stop>>& stop) const noexcept;
};

#endif //SCHEDULE_H
