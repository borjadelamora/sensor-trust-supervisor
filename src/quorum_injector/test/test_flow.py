import pytest
from px4_msgs.msg import SensorOpticalFlow, VehicleOpticalFlowVel
from quorum_injector.flow import apply_raw, apply_velocity
from quorum_injector.plan import ChannelPlan, PlannedEpisode
from quorum_injector.profile import Step
from quorum_injector.scenario import CameraBlackout, CameraFreeze, FlowCorruption

ONSET_S = 20.0
SCALE = 0.5
DISTANCE_M = 4.0
QUALITY = 200

BENIGN = ChannelPlan(())
CORRUPTION = ChannelPlan(
    (PlannedEpisode(FlowCorruption(scale=SCALE, profile=Step()), ONSET_S, None),)
)
BLACKOUT = ChannelPlan(
    (PlannedEpisode(CameraBlackout(intensity=0.0, profile=Step()), ONSET_S, None),)
)
FROZEN = ChannelPlan((PlannedEpisode(CameraFreeze(), ONSET_S, None),))


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


def make_raw_flow() -> SensorOpticalFlow:
    msg = SensorOpticalFlow()
    msg.pixel_flow = [0.02, 0.01]
    msg.delta_angle = [0.001, 0.002, 0.003]
    msg.distance_m = DISTANCE_M
    msg.quality = QUALITY
    return msg


def test_passthrough_before_onset():
    msg = make_flow()

    relayed = apply_velocity(msg, CORRUPTION, BENIGN, ONSET_S - 0.1)

    assert not relayed.corrupted
    assert relayed.msg is msg


@pytest.mark.parametrize("field", ["vel_body", "vel_ne", "vel_body_filtered", "vel_ne_filtered"])
def test_every_velocity_field_scales(field):
    msg = make_flow()

    corrupted = apply_velocity(msg, CORRUPTION, BENIGN, ONSET_S).msg

    assert list(getattr(corrupted, field)) == pytest.approx(
        [value * SCALE for value in getattr(msg, field)]
    )


# The two rates differ by the gyro term, so scaling them would show up against gyro_rate.
@pytest.mark.parametrize("field", ["flow_rate_uncompensated", "flow_rate_compensated", "gyro_rate"])
def test_the_rates_the_range_scales_survive(field):
    msg = make_flow()

    corrupted = apply_velocity(msg, CORRUPTION, BENIGN, ONSET_S).msg

    assert list(getattr(corrupted, field)) == pytest.approx(list(getattr(msg, field)))


def test_the_timestamps_survive():
    msg = make_flow()

    corrupted = apply_velocity(msg, CORRUPTION, BENIGN, ONSET_S).msg

    assert corrupted.timestamp == msg.timestamp
    assert corrupted.timestamp_sample == msg.timestamp_sample


# Raw flow times range is the derived velocity, so both have to move together
def test_the_range_carries_the_corruption_on_the_raw_topic():
    msg = make_raw_flow()

    corrupted = apply_raw(msg, CORRUPTION, BENIGN, ONSET_S).msg

    assert corrupted.distance_m == pytest.approx(DISTANCE_M * SCALE)
    assert list(corrupted.pixel_flow) == pytest.approx(list(msg.pixel_flow))
    assert corrupted.quality == QUALITY


# One lens feeds the frames and the flow, but only a blackout admits to bad quality
@pytest.mark.parametrize(("camera_plan", "quality"), [(BLACKOUT, 0), (FROZEN, QUALITY)])
def test_blinding_the_lens_stops_the_flow(camera_plan, quality):
    relayed = apply_velocity(make_flow(), BENIGN, camera_plan, ONSET_S)
    raw = apply_raw(make_raw_flow(), BENIGN, camera_plan, ONSET_S).msg

    assert relayed.corrupted
    assert list(relayed.msg.vel_ne) == pytest.approx([0.0, 0.0])
    assert list(raw.pixel_flow) == pytest.approx([0.0, 0.0])
    assert raw.quality == quality
