#ifndef QUORUM_MONITORS__VELOCITY_REFERENCE_HPP_
#define QUORUM_MONITORS__VELOCITY_REFERENCE_HPP_

#include <cassert>
#include <cmath>
#include <expected>
#include <optional>
#include <string_view>

#include <Eigen/Geometry>

namespace quorum_monitors {

/// Specific force straight off the accelerometer, -9.8 down in a level hover
struct AccelerationFrd {
  double forward_mps2{};
  double right_mps2{};
  double down_mps2{};
};

struct AccelerationNed {
  double north_mps2{};
  double east_mps2{};
  double down_mps2{};
};

struct VelocityNed {
  double north_mps{};
  double east_mps{};
  double down_mps{};
};

/// Unit quaternion in px4's w, x, y, z order, rotating body FRD to NED
struct Attitude {
  double w{};
  double x{};
  double y{};
  double z{};
};

struct ImuSample {
  AccelerationFrd acceleration{};
  double time_s{};
};

struct AttitudeSample {
  Attitude attitude{};
  double time_s{};
};

/// max_gap_s bounds a single integration step, max_age_s the whole life of an anchor
struct ReferenceLimits {
  double max_gap_s{};
  double max_age_s{};
};

/// kGap is one interval too long, kExpired an anchor too old
enum class ReferenceError { kNoAnchor, kNotAdvanced, kGap, kExpired };

[[nodiscard]] constexpr std::string_view to_string(ReferenceError error)
{
  switch (error) {
    case ReferenceError::kNoAnchor:
      return "No Anchor";

    case ReferenceError::kNotAdvanced:
      return "Not Advanced";

    case ReferenceError::kGap:
      return "Gap";

    case ReferenceError::kExpired:
      return "Expired";
  }

  return "Unknown Reference Error";
}

/// Re-expresses a body FRD vector in NED. Requires @p attitude to be a unit quaternion
[[nodiscard]] inline AccelerationNed rotate_to_ned(const AccelerationFrd& acceleration,
                                                   const Attitude& attitude)
{
  const Eigen::Quaterniond rotation(attitude.w, attitude.x, attitude.y, attitude.z);
  // Unit norm to 1e-6, the tolerance px4's float quaternion leaves after conversion
  assert(std::abs(rotation.norm() - 1.0) < 1e-6);

  const Eigen::Vector3d acceleration_frd(acceleration.forward_mps2, acceleration.right_mps2,
                                         acceleration.down_mps2);
  const Eigen::Vector3d acceleration_ned = rotation * acceleration_frd;

  return AccelerationNed{.north_mps2 = acceleration_ned[0],
                         .east_mps2 = acceleration_ned[1],
                         .down_mps2 = acceleration_ned[2]};
}

// The gravity value the gazebo worlds use
constexpr double kGravityMps2 = 9.8;

/// Turns specific force into inertial acceleration
[[nodiscard]] inline AccelerationNed remove_gravity(const AccelerationNed& acceleration)
{
  return AccelerationNed{.north_mps2 = acceleration.north_mps2,
                         .east_mps2 = acceleration.east_mps2,
                         .down_mps2 = acceleration.down_mps2 + kGravityMps2};
}

/// Forward Euler, holding @p acceleration across @p dt_s. Requires @p dt_s > 0
[[nodiscard]] inline VelocityNed integrate(const VelocityNed& previous,
                                           const AccelerationNed& acceleration, double dt_s)
{
  assert(dt_s > 0.0);
  return VelocityNed{.north_mps = previous.north_mps + acceleration.north_mps2 * dt_s,
                     .east_mps = previous.east_mps + acceleration.east_mps2 * dt_s,
                     .down_mps = previous.down_mps + acceleration.down_mps2 * dt_s};
}

/// Velocity dead-reckoned from a gps anchor, held whole or not at all
class VelocityReference {
public:
  explicit VelocityReference(ReferenceLimits limits) : limits_(limits)
  {
    assert(limits.max_gap_s > 0.0);
    assert(limits.max_age_s > 0.0);
  }

  void anchor(VelocityNed velocity, double time_s)
  {
    assert(time_s > 0.0);
    estimate_ = Estimate{.velocity = velocity, .anchored_at_s = time_s, .updated_at_s = time_s};
  }

  /// Integrates one imu sample, dropping the estimate on a gap
  [[nodiscard]] std::expected<VelocityNed, ReferenceError> advance(
      const ImuSample& imu_sample, const AttitudeSample& attitude_sample)
  {
    if (!estimate_) {
      return std::unexpected(ReferenceError::kNoAnchor);
    }

    const double dt_s = imu_sample.time_s - estimate_->updated_at_s;

    if (dt_s <= 0.0) {
      return std::unexpected(ReferenceError::kNotAdvanced);
    }

    if (dt_s > limits_.max_gap_s) {
      reset();
      return std::unexpected(ReferenceError::kGap);
    }

    const AccelerationNed acceleration =
        remove_gravity(rotate_to_ned(imu_sample.acceleration, attitude_sample.attitude));
    const VelocityNed new_velocity = integrate(estimate_->velocity, acceleration, dt_s);
    estimate_->velocity = new_velocity;
    estimate_->updated_at_s = imu_sample.time_s;

    return new_velocity;
  }

  [[nodiscard]] std::expected<VelocityNed, ReferenceError> velocity(double now_s) const
  {
    if (!estimate_) {
      return std::unexpected(ReferenceError::kNoAnchor);
    }

    if (now_s - estimate_->anchored_at_s > limits_.max_age_s) {
      return std::unexpected(ReferenceError::kExpired);
    }

    return estimate_->velocity;
  }

  void reset() { estimate_.reset(); }

  [[nodiscard]] bool anchored() const noexcept { return estimate_.has_value(); }

private:
  struct Estimate {
    VelocityNed velocity{};
    double anchored_at_s{};
    double updated_at_s{};
  };

  ReferenceLimits limits_;
  std::optional<Estimate> estimate_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__VELOCITY_REFERENCE_HPP_