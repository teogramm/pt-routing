
#ifndef STOP_H
#define STOP_H
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace raptor {
class stop {
public:
  explicit stop(std::string id) : id(std::move(id)) {}
  std::string id;
  std::vector<int> arrival_time{std::numeric_limits<int>::max()};

  void add_round() {
    arrival_time.push_back(arrival_time.back());
  }

};

} // namespace raptor

#endif // STOP_H
