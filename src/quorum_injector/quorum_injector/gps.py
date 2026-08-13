import copy
import math
from dataclasses import dataclass
from typing import NamedTuple

from px4_msgs.msg import SensorGps

from quorum_injector.plan import ChannelPlan, Relayed
from quorum_injector.scenario import GpsFreeze, GpsSpoof, NoAttack


@dataclass(frozen=True)
class SpoofState:
    offset_north_m: float
    offset_east_m: float
    bias_north_mps: float
    bias_east_mps: float


class LatLon(NamedTuple):
    latitude_deg: float
    longitude_deg: float


# Earth radius, approximation assuming a sphere
EARTH_RADIUS_M = 6_371_000.0


def apply(msg: SensorGps, held: SensorGps, plan: ChannelPlan, elapsed_s: float) -> Relayed:
    episode = plan.active(elapsed_s)

    if episode is None or isinstance(episode.attack, NoAttack):
        return Relayed(msg, corrupted=False)

    match episode.attack:
        case GpsFreeze():
            return Relayed(freeze_gps(msg, held), corrupted=True)
        case GpsSpoof() as spoof:
            spoof_state = calculate_spoof_state(spoof, elapsed_s - episode.onset_s)
            return Relayed(shift_gps(msg, spoof_state), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {episode.attack}")


def freeze_gps(msg: SensorGps, held: SensorGps) -> SensorGps:
    frozen_msg = copy.deepcopy(msg)

    frozen_msg.latitude_deg = held.latitude_deg
    frozen_msg.longitude_deg = held.longitude_deg
    frozen_msg.altitude_msl_m = held.altitude_msl_m
    frozen_msg.altitude_ellipsoid_m = held.altitude_ellipsoid_m

    frozen_msg.vel_m_s = 0.0
    frozen_msg.vel_n_m_s = 0.0
    frozen_msg.vel_e_m_s = 0.0
    frozen_msg.vel_d_m_s = 0.0
    frozen_msg.cog_rad = 0.0

    return frozen_msg


def shift_lat_lon(
    latitude_deg: float, longitude_deg: float, north_m: float, east_m: float
) -> LatLon:
    delta_lat_rad = north_m / EARTH_RADIUS_M
    delta_lon_rad = east_m / (EARTH_RADIUS_M * math.cos(math.radians(latitude_deg)))

    new_latitude_deg = latitude_deg + math.degrees(delta_lat_rad)
    new_longitude_deg = longitude_deg + math.degrees(delta_lon_rad)

    return LatLon(new_latitude_deg, new_longitude_deg)


def shift_gps(msg: SensorGps, state: SpoofState) -> SensorGps:
    shifted_msg = copy.deepcopy(msg)

    shifted_latlon = shift_lat_lon(
        shifted_msg.latitude_deg,
        shifted_msg.longitude_deg,
        state.offset_north_m,
        state.offset_east_m,
    )
    shifted_msg.latitude_deg = shifted_latlon.latitude_deg
    shifted_msg.longitude_deg = shifted_latlon.longitude_deg

    shifted_msg.vel_n_m_s += state.bias_north_mps
    shifted_msg.vel_e_m_s += state.bias_east_mps

    # Ground speed and course are the same horizontal vector as the components, in polar form.
    shifted_msg.vel_m_s = math.hypot(shifted_msg.vel_n_m_s, shifted_msg.vel_e_m_s)
    shifted_msg.cog_rad = math.atan2(shifted_msg.vel_e_m_s, shifted_msg.vel_n_m_s)

    return shifted_msg


def calculate_spoof_state(spoof: GpsSpoof, since_onset_s: float) -> SpoofState:
    # The bias is the profile's own derivative, so the fix moves at the rate it claims to
    progress = spoof.profile.evaluate(since_onset_s)
    offset_m = spoof.offset_m * progress.fraction
    velocity_bias_mps = spoof.offset_m * progress.rate_per_s

    bearing_rad = math.radians(spoof.bearing_deg)
    north_unit = math.cos(bearing_rad)
    east_unit = math.sin(bearing_rad)

    return SpoofState(
        offset_north_m=offset_m * north_unit,
        offset_east_m=offset_m * east_unit,
        bias_north_mps=velocity_bias_mps * north_unit,
        bias_east_mps=velocity_bias_mps * east_unit,
    )
