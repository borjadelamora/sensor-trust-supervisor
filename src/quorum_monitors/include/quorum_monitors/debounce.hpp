#ifndef QUORUM_MONITORS__DEBOUNCE_HPP_
#define QUORUM_MONITORS__DEBOUNCE_HPP_

#include <cassert>
#include <optional>
#include <string_view>

namespace quorum_monitors {

/// How reliable a monitor currently judges its sensor to be
enum class TrustLevel { kTrusted, kSuspect };

/// The residual a sensor is suspect above, and how long it has to stay there to count
struct DebounceLimits {
  double threshold_mps{};
  double sustain_s{};
};

[[nodiscard]] constexpr std::string_view to_string(TrustLevel level)
{
  switch (level) {
    case TrustLevel::kTrusted:
      return "Trusted";

    case TrustLevel::kSuspect:
      return "Suspect";
  }

  return "Unknown Trust Level";
}

/// Holds a level until the residual has disagreed with it for a whole sustain period
class Debounce final {
public:
  explicit Debounce(DebounceLimits limits) : limits_{limits}
  {
    assert(limits.threshold_mps > 0.0);
    assert(limits.sustain_s > 0.0);
  }

  void update(double residual_mps, double time_s)
  {
    assert(time_s > 0.0);

    const TrustLevel implied = implied_level(residual_mps);

    if (implied == level_) {
      crossed_at_s_.reset();
      return;
    }

    if (!crossed_at_s_) {
      crossed_at_s_ = time_s;
      return;
    }

    if (time_s - *crossed_at_s_ >= limits_.sustain_s) {
      level_ = implied;
      crossed_at_s_.reset();
    }
  }

  void reset()
  {
    level_ = TrustLevel::kTrusted;
    crossed_at_s_.reset();
  }

  [[nodiscard]] TrustLevel level() const noexcept { return level_; }

private:
  [[nodiscard]] TrustLevel implied_level(double residual_mps) const
  {
    return residual_mps > limits_.threshold_mps ? TrustLevel::kSuspect : TrustLevel::kTrusted;
  }

  DebounceLimits limits_;
  TrustLevel level_{TrustLevel::kTrusted};
  std::optional<double> crossed_at_s_;
};

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__DEBOUNCE_HPP_
