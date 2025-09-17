#ifndef PT_ROUTING_LINEAR_WALK_CALCULATOR_H
#define PT_ROUTING_LINEAR_WALK_CALCULATOR_H
#include "transfers.h"

namespace raptor {
    /**
     * Calculates walking times assuming a constant walking speed.
     */
    class LinearWalkTimeCalculator final : public WalkTimeCalculator {
        double walking_speed;
        double scaling_factor;

        /**
         * Calculates the great-circle distance between two points using the haversine formula.
         * @param latitude_1 Latitude of the first point in decimal degrees.
         * @param longitude_1 Longitude of the first point in decimal degrees.
         * @param latitude_2 Latitude of the second point in decimal degrees.
         * @param longitude_2 Longitude of the second point in decimal degrees.
         * @return Distance in kilometres
         */
        static double calculate_distance(double latitude_1, double longitude_1,
                                         double latitude_2, double longitude_2);

    public:
        /**
         * A scaling factor can be applied to all the calculated times to offset the accuracy loss from assuming a
         * linear path between points.
         * @param walking_speed_km_h Walking speed in kilometres per hour
         * @param time_scaling_factor Factor for multiplying calculated times. By default, times are not scaled.
         */
        explicit LinearWalkTimeCalculator(double walking_speed_km_h, double time_scaling_factor = 1.0);

        /**
         * Calculates the walking time between two coordinates assuming a straight line path and a constant walking
         * speed.
         * @param latitude_1 Latitude of the first point in decimal degrees.
         * @param longitude_1 Longitude of the first point in decimal degrees.
         * @param latitude_2 Latitude of the second point in decimal degrees.
         * @param longitude_2 Longitude of the second point in decimal degrees.
         * @return Walking time in seconds
         */
        std::chrono::seconds calculate_walking_time(double latitude_1, double longitude_1,
                                                    double latitude_2, double longitude_2) override;
        /**
         * Calculates the walking time for the given distance assuming a constant walking speed.
         * @param distance Distance in kilometres
         * @return Walking time in seconds
         */
        std::chrono::seconds calculate_walking_time(double distance) override;
    };
}

#endif //PT_ROUTING_LINEAR_WALK_CALCULATOR_H
