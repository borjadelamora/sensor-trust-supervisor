from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Any

import yaml


class AttackMode(Enum):
    NONE = "none"
    GPS_FREEZE = "gps_freeze"


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    mode: AttackMode
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


SCENARIO_KEYS = frozenset({"scenario_id", "attack", "onset_s", "onset_jitter_s", "seed"})


def load_scenario(path: Path, seed_override: int | None = None) -> Scenario:
    fields = _read_fields(path)
    seed = fields["seed"] if seed_override is None else seed_override
    attack = fields["attack"]
    scenario_id = fields["scenario_id"]

    if isinstance(seed, bool) or not isinstance(seed, int):
        raise ScenarioError(f"{path}: seed is '{seed}', expected a whole number")

    if not isinstance(scenario_id, str):
        raise ScenarioError(f"{path}: scenario_id is '{scenario_id}', expected quoted text")

    try:
        mode = AttackMode(attack)
    except ValueError as err:
        raise ScenarioError(f"{path}: '{attack}' is not a known attack") from err

    try:
        return Scenario(
            scenario_id=scenario_id,
            mode=mode,
            onset_s=_read_number(path, fields, "onset_s"),
            onset_jitter_s=_read_number(path, fields, "onset_jitter_s"),
            seed=seed,
        )
    except ValueError as err:
        raise ScenarioError(f"{path}: {err}") from err


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

    # Scenario files with a missing key are invalid
    missing = SCENARIO_KEYS - fields.keys()
    if missing:
        raise ScenarioError(f"{path}: missing {', '.join(sorted(missing))}")

    # An unknown key in a scenario file is unexpected behaviour
    unrecognised = fields.keys() - SCENARIO_KEYS
    if unrecognised:
        raise ScenarioError(f"{path}: unrecognised {', '.join(sorted(unrecognised))}")
    return fields


def _read_number(path: Path, fields: dict[str, Any], key: str) -> float:
    try:
        return float(fields[key])
    except (TypeError, ValueError) as err:
        raise ScenarioError(f"{path}: {key} is '{fields[key]}', expected a number") from err
