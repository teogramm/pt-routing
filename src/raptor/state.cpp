#include "raptor/state.h"

namespace raptor {
    RaptorState::RaptorState(const Stop& origin_stop, const Stop& destination, const Time& departure_time) :
        destination(destination) {
        // TODO: Remove the need for nullopt boarding_stop
        label_manager.add_label(origin_stop, departure_time, std::nullopt, std::nullopt);
        earliest_arrival_time[origin_stop] = departure_time;
        improved_stops.insert(origin_stop);
    }

    bool RaptorState::can_improve_current_journey_to_stop(const Time& new_arrival_time,
                                                          const Stop& current_stop) const {
        auto arrival_time_to_current_stop = earliest_arrival_time.find(current_stop);
        if (arrival_time_to_current_stop == earliest_arrival_time.end())
            return true;
        auto arrival_time_to_destination = earliest_arrival_time.find(destination);
        if (arrival_time_to_destination == earliest_arrival_time.end()) {
            return new_arrival_time.get_sys_time() < arrival_time_to_current_stop->second.get_sys_time();
        }
        return new_arrival_time.get_sys_time() < std::min(arrival_time_to_current_stop->second.get_sys_time(),
                                                          arrival_time_to_destination->second.get_sys_time());
    }

    int RaptorState::new_round() {
        n_round++;
        label_manager.new_round();
        return n_round;
    }

    bool RaptorState::have_stops_to_improve() const {
        return !improved_stops.empty();
    }

    bool RaptorState::try_improve_stop_arrival_time(const Stop& stop, const Time& new_arrival_time,
                                                    const std::optional<std::reference_wrapper<const Stop>>&
                                                    boarding_stop,
                                                    const std::optional<std::pair<
                                                        std::reference_wrapper<const Route>, TripIndex>>&
                                                    route_with_trip_index) {
        if (can_improve_current_journey_to_stop(new_arrival_time, stop)) {
            label_manager.add_label(stop, new_arrival_time, boarding_stop, route_with_trip_index);
            earliest_arrival_time[stop] = new_arrival_time;
            improved_stops.insert(std::cref(stop));
            return true;
        }
        return false;
    }

    bool RaptorState::might_catch_earlier_trip(const Stop& stop, const Time& departure_time) const {
        const auto previous_journey = label_manager.get_previous_label(stop);
        return previous_journey.has_value() &&
                previous_journey->arrival_time.get_sys_time() <= departure_time.get_sys_time();
    }

    std::unordered_set<std::reference_wrapper<const Stop>> RaptorState::get_and_clear_improved_stops() {
        auto old_improved_stops = std::move(improved_stops);
        improved_stops.clear();
        return old_improved_stops;
    }

    std::unordered_set<std::reference_wrapper<const Stop>> RaptorState::get_improved_stops() const {
        return improved_stops;
    }

    Time RaptorState::current_arrival_time_to_stop(const Stop& stop) const {
        return earliest_arrival_time.at(stop);
    }

    Time RaptorState::previous_arrival_time_to_stop(const Stop& stop) const {
        return label_manager.get_previous_label(stop)->arrival_time;
    }

    // TODO: Remove this, currently used when building journeys.
    const LabelManager& RaptorState::get_label_manager() const {
        return label_manager;
    }
}
