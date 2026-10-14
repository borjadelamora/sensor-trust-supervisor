#ifndef QUORUM_COMMON__PX4_TIME_HPP_
#define QUORUM_COMMON__PX4_TIME_HPP_

#include <cstdint>

namespace quorum_common {

// px4 leaves a stamp it has not filled in at zero
constexpr uint64_t kUnsetStampUs = 0;

[[nodiscard]] constexpr double seconds_from_micros(uint64_t micros)
{
  return static_cast<double>(micros) / 1e6;
}

}  // namespace quorum_common

#endif  // QUORUM_COMMON__PX4_TIME_HPP_
