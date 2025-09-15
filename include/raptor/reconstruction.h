#ifndef PT_ROUTING_RECONSTRUCTION_H
#define PT_ROUTING_RECONSTRUCTION_H
#include <cstddef>
#include <functional>
#include <ranges>
#include <variant>

#include "schedule/Schedule.h"

namespace raptor {
    using LatLng = std::pair<double, double>;

    class BaseMovement {
        std::reference_wrapper<const Stop> from_stop;
        std::reference_wrapper<const Stop> to_stop;
        std::vector<LatLng> shape;

    public:
        BaseMovement() = delete;

        [[nodiscard]] std::vector<LatLng> get_shape() const {
            return shape;
        }

        [[nodiscard]] const Stop& get_from_stop() const {
            return from_stop;
        }

        [[nodiscard]] const Stop& get_to_stop() const {
            return to_stop;
        }

    protected:
        BaseMovement(const Stop& from_stop, const Stop& to_stop, std::vector<LatLng> shape) :
            from_stop(from_stop), to_stop(to_stop), shape(std::move(shape)) {

        }
    };

    class PTMovement final : public BaseMovement {
        std::reference_wrapper<const Trip> trip;
        std::reference_wrapper<const Route> route;
        std::size_t from_stop_idx;
        std::size_t to_stop_idx;

    public:
        PTMovement() = delete;

        PTMovement(const Trip& trip, const std::size_t from_stop_idx, const std::size_t to_stop_idx,
                   const Route& route, std::vector<LatLng> shape = {}) :
            BaseMovement(trip.get_stop_time(from_stop_idx).get_stop(),
                         trip.get_stop_time(to_stop_idx).get_stop(),
                         std::move(shape)),
            trip(trip),
            route(route),
            from_stop_idx(from_stop_idx),
            to_stop_idx(to_stop_idx) {
        }

        [[nodiscard]] const Route& get_route() const {
            return route;
        }

        [[nodiscard]] std::vector<std::reference_wrapper<const StopTime>> get_stop_times() const {
            auto stop_times = std::vector<std::reference_wrapper<const StopTime>>{};
            stop_times.reserve(to_stop_idx - from_stop_idx + 1);
            auto trip_stop_times = trip.get().get_stop_times() | std::views::transform([&](const StopTime& st) {
                return std::cref(st);
            });
            return {std::next(trip_stop_times.begin(), from_stop_idx),
                    std::next(trip_stop_times.begin(), to_stop_idx)};
        }

        [[nodiscard]] Time get_departure_time() const {
            return trip.get().get_stop_times().at(from_stop_idx).get_departure_time();
        }

        [[nodiscard]] Time get_arrival_time() const {
            return trip.get().get_stop_times().at(to_stop_idx).get_arrival_time();
        }
    };

    class WalkingMovement final : public BaseMovement {
    public:
        WalkingMovement() = delete;

        WalkingMovement(const Stop& from_stop, const Stop& to_stop, std::vector<LatLng> shape) :
            BaseMovement(from_stop, to_stop, std::move(shape)) {

        }

    };

    using Movement = std::variant<WalkingMovement, PTMovement>;
}
#endif //PT_ROUTING_RECONSTRUCTION_H
