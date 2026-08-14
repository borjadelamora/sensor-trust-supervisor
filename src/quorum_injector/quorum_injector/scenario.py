import dataclasses
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any, ClassVar

import yaml

from quorum_injector.profile import Linear, Oscillating, Profile, Sigmoid, Step, overshoot

FULL_TURN_DEG = 360.0


@dataclass(frozen=True)
class NoAttack:
    pass


@dataclass(frozen=True)
class GpsFreeze:
    label: ClassVar[str] = "gps_freeze"


@dataclass(frozen=True)
class GpsSpoof:
    label: ClassVar[str] = "gps_spoof"

    offset_m: float
    bearing_deg: float
    profile: Profile

    def __post_init__(self) -> None:
        if self.offset_m <= 0.0:
            raise ValueError(f"offset_m is {self.offset_m}, it must be positive")
        if not 0.0 <= self.bearing_deg < FULL_TURN_DEG:
            raise ValueError(f"bearing_deg is {self.bearing_deg}, outside range [0, 360)")


@dataclass(frozen=True)
class FlowCorruption:
    label: ClassVar[str] = "flow_corruption"

    scale: float
    profile: Profile

    def __post_init__(self) -> None:
        if self.scale <= 0.0:
            raise ValueError(f"scale is {self.scale}, it must be positive")

        # A reported range cannot go negative under an overshooting profile
        past_target = overshoot(self.profile)
        if self.scale * (1.0 + past_target) <= past_target:
            raise ValueError(
                f"scale is {self.scale}, low enough that this profile drives the range negative"
            )


@dataclass(frozen=True)
class CameraBlackout:
    label: ClassVar[str] = "camera_blackout"

    intensity: float
    profile: Profile

    def __post_init__(self) -> None:
        _check_intensity(self.intensity)


@dataclass(frozen=True)
class CameraDazzle:
    label: ClassVar[str] = "camera_dazzle"

    intensity: float
    profile: Profile

    def __post_init__(self) -> None:
        _check_intensity(self.intensity)


@dataclass(frozen=True)
class CameraFreeze:
    label: ClassVar[str] = "camera_freeze"


def _check_intensity(intensity: float) -> None:
    if not 0.0 < intensity <= 1.0:
        raise ValueError(f"intensity is {intensity}, it must be in range (0, 1]")


GpsAttack = NoAttack | GpsFreeze | GpsSpoof
FlowAttack = NoAttack | FlowCorruption
CameraAttack = NoAttack | CameraBlackout | CameraDazzle | CameraFreeze
Attack = GpsAttack | FlowAttack | CameraAttack

CHANNELS = ("gps", "flow", "camera")
TOP_KEYS = frozenset({"scenario_id", "seed"})
TIMING_KEYS = frozenset({"attack", "onset_s", "onset_jitter_s", "duration_s"})
PROFILE_KEY = "profile"
SHAPE_KEY = "shape"

_GPS_ATTACKS: dict[str, type[GpsAttack]] = {c.label: c for c in (GpsFreeze, GpsSpoof)}
_FLOW_ATTACKS: dict[str, type[FlowAttack]] = {c.label: c for c in (FlowCorruption,)}
_CAMERA_ATTACKS: dict[str, type[CameraAttack]] = {
    c.label: c for c in (CameraBlackout, CameraDazzle, CameraFreeze)
}
_PROFILES: dict[str, type[Profile]] = {c.label: c for c in (Step, Linear, Sigmoid, Oscillating)}


@dataclass(frozen=True)
class Episode:
    attack: Attack
    onset_s: float
    onset_jitter_s: float
    duration_s: float | None

    def __post_init__(self) -> None:
        if self.onset_s < 0.0:
            raise ValueError(f"onset_s is {self.onset_s}, it cannot be negative")
        if self.onset_jitter_s < 0.0:
            raise ValueError(f"onset_jitter_s is {self.onset_jitter_s}, it cannot be negative")
        # The draw is symmetric and reaches its own width, so it could attack before takeoff
        if self.onset_jitter_s > 0.0 and self.onset_jitter_s >= self.onset_s:
            raise ValueError(
                f"onset_jitter_s is {self.onset_jitter_s}, wider than the {self.onset_s} onset"
            )
        if self.duration_s is not None and self.duration_s <= 0.0:
            raise ValueError(f"duration_s is {self.duration_s}, it must be positive")


BENIGN = (Episode(NoAttack(), 0.0, 0.0, None),)


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    seed: int
    gps: tuple[Episode, ...] = BENIGN
    flow: tuple[Episode, ...] = BENIGN
    camera: tuple[Episode, ...] = BENIGN


class ScenarioError(Exception):
    """A scenario file is absent, malformed or out of range."""


