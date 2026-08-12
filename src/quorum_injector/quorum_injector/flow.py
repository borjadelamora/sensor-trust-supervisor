import copy

from px4_msgs.msg import VehicleOpticalFlowVel

from quorum_injector.plan import ChannelPlan, Relayed, is_active
from quorum_injector.scenario import FlowCorruption, NoAttack


def apply(msg: VehicleOpticalFlowVel, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    if isinstance(plan.attack, NoAttack):
        return Relayed(msg, corrupted=False)

    if not is_active(elapsed_s, plan.onset_s):
        return Relayed(msg, corrupted=False)

    match plan.attack:
        case FlowCorruption() as corruption:
            return Relayed(scale_velocity(msg, corruption.scale), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {plan.attack}")


def scale_velocity(msg: VehicleOpticalFlowVel, factor: float) -> VehicleOpticalFlowVel:
    scaled_msg = copy.deepcopy(msg)

    # The corrupted quantity is the range, so the rates it scales into velocity stay true.
    scaled_msg.vel_body *= factor
    scaled_msg.vel_ne *= factor
    scaled_msg.vel_body_filtered *= factor
    scaled_msg.vel_ne_filtered *= factor

    return scaled_msg
