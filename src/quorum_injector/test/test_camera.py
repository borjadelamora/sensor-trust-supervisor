import array

import pytest
from quorum_injector.camera import apply
from quorum_injector.plan import ChannelPlan
from quorum_injector.scenario import CameraBlackout
from sensor_msgs.msg import Image

ONSET_S = 20.0
DURATION_S = 10.0
FRAME_BYTES = 30_000
BLACKOUT = ChannelPlan(attack=CameraBlackout(intensity=0.0, duration_s=DURATION_S), onset_s=ONSET_S)


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

    relayed = apply(msg, BLACKOUT, elapsed_s)

    assert not relayed.corrupted
    assert relayed.msg is msg


@pytest.mark.parametrize(("intensity", "level"), [(0.0, 0), (1.0, 255)])
def test_the_frame_is_flooded_to_the_intensity(intensity, level):
    camera_plan = ChannelPlan(
        attack=CameraBlackout(intensity=intensity, duration_s=DURATION_S), onset_s=ONSET_S
    )

    relayed = apply(make_frame(), camera_plan, ONSET_S)

    assert relayed.corrupted
    assert set(relayed.msg.data) == {level}


# A monitor still has to be able to read the frame, and the data setter only takes an array.
def test_the_flooded_frame_stays_a_valid_image():
    msg = make_frame()

    flooded = apply(msg, BLACKOUT, ONSET_S + DURATION_S - 0.1).msg

    assert isinstance(flooded.data, array.array)
    assert len(flooded.data) == FRAME_BYTES
    assert (flooded.encoding, flooded.step, flooded.height) == (msg.encoding, msg.step, msg.height)
