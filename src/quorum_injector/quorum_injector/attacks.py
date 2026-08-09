import copy
from dataclasses import dataclass

from numpy.random import Generator
from px4_msgs.msg import SensorGps

from quorum_injector.scenario import AttackMode, Scenario


@dataclass(frozen=True)
class AttackPlan:
    mode: AttackMode
    onset_s: float


@dataclass(frozen=True)
class RelayedFix:
    fix: SensorGps
    corrupted: bool


NO_ATTACK = AttackPlan(mode=AttackMode.NONE, onset_s=0.0)


def plan_attack(scenario: Scenario, rng: Generator) -> AttackPlan:
    return AttackPlan(
        mode=scenario.mode,
        onset_s=scenario.onset_s + rng.uniform(-scenario.onset_jitter_s, scenario.onset_jitter_s),
    )


def get_elapsed_s(timestamp_us: int, start_us: int) -> float:
    return (timestamp_us - start_us) / 1e6


def is_active(elapsed_s: float, onset_s: float) -> bool:
    return elapsed_s >= onset_s


def apply_to_gps(msg: SensorGps, held: SensorGps, plan: AttackPlan, elapsed_s: float) -> RelayedFix:
    if plan.mode is AttackMode.NONE:
        return RelayedFix(msg, corrupted=False)

    if not is_active(elapsed_s, plan.onset_s):
        return RelayedFix(msg, corrupted=False)

    match plan.mode:
        case AttackMode.GPS_FREEZE:
            return RelayedFix(freeze_gps(msg, held), corrupted=True)
        case _:
            raise AssertionError(f"no corruption for {plan.mode}")


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

    return frozen_msg
