#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "quorum_monitors/camera_monitor.hpp"

namespace quorum_monitors {
namespace {

// The band the node flies with, so the timings here are the ones that fly
constexpr double kCentreLevel = 105.0;

constexpr DebounceLimits kDebounceLimits{.threshold = 55.0, .sustain_s = 0.5};

// One frame of the 50 Hz camera
constexpr double kStepS = 1.0 / 50.0;

constexpr double kStartS = 20.0;

constexpr double kBlackout = 0.0;
constexpr double kDazzle = 255.0;

// The ends of the benign range over the four flow flights: cruise and the ground on descent
constexpr double kCruise = 77.8;
constexpr double kGround = 133.2;

// Four pixels of which one is pure red, so a luminance weighting would not give 255 over twelve
TEST(FrameMean, CountsEveryChannelAlike)
{
  constexpr std::array<uint8_t, 12> kFrame{255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  EXPECT_NEAR(frame_mean(kFrame), 255.0 / 12.0, 1e-9);
}

TEST(CameraMonitor, ReportsHowFarTheFrameSitsFromTheBandCentre)
{
  CameraMonitor monitor(kCentreLevel, kDebounceLimits);

  EXPECT_NEAR(*monitor.on_frame({.mean_level = kBlackout, .time_s = kStartS}), kCentreLevel, 1e-9);
  EXPECT_NEAR(*monitor.on_frame({.mean_level = kDazzle, .time_s = kStartS + kStepS}),
              kDazzle - kCentreLevel, 1e-9);
}

TEST(CameraMonitor, RejectsAFrameStampedNoLaterThanTheLast)
{
  CameraMonitor monitor(kCentreLevel, kDebounceLimits);

  EXPECT_TRUE(monitor.on_frame({.mean_level = kCruise, .time_s = kStartS}).has_value());
  EXPECT_EQ(monitor.on_frame({.mean_level = kCruise, .time_s = kStartS}).error(),
            ResidualError::kNotAdvanced);
}

// The frame brightens on the descent, so the band holds at both ends of the benign range
TEST(CameraMonitor, StaysTrustedFromCruiseDownToTheGround)
{
  CameraMonitor monitor(kCentreLevel, kDebounceLimits);

  for (int taken = 0; taken <= static_cast<int>(kDebounceLimits.sustain_s / kStepS); ++taken) {
    const double mean_level = (taken % 2 == 0) ? kCruise : kGround;
    static_cast<void>(monitor.on_frame(
        {.mean_level = mean_level, .time_s = kStartS + (kStepS * static_cast<double>(taken))}));
  }

  EXPECT_EQ(monitor.level(), TrustLevel::kTrusted);
}

TEST(CameraMonitor, TurnsSuspectAfterTheFrameStaysDarkForTheSustainPeriod)
{
  CameraMonitor monitor(kCentreLevel, kDebounceLimits);

  static_cast<void>(monitor.on_frame({.mean_level = kBlackout, .time_s = kStartS}));
  static_cast<void>(monitor.on_frame(
      {.mean_level = kBlackout, .time_s = kStartS + kDebounceLimits.sustain_s - kStepS}));
  EXPECT_EQ(monitor.level(), TrustLevel::kTrusted);

  static_cast<void>(
      monitor.on_frame({.mean_level = kBlackout, .time_s = kStartS + kDebounceLimits.sustain_s}));
  EXPECT_EQ(monitor.level(), TrustLevel::kSuspect);
}

}  // namespace
}  // namespace quorum_monitors
