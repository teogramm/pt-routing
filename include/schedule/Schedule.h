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

#endif //SCHEDULE_H
