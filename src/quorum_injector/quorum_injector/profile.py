import math
from dataclasses import dataclass
from typing import ClassVar, NamedTuple


class Progress(NamedTuple):
    fraction: float
    rate_per_s: float


@dataclass(frozen=True)
class Step:
    label: ClassVar[str] = "step"

    def evaluate(self, since_onset_s: float) -> Progress:
        return Progress(1.0, 0.0)


@dataclass(frozen=True)
class Linear:
    label: ClassVar[str] = "linear"

    rise_s: float
    ease_s: float

    def __post_init__(self) -> None:
        if self.rise_s <= 0.0:
            raise ValueError(f"rise_s is {self.rise_s}, it must be positive")
        if self.ease_s <= 0.0:
            raise ValueError(f"ease_s is {self.ease_s}, it must be positive")
        if 2.0 * self.ease_s > self.rise_s:
            raise ValueError(
                f"ease_s is {self.ease_s}, and the {self.rise_s} rise has to hold two of them"
            )

    def evaluate(self, since_onset_s: float) -> Progress:
        if since_onset_s >= self.rise_s:
            return Progress(1.0, 0.0)

        rate_per_s = 1.0 / (self.rise_s - self.ease_s)
        eased_rate_per_s2 = rate_per_s / self.ease_s

        if since_onset_s < self.ease_s:
            return Progress(
                0.5 * eased_rate_per_s2 * since_onset_s * since_onset_s,
                eased_rate_per_s2 * since_onset_s,
            )

        # The spoofer hands back as carefully as it took over, so the ease is mirrored at the top
        remaining_s = self.rise_s - since_onset_s
        if remaining_s < self.ease_s:
            return Progress(
                1.0 - 0.5 * eased_rate_per_s2 * remaining_s * remaining_s,
                eased_rate_per_s2 * remaining_s,
            )

        return Progress(rate_per_s * (since_onset_s - self.ease_s / 2.0), rate_per_s)


@dataclass(frozen=True)
class Sigmoid:
    label: ClassVar[str] = "sigmoid"

    rise_s: float

    def __post_init__(self) -> None:
        if self.rise_s <= 0.0:
            raise ValueError(f"rise_s is {self.rise_s}, it must be positive")

    def evaluate(self, since_onset_s: float) -> Progress:
        progress = min(since_onset_s / self.rise_s, 1.0)

        # Smootherstep, flat at both ends, so the clamp needs no branch of its own
        fraction = progress**3 * (progress * (6.0 * progress - 15.0) + 10.0)
        rate_per_s = 30.0 * progress**2 * (progress - 1.0) ** 2 / self.rise_s

        return Progress(fraction, rate_per_s)


@dataclass(frozen=True)
class Oscillating:
    label: ClassVar[str] = "oscillating"

    rise_s: float
    ease_s: float
    amplitude_fraction: float
    period_s: float

    def __post_init__(self) -> None:
        # Building the rise is what checks rise_s and ease_s
        self._rise()
        if self.period_s <= 0.0:
            raise ValueError(f"period_s is {self.period_s}, it must be positive")
        if not 0.0 <= self.amplitude_fraction < 1.0:
            raise ValueError(
                f"amplitude_fraction is {self.amplitude_fraction}, outside range [0, 1)"
            )

    def _rise(self) -> Linear:
        return Linear(self.rise_s, self.ease_s)

    def evaluate(self, since_onset_s: float) -> Progress:
        angular_rate_rad_per_s = 2.0 * math.pi / self.period_s
        phase_rad = angular_rate_rad_per_s * since_onset_s
        rise = self._rise().evaluate(since_onset_s)

        # Off a cosine rather than a sine, so the wobble starts at rest with the rise
        return Progress(
            rise.fraction + self.amplitude_fraction * (1.0 - math.cos(phase_rad)) / 2.0,
            rise.rate_per_s
            + self.amplitude_fraction * angular_rate_rad_per_s * math.sin(phase_rad) / 2.0,
        )


Profile = Step | Linear | Sigmoid | Oscillating


def overshoot(profile: Profile) -> float:
    match profile:
        case Oscillating() as wobble:
            return wobble.amplitude_fraction
        case _:
            return 0.0
