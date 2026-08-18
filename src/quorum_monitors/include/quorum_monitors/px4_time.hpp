#ifndef QUORUM_MONITORS__PX4_TIME_HPP_
#define QUORUM_MONITORS__PX4_TIME_HPP_

#include <cstdint>

namespace quorum_monitors {

// px4 leaves a stamp it has not filled in at zero
constexpr uint64_t kUnsetStampUs = 0;

[[nodiscard]] constexpr double seconds_from_micros(uint64_t micros)
{
  return static_cast<double>(micros) / 1e6;
}

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__PX4_TIME_HPP_
