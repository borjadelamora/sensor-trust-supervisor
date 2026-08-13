import copy

from px4_msgs.msg import SensorOpticalFlow, VehicleOpticalFlowVel

from quorum_injector.plan import ChannelPlan, Relayed
from quorum_injector.scenario import CameraAttack, CameraBlackout, FlowCorruption, NoAttack

UNSCALED = 1.0
BLIND = 0.0


def apply_velocity(
    msg: VehicleOpticalFlowVel, plan: ChannelPlan, camera_plan: ChannelPlan, elapsed_s: float
) -> Relayed:
    scale = flow_scale(plan, elapsed_s)
    lens = lens_attack(camera_plan, elapsed_s)

    if scale == UNSCALED and lens is None:
        return Relayed(msg, corrupted=False)

    # One lens feeds the frames and the flow, so blinding it takes the velocity with it
    velocity_factor = BLIND if lens is not None else scale
    rate_factor = BLIND if lens is not None else UNSCALED

    scaled_msg = copy.deepcopy(msg)

    scaled_msg.vel_body *= velocity_factor
    scaled_msg.vel_ne *= velocity_factor
    scaled_msg.vel_body_filtered *= velocity_factor
    scaled_msg.vel_ne_filtered *= velocity_factor

    scaled_msg.flow_rate_uncompensated *= rate_factor
    scaled_msg.flow_rate_compensated *= rate_factor

    return Relayed(scaled_msg, corrupted=True)


def apply_raw(
    msg: SensorOpticalFlow, plan: ChannelPlan, camera_plan: ChannelPlan, elapsed_s: float
) -> Relayed:
    scale = flow_scale(plan, elapsed_s)
    lens = lens_attack(camera_plan, elapsed_s)

    if scale == UNSCALED and lens is None:
        return Relayed(msg, corrupted=False)

    scaled_msg = copy.deepcopy(msg)

    # The corrupted quantity is the range, so the rates it scales into velocity stay true
    scaled_msg.distance_m *= scale
    scaled_msg.pixel_flow *= BLIND if lens is not None else UNSCALED

    # A frozen scene still looks like a good reading, a blacked-out one does not
    if isinstance(lens, CameraBlackout):
        scaled_msg.quality = 0

    return Relayed(scaled_msg, corrupted=True)


def flow_scale(plan: ChannelPlan, elapsed_s: float) -> float:
    episode = plan.active(elapsed_s)

    if episode is None or isinstance(episode.attack, NoAttack):
        return UNSCALED

    match episode.attack:
        case FlowCorruption() as corruption:
            progress = corruption.profile.evaluate(elapsed_s - episode.onset_s)
            return UNSCALED + (corruption.scale - UNSCALED) * progress.fraction
        case _:
            raise AssertionError(f"no corruption for {episode.attack}")


def lens_attack(camera_plan: ChannelPlan, elapsed_s: float) -> CameraAttack | None:
    episode = camera_plan.active(elapsed_s)

    if episode is None or isinstance(episode.attack, NoAttack):
        return None

    return episode.attack
