#ifndef PT_ROUTING_STATE_H
#define PT_ROUTING_STATE_H
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <schedule/Schedule.h>


namespace raptor {
    using StopIndex = std::ranges::range_difference_t<decltype(std::declval<Route>().stop_sequence())>;
    using TripIndex = std::ranges::range_difference_t<decltype(std::declval<Route>().get_trips())>;
    using StopTimeIndex = std::ranges::range_difference_t<decltype(std::declval<Trip>().get_stop_times())>;

    /**
     * Contains information about reaching a stop. The algorithm supports two types of reaching a stop: either on
     * foot or using public transport. When travelling on foot the route_and_trip_index member is set to std::nullopt.
     *
     * The boarding_stop member is set to std::nullopt for the starting point of the journey.
     */
    struct JourneyInformation {
        Time arrival_time;
        std::optional<std::reference_wrapper<const Stop>> boarding_stop;
        std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>> route_and_trip_index;
    };

    // TODO: Use this for the route_and_trip_index parameter.
    struct JourneyInformationPTExtension {
        const Route& route;
        TripIndex trip_index;
        StopIndex stop_index;
    };

    /**
     * LabelManager is responsible for controlling the labels associated with each Stop.
     * It keeps track of two sets of the current and previous sets of labels.
     *
     * All modifications to the labels apply to the current set.
     */
    class LabelManager {
        using LabelType = JourneyInformation;
        using LabelContainer = std::unordered_map<std::reference_wrapper<const Stop>, LabelType>;

        LabelContainer current_round_labels;
        LabelContainer previous_round_labels;

        /**
         * Retrieves the label for the given stop from the given container, if it exists.
         */
        static std::optional<LabelType> get_label(const Stop& stop, const LabelContainer& labels);

    public:
        LabelManager() = default;

        /**
         * Initialises the object and adds a label to the current set.
         */
        LabelManager(const Stop& stop,
                     const Time& arrival_time,
                     const std::optional<std::reference_wrapper<const Stop>>& boarding_stop,
                     const std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>>&
                     route_with_trip_index) {
            add_label(stop, arrival_time, boarding_stop, route_with_trip_index);
        }

        /**
         * Copies the labels of the current round to the previous round
         */
        void new_round();

        /**
         * Adds or changes the value of the label for the latest set.
         * @param stop
         * @param arrival_time Arrival time at the stop.
         * @param boarding_stop Stop where the line is boarded to reach the stop.
         * @param route_with_trip_index Route used to travel between the stops along with the index of the trip used.
         */
        void add_label(const Stop& stop,
                       const Time& arrival_time,
                       const std::optional<std::reference_wrapper<const Stop>>& boarding_stop,
                       const std::optional<std::pair<std::reference_wrapper<const Route>, TripIndex>>&
                       route_with_trip_index) {
            current_round_labels.insert_or_assign(
                    stop, JourneyInformation(arrival_time, boarding_stop, route_with_trip_index));
        }

        std::optional<LabelType> get_latest_label(const Stop& stop) const;

        std::optional<LabelType> get_previous_label(const Stop& stop) const;

        using IndexWithTime = std::pair<StopIndex, Time>;

        /**
         * Get the first stop of the route that was reached in the previous round.
         * @param stops Range containing stop objects for this route in travel order.
         * @return Pair containing a reference to the stop and the arrival time to the stop. No value if there is no
         * stop in the given route that can be reached with the given number of transfers.
         */
        template <std::ranges::input_range R>
            requires std::is_convertible_v<std::ranges::range_value_t<R>, Stop>
        std::optional<IndexWithTime> find_hop_on_stop(
                R&& stops) {
            auto stops_with_journeys = previous_round_labels | std::views::keys;
            auto hop_on_stop = std::ranges::find_first_of(stops, stops_with_journeys,
                                                          {}, {}, &LabelContainer::value_type::first);
            if (hop_on_stop == std::ranges::end(stops)) {
                return std::nullopt;
            }
            return std::make_pair(std::ranges::distance(std::ranges::begin(stops), hop_on_stop),
                                  previous_round_labels.at(*hop_on_stop).arrival_time);
        }
    };

