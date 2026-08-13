import dataclasses
from pathlib import Path
from typing import Any

import pytest
import yaml
from quorum_injector.profile import Linear, Step
from quorum_injector.scenario import (
    BENIGN,
    CameraBlackout,
    GpsFreeze,
    GpsSpoof,
    ScenarioError,
    load_scenario,
)

SCENARIOS = Path(__file__).parents[3] / "scenarios"

LINEAR = {"shape": "linear", "rise_s": 49.5, "ease_s": 5.0}

FREEZE = {"attack": "gps_freeze", "onset_s": 20.0, "onset_jitter_s": 2.0, "duration_s": None}

DRIFT = FREEZE | {
    "attack": "gps_spoof",
    "offset_m": 47.0,
    "bearing_deg": 60.0,
    "profile": LINEAR,
}

JUMP = DRIFT | {"offset_m": 50.0, "profile": {"shape": "step"}}

BLACKOUT = {
    "attack": "camera_blackout",
    "onset_s": 26.0,
    "onset_jitter_s": 2.0,
    "duration_s": 10.0,
    "intensity": 0.0,
    "profile": {"shape": "step"},
}

DRIFT_ATTACK = GpsSpoof(offset_m=47.0, bearing_deg=60.0, profile=Linear(rise_s=49.5, ease_s=5.0))


def scenario_fields(**channels: dict[str, Any]) -> dict[str, Any]:
    return {"scenario_id": "ATK-001", "seed": 7} | channels


def write_scenario(tmp_path: Path, fields: dict[str, Any]) -> Path:
    path = tmp_path / "scenario.yaml"
    path.write_text(yaml.safe_dump(fields))
    return path


def test_loads_a_valid_scenario(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, scenario_fields(gps=FREEZE)))

    assert scenario.scenario_id == "ATK-001"
    assert scenario.gps[0].attack == GpsFreeze()
    assert scenario.gps[0].onset_s == 20.0
    assert scenario.seed == 7


@pytest.mark.parametrize(
    ("block", "attack"),
    [
        (DRIFT, DRIFT_ATTACK),
        (JUMP, GpsSpoof(offset_m=50.0, bearing_deg=60.0, profile=Step())),
    ],
)
def test_loads_the_attack_parameters(tmp_path, block, attack):
    scenario = load_scenario(write_scenario(tmp_path, scenario_fields(gps=block)))

    assert scenario.gps[0].attack == attack


# The channels are independent, so an attack on one says nothing about the others.
def test_each_channel_carries_its_own_attack(tmp_path):
    fields = scenario_fields(gps=DRIFT, camera=BLACKOUT)

    scenario = load_scenario(write_scenario(tmp_path, fields))

    assert scenario.gps[0].attack == DRIFT_ATTACK
    assert scenario.camera[0].attack == CameraBlackout(intensity=0.0, profile=Step())
    assert scenario.camera[0].onset_s == 26.0


# A channel written as a list is several episodes spaced across one flight.
def test_a_channel_carries_several_episodes(tmp_path):
    later = FREEZE | {"onset_s": 40.0, "duration_s": 10.0}

    scenario = load_scenario(write_scenario(tmp_path, scenario_fields(gps=[DRIFT, later])))

    assert [episode.attack for episode in scenario.gps] == [DRIFT_ATTACK, GpsFreeze()]
    assert scenario.gps[1].duration_s == 10.0


def test_an_absent_channel_is_benign(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, scenario_fields(gps=FREEZE)))

    assert scenario.flow == BENIGN
    assert scenario.camera == BENIGN


def test_seed_override_beats_the_file(tmp_path):
    path = write_scenario(tmp_path, scenario_fields(gps=FREEZE))

    assert load_scenario(path, seed_override=11).seed == 11


def test_scenario_is_frozen(tmp_path):
    scenario = load_scenario(write_scenario(tmp_path, scenario_fields(gps=FREEZE)))

    with pytest.raises(dataclasses.FrozenInstanceError):
        scenario.gps[0].onset_s = 1.0


