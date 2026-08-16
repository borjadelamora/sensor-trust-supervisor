#ifndef QUORUM_MONITORS__PX4_TIME_HPP_
#define QUORUM_MONITORS__PX4_TIME_HPP_

#include <cstdint>

namespace quorum_monitors {

[[nodiscard]] constexpr double seconds_from_micros(uint64_t micros)
{
  return static_cast<double>(micros) / 1e6;
}

}  // namespace quorum_monitors

#endif  // QUORUM_MONITORS__PX4_TIME_HPP_
