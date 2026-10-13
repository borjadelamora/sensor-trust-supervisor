#ifndef QUORUM_ARBITER__TRUST_STATE_HPP_
#define QUORUM_ARBITER__TRUST_STATE_HPP_

#include <cassert>
#include <optional>

namespace quorum_arbiter {

/// How far the arbiter trusts one sensor
enum class TrustLevel { kTrusted, kSuspect, kBanned };

/// What one cycle tells the arbiter about a sensor
enum class Evidence { kAccused, kCleared, kSilent };

/// How long each condition has to hold before the level moves
struct TrustLimits {
  double debounce_s{};
  double ban_s{};
  double recover_s{};
};

/// One sensor's trust level, moved only by evidence held for its whole limit
class TrustState final {
public:
  explicit TrustState(TrustLimits limits) : limits_{limits}
  {
    assert(limits.debounce_s > 0.0);
    assert(limits.ban_s > 0.0);
    assert(limits.recover_s > 0.0);
  }

  /// Requires @p time_s to be no earlier than the previous call
  void advance(Evidence evidence, double time_s)
  {
    assert(time_s >= last_time_s_);

    last_time_s_ = time_s;

    if (level_ == TrustLevel::kBanned) {
      return;
    }

    const bool accuses = evidence == Evidence::kAccused || evidence == Evidence::kSilent;

    if (!run_start_s_.has_value() || accuses != run_accusing_) {
      run_start_s_ = time_s;
      run_accusing_ = accuses;
      return;
    }

    const double run_s = time_s - *run_start_s_;
    if (run_accusing_) {
      if (level_ == TrustLevel::kTrusted && run_s >= limits_.debounce_s) {
        level_ = TrustLevel::kSuspect;
        run_start_s_ = time_s;
      } else if (level_ == TrustLevel::kSuspect && run_s >= limits_.ban_s) {
        level_ = TrustLevel::kBanned;
      }
    } else if (level_ == TrustLevel::kSuspect && run_s >= limits_.recover_s) {
      level_ = TrustLevel::kTrusted;
    }
  }

  [[nodiscard]] TrustLevel level() const noexcept { return level_; }

private:
  TrustLimits limits_{};
  TrustLevel level_{TrustLevel::kTrusted};
  std::optional<double> run_start_s_;
  bool run_accusing_{};
  double last_time_s_{};
};

}  // namespace quorum_arbiter

#endif  // QUORUM_ARBITER__TRUST_STATE_HPP_
