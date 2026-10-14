#ifndef QUORUM_MONITORS__GNSS_MONITOR_HPP_
#define QUORUM_MONITORS__GNSS_MONITOR_HPP_

#include <cassert>
#include <cmath>
#include <cstdint>
#include <expected>
#include <numbers>
#include <optional>
#include <string_view>

#include "quorum_common/velocity_reference.hpp"

#include "quorum_monitors/debounce.hpp"
#include "quorum_monitors/sensor_monitor.hpp"

namespace quorum_monitors {

/// A GPS fix in WGS84, horizontal only: the gate bounds lateral speed and nothing else
struct GeoPosition {
  double latitude_deg{};
  double longitude_deg{};
};

/// A fix and the time it was stamped, so the gate never reads a clock of its own
struct FixSample {
  GeoPosition position{};
  double time_s{};
};

/// Displacement between two fixes in the local tangent plane
struct OffsetNe {
  double north_m{};
  double east_m{};
};

/// speed_mps is averaged over dt_s, the interval the two fixes were taken across
struct GateVerdict {
  double speed_mps{};
  double dt_s{};
  bool exceeded{};
};

/// kTooSoon and kGap mean the interval was the wrong length to judge
enum class GateError { kUnsetStamp, kNotAdvanced, kNoPreviousFix, kTooSoon, kGap };

/// The interval bounds are seconds between the two fixes, not a window over many
struct GateLimits {
  double limit_mps{};
  double min_interval_s{};
  double max_interval_s{};
};

// Spherical earth, the same radius the injector's shift_lat_lon uses, so the two invert
constexpr double kEarthRadiusM = 6'371'000.0;

[[nodiscard]] constexpr bool advances_reference(const std::expected<GateVerdict, GateError>& result)
{
  return result.has_value() || result.error() == GateError::kGap;
}

[[nodiscard]] constexpr std::string_view to_string(GateError error)
{
  switch (error) {
    case GateError::kUnsetStamp:
      return "Unset Stamp";

    case GateError::kNotAdvanced:
      return "Not Advanced";

    case GateError::kNoPreviousFix:
      return "No Previous Fix";

    case GateError::kTooSoon:
      return "Too Soon";

    case GateError::kGap:
      return "Gap";
  }

  return "Unknown Gate Error";
}

[[nodiscard]] constexpr double to_radians(double degrees)
{
  return degrees * std::numbers::pi / 180.0;
}

/// Small-angle conversion of a lat/lon delta to metres. Requires |from.latitude_deg| < 89
[[nodiscard]] inline OffsetNe displacement_m(const GeoPosition& from, const GeoPosition& to)
{
  assert(std::abs(from.latitude_deg) < 89.0);

  const double delta_lat_rad = to_radians(to.latitude_deg - from.latitude_deg);
  const double delta_lon_rad = to_radians(to.longitude_deg - from.longitude_deg);

  const double north_m = delta_lat_rad * kEarthRadiusM;
  // The two fixes sit centimetres apart, so the first latitude scales both
  const double east_m = delta_lon_rad * std::cos(to_radians(from.latitude_deg)) * kEarthRadiusM;

  return OffsetNe{.north_m = north_m, .east_m = east_m};
}

/// Requires @p dt_s > 0
[[nodiscard]] inline double horizontal_speed_mps(const OffsetNe& offset, double dt_s)
{
  assert(dt_s > 0.0);
  return std::hypot(offset.north_m, offset.east_m) / dt_s;
}

/// Compares the speed the two fixes imply against a kinematic limit
[[nodiscard]] inline std::expected<GateVerdict, GateError> check_kinematic_gate(
    const FixSample& previous, const FixSample& current, const GateLimits& limits)
{
  if (previous.time_s <= 0.0 || current.time_s <= 0.0) {
    return std::unexpected(GateError::kUnsetStamp);
  }

  const double dt_s = current.time_s - previous.time_s;

  if (dt_s <= 0.0) {
    return std::unexpected(GateError::kNotAdvanced);
  }

  if (dt_s < limits.min_interval_s) {
    return std::unexpected(GateError::kTooSoon);
  }

  if (dt_s > limits.max_interval_s) {
    return std::unexpected(GateError::kGap);
  }

  const OffsetNe offset = displacement_m(previous.position, current.position);
  const double speed_mps = horizontal_speed_mps(offset, dt_s);

  return GateVerdict{
      .speed_mps = speed_mps, .dt_s = dt_s, .exceeded = speed_mps > limits.limit_mps};
}

/// gps minus the reference, keeping a bias at its own sign and bearing
[[nodiscard]] inline quorum_common::VelocityNed velocity_residual(
    const quorum_common::VelocityNed& gps, const quorum_common::VelocityNed& reference)
{
  return quorum_common::VelocityNed{.north_mps = gps.north_mps - reference.north_mps,
                                    .east_mps = gps.east_mps - reference.east_mps,
                                    .down_mps = gps.down_mps - reference.down_mps};
}

/// Watches gps with a kinematic gate on the fix and a residual against dead reckoning
class GnssMonitor final : public SensorMonitor {
public:
  GnssMonitor(GateLimits gate_limits, quorum_common::ReferenceLimits reference_limits,
              DebounceLimits debounce_limits)
      : limits_(gate_limits), reference_(reference_limits), debounce_(debounce_limits)
  {}

  [[nodiscard]] SensorId sensor_id() const noexcept override { return SensorId::kGnss; }

  void on_attitude(const quorum_common::AttitudeSample& attitude, uint8_t reset_count)
  {
    reference_.on_attitude(attitude, reset_count);
  }

  [[nodiscard]] std::expected<quorum_common::VelocityNed, quorum_common::ReferenceError> on_imu(
      const quorum_common::ImuSample& imu)
  {
    return reference_.on_imu(imu);
  }

  [[nodiscard]] std::expected<GateVerdict, GateError> check_gate(const FixSample& current)
  {
    if (!last_fix_) {
      last_fix_ = current;
      return std::unexpected(GateError::kNoPreviousFix);
    }

    const std::expected<GateVerdict, GateError> result =
        check_kinematic_gate(*last_fix_, current, limits_);

    if (advances_reference(result)) {
      last_fix_ = current;
    }

    return result;
  }

  [[nodiscard]] std::expected<quorum_common::VelocityNed, ResidualError> check_velocity(
      const quorum_common::VelocityNed& gps, double time_s)
  {
    if (judgement_ && time_s <= judgement_->time_s) {
      return std::unexpected(ResidualError::kNotAdvanced);
    }

    const std::expected<quorum_common::VelocityNed, quorum_common::ReferenceError> held =
        reference_.velocity(time_s);

    if (!held) {
      reference_.anchor(gps, time_s);

      return std::unexpected(ResidualError::kNoReference);
    }

    const quorum_common::VelocityNed residual = velocity_residual(gps, *held);
    const double residual_mps = std::hypot(residual.north_mps, residual.east_mps);
    debounce_.update(residual_mps, time_s);
    judgement_ = Judgement{.residual = residual_mps, .time_s = time_s};

    return residual;
  }

  [[nodiscard]] TrustLevel level() const noexcept override { return debounce_.level(); }

  [[nodiscard]] std::optional<Judgement> judgement() const noexcept override { return judgement_; }

  void reset() noexcept override
  {
    last_fix_.reset();
    reference_.reset();
    debounce_.reset();
    judgement_.reset();
  }

private:
  GateLimits limits_;
  std::optional<FixSample> last_fix_;
  quorum_common::ReferenceFeed reference_;
  Debounce debounce_;
  std::optional<Judgement> judgement_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__GNSS_MONITOR_HPP_
