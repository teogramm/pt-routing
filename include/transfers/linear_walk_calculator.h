#ifndef PT_ROUTING_LINEAR_WALK_CALCULATOR_H
#define PT_ROUTING_LINEAR_WALK_CALCULATOR_H
#include "transfers.h"

namespace raptor {
    /**
     * Calculates walking times assuming a constant walking speed.
     */
    class LinearWalkTimeCalculator final : public WalkTimeCalculator {
        double walking_speed;

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
        explicit LinearWalkTimeCalculator(double walking_speed_km_h);

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
         * Calculates the walking time for the given distance assuming a contsant walking speed.
         * @param distance Distance in kilometres
         * @return Walking time in seconds
         */
        std::chrono::seconds calculate_walking_time(double distance) override;
    };
}

#endif //PT_ROUTING_LINEAR_WALK_CALCULATOR_H
