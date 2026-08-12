from dataclasses import dataclass
from typing import Any

from numpy.random import Generator

from quorum_injector.scenario import Attack, ChannelSpec, NoAttack, Scenario


@dataclass(frozen=True)
class ChannelPlan:
    attack: Attack
    onset_s: float


@dataclass(frozen=True)
class AttackPlan:
    gps: ChannelPlan
    flow: ChannelPlan
    camera: ChannelPlan


@dataclass(frozen=True)
class Relayed:
    msg: Any
    corrupted: bool


BENIGN_PLAN = ChannelPlan(NoAttack(), 0.0)
NO_ATTACK = AttackPlan(BENIGN_PLAN, BENIGN_PLAN, BENIGN_PLAN)


def _draw_onset(spec: ChannelSpec, rng: Generator) -> float:
    return spec.onset_s + rng.uniform(-spec.onset_jitter_s, spec.onset_jitter_s)


def plan_attack(scenario: Scenario, rng: Generator) -> AttackPlan:
    # Benign channels draw too, so one channel's onset never depends on the others.
    gps_onset = _draw_onset(scenario.gps, rng)
    flow_onset = _draw_onset(scenario.flow, rng)
    camera_onset = _draw_onset(scenario.camera, rng)

    return AttackPlan(
        gps=ChannelPlan(attack=scenario.gps.attack, onset_s=gps_onset),
        flow=ChannelPlan(attack=scenario.flow.attack, onset_s=flow_onset),
        camera=ChannelPlan(attack=scenario.camera.attack, onset_s=camera_onset),
    )


def is_active(elapsed_s: float, onset_s: float) -> bool:
    return elapsed_s >= onset_s


def is_within(elapsed_s: float, onset_s: float, duration_s: float) -> bool:
    return onset_s <= elapsed_s < onset_s + duration_s
