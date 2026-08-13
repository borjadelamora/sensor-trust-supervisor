import math

import pytest
from px4_msgs.msg import SensorGps
from quorum_injector.gps import SpoofState, apply, calculate_spoof_state
from quorum_injector.plan import ChannelPlan, PlannedEpisode
from quorum_injector.profile import Linear, Step
from quorum_injector.scenario import GpsFreeze, GpsSpoof

ONSET_S = 20.0
BEARING_DEG = 60.0
OFFSET_M = 47.0
DRIFT = GpsSpoof(
    offset_m=OFFSET_M, bearing_deg=BEARING_DEG, profile=Linear(rise_s=49.5, ease_s=5.0)
)
JUMP = GpsSpoof(offset_m=50.0, bearing_deg=BEARING_DEG, profile=Step())


def make_plan(attack) -> ChannelPlan:
    return ChannelPlan((PlannedEpisode(attack, ONSET_S, None),))


def make_fix(
    latitude_deg: float = 41.0,
    longitude_deg: float = 2.0,
    north_speed_mps: float = 0.0,
    east_speed_mps: float = 0.0,
) -> SensorGps:
    msg = SensorGps()
    msg.latitude_deg = latitude_deg
    msg.longitude_deg = longitude_deg
    msg.vel_n_m_s = north_speed_mps
    msg.vel_e_m_s = east_speed_mps
    msg.vel_m_s = math.hypot(north_speed_mps, east_speed_mps)
    msg.cog_rad = math.atan2(east_speed_mps, north_speed_mps)
    return msg


def offset_m(state: SpoofState) -> float:
    return math.hypot(state.offset_north_m, state.offset_east_m)


def bias_mps(state: SpoofState) -> float:
    return math.hypot(state.bias_north_mps, state.bias_east_mps)


def test_passthrough_before_onset():
    gps_plan = make_plan(GpsFreeze())
    msg = make_fix(latitude_deg=41.5, north_speed_mps=4.8)

    relayed = apply(msg, make_fix(north_speed_mps=4.8), gps_plan, ONSET_S - 0.1)

    assert not relayed.corrupted
    assert relayed.msg is msg


def test_freeze_holds_the_fix():
    gps_plan = make_plan(GpsFreeze())
    held = make_fix(north_speed_mps=4.8)

    relayed = apply(make_fix(latitude_deg=41.5, north_speed_mps=4.8), held, gps_plan, ONSET_S)

    assert relayed.corrupted
    assert relayed.msg.latitude_deg == held.latitude_deg
    assert relayed.msg.vel_n_m_s == 0.0
    assert relayed.msg.vel_m_s == 0.0
    assert relayed.msg.cog_rad == 0.0


# The profile owns the shape, so what is left to check here is that the offset it reports is
# what the fix actually moves by.
@pytest.mark.parametrize("since_onset_s", [2.5, 5.0, 12.0])
def test_the_offset_follows_the_profile(since_onset_s):
    state = calculate_spoof_state(DRIFT, since_onset_s)

    assert offset_m(state) == pytest.approx(
        OFFSET_M * DRIFT.profile.evaluate(since_onset_s).fraction
    )
    assert bias_mps(state) == pytest.approx(
        OFFSET_M * DRIFT.profile.evaluate(since_onset_s).rate_per_s
    )


@pytest.mark.parametrize(
    ("bearing_deg", "north_m", "east_m"),
    [(0.0, 100.0, 0.0), (90.0, 0.0, 100.0), (BEARING_DEG, 50.0, 86.603)],
)
def test_bearing_resolves_into_north_and_east(bearing_deg, north_m, east_m):
    spoof = GpsSpoof(offset_m=100.0, bearing_deg=bearing_deg, profile=Step())

    state = calculate_spoof_state(spoof, 0.0)

    assert state.offset_north_m == pytest.approx(north_m, abs=1e-3)
    assert state.offset_east_m == pytest.approx(east_m, abs=1e-3)


def test_drift_biases_the_velocity_coherently():
    gps_plan = make_plan(DRIFT)

    # Well past the ease, so the bias is the full 1 m/s resolved along the bearing.
    fix = apply(make_fix(north_speed_mps=3.0), make_fix(), gps_plan, ONSET_S + 12.0).msg

    assert fix.vel_n_m_s == pytest.approx(3.0 + math.cos(math.radians(BEARING_DEG)), abs=1e-4)
    assert fix.vel_e_m_s == pytest.approx(math.sin(math.radians(BEARING_DEG)), abs=1e-4)
    assert fix.vel_m_s == pytest.approx(math.hypot(fix.vel_n_m_s, fix.vel_e_m_s))
    assert fix.cog_rad == pytest.approx(math.atan2(fix.vel_e_m_s, fix.vel_n_m_s))


# The derivative of a step is zero either side, so the velocity comes through untouched.
def test_a_jump_steps_the_position_and_leaves_the_velocity():
    gps_plan = make_plan(JUMP)
    msg = make_fix(north_speed_mps=3.0)

    early = apply(msg, make_fix(), gps_plan, ONSET_S + 1.0).msg
    late = apply(msg, make_fix(), gps_plan, ONSET_S + 30.0).msg

    assert early.latitude_deg != msg.latitude_deg
    assert late.latitude_deg == early.latitude_deg
    assert late.vel_n_m_s == msg.vel_n_m_s
    assert late.vel_e_m_s == msg.vel_e_m_s
