#ifndef PT_ROUTING_AGENCY_H
#define PT_ROUTING_AGENCY_H

#include <chrono>
#include <string>

namespace raptor {
    class Agency {
        std::string gtfs_id;
        std::string name;
        std::string url;
        const std::chrono::time_zone* time_zone;

    public:
        Agency(std::string gtfs_id, std::string name,
               std::string url, const std::chrono::time_zone* time_zone) :
            gtfs_id(std::move(gtfs_id)), name(std::move(name)), url(std::move(url)), time_zone(time_zone) {
        }

        [[nodiscard]] const std::chrono::time_zone* get_time_zone() const {
            return time_zone;
        }

        [[nodiscard]] const std::string& get_gtfs_id() const {
            return gtfs_id;
        }

        [[nodiscard]] const std::string& get_name() const {
            return name;
        }

        [[nodiscard]] const std::string& get_url() const {
            return url;
        }

        friend bool operator==(const Agency& lhs, const Agency& rhs) {
            return lhs.gtfs_id == rhs.gtfs_id;
        }

        friend bool operator!=(const Agency& lhs, const Agency& rhs) {
            return !(lhs == rhs);
        }
    };
}

#endif //PT_ROUTING_AGENCY_H