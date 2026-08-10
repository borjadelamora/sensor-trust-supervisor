import math

import pytest
from numpy.random import default_rng
from px4_msgs.msg import SensorGps
from quorum_injector.attacks import (
    AttackPlan,
    SpoofState,
    apply_to_gps,
    calculate_spoof_drift,
    calculate_spoof_state,
    get_elapsed_s,
    is_active,
    plan_attack,
)
from quorum_injector.scenario import GpsDrift, GpsFreeze, GpsJump, Scenario

ONSET_S = 20.0
DRIFT = GpsDrift(rate_mps=1.0, bearing_deg=60.0, ramp_s=5.0)
JUMP = GpsJump(magnitude_m=50.0, bearing_deg=60.0)


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


def test_elapsed_s():
    assert get_elapsed_s(21_500_000, 1_500_000) == 20.0


def test_onset_is_inclusive():
    assert not is_active(ONSET_S - 0.1, ONSET_S)
    assert is_active(ONSET_S, ONSET_S)


def test_the_same_seed_plans_the_same_onset():
    scenario = Scenario(
        scenario_id="ATK-001",
        attack=GpsFreeze(),
        onset_s=ONSET_S,
        onset_jitter_s=2.0,
        seed=7,
    )

    planned = plan_attack(scenario, default_rng(scenario.seed))

    assert planned == plan_attack(scenario, default_rng(scenario.seed))
    assert planned != plan_attack(scenario, default_rng(scenario.seed + 1))


def test_passthrough_before_onset():
    plan = AttackPlan(attack=GpsFreeze(), onset_s=ONSET_S)
    msg = make_fix(latitude_deg=41.5, north_speed_mps=4.8)

    relayed = apply_to_gps(msg, make_fix(north_speed_mps=4.8), plan, 1.0)

    assert not relayed.corrupted
    assert relayed.fix is msg


def test_freeze_holds_the_fix():
    plan = AttackPlan(attack=GpsFreeze(), onset_s=ONSET_S)
    held = make_fix(north_speed_mps=4.8)

    relayed = apply_to_gps(make_fix(latitude_deg=41.5, north_speed_mps=4.8), held, plan, ONSET_S)

    assert relayed.corrupted
    assert relayed.fix.latitude_deg == held.latitude_deg
    assert relayed.fix.vel_n_m_s == 0.0
    assert relayed.fix.vel_m_s == 0.0
    assert relayed.fix.cog_rad == 0.0


# A spoofed fix is coherent only if its velocity is the derivative of its position, so the bias
# has to track the offset through the ramp and past it.
@pytest.mark.parametrize("since_onset_s", [2.5, 5.0, 12.0])
def test_drift_bias_is_the_derivative_of_the_offset(since_onset_s):
    step_s = 1e-4
    before = offset_m(calculate_spoof_drift(DRIFT, since_onset_s - step_s))
    after = offset_m(calculate_spoof_drift(DRIFT, since_onset_s + step_s))

    derivative = (after - before) / (2 * step_s)

    assert derivative == pytest.approx(
        bias_mps(calculate_spoof_drift(DRIFT, since_onset_s)), abs=1e-4
    )


# A synchronous spoofer takes over without a discontinuity, so both start at zero.
def test_drift_starts_from_rest_at_onset():
    state = calculate_spoof_drift(DRIFT, 0.0)

    assert offset_m(state) == 0.0
    assert bias_mps(state) == 0.0


@pytest.mark.parametrize(
    ("bearing_deg", "north_m", "east_m"),
    [(0.0, 100.0, 0.0), (90.0, 0.0, 100.0), (60.0, 50.0, 86.603)],
)
def test_bearing_resolves_into_north_and_east(bearing_deg, north_m, east_m):
    state = calculate_spoof_state(100.0, 0.0, bearing_deg)

    assert state.offset_north_m == pytest.approx(north_m, abs=1e-3)
    assert state.offset_east_m == pytest.approx(east_m, abs=1e-3)


def test_drift_biases_the_velocity_coherently():
    plan = AttackPlan(attack=DRIFT, onset_s=ONSET_S)

    # Well past the ramp, so the bias is the full rate resolved along the bearing.
    fix = apply_to_gps(make_fix(north_speed_mps=3.0), make_fix(), plan, ONSET_S + 12.0).fix

    assert fix.vel_n_m_s == pytest.approx(3.0 + math.cos(math.radians(60.0)), abs=1e-4)
    assert fix.vel_e_m_s == pytest.approx(math.sin(math.radians(60.0)), abs=1e-4)
    assert fix.vel_m_s == pytest.approx(math.hypot(fix.vel_n_m_s, fix.vel_e_m_s))
    assert fix.cog_rad == pytest.approx(math.atan2(fix.vel_e_m_s, fix.vel_n_m_s))


def test_a_jump_steps_the_position_and_leaves_the_velocity():
    plan = AttackPlan(attack=JUMP, onset_s=ONSET_S)
    msg = make_fix(north_speed_mps=3.0)

    early = apply_to_gps(msg, make_fix(), plan, ONSET_S + 1.0).fix
    late = apply_to_gps(msg, make_fix(), plan, ONSET_S + 30.0).fix

    assert early.latitude_deg != msg.latitude_deg
    assert late.latitude_deg == early.latitude_deg
    assert late.vel_n_m_s == msg.vel_n_m_s
    assert late.vel_e_m_s == msg.vel_e_m_s
