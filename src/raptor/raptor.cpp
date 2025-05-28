#include "raptor/raptor.h"

#include "raptor/stop.h"

#include <set>
#include <vector>

namespace raptor {

    std::set<std::pair<std::string, int>> routes_serving_stop(std::string stop_id,
                                                              gtfs::StopTimes& stop_times,
                                                              gtfs::Trips& trips) {
        auto trip_ids = std::unordered_map<std::string, int>();
        for (auto& st : stop_times) {
            if (stop_id == st.stop_id) {
                trip_ids.insert(std::pair(st.trip_id, st.stop_sequence));
            }
        }
        auto routes = std::set<std::pair<std::string, int>>();
        for (auto& trip : trips) {
            if (trip_ids.contains(trip.trip_id)) {
                routes.insert(std::pair(trip.route_id, trip_ids.at(trip.trip_id)));
            }
        }
        return routes;
    }

    void raptor(std::string& source_stop, std::string& target_stop,
                int departure_time, std::vector<stop>& all_stops,
                gtfs::StopTimes& stop_times, gtfs::Routes& routes,
                gtfs::Trips& trips) {
        constexpr auto n_rounds = 5;
        auto stop_item = std::ranges::find(all_stops, source_stop, &stop::id);
        stop_item->arrival_time[0] = departure_time;
        auto improved_stops = std::set{source_stop};
        for (int k = 0; k < n_rounds; k++) {
            auto q = std::unordered_map<std::string, std::pair<std::string, int>>();
            for (auto& s : improved_stops) {
                auto stop_routes = routes_serving_stop(s, stop_times, trips);
                for (auto& r : stop_routes) {
                    auto route_id = r.first;
                    auto sequence = r.second;
                    if (q.contains(route_id)) {
                        auto other_sequence = std::get<1>(q[route_id]);
                        if (other_sequence > sequence) {
                            q[route_id] = std::pair(s, sequence);
                        }
                    }
                    else {
                        q[route_id] = std::pair(s, sequence);
                    }
                }
            }
            improved_stops.clear();


        }
    }
} // namespace raptor
