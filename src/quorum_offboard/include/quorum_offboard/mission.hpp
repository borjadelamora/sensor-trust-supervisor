#ifndef QUORUM_OFFBOARD__MISSION_HPP_
#define QUORUM_OFFBOARD__MISSION_HPP_

#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace quorum_offboard {

/// A point in the PX4 local NED frame: x north, y east, z down.
struct PositionNed {
  double north_m{};
  double east_m{};
  double down_m{};
};

/// Horizontal only; the climb to cruise altitude is tested separately.
[[nodiscard]] inline bool is_within(const PositionNed& position, const PositionNed& target,
                                    double radius_m)
{
  const double diff_north = position.north_m - target.north_m;
  const double diff_east = position.east_m - target.east_m;
  const double distance_sq_m2 = (diff_north * diff_north) + (diff_east * diff_east);

  return distance_sq_m2 <= (radius_m * radius_m);
}

[[nodiscard]] inline bool is_at_altitude(const PositionNed& position, double target_down_m,
                                         double tolerance_m)
{
  return std::abs(position.down_m - target_down_m) <= tolerance_m;
}

/// Walks a setpoint @p step_m towards @p target, stopping there rather than overshooting
[[nodiscard]] inline PositionNed step_towards(const PositionNed& setpoint,
                                              const PositionNed& target, double step_m)
{
  assert(step_m >= 0.0);

  const double diff_north = target.north_m - setpoint.north_m;
  const double diff_east = target.east_m - setpoint.east_m;
  const double diff_down = target.down_m - setpoint.down_m;
  const double distance_m =
      std::sqrt((diff_north * diff_north) + (diff_east * diff_east) + (diff_down * diff_down));

  if (distance_m <= step_m) {
    return target;
  }

  const double fraction = step_m / distance_m;
  return {setpoint.north_m + (fraction * diff_north), setpoint.east_m + (fraction * diff_east),
          setpoint.down_m + (fraction * diff_down)};
}

class Mission {
public:
  explicit Mission(std::vector<PositionNed> waypoints) : waypoints_(std::move(waypoints))
  {
    assert(!waypoints_.empty());
  }

  [[nodiscard]] const PositionNed& current() const
  {
    assert(!finished());
    return waypoints_[index_];
  }

  [[nodiscard]] bool finished() const { return index_ == waypoints_.size(); }

  void advance() { ++index_; }

private:
  std::vector<PositionNed> waypoints_;
  std::size_t index_{0};
};

}  // namespace quorum_offboard

#endif  // QUORUM_OFFBOARD__MISSION_HPP_
