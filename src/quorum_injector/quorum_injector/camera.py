import array
import copy

from sensor_msgs.msg import Image

from quorum_injector.plan import ChannelPlan, Relayed, is_within
from quorum_injector.scenario import CameraBlackout, NoAttack

MAX_LEVEL = 255


def apply(msg: Image, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    if isinstance(plan.attack, NoAttack):
        return Relayed(msg, corrupted=False)

    match plan.attack:
        case CameraBlackout() as blackout:
            if not is_within(elapsed_s, plan.onset_s, blackout.duration_s):
                return Relayed(msg, corrupted=False)

            return Relayed(flood(msg, round(blackout.intensity * MAX_LEVEL)), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {plan.attack}")


def flood(msg: Image, level: int) -> Image:
    flooded_msg = copy.deepcopy(msg)

    flooded_msg.data = array.array("B", bytes([level]) * len(flooded_msg.data))

    return flooded_msg
