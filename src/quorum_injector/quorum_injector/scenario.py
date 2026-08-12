import dataclasses
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any, ClassVar

import yaml

FULL_TURN_DEG = 360.0


@dataclass(frozen=True)
class NoAttack:
    pass


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
        if self.rate_mps <= 0.0:
            raise ValueError(f"rate_mps is {self.rate_mps}, it must be positive")
        if not 0.0 <= self.bearing_deg < FULL_TURN_DEG:
            raise ValueError(f"bearing_deg is {self.bearing_deg}, outside range [0, 360)")
        if self.ramp_s <= 0.0:
            raise ValueError(f"ramp_s is {self.ramp_s}, it must be positive")


@dataclass(frozen=True)
class GpsJump:
    label: ClassVar[str] = "gps_jump"

    magnitude_m: float
    bearing_deg: float

    def __post_init__(self) -> None:
        if self.magnitude_m <= 0.0:
            raise ValueError(f"magnitude_m is {self.magnitude_m}, it must be positive")
        if not 0.0 <= self.bearing_deg < FULL_TURN_DEG:
            raise ValueError(f"bearing_deg is {self.bearing_deg}, outside range [0, 360)")


@dataclass(frozen=True)
class FlowCorruption:
    label: ClassVar[str] = "flow_corruption"

    scale: float

    def __post_init__(self) -> None:
        if self.scale <= 0.0:
            raise ValueError(f"scale is {self.scale}, it must be positive")


@dataclass(frozen=True)
class CameraBlackout:
    label: ClassVar[str] = "camera_blackout"

    intensity: float
    duration_s: float

    def __post_init__(self) -> None:
        if not 0.0 <= self.intensity <= 1.0:
            raise ValueError(f"intensity is {self.intensity}, it must be in range [0, 1]")
        if self.duration_s <= 0.0:
            raise ValueError(f"duration_s is {self.duration_s}, it must be positive")


GpsAttack = NoAttack | GpsFreeze | GpsDrift | GpsJump
FlowAttack = NoAttack | FlowCorruption
CameraAttack = NoAttack | CameraBlackout
Attack = GpsAttack | FlowAttack | CameraAttack

CHANNELS = ("gps", "flow", "camera")
TOP_KEYS = frozenset({"scenario_id", "seed"})
TIMING_KEYS = frozenset({"attack", "onset_s", "onset_jitter_s"})

_GPS_ATTACKS: dict[str, type[GpsAttack]] = {c.label: c for c in (GpsFreeze, GpsDrift, GpsJump)}
_FLOW_ATTACKS: dict[str, type[FlowAttack]] = {c.label: c for c in (FlowCorruption,)}
_CAMERA_ATTACKS: dict[str, type[CameraAttack]] = {c.label: c for c in (CameraBlackout,)}


@dataclass(frozen=True)
class ChannelSpec:
    attack: Attack
    onset_s: float
    onset_jitter_s: float

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


BENIGN = ChannelSpec(NoAttack(), 0.0, 0.0)


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    seed: int
    gps: ChannelSpec = BENIGN
    flow: ChannelSpec = BENIGN
    camera: ChannelSpec = BENIGN


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


def _check_channel_keys(path: Path, channel: str, block: dict[str, Any], cls: type[Attack]) -> None:
    channel_keys = TIMING_KEYS | {f.name for f in dataclasses.fields(cls)}

    # Scenario files with a missing key are invalid
    missing = channel_keys - block.keys()
    if missing:
        raise ScenarioError(f"{path}: {channel}: missing {', '.join(sorted(missing))}")

    # An unknown key in a scenario file is unexpected behaviour
    unrecognised = block.keys() - channel_keys
    if unrecognised:
        raise ScenarioError(
            f"{path}: {channel}: {cls.label} does not take {', '.join(sorted(unrecognised))}"
        )


def _read_channel(
    path: Path, fields: dict[str, Any], channel: str, registry: Mapping[str, type[Attack]]
) -> ChannelSpec:
    if channel not in fields:
        return BENIGN

    block = fields[channel]
    if not isinstance(block, dict):
        raise ScenarioError(
            f"{path}: {channel}: expected the channel keys, found a bare {type(block).__name__}"
        )

    if "attack" not in block:
        raise ScenarioError(f"{path}: {channel}: missing attack")

    label = block["attack"]
    cls = registry.get(label) if isinstance(label, str) else None
    if cls is None:
        raise ScenarioError(f"{path}: {channel}: '{label}' is not a known {channel} attack")

    _check_channel_keys(path, channel, block, cls)

    try:
        return ChannelSpec(
            attack=_read_attack(path, channel, block, cls),
            onset_s=_read_number(path, channel, block, "onset_s"),
            onset_jitter_s=_read_number(path, channel, block, "onset_jitter_s"),
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


def _read_attack(path: Path, channel: str, block: dict[str, Any], cls: type[Attack]) -> Attack:
    # Every attack parameter is a number named as its key in the scenario file.
    return cls(
        **{f.name: _read_number(path, channel, block, f.name) for f in dataclasses.fields(cls)}
    )


def _read_number(path: Path, channel: str, block: dict[str, Any], key: str) -> float:
    try:
        return float(block[key])
    except (TypeError, ValueError) as err:
        raise ScenarioError(
            f"{path}: {channel}: {key} is '{block[key]}', expected a number"
        ) from err