def load_scenario(path: Path, seed_override: int | None = None) -> Scenario:
    fields = _read_fields(path)

    _check_top_keys(path, fields)

    seed = fields["seed"] if seed_override is None else seed_override
    scenario_id = fields["scenario_id"]

    if isinstance(seed, bool) or not isinstance(seed, int):
        raise ScenarioError(f"{path}: seed is '{seed}', expected a whole number")

    if not isinstance(scenario_id, str):
        raise ScenarioError(f"{path}: scenario_id is '{scenario_id}', expected quoted text")

    return Scenario(
        scenario_id=scenario_id,
        seed=seed,
        gps=_read_channel(path, fields, "gps", _GPS_ATTACKS),
        flow=_read_channel(path, fields, "flow", _FLOW_ATTACKS),
        camera=_read_channel(path, fields, "camera", _CAMERA_ATTACKS),
    )


def _check_top_keys(path: Path, fields: dict[str, Any]) -> None:
    valid_keys = TOP_KEYS | set(CHANNELS)

    # Scenario files with a missing key are invalid
    missing = TOP_KEYS - fields.keys()
    if missing:
        raise ScenarioError(f"{path}: missing {', '.join(sorted(missing))}")

    # An unknown key in a scenario file is unexpected behaviour
    unrecognised = fields.keys() - valid_keys
    if unrecognised:
        raise ScenarioError(f"{path}: not a scenario key: {', '.join(sorted(unrecognised))}")


def _check_keys(
    path: Path, channel: str, block: dict[str, Any], expected: set[str], label: str
) -> None:
    # Scenario files with a missing key are invalid
    missing = expected - block.keys()
    if missing:
        raise ScenarioError(f"{path}: {channel}: missing {', '.join(sorted(missing))}")

    # An unknown key in a scenario file is unexpected behaviour
    unrecognised = block.keys() - expected
    if unrecognised:
        raise ScenarioError(
            f"{path}: {channel}: {label} does not take {', '.join(sorted(unrecognised))}"
        )


def _read_channel(
    path: Path, fields: dict[str, Any], channel: str, registry: Mapping[str, type[Attack]]
) -> tuple[Episode, ...]:
    if channel not in fields:
        return BENIGN

    block = fields[channel]
    # A channel is one episode written out, or a list of them spaced across the flight
    blocks = block if isinstance(block, list) else [block]

    if not blocks:
        raise ScenarioError(f"{path}: {channel}: the channel has no episodes")

    for episode in blocks:
        if not isinstance(episode, dict):
            raise ScenarioError(
                f"{path}: {channel}: expected the channel keys, found a bare "
                f"{type(episode).__name__}"
            )

    return tuple(_read_episode(path, channel, episode, registry) for episode in blocks)


def _read_episode(
    path: Path, channel: str, block: dict[str, Any], registry: Mapping[str, type[Attack]]
) -> Episode:
    if "attack" not in block:
        raise ScenarioError(f"{path}: {channel}: missing attack")

    label = block["attack"]
    cls = registry.get(label) if isinstance(label, str) else None
    if cls is None:
        raise ScenarioError(f"{path}: {channel}: '{label}' is not a known {channel} attack")

    _check_keys(path, channel, block, TIMING_KEYS | _field_names(cls), cls.label)

    try:
        return Episode(
            attack=_read_attack(path, channel, block, cls),
            onset_s=_read_number(path, channel, block, "onset_s"),
            onset_jitter_s=_read_number(path, channel, block, "onset_jitter_s"),
            duration_s=_read_duration(path, channel, block),
        )
    except ValueError as err:
        raise ScenarioError(f"{path}: {channel}: {err}") from err


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

    return fields


def _field_names(cls: type) -> set[str]:
    return {f.name for f in dataclasses.fields(cls)}


def _read_attack(path: Path, channel: str, block: dict[str, Any], cls: type[Attack]) -> Attack:
    # Every attack parameter is a number named as its key, apart from the profile
    return cls(**{name: _read_field(path, channel, block, name) for name in _field_names(cls)})


def _read_field(path: Path, channel: str, block: dict[str, Any], key: str) -> float | Profile:
    if key == PROFILE_KEY:
        return _read_profile(path, channel, block[PROFILE_KEY])

    return _read_number(path, channel, block, key)


def _read_profile(path: Path, channel: str, block: Any) -> Profile:
    if not isinstance(block, dict) or SHAPE_KEY not in block:
        raise ScenarioError(f"{path}: {channel}: the profile is missing {SHAPE_KEY}")

    label = block[SHAPE_KEY]
    cls = _PROFILES.get(label) if isinstance(label, str) else None
    if cls is None:
        raise ScenarioError(f"{path}: {channel}: '{label}' is not a known profile shape")

    _check_keys(path, channel, block, {SHAPE_KEY} | _field_names(cls), cls.label)

    return cls(**{name: _read_number(path, channel, block, name) for name in _field_names(cls)})


def _read_duration(path: Path, channel: str, block: dict[str, Any]) -> float | None:
    # An episode with no duration runs to the end of the flight
    if block["duration_s"] is None:
        return None

    return _read_number(path, channel, block, "duration_s")


def _read_number(path: Path, channel: str, block: dict[str, Any], key: str) -> float:
    try:
        return float(block[key])
    except (TypeError, ValueError) as err:
        raise ScenarioError(
            f"{path}: {channel}: {key} is '{block[key]}', expected a number"
        ) from err
