#ifndef QUORUM_ARBITER__INERTIAL_CLOCK_HPP_
#define QUORUM_ARBITER__INERTIAL_CLOCK_HPP_

#include <algorithm>
#include <cassert>
#include <optional>

namespace quorum_arbiter {

/// Seconds seen through the imu stream, each step capped so a px4 stamp jump is an interval
class InertialClock final {
public:
  explicit InertialClock(double max_step_s) : max_step_s_{max_step_s} { assert(max_step_s > 0.0); }

  /// A negative step is invalid and a jump capped by max interval
  void advance(double stamp_s)
  {
    if (!last_stamp_s_.has_value()) {
      last_stamp_s_ = stamp_s;
      return;
    }

    const double step_s = stamp_s - *last_stamp_s_;

    total_s_ += std::clamp(step_s, 0.0, max_step_s_);
    last_stamp_s_ = stamp_s;
  }

  [[nodiscard]] double now_s() const noexcept { return total_s_; }

private:
  double max_step_s_{};
  std::optional<double> last_stamp_s_;
  double total_s_{};
};

}  // namespace quorum_arbiter

#endif  // QUORUM_ARBITER__INERTIAL_CLOCK_HPP_
