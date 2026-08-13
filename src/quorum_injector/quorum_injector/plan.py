from dataclasses import dataclass
from typing import Any

from numpy.random import Generator

from quorum_injector.scenario import Attack, Episode, NoAttack, Scenario


@dataclass(frozen=True)
class PlannedEpisode:
    attack: Attack
    onset_s: float
    duration_s: float | None

    def contains(self, elapsed_s: float) -> bool:
        if elapsed_s < self.onset_s:
            return False

        return self.duration_s is None or elapsed_s < self.onset_s + self.duration_s


@dataclass(frozen=True)
class ChannelPlan:
    episodes: tuple[PlannedEpisode, ...]

    # Jitter can draw two declared episodes into each other, and the earlier one keeps the channel
    def active(self, elapsed_s: float) -> PlannedEpisode | None:
        return next((episode for episode in self.episodes if episode.contains(elapsed_s)), None)


@dataclass(frozen=True)
class AttackPlan:
    gps: ChannelPlan
    flow: ChannelPlan
    camera: ChannelPlan


@dataclass(frozen=True)
class Relayed:
    msg: Any
    corrupted: bool


BENIGN_PLAN = ChannelPlan((PlannedEpisode(NoAttack(), 0.0, None),))
NO_ATTACK = AttackPlan(BENIGN_PLAN, BENIGN_PLAN, BENIGN_PLAN)


def _draw_onset(episode: Episode, rng: Generator) -> float:
    return episode.onset_s + rng.uniform(-episode.onset_jitter_s, episode.onset_jitter_s)


def _plan_channel(episodes: tuple[Episode, ...], rng: Generator) -> ChannelPlan:
    return ChannelPlan(
        tuple(
            PlannedEpisode(episode.attack, _draw_onset(episode, rng), episode.duration_s)
            for episode in episodes
        )
    )


def plan_attack(scenario: Scenario, rng: Generator) -> AttackPlan:
    # Benign channels draw too, so one channel's onset never depends on the others.
    return AttackPlan(
        gps=_plan_channel(scenario.gps, rng),
        flow=_plan_channel(scenario.flow, rng),
        camera=_plan_channel(scenario.camera, rng),
    )
