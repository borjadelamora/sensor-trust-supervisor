import array
import copy

from sensor_msgs.msg import Image

from quorum_injector.plan import ChannelPlan, Relayed
from quorum_injector.scenario import CameraBlackout, CameraFreeze, NoAttack

MAX_LEVEL = 255
LEVELS = 256


def apply(msg: Image, held: Image | None, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    episode = plan.active(elapsed_s)

    if episode is None or isinstance(episode.attack, NoAttack):
        return Relayed(msg, corrupted=False)

    match episode.attack:
        case CameraBlackout() as blackout:
            progress = blackout.profile.evaluate(elapsed_s - episode.onset_s)
            level = round(blackout.intensity * MAX_LEVEL)

            return Relayed(dim(msg, level, progress.fraction), corrupted=True)
        case CameraFreeze():
            if held is None:
                return Relayed(msg, corrupted=False)

            return Relayed(hold(msg, held), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {episode.attack}")


def dim(msg: Image, level: int, fraction: float) -> Image:
    dimmed_msg = copy.deepcopy(msg)

    # A byte at a time is too slow for 45 Hz, so every level is mapped once and applied in C
    mapping = bytes(
        min(MAX_LEVEL, max(0, round(value + (level - value) * fraction))) for value in range(LEVELS)
    )
    dimmed_msg.data = array.array("B", bytes(msg.data).translate(mapping))

    return dimmed_msg


def hold(msg: Image, held: Image) -> Image:
    held_msg = copy.deepcopy(msg)

    held_msg.data = array.array("B", bytes(held.data))

    return held_msg
