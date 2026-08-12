import pytest
from px4_msgs.msg import VehicleOpticalFlowVel
from quorum_injector.flow import apply
from quorum_injector.plan import ChannelPlan
from quorum_injector.scenario import FlowCorruption

ONSET_S = 20.0
SCALE = 0.5
CORRUPTION = ChannelPlan(attack=FlowCorruption(scale=SCALE), onset_s=ONSET_S)


def make_flow() -> VehicleOpticalFlowVel:
    msg = VehicleOpticalFlowVel()
    msg.timestamp = 21_500_000
    msg.timestamp_sample = 21_499_000
    msg.vel_body = [2.0, 4.0]
    msg.vel_ne = [4.0, 2.0]
    msg.vel_body_filtered = [1.8, 3.9]
    msg.vel_ne_filtered = [3.9, 1.8]
    msg.flow_rate_uncompensated = [0.4, 0.2]
    msg.flow_rate_compensated = [0.3, 0.1]
    msg.gyro_rate = [0.10, 0.20, 0.30]
    return msg


def test_passthrough_before_onset():
    msg = make_flow()

    relayed = apply(msg, CORRUPTION, ONSET_S - 0.1)

    assert not relayed.corrupted
    assert relayed.msg is msg


@pytest.mark.parametrize("field", ["vel_body", "vel_ne", "vel_body_filtered", "vel_ne_filtered"])
def test_every_velocity_field_scales(field):
    msg = make_flow()

    corrupted = apply(msg, CORRUPTION, ONSET_S).msg

    assert list(getattr(corrupted, field)) == pytest.approx(
        [value * SCALE for value in getattr(msg, field)]
    )


# The two rates differ by the gyro term, so scaling them would show up against gyro_rate.
@pytest.mark.parametrize("field", ["flow_rate_uncompensated", "flow_rate_compensated", "gyro_rate"])
def test_the_rates_the_range_scales_survive(field):
    msg = make_flow()

    corrupted = apply(msg, CORRUPTION, ONSET_S).msg

    assert list(getattr(corrupted, field)) == pytest.approx(list(getattr(msg, field)))


def test_the_timestamps_survive():
    msg = make_flow()

    corrupted = apply(msg, CORRUPTION, ONSET_S).msg

    assert corrupted.timestamp == msg.timestamp
    assert corrupted.timestamp_sample == msg.timestamp_sample