def test_missing_key_is_rejected(tmp_path):
    fields = scenario_fields(gps=FREEZE)
    del fields["seed"]

    with pytest.raises(ScenarioError, match="missing seed"):
        load_scenario(write_scenario(tmp_path, fields))


def test_missing_timing_key_is_rejected(tmp_path):
    block = dict(FREEZE)
    del block["onset_jitter_s"]

    with pytest.raises(ScenarioError, match="gps: missing onset_jitter_s"):
        load_scenario(write_scenario(tmp_path, scenario_fields(gps=block)))


def test_missing_attack_parameter_is_rejected(tmp_path):
    block = dict(DRIFT)
    del block["bearing_deg"]

    with pytest.raises(ScenarioError, match="gps: missing bearing_deg"):
        load_scenario(write_scenario(tmp_path, scenario_fields(gps=block)))


def test_missing_profile_parameter_is_rejected(tmp_path):
    fields = scenario_fields(gps=DRIFT | {"profile": {"shape": "linear", "rise_s": 49.5}})

    with pytest.raises(ScenarioError, match="gps: missing ease_s"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_profile_shape_is_rejected(tmp_path):
    fields = scenario_fields(gps=DRIFT | {"profile": {"shape": "sigmoud", "rise_s": 40.0}})

    with pytest.raises(ScenarioError, match="not a known profile shape"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_key_is_rejected(tmp_path):
    fields = scenario_fields(gps=FREEZE | {"onset_sec": 9.0})

    with pytest.raises(ScenarioError, match="gps_freeze does not take onset_sec"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_channel_is_rejected(tmp_path):
    fields = scenario_fields(gpss=FREEZE)

    with pytest.raises(ScenarioError, match="not a scenario key: gpss"):
        load_scenario(write_scenario(tmp_path, fields))


def test_another_attacks_parameter_is_rejected(tmp_path):
    fields = scenario_fields(gps=JUMP | {"intensity": 0.5})

    with pytest.raises(ScenarioError, match="gps_spoof does not take intensity"):
        load_scenario(write_scenario(tmp_path, fields))


# Each channel resolves its attack against its own set, so a valid label is still wrong here.
def test_another_channels_attack_is_rejected(tmp_path):
    fields = scenario_fields(camera=FREEZE)

    with pytest.raises(ScenarioError, match="'gps_freeze' is not a known camera attack"):
        load_scenario(write_scenario(tmp_path, fields))


def test_unknown_attack_is_rejected(tmp_path):
    fields = scenario_fields(gps=FREEZE | {"attack": "gps_freze"})

    with pytest.raises(ScenarioError, match="not a known gps attack"):
        load_scenario(write_scenario(tmp_path, fields))


def test_negative_onset_is_rejected(tmp_path):
    fields = scenario_fields(gps=FREEZE | {"onset_s": -1.0})

    with pytest.raises(ScenarioError, match="cannot be negative"):
        load_scenario(write_scenario(tmp_path, fields))


def test_jitter_wider_than_the_onset_is_rejected(tmp_path):
    fields = scenario_fields(gps=FREEZE | {"onset_s": 1.0, "onset_jitter_s": 2.0})

    with pytest.raises(ScenarioError, match="wider than"):
        load_scenario(write_scenario(tmp_path, fields))


# A zero ease steps the velocity at onset, which is what the rise is shaped to avoid.
def test_an_instant_ease_is_rejected(tmp_path):
    fields = scenario_fields(gps=DRIFT | {"profile": LINEAR | {"ease_s": 0.0}})

    with pytest.raises(ScenarioError, match="ease_s is 0.0, it must be positive"):
        load_scenario(write_scenario(tmp_path, fields))


def test_absent_file_is_rejected(tmp_path):
    with pytest.raises(ScenarioError):
        load_scenario(tmp_path / "nowhere.yaml")


# Otherwise a typo in one waits until the flight that was meant to use it.
def test_the_committed_scenarios_load():
    paths = sorted(SCENARIOS.glob("ATK-*.yaml"))

    assert paths

    for path in paths:
        assert path.stem.startswith(load_scenario(path).scenario_id)
