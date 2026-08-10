import copy
import math
from dataclasses import dataclass
from typing import NamedTuple

from numpy.random import Generator
from px4_msgs.msg import SensorGps

from quorum_injector.scenario import Attack, GpsDrift, GpsFreeze, GpsJump, NoAttack, Scenario


@dataclass(frozen=True)
class AttackPlan:
    attack: Attack
    onset_s: float


@dataclass(frozen=True)
class RelayedFix:
    fix: SensorGps
    corrupted: bool


@dataclass(frozen=True)
class SpoofState:
    offset_north_m: float
    offset_east_m: float
    bias_north_mps: float
    bias_east_mps: float


class LatLon(NamedTuple):
    latitude_deg: float
    longitude_deg: float


NO_ATTACK = AttackPlan(attack=NoAttack(), onset_s=0.0)

# Earth radius, approximation assuming a sphere
EARTH_RADIUS_M = 6_371_000.0


def plan_attack(scenario: Scenario, rng: Generator) -> AttackPlan:
    return AttackPlan(
        attack=scenario.attack,
        onset_s=scenario.onset_s + rng.uniform(-scenario.onset_jitter_s, scenario.onset_jitter_s),
    )


def get_elapsed_s(timestamp_us: int, start_us: int) -> float:
    return (timestamp_us - start_us) / 1e6


def is_active(elapsed_s: float, onset_s: float) -> bool:
    return elapsed_s >= onset_s


def calculate_spoof_state(
    offset_m: float, velocity_bias_mps: float, bearing_deg: float
) -> SpoofState:
    bearing_rad = math.radians(bearing_deg)
    north_unit = math.cos(bearing_rad)
    east_unit = math.sin(bearing_rad)

    return SpoofState(
        offset_north_m=offset_m * north_unit,
        offset_east_m=offset_m * east_unit,
        bias_north_mps=velocity_bias_mps * north_unit,
        bias_east_mps=velocity_bias_mps * east_unit,
    )


def calculate_spoof_drift(drift: GpsDrift, since_onset_s: float) -> SpoofState:
    if since_onset_s < drift.ramp_s:
        offset_m = 0.5 * (drift.rate_mps / drift.ramp_s) * (since_onset_s * since_onset_s)
        velocity_bias_mps = (drift.rate_mps / drift.ramp_s) * since_onset_s
    else:
        offset_m = drift.rate_mps * (since_onset_s - drift.ramp_s / 2)
        velocity_bias_mps = drift.rate_mps

    return calculate_spoof_state(offset_m, velocity_bias_mps, drift.bearing_deg)


def apply_to_gps(msg: SensorGps, held: SensorGps, plan: AttackPlan, elapsed_s: float) -> RelayedFix:
    if isinstance(plan.attack, NoAttack):
        return RelayedFix(msg, corrupted=False)

    if not is_active(elapsed_s, plan.onset_s):
        return RelayedFix(msg, corrupted=False)

    match plan.attack:
        case GpsFreeze():
            return RelayedFix(freeze_gps(msg, held), corrupted=True)
        case GpsJump() as jump:
            spoof_state = calculate_spoof_state(jump.magnitude_m, 0.0, jump.bearing_deg)
            return RelayedFix(shift_gps(msg, spoof_state), corrupted=True)
        case GpsDrift() as drift:
            spoof_state = calculate_spoof_drift(drift, elapsed_s - plan.onset_s)
            return RelayedFix(shift_gps(msg, spoof_state), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {plan.attack}")


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
