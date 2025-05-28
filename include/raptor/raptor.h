
#ifndef RAPTOR_H
#define RAPTOR_H
#include "just_gtfs/just_gtfs.h"
#include "stop.h"

namespace raptor {
void raptor(std::string &source_stop, std::string &target_stop,
            int departure_time, std::vector<stop> &all_stops,
            gtfs::StopTimes &stop_times,
            gtfs::Routes &routes,
            gtfs::Trips &trips);
}

#endif //RAPTOR_H