    /**
     * Manages the internal state of the algorithm. The algorithm takes place in rounds, with the round number
     * indicating the maximum number of transfers allowed to reach each stop.
     *
     * The object starts at round 0, indicating no transfers are required to reach a stop. By default, the origin stop
     * does not require any transfers to reach.
     *
     * Besides keeping track of the arrival time for the current round, the object also retains the arrival times for
     * the previous round.
     */
    class RaptorState {
        LabelManager label_manager;
        std::unordered_map<std::reference_wrapper<const Stop>, Time> earliest_arrival_time;
        std::unordered_set<std::reference_wrapper<const Stop>> improved_stops;
        int n_round = 0;
        const Stop& destination;

        /**
         * Examines if the given arrival time can be used to improve the arrival time to the given stop.
         * This happens if the given arrival time is sooner that the current arrival time at the stop, and it is not
         * later than the current arrival time at the destination.
         */
        bool can_improve_current_journey_to_stop(const Time& new_arrival_time, const Stop& current_stop) const;

    public:
        RaptorState() = delete;

        // Copying this object is probably an error. Delete to avoid doing it by mistake.
        RaptorState(const RaptorState& other) = delete;
        RaptorState& operator=(const RaptorState& other) = delete;

        /**
         * Initialises a new object and creates a label for the origin stop, using 0 transfers.
         * After the object is created, the object remains at round 0, and more arrival times can be added.
         * @param origin_stop Origin stop, used to initialise the object.
         * @param destination Destination stop, used for target pruning.
         * @param departure_time Departure time from origin stop, used to initialise the object.
         */
        RaptorState(const Stop& origin_stop, const Stop& destination, const Time& departure_time);
        /**
         * Starts a new round of the algorithm.
         * @return Number of transfers used in this round.
         */
        int new_round();

        /**
         * Checks if there are stops that might be further improved.
         * @return true if there are marked stops.
         */
        bool have_stops_to_improve() const;

        /**
         * Attempts to improve the arrival time for a stop.
         * @param boarding_stop In case of a movement with public transport stores the stop where the trip was boarded.
         * For walking movements, stores the stop where the movement started. Used for journey reconstruction.
         * @param route_with_trip_index Defined only for movements that use public transport. Used for journey
         * reconstruction.
         * @return true if the given time resulted in an improvement.
         */
        bool try_improve_stop_arrival_time(const Stop& stop, const Time& new_arrival_time,
                                           const std::optional<std::reference_wrapper<const Stop>>& boarding_stop,
                                           const std::optional<std::pair<
                                               std::reference_wrapper<const Route>, TripIndex>>&
                                           route_with_trip_index);

        /**
         * Check if it might be possible to take an earlier trip from the given stop.
         * It might be possible to catch an earlier trip if the arrival time to the stop, using one less transfer
         * than in the current round, is before the given departure time.
         * As such, it should be checked if an earlier trip can be taken if the traveller arrives at the stop earlier,
         * using one less transfer.
         *
         * @param stop Current stop, on which it might be possible to take an earlier trip.
         * @param departure_time Current departure time from the stop.
         * @return True if the arrival time at the stop using one less transfer (k-1) is earlier than the given
         * departure time.
         */
        bool might_catch_earlier_trip(const Stop& stop, const Time& departure_time) const;

        /**
         * Gets the stops which are currently marked as improved and empties the collection.
         * @return Set of improved stops
         */
        std::unordered_set<std::reference_wrapper<const Stop>> get_and_clear_improved_stops();

        /**
         * Get the stops which have been marked as improved.
         * @return Copy of the set with the stops which have been marked as improved.
         */
        [[nodiscard]] std::unordered_set<std::reference_wrapper<const Stop>> get_improved_stops() const;
        [[nodiscard]] Time current_arrival_time_to_stop(const Stop& stop) const;
        [[nodiscard]] Time previous_arrival_time_to_stop(const Stop& stop) const;
        // TODO: Remove this, currently used when building journeys.
        [[nodiscard]] const LabelManager& get_label_manager() const;
    };
}
#endif //PT_ROUTING_STATE_H
