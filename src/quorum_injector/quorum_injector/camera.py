import array
import copy

from sensor_msgs.msg import Image

from quorum_injector.plan import ChannelPlan, Relayed
from quorum_injector.scenario import CameraBlackout, CameraDazzle, CameraFreeze

LEVELS = 256
BLACK = 0
SATURATED = 255


def apply(msg: Image, held: Image | None, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    episode = plan.active(elapsed_s)

    if episode is None:
        return Relayed(msg, corrupted=False)

    match episode.attack:
        case CameraBlackout() as blackout:
            covered = obscuration(blackout, elapsed_s - episode.onset_s)

            return Relayed(flood(msg, BLACK, covered), corrupted=True)
        case CameraDazzle() as dazzle:
            covered = obscuration(dazzle, elapsed_s - episode.onset_s)

            return Relayed(flood(msg, SATURATED, covered), corrupted=True)
        case CameraFreeze():
            if held is None:
                return Relayed(msg, corrupted=False)

            return Relayed(hold(msg, held), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {episode.attack}")


def obscuration(lens: CameraBlackout | CameraDazzle, since_onset_s: float) -> float:
    # A profile can overshoot 1, and there is nothing past a covered lens
    return min(1.0, lens.intensity * lens.profile.evaluate(since_onset_s).fraction)


def flood(msg: Image, level: int, covered: float) -> Image:
    flooded_msg = copy.deepcopy(msg)

    # A byte at a time is too slow for 45 Hz, so every level is mapped once and applied in C
    mapping = bytes(round(value + (level - value) * covered) for value in range(LEVELS))
    flooded_msg.data = array.array("B", bytes(msg.data).translate(mapping))

    return flooded_msg


def hold(msg: Image, held: Image) -> Image:
    held_msg = copy.deepcopy(msg)

    held_msg.data = array.array("B", bytes(held.data))

    return held_msg
