import dataclasses
from dataclasses import dataclass
from pathlib import Path
from typing import Any, ClassVar

import yaml

FULL_TURN_DEG = 360.0


@dataclass(frozen=True)
class NoAttack:
    label: ClassVar[str] = "none"


@dataclass(frozen=True)
class GpsFreeze:
    label: ClassVar[str] = "gps_freeze"


@dataclass(frozen=True)
class GpsDrift:
    label: ClassVar[str] = "gps_drift"

    rate_mps: float
    bearing_deg: float
    ramp_s: float

    def __post_init__(self) -> None:
        if self.rate_mps <= 0:
            raise ValueError(f"rate_mps is {self.rate_mps}, it must be positive")
        if not 0.0 <= self.bearing_deg < FULL_TURN_DEG:
            raise ValueError(f"bearing_deg is {self.bearing_deg}, outside range [0, 360)")
        if self.ramp_s <= 0:
            raise ValueError(f"ramp_s is {self.ramp_s}, it must be positive")


@dataclass(frozen=True)
class GpsJump:
    label: ClassVar[str] = "gps_jump"

    magnitude_m: float
    bearing_deg: float

    def __post_init__(self) -> None:
        if self.magnitude_m <= 0:
            raise ValueError(f"magnitude_m is {self.magnitude_m}, it must be positive")
        if not 0.0 <= self.bearing_deg < FULL_TURN_DEG:
            raise ValueError(f"bearing_deg is {self.bearing_deg}, outside range [0, 360)")


Attack = NoAttack | GpsFreeze | GpsDrift | GpsJump

_ATTACKS: dict[str, type[Attack]] = {
    cls.label: cls for cls in (NoAttack, GpsFreeze, GpsDrift, GpsJump)
}


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    attack: Attack
    onset_s: float
    onset_jitter_s: float
    seed: int

    def __post_init__(self) -> None:
        if self.onset_s < 0.0:
            raise ValueError(f"onset_s is {self.onset_s}, it cannot be negative")
        if self.onset_jitter_s < 0.0:
            raise ValueError(f"onset_jitter_s is {self.onset_jitter_s}, it cannot be negative")
        # The draw is symmetric, so a wider jitter could attack before takeoff.
        if self.onset_jitter_s > self.onset_s:
            raise ValueError(
                f"onset_jitter_s is {self.onset_jitter_s}, wider than the {self.onset_s} onset"
            )


class ScenarioError(Exception):
    """A scenario file is absent, malformed or out of range."""


BASE_KEYS = frozenset({"scenario_id", "attack", "onset_s", "onset_jitter_s", "seed"})


def load_scenario(path: Path, seed_override: int | None = None) -> Scenario:
    fields = _read_fields(path)

    attack_name = fields["attack"]
    cls = _ATTACKS.get(attack_name) if isinstance(attack_name, str) else None
    if cls is None:
        raise ScenarioError(f"{path}: '{attack_name}' is not a known attack")

    _check_keys(path, fields, cls)

    seed = fields["seed"] if seed_override is None else seed_override
    scenario_id = fields["scenario_id"]

    if isinstance(seed, bool) or not isinstance(seed, int):
        raise ScenarioError(f"{path}: seed is '{seed}', expected a whole number")

    if not isinstance(scenario_id, str):
        raise ScenarioError(f"{path}: scenario_id is '{scenario_id}', expected quoted text")

    try:
        return Scenario(
            scenario_id=scenario_id,
            attack=_read_attack(path, fields, cls),
            onset_s=_read_number(path, fields, "onset_s"),
            onset_jitter_s=_read_number(path, fields, "onset_jitter_s"),
            seed=seed,
        )
    except ValueError as err:
        raise ScenarioError(f"{path}: {err}") from err


def _check_keys(path: Path, fields: dict[str, Any], cls: type[Attack]) -> None:
    scenario_keys = BASE_KEYS | {f.name for f in dataclasses.fields(cls)}

    # Scenario files with a missing key are invalid
    missing = scenario_keys - fields.keys()
    if missing:
        raise ScenarioError(f"{path}: missing {', '.join(sorted(missing))}")

    # An unknown key in a scenario file is unexpected behaviour
    unrecognised = fields.keys() - scenario_keys
    if unrecognised:
        raise ScenarioError(f"{path}: {cls.label} does not take {', '.join(sorted(unrecognised))}")


def _read_fields(path: Path) -> dict[str, Any]:
    try:
        fields = yaml.safe_load(path.read_text())
    except FileNotFoundError as err:
        raise ScenarioError(f"{path}: no such scenario file") from err
    except (OSError, UnicodeDecodeError) as err:
        raise ScenarioError(f"{path}: could not be read") from err
    except yaml.YAMLError as err:
        problem = getattr(err, "problem", "could not be parsed")
        raise ScenarioError(f"{path}: not valid YAML, {problem}") from err

    if fields is None:
        raise ScenarioError(f"{path}: the file is empty")
    if not isinstance(fields, dict):
        raise ScenarioError(
            f"{path}: expected the scenario keys, found a bare {type(fields).__name__}"
        )

    # The attack names the key set the rest of the file is checked against
    if "attack" not in fields:
        raise ScenarioError(f"{path}: missing attack")

    return fields


def _read_attack(path: Path, fields: dict[str, Any], cls: type[Attack]) -> Attack:
    # Every attack parameter is a number named as its key in the scenario file.
    return cls(**{f.name: _read_number(path, fields, f.name) for f in dataclasses.fields(cls)})


def _read_number(path: Path, fields: dict[str, Any], key: str) -> float:
    try:
        return float(fields[key])
    except (TypeError, ValueError) as err:
        raise ScenarioError(f"{path}: {key} is '{fields[key]}', expected a number") from err
