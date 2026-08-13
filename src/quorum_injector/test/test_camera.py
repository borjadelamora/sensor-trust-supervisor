import array

import pytest
from quorum_injector.camera import apply
from quorum_injector.plan import ChannelPlan, PlannedEpisode
from quorum_injector.profile import Linear, Step
from quorum_injector.scenario import CameraBlackout, CameraFreeze
from sensor_msgs.msg import Image

ONSET_S = 20.0
DURATION_S = 10.0
FRAME_BYTES = 30_000
BLACKOUT = ChannelPlan(
    (PlannedEpisode(CameraBlackout(intensity=0.0, profile=Step()), ONSET_S, DURATION_S),)
)
FROZEN = ChannelPlan((PlannedEpisode(CameraFreeze(), ONSET_S, DURATION_S),))


def make_frame(level: int = 128) -> Image:
    msg = Image()
    msg.height = 100
    msg.width = 100
    msg.encoding = "rgb8"
    msg.step = 300
    msg.data = array.array("B", bytes([level]) * FRAME_BYTES)
    return msg


@pytest.mark.parametrize("elapsed_s", [ONSET_S - 0.1, ONSET_S + DURATION_S])
def test_frames_outside_the_window_pass_through(elapsed_s):
    msg = make_frame()

    relayed = apply(msg, None, BLACKOUT, elapsed_s)

    assert not relayed.corrupted
    assert relayed.msg is msg


@pytest.mark.parametrize(("intensity", "level"), [(0.0, 0), (1.0, 255)])
def test_the_frame_is_flooded_to_the_intensity(intensity, level):
    camera_plan = ChannelPlan(
        (PlannedEpisode(CameraBlackout(intensity=intensity, profile=Step()), ONSET_S, None),)
    )

    relayed = apply(make_frame(), None, camera_plan, ONSET_S)

    assert relayed.corrupted
    assert set(relayed.msg.data) == {level}


# A profile takes the frame down over time, so a mean-brightness check crosses slowly
def test_a_shaped_blackout_dims_part_of_the_way():
    camera_plan = ChannelPlan(
        (
            PlannedEpisode(
                CameraBlackout(intensity=0.0, profile=Linear(rise_s=10.0, ease_s=2.0)),
                ONSET_S,
                None,
            ),
        )
    )

    dimmed = apply(make_frame(level=200), None, camera_plan, ONSET_S + 5.0).msg

    assert set(dimmed.data) == {111}


# A frozen frame is a replayed one, so it stays a plausible image of the wrong moment
def test_a_freeze_replays_the_held_frame():
    held = make_frame(level=64)

    relayed = apply(make_frame(level=200), held, FROZEN, ONSET_S)

    assert relayed.corrupted
    assert set(relayed.msg.data) == {64}


# A monitor still has to be able to read the frame, and the data setter only takes an array.
def test_the_flooded_frame_stays_a_valid_image():
    msg = make_frame()

    flooded = apply(msg, None, BLACKOUT, ONSET_S + DURATION_S - 0.1).msg

    assert isinstance(flooded.data, array.array)
    assert len(flooded.data) == FRAME_BYTES
    assert (flooded.encoding, flooded.step, flooded.height) == (msg.encoding, msg.step, msg.height)
