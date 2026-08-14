import copy
from dataclasses import dataclass

from px4_msgs.msg import DistanceSensor, SensorOpticalFlow, VehicleOpticalFlowVel

from quorum_injector.camera import obscuration
from quorum_injector.plan import ChannelPlan, Relayed
from quorum_injector.scenario import CameraBlackout, CameraDazzle, CameraFreeze, FlowCorruption

UNSCALED = 1.0
BLIND = 0.0


@dataclass(frozen=True)
class Lens:
    flow_factor: float
    quality_factor: float


CLEAR = Lens(UNSCALED, UNSCALED)


def apply_velocity(
    msg: VehicleOpticalFlowVel, plan: ChannelPlan, camera_plan: ChannelPlan, elapsed_s: float
) -> Relayed:
    scale = flow_scale(plan, elapsed_s)
    lens = lens_state(camera_plan, elapsed_s)

    if scale == UNSCALED and lens == CLEAR:
        return Relayed(msg, corrupted=False)

    corrupted_msg = copy.deepcopy(msg)

    # One lens feeds the frames and the flow, and px4 still takes the gyro out of what it measures
    if lens != CLEAR:
        corrupted_msg.flow_rate_uncompensated *= lens.flow_factor
        corrupted_msg.flow_rate_compensated = (
            corrupted_msg.flow_rate_uncompensated - corrupted_msg.gyro_rate[:2]
        )

    velocity_factor = scale * lens.flow_factor

    corrupted_msg.vel_body *= velocity_factor
    corrupted_msg.vel_ne *= velocity_factor
    corrupted_msg.vel_body_filtered *= velocity_factor
    corrupted_msg.vel_ne_filtered *= velocity_factor

    return Relayed(corrupted_msg, corrupted=True)


def apply_raw(
    msg: SensorOpticalFlow, plan: ChannelPlan, camera_plan: ChannelPlan, elapsed_s: float
) -> Relayed:
    scale = flow_scale(plan, elapsed_s)
    lens = lens_state(camera_plan, elapsed_s)

    # This airframe leaves the range out of the message, so there is nothing here to scale
    ranged = msg.distance_available and scale != UNSCALED

    if not ranged and lens == CLEAR:
        return Relayed(msg, corrupted=False)

    corrupted_msg = copy.deepcopy(msg)

    if ranged:
        corrupted_msg.distance_m *= scale

    if lens != CLEAR:
        corrupted_msg.pixel_flow *= lens.flow_factor
        corrupted_msg.quality = round(msg.quality * lens.quality_factor)

    return Relayed(corrupted_msg, corrupted=True)


def apply_range(msg: DistanceSensor, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    scale = flow_scale(plan, elapsed_s)

    if scale == UNSCALED:
        return Relayed(msg, corrupted=False)

    corrupted_msg = copy.deepcopy(msg)

    corrupted_msg.current_distance *= scale

    return Relayed(corrupted_msg, corrupted=True)


def flow_scale(plan: ChannelPlan, elapsed_s: float) -> float:
    episode = plan.active(elapsed_s)

    if episode is None:
        return UNSCALED

    match episode.attack:
        case FlowCorruption() as corruption:
            progress = corruption.profile.evaluate(elapsed_s - episode.onset_s)
            return UNSCALED + (corruption.scale - UNSCALED) * progress.fraction
        case _:
            raise AssertionError(f"no corruption for {episode.attack}")


def lens_state(camera_plan: ChannelPlan, elapsed_s: float) -> Lens:
    episode = camera_plan.active(elapsed_s)

    if episode is None:
        return CLEAR

    match episode.attack:
        case CameraBlackout() | CameraDazzle() as lens:
            passed = UNSCALED - obscuration(lens, elapsed_s - episode.onset_s)
            return Lens(passed, passed)
        case CameraFreeze():
            return Lens(BLIND, UNSCALED)
        case _:
            raise AssertionError(f"no lens for {episode.attack}")
