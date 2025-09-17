#include <cmath>

#include "transfers/linear_walk_calculator.h"

namespace raptor {
    double LinearWalkTimeCalculator::calculate_distance(double latitude_1, double longitude_1,
                                                        double latitude_2, double longitude_2) {
        const double R = 6371;
        // Use the Haversine formula
        double delta_phi = (latitude_2 - latitude_1) * M_PI / 180;
        double delta_lambda = longitude_2 - longitude_1 * M_PI / 180;

        double phi_1 = latitude_1 * M_PI / 180;
        double phi_2 = latitude_2 * M_PI / 180;

        double a = std::pow(std::sin(delta_phi / 2), 2) + std::cos(phi_1) * std::cos(phi_2) *
                std::pow(std::sin(delta_lambda / 2), 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

        return R * c;
    }

    LinearWalkTimeCalculator::LinearWalkTimeCalculator(const double walking_speed_km_h,
                                                       const double time_scaling_factor) :
        walking_speed(walking_speed_km_h), scaling_factor(time_scaling_factor) {
        if (walking_speed <= 0) {
            throw std::invalid_argument("Walking speed must be positive");
        }
    }

    std::chrono::seconds LinearWalkTimeCalculator::calculate_walking_time(
            const double latitude_1, const double longitude_1,
            const double latitude_2, const double longitude_2) {
        return calculate_walking_time(calculate_distance(latitude_1, longitude_1, latitude_2, longitude_2));
    }

    std::chrono::seconds LinearWalkTimeCalculator::calculate_walking_time(const double distance) {
        const auto time = 3600 * distance / walking_speed;
        return 2 * std::chrono::seconds{static_cast<int>(std::ceil(time))};
    }
}
